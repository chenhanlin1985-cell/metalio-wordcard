// Kept in the learning-core translation unit so host and device run identical logic.
bool GuidedLearning::dialogue() const {
    return current() && (state.pending.front().kind==GuidedKind::ReadDialogue ||
                         state.pending.front().kind==GuidedKind::ListenDialogue);
}
std::string GuidedLearning::expected_answer() const {
    if(!current())return {};
    if(dialogue()){
        const auto &t=guided_courses()[state.course].tasks[state.pending.front().item-12];
        return t.options[t.answer];
    }
    const auto &s=state.pending.front();
    return s.kind==GuidedKind::ListenWord&&!s.reading?current()->card.front:current()->meaning;
}
int GuidedLearning::next_node(int c) const {
    if(c<0||c>=guided_course_count)return 0;
    for(int n=0;n<guided_node_count;++n)if(!(state.completed[c]&(1u<<n)))return n;
    // Content complete but listening deferred: revisit the earliest pending listening section.
    for(int n=0;n<guided_node_count;++n)if(!(state.listened[c]&(1u<<n)))return n;
    return 0;
}
std::vector<int> GuidedLearning::catalog() const {
    std::vector<int> out;
    for(int c=0;c<guided_course_count;++c){const auto &m=guided_courses()[c];
        if((catalog_track<0||m.track==catalog_track)&&(catalog_level<0||m.level==catalog_level))out.push_back(c);
    }
    std::stable_sort(out.begin(),out.end(),[](int a,int b){
        const auto &x=guided_courses()[a],&y=guided_courses()[b];
        const int xt=x.track==3?-1:x.track,yt=y.track==3?-1:y.track;
        return x.level!=y.level?x.level<y.level:xt!=yt?xt<yt:x.order<y.order;
    });return out;
}
int GuidedLearning::recommended_course() const {
    if(state.active)return state.course;
    auto unfinished=[&](int c){return state.completed[c]!=guided_complete_mask||state.listened[c]!=guided_complete_mask;};
    int last=profile.last_course;
    if(last>=0&&last<guided_course_count&&unfinished(last))return last;
    int track=last>=0&&guided_courses()[last].track!=3?guided_courses()[last].track:profile.interest;
    std::vector<int> route;
    if(profile.start==0)for(int c=12;c<guided_course_count;++c)route.push_back(c);
    std::vector<int> interest;
    for(int c=0;c<guided_course_count;++c)if(guided_courses()[c].track==track)interest.push_back(c);
    std::sort(interest.begin(),interest.end(),[](int a,int b){return guided_courses()[a].order<guided_courses()[b].order;});
    for(int c:interest)if(profile.start<2||guided_courses()[c].level>=2)route.push_back(c);
    if(route.empty())route=interest;
    // Continue within the chosen branch after the last fully completed course.
    auto it=std::find(route.begin(),route.end(),last);
    if(it!=route.end())for(auto next=it+1;next!=route.end();++next)if(unfinished(*next))return *next;
    for(int c:route)if(unfinished(c))return c;
    return last>=0?last:12;
}
std::string GuidedLearning::recommendation_reason() const {
    if(state.active)return "先继续上次暂停的小节，当前答案已保留。";
    int c=recommended_course();
    if(profile.last_course==c){
        if(state.completed[c]==guided_complete_mask&&state.listened[c]!=guided_complete_mask)return "本课词句已完成，还有听力待补。";
        if(state.completed[c]==guided_complete_mask)return "这一阶段已学完，可以复习或探索其他课程。";
        return "接着学这门课的下一小节。";
    }
    return guided_courses()[c].track==3?"从基础听读开始，再进入你需要的日常或科技场景。":
           "根据你的用途和自报基础推荐，可以随时换课。";
}
std::string GuidedLearning::encode_session() const {
    std::ostringstream s;
    s<<profile.configured<<' '<<profile.interest<<' '<<profile.start<<' '
     <<std::quoted(profile.last_course<0?std::string():guided_courses()[profile.last_course].id)<<'\n';
    s<<std::quoted(guided_courses()[state.course].id)<<' '<<state.node<<' '<<state.total<<' '<<state.solved<<' '
     <<state.errors<<' '<<state.skipped<<' '<<state.first_correct<<' '<<state.solved_mask<<' '
     <<state.attempted_mask<<' '<<state.skipped_mask<<' '<<state.active<<' '<<state.finished<<' '
     <<state.checked<<' '<<state.correct<<'\n';
    s<<guided_course_count<<'\n';
    for(int c=0;c<guided_course_count;++c)s<<std::quoted(guided_courses()[c].id)<<' '<<state.completed[c]<<' '<<state.listened[c]<<'\n';
    s<<state.pending.size()<<'\n';
    for(const auto &t:state.pending)s<<std::quoted(guided_courses()[state.course].items[t.item].card.id)<<' '
        <<int(t.kind)<<' '<<t.slot<<' '<<t.reading<<'\n';
    s<<state.selected.size();for(int n:state.selected)s<<' '<<n;s<<'\n';
    const auto payload=s.str();return "guided-session-v3 "+std::to_string(hash(payload))+"\n"+payload;
}
bool GuidedLearning::decode_session(const std::string &bytes,std::string &error) {
    auto invalid=[&](){error="Invalid graded learning state";return false;};
    const auto nl=bytes.find('\n');if(nl==std::string::npos||bytes.size()>16384)return invalid();
    std::istringstream h(bytes.substr(0,nl));std::string v,extra;uint32_t sum;
    if(!(h>>v>>sum)||(h>>extra)||v!="guided-session-v3"||hash(bytes.substr(nl+1))!=sum)return invalid();
    auto course=[](const std::string &id){for(int c=0;c<guided_course_count;++c)if(guided_courses()[c].id==id)return c;return -1;};
    GuidedLearning next;auto &s=next.state;auto &p=next.profile;std::istringstream in(bytes.substr(nl+1));std::string id;
    if(!(in>>p.configured>>p.interest>>p.start>>std::quoted(id))||p.interest<0||p.interest>2||p.start<0||p.start>2)return invalid();
    p.last_course=id.empty()?-1:course(id);if(!id.empty()&&p.last_course<0)return invalid();
    if(!(in>>std::quoted(id)>>s.node>>s.total>>s.solved>>s.errors>>s.skipped>>s.first_correct>>
         s.solved_mask>>s.attempted_mask>>s.skipped_mask>>s.active>>s.finished>>s.checked>>s.correct))return invalid();
    s.course=course(id);
    if(s.course<0||s.node<0||s.node>=guided_node_count||s.total<0||s.total>8||s.solved<0||s.solved>s.total||
       s.skipped<0||s.skipped>s.total||s.first_correct<0||s.first_correct>s.total||s.errors<0||s.errors>1000000||
       (s.active&&s.finished)||(s.correct&&!s.checked))return invalid();
    size_t n;std::set<int> seen;
    if(!(in>>n)||n!=guided_course_count)return invalid();
    for(size_t i=0;i<n;++i){unsigned done,heard;if(!(in>>std::quoted(id)>>done>>heard))return invalid();int c=course(id);
        if(c<0||!seen.insert(c).second||done>guided_complete_mask||(done&(done+1))||(heard&~done))return invalid();
        s.completed[c]=done;s.listened[c]=heard;
    }
    if(!(in>>n)||n>11)return invalid();
    std::set<int> slots;
    for(size_t i=0;i<n;++i){GuidedStep t;int kind;if(!(in>>std::quoted(id)>>kind>>t.slot>>t.reading))return invalid();
        t.item=-1;for(int j=0;j<16;++j)if(guided_courses()[s.course].items[j].card.id==id)t.item=j;
        if(t.item<0||kind<0||kind>6||t.slot<0||t.slot>=s.total||(kind>=5)!=(t.item>=12))return invalid();
        if(kind!=0&&!slots.insert(t.slot).second)return invalid();
        t.kind=GuidedKind(kind);s.pending.push_back(t);
    }
    if(s.active!=!s.pending.empty()||(s.active&&!next.unlocked(s.course,s.node)))return invalid();
    if(!(in>>n)||n>20)return invalid();
    const auto opts=next.options();seen.clear();
    for(size_t i=0;i<n;++i){int value;if(!(in>>value)||value<0||value>=int(opts.size())||!seen.insert(value).second)return invalid();s.selected.push_back(value);}
    if(in>>extra)return invalid();
    const unsigned bits=(1u<<s.total)-1;
    if(s.solved_mask>bits||s.attempted_mask>bits||s.skipped_mask>bits||(s.solved_mask&~s.attempted_mask)||
       count_bits(s.solved_mask)!=unsigned(s.solved)||count_bits(s.skipped_mask)!=unsigned(s.skipped)||
       (s.finished&&(s.solved!=s.total||!s.total||!(s.completed[s.course]&(1u<<s.node))))||
       (s.checked&&(!s.active||s.pending.front().kind==GuidedKind::Introduce||s.selected.empty()))||
       (s.active&&s.pending.front().kind!=GuidedKind::Order&&s.selected.size()>1))return invalid();
    next.draft=next.profile;*this=std::move(next);return true;
}
