#pragma once
#include "amy_stream.h"
#include "amy_parallel.h"
#include "snt_front_f32.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t amy_now_us(void);
enum {AMY_P_DURATION,AMY_P_LATENT,AMY_P_PRE,AMY_P_UP0,AMY_P_RES0,AMY_P_UP1,AMY_P_RES1,AMY_P_UP2,AMY_P_RES2,AMY_P_POST,AMY_P_COPY,AMY_P_PCM,AMY_P_FIR,AMY_P_COUNT};
/* Per-stage profiling was an experiment; total synthesis timing stays active. */
static inline void amy_profile_reset(void){}
static inline int amy_profile_enabled(void){return 0;}
static inline void amy_profile_mark(int stage,int64_t *since){(void)stage;(void)since;}
static inline void amy_profile_report(int64_t total_us){(void)total_us;}
void amy_compute_poll(void);
long amy_duration_run(const snt_front_model *,const int32_t *,int,float,int32_t *,float *,size_t,int (*)(void *),void *);
int amy_latent_run(const snt_front_model *,const int32_t *,const int32_t *,int,long,float *,float *,size_t,int (*)(void *),void *);
int amy_reuse_run(const snt_piperlite_model *,const float *,int,int,float *,size_t,amy_stream_sink,amy_stream_cancel,void *);
int amy_blocks_run(amy_parallel_fn,void *,int,int (*)(void *),void *);
#ifdef __cplusplus
}
#endif
