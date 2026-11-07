#include "vocabulary.hpp"
#include <cctype>
#include <set>
#include <ctime>
namespace wordcard {
namespace {
std::string recovery_time(int64_t ms) {
    if(ms<1704067200000LL || ms>4102444800000LL) return "未校准";
    std::time_t stamp=std::time_t(ms/1000+8*3600);std::tm value{};
#ifdef _WIN32
    gmtime_s(&value,&stamp);
#else
    gmtime_r(&stamp,&value);
#endif
    char text[32];std::strftime(text,sizeof(text),"%Y-%m-%d %H:%M",&value);return text;
}
std::string n(int64_t x) {
    return std::to_string(x);
}
struct View {
    WebPage &p;
    std::string taskId = {}, sessionId = {};
    int64_t learnAt = 0;
    int action(VJ a) {
        const auto name = str(a, "action"), type = str(a, "type");
        if (name == "v-answer" || (name == "v-do" && (type == "continue" || type == "hint")))
            a["taskId"] = taskId;
        if (name == "v-do" && type == "learnNext") {
            a["sessionId"] = sessionId;
            a["learnAt"] = learnAt;
        }
        p.actions.push_back(a.dump());
        return int(p.actions.size() - 1);
    }
    void text(const std::string &s, const std::string &role = "", bool heading = false) {
        if (s.empty())
            return;
        WebBlock b;
        b.kind = heading ? "heading" : "text";
        b.text = s;
        b.area = "body";
        b.role = role;
        p.blocks.push_back(b);
    }
    void button(const std::string &s, VJ a, bool primary = false, const std::string &area = "body",
                const std::string &detail = "", bool selected = false, bool enabled = true) {
        WebBlock b;
        b.kind = "button";
        b.text = s;
        b.action = action(a);
        b.area = area;
        if (str(a,"action")=="v-audio" && area=="foot") b.role="audio";
        if (str(a,"action")=="v-answer" && area=="foot") b.role="option";
        b.primary = primary;
        b.detail = detail;
        b.selected = selected;
        b.enabled = enabled;
        p.blocks.push_back(b);
    }
    void top(const std::string &s, VJ a) { p.top.push_back(WebControl{s, action(a), false, true}); }
    void tabs(const std::string &selected) {
        for (const auto &pair : std::vector<std::pair<std::string, std::string>>{
                 {"home", "首页"}, {"levels", "词库"}, {"review", "复习"}, {"search", "词典"}})
            p.tabs.push_back(WebControl{pair.second,
                                        action(VJ{{"action", "v-go"}, {"value", pair.first}, {"root", true}}),
                                        pair.first == selected, true});
    }
};
VJ action(const std::string &type) {
    return VJ{{"action", "v-do"}, {"type", type}};
}
std::string lower(std::string s) {
    for (char &c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}
std::string stage_title(const VocabularyCatalog &c, const std::string &id) {
    for (const auto &s : c.metadata["stages"])
        if (s["id"] == id)
            return str(s, "title");
    return "当前词组";
}
} // namespace
void NativeVocabulary::go(const std::string &r) {
    stop_audio();
    note_.clear();
    route_ = r;
    ++viewSerial_;
}
void NativeVocabulary::navigate(const std::string &r) {
    if(r != route_) backStack_.push_back({route_,lookup_,query_});
    go(r);
}
std::string NativeVocabulary::back_label() const {
    if(backStack_.empty()) return "回首页";
    const auto &r=backStack_.back().route;
    return r=="home"?"回首页":r=="word-batch"?"返回学习":r=="review"?"返回复习":r=="notebook"?"返回生词":r=="notebook-words"?"返回列表":r=="search"?"返回查词":"返回";
}
bool NativeVocabulary::back() {
    if (!error_.empty() || !loaded_) { dismiss_error(); return true; }
    if(route_=="word-batch" && str(field(state_,"session"),"phase")=="summary") {
        const auto source=str(field(state_,"session"),"source");
        return dispatch(VJ{{"action","v-return"},{"value",source=="review"?"review":source=="notebook"?"notebook":"home"}}.dump());
    }
    if(!backStack_.empty()) {
        auto f=backStack_.back();
        if(f.route=="search" && !expose_candidates(f.query))return false;
        backStack_.pop_back();
        lookup_=f.lookup;query_=f.query;go(f.route);
    } else go("home");
    return true;
}
bool NativeVocabulary::type(const std::string &text) {
    if (!loaded_ || !error_.empty()) return false;
    if (text.size() > 100)
        return false;
    if (route_ == "search") {
        if(!expose_candidates(text))return false;
        query_ = text;
        ++viewSerial_;
        return true;
    }
    auto t = engine_.current(state_);
    if (route_ != "word-batch" || t.is_null() || !field(state_["session"], "feedback").is_null())
        return false;
    return commit(VJ{{"type", "draft"}, {"taskId", t["uid"]}, {"value", text}});
}
bool NativeVocabulary::edit(const std::string &keys) {
    auto text = route_ == "search" ? query_ : str(field(state_["session"], "draft"), "value");
    auto task = engine_.current(state_);
    if (route_ != "search" &&
        field(field(state_["session"], "draft"), "taskId") != field(task, "uid"))
        text.clear();
    for (char c : keys) {
        if (c == '\b') {
            if (!text.empty())
                text.pop_back();
        } else
            text += c;
    }
    return type(text);
}
bool NativeVocabulary::input(const std::string &kind, const std::string &text) {
    if (kind == "key")
        return edit(shift_ ? [&] {
            auto v = text;
            for (char &c : v)
                c = char(std::toupper((unsigned char)c));
            return v;
        }()
                           : text);
    if (kind == "backspace")
        return edit("\b");
    if (kind == "shift") {
        shift_ = !shift_;
        return true;
    }
    if (kind == "symbols") {
        symbols_ = !symbols_;
        return true;
    }
    if (kind == "submit") {
        if (route_ == "search") {
            for (const auto &e : catalog_.entries)
                if (lower(e.word) == lower(query_)) {
                    if (commit(VJ{{"type", "lookup"}, {"id", e.id}})) {
                        navigate("word");
                        lookup_ = e.id;
                    }
                    return true;
                }
            note_ = "未收录，可点“完整词典”";
            return true;
        }
        return dispatch(VJ{{"action", "v-answer"}}.dump());
    }
    return false;
}
void NativeVocabulary::stop_audio() {
    if (!audioPath_.empty()) {
        pendingAudio_ = VJ{{"stop", true}, {"serial", ++audioSerial_}};
        audioPath_.clear();
    }
}
void NativeVocabulary::play(const std::string &id, bool example) {
    auto w = catalog_.get(id);
    auto key = example ? "exampleAudio" : "audio";
    if (field(w, key).is_null()) {
        note_ = "此条暂未配置例句录音";
        return;
    }
    auto p = "vocabulary/" + str(w[key], "path");
    if (audioPath_ == p) {
        stop_audio();
        note_ = "已停止播放";
        return;
    }
    if (!commit(VJ{{"type", "audio"}, {"id", id}}))
        return;
    audioPath_ = p;
    audioText_ = str(w[key], "text");
    if (audioText_.empty())
        audioText_ = str(w, example ? "example" : "word");
    pendingAudio_ = VJ{{"path", p}, {"serial", ++audioSerial_}};
    note_ = "SD 合成录音";
}
bool NativeVocabulary::take_audio(std::string &p, int &serial, bool &stop, double *rate) {
    if (pendingAudio_.is_null())
        return false;
    p = str(pendingAudio_, "path");
    serial = int(num(pendingAudio_, "serial"));
    stop = flag(pendingAudio_, "stop");
    if (rate)
        *rate = 1;
    pendingAudio_ = nullptr;
    return true;
}
std::string NativeVocabulary::audio_text(const std::string &p) const {
    return p == audioPath_ ? audioText_ : "";
}
bool NativeVocabulary::playback(int serial, bool success) {
    if (serial != audioSerial_)
        return false;
    audioPath_.clear();
    note_ = success ? "播放完成" : "播放失败，可重新试听";
    return true;
}
bool NativeVocabulary::dispatch(const std::string &raw) {
    try {
        const auto a = VJ::parse(raw);
        auto name = str(a, "action"), value = str(a, "value");
        if (name == "go" && str(a, "route") == "books") {
            stop_audio();
            return false;
        }
        if (name == "v-close-error" || name == "v-check-recovery") {
            retry_recovery(); return true;
        }
        if (name == "v-dismiss-error") { dismiss_error(); return true; }
        if (name == "v-error-home") { dismiss_error(true); return true; }
        if (name == "v-back" && (!error_.empty() || !loaded_)) return back();
        if ((name == "w-day-home" || (name == "v-go" && value == "home")) && (!error_.empty() || !loaded_)) {
            dismiss_error(true); return true;
        }
        if (!error_.empty() || !loaded_) return true;
        if(name=="v-import") return import_word(VJ::parse(value));
        if(name=="v-list-read") { prepare_word_list(); return true; }
        if(name=="v-list-save") return accept_word_list(uint64_t(num(a,"token")));
        if(name=="v-lookup-exposure") return expose_lookup(value);
        if (name == "w-day-home") {
            backStack_.clear(); go("home");
            return true;
        }
        if (name == "lookup") {
            navigate("search");
            query_.clear();
            return true;
        }
        if (name == "u-notebook") {
            navigate("notebook");
            return true;
        }
        if (name == "v-go") {
            if(value=="search" && !expose_candidates(query_))return false;
            if(value=="notebook-words"){const auto ids=state_["notebook"];for(const auto &id:ids){const auto *entry=catalog_.entry(str(id));if(entry&&!expose_lookup(entry->word))return false;}}
            if(flag(a,"root") || value=="home") { backStack_.clear();go(value); }
            else navigate(value);
            if(value=="word-list"){wordListError_.clear();wordListReady_=false;wordList_=VJ::array();wordListIssues_=VJ::array();++wordListToken_;}
            if(value=="levels") stageChoice_=str(state_,"selectedStage");
            return true;
        }
        if(name=="v-return") {
            if(value!="review"&&value!="notebook"&&value!="home")return true;
            if(value!="home")for(size_t i=backStack_.size();i>0;--i)if(backStack_[i-1].route==value){
                auto frame=backStack_[i-1];backStack_.resize(i-1);
                lookup_=frame.lookup;query_=frame.query;go(value);return true;
            }
            backStack_.clear();go(value);return true;
        }
        if (name == "v-back")
            return back();
        if (name == "v-open") {
            if (commit(VJ{{"type", "lookup"}, {"id", value}})) {
                navigate("word");
                lookup_ = value;
            }
            return true;
        }
        if (name == "v-audio") {
            play(str(a, "id"), flag(a, "example"));
            return true;
        }
        if (name == "v-start") {
            const auto intent=field(a,"intent").is_string()?str(a,"intent"):(route_=="review"?"review":"new");
            if(intent=="new"&&!flag(state_,"stageChosen")){stageChoice_=str(state_,"selectedStage");navigate("levels");return true;}
            if(commit(VJ{{"type","begin"},{"intent",intent},{"allowExtra",flag(a,"allowExtra")}})) navigate("word-batch");
            return true;
        }
        if (name == "v-stage") { stageChoice_=value; ++viewSerial_; return true; }
        if(name=="v-use-stage") {
            if(!flag(state_,"stageChosen") || value!=str(state_,"selectedStage"))
                if(!commit(VJ{{"type","chooseStage"},{"stage",value}})) return true;
            backStack_.clear();go("home");return true;
        }
        if (name == "v-mode" || name == "v-count") {
            auto setting = state_["settings"];
            if (name == "v-mode")
                setting["mode"] = value;
            else
                setting["dailyNew"] = std::stoi(value);
            setting["type"] = "settings";
            commit(setting);
            return true;
        }
        auto task = engine_.current(state_);
        const auto &ss = field(state_, "session");
        if (name == "v-answer" || name == "w-unknown") {
            VJ answer{{"type", "answer"},
                      {"taskId", a.contains("taskId") ? a["taskId"] : field(task, "uid")},
                      {"value", field(a, "value").is_string() ? a["value"]
                                                              : field(field(ss, "draft"), "value")},
                      {"unknown", name == "w-unknown"}};
            commit(answer);
            return true;
        }
        if (name == "v-do") {
            auto cmd = a;
            cmd.erase("action");
            auto type = str(cmd, "type");
            if(type=="learnNotebook") cmd["explicit"]=true;
            if (type == "learnNext" && !cmd.contains("sessionId"))
                cmd["sessionId"] = field(ss, "id");
            if (type == "continue" && !cmd.contains("taskId"))
                cmd["taskId"] = field(field(ss, "feedback"), "uid");
            if (type == "hint" && !cmd.contains("taskId"))
                cmd["taskId"] = field(task, "uid");
            if (commit(cmd)) {
                if (type == "resumePaused" || type == "learnNotebook")
                    navigate("word-batch");
                else if(type=="removeNotebook"){back();note_="已移出，复习进度保留";}
                else if (type == "notebook")
                    note_ = "已加入生词本";
            }
            return true;
        }
        return false;
    } catch (const std::exception &e) {
        capture_error(e);
        return true;
    }
}
WebPage NativeVocabulary::recovery_page() {
    WebPage p; View v{p}; p.route="recovery";
    p.key="vocabulary:recovery:"+std::to_string(int(failure_))+":"+std::to_string(viewSerial_);
    p.title = recoveryHome_ ? "学习暂不可用" : failure_==VocabularyFailure::Clock ? "请校准时间" :
              failure_==VocabularyFailure::Storage ? "这一步未能保存" : "这一步未完成";
    v.top(loaded_ ? "返回原页" : "返回",VJ{{"action","v-dismiss-error"}});
    if (failure_==VocabularyFailure::Clock) {
        v.text("日期异常，学习记录暂不更新。","meaning");
        v.text("校准后回到原题，不重置进度。","meaning");
        v.text("设备时间："+recovery_time(now_),"meaning");
        if(loaded_ && num(state_,"lastClock")>0) v.text("上次记录："+recovery_time(num(state_,"lastClock")),"meaning");
        v.button("校准日期",VJ{{"action","v-device-date"}},true,"foot");
        v.button("已校准，检查时间",VJ{{"action","v-check-recovery"}},false,"foot");
    } else if (failure_==VocabularyFailure::Storage) {
        v.text("刚才这一步尚未确认保存。","meaning");
        v.text("原题和已保存的记录不变。","meaning");
        v.text("请检查 SD 卡的空间和连接。","meaning");
        v.button("检查存储",VJ{{"action","v-check-recovery"}},true,"foot");
    } else {
        v.text("本次操作没有完成，已有文件未被重置。","meaning");
        v.text("可先返回。记录异常时请保留原文件，再检查存储和资源。","meaning");
        v.button(loaded_ ? "返回原页面" : "重新检查",VJ{{"action",loaded_ ? "v-dismiss-error" : "v-check-recovery"}},true,"foot");
    }
    if (!error_.empty()) v.text(error_,"detail");
    if (loaded_) v.button("返回首页",VJ{{"action","v-error-home"}},false,"foot");
    return p;
}
WebPage NativeVocabulary::page() try {
    if (!error_.empty() || !loaded_) return recovery_page();
    WebPage p;
    View v{p};
    p.route = route_;
    p.shift = shift_;
    p.symbols = symbols_;
    p.toast = note_;
    const auto &ss = field(state_, "session");
    auto task = loaded_ ? engine_.current(state_) : VJ();
    auto phase = str(ss, "phase");
    v.taskId = str(task, "uid");
    v.sessionId = str(ss, "id");
    v.learnAt = num(ss, "learnAt");
    p.key = "vocabulary:" + route_ + ":" + str(ss, "id") + ":" + phase + ":" + str(task, "uid") +
            ":" + (field(ss, "feedback").is_null() ? "question" : "feedback") + ":" +
            str(ss, "learnAt") + ":" + (route_=="word"?lookup_:"");
    const auto title = stage_title(catalog_, str(state_, "selectedStage"));
    auto due = engine_.available_due(state_, now_);
    const auto &today = field(state_["days"], VocabularyEngine::date(now_));
    auto startAction=[](const std::string &intent){return VJ{{"action","v-start"},{"intent",intent},{"allowExtra",intent=="new"}};};
    if (route_ == "home") {
        p.title = "小小单词卡";
        v.top("设置", VJ{{"action", "v-go"}, {"value", "settings"}});
        v.text(title, "vocab-home-stage", true);
        v.text("今天已学 " + n(field(today, "new").size()) + " / " +
                   str(state_["settings"], "dailyNew") + " 个新词",
               "vocab-home-today");
        v.text(!engine_.resumable(state_,"review").is_null() ? "有未完成复习，可在复习页继续" : due.empty() ? "暂时没有到期复习"
                           : "到期复习 " + n(due.size()) + " 项", "vocab-home-due");
        std::set<std::string> days;
        for (auto it = state_["days"].begin(); it != state_["days"].end(); ++it)
            if (num(it.value(), "answers") > 0)
                days.insert(it.key());
        for (const auto &d : field(field(field(state_, "legacy"), "days"), "days"))
            days.insert(str(d));
        int streak = 0;
        auto at = now_;
        if (!days.count(VocabularyEngine::date(at)))
            at -= 86400000;
        while (days.count(VocabularyEngine::date(at))) {
            ++streak;
            at -= 86400000;
        }
        v.text("累计已学 " + n(state_["seen"].size()) + " 词 · 学习 " + n(days.size()) + " 天", "vocab-home-history");
        if (streak)
            v.text("连续学习 " + n(streak) + " 天", "vocab-home-streak");
        auto nextGroup=engine_.resumable(state_,"new");
        const bool active=!nextGroup.is_null();
        std::string next="每组几个词，学完再一起检查";
        if(active) {
            auto local=state_;local["session"]=nextGroup;auto nextTask=engine_.current(local);
            if(str(nextGroup,"phase")=="learn") next="认识单词 · "+n(num(nextGroup,"learnAt")+1)+"/"+n(nextGroup["ids"].size());
            else if(!nextTask.is_null()) {
                const auto kind=str(nextTask,"kind");int total=0,done=0;
                for(const auto&q:nextGroup["tasks"])if(str(q,"kind")==kind&&flag(q,"followup")==flag(nextTask,"followup")){++total;if(flag(q,"done"))++done;}
                const bool feedback=!field(nextGroup,"feedback").is_null();
                next=(kind=="meaning"?"词义检查":kind=="gap"?"补字母":"完整拼写")+std::string(" · ")+n(std::min(feedback?std::max(done,1):done+1,total))+"/"+n(total);
                if(feedback) next+=" · 查看反馈";
            }
        }
        const bool goalDone=int64_t(field(today,"new").size())>=num(state_["settings"],"dailyNew");
        bool exhausted=true;
        for(const auto&e:catalog_.entries)if(e.eligible&&e.stage==str(state_,"selectedStage")&&!state_["seen"].contains(e.id)){exhausted=false;break;}
        const bool blocked=!active&&!exhausted&&engine_.available_new(state_).empty();
        const bool blockedNotebook=blocked&&str(ss,"source")=="notebook"&&str(ss,"phase")!="summary";
        const bool chooseNext=!active&&exhausted&&flag(state_,"stageChosen");
        if(!active&&goalDone)next="已达今日目标，还可以再学一组";
        if(chooseNext)next="本档新词已学完，可以选择下一档";
        if(blocked)next=blockedNotebook?"剩余新词在生词本词组中，可回去继续":"剩余新词在暂停词组中，可继续原词组";
        const bool clockInvalid=now_<1704067200000LL || now_>4102444800000LL || now_<num(state_,"lastClock")-300000;
        if(storageBlocked_) next="存储待检查，原题与进度保持";
        else if(clockInvalid) next="先校准日期，再继续原题";
        v.text(next,"vocab-home-next");
        auto label=!flag(state_,"stageChosen")?"选择起点":active?"继续学习":chooseNext?"选择下一词库":blocked?(blockedNotebook?"返回生词本":"查看暂停词组"):goalDone?"再学一组":"开始背词";
        if(storageBlocked_) v.button("检查存储",VJ{{"action","v-check-recovery"}},true,"foot");
        else if(clockInvalid) v.button("校准日期",VJ{{"action","v-device-date"}},true,"foot");
        else v.button(label,chooseNext?VJ{{"action","v-go"},{"value","levels"}}:blocked?VJ{{"action","v-go"},{"value",blockedNotebook?"notebook":"paused"}}:startAction("new"),true,"foot");
        v.tabs("home");
        return p;
    }
    if (route_ == "levels") {
        p.title="选择词库";v.top(back_label(),VJ{{"action","v-back"}});
        const auto selected=stageChoice_.empty()?str(state_,"selectedStage"):stageChoice_;
        p.key+=":"+selected;
        for(const auto&s:catalog_.metadata["stages"]){
            const auto id=str(s,"id");
            const bool trial=num(s,"ready")<num(s,"inventoryCount");
            v.button(str(s,"title")+" · "+str(s,"ready")+" 词"+(trial?"（试学）":""),VJ{{"action","v-stage"},{"value",id}},false,"body","",id==selected);
        }
        v.button("我的单词本",VJ{{"action","v-go"},{"value","notebook"}},false,"body","学习自己指定的单词");
        const bool same=flag(state_,"stageChosen")&&selected==str(state_,"selectedStage");
        v.button(same?"使用当前词库":"使用所选词库",VJ{{"action","v-use-stage"},{"value",selected}},true,"foot");
        v.tabs("levels");return p;
    }
    if (route_ == "settings") {
        p.title = "背词设置";
        v.top(back_label(), VJ{{"action", "v-back"}});
        v.text("训练方式", "", true);
        for (const auto &pair : std::vector<std::pair<std::string, std::string>>{
                 {"staged", "分轮拼写"}, {"full", "直接完整拼写"}, {"meaning", "只练词义"}})
            v.button(pair.second, VJ{{"action", "v-mode"}, {"value", pair.first}}, false, "body",
                     "", str(state_["settings"], "mode") == pair.first);
        v.text("每日新词", "", true);
        for (auto count : {5, 10, 20})
            v.button(n(count) + " 个", VJ{{"action", "v-count"}, {"value", n(count)}}, false,
                     "body", "", num(state_["settings"], "dailyNew") == count);
        v.text("修改从下一组生效。", "detail");
        if(!state_["paused"].empty())v.button("已暂停的词组",VJ{{"action","v-go"},{"value","paused"}});
        return p;
    }
    if (route_ == "review") {
        p.title="复习";v.top(back_label(),VJ{{"action","v-back"}});
        auto resume=engine_.resumable(state_,"review");int meaning=0,spelling=0;
        if(resume.is_null()) { for(const auto&q:due)(str(q,"kind")=="meaning"?meaning:spelling)++; }
        else for(const auto&q:resume["tasks"])if(!flag(q,"done"))(str(q,"kind")=="meaning"?meaning:spelling)++;
        v.text(!resume.is_null()?"上次的复习还没完成":due.empty()?"暂时没有到期复习":"巩固已经学过的词","",true);
        v.text("词义 "+n(meaning)+" 项 · 拼写 "+n(spelling)+" 项","meaning");
        v.text("这里仅复习，不加入词库中的新词。","detail");
        v.button("我的单词本 · "+n(state_["notebook"].size())+" 词",VJ{{"action","v-go"},{"value","notebook"}},false,"foot");
        const bool can=!resume.is_null()||!due.empty();
        v.button(!resume.is_null()?"继续复习":!due.empty()?"开始复习":"暂无到期复习",startAction("review"),true,"foot","",false,can);
        v.tabs("review");return p;
    }
    if(route_=="paused") {
        p.title="已暂停的词组";v.top(back_label(),VJ{{"action","v-back"}});
        for(const auto&paused:state_["paused"]){auto a=action("resumePaused");a["id"]=paused["id"];
            const auto source=str(paused,"source");auto label=source=="review"?"到期复习":source=="notebook"?"生词本":"新词学习";
            v.button(std::string(label)+" · "+n(paused["ids"].size())+" 词",a);
        }
        v.button(back_label(),VJ{{"action","v-back"}},true,"foot");return p;
    }
    if(route_=="word-list") {
        p.title="添加单词表";v.top(back_label(),VJ{{"action","v-back"}});
        if(!wordListReady_) {
            if(!wordListError_.empty()) v.text(wordListError_,"meaning");
            v.text("只需要英文，每行一个词。","",true);
            v.text("把 my-words.txt 放到 SD 卡的 metalio/e-ink/wordcards 文件夹。安全退出 U 盘后，再点下方读取。","meaning");
            v.text("可以包含短语，一次最多 200 行。释义和音标由设备词典匹配。","detail");
            v.button("读取单词表",VJ{{"action","v-list-read"}},true,"foot");
        } else {
            v.text("可以添加 "+n(wordList_.size())+" 个词","",true);
            v.text("已有 "+n(wordListExisting_)+" 个 · 待确认 "+n(wordListIssues_.size())+" 行","meaning");
            v.text("加入后沿用当前背词和复习方式。已有进度保留。","detail");
            if(!wordListIssues_.empty())v.button("查看未匹配的行",VJ{{"action","v-go"},{"value","word-list-issues"}});
            v.button("重新读取",VJ{{"action","v-list-read"}},false,"foot");
            if(!wordList_.empty())v.button("加入我的单词本",VJ{{"action","v-list-save"},{"token",wordListToken_}},true,"foot");
            else v.button("返回单词本",VJ{{"action","v-return"},{"value","notebook"}},true,"foot");
        }
        return p;
    }
    if(route_=="word-list-issues") {
        p.title="未匹配的行";v.top("返回结果",VJ{{"action","v-back"}});
        for(const auto &row:wordListIssues_)v.text("第 "+str(row,"line")+" 行："+str(row,"reason"),"meaning");
        v.button("返回匹配结果",VJ{{"action","v-back"}},true,"foot");return p;
    }
    if (route_ == "notebook") {
        p.title = "我的单词本";
        v.top(back_label(), VJ{{"action", "v-back"}});
        v.top("添加",VJ{{"action","v-go"},{"value","notebook-add"}});
        if (state_["notebook"].empty()){
            v.text("把想学的单词放在这里。", "", true);
            v.text("输入单词，或添加照片识别出的单词表。词义、音标和发音沿用设备词典。","meaning");
            v.button("添加单词",VJ{{"action","v-go"},{"value","notebook-add"}},true,"foot");
        }
        else {
            const auto saved=engine_.resumable(state_,"notebook");
            std::set<std::string> reserved;
            auto reserve=[&](const VJ&group){if(group.is_object()&&str(group,"phase")!="summary")for(const auto&id:field(group,"ids"))reserved.insert(str(id));};
            reserve(ss);for(const auto&group:state_["paused"])reserve(group);
            bool fresh=false,occupied=false;
            for(const auto&id:state_["notebook"])if(!state_["seen"].contains(str(id))){if(reserved.count(str(id)))occupied=true;else fresh=true;}
            if(!saved.is_null()||fresh)v.button(saved.is_null()?"学习这些生词":"继续生词学习",action("learnNotebook"),true,"foot");
            else v.button(occupied?"返回学习":"去复习",VJ{{"action","v-go"},{"value",occupied?"home":"review"}},true,"foot");
        }
        if(!state_["notebook"].empty()) {
            v.text("自己想学的 "+n(state_["notebook"].size())+" 个词","",true);
            v.text("新词每组最多 5 个。学过的词保留原来的复习时间。","meaning");
            v.button("查看和整理单词",VJ{{"action","v-go"},{"value","notebook-words"}});
        }
        return p;
    }
    if(route_=="notebook-words") {
        p.title="我的单词";v.top("返回生词",VJ{{"action","v-back"}});
        for (const auto &id : state_["notebook"]) {
            auto e = catalog_.entry(str(id));
            if (e)
                v.button(e->word, VJ{{"action", "v-open"}, {"value", id}}, false, "body",
                         e->meaning);
        }
        return p;
    }
    if(route_=="notebook-add") {
        p.title="添加单词";v.top(back_label(),VJ{{"action","v-back"}});
        v.text("只输入英文即可。","",true);
        v.text("词典会提供词义和音标，发音沿用现有音频与离线合成。","meaning");
        v.button("从单词表添加",VJ{{"action","v-go"},{"value","word-list"}},false,"foot");
        v.button("输入单词",VJ{{"action","v-go"},{"value","search"}},true,"foot");
        return p;
    }
    if (route_ == "search") {
        p.title = "词典";
        v.top(back_label(), VJ{{"action", "v-back"}});
        p.keyboard = true;
        p.typed = query_;
        p.prompt = "输入英文单词";
        p.can_submit = !query_.empty();
        auto q = lower(query_);
        int count = 0;
        if (!q.empty())
            for (const auto &e : catalog_.entries)
                if (lower(e.word).rfind(q, 0) == 0 && count++ < 2)
                {
                    v.button(e.word, VJ{{"action", "v-open"}, {"value", e.id}}, false, "body", e.meaning);
                    if(count==2)break;
                }
        return p;
    }
    if (route_ == "word") {
        auto w = catalog_.get(lookup_);
        p.title = "词义与发音";
        v.top(back_label(), VJ{{"action", "v-back"}});
        v.text(str(w, "word"), "word", true);
        v.text(str(w, "ipa"), "ipa");
        v.text(str(w, "pos") + " " + str(w, "meaning"), "meaning");
        v.button("听单词", VJ{{"action", "v-audio"}, {"id", lookup_}, {"example", false}}, false, "foot", "", false, true);
        v.text(str(w, "example"), "example");
        v.text(str(w, "exampleZh"), "detail");
        if(!str(w,"dictionaryDefinition").empty())v.text("词典完整释义："+str(w,"dictionaryDefinition"),"detail");
        if (!field(w, "exampleAudio").is_null())
            v.button("听例句", VJ{{"action", "v-audio"}, {"id", lookup_}, {"example", true}}, false, "foot", "", false, true);
        v.text(str(w, "pronunciationNote"));
        auto a = action("notebook");
        a["id"] = lookup_;
        bool saved = has(state_["notebook"], lookup_);
        const bool fromNotebook=!backStack_.empty()&&(backStack_.back().route=="notebook"||backStack_.back().route=="notebook-words");
        if(saved&&fromNotebook){a["type"]="removeNotebook";v.button("移出生词本",a,true,"foot");}
        else v.button(saved ? "已加入生词本" : "加入生词本", a, true, "foot", "", false, !saved);
        return p;
    }
    if (route_ == "word-batch") {
        if (ss.is_null()) {
            go("home");
            return page();
        }
        if (phase == "summary") {
            p.route = "word-result";
            p.title = "这一组完成";
            const auto source=str(ss,"source");
            const auto destination=source=="review"?"review":source=="notebook"?"notebook":"home";
            v.top(source=="review"?"返回复习":source=="notebook"?"返回生词":"回首页",VJ{{"action","v-return"},{"value",destination}});
            if(num(ss,"answered")==0)p.title="本组已记录";
            v.text(source=="review"?"完成 "+str(ss,"answered")+" 次复习":
                   "新学 "+str(ss,"newCount")+" 词 · 完成 "+str(ss,"answered")+" 次练习","meaning");
            int pending = 0;
            for (auto it = state_["cards"].begin(); it != state_["cards"].end(); ++it)
                for (auto c = it.value().begin(); c != it.value().end(); ++c)
                    if (flag(c.value(), "pendingRelearn"))
                        for (const auto &t : ss["tasks"])
                            if (str(t, "id") == it.key() && str(t, "kind") == c.key()) {
                                ++pending;
                                break;
                            }
            v.text(pending ? n(pending) + " 项待巩固，已安排稍后复习"
                           : "本组已记录，下次到期再检查。");
            if(source=="review") {
                if(!due.empty()||!engine_.resumable(state_,"review").is_null()) {
                    v.button("返回复习",VJ{{"action","v-return"},{"value","review"}},false,"foot");
                    v.button("继续复习",startAction("review"),true,"foot");
                } else v.button("返回复习",VJ{{"action","v-return"},{"value","review"}},true,"foot");
            } else if(source=="notebook") {
                const bool more=!engine_.available_notebook(state_).empty()||!engine_.resumable(state_,"notebook").is_null();
                v.button("返回生词本",VJ{{"action","v-return"},{"value","notebook"}},!more,"foot");
                if(more)v.button("下一组生词",action("learnNotebook"),true,"foot");
            } else {
                bool unseen=false;for(const auto&e:catalog_.entries)if(e.eligible&&e.stage==str(state_,"selectedStage")&&!state_["seen"].contains(e.id)){unseen=true;break;}
                const bool available=!engine_.available_new(state_).empty()||!engine_.resumable(state_,"new").is_null();
                v.button("回首页",VJ{{"action","v-return"},{"value","home"}},false,"foot");
                v.button(available?"再学一组":unseen?"查看暂停词组":"选择下一词库",available?startAction("new"):VJ{{"action","v-go"},{"value",unseen?"paused":"levels"}},true,"foot");
            }
            return p;
        }
        v.top(back_label(), VJ{{"action", "v-back"}});
        v.top("查词", VJ{{"action", "lookup"}});
        auto id = phase == "learn" ? str(ss["ids"][size_t(num(ss, "learnAt"))]) : str(task, "id");
        auto w = catalog_.get(id);
        auto kind = str(task, "kind");
        int roundAt = 1, roundTotal = 0;
        for (const auto &q : ss["tasks"])
            if (str(q, "kind") == kind && flag(q, "followup") == flag(task, "followup")) {
                ++roundTotal;
                if (flag(q, "done")) ++roundAt;
            }
        const auto roundCount = " " + n(roundAt) + "/" + n(roundTotal);
        auto feedback = field(ss, "feedback");
        if (phase == "learn") {
            p.title = "认识单词 " + n(num(ss, "learnAt") + 1) + " / " + n(ss["ids"].size());
            v.text(str(w, "word"), "word", true);
            v.text(str(w, "ipa"), "ipa");
            v.text(str(w, "pos") + " " + str(w, "meaning"), "meaning");
            v.button("听单词", VJ{{"action", "v-audio"}, {"id", id}, {"example", false}}, false, "foot", "", false, true);
            v.text(str(w, "example"), "example");
            v.text(str(w, "exampleZh"), "detail");
        if(!str(w,"dictionaryDefinition").empty())v.text("词典完整释义："+str(w,"dictionaryDefinition"),"detail");
            if (!field(w, "exampleAudio").is_null())
                v.button("听例句", VJ{{"action", "v-audio"}, {"id", id}, {"example", true}}, false, "foot", "", false, true);
            v.text(str(w, "pronunciationNote"));
            v.button(num(ss, "learnAt") + 1 < int64_t(ss["ids"].size()) ? "下一个" : "开始检查",
                     action("learnNext"), true, "foot");
            return p;
        }
        if (!feedback.is_null()) {
            p.title = flag(feedback, "correct")
                          ? (flag(feedback, "assisted") ? "提示后答对" : "答对了")
                          : "再记一遍";
            v.text(str(w, "word"), "word", true);
            v.text(str(w, "ipa"), "ipa");
            v.text(str(w, "meaning"), "meaning");
            v.text(str(w, "example"), "detail");
            v.text(str(w, "exampleZh"), "detail");
        if(!str(w,"dictionaryDefinition").empty())v.text("词典完整释义："+str(w,"dictionaryDefinition"),"detail");
            v.button("听单词", VJ{{"action", "v-audio"}, {"id", id}, {"example", false}}, false, "foot", "", false, true);
            v.text(!flag(feedback, "correct") || flag(feedback, "assisted")
                       ? "隔开其他词后再检查，不马上重答。"
                   : kind == "gap" ? "补字母完成，之后再练完整拼写。"
                                   : "已记录，继续下一个。",
                   "source");
            const auto revision=uint64_t(num(state_,"revision"));
            if(continuationRevision_!=revision||continuationClock_!=now_/1000) {
                continuationPreview_=engine_.continue_preview(state_,now_);
                continuationRevision_=revision;continuationClock_=now_/1000;
            }
            const auto nextKind=str(continuationPreview_,"kind");
            const auto nextLabel=str(continuationPreview_,"phase")=="summary"?"查看小结":
                flag(continuationPreview_,"followup")&&!flag(task,"followup")?"开始巩固":
                nextKind!=kind&&nextKind=="gap"?"开始补字母":
                nextKind!=kind&&nextKind=="spelling"?"开始完整拼写":"下一题";
            v.button(nextLabel, action("continue"), true, "foot");
            return p;
        }
        if (kind == "meaning") {
            p.title = (flag(task, "followup") ? "词义巩固" : "词义检查") + roundCount;
            v.text(str(w, "word"), "word", true);
            v.text(str(w, "example"), "example");
            for (const auto &opt : engine_.choices(task, ss))
                v.button(str(opt), VJ{{"action", "v-answer"}, {"value", opt}}, false, "foot");
            v.button("不认识", VJ{{"action", "w-unknown"}}, false, "foot");
            return p;
        }
        p.title = (kind == "gap" ? "补上字母" : "完整拼写") + roundCount;
        p.keyboard = true;
        p.prompt = str(w, "meaning");
        p.typed =
            field(field(ss, "draft"), "taskId") == task["uid"] ? str(ss["draft"], "value") : "";
        p.can_submit = !p.typed.empty();
        if (kind == "gap") {
            p.gap_mask = str(engine_.gap(task), "mask");
            for (size_t at; (at = p.gap_mask.find("＿")) != std::string::npos;)
                p.gap_mask.replace(at, 3, "_");
        }
        v.text(str(w, "meaning"), "meaning");
        if (flag(task, "answerRevealed"))
            v.text(str(w, "word"), "word");
        v.button("听发音", VJ{{"action", "v-audio"}, {"id", id}, {"example", false}});
        // "想不起来" submits an unknown response and shows feedback directly.
        // Existing revealed drafts remain visible above, but a new learner is
        // never asked to copy a just-revealed answer before progressing.
        return p;
    }
    p.title = "学习";
    v.button("回首页", VJ{{"action", "v-go"}, {"value", "home"}}, true);
    return p;
} catch(const std::exception &e) {
    capture_error(e, VocabularyFailure::Data); return recovery_page();
}
} // namespace wordcard
