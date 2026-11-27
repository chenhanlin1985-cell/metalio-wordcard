#include "power_control.hpp"
#include "metalio_i2c1.h"
#include "power_i2c.h"
#include <algorithm>
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
int home_battery_percent() {
    // Called only by the display owner on a normal home redraw. The board's
    // BQ27220 reports millivolts, little-endian, on the shared display I2C bus.
    static int cached = -1;
    static int64_t next_sample_us = 0;
    const auto now = esp_timer_get_time();
    if (now < next_sample_us) return cached;
    const power_i2c_t bus{I2C_NUM_1, metalio_i2c1_lock, metalio_i2c1_unlock};
    uint8_t data[2]{};
    const auto err = power_i2c_write_read(&bus, 0x55, 0x08, data, sizeof(data), 30);
    const int mv = data[0] | (int(data[1]) << 8);
    cached = -1;
    if (err == ESP_OK && mv >= 2500 && mv <= 5000) {
        // Metalio Plus voltage endpoints. Round to 5% for an approximate reading.
        const int range_mv = std::clamp(mv - 3300, 0, 1030);
        cached = 5 * ((range_mv * 20 + 515) / 1030);
    }
    next_sample_us = now + (cached < 0 ? 5000000 : 60000000);
    ESP_LOGI(TAG,"HOME_BATTERY mv=%d estimate=%d err=%d",mv,cached,int(err));
    return cached;
}
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
