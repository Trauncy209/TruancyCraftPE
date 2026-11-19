#include "client/Minecraft.h"
#include "client/player/input/touchscreen/TouchInputHolder.h"
#include <cassert>
#include <iostream>

static void event(int action, int data, int x, int y, int pointer = 0) {
    Multitouch::feed(action, data, x, y, pointer);
    Multitouch::commit();
}
struct Game : Minecraft { Game() : Minecraft() {} };
int main() {
    Minecraft* game = new Game();
    game->width = 800; game->height = 600;
    CustomInputHolder holder(NULL, NULL, NULL);
    UnifiedTurnBuild touch(UnifiedTurnBuild::MODE_DELTA, 800, 600, 1000, 1, &holder, game);
    BuildActionIntention action;
    // A prior camera drag must not poison a fresh tap that occurs entirely
    // between two rendered frames (common when tapping mobs rapidly).
    event(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN, 100, 100);
    touch.getTurnDelta(); touch.tickBuild(NULL, &action); Multitouch::reset();
    event(MouseAction::ACTION_MOVE, 0, 300, 100);
    touch.getTurnDelta(); assert(!touch.tickBuild(NULL, &action)); Multitouch::reset();
    event(MouseAction::ACTION_LEFT, MouseAction::DATA_UP, 300, 100);
    touch.getTurnDelta(); assert(!touch.tickBuild(NULL, &action)); Multitouch::reset();
    for (int i = 0; i < 5; ++i) {
        event(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN, 300, 100);
        event(MouseAction::ACTION_LEFT, MouseAction::DATA_UP, 300, 100);
        touch.getTurnDelta();
        assert(touch.tickBuild(NULL, &action));
        assert(action.isAttack());
        Multitouch::reset();
    }
    // A fast swipe, including one that returns to its starting point, is not
    // a tap. Neither is a D-pad press that slides into the world area.
    event(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN, 300, 100);
    event(MouseAction::ACTION_MOVE, 0, 400, 100);
    event(MouseAction::ACTION_LEFT, MouseAction::DATA_UP, 300, 100);
    touch.getTurnDelta(); assert(!touch.tickBuild(NULL, &action)); Multitouch::reset();
    NativeTouchTapTracker tracker;
    assert(!tracker.update(MouseAction(1, 1, 100, 100, 1), false, 0));
    assert(!tracker.update(MouseAction(1, 0, 300, 100, 1), true, .1));
    assert(!tracker.update(MouseAction(1, 1, 100, 100, 0), true, 0));
    assert(!tracker.update(MouseAction(1, 0, 100, 100, 0), true, .5));
    std::cout << "PASS: actual native touch taps after camera drag, rapid taps, swipe rejection, control-origin and held gestures\n";
}
