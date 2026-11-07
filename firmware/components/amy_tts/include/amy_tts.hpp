#pragma once
#include "amy_frontend.hpp"
#include "resource_cache.hpp"
#include <cstdlib>
#include <memory>
/* Reserve is total free memory; it need not adjoin the decoder arena. */
inline int amy_chunk_frames(size_t required24,size_t largest,size_t free_bytes){
 constexpr size_t reserve=512u*1024u;
 return largest>=required24 && free_bytes>=required24 && free_bytes-required24>=reserve?24:16;
}
struct AmyPcm {std::unique_ptr<int16_t,decltype(&std::free)> data{nullptr,&std::free};size_t count=0,arena_peak=0;int64_t prepare_us=0,compute_us=0;unsigned simd_calls=0,scalar_calls=0;int pie_check=0;};
class AmyTts {
public:
 bool render(OfflineTts &,const std::string &espeak_folder,const std::string &model_folder,const std::string &text,const OfflineTts::Cancel &,AmyPcm &,std::string &error,int64_t budget_us=120000000);
 bool initialized()const{return bool(front_) && bool(decoder_);}
 void release(){front_.reset();decoder_.reset();}
private:
 std::unique_ptr<void,decltype(&std::free)> front_{nullptr,&std::free},decoder_{nullptr,&std::free};
 std::string folder_;wordcard::ResourceCache cache_;
};
