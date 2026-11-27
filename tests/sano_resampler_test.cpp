#include "sano_resampler.hpp"
#include "speech_policy.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace wordcard;
static int checks=0;
static void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
static double rms(const std::vector<int16_t> &v) {
    double s=0;for(size_t i=128;i+128<v.size();++i)s+=double(v[i])*v[i];
    return std::sqrt(s/(v.size()-256));
}
int main() {
    std::vector<int16_t> input(24000),output(16000);
    for(double frequency:{1000.,4000.,6000.,8100.,9000.,10000.,11000.}) {
        for(size_t i=0;i<input.size();++i)input[i]=int16_t(std::lround(12000*std::sin(6.283185307179586*frequency*i/24000)));
        check(sano_resample_24k(input.data(),input.size(),output.data(),output.size(),[]{return false;}),"FIR succeeds");
        std::vector<int16_t> old;SpeechResampler linear(24000);
        check(linear.push(input.data(),input.size(),[&](const int16_t *p,size_t n){old.insert(old.end(),p,p+n);return true;}),"old control succeeds");
        const double new_rms=rms(output),old_rms=rms(old);
        if(frequency<=6000)check(new_rms>8200 && new_rms<8600,"speech passband retained");
        else {
            check(new_rms<8,"above-Nyquist alias rejected by at least 60 dB");
            check(old_rms>2000,"negative control reproduces aliasing in old linear converter");
        }
        std::cout<<"tone="<<frequency<<" old_rms="<<old_rms<<" new_rms="<<new_rms<<'\n';
    }
    for(int value:{-32768,-20000,0,20000,32767}) {
        std::fill(input.begin(),input.end(),int16_t(value));
        check(sano_resample_24k(input.data(),input.size(),output.data(),output.size(),[]{return false;}),"DC resample");
        for(size_t i=128;i+128<output.size();++i)check(output[i]==value,"unity DC both phases and signed full scale");
    }
    for(size_t count:{size_t(1),size_t(2),size_t(3),size_t(4),size_t(63),size_t(64),size_t(65),size_t(60160)}) {
        std::vector<int16_t> short_in(count,0),short_out(sano_resampled_size(count)+1,1234);
        const size_t length=sano_resampled_size(count);
        check(sano_resample_24k(short_in.data(),count,short_out.data(),length,[]{return false;}),"short or odd utterance including tail");
        check(short_out[length]==1234,"bounded output");
        check(length==(count-1)*16000/24000+1,"duration matches old converter");
        check(!sano_resample_24k(short_in.data(),count,short_out.data(),length-1,[]{return false;}),"capacity rejected");
    }
    check(!sano_resample_24k(input.data(),input.size(),output.data(),output.size(),[]{return true;}),"pre-cancel");
    int polls=0;check(!sano_resample_24k(input.data(),input.size(),output.data(),output.size(),[&](){return ++polls==3;}),"mid-filter cancellation");
    check(!sano_resample_24k(nullptr,0,output.data(),output.size(),[]{return false;}),"empty rejected");
    // Centered impulse confirms phase alignment and compensation for FIR delay.
    std::fill(input.begin(),input.end(),0);input[12000]=12000;
    check(sano_resample_24k(input.data(),input.size(),output.data(),output.size(),[]{return false;}),"impulse");
    const auto peak=std::max_element(output.begin(),output.end());
    check(size_t(peak-output.begin())==8000,"no added latency or truncated word tail");
    std::cout<<"PASS: "<<checks<<" sano anti-alias checks; passband, stopband, old-converter negative control, bounds, cancellation and phase\n";
}
