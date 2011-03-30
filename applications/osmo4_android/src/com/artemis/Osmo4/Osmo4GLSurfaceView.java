package com.artemis.Osmo4;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.KeyEvent;
import android.view.MotionEvent;

/**
 * The view represented the blitted contents by libgpac
 * 
 * @version $Revision$
 * 
 */
public class Osmo4GLSurfaceView extends GLSurfaceView {

    // private final static String LOG_GL_SURFACE = Osmo4GLSurfaceView.class.getSimpleName();

    /**
     * Constructor
     * 
     * @param context
     */
    public Osmo4GLSurfaceView(Context context) {
        super(context);
    }

    private Osmo4Renderer renderer;

    /**
     * Set the renderer
     * 
     * @param renderer
     */
    public void setRenderer(Osmo4Renderer renderer) {
        synchronized (this) {
            this.renderer = renderer;
        }
        super.setRenderer(renderer);
    }

    private Osmo4Renderer getRenderer() {
        return renderer;
    }

    private GPACInstance getInstance() {
        Osmo4Renderer r = getRenderer();
        if (r == null)
            return null;
        return r.getInstance();
    }

    // ------------------------------------
    @Override
    public boolean onTouchEvent(final MotionEvent event) {
        GPACInstance instance = getInstance();
        if (instance == null)
            return false;
        instance.motionEvent(event);
        return true;
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
        GPACInstance instance = getInstance();
        if (instance == null)
            return false;
        instance.eventKey(keyCode, event, true);
        return true;
    }

    // ------------------------------------
    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        GPACInstance instance = getInstance();
        if (instance == null)
            return false;
        instance.eventKey(keyCode, event, false);
        return true;
    }
}
