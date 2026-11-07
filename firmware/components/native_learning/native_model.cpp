#include "native_model.hpp"
#include "../learning_web/vendor/libunicode.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>
namespace wordcard {
namespace {
const J nil;
J arr() { return J::array(); }
J obj() { return J::object(); }
std::vector<uint32_t> points(const std::string &s) {
    std::vector<uint32_t> r;
    for (size_t i = 0; i < s.size();) {
        uint32_t c = (unsigned char)s[i++];
        if (c >= 0xc0) {
            int n = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
            c &= (1 << (6 - n)) - 1;
            while (n-- && i < s.size())
                c = (c << 6) | ((unsigned char)s[i++] & 63);
        }
        r.push_back(c);
    }
    return r;
}
void utf8(std::string &r, uint32_t c) {
    if (c < 128)
        r += char(c);
    else if (c < 2048) {
        r += char(0xc0 | (c >> 6));
        r += char(0x80 | (c & 63));
    } else if (c < 65536) {
        r += char(0xe0 | (c >> 12));
        r += char(0x80 | ((c >> 6) & 63));
        r += char(0x80 | (c & 63));
    } else {
        r += char(0xf0 | (c >> 18));
        r += char(0x80 | ((c >> 12) & 63));
        r += char(0x80 | ((c >> 6) & 63));
        r += char(0x80 | (c & 63));
    }
}
uint32_t hash(const std::string &s) {
    uint32_t h = 2166136261u;
    for (auto c : points(s)) {
        if (c > 0xffff)
            c = 0xd800 + ((c - 0x10000) >> 10);
        h = (h ^ c) * 16777619u;
    }
    return h;
}
J order(J a, const std::string &seed, const std::string &avoid = "") {
    const J original = a;
    std::stable_sort(a.begin(), a.end(), [&](const J &x, const J &y) {
        const auto hx = hash(seed + ":" + str(x)), hy = hash(seed + ":" + str(y));
        return hx != hy ? hx < hy : str(x) < str(y);
    });
    auto rotate = [&] {
        if (a.size() > 1)
            std::rotate(a.begin(), a.begin() + 1, a.end());
    };
    if (a.size() > 1 && a == original)
        rotate();
    if (a.size() > 1 && str(a[0]) == avoid)
        rotate();
    return a;
}
std::string gloss(const std::string &s) {
    std::string r;
    int count = 0;
    for (auto c : points(s)) {
        if (c == ';' || c == ',' || c == '(' || c == ':' || c == 0xff1b || c == 0xff0c || c == 0xff08 ||
            c == 0xff1a)
            break;
        if (!count && lre_is_space(c))
            continue;
        if (count++ == 2)
            break;
        utf8(r, c);
    }
    return r;
}
J rotate(J a, const std::string &avoid, const J &previous) {
    for (size_t i = 0; i < a.size(); ++i) {
        if (str(a[0]) != avoid && (a.size() < 3 || a != previous))
            break;
        std::rotate(a.begin(), a.begin() + 1, a.end());
    }
    return a;
}
J unique(J a) {
    J r = arr();
    for (const auto &v : a)
        addUnique(r, v);
    return r;
}
void cap(J &a, size_t n) {
    if (a.size() > n)
        a.erase(a.begin(), a.end() - n);
}
void credit(J &x, const std::string &kind, const std::string &id, int64_t now,
            const char *fieldName = "dailyLog") {
    const auto d = NativeModel::dateKey(now);
    if (field(field(field(x, fieldName), d), kind).is_null())
        x[fieldName][d][kind] = obj();
    if (!x[fieldName][d][kind].contains(id))
        x[fieldName][d][kind][id] = now;
    auto dates = keys(x[fieldName]);
    std::sort(dates.begin(), dates.end());
    while (dates.size() > 35) {
        x[fieldName].erase(str(dates[0]));
        dates.erase(dates.begin());
    }
}
bool resultRow(const J &r, const std::string &kind) {
    return r.is_object() && field(r, "correct").is_boolean() && field(r, "assisted").is_boolean() &&
           field(r, "unknown").is_boolean() &&
           (kind == "meaning" ? field(r, "typed").is_null()
                              : field(r, "typed").is_string() && points(str(r, "typed")).size() <= 120);
}
} // namespace
NativeModel::NativeModel(const J &data) : data_(data) {
    for (const auto &w : data_.at("catalog"))
        words_.emplace(str(w, "id"), &w);
    for (const auto &l : data_.at("lessons"))
        lessons_.emplace(str(l, "id"), &l);
}
const J &NativeModel::word(std::string id) const {
    const auto it = words_.find(id);
    return it == words_.end() ? nil : *it->second;
}
const J &NativeModel::lesson(std::string id) const {
    const auto it = lessons_.find(id);
    return it == lessons_.end() ? nil : *it->second;
}
const J &NativeModel::tasks(const J &s) const {
    return field(field(data_, flag(s, "legacy") ? "legacyTasks" : "tasks"), str(s, "lesson"));
}
const J &NativeModel::task(const J &s) const {
    const auto &q = field(s, "queue");
    const auto at = num(s, "at");
    if (!q.is_array() || at < 0 || size_t(at) >= q.size())
        return nil;
    for (const auto &t : tasks(s))
        if (field(t, "id") == q[size_t(at)])
            return t;
    return nil;
}
J NativeModel::known(const J &x) const {
    J r = arr();
    for (const auto *k : {"memory", "batchMemory"})
        for (const auto &id : keys(field(x, k)))
            if (!word(str(id)).is_null())
                addUnique(r, id);
    return r;
}
J NativeModel::pending(const J &x) const {
    J r = arr();
    const auto &s = field(x, "session");
    if (flag(s, "unified"))
        for (const auto &id : field(s, "wordIds"))
            if (!has(field(s, "wordDone"), id))
                r.push_back(id);
    return r;
}
std::string NativeModel::normal(const std::string &s) {
    auto p = points(s);
    uint32_t *dst = nullptr;
    const auto reallocFn = [](void *, void *v, size_t n) -> void * {
        if (!n) {
            std::free(v);
            return nullptr;
        }
        return std::realloc(v, n);
    };
    int n = unicode_normalize(&dst, p.data(), int(p.size()), UNICODE_NFKC, nullptr, reallocFn);
    require(n >= 0, "拼写规范化失败");
    std::string out;
    bool space = false;
    for (int i = 0; i < n; ++i) {
        uint32_t lower[3];
        const int len = lre_case_conv(lower, dst[i], 1);
        for (int k = 0; k < len; ++k) {
            auto c = lower[k];
            if (c == 0x2018 || c == 0x2019)
                c = '\'';
            if (c == 0x2010 || c == 0x2011 || c == 0x2013 || c == 0x2014)
                c = '-';
            if (c == '.' || c == ',' || c == '?' || c == '!' || c == ';' || c == ':' || c == 0x2026)
                c = ' ';
            if (lre_is_space(c)) {
                space = !out.empty();
                continue;
            }
            if (space) {
                out += ' ';
                space = false;
            }
            utf8(out, c);
        }
    }
    std::free(dst);
    return out;
}
std::string NativeModel::dateKey(int64_t t) {
    std::time_t time = std::time_t(t / 1000 + 28800);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &tm);
    return b;
}
J NativeModel::gap(const std::string &id, int64_t serial) const {
    const auto p = points(normal(str(word(id), "word")));
    std::vector<size_t> positions;
    for (size_t i = 0; i < p.size(); ++i)
        if (p[i] >= 'a' && p[i] <= 'z')
            positions.push_back(i);
    uint32_t h = uint32_t(serial);
    for (auto c : points(id))
        h = h * 31u + c;
    std::stable_sort(positions.begin(), positions.end(), [&](size_t a, size_t b) {
        return uint32_t(a * 1103515245ull + h) < uint32_t(b * 1103515245ull + h);
    });
    if (positions.size() > 2)
        positions.resize(2);
    std::sort(positions.begin(), positions.end());
    std::string mask, answer;
    for (size_t i = 0; i < p.size(); ++i) {
        if (std::find(positions.begin(), positions.end(), i) != positions.end()) {
            utf8(mask, 0xff3f);
            utf8(answer, p[i]);
        } else
            utf8(mask, p[i]);
    }
    return J{{"positions", positions}, {"mask", mask}, {"answer", answer}};
}
J NativeModel::options(const std::string &id, const J &ids, int64_t serial) const {
    const auto &w = word(id);
    J pool = arr();
    auto add = [&](const J &v) {
        if (str(v, "meaning") != str(w, "meaning") && gloss(str(v, "meaning")) != gloss(str(w, "meaning")))
            addUnique(pool, field(v, "meaning"));
    };
    for (const auto &i : ids)
        add(word(str(i)));
    for (const auto &v : field(data_, "catalog"))
        add(v);
    pool = order(pool, std::to_string(serial));
    require(pool.size() >= 2);
    pool.erase(pool.begin() + 2, pool.end());
    const unsigned right = hash(std::to_string(serial) + ":" + id) % 3;
    pool.insert(pool.begin() + right, field(w, "meaning"));
    return J{{"options", pool}, {"answer", right}};
}
J NativeModel::make(const J &ids, const J &source, int64_t serial, const std::string &mode) const {
    require(ids.is_array() && !ids.empty() && ids.size() <= 8 && unique(ids) == ids);
    for (const auto &id : ids)
        require(!word(str(id)).is_null());
    auto meaning = order(ids, std::to_string(serial) + ":meaning", str(ids.back()));
    auto spell = order(ids, std::to_string(serial) + ":spelling", str(meaning.back()));
    if (ids.size() > 2 && spell == meaning) {
        std::rotate(spell.begin(), spell.begin() + 1, spell.end());
        if (spell[0] == meaning.back())
            std::rotate(spell.begin(), spell.begin() + 1, spell.end());
    }
    J queue = arr(), choices = obj(), gaps = obj();
    for (const auto &id : meaning)
        queue.push_back(J{{"id", id}, {"kind", "meaning"}, {"filler", false}});
    for (const auto &id : spell)
        queue.push_back(J{{"id", id}, {"kind", "spelling"}, {"filler", false}});
    for (const auto &id : ids) {
        choices[str(id)] = options(str(id), ids, serial);
        gaps[str(id)] = gap(str(id), serial);
    }
    return J{{"version", "word-groups-1"},
             {"serial", serial},
             {"mode", mode},
             {"source", source},
             {"ids", ids},
             {"started", now},
             {"phase", "overview"},
             {"learnAt", 0},
             {"learned", arr()},
             {"queue", queue},
             {"at", 0},
             {"selected", nullptr},
             {"typed", ""},
             {"hint", false},
             {"checked", nullptr},
             {"results", obj()},
             {"choices", choices},
             {"targets", arr()},
             {"paused", false},
             {"trainingVersion", 2},
             {"practiceMode", nullptr},
             {"gaps", gaps},
             {"reviewVersion", 1},
             {"targetIds", ids},
             {"corrections", arr()},
             {"retryScheduled", arr()},
             {"deferredReviews", arr()},
             {"retryPool", ids}};
}
void NativeModel::applyMode(J &s, const std::string &mode) const {
    require(mode == "staged" || mode == "full" || mode == "meaning", "请选择训练方式");
    require(str(s, "phase") == "overview", "开始后不更换本组训练");
    s["trainingVersion"] = 2;
    s["practiceMode"] = mode;
    const auto base = make(s["ids"], s["source"], num(s, "serial"), str(s, "mode"));
    J meaning = arr(), full = arr();
    for (const auto &q : base["queue"])
        (str(q, "kind") == "meaning" ? meaning : full).push_back(q["id"]);
    s["queue"] = arr();
    auto append = [&](const J &ids, const char *kind) {
        for (const auto &id : ids)
            s["queue"].push_back(J{{"id", id}, {"kind", kind}, {"filler", false}});
    };
    append(meaning, "meaning");
    if (mode == "staged" && s["ids"].size() > 1) {
        J reversed = s["ids"];
        std::reverse(reversed.begin(), reversed.end());
        const auto g = rotate(reversed, str(meaning.back()), meaning), f = rotate(full, str(g.back()), g);
        append(g, "gap");
        append(f, "spelling");
    } else if (mode != "meaning")
        append(full, "spelling");
    s["gaps"] = base["gaps"];
    s["at"] = 0;
    s["selected"] = nullptr;
    s["typed"] = "";
    s["hint"] = false;
    s["checked"] = nullptr;
    s.erase("deferredKind");
    if (s["ids"].size() == 1 && str(s, "mode") == "review") {
        const auto kind = mode == "meaning"              ? "meaning"
                          : str(s, "singleKind").empty() ? "meaning"
                                                         : str(s, "singleKind");
        s["queue"] = J::array({J{{"id", s["ids"][0]}, {"kind", kind}, {"filler", false}}});
        s["singleKind"] = kind;
        if (mode != "meaning")
            s["deferredKind"] = kind == "meaning" ? "spelling" : "meaning";
    }
}
bool NativeModel::addKnown(J &s, const std::string &id) const {
    if (has(s["ids"], id))
        return true;
    if (s["ids"].size() >= 8)
        return false;
    s["ids"].push_back(id);
    s["choices"][id] = options(id, J::array({id}), num(s, "serial"));
    s["gaps"][id] = gap(id, num(s, "serial"));
    if (str(s, "phase") == "check" && str(s, "mode") == "learn")
        s["learned"].push_back(id);
    return true;
}
void NativeModel::schedule(J &s, const J &q) const {
    const auto kind = str(q, "kind"), id = str(q, "id"), key = id + "|" + kind;
    if (kind == "gap" || flag(q, "filler") || flag(q, "followup") || has(s["retryScheduled"], key))
        return;
    s["retryScheduled"].push_back(key);
    J others = arr();
    int insert = -1;
    for (size_t i = num(s, "at") + 1; i < s["queue"].size(); ++i) {
        const auto &p = s["queue"][i];
        if (str(p, "id") == id)
            others = arr();
        else
            addUnique(others, p["id"]);
        if (others.size() >= 3) {
            insert = int(i + 1);
            break;
        }
    }
    if (insert < 0) {
        J present = arr(), extra = arr();
        for (const auto &v : s["retryPool"])
            if (v != id && !has(others, v))
                (has(s["ids"], v) ? present : extra).push_back(v);
        const auto room = 8 - s["ids"].size();
        for (size_t i = 0; i < std::min(room, extra.size()); ++i)
            present.push_back(extra[i]);
        const size_t need = 3 - others.size();
        if (present.size() < need) {
            s["deferredReviews"].push_back(J{{"id", id}, {"kind", kind}});
            return;
        }
        for (size_t i = 0; i < need; ++i) {
            addKnown(s, str(present[i]));
            s["queue"].push_back(J{{"id", present[i]}, {"kind", kind}, {"filler", true}});
        }
        insert = int(s["queue"].size());
    }
    s["queue"].insert(s["queue"].begin() + insert,
                      J{{"id", id}, {"kind", kind}, {"filler", false}, {"followup", true}});
}
void NativeModel::checkBatch(J &s, bool unknown) const {
    require(str(s, "phase") == "check" && field(s, "checked").is_null(), "当前不能检查");
    const auto q = s["queue"].at(num(s, "at"));
    const auto id = str(q, "id"), kind = str(q, "kind");
    require(unknown ||
                (kind == "meaning" ? !field(s, "selected").is_null() : !normal(str(s, "typed")).empty()),
            "请先选择或输入拼写");
    const bool correct =
        !unknown &&
        (kind == "meaning" ? s["selected"] == s["choices"][id]["answer"]
                           : normal(str(s, "typed")) == (kind == "gap" ? str(s["gaps"][id], "answer")
                                                                       : normal(str(word(id), "word"))));
    J r{{"correct", correct},
        {"assisted", s["hint"]},
        {"unknown", unknown},
        {"typed", kind == "meaning" ? J(nullptr) : s["typed"]}};
    s["checked"] = r;
    if (num(s, "reviewVersion") == 1 && flag(q, "followup")) {
        J correction{{"id", id}, {"kind", kind}};
        correction.update(r);
        correction["afterFeedback"] = true;
        s["corrections"].push_back(correction);
    } else {
        if (!flag(q, "filler"))
            s["results"][id][kind] = r;
        if (num(s, "reviewVersion") == 1 && !actual(r))
            schedule(s, q);
    }
}
bool NativeModel::nextBatch(J &s) const {
    require(str(s, "phase") == "check" && !s["checked"].is_null(), "先检查本题");
    size_t at = num(s, "at");
    if (num(s, "reviewVersion") != 1 && str(s, "mode") == "review" && s["ids"].size() == 1 &&
        at + 1 < s["queue"].size() && s["queue"][at + 1]["id"] == s["queue"][at]["id"]) {
        s["deferredKind"] = s["queue"][at + 1]["kind"];
        return true;
    }
    if (at + 1 >= s["queue"].size())
        return true;
    ++at;
    s["at"] = at;
    s["selected"] = nullptr;
    s["typed"] = "";
    s["hint"] = false;
    s["checked"] = nullptr;
    const J q = s["queue"][at];
    if (num(s, "reviewVersion") != 1 || !flag(q, "followup"))
        return false;
    const auto id = q["id"];
    J seen = arr();
    for (size_t i = 0; i < at; ++i) {
        if (s["queue"][i]["id"] == id)
            seen = arr();
        else
            addUnique(seen, s["queue"][i]["id"]);
    }
    if (seen.size() >= 3)
        return false;
    int to = -1;
    for (size_t i = at + 1; i < s["queue"].size(); ++i) {
        if (s["queue"][i]["id"] == id)
            seen = arr();
        else
            addUnique(seen, s["queue"][i]["id"]);
        if (seen.size() >= 3) {
            to = int(i);
            break;
        }
    }
    if (to >= 0) {
        s["queue"].erase(s["queue"].begin() + at);
        s["queue"].insert(s["queue"].begin() + to, q);
        return false;
    }
    J available = arr();
    for (const auto &v : s["retryPool"])
        if (v != id && !has(seen, v))
            available.push_back(v);
    J fillers = arr();
    const size_t need = 3 - seen.size();
    for (size_t i = 0; i < std::min(need, available.size()); ++i)
        if (addKnown(s, str(available[i])))
            fillers.push_back(available[i]);
    s["queue"].erase(s["queue"].begin() + at);
    if (fillers.size() + seen.size() < 3) {
        s["deferredReviews"].push_back(J{{"id", id}, {"kind", q["kind"]}});
        return at >= s["queue"].size();
    }
    for (const auto &v : fillers)
        s["queue"].push_back(J{{"id", v}, {"kind", q["kind"]}, {"filler", true}});
    s["queue"].push_back(q);
    return false;
}
J NativeModel::summarize(const J &s) const {
    J r{{"version", s["version"]},
        {"serial", s["serial"]},
        {"originSerial", truth(field(s, "originSerial")) ? s["originSerial"] : s["serial"]},
        {"mode", s["mode"]},
        {"source", s["source"]},
        {"ids", s["ids"]},
        {"time", now},
        {"results", s["results"]},
        {"targets", s["targets"]}};
    for (const auto *k : {"deferredKind", "trainingVersion", "practiceMode", "courseOwner"})
        if (s.contains(k))
            r[k] = s[k];
    if (num(s, "reviewVersion") == 1) {
        r["reviewVersion"] = 1;
        r["ids"] = s["targetIds"];
        if (s.contains("reviewKind"))
            r["reviewKind"] = s["reviewKind"];
        r["corrections"] = s["corrections"];
        r["deferredReviews"] = s["deferredReviews"];
    }
    return r;
}
void NativeModel::updateMemory(J &memory, const J &r) const {
    static const int intervals[] = {1, 2, 4, 7};
    for (const auto &idj : r["ids"]) {
        const auto id = str(idj);
        for (const auto *kind : {"meaning", "spelling"}) {
            const auto &v = field(field(r["results"], id), kind);
            if (v.is_null())
                continue;
            const auto old = field(field(memory, id), kind);
            J m;
            const bool early = !old.is_null() && now < num(old, "due");
            if (num(r, "reviewVersion") == 1) {
                J correction;
                for (const auto &q : r["corrections"])
                    if (q["id"] == id && q["kind"] == kind) {
                        correction = q;
                        break;
                    }
                if (!actual(v) || str(r, "mode") == "revisit") {
                    m = old.is_object() ? old : obj();
                    m.update(J{{"stage", 0},
                               {"due", now + (actual(correction) ? StudyDay : StudyGap)},
                               {"last", now},
                               {"assisted", flag(v, "assisted")},
                               {"afterFeedback", true},
                               {"reviewPolicy", 1},
                               {"independent", 0},
                               {"errors", num(old, "errors") + (!flag(v, "correct") ? 1 : 0)},
                               {"corrections", num(old, "corrections") + (actual(correction) ? 1 : 0)},
                               {"needsCorrection", !actual(correction)}});
                } else if (early) {
                    m = old;
                    m["last"] = now;
                } else {
                    int stage =
                        int(std::max(num(old, "stage"), std::min(int64_t(3), num(old, "independent"))));
                    m = old.is_object() ? old : obj();
                    m.update(J{{"stage", stage},
                               {"due", now + intervals[stage] * StudyDay},
                               {"last", now},
                               {"lastIndependent", now},
                               {"assisted", false},
                               {"afterFeedback", false},
                               {"pending", false},
                               {"needsCorrection", false},
                               {"reviewPolicy", 1},
                               {"independent", num(old, "independent") + 1},
                               {"errors", num(old, "errors")},
                               {"corrections", num(old, "corrections")}});
                }
            } else {
                if (!actual(v))
                    m = J{{"stage", 0},
                          {"due", now + (r["ids"].size() == 1 ? StudyGap : 0)},
                          {"last", now},
                          {"assisted", v["assisted"]}};
                else if (str(r, "mode") == "revisit")
                    m = J{{"stage", num(old, "stage")},
                          {"due", std::max(num(old, "due"), now + StudyDay)},
                          {"last", now},
                          {"assisted", false},
                          {"afterFeedback", true}};
                else if (early) {
                    m = old;
                    m["last"] = now;
                } else {
                    const auto stage = old.is_null() ? 0 : std::min(int64_t(3), num(old, "stage") + 1);
                    m = J{{"stage", stage},
                          {"due", now + intervals[stage] * StudyDay},
                          {"last", now},
                          {"assisted", false}};
                }
            }
            memory[id][kind] = m;
        }
    }
    if (flag(r, "deferredKind")) {
        const auto id = str(r["ids"][0]), kind = str(r, "deferredKind");
        const J old = field(field(memory, id), kind);
        memory[id][kind] = J{{"stage", num(old, "stage")},
                             {"due", std::max(num(old, "due"), now + StudyGap)},
                             {"last", num(old, "last")},
                             {"pending", true}};
    }
}
J NativeModel::memoryRow(const J &x, const std::string &id, const std::string &kind) const {
    const auto &m = field(field(field(x, "batchMemory"), id), kind);
    if (!m.is_null())
        return m;
    const auto &old = field(field(x, "memory"), id);
    if (kind == "meaning" && !old.is_null()) {
        J v = old;
        v["last"] = 0;
        v["legacy"] = true;
        return v;
    }
    return nullptr;
}
J NativeModel::reviewItems(const J &x, const std::string &kind, const std::string &mode, const J &ids) const {
    J chosen = ids;
    if (ids.is_null()) {
        chosen = mode == "notebook" ? field(x, "notebook") : known(x);
        if (mode == "notebook")
            for (const auto &id : field(x, "extraNotebook"))
                addUnique(chosen, id);
    }
    J rows = arr();
    const auto &last = field(x, "batchLast");
    for (const auto &idj : unique(chosen)) {
        const auto id = str(idj);
        if (word(id).is_null())
            continue;
        for (const auto *k : {"meaning", "spelling"}) {
            if (kind != "all" && kind != k)
                continue;
            const auto m = memoryRow(x, id, k);
            if (mode == "due" && m.is_null())
                continue;
            int64_t due = m.is_null() ? now : num(m, "due");
            if (!m.is_null()) {
                if (num(m, "last") && due <= num(m, "last") &&
                    (flag(m, "assisted") || flag(m, "afterFeedback")))
                    due = std::max(due, num(m, "last") + StudyGap);
                const auto &v = field(field(field(last, "results"), id), k);
                bool corrected = false;
                for (const auto &q : field(last, "corrections"))
                    if (str(q, "id") == id && str(q, "kind") == k && actual(q))
                        corrected = true;
                if (!v.is_null() && !actual(v) && !corrected)
                    due = std::max(due, num(last, "time") + StudyGap);
                if (field(last, "ids").size() == 1 && last["ids"][0] == id) {
                    J all = keys(field(x, "memory"));
                    for (const auto &i : keys(field(x, "batchMemory")))
                        addUnique(all, i);
                    if (all.size() == 1)
                        due = std::max(due, num(last, "time") + StudyGap);
                }
            }
            const bool ready = mode == "due"
                                   ? due <= now
                                   : !(!m.is_null() && due > now &&
                                       due <= std::max(num(m, "last"), num(last, "time")) + StudyGap);
            rows.push_back(J{{"id", id},
                             {"kind", k},
                             {"due", due},
                             {"ready", ready},
                             {"memory", m},
                             {"untested", m.is_null()}});
        }
    }
    std::stable_sort(rows.begin(), rows.end(), [](const J &a, const J &b) {
        if (a["due"] != b["due"])
            return num(a, "due") < num(b, "due");
        if (a["id"] != b["id"])
            return str(a, "id") < str(b, "id");
        return str(a, "kind") < str(b, "kind");
    });
    return rows;
}
std::string NativeModel::memoryLabel(const J &m) {
    if (m.is_null())
        return "尚未检查";
    if (flag(m, "needsCorrection"))
        return "还需重练";
    if (flag(m, "afterFeedback"))
        return "纠正后待复习";
    if (num(m, "reviewPolicy") != 1)
        return "原记录保留";
    return num(m, "independent") < 2   ? "首次记住，待巩固"
           : num(m, "independent") < 4 ? "已间隔回忆" + std::to_string(num(m, "independent")) + "次"
                                       : "已多次间隔回忆";
}
std::string NativeModel::sourceTitle(const J &s) const {
    const auto type = str(s, "type"), id = str(s, "id");
    if (type == "course") {
        const auto &l = lesson(id);
        return str(l, "code") + " " + str(l, "title");
    }
    if (type == "bridge")
        for (const auto &p : data_["packs"])
            if (str(p, "id") == id)
                return str(p, "title");
    return id == "notebook" ? "我的生词本" : "已学词分组复习";
}
bool NativeModel::complete(const J &x, const std::string &id) const {
    const auto &l = lesson(id), c = field(field(x, "completed"), id);
    return !l.is_null() &&
           (field(field(x, "achievements"), id) == field(l, "revision") ||
            (!c.is_null() && field(c, "revision") == field(l, "revision") && field(c, "skipped").empty()));
}
int NativeModel::unitCount(const J &x, int unit) const {
    int n = 0;
    for (const auto &l : data_["lessons"])
        if (num(l, "unit") == unit && str(l, "role") == "lesson" && complete(x, str(l, "id")))
            ++n;
    return n;
}
} // namespace wordcard
