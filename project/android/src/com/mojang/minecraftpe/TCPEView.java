package com.mojang.minecraftpe;

import android.opengl.GLSurfaceView;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.text.InputType;
import android.graphics.Rect;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

/** Native TCPE touch gestures and configurable hardware keys share one GL thread. */
public final class TCPEView extends GLSurfaceView {
    private volatile boolean textInputActive;
    // Keep IME context across native surface resizes/restarted input connections.
    private final StringBuilder preview=new StringBuilder();
    private String composing="";
    public void setTextInputActive(boolean active){if(active!=textInputActive){preview.setLength(0);composing="";}textInputActive=active;}
    public TCPEView(MainActivity activity) {
        super(activity);
        // Baseline native frontend currently uses GLES1; GLES3 is a separate gate.
        setEGLContextClientVersion(MainActivity.nativeRendererVersion());
        setEGLConfigChooser(8,8,8,0,16,0);
        setPreserveEGLContextOnPause(true);
        setRenderer(new GLRenderer(activity));
        setFocusable(true);setFocusableInTouchMode(true);requestFocus();
        // Fullscreen legacy windows ignore adjustResize: explicitly resize the
        // native GL surface to the visible frame so chat input remains above IME.
        getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener(){
            public void onGlobalLayout(){
                Rect visible=new Rect();getWindowVisibleDisplayFrame(visible);
                int full=getRootView().getHeight();int[] origin=new int[2];getLocationOnScreen(origin);
                int available=Math.max(1,visible.bottom-origin[1]);
                int wanted=full-available>full/4?available:ViewGroup.LayoutParams.MATCH_PARENT;
                ViewGroup.LayoutParams params=getLayoutParams();if(params!=null&&params.height!=wanted){params.height=wanted;setLayoutParams(params);}
            }
        });
    }
    public void enqueueKey(final KeyEvent event) {
        final int code=event.getKeyCode(),source=event.getSource();
        final boolean down=event.getAction()==KeyEvent.ACTION_DOWN;
        final int unicode=down&&!event.isCtrlPressed()&&!event.isAltPressed()?event.getUnicodeChar():0;
        queueEvent(new Runnable(){public void run(){MainActivity.nativeKeyEvent(code,source,down,unicode);}});
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        final int action=event.getActionMasked();
        if(action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN||action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP){
            final int index=event.getActionIndex(),id=event.getPointerId(index);
            final float x=event.getX(index),y=event.getY(index);
            final boolean down=action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN;
            queueEvent(new Runnable(){public void run(){if(down)MainActivity.nativeMouseDown(id,1,x,y);else MainActivity.nativeMouseUp(id,1,x,y);}});
        }else if(action==MotionEvent.ACTION_MOVE){
            for(int i=0;i<event.getPointerCount();i++){
                final int id=event.getPointerId(i);final float x=event.getX(i),y=event.getY(i);
                queueEvent(new Runnable(){public void run(){MainActivity.nativeMouseMove(id,x,y);}});
            }
        }else if(action==MotionEvent.ACTION_CANCEL){queueEvent(new Runnable(){public void run(){MainActivity.nativeStopThis();}});}
        return true;
    }
    @Override public boolean onCheckIsTextEditor(){return textInputActive;}
    @Override public InputConnection onCreateInputConnection(EditorInfo info){
        if(!textInputActive)return null;
        info.inputType=InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        info.imeOptions=EditorInfo.IME_ACTION_SEND|EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        return new BaseInputConnection(this,false){
            private void replaceComposition(final String next){
                final int previous=composing.codePointCount(0,composing.length());
                if(preview.length()>=composing.length())preview.setLength(preview.length()-composing.length());
                preview.append(next);
                queueEvent(new Runnable(){public void run(){
                    for(int i=0;i<previous;i++){MainActivity.nativeKeyEvent(KeyEvent.KEYCODE_DEL,0x101,true,0);MainActivity.nativeKeyEvent(KeyEvent.KEYCODE_DEL,0x101,false,0);}
                    for(int i=0;i<next.length();){int cp=next.codePointAt(i);i+=Character.charCount(cp);MainActivity.nativeTextChar(cp);}
                }});
            }
            @Override public CharSequence getTextBeforeCursor(int length,int flags){int begin=Math.max(0,preview.length()-Math.max(0,length));if(begin>0&&begin<preview.length()&&Character.isLowSurrogate(preview.charAt(begin)))begin--;return preview.substring(begin);}
            @Override public CharSequence getTextAfterCursor(int length,int flags){return "";}
            @Override public boolean deleteSurroundingTextInCodePoints(int before,int after){int count=Math.max(0,Math.min(before,1024));int units=0,end=preview.length();for(int i=0;i<count&&end>0;i++){int size=Character.charCount(Character.codePointBefore(preview,end));units+=size;end-=size;}return deleteSurroundingText(units+Math.max(0,count-preview.codePointCount(end,preview.length())),0);}
            @Override public boolean commitText(CharSequence text,int cursor){replaceComposition(text.toString());composing="";return true;}
            @Override public boolean setComposingText(CharSequence text,int cursor){String next=text.toString();replaceComposition(next);composing=next;return true;}
            @Override public boolean finishComposingText(){composing="";return true;}
            @Override public boolean deleteSurroundingText(int before,int after){
                int remaining=Math.max(0,Math.min(before,1024)),deleted=0;
                while(remaining>0&&preview.length()>0){int units=Character.charCount(Character.codePointBefore(preview,preview.length()));preview.setLength(preview.length()-units);remaining-=units;deleted++;}
                final int count=deleted+Math.max(0,remaining);composing="";
                queueEvent(new Runnable(){public void run(){for(int i=0;i<count;i++){MainActivity.nativeKeyEvent(KeyEvent.KEYCODE_DEL,0x101,true,0);MainActivity.nativeKeyEvent(KeyEvent.KEYCODE_DEL,0x101,false,0);}}});return true;
            }
            @Override public boolean sendKeyEvent(KeyEvent event){enqueueKey(event);return true;}
            @Override public boolean performEditorAction(int action){preview.setLength(0);composing="";queueEvent(new Runnable(){public void run(){MainActivity.nativeKeyEvent(KeyEvent.KEYCODE_ENTER,0x101,true,0);MainActivity.nativeKeyEvent(KeyEvent.KEYCODE_ENTER,0x101,false,0);}});return true;}
        };
    }
}
