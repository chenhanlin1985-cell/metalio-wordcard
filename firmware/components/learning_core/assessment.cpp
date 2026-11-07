#include "assessment.hpp"
#include "wordcard.hpp"
#include <fstream>
#include <limits>
#include <sstream>
namespace wordcard {
uint32_t assessment_hash(const std::string &s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}
const AssessmentItem *Assessment::item() const {
    return (state.stage == AssessmentStage::Reading || state.stage == AssessmentStage::Listening) &&
                   state.position >= 0 && state.position < assessment_count
               ? &assessment_items()[state.position]
               : nullptr;
}
bool Assessment::begin(int interest) {
    if ((state.stage != AssessmentStage::Ready && state.stage != AssessmentStage::Done) ||
        interest < 0 || interest > 1 || state.attempt >= 1000000)
        return false;
    const auto attempt = state.attempt + 1;
    state = {};
    state.attempt = attempt;
    state.interest = interest;
    state.stage = AssessmentStage::Reading;
    return true;
}
bool Assessment::choose(int option) {
    if (!item() || state.paused || state.inflight || option < 0 || option > 2)
        return false;
    state.selected = option;
    return true;
}
bool Assessment::can_submit() const {
    return item() && !state.paused && !state.inflight && state.selected >= 0 &&
           (state.stage == AssessmentStage::Reading || state.responses[state.position].plays > 0);
}
bool Assessment::submit(bool unknown) {
    if (!item() || state.paused || state.inflight ||
        (state.stage == AssessmentStage::Listening && state.responses[state.position].plays == 0) ||
        (!unknown && !can_submit()))
        return false;
    state.responses[state.position].answer = unknown ? -1 : state.selected;
    ++state.position;
    state.selected = -1;
    if (state.position == 12)
        state.stage = AssessmentStage::Sound;
    else if (state.position == assessment_count)
        state.stage = AssessmentStage::Done;
    return true;
}
bool Assessment::begin_audio() {
    if (state.paused || state.inflight)
        return false;
    if (state.stage != AssessmentStage::Sound &&
        (state.stage != AssessmentStage::Listening || state.responses[state.position].plays >= 2))
        return false;
    state.inflight = true;
    return true;
}
void Assessment::finish_audio(bool success, bool interrupted) {
    if (!state.inflight)
        return;
    state.inflight = false;
    if (state.stage == AssessmentStage::Sound) {
        if (success)
            state.sound_heard = true;
        return;
    }
    if (state.stage == AssessmentStage::Listening) {
        auto &r = state.responses[state.position];
        if (success)
            ++r.plays;
        else if (interrupted)
            ++r.interruptions;
        else
            ++r.failures;
    }
}
bool Assessment::sound_confirm() {
    if (state.stage != AssessmentStage::Sound || !state.sound_heard || state.inflight ||
        state.paused)
        return false;
    state.stage = AssessmentStage::Listening;
    return true;
}
bool Assessment::omit_listening() {
    if (state.paused || state.inflight ||
        (state.stage != AssessmentStage::Listening && state.stage != AssessmentStage::Sound))
        return false;
    for (int i = state.position; i < assessment_count; ++i)
        state.responses[i].answer = -2;
    state.position = assessment_count;
    state.selected = -1;
    state.stage = AssessmentStage::Done;
    return true;
}
void Assessment::pause() {
    if (state.stage != AssessmentStage::Ready && state.stage != AssessmentStage::Done) {
        finish_audio(false, true);
        state.paused = true;
    }
}
void Assessment::resume() {
    state.paused = false;
}
AssessmentReport Assessment::report(int skill, int interest) const {
    AssessmentReport r;
    if (skill < 0 || skill > 1)
        return r;
    for (int i = skill * 12; i < skill * 12 + 12; ++i) {
        const auto &a = state.responses[i];
        const auto &q = assessment_items()[i];
        r.failures += a.failures;
        r.interruptions += a.interruptions;
        if (a.answer == -2) {
            ++r.invalid;
            continue;
        }
        if (a.answer == -3)
            continue;
        ++r.valid;
        ++r.band_valid[q.band];
        if (a.answer == -1)
            ++r.unknown;
        if (a.answer == q.answer) {
            ++r.correct;
            ++r.band_correct[q.band];
        }
    }
    if (r.valid < 9) {
        r.reason = "有效题不足，暂不给出测试推荐。";
        return r;
    }
    int weak = 3;
    for (int b = 0; b < 4; ++b)
        if (r.band_valid[b] == 3 && r.band_correct[b] < 3) {
            weak = b;
            break;
        }
    static const char *daily[] = {"foundation-02", "daily-02", "foundation-08", ""};
    static const char *tech[] = {"foundation-05", "tech-01", "tech-02", "tech-04"};
    r.course_id = (interest == 1 ? tech : daily)[weak];
    static const char *why[] = {"先巩固短句中的时间、位置与步骤。", "先练习安排变化与简单通知。",
                                "先练习原因、条件与前后顺序。", "可试学比较与观点类课程。"};
    r.reason = why[weak];
    if (weak == 3 && interest != 1)
        r.reason = "日常进阶课待补，可自选现有课程巩固。";
    return r;
}
std::string Assessment::encode() const {
    std::ostringstream out;
    out << assessment_bank_version << ' ' << assessment_rule_version << ' '
        << assessment_bank_fingerprint() << '\n';
    out << state.attempt << ' ' << int(state.stage) << ' ' << state.position << ' '
        << state.selected << ' ' << state.interest << ' ' << state.paused << ' ' << state.inflight
        << ' ' << state.sound_heard << '\n';
    for (int i = 0; i < assessment_count; ++i) {
        auto &r = state.responses[i];
        out << assessment_items()[i].id << ' ' << r.answer << ' ' << r.plays << ' ' << r.failures
            << ' ' << r.interruptions << '\n';
    }
    const auto payload = out.str();
    return "assessment-v1 " + std::to_string(assessment_hash(payload)) + "\n" + payload;
}
bool Assessment::decode(const std::string &bytes, std::string &error) {
    auto bad = [&]() {
        error = "Invalid assessment state, versions or checksum; original retained";
        return false;
    };
    if (bytes.size() > 8192)
        return bad();
    const auto nl = bytes.find('\n');
    if (nl == std::string::npos)
        return bad();
    std::istringstream h(bytes.substr(0, nl));
    std::string v, extra;
    uint32_t sum;
    if (!(h >> v >> sum) || (h >> extra) || v != "assessment-v1" ||
        sum != assessment_hash(bytes.substr(nl + 1)))
        return bad();
    std::istringstream in(bytes.substr(nl + 1));
    std::string bank, rule, fingerprint;
    Assessment next;
    auto &s = next.state;
    int stage, pause, flight, sound;
    if (!(in >> bank >> rule >> fingerprint) || bank != assessment_bank_version ||
        rule != assessment_rule_version || fingerprint != assessment_bank_fingerprint())
        return bad();
    if (!(in >> s.attempt >> stage >> s.position >> s.selected >> s.interest >> pause >> flight >>
          sound) ||
        stage < 0 || stage > 4 || s.position < 0 || s.position > 24 || s.selected < -1 ||
        s.selected > 2 || s.interest < 0 || s.interest > 1 || pause < 0 || pause > 1 ||
        flight < 0 || flight > 1 || sound < 0 || sound > 1 || s.attempt > 1000000)
        return bad();
    s.stage = AssessmentStage(stage);
    s.paused = pause;
    s.inflight = flight;
    s.sound_heard = sound;
    if ((stage == 0 && (s.attempt || s.position || pause || flight || sound || s.selected != -1)) ||
        (stage > 0 && !s.attempt) || (stage == 1 && s.position >= 12) ||
        (stage == 2 && s.position != 12) ||
        (stage == 3 && (s.position < 12 || s.position >= 24 || !sound)) ||
        (stage == 4 && (s.position != 24 || pause || flight || s.selected != -1)) ||
        (flight && (pause || (stage != 2 && stage != 3))) || (stage == 2 && s.selected != -1))
        return bad();
    for (int i = 0; i < assessment_count; ++i) {
        std::string id;
        auto &r = s.responses[i];
        if (!(in >> id >> r.answer >> r.plays >> r.failures >> r.interruptions) ||
            id != assessment_items()[i].id || r.answer < -3 || r.answer > 2 || r.plays < 0 ||
            r.plays > 2 || r.failures < 0 || r.failures > 100000 || r.interruptions < 0 ||
            r.interruptions > 100000)
            return bad();
        if ((i < s.position && r.answer == -3) || (i >= s.position && r.answer != -3) ||
            (i < 12 && (r.plays || r.failures || r.interruptions || r.answer == -2)) ||
            (i >= 12 && r.answer >= -1 && r.plays == 0) ||
            (i > s.position && (r.plays || r.failures || r.interruptions)))
            return bad();
    }
    if (in >> extra)
        return bad();
    *this = next;
    return true;
}
bool Assessment::save(const std::string &path, std::string &error) const {
    return atomic_write(path, encode(), error);
}
bool Assessment::load(const std::string &path, std::string &error) {
    if (!recover_backup(path, error))
        return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Cannot read assessment";
        return false;
    }
    std::string bytes;
    char c;
    while (in.get(c)) {
        bytes += c;
        if (bytes.size() > 8192) {
            error = "Oversized assessment";
            return false;
        }
    }
    if (!decode(bytes, error))
        return false;
    // Playback interrupted by shutdown is not a completed listen.
    pause();
    return true;
}
bool assessment_audio_valid(const std::string &path, const AssessmentAudio &audio) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    uint32_t h = 2166136261u;
    size_t size = 0;
    char buf[4096];
    while (in) {
        in.read(buf, sizeof(buf));
        const auto n = in.gcount();
        size += size_t(n);
        if (size > audio.bytes)
            return false;
        for (int i = 0; i < n; ++i) {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 16777619u;
        }
    }
    return in.eof() && size == audio.bytes && h == audio.hash;
}
} // namespace wordcard
