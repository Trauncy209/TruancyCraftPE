#ifndef TCPE_NETWORK_WEATHER_PACKET_H
#define TCPE_NETWORK_WEATHER_PACKET_H
#include "../Packet.h"
#include "../../world/level/WeatherState.h"
// Server-to-client snapshot. Clients never own target state or scheduling.
class WeatherPacket:public Packet {
public:
    WeatherState state;bool valid=false;
    WeatherPacket(){reliability=RELIABLE_ORDERED;}
    explicit WeatherPacket(const WeatherState& value):state(value),valid(true){reliability=RELIABLE_ORDERED;}
    void write(RakNet::BitStream* stream)override {
        stream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM+PACKET_WEATHER));
        stream->Write((unsigned char)1);stream->Write((unsigned char)((state.raining?1:0)|(state.thundering?2:0)));
        stream->Write(state.rainTicks);stream->Write(state.thunderTicks);stream->Write(state.rain);stream->Write(state.thunder);
    }
    void read(RakNet::BitStream* stream)override {
        unsigned char version=0,flags=0;WeatherState incoming;
        valid=stream->Read(version)&&stream->Read(flags)&&stream->Read(incoming.rainTicks)&&stream->Read(incoming.thunderTicks)&&stream->Read(incoming.rain)&&stream->Read(incoming.thunder);
        valid=valid&&version==1&&!(flags&~3)&&incoming.rainTicks>=0&&incoming.thunderTicks>=0&&std::isfinite(incoming.rain)&&std::isfinite(incoming.thunder)&&incoming.rain>=0&&incoming.rain<=1&&incoming.thunder>=0&&incoming.thunder<=1;
        if(valid){incoming.raining=(flags&1)!=0;incoming.thundering=(flags&2)!=0;incoming.previousRain=incoming.rain;incoming.previousThunder=incoming.thunder;state=incoming;}
    }
    void handle(const RakNet::RakNetGUID& source,NetEventCallback* callback)override {callback->handle(source,this);}
};
#endif
