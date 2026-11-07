#pragma once
#include <atomic>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string>
#include <vector>
#include "offline_tts.hpp"
#include "speech_tts.hpp"
#include "speech_policy.hpp"
#include "tts_audio_cache.hpp"
class AudioPlayer {
  public:
    bool init(bool storage_mounted);
    bool play(const std::string &path, const std::string &text = "",uint32_t client_token=0);
    bool tts_ready() const { return tts_available_; }
    wordcard::SpeechSource speech_source() const { return speech_source_; }
    void reload_tts();
    bool tone();
    bool probe_sd() {return submit(8);}
    bool mic_test();
    bool record();
    bool replay();
    void finish_recording() { finish_capture_ = true; }
    bool recording() const { return recording_; }
    bool has_recording() const { return clip_ready_; }
    void clear_recording() {
        clip_ready_ = false;
        clear_clip_ = true;
    }
    uint32_t cache_hits() const { return cache_hits_; }
    void invalidate_cache() { clear_cache_ = true; }
    void stop();
    bool suspend_io();
    void resume_io();
    bool benchmark(wordcard::TtsTestMode,int phrase,uint32_t run_id,bool reuse=false);
    void capture_dma(bool enabled){capture_dma_=enabled;}
    bool benchmark_play(uint32_t run_id);
    bool benchmark_result(wordcard::TtsTestResult &);
    void volume(int n) { volume_ = n < 0 ? 0 : n > 100 ? 100 : n; }
    int volume() const { return volume_; }
    bool busy() const { return busy_; }
    bool ready() const { return ready_; }
    bool uart_rx_seen() const { return uart_rx_; }
    bool result(std::string &text, int &kind,std::string &path,uint32_t *client_token=nullptr);

  private:
    struct Request {
        uint32_t client_token=0;
        int mode = 0;
        bool reuse=false;
        int test_engine=0,test_phrase=0;uint32_t test_id=0;
        char path[384]{};
        char text[384]{};
    };
    struct Result {
        uint32_t client_token=0;
        char text[128]{};
        char path[384]{};
        int kind = 0;
    };
    QueueHandle_t requests_ = nullptr, results_ = nullptr;
    QueueHandle_t test_results_=nullptr;
    SpeechPcm test_pcm_;
    wordcard::TtsTestResult test_result_;
    std::atomic<bool> test_ready_{false};
    i2s_chan_handle_t tx_ = nullptr, rx_ = nullptr;
    // Diagnostic only: finished TX DMA buffers, before auto-clear. Lifetime is
    // bounded by disabling TX before releasing the buffer; never used by lessons.
    std::atomic<int32_t *> tx_probe_{nullptr};
    portMUX_TYPE tx_probe_lock_=portMUX_INITIALIZER_UNLOCKED;
    size_t tx_probe_capacity_=0,tx_probe_count_=0;
    uint32_t tx_probe_last_us_=0,tx_probe_max_gap_us_=0;
    static bool tx_sent(i2s_chan_handle_t,i2s_event_data_t *,void *);
    void finish_tx_probe(int phrase,int mode);
    bool rx_suspended_=false,tx_suspended_=false;
    std::atomic<bool> busy_{false}, cancel_{false};
    std::atomic<int> volume_{50};
    std::atomic<bool> recording_{false}, finish_capture_{false}, clip_ready_{false},
        clear_clip_{false};
    std::atomic<uint32_t> cache_hits_{0};
    std::atomic<bool> clear_cache_{false};
    std::vector<int16_t> clip_, cached_audio_;
    std::string cached_path_;
    uint32_t cached_gain_=65536;
    std::string streamed_gain_path_;
    uint32_t streamed_gain_=65536;
    wordcard::TtsAudioCache speech_cache_;
    const std::string speech_cache_folder_="/sdcard/metalio/e-ink/wordcards/tts-cache";
    std::atomic<bool> capture_dma_{false};
    SpeechTts tts_;
    std::atomic<bool> tts_available_{false};
    std::atomic<wordcard::SpeechSource> speech_source_{wordcard::SpeechSource::None};
    const std::string tts_folder_ = "/sdcard/metalio/e-ink/wordcards/tts/espeak-ng-data";
    const std::string small_folder_ = "/sdcard/metalio/e-ink/wordcards/tts/amy-small-en-us";
    bool ready_ = false, uart_rx_ = false;
    bool storage_mounted_=false;
    uint32_t active_client_token_=0;
    bool submit(int, const std::string &path = "", const std::string &text = "",int test_engine=0,int test_phrase=0,uint32_t test_id=0,bool reuse=false,uint32_t client_token=0);
    void run_benchmark(const Request &);
    static void task(void *);
    void run();
    bool write(const int16_t *, size_t,uint32_t gain=65536);
    void report(const std::string &, int kind = 0,const char *path="");
};
