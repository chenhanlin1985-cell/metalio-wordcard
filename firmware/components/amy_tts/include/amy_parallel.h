#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Single coordinating audio worker; both compute lanes use HP stacks. Context
 * and output remain owned until join, including cancellation/timeout. */
typedef void (*amy_parallel_fn)(void *,int first,int limit,int lane);
void amy_parallel_for_blocks(amy_parallel_fn,void *,int blocks);
int amy_parallel_poll(int lane);
void amy_parallel_abort(void);
int amy_parallel_active(void);
/* Fixed production configuration; compile-time constants erase old branches. */
static inline int amy_fp32_options(void){return 63;}
static inline int amy_parallel_yield_interval(void){return 40000;}
void amy_fp32_front_conv(float *,const float *,const float *,const float *,int,int,int,int,int);
void amy_fp32_pointwise(float *,const float *,int,int,float);
void amy_parallel_reset_stats(void);
void amy_parallel_report(void);
void amy_parallel_copy(void *dest,const void *src,size_t bytes,int lane);
#ifdef __cplusplus
}
#endif
