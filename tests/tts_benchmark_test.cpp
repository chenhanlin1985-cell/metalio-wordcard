#include "sano_tts.hpp"
#include "sano_simd.h"
#include <filesystem>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
using namespace wordcard;
static int checks;
static void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv) {
    try {
        check(argc==4,"arguments");std::string ef=argv[1],mf=argv[2];std::filesystem::path out=argv[3];
        HybridTts tts;SanoPcm pcm;TtsTestResult r;
        HybridTts missing_frontend;
        check(!missing_frontend.benchmark(TtsTestMode::Simd,0,ef+"-missing",mf,[]{return false;},pcm,r),"missing English frontend fails strictly");
        check(std::string(r.error).find("资源")!=std::string::npos && !pcm.data,"missing resource diagnosis preserved");
        uint32_t simd_crc[6]{},scalar_crc[6]{};
        for(int phrase=0;phrase<6;++phrase)for(int m=0;m<3;++m) {
            check(tts.benchmark(TtsTestMode(m),phrase,ef,mf,[]{return false;},pcm,r),r.error);
            check(r.generated && r.available && !r.played && !r.play_event,"generation never implies playback");
            check(r.samples==pcm.count && r.compute_us>0 && r.total_us>=r.compute_us+r.prepare_us,"timing and samples");
            check(!r.memory_valid,"host does not invent device heap readings");
            check(r.warm==(phrase!=0 || m!=0),"frontend cold/warm truth");
            if(m==0) {
                simd_crc[phrase]=r.pcm_crc;
#ifdef SANO_SIMD_HOST_MODEL
                check(r.actual==TtsActual::Simd && r.simd_calls>0 && !r.scalar_calls,"SIMD source model selected");
#else
                check(r.actual==TtsActual::Scalar && !r.simd_calls && r.scalar_calls>0,"unavailable SIMD labelled scalar");
#endif
            }
            if(m==1){scalar_crc[phrase]=r.pcm_crc;check(r.actual==TtsActual::Scalar && !r.simd_calls && r.scalar_calls>0,"forced scalar");}
            if(m==2)check(r.actual==TtsActual::Espeak && !r.simd_calls && !r.scalar_calls,"forced eSpeak never neural");
            std::ifstream f(out/(std::to_string(phrase+1)+(m==2?"-espeak.wav":"-sano.wav")),std::ios::binary);
            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});
            check(bytes.size()==44+pcm.count*2,"same reference PCM size");
            check(std::memcmp(bytes.data()+44,pcm.data.get(),pcm.count*2)==0,"benchmark PCM equals normal engine, no playback gain baked in");
            check(sano_kernels_allow_simd(1)==1,"diagnostic override restored");
        }
        for(int i=0;i<6;++i)check(simd_crc[i]==scalar_crc[i],"SIMD/scalar PCM CRC");
        check(!tts.benchmark(TtsTestMode::Simd,0,ef,mf+"-missing",[]{return false;},pcm,r),"strict neural missing model fails");
        check(!r.generated && r.actual!=TtsActual::Espeak && !pcm.data,"no benchmark engine fallback");
        check(!tts.benchmark(TtsTestMode::Scalar,0,ef,mf,[]{return true;},pcm,r) && r.cancelled,"pre-cancel");
        int polls=0;check(!tts.benchmark(TtsTestMode::Scalar,5,ef,mf,[&](){return ++polls>15;},pcm,r) && r.cancelled,"mid-generation cancel");
        check(!pcm.data && !pcm.count,"cancel releases partial audio");
        check(!tts.benchmark(TtsTestMode::Espeak,0,ef,mf,[]{return false;},pcm,r,1),"eSpeak deadline");
        check(!tts.benchmark(TtsTestMode::Simd,0,ef,mf,[]{return false;},pcm,r,1),"neural deadline");
        check(!tts.benchmark(TtsTestMode::Scalar,6,ef,mf,[]{return false;},pcm,r),"invalid phrase");
        check(sano_kernels_allow_simd(1)==1,"all failure paths restore production SIMD policy");
        check(tts.benchmark(TtsTestMode::Simd,0,ef,mf,[]{return false;},pcm,r),"recovery");
#ifdef SANO_SIMD_HOST_MODEL
        check(r.actual==TtsActual::Simd,"post-failure SIMD restored");
        sano_simd_test_config(1,0,1);
        check(tts.benchmark(TtsTestMode::Simd,0,ef,mf,[]{return false;},pcm,r),"SIMD check failure permits scalar");
        check(r.actual==TtsActual::Scalar && r.pie_check==-1,"degraded SIMD truthfully reported");
        sano_simd_test_config(1,0,0);
#endif
        TtsTestSession s;
        check(!s.select_phrase(-1) && !s.select_phrase(6) && !s.select_mode(tts_mode_count),"bounded selectors");
        check(s.start(true) && s.active() && s.phase==TtsTestPhase::Queued,"batch queued before drawing");
        check(!s.start(false) && !s.select_mode(2) && !s.select_phrase(4),"busy controls locked");
        for(int m=0;m<tts_mode_count;++m) {
            check(int(s.mode)==m && s.phase==TtsTestPhase::Queued,"ordered batch");s.phase=TtsTestPhase::Generating;
            r={};r.available=r.generated=true;r.run_id=s.run_id;r.requested=s.mode;r.phrase=s.phrase;
            check(s.accept(r) && s.phase==TtsTestPhase::Ready,"result must be rendered before playback");
            check(!s.accept(r),"duplicate generation rejected");s.phase=TtsTestPhase::Playing;
            r.play_event=r.played=true;check(s.accept(r),"play completion accepted");
        }
        check(!s.active() && s.phase==TtsTestPhase::Done,"batch completes");
        check(s.select_phrase(4) && s.start(false),"new phrase run");s.phase=TtsTestPhase::Generating;
        r={};r.run_id=s.run_id;r.phrase=s.phrase;r.requested=s.mode;r.generated=true;
        s.cancel();check(!s.accept(r) && !s.active(),"late worker completion after stop ignored");
        check(s.start(true),"restart after stop");s.phase=TtsTestPhase::Generating;
        r={};r.run_id=s.run_id;r.phrase=s.phrase;r.requested=s.mode;
        check(s.accept(r) && s.phase==TtsTestPhase::Failed && !s.batch,"failure stops batch");
        for(auto phase:{TtsTestPhase::Queued,TtsTestPhase::Generating,TtsTestPhase::Ready,TtsTestPhase::Playing}) {
            s.start(true);s.phase=phase;const auto old_id=s.run_id;
            r={};r.run_id=old_id;r.phrase=s.phrase;r.requested=s.mode;r.generated=true;r.play_event=phase==TtsTestPhase::Playing;r.played=true;
            check(!s.leave_page(true) && s.run_id==old_id,"same page retains active request");
            check(s.leave_page(false) && !s.active() && !s.batch && s.run_id!=old_id,"every navigation exit cancels pending batch");
            check(!s.accept(r) && s.phase==TtsTestPhase::Cancelled,"late completion after exit cannot resume on reentry");
        }
        std::cout<<"PASS: "<<checks<<" TTS benchmark checks; 18 real engine runs; scoped mode, exact PCM, strict failure, cancel, timing and batch state; host only\n";
    }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
