#pragma once
#include <cstdio>
#include <cerrno>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// Explicit serial diagnostic only. Never scans resources during normal use.
inline void sd_read_probe() {
    alignas(64) unsigned char local[4096];
    for (int kind=0;kind<4;++kind) {
        auto *buffer=kind<2?local:static_cast<unsigned char *>(heap_caps_aligned_alloc(
            64,4096,MALLOC_CAP_8BIT|(kind==2?MALLOC_CAP_INTERNAL:MALLOC_CAP_SPIRAM)));
        if(!buffer)continue;
        FILE *f=std::fopen("/sdcard/metalio/e-ink/wordcards/tts/espeak-ng-data/en_dict","rb");
        if(f && kind!=0)std::setvbuf(f,nullptr,_IONBF,0);
        uint32_t crc=0xffffffff;size_t total=0,n=0;
        const auto start=esp_timer_get_time();errno=0;
        while(f && (n=std::fread(buffer,1,4096,f))) {
            total+=n;
            for(size_t i=0;i<n;++i){crc^=buffer[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}
        }
        ESP_LOGI("sdprobe","core=%d kind=%d buffer=%p bytes=%u crc=%08lx ferror=%d errno=%d us=%lld",
            int(xPortGetCoreID()),kind,buffer,unsigned(total),(unsigned long)~crc,f?std::ferror(f):-1,errno,
            (long long)(esp_timer_get_time()-start));
        if(f)std::fclose(f);
        if(kind>=2)heap_caps_free(buffer);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
