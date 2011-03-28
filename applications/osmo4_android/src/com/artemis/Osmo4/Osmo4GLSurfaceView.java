package com.artemis.Osmo4;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;

/**
 * The view represented the blitted contents by libgpac
 * 
 * @version $Revision$
 * 
 */
public class Osmo4GLSurfaceView extends GLSurfaceView {

    private final static String LOG_GL_SURFACE = Osmo4GLSurfaceView.class.getSimpleName();

    /**
     * Constructor
     * 
     * @param context
     */
    public Osmo4GLSurfaceView(Context context) {
        super(context);
    }

    // ------------------------------------
    @Override
    public boolean onTouchEvent(final MotionEvent event) {
        float x = event.getX();
        float y = event.getY();

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

        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        int code = keyCode;
        int action = event.getAction();

        Log.e(LOG_GL_SURFACE, " OnKeyDown: " + code + ", action=" + action); //$NON-NLS-1$ //$NON-NLS-2$

        // 0 == up, 1 == down
        GpacObject.gpaceventkeypress(code, event.getScanCode(), 1, event.getFlags());

        return false;

        // return super.onKeyDown(keyCode, event);
    }

    // ------------------------------------
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        int code = keyCode;
        Log.e(LOG_GL_SURFACE, " OnKeyUp: " + code); //$NON-NLS-1$

        // 0 == up, 1 == down
        GpacObject.gpaceventkeypress(code, event.getScanCode(), 0, event.getFlags());

        return false;

        // return super.onKeyDown(keyCode, event);
    }
}
