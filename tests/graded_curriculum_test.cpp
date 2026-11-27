#include "wordcard_ui.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace wordcard;
namespace fs=std::filesystem;
static int checks=0;
static void require(bool ok,const std::string &why){++checks;if(!ok)throw std::runtime_error(why);}
static uint32_t digest(const std::string&s){uint32_t h=2166136261u;for(unsigned char c:s){h^=c;h*=16777619u;}return h;}
static std::string legacy_fixture(){
    std::ostringstream s;s<<"4 3 8 8 0 0 8 255 255 0 0 1 0 0\n";
    for(int c=0;c<12;++c)s<<(c==4?15:0)<<' ';s<<'\n';
    for(int c=0;c<12;++c)s<<(c==4?15:0)<<' ';s<<"\n0\n0\n";
    return "guided-v1 "+std::to_string(digest(s.str()))+"\n"+s.str();
}
static void roundtrip(GuidedLearning &g){GuidedLearning other;std::string error;
    const auto bytes=g.encode();require(other.decode(bytes,error),"state decode: "+error);
    require(other.encode()==bytes,"lossless v3 state");g=std::move(other);
}
static void correct(GuidedLearning &g){const auto opts=g.options();const auto answer=g.expected_answer();
    const auto it=std::find(opts.begin(),opts.end(),answer);require(it!=opts.end(),"authored correct option exists");
    require(g.choose(int(it-opts.begin())),"select correct");
    if(g.listening()){require(!g.check(),"unheard audio cannot score");g.played(false);require(!g.check(),"failed audio cannot score");g.played(true);}
    require(g.check()&&g.state.correct,"correct judgement");
}
int main(int argc,char **argv){try{
    require(argc==3,"font and output arguments");fs::create_directories(argv[2]);
    std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),{}),fb(Canvas::bytes);
    Font font;require(font.init(data.data(),data.size()),"device font");Canvas canvas(fb.data(),font);UI ui;
    auto render=[&](GuidedLearning &g,Screen screen,const std::string &name,int browse=-1){
        View v;v.guided_enabled=true;v.guided=g.view(browse);v.screen=screen;v.writable=true;
        v.audio_available=true;v.audio_source=SpeechSource::Sd;ui.render(canvas,v);
        const auto &buttons=ui.buttons();
        for(const auto &b:buttons){require(b.rect.x>=0&&b.rect.y>=0&&b.rect.x+b.rect.w<=684&&b.rect.y+b.rect.h<=1216,"button bounds: "+b.label);
            require(canvas.wrap(b.label,b.rect.w-16).size()<=size_t(std::max(1,b.rect.h/38)),"button text fits: "+b.label);
            if(b.enabled){auto e=ui.hit(b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2);require(e.action==b.event.action&&e.value==b.event.value,"real button hit: "+b.label);}}
        for(size_t i=0;i<buttons.size();++i)for(size_t j=i+1;j<buttons.size();++j){auto a=buttons[i].rect,b=buttons[j].rect;
            require(a.x>=b.x+b.w||b.x>=a.x+a.w||a.y>=b.y+b.h||b.y>=a.y+a.h,"no overlap");}
        if(!name.empty()){std::ofstream out(fs::path(argv[2])/(name+".pgm"),std::ios::binary);out<<"P5\n684 1216\n255\n";
            for(int y=0;y<1216;++y)for(int x=0;x<684;++x)out.put(char(canvas.shade(x,y)*17));}
    };
    GuidedLearning g;require(g.recommended_course()==12,"new learner starts foundation");
    render(g,Screen::Home,"00-home");render(g,Screen::LearningStart,"01-start");render(g,Screen::GuidedCourses,"02-catalog");
    g.catalog_track=1;render(g,Screen::GuidedCourses,"03-tech-catalog");require(g.catalog()==std::vector<int>({4,6,5,7}),"technology teaching order independent of old indices");
    g.catalog_level=0;require(g.catalog().empty(),"honest empty combination");render(g,Screen::GuidedCourses,"04-empty-filter");
    g.catalog_track=-1;g.catalog_level=-1;g.profile={true,1,1,-1};roundtrip(g);require(g.recommended_course()==4,"technology self-reported start");
    render(g,Screen::Home,"05-tech-home");render(g,Screen::CourseDetail,"06-tech-detail",4);
    g.profile.start=2;require(g.recommended_course()==5,"independent technology start");
    g.profile.start=1;require(g.begin(4,0),"begin recommended course");g.profile={true,0,0,-1};require(g.recommended_course()==4,"active session outranks changed profile");
    render(g,Screen::GuidedLesson,"07-foundation-or-tech-word");
    std::string error;GuidedLearning migrated;
    const auto legacy=legacy_fixture();require(migrated.decode(legacy,error),"v1 migration");
    require(migrated.state.completed[4]==15&&migrated.state.listened[4]==15,"old four sections preserved");
    require(migrated.next_node(4)==4,"new dialogue sections not falsely completed");
    require(migrated.profile.last_course==4,"old chosen course becomes continue target");
    LearningReview old_review;old_review.register_card(guided_courses()[0].items[0].card,"重复");
    auto r=old_review.encode();auto payload=std::to_string(legacy.size())+" "+std::to_string(r.size())+"\n"+legacy+r;
    require(migrated.decode("guided-v2 "+std::to_string(digest(payload))+"\n"+payload,error),"v2 unified migration");
    require(migrated.review.records.size()==1,"review memory retained");roundtrip(migrated);
    auto good=migrated.encode(),bad=good;bad.back()='x';require(!migrated.decode(bad,error)&&migrated.encode()==good,"corruption preserves live state");
    require(!g.begin(-1,0)&&!g.begin(guided_course_count,0),"invalid course rejected");
    std::set<std::string> identities;
    for(int course=0;course<guided_course_count;++course){
        GuidedLearning all;
        for(const auto &item:guided_courses()[course].items)require(identities.insert(item.card.id).second,"global unique stable ID");
        for(const auto &task:guided_courses()[course].tasks){
            require(canvas.wrap(task.passage,604).size()<=7,"dialogue passage fits: "+task.passage);
            require(canvas.wrap(task.question,604).size()<=2,"question fits");
            require(canvas.wrap(task.explanation,604).size()<=4,"feedback fits");
        }
        for(int node=0;node<guided_node_count;++node){
            require(all.begin(course,node),"all 120 sections reachable");int guard=0;
            while(all.state.active){require(++guard<35,"bounded lesson sequence");
                if(all.view().kind==GuidedKind::Introduce){auto item=all.view();
                    require(canvas.wrap(item.text,604,2).size()<=3,"all course English introductions fit");
                    require(canvas.wrap(item.ipa,604).size()<=3,"all course IPA fits");
                    require(canvas.wrap(item.usage,604).size()<=4,"all course introduction meaning fits");}
                if(node>=4)render(all,Screen::GuidedLesson,course==12&&guard==1?(node==4?"08-read-dialogue":"09-listen-dialogue"):"");
                if(all.view().kind!=GuidedKind::Introduce){
                    if(node==5&&guard==1){require(all.skip_listening(),"listen to reading fallback");require(!all.listening(),"fallback is reading");roundtrip(all);}
                    correct(all);
                    if(course==12&&node==4&&guard==1)render(all,Screen::GuidedLesson,"10-reading-feedback");
                    require(!all.check(),"no duplicate judgement");
                }
                roundtrip(all);require(all.advance(),"advance completed task");
            }
            require(all.state.completed[course]&(1u<<node),"section persisted");
            if(node==5)require(!(all.state.listened[course]&(1u<<node)),"skipped dialogue remains due to listen");
        }
        require(all.state.completed[course]==guided_complete_mask,"six sections complete");
        require(all.next_node(course)==5,"recommend catch-up listening");
        require(all.begin(course,5),"revisit deferred dialogue");
        while(all.state.active){correct(all);all.advance();}
        require(all.state.listened[course]==guided_complete_mask,"only actual heard answers finish listening");
        if(course==12)render(all,Screen::GuidedDone,"11-course-complete");
    }
    // Wrong comprehension answers retry, survive a restart and never create false word memory.
    GuidedLearning wrong;wrong.state.completed[12]=15;wrong.begin(12,4);auto opts=wrong.options();
    int incorrect=opts[0]==wrong.expected_answer()?1:0;wrong.choose(incorrect);require(wrong.check()&&!wrong.state.correct,"wrong reading answer");
    render(wrong,Screen::GuidedLesson,"12-wrong-feedback");roundtrip(wrong);wrong.advance();
    require(wrong.state.pending.size()==2&&wrong.review.records.empty(),"retry preserved without fake lexical record");
    // Genuine failed/pending playback and save-failure states must be visible, never open the audio gate.
    for(int mode=0;mode<3;++mode){
        GuidedLearning dialogue;dialogue.state.completed[12]=31;dialogue.begin(12,mode==2?4:5);
        View notice;notice.guided_enabled=true;notice.guided=dialogue.view();notice.screen=Screen::GuidedLesson;
        notice.audio_available=true;notice.writable=mode!=2;notice.audio_busy=mode==0;
        if(mode==1)notice.notice="播放失败，请重试或改为阅读。";
        ui.render(canvas,notice);bool visible=false;
        for(int y=1034;y<1068;++y)for(int x=40;x<644;++x)visible|=canvas.shade(x,y)<15;
        require(visible,"dialogue status rendered in reserved line");
        require(!dialogue.view().can_check,"status never bypasses answer/audio gate");
        std::ofstream out(fs::path(argv[2])/("13-dialogue-status-"+std::to_string(mode)+".pgm"),std::ios::binary);
        out<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x)out.put(char(canvas.shade(x,y)*17));
    }
    while(wrong.state.active){correct(wrong);wrong.advance();}require(wrong.state.errors==1&&wrong.state.first_correct==1,"honest first-attempt reading count");
    const auto path=(fs::path(argv[2])/"state-v3.txt").string();require(wrong.save(path,error),"save new state");require(wrong.save(path,error),"backup write");
    fs::remove(path);GuidedLearning recovered;require(recovered.load(path,error)&&recovered.encode()==wrong.encode(),"backup recovery");
    LearningReview focused;focused.bind_clock(100,true);auto &record=focused.register_card(guided_courses()[12].items[0].card,"名字");
    record.memory[0].attempts=1;record.memory[0].due=200;record.memory[2].attempts=1;record.memory[2].due=99;
    require(focused.counts().due==0&&!focused.begin(RecallFilter::Due,10,0),"default adult review excludes spelling-only due work");
    require(focused.begin(RecallFilter::Spelling,1),"writing remains an explicit optional exercise");
    std::cout<<"PASS: "<<checks<<" graded curriculum / migration / audio-gate / native UI checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
