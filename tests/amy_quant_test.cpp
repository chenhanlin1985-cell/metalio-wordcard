#include "amy_quant.h"
#include "amy_parallel.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <iostream>
extern "C" void amy_compute_poll(){}
extern "C" int amy_profile_enabled(){return 0;}
extern "C" int64_t amy_now_us(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static unsigned checks=0,rng=23171;
static void require(bool v,const char *m){++checks;if(!v)throw std::runtime_error(m);}
static float randomf(){rng=1664525*rng+1013904223;return (int(rng%20001)-10000)*0.00013f;}
static float scale(float v,int bits){return v==0?1.f:std::exp2(std::ceil(std::log2(std::max(v/(bits==8?127.f:32767.f),1e-20f))));}
static int quant(float v,float s){return int(std::nearbyint(double(v/s)));}
int main(){try{
 uint64_t state=0x123456789abcdef0ull;
 for(int i=0;i<100000;++i){state=state*6364136223846793005ull+1442695040888963407ull;int64_t n=(int64_t)state;float a=amy_quant_float(n),b=(float)n;require(!std::memcmp(&a,&b,4),"exact signed64 IEEE conversion");}
 for(int bit=0;bit<63;++bit)for(int d=-2;d<=2;++d)for(int sign:{-1,1}){int64_t n=sign*((1LL<<bit)+d);float a=amy_quant_float(n),b=(float)n;require(!std::memcmp(&a,&b,4),"conversion powers and rounding boundaries");}
 for(int i=-32766;i<32766;++i){require(amy_quant_round(i+0.5f)==int(std::nearbyint(i+0.5)),"ties even");}
 std::vector<int16_t>x(3072),w(3072);std::vector<int8_t>w8(3072);
 for(int pattern=0;pattern<5;++pattern){for(int i=0;i<3072;++i){x[i]=pattern==0?-32768:pattern==1?32767:int16_t(i*7919);w[i]=pattern<2?x[i]:int16_t(i*3833);w8[i]=pattern<2?-128:int8_t(i*53);}
  for(int n:{16,240,256,272,672,1344,3072})for(int bits:{8,16}){int64_t oracle=0,blocked=0;
   for(int i=0;i<n;++i)oracle+=int64_t(x[i])*(bits==8?w8[i]:w[i]);
   for(int begin=0;begin<n;begin+=256){int64_t a=0;for(int i=begin;i<std::min(n,begin+256);++i)a+=int64_t(x[i])*(bits==8?w8[i]:w[i]);require(a>=-(1LL<<39) && a<(1LL<<39),"XACC 40 bit bound");
    // Signed full-width XACC store, then RV32 low/high carry accumulation.
    uint32_t lo=uint32_t(a);int32_t hi=int32_t(a>>32);int64_t extracted=int64_t(hi)*(1LL<<32)+lo;require(extracted==a,"exact XACC signed store");blocked+=extracted;}
   require(blocked==oracle && amy_quant_dot_scalar(x.data(),bits==8?(void*)w8.data():(void*)w.data(),n,bits)==oracle,"integer dot exact64 including >int32");
  }
 }
 std::vector<unsigned char> scratch(AMY_QUANT_BYTES+16);void *aligned=(void *)((uintptr_t(scratch.data())+15)&~uintptr_t(15));
 for(int mode:{AMY_INT16,AMY_MIXED})for(int cores:{1,2})for(int transpose:{0,1})for(int K:transpose?std::vector<int>{8,16}:std::vector<int>{3,5,7})for(int dil:{1,3,12})for(int T:{3,17,48}){
  if(mode==AMY_MIXED && transpose)continue;
  int ic=24,oc=transpose?16:24,stride=transpose?K/2:0,pad=transpose?K/4:0,L=transpose?(T-1)*stride-2*pad+K:T,bits=mode==AMY_INT16?16:8;
  std::vector<float>a(ic*T),b(oc),weight(ic*oc*K),out(oc*L),expected(oc*L),ws(oc);
  for(auto &v:a)v=randomf();for(auto&v:b)v=randomf();for(auto&v:weight)v=randomf();for(auto&v:out)v=randomf();expected=out;
  float mx=0;for(float v:a)mx=std::max(mx,std::abs(v));float xs=scale(mx,16);
  for(int o=0;o<oc;++o){mx=0;for(int i=0;i<ic;++i)for(int k=0;k<K;++k)mx=std::max(mx,std::abs(weight[transpose?(i*oc+o)*K+k:(o*ic+i)*K+k]));ws[o]=scale(mx,bits);
   for(int t=0;t<L;++t){int64_t sum=0;for(int i=0;i<ic;++i)for(int k=0;k<K;++k){int n=transpose?t+pad-k:t+k*dil-dil*(K/2);if(transpose){if(n%stride)continue;n/=stride;}if(n<0||n>=T)continue;
    sum+=int64_t(quant(a[i*T+n],xs))*quant(weight[transpose?(i*oc+o)*K+k:(o*ic+i)*K+k],ws[o]);}
    float base=transpose?b[o]:expected[o*L+t]+b[o];expected[o*L+t]=base+float(sum)*(xs*ws[o]);
   }
  }
  amy_parallel_set_mode(cores,0);amy_quant_begin(mode,aligned,AMY_QUANT_BYTES);
  require(amy_quant_conv(out.data(),a.data(),weight.data(),b.data(),ic,oc,T,K,dil,!transpose,stride,pad)==1,"integer route");
  require(!amy_quant_failed() && !std::memcmp(out.data(),expected.data(),out.size()*4),"conv/tconv independent int64 reference, dual exact");amy_quant_end();
 }
 float o=0,a=1,wf=1,b=0;amy_quant_begin(AMY_INT16,aligned,1);require(amy_quant_conv(&o,&a,&wf,&b,1,1,1,1,1,0,0,0) && amy_quant_failed(),"short arena fails closed");amy_quant_end();require(!amy_quant_failed(),"failure state cleared");
 std::cout<<"PASS: "<<checks<<" integer checks (rounding, 40/64-bit overflow, independent conv/transpose, dual, short workspace). Device SIMD execution checked separately.\n";
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
