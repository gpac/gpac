package com.artemis.Osmo4;

import java.io.File;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.opengl.GLSurfaceView;
import android.os.Environment;
import android.util.Log;

/**
 * The renderer
 * 
 * @version $Revision$
 * 
 */
public class Osmo4Renderer implements GLSurfaceView.Renderer {

    /**
     * Default directory for GPAC configuration directory, ends with /
     */
    public final static String GPAC_CFG_DIR;

    /**
     * Default directory for cached files
     */
    public final static String GPAC_CACHE_DIR;

    /**
     * Default directory for GPAC modules directory, ends with /
     */
    public final static String GPAC_MODULES_DIR;

    /**
     * Creates a given directory if it does not exist
     * 
     * @param path
     */
    private static boolean createDirIfNotExist(String path) {
        File f = new File(path);
        if (!f.exists()) {
            if (!f.mkdirs()) {
                Log.e(LOG_RENDERER, "Failed to create directory " + path); //$NON-NLS-1$
                return false;
            } else {
                Log.i(LOG_RENDERER, "Created directory " + path); //$NON-NLS-1$
            }
        }
        return true;
    }

    // ------------------------------------
    private static void initGPACDirs() {
        createDirIfNotExist(GPAC_CFG_DIR);
        createDirIfNotExist(GPAC_CACHE_DIR);
        createDirIfNotExist(GPAC_MODULES_DIR);
    }

    private final static String LOG_RENDERER = Osmo4Renderer.class.getSimpleName();

    /**
     * Static block loaded when class is loaded
     */
    static {
        File rootCfg = Environment.getExternalStorageDirectory();
        File osmo = new File(rootCfg, "osmo"); //$NON-NLS-1$
        GPAC_CFG_DIR = osmo.getAbsolutePath() + '/';
        Log.v(LOG_RENDERER, "Using directory " + GPAC_CFG_DIR + " for osmo"); //$NON-NLS-1$ //$NON-NLS-2$
        GPAC_CACHE_DIR = new File(osmo, "cache").getAbsolutePath() + '/'; //$NON-NLS-1$
        Log.v(LOG_RENDERER, "Using directory " + GPAC_CACHE_DIR + " for cache"); //$NON-NLS-1$ //$NON-NLS-2$
        GPAC_MODULES_DIR = (new File(Environment.getDataDirectory() + "/data", "com.artemis.Osmo4")).getAbsolutePath() + '/'; //$NON-NLS-1$ //$NON-NLS-2$
        Log.v(LOG_RENDERER, "Using directory " + GPAC_MODULES_DIR + " for modules"); //$NON-NLS-1$ //$NON-NLS-2$
        initGPACDirs();
    };

    /**
     * Directory of fonts
     */
    public final static String GPAC_FONT_DIR = "/system/fonts/"; //$NON-NLS-1$

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        if (instance == null) {
            try {
                Log.i(LOG_RENDERER, "Creating instance from thread " + Thread.currentThread()); //$NON-NLS-1$
                instance = new GPACInstance(callback,
                                            320,
                                            430,
                                            GPAC_CFG_DIR,
                                            GPAC_MODULES_DIR,
                                            GPAC_CACHE_DIR,
                                            GPAC_FONT_DIR,
                                            urlToLoad);
            } catch (com.artemis.Osmo4.GPACInstanceInterface.GpacInstanceException e) {
                Log.e(LOG_RENDERER, "Failed to create new GPAC instance !"); //$NON-NLS-1$
                instance = null;
                callback.onGPACError(e);
                return;
            }
            if (callback != null)
                callback.onGPACReady();
        }
    }

    private GPACInstance instance = null;

    private final String urlToLoad;

    /**
     * @return the urlToLoad
     */
    public synchronized String getUrlToLoad() {
        return urlToLoad;
    }

    /**
     * Constructor
     * 
     * @param callback
     * @param urlToLoad The URL to load at startup, can be null
     */
    public Osmo4Renderer(GpacCallback callback, String urlToLoad) {
        this.callback = callback;
        this.urlToLoad = urlToLoad;
    }

    private final GpacCallback callback;

    @Override
    public void onSurfaceChanged(GL10 gl, int w, int h) {
        // gl.glViewport(0, 0, w, h);
        if (instance != null) {
            Log.i(LOG_RENDERER, "Surface changed from thread " + Thread.currentThread()); //$NON-NLS-1$
            instance.resize(w, h);
        }
    }

    private int frames;

    private long startFrame = System.currentTimeMillis();

    @Override
    public void onDrawFrame(GL10 gl) {
        if (instance != null) {
            frames++;
            if (frames % 1000 == 0) {
                long now = System.currentTimeMillis();
                Log.i(LOG_RENDERER, "Frames Per Second = " + ((now - startFrame) / 1000) + " fps"); //$NON-NLS-1$//$NON-NLS-2$
                this.startFrame = now;
            }
            instance.render();
        }
    }

    /**
     * Get the current GPAC Instance
     * 
     * @return the instance
     */
    synchronized GPACInstance getInstance() {
        return instance;
    }
}
