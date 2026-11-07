#include "learning_web.hpp"
#include "../native_learning/include/native_study.hpp"
#include "quickjs.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif
namespace wordcard {
#ifdef ESP_PLATFORM
struct WebJob {
    const std::function<void()> *fn;
    SemaphoreHandle_t done;
};
bool LearningWeb::is_owner() const { return owner_ == xTaskGetCurrentTaskHandle(); }
void LearningWeb::owner_task(void *ptr) {
    auto *self = static_cast<LearningWeb *>(ptr);
    for (;;) {
        // Short idle grace: an immediate next tap must not first wait for speculative
        // JSON/native work to reach QuickJS's next interrupt checkpoint. Queue receive
        // wakes immediately for foreground jobs; this is never a foreground sleep.
        TickType_t wait = portMAX_DELAY;
        if (self->prepare_requested_) {
            const auto left = int32_t(self->prepare_after_tick_.load() - uint32_t(xTaskGetTickCount()));
            wait = left > 0 ? TickType_t(left) : 0;
        }
        WebJob *job = nullptr;
        if (xQueueReceive(static_cast<QueueHandle_t>(self->jobs_), &job, wait) == pdTRUE) {
            if (job) {
                (*job->fn)();
                if (self->prepare_requested_)
                    self->prepare_after_tick_ = uint32_t(xTaskGetTickCount() + pdMS_TO_TICKS(80));
                xSemaphoreGive(job->done);
            }
        } else if (self->prepare_requested_) {
            const auto generation = self->prepare_generation_.load();
            const bool more = self->prepare_step();
            if (!more && generation == self->prepare_generation_.load())
                self->prepare_requested_ = false;
        }
    }
}
bool LearningWeb::invoke_owner(const std::function<void()> &fn) {
    WebJob job{&fn, xSemaphoreCreateBinary()};
    if (!job.done)
        return false;
    WebJob *p = &job;
    const bool queued = xQueueSend(static_cast<QueueHandle_t>(jobs_), &p, portMAX_DELAY) == pdTRUE;
    if (queued)
        xSemaphoreTake(job.done, portMAX_DELAY);
    vSemaphoreDelete(job.done);
    return queued;
}
#endif
struct WebScript {
    const char *name;
    const char *data;
    size_t length;
};
extern const WebScript web_scripts[];
extern const size_t web_script_count;
static int64_t micros() {
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
#endif
}
struct VmTimer {
    uint64_t &out;
    int64_t began = micros();
    explicit VmTimer(uint64_t &v) : out(v) {}
    ~VmTimer() { out = uint64_t(micros() - began); }
};
static std::string string(JSContext *c, JSValueConst v) {
    const char *p = JS_ToCString(c, v);
    if (!p)
        return {};
    std::string s(p);
    JS_FreeCString(c, p);
    return s;
}
static std::string prop(JSContext *c, JSValueConst v, const char *k) {
    auto p = JS_GetPropertyStr(c, v, k);
    auto s = string(c, p);
    JS_FreeValue(c, p);
    return s == "undefined" ? "" : s;
}
static bool flag(JSContext *c, JSValueConst v, const char *k, bool def = false) {
    auto p = JS_GetPropertyStr(c, v, k);
    bool b = JS_IsUndefined(p) ? def : JS_ToBool(c, p) > 0;
    JS_FreeValue(c, p);
    return b;
}
static int number(JSContext *c, JSValueConst v, const char *k, int def = 0) {
    auto p = JS_GetPropertyStr(c, v, k);
    int32_t n = def;
    if (!JS_IsUndefined(p))
        JS_ToInt32(c, &n, p);
    JS_FreeValue(c, p);
    return n;
}
static std::string quote(JSContext *c, const std::string &s) {
    auto p = JS_NewStringLen(c, s.data(), s.size());
    auto j = JS_JSONStringify(c, p, JS_UNDEFINED, JS_UNDEFINED);
    auto out = string(c, j);
    JS_FreeValue(c, j);
    JS_FreeValue(c, p);
    return out;
}
static JSValue native_read(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_NULL;
    bool found = false;
    const auto value = static_cast<LearningWeb *>(JS_GetContextOpaque(c))->read(string(c, argv[0]), found);
    return found ? JS_NewStringLen(c, value.data(), value.size()) : JS_NULL;
}
static JSValue native_write(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
    return JS_NewBool(c, argc == 2 && static_cast<LearningWeb *>(JS_GetContextOpaque(c))
                                          ->write(string(c, argv[0]), string(c, argv[1])));
}
static JSValue native_export(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
    if (argc != 2)
        return JS_FALSE;
    const auto name = string(c, argv[0]);
    const std::string key = name == "wordcard-web-progress.json"           ? "export-course"
                            : name == "wordcard-goal-assessment-plan.json" ? "export-profile"
                                                                           : "";
    return JS_NewBool(c,
                      !key.empty() &&
                          static_cast<LearningWeb *>(JS_GetContextOpaque(c))->write(key, string(c, argv[1])));
}
#ifdef ESP_PLATFORM
static void *web_malloc(void *, size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void *web_calloc(void *, size_t n, size_t s) {
    return heap_caps_calloc(n, s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static void web_free(void *, void *p) { heap_caps_free(p); }
static void *web_realloc(void *, void *p, size_t n) {
    return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static size_t web_usable(const void *p) {
    return p ? heap_caps_get_allocated_size(const_cast<void *>(p)) : 0;
}
#endif
static std::string file_key(const std::string &key) {
    if (key == "wordcard-adult-web-v2")
        return "course-v2.json";
    if (key == "wordcard-learning-path-v2")
        return "profile-v2.json";
    if (key == "wordcard-learning-path-v1")
        return "profile-v1.json";
    if (key == "wordcard-adult-web-v1")
        return "course-v1.json";
    if (key == "import-course" || key == "import-profile" || key == "export-course" ||
        key == "export-profile")
        return key + ".json";
    return {};
}
std::string LearningWeb::read(const std::string &key, bool &found) {
    const auto name = file_key(key);
    found = false;
    if (name.empty())
        return {};
    const auto p = directory_ + '/' + name;
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        struct stat st{};
        if (stat(p.c_str(), &st) == 0) {
            found = true;
            return "{unreadable";
        }
        in.clear();
        in.open(p + ".bak", std::ios::binary);
        if (!in)
            return {};
    }
    std::ostringstream s;
    s << in.rdbuf();
    found = true;
    return s.str();
}
bool LearningWeb::write(const std::string &key, const std::string &value) {
    if (maintenance_)
        return false; // Speculative rendering can never write the SD.
    struct WriteTimer {
        uint64_t &out;
        int64_t began = micros();
        ~WriteTimer() { out += uint64_t(micros() - began); }
    } write_timer{last_write_us_};
    const auto n = file_key(key);
    if (n.empty() || value.size() > 700000)
        return false;
    const auto p = directory_ + '/' + n, tmp = p + ".tmp", bak = p + ".bak";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(value.data(), value.size());
        out.flush();
        if (!out)
            return false;
    }
    struct stat st{};
    const bool present = stat(p.c_str(), &st) == 0;
    if (present) {
        std::remove(bak.c_str());
        if (std::rename(p.c_str(), bak.c_str()))
            return false;
    }
    if (std::rename(tmp.c_str(), p.c_str())) {
        if (present)
            std::rename(bak.c_str(), p.c_str());
        return false;
    }
    return true;
}
int LearningWeb::interrupt(JSRuntime *, void *p) {
    const auto self = static_cast<LearningWeb *>(p);
    const auto now = micros();
    if (now > self->deadline_)
        return 1;
#ifdef ESP_PLATFORM
    if (self->preparing_ && uxQueueMessagesWaiting(static_cast<QueueHandle_t>(self->jobs_)))
        return 1;
    // A real blocked tick lets IDLE1 and input/power tasks run. A zero-tick
    // yield would keep this priority-1 VM runnable and still starve the idle task.
    if (now >= self->next_yield_us_) {
        self->next_yield_us_ = now + 10000;
        vTaskDelay(1);
    }
#endif
    return 0;
}
LearningWeb::~LearningWeb() {
#ifdef ESP_PLATFORM
    if (owner_ && !is_owner()) {
        invoke_owner([&] {
            delete native_;
            native_ = nullptr;
            if (ctx_)
                JS_FreeContext(ctx_);
            if (rt_)
                JS_FreeRuntime(rt_);
            ctx_ = nullptr;
            rt_ = nullptr;
        });
        vTaskDeleteWithCaps(static_cast<TaskHandle_t>(owner_));
        vQueueDelete(static_cast<QueueHandle_t>(jobs_));
        return;
    }
#endif
    delete native_;
    if (ctx_) {
        JS_FreeContext(ctx_);
    }
    if (rt_) {
        JS_FreeRuntime(rt_);
    }
}
bool LearningWeb::exception() {
    auto e = JS_GetException(ctx_);
    error_ = string(ctx_, e);
    const auto stack = prop(ctx_, e, "stack");
    if (!stack.empty())
        error_ += '\n' + stack;
    JS_FreeValue(ctx_, e);
    JSMemoryUsage usage{};
    JS_ComputeMemoryUsage(rt_, &usage);
    error_ += " [vm allocated=" + std::to_string(usage.malloc_size) +
              " used=" + std::to_string(usage.memory_used_size) + "]";
    return false;
}
bool LearningWeb::native_active() const { return native_ && native_->active(); }
void LearningWeb::native_enabled(bool enabled) {
    if (!enabled && native_active())
        leave_native();
    native_enabled_ = enabled;
}
bool LearningWeb::enter_native() {
    if (!native_enabled_ || !ctx_ || maintenance_)
        return false;
    const char *src = "nativeInspect().route";
    auto v = JS_Eval(ctx_, src, strlen(src), "study-route", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        exception();
        return false;
    }
    const auto route = string(ctx_, v);
    JS_FreeValue(ctx_, v);
    if (!NativeStudy::supported(route))
        return false;
    std::string context;
    if (!eval("nativeTakeStudy()", &context))
        return false;
    try {
        if (!native_)
            native_ = new NativeStudy(
                [this](const std::string &key, const std::string &value) { return write(key, value); },directory_);
        native_->vocabulary_enabled(!courses_open_);
        if (!native_->enter(context))
            return false;
        native_dirty_ = true;
        cached_revision_ = -1;
        return true;
    } catch (const std::exception &e) {
        error_ = e.what();
        return false;
    }
}
bool LearningWeb::leave_native(const std::string &root_navigation) {
    if (!native_active())
        return true;
    native_->stop_for_boundary();
    const auto context = native_->context();
    native_->leave();
    cached_revision_ = -1;
    prepare_requested_ = false;
    const auto command = root_navigation.empty() ? "nativeRestoreStudy(" + context + ")"
        : "nativeRestoreStudy(" + context + ",true);nativeAction(" + root_navigation + ")";
    if (!eval(command)) {
        native_->enter(context);
        return false;
    }
    audio_dirty_ = true;
    return true;
}
bool LearningWeb::dispatch_native(const std::string &data) {
    VmTimer timer(last_call_us_);
    last_write_us_ = last_gc_us_ = 0;
    native_->tick(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count());
    const bool vocabulary_boundary = native_->vocabulary_active();
    if (native_->dispatch(data)) {
        native_dirty_ = true;
        audio_dirty_ = true;
        return true;
    }
    // These routes neither inspect the old DOM nor write learner state. Restore
    // the live context and navigate once; never render the abandoned native page.
    // All other boundaries retain the original restore-then-action path.
    try {
        const auto action = J::parse(data);
        const auto route = str(action, "route");
        if (str(action, "action") == "go" && (route == "books" || route == "dictionary"))
            return leave_native(data);
    } catch (const std::exception &) { /* Original action handler reports invalid input. */ }
    // Vocabulary actions must never fall through to the old daily-plan engine.
    // Only the explicit old-course entry above is a permitted boundary.
    if (vocabulary_boundary) {
        native_dirty_ = true;
        return true;
    }
    if (!leave_native())
        return false;
    return eval("nativeAction(" + data + ")");
}
bool LearningWeb::status(std::string &json) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = status(json); }) && ok;
    }
#endif
    if (native_active()) {
        json = native_->status();
        return true;
    }
    return eval("(()=>{const "
                "n=nativeDebug,m=n.model,p=n.pathModel,b=m.state.batch,s=m.state.session,q=b?n.M.wordBatch."
                "current(b):null,d=n.M.daily.snapshot(m,p);return "
                "{route:nativeInspect().route,revision:nativeRevision,selectors:nativeSelectors.size,next:d."
                "next,stages:d.stages.map(x=>({kind:x.kind,done:x.done,total:x.total,remaining:x.remaining}))"
                ",batch:b?{phase:b.phase,at:b.at,serial:b.serial,learnAt:b.learnAt,id:q?.id||b.ids[b.learnAt]"
                ",kind:q?.kind,typed:b.typed,checked:b.checked?.correct}:null,session:s?{lesson:s.lesson,at:"
                "s.at,page:s.page,id:n.M.task(s)?.id,kind:n.M.task(s)?.kind,heard:!!s.heard[s.at],checked:s."
                "checked?.correct}:null,courseError:m.error,profileError:p.error};})()",
                &json);
}
bool LearningWeb::eval(const std::string &code, std::string *json) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = eval(code, json); }) && ok;
    }
#endif
    if (native_active() && !leave_native())
        return false;
    finish_input();
    if (!maintenance_)
        audio_dirty_ = true;
    last_write_us_ = 0;
    // Every command still collects. For text edits alone, move collection to the
    // end of this physical paint, so committed letters appear before heap scanning.
    VmTimer timer(last_call_us_);
    JS_SetGCThreshold(rt_, 3 * 1024 * 1024);
    deadline_ = micros() + 5000000;
    JS_UpdateStackTop(rt_);
    auto v = JS_Eval(ctx_, code.data(), code.size(), "native-command", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        const bool ok = exception();
        if (!maintenance_) {
            input_gc_pending_ = true;
            finish_input();
        }
        return ok;
    }
    if (json) {
        auto j = JS_JSONStringify(ctx_, v, JS_UNDEFINED, JS_UNDEFINED);
        if (JS_IsException(j)) {
            JS_FreeValue(ctx_, v);
            return exception();
        }
        *json = string(ctx_, j);
        JS_FreeValue(ctx_, j);
    }
    JS_FreeValue(ctx_, v);
    ++eval_calls_;
    if (!maintenance_) {
        input_gc_pending_ = true;
        if (!defer_input_gc_)
            finish_input();
    }
    return true;
}
void LearningWeb::finish_input() {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        invoke_owner([&] { finish_input(); });
        return;
    }
#endif
    if (input_gc_pending_) {
        VmTimer timer(last_gc_us_);
        JS_RunGC(rt_);
        input_gc_pending_ = false;
    }
}
void LearningWeb::prepare_next() {
    if (native_active())
        return; // Native transitions/layout avoid the interpreter work.
    bool next = cached_page_.route == "quiz";
    for (const auto &a : cached_page_.actions)
        if (a.find("\"action\":\"next\"") != std::string::npos ||
            a.find("\"action\":\"w-next\"") != std::string::npos) {
            next = true;
            break;
        }
    if (!ready_ || !next)
        return;
#ifdef ESP_PLATFORM
    ++prepare_generation_;
    prepare_after_tick_ =
        uint32_t(xTaskGetTickCount() + (cached_page_.route == "quiz" ? 0 : pdMS_TO_TICKS(80)));
    prepare_requested_ = true;
    WebJob *wake = nullptr;
    xQueueSend(static_cast<QueueHandle_t>(jobs_), &wake, 0);
#else
    for (int i = 0; i < 8 && prepare_step(); ++i) {
    }
#endif
}
static void decode_page(JSContext *, JSValueConst, WebPage &);
void LearningWeb::cancel_prepare() {
    prepare_requested_ = false;
    ++prepare_generation_;
#ifdef ESP_PLATFORM
    if (owner_ && !is_owner())
        invoke_owner([] {});
#endif
    rendered_prepare_token_ = 0;
}
bool LearningWeb::prepare_step() {
    if (!ctx_)
        return false;
    const auto previous_error = error_;
    const auto call = last_call_us_, page_time = last_page_us_, gc = last_gc_us_, writes = last_write_us_;
    const auto generation = prepare_generation_.load();
    maintenance_ = true;
    preparing_ = true;
    std::string more;
    const auto began = micros();
    JS_SetInterruptInterval(ctx_, 256);
    const bool ok = eval("nativePrepareStep()", &more);
    preparing_ = false;
    bool retry = false;
    if (!ok) {
#ifdef ESP_PLATFORM
        retry = uxQueueMessagesWaiting(static_cast<QueueHandle_t>(jobs_)) != 0;
#endif
        // Foreground requests interrupt speculative work. Restore before granting
        // the next VM job; the emergency closure also covers interrupted finally.
        if (!eval(retry ? "nativeAbortPreparation(true)" : "nativeAbortPreparation()")) {
            ready_ = false;
            retry = false;
        }
    }
    // Collect each speculative step after restoration. Do not publish the
    // foreground input-GC flag: that queues a spurious main-loop job, cancels
    // preparation and restarts its idle grace between stages.
    // A queued foreground job already collects on completion. Do not make that
    // tap wait for an extra speculative collection after restoring live state.
    // The 3 MiB automatic GC threshold and 4 MiB hard limit remain active.
    if (!retry) {
        VmTimer gc_timer(last_gc_us_);
        JS_RunGC(rt_);
    }
    if (ok && more == "false" && prepare_renderer_) {
        const char *source = "nativePrepared";
        auto v = JS_Eval(ctx_, source, strlen(source), "prepared-view", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(v) && !JS_IsNull(v)) {
            const int token = number(ctx_, v, "token");
            auto projection = JS_GetPropertyStr(ctx_, v, "view");
            WebPage page;
            if (token != rendered_prepare_token_)
                decode_page(ctx_, projection, page);
            JS_FreeValue(ctx_, projection);
            JS_FreeValue(ctx_, v);
            auto cancel = [&] {
                if (generation != prepare_generation_.load())
                    return true;
#ifdef ESP_PLATFORM
                if (uxQueueMessagesWaiting(static_cast<QueueHandle_t>(jobs_)))
                    return true;
#endif
                return false;
            };
            if (token != rendered_prepare_token_) {
                page.secondary=courses_open_;
                prepare_renderer_(page, cancel);
                if (!cancel())
                    rendered_prepare_token_ = token;
            }
        } else {
            if (JS_IsException(v)) {
                auto e = JS_GetException(ctx_);
                JS_FreeValue(ctx_, e);
            }
            JS_FreeValue(ctx_, v);
        }
    }
    JS_SetInterruptInterval(ctx_, 10000);
    maintenance_ = false;
#ifdef ESP_PLATFORM
    ESP_LOGI("learning.vm", "PREPARE elapsed_us=%lld ok=%d more=%d cancelled=%d",
             (long long)(micros() - began), ok, more == "true", retry);
#else
    (void)began;
#endif
    if (ready_) {
        error_ = previous_error;
    }
    last_call_us_ = call;
    last_page_us_ = page_time;
    last_gc_us_ = gc;
    last_write_us_ = writes;
    return retry || (ok && more == "true");
}
bool LearningWeb::edit(const std::string &keys) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = edit(keys); }) && ok;
    }
#endif
    if (native_active()) {
        VmTimer timer(last_call_us_);
        last_write_us_ = last_gc_us_ = 0;
        native_->tick(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count());
        native_->edit(keys);
        native_dirty_ = true;
        audio_dirty_ = true;
        return true;
    }
    // Only this unchanged spelling page has a stable action table. The original
    // model validates and atomically saves the final text before it is displayed.
    if (!cached_page_.keyboard || cached_page_.route != "word-batch")
        return false;
    defer_input_gc_ = true;
    std::string delta;
    const bool ok = eval("nativeEdit(" + quote(ctx_, keys) + ")", &delta);
    defer_input_gc_ = false;
    if (!ok)
        return false;
    auto v = JS_ParseJSON(ctx_, delta.data(), delta.size(), "input-delta");
    if (JS_IsException(v)) {
        finish_input();
        return exception();
    }
    if (flag(ctx_, v, "stable") && cached_revision_ == number(ctx_, v, "baseRevision")) {
        cached_page_.typed = prop(ctx_, v, "typed");
        cached_page_.toast = prop(ctx_, v, "toast");
        cached_page_.can_submit = flag(ctx_, v, "canSubmit");
        cached_revision_ = number(ctx_, v, "revision");
    }
    JS_FreeValue(ctx_, v);
    return true;
}
bool LearningWeb::init(const std::string &dir) {
#ifdef ESP_PLATFORM
    if (!owner_) {
        jobs_ = xQueueCreate(1, sizeof(WebJob *));
        TaskHandle_t task = nullptr;
        if (!jobs_ || xTaskCreateWithCaps(owner_task, "learning.vm", 256 * 1024, this, 1, &task,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
            error_ = "学习任务内存不足";
            return false;
        }
        owner_ = task;
    }
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = init(dir); }) && ok;
    }
#endif
#ifdef WORDCARD_REFERENCE_TEST
    native_enabled_ = false;
#endif
    if (ctx_) {
        return ready_;
    }
    directory_ = dir;
#ifdef ESP_PLATFORM
    static const JSMallocFunctions allocator = {web_calloc, web_malloc, web_free, web_realloc, web_usable};
    rt_ = JS_NewRuntime2(&allocator, nullptr);
#else
    rt_ = JS_NewRuntime();
#endif
    if (!rt_) {
        error_ = "学习引擎内存不足";
        return false;
    }
    JS_SetMemoryLimit(rt_, 4 * 1024 * 1024);
    JS_SetMaxStackSize(rt_, 192 * 1024);
    JS_SetInterruptHandler(rt_, interrupt, this);
    ctx_ = JS_NewContext(rt_);
    if (!ctx_) {
        error_ = "学习上下文创建失败";
        return false;
    }
    JS_SetContextOpaque(ctx_, this);
    auto g = JS_GetGlobalObject(ctx_);
    JS_SetPropertyStr(ctx_, g, "nativeRead", JS_NewCFunction(ctx_, native_read, "nativeRead", 1));
    JS_SetPropertyStr(ctx_, g, "nativeWrite", JS_NewCFunction(ctx_, native_write, "nativeWrite", 2));
    JS_SetPropertyStr(ctx_, g, "nativeExport", JS_NewCFunction(ctx_, native_export, "nativeExport", 2));
    JS_FreeValue(ctx_, g);
    for (size_t i = 0; i < web_script_count; ++i) {
        const auto began = micros();
        deadline_ = began + 10000000;
        auto v = JS_Eval(ctx_, web_scripts[i].data, web_scripts[i].length, web_scripts[i].name,
                         JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(v)) {
            const auto ok = exception();
            error_ = std::string(web_scripts[i].name) + ": " + error_;
            return ok;
        }
        JS_FreeValue(ctx_, v);
#ifdef ESP_PLATFORM
        ESP_LOGI("learning.vm", "SCRIPT %s elapsed_ms=%lld", web_scripts[i].name,
                 (long long)(micros() - began) / 1000);
        vTaskDelay(1);
#endif
    }
    ready_ = true;
    if(native_enabled_) {
        const auto at=directory_.rfind("/learning-web");
        if(at!=std::string::npos) {
            std::ifstream catalog(directory_.substr(0,at)+"/vocabulary/catalog.json");
            if(catalog && !eval("nativeDebug.go('home')")) return false;
        }
    }
    return true;
}
bool LearningWeb::command(const std::string &a, const std::string &value) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = command(a, value); }) && ok;
    }
#endif
    const auto data = J{{"action", a}, {"value", value}}.dump();
    if (native_active())
        return dispatch_native(data);
    return eval("nativeAction(" + data + ")");
}
bool LearningWeb::action(int i) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = action(i); }) && ok;
    }
#endif
    if (i < 0 || size_t(i) >= actions_.size())
        return false;
    const auto data = actions_[i];
    return native_active() ? dispatch_native(data) : eval("nativeAction(" + data + ")");
}
bool LearningWeb::key(const std::string &k, const std::string &v) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = key(k, v); }) && ok;
    }
#endif
    if (native_active()) {
        if (cached_page_.keyboard && cached_page_.route == "word-batch" && (k == "key" || k == "backspace"))
            return edit(k == "backspace" ? std::string(1, '\b') : v);
        VmTimer timer(last_call_us_);
        last_write_us_ = last_gc_us_ = 0;
        native_->tick(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count());
        native_->input(k, v);
        native_dirty_ = true;
        audio_dirty_ = true;
        return true;
    }
    if (cached_page_.keyboard && cached_page_.route == "word-batch" && (k == "key" || k == "backspace"))
        return edit(k == "backspace" ? std::string(1, '\b') : v);
    return eval("nativeKey(" + quote(ctx_, k) + "," + quote(ctx_, v) + ")");
}
bool LearningWeb::type(const std::string &v) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = type(v); }) && ok;
    }
#endif
    if (native_active() && (cached_page_.route == "word-batch" || native_->vocabulary_active())) {
        VmTimer timer(last_call_us_);
        last_write_us_ = last_gc_us_ = 0;
        native_->tick(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count());
        native_->type(v);
        native_dirty_ = true;
        return true;
    }
    return eval("nativeType(" + quote(ctx_, v) + ")");
}
bool LearningWeb::open_courses() {
#ifdef ESP_PLATFORM
    if(!is_owner()){bool ok=false;return invoke_owner([&]{ok=open_courses();})&&ok;}
#endif
    if(courses_open_)return true;
    cancel_prepare();
    courses_open_=true;
    if(native_)native_->vocabulary_enabled(false);
    const auto nav=J{{"action","go"},{"route","books"}}.dump();
    const bool ok=native_active()?leave_native(nav):eval("nativeAction("+nav+")");
    if(!ok){courses_open_=false;if(native_){native_->vocabulary_enabled(true);native_->resume_vocabulary();}native_dirty_=true;}
    return ok;
}
bool LearningWeb::resume_main() {
#ifdef ESP_PLATFORM
    if(!is_owner()){bool ok=false;return invoke_owner([&]{ok=resume_main();})&&ok;}
#endif
    if(!courses_open_)return true;
    cancel_prepare();
    if(!leave_native())return false;
    courses_open_=false;
    if(native_){native_->vocabulary_enabled(true);if(native_->resume_vocabulary()){native_dirty_=true;cached_revision_=-1;audio_dirty_=true;return true;}}
    return eval("nativeDebug.go('home')");
}
bool LearningWeb::home() { return resume_main() && command("w-day-home"); }
bool LearningWeb::back() {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = back(); }) && ok;
    }
#endif
    if (native_active()) {
        VmTimer timer(last_call_us_);
        last_write_us_ = last_gc_us_ = 0;
        native_->tick(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count());
        if (native_->back()) {
            native_dirty_ = true;
            audio_dirty_ = true;
            return true;
        }
    }
    return eval("nativeBack()");
}
bool LearningWeb::cancel_audio() {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool result = false;
        return invoke_owner([&] { result = cancel_audio(); }) && result;
    }
#endif
    if (native_active()) {
        native_->stop_for_boundary();
        native_dirty_ = true;
        audio_dirty_ = true;
        return true;
    }
    return eval("nativeDebug.cancelAudio()");
}
bool LearningWeb::playback(int serial, bool ok) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool result = false;
        return invoke_owner([&] { result = playback(serial, ok); }) && result;
    }
#endif
    if (native_active()) {
        VmTimer timer(last_call_us_);
        last_write_us_ = last_gc_us_ = 0;
        native_->tick(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count());
        if (native_->playback(serial, ok))
            native_dirty_ = true;
        audio_dirty_ = true;
        return true;
    }
    return eval("nativeAudioEnded(" + std::to_string(serial) + "," + (ok ? "true" : "false") + ")");
}
bool LearningWeb::take_audio(std::string &path, int &serial, bool &stop, double *rate) {
    if (!audio_dirty_)
        return false;
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = take_audio(path, serial, stop, rate); }) && ok;
    }
#endif
    if (native_active()) {
        audio_dirty_ = false;
        return native_->take_audio(path, serial, stop, rate);
    }
    audio_dirty_ = false;
    deadline_ = micros() + 5000000;
    auto global = JS_GetGlobalObject(ctx_);
    auto v = JS_GetPropertyStr(ctx_, global, "nativePending");
    if (JS_IsNull(v) || JS_IsUndefined(v)) {
        JS_FreeValue(ctx_, v);
        JS_FreeValue(ctx_, global);
        return false;
    }
    path = prop(ctx_, v, "path");
    serial = number(ctx_, v, "serial");
    stop = flag(ctx_, v, "stop");
    if (rate) {
        auto r = JS_GetPropertyStr(ctx_, v, "rate");
        *rate = 1;
        JS_ToFloat64(ctx_, rate, r);
        JS_FreeValue(ctx_, r);
    }
    JS_FreeValue(ctx_, v);
    JS_SetPropertyStr(ctx_, global, "nativePending", JS_NULL);
    JS_FreeValue(ctx_, global);
    return true;
}
std::string LearningWeb::audio_text(const std::string &path) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        std::string text;
        invoke_owner([&] { text = audio_text(path); });
        return text;
    }
#endif
    if (native_active())
        return native_->audio_text(path);
    deadline_ = micros() + 5000000;
    const auto code =
        "Object.keys(WORDCARD_AUDIO).find(k=>WORDCARD_AUDIO[k]===" + quote(ctx_, path) + ")||''";
    auto v = JS_Eval(ctx_, code.data(), code.size(), "audio-text", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        exception();
        return {};
    }
    auto text = string(ctx_, v);
    JS_FreeValue(ctx_, v);
    return text;
}
static void decode_page(JSContext *c, JSValueConst v, WebPage &out) {
    out = {};
    out.title = prop(c, v, "title");
    out.route = prop(c, v, "route");
    out.key = prop(c, v, "key");
    out.typed = prop(c, v, "typed");
    out.prompt = prop(c, v, "prompt");
    out.gap_mask = prop(c, v, "gapMask");
    out.toast = prop(c, v, "toast");
    out.keyboard = flag(c, v, "keyboard");
    out.shift = flag(c, v, "shift");
    out.symbols = flag(c, v, "symbols");
    out.can_submit = flag(c, v, "canSubmit");
    out.corrupt = flag(c, v, "corrupt");
    out.actions.clear();
    auto list = [&](const char *k, auto fn) {
        auto a = JS_GetPropertyStr(c, v, k);
        const int n = number(c, a, "length");
        for (int i = 0; i < n; ++i) {
            auto x = JS_GetPropertyUint32(c, a, i);
            fn(x);
            JS_FreeValue(c, x);
        }
        JS_FreeValue(c, a);
    };
    list("blocks", [&](JSValue x) {
        WebBlock b;
        b.kind = prop(c, x, "kind");
        b.text = prop(c, x, "text");
        b.area = prop(c, x, "area");
        b.role = prop(c, x, "role");
        b.detail = prop(c, x, "detail");
        b.action = number(c, x, "action", -1);
        b.enabled = flag(c, x, "enabled", true);
        b.selected = flag(c, x, "selected");
        b.primary = flag(c, x, "primary");
        out.blocks.push_back(b);
    });
    for (const auto k : {"top", "tabs"})
        list(k, [&](JSValue x) {
            WebControl b;
            b.text = prop(c, x, "text");
            b.action = number(c, x, "action", -1);
            b.enabled = flag(c, x, "enabled", true);
            b.selected = flag(c, x, "selected");
            (std::string(k) == "top" ? out.top : out.tabs).push_back(b);
        });
    list("actions", [&](JSValue x) {
        auto j = JS_JSONStringify(c, x, JS_UNDEFINED, JS_UNDEFINED);
        out.actions.push_back(string(c, j));
        JS_FreeValue(c, j);
    });
}
bool LearningWeb::page(WebPage &out) {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = page(out); }) && ok;
    }
#endif
    VmTimer timer(last_page_us_);
    deadline_ = micros() + 5000000;
    JS_UpdateStackTop(rt_);
    if (native_active() || enter_native()) {
        if (native_dirty_) {
            cached_page_ = native_->page();
            cached_page_.secondary=courses_open_;
            actions_ = cached_page_.actions;
            native_dirty_ = false;
            ++page_builds_;
        } else
            ++page_hits_;
        out = cached_page_;
        return true;
    }
    auto global = JS_GetGlobalObject(ctx_);
    auto rev = JS_GetPropertyStr(ctx_, global, "nativeRevision");
    int64_t revision = 0;
    JS_ToInt64(ctx_, &revision, rev);
    JS_FreeValue(ctx_, rev);
    JS_FreeValue(ctx_, global);
    if (cached_revision_ == revision) {
        out = cached_page_;
        ++page_hits_;
        return true;
    }

    deadline_ = micros() + 5000000;
    auto v = JS_Eval(ctx_, "nativeView()", 12, "view", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v))
        return exception();
    decode_page(ctx_, v, out);
    out.secondary=courses_open_;
    actions_ = out.actions;
    JS_FreeValue(ctx_, v);
    cached_page_ = out;
    cached_revision_ = revision;
    ++page_builds_;
    return true;
}
size_t LearningWeb::memory_bytes() const {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        size_t n = 0;
        const_cast<LearningWeb *>(this)->invoke_owner([&] { n = memory_bytes(); });
        return n;
    }
#endif
    JSMemoryUsage m{};
    if (rt_)
        JS_ComputeMemoryUsage(rt_, &m);
    return size_t(m.memory_used_size);
}
bool LearningWeb::import_progress() {
#ifdef ESP_PLATFORM
    if (!is_owner()) {
        bool ok = false;
        return invoke_owner([&] { ok = import_progress(); }) && ok;
    }
#endif

    bool a = false, b = false;
    const auto course = read("import-course", a), profile = read("import-profile", b);
    if (!a && !b) {
        error_ = "未找到 import-course.json 或 import-profile.json";
        return false;
    }
    // Validate both with disposable storage first. No partial replacement on bad input.
    std::string check;
    if (!eval("(()=>{const s={getItem:()=>null,setItem:()=>{}};const m=new WORDCARD_MODEL.Model(s),p=new "
              "WORDCARD_PATH.Model(s);return " +
                  (a ? "m.import(" + quote(ctx_, course) + ")" : "true") + "&&" +
                  (b ? "p.import(" + quote(ctx_, profile) + ")" : "true") + ";})()",
              &check) ||
        check != "true") {
        error_ = "导入文件无效，原记录保留";
        return false;
    }
    // Persistent replacement is explicit; each original is retained as .bak.
    if (a && !eval("nativeDebug.model.import(" + quote(ctx_, course) + ")", &check))
        return false;
    if (a && check != "true") {
        error_ = "课程进度未保存，导入停止";
        return false;
    }
    if (b && !eval("nativeDebug.pathModel.import(" + quote(ctx_, profile) + ")", &check))
        return false;
    if (b && check != "true") {
        error_ = "课程导入已完成；计划保存失败，请重试计划导入";
        return false;
    }
    return home();
}
} // namespace wordcard
