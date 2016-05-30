package com.gpac.Osmo4;

import android.content.Context;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

/**
 * Class for 360video sensors management
 *
 * @author Emmanouil Potetsianakis <emmanouil.potetsianakis@telecom-paristech.fr>
 * @version $Revision$
 * 
 */
public class SensorsServices implements SensorEventListener {

	private static SensorManager sensorManager;

	private static Sensor accelerometer;
	private static Sensor magnetometer;

	private float[] lastAcc;
	private float[] lastMagn;

    private float rotation[] = new float[9];
    private float identity[] = new float[9];

    private boolean initAcc = false, initMagn = false;


    /**
     * Constructor (initialize sensors)
     * 
     * @param context The parent Context
     * @return SensorServices object
     *
     */
    public SensorsServices(Context context){
        sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
    }


    @Override
    public void onSensorChanged(SensorEvent event) {
    	//new data arrives here
    }


    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    	//required - but not used
    }

}