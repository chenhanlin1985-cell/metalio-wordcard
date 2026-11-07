#include "learning_web.hpp"
#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include "web_keyboard.hpp"
#include "web_disclosure.hpp"
#include "web_refresh.hpp"
#include "web_frame_cache.hpp"
#include "audio_identity.hpp"
#include "keyboard_fast_refresh.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
using namespace wordcard;namespace fs=std::filesystem;
static int checks=0;
static std::string lastCommand;
static void req(bool x,const std::string&why){++checks;if(!x)throw std::runtime_error(why+" LAST: "+lastCommand);}
static void run(LearningWeb&m,const std::string&s){lastCommand=s;const bool ok=m.eval(s);req(ok,m.error());}
static void yes(LearningWeb&m,const std::string&s){std::string r;const bool ok=m.eval(s,&r);req(ok,m.error());req(r=="true",s+" => "+r);}
static WebPage page(LearningWeb&m){WebPage p;const bool ok=m.page(p);req(ok,m.error());return p;}
static void click(LearningWeb&m,const std::string&a){auto p=page(m);for(size_t i=0;i<p.actions.size();++i)if(p.actions[i].find("\"action\":\""+a+"\"")!=std::string::npos){const bool ok=m.action(int(i));req(ok,m.error());return;}for(const auto&b:p.blocks)std::cerr<<b.kind<<":"<<b.text<<"\n";for(const auto&b:p.actions)std::cerr<<b<<"\n";throw std::runtime_error("Missing visible "+a+" in "+p.route);}
static Font font;static std::vector<uint8_t> fontBytes,fb(Canvas::bytes);static UI ui;
static void frame(LearningWeb&m,const fs::path&out,const std::string&name,Screen screen=Screen::WebLearning,const std::string&notice=""){
 View v;v.screen=screen;v.notice=notice;v.web=page(m);Canvas c(fb.data(),font);fs::create_directories(out);int count=1;
 for(int part=0;part<count;++part){v.web_page=part;ui.render(c,v);count=ui.answer_pages();const auto filename=name+(part?"-page"+std::to_string(part+1):"");
  if(screen==Screen::WebPrompt)req(std::any_of(ui.buttons().begin(),ui.buttons().end(),[](const auto&b){return b.event.action==Action::Back&&b.label=="返回"&&b.enabled;}),"all disclosure pages expose actual Back");
  for(const auto&b:ui.buttons()){req(b.rect.x>=0&&b.rect.y>=0&&b.rect.x+b.rect.w<=684&&b.rect.y+b.rect.h<=1216,"panel bounds "+b.label);if(!v.web.keyboard&&screen==Screen::WebLearning){if(b.enabled)req(b.rect.h>=88,"finger target height "+b.label);if(b.enabled){const auto e=ui.hit(b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2);req(e.action==b.event.action&&e.value==b.event.value,"rendered center dispatch "+b.label);}}}
  if(!v.web.keyboard&&screen==Screen::WebLearning)for(size_t i=0;i<ui.buttons().size();++i)for(size_t j=i+1;j<ui.buttons().size();++j){const auto&a=ui.buttons()[i].rect;const auto&b=ui.buttons()[j].rect;req(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y,"nonoverlapping targets "+name);}
  std::ofstream p(out/(filename+".pgm"),std::ios::binary);p<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x){const char b=char(c.shade(x,y)*17);p.write(&b,1);}std::ofstream a(out/(filename+".json"));a<<"{\"route\":\""<<v.web.route<<"\",\"pages\":"<<count<<",\"buttons\":"<<ui.buttons().size()<<"}";
 }
 if(name=="02-home-due"||name=="04-meaning"||name=="08-learn-group"||name=="11-listen-unheard")req(count==1,"core task fits one screen "+name);
}
static Event touch(const WebPage&p,int x,int y){const auto s=web_keyboard_state(p);KeyboardInput k;k.publish(p.shift,p.symbols,s.ready,s.count,s.recall,s.enabled,s.compactStudy,s.nativeSearch);k.sample(false,0,0,1000);k.sample(true,x,y,2000);k.sample(false,x,y,3000);Event e;k.pop(e);return e;}
static const std::string setup="globalThis.t={...nativeDebug,B:WORDCARD_MODEL.wordBatch,D:WORDCARD_MODEL.daily};Date.now=()=>Date.parse('2026-11-01T10:00:00+08:00');t.ids=t.B.catalog.slice(0,4).map(w=>w.id);t.p=t.pathModel;t.m=t.model;";
static const std::string plan="t.p.goal('all');t.p.preference('minutes',10);t.p.preference('days',7);t.p.preference('context','daily');t.p.confirmPlan();t.go('home');";
static void answer(LearningWeb&m){run(m,"{const s=t.m.state.batch,q=t.B.current(s);if(q.kind==='meaning')nativeAction({action:'w-choose',n:s.choices[q.id].answer});else nativeType(q.kind==='gap'?s.gaps[q.id].answer:t.B.get(q.id).word);}");if(page(m).keyboard)req(m.key("submit"),m.error());else click(m,"w-check");click(m,"w-next");}
static void finish(LearningWeb&m){for(int i=0;i<128;++i){std::string r;req(m.eval("!!t.m.state.batch",&r),m.error());if(r!="true")return;answer(m);}throw std::runtime_error("batch loop");}
int main(int argc,char**argv){try{req(argc==4,"work,font,frames arguments");const fs::path work=argv[1],frames=argv[3];fs::create_directories(work/"flow");std::ifstream f(argv[2],std::ios::binary);fontBytes.assign(std::istreambuf_iterator<char>(f),{});req(font.init(fontBytes.data(),fontBytes.size()),"EFNT font valid");
 {LearningWeb m;bool ok=m.init((work/"flow").string());req(ok,m.error());run(m,setup);frame(m,frames,"01-purpose");req(page(m).route=="path-goal","goal opens first");run(m,plan);
 run(m,"t.m.commit(x=>{x.batchMemory=Object.fromEntries(t.ids.map(id=>[id,{meaning:{stage:0,due:Date.now()-1,last:0},spelling:{stage:0,due:Date.now()-1,last:0}}]));x.wordMode='meaning';});t.render();");frame(m,frames,"02-home-due");yes(m,"t.D.snapshot(t.m,t.p).next.kind==='meaning'");click(m,"w-day-next");frame(m,frames,"03-review-overview");click(m,"w-begin");frame(m,frames,"04-meaning");finish(m);
 yes(m,"t.D.snapshot(t.m,t.p).next.kind==='spelling'");click(m,"w-day-next");click(m,"w-begin");frame(m,frames,"05-spelling");req(page(m).keyboard,"native keyboard present");req(m.key("key","c"),m.error());req(page(m).typed=="c","key enters exact character");req(m.key("backspace"),m.error());req(page(m).typed.empty(),"backspace works");req(m.type("cou"),m.error());click(m,"lookup");req(m.key("key","f"),m.error());click(m,"lookup-return");req(page(m).typed=="cou","lookup restores exact typed input");frame(m,frames,"06-restored-input");finish(m);
 yes(m,"t.D.snapshot(t.m,t.p).next.kind==='words'");click(m,"w-day-next");run(m,"nativeAction({action:'w-mode',value:'staged'})");frame(m,frames,"07-new-group");click(m,"w-begin");frame(m,frames,"08-learn-group");for(int i=0;i<4;++i)run(m,"nativeAction({action:'w-learn',delta:1})");yes(m,"t.m.state.batch.phase==='check'");for(int i=0;i<4;++i)answer(m);yes(m,"t.B.current(t.m.state.batch).kind==='gap'");frame(m,frames,"09-two-letter");for(int i=0;i<4;++i)answer(m);yes(m,"t.B.current(t.m.state.batch).kind==='spelling'");frame(m,frames,"10-full-spelling");finish(m);
 yes(m,"t.D.snapshot(t.m,t.p).next.kind==='listen'");click(m,"w-day-next");frame(m,frames,"11-listen-unheard");run(m,"nativeAction({action:'check'})");yes(m,"!t.m.state.session.checked");run(m,"t.m.error='';t.render()");
 for(int i=0;i<2;++i){click(m,"audio");std::string p;int serial=0;bool stop=false;req(m.take_audio(p,serial,stop),"audio request");req(!stop&&p.rfind("audio/",0)==0,"mapped audio path");req(!m.audio_text(p).empty(),"fallback text exists");req(m.playback(serial+1,true),"stale callback");yes(m,"!t.m.state.session.heard[t.m.state.session.at]");req(m.playback(serial,false),"failed callback");yes(m,"!t.m.state.session.heard[t.m.state.session.at]");click(m,"audio");req(m.take_audio(p,serial,stop),"replay");req(m.playback(serial,true),"full playback");run(m,"nativeAction({action:'choose',n:t.M.task(t.m.state.session).answer})");click(m,"check");click(m,"next");}
 frame(m,frames,"12-after-listening");click(m,"w-day-next");frame(m,frames,"13-reading");for(int i=0;i<2;++i){run(m,"nativeAction({action:'choose',n:t.M.task(t.m.state.session).answer})");click(m,"check");click(m,"next");}req(m.home(),m.error());frame(m,frames,"14-daily-complete");yes(m,"t.D.snapshot(t.m,t.p).remaining===0");yes(m,"Object.keys(t.m.state.completed).length===0");yes(m,"t.D.snapshot(t.m,t.p).stages.every(s=>s.done===s.total)");
 std::string before,after;req(m.eval("t.m.export()",&before),"export");for(int i=0;i<4;++i)page(m);req(m.eval("t.m.export()",&after),"export2");req(before==after,"render preserves export exactly");run(m,"nativeAction({action:'book',id:'empower-a2'});nativeAction({action:'go',route:'units'});nativeAction({action:'unit',number:2});");frame(m,frames,"15-unit-two");run(m,"nativeAction({action:'lesson',id:'u2a'})");frame(m,frames,"16-lesson-detail");frame(m,frames,"17-boot",Screen::Booting);frame(m,frames,"18-tools",Screen::WebTools);std::cout<<"QuickJS memory="<<m.memory_bytes()<<"\n";req(m.memory_bytes()<3*1024*1024,"memory bound");}
 {LearningWeb m;bool ok=m.init((work/"flow").string());req(ok,m.error());run(m,setup);yes(m,"t.D.snapshot(t.m,t.p).remaining===0");yes(m,"Object.keys(t.m.state.batchMemory).length>=8");}
 {fs::create_directories(work/"corrupt");std::ofstream(work/"corrupt/course-v2.json")<<"{broken";LearningWeb m;bool ok=m.init((work/"corrupt").string());req(ok,m.error());req(page(m).corrupt,"corrupt file flagged");bool found=false;req(m.read("wordcard-adult-web-v2",found)=="{broken","corrupt file retained");}
 {fs::create_directories(work/"backup");fs::copy_file(work/"flow/course-v2.json",work/"backup/course-v2.json.bak",fs::copy_options::overwrite_existing);LearningWeb m;bool ok=m.init((work/"backup").string());req(ok,m.error());run(m,setup);yes(m,"Object.keys(t.m.state.batchMemory).length>=8");req(!fs::exists(work/"backup/course-v2.json"),"backup read stays pure");}
 {fs::create_directories(work/"import");fs::copy_file(work/"flow/course-v2.json",work/"import/import-course.json",fs::copy_options::overwrite_existing);std::ofstream(work/"import/import-profile.json")<<"{invalid";LearningWeb m;bool ok=m.init((work/"import").string());req(ok,m.error());req(!m.import_progress(),"invalid pair rejected");req(!fs::exists(work/"import/course-v2.json"),"bad import cannot replace course");fs::copy_file(work/"flow/profile-v2.json",work/"import/import-profile.json",fs::copy_options::overwrite_existing);ok=m.import_progress();req(ok,m.error());run(m,setup);yes(m,"t.D.snapshot(t.m,t.p).remaining===0");}
 {LearningWeb m;bool ok=m.init((work/"missing-directory").string());req(ok,m.error());run(m,setup);yes(m,"t.p.goal('listen')===false");yes(m,"t.p.state.goal===null");req(!fs::exists(work/"missing-directory/profile-v2.json"),"failed storage cannot advance state");}

#include "web_firmware_review_test.inc"
#include "web_firmware_cache_test.inc"
#include "web_device_layout_test.inc"
#include "web_home_test.inc"
#include "web_navigation_test.inc"
#include "web_input_test.inc"
#include "web_dom_projection_test.inc"
#include "web_preload_test.inc"
#include "web_early_preload_test.inc"
#include "web_direct_quiz_test.inc"
 std::cout<<"PASS "<<checks<<" native checks: actual shared UI and runtime; ordered daily workflow, staged spelling, keyboard, lookup return, audio gates, pure reads, reload/corrupt/backup preservation, validated explicit imports and storage-failure rollback, native bounds/captures. Host callbacks and learner data are fixtures, not device evidence.\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<"\n";return 1;}}
