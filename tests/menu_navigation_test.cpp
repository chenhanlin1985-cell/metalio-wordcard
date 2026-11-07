// Actual menu dispatcher + real native/legacy engines + production Canvas.
#include "wordcard_ui.hpp"
#include "native_json.hpp"
#include "web_frame_cache.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace wordcard;namespace fs=std::filesystem;using J=StudyJson;
static int checks=0;
void require(bool b,const std::string&s){++checks;if(!b)throw std::runtime_error(s);}
std::string read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
class App{
 public:
 View view_;LearningWeb web_;std::vector<Screen> menu_stack_;bool menu_training_=false;
 Screen previous_=Screen::Home;std::string notice_;int stops=0;
 void menu_push(Screen);bool menu_return();bool menu_dispatch(Event);void menu_finish(Event,Screen);
 void leave_player(){++stops;}
 void page(){if(view_.screen==Screen::WebLearning)require(web_.page(view_.web),web_.error());}
 void press(Action a,int n=0){const auto before=view_.screen;Event e{a,n};
  if(!menu_dispatch(e))switch(a){
   case Action::Back:require(web_.back(),web_.error());break;
   case Action::WebAction:require(web_.action(n),web_.error());break;
   case Action::GuidedCourses:view_.screen=Screen::GuidedCourses;break;
   case Action::Library:view_.screen=Screen::Library;break;
   case Action::SaveDate:view_.screen=Screen::Home;break; // RTC success boundary
   case Action::UsbDisk:view_.screen=Screen::UsbDisk;break;
   case Action::UsbExit:view_.screen=Screen::Resources;break;
   default:break;
  }
  menu_finish(e,before);page();
 }
};
#include "app_menu.inc"
int main(int argc,char**argv){try{
 require(argc==4,"resources/font/fresh output");fs::path out=argv[3];require(!fs::exists(out),"fresh output");
 fs::create_directories(out/"vocabulary");fs::create_directories(out/"learning-web");fs::create_directories(out/"frames");
 for(auto name:{"catalog.json","catalog.ndjson","catalog.tsv"})fs::copy_file(fs::path(argv[1])/name,out/"vocabulary"/name);
 App a;require(a.web_.init((out/"learning-web").generic_string()),a.web_.error());a.view_.screen=Screen::WebLearning;a.page();
 const auto course=read(out/"learning-web/course-v2.json"),profile=read(out/"learning-web/profile-v2.json");
 auto fontData=read(argv[2]);Font font;require(font.init((const uint8_t*)fontData.data(),fontData.size()),"font");
 UI ui;std::vector<uint8_t> pixels(Canvas::bytes);J captures=J::array();
 auto frame=[&](const std::string&name){Canvas c(pixels.data(),font);ui.render(c,a.view_);J buttons=J::array();
  for(const auto&b:ui.buttons()){require(b.rect.x>=0&&b.rect.y>=0&&b.rect.x+b.rect.w<=684&&b.rect.y+b.rect.h<=1216,"button in frame: "+b.label);buttons.push_back({{"text",b.label},{"action",int(b.event.action)},{"x",b.rect.x},{"y",b.rect.y},{"w",b.rect.w},{"h",b.rect.h}});}
  std::ofstream p(out/"frames"/(name+".pgm"),std::ios::binary);p<<"P5\n684 1216\n255\n";for(int y=0;y<1216;y++)for(int x=0;x<684;x++){p.put(char(c.shade(x,y)*17));}
  captures.push_back({{"name",name},{"screen",int(a.view_.screen)},{"buttons",buttons}});
 };
 frame("01-home");auto settings=std::find_if(ui.buttons().begin(),ui.buttons().end(),[](auto&b){return b.label=="设置";});require(settings!=ui.buttons().end()&&settings->event.action==Action::DeviceMenu,"single settings entry uses menu");
 // A real search draft and pagination survive overlay navigation, without engine calls.
 require(a.web_.command("v-go","search")&&a.web_.type("appointment"),"prepare main query");a.page();a.view_.web_page=2;auto key=a.view_.web.key;
 auto scripts=a.web_.script_calls();
 a.press(Action::DeviceMenu);frame("02-settings");a.press(Action::DeviceSettings);frame("03-device");
 for(auto leaf:{Action::Settings,Action::Date,Action::Resources}){
  a.press(leaf);a.press(Action::Back);require(a.view_.screen==Screen::DeviceSettings,"device child returns device, never old profile");
 }
 a.press(Action::Date);a.press(Action::SaveDate);require(a.view_.screen==Screen::DeviceSettings,"saved RTC returns exact parent");
 a.press(Action::Settings);a.press(Action::SelfTest);a.press(Action::Settings);require(a.menu_stack_.size()==3,"diagnostic cycle collapses history");a.press(Action::TtsTest);a.press(Action::Back);require(a.view_.screen==Screen::Settings,"TTS returns sound");a.press(Action::Back);
 a.press(Action::Resources);a.press(Action::UsbDisk);a.press(Action::UsbExit);require(a.view_.screen==Screen::Resources,"USB exit keeps storage page");a.press(Action::Back);a.press(Action::Back);require(a.view_.screen==Screen::WebTools,"device returns unified settings");
 a.press(Action::LearningSettings);frame("04-training");require(a.view_.web.route=="settings","training only");
 for(auto&b:a.view_.web.blocks)require(b.text.find("原课程")==std::string::npos,"training contains no old-course exit");
 a.press(Action::Back);require(a.view_.screen==Screen::WebTools,"training returns settings");
 a.press(Action::Back);require(a.view_.screen==Screen::WebLearning&&a.view_.web.typed=="appointment"&&a.view_.web_page==2,"query + pagination restored");require(a.web_.script_calls()==scripts,"device settings do not run legacy engine");
 // Secondary courses retain their own home; returning resumes the main view exactly.
 a.press(Action::DeviceMenu);a.press(Action::MoreLearning);frame("05-more");
 a.press(Action::WebImport);a.press(Action::Back);require(a.view_.screen==Screen::MoreLearning,"import cancellation returns more");
 a.press(Action::LegacyCourses);frame("06-courses");require(a.web_.courses_open()&&a.view_.web.route=="books","explicit secondary entry");
 require(a.web_.command("w-day-home"),"secondary course home");a.page();require(a.view_.web.key.rfind("vocabulary:",0)!=0,"secondary home cannot silently switch engine");frame("07-course-progress");
 a.press(Action::MoreLearning);require(!a.web_.courses_open()&&a.view_.screen==Screen::MoreLearning,"course exit");
 a.press(Action::Back);a.press(Action::Back);require(a.view_.web.key==key&&a.view_.web.typed=="appointment"&&a.view_.web_page==2,"courses preserve main search identity and draft");
 for(auto entry:{Action::GuidedCourses,Action::Library}){a.press(Action::DeviceMenu);a.press(Action::MoreLearning);a.press(entry);a.press(Action::Back);require(a.view_.screen==Screen::MoreLearning,"old course root returns more");a.press(Action::Back);a.press(Action::Back);}
 // Stress repeated cycles and nested device origin in the old section.
 for(int i=0;i<12;i++){a.press(Action::DeviceMenu);a.press(Action::MoreLearning);a.press(Action::LegacyCourses);a.press(Action::Profile);a.press(Action::Date);a.press(Action::SaveDate);a.press(Action::Back);require(a.view_.screen==Screen::WebLearning&&a.web_.courses_open(),"old course device origin");a.press(Action::Back);a.press(Action::Back);a.press(Action::Back);require(a.menu_stack_.empty()&&a.view_.web.typed=="appointment","bounded repeat history");}
 require(read(out/"learning-web/course-v2.json")==course&&read(out/"learning-web/profile-v2.json")==profile,"browsing preserves original legacy files");
 a.press(Action::DeviceMenu);a.press(Action::MoreLearning);a.press(Action::LegacyCourses);a.press(Action::Home);require(a.view_.web.key.rfind("vocabulary:",0)==0&&a.view_.web.route=="home"&&a.menu_stack_.empty(),"explicit home always main vocabulary");
 require(a.web_.command("v-use-stage","foundation")&&a.web_.command("v-mode","full")&&a.web_.command("v-count","5")&&a.web_.command("v-start"),"start a real main group");a.page();
 // If the first check is meaning, continue until spelling is presented.
 for(int i=0;i<80&&!a.view_.web.keyboard;i++){
  int next=-1;for(const auto&b:a.view_.web.blocks)if(b.kind=="button"&&b.primary&&b.enabled&&b.area=="foot")next=b.action;
  if(next>=0)require(a.web_.action(next),"continue task");else require(a.web_.command("w-unknown"),"advance meaning to spelling");a.page();
 }
 require(a.view_.web.keyboard&&a.web_.type("dra"),"prepare actual spelling draft");a.page();
 std::string beforeTask,afterTask;require(a.web_.status(beforeTask),"task status");key=a.view_.web.key;
 frame("08-spelling-before");a.press(Action::DeviceMenu);a.press(Action::MoreLearning);a.press(Action::LegacyCourses);a.press(Action::MoreLearning);a.press(Action::Back);a.press(Action::Back);frame("09-spelling-restored");
 require(a.web_.status(afterTask)&&beforeTask==afterTask&&a.view_.web.key==key&&a.view_.web.typed=="dra","real spelling session, revision, task, draft unchanged");
 a.press(Action::DeviceMenu);a.press(Action::MoreLearning);a.press(Action::LegacyCourses);
 require(a.web_.eval("nativeDebug.model.commit(s=>{s.navigationSentinel={answers:37,word:'previous',futureField:'retain'};});nativeDebug.go('home');"),"write existing-course progress");a.page();
 require(a.view_.web.secondary&&a.view_.web.key.rfind("vocabulary:",0)!=0,"course progress stays in secondary context");
 a.press(Action::MoreLearning);a.press(Action::Back);a.press(Action::Back);
 require(J::parse(read(out/"learning-web/course-v2.json"))["navigationSentinel"]["answers"]==37,"legacy record survives return");
 a.press(Action::DeviceMenu);a.press(Action::MoreLearning);a.press(Action::LegacyCourses);a.press(Action::MoreLearning);a.press(Action::Back);a.press(Action::Back);
 require(J::parse(read(out/"learning-web/course-v2.json"))["navigationSentinel"]["futureField"]=="retain","legacy progress never overwritten by main context");
 require(a.web_.status(afterTask)&&afterTask==beforeTask,"main task unchanged by old-course progress");
 WebFrameCache cache;cache.configure(font);auto cached=a.view_.web;cached.secondary=true;require(cache.build(cached,[]{return false;}),"prepare secondary frame");
 View target=a.view_;target.web_page=0;require(!cache.take(target,pixels.data(),ui),"never reuse secondary frame for main scope");target.web=cached;require(cache.take(target,pixels.data(),ui),"matching secondary pre-render remains usable");
 std::ofstream(out/"captures.json")<<captures.dump(2);std::ofstream(out/"checks.json")<<J{{"passed",true},{"checks",checks},{"deviceTouched",false}}.dump(2);
 std::cout<<"PASS navigation "<<checks<<" checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
