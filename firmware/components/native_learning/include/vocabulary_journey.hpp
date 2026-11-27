#pragma once
#include "native_json.hpp"
#include <set>
namespace wordcard {
inline bool vocabulary_journey_valid(const StudyJson &j) {
    if(j.is_null()) return true;
    if(!j.is_object() || num(j,"version")!=1 || !field(j,"stamps").is_array()) return false;
    std::set<std::string> ids;
    for(const auto &st:j["stamps"]) {
        if(!st.is_object() || str(st,"sessionId").empty() || !ids.insert(str(st,"sessionId")).second ||
           !field(st,"ids").is_array() || st["ids"].empty() || num(st,"finishedAt")<1704067200000LL || num(st,"finishedAt")>4102444800000LL) return false;
        for(const auto &id:st["ids"]) if(!id.is_string() || str(id).empty()) return false;
        for(const auto *key:{"answered","correct","wrong"}) if(!field(st,key).is_number_integer() || num(st,key)<0) return false;
        if(num(st,"answered")!=num(st,"correct")+num(st,"wrong")) return false;
    }
    return true;
}
inline StudyJson vocabulary_stamps(const StudyJson &state) {
    const auto &j=field(state,"journey");
    return vocabulary_journey_valid(j)&&field(j,"stamps").is_array()?j["stamps"]:StudyJson::array();
}
inline void vocabulary_award(StudyJson &s,const StudyJson &before,int64_t now,StudyJson &events) {
    const auto ss=field(s,"session"),old=field(before,"session");
    if(str(ss,"phase")!="summary" || (str(old,"phase")=="summary"&&field(old,"id")==field(ss,"id")) ||
       num(ss,"journeyAttempts",num(ss,"correct"))<=0) return;
    const auto j=field(s,"journey");
    if(!vocabulary_journey_valid(j)) {events.push_back(StudyJson{{"type","journey_unavailable"},{"sessionId",ss["id"]},{"at",now}});return;}
    auto next=j.is_null()?StudyJson{{"version",1},{"startedAt",now},{"stamps",StudyJson::array()}}:j;
    for(const auto &st:next["stamps"])if(field(st,"sessionId")==field(ss,"id"))return;
    auto st=StudyJson{{"sessionId",ss["id"]},{"finishedAt",now},{"ids",ss["ids"]},{"source",ss["source"]},{"mode",ss["mode"]},{"answered",ss["answered"]},{"correct",ss["correct"]},{"wrong",ss["wrong"]}};
    next["stamps"].push_back(st);s["journey"]=next;
    events.push_back(StudyJson{{"type","journey_stamp"},{"sessionId",ss["id"]},{"at",now}});
}
}
