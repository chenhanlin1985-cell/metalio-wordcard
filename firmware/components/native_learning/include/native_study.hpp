#pragma once
#include "native_model.hpp"
#include "native_page.hpp"
#include "vocabulary.hpp"
#include <memory>
namespace wordcard {
class NativeStudy {
  public:
    using Write = std::function<bool(const std::string &, const std::string &)>;
    explicit NativeStudy(Write writer, const std::string &directory = "");
    bool enter(const std::string &context);
    bool active() const { return active_; }
    void leave() { active_ = false; if(vocabulary_) vocabulary_->leave(); }
    void vocabulary_enabled(bool enabled) { vocabulary_enabled_=enabled; }
    bool resume_vocabulary() { if(!vocabulary_ || !vocabulary_->resume()) return false; active_=true; return true; }
    bool vocabulary_active() const { return vocabulary_ && vocabulary_->active(); }
    void stop_for_boundary() { if(vocabulary_active()) vocabulary_->stop_audio(); else stopAudio(); }
    static bool supported(const std::string &route);
    bool dispatch(const std::string &data); // false = cold navigation boundary only
    bool input(const std::string &kind, const std::string &text = "");
    bool type(const std::string &text);
    bool edit(const std::string &text);
    bool back();
    bool playback(int serial, bool success);
    bool take_audio(std::string &path, int &serial, bool &stop, double *rate);
    std::string audio_text(const std::string &path) const;
    WebPage page();
    std::string context() const;
    std::string status() const;
    void tick(int64_t now) { model_.now = now; if(vocabulary_) vocabulary_->tick(now); }
    uint64_t calls() const { return calls_; }
    uint64_t pages() const { return pages_; }
    const std::string &error() const { return error_; }
    const NativeModel &model() const { return model_; }

  private:
    static const J &data();
    Write writer_;
    std::unique_ptr<NativeVocabulary> vocabulary_;
    NativeModel model_;
    J ui_, profile_, pendingAudio_;
    int audioSerial_ = 0, playingSerial_ = 0, audioAt_ = -1;
    std::string playingText_, playingLesson_, error_, toast_, audioNote_;
    bool active_ = false, playing_ = false, vocabulary_enabled_=true;
    uint64_t calls_ = 0, pages_ = 0;
    bool commit(const std::string &method, const J &args = J::array());
    void go(const std::string &route);
    bool dailyNext();
    void stopAudio();
    void play(const std::string &text, int step = -1, double rate = 1, bool toggle = true);
    std::string key() const;
    void batchPage(WebPage &out);
    void quizPage(WebPage &out);
    void resultPage(WebPage &out);
    void lookupPage(WebPage &out);
    void homePage(WebPage &out);
    void reviewPage(WebPage &out);
    void todayPage(WebPage &out);
    void summaryPage(WebPage &out);
    void dailyFooter(WebPage &out);
};
} // namespace wordcard
