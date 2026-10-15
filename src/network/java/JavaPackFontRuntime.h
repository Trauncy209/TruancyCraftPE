#ifndef TCPE_JAVA_PACK_FONT_RUNTIME_H
#define TCPE_JAVA_PACK_FONT_RUNTIME_H
#include "JavaGameSession.h"
#include "JavaPackItemModels.h"
#include "../../client/gui/Font.h"
#include "../../client/renderer/Textures.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
// Render-thread ownership. Downloads arrive through the session; only current,
// complete stacks are applied, in server order rather than download order.
class JavaPackFontRuntime {
 JavaGameSession* owner=nullptr;uint64_t connection=0,generation=0;
 unsigned revision=0;bool dirty=false;
 std::map<std::string,JavaGameSession::PackFonts> decoded;
 std::vector<std::string> textureNames;
 struct ActiveAnimation {std::string pack;size_t index=0;TextureId texture=0;int width=0,height=0;uint64_t lastTick=std::numeric_limits<uint64_t>::max();size_t lastFrame=std::numeric_limits<size_t>::max();std::vector<unsigned char> mixed;};
 std::vector<ActiveAnimation> animations;
 using Clock=std::chrono::steady_clock;Clock::time_point animationEpoch;bool hasAnimationEpoch=false;
 void updateAnimations(Textures* textures,Clock::time_point now){
  if(animations.empty()||!hasAnimationEpoch)return;
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(now-animationEpoch).count();const uint64_t tick=elapsed>0?uint64_t(elapsed/50):0;
  GLint binding=0,alignment=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);bool uploaded=false;
  for(auto& active:animations){if(active.lastTick==tick)continue;auto pack=decoded.find(active.pack);if(pack==decoded.end()||active.index>=pack->second.animations.size())continue;const auto& source=pack->second.animations[active.index];uint64_t cycle=0;for(const auto& frame:source.frames)cycle+=frame.time;if(cycle==0)continue;
   uint64_t sub=tick%cycle;size_t current=0;while(current+1<source.frames.size()&&sub>=source.frames[current].time){sub-=source.frames[current].time;++current;}active.lastTick=tick;
   if(!source.interpolate&&active.lastFrame==current)continue;active.lastFrame=current;
   const auto& frame=source.frames[current];const unsigned char* pixels=frame.rgba.data();
   if(source.interpolate&&sub>0){const auto& next=source.frames[(current+1)%source.frames.size()];const double progress=double(sub)/frame.time;active.mixed.resize(frame.rgba.size());
    // 1.21.11 animate_sprite_interpolate.fsh mixes all RGBA channels.
    for(size_t i=0;i<frame.rgba.size();++i)active.mixed[i]=static_cast<unsigned char>(std::lround(frame.rgba[i]*(1-progress)+next.rgba[i]*progress));pixels=active.mixed.data();}
   // Keep the Textures cache synchronized even if an external draw bound GL
   // directly. Restore the caller's actual binding and unpack alignment below.
   glBindTexture2(GL_TEXTURE_2D,active.texture);textures->bind(active.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D2(GL_TEXTURE_2D,0,0,0,active.width,active.height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);uploaded=true;
  }
  if(uploaded){glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glBindTexture2(GL_TEXTURE_2D,TextureId(binding));textures->bind(TextureId(binding));}
 }

 void release(Font* font,Textures* textures){
  animations.clear();
  JavaPackItemModels::clear();
  JavaSoundPackBridge::clear();
  if(font)font->clearPackGlyphs();
  if(textures)for(const auto& name:textureNames)textures->removeTexture(name);
  textureNames.clear();
 }
public:
 void clear(Font* font,Textures* textures){release(font,textures);decoded.clear();owner=nullptr;connection=generation=0;revision=0;dirty=false;hasAnimationEpoch=false;}
 void graphicsReset(Font* font){animations.clear();JavaPackItemModels::clear();if(font)font->clearPackGlyphs();textureNames.clear();dirty=true;}
 void tick(JavaGameSession& session,Font* font,Textures* textures){tickAt(session,font,textures,Clock::now());}
 void tickAt(JavaGameSession& session,Font* font,Textures* textures,std::chrono::steady_clock::time_point now){
  if(!font||!textures)return;
  if(session.phase=="Disconnected"||session.phase=="Error"||!session.error.empty()){clear(font,textures);session.pendingPackFonts.clear();return;}
  if(owner!=&session||connection!=session.resourcePackConnection||generation!=session.resourcePackGeneration||revision!=session.resourcePackRevision){
   clear(font,textures);owner=&session;connection=session.resourcePackConnection;generation=session.resourcePackGeneration;revision=session.resourcePackRevision;dirty=true;
  }
  for(auto& pack:session.pendingPackFonts){
   if(pack.connection!=connection||pack.generation!=generation)continue;
   if(std::find_if(session.resourcePacks.begin(),session.resourcePacks.end(),[&](const JavaGameSession::ResourcePack& item){return item.id==pack.id;})==session.resourcePacks.end())continue;
   auto bytes=[](const JavaGameSession::PackFonts& value){size_t count=0;for(const auto& sample:value.sounds.samples)count+=sample.pcm.size();for(const auto& texture:value.textures)count+=texture.rgba.size();for(const auto& animation:value.animations)for(const auto& frame:animation.frames)count+=frame.rgba.size();return count;};size_t budget=bytes(pack);for(const auto& entry:decoded)if(entry.first!=pack.id)budget+=bytes(entry.second);
   if(budget>256*1024*1024){session.packFontError="Resource pack GPU data exceeds 256 MiB";clear(font,textures);session.pendingPackFonts.clear();return;}
   decoded[pack.id]=std::move(pack);dirty=true;
  }
  session.pendingPackFonts.clear();
  if(!dirty){updateAnimations(textures,now);return;}
  for(const auto& pack:session.resourcePacks)if(decoded.find(pack.id)==decoded.end())return;
  release(font,textures);
  std::vector<const JavaPackSounds*> soundStack;for(const auto& item:session.resourcePacks)soundStack.push_back(&decoded.at(item.id).sounds);JavaSoundPackBridge::apply(soundStack);
  for(size_t p=0;p<session.resourcePacks.size();++p){
   const auto& pack=decoded.at(session.resourcePacks[p].id);
   std::vector<TextureId> ids;
   for(size_t i=0;i<pack.textures.size();++i){
    const auto& source=pack.textures[i];
    if(source.width<=0||source.height<=0||source.width>8192||source.height>8192||source.rgba.size()!=size_t(source.width)*size_t(source.height)*4){session.packFontError="Invalid font texture dimensions";release(font,textures);dirty=false;return;}
    TextureData image;image.w=source.width;image.h=source.height;image.numBytes=int(source.rgba.size());image.transparent=true;
    image.data=new unsigned char[image.numBytes];std::copy(source.rgba.begin(),source.rgba.end(),image.data);
    const std::string key="java-pack-font/"+std::to_string(connection)+"/"+std::to_string(generation)+"/"+std::to_string(p)+"/"+std::to_string(i);
    textures->removeTexture(key);ids.push_back(textures->assignTexture(key,image));textureNames.push_back(key);
    textures->bind(ids.back());glTexParameteri2(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri2(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri2(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri2(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   }
   for(size_t a=0;a<pack.animations.size();++a){const auto& animation=pack.animations[a];bool valid=animation.texture>=0&&size_t(animation.texture)<ids.size()&&!animation.frames.empty()&&animation.frames.size()<=1024;
    if(valid){const auto& page=pack.textures[animation.texture];for(const auto& frame:animation.frames)if(frame.time==0||frame.time>1000000||frame.rgba.size()!=page.rgba.size())valid=false;}
    if(!valid){session.packFontError="Invalid pack animation";release(font,textures);dirty=false;return;}ActiveAnimation active;active.pack=pack.id;active.index=a;active.texture=ids[animation.texture];active.width=pack.textures[animation.texture].width;active.height=pack.textures[animation.texture].height;animations.push_back(std::move(active));
   }
   for(const auto& model:pack.models)JavaPackItemModels::install(model,ids);
   for(const auto& source:pack.glyphs){
    if(source.font.empty()||!std::isfinite(source.scale)||!std::isfinite(source.advance)||!std::isfinite(source.ascent)||source.texture<-1||source.texture>=int(ids.size()))continue;
    Font::PackGlyph glyph;glyph.advance=source.advance;glyph.ascent=source.ascent;
    if(source.texture>=0){
     const auto& image=pack.textures[source.texture];
     if(source.x<0||source.y<0||source.width<0||source.height<0||source.x>image.width-source.width||source.y>image.height-source.height||source.scale<=0)continue;
     glyph.texture=ids[source.texture];glyph.u=float(source.x)/image.width;glyph.v=float(source.y)/image.height;glyph.uw=float(source.width)/image.width;glyph.vh=float(source.height)/image.height;
     glyph.width=source.width*source.scale;glyph.height=source.height*source.scale;
    }
    font->setPackGlyph(source.codepoint,glyph,source.font);
   }
  }
  // Report only after every current pack's native textures/models/fonts and
  // ordered audio definitions have activated. Rust rejects stale/duplicate IDs.
  for(const auto& pack:session.resourcePacks)if(!pack.id.empty()&&pack.id.find_first_not_of("0123456789abcdefABCDEF-")==std::string::npos){
   session.connection.send("{\"type\":\"resource_pack_loaded\",\"connection\":"+std::to_string(connection)+",\"generation\":"+std::to_string(generation)+",\"id\":\""+pack.id+"\"}");
  }
  dirty=false;if(!animations.empty()&&!hasAnimationEpoch){animationEpoch=now;hasAnimationEpoch=true;}updateAnimations(textures,now);
 }
};
inline JavaPackFontRuntime& javaPackFontRuntime(){static JavaPackFontRuntime runtime;return runtime;}
#endif
