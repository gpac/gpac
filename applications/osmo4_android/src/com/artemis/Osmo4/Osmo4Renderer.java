package com.artemis.Osmo4;

import java.io.File;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.opengl.GLSurfaceView;
import android.os.Environment;
import android.util.Log;
import com.artemis.Osmo4.GPACInstance.GpacInstanceException;

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

    static {
        File rootCfg = Environment.getExternalStorageDirectory();
        File osmo = new File(rootCfg, "osmo/"); //$NON-NLS-1$
        GPAC_CFG_DIR = osmo.getAbsolutePath() + "/"; //$NON-NLS-1$
        GPAC_CACHE_DIR = new File(osmo, "cache/").getAbsolutePath(); //$NON-NLS-1$
    };

    private final static String LOG_RENDERER = Osmo4Renderer.class.getSimpleName();

    /**
     * Default directory for GPAC modules directory, ends with /
     */
    public final static String GPAC_MODULES_DIR = "/data/data/com.artemis.Osmo4/";//$NON-NLS-1$

    /**
     * Directory of fonts
     */
    public final static String GPAC_FONT_DIR = "/system/fonts/"; //$NON-NLS-1$

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        initGPACDir();
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
        } catch (GpacInstanceException e) {
            Log.e(LOG_RENDERER, "Failed to create new GPAC instance !"); //$NON-NLS-1$
            instance = null;
            callback.onGPACError(e);
            return;
        }
        if (callback != null)
            callback.onGPACReady();
    }

    private GPACInstance instance = null;

    private final String urlToLoad;

    /**
     * Get the current GPAC Instance
     * 
     * @return the instance
     */
    public synchronized GPACInstance getInstance() {
        return instance;
    }

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

    @Override
    public void onDrawFrame(GL10 gl) {
        if (instance != null) {
            instance.render();
        }
    }

    // ------------------------------------
    private void initGPACDir() {
        File osmo = new File(GPAC_CFG_DIR);
        if (!osmo.exists()) {
            if (!osmo.mkdirs()) {
                Log.e(LOG_RENDERER, "Failed to create osmo directory " + GPAC_CFG_DIR); //$NON-NLS-1$
            }
        }
        File cache = new File(GPAC_CACHE_DIR);
        if (!cache.exists()) {
            if (!cache.mkdirs()) {
                Log.e(LOG_RENDERER, "Failed to create cache directory " + GPAC_CFG_DIR); //$NON-NLS-1$

            }
        }
    }
    // ------------------------------------
}
