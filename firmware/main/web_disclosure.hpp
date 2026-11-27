#pragma once
#include "web_word_card.hpp"
namespace wordcard {
inline bool web_prompt_action_allowed(const WebPage&p,int index){
 if(!p.keyboard||index<0||size_t(index)>=p.actions.size())return false;
 const auto&s=p.actions[index];return s.find("\"action\":\"w-audio\"")!=std::string::npos||s.find("\"action\":\"audio\"")!=std::string::npos||s.find("\"action\":\"w-hint\"")!=std::string::npos||s.find("\"action\":\"v-audio\"")!=std::string::npos||(s.find("\"action\":\"v-do\"")!=std::string::npos&&s.find("\"type\":\"hint\"")!=std::string::npos);
}
inline WebPage web_disclosure(const WebPage&p, int example=-1){
 if(const auto *sample=card_example(p,example)){
  WebPage w;
  w.key=p.key;w.route="card-listening";w.title="听例句";w.speech_note=p.speech_note;
  for(int i=example-1;i>=0;--i){
   const auto &b=p.blocks[size_t(i)];
   if(b.role=="card-word"||b.role=="word"){w.blocks.push_back(card_line(b.text,"word"));break;}
  }
  w.blocks.push_back(card_line(sample->text,"example-en"));
  if(!sample->detail.empty())w.blocks.push_back(card_line(sample->detail,"reader-copy"));
  w.blocks.push_back(example_control(size_t(example),"foot",true));
  return w;
 }
 WebPage w=p;if(!p.keyboard && word_card_head(p)){layout_word_card(w,true);return w;}w.keyboard=false;w.title=p.keyboard?"完整题干与提示":"学习说明";w.tabs.clear();w.top.clear();w.blocks.clear();
 for(auto b:p.blocks){if(p.keyboard){if(b.kind=="button"&&!web_prompt_action_allowed(p,b.action))continue;}else if(b.role!="detail")continue;b.area="body";w.blocks.push_back(b);}return w;
}
}
