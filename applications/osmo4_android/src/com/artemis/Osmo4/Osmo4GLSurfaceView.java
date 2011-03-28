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

    // ------------------------------------
    @Override
    public boolean onTouchEvent(final MotionEvent event) {
        final float x = event.getX();
        final float y = event.getY();
        return postAsync(new Runnable() {

            @Override
            public void run() {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        GpacObject.gpaceventmousedown(x, y);
                        break;
                    case MotionEvent.ACTION_UP:
                        GpacObject.gpaceventmouseup(x, y);
                        break;
                    case MotionEvent.ACTION_MOVE:
                        GpacObject.gpaceventmousemove(x, y);
                }
            }
        });
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
        return postAsync(new Runnable() {

            @Override
            public void run() {
                GpacObject.gpaceventkeypress(keyCode, event.getScanCode(), 1, event.getFlags());
            }
        });
    }

    // ------------------------------------
    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        return postAsync(new Runnable() {

            @Override
            public void run() {
                GpacObject.gpaceventkeypress(keyCode, event.getScanCode(), 0, event.getFlags());
            }
        });
    }

    private boolean postAsync(Runnable command) {
        Osmo4Renderer r;
        synchronized (this) {
            r = this.renderer;
        }
        if (r == null)
            return false;
        r.postCommand(command);
        return true;
    }
}
