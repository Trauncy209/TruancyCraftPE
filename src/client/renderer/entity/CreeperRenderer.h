#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY__CreeperRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY__CreeperRenderer_H__

//package net.minecraft.client.renderer.entity;

#include "MobRenderer.h"

#include "../../model/CreeperModel.h"
#include "../../model/Model.h"
#include "../../../world/entity/monster/Creeper.h"
#include "../../../util/Mth.h"

class CreeperRenderer: public MobRenderer
{
	typedef MobRenderer super;
public:
    CreeperRenderer()
	:	super(new CreeperModel(), 0.5f), powerArmor(2.f)
	{
    }

protected:
    void scale(Mob* mob, float a) {
        Creeper* creeper = (Creeper*) mob;

        float g = creeper->getSwelling(a);

        float wobble = 1.0f + Mth::sin(g * 100) * g * 0.01f;
        if (g < 0) g = 0;
        if (g > 1) g = 1;
        g = g * g;
        g = g * g;
        float s = (1.0f + g * 0.4f) * wobble;
        float hs = (1.0f + g * 0.1f) / wobble;
        glScalef(s, hs, s);
    }

    int getOverlayColor(Mob* mob, float br, float a) {
        Creeper* creeper = (Creeper*) mob;

        float step = creeper->getSwelling(a);

        if ((int) (step * 10) % 2 == 0) return 0;

        int _a = (int) (step * 0.2f * 255);
        if (_a < 0) _a = 0;
        if (_a > 255) _a = 255;

        int r = 255;
        int g = 255;
        int b = 255;

        return (_a << 24) | (r << 16) | (g << 8) | b;
    }

    int prepareArmor(Mob* mob,int layer,float fraction) override {
        Creeper* creeper=static_cast<Creeper*>(mob);
        if(!creeper->isPowered())return -1;
        if(layer==1) {
            bindTexture("armor/power.png");
            glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
            const float shift=(mob->tickCount+fraction)*.01f;
            glTranslatef(shift,shift,0);glMatrixMode(GL_MODELVIEW);
            glEnable2(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);
            glDisable2(GL_LIGHTING);glColor4f(.5f,.5f,.5f,1.f);
            return 1;
        }
        if(layer==2) {
            glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);
            glEnable2(GL_LIGHTING);glDisable2(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(1,1,1,1);
        }
        return -1;
    }
private:
    CreeperModel powerArmor;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_ENTITY__CreeperRenderer_H__*/
