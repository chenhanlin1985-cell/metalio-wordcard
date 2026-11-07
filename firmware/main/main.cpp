#include "audio_player.hpp"
#include "audio_identity.hpp"
#include "builtin_decks.hpp"
#include "config.h"
#include "epd_board_metalio_eink4_plus.h"
#include "frontlight.h"
#include "metalio_epd.h"
#include "metalio_i2c1.h"
#include "metalio_touch.h"
#include "metalio_vibe.h"
#include "pcf8563.h"
#include "wordcard.hpp"
#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include "web_keyboard.hpp"
#include "web_disclosure.hpp"
#include "web_refresh.hpp"
#include "panel_power_lease.hpp"
#include "epd_board.h"
#include "web_frame_cache.hpp"
#include "keyboard_fast_refresh.hpp"
#include "usb_disk.hpp"
#include "power_control.hpp"
#include "sd_read_probe.hpp"
#include "resource_cache_io.hpp"
#include "my_waveform.h"
#include "text_waveform_policy.hpp"
#include "text_waveform.hpp"
#include "quality_regions.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <dirent.h>
#include <driver/sdmmc_host.h>
#include <driver/usb_serial_jtag.h>
#include <driver/usb_serial_jtag_vfs.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/idf_additions.h>
#include <fstream>
#include <nvs.h>
#include <nvs_flash.h>
#include <sdmmc_cmd.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
using namespace wordcard;
static constexpr const char *ROOT = "/sdcard/metalio/e-ink/wordcards";
static constexpr const char *TAG = "wordcard";
extern "C" void wordcard_boot_io_ready(void){
    MetalioVibe_Init();MetalioVibe_Pulse();
    ESP_LOGI(TAG,"BOOT_STAGE power_ack elapsed_ms=%lld",(long long)esp_timer_get_time()/1000);
}
static esp_err_t (*sd_transaction)(int,sdmmc_command_t *)=nullptr;
static bool (*sd_buffer_aligned)(int,const void *,size_t)=nullptr;
static bool sd_dma_buffer_usable(int slot,const void *buf,size_t bytes) {
    if(!buf || !bytes || uintptr_t(buf)>UINTPTR_MAX-(bytes-1))return false;
    const void *last=reinterpret_cast<const void *>(uintptr_t(buf)+bytes-1);
    // S31 can allocate task stacks in LP SRAM (0x2e00xxxx). Alignment alone
    // does not make that memory SDMMC DMA accessible. Returning false lets
    // the IDF sector layer use its DMA-capable bounce buffer, for reads/writes.
    const bool reachable=(esp_ptr_dma_capable(buf) && esp_ptr_dma_capable(last)) ||
                         (esp_ptr_external_ram(buf) && esp_ptr_external_ram(last));
    return reachable && sd_buffer_aligned(slot,buf,bytes);
}
static esp_err_t trace_sd_transaction(int slot,sdmmc_command_t *cmd) {
    const auto err=sd_transaction(slot,cmd);
    if(cmd->data && (err!=ESP_OK || cmd->error!=ESP_OK))
        ESP_LOGE("sdprobe","transaction core=%d op=%u bytes=%u buf=%p ret=%x cmd_error=%x",
            int(xPortGetCoreID()),unsigned(cmd->opcode),unsigned(cmd->datalen),cmd->data,unsigned(err),unsigned(cmd->error));
    return err;
}
struct BookSource {
    std::string filename, name, description;
    const char *builtin_data;
    size_t total;
};
// The library and Home consume catalog data rather than defining one tab per exam.
static const BookSource BOOKS[] = {
    {"KET_2025_OFFICIAL.csv", "KET 官方词汇", "考试英语 · 2025 版", KET_DATA, 1997},
    {"PET_2024.csv", "PET 核心词汇", "考试英语 · 2024 版", PET_DATA, 1871},
};
static std::atomic<int> virtual_key{0};
static std::atomic<uint64_t> input_ready_us{0}, touch_started_us{0};
static KeyboardInput keyboard_input;
static portMUX_TYPE keyboard_mux=portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint32_t> keyboard_epoch{0};
static std::atomic<bool> keyboard_haptics{false};
static SemaphoreHandle_t vibe_mutex=nullptr;
static TaskHandle_t keyboard_feedback_task=nullptr;
static std::atomic<uint64_t> button_feedback_requested_us{0};
static void app_vibe_pulse() {
    if(vibe_mutex&&xSemaphoreTake(vibe_mutex,pdMS_TO_TICKS(30))==pdTRUE){MetalioVibe_Pulse();xSemaphoreGive(vibe_mutex);}
}
static void keyboard_feedback(void *) {
    for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);const auto requested=button_feedback_requested_us.exchange(0);
        if(keyboard_haptics){app_vibe_pulse();if(requested)ESP_LOGI(TAG,"TOUCH_ACK queue_to_pulse_us=%llu",(unsigned long long)(esp_timer_get_time()-requested));}}
}
static void keyboard_deactivate() {
    portENTER_CRITICAL(&keyboard_mux);keyboard_input.deactivate();++keyboard_epoch;portEXIT_CRITICAL(&keyboard_mux);
}
static void keyboard_sample(bool down,int x,int y,uint64_t now) {
    portENTER_CRITICAL(&keyboard_mux);const bool accepted=keyboard_input.sample(down,x,y,now);portEXIT_CRITICAL(&keyboard_mux);
    // The touch task never waits on EPD or I2C feedback. Only an accepted key
    // signals this worker; main-loop batching must not emit a second pulse.
    if(accepted&&keyboard_haptics&&keyboard_feedback_task)xTaskNotifyGive(keyboard_feedback_task);
}
// Bounded diagnostic injects touch samples from another task while the main
// task scans the EPD. It tests concurrency, not physical FT6336U accuracy.
static std::atomic<bool> keyboard_probe_busy{false};
static char keyboard_probe_text[49]{};
static bool keyboard_probe_submit=false;
static void keyboard_probe(void *) {
    const auto epoch=keyboard_epoch.load();unsigned sent=0;
    static constexpr char letters[]="qwertyuiopasdfghjklzxcvbnm";
    for(const char *p=keyboard_probe_text;*p&&keyboard_epoch.load()==epoch;++p) {
        const int slot=int(std::strchr(letters,*p)-letters);
        const auto r=dictionary_keyboard_key(slot,false,false).rect;
        keyboard_sample(true,r.x+r.w/2,r.y+r.h/2,uint64_t(esp_timer_get_time()));
        vTaskDelay(pdMS_TO_TICKS(40));
        keyboard_sample(false,0,0,uint64_t(esp_timer_get_time()));
        ++sent;vTaskDelay(pdMS_TO_TICKS(40));
    }
    if(keyboard_probe_submit&&keyboard_epoch.load()==epoch){keyboard_sample(true,600,1134,uint64_t(esp_timer_get_time()));vTaskDelay(pdMS_TO_TICKS(40));keyboard_sample(false,0,0,uint64_t(esp_timer_get_time()));}
    ESP_LOGI(TAG,"KEYBOARD_PROBE synthetic_samples=%u epoch_unchanged=%d",sent,keyboard_epoch.load()==epoch);
    keyboard_probe_busy=false;vTaskDelete(nullptr);
}
static void touch_sample(uint8_t n, int rawx, int rawy, int x, int y) {
    static int held = 0;
    static bool down = false;
    if (n && !down)
        touch_started_us = uint64_t(esp_timer_get_time());
    down = n != 0;
    // Cover keys are outside the panel; never turn their clamped coordinates
    // into a letter. Keep the capture task running during stable input redraws.
    const bool cover=n && std::abs(rawy-TOUCH_VK_Y)<=TOUCH_VK_TOL;
    keyboard_sample(n!=0,cover?-100:x,cover?-100:y,uint64_t(esp_timer_get_time()));
    if (n && std::abs(rawy - TOUCH_VK_Y) <= TOUCH_VK_TOL) {
        int k = 0;
        if (std::abs(rawx - TOUCH_VK_HOME_X) <= TOUCH_VK_TOL)
            k = 1;
        else if (std::abs(rawx - TOUCH_VK_PREV_X) <= TOUCH_VK_TOL)
            k = 2;
        else if (std::abs(rawx - TOUCH_VK_NEXT_X) <= TOUCH_VK_TOL)
            k = 3;
        held = k;
        metalio_touch_set_ui_suppress(true);
    }
    if (!n) {
        if (held && touch_started_us.load() > input_ready_us.load())
            virtual_key = held;
        held = 0;
        metalio_touch_set_ui_suppress(false);
    }
}
extern "C" bool epd_lcd_set_gate_us(unsigned);
class App {
  public:
    void run();

  private:
    Font font_;
    Font dictionary_font_;
    uint8_t *dictionary_font_bytes_ = nullptr;
    Dictionary dictionary_;
    Card dictionary_card_;
    std::set<std::string> personal_words_;
    std::vector<Card> personal_cards_;
    Screen dictionary_return_ = Screen::DictionaryResults;
    Screen dictionary_origin_return_ = Screen::Home;
    Screen vocabulary_return_ = Screen::Home;
    Card vocabulary_origin_card_;
    Screen vocabulary_origin_return_ = Screen::Dictionary;
    int vocabulary_origin_page_ = 0;
    bool personal_deck_dirty_ = false;
    UI ui_;
    WebFrameCache web_frame_;
    View view_;
    std::vector<Card> cards_;
    LearningSystem learning_;
    GuidedLearning guided_;
    LearningWeb web_;
    bool web_qa_=false;
    std::vector<Screen> menu_stack_;
    bool menu_training_=false;
    void menu_push(Screen);bool menu_return();bool menu_dispatch(Event);void menu_finish(Event,Screen);
    std::map<std::string,int> web_positions_;
    int web_prompt_return_page_=0;
    unsigned web_fast_selections_=0;
    bool web_audio_pending_=false,web_audio_playing_=false;
    int web_audio_serial_=0;
    std::string web_audio_path_;
    std::string web_audio_note_;
    bool web_audio_slow_=false;
    void web_load();void web_view();bool web_dispatch(Event);void web_audio_tick();
    void boot_message(const std::string&);
    Assessment assessment_;
    bool assessment_valid_=true,assessment_storage_ok_=false,assessment_audio_pending_=false;
    int assessment_page_=0,assessment_skill_=0,assessment_interest_=0;
    std::string assessment_audio_path_;
    const std::string assessment_path_=std::string(ROOT)+"/assessment-state-v1.txt";
    std::set<std::string> assessment_verified_;
    void assessment_load();
    bool assessment_write();
    void assessment_view();
    void assessment_screen();
    bool assessment_dispatch(Event);
    bool assessment_result(int,const std::string&);
    int guided_browse_=0,guided_sentence_=0;
    bool guided_valid_=true,guided_audio_pending_=false;
    const std::string guided_path_=std::string(ROOT)+"/guided-state-v3.txt";
    bool recall_catalog_ready_=false,recall_audio_pending_=false;
    void recall_import();
    void recall_view();
    bool recall_dispatch(Event);
    bool recall_input() const {return view_.screen==Screen::RecallLesson&&guided_.review.typing()&&!guided_.review.session.checked;}
    bool keyboard_page() const {return view_.screen==Screen::Dictionary||recall_input()||(view_.screen==Screen::WebLearning&&view_.web.keyboard);}
    void guided_load();
    void guided_view();
    bool guided_dispatch(Event);
    ProgressMap &progress_ = learning_.state.memory[0];
    std::vector<BookSource> books_{std::begin(BOOKS), std::end(BOOKS)};
    std::set<std::string> available_audio_;
    std::set<std::string> sd_audio_;
    ResourceCache checked_audio_paths_;
    const std::string audio_cache_file_=std::string(ROOT)+"/.audio-checks-v2";
    uint64_t cache_save_at_=0;
    void flush_audio_cache();
    std::string last_spoken_id_;
    std::vector<size_t> playlist_;
    size_t player_pos_ = 0;
    bool player_running_ = false, pending_play_ = false, pending_record_ = false, player_sentences_ = false;
    uint64_t player_next_ = 0;
    Screen previous_ = Screen::Home;
    uint32_t render_generation_ = 0;
    bool keyboard_dispatch_=false;
    bool touch_dispatch_acknowledged_=false;
    uint64_t keyboard_dirty_since_=0,keyboard_redraw_at_=0;
    PanelPowerLease panel_power_;
    // Retain the old caller name so USB/shutdown and keyboard boundaries share
    // one release path. The lease now covers ordinary learning pages as well.
    void keyboard_poweroff(){if(panel_power_.held()){epd_poweroff();panel_power_.release();}}
    uint32_t keyboard_overflow_=0;
    void keyboard_tick();
    void keyboard_publish();
    uint64_t layout_us_ = 0, panel_us_ = 0, save_us_ = 0;
    Screen painted_screen_=Screen::Welcome;
    std::string painted_card_id_;
    size_t painted_position_=0;
    int painted_page_=0;
    std::vector<WordcardQualityRect> painted_dark_controls_;
    DisplayRefreshBudget refresh_budget_;
    unsigned display_mode_=0; // Text hold + separate black/white/gray control sequences.
    AudioPlayer audio_;
    TtsTestSession tts_test_;
    sdmmc_card_t *sd_card_=nullptr;
    bool usb_pending_=false;
    uint64_t usb_wait_since_=0;
    void usb_tick();
    std::map<std::string, AudioEntry> audio_index_;
    bool sd_ = false, progress_valid_ = true, rtc_ = false, dirty_ = true, full_ = true,
         nvs_ok_ = false;
    uint32_t day_ = 1, flash_bytes_ = 0;
    int deck_ = 0, updates_ = 0;
    std::string group_, clock_, notice_, storage_warning_;
    nvs_handle_t nvs_ = 0;
    esp_partition_mmap_handle_t font_mapping_ = 0;
    void power_off();
    bool up_prev_ = true, down_prev_ = true;
    std::string progress_path_ = std::string(ROOT) + "/progress-v1.csv";
    std::string state_path_ = std::string(ROOT) + "/learning-state-v2.csv";
    std::string personal_path_ = std::string(ROOT) + "/decks/custom-my-vocabulary.csv";
    void mount();
    void load_deck();
    void update_day();
    void render();
    void dispatch(Event);
    void refresh_view();
    bool save();
    void serial();
    void tts_tick();
    void tts_serial(const std::string &);
    void tts_log(const TtsTestResult &,bool save_to_sd=true);
    void catalog();
    void date_editor();
    void leave_player();
    void start_player(bool repeat, bool sentences_only = false);
    void speak();
    void playback_tick();
    void load_dictionary();
    bool open_dictionary_entry(size_t match, Screen origin);
    void refresh_vocabulary();
    bool dictionary_back();
    bool dictionary_native_dispatch(Event);
    bool dictionary_expose(const std::string &word);
    void end_lesson();
    bool sync_personal_deck();
    void refresh_speech();
    std::string sd_speech_path(const Card &) const;
    SpeechSource card_speech_source(const Card &, bool dictionary = false);
    bool validate_sd_speech(const Card &);
};
void App::mount() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sd_transaction=host.do_transaction;host.do_transaction=trace_sd_transaction;
    sd_buffer_aligned=host.check_buffer_alignment;host.check_buffer_alignment=sd_dma_buffer_usable;
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = SDMMC_CLK_PIN;
    slot.cmd = SDMMC_CMD_PIN;
    slot.d0 = SDMMC_D0_PIN;
    slot.d1 = SDMMC_D1_PIN;
    slot.d2 = SDMMC_D2_PIN;
    slot.d3 = SDMMC_D3_PIN;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t cfg{};
    cfg.format_if_mount_failed = false;
    cfg.max_files = 12;
    cfg.allocation_unit_size = 16384;
    const auto err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &cfg, &sd_card_);
    sd_ = err == ESP_OK;
    ESP_LOGI(TAG, "SD mount: %s (format disabled)", esp_err_to_name(err));
    if (!sd_) {
        notice_ = "未检测到 SD：可背词，进度仅本次有效";
        return;
    }
    for (const char *p : {"/sdcard/metalio", "/sdcard/metalio/e-ink", ROOT}) {
        if (mkdir(p, 0775) != 0 && !exists(p)) {
            notice_ = "SD 目录不可写，进度无法保存";
            view_.writable = false;
            return;
        }
    }
    std::string error;
    view_.writable = atomic_write(std::string(ROOT) + "/.write-test", "wordcard\n", error);
    std::remove((std::string(ROOT) + "/.write-test").c_str());
    load_resource_cache(audio_cache_file_,checked_audio_paths_);
    if (!view_.writable) {
        notice_ = "SD 无法写入，进度仅本次有效";
        ESP_LOGE(TAG, "%s", error.c_str());
    }
    if (exists(state_path_) || exists(state_path_ + ".bak")) {
        progress_valid_ = load_learning_state(state_path_, learning_.state, error);
        if (!progress_valid_) {
            view_.writable = false;
            notice_ = "学习记录损坏：已保护文件，可临时体验";
            ESP_LOGE(TAG, "State protected: %s", error.c_str());
        }
    } else if (exists(progress_path_) || exists(progress_path_ + ".bak")) {
        progress_valid_ = wordcard::load_progress(progress_path_, progress_, error);
        if (!progress_valid_) {
            view_.writable = false;
            notice_ = "进度文件损坏：已保护原文件，请先备份";
            ESP_LOGE(TAG, "Progress protected: %s", error.c_str());
        }
    }
    std::ifstream index(std::string(ROOT) + "/audio_index.csv");
    if (index && !load_audio_index(index, audio_index_, error)) {
        notice_ = "发音索引有误：背词仍可用";
        ESP_LOGE(TAG, "%s", error.c_str());
    }
}
void App::catalog() {
    books_.assign(std::begin(BOOKS), std::end(BOOKS));
    if (!sd_)
        return;
    std::vector<std::string> paths;
    if (exists(std::string(ROOT) + "/catalog.csv"))
        paths.push_back(std::string(ROOT) + "/catalog.csv");
    const std::string folder = std::string(ROOT) + "/catalog.d";
    if (auto *dir = opendir(folder.c_str())) {
        while (auto *entry = readdir(dir)) {
            std::string name = entry->d_name;
            if (name.size() > 4 && name.substr(name.size() - 4) == ".csv" &&
                safe_relative_path(name) && name.find('/') == std::string::npos)
                paths.push_back(folder + "/" + name);
        }
        closedir(dir);
    }
    std::sort(paths.begin(), paths.end());
    std::set<std::string> ids;
    for (const auto &base : books_) {
        std::vector<Card> cards;
        std::string error;
        std::istringstream in(base.builtin_data);
        wordcard::load_deck(in, cards, error);
        std::ifstream override_file(std::string(ROOT) + "/decks/" + base.filename);
        if (override_file)
            wordcard::load_deck(override_file, cards, error);
        for (const auto &c : cards)
            ids.insert(c.id);
    }
    for (const auto &path : paths) {
        std::ifstream file(path);
        std::vector<CatalogBook> entries;
        std::string error;
        if (!load_catalog(file, entries, error)) {
            notice_ = "部分词书清单有误，已跳过";
            continue;
        }
        for (const auto &book : entries) {
            if (books_.size() >= 64)
                break;
            if (std::any_of(books_.begin(), books_.end(),
                            [&](const auto &b) { return b.filename == book.filename; }))
                continue;
            std::ifstream csv(std::string(ROOT) + "/decks/" + book.filename);
            std::vector<Card> cards;
            if (!wordcard::load_deck(csv, cards, error) || cards.empty() || cards.size() > 20000 ||
                std::any_of(cards.begin(), cards.end(),
                            [&](const Card &c) { return ids.count(c.id) != 0; })) {
                notice_ = "部分词书无效或词条 ID 重复，已跳过";
                continue;
            }
            for (const auto &c : cards)
                ids.insert(c.id);
            books_.push_back({book.filename, book.name, book.description, nullptr, cards.size()});
        }
    }
    personal_words_.clear();
    personal_cards_.clear(); view_.personal_readable = true; view_.personal_error.clear();
    std::string personal_error;
    if (!recover_backup(personal_path_,personal_error)) {
        view_.personal_readable = false; view_.personal_error = "生词本恢复失败，原文件已保留";
        notice_ = view_.personal_error;
    }
    if (exists(personal_path_)) {
        std::vector<Card> personal; std::string error;
        std::ifstream input(personal_path_);
        if (wordcard::load_deck(input,personal,error)) {
            for (const auto &c : personal) personal_words_.insert(c.id);
            if (std::none_of(books_.begin(),books_.end(),[](const BookSource &b){return b.filename=="custom-my-vocabulary.csv";}))
                books_.push_back({"custom-my-vocabulary.csv","我的生词本","查词加入 · 按计划复习",nullptr,personal.size()});
            personal_cards_ = std::move(personal);
        } else {
            view_.personal_readable = false; view_.personal_error = "生词本读取失败，原文件已保留";
            notice_ = view_.personal_error;
        }
    }
    const std::string audio_folder = std::string(ROOT) + "/audio.d";
    if (auto *dir = opendir(audio_folder.c_str())) {
        while (auto *entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name.size() < 5 || name.substr(name.size() - 4) != ".csv" ||
                !safe_relative_path(name))
                continue;
            std::ifstream file(audio_folder + "/" + name);
            std::map<std::string, AudioEntry> extra;
            std::string error;
            if (load_audio_index(file, extra, error))
                for (const auto &a : extra)
                    if (!audio_index_.count(a.first))
                        audio_index_.insert(a);
        }
        closedir(dir);
    }
}
void App::load_dictionary() {
    web_.cancel_prepare(); // Drain private render before replacing font assets.
    std::string error;
    dictionary_.open(std::string(ROOT)+"/dictionary",error);
    font_.fallback(nullptr);
    if (dictionary_font_bytes_) { heap_caps_free(dictionary_font_bytes_); dictionary_font_bytes_ = nullptr; }
    std::ifstream in(std::string(ROOT)+"/dictionary/font-extra.fontpack",std::ios::binary|std::ios::ate);
    if (in) {
        const auto size = in.tellg();
        if (size > 0 && size <= 8*1024*1024) {
            dictionary_font_bytes_ = static_cast<uint8_t *>(heap_caps_malloc(size_t(size),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
            if (dictionary_font_bytes_) {
                in.seekg(0); in.read(reinterpret_cast<char *>(dictionary_font_bytes_),size);
                if (in && dictionary_font_.init(dictionary_font_bytes_,size_t(size))) font_.fallback(&dictionary_font_);
                else { heap_caps_free(dictionary_font_bytes_); dictionary_font_bytes_ = nullptr; }
            }
        }
    }
    web_frame_.configure(font_,dictionary_font_bytes_?&dictionary_font_:nullptr);
    ESP_LOGI(TAG,"Dictionary entries=%u, supplemental font=%s",unsigned(dictionary_.count()),dictionary_font_bytes_?"PSRAM":"unavailable");
}
void App::load_deck() {
    audio_.stop();
    personal_deck_dirty_ = false;
    player_sentences_ = false;
    deck_ = std::clamp(deck_, 0, int(books_.size()) - 1);
    const auto &book = books_[deck_];
    std::string error;
    bool from_sd = false;
    if (sd_) {
        std::ifstream file(std::string(ROOT) + "/decks/" + book.filename);
        if (file) {
            from_sd = wordcard::load_deck(file, cards_, error);
            if (!from_sd) {
                notice_ = "SD 词库有误，已使用内置词库";
                ESP_LOGE(TAG, "%s", error.c_str());
            }
        }
    }
    if (!from_sd && book.builtin_data) {
        std::istringstream data(book.builtin_data);
        error.clear();
        if (!wordcard::load_deck(data, cards_, error)) {
            ESP_LOGE(TAG, "Built-in deck failed: %s", error.c_str());
            cards_.clear();
            notice_ = "内置词库读取失败，请查看串口";
        }
    }
    if (!from_sd && !book.builtin_data) {
        cards_.clear();
        notice_ = "词书文件不可用，请重新安装资源包";
    }
    learning_.bind(&cards_);
    refresh_speech();
    view_.deck = book.name;
    view_.book_name = book.name;
    view_.selected_book = deck_;
    view_.books.clear();
    for (int i = 0; i < int(books_.size()); ++i)
        view_.books.push_back(
            {books_[i].name, books_[i].description, i == deck_ ? cards_.size() : books_[i].total});
    view_.groups = {"全部主题"};
    for (const auto &card : cards_)
        if (!card.group.empty() &&
            std::find(view_.groups.begin(), view_.groups.end(), card.group) == view_.groups.end())
            view_.groups.push_back(card.group);
    group_.clear();
    view_.page = 0;
    ESP_LOGI(TAG, "Deck=%s cards=%u source=%s", view_.deck.c_str(), unsigned(cards_.size()),
             from_sd ? "SD" : "built-in");
}
bool App::sync_personal_deck() {
    if (!personal_deck_dirty_ || books_[deck_].filename != "custom-my-vocabulary.csv") return true;
    std::string error;
    if (!reload_personal_words(personal_path_, cards_, error)) {
        notice_ = "生词已保存，词表刷新失败：" + error;
        return false;
    }
    // bind rebuilds only the ID lookup; queue, reveal phase, undo and progress stay intact.
    learning_.bind(&cards_);
    refresh_speech();
    personal_deck_dirty_ = false;
    return true;
}
#include "app_dictionary_helpers.inc"
std::string App::sd_speech_path(const Card &card) const {
    const auto it=audio_index_.find(card.id);
    if(it==audio_index_.end() || it->second.path.empty() ||
       ((card.front=="IT" || card.front=="it") && it->second.path=="audio/it.wav")) return {};
    return std::string(ROOT)+"/"+it->second.path;
}
bool App::validate_sd_speech(const Card &card) {
    const auto path=sd_speech_path(card);
    if(path.empty())return false;
    return checked_audio_paths_.get(path,
        [&](ResourceStamp &stamp){return resource_stamp(path,stamp);},[&] {
            WaveReader wave;std::string error;errno=0;
            if(wave.open(path,error))return ResourceCheck::Valid;
            return errno==EIO || errno==ENOMEM || errno==EMFILE?ResourceCheck::Retry:ResourceCheck::Invalid;
        })==ResourceCheck::Valid;
}
SpeechSource App::card_speech_source(const Card &card,bool /*dictionary*/) {
    // Only the current word needs a WAV check. Book selection and startup use
    // index candidates; playback also validates the actual file before output.
    const bool sd_valid=validate_sd_speech(card);
    return wordcard::speech_source(sd_valid,audio_.tts_ready(),card.front);
}
void App::refresh_speech() {
    sd_audio_.clear();available_audio_.clear();last_spoken_id_.clear();
    for(const auto &card:cards_) {
        const auto path=sd_speech_path(card);
        const bool sd_valid=!path.empty() && checked_audio_paths_.verified(path);
        const bool candidate=!path.empty() && checked_audio_paths_.candidate(path);
        if(sd_valid) sd_audio_.insert(card.id);
        if(wordcard::speech_source(candidate,audio_.tts_ready(),card.front)!=SpeechSource::None)
            available_audio_.insert(card.id);
    }
}
void App::update_day() {
    time_t now = std::time(nullptr);
    struct tm local{};
    localtime_r(&now, &local);
    if (rtc_ && local.tm_year >= 124 && local.tm_year <= 199) {
        day_ = uint32_t((int64_t(now) + 8 * 3600) / 86400);
        char b[48];
        std::strftime(b, sizeof(b), "时间：%Y-%m-%d", &local);
        clock_ = b;
        std::strftime(b, sizeof(b), "%m月%d日", &local);
        view_.date_label = b;
        if (nvs_ok_) {
            uint32_t last = 0;
            nvs_get_u32(nvs_, "study_day", &last);
            if (last != day_) {
                nvs_set_u32(nvs_, "study_day", day_);
                nvs_commit(nvs_);
            }
        }
    } else {
        rtc_ = false;
        // Keep undated activity in its own logical bucket; never assign it the last real date.
        day_ = 1;
        clock_ = "时间未校准 · 请在我的页面设置";
        view_.date_label.clear();
    }
}
bool App::save() {
    if (!sd_ || !progress_valid_ || (!view_.writable && storage_warning_.empty()))
        return false;
    std::string error;
    const auto start = esp_timer_get_time();
    if (!save_learning_state(state_path_, learning_.state, error) ||
        (guided_valid_ && !guided_.save(guided_path_,error))) {
        storage_warning_ = "SD 保存失败，请检查存储；暂勿关机";
        notice_ = storage_warning_;
        view_.writable = false;
        ESP_LOGE(TAG, "%s", error.c_str());
        return false;
    }
    if (!storage_warning_.empty()) { storage_warning_.clear(); notice_="存储已恢复"; }
    view_.writable = true;
    save_us_ = uint64_t(esp_timer_get_time() - start);
    // A damaged test file must never prevent saving ordinary learning progress.
    const bool assessment_saved=!assessment_valid_||!assessment_.state.attempt||assessment_write();
    flush_audio_cache();
    // Learning is already committed, but shutdown must not discard an unsaved test.
    return assessment_saved;
}
void App::flush_audio_cache() {
    cache_save_at_=uint64_t(esp_timer_get_time())+10000000;
    if(sd_ && view_.writable && checked_audio_paths_.dirty() &&
       !save_resource_cache(audio_cache_file_,checked_audio_paths_))
        ESP_LOGW(TAG,"Optional audio cache save failed; learning state is separate");
}
void App::refresh_view() {
    web_view();
    guided_view();
    recall_view();
    assessment_view();
    view_.tts_phrase=tts_test_.phrase;view_.tts_mode=tts_test_.mode;view_.tts_phase=tts_test_.phase;
    view_.tts_results=tts_test_.results[tts_test_.phrase];view_.tts_test_ready=audio_.ready();
    view_.total = cards_.size();
    view_.group = group_;
    const auto &lesson = learning_.state.lesson;
    view_.card = learning_.current();
    if (view_.screen == Screen::DictionaryEntry) view_.card = &dictionary_card_;
    if (view_.screen == Screen::GuidedLesson) view_.card = guided_.current()?&guided_.current()->card:nullptr;
    if (view_.screen == Screen::GuidedListen) view_.card = &guided_courses()[guided_browse_].items[6+guided_sentence_].card;
    if (view_.screen == Screen::RecallLesson) view_.card = guided_.review.current()?&guided_.review.current()->card:nullptr;
    if (view_.screen >= Screen::AssessmentHome) view_.card=nullptr;
    view_.revealed = lesson.revealed;
    view_.count = lesson.queue.size();
    view_.position = lesson.position;
    view_.resume_available = lesson.active();
    view_.can_undo = learning_.can_undo();
    view_.study_mode = lesson.mode;
    view_.lesson_distinct = learning_.distinct_in_lesson();
    view_.lesson_difficult = learning_.difficult_in_lesson();
    view_.haptics = learning_.state.haptics;
    view_.date_valid = rtc_;
    view_.onboarding = !learning_.state.onboarded && menu_stack_.empty();
    if (view_.screen != Screen::Plan) {
        view_.new_limit = learning_.state.new_limit;
        view_.lesson_size = learning_.state.lesson_size;
    }
    const auto plan = learning_.counts(day_, group_);
    view_.course = course_progress(cards_, progress_, group_);
    view_.player_sentences = player_sentences_;
    view_.planned_new = plan.fresh;
    view_.today_new = plan.today_new;
    view_.audio_count = available_audio_.size();
    view_.favorite_count = learning_.state.favorites.size();
    if (view_.screen == Screen::Player) {
        view_.card = playlist_.empty() ? nullptr : &cards_[playlist_[player_pos_]];
        view_.count = playlist_.size();
        view_.position = player_pos_;
    }
    view_.favorite = view_.card && learning_.state.favorites.count(view_.card->id);
    view_.player_running = player_running_;
    view_.recording = audio_.recording() || pending_record_;
    view_.clip_available = audio_.has_recording();
    const auto book_counts = study_counts(cards_, progress_, day_);
    const auto scope_counts =
        group_.empty() ? book_counts : study_counts(cards_, progress_, day_, group_);
    view_.learned = book_counts.learned;
    view_.review_due = scope_counts.due;
    if (view_.screen == Screen::Done)
        view_.review_due = learning_.counts(day_, group_, lesson.mode).due;
    view_.unseen = scope_counts.unseen;
    view_.due = scope_counts.due + scope_counts.unseen;
    view_.audio_available = false;
    view_.audio_pending = pending_play_;
    view_.audio_source = SpeechSource::None;
    view_.tts_available = audio_.tts_ready();
    if (view_.card) {
        view_.audio_source = card_speech_source(*view_.card,view_.screen==Screen::DictionaryEntry);
        if(last_spoken_id_==view_.card->id && audio_.speech_source()==SpeechSource::Synthesis)
            view_.audio_source=SpeechSource::Synthesis;
        view_.audio_available = audio_.ready() && view_.audio_source != SpeechSource::None;
    }
    view_.audio_busy = audio_.busy();
    view_.volume = audio_.volume();
    view_.brightness = Frontlight::GetInstance().target_brightness();
    view_.cct = int(Frontlight::GetInstance().cct());
    view_.storage = sd_ ? "SD 已挂载 · " + std::to_string(audio_index_.size()) + " 条发音索引"
                        : "使用内置词库 · 未挂载 SD";
    view_.clock = clock_;
    view_.notice = storage_warning_.empty() ? notice_ : storage_warning_;
    view_.dictionary_ready = view_.screen==Screen::WebLearning?view_.web.can_submit:dictionary_.ready();
    view_.dictionary_count = dictionary_.count();
    view_.personal_count = personal_words_.size();
    view_.dictionary_added = view_.dictionary_native_lookup
        ? view_.dictionary_native_added_id==dictionary_card_.id && !dictionary_card_.id.empty()
        : personal_words_.count(dictionary_card_.id) > 0;
    view_.personal_book = books_[deck_].filename == "custom-my-vocabulary.csv";
    if (view_.screen == Screen::Vocabulary) refresh_vocabulary();
    view_.resources = {"当前词书：" + books_[deck_].name,
                       "已安装 " + std::to_string(books_.size()) + " 本词书",
                       "当前词书 " + std::to_string(cards_.size()) + " 词",
                       "可用发音 " + std::to_string(available_audio_.size()) + " 词",
                       audio_.tts_ready() ? "离线合成可用 · 英语" : "离线合成未就绪 · 请检查 SD 包",
                       view_.writable ? "进度保存正常" : "进度仅本次有效"};
    if (view_.screen == Screen::Stats) {
        view_.stats.clear();
        for (int offset = 0; offset < 7; ++offset) {
            const uint32_t day = day_ >= uint32_t(offset) ? day_ - offset : 0;
            auto it = learning_.state.days.find(day);
            const auto d = it == learning_.state.days.end() ? DailyRecord{} : it->second;
            std::string label;
            if (rtc_) {
                time_t t = int64_t(day) * 86400;
                tm date{};
                gmtime_r(&t, &date);
                char b[16];
                std::strftime(b, sizeof(b), "%m/%d", &date);
                label = b;
            } else if (offset == 0)
                label = "未设日期";
            else
                break;
            view_.stats.push_back(label + "  新学 " + std::to_string(d.fresh) + "  复习 " +
                                  std::to_string(d.reviewed) + "  听音 " +
                                  std::to_string(d.listening));
        }
        view_.stats.push_back("按不同词统计，重复尝试不重复计词。");
        view_.stats.push_back("收藏 " + std::to_string(learning_.state.favorites.size()) +
                              " 词 · 播放不计掌握");
    }
    view_.diagnostics = {
        std::string("屏幕：") + (MetalioEpd_Ready() ? "驱动初始化成功" : "初始化失败"),
        std::string("触控：") + (metalio_touch_ok() ? "FT6336U 应答" : "未应答"),
        std::string("SD：") +
            (sd_ ? (view_.writable ? "已挂载，可保存" : "已挂载，未能保存") : "未挂载"),
        std::string("音频：") + (audio_.ready() ? "驱动就绪，需试听" : "初始化失败"),
        std::string("BH 串口：") + (audio_.uart_rx_seen() ? "收到响应" : "尚未收到响应"),
        std::string("RTC：") + (rtc_ ? "时间有效" : "请设置日期"),
        "Flash / PSRAM：" + std::to_string(flash_bytes_ / 1024 / 1024) + " / " +
            std::to_string(esp_psram_get_size() / 1024 / 1024) + " MB"};
}
void App::keyboard_publish() {
    bool sync=false;keyboard_haptics=learning_.state.haptics;
    portENTER_CRITICAL(&keyboard_mux);
    if(view_.screen==Screen::WebLearning&&view_.web.keyboard){
        const auto keys=web_keyboard_state(view_.web);
        sync=keyboard_input.publish(view_.web.shift,view_.web.symbols,keys.ready,keys.count,keys.recall,keys.enabled,keys.compactStudy,keys.nativeSearch);
    }else if(keyboard_page())
        sync=keyboard_input.publish(view_.dictionary_shift,view_.dictionary_symbols,recall_input()?guided_.review.ready():view_.dictionary_ready,recall_input()?0:int(view_.dictionary_recent.size()),recall_input()||(view_.screen==Screen::WebLearning&&view_.web.route=="word-batch"));
    else keyboard_input.deactivate();
    portEXIT_CRITICAL(&keyboard_mux);
    if(sync)metalio_touch_request_sample();
    keyboard_dirty_since_=keyboard_redraw_at_=0;
}
void App::keyboard_tick() {
    if(panel_power_.expired(uint64_t(esp_timer_get_time()))){keyboard_poweroff();ESP_LOGI(TAG,"DISPLAY_POWER idle_release=1");}
    Event event;
    for(size_t i=0;i<KeyboardInput::capacity;++i) {
        portENTER_CRITICAL(&keyboard_mux);const bool got=keyboard_input.pop(event);portEXIT_CRITICAL(&keyboard_mux);
        if(!got)break;
        if(view_.screen==Screen::WebLearning&&view_.web.keyboard&&view_.web.route=="word-batch"&&
           (event.action==Action::DictionaryKey||event.action==Action::DictionaryErase)){
            // Snapshot already accepted edits in order; never wait for future taps.
            // One validated save and paint for a burst, leaving mode/navigation barriers queued.
            std::string edits;edits.reserve(KeyboardInput::capacity);auto append=[&](Event e){edits+=e.action==Action::DictionaryErase?'\b':char(e.value);};append(event);
            portENTER_CRITICAL(&keyboard_mux);
            while(keyboard_input.peek(event)&&(event.action==Action::DictionaryKey||event.action==Action::DictionaryErase)){
                keyboard_input.pop(event);append(event);
            }
            portEXIT_CRITICAL(&keyboard_mux);
            const auto began=esp_timer_get_time();notice_.clear();
            if(!web_.edit(edits))notice_="输入未保存，请重试";
            ESP_LOGI(TAG,"WEB_EDIT keys=%u elapsed_us=%lld",unsigned(edits.size()),(long long)(esp_timer_get_time()-began));
            dirty_=true;keyboard_dirty_since_=keyboard_redraw_at_=uint64_t(esp_timer_get_time());
            // A queued submit/navigation closes capture. Consume it now, after
            // the saved edits, before any repaint deactivates the old keyboard.
            portENTER_CRITICAL(&keyboard_mux);const bool barrier=keyboard_input.peek(event)&&!dictionary_keyboard_edit(event.action);if(barrier)keyboard_input.pop(event);portEXIT_CRITICAL(&keyboard_mux);
            if(barrier){keyboard_dispatch_=true;dispatch(event);keyboard_dispatch_=false;}
            break;
        }
        keyboard_dispatch_=true;dispatch(event);keyboard_dispatch_=false;
        if(!dictionary_keyboard_edit(event.action))break;
    }
    portENTER_CRITICAL(&keyboard_mux);const auto dropped=keyboard_input.overflow;portEXIT_CRITICAL(&keyboard_mux);
    if(dropped!=keyboard_overflow_){keyboard_overflow_=dropped;notice_="输入过快，请核对后再查询";dirty_=true;}
}
void App::render() {
    const auto started = esp_timer_get_time();
    // Shutdown renders from resident fonts after FAT is closed; no resource probes.
    if(view_.screen!=Screen::PowerOff)refresh_view();
    if (!MetalioEpd_Ready()) {
        dirty_ = false;
        return;
    }
    Canvas canvas(epd_hl_get_framebuffer(MetalioEpd_Hl()), font_);
    const bool frame_hit=web_frame_.take(view_,epd_hl_get_framebuffer(MetalioEpd_Hl()),ui_);
    // Overlap preparation with this paint, not with the user's next tap.
    if(view_.screen==Screen::WebLearning&&view_.web.route=="quiz")web_.prepare_next();
    const bool input_patch=!frame_hit&&!full_&&ui_.render_input(canvas,view_);
    if(!frame_hit&&!input_patch)ui_.render(canvas, view_);
    layout_us_ = uint64_t(esp_timer_get_time() - started);
    if(view_.screen==Screen::WebLearning)ESP_LOGI(TAG,"WEB_FRAME hit=%d layout_us=%llu built=%lu hits=%lu cancelled=%lu",frame_hit,(unsigned long long)layout_us_,(unsigned long)web_frame_.built(),(unsigned long)web_frame_.hits(),(unsigned long)web_frame_.cancelled());
    if (!full_ && std::memcmp(epd_hl_get_framebuffer(MetalioEpd_Hl()),
                              epd_hl_get_back_framebuffer(MetalioEpd_Hl()), Canvas::bytes) == 0) {
        dirty_ = false;
        panel_us_ = 0;
        ++render_generation_;
        input_ready_us = uint64_t(esp_timer_get_time());
        keyboard_publish();
        return;
    }
    portENTER_CRITICAL(&keyboard_mux);
    const bool typing_context=keyboard_page() && painted_screen_==view_.screen &&
        !full_ && display_mode_!=2 && keyboard_input.same_layout(view_.dictionary_shift,view_.dictionary_symbols);
    portEXIT_CRITICAL(&keyboard_mux);
    const bool shutdown_art=view_.screen==Screen::PowerOff && view_.power_phase==2;
    const bool web_screen=view_.screen==Screen::WebLearning||view_.screen==Screen::WebPrompt;
    const std::string card_id=web_screen?view_.web.key+"/"+std::to_string(view_.web_page):view_.card?view_.card->id:"";
    const bool input_only=typing_context&&keyboard_fast_refresh_safe(
        epd_hl_get_framebuffer(MetalioEpd_Hl()),epd_hl_get_back_framebuffer(MetalioEpd_Hl()),Canvas::bytes,view_.screen==Screen::WebLearning);
    const auto boundary=web_screen&&display_mode_==0?study_refresh_reason(painted_card_id_,card_id,refresh_budget_.partial):StudyRefreshReason::None;
    const bool periodic_cleanup=refresh_budget_.due(input_only,uint64_t(esp_timer_get_time())/1000);
    const bool content_cleanup=boundary!=StudyRefreshReason::None;
    bool gc=shutdown_art || full_ || display_mode_==1 || display_mode_==2 || periodic_cleanup || content_cleanup;
    // A full refresh also pauses input feed. Do not acknowledge new key taps
    // while an expensive physical cleanup is hiding their visual feedback.
    const bool typing=typing_context&&!gc;
    if(!typing) {
        input_ready_us=UINT64_MAX;metalio_touch_set_feed_paused(true);
        if(typing_context){portENTER_CRITICAL(&keyboard_mux);keyboard_input.pause_capture();portEXIT_CRITICAL(&keyboard_mux);}
        else keyboard_deactivate();
    }
    const bool page_turn=painted_screen_!=view_.screen || painted_page_!=view_.page ||
        painted_card_id_!=card_id || (view_.screen==Screen::Study && painted_position_!=view_.position);
    const bool selection_fast=!gc&&web_fast_selections_<3&&!full_&&!page_turn&&!view_.web.keyboard&&view_.screen==Screen::WebLearning&&display_mode_==0&&web_binary_selection_safe(
        epd_hl_get_framebuffer(MetalioEpd_Hl()),epd_hl_get_back_framebuffer(MetalioEpd_Hl()),Canvas::bytes);
    const bool fast=refresh_budget_.allow_fast(selection_fast||(typing&&display_mode_==0&&input_only),gc);
    const auto panel_start = esp_timer_get_time();
    const bool hold_allowed=view_.screen!=Screen::PowerOff&&view_.screen!=Screen::UsbDisk;
    if(full_||!hold_allowed)keyboard_poweroff();
    // Check live PWR_GOOD before reusing rails; a stale cached TPS-ok bit is not
    // evidence that power survived an idle interval. Failure takes the old path.
    uint8_t rail_port=0;
    if(panel_power_.held()&&(!epd_board_metalio_eink4_plus_ioexp_read_port(0,&rail_port)||
                            !(rail_port&(1u<<(EPD_TPS_IO_PWR_GOOD&7)))))keyboard_poweroff();
    const bool warm=panel_power_.held();
    if(!warm)epd_poweron();
    const auto power_ready_us=uint64_t(esp_timer_get_time()-panel_start);
    if (!epd_board_metalio_eink4_plus_tps_ok()) {
        epd_poweroff();
        panel_power_.release();
        ESP_LOGE(TAG, "TPS power is not ready; refresh skipped");
        if(!typing)metalio_touch_set_feed_paused(false);
        full_ = true;
        keyboard_deactivate();
        dirty_ = false;
        input_ready_us = uint64_t(esp_timer_get_time());
        return;
    }
    // Periodic cleanup is one complete GC16 pass. Startup/manual recovery
    // retains its stronger physical clear; the waveform tables are unchanged.
    // Ink persists across resets and page transitions. Establish an actual
    // white panel before using the from-white waveform, not only a white RAM
    // back buffer. Retain the factory's clear pulse sequence unchanged.
    epd_hl_waveform(MetalioEpd_Hl(),fast || gc || display_mode_==3?&MY_WAVEFORM:wordcard_text_waveform());
    const bool physical_clear=full_ || display_mode_==2;
    wordcard_quality_clear();
    std::vector<WordcardQualityRect> dark_controls;
    for(const auto &button:ui_.buttons())if(button.dark_fill&&button.enabled) {
        const auto &r=button.rect;
        dark_controls.push_back({r.y,Canvas::width-r.x-r.w,r.h,r.w});
    }
    std::vector<WordcardQualityRect> quality_controls;
    if(!fast && !gc && display_mode_==0) {
        const auto *hl=MetalioEpd_Hl();
        auto add_changed=[&](const auto &controls){for(const auto &r:controls)
            if(wordcard_quality_changed(r,hl->front_fb,hl->back_fb,epd_width(),epd_height()))
                quality_controls.push_back(r);};
        add_changed(painted_dark_controls_);add_changed(dark_controls);
        const EpdWaveformPhases *quality=quality_controls.empty()?nullptr:wordcard_control_phases();
        if(!quality_controls.empty() && (!quality || !wordcard_quality_set(
                quality_controls.data(),unsigned(quality_controls.size()),quality->luts,
                quality->phases,epd_width(),epd_height()))) {
            ESP_LOGW(TAG,"Regional quality configuration unavailable; using GC16");
            gc=true;
            epd_hl_waveform(MetalioEpd_Hl(),&MY_WAVEFORM);
        }
    }
    if(physical_clear)epd_clear();
    auto err = fast ? epd_hl_update_screen(MetalioEpd_Hl(),MODE_DU,25)
        : physical_clear ? epd_hl_update_screen_from_white(MetalioEpd_Hl(),MODE_GC16,25)
        : gc || display_mode_==3 ? epd_hl_update_screen_full(MetalioEpd_Hl(),gc?MODE_GC16:MODE_GL16,25)
             : epd_hl_update_screen(MetalioEpd_Hl(),MODE_GL16,25);
    wordcard_quality_clear();
    // Also covers an unchanged differential, which never enters lcd_do_update.
    // MODE/OE are direct GPIOs on this board; neither stays enabled while idle.
    epd_set_mode(false);
    panel_power_.completed(uint64_t(esp_timer_get_time()),err==EPD_DRAW_SUCCESS,hold_allowed);
    if(!panel_power_.held())epd_poweroff();
    if(!typing)metalio_touch_set_feed_paused(false);
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Refresh failed: %d", int(err));
        full_ = true;
        keyboard_deactivate();
    } else {
        ++updates_;
        web_fast_selections_=selection_fast?web_fast_selections_+1:0;
        full_ = false;
        painted_screen_=view_.screen;
        painted_card_id_=card_id;painted_page_=view_.page;painted_position_=view_.position;
        painted_dark_controls_=std::move(dark_controls);
        refresh_budget_.completed(!fast&&gc,true,uint64_t(esp_timer_get_time())/1000);
    }
    dirty_ = false;
    panel_us_ = uint64_t(esp_timer_get_time() - panel_start);
    ESP_LOGI(TAG,"DISPLAY mode=%s refreshes=%u cost=%llu us err=%d screen=%d pos=%u revealed=%d turn=%d quality_controls=%u controls=%s typing=%d warm=%d cleanup=%d",selection_fast?"DU_CONTROL":fast?"DU_INPUT":physical_clear?"CLEAR+GC16":gc?"GC16":"GL16",refresh_budget_.partial,
        (unsigned long long)panel_us_,int(err),int(view_.screen),unsigned(view_.position),view_.revealed,page_turn,unsigned(quality_controls.size()),
        !gc&&display_mode_==0&&!quality_controls.empty()?"black-GL_white-hold_gray-GC":"none",typing,warm,periodic_cleanup);
    ESP_LOGI(TAG,"DISPLAY_POWER reused=%d ready_us=%llu hold=%d drive=off",warm,(unsigned long long)power_ready_us,panel_power_.held());
    if(content_cleanup)ESP_LOGI(TAG,"DISPLAY_BOUNDARY reason=%s input_only=%d",boundary==StudyRefreshReason::Summary?"summary":"question",input_only);
    ++render_generation_;
    input_ready_us = uint64_t(esp_timer_get_time());
    if(err==EPD_DRAW_SUCCESS)keyboard_publish();
    if(typing)ESP_LOGI(TAG,"INPUT_PAINT layout_us=%llu panel_us=%llu patch=%d",(unsigned long long)layout_us_,(unsigned long long)panel_us_,input_patch);
}
#include "app_tts.inc"
#include "app_usb.inc"
#include "app_power.inc"
#include "app_menu.inc"
#include "app_web.inc"
#include "app_guided.inc"
#include "app_assessment.inc"
#include "app_recall.inc"
#include "app_actions.inc"
void App::serial() {
    static std::string line;
    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == '\r')
            continue;
        if (ch != '\n') {
            if (line.size() < 120)
                line += ch;
            continue;
        }
        if(usb_pending_ && line!="status") {line.clear();continue;}
        if(line=="power status")power_key_status();
        else if(line=="web status"&&web_.ready()){
            std::string report;
            const bool ok=web_.status(report);
            ESP_LOGI(TAG,"WEB_STATUS %s",ok?report.c_str():web_.error().c_str());
            ESP_LOGI(TAG,"WEB_PERF call_us=%llu page_us=%llu builds=%lu hits=%lu heap=%u native=%d scripts=%lu",(unsigned long long)web_.last_call_us(),(unsigned long long)web_.last_page_us(),(unsigned long)web_.page_builds(),(unsigned long)web_.page_hits(),unsigned(web_.memory_bytes()),web_.native_active(),(unsigned long)web_.script_calls());
            ESP_LOGI(TAG,"WEB_AUDIO busy=%d pending=%d playing=%d source=%d",audio_.busy(),web_audio_pending_,web_audio_playing_,int(audio_.speech_source()));
            ESP_LOGI(TAG,"WEB_PROFILE isolated=%d",web_qa_);
        }
        else if(line=="web prefetch"&&web_.ready()){
            std::string report;if(web_.native_active())report="{\"engine\":\"cpp\",\"scriptPreparation\":false}";else web_.eval("({...nativePrepStats,directPages:nativeDirectStats.pages})",&report);ESP_LOGI(TAG,"WEB_PREPARE %s",report.c_str());
        }
        else if(line=="web hash-check"&&web_.ready()&&web_qa_){
            std::string strings;
            const bool strings_ok=web_.eval("(()=>{const s=String(904)+':spelling:'+WORDCARD_BATCH.catalog[0].id;return {length:s.length,first:s.charCodeAt(0),last:s.charCodeAt(s.length-1),literalLength:'904:spelling:u1a-w1'.length};})()",&strings);
            ESP_LOGI(TAG,"WEB_STRING ok=%d result=%s",strings_ok,strings_ok?strings.c_str():web_.error().c_str());
            for(const auto name:{"nativeCachedHash","nativeReferenceHash"}){
                std::string report;const auto began=esp_timer_get_time();
                const bool ok=web_.eval(std::string("WORDCARD_BATCH.")+name+"('904:spelling:u1a-w1')",&report);
                ESP_LOGI(TAG,"WEB_HASH name=%s elapsed_us=%lld ok=%d result=%s",name,(long long)(esp_timer_get_time()-began),ok,ok?report.c_str():web_.error().c_str());
            }
        }
        else if(line=="web benchmark"&&web_.ready()&&web_qa_){
            for(const int probe:{4,8,9}){
                std::string report;
                const bool ok=web_.eval("(()=>{const B=nativeDebug.M.wordBatch,probe="+std::to_string(probe)+",n=probe===9?8:probe,ids=B.catalog.slice(0,n).map(w=>w.id),began=Date.now();const s=probe===9?B.make(ids,{type:'course',id:'u1a'},999,Date.now()):B.makeReview(ids,{type:'review',id:'due'},900+n,Date.now(),ids.map(id=>({id,kind:'meaning'})),ids);if(probe===9)B.applyMode(s,'staged');return {kind:probe===9?'staged-new':'review',words:n,ms:Date.now()-began,questions:s.queue.length};})()",&report);
                ESP_LOGI(TAG,"WEB_BENCH %s",ok?report.c_str():web_.error().c_str());if(!ok)break;
            }
        }
        else if(line=="web controls"&&web_.ready()){
            for(const auto&b:ui_.buttons())if(b.enabled)ESP_LOGI(TAG,"WEB_CONTROL %d,%d,%d,%d action=%d value=%d primary=%d label=%s",b.rect.x,b.rect.y,b.rect.w,b.rect.h,int(b.event.action),b.event.value,b.primary,b.label.c_str());
        }
        else if(line=="usb")dispatch({Action::UsbDisk});
        else if(line=="kbd status") {
            portENTER_CRITICAL(&keyboard_mux);
            const auto accepted=keyboard_input.accepted,consumed=keyboard_input.consumed,overflow=keyboard_input.overflow,cancelled=keyboard_input.cancelled;
            const auto pending=keyboard_input.pending();const auto active=keyboard_input.active();
            portEXIT_CRITICAL(&keyboard_mux);
            ESP_LOGI(TAG,"KEYBOARD accepted=%lu consumed=%lu pending=%u overflow=%lu cancelled=%lu active=%d query=%s",
                (unsigned long)accepted,(unsigned long)consumed,unsigned(pending),(unsigned long)overflow,(unsigned long)cancelled,active,view_.dictionary_query.c_str());
        }
        else if(line.rfind("kbd type ",0)==0 && line.size()>9 && line.size()<=57) {
            const std::string text=line.substr(9);
            if(view_.screen==Screen::Dictionary && view_.dictionary_query.empty() && !view_.dictionary_shift && !view_.dictionary_symbols &&
                !keyboard_probe_busy && std::all_of(text.begin(),text.end(),[](char c){return c>='a'&&c<='z';})) {
                std::memcpy(keyboard_probe_text,text.c_str(),text.size()+1);keyboard_probe_submit=false;keyboard_probe_busy=true;
                if(xTaskCreate(keyboard_probe,"kbd_probe",4096,nullptr,5,nullptr)!=pdPASS){keyboard_probe_busy=false;ESP_LOGE(TAG,"KEYBOARD_PROBE task failed");}
            } else ESP_LOGW(TAG,"KEYBOARD_PROBE requires empty lowercase input and idle probe");
        }
        else if(line.rfind("kbd web ",0)==0&&web_qa_&&view_.screen==Screen::WebLearning&&view_.web.keyboard&&line.size()>8&&line.size()<=57){
            auto text=line.substr(8);const bool submit=text.back()=='!';if(submit)text.pop_back();
            if(!text.empty()&&text.size()<sizeof(keyboard_probe_text)&&!view_.web.shift&&!view_.web.symbols&&!keyboard_probe_busy&&std::all_of(text.begin(),text.end(),[](char c){return c>='a'&&c<='z';})){
                std::memcpy(keyboard_probe_text,text.c_str(),text.size()+1);keyboard_probe_submit=submit;keyboard_probe_busy=true;
                if(xTaskCreate(keyboard_probe,"kbd_probe",4096,nullptr,5,nullptr)!=pdPASS){keyboard_probe_busy=false;ESP_LOGE(TAG,"KEYBOARD_PROBE task failed");}
            }
        }
        else if(line=="display gl" || line=="display gc" || line=="display clean" || line=="display raw") {
            display_mode_=line=="display gl"?0:line=="display gc"?1:line=="display clean"?2:3;
            // Each A/B run starts from a known optical baseline; subsequent
            // pages use only the selected policy. This does not change SD data.
            full_=true;dirty_=true;
            ESP_LOGI(TAG,"DISPLAY_POLICY %u (0=GL16,1=GC16,2=clear+GC16,3=rawGL16full)",display_mode_);
        }
        else if(line=="display gate 6" || line=="display gate 10" || line=="display gate 14") {
            const unsigned gate=unsigned(std::atoi(line.c_str()+13));
            if(epd_lcd_set_gate_us(gate)){full_=true;dirty_=true;}
        }
        else if(line=="usb on")dispatch({Action::UsbStart});
        else if(line=="sdprobe 0" && !audio_.busy())sd_read_probe();
        else if(line=="sdprobe 1")audio_.probe_sd();
        else if(line=="cache status")
            ESP_LOGI(TAG,"RESOURCE_CACHE entries=%u hits=%lu metadata=%lu validations=%lu",
                unsigned(checked_audio_paths_.size()),(unsigned long)checked_audio_paths_.hits,
                (unsigned long)checked_audio_paths_.metadata_checks,(unsigned long)checked_audio_paths_.validations);
        else if(line=="lookup")dispatch({Action::Dictionary});
        else if(line.rfind("lookup ",0)==0 && line.size()<=55) {
            dispatch({Action::Dictionary});view_.dictionary_query=line.substr(7);dispatch({Action::DictionarySearch});
        }
        else if(line=="vocab")dispatch({Action::Vocabulary});
        else if(line=="speak")dispatch({Action::Speak});
        else if(line.rfind("ui tap ",0)==0) {
            int x=0,y=0;
            if(std::sscanf(line.c_str(),"ui tap %d %d",&x,&y)==2)dispatch(ui_.hit(x,y));
        }
        else if(line.rfind("tts",0)==0)tts_serial(line);
        else if (line.rfind("time ", 0) == 0) {
            int y, m, d, h, mi, s;
            if (std::sscanf(line.c_str(), "time %d-%d-%d %d:%d:%d", &y, &m, &d, &h, &mi, &s) == 6 &&
                y >= 2024 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31 && h >= 0 &&
                h < 24 && mi >= 0 && mi < 60 && s >= 0 && s < 60) {
                tm t{};
                t.tm_year = y - 1900;
                t.tm_mon = m - 1;
                t.tm_mday = d;
                t.tm_hour = h;
                t.tm_min = mi;
                t.tm_sec = s;
                auto stamp = mktime(&t);
                tm check{};
                localtime_r(&stamp, &check);
                if (check.tm_mon == m - 1 && check.tm_mday == d &&
                    Pcf8563::GetInstance().SetTime(t)) {
                    rtc_ = Pcf8563::GetInstance().ApplyRtcToSystem();
                    update_day();
                    notice_ = "日期已设置";
                    dirty_ = true;
                }
            }
        } else if (line == "status") {
            const auto &rs=guided_.review.session;const auto*rc=guided_.review.current();
            const auto rn=guided_.review.counts();
            ESP_LOGI(TAG,"RECALL_STATE screen=%d queue=%u solved=%u/%u checked=%d correct=%d heard=%d kind=%d due=%u weak=%u listen=%u id=%s typed=%s",
                int(view_.screen),unsigned(rs.queue.size()),rs.solved,rs.total,rs.checked,rs.correct,guided_.review.heard,
                rs.queue.empty()?-1:int(rs.queue.front().kind),unsigned(rn.due),unsigned(rn.mistakes),unsigned(rn.listening),
                rc?rc->card.id.c_str():"",rs.typed.c_str());
            ESP_LOGI(TAG,"GUIDED_STATE screen=%d course=%d node=%d active=%d checked=%d correct=%d solved=%d/%d errors=%d skipped=%d pending=%u",
                int(view_.screen),guided_.state.course,guided_.state.node,int(guided_.state.active),int(guided_.state.checked),int(guided_.state.correct),
                guided_.state.solved,guided_.state.total,guided_.state.errors,guided_.state.skipped,unsigned(guided_.state.pending.size()));
            ESP_LOGI(TAG,"AUDIO_STATE volume=%d ready=%d busy=%d",audio_.volume(),audio_.ready(),audio_.busy());
            refresh_view();
            refresh_vocabulary();
            ESP_LOGI(TAG,"DICT_FLOW screen=%d query=%s word=%s ipa=%s personal=%u due=%u new=%u filter=%d page=%d readable=%d active=%d position=%u writable=%d",
                int(view_.screen),view_.dictionary_query.c_str(),dictionary_card_.front.c_str(),dictionary_card_.ipa.c_str(),
                unsigned(personal_cards_.size()),unsigned(view_.vocabulary.due),unsigned(view_.vocabulary.fresh),int(view_.vocabulary_filter),view_.vocabulary.page,
                int(view_.personal_readable),int(learning_.state.lesson.active()),unsigned(learning_.state.lesson.position),int(view_.writable));
            for (auto &d : view_.diagnostics)
                ESP_LOGI(TAG, "%s", d.c_str());
            ESP_LOGI(TAG, "%s cards=%u day=%u progress=%u", clock_.c_str(), unsigned(cards_.size()),
                     unsigned(day_), unsigned(progress_.size()));
            ESP_LOGI(TAG,
                     "PERF cpu=%dMHz layout=%llu us panel=%llu us save=%llu us "
                     "audio_cache_hits=%lu heap_internal=%u heap_psram=%u",
                     CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, (unsigned long long)layout_us_,
                     (unsigned long long)panel_us_, (unsigned long long)save_us_,
                     (unsigned long)audio_.cache_hits(),
                     unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                     unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        } else if (line == "home")
            dispatch({Action::Home});
        else if (line == "back")
            dispatch({Action::Back});
        else if (line == "resume")
            dispatch({Action::Resume});
        else if (line == "undo")
            dispatch({Action::Undo});
        else if (line == "selftest")
            dispatch({Action::SelfTest});
        else if (line == "start")
            dispatch({Action::Start});
        else if (line == "reveal")
            dispatch({Action::Reveal});
        else if (line == "again")
            dispatch({Action::Rate, 0});
        else if (line == "hard")
            dispatch({Action::Rate, 1});
        else if (line == "good")
            dispatch({Action::Rate, 2});
        else if (line == "tone")
            dispatch({Action::Tone});
        else if (line == "mic")
            dispatch({Action::Mic});
        ESP_LOGI(TAG, "Command: %s", line.c_str());
        line.clear();
    }
}
void App::run() {
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_flash_get_size(nullptr, &flash_bytes_);
    auto nvs_error = nvs_flash_init();
    nvs_ok_ = nvs_error == ESP_OK && nvs_open("wordcard", NVS_READWRITE, &nvs_) == ESP_OK;
    if (!nvs_ok_)
        ESP_LOGW(TAG, "NVS unavailable: %s; preserving existing contents",
                 esp_err_to_name(nvs_error));
    epd_board_metalio_eink4_plus_boot_release_holds();
    if (!MetalioEpd_Init()) {
        epd_poweroff();
        ESP_LOGE(TAG, "EPD init failed, serial diagnostics only");
    }
    const auto part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                               esp_partition_subtype_t(0x40), "font_data");
    const void *ptr = nullptr;
    if (!part ||
        esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, &ptr, &font_mapping_) !=
            ESP_OK ||
        !font_.init(static_cast<const uint8_t *>(ptr), part->size)) {
        ESP_LOGE(TAG, "EFNT font_data missing/invalid: flash font.fontpack with firmware");
        while (true)
            vTaskDelay(pdMS_TO_TICKS(1000));
    }
    boot_message("正在读取学习资料与进度…");
    MetalioVibe_Init();
    vibe_mutex=xSemaphoreCreateMutex();
    if(xTaskCreate(keyboard_feedback,"kbd_feedback",3072,nullptr,4,&keyboard_feedback_task)!=pdPASS)keyboard_feedback_task=nullptr;
    metalio_touch_set_sample_cb(touch_sample);
    metalio_touch_init();
    Frontlight::GetInstance().Init(FL_COOL_PIN, FL_WARM_PIN);
    Frontlight::GetInstance().RestoreBrightness();
    Frontlight::GetInstance().SetCct(FrontlightCct::kOff);
    power_i2c_t bus{I2C_NUM_1, metalio_i2c1_lock, metalio_i2c1_unlock};
    auto &rtc = Pcf8563::GetInstance();
    rtc_ = rtc.Begin(&bus) && rtc.ApplyRtcToSystem();
    update_day();
    mount();
    catalog();
    load_dictionary();
    if (nvs_ok_) {
        char saved_book[96]{};
        size_t size = sizeof(saved_book);
        if (nvs_get_str(nvs_, "book", saved_book, &size) == ESP_OK)
            for (int i = 0; i < int(books_.size()); ++i)
                if (std::strcmp(saved_book, books_[i].filename.c_str()) == 0)
                    deck_ = i;
    }
    load_deck();
    learning_.pause();
    guided_load();
    assessment_load();
    view_.screen = Screen::Home;
    web_load();
    audio_.init(sd_);
    refresh_speech();
    uint8_t volume = 50;
    if (nvs_ok_)
        nvs_get_u8(nvs_, "volume", &volume);
    audio_.volume(volume);
    gpio_config_t boot{};
    boot.pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO;
    boot.mode = GPIO_MODE_INPUT;
    boot.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&boot);
    // Board wiring defines idle HIGH; sampling while powering on is not polarity.
    up_prev_ = epd_board_metalio_eink4_plus_ioexp_get_level(EPD_IO_VOL_UP);
    down_prev_ = epd_board_metalio_eink4_plus_ioexp_get_level(EPD_IO_VOL_DOWN);
    ESP_LOGI(TAG, "POWER_INPUT active-low; startup hold ignored until release");
    power_key_start();
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    ESP_LOGI(TAG, "小小单词卡 0.3.5 USB + TTS ready. Commands: usb, usb on, tts, tts small/espeak 1..6, tts stop/status, "
                  "status, time YYYY-MM-DD HH:MM:SS, selftest, tone, mic, start, reveal, again/hard/good, home");
    bool pressed = false, boot_prev = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
    Event pressed_event{};
    bool pressed_acknowledged=false;
    uint32_t pressed_generation = 0;
    metalio_touch_point_t last{};
    for (;;) {
        if(power_key_requested() && !usb_pending_)power_off();
        // EPD's high-priority dual-core line feeders can starve audio refill. Finish/stop
        // playback first; DMA audio and touch processing remain independently scheduled.
        keyboard_tick();
        const uint64_t loop_now=uint64_t(esp_timer_get_time());
        if (dirty_ && !audio_.busy() && (!keyboard_dirty_since_ || loop_now>=keyboard_redraw_at_ || loop_now>=keyboard_dirty_since_+300000)) {
            render();
            if(!dirty_&&view_.screen==Screen::WebLearning&&view_.web.route!="quiz")web_.prepare_next();
        }
        // Each edit still collects, bounded to this iteration. Save/validation
        // already succeeded before painting; even skipped/failed paints collect.
        if(web_.ready()&&web_.input_pending()){web_.finish_input();ESP_LOGI(TAG,"WEB_INPUT_GC elapsed_us=%llu",(unsigned long long)web_.last_gc_us());}
        metalio_touch_point_t point{};
        bool down = metalio_touch_read(&point);
        if(keyboard_page()){pressed=false;pressed_event={};pressed_acknowledged=false;}
        else if (down) {
            last = point;
            if (!pressed && touch_started_us.load() > input_ready_us.load()) {
                pressed_event = ui_.hit(point.x, point.y);
                pressed_generation = render_generation_;
                pressed_acknowledged=false;
                if(pressed_event.action!=Action::None&&learning_.state.haptics&&keyboard_feedback_task){
                    button_feedback_requested_us=uint64_t(esp_timer_get_time());
                    xTaskNotifyGive(keyboard_feedback_task);pressed_acknowledged=true;
                    ESP_LOGI(TAG,"TOUCH_DOWN accepted action=%d down_to_queue_us=%llu",int(pressed_event.action),
                        (unsigned long long)(esp_timer_get_time()-touch_started_us.load()));
                }
            }
        }
        if (pressed && !down) {
            ESP_LOGI(TAG, "Touch %d,%d raw=%d,%d", last.x, last.y, last.raw_x, last.raw_y);
            const Event released = ui_.hit(last.x, last.y);
            if (pressed_generation == render_generation_ &&
                released.action == pressed_event.action && released.value == pressed_event.value){
                touch_dispatch_acknowledged_=pressed_acknowledged;
                dispatch(pressed_event);
                touch_dispatch_acknowledged_=false;
            }
            pressed_event = {};
            pressed_acknowledged=false;
        }
        pressed = keyboard_page()?false:down;
        int vk = virtual_key.exchange(0);
        if (vk == 1)
            dispatch({Action::Home});
        if (vk == 2)
            dispatch({Action::Back});
        if (vk == 3 && view_.screen == Screen::Study)
            dispatch({Action::Reveal});
        bool boot_down = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
        if (boot_down && !boot_prev)
            dispatch({Action::SelfTest});
        boot_prev = boot_down;
        uint8_t side_keys = 0xff;
        const bool keys_valid = epd_board_metalio_eink4_plus_ioexp_read_port(1, &side_keys);
        const bool up = (side_keys & (1u << (EPD_IO_VOL_UP & 7))) != 0,
                   dn = (side_keys & (1u << (EPD_IO_VOL_DOWN & 7))) != 0;
        if (keys_valid && !up && up != up_prev_) {
            dispatch({Action::Volume, 10});
        }
        if (keys_valid && !dn && dn != down_prev_) {
            dispatch({Action::Volume, -10});
        }
        if (keys_valid) { up_prev_ = up; down_prev_ = dn; }
        std::string result,result_path;
        int result_kind = 0;uint32_t result_token=0;
        while (audio_.result(result, result_kind,result_path,&result_token)) {
            if(result_token){
                if(audio_result_matches(result_token,uint32_t(web_audio_serial_),web_audio_playing_,result_kind)){
                    web_audio_playing_=false;web_.playback(web_audio_serial_,result_kind==1);notice_=result_kind==1?"":result;dirty_=true;
                }
                continue;
            }
            if(assessment_result(result_kind,result_path))continue;
            view_.audio_status = result;
            if(view_.screen==Screen::GuidedLesson && guided_audio_pending_ && (result_kind==1 || result_kind==2 || result_kind==3)){
                guided_.played(result_kind==1);guided_audio_pending_=false;dirty_=true;
                notice_=result;
            }
            if(view_.screen==Screen::RecallLesson && recall_audio_pending_ && (result_kind==1 || result_kind==2 || result_kind==3)){
                guided_.review.heard=result_kind==1;recall_audio_pending_=false;dirty_=true;notice_=result;
            }
            if (view_.screen == Screen::DictionaryEntry && result_kind == 1) dirty_=true;
            if (view_.screen == Screen::GuidedListen && (result_kind == 1 || result_kind == 3)) dirty_=true;
            if (result_kind == 2) {
                if(!result_path.empty())checked_audio_paths_.invalidate(result_path);
                refresh_speech();
                notice_ = result;
                player_running_ = false;
                pending_play_ = false;
                if (view_.screen == Screen::Study &&
                    learning_.state.lesson.mode == StudyMode::Listening &&
                    !learning_.state.lesson.revealed) {
                    learning_.pause();
                    save();
                    view_.screen = Screen::Pause;
                    notice_ = "发音播放失败，请检查资源；本题尚未计分";
                }
                dirty_ = true;
            } else if (result_kind == 4) {
                notice_ = result;
                dirty_ = true;
            } else if (view_.screen == Screen::SelfTest ||
                       (view_.screen == Screen::Player && view_.repeat_mode)) {
                notice_ = result;
                dirty_ = true;
            }
            if (result_kind == 1 && view_.screen == Screen::Player && player_running_ &&
                !pending_play_)
                player_next_ = uint64_t(esp_timer_get_time()) + 3000000;
        }
        tts_tick();
        playback_tick();
        serial();
        web_audio_tick();
        usb_tick();
        if(!audio_.busy() && !dirty_ && uint64_t(esp_timer_get_time())>=cache_save_at_)flush_audio_cache();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
extern "C" void app_main() {
    usb_serial_jtag_driver_config_t console=USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    console.rx_buffer_size=1024;console.tx_buffer_size=2048;
    if(usb_serial_jtag_driver_install(&console)==ESP_OK) {
        usb_serial_jtag_vfs_use_driver();
        usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    }
    ESP_LOGI(TAG,"BOOT_RESET reason=%d revision=fix55h-recovery",int(esp_reset_reason()));
    if(usb_disk_boot_requested())usb_disk_run();
    static App app;
    app.run();
}
