// Browsing and voluntary practice have their own navigation and never replace the saved study group.
void NativeVocabulary::load_history(){
    std::map<std::string,int64_t> learned;
    for(const auto &w:field(state_,"seen").items())learned[w.key()]=num(w.value());
    for(const auto &w:field(state_,"cards").items()){
        auto &at=learned[w.key()];
        for(const auto &skill:w.value())at=std::max(at,num(skill,"lastReviewAt"));
    }
    for(const auto &stamp:album_stamps())
        for(const auto &word:field(stamp,"ids")){
            auto &when=learned[str(word)];when=std::max(when,num(stamp,"finishedAt"));
        }
    historyIds_.clear();
    for(const auto &item:learned){
        const auto *word=catalog_.entry(item.first);
        if(!word||(currentHistory_&&!catalog_.contains_word(str(state_,"selectedStage"),item.first)))continue;
        if(!historyFilter_.empty()&&lower(word->word).find(historyFilter_)==std::string::npos)continue;
        historyIds_.push_back(item.first);
    }
    std::stable_sort(historyIds_.begin(),historyIds_.end(),[&](const auto &a,const auto &b){
        if(learned.at(a)!=learned.at(b))return learned.at(a)>learned.at(b);
        return catalog_.entry(a)->word<catalog_.entry(b)->word;
    });
    historyPage_=std::min(historyPage_,historyIds_.empty()?size_t(0):(historyIds_.size()-1)/5);
}
void NativeVocabulary::load_history_groups(){
    historyGroups_=VJ::array();std::set<std::string> sessions;
    const std::set<std::string> available(historyIds_.begin(),historyIds_.end());
    auto collect=[&](const VJ &group,const std::string &id,int64_t when){
        if(id.empty()||sessions.count(id))return;
        VJ words=VJ::array();
        for(const auto &w:field(group,"ids"))if(available.count(str(w)))addUnique(words,w);
        if(words.empty())return;
        sessions.insert(id);historyGroups_.push_back(VJ{{"ids",std::move(words)},{"at",when}});
    };
    for(const auto &stamp:album_stamps())collect(stamp,str(stamp,"sessionId"),num(stamp,"finishedAt"));
    const auto &active=field(state_,"session");collect(active,str(active,"id"),num(active,"startedAt"));
    for(const auto &g:field(state_,"paused"))collect(g,str(g,"id"),num(g,"startedAt"));
    std::stable_sort(historyGroups_.begin(),historyGroups_.end(),[](const VJ &x,const VJ &y){return num(x,"at")>num(y,"at");});
}
std::vector<std::string> NativeVocabulary::stamp_ids() const {
    for(const auto &stamp:album_stamps())if(str(stamp,"sessionId")==journeyStamp_){
        std::vector<std::string> ids;
        for(const auto &word:field(stamp,"ids"))if(catalog_.entry(str(word)))ids.push_back(str(word));
        return ids;
    }
    return {};
}
bool NativeVocabulary::study_action(const VJ &a){
    if(route_!="history-quiz")return commit(a);
    VJ events;
    practice_=engine_.reduce(practice_,a,now_,events);
    if(str(a,"type")=="answer"||str(a,"type")=="continue")stop_audio();
    ++viewSerial_;return true;
}

