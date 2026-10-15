#include "WeatherLightningRenderer.h"
#include "Tesselator.h"
#include "gles.h"
#include "../../world/level/Level.h"
#include "../../world/level/WeatherLightningGeometry.h"
namespace WeatherLightningRenderer {
void render(Level& level,const Vec3& origin) {
    if(level.isJavaServerLevel() || level.weatherLightning.empty())return;
    bool visible=false;for(const WeatherLightning& bolt:level.weatherLightning)visible=visible||bolt.visible();
    if(!visible)return;
    glDisable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glDisable2(GL_CULL_FACE);glDepthMask(GL_FALSE);
    Tesselator& mesh=Tesselator::instance;mesh.begin();mesh.color(.45f,.45f,.5f,.3f);
    for(const WeatherLightning& bolt:level.weatherLightning) {
        if(!bolt.visible())continue;
        mesh.offset(float(bolt.x-origin.x),float(bolt.y-origin.y),float(bolt.z-origin.z));
        WeatherLightningGeometry::generate(bolt.seed,[&](const WeatherLightningGeometry::Vertex& a,
            const WeatherLightningGeometry::Vertex& b,const WeatherLightningGeometry::Vertex& c,
            const WeatherLightningGeometry::Vertex& d) {
            mesh.vertex(a.x,a.y,a.z);mesh.vertex(b.x,b.y,b.z);mesh.vertex(c.x,c.y,c.z);mesh.vertex(d.x,d.y,d.z);
        });
    }
    mesh.draw();mesh.offset(0,0,0);
    glDepthMask(GL_TRUE);glEnable2(GL_CULL_FACE);glDisable2(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_TEXTURE_2D);glColor4f(1,1,1,1);
}
}
