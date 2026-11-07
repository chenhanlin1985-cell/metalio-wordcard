#include "amy_quant.h"
#include "amy_parallel.h"
#include "amy_compute.h"
#include "amy_packed.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_attr.h"
extern int64_t amy_s31_dot16(const int16_t *,const int16_t *,int);
extern int64_t amy_s31_dot8(const int16_t *,const int8_t *,int);
extern void amy_s31_mat8_16(const int16_t *,const int16_t *,int64_t *,int);
extern void amy_s31_mat8_8(const int16_t *,const int8_t *,int64_t *,int);
extern void amy_s31_mat8_16_cached(const int16_t *,const int16_t *,int64_t *,int);
extern void amy_s31_mat8_8_cached(const int16_t *,const int8_t *,int64_t *,int);
/* PIE accesses get a bounded HP working set. PSRAM remains model/storage;
 * normal CPU loads stage it through cache before the integer vector loads. */
static DRAM_ATTR union {int16_t patch[1344];struct {int16_t x[256];int64_t sums[16][8];} tile;} pie_work __attribute__((aligned(64)));
static DRAM_ATTR int64_t pie_result[8] __attribute__((aligned(64)));
extern void *amy_conv_shared_scratch(void);
#endif
// Generated from tools/amy_packed.c.inc; scalar parsing also exercised on host.
#include "amy_packed.inc"
static int mode,failed;
static unsigned char *memory;
static size_t capacity;
static unsigned calls[2],simd[2],layers;
static int64_t packing_us,kernel_us;
static int checked,simd_ok;
#ifdef ESP_PLATFORM
static int64_t profile_us[5];
static int round_checked,round_ok;
static void check_round(void);
#endif
void amy_quant_begin(int value,void *workspace,size_t bytes){
 mode=value;memory=workspace;capacity=bytes;failed=0;layers=0;
 calls[0]=calls[1]=simd[0]=simd[1]=0;
 packing_us=kernel_us=0;
#ifdef ESP_PLATFORM
 if(value)check_round();
#endif
}
void amy_quant_end(void){mode=0;memory=NULL;capacity=0;failed=0;amy_packed_unbind();}
int amy_quant_failed(void){return failed;}
void amy_quant_stats(unsigned *s,unsigned *c,int *check){*s=simd[0]+simd[1];*c=calls[0]+calls[1];*check=checked?(simd_ok?1:-1):0;}
void amy_quant_report(void){printf("AMY_QUANT mode=%d layers=%u scalar=%u/%u simd=%u/%u check=%d failed=%d workspace=%u packing_us=%lld kernel_us=%lld\n",mode,layers,calls[0],calls[1],simd[0],simd[1],checked?simd_ok:-1,failed,(unsigned)capacity,(long long)packing_us,(long long)kernel_us);}
/* Explicit ties-to-even, independent of the host's rounding environment. */
static int round_reference(float v){
 const float a=fabsf(v);int n=(int)a;const float f=a-(float)n;
 if(f>0.5f || (f==0.5f && (n&1)))++n;
 return v<0?-n:n;
}
#ifdef ESP_PLATFORM
static int round_hardware(float v){int result;__asm__ volatile("fcvt.w.s %0,%1,rne":"=r"(result):"f"(v));return result;}
static void check_round(void){
 if(round_checked)return;
 round_checked=1;round_ok=1;
 unsigned cases=0;
 for(int i=-32767;i<32767;++i){float v=i+0.5f;if(round_hardware(v)!=round_reference(v))round_ok=0;++cases;}
 uint32_t seed=173;for(int i=0;i<4096;++i){seed=seed*1664525u+1013904223u;float v=((int)(seed%262137)-131068)*0.25f;if(round_hardware(v)!=round_reference(v))round_ok=0;++cases;}
 printf("AMY_ROUND_CHECK ok=%d cases=%u mode=RNE\n",round_ok,cases);
}
#endif
int amy_quant_round(float v){
#ifdef ESP_PLATFORM
 if(round_ok)return round_hardware(v);
#endif
 return round_reference(v);
}
/* Exact RN-even int64 -> IEEE binary32 without the S31 ROM soft conversion.
 * In particular, do not approximate with rounded high + low float halves. */
float amy_quant_float(int64_t value){
 if(!value)return 0.f;
 const uint32_t sign=value<0?0x80000000u:0;
 const uint64_t a=value<0?0-(uint64_t)value:(uint64_t)value;
 const uint32_t hi=(uint32_t)(a>>32);uint32_t v=hi?hi:(uint32_t)a;int bit=hi?32:0;
 if(v>>16){v>>=16;bit+=16;}if(v>>8){v>>=8;bit+=8;}if(v>>4){v>>=4;bit+=4;}if(v>>2){v>>=2;bit+=2;}if(v>>1)++bit;
 uint32_t mant;
 if(bit<=23)mant=(uint32_t)a<<(23-bit);
 else {int shift=bit-23;mant=(uint32_t)(a>>shift);const uint64_t rem=a-((uint64_t)mant<<shift),half=(uint64_t)1<<(shift-1);
  if(rem>half || (rem==half && (mant&1)))++mant;
  if(mant==0x1000000u){mant>>=1;++bit;}
 }
 const uint32_t bits=sign|((uint32_t)(bit+127)<<23)|(mant&0x7fffffu);float out;memcpy(&out,&bits,4);return out;
}
static float scale_for(float max,int bits){
 if(max==0)return 1.f;
 int e;float m=frexpf(fmaxf(max/(bits==8?127.f:32767.f),1e-20f),&e);
 return ldexpf(1.f,m==0.5f?e-1:e);
}
int64_t amy_quant_dot_scalar(const int16_t *x,const void *w,int n,int bits){
 int64_t sum=0;
 if(bits==8){const int8_t *p=w;for(int i=0;i<n;++i)sum+=(int32_t)x[i]*p[i];}
 else {const int16_t *p=w;for(int i=0;i<n;++i)sum+=(int32_t)x[i]*p[i];}
 return sum;
}
static void mat8_scalar(const int16_t *x,const void *w,int64_t *s,int n,int bits){
 for(int o=0;o<8;++o){int64_t sum=0;
  if(bits==8){const int8_t *p=w;for(int i=0;i<n;++i)sum+=(int32_t)x[i]*p[i*8+o];}
  else {const int16_t *p=w;for(int i=0;i<n;++i)sum+=(int32_t)x[i]*p[i*8+o];}
  s[o]=sum;
 }
}
#ifdef ESP_PLATFORM
static int check_pie(void){
 if(checked)return simd_ok;
 checked=1;simd_ok=1;
 /* PSRAM, not the original audio task's LP stack. All allocation is freed
  * before returning to the coordinator's cancellation boundary. */
 int16_t *x=heap_caps_aligned_alloc(16,3072*6,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!x){simd_ok=0;return 0;}
 int16_t *w=x+3072;int8_t *w8=(int8_t *)(w+3072);
 const int sizes[]={16,240,256,272,672,1344,3072};
 for(int pattern=0;pattern<4;++pattern){
  for(int i=0;i<3072;++i){x[i]=pattern==0?-32768:pattern==1?32767:(int16_t)(i*7919+213);
   w[i]=pattern<2?x[i]:pattern==2?(int16_t)(i*3817-177):(i&1?-32768:32767);
   w8[i]=pattern<2?-128:(int8_t)(i*53-17);}
  for(unsigned j=0;j<sizeof(sizes)/sizeof(sizes[0]);++j){int n=sizes[j];
   const int64_t a=amy_s31_dot16(x,w,n),b=amy_quant_dot_scalar(x,w,n,16),c=amy_s31_dot8(x,w8,n),d=amy_quant_dot_scalar(x,w8,n,8);
   if(a!=b || c!=d){if(simd_ok)printf("AMY_INT_PIE_MISMATCH pattern=%d n=%d s16=%lld/%lld s8=%lld/%lld\n",pattern,n,(long long)a,(long long)b,(long long)c,(long long)d);simd_ok=0;}
  }
 }
 free(x);
 int16_t *mx=heap_caps_aligned_alloc(16,1344*26,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!mx)simd_ok=0;
 else {int16_t *mw=mx+1344;int8_t *m8=(int8_t *)(mw+1344*8);
  const int ns[]={16,80,176,384,672,1344};int64_t ref[8];
  for(int p=0;p<4;++p){for(int i=0;i<1344;++i){mx[i]=p==0?-32768:p==1?32767:(int16_t)(i*7919+17);
    for(int o=0;o<8;++o){mw[i*8+o]=p<2?(o&1?32767:-32768):(int16_t)(i*3817+o*977);m8[i*8+o]=p<2?(o&1?127:-128):(int8_t)(i*53+o*23);}}
   for(unsigned k=0;k<sizeof(ns)/sizeof(ns[0]);++k)for(int bits=8;bits<=16;bits+=8){
    mat8_scalar(mx,bits==8?(void*)m8:(void*)mw,ref,ns[k],bits);
    if(bits==8)amy_s31_mat8_8(mx,m8,pie_result,ns[k]);else amy_s31_mat8_16(mx,mw,pie_result,ns[k]);
    for(int o=0;o<8;++o)if(ref[o]!=pie_result[o]){if(simd_ok){printf("AMY_QACC_MISMATCH bits=%d n=%d lane=%d got=%lld want=%lld\n",bits,ns[k],o,(long long)pie_result[o],(long long)ref[o]);for(int j=0;j<8;++j)printf("QACC_LANE %d got=%lld want=%lld\n",j,(long long)pie_result[j],(long long)ref[j]);}simd_ok=0;}
    if(bits==8)amy_s31_mat8_8_cached(mx,m8,pie_result,ns[k]);else amy_s31_mat8_16_cached(mx,mw,pie_result,ns[k]);
    for(int o=0;o<8;++o)if(ref[o]!=pie_result[o]){printf("AMY_CACHED_QACC_MISMATCH bits=%d n=%d lane=%d\n",bits,ns[k],o);simd_ok=0;}
   }
  }
  if(simd_ok){
   const int n=128,repeats=256;int64_t elapsed[4];volatile int64_t keep=0;
   int16_t *hx=pie_work.patch,*hw=amy_conv_shared_scratch();
   memcpy(hx,mx,n*2);memcpy(hw,mw,n*16);
   for(int path=0;path<4;++path){const int64_t begin=amy_now_us();
    for(int r=0;r<repeats;++r){
     if(path==0)amy_s31_mat8_16(mx,mw,pie_result,n);
     else if(path==1)amy_s31_mat8_16(hx,hw,pie_result,n);
     else if(path==2)mat8_scalar(hx,hw,pie_result,n,16);
     else amy_s31_mat8_16_cached(hx,hw,pie_result,n);
     keep^=pie_result[r&7];
    }elapsed[path]=amy_now_us()-begin;
   }
   printf("AMY_INT_MICRO macs=%d psram_pie_us=%lld hp_pie_us=%lld hp_scalar_us=%lld cached_code_hp_pie_us=%lld guard=%lld\n",n*8*repeats,(long long)elapsed[0],(long long)elapsed[1],(long long)elapsed[2],(long long)elapsed[3],(long long)keep);
  }free(mx);
 }
 printf("AMY_INT_PIE_CHECK ok=%d cases=824 overflow40=blocked qacc64=checked\n",simd_ok);return simd_ok;
}
#endif
typedef struct {
 float *out;const float *bias;const int16_t *x;const unsigned char *w;float *scales;
 int ic,oc,T,L,K,dil,accumulate,stride,pad,n,bits;
 float xs;int16_t *patch[2];
} QuantJob;
/* The coordinator has an LP SRAM stack. Workers repeatedly reading a stack
 * descriptor would cross the LP bridge on every loop iteration. One render
 * owns this HP descriptor until both workers join, including cancellation. */
#ifdef ESP_PLATFORM
static DRAM_ATTR QuantJob hp_job;
#else
static QuantJob hp_job;
#endif
/* Weight-stationary tile: copy at most 4 KiB of weights into HP SRAM once,
 * reuse for 16 time positions. Input rows and weights have bounded working
 * sets. Integer partial sums remain exact64. */
static void quant_tile(const QuantJob *j,int first,int end,int lane){
#ifdef ESP_PLATFORM
 const int profile=layers<=23 && lane==0 && first<16;
 int64_t mark=profile?amy_now_us():0;
#define PROFILE_STEP(i) do {if(profile){int64_t now=amy_now_us();profile_us[i]+=now-mark;mark=now;}}while(0)
#else
#define PROFILE_STEP(i) ((void)0)
#endif
 int16_t *patch=j->patch[lane];
 /* Build all 16 input rows once, with no divide/remainder in the gather.
  * Every output-channel block reuses them. Padding stays explicit zero. */
 for(int t=first;t<end;++t){int16_t *row=patch+(size_t)(t-first)*j->n;int at=0;
  for(int c=0;c<j->ic;++c)for(int k=0;k<j->K;++k){int pos=t+k*j->dil-j->dil*(j->K/2);
   row[at++]=pos>=0 && pos<j->T?j->x[(long)pos*j->ic+c]:0;
  }
  while(at<j->n)row[at++]=0;
 }
 PROFILE_STEP(0);
#ifdef ESP_PLATFORM
 int16_t *p=pie_work.tile.x;int64_t (*total)[8]=pie_work.tile.sums;
 unsigned char *weight=amy_conv_shared_scratch();
#else
 int16_t p[256];int64_t total[16][8],result[8];
 unsigned char weight[4096];
#endif
 for(int oc=0;oc<j->oc;oc+=8){
  memset(total,0,sizeof(int64_t)*16*8);
  for(int start=0;start<j->n;start+=256){
   if(amy_parallel_poll(lane))return;
   int count=j->n-start<256?j->n-start:256;
   const unsigned char *src=j->w+((size_t)oc*j->n+(size_t)start*8)*(j->bits/8);
   memcpy(weight,src,(size_t)count*8*(j->bits/8));
   PROFILE_STEP(1);
   for(int t=first;t<end;++t){
    memcpy(p,patch+(size_t)(t-first)*j->n+start,(size_t)count*2);
    PROFILE_STEP(2);
#ifdef ESP_PLATFORM
    int64_t *r=pie_result;
    if(j->bits==8)amy_s31_mat8_8_cached(p,(const int8_t*)weight,r,count);else amy_s31_mat8_16_cached(p,(const int16_t*)weight,r,count);
    ++simd[lane];
#else
    int64_t *r=result;mat8_scalar(p,weight,r,count,j->bits);++calls[lane];
#endif
    PROFILE_STEP(3);
    for(int o=0;o<8;++o)total[t-first][o]+=r[o];
    PROFILE_STEP(4);
   }
  }
  for(int t=first;t<end;++t)for(int o=0;o<8 && oc+o<j->oc;++o){
   const float v=amy_quant_float(total[t-first][o])*(j->xs*j->scales[oc+o]);
   const long at=(long)(oc+o)*j->L+t;
   const float base=j->accumulate?j->out[at]+j->bias[oc+o]:j->bias[oc+o];j->out[at]=base+v;
  }
  PROFILE_STEP(4);
 }
#undef PROFILE_STEP
}
static void quant_job(void *ctx,int first,int limit,int lane){
 const QuantJob *j=ctx;int16_t *p=j->patch[lane];
 int pie=0;
#ifdef ESP_PLATFORM
 pie=j->n<=1344 && amy_parallel_pie_worker() && check_pie();
 if(pie)p=pie_work.patch;
#endif
 /* SIMD core claims up to 16 positions; scalar core claims one. The faster
  * worker cannot end up waiting for a 64-position scalar tail. */
 if(!j->stride
#ifdef ESP_PLATFORM
    && pie
#endif
 ){for(int begin=first;begin<limit;begin+=16)quant_tile(j,begin,begin+16<limit?begin+16:limit,lane);return;}
 for(int t=first;t<limit;++t){
  if(amy_parallel_poll(lane))return;
  memset(p,0,(size_t)j->n*2);int q=0;
  const int phase=j->stride?t%j->stride:0;
  for(int ic=0;ic<j->ic;++ic){
   if(j->stride){
    const int firstk=(phase+j->pad)%j->stride;
    for(int k=firstk;k<j->K;k+=j->stride){int pos=(t+j->pad-k)/j->stride;
     p[q++]=pos>=0 && pos<j->T?j->x[(long)pos*j->ic+ic]:0;}
   }else for(int k=0;k<j->K;++k){int pos=t+k*j->dil-j->dil*(j->K/2);
    p[q++]=pos>=0 && pos<j->T?j->x[(long)pos*j->ic+ic]:0;}
  }
  for(int oc=0;oc<j->oc;oc+=8){
   const void *w=j->w+((size_t)phase*((j->oc+7)&~7)+oc)*j->n*(j->bits/8);
   int64_t local[8],*sums=local;
#ifdef ESP_PLATFORM
   if(pie){sums=pie_result;if(j->bits==8)amy_s31_mat8_8_cached(p,w,sums,j->n);else amy_s31_mat8_16_cached(p,w,sums,j->n);++simd[lane];}
   else
#endif
   {mat8_scalar(p,w,sums,j->n,j->bits);++calls[lane];}
   for(int o=0;o<8 && oc+o<j->oc;++o){const float v=amy_quant_float(sums[o])*(j->xs*j->scales[oc+o]);
    const long at=(long)(oc+o)*j->L+t;
    const float base=j->accumulate?j->out[at]+j->bias[oc+o]:j->bias[oc+o];j->out[at]=base+v;
   }
  }
 }
}
int amy_quant_conv(float *out,const float *x,const float *w,const float *b,int ic,int oc,int T,int K,int dil,int accumulate,int stride,int pad){
 if(!mode || (mode==AMY_MIXED && (stride || ic!=oc || (K!=3 && K!=5 && K!=7))))return 0;
 const int bits=mode==AMY_MIXED?8:16;
 const int64_t begin=amy_now_us();
 const int L=stride?(T-1)*stride-2*pad+K:T;
 if(ic<=0 || oc<=0 || T<=0 || L<=0 || K<=0 || (stride && K%stride)){failed=1;amy_compute_poll();return 1;}
 const int n=(ic*(stride?K/stride:K)+15)&~15;
 const int ocpad=(oc+7)&~7;
 const size_t weights=(size_t)(stride?stride:1)*ocpad*n*(bits/8);
 const size_t inputs=((size_t)ic*T*2+15)&~(size_t)15;
 const size_t scales=((size_t)oc*4+15)&~(size_t)15;
 const int prepacked=mode==AMY_PACKED16;
 const unsigned char *desc=prepacked?amy_packed_find(w,ic,oc,K,stride,pad,dil):NULL;
 if(prepacked && !desc){failed=1;amy_compute_poll();return 1;}
 const size_t need=(prepacked?0:weights+scales)+inputs+(size_t)n*4*16;
 if(!memory || need>capacity){failed=1;printf("AMY_QUANT_OOM need=%u cap=%u\n",(unsigned)need,(unsigned)capacity);amy_compute_poll();return 1;}
 ++layers;unsigned char *qw=prepacked?(unsigned char *)(packed_model+aq_u32(desc+32)):memory;
 int16_t *qx=(int16_t *)(memory+(prepacked?0:weights));
 float *ws=prepacked?(float *)(packed_model+aq_u32(desc+36)):(float *)((unsigned char *)qx+inputs);
 int16_t *patches=(int16_t *)((unsigned char *)qx+inputs+(prepacked?0:scales));
 float maximum=0;
 for(long i=0;i<(long)ic*T;++i){if((i&8191)==0)amy_compute_poll();if(!isfinite(x[i])){failed=1;amy_compute_poll();return 1;}float a=fabsf(x[i]);if(a>maximum)maximum=a;}
 float xs=scale_for(maximum,16),inv=1.f/xs;
 /* NWC integer input removes repeated long-stride PSRAM gathers in every
  * output dot. Conversion still uses exactly the same per-tensor scale. */
 /* Cache-block the channel-major -> time-major conversion. A full strided
  * sweep conflicts with the cache sets of the 7168-position final stage. */
 for(int start=0;start<T;start+=32){amy_compute_poll();int end=start+32<T?start+32:T;
  for(int c=0;c<ic;++c)for(int t=start;t<end;++t)qx[(long)t*ic+c]=(int16_t)amy_quant_round(x[(long)c*T+t]*inv);
 }
 if(!prepacked){memset(qw,0,weights);
 for(int o=0;o<oc;++o){
  amy_compute_poll();maximum=0;
  for(int i=0;i<ic;++i)for(int k=0;k<K;++k){const float v=w[stride?((long)i*oc+o)*K+k:((long)o*ic+i)*K+k];
   if(!isfinite(v)){failed=1;amy_compute_poll();return 1;}float a=fabsf(v);if(a>maximum)maximum=a;}
  ws[o]=scale_for(maximum,bits);inv=1.f/ws[o];
  for(int ph=0;ph<(stride?stride:1);++ph){long q=((long)ph*ocpad+(o/8)*8)*n+(o%8);
   for(int i=0;i<ic;++i)for(int k=stride?(ph+pad)%stride:0;k<K;k+=stride?stride:1){
    const float v=w[stride?((long)i*oc+o)*K+k:((long)o*ic+i)*K+k];const int value=amy_quant_round(v*inv);
    if(bits==8)((int8_t *)qw)[q]=(int8_t)value;else ((int16_t *)qw)[q]=(int16_t)value;q+=8;
   }
  }
 }
 }
 hp_job=(QuantJob){out,b,qx,qw,ws,ic,oc,T,L,K,dil,accumulate,stride,pad,n,bits,xs,{patches,patches+n*16}};
 const int64_t packed=amy_now_us();packing_us+=packed-begin;
#ifdef ESP_PLATFORM
 memset(profile_us,0,sizeof(profile_us));
#endif
 amy_parallel_for_quant(quant_job,&hp_job,L);const int64_t done=amy_now_us();kernel_us+=done-packed;
#ifdef ESP_PLATFORM
 if(layers<=23)printf("AMY_INT_LAYER i=%u ic=%d oc=%d t=%d k=%d tr=%d pack_us=%lld kernel_us=%lld\n",layers,ic,oc,T,K,stride,(long long)(packed-begin),(long long)(done-packed));
 if(layers<=23 && !stride)printf("AMY_TILE_PROFILE gather=%lld weights=%lld input=%lld mac=%lld post=%lld\n",(long long)profile_us[0],(long long)profile_us[1],(long long)profile_us[2],(long long)profile_us[3],(long long)profile_us[4]);
#endif
 return 1;
}
