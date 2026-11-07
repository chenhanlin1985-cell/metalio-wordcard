#include "dictionary_keyboard.hpp"
#include "keyboard_fast_refresh.hpp"
#include "web_keyboard.hpp"
#include <iostream>
#include <set>
#include <vector>
#include <stdexcept>
using namespace wordcard;
static unsigned checks=0;
static void check(bool yes,const char *why){++checks;if(!yes)throw std::runtime_error(why);}
int main(){try {
    const std::string letters="qwertyuiopasdfghjklzxcvbnm";
    for(bool symbols:{false,true})for(bool shift:{false,true}) {
        std::set<int> values;
        for(int slot=0;slot<30;++slot) {
            const auto key=dictionary_keyboard_key(slot,shift,symbols);const auto r=key.rect,cell=dictionary_keyboard_cell(slot);
            check(r.w>=64&&r.h==84,"enlarged portrait key size");
            check(r.x>=0&&r.y>=0&&r.x+r.w<=684&&r.y+r.h<=1216,"portrait bounds");
            for(int dx:{0,cell.w/2,cell.w-1})for(int dy:{0,cell.h/2,cell.h-1}){
                const auto hit=dictionary_keyboard_hit(cell.x+dx,cell.y+dy,shift,symbols);
                check(hit.event.action==key.event.action&&hit.event.value==key.event.value,"expanded cell belongs to exactly one key");
                const auto compact=dictionary_keyboard_hit(cell.x+dx,cell.y+dy,shift,symbols,true,1,true,1,true);
                check(compact.event.action==hit.event.action&&compact.event.value==hit.event.value,"compact study preserves every letter cell and mode key");
            }
            if(key.event.action==Action::DictionaryKey&&key.event.value!=' ')values.insert(key.event.value);
            if(slot<26&&!symbols)check(key.event.value==letters[slot]-(shift?32:0),"familiar QWERTY order");
        }
        check(values.size()==(symbols?27u:26u),"complete letters and technology symbols");
        if(!symbols)for(int c=shift?'A':'a';c<=(shift?'Z':'z');++c)check(values.count(c),"alphabet coverage");
        else for(char c:std::string("1234567890-'./@+_():?!&=,%#"))check(values.count(c),"punctuation coverage");
    }
    check(dictionary_keyboard_hit(685,1134,0,0).event.action==Action::None,"outside right");
    check(dictionary_keyboard_hit(20,1184,0,0).event.action==Action::None,"outside bottom");
    check(dictionary_keyboard_hit(600,1134,0,0,false).event.action==Action::None,"unavailable dictionary disables search");
    check(dictionary_keyboard_hit(300,440,0,0,true,0).event.action==Action::None,"empty history has no invisible action");
    check(dictionary_keyboard_hit(300,440,0,0,true,1).event.action==Action::DictionaryRecent,"visible recent query opens from its actual row");
    check(dictionary_keyboard_hit(80,100,0,0).event.action==Action::Back,"enlarged keyboard back target");
    check(dictionary_keyboard_hit(560,100,0,0).event.action==Action::Vocabulary,"enlarged keyboard secondary target");
    check(dictionary_keyboard_hit(340,730,0,0,true,0,true).event.action==Action::RecallSkip,"enlarged keyboard recall skip target");
    for(int x:{24,180,335})for(int y:{650,694,737}){
        const auto audio=dictionary_keyboard_hit(x,y,false,false,true,1,true,1,true);
        check(audio.event.action==Action::DictionaryRecent&&audio.event.value==0,"native compact audio owns its complete left button");
    }
    for(int x:{348,504,659})for(int y:{650,694,737})
        check(dictionary_keyboard_hit(x,y,false,false,true,1,true,1,true).event.action==Action::RecallSkip,"native compact unknown owns its complete right button");
    for(int x:{24,180,340,504,659})for(int y:{398,440,485,506,550,593})
        check(dictionary_keyboard_hit(x,y,false,false,true,3,true,7,true).event.action==Action::None,"old native help rows never remain active");
    check(dictionary_keyboard_hit(340,694,false,false,true,3,true,7,true).event.action==Action::None,"compact toolbar gutter has no stale full width skip target");
    check(dictionary_keyboard_hit(180,694,false,false,true,1,true,0,true).event.action==Action::None,"disabled native audio has no invisible target");
    check(dictionary_keyboard_hit(180,694,false,false,true,0,true,1,true).event.action==Action::None,"missing native audio has no invisible target");
    check(dictionary_keyboard_hit(180,60,false,false,true,1,true,1,true).event.action==Action::Back,"compact contextual back label has full capture width");
    check(dictionary_keyboard_hit(180,60,false,false).event.action==Action::None,"legacy back capture width unchanged");
    check(dictionary_keyboard_hit(180,694,false,false,true,2,true,3).event.action==Action::RecallSkip,"legacy recall skip remains full width");
    check(dictionary_keyboard_hit(180,550,false,false,true,2,true,3).event.value==1,"legacy second help row unchanged");
    check(dictionary_keyboard_hit(180,694,false,false,true,3,false,7).event.value==2,"dictionary third candidate unchanged");
    WebPage nativePage;nativePage.key="vocabulary:spelling:fixture";nativePage.route="word-batch";nativePage.keyboard=true;nativePage.can_submit=true;
    nativePage.blocks={{"button","audio","",21,true},{"button","old hint","",22,true},{"button","unknown","foot",23,true}};
    auto nativeState=web_keyboard_state(nativePage);
    check(nativeState.compactStudy&&nativeState.recall&&nativeState.ready&&nativeState.count==1&&nativeState.enabled==1,"only native vocabulary spelling selects compact one-control layout");
    nativePage.blocks[0].enabled=false;nativeState=web_keyboard_state(nativePage);
    check(nativeState.count==1&&nativeState.enabled==0,"native compact audio availability follows its actual control");
    nativePage.blocks[0].enabled=true;nativePage.key="legacy:spelling";nativeState=web_keyboard_state(nativePage);
    check(!nativeState.compactStudy&&nativeState.recall&&nativeState.count==2&&nativeState.enabled==3,"legacy study keeps both help rows");
    nativePage.key="vocabulary:lookup";nativePage.route="lookup";nativeState=web_keyboard_state(nativePage);
    check(!nativeState.compactStudy&&!nativeState.recall&&nativeState.count==2,"native dictionary search keeps legacy keyboard layout");
    nativePage.route="word-batch";nativePage.keyboard=false;
    check(!web_keyboard_state(nativePage).compactStudy,"non-keyboard vocabulary page cannot select compact capture");
    nativePage.keyboard=true;nativePage.route="search";nativePage.key="vocabulary:search:fixture";nativeState=web_keyboard_state(nativePage);
    check(nativeState.nativeSearch&&!nativeState.compactStudy&&!nativeState.recall,"native search selects its header capture layout only");
    nativePage.key="legacy:search";check(!web_keyboard_state(nativePage).nativeSearch,"legacy search keeps original header capture");
    nativePage.key="vocabulary:search:fixture";nativePage.keyboard=false;
    check(!web_keyboard_state(nativePage).nativeSearch,"non-keyboard search cannot select native header capture");
    for(int y:{12,60,107}){
        for(int x:{24,120,207})check(dictionary_keyboard_hit(x,y,false,false,true,2,false,3,false,true).event.action==Action::Back,"native search captures full contextual return button");
        for(int x:{224,338,451})check(dictionary_keyboard_hit(x,y,false,false,true,2,false,3,false,true).event.action==Action::Dictionary,"native search captures full complete-dictionary button");
        for(int x:{476,568,659})check(dictionary_keyboard_hit(x,y,false,false,true,2,false,3,false,true).event.action==Action::Vocabulary,"native search notebook capture matches renderer");
    }
    for(int x:{208,220,452,464})
        check(dictionary_keyboard_hit(x,60,false,false,true,2,false,3,false,true).event.action==Action::None,"native search header gutters have no hidden control");
    check(dictionary_keyboard_hit(338,60,false,false,true,2,false,3).event.action==Action::None,"legacy search center remains title with no dictionary action");
    check(dictionary_keyboard_hit(338,60,false,false,true,1,true,1,true).event.action==Action::None,"native study center remains title with no dictionary action");
    check(dictionary_keyboard_hit(568,60,false,false,true,1,true,1,true).event.action==Action::Vocabulary,"native study lookup tool uses existing vocabulary dispatch");
    for(int row:{0,1}){
        const auto candidate=dictionary_keyboard_hit(338,440+row*108,false,false,true,2,false,3,false,true);
        check(candidate.event.action==Action::DictionaryRecent&&candidate.event.value==row,"native search retains candidate row capture");
    }
    // Recorded physical A attempts (fix39b trace at device t=582074..583876).
    for(auto p:{std::pair<int,int>{40,918},{9,927},{0,934},{0,946},{0,942},{76,937}})
        check(dictionary_keyboard_hit(p.first,p.second,0,0).event.value=='a',"recorded edge A X positions at the new A row have no dead zone");
    check(dictionary_keyboard_hit(683,934,0,0).event.value=='l',"right staggered-row margin belongs to L");
    KeyboardInput k;uint64_t now=1000000;Event e;
    auto tap=[&](int slot){const auto r=dictionary_keyboard_cell(slot);
        k.sample(true,r.x+r.w/2,r.y+r.h/2,now);now+=50000;k.sample(false,0,0,now);now+=50000;};
    auto activate=[&](bool shift,bool symbols,bool ready){k.publish(shift,symbols,ready);k.sample(false,0,0,now);};
    activate(0,0,false);for(int i=0;i<26;++i)tap(i);activate(0,0,true);
    for(int i=0;i<26;++i){check(k.pop(e),"queue survives redraw");check(e.value==letters[i],"FIFO order");}check(!k.pop(e),"one per tap");
    tap(0);tap(1);k.sample(true,600,1134,now);now+=50000;k.sample(false,0,0,now);now+=50000;tap(2);activate(0,0,true);
    check(k.pop(e)&&e.value=='q',"first letter before query retained");check(k.pop(e)&&e.value=='w',"second letter retained");
    check(k.pop(e)&&e.action==Action::DictionarySearch,"query retained during repaint");check(!k.pop(e)&&!k.active(),"query barrier");
    k.deactivate();activate(0,0,true);
    for(int duration:{11000,23000}){k.sample(true,70,896,now);now+=duration;k.sample(false,0,0,now);check(k.pop(e)&&e.value=='a',"recorded short A tap accepted");now+=100000;}
    k.sample(true,36,834,now);now+=20000;k.sample(false,0,0,now);check(k.pop(e)&&e.value=='q',"valid 20ms hardware tap accepted");now+=50000;
    k.sample(true,36,834,now);now+=50000;k.sample(true,38,838,now);now+=50000;k.sample(false,0,0,now);
    check(k.pop(e)&&e.value=='q'&&!k.pop(e),"finger roll one key");now+=50000;
    k.sample(true,36,834,now);now+=50000;k.sample(true,106,834,now);now+=50000;k.sample(false,0,0,now);
    check(k.pop(e)&&e.value=='w',"slide to adjacent key then release commits final selection");now+=50000;
    k.sample(true,639,1034,now);k.sample(true,639,1034,now+499999);check(!k.pop(e),"erase repeat delay");
    k.sample(true,639,1034,now+500000);check(k.pop(e)&&e.action==Action::DictionaryErase,"hold erase first");
    k.sample(true,639,1034,now+599999);check(!k.pop(e),"repeat interval");k.sample(true,639,1034,now+600000);
    check(k.pop(e)&&e.action==Action::DictionaryErase,"hold erase repeat");k.sample(false,0,0,now+650000);check(!k.pop(e),"no extra release delete");now+=1000000;
    tap(26);check(k.pop(e)&&e.action==Action::DictionaryShift,"shift barrier");tap(0);check(!k.pop(e),"old layout gated");
    k.deactivate();activate(true,false,true);tap(0);check(k.pop(e)&&e.value=='Q',"shifted input");
    k.sample(true,36,834,now);k.deactivate();k.publish(0,1,true);now+=100000;k.sample(false,0,0,now);
    check(!k.pop(e),"carried finger ignored");tap(0);check(k.pop(e)&&e.value=='1',"symbol after transition");
    // A release can happen while the BSP pauses samples for a full page.
    k.sample(true,36,834,now);k.deactivate();k.publish(0,0,true);now+=100000;
    k.sample(false,0,0,now);tap(10);check(k.pop(e)&&e.value=='a',"fresh controller UP resync preserves first tap after a mode/page refresh");
    k.deactivate();activate(0,0,true);for(int i=0;i<65;++i)tap(i%26);check(k.pending()==64&&k.overflow==1,"bounded overflow");
    for(int i=0;i<64;++i)check(k.pop(e)&&e.value==letters[i%26],"overflow retains order");k.deactivate();check(!k.active()&&!k.pending(),"leave clears pending");
    // Late arrivals while the UI commits A: B,C + submit close the gate, but
    // the unchanged-layout paint and periodic GC must retain that accepted FIFO.
    for(bool cleanup:{false,true}){
        k.deactivate();activate(false,false,true);tap(10);check(k.pop(e)&&e.value=='a',"first edit committing");
        tap(23);tap(21);k.sample(true,600,1130,now);k.sample(false,600,1130,now+1000);now+=2000;
        check(!k.active()&&k.same_layout(false,false),"barrier stops new taps without invalidating painted geometry");
        check(k.pending()==3,"late B,C,submit accepted");
        if(cleanup)k.pause_capture();
        check(k.publish(false,false,true)==cleanup,"same-layout cleanup requests a fresh sample only");
        check(k.pop(e)&&e.value=='b',"B survives paint");check(k.pop(e)&&e.value=='c',"C survives paint");
        check(k.pop(e)&&e.action==Action::DictionarySearch,"submit survives and follows letters");
        check(!k.pending(),"barrier consumed exactly once");
    }
    // Exercise controller DOWN/hold/UP, including layout changes while a key is held.
    auto compactActivate=[&](){k.deactivate();k.publish(false,false,true,1,true,1,true);k.sample(false,0,0,now);};
    for(int x:{180,504}){
        compactActivate();k.sample(true,x,694,now);k.sample(true,x,694,now+600000);
        check(!k.pending(),"compact toolbar hold never submits before release");
        k.sample(false,x,694,now+650000);now+=700000;
        check(k.pop(e)&&e.action==(x==180?Action::DictionaryRecent:Action::RecallSkip)&&e.value==0,"compact toolbar releases audio and unknown as distinct actions");
        check(!k.pop(e)&&!k.active(),"compact toolbar queues once and gates later taps");
        check(!k.publish(false,false,true,1,true,1,true)&&!k.active(),"same compact repaint preserves toolbar barrier");
    }
    compactActivate();k.sample(true,180,694,now);k.sample(true,504,694,now+10000);k.sample(false,504,694,now+20000);now+=30000;
    check(!k.pop(e),"sliding audio into unknown cancels rather than changes action");
    for(auto p:{std::pair<int,int>{180,440},{504,550},{340,694}}){
        k.sample(true,p.first,p.second,now);k.sample(false,p.first,p.second,now+10000);now+=20000;
        check(!k.pop(e),"captured taps on stale rows or toolbar gutter produce no action");
    }
    for(bool cleanup:{false,true}){
        compactActivate();tap(0);tap(1);k.sample(true,600,1130,now);k.sample(false,600,1130,now+10000);now+=20000;
        if(cleanup)k.pause_capture();
        check(k.publish(false,false,true,1,true,1,true)==cleanup,"compact unchanged-layout publish preserves queued input across cleanup");
        check(k.pop(e)&&e.value=='q',"compact first queued letter survives repaint");
        check(k.pop(e)&&e.value=='w',"compact second queued letter survives repaint");
        check(k.pop(e)&&e.action==Action::DictionarySearch&&!k.pop(e)&&!k.active(),"compact queued submit follows letters and retains barrier");
    }
    compactActivate();tap(0);k.sample(true,180,694,now);
    check(k.publish(false,false,true,2,true,3,false),"leaving compact layout requests controller resampling");
    k.sample(false,180,694,now+10000);now+=20000;
    check(!k.pop(e),"leaving compact layout discards queued old letter and carried toolbar finger");
    k.sample(true,180,694,now);k.sample(false,180,694,now+10000);now+=20000;
    check(k.pop(e)&&e.action==Action::RecallSkip,"fresh legacy toolbar tap uses legacy geometry after transition");
    check(k.publish(false,false,true,1,true,1,true),"entering compact layout resets legacy navigation barrier");
    k.sample(false,0,0,now);tap(0);check(k.pop(e)&&e.value=='q',"fresh compact letter works after layout transition");
    k.sample(true,36,834,now);check(k.publish(true,false,true,1,true,1,true),"compact shift changes layout");
    k.sample(false,36,834,now+10000);now+=20000;check(!k.pop(e),"compact mode switch discards held old-mode letter");
    tap(0);check(k.pop(e)&&e.value=='Q',"compact shifted letter keeps existing key behavior");
    for(auto target:{std::pair<int,Action>{180,Action::Back},{338,Action::Dictionary},{568,Action::Vocabulary}}){
        k.deactivate();k.publish(false,false,true,2,false,3,false,true);k.sample(false,0,0,now);
        k.sample(true,target.first,60,now);k.sample(true,target.first,60,now+600000);
        check(!k.pending(),"native search header hold waits for controller release");
        k.sample(false,target.first,60,now+650000);now+=700000;
        check(k.pop(e)&&e.action==target.second&&!k.pop(e)&&!k.active(),"native search physical header release dispatches exactly the rendered action");
        check(!k.publish(false,false,true,2,false,3,false,true)&&!k.active(),"native search same-layout publish retains navigation barrier");
    }
    k.deactivate();k.publish(false,false,true,2,false,3,false,true);k.sample(false,0,0,now);tap(0);k.sample(true,338,60,now);
    check(k.publish(false,false,true,2,false,3,false,false),"leaving native search resets capture even with same letter modes");
    k.sample(false,338,60,now+10000);now+=20000;
    check(!k.pop(e),"native search transition discards stale queued letter and held dictionary action");
    k.sample(true,338,60,now);k.sample(false,338,60,now+10000);now+=20000;
    check(!k.pop(e),"fresh legacy center tap is inactive after native search transition");
    check(k.publish(false,false,true,2,false,3,false,true),"entering native search requests fresh controller sample");
    k.sample(false,0,0,now);tap(0);check(k.pop(e)&&e.value=='q',"native search preserves ordinary keyboard input");
    std::vector<uint8_t> front(Canvas::bytes,255),back=front;
    auto pixel=[&](std::vector<uint8_t>&fb,int x,int y,int v){const int p=(683-x)*1216+y;fb[p/2]=uint8_t((fb[p/2]&~(15<<((p&1)*4)))|(v<<((p&1)*4)));};
    check(!keyboard_fast_refresh_safe(front.data(),back.data(),front.size()),"identical frame needs no DU");
    for(auto pos: {std::pair<int,int>{12,124},{671,353},{100,180}}){front=back;pixel(front,pos.first,pos.second,0);check(keyboard_fast_refresh_safe(front.data(),back.data(),front.size()),"black insertion allowed");check(keyboard_fast_refresh_safe(back.data(),front.data(),front.size()),"white erase allowed");}
    for(int gray=1;gray<15;++gray){front=back;pixel(front,100,180,gray);check(!keyboard_fast_refresh_safe(front.data(),back.data(),front.size()),"gray destination falls back");check(!keyboard_fast_refresh_safe(back.data(),front.data(),front.size()),"gray source falls back");}
    for(auto pos:{std::pair<int,int>{11,180},{672,180},{100,123},{100,354},{36,834},{600,1134}}){front=back;pixel(front,pos.first,pos.second,0);check(!keyboard_fast_refresh_safe(front.data(),back.data(),front.size()),"change outside input strip falls back");}
    check(!keyboard_fast_refresh_safe(front.data(),back.data(),0),"wrong framebuffer rejected");
    front=back;pixel(front,600,1134,0);check(keyboard_fast_refresh_safe(front.data(),back.data(),front.size(),true),"only explicit web submit may join binary input strip");
    pixel(front,600,1134,7);check(!keyboard_fast_refresh_safe(front.data(),back.data(),front.size(),true),"web submit still rejects gray pixels");
    std::cout<<"PASS: "<<checks<<" keyboard checks: QWERTY geometry, FIFO and transitions, erase repeat, binary input-only DU guard.\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}


