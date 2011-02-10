package com.artemis.Osmo4;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.util.Log;

public class Osmo4GLSurfaceView extends GLSurfaceView {

	public Osmo4Renderer mRenderer;
	//------------------------------------
	public Osmo4GLSurfaceView(Context context) {
        super(context);

        mRenderer = new Osmo4Renderer();
        setRenderer(mRenderer);
    }
	//------------------------------------
    public boolean onTouchEvent(final MotionEvent event) {
    	float x = event.getX();
    	float y = event.getY();

    	switch (event.getAction()){
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
    //------------------------------------
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	int code = keyCode;
    	int action = event.getAction();

    	Log.e("OnKeyListener", " OnKeyListener: "+code);

    	// 0 == up, 1 == down
    	GpacObject.gpaceventkeypress(code, event.getScanCode(), 1, event.getFlags());

    	return false;

    	//return super.onKeyDown(keyCode, event);
    }
  //------------------------------------
    public boolean onKeyUp(int keyCode, KeyEvent event) {
    	int code = keyCode;
    	int action = event.getAction();

    	Log.e("OnKeyListener", " OnKeyListener: "+code);

    	// 0 == up, 1 == down
    	GpacObject.gpaceventkeypress(code, event.getScanCode(), 0, event.getFlags());

    	return false;

    	//return super.onKeyDown(keyCode, event);
    }
}
