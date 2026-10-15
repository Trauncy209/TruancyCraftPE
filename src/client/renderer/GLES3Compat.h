#pragma once
#include <GLES3/gl3.h>
// Fixed-function state is implemented by our shader backend, never by GLES1.
#define GL_FIXED 0x140C
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702
#define GL_MATRIX_MODE 0x0BA0
#define GL_MODELVIEW_MATRIX 0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7
#define GL_TEXTURE_MATRIX 0x0BA8
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_CURRENT_COLOR 0x0B00
#define GL_CURRENT_NORMAL 0x0B02
#define GL_COLOR_ARRAY 0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_ALPHA_TEST 0x0BC0
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_LIGHTING 0x0B50
#define GL_LIGHT0 0x4000
#define GL_LIGHT1 0x4001
#define GL_NORMALIZE 0x0BA1
#define GL_RESCALE_NORMAL 0x803A
#define GL_COLOR_MATERIAL 0x0B57
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_SPECULAR 0x1202
#define GL_POSITION 0x1203
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_FOG 0x0B60
#define GL_FOG_MODE 0x0B65
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_COLOR 0x0B66
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_FOG_HINT 0x0C54
#define GL_EXP 0x0800
#define GL_EXP2 0x0801
#define GL_FLAT 0x1D00
#define GL_SMOOTH 0x1D01
#define GL_SHADE_MODEL 0x0B54
#define GL_BLEND_SRC GL_BLEND_SRC_RGB
#define GL_BLEND_DST GL_BLEND_DST_RGB
#define GL_QUADS 0x0007
namespace TCPEGL3 {
bool initialize();void resetContext();GLenum getError();
void matrixMode(GLenum);void loadIdentity();void loadMatrix(const GLfloat*);void multMatrix(const GLfloat*);void pushMatrix();void popMatrix();void translate(GLfloat,GLfloat,GLfloat);void rotate(GLfloat,GLfloat,GLfloat,GLfloat);void scale(GLfloat,GLfloat,GLfloat);void ortho(GLfloat,GLfloat,GLfloat,GLfloat,GLfloat,GLfloat);
void enable(GLenum);void disable(GLenum);GLboolean isEnabled(GLenum);void getFloat(GLenum,GLfloat*);void getInteger(GLenum,GLint*);void getBoolean(GLenum,GLboolean*);void hint(GLenum,GLenum);
void color(GLfloat,GLfloat,GLfloat,GLfloat);void normal(GLfloat,GLfloat,GLfloat);void alphaFunc(GLenum,GLfloat);void fog(GLenum,GLfloat);void fogv(GLenum,const GLfloat*);void fogx(GLenum,GLint);void light(GLenum,GLenum,const GLfloat*);void lightModel(GLenum,const GLfloat*);void shade(GLenum);void colorMaterial(GLenum,GLenum);
void enableArray(GLenum);void disableArray(GLenum);void vertexPointer(GLint,GLenum,GLsizei,const void*);void colorPointer(GLint,GLenum,GLsizei,const void*);void texPointer(GLint,GLenum,GLsizei,const void*);void normalPointer(GLenum,GLsizei,const void*);void drawArrays(GLenum,GLint,GLsizei);
}
#ifndef TCPE_GLES3_IMPLEMENTATION
#define glGetError TCPEGL3::getError
#define glMatrixMode TCPEGL3::matrixMode
#define glLoadIdentity TCPEGL3::loadIdentity
#define glLoadMatrixf TCPEGL3::loadMatrix
#define glMultMatrixf TCPEGL3::multMatrix
#define glPushMatrix TCPEGL3::pushMatrix
#define glPopMatrix TCPEGL3::popMatrix
#define glTranslatef TCPEGL3::translate
#define glRotatef TCPEGL3::rotate
#define glScalef TCPEGL3::scale
#define glOrthof TCPEGL3::ortho
#define glEnable TCPEGL3::enable
#define glDisable TCPEGL3::disable
#define glIsEnabled TCPEGL3::isEnabled
#define glGetFloatv TCPEGL3::getFloat
#define glGetIntegerv TCPEGL3::getInteger
#define glGetBooleanv TCPEGL3::getBoolean
#define glHint TCPEGL3::hint
#define glColor4f TCPEGL3::color
#define glColor3f(r,g,b) TCPEGL3::color(r,g,b,1)
#define glNormal3f TCPEGL3::normal
#define glAlphaFunc TCPEGL3::alphaFunc
#define glFogf TCPEGL3::fog
#define glFogfv TCPEGL3::fogv
#define glFogx TCPEGL3::fogx
#define glLightfv TCPEGL3::light
#define glLightModelfv TCPEGL3::lightModel
#define glColorMaterial TCPEGL3::colorMaterial
#define glShadeModel TCPEGL3::shade
#define glEnableClientState TCPEGL3::enableArray
#define glDisableClientState TCPEGL3::disableArray
#define glVertexPointer TCPEGL3::vertexPointer
#define glColorPointer TCPEGL3::colorPointer
#define glTexCoordPointer TCPEGL3::texPointer
#define glNormalPointer TCPEGL3::normalPointer
#define glDrawArrays TCPEGL3::drawArrays
#endif
