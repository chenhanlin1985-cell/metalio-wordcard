// Exercise the production shutdown sequence; only hardware and storage edges are fakes.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#define ESP_LOGE(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
constexpr int ESP_OK=0;
static const char *TAG="test";
static uint64_t now=0,release_at=0,audio_idle_at=0;
static bool key_cancelled=false,keyboard_haptics=true;
static int unmount_result=0;
static std::vector<std::string> calls;
uint64_t esp_timer_get_time(){return now;}
void vTaskDelay(int ms){now+=uint64_t(ms)*1000;}
bool power_key_released(){return now>=release_at;}
void power_key_cancel(){key_cancelled=true;calls.push_back("cancel");}
void keyboard_deactivate(){calls.push_back("keyboard-stop");}
void app_vibe_pulse(){calls.push_back("feedback");}
void metalio_touch_set_feed_paused(bool){calls.push_back("touch-stop");}
int esp_vfs_fat_sdcard_unmount(const char *,void *){calls.push_back("unmount");return unmount_result;}
struct Cut {};
[[noreturn]] void power_cut_until_off(){calls.push_back("cut");throw Cut{};}
enum class Screen{Home,PowerOff};
struct App {
    struct {Screen screen=Screen::Home;int power_phase=0;}view_;
    struct {struct {bool haptics=true;}state;void pause(){calls.push_back("pause");}}learning_;
    struct {void cancel(){calls.push_back("tts-cancel");}}tts_test_;
    struct {bool suspend_ok=true;bool busy(){return now<audio_idle_at;}
        bool suspend_io(){calls.push_back("audio-suspend");return suspend_ok;}
        void resume_io(){calls.push_back("audio-resume");}}audio_;
    struct {void close(){calls.push_back("dictionary-close");}}dictionary_;
    bool dirty_=false,personal_deck_dirty_=false,save_ok=true,sync_ok=true,sd_=true;
    void *sd_card_=reinterpret_cast<void*>(1);std::string notice_;
    void keyboard_poweroff(){calls.push_back("panel-idle");}
    void leave_player(){calls.push_back("audio-stop");}
    void render(){calls.push_back("render-"+std::to_string(view_.power_phase));}
    void load_dictionary(){calls.push_back("dictionary-open");}
    bool sync_personal_deck(){calls.push_back("sync-personal");return sync_ok;}
    bool save(){assert(power_key_released());assert(!audio_.busy());calls.push_back("save");return save_ok;}
    void power_off();
};
#include "app_power.inc"
static bool has(const char *name){return std::find(calls.begin(),calls.end(),name)!=calls.end();}
static size_t pos(const char *name){return std::find(calls.begin(),calls.end(),name)-calls.begin();}
static App fresh(){now=0;release_at=100000;audio_idle_at=0;key_cancelled=false;unmount_result=0;calls.clear();return {};}
int main(){
    auto a=fresh();audio_idle_at=80000;
    try{a.power_off();assert(false);}catch(const Cut&){}
    assert(pos("audio-stop")<pos("save") && pos("save")<pos("unmount"));
    assert(pos("audio-suspend")<pos("unmount") && pos("unmount")<pos("render-2") && pos("render-2")<pos("cut"));
    assert(a.sd_card_==nullptr && !a.sd_ && !key_cancelled);
    a=fresh();a.save_ok=false;a.power_off();assert(key_cancelled && !has("unmount") && !has("cut") && a.view_.screen==Screen::Home);
    a=fresh();a.personal_deck_dirty_=true;a.sync_ok=false;a.power_off();assert(key_cancelled && !has("save") && !has("cut"));
    a=fresh();a.audio_.suspend_ok=false;a.power_off();assert(has("save") && !has("unmount") && !has("cut"));
    a=fresh();unmount_result=-1;a.power_off();assert(has("dictionary-open") && has("audio-resume") && key_cancelled && !has("cut") && a.sd_);
    a=fresh();audio_idle_at=6000000;a.power_off();assert(now<=5020000 && !has("save") && key_cancelled && !has("cut"));
    a=fresh();release_at=40000000;a.power_off();assert(now<=30020000 && !has("save") && key_cancelled && !has("cut"));
    puts("PASS: production power-off orders stop/release/save/unmount/cut; failures retain data and abort without power cut");
}
