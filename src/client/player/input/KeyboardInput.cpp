#include "KeyboardInput.h"
#include "../../../platform/input/ControllerActionKeys.h"
#include "../../Options.h"
#include "../../../world/entity/player/Player.h"

KeyboardInput::KeyboardInput( Options* options )
{
	for (int i = 0; i < NumKeys; ++i)
		keys[i] = false;
	this->options = options;
}

void KeyboardInput::setKey( int key, bool state )
{
	if(key>=ControllerKeys::Forward&&key<=ControllerKeys::Craft){keys[key-ControllerKeys::Forward]=state;return;}
    if (!key) return; // Unbound actions never receive a synthetic key0 event.
    // A physical key can intentionally drive more than one action.
    if (key == options->keyUp.key) keys[KEY_UP] = state;
    if (key == options->keyDown.key) keys[KEY_DOWN] = state;
    if (key == options->keyLeft.key) keys[KEY_LEFT] = state;
    if (key == options->keyRight.key) keys[KEY_RIGHT] = state;
    if (key == options->keyJump.key) keys[KEY_JUMP] = state;
    if (key == options->keySneak.key) keys[KEY_SNEAK] = state;
    if (key == options->keyCraft.key) keys[KEY_CRAFT] = state;
}

void KeyboardInput::releaseAllKeys()
{
	xa = 0;
	ya = 0;

	for (int i = 0; i < NumKeys; i++) {
		keys[i] = false;
	}
	wantUp = wantDown = false;
}

void KeyboardInput::tick( Player* player )
{
	xa = 0;
	ya = 0;

	if (keys[KEY_UP]) ya++;
	if (keys[KEY_DOWN]) ya--;
	if (keys[KEY_LEFT]) xa++;
	if (keys[KEY_RIGHT]) xa--;
	jumping = keys[KEY_JUMP];
	sneaking = keys[KEY_SNEAK];
	if (sneaking) {
		xa *= 0.3f;
		ya *= 0.3f;
	}

	#if defined(RPI) || defined(PLATFORM_DESKTOP)
		wantUp = jumping;
		wantDown = sneaking;
	#endif

	if (keys[KEY_CRAFT])
		player->startCrafting((int)player->x, (int)player->y, (int)player->z, Recipe::SIZE_2X2);

	//printf("\n>- %f %f\n", xa, ya);
}

