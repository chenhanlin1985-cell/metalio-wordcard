#include "amy_compute.h"
#include "amy_parallel.h"
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif
static jmp_buf boundary;
static int armed;
static int (*poll_fn)(void *);
static void *poll_user;
static int64_t yielded;
int64_t amy_now_us(void) {
#ifdef ESP_PLATFORM
 return esp_timer_get_time();
#elif defined(_WIN32)
 LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return t.QuadPart/f.QuadPart*1000000+t.QuadPart%f.QuadPart*1000000/f.QuadPart;
#else
 return (int64_t)clock()*1000000/CLOCKS_PER_SEC;
#endif
}
void amy_compute_poll(void) {
 if(!armed)return;
 if(poll_fn && poll_fn(poll_user)){amy_parallel_abort();longjmp(boundary,1);}
#ifdef ESP_PLATFORM
 const int64_t now=amy_now_us();if(now-yielded>=amy_parallel_yield_interval()){vTaskDelay(1);yielded=amy_now_us();}
#endif
}
/* setjmp and every jump target remain inside C. C++ callbacks have returned
 * before jumping; all owning C++ allocations live above these guard frames. */
#define BEGIN_GUARD(fn,ctx) if(armed)return -91; poll_fn=fn;poll_user=ctx;yielded=amy_now_us(); if(setjmp(boundary)){armed=0;poll_fn=0;poll_user=0;return -90;} armed=1;amy_compute_poll()
#define END_GUARD armed=0;poll_fn=0;poll_user=0
long amy_duration_run(const snt_front_model *m,const int32_t *ids,int n,float scale,int32_t *dur,float *arena,size_t cap,int (*poll)(void *),void *user){
 BEGIN_GUARD(poll,user);long rc=snt_front_durations(m,ids,n,scale,dur,arena,cap);END_GUARD;return rc;
}
int amy_latent_run(const snt_front_model *m,const int32_t *ids,const int32_t *dur,int n,long frames,float *out,float *arena,size_t cap,int (*poll)(void *),void *user){
 BEGIN_GUARD(poll,user);int rc=snt_front_latent(m,ids,dur,n,frames,out,arena,cap);END_GUARD;return rc;
}
int amy_reuse_run(const snt_piperlite_model *m,const float *z,int frames,int chunk,float *scratch,size_t cap,amy_stream_sink sink,amy_stream_cancel poll,void *user){
 amy_parallel_reset_stats();
 BEGIN_GUARD(poll,user);int rc=amy_reuse_decode(m,z,frames,chunk,scratch,cap,sink,poll,user);END_GUARD;amy_parallel_report();return rc;
}
int amy_blocks_run(amy_parallel_fn fn,void *ctx,int blocks,int (*poll)(void *),void *user){
 BEGIN_GUARD(poll,user);amy_parallel_for_blocks(fn,ctx,blocks);amy_compute_poll();END_GUARD;return 0;
}
