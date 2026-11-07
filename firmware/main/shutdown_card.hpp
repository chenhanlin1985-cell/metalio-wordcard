#pragma once
#include "wordcard_ui.hpp"
#include "generated/shutdown_art.hpp"
#include <array>
#include <algorithm>
#include <cstddef>

namespace wordcard {
inline void draw_shutdown_card(Canvas &canvas) {
    // Stream original RLE rows from flash. No SD dependency, no full-size RAM
    // bitmap, and no persistent allocations during shutdown.
    constexpr int w=shutdown_art::width,h=shutdown_art::height;
    constexpr int dw=w*Canvas::height<=h*Canvas::width?w*Canvas::height/h:Canvas::width;
    constexpr int dh=w*Canvas::height<=h*Canvas::width?Canvas::height:h*Canvas::width/w;
    constexpr int ox=(Canvas::width-dw)/2,oy=(Canvas::height-dh)/2;
    // Area sampling retains thin ink lines instead of dropping them during
    // nearest-neighbor reduction. At this fit at most three source rows overlap.
    static_assert((h+dh-1)/dh+1<=3,"Increase the streaming row ring for a different asset");
    std::array<std::array<uint8_t,w>,3> rows{};
    size_t index=0;unsigned remaining=0;uint8_t shade=15;
    auto next=[&](){
        if(!remaining){const auto run=shutdown_art::runs[index++];remaining=(run>>4)+1;shade=run&15;}
        --remaining;return shade;
    };
    int decoded=-1;
    for(int y=0;y<dh;++y){
        const int top=y*h,bottom=(y+1)*h,first_y=top/dh,end_y=(bottom+dh-1)/dh;
        while(decoded<end_y-1){++decoded;for(auto &pixel:rows[decoded%3])pixel=next();}
        for(int x=0;x<dw;++x){
            const int left=x*w,right=(x+1)*w,first_x=left/dw,end_x=(right+dw-1)/dw;
            uint32_t sum=0;
            for(int sy=first_y;sy<end_y;++sy){
                const int wy=std::min(bottom,(sy+1)*dh)-std::max(top,sy*dh);
                for(int sx=first_x;sx<end_x;++sx){
                    const int wx=std::min(right,(sx+1)*dw)-std::max(left,sx*dw);
                    sum+=uint32_t(rows[sy%3][sx])*wx*wy;
                }
            }
            canvas.gray(ox+x,oy+y,uint8_t((sum+uint32_t(w*h)/2)/uint32_t(w*h)));
        }
    }
}
}
