package io.gpac.gpac;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.Toast;

/**
 * The view represented the blitted contents by libgpac
 * 
 * @version $Revision$
 * 
 */
public class GPACGLSurfaceView extends GLSurfaceView {

    private final static String LOG_GL_SURFACE = "GPACGLSurfaceView";

    private GPAC main_act;

    public GPACGLSurfaceView(GPAC gpac_act) {
        super(gpac_act);
        setEGLContextClientVersion(2);
        setPreserveEGLContextOnPause(true);
        //setDebugFlags(DEBUG_CHECK_GL_ERROR | DEBUG_LOG_GL_CALLS);
        setFocusable(true);
        setFocusableInTouchMode(true);
        main_act = gpac_act;
    }

    /* Set the renderer */
    public void setRenderer(GPACRenderer renderer) {
        super.setRenderer(renderer);
        setRenderMode(RENDERMODE_CONTINUOUSLY);
    }

    @Override
    public boolean onTouchEvent(final MotionEvent event) {
		if (GPAC.noEventForward() )
			return false;

		queueEvent(new Runnable() {
		    @Override
		    public void run() {
		        main_act.motionEvent(event);
		    }
		});
		return true;
	}

    private static final long TIME_INTERVAL = 500;
    private static long mBackPressed;
    /* Should we handle this key in GPAC ?
     @param keyCode
     @param event
     @return true if event is to be handled by GPAC
	*/
    private static boolean handleInGPAC(int keyCode, KeyEvent event) {
        if (event.isSystem() && keyCode!=KeyEvent.KEYCODE_BACK)
            return false;
        switch (keyCode) {
		case KeyEvent.KEYCODE_MEDIA_STOP:
		case KeyEvent.KEYCODE_MENU:
			return false;
		case KeyEvent.KEYCODE_BACK:
			if (mBackPressed + TIME_INTERVAL > System.currentTimeMillis()){
				return false;
			}
			return true;
		default:
			return true;
        }
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
	if (keyCode == KeyEvent.KEYCODE_BACK) Toast.makeText(main_act, "Press back button twice to exit", Toast.LENGTH_SHORT).show();
        if (handleInGPAC(keyCode, event)) {
            Log.d(LOG_GL_SURFACE, "onKeyDown = " + keyCode); //$NON-NLS-1$
            queueEvent(new Runnable() {

                @Override
                public void run() {
                    main_act.eventKey(keyCode, event, true, event.getUnicodeChar());
                }
            });
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        if (handleInGPAC(keyCode, event)) {
            Log.d(LOG_GL_SURFACE, "onKeyUp =" + keyCode); //$NON-NLS-1$
	    if (keyCode == KeyEvent.KEYCODE_BACK) mBackPressed = System.currentTimeMillis();
            queueEvent(new Runnable() {

                @Override
                public void run() {
                    main_act.eventKey(keyCode, event, false, event.getUnicodeChar());
                }
            });
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        if ( main_act.isLoaded() )
            super.onResume();
    }

    @Override
    public void onPause() {
        if ( main_act.isLoaded() )
            super.onPause();
    }

    public void connect(final String url) {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                main_act.connect(url);
            }
        });
    }
}
