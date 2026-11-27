// The chapter shares the established session, journal and FSRS channels.
if(type=="shopBegin"){
    require(catalog.shopCues.is_object(),"书店资源尚未就绪");
    const auto saved=resumable(s,"bookshop");
    if(!saved.is_null()){
        if(field(saved,"id")==field(ss,"id"))return s;
        activate_session(s,saved);
    }else{
        const auto b=field(s,"bookshop");const int group=int(num(b,"groups"));
        const bool visit=shop_visit(s,now);auto due=shop_due(s,now);
        if(due.size()>10)due.erase(due.begin()+10,due.end());
        VJ ids=VJ::array();const auto mode=str(s["settings"],"mode");
        if(!due.empty()){
            for(const auto &q:due)addUnique(ids,q["id"]);
            auto next=session(s,ids,"review",now,mode,true);
            next["shopRole"]=visit?"visit":"review";
            for(auto q:due){
                q["uid"]=str(next,"id")+":"+std::to_string(next["tasks"].size());
                q["done"]=q["followup"]=false;q["requiresGap"]=true;next["tasks"].push_back(q);
            }
            activate_session(s,next);advance(s,now,events);
        }else{
            require(group>=0&&group<2,"这两件委托已经办妥");
            auto plan=field(b,"ids");
            if(!plan.is_array()){
                plan=VJ::array();const auto reserved=reserved_words(s);
                // Prefer authored situations and new words; never take another unfinished group's words.
                for(bool seen:{false,true})for(bool authored:{true,false})for(const auto &e:catalog.entries){
                    if(plan.size()==10)break;
                    if(e.stage=="gre-trial"&&e.eligible&&!reserved.count(e.id)&&s["seen"].contains(e.id)==seen&&catalog.shopCues.contains(e.id)==authored)plan.push_back(e.id);
                }
                require(plan.size()==10,"可用词不足，请先完成已暂停的词组");
            }
            for(size_t i=group*5;i<group*5+5;++i)ids.push_back(plan.at(i));
            auto next=session(s,ids,"new",now,mode,true);next["shopRole"]="mission";next["shopGroup"]=group;
            if(mode!="meaning"){
                auto &tasks=next["tasks"];tasks.erase(std::remove_if(tasks.begin(),tasks.end(),[&](const VJ &t){return str(t,"kind")!=(mode=="partial"?"partial":"recall");}),tasks.end());
            }
            activate_session(s,next);
            s["bookshop"]["ids"]=plan;
        }
    }
    events.push_back(VJ{{"type","shop_begin"},{"at",now},{"sessionId",field(s["session"],"id")}});
}else if(type=="shopDecor"){
    const auto branch=str(a,"value");require(num(field(s,"bookshop"),"groups")==2&&(branch=="reading"||branch=="repair"),"请先完成两件委托");
    if(!shop_scene(s))s["bookshop"]["branch"]=branch;
}else if(type=="shopPostcard"){
    if(shop_visit(s,now))s["bookshop"]["visit"]=date(now);
}
