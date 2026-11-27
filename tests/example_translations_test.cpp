#include "vocabulary.hpp"
#include "wordcard_ui.hpp"
#include "web_disclosure.hpp"
#include "native_study.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
namespace wordcard { std::string native_learning_json(); }
using namespace wordcard;namespace fs=std::filesystem;
static int checks=0;
static void ck(bool ok,const char*s){++checks;if(!ok)throw std::runtime_error(s);}
static void fixture(const fs::path&p,const VJ&s){fs::create_directories(p/"vocabulary-v2");auto d=VJ{{"state",s},{"journalOffset",0}}.dump();uint32_t h=2166136261;for(unsigned char b:d)h=(h^b)*16777619;std::ofstream(p/"vocabulary-v2/state.json",std::ios::binary)<<VJ{{"data",d},{"checksum",h}}.dump()<<'\n';}
int main(int argc,char**argv){try{
 ck(argc==4,"catalog font output");fs::path out=argv[3];ck(!fs::exists(out),"fresh fixtures only");fs::create_directories(out/"vocabulary");fs::create_directories(out/"frames");
 for(auto n:{"catalog.json","catalog.tsv","catalog.ndjson","pictures-v1.bin","pictures-v2.bin"})fs::copy_file(fs::path(argv[1])/n,out/"vocabulary"/n);
 VocabularyCatalog cat;ck(cat.load((out/"vocabulary").generic_string()),"catalog loads");ck(cat.entries.size()==4738,"all 4738 records");
 for(const auto&e:cat.entries){auto w=cat.get(e.id);ck(!str(w,"example").empty()&&!str(w,"exampleZh").empty(),"all examples translated");}
 int legacy=0;std::function<void(const J&)> walk=[&](const J&x){if(x.is_object()){if(x.contains("example")&&!str(x,"example").empty()){++legacy;ck(!str(x,"exampleZh").empty(),"legacy translated");}for(auto it=x.begin();it!=x.end();++it)walk(it.value());}else if(x.is_array())for(auto&v:x)walk(v);};walk(J::parse(native_learning_json()));ck(legacy==212,"212 legacy occurrences");
 std::ifstream ff(argv[2],std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(ff),{}};Font font;ck(font.init((const uint8_t*)bytes.data(),bytes.size()),"font");std::vector<uint8_t> fb(Canvas::bytes);Canvas c(fb.data(),font);UI ui;View view;view.screen=Screen::WebLearning;
 auto render=[&](const WebPage&p,const std::string&name){view.web=p;view.web_page=0;ui.render(c,view);const int pages=ui.answer_pages();ck(pages>0&&pages<100,"bounded pagination");for(int page=0;page<pages;++page){view.web_page=page;ui.render(c,view);std::ofstream f(out/"frames"/(name+"-"+std::to_string(page+1)+".pgm"),std::ios::binary);f<<"P5\n684 1216\n255\n";for(int y=0;y<1216;++y)for(int x=0;x<684;++x){char b=char(c.shade(x,y)*17);f.write(&b,1);}auto buttons=ui.buttons();for(size_t i=0;i<buttons.size();++i){auto a=buttons[i].rect;ck(a.x>=0&&a.y>=0&&a.x+a.w<=684&&a.y+a.h<=1216,"screen bounds");for(size_t j=0;j<i;++j){auto b=buttons[j].rect;ck(a.x>=b.x+b.w||b.x>=a.x+a.w||a.y>=b.y+b.h||b.y>=a.y+a.h,"buttons do not overlap");}}}return pages;};
 VocabularyEngine eng(cat);int64_t now=1794900000000LL;VJ ev;auto reduce=[&](const VJ&s,VJ a){return eng.reduce(s,a,++now,ev);};auto seed=reduce(eng.create(now),VJ{{"type","chooseStage"},{"stage","gre-trial"}});auto s=reduce(seed,VJ{{"type","begin"},{"intent","new"},{"associative",true}});
 auto open=[&](const char*name,const VJ&state){auto dir=out/name;fixture(dir,state);auto m=std::make_unique<NativeVocabulary>(dir.generic_string());ck(m->enter(now),"model enter");ck(m->dispatch(VJ{{"action","v-go"},{"value","word-batch"}}.dump()),"open study");return m;};
 auto pair=[&](const WebPage&p,const VJ&w){bool found=false;for(auto&b:p.blocks)if(b.role=="card-example"){ck(b.text==str(w,"example")&&b.detail==str(w,"exampleZh"),"paired exact translation");found=true;}ck(found,"listening transcript present");
   auto front=p;layout_word_card(front,false);
   for(const auto &b:front.blocks)ck(b.text!=str(w,"example")&&b.detail!=str(w,"exampleZh"),"transcript hidden on card");
   auto button=std::find_if(front.blocks.begin(),front.blocks.end(),[](const auto &b){return b.role=="example-audio";});
   ck(button!=front.blocks.end(),"listening available on card");
   auto listening=web_disclosure(p,button->action);
   ck(listening.title=="听例句" && listening.blocks[1].text==str(w,"example") &&
      listening.blocks[2].text==str(w,"exampleZh") && listening.blocks.back().text=="再听一遍","matching sentence, translation and replay");};

 // A multi-word teaching page must bind each listen button to its own pair.
 WebPage lesson;
 for(const auto &word : {"first","second"}){
  lesson.blocks.push_back(card_line(word,"word"));
  auto example=card_line(std::string("The ")+word+" example.","card-example");
  example.detail=std::string(word)=="first"?"第一句。":"第二句。";
  lesson.blocks.push_back(example);
 }
 auto lessonFront=lesson;
 layout_word_card(lessonFront,false);
 for(int i:{1,3}){
  const auto &button=lessonFront.blocks[i];
  ck(button.text=="听例句" && button.action==i,"distinct listen targets in a multi-word lesson");
  auto transcript=web_disclosure(lesson,button.action);
  ck(transcript.blocks[0].text==lesson.blocks[i-1].text && transcript.blocks[1].text==lesson.blocks[i].text &&
     transcript.blocks[2].text==lesson.blocks[i].detail,"selected word and both lines belong together");
 }
 auto learn=open("learning-web-learn",s);pair(learn->page(),cat.get(str(s["session"]["ids"][0])));render(learn->page(),"learn");const auto status=learn->status();learn->page();ck(status==learn->status(),"render never updates progress");
 while(str(s["session"],"phase")=="learn")s=reduce(s,VJ{{"type","learnNext"},{"sessionId",s["session"]["id"]},{"learnAt",s["session"]["learnAt"]}});
 auto question=open("learning-web-question",s);auto task=eng.current(s);auto w=cat.get(str(task,"id"));for(auto&b:question->page().blocks){ck(b.role!="card-example","question has no translated answer");ck(b.text!=str(w,"exampleZh")&&b.detail!=str(w,"exampleZh"),"meaning translation not leaked");}render(question->page(),"question");
 s=reduce(s,VJ{{"type","answer"},{"taskId",task["uid"]},{"unknown",true}});auto feedback=open("learning-web-feedback",s);pair(feedback->page(),w);render(feedback->page(),"feedback");
 ck(learn->dispatch(VJ{{"action","v-open"},{"value","legacy:u1a-w2"}}.dump()),"legacy word detail");auto detail=learn->page();pair(detail,cat.get("legacy:u1a-w2"));render(detail,"translated-detail");
 auto longPage=detail;for(auto&b:longPage.blocks)if(b.role=="card-example"){for(int i=0;i<20;i++){b.text+=" Please confirm the appointment tomorrow.";b.detail+="请确认明天的预约时间。";}}ck(render(longPage,"long-hidden")==render(detail,"short-hidden"),"length of hidden sentence does not affect card");
 const int index=int(std::find_if(longPage.blocks.begin(),longPage.blocks.end(),[](const auto&b){return b.role=="card-example";})-longPage.blocks.begin());
 view.screen=Screen::WebPrompt;view.web_example=index;
 ck(render(longPage,"long-pair")>2,"long bilingual example paginates when listening");
 view.screen=Screen::WebLearning;view.web_example=-1;
 // Pixel comparison verifies that the translation is actually drawn, not just
 // present in the model or hidden behind disclosure.
 WebPage p;p.key="vocabulary:word:test";p.route="word";p.title="例句";WebBlock b;b.kind="text";b.area="body";b.role="example-pair";b.text="We live near the station.";b.detail="我们住在车站附近。";p.blocks.push_back(b);view.web=p;view.web_page=0;ui.render(c,view);auto withZh=fb;view.web.blocks[0].detail.clear();ui.render(c,view);size_t changed=0;for(size_t i=0;i<fb.size();++i)changed+=fb[i]!=withZh[i];ck(changed>100,"Chinese visibly rendered");
 // A fitting pair moves as a whole when there is insufficient remaining room.
 WebBlock filler;filler.kind="text";filler.area="body";filler.text="one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten\neleven\ntwelve\nthirteen\nfourteen\nfifteen\nsixteen\nseventeen";p.blocks.insert(p.blocks.begin(),filler);view.web=p;view.web_page=0;ui.render(c,view);ck(ui.answer_pages()==2,"pair is moved to next page");view.web_page=1;ui.render(c,view);auto pairedPage=fb;view.web.blocks.erase(view.web.blocks.begin());view.web_page=0;ui.render(c,view);for(int y=194;y<350;++y)for(int x=24;x<660;++x){size_t offset=((683-x)*1216+y)/2;if(pairedPage[offset]!=fb[offset]){std::cerr<<"Pair mismatch x="<<x<<" y="<<y<<" lines="<<c.wrap_px(filler.text,628,40).size()<<" en="<<c.wrap_px(b.text,628,40).size()<<" zh="<<c.wrap_px(b.detail,628,40).size()<<"\n";throw std::runtime_error("Pair position");}++checks;}
 std::ofstream(out/"checks.json")<<VJ{{"passed",checks},{"catalogExamples",4738},{"legacyOccurrences",legacy},{"physicalDevice",false}}.dump(2);std::cout<<"Translation checks passed "<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
