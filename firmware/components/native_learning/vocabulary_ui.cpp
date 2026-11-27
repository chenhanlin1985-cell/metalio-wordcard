#include "vocabulary.hpp"
#include "bookshop.hpp"
#include "vocabulary_journey.hpp"
#include "vocabulary_picture_index.hpp"
#include <cctype>
#include <set>
#include <ctime>
namespace wordcard {
namespace {
std::string mode_name(const std::string &mode) {
    return mode=="meaning"?"简单":mode=="partial"?"普通":"困难";
}
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
#include "word_roots.inc"
struct View {
    WebPage &p;
    std::string taskId = {}, sessionId = {};
    int64_t learnAt = 0;
    int action(VJ a) {
        const auto name = str(a, "action"), type = str(a, "type");
        if (name == "v-answer" || (name == "v-do" && (type == "continue" || type == "hint" || type == "associationHint")))
            a["taskId"] = taskId;
        if (name == "v-do" && (type == "learnNext" || type == "learnPrev")) {
            a["sessionId"] = sessionId;
            a["learnAt"] = learnAt;
        }
        p.actions.push_back(a.dump());
        return int(p.actions.size() - 1);
    }
    void text(const std::string &s, const std::string &role = "", bool heading = false, const std::string &detail = "") {
        if (s.empty())
            return;
        WebBlock b;
        b.kind = heading ? "heading" : "text";
        b.text = s;
        b.area = "body";
        b.role = role;
        b.detail = detail;
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
        if (str(a,"action")=="v-browse" || str(a, "type") == "learnPrev" || str(a, "type") == "learnNext") b.role = "word-nav";
        b.primary = primary;
        b.detail = detail;
        b.selected = selected;
        b.enabled = enabled;
        p.blocks.push_back(b);
    }
    void top(const std::string &s, VJ a) { p.top.push_back(WebControl{s, action(a), false, true}); }
    void tabs(const std::string &selected) {
        for (const auto &pair : std::vector<std::pair<std::string, std::string>>{
                 {"home", "首页"}, {"levels", "内容库"}, {"review", "复习"}, {"search", "词典"}})
            p.tabs.push_back(WebControl{pair.second,
                                        action(pair.first=="levels"?VJ{{"action","pack-library"}}:VJ{{"action", "v-go"}, {"value", pair.first}, {"root", true}}),
                                        pair.first == selected, true});
    }
    void picture(const std::string &id,const std::string &directory) {
        const auto *art=vocabulary_picture(id);if(!art||p.blocks.empty())return;
        auto &b=p.blocks.back();b.picture=art->tile;b.picture_source=directory+(art->pack==2?"/pictures-v2.bin":"/pictures-v1.bin");
        b.picture_width=art->width;b.picture_height=art->height;
    }
    void card(const VJ &w, const std::string &id, const std::string &directory, bool illustrate) {
        text(str(w,"word"), "card-word", true);
        if (illustrate) picture(id,directory);
        text(str(w,"ipa"), "ipa");
        const auto pos = str(w,"pos"), meaning = str(w,"meaning");
        text(pos.empty() || meaning.rfind(pos,0)==0 ? meaning : pos+" "+meaning, "meaning");
        const auto root = word_root(w);
        if (!root.is_null()) {
            text(str(root,"root"), "card-root");
            text(str(root,"connection"), "detail");
            text(str(root,"family"), "detail");
        }
        const auto cue = str(w,"memoryCue");
        if (cue != str(w,"exampleZh")) text(cue,"detail");
        const auto full = str(w,"dictionaryDefinition");
        if (!full.empty() && full != meaning) text(full,"detail");
        text(str(w,"pronunciationNote"), "detail");
        button("听单词", VJ{{"action","v-audio"},{"id",id},{"example",false}}, false, "foot");
        const auto sentence = str(w,"example");
        if (!sentence.empty()) {
            text(sentence, "card-example", false, str(w,"exampleZh"));
            auto &sample = p.blocks.back();
            sample.area = "foot";
            const auto path = str(field(w,"exampleAudio"),"path");
            if (!path.empty()) sample.example_audio = path.rfind("packs/",0)==0 ? path : "vocabulary/"+path;
        } else if (!field(w,"exampleAudio").is_null())
            button("听例句", VJ{{"action","v-audio"},{"id",id},{"example",true}}, false, "foot");
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
std::string stage_title(const VocabularyCatalog &c, const std::string &id, const char *fallback="当前词组") {
    for (const auto &s : c.metadata["stages"])
        if (s["id"] == id)
            return str(s, "title");
    return fallback;
}
// Old stamps have word IDs but no book label. Resolve only the visible stamps from the index.
std::vector<std::string> stamp_books(const VocabularyCatalog &catalog, const VJ &stamp) {
    if(!str(stamp,"contentTitle").empty())return {str(stamp,"contentTitle")};
    if(str(stamp,"source")=="notebook" || str(stamp,"scope")=="notebook") return {"我的单词本"};
    std::vector<std::string> books;
    for(const auto &id:stamp["ids"]) {
        const auto *entry=catalog.entry(str(id));
        const auto book=!entry?std::string("来源待确认"):entry->stage=="personal"?std::string("我的单词本"):
                        stage_title(catalog,entry->stage,"来源待确认");
        if(std::find(books.begin(),books.end(),book)==books.end())books.push_back(book);
    }
    return books;
}
} // namespace
#include "learned_words.inc"
VJ NativeVocabulary::album_stamps() const {
    auto stamps=vocabulary_stamps(state_);
    for(const auto &book:field(state_,"storyRuns"))for(const auto &s:field(book,"stamps"))stamps.push_back(s);
    std::stable_sort(stamps.begin(),stamps.end(),[](const VJ &a,const VJ &b){return num(a,"finishedAt")<num(b,"finishedAt");});
    return stamps;
}
void NativeVocabulary::go(const std::string &r) {
    stop_audio();
    note_.clear();
    if(r=="review" && (route_=="home" || route_=="levels" || route_=="notebook"))
        reviewScope_=route_=="notebook"?"notebook":"current";
    route_ = r;
    continuationRevision_=~uint64_t(0);
    ++viewSerial_;
}
void NativeVocabulary::navigate(const std::string &r) {
    if(r != route_) backStack_.push_back({route_,lookup_,query_});
    go(r);
}
std::string NativeVocabulary::back_label() const {
    if(backStack_.empty()) return "回首页";
    const auto &r=backStack_.back().route;
    return r=="history-groups"?"返回词组":r=="history"?"返回已学单词":r=="history-card"?"返回词卡":r=="stamp"?"返回印章":r=="home"?"回首页":r=="word-batch"?"返回学习":r=="review"?"返回复习":r=="notebook"?"返回生词":r=="notebook-words"?"返回列表":r=="search"?"返回查词":r=="gallery"?"返回图鉴":r=="collection"?"回收集册":"返回";
}
bool NativeVocabulary::expose_journey(const std::string &route) {
    if(route!="gallery"&&route!="stamp")return true;
    std::vector<std::string> ids;std::set<std::string> seen;
    for(const auto &st:album_stamps())if(route=="gallery"||str(st,"sessionId")==journeyStamp_)
        for(const auto &id:st["ids"]){const auto *entry=catalog_.entry(str(id));if(entry&&(route!="gallery"||vocabulary_picture(str(id)))&&seen.insert(entry->word).second)ids.push_back(str(id));}
    const size_t offset=route=="gallery"?galleryPage_*3:0,limit=route=="gallery"?std::min(offset+3,ids.size()):ids.size();
    for(size_t i=offset;i<limit;++i)if(!expose_lookup(catalog_.entry(ids[i])->word))return false;
    return true;
}
bool NativeVocabulary::back() {
    if (!error_.empty() || !loaded_) { dismiss_error(); return true; }
    if(route_=="word-batch" && str(field(state_,"session"),"phase")=="summary") {
        if(field(state_,"session").contains("shopRole")){backStack_.clear();go("bookshop");return true;}
        const auto source=str(field(state_,"session"),"source");
        return dispatch(VJ{{"action","v-return"},{"value",source=="review"?"review":source=="notebook"?"notebook":"home"}}.dump());
    }
    if(!backStack_.empty()) {
        auto f=backStack_.back();
        if(f.route=="search" && !expose_candidates(f.query))return false;
        if(!expose_journey(f.route))return false;
        backStack_.pop_back();
        lookup_=f.lookup;query_=f.query;go(f.route);
    } else go("home");
    return true;
}
bool NativeVocabulary::type(const std::string &text) {
    if (!loaded_ || !error_.empty()) return false;
    if (text.size() > 100)
        return false;
    if(route_=="history-search"){historyInput_=text;++viewSerial_;return true;}
    if (route_ == "search") {
        if(!expose_candidates(text))return false;
        query_ = text;
        ++viewSerial_;
        return true;
    }
    const auto &answers=answer_state();auto t = engine_.current(answers);
    if ((route_ != "word-batch"&&route_!="history-quiz") || t.is_null() || !field(answers["session"], "feedback").is_null())
        return false;
    return study_action(VJ{{"type", "draft"}, {"taskId", t["uid"]}, {"value", text}});
}
bool NativeVocabulary::edit(const std::string &keys) {
    const auto &answers=answer_state();
    auto text = route_=="history-search"?historyInput_:route_ == "search" ? query_ : str(field(answers["session"], "draft"), "value");
    auto task = engine_.current(answers);
    if (route_!="history-search"&&route_ != "search" &&
        field(field(answers["session"], "draft"), "taskId") != field(task, "uid"))
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
        if(route_=="history-search"){historyFilter_=lower(historyInput_);historyPage_=0;load_history();return back();}
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
    auto p = str(w[key],"path");
    if(p.rfind("packs/",0)!=0)p="vocabulary/"+p;
    if (audioPath_ == p) {
        stop_audio();
        note_ = "已停止播放";
        return;
    }
    if (route_!="history-card"&&!study_action(VJ{{"type", "audio"}, {"id", id}}))
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
VJ NativeVocabulary::library_rows() const {
    VJ rows=VJ::array();
    for(const auto &stage:field(catalog_.metadata,"stages")){
        const auto id=str(stage,"id");
        bool practised=flag(state_,"stageChosen")&&str(state_,"selectedStage")==id;
        if(!practised)for(const auto &item:field(state_,"seen").items()){
            const auto *entry=catalog_.entry(item.key());
            if(entry&&catalog_.contains_word(id,item.key())){practised=true;break;}
        }
        VJ row={{"id",id},{"title",str(stage,"title")},{"detail",std::to_string(num(stage,"ready"))+" 词"},{"practised",practised}};
        // Exact published mirror of the built-in GRE deck; other editions keep separate entries.
        for(const auto &d:catalog_.decks)if(d.second.stage==id){row["packKey"]=d.first;row["mirror"]=d.second.signature;break;}
        if(id=="gre-trial"&&!row.contains("mirror"))row["mirror"]="1918b4647d7c16bb73afc1b8e87e142a9697186588ea2334c08be7e2336bbfa4";
        rows.push_back(row);
    }
    size_t learned=0;const auto &notebook=field(state_,"notebook");for(const auto &id:notebook)if(field(state_,"seen").contains(str(id)))++learned;
    rows.push_back({{"id","notebook"},{"title","我的单词本"},{"done",learned},{"total",notebook.size()},{"detail",std::to_string(notebook.size())+" 词"}});
    return rows;
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
        #include "bookshop_actions.inc"
        #include "learned_actions.inc"
        if(name=="v-stamp") {
            const auto stamps=album_stamps();
            for(const auto &st:stamps)if(str(st,"sessionId")==value){
                for(const auto &id:st["ids"]){const auto *entry=catalog_.entry(str(id));if(entry&&!expose_lookup(entry->word))return false;}
                journeyStamp_=value;navigate("stamp");return true;
            }return true;
        }
        if(name=="v-gallery"||name=="v-gallery-page") {
            const size_t requested=name=="v-gallery"?0:size_t(std::max<int64_t>(0,num(a,"page")));
            std::vector<std::string> ids;std::set<std::string> seen;
            for(const auto &st:album_stamps())for(const auto &id:st["ids"]){const auto *entry=catalog_.entry(str(id));if(entry&&vocabulary_picture(str(id))&&seen.insert(entry->word).second)ids.push_back(str(id));}
            const size_t offset=std::min(requested,ids.empty()?size_t(0):(ids.size()-1)/3)*3;
            for(size_t i=offset;i<std::min(offset+3,ids.size());++i){const auto *entry=catalog_.entry(ids[i]);if(entry&&!expose_lookup(entry->word))return false;}
            galleryPage_=offset/3;if(route_!="gallery")navigate("gallery");else ++viewSerial_;return true;
        }
        if(name=="v-collection-page"){journeyPage_=size_t(std::max<int64_t>(0,num(a,"page")));++viewSerial_;return true;}
        if(name=="v-journey-resume") {
            if(str(field(state_,"session"),"phase")!="summary"&&!field(state_,"session").is_null()){navigate("word-batch");return true;}
            return dispatch(VJ{{"action","v-start"},{"intent","new"},{"allowExtra",true}}.dump());
        }
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
            if(!expose_journey(value))return false;
            if(value=="notebook-words"){const auto ids=state_["notebook"];for(const auto &id:ids){const auto *entry=catalog_.entry(str(id));if(entry&&!expose_lookup(entry->word))return false;}}
            if(flag(a,"root") || value=="home") { backStack_.clear();go(value); }
            else navigate(value);
            if(value=="word-list"){wordListError_.clear();wordListReady_=false;wordList_=VJ::array();wordListIssues_=VJ::array();++wordListToken_;}
            if(value=="levels") stageChoice_=str(state_,"selectedStage");
            if(value=="modes") {modeChoice_=str(state_["settings"],"mode");if(modeChoice_=="staged")modeChoice_="full";}
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
            if(commit(VJ{{"type","begin"},{"intent",intent},{"scope",reviewScope_},{"associative",true},{"allowExtra",flag(a,"allowExtra")}})) navigate("word-batch");
            return true;
        }
        if (name == "v-review-scope") {
            if(value=="current" || value=="all" || value=="notebook") {reviewScope_=value;++viewSerial_;}
            return true;
        }
        if (name == "v-stage") { stageChoice_=value; ++viewSerial_; return true; }
        if(name=="v-library-study"||name=="v-library-review"||name=="v-library-mode"){
            if(name=="v-library-mode")return dispatch(VJ{{"action","v-go"},{"value","modes"}}.dump());
            if(value=="notebook"){
                backStack_.clear();go("notebook");
                const auto next=engine_.resumable(state_,"notebook");
                if(!next.is_null()||!engine_.available_notebook(state_).empty())return dispatch(action("learnNotebook").dump());
                return true;
            }
            if(!flag(state_,"stageChosen")||value!=str(state_,"selectedStage"))
                if(!commit(VJ{{"type","chooseStage"},{"stage",value}}))return true;
            backStack_.clear();go("home");
            if(name=="v-library-review"){reviewScope_="current";navigate("review");return true;}
            if(engine_.resumable(state_,"new").is_null()&&engine_.available_new(state_).empty()){
                navigate("library-finished");return true;
            }
            return dispatch(VJ{{"action","v-start"},{"intent","new"},{"allowExtra",true}}.dump());
        }
        if(name=="v-use-stage") {
            if(!flag(state_,"stageChosen") || value!=str(state_,"selectedStage"))
                if(!commit(VJ{{"type","chooseStage"},{"stage",value}})) return true;
            backStack_.clear();go("home");return true;
        }
        if(name=="v-select-mode") {
            if(value=="meaning"||value=="partial"||value=="full"){modeChoice_=value;++viewSerial_;}
            return true;
        }
        if(name=="v-use-mode") {
            if(modeChoice_!="meaning"&&modeChoice_!="partial"&&modeChoice_!="full")return true;
            const auto current=str(state_["settings"],"mode");
            if(current==modeChoice_&&str(field(state_,"session"),"mode")==modeChoice_&&field(state_,"paused").empty())return back();
            auto setting=state_["settings"];setting["mode"]=modeChoice_;setting["type"]="settings";setting["applyStudy"]=true;
            if(commit(setting))return back();
            return false;
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
        const auto &answers=answer_state();auto task = engine_.current(answers);
        const auto &ss = field(answers, "session");
        if (name == "v-answer" || name == "w-unknown") {
            VJ answer{{"type", "answer"},
                      {"taskId", a.contains("taskId") ? a["taskId"] : field(task, "uid")},
                      {"value", field(a, "value").is_string() ? a["value"]
                                                              : field(field(ss, "draft"), "value")},
                      {"unknown", name == "w-unknown"}};
            study_action(answer);
            return true;
        }
        if (name == "v-do") {
            auto cmd = a;
            cmd.erase("action");
            auto type = str(cmd, "type");
            if(type=="learnNotebook") {cmd["explicit"]=true;cmd["associative"]=true;}
            if ((type == "learnNext" || type == "learnPrev") && !cmd.contains("sessionId"))
                cmd["sessionId"] = field(ss, "id");
            if (type == "continue" && !cmd.contains("taskId"))
                cmd["taskId"] = field(field(ss, "feedback"), "uid");
            if ((type == "hint" || type == "associationHint") && !cmd.contains("taskId"))
                cmd["taskId"] = field(task, "uid");
            if (study_action(cmd)) {
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
    const auto &answers=answer_state();
    const auto &ss = field(answers, "session");
    auto task = loaded_ ? engine_.current(answers) : VJ();
    auto phase = str(ss, "phase");
    v.taskId = str(task, "uid");
    v.sessionId = str(ss, "id");
    v.learnAt = num(ss, "learnAt");
    p.key = "vocabulary:" + route_ + ":" + str(ss, "id") + ":" + phase + ":" + str(task, "uid") +
            ":" + (field(ss, "feedback").is_null() ? "question" : "feedback") + ":" +
            str(ss, "learnAt") + ":" + (route_=="word"?lookup_:"");
    const auto title = stage_title(catalog_, str(state_, "selectedStage"));
    const auto reviewScope = route_ == "home" ? std::string("current") : reviewScope_;
    auto due = (route_=="home" || route_=="review" || phase=="summary")
        ? engine_.review_tasks(state_, now_, reviewScope) : VJ::array();
    const auto &today = field(state_["days"], VocabularyEngine::date(now_));
    auto startAction=[](const std::string &intent){return VJ{{"action","v-start"},{"intent",intent},{"allowExtra",intent=="new"}};};
    #include "bookshop_pages.inc"
    #include "learned_pages.inc"
    if(route_=="library-finished"){
        p.route="word-result";p.title=title;v.top("返回",VJ{{"action","pack-decks"}});
        v.text("当前没有可开始的新词","meaning");
        v.button("已学单词",VJ{{"action","v-history"},{"scope","current"}});
        v.button("查看本词库复习",VJ{{"action","v-go"},{"value","review"},{"root",true}},true,"foot");return p;
    }
    if (route_ == "home") {
        p.title = "小小单词卡";

        v.top("设置", VJ{{"action", "v-go"}, {"value", "settings"}});
        const auto preference = str(state_["settings"], "mode");
        v.button(mode_name(preference) + "模式", VJ{{"action","v-go"},{"value","modes"}});
        p.blocks.back().role = "vocab-home-mode";
        const bool chosen=flag(state_,"stageChosen");
        v.text(chosen?title:"选一本开始", "vocab-home-stage", true);
        p.home_new_words=int(field(today,"new").size());
        p.home_new_target=int(num(state_["settings"],"dailyNew"));
        const bool journeyOk=!field(state_,"storyRuns").empty()||vocabulary_journey_valid(field(state_,"journey"));
        v.button("已学单词",VJ{{"action","v-history"}});p.blocks.back().role="vocab-home-learned";
        const auto stampCount=album_stamps().size();
        v.button(journeyOk?"我的收集册 · "+n(stampCount)+" 枚":"收集册暂不可用",VJ{{"action","v-go"},{"value","collection"}});
        p.blocks.back().role="vocab-home-collection";
        if(journeyOk){
            const size_t start=stampCount?((stampCount-1)/6)*6:0;
            for(size_t i=start;i<start+6;++i){v.text(n(i+1),"vocab-home-stamp");p.blocks.back().selected=i<stampCount;p.blocks.back().detail=i==stampCount?"next":"";}
            v.text(stampCount?"每完成一轮，就留下一枚印章":"完成一轮，留下第一枚印章","vocab-home-stamp-note");
        }else v.text("收集记录保留，学习可以继续","vocab-home-stamp-note");
        v.text("今天已学 " + n(field(today, "new").size()) + " / " +
                   str(state_["settings"], "dailyNew") + " 个新词",
               "vocab-home-today");
        v.text(!engine_.resumable(state_,"review","current").is_null() ? "本词库有未完成复习" : due.empty() ? "本词库暂时没有待复习项"
                           : "本词库待巩固 " + n(due.size()) + " 项", "vocab-home-due");
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
        std::string next="记场景 → 辨词义 → 想英文";
        if (preference == "meaning") next = "认识单词 → 辨词义";
        if (preference == "partial") next = "认识单词 → 辨词义 → 补字母";
        if(active) {
            auto local=state_;local["session"]=nextGroup;auto nextTask=engine_.current(local);
            if(str(nextGroup,"phase")=="learn") next="认识单词 · "+n(num(nextGroup,"learnAt")+1)+"/"+n(nextGroup["ids"].size());
            else if(!nextTask.is_null()) {
                const auto kind=str(nextTask,"kind");int total=0,done=0;
                for(const auto&q:nextGroup["tasks"])if(str(q,"kind")==kind&&flag(q,"followup")==flag(nextTask,"followup")){++total;if(flag(q,"done"))++done;}
                const bool feedback=!field(nextGroup,"feedback").is_null();
                next=(kind=="meaning"?"词义检查":(kind=="gap"||kind=="partial")?"补字母":kind=="recall"?"想出英文":"完整拼写")+std::string(" · ")+n(std::min(feedback?std::max(done,1):done+1,total))+"/"+n(total);
                if(feedback) next+=" · 查看反馈";
            }
        }
        if (active && mode_name(str(nextGroup,"mode")) != mode_name(preference))
            next = "本组" + mode_name(str(nextGroup,"mode")) + " · " + next;
        const bool goalDone=int64_t(field(today,"new").size())>=num(state_["settings"],"dailyNew");
        bool exhausted=true;
        for(const auto&e:catalog_.entries)if(e.eligible&&catalog_.contains_word(str(state_,"selectedStage"),e.id)&&!state_["seen"].contains(e.id)){exhausted=false;break;}
        const bool blocked=!active&&!exhausted&&engine_.available_new(state_).empty();
        const bool blockedNotebook=blocked&&str(ss,"source")=="notebook"&&str(ss,"phase")!="summary";
        const bool chooseNext=!active&&exhausted&&flag(state_,"stageChosen");
        if(!active&&goalDone)next="已达今日目标，还可以再学一组";
        if(chooseNext)next="本档新词已学完，可以选择下一档";
        if(blocked)next=blockedNotebook?"剩余新词在生词本词组中，可回去继续":"剩余新词在暂停词组中，可继续原词组";
        if(!chosen)next="从故事书或词库开始学习";
        const bool clockInvalid=now_<1704067200000LL || now_>4102444800000LL || now_<num(state_,"lastClock")-300000;
        if(storageBlocked_) next="存储待检查，原题与进度保持";
        else if(clockInvalid) next="先校准日期，再继续原题";
        v.text(next,"vocab-home-next");
        auto label=!chosen?"选择学习内容":active?"继续学习":chooseNext?"选择下一词库":blocked?(blockedNotebook?"返回生词本":"查看暂停词组"):goalDone?"再学一组":"开始背词";
        if(storageBlocked_) v.button("检查存储",VJ{{"action","v-check-recovery"}},true,"foot");
        else if(clockInvalid) v.button("校准日期",VJ{{"action","v-device-date"}},true,"foot");
        else if(!chosen)v.button(label,VJ{{"action","pack-library"}},true,"foot");
        else v.button(label,chooseNext?VJ{{"action","pack-decks"}}:blocked?VJ{{"action","v-go"},{"value",blockedNotebook?"notebook":"paused"}}:startAction("new"),true,"foot");
        v.tabs("home");
        return p;
    }
    if(route_=="collection"||route_=="gallery"||route_=="stamp") {
        p.title=route_=="collection"?"我的收集册":route_=="gallery"?"单词图鉴":"这一站的足迹";
        p.key+=":"+n(journeyPage_)+":"+n(galleryPage_)+":"+journeyStamp_;
        v.top(back_label(),VJ{{"action","v-back"}});
        const auto stamps=album_stamps();
        if(field(state_,"storyRuns").empty()&&!vocabulary_journey_valid(field(state_,"journey"))) {
            v.text("先安心学习","",true);v.text("收集记录暂不可用，原数据保留。背词和复习照常保存，暂不发新印章。","meaning");
        } else if(route_=="collection") {
            v.text(stamps.empty()?"第一枚，在路上":"已完成 "+n(stamps.size())+" 站","vocab-album-title",true);
            v.button("单词图鉴",VJ{{"action","v-gallery"}});
            p.blocks.back().role="vocab-album-gallery";
            journeyPage_=std::min(journeyPage_,stamps.empty()?size_t(0):(stamps.size()-1)/6);
            for(size_t i=journeyPage_*6;i<journeyPage_*6+6;++i){
                std::string origin;
                if(i<stamps.size()){const auto &st=stamps[i];
                    const auto books=stamp_books(catalog_,st);
                    origin=books.size()==1?books.front():
                           std::find(books.begin(),books.end(),"来源待确认")!=books.end()?"来源待确认":"多词库";
                    v.button("第 "+n(i+1)+" 枚完成印章 · "+origin,VJ{{"action","v-stamp"},{"value",st["sessionId"]}},false,"body",(num(st,"finishedAt")?VocabularyEngine::date(num(st,"finishedAt")):"日期未记录"));p.blocks.back().selected=true;}
                else {v.text("待收集","vocab-album-stamp");p.blocks.back().enabled=false;}
                p.blocks.back().role="vocab-album-stamp";
                // Visible number is separate text metadata, not an image or reward mutation.
                v.text(n(i+1),"vocab-album-number",false,origin);
            }
            if(journeyPage_)v.button("上一页",VJ{{"action","v-collection-page"},{"page",journeyPage_-1}},false,"foot");
            if((journeyPage_+1)*6<stamps.size())v.button("下一页",VJ{{"action","v-collection-page"},{"page",journeyPage_+1}},false,"foot");
            v.text("印章记录完成，不代表已经掌握。","source");
        } else {
            std::vector<std::string> ids;std::set<std::string> seen;
            bool packDetail=false;
            for(const auto &st:stamps)if(route_=="gallery"||str(st,"sessionId")==journeyStamp_){
                if(route_=="stamp")for(const auto &book:stamp_books(catalog_,st))v.text(book,"stamp-book",true);
                if(route_=="stamp" && !str(st,"contentTitle").empty()){
                    packDetail=true;v.text(str(st,"kind")=="story"?"全书读毕":"一组词已练完","meaning");
                    std::string terms;for(const auto &w:field(st,"words")){if(!terms.empty())terms+=" · ";terms+=str(w);}
                    if(!terms.empty()&&field(st,"ids").size()<field(st,"words").size())v.text(terms,"source");
                }
                for(const auto &id:st["ids"]){const auto *entry=catalog_.entry(str(id));if(entry&&(route_!="gallery"||vocabulary_picture(str(id)))&&seen.insert(route_=="stamp"?str(id):entry->word).second)ids.push_back(str(id));}
            }
            const size_t offset=route_=="gallery"?galleryPage_*3:0,limit=route_=="gallery"?std::min(offset+3,ids.size()):ids.size();
            if(route_=="stamp"){
                if(!ids.empty()&&ids.size()<=5)v.button("重练这组词",VJ{{"action","v-practice"}});
                else if(ids.empty())v.button("查看已学单词",VJ{{"action","v-history"}});
            }
            if(ids.empty()&&!packDetail)v.text("完成一站后，学过的配图会留在这里。","meaning");
            for(size_t i=offset;i<limit;++i){const auto *entry=catalog_.entry(ids[i]);v.button(entry->word,VJ{{"action",route_=="stamp"?"v-browse":"v-open"},{"value",ids[i]}},false,"body",entry->meaning);v.picture(ids[i],catalog_.directory);}
            if(route_=="gallery"){
                if(galleryPage_)v.button("上一页",VJ{{"action","v-gallery-page"},{"page",galleryPage_-1}},false,"foot");
                if(limit<ids.size())v.button("下一页",VJ{{"action","v-gallery-page"},{"page",galleryPage_+1}},false,"foot");
            }
        }
        const bool fromShop=std::any_of(backStack_.begin(),backStack_.end(),[](const auto &frame){return frame.route=="bookshop";});
        if(fromShop)v.button("回到书店",shopAction("v-shop-home"),true,"foot");
        else v.button(!ss.is_null()&&phase!="summary"?"继续这一站":"开始下一站",VJ{{"action","v-journey-resume"}},true,"foot");
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
    if(route_=="modes") {
        p.title="学习模式";v.top(back_label(),VJ{{"action","v-back"}});
        v.text("选择你希望练到的程度。","source");
        for(const auto &mode:std::vector<std::string>{"meaning","partial","full"})
            v.button(mode_name(mode),VJ{{"action","v-select-mode"},{"value",mode}},false,"body",
                     mode=="meaning"?"认出单词，选对词义":mode=="partial"?"辨词义，再补 1～2 个字母":"辨词义，再完整拼出英文",modeChoice_==mode);
        const auto &current=field(state_,"session");
        v.text(!current.is_null()&&str(current,"phase")!="summary"?
               "背词组的后续题随模式调整，当前作答会保留。":"新学习和新复习都用所选模式。","source");
        v.button("使用"+mode_name(modeChoice_)+"模式",VJ{{"action","v-use-mode"}},true,"foot");
        return p;
    }
    if (route_ == "settings") {
        p.title = "背词设置";
        v.top(back_label(), VJ{{"action", "v-back"}});
        v.button("学习模式："+mode_name(str(state_["settings"],"mode")),VJ{{"action","v-go"},{"value","modes"}});
        v.text("每日新词", "", true);
        for (auto count : {5, 10, 20})
            v.button(n(count) + " 个", VJ{{"action", "v-count"}, {"value", n(count)}}, false,
                     "body", "", num(state_["settings"], "dailyNew") == count);
        v.text("每日目标用于安排新词。", "source");
        if(!state_["paused"].empty())v.button("已暂停的词组",VJ{{"action","v-go"},{"value","paused"}});
        v.button("回首页",VJ{{"action","v-return"},{"value","home"}},true,"foot");
        p.blocks.back().role="settings-exit";
        return p;
    }
    if (route_ == "review") {
        p.title="复习";v.top(back_label(),VJ{{"action","v-back"}});
        v.button("已学单词",VJ{{"action","v-history"}});p.blocks.back().role="review-history";
        p.key += ":" + reviewScope_;
        auto resume=engine_.resumable(state_,"review",reviewScope_);int meaning=0,spelling=0,recall=0,partial=0,first=0;
        const auto &queue=resume.is_null()?due:resume["tasks"];
        for(const auto&q:queue)if(!flag(q,"done")){(str(q,"kind")=="meaning"?meaning:str(q,"kind")=="partial"?partial:str(q,"kind")=="recall"?recall:spelling)++;if(flag(q,"firstRecall")||flag(q,"firstPartial"))first++;}
        v.text(reviewScope_=="current"?title:reviewScope_=="notebook"?"我的单词本":"全部已学词库","",true);
        v.text("辨词义 "+n(meaning)+(partial?" · 补字母 "+n(partial):recall?" · 想英文 "+n(recall):""),"meaning");
        if(spelling)v.text("旧拼写任务 "+n(spelling)+" 项","meaning");
        v.text(!resume.is_null()?"继续上次"+mode_name(str(resume,"mode"))+"模式，保留输入。":first?"含 "+n(first)+" 项首次"+(partial?"补字母练习。":"主动回忆。") :due.empty()?"没有到期任务，学过的记录都在。":"按各项记忆情况安排复习。","source");
        v.button(reviewScope_!="current"?"只看当前词库":"查看全部词库",VJ{{"action","v-review-scope"},{"value",reviewScope_!="current"?"current":"all"}},false,"foot");
        v.button("我的单词本 · "+n(state_["notebook"].size())+" 词",VJ{{"action","v-go"},{"value","notebook"}},false,"foot");
        const bool can=!resume.is_null()||!due.empty();
        v.button(!resume.is_null()?"继续复习":!due.empty()?"开始复习":"暂无到期复习",startAction("review"),true,"foot","",false,can);
        v.tabs("review");return p;
    }
    if(route_=="paused") {
        p.title="已暂停的词组";v.top(back_label(),VJ{{"action","v-back"}});
        for(const auto&paused:state_["paused"]){auto a=action("resumePaused");a["id"]=paused["id"];
            const auto source=str(paused,"source");auto label=source=="review"?"到期复习":source=="notebook"?"生词本":"新词学习";
            v.button(std::string(label)+" · "+mode_name(str(paused,"mode"))+" · "+n(paused["ids"].size())+" 词",a);
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
    if (route_ == "word"||route_=="history-card") {
        auto w = catalog_.get(lookup_);
        const bool browsing=route_=="history-card";
        p.title = browsing?"重学单词 "+n(browseAt_+1)+" / "+n(browseIds_.size()):"词义与发音";
        p.key+=":"+lookup_;
        v.top(back_label(), VJ{{"action","v-back"}});
        v.card(w,lookup_,catalog_.directory,true);
        for(const auto &row:field(field(state_,"cards"),lookup_).items()){
            const auto kind=row.key();
            const std::string label=kind=="meaning"?"词义":kind=="partial"?"补字母":kind=="recall"?"想英文":"拼写";
            v.text(label+"复习 "+recovery_time(num(row.value(),"due")),"detail");
        }
        if(browsing){
            v.button("重练这个词",VJ{{"action","v-practice"},{"value",lookup_}});
            if(browseIds_.size()>1&&!backStack_.empty()&&backStack_.back().route=="history-groups")
                v.button(browseIds_.size()<=5?"重练这组词":"重练这五词",VJ{{"action","v-practice"},{"group",true}});
        }
        if(browsing){
            if(browseAt_>0)v.button("上一个词",VJ{{"action","v-browse"},{"value",browseIds_[browseAt_-1]}},false,"foot");
            if(browseAt_+1<browseIds_.size())v.button("下一个词",VJ{{"action","v-browse"},{"value",browseIds_[browseAt_+1]}},true,"foot");
            return p;
        }
        auto a = action("notebook");
        a["id"] = lookup_;
        bool saved = has(state_["notebook"], lookup_);
        const bool fromNotebook=!backStack_.empty()&&(backStack_.back().route=="notebook"||backStack_.back().route=="notebook-words");
        if(saved&&fromNotebook){a["type"]="removeNotebook";v.button("移出生词本",a,true,"foot");}
        else v.button(saved ? "已加入生词本" : "加入生词本", a, true, "foot", "", false, !saved);
        return p;
    }
    if (route_ == "word-batch"||route_=="history-quiz") {
        if(route_=="history-quiz")p.route="word-batch";
        if (ss.is_null()) {
            go("home");
            return page();
        }
        if (phase == "summary") {
            p.route = "word-result";
            if(route_=="history-quiz"){
                p.title="重练完成";v.text("答对 "+str(ss,"correct")+" / "+str(ss,"answered")+" 题","meaning");
                v.button(back_label(),VJ{{"action","v-back"}},true,"foot");return p;
            }
            if(ss.contains("shopRole")){
                const auto role=str(ss,"shopRole");
                p.title=role=="visit"?"客人的回信":role=="review"?"旧词已温习":num(ss,"shopGroup")==0?"寻书信已回寄":"书箱已整理好";
                v.top("返回",shopAction("v-shop-home"));shopArt(scene);
                v.text(role=="mission"?(num(ss,"shopGroup")==0?"在顶层找到了地图册。回信刚寄出，邻居又抱来一箱淋过雨的书。":"最后一册也晾干了。窗边空出了一角，猫等你来布置。"):"猫把刚刚读过的便笺压进了书里。");
                v.text(n(field(ss,"ids").size())+" 个词 · 答对 "+str(ss,"correct")+" / "+str(ss,"answered")+" 题","source");
                for(const auto &stamp:vocabulary_stamps(state_))if(field(stamp,"sessionId")==field(ss,"id")){
                    v.text("新印章已收入我的收集册","source");break;
                }
                v.button(role=="visit"?"回到书店":shopGroup==2&&!scene?"布置我的书店":role=="review"?"继续营业":"接下一份委托",shopAction(role=="visit"?"v-shop-home":"v-shop-start"),true,"foot");return p;
            }
            p.title = "这一组完成";
            const auto source=str(ss,"source");
            const auto destination=source=="review"?"review":source=="notebook"?"notebook":"home";
            v.top(source=="review"?"返回复习":source=="notebook"?"返回生词":"回首页",VJ{{"action","v-return"},{"value",destination}});
            if(num(ss,"answered")==0)p.title="本组已记录";
            for(const auto &st:vocabulary_stamps(state_))if(field(st,"sessionId")==field(ss,"id")){
                v.text("又留下了一枚印章","",true);v.button("查看收集册",VJ{{"action","v-go"},{"value","collection"}});break;
            }
            if(!vocabulary_journey_valid(field(state_,"journey")))v.text("收集册暂不可用，本轮学习已保存。","source");
            v.text(source=="review"?"完成 "+str(ss,"answered")+" 次复习":
                   "新学 "+str(ss,"newCount")+" 词 · 完成 "+str(ss,"answered")+" 次练习","meaning");
            int recalled=0,attempted=0;
            for(const auto&q:ss["tasks"])if(str(q,"kind")=="recall"&&!flag(q,"followup")&&q.contains("responseCorrect")) {
                ++attempted;if(flag(q,"responseCorrect")&&!flag(q,"responseAssisted"))++recalled;
            }
            if(attempted)v.text("想英文：首次无提示答对 "+n(recalled)+" / "+n(attempted),"meaning");
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
                if(!due.empty()||!engine_.resumable(state_,"review",reviewScope_).is_null()) {
                    v.button("返回复习",VJ{{"action","v-return"},{"value","review"}},false,"foot");
                    v.button("继续复习",startAction("review"),true,"foot");
                } else v.button("返回复习",VJ{{"action","v-return"},{"value","review"}},true,"foot");
            } else if(source=="notebook") {
                const bool more=!engine_.available_notebook(state_).empty()||!engine_.resumable(state_,"notebook").is_null();
                v.button("返回生词本",VJ{{"action","v-return"},{"value","notebook"}},!more,"foot");
                if(more)v.button("下一组生词",action("learnNotebook"),true,"foot");
            } else {
                bool unseen=false;for(const auto&e:catalog_.entries)if(e.eligible&&catalog_.contains_word(str(state_,"selectedStage"),e.id)&&!state_["seen"].contains(e.id)){unseen=true;break;}
                const bool available=!engine_.available_new(state_).empty()||!engine_.resumable(state_,"new").is_null();
                v.button("回首页",VJ{{"action","v-return"},{"value","home"}},false,"foot");
                v.button(available?"再学一组":unseen?"查看暂停词组":"选择下一词库",available?startAction("new"):unseen?VJ{{"action","v-go"},{"value","paused"}}:VJ{{"action","pack-decks"}},true,"foot");
            }
            return p;
        }
        v.top(back_label(), VJ{{"action", "v-back"}});
        if(route_!="history-quiz"){
            v.top("模式", VJ{{"action","v-go"},{"value","modes"}});
            v.top("查词", VJ{{"action", "lookup"}});
        }
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
            p.title = (flag(ss,"associative")?"记住场景 ":"认识单词 ") + n(num(ss, "learnAt") + 1) + " / " + n(ss["ids"].size());
            v.card(w,id,catalog_.directory,!ss.contains("shopRole"));
            if (num(ss, "learnAt") > 0)
                v.button("上一个词", action("learnPrev"), false, "foot");
            v.button(num(ss, "learnAt") + 1 < int64_t(ss["ids"].size()) ? "下一个词" : "开始测试",
                     action("learnNext"), true, "foot");
            return p;
        }
        if (!feedback.is_null()) {
            p.title = flag(feedback, "correct")
                          ? (flag(feedback, "assisted") ? "提示后答对" : "答对了")
                          : "再记一遍";
            v.card(w,id,catalog_.directory,false);
            if (route_!="history-quiz" && (!flag(feedback,"correct") || flag(feedback,"assisted")))
                v.text("隔几个词再练。","source");
            const auto revision=uint64_t(num(answers,"revision"));
            if(route_=="history-quiz"||continuationRevision_!=revision||continuationClock_!=now_/1000) {
                continuationPreview_=engine_.continue_preview(answers,now_);
                continuationRevision_=revision;continuationClock_=now_/1000;
            }
            const auto nextKind=str(continuationPreview_,"kind");
            const auto nextLabel=str(continuationPreview_,"phase")=="summary"?"查看小结":
                flag(continuationPreview_,"followup")&&!flag(task,"followup")?"开始巩固":
                nextKind!=kind&&(nextKind=="gap"||nextKind=="partial")?"开始补字母":
                nextKind!=kind&&nextKind=="recall"?"开始想英文":nextKind!=kind&&nextKind=="spelling"?"开始完整拼写":"下一题";
            v.button(nextLabel, action("continue"), true, "foot");
            return p;
        }
        if (kind == "meaning") {
            p.title = (flag(task, "followup") ? "词义巩固" : "词义检查") + roundCount;
            if(ss.contains("shopRole")){
                p.title="情境回忆"+roundCount;
                v.text(shop_cue(catalog_,id,str(ss,"shopRole")=="visit"?scene:flag(task,"followup")?1:0),"question");
            }else{v.text(str(w, "word"), "word", true);v.text(str(w, "example"), "example");}
            for (const auto &opt : engine_.choices(task, ss))
                v.button(str(opt), VJ{{"action", "v-answer"}, {"value", opt}}, false, "foot");
            v.button("不认识", VJ{{"action", "w-unknown"}}, false, "foot");
            return p;
        }
        p.title = ((kind == "gap"||kind=="partial") ? "补上字母" : kind == "recall" ? "想出英文" : "完整拼写") + roundCount;
        p.keyboard = true;
        p.prompt = str(w, "meaning");
        p.typed =
            field(field(ss, "draft"), "taskId") == task["uid"] ? str(ss["draft"], "value") : "";
        p.can_submit = !p.typed.empty();
        if (kind == "gap"||kind=="partial") {
            p.gap_mask = str(engine_.gap(task), "mask");
            for (size_t at; (at = p.gap_mask.find("＿")) != std::string::npos;)
                p.gap_mask.replace(at, 3, "_");
        }
        if(ss.contains("shopRole")){
            p.prompt=shop_cue(catalog_,id,str(ss,"shopRole")=="visit"?scene:flag(task,"followup")?1:0);
            v.text(p.prompt,"meaning");if(kind=="partial")p.can_submit=p.typed.size()==str(engine_.gap(task),"answer").size();
            v.button("完整题干",shopAction("v-go","shop-prompt"));return p;
        }
        v.text(str(w, "meaning"), "meaning");
        if (flag(task, "answerRevealed"))
            v.text(str(w, "word"), "word");
        if(kind=="recall") {
            if(flag(task,"associationHint")) {
                const auto clue=str(w,"memoryCue").empty()?str(w,"exampleZh"):str(w,"memoryCue");
                v.text("线索："+clue+"（首字母 "+str(w,"word").substr(0,1)+"）","meaning");
                v.button("听提示",VJ{{"action","v-audio"},{"id",id},{"example",false}});
            } else {
                const bool fade=str(ss,"source")=="review" && num(field(field(state_["cards"],id),"recall"),"successes")>0;
                v.text(fade?"在脑中还原场景，想出目标词。":str(w,"exampleZh"),"meaning");
                v.button("给我线索",action("associationHint"));
            }
            p.speech_note=flag(task,"hint")?"本题用过提示，稍后再回忆":"回忆学过的目标词，再输入";
        } else v.button("听发音", VJ{{"action", "v-audio"}, {"id", id}, {"example", false}});
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
