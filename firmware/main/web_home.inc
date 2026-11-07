// A product-level home, not the linear lesson-body paginator.
auto navBar=[&](const WebPage&p){
 c.fill({0,1102,684,2});
 for(size_t i=0;i<p.tabs.size()&&i<4;++i){
  const auto&b=p.tabs[i];const auto label=p.secondary&&i==0?std::string("进度"):b.text;const int x=int(i)*171;
  buttons_.push_back({{x+4,1108,163,104},{Action::WebAction,b.action},label,true,false});
  if(b.selected)c.fill({x+24,1104,123,5});
  // Simple authored outline icons, with identical line weight.
  const int cx=x+86,y=1130;
  if(i==0){for(int j=0;j<17;++j){c.fill({cx-18+j,y+16-j,3,3});c.fill({cx+j,y+j,3,3});}c.border({cx-14,y+18,30,22},3);c.border({cx-3,y+26,10,14},3);}
  else if(i==1){c.border({cx-21,y+2,21,36},3);c.border({cx,y+2,21,36},3);c.fill({cx-15,y+10,10,2});c.fill({cx+6,y+10,10,2});}
  else if(i==2){c.border({cx-18,y+3,36,34},3);c.fill({cx-11,y+12,21,3});c.fill({cx-11,y+23,15,3});c.fill({cx-20,y+39,42,3});}
  else{c.border({cx-19,y+1,29,29},3);for(int j=0;j<14;++j)c.fill({cx+8+j,y+27+j,4,4});}
  c.text_px(x+(171-c.text_width_px(label,36))/2,1174,label,36);
 }
};
// Vocabulary home has its own hierarchy and a single fixed bottom action.
if(root_home&&std::any_of(w.blocks.begin(),w.blocks.end(),[](const auto&b){return b.role=="vocab-home-stage";})){
 answer_pages_=1;
 auto get=[&](const std::string&role)->WebBlock{for(const auto&b:w.blocks)if(b.role==role)return b;return {};};
 c.text_px(28,28,"小小单词卡",44);
 for(const auto &b:w.top)if(b.text=="设置")button(c,{536,12,124,96},Action::DeviceMenu,b.text,0,false,b.enabled);
 c.fill({24,128,636,2});
 int y=200;for(const auto &line:c.wrap_px(get("vocab-home-stage").text,628,52)){c.text_px(28,y,line,52);y+=64;}
 y=324;for(const auto &line:c.wrap_px(get("vocab-home-next").text,628,36)){c.text_px(28,y,line,36);y+=48;}
 c.text_px(28,482,"今日进度",40);
 c.text_px(28,554,get("vocab-home-today").text,40);
 c.text_px(28,620,get("vocab-home-due").text,36);
 c.fill({28,720,628,1});
 c.text_px(28,764,get("vocab-home-history").text,36);
 c.text_px(28,824,get("vocab-home-streak").text,36);
 if(!status.empty())c.text_px(28,896,c.ellipsize(status,628),36);
 for(const auto &b:w.blocks)if(b.kind=="button"&&b.area=="foot"){
  button(c,{24,980,636,100},Action::WebAction,"",b.action,b.primary,b.enabled);buttons_.back().label=b.text;
  c.text_px(342-c.text_width_px(b.text,44)/2,1002,b.text,44);
 }
 navBar(w);return;
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
