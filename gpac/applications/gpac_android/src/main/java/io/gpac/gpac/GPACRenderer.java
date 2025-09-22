package io.gpac.gpac;

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
public class GPACRenderer implements GLSurfaceView.Renderer {

    private final static String LOG_RENDERER = GPACRenderer.class.getSimpleName();

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
		try {
			main_act.initGPAC();
		} catch (Exception e) {
			return;
		}
    }

    /**
     * Constructor
     * 
     * @param main_act main GPAC activity
     * @param urlToLoad The URL to load at startup, can be null
     */
    public GPACRenderer(GPAC main_act) {
        this.main_act = main_act;
    }

    private final GPAC main_act;

    @Override
    public void onSurfaceChanged(GL10 gl, int w, int h) {
        Log.i(LOG_RENDERER, "Surface changed from thread " + Thread.currentThread()); //$NON-NLS-1$
        main_act.resize(w, h);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        main_act.render();
    }
}
