#ifndef TCPE_JAVA_ITEM_ICONS_H
#define TCPE_JAVA_ITEM_ICONS_H
#include <string>
#include "JavaGameSession.h"
class Textures;
namespace JavaItemIcons {
// Resolves the vanilla 32-frame compass with 20 Hz heading wobble. Pack models take precedence.
std::string animatedResource(const JavaGameSession& session,const JavaGameSession::Item& item,double x,double z,double yaw,unsigned long long tick);
std::string animatedResource(const JavaGameSession& session,const JavaGameSession::Item& item,double x,double z,double yaw,unsigned long long tick,int viewerId,bool smoothHeading=true);
bool has(const std::string& resourceId);
bool isBlock(const std::string& resourceId);
// Independent GUI draw: caller must flush an active Tesselator batch first.
// False permits native TCPE translation for missing supplied resources.
// World draw preserves depth testing; caller supplies placement/orientation.
bool renderWorld(Textures* textures,const std::string& resourceId,float size=.5f,float brightness=1.f,const JavaGameSession::Item* item=nullptr);
bool render(Textures* textures,const std::string& resourceId,float x,float y,float size=16,const JavaGameSession::Item* item=nullptr);
}
#endif
