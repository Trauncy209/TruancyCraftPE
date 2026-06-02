#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__ChooseLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__ChooseLevelScreen_H__

#include "../Screen.h"
#include "../../../world/level/storage/LevelStorageSource.h"

class ChooseLevelScreen: public Screen
{
public:
	void init();

protected:
	std::string getUniqueLevelName(const std::string& level);
	bool hasLevelWithName(const std::string& levelName) const;
	void loadLevelSource();

private:
	LevelSummaryList levels;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__ChooseLevelScreen_H__*/
