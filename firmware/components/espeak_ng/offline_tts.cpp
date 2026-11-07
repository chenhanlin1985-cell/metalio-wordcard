#include "offline_tts.hpp"
#include "speech_policy.hpp"
#include "tts_data_integrity.hpp"
#include "resource_asset.hpp"
#include <espeak-ng/espeak_ng.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include <esp_log.h>
#endif
namespace {
struct Stream {
    wordcard::SpeechResampler resampler;
    const OfflineTts::Sink &sink;
    const OfflineTts::Cancel &cancel;
    bool stopped=false;
};
Stream *active=nullptr;
int callback(short *samples,int count,espeak_EVENT *) {
    if (!active) return 1;
    if(active->cancel() || active->resampler.samples()>=16000*120) {active->stopped=true;return 1;}
    if(samples && count>0 && !active->resampler.push(samples,size_t(count),active->sink)) {active->stopped=true;return 1;}
    return 0;
}
}
bool OfflineTts::assets_present(const std::string &folder) {
    for(const auto &asset:TTS_DATA_FILES) {
        wordcard::ResourceStamp info;
        if(!wordcard::resource_stamp(folder+"/"+asset.name,info) ||
           !info.present || info.bytes!=asset.bytes)return false;
    }
    return true;
}
bool OfflineTts::assets_available(const std::string &folder) {
    std::string error;
    for(const auto &asset:TTS_DATA_FILES)
        if(wordcard::read_binary_asset(folder+"/"+asset.name,asset.bytes,asset.crc,nullptr,true,error)!=wordcard::ResourceCheck::Valid)return false;
    return true;
}
bool OfflineTts::prepare(const std::string &folder,std::string &error) {
    if(failed_) {error="合成未就绪，请重启后重试";return false;}
    if(!initialized_) {
        error.clear();
        if(cache_folder_!=folder) {
            cache_folder_=folder;assets_cache_.clear();
#ifdef ESP_PLATFORM
            wordcard::load_resource_cache(folder+"/.verified-v2",assets_cache_);
#endif
        }
        bool valid=true;
        for(const auto &asset:TTS_DATA_FILES)
            if(!wordcard::cached_binary_asset(assets_cache_,folder+"/"+asset.name,asset.bytes,asset.crc,nullptr,error)){valid=false;break;}
#ifdef ESP_PLATFORM
        wordcard::save_resource_cache(folder+"/.verified-v2",assets_cache_);
        ESP_LOGI("wordcard.tts","ASSET_CACHE hits=%lu metadata=%lu validations=%lu",
            (unsigned long)assets_cache_.hits,(unsigned long)assets_cache_.metadata_checks,(unsigned long)assets_cache_.validations);
#endif
        if(!valid)return false;
        espeak_ng_InitializePath(folder.c_str());
        espeak_ng_ERROR_CONTEXT context=nullptr;
        auto status=espeak_ng_Initialize(&context);
        espeak_ng_ClearErrorContext(&context);
        if(status==ENS_OK) status=espeak_ng_InitializeOutput(ENOUTPUT_MODE_SYNCHRONOUS,40,nullptr);
        if(status==ENS_OK) status=espeak_ng_SetVoiceByName("en-us");
        if(status!=ENS_OK) {failed_=true;error="离线合成初始化失败，请重启后重试";return false;}
        espeak_ng_SetParameter(espeakRATE,145,0);
        espeak_ng_SetParameter(espeakVOLUME,100,0);
        espeak_SetSynthCallback(callback);initialized_=true;
    }
    return true;
}
bool OfflineTts::phonemes(const std::string &folder,const std::string &text,std::string &ipa,std::string &error) {
    ipa.clear();
    if(!prepare(folder,error) || text.size()>360) return false;
    const void *cursor=text.c_str();
    for(int guard=0;cursor && guard<128;++guard) {
        const void *previous=cursor;
        const char *part=espeak_TextToPhonemes(&cursor,espeakCHARS_UTF8,espeakPHONEMES_IPA | 0x80 | (0x361<<8));
        if(part && *part) {if(!ipa.empty())ipa+=' ';ipa+=part;}
        if(cursor==previous || ipa.size()>4096) {error="音素转换失败";return false;}
    }
    return cursor==nullptr && !ipa.empty();
}
bool OfflineTts::synthesize(const std::string &folder,const std::string &text,const Sink &sink,const Cancel &cancel,std::string &error) {
    std::string spoken;
    if(!wordcard::speech_text(text,spoken)) {error="此文本暂不支持离线合成";return false;}
    if(!prepare(folder,error)) return false;
    if(cancel()) return false;
    Stream stream{wordcard::SpeechResampler(unsigned(espeak_ng_GetSampleRate())),sink,cancel};
    active=&stream;
    const auto status=espeak_ng_Synthesize(spoken.c_str(),spoken.size()+1,0,POS_CHARACTER,0,espeakCHARS_UTF8,nullptr,nullptr);
    active=nullptr;
    if(stream.stopped || cancel()) {espeak_ng_Cancel();error="合成播放已停止";return false;}
    if(status!=ENS_OK || !stream.resampler.samples()) {error="离线合成失败，请重试";return false;}
    return true;
}
