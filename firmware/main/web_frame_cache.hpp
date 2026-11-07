#pragma once
#include "wordcard_ui.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
#endif

namespace wordcard {
// One private raster, never an EPD front/back buffer. Pixel content and the UI
// hit map are published together. Progress is not stored in this cache.
class WebFrameCache {
public:
    ~WebFrameCache(){
#ifdef ESP_PLATFORM
        drain();if(worker_)vTaskDeleteWithCaps(worker_);
        heap_caps_free(pixels_);
#else
        delete[] pixels_;
#endif
    }
    // Caller drains the VM worker before releasing/replacing font assets.
    void configure(const Font&font,const Font*fallback=nullptr){
#ifdef ESP_PLATFORM
        drain();
#endif
        std::lock_guard<std::mutex> lock(mutex_);ready_=false;
        extra_=fallback?fallback->detached():Font{};
        font_=font.detached(fallback?&extra_:nullptr);
        if(!pixels_){
#ifdef ESP_PLATFORM
            pixels_=static_cast<uint8_t*>(heap_caps_malloc(Canvas::bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
            pixels_=new(std::nothrow) uint8_t[Canvas::bytes];
#endif
        }
        enabled_=true;
    }
    void prepare(const WebPage&page){
#ifdef ESP_PLATFORM
        std::lock_guard<std::mutex> lock(mutex_);
        if(!enabled_||!pixels_)return;
        if(!worker_&&xTaskCreateWithCaps(worker_entry,"learning.paint",64*1024,this,1,&worker_,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS)return;
        requested_=page;pending_=true;++generation_;xTaskNotifyGive(worker_);
#else
        build(page,[]{return false;});
#endif
    }
    bool build(const WebPage&page,const std::function<bool()>&cancel){
        {std::lock_guard<std::mutex> lock(mutex_);ready_=false;}
        if(!pixels_||!font_.ready()||cancel())return false;
        View v;v.screen=Screen::WebLearning;v.web=page;
        v.dictionary_shift=page.shift;v.dictionary_symbols=page.symbols;v.dictionary_ready=page.can_submit;
        for(const auto&b:page.blocks)if(b.kind=="button"&&b.area!="foot"&&v.dictionary_recent.size()<2)v.dictionary_recent.push_back(b.text);
        UI ui;Canvas canvas(pixels_,font_,cancel);ui.render(canvas,v);
        if(cancel()){++cancelled_;return false;}
        std::lock_guard<std::mutex> lock(mutex_);
        if(cancel()){++cancelled_;return false;}
        page_=page;ui_=std::move(ui);ready_=true;++built_;return true;
    }
    bool take(const View&v,uint8_t*destination,UI&ui){
        std::lock_guard<std::mutex> lock(mutex_);
        if(!ready_||v.screen!=Screen::WebLearning||v.web_page!=0||!v.notice.empty()||!equal(page_,v.web))return false;
        std::memcpy(destination,pixels_,Canvas::bytes);ui=ui_;ready_=false;++hits_;return true;
    }
    uint32_t built()const{return built_;}uint32_t hits()const{return hits_;}uint32_t cancelled()const{return cancelled_;}
    static bool equal(const WebPage&a,const WebPage&b){
        if(a.secondary!=b.secondary||a.title!=b.title||a.typed!=b.typed||a.prompt!=b.prompt||a.route!=b.route||a.key!=b.key||a.toast!=b.toast||a.error!=b.error||a.speech_note!=b.speech_note||a.gap_mask!=b.gap_mask||a.actions!=b.actions||a.keyboard!=b.keyboard||a.shift!=b.shift||a.symbols!=b.symbols||a.can_submit!=b.can_submit||a.corrupt!=b.corrupt||a.blocks.size()!=b.blocks.size()||a.top.size()!=b.top.size()||a.tabs.size()!=b.tabs.size())return false;
        for(size_t i=0;i<a.blocks.size();++i){const auto&x=a.blocks[i];const auto&y=b.blocks[i];if(x.kind!=y.kind||x.text!=y.text||x.area!=y.area||x.action!=y.action||x.enabled!=y.enabled||x.selected!=y.selected||x.primary!=y.primary||x.role!=y.role||x.detail!=y.detail)return false;}
        auto controls=[](const auto&x,const auto&y){for(size_t i=0;i<x.size();++i)if(x[i].text!=y[i].text||x[i].action!=y[i].action||x[i].selected!=y[i].selected||x[i].enabled!=y[i].enabled)return false;return true;};
        return controls(a.top,b.top)&&controls(a.tabs,b.tabs);
    }
private:
    uint8_t*pixels_=nullptr;Font font_,extra_;UI ui_;WebPage page_;bool ready_=false;
    std::mutex mutex_;std::atomic_uint32_t built_{0},hits_{0},cancelled_{0};
    bool enabled_=false;
#ifdef ESP_PLATFORM
    TaskHandle_t worker_=nullptr;WebPage requested_;bool pending_=false;
    std::atomic_bool busy_{false};std::atomic_uint32_t generation_{0};
    void drain(){
        {std::lock_guard<std::mutex> lock(mutex_);enabled_=false;pending_=false;++generation_;}
        while(busy_)vTaskDelay(1);
    }
    static void worker_entry(void*ptr){
        auto*self=static_cast<WebFrameCache*>(ptr);
        for(;;){
            ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
            WebPage page;uint32_t generation;
            {std::lock_guard<std::mutex> lock(self->mutex_);if(!self->pending_||!self->enabled_)continue;
             page=self->requested_;generation=self->generation_.load();self->pending_=false;self->busy_=true;}
            self->build(page,[&]{return generation!=self->generation_.load();});self->busy_=false;
        }
    }
#endif
};
}
