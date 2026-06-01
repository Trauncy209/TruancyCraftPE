#include "FoliageColor.h"
#include "LevelSource.h"
#include "biome/BiomeSource.h"

// Original 0.6.1 foliage tint is also a smooth temperature/rainfall color map.
// Keep that variation, smooth the lookup, and only restrain colors that become
// too blue/cyan or neon for this port's lighting.
bool FoliageColor::useTint = true;

static int FOLIAGE_FALLBACK_PIXELS[256 * 256];
static int FOLIAGE_TUNED_PIXELS[256 * 256];
static int* foliageSourcePixels = nullptr;
static bool foliageFallbackInitialized = false;
static bool foliageTunedInitialized = false;

static int clampColor(int v) {
	if (v < 0) return 0;
	if (v > 255) return 255;
	return v;
}

static float clamp01(float v) {
	if (v < 0.0f) return 0.0f;
	if (v > 1.0f) return 1.0f;
	return v;
}

static int mixChannel(int a, int b, int weightB) {
	return (a * (256 - weightB) + b * weightB + 128) >> 8;
}

static int mixColor(int a, int b, int weightB) {
	int ar = (a >> 16) & 0xff;
	int ag = (a >> 8) & 0xff;
	int ab = a & 0xff;
	int br = (b >> 16) & 0xff;
	int bg = (b >> 8) & 0xff;
	int bb = b & 0xff;
	return (mixChannel(ar, br, weightB) << 16) |
		   (mixChannel(ag, bg, weightB) << 8) |
		   mixChannel(ab, bb, weightB);
}

static int tuneFoliageColor(int color) {
	int r = (color >> 16) & 0xff;
	int g = (color >> 8) & 0xff;
	int b = color & 0xff;

	if (b > g - 22) {
		b = g - 32;
	}
	if (b > 108) {
		b = 108;
	}
	if (g > 176) {
		g = 176;
	}
	if (g < 88) {
		g = 88;
	}
	if (r < 34) {
		r = 34;
	}
	if (r > 158) {
		r = 158;
	}
	if (g > 160 && r < 70) {
		r = (r * 3 + 70) / 4;
		g = (g * 5 + 150) / 6;
	}

	return (clampColor(r) << 16) | (clampColor(g) << 8) | clampColor(b);
}

static void initFoliageFallbackPixels() {
	if (foliageFallbackInitialized) {
		return;
	}
	foliageFallbackInitialized = true;
	for (int y = 0; y < 256; ++y) {
		for (int x = 0; x < 256; ++x) {
			float temp = 1.0f - (x / 255.0f);
			float wet = 1.0f - (y / 255.0f);
			int r = (int)(62 + (1.0f - wet) * 70 + temp * 6);
			int g = (int)(118 + wet * 45 + temp * 5);
			int b = (int)(46 + (1.0f - temp) * 42 + wet * 3);
			FOLIAGE_FALLBACK_PIXELS[(y << 8) | x] = tuneFoliageColor((r << 16) | (g << 8) | b);
		}
	}
}

static void initFoliageTunedPixels() {
	if (foliageTunedInitialized) {
		return;
	}
	foliageTunedInitialized = true;
	int* source = foliageSourcePixels;
	if (!source) {
		initFoliageFallbackPixels();
		source = FOLIAGE_FALLBACK_PIXELS;
	}

	int pass[256 * 256];
	for (int i = 0; i < 256 * 256; ++i) {
		pass[i] = tuneFoliageColor(source[i]);
	}

	for (int y = 0; y < 256; ++y) {
		for (int x = 0; x < 256; ++x) {
			int totalR = 0;
			int totalG = 0;
			int totalB = 0;
			int totalW = 0;
			for (int oy = -1; oy <= 1; ++oy) {
				int sy = y + oy;
				if (sy < 0) sy = 0;
				if (sy > 255) sy = 255;
				for (int ox = -1; ox <= 1; ++ox) {
					int sx = x + ox;
					if (sx < 0) sx = 0;
					if (sx > 255) sx = 255;
					int w = (ox == 0 && oy == 0) ? 4 : ((ox == 0 || oy == 0) ? 2 : 1);
					int c = pass[(sy << 8) | sx];
					totalR += ((c >> 16) & 0xff) * w;
					totalG += ((c >> 8) & 0xff) * w;
					totalB += (c & 0xff) * w;
					totalW += w;
				}
			}
			FOLIAGE_TUNED_PIXELS[(y << 8) | x] =
				((totalR / totalW) << 16) | ((totalG / totalW) << 8) | (totalB / totalW);
		}
	}
}

void FoliageColor::init(int* p) {
	foliageSourcePixels = p;
	foliageTunedInitialized = false;
	initFoliageTunedPixels();
	pixels = FOLIAGE_TUNED_PIXELS;
}

int FoliageColor::get(float temp, float rain) {
	if (!pixels) {
		initFoliageTunedPixels();
		pixels = FOLIAGE_TUNED_PIXELS;
	}

	temp = clamp01(temp);
	rain = clamp01(rain) * temp;

	float fx = (1.0f - temp) * 255.0f;
	float fy = (1.0f - rain) * 255.0f;
	int x0 = (int)fx;
	int y0 = (int)fy;
	if (x0 < 0) x0 = 0;
	if (x0 > 255) x0 = 255;
	if (y0 < 0) y0 = 0;
	if (y0 > 255) y0 = 255;
	int x1 = x0 < 255 ? x0 + 1 : x0;
	int y1 = y0 < 255 ? y0 + 1 : y0;
	int wx = (int)((fx - x0) * 256.0f);
	int wy = (int)((fy - y0) * 256.0f);
	if (wx < 0) wx = 0;
	if (wx > 256) wx = 256;
	if (wy < 0) wy = 0;
	if (wy > 256) wy = 256;

	int top = mixColor(pixels[(y0 << 8) | x0], pixels[(y0 << 8) | x1], wx);
	int bottom = mixColor(pixels[(y1 << 8) | x0], pixels[(y1 << 8) | x1], wx);
	return mixColor(top, bottom, wy);
}

int FoliageColor::getFast(LevelSource* level, int x, int z) {
	if (!level || !level->getBiomeSource()) {
		return get(0.5f, 1.0f);
	}
	BiomeSource* source = level->getBiomeSource();
	source->getBiomeBlock(x, z, 1, 1);
	float temp = source->temperatures ? source->temperatures[0] : 0.5f;
	float rain = source->downfalls ? source->downfalls[0] : 1.0f;
	int c = get(temp, rain);
	int r = (c >> 16) & 0xff;
	int g = (c >> 8) & 0xff;
	int b = c & 0xff;
	unsigned int h = (unsigned int)(x * 1103515245u) ^ (unsigned int)(z * 12345u);
	h ^= h >> 11;
	int shade = (int)(h & 5) - 2;
	r = clampColor(r + shade);
	g = clampColor(g + shade);
	b = clampColor(b + shade - 1);
	return (r << 16) | (g << 8) | b;
}

int FoliageColor::getSmoothed(LevelSource* level, int x, int z) {
	if (!level || !level->getBiomeSource()) {
		return get(0.5f, 1.0f);
	}
	static const int DX[5] = { 0, -5, 5, 0, 0 };
	static const int DZ[5] = { 0, 0, 0, -5, 5 };
	static const int W[5]  = { 6, 3, 3, 3, 3 };
	int totalR = 0;
	int totalG = 0;
	int totalB = 0;
	int totalW = 0;
	BiomeSource* source = level->getBiomeSource();
	for (int i = 0; i < 5; ++i) {
		source->getBiomeBlock(x + DX[i], z + DZ[i], 1, 1);
		float temp = source->temperatures ? source->temperatures[0] : 0.5f;
		float rain = source->downfalls ? source->downfalls[0] : 1.0f;
		int c = get(temp, rain);
		int w = W[i];
		totalR += ((c >> 16) & 0xff) * w;
		totalG += ((c >> 8) & 0xff) * w;
		totalB += (c & 0xff) * w;
		totalW += w;
	}
	int r = totalR / totalW;
	int g = totalG / totalW;
	int b = totalB / totalW;
	unsigned int h = (unsigned int)(x * 1103515245u) ^ (unsigned int)(z * 12345u);
	h ^= h >> 11;
	int shade = (int)(h & 5) - 2;
	r = clampColor(r + shade);
	g = clampColor(g + shade);
	b = clampColor(b + shade - 1);
	return (r << 16) | (g << 8) | b;
}

int* FoliageColor::pixels = nullptr;
