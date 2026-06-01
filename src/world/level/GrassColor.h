#ifndef NET_MINECRAFT_WORLD_LEVEL__GrassColor_H__
#define NET_MINECRAFT_WORLD_LEVEL__GrassColor_H__

//package net.minecraft.world.level;

class GrassColor
{
public:
	static bool useTint;

	static void setUseTint(bool value) {
       useTint = value;
    }

    static void init(int* p);
    static int get(float temp, float rain);
    static int getSmoothed(class LevelSource* level, int x, int z);
    static int getFast(class LevelSource* level, int x, int z);

private:
	static int* pixels;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__GrassColor_H__*/
