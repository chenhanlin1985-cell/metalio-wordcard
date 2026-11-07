#include "wordcard_ui.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace wordcard;
namespace fs = std::filesystem;
static int checks = 0;
static void require(bool ok, const std::string &why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
static void roundtrip(Assessment &a) {
    const auto bytes = a.encode();
    Assessment b;
    std::string e;
    require(b.decode(bytes, e), "decode " + e);
    require(b.encode() == bytes, "state identity");
    a = b;
}
static std::string ROOT;
#define ESP_LOGE(...) ((void)0)
class App {
  public:
    View view_;
    UI ui_;
    GuidedLearning guided_;
    LearningSystem learning_;
    Assessment assessment_;
    bool sd_ = true, guided_valid_ = true, pending_play_ = false, dirty_ = false;
    bool assessment_valid_ = true, assessment_storage_ok_ = true, assessment_audio_pending_ = false;
    int guided_browse_ = 0, guided_sentence_ = 0, saves = 0, assessment_page_ = 0,
        assessment_skill_ = 0, assessment_interest_ = 0;
    std::string guided_path_, assessment_path_, notice_, assessment_audio_path_;
    std::set<std::string> assessment_verified_;
    struct Audio {
        bool running = false;
        int calls = 0;
        std::string path, text;
        bool ready() { return true; }
        bool busy() { return running; }
        int stale = 0;
        bool result(std::string &, int &, std::string &) {
            if (stale) {
                --stale;
                return true;
            }
            return false;
        }
        void stop() { running = false; }
        bool play(const std::string &p, const std::string &t) {
            running = true;
            ++calls;
            path = p;
            text = t;
            return true;
        }
    } audio_;
    App(const std::string &path) : guided_path_(path + "-guided"), assessment_path_(path) {
        view_.writable = true;
    }
    void leave_player() { audio_.stop(); }
    bool save() {
        ++saves;
        std::string e;
        return guided_.save(guided_path_, e);
    }
    void guided_load();
    void guided_view();
    bool guided_dispatch(Event);
    void assessment_load();
    bool assessment_write();
    void assessment_view();
    void assessment_screen();
    bool assessment_dispatch(Event);
    bool assessment_result(int, const std::string &);
    bool tap(Event e) {
        notice_.clear();
        return assessment_dispatch(e);
    }
};
#include "app_assessment.inc"
#include "app_guided.inc"
int main(int argc, char **argv) {
    try {
        require(argc == 4, "font, output, SD fixture");
        fs::create_directories(argv[2]);
        ROOT = argv[3];
        std::ifstream f(argv[1], std::ios::binary);
        std::vector<uint8_t> fontdata((std::istreambuf_iterator<char>(f)), {}), fb(Canvas::bytes);
        Font font;
        require(font.init(fontdata.data(), fontdata.size()), "device font");
        Canvas c(fb.data(), font);
        UI ui;
        auto render = [&](App &a, Screen screen, const std::string &name) {
            a.view_.screen = screen;
            a.guided_view();
            a.assessment_view();
            a.view_.notice = a.notice_;
            ui.render(c, a.view_);
            const auto &buttons = ui.buttons();
            for (const auto &b : buttons) {
                require(b.rect.x >= 0 && b.rect.y >= 0 && b.rect.x + b.rect.w <= 684 &&
                            b.rect.y + b.rect.h <= 1216,
                        "bounds " + b.label);
                require(c.wrap(b.label, b.rect.w - 16).size() <= size_t(std::max(1, b.rect.h / 38)),
                        "button text fits " + b.label);
                require(!b.dark_fill, "accepted white controls");
                if (b.enabled) {
                    auto e = ui.hit(b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2);
                    require(e.action == b.event.action && e.value == b.event.value,
                            "hit " + b.label);
                }
            }
            for (size_t i = 0; i < buttons.size(); ++i)
                for (size_t j = i + 1; j < buttons.size(); ++j) {
                    auto x = buttons[i].rect, y = buttons[j].rect;
                    require(x.x >= y.x + y.w || y.x >= x.x + x.w || x.y >= y.y + y.h ||
                                y.y >= x.y + x.h,
                            "nonoverlap");
                }
            if (!name.empty()) {
                std::ofstream out(fs::path(argv[2]) / (name + ".pgm"), std::ios::binary);
                out << "P5\n684 1216\n255\n";
                for (int y = 0; y < 1216; ++y)
                    for (int x = 0; x < 684; ++x)
                        out.put(char(c.shade(x, y) * 17));
            }
        };
        // Independent expected totals, all 24 items and every answer. Restore at every state
        // transition.
        for (int seed = 0; seed < 64; ++seed) {
            Assessment a;
            require(!a.submit() && !a.begin_audio(), "ready guards");
            require(a.begin(seed % 2), "begin");
            int expected[2] = {0, 0}, unknown[2] = {0, 0};
            for (int i = 0; i < 24; ++i) {
                if (i == 12) {
                    require(a.state.stage == AssessmentStage::Sound, "separate sound gate");
                    require(!a.sound_confirm(), "must hear soundcheck");
                    require(a.begin_audio(), "soundcheck start");
                    a.finish_audio(true);
                    require(a.sound_confirm(), "explicit sound confirmation");
                }
                require(a.state.position == i && a.item(), "fixed form order");
                roundtrip(a);
                require(!a.choose(-2) && !a.choose(3), "option guard");
                if (i >= 12) {
                    require(!a.submit(true), "unknown requires hearing");
                    require(a.begin_audio(), "listen start");
                    a.finish_audio(false);
                    require(a.state.responses[i].plays == 0 && a.state.responses[i].failures == 1,
                            "failure not play");
                    require(a.begin_audio(), "retry");
                    a.pause();
                    require(a.state.responses[i].interruptions == 1, "interruption recorded");
                    require(!a.choose(1) && !a.begin_audio(), "paused guards");
                    a.resume();
                    require(a.begin_audio(), "listen again");
                    a.finish_audio(true);
                    require(a.begin_audio(), "second complete play");
                    a.finish_audio(true);
                    require(!a.begin_audio(), "third complete play blocked");
                }
                bool unk = (seed + i) % 11 == 0;
                int answer = (seed + i) % 3;
                require(a.choose(answer), "select");
                if (unk)
                    ++unknown[i / 12];
                else if (answer == a.item()->answer)
                    ++expected[i / 12];
                require(a.submit(unk), "commit");
                roundtrip(a);
            }
            require(a.state.stage == AssessmentStage::Done && !a.submit(true) && !a.choose(0),
                    "complete guards");
            for (int k = 0; k < 2; ++k) {
                auto r = a.report(k, seed % 2);
                require(r.correct == expected[k] && r.valid == 12 && r.unknown == unknown[k],
                        "independent report counts");
                bool found = r.course_id.empty();
                for (auto &g : guided_courses())
                    found |= g.id == r.course_id;
                require(found, "recommendation resolves to existing stable id");
            }
            auto bytes = a.encode();
            bytes.back() = 'X';
            Assessment b;
            std::string error;
            require(!b.decode(bytes, error), "corruption rejected");
        }
        App a((fs::path(argv[2]) / "assessment-state.txt").string());
        a.guided_load();
        a.guided_.begin(4, 0);
        a.guided_.state.completed[0] = 15;
        const auto guided_before = a.guided_.encode();
        a.learning_.state.memory[0]["user-word"].seen = 19;
        render(a, Screen::LearningStart, "00-learning-start");
        render(a, Screen::AssessmentHome, "01-test-intro");
        a.assessment_dispatch({Action::AssessmentBegin});
        require(a.view_.screen == Screen::AssessmentQuestion, "real begin route");
        render(a, a.view_.screen, "02-reading");
        a.assessment_dispatch({Action::AssessmentChoose, 1});
        render(a, a.view_.screen, "03-selected");
        a.assessment_dispatch({Action::Back});
        require(a.view_.screen == Screen::AssessmentPause, "real back pauses");
        render(a, a.view_.screen, "04-paused");
        App restored(a.assessment_path_);
        restored.assessment_load();
        require(restored.assessment_.state.paused && restored.assessment_.state.selected == 1,
                "reboot selection preserved");
        a.assessment_dispatch({Action::AssessmentResume});
        for (int i = 0; i < 12; ++i) {
            a.assessment_dispatch(
                {Action::AssessmentChoose,
                 i < 9 ? assessment_items()[i].answer : (assessment_items()[i].answer + 1) % 3});
            if (i == 9) {
                render(a, Screen::AssessmentQuestion, "05-reading-long-1");
                a.assessment_page_ = 1;
                render(a, Screen::AssessmentQuestion, "06-reading-long-2");
                require(ui.answer_pages() > 1, "long passage paginated");
            }
            a.assessment_dispatch({Action::AssessmentSubmit});
        }
        require(a.view_.screen == Screen::AssessmentSound, "actual sound gate");
        render(a, a.view_.screen, "07-soundcheck");
        a.assessment_dispatch({Action::AssessmentSoundConfirm});
        require(a.view_.screen == Screen::AssessmentSound, "no unplayed sound confirm");
        a.assessment_dispatch({Action::AssessmentPlay});
        require(a.audio_.text.empty() && a.audio_.calls == 1, "fixed audio no TTS text");
        require(!a.assessment_result(1, "unrelated.wav"), "stale unrelated audio ignored");
        a.audio_.running = false;
        a.assessment_result(1, "");
        a.assessment_dispatch({Action::AssessmentSoundConfirm});
        require(a.view_.screen == Screen::AssessmentQuestion && a.assessment_.state.position == 12,
                "start listen");
        a.notice_.clear();
        render(a, a.view_.screen, "08-listening-unheard");
        a.assessment_dispatch({Action::AssessmentChoose, 0});
        a.assessment_dispatch({Action::AssessmentSubmit});
        require(a.assessment_.state.position == 12, "actual unheard submit guard");
        a.assessment_dispatch({Action::AssessmentPlay});
        a.audio_.running = false;
        a.assessment_result(2, a.audio_.path);
        render(a, a.view_.screen, "09-playback-failure");
        for (int i = 12; i < 24; ++i) {
            a.audio_.stale = 1;
            a.assessment_dispatch({Action::AssessmentPlay});
            require(a.audio_.stale == 0, "stale events drained before play");
            a.audio_.running = false;
            a.assessment_result(1, "");
            a.assessment_dispatch(
                {Action::AssessmentChoose,
                 i < 18 ? assessment_items()[i].answer : (assessment_items()[i].answer + 1) % 3});
            if (i == 12) {
                a.notice_.clear();
                render(a, a.view_.screen, "10-listening-selected");
            }
            a.assessment_dispatch({Action::AssessmentSubmit});
        }
        require(a.view_.screen == Screen::AssessmentReport, "report route");
        a.notice_.clear();
        render(a, a.view_.screen, "11-report-reading");
        a.assessment_dispatch({Action::AssessmentSkill, 1});
        render(a, a.view_.screen, "12-report-listening");
        a.assessment_dispatch({Action::AssessmentInterest, 1});
        render(a, a.view_.screen, "13-report-tech");
        a.assessment_dispatch({Action::AssessmentRecommend});
        require(a.view_.screen == Screen::CourseDetail, "recommendation opens real course");
        render(a, a.view_.screen, "14-recommended-course");
        require(a.guided_.encode() == guided_before && a.saves == 0 &&
                    a.learning_.state.memory[0]["user-word"].seen == 19,
                "testing and recommendation leave all learning state unchanged");
        a.tap({Action::AssessmentHome});
        a.tap({Action::AssessmentBegin});
        require(a.view_.screen == Screen::AssessmentRestart, "repeat warning");
        render(a, a.view_.screen, "15-repeat-warning");
        a.assessment_dispatch({Action::AssessmentRestart});
        require(a.assessment_.state.attempt == 2 && fs::exists(a.assessment_path_ + ".attempt-1"),
                "archive before restart");
        // Loss of writable storage rolls back a submission; retry saves then resumes safely.
        auto path = a.assessment_path_;
        a.assessment_path_ = (fs::path(argv[2]) / "no-such-parent/state").string();
        a.assessment_dispatch({Action::AssessmentChoose, 0});
        a.assessment_dispatch({Action::AssessmentSubmit});
        require(a.assessment_.state.position == 0 && !a.assessment_storage_ok_,
                "failed save does not advance");
        render(a, a.view_.screen, "21-question-save-failure");
        require(std::any_of(ui.buttons().begin(), ui.buttons().end(),
                            [](const Button &b) {
                                return b.enabled && b.event.action == Action::AssessmentSave;
                            }),
                "failed submission has same-page retry");
        a.assessment_dispatch({Action::AssessmentPause});
        render(a, a.view_.screen, "16-storage-failure");
        a.assessment_path_ = path;
        a.assessment_dispatch({Action::AssessmentSave});
        a.assessment_dispatch({Action::AssessmentResume});
        require(a.assessment_storage_ok_ && !a.assessment_.state.paused, "save retry recovers");
        // Unavailable listening produces missing evidence, never an inferred low CEFR level.
        for (int i = 0; i < 12; ++i) {
            a.assessment_dispatch({Action::AssessmentChoose, assessment_items()[i].answer});
            a.assessment_dispatch({Action::AssessmentSubmit});
        }
        a.assessment_dispatch({Action::AssessmentSkip});
        render(a, a.view_.screen, "17-skip-confirm");
        a.assessment_dispatch({Action::AssessmentSkipConfirm});
        a.assessment_skill_ = 1;
        a.notice_.clear();
        render(a, a.view_.screen, "18-listening-unmeasured");
        require(a.assessment_.report(1, 1).valid == 0 &&
                    a.assessment_.report(1, 1).course_id.empty(),
                "unmeasured no recommendation");
        // All content and states use real native rendering, with no clipped answer controls.
        a.assessment_ = Assessment{};
        a.assessment_.begin(0);
        a.assessment_storage_ok_ = true;
        for (int i = 0; i < 24; ++i) {
            a.assessment_.state.position = i;
            a.assessment_.state.stage =
                i < 12 ? AssessmentStage::Reading : AssessmentStage::Listening;
            a.assessment_page_ = 0;
            render(a, Screen::AssessmentQuestion, "");
            auto lines = c.wrap(assessment_items()[i].text, 604);
            require(lines.size() < 35, "passage bounded");
            require(c.wrap(assessment_items()[i].question, 604).size() <= 2, "question fits");
        }
        const auto soundpath = ROOT + "/assessment/pilot-1/" + assessment_soundcheck().file;
        require(assessment_audio_valid(soundpath, assessment_soundcheck()),
                "recording content hash");
        require(!assessment_audio_valid(soundpath, assessment_items()[12].audio),
                "wrong recording rejected");
        // Real result protocol: success/cancel have empty paths, errors carry the file path.
        App interrupted((fs::path(argv[2]) / "interrupted.txt").string());
        interrupted.assessment_.begin(0);
        for (int i = 0; i < 12; ++i) {
            interrupted.assessment_.choose(0);
            interrupted.assessment_.submit();
        }
        interrupted.assessment_.begin_audio();
        interrupted.assessment_.finish_audio(true);
        interrupted.assessment_.sound_confirm();
        interrupted.assessment_screen();
        interrupted.tap({Action::AssessmentPlay});
        require(interrupted.assessment_.state.inflight, "inflight persisted before playback");
        App interruptedBoot(interrupted.assessment_path_);
        interruptedBoot.assessment_load();
        require(interruptedBoot.assessment_.state.paused &&
                    !interruptedBoot.assessment_.state.inflight &&
                    interruptedBoot.assessment_.state.responses[12].plays == 0 &&
                    interruptedBoot.assessment_.state.responses[12].interruptions == 1,
                "power loss cannot become completed play");
        interrupted.tap({Action::AssessmentPause});
        require(!interrupted.assessment_result(1, ""), "late success ignored after pause");
        interrupted.tap({Action::AssessmentResume});
        interrupted.audio_.stale = 1;
        interrupted.tap({Action::AssessmentPlay});
        require(interrupted.audio_.stale == 0, "same-file stale event drained");
        interrupted.audio_.running = false;
        require(interrupted.assessment_result(3, ""),
                "empty-path cancellation matches owned request");
        require(interrupted.assessment_.state.responses[12].plays == 0, "cancelled not heard");
        // A corrupt test file remains untouched; normal lessons still open.
        const auto badpath = (fs::path(argv[2]) / "corrupt-test.txt").string();
        {
            std::ofstream bad(badpath);
            bad << "broken assessment";
        }
        App bad(badpath);
        bad.assessment_load();
        require(!bad.assessment_valid_, "bad test state rejected");
        bad.view_.screen = Screen::AssessmentHome;
        bad.tap({Action::AssessmentBegin});
        require(bad.assessment_.state.attempt == 0, "corrupt test cannot be replaced by begin");
        render(bad, Screen::AssessmentHome, "19-corrupt-record");
        require(std::none_of(ui.buttons().begin(), ui.buttons().end(),
                             [](const Button &b) {
                                 return b.enabled && b.event.action == Action::AssessmentSave;
                             }),
                "corrupt record does not advertise impossible retry");
        std::ifstream broken(badpath);
        std::string retained((std::istreambuf_iterator<char>(broken)), {});
        require(retained == "broken assessment", "bad state retained exactly");
        bad.guided_dispatch({Action::GuidedCourses});
        require(bad.view_.screen == Screen::GuidedCourses,
                "corrupt test does not block normal courses");
        // Independent recommendation must not silently replace the unfinished original course.
        App preserved((fs::path(argv[2]) / "preserved.txt").string());
        preserved.guided_load();
        preserved.guided_.begin(4, 0);
        const auto old = preserved.guided_.encode();
        preserved.guided_dispatch({Action::GuidedCourse, 5});
        preserved.guided_dispatch({Action::GuidedBegin, 0});
        require(preserved.view_.screen == Screen::GuidedPause && preserved.guided_.encode() == old,
                "new recommended lesson respects active session");
        render(preserved, Screen::GuidedPause, "20-existing-course-retained");
        // Whole valid payload mutations are rejected, not only checksum errors.
        auto valid = interruptedBoot.assessment_.encode();
        auto altered = valid;
        auto fp = altered.find(assessment_bank_fingerprint());
        require(fp != std::string::npos, "bank content signature stored");
        altered[fp] = 'X';
        auto nl = altered.find('\n');
        altered = "assessment-v1 " + std::to_string(assessment_hash(altered.substr(nl + 1))) +
                  altered.substr(nl);
        Assessment reject;
        std::string err;
        require(!reject.decode(altered, err), "changed form rejected despite valid checksum");
        App soundFailure((fs::path(argv[2]) / "sound-save.txt").string());
        soundFailure.assessment_.begin(0);
        for (int i = 0; i < 12; ++i) {
            soundFailure.assessment_.choose(0);
            soundFailure.assessment_.submit();
        }
        soundFailure.assessment_screen();
        soundFailure.tap({Action::AssessmentPlay});
        const auto goodPath = soundFailure.assessment_path_;
        soundFailure.assessment_path_ = (fs::path(argv[2]) / "missing-sound-parent/state").string();
        soundFailure.audio_.running = false;
        soundFailure.assessment_result(1, "");
        require(!soundFailure.assessment_storage_ok_ && soundFailure.assessment_.state.sound_heard,
                "sound completed but persistence failed");
        render(soundFailure, Screen::AssessmentSound, "22-sound-save-failure");
        require(std::any_of(ui.buttons().begin(), ui.buttons().end(),
                            [](const Button &b) {
                                return b.enabled && b.event.action == Action::AssessmentSave;
                            }),
                "sound save failure has visible same-page recovery");
        soundFailure.tap({Action::AssessmentSoundConfirm});
        require(soundFailure.assessment_.state.stage == AssessmentStage::Sound,
                "cannot begin hearing test until soundcheck saved");
        soundFailure.assessment_path_ = goodPath;
        soundFailure.tap({Action::AssessmentSave});
        soundFailure.tap({Action::AssessmentSoundConfirm});
        require(soundFailure.assessment_.state.stage == AssessmentStage::Listening &&
                    soundFailure.assessment_storage_ok_,
                "soundcheck retry saves then permits listening");
        std::cout << "PASS " << checks
                  << " assessment core, real App dispatch, storage isolation, audio and native UI "
                     "checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
