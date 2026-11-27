#pragma once
#include "wordcard_ui.hpp"
#include <array>
#include <fstream>
#include <vector>
namespace wordcard {
// Optional SD artwork: bounded stack storage, no persistent allocations or shared
// file handles between foreground/background renderers. Bad/missing art is skipped.
inline bool draw_vocabulary_picture(Canvas &c,const WebBlock &b,int x,int y) {
    if(b.picture<0||b.picture>=1024||b.picture_source.empty())return false;
    std::ifstream file(b.picture_source,std::ios::binary);
    std::array<uint8_t,16> header{};file.read(reinterpret_cast<char*>(header.data()),header.size());
    const auto le=[&](int i){return unsigned(header[i])|(unsigned(header[i+1])<<8);};
    if(file.gcount()==16&&std::string(reinterpret_cast<char*>(header.data()),8)=="WPIX0002"){
        if(le(8)==0||le(8)>1024||unsigned(b.picture)>=le(8)||le(10)!=16||le(12)||le(14))return false;
        file.seekg(16+std::streamoff(b.picture)*16);std::array<uint8_t,16> e{};file.read(reinterpret_cast<char*>(e.data()),16);
        if(file.gcount()!=16)return false;
        const auto u16=[&](int i){return unsigned(e[i])|(unsigned(e[i+1])<<8);};
        const auto u32=[&](int i){return uint32_t(e[i])|(uint32_t(e[i+1])<<8)|(uint32_t(e[i+2])<<16)|(uint32_t(e[i+3])<<24);};
        const auto off=u32(0),size=u32(4),expected=u32(12);const unsigned w=u16(8),h=u16(10);
        const bool picturebook=b.role=="picturebook-art"&&w==576&&h==384;
        if(w<32||w%8||h<32||(!picturebook&&(w>384||h>192))||w!=unsigned(b.picture_width)||h!=unsigned(b.picture_height)||size!=w*h/8||off<16+le(8)*16)return false;
        file.seekg(0,std::ios::end);const auto length=file.tellg();if(length<0||uint64_t(off)+size>uint64_t(length))return false;
        // 27 KiB for a picture-book page, at most 9 KiB for a vocabulary image.
        std::vector<uint8_t> pixels(size);file.seekg(off);file.read(reinterpret_cast<char*>(pixels.data()),size);if(file.gcount()!=std::streamsize(size))return false;
        uint32_t hash=2166136261;for(auto v:pixels)hash=(hash^v)*16777619;if(hash!=expected)return false;
        for(unsigned yy=0;yy<h;++yy){if(c.cancelled())return false;for(unsigned xx=0;xx<w;++xx){const auto bit=yy*w+xx;if(pixels[bit/8]&(0x80>>(bit%8)))c.pixel(x+xx,y+yy,true);}}
        return true;
    }
    if(file.gcount()!=16||std::string(reinterpret_cast<char*>(header.data()),8)!="WPIX0001"||le(8)!=160||le(10)!=160||le(12)==0||le(12)>1024||unsigned(b.picture)>=le(12)||le(14)!=0)return false;
    file.seekg(16+std::streamoff(b.picture)*3204);std::array<uint8_t,3204> bytes{};file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(file.gcount()!=std::streamsize(bytes.size()))return false;
    uint32_t hash=2166136261,expected=uint32_t(bytes[0])|(uint32_t(bytes[1])<<8)|(uint32_t(bytes[2])<<16)|(uint32_t(bytes[3])<<24);
    for(size_t i=4;i<bytes.size();++i)hash=(hash^bytes[i])*16777619;
    if(hash!=expected)return false;
    for(int yy=0;yy<160;++yy){if(c.cancelled())return false;for(int xx=0;xx<160;++xx){const int bit=yy*160+xx;if(bytes[4+bit/8]&(0x80>>(bit%8)))c.pixel(x+xx,y+yy,true);}}
    return true;
}
}
