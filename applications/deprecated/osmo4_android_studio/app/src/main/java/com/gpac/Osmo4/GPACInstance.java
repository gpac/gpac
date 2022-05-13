/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4;

import java.io.File;
import java.io.PrintStream;
import java.util.Collections;
import java.util.Date;
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
     *
     * a recursive method to get the directory hierarchy under root.
     *
     * @param sb StringBuilder object which will return the final hierarchy
     * @param root root of the hierarchy
     * @param inc initial number of spaces.
     */
    private static void listing(StringBuilder sb, File root, int inc) {
        StringBuilder increment = new StringBuilder();
        for (int i = 0; i < inc; i++)
            increment.append(' ');
        String incr = increment.toString();
        for (File f : root.listFiles()) {
            sb.append(incr).append(f.getName());
            if (f.isDirectory()) {
                sb.append(" [Directory]\n"); //$NON-NLS-1$
                listing(sb, f, inc + 2);
            } else {
                sb.append(" [").append(f.length() + " bytes]\n"); //$NON-NLS-1$//$NON-NLS-2$
            }
        }
    }

    /**
     * Loads all libraries
     *
     * @param config
     *
     * @return a map of exceptions containing the library as key and the exception as value. If map is empty, no error
     *
     */


    synchronized static Map<String, Throwable> loadAllLibraries(GpacConfig config) {
        if (errors != null)
            return errors;
        StringBuilder sb = new StringBuilder();
        final String[] toLoad = { "GLESv2", "log",
                                 "jpegdroid", "javaenv",
                                 "mad", "ft2", "faad",
                                 "openjpeg", "png", "z",
                                 "avutil", "swscale", "swresample", "avcodec", "avformat", "avfilter", "avdevice",
                                 "gpacWrapper", "gpac",
                                 "gm_droid_mpegv" };
        HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();
        for (String s : toLoad) {
            try {
                String msg = "Loading library " + s + "...";//$NON-NLS-1$//$NON-NLS-2$
                sb.append(msg);
                Log.i(LOG_LIB, msg);
                System.loadLibrary(s);
            } catch (UnsatisfiedLinkError e) {
                sb.append("Failed to load " + s + ", error=" + e.getLocalizedMessage() + " :: " //$NON-NLS-1$//$NON-NLS-2$//$NON-NLS-3$
                          + e.getClass().getSimpleName() + "\n"); //$NON-NLS-1$
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

        if (!exceptions.isEmpty()) {
            try {
                PrintStream out = new PrintStream(config.getGpacLogDirectory() + "debug_libs.txt", "UTF-8"); //$NON-NLS-1$//$NON-NLS-2$
                out.println("$Revision$"); //$NON-NLS-1$
                out.println(new Date());
                out.println("\n*** Configuration\n"); //$NON-NLS-1$
                out.println(config.getConfigAsText());
                sb.append("*** Libs listing: "); //$NON-NLS-1$
                sb.append(config.getGpacLibsDirectory());
                sb.append('\n');
                listing(sb, new File(config.getGpacLibsDirectory()), 2);
                sb.append("*** Modules listing: "); //$NON-NLS-1$
                sb.append(config.getGpacModulesDirectory());
                sb.append('\n');
                listing(sb, new File(config.getGpacModulesDirectory()), 2);
                sb.append("*** Fonts listing: \n"); //$NON-NLS-1$
                sb.append(config.getGpacFontDirectory());
                sb.append('\n');
                listing(sb, new File(config.getGpacFontDirectory()), 2);
                sb.append("*** Exceptions:\n"); //$NON-NLS-1$
                for (Map.Entry<String, Throwable> ex : exceptions.entrySet()) {
                    sb.append(ex.getKey()).append(": ") //$NON-NLS-1$
                      .append(ex.getValue().getLocalizedMessage())
                      .append('(')
                      .append(ex.getValue().getClass())
                      .append(")\n"); //$NON-NLS-1$
                }
                out.println(sb.toString());
                out.flush();
                out.close();
            } catch (Exception e) {
                Log.e(LOG_LIB, "Failed to output debug info to debug file", e); //$NON-NLS-1$
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
     * @param config The configuration to use for GPAC
     * @param urlToOpen
     * @throws GpacInstanceException
     */
    public GPACInstance(GpacCallback callback, int width, int height, GpacConfig config, String urlToOpen)
            throws GpacInstanceException {
        StringBuilder sb = new StringBuilder();
        Map<String, Throwable> errors = loadAllLibraries(config);
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
            handle = createInstance(callback,
                                    width,
                                    height,
                                    config.getGpacConfigDirectory(),
                                    config.getGpacModulesDirectory(),
                                    config.getGpacCacheDirectory(),
                                    config.getGpacFontDirectory(),
                                    config.getGpacGuiDirectory(),
                                    urlToOpen);
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
        gpacrender();
        gpacdisconnect();
    }

    @Override
    public void connect(String url) {
        Log.i(LOG_LIB, "connect(" + url + ")"); //$NON-NLS-1$ //$NON-NLS-2$
        checkCurrentThread();
        gpacrender();
        gpacconnect(url);
    }

    /**
     * Call this method when a key has been pressed
     *
     * @param keyCode
     * @param event
     * @param pressed true if key is pressed, false if key is released
     * @param unicode
     */
    public void eventKey(int keyCode, KeyEvent event, boolean pressed, int unicode) {
        checkCurrentThread();
        gpaceventkeypress(keyCode, event.getScanCode(), pressed ? 1 : 0, event.getFlags(), unicode);
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

    private boolean touched = false;

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
            // not in 1.6 case MotionEvent.ACTION_POINTER_1_DOWN:
            case MotionEvent.ACTION_DOWN:
               gpaceventmousemove(x, y);
               gpaceventmousedown(x, y);
               touched = true;
               break;
            // not in 1.6 case MotionEvent.ACTION_POINTER_1_UP:
            case MotionEvent.ACTION_UP:
            	if ( !touched )
            		break;
            	gpaceventmouseup(x, y);
              touched = false;
              break;
            case MotionEvent.ACTION_MOVE:
            	gpaceventmousemove(x, y);
            	if ( !touched ) {
            		touched = true;
            		//Log.i("Osmo4", "Mouse down: " + x +"," + y);
            		gpaceventmousedown(x, y);
            	}
            	break;
            case MotionEvent.ACTION_CANCEL:
            	if ( !touched )
            		break;
            	//Log.i("Osmo4", "Mouse up: " + x +"," + y);
              gpaceventmouseup(x, y);
              touched = false;
              break;
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
     * @param gui_dir
     * @return
     */
    private native long createInstance(GpacCallback callback, int width, int height, String cfg_dir,
            String modules_dir, String cache_dir, String font_dir, String gui_dir, String url_to_open);

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
    private native void gpaceventkeypress(int keycode, int rawkeycode, int up, int flag, int unicode);

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

    /**
     * To call when a change in orientation is returned from the sensors
     * All angles are in RADIANS and positive in the COUNTER-CLOCKWISE direction
     *
     * @param x yaw (rotation around the -Z axis) range: [-PI, PI]
     * @param y pitch (rotation around the -X axis) range: [-PI/2, PI/2]
     * @param z roll (rotation around the Y axis) range: [-PI, PI]
     */
    private native void gpaceventorientationchange(float x, float y, float z);

    public void onOrientationChange(float yaw, float pitch, float roll){
        gpaceventorientationchange(yaw, pitch, roll);
    }

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

    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#setGpacPreference(String, String, String)
     */
    @Override
    public native void setGpacPreference(String category, String name, String value);



    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#setGpacLogs(String)
     */
    @Override
    public native void setGpacLogs(String tools_at_levels);
}
