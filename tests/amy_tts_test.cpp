#include "sano_tts.hpp"
#include "amy_resampler.hpp"
#include "amy_parallel.h"
#include "speech_policy.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <stdexcept>
using namespace wordcard;
static int checks=0;
static void check(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
static std::vector<char> read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);check(bool(f),"reference exists");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try{
 const size_t arena24=4804608,reserve=512*1024;
 check(amy_chunk_frames(arena24,arena24,arena24+reserve)==24,"decoder arena can be separate from the reserve");
 check(amy_chunk_frames(arena24,arena24-1,arena24+reserve+1000000)==16,"fragmented free bytes cannot replace contiguous arena");
 check(amy_chunk_frames(arena24,arena24+1000000,arena24+reserve-1)==16,"workspace and total reserve must both fit");
 check(amy_chunk_frames(arena24,0,0)==16,"allocation failure path retains small chunk");
 check(amy_chunk_frames(arena24,5200000,6200000)==24,"fragmentation must not unnecessarily shrink an otherwise safe arena");
 check(amy_chunk_frames(arena24,arena24-1,arena24-1)==16,"never infer capacity from a wrapped subtraction");
 check(argc==6,"Amy arguments");const std::string ef=argv[1],mf=argv[2];const std::filesystem::path out=argv[3],base=argv[4];
 OfflineTts espeak;AmyTts amy;AmyPcm pcm;std::string error;std::vector<int32_t> ids;
 check(!amy.render(espeak,ef,mf,"hello",[]{return true;},pcm,error) && !amy.initialized(),"pre-cancel is lazy");
 std::ifstream corpus(argv[5]);std::ofstream trace(out/"amy-front.tsv");std::string text;
 while(std::getline(corpus,text)){
  std::string spoken;if(!speech_text(text,spoken))continue;
  if(!amy_phoneme_ids(espeak,ef,text,ids,error))continue;
  static const char hex[]="0123456789abcdef";
  for(unsigned char c:spoken)trace<<hex[c>>4]<<hex[c&15];trace<<'\t';for(size_t i=0;i<ids.size();++i)trace<<(i?",":"")<<ids[i];trace<<'\n';
 }
 trace.close();
 for(int i=0;i<6;++i){
  check(amy_phoneme_ids(espeak,ef,tts_phrases[i].text,ids,error),error.c_str());auto expected_ids=read(base/(std::to_string(i+1)+"-amy-ids.bin"));
  check(expected_ids.size()==ids.size()*4 && !std::memcmp(expected_ids.data(),ids.data(),expected_ids.size()),"Amy real text IDs match original G2P WASM");
  check(amy.render(espeak,ef,mf,tts_phrases[i].text,[]{return false;},pcm,error),error.c_str());
  auto original=read(base/(std::to_string(i+1)+"-amy-raw.wav"));const size_t raw_n=(original.size()-44)/2;std::vector<int16_t> raw(raw_n),expected(amy_resampled_size(raw_n));std::memcpy(raw.data(),original.data()+44,raw_n*2);
  check(amy_resample(raw.data(),raw_n,expected.data(),expected.size(),[]{return false;}),"reference resample");
  check(pcm.count==expected.size() && !std::memcmp(pcm.data.get(),expected.data(),pcm.count*2),"real text through complete port equals original Amy plus same FIR");
  std::ofstream f(out/(std::to_string(i+1)+"-amy.wav"),std::ios::binary);auto u32=[&](uint32_t n){for(int b=0;b<4;++b)f.put(char(n>>(b*8)));};auto u16=[&](uint16_t n){f.put(char(n));f.put(char(n>>8));};
  f.write("RIFF",4);u32(36+uint32_t(pcm.count*2));f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(16000);u32(32000);u16(2);u16(16);f.write("data",4);u32(uint32_t(pcm.count*2));f.write((const char *)pcm.data.get(),pcm.count*2);check(bool(f),"port WAV save");
 }
 int polls=0;check(!amy.render(espeak,ef,mf,tts_phrases[0].text,[&](){return ++polls>500;},pcm,error) && !pcm.data && !pcm.count,"in-kernel cancellation drops output");
 check(!amy.render(espeak,ef,mf,tts_phrases[0].text,[]{return amy_parallel_active()!=0;},pcm,error) && !pcm.data && !pcm.count && !amy_parallel_active(),"parallel cancellation joins helper before dropping arena");
 check(amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error),"arbitrary unseen word after cancellation");
 check(!amy.render(espeak,ef,mf,tts_phrases[4].text,[]{return false;},pcm,error,1) && !pcm.data,"deadline");
 check(!amy.render(espeak,ef,mf,std::string(361,'a'),[]{return false;},pcm,error),"oversize text");
 AmyTts absent;check(!absent.render(espeak,ef,mf+"-missing","test",[]{return false;},pcm,error),"missing models fail strictly");
 HybridTts hybrid;SanoPcm hp;TtsTestResult result;
 check(hybrid.benchmark(TtsTestMode::Amy,0,ef,"unused",[]{return false;},hp,result) && result.actual==TtsActual::Amy && !result.memory_valid,"real benchmark routes Amy without fake heap");
 hybrid.prefer_amy(true);std::vector<int16_t> heard;check(hybrid.synthesize(ef,"unused","firmware",[&](const int16_t *p,size_t n){heard.insert(heard.end(),p,p+n);return true;},[]{return false;},[](const char *){},error) && std::string(hybrid.last_engine())=="Amy" && !heard.empty(),"arbitrary text through normal hybrid path");
 check(!hybrid.synthesize(ef,"unused","firmware",[](const int16_t *,size_t){return false;},[]{return false;},[](const char *){},error) && std::string(hybrid.last_engine())=="Amy","sink failure does not replay fallback");
 for(int mode=1;mode<=2;++mode){
  for(int phrase=0;phrase<6;++phrase){
   amy_parallel_set_mode(2,0);
   check(hybrid.benchmark(mode==1?TtsTestMode::AmyInt16:TtsTestMode::AmyMixed,phrase,ef,"unused",[]{return false;},hp,result),result.error);
   check(result.actual==(mode==1?TtsActual::AmyInt16:TtsActual::AmyMixed) && result.scalar_calls>0 && !result.simd_calls,"host runs genuine integer path, no device SIMD claim");
   auto baseline=read(out/(std::to_string(phrase+1)+"-amy.wav"));check(baseline.size()==hp.count*2+44,"integer mode preserves duration/sample rate");
   std::ofstream f(out/(std::to_string(phrase+1)+(mode==1?"-amy16.wav":"-amymix.wav")),std::ios::binary);f.write(baseline.data(),44);f.write((const char*)hp.data.get(),hp.count*2);f.close();
   if(phrase==0){amy_parallel_set_mode(1,0);check(amy.render(espeak,ef,mf,tts_phrases[phrase].text,[]{return false;},pcm,error,120000000,mode),error.c_str());
    check(pcm.count==hp.count && !std::memcmp(pcm.data.get(),hp.data.get(),pcm.count*2),"integer single/dual complete PCM exact");amy_parallel_set_mode(2,0);}
  }
  check(!amy.render(espeak,ef,mf,tts_phrases[0].text,[]{return amy_parallel_active()!=0;},pcm,error,120000000,mode) && !pcm.data && !amy_parallel_active(),"integer cancellation joins workers before workspace free");
  check(amy.render(espeak,ef,mf,"router",[]{return false;},pcm,error,120000000,mode),"integer recovery after cancellation");
  hybrid.prefer_amy(true,mode);check(hybrid.synthesize(ef,"unused","router",[](const int16_t*,size_t){return true;},[]{return false;},[](const char*){},error),"integer normal speech route");
  check(std::string(hybrid.last_engine())==(mode==1?"Amy INT16":"Amy INT8/16"),"normal speech identifies integer precision");
  // Invalid precision must fail before PCM and use the same eSpeak fallback.
  hybrid.prefer_amy(true,99);check(hybrid.synthesize(ef,"unused","router",[](const int16_t*,size_t){return true;},[]{return false;},[](const char*){},error) && std::string(hybrid.last_engine())=="eSpeak-NG","failed Amy falls back before any PCM");
 }
 // Exercise alias rejection, clipping and cancellation at the real converter.
 for(int hz:{1000,3000,5000,8200,10000}){
  std::vector<int16_t> input(22050),output(16000);for(size_t i=0;i<input.size();++i)input[i]=int16_t(std::lround(10000*std::sin(i*6.283185307179586*hz/22050)));
  check(amy_resample(input.data(),input.size(),output.data(),output.size(),[]{return false;}),"sine conversion");double power=0;for(int i=400;i<15600;++i)power+=double(output[i])*output[i];double rms=std::sqrt(power/15200);
  check(hz<6000?std::abs(rms-7071)<40:rms<3,"passband retained and above-Nyquist suppressed");
 }
 std::cout<<"PASS: "<<checks<<" Amy port checks: real G2P, full six PCM oracles, arbitrary text, cancellation/recovery/deadline, benchmark, no replay fallback and FIR\n";
 }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
