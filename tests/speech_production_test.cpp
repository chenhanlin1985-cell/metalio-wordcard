#include "speech_tts.hpp"
#include "amy_parallel.h"
#include "amy_resampler.hpp"
#include "amy_stream.h"
#include "tts_audio_cache.hpp"
#include "playback_gain.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace wordcard;
static int checks;
static void check(bool v,const char *why){++checks;if(!v)throw std::runtime_error(why);}
static std::vector<char> read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);check(bool(f),"reference exists");return {std::istreambuf_iterator<char>(f),{}};}
static void save(const std::filesystem::path &p,const int16_t *v,size_t n){
 std::ofstream f(p,std::ios::binary);auto u32=[&](uint32_t x){for(int b=0;b<4;++b)f.put(char(x>>(8*b)));};auto u16=[&](uint16_t x){f.put(char(x));f.put(char(x>>8));};
 f.write("RIFF",4);u32(36+uint32_t(n*2));f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(16000);u32(32000);u16(2);u16(16);f.write("data",4);u32(uint32_t(n*2));f.write((const char*)v,n*2);check(bool(f),"PCM saved");
}
struct Capture{std::vector<float> v;};
static int capture(const float *p,int n,void *u){auto &v=((Capture*)u)->v;v.insert(v.end(),p,p+n);return 0;}
static int never(void*){return 0;}
int main(int argc,char **argv){try{
 check(argc==5,"arguments");const std::string ef=argv[1],mf=argv[2];const std::filesystem::path out=argv[3],prior=argv[4];
 check(tts_mode_count==2 && int(TtsActual::AmySmall)==9 && int(TtsActual::Espeak)==4,"two engines, stable persistent actual IDs");
 check(std::string(tts_test_cache_tag(TtsTestMode::AmySmall))=="test-7" && std::string(tts_test_cache_tag(TtsTestMode::Espeak))=="test-2","test cache keys remain compatible despite compact UI enum");
 OfflineTts espeak;AmyTts amy;AmyPcm pcm;std::string error;
 check(!amy.render(espeak,ef,mf,"hello",[]{return true;},pcm,error) && !amy.initialized(),"pre-cancel remains lazy");
 for(int i=0;i<6;++i){
  check(amy.render(espeak,ef,mf,tts_phrases[i].text,[]{return false;},pcm,error),error.c_str());
  auto ref=read(prior/(std::to_string(i+1)+"-amysmall.wav"));
  check(ref.size()==44+pcm.count*2 && !std::memcmp(ref.data()+44,pcm.data.get(),pcm.count*2),"fixed production PCM identical to fix36a best method");
  save(out/(std::to_string(i+1)+"-amysmall.wav"),pcm.data.get(),pcm.count);
 }
 auto model=read(std::filesystem::path(mf)/"dec_f32.bin");snt_piperlite_model dm{};
 check(!snt_piperlite_init(&dm,model.data(),416,(const float*)(model.data()+416),(model.size()-416)/4),"Small model binds");
 for(int frames:{1,2,23,24,25,127,128,129,257}){
  std::vector<float> z(192*frames);uint32_t seed=123;
  for(auto &v:z){seed=seed*1664525u+1013904223u;v=(int(seed>>16)-32768)/65536.f;}
  std::vector<float> expected(frames*256),arena(snt_piperlite_arena_floats(&dm,frames));
  check(!snt_piperlite_synthesize(&dm,z.data(),frames,expected.data(),arena.data(),arena.size()),"full graph boundary reference");
  for(int chunk:{16,24,64}){
   const auto need=amy_reuse_scratch_floats(&dm,frames,chunk);std::vector<float> scratch(need+16,12345.f);Capture got;
   check(!amy_reuse_decode(&dm,z.data(),frames,chunk,scratch.data()+8,need,capture,never,&got),"context reuse and tails");
   check(got.v.size()==expected.size() && !std::memcmp(got.v.data(),expected.data(),expected.size()*4),"chunking preserves every float");
   check(scratch[0]==12345.f && scratch[7]==12345.f && scratch[need+8]==12345.f && scratch.back()==12345.f,"arena guards");
   check(amy_reuse_decode(&dm,z.data(),frames,chunk,scratch.data()+8,need-1,capture,never,&got)==-2,"short arena rejects before write");
  }
 }
 check(!amy.render(espeak,ef,mf,tts_phrases[0].text,[]{return amy_parallel_active()!=0;},pcm,error) && !pcm.data && !pcm.count && !amy_parallel_active(),"cancel joins workers and discards PCM");
 check(amy.render(espeak,ef,mf,"microcontroller",[]{return false;},pcm,error),"arbitrary word recovery");
 check(!amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error,1) && !pcm.data,"deadline discards PCM");
 check(!amy.render(espeak,ef,mf+"-absent","router",[]{return false;},pcm,error),"missing Small assets fail strictly");
 SpeechTts service;SpeechPcm audio;TtsTestResult r;
 check(service.benchmark(TtsTestMode::AmySmall,0,ef,mf,[]{return false;},audio,r) && r.actual==TtsActual::AmySmall && !r.warm,"strict Small diagnostic");
 check(r.generated && !r.played && !r.play_event && r.total_us>=r.prepare_us+r.compute_us && !r.memory_valid,"host timings and no fabricated playback/heap");
 check(service.benchmark(TtsTestMode::AmySmall,0,ef,mf,[]{return false;},audio,r) && r.warm,"resident model reused");
 check(service.benchmark(TtsTestMode::Espeak,0,ef,mf,[]{return false;},audio,r) && r.actual==TtsActual::Espeak,"eSpeak diagnostic retained");
 auto ep=read(prior/"1-espeak.wav");check(ep.size()==44+audio.count*2 && !std::memcmp(ep.data()+44,audio.data.get(),audio.count*2),"eSpeak PCM unchanged");
 check(!service.benchmark(TtsTestMode(2),0,ef,mf,[]{return false;},audio,r) && !audio.data,"removed modes rejected");
 check(!service.benchmark(TtsTestMode::AmySmall,0,ef,mf+"-absent",[]{return false;},audio,r) && r.actual==TtsActual::AmySmall,"diagnostic does not disguise fallback");
 size_t heard=0;bool fallback=false;
 auto sink=[&](const int16_t*,size_t n){heard+=n;return true;};auto status=[&](const char *s){fallback|=std::string(s).find("eSpeak-NG")!=std::string::npos;};
 check(service.synthesize(ef,mf,"Please save this file.",sink,[]{return false;},status,error) && heard && !fallback && std::string(service.last_engine())=="Amy Small","ordinary default is Small without runtime selection");
 heard=0;fallback=false;
 check(!service.synthesize(ef,mf,"router",[](const int16_t*,size_t){return false;},[]{return false;},status,error) && !fallback,"never replay eSpeak after sink failure");
 check(service.synthesize(ef,mf+"-absent","router",sink,[]{return false;},status,error) && fallback && heard,"missing model falls back before PCM");
 std::vector<int16_t> fallback_pcm;
 auto collect=[&](const int16_t *p,size_t n){fallback_pcm.insert(fallback_pcm.end(),p,p+n);return true;};
 check(service.synthesize(ef,mf+"-absent",tts_phrases[0].text,collect,[]{return false;},status,error),"leveled fallback succeeds");
 check(!fallback_pcm.empty() && fallback_pcm.size()<=TtsAudioCache::max_samples,"short fallback fits bounded cache");
 check(service.playback_gain()==speech_playback_gain(fallback_pcm.data(),fallback_pcm.size()) && service.playback_gain()>65536,"first fallback gain equals cache/test gain");
 check(!service.synthesize(ef,mf+"-absent","router",[](const int16_t*,size_t){return false;},[]{return false;},status,error),"leveled fallback sink failure propagates");
 const std::string long_text="Please save this file before updating the firmware. Please save this file before updating the firmware. Please save this file before updating the firmware. Please save this file before updating the firmware. Please save this file before updating the firmware.";
 fallback_pcm.clear();
 check(service.synthesize(ef,mf+"-absent",long_text,collect,[]{return false;},status,error),"long fallback remains available");
 check(fallback_pcm.size()>TtsAudioCache::max_samples && service.playback_gain()==65536,"long fallback exceeds bounded buffer and streams at constant unity");
 heard=0;fallback=false;
 check(service.synthesize(ef,mf,"router",sink,[]{return false;},status,error,1) && fallback && heard,"deadline falls back to eSpeak");
 heard=0;fallback=false;
 check(!service.synthesize(ef,mf,"router",sink,[]{return true;},status,error) && !heard && !fallback,"cancel cannot trigger fallback");
 for(int count:{1,2,63,64,65,127,128,129,440,441,442,8193,65537}){
  size_t n=size_t(count),total=amy_resampled_size(n);std::vector<int16_t> raw(n),ref(total),got(total+16,12345);uint32_t seed=567;
  for(auto &v:raw){seed=seed*1664525u+1013904223u;v=int16_t(seed>>16);}
  check(amy_resample(raw.data(),n,ref.data(),total,[]{return false;}),"scalar FIR oracle");
  check(amy_resample_parallel(raw.data(),n,got.data()+8,total,[]{return false;}),"parallel FIR boundary");
  check(!std::memcmp(ref.data(),got.data()+8,total*2),"FIR exact signed samples");
  check(got[0]==12345 && got[7]==12345 && got[total+8]==12345 && got.back()==12345,"FIR guards");
  check(!amy_resample_parallel(raw.data(),n,got.data()+8,total-1,[]{return false;}),"FIR short output rejects");
 }
 std::vector<int16_t> input(65537,123),output(amy_resampled_size(input.size()));
 check(!amy_resample_parallel(input.data(),input.size(),output.data(),output.size(),[]{return amy_parallel_active()!=0;}) && !amy_parallel_active(),"FIR cancel joins");
 check(amy_resample_parallel(input.data(),input.size(),output.data(),output.size(),[]{return false;}),"FIR recovery");
 TtsTestSession session;check(session.mode==TtsTestMode::AmySmall && !session.select_mode(2) && !session.select_mode(-1),"UI default and removed selectors");
 check(session.start(false,true) && !session.select_mode(1),"busy locks selector");session.phase=TtsTestPhase::Generating;
 r={};r.generated=r.available=true;r.run_id=session.run_id;r.requested=session.mode;r.phrase=session.phrase;
 check(session.accept(r) && session.phase==TtsTestPhase::Ready,"generation must be drawn before playback");
 check(!session.accept(r),"duplicate generation rejected");session.phase=TtsTestPhase::Playing;r.play_event=r.played=true;
 check(session.accept(r) && session.phase==TtsTestPhase::Done,"playback completion");
 check(session.start(false),"restart after playback");session.phase=TtsTestPhase::Generating;r.run_id=session.run_id;r.play_event=false;
 check(session.leave_page(false) && !session.accept(r) && !session.active(),"leave cancels and rejects late result");
 std::cout<<"PASS: "<<checks<<" production speech checks; six exact Small PCM, fixed dual FP32, boundaries, normal default, fallback and cancellation\n";
 }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
