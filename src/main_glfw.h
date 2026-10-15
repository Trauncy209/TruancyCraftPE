#ifndef MAIN_GLFW_H__
#define MAIN_GLFW_H__

#include "App.h"
#include "GLFW/glfw3.h"
#include "client/renderer/gles.h"
#include "SharedConstants.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "platform/input/Keyboard.h"
#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"

#ifdef TCPE_GLES3
namespace TCPEGL3 { void takeFrameStats(unsigned&,double&); }
#endif
static App* g_app = 0;

int transformKey(int glfwkey) {
	if (glfwkey >= GLFW_KEY_KP_0 && glfwkey <= GLFW_KEY_KP_9) return glfwkey - GLFW_KEY_KP_0 + '0';
	if (glfwkey >= GLFW_KEY_F1 && glfwkey <= GLFW_KEY_F12) {
		return glfwkey - 178;
	}

	switch (glfwkey) {
		case GLFW_KEY_ESCAPE: return Keyboard::KEY_ESCAPE;
		case GLFW_KEY_BACKSPACE: return Keyboard::KEY_BACKSPACE;
		case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: return Keyboard::KEY_LSHIFT;
        case GLFW_KEY_TAB: return 9;
        case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL: return 17;
        case GLFW_KEY_LEFT_ALT: case GLFW_KEY_RIGHT_ALT: return 18;
        case GLFW_KEY_LEFT: return 37;
        case GLFW_KEY_UP: return 38;
        case GLFW_KEY_RIGHT: return 39;
        case GLFW_KEY_DOWN: return 40;
        case GLFW_KEY_HOME: return 36;
        case GLFW_KEY_END: return 35;
        case GLFW_KEY_PAGE_UP: return 33;
        case GLFW_KEY_PAGE_DOWN: return 34;
        case GLFW_KEY_DELETE: return 46;
        case GLFW_KEY_KP_ENTER:return 13;case GLFW_KEY_KP_DIVIDE:return 111;case GLFW_KEY_KP_MULTIPLY:return 106;case GLFW_KEY_KP_SUBTRACT:return 109;case GLFW_KEY_KP_ADD:return 107;case GLFW_KEY_KP_DECIMAL:return 110;case GLFW_KEY_KP_EQUAL:return 187;
        case GLFW_KEY_COMMA:return 188;case GLFW_KEY_PERIOD:return 190;case GLFW_KEY_GRAVE_ACCENT:return 192;
        case GLFW_KEY_MINUS:return 189;case GLFW_KEY_EQUAL:return 187;case GLFW_KEY_LEFT_BRACKET:return 219;case GLFW_KEY_RIGHT_BRACKET:return 221;
        case GLFW_KEY_BACKSLASH:return 220;case GLFW_KEY_SEMICOLON:return 186;case GLFW_KEY_APOSTROPHE:return 222;case GLFW_KEY_SLASH:return 191;
		case GLFW_KEY_ENTER: return Keyboard::KEY_RETURN;
		default: return glfwkey >= 0 && glfwkey < 256 ? glfwkey : 0;
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if(action == GLFW_REPEAT) return;

	if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
		GLFWmonitor* monitor = glfwGetWindowMonitor(window);
		if (monitor) {
			// Currently fullscreen → go windowed
			glfwSetWindowMonitor(window, NULL, 80, 80, 854, 480, 0);
		} else {
			// Currently windowed → go fullscreen on primary monitor
			GLFWmonitor* primary = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(primary);
			glfwSetWindowMonitor(window, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		return;
	}

	int translated = transformKey(key);
    if (translated) Keyboard::feed(translated, action);
}

void character_callback(GLFWwindow* window, unsigned int codepoint) {
	Keyboard::feedCodepoint(codepoint);
}

static bool resetCursorTracking=true;
static void window_focus_callback(GLFWwindow*,int){resetCursorTracking=true;Mouse::reset2();}
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	// Disabled cursor mode persists while another client has focus. Ignore
	// background pointer warps and re-anchor on the first focused motion.
	if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
		resetCursorTracking = true;
		Mouse::reset2();
		return;
	}
	static double lastX = 0.0, lastY = 0.0;
	static int lastMode=-1;
    const int mode=glfwGetInputMode(window,GLFW_CURSOR);
    const bool firstMouse=resetCursorTracking||mode!=lastMode;
    resetCursorTracking=false;lastMode=mode;

	if (firstMouse) {
        Mouse::reset2();
        lastX = xpos;
        lastY = ypos;
    }

	double deltaX = xpos - lastX;
    double deltaY = ypos - lastY;

    lastX = xpos;
    lastY = ypos;

	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
		Mouse::feed(0, 0, xpos, ypos, deltaX, deltaY);
	} else { 
		Mouse::feed( MouseAction::ACTION_MOVE, 0, xpos, ypos);
	}
	Multitouch::feed(0, 0, xpos, ypos, 0);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(action == GLFW_REPEAT) return;

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		Mouse::feed( MouseAction::ACTION_LEFT, action, xpos, ypos);
		Multitouch::feed(1, action, xpos, ypos, 0);
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		Mouse::feed( MouseAction::ACTION_RIGHT, action, xpos, ypos);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	Mouse::feed(3, 0, xpos, ypos, 0, yoffset);
}

void window_size_callback(GLFWwindow* window, int width, int height) {
	if (g_app) g_app->setSize(width, height);
}

void error_callback(int error, const char* desc) {
	printf("Error: %s\n", desc);
}

int main(void) {
	AppContext appContext;

#ifndef STANDALONE_SERVER
	// Platform init.
	appContext.platform = new AppPlatform_glfw();

	glfwSetErrorCallback(error_callback);

	if (!glfwInit()) {
		return 1;
	}

	#ifdef TCPE_GLES3
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#endif

	GLFWwindow* window = glfwCreateWindow(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight(), "TruancyCraftPE", NULL, NULL);
	
	if (window == NULL) {
		return 1;
	}

	glfwSetKeyCallback(window, key_callback);
	glfwSetCharCallback(window, character_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);

	glfwMakeContextCurrent(window);
	#ifdef TCPE_GLES3
    if(!TCPEGL3::initialize()) return 1;
#else
	gladLoadGLES1Loader((GLADloadproc)winGLLoader);
#endif
	glfwSwapInterval(0);
	glPatchDesktopCompat();
#endif

	App* app = new MAIN_CLASS();

	g_app = app;
	((MAIN_CLASS*)g_app)->externalStoragePath = ".";
	((MAIN_CLASS*)g_app)->externalCacheStoragePath = ".";
	g_app->init(appContext);
	g_app->setSize(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());

	// Main event loop
	using clock = std::chrono::steady_clock;
	#ifdef TCPE_GLES3
    const bool qaStats=std::getenv("TCPE_GLES3_QA_STATS")!=nullptr;
    auto qaStart=clock::now();unsigned qaFrames=0;double qaAppMs=0,qaGpuMs=0,qaDrawMs=0;unsigned qaDraws=0;const bool qaSync=std::getenv("TCPE_GLES3_QA_SYNC")!=nullptr;unsigned qaSyncWindows=0;
#endif
	while(!glfwWindowShouldClose(window) && !app->wantToQuit()) {
		auto frameStart = clock::now();

		app->update();
#ifdef TCPE_GLES3
        if(qaStats){auto after=clock::now();qaAppMs+=std::chrono::duration<double,std::milli>(after-frameStart).count();unsigned n;double ms;TCPEGL3::takeFrameStats(n,ms);qaDraws+=n;qaDrawMs+=ms;if(qaSync&&((MAIN_CLASS*)app)->player&&qaSyncWindows<2){glFinish();qaGpuMs+=std::chrono::duration<double,std::milli>(clock::now()-after).count();}}
#endif

		glfwSwapBuffers(window);
		glfwPollEvents();
#ifdef TCPE_GLES3
        if(qaStats){++qaFrames;auto qaNow=clock::now();double seconds=std::chrono::duration<double>(qaNow-qaStart).count();if(seconds>=5){std::fprintf(stderr,"GLES3 visible frame sample: %.1f fps over %.2fs; app %.2fms, GL submission %.2fms, GPU finish %.2fms, draws %.1f\n",qaFrames/seconds,seconds,qaAppMs/qaFrames,qaDrawMs/qaFrames,qaGpuMs/qaFrames,double(qaDraws)/qaFrames);if(qaSync&&((MAIN_CLASS*)app)->player&&qaSyncWindows<2)++qaSyncWindows;qaFrames=0;qaAppMs=qaGpuMs=qaDrawMs=0;qaDraws=0;qaStart=qaNow;}}
#endif

		glfwSwapInterval(((MAIN_CLASS*)app)->options.vsync ? 1 : 0);
		if(((MAIN_CLASS*)app)->options.limitFramerate) {
			auto frameEnd = clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);
			auto target = std::chrono::microseconds(33333); // ~30 fps
			if(elapsed < target)
				std::this_thread::sleep_for(target - elapsed);
		}
	}

	delete app;

	appContext.platform->finish();
	
	delete appContext.platform;
	
#ifndef STANDALONE_SERVER
	// Exit.
	glfwDestroyWindow(window);
	glfwTerminate();
#endif

	return 0;
}

#endif /*MAIN_GLFW_H__*/
