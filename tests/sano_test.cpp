#include "sano_tts.hpp"
#include <clocale>
#include "sano_frontend.hpp"
#include "speech_policy.hpp"
#include "sano_simd.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
namespace fs=std::filesystem;
static int checks=0;
static void check(bool good,const char *why) {++checks;if(!good)throw std::runtime_error(why);}
static std::string quote(const std::string &s) {
    std::string o="\"";for(unsigned char c:s) {if(c=='\\' || c=='"'){o+='\\';o+=char(c);}else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c=='\t')o+="\\t";else o+=char(c);}return o+'"';
}
static void wav(const fs::path &path,const int16_t *pcm,size_t n) {
    std::ofstream out(path,std::ios::binary);
    auto u32=[&](uint32_t v){for(int i=0;i<4;++i)out.put(char(v>>(8*i)));};
    auto u16=[&](uint16_t v){out.put(char(v));out.put(char(v>>8));};
    out.write("RIFF",4);u32(uint32_t(36+n*2));out.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(16000);u32(32000);u16(2);u16(16);
    out.write("data",4);u32(uint32_t(n*2));for(size_t i=0;i<n;++i)u16(uint16_t(pcm[i]));check(bool(out),"WAV write");
}
int main(int argc,char **argv) {
    try {
        check(argc==5,"arguments");std::string ef=argv[1],mf=argv[2],error;fs::path out=argv[3];fs::create_directories(out);
        check(OfflineTts::assets_available(ef),"English data available");check(SanoTts::assets_available(mf),"known model checksums");
        OfflineTts g2p;SanoTts neural;SanoPcm pcm;
        check(g2p.prepare(ef,error),"initialize frontend for locale regression");
        std::setlocale(LC_CTYPE,"C");
        std::string locale_ipa;
        check(g2p.phonemes(ef,"Friday available router choice",locale_ipa,error),"C locale IPA");
        check(locale_ipa.find("a͡ɪ")!=std::string::npos && locale_ipa.find("e͡ɪ")!=std::string::npos &&
              locale_ipa.find("a͡ʊ")!=std::string::npos && locale_ipa.find("ɔ͡ɪ")!=std::string::npos,
              "IPA diphthong ties must not depend on OS locale");
        const std::vector<std::string> phrases={
            "Are you available on Friday morning?",
            "I'd like to book a table for two.",
            "Could you tell me where the nearest station is?",
            "Please update the firmware and restart the device.",
            "The server is not responding. Check the network connection.",
            "This application stores your data locally."};
        // Record every raw IPA fragment so the unchanged upstream JS frontend is an independent oracle.
        std::ifstream corpus(argv[4]);std::ofstream trace(out/"frontend.jsonl"),metrics(out/"samples.jsonl");
        std::string original;size_t cases=0;
        while(std::getline(corpus,original)) {
            std::string text;if(!wordcard::speech_text(original,text))continue;
            std::map<std::string,std::string> fragments;std::vector<int32_t> ids;std::string ps;
            bool ok=wordcard::sano_phonemes(text,[&](const std::string &part,std::string &ipa){bool ready=g2p.phonemes(ef,part,ipa,error);fragments[part]=ipa;return ready;},ids,ps);
            trace<<"{\"text\":"<<quote(text)<<",\"ok\":"<<(ok?"true":"false")<<",\"phonemes\":"<<quote(ps)<<",\"ids\":[";
            for(size_t i=0;i<ids.size();++i){if(i)trace<<',';trace<<ids[i];}trace<<"],\"fragments\":{";
            size_t i=0;for(auto &kv:fragments){if(i++)trace<<',';trace<<quote(kv.first)<<':'<<quote(kv.second);}trace<<"}}\n";++cases;
        }
        check(cases>100 && bool(trace),"frontend corpus trace");
        for(size_t i=0;i<phrases.size();++i) {
            check(neural.render(g2p,ef,mf,phrases[i],[]{return false;},pcm,error),error.c_str());
#ifdef SANO_SIMD_HOST_MODEL
            const auto ks=sano_kernels_stats();
            check(ks.selfcheck==1 && ks.simd_calls>0 && ks.scalar_calls==0,"all neural matvecs use SIMD source model");
            std::cout<<"phrase "<<i+1<<": simd_calls="<<ks.simd_calls<<" scalar_calls="<<ks.scalar_calls<<" macs="<<ks.simd_macs<<'\n';
#endif
            check(pcm.count>1600 && pcm.count<=16000*12,"neural bounded output");
            check(std::any_of(pcm.data.get(),pcm.data.get()+pcm.count,[](int16_t n){return n>1000 || n< -1000;}),"non-silent neural PCM");
            std::string name=std::to_string(i+1);
            wav(out/(name+"-sano.wav"),pcm.data.get(),pcm.count);
            metrics<<"{\"id\":"<<i+1<<",\"text\":"<<quote(phrases[i])<<",\"sano_samples\":"<<pcm.count<<",\"arena_peak\":"<<pcm.arena_peak<<",\"host_compute_us\":"<<pcm.elapsed_us;
            std::vector<int16_t> espeak;
            check(g2p.synthesize(ef,phrases[i],[&](const int16_t *p,size_t n){espeak.insert(espeak.end(),p,p+n);return true;},[]{return false;},error),"eSpeak still speaks after G2P");
            wav(out/(name+"-espeak.wav"),espeak.data(),espeak.size());
            metrics<<",\"espeak_samples\":"<<espeak.size()<<"}\n";
        }
        check(!neural.render(g2p,ef,mf,"Ready.",[]{return true;},pcm,error),"pre-cancel");
        int polls=0;check(!neural.render(g2p,ef,mf,phrases[4],[&](){return ++polls>12;},pcm,error),"cancel before decoder output");
        check(!pcm.data && pcm.count==0,"cancel drops all buffered PCM");
        check(!neural.render(g2p,ef,mf,"Ready.",[]{return false;},pcm,error,64),"small arena rejected without exit");
        check(!neural.render(g2p,ef,mf,"Ready.",[]{return false;},pcm,error,512*1024,1),"compute deadline");
        check(neural.render(g2p,ef,mf,"Ready.",[]{return false;},pcm,error),"new render after cancellation/OOM/deadline");
        HybridTts hybrid;size_t delivered=0;int statuses=0;
        auto sink=[&](const int16_t *,size_t n){delivered+=n;return true;};auto status=[&](const char *){++statuses;};
        check(hybrid.synthesize(ef,mf,"Ready.",sink,[]{return false;},status,error),"hybrid neural path");
        check(std::string(hybrid.last_engine())=="sanoTTS" && delivered>0,"neural selected");
        delivered=0;
        check(hybrid.synthesize(ef,mf+"-absent","Ready.",sink,[]{return false;},status,error),"missing model falls back");
        check(std::string(hybrid.last_engine())=="eSpeak-NG" && delivered>0,"real fallback audio");
        const auto damaged=fs::current_path()/"damaged-model";fs::create_directories(damaged);
        for(const char *name:{"front_q8.bin","model_q8.bin"})fs::copy_file(fs::path(mf)/name,damaged/name,fs::copy_options::overwrite_existing);
        {std::fstream f(damaged/"front_q8.bin",std::ios::binary|std::ios::in|std::ios::out);char b=0;f.read(&b,1);b^=1;f.seekp(0);f.write(&b,1);check(bool(f),"fault fixture write");}
        check(!SanoTts::assets_available(damaged.string()),"same-size damaged model rejected");
        delivered=0;check(hybrid.synthesize(ef,damaged.string(),"Ready.",sink,[]{return false;},status,error),"damaged model falls back");
        check(delivered>0 && std::string(hybrid.last_engine())=="eSpeak-NG","damage never reaches neural kernels");
        std::string long_text;for(int i=0;i<30;++i)long_text+="firmware ";delivered=0;
        check(hybrid.synthesize(ef,mf,long_text,sink,[]{return false;},status,error),"over model token limit falls back without truncation");
        check(std::string(hybrid.last_engine())=="eSpeak-NG" && delivered>0,"long English handled by eSpeak");
        int attempts=0;
        check(!hybrid.synthesize(ef,mf,"Ready.",[&](const int16_t *,size_t){++attempts;return false;},[]{return false;},status,error),"playback sink failure");
        check(attempts==1 && std::string(hybrid.last_engine())=="sanoTTS","no voice restart after playback starts");
        delivered=0;
        check(!hybrid.synthesize(ef,mf,"Ready.",sink,[]{return true;},status,error) && delivered==0,"cancel never triggers fallback");
        check(!SanoTts::assets_available(mf+"-absent"),"missing assets unavailable");
        check(statuses>=6,"source status emitted");
        std::cout<<"PASS: "<<checks<<" sano/hybrid checks; 6 real paired WAVs; "<<cases<<" frontend traces; host only\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
