#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "network/java/JavaRichText.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <png.h>
#include <thread>
#include "network/java/JavaLocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:explicit InventoryProbe(int id=0):JavaInventoryScreen(id){}using Screen::buttons;};
static std::string sent;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* text,size_t length){sent.assign(text,length);return true;}
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native inventory layout",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();

 auto* geometryLevel=new JavaLevel(new MemoryLevelStorage(),game->javaSession->chunks);auto* localGeometry=new JavaLocalPlayer(game,geometryLevel,nullptr,0);game->javaSession->hasPosition=true;for(double feet:{-50.25,64.,300.75}){game->javaSession->position={{2.,feet+localGeometry->heightOffset,3.}};localGeometry->moveTo(2.f,float(feet),3.f,0,0);assert(std::abs(localGeometry->bb.y0-feet)<.001);localGeometry->travel(0,0);assert(std::abs(localGeometry->bb.y0-feet)<.001);assert(std::abs(localGeometry->y-(feet+localGeometry->heightOffset))<.001);}delete localGeometry;delete geometryLevel;sent.clear();
 const std::string customText=u8"\ue000\ue001";const int originalWidth=font.widthJava(customText);textures.loadAndBindTexture("font/default8.png");GLint glyphTexture=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&glyphTexture);Font::PackGlyph packed;packed.texture=glyphTexture;packed.u=1.f/16;packed.v=4.f/16;packed.uw=packed.vh=1.f/16;packed.width=packed.height=8;packed.ascent=8;packed.advance=7;font.setPackGlyph(0xe000,packed);Font::PackGlyph spacer;spacer.advance=-3;font.setPackGlyph(0xe001,spacer);assert(font.widthJava(customText)==4);assert(font.javaGlyphAdvance(0xe001)==-3);Font::PackGlyph named=packed;named.advance=17;font.setPackGlyph(0xe000,named,"oraxen:shift");assert(font.widthJava(u8"§zoraxen:shift;\ue000")==17);assert(font.widthJava(u8"§zminecraft:default;\ue000")==7);font.clearPackGlyphs();assert(font.widthJava(customText)==originalWidth);
 GLuint testTexture=0;glGenTextures(1,&testTexture);glBindTexture(GL_TEXTURE_2D,testTexture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);const unsigned char testPixels[]={255,255,255,255,255,255,255,0};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,testPixels);Font::PackGlyph gpuGlyph;gpuGlyph.texture=testTexture;gpuGlyph.uw=gpuGlyph.vh=1;gpuGlyph.width=8;gpuGlyph.height=12;gpuGlyph.ascent=10;gpuGlyph.advance=8;font.setPackGlyph(0xe002,gpuGlyph,"test:font");Font::PackGlyph gpuSpace;gpuSpace.advance=-4;font.setPackGlyph(0xe003,gpuSpace,"test:font");
 auto gpuFrame=[&](const std::string& value){glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);font.drawJava(value,20,20,0xffffffff);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 auto sample=[](const std::vector<unsigned char>& pixels,int x,int y,int channel){return pixels[((479-y)*800+x)*4+channel];};auto bitmap=gpuFrame(u8"§ztest:font;§c\ue002");assert(sample(bitmap,42,40,0)==255&&sample(bitmap,42,40,1)==85&&sample(bitmap,42,40,2)==85);assert(sample(bitmap,52,40,0)==0);assert(sample(bitmap,42,32,0)==0);auto shifted=gpuFrame(u8"§ztest:font;§c\ue002\ue003\ue002");assert(sample(shifted,52,40,0)==255);auto mixed=gpuFrame(u8"§ztest:font;§c\ue002§zminecraft:default;A");auto baseline=gpuFrame(u8"§cA");for(int yy=0;yy<16;++yy)for(int xx=0;xx<16;++xx)for(int channel=0;channel<3;++channel)assert(sample(mixed,56+xx,40+yy,channel)==sample(baseline,40+xx,40+yy,channel));font.clearPackGlyphs();auto restored=gpuFrame(u8"§ztest:font;§c\ue002");assert(restored!=bitmap);glDeleteTextures(1,&testTexture);
 auto& inv=game->javaSession->inventory;inv.windowId=7;inv.title=u8"§bRandom Teleport";inv.slots.resize(63);
 for(int i=0;i<27;++i){auto& item=inv.slots[i];item.resourceId="minecraft:light_blue_stained_glass_pane";item.kind="LightBlueStainedGlassPane";item.name=i%2?"§rLight Blue Stained Glass Pane":"§r ";item.count=1;}
 for(int i=0;i<3;++i){auto& item=inv.slots[11+i*2];item.resourceId=i==0?"minecraft:grass_block":i==1?"minecraft:netherrack":"minecraft:end_stone";item.name=i==0?"§aOverworld":i==1?"§cNether":"§eThe End";item.count=1;item.lore={"Find a new place to explore.","§bChoose this destination.",u8"§7雪 · No items are moved."};}
 auto* screen=new InventoryProbe(7);game->javaSession->connection.connect("fixture","Test",false);
 auto find=[&](int id)->Button*{for(auto* b:screen->buttons)if(b->id==id)return b;return nullptr;};
 for(auto dimensions:{std::make_pair(200,160),std::make_pair(285,160),std::make_pair(400,240)}){screen->Screen::init(game,dimensions.first,dimensions.second);for(auto* b:screen->buttons)if(b->visible){assert(b->x>=0&&b->y>=0&&b->x+b->width<=dimensions.first&&b->y+b->height<=dimensions.second);}}
 screen->Screen::init(game,400,240);for(int i=0;i<3;++i){auto* b=find(i);assert(b&&b->visible);assert(screen->describedSlotAt(b->x+1,b->y+1)==11+i*2);}assert(!find(3)->visible);
 auto render=[&](const char* name,bool dirtyDepth=false){glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearDepthf(dirtyDepth?0.f:1.f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glClearDepthf(1.f);screen->render(-1,-1,0);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(800*480*4),flipped(pixels.size());glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());for(int row=0;row<480;++row)std::copy(pixels.begin()+row*800*4,pixels.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,name,0,flipped.data(),0,nullptr));return pixels;};
 auto cleanPixels=render(argv[1]);assert(cleanPixels==render(argv[1],true));screen->buttonClicked(find(0));assert(sent=="{\"type\":\"click_slot\",\"window_id\":7,\"slot\":11,\"right\":false}");sent.clear();auto* info=find(0);screen->mouseClicked(info->x+1,info->y+1,1);std::this_thread::sleep_for(std::chrono::milliseconds(660));screen->mouseReleased(info->x+1,info->y+1,1);assert(sent.empty());assert(find(-7)->visible);render(argv[2]);screen->buttonClicked(find(-1));sent.clear();inv.windowId=8;screen->buttonClicked(find(0));assert(sent.empty());inv.windowId=7;
 inv.title="§bSelect Kit 1v1";for(int i=0;i<3;++i){auto& item=inv.slots[11+i*2];item.resourceId=i==0?"minecraft:diamond_sword":i==1?"minecraft:iron_sword":"minecraft:potion";item.name=i==0?"§eBeast":i==1?"§bBoxing":"§dNoDebuff";item.lore={"Choose a Practice match.","§7View the kit, then select."};}render(argv[3]);screen->buttonClicked(find(2));assert(sent.find("\"slot\":15")!=std::string::npos);
 sent.clear();for(int i=0;i<27;++i){inv.slots[i]=inv.slots[13];inv.slots[i].name="Kit "+std::to_string(i);}screen->Screen::init(game,285,160);auto* first=find(0);assert(screen->describedSlotAt(first->x+1,first->y+1)==0);screen->mouseClicked(first->x+1,100,1);screen->mouseReleased(first->x+1,40,1);screen->tick();assert(sent.empty());assert(screen->describedSlotAt(first->x+1,first->y+1)==2);screen->buttonClicked(first);assert(sent.find("\"slot\":2")!=std::string::npos);sent.clear();screen->mouseClicked(first->x+1,50,1);screen->mouseReleased(first->x+1,150,1);screen->tick();assert(sent.empty());assert(screen->describedSlotAt(first->x+1,first->y+1)==0);

 std::cout<<"Native popup: SMP/Practice choices, clean icon render, lore, exact slots, direct single tap, stale-window guard and responsive bounds PASS"<<std::endl;std::_Exit(0);
}
