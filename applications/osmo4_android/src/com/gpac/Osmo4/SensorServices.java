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

	private float[] lastAcc = {0.0f, 0.0f, 0.0f}, prevAcc;
	private float[] lastMagn = {0.0f, 0.0f, 0.0f}, prevMagn;
    private float[] lastOrient = {0.0f, 0.0f, 0.0f}, prevOrient;

    private float rotation[] = new float[9];
    private float identity[] = new float[9];

    private static final String LOG_TAG = "GPAC SensorServices";

    //the lower the value, the more smoothing is applied (lower response) - set to 1.0 for no filter
    private static final float filterLevel = 0.2f;
    private static final boolean useOrientationThreshold = true;    //if true keepOrientation() discards results within the error margin

    private static final boolean useSensorFilter = false;        //if true smoothSensorMeasurement is applied to Sensor values (Acceleration, Magnetic)

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
                if(useSensorFilter) prevAcc = smoothSensorMeasurement(lastAcc, prevAcc);
                break;
            case Sensor.TYPE_MAGNETIC_FIELD:
                lastMagn = event.values;
                if(useSensorFilter) prevMagn = smoothSensorMeasurement(lastMagn, prevMagn);
                break;
            default:
                return;
        }

        boolean gotRotation = false;

        try {
            if(!useSensorFilter){
                gotRotation = SensorManager.getRotationMatrix(rotation, identity, lastAcc, lastMagn);
            }else{
                gotRotation = SensorManager.getRotationMatrix(rotation, identity, prevAcc, prevMagn);
            }
            
        } catch (Exception e) {
            gotRotation = false;
            Log.e(LOG_TAG, "Error getting rotation and identity matrices"+ e.getMessage());
        }

        if(gotRotation){

            float orientation[] = new float[3];
            SensorManager.getOrientation(rotation, orientation);
            Log.v(LOG_TAG, "We have orientation: "+orientation[0]+" ,  "+orientation[1]+" ,  "+orientation[2]);

            lastOrient = orientation;
            boolean refreshOrientation = true;
            if(useOrientationThreshold){
                refreshOrientation = keepOrientation(lastOrient, prevOrient);
            }else{
                prevOrient = lastOrient;
            }

            if(refreshOrientation){
                prevOrient = smoothSensorMeasurement(lastOrient, prevOrient);
            }

            //NOTE: we invert yaw and roll (for 360 navigation)
            //rend.getInstance().onOrientationChange(- orientation[0], orientation[1], - orientation[2]);
            rend.getInstance().onOrientationChange(- prevOrient[0], prevOrient[1], - prevOrient[2]);

        }

    }


    private static boolean keepOrientation(float[] in, float[] out){
        
        if(out==null) return true;
        
        for(int i=0; i<in.length; i++){
            if(Math.abs(in[0]-out[0])<0.05) return false;
        }

        return true;
    }


    private static float[] smoothSensorMeasurement(float[] in, float[] out){
        
        if(out==null) return in;

        float[] output = {0.0f, 0.0f, 0.0f};

        for(int i=0; i<in.length; i++){
            output[i] = out[i] + filterLevel * (in[i] - out[i]);
        }

        return output;
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