// Resample the existing EFNT face; no font partition or legacy metrics change.
// Use the resident 30px face consistently for body text, and true-size 90px
// Latin glyphs for large words; do not mix two Chinese weights in one line.
void Font::RasterDelete::operator()(RasterArena *arena) const {
 if(!arena)return;
 arena->~RasterArena();
#ifdef ESP_PLATFORM
 heap_caps_free(arena);
#else
 ::operator delete(arena);
#endif
}
const Font::RasterGlyph *Font::raster(uint32_t cp,int px)const{
 // Unsupported sizes and allocation failures retain the original renderer.
 if(px<1||px>128)return nullptr;
 const uint64_t key=(uint64_t(cp)<<32)|uint32_t(px);
 if(raster_)for(size_t i=0;i<raster_->count;++i)if(raster_->entries[i].key==key)return &raster_->entries[i];
 const auto g=glyph(cp,uint16_t(px>60?90:30));
 if(!g.bitmap)return nullptr;
 const int sz=std::max(1,int(g.font_size));
 const int ww=(g.width*px+sz-1)/sz,hh=(g.height*px+sz-1)/sz;
 const size_t size=size_t(ww)*size_t(hh);
 if(size>RasterArena::capacity)return nullptr;
 if(!raster_){
#ifdef ESP_PLATFORM
  void *mem=heap_caps_malloc(sizeof(RasterArena),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  void *mem=::operator new(sizeof(RasterArena),std::nothrow);
#endif
  if(!mem)return nullptr;
  raster_.reset(new(mem)RasterArena);
 }
 auto &arena=*raster_;
 if(arena.count==256||arena.used+size>RasterArena::capacity){arena.count=0;arena.used=0;}
 auto *coverage=arena.pixels+arena.used;
 auto alpha=[&](int gx,int gy){if(gx<0||gy<0||gx>=g.width||gy>=g.height)return 0;const int bit=(gy*g.width+gx)*2;return int((g.bitmap[bit/8]>>(6-bit%8))&3);};
 for(int dx=0;dx<ww;++dx){
  const int sx=dx*sz*256/px,gx=sx/256,fx=sx%256;
  for(int dy=0;dy<hh;++dy){
   const int sy=dy*sz*256/px,gy=sy/256,fy=sy%256;
   const int a=(alpha(gx,gy)*(256-fx)*(256-fy)+alpha(gx+1,gy)*fx*(256-fy)+alpha(gx,gy+1)*(256-fx)*fy+alpha(gx+1,gy+1)*fx*fy)*5;
   coverage[size_t(dx)*hh+dy]=uint8_t((a+32768)/65536);
  }
 }
 auto &entry=arena.entries[arena.count++];
 entry={key,ww,hh,g.x*px/sz,(32*px+15)/30+(-g.y-g.height)*px/sz,(g.advance*px+sz/2)/sz,coverage};
 arena.used+=size;
 return &entry;
}
int Canvas::text_width_px(const std::string&s,int px)const{
 int width=0;for(size_t i=0;i<s.size();){auto g=font_.glyph(next(s,i),uint16_t(px>60?90:30));width+=(g.advance*px+g.font_size/2)/std::max(1,int(g.font_size));}return width;
}
void Canvas::text_px(int x,int y,const std::string&s,int px){
 for(size_t i=0;i<s.size()&&!cancelled();){const auto cp=next(s,i);
  if(const auto *r=font_.raster(cp,px)){
   const int left=x+r->left,top=y+r->top;
   const int x0=std::max(0,-left),x1=std::min(r->width,width-left);
   const int y0=std::max(0,-top),y1=std::min(r->height,height-top);
   for(int dx=x0;dx<x1&&!cancelled();++dx){
    auto *row=fb_+(width-1-left-dx)*(height/2);
    const auto *cover=r->coverage+size_t(dx)*r->height;
    for(int dy=y0;dy<y1;++dy)if(cover[dy]){
     const int yy=top+dy,shift=(yy&1)*4;
     auto &b=row[yy/2];const int old=(b>>shift)&15;
     const int value=(old*(15-cover[dy])+7)/15;
     b=uint8_t((b&~(15<<shift))|(value<<shift));
    }
   }
   x+=r->advance;continue;
  }
  const auto g=font_.glyph(cp,uint16_t(px>60?90:30));const int sz=std::max(1,int(g.font_size));
  auto alpha=[&](int gx,int gy){if(gx<0||gy<0||gx>=g.width||gy>=g.height||!g.bitmap)return 0;const int bit=(gy*g.width+gx)*2;return int((g.bitmap[bit/8]>>(6-bit%8))&3);};
  if(!g.bitmap&&cp!=' ')border({x,y+px/6,px/2,px*3/4});
  else{const int ww=(g.width*px+sz-1)/sz,hh=(g.height*px+sz-1)/sz;
   const int left=x+g.x*px/sz,top=y+(32*px+15)/30+(-g.y-g.height)*px/sz;
   // Bilinear coverage on the original 2-bit glyph, entirely integer math.
   for(int dy=0;dy<hh;++dy)for(int dx=0;dx<ww;++dx){const int sx=dx*sz*256/px,sy=dy*sz*256/px,gx=sx/256,gy=sy/256,fx=sx%256,fy=sy%256;
    const int a=(alpha(gx,gy)*(256-fx)*(256-fy)+alpha(gx+1,gy)*fx*(256-fy)+alpha(gx,gy+1)*(256-fx)*fy+alpha(gx+1,gy+1)*fx*fy)*5;
    const int cover=(a+32768)/65536;if(cover)gray(left+dx,top+dy,uint8_t((shade(left+dx,top+dy)*(15-cover)+7)/15));
   }
  }x+=(g.advance*px+sz/2)/sz;
 }
}
std::vector<std::string> Canvas::wrap_px(const std::string&s,int max,int px)const{
 std::vector<std::string> lines;std::string line;int used=0;
 auto flush=[&]{while(!line.empty()&&line.back()==' ')line.pop_back();lines.push_back(line);line.clear();used=0;};
 for(size_t i=0;i<s.size();){const auto start=i;const auto cp=next(s,i);if(cp=='\n'){flush();continue;}if(cp=='\r')continue;
  if(cp==' '||cp=='\t'){const int w=text_width_px(" ",px);if(!line.empty()&&used+w<=max){line+=' ';used+=w;}else if(!line.empty())flush();continue;}
  if(cp>32&&cp<0x3000)while(i<s.size()){size_t end=i;const auto n=next(s,end);if(n<=32||n>=0x3000)break;i=end;}
  const auto token=s.substr(start,i-start);const int w=text_width_px(token,px);if(used+w>max&&!line.empty())flush();
  if(w<=max){line+=token;used+=w;continue;}
  for(size_t j=0;j<token.size();){const auto a=j;next(token,j);const auto part=token.substr(a,j-a);const int pw=text_width_px(part,px);if(used+pw>max&&!line.empty())flush();line+=part;used+=pw;}
 }if(!line.empty()||lines.empty())lines.push_back(line);return lines;
}
