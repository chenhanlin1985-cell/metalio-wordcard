#include "native_model.hpp"
#include <algorithm>
namespace wordcard {
namespace {
J array() { return J::array(); }
J object() { return J::object(); }
int64_t dayIndex(int64_t time) { return (time + 28800000) / StudyDay; }
std::string label(const std::string &k) {
    return k == "meaning"    ? "词义复习"
           : k == "spelling" ? "拼写复习"
           : k == "words"    ? "新词"
           : k == "listen"   ? "听力"
                             : "阅读";
}
const J &pack(const J &data, const std::string &id) {
    static const J nil;
    for (const auto &p : data["packs"])
        if (str(p, "id") == id)
            return p;
    return nil;
}
} // namespace
J NativeModel::snapshot(const J &profile) const {
    const auto &x = state;
    const auto date = dateKey(now);
    const auto &savedPlan = field(profile, "plan");
    const J plan =
        savedPlan.is_null()
            ? J{{"goal", "all"}, {"minutes", 10}, {"days", 7}, {"context", "daily"}, {"lesson", "u1a"}}
            : savedPlan;
    bool study = true;
    if (!savedPlan.is_null()) {
        auto n = dayIndex(now) - dayIndex(num(plan, "time"));
        const auto d = num(plan, "days");
        study = n >= 0 &&
                (d == 7 || (d == 5 && (n % 7 == 0 || n % 7 == 1 || n % 7 == 2 || n % 7 == 4 || n % 7 == 6)) ||
                 (d == 3 && (n % 7 == 0 || n % 7 == 2 || n % 7 == 4)));
    }
    auto merged = [&](const std::string &kind) {
        J found = object();
        auto add = [&](const std::string &id, int64_t time) {
            if (dateKey(time) == date)
                found[id] = time;
        };
        J logs = field(x, "batchLog").is_array() ? x["batchLog"] : array();
        if (!field(x, "batchLast").is_null())
            logs.push_back(x["batchLast"]);
        for (const auto &r : logs) {
            if (kind == "words" && str(r, "mode") == "learn") {
                const auto &recycle = field(lesson(str(r, "courseOwner")), "recycle");
                for (const auto &id : r["ids"])
                    if (!has(recycle, id))
                        add(str(id), num(r, "time"));
            } else if ((kind == "meaning" || kind == "spelling") && str(r["source"], "type") == "review" &&
                       str(r["source"], "id") == "due")
                for (const auto &id : r["ids"])
                    if (!field(field(r["results"], str(id)), kind).is_null())
                        add(str(id), num(r, "time"));
        }
        if (kind == "read" || kind == "listen") {
            J completed = array();
            for (auto it = x["completed"].begin(); it != x["completed"].end(); ++it) {
                J r = it.value();
                r["lesson"] = it.key();
                completed.push_back(r);
            }
            if (!field(x, "lastPractice").is_null())
                completed.push_back(x["lastPractice"]);
            for (const auto &r : completed)
                for (const auto &id : keys(r["first"]))
                    for (const auto &t : field(data_["tasks"], str(r, "lesson")))
                        if (t["id"] == id && str(t, "kind") == kind && !has(field(r, "skipped"), id) &&
                            (kind != "listen" || has(field(r, "heard"), id)))
                            add("course:" + str(r, "lesson") + ":" + str(id), num(r, "time"));
            for (const auto &r : field(profile, "practiceLog"))
                if (str(r, "skill") == kind)
                    for (const auto &id : keys(field(r, "first")))
                        if (!has(field(r, "skipped"), id))
                            add("bridge:" + str(r, "pack") + ":" + str(id), num(r, "time"));
        }
        for (const auto *statePtr : {&x, &profile}) {
            const auto &entries = field(field(field(*statePtr, "dailyLog"), date), kind);
            if (entries.is_object())
                found.update(entries);
        }
        return found;
    };
    J deferredMap = object();
    for (const auto *p : {&x, &profile}) {
        const auto &map = field(field(field(*p, "dailySkip"), date), "listen");
        if (map.is_object())
            deferredMap.update(map);
    }
    const auto &last = field(x, "lastPractice");
    if (str(last, "focus") == "listen" && dateKey(num(last, "time")) == date)
        for (const auto &id : last["skipped"])
            deferredMap["course:" + str(last, "lesson") + ":" + str(id)] = last["time"];
    for (const auto &r : field(profile, "practiceLog"))
        if (str(r, "skill") == "listen" && dateKey(num(r, "time")) == date)
            for (const auto &id : r["skipped"])
                deferredMap["bridge:" + str(r, "pack") + ":" + str(id)] = r["time"];
    const auto heard = merged("listen");
    J later = array();
    for (const auto &key : keys(deferredMap)) {
        if (flag(heard, str(key)))
            continue;
        for (const auto &l : data_["lessons"])
            for (const auto &t : field(data_["tasks"], str(l, "id")))
                if (str(t, "kind") == "listen" && str(key) == "course:" + str(l, "id") + ":" + str(t, "id"))
                    later.push_back(
                        J{{"key", key}, {"type", "course"}, {"id", l["id"]}, {"question", t["id"]}});
        for (const auto &p : data_["packs"])
            for (const auto &q : field(field(data_["bridgeTasks"], str(p, "id")), "listen"))
                if (str(key) == "bridge:" + str(p, "id") + ":" + str(q, "id"))
                    later.push_back(
                        J{{"key", key}, {"type", "bridge"}, {"id", p["id"]}, {"question", q["id"]}});
    }
    const auto knownIds = known(x);
    auto choose = [&](const std::string &kind, const J &done, const J &skip) -> J {
        J target = field(field(plan, "targets"), kind);
        if (target.is_null())
            target = J{
                {"type", "course"}, {"id", flag(plan, "lesson") ? plan["lesson"] : J("u1a")}, {"kind", kind}};
        if (str(plan, "context") == "mixed" && (dayIndex(now) - dayIndex(num(plan, "time", now))) % 2 &&
            str(target, "type") == "bridge") {
            auto id = str(target, "id");
            const auto at = id.find("-daily");
            if (at != std::string::npos)
                id.replace(at, 6, "-tech");
            target["id"] = id;
        }
        if (str(target, "type") == "bridge") {
            const auto id = str(target, "id");
            const auto &p = pack(data_, id);
            if (p.is_null())
                return nullptr;
            J ids = array();
            if (kind == "words") {
                for (const auto &w : data_["catalog"])
                    if (str(w, "source") == "bridge:" + id && !has(knownIds, w["id"]))
                        ids.push_back(w["id"]);
            } else {
                const auto &used = field(field(profile, "practiceDone"), id + ":" + kind);
                for (const auto &q : field(field(data_["bridgeTasks"], id), kind)) {
                    const auto key = "bridge:" + id + ":" + str(q, "id");
                    if (!has(used, q["id"]) && !flag(done, key) && !has(skip, key))
                        ids.push_back(q["id"]);
                }
            }
            if (ids.empty())
                return nullptr;
            target["ids"] = ids;
            target["title"] = p["title"];
            return target;
        }
        J lessons = array();
        for (const auto &l : data_["lessons"])
            if (str(l, "role") == "lesson" && (str(plan, "context") != "daily" || str(l, "id") != "u3b"))
                lessons.push_back(J{{"id", l["id"]}, {"unit", l["unit"]}, {"code", l["code"]}});
        std::stable_sort(lessons.begin(), lessons.end(), [](const J &a, const J &b) {
            return a["unit"] != b["unit"] ? num(a, "unit") < num(b, "unit") : str(a, "code") < str(b, "code");
        });
        size_t start = 0;
        for (size_t i = 0; i < lessons.size(); ++i)
            if (lessons[i]["id"] == target["id"]) {
                start = i;
                break;
            }
        J first;
        size_t available = 0;
        for (size_t i = start; i < lessons.size(); ++i) {
            const auto id = str(lessons[i], "id");
            const auto &l = lesson(id);
            J ids = array();
            if (kind == "words") {
                for (const auto &w : l["words"])
                    if (!has(knownIds, w["id"]))
                        ids.push_back(w["id"]);
            } else if (!complete(x, id)) {
                const auto &used = field(field(x, "focusProgress"), id + ":" + kind);
                J queue = array();
                for (const auto &t : field(data_["tasks"], id))
                    if (str(t, "kind") == kind && !has(used, t["id"]) && queue.size() < 24)
                        queue.push_back(t["id"]);
                for (const auto &q : queue) {
                    const auto key = "course:" + id + ":" + str(q);
                    if (!flag(done, key) && !has(skip, key))
                        ids.push_back(q);
                }
            }
            if (ids.empty())
                continue;
            available += ids.size();
            if (first.is_null())
                first = J{{"type", "course"},
                          {"id", id},
                          {"kind", kind},
                          {"ids", ids},
                          {"title", str(l, "code") + " " + str(l, "title")}};
        }
        if (!first.is_null())
            first["available"] = available;
        return first;
    };
    J stages = array();
    for (const auto *kind : {"meaning", "spelling"}) {
        const auto items = reviewItems(x, kind);
        J ready = array(), cooling = array(), pending = array();
        int64_t nextDue = INT64_MAX;
        const int64_t end = (dayIndex(now) + 1) * StudyDay - 28800000 - 1;
        for (const auto &q : items) {
            if (flag(q, "ready"))
                ready.push_back(q);
            else if (num(q, "due") <= end &&
                     (flag(q["memory"], "needsCorrection") || flag(q["memory"], "pending") ||
                      (str(field(x, "batchLast"), "deferredKind") == kind &&
                          dateKey(num(field(x, "batchLast"), "time")) == date))) {
                cooling.push_back(q);
                nextDue = std::min(nextDue, num(q, "due"));
            }
        }
        for (const auto &q : ready)
            addUnique(pending, q["id"]);
        for (const auto &q : cooling)
            addUnique(pending, q["id"]);
        size_t done = 0;
        for (const auto &id : keys(merged(kind)))
            if (!has(pending, id))
                ++done;
        stages.push_back(J{{"kind", kind},
                           {"label", label(kind)},
                           {"done", done},
                           {"total", done + pending.size()},
                           {"remaining", pending.size()},
                           {"ready", ready.size()},
                           {"cooling", cooling.size()},
                           {"nextDue", cooling.empty() ? J(nullptr) : J(nextDue)}});
    }
    for (const auto *kind : {"words", "listen", "read"}) {
        const auto done = merged(kind);
        int64_t n = done.size();
        J skip = array();
        if (std::string(kind) == "listen")
            for (const auto &t : later)
                skip.push_back(t["key"]);
        const auto target = choose(kind, done, skip);
        const bool priority = str(plan, "goal") == "all" || str(plan, "goal") == kind;
        const auto minutes = num(plan, "minutes");
        int64_t dose = std::string(kind) == "words" ? (priority ? (minutes == 10   ? 4
                                                                   : minutes == 15 ? 6
                                                                                   : 8)
                                                                : 2)
                                                    : (priority ? (minutes == 25 ? 3 : 2) : 1),
                amount = study ? dose : 0;
        const auto held = std::min(std::max(int64_t(0), amount - n), int64_t(skip.size()));
        const auto capacity = target.is_null()               ? 0
                              : target.contains("available") ? num(target, "available")
                                                             : int64_t(target["ids"].size());
        const auto ready = std::min(std::max(int64_t(0), amount - n - held), capacity);
        stages.push_back(J{{"kind", kind},
                           {"label", label(kind)},
                           {"done", n},
                           {"total", n + ready + held},
                           {"remaining", ready + held},
                           {"readyNew", ready},
                           {"deferred", held},
                           {"target", target},
                           {"unavailable", study && n < amount && target.is_null() && !held}});
    }
    J active;
    const auto &b = field(x, "batch"), paused = field(x, "dailyPausedBatch"), s = field(x, "session"),
               training = field(profile, "training"), attempt = field(profile, "attempt");
    if (!b.is_null())
        active = J{{"type", str(b["source"], "type") == "review" ? "review" : "batch"},
                   {"title", sourceTitle(b["source"])},
                   {"answered", b["at"]},
                   {"total", b["queue"].size()}};
    else if (!paused.is_null())
        active = J{{"type", "saved-batch"},
                   {"title", sourceTitle(paused["source"])},
                   {"answered", paused["at"]},
                   {"total", paused["queue"].size()}};
    else if (!s.is_null()) {
        const auto &l = lesson(str(s, "lesson"));
        active = J{{"type", "course"},
                   {"title", str(l, "code") + " " + str(l, "title")},
                   {"answered", s["at"]},
                   {"total", s["queue"].size()}};
    } else if (!training.is_null())
        active = J{{"type", "practice"},
                   {"title", field(pack(data_, str(training, "pack")), "title")},
                   {"answered", training["at"]},
                   {"total", training["ids"].size()}};
    else if (!attempt.is_null())
        active = J{{"type", "assessment"},
                   {"title", "已保存的起点摸底"},
                   {"answered", attempt["at"]},
                   {"total", attempt["queue"].size()}};
    J due, newItem;
    bool waiting = false, unavailable = false;
    int64_t remaining = 0, done = 0, cooling = 0;
    for (const auto &t : stages) {
        if (due.is_null() && num(t, "ready") > 0)
            due = t;
        if (newItem.is_null() && num(t, "readyNew") > 0)
            newItem = t;
        waiting |= num(t, "cooling") || num(t, "deferred");
        unavailable |= flag(t, "unavailable");
        remaining += num(t, "remaining");
        done += num(t, "done");
        cooling += num(t, "cooling");
    }
    J next = str(active, "type") == "review" ? J{{"type", "active-review"}}
             : !due.is_null()                ? J{{"type", "review"}, {"kind", due["kind"]}}
             : !active.is_null()             ? J{{"type", active["type"]}}
             : !newItem.is_null()            ? J{{"type", "new"}, {"kind", newItem["kind"]}}
                                             : J{{"type", waiting       ? "waiting"
                                                          : unavailable ? "available-end"
                                                                        : "done"}};
    return J{{"date", date},
             {"plan", plan},
             {"provisional", savedPlan.is_null()},
             {"study", study},
             {"stages", stages},
             {"active", active},
             {"next", next},
             {"deferredTargets", later},
             {"remaining", remaining},
             {"done", done},
             {"cooling", cooling}};
}
std::string NativeModel::nextLabel(const J &s) {
    const auto &n = s["next"];
    const auto type = str(n, "type"), kind = str(n, "kind");
    if (type == "active-review")
        return "继续已保存的复习";
    if (type == "review")
        return "先复习" + std::string(kind == "meaning" ? "词义" : "完整拼写");
    if (type == "assessment")
        return "继续未完成摸底";
    if (type == "batch" || type == "saved-batch" || type == "course" || type == "practice")
        return "继续已保存的学习";
    if (type == "new")
        return "开始" + label(kind);
    if (type == "waiting")
        return s["deferredTargets"].empty() ? "查看稍后复习" : "继续补听暂缓题";
    return "查看复习安排";
}
} // namespace wordcard
