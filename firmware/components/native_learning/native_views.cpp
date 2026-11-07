#include "native_study.hpp"
#include <ctime>
#include <sstream>
namespace wordcard {
namespace {
std::string n(int64_t v) { return std::to_string(v); }
std::string text(std::string s) {
    for (size_t p = 0; (p = s.find("＿", p)) != std::string::npos;)
        s.replace(p, 3, "_");
    std::istringstream in(s);
    std::string part, out;
    while (in >> part) {
        if (!out.empty())
            out += ' ';
        out += part;
    }
    return out;
}
struct View {
    WebPage &o;
    int action(J data) {
        for (auto it = data.begin(); it != data.end(); ++it)
            if (!it.value().is_string())
                it.value() = str(it.value());
        o.actions.push_back(data.dump());
        return int(o.actions.size() - 1);
    }
    void line(const std::string &t, const std::string &role = "", bool heading = false,
              const std::string &detail = "") {
        const auto v = text(t);
        if (v.empty())
            return;
        WebBlock b;
        b.kind = heading ? "heading" : "text";
        b.text = v;
        b.role = role;
        b.area = "body";
        b.detail = detail;
        o.blocks.push_back(b);
    }
    void button(const std::string &t, J data, bool primary = false, bool enabled = true,
                const std::string &role = "", bool selected = false, const std::string &area = "body",
                const std::string &detail = "") {
        WebBlock b;
        b.kind = "button";
        b.text = text(t);
        b.detail = text(detail);
        b.area = area;
        b.role = role;
        b.action = action(data);
        b.enabled = enabled;
        b.primary = primary;
        b.selected = selected;
        o.blocks.push_back(b);
    }
    void foot(const std::string &t, J data, bool primary = false, bool enabled = true) {
        button(t, data, primary, enabled, "", false, "foot");
    }
    void top(const std::string &t, J data) {
        o.top.push_back(WebControl{text(t), action(data), false, true});
    }
    void header(const std::string &title, const std::string &back = "", const std::string &pause = "") {
        o.title = title;
        if (!back.empty())
            top("返回", J{{"action", "go"}, {"route", back}});
        if (o.route == "word-batch" || o.route == "quiz")
            top("查词", J{{"action", "lookup"}});
        if (!pause.empty())
            top("暂停", pause == "w-pause" ? J{{"action", pause}} : J{{"action", "go"}, {"route", "pause"}});
    }
    void tabs(const std::string &selected) {
        const char *routes[] = {"home", "books", "review", "dictionary"};
        const char *names[] = {"首页", "课程", "复习", "词典"};
        for (int i = 0; i < 4; ++i)
            o.tabs.push_back(WebControl{names[i], action(J{{"action", "go"}, {"route", routes[i]}}),
                                        selected == routes[i], true});
    }
};
std::string when(int64_t due, int64_t now) {
    const auto delta = due - now;
    if (delta <= 0)
        return "现在到期";
    if (delta < 3600000)
        return n(std::max(int64_t(1), (delta + 59999) / 60000)) + "分钟后";
    if (delta < StudyDay)
        return n((delta + 3599999) / 3600000) + "小时后";
    std::time_t ts = std::time_t(due / 1000 + 28800);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &ts);
#else
    gmtime_r(&ts, &tm);
#endif
    char t[32];
    std::snprintf(t, sizeof t, "%d/%d %02d:%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    return t;
}
std::string kindLabel(const std::string &k) {
    return k == "meaning"    ? "词义"
           : k == "spelling" ? "完整拼写"
           : k == "words"    ? "词汇"
           : k == "listen"   ? "听力"
                             : "阅读";
}
std::string goalLabel(const std::string &k) {
    return k == "words" ? "背单词" : k == "listen" ? "练听力" : k == "read" ? "练阅读" : "综合学习";
}
std::vector<std::string> splitPages(const std::string &s, size_t limit) {
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string w;
    size_t count = 0;
    while (in >> w) {
        if (count++ % limit == 0)
            out.emplace_back();
        if (!out.back().empty())
            out.back() += ' ';
        out.back() += w;
    }
    if (out.empty())
        out.push_back("");
    return out;
}
std::string dailyStatus(const J &s) {
    const auto type = str(s["next"], "type");
    if (!flag(s, "study") && type == "done")
        return "休息日不安排新词与听读，可按需回顾已学内容。";
    if (type == "available-end")
        return "当前开放材料已练过，可回顾已学内容；已完成的数量保留。";
    if (type == "done")
        return "今天安排已完成，之后仍按到期日复习。";
    if (type == "waiting")
        return !s["deferredTargets"].empty()
                   ? "听力暂缓项尚未完成，可在今日计划中补听。"
                   : "当前可练部分已完成，还有 " + n(num(s, "cooling")) + " 项需要稍后再练。";
    if (!s["active"].is_null() && type != "review")
        return "已保存任务：" + str(s["active"], "title") + " · 已走到 " +
               n(num(s["active"], "answered") + 1) + "/" + n(num(s["active"], "total")) +
               " 项；完成后更新今日数量。";
    return "先完成到期词义与拼写；暂缓项等待时，可以继续新内容。";
}
} // namespace
WebPage NativeStudy::page() {
    if(vocabulary_active()) return vocabulary_->page();
    ++pages_;
    WebPage out;
    out.route = str(ui_, "route");
    out.key = key();
    out.typed = str(ui_, "searchQuery");
    out.shift = flag(ui_, "shift");
    out.symbols = flag(ui_, "symbols");
    out.toast = toast_;
    try {
        if (!error_.empty()) {
            View v{out};
            out.title = "请确认";
            v.line("记录需要处理", "", true);
            v.line(error_);
            v.button("导出当前记录", J{{"action", "export"}});
            v.button("返回原题，重试操作", J{{"action", "error-close"}});
            return out;
        }
        if (out.route == "word-batch")
            batchPage(out);
        else if (out.route == "quiz")
            quizPage(out);
        else if (out.route == "word-result" || out.route == "focus-summary")
            resultPage(out);
        else if (out.route == "search" || out.route == "word")
            lookupPage(out);
        else if (out.route == "home")
            homePage(out);
        else if (out.route == "review" || out.route == "word-review")
            reviewPage(out);
        else if (out.route == "path-today")
            todayPage(out);
        else if (out.route == "summary")
            summaryPage(out);
        else if (out.route == "pause") {
            const auto &s = model_.state["session"], l = model_.lesson(str(s, "lesson"));
            View v{out};
            v.line("下次从这里继续", "", true);
            v.line(str(l, "code") + " " + str(l, "title"), "meaning");
            v.line("第 " + n(num(s, "at") + 1) + " 步的题目、选项和已检查结果已经保存。");
            v.line(flag(s, "checked") ? "本题反馈已保存，继续后不会重复计分。"
                                      : "未检查的选项也保留。播放中退出不会当成完整听过。");
            v.line("当前单元的已完成课程和生词本都会保留。", "detail");
            v.foot("继续这一课", J{{"action", "go"}, {"route", "quiz"}}, true);
            v.foot("回首页，稍后继续", J{{"action", "go"}, {"route", "home"}});
            v.header("学习已暂停", "quiz");
        }
        out.key = key();
        return out;
    } catch (const std::exception &e) {
        error_ = e.what();
        out = {};
        out.route = str(ui_, "route");
        out.key = key();
        out.title = "记录需要处理";
        View v{out};
        v.line("本页未能生成，学习记录保留。");
        v.line(error_);
        v.button("返回原题，重试操作", J{{"action", "error-close"}});
        return out;
    }
}
void NativeStudy::batchPage(WebPage &out) {
    View v{out};
    const auto &s = model_.state["batch"];
    if (s.is_null()) {
        resultPage(out);
        return;
    }
    const auto phase = str(s, "phase"), mode = str(s, "mode");
    const auto count = s["ids"].size();
    if (phase == "overview") {
        if (num(s, "reviewVersion") == 1 && flag(s, "reviewKind")) {
            std::string counts;
            J kinds = J::array();
            for (const auto &q : s["targets"])
                addUnique(kinds, q["kind"]);
            for (const auto &k : kinds) {
                int c = 0;
                for (const auto &q : s["targets"])
                    if (q["kind"] == k)
                        ++c;
                if (!counts.empty())
                    counts += "；";
                counts += kindLabel(str(k)) + " " + n(c) + " 项";
            }
            v.line(counts, "", true);
            v.line("直接回忆，不先显示答案。词义和拼写按各自到期时间选题。");
            v.line("答错或用提示后，自动隔至少3个其他已学词再纠正；词不够时至少15分钟后再检查。", "detail");
            v.line("首次结果单独保留。隔词纠正只是这次纠正了，下一次仍需独立回忆。");
            v.foot("开始本次复习", J{{"action", "w-begin"}}, true);
            v.header("本次复习安排", "", "w-pause");
            return;
        }
        const bool modern = num(s, "trainingVersion") == 2 && mode != "revisit" && !flag(s, "reviewKind");
        v.line("本组 " + n(count) + " 个词／表达", "", true);
        v.line(model_.sourceTitle(s["source"]));
        if (modern) {
            v.line("选择训练方式");
            const char *keys[] = {"staged", "full", "meaning"},
                       *names[] = {"分轮拼写", "直接完整拼写", "只练词义"},
                       *notes[] = {"先补两个字母，再整组完整拼写", "词义检查后，直接输入完整英文",
                                   "本组只检查词义；不代记拼写会了"};
            for (int i = 0; i < 3; ++i)
                v.button(names[i], J{{"action", "w-mode"}, {"value", keys[i]}}, false, true, "mode",
                         str(s, "practiceMode") == keys[i], "body", notes[i]);
            v.line("记住上次选择，下组仍可调整。", "detail");
        } else
            v.line("已保存的练习按原题继续，答案与位置保留。", "detail");
        v.line(mode == "learn"     ? "先认识整组，之后再按轮次回忆。"
               : mode == "revisit" ? "先穿插其他已学词，再回访错项。"
                                   : "复习直接回忆，不先显示答案。");
        if (flag(s, "deferredKind"))
            v.line("只有一个已学词；本次检查" + kindLabel(str(s["queue"][size_t(num(s, "at"))], "kind")) +
                       "，另一项至少15分钟后检查。两字母训练在多词组中进行。",
                   "detail");
        v.line("两字母是辅助训练，结果不代替完整拼写。一次练完不等于长期掌握。", "detail");
        v.foot(mode == "learn"     ? "开始认识这组词"
               : mode == "revisit" ? "开始隔词回访"
                                   : "开始本组复习",
               J{{"action", "w-begin"}}, true, !modern || flag(s, "practiceMode"));
        v.header("本组训练安排", "", "w-pause");
        return;
    }
    if (phase == "learn") {
        const auto &w = model_.word(str(s["ids"][size_t(num(s, "learnAt"))]));
        v.line(str(w, "word"), "word", true);
        v.line(str(w, "ipa"), "ipa");
        v.line(str(w, "meaning"), "meaning");
        v.button("单词录音 / 停止", J{{"action", "w-audio"}, {"text", w["word"]}}, false, true, "audio");
        v.line(str(w, "example"), "example");
        v.button("例句录音 / 停止", J{{"action", "w-audio"}, {"text", w["example"]}}, false, true, "audio");
        v.line("先认识整组，之后再检查词义和拼写。", "detail");
        v.foot("上一个", J{{"action", "w-learn"}, {"delta", -1}}, false, num(s, "learnAt") != 0);
        v.foot(size_t(num(s, "learnAt")) + 1 == count ? "学完本组，开始检查" : "下一个词",
               J{{"action", "w-learn"}, {"delta", 1}}, true);
        v.header("认识本组 · " + n(num(s, "learnAt") + 1) + "/" + n(count), "", "w-pause");
        return;
    }
    const auto &q = s["queue"][size_t(num(s, "at"))];
    const auto id = str(q, "id"), kind = str(q, "kind");
    const auto &w = model_.word(id);
    const bool modernReview = num(s, "reviewVersion") == 1;
    if (!s["checked"].is_null()) {
        const auto &r = s["checked"];
        bool deferred = false;
        for (const auto &p : field(s, "deferredReviews"))
            if (p["id"] == q["id"] && p["kind"] == q["kind"])
                deferred = true;
        std::string title, note, button, header;
        if (modernReview) {
            title = flag(q, "followup")  ? (flag(r, "correct") ? "这次纠正了" : "还需要再练")
                    : flag(q, "filler")  ? "穿插题已记录"
                    : flag(r, "correct") ? (flag(r, "assisted") ? "辅助后答对" : "答对了，仍需复习")
                                         : "这项还需巩固";
            note = flag(q, "filler") ? "穿插题不改写首次结果，也不增加间隔回忆次数。"
                   : flag(q, "followup")
                       ? (flag(r, "correct") ? "本次纠正单独记录，不改首次答错，也不算掌握；次日再独立回忆。"
                                             : "不连续照着重答；至少15分钟后再检查这一项。")
                   : !actual(r) ? (deferred ? "其他已学词不足3个；这一项至少15分钟后再检查。"
                                            : "已自动安排：隔至少3个不同的已学词后再检查这一项。")
                                : "记录这次独立答对；到期后还要再次回忆，不直接算掌握。";
            if (kind == "gap")
                note = "两字母为辅助练习；后面仍需完整拼写，辅助结果不算完整拼写会了。";
            button = num(s, "at") + 1 < int64_t(s["queue"].size()) ? "继续其他词" : "查看本轮结果";
            header = kind == "gap" ? "补字母反馈" : kindLabel(kind);
        } else {
            title = flag(r, "correct") ? (flag(r, "assisted") ? "提示后答对" : "答对了") : "这项还需巩固";
            note = flag(q, "filler") ? "穿插题不改写本组首次结果。"
                   : !actual(r)      ? "已记录，放到后面回访，不立即照着重写。"
                                     : "本题已记录，接着回忆其他词。";
            const auto at = num(s, "at");
            const J next = at + 1 < int64_t(s["queue"].size()) ? s["queue"][size_t(at + 1)] : J(nullptr);
            button = next.is_null()                                          ? "查看本轮结果"
                     : kind == "meaning" && str(next, "kind") == "gap"       ? "开始两字母训练"
                     : kind != "spelling" && str(next, "kind") == "spelling" ? "开始完整拼写"
                                                                             : "继续下一项";
            header = (kind == "gap" ? "补字母" : kindLabel(kind)) + "反馈";
        }
        v.line(title, "", true);
        v.line(str(w, "word"), "word");
        v.line(str(w, "ipa"), "ipa");
        v.line(str(w, "meaning"), "meaning");
        if (kind != "meaning")
            v.line("你的输入：" + (str(r, "typed").empty() ? "未输入" : str(r, "typed")), "detail");
        v.line(str(w, "example"), "example");
        v.button("单词录音 / 停止", J{{"action", "w-audio"}, {"text", w["word"]}}, false, true, "audio");
        v.line(note, "detail");
        v.foot(button, J{{"action", "w-next"}}, true);
        v.header(header, "", "w-pause");
        return;
    }
    int64_t index = num(s, "at") + 1, total = s["queue"].size();
    if (!modernReview && mode != "revisit") {
        index = 0;
        for (int64_t i = 0; i <= num(s, "at"); ++i)
            if (str(s["queue"][size_t(i)], "kind") == kind)
                ++index;
        total = count;
    }
    if (kind == "meaning") {
        if (flag(q, "filler"))
            v.line("穿插巩固 · 拉开与错词的间隔", "detail");
        v.line(str(w, "word"), "word", true);
        v.line(str(w, "example"), "example");
        v.line("这里是什么意思？", "question");
        const auto &c = s["choices"][id];
        for (size_t i = 0; i < c["options"].size(); ++i)
            v.button(str(c["options"][i]), J{{"action", "w-choose"}, {"n", i}}, false, true, "option",
                     s["selected"] == i);
        v.foot("检查词义", J{{"action", "w-check"}}, true, !s["selected"].is_null());
        v.foot("不会，看解释", J{{"action", "w-unknown"}});
        v.header(std::string(flag(q, "followup") ? "隔词纠正 · 词义"
                             : flag(q, "filler") ? "穿插巩固 · 词义"
                                                 : "词义检查") +
                     " · " + n(index) + "/" + n(total),
                 "", "w-pause");
        return;
    }
    const bool gap = kind == "gap", templ = str(w, "word").find("...") != std::string::npos ||
                                            str(w, "word").find("…") != std::string::npos;
    v.line(str(w, "meaning"), "", true);
    if (gap) {
        const auto &g = s["gaps"][id];
        v.line(str(g, "mask"));
        out.gap_mask = text(str(g, "mask"));
        v.line("按空格顺序填写" + n(str(g, "answer").size()) + "个字母" +
                   (str(g, "answer").size() < 2 ? "（这个词较短）" : "") + "。",
               "detail");
    } else
        v.line(templ ? "只写句型的英文开头，不输入省略号。" : "凭记忆输入本组对应英文。", "detail");
    v.button("单词录音 / 停止", J{{"action", "w-audio"}, {"text", w["word"]}}, false, true, "audio");
    v.line(gap ? "空缺字母" : "完整英文");
    if (flag(s, "hint")) {
        const auto answer = gap ? str(s["gaps"][id], "answer") : NativeModel::normal(str(w, "word"));
        v.line("提示：" + answer.substr(0, 1) + " · 已记为辅助");
    }
    v.button("提示一个字母", J{{"action", "w-hint"}}, false, !flag(s, "hint"));
    v.button("看答案", J{{"action", "w-unknown"}});
    v.line(gap ? "只填写空缺字母，不用重写整词。" : "不区分大小写；词序、撇号和连字符需正确。", "detail");
    out.keyboard = true;
    out.typed = str(s, "typed");
    out.can_submit = !text(out.typed).empty();
    const auto title = std::string(flag(q, "followup")                               ? "隔词纠正 · 拼写"
                                   : flag(q, "filler")                               ? "穿插巩固 · 拼写"
                                   : gap                                             ? "第1轮 · 补字母"
                                   : str(s, "practiceMode") == "staged" && count > 1 ? "第2轮 · 完整拼写"
                                                                                     : "完整拼写");
    v.header(title + " " + n(index) + "/" + n(total), "", "w-pause");
}
void NativeStudy::quizPage(WebPage &out) {
    View v{out};
    const auto &s = model_.state["session"];
    if (s.is_null()) {
        summaryPage(out);
        return;
    }
    const auto &t = model_.task(s), l = model_.lesson(str(s, "lesson"));
    const auto kind = str(t, "kind");
    const int64_t total = flag(s, "unified")             ? num(s, "courseTaskCount")
                          : str(s, "purpose") == "focus" ? num(s, "baseLength")
                                                         : model_.tasks(s).size();
    auto audio = [&](int step = -1) {
        v.button("录音 / 停止",
                 J{{"action", "audio"}, {"text", t["text"]}, {"step", step < 0 ? J("") : J(step)}}, false,
                 true, "audio");
    };
    if (kind == "learn") {
        v.line(str(t, "title"), "", true);
        v.line("听发音，结合例句理解。稍后会在听读中再次遇到。", "detail");
        for (const auto &id : t["wordIds"]) {
            const auto &w = model_.word(str(id));
            v.line(str(w, "word"), "word");
            v.button("录音 / 停止", J{{"action", "audio"}, {"text", w["word"]}}, false, true, "audio");
            v.line(str(w, "ipa"), "ipa");
            v.line(str(w, "meaning"), "meaning");
            v.line(str(w, "example"), "example");
            v.button("录音 / 停止", J{{"action", "audio"}, {"text", w["example"]}}, false, true, "audio");
            v.button(has(model_.state["notebook"], id) ? "已加入生词本" : "加入生词本",
                     J{{"action", "add"}, {"id", id}});
        }
        v.foot("认识了，继续", J{{"action", "next"}}, true);
    } else if (!s["checked"].is_null()) {
        const auto &c = s["checked"];
        v.line(flag(c, "skip")      ? "听力已暂缓"
               : flag(c, "correct") ? (flag(c, "assisted") ? "查词后答对" : "答对了")
                                    : "先弄懂，再练一次",
               "", true);
        v.line(flag(c, "skip")      ? "这题不会计错，但也不会记为听力完成。课后仍可补听。"
               : flag(c, "correct") ? "你找到了这段信息的关键。"
                                    : "这题会在课末重练；当前回答已保存。");
        if (!flag(c, "skip")) {
            v.line(str(t["options"][size_t(num(t, "answer"))]));
            v.line(str(t, "why"));
            if (kind == "listen") {
                audio();
                v.line(str(t, "text"));
            } else if (kind == "meaning")
                v.line(str(t, "text"), "example");
        }
        v.foot(num(s, "at") + 1 == int64_t(s["queue"].size()) && s["errors"].empty()
                   ? (str(s, "purpose") == "focus" ? "完成本组" : "完成本课")
                   : "继续",
               J{{"action", "next"}}, true);
    } else {
        v.line(str(t, "title"), "", true);
        if (kind == "read") {
            const auto parts = splitPages(str(t, "text"), num(l, "unit") == 2 ? 24 : 32);
            const auto at = std::min(size_t(num(ui_, "readPage")), parts.size() - 1);
            ui_["readPage"] = at;
            v.line(parts[at], "passage");
            if (parts.size() > 1) {
                v.line("原文 " + n(at + 1) + "/" + n(parts.size()) + " 段", "article-position");
                v.button("上段原文", J{{"action", "read-page"}, {"delta", -1}}, false, at != 0, "read-nav");
                v.button("下段原文", J{{"action", "read-page"}, {"delta", 1}}, false, at + 1 != parts.size(),
                         "read-nav");
            }
            v.line(str(t, "q"), "question");
        } else if (kind == "listen") {
            audio(int(num(s, "at")));
            v.button("慢速 / 停止",
                     J{{"action", "audio"}, {"text", t["text"]}, {"step", s["at"]}, {"rate", "0.85"}}, false,
                     true, "audio");
            v.button("稍后听", J{{"action", "skip"}}, false, true, "audio");
            const auto note = audioNote_.empty() ? "完整听完一次后再作答；可以重复听。" : audioNote_;
            v.line(note, note == "完整听完一次后再作答；可以重复听。" ? "detail" : "");
            v.line(str(t, "q"), "question");
        } else if (flag(t, "text"))
            v.line(str(t, "text"), "example");
        else
            v.line(str(t, "q"), "meaning");
        for (size_t i = 0; i < t["options"].size(); ++i)
            v.button(std::string(1, char('A' + i)) + " " + str(t["options"][i]),
                     J{{"action", "choose"}, {"n", i}}, false, true, "option", s["selected"] == i);
        const bool heard = kind != "listen" || flag(s["heard"], str(s["at"]));
        v.foot(s["selected"].is_null() ? (heard ? "选择答案后检查" : "先完整播放录音") : "检查答案",
               J{{"action", "check"}}, true, !s["selected"].is_null() && heard && !playing_);
        v.foot("不会，看看解释", J{{"action", "unknown"}}, false, heard && !playing_);
    }
    const auto title = str(l, "code") + (flag(s, "legacy") ? " 旧版" : "") +
                       (str(s, "purpose") == "focus" ? " " + kindLabel(str(s, "focus")) + "专项" : "") +
                       " · " +
                       (str(s, "mode") == "catchup" ? "补听力"
                        : num(s, "at") >= total     ? "课末巩固"
                                                    : n(num(s, "at") + 1) + " / " + n(total) + " 步");
    v.header(title, "", "pause");
}
void NativeStudy::dailyFooter(WebPage &out) {
    View v{out};
    const auto snap = model_.snapshot(profile_);
    v.foot(NativeModel::nextLabel(snap), J{{"action", "w-day-next"}}, true);
    v.foot("查看今日剩余安排", J{{"action", "p-today"}});
    if (!snap["deferredTargets"].empty() || !field(field(model_.state, "lastPractice"), "skipped").empty() ||
        !field(field(profile_, "lastTraining"), "skipped").empty())
        v.foot("补听暂缓内容", J{{"action", "w-day-catchup"}});
}
void NativeStudy::resultPage(WebPage &out) {
    View v{out};
    if (out.route == "focus-summary") {
        const auto &r = model_.state.at("lastPractice"), l = model_.lesson(str(r, "lesson"));
        v.line(str(l, "code") + " · " + kindLabel(str(r, "focus")) + "专项", "", true);
        int right = 0;
        for (const auto &c : r["first"])
            if (truth(c))
                ++right;
        v.line("首次作答：" + n(right) + " / " + n(r["first"].size()) + " 题正确");
        v.line(r["skipped"].empty() ? "本组作答已完成" : n(r["skipped"].size()) + " 题听力待补");
        v.line("专项成绩单独记录，不会把整课或单元标成完成。原课程进度和单元回顾日程保留。");
        dailyFooter(out);
        v.header(r["skipped"].empty() ? "本组练习完成" : "本组听力待补");
        return;
    }
    const auto &r = model_.state.at("batchLast");
    auto score = [&](const std::string &kind) {
        int total = 0, right = 0;
        for (const auto &id : r["ids"]) {
            const auto &row = field(field(r["results"], str(id)), kind);
            if (!row.is_null()) {
                ++total;
                right += actual(row);
            }
        }
        return total ? n(right) + "/" + n(total) + " 独立答对" : "本轮未检查";
    };
    v.line(model_.sourceTitle(r["source"]), "", true);
    v.line("词义首次");
    v.line(score("meaning"));
    bool gaps = false;
    for (const auto &id : r["ids"])
        if (!field(field(r["results"], str(id)), "gap").is_null())
            gaps = true;
    if (gaps) {
        v.line("两字母辅助");
        v.line(score("gap"));
    }
    v.line("完整拼写首次");
    v.line(score("spelling"));
    const auto &corr = field(r, "corrections");
    if (!corr.empty()) {
        int fixed = 0;
        for (const auto &q : corr)
            fixed += actual(q);
        v.line("隔词纠正");
        v.line(n(fixed) + "/" + n(corr.size()) + " 项已纠正");
    }
    v.line("首次结果保留；纠正与独立回忆分开。一次答对或纠正成功，都不直接算掌握。");
    for (const auto &id : r["ids"]) {
        std::string line = str(model_.word(str(id)), "word") + " · ";
        bool first = true;
        for (const auto *kind : {"meaning", "spelling"})
            if (!field(field(r["results"], str(id)), kind).is_null()) {
                if (!first)
                    line += "；";
                first = false;
                line += kindLabel(kind) + "：" +
                        NativeModel::memoryLabel(model_.memoryRow(model_.state, str(id), kind));
                for (const auto &q : model_.reviewItems(model_.state, kind))
                    if (q["id"] == id) {
                        line += "，" + when(num(q, "due"), model_.now);
                        break;
                    }
            }
        v.line(line, "detail");
    }
    dailyFooter(out);
    v.header("本轮完成，继续巩固", "home");
}
void NativeStudy::lookupPage(WebPage &out) {
    View v{out};
    const bool context = !field(model_.state, "lookup").is_null();
    if (out.route == "search") {
        const auto query = str(ui_, "searchQuery");
        auto lower = [](std::string s) {
            for (auto &c : s)
                if (c >= 'A' && c <= 'Z')
                    c += 32;
            return s;
        };
        const auto q = lower(text(query));
        const auto origin = str(field(field(model_.state, "lookup"), "point"), "route");
        v.line(context ? (origin == "quiz" || origin == "word-batch" || origin == "path-practice"
                              ? "查词后返回原位置；本题记为辅助练习。"
                              : "查看词条后返回原列表。")
                       : ("可查询 " + n(data()["catalog"].size()) + " 项日常／科技与课内表达。"),
               "detail");
        int matches = 0;
        if (!q.empty())
            for (const auto &w : data()["catalog"])
                if (lower(str(w, "word")).find(q) != std::string::npos) {
                    v.button(str(w, "word"), J{{"action", "word"}, {"id", w["id"]}}, false, true, "row",
                             false, "body", str(w, "meaning"));
                    if (++matches == 3)
                        break;
                }
        if (!query.empty() && !matches)
            v.line("试学词库未找到。当前不是完整离线词典。");
        out.keyboard = true;
        out.can_submit = true;
        v.header(context ? "临时查词" : "输入查词", context ? "" : "dictionary");
        if (context)
            v.top("返回原处", J{{"action", "lookup-return"}});
        return;
    }
    const auto &w = model_.word(str(ui_, "entryId"));
    if (w.is_null()) {
        out.route = "search";
        lookupPage(out);
        return;
    }
    const bool saved =
        has(model_.state["notebook"], w["id"]) || has(field(model_.state, "extraNotebook"), w["id"]);
    v.line(str(w, "word"), "word", true);
    v.line(str(w, "ipa"), "ipa");
    v.button("录音 / 停止", J{{"action", "audio"}, {"text", w["word"]}}, false, true, "audio");
    v.line(str(w, "meaning"), "meaning");
    const auto source = str(w, "source");
    v.line(model_.sourceTitle(
               J{{"type", source.rfind("bridge:", 0) == 0 ? "bridge" : "course"}, {"id", source.substr(7)}}),
           "detail");
    v.line(str(w, "example"), "example");
    v.button("录音 / 停止", J{{"action", "audio"}, {"text", w["example"]}}, false, true, "audio");
    v.line(saved ? "已在生词本，可从复习入口集中练习。" : "加入生词本，之后集中复习。");
    v.foot(saved ? "移出生词本" : "加入生词本", J{{"action", saved ? "remove-word" : "add"}, {"id", w["id"]}},
           true);
    v.foot(context ? "返回原位置" : "返回查词",
           J{{"action", context ? "lookup-return" : "go"}, {"route", "search"}});
    v.header("词义与发音", "search");
}
void NativeStudy::homePage(WebPage &out) {
    View v{out};
    const auto snap = model_.snapshot(profile_), active = snap["active"];
    const auto nextType = str(snap["next"], "type"), activeType = str(active, "type");
    const auto &b = field(model_.state, "batch").is_null() ? field(model_.state, "dailyPausedBatch")
                                                           : model_.state["batch"];
    J source;
    if (activeType == "batch" || activeType == "saved-batch" || activeType == "review")
        source = b["source"];
    else if (activeType == "course")
        source = J{{"type", "course"}, {"id", model_.state["session"]["lesson"]}};
    else if (activeType == "practice")
        source = J{{"type", "bridge"}, {"id", profile_["training"]["pack"]}};
    else if (activeType != "assessment") {
        for (const auto &t : snap["stages"])
            if (field(t, "kind") == field(snap["next"], "kind"))
                source = field(t, "target");
        if (source.is_null())
            source = J{{"type", "course"}, {"id", field(snap["plan"], "lesson")}};
    }
    const auto &l = str(source, "type") == "course" ? model_.lesson(str(source, "id")) : J(nullptr);
    J pack;
    if (str(source, "type") == "bridge")
        for (const auto &p : data()["packs"])
            if (p["id"] == source["id"])
                pack = p;
    const auto current = activeType == "assessment"        ? "继续已保存的起点摸底"
                         : !pack.is_null()                 ? str(pack, "title")
                         : !l.is_null()                    ? str(l, "code") + " · " + str(l, "title")
                         : str(source, "type") == "review" ? model_.sourceTitle(source)
                                                           : "当前学习安排",
               track = activeType == "assessment" ? "词汇、听力与阅读"
                       : !pack.is_null()          ? "日常与科技 · 听读练习"
                       : !l.is_null()             ? "Empower A2 · 第 " + n(num(l, "unit")) + " 单元"
                                                  : "自主复习";
    std::string next;
    if (nextType == "review" || nextType == "active-review")
        next = "先巩固到期词义与拼写，再学新内容";
    else if (!b.is_null() && (nextType == "batch" || nextType == "saved-batch"))
        next = str(b, "phase") == "learn"
                   ? "认识新词 · 第 " + n(num(b, "learnAt") + 1) + " / " + n(b["ids"].size()) + " 个"
               : str(b, "phase") == "overview"
                   ? "已准备好一组词，选择训练方式"
                   : "检查词义与拼写 · 第 " + n(num(b, "at") + 1) + " / " + n(b["queue"].size()) + " 项";
    else if (!active.is_null())
        next = std::string(activeType == "assessment" ? "起点摸底" : "听读练习") + " · 第 " +
               n(num(active, "answered") + 1) + " / " + n(num(active, "total")) + " 项";
    else if (nextType == "done")
        next = flag(snap, "study") ? "今日安排已完成，明天接着学" : "今天休息，可按需回顾";
    else if (nextType == "available-end")
        next = "当前可用材料已练完，可以回顾已学内容";
    else if (nextType == "waiting")
        next =
            snap["deferredTargets"].empty() ? "错项稍后再练，当前进度已保存" : "还有暂缓听力，可以接着补听";
    else
        next = "按今天的安排，先完成一小组";
    const auto remaining = num(snap, "remaining"), cooling = num(snap, "cooling");
    const auto summary = remaining                     ? "今日还剩 " + n(remaining) + " 项"
                         : cooling                     ? "稍后再练 " + n(cooling) + " 项"
                         : !flag(snap, "study")        ? "休息日，按需回顾"
                         : nextType == "available-end" ? "当前材料已练完"
                                                       : "今日安排已完成";
    const auto goal = goalLabel(str(snap["plan"], "goal"));
    if (flag(ui_, "nativeHomeOptions")) {
        out.route = "home-options";
        out.title = "学习安排";
        v.line(summary, "section-heading");
        v.line(flag(snap, "provisional") ? "试学安排，可调整方向与时间"
                                         : goal + " · 每天 " + n(num(snap["plan"], "minutes")) + " 分钟",
               "section-note");
        v.button("今日分项进度", J{{"action", "p-today"}, {"homeReturn", "path-today"}}, false, true,
                 "section-row", false, "body", "查看词义、拼写、新词与听读");
        v.button("学习计划",
                 field(profile_, "plan").is_null()
                     ? J{{"action", "p-edit"}}
                     : J{{"action", "go"}, {"route", "path-plan"}, {"homeReturn", "path-plan"}},
                 false, true, "section-row", false, "body", "调整学习方向与时间");
        const bool attempt = !field(profile_, "attempt").is_null(),
                   report = !field(profile_, "report").is_null();
        const auto route = report ? "path-report" : flag(profile_, "goal") ? "path-brief" : "path-goal";
        v.button(attempt  ? "继续摸底"
                 : report ? "摸底报告"
                          : "起点摸底",
                 attempt ? J{{"action", "p-resume"}}
                         : J{{"action", "go"}, {"route", route}, {"homeReturn", route}},
                 false, true, "section-row", false, "body", "判断起点，查看课程建议");
        v.button("课内生词本 · " +
                     n(model_.state["notebook"].size() + field(model_.state, "extraNotebook").size()),
                 J{{"action", "u-notebook"}, {"homeReturn", "word-list"}}, false, true, "section-row", false,
                 "body", "查看收藏的课程词语");
        v.tabs("home");
        return;
    }
    out.title = "首页";
    const auto date = str(snap, "date");
    v.line(date.substr(5, 2) + "月" + date.substr(8, 2) + "日 · " +
               (flag(snap, "study") ? (flag(snap, "provisional") ? "试学安排" : goal) + " · " +
                                          n(num(snap["plan"], "minutes")) + "分钟"
                                    : "休息日 · 按需回顾"),
           "home-meta");
    v.line(track, "home-course", false, current);
    v.line(next, "home-next");
    const bool resume =
        nextType == "batch" || nextType == "saved-batch" || nextType == "course" || nextType == "practice";
    v.button(resume ? "继续学习" : NativeModel::nextLabel(snap), J{{"action", "w-day-next"}}, true, true,
             "home-primary");
    v.line(summary, "home-summary");
    v.button("学习安排", J{{"action", "native-home-options"}}, false, true, "home-today");
    v.line(date + " · " +
               (flag(snap, "provisional")
                    ? "试学安排，可设置方向与时间"
                    : goal + "为重点 · " + n(num(snap["plan"], "minutes")) + "分钟预算") +
               (flag(snap, "study") ? "" : " · 休息日"),
           "detail");
    v.line(dailyStatus(snap), "detail");
    v.tabs("home");
}
void NativeStudy::reviewPage(WebPage &out) {
    View v{out};
    const auto rows = model_.reviewItems(model_.state);
    const auto ids = model_.known(model_.state);
    if (out.route == "word-review") {
        const auto total = std::max(size_t(1), (ids.size() + 2) / 3);
        size_t page = std::min(size_t(std::max(int64_t(0), num(ui_, "reviewPage"))), total - 1);
        ui_["reviewPage"] = page;
        v.line("词义、完整拼写各自记录。纠正不改首次结果，也不增加间隔回忆次数；到期独立答对后逐步安排1／2／4"
               "／7天复习。这是当前试用规则。",
               "detail");
        for (size_t i = page * 3; i < std::min(ids.size(), page * 3 + 3); ++i) {
            const auto id = str(ids[i]);
            v.line(str(model_.word(id), "word"), "", true);
            for (const auto *kind : {"meaning", "spelling"}) {
                J item;
                for (const auto &q : rows)
                    if (str(q, "id") == id && str(q, "kind") == kind) {
                        item = q;
                        break;
                    }
                v.line(
                    kindLabel(kind) + "：" +
                    NativeModel::memoryLabel(model_.memoryRow(model_.state, id, kind)) +
                    (item.is_null() ? "尚未安排，不代记为会" : "下次 " + when(num(item, "due"), model_.now)));
            }
        }
        if (ids.empty()) {
            v.line("还没有已学词", "", true);
            v.line("先从教材认识一组词，或把查到的词加入生词本。");
        }
        v.button("上一页", J{{"action", "w-r-page"}, {"delta", -1}}, false, page > 0);
        v.button(n(page + 1) + "/" + n(total) + " · 下一页", J{{"action", "w-r-page"}, {"delta", 1}}, false,
                 page + 1 < total);
        v.foot("主动练词义", J{{"action", "w-r-start"}, {"mode", "all"}, {"kind", "meaning"}}, false,
               !ids.empty());
        v.foot("主动练完整拼写", J{{"action", "w-r-start"}, {"mode", "all"}, {"kind", "spelling"}}, false,
               !ids.empty());
        v.header("错项与复习日程", "review");
        v.tabs("review");
        return;
    }
    int meaning = 0, spelling = 0, units = 0;
    for (const auto &q : rows)
        if (flag(q, "ready")) {
            if (str(q, "kind") == "meaning")
                ++meaning;
            else
                ++spelling;
        }
    for (const auto &q : model_.state["reviewSchedule"])
        if (!q["due"].is_null() && num(q, "due") <= model_.now)
            ++units;
    out.title = "复习";
    v.line(meaning + spelling ? "先复习到期内容" : "今天暂无到期词汇", "section-heading");
    v.line("词义 " + n(meaning) + " 项 · 拼写 " + n(spelling) + " 项", "section-note");
    if (!field(model_.state, "batch").is_null())
        v.button("继续未完成的背词", J{{"action", "w-resume"}}, false, true, "section-link");
    auto section = [&](const std::string &name, const std::string &detail, J action, bool primary = false,
                       bool enabled = true, const std::string &role = "section-row") {
        action["nativeFrom"] = "review";
        v.button(name, action, primary, enabled, role, false, "body", detail);
    };
    section("复习词义", n(meaning) + " 项到期",
            J{{"action", "w-r-start"}, {"kind", "meaning"}, {"mode", "due"}}, true, meaning > 0);
    section("复习完整拼写", n(spelling) + " 项到期",
            J{{"action", "w-r-start"}, {"kind", "spelling"}, {"mode", "due"}}, false, spelling > 0);
    section("听读回顾", n(units) + " 个单元到期", J{{"action", "go"}, {"route", "review-schedule"}});
    section("错词与复习日程", "查看分项状态，也可主动加练", J{{"action", "go"}, {"route", "word-review"}});
    section("课内生词本 · " +
                n(model_.state["notebook"].size() + field(model_.state, "extraNotebook").size()),
            "", J{{"action", "u-notebook"}}, false, true, "section-link");
    v.line("错后隔词再练；一次答对不算掌握。", "detail");
    v.tabs("review");
}
void NativeStudy::todayPage(WebPage &out) {
    View v{out};
    const auto s = model_.snapshot(profile_);
    v.line(str(s, "date") + " · " + (flag(s, "study") ? "先复习，再学新内容" : "休息与到期回顾"), "", true);
    for (const auto &t : s["stages"])
        v.line(str(t, "label"), "progress", false,
               n(num(t, "done")) + " / " + n(num(t, "total")) + (str(t, "kind") == "words" ? "个" : "项"));
    v.line(dailyStatus(s));
    v.line("数量按实际完成更新，打开入口不算完成；纠正不等于掌握。新词、听力和阅读以现有材料及时间预算安排。",
           "detail");
    if (!s["deferredTargets"].empty())
        v.button("补听暂缓内容", J{{"action", "w-day-catchup"}}, false, true, "row", false, "body",
                 "未听完的题不算完成；现在可以继续");
    v.button("查看复习日程", J{{"action", "go"}, {"route", "review"}}, false, true, "row", false, "body",
             "查看稍后可练的词义与拼写");
    v.button("查看本周安排", J{{"action", "w-day-week"}}, false, true, "row", false, "body",
             "保留原来的7日计划与未来日期查看");
    v.button("调整学习方向与时间", J{{"action", "p-edit"}}, false, true, "row", false, "body",
             "已有课程与今天已完成的数量保留");
    v.foot(NativeModel::nextLabel(s), J{{"action", "w-day-next"}}, true);
    v.header("今日计划", "home");
    v.tabs("home");
}
void NativeStudy::summaryPage(WebPage &out) {
    View v{out};
    const auto id = str(ui_, "lastLesson");
    const bool legacy = flag(ui_, "legacySummary");
    const auto &l = model_.lesson(id), c = field(model_.state[legacy ? "legacyCompleted" : "completed"], id);
    if (l.is_null() || c.is_null()) {
        homePage(out);
        return;
    }
    J next;
    const bool review = str(l, "role") == "unit-review";
    if (legacy)
        next = J{{"action", "start"}, {"id", id}, {"code", l["code"]}};
    else if (!c["skipped"].empty())
        next = J{{"action", "catchup"}, {"id", id}};
    else if (review) {
        for (const auto &u : data()["units"]) {
            int count = 0;
            for (const auto &t : data()["lessons"])
                if (t["unit"] == u["number"] && str(t, "role") == "lesson")
                    ++count;
            if (num(u, "number") > num(l, "unit") && count == 4 &&
                model_.unitCount(model_.state, int(num(u, "number"))) < 4) {
                for (const auto &p : u["lessons"])
                    if (!model_.complete(model_.state, str(p, "id"))) {
                        next = J{{"action", "lesson"}, {"id", p["id"]}, {"code", p["code"]}};
                        break;
                    }
                if (!next.is_null())
                    break;
            }
        }
    }
    if (next.is_null() && !review) {
        for (const auto &t : data()["lessons"])
            if (t["unit"] == l["unit"] && str(t, "role") == "lesson" &&
                !model_.complete(model_.state, str(t, "id"))) {
                next = J{{"action", "start"}, {"id", t["id"]}, {"code", t["code"]}};
                break;
            }
        if (next.is_null()) {
            const auto &r = model_.lesson("u" + str(l["unit"]) + "r");
            if (r.is_null())
                next = J{{"action", "lesson-review"}, {"id", id}};
            else if (!model_.complete(model_.state, str(r, "id")))
                next = J{{"action", "start"}, {"id", r["id"]}, {"code", r["code"]}};
        }
    }
    if (next.is_null())
        next = J{{"action", "go"}, {"route", "review"}};
    int right = 0;
    for (const auto &q : c["first"])
        right += truth(q);
    const auto unit = model_.unitCount(model_.state, int(num(l, "unit")));
    v.line(str(l, "code") + " " + str(l, "title"), "", true);
    v.line("首次作答：" + n(right) + " / " + n(c["first"].size()) + " 题正确");
    v.line(review ? "本单元目标表达抽样复现"
                  : (legacy ? "旧版" : n(l["words"].size()) + " 个") + "目标表达的学习记录保留");
    v.line("听力：" + (c["skipped"].empty() ? "本次已完成" : n(c["skipped"].size()) + " 题待补"));
    v.line("第 " + n(num(l, "unit")) + " 单元：" + n(unit) + " / 4 课完成");
    const auto action = str(next, "action");
    const bool nextReview = str(model_.lesson(str(next, "id")), "role") == "unit-review";
    std::string note =
        legacy ? "旧版完成状态单独保留。扩充课有新材料和新任务，不会静默替你标记已完成。"
        : !c["skipped"].empty()
            ? (model_.complete(model_.state, id)
                   ? "本次重练还有 " + n(c["skipped"].size()) + " 题听力待补；此前的整课和单元完成记录保留。"
                   : "本课还有 " + n(c["skipped"].size()) + " 题听力待补；完成补听后才算整课完成。")
        : review ? "四课与综合复习各有记录。" +
                       std::string(action == "lesson" ? "可以进入第二单元；第一单元到期回顾仍按日程保留。"
                                                      : "下一次听读回顾会按日程出现，也可提前巩固。")
        : action == "go"            ? "四课和综合复习都已完成，接下来按日程回顾，也可主动巩固。"
        : nextReview                ? "四课已经练完，接下来用全新的材料做综合听读复习。"
        : action == "lesson-review" ? "本单元其他正文待制作。先巩固这一课，或回目录选课。"
                                    : "这一课练完了，下一课会复用前面学过的表达。";
    v.line(note);
    const auto label = legacy                ? "开始扩充版 " + str(l, "code")
                       : action == "catchup" ? "补本课听力"
                       : action == "lesson"  ? "进入下一单元 " + str(next, "code")
                       : action == "start"   ? (nextReview ? "开始综合听读复习" : "继续 " + str(next, "code"))
                       : action == "go"      ? "查看复习安排"
                                             : "巩固本课词汇";
    v.foot(label,
           J{{"action", action}, {"id", str(next, "id")}, {"unit", l["unit"]}, {"route", str(next, "route")}},
           true);
    v.foot("回到学习首页", J{{"action", "go"}, {"route", "home"}});
    v.header(legacy                  ? "旧版记录已保存"
             : !c["skipped"].empty() ? (model_.complete(model_.state, id) ? "本次听力待补" : "本课听力待补")
             : review                ? "综合听读已完成"
                                     : "这一课完成了");
}
} // namespace wordcard
