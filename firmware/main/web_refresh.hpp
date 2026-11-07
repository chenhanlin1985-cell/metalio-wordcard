#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <string_view>
namespace wordcard {
// A question's identity excludes selection, feedback and typed letters. A
// changed identity replaces the text body; don't carry dirty ink across it.
enum class StudyRefreshReason { None, Question, Summary };
inline StudyRefreshReason study_refresh_reason(std::string_view before,std::string_view after,unsigned debt){
 if(before==after)return StudyRefreshReason::None;
 auto parts=[](std::string_view key){std::array<std::string_view,11> out{};for(auto &p:out){const auto n=key.find('|');p=key.substr(0,n);if(n==std::string_view::npos)break;key.remove_prefix(n+1);}return out;};
 const auto old=parts(before),now=parts(after);
 const auto summary=[](std::string_view r){return r=="word-result"||r=="focus-summary"||r=="summary";};
 if(summary(now[0]))return StudyRefreshReason::Summary;
 if(!debt)return StudyRefreshReason::None;
 const auto study=[&](std::string_view r){return r=="word-batch"||r=="quiz"||summary(r);};
 if(now[0]=="home"&&study(old[0]))return StudyRefreshReason::Question;
 if(now[0]=="word-batch"){
  if(old[0]!=now[0])return StudyRefreshReason::Question;
  for(size_t i=1;i<=4;++i)if(old[i]!=now[i])return StudyRefreshReason::Question;
 }
 if(now[0]=="quiz"&&(old[0]!=now[0]||old[6]!=now[6]||old[7]!=now[7]))return StudyRefreshReason::Question;
 return StudyRefreshReason::None;
}
// Count physical updates, not route/page identities. A required GC16 must
// take precedence over both DU paths and is credited only after success.
struct DisplayRefreshBudget {
 static constexpr unsigned content_limit=6,input_limit=32;
 static constexpr uint64_t max_active_age_ms=120000;
 unsigned partial=0;
 uint64_t last_quality_ms=0;
 bool due(bool typing,uint64_t now_ms)const {
  return partial+1>=input_limit || (!typing &&
   (partial+1>=content_limit || (partial>0&&now_ms-last_quality_ms>=max_active_age_ms)));
 }
 bool allow_fast(bool requested,bool quality_required)const{return requested&&!quality_required;}
 void completed(bool quality,bool success,uint64_t now_ms){
  if(!success)return;
  if(quality){partial=0;last_quality_ms=now_ms;}
  else if(partial<input_limit)++partial;
 }
};
// Existing DU is suitable for small binary outline changes only. Never send a
// gray edge, text replacement or a large repaint through this selection path.
inline bool web_binary_selection_safe(const uint8_t*front,const uint8_t*back,size_t bytes){
 if(!front||!back||bytes!=1216u*684u/2u)return false;
 size_t changed=0;const size_t limit=bytes*2/50;
 for(size_t i=0;i<bytes;++i){if(front[i]==back[i])continue;for(int shift=0;shift<8;shift+=4){const auto a=(front[i]>>shift)&15,b=(back[i]>>shift)&15;if(a==b)continue;if((a!=0&&a!=15)||(b!=0&&b!=15)||++changed>limit)return false;}}
 return changed>0;
}
}
