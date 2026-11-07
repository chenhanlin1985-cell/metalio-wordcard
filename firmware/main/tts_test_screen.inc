void UI::tts_test_page(Canvas &c,const View &v) {
    const bool active=v.tts_phase==TtsTestPhase::Queued || v.tts_phase==TtsTestPhase::Generating ||
        v.tts_phase==TtsTestPhase::Ready || v.tts_phase==TtsTestPhase::Playing;
    c.text(40,54,"发音测试");c.text(41,54,"发音测试");
    button(c,{512,32,132,88},Action::Back,"返回");
    c.text(40,130,v.tts_preview?"主机界面示例 · 非设备实测":"试听优先用缓存 · 重新测试才合成");
    const auto &phrase=tts_phrases[std::clamp(v.tts_phrase,0,5)];
    c.text(40,178,std::string(phrase.title)+"  "+std::to_string(v.tts_phrase+1)+"/6");
    c.text(510,178,"音量 "+std::to_string(v.volume));
    auto lines=c.wrap(phrase.text,604);
    for(size_t i=0;i<lines.size() && i<3;++i)c.text(40,224+int(i)*42,lines[i]);
    button(c,{40,350,292,60},Action::TtsPhrase,"上一句",-1,false,!active && v.tts_phrase>0);
    button(c,{352,350,292,60},Action::TtsPhrase,"下一句",1,false,!active && v.tts_phrase<5);
    for(int i=0;i<tts_mode_count;++i)button(c,{40+i*312,426,292,88},Action::TtsMode,tts_mode_name(TtsTestMode(i)),i,int(v.tts_mode)==i,!active);
    c.text(40,544,"日常发音使用 Amy Small，生成后保存。");
    c.text(40,586,"缺资源或超时自动使用备用发音。");
    c.fill({40,650,604,1});
    c.text(40,662,"实际引擎");c.text(252,662,"合成 ms");c.text(452,662,"堆峰值 KiB");
    for(int i=0;i<tts_mode_count;++i) {
        const auto &r=v.tts_results[i];const int y=704+i*40;
        std::string name=r.available?tts_actual_name(r.actual):tts_mode_name(TtsTestMode(i));
        c.text(40,y,name);
        if(active && int(v.tts_mode)==i && (v.tts_phase==TtsTestPhase::Queued || v.tts_phase==TtsTestPhase::Generating))c.text(252,y,"生成中");
        else c.text(252,y,!r.available?"未测":!r.generated?"失败":r.cached?"缓存试听":std::to_string((r.compute_us+500)/1000));
        c.text(452,y,r.memory_valid?std::to_string((uint64_t(r.internal_peak())+r.psram_peak()+1023)/1024):"—");
    }
    c.fill({40,838,604,1});
    const auto &r=v.tts_results[int(v.tts_mode)];char text[160];
    if(r.cached && r.generated) {
        c.text(40,846,"直接播放已生成的发音");
        c.text(40,886,r.persisted?"已存 SD · 重启后仍可试听":"仅本次缓存 · SD 未保存");
        c.text(40,926,"缓存试听不计入合成性能。");
    } else if(r.available && r.generated) {
        std::snprintf(text,sizeof(text),"准备 %llu ms · 音频 %.2f s",(unsigned long long)((r.prepare_us+500)/1000),r.samples/16000.0);c.text(40,846,text);
        std::snprintf(text,sizeof(text),"合成 / 音频 %.3f · %s",r.samples?r.compute_us/(r.samples*62.5):0,r.warm?"前端已就绪":"前端首次初始化");c.text(40,886,c.ellipsize(text,604));
        if(r.memory_valid)std::snprintf(text,sizeof(text),"RAM +%u · PSRAM +%u KiB",unsigned((r.internal_peak()+1023)/1024),unsigned((r.psram_peak()+1023)/1024));
        else std::snprintf(text,sizeof(text),"内存监测不可用；详见串口日志");
        c.text(40,926,text);
    } else {
        c.text(40,846,"选择日常句或科技句，检查发音。");
        c.text(40,886,"首次生成后保存，再听无需等待。");
        c.text(40,926,"结果显示在这里，详细数据见串口。");
    }
    if(active)button(c,{40,976,604,92},Action::TtsStop,"停止本轮测试",0,true);
    else {
        button(c,{40,976,292,92},Action::TtsListen,"试听 / 再听",0,true,v.tts_test_ready && !v.audio_busy);
        button(c,{352,976,292,92},Action::TtsRun,"重新生成",0,false,v.tts_test_ready && !v.audio_busy);
    }
    std::string status=v.notice;
    if(status.empty()) {
        if(v.tts_phase==TtsTestPhase::Queued || v.tts_phase==TtsTestPhase::Generating)status="正在生成 · 可随时停止";
        else if(v.tts_phase==TtsTestPhase::Ready || v.tts_phase==TtsTestPhase::Playing)status="生成完成 · 试听期间可点击停止";
        else if(v.tts_phase==TtsTestPhase::Cancelled)status="本轮已停止，已有结果仍可查看";
        else if(v.tts_phase==TtsTestPhase::Failed)status=r.error;
        else if(v.tts_phase==TtsTestPhase::Done)status="发音完成，可以再听或换下一句";
        else status=v.tts_test_ready?"先选句子，再点击试听":"音频驱动未就绪，请查看硬件自检";
    }
    lines=c.wrap(status,604);
    for(size_t i=0;i<std::min<size_t>(2,lines.size());++i)c.text(40,1080+int(i)*38,c.ellipsize(lines[i],604));
    c.text(40,1172,"峰值按各堆低水位估算，包含后台。");
}
