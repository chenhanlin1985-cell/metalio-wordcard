if(name=="v-history"){
    currentHistory_=str(a,"scope")=="current";
    historyFilter_.clear();historyPage_=0;load_history();navigate("history");return true;
}
if(name=="v-history-page"&&route_=="history"){
    const auto page=num(a,"page");
    if(page>=0&&size_t(page)*5<historyIds_.size()){historyPage_=size_t(page);++viewSerial_;}
    return true;
}
if(name=="v-history-scope"&&route_=="history"){
    currentHistory_=!currentHistory_;historyPage_=0;load_history();++viewSerial_;return true;
}
if(name=="v-history-groups"&&route_=="history"){
    groupPage_=0;load_history_groups();navigate("history-groups");return true;
}
if(name=="v-group-page"&&route_=="history-groups"){
    const auto page=num(a,"page");if(page>=0&&size_t(page)*3<historyGroups_.size()){groupPage_=size_t(page);++viewSerial_;}return true;
}
if(name=="v-learned-group"&&route_=="history-groups"){
    const auto index=num(a,"index",-1);if(index<0||size_t(index)>=historyGroups_.size())return false;
    browseIds_.clear();for(const auto &word:historyGroups_[size_t(index)]["ids"])browseIds_.push_back(str(word));
    browseAt_=0;navigate("history-card");lookup_=browseIds_.front();return true;
}
if(name=="v-history-find"&&route_=="history"){
    historyInput_=historyFilter_;navigate("history-search");return true;
}
if(name=="v-history-clear"&&route_=="history"){
    historyFilter_.clear();historyPage_=0;load_history();++viewSerial_;return true;
}
if(name=="v-browse"){
    if(route_=="history")browseIds_=historyIds_;
    else if(route_=="stamp")browseIds_=stamp_ids();
    else if(route_!="history-card")return false;
    const auto at=std::find(browseIds_.begin(),browseIds_.end(),value);
    if(at==browseIds_.end())return false;
    browseAt_=size_t(at-browseIds_.begin());navigate("history-card");lookup_=value;
    if(!backStack_.empty()&&backStack_.back().route=="history")historyPage_=browseAt_/5;
    return true;
}
if(name=="v-practice"){
    VJ ids=VJ::array();
    if(route_=="history-card"&&lookup_==value)ids.push_back(value);
    else if(route_=="history-card"&&flag(a,"group")&&!backStack_.empty()&&backStack_.back().route=="history-groups"){
        const size_t start=browseAt_/5*5;
        for(size_t i=start;i<std::min(start+5,browseIds_.size());++i)ids.push_back(browseIds_[i]);
    }
    else if(route_=="history"){
        const auto first=historyPage_*5;
        for(size_t i=first;i<std::min(first+5,historyIds_.size());++i)ids.push_back(historyIds_[i]);
    }else if(route_=="stamp")for(const auto &id:stamp_ids()){
        if(ids.size()==5)break;
        ids.push_back(id);
    }
    if(ids.empty())return false;
    practice_=engine_.practice(ids,str(state_["settings"],"mode"),now_);
    shift_=symbols_=false;navigate("history-quiz");return true;
}

