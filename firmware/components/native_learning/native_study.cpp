#include "native_study.hpp"
#include <chrono>
#include <sstream>
namespace wordcard {
extern std::string native_learning_json();
const J &NativeStudy::data() {
    static const J value = J::parse(native_learning_json());
    return value;
}
NativeStudy::NativeStudy(Write writer, const std::string &directory) : writer_(std::move(writer)), model_(data()) {
    if(!directory.empty()) vocabulary_=std::make_unique<NativeVocabulary>(directory);
}
bool NativeStudy::supported(const std::string &r) {
    return r == "word-batch" || r == "word-result" || r == "quiz" || r == "focus-summary" || r == "summary" ||
           r == "pause" || r == "search" || r == "word" || r == "home" || r == "review" ||
           r == "word-review" || r == "path-today";
}
bool NativeStudy::enter(const std::string &raw) {
    try {
        const J c = J::parse(raw);
        if (!supported(str(c["ui"], "route")) || flag(c, "playing") || flag(c, "modal") ||
            flag(c, "corrupt") || flag(c, "error"))
            return false;
        // Pre-review-policy sessions remain on their original compatibility
        // implementation. Never reinterpret an old learner's saved questions.
        const auto route = str(c["ui"], "route");
        if ((route == "word-batch" && num(field(c["course"], "batch"), "reviewVersion") != 1) ||
            (route == "word-result" && num(field(c["course"], "batchLast"), "reviewVersion") != 1))
            return false;
        require(model_.validate(c.at("course")), "原生学习记录校验未通过");
        model_.state = c["course"];
        profile_ = c["profile"];
        ui_ = c["ui"];
        model_.now = num(c, "now");
        audioSerial_ = int(num(c, "soundSerial"));
        error_.clear();
        toast_ = str(c, "toast");
        audioNote_ = str(c, "audioNote");
        playing_ = false;
        pendingAudio_ = nullptr;
        active_ = true;
        if(vocabulary_) {
            if(route=="home" && vocabulary_enabled_) vocabulary_->enter(J{{"course",model_.state},{"profile",profile_}},model_.now);
            else vocabulary_->leave();
        }
        return true;
    } catch (const std::exception &e) {
        error_ = e.what();
        return false;
    }
}
std::string NativeStudy::context() const {
    return J{{"course", model_.state},
             {"profile", profile_},
             {"ui", ui_},
             {"now", model_.now},
             {"soundSerial", audioSerial_},
             {"pendingAudio", pendingAudio_},
             {"audioNote", audioNote_},
             {"toast", toast_},
             {"error", error_}}
        .dump();
}
bool NativeStudy::commit(const std::string &method, const J &args) {
    try {
        J draft = model_.state;
        require(model_.mutate(draft, method, args), "原生学习动作不存在");
        require(model_.validate(draft), "学习记录未通过校验");
        require(writer_("wordcard-adult-web-v2", draft.dump()), "SD 保存失败，进度未前进，请重试。");
        model_.state = std::move(draft);
        error_.clear();
        return true;
    } catch (const std::exception &e) {
        error_ = e.what();
        return false;
    }
}
void NativeStudy::stopAudio() {
    if (playing_) {
        pendingAudio_ = J{{"stop", true}, {"serial", ++audioSerial_}};
    }
    playing_ = false;
    playingText_.clear();
    playingSerial_ = 0;
    audioAt_ = -1;
}
void NativeStudy::go(const std::string &destination) {
    stopAudio();
    audioNote_.clear();
    auto r = destination;
    if (r == "quiz") {
        if (!model_.pending(model_.state).empty()) {
            const auto minutes = num(field(profile_, "plan"), "minutes");
            if (!commit("coursePrepare", J::array({field(profile_, "plan").is_null() ? 6
                                                   : minutes == 10                   ? 4
                                                   : minutes == 15                   ? 6
                                                                                     : 8})))
                return;
            r = "word-batch";
        } else
            ui_["readPage"] = num(field(model_.state, "session"), "page");
    }
    if (!field(model_.state, "lookup").is_null() && (r == "search" || r == "word") &&
        !commit("lookupEdit", J::array({ui_["searchQuery"], ui_["entryId"], r})))
        return;
    ui_["route"] = r;
    if (r == "quiz" || r == "word-batch")
        ui_["nativeHomeOptions"] = false;
}
void NativeStudy::play(const std::string &text, int step, double rate, bool toggle) {
    if (toggle && playing_ && playingText_ == text) {
        stopAudio();
        audioNote_ = "已停止；未播完不算听过。";
        return;
    }
    stopAudio();
    const auto &file = field(data()["audio"], text);
    if (file.is_null()) {
        audioNote_ = "这段录音未找到，请重新打开网页资源包。";
        return;
    }
    playingText_ = text;
    playingLesson_ = str(field(model_.state, "session"), "lesson");
    audioAt_ = step;
    playingSerial_ = ++audioSerial_;
    playing_ = true;
    audioNote_ = "正在播放…";
    pendingAudio_ = J{{"path", file}, {"rate", rate}, {"serial", playingSerial_}};
}
bool NativeStudy::playback(int serial, bool success) {
    if(vocabulary_active()) return vocabulary_->playback(serial,success);
    if (serial != playingSerial_ || !playing_)
        return false;
    playing_ = false;
    playingSerial_ = 0;
    audioNote_ = success ? "播放完成，可以继续。" : "录音播放失败，请点“再听一遍”重试。";
    const auto &s = field(model_.state, "session");
    if (success && audioAt_ >= 0 && str(ui_, "route") == "quiz" && str(s, "lesson") == playingLesson_ &&
        num(s, "at") == audioAt_)
        commit("heard", J::array({audioAt_}));
    audioAt_ = -1;
    ++calls_;
    return true;
}
bool NativeStudy::take_audio(std::string &path, int &serial, bool &stop, double *rate) {
    if(vocabulary_active()) return vocabulary_->take_audio(path,serial,stop,rate);
    if (pendingAudio_.is_null())
        return false;
    path = str(pendingAudio_, "path");
    serial = int(num(pendingAudio_, "serial"));
    stop = flag(pendingAudio_, "stop");
    if (rate)
        *rate = field(pendingAudio_, "rate").is_number() ? pendingAudio_["rate"].get<double>() : 1.;
    pendingAudio_ = nullptr;
    return true;
}
std::string NativeStudy::audio_text(const std::string &path) const {
    if(vocabulary_active()) return vocabulary_->audio_text(path);
    for (auto it = data()["audio"].begin(); it != data()["audio"].end(); ++it)
        if (it.value() == path)
            return it.key();
    return {};
}
bool NativeStudy::dailyNext() {
    const auto snap = model_.snapshot(profile_), n = snap["next"];
    const auto type = str(n, "type");
    const auto &s = field(model_.state, "session");
    if (type == "active-review") {
        if (commit("batchResume"))
            go("word-batch");
    } else if (type == "review") {
        const auto min = num(snap["plan"], "minutes");
        if (commit("dailyReview", J::array({n["kind"], min == 25 ? 8 : min == 15 ? 6 : 4})))
            go("word-batch");
    } else if (type == "batch" || type == "saved-batch") {
        if (commit("dailyResume"))
            go("word-batch");
    } else if (type == "course") {
        ui_["lastLesson"] = s["lesson"];
        ui_["pickedUnit"] = model_.lesson(str(s, "lesson"))["unit"];
        go("quiz");
    } else if (type == "assessment" || type == "practice")
        return false;
    else if (type == "new") {
        const auto kind = str(n, "kind");
        for (const auto &t : snap["stages"])
            if (str(t, "kind") == kind) {
                const auto target = t["target"];
                const auto limit = num(t, "readyNew", num(t, "remaining"));
                if (target.is_null() || !limit) {
                    go("path-today");
                    break;
                }
                J ids = J::array();
                for (size_t i = 0; i < std::min(size_t(limit), target["ids"].size()); ++i)
                    ids.push_back(target["ids"][i]);
                if (kind == "words") {
                    if (commit("batchBegin",
                               J::array({ids, J{{"type", target["type"]}, {"id", target["id"]}}, "learn"})))
                        go("word-batch");
                } else if (str(target, "type") == "bridge")
                    return false;
                else if (commit("dailyFocus", J::array({target["id"], kind, ids})))
                    go("quiz");
                break;
            }
    } else if (type == "waiting" && !snap["deferredTargets"].empty())
        return dispatch(J{{"action", "w-day-catchup"}}.dump());
    else
        go("review");
    return true;
}
bool NativeStudy::type(const std::string &text) {
    if(vocabulary_active()) return vocabulary_->type(text);
    if (str(ui_, "route") == "word-batch") {
        ++calls_;
        toast_.clear();
        return commit("batchType", J::array({text}));
    }
    return false;
}
bool NativeStudy::edit(const std::string &input) {
    if(vocabulary_active()) return vocabulary_->edit(input);
    if (str(ui_, "route") != "word-batch")
        return false;
    auto value = str(field(model_.state, "batch"), "typed");
    for (char c : input) {
        if (c == '\b') {
            if (!value.empty()) {
                size_t pos = value.size() - 1;
                while (pos && ((uint8_t(value[pos]) & 0xc0) == 0x80))
                    --pos;
                value.resize(pos);
            }
        } else
            value += c;
    }
    return type(value);
}
bool NativeStudy::input(const std::string &kind, const std::string &text) {
    if(vocabulary_active()) return vocabulary_->input(kind,text);
    const auto r = str(ui_, "route");
    const bool batch = r == "word-batch";
    return dispatch(J{
        {"action", std::string(batch ? "w-kbd-" : "") + (kind == "submit"      ? "search-submit"
                                                         : kind == "backspace" ? "erase"
                                                                               : kind)},
        {"value", text}}.dump());
}
bool NativeStudy::back() {
    if(vocabulary_active()) return vocabulary_->back();
    const auto r = str(ui_, "route");
    if (!field(model_.state, "lookup").is_null())
        return dispatch(J{{"action", "lookup-return"}}.dump());
    if (r == "home" && flag(ui_, "nativeHomeOptions")) {
        ui_["nativeHomeOptions"] = false;
        return true;
    }
    if (r == "word-batch")
        return dispatch(J{{"action", "w-pause"}}.dump());
    if (r == "word")
        go("search");
    else if (r == "search")
        return false;
    else if (r == "pause")
        go("quiz");
    else if (r == "word-review")
        go("review");
    else
        go("home");
    return true;
}
bool NativeStudy::dispatch(const std::string &raw) {
    if(vocabulary_active()) return vocabulary_->dispatch(raw);
    ++calls_;
    toast_.clear();
    try {
        const auto d = J::parse(raw);
        const auto a = str(d, "action"), r = str(ui_, "route");
        auto integer = [&](const char *k, int def = 0) {
            const auto &v = field(d, k);
            return v.is_string() && !str(v).empty() ? std::stoi(str(v)) : int(num(v, def));
        };
        const auto &b = field(model_.state, "batch");
        const auto &s = field(model_.state, "session");
        if ((!b.is_null() && num(b, "reviewVersion") != 1) &&
            (a == "w-day-next" || a == "w-resume" || a == "u-main" || a == "p-start" || a == "u-continue" ||
             a == "w-r-start" || a == "w-review" || a == "u-review" ||
             (a == "go" && (str(d, "route") == "word-batch" || str(d, "route") == "quiz"))))
            return false;
        if (a == "error-close") {
            error_.clear();
            return true;
        }
        if (!error_.empty())
            return false;
        if (a == "w-day-next" || a == "u-main" || a == "p-start")
            return dailyNext();
        if (a == "w-day-home") {
            ui_["nativeHomeOptions"] = false;
            go("home");
        } else if (a == "native-home-options")
            ui_["nativeHomeOptions"] = true;
        else if (a == "p-today")
            go("path-today");
        else if (a == "go") {
            const auto dest = str(d, "route");
            if (!supported(dest))
                return false;
            if (dest == "home" || dest == "review")
                ui_["nativeHomeOptions"] = false;
            go(dest);
        } else if (a == "w-mode") {
            commit("batchSetMode", J::array({d["value"]}));
        } else if (a == "w-begin") {
            if (commit("batchStart"))
                go("word-batch");
        } else if (a == "w-learn") {
            if (commit("batchLearn", J::array({integer("delta", 1)})))
                go("word-batch");
        } else if (a == "w-choose")
            commit("batchChoose", J::array({integer("n")}));
        else if (a == "w-check" || a == "w-kbd-search-submit" || a == "w-unknown") {
            if (commit("batchCheck", J::array({a == "w-unknown"})))
                go("word-batch");
        } else if (a == "w-next") {
            if (commit("batchNext"))
                go(field(model_.state, "batch").is_null() ? "word-result" : "word-batch");
        } else if (a == "w-hint")
            commit("batchHint");
        else if (a == "w-pause") {
            if (commit("batchPause"))
                go("home");
        } else if (a == "w-resume") {
            if (commit("batchResume"))
                go("word-batch");
        } else if (a == "w-audio")
            play(str(d, "text"), -1, 1, false);
        else if (a == "w-kbd-key" || a == "w-kbd-erase") {
            edit(a == "w-kbd-erase" ? "\b" : str(d, "value"));
        } else if (a == "w-kbd-shift" || a == "shift")
            ui_["shift"] = !flag(ui_, "shift");
        else if (a == "w-kbd-symbols" || a == "symbols")
            ui_["symbols"] = !flag(ui_, "symbols");
        else if (a == "choose")
            commit("choose", J::array({integer("n")}));
        else if (a == "check" || a == "unknown") {
            if (commit("check", J::array({a == "unknown"}))) {
                stopAudio();
                ui_["readPage"] = 0;
            }
        } else if (a == "next") {
            const auto lesson = str(s, "lesson");
            const bool focused = str(s, "purpose") == "focus";
            ui_["legacySummary"] = flag(s, "legacy");
            if (commit("advance")) {
                ui_["lastLesson"] = lesson;
                go(!field(model_.state, "session").is_null() ? "quiz"
                   : focused                                 ? "focus-summary"
                                                             : "summary");
            }
        } else if (a == "skip") {
            if (commit("skip"))
                stopAudio();
        } else if (a == "audio")
            play(str(d, "text"), str(d, "step").empty() ? -1 : integer("step"),
                 str(d, "rate").empty() ? 1 : std::stod(str(d, "rate")));
        else if (a == "read-page") {
            const auto page = num(ui_, "readPage") + integer("delta");
            if (commit("setPage", J::array({page})))
                ui_["readPage"] = page;
        } else if (a == "lookup" || (a == "word" && r != "search")) {
            const auto id = a == "word" ? str(d, "id") : "";
            J point{{"route", r},
                    {"activity",
                     r == "word-batch" ? str(b, "serial") + ":" + str(b, "at") + ":" + str(b, "phase") + ":" +
                                             (flag(b, "checked") ? "true" : "false")
                     : r == "quiz" ? str(s, "lesson") + ":" + str(s, "at") + ":" + str(s, "page") + ":" +
                                         (flag(s, "checked") ? "true" : "false")
                                   : ""}};
            for (const auto *k :
                 {"lastLesson", "pickedUnit", "unitPage", "readPage", "listMode", "listPage", "listLessonId"})
                point[k] = field(ui_, k);
            point["scroll"] = 0;
            const bool spelling = r == "word-batch" && str(b, "phase") == "check" &&
                                  field(b, "checked").is_null() &&
                                  str(b["queue"][size_t(num(b, "at"))], "kind") != "meaning";
            point["caret"] =
                spelling ? J::array({str(b, "typed").size(), str(b, "typed").size()}) : J(nullptr);
            if (commit("lookupOpen", J::array({point, id}))) {
                ui_["searchQuery"] = "";
                ui_["entryId"] = id;
                ui_["returnTo"] = "search";
                go(id.empty() ? "search" : "word");
            }
        } else if (a == "lookup-return") {
            const auto point = field(field(model_.state, "lookup"), "point");
            if (point.is_null())
                return false;
            if (commit("lookupClose")) {
                const auto &currentBatch = field(model_.state, "batch"),
                           &currentSession = field(model_.state, "session");
                const auto origin = str(point, "route");
                const auto activity =
                    origin == "word-batch" ? str(currentBatch, "serial") + ":" + str(currentBatch, "at") +
                                                 ":" + str(currentBatch, "phase") + ":" +
                                                 (flag(currentBatch, "checked") ? "true" : "false")
                    : origin == "quiz" ? str(currentSession, "lesson") + ":" + str(currentSession, "at") +
                                             ":" + str(currentSession, "page") + ":" +
                                             (flag(currentSession, "checked") ? "true" : "false")
                                       : "";
                if (!str(point, "activity").empty() && str(point, "activity") != activity) {
                    go("home");
                    toast_ = "任务位置已变化，请继续当前学习。";
                    return true;
                }
                for (const auto *k : {"lastLesson", "pickedUnit", "unitPage", "readPage", "listMode",
                                      "listPage", "listLessonId"})
                    ui_[k] = field(point, k);
                go(str(point, "route"));
            }
        } else if (a == "key" || a == "erase") {
            auto value = str(ui_, "searchQuery");
            if (a == "erase") {
                if (!value.empty()) {
                    size_t pos = value.size() - 1;
                    while (pos && (uint8_t(value[pos]) & 0xc0) == 0x80)
                        --pos;
                    value.resize(pos);
                }
            } else {
                value += str(d, "value");
                if (value.size() > 45)
                    value.resize(45);
            }
            ui_["searchQuery"] = value;
            if (!field(model_.state, "lookup").is_null())
                commit("lookupEdit", J::array({value}));
        } else if (a == "word") {
            ui_["entryId"] = d["id"];
            ui_["returnTo"] = "search";
            go("word");
        } else if (a == "search-submit") {
            const auto query = NativeModel::normal(str(ui_, "searchQuery"));
            bool found = false;
            for (const auto &w : data()["catalog"])
                if (NativeModel::normal(str(w, "word")) == query) {
                    ui_["entryId"] = w["id"];
                    ui_["returnTo"] = "search";
                    go("word");
                    found = true;
                    break;
                }
            if (!found)
                toast_ = "试学词库中未找到，可选择下方候选词。";
        } else if (a == "add") {
            if (commit("add", J::array({d["id"]})))
                toast_ = "已加入生词本。";
        } else if (a == "remove-word")
            commit("remove", J::array({d["id"]}));
        else if (a == "u-dictionary-input") {
            ui_["searchQuery"] = "";
            ui_["entryId"] = "";
            ui_["returnTo"] = "dictionary";
            go("search");
        } else if (a == "u-continue") {
            if (s.is_null())
                go("home");
            else {
                ui_["lastLesson"] = s["lesson"];
                ui_["pickedUnit"] = model_.lesson(str(s, "lesson"))["unit"];
                go("quiz");
            }
        } else if (a == "w-r-start" || a == "w-review" || a == "w-retry" || a == "u-review") {
            if (flag(d, "open")) {
                go("word-review");
                return true;
            }
            const auto kind = flag(d, "kind")                              ? str(d, "kind")
                              : str(model_.state, "wordMode") == "meaning" ? "meaning"
                                                                           : "all",
                       mode = flag(d, "mode") ? str(d, "mode") : "due";
            const auto min = num(field(profile_, "plan"), "minutes");
            if (commit("batchReview", J::array({mode, kind,
                                                field(profile_, "plan").is_null() ? 6
                                                : min == 10                       ? 4
                                                : min == 15                       ? 6
                                                                                  : 8})))
                go("word-batch");
        } else if (a == "w-r-page")
            ui_["reviewPage"] = num(ui_, "reviewPage") + integer("delta");
        else if (a == "w-day-catchup") {
            const auto snap = model_.snapshot(profile_);
            if (!snap["active"].is_null() || str(snap["next"], "type") == "review")
                return dailyNext();
            if (snap["deferredTargets"].empty())
                return false;
            const auto first = snap["deferredTargets"][0];
            if (str(first, "type") != "course")
                return false;
            J ids = J::array();
            for (const auto &t : snap["deferredTargets"])
                if (t["type"] == first["type"] && t["id"] == first["id"] && ids.size() < 8)
                    ids.push_back(t["question"]);
            if (commit("dailyFocus", J::array({first["id"], "listen", ids})))
                go("quiz");
        } else
            return false;
        return true;
    } catch (const std::exception &e) {
        error_ = e.what();
        return true;
    }
}
std::string NativeStudy::key() const {
    const auto &b = field(model_.state, "batch"), s = field(model_.state, "session"),
               t = field(profile_, "training"), a = field(profile_, "attempt");
    std::vector<std::string> v = {str(ui_, "route"),
                                  str(b, "serial"),
                                  str(b, "phase"),
                                  str(b, "learnAt"),
                                  str(b, "at"),
                                  flag(b, "checked") ? "true" : "false",
                                  str(s, "lesson"),
                                  str(s, "at"),
                                  str(s, "page"),
                                  str(ui_, "readPage"),
                                  flag(s, "checked") ? "true" : "false",
                                  str(t, "serial"),
                                  str(t, "at"),
                                  str(t, "page"),
                                  str(t, "stage"),
                                  flag(t, "checked") ? "true" : "false",
                                  str(a, "serial"),
                                  str(a, "at"),
                                  str(a, "page"),
                                  str(ui_, "pickedUnit"),
                                  str(ui_, "unitPage"),
                                  str(ui_, "listPage"),
                                  str(ui_, "entryId"),
                                  "false",
                                  flag(ui_, "nativeOtherBooks") ? "1" : "0",
                                  flag(ui_, "nativeHomeOptions") ? "1" : "0"};
    std::string out;
    for (const auto &s : v) {
        if (!out.empty())
            out += '|';
        out += s;
    }
    return out;
}
std::string NativeStudy::status() const {
    if(vocabulary_active()) return vocabulary_->status();
    const auto snap = model_.snapshot(profile_);
    J stages = J::array();
    for (const auto &t : snap["stages"])
        stages.push_back(J{
            {"kind", t["kind"]}, {"done", t["done"]}, {"total", t["total"]}, {"remaining", t["remaining"]}});
    const auto &b = field(model_.state, "batch"), s = field(model_.state, "session");
    J batch, session;
    if (!b.is_null()) {
        const auto q = b["queue"][size_t(num(b, "at"))];
        batch = J{{"phase", b["phase"]},
                  {"at", b["at"]},
                  {"serial", b["serial"]},
                  {"learnAt", b["learnAt"]},
                  {"id", flag(q, "id") ? q["id"] : b["ids"][size_t(num(b, "learnAt"))]},
                  {"kind", q["kind"]},
                  {"typed", b["typed"]}};
        if (!b["checked"].is_null())
            batch["checked"] = b["checked"]["correct"];
    }
    if (!s.is_null()) {
        const auto &t = model_.task(s);
        session = J{{"lesson", s["lesson"]}, {"at", s["at"]},     {"page", s["page"]},
                    {"id", t["id"]},         {"kind", t["kind"]}, {"heard", flag(s["heard"], str(s["at"]))}};
        if (!s["checked"].is_null())
            session["checked"] = s["checked"]["correct"];
    }
    return J{{"route", ui_["route"]}, {"revision", calls_},    {"selectors", 0},        {"engine", "cpp"},
             {"nativeCalls", calls_}, {"nativePages", pages_}, {"next", snap["next"]},  {"stages", stages},
             {"batch", batch},        {"session", session},    {"courseError", error_}, {"profileError", ""}}
        .dump();
}
} // namespace wordcard
