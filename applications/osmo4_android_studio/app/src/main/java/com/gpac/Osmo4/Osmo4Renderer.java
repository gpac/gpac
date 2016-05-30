package com.gpac.Osmo4;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.util.Log;

/**
 * The renderer
 * 
 * @version $Revision$
 * 
 */
public class Osmo4Renderer implements GLSurfaceView.Renderer {

    private final GpacConfig gpacConfig;

    private final static String LOG_RENDERER = Osmo4Renderer.class.getSimpleName();

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        if (instance == null) {
            try {
                Log.i(LOG_RENDERER, "Creating instance from thread " + Thread.currentThread()); //$NON-NLS-1$
                instance = new GPACInstance(callback, 320, 430, gpacConfig, urlToLoad);
            } catch (com.gpac.Osmo4.GPACInstanceInterface.GpacInstanceException e) {
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
     * @param gpacConfig
     * @param urlToLoad The URL to load at startup, can be null
     */
    public Osmo4Renderer(GpacCallback callback, GpacConfig gpacConfig, String urlToLoad) {
        this.callback = callback;
        this.urlToLoad = urlToLoad;
        this.gpacConfig = gpacConfig;
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
