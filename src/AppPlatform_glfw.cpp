#include "AppPlatform_glfw.h"

#ifdef WIN32
#include <Windows.h>
#endif

#ifndef TCPE_GLES3
// Loader that tries GLFW first, then falls back to opengl32.dll for any
// function that glfwGetProcAddress can't resolve.
void* winGLLoader(const char* name) {
    void* p = (void*)glfwGetProcAddress(name);
#ifdef WIN32
    if (!p) {
        static HMODULE opengl32 = LoadLibraryA("opengl32.dll");
        if (opengl32) p = (void*)GetProcAddress(opengl32, name);
    }
#endif
    return p;
}

// -----------------------------------------------------------------------
// Compatibility wrappers for OpenGL ES functions absent from desktop GL.
// The desktop equivalents use double parameters and are not declared in
// the GLES1 GLAD header, so look them up at runtime via winGLLoader.
// -----------------------------------------------------------------------
static void APIENTRY compat_glDepthRangef(GLfloat n, GLfloat f) {
    typedef void(APIENTRY *fn_t)(double, double);
    static fn_t fn = (fn_t)winGLLoader("glDepthRange");
    if (fn) fn((double)n, (double)f);
}
static void APIENTRY compat_glClearDepthf(GLfloat d) {
    typedef void(APIENTRY *fn_t)(double);
    static fn_t fn = (fn_t)winGLLoader("glClearDepth");
    if (fn) fn((double)d);
}
static void APIENTRY compat_glOrthof(GLfloat l, GLfloat r, GLfloat b,
                                      GLfloat t, GLfloat n, GLfloat f) {
    typedef void(APIENTRY *fn_t)(double,double,double,double,double,double);
    static fn_t fn = (fn_t)winGLLoader("glOrtho");
    if (fn) fn(l, r, b, t, n, f);
}
static void APIENTRY compat_glFrustumf(GLfloat l, GLfloat r, GLfloat b,
                                        GLfloat t, GLfloat n, GLfloat f) {
    typedef void(APIENTRY *fn_t)(double,double,double,double,double,double);
    static fn_t fn = (fn_t)winGLLoader("glFrustum");
    if (fn) fn(l, r, b, t, n, f);
}

// glFogx / glFogxv are OpenGL ES fixed-point variants that don't exist in
// desktop opengl32.dll.  Wrap them via the float equivalents.  For fog-mode
// enum params the GLfixed value IS the enum integer, so the cast is exact.
static void APIENTRY compat_glFogx(GLenum pname, GLfixed param) {
    glFogf(pname, (GLfloat)param);
}
static void APIENTRY compat_glFogxv(GLenum pname, const GLfixed* params) {
    // Only GL_FOG_COLOR uses an array; convert each element.
    GLfloat fp[4] = {
        (GLfloat)params[0], (GLfloat)params[1],
        (GLfloat)params[2], (GLfloat)params[3]
    };
    glFogfv(pname, fp);
}

void glPatchDesktopCompat() {
    if (!glad_glDepthRangef) glad_glDepthRangef = compat_glDepthRangef;
    if (!glad_glClearDepthf) glad_glClearDepthf = compat_glClearDepthf;
    if (!glad_glOrthof)      glad_glOrthof      = compat_glOrthof;
    if (!glad_glFrustumf)    glad_glFrustumf    = compat_glFrustumf;
    if (!glad_glFogx)        glad_glFogx        = compat_glFogx;
    if (!glad_glFogxv)       glad_glFogxv       = compat_glFogxv;
}
#else
void glPatchDesktopCompat() {}
#endif


float AppPlatform_glfw::getPixelsPerMillimeter() {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    int width_mm, height_mm;
    glfwGetMonitorPhysicalSize(monitor, &width_mm, &height_mm);

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    return (float)mode->width / (float)width_mm;
}