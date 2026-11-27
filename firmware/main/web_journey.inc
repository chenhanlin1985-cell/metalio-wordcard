// The collection and home strip share the six reward illustrations, in award order.
if(vocabulary&&w.route=="collection"&&std::any_of(w.blocks.begin(),w.blocks.end(),[](const auto&b){return b.role=="vocab-album-title";})){
 answer_pages_=1;
 const auto backLabel=w.top.empty()?std::string("回首页"):w.top.front().text;
 pageButton({24,12,184,96},Action::Back,backLabel);
 folio_line(c,642,58,"我的收集册",136,Canvas::Align::Right);c.fill({42,110,600,2});
 std::vector<const WebBlock*> slots,numbers;
 for(const auto&b:w.blocks){
  if(b.role=="vocab-album-title")folio_line(c,42,175,b.text,44);
  if(b.role=="vocab-album-stamp")slots.push_back(&b);
  if(b.role=="vocab-album-number")numbers.push_back(&b);
  if(b.role=="vocab-album-gallery"){pageButton({42,273,600,64},Action::WebAction,b.text,b.action,false,b.enabled);}
 }
 folio_line(c,42,234,"印章记录完成，不代表已经掌握",28);
 for(size_t i=0;i<slots.size()&&i<6&&i<numbers.size();++i){
  const auto&b=*slots[i];int x=24+int(i%2)*324,y=350+int(i/2)*200,cx=x+156;
  if(b.action>=0&&b.enabled)buttons_.push_back({{x,y,312,190},{Action::WebAction,b.action},b.text,true,false});
  folio_stamp(c,cx,y+49,i,numbers[i]->text,b.selected,true);
  if(b.selected){
   bookLine(cx,y+129,numbers[i]->detail,298);
   folio_line(c,cx,y+177,b.detail,28,Canvas::Align::Center);
  }else folio_line(c,cx,y+129,"待收集",28,Canvas::Align::Center);
 }
 for(const auto&b:w.blocks)if(b.kind=="button"&&b.area=="foot"){
  if(b.primary){pageButton({42,1080,600,100},Action::WebAction,b.text,b.action,true,b.enabled);}
  else if(b.text=="上一页")pageButton({24,968,220,76},Action::WebAction,b.text,b.action,false,b.enabled);
  else if(b.text=="下一页")pageButton({440,968,220,76},Action::WebAction,b.text,b.action,false,b.enabled);
 }
 if(!status.empty())c.text_px(28,1042,c.ellipsize(status,628),30);
 return;
}
