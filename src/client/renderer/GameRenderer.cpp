#include "network/java/JavaGameSession.h"
#include "GameRenderer.h"
#include "WeatherLightningRenderer.h"
#include "WeatherPrecipitationPolicy.h"
#include "gles.h"

#include "../../util/PerfTimer.h"

#include "LevelRenderer.h"
#include "ItemInHandRenderer.h"
#include "culling/AllowAllCuller.h"
#include "culling/FrustumCuller.h"
#include "entity/EntityRenderDispatcher.h"
#include "../Minecraft.h"
#include "../gamemode/GameMode.h"
#include "../particle/ParticleEngine.h"
#include "../player/LocalPlayer.h"
#include "../gui/Screen.h"
#include "../../world/level/Level.h"
#include "../../world/level/biome/Biome.h"
#include "../../world/level/dimension/Dimension.h"
#include <cmath>
#include <cstdint>
#include "../../world/entity/Mob.h"
#include "../../world/level/chunk/ChunkCache.h"
#include "../../world/level/material/Material.h"
#include "../../world/Facing.h"
#include "../../platform/input/Controller.h"
#include "../../platform/input/Mouse.h"
#include "../../platform/input/Multitouch.h"
#include "../../NinecraftApp.h"
#include "../../world/level/tile/Tile.h"
#include "../player/input/IInputHolder.h"
#include "../player/input/touchscreen/TouchInputHolder.h"
#include "Textures.h"
#include "../gui/components/ImageButton.h"
#include "Tesselator.h"

#if defined(TCPE_GLES3) && defined(PLATFORM_DESKTOP)
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned qaWorldFrame=0;
bool qaWorldEnabled(){static const bool enabled=std::getenv("TCPE_GLES3_QA_STATE")!=nullptr;return enabled;}
void qaWorldBeforeClear(){
 if(!qaWorldEnabled())return;++qaWorldFrame;
 if(qaWorldFrame>8&&qaWorldFrame!=300)return;
 GLboolean mask=0,color[4]{};GLfloat clear=0;GLint scissor[4]{},fbo=0,depth=0;
 glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetBooleanv(GL_COLOR_WRITEMASK,color);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clear);glGetIntegerv(GL_SCISSOR_BOX,scissor);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_DEPTH_FUNC,&depth);
 std::fprintf(stderr,"GLES3_QA preclear frame=%u depthwrite=%d clear=%g depthtest=%d depthfunc=%x scissor=%d box=%d,%d,%d,%d color=%d,%d,%d,%d fbo=%d\n",qaWorldFrame,mask,clear,glIsEnabled(GL_DEPTH_TEST),depth,glIsEnabled(GL_SCISSOR_TEST),scissor[0],scissor[1],scissor[2],scissor[3],color[0],color[1],color[2],color[3],fbo);
}
void qaWorldPass(const char* name){
 if(!qaWorldEnabled()||qaWorldFrame!=300)return;
 GLint viewport[4]{},buffer=0,alignment=0,row=0,skipRows=0,skipPixels=0;glGetIntegerv(GL_VIEWPORT,viewport);
 const int width=viewport[2],height=viewport[3];if(width<=0||height<=0||width>4096||height>4096)return;
 glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&buffer);glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glGetIntegerv(GL_PACK_ROW_LENGTH,&row);glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
 glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
 std::vector<unsigned char> pixels(size_t(width)*height*4);glReadPixels(viewport[0],viewport[1],width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
 char filename[128];std::snprintf(filename,sizeof(filename),"tcpe-gles3-qa-world-%s.rgba",name);if(FILE*f=std::fopen(filename,"wb")){std::fwrite(pixels.data(),1,pixels.size(),f);std::fclose(f);std::fprintf(stderr,"GLES3_QA world frame=%u phase=%s size=%dx%d file=%s\n",qaWorldFrame,name,width,height,filename);}
 glBindBuffer(GL_PIXEL_PACK_BUFFER,buffer);glPixelStorei(GL_PACK_ALIGNMENT,alignment);glPixelStorei(GL_PACK_ROW_LENGTH,row);glPixelStorei(GL_PACK_SKIP_ROWS,skipRows);glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
}
}
#else
static void qaWorldBeforeClear(){}
static void qaWorldPass(const char*){}
#endif

#if defined(TCPE_GLES3) && defined(PLATFORM_DESKTOP)
void tcpeQaWorldAtlasPass(int layer,int atlas){char phase[48];std::snprintf(phase,sizeof(phase),"terrain%d-atlas%d",layer,atlas);qaWorldPass(phase);}
#endif

static int _shTicks = -1;

GameRenderer::GameRenderer( Minecraft* mc )
:	mc(mc),
	renderDistance(0),
	_tick(0),
	_lastTickT(0),
	fovOffset(0),
	fovOffsetO(0),
	fov(1), oFov(1),
	_setupCameraFov(0),
	zoom(1), zoom_x(0), zoom_y(0),
	cameraRoll(0), cameraRollO(0),
	pickDirection(1, 0, 0),

	thirdDistance(4), thirdDistanceO(4),
	thirdRotation(0), thirdRotationO(0),
	thirdTilt(0), thirdTiltO(0),

	fogBr(0), fogBrO(0),

	fr(0), fg(0), fb(0),
	_rotX(0), _rotY(0),
	_rotXlast(0), _rotYlast(0),
	useScreenScissor(false)
{
	saveMatrices();

	itemInHandRenderer = new ItemInHandRenderer(mc);

	EntityRenderDispatcher* e = EntityRenderDispatcher::getInstance();
	e->itemInHandRenderer = itemInHandRenderer;
	e->textures = mc->textures;
}

GameRenderer::~GameRenderer() {
	delete itemInHandRenderer;
}

void renderCursor(float x, float y, Minecraft* minecraft) {
	Tesselator& t = Tesselator::instance;

	minecraft->textures->loadAndBindTexture("gui/cursor.png");
	glEnable(GL_BLEND);

	const float s = 32;
	const float width = 16;
	const float height = 16;
	t.begin();
	t.color(0xffffffff);
	t.vertexUV(x, y + (float)height, 0, 0, 1);
	t.vertexUV(x + (float)width, y + (float)height, 0, 1, 1);
	t.vertexUV(x + (float)width, y, 0, 1, 0);
	t.vertexUV(x, y, 0, 0, 0);
	t.draw();

	glDisable(GL_BLEND);
}

/*private*/
void GameRenderer::setupCamera(float a, int eye) {
    renderDistance = (float) (16 * 16 >> (mc->options.viewDistance));
#if defined(ANDROID)
    if (mc->isPowerVR() && mc->options.viewDistance <= 2)
		renderDistance *= 0.8f;
#endif

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity2();

    float stereoScale = 0.07f;
    if (mc->options.anaglyph3d) glTranslatef2(-(eye * 2 - 1) * stereoScale, 0, 0);
    if (zoom != 1) {
        glTranslatef2((float) zoom_x, (float) -zoom_y, 0);
		glScalef2(zoom, zoom, 1);
        gluPerspective(_setupCameraFov = getFov(a, true), mc->width / (float) mc->height, 0.05f, renderDistance);
    } else {
        gluPerspective(_setupCameraFov = getFov(a, true), mc->width / (float) mc->height, 0.05f, renderDistance);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity2();
    if (mc->options.anaglyph3d) glTranslatef2((eye * 2 - 1) * 0.10f, 0, 0);

    bobHurt(a);
    if (mc->options.bobView) bobView(a);
	 
	moveCameraToPlayer(a);
}

extern int _t_keepPic;

/*public*/
void GameRenderer::render(float a) {
	TIMER_PUSH("mouse");
	if (mc->player && mc->mouseGrabbed) {
        mc->mouseHandler.poll();
        //printf("Controller.x,y : %f,%f\n", Controller::getX(0), Controller::getY(0));

        float ss = mc->options.sensitivity * 0.6f + 0.2f;
        float sens = (ss * ss * ss) * 8;
        float xo = mc->mouseHandler.xd * sens * 4.f;
        float yo = mc->mouseHandler.yd * sens * 4.f;

		const float now = _tick + a;
		float deltaT = now - _lastTickT;
		if (deltaT > 3.0f) deltaT = 3.0f;
		_lastTickT = now;

		_rotX += xo;
		_rotY += yo;

        int yAxis = -1;
        if (mc->options.invertYMouse) yAxis = 1;

		bool screenCovering = mc->screen && !mc->screen->passEvents;
		if (!screenCovering)
		{
			mc->player->turn(deltaT * _rotXlast, deltaT * _rotYlast * yAxis);
		}
    }

	int xMouse = (int)(Mouse::getX() * Gui::InvGuiScale);
	int yMouse = (int)(Mouse::getY() * Gui::InvGuiScale);
#ifndef PLATFORM_DESKTOP
    if (mc->useTouchscreen() && !mc->screen) {
		const int pid = Multitouch::getFirstActivePointerIdExThisUpdate();
		if (pid >= 0) {
			xMouse = (int)(Multitouch::getX(pid) * Gui::InvGuiScale);
			yMouse = (int)(Multitouch::getY(pid) * Gui::InvGuiScale);
		} else {
			xMouse = -9999;
			yMouse = -9999;
		}
	}
#endif
	TIMER_POP();

	bool hasClearedColorBuffer = false;
	bool hasSetupGuiScreen = false;
	useScreenScissor = false;
	if (mc->isLevelGenerated()) {

		TIMER_PUSH("level");
		if (_t_keepPic < 0) {
		if (!(mc->screen && !mc->screen->renderGameBehind())) {

			if (mc->screen && mc->screen->hasClippingArea(screenScissorArea))
				useScreenScissor = true;

			renderLevel(a);
			hasClearedColorBuffer = true;

			if (!mc->options.hideGui) {
				TIMER_POP_PUSH("gui");
				setupGuiScreen(false);
				hasSetupGuiScreen = true;
				mc->gui.render(a, mc->screen != NULL, xMouse, yMouse);
			}
		}}
		TIMER_POP();

	} else {
        glViewport(0, 0, mc->width, mc->height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity2();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity2();
        setupGuiScreen(true);
		hasSetupGuiScreen = true;
		hasClearedColorBuffer = true;
    }
	//@todo
	if (!hasSetupGuiScreen)
		setupGuiScreen(!hasClearedColorBuffer);

	if (mc->player && mc->screen == NULL) {
		if (mc->inputHolder) mc->inputHolder->render(a);
		if (mc->player->input) mc->player->input->render(a);
	}

    if (mc->screen != NULL) {
		if (useScreenScissor)
			glDisable2(GL_SCISSOR_TEST);

		mc->screen->render(xMouse, yMouse, a);
#ifdef RPI
		renderCursor(xMouse, yMouse, mc);
#endif
		// Screen might have been removed, so check it again
		if (mc->screen && !mc->screen->isInGameScreen())
			sleepMs(15);
    }
}

/*public*/
void GameRenderer::renderLevel(float a) {

    if (mc->cameraTargetPlayer == NULL) {
		if (mc->player)
		{
			mc->cameraTargetPlayer = mc->player;
		}
		else
		{
			return;
		}
    }

	TIMER_PUSH("pick");
    pick(a);

    Mob* cameraEntity = mc->cameraTargetPlayer;
    LevelRenderer* levelRenderer = mc->levelRenderer;
    ParticleEngine* particleEngine = mc->particleEngine;
    float xOff = cameraEntity->xOld + (cameraEntity->x - cameraEntity->xOld) * a;
    float yOff = cameraEntity->yOld + (cameraEntity->y - cameraEntity->yOld) * a;
    float zOff = cameraEntity->zOld + (cameraEntity->z - cameraEntity->zOld) * a;

    for (int i = 0; i < 2; i++) {
        if (mc->options.anaglyph3d) {
            if (i == 0) glColorMask(false, true, true, false);
            else glColorMask(true, false, false, false);
        }

		TIMER_POP_PUSH("clear");
		glViewport(0, 0, mc->width, mc->height);
		setupClearColor(a);

        qaWorldBeforeClear();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable2(GL_CULL_FACE);

		TIMER_POP_PUSH("camera");
        setupCamera(a, i);
		saveMatrices();

		if (useScreenScissor) {
			glEnable2(GL_SCISSOR_TEST);
			glScissor(	screenScissorArea.x, screenScissorArea.y,
						screenScissorArea.w, screenScissorArea.h);
		}
		
        if(mc->options.fancyGraphics) {
			setupFog(-1);
			TIMER_POP_PUSH("sky");
			glFogf(GL_FOG_START, renderDistance  * 0.2f);
			glFogf(GL_FOG_END, renderDistance *0.75);
			glDepthMask(GL_FALSE);
            levelRenderer->renderSky(a);
			glDepthMask(GL_TRUE);
			glFogf(GL_FOG_START, renderDistance  * 0.6f);
			glFogf(GL_FOG_END, renderDistance);
        }
        glEnable2(GL_FOG);
        setupFog(1);

        // Keep the actual chunk tessellator AO flag in sync with the UI option.
        // Previously the option only changed GL shade model; chunks still rebuilt
        // through Minecraft::useAmbientOcclusion=false, making smooth lighting look
        // broken until unrelated code happened to touch the static flag.
        if (Minecraft::useAmbientOcclusion != mc->options.ambientOcclusion) {
            Minecraft::useAmbientOcclusion = mc->options.ambientOcclusion;
            if (mc->levelRenderer != NULL) {
                mc->levelRenderer->allChanged();
            }
        }
        if (Minecraft::useAmbientOcclusion) {
            glShadeModel2(GL_SMOOTH);
		}
        
		TIMER_POP_PUSH("frustrum");
		FrustumCuller frustum;
        frustum.prepare(xOff, yOff, zOff);

		TIMER_POP_PUSH("culling");
        if (mc->options.fancyGraphics && !mc->level->isJavaServerLevel()) {
            mc->levelRenderer->setAllVisible();
        } else {
            mc->levelRenderer->cull(&frustum, a);
        }
        mc->levelRenderer->updateDirtyChunks(cameraEntity, false);

		if(mc->options.fancyGraphics) {
			prepareAndRenderClouds(levelRenderer, a);
			setupCamera(a, i);
		}

        setupFog(0);
        glEnable2(GL_FOG);

		mc->textures->loadAndBindTexture("terrain.png");
        glDisable2(GL_ALPHA_TEST);
        glDisable2(GL_BLEND);
        glEnable2(GL_CULL_FACE);
		TIMER_POP_PUSH("terrain-0");
        levelRenderer->render(cameraEntity, 0, a);
        qaWorldPass("terrain0");

		TIMER_POP_PUSH("terrain-1");
        glEnable2(GL_ALPHA_TEST);
        levelRenderer->render(cameraEntity, 1, a);
        qaWorldPass("terrain1");
        
        glShadeModel2(GL_FLAT);
		TIMER_POP_PUSH("entities");
		mc->levelRenderer->renderEntities(cameraEntity->getPos(a), &frustum, a);
        qaWorldPass("entities");
//        setupFog(0);
		TIMER_POP_PUSH("particles");
        particleEngine->render(cameraEntity, a);

		glDisable2(GL_BLEND);
        glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        setupFog(0);
        glEnable2(GL_BLEND);
        glDisable2(GL_CULL_FACE);
		glDepthMask(GL_FALSE);
        glDisable2(GL_ALPHA_TEST);
		mc->textures->loadAndBindTexture("terrain.png");
        //if (mc->options.fancyGraphics) {
        //    glColorMask(false, false, false, false);
        //    int visibleWaterChunks = levelRenderer->render(cameraEntity, 1, a);
        //    glColorMask(true, true, true, true);
        //    if (mc->options.anaglyph3d) {
        //        if (i == 0) glColorMask(false, true, true, false);
        //        else glColorMask(true, false, false, false);
        //    }
        //    if (visibleWaterChunks > 0) {
        //        levelRenderer->renderSameAsLast(1, a);
        //    }
        //} else
		{
			//glDepthRangef(0.1f, 1.0f);
			//glDepthMask(GL_FALSE);
			TIMER_POP_PUSH("terrain-water");
			glEnable2(GL_DEPTH_TEST);
            levelRenderer->render(cameraEntity, 2, a);
            qaWorldPass("water");
			//glDepthRangef(0, 1);

        }
        
		glDepthMask(GL_TRUE);
        glEnable2(GL_CULL_FACE);
        glDisable2(GL_BLEND);
        glEnable2(GL_ALPHA_TEST);

		if (/*!Minecraft::FLYBY_MODE &&*/ zoom == 1 && cameraEntity->isPlayer()) {
			if (mc->hitResult.isHit() && !cameraEntity->isUnderLiquid(Material::water)) {
				TIMER_POP_PUSH("select");
				Player* player = (Player*) cameraEntity;
				if (mc->useTouchscreen()) {
					levelRenderer->renderHitSelect(player, mc->hitResult, 0, NULL, a); //player.inventory->getSelected(), a);
				}
				levelRenderer->renderHit(player, mc->hitResult, 0, NULL, a);//player->inventory.getSelected(), a);
			}
		}

		glDisable2(GL_FOG);
//
//        setupFog(0);
//        glEnable2(GL_FOG);
////        levelRenderer->renderClouds(a);
//        glDisable2(GL_FOG);
        setupFog(1);

        renderNativePrecipitation(a);
        if(!mc->level->isJavaServerLevel()) WeatherLightningRenderer::render(*mc->level,cameraEntity->getPos(a));

        if (zoom == 1) {
			TIMER_POP_PUSH("hand");
            glClear(GL_DEPTH_BUFFER_BIT);
            renderItemInHand(a, i);
            qaWorldPass("hand");
        }

        if (!mc->options.anaglyph3d) {
			TIMER_POP();
            return;
        }
    }
    glColorMask(true, true, true, false);
	TIMER_POP();
}

void GameRenderer::tickFov() {
	if (mc->cameraTargetPlayer != mc->player)
		return;

	oFov = fov;
	fov += (mc->player->getFieldOfViewModifier() - fov) * 0.5f;
}

/*private*/
float GameRenderer::getFov(float a, bool applyEffects) {
    Mob* player = mc->cameraTargetPlayer;
    float fov = mc->options.fov;

	if (applyEffects)
		fov *= this->oFov + (this->fov - this->oFov) * a;

    if (player->isUnderLiquid(Material::water)) fov = 60;
    if (player->health <= 0) {
        float duration = player->deathTime + a;

        fov /= ((1 - 500 / (duration + 500)) * 2.0f + 1);
    }
    return fov + fovOffsetO + (fovOffset - fovOffsetO) * a;
}

/*private*/
void GameRenderer::moveCameraToPlayer(float a) {
    Entity* player = mc->cameraTargetPlayer;

    float heightOffset = player->heightOffset - 1.62f;

    float x = player->xo + (player->x - player->xo) * a;
    float y = player->yo + (player->y - player->yo) * a - heightOffset;
	//printf("camera y: %f\n", y);
    float z = player->zo + (player->z - player->zo) * a;

	//printf("rot: %f %f\n", cameraRollO, cameraRoll);
    glRotatef2(cameraRollO + (cameraRoll - cameraRollO) * a, 0, 0, 1);

	//LOGI("player. alive, removed: %d, %d\n", player->isAlive(), player->removed);
	if(player->isPlayer() && ((Player*)player)->isSleeping()) {
		heightOffset += 1.0;
		glTranslatef(0.0f, 0.3f, 0);
		if (!mc->options.fixedCamera) {
			int t = mc->level->getTile(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
			if (t == Tile::bed->id) {
				int data = mc->level->getData(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));

				int direction = data & 3;
				glRotatef(float(direction * 90), 0, 1, 0);
			}
			glRotatef(player->yRotO + (player->yRot - player->yRotO) * a + 180, 0, -1, 0);
			glRotatef(player->xRotO + (player->xRot - player->xRotO) * a, -1, 0, 0);
		}
	} else if (mc->options.thirdPersonView/* || (player->isPlayer() && !player->isAlive())*/) {
        float cameraDist = thirdDistanceO + (thirdDistance - thirdDistanceO) * a;

        if (mc->options.fixedCamera) {

            float rotationY = thirdRotationO + (thirdRotation - thirdRotationO) * a;
            float xRot = thirdTiltO + (thirdTilt - thirdTiltO) * a;

            glTranslatef2(0, 0, (float) -cameraDist);
            glRotatef2(xRot, 1, 0, 0);
            glRotatef2(rotationY, 0, 1, 0);
        } else {
            float yRot = player->yRot;
            float xRot = player->xRot/* + 180.0f*/;
            float xd = -Mth::sin(yRot / 180 * Mth::PI) * Mth::cos(xRot / 180 * Mth::PI) * cameraDist;
            float zd = Mth::cos(yRot / 180 * Mth::PI) * Mth::cos(xRot / 180 * Mth::PI) * cameraDist;
            float yd = -Mth::sin(xRot / 180 * Mth::PI) * cameraDist;

            for (int i = 0; i < 8; i++) {
                float xo = (float)((i & 1) * 2 - 1);
                float yo = (float)(((i >> 1) & 1) * 2 - 1);
                float zo = (float)(((i >> 2) & 1) * 2 - 1);

                xo *= 0.1f;
                yo *= 0.1f;
                zo *= 0.1f;

                HitResult hr = mc->level->clip(Vec3(x + xo, y + yo, z + zo), Vec3(x - xd + xo + zo, y - yd + yo, z - zd + zo)); // newTemp
				if (hr.type != NO_HIT) {
                    float dist = hr.pos.distanceTo(Vec3(x, y, z)); // newTemp
                    if (dist < cameraDist) cameraDist = dist;
                }
            }

			//glRotatef2(180, 0, 1, 0);

			glRotatef2(player->xRot - xRot, 1, 0, 0);
            glRotatef2(player->yRot - yRot, 0, 1, 0);
            glTranslatef2(0, 0, (float) -cameraDist);
            glRotatef2(yRot - player->yRot, 0, 1, 0);
            glRotatef2(xRot - player->xRot, 1, 0, 0);
        }
    } else {
        glTranslatef2(0, 0, -0.1f);
    }

    if (!mc->options.fixedCamera) {
        glRotatef2(player->xRotO + (player->xRot - player->xRotO) * a, 1.0f, 0.0f, 0.0f);
        glRotatef2(player->yRotO + (player->yRot - player->yRotO) * a + 180, 0, 1, 0);
		//if (_t_keepPic > 0)
	}
    glTranslatef2(0, heightOffset, 0);
}

/*private*/
void GameRenderer::bobHurt(float a) {
    Mob* player = mc->cameraTargetPlayer;

    float hurt = player->hurtTime - a;

    if (player->health <= 0) {
        float duration = player->deathTime + a;
        glRotatef2(40 - (40 * 200) / (duration + 200), 0, 0, 1);
    }

    if (player->hurtTime <= 0) return;

	hurt /= player->hurtDuration;
    hurt = (float) Mth::sin(hurt * hurt * hurt * hurt * Mth::PI);

    float rr = player->hurtDir;

    glRotatef2(-rr, 0, 1, 0);
    glRotatef2(-hurt * 14, 0, 0, 1);
    glRotatef2(+rr, 0, 1, 0);
}

/*private*/
void GameRenderer::bobView(float a) {
    //if (mc->options.thirdPersonView) return;
	if (!(mc->cameraTargetPlayer->isPlayer())) {
        return;
    }
    Player* player = (Player*) mc->cameraTargetPlayer;

    float wda = player->walkDist - player->walkDistO;
    float b = -(player->walkDist + wda * a);
    float bob = player->oBob + (player->bob - player->oBob) * a;
    float tilt = player->oTilt + (player->tilt - player->oTilt) * a;
    glTranslatef2((float) Mth::sin(b * Mth::PI) * bob * 0.5f, -(float) std::abs(Mth::cos(b * Mth::PI) * bob), 0);
    glRotatef2((float) Mth::sin(b * Mth::PI) * bob * 3, 0, 0, 1);
    glRotatef2((float) std::abs(Mth::cos(b * Mth::PI - 0.2f) * bob) * 5, 1, 0, 0);
    glRotatef2((float) tilt, 1, 0, 0);
}

/*private*/
void GameRenderer::setupFog(int i) {
    Mob* player = mc->cameraTargetPlayer;
	float fogBuffer[4] = {fr, fg, fb, 1};

    glFogfv(GL_FOG_COLOR, (GLfloat*)fogBuffer);
    glColor4f2(1, 1, 1, 1);

    if (player->isUnderLiquid(Material::water)) {
        glFogx(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 0.1f); // was 0.06

//        float rr = 0.4f;
//        float gg = 0.4f;
//        float bb = 0.9f;
//
//        if (mc->options.anaglyph3d) {
//            float rrr = (rr * 30 + gg * 59 + bb * 11) / 100;
//            float ggg = (rr * 30 + gg * 70) / (100);
//            float bbb = (rr * 30 + bb * 70) / (100);
//
//            rr = rrr;
//            gg = ggg;
//            bb = bbb;
//        }
    } else if (player->isUnderLiquid(Material::lava)) {
        glFogx(GL_FOG_MODE, GL_EXP);
        glFogf(GL_FOG_DENSITY, 2.f); // was 0.06
//        float rr = 0.4f;
//        float gg = 0.3f;
//        float bb = 0.3f;
//
//        if (mc->options.anaglyph3d) {
//            float rrr = (rr * 30 + gg * 59 + bb * 11) / 100;
//            float ggg = (rr * 30 + gg * 70) / (100);
//            float bbb = (rr * 30 + bb * 70) / (100);
//
//            rr = rrr;
//            gg = ggg;
//            bb = bbb;
//        }
    } else {
        glFogx(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, renderDistance * 0.6f);
        glFogf(GL_FOG_END, renderDistance);
        if (i < 0) {
            glFogf(GL_FOG_START, 0);
            glFogf(GL_FOG_END, renderDistance * 1.0f);
        }

        if (mc->level->dimension->foggy) {
            glFogf(GL_FOG_START, 0);
        }
    }

    glEnable2(GL_COLOR_MATERIAL);
    //glColorMaterial(GL_FRONT, GL_AMBIENT);
}

void GameRenderer::updateAllChunks() {
    mc->levelRenderer->updateDirtyChunks(mc->cameraTargetPlayer, true);
}

bool GameRenderer::updateFreeformPickDirection(float a, Vec3& outDir) {

    if (!mc->inputHolder->allowPicking()) {
        _shTicks = 1;
        return false;
    }

    Vec3 c = mc->cameraTargetPlayer->getPos(a);

    bool firstPerson = !mc->options.thirdPersonView;
    const float PickingDistance = firstPerson? 6.0f : 12.0f;

    _shTicks = -1;

    int vp[4] = {0, 0, mc->width, mc->height};
    float pt[3];
    float x = mc->inputHolder->mousex;
    float y = mc->height - mc->inputHolder->mousey;

    //sw.start();

    if (!glhUnProjectf(x, y, 1, lastModelMatrix, lastProjMatrix, vp, pt)) {
        return false;
    }
    Vec3 p1(pt[0] + c.x, pt[1] + c.y, pt[2] + c.z);

    glhUnProjectf(x, y, 0, lastModelMatrix, lastProjMatrix, vp, pt);
    Vec3 p0(pt[0] + c.x, pt[1] + c.y, pt[2] + c.z);

    outDir = (p1 - p0).normalized();
    p1 = p0 + outDir * PickingDistance;

    //sw.stop();
    //sw.printEvery(30, "unproject ");

    const HitResult& hit = mc->hitResult = mc->level->clip(p0, p1, false);

    // If in 3rd person view - verify that the hit target is within range
    if (!firstPerson && hit.isHit()) {
        const float MaxSqrDist = PickingDistance*PickingDistance;
        if (mc->cameraTargetPlayer->distanceToSqr((float)hit.x, (float)hit.y, (float)hit.z) > MaxSqrDist)
            mc->hitResult.type = NO_HIT;
    }
    return true;
}

/*public*/
void GameRenderer::pick(float a) {
	if (mc->level == NULL) return;
    if (mc->cameraTargetPlayer == NULL) return;
	if (!mc->cameraTargetPlayer->isAlive()) return;

    if(mc->level->isJavaServerLevel()&&mc->javaSession){
        JavaGameSession& session=*mc->javaSession;
        Vec3 direction=mc->cameraTargetPlayer->getViewVector(a),offset(0,0,0);
        bool enabled=true;
#ifndef PLATFORM_DESKTOP
        const bool freeform=mc->useTouchscreen()&&!mc->options.isJoyTouchArea;
#else
        const bool freeform=false;
#endif
        auto* touch=mc->inputHolder?dynamic_cast<UnifiedTurnBuild*>(mc->inputHolder->getBuildInput()):nullptr;
        const bool queuedTap=freeform&&touch&&touch->pendingJavaTap();
        const bool lockedTap=queuedTap&&touch->javaTaps.locked(session.pickInputRevision);
        if(lockedTap){enabled=true;offset=Vec3(session.pickRayOffset[0],session.pickRayOffset[1],session.pickRayOffset[2]);direction=Vec3(session.pickRayDirection[0],session.pickRayDirection[1],session.pickRayDirection[2]);}
        else if(freeform){
            enabled=mc->inputHolder->allowPicking();
            if(enabled){
                int vp[4]={0,0,mc->width,mc->height};float nearPoint[3],farPoint[3];
                const float x=mc->inputHolder->mousex,y=mc->height-mc->inputHolder->mousey;
                enabled=glhUnProjectf(x,y,0,lastModelMatrix,lastProjMatrix,vp,nearPoint)&&glhUnProjectf(x,y,1,lastModelMatrix,lastProjMatrix,vp,farPoint);
                if(enabled){offset=Vec3(nearPoint[0],nearPoint[1],nearPoint[2]);direction=(Vec3(farPoint[0],farPoint[1],farPoint[2])-offset).normalized();}
            }
        }
        pickDirection=direction;mc->cameraTargetPlayer->aimDirection=direction;_shTicks=-1;
        enabled=session.updatePickRay(enabled,{{offset.x,offset.y,offset.z}},{{direction.x,direction.y,direction.z}})&&enabled;
        if(queuedTap&&enabled&&!lockedTap)touch->javaTaps.bind(session.pickInputRevision);
        mc->hitResult=HitResult();
        if(enabled&&session.hasTargeting()){
            const JavaGameSession::Targeting& hit=session.targeting;
            if(hit.kind=="block")mc->hitResult=HitResult(hit.block[0],hit.block[1],hit.block[2],hit.face,Vec3(float(hit.location[0]),float(hit.location[1]),float(hit.location[2])));
            else if(hit.kind=="entity")for(Entity* entity:mc->level->entities)if(entity&&entity->entityId==hit.entityId){mc->hitResult=HitResult(entity);break;}
        }
        if(enabled&&session.hasAttackTargeting())for(Entity* entity:mc->level->entities)if(entity&&entity->entityId==session.targeting.attackEntityId){mc->hitResult=HitResult(entity);break;}
        return; // Java geometry never enters PE tile/full-cube or floor-assist picking.
    }
    float range = mc->gameMode->getPickRange();
    bool isPicking = true;
#ifndef PLATFORM_DESKTOP
    bool freeform = mc->useTouchscreen()  && !mc->options.isJoyTouchArea;
#else 
    bool freeform = false;
#endif
    if (freeform) {
        isPicking = updateFreeformPickDirection(a, pickDirection);
    } else {
        mc->hitResult = mc->cameraTargetPlayer->pick(range, a);
        pickDirection = mc->cameraTargetPlayer->getViewVector(a);
    }

    Vec3  from = mc->cameraTargetPlayer->getPos(a);
    float dist = range;
	if (mc->hitResult.isHit()) {
        dist = mc->hitResult.pos.distanceTo(from);
    }

	if (mc->gameMode->isCreativeType()) {
        /*dist =*/ range = 12;
    } else {
        const float entityRange=mc->javaSession?float(mc->javaSession->attackMaxRange+mc->javaSession->attackHitboxMargin):3.f;
        if (dist > entityRange) dist = entityRange;
        range = dist;
    }

    Vec3 pv = (pickDirection * range);
    Vec3 to  = from + pv;
    mc->cameraTargetPlayer->aimDirection = pickDirection;

    Entity* hovered = NULL;
    const float g = 1;
    AABB aabb = mc->cameraTargetPlayer->bb.expand(pv.x, pv.y, pv.z).grow(g, g, g);
	EntityList& objects = mc->level->getEntities(mc->cameraTargetPlayer, aabb);
    float nearest = 0;
    for (unsigned int i = 0; i < objects.size(); i++) {
        Entity* e = objects[i];
        if (!e->isPickable()) continue;

        float rr = e->getPickRadius();
        AABB bb = e->bb.grow(rr, rr, rr);
        HitResult p = bb.clip(from, to);
		//printf("Clip Hitresult %d (%d)\n", p.type, p.isHit());

        if (bb.contains(from)) {
            //@todo: hovered = e; break; ?
            if (nearest >= 0) {
                hovered = e;
                nearest = 0;
            }
		} else if (p.isHit()) {
            float dd = from.distanceTo(p.pos);
            if (dd < nearest || nearest == 0) {
                hovered = e;
                nearest = dd;
            }
        }
    }

    if (hovered != NULL) {
		if(nearest < dist) {
			mc->hitResult = HitResult(hovered);
		}
    }
	else if (isPicking && !mc->hitResult.isHit()) {
		// if we don't have a hit result, attempt to hit the edge of the block we are standing on
		// (this is an pocket edition simplification to help building floors)
        //LOGI("hovered : %d (%f)\n", mc->hitResult.type, viewVec.y);
		if (pickDirection.y < -.7f) {
			// looking down by more than roughly 45 degrees, fetch a hit to the block standing on
			Vec3 to = from.add(0, -2.0f, 0);

			HitResult downHitResult = mc->level->clip(from, to);
			if (downHitResult.isHit()) {
				mc->hitResult = downHitResult;
				mc->hitResult.indirectHit = true;
				// change face (not up)
				if (std::abs(pickDirection.x) > std::abs(pickDirection.z)) {
                    mc->hitResult.f = (pickDirection.x < 0)? 4 : 5;
				} else {
                    mc->hitResult.f = (pickDirection.z < 0)? 2 : 3;
				}
			}
		}
	}
}
/*public*/
void GameRenderer::tick(int nTick, int maxTick) {
    weatherEffects.tick(*mc,_tick);
	--_t_keepPic;

	if (!mc->player)
	{
		return;
	}

	if (--_shTicks == 0)
		mc->hitResult.type = NO_HIT;

	//_rotXlast = _rotX;
	//_rotYlast = _rotY;

	//LOGI("x: %f\n", _rotX);

    if (nTick == maxTick) {
        const float tickMult = 1.0f / (float)(1 + maxTick);
        _rotXlast = 0.4f * std::pow(std::abs(_rotX), 1.2f) * tickMult;
        if (_rotX < 0) _rotXlast = -_rotXlast;

        _rotYlast = 0.4f * std::pow(std::abs(_rotY), 1.2f) * tickMult;
        if (_rotY < 0) _rotYlast = -_rotYlast;

        _rotX = 0;
        _rotY = 0;
    }

    fogBrO = fogBr;
    thirdDistanceO = thirdDistance;
    thirdRotationO = thirdRotation;
    thirdTiltO = thirdTilt;
    fovOffsetO = fovOffset;
    cameraRollO = cameraRoll;

    if (mc->cameraTargetPlayer == NULL) {
        mc->cameraTargetPlayer = mc->player;
    }

	tickFov();

    float brr = mc->level->getBrightness(	Mth::floor(mc->cameraTargetPlayer->x),
											Mth::floor(mc->cameraTargetPlayer->y),
											Mth::floor(mc->cameraTargetPlayer->z));

	float whiteness = (3 - mc->options.viewDistance) / 3.0f;
    float fogBrT = brr * (1 - whiteness) + whiteness;
    fogBr += (fogBrT - fogBr) * 0.1f;

    _tick++;

    itemInHandRenderer->tick();
//    if (mc->isRaining) tickRain();
}

/*private*/
void GameRenderer::setupClearColor(float a) {
    Level* level = mc->level;
    Mob* player = mc->cameraTargetPlayer;

    float whiteness = 1.0f / (4 - mc->options.viewDistance);
    whiteness = 1 - (float) pow(whiteness, 0.25f);

    Vec3 skyColor = level->getSkyColor(mc->cameraTargetPlayer, a);
    float sr = (float) skyColor.x;
    float sg = (float) skyColor.y;
    float sb = (float) skyColor.z;

    Vec3 fogColor = level->getFogColor(a);
    fr = (float) fogColor.x;
    fg = (float) fogColor.y;
    fb = (float) fogColor.z;

    fr += (sr - fr) * whiteness;
    fg += (sg - fg) * whiteness;
    fb += (sb - fb) * whiteness;

    if(!level->isJavaServerLevel()) {
        const float rain=level->getLevelData()->weather.rainAt(a);
        const float storm=level->getLevelData()->weather.thunderAt(a);
        const float redGreen=(1.f-.5f*rain)*(1.f-.5f*storm);
        fr*=redGreen;fg*=redGreen;fb*=(1.f-.4f*rain)*(1.f-.5f*storm);
    }

    if (player->isUnderLiquid(Material::water)) {
        fr = 0.02f;
        fg = 0.02f;
        fb = 0.2f;
    } else if (player->isUnderLiquid(Material::lava)) {
        fr = 0.6f;
        fg = 0.1f;
        fb = 0.00f;
    }

    float brr = fogBrO + (fogBr - fogBrO) * a;
    fr *= brr;
    fg *= brr;
    fb *= brr;

    if (mc->options.anaglyph3d) {
        float frr = (fr * 30 + fg * 59 + fb * 11) / 100;
        float fgg = (fr * 30 + fg * 70) / (100);
        float fbb = (fr * 30 + fb * 70) / (100);

        fr = frr;
        fg = fgg;
        fb = fbb;
    }

    glClearColor(fr, fg, fb, 1.0f);
}

void GameRenderer::zoomRegion( float zoom, float xa, float ya )
{
	this->zoom = zoom;
	this->zoom_x = xa;
	this->zoom_y = ya;
}

void GameRenderer::unZoomRegion()
{
	zoom = 1;
}

void GameRenderer::setupGuiScreen( bool clearColorBuffer )
{
	int screenWidth = (int)(mc->width * Gui::InvGuiScale);
	int screenHeight = (int)(mc->height * Gui::InvGuiScale);

	// Setup GUI render mode
	GLbitfield clearBits = clearColorBuffer?
			GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT
		:	GL_DEPTH_BUFFER_BIT;
	glClear(clearBits);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity2();
	glOrthof(0, (GLfloat)screenWidth, (GLfloat)screenHeight, 0, 2000, 3000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity2();
	glTranslatef2(0, 0, -2000);
}

/*private*/
void GameRenderer::renderItemInHand(float a, int eye) {
    glLoadIdentity2();
    if (mc->options.anaglyph3d) glTranslatef2((eye * 2 - 1) * 0.10f, 0, 0);

    glPushMatrix2();
    bobHurt(a);
    if (mc->options.bobView) bobView(a);

    if (!mc->options.thirdPersonView && (mc->cameraTargetPlayer->isPlayer() && !((Player*)mc->cameraTargetPlayer)->isSleeping())) {
        if (!mc->options.hideGui) {
			float fov = getFov(a, false);
			if (fov != _setupCameraFov) {
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				gluPerspective(fov, mc->width / (float) mc->height, 0.05f, renderDistance);
				glMatrixMode(GL_MODELVIEW);
			}
            itemInHandRenderer->render(a);
        }
    }

    glPopMatrix2();
    if (!mc->options.thirdPersonView && (mc->cameraTargetPlayer->isPlayer() && !((Player*)mc->cameraTargetPlayer)->isSleeping())) {
        itemInHandRenderer->renderScreenEffect(a);
        bobHurt(a);
    }
    if (mc->options.bobView) bobView(a);
}

void GameRenderer::onGraphicsReset()
{
	if (itemInHandRenderer) itemInHandRenderer->onGraphicsReset();
}

void GameRenderer::saveMatrices()
{
	#if defined(RPI)
		return;
	#endif

	static bool saved = false;
	//if (saved) return;

	saved = true;

	glGetFloatv(GL_PROJECTION_MATRIX, lastProjMatrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, lastModelMatrix);
}

void GameRenderer::prepareAndRenderClouds( LevelRenderer* levelRenderer, float a ) {
	//if(mc->options.isCloudsOn()) {
	TIMER_PUSH("clouds");
	glMatrixMode(GL_PROJECTION);
	glPushMatrix2();
	glLoadIdentity2();
	gluPerspective(_setupCameraFov = getFov(a, true), mc->width / (float) mc->height, 2, renderDistance * 512);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix2();
	setupFog(0);
	glDepthMask(false);
	glEnable2(GL_FOG);
	glFogf(GL_FOG_START, renderDistance  * 0.2f);
	glFogf(GL_FOG_END, renderDistance * 0.75f);
	levelRenderer->renderSky(a);
	glFogf(GL_FOG_START, renderDistance * 4.2f * 0.6f);
	glFogf(GL_FOG_END, renderDistance * 4.2f);
	levelRenderer->renderClouds(a);
	glFogf(GL_FOG_START, renderDistance  * 0.6f);
	glFogf(GL_FOG_END, renderDistance);
	glDisable2(GL_FOG);
	glDepthMask(true);
	setupFog(1);
	glPopMatrix2();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix2();
	glMatrixMode(GL_MODELVIEW);
	TIMER_POP();
	//}
}

// Independent native precipitation pass using existing TCPE textures. Two
// batches avoid per-column draw calls and only loaded columns are inspected.
void GameRenderer::renderNativePrecipitation(float a) {
    Level* level=mc->level;Mob* camera=mc->cameraTargetPlayer;
    if(!level||!camera||level->isJavaServerLevel()||level->dimension->hasCeiling)return;
    const float intensity=level->getLevelData()->weather.rainAt(a);
    if(intensity<=0)return;
    const Vec3 origin=camera->getPos(a);
    const int radius=WeatherPrecipitationPolicy::radius(1);
    const int cx=Mth::floor(camera->x),cz=Mth::floor(camera->z);
    struct Column {int x,z;WeatherPrecipitationPolicy::Column shape;};Column columns[361];int count=0;
    for(int z=cz-radius;z<=cz+radius;++z)for(int x=cx-radius;x<=cx+radius;++x){
        if(!level->hasChunkAtNow(x,64,z))continue;
        Biome* biome=level->getBiome(x,z);
        if(!biome)continue;
        // Existing TCPE snow biomes map to the reference cold classification.
        // Warm native biomes stay above the reference .15 rain/snow threshold.
        const float temperature=biome->hasSnow()?(biome==Biome::taiga?.05f:0.f):.8f;
        auto shape=WeatherPrecipitationPolicy::sample(x,z,level->getTopRainBlock(x,z),temperature,
            biome->hasRain()||biome->hasSnow(),camera->x,camera->y,camera->z,origin.y,
            _tick,a,intensity);
        if(shape.visible)columns[count++]={x,z,shape};
    }
#if defined(PLATFORM_DESKTOP)
    static unsigned qaPass=0;
    if(std::getenv("TCPE_WEATHER_QA") && (++qaPass%300)==1){
        int snowColumns=0;for(int i=0;i<count;++i)if(columns[i].shape.snowy)++snowColumns;
        std::fprintf(stderr,"WEATHER_QA rain=%.3f thunder=%.3f columns=%d snow=%d camera=%.2f,%.2f,%.2f roof=%d\n",intensity,level->getLevelData()->weather.thunderAt(a),count,snowColumns,camera->x,camera->y,camera->z,level->getTopRainBlock(cx,cz));
    }
#endif
    glDisable2(GL_CULL_FACE);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    GLint previousShade=GL_FLAT;glGetIntegerv(GL_SHADE_MODEL,&previousShade);
    glShadeModel2(GL_SMOOTH);
    glAlphaFunc(GL_GREATER,.01f);
    glDepthMask(GL_FALSE);glColor4f(1,1,1,1);
    Tesselator& t=Tesselator::instance;
    for(int pass=0;pass<2;++pass){
        mc->textures->loadAndBindTexture(pass?"environment/snow.png":"environment/rain.png");
        t.begin();t.offset(-origin.x,-origin.y,-origin.z);
        for(int i=0;i<count;++i){const Column& col=columns[i];const auto& q=col.shape;
            if(q.snowy!=bool(pass))continue;
            float light=level->getBrightness(col.x,std::min(127,std::max(0,q.lightY)),col.z);
            if(pass)light=light*.75f+.25f;
            t.color(light,light,light,q.lowerAlpha);
            t.vertexUV(col.x+.5f-q.sideX,q.bottom,col.z+.5f-q.sideZ,q.u0,q.v0);
            t.vertexUV(col.x+.5f+q.sideX,q.bottom,col.z+.5f+q.sideZ,q.u1,q.v0);
            t.color(light,light,light,q.upperAlpha);
            t.vertexUV(col.x+.5f+q.sideX,q.top,col.z+.5f+q.sideZ,q.u1,q.v1);
            t.vertexUV(col.x+.5f-q.sideX,q.top,col.z+.5f-q.sideZ,q.u0,q.v1);
        }
        t.draw();t.offset(0,0,0);
    }
    glShadeModel2(previousShade);
    glAlphaFunc(GL_GREATER,.1f);
    glDepthMask(GL_TRUE);glDisable2(GL_BLEND);glEnable2(GL_CULL_FACE);glColor4f(1,1,1,1);
}
