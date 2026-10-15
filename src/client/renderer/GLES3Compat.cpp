#ifdef TCPE_GLES3
#define TCPE_GLES3_IMPLEMENTATION
#include "GLES3Compat.h"
#if defined(PLATFORM_DESKTOP)
#include <GLES3/gl31.h>
#endif
#include <array>
#include <vector>
#include <set>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <chrono>
#include <cstdlib>
namespace TCPEGL3 {
using Mat=std::array<float,16>;
Mat identity(){Mat m{};m[0]=m[5]=m[10]=m[15]=1;return m;}
Mat multiply(const Mat&a,const Mat&b){Mat m{};for(int c=0;c<4;c++)for(int r=0;r<4;r++)for(int k=0;k<4;k++)m[c*4+r]+=a[k*4+r]*b[c*4+k];return m;}
struct Array{int size=4;GLenum type=GL_FLOAT;int stride=0;const void* pointer=nullptr;GLuint buffer=0;bool enabled=false;};
struct Light{std::array<float,4>position{{0,0,1,0}},diffuse{{0,0,0,1}},ambient{{0,0,0,1}};};
struct State{GLenum mode=GL_MODELVIEW,alpha=GL_ALWAYS,fogMode=GL_EXP,shade=GL_SMOOTH;float alphaRef=0,density=1,start=0,end=1;std::vector<Mat>mv{identity()},pr{identity()},tx{identity()};std::set<GLenum>enables;std::array<float,4> color{{1,1,1,1}},fogColor{{0,0,0,0}},ambient{{.2,.2,.2,1}};std::array<float,3>normal{{0,0,1}};Array arrays[4];Light lights[2];GLuint program=0,vao=0,stream[4]{};GLenum error=GL_NO_ERROR;std::unordered_map<std::string,GLint> locations;std::unordered_map<std::string,int> ints;std::unordered_map<std::string,float> floats;std::unordered_map<std::string,std::array<float,4>> vectors;std::unordered_map<std::string,Mat> matrices;};
static State s;
std::vector<Mat>& stack(){return s.mode==GL_PROJECTION?s.pr:s.mode==GL_TEXTURE?s.tx:s.mv;}
bool legacy(GLenum e){return e==GL_TEXTURE_2D||e==GL_ALPHA_TEST||e==GL_LIGHTING||e==GL_LIGHT0||e==GL_LIGHT1||e==GL_COLOR_MATERIAL||e==GL_NORMALIZE||e==GL_RESCALE_NORMAL||e==GL_FOG;}
void enable(GLenum e){if(legacy(e))s.enables.insert(e);else glEnable(e);}void disable(GLenum e){if(legacy(e))s.enables.erase(e);else glDisable(e);}GLboolean isEnabled(GLenum e){return legacy(e)?s.enables.count(e)!=0:glIsEnabled(e);}
void matrixMode(GLenum m){if(m==GL_MODELVIEW||m==GL_PROJECTION||m==GL_TEXTURE)s.mode=m;else s.error=GL_INVALID_ENUM;}void loadIdentity(){stack().back()=identity();}void loadMatrix(const float*m){std::copy(m,m+16,stack().back().begin());}void multMatrix(const float*m){Mat a;std::copy(m,m+16,a.begin());stack().back()=multiply(stack().back(),a);}void pushMatrix(){auto&v=stack();if(v.size()<64)v.push_back(v.back());else s.error=GL_INVALID_OPERATION;}void popMatrix(){auto&v=stack();if(v.size()>1)v.pop_back();else s.error=GL_INVALID_OPERATION;}
void translate(float x,float y,float z){auto m=identity();m[12]=x;m[13]=y;m[14]=z;multMatrix(m.data());}void scale(float x,float y,float z){auto m=identity();m[0]=x;m[5]=y;m[10]=z;multMatrix(m.data());}
void rotate(float degrees,float x,float y,float z){float n=std::sqrt(x*x+y*y+z*z);if(n==0)return;x/=n;y/=n;z/=n;float a=degrees*.0174532925199433f,c=std::cos(a),q=std::sin(a),t=1-c;auto m=identity();m[0]=t*x*x+c;m[4]=t*x*y-q*z;m[8]=t*x*z+q*y;m[1]=t*x*y+q*z;m[5]=t*y*y+c;m[9]=t*y*z-q*x;m[2]=t*x*z-q*y;m[6]=t*y*z+q*x;m[10]=t*z*z+c;multMatrix(m.data());}
void ortho(float l,float r,float b,float t,float n,float f){if(l==r||b==t||n==f){s.error=GL_INVALID_VALUE;return;}auto m=identity();m[0]=2/(r-l);m[5]=2/(t-b);m[10]=-2/(f-n);m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-(f+n)/(f-n);multMatrix(m.data());}
void getFloat(GLenum e,float*p){if(e==GL_CURRENT_COLOR)std::copy(s.color.begin(),s.color.end(),p);else if(e==GL_CURRENT_NORMAL)std::copy(s.normal.begin(),s.normal.end(),p);else if(e==GL_MODELVIEW_MATRIX)std::copy(s.mv.back().begin(),s.mv.back().end(),p);else if(e==GL_PROJECTION_MATRIX)std::copy(s.pr.back().begin(),s.pr.back().end(),p);else if(e==GL_TEXTURE_MATRIX)std::copy(s.tx.back().begin(),s.tx.back().end(),p);else if(e==GL_ALPHA_TEST_REF)*p=s.alphaRef;else if(e==GL_FOG_COLOR)std::copy(s.fogColor.begin(),s.fogColor.end(),p);else if(e==GL_FOG_DENSITY)*p=s.density;else if(e==GL_FOG_START)*p=s.start;else if(e==GL_FOG_END)*p=s.end;else glGetFloatv(e,p);}
void getInteger(GLenum e,GLint*p){if(e==GL_MATRIX_MODE)*p=s.mode;else if(e==GL_ALPHA_TEST_FUNC)*p=s.alpha;else if(e==GL_FOG_MODE)*p=s.fogMode;else if(e==GL_SHADE_MODEL)*p=s.shade;else glGetIntegerv(e,p);}void getBoolean(GLenum e,GLboolean*p){if(legacy(e))*p=isEnabled(e);else glGetBooleanv(e,p);}void hint(GLenum e,GLenum v){if(e!=GL_FOG_HINT&&e!=GL_PERSPECTIVE_CORRECTION_HINT)glHint(e,v);}
void color(float r,float g,float b,float a){s.color={{r,g,b,a}};}void normal(float x,float y,float z){s.normal={{x,y,z}};}void alphaFunc(GLenum f,float r){s.alpha=f;s.alphaRef=std::max(0.f,std::min(1.f,r));}void fog(GLenum e,float v){if(e==GL_FOG_MODE)s.fogMode=(GLenum)v;else if(e==GL_FOG_DENSITY)s.density=v;else if(e==GL_FOG_START)s.start=v;else if(e==GL_FOG_END)s.end=v;else s.error=GL_INVALID_ENUM;}void fogv(GLenum e,const float*p){if(e==GL_FOG_COLOR)std::copy(p,p+4,s.fogColor.begin());else fog(e,*p);}void fogx(GLenum e,GLint v){fog(e,e==GL_FOG_MODE?(float)v:v/65536.f);}
void light(GLenum l,GLenum p,const float*v){if(l<GL_LIGHT0||l>GL_LIGHT1){s.error=GL_INVALID_ENUM;return;}auto&a=s.lights[l-GL_LIGHT0];if(p==GL_POSITION){for(int r=0;r<4;r++){a.position[r]=0;for(int k=0;k<4;k++)a.position[r]+=s.mv.back()[k*4+r]*v[k];}}else if(p==GL_DIFFUSE)std::copy(v,v+4,a.diffuse.begin());else if(p==GL_AMBIENT)std::copy(v,v+4,a.ambient.begin());else if(p!=GL_SPECULAR)s.error=GL_INVALID_ENUM;}
void lightModel(GLenum e,const float*v){if(e==GL_LIGHT_MODEL_AMBIENT)std::copy(v,v+4,s.ambient.begin());else s.error=GL_INVALID_ENUM;}void shade(GLenum e){s.shade=e;}void colorMaterial(GLenum face,GLenum mode){if(face!=GL_FRONT_AND_BACK||mode!=GL_AMBIENT_AND_DIFFUSE)s.error=GL_INVALID_ENUM;}
int arrayIndex(GLenum e){return e==GL_VERTEX_ARRAY?0:e==GL_TEXTURE_COORD_ARRAY?1:e==GL_COLOR_ARRAY?2:e==GL_NORMAL_ARRAY?3:-1;}void enableArray(GLenum e){int i=arrayIndex(e);if(i>=0)s.arrays[i].enabled=true;else s.error=GL_INVALID_ENUM;}void disableArray(GLenum e){int i=arrayIndex(e);if(i>=0)s.arrays[i].enabled=false;else s.error=GL_INVALID_ENUM;}
void pointer(int i,int n,GLenum t,int stride,const void*p){GLint b=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&b);auto&a=s.arrays[i];a.size=n;a.type=t;a.stride=stride;a.pointer=p;a.buffer=b;}void vertexPointer(GLint n,GLenum t,GLsizei st,const void*p){pointer(0,n,t,st,p);}void texPointer(GLint n,GLenum t,GLsizei st,const void*p){pointer(1,n,t,st,p);}void colorPointer(GLint n,GLenum t,GLsizei st,const void*p){pointer(2,n,t,st,p);}void normalPointer(GLenum t,GLsizei st,const void*p){pointer(3,3,t,st,p);}
static const char*vs=R"(#version 300 es
precision highp float;
layout(location=0)in vec4 aPosition;layout(location=1)in vec2 aUV;layout(location=2)in vec4 aColor;layout(location=3)in vec3 aNormal;
uniform mat4 uMV,uPR,uTX;uniform mat3 uNormal;uniform int uLighting,uNormalize;uniform vec4 uAmbient,uLightPosition[2],uLightDiffuse[2],uLightAmbient[2];uniform int uLights[2];
out vec2 vUV;out vec4 vColor;flat out vec4 fColor;out float vFog;
void main(){vec4 eye=uMV*aPosition;gl_Position=uPR*eye;vUV=(uTX*vec4(aUV,0.,1.)).xy;vec4 c=aColor;if(uLighting!=0){vec3 n=uNormal*aNormal;if(uNormalize!=0)n=normalize(n);vec3 illumination=uAmbient.rgb;for(int i=0;i<2;i++){if(uLights[i]!=0){vec3 l=uLightPosition[i].w==0.?normalize(uLightPosition[i].xyz):normalize(uLightPosition[i].xyz-eye.xyz);illumination+=uLightAmbient[i].rgb+uLightDiffuse[i].rgb*max(0.,dot(n,l));}}c.rgb=clamp(c.rgb*illumination,0.,1.);}vColor=c;fColor=c;vFog=abs(eye.z);})";
static const char*fs=R"(#version 300 es
precision highp float;
in vec2 vUV;in vec4 vColor;flat in vec4 fColor;in float vFog;layout(location=0)out vec4 outColor;
uniform sampler2D uTexture;uniform int uTextured,uFlat,uAlpha,uAlphaFunc,uFog,uFogMode;uniform float uAlphaRef,uDensity,uStart,uEnd;uniform vec4 uFogColor;
void main(){vec4 c=uFlat!=0?fColor:vColor;if(uTextured!=0)c*=texture(uTexture,vUV);if(uAlpha!=0){bool pass=true;if(uAlphaFunc==512)pass=false;else if(uAlphaFunc==513)pass=c.a<uAlphaRef;else if(uAlphaFunc==514)pass=c.a==uAlphaRef;else if(uAlphaFunc==515)pass=c.a<=uAlphaRef;else if(uAlphaFunc==516)pass=c.a>uAlphaRef;else if(uAlphaFunc==517)pass=c.a!=uAlphaRef;else if(uAlphaFunc==518)pass=c.a>=uAlphaRef;if(!pass)discard;}if(uFog!=0){float f=uFogMode==9729?(uEnd-vFog)/max(.000001,uEnd-uStart):uFogMode==2048?exp(-uDensity*vFog):exp(-pow(uDensity*vFog,2.));c.rgb=mix(uFogColor.rgb,c.rgb,clamp(f,0.,1.));}outColor=c;})";
GLuint compile(GLenum type,const char*source){GLuint id=glCreateShader(type);glShaderSource(id,1,&source,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[2048];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::fprintf(stderr,"GLES3 shader failed: %s\n",log);glDeleteShader(id);return 0;}return id;}
bool initialize(){if(s.program)return true;const char*v=(const char*)glGetString(GL_VERSION);if(!v||!std::strstr(v,"OpenGL ES 3")){std::fprintf(stderr,"GLES3 required; actual context: %s\n",v?v:"none");return false;}GLuint a=compile(GL_VERTEX_SHADER,vs),b=compile(GL_FRAGMENT_SHADER,fs);if(!a||!b)return false;s.program=glCreateProgram();glAttachShader(s.program,a);glAttachShader(s.program,b);glLinkProgram(s.program);glDeleteShader(a);glDeleteShader(b);GLint ok;glGetProgramiv(s.program,GL_LINK_STATUS,&ok);if(!ok){char log[2048];glGetProgramInfoLog(s.program,sizeof(log),nullptr,log);std::fprintf(stderr,"GLES3 program failed: %s\n",log);glDeleteProgram(s.program);s.program=0;return false;}glGenVertexArrays(1,&s.vao);glGenBuffers(4,s.stream);std::fprintf(stderr,"TCPE shader renderer: %s\n",v);std::fprintf(stderr,"TCPE GLES3 GPU: %s / %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER));return true;}
void resetContext(){s=State{};}
GLenum getError(){if(s.error!=GL_NO_ERROR){auto e=s.error;s.error=GL_NO_ERROR;return e;}return glGetError();}
GLint loc(const char*n){auto f=s.locations.find(n);if(f!=s.locations.end())return f->second;GLint value=glGetUniformLocation(s.program,n);s.locations.emplace(n,value);return value;}void ui(const char*n,int v){auto f=s.ints.find(n);if(f!=s.ints.end()&&f->second==v)return;s.ints[n]=v;glUniform1i(loc(n),v);}void uf(const char*n,float v){auto f=s.floats.find(n);if(f!=s.floats.end()&&f->second==v)return;s.floats[n]=v;glUniform1f(loc(n),v);}void u4(const char*n,const float*p){std::array<float,4>v;std::copy(p,p+4,v.begin());auto f=s.vectors.find(n);if(f!=s.vectors.end()&&f->second==v)return;s.vectors[n]=v;glUniform4fv(loc(n),1,p);}
void um(const char*n,const float*p,int dim){Mat v{};std::copy(p,p+dim*dim,v.begin());auto f=s.matrices.find(n);if(f!=s.matrices.end()&&f->second==v)return;s.matrices[n]=v;if(dim==3)glUniformMatrix3fv(loc(n),1,GL_FALSE,p);else glUniformMatrix4fv(loc(n),1,GL_FALSE,p);}
static unsigned frameDraws=0;static double frameDrawMs=0;static const bool qaStats=std::getenv("TCPE_GLES3_QA_STATS")!=nullptr;
void takeFrameStats(unsigned&count,double&ms){count=frameDraws;ms=frameDrawMs;frameDraws=0;frameDrawMs=0;}
// Off by default: capture the real GL-thread state without debugger function calls.
static void qaDrawState(GLsizei count){
#if defined(PLATFORM_DESKTOP)
 static const bool enabled=std::getenv("TCPE_GLES3_QA_STATE")!=nullptr;
 if(!enabled)return;
 static std::set<std::string> seen;static std::set<GLuint> dumped;
 GLint texture=0,width=0,height=0,format=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
 if(texture){glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);}
 if((width!=2048&&width!=256)||height!=width||count<100||s.pr.back()[15]!=0)return;
 std::string key=std::to_string(texture)+":"+std::to_string(isEnabled(GL_ALPHA_TEST))+":"+std::to_string(count);
 if(seen.size()<96&&seen.insert(key).second){
  GLint depth=0,bits=0,read=0,draw=0,active=0,viewport[4]{},range[2]{},precision=0;GLboolean mask=0;GLfloat depthRange[2]{},clear=0;
  glGetIntegerv(GL_DEPTH_FUNC,&depth);glGetIntegerv(GL_DEPTH_BITS,&bits);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetFloatv(GL_DEPTH_RANGE,depthRange);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clear);glGetShaderPrecisionFormat(GL_VERTEX_SHADER,GL_HIGH_FLOAT,range,&precision);
  std::fprintf(stderr,"GLES3_QA draw count=%d texture=%d size=%dx%d format=%x active=%x fbo=%d/%d depth=%d func=%x bits=%d write=%d range=%g,%g clear=%g alpha=%d/%x/%g viewport=%d,%d,%d,%d highp=%d/%d/%d\n",count,texture,width,height,format,active,read,draw,glIsEnabled(GL_DEPTH_TEST),depth,bits,mask,depthRange[0],depthRange[1],clear,isEnabled(GL_ALPHA_TEST),s.alpha,s.alphaRef,viewport[0],viewport[1],viewport[2],viewport[3],range[0],range[1],precision);
  GLint current=0,sampler=0,minFilter=0,magFilter=0,wrapS=0,wrapT=0,base=0,maxLevel=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&minFilter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&magFilter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&wrapS);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,&wrapT);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,&base);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,&maxLevel);std::fprintf(stderr,"GLES3_QA program actual=%d expected=%u sampler=%d filters=%x/%x wrap=%x/%x levels=%d/%d\n",current,s.program,sampler,minFilter,magFilter,wrapS,wrapT,base,maxLevel);
  for(const char*n:{"uTexture","uTextured","uAlpha","uAlphaFunc","uFlat","uFog","uLighting"}){GLint actual=0;glGetUniformiv(s.program,loc(n),&actual);std::fprintf(stderr,"GLES3_QA uniform %s actual=%d cached=%d\n",n,actual,s.ints[n]);}
  for(const char*n:{"uMV","uPR","uTX"}){GLfloat actual[16]{};glGetUniformfv(s.program,loc(n),actual);std::fprintf(stderr,"GLES3_QA uniform %s",n);for(float v:actual)std::fprintf(stderr," %.9g",v);std::fprintf(stderr,"\n");}
  for(GLuint a=0;a<4;++a){GLint buffer=0,stride=0,size=0,type=0,on=0;void*ptr=nullptr;glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&stride);glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_SIZE,&size);glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_TYPE,&type);glGetVertexAttribiv(a,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&on);glGetVertexAttribPointerv(a,GL_VERTEX_ATTRIB_ARRAY_POINTER,&ptr);std::fprintf(stderr,"GLES3_QA attr%u enabled=%d buffer=%d stride=%d size=%d type=%x offset=%p\n",a,on,buffer,stride,size,type,ptr);}
  std::fprintf(stderr,"GLES3_QA matrices MV");for(float v:s.mv.back())std::fprintf(stderr," %.9g",v);std::fprintf(stderr," PR");for(float v:s.pr.back())std::fprintf(stderr," %.9g",v);std::fprintf(stderr," TX");for(float v:s.tx.back())std::fprintf(stderr," %.9g",v);std::fprintf(stderr,"\n");
 }
 static std::set<std::string> buffers;
 std::string bufferKey=std::to_string(texture)+":"+std::to_string(isEnabled(GL_ALPHA_TEST));
 if(buffers.size()<8&&buffers.insert(bufferKey).second){GLint previous=0,buffer=0,size=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);if(buffer){glBindBuffer(GL_ARRAY_BUFFER,buffer);glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&size);if(size>0&&size<=16*1024*1024){if(void*data=glMapBufferRange(GL_ARRAY_BUFFER,0,size,GL_MAP_READ_BIT)){char name[96];std::snprintf(name,sizeof(name),"tcpe-gles3-qa-vbo-%d-alpha%d.bin",buffer,isEnabled(GL_ALPHA_TEST));if(FILE*f=std::fopen(name,"wb")){std::fwrite(data,1,size,f);std::fclose(f);std::fprintf(stderr,"GLES3_QA vbo=%s bytes=%d\n",name,size);}glUnmapBuffer(GL_ARRAY_BUFFER);}}glBindBuffer(GL_ARRAY_BUFFER,previous);}}
 if(dumped.size()<8&&dumped.insert(texture).second){
  GLint read=0,draw=0,pack=0,alignment=0,row=0,skipRows=0,skipPixels=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glGetIntegerv(GL_PACK_ROW_LENGTH,&row);glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
  GLuint fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE){std::vector<unsigned char> pixels(size_t(width)*height*4);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());char name[96];std::snprintf(name,sizeof(name),"tcpe-gles3-qa-atlas-%d.rgba",texture);if(FILE*f=std::fopen(name,"wb")){std::fwrite(pixels.data(),1,pixels.size(),f);std::fclose(f);std::fprintf(stderr,"GLES3_QA atlas=%s bytes=%zu\n",name,pixels.size());}}
  glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glDeleteFramebuffers(1,&fbo);glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);glPixelStorei(GL_PACK_ALIGNMENT,alignment);glPixelStorei(GL_PACK_ROW_LENGTH,row);glPixelStorei(GL_PACK_SKIP_ROWS,skipRows);glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
 }
#else
 (void)count;
#endif
}
int typeSize(GLenum t){return t==GL_FLOAT||t==GL_FIXED?4:t==GL_SHORT||t==GL_UNSIGNED_SHORT?2:1;}
void drawArrays(GLenum mode,GLint first,GLsizei count){if(count<=0||first<0)return;if(!initialize())return;auto drawStart=qaStats?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};GLint oldBuffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);
 // The TCPE renderer owns this context's programmable pipeline. Legacy calls
 // have no program/VAO state; keep our pipeline bound across glyph/chunk draws.
 // ARRAY_BUFFER still restores exactly because it is visible to legacy arrays.
 glBindVertexArray(s.vao);glUseProgram(s.program);
 um("uMV",s.mv.back().data(),4);um("uPR",s.pr.back().data(),4);um("uTX",s.tx.back().data(),4);
 // Inverse-transpose upper 3x3, including nonuniform item scale.
 const auto&m=s.mv.back();float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);float norm[9]{1,0,0,0,1,0,0,0,1};if(std::abs(det)>1e-20){float z=1/det;float q[9]{(e*i-f*h)*z,(b*i-c*h)*-z,(b*f-c*e)*z,(d*i-f*g)*-z,(a*i-c*g)*z,(a*f-c*d)*-z,(d*h-e*g)*z,(a*h-b*g)*-z,(a*e-b*d)*z};std::copy(q,q+9,norm);}um("uNormal",norm,3);
 ui("uLighting",isEnabled(GL_LIGHTING));ui("uNormalize",isEnabled(GL_NORMALIZE)||isEnabled(GL_RESCALE_NORMAL));u4("uAmbient",s.ambient.data());for(int j=0;j<2;j++){std::string idx="["+std::to_string(j)+"]";u4(("uLightPosition"+idx).c_str(),s.lights[j].position.data());u4(("uLightDiffuse"+idx).c_str(),s.lights[j].diffuse.data());u4(("uLightAmbient"+idx).c_str(),s.lights[j].ambient.data());ui(("uLights"+idx).c_str(),isEnabled(GL_LIGHT0+j));}
 ui("uTexture",0);ui("uTextured",isEnabled(GL_TEXTURE_2D));ui("uFlat",s.shade==GL_FLAT);ui("uAlpha",isEnabled(GL_ALPHA_TEST));ui("uAlphaFunc",s.alpha);uf("uAlphaRef",s.alphaRef);ui("uFog",isEnabled(GL_FOG));ui("uFogMode",s.fogMode);uf("uDensity",s.density);uf("uStart",s.start);uf("uEnd",s.end);u4("uFogColor",s.fogColor.data());
 for(int j=0;j<4;j++){auto&a=s.arrays[j];if(!a.enabled){glDisableVertexAttribArray(j);if(j==0)glVertexAttrib4f(j,0,0,0,1);if(j==1)glVertexAttrib2f(j,0,0);if(j==2)glVertexAttrib4fv(j,s.color.data());if(j==3)glVertexAttrib3fv(j,s.normal.data());continue;}const void*p=a.pointer;int st=a.stride?a.stride:a.size*typeSize(a.type);if(a.buffer){glBindBuffer(GL_ARRAY_BUFFER,a.buffer);}else{if(!p){s.error=GL_INVALID_OPERATION;continue;}size_t bytes=(size_t)(first+count)*st;glBindBuffer(GL_ARRAY_BUFFER,s.stream[j]);glBufferData(GL_ARRAY_BUFFER,bytes,p,GL_STREAM_DRAW);p=nullptr;}glEnableVertexAttribArray(j);glVertexAttribPointer(j,a.size,a.type,j==2&&a.type!=GL_FLOAT?GL_TRUE:GL_FALSE,st,p);}
 qaDrawState(count);
 glDrawArrays(mode==GL_QUADS?GL_TRIANGLES:mode,first,count);glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);if(qaStats){++frameDraws;frameDrawMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-drawStart).count();}
}
}
#endif
