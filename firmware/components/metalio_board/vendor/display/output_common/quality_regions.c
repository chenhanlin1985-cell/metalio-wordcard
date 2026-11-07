#include "quality_regions.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define QUALITY_HOT IRAM_ATTR __attribute__((optimize("O3")))
#define QUALITY_INLINE static inline __attribute__((always_inline))
#else
#define QUALITY_HOT
#define QUALITY_INLINE static inline
#endif
static struct {
    WordcardQualityRect rects[WORDCARD_QUALITY_REGIONS_MAX];
    unsigned count,phases;
    int width,height;
    const uint8_t *luts;
    // Internal SRAM: expand once before the two feeders start a phase. Never
    // decode flash-resident waveform bits in the time-critical per-pixel loop.
    uint8_t actions[1024]; // Same four-byte stride as the optimized EPD LUT.
    int prepared_phase;
} quality;
static WordcardQualityRect clipped(WordcardQualityRect r,int width,int height) {
    const int64_t right=(int64_t)r.x+r.width,bottom=(int64_t)r.y+r.height;
    int x=r.x<0?0:r.x,y=r.y<0?0:r.y;
    int end=right<0?0:right>width?width:(int)right;
    int low=bottom<0?0:bottom>height?height:(int)bottom;
    if(r.width<=0||r.height<=0||end<=x||low<=y)return (WordcardQualityRect){0,0,0,0};
    return (WordcardQualityRect){x,y,end-x,low-y};
}
void wordcard_quality_clear(void){memset(&quality,0,sizeof(quality));quality.prepared_phase=-1;}
bool wordcard_quality_set(const WordcardQualityRect *rects,unsigned count,const uint8_t *luts,
                          unsigned phases,int width,int height) {
    wordcard_quality_clear();
    if(count>WORDCARD_QUALITY_REGIONS_MAX||!rects||!luts||phases!=48||width<=0||width%4||height<=0)return false;
    quality.phases=phases;quality.luts=luts;quality.width=width;quality.height=height;
    for(unsigned i=0;i<count;++i){
        WordcardQualityRect r=clipped(rects[i],width,height);
        bool duplicate=false;
        for(unsigned j=0;j<quality.count;++j)if(!memcmp(&r,&quality.rects[j],sizeof(r)))duplicate=true;
        if(r.width&&r.height&&!duplicate)quality.rects[quality.count++]=r;
    }
    // Row conversion can then merge overlaps in a single forward traversal.
    for(unsigned i=1;i<quality.count;++i){
        WordcardQualityRect r=quality.rects[i];unsigned j=i;
        while(j&&quality.rects[j-1].x>r.x){quality.rects[j]=quality.rects[j-1];--j;}
        quality.rects[j]=r;
    }
    return true;
}
bool wordcard_quality_changed(WordcardQualityRect r,const uint8_t *front,const uint8_t *back,int width,int height){
    if(!front||!back||width<=0||width%2||height<=0)return false;
    r=clipped(r,width,height);
    for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)
        if((front[y*(width/2)+x/2]^back[y*(width/2)+x/2])&(x%2?0xF0:0x0F))return true;
    return false;
}
void wordcard_quality_mark(bool *lines,uint8_t *columns,int width,int height){
    if(!quality.count||width!=quality.width||height!=quality.height)return;
    for(unsigned i=0;i<quality.count;++i){
        WordcardQualityRect r=quality.rects[i];
        for(int y=r.y;y<r.y+r.height;++y)lines[y]=true;
        for(int x=r.x;x<r.x+r.width;++x)columns[x/2]|=x%2?0xF0:0x0F;
    }
}
void wordcard_quality_prepare_phase(int phase){
    quality.prepared_phase=-1;
    if(!quality.count||phase<0||(unsigned)phase>=quality.phases)return;
    const uint8_t *lut=quality.luts+(size_t)phase*64;
    for(unsigned pair=0;pair<256;++pair){
        const unsigned from=pair&15,to=pair>>4;
        quality.actions[pair*4]=(lut[to*4+from/4]>>(6-2*(from%4)))&3;
    }
    quality.prepared_phase=phase;
}
void QUALITY_HOT wordcard_quality_patch(int row,int phase,const uint8_t *difference,uint8_t *drive){
    if(!quality.count||row<0||row>=quality.height||phase<0||phase!=quality.prepared_phase)return;
    const uint8_t *actions=quality.actions;
    for(unsigned i=0;i<quality.count;++i){
        WordcardQualityRect r=quality.rects[i];
        if(row<r.y||row>=r.y+r.height)continue;
        int x=r.x,end=r.x+r.width;
        // Only edge pixels need read/modify/write. Each interior output byte
        // is four direct SRAM lookups in native S31 REV order.
        for(;x<end&&(x&3);++x){
            const unsigned shift=6-2*(x&3);
            drive[x/4]=(uint8_t)((drive[x/4]&~(3u<<shift))|(actions[difference[x]*4]<<shift));
        }
        for(;x+4<=end;x+=4){
            drive[x/4]=(uint8_t)((actions[difference[x]*4]<<6)|(actions[difference[x+1]*4]<<4)|
                                (actions[difference[x+2]*4]<<2)|actions[difference[x+3]*4]);
        }
        for(;x<end;++x){
            const unsigned shift=6-2*(x&3);
            drive[x/4]=(uint8_t)((drive[x/4]&~(3u<<shift))|(actions[difference[x]*4]<<shift));
        }
    }
}

QUALITY_INLINE uint8_t lookup_four(const uint8_t *src,const uint8_t *lut){
    uint32_t p;memcpy(&p,src,sizeof(p));
    return (uint8_t)((lut[(p&255)*4]<<6)|(lut[((p>>8)&255)*4]<<4)|
                     (lut[((p>>16)&255)*4]<<2)|lut[(p>>24)*4]);
}
QUALITY_INLINE void convert_span(int x,int end,const uint8_t *src,uint8_t *dst,
                                 const uint8_t *lut,WordcardLineLookup lookup){
    for(;x<end&&(x&3);++x){
        const unsigned shift=6-2*(x&3);
        dst[x/4]=(uint8_t)((dst[x/4]&~(3u<<shift))|(lut[src[x]*4]<<shift));
    }
    for(;x+4<=end&&(x&15);x+=4)dst[x/4]=lookup_four(src+x,lut);
    const int wide=(end-x)&~15;
    if(wide>0){lookup((const uint32_t*)(src+x),dst+x/4,lut,(uint32_t)wide);x+=wide;}
    for(;x+4<=end;x+=4)dst[x/4]=lookup_four(src+x,lut);
    for(;x<end;++x){
        const unsigned shift=6-2*(x&3);
        dst[x/4]=(uint8_t)((dst[x/4]&~(3u<<shift))|(lut[src[x]*4]<<shift));
    }
}
bool QUALITY_HOT wordcard_quality_render(int row,int phase,const uint8_t *src,uint8_t *dst,
                                        const uint8_t *normal,WordcardLineLookup lookup){
    if(!quality.count||row<0||row>=quality.height||phase<0||phase!=quality.prepared_phase)return false;
    bool active=false;int x=0;
    for(unsigned i=0;i<quality.count;++i){
        const WordcardQualityRect r=quality.rects[i];
        if(row<r.y||row>=r.y+r.height)continue;
        active=true;
        const int end=r.x+r.width;
        if(end<=x)continue;
        if(r.x>x){convert_span(x,r.x,src,dst,normal,lookup);x=r.x;}
        convert_span(x,end,src,dst,quality.actions,lookup);x=end;
    }
    if(active&&x<quality.width)convert_span(x,quality.width,src,dst,normal,lookup);
    return active;
}
