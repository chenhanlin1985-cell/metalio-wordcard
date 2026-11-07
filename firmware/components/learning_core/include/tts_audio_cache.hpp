#pragma once
#include "resource_cache_io.hpp"
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#endif
namespace wordcard {
// Bump only when voice, model, frontend, rate or PCM processing changes.
// Volume is applied at playback and must not invalidate source PCM.
inline std::string tts_audio_key(const std::string &text,const std::string &engine) {
    if(engine=="amysmall16" || engine=="test-8")return "tts-small-q16-v1/53048510620bf886/reuse128-24/dynamic-a16-exact64/16k-fir6900-129/"+engine+"\n"+text;
    if(engine=="amysmall" || engine=="test-7")return "tts-amy-small-v1/b45240e7c24ac4ca/en-us/16k/fir6900-129/12halo-f32/"+engine+"\n"+text;
    if(engine=="amy16" || engine=="amymix" || engine=="test-5" || engine=="test-6")return "tts-amy-int-v1/f73b8382/en-us/16k/fir6900-129/12halo/pc-pow2-dynamic-a16-exact64/"+engine+"\n"+text;
    if(engine=="amy" || engine=="test-4")return "tts-amy-v1/f73b8382/en-us/16k/fir6900-129/12halo-f32/"+engine+"\n"+text;
    if(engine=="test-3")return "tts-pico-v1/en-US/16k/exp-libm/bf1a8df9/3038081-4144641790/"+engine+"\n"+text;
    return "tts-v1/ipa2/fir6800-129/16k/eSpeak145/en-us/heartnano-df2dc8f7-40775392/"+engine+"\n"+text;
}
struct CachedSpeech {std::vector<int16_t> pcm;uint32_t gain=65536,actual=0;};
inline uint32_t tts_pcm_crc(const int16_t *pcm,size_t count) {
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<count;++i)for(int byte=0;byte<2;++byte) {
        crc^=(uint16_t(pcm[i])>>(byte*8))&255;
        for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
class TtsAudioCache {
    std::string folder_;
    ResourceCache verified_;
    static uint32_t hash(const std::string &s) {uint32_t x=2166136261u;for(unsigned char c:s){x^=c;x*=16777619u;}return x;}
    static uint32_t get(const std::string &s,size_t at) {uint32_t x=0;for(int i=0;i<4;++i)x|=uint32_t(uint8_t(s[at+i]))<<(i*8);return x;}
    static void put(std::string &s,uint32_t x){for(int i=0;i<4;++i)s+=char(x>>(i*8));}
    static bool read(const std::string &path,std::string &bytes) {
        FILE *f=std::fopen(path.c_str(),"rb");if(!f)return false;
        char block[4096];size_t n;bytes.clear();bool ok=true;
        while((n=std::fread(block,1,sizeof(block),f))) {
            if(bytes.size()+n>max_samples*2+4096){ok=false;break;}bytes.append(block,n);
        }
        ok=ok && !std::ferror(f);std::fclose(f);return ok;
    }
    static bool format(const std::string &s,bool checksum) {
        if(s.size()<28 || s.compare(0,4,"WTAC")!=0)return false;
        const uint32_t key=get(s,4),samples=get(s,8),gain=get(s,12),actual=get(s,16);
        return key>0 && key<=2048 && samples>0 && samples<=max_samples && gain>=65536 && gain<=196608 &&
            actual>=1 && actual<=10 && get(s,20)==16000 && s.size()==28+key+samples*2 &&
            (!checksum || hash(s.substr(0,s.size()-4))==get(s,s.size()-4));
    }
  public:
    static constexpr size_t max_samples=192000;
    void open(const std::string &folder) {
        if(folder_==folder)return;
        folder_=folder;verified_.clear();
        load_resource_cache(folder_+"/.verified-v2",verified_);
    }
    // 128 replaceable slots, bounded to <100 MiB including atomic backups.
    // Exact identity inside each slot prevents a hash collision playing a wrong word.
    std::string path(const std::string &key)const {char name[16];std::snprintf(name,sizeof(name),"/%02x.tts",unsigned(hash(key)%128));return folder_+name;}
    bool load(const std::string &key,CachedSpeech &speech) {
        speech={};if(folder_.empty())return false;
        const auto file=path(key);std::string bytes;
        const auto status=verified_.get(file,[&](ResourceStamp &s){return resource_stamp(file,s);},[&](){
            if(!read(file,bytes))return ResourceCheck::Retry;
            return format(bytes,true)?ResourceCheck::Valid:ResourceCheck::Invalid;
        });
        if(status!=ResourceCheck::Valid)return false;
        if(bytes.empty() && !read(file,bytes)){verified_.invalidate(file);return false;}
        if(!format(bytes,false) || bytes.substr(24,get(bytes,4))!=key)return false;
        const size_t n=get(bytes,8);speech.pcm.resize(n);speech.gain=get(bytes,12);speech.actual=get(bytes,16);
        std::memcpy(speech.pcm.data(),bytes.data()+24+key.size(),n*2);
        save_resource_cache(folder_+"/.verified-v2",verified_);
        return true;
    }
    bool store(const std::string &key,const int16_t *pcm,size_t n,uint32_t gain,uint32_t actual) {
        if(folder_.empty() || !pcm || !n || n>max_samples || key.empty() || key.size()>2048 ||
           gain<65536 || gain>196608 || actual<1 || actual>10)return false;
#ifdef _WIN32
        _mkdir(folder_.c_str());
#else
        mkdir(folder_.c_str(),0775);
#endif
        std::string bytes="WTAC",error;put(bytes,uint32_t(key.size()));put(bytes,uint32_t(n));put(bytes,gain);put(bytes,actual);put(bytes,16000);
        bytes+=key;bytes.append(reinterpret_cast<const char *>(pcm),n*2);put(bytes,hash(bytes));
        const auto file=path(key);if(!atomic_write(file,bytes,error))return false;
        verified_.invalidate(file);
        verified_.get(file,[&](ResourceStamp &s){return resource_stamp(file,s);},[]{return ResourceCheck::Valid;});
        save_resource_cache(folder_+"/.verified-v2",verified_);return true;
    }
    uint32_t validations()const{return verified_.validations;}
};
}
