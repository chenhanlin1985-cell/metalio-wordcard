#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
uint32_t epd_interlace_4bpp_line_VE(const uint8_t*,const uint8_t*,uint8_t*,uint8_t*,int);
void epd_populate_line_mask(uint8_t*,const uint8_t*,int);
void epd_apply_line_mask_VE(uint8_t*,const uint8_t*,int);
void calc_epd_input_1ppB_1k_S3_VE_aligned(const uint32_t*,uint8_t*,const uint8_t*,uint32_t);
int main() {
    unsigned failures=0,checks=0;
    // Exhaust all two-byte dirty-column inputs, including non-binary XORs.
    for(unsigned bits=0;bits<65536;++bits) {
        uint8_t dirty[2]={uint8_t(bits),uint8_t(bits>>8)},mask=0;
        epd_populate_line_mask(&mask,dirty,1);
        for(unsigned pixel=0;pixel<4;++pixel) {
            const unsigned expected=((bits>>(pixel*4))&15)?3:0;
            const unsigned actual=(mask>>(6-2*pixel))&3;
            ++checks;
            if(actual!=expected) {
                if(failures++==0)std::cout<<"FIRST_FAILURE dirty="<<bits<<" pixel="<<pixel
                    <<" mask="<<unsigned(mask)<<" expected_pair="<<expected<<" actual_pair="<<actual<<"\n";
            }
        }
    }
    // Run the real S31 diff -> LUT pack -> mask functions, including the
    // four-byte mask loop and the 1216-pixel production scan-line width.
    alignas(16) std::array<uint32_t,256> lut{};
    for(unsigned to=0;to<16;++to)for(unsigned from=0;from<16;++from)
        lut[to*16+from]=(to==from)?0:(to>from?2:1);
    for(unsigned width:{4u,16u,1216u})for(unsigned position:{0u,1u,2u,3u})
      for(unsigned to=0;to<16;++to)for(unsigned from=0;from<16;++from) {
        const unsigned pixel=width-4+position;
        std::vector<uint8_t> old(width/2,255),next=old,dirty(width/2),mask(width/4),line(width/4);
        std::vector<uint32_t> diff(width/4);
        const unsigned shift=(pixel%2)*4;
        old[pixel/2]=uint8_t((old[pixel/2]&~(15u<<shift))|(from<<shift));
        next[pixel/2]=uint8_t((next[pixel/2]&~(15u<<shift))|(to<<shift));
        epd_interlace_4bpp_line_VE(next.data(),old.data(),reinterpret_cast<uint8_t*>(diff.data()),dirty.data(),width);
        epd_populate_line_mask(mask.data(),dirty.data(),width/4);
        calc_epd_input_1ppB_1k_S3_VE_aligned(diff.data(),line.data(),reinterpret_cast<uint8_t*>(lut.data()),width);
        epd_apply_line_mask_VE(line.data(),mask.data(),width/4);
        for(unsigned x=0;x<width;++x) {
            const unsigned got=(line[x/4]>>(6-2*(x%4)))&3;
            const unsigned want=x==pixel?lut[to*16+from]:0;
            ++checks;
            if(got!=want)++failures;
        }
      }
    std::cout<<(failures?"FAIL":"PASS")<<": "<<checks<<" S31 pixel pipeline checks; failures="<<failures<<"\n";
    return failures?1:0;
}
