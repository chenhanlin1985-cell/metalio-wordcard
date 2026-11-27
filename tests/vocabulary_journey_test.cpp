#include "vocabulary.hpp"
#include "vocabulary_journey.hpp"
#include "vocabulary_picture_index.hpp"
#include "vocabulary_picture_draw.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace wordcard;
namespace fs=std::filesystem;
static int checks=0;
static void check(bool b,const std::string&s){++checks;if(!b)throw std::runtime_error(s);}
static std::string read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static VJ saved(const fs::path&p){const auto frame=VJ::parse(str(VJ::parse(read(p/"vocabulary-v2/state.json")),"data"));auto s=frame["state"];std::ifstream f(p/"vocabulary-v2/events.ndjson");f.seekg(num(frame,"journalOffset"));std::string line;while(std::getline(f,line))s=s.patch(VJ::parse(str(VJ::parse(line),"data"))["patch"]);return s;}
static void fixture(const fs::path&p,const VJ&s){fs::create_directories(p/"vocabulary");auto data=VJ{{"state",s},{"journalOffset",0}}.dump();uint32_t h=2166136261;for(unsigned char c:data)h=(h^c)*16777619;std::ofstream(p/"vocabulary-v2/state.json")<<VJ{{"data",data},{"checksum",h}}.dump()<<'\n';}
int main(int argc,char**argv){try{
 check(argc==5 || argc==6,"resources font fresh-output packed-images [folio font]");const fs::path out=argv[3];check(!fs::exists(out),"Fresh fixtures only");fs::create_directories(out/"vocabulary");fs::create_directories(out/"frames");
 for(const auto*n:{"catalog.json","catalog.tsv","catalog.ndjson"})fs::copy_file(fs::path(argv[1])/n,out/"vocabulary"/n);
 fs::copy_file(argv[4],out/"vocabulary/pictures-v1.bin");
 VocabularyCatalog catalog;check(catalog.load((out/"vocabulary").generic_string()),"Catalog");VocabularyEngine engine(catalog);int64_t now=1794823200000LL;
 auto step=[&](const VJ&s,VJ a){VJ ev;auto n=engine.reduce(s,a,++now,ev);return n;};
 auto fresh=engine.create(now);check(!fresh.contains("journey"),"Legacy profile not rewritten with invented awards");
 auto chosen=step(fresh,VJ{{"type","chooseStage"},{"stage","foundation"}});
 VJ completed,spelling;
 for(const auto*mode:{"meaning","full","staged"}){
  auto s=chosen;s["settings"]["mode"]=mode;s=step(s,VJ{{"type","begin"},{"intent","new"}});int count=0;bool captured=false;
  while(str(s["session"],"phase")!="summary"){
   check(++count<150,"Bounded full round");const auto ss=s["session"];
   if(str(ss,"phase")=="learn")s=step(s,VJ{{"type","learnNext"},{"sessionId",ss["id"]},{"learnAt",ss["learnAt"]}});
   else if(!field(ss,"feedback").is_null())s=step(s,VJ{{"type","continue"},{"taskId",ss["feedback"]["uid"]}});
   else{const auto t=engine.current(s),w=catalog.get(str(t,"id"));const auto kind=str(t,"kind");if(kind=="gap"&&!captured){spelling=step(s,VJ{{"type","draft"},{"taskId",t["uid"]},{"value","a"}});captured=true;}
    s=step(s,VJ{{"type","answer"},{"taskId",t["uid"]},{"value",kind=="meaning"?str(w,"meaning"):kind=="gap"?str(engine.gap(t),"answer"):str(w,"word")}});
   }
  }
  check(vocabulary_stamps(s).size()==1,"Exactly one stamp on completed round");check(vocabulary_stamps(s)[0]["answered"]==s["session"]["answered"],"Actual attempts in stamp");
  VJ ev=VJ::array();auto copy=s;vocabulary_award(copy,s,now,ev);check(copy==s&&ev.empty(),"Repeated completion is idempotent");
  auto corrupt=chosen;corrupt["journey"]=VJ{{"broken",true}};auto finishing=s;finishing["journey"]=corrupt["journey"];auto before=finishing;before["session"]["phase"]="tasks";auto original=finishing;ev=VJ::array();vocabulary_award(finishing,before,now,ev);check(finishing==original&&str(ev[0],"type")=="journey_unavailable","Bad optional album preserves core state and raw bytes");
  auto skipped=s;skipped.erase("journey");skipped["session"]["journeyAttempts"]=0;skipped["session"]["correct"]=0;before=skipped;before["session"]["phase"]="tasks";ev=VJ::array();vocabulary_award(skipped,before,now,ev);check(!skipped.contains("journey")&&ev.empty(),"All unknown earns no stamp");completed=s;
 }
 const auto fontBytes=read(argv[2]);Font font;check(font.init((const uint8_t*)fontBytes.data(),fontBytes.size()),"Font");const auto extra=argc==6?read(argv[5]):std::string();Font folio;if(argc==6){check(folio.init((const uint8_t*)extra.data(),extra.size()),"Folio font");font.folio(&folio);}std::vector<uint8_t> fb(Canvas::bytes);Canvas canvas(fb.data(),font);
 WebBlock image;image.picture_source=(out/"vocabulary/pictures-v1.bin").generic_string();
 const auto packed=read(image.picture_source);check(packed.size()>=16,"Image header present");
 const int pictureCount=uint8_t(packed[12])|(int(uint8_t(packed[13]))<<8);
 check(pictureCount>=144&&pictureCount<=1024,"Bounded extensible picture count");
 for(int i=0;i<pictureCount;++i){image.picture=i;check(draw_vocabulary_picture(canvas,image,0,0),"Picture CRC and actual decoder "+std::to_string(i));}
 image.picture=pictureCount;check(!draw_vocabulary_picture(canvas,image,0,0),"Out-of-range image rejected");
 auto legacy=packed.substr(0,16+144*3204);legacy[12]=char(144);legacy[13]=0;std::ofstream(out/"legacy.bin",std::ios::binary).write(legacy.data(),legacy.size());
 image.picture=143;image.picture_source=(out/"legacy.bin").generic_string();check(draw_vocabulary_picture(canvas,image,0,0),"Old 144-picture SD stays compatible");
 image.picture=144;check(!draw_vocabulary_picture(canvas,image,0,0),"Expanded index soft-falls back with old SD");
 image.picture_source=(out/"vocabulary/pictures-v1.bin").generic_string();
 auto bytes=read(image.picture_source);bytes[100]^=0x80;std::ofstream(out/"bad.bin",std::ios::binary).write(bytes.data(),bytes.size());image.picture=0;image.picture_source=(out/"bad.bin").generic_string();check(!draw_vocabulary_picture(canvas,image,0,0),"Bad CRC skipped");image.picture_source=(out/"missing.bin").generic_string();check(!draw_vocabulary_picture(canvas,image,0,0),"Missing art skipped");std::ofstream(out/"short.bin",std::ios::binary).write(bytes.data(),20);image.picture_source=(out/"short.bin").generic_string();check(!draw_vocabulary_picture(canvas,image,0,0),"Truncated art skipped");
 for(const auto &p:vocabulary_pictures)if(p.pack==1||catalog.entries.size()==4738)check(catalog.entry(p.id)!=nullptr,"Every installed-catalog picture is an existing word sense");
 UI ui;View view;view.screen=Screen::WebLearning;
 auto paint=[&](const WebPage&p,const std::string&name){view.web=p;view.web_page=0;ui.render(canvas,view);std::ofstream f(out/"frames"/(name+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x){char b=char(canvas.shade(x,y)*17);f.write(&b,1);}const auto&buttons=ui.buttons();for(size_t i=0;i<buttons.size();++i){const auto&r=buttons[i].rect;check(r.x>=0&&r.y>=0&&r.x+r.w<=684&&r.y+r.h<=1216,"Button bounds "+name);for(size_t j=0;j<i;++j){const auto&q=buttons[j].rect;check(r.x>=q.x+q.w||q.x>=r.x+r.w||r.y>=q.y+q.h||q.y>=r.y+r.h,"No button overlap "+name);}}};
 const auto dir=out/"learning-web-journey";fixture(dir,completed);NativeVocabulary model(dir.generic_string());check(model.enter(now),"Load completed fixture");paint(model.page(),"home");check(saved(dir)==completed,"Viewing home preserves complete old state");
 auto act=[&](VJ a){check(model.dispatch(a.dump()),"Dispatch "+str(a,"action"));check(str(VJ::parse(model.status()),"courseError").empty(),"No recovery error");};
 act(VJ{{"action","v-go"},{"value","word-batch"}});paint(model.page(),"summary");act(VJ{{"action","v-go"},{"value","collection"}});paint(model.page(),"collection");act(VJ{{"action","v-gallery"}});auto gallery=model.page();paint(gallery,"gallery");check(std::count_if(ui.buttons().begin(),ui.buttons().end(),[](const auto&b){return b.label=="回收集册"&&b.event.action==Action::Back&&b.rect.x==24&&b.rect.w==184;})==1,"One correctly classified album back control");check(std::count_if(gallery.blocks.begin(),gallery.blocks.end(),[](const auto&b){return b.picture>=0;})==3,"Three earned word illustrations on first gallery page");
 auto id=str(completed["session"]["ids"][0]);act(VJ{{"action","v-open"},{"value",id}});paint(model.page(),"word");check(model.back()&&model.page().route=="gallery","Detail returns to gallery");
 const auto draftDir=out/"learning-web-draft";spelling["journey"]=completed["journey"];fixture(draftDir,spelling);NativeVocabulary d(draftDir.generic_string());check(d.enter(now),"Load real spelling draft");check(d.dispatch(VJ{{"action","v-go"},{"value","word-batch"}}.dump()),"Open question");auto question=d.page();paint(question,"gap");check(std::none_of(question.blocks.begin(),question.blocks.end(),[](const auto&b){return b.picture>=0;}),"No picture answer cues in spelling");auto before=saved(draftDir);check(d.dispatch(VJ{{"action","v-go"},{"value","collection"}}.dump()),"Collection from challenge");check(d.dispatch(VJ{{"action","v-gallery"}}.dump()),"Exposure before gallery");check(d.dispatch(VJ{{"action","v-journey-resume"}}.dump()),"Resume challenge");auto after=saved(draftDir);check(after["session"]["current"]==before["session"]["current"]&&after["session"]["draft"]==before["session"]["draft"],"Gallery preserves position and typed input");check(flag(engine.current(after),"hint"),"Seeing current word marks assisted recall");
 bool returnedExposure=false;
 for(int retry=0;retry<5&&!returnedExposure;++retry){
  const auto nextTask=engine.current(after);check(d.dispatch(VJ{{"action","v-answer"},{"value",str(engine.gap(nextTask),"answer")}}.dump()),"Answer current gap");check(d.dispatch(VJ{{"action","v-do"},{"type","continue"}}.dump()),"Advance before returning to gallery");
  auto advanced=saved(draftDir);const auto newTask=engine.current(advanced);check(!flag(newTask,"lookupHint"),"Next question initially unassisted");check(d.back()&&d.page().route=="gallery","Back to gallery after next question");after=saved(draftDir);
  const bool visible=std::find(completed["session"]["ids"].begin(),completed["session"]["ids"].begin()+3,newTask["id"])!=completed["session"]["ids"].begin()+3;
  check(flag(engine.current(after),"lookupHint")==visible,"Return exposure follows actually visible gallery words");check(after["session"]["current"]==advanced["session"]["current"]&&after["session"]["draft"]==advanced["session"]["draft"],"Return exposure preserves next question and draft");
  returnedExposure=visible;if(!visible)check(d.dispatch(VJ{{"action","v-journey-resume"}}.dump()),"Continue after nonmatching gallery");
 }
 check(returnedExposure,"Covered returning to a gallery containing the new current answer");
 NativeVocabulary reopened(draftDir.generic_string());check(reopened.enter(now),"Reopen");check(saved(draftDir)==after,"Durable stamps and original FSRS/draft retained after reopening");
 auto learning=step(chosen,VJ{{"type","begin"},{"intent","new"}});const auto learnDir=out/"learning-web-recognition";fixture(learnDir,learning);NativeVocabulary l(learnDir.generic_string());check(l.enter(now),"Recognition fixture");check(l.dispatch(VJ{{"action","v-go"},{"value","word-batch"}}.dump()),"Open recognition");const auto recognition=l.page();paint(recognition,"recognition");check(std::any_of(recognition.blocks.begin(),recognition.blocks.end(),[](const auto&b){return b.picture>=0;}),"Recognition picture present");
 for(int count:{0,1,6,7}){
  auto scenario=completed;scenario["journey"]["stamps"]=VJ::array();
  for(int i=0;i<count;++i){auto st=completed["journey"]["stamps"][0];st["sessionId"]="home-fixture-"+std::to_string(i);scenario["journey"]["stamps"].push_back(st);}
  const auto sd=out/("learning-web-home-stamps-"+std::to_string(count));fixture(sd,scenario);NativeVocabulary home(sd.generic_string());check(home.enter(now),"Stamp-count fixture loads");auto p=home.page();paint(p,"home-stamps-"+std::to_string(count));
  check(std::count_if(p.blocks.begin(),p.blocks.end(),[](const auto&b){return b.role=="vocab-home-stamp";})==6,"Six home stamp positions even when empty");
  check(std::count_if(p.blocks.begin(),p.blocks.end(),[](const auto&b){return b.role=="vocab-home-stamp"&&b.selected;})==(count?((count-1)%6)+1:0),"Only real earned stamps appear filled");
  check(saved(sd)==scenario,"Home does not invent or alter progress");
  check(home.dispatch(VJ{{"action","v-go"},{"value","collection"}}.dump()),"Open album count fixture");auto album=home.page();paint(album,"album-stamps-"+std::to_string(count));
  check(std::count_if(album.blocks.begin(),album.blocks.end(),[](const auto&b){return b.role=="vocab-album-stamp";})==6,"Six collection slots");
  if(count==7){check(home.dispatch(VJ{{"action","v-collection-page"},{"page",1}}.dump()),"Next six-stamp spread");auto p2=home.page();paint(p2,"album-stamps-7-next");check(std::count_if(p2.blocks.begin(),p2.blocks.end(),[](const auto&b){return b.role=="vocab-album-stamp"&&b.enabled;})==1,"One earned stamp on second page");}
 }
 // Historical stamps can predate the current book and contain more than one stage.
 {
  auto profile=chosen;
  const auto idFor=[&](const std::string &stage){
   const auto it=std::find_if(catalog.entries.begin(),catalog.entries.end(),[&](const auto &e){return e.stage==stage;});
   check(it!=catalog.entries.end(),"Test stage exists");return it->id;
  };
  const auto daily=idFor("everyday"),gre=idFor("gre-trial");
  auto make=[&](const char *key,const char *source,VJ ids){
   auto st=completed["journey"]["stamps"][0];st["sessionId"]=key;st["source"]=source;st["ids"]=std::move(ids);return st;
  };
  profile["selectedStage"]="gre-trial";
  profile["journey"]=completed["journey"];
  profile["journey"]["stamps"]=VJ::array({make("old","new",{daily}),make("gre","new",{gre}),
     make("both","review",{daily,gre}),make("book","notebook",{daily}),make("gone","review",{gre,"absent-id"})});
  const auto dir=out/"learning-web-stamp-origins";fixture(dir,profile);NativeVocabulary view(dir.generic_string());
  check(view.enter(now),"Load old stamps");check(view.dispatch(VJ{{"action","v-go"},{"value","collection"}}.dump()),"Open origin fixtures");
  const auto album=view.page();paint(album,"stamp-book-labels");std::vector<std::string> origins;
  for(const auto &b:album.blocks)if(b.role=="vocab-album-number")origins.push_back(b.detail);
  check(origins==std::vector<std::string>({"日常核心","GRE 难词试学","多词库","我的单词本","来源待确认",""}),"Per-stamp sources and empty slot");
  check(view.dispatch(VJ{{"action","v-stamp"},{"value","both"}}.dump()),"Open mixed stamp");auto detail=view.page();
  std::vector<std::string> sources;for(const auto &b:detail.blocks)if(b.role=="stamp-book")sources.push_back(b.text);
  check(sources==std::vector<std::string>({"日常核心","GRE 难词试学"}),"Detail lists contributing books");
  check(saved(dir)==profile,"Browsing old stamp origins preserves vocabulary and award data");
 }
 std::ofstream(out/"checks.json")<<VJ{{"passed",checks},{"picturesDecoded",pictureCount},{"mappedEntries",std::size(vocabulary_pictures)},{"synthetic",true},{"realDevice",false}}.dump(2);std::cout<<"Journey/illustration checks passed "<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
