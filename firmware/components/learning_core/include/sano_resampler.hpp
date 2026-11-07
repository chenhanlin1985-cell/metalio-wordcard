#pragma once
#include "sano_filter_coefficients.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
namespace wordcard {
// Centered two-phase FIR: suppress >8 kHz BEFORE dropping 24 kHz samples.
// Integer coefficients keep SIMD/scalar and host/device resampling identical.
// Input is the bounded complete utterance; zero extension compensates delay
// without dropping the end of a word or adding silence to its duration.
inline size_t sano_resampled_size(size_t count) {return count ? (count-1)*2/3+1 : 0;}
inline bool sano_resample_24k(const int16_t *input,size_t count,int16_t *output,size_t capacity,
                              const std::function<bool()> &cancel) {
    const size_t length=sano_resampled_size(count);
    if(!input || !output || !length || length>capacity)return false;
    for(size_t k=0;k<length;++k) {
        if(k%256==0 && cancel())return false;
        const size_t phase=k%2;
        const int64_t newest=int64_t((3*k+64)/2);
        int64_t acc=0;
        for(int j=0;j<65;++j) {
            const int64_t pos=newest-j;
            if(pos>=0 && uint64_t(pos)<count)
                acc+=int64_t(input[size_t(pos)])*sano_filter_q30[phase][j];
        }
        const int64_t rounded=acc>=0 ? (acc+(1ll<<29))/(1ll<<30) : -((-acc+(1ll<<29))/(1ll<<30));
        output[k]=int16_t(std::clamp<int64_t>(rounded,-32768,32767));
    }
    return !cancel();
}
}
