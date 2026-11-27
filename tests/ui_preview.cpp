#include "wordcard_ui.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace wordcard;
int main(int argc, char **argv) {
    if (argc != 3)
        return 1;
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    Font font;
    if (!font.init(data.data(), data.size()))
        return 2;
    std::vector<uint8_t> fb(Canvas::bytes);
    Canvas c(fb.data(), font);
    // Physical 4-bpp layout must preserve all gray levels and the neighboring
    // nibble; glyph edge coverage must survive both black and reversed text.
    c.clear();
    for(int level=0;level<16;++level) {
        c.gray(20,level,uint8_t(level));
        if(c.shade(20,level)!=level || c.shade(21,level)!=15)return 90;
    }
    for(bool ink:{true,false}) {
        std::fill(fb.begin(),fb.end(),ink?255:0);
        c.text(40,80,"Ag汉",1,ink);
        bool edge=false,solid=false;
        for(int y=0;y<200;++y)for(int x=0;x<200;++x) {
            const auto gray=c.shade(x,y);
            edge|=gray>0 && gray<15;
            solid|=gray==(ink?0:15);
        }
        if(!edge || !solid)return 91;
    }
    std::cout << "PASS: native 4-bpp grayscale and normal/reversed font antialiasing\n";
    UI ui;
    Card sample_card{
        "KET-1", "beautiful",      "adj. 美丽的；美好的。\n例句：What a beautiful day!", "", "",
        "",      "US /ˈbjuːtəfəl/"};
    View v;
    v.total = 1997;
    v.learned = 124;
    v.review_due = 24;
    v.unseen = 1873;
    v.due = 1897;
    v.planned_new = 10;
    v.book_name = "KET 官方词汇";
    v.date_label = "10月21日";
    v.books = {{"KET 官方词汇", "考试英语 · 2025 版", 1997},
               {"PET 核心词汇", "考试英语 · 2024 版", 1871}};
    v.writable = true;
    v.storage = "SD 已挂载 · 发音索引已载入";
    v.clock = "时间：2026-10-21";
    auto capture = [&](const std::string &name, const View &view) {
        ui.render(c, view);
        for (const auto &b : ui.buttons()) {
            if (b.rect.x < 8 || b.rect.y < 6 || b.rect.x + b.rect.w > 676 ||
                b.rect.y + b.rect.h > 1206)
                return false;
            const auto event = ui.hit(b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2);
            if (b.enabled && (event.action != b.event.action || event.value != b.event.value))
                return false;
        }
        for (size_t i = 0; i < ui.buttons().size(); ++i)
            for (size_t j = i + 1; j < ui.buttons().size(); ++j) {
                const auto a = ui.buttons()[i].rect, b = ui.buttons()[j].rect;
                if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h)
                    return false;
            }
        std::ofstream image(std::string(argv[2]) + "/" + name + ".pgm", std::ios::binary);
        image << "P5\n684 1216\n255\n";
        for (int y = 0; y < Canvas::height; ++y)
            for (int x = 0; x < Canvas::width; ++x)
                image.put(char(c.shade(x,y)*17));
        return bool(image);
    };
    for (int screen = 0; screen < 6; ++screen) {
        if (screen == 0)
            v.screen = Screen::Home;
        if (screen == 1) {
            v.screen = Screen::Groups;
            v.groups = {"全部主题", "动物", "食物和饮料", "家庭与朋友", "学校与学习", "旅行与交通"};
        }
        if (screen == 2 || screen == 3) {
            v.screen = Screen::Study;
            v.card = &sample_card;
            v.count = 21;
            v.position = 4;
            v.audio_available = true;
            v.revealed = screen == 3;
        }
        if (screen == 4) {
            v.screen = Screen::SelfTest;
            v.diagnostics = {"屏幕：初始化成功",   "触控：FT6336U 应答", "SD：已挂载 · 未格式化",
                             "音频：等待实机验证", "RTC：时间有效",      "触点：x=324  y=760"};
        }
        if (screen == 5)
            v.screen = Screen::Settings;
        if (!capture("preview-" + std::to_string(screen), v))
            return 3;
    }
    v.screen = Screen::Home;
    v.learned = v.review_due = 0;
    v.unseen = 1997;
    if (!capture("home-fresh", v))
        return 8;
    v.unseen = 0;
    v.planned_new = 0;
    v.learned = v.total;
    if (!capture("home-complete", v))
        return 9;
    bool has_library_action = false;
    for (const auto &b : ui.buttons())
        if (b.primary && b.event.action == Action::Practice)
            has_library_action = true;
    if (!has_library_action)
        return 10;
    v.unseen = 1873;
    v.planned_new = 10;
    v.learned = 124;
    v.review_due = 24;
    v.writable = false;
    v.notice = "未检测到 SD：可背词，进度仅本次有效";
    if (!capture("home-no-sd", v))
        return 11;
    v.notice = "进度文件损坏：已保护原文件，请先备份";
    if (!capture("home-error", v))
        return 12;
    v.notice.clear();
    v.writable = true;
    v.book_name = "商务英语：跨文化沟通与职场协作实用词汇";
    v.group = "日常会议与跨文化团队协作";
    v.total = 4800;
    v.learned = 3021;
    v.review_due = 18;
    v.unseen = 1779;
    if (!capture("home-general-english", v))
        return 13;
    v.screen = Screen::Library;
    v.page = 0;
    if (!capture("library-current", v))
        return 14;
    v.books = {{"日常英语", "生活与交流", 1500},
               {"职场沟通", "会议与邮件", 2400},
               {"旅行英语", "出行与文化", 800},
               {"学术英语", "阅读与表达", 4800},
               {"专业术语示例", "仅为布局扩展示例，非已提供词库", 10000}};
    if (!capture("library-extension-demo", v))
        return 15;
    v.page = 1;
    if (!capture("library-page-two-demo", v))
        return 16;
    bool last_book = false;
    for (const auto &b : ui.buttons())
        if (b.event.action == Action::Deck && b.event.value == 4)
            last_book = true;
    if (!last_book)
        return 17;
    v.books.clear();
    v.page = 0;
    if (!capture("library-empty", v))
        return 18;
    v = View{};
    v.writable = true;
    v.card = &sample_card;
    v.book_name = "KET 官方词汇";
    v.total = 1997;
    v.learned = 124;
    v.count = 10;
    v.position = 3;
    v.audio_available = true;
    v.can_undo = true;
    v.planned_new = 5;
    v.lesson_distinct = 10;
    v.lesson_difficult = 3;
    v.review_due = 8;
    v.notice = "";
    v.onboarding = true;
    for (auto item :
         std::vector<std::pair<Screen, std::string>>{{Screen::Welcome, "welcome"},
                                                     {Screen::Plan, "plan"},
                                                     {Screen::Date, "date"},
                                                     {Screen::Practice, "practice"},
                                                     {Screen::Profile, "profile"},
                                                     {Screen::Pause, "pause"},
                                                     {Screen::Done, "done"},
                                                     {Screen::Player, "player"},
                                                     {Screen::Stats, "stats"},
                                                     {Screen::Resources, "resources"}}) {
        v.screen = item.first;
        v.stats = {"10月24日  新词 5 · 复习 18 · 听力 6", "10月23日  新词 8 · 复习 12 · 听力 5",
                   "记录按不同词计数；播放不计复习"};
        v.resources = {"当前词书  KET 官方词汇", "已安装词书  2 本", "词条 1997 · 发音 1980",
                       "SD 可写 · 进度自动保存"};
        if (!capture("system-" + item.second, v))
            return 20;
    }
    v.screen = Screen::Player;
    v.repeat_mode = true;
    if (!capture("system-repeat", v))
        return 21;
    v.recording = true;
    if (!capture("system-recording", v))
        return 22;
    v.recording = false;
    v.clip_available = true;
    if (!capture("system-recorded", v))
        return 23;
    v.card = nullptr;
    if (!capture("system-player-empty", v))
        return 24;
    v.card = &sample_card;
    v.screen = Screen::Study;
    v.study_mode = StudyMode::Listening;
    v.revealed = false;
    if (!capture("system-listening-question", v))
        return 25;
    const auto hidden = fb;
    Card secret = sample_card;
    secret.front = "something entirely different";
    secret.back = "secret answer";
    secret.ipa = "US /sikrət/";
    v.card = &secret;
    ui.render(c, v);
    if (fb != hidden)
        return 26; // Neither word nor meaning may leak before reveal.
    v.card = &sample_card;
    v.revealed = true;
    if (!capture("system-listening-answer", v))
        return 27;
    v.study_mode = StudyMode::Reverse;
    v.revealed = false;
    if (!capture("system-reverse", v))
        return 28;
    v.study_mode = StudyMode::Visual;
    v.revealed = true;
    if (!capture("system-study-answer", v))
        return 29;
    v.screen = Screen::Home;
    v.resume_available = true;
    if (!capture("system-home-resume", v))
        return 30;
    v.screen = Screen::Done;
    v.study_mode = StudyMode::Listening;
    v.writable = false;
    v.notice = "未能保存进度，当前记录仅本次有效";
    if (!capture("system-done-no-storage", v))
        return 32;
    v.screen = Screen::Home;
    v.notice = "计划已应用，仅本次有效，无法保存";
    if (!capture("system-plan-no-storage", v))
        return 33;
    v.notice = "日期已设置；学习记录仅本次有效";
    if (!capture("system-date-no-storage", v))
        return 34;
    v.writable = true;
    v.screen = Screen::Pause;
    v.notice = "发音不可用，请关机补齐资源后重启，或结束这节";
    if (!capture("system-resume-audio-missing", v))
        return 35;
    v.screen=Screen::TtsTest;v.notice="";v.tts_preview=true;v.tts_test_ready=true;
    v.audio_busy=false;v.tts_phrase=0;v.tts_phase=TtsTestPhase::Idle;
    if(!capture("tts-01-idle-daily",v))return 40;
    for(const auto &phrase:tts_phrases) {
        auto lines=c.wrap(phrase.text,604);if(lines.size()>3)return 41;
        for(const auto &line:lines)if(c.text_width(line)>604)return 42;
    }
    v.tts_phrase=4;v.tts_mode=TtsTestMode::Espeak;
    if(!capture("tts-02-select-tech",v))return 43;
    v.tts_phase=TtsTestPhase::Generating;v.audio_busy=true;
    if(!capture("tts-03-generating",v))return 44;
    for(const auto &b:ui.buttons())if(b.enabled && b.event.action!=Action::Back && b.event.action!=Action::TtsStop)return 45;
    for(int i=0;i<tts_mode_count;++i) {
        auto &r=v.tts_results[i];r.available=r.generated=r.played=r.memory_valid=true;
        r.actual=i==0?TtsActual::AmySmall:TtsActual::Espeak;r.requested=TtsTestMode(i);
        r.compute_us=(i+1)*123400;r.prepare_us=123000;r.samples=64000;r.warm=true;
        r.internal_before=200000;r.internal_min=180000;r.psram_before=30000000;r.psram_min=29000000;
    }
    v.tts_mode=TtsTestMode::AmySmall;v.tts_phase=TtsTestPhase::Ready;v.audio_busy=false;
    if(!capture("tts-04-ready-listen",v))return 46;
    v.tts_phase=TtsTestPhase::Done;
    if(!capture("tts-05-comparison",v))return 47;
    v.tts_mode=TtsTestMode::Espeak;
    if(!capture("tts-05c-espeak",v))return 47;
    {int modes=0;for(const auto &b:ui.buttons()){
        if(b.event.action==Action::TtsRun && b.event.value!=0)return 47;
        if(b.enabled && b.event.action==Action::TtsMode)++modes;
    }if(modes!=2)return 47;}
    v.tts_mode=TtsTestMode::AmySmall;
    v.tts_results[0].cached=true;v.tts_results[0].persisted=true;v.tts_results[0].memory_valid=false;
    if(!capture("tts-05b-cached-listen",v))return 47;
    {bool listen=false,retest=false;for(const auto &b:ui.buttons()){
        listen|=b.enabled && b.event.action==Action::TtsListen;
        retest|=b.enabled && b.event.action==Action::TtsRun && b.event.value==0;
    }if(!listen || !retest)return 47;}
    v.tts_results[0].cached=false;
    v.tts_results[0].actual=TtsActual::Espeak;
    if(!capture("tts-06-fallback",v))return 48;
    v.tts_results[0].generated=false;v.tts_results[0].actual=TtsActual::None;
    std::snprintf(v.tts_results[0].error,sizeof(v.tts_results[0].error),"Amy Small 模型缺失或损坏，请补齐 SD 资源");
    v.tts_phase=TtsTestPhase::Failed;
    if(!capture("tts-07-missing-model",v))return 49;
    v.tts_phase=TtsTestPhase::Cancelled;
    if(!capture("tts-08-stopped",v))return 50;
    v.tts_phase=TtsTestPhase::Idle;v.tts_results={};v.tts_test_ready=false;
    if(!capture("tts-09-audio-unavailable",v))return 51;
    std::cout<<"PASS: 12 TTS test states; two engines, cached replay and explicit retest; 6 complete phrases; busy-state controls and all hit rectangles verified\n";
    v.screen=Screen::Resources;v.notice="";
    if(!capture("usb-00-entry",v))return 52;
    v.screen=Screen::UsbDisk;
    for(int phase=0;phase<4;++phase) {
        v.usb_phase=phase;v.usb_error="U 盘缓冲区：ESP_ERR_NO_MEM";
        if(!capture("usb-0"+std::to_string(phase+1),v))return 53;
        if(phase==1)
            for(const auto &b:ui.buttons())if(b.enabled)return 54;
        if(phase==2)
            for(const auto &b:ui.buttons())if(b.enabled && b.event.action!=Action::UsbExit)return 54;
    }
    std::cout<<"PASS: USB entry and 4 states; explicit return only, preparing navigation disabled\n";
    v.screen=Screen::PowerOff;
    for(int phase=0;phase<3;++phase){
        v.power_phase=phase;
        if(!capture("power-"+std::to_string(phase),v) || !ui.buttons().empty())return 55;
    }
    std::cout<<"PASS: shutdown release/save/cut states have no active learning controls\n";
    int longest_lines = 0, checked = 0;
    for (const char *name : {"KET_2025_OFFICIAL.csv", "PET_2024.csv"}) {
        std::ifstream input(std::string(argv[2]) + "/" + name);
        std::vector<Card> cards;
        std::string error;
        if (!load_deck(input, cards, error))
            return 5;
        for (const auto &card : cards) {
            if(card.ipa.empty()) return 36;
            const auto answer=c.wrap(card.ipa+"\n"+card.back,616);
            for(const auto &line:answer) if(c.text_width(line)>616) return 37;
            if(card.id=="ket-official-0605" || card.id=="ket-official-0293") {
                v.screen=Screen::Study; v.card=&card; v.revealed=true;
                v.study_mode=StudyMode::Visual; v.notice=""; v.answer_page=0;
                if(!capture("pronunciation-"+card.id,v)) return 38;
            }
            int scale = c.text_width(card.front, 3) <= 620 ? 3 : 2;
            auto lines = c.wrap(card.front, 620, scale);
            if (lines.size() > 2) {
                scale = 1;
                lines = c.wrap(card.front, 620, scale);
            }
            if (230 + int(lines.size()) * scale * 44 + 30 + 88 > 550)
                return 6;
            longest_lines = std::max(longest_lines, int(c.wrap(card.back, 616).size()));
            if (c.wrap(card.group, 612).size() > 2)
                return 7;
            auto player_lines = c.wrap(card.front, 604, 2);
            if (player_lines.size() > 2)
                player_lines = c.wrap(card.front, 604, 1);
            if (player_lines.size() > 4)
                return 31;
            ++checked;
        }
    }
    std::cout << "PASS: all 16 UI screens; all " << checked
              << " cards fit word area; longest definition " << longest_lines << " lines (paged)\n";
    std::cout << "PASS: Home/Library states, generic English books, pagination, non-overlapping "
                 "hit regions; hidden listening answers; recording/resume states\n";
}
