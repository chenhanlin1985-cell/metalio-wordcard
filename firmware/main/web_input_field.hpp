#pragma once
#include "wordcard_ui.hpp"
#include <algorithm>
namespace wordcard {
inline void dictionary_input_field(Canvas&c,const View&v){
 c.fill({12,124,660,132},false);c.border({12,124,660,132},3);
 std::string visible=v.dictionary_query;bool clipped=false;
 while(c.text_width((clipped?"…":"")+visible,2)>620&&!visible.empty()){visible.erase(0,1);clipped=true;}
 if(visible.empty())c.text_px(32,172,"输入英文单词或短语",36);else c.text(32,146,(clipped?"…":"")+visible,2);
 for(int y=124;y<256;++y)for(int x=12;x<672;++x)c.pixel(x,y,c.shade(x,y)<8);
}
// Paint only public question text and the learner's input. The answer is never
// part of this projection. An underline remains under every editable letter.
inline void web_input_field(Canvas&c,const WebPage&w){
 if(w.gap_mask.empty()){
  c.fill({12,124,660,132},false);c.border({12,124,660,132},3);
  std::string visible=w.typed;bool clipped=false;
  while(c.text_width((clipped?"…":"")+visible,2)>620&&!visible.empty()){visible.erase(0,1);clipped=true;}
  if(visible.empty())c.text_px(32,172,"输入英文",36);else c.text(32,146,(clipped?"…":"")+visible,2);
 }else{
  c.fill({12,124,660,190},false);
  const size_t count=std::count(w.gap_mask.begin(),w.gap_mask.end(),'_');
  c.text_px(24,126,w.typed.size()>count?"多输入了 "+std::to_string(w.typed.size()-count)+" 个字符，请退格":"补齐 "+std::to_string(count)+" 个字母 · 按空缺顺序输入",36);
  // A fixed cell width at each blank prevents the word jumping as it fills.
  auto width=[&](char ch,int px){return ch=='_'?px*4/5:c.text_width_px(std::string(1,ch),px);};
  int px=60;for(;px>36;px-=2){int rows=1,x=0;for(char ch:w.gap_mask){const int ww=width(ch,px);if(x+ww>636){++rows;x=0;}x+=ww;}if(rows<=2)break;}
  int x=24,y=174;size_t at=0;
  for(char ch:w.gap_mask){const int ww=width(ch,px);if(x+ww>660){x=24;y+=68;}
   if(y>242)break;
   if(ch=='_'){
    if(at<w.typed.size()){const auto letter=w.typed.substr(at,1);c.text_px(x+(ww-c.text_width_px(letter,px))/2,y,letter,px);}
    c.fill({x+3,y+px+5,ww-6,3});
    if(at==w.typed.size())c.border({x+1,y-2,ww-2,px+14},2);
    ++at;
   }else c.text_px(x,y,std::string(1,ch),px);
   x+=ww;
  }
 }
 for(int y=124;y<314;++y)for(int x=12;x<672;++x)c.pixel(x,y,c.shade(x,y)<8);
}
}
