// Uses the existing native white-button design. No answers or transcripts on listening pages.
void UI::assessment_pages(Canvas &c, const View &v) {
    const auto &a = v.assessment;
    const auto &s = a.state;
    auto text = [&](int y, const std::string &t, int lines = 3) {
        auto wrapped = c.wrap(t, 604);
        for (int i = 0; i < std::min(lines, int(wrapped.size())); ++i)
            c.text(40, y + i * 42, wrapped[i]);
    };
    auto title = [&](const std::string &t) { strong(c, 40, 143, t); };
    auto status = [&](int y) {
        text(y,
             !v.notice.empty() ? v.notice
             : !a.storage_ok   ? "测试尚未保存，请检查 SD 卡。"
                               : "",
             2);
    };
    if (v.screen == Screen::AssessmentHome) {
        button(c, {40, 16, 136, 96}, Action::Back, "返回");
        title("听读起点测试");
        text(230, "读懂通知、安排和说明；听懂日常信息。");
        strong(c, 40, 350, "阅读 12 题  /  听力 12 题");
        text(430, "独立作答，不查词。提交后不能回改。测试中不显示答案。", 3);
        text(585, "听力每题最多完整播放两遍。不方便听时可只完成阅读。", 3);
        text(742, "希望学哪类英语？");
        for (int i = 0; i < 2; ++i)
            button(c, {40 + i * 310, 792, 294, 88}, Action::AssessmentInterest,
                   i ? "科技英语" : "日常英语", i, i == s.interest);
        text(906, "用于选课的试测，尚未校准 CEFR。已有课程进度不会改变。", 2);
        status(1000);
        if (!a.record_valid)
            button(c, {40, 1090, 604, 90}, Action::Back, "返回学习，保留原记录", 0, true);
        else if (!a.storage_ok)
            button(c, {40, 1090, 604, 90}, Action::AssessmentSave, "重试保存测试记录", 0, true);
        else if (s.stage == AssessmentStage::Done) {
            button(c, {40, 1090, 294, 90}, Action::AssessmentReport, "查看分项报告", 0, true);
            button(c, {350, 1090, 294, 90}, Action::AssessmentBegin, "再测一次", 0, false,
                   a.storage_ok);
        } else
            button(c, {40, 1090, 604, 90}, Action::AssessmentBegin,
                   s.stage == AssessmentStage::Ready ? "开始阅读测试" : "继续未完成的测试", 0, true,
                   a.storage_ok);
        return;
    }
    if (v.screen == Screen::AssessmentRestart || v.screen == Screen::AssessmentSkip ||
        v.screen == Screen::AssessmentPause) {
        button(c, {40, 16, 136, 96}, Action::Back, "返回");
        const bool restart = v.screen == Screen::AssessmentRestart,
                   skip = v.screen == Screen::AssessmentSkip;
        title(restart ? "再次使用同一套题" : skip ? "暂不测剩余听力？" : "测试已暂停");
        text(300,
             restart
                 ? "上次报告会单独保留。但题目相同，记住答案会影响结果，不能把涨分当成能力进步。"
             : skip         ? "未完成的听力题标记为未测，不计错。阅读结果和已经提交的听力答案保留。"
             : a.storage_ok ? "当前题目、已提交答案与播放次数已保存。"
                            : "测试记录尚未保存成功，请保持开机。",
             5);
        text(565,
             restart        ? "需要重新确认时再开始，不必反复刷题。"
             : skip         ? "如果是录音缺失，请先合并本版 SD 资源包；也可以返回重试。"
             : a.storage_ok ? "回到首页可以继续原来的课程。下次从测试入口回来即可接着做。"
                            : "检查 SD 卡后重试保存。保存成功前，关机会丢失尚未保存的测试操作。",
             4);
        status(810);
        if (!a.storage_ok)
            button(c, {40, 940, 604, 90}, Action::AssessmentSave, "重试保存测试记录");
        else
            button(c, {40, 940, 604, 90},
                   restart ? Action::AssessmentRestart
                   : skip  ? Action::AssessmentSkipConfirm
                           : Action::AssessmentResume,
                   restart ? "保留旧报告，开始新测试"
                   : skip  ? "暂不测听力，查看报告"
                           : "继续测试",
                   0, true);
        button(c, {40, 1080, 604, 90}, Action::Back,
               restart ? "保留当前报告"
               : skip  ? "返回，继续听力"
                       : "回首页，稍后继续");
        return;
    }
    if (v.screen == Screen::AssessmentSound) {
        button(c, {40, 16, 136, 96}, Action::AssessmentPause, "暂停");
        title("先确认听得清");
        text(245, "阅读已提交。接下来是 12 道听力题。");
        button(c, {40, 380, 604, 108}, Action::AssessmentPlay, a.busy ? "正在播放" : "播放试听录音",
               0, true, a.can_play);
        text(545, "用侧面的音量键调到舒适音量。试听不计分，也不占题目的播放次数。", 3);
        text(740, "正式题目不显示原文或翻译。每题最多完整播放两遍。", 3);
        status(890);
        button(c, {40, 995, 604, 88},
               a.storage_ok ? Action::AssessmentSoundConfirm : Action::AssessmentSave,
               a.storage_ok ? "听清了，开始听力" : "重试保存测试记录", 0, true,
               a.storage_ok ? s.sound_heard && !s.inflight : a.record_valid);
        button(c, {40, 1096, 604, 88}, Action::AssessmentSkip, "现在不方便听");
        return;
    }
    if (v.screen == Screen::AssessmentReport) {
        button(c, {40, 16, 136, 96}, Action::Back, "返回");
        title(s.attempt > 1 ? "分项报告 · 同卷复测" : "听读分项报告");
        for (int k = 0; k < 2; ++k)
            button(c, {40 + k * 310, 218, 294, 88}, Action::AssessmentSkill, k ? "听力" : "阅读", k,
                   k == a.report_skill);
        const auto &r = a.reports[a.report_skill];
        strong(c, 40, 334,
               r.valid ? "答对 " + std::to_string(r.correct) + " / " + std::to_string(r.valid) +
                             " 道有效题"
                       : "本项未测");
        text(390,
             "不会 " + std::to_string(r.unknown) + " 题 · 未测 " + std::to_string(12 - r.valid) +
                 " 题",
             1);
        static const char *targets[] = {"短句与具体信息", "通知与操作要求", "原因、条件与顺序",
                                        "观点与证据"};
        for (int b = 0; b < 4; ++b) {
            int y = 470 + b * 50;
            c.text(40, y, targets[b]);
            c.text(536, y,
                   r.band_valid[b]
                       ? std::to_string(r.band_correct[b]) + " / " + std::to_string(r.band_valid[b])
                       : "未测");
        }
        text(680, "按有效题统计；未测不计错。", 1);
        for (int t = 0; t < 2; ++t)
            button(c, {40 + t * 310, 748, 294, 88}, Action::AssessmentInterest,
                   t ? "科技英语" : "日常英语", t, t == s.interest);
        text(856, r.reason, 2);
        std::string course;
        for (const auto &g : guided_courses())
            if (g.id == r.course_id)
                course = g.title;
        if (!course.empty())
            button(c, {40, 966, 604, 94}, Action::AssessmentRecommend, "试学：" + course, 0, true);
        else
            button(c, {40, 966, 604, 94}, Action::GuidedCourses, "自行选择课程", 0, true);
        text(1090, "仅为选课建议，未校准 CEFR。口语和写作未测，不是完整英语等级。", 3);
        return;
    }
    if (v.screen != Screen::AssessmentQuestion || !a.item)
        return;
    const bool listen = s.stage == AssessmentStage::Listening;
    const auto &q = *a.item;
    const auto &r = s.responses[s.position];
    button(c, {40, 16, 136, 96}, Action::AssessmentPause, "暂停");
    c.text(365, 45,
           std::string(listen ? "听力 " : "阅读 ") + std::to_string(s.position % 12 + 1) + " / 12");
    title(listen ? "听取信息，选择答案" : "阅读材料，选择答案");
    if (listen) {
        button(c, {40, 248, 604, 108}, Action::AssessmentPlay,
               a.busy         ? "正在播放"
               : r.plays == 0 ? "播放录音"
               : r.plays == 1 ? "再听一遍"
                              : "已听两遍",
               0, true, a.can_play);
        text(398, "完整播放 " + std::to_string(r.plays) + " / 2 次", 1);
        text(449, r.plays ? "按听到的信息作答，不确定可选不会。" : "听完录音后才能提交答案。", 2);
        button(c, {40, 519, 604, 88}, Action::AssessmentSkip, "无法继续听？暂不测剩余听力");
    } else {
        const auto lines = c.wrap(q.text, 604);
        answer_pages_ = std::max(1, (int(lines.size()) + 6) / 7);
        const int page = std::clamp(a.page, 0, answer_pages_ - 1);
        for (int i = page * 7; i < std::min(int(lines.size()), page * 7 + 7); ++i)
            c.text(40, 215 + (i - page * 7) * 42, lines[i]);
        if (answer_pages_ > 1) {
            button(c, {40, 535, 186, 88}, Action::AssessmentPage, "上一页", -1, false, page > 0);
            c.text_px(286, 560, std::to_string(page + 1) + " / " + std::to_string(answer_pages_),36);
            button(c, {458, 535, 186, 88}, Action::AssessmentPage, "下一页", 1, false,
                   page + 1 < answer_pages_);
        }
    }
    text(listen ? 624 : 640, q.question, 2);
    for (int i = 0; i < 3; ++i)
        button(c, {40, 720 + i * 96, 604, 88}, Action::AssessmentChoose, q.options[i], i,
               i == s.selected, !s.inflight && a.storage_ok);
    status(1008);
    const bool heard = !listen || r.plays > 0;
    button(c, {40, 1096, 192, 96}, Action::AssessmentUnknown, "不会", 0, false,
           heard && !s.inflight && a.storage_ok);
    button(c, {248, 1096, 396, 96},
           a.storage_ok ? Action::AssessmentSubmit : Action::AssessmentSave,
           !a.storage_ok      ? "重试保存"
           : s.position == 23 ? "提交并看报告"
                              : "提交并继续",
           0, true, a.storage_ok ? a.can_submit : a.record_valid);
}
