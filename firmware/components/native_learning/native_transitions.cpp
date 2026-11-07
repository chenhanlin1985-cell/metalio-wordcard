#include "native_model.hpp"
#include <algorithm>
namespace wordcard {
namespace {
J array() { return J::array(); }
J object() { return J::object(); }
void limit(J &a, size_t n) {
    if (a.size() > n)
        a.erase(a.begin(), a.end() - n);
}
void record(J &x, const std::string &kind, const std::string &id, int64_t now,
            const std::string &name = "dailyLog") {
    const auto date = NativeModel::dateKey(now);
    if (field(field(field(x, name), date), kind).is_null())
        x[name][date][kind] = object();
    if (!x[name][date][kind].contains(id))
        x[name][date][kind][id] = now;
    auto dates = keys(x[name]);
    std::sort(dates.begin(), dates.end());
    while (dates.size() > 35) {
        x[name].erase(str(dates[0]));
        dates.erase(dates.begin());
    }
}
J arg(const J &a, size_t i, const J &def = nullptr) { return a.is_array() && i < a.size() ? a[i] : def; }
J group(const J &all, const J &fresh, int lim) {
    const J &pool = fresh.empty() ? all : fresh;
    size_t n = std::min(size_t(std::min(8, std::max(2, lim))), pool.size());
    if (pool.size() - n == 1 && n > 2)
        --n;
    J r = J::array();
    for (size_t i = 0; i < n; ++i)
        r.push_back(pool[i]);
    if (r.size() == 1 && all.size() > 1)
        for (const auto &id : all)
            if (id != r[0]) {
                r.push_back(id);
                break;
            }
    return r;
}
} // namespace
void NativeModel::recordDaily(const J &before, J &x) const {
    const auto &r = field(x, "batchLast"), b = field(before, "batch");
    if (!b.is_null() && !r.is_null() && field(r, "serial") == field(b, "serial") &&
        field(field(before, "batchLast"), "serial") != field(r, "serial"))
        for (const auto &id : r["ids"]) {
            if (str(r, "mode") == "learn" && field(field(before, "batchMemory"), str(id)).is_null() &&
                field(before["memory"], str(id)).is_null())
                record(x, "words", str(id), now);
            for (const auto *k : {"meaning", "spelling"})
                if (!field(field(r["results"], str(id)), k).is_null()) {
                    const auto m = memoryRow(before, str(id), k);
                    if (!m.is_null() && num(m, "due") <= now)
                        record(x, k, str(id), now);
                }
        }
    const auto &prev = field(before, "lastPractice");
    if (str(prev, "focus") == "listen" && dateKey(num(prev, "time")) == dateKey(now))
        for (const auto &id : field(prev, "skipped"))
            record(x, "listen", "course:" + str(prev, "lesson") + ":" + str(id), num(prev, "time"),
                   "dailySkip");
    const auto &s = field(before, "session"), t = task(s);
    if (t.is_null() || (str(t, "kind") != "listen" && str(t, "kind") != "read"))
        return;
    if (field(field(x, "session"), "lesson") != field(s, "lesson") ||
        field(field(x, "session"), "at") != field(s, "at")) {
        const auto id = "course:" + str(s, "lesson") + ":" + str(t, "id"), kind = str(t, "kind");
        if (flag(field(s, "checked"), "correct")) {
            record(x, kind, id, now);
            if (kind == "listen" && !field(field(field(x, "dailySkip"), dateKey(now)), "listen").is_null())
                x["dailySkip"][dateKey(now)]["listen"].erase(id);
        } else if (kind == "listen" && flag(field(s, "checked"), "skip"))
            record(x, kind, id, now, "dailySkip");
    }
}
void NativeModel::advance(J &x) const {
    auto &s = x["session"];
    const J t = task(s);
    require(!t.is_null() && (str(t, "kind") == "learn" || !s["checked"].is_null()), "请先检查当前答案");
    const auto lessonId = str(s, "lesson"), kind = str(t, "kind"), purpose = str(s, "purpose");
    const auto ts = tasks(s);
    if (kind == "learn")
        for (const auto &id : t["wordIds"])
            if (!x["memory"].contains(str(id)))
                x["memory"][str(id)] = J{{"success", 0}, {"stage", 0}, {"due", now + StudyDay}};
    if (!s["checked"].is_null() && !flag(s["checked"], "correct") && !flag(s["checked"], "skip"))
        s["queue"].push_back(t["id"]);
    if (flag(s["checked"], "correct") && kind == "listen")
        drop(s["skipped"], t["id"]);
    x["visits"][lessonId] = num(x["visits"], lessonId) + 1;
    if (num(s, "at") + 1 < int64_t(s["queue"].size())) {
        s["at"] = num(s, "at") + 1;
        s["page"] = 0;
        s["selected"] = nullptr;
        s["checked"] = nullptr;
        return;
    }
    auto heardNow = [&] {
        J r = array();
        for (size_t i = 0; i < s["queue"].size(); ++i) {
            const auto id = s["queue"][i];
            bool listening = false;
            for (const auto &q : ts)
                if (q["id"] == id && str(q, "kind") == "listen")
                    listening = true;
            if (listening && flag(s["heard"], std::to_string(i)) && s["first"].contains(str(id)))
                addUnique(r, id);
        }
        return r;
    };
    if (purpose == "focus") {
        const auto key = lessonId + ":" + str(s, "focus");
        if (field(field(x, "focusProgress"), key).is_null())
            x["focusProgress"][key] = array();
        for (const auto &id : keys(s["first"]))
            addUnique(x["focusProgress"][key], id);
        const auto prev = flag(s, "focusCatchup") ? field(x, "lastPractice") : J(nullptr);
        J heard = field(prev, "heard").is_array() ? prev["heard"] : array();
        for (const auto &id : heardNow())
            addUnique(heard, id);
        J first = field(prev, "first").is_object() ? prev["first"] : object();
        first.update(s["first"]);
        J skipped = prev.is_null() ? s["skipped"] : prev["skipped"];
        if (!prev.is_null())
            for (const auto &id : heard)
                drop(skipped, id);
        x["lastPractice"] = J{{"lesson", lessonId}, {"focus", s["focus"]}, {"time", now},
                              {"first", first},     {"heard", heard},      {"skipped", skipped}};
        x["events"].push_back(J{{"lesson", lessonId},
                                {"time", now},
                                {"kind", "focus"},
                                {"focus", s["focus"]},
                                {"skipped", skipped.size()}});
        limit(x["events"], 300);
        s = nullptr;
        return;
    }
    const bool legacy = flag(s, "legacy"), catchup = str(s, "mode") == "catchup";
    auto &target = x[legacy ? "legacyCompleted" : "completed"];
    const auto prev = field(target, lessonId);
    J heard = catchup && field(prev, "heard").is_array() ? prev["heard"] : array();
    for (const auto &id : heardNow())
        addUnique(heard, id);
    J skipped = catchup ? (field(prev, "skipped").is_array() ? prev["skipped"] : array()) : s["skipped"];
    if (catchup)
        for (const auto &id : heard)
            drop(skipped, id);
    J first = catchup && field(prev, "first").is_object() ? prev["first"] : object();
    first.update(s["first"]);
    const auto &l = lesson(lessonId);
    target[lessonId] = J{{"time", now},
                         {"revision", legacy ? J(1) : l["revision"]},
                         {"first", first},
                         {"heard", heard},
                         {"skipped", skipped}};
    if (!legacy && skipped.empty())
        x["achievements"][lessonId] = l["revision"];
    if (!legacy && str(l, "role") == "unit-review" && skipped.empty()) {
        const auto unit = str(l["unit"]);
        const auto &r = field(x["reviewSchedule"], unit);
        if (r.is_null())
            x["reviewSchedule"][unit] = J{{"stage", 0}, {"due", now + StudyDay}};
        else if (purpose == "scheduled" && field(s, "slot") == field(r, "stage") && num(r, "due") <= now) {
            const auto stage = num(r, "stage") + 1;
            x["reviewSchedule"][unit]["stage"] = stage;
            x["reviewSchedule"][unit]["due"] =
                stage == 3 ? J(nullptr) : J(now + (stage == 1 ? 2 : 4) * StudyDay);
        }
    }
    if (legacy && num(l, "revision") == 1) {
        x["completed"][lessonId] = target[lessonId];
        x["completed"][lessonId]["revision"] = 1;
        if (skipped.empty())
            x["achievements"][lessonId] = 1;
    }
    x["events"].push_back(J{{"lesson", lessonId},
                            {"time", now},
                            {"kind", purpose},
                            {"legacy", legacy},
                            {"skipped", skipped.size()}});
    limit(x["events"], 300);
    s = nullptr;
}
bool NativeModel::mutate(J &x, const std::string &m, const J &a) {
    // ordered_json preserves JS property order for deterministic retry pools. Its
    // object uses a vector: reserve before retaining sibling references below.
    x.get_ref<J::object_t &>().reserve(x.size() + 32);
    const J before = x;
    auto &s = x["session"];
    if (m == "batchStart") {
        auto &b = x["batch"];
        require(str(b, "phase") == "overview", "本组已经开始");
        require(num(b, "trainingVersion") != 2 || flag(b, "practiceMode"), "请先选择训练方式");
        b["phase"] = str(b, "mode") == "learn" ? "learn" : "check";
    } else if (m == "batchLearn") {
        auto &b = x["batch"];
        require(str(b, "phase") == "learn", "当前不是认识阶段");
        auto at = num(b, "learnAt");
        if (num(arg(a, 0, 1)) < 0)
            b["learnAt"] = std::max(int64_t(0), at - 1);
        else {
            addUnique(b["learned"], b["ids"].at(at));
            if (at + 1 < int64_t(b["ids"].size()))
                b["learnAt"] = at + 1;
            else {
                require(b["learned"].size() == b["ids"].size());
                b["phase"] = "check";
                b["at"] = 0;
            }
        }
    } else if (m == "batchChoose") {
        auto &b = x["batch"];
        const auto n = arg(a, 0);
        require(str(b, "phase") == "check" && str(b["queue"].at(num(b, "at")), "kind") == "meaning" &&
                    b["checked"].is_null() && n.is_number_integer() && num(n) >= 0 && num(n) <= 2,
                "请选择当前题的词义");
        b["selected"] = n;
    } else if (m == "batchType") {
        auto &b = x["batch"];
        const auto kind = str(b["queue"].at(num(b, "at")), "kind");
        const auto text = arg(a, 0);
        require(str(b, "phase") == "check" && (kind == "gap" || kind == "spelling") &&
                    b["checked"].is_null() && text.is_string() && str(text).size() <= 480,
                "拼写输入无效");
        b["typed"] = text;
    } else if (m == "batchHint") {
        auto &b = x["batch"];
        const auto kind = str(b["queue"].at(num(b, "at")), "kind");
        require(b["checked"].is_null() && (kind == "gap" || kind == "spelling"), "当前不能提示");
        b["hint"] = true;
    } else if (m == "batchCheck")
        checkBatch(x["batch"], truth(arg(a, 0, false)));
    else if (m == "batchSetMode") {
        auto &b = x["batch"];
        require(str(b, "mode") != "revisit" && !flag(b, "reviewKind"), "本组开始后不能更换训练方式");
        applyMode(b, str(arg(a, 0)));
        x["wordMode"] = arg(a, 0);
    } else if (m == "batchPause" || m == "batchResume") {
        if (!field(x, "batch").is_null())
            x["batch"]["paused"] = m == "batchPause";
    } else if (m == "batchNext") {
        auto &b = x["batch"];
        if (nextBatch(b)) {
            const auto r = summarize(b);
            x["batchLast"] = r;
            if (field(x, "batchLog").is_null())
                x["batchLog"] = array();
            x["batchLog"].push_back(r);
            limit(x["batchLog"], 30);
            if (field(x, "batchMemory").is_null())
                x["batchMemory"] = object();
            auto &memory = x["batchMemory"];
            for (const auto &id : r["ids"]) {
                const auto key = str(id);
                const auto old = field(x["memory"], key);
                if (!old.is_null() && field(field(memory, key), "meaning").is_null())
                    memory[key]["meaning"] =
                        J{{"stage", num(old, "stage")}, {"due", old["due"]}, {"last", 0}};
            }
            updateMemory(memory, r);
            for (const auto &id : r["ids"]) {
                const auto key = str(id);
                if (str(word(key), "source").rfind("course:", 0) != 0)
                    continue;
                const auto v = field(field(r["results"], key), "meaning"),
                           mem = field(field(memory, key), "meaning");
                if (!mem.is_null() && (!v.is_null() || str(r, "deferredKind") == "meaning")) {
                    J old = field(x["memory"], key);
                    if (old.is_null())
                        old = object();
                    old["success"] = v.is_null() ? num(old, "success")
                                     : actual(v) ? num(old, "success") + 1
                                                 : 0;
                    old["stage"] = mem["stage"];
                    old["due"] = mem["due"];
                    x["memory"][key] = old;
                }
            }
            if (flag(b, "courseOwner") && flag(s, "unified") && field(s, "lesson") == field(b, "courseOwner"))
                for (const auto &id : r["ids"]) {
                    const auto key = str(id);
                    J values = field(s["wordResults"], key);
                    if (values.is_null())
                        values = object();
                    if (field(r["results"], key).is_object())
                        values.update(r["results"][key]);
                    s["wordResults"][key] = values;
                    addUnique(s["wordDone"], id);
                    for (const auto &t : tasks(s))
                        if (field(t, "wordId") == id) {
                            const auto v = field(field(r["results"], key), "meaning");
                            if (!v.is_null() && !s["first"].contains(str(t, "id")))
                                s["first"][str(t, "id")] = actual(v);
                            break;
                        }
                }
            b = nullptr;
        }
    } else if (m == "batchBegin") {
        if (!field(x, "batch").is_null())
            return true;
        const auto ids = arg(a, 0), source = arg(a, 1);
        const auto mode = str(arg(a, 2, "learn"));
        if (mode == "review")
            return mutate(x, "batchReview",
                          J::array({"all", str(x, "wordMode") == "meaning" ? "meaning" : "all", 8, ids}));
        x["batchSerial"] = num(x, "batchSerial") + 1;
        auto b = make(ids, source, num(x, "batchSerial"), mode);
        for (const auto &id : known(x))
            addUnique(b["retryPool"], id);
        if (flag(x, "wordMode"))
            applyMode(b, str(x, "wordMode"));
        x["batch"] = b;
    } else if (m == "batchReview" || m == "dailyReview") {
        const bool daily = m == "dailyReview";
        if (!field(x, "batch").is_null() && !daily) {
            x["batch"]["paused"] = false;
            return true;
        }
        const auto mode = daily ? "due" : str(arg(a, 0, "due")), kind = str(arg(a, daily ? 0 : 1, "all"));
        const int lim = int(num(arg(a, daily ? 1 : 2, 6)));
        const auto scope = daily ? J(nullptr) : arg(a, 3);
        require(mode == "due" || mode == "all" || mode == "notebook");
        require(kind == "all" || kind == "meaning" || kind == "spelling");
        J rows = array(), ids = array();
        for (const auto &q : reviewItems(x, kind, mode, scope))
            if (flag(q, "ready")) {
                rows.push_back(q);
                if (ids.size() < size_t(std::max(1, std::min(8, lim))))
                    addUnique(ids, q["id"]);
            }
        J targets = array();
        for (const auto &q : rows)
            if (has(ids, q["id"]) && (ids.size() != 1 || known(x).size() >= 4 || targets.empty()))
                targets.push_back(J{{"id", q["id"]}, {"kind", q["kind"]}});
        require(!targets.empty(),
                daily ? "当前没有这类到期项" : "当前没有这类待复习内容，可以先学一组新词。");
        if (daily && !field(x, "batch").is_null()) {
            require(field(x, "dailyPausedBatch").is_null(), "已有待继续背词，请先完成当前复习");
            x["dailyPausedBatch"] = x["batch"];
            x["dailyPausedBatch"]["paused"] = true;
        }
        x["batchSerial"] = num(x, "batchSerial") + 1;
        auto b = make(ids, J{{"type", "review"}, {"id", mode}}, num(x, "batchSerial"), "review");
        bool meaning = true, spelling = true;
        for (const auto &q : targets) {
            meaning &= str(q, "kind") == "meaning";
            spelling &= str(q, "kind") == "spelling";
        }
        b["reviewKind"] = meaning ? "meaning" : spelling ? "spelling" : "all";
        b["practiceMode"] = meaning ? "meaning" : "full";
        b["queue"] = array();
        for (auto q : targets) {
            q["filler"] = false;
            b["queue"].push_back(q);
        }
        b["targets"] = targets;
        for (const auto &id : known(x))
            addUnique(b["retryPool"], id);
        x["batch"] = b;
    } else if (m == "dailyResume") {
        if (field(x, "batch").is_null() && !field(x, "dailyPausedBatch").is_null()) {
            x["batch"] = x["dailyPausedBatch"];
            x.erase("dailyPausedBatch");
        }
        if (!field(x, "batch").is_null())
            x["batch"]["paused"] = false;
    } else if (m == "coursePrepare") {
        const auto ids = pending(x);
        if (ids.empty())
            return true;
        if (!field(x, "batch").is_null()) {
            require(field(x["batch"], "courseOwner") == field(s, "lesson"),
                    "请先完成或暂停当前背词组，再继续课程。");
            x["batch"]["paused"] = false;
        } else {
            x["batchSerial"] = num(x, "batchSerial") + 1;
            auto b = make(group(s["wordIds"], ids, int(num(arg(a, 0, 6)))),
                          J{{"type", "course"}, {"id", s["lesson"]}}, num(x, "batchSerial"));
            for (const auto &id : known(x))
                addUnique(b["retryPool"], id);
            b["courseOwner"] = s["lesson"];
            if (flag(x, "wordMode"))
                applyMode(b, str(x, "wordMode"));
            x["batch"] = b;
        }
    } else if (m == "dailyFocus") {
        require(s.is_null(), "先完成或明确结束当前课程。");
        const auto id = str(arg(a, 0)), kind = str(arg(a, 1));
        const auto ids = arg(a, 2);
        require((kind == "read" || kind == "listen") && ids.is_array() && !ids.empty() && ids.size() <= 8);
        J seen = array();
        for (const auto &v : ids) {
            require(!has(seen, v));
            seen.push_back(v);
            bool found = false;
            for (const auto &t : field(field(data_, "tasks"), id))
                if (t["id"] == v && str(t, "kind") == kind)
                    found = true;
            require(found, "今日专项任务无效");
        }
        s = J{{"lesson", id},
              {"legacy", false},
              {"purpose", "focus"},
              {"mode", "focus"},
              {"focus", kind},
              {"focusCatchup", false},
              {"baseLength", ids.size()},
              {"queue", ids},
              {"at", 0},
              {"page", 0},
              {"selected", nullptr},
              {"checked", nullptr},
              {"heard", object()},
              {"skipped", array()},
              {"first", object()},
              {"errors", array()}};
    } else if (m == "choose") {
        const auto &t = task(s), n = arg(a, 0);
        require(field(t, "options").is_array() && field(s, "checked").is_null() && n.is_number_integer() &&
                    num(n) >= 0 && size_t(num(n)) < t["options"].size(),
                "请在当前题选择有效答案");
        s["selected"] = n;
    } else if (m == "heard") {
        require(!s.is_null() && field(s, "at") == arg(a, 0) && str(task(s), "kind") == "listen" &&
                    s["checked"].is_null(),
                "播放状态已变化，未记为听完");
        s["heard"][str(s["at"])] = true;
    } else if (m == "setPage") {
        require(!s.is_null() && str(task(s), "kind") == "read" && num(arg(a, 0)) >= 0, "分页无效");
        s["page"] = arg(a, 0);
    } else if (m == "check") {
        const auto t = task(s);
        const bool unknown = truth(arg(a, 0, false));
        require(!t.is_null() && s["checked"].is_null() && str(t, "kind") != "learn" &&
                    (unknown || !s["selected"].is_null()) &&
                    (str(t, "kind") != "listen" || flag(s["heard"], str(s["at"]))),
                "请先完成播放并选择答案");
        const bool correct = !unknown && s["selected"] == t["answer"],
                   first = !s["first"].contains(str(t, "id"));
        s["checked"] = J{{"correct", correct}, {"unknown", unknown}};
        if (first)
            s["first"][str(t, "id")] = correct;
        if (!correct)
            addUnique(s["errors"], t["id"]);
        else
            drop(s["errors"], t["id"]);
        if (flag(t, "wordId")) {
            const auto id = str(t, "wordId");
            J mem = field(x["memory"], id);
            if (mem.is_null())
                mem = J{{"success", 0}, {"stage", 0}, {"due", now + StudyDay}};
            if (correct)
                mem["success"] = num(mem, "success") + 1;
            else
                mem["due"] = now;
            x["memory"][id] = mem;
        }
        if (field(s, "lookupAssistedAt") == field(s, "at")) {
            s["checked"]["assisted"] = true;
            if (first)
                s["first"][str(t, "id")] = false;
        }
    } else if (m == "skip") {
        const auto &t = task(s);
        require(str(t, "kind") == "listen" && s["checked"].is_null(), "当前不是可暂缓的听力题");
        addUnique(s["skipped"], t["id"]);
        s["checked"] = J{{"correct", false}, {"unknown", false}, {"skip", true}};
    } else if (m == "advance") {
        require(pending(x).empty(), "先完成本课的分组词汇，再继续听读。");
        const J prev = s;
        advance(x);
        if (flag(prev, "unified") && x["session"].is_null() &&
            field(x["completed"], str(prev, "lesson")).is_object())
            x["completed"][str(prev, "lesson")]["vocabulary"] = J{{"results", prev["wordResults"]},
                                                                  {"ids", prev["wordIds"]},
                                                                  {"completed", prev["wordDone"]},
                                                                  {"time", now}};
    } else if (m == "lookupOpen") {
        x["lookup"] = J{{"point", arg(a, 0)},
                        {"query", ""},
                        {"entry", arg(a, 1, "")},
                        {"view", truth(arg(a, 1, "")) ? "word" : "search"}};
        const auto route = str(arg(a, 0), "route");
        const auto &b = field(x, "batch");
        if (route == "word-batch" && str(b, "phase") == "check" && field(b, "checked").is_null())
            x["batch"]["hint"] = true;
        if (route == "quiz" && !s.is_null() && s["checked"].is_null())
            s["lookupAssistedAt"] = s["at"];
    } else if (m == "lookupEdit") {
        if (!field(x, "lookup").is_null()) {
            x["lookup"]["query"] = arg(a, 0);
            x["lookup"]["entry"] = arg(a, 1, "");
            x["lookup"]["view"] = arg(a, 2, "search");
        }
    } else if (m == "lookupClose")
        x["lookup"] = nullptr;
    else if (m == "add") {
        const auto id = str(arg(a, 0));
        require(!word(id).is_null(), "词条无效");
        const bool bridge = id.rfind("bridge:", 0) == 0;
        addUnique(x[bridge ? "extraNotebook" : "notebook"], id);
        if (!bridge && field(x["memory"], id).is_null())
            x["memory"][id] = J{{"success", 0}, {"stage", 0}, {"due", now}};
    } else if (m == "remove") {
        drop(x["notebook"], arg(a, 0));
        if (x.contains("extraNotebook"))
            drop(x["extraNotebook"], arg(a, 0));
        else
            x["extraNotebook"] = array();
    } else if (m == "abandon")
        s = nullptr;
    else
        return false;
    // Same merge order as UnifiedModel.commit: legacy meaning memory remains the
    // canonical due/stage when both representations exist; unknown fields survive.
    if (field(x, "batchMemory").is_object())
        for (auto it = x["batchMemory"].begin(); it != x["batchMemory"].end(); ++it) {
            const auto &mem = field(x["memory"], it.key());
            if (!mem.is_null() && !field(it.value(), "meaning").is_null()) {
                it.value()["meaning"]["due"] = mem["due"];
                it.value()["meaning"]["stage"] = num(mem, "stage");
            }
        }
    if (m == "advance" || m == "batchNext")
        recordDaily(before, x);
    return true;
}
} // namespace wordcard
