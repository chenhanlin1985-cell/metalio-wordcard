#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace wordcard {
constexpr int assessment_count = 24;
constexpr const char *assessment_bank_version = "listen-read-pilot-1";
constexpr const char *assessment_rule_version = "course-trial-1";
struct AssessmentAudio {
    const char *file;
    uint32_t bytes, hash;
};
struct AssessmentItem {
    const char *id, *text, *question;
    std::array<const char *, 3> options;
    int answer, band; // band is an editorial task target, NEVER a measured learner level.
    const char *focus;
    AssessmentAudio audio;
};
const std::array<AssessmentItem, assessment_count> &assessment_items();
const AssessmentAudio &assessment_soundcheck();
const char *assessment_bank_fingerprint();
enum class AssessmentStage { Ready, Reading, Sound, Listening, Done };
struct AssessmentResponse {
    int answer = -3, plays = 0, failures = 0, interruptions = 0;
};
struct AssessmentState {
    uint32_t attempt = 0;
    AssessmentStage stage = AssessmentStage::Ready;
    int position = 0, selected = -1, interest = 0;
    bool paused = false, inflight = false, sound_heard = false;
    std::array<AssessmentResponse, assessment_count> responses{};
};
struct AssessmentReport {
    int correct = 0, valid = 0, unknown = 0, invalid = 0, failures = 0, interruptions = 0;
    std::array<int, 4> band_correct{}, band_valid{};
    std::string course_id, reason;
};
struct AssessmentView {
    AssessmentState state;
    const AssessmentItem *item = nullptr;
    std::array<AssessmentReport, 2> reports{};
    bool storage_ok = false, record_valid = true, can_play = false, can_submit = false,
         busy = false;
    int page = 0, report_skill = 0;
};
class Assessment {
  public:
    AssessmentState state;
    bool begin(int interest);
    bool choose(int option);
    bool can_submit() const;
    bool submit(bool unknown = false);
    bool begin_audio();
    void finish_audio(bool success, bool interrupted = false);
    bool sound_confirm();
    bool omit_listening();
    void pause();
    void resume();
    const AssessmentItem *item() const;
    AssessmentReport report(int skill, int interest) const;
    std::string encode() const;
    bool decode(const std::string &, std::string &);
    bool save(const std::string &, std::string &) const;
    bool load(const std::string &, std::string &);
};
uint32_t assessment_hash(const std::string &);
bool assessment_audio_valid(const std::string &, const AssessmentAudio &);
} // namespace wordcard
