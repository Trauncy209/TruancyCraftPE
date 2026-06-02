#include "SimpleChooseLevelScreen.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../components/Button.h"
#include "../components/ImageButton.h"
#include "../../Minecraft.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../SharedConstants.h"
#include "../../../util/StringUtils.h"

static const char SIMPLE_ILLEGAL_FILE_CHARACTERS[] = {
	'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '\"', ':'
};

static std::string sanitizeLevelId(const std::string& name)
{
	std::string levelId = Util::stringTrim(name);
	for (int i = 0; i < (int)(sizeof(SIMPLE_ILLEGAL_FILE_CHARACTERS) / sizeof(char)); ++i)
		levelId = Util::stringReplace(levelId, std::string(1, SIMPLE_ILLEGAL_FILE_CHARACTERS[i]), "");
	if (levelId.empty()) levelId = "World";
	return levelId;
}

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:   bHeader(0),
    bGamemode(0),
    bWorldType(0),
    bWorldOptions(0),
    bOptionsBack(0),
    bOptionCaves(0),
    bOptionRavines(0),
    bOptionWaterLakes(0),
    bOptionLavaLakes(0),
    bOptionWaterSprings(0),
    bOptionLavaSprings(0),
    bOptionBiomeGrassTint(0),
    bOptionTallGrass(0),
    bOptionBetaWorldGeneration(0),
    bOptionModernTerrainGeneration(0),
    bOptionExperimentalFeatures(0),
    bBack(0),
    bCreate(0),
    hasChosen(false),
    inWorldOptions(false),
    levelName(levelName),
    gamemode(GameType::Survival),
    generatorVersion(LGV_ORIGINAL),
    optCaves(false),
    optRavines(false),
    optWaterLakes(false),
    optLavaLakes(false),
    optWaterSprings(false),
    optLavaSprings(false),
    optBiomeGrassTint(true),
    optTallGrass(true),
    optBetaWorldGeneration(false),
    optModernTerrainGeneration(false),
    optExperimentalFeatures(false),
    duplicateWorldName(false),
    worldOptionsScroll(0),
    tLevelName(0, "World name"),
    tSeed(1, "World seed")
{
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
    if (bHeader) delete bHeader;
    delete bGamemode;
    delete bWorldType;
    delete bWorldOptions;
    delete bOptionsBack;
    delete bOptionCaves;
    delete bOptionRavines;
    delete bOptionWaterLakes;
    delete bOptionLavaLakes;
    delete bOptionWaterSprings;
    delete bOptionLavaSprings;
    delete bOptionBiomeGrassTint;
    delete bOptionTallGrass;
    delete bOptionBetaWorldGeneration;
    delete bOptionModernTerrainGeneration;
    delete bOptionExperimentalFeatures;
    delete bBack;
    delete bCreate;
}

void SimpleChooseLevelScreen::applyGeneratorDefaults()
{
    optCaves = LevelSettings::defaultCavesForGenerator(generatorVersion);
    optRavines = LevelSettings::defaultRavinesForGenerator(generatorVersion);
    optWaterLakes = LevelSettings::defaultWaterLakesForGenerator(generatorVersion);
    optLavaLakes = LevelSettings::defaultLavaLakesForGenerator(generatorVersion);
    optWaterSprings = LevelSettings::defaultWaterSpringsForGenerator(generatorVersion);
    optLavaSprings = LevelSettings::defaultLavaSpringsForGenerator(generatorVersion);
    optBiomeGrassTint = true;
    optTallGrass = true;
    optBetaWorldGeneration = false;
    optModernTerrainGeneration = false;
    optExperimentalFeatures = false;
}

void SimpleChooseLevelScreen::refreshWorldOptionLabels()
{
    if (!bOptionCaves) return;
    bOptionCaves->msg = std::string("Caves: ") + (optCaves ? "ON" : "OFF");
    bOptionRavines->msg = std::string("Ravines: ") + (optRavines ? "ON" : "OFF");
    bOptionWaterLakes->msg = std::string("Water Lakes: ") + (optWaterLakes ? "ON" : "OFF");
    bOptionLavaLakes->msg = std::string("Lava Lakes: ") + (optLavaLakes ? "ON" : "OFF");
    bOptionWaterSprings->msg = std::string("Water Springs: ") + (optWaterSprings ? "ON" : "OFF");
    bOptionLavaSprings->msg = std::string("Lava Springs: ") + (optLavaSprings ? "ON" : "OFF");
    bOptionBiomeGrassTint->msg = std::string("Grass Top Biome Color: ") + (optBiomeGrassTint ? "ON" : "OFF");
    bOptionTallGrass->msg = std::string("Tall Grass In Worlds: ") + (optTallGrass ? "ON" : "OFF");
    bOptionBetaWorldGeneration->msg = std::string("Beta World Generation: ") + (optBetaWorldGeneration ? "ON" : "OFF");
    bOptionModernTerrainGeneration->msg = std::string("Extreme Terrain: ") + (optModernTerrainGeneration ? "ON" : "OFF");
    bOptionExperimentalFeatures->msg = std::string("Experimental Features: ") + (optExperimentalFeatures ? "ON" : "OFF");
}

void SimpleChooseLevelScreen::setWorldOptionsVisible(bool visible)
{
    if (bGamemode) { bGamemode->visible = !visible; bGamemode->active = !visible; }
    if (bWorldType) { bWorldType->visible = !visible; bWorldType->active = !visible; }
    if (bWorldOptions) { bWorldOptions->visible = !visible; bWorldOptions->active = !visible; }
    if (bCreate) { bCreate->visible = !visible; bCreate->active = !visible; }

    Button* optionButtons[] = { bOptionsBack, bOptionCaves, bOptionRavines, bOptionWaterLakes, bOptionLavaLakes, bOptionWaterSprings, bOptionLavaSprings, bOptionBiomeGrassTint, bOptionTallGrass, bOptionBetaWorldGeneration, bOptionModernTerrainGeneration, bOptionExperimentalFeatures };
    for (int i = 0; i < 12; ++i) {
        if (optionButtons[i]) {
            optionButtons[i]->visible = visible;
            optionButtons[i]->active = visible;
        }
    }
}

void SimpleChooseLevelScreen::init()
{
	loadLevelSource();
    bHeader = new Touch::THeader(0, "Create World");
    bBack = new ImageButton(2, "");
    {
        ImageDef def;
        def.name = "gui/touchgui.png";
        def.width = 34;
        def.height = 26;
        def.setSrc(IntRectangle(150, 0, (int)def.width, (int)def.height));
        bBack->setImageDef(def, true);
    }

    applyGeneratorDefaults();
    if (minecraft->useTouchscreen()) {
        bGamemode = new Touch::TButton(1, "Survival mode");
        bWorldType = new Touch::TButton(4, "Classic world");
        bWorldOptions = new Touch::TButton(5, "World Options");
        bOptionsBack = new Touch::TButton(6, "Back");
        bOptionCaves = new Touch::TButton(7, "");
        bOptionRavines = new Touch::TButton(8, "");
        bOptionWaterLakes = new Touch::TButton(9, "");
        bOptionLavaLakes = new Touch::TButton(10, "");
        bOptionWaterSprings = new Touch::TButton(11, "");
        bOptionLavaSprings = new Touch::TButton(12, "");
        bOptionBiomeGrassTint = new Touch::TButton(13, "");
        bOptionTallGrass = new Touch::TButton(14, "");
        bOptionBetaWorldGeneration = new Touch::TButton(15, "");
        bOptionModernTerrainGeneration = new Touch::TButton(17, "");
        bOptionExperimentalFeatures = new Touch::TButton(16, "");
        bCreate  = new Touch::TButton(3, "Create");
    } else {
        bGamemode = new Button(1, "Survival mode");
        bWorldType = new Button(4, "Classic world");
        bWorldOptions = new Button(5, "World Options");
        bOptionsBack = new Button(6, "Back");
        bOptionCaves = new Button(7, "");
        bOptionRavines = new Button(8, "");
        bOptionWaterLakes = new Button(9, "");
        bOptionLavaLakes = new Button(10, "");
        bOptionWaterSprings = new Button(11, "");
        bOptionLavaSprings = new Button(12, "");
        bOptionBiomeGrassTint = new Button(13, "");
        bOptionTallGrass = new Button(14, "");
        bOptionBetaWorldGeneration = new Button(15, "");
        bOptionModernTerrainGeneration = new Button(17, "");
        bOptionExperimentalFeatures = new Button(16, "");
        bCreate  = new Button(3, "Create");
    }
    refreshWorldOptionLabels();

    buttons.push_back(bHeader);
    buttons.push_back(bBack);
    buttons.push_back(bGamemode);
    buttons.push_back(bWorldType);
    buttons.push_back(bWorldOptions);
    buttons.push_back(bOptionsBack);
    buttons.push_back(bOptionCaves);
    buttons.push_back(bOptionRavines);
    buttons.push_back(bOptionWaterLakes);
    buttons.push_back(bOptionLavaLakes);
    buttons.push_back(bOptionWaterSprings);
    buttons.push_back(bOptionLavaSprings);
    buttons.push_back(bOptionBiomeGrassTint);
    buttons.push_back(bOptionTallGrass);
    buttons.push_back(bOptionBetaWorldGeneration);
    buttons.push_back(bOptionModernTerrainGeneration);
    buttons.push_back(bOptionExperimentalFeatures);
    buttons.push_back(bCreate);

    tabButtons.push_back(bGamemode);
    tabButtons.push_back(bWorldType);
    tabButtons.push_back(bWorldOptions);
    tabButtons.push_back(bOptionsBack);
    tabButtons.push_back(bOptionCaves);
    tabButtons.push_back(bOptionRavines);
    tabButtons.push_back(bOptionWaterLakes);
    tabButtons.push_back(bOptionLavaLakes);
    tabButtons.push_back(bOptionWaterSprings);
    tabButtons.push_back(bOptionLavaSprings);
    tabButtons.push_back(bOptionBiomeGrassTint);
    tabButtons.push_back(bOptionTallGrass);
    tabButtons.push_back(bOptionBetaWorldGeneration);
    tabButtons.push_back(bOptionModernTerrainGeneration);
    tabButtons.push_back(bOptionExperimentalFeatures);
    tabButtons.push_back(bBack);
    tabButtons.push_back(bCreate);

    textBoxes.push_back(&tLevelName);
    textBoxes.push_back(&tSeed);
    setWorldOptionsVisible(false);
}

void SimpleChooseLevelScreen::setupPositions()
{
    int buttonHeight = bBack->height;
    bBack->x = width - bBack->width;
    bBack->y = 0;

    if (bHeader) {
        bHeader->x = 0;
        bHeader->y = 0;
        bHeader->width = width - bBack->width;
        bHeader->height = buttonHeight;
    }

    int centerX = width / 2;
    tLevelName.width = tSeed.width = 200;
    tLevelName.x = centerX - tLevelName.width / 2;
    tLevelName.y = buttonHeight + 20;
    tSeed.x = tLevelName.x;
    tSeed.y = tLevelName.y + 30;

    bGamemode->width = 170;
    bWorldType->width = 170;
    bWorldOptions->width = 170;
    bGamemode->x = centerX - bGamemode->width / 2;
    bWorldType->x = centerX - bWorldType->width / 2;
    bWorldOptions->x = centerX - bWorldOptions->width / 2;
    {
        int bottomPad = 20;
        int availTop = buttonHeight + 20 + 30 + 10;
        int availBottom = height - bottomPad - bCreate->height - 10;
        int availHeight = availBottom - availTop;
        if (availHeight < 0) availHeight = 0;
        int gap = 20;
        int totalButtonsHeight = bGamemode->height + gap + bWorldType->height + gap + bWorldOptions->height;
        int startY = availTop + (availHeight - totalButtonsHeight) / 2;
        bGamemode->y = startY;
        bWorldType->y = bGamemode->y + bGamemode->height + gap;
        bWorldOptions->y = bWorldType->y + bWorldType->height + gap;
    }

    Button* optionButtons[] = { bOptionCaves, bOptionRavines, bOptionWaterLakes, bOptionLavaLakes, bOptionWaterSprings, bOptionLavaSprings, bOptionBiomeGrassTint, bOptionTallGrass, bOptionBetaWorldGeneration, bOptionModernTerrainGeneration, bOptionExperimentalFeatures };
    int optionsStartY = buttonHeight + 34;
    int optionsGap = 4;
    int colGap = 6;
    int optionWidth = (width - 24 - colGap) / 2;
    if (optionWidth > 156) optionWidth = 156;
    if (optionWidth < 118) optionWidth = 118;
    int leftX = centerX - optionWidth - colGap / 2;
    int rightX = centerX + colGap / 2;
    for (int i = 0; i < 11; ++i) {
        int row = i / 2;
        bool right = (i & 1) != 0;
        optionButtons[i]->width = optionWidth;
        optionButtons[i]->x = right ? rightX : leftX;
        optionButtons[i]->y = optionsStartY + row * (optionButtons[i]->height + optionsGap);
    }
    bOptionsBack->width = 120;
    bOptionsBack->x = centerX - bOptionsBack->width / 2;
    bOptionsBack->y = optionsStartY + 6 * (bOptionsBack->height + optionsGap) + 2;

    bCreate->width = 100;
    bCreate->x = centerX - bCreate->width / 2;
    int bottomPadding = 20;
    bCreate->y = height - bottomPadding - bCreate->height;
}

void SimpleChooseLevelScreen::tick()
{
    if (inWorldOptions) {
        // Options are laid out in two columns on touch now, so no off-screen
        // scrolling state is needed for phone-height displays.
        worldOptionsScroll = 0;
        setupPositions();
    }
    for (auto* tb : textBoxes)
        tb->tick(minecraft);

    if (!inWorldOptions && bCreate) {
        loadLevelSource();
        std::string typedName = Util::stringTrim(tLevelName.text);
        if (typedName.empty()) typedName = "World";
        duplicateWorldName = hasLevelWithName(typedName);
        bCreate->msg = duplicateWorldName ? "Name exists" : "Create";
    }
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
    renderDirtBackground(0);
    glEnable2(GL_BLEND);

    if (!inWorldOptions) {
        const char* gamemodeDesc = NULL;
        if (gamemode == GameType::Survival) {
            gamemodeDesc = "Mobs, health and gather resources";
        } else if (gamemode == GameType::Creative) {
            gamemodeDesc = "Unlimited resources and flying";
        }
        if (gamemodeDesc) {
            drawCenteredString(minecraft->font, gamemodeDesc, width/2, bGamemode->y + bGamemode->height + 4, 0xffcccccc);
        }

        const char* worldTypeDesc = generatorVersion == LGV_INFINITE
            ? "Infinite terrain with chunk streaming"
            : "Original finite 256x256 world";
        drawCenteredString(minecraft->font, worldTypeDesc, width/2, bWorldType->y + bWorldType->height + 4, 0xffcccccc);

        drawString(minecraft->font, "World name:", tLevelName.x, tLevelName.y - Font::DefaultLineHeight - 2, 0xffcccccc);
        drawString(minecraft->font, "World seed:", tSeed.x, tSeed.y - Font::DefaultLineHeight - 2, 0xffcccccc);
        if (duplicateWorldName)
            drawCenteredString(minecraft->font, "A world with this name already exists", width/2, bCreate->y - Font::DefaultLineHeight - 4, 0xffff5555);
    } else {
        drawCenteredString(minecraft->font, "World Options", width/2, bHeader->height + 14, 0xffffffff);
        drawCenteredString(minecraft->font, generatorVersion == LGV_INFINITE ? "Infinite tuning + extras" : "Classic options default off", width/2, bHeader->height + 25, 0xffcccccc);
    }

    for (unsigned int i = 0; i < buttons.size(); i++)
        buttons[i]->render(minecraft, xm, ym);
    if (!inWorldOptions)
        for (unsigned int i = 0; i < textBoxes.size(); i++)
            textBoxes[i]->render(minecraft, xm, ym);

    glDisable2(GL_BLEND);
}

void SimpleChooseLevelScreen::mouseClicked(int x, int y, int buttonNum)
{
    if (inWorldOptions) {
        Screen::mouseClicked(x, y, buttonNum);
        return;
    }

    if (buttonNum == MouseAction::ACTION_LEFT) {
        auto hitBox = [&](TextBox &tb) {
            int top = tb.y - Font::DefaultLineHeight - 4;
            int bottom = tb.y + tb.height;
            int left = tb.x;
            int right = tb.x + tb.width;
            return x >= left && x < right && y >= top && y < bottom;
        };

        bool clickedLevel = hitBox(tLevelName);
        bool clickedSeed  = hitBox(tSeed);

        if (clickedLevel) {
            tLevelName.setFocus(minecraft);
            tSeed.loseFocus(minecraft);
        } else if (clickedSeed) {
            tSeed.setFocus(minecraft);
            tLevelName.loseFocus(minecraft);
        } else {
            tLevelName.loseFocus(minecraft);
            tSeed.loseFocus(minecraft);
        }
    }

    Screen::mouseClicked(x, y, buttonNum);
}

void SimpleChooseLevelScreen::buttonClicked( Button* button )
{
    if (hasChosen)
        return;

    if (button == bGamemode) {
        gamemode ^= 1;
        bGamemode->msg = (gamemode == GameType::Survival) ? "Survival mode" : "Creative mode";
        return;
    }

    if (button == bWorldType) {
        generatorVersion = (generatorVersion == LGV_ORIGINAL) ? LGV_INFINITE : LGV_ORIGINAL;
        bWorldType->msg = (generatorVersion == LGV_INFINITE) ? "Infinite world" : "Classic world";
        applyGeneratorDefaults();
        refreshWorldOptionLabels();
        return;
    }

    if (button == bWorldOptions) {
        inWorldOptions = true;
        worldOptionsScroll = 0;
        tLevelName.loseFocus(minecraft);
        tSeed.loseFocus(minecraft);
        setWorldOptionsVisible(true);
        setupPositions();
        return;
    }
    if (button == bOptionsBack) { inWorldOptions = false; worldOptionsScroll = 0; setWorldOptionsVisible(false); setupPositions(); return; }
    if (button == bOptionCaves) { optCaves = !optCaves; refreshWorldOptionLabels(); return; }
    if (button == bOptionRavines) { optRavines = !optRavines; refreshWorldOptionLabels(); return; }
    if (button == bOptionWaterLakes) { optWaterLakes = !optWaterLakes; refreshWorldOptionLabels(); return; }
    if (button == bOptionLavaLakes) { optLavaLakes = !optLavaLakes; refreshWorldOptionLabels(); return; }
    if (button == bOptionWaterSprings) { optWaterSprings = !optWaterSprings; refreshWorldOptionLabels(); return; }
    if (button == bOptionLavaSprings) { optLavaSprings = !optLavaSprings; refreshWorldOptionLabels(); return; }
    if (button == bOptionBiomeGrassTint) { optBiomeGrassTint = !optBiomeGrassTint; refreshWorldOptionLabels(); return; }
    if (button == bOptionTallGrass) { optTallGrass = !optTallGrass; refreshWorldOptionLabels(); return; }
    if (button == bOptionBetaWorldGeneration) { optBetaWorldGeneration = !optBetaWorldGeneration; if (optBetaWorldGeneration) optModernTerrainGeneration = false; refreshWorldOptionLabels(); return; }
    if (button == bOptionModernTerrainGeneration) { optModernTerrainGeneration = !optModernTerrainGeneration; if (optModernTerrainGeneration) optBetaWorldGeneration = false; refreshWorldOptionLabels(); return; }
    if (button == bOptionExperimentalFeatures) { optExperimentalFeatures = !optExperimentalFeatures; refreshWorldOptionLabels(); return; }

    if (button == bCreate) {
        int seed = getEpochTimeS();
        if (!tSeed.text.empty()) {
            std::string seedString = Util::stringTrim(tSeed.text);
            int tmpSeed;
            if (sscanf(seedString.c_str(), "%d", &tmpSeed) > 0) {
                seed = tmpSeed;
            } else {
                seed = Util::hashCode(seedString);
            }
        }
        // Re-read the storage source at create time so duplicate world names use
        // a new folder instead of writing new settings/player data into an old save.
        loadLevelSource();
        std::string levelName = Util::stringTrim(tLevelName.text);
        if (levelName.empty()) levelName = "World";
        if (hasLevelWithName(levelName)) {
            duplicateWorldName = true;
            if (bCreate) bCreate->msg = "Name exists";
            return;
        }
        std::string levelId = getUniqueLevelName(sanitizeLevelId(levelName));
        while (!minecraft->getLevelSource()->isNewLevelIdAcceptable(levelId))
            levelId += "-";
        LevelSettings settings(seed, gamemode, generatorVersion,
            optCaves, optRavines, optWaterLakes, optLavaLakes, optWaterSprings, optLavaSprings, optBiomeGrassTint, optTallGrass, optBetaWorldGeneration, optModernTerrainGeneration, optExperimentalFeatures);
        minecraft->selectLevel(levelId, levelName, settings);
        minecraft->hostMultiplayer();
        minecraft->setScreen(new ProgressScreen());
        hasChosen = true;
        return;
    }

    if (button == bBack) {
        minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
    }
}

void SimpleChooseLevelScreen::keyPressed(int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE) {
        if (inWorldOptions) {
            inWorldOptions = false;
            setWorldOptionsVisible(false);
        } else {
            minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
        }
        return;
    }
    Screen::keyPressed(eventKey);
}

void SimpleChooseLevelScreen::keyboardNewChar(char inputChar)
{
    if (!inWorldOptions)
        for (auto* tb : textBoxes) tb->handleChar(inputChar);
}

bool SimpleChooseLevelScreen::handleBackEvent(bool isDown) {
	if (!isDown) {
		if (inWorldOptions) {
			inWorldOptions = false;
			setWorldOptionsVisible(false);
		} else {
			minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
		}
	}
	return true; 
}
