#include "wordcard_ui.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace wordcard;
namespace fs=std::filesystem;
static const char *ROOT="";
struct BookSource {std::string filename,name,description;const char*builtin_data=nullptr;size_t total=0;};
static int checks=0;
static void require(bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);}
#define ESP_LOGE(...) ((void)0)
class App {
  public:
    View view_;LearningSystem learning_;GuidedLearning guided_;
    std::vector<BookSource> books_{{"test.csv","词书","",nullptr,0}};
    std::vector<Card> cards_,personal_cards_;std::set<std::string>personal_words_;
    bool recall_catalog_ready_=false,recall_audio_pending_=false,sd_=true,rtc_=true;
    bool guided_valid_=true,pending_play_=false,personal_deck_dirty_=false;
    int deck_=0,guided_browse_=0,guided_sentence_=0;
    uint32_t day_=20000;
    std::string guided_path_,personal_path_,notice_;
    void recall_import();void recall_view();bool recall_dispatch(Event);
    void guided_load();void guided_view();bool guided_dispatch(Event);
    bool recall_input()const{return view_.screen==Screen::RecallLesson&&guided_.review.typing()&&!guided_.review.session.checked;}
    void leave_player(){pending_play_=false;recall_audio_pending_=false;}
    void update_day(){}
    void load_deck(){}
    bool save(){if(!view_.writable||!guided_valid_)return false;std::string e;return guided_.save(guided_path_,e);}
};
#include "app_guided.inc"
#include "app_recall.inc"
static void right(App&a){auto&r=a.guided_.review;auto*item=r.current();require(item,"question exists");
    if(r.typing())for(char c:item->card.front)a.recall_dispatch({Action::DictionaryKey,c});
    else {const auto key=r.session.queue.front().kind==RecallKind::Reverse?item->card.front:item->meaning;
        auto it=std::find(r.session.options.begin(),r.session.options.end(),key);require(it!=r.session.options.end(),"answer available");a.recall_dispatch({Action::RecallChoose,int(it-r.session.options.begin())});}
    if(r.listening())r.heard=true;
    a.recall_dispatch({Action::RecallCheck});require(r.session.checked&&r.session.correct,"real dispatcher correct");
}
int main(int argc,char**argv){try{
    require(argc==2,"isolated output");fs::create_directories(fs::path(argv[1])/"decks");ROOT=argv[1];
    App a;a.guided_path_=(fs::path(ROOT)/"guided-state-v2.txt").string();a.personal_path_=(fs::path(ROOT)/"decks/personal.csv").string();a.view_.writable=true;
    a.cards_={guided_courses()[0].items[0].card};a.learning_.bind(&a.cards_);
    auto &p=a.learning_.state.memory[0][a.cards_[0].id];p.seen=9;p.good=5;p.box=3;p.interval=7;p.due_day=19999;
    a.guided_load();a.recall_dispatch({Action::RecallHome});
    require(a.view_.screen==Screen::RecallHome&&a.guided_.review.counts().due==1,"one agenda includes legacy due");
    require(a.guided_.review.records[a.cards_[0].id].memory[0].attempts==0,"no fake automatic score imported");
    a.recall_dispatch({Action::RecallBegin,0});require(a.view_.screen==Screen::RecallLesson,"agenda starts due practice");right(a);
    const auto checks_saved=a.guided_.encode();App restored;restored.guided_path_=a.guided_path_;restored.guided_load();
    require(restored.guided_.encode()==checks_saved&&restored.guided_.review.session.checked,"checked recall survives actual load");
    a.recall_dispatch({Action::RecallNext});require(a.view_.screen==Screen::RecallDone,"automatic review summary");
    a.recall_dispatch({Action::RecallHome});require(a.guided_.review.counts().due==0,"unchanged old due does not reappear");
    require(p.seen==9&&p.good==5&&p.due_day==19999,"old self-rating history preserved exactly");
    a.recall_dispatch({Action::RecallBegin,3});require(a.recall_input(),"spelling uses keyboard route");
    a.recall_dispatch({Action::DictionaryKey,'x'});a.recall_dispatch({Action::DictionaryErase});require(a.guided_.review.session.typed.empty(),"typing and backspace");
    a.recall_dispatch({Action::Vocabulary});require(a.guided_.review.session.hint,"top-right captured key provides hint");right(a);
    a.recall_dispatch({Action::RecallNext});require(a.guided_.review.counts().mistakes>0,"hint remains a weak item");
    require(a.guided_.review.current()&&a.guided_.review.session.solved==0,"hint schedules independent retry");right(a);a.recall_dispatch({Action::RecallNext});
    a.recall_dispatch({Action::RecallHome});a.recall_dispatch({Action::RecallBegin,5});require(a.recall_input()&&a.guided_.review.listening(),"dictation keyboard");
    a.recall_dispatch({Action::Vocabulary});require(a.pending_play_,"captured header key schedules actual audio route");
    a.recall_dispatch({Action::Back});require(a.view_.screen==Screen::RecallHome&&a.guided_.review.current(),"input Back saves and pauses");
    const auto queue=a.guided_.review.session.queue.front().id;a.recall_dispatch({Action::RecallBegin,1});require(a.guided_.review.session.queue.front().id==queue,"new drill does not replace pending work");
    a.recall_dispatch({Action::RecallSkip});require(a.guided_.review.counts().listening==1,"unheard dictation stays pending");
    a.guided_.begin(4,0);a.view_.screen=Screen::GuidedLesson;a.recall_dispatch({Action::GuidedAdd});
    require(a.personal_cards_.size()==1&&a.personal_cards_[0].id==guided_courses()[4].items[0].card.id,"course to notebook preserves sense ID");
    a.recall_dispatch({Action::GuidedAdd});require(a.personal_cards_.size()==1,"adding same sense is idempotent");
    std::ifstream saved(a.personal_path_);std::vector<Card> personal;std::string error;require(load_deck(saved,personal,error)&&personal.size()==1,"real personal CSV saved");
    a.guided_.review.bind_clock(20000,true);a.guided_dispatch({Action::GuidedNext});
    const auto opts=a.guided_.options();const auto meaning=a.guided_.current()->meaning;
    a.guided_dispatch({Action::GuidedChoose,int(std::find(opts.begin(),opts.end(),meaning)-opts.begin())});a.guided_dispatch({Action::GuidedCheck});
    require(a.guided_.review.records[guided_courses()[4].items[0].card.id].memory[0].attempts==1,"course result and notebook share exact ID");
    std::ofstream broken(a.guided_path_);broken<<"corrupt";broken.close();App invalid;invalid.guided_path_=a.guided_path_;invalid.guided_load();
    require(!invalid.guided_valid_,"corrupt new file blocks scoring");invalid.recall_dispatch({Action::RecallBegin,0});require(!invalid.guided_.review.current(),"corrupt state not silently overwritten");
    std::cout<<"PASS: "<<checks<<" actual recall App dispatch, file storage, provenance and keyboard routing checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
