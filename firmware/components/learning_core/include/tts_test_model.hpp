#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
namespace wordcard {
enum class TtsTestMode { AmySmall, Espeak };
// Persistent cache metadata retains the previous engine IDs.
enum class TtsActual { None=0, Espeak=4, AmySmall=9 };
inline constexpr int tts_mode_count=2;
inline const char *tts_test_cache_tag(TtsTestMode m){return m==TtsTestMode::AmySmall?"test-7":"test-2";}
inline const char *tts_test_file_tag(TtsTestMode m){return m==TtsTestMode::AmySmall?"amysmall":"espeak";}
enum class TtsTestPhase { Idle, Queued, Generating, Ready, Playing, Done, Failed, Cancelled };
struct TtsPhrase { const char *title; const char *text; };
inline constexpr std::array<TtsPhrase,6> tts_phrases{{
    {"日常 · 约时间","Are you available on Friday morning?"},
    {"日常 · 餐厅订位","I'd like to book a table for two."},
    {"日常 · 问路","Could you tell me where the nearest station is?"},
    {"科技 · 固件","Please update the firmware and restart the device."},
    {"科技 · 网络","The server is not responding. Check the network connection."},
    {"科技 · 本地存储","This application stores your data locally."}
}};
inline bool tts_is_amy(TtsTestMode m){return m==TtsTestMode::AmySmall;}
inline const char *tts_mode_name(TtsTestMode m) {
    return m==TtsTestMode::AmySmall?"Amy Small":"eSpeak 托底";
}
inline const char *tts_actual_name(TtsActual m) {
    switch(m){case TtsActual::AmySmall:return "Amy Small";case TtsActual::Espeak:return "eSpeak";default:return "未运行";}
}
struct TtsTestResult {
    uint32_t run_id=0;int phrase=0;TtsTestMode requested=TtsTestMode::AmySmall;
    TtsActual actual=TtsActual::None;
    bool available=false,generated=false,played=false,cancelled=false,warm=false,memory_valid=false,play_event=false,cached=false,persisted=false;
    uint64_t prepare_us=0,compute_us=0,total_us=0,selfcheck_us=0;
    uint32_t samples=0,pcm_crc=0,arena_peak=0,simd_calls=0,scalar_calls=0;
    int pie_check=0,volume=0;
    uint32_t internal_before=0,psram_before=0,internal_min=0,psram_min=0,internal_after=0,psram_after=0;
    char error[160]{};
    uint32_t internal_peak() const {return internal_before>internal_min?internal_before-internal_min:0;}
    uint32_t psram_peak() const {return psram_before>psram_min?psram_before-psram_min:0;}
};
struct TtsTestSession {
    int phrase=0;TtsTestMode mode=TtsTestMode::AmySmall;TtsTestPhase phase=TtsTestPhase::Idle;
    bool batch=false,reuse_audio=false;uint32_t run_id=0;
    // Large history is heap-backed. The S31 project's >16-byte allocations
    // use PSRAM, preserving scarce HP/DMA RAM for SD and audio peripherals.
    using ResultRow=std::array<TtsTestResult,tts_mode_count>;
    std::vector<ResultRow> results=std::vector<ResultRow>(6);
    bool active() const {return phase==TtsTestPhase::Queued || phase==TtsTestPhase::Generating || phase==TtsTestPhase::Ready || phase==TtsTestPhase::Playing;}
    bool start(bool all,bool reuse=false) {
        if(active())return false;
        batch=all;reuse_audio=reuse && !all;if(all){mode=TtsTestMode::AmySmall;results[phrase]={};}
        results[phrase][int(mode)]={};++run_id;phase=TtsTestPhase::Queued;return true;
    }
    bool select_mode(int value) {if(active() || value<0 || value>=tts_mode_count)return false;mode=TtsTestMode(value);phase=TtsTestPhase::Idle;return true;}
    bool select_phrase(int value) {if(active() || value<0 || value>=6)return false;phrase=value;phase=TtsTestPhase::Idle;return true;}
    void cancel() {batch=false;++run_id;phase=TtsTestPhase::Cancelled;}
    bool leave_page(bool remains_on_test) {if(remains_on_test)return false;cancel();return true;}
    bool accept(const TtsTestResult &r) {
        if(!active() || r.run_id!=run_id || r.phrase!=phrase || r.requested!=mode)return false;
        if((r.play_event && phase!=TtsTestPhase::Playing) || (!r.play_event && phase!=TtsTestPhase::Generating))return false;
        results[phrase][int(mode)]=r;
        if(r.cancelled){batch=false;phase=TtsTestPhase::Cancelled;}
        else if(!r.generated || (r.play_event && !r.played)){batch=false;phase=TtsTestPhase::Failed;}
        else if(!r.play_event)phase=TtsTestPhase::Ready;
        else if(batch && int(mode)+1<tts_mode_count) {mode=TtsTestMode(int(mode)+1);++run_id;phase=TtsTestPhase::Queued;}
        else {batch=false;phase=TtsTestPhase::Done;}
        return true;
    }
};
} // namespace wordcard
