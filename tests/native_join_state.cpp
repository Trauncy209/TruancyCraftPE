#include "../src/network/NativeJoinState.h"
#include <cassert>
#include <iostream>

int main() {
    NativeJoinState join;
    assert(!join.expired(1000000)); // Browsing servers is not a join attempt.
    assert(!join.connected(0));
    join.begin(100);
    assert(!join.expired(129.99));
    assert(join.expired(130));
    join.begin(200);
    assert(join.connected(205));
    assert(!join.connected(206)); // Repeated events cannot reset the deadline.
    assert(!join.expired(235)); // Original live trace delivered StartGame after 30s.
    assert(!join.expired(324.99));
    assert(join.expired(325)); // An unresponsive connected server is still bounded.
    join.cancel();
    assert(!join.acceptsWorld());
    assert(!join.connected(330)); // Late connect cannot revive a cancelled attempt.
    assert(!join.expired(1000000));
    join.begin(400);
    assert(join.connected(401));
    assert(join.acceptsWorld());
    join.worldStarted(402);
    assert(!join.acceptsWorld()); // Duplicate StartGame must not replace the world.
    assert(!join.expired(521.99));
    assert(join.expired(522));
    join.worldReady();
    assert(!join.expired(1000000));
    join.cancel();
    assert(!join.acceptsWorld());
    std::cout << "Native join deadlines, late/duplicate packet gates, cancellation and retry PASS\n";
}
