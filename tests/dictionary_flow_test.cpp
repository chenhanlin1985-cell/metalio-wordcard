// Executes the firmware's dictionary dispatch cases and helpers with filesystem I/O.
// Hardware audio, NVS and the unrelated navigation dispatcher are not emulated.
#include "wordcard_ui.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace wordcard;
namespace fs=std::filesystem;
static std::string ROOT;
static int checks=0,frames=0;
static void check(bool ok,const std::string &why){++checks;if(!ok)throw std::runtime_error(why);}
static void nvs_set_str(int,const char *,const char *){}
static void nvs_commit(int){}
static void task_mkdir(const char *p,int){fs::create_directories(p);}
struct BookSource{std::string filename,name,description;const char *builtin_data=nullptr;size_t total=0;};
class App {
public:
    Dictionary dictionary_;Card dictionary_card_;std::set<std::string> personal_words_;
    std::vector<Card> personal_cards_,cards_;bool personal_deck_dirty_=false;
    UI ui_;View view_;LearningSystem learning_;GuidedLearning guided_;bool rtc_=true;ProgressMap &progress_=learning_.state.memory[0];
    std::vector<BookSource> books_{{"daily.csv","日常英语","",nullptr,0}};
    Screen dictionary_return_=Screen::DictionaryResults,vocabulary_return_=Screen::Home,dictionary_origin_return_=Screen::Home;
    Card vocabulary_origin_card_;Screen vocabulary_origin_return_=Screen::Dictionary;int vocabulary_origin_page_=0;
    struct{void stop(){}}audio_;
    // App routing uses the actual shared helper. Only the native model transport
    // is a stub here; vocabulary_ui_test covers its persistence separately.
    struct {
        bool available=true,succeeds=true,status_ok=true;
        unsigned calls=0;
        std::string last_action,last_value,status_json="{}";
        std::vector<std::string> exposed_words;
        bool ready()const{return available;}
        bool command(const std::string &action,const std::string &value="") {
            ++calls;last_action=action;last_value=value;
            if(action=="v-lookup-exposure")exposed_words.push_back(value);
            return succeeds;
        }
        bool status(std::string &out){out=status_json;return status_ok;}
    }web_;
    bool pending_play_=false,nvs_ok_=false;int deck_=0,nvs_=0;uint32_t day_=100;
    std::string group_,notice_,personal_path_;
    App(){personal_path_=ROOT+"/decks/custom-my-vocabulary.csv";view_.writable=true;view_.date_valid=true;learning_.state.onboarded=true;}
    void leave_player(){pending_play_=false;}
    void update_day(){}
    bool save(){std::string error;return save_learning_state(ROOT+"/learning.csv",learning_.state,error);}
    void load_deck(){std::string error;std::ifstream f(personal_path_);check(load_deck_file(f,error),error);learning_.bind(&cards_);personal_deck_dirty_=false;group_.clear();}
    bool load_deck_file(std::istream &f,std::string &error){return wordcard::load_deck(f,cards_,error);}
    bool sync_personal_deck(){if(!personal_deck_dirty_)return true;std::string error;if(!reload_personal_words(personal_path_,cards_,error)){notice_=error;return false;}learning_.bind(&cards_);personal_deck_dirty_=false;return true;}
    bool dictionary_expose(const std::string &);
    bool open_dictionary_entry(size_t,Screen);
    void refresh_vocabulary();
    bool dictionary_back();bool dictionary_native_dispatch(Event);void end_lesson();
    void refresh(){
        view_.notice=notice_;view_.dictionary_ready=dictionary_.ready();
        view_.dictionary_added=view_.dictionary_native_lookup
            ? view_.dictionary_native_added_id==dictionary_card_.id && !dictionary_card_.id.empty()
            : personal_words_.count(dictionary_card_.id)>0;
        view_.personal_count=personal_words_.size();view_.card=view_.screen==Screen::DictionaryEntry?&dictionary_card_:learning_.current();
        view_.audio_available=true;view_.audio_source=SpeechSource::Synthesis;view_.tts_available=true;
        view_.personal_book=books_[deck_].filename=="custom-my-vocabulary.csv";refresh_vocabulary();
        view_.book_name=books_[deck_].name;view_.deck=view_.book_name;view_.count=learning_.state.lesson.queue.size();
        view_.position=learning_.state.lesson.position;view_.revealed=learning_.state.lesson.revealed;view_.writable=true;
        view_.lesson_distinct=learning_.distinct_in_lesson();view_.lesson_difficult=learning_.difficult_in_lesson();
        view_.can_undo=learning_.can_undo();view_.study_mode=learning_.state.lesson.mode;
    }
    void dispatch(Event e){const auto screen=view_.screen;notice_.clear();if(dictionary_native_dispatch(e)){refresh();return;}switch(e.action){
#define mkdir task_mkdir
#include "app_dictionary_actions.inc"
#undef mkdir
        case Action::Resume:view_.vocabulary_review_pending=false;learning_.resume();view_.screen=Screen::Study;break;
        case Action::Back:dictionary_back();break;
        case Action::EndLesson:end_lesson();break;
        default:break;
    }refresh();}
};
#include "app_dictionary_helpers.inc"
int main(int argc,char **argv){try{
    check(argc==5,"arguments: dictionary, font, isolated output, supplemental font");ROOT=std::string(argv[3])+"/sd";fs::create_directories(ROOT);
    std::string error;App app;check(app.dictionary_.open(argv[1],error),error);
    auto read_bytes=[](const char *p){std::ifstream f(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});};
    auto bytes=read_bytes(argv[2]),extra_bytes=read_bytes(argv[4]);Font font,extra;
    check(font.init(bytes.data(),bytes.size()),"font");check(extra.init(extra_bytes.data(),extra_bytes.size()),"extra font");font.fallback(&extra);
    std::vector<uint8_t> fb(Canvas::bytes);Canvas c(fb.data(),font);
    auto frame=[&](const std::string &name){
        app.refresh();app.ui_.render(c,app.view_);
        const auto &buttons=app.ui_.buttons();
        for(size_t i=0;i<buttons.size();++i){const auto &b=buttons[i];
            check(b.rect.x>=0 && b.rect.y>=0 && b.rect.x+b.rect.w<=684 && b.rect.y+b.rect.h<=1216,name+" bounds");
            const auto hit=app.ui_.hit(b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2);
            check(!b.enabled || hit.action==b.event.action && hit.value==b.event.value,name+" hit");
            for(size_t j=i+1;j<buttons.size();++j){const auto a=buttons[j].rect;
                check(!(b.rect.x<a.x+a.w && a.x<b.rect.x+b.rect.w && b.rect.y<a.y+a.h && a.y<b.rect.y+b.rect.h),name+" overlap");}
        }
        std::ofstream f(fs::path(argv[3])/(name+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";
        for(int y=0;y<1216;++y)for(int x=0;x<684;++x)f.put(char(c.shade(x,y)*17));++frames;
    };
    auto tap=[&](Action action,int value=0){app.ui_.render(c,app.view_);for(const auto &b:app.ui_.buttons())
        if(b.enabled && b.event.action==action && b.event.value==value){auto event=app.ui_.hit(b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2);app.dispatch(event);return;}
        check(false,"missing enabled control "+std::to_string(int(action)));};
    auto query=[&](const std::string &word){app.dispatch({Action::Dictionary});app.dispatch({Action::DictionaryClear});
        for(char key:word)app.dispatch({Action::DictionaryKey,int(key)});app.dispatch({Action::DictionarySearch});};
    app.dispatch({Action::Vocabulary});frame("00-empty-notebook");tap(Action::Dictionary);frame("01-input");
    for(char key:std::string("appointment"))tap(Action::DictionaryKey,int(key));frame("02-typed-daily");tap(Action::DictionarySearch);
    check(app.view_.screen==Screen::DictionaryEntry && app.dictionary_card_.front=="appointment","daily exact opens detail");
    check(!app.dictionary_card_.ipa.empty(),"daily IPA");frame("03-daily-entry");
    check(app.dictionary_card_.ipa=="US /əˈpɔɪntmənt/" && app.dictionary_card_.back.find("预约")!=std::string::npos,"reviewed daily course meaning and IPA");
    app.view_.audio_pending=true;app.view_.audio_status="正在准备合成发音；可停止或返回";frame("04-speech-pending");tap(Action::SpeechStop);app.view_.audio_pending=false;
    tap(Action::DictionaryAdd);check(app.personal_cards_.size()==1,"daily durable add");frame("05-added");
    app.dispatch({Action::DictionaryAdd});check(app.personal_cards_.size()==1,"double add idempotent");
    tap(Action::Vocabulary);check(app.view_.screen==Screen::Vocabulary && app.deck_==0,"browse keeps selected course");frame("06-one-word");
    tap(Action::Back);check(app.view_.screen==Screen::DictionaryEntry && app.dictionary_card_.front=="appointment" && app.dictionary_return_==Screen::Dictionary,"notebook back restores lookup detail");
    query("firmware");check(app.dictionary_card_.front=="firmware" && !app.dictionary_card_.ipa.empty(),"tech word and IPA");frame("07-tech-entry");tap(Action::DictionaryAdd);
    query("protocol");tap(Action::DictionaryAdd);query("latency");
    check(app.dictionary_card_.back.find("延迟")!=std::string::npos && app.dictionary_card_.ipa=="US /ˈleɪtənsi/","reviewed technology meaning and IPA");
    tap(Action::DictionaryAdd);query("coffee");tap(Action::DictionaryAdd);
    query("firmware");app.view_.answer_page=1;tap(Action::Vocabulary);tap(Action::VocabularySelect,4);
    check(app.dictionary_card_.front=="coffee","browse a different saved word");tap(Action::Back);tap(Action::Back);
    check(app.view_.screen==Screen::DictionaryEntry && app.dictionary_card_.front=="firmware" && app.view_.answer_page==1 && !app.view_.entry_from_vocabulary,"nested notebook browse restores original word and definition page");
    tap(Action::Back);check(app.view_.screen==Screen::Dictionary && app.view_.dictionary_query=="firmware","restored detail still returns to input without a navigation loop");
    tap(Action::Vocabulary);frame("08-notebook-page-one");
    app.learning_.state.lesson.book="daily.csv";app.learning_.state.lesson.queue={"existing-old-question"};
    const auto old_queue=app.learning_.state.lesson.queue;
    tap(Action::VocabularyReview);check(app.view_.screen==Screen::Pause && app.view_.vocabulary_review_pending && app.learning_.state.lesson.queue==old_queue,"old lesson prompts instead of silently replacing");frame("29-review-conflict");
    tap(Action::Back);check(app.view_.screen==Screen::Vocabulary && !app.view_.vocabulary_review_pending && app.learning_.state.lesson.queue==old_queue,"cancel returns to notebook without ending old lesson");
    tap(Action::VocabularyReview);tap(Action::EndLesson);
    check(app.view_.screen==Screen::Study && app.learning_.state.lesson.book=="custom-my-vocabulary.csv" && app.learning_.state.lesson.queue.size()==5 && !app.view_.vocabulary_review_pending,"explicit end-and-study enters words directly through normal planner");
    app.learning_.end();app.dispatch({Action::Vocabulary});tap(Action::VocabularyPage,1);frame("09-notebook-page-two");
    check(app.view_.vocabulary.items.size()==1 && app.view_.vocabulary.items[0].word=="appointment","reverse-added pagination");
    tap(Action::VocabularySelect,0);check(app.view_.entry_from_vocabulary && app.dictionary_return_==Screen::Vocabulary,"detail back origin");frame("10-saved-entry");
    tap(Action::Vocabulary);tap(Action::VocabularyFilter,1);frame("11-no-due-words");
    check(app.view_.vocabulary.items.empty(),"new words are not due words");
    tap(Action::VocabularyFilter,2);check(app.view_.vocabulary.page==0,"filter resets page");tap(Action::VocabularyReview);
    check(app.view_.screen==Screen::Study && app.learning_.state.lesson.queue.size()==5,"review route uses actual learning system");
    check(!app.learning_.rate(Rating::Good,100,true,0),"cannot grade before reveal");frame("12-study-question");
    app.learning_.reveal();frame("13-study-answer");app.learning_.rate(Rating::Good,100,true,0);check(app.save(),"save rating");frame("14-next-word");
    const auto queue=app.learning_.state.lesson.queue;const auto position=app.learning_.state.lesson.position;
    app.learning_.pause();app.dispatch({Action::Vocabulary});frame("15-notebook-paused");query("  API  ");
    check(app.view_.screen==Screen::DictionaryEntry && app.dictionary_card_.front=="API","spaced acronym exact keeps case");frame("16-acronym");tap(Action::DictionaryAdd);
    check(app.learning_.state.lesson.queue==queue && app.learning_.state.lesson.position==position,"lookup/add keeps active queue");
    app.dispatch({Action::Vocabulary});tap(Action::VocabularyReview);check(app.learning_.state.lesson.position==position,"resume position");
    while(app.learning_.state.lesson.active()){app.learning_.reveal();app.learning_.rate(Rating::Good,100,true,app.learning_.state.lesson.position);}
    check(app.save(),"finish persist");app.view_.screen=Screen::Done;frame("17-session-done");tap(Action::Vocabulary);frame("18-review-scheduled");
    LearningState loaded;check(load_learning_state(ROOT+"/learning.csv",loaded,error),error);check(loaded.memory[0].size()==5,"reload real personal progress");
    std::vector<Card> reloaded;std::ifstream saved(app.personal_path_);check(wordcard::load_deck(saved,reloaded,error) && reloaded.size()==6,"personal file roundtrip");saved.close();
    check(reloaded[0].id==personal_word_id("appointment") && reloaded[1].ipa==app.personal_cards_[1].ipa,"saved ID and IPA");
    app.day_=103;app.dispatch({Action::VocabularyFilter,1});frame("19-due-review");tap(Action::VocabularyReview);
    check(app.learning_.state.lesson.queue.size()==5,"due-only excludes newly added API");app.learning_.end();
    query("firmw");check(app.view_.screen==Screen::DictionaryResults,"prefix remains candidate list");frame("20-prefix-results");tap(Action::DictionarySelect,0);
    check(app.dictionary_return_==Screen::DictionaryResults,"candidate detail returns to results");
    query("zzzznonexistent");check(app.view_.dictionary_matches.empty(),"no result");frame("21-no-result");
    query("   ");check(app.view_.screen==Screen::Dictionary && !app.notice_.empty(),"whitespace validation");frame("22-empty-validation");
    query("IT");check(app.dictionary_card_.front=="IT","IT exact");const auto upper=app.dictionary_card_.id;
    query("it");check(app.dictionary_card_.front=="it" && app.dictionary_card_.id!=upper,"it not IT");
    for (const char *term:{"C++","C#","usb","api"}) {
        query(term);check(app.view_.screen==Screen::DictionaryEntry && !app.dictionary_card_.ipa.empty(),"technology lookup with IPA");
        std::string text;check(speech_text(app.dictionary_card_.front,text),"technology speakable");
        if (std::string(term)=="C++")check(text=="C plus plus","plus symbols pronounced");
        if (std::string(term)=="C#")check(text=="C sharp","sharp symbol pronounced");
        if (std::string(term)=="usb")check(text=="U S B","USB canonical letters");
    }
    query("coffee");app.dictionary_card_.ipa.clear();frame("23-missing-ipa");app.dictionary_card_.back=std::string(1800,'x');frame("24-long-definition");
    tap(Action::DictionaryPage,1);check(app.view_.answer_page==1,"meaning pagination");
    app.dispatch({Action::Dictionary});app.view_.dictionary_query=std::string(48,'x');app.dispatch({Action::DictionaryKey,'y'});check(app.view_.dictionary_query.size()==48 && !app.notice_.empty(),"bounded input");
    app.view_.dictionary_symbols=true;frame("25-symbol-keyboard");app.dispatch({Action::DictionaryClear});tap(Action::DictionaryKey,'#');check(app.view_.dictionary_query=="#","technology punctuation");
    std::vector<DictionaryMatch> ambiguous{{"IT",1,"科技"},{"it",2,"代词"}};
    check(dictionary_exact_match("It",ambiguous)<0,"mixed case ambiguous not silently chosen");
    const auto before=app.personal_cards_.size();query("reservation");app.view_.writable=false;app.dispatch({Action::DictionaryAdd});check(app.personal_cards_.size()==before,"read-only add blocked");
    // Disk damage cannot be replaced by a successful-looking in-memory append.
    {std::ofstream f(app.personal_path_,std::ios::trunc);f<<"broken";}
    app.dispatch({Action::DictionaryAdd});check(app.personal_cards_.size()==before && app.notice_.find("未能保存")!=std::string::npos,"failed save leaves list unchanged");frame("26-save-failed");
    app.view_.personal_readable=false;app.view_.personal_error="生词本读取失败，原文件已保留";app.dispatch({Action::Vocabulary});frame("27-damaged-notebook");
    // Dates and limits are real schedule data; do not turn browsing into a review.
    auto page=vocabulary_page(reloaded,loaded.memory[0],100,VocabularyFilter::All,99);
    check(page.page==1 && page.fresh==1 && page.due==0 && page.next_day>100,"clamp and future review");
    app.view_.personal_readable=true;app.day_=100;app.learning_.state=loaded;app.learning_.state.new_limit=0;app.dispatch({Action::Vocabulary});app.dispatch({Action::VocabularyFilter,2});frame("28-daily-limit");
    app.view_.guided_enabled=true;app.learning_.state.new_limit=10;app.notice_.clear();
    app.guided_.review.bind_clock(100,true);const auto original_due=app.progress_.at(reloaded.front().id).due_day;
    app.guided_.review.observe(reloaded.front(),"",RecallSkill::Meaning,false);
    app.view_.vocabulary_filter=VocabularyFilter::Due;frame("29-unified-notebook-review");
    check(app.view_.vocabulary.due>0&&app.progress_.at(reloaded.front().id).due_day==original_due,"notebook shows auto due without replacing historical self ratings");
    check(std::any_of(app.ui_.buttons().begin(),app.ui_.buttons().end(),[](const Button&b){return b.event.action==Action::RecallBegin&&b.event.value==8&&b.enabled;}),"notebook due action enters automatic review");
    const auto standalone_native_calls=app.web_.calls;
    app.view_.screen=Screen::WebLearning;app.dispatch({Action::Dictionary});
    check(app.view_.screen==Screen::Dictionary,"root opens full offline dictionary");
    app.web_.succeeds=false;query("firmware");
    check(app.view_.screen==Screen::DictionaryEntry && app.dictionary_card_.front=="firmware",
          "standalone dictionary detail works without the native transport");
    app.web_.succeeds=true;app.dispatch({Action::Back});app.dispatch({Action::Back});
    check(app.view_.screen==Screen::WebLearning,"full dictionary returns exact web root");
    check(app.web_.calls==standalone_native_calls,"standalone dictionary does not report native exposure");
    app.view_.screen=Screen::WebTools;app.dispatch({Action::Dictionary});app.dispatch({Action::Back});check(app.view_.screen==Screen::WebTools,"offline dictionary preserves settings origin too");
    // Native lookup fallback keeps its own query, saves to the same native
    // notebook, and cannot open a legacy practice session.
    auto read_text=[](const std::string &path){std::ifstream f(path,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});};
    const auto personal_before_native=read_text(app.personal_path_);
    const auto native_queue=app.learning_.state.lesson.queue;
    const auto native_position=app.learning_.state.lesson.position;
    const auto native_personal_count=app.personal_cards_.size();
    app.view_.screen=Screen::WebLearning;app.view_.web.key="vocabulary:search:fixture";
    app.view_.web.route="search";app.view_.web.typed="firmw";app.view_.web_page=2;
    app.view_.dictionary_query="stale query";app.view_.dictionary_recent={"stale suggestion"};
    app.dispatch({Action::Dictionary});
    check(app.view_.dictionary_native_lookup && app.view_.dictionary_query=="firmw" &&
          app.view_.dictionary_recent.empty(),"native fallback copies original query and drops stale suggestions");
    frame("33-native-full-input");
    const auto before_prefix_card=app.dictionary_card_.front;
    app.web_.succeeds=false;tap(Action::DictionarySearch);
    check(app.view_.screen==Screen::Dictionary && app.view_.dictionary_matches.empty() &&
          app.view_.dictionary_recent.empty() && app.dictionary_card_.front==before_prefix_card &&
          app.view_.dictionary_query=="firmw" && app.view_.web.typed=="firmw" && app.view_.web_page==2,
          "failed prefix exposure retains original input, card, native draft and page");
    app.web_.succeeds=true;
    app.web_.status_json=StudyJson{{"courseError","提示保存失败，原记录保留"}}.dump();
    tap(Action::DictionarySearch);
    check(app.view_.screen==Screen::Dictionary && app.view_.dictionary_matches.empty() &&
          app.notice_=="提示保存失败，原记录保留","prefix save failure never publishes answer candidates");
    app.web_.status_json=StudyJson{{"courseError",""}}.dump();app.web_.exposed_words.clear();
    tap(Action::DictionarySearch);check(app.view_.screen==Screen::DictionaryResults,"native prefix reaches results");
    check(app.web_.exposed_words.size()==std::min<size_t>(6,app.view_.dictionary_matches.size()),
          "prefix exposure reports exactly the rendered result count");
    for(size_t i=0;i<app.web_.exposed_words.size();++i)
        check(app.web_.exposed_words[i]==app.view_.dictionary_matches[i].word,
              "prefix exposure follows the visible six-row order");
    check(std::find(app.web_.exposed_words.begin(),app.web_.exposed_words.end(),"firmware")!=
              app.web_.exposed_words.end(),"visible current-word candidate is reported before selection");
    frame("34-native-full-results");
    const auto before_native_entry=app.dictionary_card_.front;
    const auto check_exposure_blocked=[&](const std::string &why) {
        check(app.view_.screen==Screen::DictionaryResults &&
              app.dictionary_card_.front==before_native_entry && !app.notice_.empty(),why);
        check(app.learning_.state.lesson.queue==native_queue &&
              app.learning_.state.lesson.position==native_position && app.view_.web.typed=="firmw" &&
              app.view_.web_page==2,"failed exposure retains learning position, query and page");
    };
    app.web_.succeeds=false;tap(Action::DictionarySelect,0);
    check_exposure_blocked("failed exposure command does not reveal the full entry");
    check(app.web_.last_action=="v-lookup-exposure" && app.web_.last_value=="firmware",
          "dictionary reports the selected spelling before revealing it");
    app.web_.succeeds=true;app.web_.status_ok=false;tap(Action::DictionarySelect,0);
    check_exposure_blocked("missing exposure status does not reveal the full entry");
    app.web_.status_ok=true;app.web_.status_json="not json";tap(Action::DictionarySelect,0);
    check_exposure_blocked("malformed exposure status does not reveal the full entry");
    app.web_.status_json="{}";tap(Action::DictionarySelect,0);
    check_exposure_blocked("incomplete exposure status does not reveal the full entry");
    app.web_.status_json=StudyJson{{"courseError","提示保存失败，原记录保留"}}.dump();
    tap(Action::DictionarySelect,0);
    check_exposure_blocked("exposure save error preserves the previous dictionary page");
    check(app.notice_=="提示保存失败，原记录保留","exposure failure shows the actual storage error");
    app.web_.status_json=StudyJson{{"courseError",""}}.dump();tap(Action::DictionarySelect,0);
    check(app.view_.screen==Screen::DictionaryEntry && app.dictionary_card_.front=="firmware",
          "native detail opens after a successful exposure retry");
    check(!app.view_.dictionary_added,"old notebook membership does not masquerade as native membership");
    frame("35-native-full-entry");
    check(std::any_of(app.ui_.buttons().begin(),app.ui_.buttons().end(),[](const Button &b){
        return b.event.action==Action::DictionaryAdd && b.primary && b.rect.y==1080 && b.rect.w==604;
    }),"native full-entry primary is anchored at the bottom");
    for(const auto action:{Action::RecallHome,Action::RecallBegin,Action::RecallResume,Action::VocabularyReview}) {
        check(app.dictionary_native_dispatch({action}),"native lookup consumes legacy learning entry");
        check(app.view_.screen==Screen::DictionaryEntry && app.learning_.state.lesson.queue==native_queue &&
              app.learning_.state.lesson.position==native_position,"blocked old entry preserves screen and queue");
    }
    app.web_.status_json=StudyJson{{"courseError","保存失败，原记录保留"}}.dump();
    tap(Action::DictionaryAdd);
    check(app.notice_=="保存失败，原记录保留" && !app.view_.dictionary_added,
          "native add shows actual save error without a success state");
    const auto payload=StudyJson::parse(app.web_.last_value);
    check(app.web_.last_action=="v-import" && str(payload,"id")=="dict:"+app.dictionary_card_.id &&
          str(payload,"word")==app.dictionary_card_.front && str(payload,"meaning")==app.dictionary_card_.back &&
          str(payload,"ipa")==app.dictionary_card_.ipa,"native import carries stable dictionary ID and exact text");
    app.web_.status_json=StudyJson{{"courseError",""},{"lastImportedWord","another word"},{"lastImportedId","dict:test"}}.dump();
    tap(Action::DictionaryAdd);check(!app.view_.dictionary_added && app.notice_.find("未能保存")!=std::string::npos,
          "stale import receipt cannot confirm a different word");
    app.web_.status_json=StudyJson{{"courseError",""},{"lastImportedWord",app.dictionary_card_.front},
                                  {"lastImportedId","dict:"+app.dictionary_card_.id}}.dump();
    // Damage in the separate historical notebook cannot block a native save.
    app.view_.personal_readable=false;tap(Action::DictionaryAdd);
    check(app.view_.dictionary_added && app.notice_=="已加入生词本" && app.view_.screen==Screen::DictionaryEntry,
          "verified native receipt confirms save while retaining entry");
    const auto after_native_add_calls=app.web_.calls;app.dispatch({Action::DictionaryAdd});
    check(app.web_.calls==after_native_add_calls,"repeated confirmed add is inert");
    check(read_text(app.personal_path_)==personal_before_native && app.personal_cards_.size()==native_personal_count &&
          app.learning_.state.lesson.queue==native_queue && app.learning_.state.lesson.position==native_position,
          "native import does not touch historical notebook or learning queue");
    app.view_.personal_readable=true;frame("36-native-full-added");
    tap(Action::Back);check(app.view_.screen==Screen::DictionaryResults,"native full detail returns to result list");
    tap(Action::Back);check(app.view_.screen==Screen::Dictionary && app.view_.dictionary_query=="firmw","native results restore full query");
    tap(Action::Back);check(app.view_.screen==Screen::WebLearning && !app.view_.dictionary_native_lookup &&
          app.view_.web.typed=="firmw" && app.view_.web_page==2,"native fallback restores exact original query and page");
    std::vector<DictionaryMatch> unrelated_prefix;
    check(app.dictionary_.search("appoin",unrelated_prefix,error) && unrelated_prefix.size()==6 &&
          dictionary_exact_match("appoin",unrelated_prefix)<0 &&
          std::none_of(unrelated_prefix.begin(),unrelated_prefix.end(),[](const DictionaryMatch &match){
              return match.word=="firmware";
          }),"unrelated fixture is a real six-row prefix, not an exact dictionary headword");
    app.dispatch({Action::Dictionary});app.view_.dictionary_query="appoin";
    app.web_.exposed_words.clear();tap(Action::DictionarySearch);
    check(app.view_.screen==Screen::DictionaryResults && !app.web_.exposed_words.empty() &&
          std::find(app.web_.exposed_words.begin(),app.web_.exposed_words.end(),"firmware")==
              app.web_.exposed_words.end(),"unrelated prefix does not report a hidden current-word answer");
    for(size_t i=0;i<app.web_.exposed_words.size();++i)
        check(i<6 && app.web_.exposed_words[i]==app.view_.dictionary_matches[i].word,
              "unrelated prefix reports only actually rendered words");
    tap(Action::Back);app.view_.dictionary_query="appointment";app.web_.exposed_words.clear();
    const auto before_exact_first=app.view_.dictionary_matches.front().word;
    const auto before_exact_card=app.dictionary_card_.front;
    app.web_.succeeds=false;tap(Action::DictionarySearch);
    check(app.view_.screen==Screen::Dictionary && app.view_.dictionary_query=="appointment" &&
          app.dictionary_card_.front==before_exact_card &&
          app.view_.dictionary_matches.front().word==before_exact_first,
          "failed exact exposure restores the previous matches and leaves the input screen open");
    app.web_.succeeds=true;app.web_.exposed_words.clear();tap(Action::DictionarySearch);
    check(app.view_.screen==Screen::DictionaryEntry && app.web_.exposed_words.size()==1 &&
          app.web_.exposed_words.front()=="appointment",
          "exact lookup reports only the opened entry, without unseen prefix alternatives");
    tap(Action::Back);tap(Action::Back);
    app.dispatch({Action::Dictionary});app.web_.succeeds=false;tap(Action::Vocabulary);
    check(app.view_.screen==Screen::Dictionary && app.view_.dictionary_native_lookup,"failed notebook transition stays in full dictionary");
    app.web_.succeeds=true;tap(Action::Vocabulary);
    check(app.view_.screen==Screen::WebLearning && !app.view_.dictionary_native_lookup &&
          app.web_.last_action=="u-notebook","full dictionary notebook entry routes to native notebook");
    app.view_.screen=Screen::WebTools;app.dispatch({Action::Dictionary});
    check(!app.view_.dictionary_native_lookup,"device tools retains its original dictionary path");
    tap(Action::Back);app.view_.screen=Screen::WebLearning;app.view_.web.key="legacy:dictionary";
    app.dispatch({Action::Dictionary});check(!app.view_.dictionary_native_lookup,"legacy web lookup does not enter native bridge");
    tap(Action::Back);
    // Visual long-content fixtures: rows disclose truncation and full detail
    // still renders the phrase and paginated definition at the readable size.
    const std::string phrase="application programming interface architecture";
    const std::string meaning="应用程序编程接口架构；用于连接多个软件系统并交换数据的设计方式。";
    app.view_.screen=Screen::DictionaryResults;app.view_.dictionary_query="application";
    app.view_.dictionary_matches={{phrase,1,meaning}};app.notice_.clear();frame("30-long-result-preview");
    app.personal_cards_.back().front=phrase;app.personal_cards_.back().back=meaning;
    app.view_.screen=Screen::Vocabulary;app.view_.vocabulary_filter=VocabularyFilter::All;app.view_.vocabulary.page=0;frame("31-long-notebook-preview");
    app.dictionary_card_.front=phrase;app.dictionary_card_.back=meaning;app.dictionary_card_.ipa.clear();app.view_.answer_page=0;app.view_.screen=Screen::DictionaryEntry;frame("32-long-entry-detail");
    std::cout<<"PASS: "<<checks<<" dictionary/notebook checks, "<<frames<<" native frames; actual dispatch cases, SD dictionary, durable save, queue, scheduling, missing/error states. Hardware audio/NVS are stubs.\n";
    return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
