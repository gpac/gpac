package com.gpac.Osmo4;

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
public class Osmo4GLSurfaceView extends GLSurfaceView implements GPACInstanceInterface {

    private final static String LOG_GL_SURFACE = Osmo4GLSurfaceView.class.getSimpleName();

    /**
     * Constructor
     * 
     * @param context
     */
    public Osmo4GLSurfaceView(Context context) {
        super(context);
        setDebugFlags(DEBUG_CHECK_GL_ERROR | DEBUG_LOG_GL_CALLS);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    private Osmo4Renderer gpacRenderer;

    /**
     * Set the renderer
     * 
     * @param renderer
     */
    public void setRenderer(Osmo4Renderer renderer) {
        synchronized (this) {
            this.gpacRenderer = renderer;
        }
        super.setRenderer(renderer);
        setRenderMode(RENDERMODE_CONTINUOUSLY);
    }

    private synchronized Osmo4Renderer getGpacRenderer() {
        return gpacRenderer;
    }

    private GPACInstance getInstance() {
        Osmo4Renderer r = getGpacRenderer();
        if (r == null)
            return null;
        return r.getInstance();
    }

    // ------------------------------------
    @Override
    public boolean onTouchEvent(final MotionEvent event) {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                GPACInstance instance = getInstance();
                if (instance != null)
                    instance.motionEvent(event);
            }
        });
        return true;
    }

    /**
     * Should we handle this key in GPAC ?
     * 
     * @param keyCode
     * @param event
     * @return
     */
    private static boolean handleInGPAC(int keyCode, KeyEvent event) {
        if (event.isSystem())
            return false;
        switch (keyCode) {
            case KeyEvent.KEYCODE_MEDIA_STOP:
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_BACK:
                return false;
            default:
                return true;
        }
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
        if (handleInGPAC(keyCode, event)) {
            Log.d(LOG_GL_SURFACE, "onKeyDown = " + keyCode); //$NON-NLS-1$
            queueEvent(new Runnable() {

                @Override
                public void run() {
                    GPACInstance instance = getInstance();
                    if (instance != null)
                        instance.eventKey(keyCode, event, true, event.getUnicodeChar());
                }
            });
            return true;
        }
        return false;
    }

    // ------------------------------------
    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        if (handleInGPAC(keyCode, event)) {
            Log.d(LOG_GL_SURFACE, "onKeyUp =" + keyCode); //$NON-NLS-1$
            queueEvent(new Runnable() {

                @Override
                public void run() {
                    GPACInstance instance = getInstance();
                    if (instance != null)
                        instance.eventKey(keyCode, event, false, event.getUnicodeChar());
                }
            });
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        if (getInstance() != null)
            super.onResume();
    }

    @Override
    public void onPause() {
        if (getInstance() != null)
            super.onPause();
    }

    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#connect(java.lang.String)
     */
    @Override
    public void connect(final String url) {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                GPACInstance instance = getInstance();
                if (instance != null)
                    instance.connect(url);
            }
        });
    }

    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#disconnect()
     */
    @Override
    public void disconnect() {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                GPACInstance instance = getInstance();
                if (instance != null)
                    instance.disconnect();
            }
        });
    }

    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#destroy()
     */
    @Override
    public void destroy() {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                GPACInstance instance = getInstance();
                if (instance != null)
                    instance.destroy();
            }
        });
    }

    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#setGpacPreference(String, String, String)
     */
    @Override
    public void setGpacPreference(final String category, final String name, final String value) {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                GPACInstance instance = getInstance();
                if (instance != null)
                    instance.setGpacPreference(category, name, value);
            }
        });
    }
    
    /**
     * @see com.gpac.Osmo4.GPACInstanceInterface#setGpacLogs(String tools_at_levels)
     */
    @Override
    public void setGpacLogs(final String tools_at_levels) {
        queueEvent(new Runnable() {

            @Override
            public void run() {
                GPACInstance instance = getInstance();
                if (instance != null)
                    instance.setGpacLogs(tools_at_levels);
            }
        });
    }
}
