#include "network/packet/WeatherLightningPacket.h"
#include <cassert>
#include <limits>
#include <iostream>
// Isolated codec test: production Packet.cpp also contains every other factory.
Packet::Packet():priority(HIGH_PRIORITY),reliability(RELIABLE){}
static WeatherLightningPacket roundTrip(WeatherLightning bolt,unsigned char phase) {
    WeatherLightningPacket out(bolt,phase),in;RakNet::BitStream wire;out.write(&wire);
    unsigned char id=0;assert(wire.Read(id));assert(id==ID_USER_PACKET_ENUM+PACKET_WEATHER_LIGHTNING);
    in.read(&wire);return in;
}
int main() {
    WeatherLightning bolt;bolt.id=123;bolt.seed=UINT64_C(0xfedcba9876543210);bolt.x=-40.25;bolt.y=70;bolt.z=1;
    auto a=roundTrip(bolt,0);assert(a.valid&&a.bolt.id==123&&a.bolt.seed==bolt.seed&&a.bolt.x==bolt.x);
    bolt.life=1;bolt.flashes=0;assert(roundTrip(bolt,1).valid);
    bolt.life=-1;assert(roundTrip(bolt,2).valid);
    assert(!roundTrip(bolt,3).valid);bolt.x=std::numeric_limits<double>::infinity();assert(!roundTrip(bolt,0).valid);
    bolt.x=0;bolt.life=3;assert(!roundTrip(bolt,0).valid);bolt.life=2;bolt.id=0;assert(!roundTrip(bolt,0).valid);
    RakNet::BitStream truncated;truncated.Write((unsigned char)1);WeatherLightningPacket partial;partial.read(&truncated);assert(!partial.valid);
    std::cout<<"lightning packet roundtrip, 64-bit seed, invalid coordinates/state/phase and truncation: PASS\n";
}
