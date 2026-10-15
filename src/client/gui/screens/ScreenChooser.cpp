#include "ScreenChooser.h"
#include "StartMenuScreen.h"
#include "SelectWorldScreen.h"
#include "JoinGameScreen.h"
#include "JoinByIPScreen.h"
#include "PauseScreen.h"
#include "RenameMPLevelScreen.h"
#include "IngameBlockSelectionScreen.h"
#include "JavaInventoryScreen.h"
#include "touch/TouchStartMenuScreen.h"
#include "touch/TouchSelectWorldScreen.h"
#include "touch/TouchJoinGameScreen.h"
#include "touch/TouchIngameBlockSelectionScreen.h"

#include "../../Minecraft.h"

Screen* ScreenChooser::createScreen( ScreenId id )
{
 if(id==SCREEN_BLOCKSELECTION&&_mc->javaSession){
  const auto& inventory=_mc->javaSession->inventory;
  if(inventory.windowId==0)return new JavaPlayerInventoryScreen();
  if(inventory.menuKind=="crafting")return new JavaCraftingScreen(inventory.windowId);
  return new JavaInventoryScreen(inventory.windowId);
 }
	Screen* screen = NULL;

#ifdef PLATFORM_DESKTOP
	// Keep TCPE's native menus when selecting desktop gameplay controls.
	const bool useNativeTouchScreens = true;
#else
	const bool useNativeTouchScreens = _mc->useTouchscreen();
#endif
	if (useNativeTouchScreens) {
		switch (id) {
		case SCREEN_STARTMENU:	screen = new Touch::StartMenuScreen();	break;
		case SCREEN_SELECTWORLD:screen = new Touch::SelectWorldScreen();break;
		case SCREEN_JOINGAME:	screen = new Touch::JoinGameScreen();	break;
		case SCREEN_JOINBYIP:	screen = new JoinByIPScreen();	break;
		case SCREEN_PAUSE:	    screen = new PauseScreen(false); break;
		case SCREEN_PAUSEPREV:	screen = new PauseScreen(true);	 break;
		case SCREEN_BLOCKSELECTION:	screen = new Touch::IngameBlockSelectionScreen();	break;

		case SCREEN_NONE:
		default:
			// Do nothing
			break;
		}
	} else {
		switch (id) {
		case SCREEN_STARTMENU:	screen = new StartMenuScreen();  break;
		case SCREEN_SELECTWORLD:screen = new SelectWorldScreen();break;
		case SCREEN_JOINGAME:	screen = new JoinGameScreen();   break;
		case SCREEN_JOINBYIP:	screen = new JoinByIPScreen();   break;
		case SCREEN_PAUSE:	    screen = new PauseScreen(false); break;
		case SCREEN_PAUSEPREV:	screen = new PauseScreen(true);	 break;
		case SCREEN_BLOCKSELECTION:	screen = new IngameBlockSelectionScreen();	break;

		case SCREEN_NONE:
		default:
			// Do nothing
			break;
		}
	}
	return screen;
}

Screen* ScreenChooser::setScreen(ScreenId id)
{
	Screen* screen = createScreen(id);
	_mc->setScreen(screen);
	return screen;
}
