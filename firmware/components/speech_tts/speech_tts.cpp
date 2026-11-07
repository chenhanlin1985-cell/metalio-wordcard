#include "speech_tts.hpp"
#include "amy_compute.h"
#include "playback_gain.hpp"
#include <algorithm>
#include <cstring>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif
bool SpeechTts::synthesize(const std::string &ef,const std::string &mf,const std::string &text,
    const OfflineTts::Sink &sink,const OfflineTts::Cancel &cancel,const Status &status,
    std::string &error,int64_t budget){
    last_="";gain_=65536;error.clear();if(cancel())return false;
    AmyPcm audio;status("合成发音 · Amy Small 生成中");
    if(amy_.render(espeak_,ef,mf,text,cancel,audio,error,budget)){
        gain_=wordcard::speech_playback_gain(audio.data.get(),audio.count);
        last_="Amy Small";status("合成发音 · Amy Small");
        for(size_t p=0;p<audio.count;p+=512){
            if(cancel())return false;
            if(!sink(audio.data.get()+p,std::min<size_t>(512,audio.count-p))){error="发音播放中断";return false;}
        }
        return true;
    }
    if(cancel())return false;
    // No PCM has reached the sink yet. Failed/deadline/cancelled Small output
    // cannot become a partial cache, and a sink failure never retries eSpeak.
    amy_.release();last_="eSpeak-NG";status("合成发音 · eSpeak-NG 托底");error.clear();
    // Match first playback to cached playback without regenerating eSpeak.
    // Bound the temporary buffer to the existing 12-second persistent limit.
    // Longer utterances or allocation failure retain unity streaming playback.
    constexpr size_t limit=16000*12;
    SpeechPcm buffered;
#ifdef ESP_PLATFORM
    buffered.data.reset(static_cast<int16_t *>(heap_caps_malloc(limit*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
#else
    buffered.data.reset(static_cast<int16_t *>(std::malloc(limit*2)));
#endif
    wordcard::SpeechGainBuffer leveled(buffered.data.get(),limit);
    auto emit=[&](const int16_t *p,size_t n){gain_=leveled.gain();return !cancel() && sink(p,n);};
    const bool ok=espeak_.synthesize(ef,text,[&](const int16_t *p,size_t n){
        if(cancel())return false;
        return leveled.push(p,n,emit);
    },cancel,error);
    if(!ok || cancel())return false;
    if(!leveled.finish(emit)){error="发音播放中断";return false;}
    return true;
}
bool SpeechTts::benchmark(wordcard::TtsTestMode mode,int phrase,const std::string &ef,
    const std::string &mf,const OfflineTts::Cancel &cancel,SpeechPcm &pcm,
    wordcard::TtsTestResult &r,int64_t budget){
    using namespace wordcard;
    select_test_engine(mode);r={};r.available=true;r.requested=mode;r.phrase=phrase;pcm={};
    const auto start=amy_now_us(),deadline=start+budget;
    auto stop=[&](){return cancel() || amy_now_us()>=deadline;};
    std::string error;bool ok=false,overflow=false;
    if(phrase<0 || phrase>=int(tts_phrases.size()) || int(mode)<0 || int(mode)>=tts_mode_count)error="发音选项无效";
    else if(stop())error="发音已停止";
    else if(mode==TtsTestMode::AmySmall){
        r.actual=TtsActual::AmySmall;r.warm=amy_.initialized();AmyPcm audio;
        ok=amy_.render(espeak_,ef,mf,tts_phrases[phrase].text,stop,audio,error,budget);
        r.prepare_us=audio.prepare_us;r.compute_us=audio.compute_us;r.arena_peak=audio.arena_peak;
        pcm.data=std::move(audio.data);pcm.count=audio.count;
    }else{
        r.actual=TtsActual::Espeak;r.warm=espeak_.initialized();
        if(espeak_.prepare(ef,error) && !stop()){
#ifdef ESP_PLATFORM
            pcm.data.reset(static_cast<int16_t *>(heap_caps_aligned_alloc(16,16000*12*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
#else
            pcm.data.reset(static_cast<int16_t *>(std::malloc(16000*12*2)));
#endif
            const auto compute=amy_now_us();r.prepare_us=compute-start;
            if(!pcm.data)error="发音内存不足";
            else ok=espeak_.synthesize(ef,tts_phrases[phrase].text,[&](const int16_t *p,size_t n){
                if(stop())return false;
                if(pcm.count+n>16000*12){overflow=true;return false;}
                std::memcpy(pcm.data.get()+pcm.count,p,n*2);pcm.count+=n;return true;
            },stop,error);
            r.compute_us=amy_now_us()-compute;
        }
    }
    r.total_us=amy_now_us()-start;r.cancelled=cancel();
    if(r.cancelled)error="发音已停止";
    else if(amy_now_us()>=deadline){ok=false;error="合成超时，请换短句或重试";}
    else if(overflow){ok=false;error="音频超过 12 秒上限";}
    r.generated=ok && !r.cancelled && pcm.count>0;
    if(r.generated){
        r.samples=uint32_t(pcm.count);uint32_t crc=0xffffffff;
        for(size_t i=0;i<pcm.count;++i)for(int byte=0;byte<2;++byte){
            crc^=(uint16_t(pcm.data.get()[i])>>(8*byte))&255;
            for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
        }
        r.pcm_crc=~crc;
    }else{pcm.data.reset();pcm.count=0;std::snprintf(r.error,sizeof(r.error),"%s",error.empty()?"合成失败，请检查 SD 资源":error.c_str());}
    return r.generated;
}
