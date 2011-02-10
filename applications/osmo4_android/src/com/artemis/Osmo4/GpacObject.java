/*
 *  Osmo on Android
 *  Aug/2010
 *  NGO Van Luyen
 *
 *
 */
package com.artemis.Osmo4;

//import android.graphics.Bitmap;

//----------------------------------------------------------------------------------------
public class GpacObject {

    static {
    	System.loadLibrary("javaenv");
    	System.loadLibrary("mad");
    	System.loadLibrary("editline");
        System.loadLibrary("ft2");
        System.loadLibrary("js_osmo");
        System.loadLibrary("openjpeg");
        System.loadLibrary("jpeg");
        System.loadLibrary("png");
        System.loadLibrary("z");
        System.loadLibrary("ffmpeg");
        System.loadLibrary("faad");        
        System.loadLibrary("gpac");
        

        System.loadLibrary("gpacWrapper");
    }

    public static native void gpacinit(Object  bitmap, int width, int height, String cfg_dir, String modules_dir, String cache_dir, String font_dir);
    public static native void gpacconnect(String url);
    public static native void gpacdisconnect();
    public static native void gpacrender(Object  bitmap);
    public static native void gpacresize(int width, int height);
    public static native void gpacfree();

    public static native void gpaceventkeypress(int keycode, int rawkeycode, int up, int flag);
    public static native void gpaceventmousedown(float x, float y);
    public static native void gpaceventmouseup(float x, float y);
    public static native void gpaceventmousemove(float x, float y);

}
