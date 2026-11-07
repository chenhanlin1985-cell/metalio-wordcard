#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
namespace wordcard {
// Playback-only, whole utterance gain. Leave source PCM and benchmark CRC intact.
// Bring quiet speech toward -16.7 dBFS RMS while keeping 10% peak headroom.
// One constant gain per utterance: no compressor, clipping, or per-block pumping.
struct SpeechLevel {
    uint64_t energy=0,count=0;
    unsigned peak=0;
    void add(const int16_t *pcm,size_t n) {
        if(!pcm)return;
        for(size_t i=0;i<n;++i) {
            const int32_t x=pcm[i];energy+=uint64_t(int64_t(x)*x);
            peak=std::max(peak,unsigned(x<0?-x:x));
        }
        count+=n;
    }
    uint32_t gain()const {
        if(!energy || !peak || !count)return 65536;
        const double rms=std::sqrt(double(energy)/count);
        return uint32_t(std::max(1.0,std::min({3.0,4800.0/rms,29490.0/peak}))*65536.0);
    }
};
inline uint32_t speech_playback_gain(const int16_t *pcm,size_t count) {
    SpeechLevel level;level.add(pcm,count);return level.gain();
}
// Caller-owned bounded storage; overflow flushes the complete prefix once and
// then streams at unity. The sink observes gain() before receiving any samples.
class SpeechGainBuffer {
    int16_t *storage_;
    size_t capacity_,count_=0;
    bool streaming_;
    uint32_t gain_=65536;
    template<class Sink> bool flush(const Sink &sink) {
        for(size_t p=0;p<count_;p+=512)
            if(!sink(storage_+p,std::min<size_t>(512,count_-p)))return false;
        count_=0;return true;
    }
public:
    SpeechGainBuffer(int16_t *storage,size_t capacity):storage_(storage),capacity_(capacity),streaming_(!storage || !capacity){}
    uint32_t gain()const{return gain_;}
    template<class Sink> bool push(const int16_t *pcm,size_t n,const Sink &sink) {
        if(!streaming_ && n<=capacity_-count_) {
            std::memcpy(storage_+count_,pcm,n*2);count_+=n;return true;
        }
        if(!streaming_) {streaming_=true;if(!flush(sink))return false;}
        return sink(pcm,n);
    }
    template<class Sink> bool finish(const Sink &sink) {
        if(streaming_)return true;
        gain_=speech_playback_gain(storage_,count_);return flush(sink);
    }
};
inline int32_t speech_i2s_sample(int16_t sample,int volume,uint32_t gain=65536) {
    volume=std::clamp(volume,0,100);
    const int64_t factor=int64_t(std::min<uint32_t>(gain,196608))*volume*volume/10000;
    return int32_t(std::clamp(int64_t(sample)*factor,int64_t(INT32_MIN),int64_t(INT32_MAX)));
}
}
