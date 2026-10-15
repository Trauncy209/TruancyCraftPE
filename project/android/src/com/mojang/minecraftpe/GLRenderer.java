package com.mojang.minecraftpe;

import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** Owns the GL context; every native render/input callback runs on this thread. */
public final class GLRenderer implements GLSurfaceView.Renderer {
    private final MainActivity activity;
    private boolean created, newSurface;
    public GLRenderer(MainActivity activity) { this.activity=activity; }
    public void onSurfaceCreated(GL10 gl,EGLConfig config) { newSurface=true; }
    public void onSurfaceChanged(GL10 gl,int width,int height) {
        if(!created){activity.nativeOnCreate(width,height);created=true;}
        if(newSurface){nativeOnSurfaceCreated();newSurface=false;}
        nativeOnSurfaceChanged(width,height);
    }
    public void onDrawFrame(GL10 gl) { if(created)nativeUpdate(); }
    private static native void nativeOnSurfaceCreated();
    private static native void nativeOnSurfaceChanged(int width,int height);
    private static native void nativeUpdate();
}
