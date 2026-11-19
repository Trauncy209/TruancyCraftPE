#ifndef TCPE_NATIVE_TOUCH_TAP_TRACKER_H
#define TCPE_NATIVE_TOUCH_TAP_TRACKER_H
#include <cstdlib>
#include "../../../../platform/input/Multitouch.h"

// A short tap can begin and end between render frames. Keep its movement
// separate from the previous camera gesture and from other control fingers.
class NativeTouchTapTracker {
    struct Press { bool down = false; int x = 0, y = 0; double time = 0; bool moved = false; };
    Press presses[Multitouch::MAX_POINTERS];
public:
    bool update(const MouseAction& event, bool inside, double now) {
        const int id = event.pointerId;
        if (id < 0 || id >= Multitouch::MAX_POINTERS) return false;
        Press& press = presses[id];
        if (event.action == MouseAction::ACTION_LEFT && event.data == MouseAction::DATA_DOWN) {
            press = Press();
            if (inside) { press.down = true; press.x = event.x; press.y = event.y; press.time = now; }
            return false;
        }
        if (!press.down) return false;
        if (std::abs(int(event.x) - press.x) + std::abs(int(event.y) - press.y) > 40)
            press.moved = true;
        if (event.action != MouseAction::ACTION_LEFT || event.data != MouseAction::DATA_UP) return false;
        const bool tap = inside && !press.moved && now >= press.time && now - press.time < .25;
        press.down = false;
        return tap;
    }
};
#endif
