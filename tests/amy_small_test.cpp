#include "sano_tts.hpp"
#include "amy_resampler.hpp"
#include "amy_parallel.h"
#include "amy_packed.h"
#include "amy_stream.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <cmath>
using namespace wordcard;
static int checks;
static void check(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
static std::vector<char> read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);check(bool(f),"oracle exists");return {std::istreambuf_iterator<char>(f),{}};}
static void wave(const std::filesystem::path &p,const int16_t *pcm,size_t count){
 std::ofstream f(p,std::ios::binary);auto u32=[&](uint32_t n){for(int b=0;b<4;++b)f.put(char(n>>(b*8)));};auto u16=[&](uint16_t n){f.put(char(n));f.put(char(n>>8));};
 f.write("RIFF",4);u32(36+uint32_t(count*2));f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(16000);u32(32000);u16(2);u16(16);f.write("data",4);u32(uint32_t(count*2));f.write((const char*)pcm,count*2);check(bool(f),"wave saved");
}
struct Capture {std::vector<float> values;};
static int capture(const float *p,int n,void *u){auto &v=((Capture*)u)->values;v.insert(v.end(),p,p+n);return 0;}
static int never(void*){return 0;}
int main(int argc,char **argv){try{
 check(argc==6,"Small arguments");const std::string ef=argv[1],mf=argv[2];const std::filesystem::path out=argv[3],oracle=argv[4];const std::string original=argv[5];
 OfflineTts espeak;AmyTts amy;AmyPcm pcm;std::string error;
 check(!amy.render(espeak,ef,mf,"hello",[]{return true;},pcm,error,120000000,AMY_FP32,AmyVoice::Small) && !amy.initialized(AmyVoice::Small),"Small pre-cancel is lazy");
 for(int i=0;i<6;++i){
  amy_parallel_set_mode(2,0);
  check(amy.render(espeak,ef,mf,tts_phrases[i].text,[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),error.c_str());
  check(amy.initialized(AmyVoice::Small) && !amy.initialized(),"resident model identity is Small only");
  auto raw=read(oracle/(std::to_string(i+1)+"-amy-raw.wav"));size_t n=(raw.size()-44)/2;std::vector<int16_t> input(n),expected(amy_resampled_size(n));std::memcpy(input.data(),raw.data()+44,n*2);
  check(amy_resample(input.data(),n,expected.data(),expected.size(),[]{return false;}),"reference FIR");
  check(pcm.count==expected.size() && !std::memcmp(pcm.data.get(),expected.data(),pcm.count*2),"Small full text and chunks equal unchanged upstream full-utterance PCM");
  wave(out/(std::to_string(i+1)+"-amysmall.wav"),pcm.data.get(),pcm.count);
  if(i==0){
   for(int option:{0,1,3,7,15,31,63}){
    check(amy_fp32_set_options(option),"Small FP32 option accepted");
    check(amy.render(espeak,ef,mf,tts_phrases[i].text,[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),error.c_str());
    check(pcm.count==expected.size() && !std::memcmp(pcm.data.get(),expected.data(),pcm.count*2),"baseline/HP descriptor/dual frontend/yield same complete PCM");
    check(!amy_fp32_options(),"FP32 option scope cleared after render");
   }
   amy_parallel_set_mode(1,0);check(amy.render(espeak,ef,mf,tts_phrases[i].text,[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),error.c_str());
   check(pcm.count==expected.size() && !std::memcmp(pcm.data.get(),expected.data(),pcm.count*2),"Small single/dual host paths identical");amy_parallel_set_mode(2,0);}
 }
 // Exercise exact context propagation at both sentence and block boundaries,
 // including a short tail after the stage0 128-frame cache tile.
 auto model=read(std::filesystem::path(mf)/"dec_f32.bin");snt_piperlite_model dm{};
 check(!snt_piperlite_init(&dm,model.data(),416,(const float*)(model.data()+416),(model.size()-416)/4),"Small boundary model binds");
 for(int frames:{1,2,23,24,25,127,128,129,257}){
  std::vector<float> z(192*frames);uint32_t seed=123;
  for(auto &v:z){seed=seed*1664525u+1013904223u;v=(int(seed>>16)-32768)/65536.f;}
  std::vector<float> expected(frames*256),arena(snt_piperlite_arena_floats(&dm,frames));
  check(!snt_piperlite_synthesize(&dm,z.data(),frames,expected.data(),arena.data(),arena.size()),"full decoder boundary oracle");
  for(int chunk:{16,24,64}){const size_t need=amy_reuse_scratch_floats(&dm,frames,chunk);std::vector<float> scratch(need+16,12345.f);Capture got;
   check(!amy_reuse_decode(&dm,z.data(),frames,chunk,scratch.data()+8,need,capture,never,&got),"reuse boundary decode");
   check(got.values.size()==expected.size() && !std::memcmp(got.values.data(),expected.data(),expected.size()*4),"all float values at boundaries match full decoder");
   check(scratch[0]==12345.f && scratch[7]==12345.f && scratch[need+8]==12345.f && scratch.back()==12345.f,"reuse scratch guards intact");
   check(amy_reuse_decode(&dm,z.data(),frames,chunk,scratch.data()+8,need-1,capture,never,&got)==-2,"undersized arena rejected before write");
  }
 }
 check(!amy.render(espeak,ef,mf,tts_phrases[0].text,[]{return amy_parallel_active()!=0;},pcm,error,120000000,AMY_FP32,AmyVoice::Small) && !pcm.data && !pcm.count && !amy_parallel_active(),"Small cancellation joins workers and drops partial PCM");
 check(amy.render(espeak,ef,mf,"microcontroller",[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),"Small arbitrary word after cancellation");
 check(!amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error,1,AMY_FP32,AmyVoice::Small) && !pcm.data,"Small deadline");
 check(!amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error,120000000,AMY_INT16,AmyVoice::Small),"Small rejects legacy dynamic-weight quantization");
 check(!amy.render(espeak,ef,original,"router",[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),"Small cannot silently consume original assets");
 check(amy.render(espeak,ef,original,"router",[]{return false;},pcm,error) && amy.initialized() && !amy.initialized(AmyVoice::Small),"original Amy recovers after voice mismatch");
 check(amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),"switch back to Small releases original model");
 AmyTts missing;check(!missing.render(espeak,ef,mf+"-absent","router",[]{return false;},pcm,error,120000000,AMY_FP32,AmyVoice::Small),"missing Small assets fail strictly");
 HybridTts hybrid;SanoPcm hp;TtsTestResult result;
 check(hybrid.benchmark(TtsTestMode::AmySmall,0,ef,"unused",[]{return false;},hp,result) && result.actual==TtsActual::AmySmall && !result.warm,"Small benchmark reports correct cold model");
 check(hybrid.benchmark(TtsTestMode::AmySmall,0,ef,"unused",[]{return false;},hp,result) && result.warm,"Small warm model reuse");
 check(hybrid.benchmark(TtsTestMode::Amy,0,ef,"unused",[]{return false;},hp,result) && result.actual==TtsActual::Amy && !result.warm,"switch to original is cold, not falsely warm");
 hybrid.prefer_amy(true,AMY_FP32,AmyVoice::Small);size_t heard=0;
 check(hybrid.synthesize(ef,"unused","router",[&](const int16_t*,size_t n){heard+=n;return true;},[]{return false;},[](const char*){},error) && heard && std::string(hybrid.last_engine())=="Amy Small","Small normal text route");
 check(!hybrid.synthesize(ef,"unused","router",[](const int16_t*,size_t){return false;},[]{return false;},[](const char*){},error) && std::string(hybrid.last_engine())=="Amy Small","no eSpeak replay after Small sink fails");
 hybrid.prefer_amy(true,AMY_INT16,AmyVoice::Small);
 check(hybrid.synthesize(ef,"unused","router",[](const int16_t*,size_t){return true;},[]{return false;},[](const char*){},error) && std::string(hybrid.last_engine())=="eSpeak-NG","Small pre-PCM failure falls back to eSpeak");
 for(int i=0;i<6;++i){
  check(hybrid.benchmark(TtsTestMode::AmySmallPacked,i,ef,"unused",[]{return false;},hp,result),result.error);
  check(result.actual==TtsActual::AmySmallPacked && result.scalar_calls>0 && !result.simd_calls,"host packed integer path reports its actual engine");
  auto ref=read(out/(std::to_string(i+1)+"-amysmall.wav"));check(hp.count*2+44==ref.size(),"packed voice preserves exact duration");
  double signal=0,noise=0;int peak=0;for(size_t j=0;j<hp.count;++j){int16_t v;std::memcpy(&v,ref.data()+44+j*2,2);double diff=double(hp.data.get()[j])-v;signal+=double(v)*v;noise+=diff*diff;peak=std::max(peak,int(std::abs(diff)));}
  const double snr=noise?10*std::log10(signal/noise):999;
  std::cout<<"SMALL_Q16_QUALITY phrase="<<i+1<<" snr="<<snr<<" max_lsb="<<peak<<'\n';
  check(snr>60 && peak<40,"prepacked INT16 waveform error remains bounded");
  wave(out/(std::to_string(i+1)+"-amysmall16.wav"),hp.data.get(),hp.count);
  if(!i){amy_parallel_set_mode(1,0);check(amy.render(espeak,ef,mf,tts_phrases[i].text,[]{return false;},pcm,error,120000000,AMY_PACKED16,AmyVoice::Small),error.c_str());
   check(pcm.count==hp.count && !std::memcmp(pcm.data.get(),hp.data.get(),hp.count*2),"packed host single/dual exact64 outputs identical");amy_parallel_set_mode(2,0);}
 }
 check(!amy.render(espeak,ef,mf,tts_phrases[0].text,[]{return amy_parallel_active()!=0;},pcm,error,120000000,AMY_PACKED16,AmyVoice::Small) && !pcm.data && !amy_parallel_active(),"packed cancellation joins workers and unbinds model");
 check(amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error,120000000,AMY_PACKED16,AmyVoice::Small),"packed recovery");
 auto packed=read(std::filesystem::path(mf)/"dec_q16.bin");snt_piperlite_model qm{};
 check(!amy_packed_init(&qm,packed.data(),packed.size()),"packed model metadata accepted");amy_packed_unbind();
 packed[0]^=1;check(amy_packed_init(&qm,packed.data(),packed.size())!=0,"packed magic rejected");packed[0]^=1;
 packed[64+32]^=1;check(amy_packed_init(&qm,packed.data(),packed.size())!=0,"misaligned packed payload rejected");packed[64+32]^=1;
 check(amy_packed_init(&qm,packed.data(),packed.size()-1)!=0,"truncated packed asset rejected");
 for(int cores:{1,2}){
  amy_parallel_set_mode(cores,0);
  for(int count:{1,2,63,64,65,127,128,129,440,441,442,8193,65537}){
   const size_t n=size_t(count);
   const size_t total=amy_resampled_size(n);std::vector<int16_t> raw(n),expected(total),actual(total+16,12345);uint32_t seed=567;
   for(auto &v:raw){seed=seed*1664525u+1013904223u;v=int16_t(seed>>16);}
   check(amy_resample(raw.data(),n,expected.data(),total,[]{return false;}),"FIR independent scalar reference");
   check(amy_resample_parallel(raw.data(),n,actual.data()+8,total,[]{return false;}),"HP FIR accepts complete blocks and tails");
   check(!std::memcmp(expected.data(),actual.data()+8,total*2),"HP FIR all signed samples exact to original");
   check(actual.front()==12345 && actual[7]==12345 && actual[total+8]==12345 && actual.back()==12345,"HP FIR guards preserved");
   check(!amy_resample_parallel(raw.data(),n,actual.data()+8,total-1,[]{return false;}),"HP FIR rejects short output");
  }
 }
 std::vector<int16_t> fir_input(65537,123),fir_output(amy_resampled_size(fir_input.size()));
 check(!amy_resample_parallel(fir_input.data(),fir_input.size(),fir_output.data(),fir_output.size(),[]{return amy_parallel_active()!=0;}) && !amy_parallel_active(),"FIR cancellation joins HP tasks before releasing input/output");
 check(amy_resample_parallel(fir_input.data(),fir_input.size(),fir_output.data(),fir_output.size(),[]{return false;}),"FIR recovers after cancelled work");
 std::cout<<"PASS: "<<checks<<" Amy Small checks; six upstream PCM oracles, voice switching, cancellation, arbitrary text, dual/single, strict assets and eSpeak fallback\n";
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
