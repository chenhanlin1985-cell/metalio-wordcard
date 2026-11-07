#pragma once
#include "wordcard_ui.hpp"
#include <array>

namespace wordcard {
struct KeyboardKey { Rect rect; Event event; };
inline Rect dictionary_keyboard_cell(int slot) {
    if(slot<0||slot>=30)return {0,0,0,0};
    if(slot<10)return {2+slot*68,784,68,100};
    if(slot<19)return {36+(slot-10)*68,884,68,100};
    if(slot<26)return {86+(slot-19)*73,984,73,100};
    if(slot==26)return {2,984,84,100};
    if(slot==27)return {144,1084,372,100};
    if(slot==28)return {2,1084,142,100};
    return {597,984,85,100};
}
inline KeyboardKey dictionary_keyboard_key(int slot,bool shift,bool symbols) {
    if(slot<0 || slot>=30)return {{0,0,0,0},{}};
    const auto cell=dictionary_keyboard_cell(slot);
    const Rect rect{cell.x+2,cell.y+8,cell.w-4,cell.h-16};
    if(slot==26&&!symbols)return {rect,{Action::DictionaryShift}};
    if(slot==27)return {rect,{Action::DictionaryKey,' '}};
    if(slot==28)return {rect,{Action::DictionarySymbols}};
    if(slot==29)return {rect,{Action::DictionaryErase}};
    const char *characters=symbols?"1234567890-'./@+_():?!&=,%#":"qwertyuiopasdfghjklzxcvbnm";
    char value=characters[slot];
    if(!symbols && shift)value=char(value-'a'+'A');
    return {rect,{Action::DictionaryKey,int(value)}};
}
inline KeyboardKey dictionary_keyboard_hit(int x,int y,bool shift,bool symbols,bool search_enabled=true,int recent_count=0,bool recall=false,unsigned recent_enabled=3,bool compactStudy=false,bool nativeSearch=false) {
    if(compactStudy){
        if(recent_count>0&&(recent_enabled&1)&&Rect{24,650,312,88}.contains(x,y))return {{24,650,312,88},{Action::DictionaryRecent,0}};
        if(recall&&Rect{348,650,312,88}.contains(x,y))return {{348,650,312,88},{Action::RecallSkip}};
    }else if(recall&&Rect{24,650,636,88}.contains(x,y))return {{24,650,636,88},{Action::RecallSkip}};
    const int backWidth=compactStudy||nativeSearch?184:120;
    if(Rect{24,12,backWidth,96}.contains(x,y))return {{24,12,backWidth,96},{Action::Back}};
    if(nativeSearch&&Rect{224,12,228,96}.contains(x,y))return {{224,12,228,96},{Action::Dictionary}};
    if(Rect{476,12,184,96}.contains(x,y))return {{476,12,184,96},{Action::Vocabulary}};
    for(int i=0;!compactStudy&&i<recent_count&&i<2;++i)if((recent_enabled&(1u<<i))&&Rect{24,398+i*108,636,88}.contains(x,y))return {{24,398+i*108,636,88},{Action::DictionaryRecent,i}};
    if(!compactStudy&&!recall&&recent_count>2&&(recent_enabled&4)&&Rect{24,650,636,88}.contains(x,y))return {{24,650,636,88},{Action::DictionaryRecent,2}};
    if(search_enabled && Rect{516,1084,168,100}.contains(x,y))return {{516,1084,168,100},{Action::DictionarySearch}};
    for(int i=0;i<30;++i){auto cell=dictionary_keyboard_cell(i);
        // Staggered rows have visual margins, not dead touch zones. The edge
        // letter owns that row's margin, as in a nearest-key phone keyboard.
        if(i==0||i==10||i==26||i==28){cell.w+=cell.x;cell.x=0;}
        if(i==9||i==18||i==29)cell.w=684-cell.x;
        if(cell.contains(x,y)){
        auto key=dictionary_keyboard_key(i,shift,symbols);key.rect=cell;return key;
    }}
    return {{0,0,0,0},{}};
}
inline bool dictionary_keyboard_edit(Action a) {
    return a==Action::DictionaryKey || a==Action::DictionaryErase || a==Action::DictionaryClear;
}

// No allocation, no I/O. The S31 adapter protects this object with a short
// critical section, so the touch task can retain taps while the UI scans ink.
class KeyboardInput {
  public:
    static constexpr size_t capacity=64;
    bool publish(bool shift,bool symbols,bool ready,int recent_count=0,bool recall=false,unsigned recent_enabled=3,bool compactStudy=false,bool nativeSearch=false) {
        recent_enabled_=recent_enabled;
        // A search/mode key may have arrived while this unchanged layout was
        // being scanned. Preserve both its barrier and all preceding letters.
        if(active_ && shift_==shift && symbols_==symbols&&recall_==recall&&compact_study_==compactStudy&&native_search_==nativeSearch){ready_=ready;recent_count_=recent_count;const bool sample=resample_;resample_=false;return sample;}
        reset_pending();shift_=shift;symbols_=symbols;ready_=ready;recent_count_=recent_count;recall_=recall;compact_study_=compactStudy;native_search_=nativeSearch;active_=true;barrier_=false;resample_=false;
        // A finger carried over from a page/layout transition must lift first.
        // The feed may have missed a release while paused. Require a fresh
        // controller sample: up arms us, a carried finger must lift first.
        armed_=false;return true;
    }
    void deactivate() {active_=false;barrier_=false;armed_=false;reset_pending();}
    bool active() const{return active_ && !barrier_;}
    bool stable(bool shift,bool symbols) const{return active() && shift_==shift && symbols_==symbols;}
    bool same_layout(bool shift,bool symbols)const{return active_&&shift_==shift&&symbols_==symbols;}
    // Full cleanup on this same keyboard may pause sensing, never erase taps
    // already acknowledged. A carried finger must lift before capture resumes.
    void pause_capture(){armed_=false;down_=false;held_={};repeated_=false;resample_=true;}
    size_t pending() const{return size_t(write_-read_);}
    uint32_t accepted=0,consumed=0,overflow=0,cancelled=0;
    // Return true only when a completed key/repeat was actually queued.
    bool sample(bool down,int x,int y,uint64_t now) {
        bool pushed=false;
        if(!active_ || barrier_) {down_=down;if(!down)armed_=true;return false;}
        if(!down) {
            if(down_ && armed_ && held_.event.action!=Action::None && !repeated_) {
                if(within(last_x_,last_y_))pushed=push(held_.event);
                else ++cancelled;
            }
            down_=false;armed_=true;held_={};repeated_=false;return pushed;
        }
        if(!down_) {
            held_=armed_?dictionary_keyboard_hit(x,y,shift_,symbols_,ready_,recent_count_,recall_,recent_enabled_,compact_study_,native_search_):KeyboardKey{};
            started_=now;repeat_at_=now+500000;repeated_=false;
        } else if(armed_ && held_.event.action==Action::DictionaryKey && !within(x,y)) {
            // Sliding selects another character with an 8px hysteresis band;
            // only lifting confirms it. Never slide a letter into navigation.
            const auto next=dictionary_keyboard_hit(x,y,shift_,symbols_,ready_,recent_count_,recall_,recent_enabled_,compact_study_,native_search_);
            if(next.event.action==Action::DictionaryKey)held_=next;
        }
        down_=true;last_x_=x;last_y_=y;
        if(armed_ && held_.event.action==Action::DictionaryErase && now>=repeat_at_ && within(x,y)) {
            pushed=push(held_.event);repeated_=true;repeat_at_=now+100000;
        }
        return pushed;
    }
    bool pop(Event &event) {
        if(read_==write_)return false;
        event=queue_[read_++%capacity];++consumed;return true;
    }
    bool peek(Event &event) const {if(read_==write_)return false;event=queue_[read_%capacity];return true;}
  private:
    std::array<Event,capacity> queue_{};
    uint32_t read_=0,write_=0;
    bool active_=false,barrier_=false,armed_=false,down_=false,repeated_=false,resample_=false;
    bool shift_=false,symbols_=false,ready_=false,recall_=false,compact_study_=false,native_search_=false;
    int recent_count_=0;
    unsigned recent_enabled_=3;
    KeyboardKey held_{};int last_x_=0,last_y_=0;
    uint64_t started_=0,repeat_at_=0;
    void reset_pending(){read_=write_;held_={};repeated_=false;}
    bool within(int x,int y) const {
        const auto &r=held_.rect;
        return Rect{r.x-8,r.y-8,r.w+16,r.h+16}.contains(x,y);
    }
    bool push(Event event) {
        if(pending()>=capacity){++overflow;return false;}
        queue_[write_++%capacity]=event;++accepted;
        // Mode changes and navigation close the gate until the new layout is
        // painted. Coordinates from the old layout never become new actions.
        if(!dictionary_keyboard_edit(event.action))barrier_=true;
        return true;
    }
};
} // namespace wordcard
