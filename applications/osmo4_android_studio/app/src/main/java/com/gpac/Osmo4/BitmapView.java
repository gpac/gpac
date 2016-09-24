///**
// *  Osmo on Android
// *  Aug/2010
// *  NGO Van Luyen
// *
// *
// */
//package com.gpac.Osmo4;
//
//import android.content.Context;
//import android.graphics.Bitmap;
//import android.graphics.Canvas;
//import android.util.Log;
//import android.view.View;
//
///**
// * 
// * @version $Revision$
// * 
// */
//@Deprecated
//public class BitmapView extends View {
//
//    private Bitmap m_Bitmap = null;
//
//    private int m_width = 100, m_height = 100;
//
//    private final static String LOG_BITMAP = BitmapView.class.getSimpleName();
//
//    /**
//     * Constructor
//     * 
//     * @param context The current view's context
//     */
//    public BitmapView(Context context) {
//        super(context);
//    }
//
//    @Override
//    protected void onDraw(Canvas canvas) {
//        // canvas.drawColor(0xFFCCCCCC);
//        m_width = canvas.getWidth();
//        m_height = canvas.getHeight();
//        if (m_Bitmap == null) {
//            if (m_width < 1)
//                m_width = 100;
//            if (m_height < 1)
//                m_height = 100;
//            m_Bitmap = Bitmap.createBitmap(m_width, m_height, Bitmap.Config.ARGB_8888);
//            gpacinit();
//        }
//
//        GpacObject.gpacrender(m_Bitmap);
//        canvas.drawBitmap(m_Bitmap, 0, 0, null);
//        // force a redraw, with a different time-based pattern.
//        invalidate();
//
//    }
//
//    /**
//     * Called to init all GPAC resources
//     */
//    private void gpacinit() {
//        Log.i(LOG_BITMAP, "Going to gpacinit"); //$NON-NLS-1$
//        if (m_Bitmap != null) {
//            Log.e(LOG_BITMAP, "m_Bitmap != null"); //$NON-NLS-1$
//            if (m_width < 1)
//                m_width = 100;
//            if (m_height < 1)
//                m_height = 100;
//            GpacObject.gpacinit(m_Bitmap,
//                                null,
//                                m_width,
//                                m_height,
//                                Osmo4Renderer.GPAC_CFG_DIR,
//                                Osmo4Renderer.GPAC_MODULES_DIR,
//                                Osmo4Renderer.GPAC_CACHE_DIR,
//                                Osmo4Renderer.GPAC_FONT_DIR,
//                                null);
//            // GpacObject.gpacconnect("/data/osmo/bifs-2D-interactivity-stringsensor.mp4");
//            GpacObject.gpacresize(m_width, m_height);
//        }
//    }
//
//    /**
//     * Called to free all GPAC resources
//     */
//    public void gpacfree() {
//        Log.e(LOG_BITMAP, "gpacfree()"); //$NON-NLS-1$
//        GpacObject.gpacdisconnect();
//        GpacObject.gpacfree();
//    }
// }
