#pragma once
#include "wordcard.hpp"
#include <array>
#include <set>

namespace wordcard {
enum class StudyMode { Visual = 0, Listening = 1, Reverse = 2 };
struct DailyRecord {
    uint32_t attempts = 0, fresh = 0, reviewed = 0, listening = 0, recalled = 0;
};
struct Lesson {
    uint32_t serial = 0, day = 0, position = 0, initial = 0;
    std::string book, group;
    StudyMode mode = StudyMode::Visual;
    bool revealed = false, paused = true, difficult = false, favorites = false;
    std::vector<std::string> queue;
    bool active() const { return !queue.empty() && position < queue.size(); }
};
struct ReviewEvent {
    uint32_t serial = 0, lesson = 0, day = 0, position = 0, queue_size = 0, previous_day = 0;
    StudyMode mode = StudyMode::Visual;
    Rating rating = Rating::Again;
    std::string id;
    Progress before;
    bool existed = false, first_today = false, fresh = false, dated = false;
};
struct LearningState {
    std::array<ProgressMap, 3> memory;
    std::array<std::map<std::string, uint32_t>, 3> last_day;
    std::map<uint32_t, DailyRecord> days;
    std::set<std::string> favorites;
    std::vector<ReviewEvent> events; // Most recent 512; aggregate days and memory are retained.
    Lesson lesson;
    uint32_t sequence = 0;
    unsigned new_limit = 10, lesson_size = 10;
    bool onboarded = false, haptics = true;
    std::string player_book, player_group, player_id;
};
struct PlanCounts {
    size_t due = 0, unseen = 0, fresh = 0, today_new = 0;
};
struct CourseProgress {
    size_t total = 0, learned = 0, remaining = 0, sentences = 0;
};
bool is_sentence(const Card &card);
CourseProgress course_progress(const std::vector<Card> &, const ProgressMap &, const std::string &group);
class LearningSystem {
  public:
    LearningState state;
    void bind(const std::vector<Card> *cards);
    PlanCounts counts(uint32_t day, const std::string &group = "",
                      StudyMode mode = StudyMode::Visual) const;
    bool begin(const std::string &book, const std::string &group, uint32_t day,
               StudyMode mode = StudyMode::Visual, bool difficult = false,
               const std::set<std::string> *audio = nullptr, bool favorites = false,
               const std::set<std::string> *selection = nullptr);
    // Explicit user choice to finish unseen course items; never changes the daily plan.
    bool begin_remaining(const std::string &book, const std::string &group, uint32_t day);
    const Card *current() const;
    bool reveal();
    bool rate(Rating rating, uint32_t day, bool dated, uint32_t expected_position);
    bool undo();
    bool can_undo() const;
    void pause() { state.lesson.paused = true; }
    void resume() { state.lesson.paused = false; }
    void end() { state.lesson = {}; }
    size_t distinct_in_lesson() const;
    size_t difficult_in_lesson() const;
    bool validate_lesson(const std::string &book) const;
    bool validate_lesson_audio(const std::set<std::string> &available) const;

  private:
    const std::vector<Card> *cards_ = nullptr;
    std::map<std::string, size_t> index_;
};
bool save_learning_state(const std::string &, const LearningState &, std::string &error);
bool load_learning_state(const std::string &, LearningState &, std::string &error);
struct CatalogBook {
    std::string id, filename, name, description;
    size_t total = 0;
};
bool load_catalog(std::istream &, std::vector<CatalogBook> &, std::string &error);
bool valid_date(int year, int month, int day);
} // namespace wordcard
