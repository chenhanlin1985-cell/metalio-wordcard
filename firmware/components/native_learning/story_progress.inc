// Book position and its optional scored answer are committed by one journal append.
VJ NativeVocabulary::story_progress(const VJ &request) {
    require(loaded_&&!storageBlocked_,"学习记录未就绪，请检查存储后重试");
    catalog_.open_story(str(request,"book"),str(request,"digest"),str(request,"deck"),str(request,"deckDigest"));
    const auto &binding=catalog_.story;
    const auto &deckSenses=catalog_.decks.at(str(request,"deck")).senses;
    auto resolve=[&](const std::string &local)->const std::string*{
        auto own=binding.senses.find(local);
        if(own!=binding.senses.end())return &own->second;
        if(!binding.uses_deck)return nullptr;
        auto shared=deckSenses.find(local);
        return shared==deckSenses.end()?nullptr:&shared->second;
    };
    if(str(request,"op")=="save"){
        const auto &run=field(request,"run");
        require(run.is_object()&&!run.contains("cards")&&!run.contains("recent")&&!run.contains("practised"),"故事进度格式无效");
        VJ action={{"type","storySave"},{"edition",binding.edition},{"rev",request.at("rev")},{"run",run}};
        const auto &answer=field(request,"answer");
        if(!answer.is_null()){
            require(binding.grades_words,"阅读理解题单独计分");
            const auto *id=resolve(str(answer,"local"));
            require(id,"故事词义未登记");
            action["answer"]=answer;action["answer"]["id"]=*id;
        }
        VJ stamps=VJ::array();
        for(const auto &stamp:field(run,"stamps")){
            VJ ids=VJ::array();
            for(const auto &local:field(stamp,"sourceIds")){
                if(const auto *id=resolve(str(local)))addUnique(ids,*id);
            }
            stamps.push_back({{"sessionId","story/"+binding.edition+"/"+str(stamp,"key")},{"kind",str(stamp,"kind")},
                {"finishedAt",num(stamp,"at")},{"contentTitle",str(stamp,"title")},{"words",field(stamp,"words")},{"ids",ids}});
        }
        const auto exposed=str(request,"exposed");
        if(!exposed.empty()){
            const auto *id=resolve(exposed);
            require(id,"故事词条未登记");
            action["exposed"]=*id;
        }
        action["stamps"]=std::move(stamps);
        if(!commit(action))throw std::runtime_error(error_);
    }else require(str(request,"op")=="read","未知故事操作");
    const auto &record=field(field(state_,"storyRuns"),binding.edition);
    VJ cards=VJ::object(),seen=VJ::object(),recent=VJ::array();
    std::map<std::string,std::string> reverse;
    const auto &history=field(state_,"recentExposures");
    for(const auto &id:history)reverse.emplace(str(id),"");
    auto project=[&](const std::string &local,const std::string &id){
        auto h=reverse.find(id);
        if(h!=reverse.end())h->second=local;
        if(field(state_,"seen").contains(id))seen[local]=true;
        for(const auto &skill:field(field(state_,"cards"),id).items()){
            const auto &c=skill.value();
            cards[local][skill.key()]={{"senseId",id},{"due",num(c,"due")},{"target",num(c,"fsrsDue")},
                {"last",num(c,"lastReviewAt")},{"attempts",num(c,"reps")},{"pending",flag(c,"pendingRelearn")}};
        }
    };
    for(const auto &pair:binding.senses)project(pair.first,pair.second);
    if(binding.uses_deck)for(const auto &pair:deckSenses)
        if(!binding.senses.count(pair.first))project(pair.first,pair.second);
    std::set<std::string> listed;
    for(auto i=history.rbegin();i!=history.rend()&&recent.size()<60;++i){
        const auto id=str(*i);auto it=reverse.find(id);
        if(it!=reverse.end()&&!it->second.empty()&&listed.insert(id).second)
            recent.push_back(VJ::array({it->second,num(field(field(state_,"exposures"),id),"at")}));
    }
    std::reverse(recent.begin(),recent.end());
    return {{"rev",num(record,"rev")},{"run",field(record,"run")},{"cards",cards},{"practised",seen},{"recent",recent}};
}
