#pragma once
#include "native_json.hpp"
#include "native_page.hpp"
#include <functional>
#include <unordered_map>
namespace wordcard {
using VJ = StudyJson;
enum class VocabularyFailure { Action, Clock, Storage, Data };
struct VocabularyFault : std::runtime_error {
    VocabularyFailure kind;
    VocabularyFault(VocabularyFailure k, const std::string &message) : std::runtime_error(message), kind(k) {}
};
VJ vocabulary_fsrs(const VJ &memory, int rating, int elapsed);
int vocabulary_interval(const VJ &memory);
class VocabularyCatalog {
  public:
    struct Entry {
        std::string id, word, meaning, pos, stage;
        bool eligible;
        long offset;
        size_t length;
    };
    bool load(const std::string &directory);
    VJ get(const std::string &id) const;
    const Entry *entry(const std::string &id) const;
    void install_external(const VJ &word);
    void remove_external(const std::string &id);
    VJ external = VJ::object();
    VJ metadata;
    std::vector<Entry> entries;
    std::string directory;

  private:
    std::unordered_map<std::string, size_t> index_;
    mutable std::unordered_map<std::string, VJ> cache_;
};
class VocabularyEngine {
  public:
    explicit VocabularyEngine(const VocabularyCatalog &c) : catalog(c) {}
    const VocabularyCatalog &catalog;
    VJ create(int64_t now, const VJ &legacy = VJ()) const;
    VJ reduce(const VJ &original, const VJ &action, int64_t now, VJ &events) const;
    VJ due(const VJ &s, int64_t now) const;
    // Explicit native entry points reserve unfinished cards across parked groups.
    VJ available_due(const VJ &s, int64_t now) const;
    VJ available_new(const VJ &s) const;
    VJ available_notebook(const VJ &s) const;
    VJ continue_preview(const VJ &s, int64_t now) const;
    bool current_word_matches(const VJ &s, const std::string &word) const;
    VJ resumable(const VJ &s, const std::string &intent) const;
    VJ current(const VJ &s) const;
    VJ gap(const VJ &task) const;
    VJ choices(const VJ &task, const VJ &session) const;
    static std::string date(int64_t now);
    static VJ empty_card(int64_t now);

  private:
    mutable std::unordered_map<std::string, VJ> choiceCache_;
    VJ session(VJ &s, const VJ &ids, const std::string &source, int64_t now,
               const std::string &mode) const;
    void touch(VJ &s, const std::string &id, int64_t now, VJ &events, const std::string &type,
               const VJ &detail = VJ::object()) const;
    bool enough(const VJ &s, const std::string &id, int64_t now) const;
    void advance(VJ &s, int64_t now, VJ &events) const;
    void memory(VJ &s, const VJ &task, bool correct, bool assisted, int64_t now, VJ &events) const;
};
class NativeVocabulary {
  public:
    explicit NativeVocabulary(const std::string &learningDirectory);
    bool available() const { return available_; }
    bool enter(const VJ &legacy, int64_t now);
    bool active() const { return active_; }
    void leave() { active_ = false; }
    bool resume() { if(!entered_) return false; active_=true; return true; }
    void tick(int64_t now) { now_ = now; }
    bool dispatch(const std::string &data);
    bool type(const std::string &text);
    bool edit(const std::string &keys);
    bool input(const std::string &kind, const std::string &text);
    bool back();
    bool import_word(const VJ &word);
    bool expose_lookup(const std::string &word);
    bool prepare_word_list();
    bool accept_word_list(uint64_t token);
    void stop_audio();
    bool take_audio(std::string &path, int &serial, bool &stop, double *rate);
    std::string audio_text(const std::string &path) const;
    bool playback(int serial, bool success);
    WebPage page();
    std::string status() const;

  private:
    VocabularyCatalog catalog_;
    VocabularyEngine engine_;
    std::string store_, route_ = "home", return_ = "home", lookup_, query_, error_, note_,
                        audioPath_, audioText_;
    struct BackFrame { std::string route, lookup, query; };
    std::vector<BackFrame> backStack_;
    std::string stageChoice_, lastImportedWord_, lastImportedId_;
    VJ state_, pendingAudio_, migration_;
    VJ wordList_ = VJ::array(), wordListIssues_ = VJ::array();
    uint64_t wordListToken_ = 0;
    size_t wordListExisting_ = 0;
    bool wordListReady_ = false;
    bool entered_=false, available_ = false, active_ = false, loaded_ = false, shift_ = false, symbols_ = false;
    int audioSerial_ = 100000;
    int64_t now_ = 0;
    uint64_t viewSerial_ = 0;
    VJ continuationPreview_;
    uint64_t continuationRevision_ = ~uint64_t(0);
    int64_t continuationClock_ = -1;
    VocabularyFailure failure_ = VocabularyFailure::Action;
    bool storageBlocked_ = false, recoveryHome_ = false;
    int64_t pendingTail_ = -1;
    std::string wordListError_;
    void capture_error(const std::exception &e, VocabularyFailure fallback = VocabularyFailure::Action);
    bool retry_recovery();
    void dismiss_error(bool home = false);
    WebPage recovery_page();
    bool commit(const VJ &action);
    bool expose_candidates(const std::string &query);
    void load(const VJ &legacy);
    void save_checkpoint();
    void go(const std::string &route);
    void navigate(const std::string &route);
    std::string back_label() const;
    void play(const std::string &id, bool example);
};
} // namespace wordcard
