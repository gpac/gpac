package com.gpac.Osmo4;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.util.List;
import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.hardware.Camera;
import android.hardware.Camera.Size;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;

/**
 * Manage camera preview and frame grabbing.
 * 
 * Mostly copied from
 * 
 * {@link "http://developer.android.com/resources/samples/ApiDemos/src/com/example/android/apis/graphics/CameraPreview.html"}
 */
public class Preview {

    private static final String TAG = "Preview"; //$NON-NLS-1$

    private Camera mCamera = null;

    private Size mPreviewSize = null;

    private int nBuffers = 0;

    private int mBufferSize = 0;

    private int mPreviewFormat = 0;

    private PreviewHandler mPreviewHandler = null;

    private boolean takePicture = false;

    private boolean mPreviewing = false;

    private boolean isPortrait = false;

    private int width = 0;

    private int height = 0;

    // Native functions
    private native void processFrameBuf(byte[] data);

    Preview() {
        mCamera = Camera.open();
        if (mCamera == null) {
            Log.e(TAG, "Camera failed to open"); //$NON-NLS-1$
            return;
        }
        setParameters(mCamera.getParameters());
        mCamera.release();
        mCamera = null;
    }

    @Override
    protected void finalize() {
        stopCamera();
    }

    /**
     * Set up the camera parameters and begin the preview.
     * 
     * @param isPortrait True if camera orientation is portrait. False if it is landscape.
     * @return True if successful.
     */
    public boolean initializeCamera(boolean isPortrait) {
        this.isPortrait = isPortrait;

        // Open camera
        if (mCamera == null) {
            Log.i(TAG, "Open camera..."); //$NON-NLS-1$
            mCamera = Camera.open();
            if (mCamera == null) {
                Log.w(TAG, "Camera failed to open"); //$NON-NLS-1$
                return false;
            }
        }

        stopPreview();

        // Get parameters
        mCamera.setParameters(setParameters(mCamera.getParameters()));
        width = mCamera.getParameters().getPreviewSize().width;
        height = mCamera.getParameters().getPreviewSize().height;
        if (mPreviewSize == null || mPreviewFormat == 0) {
            Log.e(TAG, "Preview size or format error"); //$NON-NLS-1$
            return false;
        }

        // Define buffer size
        PixelFormat pixelinfo = new PixelFormat();
        PixelFormat.getPixelFormatInfo(mPreviewFormat, pixelinfo);
        mBufferSize = mPreviewSize.width * mPreviewSize.height * pixelinfo.bitsPerPixel / 8;
        Log.d(TAG, "Pixelsize = " + pixelinfo.bitsPerPixel + " Buffersize = " + mBufferSize); //$NON-NLS-1$//$NON-NLS-2$
        nBuffers = 1;

        // Initialize frame grabbing
        // mPreviewHandler = new PreviewHandler("CameraPreviewHandler");

        mCamera.startPreview();
        mPreviewing = true;

        Log.d(TAG, "Camera preview started"); //$NON-NLS-1$
        return true;
    }

    /**
     * Get the size of Preview
     * 
     * @return null if camera not setup
     */
    public Size getPreviewSize() {
        return mPreviewSize;
    }

    /**
     * Get the preview format
     * 
     * @return 0 if not initialized
     */
    public int getPreviewFormat() {
        return mPreviewFormat;
    }

    /**
     * Get the Image height
     * 
     * @return -1 if not initialized, the height otherwise
     */
    public int getImageHeight() {
        Size sz = getPreviewSize();
        if (sz == null)
            return -1;
        return sz.height;
    }

    /**
     * Get the image Width if set
     * 
     * @return -1 if not initialized, the width otherwise
     */
    public int getImageWidth() {
        Size sz = getPreviewSize();
        if (sz == null)
            return -1;
        return sz.width;
    }

    /**
     * Stop preview and release the camera. Because the CameraDevice object is not a shared resource, it is very
     * important to release it when the activity is paused.
     */
    public void stopCamera() {
        Log.v(TAG, "stopCamera()"); //$NON-NLS-1$

        stopPreview();
        if (mCamera != null) {
            mCamera.release();
            mCamera = null;
            Log.i(TAG, "Camera released"); //$NON-NLS-1$
        }
    }

    /**
     * Stop camera preview and frame grabbing.
     */
    synchronized protected void stopPreview() {
        Log.v(TAG, "stopPreview()"); //$NON-NLS-1$
        if (mPreviewHandler != null) {
            Log.v(TAG, "mPreviewHandler != null"); //$NON-NLS-1$
            mPreviewHandler.stopPreview();
            mPreviewHandler = null;
        }
        if (mPreviewing) {
            mCamera.stopPreview();
            Log.d(TAG, "Camera preview stopped"); //$NON-NLS-1$
            mPreviewing = false;
        }
    }

    /**
     * Stop the processing thread
     */
    public void pausePreview() {
        if (mPreviewHandler != null) {
            mPreviewHandler.stopPreview();
            mPreviewHandler = null;
        }
    }

    /**
     * Restart the processing thread
     */
    public void resumePreview() {
        if (mPreviewHandler == null) {
            mPreviewHandler = new PreviewHandler("CameraPreviewHandler"); //$NON-NLS-1$
        }
    }

    /**
     * Set preferred mPreviewSize and mPrevieformat and return adapted camera parameters. If mPreviewSize == null or
     * mPreviewFormat == 0 then some error has occurred.
     * 
     * @param parameters the parameters to set
     * 
     * @return the parameters set as argument
     * @throws IllegalArgumentException if argument is null
     */
    protected Camera.Parameters setParameters(Camera.Parameters parameters) throws IllegalArgumentException {
        if (parameters == null)
            throw new IllegalArgumentException("parameters must not be null"); //$NON-NLS-1$
        // Get preview size
        mPreviewSize = parameters.getPreviewSize();
        Log.d(TAG, "PreviewSize: " + mPreviewSize.width + 'x' + mPreviewSize.height); //$NON-NLS-1$

        // Get preview format
        mPreviewFormat = PixelFormat.YCbCr_420_SP;
        // getOptimalPreviewFormat(parameters
        // .getSupportedPreviewFormats());
        if (mPreviewFormat == 0) {
            Log.e(TAG, "Cannot set mPreviewFormat"); //$NON-NLS-1$
        } else {
            Log.d(TAG, "Previewformat: " + mPreviewFormat); //$NON-NLS-1$
            parameters.setPreviewFormat(mPreviewFormat);
        }

        parameters.setPreviewFrameRate(15);

        return parameters;
    }

    /**
     * Return preferred camera preview format. At the moment, PixelFormat.YCbCr_420_SP is required.
     * 
     * @param formats Supported formats: Camera.Parameters.getSupportedPreviewFormats()
     * @return Image format 17 (YUV420SP) if supported, 0 if not.
     */
    protected int getOptimalPreviewFormat(List<Integer> formats) {
        int optFormat = 0;
        for (Integer format : formats) {
            int intFormat = format.intValue();
            Log.d(TAG, "supported image format: " + intFormat); //$NON-NLS-1$
            if (intFormat == PixelFormat.YCbCr_420_SP) {
                optFormat = intFormat;
            }
        }
        return optFormat;
    }

    /**
     * Process a frame grabbed from the camera.
     * 
     * @param data Image from camera in YUV420SP format.
     */
    protected void processFrame(byte[] data) {
        if (takePicture) {
            // Test: decode and save image
            int[] rgbBuf = decodeYUV420SP(data, width, height);
            saveImageToFile(rgbBuf, width, height);
            takePicture = false;
        }
        // End of test

        // Send data to C library
        processFrameBuf(data);
    }

    /**
     * Save an RGB image to the file /sdcard/image.jpg
     * 
     * @param rgbBuffer RGB image buffer to save
     * @param width Width of the image
     * @param height Height of the image
     */
    public void saveImageToFile(int[] rgbBuffer, int width, int height) {
        Bitmap bm = Bitmap.createBitmap(rgbBuffer, width, height, Bitmap.Config.RGB_565);

        // Save image
        String path = Environment.getExternalStorageDirectory().toString();
        OutputStream fOut = null;
        File file = new File(path, "image.jpg"); //$NON-NLS-1$
        try {
            try {
                fOut = new FileOutputStream(file);
                bm.compress(Bitmap.CompressFormat.JPEG, 100, fOut);
                fOut.flush();
            } catch (IOException e1) {
                Log.e(TAG, "io error file output stream while writing", e1); //$NON-NLS-1$
                e1.printStackTrace();
            } finally {
                if (fOut != null)
                    fOut.close();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     * Convert an image from YUV to RGB format.
     * 
     * @param yuv420sp YUV format image, as a byte array
     * @param width Width of the image
     * @param height Height of the image
     * @return RGB image as an int array
     */
    public int[] decodeYUV420SP(byte[] yuv420sp, int width, int height) {
        final int frameSize = width * height;

        int rgb[] = new int[width * height];
        for (int j = 0, yp = 0; j < height; j++) {
            int uvp = frameSize + (j >> 1) * width, u = 0, v = 0;
            for (int i = 0; i < width; i++, yp++) {
                int y = (0xff & (yuv420sp[yp])) - 16;
                if (y < 0)
                    y = 0;
                if ((i & 1) == 0) {
                    v = (0xff & yuv420sp[uvp++]) - 128;
                    u = (0xff & yuv420sp[uvp++]) - 128;
                }

                int y1192 = 1192 * y;
                int r = (y1192 + 1634 * v);
                int g = (y1192 - 833 * v - 400 * u);
                int b = (y1192 + 2066 * u);

                if (r < 0)
                    r = 0;
                else if (r > 262143)
                    r = 262143;
                if (g < 0)
                    g = 0;
                else if (g > 262143)
                    g = 262143;
                if (b < 0)
                    b = 0;
                else if (b > 262143)
                    b = 262143;
                rgb[yp] = 0xff000000 | ((r << 6) & 0xff0000) | ((g >> 2) & 0xff00) | ((b >> 10) & 0xff);
            }
        }
        return rgb;
    }

    /**
     * Interface between PreviewHandler and mCamera
     * 
     * @param cb
     */
    private void setPreviewCallbackWithBuffer(Camera.PreviewCallback cb) {
        // call mCamera.setPreviewCallbackWithBuffer(cb)
        // if cb != null then also add a callback buffers
        try {
            if (bufferWorks) {
                if (cb != null) {
                    for (int i = 0; i < nBuffers; i++) {
                        // TODO level 8: mCamera.addCallbackBuffer(new
                        // byte[mBufferSize]);
                        camAddCbBuffer.invoke(mCamera, new byte[mBufferSize]);
                    }
                }
                // TODO level 8: mCamera.setPreviewCallbackWithBuffer(cb);
                camSetPreview.invoke(mCamera, cb);
            } else if (cb != null) {
                mCamera.setOneShotPreviewCallback(cb);
            }
        } catch (Exception e) {
            Log.e(TAG, "setPreviewCallbackWithBuffer failed:" + e, e); //$NON-NLS-1$
        }
    }

    private int fps_max_count = 100;

    private int fps_count = 1;

    private long fps_last = -42;

    private void addCallbackBuffer(byte[] buffer) {
        if (--fps_count == 0) {
            long now = System.currentTimeMillis();
            if (fps_last != -42) {
                float fps = fps_max_count * 1000.f / (now - fps_last);
                Log.i(TAG, "fps: " + fps); //$NON-NLS-1$
                fps_max_count = (int) (5.f * fps);
            }
            fps_count = fps_max_count;
            fps_last = now;
        }
        try {
            if (bufferWorks) {
                camAddCbBuffer.invoke(mCamera, buffer);
            } else {
                mCamera.setOneShotPreviewCallback(mPreviewHandler);
            }
        } catch (Exception e) {
            Log.e(TAG, "addCallbackBuffer failed: " + e); //$NON-NLS-1$
        }
    }

    /*
     * TODO: for level 8 all the stuff below becomes obsolete Use reflection for accessing hidden methods
     */
    private static final Method getCamAddCallbackBuffer() {
        try {
            return Class.forName("android.hardware.Camera").getMethod("addCallbackBuffer", byte[].class); //$NON-NLS-1$//$NON-NLS-2$
        } catch (Exception e) {
            Log.e(TAG, "No addCallbackBuffer: " + e); //$NON-NLS-1$
        }
        return null;
    }

    private static final Method getCamSetPreviewCallbackWithBuffer() {
        try {
            return Class.forName("android.hardware.Camera").getMethod("setPreviewCallbackWithBuffer", //$NON-NLS-1$ //$NON-NLS-2$
                                                                      Camera.PreviewCallback.class);
        } catch (Exception e) {
            Log.e(TAG, "No setPreviewCallbackWithBuffer: " + e); //$NON-NLS-1$
        }
        return null;
    }

    /**
     * Handles camera frame buffer callbacks in a new thread.
     */
    protected class PreviewHandler extends HandlerThread implements Camera.PreviewCallback {

        private Handler mHandler;

        private boolean stopped = false;

        PreviewHandler(String name) {
            super(name);
            mHandler = null;
            start();
        }

        @Override
        public void onPreviewFrame(final byte[] data, final Camera camera) {
            if (mHandler != null && camera == mCamera) {
                mHandler.post(new Runnable() {

                    @Override
                    public void run() {
                        if (!stopped) {
                            processFrame(data);
                            if (mHandler != null) {
                                addCallbackBuffer(data);
                            }
                        }
                    }
                });
            }
        }

        @Override
        synchronized protected void onLooperPrepared() {
            Log.v(TAG, "onLooperPrepared()"); //$NON-NLS-1$
            if (stopped == false) {
                mHandler = new Handler();
                Log.d(TAG, "frame processing start"); //$NON-NLS-1$
                setPreviewCallbackWithBuffer(this);
                // mCamera.setPreviewCallback(this);
            }
        }

        synchronized private void stopPreview() {
            stopped = true;
            if (mHandler != null) {
                Log.d(TAG, "frame processing stop"); //$NON-NLS-1$
                setPreviewCallbackWithBuffer(null);
                // mCamera.setPreviewCallback(null);
                mHandler = null;
            }
        }
    }

    private static final Method camAddCbBuffer = getCamAddCallbackBuffer();

    private static final Method camSetPreview = getCamSetPreviewCallbackWithBuffer();

    private static final boolean bufferWorks = camAddCbBuffer != null && camSetPreview != null;
}
