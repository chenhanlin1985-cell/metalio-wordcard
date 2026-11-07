#pragma once
#include "learning_system.hpp"
#include "dictionary.hpp"
#include "speech_policy.hpp"
#include "tts_test_model.hpp"
#include "wordcard.hpp"
#include "guided_learning.hpp"
#include "assessment.hpp"
#include "learning_web.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
namespace wordcard {
enum class Screen {
    Home,
    Library,
    Groups,
    Study,
    Done,
    SelfTest,
    Settings,
    Welcome,
    Plan,
    Date,
    Practice,
    Profile,
    Pause,
    Player,
    Stats,
    Resources,
    Dictionary,
    DictionaryResults,
    DictionaryEntry,
    Vocabulary,
    TtsTest,
    UsbDisk,
    PowerOff,
    GuidedCourses,
    GuidedLesson,
    GuidedPause,
    GuidedDone,
    GuidedListen,
    RecallHome,
    RecallLesson,
    RecallDone,
    LearningStart,
    CourseDetail,
    AssessmentHome, AssessmentQuestion, AssessmentSound, AssessmentPause,
    AssessmentReport, AssessmentSkip, AssessmentRestart,
    WebLearning, Booting, WebTools, WebImportConfirm, WebPrompt, DeviceSettings, MoreLearning
};
enum class Action {
    None,
    Home,
    Library,
    Deck,
    Groups,
    Group,
    Page,
    Start,
    Difficult,
    Reveal,
    Speak,
    Rate,
    SelfTest,
    Settings,
    Back,
    Light,
    Brightness,
    Volume,
    Vibe,
    Tone,
    Mic,
    Refresh,
    AnswerPage,
    Plan,
    NewLimit,
    LessonSize,
    SavePlan,
    Date,
    DateField,
    DateStep,
    SaveDate,
    SkipDate,
    Practice,
    Profile,
    Resume,
    EndLesson,
    Undo,
    Favorite,
    Favorites,
    Player,
    PlayerToggle,
    PlayerStep,
    Repeat,
    Record,
    Replay,
    Stats,
    Haptics,
    Resources,
    Reload,
    ListenCourse,
    ContinueCourse,
    Dictionary,
    DictionaryKey,
    DictionaryErase,
    DictionaryClear,
    DictionaryShift,
    DictionarySymbols,
    DictionarySearch,
    DictionarySelect,
    DictionaryAdd,
    DictionaryPage,
    Vocabulary,
    TtsTest,
    TtsMode,
    TtsPhrase,
    TtsRun,
    TtsListen,
    TtsStop,
    UsbDisk,
    UsbStart,
    UsbExit,
    DictionaryRecent,
    SpeechStop,
    VocabularyFilter,
    VocabularyPage,
    VocabularySelect,
    VocabularyReview,
    GuidedCourses,
    GuidedTrack,
    GuidedCourse,
    GuidedBegin,
    GuidedChoose,
    GuidedRemove,
    GuidedCheck,
    GuidedNext,
    GuidedSkip,
    GuidedResume,
    GuidedPause,
    GuidedAbandon,
    GuidedListen,
    GuidedListenStep,
    LegacyPractice,
    RecallHome,
    RecallBegin,
    RecallResume,
    RecallChoose,
    RecallCheck,
    RecallNext,
    RecallSkip,
    RecallHint,
    GuidedAdd,
    LearningStart,
    LearningInterest,
    LearningLevel,
    LearningApply,
    CurriculumContinue,
    CurriculumLevel,
    CurriculumPage,
    AssessmentHome, AssessmentBegin, AssessmentRestart, AssessmentChoose,
    AssessmentSubmit, AssessmentUnknown, AssessmentPlay, AssessmentSoundConfirm,
    AssessmentPage, AssessmentPause, AssessmentResume, AssessmentSkip,
    AssessmentSkipConfirm, AssessmentReport, AssessmentSkill, AssessmentInterest,
    AssessmentRecommend, AssessmentSave,
    WebAction, WebPage, WebLearning, DeviceMenu, WebImport, DeviceSettings, MoreLearning, LearningSettings, LegacyCourses, StorageCheck
};
struct Event {
    Action action = Action::None;
    int value = 0;
};
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
struct Button {
    Rect rect;
    Event event;
    std::string label;
    bool enabled = true, primary = false;
    // Physical fill, independent of the action's visual importance.
    bool dark_fill = false;
};
struct BookInfo {
    std::string name, description;
    size_t total = 0;
};
struct View {
    bool more_learning_scope=false;
    WebPage web;
    int web_page=0;
    std::string boot_stage;
    AssessmentView assessment;
    bool guided_enabled=false,legacy_home=false,legacy_practice=false;
    GuidedView guided;
    int guided_sentence=0;
    RecallCounts recall_counts;
    RecallSession recall;
    const RecallRecord *recall_card=nullptr;
    bool recall_heard=false,recall_ready=false,recall_typing=false,recall_listening=false;
    int power_phase=0; // 0 release key, 1 saving, 2 safe shutdown
    int usb_phase=0; // intro, preparing, active, error
    std::string usb_error;
    int tts_phrase=0;TtsTestMode tts_mode=TtsTestMode::AmySmall;TtsTestPhase tts_phase=TtsTestPhase::Idle;
    std::array<TtsTestResult,tts_mode_count> tts_results{};
    bool tts_test_ready=false,tts_preview=false;
    wordcard::SpeechSource audio_source = wordcard::SpeechSource::None;
    bool tts_available = false;
    Screen screen = Screen::Home;
    std::string deck = "KET", group, notice, storage = "", clock = "", audio_status;
    std::string book_name, date_label;
    std::vector<BookInfo> books;
    std::vector<std::string> groups, diagnostics;
    const Card *card = nullptr;
    size_t total = 0, due = 0, position = 0, count = 0;
    size_t learned = 0, review_due = 0, unseen = 0;
    CourseProgress course;
    bool player_sentences = false;
    std::string dictionary_query;
    std::vector<DictionaryMatch> dictionary_matches;
    bool dictionary_ready = false, dictionary_added = false, dictionary_shift = false,
         dictionary_symbols = false;
    size_t dictionary_count = 0, personal_count = 0;
    std::vector<std::string> dictionary_recent;
    bool entry_from_vocabulary = false, personal_book = false, personal_resume = false;
    bool vocabulary_review_pending = false;
    bool dictionary_native_lookup = false;
    std::string dictionary_native_added_id;
    bool personal_readable = true;
    std::string personal_error;
    VocabularyFilter vocabulary_filter = VocabularyFilter::All;
    VocabularyPage vocabulary;
    size_t personal_planned_new = 0;
    int selected_book = 0;
    int page = 0, answer_page = 0, brightness = 40, cct = 3, volume = 50;
    bool revealed = false, audio_available = false, audio_busy = false, audio_pending = false,
         writable = false;
    bool resume_available = false, can_undo = false, favorite = false, haptics = true,
         date_valid = false, player_running = false, recording = false, clip_available = false,
         onboarding = false, repeat_mode = false;
    StudyMode study_mode = StudyMode::Visual;
    size_t planned_new = 0, today_new = 0, lesson_distinct = 0, lesson_difficult = 0,
           audio_count = 0, favorite_count = 0;
    unsigned new_limit = 10, lesson_size = 10;
    std::array<int, 5> date_fields{2026, 1, 1, 12, 0};
    int date_field = 0;
    std::vector<std::string> stats, resources;
};
class Font {
  public:
    bool init(const uint8_t *bytes, size_t length);
    struct Glyph {
        const uint8_t *bitmap = nullptr;
        uint16_t width = 0, height = 0, advance = 18;
        uint16_t font_size = 30;
        int16_t x = 0, y = 0;
    };
    Glyph glyph(uint32_t codepoint, uint16_t font_size = 30) const;
    struct RasterGlyph {
        uint64_t key = 0;
        int width = 0, height = 0, left = 0, top = 0, advance = 0;
        const uint8_t *coverage = nullptr; // column-major, exact 0..15 coverage
    };
    const RasterGlyph *raster(uint32_t codepoint, int pixels) const;
    bool ready() const { return data_ != nullptr; }
    void fallback(const Font *font) { fallback_ = font; cache_.clear(); raster_.reset(); }
    // Immutable font data is shared, mutable glyph maps are worker-owned.
    Font detached(const Font *fallback=nullptr) const { Font f;f.data_=data_;f.size_=size_;f.count_=count_;f.index_=index_;f.fallback_=fallback;return f; }

  private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
    uint32_t count_ = 0;
    uint64_t index_ = 0;
    mutable std::map<uint64_t, Glyph> cache_;
    // One bounded arena per owner, including metadata. On the device this is
    // explicitly PSRAM, so small glyph allocations cannot consume internal RAM.
    struct RasterArena {
        static constexpr size_t capacity = 256 * 1024;
        size_t count = 0, used = 0;
        RasterGlyph entries[256];
        uint8_t pixels[capacity];
    };
    struct RasterDelete { void operator()(RasterArena *) const; };
    mutable std::unique_ptr<RasterArena, RasterDelete> raster_;
    const Font *fallback_ = nullptr;
};
class Canvas {
  public:
    static constexpr int width = 684, height = 1216;
    static constexpr size_t bytes = 1216 * 684 / 2;
    Canvas(uint8_t *framebuffer, const Font &font, std::function<bool()> cancel={}) : fb_(framebuffer), font_(font), cancel_(std::move(cancel)) {}
    bool cancelled() const { return cancel_&&cancel_(); }
    void clear();
    void pixel(int x, int y, bool black = true);
    void gray(int x,int y,uint8_t level);
    uint8_t shade(int x,int y) const;
    bool black(int x, int y) const;
    void fill(Rect rect, bool black = true);
    void border(Rect rect, int weight = 2);
    int text_width(const std::string &text, int scale = 1) const;
    void text(int x, int y, const std::string &text, int scale = 1, bool black = true);
    void centered(int y, const std::string &text, int scale = 1, bool black = true);
    std::string ellipsize(const std::string &text, int max_width, int scale = 1) const;
    std::vector<std::string> wrap(const std::string &text, int max_width, int scale = 1) const;
    // Device-sized typography, independent of the legacy 30/60/90 scale API.
    int text_width_px(const std::string &, int pixels) const;
    void text_px(int x,int y,const std::string &,int pixels);
    std::vector<std::string> wrap_px(const std::string &,int max_width,int pixels) const;

  private:
    uint8_t *fb_;
    const Font &font_;
    std::function<bool()> cancel_;
};
class UI {
  public:
    void render(Canvas &, const View &);
    bool render_input(Canvas &, const View &);
    Event hit(int x, int y) const;
    const std::vector<Button> &buttons() const { return buttons_; }
    int answer_pages() const { return answer_pages_; }

  private:
    std::vector<Button> buttons_;
    int answer_pages_ = 1;
    bool keyboard_=false,keyboard_shift_=false,keyboard_symbols_=false,keyboard_search_=false,keyboard_recall_=false;
    bool guided_enabled_=false,more_learning_scope_=false;
    int keyboard_recent_=0;
    WebPage painted_web_input_;
    Screen painted_input_screen_=Screen::Home;
    std::string painted_input_notice_;
    std::string painted_dictionary_query_;
    std::vector<std::string> painted_dictionary_recent_;
    void button(Canvas &, Rect, Action, const std::string &, int value = 0, bool primary = false,
                bool enabled = true);
    void home(Canvas &, const View &);
    void library(Canvas &, const View &);
    void tabs(Canvas &, Action selected);
    void row(Canvas &, Rect, Action, const std::string &, int value = 0);
    void learning_pages(Canvas &, const View &);
    void dictionary_pages(Canvas &, const View &);
    void tts_test_page(Canvas &, const View &);
    void usb_page(Canvas &, const View &);
    void guided_pages(Canvas &, const View &);
    void recall_pages(Canvas &, const View &);
    void assessment_pages(Canvas &, const View &);
    void web_pages(Canvas &, const View &);
};
} // namespace wordcard
