#include "wordcard_ui.hpp"
#include <algorithm>
namespace wordcard {
void UI::learning_pages(Canvas &c, const View &v) {
    c.text(40, 32, c.ellipsize(v.screen == Screen::Date ? "日期与时间"
                    : v.screen == Screen::Player ? v.book_name + " · " + v.group : "小小单词卡",604));
    c.fill({40, 98, 604, 2});
    if (v.screen != Screen::Welcome)
        button(c, {40, 120, 140, 88}, Action::Back, "返回");
    auto title = [&](const std::string &s) { c.text(40, 230, s, 2); };
    auto line = [&](int y, const std::string &s) { c.text(40, y, c.ellipsize(s, 604)); };
    auto entry = [&](int y, Action a, const std::string &s, int value = 0) {
        row(c, {40, y, 604, 96}, a, s, value);
        c.fill({40, y + 96, 604, 1});
    };
    const auto status = c.wrap(v.notice, 604);
    for (size_t i = 0; i < std::min<size_t>(2, status.size()); ++i)
        c.text(40, 1030 + int(i) * 38, status[i]);
    if (v.screen == Screen::Welcome) {
        title("每天学一点");
        line(370, "每次一小节，学完就可以休息。");
        line(470, "先回忆，再揭示，按真实感受评分。");
        line(570, "听音练习和词书都可以随时选择。");
        line(690, v.writable ? "进度会保存在 SD 卡上。" : "未能保存进度，当前可体验学习。");
        button(c, {40, 850, 604, 112}, Action::Plan, "设置我的计划", 0, true);
    } else if (v.screen == Screen::Plan) {
        title(v.more_learning_scope?"词书速记计划":"学习计划");
        line(350, "每日新词上限  " + std::to_string(v.new_limit) + " 词");
        button(c, {40, 410, 280, 96}, Action::NewLimit, "减少 5", -5, false, v.new_limit > 0);
        button(c, {364, 410, 280, 96}, Action::NewLimit, "增加 5", 5, false, v.new_limit < 50);
        line(550, "每节起始题数  " + std::to_string(v.lesson_size) + " 题");
        button(c, {40, 610, 280, 96}, Action::LessonSize, "减少", -1, false, v.lesson_size > 5);
        button(c, {364, 610, 280, 96}, Action::LessonSize, "增加", 1, false, v.lesson_size < 12);
        line(750, "到期词优先。不会的词会稍后再见。");
        line(810, "新词设为 0 时，只安排复习。");
        button(c, {40, 900, 604, 112}, Action::SavePlan,
               v.onboarding ? "下一步：设置日期" : "保存计划", 0, true);
    } else if (v.screen == Screen::Date) {
        const char *labels[] = {"年", "月", "日", "时", "分"};
        for (int i = 0; i < 5; ++i)
            button(c, {40, 250 + i * 108, 604, 88}, Action::DateField,
                   std::string(labels[i]) + "  " + std::to_string(v.date_fields[i]), i,
                   i == v.date_field);
        button(c, {40, 810, 280, 88}, Action::DateStep, "减一", -1);
        button(c, {364, 810, 280, 88}, Action::DateStep, "加一", 1);
        button(c, {40, 926, 604, 96}, Action::SaveDate, "保存日期和时间", 0, true);
        if (v.onboarding)
            button(c, {216, 120, 428, 88}, Action::SkipDate, "稍后设置，先体验");
    } else if (v.screen == Screen::Practice) {
        title("专项练习");
        line(305,v.group.empty() ? v.book_name : v.group);
        auto practice_entry = [&](int index,Action action,const std::string &label,int value=0) {
            const int y=360+index*92;
            row(c,{40,y,604,88},action,label,value);c.fill({40,y+88,604,1});
        };
        if (v.course.sentences) practice_entry(0, Action::ListenCourse, "听本课句子");
        else practice_entry(0,Action::Groups,"选择课程 / 主题");
        practice_entry(1, Action::Start, "听音回忆", 1);
        practice_entry(2, Action::Start, "中英回忆", 2);
        practice_entry(3, Action::Player, "词句随身听");
        practice_entry(4, Action::Repeat, "跟读对照");
        practice_entry(5, Action::Difficult, "难词巩固");
        practice_entry(6, Action::Favorites, "我的收藏 · " + std::to_string(v.favorite_count));
        tabs(c, Action::Practice);
    } else if (v.screen == Screen::Profile) {
        title("我的学习");
        entry(340, Action::Stats, v.guided_enabled?"词书速记记录":"学习记录");
        entry(450, Action::Plan, v.guided_enabled?"词书速记计划":"学习计划");
        entry(560, Action::Date, v.date_valid ? "日期与时间" : "日期未设 · 点击设置");
        entry(670, Action::Settings, "声音与前光");
        entry(780, Action::Resources, "词书与资源管理");
        entry(890, Action::Haptics, v.haptics ? "触控反馈：开启" : "触控反馈：关闭");
        tabs(c, Action::Profile);
    } else if (v.screen == Screen::Pause) {
        const bool switching=v.vocabulary_review_pending;
        title(switching?"还有未完成的小节":"休息一下");
        line(380, "已经完成 " + std::to_string(v.position) + " 次回忆。");
        line(450, switching?"可以继续，也可以改学生词。":"当前题目和位置会一起保留。");
        line(530, !v.writable?"当前无法保存，关机后不能续学。":switching?"结束旧小节会保留已学进度。":"下次打开，从这里继续。");
        button(c, {40, 650, 604, 108}, Action::Resume, switching?"继续原来的小节":"继续这一节", 0, true);
        button(c, {40, 790, 604, 96}, (v.personal_book||switching)?Action::Vocabulary:Action::Home, switching?"返回生词本":v.personal_book?"回生词本，稍后继续":"稍后继续");
        button(c, {40, 916, 604, 96}, Action::EndLesson, switching?"结束旧小节，开始学生词":"结束这节，保留已学进度");
    } else if (v.screen == Screen::Done) {
        const bool course_done = v.course.total && v.course.remaining == 0 && v.study_mode == StudyMode::Visual;
        title(course_done ? "整课首轮完成" : "小节完成");
        line(350,"本小节练习了 " + std::to_string(v.lesson_distinct) + " 项");
        if (v.course.total) {
            line(420,"本课已学 " + std::to_string(v.course.learned) + " / " + std::to_string(v.course.total) + " 项");
            line(480,course_done ? "已学完一轮，之后仍会安排复习。"
                    : "本课还有 " + std::to_string(v.course.remaining) + " 项未学");
        } else line(420,std::to_string(v.lesson_difficult) + " 项需要继续巩固");
        line(550,"到期复习 " + std::to_string(v.review_due) + " 项 · 新词余量 " + std::to_string(v.planned_new));
        if (v.course.remaining && v.study_mode == StudyMode::Visual)
            line(610,"继续本课可额外学新项，不改每日计划。");
        else if (!v.writable) line(610,"未能保存记录，仅本次有效。");
        if (v.course.sentences)
            button(c,{40,670,604,88},Action::ListenCourse,"听本课句子");
        if (v.course.remaining && v.study_mode == StudyMode::Visual)
            button(c,{40,780,604,100},Action::ContinueCourse,"继续本课未学内容",0,true);
        else if (course_done)
            button(c,{40,780,604,100},Action::Groups,"选择下一课",0,true);
        else
            button(c,{40,780,604,100},Action::Start,"再来一节",int(v.study_mode),true);
        button(c, {40, 900, 280, 96}, v.personal_book?Action::Vocabulary:Action::Home, v.personal_book?"回生词本":"休息一下");
        button(c, {364, 900, 280, 96}, Action::Undo, "撤销上一题", 0, false, v.can_undo);
    } else if (v.screen == Screen::Player) {
        if (!v.card) {
            title("暂无可用发音");
            line(390, "这本词书缺少可用音频。");
            button(c, {40, 760, 604, 108}, Action::Resources, "查看资源", 0, true);
        } else {
            line(225, (v.repeat_mode ? "跟读对照" : v.player_sentences ? "听本课句子" : "词句随身听") + std::string("  ") +
                          std::to_string(v.position + 1) + " / " + std::to_string(v.count));
            auto words = c.wrap(v.card->front, 604, 2);
            int scale = 2;
            if (words.size() > 3) {
                scale = 1;
                words = c.wrap(v.card->front, 604, 1);
            }
            for (size_t i = 0; i < std::min<size_t>(size_t(scale == 2 ? 3 : 5), words.size()); ++i)
                c.text(40, 300 + int(i) * (scale == 2 ? 76 : 44), words[i], scale);
            auto meaning =
                c.wrap((v.card->ipa.empty() ? "" : v.card->ipa + "\n") + v.card->back, 604);
            answer_pages_ = std::max(1, int((meaning.size() + 2) / 3));
            const int start = std::min(v.answer_page, answer_pages_ - 1) * 3;
            for (size_t i = start; i < meaning.size() && i < size_t(start + 3); ++i)
                c.text(40, 570 + int(i - start) * 42, meaning[i]);
            if (answer_pages_ > 1)
                button(c, {40, 700, 604, 88}, Action::AnswerPage, "释义 " + std::to_string(v.answer_page+1) + " / " + std::to_string(answer_pages_));
            button(c, {40, 800, 184, 88}, Action::PlayerStep, v.player_sentences ? "上一句" : "上一条", -1, false,
                   v.position > 0 && !v.recording);
            button(c, {250, 800, 184, 88}, Action::Speak, v.audio_source==SpeechSource::Synthesis ? "合成发音" : "SD 发音", 0, false, !v.recording && v.audio_available);
            button(c, {460, 800, 184, 88}, Action::PlayerStep, v.player_sentences ? "下一句" : "下一条", 1, false,
                   v.position + 1 < v.count && !v.recording);
            if (v.repeat_mode) {
                button(c, {40, 910, 280, 108}, Action::Record,
                       v.recording ? "结束录音" : "开始跟读", 0, true);
                button(c, {364, 910, 280, 108}, Action::Replay, "回放我的声音", 0, false,
                       v.clip_available && !v.recording);
            } else
                button(c, {40, 910, 604, 108}, Action::PlayerToggle,
                       v.player_running ? "暂停播放" : "继续播放", 0, true);
        }
    } else if (v.screen == Screen::Stats) {
        title("学习记录");
        for (size_t i = 0; i < std::min<size_t>(9, v.stats.size()); ++i)
            line(350 + int(i) * 68, v.stats[i]);
    } else if (v.screen == Screen::Resources) {
        title("内容与资源");
        for (size_t i = 0; i < std::min<size_t>(6, v.resources.size()); ++i)
            line(350 + int(i) * 65, v.resources[i]);
        line(760, "用 USB 连接电脑，导入课程与发音。");
        button(c, {40, 810, 604, 88}, Action::UsbDisk, "U 盘导入资源", 0, true);
        button(c, {40, 920, 604, 88}, Action::Reload, "重新载入资源");
    }
}
} // namespace wordcard
