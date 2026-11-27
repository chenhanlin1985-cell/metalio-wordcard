#pragma once
#include "wordcard_ui.hpp"
#include "generated/home_folio_titles.hpp"

namespace wordcard {
// Bitmaps share a vertical center across each line, including mixed Latin/CJK titles.
inline int folio_lettering(Canvas &c, int x, int centerY, const std::string &text, int size) {
    for (const auto &art : folio_type::lettering) {
        if (art.size != size || text != art.text) continue;
        const int stride = (art.width + 7) / 8;
        const int top = centerY - art.height / 2;
        uint8_t row[80]{};
        const auto *source = folio_type::bytes + art.offset;
        unsigned flags = 0, flag = 0x100;
        for (int y = 0; y < art.height && !c.cancelled(); ++y) {
            for (int col = 0; col < stride; ++col) {
                if (flag == 0x100) {
                    flags = *source++;
                    flag = 1;
                }
                if (flags & flag) row[col] ^= *source++;
                flag <<= 1;
                if (row[col]) {
                    for (int bit = 0; bit < 8; ++bit)
                        if (row[col] & (128 >> bit)) c.pixel(x + col * 8 + bit, top + y);
                }
            }
        }
        return art.width;
    }
    return 0;
}

inline int folio_line(Canvas &c,int x,int cy,const std::string &text,int slot,
                      Canvas::Align align=Canvas::Align::Left,bool paint=true) {
    const int width=c.folio_run(x,cy,text,slot,align,paint);
    if(width||text.empty())return width;
    const int px=(slot==136 || slot==236)?36:slot;
    const int fallback=c.text_width_px(text,px);
    if(paint)c.text_px(x-(align==Canvas::Align::Center?fallback/2:align==Canvas::Align::Right?fallback:0),
                       cy-(px+10)/2,text,px);
    return fallback;
}
inline void folio_title(Canvas &c, const std::string &name) {
    int x=42;
    auto title=name;
    if(name.rfind("GRE ",0)==0) {
        const int width=c.folio_run(x,221,"GRE",60,Canvas::Align::Left,true);
        x+=(width?width:folio_lettering(c,x,221,"GRE",60))+19;
        title=name.substr(4);
    }
    const int width=c.folio_run(x,221,title,76,Canvas::Align::Left,false);
    if(width>0 && x+width<=642){c.folio_run(x,221,title,76,Canvas::Align::Left,true);return;}
    if(folio_lettering(c,x,221,title,76))return;
    // A new or user-supplied catalog title stays readable without a new font asset.
    const int room=642-x;
    const int size = c.text_width_px(title,64)<=room ? 64 : 44;
    const auto lines=c.wrap_px(title,room,size);
    for(size_t i=0;i<lines.size()&&i<2;++i){
        auto line=lines[i];
        if (i == 1 && lines.size() > 2) {
            const auto shortened = c.wrap_px(line, room - c.text_width_px("…", size), size);
            line = (shortened.empty() ? std::string() : shortened.front()) + "…";
        }
        c.text_px(x,157+int(i)*(size+7),line,size);
    }
}

inline void folio_arrow(Canvas &c,int x,int y,int size) {
    c.fill({x-2*size,y-1,2*size+1,3});
    for(int i=0;i<size;++i){
        c.fill({x-i,y-i,3,2});
        c.fill({x-i,y+i,3,2});
    }
}

inline void folio_stamp(Canvas &c,int cx,int cy,unsigned index,const std::string &number,bool earned,bool album) {
    if (earned && c.folio_reward(cx,cy,index,album?80:64)) return;
    if (!earned && !album) {
        c.fill({cx-10,cy,20,2});
        return;
    }
    // The original award number also identifies earned stamps on an older SD card.
    int px=earned?34:28, room=album?76:64;
    while(px>18 && folio_line(c,0,0,number,px,Canvas::Align::Left,false)>room)px-=2;
    folio_line(c,cx,cy,number,px,Canvas::Align::Center);
    if (earned) c.fill({cx-10,cy+24,20,2});
}
} // namespace wordcard
