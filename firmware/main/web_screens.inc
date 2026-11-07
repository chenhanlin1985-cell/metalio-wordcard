#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include "web_keyboard.hpp"
#include "web_disclosure.hpp"
#include "web_input_field.hpp"
#include <algorithm>
namespace wordcard {
void UI::web_pages(Canvas&c,const View&v){
 auto w=v.web;
 if(v.screen==Screen::WebPrompt)w=web_disclosure(w);
 const auto status=!v.notice.empty()?v.notice:w.toast;
 const bool vocabulary=w.key.rfind("vocabulary:",0)==0;
 if(v.screen==Screen::WebTools||v.screen==Screen::DeviceSettings||v.screen==Screen::MoreLearning){
  const bool device=v.screen==Screen::DeviceSettings,more=v.screen==Screen::MoreLearning;
  button(c,{24,12,120,96},Action::Back,"返回");
  c.text_px(180,30,device?"设备设置":more?"更多学习":"设置",44);
  c.fill({24,128,636,2});
  auto entry=[&](int y,Action a,const std::string&label){button(c,{24,y,636,104},a,"");buttons_.back().label=label;c.text_px(46,y+28,label,38);};
  if(device){
   entry(200,Action::Settings,"声音与前光");
   entry(334,Action::Date,v.date_valid?"日期与时间":"设置日期与时间");
   entry(468,Action::Haptics,v.haptics?"触控反馈：开启":"触控反馈：关闭");
   entry(602,Action::Resources,"SD 卡与 USB 传输");
   entry(736,Action::StorageCheck,"检查存储与保存");
  }else if(more){
   c.text_px(28,176,"原有课程与记录保留在这里。",36);
   entry(252,Action::LegacyCourses,"教材课程与听读");
   entry(386,Action::GuidedCourses,"情境英语课程");
   entry(520,Action::Library,"原词书速记");
   entry(654,Action::Stats,"词书速记记录");
   entry(788,Action::WebImport,"导入教材课程进度");
  }else{
   entry(220,Action::LearningSettings,"背词设置");
   entry(370,Action::DeviceSettings,"设备设置");
   entry(520,Action::MoreLearning,"更多学习");
  }
  if(!v.notice.empty()){int y=930;for(const auto&s:c.wrap_px(v.notice,628,36)){if(y>1040)break;c.text_px(28,y,s,36);y+=46;}}
  button(c,{24,1080,636,100},Action::Back,"返回",0,true);return;
 }
 if(v.screen==Screen::WebImportConfirm){
  c.centered(66,"导入教材进度");
  button(c,{24,12,120,96},Action::Back,"返回");
  {
   int y=224;for(const auto&s:c.wrap_px("在网页分别导出课程进度、摸底与计划，复制到 SD 的 learning-web 目录，命名 import-course.json 和 import-profile.json。",636,36)){c.text_px(24,y,s,36);y+=50;}
   y+=30;for(const auto&s:c.wrap_px("确认后替换本机新版课程或计划；当前文件会保留 .bak。原固件学习文件不变。",636,36)){c.text_px(24,y,s,36);y+=50;}
   button(c,{24,886,636,92},Action::WebImport,"确认导入已放入的文件",1,true);
   button(c,{24,1008,636,92},Action::Back,"取消，返回");
  }if(!v.notice.empty()){int y=v.screen==Screen::WebImportConfirm?730:980;for(const auto&s:c.wrap_px(v.notice,636,36)){if(y>(v.screen==Screen::WebImportConfirm?840:1100))break;c.text_px(24,y,s,36);y+=48;}}return;
 }
 if(v.screen==Screen::Booting){c.centered(270,"小小单词卡",2);c.border({96,472,492,2});c.centered(542,"正在开机");c.centered(614,v.boot_stage.empty()?"正在准备学习资料…":v.boot_stage);c.centered(956,"稍候即可继续学习");return;}
 const bool root_home=w.route=="home"&&v.screen!=Screen::WebPrompt;
 #include "web_home.inc"
 #include "web_sections.inc"
 std::string backText=root_home?"设备":"返回";
 if(vocabulary&&!root_home)for(const auto&b:w.top)if(b.text.rfind("返回",0)==0||b.text=="回首页"){backText=b.text;break;}
 if(vocabulary&&w.route=="settings")backText="返回设置";
 const int backWidth=vocabulary&&backText.size()>6?184:120;
 button(c,{24,12,backWidth,96},root_home?Action::DeviceMenu:Action::Back,backText);
 if(w.keyboard){
  if(vocabulary&&w.route=="search")button(c,{224,12,228,96},Action::Dictionary,"完整词典");
  else c.text_px(backWidth>120?224:180,34,w.route=="word-batch"?(vocabulary?w.title:"练拼写"):"查词",36);
  button(c,{476,12,184,96},Action::Vocabulary,w.route=="word-batch"?"查词":"生词本");
 }
 else{
 std::vector<WebControl> tools;for(const auto&b:w.top)if(b.text.rfind("返回",0)!=0&&b.text!="回首页")tools.push_back(b);
 c.text_px(28,112,w.title,36);
 if(tools.size()>1){const auto&b=tools[tools.size()-2];button(c,{408,12,116,96},Action::WebAction,b.text,b.action,false,b.enabled);}
 if(!tools.empty()){const auto&b=tools.back();button(c,{tools.size()>1?536:510,12,tools.size()>1?128:154,96},Action::WebAction,b.text,b.action,false,b.enabled);}
 else if(!(vocabulary&&(w.route=="word-result"||w.route=="settings")))button(c,{492,12,168,96},(v.more_learning_scope||w.secondary)?Action::MoreLearning:Action::DeviceMenu,(v.more_learning_scope||w.secondary)?"更多学习":"设置");
 }
 c.fill({24,w.keyboard?116:168,636,2});
 if(!w.error.empty()){int y=160;for(const auto&s:c.wrap_px(w.error,620,36)){c.text_px(30,y,s,36);y+=48;}return;}
 if(w.keyboard){
  web_input_field(c,w);
  const bool compactStudy=web_compact_study_keyboard(w);
  int y=w.gap_mask.empty()?284:316;std::vector<WebBlock> visibleBlocks;
  for(const auto&b:w.blocks)if(b.area!="foot"&&b.text!=w.typed)visibleBlocks.push_back(b);
  const auto choices=web_keyboard_controls(w);
  std::vector<std::string> prompts;
  if(!status.empty()&&!vocabulary)prompts=c.wrap(status,636);
  else for(const auto&b:visibleBlocks)if(b.kind=="heading"||(w.gap_mask.empty()&&b.text.find('_')!=std::string::npos)){auto lines=c.wrap(b.text,636);if(b.kind=="heading"&&lines.size()>2)lines={lines[0],c.ellipsize(lines[1]+"…",636)};prompts.insert(prompts.end(),lines.begin(),lines.end());}
  for(const auto&s:prompts){for(const auto&line:c.wrap_px(s,636,w.gap_mask.empty()?40:36)){if(y>(w.gap_mask.empty()?348:356))break;c.text_px(24,y,line,w.gap_mask.empty()?40:36);y+=w.gap_mask.empty()?48:44;}}
  const auto revealed=std::find_if(visibleBlocks.begin(),visibleBlocks.end(),[&](const auto&b){return compactStudy&&b.role=="word";});
  const int textLimit=compactStudy?(revealed!=visibleBlocks.end()?396:600):370;
  if(prompts.empty()){for(const auto&b:visibleBlocks)if(b.kind!="button"&&!(compactStudy&&b.role=="word")){for(const auto&s:c.wrap_px(b.text,636,36)){if(y>textLimit)break;c.text_px(24,y,s,36);y+=44;}if(y>textLimit)break;}}
  if(revealed!=visibleBlocks.end()){
   c.text_px(24,442,"已查看答案",36);int answerY=490;
   for(const auto&line:c.wrap_px(revealed->text,636,48)){if(answerY>586)break;c.text_px(24,answerY,line,48);answerY+=48;}
  }
  if(compactStudy){
   if(!choices.empty()){const auto&b=*choices[0];button(c,{24,650,312,88},Action::DictionaryRecent,b.text,0,false,b.enabled);}
   button(c,{348,650,312,88},Action::RecallSkip,"想不起来");
  }
  else for(size_t row=0;row<choices.size();++row){const auto&b=*choices[row];const bool expanded=row==1&&web_long_prompt(w);const Rect r=row<2?Rect{24,398+int(row)*108,636,88}:Rect{24,650,636,88};button(c,r,b.role=="device-dictionary"?Action::Dictionary:Action::DictionaryRecent,expanded?"查看完整题干与提示":c.ellipsize(b.text,600),int(row),false,expanded||b.enabled);}
  for(const auto&b:visibleBlocks)if(b.text.rfind("提示：",0)==0){const auto lines=c.wrap_px(b.text,636,36);if(!lines.empty())c.text_px(24,608,lines.front(),36);}
  if(w.route=="word-batch"&&!compactStudy){button(c,{24,650,636,88},Action::RecallSkip,"暂缓本题，稍后复习");}
  {const auto note=vocabulary&&!status.empty()?status:w.speech_note.empty()?"长按退格连续删除":w.speech_note;const auto lines=c.wrap_px(note,636,36);if(!lines.empty())c.text_px(24,748,lines.front(),36);}
  for(int slot=0;slot<30;++slot){const auto k=dictionary_keyboard_key(slot,w.shift,w.symbols);std::string label;
   if(k.event.action==Action::DictionaryKey)label=k.event.value==' '?"空格":std::string(1,char(k.event.value));
   else if(k.event.action==Action::DictionaryShift)label="大写";
   else if(k.event.action==Action::DictionarySymbols)label=w.symbols?"ABC":"123";else label="退格";
   button(c,k.rect,k.event.action,label,k.event.value);
  }
  button(c,{518,1092,164,84},Action::DictionarySearch,w.route=="word-batch"?"检查":"查询",0,true,w.can_submit);
  for(int py=1092;py<1176;++py)for(int px=518;px<682;++px)c.pixel(px,py,c.shade(px,py)<8);
  for(int py=124;py<354;++py)for(int px=12;px<672;++px)c.pixel(px,py,c.shade(px,py)<8);
  return;
 }
 std::vector<WebBlock> content,feet;bool details=false;
 if(!status.empty())content.push_back({"text",status,"body"});
 if(!w.speech_note.empty()){WebBlock source{"text",w.speech_note,"body"};source.role="source";content.push_back(source);}
 for(const auto&b:w.blocks){
  if(b.role=="detail"&&v.screen!=Screen::WebPrompt){details=true;continue;}
  if(b.area=="foot"&&b.kind=="button"&&b.action>=0)feet.push_back(b);else content.push_back(b);
 }
 // On Home the sole next-step action is anchored above the four destinations.
 if(w.route=="home"&&v.screen!=Screen::WebPrompt)for(auto it=content.begin();it!=content.end();){if(it->primary&&it->kind=="button"){feet.insert(feet.begin(),*it);it=content.erase(it);}else ++it;}
 if(feet.size()>4){content.insert(content.end(),feet.begin(),feet.end()-1);feet.erase(feet.begin(),feet.end()-1);}
 if(details)button(c,{backWidth>120?216:156,12,112,96},Action::WebPage,"说明",0);
 auto bigButton=[&](Rect r,Action a,const std::string&label,int value,bool primary,bool enabled,int px=38){
  button(c,r,a,"",value,primary,enabled);auto lines=c.wrap_px(label,r.w-36,px);const int lh=px+10;
  const int top=r.y+(r.h-int(lines.size())*lh)/2;for(size_t j=0;j<lines.size();++j)c.text_px(r.x+(r.w-c.text_width_px(lines[j],px))/2,top+int(j)*lh,lines[j],px);buttons_.back().label=label;
 };
 const int navY=w.tabs.empty()?1204:1104,footRows=(int(feet.size())+1)/2;
 struct FooterRow{std::vector<WebBlock> buttons;int height=100;int px=36;};
 std::vector<FooterRow> footer;
 if(vocabulary){
  std::stable_partition(feet.begin(),feet.end(),[](const auto&b){return !b.primary;});
  for(const auto &b:feet){
   if(b.role=="audio"&&!footer.empty()&&footer.back().buttons.size()==1&&footer.back().buttons[0].role=="audio")footer.back().buttons.push_back(b);
   else {const int px=b.role=="option"?40:36;footer.push_back({{b},std::max(100,int(c.wrap_px(b.text,600,px).size())*(px+10)+28),px});}
  }
 }
 int footerHeight=0;for(const auto&r:footer)footerHeight+=r.height+12;
 const int footY=navY-(vocabulary?(footer.empty()?0:footerHeight+12):(footRows?footRows*112+12:0));
 struct Item{WebBlock b;std::vector<std::string> lines,sub;int height=0,px=40,line=52;std::vector<WebBlock> group={};};
 const int startY=194;
 auto makePages=[&](int reserve){std::vector<std::vector<Item>> pages(1);int used=0;const int cap=footY-startY-reserve;
  for(size_t bi=0;bi<content.size();++bi){const auto&b=content[bi];
   if(b.kind=="button"&&(b.role=="audio"||b.role=="read-nav")){
    std::vector<WebBlock> group{b};while(bi+1<content.size()&&content[bi+1].role==b.role&&group.size()<3)group.push_back(content[++bi]);
    const int h=104;if(used+h+16>cap&&!pages.back().empty()){pages.emplace_back();used=0;}
    pages.back().push_back({b,{},{},h,36,48,group});used+=h+16;continue;
   }
   // Keep the question with its alternatives whenever the complete set fits.
   if(b.role=="question"){int need=int(c.wrap_px(b.text,628,40).size())*52+22;for(size_t j=bi+1;j<content.size()&&content[j].role=="option";++j)need+=std::max(96,int(c.wrap_px(content[j].text,592,40).size())*52+28)+16;if(need<=cap&&used+need>cap&&!pages.back().empty()){pages.emplace_back();used=0;}}
   const int px=b.role=="word"?78:b.role=="ipa"?36:b.kind=="heading"?48:b.role=="meaning"?44:b.role=="detail"||b.role=="source"?36:40,lh=px+12;
   const int width=b.kind=="button"?592:b.role=="progress"?320:628;
   auto lines=c.wrap_px(b.text,width,px);auto sub=b.detail.empty()?std::vector<std::string>{}:c.wrap_px(b.detail,b.role=="progress"?250:592,36);
   if(b.kind=="button"||b.role=="progress"){
    const int h=b.role=="progress"?88:std::max(96,int(lines.size())*lh+int(sub.size())*46+28);
    if(used+h+16>cap&&!pages.back().empty()){pages.emplace_back();used=0;}
    pages.back().push_back({b,lines,sub,h,px,lh});used+=h+16;
   }else{size_t at=0;while(at<lines.size()){int room=(cap-used-22)/lh;if(room<1){pages.emplace_back();used=0;room=std::max(1,(cap-22)/lh);}const int n=std::min(int(lines.size()-at),room);std::vector<std::string> take(lines.begin()+at,lines.begin()+at+n);const int h=n*lh+22;pages.back().push_back({b,take,{},h,px,lh});used+=h;at+=n;}}
  }return pages;
 };
 auto pages=makePages(0);if(pages.size()>1)pages=makePages(100);
 answer_pages_=int(pages.size());const int page=std::clamp(v.web_page,0,answer_pages_-1);int y=startY;
 for(const auto&i:pages[page]){
  if(!i.group.empty()){const int count=int(i.group.size()),ww=(636-(count-1)*12)/count;for(int j=0;j<count;++j){const auto&b=i.group[j];bigButton({24+j*(ww+12),y,ww,i.height},Action::WebAction,b.text,b.action,b.primary,b.enabled,36);}}
  else if(i.b.role=="progress"){c.text_px(28,y+12,i.b.text,40);int sy=y+18;for(const auto&s:i.sub){c.text_px(656-c.text_width_px(s,36),sy,s,36);sy+=46;}c.fill({28,y+86,628,1});}
  else{const int inset=i.b.kind=="button"?46:28;
   if(i.b.kind=="button"){button(c,{24,y,636,i.height},Action::WebAction,"",i.b.action,i.b.primary,i.b.enabled);if(i.b.selected)c.border({24,y,636,i.height},6);buttons_.back().label=i.b.text;}
   int ly=y+(i.b.kind=="button"?14:0);for(const auto&s:i.lines){c.text_px(inset,ly,s,i.px);ly+=i.line;}for(const auto&s:i.sub){c.text_px(inset,ly,s,36);ly+=46;}
  }y+=i.height+(i.b.kind=="button"||i.b.role=="progress"?16:0);
 }
 const bool reading=std::any_of(content.begin(),content.end(),[](const auto&b){return b.role=="passage";});
 if(answer_pages_>1){const int py=footY-100;button(c,{24,py,196,88},Action::WebPage,reading?"返回原文":"上一屏",-1,false,page>0);const auto label=std::to_string(page+1)+" / "+std::to_string(answer_pages_);c.text_px(342-c.text_width_px(label,36)/2,py+24,label,36);if(feet.empty())button(c,{464,py,196,88},Action::WebPage,"下一屏",1,false,page+1<answer_pages_);}
 if(page+1<answer_pages_&&!feet.empty()){
  if(vocabulary){int fy=footY;for(const auto&r:footer){if(r.buttons[0].role=="audio"){int ww=r.buttons.size()==2?312:636;for(size_t j=0;j<r.buttons.size();++j){const auto&b=r.buttons[j];bigButton({24+int(j)*324,fy,ww,r.height},Action::WebAction,b.text,b.action,false,b.enabled,36);}}fy+=r.height+12;}}
  bigButton({24,vocabulary?navY-124:footY,636,100},Action::WebPage,reading?"查看题目":"下一屏",1,true,true,36);
 }
 else if(vocabulary){int fy=footY;for(const auto&r:footer){int ww=r.buttons.size()==2?312:636;for(size_t j=0;j<r.buttons.size();++j){const auto&b=r.buttons[j];bigButton({24+int(j)*324,fy,ww,r.height},Action::WebAction,b.text,b.action,b.primary,b.enabled,r.px);}fy+=r.height+12;}}
 else for(size_t i=0;i<feet.size();++i){const auto&b=feet[i];const bool single=feet.size()==1||(feet.size()%2&&i==feet.size()-1);const int ww=single?636:312,xx=single?24:24+int(i%2)*324,yy=footY+int(i/2)*112;bigButton({xx,yy,ww,100},Action::WebAction,b.text,b.action,b.primary,b.enabled,36);}
 if(!w.tabs.empty())navBar(w);
}
}
