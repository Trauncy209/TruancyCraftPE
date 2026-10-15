package com.mojang.minecraftpe;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Map;

import com.mojang.android.StringValue;
import com.mojang.android.licensing.LicenseCodes;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.BitmapFactory;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Build;
import android.os.Environment;
import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.view.inputmethod.*;
import com.mojang.minecraftpe.R;

public class MainActivity extends Activity {
    private TCPEView gameView;
    private final TcpeAudio tcpeAudio=new TcpeAudio(this);
    public void playSound(String filename,float volume,float pitch){tcpeAudio.command(filename,volume,pitch);}

	private boolean _isTouchscreen = true;
	private int _viewDistance = 2;
	
	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
    	getOptionStrings(); // Updates settings
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        super.onCreate(savedInstanceState);
        nativeRegisterThis();
        gameView=new TCPEView(this);setContentView(gameView);
        prepareLegacyWorldImport();
    }
    private static final int LEGACY_WORLD_PERMISSION = 5221;
    private void prepareLegacyWorldImport() {
        File oldWorlds = new File(Environment.getExternalStorageDirectory(), "games/com.mojang/minecraftWorlds");
        if (Build.VERSION.SDK_INT >= 23 && checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            // Only storage import needs this; networking/app-owned saves work without it.
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, LEGACY_WORLD_PERMISSION);
            return;
        }
        importLegacyWorlds(oldWorlds);
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == LEGACY_WORLD_PERMISSION && results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) importLegacyWorlds(new File(Environment.getExternalStorageDirectory(), "games/com.mojang/minecraftWorlds"));
    }
    private void importLegacyWorlds(final File source) {
        File owned = getExternalFilesDir(null); if (owned == null) owned = getFilesDir();
        final File destination = new File(owned, "games/com.mojang/minecraftWorlds");
        new Thread(new Runnable() { public void run() {
            try { final LegacyWorldMigration.Result result = LegacyWorldMigration.copyWorlds(source, destination);
                if (result.imported > 0 || result.failed > 0) runOnUiThread(new Runnable() { public void run() {
                    new AlertDialog.Builder(MainActivity.this).setTitle("Your worlds").setMessage(result.failed == 0 ? "Your old worlds are ready in Start Game. The originals were kept." : "Some old worlds could not be copied. The originals were kept.").setPositiveButton("OK", null).show();
                }});
            } catch (IOException ignored) { /* Existing legacy worlds remain untouched. */ }
        }}, "TCPE world import").start();
    }
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Dialog text fields retain their Android input connection.
        if(gameView!=null&&getCurrentFocus()==gameView&&(event.getAction()==KeyEvent.ACTION_DOWN||event.getAction()==KeyEvent.ACTION_UP)) {gameView.enqueueKey(event);return true;}
        return super.dispatchKeyEvent(event);
    }
    @Override public void onBackPressed(){
        if(gameView!=null)gameView.queueEvent(new Runnable(){public void run(){nativeHandleBack(true);nativeHandleBack(false);}});
    }

    private final OnCancelListener dialogCancelListener = new OnCancelListener() {
    	@Override
    	public void onCancel(DialogInterface dialog) {
    		onDialogCanceled();
    	}
    };

    private final OnClickListener dialogOkClickListener = new OnClickListener() {
    	@Override
    	public void onClick(DialogInterface dialog, int which) {
    		onDialogCompleted();
    	}
    };

    private final OnClickListener dialogCancelClickListener = new OnClickListener() {
    	@Override
    	public void onClick(DialogInterface dialog, int which) {
    		onDialogCanceled();
    	}
    };

    private void createAlertDialog(boolean hasOkButton, boolean hasCancelButton, boolean preventBackKey) {
    	AlertDialog.Builder builder = new AlertDialog.Builder(this);

    	builder.setTitle("");
    	if (preventBackKey)
    		builder.setCancelable(false);

    	builder.setOnCancelListener(dialogCancelListener);

    	if (hasOkButton)
    		builder.setPositiveButton("Ok", dialogOkClickListener);

    	if (hasCancelButton)
	    	builder.setNegativeButton("Cancel", dialogCancelClickListener);

    	mDialog = builder.create();
    	mDialog.setOwnerActivity(this);
    }

	static private boolean _isPowerVr = false;
    public void setIsPowerVR(boolean status) { MainActivity._isPowerVr = status; }
    static public boolean isPowerVR() { return _isPowerVr; }
    
    public boolean supportsTouchscreen() {
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN);
    }

	static public boolean isXperiaPlay() {
		final String[] tags = {	android.os.Build.MODEL,
								android.os.Build.DEVICE,
								android.os.Build.PRODUCT};
    	for (String tag : tags) {
    		tag = tag.toLowerCase();
        	if (tag.indexOf("r800") >= 0) return true;
        	if (tag.indexOf("so-01d") >= 0) return true;
        	if (tag.indexOf("xperia") >= 0 && tag.indexOf("play") >= 0) return true;
    	}
    	return false;
	}

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
    	// TODO Auto-generated method stub
    	//System.out.println("Focus has changed. Has Focus? " + hasFocus);
    	super.onWindowFocusChanged(hasFocus);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	// TODO Auto-generated method stub
    	//System.out.println("KeyDown: " + keyCode);
    	Log.w("MCPE", "onKeyDown: " + keyCode);
    	return super.onKeyDown(keyCode, event);
    }
    public int getKeyFromKeyCode(int keyCode, int metaState, int deviceId) {
        InputDevice device = InputDevice.getDevice(deviceId);
        return device == null ? 0 : device.getKeyCharacterMap().get(keyCode, metaState);
    }
    static public void saveScreenshot(String filename, int w, int h, int[] pixels) {
    	Bitmap bitmap = Bitmap.createBitmap(pixels, w, h, Bitmap.Config.ARGB_8888);

    	//System.out.println("Save screenshot: " + filename);
    	
    	try {
	        FileOutputStream fos = new FileOutputStream(filename);
	        bitmap.compress(CompressFormat.JPEG, 85, fos);
	    	//System.out.println("Compression completed!");

	        try {
                fos.flush();
	        } catch (IOException e) {
                e.printStackTrace();
	        }

	        try {
                fos.close();
	        } catch (IOException e) {
                e.printStackTrace();
	        }
        }
        catch (FileNotFoundException e) {
        	System.err.println("Couldn't create file: " + filename);
            e.printStackTrace();
        }
    }

    public byte[] getFileDataBytes(String filename) {
    	AssetManager assets = getAssets();

    	BufferedInputStream bis;
    	try {
            InputStream is = assets.open(filename.startsWith("images/") ? filename.substring(7) : filename);
        	bis = new BufferedInputStream(is);
        } catch (IOException e) {
        	e.printStackTrace();
        	return null;
        }

    	ByteArrayOutputStream s = new ByteArrayOutputStream(4096);
    	byte[] tmp = new byte[1024];
    	
    	try {
	    	while (true) {
	    		int count = bis.read(tmp);
	    		if (count <= 0) break;
	    		s.write(tmp, 0, count);
	    	}
    	} catch (IOException e) {
    	} finally {
    		try { bis.close(); }
    		catch (IOException e) {}
    	}

    	return s.toByteArray();
    }
    
    public static native int nativeRendererVersion();

    public void setKeyboardVisible(final boolean visible) {
        runOnUiThread(new Runnable(){ public void run(){
            if(gameView==null)return;
            gameView.setTextInputActive(visible);
            InputMethodManager manager=(InputMethodManager)getSystemService(INPUT_METHOD_SERVICE);
            if(visible){gameView.requestFocus();manager.restartInput(gameView);manager.showSoftInput(gameView,InputMethodManager.SHOW_IMPLICIT);}
            else {manager.hideSoftInputFromWindow(gameView.getWindowToken(),0);manager.restartInput(gameView);}
        }});
    }

    public int[] getImageData(String filename) {
    	AssetManager assets = getAssets();

        try {
        	/*String[] filenames = */assets.list("images");
        } catch (IOException e) {
        	System.err.println("getImageData: Could not list directory");
        	return null;
        }

        InputStream inputStream = null;
        try {
            inputStream = assets.open(filename.startsWith("images/") ? filename.substring(7) : filename);
        } catch (IOException e) {
        	System.err.println("getImageData: Could not open image " + filename);
        	return null;
        }

        Bitmap bm = BitmapFactory.decodeStream(inputStream);
        int w = bm.getWidth();
        int h = bm.getHeight();

        int[] pixels = new int[w * h + 2];
        pixels[0] = w;
        pixels[1] = h;
        bm.getPixels(pixels, 2, w, 0, 0, w, h);

        return pixels;
    }

    public int getScreenWidth() {
    	Display display = ((WindowManager)this.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
    	int out = Math.max(display.getWidth(), display.getHeight());
    	System.out.println("getwidth: " + out);
    	return out;
    }
    public int getScreenHeight() {
    	Display display = ((WindowManager)this.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
    	int out = Math.min(display.getWidth(), display.getHeight());
    	System.out.println("getheight: " + out);
    	return out;
    }
    public float getPixelsPerMillimeter() {
    	 DisplayMetrics metrics = new DisplayMetrics();
    	 getWindowManager().getDefaultDisplay().getMetrics(metrics);
    	 //System.err.println("metrics: " + metrics.xdpi + ", " + metrics.ydpi);
    	 return (metrics.xdpi + metrics.ydpi) * 0.5f / 25.4f;
    }

    public int checkLicense() { return LicenseCodes.LICENSE_OK; }
    
    public String getDateString(int s) {
    	return DateFormat.format(new Date(s * 1000L));
    }

    public boolean hasBuyButtonWhenInvalidLicense() { return true; }
    
    public void postScreenshotToFacebook(String filename, int w, int h, int[] pixels) {
    	return;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
                                    Intent data) {

    	if (requestCode == DialogDefinitions.DIALOG_MAINMENU_OPTIONS) {
    		_userInputStatus = 1;
    	}
    }

    private final Runnable finishRunnable = new Runnable() {
    	@Override
    	public void run() {
    		finish();
    	}
    };

    public void quit() {
		runOnUiThread(finishRunnable);
    }

    public void displayDialog(int dialogId) {

    	if (dialogId == DialogDefinitions.DIALOG_CREATE_NEW_WORLD) {
    		chooseDialog(R.layout.create_new_world,
    			new int[] { R.id.editText_worldName,
    						R.id.editText_worldSeed,
    						R.id.button_gameMode,
    						R.id.button_biomeGrassTint},
    			false, // Don't prevent back key
    			R.id.button_createworld_create,
    			R.id.button_createworld_cancel
    		); 
    	} else if (dialogId == DialogDefinitions.DIALOG_MAINMENU_OPTIONS) { 
    		Intent intent = new Intent(this, MainMenuOptionsActivity.class);
    		intent.putExtra("preferenceId", R.xml.preferences);
    		startActivityForResult(intent, dialogId);
    	} else if (dialogId == DialogDefinitions.DIALOG_RENAME_MP_WORLD) {
    		chooseDialog(R.layout.rename_mp_world,
    			new int[] { R.id.editText_worldNameRename },
    			false
        	);
    	}
    }

    void chooseDialog(final int layoutId, final int[] viewIds) {
    	chooseDialog(layoutId, viewIds, true);
    }
    void chooseDialog(final int layoutId, final int[] viewIds, final boolean hasCancelButton) {
    	chooseDialog(layoutId, viewIds, hasCancelButton, true);
    }
    void chooseDialog(final int layoutId, final int[] viewIds, final boolean hasCancelButton, final boolean preventBackKey) {
    	chooseDialog(layoutId, viewIds, preventBackKey, 0, hasCancelButton? 0 : -1);
    }
    private void handleCustomDialogOk() {
    	if (mDialog != null) mDialog.dismiss();
    	onDialogCompleted();
    }

    private void handleCustomDialogCancel() {
    	if (mDialog != null) mDialog.cancel();
    	onDialogCanceled();
    }

    private final class DialogButtonClickListener implements View.OnClickListener {
    	private final boolean cancel;

    	DialogButtonClickListener(boolean cancel) {
    		this.cancel = cancel;
    	}

    	@Override
    	public void onClick(View v) {
    		if (cancel) handleCustomDialogCancel();
    		else handleCustomDialogOk();
    	}
    }

    private final class ShowDialogRunnable implements Runnable {
    	private final int layoutId;
    	private final int[] viewIds;
    	private final boolean preventBackKey;
    	private final int okButtonId;
    	private final int cancelButtonId;

    	ShowDialogRunnable(int layoutId, int[] viewIds, boolean preventBackKey, int okButtonId, int cancelButtonId) {
    		this.layoutId = layoutId;
    		this.viewIds = viewIds;
    		this.preventBackKey = preventBackKey;
    		this.okButtonId = okButtonId;
    		this.cancelButtonId = cancelButtonId;
    	}

    	@Override
    	public void run() {
    		createAlertDialog(okButtonId == 0, cancelButtonId == 0, preventBackKey);
    		LayoutInflater li = LayoutInflater.from(MainActivity.this);

    		try {
    			View view = li.inflate(layoutId, null);
    			if (okButtonId != 0 && okButtonId != -1) {
    				View b = view.findViewById(okButtonId);
    				if (b != null) b.setOnClickListener(new DialogButtonClickListener(false));
    			}
    			if (cancelButtonId != 0 && cancelButtonId != -1) {
    				View b = view.findViewById(cancelButtonId);
    				if (b != null) b.setOnClickListener(new DialogButtonClickListener(true));
    			}

    			MainActivity.this.mDialog.setView(view);

    			if (viewIds != null)
    				for (int viewId : viewIds) {
    					View v = view.findViewById(viewId);
    					if (v instanceof StringValue)
    						_userInputValues.add((StringValue) v);
    					else if (v instanceof TextView)
    						_userInputValues.add(new TextViewReader((TextView) v));
    				}

    		} catch (Error e) {
    			e.printStackTrace();
    		}

    		MainActivity.this.mDialog.show();
    		MainActivity.this.mDialog.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
    		MainActivity.this.mDialog.getWindow().setLayout(LayoutParams.FILL_PARENT, LayoutParams.MATCH_PARENT);
    	}
    }

    void chooseDialog(final int layoutId, final int[] viewIds, final boolean preventBackKey, final int okButtonId, final int cancelButtonId) {
    	_userInputValues.clear();
    	runOnUiThread(new ShowDialogRunnable(layoutId, viewIds, preventBackKey, okButtonId, cancelButtonId));
    }
    
    public void tick() {}

    public String[] getOptionStrings() {
    	SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
    	Map<String, ?> m = prefs.getAll();
    	
    	String[] tmpOut = new String[m.size() * 2];

    	int n = 0;
    	for (Map.Entry<String, ?> e : m.entrySet()) {
    		// @todo: Would be nice if the disabled settings could be stripped out here

    		String key   = e.getKey();
    		String value = e.getValue().toString(); 

    		//
    		// Feel free to modify key or value!
    		//
    		// This can be used to correct differences between the different
    		// platforms, such as Android not supporting floating point
    		// ranges for Sliders/SeekBars: {0..100} - TRANSFORM -> {0..1}
    		//
    		if (key.equals(MainMenuOptionsActivity.Controls_UseTouchscreen))
    			_isTouchscreen = !isXperiaPlay() || (Boolean) e.getValue(); 

    		if (key.equals(MainMenuOptionsActivity.Graphics_LowQuality))
    			_viewDistance = ((Boolean) e.getValue()) ? 3 : 2;

    		if (key.equals(MainMenuOptionsActivity.Internal_Game_DifficultyPeaceful)) {
    			key = MainMenuOptionsActivity.Game_DifficultyLevel;
    			value = ((Boolean) e.getValue()) ? "0" : "2";
    		}

    		try {
    			if (key.equals(MainMenuOptionsActivity.Controls_Sensitivity))
    				value = new Double( 0.01 * Integer.parseInt(value) ).toString();
    		} catch (Exception exc) {}

    		tmpOut[n++] = key;
    		tmpOut[n++] = value;

    		//System.out.println("Key: " + e.getKey());
    		//System.out.println("Val: " + e.getValue().toString() + " (" + e.getValue().getClass().getName() + ")\n");
    	}

    	// Copy over the enabled preferences
    	String[] out = new String[n];
    	for (int i = 0; i < n; ++i)
    		out[i] = tmpOut[i];
    	
    	return out;
    }

    public void buyGame() {}

    public String getPlatformStringVar(int id) {
    	if (id == 0) return android.os.Build.MODEL;
    	return null;
    }

    public boolean isNetworkEnabled(boolean onlyWifiAllowed) {
    	return true;
    	/*
	    ConnectivityManager cm = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
	    NetworkInfo info = onlyWifiAllowed? cm.getNetworkInfo(ConnectivityManager.TYPE_WIFI)
	    								:	cm.getActiveNetworkInfo();
	  //(info.getState() == NetworkInfo.State.CONNECTED || info.getState() == NetworkInfo.State.CONNECTING));
	    return (info != null && info.isConnectedOrConnecting());
    	 */
    }
    
    private Bundle data;
    private int _userInputStatus = -1;
    private String[] _userInputText = null;
    private ArrayList<StringValue> _userInputValues = new ArrayList<StringValue>();
    public void initiateUserInput(int id) {
    	_userInputText = null;
    	_userInputStatus = -1;
    }
    public int getUserInputStatus() { return _userInputStatus; }
    public String[] getUserInputString() { return _userInputText; }
    
    private AlertDialog mDialog;
    private final DateFormat DateFormat = new SimpleDateFormat();
//    public EditText mTextInputWidget;
    
    public void vibrate(int milliSeconds) {
    	Vibrator v = (Vibrator)this.getSystemService(VIBRATOR_SERVICE);
    	v.vibrate(milliSeconds);
    }

    private void onDialogCanceled() {
    	_userInputStatus = 0;
    }

    private void onDialogCompleted() {
	    int size = _userInputValues.size(); 
	    _userInputText = new String[size];
	    for (int i = 0; i < size; ++i) {
	    	_userInputText[i] = _userInputValues.get(i).getStringValue(); 
	    }
	    for (String s : _userInputText) System.out.println("js: " + s);

	    _userInputStatus = 1;
	    InputMethodManager inputManager = (InputMethodManager)getSystemService("input_method");
        boolean result = inputManager.showSoftInput(this.getCurrentFocus(), InputMethodManager.SHOW_IMPLICIT);
	}
    
    protected void onStart() {
    	//System.out.println("onStart");
    	super.onStart();
    }

    protected void onResume() {
    	//System.out.println("onResume");
    	super.onResume();
        if(gameView!=null)gameView.onResume();
        tcpeAudio.resume();
    }

    protected void onPause() {
    	//System.out.println("onPause");
        if(gameView!=null){gameView.queueEvent(new Runnable(){public void run(){nativeStopThis();}});gameView.onPause();}
        tcpeAudio.pause();
    	super.onPause();
    }

    protected void onStop() {
    	//System.out.println("onStop");
    	super.onStop();
    }
    protected void onDestroy() {
        tcpeAudio.destroy();
    	System.out.println("onDestroy");

        if(gameView!=null)gameView.queueEvent(new Runnable(){public void run(){nativeOnDestroy();nativeUnregisterThis();}});
        else nativeUnregisterThis();
    	super.onDestroy();
    }

    protected boolean isDemo() { return false; }
    
    //
    // Native interface
    //
    native void nativeRegisterThis(); 
    native void nativeUnregisterThis();
    static native void nativeStopThis();
    native void nativeOnCreate(int width,int height);
    static native void nativeOnDestroy();
    static native void nativeKeyEvent(int code,int source,boolean down,int unicode);
    static native void nativeTextChar(int unicode);
    static native boolean nativeHandleBack(boolean down);
    static native void nativeMouseDown(int id,int button,float x,float y);
    static native void nativeMouseUp(int id,int button,float x,float y);
    static native void nativeMouseMove(int id,float x,float y);
    
    static {
        System.loadLibrary("minecraftpe");
    }
}

// see client/gui/screens/DialogDefinitions.h
class DialogDefinitions {
	static final int DIALOG_CREATE_NEW_WORLD = 1;
	static final int DIALOG_NEW_CHAT_MESSAGE = 2;
	static final int DIALOG_MAINMENU_OPTIONS = 3;
	static final int DIALOG_RENAME_MP_WORLD = 4;
}

class TextViewReader implements StringValue {
	public TextViewReader(TextView view) {
		_view = view;
	}
	public String getStringValue() {
		return _view.getText().toString();
	}
	private TextView _view;
}
