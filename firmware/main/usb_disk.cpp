#include "usb_disk.hpp"
#include "wordcard_ui.hpp"
#include "power_control.hpp"
#include "config.h"
#include "metalio_sd.h"
#include "metalio_epd.h"
#include "metalio_touch.h"
#include "epd_board_metalio_eink4_plus.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <driver/sdmmc_host.h>
#include <driver/usb_serial_jtag.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#include <tinyusb_msc.h>
#include <soc/usb_dwc_struct.h>

namespace {
constexpr const char *TAG="wordcard_usb";
constexpr uint32_t kCookie=0x57435535;
RTC_NOINIT_ATTR uint32_t request_cookie;
std::atomic<bool> attached{false},detached{false},ejected{false};
void usb_event(tinyusb_event_t *event,void*) {
    ESP_LOGI(TAG,"USB_EVENT id=%d",int(event->id));
    if(event->id==TINYUSB_EVENT_ATTACHED){attached=true;detached=false;}
    else if(event->id==TINYUSB_EVENT_DETACHED && attached.exchange(false)) detached=true;
}
void heap_log(const char *stage) {
    ESP_LOGI(TAG,"USB_MEMORY stage=%s internal=%u dma_free=%u dma_largest=%u psram=%u",
        stage,unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        unsigned(heap_caps_get_free_size(MALLOC_CAP_DMA)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)),
        unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}
}
// esp_tinyusb disables its implicit FAT remount for *both* disconnect and
// SCSI eject when auto_mount_off is set. Observe the successful SCSI command
// explicitly; USB mode never needs to mount a local filesystem.
extern "C" bool wordcard_storage_start_stop_cb(uint8_t,uint8_t,bool,bool);
extern "C" bool tud_msc_start_stop_cb(uint8_t lun,uint8_t power,bool start,bool load_eject) {
    const bool ok=wordcard_storage_start_stop_cb(lun,power,start,load_eject);
    if(ok && load_eject && !start)ejected=true;
    return ok;
}
bool usb_disk_boot_requested() {
    const bool requested=esp_reset_reason()==ESP_RST_SW && request_cookie==kCookie;
    request_cookie=0;
    return requested;
}
[[noreturn]] void usb_disk_request_boot() {
    request_cookie=kCookie;
    esp_restart();
}

[[noreturn]] void usb_disk_run() {
    // Dedicated boot: no App, local FAT mount, audio worker, dictionary or TTS.
    assert(heap_caps_check_integrity_all(true));
    epd_board_metalio_eink4_plus_boot_release_holds();
    MetalioEpd_Init();
    assert(heap_caps_check_integrity_all(true));
    metalio_touch_init();
    assert(heap_caps_check_integrity_all(true));
    gpio_config_t boot{};boot.pin_bit_mask=1ULL<<BOOT_BUTTON_GPIO;
    boot.mode=GPIO_MODE_INPUT;boot.pull_up_en=GPIO_PULLUP_ENABLE;gpio_config(&boot);
    wordcard::Font font;wordcard::UI ui;wordcard::View view;
    view.screen=wordcard::Screen::UsbDisk;view.usb_phase=1;
    esp_partition_mmap_handle_t mapping=0;const void *bytes=nullptr;
    const auto *part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        esp_partition_subtype_t(0x40),"font_data");
    const bool has_font=part && esp_partition_mmap(part,0,part->size,
        ESP_PARTITION_MMAP_DATA,&bytes,&mapping)==ESP_OK &&
        font.init(static_cast<const uint8_t*>(bytes),part->size);
    auto draw=[&]() {
        if(!has_font || !MetalioEpd_Ready())return;
        wordcard::Canvas canvas(epd_hl_get_framebuffer(MetalioEpd_Hl()),font);
        ui.render(canvas,view);metalio_touch_set_feed_paused(true);epd_poweron();
        if(epd_board_metalio_eink4_plus_tps_ok()) {
            ESP_LOGI(TAG,"USB display begin phase=%d",view.usb_phase);
            epd_clear();
            auto result=epd_hl_update_screen_from_white(MetalioEpd_Hl(),MODE_GC16,25);
            ESP_LOGI(TAG,"USB display end err=%d",int(result));
        }
        epd_poweroff();metalio_touch_set_feed_paused(false);
    };
    heap_log("before-storage");
    sdmmc_host_t host=SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width=4;slot.clk=SDMMC_CLK_PIN;slot.cmd=SDMMC_CMD_PIN;
    slot.d0=SDMMC_D0_PIN;slot.d1=SDMMC_D1_PIN;slot.d2=SDMMC_D2_PIN;slot.d3=SDMMC_D3_PIN;
    slot.flags=0;MetalioSd_ReleaseHold();
    sdmmc_card_t card{};tinyusb_msc_storage_handle_t storage=nullptr;
    bool host_live=false,msc_live=false,usb_live=false;
    const char *stage="SD 主机";
    esp_err_t err=host.init();host_live=err==ESP_OK;
    if(err==ESP_OK){stage="SD 接口";err=sdmmc_host_init_slot(host.slot,&slot);}
    if(err==ESP_OK){stage="SD 卡";err=sdmmc_card_init(&host,&card);}
    if(err==ESP_OK){stage="SD 状态";err=sdmmc_get_status(&card);}
    if(err==ESP_OK) {
        sdmmc_card_print_info(stdout,&card);
        tinyusb_msc_driver_config_t cfg{};
        cfg.user_flags.auto_mount_off=1;
        stage="U 盘驱动";err=tinyusb_msc_install_driver(&cfg);msc_live=err==ESP_OK;
    }
    if(err==ESP_OK) {
        tinyusb_msc_storage_config_t cfg{};
        cfg.medium.card=&card;cfg.mount_point=TINYUSB_MSC_STORAGE_MOUNT_USB;
        cfg.fat_fs.do_not_format=true;cfg.fat_fs.config.format_if_mount_failed=false;
        cfg.fat_fs.config.max_files=2;
        stage="U 盘缓冲区";err=tinyusb_msc_new_storage_sdmmc(&cfg,&storage);
    }
    if(err==ESP_OK) {
        auto cfg=TINYUSB_CONFIG_HIGH_SPEED(usb_event,nullptr);
        cfg.task=TINYUSB_TASK_CUSTOM(4096,5,0);
        stage="USB 接口";err=tinyusb_driver_install(&cfg);usb_live=err==ESP_OK;
        if(usb_live)tud_disconnect(); // Advertise only after routing and the initial screen.
    }
    heap_log(err==ESP_OK?"ready":"failed");
    auto stop=[&]() -> bool {
        // Disconnect before deleting storage. Deletion refuses pending writes;
        // keep the card alive until the stack has drained, never force/free it.
        if(usb_live)tud_disconnect();
        if(storage) {
            const auto result=tinyusb_msc_delete_storage(storage);
            if(result!=ESP_OK){ESP_LOGE(TAG,"SD release: %s",esp_err_to_name(result));return false;}
            storage=nullptr;
        }
        // Keep the TinyUSB task alive until pending deferred writes have drained
        // and storage deletion succeeds. A failed delete can be retried safely.
        if(usb_live) {
            const auto result=tinyusb_driver_uninstall();
            if(result!=ESP_OK){ESP_LOGE(TAG,"USB stop: %s",esp_err_to_name(result));return false;}
            usb_live=false;
        }
        if(msc_live) {
            if(tinyusb_msc_uninstall_driver()!=ESP_OK)return false;
            msc_live=false;
        }
        if(host_live) {
            if(host.flags & SDMMC_HOST_FLAG_DEINIT_ARG)host.deinit_p(host.slot);
            else host.deinit();
            host_live=false;
        }
        epd_board_metalio_eink4_plus_ana_sw_set(true);
        return true;
    };
    if(err!=ESP_OK) {
        stop();view.usb_phase=3;
        view.usb_error=std::string(stage)+"："+esp_err_to_name(err);
        ESP_LOGE(TAG,"USB start failed at %s: %s",stage,esp_err_to_name(err));draw();
    } else {
        view.usb_phase=2;draw(); // Never refresh EPD while the host is transferring.
        ESP_LOGI(TAG,"USB_WAIT_HOST buffer=%d bytes; exclusive SD; no auto-format",CONFIG_TINYUSB_MSC_BUFSIZE);
        fflush(stdout);vTaskDelay(pdMS_TO_TICKS(100));
        epd_board_metalio_eink4_plus_ana_sw_set(false);
        gpio_input_enable(ANA_SW_GPIO);
        ESP_LOGI(TAG,"USB_ROUTE gpio=%d level=%d (low=OTG)",int(ANA_SW_GPIO),gpio_get_level(ANA_SW_GPIO));
        vTaskDelay(pdMS_TO_TICKS(100));
        tud_connect();
    }
    bool down_before=false,boot_before=gpio_get_level(BOOT_BUTTON_GPIO)==0;
    power_key_start();bool power_off=false;
    wordcard::Event pressed{};uint64_t return_at=0,diagnose_at=0;
    for(;;) {
        // No enumeration timeout: cable/host detection may take longer than 15s.
        // Users explicitly return after host eject. Some hosts suspend the bus
        // without issuing START STOP UNIT; suspend alone must never force exit.
        if(usb_live && uint64_t(esp_timer_get_time())>=diagnose_at) {
            ESP_LOGI(TAG,"USB_LINK mounted=%d connected=%d usj=%d gotgctl=%08lx gintsts=%08lx dctl=%08lx dsts=%08lx",
                attached.load(),tud_connected(),usb_serial_jtag_is_connected(),
                (unsigned long)USB_OTGHS.gotgctl_reg.val,(unsigned long)USB_OTGHS.gintsts_reg.val,
                (unsigned long)USB_OTGHS.dctl_reg.val,(unsigned long)USB_OTGHS.dsts_reg.val);
            diagnose_at=esp_timer_get_time()+5000000;
        }
        if(attached.load() && !ejected.load())return_at=0; // Host reconfiguration is not eject.
        if((ejected.load() || detached.load()) && !return_at)
            return_at=esp_timer_get_time()+700000; // Let the eject response finish.
        wordcard::Event event{};metalio_touch_point_t point{};
        const bool down=metalio_touch_read(&point);
        if(down && !down_before)pressed=ui.hit(point.x,point.y);
        if(!down && down_before)event=pressed;
        down_before=down;
        const bool boot_down=gpio_get_level(BOOT_BUTTON_GPIO)==0;
        if(boot_down && !boot_before)event={wordcard::Action::UsbExit};
        boot_before=boot_down;
        if(power_key_requested() && power_key_released()) {
            ESP_LOGI(TAG,"POWER_OFF confirmed hold and release in USB mode");
            power_off=true;event={wordcard::Action::UsbExit};
        }
        if((return_at && uint64_t(esp_timer_get_time())>=return_at) ||
           event.action==wordcard::Action::UsbExit) {
            ESP_LOGI(TAG,"USB_EXIT reason=%s",return_at?"eject/disconnect":"user");
            if(stop()) {
                if(power_off){view.screen=wordcard::Screen::PowerOff;view.power_phase=2;draw();power_cut_until_off();}
                esp_restart();
            }
            // Remain in exclusive mode on teardown failure; no local SD access.
            return_at=0;ejected=false;detached=false;power_off=false;power_key_cancel();view.usb_phase=3;
            view.usb_error="退出未完成，请稍后点击重试";draw();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
