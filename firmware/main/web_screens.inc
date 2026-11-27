#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include "web_keyboard.hpp"
#include "web_disclosure.hpp"
#include "web_input_field.hpp"
#include "vocabulary_picture_draw.hpp"
#include "journey_seal.hpp"
#include "home_folio.hpp"
#include <algorithm>
namespace wordcard {
void UI::web_pages(Canvas&c,const View&v){
 auto w=v.web;
 const bool card = !w.keyboard && word_card_head(w);
 const bool listening = v.screen==Screen::WebPrompt && card_example(w,v.web_example);
 const bool cardBack = (card || listening) && v.screen==Screen::WebPrompt;
 const auto blockAction=[](const WebBlock &b){return b.role=="example-audio"?Action::WebExample:Action::WebAction;};
 const auto audioButton=[](const WebBlock &b){return b.role=="audio"||b.role=="example-audio";};
 const char *cardDetails=nullptr;
 if(v.screen==Screen::WebPrompt)w=web_disclosure(w,v.web_example);
 else cardDetails=layout_word_card(w,false);
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
   entry(152,Action::LearningSettings,"背词设置");
   entry(265,Action::Settings,"声音与前光");
   entry(378,Action::Date,v.date_valid?"日期与时间":"设置日期与时间");
   entry(491,Action::Haptics,v.haptics?"触控反馈：开启":"触控反馈：关闭");
   entry(604,Action::Resources,"SD 卡与 USB 传输");
   entry(717,Action::StorageCheck,"检查存储与保存");
   entry(830,Action::MoreLearning,"更多学习");
  }
  if(!v.notice.empty()){int y=952;for(const auto&s:c.wrap_px(v.notice,628,32)){if(y>1028)break;c.text_px(28,y,s,32);y+=40;}}
  button(c,{24,1080,636,100},Action::CloseSettings,v.settings_exit_label(),0,true);return;
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
 #include "web_learning_folio.inc"
 #include "web_history.inc"
 #include "web_journey.inc"
 #include "web_sections.inc"
 std::string backText=root_home?"设备":"返回";
 if(vocabulary&&!root_home)for(const auto&b:w.top)if(b.text.rfind("返回",0)==0||b.text=="回首页"||b.text=="回收集册"){backText=b.text;break;}
 if(vocabulary&&w.route=="settings"&&v.settings_origin)backText="返回设置";
 if(cardBack)backText="返回词卡";
 const auto storyHeader=std::find_if(w.blocks.begin(),w.blocks.end(),[](const auto&b){return b.role=="story-heading";});
 const bool storyBrief=storyHeader!=w.blocks.end();
 const bool nativeKeyboard=web_native_search_keyboard(w)||web_compact_study_keyboard(w);
 const int backWidth=storyBrief?222:cardBack||nativeKeyboard||(vocabulary&&backText.size()>6)?184:120;
 pageButton({24,12,backWidth,96},root_home?Action::DeviceMenu:Action::Back,backText);
 if(w.keyboard){
  if(web_native_search_keyboard(w))pageButton(study_keyboard_dictionary,Action::Dictionary,"完整词典");
  else if(nativeKeyboard){
   const auto lines=c.wrap_px(w.title,172,32);
   int y=lines.size()>1?16:34;
   for(const auto&line:lines){c.text_px(216,y,line,32);y+=40;}
  }
  else c.text_px(backWidth>120?224:180,34,w.route=="history-search"?"查找已学词":w.route=="word-batch"?"练拼写":"查词",36);
  if(w.route!="history-search"&&w.key.rfind("vocabulary:history-quiz:",0)!=0)pageButton(nativeKeyboard?study_keyboard_words:Rect{476,12,184,96},Action::Vocabulary,w.route=="word-batch"?"查词":"生词本");
  if(nativeKeyboard)pageButton(study_keyboard_settings,Action::DeviceMenu,"设置");
 }
 else{
 std::vector<WebControl> tools;for(const auto&b:w.top)if(b.text.rfind("返回",0)!=0&&b.text!="回首页"&&b.text!="回收集册")tools.push_back(b);
 if(storyBrief)folio_line(c,342,60,storyHeader->detail,28,Canvas::Align::Center);
 else if(folioUI)folio_line(c,42,137,w.title,136);else c.text_px(28,112,w.title,36);
 if(tools.size()>1){const auto&b=tools[tools.size()-2];pageButton({408,12,116,96},Action::WebAction,b.text,b.action,false,b.enabled);}
 if(!tools.empty()){const auto&b=tools.back();pageButton({tools.size()>1?536:510,12,tools.size()>1?128:154,96},Action::WebAction,b.text,b.action,false,b.enabled);}
 else if(vocabulary&&w.route=="paused")
  pageButton({506,12,154,96},v.settings_origin?Action::CloseSettings:Action::Home,v.settings_origin?v.settings_exit_label():"回首页");
 else if(!cardBack && !(vocabulary&&(w.route=="word-result"||w.route=="modes"||w.route=="settings")))pageButton({492,12,168,96},(v.more_learning_scope||w.secondary)?Action::MoreLearning:Action::DeviceMenu,(v.more_learning_scope||w.secondary)?"更多学习":"设置");
 }
 if(storyBrief)c.fill({42,112,600,2});else c.fill({24,w.keyboard?116:168,636,2});
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
   if(!choices.empty()){const auto&b=*choices[0];pageButton({24,650,312,88},Action::DictionaryRecent,b.text,0,false,b.enabled);}
   pageButton({348,650,312,88},Action::RecallSkip,"想不起来");
  }
  else for(size_t row=0;row<choices.size();++row){const auto&b=*choices[row];const bool expanded=row==1&&web_long_prompt(w);const Rect r=row<2?Rect{24,398+int(row)*108,636,88}:Rect{24,650,636,88};pageButton(r,b.role=="device-dictionary"?Action::Dictionary:Action::DictionaryRecent,expanded?"查看完整题干与提示":c.ellipsize(b.text,600),int(row),false,expanded||b.enabled);}
  for(const auto&b:visibleBlocks)if(b.text.rfind("提示：",0)==0){const auto lines=c.wrap_px(b.text,636,36);if(!lines.empty())c.text_px(24,608,lines.front(),36);}
  if(w.route=="word-batch"&&!compactStudy){pageButton({24,650,636,88},Action::RecallSkip,"暂缓本题，稍后复习");}
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
 if(vocabulary){
  const auto wideOption=[&](const WebBlock &b){
   return b.role=="option" && (folioUI?folio_line(c,0,0,b.text,236,Canvas::Align::Left,false):c.text_width_px(b.text,36))>548;
  };
  if(std::any_of(feet.begin(),feet.end(),wideOption)){
   int index=0;
   for(auto &b:feet)if(b.role=="option"){
    const auto letter=std::string(1,char('A'+index++));
    WebBlock full;full.kind="text";full.role="reader-copy";full.text=letter+"   "+b.text;
    content.push_back(std::move(full));b.text=letter;b.role="answer-letter";
   }
  }
 }
 const bool letterChoices=std::any_of(feet.begin(),feet.end(),[](const auto&b){return b.role=="answer-letter";});
 if(feet.size()>4&&!letterChoices){content.insert(content.end(),feet.begin(),feet.end()-1);feet.erase(feet.begin(),feet.end()-1);}
 if(cardDetails)pageButton({212,12,180,96},Action::WebPage,cardDetails,0);
 if(details)pageButton({backWidth>120?216:156,12,112,96},Action::WebPage,"说明",0);
 auto bigButton=[&](Rect r,Action a,const std::string&label,int value,bool primary,bool enabled,int px=38){
  if(folioUI){
   const int slot=primary?44:px==40?236:136;
   if(r.w==636){r.x=42;r.w=600;}
   folioButton(r,a,label,value,primary,enabled,slot);return;
  }
  pageButton(r,a,"",value,primary,enabled);
  button_line(c,r,label,px,r.w-36,false);
  buttons_.back().label=label;
 };
 const int navY=w.tabs.empty()?1204:1104,footRows=(int(feet.size())+1)/2;
 struct FooterRow{std::vector<WebBlock> buttons;int height=100;int px=36;};
 std::vector<FooterRow> footer;
 if(vocabulary){
  std::stable_partition(feet.begin(),feet.end(),[](const auto&b){return !b.primary;});
  for(const auto &b:feet){
   if((b.role=="answer-letter"||b.role=="picture-nav"||b.role=="word-nav")&&!footer.empty()&&footer.back().buttons[0].role==b.role&&footer.back().buttons.size()<4)footer.back().buttons.push_back(b);
   else if(audioButton(b)&&!footer.empty()&&footer.back().buttons.size()==1&&audioButton(footer.back().buttons[0]))footer.back().buttons.push_back(b);
   else {const int px=b.role=="option"?40:36;footer.push_back({{b},std::max(100,int(c.wrap_px(b.text,folioUI?(b.primary?512:548):600,folioUI?(b.primary?44:36):px,folioUI?(b.primary?44:b.role=="option"?236:136):0).size())*((folioUI?(b.primary?44:36):px)+10)+28),px});}
  }
 }
 int footerHeight=0;for(const auto&r:footer)footerHeight+=r.height+12;
 const int footY=navY-(vocabulary?(footer.empty()?0:footerHeight+12):(footRows?footRows*112+12:0));
 struct Item{WebBlock b;std::vector<std::string> lines,sub;int height=0,px=40,line=52;std::vector<WebBlock> group={};};
 const int startY=storyBrief?152:folioUI?205:194;
 auto makePages=[&](int reserve){std::vector<std::vector<Item>> pages(1);int used=0;const int cap=footY-startY-reserve;
  for(size_t bi=0;bi<content.size();++bi){const auto&b=content[bi];
   if(b.picture>=0) {
    if(b.role=="card-illustration"){
     const int h=b.picture_height+16;
     if(pages.size()==1 && used+h<=cap){pages.back().push_back({b,{},{},h});used+=h;}
     continue;
    }
    const bool wide=b.picture_width>160;
    const int px=b.role=="word"?64:40,lh=px+12;auto lines=c.wrap_px(b.text,wide?592:432,px,folioUI && b.role=="word"?px:0);auto sub=c.wrap_px(b.detail,wide?592:432,36);
    const int textHeight=int(lines.size())*lh+int(sub.size())*46+28;
    const int h=wide?textHeight+b.picture_height+8:std::max(176,textHeight);
    if(used+h+16>cap&&!pages.back().empty()){pages.emplace_back();used=0;}
    pages.back().push_back({b,lines,sub,h,px,lh});used+=h+16;continue;
   }
   if(b.kind=="button"&&(audioButton(b)||b.role=="read-nav")){
    std::vector<WebBlock> group{b};while(bi+1<content.size()&&(content[bi+1].role==b.role||(audioButton(b)&&audioButton(content[bi+1])))&&group.size()<3)group.push_back(content[++bi]);
    const int h=104;if(used+h+16>cap&&!pages.back().empty()){pages.emplace_back();used=0;}
    pages.back().push_back({b,{},{},h,36,48,group});used+=h+16;continue;
   }
   // Keep the question with its alternatives whenever the complete set fits.
   if(b.role=="question"){int need=int(c.wrap_px(b.text,628,40).size())*52+22;for(size_t j=bi+1;j<content.size()&&content[j].role=="option";++j)need+=std::max(96,int(c.wrap_px(content[j].text,592,40).size())*52+28)+16;if(need<=cap&&used+need>cap&&!pages.back().empty()){pages.emplace_back();used=0;}}
   const int px=b.role=="card-parts"?36:b.role=="card-part"?36:card&&b.role=="ipa"?30:b.role=="reader-copy"?34:b.role=="story-heading"?52:storyBrief&&b.role=="meaning"?36:storyBrief&&b.role=="source"?28:b.role=="stamp-book"?52:b.role=="word"?78:b.role=="ipa"?36:b.kind=="heading"?48:b.role=="meaning"?44:b.role=="detail"||b.role=="source"?36:40,lh=px+12;
   const int width=b.kind=="button"?592:b.role=="progress"?320:(folioUI?596:628);
   auto lines=c.wrap_px(b.text,width,px,(folioUI && (b.role=="word"||b.role=="story-heading"))?px:0,folioUI);auto sub=b.detail.empty()?std::vector<std::string>{}:c.wrap_px(b.detail,b.role=="progress"?250:592,36);
   if(b.role=="example-pair"){
    // Keep a sentence and its translation together when they fit on a page.
    // Oversized pairs use the normal bounded line splitter, without truncation.
    auto translated=c.wrap_px(b.detail,width,px,0,folioUI);lines.insert(lines.end(),translated.begin(),translated.end());sub.clear();
   }
   if(b.role=="example-pair"||b.role=="meaning"){
    const int need=int(lines.size())*lh+22;
    if(need<=cap&&used+need>cap&&!pages.back().empty()){pages.emplace_back();used=0;}
   }
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
  if(!i.group.empty()){const int count=int(i.group.size()),ww=(636-(count-1)*12)/count;for(int j=0;j<count;++j){const auto&b=i.group[j];bigButton({24+j*(ww+12),y,ww,i.height},blockAction(b),b.text,b.action,b.primary,b.enabled,36);}}
  else if(i.b.role=="progress"){c.text_px(28,y+12,i.b.text,40);int sy=y+18;for(const auto&s:i.sub){c.text_px(656-c.text_width_px(s,36),sy,s,36);sy+=46;}c.fill({28,y+86,628,1});}
  else{const int inset=i.b.kind=="button"?46:folioUI?42:28;
   if(i.b.kind=="button"){button(c,{24,y,636,i.height},blockAction(i.b),"",i.b.action,i.b.primary,i.b.enabled);if(i.b.selected)c.border({24,y,636,i.height},6);buttons_.back().label=i.b.text;}
   int ly=y+(i.b.kind=="button"?14:0);for(const auto&s:i.lines){if(folioUI && i.b.role=="stamp-book")bookLine(inset,ly+i.px/2,s);
    else if(folioUI && (i.b.role=="word"||i.b.role=="story-heading"))folio_line(c,inset,ly+i.px/2,s,i.px);
    else if(folioUI && (i.b.kind=="heading" || i.b.kind=="button"))folio_line(c,inset,ly+i.px/2,s,136);
    else c.text_px(inset,ly,s,i.px);
    ly+=i.line;}for(const auto&s:i.sub){c.text_px(inset,ly,s,36);ly+=46;}
   if(i.b.picture>=0){const bool wide=i.b.picture_width>160;const bool centered=wide||i.b.role=="card-illustration"||i.b.role=="picture";draw_vocabulary_picture(c,i.b,centered?(684-i.b.picture_width)/2:490,wide?ly+8:y+8);}
  }y+=i.height+(i.b.kind=="button"||i.b.role=="progress"?16:0);
 }
 const bool wordNavigation=card||listening||std::any_of(feet.begin(),feet.end(),[](const auto &b){return b.role=="word-nav";});
 const bool reading=std::any_of(content.begin(),content.end(),[](const auto&b){return b.role=="passage";});
 if(answer_pages_>1){const int py=footY-100;pageButton({24,py,196,88},Action::WebPage,reading?"返回原文":"上一屏",-1,false,page>0);const auto label=std::to_string(page+1)+" / "+std::to_string(answer_pages_);c.text_px(342-c.text_width_px(label,36)/2,py+24,label,36);if(feet.empty()||wordNavigation)pageButton({464,py,196,88},Action::WebPage,"下一屏",1,false,page+1<answer_pages_);}
 if(page+1<answer_pages_&&!feet.empty()&&!wordNavigation){
  if(vocabulary){int fy=footY;for(const auto&r:footer){if(audioButton(r.buttons[0])){int ww=r.buttons.size()==2?312:636;for(size_t j=0;j<r.buttons.size();++j){const auto&b=r.buttons[j];bigButton({24+int(j)*324,fy,ww,r.height},blockAction(b),b.text,b.action,false,b.enabled,36);}}fy+=r.height+12;}}
  bigButton({24,vocabulary?navY-124:footY,636,100},Action::WebPage,reading?"查看题目":"下一屏",1,true,true,36);
 }
 else if(vocabulary){int fy=footY;for(const auto&r:footer){const int count=int(r.buttons.size()),ww=(636-(count-1)*12)/count;for(size_t j=0;j<r.buttons.size();++j){
  const auto&b=r.buttons[j];const bool exitSettings=b.role=="settings-exit"&&v.settings_origin;
  bigButton({24+int(j)*(ww+12),fy,ww,r.height},exitSettings?Action::CloseSettings:blockAction(b),
            exitSettings?v.settings_exit_label():b.text,b.action,b.primary,b.enabled,r.px);
 }fy+=r.height+12;}}
 else for(size_t i=0;i<feet.size();++i){const auto&b=feet[i];const bool single=feet.size()==1||(feet.size()%2&&i==feet.size()-1);const int ww=single?636:312,xx=single?24:24+int(i%2)*324,yy=footY+int(i/2)*112;bigButton({xx,yy,ww,100},blockAction(b),b.text,b.action,b.primary,b.enabled,36);}
 if(!w.tabs.empty())navBar(w,folioUI);
}
}
