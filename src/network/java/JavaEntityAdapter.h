#ifndef TCPE_JAVA_ENTITY_ADAPTER_H
#define TCPE_JAVA_ENTITY_ADAPTER_H
class Level;
class Entity;
class EntityRenderDispatcher;
#include "JavaGameSession.h"
const JavaGameSession::Item* javaCarriedItem(Entity* entity,bool offHand=false);
// Level owns all presentation entities. Call on the game thread after pumping
// snapshots and before rendering; no pointers retained across world destruction.
const JavaGameSession::Entity* javaPresentationSnapshot(Entity* entity);
unsigned javaPlayerSkinLayers(Entity* entity);
bool javaPlayerSkinModel(Entity* entity,bool& slim);
bool renderJavaPresentation(Entity* entity,EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial=0.f);
void synchronizeJavaEntities(Level* level,const JavaGameSession& session);
#endif
