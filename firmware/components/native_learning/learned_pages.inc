if(route_=="history-groups"){
    p.title="学过的词组";p.key+=":"+n(groupPage_);v.top("返回已学单词",VJ{{"action","v-back"}});
    if(historyGroups_.empty())v.text("这段记录只保留了单词，可返回已学单词逐个查看。","meaning");
    const auto end=std::min(groupPage_*3+3,historyGroups_.size());
    for(size_t i=groupPage_*3;i<end;++i){
        const auto &g=historyGroups_[i],&ids=g.at("ids");std::string label;
        for(size_t j=0;j<std::min(size_t(3),ids.size());++j){if(j)label+=" · ";label+=catalog_.entry(str(ids[j]))->word;}
        if(ids.size()>3)label+=" …";
        const auto day=num(g,"at")?VocabularyEngine::date(num(g,"at"))+" · ":"";
        v.button(label,VJ{{"action","v-learned-group"},{"index",i}},false,"body",day+n(ids.size())+" 个词");
    }
    if(groupPage_)v.button("上一页",VJ{{"action","v-group-page"},{"page",groupPage_-1}},false,"foot");
    if(end<historyGroups_.size())v.button("下一页",VJ{{"action","v-group-page"},{"page",groupPage_+1}},false,"foot");
    return p;
}
if(route_=="history-search"){
    p.title="查找已学单词";p.keyboard=true;p.typed=historyInput_;p.can_submit=true;
    v.top("返回已学单词",VJ{{"action","v-back"}});v.text("输入单词或其中几个字母","meaning");return p;
}
if(route_=="history"){
    p.title="已学单词";p.key+=":"+n(historyPage_)+":"+historyFilter_+(currentHistory_?":current":":all");
    v.top(back_label(),VJ{{"action","v-back"}});v.top("查找",VJ{{"action","v-history-find"}});
    const auto total=historyIds_.size(),first=historyPage_*5,last=std::min(first+5,total);
    v.text((currentHistory_?title:"全部词库")+" · "+n(total)+" 个词","history-scope");
    v.button(currentHistory_?"查看全部词库":"只看当前词库",VJ{{"action","v-history-scope"}});
    if(total&&historyFilter_.empty())v.button("按词组找",VJ{{"action","v-history-groups"}});
    if(!historyFilter_.empty())v.button("清除搜索",VJ{{"action","v-history-clear"}},false,"body",historyFilter_);
    if(!total)v.text(!historyFilter_.empty()?"没有找到这个词，试试更短的拼写。":currentHistory_?"这个词库还没有已学单词。":"学过的词会出现在这里。","meaning");
    for(size_t i=first;i<last;++i){
        const auto *entry=catalog_.entry(historyIds_[i]);
        v.button(entry->word,VJ{{"action","v-browse"},{"value",entry->id}},false,"body",entry->pos+" "+entry->meaning);p.blocks.back().role="history-entry";
    }
    if(total){v.button("重练本页 "+n(last-first)+" 词",VJ{{"action","v-practice"}});p.blocks.back().role="history-practice";}
    if(total>5)v.text(n(historyPage_+1)+" / "+n((total+4)/5),"history-count");
    if(first)v.button("上一页",VJ{{"action","v-history-page"},{"page",historyPage_-1}},false,"foot");
    if(last<total)v.button("下一页",VJ{{"action","v-history-page"},{"page",historyPage_+1}},false,"foot");
    return p;
}

