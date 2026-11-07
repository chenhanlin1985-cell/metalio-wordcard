#pragma once
#include "learning_web.hpp"
namespace wordcard {
inline bool web_compact_study_keyboard(const WebPage&w){return w.keyboard&&w.route=="word-batch"&&w.key.rfind("vocabulary:",0)==0;}
inline bool web_native_search_keyboard(const WebPage&w){return w.keyboard&&w.route=="search"&&w.key.rfind("vocabulary:",0)==0;}
inline std::vector<const WebBlock*> web_keyboard_controls(const WebPage&w){std::vector<const WebBlock*> out;for(const auto&b:w.blocks)if(b.kind=="button"&&b.area!="foot"&&out.size()<(web_compact_study_keyboard(w)?1u:w.route=="word-batch"?2u:3u))out.push_back(&b);return out;}
inline bool web_long_prompt(const WebPage&w){for(const auto&b:w.blocks)if(b.kind=="heading"){unsigned chars=0;for(unsigned char c:b.text)if((c&0xc0)!=0x80)++chars;if(chars>40)return true;}return false;}
struct WebKeyboardState {bool ready=false,recall=false,compactStudy=false,nativeSearch=false;int count=0;unsigned enabled=0;};
inline WebKeyboardState web_keyboard_state(const WebPage&w){WebKeyboardState s;s.ready=w.can_submit;s.recall=w.route=="word-batch";s.compactStudy=web_compact_study_keyboard(w);s.nativeSearch=web_native_search_keyboard(w);const auto a=web_keyboard_controls(w);s.count=int(a.size());for(size_t i=0;i<a.size();++i)if(a[i]->enabled||(i==1&&web_long_prompt(w)))s.enabled|=1u<<i;return s;}
}
