#include "text_waveform_policy.hpp"
#include "waveform_fixtures.hpp"
#include <array>
#include <cassert>
#include <iostream>
int main() {
    std::array<uint8_t,48*64+2> guarded{};guarded.front()=0x69;guarded.back()=0x96;
    auto *out=guarded.data()+1;
    const auto *gray=&my_gl_25_0_data[0][0][0];
    assert(!wordcard::make_text_waveform(gray,48,out,48*64-1));
    assert(wordcard::make_text_waveform(gray,48,out,48*64));
    unsigned checks=0;
    for(unsigned f=0;f<48;++f)for(unsigned to=0;to<16;++to)for(unsigned from=0;from<16;++from) {
        const auto got=wordcard::wave_action(out,f,to,from);
        if(to==from)assert(got==0);
        else assert(got==wordcard::wave_action(gray,f,to,from));
        ++checks;
    }
    assert(guarded.front()==0x69 && guarded.back()==0x96);
    std::cout << "PASS: " << checks << " waveform transitions; equal grays held; all changed-pixel calibration preserved; guards intact\n";
}
