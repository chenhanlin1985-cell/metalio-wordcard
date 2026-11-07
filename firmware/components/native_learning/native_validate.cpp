#include "native_model.hpp"
#include <cmath>
namespace wordcard {
namespace {
bool integer(const J &v, int64_t min, int64_t max = INT64_MAX) {
    return v.is_number_integer() && num(v) >= min && num(v) <= max;
}
bool finite(const J &v) { return v.is_number() && std::isfinite(v.get<double>()); }
bool one(const J &v, std::initializer_list<const char *> list) {
    const auto s = str(v);
    for (auto k : list)
        if (s == k)
            return true;
    return false;
}
size_t length(const std::string &s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const auto c = uint8_t(s[i]);
        if (c < 128 || c >= 192)
            n += c >= 240 ? 2 : 1;
    }
    return n;
}
bool text(const J &v, size_t max = 120) { return v.is_string() && length(str(v)) <= max; }
bool distinct(const J &a) {
    if (!a.is_array())
        return false;
    J seen = J::array();
    for (const auto &id : a) {
        if (has(seen, id))
            return false;
        seen.push_back(id);
    }
    return true;
}
bool row(const J &r, const std::string &k) {
    return r.is_object() && field(r, "correct").is_boolean() && field(r, "assisted").is_boolean() &&
           field(r, "unknown").is_boolean() &&
           (k == "meaning" ? field(r, "typed").is_null() : text(field(r, "typed")));
}
bool results(const J &r, const J &ids, bool gap) {
    if (!r.is_object())
        return false;
    for (auto it = r.begin(); it != r.end(); ++it) {
        if (!has(ids, it.key()) || !it.value().is_object())
            return false;
        for (auto v = it.value().begin(); v != it.value().end(); ++v)
            if ((v.key() != "meaning" && v.key() != "spelling" && (!gap || v.key() != "gap")) ||
                !row(v.value(), v.key()))
                return false;
    }
    return true;
}
bool source(const NativeModel &m, const J &s) {
    const auto type = str(s, "type"), id = str(s, "id");
    if (!field(s, "id").is_string())
        return false;
    if (type == "course")
        return !m.lesson(id).is_null();
    if (type == "review")
        return id == "due" || id == "all" || id == "notebook";
    if (type == "bridge")
        for (const auto &p : m.data()["packs"])
            if (str(p, "id") == id)
                return true;
    return false;
}
bool extensions(const J &s, bool result) {
    if (s.contains("reviewVersion") && num(s, "reviewVersion") != 1)
        return false;
    if (num(s, "reviewVersion") != 1)
        return true;
    const auto &ids = field(s, "ids"), cor = field(s, "corrections"), defer = field(s, "deferredReviews");
    if (!cor.is_array() || cor.size() > 16 || !defer.is_array())
        return false;
    J seen = J::array();
    for (const auto &q : cor) {
        const auto key = str(q, "id") + "|" + str(q, "kind");
        if (!has(ids, field(q, "id")) || !one(field(q, "kind"), {"meaning", "spelling"}) ||
            !row(q, str(q, "kind")) || field(q, "afterFeedback") != true ||
            actual(field(field(field(s, "results"), str(q, "id")), str(q, "kind"))) || has(seen, key))
            return false;
        seen.push_back(key);
    }
    for (const auto &q : defer)
        if (!has(ids, field(q, "id")) || !one(field(q, "kind"), {"meaning", "spelling"}))
            return false;
    if (flag(s, "reviewKind") && !one(s["reviewKind"], {"all", "meaning", "spelling"}))
        return false;
    if (result) {
        if (flag(s, "reviewKind")) {
            if (!field(s, "targets").is_array() || s["targets"].empty())
                return false;
            for (const auto &q : s["targets"])
                if (!has(ids, field(q, "id")) || !one(field(q, "kind"), {"meaning", "spelling"}) ||
                    field(field(field(s, "results"), str(q, "id")), str(q, "kind")).is_null())
                    return false;
        }
        return true;
    }
    return true;
}
} // namespace
bool NativeModel::validateBatch(const J &s) const {
    try {
        const auto &ids = s.at("ids"), queue = s.at("queue");
        const bool modern = num(s, "trainingVersion") == 2, review = num(s, "reviewVersion") == 1;
        if (!source(*this, s.at("source")) || str(s, "version") != "word-groups-1" ||
            !integer(s.at("serial"), 1) || !one(s.at("mode"), {"learn", "review", "revisit"}) ||
            !one(s.at("phase"), {"overview", "learn", "check"}) || !s.at("paused").is_boolean() ||
            !finite(s.at("started")) || !distinct(ids) || ids.empty() || ids.size() > 8 ||
            !integer(s.at("learnAt"), 0, int64_t(ids.size()) - 1) || !distinct(s.at("learned")) ||
            !queue.is_array() || queue.empty() || queue.size() > (review ? 128 : 48) ||
            !integer(s.at("at"), 0, int64_t(queue.size()) - 1) || !text(s.at("typed")) ||
            !s.at("hint").is_boolean() || (!s.at("selected").is_null() && !integer(s.at("selected"), 0, 2)) ||
            !results(s.at("results"), ids, modern) || !extensions(s, false))
            return false;
        for (const auto &id : ids)
            if (word(str(id)).is_null())
                return false;
        for (const auto &id : s["learned"])
            if (!has(ids, id))
                return false;
        for (const auto &q : queue)
            if (!has(ids, field(q, "id")) ||
                !one(field(q, "kind"), modern
                                           ? std::initializer_list<const char *>{"meaning", "spelling", "gap"}
                                           : std::initializer_list<const char *>{"meaning", "spelling"}) ||
                !field(q, "filler").is_boolean() ||
                (review && q.contains("followup") && (q["followup"] != true || flag(q, "filler"))))
                return false;
        if (modern) {
            if (s.at("practiceMode").is_null() && str(s, "phase") != "overview")
                return false;
            if (!s["practiceMode"].is_null() && !one(s["practiceMode"], {"staged", "full", "meaning"}))
                return false;
            for (const auto &id : ids)
                if (field(field(s, "gaps"), str(id)) != gap(str(id), num(s, "serial")))
                    return false;
        }
        if (str(s, "mode") == "learn" && str(s, "phase") == "check" && s["learned"].size() != ids.size())
            return false;
        const auto &q = queue[size_t(num(s, "at"))];
        if (flag(s, "deferredKind") &&
            !(str(s, "mode") == "review" && ids.size() == 1 && queue.size() == 1 &&
              s["deferredKind"] != q["kind"] && one(s["deferredKind"], {"meaning", "spelling"})))
            return false;
        for (const auto &id : ids) {
            const auto &c = s.at("choices").at(str(id));
            if (!distinct(c.at("options")) || c["options"].size() != 3 || !integer(c.at("answer"), 0, 2) ||
                c["options"][size_t(num(c, "answer"))] != word(str(id))["meaning"])
                return false;
        }
        if (!s.at("checked").is_null()) {
            const auto &r = s["checked"];
            if (!field(r, "correct").is_boolean() || !field(r, "assisted").is_boolean() ||
                !field(r, "unknown").is_boolean() || str(s, "phase") != "check")
                return false;
            const auto id = str(q, "id"), kind = str(q, "kind");
            const bool correct = !flag(r, "unknown") &&
                                 (kind == "meaning" ? s["selected"] == s["choices"][id]["answer"]
                                                    : normal(str(s, "typed")) ==
                                                          (kind == "gap" ? str(s["gaps"][id], "answer")
                                                                         : normal(str(word(id), "word"))));
            if (flag(r, "correct") != correct || r["assisted"] != s["hint"])
                return false;
        }
        if (review) {
            if (!distinct(s.at("targetIds")) || s["targetIds"].empty() || !s.at("retryPool").is_array() ||
                s["retryPool"].size() > data_["catalog"].size() || !s.at("retryScheduled").is_array() ||
                s["retryScheduled"].size() > 16)
                return false;
            for (const auto &id : s["targetIds"])
                if (!has(ids, id))
                    return false;
            for (const auto &id : s["retryPool"])
                if (word(str(id)).is_null())
                    return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}
bool NativeModel::validateResult(const J &r) const {
    try {
        const auto &ids = r.at("ids");
        const bool modern = num(r, "trainingVersion") == 2, review = num(r, "reviewVersion") == 1;
        if (!source(*this, r.at("source")) || str(r, "version") != "word-groups-1" ||
            !one(r.at("mode"), {"learn", "review", "revisit"}) || !finite(r.at("time")) || !distinct(ids) ||
            ids.empty() || ids.size() > 8 || !results(r.at("results"), ids, modern) || !extensions(r, true))
            return false;
        for (const auto &id : ids)
            if (word(str(id)).is_null())
                return false;
        if (modern && (!one(r.at("practiceMode"), {"staged", "full", "meaning"}) ||
                       !r.at("serial").is_number_integer()))
            return false;
        const bool partial = str(r, "mode") == "revisit" || (review && flag(r, "reviewKind"));
        if (!partial)
            for (const auto &id : ids) {
                const auto &v = field(r["results"], str(id));
                if (field(v, "meaning").is_null() && str(r, "deferredKind") != "meaning")
                    return false;
                if ((!modern || str(r, "practiceMode") != "meaning") && field(v, "spelling").is_null() &&
                    str(r, "deferredKind") != "spelling")
                    return false;
                if (modern && str(r, "practiceMode") == "staged" && ids.size() > 1 &&
                    field(v, "gap").is_null())
                    return false;
                if (modern && str(r, "practiceMode") == "meaning" &&
                    (!field(v, "gap").is_null() || !field(v, "spelling").is_null()))
                    return false;
            }
        return true;
    } catch (...) {
        return false;
    }
}
bool NativeModel::validate(const J &x) const {
    try {
        if (x.at("schema") != 2 || x.at("content") != data_["version"] ||
            !one(x.at("selectedBook"), {"empower-a2", "english-file", "new-concept"}) ||
            !integer(x.at("selectedUnit"), 1, 12))
            return false;
        for (const auto *k :
             {"completed", "legacyCompleted", "achievements", "memory", "reviewSchedule", "visits"})
            if (!x.at(k).is_object())
                return false;
        if (!distinct(x.at("notebook")) || !x.at("events").is_array())
            return false;
        for (const auto &id : x["notebook"])
            if (word(str(id)).is_null() || str(word(str(id)), "source").rfind("course:", 0) != 0)
                return false;
        for (auto it = x["achievements"].begin(); it != x["achievements"].end(); ++it)
            if (lesson(it.key()).is_null() || field(lesson(it.key()), "revision") != it.value())
                return false;
        for (auto it = x["memory"].begin(); it != x["memory"].end(); ++it) {
            const auto &m = it.value();
            if (word(it.key()).is_null() || str(word(it.key()), "source").rfind("course:", 0) != 0 ||
                !finite(field(m, "due")) || !integer(field(m, "success"), 0) ||
                (m.contains("stage") && !integer(m["stage"], 0, 3)))
                return false;
        }
        auto taskExists = [&](const std::string &lessonId, const J &id, const std::string &kind,
                              bool legacy = false) {
            const auto &ts = field(field(data_, legacy ? "legacyTasks" : "tasks"), lessonId);
            for (const auto &t : ts)
                if (t["id"] == id &&
                    (kind.empty() || kind == "answer" ? kind.empty() || str(t, "kind") != "learn"
                                                      : str(t, "kind") == kind))
                    return true;
            return false;
        };
        for (const auto *name : {"completed", "legacyCompleted"}) {
            const bool legacy = std::string(name) == "legacyCompleted";
            for (auto it = x[name].begin(); it != x[name].end(); ++it) {
                const auto &c = it.value();
                if (lesson(it.key()).is_null() || !finite(field(c, "time")) ||
                    !field(c, "skipped").is_array() || !field(c, "heard").is_array() ||
                    !field(c, "first").is_object() ||
                    (!legacy && field(c, "revision") != field(lesson(it.key()), "revision")))
                    return false;
                for (const auto *k : {"skipped", "heard"})
                    for (const auto &id : c[k])
                        if (!taskExists(it.key(), id, "listen", legacy))
                            return false;
                for (auto q = c["first"].begin(); q != c["first"].end(); ++q)
                    if (!q.value().is_boolean() || !taskExists(it.key(), q.key(), "answer", legacy))
                        return false;
            }
        }
        for (auto it = x["reviewSchedule"].begin(); it != x["reviewSchedule"].end(); ++it) {
            bool exists = false;
            for (const auto &l : data_["lessons"])
                if (str(l["unit"]) == it.key() && str(l, "role") == "unit-review")
                    exists = true;
            const auto &r = it.value();
            if (!exists || !integer(field(r, "stage"), 0, 3) ||
                (num(r, "stage") == 3 ? !field(r, "due").is_null() : !finite(field(r, "due"))))
                return false;
        }
        if (!field(x, "focusProgress").is_null()) {
            if (!x["focusProgress"].is_object())
                return false;
            for (auto it = x["focusProgress"].begin(); it != x["focusProgress"].end(); ++it) {
                const auto pos = it.key().find(':');
                if (pos == std::string::npos || !distinct(it.value()))
                    return false;
                const auto id = it.key().substr(0, pos), kind = it.key().substr(pos + 1);
                if (lesson(id).is_null() || (kind != "words" && kind != "read" && kind != "listen"))
                    return false;
                for (const auto &q : it.value())
                    if (!taskExists(id, q, kind == "words" ? "meaning" : kind))
                        return false;
            }
        }
        if (!field(x, "lastPractice").is_null()) {
            const auto &p = x["lastPractice"];
            if (lesson(str(p, "lesson")).is_null() || !one(field(p, "focus"), {"words", "read", "listen"}) ||
                !finite(field(p, "time")) || !field(p, "first").is_object() ||
                !field(p, "heard").is_array() || !field(p, "skipped").is_array())
                return false;
            for (auto it = p["first"].begin(); it != p["first"].end(); ++it)
                if (!it.value().is_boolean() || !taskExists(str(p, "lesson"), it.key(), "answer"))
                    return false;
            for (const auto *k : {"heard", "skipped"})
                for (const auto &id : p[k])
                    if (!taskExists(str(p, "lesson"), id, "listen"))
                        return false;
        }
        const auto &s = x.at("session");
        if (!s.is_null()) {
            const auto id = str(s, "lesson");
            const bool legacy = flag(s, "legacy");
            if (lesson(id).is_null() || !s.at("queue").is_array() || s["queue"].empty() ||
                s["queue"].size() > 500 || !integer(s.at("at"), 0, int64_t(s["queue"].size()) - 1) ||
                !integer(s.at("page"), 0, 50) || !s.at("heard").is_object() || !s.at("first").is_object() ||
                !s.at("skipped").is_array() || !s.at("errors").is_array() ||
                !one(s.at("purpose"), {"learn", "catchup", "practice", "scheduled", "focus"}))
                return false;
            for (const auto &q : s["queue"])
                if (!taskExists(id, q, "", legacy))
                    return false;
            for (const auto &q : s["errors"])
                if (!taskExists(id, q, "answer", legacy))
                    return false;
            for (const auto &q : s["skipped"])
                if (!taskExists(id, q, "listen", legacy))
                    return false;
            for (auto it = s["heard"].begin(); it != s["heard"].end(); ++it) {
                const auto n = std::stoll(it.key());
                if (n < 0 || size_t(n) >= s["queue"].size() || it.value() != true ||
                    !taskExists(id, s["queue"][size_t(n)], "listen", legacy))
                    return false;
            }
            const auto &t = task(s);
            if (!s.at("selected").is_null() &&
                (!t.contains("options") || !integer(s["selected"], 0, int64_t(t["options"].size()) - 1)))
                return false;
            if (!s.at("checked").is_null() &&
                (!field(s["checked"], "correct").is_boolean() ||
                 !field(s["checked"], "unknown").is_boolean() || str(t, "kind") == "learn"))
                return false;
            for (auto it = s["first"].begin(); it != s["first"].end(); ++it)
                if (!it.value().is_boolean() || !taskExists(id, it.key(), "answer", legacy))
                    return false;
            if (str(s, "purpose") == "focus") {
                if (!one(field(s, "focus"), {"words", "read", "listen"}) || legacy ||
                    str(s, "mode") != "focus" || !integer(field(s, "baseLength"), 1, s["queue"].size()))
                    return false;
                for (const auto &q : s["queue"]) {
                    if (str(s, "focus") != "words" && !taskExists(id, q, str(s, "focus")))
                        return false;
                }
            }
            if (flag(s, "unified")) {
                if (!field(s, "wordDone").is_array() || !field(s, "wordIds").is_array() ||
                    !field(s, "wordResults").is_object())
                    return false;
                for (const auto &q : s["wordIds"])
                    if (word(str(q)).is_null())
                        return false;
                for (const auto &q : s["wordDone"])
                    if (!has(s["wordIds"], q))
                        return false;
            }
        }
        if (flag(x, "wordMode") && !one(x["wordMode"], {"staged", "full", "meaning"}))
            return false;
        for (const auto *k : {"batch", "dailyPausedBatch"})
            if (!field(x, k).is_null() && !validateBatch(x[k]))
                return false;
        if (!field(x, "batchLast").is_null() && !validateResult(x["batchLast"]))
            return false;
        if (!field(x, "batchLog").is_null()) {
            if (!x["batchLog"].is_array() || x["batchLog"].size() > 30)
                return false;
            for (const auto &r : x["batchLog"])
                if (!validateResult(r))
                    return false;
        }
        if (!field(x, "batchMemory").is_null()) {
            if (!x["batchMemory"].is_object())
                return false;
            for (auto it = x["batchMemory"].begin(); it != x["batchMemory"].end(); ++it) {
                if (word(it.key()).is_null() || !it.value().is_object())
                    return false;
                for (auto r = it.value().begin(); r != it.value().end(); ++r) {
                    const auto &v = r.value();
                    if ((r.key() != "meaning" && r.key() != "spelling") ||
                        !integer(field(v, "stage"), 0, 3) || !finite(field(v, "due")) ||
                        !finite(field(v, "last")))
                        return false;
                    if (v.contains("reviewPolicy") &&
                        (v["reviewPolicy"] != 1 || !integer(field(v, "independent"), 0) ||
                         !integer(field(v, "errors"), 0) || !integer(field(v, "corrections"), 0) ||
                         !field(v, "needsCorrection").is_boolean()))
                        return false;
                }
            }
        }
        if (!field(x, "extraNotebook").is_null()) {
            if (!distinct(x["extraNotebook"]))
                return false;
            for (const auto &id : x["extraNotebook"])
                if (word(str(id)).is_null() || str(id).rfind("bridge:", 0) != 0)
                    return false;
        }
        if (!field(x, "lookup").is_null()) {
            const auto &l = x["lookup"], p = field(l, "point");
            if (!one(field(p, "route"), {"quiz", "word-batch", "path-practice", "word-list", "lesson",
                                         "word-result", "home", "review", "unit-recall"}) ||
                !one(field(l, "view"), {"search", "word"}) || !text(field(l, "query")) ||
                (flag(l, "entry") && word(str(l, "entry")).is_null()) || !finite(field(p, "scroll")))
                return false;
        }
        for (const auto *name : {"dailyLog", "dailySkip"})
            if (!field(x, name).is_null()) {
                const auto &log = x[name];
                if (!log.is_object() || log.size() > 35)
                    return false;
                for (auto d = log.begin(); d != log.end(); ++d) {
                    if (d.key().size() != 10 || !d.value().is_object())
                        return false;
                    for (auto k = d.value().begin(); k != d.value().end(); ++k) {
                        if (!one(k.key(), {"meaning", "spelling", "words", "listen", "read"}) ||
                            !k.value().is_object() || k.value().size() > 300)
                            return false;
                        for (auto r = k.value().begin(); r != k.value().end(); ++r) {
                            if (!finite(r.value()) || dateKey(num(r.value())) != d.key())
                                return false;
                            if (k.key() == "words" || k.key() == "meaning" || k.key() == "spelling") {
                                if (word(r.key()).is_null())
                                    return false;
                            } else if (r.key().size() >= 160 ||
                                       (r.key().rfind("course:", 0) != 0 && r.key().rfind("bridge:", 0) != 0))
                                return false;
                        }
                    }
                }
            }
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace wordcard
