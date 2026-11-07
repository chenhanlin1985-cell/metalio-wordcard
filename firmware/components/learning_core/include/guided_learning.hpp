#pragma once
#include "wordcard.hpp"
#include "learning_review.hpp"
#include <array>
#include <deque>
namespace wordcard {
// Independent, versioned progress: old self-rated cards are never converted to scores.
inline constexpr int guided_course_count = 20, guided_node_count = 6;
inline constexpr unsigned guided_complete_mask = (1u << guided_node_count) - 1;
inline constexpr const char *guided_tracks[] = {"日常英语", "科技英语", "工作英语", "基础英语"};
inline constexpr const char *guided_levels[] = {"A1 起步", "A2 应用", "B1 进阶"};
inline constexpr const char *guided_nodes[] = {"认识关键词", "继续学词", "读懂表达", "听懂表达", "读短对话", "听短对话"};
struct GuidedItem {
    Card card;
    std::string meaning;
};
struct GuidedCourse {
    std::string id, title, goal;
    std::array<GuidedItem, 16> items;
    struct Task {std::string passage, question; std::array<std::string,3> options; int answer=0; std::string explanation;};
    std::array<Task,4> tasks;
    int track=0, level=0, order=0;
};
const std::array<GuidedCourse, guided_course_count> &guided_courses();
struct GuidedProfile {bool configured=false; int interest=0, start=0, last_course=-1;};
enum class GuidedKind { Introduce, Meaning, ListenWord, ListenMeaning, Order, ReadDialogue, ListenDialogue };
struct GuidedStep {
    int item = 0;
    GuidedKind kind = GuidedKind::Introduce;
    int slot = 0;
    bool reading = false;
};
struct GuidedState {
    std::array<unsigned, guided_course_count> completed{};
    std::array<unsigned, guided_course_count> listened{};
    int course = 0, node = 0, total = 0, solved = 0, errors = 0, skipped = 0, first_correct = 0;
    unsigned solved_mask = 0, attempted_mask = 0, skipped_mask = 0;
    bool active = false, finished = false, checked = false, correct = false;
    std::deque<GuidedStep> pending;
    std::vector<int> selected;
};
struct GuidedView {
    int course = 0, node = 0, track = 0, total = 0, solved = 0, errors = 0, skipped = 0,
        first_correct = 0;
    std::array<unsigned, guided_course_count> completed{};
    std::array<unsigned, guided_course_count> listened{};
    std::array<std::string, guided_course_count> titles{};
    GuidedProfile profile, draft;
    int recommended=12, recommended_node=0, catalog_track=-1, catalog_level=-1, catalog_page=0;
    std::vector<int> catalog;
    std::string recommendation_reason, passage, explanation;
    std::string title, goal, prompt, text, meaning, ipa, usage, answer, selected_text, resume_title;
    std::vector<std::string> options;
    std::vector<int> selected;
    GuidedKind kind = GuidedKind::Introduce;
    bool active = false, finished = false, checked = false, correct = false, can_check = false,
         reading = false, heard = false;
};
class GuidedLearning {
  public:
    GuidedState state;
    LearningReview review;
    GuidedProfile profile, draft;
    int catalog_track=-1, catalog_level=-1, catalog_page=0;
    int recommended_course() const;
    int next_node(int course) const;
    std::vector<int> catalog() const;
    std::string recommendation_reason() const;
    bool dialogue() const;
    std::string expected_answer() const;
    bool begin(int course, int node); // refuses to replace an unfinished lesson
    bool choose(int option);
    bool remove(int selection);
    bool check();
    bool advance();
    bool skip_listening();
    bool listening() const;
    bool ready_to_check() const;
    void played(bool success) { heard_ = success; }
    const GuidedItem *current() const;
    GuidedView view(int browse_course = -1) const;
    std::vector<std::string> options() const;
    bool unlocked(int course, int node) const;
    std::string encode() const;
    bool decode(const std::string &, std::string &error);
    bool save(const std::string &path, std::string &error) const;
    bool load(const std::string &path, std::string &error);

  private:
    std::string encode_session() const;
    bool decode_session(const std::string &, std::string &error);
    std::string encode_legacy() const;
    bool decode_legacy(const std::string &, std::string &error);
    bool heard_ = false;
};
} // namespace wordcard
