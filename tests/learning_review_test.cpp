#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace wordcard;
namespace fs=std::filesystem;
static unsigned checks=0;
static void require(bool value,const std::string&why){++checks;if(!value)throw std::runtime_error(why);}
static void select_answer(LearningReview&r,bool correct=true){
    const auto*item=r.current();require(item!=nullptr,"current task");
    if(r.typing())r.session.typed=correct?item->card.front:"incorrect-spelling";
    else {
        const auto target=r.session.queue.front().kind==RecallKind::Reverse?item->card.front:item->meaning;
        const auto it=std::find(r.session.options.begin(),r.session.options.end(),target);
        require(it!=r.session.options.end(),"answer in choices");
        r.session.selected=int(it-r.session.options.begin());
        if(!correct)r.session.selected=(r.session.selected+1)%int(r.session.options.size());
    }
}
int main(int argc,char**argv){try{
    require(argc==3,"font/output args");fs::create_directories(argv[2]);
    std::ifstream font_file(argv[1],std::ios::binary);std::vector<uint8_t> fontbytes((std::istreambuf_iterator<char>(font_file)),{}),fb(Canvas::bytes);
    Font font;require(font.init(fontbytes.data(),fontbytes.size()),"real device font");Canvas canvas(fb.data(),font);UI ui;
    GuidedLearning g;auto&r=g.review;
    for(const auto&course:guided_courses())for(const auto&item:course.items)r.register_card(item.card,item.meaning);
    const auto a=guided_courses()[0].items[0],b=guided_courses()[4].items[0];
    auto render=[&](Screen screen,const char*name,const std::string&notice="",bool audio=true){
        View v;v.guided_enabled=true;v.writable=true;v.date_valid=r.dated;v.date_label="10月28日";v.recall=r.session;v.recall_counts=r.counts();
        v.screen=screen;v.recall_card=r.current();v.recall_ready=r.ready();v.recall_heard=r.heard;v.recall_typing=r.typing();v.recall_listening=r.listening();v.audio_available=audio;v.notice=notice;
        ui.render(canvas,v);
        for(const auto&button:ui.buttons()){
            const auto&box=button.rect;require(box.x>=0&&box.y>=0&&box.x+box.w<=684&&box.y+box.h<=1216,"control in bounds: "+button.label);
            require(canvas.wrap(button.label,box.w-16).size()<=size_t(std::max(1,box.h/38)),"control text fits: "+button.label);
            if(button.enabled){const auto hit=ui.hit(box.x+box.w/2,box.y+box.h/2);require(hit.action==button.event.action&&hit.value==button.event.value,"real hit matches: "+button.label);}
        }
        for(size_t i=0;i<ui.buttons().size();++i)for(size_t j=i+1;j<ui.buttons().size();++j){const auto&a=ui.buttons()[i].rect;const auto&b=ui.buttons()[j].rect;require(a.x>=b.x+b.w||b.x>=a.x+a.w||a.y>=b.y+b.h||b.y>=a.y+a.h,"no overlapping controls");}
        std::ofstream out(fs::path(argv[2])/(std::string(name)+".pgm"),std::ios::binary);out<<"P5\n684 1216\n255\n";
        for(int y=0;y<1216;++y)for(int x=0;x<684;++x)out.put(char(canvas.shade(x,y)*17));
    };
    auto roundtrip=[&](){const auto bytes=g.encode();GuidedLearning copy;std::string error;require(copy.decode(bytes,error),"load unified state: "+error);require(copy.encode()==bytes,"all study data round trips");g=std::move(copy);};
    r.bind_clock(20000,true);r.observe(a.card,a.meaning,RecallSkill::Meaning,true);
    require(r.records[a.card.id].memory[0].due==20001,"first independent success schedules tomorrow");
    r.observe(a.card,a.meaning,RecallSkill::Meaning,true);require(r.records[a.card.id].memory[0].interval==1,"same-day repetition does not grow interval");
    r.observe(b.card,b.meaning,RecallSkill::Meaning,false);r.observe(b.card,b.meaning,RecallSkill::Meaning,true);
    require(r.records[b.card.id].memory[0].weak,"immediate correction remains weak");
    r.defer_listening(a.card,a.meaning);render(Screen::RecallHome,"00-today");roundtrip();r.bind_clock(20000,true);
    require(r.counts().mistakes==1&&r.counts().listening==1,"mistakes and skipped listening survive reboot");
    require(r.begin(RecallFilter::Listening),"targeted listening without full lesson replay");render(Screen::RecallLesson,"01-listen");
    render(Screen::RecallLesson,"08-audio-failed","音频驱动未就绪，可暂缓并检查硬件自检。",false);
    select_answer(r);require(!r.check(),"unplayed listening cannot score");r.heard=true;require(r.check(),"successful playback enables grading");
    require(!r.check(),"duplicate check never scores twice");render(Screen::RecallLesson,"02-listen-feedback");roundtrip();r.bind_clock(20000,true);require(r.next(),"continue feedback after reboot");
    require(r.counts().listening==0,"only correct heard content clears listening debt");
    require(r.begin(RecallFilter::Mistakes),"long-term mistake practice");select_answer(r,false);require(r.check()&&!r.session.correct,"wrong recall graded");require(r.next()&&r.current(),"wrong recall retried");select_answer(r);require(r.check()&&r.next(),"retry clears session task");require(r.records[b.card.id].memory[0].weak,"retry not long-term mastery");
    r.bind_clock(20001,true);require(r.begin(RecallFilter::Mistakes),"next day mistaken items remain");select_answer(r);require(r.check()&&r.next(),"later independent answer");require(!r.records[b.card.id].memory[0].weak,"later correct answer resolves weakness");
    require(r.begin(RecallFilter::Spelling,1),"spelling draws learned content");render(Screen::RecallLesson,"03-spelling");select_answer(r,false);require(r.check()&&!r.session.correct,"misspelling rejected");require(r.next(),"retry spelling");select_answer(r);r.session.hint=true;require(r.check(),"hint-assisted result counted separately");render(Screen::RecallLesson,"09-assisted-feedback");require(r.next(),"schedule independent retry");
    require(r.counts().mistakes>=1&&r.current()&&r.session.solved==0&&!r.session.hint,"assisted answer schedules independent retry without claiming solved");
    select_answer(r);require(r.check()&&r.next()&&!r.current(),"unassisted retry can finish");
    require(r.begin(RecallFilter::Dictation,1),"dictation available");render(Screen::RecallLesson,"04-dictation");select_answer(r);require(!r.check(),"dictation also requires hearing");r.heard=true;require(r.check()&&r.next(),"dictation completed");
    require(dictionary_keyboard_hit(300,680,false,false,true,0,true).event.action==Action::RecallSkip,"typing skip has real captured hit target");
    require(dictionary_keyboard_hit(300,680,false,false,true,0,false).event.action==Action::None,"dictionary keyboard behavior unchanged");
    Card phrase;phrase.front="Are you ready?";phrase.tags="adult|sentence";require(!LearningReview::spellable(phrase),"dictation excludes sentence punctuation ambiguity");
    require(r.begin(RecallFilter::Reverse,1),"Chinese-to-English choice available");render(Screen::RecallLesson,"05-reverse");select_answer(r);require(r.check()&&r.next(),"reverse correct");render(Screen::RecallDone,"06-done");
    const auto total=r.records[a.card.id].memory[0].attempts;
    Progress old;old.seen=17;old.good=10;old.interval=7;old.due_day=20008;old.box=4;
    r.import_legacy(a.card,old,StudyMode::Visual,false);
    {
        LearningReview imported;Progress good;good.seen=1;good.good=1;good.box=1;
        imported.import_legacy(a.card,good,StudyMode::Visual,false);
        require(!imported.records[a.card.id].memory[0].weak,"young successful legacy card is not a mistake");
        good.hard=1;imported.import_legacy(a.card,good,StudyMode::Visual,false);
        require(imported.records[a.card.id].memory[0].weak,"explicit legacy difficulty remains available");
    }
    require(r.records[a.card.id].memory[0].attempts==total,"legacy self ratings not fabricated as automatic scores");
    r.observe(a.card,a.meaning,RecallSkill::Meaning,true);const auto schedule=r.records[a.card.id].memory[0].due;
    r.import_legacy(a.card,old,StudyMode::Visual,false);require(r.records[a.card.id].memory[0].due==schedule,"unchanged legacy data does not reset schedule");
    old.due_day=20012;old.good=11;r.import_legacy(a.card,old,StudyMode::Visual,false);require(r.records[a.card.id].memory[0].due==20012,"undo/re-rate changes with same seen count are detected");
    ProgressMap legacy;legacy[a.card.id]=old;
    r.observe(a.card,a.meaning,RecallSkill::Meaning,false);
    auto visible=r.display_progress(legacy,{a.card});require(visible.at(a.card.id).due_day==r.day&&legacy.at(a.card.id).due_day==20012,"notebook projects current auto schedule without rewriting old ratings");
    r.bind_clock(0,false);r.observe(a.card,a.meaning,RecallSkill::Listening,true);require(r.records[a.card.id].memory[1].undated,"untrusted clock records undated activity");render(Screen::RecallHome,"07-date-needed");roundtrip();
    auto before=g.encode();std::string error;auto corrupt=before;corrupt.back()='!';require(!g.decode(corrupt,error)&&g.encode()==before,"damaged new state cannot overwrite memory");
    require(!g.decode(before.substr(0,before.size()/2),error),"truncated state rejected");
    std::string old_payload="0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
    for(int line=0;line<2;++line){for(int c=0;c<12;++c)old_payload+="0 ";old_payload+='\n';}old_payload+="0\n0\n";
    GuidedLearning old_import;require(old_import.decode("guided-v1 "+std::to_string(LearningReview::stable_hash(old_payload))+"\n"+old_payload,error),"old guided-v1 state remains readable");
    require(old_import.review.records.empty(),"old completion not invented as word-level grades");
    const auto path=(fs::path(argv[2])/"state.txt").string();require(g.save(path,error),"atomic new state save");require(g.save(path,error),"backup commit");fs::remove(path);GuidedLearning restored;require(restored.load(path,error)&&restored.encode()==g.encode(),"recover interrupted rename");
    for(int c=0;c<12;++c)for(int n=0;n<6;++n){
        const auto&item=guided_courses()[c].items[n];LearningReview local;local.bind_clock(21000,true);
        for(const auto&candidate:guided_courses()[c].items)local.register_card(candidate.card,candidate.meaning);
        local.observe(item.card,item.meaning,RecallSkill::Meaning,true);
        for(auto filter:{RecallFilter::Spelling,RecallFilter::Reverse,RecallFilter::Dictation}){
            require(local.begin(filter,1),"all adult material supports focused recall");select_answer(local);local.heard=true;require(local.check()&&local.session.correct&&local.next(),"all canonical answers accepted");
            LearningReview copy;require(copy.decode(local.encode()),"all completed recall states restore");
        }
    }
    std::cout<<"PASS: "<<checks<<" review scheduling, legacy provenance, listening, input, recovery and native UI checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
