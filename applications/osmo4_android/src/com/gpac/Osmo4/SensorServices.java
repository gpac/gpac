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
public class SensorServices implements SensorEventListener {

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
    public SensorServices(Context context){
        sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
    }

    /**
     * Register sensors to start receiving data
     * 
     * @return SensorServices object
     *
     */
    public void registerSensors(){
        sensorManager.registerListener(this, magnetometer, SensorManager.SENSOR_DELAY_UI);
        sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_UI);
    }

    public void unregisterSensors(){
        sensorManager.unregisterListener(this);
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