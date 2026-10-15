#include "network/java/JavaDisplayRenderer.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaLevel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/player/RemotePlayer.h"
#include "client/gui/Font.h"
#include "client/renderer/Textures.h"
#include "client/renderer/Tesselator.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <png.h>
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 GLFWwindow* window=glfwCreateWindow(854,480,"TCPE controlled hologram verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);
 JavaChunkStore chunks;JavaLevel level(new MemoryLevelStorage(),chunks);RemotePlayer camera(&level,false);camera.moveTo(0,0,0,0,0);camera.yRotO=0;camera.xRotO=0;
 auto& dispatcher=*EntityRenderDispatcher::getInstance();dispatcher.textures=&textures;dispatcher.prepare(&level,&font,&camera,&options,1);
 glViewport(0,0,854,480);glClearColor(.045f,.06f,.085f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-6,6,-3.375f,3.375f,-20,20);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef2(180,0,1,0);glEnable2(GL_TEXTURE_2D);glDisable2(GL_LIGHTING);glEnable2(GL_DEPTH_TEST);glEnableClientState2(GL_VERTEX_ARRAY);glEnableClientState2(GL_TEXTURE_COORD_ARRAY);
 JavaGameSession::Entity entity{};entity.kind="ArmorStand";entity.invisible=true;entity.customNameVisible=true;entity.hitbox={{.5,1.975}};entity.customName="§x§2§2§f§f§f§fLABEL";
 assert(javaArmorStandNameHeight(entity)==1.975f);renderJavaDisplay(entity,dispatcher,0,0,0);
 entity.armorStandMarker=true;entity.customName="§x§f§f§2§2§f§fLABEL";assert(javaArmorStandNameHeight(entity)==0.f);renderJavaDisplay(entity,dispatcher,0,0,0);
 glFinish();std::vector<unsigned char> pixels(854*480*4),flipped(pixels.size());glReadPixels(0,0,854,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());for(int row=0;row<480;++row)std::copy(pixels.begin()+row*854*4,pixels.begin()+(row+1)*854*4,flipped.begin()+(479-row)*854*4);
 double normalY=0,markerY=0;int normal=0,marker=0;
 for(int y=0;y<480;++y)for(int x=0;x<854;++x){int p=(y*854+x)*4;int r=flipped[p],g=flipped[p+1],b=flipped[p+2];if(r<80&&g>200&&b>200){normal++;normalY+=y;}if(r>200&&g<80&&b>200){marker++;markerY+=y;}}
 assert(normal>10&&marker>10);assert(std::abs((markerY/marker-normalY/normal)-1.975*480/6.75)<2.);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=854;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argc>1?argv[1]:"display-render.png",0,flipped.data(),0,nullptr));
 assert(glGetError()==GL_NO_ERROR);std::cout<<"Armorstand marker/name height actual GL pixel separation PASS\n";
 EntityRenderDispatcher::destroy();return 0;
}
