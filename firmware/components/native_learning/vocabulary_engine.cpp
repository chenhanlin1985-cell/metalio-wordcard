#include "vocabulary.hpp"
#include "pack_paths.hpp"
#ifdef ESP_PLATFORM
#include <psa/crypto.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
#else
#include <openssl/sha.h>
#endif
#include "bookshop.hpp"
#include "vocabulary_journey.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
namespace wordcard {
namespace {
constexpr int64_t Day = 86400000, Gap = 900000;
const char *Hash = "0dc7757d42a0c544871b688883dfb0f3b7dc4f0ceec5546a618b6a06f4081b6a";
bool partial_word(const std::string &word) {
    return std::count_if(word.begin(),word.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');})>=2;
}
uint32_t hash(const std::string &s) {
    uint32_t h = 2166136261;
    for (size_t i = 0; i < s.size();) {
        uint32_t c = (unsigned char)s[i++];
        if (c >= 0xf0 && i + 2 < s.size()) {
            c = (c & 7) << 18;
            c |= ((unsigned char)s[i++] & 63) << 12;
            c |= ((unsigned char)s[i++] & 63) << 6;
            c |= (unsigned char)s[i++] & 63;
        } else if (c >= 0xe0 && i + 1 < s.size()) {
            c = (c & 15) << 12;
            c |= ((unsigned char)s[i++] & 63) << 6;
            c |= (unsigned char)s[i++] & 63;
        } else if (c >= 0xc0 && i < s.size()) {
            c = (c & 31) << 6;
            c |= (unsigned char)s[i++] & 63;
        }
        if (c > 0xffff) {
            c -= 0x10000;
            h = (h ^ (0xd800 + (c >> 10))) * 16777619;
            c = 0xdc00 + (c & 1023);
        }
        h = (h ^ c) * 16777619;
    }
    return h;
}
std::string norm(std::string s) {
    for (size_t p; (p = s.find("’")) != std::string::npos;)
        s.replace(p, 3, "'");
    for (char &c : s)
        if ((unsigned char)c < 128)
            c = char(std::tolower((unsigned char)c));
    auto a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
VJ &day(VJ &s, int64_t now) {
    auto k = VocabularyEngine::date(now);
    if (!s["days"].contains(k))
        s["days"][k] = VJ{{"new", VJ::array()}, {"reviews", VJ::array()}, {"answers", 0}};
    return s["days"][k];
}
int64_t size(const VJ &s) {
    return s.is_array() || s.is_object() ? int64_t(s.size()) : 0;
}
bool unfinished(const VJ &session) {
    return !session.is_null() && str(session, "phase") != "summary";
}
std::set<std::string> reserved_words(const VJ &s) {
    std::set<std::string> ids;
    auto reserve = [&](const VJ &session) {
        if (unfinished(session))
            for (const auto &id : field(session, "ids"))
                ids.insert(str(id));
    };
    reserve(field(s, "session"));
    for (const auto &session : field(s, "paused")) reserve(session);
    const auto &chapter = field(s, "bookshop");
    const auto &plan = field(chapter, "ids");
    if (plan.is_array() && plan.size() == 10) {
        const size_t first = 5 * std::clamp<int64_t>(num(chapter, "groups"), 0, 2);
        for (size_t i = first; i < plan.size(); ++i) ids.insert(str(plan[i]));
    }
    return ids;
}
void activate_session(VJ &s, const VJ &target) {
    const auto &previous = field(s, "session");
    if (unfinished(previous) && str(previous, "source") == "legacy" &&
        field(previous, "id") != field(target, "id")) {
        const auto id = str(previous, "id");
        const auto stage = str(s, "selectedStage");
        auto &stages = s["nativeResumeStages"];
        if (stages.is_null()) stages = VJ::object();
        require(stages.is_object(), "暂停词组来源记录损坏，原进度保留");
        stages[id] = stage;
    }
    // Adding optional top-level metadata may invalidate ordered-JSON references.
    auto &current = s["session"];
    auto &paused = s["paused"];
    paused.erase(std::remove_if(paused.begin(), paused.end(), [&](const VJ &p) {
        return field(p, "id") == field(target, "id");
    }), paused.end());
    if (unfinished(current) && field(current, "id") != field(target, "id"))
        paused.push_back(current);
    current = target;
    s["summary"] = nullptr;
}
std::vector<std::string> senses(const std::string &s) {
    std::vector<std::string> out;
    size_t start = 0, p;
    while ((p = s.find("；", start)) != std::string::npos) {
        out.push_back(s.substr(start, p - start));
        start = p + 3;
    }
    out.push_back(s.substr(start));
    return out;
}
} // namespace
#include "deck_catalog.inc"
#include "story_catalog.inc"
bool VocabularyCatalog::load(const std::string &dir) {
    std::ifstream meta(dir + "/catalog.json", std::ios::binary),
        idx(dir + "/catalog.tsv", std::ios::binary);
    if (!meta || !idx)
        return false;
    metadata = VJ::parse(meta);
    require(str(metadata, "version") == "vocabulary-complete-2026-11-04-v1" ||
            str(metadata, "version") == "vocabulary-independent-2026-11-04-v1" ||
            str(metadata, "version") == "vocabulary-gre-2026-11-17-v1", "词库版本不支持");
    directory = dir;
    entries.clear();
    index_.clear();
    external = VJ::object();
    cache_.clear();
    std::string line;
    while (std::getline(idx, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream in(line);
        std::vector<std::string> f;
        std::string part;
        while (std::getline(in, part, '\t'))
            f.push_back(part);
        require(f.size() == 8, "词库索引不完整");
        require(!index_.count(f[0]), "词条ID重复");
        Entry e{
            f[0], f[1], f[2], f[3], f[4], f[5] == "1", std::stol(f[6]), size_t(std::stoul(f[7]))};
        require(e.offset >= 0 && e.length > 0 && e.length < 16384, "词库位置损坏");
        index_[e.id] = entries.size();
        entries.push_back(std::move(e));
    }
    const size_t expected = str(metadata, "version") == "vocabulary-gre-2026-11-17-v1" ? 4738u :
        (str(metadata, "version") == "vocabulary-independent-2026-11-04-v1" ? 4598u : 2479u);
    require(entries.size() == expected, "词库数量不完整");
    load_decks(dir.substr(0,dir.find_last_of("/\\"))+"/packs");
    shopCues=nullptr;
    std::ifstream story(dir+"/bookshop-v1.json",std::ios::binary|std::ios::ate);
    if(story && story.tellg()>0 && story.tellg()<=65536){
        story.seekg(0);auto file=VJ::parse(story,nullptr,false);
        if(file.is_object()&&num(file,"version")==1){
            const auto &cues=field(file,"cues");bool ok=cues.is_object()&&cues.size()>=10;
            for(auto i=cues.begin();ok&&i!=cues.end();++i){
                const auto *e=entry(i.key());
                ok=e&&e->stage=="gre-trial"&&i.value().is_array()&&i.value().size()==3;
                if(ok)for(const auto &cue:i.value())ok=ok&&cue.is_string()&&!str(cue).empty();
            }
            if(ok)shopCues=cues;
        }
    }
    return true;
}
const VocabularyCatalog::Entry *VocabularyCatalog::entry(const std::string &id) const {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &entries[it->second];
}
VJ VocabularyCatalog::get(const std::string &id) const {
    auto hit = cache_.find(id);
    if (hit != cache_.end())
        return hit->second;
    auto e = entry(id);
    require(e != nullptr, "词条不存在");
    if (external.contains(id)) return external[id];
    if (auto w=deck_word(id); !w.is_null()) return w;
    std::ifstream in(directory + "/catalog.ndjson", std::ios::binary);
    require(bool(in), "SD词库无法读取");
    in.seekg(e->offset);
    std::string raw(e->length, '\0');
    in.read(&raw[0], std::streamsize(raw.size()));
    require(size_t(in.gcount()) == raw.size(), "SD词条不完整");
    auto w = VJ::parse(raw);
    require(str(w, "id") == id, "词库与索引不匹配");
    if (cache_.size() >= 64)
        cache_.clear();
    cache_[id] = w;
    return w;
}
std::string VocabularyEngine::date(int64_t now) {
    std::time_t t = now / 1000 + 28800;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &tm);
    return b;
}
VJ VocabularyEngine::empty_card(int64_t now) {
    return VJ{{"memory", nullptr},
              {"lastReviewAt", nullptr},
              {"due", now},
              {"fsrsDue", now},
              {"reps", 0},
              {"lapses", 0},
              {"successes", 0},
              {"independentDays", VJ::array()},
              {"lastRating", nullptr},
              {"pendingRelearn", false}};
}
VJ VocabularyEngine::session(VJ &s, const VJ &ids, const std::string &source, int64_t now,
                             const std::string &mode, bool associative) const {
    s["serial"] = num(s, "serial") + 1;
    auto id = "s" + str(s, "serial") + "-" + std::to_string(now);
    VJ v{{"id", id},
         {"source", source},
         {"mode", mode},
         {"ids", ids},
         {"startedAt", now},
         {"phase", source == "review" ? "tasks" : "learn"},
         {"learnAt", 0},
         {"tasks", VJ::array()},
         {"current", nullptr},
         {"feedback", nullptr},
         {"answered", 0},
         {"correct", 0},
         {"wrong", 0},
         {"newCount", 0},
         {"deferrals", VJ::array()},
         {"retryKeys", VJ::array()},
         {"lastWords", VJ::array()},
         {"journeyAttempts", 0}};
    if (source != "review") {
        std::vector<std::string> kinds = {"meaning"};
        if (mode == "staged")
            kinds.push_back("gap");
        if (mode == "partial") kinds.push_back("partial");
        if (mode != "meaning" && mode != "partial")
            kinds.push_back(associative ? "recall" : "spelling");
        for (const auto &k : kinds) {
            auto sorted = ids;
            std::stable_sort(sorted.begin(), sorted.end(), [&](const VJ &a, const VJ &b) {
                return hash(id + k + str(a)) < hash(id + k + str(b));
            });
            for (const auto &wid : sorted) {
                if(k=="partial"&&!partial_word(catalog.entry(str(wid))->word))continue;
                v["tasks"].push_back(VJ{{"uid", id + ":" + std::to_string(v["tasks"].size())},
                                        {"id", wid},
                                        {"kind", k},
                                        {"followup", false},
                                        {"done", false},
                                        {"requiresGap", true}});
            }
        }
    }
    if(source=="new"||source=="review")v["deckStage"]=str(s,"selectedStage");
    if (associative) v["associative"] = true;
    return v;
}
VJ VocabularyEngine::create(int64_t now) const {
    VJ s{{"schema", 1},
         {"contentVersion", catalog.metadata["version"]},
         {"revision", 0},
         {"createdAt", now},
         {"lastClock", now},
         {"selectedStage", "foundation"},
         {"stageChosen", false},
         {"settings", VJ{{"mode", "staged"}, {"dailyNew", 10}}},
         {"cards", VJ::object()},
         {"seen", VJ::object()},
         {"notebook", VJ::array()},
         {"days", VJ::object()},
         {"session", nullptr},
         {"paused", VJ::array()},
         {"summary", nullptr},
         {"serial", 0},
         {"exposureSequence", 0},
         {"exposures", VJ::object()},
         {"recentExposures", VJ::array()},
         {"commandIds", VJ::array()},
         {"storyRuns", VJ::object()}};
    return s;
}
VJ VocabularyEngine::current(const VJ &s) const {
    const auto &ss = field(s, "session");
    for (const auto &t : field(ss, "tasks"))
        if (field(t, "uid") == field(ss, "current"))
            return t;
    return nullptr;
}
bool VocabularyEngine::current_word_matches(const VJ &s, const std::string &word) const {
    const auto &ss = field(s, "session");
    if (str(ss, "phase") != "tasks" || !field(ss, "feedback").is_null()) return false;
    const auto task = current(s);
    const auto *entry = catalog.entry(str(task, "id"));
    return entry && !norm(word).empty() && norm(entry->word) == norm(word);
}
void VocabularyEngine::touch(VJ &s, const std::string &id, int64_t now, VJ &events,
                             const std::string &type, const VJ &detail) const {
    s["exposureSequence"] = num(s, "exposureSequence") + 1;
    s["exposures"][id] = VJ{{"sequence", s["exposureSequence"]}, {"at", now}};
    auto &recent = s["recentExposures"];
    recent.push_back(id);
    if (recent.size() > 400)
        recent.erase(recent.begin());
    if (type!="story_answer" && type!="story_exposure" && type!="course_answer" && !s["session"].is_null()) {
        auto &last = s["session"]["lastWords"];
        last.push_back(id);
        if (last.size() > 200)
            last.erase(last.begin());
    }
    VJ e{{"type", type}, {"wordId", id}, {"senseId", id}, {"at", now}};
    e.update(detail);
    events.push_back(e);
}
bool VocabularyEngine::enough(const VJ &s, const std::string &id, int64_t now) const {
    const auto &e = field(field(s, "exposures"), id);
    if (!e.is_null() && now - num(e, "at") >= Gap)
        return true;
    std::set<std::string> others;
    const auto *target=catalog.entry(id);
    const auto &r = field(s, "recentExposures");
    for (auto it = r.rbegin(); it != r.rend(); ++it) {
        if (*it == id)
            return others.size() >= 3;
        const auto *word=catalog.entry(str(*it));
        if(word&&(!target||norm(word->word)!=norm(target->word)))others.insert(norm(word->word));
    }
    return true;
}
VJ VocabularyEngine::gap(const VJ &t) const {
    auto word = catalog.entry(str(t, "id"))->word;
    std::vector<size_t> positions;
    for (size_t i = 0; i < word.size(); ++i)
        if (std::isalpha((unsigned char)word[i]))
            positions.push_back(i);
    auto interior = positions;
    if (positions.size() > 3) {
        interior.erase(interior.begin());
        interior.pop_back();
    }
    auto count = std::min(size_t(2), std::max(size_t(1), positions.size() - 1));
    VJ selected = VJ::array();
    std::string answer, mask;
    for (size_t i = 0; i < std::min(count, interior.size()); ++i) {
        selected.push_back(interior[i]);
        answer += word[interior[i]];
    }
    for (size_t i = 0; i < word.size(); ++i)
        mask += has(selected, i) ? "＿" : word.substr(i, 1);
    return VJ{{"positions", selected}, {"answer", answer}, {"mask", mask}};
}
VJ VocabularyEngine::choices(const VJ &t, const VJ &ss) const {
    if (field(t, "options").is_array())
        return t["options"];
    const auto cacheKey = str(ss, "id") + ":" + str(t, "uid");
    const auto cached = choiceCache_.find(cacheKey);
    if (cached != choiceCache_.end()) return cached->second;
    auto w = catalog.entry(str(t, "id"));
    require(w != nullptr);
    std::vector<std::pair<const VocabularyCatalog::Entry *, uint32_t>> pool;
    pool.reserve(catalog.entries.size());
    auto seed = str(ss, "id") + w->id;
    auto target = senses(w->meaning);
    const bool story=ss.contains("shopRole");
    for (const auto &e : catalog.entries) {
        if(story && (e.stage!="gre-trial" || e.word==w->word))continue;
        if (!e.eligible || catalog.external.contains(e.id) || e.id == w->id || norm(e.word) == norm(w->word) || e.meaning == w->meaning)
            continue;
        bool distinct = true;
        for (const auto &a : senses(e.meaning))
            if (std::find(target.begin(), target.end(), a) != target.end()) {
                distinct = false;
                break;
            }
        if (distinct)
            pool.emplace_back(&e, hash(seed + e.id));
    }
    std::stable_sort(pool.begin(), pool.end(), [&](const auto &ra, const auto &rb) {
        const auto *a = ra.first, *b = rb.first;
        if ((a->pos == w->pos) != (b->pos == w->pos))
            return a->pos == w->pos;
        return ra.second < rb.second;
    });
    VJ result = VJ::array({story?w->word:w->meaning});
    for (const auto &ranked : pool) {
        const auto *e = ranked.first;
        addUnique(result, story?e->word:e->meaning);
        if (result.size() == 3)
            break;
    }
    seed = str(ss, "id") + str(t, "uid");
    std::stable_sort(result.begin(), result.end(), [&](const VJ &a, const VJ &b) {
        return hash(seed + str(a)) < hash(seed + str(b));
    });
    if (choiceCache_.size() >= 64) choiceCache_.clear();
    choiceCache_[cacheKey] = result;
    return result;
}
VJ VocabularyEngine::due(const VJ &s, int64_t now) const {
    VJ result = VJ::array();
    for (auto it = s["cards"].begin(); it != s["cards"].end(); ++it)
        for (auto c = it.value().begin(); c != it.value().end(); ++c) {
            if (!catalog.entry(it.key()) ||
                (c.key() != "meaning" && str(s["settings"], "mode") == "meaning") ||
                (c.key() != "meaning" && c.key() != "partial" && str(s["settings"], "mode") == "partial") ||
                (c.key() == "partial" && str(s["settings"], "mode") != "partial"))
                continue;
            if (num(c.value(), "due") <= now)
                result.push_back(
                    VJ{{"id", it.key()}, {"kind", c.key()}, {"due", c.value()["due"]}});
        }
    std::stable_sort(result.begin(), result.end(), [](const VJ &a, const VJ &b) {
        return num(a, "due") != num(b, "due") ? num(a, "due") < num(b, "due")
               : str(a, "id") != str(b, "id") ? str(a, "id") < str(b, "id")
                                              : str(a, "kind") < str(b, "kind");
    });
    return result;
}
VJ VocabularyEngine::available_due(const VJ &s, int64_t now) const {
    std::set<std::string> reserved;
    auto reserve = [&](const VJ &session) {
        if (!unfinished(session)) return;
        for (const auto &task : field(session, "tasks"))
            if (!flag(task, "done") && str(task, "kind") != "gap")
                reserved.insert(str(task, "id") + "|" + str(task, "kind"));
    };
    reserve(field(s, "session"));
    for (const auto &session : field(s, "paused")) reserve(session);
    auto result = due(s, now);
    result.erase(std::remove_if(result.begin(), result.end(), [&](const VJ &task) {
        return reserved.count(str(task, "id") + "|" + str(task, "kind")) != 0;
    }), result.end());
    return result;
}
VJ VocabularyEngine::shop_due(const VJ &s,int64_t now) const {
    auto rows=available_due(s,now);
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const VJ&t){
        const auto id=str(t,"id");const auto *e=catalog.entry(id);
        return !e||e->stage!="gre-trial"||!enough(s,id,now);
    }),rows.end());
    return rows;
}
VJ VocabularyEngine::review_tasks(const VJ &s, int64_t now, const std::string &scope) const {
    require(scope == "current" || scope == "all" || scope == "notebook", "复习范围无效");
    const auto matches = [&](const std::string &id) {
        const auto *e = catalog.entry(id);
        return e && (scope == "all" || (scope == "notebook" ? has(s["notebook"], id)
                                                      : catalog.contains_word(str(s,"selectedStage"),id)));
    };
    auto result = available_due(s, now);
    result.erase(std::remove_if(result.begin(), result.end(), [&](const VJ &q) {
        return !matches(str(q, "id"));
    }), result.end());
    // Old listening/spelling records do not establish productive recall. Only
    // completed learning groups may seed this new channel; listing is read-only.
    if (str(s["settings"], "mode") != "meaning") {
        const auto skill=str(s["settings"],"mode")=="partial"?"partial":"recall";
        const auto reserved = reserved_words(s);
        for (auto it = s["seen"].begin(); it != s["seen"].end(); ++it) {
            if (!matches(it.key()) || reserved.count(it.key()) ||
                !field(field(s["cards"], it.key()), skill).is_null()) continue;
            if(std::string(skill)=="partial"&&!partial_word(catalog.entry(it.key())->word))continue;
            result.push_back(VJ{{"id", it.key()}, {"kind", skill}, {"due", now}, {std::string(skill)=="partial"?"firstPartial":"firstRecall", true}});
        }
    }
    return result;
}
VJ VocabularyEngine::available_new(const VJ &s) const {
    const auto reserved = reserved_words(s);
    VJ ids = VJ::array();
    auto deck=catalog.decks.find(str(s,"selectedStage"));
    if(deck!=catalog.decks.end()){
        for(const auto &id:deck->second.ids)if(!s["seen"].contains(id)&&!reserved.count(id))ids.push_back(id);
        return ids;
    }
    for (const auto &entry : catalog.entries)
        if (entry.eligible && catalog.contains_word(str(s, "selectedStage"),entry.id) &&
            !s["seen"].contains(entry.id) && !reserved.count(entry.id))
            ids.push_back(entry.id);
    return ids;
}
VJ VocabularyEngine::practice(const VJ &ids,const std::string &mode,int64_t now) const {
    require(ids.is_array()&&!ids.empty()&&ids.size()<=5,"请选择一至五个词");
    for(const auto &id:ids)require(catalog.entry(str(id))!=nullptr,"重练词条不可用");
    auto training=create(now);
    training["settings"]["mode"]=mode=="staged"?"full":mode;
    auto group=session(training,ids,"practice",now,str(training["settings"],"mode"),true);
    // A single selected word still gets both regular rounds. Corrections keep the usual spacing.
    for(auto &task:group["tasks"])task["requiresGap"]=false;
    training["session"]=std::move(group);
    VJ events=VJ::array();advance(training,now,events);
    return training;
}
VJ VocabularyEngine::available_notebook(const VJ &s) const {
    const auto reserved = reserved_words(s);
    VJ ids = VJ::array();
    // Personal dictionary entries are intentionally ineligible for the curriculum,
    // but remain valid members of the explicitly selected notebook.
    for (const auto &id : field(s, "notebook"))
        if (catalog.entry(str(id)) && !s["seen"].contains(str(id)) && !reserved.count(str(id)))
            ids.push_back(id);
    return ids;
}
VJ VocabularyEngine::continue_preview(const VJ &s, int64_t now) const {
    const auto &ss = field(s, "session");
    if (str(ss, "phase") != "tasks" || field(ss, "feedback").is_null()) return nullptr;
    auto next = s;
    VJ ignored = VJ::array();
    advance(next, now, ignored);
    const auto task = current(next);
    return VJ{{"phase", field(next["session"], "phase")},
              {"kind", str(task, "kind")},
              {"followup", flag(task, "followup")},
              {"source", field(next["session"], "source")}};
}
VJ VocabularyEngine::resumable(const VJ &s, const std::string &intent, const std::string &scope) const {
    auto matches = [&](const VJ &session, bool active) {
        if (!unfinished(session)) return false;
        if(session.contains("shopRole"))return intent=="bookshop";
        const auto source = str(session, "source");
        if (intent == "notebook") return source == intent;
        if (intent == "review") {
            if (source != intent) return false;
            if (scope == "all") return true;
            if(scope=="current"&&session.contains("deckStage")&&str(session,"deckStage")!=str(s,"selectedStage"))return false;
            if (field(session, "ids").empty()) return false;
            for (const auto &id : session["ids"]) {
                const auto *e = catalog.entry(str(id));
                if (!e || (scope == "notebook" ? !has(s["notebook"], id)
                                               : !catalog.contains_word(str(s,"selectedStage"),str(id)))) return false;
            }
            return true;
        }
        if (intent != "new") return false;
        // Only a native intent switch records a legacy group's selected stage.
        // Older parked legacy groups without that evidence require explicit resume.
        if (source == "legacy") {
            const auto &stages = field(s, "nativeResumeStages");
            return active || (stages.is_object() && stages.contains(str(session, "id")) &&
                str(stages, str(session, "id")) == str(s, "selectedStage"));
        }
        if (source != "new" || field(session, "ids").empty()) return false;
        if(session.contains("deckStage"))return str(session,"deckStage")==str(s,"selectedStage");
        for (const auto &id : session["ids"]) {
            const auto *entry = catalog.entry(str(id));
            if (!entry || !catalog.contains_word(str(s,"selectedStage"),str(id))) return false;
        }
        return true;
    };
    const auto &active = field(s, "session");
    if (matches(active, true)) return active;
    const auto &paused = field(s, "paused");
    for (auto it = paused.rbegin(); it != paused.rend(); ++it)
        if (matches(*it, false)) return *it;
    return nullptr;
}
void VocabularyEngine::advance(VJ &s, int64_t now, VJ &events) const {
    auto &ss = s["session"];
    if (ss.is_null())
        return;
    ss["feedback"] = ss["current"] = nullptr;
    // Finish each normal round before corrections; never alternate the exercise
    // kind merely because an earlier task is still inside its exposure gap.
    const auto rank = [](const VJ &t) {
        return (flag(t, "followup") ? 3 : 0) +
               (str(t, "kind") == "meaning" ? 0 : (str(t, "kind") == "gap" || str(t,"kind")=="partial") ? 1 : 2);
    };
    int round = 6;
    for (const auto &t : ss["tasks"]) if (!flag(t, "done")) round = std::min(round, rank(t));
    for (auto &t : ss["tasks"])
        if (!flag(t, "done") && rank(t) == round && (!flag(t, "requiresGap") || enough(s, str(t, "id"), now))) {
            ss["current"] = t["uid"];
            ss["phase"] = "tasks";
            return;
        }
    for (auto &t : ss["tasks"]) {
        if (flag(t, "done") || rank(t) != round)
            continue;
        t["done"] = true;
        if (str(t, "kind") == "gap")
            continue;
        auto id = str(t, "id"), k = str(t, "kind");
        auto &c = s["cards"][id][k];
        if (c.is_null())
            c = empty_card(now);
        c["due"] = std::max(num(c, "due"), now + Gap);
        c["pendingRelearn"] = true;
        ss["deferrals"].push_back(VJ{{"id", id}, {"kind", k}, {"due", c["due"]}});
        events.push_back(VJ{{"type", "deferred"},
                            {"wordId", id},
                            {"skill", k},
                            {"at", now},
                            {"due", c["due"]},
                            {"reason", "insufficient_distinct_intervening_words"}});
    }
    for (const auto &t : ss["tasks"]) if (!flag(t, "done")) { advance(s, now, events); return; }
    ss["phase"] = "summary";
    s["summary"] = VJ{{"sessionId", ss["id"]},
                      {"source", ss["source"]},
                      {"newCount", ss["newCount"]},
                      {"answered", ss["answered"]},
                      {"correct", ss["correct"]},
                      {"wrong", ss["wrong"]},
                      {"deferred", ss["deferrals"].size()},
                      {"finishedAt", now}};
}
void VocabularyEngine::memory(VJ &s, const VJ &t, bool correct, bool assisted, int64_t now,
                              VJ &events) const {
    auto k = str(t, "kind"), id = str(t, "id");
    if (k == "gap")
        return;
    auto &c = s["cards"][id][k];
    if (c.is_null())
        c = empty_card(now);
    if(flag(t,"followup") && num(t,"origin",-1)!=num(c,"generation"))return;
    bool initial = c["memory"].is_null(),
         early = !initial && now < num(c, "due"),
         actual = correct && !assisted, updated = false;
    VJ rating, elapsed;
    if (!flag(t, "followup") && (!early || !actual)) {
        auto days = c["lastReviewAt"].is_null()
                        ? 0
                        : std::max(int64_t(0), (now - num(c, "lastReviewAt")) / Day);
        int g = actual ? 3 : 1;
        elapsed = days;
        rating = g;
        c["memory"] = vocabulary_fsrs(c["memory"], g, int(days));
        c["lastReviewAt"] = now;
        c["fsrsDue"] = now + int64_t(vocabulary_interval(c["memory"])) * Day;
        c["due"] = actual ? c["fsrsDue"] : VJ(now + Gap);
        c["reps"] = num(c, "reps") + 1;
        auto counter = actual ? "successes" : "lapses";
        c[counter] = num(c, counter) + 1;
        c["lastRating"] = g;
        c["pendingRelearn"] = !actual;
        c["scheduler"] = "ts-fsrs@6.0.0/fsrs-6";
        c["parameterHash"] = Hash;
        updated = true;
        if (actual && !initial && days >= 1)
            addUnique(c["independentDays"], date(now));
    } else if (flag(t, "followup")) {
        c["pendingRelearn"] = !actual;
        c["due"] = actual ? std::max(num(c, "fsrsDue"), now + Gap) : now + Gap;
    }
    if(updated||flag(t,"followup"))c["generation"]=num(c,"generation")+1;
    if (!flag(t,"contentAnswer") && str(s["session"], "source") == "review")
        addUnique(day(s, now)["reviews"], id + "|" + k);
    events.push_back(VJ{{"type", "memory_update"},
                        {"wordId", id},
                        {"senseId", id},
                        {"skill", k},
                        {"taskType", k == "meaning" ? "meaning_recognition" : k == "partial" ? "partial_spelling" : k == "recall" ? "meaning_to_word_recall" : "full_spelling"},
                        {"at", now},
                        {"initial", initial},
                        {"early", early},
                        {"sameSessionCorrection", flag(t, "followup")},
                        {"updated", updated},
                        {"rating", rating},
                        {"elapsedDays", elapsed},
                        {"memory", c["memory"]},
                        {"due", c["due"]},
                        {"scheduler", str(c, "scheduler").empty() ? "legacy" : str(c, "scheduler")},
                        {"parameterHash", Hash}});
}
void VocabularyEngine::change_study_mode(VJ &s,VJ &group,const std::string &mode,int64_t now) const {
    const auto source=str(group,"source");
    if(!unfinished(group)||(source!="new"&&source!="notebook"&&source!="review")||group.contains("shopRole")||str(group,"mode")==mode)return;
    VJ seed={{"serial",0}};
    auto plan=session(seed,group["ids"],source=="review"?"new":source,now,mode,true);
    auto &tasks=group["tasks"];const auto current=str(group,"current");
    const std::string productive=mode=="partial"?"partial":mode=="full"?"recall":"";
    tasks.erase(std::remove_if(tasks.begin(),tasks.end(),[&](const VJ &t){
        return !flag(t,"done")&&str(t,"uid")!=current&&str(t,"kind")!="meaning"&&str(t,"kind")!=productive;
    }),tasks.end());
    for(auto t:plan["tasks"]){
        const auto id=str(t,"id"),k=str(t,"kind");
        if(source=="review"){
            const auto &memory=field(field(s["cards"],id),k);
            if(memory.is_null()?(k=="meaning"||!s["seen"].contains(id)):num(memory,"due")>now)continue;
        }
        if(std::none_of(tasks.begin(),tasks.end(),[&](const VJ &a){return str(a,"id")==id&&str(a,"kind")==k&&!flag(a,"followup");})){
            t["uid"]=str(group,"id")+"/mode/"+k+"/"+id;tasks.push_back(t);
        }
    }
    group["mode"]=mode;group["associative"]=true;
}
void VocabularyEngine::content_answer(VJ &s,const VJ &a,const std::string &source,VJ &corrections,int64_t now,VJ &events) const {
        const auto &answer=field(a,"answer");
        if(!answer.is_null()){
            const auto id=str(answer,"id"),skill=str(answer,"skill");
            require(catalog.entry(id)&&(skill=="meaning"||skill=="partial"||skill=="recall"),"词义或技能无效");
            const bool retry=flag(answer,"retry"),correct=flag(answer,"correct"),hint=flag(answer,"assisted");
            const auto &origins=field(corrections,id);
            VJ task={{"id",id},{"kind",skill},{"contentAnswer",true},{"followup",retry},{"origin",num(origins,skill,-1)}};
            const bool applicable=!retry||num(task,"origin")==num(field(field(field(s,"cards"),id),skill),"generation");
            if(!s["seen"].contains(id)){s["seen"][id]=now;addUnique(day(s,now)["new"],id);}
            memory(s,task,correct,hint,now,events);
            touch(s,id,now,events,source+"_answer",{{"content",str(a,"edition")},{"skill",skill},{"correct",correct},{"assisted",hint},{"retry",retry},{"applicable",applicable}});
            if(applicable&&(!correct||hint))corrections[id][skill]=num(s["cards"][id][skill],"generation");
            else if((retry||!flag(field(field(s["cards"],id),skill),"pendingRelearn"))&&corrections.contains(id))corrections[id].erase(skill);
        }
}
VJ VocabularyEngine::reduce(const VJ &original, const VJ &a, int64_t now, VJ &events) const {
    if (!(now >= 1704067200000LL && now <= 4102444800000LL &&
          now >= num(original, "lastClock") - 300000))
        throw VocabularyFault(VocabularyFailure::Clock, "时间不可信，请校准设备日期与时间");
    VJ s = original;
    events = VJ::array();
    s["lastClock"] = std::max(num(s, "lastClock"), now);
    auto &ss = s["session"];
    auto t = current(s);
    auto type = str(a, "type");
    if(type=="courseSave"){
        VJ corrections=field(s,"courseCorrections");
        if(str(a,"group")!=str(s,"courseGroup"))corrections=nullptr;
        content_answer(s,a,"course",corrections,now,events);
        s["courseCorrections"]=std::move(corrections);
        s["courseGroup"]=str(a,"group");s["courseCursor"]=a.at("run");
        return s;
    }
    if(type=="storySave"){
        const auto edition=str(a,"edition");
        auto record=field(field(s,"storyRuns"),edition);
        require(num(a,"rev",-1)==num(record,"rev"),"故事进度已更新，请重新打开");
        if(a.contains("exposed"))touch(s,str(a,"exposed"),now,events,"story_exposure");
        VJ corrections=field(record,"corrections");
        content_answer(s,a,"story",corrections,now,events);
        record["corrections"]=std::move(corrections);
        record["rev"]=num(a,"rev")+1;record["run"]=a.at("run");record["stamps"]=a.at("stamps");
        s["storyRuns"][edition]=std::move(record);
        return s;
    }
    if (type == "shopBegin" || type == "shopDecor" || type == "shopPostcard") {
        #include "bookshop_engine.inc"
    } else if (type == "begin" && a.contains("intent")) {
        const auto intent = str(a, "intent");
        require(intent == "new" || intent == "review", "背词入口无效");
        if (intent == "new") require(flag(s, "stageChosen"), "请先选择词汇阶段");
        const auto scope = a.contains("scope") ? str(a, "scope") : "all";
        require(scope == "current" || scope == "all" || scope == "notebook", "复习范围无效");
        const auto saved = resumable(s, intent, scope);
        if (!saved.is_null()) {
            if (field(saved, "id") == field(ss, "id")) return s;
            activate_session(s, saved);
            events.push_back(VJ{{"type", "session_resume"}, {"sessionId", s["session"]["id"]},
                                {"intent", intent}, {"at", now}});
        } else if (intent == "review") {
            auto candidates = flag(a, "associative") ? review_tasks(s, now, scope) : available_due(s, now);
            require(!candidates.empty(), "暂无可开始的到期复习，当前词组进度已保留");
            if (candidates.size() > 10) candidates.erase(candidates.begin() + 10, candidates.end());
            VJ ids = VJ::array();
            for (const auto &q : candidates) addUnique(ids, q["id"]);
            auto next = session(s, ids, "review", now, str(s["settings"], "mode"));
            for (auto q : candidates) {
                q["uid"] = str(next, "id") + ":" + std::to_string(next["tasks"].size());
                q["done"] = q["followup"] = false;
                q["requiresGap"] = true;
                next["tasks"].push_back(q);
            }
            activate_session(s, next);
            events.push_back(VJ{{"type", "session_start"}, {"sessionId", s["session"]["id"]},
                                {"intent", intent}, {"at", now}, {"ids", ids}, {"mode", s["session"]["mode"]}});
            advance(s, now, events);
        } else {
            const auto remaining = std::max(int64_t(0), num(s["settings"], "dailyNew") -
                size(field(field(s["days"], date(now)), "new")));
            // The native explicit action treats dailyNew as a goal, so a nearly
            // reached goal must not truncate an otherwise available five-word group.
            const auto limit = flag(a, "allowExtra") ? int64_t(5)
                                                    : std::min(int64_t(5), remaining);
            require(limit > 0, "今日目标已完成，可选择再学一组");
            auto ids = available_new(s);
            if (int64_t(ids.size()) > limit) ids.erase(ids.begin() + limit, ids.end());
            require(!ids.empty(), "本阶段新词已学完，暂停词组仍可继续");
            auto next = session(s, ids, "new", now, str(s["settings"], "mode"), flag(a, "associative"));
            activate_session(s, next);
            events.push_back(VJ{{"type", "session_start"}, {"sessionId", s["session"]["id"]},
                                {"intent", intent}, {"at", now}, {"ids", ids}, {"mode", s["session"]["mode"]}});
        }
    } else if (type == "begin") {
        require(flag(s, "stageChosen"), "请先选择词汇阶段");
        if (!ss.is_null() && str(ss, "phase") != "summary")
            return s;
        ss = nullptr;
        s["summary"] = nullptr;
        auto d = due(s, now);
        if (!d.empty()) {
            VJ ids = VJ::array();
            if (d.size() > 10)
                d.erase(d.begin() + 10, d.end());
            for (const auto &q : d)
                addUnique(ids, q["id"]);
            ss = session(s, ids, "review", now, str(s["settings"], "mode"));
            for (auto q : d) {
                q["uid"] = str(ss, "id") + ":" + std::to_string(ss["tasks"].size());
                q["done"] = q["followup"] = false;
                q["requiresGap"] = true;
                ss["tasks"].push_back(q);
            }
            advance(s, now, events);
        } else {
            auto remaining =
                std::max(int64_t(0), num(s["settings"], "dailyNew") -
                                         size(field(field(s["days"], date(now)), "new")));
            VJ ids = VJ::array();
            for (const auto &e : catalog.entries)
                if (e.eligible && e.stage == str(s, "selectedStage") && !s["seen"].contains(e.id) &&
                    int64_t(ids.size()) < std::min(int64_t(5), remaining))
                    ids.push_back(e.id);
            require(!ids.empty(),
                    remaining ? "本阶段已完成，可选择下一阶段" : "今日新词已完成，稍后再复习");
            ss = session(s, ids, "new", now, str(s["settings"], "mode"));
            events.push_back(VJ{{"type", "session_start"},
                                {"sessionId", ss["id"]},
                                {"at", now},
                                {"ids", ids},
                                {"mode", ss["mode"]}});
        }
    } else if (type == "chooseStage") {
        bool valid = false;
        for (const auto &stage : catalog.metadata["stages"])
            if (stage["id"] == a["stage"])
                valid = true;
        require(valid, "阶段不存在");
        if (!ss.is_null() && str(ss, "phase") != "summary")
            s["paused"].push_back(ss);
        ss = nullptr;
        s["summary"] = nullptr;
        s["selectedStage"] = a["stage"];
        s["stageChosen"] = true;
        events.push_back(VJ{{"type", "stage_selected"},
                            {"stage", a["stage"]},
                            {"at", now},
                            {"creditsMastery", false}});
    } else if (type == "resumePaused") {
        auto &ps = s["paused"];
        auto it = std::find_if(ps.begin(), ps.end(),
                               [&](const VJ &p) { return field(p, "id") == field(a, "id"); });
        require(it != ps.end(), "暂停任务不存在");
        auto saved = *it;
        ps.erase(it);
        if (!ss.is_null() && str(ss, "phase") != "summary")
            ps.push_back(ss);
        ss = saved;
        s["summary"] = nullptr;
    } else if (type == "learnNext" || type == "learnPrev") {
        if (a.contains("learnAt") && field(a, "learnAt") != field(ss, "learnAt"))
            return s; // An already rendered button cannot consume the following word.
        require(!ss.is_null() && str(ss, "phase") == "learn" && field(a, "sessionId") == ss["id"],
                "学习位置已改变");
        const auto at = num(ss, "learnAt");
        if (type == "learnPrev" && at == 0) return s;
        auto id = str(ss["ids"][size_t(at)]);
        touch(s, id, now, events, "presentation", VJ{{"sessionId", ss["id"]}});
        if (type == "learnPrev") {
            ss["learnAt"] = at - 1;
            return s;
        }
        if (!s["seen"].contains(id)) {
            s["seen"][id] = now;
            day(s, now)["new"].push_back(id);
            ss["newCount"] = num(ss, "newCount") + 1;
        }
        ss["learnAt"] = num(ss, "learnAt") + 1;
        if (num(ss, "learnAt") >= int64_t(ss["ids"].size()))
            advance(s, now, events);
    } else if (type == "answer") {
        require(!ss.is_null() && str(ss, "phase") == "tasks" && ss["feedback"].is_null() &&
                    !t.is_null() && field(a, "taskId") == t["uid"],
                "题目已改变，请勿重复提交");
        auto id = str(t, "id"), kind = str(t, "kind"), typed = str(a, "value");
        require(typed.size() <= 120, "输入太长");
        bool unknown = flag(a, "unknown"), correct = false;
        require(unknown || !norm(typed).empty(), "请先填写或选择答案");
        if (kind == "meaning") {
            auto opts = choices(t, ss);
            require(unknown || has(opts, typed), "答案选项无效");
            correct = !unknown && typed == (ss.contains("shopRole")?catalog.entry(id)->word:catalog.entry(id)->meaning);
        } else
            correct = !unknown && norm(typed) == norm((kind == "gap" || kind=="partial") ? str(gap(t), "answer")
                                                                    : catalog.entry(id)->word);
        bool assisted = flag(t, "hint");
        for (auto &row : ss["tasks"])
            if (row["uid"] == t["uid"]) {
                row["done"] = true;
                if(kind=="recall") {row["responseCorrect"]=correct;row["responseAssisted"]=assisted;}
            }
        ss["answered"] = num(ss, "answered") + 1;
        if(!unknown) ss["journeyAttempts"] = num(ss,"journeyAttempts") + 1;
        auto counter = correct && !assisted ? "correct" : "wrong";
        ss[counter] = num(ss, counter) + 1;
        day(s, now)["answers"] = num(day(s, now), "answers") + 1;
        events.push_back(VJ{{"type", "answer"},
                            {"sessionId", ss["id"]},
                            {"taskId", t["uid"]},
                            {"wordId", id},
                            {"senseId", id},
                            {"skill", kind},
                            {"taskType", kind == "meaning" ? "meaning_recognition"
                                         : (kind == "gap" || kind=="partial") ? "partial_spelling"
                                         : kind == "recall" ? "meaning_to_word_recall" : "full_spelling"},
                            {"at", now},
                            {"timezoneOffset", 480},
                            {"clockTrust", "device_wall_clock_checked"},
                            {"correct", correct},
                            {"assisted", assisted},
                            {"promptMode", kind=="recall" ?
                                (str(ss,"source")=="review" && num(field(field(s["cards"],id),"recall"),"successes")>0 ? "meaning_only" : "meaning_context") : "legacy"},
                            {"unknown", unknown},
                            {"value", typed},
                            {"answerVisibleBeforeResponse", flag(t, "answerRevealed") || flag(t, "lookupHint")},
                            {"sameSessionCorrection", flag(t, "followup")},
                            {"lastSameWordExposure", field(s["exposures"], id)},
                            {"scheduler", "ts-fsrs@6.0.0/fsrs-6"},
                            {"parameterHash", Hash}});
        memory(s, t, correct, assisted, now, events);
        touch(s, id, now, events, "feedback_exposure", VJ{{"sessionId", ss["id"]}});
        ss["feedback"] = VJ{{"id", id},
                            {"kind", kind},
                            {"uid", t["uid"]},
                            {"correct", correct},
                            {"assisted", assisted},
                            {"unknown", unknown},
                            {"typed", typed},
                            {"followup", flag(t, "followup")}};
        auto key = id + "|" + kind;
        if ((!correct || assisted) && kind != "gap" && !flag(t, "followup") &&
            !has(ss["retryKeys"], key)) {
            ss["retryKeys"].push_back(key);
            ss["tasks"].push_back(VJ{{"id", id},
                                     {"kind", kind},
                                     {"uid", str(ss, "id") + ":retry:" + key},
                                     {"origin",num(s["cards"][id][kind],"generation")},
                                     {"done", false},
                                     {"followup", true},
                                     {"requiresGap", true}});
        }
    } else if (type == "continue") {
        require(!ss.is_null() && !ss["feedback"].is_null() &&
                    field(a, "taskId") == ss["feedback"]["uid"],
                "请先完成当前题目");
        advance(s, now, events);
    } else if (type == "associationHint") {
        require(str(t, "kind") == "recall" && ss["feedback"].is_null() && field(a, "taskId") == t["uid"],
                "题目已改变，暂不显示线索");
        if (flag(t, "associationHint")) return s;
        for (auto &row : ss["tasks"]) if (row["uid"] == t["uid"]) {
            row["hint"] = true;
            row["associationHint"] = true;
        }
        touch(s, str(t, "id"), now, events, "association_hint", VJ{{"skill", "recall"}, {"answerVisible", false}});
    } else if (type == "hint") {
        require(!t.is_null() && ss["feedback"].is_null() && field(a, "taskId") == t["uid"],
                "当前无法提示");
        for (auto &row : ss["tasks"])
            if (row["uid"] == t["uid"]) {
                row["hint"] = true;
                row["answerRevealed"] = true;
            }
        touch(s, str(t, "id"), now, events, "hint", VJ{{"skill", t["kind"]}});
    } else if (type == "audio" || type == "lookup") {
        auto id = str(a, "id");
        const auto *entry = catalog.entry(id);
        require(entry != nullptr, "单词不存在");
        const bool matches = current_word_matches(s, entry->word);
        if (matches && (type == "lookup" || str(t, "kind") != "meaning"))
            for (auto &row : ss["tasks"])
                if (row["uid"] == t["uid"]) {
                    row["hint"] = true;
                    row[type == "audio" ? "audioHint" : "lookupHint"] = true;
                }
        const auto exposure = type == "lookup" ? "lookup_exposure" : "audio_request";
        const auto detail = type == "audio" ? VJ{{"playbackConfirmed", false}} : VJ::object();
        touch(s, id, now, events, exposure, detail);
        if (matches && str(t, "id") != id) {
            auto aliasDetail = detail;
            aliasDetail["viaWordId"] = id;
            touch(s, str(t, "id"), now, events, exposure, aliasDetail);
        }
    } else if (type == "audioResult")
        events.push_back(VJ{{"type", "audio_result"},
                            {"wordId", a["id"]},
                            {"at", now},
                            {"success", flag(a, "success")}});
    else if(type=="importWordList") {
        const auto &ids=field(a,"ids"), &definitions=field(a,"definitions");
        require(ids.is_array() && !ids.empty() && ids.size()<=200 && definitions.is_object(),"单词表格式无效");
        for(const auto &id:ids) {
            require(id.is_string() && catalog.entry(str(id)),"单词表中有未匹配词条");
            if(definitions.contains(str(id))) {
                const auto &word=definitions.at(str(id));
                require(catalog.external.contains(str(id)) && catalog.external.at(str(id))==word,"单词尚未通过词典校验");
                auto &saved=s["externalWords"];if(saved.is_null())saved=VJ::object();
                require(saved.is_object() && (!saved.contains(str(id))||saved.at(str(id))==word),"已有词义已改变，原记录保留");
                saved[str(id)]=word;
            }
            addUnique(s["notebook"],id);
        }
        events.push_back(VJ{{"type","notebook_list_add"},{"wordIds",ids},{"at",now}});
    } else if(type=="removeNotebook") {
        const auto id=str(a,"id");require(catalog.entry(id),"单词不存在");
        auto &words=s["notebook"];
        words.erase(std::remove(words.begin(),words.end(),VJ(id)),words.end());
        events.push_back(VJ{{"type","notebook_remove"},{"wordId",id},{"at",now}});
        // Membership only: retain all cards, definitions, current and paused tasks.
    }
    else if (type == "importNotebook") {
        const auto &word = field(a, "word");
        const auto id = str(word, "id");
        require(!id.empty() && str(a, "id") == id && catalog.entry(id) != nullptr &&
                    catalog.external.contains(id) && catalog.external[id] == word,
                "词典生词尚未通过校验");
        auto &externalWords = s["externalWords"];
        if (externalWords.is_null()) externalWords = VJ::object();
        require(externalWords.is_object(), "词典生词记录损坏，未重置原记录");
        require(!externalWords.contains(id) || externalWords[id] == word,
                "已有生词释义不同，原记录保持不变");
        externalWords[id] = word;
        addUnique(s["notebook"], id);
        events.push_back(VJ{{"type", "notebook_add"}, {"wordId", id}, {"at", now}});
    } else if (type == "notebook") {
        require(catalog.entry(str(a, "id")) != nullptr, "单词不存在");
        addUnique(s["notebook"], a["id"]);
        events.push_back(VJ{{"type", "notebook_add"}, {"wordId", a["id"]}, {"at", now}});
    } else if (type == "learnNotebook" && flag(a,"explicit")) {
        const auto saved = resumable(s, "notebook");
        if (!saved.is_null()) {
            if (field(saved, "id") == field(ss, "id")) return s;
            activate_session(s, saved);
        } else {
            auto ids = available_notebook(s);
            if (ids.size() > 5) ids.erase(ids.begin() + 5, ids.end());
            require(!ids.empty(), "生词已在当前词组或到期复习中");
            auto next = session(s, ids, "notebook", now, str(s["settings"], "mode"), flag(a, "associative"));
            activate_session(s, next);
        }
    } else if (type == "learnNotebook") {
        VJ ids = VJ::array();
        for (const auto &id : s["notebook"])
            if (!s["seen"].contains(str(id)) && ids.size() < 5)
                ids.push_back(id);
        require(!ids.empty(), "生词已安排到期复习");
        if (!ss.is_null() && str(ss, "phase") != "summary")
            s["paused"].push_back(ss);
        ss = session(s, ids, "notebook", now, str(s["settings"], "mode"));
        s["summary"] = nullptr;
    } else if (type == "settings") {
        auto mode = str(a, "mode");
        auto count = num(a, "dailyNew");
        require((mode == "staged" || mode == "full" || mode == "meaning" || mode=="partial") &&
                    (count == 5 || count == 10 || count == 20),
                "设置无效");
        if(flag(a,"applyStudy")){
            change_study_mode(s,s["session"],mode,now);
            for(auto &group:s["paused"])change_study_mode(s,group,mode,now);
        }
        s["settings"] = VJ{{"mode", mode}, {"dailyNew", count}};
        events.push_back(
            VJ{{"type", "settings"}, {"at", now}, {"mode", mode}, {"dailyNew", count}});
    } else if (type == "pause")
        events.push_back(VJ{{"type", "pause"}, {"at", now}, {"sessionId", field(ss, "id")}});
    else if (type == "draft") {
        require(!ss.is_null() && field(a, "taskId") == field(t, "uid"), "输入位置已改变");
        ss["draft"] = VJ{{"taskId", t["uid"]}, {"value", str(a, "value")}};
    } else
        throw std::runtime_error("未知背词操作");
    const auto &finished=field(s,"session");
    if(finished.contains("shopRole")&&str(finished,"phase")=="summary"&&
       !(str(field(original,"session"),"phase")=="summary"&&field(field(original,"session"),"id")==field(finished,"id"))){
        const auto role=str(finished,"shopRole");
        if(role=="mission"&&num(finished,"shopGroup")==num(field(s,"bookshop"),"groups")){
            const auto groups=num(finished,"shopGroup")+1;s["bookshop"]["groups"]=groups;
            if(groups==2)s["bookshop"]["finished"]=now;
        }else if(role=="visit")s["bookshop"]["visit"]=date(now);
    }
    vocabulary_award(s, original, now, events);
    return s;
}
} // namespace wordcard
