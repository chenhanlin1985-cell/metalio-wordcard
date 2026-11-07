#include "amy_compute.h"
#include "amy_parallel.h"
/* snt_piperlite.c -- portable fp32 reference of the piperlite decoder.
 * Plain C99, no malloc: all working memory comes from the caller's arena.
 * Semantics mirror tools/train_roota_piper_decoder_student.py exactly
 * (variant "piperlite", leaky_relu): see snt_piperlite.h for the graph.
 *
 * Weight slot order is fixed by tools/export_piperlite_golden.py; meta.bin
 * carries dims plus a per-slot (offset,size) table, and init() verifies each
 * slot size against the shape implied by the dims, so a stale or reordered
 * blob fails loudly instead of decoding garbage.
 */
#include "snt_piperlite.h"

#include <math.h>
#include <string.h>

#define PL_HOP SNT_PIPERLITE_HOP

/* ---- meta.bin parsing ------------------------------------------------- */

static int32_t rd_i32(const unsigned char *p) {
    /* little-endian, alignment-safe */
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static float rd_f32(const unsigned char *p) {
    union { uint32_t u; float f; } v;
    v.u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
          ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return v.f;
}

/* Expected float count of weight slot `idx` given the model dims;
 * returns -1 for an out-of-range slot. Order must match the exporter. */
static long slot_floats(const snt_piperlite_model *m, int idx) {
    static const int bk[3] = {3, 5, 7}; /* residual-bank branch kernels */
    if (idx < 0) return -1;
    if (idx == 0) return (long)m->c0 * m->in_ch * 7;
    if (idx == 1) return m->c0;
    /* three upsample stages, each: up_w, up_b, then 3 branches x 4 tensors */
    {
        static const int up_k[3] = {16, 16, 8};
        int in_c[3], out_c[3];
        int s, base = 2;
        in_c[0] = m->c0; out_c[0] = m->c1;
        in_c[1] = m->c1; out_c[1] = m->c2;
        in_c[2] = m->c2; out_c[2] = m->c3;
        for (s = 0; s < 3; s++) {
            if (idx == base) return (long)in_c[s] * out_c[s] * up_k[s];
            if (idx == base + 1) return out_c[s];
            if (idx < base + 14) {
                int r = idx - (base + 2);      /* 0..11 within the bank */
                int branch = r / 4, part = r % 4;
                int ch = out_c[s];
                if (part == 0 || part == 2) return (long)ch * ch * bk[branch];
                return ch;
            }
            base += 14;
        }
        /* base == 44 here */
        if (idx == base) return (long)m->c3 * 7;      /* post.weight [1,c3,7] */
        if (idx == base + 1) return 1;                 /* post.bias */
        base += 2;
        if (m->pf_channels > 0) {
            int l;
            if (idx == base) return (long)m->pf_channels * m->pf_kernel;
            if (idx == base + 1) return m->pf_channels;
            base += 2;
            for (l = 0; l < m->pf_layers; l++) {
                if (idx == base) return 1;             /* unit scale */
                if (idx == base + 1 || idx == base + 3)
                    return (long)m->pf_channels * m->pf_channels * 3;
                if (idx == base + 2 || idx == base + 4) return m->pf_channels;
                base += 5;
            }
            if (idx == base) return (long)m->pf_channels * m->pf_kernel;
            if (idx == base + 1) return 1;
        }
    }
    return -1;
}

int snt_piperlite_init(snt_piperlite_model *m,
                       const void *meta, size_t meta_bytes,
                       const float *weights, size_t weight_floats) {
    const unsigned char *p = (const unsigned char *)meta;
    int i, expected_tensors;
    if (!m || !p || !weights) return -1;
    if (meta_bytes < 12 * 4) return -2;
    memset(m, 0, sizeof *m);
    if (rd_i32(p) != (int32_t)SNT_PIPERLITE_MAGIC) return -3;
    if (rd_i32(p + 4) != 1) return -4; /* version */
    m->in_ch = rd_i32(p + 8);
    m->c0 = rd_i32(p + 12);
    m->c1 = rd_i32(p + 16);
    m->c2 = rd_i32(p + 20);
    m->c3 = rd_i32(p + 24);
    m->pf_channels = rd_i32(p + 28);
    m->pf_layers = rd_i32(p + 32);
    m->pf_kernel = rd_i32(p + 36);
    m->pf_scale = rd_f32(p + 40);
    m->n_tensors = rd_i32(p + 44);
    if (m->in_ch <= 0 || m->c0 <= 0 || m->c1 <= 0 || m->c2 <= 0 || m->c3 <= 0)
        return -5;
    if (m->pf_channels < 0 || m->pf_layers < 0 ||
        (m->pf_channels == 0) != (m->pf_layers == 0))
        return -5;
    if (m->pf_channels > 0 && (m->pf_kernel <= 0 || m->pf_kernel % 2 == 0))
        return -5;
    expected_tensors =
        46 + (m->pf_channels > 0 ? 4 + 5 * m->pf_layers : 0);
    if (m->n_tensors != expected_tensors ||
        m->n_tensors > SNT_PIPERLITE_MAX_TENSORS)
        return -6;
    if (meta_bytes < (size_t)(12 + 2 * m->n_tensors) * 4) return -2;
    for (i = 0; i < m->n_tensors; i++) {
        long off = rd_i32(p + 48 + 8 * i);
        long size = rd_i32(p + 52 + 8 * i);
        long want = slot_floats(m, i);
        if (off < 0 || size <= 0 || want < 0) return -7;
        if (size != want) return -8;
        if ((size_t)off + (size_t)size > weight_floats) return -9;
        m->w[i] = weights + off;
    }
    return 0;
}

/* ---- kernels ----------------------------------------------------------- */

static void pl_leaky_copy(float *dst, const float *src, long n, float slope) {
    if(amy_fp32_options()&32){amy_fp32_pointwise(dst,src,(int)n,0,slope);return;}
    long i;
    for (i = 0; i < n; i++) {
        float v = src[i];
        dst[i] = v > 0.0f ? v : slope * v;
    }
}

/* Conv1d, PyTorch "same" semantics: pad = dil*(K/2) zeros each side.
 * w layout [out_ch, in_ch, K]; x,out channel-major [ch][T].
 * accumulate!=0: out += bias + conv; else out = bias + conv. */
#include "amy_parallel.h"
/* The single audio worker owns this bounded accumulator. HP DRAM avoids the
 * task's LP SRAM stack. Preserve upstream ic/k reduction order and fp32 math. */
#ifdef ESP_PLATFORM
#include "esp_attr.h"
static DRAM_ATTR float amy_conv_tile[2][4][128] __attribute__((aligned(64)));
#else
static float amy_conv_tile[2][4][128];
#endif
/* Quantized layers and FP32 layers run sequentially, after both workers join.
 * Reuse this HP allocation instead of taking more SD/DMA-capable SRAM. */
void *amy_conv_shared_scratch(void){return amy_conv_tile;}

static void pl_conv1d_range(float *out, const float *x, const float *w,
                      const float *b, int in_ch, int out_ch, int T,
                      int K, int dil, int accumulate,int first,int limit,int worker,int time_begin,int time_end) {
    const int pad=dil*(K/2);
    const int time_step=(amy_fp32_options()&8)?4:2;
    int oc=first;
    /* Four output channels share each input load. The reduction for each
     * channel is still ic then k; never sum partial dot products together. */
    for(;oc+4<=limit;oc+=4) {
        float *a=amy_conv_tile[worker][0],*c=amy_conv_tile[worker][1],*d=amy_conv_tile[worker][2],*e=amy_conv_tile[worker][3];
        for(int begin=time_begin;begin<time_end;begin+=128) {
            const int end=begin+128<time_end?begin+128:time_end;
            for(int lane=0;lane<4;++lane)for(int t=begin;t<end;++t)
                amy_conv_tile[worker][lane][t-begin]=accumulate?out[(long)(oc+lane)*T+t]+b[oc+lane]:b[oc+lane];
            for(int ic=0;ic<in_ch;++ic) {
                if((ic&7)==0 && amy_parallel_poll(worker))return;
                const float *xrow=x+(long)ic*T,*wrow=w+((long)oc*in_ch+ic)*K;
                /* Keep two time positions in registers for the complete K
                 * reduction of this one input channel. Unlike reducing all
                 * channels per tiny tile, this retains contiguous x reuse. */
                int dense=1;
                for(int k=0;k<K;++k)if(wrow[k]==0.f || wrow[(long)in_ch*K+k]==0.f || wrow[(long)2*in_ch*K+k]==0.f || wrow[(long)3*in_ch*K+k]==0.f){dense=0;break;}
                int mid_lo=begin>pad?begin:pad;
                if(mid_lo>end)mid_lo=end;
                int mid_hi=end<T-pad?end:T-pad;
                if(mid_hi<mid_lo)mid_hi=mid_lo;
                int interior_end=mid_lo+((mid_hi-mid_lo)&~(time_step-1));
                if(!dense){mid_lo=begin;interior_end=begin;}
                if(time_step==4)for(int t=mid_lo;t<interior_end;t+=4){
                    const int j=t-begin;
                    float a0=a[j],a1=a[j+1],a2=a[j+2],a3=a[j+3];
                    float b0=c[j],b1=c[j+1],b2=c[j+2],b3=c[j+3];
                    float c0=d[j],c1=d[j+1],c2=d[j+2],c3=d[j+3];
                    float d0=e[j],d1=e[j+1],d2=e[j+2],d3=e[j+3];
                    for(int k=0;k<K;++k){
                        const int at=t+k*dil-pad;
                        const float v0=xrow[at],v1=xrow[at+1],v2=xrow[at+2],v3=xrow[at+3];
                        const float w0=wrow[k],w1=wrow[(long)in_ch*K+k],w2=wrow[(long)2*in_ch*K+k],w3=wrow[(long)3*in_ch*K+k];
                        a0+=w0*v0;a1+=w0*v1;a2+=w0*v2;a3+=w0*v3;
                        b0+=w1*v0;b1+=w1*v1;b2+=w1*v2;b3+=w1*v3;
                        c0+=w2*v0;c1+=w2*v1;c2+=w2*v2;c3+=w2*v3;
                        d0+=w3*v0;d1+=w3*v1;d2+=w3*v2;d3+=w3*v3;
                    }
                    a[j]=a0;a[j+1]=a1;a[j+2]=a2;a[j+3]=a3;
                    c[j]=b0;c[j+1]=b1;c[j+2]=b2;c[j+3]=b3;
                    d[j]=c0;d[j+1]=c1;d[j+2]=c2;d[j+3]=c3;
                    e[j]=d0;e[j+1]=d1;e[j+2]=d2;e[j+3]=d3;
                }
                else for(int t=mid_lo;t<interior_end;t+=2){
                    const int j=t-begin;
                    float a0=a[j],a1=c[j],a2=d[j],a3=e[j];
                    float b0=a[j+1],b1=c[j+1],b2=d[j+1],b3=e[j+1];
                    for(int k=0;k<K;++k){
                        const int at=t+k*dil-pad;
                        const float v0=xrow[at],v1=xrow[at+1];
                        const float w0=wrow[k],w1=wrow[(long)in_ch*K+k],w2=wrow[(long)2*in_ch*K+k],w3=wrow[(long)3*in_ch*K+k];
                        a0+=w0*v0;a1+=w1*v0;a2+=w2*v0;a3+=w3*v0;
                        b0+=w0*v1;b1+=w1*v1;b2+=w2*v1;b3+=w3*v1;
                    }
                    a[j]=a0;c[j]=a1;d[j]=a2;e[j]=a3;
                    a[j+1]=b0;c[j+1]=b1;d[j+1]=b2;e[j+1]=b3;
                }
                for(int edge=0;edge<2;++edge){
                for(int k=0;k<K;++k) {
                    const float w0=wrow[k],w1=wrow[(long)in_ch*K+k],w2=wrow[(long)2*in_ch*K+k],w3=wrow[(long)3*in_ch*K+k];
                    const int shift=k*dil-pad;
                    int lo=edge?interior_end:begin,hi=edge?end:mid_lo;
                    if(lo+shift<0)lo=-shift;
                    if(hi+shift>T)hi=T-shift;
                    if(w0!=0.f && w1!=0.f && w2!=0.f && w3!=0.f) {
                        for(int t=lo;t<hi;++t){const float v=xrow[t+shift];const int j=t-begin;
                            a[j]+=w0*v;c[j]+=w1*v;d[j]+=w2*v;e[j]+=w3*v;}
                    } else {
                        for(int t=lo;t<hi;++t){const float v=xrow[t+shift];const int j=t-begin;
                            if(w0!=0.f)a[j]+=w0*v;
                            if(w1!=0.f)c[j]+=w1*v;
                            if(w2!=0.f)d[j]+=w2*v;
                            if(w3!=0.f)e[j]+=w3*v;}
                    }
                }
                }
            }
            for(int lane=0;lane<4;++lane)amy_parallel_copy(out+(long)(oc+lane)*T+begin,amy_conv_tile[worker][lane],(size_t)(end-begin)*sizeof(float),worker);
        }
    }
    float *tile=amy_conv_tile[worker][0];
    for(;oc<limit;++oc) {
        float *orow=out+(long)oc*T;
        for(int begin=time_begin;begin<time_end;begin+=128) {
            const int end=begin+128<time_end?begin+128:time_end;
            for(int t=begin;t<end;++t)tile[t-begin]=accumulate?orow[t]+b[oc]:b[oc];
            for(int ic=0;ic<in_ch;++ic) {
                if((ic&7)==0 && amy_parallel_poll(worker))return;
                const float *xrow=x+(long)ic*T,*wrow=w+((long)oc*in_ch+ic)*K;
                for(int k=0;k<K;++k) {
                    const float wv=wrow[k]; const int shift=k*dil-pad;
                    if(wv==0.f)continue;
                    int lo=begin,hi=end;
                    if(lo+shift<0)lo=-shift;
                    if(hi+shift>T)hi=T-shift;
                    for(int t=lo;t<hi;++t)tile[t-begin]+=wv*xrow[t+shift];
                }
            }
            amy_parallel_copy(orow+begin,tile,(size_t)(end-begin)*sizeof(float),worker);
        }
    }
}

static void pl_convtr1d_range(float *out, const float *x, const float *w,
                        const float *b, int in_ch, int out_ch, int T,
                        int K, int stride, int pad,int first,int limit,int worker,int time_begin,int time_end) {
    const int L=(T-1)*stride-2*pad+K;
    int oc=first;
    for(;oc+4<=limit;oc+=4) {
        float *a=amy_conv_tile[worker][0],*c=amy_conv_tile[worker][1],*d=amy_conv_tile[worker][2],*e=amy_conv_tile[worker][3];
        for(int begin=time_begin;begin<time_end;begin+=128) {
            const int end=begin+128<time_end?begin+128:time_end;
            for(int lane=0;lane<4;++lane)for(int j=begin;j<end;++j)amy_conv_tile[worker][lane][j-begin]=b[oc+lane];
            for(int ic=0;ic<in_ch;++ic) {
                if((ic&7)==0 && amy_parallel_poll(worker))return;
                const float *xrow=x+(long)ic*T,*wrow=w+((long)ic*out_ch+oc)*K;
                for(int k=0;k<K;++k) {
                    const float w0=wrow[k],w1=wrow[K+k],w2=wrow[2*K+k],w3=wrow[3*K+k];
                    const int shift=k-pad;
                    if(shift>=end)continue;
                    int lo=0,hi=T;
                    if(shift<begin)lo=(begin-shift+stride-1)/stride;
                    const long lim=((long)end-1-shift)/stride+1;
                    if(lim<hi)hi=(int)lim;
                    for(int t=lo;t<hi;++t) {
                        const int j=t*stride+shift-begin;const float v=xrow[t];
                        a[j]+=w0*v;c[j]+=w1*v;d[j]+=w2*v;e[j]+=w3*v;
                    }
                }
            }
            for(int lane=0;lane<4;++lane)amy_parallel_copy(out+(long)(oc+lane)*L+begin,amy_conv_tile[worker][lane],(size_t)(end-begin)*sizeof(float),worker);
        }
    }
    float *tile=amy_conv_tile[worker][0];
    for(;oc<limit;++oc) {
        float *orow=out+(long)oc*L;
        for(int begin=time_begin;begin<time_end;begin+=128) {
            const int end=begin+128<time_end?begin+128:time_end;
            for(int j=begin;j<end;++j)tile[j-begin]=b[oc];
            for(int ic=0;ic<in_ch;++ic) {
                if((ic&7)==0 && amy_parallel_poll(worker))return;
                const float *xrow=x+(long)ic*T,*wrow=w+((long)ic*out_ch+oc)*K;
                for(int k=0;k<K;++k) {
                    const float wv=wrow[k];const int shift=k-pad;
                    if(shift>=end)continue;
                    int lo=0,hi=T;
                    if(shift<begin)lo=(begin-shift+stride-1)/stride;
                    const long lim=((long)end-1-shift)/stride+1;
                    if(lim<hi)hi=(int)lim;
                    for(int t=lo;t<hi;++t)tile[(long)t*stride+shift-begin]+=wv*xrow[t];
                }
            }
            amy_parallel_copy(orow+begin,tile,(size_t)(end-begin)*sizeof(float),worker);
        }
    }
}

typedef struct {
    float *out; const float *x,*w,*b;
    int in_ch,out_ch,T,K,dil,accumulate,stride,pad,transpose;
} AmyConvJob;
#ifdef ESP_PLATFORM
static DRAM_ATTR AmyConvJob amy_hp_job;
#else
static AmyConvJob amy_hp_job;
#endif
static void amy_conv_dispatch(AmyConvJob *job);
static void amy_conv_time_job(void *context,int first,int limit,int worker){
    const AmyConvJob *j=context;
    const int length=j->transpose?(j->T-1)*j->stride-2*j->pad+j->K:j->T;
    const int nt=(length+127)/128;
    for(int index=first;index<limit;++index){
        const int oc=(index/nt)*4,oc_end=oc+4<j->out_ch?oc+4:j->out_ch;
        const int begin=(index%nt)*128,end=begin+128<length?begin+128:length;
        if(j->transpose)pl_convtr1d_range(j->out,j->x,j->w,j->b,j->in_ch,j->out_ch,j->T,j->K,j->stride,j->pad,oc,oc_end,worker,begin,end);
        else pl_conv1d_range(j->out,j->x,j->w,j->b,j->in_ch,j->out_ch,j->T,j->K,j->dil,j->accumulate,oc,oc_end,worker,begin,end);
    }
}
static void pl_conv1d(float *out,const float *x,const float *w,const float *b,int in_ch,int out_ch,int T,int K,int dil,int accumulate) {
    AmyConvJob job={out,x,w,b,in_ch,out_ch,T,K,dil,accumulate,0,0,0};
    amy_conv_dispatch(&job);
}
static void pl_convtr1d(float *out,const float *x,const float *w,const float *b,int in_ch,int out_ch,int T,int K,int stride,int pad) {
    AmyConvJob job={out,x,w,b,in_ch,out_ch,T,K,0,0,stride,pad,1};
    amy_conv_dispatch(&job);
}
static void amy_conv_dispatch(AmyConvJob *job){
    amy_hp_job=*job;job=&amy_hp_job;
    const int len=job->transpose?(job->T-1)*job->stride-2*job->pad+job->K:job->T;
    amy_parallel_for_blocks(amy_conv_time_job,job,((job->out_ch+3)/4)*((len+127)/128));
}
void amy_fp32_front_conv(float *out,const float *x,const float *w,const float *b,int ic,int oc,int T,int K,int accumulate){
    AmyConvJob job={out,x,w,b,ic,oc,T,K,1,accumulate,0,0,0};
    amy_conv_dispatch(&job);
}
typedef struct {float *out;const float *src;int n,operation;float factor;} AmyPointJob;
#ifdef ESP_PLATFORM
static DRAM_ATTR AmyPointJob amy_point_job;
#else
static AmyPointJob amy_point_job;
#endif
static void amy_point_worker(void *context,int first,int limit,int lane){
    const AmyPointJob *j=context;int end=limit*256<j->n?limit*256:j->n;
    for(int i=first*256;i<end;++i){const float v=j->src[i];
        if(j->operation==0)j->out[i]=v>0.f?v:j->factor*v;
        else if(j->operation==1)j->out[i]+=v;
        else j->out[i]=v*j->factor;
    }
}
void amy_fp32_pointwise(float *out,const float *src,int n,int operation,float factor){
    amy_point_job=(AmyPointJob){out,src,n,operation,factor};
    amy_parallel_for_blocks(amy_point_worker,&amy_point_job,(n+255)/256);
}

/* PiperResidualBank: 3 branches (kernel, dil1, dil2) = (3,1,2), (5,2,6),
 * (7,3,12); each branch y1 = conv1(lrelu(x)) + x, y2 = conv2(lrelu(y1)) + y1;
 * bank out = (y2_0 + y2_1 + y2_2) / 3. `ws` points at the 12 bank slots.
 * Buffers sum/t/u are ch*T floats each; out may alias sum. */
static void pl_res_bank(float *out, const float *x, const float *const *ws,
                        int ch, int T, float *sum, float *t, float *u) {
    static const int bk[3] = {3, 5, 7};
    static const int bd1[3] = {1, 2, 3};
    static const int bd2[3] = {2, 6, 12};
    long n = (long)ch * T;
    long i;
    int br;
    memset(sum, 0, (size_t)n * sizeof(float));
    for (br = 0; br < 3; br++) {
        const float *w1 = ws[br * 4 + 0], *b1 = ws[br * 4 + 1];
        const float *w2 = ws[br * 4 + 2], *b2 = ws[br * 4 + 3];
        pl_leaky_copy(t, x, n, 0.1f);
        pl_conv1d(u, t, w1, b1, ch, ch, T, bk[br], bd1[br], 0);
        if(amy_fp32_options()&32)amy_fp32_pointwise(u,x,(int)n,1,0);else for (i = 0; i < n; i++) u[i] += x[i];      /* y1 */
        pl_leaky_copy(t, u, n, 0.1f);
        pl_conv1d(sum, t, w2, b2, ch, ch, T, bk[br], bd2[br], 1); /* += conv */
        if(amy_fp32_options()&32)amy_fp32_pointwise(sum,u,(int)n,1,0);else for (i = 0; i < n; i++) sum[i] += u[i];    /* += y1 */
    }
    if(amy_fp32_options()&32)amy_fp32_pointwise(out,sum,(int)n,2,1.0f/3.0f);else for (i = 0; i < n; i++) out[i] = sum[i] * (1.0f / 3.0f);
}

/* ---- arena sizing ------------------------------------------------------ */

static long pl_lane_floats(const snt_piperlite_model *m, int frames) {
    long s = (long)m->c0 * frames;
    long v = (long)m->c1 * 8 * frames;
    if (v > s) s = v;
    v = (long)m->c2 * 64 * frames;
    if (v > s) s = v;
    v = (long)m->c3 * PL_HOP * frames;
    if (v > s) s = v;
    v = (long)(m->pf_channels > 0 ? m->pf_channels : 1) * PL_HOP * frames;
    if (v > s) s = v;
    return s;
}

size_t snt_piperlite_arena_floats(const snt_piperlite_model *m, int frames) {
    if (!m || frames <= 0) return 0;
    return (size_t)(4 * pl_lane_floats(m, frames));
}

/* ---- forward ------------------------------------------------------------ */

static void pl_tap(const snt_piperlite_model *m, const char *name,
                   const float *data, int ch, long len) {
    if (m->stage_cb) m->stage_cb(name, data, ch, (int)len, m->stage_user);
}

int snt_piperlite_synthesize(const snt_piperlite_model *m,
                             const float *z, int frames,
                             float *audio_out,
                             float *arena, size_t arena_floats) {
    long lane, n_samples;
    float *ln[4];
    float *x;
    int cur, s, i;
    long j;
    /* per-stage geometry: up kernel/stride/pad, weight slot bases */
    static const int up_k[3] = {16, 16, 8};
    static const int up_s[3] = {8, 8, 4};
    static const int up_p[3] = {4, 4, 2};
    static const char *up_name[3] = {"up0", "up1", "up2"};
    static const char *mix_name[3] = {"stage0_mix", "stage1_mix", "stage2_mix"};

    if (!m || !z || frames <= 0 || !audio_out || !arena) return -1;
    if (arena_floats < snt_piperlite_arena_floats(m, frames)) return -2;
    lane = pl_lane_floats(m, frames);
    for (i = 0; i < 4; i++) ln[i] = arena + (long)i * lane;
    n_samples = (long)frames * PL_HOP;

    /* pre: Conv1d(in_ch, c0, 7, pad 3) */
    pl_conv1d(ln[0], z, m->w[0], m->w[1], m->in_ch, m->c0, frames, 7, 1, 0);
    pl_tap(m, "pre", ln[0], m->c0, frames);
    x = ln[0];
    cur = 0;

    {
        int T = frames;
        int in_c[3], out_c[3];
        in_c[0] = m->c0; out_c[0] = m->c1;
        in_c[1] = m->c1; out_c[1] = m->c2;
        in_c[2] = m->c2; out_c[2] = m->c3;
        for (s = 0; s < 3; s++) {
            int base = 2 + s * 14;
            int nxt = (cur + 1) % 4;
            int b_sum = (cur + 2) % 4, b_t = (cur + 3) % 4, b_u = cur;
            int L = (T - 1) * up_s[s] - 2 * up_p[s] + up_k[s];
            /* act (slope 0.1) then upsample */
            pl_leaky_copy(x, x, (long)in_c[s] * T, 0.1f);
            pl_convtr1d(ln[nxt], x, m->w[base], m->w[base + 1],
                        in_c[s], out_c[s], T, up_k[s], up_s[s], up_p[s]);
            pl_tap(m, up_name[s], ln[nxt], out_c[s], L);
            /* residual bank; result lands in the sum lane */
            pl_res_bank(ln[b_sum], ln[nxt], &m->w[base + 2], out_c[s], L,
                        ln[b_sum], ln[b_t], ln[b_u]);
            pl_tap(m, mix_name[s], ln[b_sum], out_c[s], L);
            x = ln[b_sum];
            cur = b_sum;
            T = L;
        }
        /* post: lrelu(0.01) -> Conv1d(c3, 1, 7, pad 3) -> tanh */
        pl_leaky_copy(x, x, (long)m->c3 * T, 0.01f);
        pl_conv1d(audio_out, x, m->w[44], m->w[45], m->c3, 1, T, 7, 1, 0);
        pl_tap(m, "pre_tanh", audio_out, 1, T);
        for (j = 0; j < n_samples; j++) audio_out[j] = tanhf(audio_out[j]);
        pl_tap(m, "audio_pre_filter", audio_out, 1, T);
    }

    /* optional WaveformPostFilter:
     * r = in_conv(audio); r = ResidualUnit^layers(r); r2 = out_conv(r);
     * audio = tanh(audio + pf_scale * r2). ResidualUnit(dil=1+l):
     * r += scale * conv2(lrelu(conv1(lrelu(r)))). */
    if (m->pf_channels > 0) {
        int pf = m->pf_channels, K = m->pf_kernel;
        long n = (long)pf * n_samples;
        float *r = ln[0], *t = ln[1], *u = ln[2], *v = ln[3];
        int base = 46, l;
        pl_conv1d(r, audio_out, m->w[base], m->w[base + 1], 1, pf,
                  (int)n_samples, K, 1, 0);
        base += 2;
        for (l = 0; l < m->pf_layers; l++) {
            float scale = m->w[base][0];
            pl_leaky_copy(t, r, n, 0.1f);
            pl_conv1d(u, t, m->w[base + 1], m->w[base + 2], pf, pf,
                      (int)n_samples, 3, 1 + l, 0);
            pl_leaky_copy(t, u, n, 0.1f);
            pl_conv1d(v, t, m->w[base + 3], m->w[base + 4], pf, pf,
                      (int)n_samples, 3, 1, 0);
            for (j = 0; j < n; j++) r[j] += scale * v[j];
            base += 5;
        }
        pl_conv1d(t, r, m->w[base], m->w[base + 1], pf, 1,
                  (int)n_samples, K, 1, 0);
        for (j = 0; j < n_samples; j++)
            audio_out[j] = tanhf(audio_out[j] + m->pf_scale * t[j]);
    }
    pl_tap(m, "audio", audio_out, 1, n_samples);
    return 0;
}

/* Exact receptive-field windows. Stage0 is produced once and retained for this
 * utterance; stages1/2 consume only their required context, not 12 latent frames
 * at every resolution. No causal approximation, changed padding or crossfade. */
#include <stdio.h>
typedef struct {int lo,hi;} AmySpan;
static int amy_len(AmySpan s){return s.hi-s.lo;}
static AmySpan amy_expand(AmySpan s,int radius,int full){
    s.lo=s.lo>radius?s.lo-radius:0;s.hi=s.hi+radius<full?s.hi+radius:full;return s;
}
static AmySpan amy_trans_input(AmySpan s,int stride,int pad,int kernel,int full){
    int a=s.lo+pad-(kernel-1),b=s.hi-1+pad;
    /* ceil division of signed a, with a negative result clamped to zero. */
    s.lo=a>0?(a+stride-1)/stride:0;s.hi=b/stride+1;
    if(s.hi>full)s.hi=full;
    return s;
}
static void amy_crop(float *x,int ch,int oldlen,int skip,int len){
    for(int c=0;c<ch;++c)memmove(x+(long)c*len,x+(long)c*oldlen+skip,(size_t)len*4);
}
static long amy_max(long a,long b){return a>b?a:b;}
static long amy_stage0_lane(const snt_piperlite_model *m,int frames,AmySpan target,
                           AmySpan *up,AmySpan *pre,AmySpan *latent){
    *up=amy_expand(target,45,frames*8);
    *pre=amy_trans_input(*up,8,4,16,frames);*latent=amy_expand(*pre,3,frames);
    return amy_max((long)m->in_ch*amy_len(*latent),amy_max((long)m->c0*amy_len(*latent),(long)m->c1*amy_len(*pre)*8));
}
static long amy_tail_lane(const snt_piperlite_model *m,int frames,AmySpan target,
                         AmySpan *mix0,AmySpan *up1,AmySpan *mix1,AmySpan *up2,AmySpan *mix2){
    *mix2=amy_expand(target,3,frames*256);*up2=amy_expand(*mix2,45,frames*256);
    *mix1=amy_trans_input(*up2,4,2,8,frames*64);*up1=amy_expand(*mix1,45,frames*64);
    *mix0=amy_trans_input(*up1,8,4,16,frames*8);
    return amy_max((long)m->c1*amy_len(*mix0),amy_max((long)m->c2*amy_len(*mix0)*8,(long)m->c3*amy_len(*mix1)*4));
}
static int amy_reuse_supported(const snt_piperlite_model *m,int frames,int chunk){
    return m && m->in_ch==192 && m->c0==160 && m->c1==80 && m->c2==40 && m->c3==20 &&
        !m->pf_channels && !m->pf_layers && m->n_tensors==46 && frames>0 && frames<=1033 && chunk>0 && chunk<=64;
}
size_t amy_reuse_scratch_floats(const snt_piperlite_model *m,int frames,int chunk){
    if(!amy_reuse_supported(m,frames,chunk))return 0;
    long lane=0;
    for(int lo=0;lo<frames*8;lo+=1024){AmySpan up,pre,z,t={lo,lo+1024<frames*8?lo+1024:frames*8};
        lane=amy_max(lane,amy_stage0_lane(m,frames,t,&up,&pre,&z));}
    for(int lo=0;lo<frames;lo+=chunk){AmySpan a,b,c,d,e,t={lo*256,(lo+chunk<frames?lo+chunk:frames)*256};
        lane=amy_max(lane,amy_tail_lane(m,frames,t,&a,&b,&c,&d,&e));}
    return (size_t)m->c1*frames*8+4*(size_t)lane;
}
int amy_reuse_decode(const snt_piperlite_model *m,const float *z,int frames,int chunk,
                     float *scratch,size_t cap,amy_stream_sink sink,amy_stream_cancel stop,void *user){
    const size_t need=amy_reuse_scratch_floats(m,frames,chunk);
    if(!need || !z || !scratch || !sink || !stop)return -1;
    if(cap<need)return -2;
    const int full0=frames*8;const size_t cache=(size_t)m->c1*full0,lane=(need-cache)/4;
    float *saved=scratch,*ln[4];for(int i=0;i<4;++i)ln[i]=scratch+cache+(size_t)i*lane;
    unsigned stage0_blocks=0,tail_blocks=0;
    for(int lo=0;lo<full0;lo+=1024){
        if(stop(user))return 1;
        int64_t prof=amy_now_us();
        AmySpan target={lo,lo+1024<full0?lo+1024:full0},up,pre,latent;
        amy_stage0_lane(m,frames,target,&up,&pre,&latent);
        int n=amy_len(latent);
        for(int c=0;c<m->in_ch;++c){const float *src=z+(long)c*frames+latent.lo;
            for(int j=0;j<n;++j)if(!isfinite(src[j]))return -4;
            memcpy(ln[0]+(long)c*n,src,(size_t)n*4);}
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_conv1d(ln[1],ln[0],m->w[0],m->w[1],m->in_ch,m->c0,n,7,1,0);
        amy_profile_mark(AMY_P_PRE,&prof);
        amy_crop(ln[1],m->c0,n,pre.lo-latent.lo,amy_len(pre));n=amy_len(pre);
        pl_leaky_copy(ln[1],ln[1],(long)m->c0*n,0.1f);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_convtr1d(ln[2],ln[1],m->w[2],m->w[3],m->c0,m->c1,n,16,8,4);
        amy_profile_mark(AMY_P_UP0,&prof);
        amy_crop(ln[2],m->c1,n*8,up.lo-pre.lo*8,amy_len(up));n=amy_len(up);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_res_bank(ln[0],ln[2],&m->w[4],m->c1,n,ln[0],ln[1],ln[3]);
        amy_profile_mark(AMY_P_RES0,&prof);
        const int skip=target.lo-up.lo,len=amy_len(target);
        for(int c=0;c<m->c1;++c){const float *src=ln[0]+(long)c*n+skip;
            for(int j=0;j<len;++j)if(!isfinite(src[j]))return -4;
            memcpy(saved+(long)c*full0+target.lo,src,(size_t)len*4);}
        ++stage0_blocks;
        amy_profile_mark(AMY_P_COPY,&prof);
    }
    for(int lo=0;lo<frames;lo+=chunk){
        if(stop(user))return 1;
        int64_t prof=amy_now_us();
        AmySpan target={lo*256,(lo+chunk<frames?lo+chunk:frames)*256},a,b,c,d,e;
        amy_tail_lane(m,frames,target,&a,&b,&c,&d,&e);
        int n=amy_len(a);
        for(int ch=0;ch<m->c1;++ch)memcpy(ln[0]+(long)ch*n,saved+(long)ch*full0+a.lo,(size_t)n*4);
        pl_leaky_copy(ln[0],ln[0],(long)m->c1*n,0.1f);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_convtr1d(ln[1],ln[0],m->w[16],m->w[17],m->c1,m->c2,n,16,8,4);
        amy_profile_mark(AMY_P_UP1,&prof);
        amy_crop(ln[1],m->c2,n*8,b.lo-a.lo*8,amy_len(b));n=amy_len(b);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_res_bank(ln[2],ln[1],&m->w[18],m->c2,n,ln[2],ln[3],ln[0]);
        amy_profile_mark(AMY_P_RES1,&prof);
        amy_crop(ln[2],m->c2,n,c.lo-b.lo,amy_len(c));n=amy_len(c);
        pl_leaky_copy(ln[2],ln[2],(long)m->c2*n,0.1f);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_convtr1d(ln[3],ln[2],m->w[30],m->w[31],m->c2,m->c3,n,8,4,2);
        amy_profile_mark(AMY_P_UP2,&prof);
        amy_crop(ln[3],m->c3,n*4,d.lo-c.lo*4,amy_len(d));n=amy_len(d);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_res_bank(ln[0],ln[3],&m->w[32],m->c3,n,ln[0],ln[1],ln[2]);
        amy_profile_mark(AMY_P_RES2,&prof);
        amy_crop(ln[0],m->c3,n,e.lo-d.lo,amy_len(e));n=amy_len(e);
        pl_leaky_copy(ln[0],ln[0],(long)m->c3*n,0.01f);
        amy_profile_mark(AMY_P_COPY,&prof);
        pl_conv1d(ln[1],ln[0],m->w[44],m->w[45],m->c3,1,n,7,1,0);
        amy_profile_mark(AMY_P_POST,&prof);
        float *pcm=ln[1]+target.lo-e.lo;const int count=amy_len(target);
        for(int i=0;i<count;++i){pcm[i]=tanhf(pcm[i]);if(!isfinite(pcm[i]))return -4;}
        if(stop(user) || sink(pcm,count,user))return 1;
        ++tail_blocks;
        amy_profile_mark(AMY_P_PCM,&prof);
    }
    printf("AMY_REUSE stage0_blocks=%u tail_blocks=%u stage0_cache=%u scratch=%u\n",stage0_blocks,tail_blocks,(unsigned)(cache*4),(unsigned)(need*4));
    return 0;
}
