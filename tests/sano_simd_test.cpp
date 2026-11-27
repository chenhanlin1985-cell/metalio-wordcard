#include "sano_simd.h"
#include "snt_port.h"
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" void sano_ref_matvec_s8(const int8_t *,const int8_t *,int32_t *,int,int);
extern "C" void sano_s31_matvec_s8(const int8_t *,const int8_t *,int32_t *,int,int);
static int checks;
static void check(bool ok,const char *why) {++checks;if(!ok)throw std::runtime_error(why);}
// Exact last byte borders a PAGE_NOACCESS page: any final speculative load
// in the assembly-source model fails even when its result would be unused.
struct Guard {
    void *base;unsigned char *p;
    explicit Guard(size_t size) {
        size_t pages=(size+4095)&~size_t(4095);
        base=VirtualAlloc(nullptr,pages+4096,MEM_RESERVE,PAGE_NOACCESS);
        if(!base || !VirtualAlloc(base,pages,MEM_COMMIT,PAGE_READWRITE))throw std::runtime_error("VirtualAlloc");
        p=static_cast<unsigned char *>(base)+pages-size;
    }
    ~Guard(){VirtualFree(base,0,MEM_RELEASE);}
};
static uint32_t rng=0x7312026;
static int8_t random_byte(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return int8_t(int(rng&255)-128);}
int main() {
    try {
        const int lengths[]={16,32,48,64,80,96,112,128,144,160,192,256,512,704,720,1024,2048,4096};
        size_t vectors=0;
        for(int len:lengths) for(int rows:{1,2,3,17}) for(int pattern=0;pattern<12;++pattern) {
            Guard aa(len),ww(size_t(rows)*len),oo(size_t(rows)*4);
            auto a=reinterpret_cast<int8_t *>(aa.p),w=reinterpret_cast<int8_t *>(ww.p);
            auto out=reinterpret_cast<int32_t *>(oo.p);std::vector<int32_t> ref(rows);
            for(int i=0;i<len;++i)a[i]=pattern<2?-128:pattern==2?127:pattern==3?0:random_byte();
            for(int i=0;i<rows*len;++i)w[i]=pattern==0?-128:pattern==1?127:pattern==2?-128:random_byte();
            sano_ref_matvec_s8(a,w,ref.data(),rows,len);
            sano_s31_matvec_s8(a,w,out,rows,len);
            for(int r=0;r<rows;++r)check(ref[r]==out[r],"assembly-source semantics vs scalar");
            sano_simd_test_config(1,0,0);sano_kernels_begin();
            check(sano_kernels_stats().selfcheck==1,"selfcheck pass");
            snt_matvec_s8(a,w,out,rows,len);
            check(sano_kernels_stats().simd_calls==1 && sano_kernels_stats().simd_macs==uint64_t(rows)*len,"SIMD selected and counted");
            for(int r=0;r<rows;++r)check(ref[r]==out[r],"dispatched result");
            ++vectors;
        }
        alignas(16) int8_t a[4112],w[3*4112];alignas(16) int32_t out[3];
        for(auto &v:a)v=random_byte();for(auto &v:w)v=random_byte();
        // Unaligned bases, non-vector strides/tails, tiny/oversize/zero sizes.
        for(int len:{0,1,7,15,16,17,31,33,703,704,705,4097})for(int off=0;off<16;++off) {
            std::vector<int32_t> ref(3);sano_ref_matvec_s8(a+off,w+off,ref.data(),3,len);
            sano_simd_test_config(1,0,0);sano_kernels_begin();snt_matvec_s8(a+off,w+off,out,3,len);
            for(int r=0;r<3;++r)check(out[r]==ref[r],"alignment/tail fallback exact");
            check((sano_kernels_stats().simd_calls==1)==(off==0 && len>=16 && len<=4096 && len%16==0),"dispatch guard");
        }
        for(int core:{0,1})for(int disabled:{0,1})for(int corrupt:{0,1,2}) {
            sano_simd_test_config(core,disabled,corrupt);sano_kernels_begin();
            std::vector<int32_t> ref(3);sano_ref_matvec_s8(a,w,ref.data(),3,704);snt_matvec_s8(a,w,out,3,704);
            for(int r=0;r<3;++r)check(ref[r]==out[r],"core/disable/fault recovery");
            check((sano_kernels_stats().simd_calls==1)==(core==1 && !disabled && !corrupt),"no PIE on wrong core or failed selfcheck");
        }
        sano_simd_test_config(1,0,0);sano_kernels_begin();out[0]=123;snt_matvec_s8(a,w,out,0,704);
        check(out[0]==123 && !sano_kernels_stats().simd_calls,"zero rows no writes");
        int16_t a16[16];std::fill(std::begin(a16),std::end(a16),int16_t(-32768));
        snt_matvec_s16s8(a16,w,out,1,16);check(out[0]==snt_dot_s16s8(a16,w,16),"s16 unchanged");
        std::cout<<"PASS: "<<checks<<" SIMD semantic/dispatch checks; "<<vectors<<" guarded matrices; S31 machine instructions NOT executed\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
