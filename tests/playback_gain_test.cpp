#include "playback_gain.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace wordcard;
int main() {
    int checks=0;auto check=[&](bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);};
    std::vector<int16_t> quiet(16000),strong(16000),silence(16000);
    for(size_t i=0;i<quiet.size();++i){quiet[i]=int16_t(2400*std::sin(i*0.04));strong[i]=int16_t(12000*std::sin(i*0.04));}
    const auto original=quiet;const auto gain=speech_playback_gain(quiet.data(),quiet.size());
    check(gain>65536 && gain<=196608,"quiet speech gains within limit");
    check(quiet==original,"analysis never changes benchmark PCM");
    for(size_t capacity:{size_t(0),size_t(512),size_t(15999),size_t(16000),size_t(17000)}) {
        std::vector<int16_t> storage(capacity),received;
        SpeechGainBuffer buffered(capacity?storage.data():nullptr,capacity);
        auto emit=[&](const int16_t *p,size_t n){
            check(buffered.gain()==(capacity>=quiet.size()?gain:65536),"gain is fixed before first emitted sample");
            received.insert(received.end(),p,p+n);return true;
        };
        for(size_t p=0;p<quiet.size();p+=777)check(buffered.push(quiet.data()+p,std::min<size_t>(777,quiet.size()-p),emit),"chunk push succeeds");
        check(buffered.finish(emit) && received==original,"short, exact-capacity, overflow and no-memory paths preserve all PCM bytes");
    }
    std::vector<int16_t> stop_storage(quiet.size());SpeechGainBuffer stopped(stop_storage.data(),stop_storage.size());
    auto reject=[](const int16_t*,size_t){return false;};
    check(stopped.push(quiet.data(),quiet.size(),reject) && !stopped.finish(reject),"final buffered sink failure propagates");
    SpeechGainBuffer overflow_stop(stop_storage.data(),1);
    check(overflow_stop.push(quiet.data(),1,reject) && !overflow_stop.push(quiet.data()+1,quiet.size()-1,reject),"overflow prefix sink failure stops before new block");
    SpeechLevel chunks;
    for(size_t p=0;p<quiet.size();p+=511)chunks.add(quiet.data()+p,std::min<size_t>(511,quiet.size()-p));
    check(chunks.gain()==gain,"streamed whole-file analysis matches buffered gain");
    check(speech_playback_gain(nullptr,16000)==65536,"missing buffer remains unity");
    // A representative quiet utterance should now approach 4800 RMS; the old
    // 3200 target unnecessarily left more headroom even without a large peak.
    check(gain>=uint32_t(2.82*65536) && gain<=uint32_t(2.84*65536),"quiet voice reaches new linear target");
    for(int16_t v:quiet)
        check(std::abs(int64_t(speech_i2s_sample(v,100,gain)))<=int64_t(29490)*65536,"boosted speech cannot clip");
    check(speech_playback_gain(strong.data(),strong.size())==65536,"loud voice not boosted");
    check(speech_playback_gain(silence.data(),silence.size())==65536,"silence does not get artificial gain");
    quiet[0]=30000;check(speech_playback_gain(quiet.data(),quiet.size())==65536,"peaky speech respects headroom");
    quiet[0]=-32768;check(speech_playback_gain(quiet.data(),quiet.size())==65536,"negative peak cannot overflow analysis");
    check(speech_i2s_sample(32767,0,196608)==0,"mute stays muted");
    check(speech_i2s_sample(-32768,100)==INT32_MIN,"negative full scale packs correctly");
    check(speech_i2s_sample(32767,100,196608)==INT32_MAX,"gain overflow saturates instead of wrapping");
    check(speech_i2s_sample(-32768,100,196608)==INT32_MIN,"negative overflow saturates");
    for(int x=-32000;x<=32000;x+=1000)
        check(speech_i2s_sample(int16_t(x),90)==int32_t(int64_t(x)*(int64_t(90)*90*65536/10000)),"existing volume behavior unchanged at unity gain");
    std::cout<<"PASS: "<<checks<<" speech playback gain checks; mute, headroom, saturation and source PCM preserved\n";
}
