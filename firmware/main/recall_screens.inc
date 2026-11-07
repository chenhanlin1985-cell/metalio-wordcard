void UI::recall_pages(Canvas &c,const View &v) {
    auto text=[&](int y,const std::string&s,int scale=1,int limit=3){
        const auto lines=c.wrap(s,604,scale);
        for(int i=0;i<std::min(limit,int(lines.size()));++i)c.text(40,y+i*(scale==2?74:43),lines[i],scale);
    };
    const auto&s=v.recall;
    if(v.screen==Screen::RecallHome){
        button(c,{40,16,136,96},Action::Back,"返回");strong(c,210,43,"今日复习");
        if(!v.date_valid)button(c,{40,132,604,88},Action::Date,"校准日期，安排跨天复习");
        else text(154,v.date_label+" · 到期优先，每次一个小节");
        text(235,v.notice.empty()?"课程、已学词书和生词，集中练习。":v.notice,1,2);
        const auto&n=v.recall_counts;
        const std::array<std::string,6>labels={
            "开始复习 · "+std::to_string(n.due)+" 待复习 / "+std::to_string(n.fresh)+" 新词",
            "错题与难词 · "+std::to_string(n.mistakes)+" 项",
            "补上听力 · "+std::to_string(n.listening)+" 项",
            "看词义，练拼写","看中文，选英文","听发音，写单词"};
        for(int i=0;i<6;++i)button(c,{40,355+i*108,604,88},Action::RecallBegin,labels[i],i,i==0);
        if(!s.queue.empty())button(c,{40,1040,604,96},Action::RecallResume,"继续未完成的复习",0,true);
        else button(c,{40,1040,604,96},Action::Home,"回到课程学习");
        return;
    }
    if(v.screen==Screen::RecallDone){
        strong(c,40,110,"这一轮复习完成了");
        text(265,"练对 "+std::to_string(s.solved)+" / "+std::to_string(s.total)+" 项");
        text(365,"本轮答错 "+std::to_string(s.errors)+" 次 · 暂缓 "+std::to_string(s.skipped)+" 项");
        text(500,"错过的内容会保留，隔天再练对后逐步延长间隔。",1,3);
        text(700,v.writable?"进度已保存，下次可以接着练。":"保存未成功，请保持开机并检查 SD。",1,3);
        button(c,{40,925,604,96},Action::RecallHome,"查看复习安排",0,true);
        button(c,{40,1060,604,96},Action::Home,"继续课程");return;
    }
    if(!v.recall_card){text(300,"没有正在进行的复习。");button(c,{40,1000,604,96},Action::RecallHome,"返回复习");return;}
    if(v.recall_typing&&!s.checked){dictionary_pages(c,v);return;}
    const auto&r=*v.recall_card;
    button(c,{40,16,136,96},Action::Back,"暂停");
    text(140,"已完成 "+std::to_string(s.solved)+" / "+std::to_string(s.total)+" 项");
    if(s.checked){
        strong(c,40,255,s.correct?"答对了":"再记一下，稍后重练");
        text(355,r.card.front,2,3);text(580,r.card.back,1,3);
        text(735,s.hint?"本题用了提示，稍后再独立试一次。":s.correct?"记下这次练习了，之后还会安排复习。":v.recall_typing?"你的答案："+s.typed:"这道题稍后再练，也会保留在错题里。",1,2);
        button(c,{40,870,604,88},Action::Speak,"听正确发音",0,false,v.audio_available);
        text(980,v.notice,1,2);
        button(c,{40,1080,604,96},Action::RecallNext,"继续",0,true);return;
    }
    const bool reverse=!s.queue.empty()&&s.queue.front().kind==RecallKind::Reverse;
    strong(c,40,240,v.recall_listening?"听一听，选择意思":reverse?"看词义，选择英文":"选择正确的意思");
    if(v.recall_listening)button(c,{40,330,604,100},Action::Speak,v.recall_heard?"再听一遍":"播放题目",0,true,v.audio_available);
    else text(330,reverse?r.meaning:r.card.front,reverse?1:2,3);
    if(v.recall_listening)text(452,v.notice.empty()?"听完后选择；播放失败可以暂缓。":v.notice,1,2);
    for(int i=0;i<int(s.options.size());++i)button(c,{40,565+i*116,604,100},Action::RecallChoose,s.options[i],i,s.selected==i);
    button(c,{40,930,604,88},Action::RecallSkip,v.recall_listening?"暂时不方便听，保留待补":"暂缓这道题");
    button(c,{40,1080,604,96},Action::RecallCheck,v.recall_listening&&!v.recall_heard?"先播放题目":"检查答案",0,true,v.recall_ready);
}
