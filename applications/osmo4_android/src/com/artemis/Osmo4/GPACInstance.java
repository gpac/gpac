/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.artemis.Osmo4;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;

/**
 * @version $Revision$
 * 
 */
public class GPACInstance implements GPACInstanceInterface {

    private final static String LOG_LIB = "LibrariesLoader"; //$NON-NLS-1$

    private final Thread uniqueThread;

    /**
     * Loads all libraries
     * 
     * @return a map of exceptions containing the library as key and the exception as value. If map is empty, no error
     *         occurred
     */
    synchronized static Map<String, Throwable> loadAllLibraries() {
        if (errors != null)
            return errors;
        final String[] toLoad = { "jpeg", "javaenv", //$NON-NLS-1$ //$NON-NLS-2$
                                 "mad", "editline", "ft2", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
                                 "js_osmo", "openjpeg", "png", "z", //$NON-NLS-1$ //$NON-NLS-2$//$NON-NLS-3$ //$NON-NLS-4$
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
        errors = Collections.unmodifiableMap(exceptions);
        return errors;
    }

    private static Map<String, Throwable> errors = null;

    private boolean hasToBeFreed = true;

    /**
     * Constructor
     * 
     * @param callback
     * @param width
     * @param height
     * @param cfg_dir
     * @param modules_dir
     * @param cache_dir
     * @param font_dir
     * @param urlToOpen
     * @throws GpacInstanceException
     */
    public GPACInstance(GpacCallback callback, int width, int height, String cfg_dir, String modules_dir,
            String cache_dir, String font_dir, String urlToOpen) throws GpacInstanceException {
        StringBuilder sb = new StringBuilder();
        Map<String, Throwable> errors = loadAllLibraries();
        if (!errors.isEmpty()) {
            sb.append("Exceptions while loading libraries:"); //$NON-NLS-1$
            for (Map.Entry<String, Throwable> x : errors.entrySet()) {
                sb.append('\n')
                  .append(x.getKey())
                  .append('[')
                  .append(x.getValue().getClass().getSimpleName())
                  .append("]: ") //$NON-NLS-1$
                  .append(x.getValue().getLocalizedMessage());
            }
            Log.e(LOG_LIB, sb.toString());
        }
        try {
            handle = createInstance(callback, width, height, cfg_dir, modules_dir, cache_dir, font_dir, urlToOpen);
        } catch (Throwable e) {
            throw new GpacInstanceException("Error while creating instance\n" + sb.toString()); //$NON-NLS-1$
        }
        if (handle == 0) {
            throw new GpacInstanceException("Error while creating instance, no handle created!\n" + sb.toString()); //$NON-NLS-1$
        }
        synchronized (this) {
            hasToBeFreed = true;
        }
        uniqueThread = Thread.currentThread();
    }

    /**
     * This one handles the pointer to the real C object
     */
    private final long handle;

    /**
     * @return the handle
     */
    public synchronized long getHandle() {
        return handle;
    }

    private void checkCurrentThread() throws RuntimeException {
        if (Thread.currentThread() != uniqueThread)
            throw new RuntimeException("Method called outside allowed Thread scope !"); //$NON-NLS-1$
    }

    @Override
    public void disconnect() {
        checkCurrentThread();
        gpacdisconnect();
    }

    @Override
    public void connect(String url) {
        Log.i(LOG_LIB, "connect(" + url + ")"); //$NON-NLS-1$ //$NON-NLS-2$
        checkCurrentThread();
        gpacconnect(url);
    }

    /**
     * Call this method when a key has been pressed
     * 
     * @param keyCode
     * @param event
     * @param pressed true if key is pressed, false if key is released
     */
    public void eventKey(int keyCode, KeyEvent event, boolean pressed) {
        checkCurrentThread();
        gpaceventkeypress(keyCode, event.getScanCode(), pressed ? 1 : 0, event.getFlags());
    }

    /**
     * Renders the current frame
     */
    public void render() {
        checkCurrentThread();
        gpacrender();
    }

    /**
     * Resizes the object to new dimensions
     * 
     * @param width
     * @param height
     */
    public void resize(int width, int height) {
        Log.i(LOG_LIB, "Resizing to " + width + "x" + height); //$NON-NLS-1$ //$NON-NLS-2$
        gpacresize(width, height);
    }

    /**
     * Call this when a motion event occurs
     * 
     * @param event
     */
    public void motionEvent(MotionEvent event) {
        checkCurrentThread();
        final float x = event.getX();
        final float y = event.getY();
        switch (event.getAction()) {
            case MotionEvent.ACTION_POINTER_1_DOWN:
            case MotionEvent.ACTION_DOWN:
                gpaceventmousedown(x, y);
                break;
            case MotionEvent.ACTION_POINTER_1_UP:
            case MotionEvent.ACTION_UP:
                gpaceventmouseup(x, y);
                break;
            case MotionEvent.ACTION_MOVE:
                gpaceventmousemove(x, y);
        }
    }

    @Override
    public void finalize() throws Throwable {
        // Not sure how to do this..., destroy is supposed to be called already
        // destroy();
        super.finalize();
    }

    /**
     * All Native functions, those functions are binded using the handle
     */

    /**
     * Opens an URL
     * 
     * @param url The URL to open
     */
    private native void gpacconnect(String url);

    /**
     * Used to create the GPAC instance
     * 
     * @param callback
     * @param width
     * @param height
     * @param cfg_dir
     * @param modules_dir
     * @param cache_dir
     * @param font_dir
     * @return
     */
    private native long createInstance(GpacCallback callback, int width, int height, String cfg_dir,
            String modules_dir, String cache_dir, String font_dir, String url_to_open);

    /**
     * Disconnects the currently loaded URL
     */
    private native void gpacdisconnect();

    /**
     * Renders
     * 
     * @param handle
     */
    private native void gpacrender();

    /**
     * Resizes the current view
     * 
     * @param width The new width to set
     * @param height The new height to set
     */
    private native void gpacresize(int width, int height);

    /**
     * Free all GPAC resources
     */
    private native void gpacfree();

    /**
     * To call when a key has been pressed
     * 
     * @param keycode
     * @param rawkeycode
     * @param up
     * @param flag
     */
    private native void gpaceventkeypress(int keycode, int rawkeycode, int up, int flag);

    /**
     * To call when a mouse is down
     * 
     * @param x Position in pixels
     * @param y Position in pixels
     */
    private native void gpaceventmousedown(float x, float y);

    /**
     * To call when a mouse is up (released)
     * 
     * @param x Position in pixels
     * @param y Position in pixels
     */
    private native void gpaceventmouseup(float x, float y);

    /**
     * To call when a mouse is moving
     * 
     * @param x Position in pixels
     * @param y Position in pixels
     */
    private native void gpaceventmousemove(float x, float y);

    @Override
    public void destroy() {
        boolean freeIt;
        synchronized (this) {
            freeIt = hasToBeFreed;
            hasToBeFreed = false;
        }
        if (freeIt) {
            disconnect();
            gpacfree();
        }
    }
}
