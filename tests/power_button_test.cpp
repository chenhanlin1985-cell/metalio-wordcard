#include "power_button.hpp"
#include <cassert>
#include <cstdio>

int main() {
    wordcard::PowerButton p;
    // Holding POWER to turn on, then releasing, must never shut the device down.
    for (uint64_t t=0;t<10000000;t+=20000) assert(!p.update(true,false,t));
    for (uint64_t t=10000000;t<20000000;t+=20000) assert(!p.update(true,true,t));
    assert(!p.update(true,false,20000000));
    assert(!p.update(true,false,22999999));
    assert(p.update(true,false,23000000));
    assert(p.held() && !p.released_after_hold());
    assert(!p.update(true,false,30000000)); // Once per gesture.
    assert(!p.update(true,true,30000001));
    assert(!p.update(true,false,30010001)); // Short release bounce does not rearm.
    assert(!p.update(true,false,34000000));
    assert(!p.released_after_hold()); // Still physically held; must not cut power.
    assert(!p.update(true,true,34000001));
    assert(!p.update(true,true,34030001));
    assert(!p.held() && p.released_after_hold());
    assert(!p.update(true,false,35000000));
    assert(p.update(true,false,38000000)); // Deliberate second long press.

    wordcard::PowerButton q;
    assert(!q.update(true,true,0));
    assert(!q.update(true,true,30000));
    assert(!q.update(true,false,40000));
    assert(!q.update(false,false,2000000)); // I2C error cancels the held key.
    assert(!q.update(true,false,5000000));
    assert(!q.update(true,false,9000000));
    assert(!q.update(true,true,10000000));
    assert(!q.update(true,true,10030000));
    assert(!q.update(true,false,11000000));
    assert(!q.update(true,true,13000000)); // Short presses never power off.
    assert(!q.update(true,true,13030000));
    assert(!q.update(true,false,14000000));
    assert(q.update(true,false,17000000));
    assert(!q.update(false,true,18000000));
    assert(!q.released_after_hold()); // A read failure cannot authorize power cut.
    assert(!q.update(true,true,18020000));
    assert(!q.released_after_hold());
    assert(!q.update(true,true,18050000));
    assert(q.released_after_hold());

    wordcard::PowerButton bounce;
    assert(!bounce.update(true,true,0));
    assert(!bounce.update(true,true,30000));
    assert(!bounce.update(true,false,40000));
    assert(!bounce.update(true,true,2000000));
    assert(!bounce.update(true,false,2020000)); // 20ms contact noise does not erase hold.
    assert(bounce.update(true,false,3040000));
    for(uint64_t t=3060000;t<15000000;t+=20000){
        assert(!bounce.update(true,false,t));
        assert(!bounce.released_after_hold());
    }
    assert(!bounce.update(true,true,15000000));
    assert(!bounce.update(true,true,15029999));
    assert(!bounce.released_after_hold());
    assert(!bounce.update(true,true,15030000));
    assert(bounce.released_after_hold());
    puts("PASS: power key boot hold, valid release, I2C failures, bounce, one-shot 3-second hold and release-before-cut");
}
