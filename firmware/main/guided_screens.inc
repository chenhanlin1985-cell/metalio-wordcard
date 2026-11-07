// Real 684 x 1216 device canvas. White controls preserve the accepted refresh behavior.
void UI::guided_pages(Canvas &c, const View &v) {
    const auto &g = v.guided;
    auto text = [&](int y, const std::string &s, int scale = 1, int maxlines = 3) {
        // Factory 60px Chinese coverage is sparse; never mix enlarged fallback and native glyphs.
        const bool heading = scale == 2 && std::any_of(s.begin(), s.end(),
                                                       [](unsigned char ch) { return ch >= 128; });
        if (heading)
            scale = 1;
        auto lines = c.wrap(s, 604, scale);
        for (int i = 0; i < std::min(int(lines.size()), maxlines); ++i) {
            const int line_y = y + i * (scale == 2 ? 74 : 43);
            if (heading)
                strong(c, 40, line_y, lines[i]);
            else
                c.text(40, line_y, lines[i], scale);
        }
    };
    auto status = [&](int y) {
        text(y, v.notice.empty() ? (!v.writable ? "无法保存进度，本次仅体验学习。" : "") : v.notice,
             1, 2);
    };
    if (v.screen == Screen::LearningStart) {
        button(c,{40,16,136,96},Action::Back,"返回");
        text(143,"找到学习起点",2);
        text(207,"主要用途");
        for(int t=0;t<3;++t)button(c,{40+t*206,265,192,88},Action::LearningInterest,guided_tracks[t],t,t==g.draft.interest);
        text(410,"目前基础");
        const char *choices[]={"从基础开始 · 听读短句","有简单基础 · 日常应用","能读简单材料 · 进阶情境"};
        for(int l=0;l<3;++l)button(c,{40,480+l*135,604,108},Action::LearningLevel,choices[l],l,l==g.draft.start);
        button(c,{40,880,604,88},Action::AssessmentHome,"不确定基础？做听读起点测试");
        if(v.notice.empty()&&v.writable)text(990,"自选起点只作建议，可以随时换课。",1,1);else status(980);
        button(c,{40,1070,604,100},Action::LearningApply,"查看我的推荐",0,true);
        return;
    }
    if (v.screen == Screen::Home || v.screen == Screen::Welcome) {
        const int recommended=g.recommended;
        const auto &course=guided_courses()[recommended];
        text(36,"今天学一点",2);
        button(c,{480,16,164,96},Action::Dictionary,"查词");
        text(166,g.active?"继续上次的学习":"今天从这里开始",2);
        text(238,course.title,2);
        text(298,std::string(guided_levels[course.level])+" · "+guided_tracks[course.track]);
        text(362,course.goal,1,2);
        c.fill({40,474,604,2});
        text(503,g.recommendation_reason,1,2);
        button(c,{40,620,604,108},Action::CurriculumContinue,
               g.active?"继续学习":"开始第 "+std::to_string(g.recommended_node+1)+" 小节",0,true);
        button(c,{40,758,292,92},Action::LearningStart,g.profile.configured?"调整学习起点":"推荐学习起点");
        button(c,{352,758,292,92},Action::GuidedCourses,"分级选课");
        row(c,{40,880,604,90},Action::RecallHome,"今日复习 · 巩固学过的内容");
        row(c,{40,984,604,88},Action::GuidedCourse,"查看本课内容",recommended);
        c.text(40,1078,c.ellipsize(v.notice.empty()?(!v.writable?"无法保存进度，本次仅体验学习。":""):v.notice,604));
        tabs(c,Action::Home);return;
    }
    if (v.screen == Screen::CourseDetail) {
        button(c,{40,16,136,96},Action::Back,"返回");
        text(135,g.title,2);
        text(195,g.goal,1,2);
        const auto &course=guided_courses()[g.course];
        text(286,std::string(guided_levels[course.level])+" · "+guided_tracks[course.track]);
        const unsigned done=g.completed[g.course];
        int next=0;while(next<guided_node_count&&(done&(1u<<next)))++next;
        if(next==guided_node_count)for(int n=0;n<guided_node_count;++n)if(!(g.listened[g.course]&(1u<<n))){next=n;break;}
        for(int n=0;n<guided_node_count;++n){
            const int y=350+n*96;
            const bool complete=done&(1u<<n),available=n==0||(done&(1u<<(n-1)));
            c.border({40,y,604,88},n==next?3:1);
            c.text(60,y+8,std::to_string(n+1)+"  "+guided_nodes[n]);
            c.text(108,y+46,complete?(g.listened[g.course]&(1u<<n)?"已完成 · 可复习":"已学 · 听力待补"):
                   available?(n<2?"3 个词 · 看词与听词":n<4?"3 句表达 · 听懂意思":n==4?"2 段对话 · 阅读理解":"2 段对话 · 听取信息"):"完成上一小节后开始");
            buttons_.push_back({{40,y,604,88},{Action::GuidedBegin,n},guided_nodes[n],available});
        }
        button(c,{40,947,604,88},Action::GuidedListen,"听本课句子");
        button(c,{40,1060,604,100},g.active?Action::GuidedResume:Action::GuidedBegin,
               g.active?"继续未完成的小节":next<guided_node_count?"开始第 "+std::to_string(next+1)+" 小节":"再学一遍",
               next<guided_node_count?next:0,true);
        return;
    }
    if (v.screen == Screen::GuidedListen) {
        button(c, {40, 16, 136, 96}, Action::Back, "返回");
        text(143, "听本课句子", 2);
        text(220, g.title + " · " + std::to_string(v.guided_sentence + 1) + " / 6");
        text(330, g.text, 2, 3);
        text(590, g.ipa, 1, 3);
        text(750, g.meaning, 1, 3);
        status(895);
        button(c, {40, 985, 604, 88}, Action::Speak,
               v.audio_busy                                ? "重新播放"
               : v.audio_source == SpeechSource::Synthesis ? "听句子 · 离线合成"
                                                           : "听句子",
               0, true, v.audio_available);
        button(c, {40, 1084, 280, 96}, Action::GuidedListenStep, "上一句", -1, false,
               v.guided_sentence > 0);
        button(c, {364, 1084, 280, 96}, Action::GuidedListenStep, "下一句", 1, false,
               v.guided_sentence < 5);
        return;
    }
    if (v.screen == Screen::GuidedCourses) {
        text(36,"选择课程",2);
        const int tracks[]={-1,3,0,1,2};const char *names[]={"全部","基础","日常","科技","工作"};
        for(int i=0;i<5;++i)button(c,{40+i*123,130,112,88},Action::GuidedTrack,names[i],tracks[i],g.catalog_track==tracks[i]);
        const char *levels[]={"全部级别","A1 起步","A2 应用","B1 进阶"};
        for(int i=0;i<4;++i)button(c,{40+i*154,242,142,88},Action::CurriculumLevel,levels[i],i-1,g.catalog_level==i-1);
        if(g.catalog.empty()){
            text(440,"这个组合暂时没有课程。",2);
            text(535,"试试其他级别，或选择全部课程。");
        }
        for(int i=0;i<4;++i){
            int at=g.catalog_page*4+i;if(at>=int(g.catalog.size()))break;
            int idx=g.catalog[at],y=374+i*146;const auto &course=guided_courses()[idx];
            unsigned count=0;for(int n=0;n<guided_node_count;++n)count+=(g.completed[idx]>>n)&1;
            strong(c,40,y,course.title);
            c.text(40,y+55,std::string(guided_levels[course.level])+" · "+std::to_string(count)+" / 6 小节");
            chevron(c,618,y+48);c.fill({40,y+124,604,1});
            buttons_.push_back({{40,y-8,604,132},{Action::GuidedCourse,idx},course.title});
        }
        button(c,{40,983,186,88},Action::CurriculumPage,"上一页",-1,false,g.catalog_page>0);
        c.text(278,1010,std::to_string(g.catalog_page+1)+" / "+std::to_string(std::max(1,(int(g.catalog.size())+3)/4)));
        button(c,{458,983,186,88},Action::CurriculumPage,"下一页",1,false,(g.catalog_page+1)*4<int(g.catalog.size()));
        tabs(c,Action::GuidedCourses);return;
    }
    if (v.screen == Screen::Practice) {
        text(30, "随时练一练", 2);
        text(144, "课程带着学，这里按需要练习。");
        row(c, {40, 260, 604, 96}, Action::RecallHome, "今日复习 · 错题 / 听力 / 拼写");
        row(c, {40, 376, 604, 96}, Action::Dictionary, "输入查词与听发音");
        row(c, {40, 492, 604, 96}, Action::Vocabulary, "我的生词本");
        row(c, {40, 608, 604, 96}, Action::Library, "词书速记 · KET / PET / 自选词库");
        row(c, {40, 724, 604, 96}, Action::LegacyPractice, "词书专项 · 听力 / 跟读 / 难词");
        if (v.resume_available)
            row(c, {40, 840, 604, 96}, Action::Resume, "继续之前的背词小节");
        row(c,{40,970,604,92},Action::AssessmentHome,"听读起点测试 · 报告与选课");
        tabs(c, Action::Practice);
        return;
    }
    if (v.screen == Screen::GuidedPause) {
        text(80, "休息一下", 2);
        text(240, g.title + " · 第 " + std::to_string(g.node + 1) + " 小节");
        text(320, "已完成 " + std::to_string(g.solved) + " / " + std::to_string(g.total) + " 题");
        text(420, v.writable ? "当前题目和答案会保留，下次继续。" : "尚未保存成功，请先保持开机。",
             1, 3);
        status(560);
        button(c, {40, 700, 604, 104}, Action::GuidedResume, "继续这一节", 0, true);
        button(c, {40, 834, 604, 96}, Action::Home, "回首页，稍后继续");
        text(958, "结束本节会丢弃本节作答，已完成的小节保留。", 1, 2);
        button(c, {40, 1075, 604, 96}, Action::GuidedAbandon, "结束本节，重新选课");
        return;
    }
    if (v.screen == Screen::GuidedDone) {
        const bool all = g.completed[g.course] == guided_complete_mask;
        text(80,
             all ? (g.listened[g.course] == guided_complete_mask ? "这门情境课完成了" : "六小节完成，记得补听力")
                 : "小节完成",
             2);
        text(240, g.title);
        text(340, "完成第 " + std::to_string(g.node + 1) + " / 6 小节");
        text(430, "首次答对 " + std::to_string(g.first_correct) + " / " + std::to_string(g.total) +
                      " 题");
        text(520, g.errors ? "错题已在本节重新练对。" : "这一节全部一次答对。");
        text(610,
             g.skipped ? std::to_string(g.skipped) + " 道听力改为看词练习，建议稍后再听。"
                       : "听懂、读懂，再在新情景中理解。",
             1, 3);
        status(765);
        int next = 0;
        while (next < guided_node_count && (g.completed[g.course] & (1u << next)))
            ++next;
        const bool catch_up = next == guided_node_count && g.listened[g.course] != guided_complete_mask;
        if (catch_up)
            for (int n=0;n<guided_node_count;++n)
                if (!(g.listened[g.course] & (1u << n))) { next=n; break; }
        button(c, {40, 900, 604, 104}, next < guided_node_count ? Action::GuidedBegin : Action::Home,
               catch_up ? "补本课听力" : next < guided_node_count ? "继续第 " + std::to_string(next + 1) + " 小节" : "查看下一课建议",
               next < guided_node_count ? next : 0, true);
        button(c, {40, 1040, 604, 96}, Action::Home, "回到学习首页");
        return;
    }
    if(g.kind==GuidedKind::ReadDialogue||g.kind==GuidedKind::ListenDialogue){
        const bool listen=g.kind==GuidedKind::ListenDialogue&&!g.reading;
        button(c,{40,16,136,96},Action::GuidedPause,"暂停");
        text(140,g.checked?(g.correct?"答对了":"看懂原因，再试一次"):listen?"听短对话":"读短对话",2);
        if(g.checked){
            text(250,g.prompt,1,3);text(420,g.answer,2,3);text(610,g.explanation,1,4);
            button(c,{40,850,604,88},Action::Speak,"再听对话",0,false,v.audio_available);
            status(970);button(c,{40,1080,604,96},Action::GuidedNext,"继续",0,true);
        }else{
            if(listen){
                button(c,{40,250,604,108},Action::Speak,g.heard?"再听一遍":"播放对话",0,true,v.audio_available);
                text(395,v.audio_available?"先听，再选出对话中的信息。":"暂时无法播放，可以改为阅读。",1,2);
                button(c,{40,475,604,88},Action::GuidedSkip,"现在不方便听，改为阅读");
            }else text(230,g.passage,1,7);
            text(575,g.prompt,1,2);
            for(int i=0;i<int(g.options.size());++i)button(c,{40,690+i*116,604,100},Action::GuidedChoose,g.options[i],i,!g.selected.empty()&&g.selected[0]==i);
            // Keep asynchronous playback and save errors visible in both dialogue modes.
            const std::string message = !v.notice.empty() ? v.notice : !v.writable ? "无法保存进度，本次仅体验学习。" : v.audio_busy ? "正在准备或播放对话…" : "";
            c.text(40,1034,c.ellipsize(message,604));
            button(c,{40,1080,604,96},Action::GuidedCheck,listen&&!g.heard?"先播放对话":"检查答案",0,true,g.can_check);
        }
        return;
    }
    // Focused lesson: one task, explicit Check, feedback, then Continue.
    button(c, {40, 16, 136, 96}, Action::GuidedPause, "暂停");
    c.text(210, 43, "完成 " + std::to_string(g.solved) + " / " + std::to_string(g.total) + " 题");
    c.border({210, 102, 434, 12});
    if (g.total)
        c.fill({212, 104, 430 * g.solved / g.total, 8});
    text(153, g.prompt, 2);
    const bool intro = g.kind == GuidedKind::Introduce, order = g.kind == GuidedKind::Order;
    const bool listen =
        (g.kind == GuidedKind::ListenWord || g.kind == GuidedKind::ListenMeaning) && !g.reading;
    if (g.checked) {
        text(285, g.correct ? "答对了" : "再看一遍，稍后再练", 2);
        text(407, g.text, 2, 3);
        text(646, g.meaning, 1, 3);
        button(c, {40, 810, 604, 90}, Action::Speak,
               v.audio_source == SpeechSource::Synthesis ? "听发音 · 离线合成" : "听发音", 0, false,
               v.audio_available);
        status(920);
        button(c, {40, 1080, 604, 96}, Action::GuidedNext, "继续", 0, true);
        return;
    }
    if (intro) {
        text(285, g.text, 2, 3);
        text(530, g.ipa, 1, 3);
        text(685, g.usage, 1, 4);
        button(c, {40, 885, 292, 88}, Action::Speak,
               v.audio_source == SpeechSource::Synthesis ? "听发音 · 离线合成" : "听发音", 0, false,
               v.audio_available);
        button(c,{352,885,292,88},Action::GuidedAdd,"加入生词本",0,false,v.writable);
        status(985);
        button(c, {40, 1080, 604, 96}, Action::GuidedNext, "开始练习", 0, true);
        return;
    }
    if (order) {
        text(270, g.meaning, 1, 3);
        text(420, g.selected_text.empty() ? "按顺序点选下面的单词" : g.selected_text, 1, 3);
        c.fill({40, 564, 604, 1});
        int x = 40, y = 605;
        for (int i = 0; i < int(g.options.size()); ++i) {
            const int w = std::max(74, c.text_width_px(g.options[i],36) + 30);
            if (x + w > 644) {
                x = 40;
                y += 82;
            }
            const bool used =
                std::find(g.selected.begin(), g.selected.end(), i) != g.selected.end();
            button(c, {x, y, w, 80}, Action::GuidedChoose, g.options[i], i, false, !used);
            x += w + 10;
        }
        button(c, {40, 906, 604, 88}, Action::GuidedRemove, "撤回最后一个词",
               int(g.selected.size()) - 1, false, !g.selected.empty());
    } else {
        if (listen) {
            button(c, {40, 280, 604, 108}, Action::Speak, g.heard ? "再听一遍" : "播放题目", 0,
                   true, v.audio_available);
            text(410, v.audio_source == SpeechSource::Synthesis ? "本题使用离线合成发音"
                                                                : "听完后，点选一个答案。");
        } else
            text(280, g.text, 2, 3);
        for (int i = 0; i < int(g.options.size()); ++i) {
            const int y = 530 + i * 119;
            const bool selected = !g.selected.empty() && g.selected[0] == i;
            button(c, {40, y, 604, 104}, Action::GuidedChoose, g.options[i], i, selected);
        }
        if (listen)
            button(c, {40, 895, 604, 88}, Action::GuidedSkip, "现在不方便听，改为看词");
    }
    status(995);
    button(c, {40, 1080, 604, 96}, Action::GuidedCheck,
           listen && !g.heard ? "先播放题目" : "检查答案", 0, true, g.can_check);
}
