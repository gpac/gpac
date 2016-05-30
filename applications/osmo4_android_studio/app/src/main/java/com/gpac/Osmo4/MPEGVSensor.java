package com.gpac.Osmo4;

import android.app.Activity;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;

/**
 * Handle Physical sensors
 * 
 * @authors Ivica Arsov (ivica.arsov@it-sudparis.eu) - Pierre Souchay <pierre.souchay@vizionr.fr> (last changed by $LastChangedBy$)
 * @version $Revision$
 * 
 */
public class MPEGVSensor implements SensorEventListener, LocationListener {

    public static Activity myAct = null;

    private native void sendData(int ptr, String data);

    private static SensorManager mySensorManager = null;
    private static LocationManager myLocationManager = null;

    private static Sensor sensor = null;

    private static Sensor sensor1 = null;

    private int SensorType = -2;

    private int InternalPtr = 0;

    private Boolean run = false;

    /**
     * Init the sensor Manager
     * 
     * @param mgr The manager to set
     * @return the sensor manager set
     */
    public static SensorManager initSensorManager(SensorManager mgr) {
        return mySensorManager = mgr;
    }

    /**
     * Init the location Manager
     * 
     * @param mgr The manager to set
     * @return the location manager set
     */
    public static LocationManager initLocationManager(LocationManager mgr) {
        return myLocationManager = mgr;
    }

    /**
     * Starts the given sensor and register the listener
     * 
     * @param ptr The pointer
     * @param type The type of sensor as defined in {@link Sensor}
     */
    public void startSensor(int ptr, int type) {
        InternalPtr = ptr;
		SensorType = type;
		
		switch (SensorType) {
			case Sensor.TYPE_ORIENTATION: {
				sensor = mySensorManager
						.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_GAME))
					return;
				sensor1 = mySensorManager
						.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor1,
						SensorManager.SENSOR_DELAY_GAME))
				{
					mySensorManager.unregisterListener(MPEGVSensor.this);
					return;
				}
				run = true;
				break;
			}
			case Sensor.TYPE_ACCELEROMETER: {
				sensor = mySensorManager
						.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
				if( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_GAME))
					return;
				run = true;
				break;
			}
			case Sensor.TYPE_GYROSCOPE: {
				sensor = mySensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_GAME))
					return;
				run = true;
				break;
			}
			case Sensor.TYPE_MAGNETIC_FIELD: {
				sensor = mySensorManager
						.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_GAME))
					return;
				run = true;
				break;
			}
			case Sensor.TYPE_LIGHT: {
				sensor = mySensorManager.getDefaultSensor(Sensor.TYPE_LIGHT);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_UI))
					return;
				run = true;
				break;
			}
			case Sensor.TYPE_PRESSURE: {
				sensor = mySensorManager.getDefaultSensor(Sensor.TYPE_PRESSURE);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_UI))
					return;
				run = true;
				break;
			}
			//case Sensor.TYPE_ROTATION_VECTOR: {
			case 11: {
				sensor = mySensorManager
						.getDefaultSensor(11);
				if ( !mySensorManager.registerListener(MPEGVSensor.this, sensor,
						SensorManager.SENSOR_DELAY_GAME))
					return;
				run = true;
				break;
			}
			case 100: { // Type for the GPS sensor
				myAct.runOnUiThread(new Runnable() {
					
					@Override
					public void run() {
						myLocationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0, 0, MPEGVSensor.this);
						run = true;
					}
				});
			}
		}
    }

    /**
     * Stops the sensor and unregister listener
     */
    public void stopSensor() {
        if (SensorType == 100)
			myAct.runOnUiThread(new Runnable() {

				@Override
				public void run() {

					myLocationManager.removeUpdates(MPEGVSensor.this);

				}
			});
		else
			mySensorManager.unregisterListener(MPEGVSensor.this);
		run = false;
    }

    @Override
    public void onAccuracyChanged(Sensor arg0, int arg1) {
        // TODO Auto-generated method stub

    }

	public double azzimuth = 0;
	public double pitch = 0;
	public double roll = 0;

	public double x, y, z, angle;

	private float[] mGData = new float[3];
	private float[] mMData = new float[3];
	private float[] mR = new float[16];
	private float[] mI = new float[16];
	private float[] mOrientation = new float[3];

	int buffsize = 8;

	public double[] al = new double[buffsize];
	public double[] pl = new double[buffsize];
	public double[] rl = new double[buffsize];
	int ind = 0;

	double rotangle = 0;

	@Override
	public void onSensorChanged(SensorEvent event) {
		
		if ( !run )
			return;
		
		switch (SensorType)
		{
			case Sensor.TYPE_ORIENTATION:
			{
				int type = event.sensor.getType();
		        if (type == Sensor.TYPE_ACCELEROMETER) {
		            for (int i=0 ; i<3 ; i++)
		            	mGData[i] = event.values[i];
			        
		            lowPass(mGData, accel);
			        //filterAccData();
		            //Log.i("Acc: ", "" + event.values[0] + " " + event.values[1] + " " + event.values[2]);
		        } else if (type == Sensor.TYPE_MAGNETIC_FIELD) {
		            for (int i=0 ; i<3 ; i++)
		            	mMData[i] = event.values[i];
			        
		            lowPass(mMData, mag);
			        //filterMagData();
		            //Log.i("Mag: ", "" + event.values[0] + " " + event.values[1] + " " + event.values[2]);
		        } else {
		        	if ( event.sensor.getType() == Sensor.TYPE_ORIENTATION )
		    		{
		    			azzimuth = event.values[0];
		    			pitch = event.values[1];
		    			roll = event.values[2];
		    		}
		        	return;
		        }
		        
		        SensorManager.getRotationMatrix(mR, mI, mGData, mMData);
		        SensorManager.getOrientation(mR, mOrientation);
		        
		        lowPass(mOrientation, ordata);
		        //filterOrientationData();
		        
		        al[ind] = mOrientation[0];
		        pl[ind] = mOrientation[1];
		        rl[ind] = mOrientation[2];
		        
		        if ( ++ind == buffsize )
		        	ind = 0;
		        
		        double a = 0, p = 0, r = 0;
		        for ( int i =0; i < buffsize; i++ )
		        {
		        	a += al[i];
		        	p += pl[i];
		        	r += rl[i];
		        }
		        
		        a /= buffsize;
		        p /= buffsize;
		        r /= buffsize;
		        
		        final float rad2deg = (float)(180.0f/Math.PI);
		        azzimuth = a*rad2deg;//mOrientation[0]*rad2deg;
				pitch = p*rad2deg;//mOrientation[1]*rad2deg;
				roll = r*rad2deg;//mOrientation[2]*rad2deg;
				
//				mOrientation[0] = (float)azzimuth;
//				mOrientation[1] = (float)pitch;
//				mOrientation[2] = (float)roll;
//				
//				filterOrientationData();
//				
//				azzimuth = mOrientation[0];
//		        pitch = mOrientation[1];
//		        roll = mOrientation[2];
				
				String res = "";
				res += azzimuth+";" + pitch + ";" + roll + ";";
				sendData(InternalPtr, res);
				break;
			}
			case Sensor.TYPE_ACCELEROMETER:
			{
				for (int i=0 ; i<3 ; i++)
	            	mGData[i] = event.values[i];
				
				lowPass(mGData, accel);
				
				al[ind] = mGData[0];
		        pl[ind] = mGData[1];
		        rl[ind] = mGData[2];
		        
		        if ( ++ind == buffsize )
		        	ind = 0;
		        
		        double a = 0, p = 0, r = 0;
		        for ( int i =0; i < buffsize; i++ )
		        {
		        	a += al[i];
		        	p += pl[i];
		        	r += rl[i];
		        }
		        
		        a /= buffsize;
		        p /= buffsize;
		        r /= buffsize;
		        
		        String res = "";
				res += a+";" + p + ";" + r + ";";
				sendData(InternalPtr, res);
				break;
			}
			case Sensor.TYPE_GYROSCOPE:
			{
				for (int i=0 ; i<3 ; i++)
	            	mGData[i] = event.values[i];
				
				lowPass(mGData, accel);
				
				al[ind] = mGData[0];
		        pl[ind] = mGData[1];
		        rl[ind] = mGData[2];
		        
		        if ( ++ind == buffsize )
		        	ind = 0;
		        
		        double a = 0, p = 0, r = 0;
		        for ( int i =0; i < buffsize; i++ )
		        {
		        	a += al[i];
		        	p += pl[i];
		        	r += rl[i];
		        }
		        
		        a /= buffsize;
		        p /= buffsize;
		        r /= buffsize;
		        
		        String res = "";
				res += a+";" + p + ";" + r + ";";
				sendData(InternalPtr, res);
				break;
			}
			case Sensor.TYPE_MAGNETIC_FIELD:
			{
				for (int i=0 ; i<3 ; i++)
	            	mGData[i] = event.values[i];
				
				lowPass(mGData, accel);
				
				al[ind] = mGData[0];
		        pl[ind] = mGData[1];
		        rl[ind] = mGData[2];
		        
		        if ( ++ind == buffsize )
		        	ind = 0;
		        
		        double a = 0, p = 0, r = 0;
		        for ( int i =0; i < buffsize; i++ )
		        {
		        	a += al[i];
		        	p += pl[i];
		        	r += rl[i];
		        }
		        
		        a /= buffsize;
		        p /= buffsize;
		        r /= buffsize;
		        
		        String res = "";
				res += a+";" + p + ";" + r + ";";
				sendData(InternalPtr, res);
				break;
			}
			//case Sensor.TYPE_ROTATION_VECTOR:
			case 11:
			{
				double a = event.values[0], p = event.values[1], r = event.values[2];
				
				final float rad2deg = (float)(180.0f/Math.PI);
		        a = a*rad2deg;
				p = p*rad2deg;
				r = r*rad2deg;
				
				String res = "";
				res += a + ";" + p + ";" + r + ";";
				sendData(InternalPtr, res);
				
				break;
			}
			case Sensor.TYPE_LIGHT:
			{
				String res = "";
				res += event.values[0] + ";";
				sendData(InternalPtr, res);
				break;
			}
			case Sensor.TYPE_PRESSURE:
			{
				String res = "";
				res += event.values[0]+";";
				sendData(InternalPtr, res);
				break;
			}
		}
	}

	static final float ALPHA = 0.01f;

	protected float[] lowPass(float[] input, float[] output) {
		if (output == null)
			return input;

		for (int i = 0; i < input.length; i++) {
			output[i] = output[i] + ALPHA * (input[i] - output[i]);
		}
		return output;
	}

	Boolean accinited = false;
	float[] accel = new float[3];

	private void filterData(float[] data, float[] filter) {
		float kFilteringFactor = 0.1f;

		filter[0] = data[0] * kFilteringFactor + filter[0]
				* (1.0f - kFilteringFactor);
		filter[1] = data[1] * kFilteringFactor + filter[1]
				* (1.0f - kFilteringFactor);
		filter[2] = data[2] * kFilteringFactor + filter[2]
				* (1.0f - kFilteringFactor);
		data[0] = data[0] - filter[0];
		data[1] = data[1] - filter[1];
		data[2] = data[2] - filter[2];

		filter[0] = data[0];
		filter[1] = data[1];
		filter[2] = data[2];
	}

	private void filterAccData() {
		if (!accinited) {
			accel[0] = mGData[0];
			accel[1] = mGData[1];
			accel[2] = mGData[2];
			accinited = true;
		}
		filterData(mGData, accel);
	}

	Boolean maginited = false;
	float[] mag = new float[3];

	private void filterMagData() {
		if (!maginited) {
			mag[0] = mMData[0];
			mag[1] = mMData[1];
			mag[2] = mMData[2];
			maginited = true;
		}
		filterData(mMData, mag);
	}

	Boolean orinited = false;
	float[] ordata = new float[3];

	private void filterOrientationData() {
		if (!orinited) {
			ordata[0] = mOrientation[0];
			ordata[1] = mOrientation[1];
			ordata[2] = mOrientation[2];
			orinited = true;
		}
		filterData(mOrientation, ordata);
	}

	@Override
	public void onLocationChanged(Location loc) {
		String res = "";
		res += loc.getLongitude() + ";" 
				+ loc.getLatitude() + ";" 
				+ loc.getAltitude() + ";"
				+ loc.getAccuracy() + ";"
				+ loc.getBearing() + ";";
		sendData(InternalPtr, res);
	}

	@Override
	public void onProviderDisabled(String arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onProviderEnabled(String arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onStatusChanged(String arg0, int arg1, Bundle arg2) {
		// TODO Auto-generated method stub
		
	}

}

