#include "audio_player.hpp"
#include "sd_read_probe.hpp"
#include "config.h"
#include "epd_board_metalio_eink4_plus.h"
#include "wordcard.hpp"
#include "playback_gain.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <type_traits>
#include <freertos/task.h>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
static const char *TAG = "wordcard.audio";
bool AudioPlayer::init(bool storage_mounted) {
    storage_mounted_=storage_mounted;
    epd_board_metalio_eink4_plus_pa_set(false);
    uart_config_t uart{};
    uart.baud_rate = 115200;
    uart.data_bits = UART_DATA_8_BITS;
    uart.parity = UART_PARITY_DISABLE;
    uart.stop_bits = UART_STOP_BITS_1;
    uart.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart.source_clk = UART_SCLK_DEFAULT;
    if (uart_param_config(UART_NUM_1, &uart) != ESP_OK ||
        uart_set_pin(UART_NUM_1, BT_AUDIO_TX_PIN, BT_AUDIO_RX_PIN, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE) != ESP_OK ||
        uart_driver_install(UART_NUM_1, 1024, 0, 0, nullptr, 0) != ESP_OK)
        return false;
    const char *commands[] = {"AT+RX=2\r\n", "AT+MODE=1\r\n"};
    for (int i = 0; i < 2; ++i) {
        uart_write_bytes(UART_NUM_1, commands[i], std::strlen(commands[i]));
        vTaskDelay(pdMS_TO_TICKS(i ? 500 : 700));
        uint8_t b[128];
        int n = uart_read_bytes(UART_NUM_1, b, sizeof(b), pdMS_TO_TICKS(30));
        if (n > 0)
            uart_rx_ = true;
    }
    i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    cfg.dma_desc_num = 6;
    cfg.dma_frame_num = 512;
    cfg.auto_clear_after_cb = true;
    if (i2s_new_channel(&cfg, &tx_, &rx_) != ESP_OK)
        return false;
    i2s_std_config_t stdcfg{};
    stdcfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
    stdcfg.slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    stdcfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
#if SOC_I2S_HW_VERSION_2
    stdcfg.slot_cfg.left_align = true;
#endif
    stdcfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    stdcfg.gpio_cfg.bclk = AUDIO_I2S_BCLK;
    stdcfg.gpio_cfg.ws = AUDIO_I2S_WS;
    stdcfg.gpio_cfg.dout = AUDIO_I2S_DOUT;
    stdcfg.gpio_cfg.din = AUDIO_I2S_DIN;
    i2s_event_callbacks_t callbacks{};callbacks.on_sent=tx_sent;
    if (i2s_channel_init_std_mode(tx_, &stdcfg) != ESP_OK ||
        i2s_channel_init_std_mode(rx_, &stdcfg) != ESP_OK ||
        i2s_channel_register_event_callback(tx_,&callbacks,this)!=ESP_OK || i2s_channel_enable(tx_) != ESP_OK ||
        i2s_channel_enable(rx_) != ESP_OK)
        return false;
    requests_ = xQueueCreate(1, sizeof(Request));
    results_ = xQueueCreate(4, sizeof(Result));
    static_assert(std::is_trivially_copyable<wordcard::TtsTestResult>::value,"FreeRTOS queue requires POD");
    test_results_=xQueueCreate(1,sizeof(wordcard::TtsTestResult));
    if (!requests_ || !results_ || !test_results_ ||
        xTaskCreatePinnedToCore(task, "wordcard.audio", 24576, this, 6, nullptr, 1) != pdPASS)
        return false;
    ready_ = true;
    reload_tts();
    ESP_LOGI(TAG, "I2S slave ready; BH UART response=%s", uart_rx_ ? "yes" : "unconfirmed");
    return true;
}
void AudioPlayer::reload_tts() {
    // Candidate only. The owning worker checks assets on first actual synthesis.
    if (!busy_) tts_available_ = storage_mounted_ && !tts_.failed();
}
bool AudioPlayer::submit(int mode, const std::string &path, const std::string &text,int test_engine,int test_phrase,uint32_t test_id,bool reuse,uint32_t client_token) {
    if (!ready_ || path.size() >= sizeof(Request::path) || text.size() >= sizeof(Request::text) || busy_.exchange(true))
        return false;
    cancel_ = false;
    Request r;
    r.client_token=client_token;
    r.mode = mode;
    r.reuse=reuse;
    r.test_engine=test_engine;r.test_phrase=test_phrase;r.test_id=test_id;
    std::memcpy(r.path, path.c_str(), path.size() + 1);
    std::memcpy(r.text, text.c_str(), text.size() + 1);
    speech_source_ = mode==0 ? (path.empty() ? wordcard::SpeechSource::Synthesis : wordcard::SpeechSource::Sd) : wordcard::SpeechSource::None;
    if (xQueueSend(requests_, &r, 0) != pdTRUE) {
        busy_ = false;
        return false;
    }
    return true;
}
bool AudioPlayer::play(const std::string &p, const std::string &text,uint32_t client_token) {
    return submit(0, p, text,0,0,0,false,client_token);
}
void AudioPlayer::stop() {
    cancel_=true;
    if(!busy_ && test_ready_)submit(7);
}
bool AudioPlayer::suspend_io() {
    if(busy_)return false;
    // The slave RX channel keeps receiving mic samples even when no recording
    // request is active. Stop its DMA before a CPU-only restart reuses RAM.
    auto stop_channel=[](i2s_chan_handle_t channel,bool &suspended) {
        if(!channel || suspended)return true;
        const auto err=i2s_channel_disable(channel);
        suspended=err==ESP_OK;
        return err==ESP_OK || err==ESP_ERR_INVALID_STATE;
    };
    if(!stop_channel(rx_,rx_suspended_) || !stop_channel(tx_,tx_suspended_)) {
        resume_io();return false;
    }
    ESP_LOGI(TAG,"I2S RX/TX DMA stopped for USB restart");
    return true;
}
void AudioPlayer::resume_io() {
    if(rx_suspended_ && i2s_channel_enable(rx_)==ESP_OK)rx_suspended_=false;
    if(tx_suspended_ && i2s_channel_enable(tx_)==ESP_OK)tx_suspended_=false;
}
bool AudioPlayer::benchmark(wordcard::TtsTestMode mode,int phrase,uint32_t run_id,bool reuse) {
    if(int(mode)<0 || int(mode)>=wordcard::tts_mode_count || phrase<0 || phrase>=6)return false;
    return submit(5,"","",int(mode),phrase,run_id,reuse);
}
bool AudioPlayer::benchmark_play(uint32_t run_id) {return test_ready_ && submit(6,"","",0,0,run_id);}
bool AudioPlayer::benchmark_result(wordcard::TtsTestResult &r) {
    return test_results_ && xQueueReceive(test_results_,&r,0)==pdTRUE;
}
bool AudioPlayer::tone() {
    return submit(1);
}
bool AudioPlayer::mic_test() {
    finish_capture_ = false;
    return submit(2);
}
bool AudioPlayer::record() {
    finish_capture_ = false;
    if (!submit(3))
        return false;
    recording_ = true;
    return true;
}
bool AudioPlayer::replay() {
    return clip_ready_ && submit(4);
}
bool AudioPlayer::result(std::string &text, int &kind,std::string &path,uint32_t *client_token) {
    Result r{};
    if (!results_ || xQueueReceive(results_, &r, 0) != pdTRUE)
        return false;
    text = r.text;
    kind = r.kind;
    path = r.path;
    if(client_token)*client_token=r.client_token;
    return true;
}
void AudioPlayer::report(const std::string &text, int kind,const char *path) {
    Result r;
    r.client_token=active_client_token_;
    std::snprintf(r.text, sizeof(r.text), "%s", text.c_str());
    r.kind = kind;
    std::snprintf(r.path,sizeof(r.path),"%s",path);
    xQueueSend(results_, &r, 0);
    ESP_LOGI(TAG, "%s", r.text);
}
void AudioPlayer::task(void *p) {
    static_cast<AudioPlayer *>(p)->run();
}
bool AudioPlayer::tx_sent(i2s_chan_handle_t,i2s_event_data_t *event,void *ctx) {
    auto &self=*static_cast<AudioPlayer *>(ctx);
    portENTER_CRITICAL_ISR(&self.tx_probe_lock_);
    auto *capture=self.tx_probe_.load(std::memory_order_acquire);
    if(!capture){portEXIT_CRITICAL_ISR(&self.tx_probe_lock_);return false;}
    const uint32_t now=uint32_t(esp_timer_get_time());
    if(self.tx_probe_last_us_)self.tx_probe_max_gap_us_=std::max(self.tx_probe_max_gap_us_,now-self.tx_probe_last_us_);
    self.tx_probe_last_us_=now;
    const size_t n=std::min(event->size/sizeof(int32_t),self.tx_probe_capacity_-self.tx_probe_count_);
    std::memcpy(capture+self.tx_probe_count_,event->dma_buf,n*sizeof(int32_t));
    self.tx_probe_count_+=n;
    portEXIT_CRITICAL_ISR(&self.tx_probe_lock_);
    return false;
}
void AudioPlayer::finish_tx_probe(int phrase,int mode) {
    auto *capture=tx_probe_.load();if(!capture)return;
    // Synchronize with ISR before reading/releasing its data. No new request is
    // accepted until this worker returns. RX remains available for the board.
    if(i2s_channel_disable(tx_)!=ESP_OK) {
        ESP_LOGE(TAG,"TX_PROBE disable failed; buffer retained to avoid ISR use-after-free");return;
    }
    portENTER_CRITICAL(&tx_probe_lock_);
    tx_probe_=nullptr;
    portEXIT_CRITICAL(&tx_probe_lock_);
    char path[160];std::snprintf(path,sizeof(path),"/sdcard/metalio/e-ink/wordcards/diagnostics/tx-%02d-%s.s32le",
        phrase+1,mode==0?"simd":mode==1?"scalar":mode==3?"pico":mode==4?"amy":"espeak");
    FILE *file=std::fopen(path,"wb");bool saved=false;
    if(file){saved=std::fwrite(capture,sizeof(int32_t),tx_probe_count_,file)==tx_probe_count_;if(std::fclose(file))saved=false;}
    ESP_LOGI(TAG,"TX_PROBE words=%u capacity=%u gap_max_us=%lu saved=%d file=%s",
        unsigned(tx_probe_count_),unsigned(tx_probe_capacity_),(unsigned long)tx_probe_max_gap_us_,int(saved),path);
    heap_caps_free(capture);
    // Restart with silent descriptors; the capture never leaks old speech into
    // subsequent playback after disabling/re-enabling the DMA ring.
    int32_t zero[1024]{};size_t loaded=0;
    for(int i=0;i<6;++i) {
        if(i2s_channel_preload_data(tx_,zero,sizeof(zero),&loaded)!=ESP_OK || !loaded)break;
    }
    if(i2s_channel_enable(tx_)!=ESP_OK)ESP_LOGE(TAG,"TX_PROBE TX restart failed");
}
bool AudioPlayer::write(const int16_t *samples, size_t count,uint32_t gain) {
    int32_t stereo[512 * 2];
    size_t pos = 0;
    while (pos < count && !cancel_) {
        size_t n = std::min<size_t>(512, count - pos);
        const int v = volume_;
        for (size_t i = 0; i < n; ++i)
            stereo[i * 2] = stereo[i * 2 + 1] = wordcard::speech_i2s_sample(samples[pos+i],v,gain);
        size_t sent = 0;
        auto err = i2s_channel_write(tx_, stereo, n * 8, &sent, 300);
        if (err != ESP_OK || sent != n * 8) {
            ESP_LOGE(TAG, "I2S write failed %s bytes=%u", esp_err_to_name(err), unsigned(sent));
            return false;
        }
        pos += n;
    }
    return !cancel_;
}
#include "audio_benchmark.inc"
#include "audio_worker.inc"
