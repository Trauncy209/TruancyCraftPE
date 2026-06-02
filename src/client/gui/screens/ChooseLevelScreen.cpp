#include "ChooseLevelScreen.h"
#include <algorithm>
#include <set>
#include "../../Minecraft.h"

void ChooseLevelScreen::init() {
	loadLevelSource();
}

void ChooseLevelScreen::loadLevelSource()
{
	levels.clear();
	LevelStorageSource* levelSource = minecraft->getLevelSource();
	levelSource->getLevelList(levels);
	std::sort(levels.begin(), levels.end());
}

std::string ChooseLevelScreen::getUniqueLevelName( const std::string& level ) {
	std::set<std::string> Set;
	for (unsigned int i = 0; i < levels.size(); ++i)
		Set.insert(levels[i].id);

	std::string s = level;
	if (s.empty()) s = "World";
	while ( Set.find(s) != Set.end() )
		s += "-";
	return s;
}

bool ChooseLevelScreen::hasLevelWithName(const std::string& levelName) const {
	for (unsigned int i = 0; i < levels.size(); ++i) {
		if (levels[i].name == levelName)
			return true;
	}
	return false;
}
