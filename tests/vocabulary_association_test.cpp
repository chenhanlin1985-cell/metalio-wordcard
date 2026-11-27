#include "vocabulary.hpp"
#include "wordcard_ui.hpp"
#include "panel_power_lease.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using namespace wordcard;namespace fs=std::filesystem;
static int checks=0;static void ck(bool ok,const char*m){++checks;if(!ok)throw std::runtime_error(m);}
static void fixture(const fs::path&p,const VJ&s){fs::create_directories(p/"vocabulary-v2");auto d=VJ{{"state",s},{"journalOffset",0}}.dump();uint32_t h=2166136261;for(unsigned char b:d)h=(h^b)*16777619;std::ofstream(p/"vocabulary-v2/state.json",std::ios::binary)<<VJ{{"data",d},{"checksum",h}}.dump()<<'\n';}
int main(int argc,char**argv){try{
 ck(argc==4,"catalog font fresh output");fs::path out=argv[3];ck(!fs::exists(out),"Fresh fixture required");fs::create_directories(out/"vocabulary");fs::create_directories(out/"frames");
 for(auto n:{"catalog.json","catalog.tsv","catalog.ndjson","pictures-v1.bin","pictures-v2.bin"})fs::copy_file(fs::path(argv[1])/n,out/"vocabulary"/n);
 VocabularyCatalog cat;ck(cat.load((out/"vocabulary").generic_string()),"catalog");VocabularyEngine eng(cat);VJ ev;int64_t now=1794900000000LL;
 auto act=[&](VJ s,VJ a){return eng.reduce(s,a,++now,ev);};auto seed=eng.create(now);
 std::vector<std::string> gre;std::string old;
 for(const auto&e:cat.entries){if(e.stage=="gre-trial")gre.push_back(e.id);if(old.empty()&&e.stage=="foundation")old=e.id;}
 ck(gre.size()==140,"140 GRE scenes");for(const auto&id:gre){auto w=cat.get(id);ck(!str(w,"memoryCue").empty(),"Scene exists");ck(str(w,"memoryCue").find(str(w,"word"))==std::string::npos,"No answer in scene");}
 seed=act(seed,VJ{{"type","chooseStage"},{"stage","gre-trial"}});
 seed["seen"][old]=now;seed["cards"][old]["meaning"]=eng.empty_card(now-1);
 auto oldSession=act(seed,VJ{{"type","begin"},{"intent","review"}})["session"];
 oldSession["draft"]=VJ{{"taskId",oldSession["current"]},{"value","saved-input"}};seed["paused"].push_back(oldSession);
 for(int i=0;i<5;i++){seed["seen"][gre[i]]=now;auto c=eng.empty_card(now+86400000);seed["cards"][gre[i]]["meaning"]=c;seed["cards"][gre[i]]["spelling"]=c;}
 auto before=seed;auto q=eng.review_tasks(seed,now,"current");ck(seed==before,"Listing does not mutate progress");ck(q.size()==5,"Current known GRE seeds productive recall, not future old channels");
 for(const auto&t:q)ck(str(t,"kind")=="recall"&&cat.entry(str(t,"id"))->stage=="gre-trial","No unrelated review");
 ck(eng.resumable(seed,"review","current").is_null(),"Old book is not resumed in current scope");ck(eng.resumable(seed,"review","all")==oldSession,"Old resume remains exact");
 auto s=act(seed,VJ{{"type","begin"},{"intent","review"},{"scope","current"},{"associative",true}});
 ck(s["paused"][0]==oldSession,"Old draft preserved");auto t=eng.current(s);ck(str(t,"kind")=="recall","Review opens recall");
 auto independent=s;independent=act(independent,VJ{{"type","answer"},{"taskId",t["uid"]},{"value",cat.entry(str(t,"id"))->word}});
 ck(num(independent["cards"][str(t,"id")]["recall"],"successes")==1,"Independent success recorded separately");ck(independent["cards"][str(t,"id")]["recall"]["independentDays"].empty(),"One success not mastery");
 s=act(s,VJ{{"type","draft"},{"taskId",t["uid"]},{"value","ab"}});s=act(s,VJ{{"type","associationHint"},{"taskId",t["uid"]}});
 ck(str(s["session"]["draft"],"value")=="ab","Hint retains input");ck(flag(eng.current(s),"hint")&&!flag(eng.current(s),"answerRevealed"),"Cue marks assistance without full answer");
 s=act(s,VJ{{"type","answer"},{"taskId",t["uid"]},{"value",cat.entry(str(t,"id"))->word}});
 ck(flag(s["session"]["feedback"],"assisted"),"Correct cued response is not independent");ck(num(s["cards"][str(t,"id")]["recall"],"lapses")==1,"Assisted recall scheduled again");
 ck(s["cards"][str(t,"id")]["meaning"]==seed["cards"][str(t,"id")]["meaning"],"Meaning memory unchanged");ck(s["cards"][str(t,"id")]["spelling"]==seed["cards"][str(t,"id")]["spelling"],"Old spelling memory unchanged");
 s=act(s,VJ{{"type","continue"},{"taskId",t["uid"]}});ck(eng.current(s)["id"]!=t["id"],"No immediate repeat after cue");
 auto audio=act(before,VJ{{"type","begin"},{"intent","review"},{"scope","current"},{"associative",true}});auto at=eng.current(audio);audio=act(audio,VJ{{"type","audio"},{"id",at["id"]}});audio=act(audio,VJ{{"type","answer"},{"taskId",at["uid"]},{"value",cat.entry(str(at,"id"))->word}});ck(flag(audio["session"]["feedback"],"assisted"),"Heard spelling is not independent recall");
 auto onlyMeaning=seed;onlyMeaning["settings"]["mode"]="meaning";ck(eng.review_tasks(onlyMeaning,now,"current").empty(),"Meaning-only preference preserved");
 // Real native pages, persisted hints/drafts, explicit scope and Canvas bounds.
 std::ifstream ff(argv[2],std::ios::binary);std::string fontbytes{std::istreambuf_iterator<char>(ff),{}};Font font;ck(font.init((const uint8_t*)fontbytes.data(),fontbytes.size()),"Font");std::vector<uint8_t> fb(Canvas::bytes);Canvas canvas(fb.data(),font);UI ui;View view;view.screen=Screen::WebLearning;
 auto paint=[&](NativeVocabulary&model,const std::string&name){auto p=model.page();view.web=p;view.web_page=0;ui.render(canvas,view);std::ofstream f(out/"frames"/(name+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";for(int y=0;y<1216;y++)for(int x=0;x<684;x++){char c=char(canvas.shade(x,y)*17);f.write(&c,1);}const auto&bs=ui.buttons();for(size_t i=0;i<bs.size();++i){auto a=bs[i].rect;ck(a.x>=0&&a.y>=0&&a.x+a.w<=684&&a.y+a.h<=1216,"Control on screen");for(size_t j=0;j<i;++j){auto b=bs[j].rect;ck(a.x>=b.x+b.w||b.x>=a.x+a.w||a.y>=b.y+b.h||b.y>=a.y+a.h,"Controls non-overlap");}}return p;};
 fixture(out/"learning-web-review",seed);NativeVocabulary model((out/"learning-web-review").generic_string());ck(model.enter(now),"Enter review fixture");
 auto send=[&](VJ a){ck(model.dispatch(a.dump()),"Native action");ck(model.page().error.empty(),"No native error");};
 send(VJ{{"action","v-go"},{"value","review"}});paint(model,"review-current");send(VJ{{"action","v-review-scope"},{"value","all"}});paint(model,"review-all");send(VJ{{"action","v-review-scope"},{"value","current"}});send(VJ{{"action","v-start"},{"intent","review"}});paint(model,"recall-question");ck(model.type("ab"),"Typing");send(VJ{{"action","v-do"},{"type","associationHint"}});auto hintPage=paint(model,"recall-cue");ck(hintPage.typed=="ab","Hint page draft");
 NativeVocabulary reloaded((out/"learning-web-review").generic_string());ck(reloaded.enter(now),"Restart");ck(reloaded.dispatch(VJ{{"action","v-start"},{"intent","review"}}.dump()),"Resume");ck(reloaded.page().typed=="ab","Restart preserves input");ck(reloaded.page().speech_note.find("用过提示")!=std::string::npos,"Restart preserves assistance");
 send(VJ{{"action","w-unknown"}});paint(model,"recall-feedback");send(VJ{{"action","v-return"},{"value","review"}});send(VJ{{"action","v-go"},{"value","home"}});paint(model,"home");
 auto fresh=act(eng.create(now),VJ{{"type","chooseStage"},{"stage","gre-trial"}});fresh=act(fresh,VJ{{"type","begin"},{"intent","new"},{"associative",true}});int recallCount=0;for(auto&q:fresh["session"]["tasks"]){if(str(q,"kind")=="recall")++recallCount;ck(str(q,"kind")!="spelling","New groups use recall instead of old spelling");}ck(recallCount==5,"Five productive tasks");
 fixture(out/"learning-web-learn",fresh);NativeVocabulary learner((out/"learning-web-learn").generic_string());ck(learner.enter(now),"Learn fixture");learner.dispatch(VJ{{"action","v-go"},{"value","word-batch"}}.dump());paint(learner,"scene-learning");
 // Completed old task states are preserved, not rewritten on entry.
 auto continued=act(fresh,VJ{{"type","begin"},{"intent","new"},{"associative",true}});ck(continued["session"]==fresh["session"],"Resume never replaces session");
 // Finish a new group with one failed recall and delayed correction. The three
 // distinct intervening words rule must still hold across all prompt rounds.
 auto run=fresh;bool failed=false;int steps=0;std::vector<std::string> exposures;std::string failedId;
 while(str(run["session"],"phase")!="summary") {
  ck(++steps<100,"Full group terminates");const auto&ss=run["session"];
  if(str(ss,"phase")=="learn"){exposures.push_back(str(ss["ids"][size_t(num(ss,"learnAt"))]));run=act(run,VJ{{"type","learnNext"},{"sessionId",ss["id"]}});continue;}
  auto q=eng.current(run);if(!field(ss,"feedback").is_null()){run=act(run,VJ{{"type","continue"},{"taskId",ss["feedback"]["uid"]}});continue;}
  const auto id=str(q,"id"),kind=str(q,"kind");
  if(flag(q,"followup")){std::set<std::string> other;for(auto it=exposures.rbegin();it!=exposures.rend()&&*it!=id;++it)other.insert(*it);ck(other.size()>=3,"Failed word separated by three distinct words");}
  auto value=kind=="meaning"?cat.entry(id)->meaning:kind=="gap"?str(eng.gap(q),"answer"):cat.entry(id)->word;
  if(kind=="recall"&&!failed){failed=true;failedId=id;value="incorrect";}
  run=act(run,VJ{{"type","answer"},{"taskId",q["uid"]},{"value",value}});exposures.push_back(id);
 }
 ck(failed,"Productive recall actually exercised");ck(run["seen"].size()==5,"Exactly five words learned");
 for(const auto&id:run["session"]["ids"]){auto cards=run["cards"][str(id)];ck(cards.contains("meaning")&&cards.contains("recall")&&!cards.contains("spelling"),"New word gets independent meaning and recall channels");ck(cards["recall"]["independentDays"].empty(),"No same-session mastery credit");}
 ck(run["journey"]["stamps"].size()==1,"One completion stamp only");
 auto nextDay=eng.review_tasks(run,now+86400000LL,"current");ck(!nextDay.empty(),"Current learning feeds scheduled review");for(auto&q:nextDay)ck(cat.entry(str(q,"id"))->stage=="gre-trial","Scheduled review stays in current course");
 auto looked=act(before,VJ{{"type","begin"},{"intent","review"},{"scope","current"},{"associative",true}});auto lt=eng.current(looked);looked=act(looked,VJ{{"type","lookup"},{"id",lt["id"]}});ck(flag(eng.current(looked),"hint"),"Lookup is assisted recall");
 now+=86400000LL;auto faded=independent;faded["session"]=nullptr;faded["paused"]=VJ::array();
 for(auto&word:faded["cards"])for(auto&card:word)card["due"]=now+86400000LL;
 faded["cards"][str(t,"id")]["recall"]["due"]=now-1;
 faded=act(faded,VJ{{"type","begin"},{"intent","review"},{"scope","current"},{"associative",true}});ck(eng.current(faded)["id"]==t["id"],"Due recall precedes first attempts");
 fixture(out/"learning-web-faded",faded);NativeVocabulary fading((out/"learning-web-faded").generic_string());ck(fading.enter(now),"Faded fixture");fading.dispatch(VJ{{"action","v-start"},{"intent","review"}}.dump());auto fp=paint(fading,"meaning-only");bool fadeText=false;for(const auto&b:fp.blocks){fadeText|=b.text=="在脑中还原场景，想出目标词。";ck(b.text!=str(cat.get(str(t,"id")),"exampleZh"),"Faded review hides the example context");}ck(fadeText,"Faded prompt shown");
 auto ft=eng.current(faded);faded=act(faded,VJ{{"type","answer"},{"taskId",ft["uid"]},{"value",cat.entry(str(ft,"id"))->word}});bool evidence=false;for(const auto&e:ev)if(str(e,"type")=="answer")evidence=str(e,"promptMode")=="meaning_only"&&str(e,"taskType")=="meaning_to_word_recall";ck(evidence,"Logs distinguish meaning-only retrieval");
 auto homePage=model.page();view.web=homePage;ui.render(canvas,view);int settings=0;for(const auto&b:ui.buttons())settings+=b.label=="设置";ck(settings==1,"One settings control on home");
 for(auto route:{"home","review","collection","gallery","word","settings"})ck(panel_static_native_page(std::string("vocabulary:")+route+":s:tasks:x"),"Static route closes rails now");
 ck(!panel_static_native_page("vocabulary:word-batch:s:tasks:x"),"Study burst retained");ck(!panel_static_native_page("vocabulary:search:::"),"Search typing burst retained");ck(!panel_static_native_page("word-batch|legacy"),"Legacy unaffected");
 std::ofstream(out/"checks.json")<<VJ{{"passed",checks},{"sceneCount",140},{"physicalDevice",false}}.dump(2);std::cout<<"Association checks passed "<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
