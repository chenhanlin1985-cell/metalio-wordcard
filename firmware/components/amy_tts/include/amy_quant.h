#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Per-render workspace is owned above the C cancellation boundary. */
#define AMY_QUANT_BYTES (640u*1024u)
enum { AMY_FP32=0, AMY_INT16=1, AMY_MIXED=2, AMY_PACKED16=3 };
void amy_quant_begin(int mode,void *workspace,size_t bytes);
void amy_quant_end(void);
int amy_quant_failed(void);
void amy_quant_report(void);
void amy_quant_stats(unsigned *simd,unsigned *scalar,int *check);
int amy_quant_conv(float *out,const float *x,const float *w,const float *bias,
 int ic,int oc,int t,int k,int dilation,int accumulate,int stride,int pad);
int64_t amy_quant_dot_scalar(const int16_t *x,const void *w,int n,int bits);
int amy_quant_round(float value);
float amy_quant_float(int64_t value);
#ifdef __cplusplus
}
#endif
