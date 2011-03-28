/**
 *  Osmo on Android
 *  Aug/2010
 *  NGO Van Luyen
 *
 *
 */
package com.artemis.Osmo4;

import java.util.HashMap;
import java.util.Map;
import android.util.Log;

/**
 * GPAC main initialization object
 * 
 * @version $Revision$
 * 
 */
public class GpacObject {

    private final static String LOG_LIB = "LibrariesLoader"; //$NON-NLS-1$

    /**
     * Loads all libraries
     * 
     * @return a map of exceptions containing the library as key and the exception as value. If map is empty, no error
     *         occurred
     */
    static Map<String, Throwable> loadAllLibraries() {
        final String[] toLoad = { "javaenv", //$NON-NLS-1$
                                 "mad", "editline", "ft2", //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$
                                 "js_osmo", "openjpeg", "jpeg", "png", "z", //$NON-NLS-1$ //$NON-NLS-2$//$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
                                 "ffmpeg", "faad", "gpac", "gpacWrapper" }; //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
        HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();
        for (String s : toLoad) {
            try {
                Log.i(LOG_LIB, "Loading library " + s + "..."); //$NON-NLS-1$//$NON-NLS-2$
                System.loadLibrary(s);
            } catch (UnsatisfiedLinkError e) {
                exceptions.put(s, e);
                Log.e(LOG_LIB, "Failed to load library : " + s + " due to link error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
            } catch (SecurityException e) {
                exceptions.put(s, e);
                Log.e(LOG_LIB, "Failed to load library : " + s + " due to security error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
            } catch (Throwable e) {
                exceptions.put(s, e);
                Log.e(LOG_LIB, "Failed to load library : " + s + " due to Runtime error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
            }
        }
        return exceptions;
    }

    /**
     * GPAC initialization routine
     * 
     * @param bitmap
     * @param callback
     * @param width
     * @param height
     * @param cfg_dir
     * @param modules_dir
     * @param cache_dir
     * @param font_dir
     */
    public static native void gpacinit(Object bitmap, GpacCallback callback, int width, int height, String cfg_dir,
            String modules_dir, String cache_dir, String font_dir);

    /**
     * Opens an URL
     * 
     * @param url The URL to open
     */
    public static native void gpacconnect(String url);

    /**
     * Disconnects the currently loaded URL
     */
    public static native void gpacdisconnect();

    /**
     * Renders into a bitmap
     * 
     * @param bitmap
     */
    public static native void gpacrender(Object bitmap);

    /**
     * Resizes the current view
     * 
     * @param width The new width to set
     * @param height The new height to set
     */
    public static native void gpacresize(int width, int height);

    /**
     * Free all GPAC resources
     */
    public static native void gpacfree();

    /**
     * To call when a key has been pressed
     * 
     * @param keycode
     * @param rawkeycode
     * @param up
     * @param flag
     */
    public static native void gpaceventkeypress(int keycode, int rawkeycode, int up, int flag);

    /**
     * To call when a mouse is down
     * 
     * @param x Position in pixels
     * @param y Position in pixels
     */
    public static native void gpaceventmousedown(float x, float y);

    /**
     * To call when a mouse is up (released)
     * 
     * @param x Position in pixels
     * @param y Position in pixels
     */
    public static native void gpaceventmouseup(float x, float y);

    /**
     * To call when a mouse is moving
     * 
     * @param x Position in pixels
     * @param y Position in pixels
     */
    public static native void gpaceventmousemove(float x, float y);

}
