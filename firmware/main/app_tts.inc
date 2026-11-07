void App::tts_log(const TtsTestResult &r,bool save_to_sd) {
    char json[1600];
    std::snprintf(json,sizeof(json),
        "{\"v\":1,\"firmware\":\"0.3.5\",\"cpu_mhz\":%d,\"uptime_us\":%llu,\"run_id\":%u,\"phrase\":%d,\"requested\":%d,\"actual\":%d,"
        "\"play_event\":%d,\"generated\":%d,\"played\":%d,\"cancelled\":%d,\"frontend_warm\":%d,\"volume\":%d,\"cached\":%d,\"persisted\":%d,"
        "\"prepare_us\":%llu,\"compute_us\":%llu,\"total_us\":%llu,\"samples_16k\":%u,\"pcm_crc32\":%u,"
        "\"arena_peak_bytes\":%u,\"pie_check\":%d,\"selfcheck_us\":%llu,\"simd_calls\":%u,\"scalar_calls\":%u,"
        "\"memory_valid\":%d,\"internal_before\":%u,\"internal_min\":%u,\"internal_after\":%u,"
        "\"psram_before\":%u,\"psram_min\":%u,\"psram_after\":%u}",
        CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,(unsigned long long)esp_timer_get_time(),unsigned(r.run_id),r.phrase+1,int(r.requested),int(r.actual),
        int(r.play_event),int(r.generated),int(r.played),int(r.cancelled),int(r.warm),r.volume,int(r.cached),int(r.persisted),
        (unsigned long long)r.prepare_us,(unsigned long long)r.compute_us,(unsigned long long)r.total_us,unsigned(r.samples),unsigned(r.pcm_crc),
        unsigned(r.arena_peak),r.pie_check,(unsigned long long)r.selfcheck_us,unsigned(r.simd_calls),unsigned(r.scalar_calls),int(r.memory_valid),
        unsigned(r.internal_before),unsigned(r.internal_min),unsigned(r.internal_after),unsigned(r.psram_before),unsigned(r.psram_min),unsigned(r.psram_after));
    std::printf("TTS_RESULT %s\n",json);
    if(r.error[0])ESP_LOGW(TAG,"TTS test: %s",r.error);
    if(!save_to_sd || !sd_)return;
    const std::string dir=std::string(ROOT)+"/diagnostics";
    if(mkdir(dir.c_str(),0775)!=0 && !exists(dir)){ESP_LOGW(TAG,"TTS results: cannot create diagnostics directory");return;}
    FILE *f=std::fopen((dir+"/tts-benchmark.jsonl").c_str(),"a");
    if(!f){ESP_LOGW(TAG,"TTS results: SD log unavailable; serial record retained");return;}
    const bool written=std::fprintf(f,"%s\n",json)>0;const int close=std::fclose(f);
    if(!written || close)ESP_LOGW(TAG,"TTS results: SD append failed; serial record retained");
}
void App::tts_tick() {
    TtsTestResult r;
    while(audio_.benchmark_result(r)) {
        tts_log(r);
        if(tts_test_.accept(r) && view_.screen==Screen::TtsTest){notice_.clear();dirty_=true;}
    }
    if(view_.screen!=Screen::TtsTest || audio_.busy() || dirty_)return;
    if(tts_test_.phase==TtsTestPhase::Queued) {
        if(audio_.benchmark(tts_test_.mode,tts_test_.phrase,tts_test_.run_id,tts_test_.reuse_audio))tts_test_.phase=TtsTestPhase::Generating;
        else {
            tts_test_.phase=TtsTestPhase::Generating;
            r={};r.available=true;r.run_id=tts_test_.run_id;r.phrase=tts_test_.phrase;r.requested=tts_test_.mode;
            std::snprintf(r.error,sizeof(r.error),"音频任务未能开始，请重试");tts_test_.accept(r);dirty_=true;
        }
    } else if(tts_test_.phase==TtsTestPhase::Ready) {
        tts_test_.phase=TtsTestPhase::Playing;
        if(audio_.benchmark_play(tts_test_.run_id)){}
        else {
            r=tts_test_.results[tts_test_.phrase][int(tts_test_.mode)];r.play_event=true;
            std::snprintf(r.error,sizeof(r.error),"试听缓冲已失效，请重新测试");tts_test_.accept(r);dirty_=true;
        }
    }
}
void App::tts_serial(const std::string &line) {
    if(line=="tts capture on" || line=="tts capture off"){audio_.capture_dma(line=="tts capture on");return;}
    if(line=="tts cache ram-clear"){
        if(audio_.busy() || tts_test_.active()){std::printf("TTS_BUSY: wait for playback or stop first\n");return;}
        audio_.invalidate_cache();std::printf("TTS_RAM_CACHE_CLEAR queued=1\n");return;
    }
    if(line.rfind("tts say ",0)==0){if(!tts_test_.active())audio_.play("",line.substr(8));return;}
    if(line=="tts"){dispatch({Action::TtsTest});return;}
    if(line=="tts stop"){tts_test_.cancel();audio_.stop();dirty_=true;return;}
    if(line=="tts status"){
        std::printf("TTS_STATE phase=%d phrase=%d requested=%d active=%d pipeline=SD-cache-SmallFP32-eSpeak\n",int(tts_test_.phase),tts_test_.phrase+1,int(tts_test_.mode),int(tts_test_.active()));
        for(const auto &row:tts_test_.results)for(const auto &r:row)if(r.available)tts_log(r,false);
        return;
    }
    char option[16]{},extra;int phrase=0;
    const bool listen=line.rfind("tts listen ",0)==0;
    if(std::sscanf(line.c_str(),listen?"tts listen %15s %d %c":"tts %15s %d %c",option,&phrase,&extra)==2 && phrase>=1 && phrase<=6){
        const std::string mode=option;
        const int selected=(mode=="small" || mode=="amysmall")?0:mode=="espeak"?1:-1;
        if(selected>=0){
            if(tts_test_.active()){std::printf("TTS_BUSY: use tts stop first\n");return;}
            dispatch({Action::TtsTest});tts_test_.select_phrase(phrase-1);tts_test_.select_mode(selected);
            dispatch({listen?Action::TtsListen:Action::TtsRun,0});return;
        }
    }
    std::printf("TTS_HELP: tts | tts small/espeak 1..6 | tts listen small/espeak 1..6 | tts say TEXT | tts stop | tts status | tts cache ram-clear\n");
}
