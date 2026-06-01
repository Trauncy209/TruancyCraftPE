#include "Minecraft.h"
#include "client/player/input/IBuildInput.h"

#if defined(APPLE_DEMO_PROMOTION)
    #define NO_NETWORK
#endif

#if defined(RPI)
	#define CREATORMODE
#endif

#include "../network/RakNetInstance.h"
#include "../network/ClientSideNetworkHandler.h"
#include "../network/ServerSideNetworkHandler.h"
//#include "../network/Packet.h"
#include "../world/entity/player/Inventory.h"
#include "../world/level/chunk/ChunkCache.h"
#include "../world/level/GrassColor.h"
#include "../world/level/FoliageColor.h"
#include "../world/level/tile/Tile.h"
#include "../world/level/storage/LevelStorageSource.h"
#include "../world/level/storage/LevelStorage.h"
#include "../world/level/storage/LevelData.h"
#include "../world/level/LevelSettings.h"
#include "player/input/KeyboardInput.h"
#ifndef STANDALONE_SERVER
#include "player/input/touchscreen/TouchInputHolder.h"
#endif
#include "player/LocalPlayer.h"
#include "gamemode/CreativeMode.h"
#include "gamemode/SurvivalMode.h"
#include "player/LocalPlayer.h"
#ifndef STANDALONE_SERVER
#include "particle/ParticleEngine.h"
#include "gui/Screen.h"
#include "gui/Font.h"
#include "gui/screens/RenameMPLevelScreen.h"
#include "gui/screens/ConsoleScreen.h"
#include "sound/SoundEngine.h"
#endif
#include "../platform/CThread.h"
#include "../platform/input/Mouse.h"
#include "../AppPlatform.h"
#include "../Performance.h"
#include "../LicenseCodes.h"
#include "../util/PerfTimer.h"
#include "../util/PerfRenderer.h"
#include "player/input/MouseBuildInput.h"

#include "../world/Facing.h"

#include "../network/packet/PlaceBlockPacket.h"

#include "player/input/IInputHolder.h"
#ifndef STANDALONE_SERVER
#include "player/input/touchscreen/TouchscreenInput.h"

#include "player/input/ControllerTurnInput.h"
#include "player/input/XperiaPlayInput.h"

#endif

#include "player/input/MouseTurnInput.h"
#include "../world/entity/MobFactory.h"
#include "../world/level/MobSpawner.h"
#include "../util/Mth.h"
#include "../network/packet/InteractPacket.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/PrerenderTilesScreen.h"
#include "renderer/Textures.h"
#include "renderer/LevelRenderer.h"
#include "gui/screens/DeathScreen.h"
#endif

#include "../network/packet/RespawnPacket.h"
#include "IConfigListener.h"
#include "../world/entity/MobCategory.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/FurnaceScreen.h"
#endif
#include "../world/Difficulty.h"
#include "../server/ServerLevel.h"
#ifdef CREATORMODE
#include "../server/CreatorLevel.h"
#endif
#include "../network/packet/AdventureSettingsPacket.h"
#include "../network/packet/SetSpawnPositionPacket.h"
#include "../network/command/CommandServer.h"
#include "gamemode/CreatorMode.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/ArmorScreen.h"
#endif
#include "../world/level/levelgen/synth/ImprovedNoise.h"
#ifndef STANDALONE_SERVER
#include "renderer/tileentity/TileEntityRenderDispatcher.h"
#endif

#ifndef STANDALONE_SERVER
#include "renderer/ptexture/DynamicTexture.h"
#include "renderer/GameRenderer.h"
#include "renderer/ItemInHandRenderer.h"
#include "renderer/LevelRenderer.h"
#include "renderer/entity/EntityRenderDispatcher.h"
#include "gui/Screen.h"
#include "gui/Font.h"
#include "gui/screens/RenameMPLevelScreen.h"
#include "sound/SoundEngine.h"
#endif

static void checkGlError(const char* tag) {
#ifdef GLDEBUG
	while (1) {
        const int errCode = glGetError();
        if (errCode == GL_NO_ERROR) break;

		LOGE("################\nOpenGL-error @ %s : #%d\n", tag, errCode);
	}
#endif /*GLDEBUG*/
}

#ifndef STANDALONE_SERVER
static void initializeBiomeColorTables(Minecraft* minecraft) {
	if (!minecraft || !minecraft->platform()) {
		return;
	}

	TextureData grassTex = minecraft->platform()->loadTexture("misc/grasscolor.png", true);
	if (grassTex.data && grassTex.w == 256 && grassTex.h == 256) {
		GrassColor::init(reinterpret_cast<int*>(grassTex.data));
	}

	TextureData foliageTex = minecraft->platform()->loadTexture("misc/foliagecolor.png", true);
	if (foliageTex.data && foliageTex.w == 256 && foliageTex.h == 256) {
		FoliageColor::init(reinterpret_cast<int*>(foliageTex.data));
	}
}
#endif

/*static*/
const char* Minecraft::progressMessages[] = {
	"Locating server",
	"Building terrain",
	"Preparing",
	"Saving chunks"
};

int Minecraft::customDebugId = Minecraft::CDI_NONE;

#if defined(_MSC_VER)
	#pragma warning( disable : 4355 ) // 'this' pointer in initialization list which is perfectly legal
#endif

bool Minecraft::useAmbientOcclusion = false;

Minecraft::Minecraft()
:	user(NULL),
	level(NULL),
	player(NULL),
	cameraTargetPlayer(NULL),
	levelRenderer(NULL),
	gameRenderer(NULL),
#ifndef STANDALONE_SERVER
	particleEngine(NULL),
	_perfRenderer(NULL),
#endif
	_commandServer(NULL),
#ifndef STANDALONE_SERVER
	textures(NULL),
#endif
	lastTickTime(-1),
	lastTime(0),
	ticksSinceLastUpdate(0),
	gameMode(NULL),
	mouseGrabbed(true),
	missTime(0),
	pause(false),
	_running(false),
	timer(20),
#ifndef STANDALONE_SERVER
	gui(this),
#endif
	netCallback(NULL),
#ifndef STANDALONE_SERVER
	screen(NULL),
	font(NULL),
#endif
	screenMutex(false),
#ifndef STANDALONE_SERVER
	scheduledScreen(NULL),
	hasScheduledScreen(false),
	soundEngine(NULL),
#endif
	ticks(0),
	isGeneratingLevel(false),
	_hasSignaledGeneratingLevelFinished(true),
	generateLevelThread(NULL),
	progressStagePercentage(0),
	progressStageStatusId(0),
	isLookingForMultiplayer(false),
	_licenseId(LicenseCodes::WAIT_PLATFORM_NOT_READY),
	inputHolder(0),
	_supportsNonTouchscreen(false),
#ifndef STANDALONE_SERVER
	screenChooser(this),
#endif
	width(1), height(1),
	//_respawnPlayerTicks(-1),
#ifdef __APPLE__
    _isSuperFast(false),
#endif
	_powerVr(false),
	commandPort(4711),
	reserved_d1(0),reserved_d2(0),
	reserved_f1(0),reserved_f2(0)
{
//#ifdef ANDROID

#if defined(NO_NETWORK)
    raknetInstance = new IRakNetInstance();
#else
	raknetInstance = new RakNetInstance();
#endif
#ifndef STANDALONE_SERVER
	soundEngine = new SoundEngine(20.0f);
	soundEngine->init(this, &options);
#endif
	//setupPieces();
}

Minecraft::~Minecraft()
{
	delete netCallback;
	delete raknetInstance;
#ifndef STANDALONE_SERVER
	delete levelRenderer;
	delete gameRenderer;
	delete particleEngine;

	delete soundEngine;
#endif
	delete gameMode;
#ifndef STANDALONE_SERVER
	delete font;
	delete textures;

	if (screen != NULL) {
		delete screen;
		screen = NULL;
	}
#endif
	if (level != NULL) {
		level->saveGame();
		if (level->getChunkSource())
			level->getChunkSource()->saveAll(true);
		delete level->getLevelStorage();
		delete level;
		level = NULL;
	}

	//delete player;
	delete user;
	delete inputHolder;

	delete storageSource;
	delete _perfRenderer;
	delete _commandServer;

	MobFactory::clearStaticTestMobs();
#ifndef STANDALONE_SERVER
	EntityRenderDispatcher::destroy();
#endif
}

// Only called by server
void Minecraft::selectLevel( const std::string& levelId, const std::string& levelName, const LevelSettings& settings )
{
#if defined(CREATORMODE)
	level = new CreatorLevel(
#else
	level = new ServerLevel(
#endif
		storageSource->selectLevel(levelId, false),
		levelName,
		settings,
		settings.getGeneratorVersion());

	// note: settings is useless beyond this point, since it's
	//       either copied to LevelData (or LevelData read from file)
	setLevel(level, "Generating level");
	setIsCreativeMode(level->getLevelData()->getGameType() == GameType::Creative);
	_running = true;
}

void Minecraft::setLevel(Level* level, const std::string& message /* ="" */, LocalPlayer* forceInsertPlayer /* = NULL */) {
	cameraTargetPlayer = NULL;
	LOGI("Seed is %ld\n", level->getSeed());

	if (level != NULL) {
		level->raknetInstance = raknetInstance;
        gameMode->initLevel(level);

		if (!player && forceInsertPlayer)
		{
			player = forceInsertPlayer;
			player->resetPos(false);
			//level->addEntity(forceInsertPlayer);
		}
		else if (player != NULL) {
			player->resetPos(false);
			if (level != NULL) {
				level->addEntity(player);
			}
		}
		this->level = level;
		_hasSignaledGeneratingLevelFinished = false;
#ifdef STANDALONE_SERVER
		const bool threadedLevelCreation = false;
#else
		const bool threadedLevelCreation = true;
#endif

		if (threadedLevelCreation) {
			// Threaded
			// "Lock"
			isGeneratingLevel = true;
			generateLevelThread = new CThread(Minecraft::prepareLevel_tspawn, this);
		} else {
			// Non-threaded
			generateLevel("Currently not used", level);
		}
    } else {
        player = NULL;
    }

    this->lastTickTime = 0;
	this->_running = true;
}

void Minecraft::leaveGame(bool renameLevel /*=false*/)
{
    if (isGeneratingLevel || !_hasSignaledGeneratingLevelFinished)
        return;

	isGeneratingLevel = false;
	bool saveLevel = level && (!level->isClientSide || renameLevel);

	raknetInstance->disconnect();
	if (saveLevel) {
		// If server or wanting to save level as client, save all unsaved chunks!
		level->getChunkSource()->saveAll(true);
	}

	LOGI("Clearing levels\n");

	cameraTargetPlayer = NULL;
#ifndef STANDALONE_SERVER
	levelRenderer->setLevel(NULL);
	particleEngine->setLevel(NULL);
#endif
	LOGI("Erasing callback\n");
	delete netCallback;
	netCallback = NULL;

	LOGI("Erasing level\n");
	if (level != NULL) {
		delete level->getLevelStorage();
		delete level;
		level = NULL;
	}
	//delete player;
	player = NULL;
	cameraTargetPlayer = NULL;

	_running = false;
#ifndef STANDALONE_SERVER
	if (renameLevel) {
		setScreen(new RenameMPLevelScreen(LevelStorageSource::TempLevelId));
	}
	else
		screenChooser.setScreen(SCREEN_STARTMENU);
#endif
}

void Minecraft::prepareLevel(const std::string& title) {
	LOGI("status: 1\n");
	progressStageStatusId = 1;

	Stopwatch A, B, C, D;
	A.start();

	Stopwatch L;

	// Dont update lights if we load the level (ok, actually just with leveldata version=1.+(?))
	if (!level->isNew())
		level->setUpdateLights(false);

	const bool infiniteWorld = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE;
	int chunkRadius = CHUNK_CACHE_WIDTH / 2;
	int Max = CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH;
	int pp = 0;
	for (int cx = infiniteWorld ? -chunkRadius : 0; cx < (infiniteWorld ? chunkRadius : CHUNK_CACHE_WIDTH); ++cx) {
        for (int cz = infiniteWorld ? -chunkRadius : 0; cz < (infiniteWorld ? chunkRadius : CHUNK_CACHE_WIDTH); ++cz) {
            progressStagePercentage = 100 * pp++ / Max;
			int x = cx * CHUNK_WIDTH + 8;
			int z = cz * CHUNK_WIDTH + 8;
            //printf("level generation progress %d\n", progressStagePercentage);
			B.start();
            level->getTile(x, 64, z);
			B.stop();
			L.start();
			if (level->isNew())
				while (level->updateLights())
					;
			L.stop();
        }
    }
	A.stop();
	level->setUpdateLights(true);

	C.start();
	for (int cx = infiniteWorld ? -chunkRadius : 0; cx < (infiniteWorld ? chunkRadius : CHUNK_CACHE_WIDTH); cx++)
	{
		for (int cz = infiniteWorld ? -chunkRadius : 0; cz < (infiniteWorld ? chunkRadius : CHUNK_CACHE_WIDTH); cz++)
		{
			LevelChunk* chunk = level->getChunk(cx, cz);
			if (chunk && !chunk->createdFromSave)
			{
				chunk->unsaved = false;
				chunk->clearUpdateMap();
			}
		}
	}
	C.stop();

	LOGI("status: 3\n");
	progressStageStatusId = 3;
	if (level->isNew()) {
		level->setInitialSpawn(); // @note: should obviously be called from Level itself
		level->saveLevelData();
		level->getChunkSource()->saveAll(false);
		level->saveGame();
	} else {
		level->saveLevelData();
		level->loadEntities();
	}

	progressStagePercentage = -1;
	progressStageStatusId = 2;
	LOGI("status: 2\n");

	D.start();
	level->prepare();
	D.stop();

	A.print("Generate level: ");
	L.print(" - light: ");
	B.print(" - getTl: ");
	C.print(" - clear: ");
	D.print(" - prepr: ");
	progressStageStatusId = 0;
}

void Minecraft::update() {
	//LOGI("Enter Update\n");

	if (Options::debugGl)
		LOGI(">>>>>>>>>>\n");

	TIMER_PUSH("root");

	//if (level) {
	//	LOGI("numplayers: %d\n", level->players.size());
	//	for (int i = 0; i < level->players.size(); ++i) {
	//		Player* p = level->players[i];
	//		bool inEnt = std::find(level->entities.begin(), level->entities.end(), p) != level->entities.end();
	//		LOGI("  %p, %d, %d - in? %d\n", p, p->entityId, p->owner.ToUint32(p->owner), inEnt);
	//	}
	//}

	if (pause && level != NULL) {
		float lastA = timer.a;
		timer.advanceTime();
		timer.a = lastA;
	} else {
		timer.advanceTime();
	}

	if (raknetInstance) {
		raknetInstance->runEvents(netCallback);
	}

	TIMER_PUSH("tick");
	int toTick = timer.ticks;
	for (int i = 0; i < toTick; ++i, ++ticks)
		tick(i, toTick-1);

	TIMER_POP_PUSH("updatelights");
	if (level && !isGeneratingLevel) {
		level->updateLights();
	}
	TIMER_POP();

	#ifndef STANDALONE_SERVER
		if (gameMode != NULL) gameMode->render(timer.a);
		TIMER_PUSH("sound");
		soundEngine->update(player, timer.a);
		TIMER_POP_PUSH("render");
		gameRenderer->render(timer.a);
		TIMER_POP();
	#else
	CThread::sleep(1);
	#endif
#ifndef STANDALONE_SERVER
	Multitouch::resetThisUpdate();
#endif
	TIMER_POP();
#ifndef STANDALONE_SERVER
	checkGlError("Update finished");

	if (options.renderDebug) {
#ifndef PLATFORM_DESKTOP
		if (!PerfTimer::enabled) {
			PerfTimer::reset();
			PerfTimer::enabled = true;
		}
		_perfRenderer->renderFpsMeter(1);
		checkGlError("render debug");
#endif
	} else {
		PerfTimer::enabled = false;
	}
#endif
	//LOGI("Exit Update\n");
}

void Minecraft::tick(int nTick, int maxTick) {
	if (missTime > 0) missTime--;
#ifndef STANDALONE_SERVER
	if (!screen && player) {
		if (player->health <= 0) {
			setScreen(new DeathScreen());
		}
	}
#endif
	TIMER_PUSH("gameMode");
	if (level && !pause) {
		gameMode->tick();
	}

	TIMER_POP_PUSH("commandServer");
	if (level && _commandServer) {
		_commandServer->tick();
	}

	TIMER_POP_PUSH("input");
	tickInput();
#ifndef STANDALONE_SERVER
	TIMER_POP_PUSH("gui");
	gui.tick();
#endif
	//
	// Ongoing level generation in a (perhaps) different thread. When it's
	// ready, _levelGenerated() is called once and any threads are deleted.
	//
	if (isGeneratingLevel) {
		return;
	} else if (!_hasSignaledGeneratingLevelFinished) {
		if (generateLevelThread) {
			delete generateLevelThread;
			generateLevelThread = NULL;
		}
		_levelGenerated();
	}

	//
	// Normal game loop, run before or efter level generation
	//
	if (level != NULL)
	{
		if (!pause) {
#ifndef STANDALONE_SERVER
			TIMER_POP_PUSH("gameRenderer");
			gameRenderer->tick(nTick, maxTick);

			TIMER_POP_PUSH("levelRenderer");
			levelRenderer->tick();
#endif
			level->difficulty = options.difficulty;
			if (level->isClientSide) level->difficulty = Difficulty::EASY;

			TIMER_POP_PUSH("level");
			level->tickEntities();
			level->tick();
#ifndef STANDALONE_SERVER
			TIMER_POP_PUSH("animateTick");
			if (player) {
				level->animateTick(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
			}
#endif
		}
	}
#ifndef STANDALONE_SERVER
	textures->loadAndBindTexture("terrain.png");
	if (!pause && !(screen && !screen->renderGameBehind())) {
		#if !defined(RPI)
			#ifdef __APPLE__
			if (isSuperFast())
			#endif
			{
			if (nTick == maxTick) {
				TIMER_POP_PUSH("textures");
				textures->tick(true);
			}
			}
		#endif
	}
	TIMER_POP_PUSH("particles");
	particleEngine->tick();
	if (screen) {
		screenMutex = true;
		screen->tick();
		screenMutex = false;
	}

	// @note: fix to keep "isPressed" and "isReleased" as long as necessary.
	//      Most likely keyboard and mouse could/should be reset here as well.
	Multitouch::reset();
#endif
	TIMER_POP();
}

class InputRAII {
public:
	~InputRAII() {
#ifndef STANDALONE_SERVER
		Mouse::reset();
		Keyboard::reset();
#endif
	}
};

void Minecraft::tickInput() {
#ifndef STANDALONE_SERVER
	InputRAII raiiInput;

	if (screen && !screen->passEvents) {
		screenMutex = true;
		screen->updateEvents();
		//screen->updateSetScreen();
		screenMutex = false;
		if (hasScheduledScreen) {
			setScreen(scheduledScreen);
			scheduledScreen = NULL;
			hasScheduledScreen = false;
		}
		return;
	}

	if (!player) {
		return;
	}

#ifdef RPI
	bool mouseDiggable = true;
	bool allowGuiClicks = !mouseGrabbed;
#else
	bool mouseDiggable = !gui.isInside(Mouse::getX(), Mouse::getY());
	bool allowGuiClicks = true;
#endif

	TIMER_PUSH("mouse");
	while (Mouse::next()) {
		//if (Mouse::getButtonState(MouseAction::ACTION_LEFT))
		//	LOGI("mouse-down-at: %d, %d\n", Mouse::getX(), Mouse::getY());
        	int passedTime = getTimeMs() - lastTickTime;
        	if (passedTime > 200) continue; // @note: As long Mouse::clear CLEARS the whole buffer, it's safe to break here
		// But since it might be rewritten anyway (and hopefully there aren't a lot of messages, we just continue.

		const MouseAction& e = Mouse::getEvent();

#ifdef RPI // If clicked when not having focus, get focus @keyboard
		if (!mouseGrabbed) {
			if (!screen && e.data == MouseAction::DATA_DOWN) {
				grabMouse();
			}
		}
#endif

		if (allowGuiClicks && e.action == MouseAction::ACTION_LEFT && e.data == MouseAction::DATA_DOWN) {
			gui.handleClick(MouseAction::ACTION_LEFT, Mouse::getX(), Mouse::getY());
		}

		if (e.action == MouseAction::ACTION_WHEEL) {
			Inventory* v = player->inventory;

			int numSlots = gui.getItemBarSelectionSize();
			if (numSlots <= 0) continue;

			int slot = (v->selected - e.dy + numSlots) % numSlots;
			v->selectSlot(slot);
		}
		/*
		if (mouseDiggable && options.useMouseForDigging) {
			if (Mouse::getEventButton() == MouseAction::ACTION_LEFT && Mouse::getEventButtonState()) {
				handleMouseClick(MouseAction::ACTION_LEFT);
				lastClickTick = ticks;
			}
			if (Mouse::getEventButton() == MouseAction::ACTION_RIGHT && Mouse::getEventButtonState()) {
				handleMouseClick(MouseAction::ACTION_RIGHT);
				lastClickTick = ticks;
			}
		}
		*/
	}

	TIMER_POP_PUSH("keyboard");
	while (Keyboard::next()) {
		int key = Keyboard::getEventKey();
		bool isPressed = (Keyboard::getEventKeyState() == KeyboardAction::KEYDOWN);
		player->setKey(key, isPressed);

		if (isPressed) {
			gui.handleKeyPressed(key);

			#if defined(WIN32) || defined(RPI) || defined (PLATFORM_DESKTOP)//|| defined(_DEBUG) || defined(DEBUG)
				if (key >= '0' && key <= '9') {
					int digit = key - '0';
					int slot = digit - 1;

					if (slot >= 0 && slot < gui.getNumSlots())
						player->inventory->selectSlot(slot);

					#if defined(WIN32)
						if (digit >= 1 && GetAsyncKeyState(VK_CONTROL) < 0) {
							// Set adventure settings here!
							AdventureSettingsPacket p(level->adventureSettings);
							p.toggle((AdventureSettingsPacket::Flags)(1 << slot));
							p.fillIn(level->adventureSettings);
							raknetInstance->send(p);
						}
						if (digit == 0) {
							Pos pos((int)player->x, (int)player->y-1, (int)player->z);
							SetSpawnPositionPacket p(pos);
							raknetInstance->send(p);
						}
					#endif
				}
			#endif
			#if defined(PLATFORM_DESKTOP)
				if (key == Keyboard::KEY_E) {
					screenChooser.setScreen(SCREEN_BLOCKSELECTION);
				}
				if (!screen && key == Keyboard::KEY_T && level) {
					setScreen(new ConsoleScreen());
				}
				if (!screen && key == Keyboard::KEY_O || key == 250) {
					releaseMouse();
				}

				if (key == Keyboard::KEY_F5) {
					options.thirdPersonView = !options.thirdPersonView;
					/*
					ImprovedNoise noise;
					for (int i = 0; i < 16; ++i)
						printf("%d\t%f\n", i, noise.grad2(i, 3, 8));
					*/
				}
			#endif
			#if defined(WIN32)
				if (key == Keyboard::KEY_F) {
					options.isFlying = !options.isFlying;
					player->noPhysics = options.isFlying;
				}


				if (key == Keyboard::KEY_L)
					options.viewDistance = (options.viewDistance + 1) % 4;

				if (key == Keyboard::KEY_U) {
					onGraphicsReset();
					player->heal(100);
				}

				if (key == Keyboard::KEY_B || key == 108) // Toggle the game mode
					setIsCreativeMode(!isCreativeMode());

				if (key == Keyboard::KEY_P) // Step forward in time
					level->setTime( level->getTime() + 1000);

				if (key == Keyboard::KEY_G) {
					setScreen(new ArmorScreen());
					/*
					std::vector<AABB>& boxs = level->getCubes(NULL, AABB(128.1f, 73, 128.1f, 128.9f, 74.9f, 128.9f));
					LOGI("boxes: %d\n", (int)boxs.size());
					*/
				}

				if (key == Keyboard::KEY_Y) {
					textures->reloadAll();
					player->hurtTo(2);
				}
				if (key == Keyboard::KEY_Z || key == 108) {
					for (int i = 0; i < 1; ++i) {
						Mob* mob = NULL;
						int forceId = 0;//MobTypes::Sheep;

						int types[] = {
							MobTypes::Sheep,
							MobTypes::Pig,
							MobTypes::Chicken,
							MobTypes::Cow,
						};

						int mobType = (forceId > 0)? forceId : types[Mth::random(sizeof(types) / sizeof(int))];
						mob = MobFactory::CreateMob(mobType, level);

						//((Animal*)mob)->setAge(-1000);
						float dx = 4 - 8 * Mth::random() + 4 * Mth::sin(Mth::DEGRAD * player->yRot);
						float dz = 4 - 8 * Mth::random() + 4 * Mth::cos(Mth::DEGRAD * player->yRot);
						if (mob && !MobSpawner::addMob(level, mob, player->x + dx, player->y, player->z + dz, Mth::random()*360, 0, true))
							delete mob;
					}
				}

				if (key == Keyboard::KEY_X) {
					const EntityList& entities = level->getAllEntities();
					for (int i = entities.size()-1; i >= 0; --i) {
						Entity* e = entities[i];
						if (!e->isPlayer())
							level->removeEntity(e);
					}
				}

				if (key == Keyboard::KEY_C /*|| key == 4*/) {
					player->inventory->clearInventoryWithDefault();
					// @todo: Add saving here for benchmarking
				}
				if (key == Keyboard::KEY_H) {
					setScreen( new PrerenderTilesScreen() );
				}

				if (key == Keyboard::KEY_O) {
					for (int i = Inventory::MAX_SELECTION_SIZE; i < player->inventory->getContainerSize(); ++i)
						if (player->inventory->getItem(i))
							player->inventory->dropSlot(i, false);
				}
				if (key == Keyboard::KEY_F3) {
					options.renderDebug = !options.renderDebug;
				}
				if (key == Keyboard::KEY_M) {
					options.difficulty = (options.difficulty == Difficulty::PEACEFUL)?
						Difficulty::NORMAL : Difficulty::PEACEFUL;
					//setIsCreativeMode( !isCreativeMode() );
				}

				if (options.renderDebug) {
					if (key >= '0' && key <= '9') {
						_perfRenderer->debugFpsMeterKeyPress(key - '0');
					}
				}
			#endif

			#ifndef PLATFORM_DESKTOP
				if (key == 82)
					pauseGame(false);
			#else
				if (key == Keyboard::KEY_ESCAPE)
					pauseGame(false);
			#endif

			#ifndef OPENGL_ES
				if (key == Keyboard::KEY_P) {
					static bool isWireFrame = false;
					isWireFrame = !isWireFrame;
					glPolygonMode(GL_FRONT, isWireFrame? GL_LINE : GL_FILL);
					//glPolygonMode(GL_BACK, isWireFrame? GL_LINE : GL_FILL);
				}
			#endif
		}
		#ifdef WIN32
			if (key == Keyboard::KEY_M) {
				for (int i = 0; i < 5 * SharedConstants::TicksPerSecond; ++i)
					level->tick();
			}
		#endif
	}

	TIMER_POP_PUSH("handlemouse");

	static bool prevMouseDownLeft = false;

	if (useTouchscreen()) {
		// Touch: gesture recognizer classifies the action type (turn/destroy/build)
		BuildActionIntention bai;
		if (inputHolder && inputHolder->getBuildInput()->tickBuild(player, &bai)) {
			handleBuildAction(&bai);
		}
	} else {
		// Desktop: left mouse = destroy/attack
		if (Mouse::isButtonDown(MouseAction::ACTION_LEFT)) {
			auto baiFlags = BuildActionIntention::BAI_REMOVE | BuildActionIntention::BAI_ATTACK;

			if (!prevMouseDownLeft) baiFlags |= BuildActionIntention::BAI_FIRSTREMOVE;

			BuildActionIntention bai(baiFlags);
			handleBuildAction(&bai);
		}

		prevMouseDownLeft = Mouse::isButtonDown(MouseAction::ACTION_LEFT);

		// Build and use/interact is on same button
		// USPESHNO spizheno
		static int buildHoldTicks = 0;
		if (Mouse::isButtonDown(MouseAction::ACTION_RIGHT)) {
			BuildActionIntention bai(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
			handleBuildAction(&bai);
			if (buildHoldTicks >= 5) buildHoldTicks = 0;

			if (++buildHoldTicks == 1) {
				BuildActionIntention bai(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
				handleBuildAction(&bai);
			}
		} else {
			buildHoldTicks = 0;
		}
	}

	lastTickTime = getTimeMs();

	// we have (hopefully) handled the keyboard & mouse queue and it
	// can now be emptied. If wanted, the reset could be changed to:
	// index -= numRead; // then this code doesn't have to be placed here
	// + it prepares for tick not handling all or any events.
	// update: RAII'ing instead, see above
	//Keyboard::reset();
	//Mouse::reset();

	TIMER_POP();
#endif
}

void Minecraft::handleBuildAction(BuildActionIntention* action) {
#ifndef STANDALONE_SERVER
	if (action->isRemove()) {
		if (missTime > 0) return;
		player->swing();
	}
	if(player->isUsingItem())
		return;
    bool mayUse = true;

	if (!hitResult.isHit()) {
		if (action->isRemove() && !gameMode->isCreativeType()) {
			missTime = 10;
		}
    } else if (hitResult.type == ENTITY) {
        if (action->isAttack()) {
			player->swing();
			//LOGI("attacking!\n");
			InteractPacket packet(InteractPacket::Attack, player->entityId, hitResult.entity->entityId);
			raknetInstance->send(packet);
            gameMode->attack(player, hitResult.entity);
		} else if (action->isInteract()) {
			if (hitResult.entity->interactPreventDefault())
				mayUse = false;
			//LOGI("interacting!\n");
			InteractPacket packet(InteractPacket::Interact, player->entityId, hitResult.entity->entityId);
			raknetInstance->send(packet);
            gameMode->interact(player, hitResult.entity);
        }
    } else if (hitResult.type == TILE) {
		int x = hitResult.x;
        int y = hitResult.y;
        int z = hitResult.z;
        int face = hitResult.f;

		int oldTileId = level->getTile(x, y, z);
        Tile* oldTile = Tile::tiles[oldTileId];

		//bool tryDestroyBlock = false;

		if (action->isRemove()) {
			if (!oldTile)
				return;

			//LOGI("tile: %s - %d, %d, %d. b: %f - %f\n", oldTile->getDescriptionId().c_str(), x, y, z, oldTile->getBrightness(level, x, y, z), oldTile->getBrightness(level, x, y+1, z));
            level->extinguishFire(x, y, z, hitResult.f);

			if (action->isFirstRemove()) {
				gameMode->startDestroyBlock(x, y, z, hitResult.f);
			} else {
				gameMode->continueDestroyBlock(x, y, z, hitResult.f);
			}

			particleEngine->crack(x, y, z, hitResult.f);
			player->swing();
        }
		else {
			ItemInstance* item = player->inventory->getSelected();
 			if (gameMode->useItemOn(player, level, item, x, y, z, face, hitResult.pos)) {
			    mayUse = false;
			    player->swing();
			#ifdef RPI
				} else if (item && item->id == ((Item*)Item::sword_iron)->id) {
					player->swing();
			#endif
			}
			if (item && item->count <= 0) {
				player->inventory->clearSlot(player->inventory->selected);
			}
			//} else if (item && item->count != oldCount) {
			//	  gameRenderer->itemInHandRenderer->itemPlaced();
			//}
		}
	}
	if (mayUse && action->isInteract()) {
		ItemInstance* item = player->inventory->getSelected();
		if (item && !player->isUsingItem()) {
			if (gameMode->useItem(player, level, item)) {
				gameRenderer->itemInHandRenderer->itemUsed();
			}
			if (item && item->count <= 0) {
				player->inventory->clearSlot(player->inventory->selected);
			}
		}
	}
#endif
}

bool Minecraft::isOnlineClient()
{
    return (level != NULL && level->isClientSide);
}

bool Minecraft::isOnline()
{
	return netCallback != NULL;
}

void Minecraft::pauseGame(bool isBackPaused) {
#ifndef STANDALONE_SERVER
	if (screen != NULL) return;
	screenChooser.setScreen(isBackPaused? SCREEN_PAUSEPREV : SCREEN_PAUSE);
#endif
}
void Minecraft::gameLostFocus() {
#ifndef STANDALONE_SERVER
	if(screen != NULL) {
		screen->lostFocus();
	}
#endif
}


void Minecraft::setScreen( Screen* screen )
{
#ifndef	STANDALONE_SERVER
	Mouse::reset();
	Multitouch::reset();
	Multitouch::resetThisUpdate();

	if (screenMutex) {
		hasScheduledScreen = true;
		scheduledScreen = screen;
		return;
	}

	if (screen != NULL && screen->isErrorScreen())
		return;
	if (screen == NULL && level == NULL)
		screen = screenChooser.createScreen(SCREEN_STARTMENU);

	if (this->screen != NULL) {
		this->screen->removed();
		delete this->screen;
	}

	this->screen = screen;
	if (screen != NULL) {
		releaseMouse();
		//ScreenSizeCalculator ssc = new ScreenSizeCalculator(options, width, height);
		int screenWidth = (int)(width * Gui::InvGuiScale); //ssc.getWidth();
		int screenHeight = (int)(height * Gui::InvGuiScale); //ssc.getHeight();
		screen->init(this, screenWidth, screenHeight);

		if (screen->isInGameScreen() && level) {
			level->saveLevelData();
            level->saveGame();
        }

		//noRender = false;
	} else {
		grabMouse();
	}
#endif
}

void Minecraft::grabMouse()
{
#ifndef STANDALONE_SERVER
	if (mouseGrabbed) return;
	mouseGrabbed = true;
	mouseHandler.grab();
	//setScreen(NULL);
#endif
}

void Minecraft::releaseMouse()
{
#ifndef STANDALONE_SERVER
	if (!mouseGrabbed) {
		return;
	}
	if (player) {
		player->releaseAllKeys();
	}
	mouseGrabbed = false;
	mouseHandler.release();
#endif
}

bool Minecraft::useTouchscreen() {
#ifdef RPI
	return false;
#endif
	return options.useTouchScreen || !_supportsNonTouchscreen;
}
bool Minecraft::supportNonTouchScreen() {
	return _supportsNonTouchscreen;
}
void Minecraft::init()
{
	options.minecraft = this;
	options.initDefaultValues();
#ifndef STANDALONE_SERVER
	checkGlError("Init enter");

	_supportsNonTouchscreen = !platform()->supportsTouchscreen();

	LOGI("IS TOUCHSCREEN? %d\n", options.useTouchScreen);

	textures = new Textures(&options, platform());
	initializeBiomeColorTables(this);
	textures->addDynamicTexture(new WaterTexture());
	textures->addDynamicTexture(new WaterSideTexture());
	gui.texturesLoaded(textures);

	levelRenderer = new LevelRenderer(this);
	gameRenderer = new GameRenderer(this);
	particleEngine = new ParticleEngine(level, textures);

	// Platform specific initialization here
	font = new Font(&options, "font/default8.png", textures);

	_perfRenderer = new PerfRenderer(this, font);

	checkGlError("Init complete");
#endif

	user = new User("TestUser", "");
	setIsCreativeMode(false); // false means it's Survival Mode
	reloadOptions();

}

void Minecraft::setSize(int w, int h) {
#ifndef STANDALONE_SERVER
    transformResolution(&w, &h);

	width  = w;
	height = h;

	// determine gui scale, optionally overriding auto
	if (options.guiScale != 0) {
		// manual selection: 1->small, 2->normal, 3->large, 4->larger
		switch (options.guiScale) {
		case 1: Gui::GuiScale = 2.0f; break;
		case 2: Gui::GuiScale = 3.0f; break;
		case 3: Gui::GuiScale = 4.0f; break;
		case 4: Gui::GuiScale = 5.0f; break; // bigger than large
		default: Gui::GuiScale = 1.0f; break; // auto
		}
	} else {
		// auto compute from resolution
		if (width >= 1000) {
        #ifdef __APPLE__
            Gui::GuiScale = (width > 2000)? 8.0f : 4.0f;
        #else
            Gui::GuiScale = 4.0f;
        #endif
    }
		else if (width >= 800) {
#ifdef __APPLE__
        Gui::GuiScale = 4.0f;
#else
		Gui::GuiScale = 3.0f;
#endif
    }
		else if (width >= 400)
			Gui::GuiScale = 2.0f;
		else
			Gui::GuiScale = 1.0f;
	}

	Gui::InvGuiScale = 1.0f / Gui::GuiScale;
	int screenWidth  = (int)(width  * Gui::InvGuiScale);
	int screenHeight = (int)(height * Gui::InvGuiScale);

	if (platform()) {
		float touchScalePercent = options.getProgressValue(&Options::Option::PIXELS_PER_MILLIMETER);
		float basePixelsPerMillimeter = platform()->getPixelsPerMillimeter();
		float pixelsPerMillimeter = basePixelsPerMillimeter * (0.25f + (touchScalePercent / 100.0f) * 1.75f);
		pixelCalc.setPixelsPerMillimeter(pixelsPerMillimeter);
		pixelCalcUi.setPixelsPerMillimeter(pixelsPerMillimeter * Gui::InvGuiScale);
	}

	Config config = createConfig(this);
	gui.onConfigChanged(config);

	if (screen)
		screen->setSize(screenWidth, screenHeight);

	if (inputHolder)
		inputHolder->onConfigChanged(config);
	//LOGI("Setting size: %d, %d: %f\n", width, height, Gui::InvGuiScale);

#ifdef WIN32
	char resbuf[128];
	sprintf(resbuf, "            %d x %d @ scale %.2f", width, height, Gui::GuiScale);
	//gui.addMessage(resbuf);
#endif
#endif /* STANDALONE_SERVER */
}

void Minecraft::reloadOptions() {
	options.update();
	options.save();
	bool wasTouchscreen = options.useTouchScreen;
	options.useTouchScreen = useTouchscreen();
	options.save();

	if ((wasTouchscreen != options.useTouchScreen) || (inputHolder == 0))
		_reloadInput();

	user->name = options.username;

	LOGI("Reloading-options\n");

    // @todo @fix Android and iOS behaves a bit differently when leaving
    //            an options screen (Android recreates OpenGL surface)
    setSize(width, height);
}

void Minecraft::_reloadInput() {
#ifndef STANDALONE_SERVER
	delete inputHolder;

#ifdef PLATFORM_DESKTOP
	const bool useTouchHolder = false;
#else
	const bool useTouchHolder = useTouchscreen();
#endif
	if (useTouchHolder) {
		if (options.useClassicControls) {
			inputHolder = new TouchInputHolder(this, &options, true);
		} else {
			inputHolder = new TouchInputHolder(this, &options);
		}
	} else {
		#if defined(ANDROID) || defined(__APPLE__)
			inputHolder = new CustomInputHolder(
				new XperiaPlayInput(&options),
				new ControllerTurnInput(2, ControllerTurnInput::MODE_DELTA),
				new IBuildInput());
		#else
			inputHolder = new CustomInputHolder(
				new KeyboardInput(&options),
				new MouseTurnInput(MouseTurnInput::MODE_DELTA, width/2, height/2),
				new MouseBuildInput());
		#endif
	}

	mouseHandler.setTurnInput(inputHolder->getTurnInput());
	if (level && player) {
		player->input = inputHolder->getMoveInput();
	}
#endif
}


//
// Multiplayer
//
void Minecraft::locateMultiplayer() {
	isLookingForMultiplayer = true;

	raknetInstance->pingForHosts(19132);
	netCallback = new ClientSideNetworkHandler(this, raknetInstance);
}

void Minecraft::cancelLocateMultiplayer() {
	isLookingForMultiplayer = false;

	raknetInstance->stopPingForHosts();

	delete netCallback;
	netCallback = NULL;
}

bool Minecraft::joinMultiplayer( const PingedCompatibleServer& server )
{
	if (isLookingForMultiplayer && netCallback) {
		isLookingForMultiplayer = false;
		return raknetInstance->connect(server.address.ToString(false), server.address.GetPort());
	}
	return false;
}

void Minecraft::hostMultiplayer(int port) {
    // Tear down last instance
    raknetInstance->disconnect();
    delete netCallback;
    netCallback = NULL;

#if !defined(NO_NETWORK)
	netCallback = new ServerSideNetworkHandler(this, raknetInstance);
    #ifdef STANDALONE_SERVER
        raknetInstance->host(user->name, port, 16);
    #else
        raknetInstance->host(user->name, port);
    #endif
#endif
}

//
// Level generation
//
/*static*/

void* Minecraft::prepareLevel_tspawn(void *p_param)
{
	Minecraft* mc = (Minecraft*) p_param;
	mc->generateLevel("Currently not used", mc->level);
	return 0;
}

void Minecraft::generateLevel( const std::string& message, Level* level )
{
	Stopwatch s;
	s.start();
	prepareLevel(message);
	s.stop();
	s.print("Level generated: ");

	// "Unlock"
	isGeneratingLevel = false;
}

void Minecraft::_levelGenerated()
{
#ifndef STANDALONE_SERVER
	if (player == NULL) {
		player = (LocalPlayer*) gameMode->createPlayer(level);
		gameMode->initPlayer(player);
	}

	if (player) {
		player->input = inputHolder->getMoveInput();
	}

	if (levelRenderer != NULL) levelRenderer->setLevel(level);
	if (particleEngine != NULL) particleEngine->setLevel(level);

	gameMode->adjustPlayer(player);
	gui.onLevelGenerated();
#endif

	level->validateSpawn();
	const bool hadSavedPlayerData = level->getLevelData()->getLoadedPlayerTag() != NULL || level->getLevelData()->playerDataVersion == 1;
	level->loadPlayer(player, true);
	// If we are client side, we trust the server to have given us a correct position.
	// For local saved worlds, also trust the stored player position instead of always
	// running collision-based resetPos(), which can shove underground saves upward
	// while chunks/collision are still settling during load.
	if (player && !level->isClientSide && (!hadSavedPlayerData || level->isNew())) {
		player->resetPos(false);
	}

	this->cameraTargetPlayer = player;

	if (raknetInstance->isServer())
		raknetInstance->announceServer(user->name);

	if (netCallback) {
		netCallback->levelGenerated(level);
	}

#if defined(WIN32) || defined(RPI)
	if (_commandServer) {
		delete _commandServer;
	}
	_commandServer = new CommandServer(this);
	_commandServer->init(commandPort);
#endif

	// Hack to (hopefully) get the players to show (note: in LevelListener
	// instead, since adding yourself always generates a entityAdded)
	//EntityRenderDispatcher::getInstance()->onGraphicsReset();
	_hasSignaledGeneratingLevelFinished = true;
}

Player* Minecraft::respawnPlayer(int playerId) {
	for (unsigned int i = 0; i < level->players.size(); ++i) {
		if (level->players[i]->entityId == playerId) {
			resetPlayer(level->players[i]);
			return level->players[i];
		}
	}
	return NULL;
}

void Minecraft::resetPlayer(Player* player) {
	level->validateSpawn();
	player->reset();

	Pos p;
	if(player->hasRespawnPosition()) {
		p = player->getRespawnPosition();
	}
	else {
		p = level->getSharedSpawnPos();
	}
	player->setPos((float)p.x + 0.5f, (float)p.y + 1.0f, (float)p.z + 0.5f);
	player->resetPos(true);

	if (isCreativeMode())
		player->inventory->clearInventoryWithDefault();
}

void Minecraft::respawnPlayer() {
	// RESET THE FRACKING PLAYER HERE
	//bool slowCheck = false;
	//for (int i = 0; i < level->entities.size(); ++i)
	//	if (level->entities[i] == player) slowCheck = true;
	//
	//LOGI("Has entity? %d, %d\n", level->getEntity(player->entityId), slowCheck);

	resetPlayer(player);

	// tell server (or other client) that we re-spawned
	RespawnPacket packet(player);
	raknetInstance->send(packet);
}

void Minecraft::onGraphicsReset()
{
#ifndef STANDALONE_SERVER
	textures->clear();

	font->onGraphicsReset();
	gui.onGraphicsReset();

	if (levelRenderer) levelRenderer->onGraphicsReset();
	if (gameRenderer) gameRenderer->onGraphicsReset();

	EntityRenderDispatcher::getInstance()->onGraphicsReset();
	TileEntityRenderDispatcher::getInstance()->onGraphicsReset();
#endif
}

int Minecraft::getProgressStatusId() {
	return progressStageStatusId;
}

const char* Minecraft::getProgressMessage()
{
	return progressMessages[progressStageStatusId];
}

bool Minecraft::isLevelGenerated()
{
	return level != NULL && !isGeneratingLevel;
}

LevelStorageSource* Minecraft::getLevelSource()
{
	return storageSource;
}

int Minecraft::getLicenseId() {
	if (!LicenseCodes::isReady(_licenseId))
		_licenseId = platform()->checkLicense();
	return _licenseId;
}

void Minecraft::audioEngineOn() {
#ifndef STANDALONE_SERVER
    soundEngine->enable(true);
#endif
}
void Minecraft::audioEngineOff() {
#ifndef STANDALONE_SERVER
    soundEngine->enable(false);
#endif
}

void Minecraft::setIsCreativeMode(bool isCreative)
{
#ifdef CREATORMODE
	delete gameMode;
	gameMode = new CreatorMode(this);
	_isCreativeMode = true;
#else
	if (!gameMode || isCreative != _isCreativeMode)
	{
		delete gameMode;
		if (isCreative) gameMode = new CreativeMode(this);
		else			gameMode = new SurvivalMode(this);
		_isCreativeMode = isCreative;
	}
#endif
	if (player)
		gameMode->initAbilities(player->abilities);
}

bool Minecraft::isCreativeMode() {
	return _isCreativeMode;
}

void Minecraft::toggleGameMode(bool persistToLevel)
{
	const bool makeCreative = !isCreativeMode();

	// Keep this hot-path intentionally tiny. The old implementation did a
	// level.dat save from the UI click while the local RakNet host/client and GL
	// renderer were live; the crash log shows Scudo heap corruption surfacing in
	// RakPeer::RunUpdateCycle immediately after switching modes. Do not touch the
	// network save/packet path here. Persisted mode is saved by the normal level
	// save path when leaving/pausing.
	setIsCreativeMode(makeCreative);

	if (level && level->getLevelData()) {
		level->getLevelData()->setGameType(makeCreative ? GameType::Creative : GameType::Survival);
		level->getLevelData()->setSpawnMobs(true);
	}

	if (player && player->inventory) {
		// The player inventory object is kept alive across the in-game toggle.
		// Keep its container-mode flag in sync too, or Survival chest pulls still
		// behave like Creative until save/quit/rejoin constructs a new inventory.
		player->inventory->setCreativeMode(makeCreative);
		if (makeCreative) {
			player->inventory->clearInventoryWithDefault();
		} else {
			// Do not carry the Creative drawer into Survival.  Players can still
			// intentionally stock a chest in Creative first, then switch to Survival
			// and retrieve those chest contents normally.
			player->inventory->clearInventoryEmpty();
		}
	}
}

bool Minecraft::isKindleFire(int kindleVersion) {
	if (kindleVersion != 1)
		return false;

	std::string model = platform()->getPlatformStringVar(PlatformStringVars::DEVICE_BUILD_MODEL);
	std::string modelLower(model);
	std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), tolower);

	return (modelLower.find("kindle") != std::string::npos) && (modelLower.find("fire") != std::string::npos);
}

bool Minecraft::transformResolution(int* w, int* h)
{
	bool changed = false;

	// Kindle Fire 1: reporting wrong height in
	// certain cases (e.g. after screen lock)
	if (isKindleFire(1) && *h >= 560 && *h <= 620) {
		*h = 580;
		changed = true;
	}

	return changed;
}

ICreator* Minecraft::getCreator()
{
#ifdef CREATORMODE
	return ((CreatorMode*)gameMode)->getCreator();
#else
	return NULL;
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, bool value ) {
	if(netCallback != NULL && option == &Options::Option::SERVER_VISIBLE) {
		ServerSideNetworkHandler* ss = (ServerSideNetworkHandler*) netCallback;
		ss->allowIncomingConnections(value);
	}
#ifndef STANDALONE_SERVER
	if (option == &Options::Option::CLASSIC_CONTROLS || option == &Options::Option::USE_TOUCHSCREEN || option == &Options::Option::USE_TOUCH_JOYPAD) {
		_reloadInput();
		setSize(width, height);
	}
	if ((option == &Options::Option::GRAPHICS || option == &Options::Option::AMBIENT_OCCLUSION) && levelRenderer != NULL) {
		levelRenderer->allChanged();
	}
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, float value ) {
	if(option == &Options::Option::DEBUG_TICK_SPEED) {
		if (value < 1.0f) value = 1.0f;
		if (value > 20.0f) value = 20.0f;
		timer.timeScale = value;
	}
#ifndef STANDALONE_SERVER
	if(option == &Options::Option::PIXELS_PER_MILLIMETER) {
		float basePixelsPerMillimeter = platform()->getPixelsPerMillimeter();
		float pixelsPerMillimeter = basePixelsPerMillimeter * (0.25f + (value / 100.0f) * 1.75f);
		pixelCalcUi.setPixelsPerMillimeter(pixelsPerMillimeter * Gui::InvGuiScale);
		pixelCalc.setPixelsPerMillimeter(pixelsPerMillimeter);
		setSize(width, height);
	}
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, int value ) {
    if(option == &Options::Option::GUI_SCALE) {
        // reapply screen scaling using current window size
        setSize(width, height);
    }
}
