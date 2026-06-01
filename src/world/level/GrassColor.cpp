#include "GrassColor.h"
#include "LevelSource.h"
#include "biome/BiomeSource.h"

// Biome grass tinting in the original game is a 256x256 temperature/rainfall
// color-table lookup. The previous safety pass clamped green too hard, which
// collapsed most biomes into one or two flat colors. This keeps the original
// gradient behavior, only gently pushes bad blue/cyan samples back into natural
// grass, and bilinearly samples the table so nearby biome values fade cleanly.
bool GrassColor::useTint = true;

static int GRASS_FALLBACK_PIXELS[256 * 256];
static int GRASS_TUNED_PIXELS[256 * 256];
static int* grassSourcePixels = nullptr;
static bool grassFallbackInitialized = false;
static bool grassTunedInitialized = false;

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

static int tuneGrassColor(int color) {
	int r = (color >> 16) & 0xff;
	int g = (color >> 8) & 0xff;
	int b = color & 0xff;

	// Keep the source-table variation. Only fix colors that drift into cyan/blue
	// or neon territory. Good warm/dry biome differences are intentionally left.
	if (b > g - 18) {
		b = g - 26;
	}
	if (b > 118) {
		b = 118;
	}
	if (g > 188) {
		g = 188;
	}
	if (g < 96) {
		g = 96;
	}
	if (r < 42) {
		r = 42;
	}
	if (r > 178) {
		r = 178;
	}

	// Very saturated lime greens look artificial on PE's simple lighting. Pull
	// them just a little toward olive without erasing the biome gradient.
	if (g > 168 && r < 82) {
		r = (r * 3 + 82) / 4;
		g = (g * 5 + 158) / 6;
	}

	return (clampColor(r) << 16) | (clampColor(g) << 8) | clampColor(b);
}

static void initGrassFallbackPixels() {
	if (grassFallbackInitialized) {
		return;
	}
	grassFallbackInitialized = true;
	for (int y = 0; y < 256; ++y) {
		for (int x = 0; x < 256; ++x) {
			float temp = 1.0f - (x / 255.0f);
			float wet = 1.0f - (y / 255.0f);
			int r = (int)(78 + (1.0f - wet) * 76 + temp * 12);
			int g = (int)(132 + wet * 42 + temp * 8);
			int b = (int)(54 + (1.0f - temp) * 46 + wet * 4);
			GRASS_FALLBACK_PIXELS[(y << 8) | x] = tuneGrassColor((r << 16) | (g << 8) | b);
		}
	}
}

static void initGrassTunedPixels() {
	if (grassTunedInitialized) {
		return;
	}
	grassTunedInitialized = true;
	int* source = grassSourcePixels;
	if (!source) {
		initGrassFallbackPixels();
		source = GRASS_FALLBACK_PIXELS;
	}

	int pass[256 * 256];
	for (int i = 0; i < 256 * 256; ++i) {
		pass[i] = tuneGrassColor(source[i]);
	}

	// Light table smoothing removes single-pixel discontinuities in the legacy
	// PNG without flattening the whole palette. Runtime lookup also bilinearly
	// samples, so biome edges fade instead of stepping between two obvious tones.
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
			GRASS_TUNED_PIXELS[(y << 8) | x] =
				((totalR / totalW) << 16) | ((totalG / totalW) << 8) | (totalB / totalW);
		}
	}
}

void GrassColor::init(int* p) {
	grassSourcePixels = p;
	grassTunedInitialized = false;
	initGrassTunedPixels();
	pixels = GRASS_TUNED_PIXELS;
}

int GrassColor::get(float temp, float rain) {
	if (!pixels) {
		initGrassTunedPixels();
		pixels = GRASS_TUNED_PIXELS;
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

int GrassColor::getFast(LevelSource* level, int x, int z) {
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
	unsigned int h = (unsigned int)(x * 73428767u) ^ (unsigned int)(z * 912931u) ^ (unsigned int)((x + z) * 42349u);
	h ^= h >> 13;
	h *= 1274126177u;
	int shade = (int)(h & 7) - 3;
	int warmth = (int)((h >> 3) & 3) - 1;
	r = clampColor(r + shade + warmth * 2);
	g = clampColor(g + shade);
	b = clampColor(b + shade - warmth);
	return (r << 16) | (g << 8) | b;
}

int GrassColor::getSmoothed(LevelSource* level, int x, int z) {
	if (!level || !level->getBiomeSource()) {
		return get(0.5f, 1.0f);
	}

	// Fast 4-block fade: center plus four cardinal samples. The previous 25-sample
	// blend faded well but flattened everything and cost too much during chunk
	// rebuilds. This keeps the fade, preserves more biome contrast, and is much
	// cheaper on mobile.
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

	// Subtle deterministic mottling so fields are not one plain carpet. This is
	// color-only, chunk-stable, and intentionally tiny so it does not look noisy.
	unsigned int h = (unsigned int)(x * 73428767u) ^ (unsigned int)(z * 912931u) ^ (unsigned int)((x + z) * 42349u);
	h ^= h >> 13;
	h *= 1274126177u;
	int shade = (int)(h & 7) - 3;          // -3..+4 brightness variation
	int warmth = (int)((h >> 3) & 3) - 1;  // -1..+2 olive/yellow variation
	r = clampColor(r + shade + warmth * 2);
	g = clampColor(g + shade);
	b = clampColor(b + shade - warmth);
	return (r << 16) | (g << 8) | b;
}

int* GrassColor::pixels = nullptr;
