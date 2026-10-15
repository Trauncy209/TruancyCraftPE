#include "MouseHandler.h"
#include "player/input/ITurnInput.h"
#include "platform/input/Mouse.h"

#ifdef RPI
#include <SDL/SDL.h>
#endif

#ifdef PLATFORM_GLFW
#include <GLFW/glfw3.h>
#endif

MouseHandler::MouseHandler( ITurnInput* turnInput )
:	_turnInput(turnInput)
{}

MouseHandler::MouseHandler()
:	_turnInput(0)
{}

MouseHandler::~MouseHandler() {
}

void MouseHandler::setTurnInput( ITurnInput* turnInput ) {
	_turnInput = turnInput;
}

void MouseHandler::grab() {
	xd = 0;
	yd = 0;

#if defined(RPI)
	//LOGI("Grabbing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_ShowCursor(0);
#endif

#ifdef PLATFORM_GLFW
	glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(glfwGetCurrentContext(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	Mouse::reset2();
#endif
}

void MouseHandler::release() {
#if defined(RPI)
	//LOGI("Releasing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(1);
#endif

#ifdef PLATFORM_GLFW
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(glfwGetCurrentContext(), GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	Mouse::reset2();
#endif
}

void MouseHandler::poll() {
	if (_turnInput != 0) {
		TurnDelta td = _turnInput->getTurnDelta();
		xd = td.x;
		yd = td.y;
	}
}
