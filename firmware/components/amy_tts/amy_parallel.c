#include "amy_parallel.h"
#include "amy_compute.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#include "soc/soc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
static DRAM_ATTR StackType_t helper_stack[2][4096/sizeof(StackType_t)];
static StaticTask_t helper_tcb[2];
static StaticSemaphore_t start_storage[2],done_storage[2];
static SemaphoreHandle_t start_event[2],done_event[2];
static TaskHandle_t helper[2];
typedef int atomic_int;
static int get(const atomic_int *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void put(atomic_int *p,int n){__atomic_store_n(p,n,__ATOMIC_RELEASE);}
static int add(atomic_int *p,int n){return __atomic_fetch_add(p,n,__ATOMIC_ACQ_REL);}
static void give(SemaphoreHandle_t e){xSemaphoreGive(e);}
static int take(SemaphoreHandle_t e,int ms){return xSemaphoreTake(e,ms<0?portMAX_DELAY:pdMS_TO_TICKS(ms))==pdTRUE;}
static void pause_helper(void){vTaskDelay(1);}
#elif defined(_WIN32)
#include <windows.h>
static HANDLE start_event[2],done_event[2],helper[2];
typedef volatile LONG atomic_int;
static int get(const atomic_int *p){return (int)InterlockedCompareExchange((volatile LONG *)p,0,0);}
static void put(atomic_int *p,int n){InterlockedExchange(p,n);}
static int add(atomic_int *p,int n){return (int)InterlockedExchangeAdd(p,n);}
static void give(HANDLE e){SetEvent(e);}
static int take(HANDLE e,int ms){return WaitForSingleObject(e,ms<0?INFINITE:(DWORD)ms)==WAIT_OBJECT_0;}
static void pause_helper(void){Sleep(0);}
#else
#error Amy parallel backend requires ESP-IDF or the Windows host test runtime
#endif
static atomic_int stop_requested,next_channel,active_run;
static amy_parallel_fn job_fn;
static void *job_context;
static int job_limit,pending_mask,init_failed;
static int64_t helper_yielded[2];
static unsigned jobs,helper_jobs[2],copy_calls[2],pie_copies,groups[2];
static int64_t lane_us[2],yield_us[2],last_done[2],dispatch_us,tail_gap_us;
static int coordinator_core=-1,worker_core[2]={-1,-1};
static void run_lane(int lane){
    for(;;){
        if(amy_parallel_poll(lane))return;
        const int first=add(&next_channel,1);
        if(first>=job_limit)return;
        const int limit=first+1;
        job_fn(job_context,first,limit,lane);
        ++groups[lane];
    }
}
static void helper_loop(int lane){
    for(;;){
        take(start_event[lane],-1);
        helper_yielded[lane]=amy_now_us();
#ifdef ESP_PLATFORM
        worker_core[lane]=xPortGetCoreID();
#else
        worker_core[lane]=1-lane;
#endif
        const int64_t begin=amy_now_us();
        run_lane(lane);
        last_done[lane]=amy_now_us();lane_us[lane]+=last_done[lane]-begin;
        ++helper_jobs[lane];
        give(done_event[lane]);
    }
}
#ifdef ESP_PLATFORM
static void helper_entry(void *arg){helper_loop((int)(uintptr_t)arg);}
#else
static DWORD WINAPI helper_entry(LPVOID arg){helper_loop((int)(uintptr_t)arg);return 0;}
#endif
static int ensure_helper(void){
    if(helper[0] && helper[1])return 1;
    if(init_failed)return 0;
    for(int lane=0;lane<2;++lane){
#ifdef ESP_PLATFORM
    start_event[lane]=xSemaphoreCreateBinaryStatic(&start_storage[lane]);
    done_event[lane]=xSemaphoreCreateBinaryStatic(&done_storage[lane]);
    if(start_event[lane] && done_event[lane])helper[lane]=xTaskCreateStaticPinnedToCore(helper_entry,lane?"amy.core0":"amy.core1",sizeof(helper_stack[lane]),(void *)(uintptr_t)lane,6,helper_stack[lane],&helper_tcb[lane],1-lane);
#else
    start_event[lane]=CreateEventW(NULL,FALSE,FALSE,NULL);
    done_event[lane]=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(start_event[lane] && done_event[lane])helper[lane]=CreateThread(NULL,0,helper_entry,(void *)(uintptr_t)lane,0,NULL);
#endif
    if(!helper[lane]){init_failed=1;return 0;}
    }
    return 1;
}
int amy_parallel_active(void){return get(&active_run);}
void amy_parallel_abort(void){
    if(!get(&active_run))return;
    put(&stop_requested,1);
    /* Never abandon the helper: it still holds the caller's arena/context. */
    for(int lane=0;lane<2;++lane)if(pending_mask&(1<<lane))take(done_event[lane],-1);
    pending_mask=0;put(&active_run,0);
}
int amy_parallel_poll(int lane){
    if(!get(&active_run)){amy_compute_poll();return 0;}
    if(get(&stop_requested))return 1;
    const int64_t now=amy_now_us();
    if(now-helper_yielded[lane]>=amy_parallel_yield_interval()){pause_helper();helper_yielded[lane]=amy_now_us();yield_us[lane]+=helper_yielded[lane]-now;}
    return get(&stop_requested);
}
static void dispatch(amy_parallel_fn fn,void *context,int channels){
#ifdef ESP_PLATFORM
    coordinator_core=xPortGetCoreID();
#else
    coordinator_core=1;
#endif
    /* No task migration or nested dispatch. Keep the serial path available. */
    if(coordinator_core!=1 || !ensure_helper()){
        groups[0]+=(unsigned)(channels+3)/4;
        fn(context,0,channels,0);return;
    }
    job_fn=fn;job_context=context;job_limit=channels;put(&next_channel,0);
    put(&stop_requested,0);pending_mask=3;put(&active_run,1);++jobs;
    const int64_t dispatch_begin=amy_now_us();
    give(start_event[0]);
    if(pending_mask&2)give(start_event[1]);
    while(pending_mask){
        for(int lane=0;lane<2;++lane){
            if((pending_mask&(1<<lane)) && take(done_event[lane],1))pending_mask&=~(1<<lane);
            amy_compute_poll();
        }
    }
    put(&active_run,0);
    dispatch_us+=amy_now_us()-dispatch_begin;
    {const int64_t d=last_done[0]-last_done[1];tail_gap_us+=d<0?-d:d;}
    amy_compute_poll();
}
void amy_parallel_for_blocks(amy_parallel_fn fn,void *context,int blocks){dispatch(fn,context,blocks);}
void amy_parallel_reset_stats(void){
    if(get(&active_run))return;
    jobs=helper_jobs[0]=helper_jobs[1]=copy_calls[0]=copy_calls[1]=pie_copies=groups[0]=groups[1]=0;
    lane_us[0]=lane_us[1]=yield_us[0]=yield_us[1]=last_done[0]=last_done[1]=dispatch_us=tail_gap_us=0;
}
void amy_parallel_report(void){
    printf("AMY_PARALLEL cores=%d pie_copy=%d jobs=%u primary_jobs=%u helper_jobs=%u owner_core=%d helper_core=%d groups=%u/%u copies=%u/%u pie_copies=%u pending=%d\n",
        2,0,jobs,helper_jobs[0],helper_jobs[1],worker_core[0],worker_core[1],groups[0],groups[1],copy_calls[0],copy_calls[1],pie_copies,pending_mask);
    if(amy_profile_enabled())printf("AMY_THREAD_TIME dispatch=%lld core1=%lld core0=%lld yield1=%lld yield0=%lld completion_gap=%lld fp_options=%d\n",(long long)dispatch_us,(long long)lane_us[0],(long long)lane_us[1],(long long)yield_us[0],(long long)yield_us[1],(long long)tail_gap_us,amy_fp32_options());
#ifdef ESP_PLATFORM
    printf("AMY_STACK core1=%p free_min=%u core0=%p free_min=%u (bytes)\n",(void *)helper_stack[0],helper[0]?(unsigned)uxTaskGetStackHighWaterMark(helper[0]):0,(void *)helper_stack[1],helper[1]?(unsigned)uxTaskGetStackHighWaterMark(helper[1]):0);
#endif
}
void amy_parallel_copy(void *dest,const void *src,size_t bytes,int lane){
    ++copy_calls[lane];
    memcpy(dest,src,bytes);
}
