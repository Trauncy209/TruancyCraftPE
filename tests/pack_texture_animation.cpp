#include "network/java/JavaPackFontRuntime.h"
#include "network/java/JavaPackItemModels.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
class PackGameProbe:public Minecraft{public:PackGameProbe():Minecraft(){}};
class PackInventoryProbe:public JavaInventoryScreen{public:PackInventoryProbe():JavaInventoryScreen(7){}using Screen::buttons;};
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
static JavaGameSession::PackModelQuad quad(int texture){JavaGameSession::PackModelQuad q;q.texture=texture;q.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};return q;}
static JavaGameSession::PackFonts bundle(const char* pack,unsigned char red,unsigned char green,unsigned char blue,uint64_t generation=4){
 JavaGameSession::PackFonts b;b.id=pack;b.connection=7;b.generation=generation;JavaGameSession::PackFontTexture texture;texture.width=texture.height=2;for(int i=0;i<4;++i)texture.rgba.insert(texture.rgba.end(),{red,green,blue,255});b.textures.push_back(texture);JavaGameSession::PackItemModel m;m.id="test:flat";m.quads.push_back(quad(0));b.models.push_back(m);return b;
}
static void projection(){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glDisable2(GL_CULL_FACE);glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClearDepthf(1);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static std::vector<unsigned char> pixels(){glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(400*240*4);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static std::array<unsigned char,3> color(const std::vector<unsigned char>& p,int x,int y){size_t i=((239-y)*400+x)*4;return {{p[i],p[i+1],p[i+2]}};}
// Reads actual GPU texels through a quad without blending, including alpha.
static std::array<unsigned char,4> rawPixel(Textures& textures){
 projection();glDisable2(GL_BLEND);glDisable2(GL_ALPHA_TEST);
 auto& model=JavaPackItemModels::models().at("test:flat");
 glPushMatrix2();glTranslatef2(40,40,0);glScalef2(32,32,1);
 JavaPackItemModels::drawGeometry(&textures,model,nullptr,1,true);glPopMatrix2();
 auto p=pixels();size_t i=((239-48)*400+48)*4;
 return {{p[i],p[i+1],p[i+2],p[i+3]}};
}
static JavaGameSession::PackAnimationFrame frame(unsigned char r,unsigned char g,unsigned char b,unsigned char a,unsigned time){JavaGameSession::PackAnimationFrame f;f.time=time;for(int i=0;i<4;++i)f.rgba.insert(f.rgba.end(),{r,g,b,a});return f;}
static JavaGameSession::PackFonts animated(const char* id,bool interpolate,uint64_t generation=4){auto b=bundle(id,255,0,0,generation);JavaGameSession::PackAnimation a;a.texture=0;a.interpolate=interpolate;a.frames={frame(255,0,0,255,2),frame(0,0,255,0,3),frame(0,255,0,255,1)};b.animations.push_back(a);return b;}
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);glfwWindowHint(GLFW_ALPHA_BITS,8);auto* window=glfwCreateWindow(400,240,"Pack animation GPU",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);JavaGameSession session;session.phase="Game";session.resourcePackConnection=7;session.resourcePackGeneration=4;JavaGameSession::ResourcePack a,b;a.id="a";b.id="b";JavaPackFontRuntime runtime;
 auto tick=[&](uint64_t ms){runtime.tickAt(session,&font,&textures,std::chrono::steady_clock::time_point(std::chrono::milliseconds(ms)));};
 auto expect=[&](int r,int g,int b,int a){auto p=rawPixel(textures);assert(std::abs(int(p[0])-r)<=1&&std::abs(int(p[1])-g)<=1&&std::abs(int(p[2])-b)<=1&&std::abs(int(p[3])-a)<=1);};
 session.resourcePacks={a};session.pendingPackFonts={animated("a",false)};tick(1000);expect(255,0,0,255);tick(1049);expect(255,0,0,255);tick(1050);expect(255,0,0,255);tick(1100);expect(0,0,255,0);tick(1249);expect(0,0,255,0);tick(1250);expect(0,255,0,255);tick(1300);expect(255,0,0,255);tick(1600);expect(255,0,0,255);
 // New generation starts at frame zero; interpolation is quantized at 20Hz.
 session.resourcePackGeneration=5;session.pendingPackFonts={animated("a",true,5)};tick(2000);expect(255,0,0,255);tick(2049);expect(255,0,0,255);auto stableTexture=JavaPackItemModels::models().at("test:flat").quads[0].texture;
 GLint beforeBinding=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&beforeBinding);glPixelStorei(GL_UNPACK_ALIGNMENT,8);glEnable2(GL_BLEND);glEnable2(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glBlendFunc2(GL_ONE,GL_ZERO);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.37f);
 tick(2050);GLint afterBinding=0,alignment=0,src=0,dst=0;GLboolean mask=1;GLfloat ref=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&afterBinding);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetFloatv(GL_ALPHA_TEST_REF,&ref);assert(beforeBinding==afterBinding&&alignment==8&&src==GL_ONE&&dst==GL_ZERO&&!mask&&std::abs(ref-.37f)<.001f&&glIsEnabled(GL_BLEND)&&glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_ALPHA_TEST));assert(stableTexture==JavaPackItemModels::models().at("test:flat").quads[0].texture);glPixelStorei(GL_UNPACK_ALIGNMENT,4);expect(128,0,128,128);tick(2100);expect(0,0,255,0);tick(2150);expect(0,85,170,85);tick(2200);expect(0,170,85,170);tick(2250);expect(0,255,0,255);
 // Stale downloads and unrelated pack IDs cannot restart or replace animation.
 auto stale=animated("a",false,4),foreign=animated("a",false,5);foreign.connection=8;session.pendingPackFonts={stale,foreign,animated("unknown",false,5)};tick(2300);expect(255,0,0,255);
 // A graphics reset reinstalls GPU texture and keeps live animation functioning.
 runtime.graphicsReset(&font);assert(!JavaPackItemModels::has("test:flat"));tick(2400);assert(JavaPackItemModels::has("test:flat"));expect(0,0,255,0);tick(2450);expect(0,85,170,85);assert(glGetError()==GL_NO_ERROR);
 // Reverse arrival still uses declared stack priority and complete-stack barrier.
 session.resourcePackGeneration=6;session.resourcePacks={a,b};session.pendingPackFonts={bundle("b",255,255,0,6)};tick(3000);assert(!JavaPackItemModels::has("test:flat"));session.pendingPackFonts={animated("a",true,6)};tick(3000);expect(255,255,0,255);tick(3050);expect(255,255,0,255);
 session.resourcePacks={a};++session.resourcePackRevision;tick(3100);assert(!JavaPackItemModels::has("test:flat"));session.pendingPackFonts={animated("a",false,6)};tick(3100);expect(255,0,0,255);tick(3200);expect(0,0,255,0);
 session.resourcePackGeneration=7;session.resourcePacks.clear();tick(3300);assert(!JavaPackItemModels::has("test:flat"));assert(!textures.hasTexture("java-pack-font/7/6/0/0"));
 session.resourcePacks={a};session.pendingPackFonts={animated("a",false,7)};tick(3400);expect(255,0,0,255);// Repeated selected frames retain independent dwell periods.
 auto repeated=animated("a",false,7);repeated.animations[0].frames={frame(255,0,0,255,1),frame(255,0,0,255,2),frame(0,255,0,255,1)};session.pendingPackFonts={repeated};tick(3400);expect(255,0,0,255);tick(3450);expect(255,0,0,255);tick(3500);expect(255,0,0,255);tick(3550);expect(0,255,0,255);tick(3600);expect(255,0,0,255);
 session.phase="Disconnected";tick(3650);assert(!JavaPackItemModels::has("test:flat"));assert(!textures.hasTexture("java-pack-font/7/7/0/0"));
 session.phase="Game";session.resourcePackGeneration=8;auto invalid=animated("a",false,8);invalid.animations[0].frames[1].time=0;session.pendingPackFonts={invalid};tick(3700);assert(!JavaPackItemModels::has("test:flat")&&!session.packFontError.empty()&&!textures.hasTexture("java-pack-font/7/8/0/0"));
 std::cout<<"Animated texture actual GPU: 20Hz duration/wrap, quiet-tick uploads, RGBA interpolation, stale rejection, complete ordered stacks, pop/reset/scope/disconnect PASS"<<std::endl;std::_Exit(0);
}
