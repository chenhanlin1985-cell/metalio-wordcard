#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include "amy_parallel.h"
extern "C" {
void amy_ref_conv(float*,const float*,const float*,const float*,int,int,int,int,int,int);
void amy_opt_conv(float*,const float*,const float*,const float*,int,int,int,int,int,int);
void amy_ref_transpose(float*,const float*,const float*,const float*,int,int,int,int,int,int);
void amy_opt_transpose(float*,const float*,const float*,const float*,int,int,int,int,int,int);
}
static uint32_t seed=0x6ad80fb3;
static float sample(){seed=seed*1664525u+1013904223u;return seed%13==0?0.f:(int(seed>>16)-32768)/32768.f;}
static std::vector<float> data(size_t n){std::vector<float> v(n);for(auto &x:v)x=sample();return v;}
static void equal(const std::vector<float>&a,const std::vector<float>&b){if(a.size()!=b.size()||std::memcmp(a.data(),b.data(),a.size()*4))throw std::runtime_error("kernel differs from unchanged upstream or overwrote guards");}
int main(){try{int cases=0;size_t values=0;

for(int t:{1,2,3,7,8,9,15,16,17,127,128,129,383,384,385,1024})for(int ic:{1,3,24,48})for(int k:{3,5,7})for(int d:{1,2,3,6,12})for(int acc:{0,1})for(int oc:{1,3,4,5,7,8,12}){
 auto x=data(ic*t),w=data(ic*oc*k),b=data(oc),ref=data(oc*t+16),opt=ref;
 amy_ref_conv(ref.data()+8,x.data(),w.data(),b.data(),ic,oc,t,k,d,acc);amy_opt_conv(opt.data()+8,x.data(),w.data(),b.data(),ic,oc,t,k,d,acc);equal(ref,opt);++cases;values+=oc*t;
}
for(int t:{1,2,3,7,8,9,15,16,17,127,128,129,384})for(int ic:{1,3,24,96})for(int s:{4,8})for(int oc:{1,3,4,5,7,8,12}){
 const int k=s*2,p=s/2,l=(t-1)*s-2*p+k;auto x=data(ic*t),w=data(ic*oc*k),b=data(oc),ref=data(oc*l+16),opt=ref;
 amy_ref_transpose(ref.data()+8,x.data(),w.data(),b.data(),ic,oc,t,k,s,p);amy_opt_transpose(opt.data()+8,x.data(),w.data(),b.data(),ic,oc,t,k,s,p);equal(ref,opt);++cases;values+=oc*l;
}
std::printf("PASS: %d kernel geometries, %zu fp32 outputs bit-exact to unchanged upstream, including padding/tails/zero weights/accumulation and write guards\n",cases,values);
}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
