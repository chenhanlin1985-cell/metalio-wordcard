#pragma once
#include "amy_filter_coefficients.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
inline size_t amy_resampled_size(size_t n){return n?(n-1)*320/441+1:0;}
bool amy_resample_parallel(const int16_t *,size_t,int16_t *,size_t,const std::function<bool()> &);
inline bool amy_resample(const int16_t *in,size_t n,int16_t *out,size_t cap,const std::function<bool()> &cancel){
 const size_t total=amy_resampled_size(n);if(!in||!out||!total||total>cap)return false;
 for(size_t i=0;i<total;++i){if(i%256==0 && cancel())return false;const size_t phase=(i*441)%320;const int64_t center=int64_t(i*441/320);int64_t acc=0;
  for(int j=0;j<129;++j){int64_t p=center+j-64;if(p>=0 && uint64_t(p)<n)acc+=int64_t(in[size_t(p)])*amy_filter_q30[phase][j];}
  const auto rounded=acc>=0?(acc+(1ll<<29))/(1ll<<30):-((-acc+(1ll<<29))/(1ll<<30));out[i]=int16_t(std::clamp<int64_t>(rounded,-32768,32767));
 }return !cancel();
}
