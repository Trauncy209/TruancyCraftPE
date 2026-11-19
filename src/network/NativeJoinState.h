#ifndef TCPE_NATIVE_JOIN_STATE_H
#define TCPE_NATIVE_JOIN_STATE_H

// Keep connection timing independent of frame rate and rendering stalls.
// A connected dedicated server may need time to load the player's spawn chunks.
class NativeJoinState {
public:
    enum Phase { Idle, Connecting, AwaitingWorld, WorldStarted, Ready, Cancelled };
    NativeJoinState() : phase(Idle), phaseStarted(0) {}
    void begin(double now) { phase = Connecting; phaseStarted = now; }
    bool connected(double now) {
        if (phase != Connecting) return false;
        phase = AwaitingWorld;
        phaseStarted = now;
        return true;
    }
    bool active() const { return phase == Connecting || phase == AwaitingWorld || phase == WorldStarted || phase == Ready; }
    bool acceptsWorld() const { return phase == AwaitingWorld; }
    void worldStarted(double now) { phase = WorldStarted; phaseStarted = now; }
    void worldReady() { if (phase == WorldStarted) phase = Ready; }
    void cancel() { phase = Cancelled; }
    bool expired(double now) const {
        return (phase == Connecting && now - phaseStarted >= 30.0) ||
               ((phase == AwaitingWorld || phase == WorldStarted) && now - phaseStarted >= 120.0);
    }
private:
    Phase phase;
    double phaseStarted;
};
#endif
