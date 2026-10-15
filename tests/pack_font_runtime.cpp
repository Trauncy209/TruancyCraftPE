#include "network/java/JavaPackFontRuntime.h"
#include "client/renderer/Tesselator.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
static std::vector<std::string> rendererAcks;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* text,size_t n){std::string message(text,n);if(message.find("resource_pack_loaded")!=std::string::npos)rendererAcks.push_back(message);return true;}
static JavaGameSession::PackFonts bundle(const char* id,float advance,uint64_t generation=4){
 JavaGameSession::PackFonts p;p.id=id;JavaPackSoundDefinition sound;sound.name=std::string("test:")+id;p.sounds.definitions.push_back(sound);p.connection=7;p.generation=generation;
 JavaGameSession::PackFontTexture t;t.width=t.height=1;t.rgba={255,255,255,255};p.textures.push_back(t);
 JavaGameSession::PackFontGlyph g;g.font="minecraft:default";g.codepoint=0xe000;g.texture=0;g.width=g.height=1;g.scale=8;g.ascent=8;g.advance=advance;p.glyphs.push_back(g);return p;
}
static std::vector<std::string> audioStack;static int audioClears=0;
int main(){
 JavaSoundPackBridge::install(&audioStack,[](void*,const std::vector<const JavaPackSounds*>& packs){audioStack.clear();if(packs.empty())++audioClears;for(const auto* p:packs)for(const auto& d:p->definitions)audioStack.push_back(d.name);});
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_FOCUS_ON_SHOW,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(160,96,"Pack lifecycle",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 const float baseline=font.javaGlyphAdvance(0xe000);JavaGameSession session;assert(session.connection.connect("fixture","owned",false));session.phase="Game";session.resourcePackConnection=7;session.resourcePackGeneration=4;
 JavaGameSession::ResourcePack a,b;a.id="a";b.id="b";session.resourcePacks={a,b};JavaPackFontRuntime runtime;
 session.pendingPackFonts={bundle("b",21)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==baseline);assert(audioStack.empty()&&audioClears==0&&rendererAcks.empty());assert(!textures.hasTexture("java-pack-font/7/4/0/0"));
 session.pendingPackFonts={bundle("a",11)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==21);assert((audioStack==std::vector<std::string>{"test:a","test:b"}));assert(rendererAcks.size()==2);assert(rendererAcks[0]=="{\"type\":\"resource_pack_loaded\",\"connection\":7,\"generation\":4,\"id\":\"a\"}");assert(textures.hasTexture("java-pack-font/7/4/0/0"));assert(textures.hasTexture("java-pack-font/7/4/1/0"));
 auto staleConnection=bundle("b",77);staleConnection.connection=6;session.pendingPackFonts={bundle("b",99,3),bundle("unknown",88),staleConnection};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==21);assert(session.pendingPackFonts.empty());assert((audioStack==std::vector<std::string>{"test:a","test:b"}));
 session.resourcePackGeneration=5;session.resourcePacks={a};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==baseline);assert(!textures.hasTexture("java-pack-font/7/4/0/0"));assert(!textures.hasTexture("java-pack-font/7/4/1/0"));assert(audioStack.empty()&&audioClears==1);
 session.pendingPackFonts={bundle("a",12,5)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==12);
 ++session.resourcePackRevision;runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==baseline);assert(!textures.hasTexture("java-pack-font/7/5/0/0"));session.pendingPackFonts={bundle("a",12,5)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==12);
 session.resourcePackGeneration=6;session.resourcePacks.clear();runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==baseline);assert(!textures.hasTexture("java-pack-font/7/5/0/0"));
 session.resourcePackGeneration=4;session.resourcePacks={a};session.pendingPackFonts={bundle("a",13)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==13);
 textures.clear();runtime.graphicsReset(&font);assert(font.javaGlyphAdvance(0xe000)==baseline);runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==13);assert(textures.hasTexture("java-pack-font/7/4/0/0"));
 session.phase="Disconnected";session.pendingPackFonts={bundle("a",44)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==baseline);assert(session.pendingPackFonts.empty());assert(audioStack.empty());assert(!textures.hasTexture("java-pack-font/7/4/0/0"));
 session.phase="Game";session.pendingPackFonts={bundle("a",15)};runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==15);session.error="Transport failed";runtime.tick(session,&font,&textures);assert(font.javaGlyphAdvance(0xe000)==baseline);assert(audioStack.empty());
 assert(glGetError()==GL_NO_ERROR);std::cout<<"Pack font/audio GPU lifecycle: ordered overrides, complete stack barrier, stale/unknown rejection, scope cleanup, graphics recreation, disconnect/error restoration PASS"<<std::endl;std::_Exit(0);
}
