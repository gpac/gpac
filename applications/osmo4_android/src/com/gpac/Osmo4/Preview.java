package com.gpac.Osmo4;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
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
 * @link
 *       http://developer.android.com/resources/samples/ApiDemos/src/com/example/
 *       android/apis/graphics/CameraPreview.html
 */
public class Preview {
	protected static final String TAG = "Preview";
	protected Camera mCamera = null;
	protected Size mPreviewSize = null;
	protected int nBuffers = 0;
	protected int mBufferSize = 0;
	protected int mPreviewFormat = 0;
	protected PreviewHandler mPreviewHandler = null;
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
			Log.e(TAG, "Camera failed to open");
			return;
		}
		setParameters(mCamera.getParameters());
		mCamera.release();
		mCamera = null;
	}

	@Override
	protected void finalize()
	{
		stopCamera();
	}

	/**
	 * Set up the camera parameters and begin the preview.
	 *
	 * @param isPortrait
	 *            True if camera orientation is portrait. False if it is
	 *            landscape.
	 * @return True if successful.
	 */
	public boolean initializeCamera(boolean isPortrait) {
		this.isPortrait = isPortrait;

		// Open camera
		if (mCamera == null) {
			Log.i(TAG, "Open camera ...");
			mCamera = Camera.open();
			if (mCamera == null) {
				Log.w(TAG, "Camera failed to open");
				return false;
			}
		}

		stopPreview();

		// Get parameters
		mCamera.setParameters(setParameters(mCamera.getParameters()));
		width = mCamera.getParameters().getPreviewSize().width;
		height = mCamera.getParameters().getPreviewSize().height;
		if (mPreviewSize == null || mPreviewFormat == 0) {
			Log.e(TAG, "Preview size or format error");
			return false;
		}

		// Define buffer size
		PixelFormat pixelinfo = new PixelFormat();
		PixelFormat.getPixelFormatInfo(mPreviewFormat, pixelinfo);
		mBufferSize = mPreviewSize.width * mPreviewSize.height
				* pixelinfo.bitsPerPixel / 8;
		Log.d(TAG, "Pixelsize = " + pixelinfo.bitsPerPixel + " Buffersize = "
				+ mBufferSize);
		nBuffers = 1;

		// Initialize frame grabbing
		//mPreviewHandler = new PreviewHandler("CameraPreviewHandler");

		mCamera.startPreview();
		mPreviewing = true;

		Log.d(TAG, "Camera preview started");
		return true;
	}

	public Size getPreviewSize() {
		return mPreviewSize;
	}

	public int getPreviewFormat() {
		return mPreviewFormat;
	}

	public int getImageHeight() {
		return mPreviewSize.height;
	}

	public int getImageWidth() {
		return mPreviewSize.width;
	}

	/**
	 * Stop preview and release the camera. Because the CameraDevice object is
	 * not a shared resource, it is very important to release it when the
	 * activity is paused.
	 */
	public void stopCamera() {
		Log.v(TAG, "stopCamera()");

		stopPreview();
		if (mCamera != null) {
			mCamera.release();
			mCamera = null;
			Log.i(TAG, "Camera released");
		}
	}

	/**
	 * Stop camera preview and frame grabbing.
	 */
	synchronized protected void stopPreview() {
		Log.v(TAG, "stopPreview()");
		if (mPreviewHandler != null) {
			Log.v(TAG, "mPreviewHandler != null");
			mPreviewHandler.stopPreview();
			mPreviewHandler = null;
		}
		if (mPreviewing) {
			mCamera.stopPreview();
			Log.d(TAG, "Camera preview stopped");
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
			mPreviewHandler = new PreviewHandler("CameraPreviewHandler");
		}
	}

	/**
	 * Set preferred mPreviewSize and mPrevieformat and return adapted camera
	 * parameters. If mPreviewSize == null or mPreviewFormat == 0 then some
	 * error has occurred.
	 */
	protected Camera.Parameters setParameters(Camera.Parameters parameters) {
		// Get preview size
		mPreviewSize = parameters.getPreviewSize();
		Log.d(TAG, "PreviewSize: " + mPreviewSize.width + 'x'
				+ mPreviewSize.height);

		// Get preview format
		mPreviewFormat = PixelFormat.YCbCr_420_SP;
			//getOptimalPreviewFormat(parameters
			//	.getSupportedPreviewFormats());
		if (mPreviewFormat == 0) {
			Log.e(TAG, "Cannot set mPreviewFormat");
		} else {
			Log.d(TAG, "Previewformat: " + mPreviewFormat);
			parameters.setPreviewFormat(mPreviewFormat);
		}

		parameters.setPreviewFrameRate(15);

		return parameters;
	}

	/**
	 * Return preferred camera preview format. At the moment,
	 * PixelFormat.YCbCr_420_SP is required.
	 *
	 * @param formats
	 *            Supported formats:
	 *            Camera.Parameters.getSupportedPreviewFormats()
	 * @return Image format 17 (YUV420SP) if supported, 0 if not.
	 */
	protected int getOptimalPreviewFormat(List<Integer> formats) {
		int optFormat = 0;
		for (Integer format : formats) {
			int intFormat = format.intValue();
			Log.d(TAG, "supported image format: " + intFormat);
			if (intFormat == PixelFormat.YCbCr_420_SP) {
				optFormat = intFormat;
			}
		}
		return optFormat;
	}

	/**
	 * Process a frame grabbed from the camera.
	 *
	 * @param data
	 *            Image from camera in YUV420SP format.
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
	 * @param rgbBuffer
	 *            RGB image buffer to save
	 * @param width
	 *            Width of the image
	 * @param height
	 *            Height of the image
	 */
	public void saveImageToFile(int[] rgbBuffer, int width, int height) {
		Bitmap bm = Bitmap.createBitmap(rgbBuffer, width, height,
				Bitmap.Config.RGB_565);

		// Save image
		String path = Environment.getExternalStorageDirectory().toString();
		OutputStream fOut = null;
		File file = new File(path, "image.jpg");
		try {
			try {
				fOut = new FileOutputStream(file);
				bm.compress(Bitmap.CompressFormat.JPEG, 100, fOut);
				fOut.flush();
			} catch (FileNotFoundException e1) {
				Log.e("TAG", "io error file output stream");
				e1.printStackTrace();
			} finally {
				fOut.close();
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	/**
	 * Convert an image from YUV to RGB format.
	 *
	 * @param yuv420sp
	 *            YUV format image, as a byte array
	 * @param width
	 *            Width of the image
	 * @param height
	 *            Height of the image
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
				rgb[yp] = 0xff000000 | ((r << 6) & 0xff0000)
						| ((g >> 2) & 0xff00) | ((b >> 10) & 0xff);
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
			Log.e(TAG, "setPreviewCallbackWithBuffer failed:" + e);
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
				Log.i(TAG, "fps: " + fps);
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
			Log.e(TAG, "addCallbackBuffer failed: " + e);
		}
	}

	/*
	 * TODO: for level 8 all the stuff below becomes obsolete Use reflection for
	 * accessing hidden methods
	 */
	private static final Method getCamAddCallbackBuffer() {
		try {
			return Class.forName("android.hardware.Camera").getMethod(
					"addCallbackBuffer", byte[].class);
		} catch (Exception e) {
			Log.e(TAG, "No addCallbackBuffer: " + e);
		}
		return null;
	}

	private static final Method getCamSetPreviewCallbackWithBuffer() {
		try {
			return Class.forName("android.hardware.Camera").getMethod(
					"setPreviewCallbackWithBuffer",
					Camera.PreviewCallback.class);
		} catch (Exception e) {
			Log.e(TAG, "No setPreviewCallbackWithBuffer: " + e);
		}
		return null;
	}

	/**
	 * Handles camera frame buffer callbacks in a new thread.
	 */
	protected class PreviewHandler extends HandlerThread implements
			Camera.PreviewCallback {
		private Handler mHandler;
		private boolean stopped = false;

		PreviewHandler(String name) {
			super(name);
			mHandler = null;
			start();
		}

		//@Override
		public void onPreviewFrame(final byte[] data, final Camera camera) {
			if (mHandler != null && camera == mCamera) {
				mHandler.post(new Runnable() {
					//@Override
					public void run() {
						if ( !stopped )
						{
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
			Log.v(TAG, "onLooperPrepared()");
			if (stopped == false) {
				mHandler = new Handler();
				Log.d(TAG, "frame processing start");
				setPreviewCallbackWithBuffer(this);
				//mCamera.setPreviewCallback(this);
			}
		}

		synchronized private void stopPreview() {
			stopped = true;
			if (mHandler != null) {
				Log.d(TAG, "frame processing stop");
				setPreviewCallbackWithBuffer(null);
				//mCamera.setPreviewCallback(null);
				mHandler = null;
			}
		}
	}

	private static final Method camAddCbBuffer = getCamAddCallbackBuffer();
	private static final Method camSetPreview = getCamSetPreviewCallbackWithBuffer();
	private static final boolean bufferWorks = camAddCbBuffer != null
			&& camSetPreview != null;
}
