#pragma once
#include <algorithm>
#include "vocabulary.hpp"
namespace wordcard {
inline const char* shop_name(int group){return group==0?"一封寻书信":"雨后的书箱";}
inline int shop_scene(const VJ &s){const auto b=str(field(s,"bookshop"),"branch");return b=="reading"?1:b=="repair"?2:0;}
inline std::string shop_title(int scene){return scene==1?"窗边阅读角":scene==2?"旧书修复台":"旧书店开张";}
inline bool shop_visit(const VJ &s,int64_t now){
    const auto &b=field(s,"bookshop");
    return shop_scene(s)&&VocabularyEngine::date(now)>VocabularyEngine::date(num(b,"finished"))&&str(b,"visit")!=VocabularyEngine::date(now);
}
inline std::string shop_cue(const VocabularyCatalog &c,const std::string &id,int variant){
    const auto &a=field(c.shopCues,id);
    if(a.is_array()&&a.size()==3)return str(a[std::clamp(variant,0,2)]);
    const auto *e=c.entry(id);return e?"来信中要用一个表示“"+e->meaning+"”的英文词。":"";
}
}
