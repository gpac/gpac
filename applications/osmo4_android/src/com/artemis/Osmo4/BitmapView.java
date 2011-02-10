/*
 *  Osmo on Android
 *  Aug/2010
 *  NGO Van Luyen
 *
 *
 */
package com.artemis.Osmo4;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.util.Log;
import android.view.View;

public class BitmapView extends View {

    private Bitmap m_Bitmap=null;
    private int m_width = 100, m_height = 100;
//----------------------------------------------------
    public BitmapView(Context context) {
        super(context);
    }

    @Override protected void onDraw(Canvas canvas) {
        //canvas.drawColor(0xFFCCCCCC);
    	m_width = canvas.getWidth();
    	m_height = canvas.getHeight();
    	if (m_Bitmap == null){
    		m_Bitmap = Bitmap.createBitmap(m_width, m_height, Bitmap.Config.ARGB_8888);

    		Log.e("BitmapView", "Going to gpacinit");
    		GpacObject.gpacinit(m_Bitmap, 100, 100, "", "", "", "");
    		//GpacObject.gpacconnect("/data/osmo/bifs-2D-interactivity-stringsensor.mp4");
    		//GpacObject.gpacconnect("/data/osmo/Tidy City.mp4");
    		GpacObject.gpacresize(m_width, m_height);

    	}

    	GpacObject.gpacrender(m_Bitmap);
		canvas.drawBitmap(m_Bitmap, 0, 0, null);
        // force a redraw, with a different time-based pattern.
        invalidate();

    }


    public void gpacinit(){
    	Log.e("BitmapView", "Going to gpacinit");
    	if (m_Bitmap != null){
    		Log.e("BitmapView", "m_Bitmap != null");
    		GpacObject.gpacinit(m_Bitmap, 100, 100, "", "", "", "");
    		//GpacObject.gpacconnect("/data/osmo/bifs-2D-interactivity-stringsensor.mp4");
    		GpacObject.gpacresize(m_width, m_height);
    	}
    }

    public void gpacfree(){
    	Log.e("BitmapView", "gpacfree");
    	GpacObject.gpacdisconnect();
    	GpacObject.gpacfree();
    }

}
