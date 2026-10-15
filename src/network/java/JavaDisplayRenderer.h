#ifndef TCPE_JAVA_DISPLAY_RENDERER_H
#define TCPE_JAVA_DISPLAY_RENDERER_H
#include "JavaGameSession.h"
class EntityRenderDispatcher;
float javaArmorStandNameHeight(const JavaGameSession::Entity& snapshot);
void renderJavaDisplay(const JavaGameSession::Entity& snapshot,EntityRenderDispatcher& dispatcher,float x,float y,float z);
void renderJavaCustomName(const JavaGameSession::Entity& snapshot,EntityRenderDispatcher& dispatcher,float x,float y,float z,float height);
#endif
