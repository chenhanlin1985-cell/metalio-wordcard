#include "offline_tts.hpp"
#include "speech_policy.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace fs=std::filesystem;
using namespace wordcard;
static int checks=0;
static void check(bool value,const char *why) {++checks;if(!value)throw std::runtime_error(why);}
static void wav(const fs::path &path,const std::vector<int16_t> &pcm) {
    std::ofstream out(path,std::ios::binary);
    auto u32=[&](uint32_t v){for(int i=0;i<4;++i)out.put(char(v>>(i*8)));};
    auto u16=[&](uint16_t v){out.put(char(v));out.put(char(v>>8));};
    out.write("RIFF",4);u32(uint32_t(36+pcm.size()*2));out.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(16000);u32(32000);u16(2);u16(16);
    out.write("data",4);u32(uint32_t(pcm.size()*2));for(auto v:pcm)u16(uint16_t(v));
}
int main(int argc,char **argv) {
    try {
        check(argc==3,"args");const fs::path out=argv[2];fs::create_directories(out);
        std::string text,error;
        check(speech_text("IT",text) && text=="I T","IT stays an initialism");
        check(speech_text("it",text) && text=="it","pronoun it stays lowercase");
        check(speech_text("Don't restart the Wi-Fi router.",text),"plain English text");
        check(speech_text("It’s ready—now.",text) && text=="It's ready-now.","smart punctuation");
        check(speech_text("café",text) && text=="café","Latin diacritics retained");
        check(!speech_text(std::string(361,'a'),text),"no silent truncation");
        check(!speech_text("中文",text),"do not pretend English TTS supports Chinese");
        check(!speech_text("  ... ",text),"reject empty speech");
        check(speech_source(true,true,"word")==SpeechSource::Sd,"SD priority");
        check(speech_source(false,true,"polymorphism")==SpeechSource::Synthesis,"missing SD fallback");
        check(speech_source(false,false,"word")==SpeechSource::None,"missing engine explicit");
        std::vector<int16_t> source(22050);for(size_t i=0;i<source.size();++i)source[i]=int16_t(std::sin(i*6.28318530718*1000/22050)*12000);
        std::vector<int16_t> whole,chunked;
        SpeechResampler one(22050),many(22050);
        check(one.push(source.data(),source.size(),[&](const int16_t *p,size_t n){whole.insert(whole.end(),p,p+n);return true;}),"resample one block");
        for(size_t i=0;i<source.size();i+=137) check(many.push(source.data()+i,std::min<size_t>(137,source.size()-i),[&](const int16_t *p,size_t n){chunked.insert(chunked.end(),p,p+n);return true;}),"resample chunk");
        check(whole==chunked && whole.size()==16000,"chunk invariant, exact sample duration");
        check(OfflineTts::assets_available(argv[1]),"data CRC and length");
        check(!OfflineTts::assets_available((out/"missing").string()),"missing data rejected");
        OfflineTts engine;
        const char *samples[]={"firmware","polymorphism","appointment","IT","it","Can you reproduce the error?"};
        for(int i=0;i<6;++i) {
            std::vector<int16_t> pcm;
            check(engine.synthesize(argv[1],samples[i],[&](const int16_t*p,size_t n){pcm.insert(pcm.end(),p,p+n);return true;},[]{return false;},error),error.c_str());
            check(pcm.size()>1600 && pcm.size()<16000*30,"synthesized duration bounded");
            check(std::any_of(pcm.begin(),pcm.end(),[](int16_t n){return n>1000 || n< -1000;}),"actual audible PCM");
            wav(out/("sample-"+std::to_string(i+1)+".wav"),pcm);
        }
        size_t blocks=0;
        check(!engine.synthesize(argv[1],"This sentence should stop before it finishes.",[&](const int16_t *,size_t){return ++blocks<2;},[]{return false;},error),"sink failure cancels safely");
        check(engine.synthesize(argv[1],"Ready.",[](const int16_t *,size_t){return true;},[]{return false;},error),"synthesize again after cancellation");
        check(!engine.synthesize(argv[1],"Ready.",[](const int16_t *,size_t){return true;},[]{return true;},error),"cancel before output");
        std::cout<<"PASS: "<<checks<<" TTS checks; real pinned engine PCM, SD-first policy, resampling, case and cancellation; host only.\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
