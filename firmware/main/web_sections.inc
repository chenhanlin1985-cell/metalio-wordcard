// Root destinations share Home's type, navigation and one-screen hierarchy.
if(v.screen!=Screen::WebPrompt&&std::any_of(w.blocks.begin(),w.blocks.end(),[](const auto&b){return b.role=="section-heading";})){
 answer_pages_=1;
 auto control=[&](Rect r,const WebBlock&b,int px=38){
  const Action a=b.role=="device-dictionary"?Action::Dictionary:b.role=="device-notebook"?Action::Vocabulary:Action::WebAction;
  button(c,r,a,"",b.action,b.primary,b.enabled);buttons_.back().label=b.text;
  c.text_px(r.x+22,r.y+16,b.text,px);
  if(!b.detail.empty())c.text_px(r.x+22,r.y+66,b.detail,36);
 };
 const bool home_options=w.route=="home-options";
 if(home_options)button(c,{20,12,124,96},Action::Back,"返回");
 c.text_px(home_options?180:28,28,w.title,44);
 if(!home_options)button(c,{352,12,124,96},Action::WebPage,"说明",0);
 button(c,{492,12,168,96},(v.more_learning_scope||w.secondary)?Action::MoreLearning:Action::DeviceMenu,(v.more_learning_scope||w.secondary)?"更多学习":"设置");c.fill({24,128,636,2});
 int y=144;
 for(const auto&b:w.blocks){
  if(b.role=="detail")continue;
  if(b.kind=="button"){
   const bool link=b.role=="section-link";const int h=link?88:120;
   control({24,y,636,h},b,link?36:38);y+=h+16;
  }else{
   const int px=b.role=="section-heading"?44:36,lh=px+12;
   for(const auto&line:c.wrap_px(b.text,628,px)){c.text_px(28,y,line,px);y+=lh;}
   y+=12;
  }
 }
 if(!status.empty()){c.fill({24,144,636,154},false);int sy=150;for(const auto&line:c.wrap_px(status,628,36)){if(sy>246)break;c.text_px(28,sy,line,36);sy+=48;}}
 navBar(w);return;
}
