#include "vocabulary.hpp"
#include "wordcard_ui.hpp"
#include "dictionary.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
using namespace wordcard;namespace fs=std::filesystem;
int checks=0;constexpr int64_t Now=1793836800000LL;
void ck(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
std::string read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
VJ state(const fs::path&p){auto f=VJ::parse(str(VJ::parse(read(p/"vocabulary/state.json")),"data"));auto s=f["state"];std::ifstream in(p/"vocabulary/events.ndjson",std::ios::binary);if(in){in.seekg(num(f,"journalOffset"));std::string l;while(std::getline(in,l))s=s.patch(VJ::parse(str(VJ::parse(l),"data"))["patch"]);}return s;}
void little(std::ostream &s,uint32_t n){for(int i=0;i<4;++i)s.put(char(n>>(8*i)));}
void dictionary(const fs::path&p){fs::create_directories(p);std::string row="quizzacious\tquizzacious\tkwɪˈzeɪʃəs\t测试词义；第二义\tSynthetic fixture only\t";std::ofstream idx(p/"index.bin",std::ios::binary),dat(p/"entries.bin",std::ios::binary);idx.write("WCDICT1\0",8);little(idx,1);little(idx,uint32_t(row.size()));little(idx,0);little(idx,uint32_t(row.size()));dat<<row;}
int main(int argc,char**argv){try{
 ck(argc==4||argc==5,"resources/font/fresh output/optional real dictionary");fs::path out=argv[3],profile=out/"learning-web";
 ck(!fs::exists(out),"never overwrite a user profile");fs::create_directories(out/"vocabulary");fs::create_directories(profile);
 for(auto n:{"catalog.json","catalog.tsv","catalog.ndjson"})fs::copy_file(fs::path(argv[1])/n,out/"vocabulary"/n);
 dictionary(out/"dictionary");
 auto model=std::make_unique<NativeVocabulary>(profile.generic_string());ck(model->enter(VJ(),Now),"enter native");
 auto run=[&](VJ a){ck(model->dispatch(a.dump()),"dispatch handled");ck(str(VJ::parse(model->status()),"courseError").empty(),"action succeeded");};
 auto click=[&](const std::string &label){auto p=model->page();for(auto b:p.blocks)if(b.kind=="button"&&b.text==label&&b.enabled){run(VJ::parse(p.actions.at(b.action)));return;}throw std::runtime_error("Missing button: "+label);};
 auto write=[&](std::string s){std::ofstream f(out/"my-words.txt",std::ios::binary);f<<s;};
 auto original=state(profile);
 run(VJ{{"action","v-go"},{"value","levels"}});click("我的单词本");click("添加单词");click("从单词表添加");
 // A preview is not a partial import. Good and bad lines may coexist.
 write("\xef\xbb\xbf" "apple\r\nAPI\r\nC++\r\nquizzacious\r\nquizzacious\r\nnonexistentzyx\r\n中文不应导入\r\nIT\r\nit\r\n");
 click("读取单词表");ck(state(profile)==original,"preview never changes progress");
 auto preview=model->page();bool issues=false;for(auto b:preview.blocks)if(b.text.find("待确认 2 行")!=std::string::npos)issues=true;ck(issues,"unresolved and invalid rows reported");
 // Production Canvas captures, not a web approximation or physical photo.
 Font font;auto fontBytes=read(argv[2]);ck(font.init(reinterpret_cast<const uint8_t*>(fontBytes.data()),fontBytes.size()),"font loads");
 auto capture=[&](std::string name){std::vector<uint8_t> pixels(1216*684/2);Canvas canvas(pixels.data(),font);View v;v.screen=Screen::WebLearning;v.web=model->page();UI ui;ui.render(canvas,v);std::ofstream f(out/(name+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x)f.put(char(canvas.shade(x,y)*17));};
 capture("01-preview");click("查看未匹配的行");capture("05-unmatched");run(VJ{{"action","v-back"}});
 VJ save;for(auto b:preview.blocks)if(b.text=="加入我的单词本")save=VJ::parse(preview.actions[b.action]);ck(save.is_object(),"confirm action available");
 run(save);auto added=state(profile);ck(added["notebook"].size()==6,"six distinct words added (IT != it)");
 for(auto k:{"cards","seen","session","paused","days","settings","selectedStage"})ck(added[k]==original[k],"old learning data unchanged");
 ck(model->page().route=="notebook","confirmation returns to notebook");capture("02-notebook");
 {std::vector<uint8_t> pixels(Canvas::bytes);Canvas c(pixels.data(),font);View v;v.screen=Screen::WebLearning;v.web=model->page();UI ui;ui.render(c,v);bool found=false;for(auto b:ui.buttons())if(b.label=="学习这些生词"&&b.enabled&&b.rect.y>=1000)found=true;ck(found,"notebook can start immediately without paging through its words");}
 click("查看和整理单词");capture("04-manage");run(VJ{{"action","v-back"}});
 run(save);ck(state(profile)==added,"stale double tap cannot append revision");
 std::string external="dict:"+personal_word_id("quizzacious");ck(added["externalWords"].contains(external),"offline dictionary used for external word");
 ck(str(added["externalWords"][external],"ipa")=="词典 /kwɪˈzeɪʃəs/","IPA retained");
 // Reordered source and repeat reading neither resets nor creates duplicate sessions.
 run(VJ{{"action","v-go"},{"value","notebook-add"}});click("从单词表添加");write("quizzacious\nit\nIT\nC++\nAPI\napple\n");click("读取单词表");ck(state(profile)==added,"rereading and reordering is nonmutating");click("返回单词本");ck(model->page().route=="notebook","zero-new bottom action reaches notebook through real add-page stack");
 // Actual production learning: existing choice, gap, spelling and wrong-word scheduling.
 click("学习这些生词");ck(str(state(profile)["session"],"source")=="notebook","same notebook learning engine");
 for(int i=0;i<30&&!model->page().keyboard;++i){auto p=model->page();bool done=false;for(auto b:p.blocks)if(b.kind=="button"&&b.primary&&b.enabled){run(VJ::parse(p.actions[b.action]));done=true;break;}if(!done)run(VJ{{"action","w-unknown"}});}
 ck(model->page().keyboard,"real spelling round reached");ck(model->type("ab"),"spelling draft saved");auto paused=state(profile);capture("03-spelling");
 run(VJ{{"action","u-notebook"}});run(VJ{{"action","v-go"},{"value","word-list"}});write("firmware\nappointment\n");click("读取单词表");click("加入我的单词本");
 ck(state(profile)["session"]==paused["session"],"adding during spelling retains entire session and draft");
 run(VJ{{"action","v-back"}});ck(model->page().route=="word-batch"&&model->page().typed=="ab","returns to same question and draft");
 // Removal and reboot keep FSRS and paused task references intact.
 run(VJ{{"action","u-notebook"}});run(VJ{{"action","v-open"},{"value",external}});click("移出生词本");
 auto removed=state(profile);ck(!has(removed["notebook"],external),"membership removed");ck(removed["cards"]==paused["cards"],"FSRS cards retained on removal");ck(removed["session"]["draft"]==paused["session"]["draft"],"draft retained on removal");
 model.reset();model=std::make_unique<NativeVocabulary>(profile.generic_string());ck(model->enter(VJ(),Now),"reboot reads records");ck(state(profile)==removed,"replay retains custom words and original progress");
 run(VJ{{"action","v-go"},{"value","word-list"}});write("quizzacious\n");click("读取单词表");click("加入我的单词本");ck(state(profile)["cards"]==removed["cards"],"re-add uses original FSRS cards");
 // Save failure must roll back dictionary installation as well as JSON state.
 run(VJ{{"action","v-go"},{"value","word-list"}});write("C#\n");click("读取单词表");const auto preFailure=state(profile);const auto journal=profile/"vocabulary/events.ndjson";fs::rename(journal,journal.string()+".held");fs::create_directory(journal);auto p=model->page();VJ failedSave;for(auto b:p.blocks)if(b.text=="加入我的单词本")failedSave=VJ::parse(p.actions[b.action]);ck(!model->dispatch(failedSave.dump()),"failed journal append reports failure");fs::remove(journal);fs::rename(journal.string()+".held",journal);ck(state(profile)==preFailure,"failed save changed no durable progress");run(VJ{{"action","v-close-error"}});run(failedSave);ck(state(profile)["notebook"].size()==preFailure["notebook"].size()+1,"retry after IO recovery succeeds once");
 auto stable=state(profile);run(VJ{{"action","v-go"},{"value","word-list"}});write(std::string(32769,'a'));ck(!model->prepare_word_list(),"oversize file rejected");ck(state(profile)==stable,"oversize keeps all state");run(VJ{{"action","v-close-error"}});
 write("");ck(!model->prepare_word_list(),"empty file rejected");run(VJ{{"action","v-close-error"}});
 std::string many;for(int i=0;i<201;++i)many+="apple\n";write(many);ck(!model->prepare_word_list(),"line limit rejects entire preview");ck(state(profile)==stable,"limit does not partially save");

 if(argc==5) {
  run(VJ{{"action","v-close-error"}});
  fs::rename(out/"dictionary",out/"synthetic-dictionary");fs::create_directory(out/"dictionary");
  for(auto f:{"index.bin","entries.bin"})fs::copy_file(fs::path(argv[4])/f,out/"dictionary"/f);
  run(VJ{{"action","v-go"},{"value","notebook"}});
  run(VJ{{"action","v-go"},{"value","word-list"}});
  write("appointment\nfirmware\nraspberry\nAPI\nUSB\nC++\nC#\nIT\nit\nlook after\nmeeting\nhealth\n");
  click("读取单词表");bool clean=false;
  for(const auto &b:model->page().blocks)if(b.text.find("待确认 0 行")!=std::string::npos)clean=true;
  ck(clean,"real offline dictionary resolves daily vocabulary, tech and phrase inputs");
  click("加入我的单词本");
  auto real=state(profile);ck(real["cards"]==stable["cards"],"real dictionary import retains existing FSRS");
  capture("06-real-notebook");
  std::ofstream(out/"real-dictionary.json")<<VJ{{"passed",true},{"inputLines",12},{"syntheticProgress",true}}.dump(2);
 }

 run(VJ{{"action","v-go"},{"value","levels"}});capture("07-library-entry");
 click("我的单词本");run(VJ{{"action","v-go"},{"value","notebook-add"}});capture("08-add-options");
 click("从单词表添加");capture("09-file-instructions");
 const auto noMatchState=state(profile);write("nonexistentzyx\n");click("读取单词表");capture("10-no-match");
 click("返回单词本");ck(model->page().route=="notebook","all-unmatched bottom action reaches notebook");
 ck(state(profile)==noMatchState,"zero-match action leaves progress unchanged");
 std::ofstream(out/"checks.json")<<VJ{{"passed",true},{"checks",checks},{"deviceTouched",false}}.dump(2);std::cout<<"PASS custom words "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
