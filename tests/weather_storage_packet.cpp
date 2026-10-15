#include "world/level/storage/LevelData.h"
#include "network/packet/WeatherPacket.h"
#include <cassert>
#include <iostream>
static WeatherPacket roundTrip(const WeatherState& state){
 WeatherPacket out(state),in;RakNet::BitStream stream;out.write(&stream);unsigned char id=0;assert(stream.Read(id));assert(id==ID_USER_PACKET_ENUM+PACKET_WEATHER);in.read(&stream);return in;
}
int main(){
 LevelData data(LevelSettings(1234,GameType::Survival,LGV_ORIGINAL),"weather-test",LGV_ORIGINAL);
 assert(data.weather.command("thunder",9876));data.weather.rain=.45f;data.weather.thunder=.7f;
 CompoundTag* tag=data.createTag();LevelData loaded(tag);assert(loaded.weather.raining&&loaded.weather.thundering&&loaded.weather.rainTicks==9876&&loaded.weather.thunderTicks==9876);assert(loaded.weather.rain==1&&loaded.weather.thunder==1);
 LevelData copy=data;LevelData assigned;assigned=data;assert(copy.weather.rain==.45f&&assigned.weather.thunder==.7f);
 auto packet=roundTrip(data.weather);assert(packet.valid&&packet.state.rain==.45f&&packet.state.thunder==.7f&&packet.state.rainTicks==9876);
 auto bad=data.weather;bad.rainTicks=-1;assert(!roundTrip(bad).valid);bad=data.weather;bad.rain=2;assert(!roundTrip(bad).valid);
 RakNet::BitStream truncated;truncated.Write((unsigned char)1);WeatherPacket partial;partial.read(&truncated);assert(!partial.valid);
 CompoundTag legacy;LevelData old(&legacy);assert(!old.weather.raining&&old.weather.rain==0&&old.weather.rainTicks==0);
 std::cout<<"Weather NBT persistence, legacy defaults, copies, snapshot roundtrip and malformed/truncated packet rejection PASS\n";
 // LevelData takes no ownership of the input root tag.
 delete tag;
}
