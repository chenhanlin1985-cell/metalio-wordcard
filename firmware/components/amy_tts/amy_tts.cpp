#include "amy_tts.hpp"
#include "amy_assets.hpp"
#include "amy_compute.h"
#include "amy_parallel.h"
#include "amy_resampler.hpp"
#include "resource_asset.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <esp_attr.h>
#endif
namespace {
using Block=std::unique_ptr<void,decltype(&std::free)>;
Block allocate(size_t bytes){
#ifdef ESP_PLATFORM
 return Block(heap_caps_aligned_alloc(16,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),&std::free);
#else
 return Block(std::malloc(bytes),&std::free);
#endif
}
struct Run {const OfflineTts::Cancel &cancel;int64_t deadline;int16_t *raw=nullptr;size_t capacity=0,count=0;bool invalid=false;};
struct FirJob {const int16_t *input;int16_t *output;size_t count,total;};
#ifdef ESP_PLATFORM
static DRAM_ATTR FirJob fir_job;
#else
static FirJob fir_job;
#endif
void fir_worker(void *context,int first,int limit,int){
 const auto &job=*(const FirJob*)context;const size_t end=std::min(job.total,size_t(limit)*64);
 for(size_t i=size_t(first)*64;i<end;++i){
  const size_t phase=(i*441)%320;const int64_t center=int64_t(i*441/320);int64_t acc=0;
  for(int j=0;j<129;++j){const int64_t p=center+j-64;if(p>=0 && uint64_t(p)<job.count)acc+=int64_t(job.input[size_t(p)])*amy_filter_q30[phase][j];}
  const auto rounded=acc>=0?(acc+(1ll<<29))/(1ll<<30):-((-acc+(1ll<<29))/(1ll<<30));
  job.output[i]=int16_t(std::clamp<int64_t>(rounded,-32768,32767));
 }
}
int poll(void *p){auto &r=*(Run *)p;return r.cancel() || amy_now_us()>=r.deadline;}
int samples(const float *data,int n,void *p){auto &r=*(Run *)p;if(poll(p) || n<0 || r.count+size_t(n)>r.capacity)return 1;
 for(int i=0;i<n;++i){if(!std::isfinite(data[i])){r.invalid=true;return 1;}r.raw[r.count++]=int16_t(std::lround(std::clamp(data[i],-1.f,1.f)*32767));}
#ifdef ESP_PLATFORM
 std::printf("AMY_CHUNK samples=%u/%u remaining_ms=%lld\n",unsigned(r.count),unsigned(r.capacity),(long long)((r.deadline-amy_now_us())/1000));
#endif
 return 0;}
}
bool amy_resample_parallel(const int16_t *input,size_t n,int16_t *output,size_t cap,const std::function<bool()> &cancel){
 const size_t total=amy_resampled_size(n);
 if(!input || !output || !total || total>cap || total>size_t(INT32_MAX) || cancel())return false;
 fir_job={input,output,n,total};
 auto stop=[](void *context)->int{return (*(const std::function<bool()>*)context)()?1:0;};
 return amy_blocks_run(fir_worker,&fir_job,int((total+63)/64),stop,(void*)&cancel)==0;
}
bool AmyTts::render(OfflineTts &espeak,const std::string &ef,const std::string &mf,const std::string &text,const OfflineTts::Cancel &cancel,AmyPcm &out,std::string &error,int64_t budget){
 out={};error.clear();const auto start=amy_now_us();Run run{cancel,start+budget};
 auto fail=[&](const char *reason){error=cancel()?"Amy 已停止":amy_now_us()>=run.deadline?"Amy 合成超时，请换短句":reason;out.data.reset();out.count=0;return false;};
 if(poll(&run))return fail("Amy 已停止");
 const auto &assets=AMY_SMALL_ASSETS;
 std::vector<int32_t> ids;
 if(!amy_phoneme_ids(espeak,ef,text,ids,error))return false;
 if(folder_!=mf){release();folder_=mf;cache_.clear();
#ifdef ESP_PLATFORM
  wordcard::load_resource_cache(mf+"/.verified-v2",cache_);
#endif
 }
 if(!initialized()){
  auto front=allocate(assets[0].bytes),decoder=allocate(assets[1].bytes);
  if(!front || !decoder)return fail("Amy 模型内存不足");
  if(!wordcard::cached_binary_asset(cache_,mf+"/"+assets[0].name,assets[0].bytes,assets[0].crc,front.get(),error) ||
     !wordcard::cached_binary_asset(cache_,mf+"/"+assets[1].name,assets[1].bytes,assets[1].crc,decoder.get(),error))return false;
#ifdef ESP_PLATFORM
  wordcard::save_resource_cache(mf+"/.verified-v2",cache_);
#endif
  front_=std::move(front);decoder_=std::move(decoder);
 }
 snt_front_model fm{};snt_piperlite_model dm{};
 if(snt_front_init(&fm,front_.get(),568,(const float *)((const char *)front_.get()+568),(assets[0].bytes-568)/4) ||
    snt_piperlite_init(&dm,decoder_.get(),416,(const float *)((const char *)decoder_.get()+416),(assets[1].bytes-416)/4))return fail("Amy 模型结构不符");
 if(ids.size()>265)return fail("Amy 文本过长，请分句");
#ifdef ESP_PLATFORM
 std::printf("AMY_FRONTEND ids=");for(size_t i=0;i<ids.size();++i)std::printf("%s%ld",i?",":"",(long)ids[i]);std::printf("\n");
#endif
 const auto compute=amy_now_us();out.prepare_us=compute-start;
 amy_profile_reset();amy_parallel_reset_stats();
 struct Clock {AmyPcm &out;int64_t begin;~Clock(){out.compute_us=amy_now_us()-begin;amy_profile_report(out.compute_us);}} clock{out,compute};
 int64_t prof=amy_now_us();
 std::vector<int32_t> durations(ids.size()),timing_ids=ids;for(auto &id:timing_ids)if(id<0||id>=fm.d_vocab)id=59;
 long frames;
 {
  const size_t size=snt_front_duration_arena_floats(&fm,int(ids.size()));auto scratch=allocate(size*4);if(!scratch)return fail("Amy 时长预测内存不足");
  frames=amy_duration_run(&fm,timing_ids.data(),int(ids.size()),1.08f,durations.data(),(float *)scratch.get(),size,poll,&run);
  if(frames<=0)return fail("Amy 时长预测失败");
 }
 if(frames>AMY_STREAM_MAX_FRAMES)return fail("Amy 音频超过12秒，请分句");
 amy_profile_mark(AMY_P_DURATION,&prof);
 const size_t raw_count=size_t(frames)*256,latent_bytes=size_t(fm.a_out)*frames*4;
 auto latent=allocate(latent_bytes);if(!latent)return fail("Amy 音素缓冲内存不足");
 {
  auto acoustic=ids;for(auto &id:acoustic)if(id<0||id>=fm.a_vocab)id=59;
  const size_t size=snt_front_latent_arena_floats(&fm,int(ids.size()),frames);auto scratch=allocate(size*4);if(!scratch)return fail("Amy 语调预测内存不足");
  if(amy_latent_run(&fm,acoustic.data(),durations.data(),int(ids.size()),frames,(float *)latent.get(),(float *)scratch.get(),size,poll,&run))return fail("Amy 语调预测失败");
 }
 amy_profile_mark(AMY_P_LATENT,&prof);
#ifdef ESP_PLATFORM
 if(amy_profile_enabled()){std::printf("AMY_FRONT_THREADS\n");amy_parallel_report();}
#endif
 auto raw=allocate(raw_count*2);if(!raw)return fail("Amy 音频缓存内存不足");run.raw=(int16_t *)raw.get();run.capacity=raw_count;
 {
  int chunk=24;
  const auto scratch_floats=amy_reuse_scratch_floats;
#ifdef ESP_PLATFORM
  chunk=amy_chunk_frames(scratch_floats(&dm,int(frames),24)*4,
   heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),
   heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#endif
  const size_t size=scratch_floats(&dm,int(frames),chunk);auto scratch=allocate(size*4);if(!scratch)return fail("Amy 分块工作区内存不足");out.arena_peak=size*4;
#ifdef ESP_PLATFORM
  std::printf("AMY_PLAN frames=%ld chunk=%d raw=%u scratch=%u free_psram=%u\n",frames,chunk,unsigned(raw_count),unsigned(size*4),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
#endif
  if(amy_reuse_run(&dm,(float *)latent.get(),int(frames),chunk,(float *)scratch.get(),size,samples,poll,&run) || run.invalid || run.count!=raw_count)return fail("Amy 分块合成失败");
 }
 latent.reset();
 const size_t count=amy_resampled_size(raw_count);auto pcm=allocate(count*2);if(!pcm)return fail("Amy 播放缓冲内存不足");
 prof=amy_now_us();
 {
  amy_parallel_reset_stats();
  if(!amy_resample_parallel((int16_t *)raw.get(),raw_count,(int16_t *)pcm.get(),count,[&](){return poll(&run)!=0;}))return fail("Amy 音频转换已停止");
#ifdef ESP_PLATFORM
  if(amy_profile_enabled()){std::printf("AMY_FIR_THREADS\n");amy_parallel_report();}
#endif
 }
 amy_profile_mark(AMY_P_FIR,&prof);
 out.data.reset((int16_t *)pcm.release());out.count=count;return true;
}
