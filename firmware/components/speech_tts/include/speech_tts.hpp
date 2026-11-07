#pragma once
#include "amy_tts.hpp"
#include "tts_test_model.hpp"
struct SpeechPcm {
    std::unique_ptr<int16_t,decltype(&std::free)> data{nullptr,&std::free};
    size_t count=0;
};
// The audio worker owns this service. SD and persistent cache lookup happen
// before calling it. There is one neural voice and one failure fallback.
class SpeechTts {
public:
    using Status=std::function<void(const char *)>;
    bool synthesize(const std::string &espeak_folder,const std::string &small_folder,
        const std::string &text,const OfflineTts::Sink &,const OfflineTts::Cancel &,
        const Status &,std::string &error,int64_t budget_us=30000000);
    bool failed()const{return espeak_.failed();}
    const char *last_engine()const{return last_;}
    uint32_t playback_gain()const{return gain_;}
    void select_test_engine(wordcard::TtsTestMode mode){if(mode!=wordcard::TtsTestMode::AmySmall)amy_.release();}
    bool benchmark(wordcard::TtsTestMode,int phrase,const std::string &espeak_folder,
        const std::string &small_folder,const OfflineTts::Cancel &,SpeechPcm &,
        wordcard::TtsTestResult &,int64_t budget_us=120000000);
private:
    OfflineTts espeak_;
    AmyTts amy_;
    const char *last_="";
    uint32_t gain_=65536;
};
