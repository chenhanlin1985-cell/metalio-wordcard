void App::power_off() {
    const Screen origin=view_.screen;
    auto abort=[&](const std::string &message){
        power_key_cancel();view_.screen=origin;notice_=message;dirty_=true;
        ESP_LOGE(TAG,"POWER_ABORT %s",message.c_str());
    };
    keyboard_deactivate();keyboard_poweroff();keyboard_haptics=false;
    if(learning_.state.haptics)app_vibe_pulse();
    tts_test_.cancel();leave_player();
    const uint64_t stop_at=uint64_t(esp_timer_get_time())+5000000;
    while(audio_.busy() && uint64_t(esp_timer_get_time())<stop_at)vTaskDelay(pdMS_TO_TICKS(20));
    if(audio_.busy()){abort("发音尚未停止，已取消关机，请稍后重试");return;}
    view_.screen=Screen::PowerOff;view_.power_phase=power_key_released()?1:0;
    dirty_=true;render();
    const uint64_t release_at=uint64_t(esp_timer_get_time())+30000000;
    while(!power_key_released() && uint64_t(esp_timer_get_time())<release_at)vTaskDelay(pdMS_TO_TICKS(20));
    if(!power_key_released()){abort("未检测到电源键松开，已取消关机");return;}
    learning_.pause();
    if((personal_deck_dirty_ && !sync_personal_deck()) || !save()) {
        abort("进度未保存，已取消关机，请检查 SD 卡");return;
    }
    if(!audio_.suspend_io()){abort("音频接口尚未停止，已取消关机，请重试");return;}
    dictionary_.close();
    const auto err=esp_vfs_fat_sdcard_unmount("/sdcard",sd_card_);
    if(err!=ESP_OK){audio_.resume_io();load_dictionary();abort("SD 卡暂时无法安全卸载，已取消关机");return;}
    sd_card_=nullptr;sd_=false;
    view_.power_phase=2;dirty_=true;render();
    keyboard_deactivate();keyboard_haptics=false;metalio_touch_set_feed_paused(true);
    fflush(stdout);fflush(stderr);
    power_cut_until_off();
}
