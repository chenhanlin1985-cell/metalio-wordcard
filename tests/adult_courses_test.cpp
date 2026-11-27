#include "wordcard_ui.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
using namespace wordcard;
static void require(bool ok, const std::string &why) { if(!ok) throw std::runtime_error(why); }
int main(int argc, char **argv) {
    try {
        require(argc==4,"arguments");
        std::string root=argv[1], out=argv[3], error;
        std::ifstream f(argv[2],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
        Font font; require(font.init(bytes.data(),bytes.size()),"font");
        std::vector<uint8_t> framebuffer(Canvas::bytes); Canvas canvas(framebuffer.data(),font); UI ui;
        size_t total=0,groups_total=0; std::set<std::string> audio_paths, ids;
        for(auto book:{"adult-daily","adult-tech","adult-work"}) {
            std::vector<CatalogBook> catalog;
            std::ifstream cf(root+"/catalog.d/"+book+".csv");
            require(load_catalog(cf,catalog,error),error);
            require(catalog.size()==1 && catalog[0].id==book && catalog[0].filename==std::string("custom-")+book+".csv","catalog lookup");
            std::vector<Card> cards;
            std::ifstream csv(root+"/decks/custom-"+book+".csv");
            require(load_deck(csv,cards,error),error); require(cards.size()==48,"book count");
            std::map<std::string,AudioEntry> index;
            std::ifstream af(root+"/audio.d/"+book+".csv");
            require(load_audio_index(af,index,error),error); require(index.size()==cards.size(),"audio count");
            std::set<std::string> groups;
            for(const auto &c:cards) {
                require(ids.insert(c.id).second,"stable ID collision");
                require(c.id.find(std::string("custom:")+book+":")==0,"book namespace");
                auto a=index.find(c.id);
                require(a!=index.end() && a->second.source_text==c.front && !a->second.review_needed,"audio lookup");
                audio_paths.insert(a->second.path); groups.insert(c.group);
                int scale=canvas.text_width(c.front,3)<=620?3:2;
                auto lines=canvas.wrap(c.front,620,scale);
                if(lines.size()>2) {scale=1;lines=canvas.wrap(c.front,620,1);}
                require(230+int(lines.size())*scale*44+30+88<=550,"front overflow: "+c.id);
                require(!c.ipa.empty(),"missing IPA");
                for(const auto &s:canvas.wrap(c.ipa+"\n"+c.back,616)) require(canvas.text_width(s)<=616,"answer overflow");
                require(canvas.wrap(c.group,612).size()<=2,"group overflow");
                auto player=canvas.wrap(c.front,604,2);
                if(player.size()>2) player=canvas.wrap(c.front,604,1);
                require(player.size()<=4,"player overflow");
                // Both question and revealed states use the unchanged firmware UI.
                View v;v.screen=Screen::Study;v.card=&c;v.book_name=catalog[0].name;v.deck=catalog[0].name;v.group=c.group;
                v.count=12;v.writable=true;v.audio_available=true;
                for(auto mode:{StudyMode::Visual,StudyMode::Listening,StudyMode::Reverse}) {
                    v.study_mode=mode;
                    for(bool reveal:{false,true}) {v.revealed=reveal;ui.render(canvas,v);}
                }
                if(c.id.find("01-word-06")!=std::string::npos || c.id.find("02-sentence-06")!=std::string::npos) {
                    v.study_mode=StudyMode::Visual; v.revealed=true;ui.render(canvas,v);
                    const std::string suffix=c.id.find("-word-")!=std::string::npos?"word":"sentence";
                    std::ofstream pgm(out+"/"+book+"-"+suffix+".pgm",std::ios::binary);
                    pgm<<"P5\n684 1216\n255\n";
                    for(int y=0;y<Canvas::height;y++)for(int x=0;x<Canvas::width;x++)pgm.put(char(canvas.shade(x,y)*17));
                }
                ++total;
            }
            require(groups.size()==4,"lesson grouping");groups_total+=groups.size();
        }
        uint64_t samples=0;
        for(const auto &path:audio_paths) {
            WaveReader w;require(w.open(root+"/"+path,error),path+": "+error);
            int16_t block[997];uint32_t read=0;
            while(auto n=w.read(block,997))read+=uint32_t(n);
            require(read==w.samples() && w.remaining()==0,"truncated WAV");samples+=read;
        }
        require(total==144 && groups_total==12,"course totals");
        std::cout<<"PASS: firmware parsed "<<total<<" cards in "<<groups_total<<" lessons; all word, IPA, definition and player layouts fit; streamed "<<audio_paths.size()<<" mapped WAV files ("<<samples<<" samples); rendered 864 study states and 6 previews\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
