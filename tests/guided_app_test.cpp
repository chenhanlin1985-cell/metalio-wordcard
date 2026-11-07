// Runs the firmware's actual guided dispatcher with real storage and a stubbed audio stop.
#include "wordcard_ui.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace wordcard;
namespace fs = std::filesystem;
static int checks = 0;
static void require(bool b, const char *why) {
    ++checks;
    if (!b)
        throw std::runtime_error(why);
}
#define ESP_LOGE(...) ((void)0)
class App {
  public:
    View view_;
    GuidedLearning guided_;
    LearningSystem learning_;
    bool sd_ = true, guided_valid_ = true, fail_save = false,pending_play_=false;
    int guided_browse_ = 0, guided_sentence_=0, stops = 0, saves = 0;
    std::string guided_path_, notice_;
    explicit App(const std::string &path) : guided_path_(path) { view_.writable = true; }
    void guided_load();
    void guided_view();
    bool guided_dispatch(Event);
    void leave_player() { ++stops; }
    bool save() {
        ++saves;
        std::string error;
        if (fail_save) {
            view_.writable = false;
            return false;
        }
        return guided_.save(guided_path_, error);
    }
};
#include "app_guided.inc"
int main(int argc, char **argv) {
    try {
        require(argc == 2, "output directory");
        fs::create_directories(argv[1]);
        const auto path = (fs::path(argv[1]) / "app-progress.txt").string();
        App a(path);
        a.guided_load();
        require(a.view_.guided_enabled && a.guided_valid_,
                "new install starts guided without changing old state");
        a.guided_dispatch({Action::LearningStart});
        a.guided_dispatch({Action::LearningInterest,1});a.guided_dispatch({Action::LearningLevel,1});
        a.guided_dispatch({Action::Back});require(!a.guided_.profile.configured,"cancelled profile draft not committed");
        a.guided_dispatch({Action::LearningStart});a.guided_dispatch({Action::LearningInterest,1});a.guided_dispatch({Action::LearningLevel,1});
        a.guided_dispatch({Action::LearningApply});
        require(a.guided_.profile.configured&&a.guided_.recommended_course()==4,"actual recommendation dispatcher");
        App preferences(path);preferences.guided_load();require(preferences.guided_.profile.interest==1&&preferences.guided_.profile.start==1,"preferences survive reboot");
        preferences.guided_dispatch({Action::CurriculumContinue});require(preferences.guided_.state.course==4&&preferences.view_.screen==Screen::GuidedLesson,"home continues recommendation");
        preferences.guided_dispatch({Action::GuidedPause});preferences.guided_dispatch({Action::GuidedCourse,12});
        preferences.guided_dispatch({Action::CurriculumContinue});require(preferences.guided_.state.course==4,"continue ignores browsing another course");
        a.guided_dispatch({Action::GuidedCourses});a.guided_dispatch({Action::GuidedTrack,3});a.guided_dispatch({Action::CurriculumLevel,0});a.guided_dispatch({Action::CurriculumPage,1});a.guided_view();
        require(a.view_.guided.catalog.size()==6&&a.view_.guided.catalog_page==1,"catalog filters and pagination");
        a.guided_dispatch({Action::GuidedTrack,1});a.guided_view();require(a.view_.guided.catalog.empty()&&a.view_.guided.catalog_page==0,"honest empty filter resets page");
        a.guided_dispatch({Action::CurriculumLevel,-1});
        a.guided_dispatch({Action::LegacyPractice});require(a.view_.legacy_practice&&a.view_.screen==Screen::Practice,"legacy specialist tools remain reachable");
        a.guided_dispatch({Action::Practice});require(!a.view_.legacy_practice,"main practice tab returns to simple navigation");
        a.learning_.state.memory[0]["existing-user-word"].seen = 11;
        a.guided_dispatch({Action::GuidedTrack, 1});
        a.guided_dispatch({Action::GuidedCourse, 4});
        a.guided_dispatch({Action::GuidedBegin, 0});
        require(a.view_.screen == Screen::GuidedLesson && a.guided_.state.course == 4 &&
                    a.learning_.state.onboarded,
                "start selected tech course");
        require(a.guided_dispatch({Action::Speak}) && a.pending_play_,"real course pronunciation dispatch schedules existing audio pipeline");
        require(a.guided_dispatch({Action::GuidedNext}), "move intro to choice");
        auto v = a.guided_.view();
        int correct = 0;
        while (v.options[correct] != v.meaning)
            ++correct;
        a.guided_dispatch({Action::GuidedChoose, correct});
        a.guided_dispatch({Action::GuidedCheck});
        require(a.guided_.state.checked && a.guided_.state.correct && a.guided_.state.solved == 1,
                "actual dispatcher grades and persists");
        App restored(path);
        restored.guided_load();
        require(restored.guided_.state.checked && restored.guided_.state.correct,
                "reboot retains checked feedback");
        restored.guided_dispatch({Action::GuidedResume});
        restored.guided_dispatch({Action::GuidedCheck});
        require(restored.guided_.state.solved == 1,
                "repeated check after reboot does not double count");
        restored.guided_dispatch({Action::Back});
        require(restored.view_.screen == Screen::GuidedPause, "back pauses lesson");
        restored.guided_dispatch({Action::GuidedCourse, 0});
        restored.guided_dispatch({Action::GuidedBegin, 0});
        require(restored.view_.screen == Screen::GuidedPause && restored.guided_.state.course == 4,
                "starting different course never silently replaces active lesson");
        restored.guided_dispatch({Action::GuidedAbandon});
        require(!restored.guided_.state.active, "explicit end permits new choice");
        restored.guided_dispatch({Action::GuidedCourse, 0});
        restored.guided_dispatch({Action::GuidedBegin, 0});
        require(restored.guided_.state.course == 0 && restored.guided_.state.active,
                "new daily lesson");
        const auto active_state=restored.guided_.encode();
        restored.guided_dispatch({Action::GuidedListen});restored.guided_dispatch({Action::GuidedListenStep,1});restored.guided_view();
        require(restored.view_.screen==Screen::GuidedListen&&restored.view_.guided_sentence==1,"direct sentence listening and next sentence");
        require(restored.view_.guided.text==guided_courses()[0].items[7].card.front,"correct current course sentence");
        require(restored.guided_dispatch({Action::Speak})&&restored.pending_play_,"sentence pronunciation schedules playback");
        restored.guided_dispatch({Action::Back});require(restored.guided_.encode()==active_state,"sentence browsing never changes lesson answers");
        require(a.learning_.state.memory[0].at("existing-user-word").seen == 11,
                "old card progress preserved");
        // Host-only section fixture: exercise the actual dispatcher, persistence, and dialogue audio gate.
        const auto dialogue_path=(fs::path(argv[1])/"dialogue-progress.txt").string();
        App dialogue(dialogue_path);dialogue.guided_load();dialogue.guided_.state.completed[12]=31;
        dialogue.guided_dispatch({Action::GuidedCourse,12});dialogue.guided_dispatch({Action::GuidedBegin,5});
        require(dialogue.guided_.view().kind==GuidedKind::ListenDialogue,"actual dispatcher enters short listening task");
        auto choices=dialogue.guided_.options();auto answer=dialogue.guided_.expected_answer();
        int pick=int(std::find(choices.begin(),choices.end(),answer)-choices.begin());
        dialogue.guided_dispatch({Action::GuidedChoose,pick});dialogue.guided_dispatch({Action::GuidedCheck});
        require(!dialogue.guided_.state.checked,"actual dialogue check rejects unheard answer");
        dialogue.guided_dispatch({Action::Speak});require(dialogue.pending_play_,"dialogue uses existing audio request path");
        dialogue.guided_.played(false);dialogue.guided_dispatch({Action::GuidedCheck});
        require(!dialogue.guided_.state.checked,"failed dialogue playback cannot score");
        dialogue.guided_.played(true);dialogue.guided_dispatch({Action::GuidedCheck});
        require(dialogue.guided_.state.checked&&dialogue.guided_.state.correct,"successful dialogue playback permits judgement");
        App dialogue_reload(dialogue_path);dialogue_reload.guided_load();
        require(dialogue_reload.guided_.state.checked&&dialogue_reload.guided_.state.correct,"dialogue feedback survives actual save and reload");
        dialogue_reload.guided_dispatch({Action::GuidedResume});dialogue_reload.guided_dispatch({Action::GuidedNext});
        dialogue_reload.guided_dispatch({Action::GuidedSkip});
        require(dialogue_reload.guided_.view().reading,"actual dialogue reading fallback");
        choices=dialogue_reload.guided_.options();answer=dialogue_reload.guided_.expected_answer();
        pick=int(std::find(choices.begin(),choices.end(),answer)-choices.begin());
        dialogue_reload.guided_dispatch({Action::GuidedChoose,pick});dialogue_reload.guided_dispatch({Action::GuidedCheck});
        dialogue_reload.guided_dispatch({Action::GuidedNext});
        require(dialogue_reload.view_.screen==Screen::GuidedDone&&!(dialogue_reload.guided_.state.listened[12]&32),"reading completion leaves dialogue listening due");
        std::ofstream broken(path, std::ios::binary | std::ios::trunc);
        broken << "damaged";
        broken.close();
        App readonly(path);
        readonly.guided_load();
        require(!readonly.guided_valid_, "corrupt progress locks guided start");
        readonly.guided_dispatch({Action::GuidedBegin, 0});
        require(!readonly.guided_.state.active && !readonly.notice_.empty(),
                "cannot overwrite corrupt state by starting");
        std::ifstream checkfile(path);
        std::string bytes;
        checkfile >> bytes;
        require(bytes == "damaged", "corrupt source preserved for recovery");
        std::cout << "PASS: " << checks
                  << " actual guided App dispatch / storage / navigation checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
