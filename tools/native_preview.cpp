// Host-only interactive preview. Same Canvas/EFNT/QuickJS as firmware, with
// separate demo files. No serial, hardware driver, TTS or learner profile access.
#include "learning_web.hpp"
#include "wordcard_ui.hpp"
#include "web_keyboard.hpp"
#include "web_disclosure.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>
using namespace wordcard;namespace fs=std::filesystem;
static std::string json(const std::string&s){std::string r="\"";for(unsigned char c:s){if(c=='\\'||c=='\"'){r+='\\';r+=char(c);}else if(c=='\n')r+="\\n";else if(c=='\r')r+="\\r";else if(c=='\t')r+="\\t";else if(c>=32)r+=char(c);}return r+'\"';}
int main(int argc,char**argv){try{
 if(argc!=4)throw std::runtime_error("work, font, frame arguments required");const fs::path work=argv[1],frame=argv[3];fs::create_directories(work);
 std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(f)),{}),fb(Canvas::bytes);Font font;if(!font.init(bytes.data(),bytes.size()))throw std::runtime_error("font");
 const bool first=!fs::exists(work/"profile-v2.json");LearningWeb web;if(!web.init(work.string()))throw std::runtime_error(web.error());
 if(first&&!web.eval("{const {model:m,pathModel:p,M}=nativeDebug;p.goal('all');p.preference('minutes',10);p.preference('days',7);p.preference('context','daily');p.confirmPlan();m.batchBegin(M.wordBatch.catalog.slice(0,4).map(w=>w.id),{type:'course',id:'u1a'});m.batchSetMode('staged');m.batchStart();m.batchLearn();m.batchPause();if(m.state.batch?.phase!=='learn'||m.state.batch?.learnAt!==1)throw Error('Demo setup failed');}"))throw std::runtime_error(web.error());
 web.home();View v;v.screen=Screen::WebLearning;UI ui;int returnPage=0;std::map<std::string,int>positions;std::string oldKey,hostNote;
 auto paint=[&](){
  if(!web.page(v.web))throw std::runtime_error(web.error());if(oldKey!=v.web.key){positions[oldKey]=v.web_page;v.web_page=positions[v.web.key];oldKey=v.web.key;}
  Canvas c(fb.data(),font);ui.render(c,v);std::ofstream p(frame,std::ios::binary);p<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x){const char b=char(c.shade(x,y)*17);p.write(&b,1);}p.close();
  web.finish_input();
  std::string audio;int serial=0;bool stop=false;double rate=1;const bool pending=web.take_audio(audio,serial,stop,&rate);
  std::cout<<"{\"route\":"<<json(v.web.route)<<",\"title\":"<<json(v.web.title)<<",\"note\":"<<json(hostNote)<<",\"keyboard\":"<<(v.web.keyboard?"true":"false")<<",\"buttons\":[";
  bool comma=false;for(const auto&b:ui.buttons()){if(comma)std::cout<<',';comma=true;std::cout<<"{\"x\":"<<b.rect.x<<",\"y\":"<<b.rect.y<<",\"w\":"<<b.rect.w<<",\"h\":"<<b.rect.h<<",\"label\":"<<json(b.label)<<",\"enabled\":"<<(b.enabled?"true":"false")<<"}";}
  std::cout<<"],\"audio\":";if(pending)std::cout<<"{\"path\":"<<json(audio)<<",\"serial\":"<<serial<<",\"stop\":"<<(stop?"true":"false")<<",\"rate\":"<<rate<<"}";else std::cout<<"null";std::cout<<"}"<<std::endl;
 };
 auto dispatch=[&](Event e){
  if(e.action==Action::None)return;
  if(e.action==Action::WebLearning){v.screen=Screen::WebLearning;return;}
  if(e.action==Action::DeviceMenu){v.screen=Screen::WebTools;return;}
  if(e.action==Action::WebImport){hostNote="设备导入不在此预览执行；演示记录与实际进度隔离。";return;}
  if(v.screen==Screen::WebPrompt&&e.action==Action::Back){v.screen=Screen::WebLearning;v.web_page=returnPage;return;}
  if(e.action==Action::WebPage){if(e.value==0){returnPage=v.web_page;v.screen=Screen::WebPrompt;v.web_page=0;}else v.web_page=std::clamp(v.web_page+e.value,0,ui.answer_pages()-1);return;}
  if(v.screen!=Screen::WebLearning&&v.screen!=Screen::WebPrompt){hostNote="此项需要真实设备。预览仅运行新版学习导航，可点返回继续试用。";return;}
  bool ok=true;
  switch(e.action){
   case Action::WebAction:if(v.screen!=Screen::WebPrompt||web_prompt_action_allowed(v.web,e.value))ok=web.action(e.value);break;
   case Action::Back:ok=web.back();break;
   case Action::DictionaryKey:ok=web.key("key",std::string(1,char(e.value)));break;
   case Action::DictionaryErase:ok=web.key("backspace");break;
   case Action::DictionaryShift:ok=web.key("shift");break;
   case Action::DictionarySymbols:ok=web.key("symbols");break;
   case Action::DictionarySearch:ok=web.key("submit");break;
   case Action::RecallSkip:ok=web.command("w-unknown");break;
   case Action::Vocabulary:if(v.web.keyboard)ok=web.command(v.web.route=="word-batch"?"lookup":"u-notebook");else hostNote="这是设备的完整离线词典生词本；主机可试用课内生词本。";break;
   case Action::Dictionary:hostNote="完整离线词典在设备 SD 卡上；主机可从课内词语与表达试用查询流程。";break;
   case Action::DictionaryRecent:{const auto rows=web_keyboard_controls(v.web);if(e.value>=0&&e.value<int(rows.size())){if(e.value==1&&web_long_prompt(v.web)){returnPage=v.web_page;v.screen=Screen::WebPrompt;v.web_page=0;}else if(rows[e.value]->enabled)ok=web.action(rows[e.value]->action);}break;}
   default:hostNote="这个设备功能尚未接入主机预览。";
  }if(!ok)hostNote=web.error();
 };
 paint();std::string line;while(std::getline(std::cin,line)){
  hostNote.clear();std::istringstream in(line);std::string command;in>>command;
  if(command=="tap"){int x,y;if(in>>x>>y)dispatch(ui.hit(x,y));}
  else if(command=="home"){web.home();v.screen=Screen::WebLearning;}
  else if(command=="key"&&v.web.keyboard){int key=0;in>>key;if(key>=32&&key<=126)dispatch({Action::DictionaryKey,key});else if(key==8)dispatch({Action::DictionaryErase,0});else if(key==13)dispatch({Action::DictionarySearch,0});}
  else if(command=="audio"){int serial=0,ok=0;if(in>>serial>>ok)web.playback(serial,ok!=0);}
  paint();
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
