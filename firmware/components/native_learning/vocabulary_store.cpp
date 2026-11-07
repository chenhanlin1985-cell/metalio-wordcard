#include "vocabulary.hpp"
#include <cstdio>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif
namespace wordcard {
#ifdef WORDCARD_TEST_STORAGE
bool vocabulary_storage_fault(const char *operation);
#endif
namespace {
uint32_t checksum(const std::string &s) {
    uint32_t h = 2166136261;
    for (unsigned char c : s)
        h = (h ^ c) * 16777619;
    return h;
}
bool exists(const std::string &p) {
    struct stat st{};
    return stat(p.c_str(), &st) == 0;
}
void storage_require(bool ok, const char *message) {
    if (!ok) throw VocabularyFault(VocabularyFailure::Storage,
        std::string(message) + "（错误码 " + std::to_string(errno) + "）");
}
void truncate_to(const std::string &p, int64_t offset) {
#ifdef WORDCARD_TEST_STORAGE
    if (vocabulary_storage_fault("truncate")) { errno=EIO; storage_require(false,"保存中断，日志尾部待恢复"); }
#endif
    FILE *f = std::fopen(p.c_str(), "r+b");
    storage_require(f != nullptr, "无法恢复中断的保存");
#ifdef _WIN32
    const bool ok = _chsize_s(_fileno(f), offset) == 0 && _commit(_fileno(f)) == 0;
#else
    const bool ok = ftruncate(fileno(f), off_t(offset)) == 0 && fsync(fileno(f)) == 0;
#endif
    std::fclose(f);
    storage_require(ok, "保存中断，无法恢复日志尾部；原记录保留");
}
void durable(const std::string &p, const std::string &bytes, bool append = false, bool *started = nullptr) {
    FILE *f = std::fopen(p.c_str(), append ? "ab" : "wb");
    storage_require(f != nullptr, "SD 无法打开保存文件");
    if (started) *started = true;
#ifdef WORDCARD_TEST_STORAGE
    if (append && vocabulary_storage_fault("partial-append")) {
        std::fwrite(bytes.data(), 1, bytes.size()/2, f); std::fflush(f); std::fclose(f);
        errno=EIO; storage_require(false,"模拟写入中断");
    }
#endif
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size() && std::fflush(f) == 0;
#ifdef _WIN32
    if (ok)
        ok = _commit(_fileno(f)) == 0;
#else
    if (ok)
        ok = fsync(fileno(f)) == 0;
#endif
    ok = std::fclose(f) == 0 && ok;
    storage_require(ok, "SD 未能完成写入");
}
std::string envelope(const VJ &v) {
    auto data = v.dump();
    return VJ{{"data", data}, {"checksum", checksum(data)}}.dump() + "\n";
}
VJ decode(const std::string &raw) {
    auto e = VJ::parse(raw);
    auto data = str(e, "data");
    require(uint64_t(num(e, "checksum")) == checksum(data), "背词记录校验失败，未重置进度");
    return VJ::parse(data);
}
std::string read_all(const std::string &p) {
    std::ifstream in(p, std::ios::binary);
    require(bool(in), "背词记录无法读取");
    return std::string(std::istreambuf_iterator<char>(in), {});
}
} // namespace
NativeVocabulary::NativeVocabulary(const std::string &directory) : engine_(catalog_) {
    if (directory.empty())
        return;
    store_ = directory + "/vocabulary";
    // QA profiles have their own state; all profiles read the same immutable resources.
    const auto marker = directory.rfind("/learning-web");
    if (marker == std::string::npos)
        return;
    auto resources = directory.substr(0, marker) + "/vocabulary";
    if (!exists(resources + "/catalog.json"))
        return;
    available_ = true;
    try {
        require(catalog_.load(resources), "词库资源无法读取，请检查 SD 卡");
    } catch (const std::exception &e) {
        capture_error(e, VocabularyFailure::Data);
    }
}
void NativeVocabulary::capture_error(const std::exception &e, VocabularyFailure fallback) {
    // Called only from a catch handler. Typed failures work without RTTI on S31.
    failure_ = fallback;
    try { throw; } catch (const VocabularyFault &f) { failure_ = f.kind; } catch (...) {}
    error_ = e.what(); recoveryHome_ = false;
    if (failure_ == VocabularyFailure::Storage) storageBlocked_ = true;
    if (storageBlocked_ && failure_ != VocabularyFailure::Clock) failure_ = VocabularyFailure::Storage;
    ++viewSerial_;
}
void NativeVocabulary::dismiss_error(bool home) {
    error_.clear(); note_.clear(); stop_audio(); ++viewSerial_;
    // No mutation, no synthesized progress and no retry from a navigation action.
    if (!loaded_) recoveryHome_ = true;
    if (home) { backStack_.clear(); route_ = "home"; }
}
bool NativeVocabulary::retry_recovery() {
    try {
        if (failure_ == VocabularyFailure::Clock) {
            if (!(now_ >= 1704067200000LL && now_ <= 4102444800000LL &&
                  (!loaded_ || now_ >= num(state_, "lastClock") - 300000)))
                throw VocabularyFault(VocabularyFailure::Clock, "日期仍未校准，请核对当前时间与上次记录时间");
        }
        if (storageBlocked_) {
            // This only probes storage. It never replays an answer or an import.
            if (loaded_) {
                // A reinserted/replaced card must contain this exact committed
                // history. Never append RAM state onto an empty or older card.
                auto snapshot=store_+"/state.json";
                auto saved=decode(read_all(exists(snapshot)?snapshot:snapshot+".bak"));
                auto restored=saved.at("state"); auto offset=num(saved,"journalOffset");
                std::ifstream journal(store_+"/events.ndjson",std::ios::binary);
                if (journal) {
                    journal.seekg(0,std::ios::end);const auto end=int64_t(journal.tellg());
                    const auto limit=pendingTail_>=0?pendingTail_:end;
                    require(offset>=0 && offset<=limit && limit<=end,"存储中的记录与当前学习不一致，请换回原 SD 卡");
                    journal.seekg(offset);std::string line;
                    while(int64_t(journal.tellg())<limit && std::getline(journal,line)) {
                        require(!journal.eof() && int64_t(journal.tellg())<=limit,"记录尾部不完整，未继续写入");
                        auto frame=decode(line);require(num(frame,"revision")==num(restored,"revision")+1,"记录顺序不一致，未继续写入");
                        restored=restored.patch(frame.at("patch"));
                    }
                } else require(offset==0 && pendingTail_<=0,"答题日志无法读取，未继续写入");
                require(restored==state_,"存储中的进度与当前学习不一致，请保留原卡和文件");
                if(pendingTail_>=0) {truncate_to(store_+"/events.ndjson",pendingTail_);pendingTail_=-1;}

                auto probe = store_ + "/.recovery-check";
                durable(probe, "wordcard-storage-check\n");
                storage_require(read_all(probe) == "wordcard-storage-check\n", "SD 写入后校验失败");
                storage_require(std::remove(probe.c_str()) == 0, "SD 无法清理检查文件");
            }
        }
        error_.clear();
        if (!loaded_) {
            const auto marker = store_.rfind("/learning-web");
            if (marker != std::string::npos) require(catalog_.load(store_.substr(0, marker) + "/vocabulary"), "词库资源无法读取，请检查 SD 卡");
            load(migration_);
        }
        storageBlocked_ = false; recoveryHome_ = false; ++viewSerial_;
        note_ = "检查通过，回到原位置；刚才未完成的操作请重新进行";
        return true;
    } catch (const std::exception &e) { capture_error(e, VocabularyFailure::Data); return false; }
}
void NativeVocabulary::save_checkpoint() {
    struct stat st{};
    const auto journal = store_ + "/events.ndjson";
    int64_t offset = stat(journal.c_str(), &st) == 0 ? int64_t(st.st_size) : 0;
    auto p = store_ + "/state.json", temp = p + ".tmp", bak = p + ".bak";
    durable(temp, envelope(VJ{{"state", state_}, {"journalOffset", offset}}));
    bool present = exists(p);
    if (present) {
        if (exists(bak))
            storage_require(std::remove(bak.c_str()) == 0, "无法保留记录备份");
        storage_require(std::rename(p.c_str(), bak.c_str()) == 0, "无法保存记录备份");
    }
    if (std::rename(temp.c_str(), p.c_str()) != 0) {
        if (present)
            std::rename(bak.c_str(), p.c_str());
        throw VocabularyFault(VocabularyFailure::Storage, "记录快照未保存");
    }
}
void NativeVocabulary::load(const VJ &legacy) {
    require(error_.empty(), error_.c_str());
    if (!exists(store_)) {
#ifdef _WIN32
        storage_require(_mkdir(store_.c_str()) == 0, "无法创建背词记录目录");
#else
        storage_require(mkdir(store_.c_str(), 0775) == 0, "无法创建背词记录目录");
#endif
    }
    auto p = store_ + "/state.json", journal = store_ + "/events.ndjson";
    int64_t offset = 0;
    if (exists(p) || exists(p + ".bak")) {
        auto frame = decode(read_all(exists(p) ? p : p + ".bak"));
        state_ = frame.at("state");
        offset = num(frame, "journalOffset");
    } else {
        require(!exists(journal), "已有答题日志但快照缺失，未新建空白进度");
        if (!(now_ >= 1704067200000LL && now_ <= 4102444800000LL))
            throw VocabularyFault(VocabularyFailure::Clock, "请先校准设备日期与时间");
        state_ = engine_.create(now_, legacy);
        save_checkpoint();
    }
    require(num(state_, "schema") == 1 && field(state_, "cards").is_object() &&
                field(state_, "seen").is_object() && field(state_, "paused").is_array(),
            "背词快照格式错误");
    std::ifstream in(journal, std::ios::binary);
    if (in) {
        in.seekg(0, std::ios::end);
        require(offset >= 0 && offset <= int64_t(in.tellg()), "背词日志长度异常");
        in.seekg(offset);
        std::string line;
        auto complete = offset;
        while (std::getline(in, line)) {
            if (in.eof()) {
                // Only an unterminated, uncommitted tail may be repaired. Keep
                // its exact bytes; a complete checksum-invalid record blocks.
                in.close();
                durable(store_ + "/interrupted-" + str(state_, "revision") + ".tail", line);
                truncate_to(journal, complete);
                break;
            }
            auto frame = decode(line);
            require(num(frame, "revision") == num(state_, "revision") + 1, "逐题记录顺序异常");
            state_ = state_.patch(frame.at("patch"));
            require(num(state_, "revision") == num(frame, "revision"), "逐题记录版本异常");
            complete = int64_t(in.tellg());
        }
    }
    const auto &external = field(state_, "externalWords");
    require(external.is_null() || external.is_object(), "生词词典记录损坏");
    if (external.is_object())
        for (auto it = external.begin(); it != external.end(); ++it) {
            require(it.key() == str(it.value(), "id"), "生词词典ID不一致");
            catalog_.install_external(it.value());
        }
    for (auto it = state_["cards"].begin(); it != state_["cards"].end(); ++it) {
        require(catalog_.entry(it.key()) != nullptr, "记录中的词条未安装");
        for (const auto &c : it.value()) {
            require(field(c, "due").is_number(), "到期记录损坏");
            if (!field(c, "memory").is_null())
                vocabulary_fsrs(c["memory"], 3, 0);
        }
    }
    loaded_ = true;
}
bool NativeVocabulary::enter(const VJ &legacy, int64_t now) {
    if (!available_)
        return false;
    now_ = now;
    entered_=true;
    active_ = true;
    if (!loaded_)
        migration_ = legacy;
    if (!loaded_ && error_.empty())
        try {
            load(legacy);
        } catch (const std::exception &e) {
            capture_error(e, VocabularyFailure::Data);
        }
    route_ = "home";
    return true;
}
bool NativeVocabulary::expose_lookup(const std::string &word) {
    if (!loaded_) {
        error_ = "背词记录未加载；请检查SD卡";
        return false;
    }
    if (!engine_.current_word_matches(state_, word)) return true;
    const auto task = engine_.current(state_);
    if (flag(task, "lookupHint")) return true;
    // The displayed spelling can come from a curated, legacy or full-dictionary
    // entry. Record assistance on the current task, without merging word IDs.
    return commit(VJ{{"type", "lookup"}, {"id", task["id"]}, {"taskId", task["uid"]}});
}
bool NativeVocabulary::expose_candidates(const std::string &query) {
    auto lower = [](std::string text) {
        for (char &c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };
    const auto prefix = lower(query);
    if (prefix.empty()) return true;
    int count = 0;
    // Match the same first two prefix suggestions shown by page(). This runs
    // before publishing an edited query, so a failed save cannot expose an answer.
    for (const auto &entry : catalog_.entries) {
        if (lower(entry.word).rfind(prefix, 0) != 0) continue;
        if (count++ >= 2) break;
        if (!expose_lookup(entry.word)) return false;
    }
    return true;
}
bool NativeVocabulary::commit(const VJ &a) {
    try {
        require(loaded_, "背词记录未加载；请检查SD卡");
        if (storageBlocked_) throw VocabularyFault(VocabularyFailure::Storage, "存储尚未恢复，请先检查存储");
        VJ events;
        auto next = engine_.reduce(state_, a, now_, events);
        const auto &beforeSession = field(state_, "session");
        const auto &afterSession = field(next, "session");
        bool audioContextChanged = false;
        for (const auto *key : {"id", "phase", "current", "feedback", "learnAt"})
            if (field(beforeSession, key) != field(afterSession, key))
                audioContextChanged = true;
        next["revision"] = num(state_, "revision") + 1;
        VJ frame{{"revision", next["revision"]},
                 {"at", now_},
                 {"action", a},
                 {"events", events},
                 {"patch", VJ::diff(state_, next)}};
        const auto journal = store_ + "/events.ndjson";
        struct stat st{};
        const int64_t committed = stat(journal.c_str(), &st) == 0 ? int64_t(st.st_size) : 0;
        bool appendStarted = false;
        try {
            durable(journal, envelope(frame), true, &appendStarted);
        } catch (...) {
            if (appendStarted && exists(journal)) {
                pendingTail_ = committed;
                truncate_to(journal, committed);
                pendingTail_ = -1;
            }
            throw;
        }
        state_ = std::move(next);
        // A failed append must leave the old task and its playback intact.
        // Drafts, hints and notebook changes keep the same audio context.
        if (audioContextChanged) {
            stop_audio();
            note_.clear();
        }
        error_.clear();
        ++viewSerial_;
        if (num(state_, "revision") % 64 == 0) {
            try {
                save_checkpoint();
            } catch (const std::exception &) {
                note_ = "本题已保存，快照将在稍后重试";
            }
        }
        return true;
    } catch (const std::exception &e) {
        capture_error(e);
        return false;
    }
}
std::string NativeVocabulary::status() const {
    auto t = loaded_ ? engine_.current(state_) : VJ();
    const auto &s = field(state_, "session");
    VJ result{{"route", route_},
              {"engine", "native-vocabulary-fsrs6"},
              {"revision", num(state_, "revision")},
              {"selectedStage", field(state_, "selectedStage")},
              {"seen", field(state_, "seen").size()},
              {"batch", VJ{{"phase", field(s, "phase")},
                           {"source", field(s, "source")},
                           {"id", field(t, "id")},
                           {"kind", field(t, "kind")},
                           {"learnAt", field(s, "learnAt")},
                           {"answered", field(s, "answered")},
                           {"checked", field(s, "feedback")},
                           {"typed", field(field(s, "draft"), "value")}}},
              {"courseError", error_},
              {"profileError", ""}};
    if (!lastImportedId_.empty()) {
        result["lastImportedWord"] = lastImportedWord_;
        result["lastImportedId"] = lastImportedId_;
    }
    return result.dump();
}
} // namespace wordcard
