void App::usb_tick() {
    if(!usb_pending_)return;
    auto fail=[&](const std::string &message) {
        usb_pending_=false;view_.usb_phase=3;view_.usb_error=message;dirty_=true;
    };
    if(audio_.busy()) {
        if(uint64_t(esp_timer_get_time())-usb_wait_since_>5000000)
            fail("音频尚未停止，请稍后重试");
        return;
    }
    if(dirty_)return; // Show the saving/transition state before restarting.
    learning_.pause();
    if(!sd_ || !view_.writable || !progress_valid_) {
        fail("无法保存学习进度，请检查 SD 卡后重试");return;
    }
    if((personal_deck_dirty_ && !sync_personal_deck()) || !save()) {
        fail("进度未保存，已取消切换；请检查 SD 卡");return;
    }
    if(!audio_.suspend_io()) {fail("音频接口尚未停止，请稍后重试");return;}
    dictionary_.close();
    // IDF 6.1's Picolibc fflush dereferences FILE immediately; unlike newlib,
    // fflush(nullptr) is not supported (device fault at FILE+0x14). SD writers
    // above already flush/fsync/close explicitly before FAT is unmounted.
    fflush(stdout);fflush(stderr);
    const auto err=esp_vfs_fat_sdcard_unmount("/sdcard",sd_card_);
    if(err!=ESP_OK) {
        audio_.resume_io();
        load_dictionary();fail(std::string("SD 卡无法安全卸载：")+esp_err_to_name(err));return;
    }
    sd_card_=nullptr;sd_=false;
    // Both writers are quiescent and FAT has been unmounted. A one-shot boot
    // request prevents stale audio/front-end handles from crossing USB ownership.
    ESP_LOGI(TAG,"USB_RESTART progress saved, FAT unmounted, audio idle");
    fflush(stdout);vTaskDelay(pdMS_TO_TICKS(100));usb_disk_request_boot();
}
