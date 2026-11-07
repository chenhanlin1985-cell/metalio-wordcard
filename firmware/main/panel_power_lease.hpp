#pragma once
#include <cstdint>
namespace wordcard {
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
