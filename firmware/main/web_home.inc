// A product-level home, not the linear lesson-body paginator.
auto navBar=[&](const WebPage&p,bool folio=false){
 c.fill({0,1102,684,2});
 for(size_t i=0;i<p.tabs.size()&&i<4;++i){
  const auto&b=p.tabs[i];const auto label=p.secondary&&i==0?std::string("进度"):b.text;const int x=int(i)*171;
  buttons_.push_back({{x+4,1108,163,104},{Action::WebAction,b.action},label,true,false});
  if(b.selected)c.fill({x+(folio?59:24),1104,folio?53:123,folio?4:5});
  if(folio){
   const int cx=x+86,cy=1143;
   if(i==0){
    for(int j=0;j<20;++j){c.fill({cx-20+j,cy-j,3,3});c.fill({cx+j,cy-20+j,3,3});}
    c.fill({cx-15,cy-1,3,17});c.fill({cx+13,cy-1,3,17});c.fill({cx-15,cy+14,31,3});
   }else if(i==1){c.border({cx-20,cy-13,21,30},3);c.border({cx,cy-13,21,30},3);}
   else if(i==2){c.border({cx-16,cy-14,32,28},3);c.fill({cx-10,cy-5,20,2});c.fill({cx-10,cy+4,16,2});c.fill({cx-16,cy+19,34,3});}
   else{
    for(int dy=-15;dy<=15;++dy)for(int dx=-15;dx<=15;++dx){const int d=dx*dx+dy*dy;if(d>=132&&d<=210)c.pixel(cx-3+dx,cy-3+dy);}
    for(int j=0;j<14;++j)c.fill({cx+6+j,cy+7+j,3,3});
   }
   folio_line(c,cx,1184,label,28,Canvas::Align::Center);
   continue;
  }
  // Simple authored outline icons, with identical line weight.
  const int cx=x+86,y=1130;
  if(i==0){for(int j=0;j<17;++j){c.fill({cx-18+j,y+16-j,3,3});c.fill({cx+j,y+j,3,3});}c.border({cx-14,y+18,30,22},3);c.border({cx-3,y+26,10,14},3);}
  else if(i==1){c.border({cx-21,y+2,21,36},3);c.border({cx,y+2,21,36},3);c.fill({cx-15,y+10,10,2});c.fill({cx+6,y+10,10,2});}
  else if(i==2){c.border({cx-18,y+3,36,34},3);c.fill({cx-11,y+12,21,3});c.fill({cx-11,y+23,15,3});c.fill({cx-20,y+39,42,3});}
  else{c.border({cx-19,y+1,29,29},3);for(int j=0;j<14;++j)c.fill({cx+8+j,y+27+j,4,4});}
  c.text_px(x+(171-c.text_width_px(label,36))/2,1174,label,36);
 }
};
if(w.route=="content-library"){
 answer_pages_=1;
 folio_line(c,42,53,"内容库",44);
 int tab=0,rows=0;
 bool paging=false;
 std::vector<std::string> messages;
 auto hit=[&](const WebBlock&b,Rect rect){
  if(b.action>=0)buttons_.push_back({rect,{Action::WebAction,b.action},b.text,b.enabled,false});
 };
 for(const auto&b:w.blocks){
  if(b.role=="library-filter"){
   const int x=42+tab++*300;hit(b,{x,86,300,90});
   folio_line(c,x+150,131,b.text,36,Canvas::Align::Center);
   c.fill({x,171,300,b.selected?4:1});
  }else if(b.role=="library-message"){
   const auto lines=c.wrap_px(b.text,600,28,28,true);
   messages.insert(messages.end(),lines.begin(),lines.end());
  }else if(b.role=="library-row")++rows;
  else if(b.role=="library-count")paging=true;
 }
 const int listBottom=paging?995:1093;
 // Keep all eight touch targets clear of paging and the bottom navigation.
 const int messageLines=std::min<int>(messages.size(),(listBottom-184-rows*84)/33);
 for(int i=0;i<messageLines;++i){
  const bool cut=i+1==messageLines&&size_t(messageLines)<messages.size();
  folio_line(c,42,205+i*33,cut?fit_px(c,messages[i]+"…",600,28):messages[i],28);
 }
 int y=184+messageLines*33;
 const int rowHeight=std::min(96,(listBottom-y)/std::max(1,rows));
 for(const auto&b:w.blocks){
  if(b.role=="library-row"){
   hit(b,{24,y,636,rowHeight});const int center=y+rowHeight/2;
   const int metaWidth=folio_line(c,0,0,b.detail,28,Canvas::Align::Left,false);
   const bool inlineMeta=!b.detail.empty()&&metaWidth<=220;
   const int room=inlineMeta?550-metaWidth-24:550;
   // A single face keeps new package names and built-in names at the same weight.
   auto drawName=[&](int cy,const std::string &text,int px){c.text_px(42,cy-(px+10)/2,text,px);};
   auto lines=c.wrap_px(b.text,room,36,0,true);
   const bool stacked=!b.detail.empty()&&!inlineMeta;
   if(stacked){
    drawName(center-18,fit_px(c,b.text,550,36),36);
    folio_line(c,42,center+19,fit_px(c,b.detail,550,28),28);
   }else{
    int px=36;
    if(lines.size()>2){px=28;lines=c.wrap_px(b.text,room,px,0,true);}
    if(lines.size()==1)drawName(center,lines.front(),px);
    else if(!lines.empty()){
     drawName(center-20,lines[0],px);
     drawName(center+20,lines.size()>2?fit_px(c,lines[1]+"…",room,px):lines[1],px);
    }
    if(inlineMeta)folio_line(c,592,center,b.detail,28,Canvas::Align::Right);
   }
   folio_arrow(c,638,center,9);c.fill({42,y+rowHeight-1,600,1});y+=rowHeight;
  }else if(b.role=="library-prev"||b.role=="library-more"){
   const int x=b.role=="library-more"?455:28;
   hit(b,{x,998,200,96});folio_line(c,x+100,1046,b.text,36,Canvas::Align::Center);
  }else if(b.role=="library-count")folio_line(c,342,1046,b.text,28,Canvas::Align::Center);
 }
 navBar(w,true);return;
}
// Vocabulary home has its own hierarchy and a single fixed bottom action.
if(root_home&&std::any_of(w.blocks.begin(),w.blocks.end(),[](const auto&b){return b.role=="vocab-home-stage";})){
 answer_pages_=1;
 auto get=[&](const std::string&role)->WebBlock{for(const auto&b:w.blocks)if(b.role==role)return b;return {};};
 const auto mode = get("vocab-home-mode");
 // Draw compact labels within full-height touch targets.
 auto shortcut = [&](Rect hit, int x, Action action, int value, const std::string &label) {
  buttons_.push_back({hit,{action,value},label,true,false});
  const int width=folio_line(c,x,56,label,28);
  c.fill({x,82,width,2});
 };
 if(mode.action>=0)shortcut({20,12,166,96},42,Action::WebAction,mode.action,mode.text);
 const auto light = v.cct==3 || v.brightness==0 ? std::string("前光 关") : "前光 "+std::to_string(v.brightness)+"%";
 shortcut({188,12,156,96},210,Action::Settings,0,light);
 const auto battery = v.battery_percent<0 ? std::string("电量 —") : "电量约"+std::to_string(v.battery_percent)+"%";
 folio_line(c,376,56,battery,28);
 for(const auto &b:w.top)if(b.text=="设置"){
  buttons_.push_back({{536,12,124,96},{Action::DeviceMenu,0},b.text,b.enabled,false});
  folio_line(c,642,56,b.text,28,Canvas::Align::Right);
 }
 c.fill({42,111,600,2});
 folio_title(c,get("vocab-home-stage").text);
 const auto hint=c.wrap_px(get("vocab-home-next").text,590,36);
 int y=hint.size()>2?285:316;
 for(const auto &line:hint){
  if(y>380)break;
  folio_line(c,42,y,line,36);y+=45;
 }
 const auto progressLabel=get("home-progress-label").text;
 folio_line(c,42,446,progressLabel.empty()?"今日新词":progressLabel,136);
 const auto fraction=std::to_string(w.home_new_words)+" / "+std::to_string(w.home_new_target);
 folio_line(c,642,446,fraction,44,Canvas::Align::Right);
 const int divisions=std::clamp(w.home_new_target,1,20);
 const int done=std::clamp(w.home_new_words,0,w.home_new_target>0?w.home_new_target:0);
 for(int i=0;i<divisions;++i){
  const int left=42+i*600/divisions,right=42+(i+1)*600/divisions-7;
  const bool filled=w.home_new_target>0&&(i+1)*w.home_new_target<=done*divisions;
  if(filled)c.fill({left,495,right-left,8});else c.border({left,495,right-left,8},2);
 }
 const auto history=get("vocab-home-history").text;
 const auto streak=get("vocab-home-streak").text;
 const int historyWidth=folio_line(c,42,559,history,28);
 if(!streak.empty()){
  const int width=folio_line(c,0,0,streak,28,Canvas::Align::Left,false);
  if(642-width>54+historyWidth)folio_line(c,642,559,streak,28,Canvas::Align::Right);
  else folio_line(c,42,600,streak,28);
 }
 c.fill({42,632,600,2});
 const auto album=get("vocab-home-collection");
 if(album.action>=0)buttons_.push_back({{24,671,636,210},{Action::WebAction,album.action},album.text,album.enabled,false});
 const auto separator=album.text.find(" · ");
 const auto collectionTitle=album.text.substr(0,separator);
 folio_line(c,42,709,collectionTitle,136);
 if(separator!=std::string::npos)folio_line(c,597,709,album.text.substr(separator+std::string(" · ").size()),28,Canvas::Align::Right);
 if(album.enabled)folio_arrow(c,638,709,10);
 int slot=0;for(const auto&b:w.blocks)if(b.role=="vocab-home-stamp"&&slot<6){
  folio_stamp(c,79+(slot*526+2)/5,801,slot,b.text,b.selected,false);++slot;
 }
 const auto note=status.empty()?get("vocab-home-stamp-note").text:status;
 const auto link=get("shop-link");
 const auto learned=get("vocab-home-learned");
 if(learned.action>=0&&status.empty()){
  const int width=link.action>=0?312:636;
  buttons_.push_back({{24,882,width,88},{Action::WebAction,learned.action},learned.text,true,false});
  folio_line(c,42,925,learned.text,36);folio_arrow(c,24+width-24,925,9);
  if(link.action>=0){
   buttons_.push_back({{350,882,310,88},{Action::WebAction,link.action},link.text,true,false});
   folio_line(c,367,925,fit_px(c,link.text,246,36),36);folio_arrow(c,636,925,9);
  }
 }else if(link.action>=0&&status.empty()){
  buttons_.push_back({{24,882,636,88},{Action::WebAction,link.action},link.text,true,false});
  folio_line(c,42,925,link.text,136);folio_arrow(c,636,925,10);
 }else{y=898;for(const auto &line:c.wrap_px(note,593,28)){if(y>936)break;folio_line(c,42,y,line,28);y+=38;}}
 const auto thumb=get("shop-thumb");if(thumb.picture>=0)draw_vocabulary_picture(c,thumb,450,522);
 for(const auto &b:w.blocks)if(b.kind=="button"&&b.area=="foot"){
  buttons_.push_back({{24,980,636,100},{Action::WebAction,b.action},b.text,b.enabled,b.primary});
  c.border({42,980,600,100},b.enabled?3:1);
  folio_line(c,67,1030,b.text,44);
  if(b.enabled)folio_arrow(c,614,1030,13);
 }
 navBar(w,true);return;
}
if(root_home&&std::any_of(w.blocks.begin(),w.blocks.end(),[](const auto&b){return b.role=="home-course";})){
 answer_pages_=1;
 auto get=[&](const std::string&role)->WebBlock{for(const auto&b:w.blocks)if(b.role==role)return b;return {};};
 auto control=[&](Rect r,const std::string&label,Action a,int value,bool primary=false,int px=36){
  button(c,r,a,"",value,primary);buttons_.back().label=label;
  c.text_px(r.x+(r.w-c.text_width_px(label,px))/2,r.y+(r.h-px-10)/2,label,px);
 };
 c.text_px(28,28,(v.more_learning_scope||w.secondary)?"课程进度":"小小单词卡",44);control({492,12,168,96},(v.more_learning_scope||w.secondary)?"更多学习":"设置",(v.more_learning_scope||w.secondary)?Action::MoreLearning:Action::DeviceMenu,0);
 c.fill({24,128,636,2});
 const auto course=get("home-course");c.text_px(28,194,course.text,38);
 auto title=c.wrap_px(course.detail,628,52);int y=274;
 for(size_t i=0;i<title.size()&&i<2;++i){c.text_px(28,y,title[i],52);y+=64;}
 auto next=c.wrap_px(get("home-next").text,628,36);y=458;
 for(size_t i=0;i<next.size()&&i<2;++i){c.text_px(28,y,next[i],36);y+=48;}
 const auto main=get("home-primary");if(main.action>=0)control({24,590,636,112},main.text,Action::WebAction,main.action,true,44);
 c.text_px(28,810,get("home-summary").text,40);
 const auto more=get("home-today");control({24,908,636,100},more.text,Action::WebAction,more.action,false,38);
 if(!status.empty()){c.fill({24,174,636,240},false);int sy=188;for(const auto&line:c.wrap_px(status,628,38)){if(sy>364)break;c.text_px(28,sy,line,38);sy+=48;}}
 navBar(w);return;
}
