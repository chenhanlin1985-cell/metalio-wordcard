#pragma once
#include <cstdint>
#include <cstddef>
namespace wordcard {
// Native framebuffer is 1216 x 684, packed low nibble first. Permit DU only
// when every changed pixel is a binary endpoint in the portrait input strip.
inline bool keyboard_fast_refresh_safe(const uint8_t *front,const uint8_t *back,size_t bytes,bool web_submit=false) {
    if(!front||!back||bytes!=1216u*684u/2)return false;
    bool changed=false;
    for(size_t i=0;i<bytes;++i)if(front[i]!=back[i]) {
        for(int n=0;n<2;++n) {
            const auto a=(front[i]>>(4*n))&15,b=(back[i]>>(4*n))&15;
            if(a==b)continue;
            const size_t p=i*2+n;const int x=683-int(p/1216),y=int(p%1216);
            const bool field=x>=12&&x<672&&y>=124&&y<354;
            const bool submit=web_submit&&x>=518&&x<682&&y>=1092&&y<1176;
            if((!field&&!submit)||(a!=0&&a!=15)||(b!=0&&b!=15))return false;
            changed=true;
        }
    }
    return changed;
}
}
