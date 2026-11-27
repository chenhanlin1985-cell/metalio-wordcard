#include "vocabulary.hpp"
#include "vocabulary_picture_index.hpp"
#include "vocabulary_picture_draw.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace wordcard;namespace fs=std::filesystem;
static int checks=0;static void ck(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
static std::string read(const fs::path&p){std::ifstream f(p,std::ios::binary);return{std::istreambuf_iterator<char>(f),{}};}
static void write(const fs::path&p,const std::string&b){std::ofstream(p,std::ios::binary).write(b.data(),b.size());}
static void fixture(const fs::path&p,const VJ&s){fs::create_directories(p/"vocabulary");auto data=VJ{{"state",s},{"journalOffset",0}}.dump();uint32_t h=2166136261;for(unsigned char b:data)h=(h^b)*16777619;write(p/"vocabulary-v2/state.json",VJ{{"data",data},{"checksum",h}}.dump()+"\n");}
int main(int argc,char**argv){try{
 ck(argc==4,"catalog font fresh-output");fs::path out=argv[3];ck(!fs::exists(out),"Fresh QA only");fs::create_directories(out/"vocabulary");fs::create_directories(out/"frames");
 for(auto n:{"catalog.json","catalog.ndjson","catalog.tsv","pictures-v2.bin"})fs::copy_file(fs::path(argv[1])/n,out/"vocabulary"/n);
 VocabularyCatalog cat;ck(cat.load((out/"vocabulary").generic_string()),"Catalog loads");VocabularyEngine eng(cat);int64_t now=1794895200000LL;VJ ev;
 auto state=eng.create(now);state=eng.reduce(state,VJ{{"type","chooseStage"},{"stage","gre-trial"}},++now,ev);state=eng.reduce(state,VJ{{"type","begin"},{"intent","new"}},++now,ev);
 const auto fontbytes=read(argv[2]);Font font;ck(font.init((const uint8_t*)fontbytes.data(),fontbytes.size()),"Font");std::vector<uint8_t> fb(Canvas::bytes);Canvas canvas(fb.data(),font);
 auto bytes=read(out/"vocabulary/pictures-v2.bin");WebBlock block;block.picture_source=(out/"vocabulary/pictures-v2.bin").generic_string();block.picture_width=320;block.picture_height=160;
 for(const auto&a:vocabulary_pictures){if(a.pack!=2)continue;ck(cat.entry(a.id)!=nullptr,"Mapped GRE exists");block.picture=a.tile;ck(draw_vocabulary_picture(canvas,block,0,0),"Wide CRC + actual decoder");}
 block.picture=0;block.picture_width=160;ck(!draw_vocabulary_picture(canvas,block,0,0),"Metadata dimensions mismatch rejected");block.picture_width=320;block.picture=48;ck(!draw_vocabulary_picture(canvas,block,0,0),"Index bounds");block.picture=0;
 for(int mutation=0;mutation<5;mutation++){auto b=bytes;if(mutation==0)b.back()^=0x80;if(mutation==1)b.resize(100);if(mutation==2){b[16]=char(0xff);b[17]=char(0xff);b[18]=char(0xff);b[19]=char(0xff);}if(mutation==3)b[24]=char(0xff);if(mutation==4)b[12]=1;write(out/"bad.bin",b);block.picture_source=(out/"bad.bin").generic_string();block.picture=mutation==0?47:0;auto before=fb;ck(!draw_vocabulary_picture(canvas,block,0,0),"Malformed art skipped");ck(fb==before,"Bad art paints no partial pixels");}
 UI ui;View view;view.screen=Screen::WebLearning;
 auto paint=[&](const WebPage&p,const std::string&name){view.web=p;view.web_page=0;ui.render(canvas,view);std::ofstream f(out/"frames"/(name+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";for(int y=0;y<1216;y++)for(int x=0;x<684;x++){char v=char(canvas.shade(x,y)*17);f.write(&v,1);}if(name=="detail-46"){std::vector<uint8_t> expected(Canvas::bytes);Font fresh=font.detached();Canvas ec(expected.data(),fresh);ec.clear();ec.text_px(28,194,"abstemious",64);for(int y=194;y<270;y++)for(int x=0;x<670;x++)ck(ec.shade(x,y)==canvas.shade(x,y),"Full uncut headword present");}const auto&bs=ui.buttons();for(size_t i=0;i<bs.size();i++){auto r=bs[i].rect;ck(r.x>=0&&r.y>=0&&r.x+r.w<=684&&r.y+r.h<=1216,"Control bounds");for(size_t j=0;j<i;j++){auto q=bs[j].rect;ck(r.x>=q.x+q.w||q.x>=r.x+r.w||r.y>=q.y+q.h||q.y>=r.y+r.h,"No control overlap");}}};
 const auto dir=out/"learning-web-scenes";fixture(dir,state);NativeVocabulary model(dir.generic_string());ck(model.enter(now),"Native enter");ck(model.dispatch(VJ{{"action","v-go"},{"value","word-batch"}}.dump()),"Recognition route");paint(model.page(),"recognition");
 for(const auto&a:vocabulary_pictures){if(a.pack!=2)continue;ck(model.dispatch(VJ{{"action","v-open"},{"value",a.id}}.dump()),"Detail opens");const auto page=model.page();ck(std::any_of(page.blocks.begin(),page.blocks.end(),[](const auto&b){return b.picture_width==320&&b.picture_height==160;}),"Wide dimensions reach Canvas");if(a.tile<3||a.tile==46){paint(page,"detail-"+std::to_string(a.tile));Font cold=font.detached();std::vector<uint8_t> other(Canvas::bytes);Canvas cc(other.data(),cold);UI uu;uu.render(cc,view);ck(other==fb,"Warm/cold Canvas matches");}}
 // All five recognition cards, then meaning/spelling tasks must hide illustrations.
 ck(model.dispatch(VJ{{"action","v-go"},{"value","word-batch"}}.dump()),"Resume recognition");
 for(int i=0;i<5;i++){ck(model.dispatch(VJ{{"action","v-do"},{"type","learnNext"}}.dump()),"Next recognition");}
 auto question=model.page();ck(std::none_of(question.blocks.begin(),question.blocks.end(),[](auto&b){return b.picture>=0;}),"Questions never expose art clues");paint(question,"question");
 std::ofstream(out/"checks.json")<<VJ{{"passed",checks},{"widePictures",48},{"device",false}}.dump(2);std::cout<<"Wide scenes checks passed "<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
