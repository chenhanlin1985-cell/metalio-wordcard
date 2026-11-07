#include "power_control.hpp"
#include "power_button.hpp"
#include "config.h"
#include "epd_board_metalio_eink4_plus.h"
#include "frontlight.h"
#include <atomic>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr const char *TAG="wordcard_power";
std::atomic<int> phase{0}; // 0 listening, 1 confirmed hold, 2 released
std::atomic<bool> reset_requested{false};
std::atomic<unsigned> failures{0},raw_port{255};
std::atomic<bool> valid_sample{false};
TaskHandle_t sampler=nullptr;
void sample(void *) {
    wordcard::PowerButton button;
    bool previous_high=true,previous_valid=false;
    for(;;) {
        if(reset_requested.exchange(false)){button={};phase=0;}
        uint8_t raw=0xff;
        const bool valid=epd_board_metalio_eink4_plus_ioexp_read_port(1,&raw);
        const bool high=(raw & (1u<<(EPD_IO_POWER&7)))!=0;
        valid_sample=valid;raw_port=raw;
        if(!valid)++failures;
        if(valid && (!previous_valid || high!=previous_high))
            ESP_LOGI(TAG,"POWER_KEY %s raw=%02x",high?"UP":"DOWN",unsigned(raw));
        if(button.update(valid,high,uint64_t(esp_timer_get_time()))) {
            phase=1;
            ESP_LOGI(TAG,"POWER_HOLD confirmed; release key before cutting power");
        }
        if(button.released_after_hold() && phase.load()==1) {
            phase=2;ESP_LOGI(TAG,"POWER_RELEASE confirmed");
        }
        previous_high=high;previous_valid=valid;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}
bool power_key_start() {
    if(sampler)return true;
    const bool ok=xTaskCreate(sample,"power_key",3072,nullptr,5,&sampler)==pdPASS;
    if(!ok)ESP_LOGE(TAG,"Power key sampler unavailable");
    return ok;
}
bool power_key_requested(){return phase.load()!=0;}
bool power_key_released(){return phase.load()==2;}
void power_key_cancel(){reset_requested=true;phase=0;}
void power_key_status(){ESP_LOGI(TAG,"POWER_STATE phase=%d valid=%d raw=%02x read_failures=%u sampler=%d",phase.load(),int(valid_sample.load()),raw_port.load(),failures.load(),sampler!=nullptr);}
[[noreturn]] void power_cut_until_off() {
    epd_board_metalio_eink4_plus_pa_set(false);
    epd_board_metalio_eink4_plus_motor_set(false);
    Frontlight::GetInstance().ForceAllLow();
    ESP_LOGW(TAG,"POWER_CUT resources closed; pulse until hardware removes power (no software restart)");
    // Match the manufacturer's dedicated shutdown task: do not return to the
    // application after one train, and never replace failed power-off by reboot.
    for(;;) {
        epd_board_metalio_eink4_plus_pwr_key_pulse_train();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
