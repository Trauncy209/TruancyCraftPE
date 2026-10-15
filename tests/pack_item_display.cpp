#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaDisplayRenderer.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
static void projection(){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-1,1,-.6f,.6f,-2,2);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glDisable2(GL_CULL_FACE);glDisable2(GL_SCISSOR_TEST);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glEnable2(GL_TEXTURE_2D);glDisable2(GL_ALPHA_TEST);glDisable2(GL_BLEND);glClearColor(0,0,0,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static std::vector<unsigned char> pixels(){glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(400*240*4);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static std::array<unsigned char,3> color(const std::vector<unsigned char>& p,int x,int y){size_t i=((239-y)*400+x)*4;return {{p[i],p[i+1],p[i+2]}};}
static std::array<int,4> bounds(const std::vector<unsigned char>& p){std::array<int,4> b{{400,240,-1,-1}};for(int y=0;y<240;++y)for(int x=0;x<400;++x){auto c=color(p,x,y);if(c[0]||c[1]||c[2]){b[0]=std::min(b[0],x);b[1]=std::min(b[1],y);b[2]=std::max(b[2],x);b[3]=std::max(b[3],y);}}return b;}
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,240,"Custom ItemDisplay",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);TextureData image;image.w=image.h=2;image.numBytes=16;image.transparent=true;image.data=new unsigned char[16];std::fill(image.data,image.data+16,255);auto id=textures.assignTexture("item-display-test/white",image);textures.bind(id);glTexParameteri2(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri2(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 JavaGameSession::PackModelQuad q;q.texture=0;q.tint=0;q.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};JavaGameSession::PackItemModel model;model.id="test:boss";model.quads={q};model.rotation={{0,90,0}};model.translation={{80,80,80}};model.scale={{.1f,.1f,.1f}};JavaGameSession::PackModelTint tint;tint.kind="constant";tint.color=0x0000ff;model.tints={tint};JavaPackItemModels::install(model,{id});
 projection();assert(JavaPackItemModels::renderDisplay(&textures,model.id));auto full=pixels();assert((bounds(full)==std::array<int,4>{{100,20,299,219}}));assert((color(full,200,120)==std::array<unsigned char,3>{{0,0,255}}));projection();assert(JavaPackItemModels::renderWorld(&textures,model.id));auto drop=pixels();auto db=bounds(drop);assert(db[2]-db[0]+1==50&&db[3]-db[1]+1==50);
 JavaGameSession::PackDisplayTransform authored;authored.translation={{4,0,0}};authored.scale={{.5f,.5f,.5f}};model.display["firstperson_righthand"]=authored;JavaPackItemModels::install(model,{id});
 projection();assert(JavaPackItemModels::renderContext(&textures,model.id,"firstperson_righthand"));assert((bounds(pixels())==std::array<int,4>{{200,70,299,169}}));
 projection();assert(JavaPackItemModels::renderContext(&textures,model.id,"firstperson_lefthand"));assert((bounds(pixels())==std::array<int,4>{{100,70,199,169}}));
 projection();assert(JavaPackItemModels::renderContext(&textures,model.id,"none"));assert(pixels()==full);
 model.display.clear();JavaPackItemModels::install(model,{id});
 projection();glDisable2(GL_TEXTURE_2D);auto& t=Tesselator::instance;t.begin();t.color(255,0,0);t.vertex(-1,-.6f,1);t.vertex(1,-.6f,1);t.vertex(1,.6f,1);t.vertex(-1,.6f,1);t.draw();glEnable2(GL_TEXTURE_2D);auto blocked=pixels();assert(JavaPackItemModels::renderDisplay(&textures,model.id));assert(pixels()==blocked);
 EntityRenderDispatcher dispatcher;dispatcher.textures=&textures;dispatcher.level=nullptr;assert(dispatcher.getFont()==nullptr);JavaGameSession::Entity snapshot{};snapshot.kind="ItemDisplay";snapshot.display.item.count=1;snapshot.display.item.originalId=-1;snapshot.display.item.itemModel=model.id;snapshot.display.itemTransform=0;snapshot.position={{0,0,0}};
 auto display=[&](){projection();renderJavaDisplay(snapshot,dispatcher,0,0,0);return pixels();};auto shown=display();assert(shown==full); // Custom geometry does not require legacyItemID or a font.
 snapshot.display.brightness=(9<<20)|(4<<4);auto explicitLight=display();auto light=color(explicitLight,200,120);assert(light[0]==0&&light[1]==0&&light[2]>=152&&light[2]<=154);snapshot.display.brightness=-1;
 projection();glEnable2(GL_FOG);glEnable2(GL_LIGHTING);glEnable2(GL_CULL_FACE);glDepthFunc(GL_LESS);glDepthMask(GL_FALSE);glBlendFunc(GL_ONE,GL_ZERO);glAlphaFunc(GL_LESS,.75f);glScissor(1,2,30,40);glEnable2(GL_SCISSOR_TEST);GLfloat before[16],after[16];glGetFloatv(GL_MODELVIEW_MATRIX,before);renderJavaDisplay(snapshot,dispatcher,0,0,0);glGetFloatv(GL_MODELVIEW_MATRIX,after);assert(std::equal(before,before+16,after));assert(glIsEnabled(GL_FOG)&&glIsEnabled(GL_LIGHTING)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_DEPTH_TEST));assert(!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_ALPHA_TEST));GLint state=0;GLboolean mask;glGetIntegerv(GL_DEPTH_FUNC,&state);assert(state==GL_LESS);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);assert(!mask);glGetIntegerv(GL_BLEND_SRC,&state);assert(state==GL_ONE);glGetIntegerv(GL_BLEND_DST,&state);assert(state==GL_ZERO);glGetIntegerv(GL_ALPHA_TEST_FUNC,&state);assert(state==GL_LESS);GLfloat ref=0;glGetFloatv(GL_ALPHA_TEST_REF,&ref);assert(std::abs(ref-.75f)<.001f);GLint clip[4];glGetIntegerv(GL_SCISSOR_BOX,clip);assert(clip[0]==1&&clip[1]==2&&clip[2]==30&&clip[3]==40);
 snapshot.display.translation={{.25,.125,0}};snapshot.display.scale={{.5,.25,1}};snapshot.display.leftRotation={{0,0,.7071067811865476,.7071067811865476}};auto trs=display();assert((bounds(trs)==std::array<int,4>{{225,45,274,144}}));
 snapshot.display.translation={{0,0,0}};snapshot.display.scale={{1,1,1}};snapshot.display.leftRotation={{0,0,0,1}};snapshot.display.item.itemModel.clear();snapshot.display.item.resourceId=model.id;assert(display()==full);snapshot.display.item.itemModel="test:missing";auto missing=display();assert(bounds(missing)[2]==-1);snapshot.display.item.itemModel=model.id;JavaPackItemModels::clear();assert(display()==missing);
 // Front-facing asymmetry verifies vanilla ItemDisplay's additional Y+180.
 JavaGameSession::PackItemModel offset=model;offset.id="test:offset";offset.quads[0].vertices[0][0]=offset.quads[0].vertices[3][0]=8;JavaPackItemModels::install(offset,{id});snapshot.display.item.itemModel=offset.id;auto flipped=display();assert((bounds(flipped)==std::array<int,4>{{100,20,199,219}}));
 std::cout<<"Custom ItemDisplay: full-unit XYZ centre, NONE ignores GUI transform, official Y180, inherited entity TRS, explicitlight+tint, font/legacyID independence, depth occlusion and cleared-model fallback PASS"<<std::endl;std::_Exit(0);
}
