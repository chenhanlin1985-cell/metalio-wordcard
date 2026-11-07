#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include "../../native_learning/include/native_page.hpp"
struct JSRuntime;struct JSContext;
namespace wordcard {
class NativeStudy;
class LearningWeb {
public:
    LearningWeb()=default;~LearningWeb();
    bool init(const std::string &directory);
    bool ready()const{return ctx_!=nullptr&&ready_;}
    bool action(int index);bool command(const std::string &action,const std::string &value="");
    bool key(const std::string &kind,const std::string &value="");
    bool type(const std::string &value);
    bool edit(const std::string &keys);
    void finish_input();
    bool input_pending()const{return input_gc_pending_;}
    uint64_t last_gc_us()const{return last_gc_us_;}
    uint64_t last_write_us()const{return last_write_us_;}
    void prepare_next();
    using PrepareRenderer=std::function<void(const WebPage&,const std::function<bool()>&)>;
    void prepare_renderer(PrepareRenderer renderer){prepare_renderer_=std::move(renderer);}
    void cancel_prepare();
    bool home();bool back();
    bool open_courses();bool resume_main();
    bool courses_open()const{return courses_open_;}
    bool playback(int serial,bool success);
    bool cancel_audio();
    bool take_audio(std::string &path,int &serial,bool &stop,double *rate=nullptr);
    std::string audio_text(const std::string &path);
    bool page(WebPage &out);
    bool eval(const std::string &code,std::string *json=nullptr);
    bool status(std::string &json);
    bool native_active()const;
    uint32_t script_calls()const{return eval_calls_;}
    void native_enabled(bool enabled);
    const std::string &error()const{return error_;}
    std::string read(const std::string &key,bool &found);
    bool write(const std::string &key,const std::string &value);
    bool import_progress();
    size_t memory_bytes()const;
    uint64_t last_call_us()const{return last_call_us_;}
    uint64_t last_page_us()const{return last_page_us_;}
    uint32_t page_builds()const{return page_builds_;}
    uint32_t page_hits()const{return page_hits_;}
private:
    JSRuntime *rt_=nullptr;JSContext *ctx_=nullptr;bool ready_=false;
    std::string directory_,error_;
    std::vector<std::string> actions_;
    int64_t deadline_=0,next_yield_us_=0;
    WebPage cached_page_;
    int64_t cached_revision_=-1;
    uint64_t last_call_us_=0,last_page_us_=0,last_gc_us_=0;
    uint64_t last_write_us_=0;
    bool input_gc_pending_=false,defer_input_gc_=false;
    bool maintenance_=false,preparing_=false;
    std::atomic_bool audio_dirty_{true},prepare_requested_{false};
 std::atomic_uint32_t prepare_generation_{0},prepare_after_tick_{0};
    bool prepare_step();
    PrepareRenderer prepare_renderer_;
    int rendered_prepare_token_=0;
    uint32_t eval_calls_=0,page_builds_=0,page_hits_=0;
    static int interrupt(JSRuntime*,void*);
    bool exception();
    NativeStudy *native_=nullptr;
    bool native_enabled_=true,native_dirty_=true,courses_open_=false;
    bool enter_native();
    bool leave_native(const std::string &root_navigation = {});
    bool dispatch_native(const std::string&data);
#ifdef ESP_PLATFORM
    void *owner_=nullptr,*jobs_=nullptr;
    bool invoke_owner(const std::function<void()> &);
    bool is_owner()const;
    static void owner_task(void*);
#endif
};
}
