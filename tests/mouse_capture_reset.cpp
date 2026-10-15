#include "NinecraftApp.h"
#define MAIN_CLASS NinecraftApp
#define main tcpe_unused_application_entry
#include "main_glfw.h"
#undef main
#include "client/MouseHandler.h"
#include "platform/input/Mouse.h"
#include <cassert>
#include <iostream>
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_FOCUS_ON_SHOW,GLFW_FALSE);glfwWindowHint(GLFW_FOCUSED,GLFW_FALSE);
#ifdef TCPE_GLES3
 glfwWindowHint(GLFW_CONTEXT_CREATION_API,GLFW_EGL_CONTEXT_API);glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
#endif
 auto* window=glfwCreateWindow(320,240,"TCPE capture transition verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);
 glfwSetWindowFocusCallback(window,window_focus_callback);glfwSetCursorPosCallback(window,cursor_position_callback);
 const bool rawSupported=glfwRawMouseMotionSupported();
 MouseHandler mouse;Mouse::feed(MouseAction::ACTION_MOVE,0,10,10,45,-120);Mouse::feed(MouseAction::ACTION_LEFT,1,10,10);assert(Mouse::getDY()==-120);mouse.grab();assert(glfwGetInputMode(window,GLFW_CURSOR)==GLFW_CURSOR_DISABLED);if(rawSupported)assert(glfwGetInputMode(window,GLFW_RAW_MOUSE_MOTION)==GLFW_TRUE);assert(Mouse::getDX()==0&&Mouse::getDY()==0);assert(Mouse::isButtonDown(MouseAction::ACTION_LEFT));
 Mouse::feed(MouseAction::ACTION_MOVE,0,14,13,4,3);assert(Mouse::getDX()==4&&Mouse::getDY()==3);mouse.release();assert(glfwGetInputMode(window,GLFW_CURSOR)==GLFW_CURSOR_NORMAL);if(rawSupported)assert(glfwGetInputMode(window,GLFW_RAW_MOUSE_MOTION)==GLFW_FALSE);assert(Mouse::getDX()==0&&Mouse::getDY()==0);assert(Mouse::isButtonDown(MouseAction::ACTION_LEFT));
 Mouse::feed(MouseAction::ACTION_LEFT,0,14,13);assert(!Mouse::isButtonDown(MouseAction::ACTION_LEFT));mouse.grab();if(rawSupported)assert(glfwGetInputMode(window,GLFW_RAW_MOUSE_MOTION)==GLFW_TRUE);Mouse::feed(MouseAction::ACTION_MOVE,0,15,15,1,2);assert(Mouse::getDX()==1&&Mouse::getDY()==2);
 // Exercise the production focus callback without moving desktop focus.
 window_focus_callback(window,GLFW_FALSE);assert(resetCursorTracking);assert(Mouse::getDX()==0&&Mouse::getDY()==0);
 Mouse::feed(MouseAction::ACTION_MOVE,0,17,18,55,66);window_focus_callback(window,GLFW_TRUE);assert(resetCursorTracking);assert(Mouse::getDX()==0&&Mouse::getDY()==0);
 // This visible window is intentionally unfocused: no peer/user input is stolen.
 assert(!glfwGetWindowAttrib(window,GLFW_FOCUSED));Mouse::feed(MouseAction::ACTION_MOVE,0,20,20,77,88);cursor_position_callback(window,4000.,-3000.);assert(resetCursorTracking);assert(Mouse::getDX()==0&&Mouse::getDY()==0);cursor_position_callback(window,-5000.,6000.);assert(Mouse::getDX()==0&&Mouse::getDY()==0);
 mouse.release();if(rawSupported)assert(glfwGetInputMode(window,GLFW_RAW_MOUSE_MOTION)==GLFW_FALSE);
 std::cout<<"Visible unfocused GLFW capture: real cursor/raw-mode transitions, stale delta clears, held buttons preserved, production focus reset and background-motion rejection PASS; raw supported="<<rawSupported<<"; focused physical motion/OS event-order acceptance remains a separate live test"<<std::endl;glfwDestroyWindow(window);glfwTerminate();
}
