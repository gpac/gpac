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

    // ------------------------------------
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        initGPACDir();
    }

    private boolean inited = false;

    /**
     * Constructor
     * 
     * @param callback
     */
    public Osmo4Renderer(GpacCallback callback) {
        this.callback = callback;
    }

    private final GpacCallback callback;

    // ------------------------------------
    public void onSurfaceChanged(GL10 gl, int w, int h) {
        // gl.glViewport(0, 0, w, h);
        if (!inited) {
            GpacObject.gpacinit(null, callback, w, h, GPAC_CFG_DIR, GPAC_MODULES_DIR, GPAC_CACHE_DIR, GPAC_FONT_DIR);
            GpacObject.gpacresize(w, h);
            inited = true;
            callback.onGPACReady();
        } else
            GpacObject.gpacresize(w, h);
    }

    // ------------------------------------
    public void onDrawFrame(GL10 gl) {
        GpacObject.gpacrender(null);
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
