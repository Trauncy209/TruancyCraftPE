#include "App.h"
#include "network/java/JavaGameSession.h"
#include "platform/input/AndroidKeyAdapter.h"
#include "AppPlatform_android.h"

//#include "main_android_java.h"
#include "platform/input/Multitouch.h"
#include "platform/input/TouchMouseOwner.h"
static TouchMouseOwner touchMouseOwner;

#include <unistd.h>
#include <sys/syscall.h>
#define gettid() ((int)syscall(SYS_gettid))

// Horrible, I know. / A
#ifndef MAIN_CLASS
#include "main.cpp"
#endif


// References for JNI
static jobject g_pActivity  = 0;
static AppPlatform_android appPlatform;

static std::string getPathString(JNIEnv* env, jobject file)
{
    if (!file)
        return std::string();

    jclass fileClass = env->GetObjectClass(file);
    jmethodID fileMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring pathString = (jstring)env->CallObjectMethod(file, fileMethod);
    const char* str = env->GetStringUTFChars(pathString, NULL);
    std::string path(str ? str : "");
    env->ReleaseStringUTFChars(pathString, str);
    env->DeleteLocalRef(pathString);
    env->DeleteLocalRef(fileClass);
    return path;
}

static void setupExternalPath(JNIEnv* env, MAIN_CLASS* app)
{
    LOGI("setupExternalPath");

    if (env)
    {
        LOGI("Environment exists");
    }

    jclass activityClass = env->GetObjectClass(g_pActivity);
    jmethodID getExternalFilesDir = env->GetMethodID(activityClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    jmethodID getExternalCacheDir = env->GetMethodID(activityClass, "getExternalCacheDir", "()Ljava/io/File;");
    jmethodID getFilesDir = env->GetMethodID(activityClass, "getFilesDir", "()Ljava/io/File;");
    jmethodID getCacheDir = env->GetMethodID(activityClass, "getCacheDir", "()Ljava/io/File;");

    std::string writablePath;
    std::string cachePath;
    std::string internalFilesPath;
    std::string internalCachePath;

    if (getExternalFilesDir) {
        jobject appFilesDir = env->CallObjectMethod(g_pActivity, getExternalFilesDir, NULL);
        writablePath = getPathString(env, appFilesDir);
        if (appFilesDir)
            env->DeleteLocalRef(appFilesDir);
    }

    if (getExternalCacheDir) {
        jobject appCacheDir = env->CallObjectMethod(g_pActivity, getExternalCacheDir);
        cachePath = getPathString(env, appCacheDir);
        if (appCacheDir)
            env->DeleteLocalRef(appCacheDir);
    }

    if (getFilesDir) {
        jobject appFilesDir = env->CallObjectMethod(g_pActivity, getFilesDir);
        internalFilesPath = getPathString(env, appFilesDir);
        if (appFilesDir)
            env->DeleteLocalRef(appFilesDir);
    }

    if (getCacheDir) {
        jobject appCacheDir = env->CallObjectMethod(g_pActivity, getCacheDir);
        internalCachePath = getPathString(env, appCacheDir);
        if (appCacheDir)
            env->DeleteLocalRef(appCacheDir);
    }

    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    // App-owned storage works without broad storage permission. The legacy
    // shared directory is imported by MainActivity; never use it for new saves.
    if (writablePath.empty())
        writablePath = internalFilesPath;
    if (cachePath.empty())
        cachePath = internalCachePath;
    if (cachePath.empty())
        cachePath = writablePath;

    if(!internalFilesPath.empty()){const std::string authPath=internalFilesPath+"/tcpe-accounts";setenv("TCPE_JAVA_AUTH_DIR",authPath.c_str(),1);}
    app->externalStoragePath = writablePath;
    app->externalCacheStoragePath = cachePath;

    if (!writablePath.empty() && chdir(writablePath.c_str()) != 0) {
        LOGI("chdir to %s failed: %s", writablePath.c_str(), strerror(errno));
        if (!internalFilesPath.empty() && writablePath != internalFilesPath) {
            writablePath = internalFilesPath;
            if(!internalFilesPath.empty()){const std::string authPath=internalFilesPath+"/tcpe-accounts";setenv("TCPE_JAVA_AUTH_DIR",authPath.c_str(),1);}
    app->externalStoragePath = writablePath;
            if (chdir(writablePath.c_str()) != 0) {
                LOGI("fallback chdir to %s failed: %s", writablePath.c_str(), strerror(errno));
            }
        }
    }

    env->DeleteLocalRef(activityClass);
}

static void pointerDown(int pointerId, int x, int y) {
    Multitouch::feed(1, 1, x, y, pointerId);
}
static void pointerUp(int pointerId, int x, int y) {
    Multitouch::feed(1, 0, x, y, pointerId);
}
static void pointerMove(int pointerId, int x, int y) {
    Multitouch::feed(0, 0, x, y, pointerId);
}


static App* gApp = 0;
static AppContext gContext;
static bool g_inNativeOnCreate = false;

extern "C" {
JNIEXPORT jint JNICALL
JNI_OnLoad( JavaVM * vm, void * reserved )
{
    LOGI("Entering OnLoad\n");
    return appPlatform.init(vm);
}

// Register/save a reference to the java main activity instance
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeRegisterThis(JNIEnv* env, jobject clazz) {
    LOGI("@RegisterThis\n");
    g_pActivity = (jobject)env->NewGlobalRef( clazz );
}

// Unregister/delete the reference to the java main activity instance
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeUnregisterThis(JNIEnv* env, jobject clazz) {
    LOGI("@UnregisterThis\n");
    if(g_pActivity)env->DeleteGlobalRef( g_pActivity );
    g_pActivity=0;appPlatform.instance=0;
}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeOnCreate(JNIEnv* env, jobject thiz, jint screenWidth, jint screenHeight) {
    LOGI("@nativeOnCreate w=%d h=%d\n", (int)screenWidth, (int)screenHeight);
    g_inNativeOnCreate = true;

    appPlatform.instance = g_pActivity;
    appPlatform.setScreenDimensions((int)screenWidth, (int)screenHeight);
    LOGI("nativeOnCreate: screen set, no initConsts needed\n");
    gContext.doRender = false;
    gContext.platform = &appPlatform;

    LOGI("nativeOnCreate: creating gApp\n");
    gApp = new MAIN_CLASS();
    LOGI("nativeOnCreate: gApp=%p\n", gApp);
    setupExternalPath(env, (MAIN_CLASS*)gApp);
    if (env->ExceptionOccurred()) {
        LOGI("nativeOnCreate: exception after setupExternalPath!\n");
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    LOGI("nativeOnCreate: done\n");
    g_inNativeOnCreate = false;
    //gApp->init(gContext);
}

JNIEXPORT jint JNICALL Java_com_mojang_minecraftpe_MainActivity_nativeRendererVersion(JNIEnv*,jclass) {
#ifdef TCPE_GLES3
 return 3;
#else
 return 1;
#endif
}
static int s_surfaceCreatedCount = 0;

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_GLRenderer_nativeOnSurfaceCreated(JNIEnv* env,jclass) {
#ifdef TCPE_GLES3
    TCPEGL3::resetContext();
    if(!TCPEGL3::initialize()){LOGE("GLES3 initialization failed");return;}
    LOGI("Android GLES3 context version=%s renderer=%s",glGetString(GL_VERSION),glGetString(GL_RENDERER));
#endif
    s_surfaceCreatedCount++;
    if (g_inNativeOnCreate) {
        // Skip re-entrant surface callbacks that fire during nativeOnCreate
        return;
    }
    LOGI("@nativeOnSurfaceCreated #%d tid=%d\n", s_surfaceCreatedCount, (int)gettid());

     if (gApp) {
         // Don't call onGraphicsReset the first time
        if (gApp->isInited()) {
            LOGI("nativeOnSurfaceCreated: calling onGraphicsReset\n");
            gApp->onGraphicsReset(gContext);
        }

        if (!gApp->isInited()) {
            LOGI("nativeOnSurfaceCreated: calling init\n");
            gApp->init(gContext);
            LOGI("nativeOnSurfaceCreated: init done, isInited=%d\n", (int)gApp->isInited());
        }
     }
}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_GLRenderer_nativeOnSurfaceChanged(JNIEnv* env, jclass cls, jint w, jint h) {
    LOGI("@nativeOnSurfaceChanged: %p\n", pthread_self());
    appPlatform.setScreenDimensions(w,h);

    if (gApp) {
        gApp->setSize(w, h);

        if (!gApp->isInited())
            gApp->init(gContext);

        if (!gApp->isInited())
            LOGI("nativeOnSurfaceChanged: NOT INITED!\n");
    }
}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeOnDestroy(JNIEnv* env,jclass) {
    LOGI("@nativeOnDestroy\n");

    delete gApp;
    gApp = 0;
    //gApp->onGraphicsReset(gContext);
}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_GLRenderer_nativeUpdate(JNIEnv* env,jclass) {
    //LOGI("@nativeUpdate: %p\n", pthread_self());
    if (gApp) {
        if (!gApp->isInited())
            gApp->init(gContext);

        gApp->update();

        if (gApp->wantToQuit())
            appPlatform.finish();
    }
}

//
// Keyboard events
//
namespace {
int convertAndroidJavaKeyCode(jint keyCode,bool down,jint inputSource=0) {
    static AndroidKeyState state;
    auto* game=static_cast<Minecraft*>(gApp);
    AndroidControllerBindings bindings;
    if(game&&game->isInited())bindings=androidControllerBindings(game->options);
    int source=inputSource?inputSource:(keyCode>=96&&keyCode<=110?0x401:0x101);
    return state.translate(keyCode,source,game&&game->screen,bindings,down);
}
}

static bool feedAndroidBack(bool down) {
    auto* game=static_cast<Minecraft*>(gApp);
    if(!game||!game->isInited())return false;
    // Android Back remains available independently of customized keyboard binds.
    Keyboard::feed(game->screen?Keyboard::KEY_ESCAPE:ControllerKeys::Pause,down);
    return true;
}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeKeyEvent(JNIEnv*,jclass,jint code,jint source,jboolean down,jint unicode) {
    if(code==4){feedAndroidBack(down==JNI_TRUE);return;} // Android KEYCODE_BACK
    int translated=convertAndroidJavaKeyCode(code,down==JNI_TRUE,source);
    if(translated)Keyboard::feed(translated,down==JNI_TRUE);
    if(down==JNI_TRUE&&unicode>0)Keyboard::feedCodepoint(static_cast<uint32_t>(unicode));
}
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeStopThis(JNIEnv*,jclass) {
    touchMouseOwner.cancel();
    for(int key=0;key<256;++key)if(Keyboard::isKeyDown(key))Keyboard::feed(key,false);
    mouseUp(1,Mouse::getX(),Mouse::getY());mouseUp(2,Mouse::getX(),Mouse::getY());
    for(int i=0;i<Multitouch::MAX_POINTERS;++i)if(Multitouch::isPointerDown(i))pointerUp(i,Multitouch::getX(i),Multitouch::getY(i));
    auto* game=static_cast<Minecraft*>(gApp);
    if(game&&game->javaSession){game->javaSession->connection.send("{\"type\":\"walk\",\"forward\":0,\"strafe\":0,\"jump\":false,\"sneak\":false}");game->javaSession->connection.send("{\"type\":\"mine\",\"held\":false}");game->javaSession->connection.send("{\"type\":\"flight\",\"flying\":false,\"vertical\":0,\"toggle\":false}");game->javaSession->connection.send("{\"type\":\"release_use\"}");}
}

JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeOnKeyDown(JNIEnv* env, jclass cls, jint keyCode) {
    LOGI("@nativeOnKeyDown: %d\n", keyCode);
    int translated=convertAndroidJavaKeyCode(keyCode,true);
    if(translated)Keyboard::feed(translated, true);
}
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeTextChar(JNIEnv* env, jclass cls, jint unicodeChar) {
    if (unicodeChar > 0)
        Keyboard::feedCodepoint(static_cast<uint32_t>(unicodeChar));
}
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeOnKeyUp(JNIEnv* env, jclass cls, jint keyCode) {
    LOGI("@nativeOnKeyUp: %d\n", (int)keyCode);
    int translated=convertAndroidJavaKeyCode(keyCode,false);
    if(translated)Keyboard::feed(translated, false);
}

JNIEXPORT jboolean JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeHandleBack(JNIEnv* env, jclass cls, jboolean isDown) {
    LOGI("@nativeHandleBack: %d\n", isDown);
    return feedAndroidBack(isDown==JNI_TRUE)?JNI_TRUE:JNI_FALSE;
}

//
// Mouse events
//
static bool observeTouchScreen(){auto* game=static_cast<Minecraft*>(gApp);touchMouseOwner.observe(reinterpret_cast<std::uintptr_t>(game?game->screen:nullptr));return game&&game->screen;}
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeMouseDown(JNIEnv* env, jclass cls, jint pointerId, jint buttonId, jfloat x, jfloat y) {
    //LOGI("@nativeMouseDown: %f %f\n", x, y);
    const bool menu=observeTouchScreen();
    if(!menu||touchMouseOwner.down(pointerId))mouseDown(1, x, y);
    pointerDown(pointerId, x, y);
}
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeMouseUp(JNIEnv* env, jclass cls, jint pointerId, jint buttonId, jfloat x, jfloat y) {
    //LOGI("@nativeMouseUp: %f %f\n", x, y);
    const bool menu=observeTouchScreen();
    if(!menu||touchMouseOwner.up(pointerId))mouseUp(1, x, y);
    pointerUp(pointerId, x, y);
}
JNIEXPORT void JNICALL
Java_com_mojang_minecraftpe_MainActivity_nativeMouseMove(JNIEnv* env, jclass cls, jint pointerId, jfloat x, jfloat y) {
    //LOGI("@nativeMouseMove: %f %f\n", x, y);
    const bool menu=observeTouchScreen();
    if(!menu||touchMouseOwner.move(pointerId))mouseMove(x, y);
    pointerMove(pointerId, x, y);
}
}
