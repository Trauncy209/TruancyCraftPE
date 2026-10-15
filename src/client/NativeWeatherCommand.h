#ifndef TCPE_CLIENT_NATIVE_WEATHER_COMMAND_H
#define TCPE_CLIENT_NATIVE_WEATHER_COMMAND_H
#include "Minecraft.h"
#include "gui/Gui.h"
#include "../world/level/Level.h"
#include "../network/RakNetInstance.h"
#include "../network/packet/ChatPacket.h"
#include "../network/packet/WeatherPacket.h"
#include <sstream>
#include <string>

// Shared by dialog/mobile chat and the native console. Only the owner of an
// authoritative local world executes; remote clients still ask their server.
namespace NativeWeatherCommand {
struct Request { bool recognized=false,valid=false;std::string kind;int ticks=6000; };
inline Request parse(const std::string& text) {
    Request result;std::istringstream input(text);std::string command,seconds,extra;
    input>>command;if(command!="/weather")return result;result.recognized=true;
    if(!(input>>result.kind) || (result.kind!="clear"&&result.kind!="rain"&&result.kind!="thunder"))return result;
    if(input>>seconds) {
        try {size_t used=0;long value=std::stol(seconds,&used);
            if(used!=seconds.size()||value<1||value>50000)return result;
            result.ticks=int(value)*20;
        } catch(...) {return result;}
    }
    if(input>>extra)return result;result.valid=true;return result;
}
inline bool dispatch(Minecraft& minecraft,const std::string& text) {
    if(minecraft.javaSession || (minecraft.level&&minecraft.level->isJavaServerLevel()))return false;
    const Request request=parse(text);if(!request.recognized)return false;
    Level* level=minecraft.level;
    if(!level){minecraft.gui.addMessage("No level loaded.");return true;}
    if(level->isClientSide) {
        // Forward raw command text, not a decorated chat message, and never
        // alter local weather or operator permissions on a remote connection.
        if(minecraft.raknetInstance){ChatPacket packet(text);minecraft.raknetInstance->send(packet);}
        return true;
    }
    if(!request.valid || !level->getLevelData()->weather.command(request.kind.c_str(),request.ticks)) {
        minecraft.gui.addMessage("Usage: /weather clear|rain|thunder [seconds: 1-50000]");return true;
    }
    level->saveLevelData();
    if(minecraft.raknetInstance&&minecraft.raknetInstance->isServer()) {
        WeatherPacket packet(level->getLevelData()->weather);minecraft.raknetInstance->send(packet);
    }
    minecraft.gui.addMessage("World weather updated.");return true;
}
}
#endif
