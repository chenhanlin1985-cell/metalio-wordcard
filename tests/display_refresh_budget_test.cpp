#include "web_refresh.hpp"
#include <stdexcept>
#include <iostream>
#include <string>
using wordcard::DisplayRefreshBudget;
int main(){try{unsigned checks=0;auto check=[&](bool x){++checks;if(!x)throw std::runtime_error("refresh budget");};
 DisplayRefreshBudget b;b.completed(true,true,1000);
 // Every sixth same-question repaint is full, even without a route change.
 for(unsigned i=1;i<=60;++i){const bool gc=b.due(false,1000+i);check(gc==(i%6==0));check(b.allow_fast(true,gc)==!gc);b.completed(gc,true,1000+i);check(b.partial==i%6);}
 // Typing can retain fast feedback for 31 updates, never indefinitely.
 b.completed(true,true,2000);
 for(unsigned i=1;i<=64;++i){const bool gc=b.due(true,2000+i);check(gc==(i%32==0));check(b.allow_fast(true,gc)==!gc);b.completed(gc,true,2000+i);check(b.partial==i%32);}
 for(unsigned i=0;i<8;++i)b.completed(false,true,3000+i);
 check(b.due(false,3010));check(!b.due(true,3010));
 const auto debt=b.partial;b.completed(true,false,3011);check(b.partial==debt&&b.due(false,3012));
 b.completed(true,true,3013);check(!b.due(false,3014)&&b.partial==0);
 b.completed(false,true,3014);check(!b.due(false,123012));check(b.due(false,123013));check(!b.due(true,123013));
 b.completed(true,true,123014);check(!b.due(false,999999)); // Static screen does not accrue debt.
 using wordcard::study_refresh_reason;using wordcard::StudyRefreshReason;
 const std::string q="word-batch|3|check|3|2|false|||||false|tail/0";
 const std::string feedback="word-batch|3|check|3|2|true|||||false|tail/0";
 const std::string next="word-batch|3|check|3|3|false|||||false|tail/0";
 check(study_refresh_reason(q,q,20)==StudyRefreshReason::None);
 check(study_refresh_reason(q,feedback,2)==StudyRefreshReason::None);
 check(study_refresh_reason(feedback,next,2)==StudyRefreshReason::Question);
 check(study_refresh_reason(feedback,next,0)==StudyRefreshReason::None);
 check(study_refresh_reason("word-batch|3|learn|0|0|false|||||false|tail/0","word-batch|3|learn|1|0|false|||||false|tail/0",1)==StudyRefreshReason::Question);
 const std::string listen="quiz||||||u1a|0|0|0|false|tail/0";
 check(study_refresh_reason(listen,"quiz||||||u1a|0|0|0|true|tail/0",2)==StudyRefreshReason::None);
 check(study_refresh_reason(listen,"quiz||||||u1a|1|0|0|false|tail/0",2)==StudyRefreshReason::Question);
 check(study_refresh_reason(listen,"quiz||||||u1a|0|1|1|false|tail/0",2)==StudyRefreshReason::None);
 check(study_refresh_reason(listen,"focus-summary||||||||||false|tail/0",0)==StudyRefreshReason::Summary);
 check(study_refresh_reason(q,"word-result||||||||||false|tail/0",0)==StudyRefreshReason::Summary);
 check(study_refresh_reason(listen,"summary||||||||||false|tail/0",0)==StudyRefreshReason::Summary);
 check(study_refresh_reason(q,"home||||||||||false|tail/0",1)==StudyRefreshReason::Question);
 check(study_refresh_reason("books||||||||||false|tail/0","unit||||||||||false|tail/0",5)==StudyRefreshReason::None);
 // Keyboard geometry alone must not grant the 32-update allowance to a
 // changed candidate list or a different question; only the input-only diff.
 b.completed(true,true,123015);for(unsigned i=0;i<5;++i)b.completed(false,true,123016+i);
 check(!b.due(true,123021));check(b.due(false,123021));
 std::cout<<"PASS: "<<checks<<" refresh budget checks: same-page cycles, DU priority, input limit, age, failed refresh debt and actual GC reset\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
