#pragma once
#include "learning_web.hpp"
namespace wordcard {
inline bool web_prompt_action_allowed(const WebPage&p,int index){
 if(!p.keyboard||index<0||size_t(index)>=p.actions.size())return false;
 const auto&s=p.actions[index];return s.find("\"action\":\"w-audio\"")!=std::string::npos||s.find("\"action\":\"audio\"")!=std::string::npos||s.find("\"action\":\"w-hint\"")!=std::string::npos||s.find("\"action\":\"v-audio\"")!=std::string::npos||(s.find("\"action\":\"v-do\"")!=std::string::npos&&s.find("\"type\":\"hint\"")!=std::string::npos);
}
inline WebPage web_disclosure(const WebPage&p){
 WebPage w=p;w.keyboard=false;w.title=p.keyboard?"完整题干与提示":"学习说明";w.tabs.clear();w.top.clear();w.blocks.clear();
 for(auto b:p.blocks){if(p.keyboard){if(b.kind=="button"&&!web_prompt_action_allowed(p,b.action))continue;}else if(b.role!="detail")continue;b.area="body";w.blocks.push_back(b);}return w;
}
}
