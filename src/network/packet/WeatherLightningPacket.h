#ifndef TCPE_NETWORK_WEATHER_LIGHTNING_PACKET_H
#define TCPE_NETWORK_WEATHER_LIGHTNING_PACKET_H
#include "../Packet.h"
#include "../../world/level/WeatherLightning.h"
class WeatherLightningPacket:public Packet {
public:
    WeatherLightning bolt;
    unsigned char phase=0;
    bool valid=false;
    WeatherLightningPacket(){reliability=RELIABLE_ORDERED;}
    WeatherLightningPacket(const WeatherLightning& value,unsigned char kind):bolt(value),phase(kind),valid(value.valid()&&kind<=2){reliability=RELIABLE_ORDERED;}
    void write(RakNet::BitStream* out) override {
        out->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM+PACKET_WEATHER_LIGHTNING));
        out->Write((unsigned char)1);out->Write(phase);
        out->Write(bolt.id);out->Write(bolt.seed);
        out->Write(bolt.x);out->Write(bolt.y);out->Write(bolt.z);
        out->Write(bolt.life);out->Write(bolt.flashes);
    }
    void read(RakNet::BitStream* in) override {
        unsigned char version=0,p=0;WeatherLightning incoming;
        valid=in->Read(version)&&in->Read(p)&&in->Read(incoming.id)&&in->Read(incoming.seed)&&in->Read(incoming.x)&&in->Read(incoming.y)&&in->Read(incoming.z)&&in->Read(incoming.life)&&in->Read(incoming.flashes);
        valid=valid&&version==1&&p<=2&&incoming.valid();
        if(valid){bolt=incoming;phase=p;}
    }
    void handle(const RakNet::RakNetGUID& source,NetEventCallback* callback) override {callback->handle(source,this);}
};
#endif
