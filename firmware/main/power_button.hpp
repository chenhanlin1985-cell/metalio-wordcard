#pragma once
#include <cstdint>

namespace wordcard {
// E-INK4 Plus POWER is active low. Consume the power-on hold and require a
// confirmed release before accepting a new shutdown gesture. A failed I2C read
// cancels the gesture instead of extending a cached pressed level.
class PowerButton {
public:
    bool update(bool valid, bool high, uint64_t now) {
        if (!valid) {
            // Never count stale samples toward a hold or a release. Once a hold
            // is confirmed, keep it pending until a fresh, debounced release.
            armed_ = released_ = pressed_ = false;
            return false;
        }
        if (high) {
            if (!released_) { released_ = true; release_at_ = now; }
            if (now - release_at_ >= 30000) {
                pressed_ = false;
                armed_ = true;
                if (fired_) released_after_hold_ = true;
            }
            return false;
        }
        released_ = false;
        if (released_after_hold_) { fired_ = released_after_hold_ = false; }
        if (!armed_ || fired_) return false;
        if (!pressed_) { pressed_ = true; press_at_ = now; }
        if (now - press_at_ < 3000000) return false;
        fired_ = true;
        return true;
    }
    bool held() const { return fired_ && !released_after_hold_; }
    bool released_after_hold() const { return released_after_hold_; }
private:
    bool armed_ = false, released_ = false, pressed_ = false, fired_ = false;
    bool released_after_hold_ = false;
    uint64_t release_at_ = 0, press_at_ = 0;
};
}
