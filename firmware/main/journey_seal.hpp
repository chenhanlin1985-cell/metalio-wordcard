#pragma once
#include "wordcard_ui.hpp"
#include <cstdlib>
namespace wordcard {
// High-contrast outline geometry; no dark fill, animation, SD read or allocation.
inline void journey_seal(Canvas &c,int cx,int cy,int radius,const std::string &number,bool earned,bool next=false) {
    const int width=earned?4:2;
    for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x){
        const int d=x*x+y*y;
        const bool dash=((std::abs(x)+std::abs(y))/5)%2==0;
        if(d<=radius*radius&&d>=(radius-width)*(radius-width)&&(earned||dash))c.pixel(cx+x,cy+y,true);
    }
    if(earned||next){const int r=radius+6;for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x){const int d=x*x+y*y;if(d<=r*r&&d>=(r-2)*(r-2))c.pixel(cx+x,cy+y,true);}}
    const int px=number.size()>3?28:radius>40?44:30;
    c.text_px(cx-c.text_width_px(number,px)/2,cy-px/2-5,number,px);
}
}
