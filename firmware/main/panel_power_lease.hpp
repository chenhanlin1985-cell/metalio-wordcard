#pragma once
#include <cstdint>
#include <string_view>
namespace wordcard {
// Retain warm rails only during an actual learning/input burst. Static native
// destinations use the factory refresh -> poweroff lifecycle; no delayed rail
// transition is scheduled several seconds after a settled home/album page.
inline bool panel_static_native_page(std::string_view key) {
    constexpr std::string_view prefix = "vocabulary:";
    if(key.substr(0,prefix.size())!=prefix)return false;
    auto rest=key.substr(prefix.size());auto route=rest.substr(0,rest.find(':'));
    return route!="word-batch" && route!="search";
}
// Main display task only. Holding rails never implies leaving MODE/OE enabled.
class PanelPowerLease {
    uint64_t until_ = 0;
public:
    static constexpr uint64_t burst_us = 4000000;
    bool held() const { return until_ != 0; }
    bool expired(uint64_t now) const { return held() && now >= until_; }
    void completed(uint64_t now, bool success, bool eligible) {
        until_ = success && eligible ? now + burst_us : 0;
    }
    void release() { until_ = 0; }
};
}
