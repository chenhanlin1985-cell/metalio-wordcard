#include "wordcard.hpp"
#include <fstream>
#include <iostream>
#include <set>
using namespace wordcard;
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    std::string root=argv[1], error;
    std::map<std::string,AudioEntry> audio;
    std::ifstream ai(root+"/audio_index.csv");
    if (!load_audio_index(ai,audio,error) || audio.size()!=3869) return 2;
    size_t checked=0; std::set<std::string> paths;
    for (auto name : {"KET_2025_OFFICIAL.csv","PET_2024.csv"}) {
        std::vector<Card> cards; std::ifstream f(root+"/decks/"+name);
        if (!load_deck(f,cards,error)) { std::cerr<<error; return 3; }
        for (const auto &card:cards) {
            const auto a=audio.find(card.id);
            if (card.ipa.empty() || a==audio.end() || a->second.path.empty()) return 4;
            if (card.front=="IT" && a->second.path=="audio/it.wav") return 5;
            paths.insert(a->second.path); ++checked;
        }
    }
    if(checked!=3869) return 6;
    uint64_t total=0; int16_t block[997];
    for(const auto &path:paths) {
        WaveReader wav;
        if(!wav.open(root+"/"+path,error)) {std::cerr<<path<<": "<<error;return 7;}
        uint32_t read=0;
        while(auto n=wav.read(block,997))read+=uint32_t(n);
        if(read!=wav.samples() || wav.remaining()!=0) return 8;
        total+=read;
    }
    std::cout<<"PASS: firmware CSV/IPA/audio lookup for "<<checked<<" cards; streamed "<<paths.size()<<" current WAVs, "<<total<<" samples\n";
}
