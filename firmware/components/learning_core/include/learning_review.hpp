#pragma once
#include "learning_system.hpp"
#include <algorithm>
#include <array>
#include <deque>
#include <iomanip>
#include <set>
#include <sstream>

namespace wordcard {
enum class RecallSkill { Meaning, Listening, Spelling };
enum class RecallKind { Meaning, Reverse, Listen, Spell, Dictation };
enum class RecallFilter { Due, Mistakes, Listening, Spelling, Reverse, Dictation, Personal, PersonalNew, PersonalDue };
struct RecallMemory {
    uint32_t attempts=0, errors=0, due=0, last=0, error_day=0;
    unsigned interval=0;
    bool weak=false, pending=false, undated=false;
};
struct RecallRecord {
    Card card;
    std::string meaning;
    std::array<RecallMemory,3> memory{};
    std::array<uint32_t,3> legacy_seen{};
    std::array<uint32_t,3> legacy_stamp{};
    bool personal=false;
    bool first_answer=false;
    uint32_t first_day=0;
};
struct RecallTask { std::string id; RecallKind kind=RecallKind::Meaning; bool failed=false; };
struct RecallSession {
    std::deque<RecallTask> queue;
    unsigned total=0, solved=0, errors=0, skipped=0;
    bool checked=false, correct=false, hint=false;
    int selected=-1;
    std::string typed;
    std::vector<std::string> options;
};
struct RecallCounts { size_t due=0, mistakes=0, listening=0, fresh=0, learned=0; };
class LearningReview {
  public:
    std::map<std::string,RecallRecord> records;
    RecallSession session;
    uint32_t day=0;
    bool dated=false, heard=false, migrated=false;
    static RecallSkill skill(RecallKind k) {
        return k==RecallKind::Listen?RecallSkill::Listening:
               k==RecallKind::Spell||k==RecallKind::Dictation?RecallSkill::Spelling:RecallSkill::Meaning;
    }
    static std::string normalized(const std::string &s) {
        std::string out; bool space=false;
        for(unsigned char ch:s) {
            if(ch==' '||ch=='\t'||ch=='\r'||ch=='\n'){space=!out.empty();continue;}
            if(space){out+=' ';space=false;}
            out+=ch>='A'&&ch<='Z'?char(ch+32):char(ch);
        }
        return out;
    }
    static bool spellable(const Card &c) {
        return !sentence(c)&&!c.front.empty()&&c.front.size()<=40&&
            std::all_of(c.front.begin(),c.front.end(),[](unsigned char ch){return ch>=32&&ch<127;});
    }
    static bool sentence(const Card&c){return c.tags.find("sentence")!=std::string::npos||c.front.find('?')!=std::string::npos||c.front.find('.')!=std::string::npos;}
    static uint32_t stable_hash(const std::string&s){uint32_t h=2166136261u;for(unsigned char c:s){h^=c;h*=16777619u;}return h;}
    void bind_clock(uint32_t d,bool valid){day=valid?d:0;dated=valid;}
    RecallRecord &register_card(const Card &c,const std::string &meaning,bool personal=false) {
        auto &r=records[c.id];r.card=c;
        if(!meaning.empty())r.meaning=meaning;
        else if(r.meaning.empty())r.meaning=c.back.substr(0,c.back.find('\n'));
        r.personal=r.personal||personal;return r;
    }
    void observe(const Card &c,const std::string &meaning,RecallSkill kind,bool correct,bool assisted=false) {
        auto &r=register_card(c,meaning);auto &m=r.memory[size_t(kind)];
        if(!r.first_answer){r.first_answer=true;r.first_day=dated?day:0;}
        ++m.attempts;
        if(!correct){++m.errors;m.weak=true;m.error_day=dated?day:0;m.interval=0;m.due=dated?day:0;}
        else if(!assisted){
            // Immediate retries and multiple exercise types on one day do not grow the interval.
            if(dated && (!m.last || m.last!=day)) {
                m.interval=m.interval==0?1:m.interval==1?3:std::min(90u,m.interval*2);
                m.due=day+m.interval;
                if(!m.error_day || m.error_day<day)m.weak=false;
            }
            m.pending=false;
        }
        if(assisted){m.weak=true;m.due=dated?day:0;}
        m.undated=!dated;
        if(dated)m.last=day;
    }
    void defer_listening(const Card &c,const std::string &meaning) {
        register_card(c,meaning).memory[1].pending=true;
    }
    void import_legacy(const Card &c,const Progress &p,StudyMode mode,bool personal) {
        auto &r=register_card(c,"",personal);const size_t i=size_t(mode);
        const auto stamp=stable_hash(std::to_string(p.seen)+":"+std::to_string(p.due_day)+":"+std::to_string(p.again)+":"+std::to_string(p.hard)+":"+std::to_string(p.good)+":"+std::to_string(p.interval)+":"+std::to_string(p.box));
        if(r.legacy_stamp[i]==stamp)return;
        r.legacy_stamp[i]=stamp;
        r.legacy_seen[i]=p.seen;
        auto &m=r.memory[mode==StudyMode::Listening?1:0];
        if(!p.seen){if(!m.attempts)m={};return;}
        // Historical self ratings remain self ratings; never synthesize automatic attempts.
        m.due=p.due_day;m.interval=p.interval;m.undated=p.due_day<=1;
        // A young memory box alone is not evidence of difficulty. Preserve an
        // unresolved automatic error; import only explicit legacy difficulty.
        m.weak=m.weak||(p.box<2&&(p.again||p.hard));
    }
    bool known(const RecallRecord &r,size_t k) const {
        return r.memory[k].attempts || (k==0&&(r.legacy_seen[0]||r.legacy_seen[2])) || (k==1&&r.legacy_seen[1]);
    }
    bool due(const RecallRecord &r,size_t k) const {
        const auto &m=r.memory[k];
        return (known(r,k)||m.pending) && (m.undated || (dated && m.due<=day));
    }
    RecallCounts counts() const {
        RecallCounts c;
        for(const auto &entry:records){const auto &r=entry.second;bool d=false,w=false,k=false;
            for(size_t i=0;i<3;++i){if(i<2)d|=due(r,i);w|=r.memory[i].weak;k|=known(r,i);}
            c.due+=d;c.mistakes+=w;c.listening+=r.memory[1].pending;c.learned+=k;
            c.fresh+=r.personal&&!k;
        }return c;
    }
    ProgressMap display_progress(const ProgressMap &legacy,const std::vector<Card>&cards) const {
        auto result=legacy;
        for(const auto&c:cards){const auto it=records.find(c.id);if(it==records.end())continue;
            const auto&m=it->second.memory[0];if(!m.attempts)continue;
            auto&p=result[c.id];p.seen=std::max(p.seen,m.attempts);p.due_day=m.undated?0:m.due;
            p.interval=uint16_t(m.interval);p.box=m.weak?0:2;
        }return result; // UI/scheduling projection only; original rating evidence is never rewritten.
    }
    const RecallRecord *current() const {
        if(session.queue.empty())return nullptr;
        auto it=records.find(session.queue.front().id);return it==records.end()?nullptr:&it->second;
    }
    bool listening() const {
        return current()&&(session.queue.front().kind==RecallKind::Listen||session.queue.front().kind==RecallKind::Dictation);
    }
    bool typing() const {
        return current()&&(session.queue.front().kind==RecallKind::Spell||session.queue.front().kind==RecallKind::Dictation);
    }
    void prepare() {
        heard=false;session.checked=session.correct=session.hint=false;
        session.selected=-1;session.typed.clear();session.options.clear();
        const auto *r=current();if(!r||typing())return;
        const bool reverse=session.queue.front().kind==RecallKind::Reverse;
        session.options.push_back(reverse?r->card.front:r->meaning);
        // Do not use identical words/meanings as distractors, including duplicate senses.
        std::vector<const RecallRecord*> candidates;
        for(const auto &e:records){
            if(sentence(e.second.card)!=sentence(r->card)||normalized(e.second.card.front)==normalized(r->card.front)||e.second.meaning==r->meaning)continue;
            candidates.push_back(&e.second);
        }
        auto rank=[&](const RecallRecord&v){const auto dot=r->meaning.find('.');const auto pos=dot<6?r->meaning.substr(0,dot+1):"";
            return int(v.card.group==r->card.group)+2*int(!pos.empty()&&v.meaning.rfind(pos,0)==0);};
        std::stable_sort(candidates.begin(),candidates.end(),[&](const auto*a,const auto*b){
            if(rank(*a)!=rank(*b))return rank(*a)>rank(*b);
            return stable_hash(a->card.id+std::to_string(day))<stable_hash(b->card.id+std::to_string(day));
        });
        for(const auto *candidate:candidates){const auto&e=*candidate;
            const auto s=reverse?e.card.front:e.meaning;
            if(s.empty()||s.size()>160||std::find(session.options.begin(),session.options.end(),s)!=session.options.end())continue;
            session.options.push_back(s);if(session.options.size()==3)break;
        }
        // Reproducible within a saved question, varied between review sessions.
        const size_t shift=(day+session.solved+r->memory[size_t(skill(session.queue.front().kind))].attempts)%session.options.size();
        std::rotate(session.options.begin(),session.options.begin()+shift,session.options.end());
    }
    bool begin(RecallFilter filter,unsigned limit=10,unsigned fresh_limit=10) {
        if(current())return false;
        std::vector<std::pair<uint32_t,RecallTask>> tasks;
        for(const auto &e:records){const auto &r=e.second;
            if((filter==RecallFilter::Personal||filter==RecallFilter::PersonalNew||filter==RecallFilter::PersonalDue)&&!r.personal)continue;
            if(filter==RecallFilter::PersonalNew)continue;
            if(filter==RecallFilter::Listening){if(r.memory[1].pending)tasks.push_back({0,{e.first,RecallKind::Listen}});continue;}
            if(filter==RecallFilter::Spelling||filter==RecallFilter::Reverse||filter==RecallFilter::Dictation){
                if(known(r,0)&&spellable(r.card))tasks.push_back({r.memory[2].due,{e.first,filter==RecallFilter::Spelling?RecallKind::Spell:filter==RecallFilter::Dictation?RecallKind::Dictation:RecallKind::Reverse}});
                continue;
            }
            // Default adult review focuses on listening and meaning. Explicit spelling/dictation
            // and existing in-progress writing sessions remain available without losing records.
            const size_t skills=filter==RecallFilter::Mistakes?3:2;
            for(size_t k=0;k<skills;++k)if(filter==RecallFilter::Mistakes?r.memory[k].weak:due(r,k)) {
                tasks.push_back({r.memory[k].due,{e.first,k==0?RecallKind::Meaning:k==1?RecallKind::Listen:RecallKind::Spell}});
                break;
            }
        }
        std::stable_sort(tasks.begin(),tasks.end(),[](const auto&a,const auto&b){return a.first<b.first;});
        session={};limit=std::max(1u,std::min(20u,limit));
        for(const auto &t:tasks){session.queue.push_back(t.second);if(session.queue.size()==limit)break;}
        if(filter==RecallFilter::Due||filter==RecallFilter::Personal||filter==RecallFilter::PersonalNew)for(const auto &e:records){
            if(session.queue.size()>=limit||!fresh_limit)break;
            const auto&r=e.second;if(r.personal&&!known(r,0)&&!known(r,1)&&!known(r,2)){
                session.queue.push_back({e.first,RecallKind::Meaning});--fresh_limit;
            }
        }
        session.total=unsigned(session.queue.size());prepare();return current()!=nullptr;
    }
    bool ready() const {
        return current()&&!session.checked&&(!listening()||heard)&&
            (typing()?!normalized(session.typed).empty():session.options.size()>1&&session.selected>=0&&session.selected<int(session.options.size()));
    }
    bool check() {
        if(!ready())return false;
        const auto *r=current();const auto k=session.queue.front().kind;
        const auto answer=k==RecallKind::Reverse||typing()?r->card.front:r->meaning;
        session.correct=typing()?normalized(session.typed)==normalized(answer):session.options[session.selected]==answer;
        observe(r->card,r->meaning,skill(k),session.correct,session.hint);
        session.checked=true;if(!session.correct){++session.errors;session.queue.front().failed=true;}
        return true;
    }
    bool next() {
        if(!current()||!session.checked)return false;
        const auto q=session.queue.front();session.queue.pop_front();
        if(!session.correct||session.hint)session.queue.push_back(q);else ++session.solved;
        prepare();return true;
    }
    bool skip() {
        const auto*r=current();if(!r||session.checked)return false;
        if(listening())defer_listening(r->card,r->meaning);
        session.queue.pop_front();++session.skipped;prepare();return true;
    }
    std::string encode() const {
        std::ostringstream o;o<<"recall-v1 "<<migrated<<' '<<records.size()<<'\n';
        for(const auto&e:records){const auto&r=e.second;const auto&c=r.card;
            o<<std::quoted(e.first)<<' '<<std::quoted(c.front)<<' '<<std::quoted(c.back)<<' '<<std::quoted(c.hint)<<' '<<std::quoted(c.tags)<<' '<<std::quoted(c.group)<<' '<<std::quoted(c.ipa)<<' '<<std::quoted(r.meaning)<<' '<<r.personal<<' '<<r.first_answer<<' '<<r.first_day;
            for(auto n:r.legacy_seen)o<<' '<<n;
            for(auto n:r.legacy_stamp)o<<' '<<n;
            for(const auto&m:r.memory)o<<' '<<m.attempts<<' '<<m.errors<<' '<<m.due<<' '<<m.last<<' '<<m.error_day<<' '<<m.interval<<' '<<m.weak<<' '<<m.pending<<' '<<m.undated;
            o<<'\n';
        }
        o<<session.total<<' '<<session.solved<<' '<<session.errors<<' '<<session.skipped<<' '<<session.checked<<' '<<session.correct<<' '<<session.hint<<' '<<session.selected<<' '<<std::quoted(session.typed)<<' '<<session.options.size();
        for(const auto&s:session.options)o<<' '<<std::quoted(s);
        o<<'\n'<<session.queue.size()<<'\n';
        for(const auto&q:session.queue)o<<std::quoted(q.id)<<' '<<int(q.kind)<<' '<<q.failed<<'\n';
        return o.str();
    }
    bool decode(const std::string &bytes) {
        if(bytes.size()>4*1024*1024)return false;
        LearningReview n;std::istringstream in(bytes);std::string version,extra;size_t count=0;
        if(!(in>>version>>n.migrated>>count)||version!="recall-v1"||count>10000)return false;
        for(size_t i=0;i<count;++i){RecallRecord r;auto&c=r.card;
            if(!(in>>std::quoted(c.id)>>std::quoted(c.front)>>std::quoted(c.back)>>std::quoted(c.hint)>>std::quoted(c.tags)>>std::quoted(c.group)>>std::quoted(c.ipa)>>std::quoted(r.meaning)>>r.personal>>r.first_answer>>r.first_day))return false;
            if(c.id.empty()||c.id.size()>256||c.front.size()>512||c.back.size()>16384||r.meaning.size()>16384||n.records.count(c.id))return false;
            for(auto&v:r.legacy_seen)if(!(in>>v))return false;
            for(auto&v:r.legacy_stamp)if(!(in>>v))return false;
            for(auto&m:r.memory)if(!(in>>m.attempts>>m.errors>>m.due>>m.last>>m.error_day>>m.interval>>m.weak>>m.pending>>m.undated)||m.errors>m.attempts||m.interval>365)return false;
            n.records.emplace(c.id,std::move(r));
        }
        auto&s=n.session;
        if(!(in>>s.total>>s.solved>>s.errors>>s.skipped>>s.checked>>s.correct>>s.hint>>s.selected>>std::quoted(s.typed)>>count)||s.total>20||s.solved+s.skipped>s.total||count>3||s.typed.size()>80||s.selected< -1||s.selected>=int(count)||(s.correct&&!s.checked))return false;
        for(size_t i=0;i<count;++i){std::string v;if(!(in>>std::quoted(v))||v.size()>16384)return false;s.options.push_back(v);}
        if(!(in>>count)||count>20||count+s.solved+s.skipped!=s.total)return false;
        std::set<std::string> queued;
        for(size_t i=0;i<count;++i){RecallTask q;int k;if(!(in>>std::quoted(q.id)>>k>>q.failed)||!n.records.count(q.id)||k<0||k>4||!queued.insert(q.id).second)return false;q.kind=RecallKind(k);s.queue.push_back(q);}
        if((in>>extra)||(s.checked&&!n.current())||(n.typing()&&!s.options.empty()))return false;
        *this=std::move(n);return true;
    }
};
} // namespace wordcard
