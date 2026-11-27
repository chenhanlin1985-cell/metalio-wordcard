#pragma once
#include "wordcard.hpp"
#include "playback_gain.hpp"
#include <esp_timer.h>
#include <esp_log.h>
// Explicit serial diagnostic; no scanning or writes during ordinary playback.
inline void audio_path_probe(const std::string &path) {
    for(int i=0;i<3;++i){const auto t=esp_timer_get_time();const bool present=wordcard::exists(path);ESP_LOGI("audio_path","stat=%d present=%d us=%lld",i,present,(long long)(esp_timer_get_time()-t));}
    wordcard::WaveReader wave;std::string error;
    auto t=esp_timer_get_time();const bool ok=wave.open(path,error);
    ESP_LOGI("audio_path","open=%d us=%lld samples=%lu",ok,(long long)(esp_timer_get_time()-t),(unsigned long)wave.samples());
    if(!ok)return;
    t=esp_timer_get_time();wordcard::SpeechLevel level;uint32_t fnv=2166136261,total=0;int16_t pcm[1024];
    while(wave.remaining()){const auto n=wave.read(pcm,1024);if(!n)break;level.add(pcm,n);total+=n;for(size_t i=0;i<n;++i){fnv=(fnv^uint8_t(pcm[i]))*16777619;fnv=(fnv^uint8_t(uint16_t(pcm[i])>>8))*16777619;}}
    ESP_LOGI("audio_path","read_us=%lld samples=%lu remaining=%lu pcm_fnv=%08lx gain=%lu",(long long)(esp_timer_get_time()-t),(unsigned long)total,(unsigned long)wave.remaining(),(unsigned long)fnv,(unsigned long)level.gain());
}
