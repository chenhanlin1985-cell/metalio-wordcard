// Runtime code is compiled into the application. Only JSON and media are read from SD.
#ifdef ESP_PLATFORM
#include "generated/content_player_deflate.inc"
#else
static const char book_script[] =
#include "generated/content_player.inc"
;
#endif
std::string LearningWeb::book_root() const {
    const auto i=directory_.rfind("/learning-web");
    return directory_.substr(0,i)+"/packs";
}

std::string LearningWeb::story_progress(const std::string &request){
    native_->tick(clock_ms());
    return native_->story_progress(J::parse(request)).dump();
}
static JSValue book_progress(JSContext *c,JSValueConst,int argc,JSValueConst *args){
    if(argc!=1)return JS_ThrowTypeError(c,"故事请求格式无效");
    try{
        const auto result=static_cast<LearningWeb*>(JS_GetContextOpaque(c))->story_progress(string(c,args[0]));
        return JS_NewStringLen(c,result.data(),result.size());
    }catch(const std::exception &e){return JS_ThrowInternalError(c,"%s",e.what());}
}
static JSValue book_keys(JSContext *c,JSValueConst,int,JSValueConst*){
    const auto root=static_cast<LearningWeb*>(JS_GetContextOpaque(c))->book_root();
    auto list=JS_NewArray(c);uint32_t index=0;
    for(const auto &id:book_directories(root)){
        if(book_path(id+"@1.0.0").empty())continue;
        for(const auto &version:book_directories(root+"/"+id)){
            const auto key=id+"@"+version;
            if(!book_path(key).empty())JS_SetPropertyUint32(c,list,index++,JS_NewString(c,key.c_str()));
        }
    }
    return list;
}
static JSValue book_read(JSContext *c,JSValueConst,int argc,JSValueConst *args){
    if(argc!=2)return JS_ThrowTypeError(c,"缺少内容包");
    const auto path=book_path(string(c,args[0])),name=string(c,args[1]);
    if(path.empty()||(name!="manifest.json"&&name!="content.json"))return JS_ThrowTypeError(c,"包路径无效");
    const auto root=static_cast<LearningWeb*>(JS_GetContextOpaque(c))->book_root();
    std::ifstream file(root+"/"+path+"/"+name,std::ios::binary|std::ios::ate);
    const auto length=file.tellg();
    if(length<=0||length>2*1024*1024)return JS_ThrowTypeError(c,"内容文件缺失或超过设备内存限制");
    std::string data(size_t(length),'\0');file.seekg(0);file.read(data.data(),length);
    if(!file)return JS_ThrowTypeError(c,"SD 卡读取失败");
    return JS_ParseJSON(c,data.c_str(),data.size(),name.c_str());
}
void LearningWeb::close_books(){
    if(!book_ctx_)return;
    JS_FreeContext(book_ctx_);book_ctx_=nullptr;JS_RunGC(rt_);JS_SetMemoryLimit(rt_,4*1024*1024);
    book_imported_=false;book_audio_text_.clear();native_dirty_=true;audio_dirty_=true;cached_revision_=-1;
}
bool LearningWeb::open_books(const std::string &command){
    cancel_prepare();auxiliary_return_.clear();
    if(book_ctx_)close_books();
    JS_SetMemoryLimit(rt_,8*1024*1024);
    book_ctx_=JS_NewContext(rt_);
    if(!book_ctx_){JS_SetMemoryLimit(rt_,4*1024*1024);error_="内容库内存不足";return false;}
    JS_SetContextOpaque(book_ctx_,this);
    auto global=JS_GetGlobalObject(book_ctx_);
    JS_SetPropertyStr(book_ctx_,global,"nativeDeckOwner",JS_TRUE);
    for(auto binding:{std::make_pair("bookKeys",book_keys),std::make_pair("bookRead",book_read),std::make_pair("nativeStory",book_progress),
                      std::make_pair("nativeRead",native_read),std::make_pair("nativeWrite",native_write)}){
        JS_SetPropertyStr(book_ctx_,global,binding.first,JS_NewCFunction(book_ctx_,binding.second,binding.first,2));
    }
    JS_FreeValue(book_ctx_,global);
    deadline_=micros()+15000000;JS_UpdateStackTop(rt_);
#ifdef ESP_PLATFORM
    auto *source=static_cast<char*>(heap_caps_malloc(book_length+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!source || tinfl_decompress_mem_to_mem(source,book_length,book_deflate,sizeof(book_deflate),0)!=book_length){
        heap_caps_free(source);close_books();error_="内容库程序解码失败";return false;
    }
    source[book_length]=0;
    auto result=JS_Eval(book_ctx_,source,book_length,"content-player",JS_EVAL_TYPE_GLOBAL);
    heap_caps_free(source);
#else
    auto result=JS_Eval(book_ctx_,book_script,sizeof(book_script)-1,"content-player",JS_EVAL_TYPE_GLOBAL);
#endif
    if(JS_IsException(result)){
        auto ex=JS_GetException(book_ctx_);error_=string(book_ctx_,ex);JS_FreeValue(book_ctx_,ex);close_books();return false;
    }
    JS_FreeValue(book_ctx_,result);
    const auto rows=native_->library_rows().dump();
    if(!book_call("installBuiltinRows",&rows)||!book_call("shelf",&shelf_location_)){close_books();return false;}
    if(!command.empty()&&!book_call("run",&command)){close_books();return false;}
    native_dirty_=true;audio_dirty_=true;return true;
}
bool LearningWeb::book_call(const char *method,const std::string *arg,std::string *result){
    deadline_=micros()+15000000;JS_UpdateStackTop(rt_);
    auto global=JS_GetGlobalObject(book_ctx_);
    auto player=JS_GetPropertyStr(book_ctx_,global,"Books");
    auto fn=JS_GetPropertyStr(book_ctx_,player,method);
    auto a=arg?JS_NewStringLen(book_ctx_,arg->data(),arg->size()):JS_UNDEFINED;
    auto v=JS_Call(book_ctx_,fn,player,arg?1:0,&a);
    if(!JS_IsException(v)&&result){
        auto json=JS_JSONStringify(book_ctx_,v,JS_UNDEFINED,JS_UNDEFINED);JS_FreeValue(book_ctx_,v);v=json;
        if(!JS_IsException(v))*result=string(book_ctx_,v);
    }
    const bool ok=!JS_IsException(v);
    if(!ok){auto e=JS_GetException(book_ctx_);error_=string(book_ctx_,e);JS_FreeValue(book_ctx_,e);}
    JS_FreeValue(book_ctx_,v);JS_FreeValue(book_ctx_,a);JS_FreeValue(book_ctx_,fn);
    JS_FreeValue(book_ctx_,player);JS_FreeValue(book_ctx_,global);return ok;
}
static J selected_book(const std::string &json){
    auto value=J::parse(json,nullptr,false);return value.is_object()?value:J::object();
}
void LearningWeb::project_selection(WebPage &p){
    if(!auxiliary_return_.empty()&&p.route=="notebook"){
        for(auto &b:p.top)if(b.text.rfind("返回",0)==0||b.text=="回首页")b.text="返回查词";
    }
    if(!library_origin_.empty()&&library_origin_==p.route&&p.key.rfind("vocabulary:history-quiz:",0)!=0){
        for(auto &b:p.top)if(b.text.rfind("返回",0)==0||b.text=="回首页")b.text="返回内容库";
    }
    if(selection_.empty())return;
    const auto item=selected_book(selection_);const auto key=str(item,"key");
    if(key.empty())return;
    if(p.route!="home"){
        if((p.route=="collection"||p.route=="stamp"||p.route=="gallery")&&!book_path(key).empty()){
            for(auto &b:p.blocks)if(b.kind=="button"&&b.action>=0&&p.actions.at(b.action)==R"({"action":"v-journey-resume"})"){
                b.text="继续学习";p.actions.at(b.action)=R"({"action":"pack-resume"})";
            }
        }
        return;
    }
    const bool notebook=key=="builtin/notebook";
    const bool story=str(item,"type")=="story";
    const auto storyType=str(item,"storyType");
    if(storyType=="vocabulary"||storyType=="comprehension"){
        p.blocks.erase(std::remove_if(p.blocks.begin(),p.blocks.end(),[](const WebBlock &b){return b.role=="vocab-home-mode";}),p.blocks.end());
    }
    for(auto &block:p.blocks){
        if(block.role=="vocab-home-stage")block.text=str(item,"title");
        if(block.role=="vocab-home-next")block.text=str(item,"detail");
        if(block.role=="vocab-home-history")block.text=notebook?"自选词表":storyType=="vocabulary"?"分段学词":storyType=="comprehension"?"阅读理解":story?"任务冒险":"按主题背词";
        if(block.role=="vocab-home-streak"||block.role=="vocab-home-today"||block.role=="vocab-home-due")block.text.clear();
        if(block.role=="vocab-home-mode"&&!notebook){
            const auto mode=str(item,"mode");block.text=(mode=="normal"?std::string("普通"):mode=="hard"?std::string("困难"):std::string("简单"))+"模式";
            p.actions.at(block.action)=R"({"action":"pack-mode"})";
        }
        if(block.area=="foot"&&block.primary){block.text="继续学习";p.actions.at(block.action)=notebook?R"({"action":"pack-notebook"})":R"({"action":"pack-resume"})";}
    }
    p.home_new_words=int(num(item,"done"));p.home_new_target=int(num(item,"total"));
    WebBlock label;label.kind="text";label.role="home-progress-label";label.text=story?"已完成章节":notebook?"词表进度":"已练词条";p.blocks.push_back(label);
    p.key+=":"+key;
}
std::string LearningWeb::deck_stage(const std::string &key) const {
    if(key.empty()||!native_)return {};
    for(const auto &row:native_->library_rows())if(str(row,"packKey")==key)return str(row,"id");
    return {};
}
bool LearningWeb::open_deck(const std::string &key,const std::string &intent,bool fromShelf){
    const auto stage=deck_stage(key);if(stage.empty())return false;
    if(book_ctx_){
        std::string loc;if(book_call("location",nullptr,&loc)){auto parsed=J::parse(loc,nullptr,false);if(parsed.is_string())shelf_location_=parsed.get<std::string>();}
        close_books();
    }
    if(!dispatch_native(J{{"action",intent=="review"?"v-library-review":intent=="mode"?"v-library-mode":"v-library-study"},{"value",stage},{"packKey",key}}.dump()))return false;
    const auto route=native_->page().route;
    if(intent=="study"&&route!="word-batch"&&route!="word-result"&&route!="library-finished")return true;
    if(fromShelf)library_origin_=route;
    return intent=="study"?write("content-selection","{}"):true;
}
bool LearningWeb::book_root_action(const std::string &root){
    if(book_ctx_){
        std::string json;
        if(book_call("location",nullptr,&json)){auto loc=J::parse(json,nullptr,false);if(loc.is_string())shelf_location_=loc.get<std::string>();}
        if(book_call("summary",nullptr,&json)&&json!="null"&&str(selected_book(json),"key")==str(selected_book(selection_),"key")&&json!=selection_){
            if(!write("content-selection",json))return false;
        }
        close_books();
    }
    library_origin_.clear();auxiliary_return_.clear();
    const auto key=str(selected_book(selection_),"key");
    if(root=="library")return open_books();
    if(root=="resume"||root=="mode"||root=="review"){
        const auto target=root=="review"?str(selected_book(selection_),"deckKey"):key;
        if(!deck_stage(target).empty())return open_deck(target,root=="resume"?"study":root);
    }
    if(!book_path(key).empty()&&(root=="resume"||root=="review"||root=="mode"))
        return open_books((root=="review"?"reviewOf/":root=="mode"?"modeOf/":"resume/")+key);
    if(root=="notebook")return dispatch_native(J{{"action","v-library-study"},{"value","notebook"}}.dump());
    const auto dest=root=="review"?"review":root=="search"?"search":"home";
    native_dirty_=true;audio_dirty_=true;
    const bool ok=native_->dispatch(J{{"action","v-go"},{"value",dest},{"root",true}}.dump());
    if(ok&&root=="review")native_->dispatch(J{{"action","v-review-scope"},{"value",key=="builtin/notebook"?"notebook":"current"}}.dump());
    return ok;
}
bool LearningWeb::book_action(const std::string &command){
    if(command.rfind("open/",0)==0&&!deck_stage(command.substr(5)).empty())return open_deck(command.substr(5),"study",true);
    if(command=="deckReview"||command=="review"||command=="reviewStart")return book_root_action("review");
    if(command=="home")return book_root_action("home");
    if(command.rfind("root/",0)==0)return book_root_action(command.substr(5));
    if(command=="exit"||(command=="back"&&cached_page_.key.find(":shelf:")!=std::string::npos))return book_root_action("home");
    if(command=="back"){
        std::string result;
        if(book_call("backRoot",nullptr,&result)&&result=="true")return book_root_action("home");
    }
    if(command.rfind("builtin/",0)==0){
        const auto id=command.substr(8);std::string json;
        auto rows=native_->library_rows();const auto row=std::find_if(rows.begin(),rows.end(),[&](const J &x){return str(x,"id")==id;});
        if(row==rows.end())return false;
        if(book_call("location",nullptr,&json)){auto loc=J::parse(json,nullptr,false);if(loc.is_string())shelf_location_=loc.get<std::string>();}
        close_books();
        if(!dispatch_native(J{{"action","v-library-study"},{"value",id}}.dump()))return false;
        const auto target=native_->page();
        if(target.route!="word-batch"&&target.route!="notebook"&&target.route!="word-result")return true;
        library_origin_=target.route;
        const auto active=id=="notebook"?J{{"key","builtin/notebook"},{"title","我的单词本"},{"detail",str(*row,"detail")},{"total",num(*row,"total")},{"done",num(*row,"done")}}:J::object();
        return write("content-selection",active.dump());
    }
    native_dirty_=true;audio_dirty_=true;
    const bool ok=book_call("run",&command);
    if(ok&&command.rfind("mode/",0)==0){
        std::string result;if(book_call("homeMode",nullptr,&result)&&result=="true")return book_root_action("home");
    }
    return ok;
}
bool LearningWeb::book_view(WebPage &out){
    std::string data;
    if(!book_call("view",nullptr,&data))return false;
    try{
        const auto p=J::parse(data);WebPage w;
        w.title=str(p,"title");w.route=str(p,"route");w.key=str(p,"key");w.keyboard=p.value("keyboard",false);
        w.typed=str(p,"draft");w.gap_mask=str(p,"mask");w.symbols=p.value("symbols",false);
        w.shift=p.value("shift",false);w.can_submit=p.value("ready",false);
        auto action=[&](const J &b){if(!b.contains("act"))return -1;w.actions.push_back(str(b,"act"));return int(w.actions.size()-1);};
        for(const auto &b:p.at("blocks")){
            WebBlock row;row.kind=b.value("kind","text");row.area=b.value("area","body");row.text=str(b,"text");
            row.detail=str(b,"detail");row.role=str(b,"role");row.action=action(b);row.primary=b.value("primary",false);
            row.selected=b.value("selected",false);row.enabled=b.value("enabled",true);
            const auto art=str(p,"art"),src=book_path(str(p,"source"));
            if(b.contains("image")&&!src.empty()&&!art.empty()&&art.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_.")==std::string::npos&&art.find("..")==std::string::npos){
                row.picture=int(num(b,"image"));
                const bool illustrated_page=row.role=="picturebook-art";
                row.picture_width=illustrated_page?576:384;row.picture_height=illustrated_page?384:192;
                row.picture_source=book_root()+"/"+src+"/"+art;
            }
            w.blocks.push_back(std::move(row));
        }
        for(const auto name:{"top","tabs"})for(const auto &b:p.at(name)){
            auto &controls=std::string(name)=="top"?w.top:w.tabs;
            controls.push_back({str(b,"text"),action(b),b.value("selected",false),true});
        }
        out=std::move(w);return true;
    }catch(const std::exception &e){error_=e.what();return false;}
}
