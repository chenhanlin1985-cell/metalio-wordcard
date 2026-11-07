static void save_test_wave(const SpeechPcm &pcm,int phrase,int mode) {
    const std::string dir="/sdcard/metalio/e-ink/wordcards/diagnostics";
    mkdir(dir.c_str(),0775);
    char name[48];std::snprintf(name,sizeof(name),"/tts-%02d-%s.wav",phrase+1,wordcard::tts_test_file_tag(wordcard::TtsTestMode(mode)));
    const auto path=dir+name;
    unsigned char header[44]{};
    const auto u32=[&](int off,uint32_t x){for(int i=0;i<4;++i)header[off+i]=(x>>(8*i))&255;};
    std::memcpy(header,"RIFF",4);u32(4,uint32_t(36+pcm.count*2));std::memcpy(header+8,"WAVEfmt ",8);
    u32(16,16);header[20]=1;header[22]=1;u32(24,16000);u32(28,32000);header[32]=2;header[34]=16;
    std::memcpy(header+36,"data",4);u32(40,uint32_t(pcm.count*2));
    // FAT rename does not replace an existing destination. Use the project's
    // tmp/backup writer so repeating a test can replace its own diagnostics.
    std::string data(reinterpret_cast<const char *>(header),44),error;
    data.append(reinterpret_cast<const char *>(pcm.data.get()),pcm.count*2);
    const bool ok=wordcard::atomic_write(path,data,error);
    ESP_LOGI("wordcard.audio","PCM_EXPORT %s file=%s",ok?"ok":"failed",path.c_str());
}
void AudioPlayer::run_benchmark(const Request &r) {
    if(r.mode==7) {test_pcm_={};test_ready_=false;return;}
    if(r.mode==5) {
        speech_cache_.open(speech_cache_folder_);
        const auto key=wordcard::tts_audio_key(wordcard::tts_phrases[r.test_phrase].text,wordcard::tts_test_cache_tag(wordcard::TtsTestMode(r.test_engine)));
        if(r.reuse) {
            bool hit=test_pcm_.data && test_result_.generated && test_result_.phrase==r.test_phrase && int(test_result_.requested)==r.test_engine;
            bool persisted=hit && test_result_.persisted;
            wordcard::CachedSpeech saved;
            if(!hit && speech_cache_.load(key,saved)) {
                test_pcm_={};
                test_pcm_.data.reset(static_cast<int16_t *>(heap_caps_malloc(saved.pcm.size()*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
                if(test_pcm_.data) {
                    test_pcm_.count=saved.pcm.size();std::memcpy(test_pcm_.data.get(),saved.pcm.data(),saved.pcm.size()*2);
                    test_result_={};test_result_.actual=wordcard::TtsActual(saved.actual);hit=persisted=true;
                }
            }
            if(hit) {
                const auto actual=test_result_.actual;test_result_={};
                test_result_.available=test_result_.generated=test_result_.cached=true;test_result_.persisted=persisted;
                test_result_.run_id=r.test_id;test_result_.phrase=r.test_phrase;test_result_.requested=wordcard::TtsTestMode(r.test_engine);
                test_result_.actual=actual;test_result_.samples=test_pcm_.count;test_result_.volume=volume_;
                test_result_.pcm_crc=wordcard::tts_pcm_crc(test_pcm_.data.get(),test_pcm_.count);
                test_result_.cancelled=cancel_;test_ready_=!cancel_;
                if(cancel_){test_pcm_={};test_result_.generated=false;}
                ESP_LOGI(TAG,"TTS_CACHE_HIT test phrase=%d mode=%d samples=%u",r.test_phrase+1,r.test_engine,unsigned(test_pcm_.count));
                xQueueOverwrite(test_results_,&test_result_);return;
            }
        }
        // Release playback cache capacity BEFORE taking the memory baseline.
        std::vector<int16_t>().swap(cached_audio_);cached_path_.clear();
        test_pcm_={};test_ready_=false;
        tts_.select_test_engine(wordcard::TtsTestMode(r.test_engine));
        const uint32_t internal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        const uint32_t psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        const bool monitor=heap_caps_monitor_local_minimum_free_size_start()==ESP_OK;
        tts_.benchmark(wordcard::TtsTestMode(r.test_engine),r.test_phrase,tts_folder_,small_folder_,
            [&](){return bool(cancel_);},test_pcm_,test_result_,120000000);
        if(test_result_.generated) {
            uint64_t sum=0;unsigned peak=0;
            for(size_t i=0;i<test_pcm_.count;++i) {
                const int32_t v=test_pcm_.data.get()[i];
                sum+=uint64_t(int64_t(v)*v);peak=std::max(peak,unsigned(std::abs(v)));
            }
            ESP_LOGI(TAG,"PCM_LEVEL engine=%d samples=%u peak=%u rms=%.1f volume=%d",
                r.test_engine,unsigned(test_pcm_.count),peak,std::sqrt(double(sum)/test_pcm_.count),int(volume_));
        }
        test_result_.run_id=r.test_id;test_result_.volume=volume_;
        test_result_.internal_before=internal;test_result_.psram_before=psram;
        test_result_.internal_after=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        test_result_.psram_after=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(monitor) {
            test_result_.internal_min=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            test_result_.psram_min=heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            test_result_.memory_valid=heap_caps_monitor_local_minimum_free_size_stop()==ESP_OK;
        }
        test_ready_=test_result_.generated && !cancel_;
        if(cancel_){test_result_.cancelled=true;test_pcm_={};test_ready_=false;}
        // Raw PCM for host/device comparisons; export is outside synthesis and
        // heap measurement. These generated diagnostics never replace resources.
        if(test_ready_) {
            save_test_wave(test_pcm_,r.test_phrase,r.test_engine);
            const uint32_t gain=wordcard::speech_playback_gain(test_pcm_.data.get(),test_pcm_.count);
            test_result_.persisted=speech_cache_.store(key,test_pcm_.data.get(),test_pcm_.count,gain,uint32_t(test_result_.actual));
            ESP_LOGI(TAG,"TTS_CACHE_SAVE test ok=%d",int(test_result_.persisted));
        }
        xQueueOverwrite(test_results_,&test_result_);
        return;
    }
    // Generation and playback are separate requests. The App refreshes the
    // result/stop controls while idle, then submits playback of this exact PCM.
    if(!test_ready_ || r.test_id!=test_result_.run_id)return;
    test_ready_=false;test_result_.play_event=true;
    // The exact digital stream consumed by DMA isolates playback corruption
    // from model artifacts and analog speaker coloration. No inference timing.
    if(capture_dma_ && !tx_probe_.load()) {
        const size_t words=(test_pcm_.count+16000)*2;
        auto *capture=static_cast<int32_t *>(heap_caps_malloc(words*sizeof(int32_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(capture) {
            tx_probe_capacity_=words;tx_probe_count_=0;tx_probe_last_us_=tx_probe_max_gap_us_=0;
            tx_probe_.store(capture,std::memory_order_release);
        }
    }
    epd_board_metalio_eink4_plus_pa_set(true);
    vTaskDelay(pdMS_TO_TICKS(20));
    const auto playback_start=esp_timer_get_time();
    const auto gain=wordcard::speech_playback_gain(test_pcm_.data.get(),test_pcm_.count);
    ESP_LOGI(TAG,"PCM_GAIN q16=%lu",(unsigned long)gain);
    bool ok=write(test_pcm_.data.get(),test_pcm_.count,gain);
    if(ok && !cancel_) {int16_t silence[512]{};ok=write(silence,512);vTaskDelay(pdMS_TO_TICKS(120));}
    epd_board_metalio_eink4_plus_pa_set(false);
    ESP_LOGI(TAG,"PCM_PLAY samples=%u elapsed_us=%lld ok=%d",unsigned(test_pcm_.count),
        (long long)(esp_timer_get_time()-playback_start),int(ok));
    finish_tx_probe(test_result_.phrase,int(test_result_.requested));
    test_result_.cancelled=cancel_;test_result_.played=ok && !cancel_;
    if(!test_result_.played)std::snprintf(test_result_.error,sizeof(test_result_.error),"%s",cancel_?"试听已停止":"试听失败，请检查音频时钟");
    if(cancel_ || !ok)test_pcm_={}; // Successful PCM stays available for instant replay.
    xQueueOverwrite(test_results_,&test_result_);
}
