#include "wordcard_ui.hpp"
#include "native_json.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
using namespace wordcard;
using J=StudyJson;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static WebPage decode(const J&p){
 WebPage o;o.title=str(p,"title");o.route=str(p,"route");o.key=str(p,"key");o.typed=str(p,"typed");o.prompt=str(p,"prompt");o.gap_mask=str(p,"gapMask");o.toast=str(p,"toast");o.keyboard=flag(p,"keyboard");o.shift=flag(p,"shift");o.symbols=flag(p,"symbols");o.can_submit=flag(p,"canSubmit");o.corrupt=flag(p,"corrupt");
 auto action=[&](const J&a){if(a.is_null())return -1;o.actions.push_back(a.dump());return int(o.actions.size()-1);};
 for(const auto&b:p["blocks"])o.blocks.push_back(WebBlock{str(b,"kind"),str(b,"text"),str(b,"area"),action(b["action"]),flag(b,"enabled"),flag(b,"selected"),flag(b,"primary"),str(b,"role"),str(b,"detail")});
 for(const auto*k:{"top","tabs"})for(const auto&b:p[k])(std::string(k)=="top"?o.top:o.tabs).push_back(WebControl{str(b,"text"),action(b["action"]),flag(b,"selected"),flag(b,"enabled")});return o;
}
int main(int argc,char**argv){try{
 check(argc==5,"record|verify fixture font oracle-file required");const bool record=std::string(argv[1])=="record";
 std::ifstream input(argv[2]),f(argv[3],std::ios::binary);check(bool(input)&&bool(f),"Inputs unavailable");
 std::vector<uint8_t>fontBytes((std::istreambuf_iterator<char>(f)),{}),storage(Canvas::bytes+64,0xA5),expected(Canvas::bytes);
 Font font;check(font.init(fontBytes.data(),fontBytes.size()),"font invalid");
 std::fstream oracle(argv[4],std::ios::binary|(record?(std::ios::out|std::ios::trunc):std::ios::in));check(bool(oracle),"oracle unavailable");
 Canvas canvas(storage.data()+32,font);size_t frames=0,scenes=0;uint64_t renderNs=0;
 auto verify=[&]{
  for(int i=0;i<32;++i)check(storage[i]==0xA5&&storage[Canvas::bytes+32+i]==0xA5,"Framebuffer guard corrupted");
  if(record)oracle.write(reinterpret_cast<char*>(storage.data()+32),Canvas::bytes);
  else{oracle.read(reinterpret_cast<char*>(expected.data()),expected.size());check(bool(oracle),"Short oracle");if(!std::equal(expected.begin(),expected.end(),storage.begin()+32))throw std::runtime_error("Raster mismatch frame "+std::to_string(frames));}
  ++frames;
 };
 std::mt19937 rng(73513);
 // Mixed starting gray levels catch incorrect antialiasing and nibble edges.
 for(int batch=0;batch<64;++batch){for(size_t i=0;i<Canvas::bytes;++i)storage[i+32]=uint8_t(rng());
  for(int i=0;i<64;++i){Rect r{int(rng()%900)-100,int(rng()%1500)-100,int(rng()%800)-80,int(rng()%1300)-80};canvas.fill(r,(rng()&1)!=0);}
  verify();
 }
 for(int px=1;px<=144;++px){
  for(size_t i=0;i<Canvas::bytes;++i)storage[i+32]=uint8_t(rng());
  for(int pass=0;pass<2;++pass){canvas.text_px(-5,5+pass*180,"Agj 学习 /ˈfɜːmwɛə/",px);canvas.text_px(620,1100,"Wi 中gj",px);}
  verify();
 }
 for(int scale=1;scale<=5;++scale){canvas.clear();canvas.text(-15,-20,"Words 学习 IPA /æŋ/",scale);canvas.text(20,300,"Words 学习",scale,false);canvas.fill({19,450,404,81});canvas.text(19,440,"WHITE 白字",scale,false);verify();}
 // More distinct sizes/codepoints than the bounded cache can retain.
 for(int i=0;i<1024;++i){canvas.text_px(int(rng()%700)-10,int(rng()%1220)-10,std::string(1,char(33+i%90)),12+i%100);if(i%32==31)verify();}
 auto detached=font.detached();Canvas other(storage.data()+32,detached);other.clear();other.text_px(20,40,"Independent worker cache",44);verify();
 font.fallback(nullptr);canvas.clear();canvas.text_px(20,40,"Font invalidation 缓存",44);verify();
 check(font.init(fontBytes.data(),fontBytes.size()),"reinit failed");canvas.clear();canvas.text_px(20,40,"Font invalidation 缓存",44);verify();
 // Change a previously cached glyph's source, then reinitialize the owner.
 auto changed=fontBytes;const auto glyphA=font.glyph('A');check(glyphA.bitmap!=nullptr,"A missing");
 const auto offset=size_t(glyphA.bitmap-fontBytes.data());
 std::fill(changed.begin()+offset,changed.begin()+offset+(size_t(glyphA.width)*glyphA.height*2+7)/8,0);
 check(font.init(changed.data(),changed.size()),"changed font invalid");canvas.clear();canvas.text_px(20,40,"AAAA",44);verify();
 check(font.init(fontBytes.data(),fontBytes.size()),"original font restore invalid");canvas.clear();canvas.text_px(20,40,"AAAA",44);verify();
 Font invalid;check(!invalid.init(nullptr,0),"invalid font accepted");Canvas missing(storage.data()+32,invalid);missing.clear();missing.text_px(2,2,"AB 缺字",40);verify();
 // Cancellation may leave an unpublished partial raster, never touch guards.
 Canvas cancelled(storage.data()+32,font,[]{return true;});auto saved=storage;cancelled.fill({0,0,684,1216});cancelled.text_px(0,0,"Must not draw",44);check(saved==storage,"Cancellation ignored");
 std::string line;while(std::getline(input,line)){const auto q=J::parse(line);View v;v.screen=Screen::WebLearning;v.web=decode(q["page"]);UI ui;int pages=1;
  for(int part=0;part<pages;++part){v.web_page=part;auto began=std::chrono::steady_clock::now();ui.render(canvas,v);renderNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count();pages=ui.answer_pages();verify();}++scenes;
 }
 if(!record)check(oracle.peek()==std::char_traits<char>::eof(),"Extra oracle frames");
 std::cout<<"PASS "<<(record?"record":"verify")<<" old/current raster: "<<frames<<" byte-exact frames, "<<scenes<<" UI scenes; UI host render_ms="<<renderNs/1000000.0<<" (not device timing)\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
