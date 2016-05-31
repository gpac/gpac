package com.gpac.Osmo4;

import android.content.Context;
import android.util.Log;

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
public class SensorServices implements SensorEventListener, GPACInstanceInterface {

	private static SensorManager sensorManager;

	private static Sensor accelerometer;
	private static Sensor magnetometer;

    protected  Osmo4Renderer rend;

	private float[] lastAcc;
	private float[] lastMagn;

    private float rotation[] = new float[9];
    private float identity[] = new float[9];

    private boolean initAcc = false, initMagn = false;

    private static final String LOG_TAG = "GPAC SensorServices";

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

    public void setRenderer(Osmo4Renderer renderer){
            rend = renderer;
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

        switch(event.sensor.getType()){
            case Sensor.TYPE_ACCELEROMETER:
                lastAcc = event.values;
                initAcc = true;
                break;
            case Sensor.TYPE_MAGNETIC_FIELD:
                lastMagn = event.values;
                initMagn = true;
                break;
            default:
                return;
        }

        boolean gotRotation = false;
        try {
            gotRotation = SensorManager.getRotationMatrix(rotation, identity, lastAcc, lastMagn);
        } catch (Exception e) {
            gotRotation = false;
            Log.e(LOG_TAG, "Error getting rotation and identity matrices"+ e.getMessage());
        }

        if(gotRotation){

            float orientation[] = new float[3];
            SensorManager.getOrientation(rotation, orientation);
            Log.v(LOG_TAG, "We have orientation: "+orientation[0]+" ,  "+orientation[1]+" ,  "+orientation[2]);

            //NOTE: we invert yaw and roll (for 360 navigation)
            rend.getInstance().onOrientationChange(- orientation[0], orientation[1], - orientation[2]);

        }

    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    	//required - but not used
    }

    @Override
    public native void setGpacLogs(String tools_at_levels);

    @Override
    public native void setGpacPreference(String category, String name, String value);

    @Override
    public void destroy(){}

    @Override
    public void connect(String pop){}

    @Override
    public void disconnect(){}
}