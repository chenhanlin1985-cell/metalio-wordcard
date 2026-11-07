#include "wordcard_ui.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace wordcard;
namespace fs=std::filesystem;
static int checks=0;
static void check(bool ok,const std::string &message) {++checks;if(!ok)throw std::runtime_error(message);}
int main(int argc,char **argv) {
    try {
        check(argc==5,"arguments");
        std::string error;fs::path root=argv[1],dict=argv[2],out=argv[4];
        std::ifstream ff(argv[3],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(ff)),{});Font font;
        check(font.init(bytes.data(),bytes.size()),"font init");
        std::vector<uint8_t> fb(Canvas::bytes);Canvas canvas(fb.data(),font);
        for(auto text:{"The device crashes after the firmware update.","Yes. I will confirm the time by email.",
                       "Send the log and the steps to reproduce the crash.","Don't reinstall the Wi-Fi driver."}) {
            for(int scale:{1,2}) {
                auto lines=canvas.wrap(text,604,scale);std::string combined;
                for(const auto &line:lines) {
                    check(canvas.text_width(line,scale)<=604,"line width");
                    if(!combined.empty())combined+=' ';combined+=line;
                }
                check(combined==text,"word boundary preserved: "+std::string(text));
            }
        }
        check(canvas.wrap("\n中文 English\n",604).size()==2,"explicit newlines");
        for(const auto &line:canvas.wrap(std::string(90,'x'),90))check(canvas.text_width(line)<=90,"long token fallback");
        for(auto track:{"daily","tech","work"}) {
            std::vector<Card> cards;std::ifstream input(root/"decks"/(std::string("custom-adult-")+track+".csv"));
            check(load_deck(input,cards,error),error);LearningSystem learning;learning.bind(&cards);
            const auto group=cards.at(12).group;
            auto c=course_progress(cards,learning.state.memory[0],group);
            check(c.total==12 && c.remaining==12 && c.sentences==6,"initial course counts");
            check(learning.begin("adult.csv",group,100),"begin");
            while(learning.state.lesson.active()) {
                check(learning.reveal(),"reveal");
                check(learning.rate(Rating::Good,100,true,learning.state.lesson.position),"rate");
            }
            c=course_progress(cards,learning.state.memory[0],group);
            check(c.learned==10 && c.remaining==2,"small session is not entire course");
            check(learning.counts(100,group).fresh==0,"daily limit exhausted");
            check(learning.begin_remaining("adult.csv",group,100),"explicit remaining");
            check(learning.state.new_limit==10 && learning.state.lesson.queue.size()==2,"extra keeps plan");
            check(!learning.begin_remaining("adult.csv",group,100),"cannot replace active");
            while(learning.state.lesson.active()) {
                learning.reveal();check(learning.rate(Rating::Good,100,true,learning.state.lesson.position),"extra rate");
            }
            c=course_progress(cards,learning.state.memory[0],group);
            check(c.learned==12 && c.remaining==0,"whole course first pass");
            check(!learning.begin_remaining("adult.csv",group,100),"no unseen content");
            check(learning.undo(),"undo completion");
            check(course_progress(cards,learning.state.memory[0],group).remaining==1,"completion derives from real progress");
            std::vector<const Card*> sentences;
            for(const auto &card:cards)if(card.group==group && is_sentence(card))sentences.push_back(&card);
            check(sentences.size()==6 && sentences[0]->id.find("sentence-01")!=std::string::npos,"sentence-only order");
        }
        Card fake;fake.tags="not-sentence|sentences";check(!is_sentence(fake),"whole tag matching");
        Dictionary dictionary;check(dictionary.open(dict.string(),error),error);
        check(dictionary.count()==770611,"actual dictionary count");
        std::vector<DictionaryMatch> matches;DictionaryEntry entry;
        for(auto word:{"firmware","appointment","latency","protocol","apple","zebra","IT","it"}) {
            check(dictionary.search(word,matches,error),error);check(!matches.empty(),std::string("query ")+word);
            auto exact=std::find_if(matches.begin(),matches.end(),[&](const auto&m){return dictionary_key(m.word)==dictionary_key(word);});
            check(exact!=matches.end(),std::string("exact ")+word);
            check(dictionary.read(exact->index,entry,error),error);
            check(!entry.translation.empty() || !entry.definition.empty(),"definition present");
        }
        check(dictionary.search("firmw",matches,error) && !matches.empty(),"prefix");
        check(dictionary.search("zzzznonexistent",matches,error) && matches.empty(),"no result");
        check(!dictionary.search("   ",matches,error),"empty normalized query");
        for(uint32_t i=0;i<dictionary.count();i+=997) {
            check(dictionary.read(i,entry,error),error);
            check(!entry.word.empty(),"sampled record");
        }
        check(dictionary.read(dictionary.count()-1,entry,error),"last record");
        check(!dictionary.read(dictionary.count(),entry,error),"range check");
        check(personal_word_id("IT")!=personal_word_id("it"),"case-specific personal ids");
        dictionary.search("firmware",matches,error);dictionary.read(matches[0].index,entry,error);
        Card word=dictionary_card(entry);bool already=false;
        auto personal=(out/"personal-roundtrip.csv").string();
        check(add_personal_word(personal,word,already,error) && !already,"add personal word");
        check(add_personal_word(personal,word,already,error) && already,"idempotent add");
        fs::rename(personal,personal+".bak"); // Simulate interrupted rename within this isolated test folder.
        dictionary.search("latency",matches,error);dictionary.read(matches[0].index,entry,error);
        check(add_personal_word(personal,dictionary_card(entry),already,error),"recover before appending");
        std::vector<Card> personal_cards;std::ifstream saved(personal);
        check(load_deck(saved,personal_cards,error) && personal_cards.size()==2,"preserve existing saved words");
        saved.close(); // Match the App's closed read handle before an atomic update on Windows.
        LearningSystem learn_personal;learn_personal.bind(&personal_cards);
        check(learn_personal.begin("custom-my-vocabulary.csv","",200),"personal deck can study");
        check(learn_personal.reveal() && learn_personal.rate(Rating::Hard,200,true,0),"personal review");
        check(save_learning_state((out/"personal-state.csv").string(),learn_personal.state,error),"personal progress save");
        LearningState loaded;check(load_learning_state((out/"personal-state.csv").string(),loaded,error),"personal progress reload");
        check(loaded.memory[0].count(personal_cards[0].id)==1,"stable personal progress ID");
        // App synchronizes this same vector and bind lookup after adding, without ending a paused lesson.
        learn_personal.reveal(); learn_personal.pause();
        const auto paused = learn_personal.state.lesson;
        const auto undo_before = learn_personal.can_undo();
        dictionary.search("appointment",matches,error);dictionary.read(matches[0].index,entry,error);
        const auto added_id = personal_word_id(entry.word);
        check(add_personal_word(personal,dictionary_card(entry),already,error),"append during paused personal lesson");
        check(reload_personal_words(personal,personal_cards,error) && personal_cards.size()==3,"ordinary navigation sees updated cards");
        learn_personal.bind(&personal_cards);
        check(learn_personal.state.lesson.queue==paused.queue && learn_personal.state.lesson.position==paused.position &&
              learn_personal.state.lesson.revealed==paused.revealed && learn_personal.state.lesson.paused==paused.paused &&
              learn_personal.state.lesson.group==paused.group,"sync preserves queue, position, reveal, pause and group");
        check(learn_personal.can_undo()==undo_before,"sync preserves undo");
        check(learn_personal.validate_lesson("custom-my-vocabulary.csv"),"paused lesson still validates");
        learn_personal.resume();
        check(learn_personal.current() && learn_personal.state.lesson.revealed,"resume after addition retains revealed card");
        while(learn_personal.state.lesson.active()) {
            learn_personal.reveal();check(learn_personal.rate(Rating::Good,200,true,learn_personal.state.lesson.position),"finish preserved lesson");
        }
        check(learn_personal.begin("custom-my-vocabulary.csv","",200),"next session after paused addition");
        check(std::find(learn_personal.state.lesson.queue.begin(),learn_personal.state.lesson.queue.end(),added_id)!=learn_personal.state.lesson.queue.end(),"next session includes new word");
        learn_personal.end();
        dictionary.search("apple",matches,error);dictionary.read(matches[0].index,entry,error);
        const auto idle_added_id = personal_word_id(entry.word);
        check(add_personal_word(personal,dictionary_card(entry),already,error),"append with no active lesson");
        check(reload_personal_words(personal,personal_cards,error) && personal_cards.size()==4,"home count updates without reselecting book");
        learn_personal.bind(&personal_cards);
        check(learn_personal.begin("custom-my-vocabulary.csv","",200),"new session after ordinary return");
        check(std::find(learn_personal.state.lesson.queue.begin(),learn_personal.state.lesson.queue.end(),idle_added_id)!=learn_personal.state.lesson.queue.end(),"ordinary return next session includes added word");
        auto damaged=(out/"damaged-personal.csv").string();{std::ofstream f(damaged);f<<"broken";}
        check(!add_personal_word(damaged,word,already,error),"do not overwrite damaged personal deck");
        check(!reload_personal_words(damaged,personal_cards,error) && personal_cards.size()==4,"failed refresh retains current deck");
        {std::ifstream f(damaged);std::string data;f>>data;check(data=="broken","damaged file preserved");}
        auto broken=out/"broken-dictionary";fs::create_directory(broken);
        {std::ofstream f(broken/"index.bin");f<<"WCDICT1";}
        Dictionary invalid;check(!invalid.open(broken.string(),error),"reject truncated dictionary");
        std::cout<<"PASS: "<<checks<<" course/dictionary checks; actual 770611-entry SD dictionary, word boundaries, course completion/undo, personal save/reload and failure recovery.\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
