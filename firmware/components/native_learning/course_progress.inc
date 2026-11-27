void NativeVocabulary::ensure_loaded(int64_t now){
    now_=now;
    if(!loaded_)load();
}

void NativeVocabulary::bind_course(const VJ &words,const std::string &edition){
    for(const auto &w:words){
        if(str(w,"source").rfind("course:",0)==0)courseLessonWords_.insert(str(w,"id"));
        const auto local=str(w,"id"),word=str(w,"word"),gloss=str(w,"meaning");
        const auto candidate=str(field(catalog_.metadata,"legacyMap"),local);
        const auto *entry=catalog_.entry(candidate);
        if(entry&&entry->word==word&&entry->meaning==gloss){
            courseSenses_[local]=candidate;
        }else{
            const auto id="course:"+edition+"/"+local+"/"+word+"/"+gloss;
            VJ separate=w;separate["id"]=id;separate["stage"]="course:"+edition;
            catalog_.restore_deck_word(separate);courseSenses_[local]=id;
        }
    }
}

void NativeVocabulary::project_course(VJ &course) const {
    if(!course.is_object())return;
    VJ meanings=VJ::object(),skills=VJ::object();
    for(const auto &pair:courseSenses_){
        const auto &local=pair.first,&id=pair.second;
        for(const auto &row:field(field(state_,"cards"),id).items()){
            if(row.key()!="meaning"&&row.key()!="recall")continue;
            const auto &c=row.value();
            const int stage=int(std::min<int64_t>(3,num(c,"successes")));
            const bool pending=flag(c,"pendingRelearn");
            skills[local][row.key()=="meaning"?"meaning":"spelling"]={{"stage",stage},{"due",num(c,"due")},
                {"last",num(c,"lastReviewAt")},{"independent",num(c,"successes")},{"errors",num(c,"lapses")},
                {"needsCorrection",pending},{"corrections",0},{"reviewPolicy",1}};
            if(row.key()=="meaning"&&courseLessonWords_.count(local))
                meanings[local]={{"stage",stage},{"due",num(c,"due")},{"success",num(c,"successes")}};
        }
    }
    course["memory"]=std::move(meanings);course["batchMemory"]=std::move(skills);
}

VJ NativeVocabulary::course_cursor() const {
    VJ run=field(state_,"courseCursor");
    project_course(run);
    return run;
}

bool NativeVocabulary::save_course(const VJ &value,const VJ &lessonTask){
    const auto &old=field(state_,"courseCursor"),&batch=field(value,"batch"),&prior=field(old,"batch");
    const auto &lesson=field(value,"session"),&priorLesson=field(old,"session");
    const bool checked=field(batch,"checked").is_object();
    const bool newQuestion=field(prior,"serial")!=field(batch,"serial")||field(prior,"at")!=field(batch,"at")||
                           !field(prior,"checked").is_object();
    VJ cursor=value;
    cursor.erase("memory");cursor.erase("batchMemory");
    VJ action={{"type","courseSave"},{"edition","course"},{"group",str(batch,"serial")},{"run",std::move(cursor)}};
    std::string local,skill;
    VJ result;
    bool retry=false;
    if(checked&&newQuestion){
        const auto &q=batch.at("queue").at(size_t(num(batch,"at")));
        local=str(q,"id");const auto kind=str(q,"kind");
        skill=kind=="spelling"?"recall":kind=="gap"?"partial":kind;
        result=batch.at("checked");retry=flag(q,"followup");
    }else if(str(lessonTask,"kind")=="meaning" && field(lesson,"checked").is_object() &&
             (!field(priorLesson,"checked").is_object()||field(lesson,"at")!=field(priorLesson,"at")||
              field(lesson,"lesson")!=field(priorLesson,"lesson"))){
        local=str(lessonTask,"wordId");skill="meaning";result=lesson.at("checked");
    }
    if(!local.empty()){
        const auto binding=courseSenses_.find(local);
        require(binding!=courseSenses_.end(),"课程词义未登记");
        action["answer"]={{"id",binding->second},{"skill",skill},{"correct",flag(result,"correct")},
                          {"assisted",flag(result,"assisted")},{"retry",retry}};
    }
    return commit(action);
}
