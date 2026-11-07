void AudioPlayer::run() {
    Request r;
    for (;;) {
        if (xQueueReceive(requests_, &r, portMAX_DELAY) != pdTRUE)
            continue;
        active_client_token_=r.client_token;
        bool ok = true;
        std::string error;
        if (clear_cache_.exchange(false)) {
            cached_audio_.clear();
            cached_path_.clear();
            streamed_gain_path_.clear();
        }
        if (clear_clip_.exchange(false))
            clip_.clear();
        if(r.mode==8) {sd_read_probe();busy_=false;continue;}
        if(r.mode>=5) {run_benchmark(r);busy_=false;continue;}
        test_pcm_={};test_ready_=false;
        if (r.mode == 2 || r.mode == 3) {
            recording_ = true;
            clip_ready_ = false;
            clip_.clear();
            const size_t limit = r.mode == 2 ? 48000 : 160000;
            clip_.reserve(limit); // Configured malloc policy places large audio buffers in PSRAM.
            report(r.mode == 2 ? "录音 3 秒，请说话…" : "正在跟读 · 最长 10 秒");
            int32_t raw[1024];
            size_t bytes = 0;
            for (int i = 0; i < 8 && !cancel_; ++i)
                i2s_channel_read(rx_, raw, sizeof(raw), &bytes, 5);
            while (clip_.size() < limit && !cancel_ && !finish_capture_) {
                const size_t want = std::min<size_t>(512, limit - clip_.size());
                bytes = 0;
                const auto result = i2s_channel_read(rx_, raw, want * 8, &bytes, 300);
                if (result != ESP_OK || !bytes || bytes % 8) {
                    ok = false;
                    error = "录音读取失败，请稍后重试";
                    break;
                }
                for (size_t i = 0; i < bytes / 8; ++i)
                    clip_.push_back(int16_t(std::clamp<int32_t>(raw[i * 2] >> 12, -32768, 32767)));
            }
            recording_ = false;
            clip_ready_ = ok && !cancel_ && !clip_.empty();
            if (clip_ready_ && r.mode == 2) {
                report("录音完成，正在回放…");
                epd_board_metalio_eink4_plus_pa_set(true);
                ok = write(clip_.data(), clip_.size());
                error = "录音回放结束";
            } else if (ok && !cancel_)
                error = clip_ready_ ? "跟读已录好，可以回放对照" : "未录到声音，请再试一次";
        } else {
            epd_board_metalio_eink4_plus_pa_set(true);
            vTaskDelay(pdMS_TO_TICKS(20));
            if (r.mode == 4) {
                ok = !clip_.empty() && write(clip_.data(), clip_.size());
                error = ok ? "跟读回放结束" : "录音回放失败";
            } else if (r.mode == 1) {
                int16_t tone[512];
                for (int block = 0; block < 16 && ok && !cancel_; ++block) {
                    for (int i = 0; i < 512; ++i) {
                        const int t = block * 512 + i;
                        const double envelope =
                            std::min(1.0, std::min(t / 160.0, (8192 - t) / 160.0));
                        tone[i] = int16_t(std::sin(t * 2 * 3.141592653589793 * 660 / 16000) * 8000 *
                                          envelope);
                    }
                    ok = write(tone, 512);
                }
                error = "提示音已播放";
            } else if (!cached_audio_.empty() &&
                       ((!std::string(r.path).empty() && cached_path_ == r.path) ||
                        (std::string(r.path).empty() && cached_path_ == std::string("tts:")+r.text))) {
                ++cache_hits_;
                ok = write(cached_audio_.data(), cached_audio_.size(),cached_gain_);
                error = std::string(r.path).empty() ? "合成发音播放结束" : "SD 发音播放结束";
            } else {
                wordcard::WaveReader wave;
                if (!r.path[0] || !wave.open(r.path, error)) {
                    std::string normalized;
                    const bool speakable=wordcard::speech_text(r.text,normalized);
                    wordcard::CachedSpeech saved;
                    speech_cache_.open(speech_cache_folder_);
                    const bool cached=speakable && (speech_cache_.load(wordcard::tts_audio_key(normalized,"amysmall"),saved) ||
                        speech_cache_.load(wordcard::tts_audio_key(normalized,"espeak"),saved));
                    if(cached) {
                        ++cache_hits_;speech_source_=wordcard::SpeechSource::Synthesis;
                        cached_audio_=std::move(saved.pcm);
                        // Stored gain is legacy metadata; raw PCM remains reusable.
                        // Analyze only when loading, then retain the result in RAM.
                        cached_gain_=wordcard::speech_playback_gain(cached_audio_.data(),cached_audio_.size());
                        cached_path_=std::string("tts:")+r.text;
                        report("合成发音 · 已缓存",4);
                        ESP_LOGI(TAG,"TTS_CACHE_HIT lesson samples=%u gain_q16=%lu",unsigned(cached_audio_.size()),(unsigned long)cached_gain_);
                        ok=write(cached_audio_.data(),cached_audio_.size(),cached_gain_);error="缓存发音播放结束";
                    } else if(tts_available_ && speakable) {
                        speech_source_ = wordcard::SpeechSource::Synthesis;
                        cached_path_.clear();cached_audio_.clear();bool cacheable=true;
                        bool pa_ready=false;
                        epd_board_metalio_eink4_plus_pa_set(false);
                        ok=tts_.synthesize(tts_folder_,small_folder_,r.text,[&](const int16_t *data,size_t n) {
                            if(cancel_) return false;
                            if(!pa_ready){epd_board_metalio_eink4_plus_pa_set(true);vTaskDelay(pdMS_TO_TICKS(20));pa_ready=true;}
                            if(cacheable && cached_audio_.size()+n<=wordcard::TtsAudioCache::max_samples) cached_audio_.insert(cached_audio_.end(),data,data+n);
                            else {cacheable=false;cached_audio_.clear();}
                            return write(data,n,tts_.playback_gain());
                        },[&](){return bool(cancel_);},[&](const char *status){report(status,4);},error);
                        if(tts_.failed())
                            tts_available_=false;
                        if(ok && !cancel_) {
                            if(cacheable) {
                                cached_path_=std::string("tts:")+r.text;cached_gain_=tts_.playback_gain();
                                const bool is_amy=std::string(tts_.last_engine())=="Amy Small";
                                const bool stored=speech_cache_.store(wordcard::tts_audio_key(normalized,is_amy?"amysmall":"espeak"),
                                    cached_audio_.data(),cached_audio_.size(),cached_gain_,uint32_t(is_amy?wordcard::TtsActual::AmySmall:wordcard::TtsActual::Espeak));
                                ESP_LOGI(TAG,"TTS_CACHE_SAVE lesson ok=%d",int(stored));
                                if(!stored)report("SD 缓存未保存 · 本次仍可重听",4);
                            }
                            error="合成发音播放结束";
                        } else cached_audio_.clear();
                    } else {
                        ok = false;
                        error = "发音不可用，请补齐 SD 音频或合成资源";
                    }
                } else if (wave.samples() <= 160000) {
                    speech_source_ = wordcard::SpeechSource::Sd;
                    report("SD 发音",4);
                    cached_path_.clear();
                    cached_audio_.resize(wave.samples());
                    size_t offset = 0;
                    while (wave.remaining() && !cancel_) {
                        const size_t n = wave.read(cached_audio_.data() + offset,
                                                   std::min<size_t>(4096, wave.remaining()));
                        if (!n) {
                            ok = false;
                            error = "SD 音频读取中断";
                            break;
                        }
                        offset += n;
                    }
                    if (ok && !cancel_) {
                        cached_path_ = r.path;
                        cached_gain_=wordcard::speech_playback_gain(cached_audio_.data(),cached_audio_.size());
                        ESP_LOGI(TAG,"SD_GAIN samples=%u q16=%lu",unsigned(cached_audio_.size()),(unsigned long)cached_gain_);
                        ok = write(cached_audio_.data(), cached_audio_.size(),cached_gain_);
                        if(ok) error="SD 发音播放结束";
                    }
                } else {
                    speech_source_ = wordcard::SpeechSource::Sd;
                    report("SD 发音",4);
                    int16_t samples[4096];
                    if(streamed_gain_path_!=r.path) {
                        wordcard::SpeechLevel level;
                        while(wave.remaining() && !cancel_) {
                            const size_t n=wave.read(samples,4096);
                            if(!n){ok=false;error="SD 音频读取中断";break;}
                            level.add(samples,n);
                        }
                        if(ok && !cancel_) {
                            streamed_gain_=level.gain();streamed_gain_path_=r.path;
                            ok=wave.open(r.path,error);
                        }
                    }
                    ESP_LOGI(TAG,"SD_STREAM_GAIN q16=%lu",(unsigned long)streamed_gain_);
                    while (ok && wave.remaining() && !cancel_) {
                        const size_t n = wave.read(samples, 4096);
                        if (!n) {
                            ok = false;
                            error = "SD 音频读取中断";
                            break;
                        }
                        if (!write(samples, n,streamed_gain_)) {
                            ok = false;
                            break;
                        }
                    }
                    if(ok) error="SD 发音播放结束";
                }
            }
        }
        if (ok && !cancel_ && r.mode != 3) {
            int16_t silence[512]{};
            ok = write(silence, 512);
            vTaskDelay(pdMS_TO_TICKS(120));
        }
        epd_board_metalio_eink4_plus_pa_set(false);
        recording_ = false;
        if (cancel_)
            report("音频已停止", 3);
        else if (!ok)
            report(error.empty() ? "音频时钟超时，请检查设备" : error, 2,r.path);
        else
            report(error.empty() ? "发音播放结束" : error, 1);
        busy_ = false;
    }
}
