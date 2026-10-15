#ifndef TCPE_WEATHER_LIGHTNING_RENDERER_H
#define TCPE_WEATHER_LIGHTNING_RENDERER_H
class Level;
class Vec3;
namespace WeatherLightningRenderer {
// Call while world camera/modelview is active, after precipitation/translucency.
void render(Level& level,const Vec3& cameraOrigin);
}
#endif
