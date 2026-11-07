#include "vocabulary.hpp"
#include "wordcard_ui.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
using namespace wordcard; namespace fs=std::filesystem;
static int checks=0, partial=0, truncateFailures=0;
namespace wordcard { bool vocabulary_storage_fault(const char *op) {
    if(std::string(op)=="partial-append" && partial>0){--partial;return true;}
    if(std::string(op)=="truncate" && truncateFailures>0){--truncateFailures;return true;}
    return false;
} }
void ck(bool b,const std::string&s){++checks;if(!b)throw std::runtime_error(s);}
std::string read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const fs::path&p,const std::string&s){std::ofstream f(p,std::ios::binary);f<<s;}
VJ state(const fs::path&p){auto frame=VJ::parse(str(VJ::parse(read(p/"vocabulary/state.json")),"data"));auto s=frame["state"];std::ifstream in(p/"vocabulary/events.ndjson",std::ios::binary);if(in){in.seekg(num(frame,"journalOffset"));std::string line;while(std::getline(in,line))s=s.patch(VJ::parse(str(VJ::parse(line),"data"))["patch"]);}return s;}
bool text(const WebPage&p,const std::string&s){for(auto b:p.blocks)if(b.text.find(s)!=std::string::npos)return true;return false;}
int main(int argc,char**argv){try{
 ck(argc==4,"catalog, font, fresh output required");const int64_t now=1793880000000LL;fs::path out=argv[3],profile=out/"learning-web";
 ck(!fs::exists(out),"fresh isolated fixture only");fs::create_directories(out/"vocabulary");fs::create_directories(profile);
 for(auto n:{"catalog.json","catalog.tsv","catalog.ndjson"})fs::copy_file(fs::path(argv[1])/n,out/"vocabulary"/n);
 auto model=std::make_unique<NativeVocabulary>(profile.generic_string());ck(model->enter(VJ(),now),"enter");
 auto status=[&]{return VJ::parse(model->status());};
 auto dispatch=[&](VJ a){ck(model->dispatch(a.dump()),"dispatch handled");};
 auto action=[&](const char* name){dispatch(VJ{{"action",name}});};
 auto go=[&](const char*route){dispatch(VJ{{"action","v-go"},{"value",route}});};
 auto click=[&](const std::string&label){auto p=model->page();for(auto b:p.blocks)if(b.kind=="button"&&b.enabled&&b.text==label){dispatch(VJ::parse(p.actions[b.action]));return;}throw std::runtime_error("Missing "+label);};
 Font font;auto bytes=read(argv[2]);ck(font.init((const uint8_t*)bytes.data(),bytes.size()),"font");
 auto capture=[&](const char*name){std::vector<uint8_t> pixels(Canvas::bytes);Canvas c(pixels.data(),font);View v;v.screen=Screen::WebLearning;v.web=model->page();UI ui;ui.render(c,v);
  int exits=0;for(auto b:ui.buttons()){ck(b.rect.y>=0&&b.rect.y+b.rect.h<=1216,"controls in screen");if((b.label.find("返回")!=std::string::npos||b.label=="回首页")&&b.enabled)++exits;}
  ck(exits>0,"visible enabled exit");std::ofstream f(out/(std::string(name)+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x)f.put(char(c.shade(x,y)*17));};
 auto original=state(profile);go("word-list");click("读取单词表");
 ck(model->page().route=="word-list"&&text(model->page(),"没有找到 my-words.txt")&&str(status(),"courseError").empty(),"missing file stays on actionable import page");
 capture("01-missing-file");ck(model->back(),"missing file back");ck(model->page().route=="home","missing file exits");
 go("word-list");write(out/"my-words.txt","");click("读取单词表");ck(text(model->page(),"单词表为空"),"empty file explains correction");
 write(out/"my-words.txt",std::string(33000,'a'));click("读取单词表");ck(text(model->page(),"32 KB"),"oversize explains limit");
 ck(state(profile)==original,"invalid imports never change progress");
 write(out/"my-words.txt","apple\nbook\nwater\nhome\nwork\n");click("读取单词表");click("加入我的单词本");click("学习这些生词");
 ck(str(status()["batch"],"source")=="notebook","custom learning source");
 auto before=state(profile);auto batch=status()["batch"];auto journal=read(profile/"vocabulary/events.ndjson");
 VJ next;{auto p=model->page();for(auto b:p.blocks)if(b.primary&&b.enabled)next=VJ::parse(p.actions[b.action]);}ck(next.is_object(),"visible learn next");
 model->tick(0);dispatch(next);ck(model->page().route=="recovery"&&model->page().title=="请校准时间","invalid clock explains cause");
 capture("02-clock");for(int i=0;i<3;++i){click("已校准，检查时间");ck(model->page().route=="recovery"&&state(profile)==before,"rechecking invalid clock does not fabricate progress");}
 ck(model->back(),"clock back works");ck(model->page().route=="word-batch"&&status()["batch"]==batch,"clock returns exact position");
 model->tick(now);dispatch(next);ck(str(status(),"courseError").empty(),"correct clock allows explicit next");
 before=state(profile);batch=status()["batch"];{auto p=model->page();for(auto b:p.blocks)if(b.primary&&b.enabled)next=VJ::parse(p.actions[b.action]);}
 model->tick(now-600000);dispatch(next);model->tick(now);action("v-close-error");ck(state(profile)==before&&status()["batch"]==batch,"clock check does not replay failed next");
 fs::rename(profile/"vocabulary",profile/"held");dispatch(next);ck(model->page().title=="这一步未能保存","missing media reports storage");capture("03-storage");
 for(int i=0;i<3;++i){click("检查存储");ck(model->page().route=="recovery"&&status()["batch"]==batch,"persistent media failure is bounded and keeps original question");}
 click("返回首页");ck(model->page().route=="home"&&status()["batch"]==batch,"storage error has non-writing home exit");
 fs::rename(profile/"held",profile/"vocabulary");action("v-check-recovery");ck(str(status(),"courseError").empty()&&state(profile)==before,"same card recovery does not advance learning");
 dispatch(VJ{{"action","v-do"},{"type","learnNotebook"}});ck(status()["batch"]==batch,"resume original custom group");before=state(profile);journal=read(profile/"vocabulary/events.ndjson");
 {auto p=model->page();for(auto b:p.blocks)if(b.primary&&b.enabled)next=VJ::parse(p.actions[b.action]);}
 partial=1;truncateFailures=1;dispatch(next);ck(read(profile/"vocabulary/events.ndjson").size()>journal.size(),"injected write left partial tail");
 ck(model->page().title=="这一步未能保存","partial write visible");model->back();dispatch(next);ck(status()["batch"]==batch,"pending repair blocks further commits");
 action("v-check-recovery");ck(str(status(),"courseError").empty()&&read(profile/"vocabulary/events.ndjson")==journal&&state(profile)==before,"repair restores exact committed prefix without replay");
 dispatch(next);ck(num(state(profile),"revision")==num(before,"revision")+1,"explicit retry advances exactly once after repair");
 // A different/older card cannot receive the pending in-memory state.
 before=state(profile);journal=read(profile/"vocabulary/events.ndjson");{auto p=model->page();for(auto b:p.blocks)if(b.primary&&b.enabled)next=VJ::parse(p.actions[b.action]);}fs::rename(profile/"vocabulary",profile/"held");dispatch(next);
 fs::create_directories(profile/"vocabulary");write(profile/"vocabulary/state.json",read(profile/"held/state.json"));action("v-check-recovery");
 ck(!str(status(),"courseError").empty()&&!fs::exists(profile/"vocabulary/events.ndjson"),"replacement/older card is not overwritten");
 fs::rename(profile/"vocabulary",profile/"other-card");fs::rename(profile/"held",profile/"vocabulary");action("v-check-recovery");ck(state(profile)==before,"original card recovers unchanged");
 // Corrupt checkpoint: escape to recovery home and keep exact bytes; never reset.
 model.reset();auto checkpoint=read(profile/"vocabulary/state.json");write(profile/"vocabulary/state.json","broken-checkpoint");
 model=std::make_unique<NativeVocabulary>(profile.generic_string());model->enter(VJ(),now);capture("04-corrupt-record");model->back();ck(model->page().title=="学习暂不可用","unloaded profile has recovery home");
 for(int i=0;i<2;++i)action("v-check-recovery");ck(read(profile/"vocabulary/state.json")=="broken-checkpoint"&&read(profile/"vocabulary/events.ndjson")==journal,"bad records are never erased on retry");
 write(profile/"vocabulary/state.json",checkpoint);action("v-check-recovery");ck(str(status(),"courseError").empty()&&state(profile)==before,"repaired checkpoint reloads old progress");
 // First use with no RTC: date setting and settings remain accessible; no invented session.
 auto fresh=out/"learning-web-new";fs::create_directories(fresh);model=std::make_unique<NativeVocabulary>(fresh.generic_string());model->enter(VJ(),0);capture("05-first-use-clock");
 ck(!fs::exists(fresh/"vocabulary/state.json"),"invalid first-use date does not write invented progress");model->back();model->tick(now);action("v-check-recovery");ck(str(status(),"courseError").empty()&&state(fresh)["seen"].empty(),"first-use calibration creates only empty genuine progress");
 std::ofstream(out/"checks.json")<<VJ{{"passed",true},{"checks",checks},{"synthetic",true},{"physicalFaultsInjected",false},{"partialWriteAndRollbackFailureInjected",true}}.dump(2);
 std::cout<<"PASS recovery "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
