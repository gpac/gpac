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

    private float[] acceleration = {0.0f, 0.0f, 0.0f};
    private float[] magnetic = {0.0f, 0.0f, 0.0f};
    private float[] lastOrient = {0.0f, 0.0f, 0.0f}, prevOrient;

    private float rotation[] = new float[9];
    private float identity[] = new float[9];

    private static final String LOG_TAG = "GPAC SensorServices";
    private static final float _PI_ = (float) Math.PI;

    //the lower the value, the more smoothing is applied (lower response) - set to 1.0 for no filter
    private static final float filterLevel = 0.05f;
    //threshold to discard orientation x, y, z
    private static float[] orThreshold = {0.2f, 0.02f, 0.02f};

        private static final boolean useOrientationFilter = true;       //if true smoothSensorMeasurement is applied to getOrientation result
    private static final boolean useOrientationThreshold = true;    //if true keepOrientation() discards results within the error margin


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
        sensorManager.registerListener(this, magnetometer, SensorManager.SENSOR_DELAY_GAME);
        sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME);
    }

    public void unregisterSensors(){
        sensorManager.unregisterListener(this);
    }

    @Override
    public void onSensorChanged(SensorEvent event) {

        switch(event.sensor.getType()){
            case Sensor.TYPE_ACCELEROMETER:
                acceleration = event.values;
                break;
            case Sensor.TYPE_MAGNETIC_FIELD:
                magnetic = event.values;
                break;
            default:
                return;
        }

        boolean gotRotation = false;

        try {
                gotRotation = SensorManager.getRotationMatrix(rotation, identity, acceleration, magnetic);
        } catch (Exception e) {
            gotRotation = false;
            Log.e(LOG_TAG, "Error getting rotation and identity matrices"+ e.getMessage());
        }

        if(gotRotation){

            float orientation[] = new float[3];
            SensorManager.getOrientation(rotation, orientation);
            Log.v(LOG_TAG, "Received Orientation - Yaw: "+orientation[0]+" , Pitch: "+orientation[1]+" , Roll: "+orientation[2]);

            lastOrient = orientation;
            boolean refreshOrientation = true;
            if(useOrientationThreshold){
                refreshOrientation = keepOrientation(lastOrient, prevOrient);
            }

            if(refreshOrientation){
                if(useOrientationFilter){
                    prevOrient = smoothSensorMeasurement(lastOrient, prevOrient);
                    Log.v(LOG_TAG, "Smoothed Orientation - Yaw: "+prevOrient[0]+" , Pitch: "+prevOrient[1]+" , Roll: "+prevOrient[2]);
                }else{
                    prevOrient = lastOrient;
                }
            }

            //NOTE: we invert yaw and roll (for 360 navigation)
            rend.getInstance().onOrientationChange(- prevOrient[0], prevOrient[1], - prevOrient[2]);

        }

    }


    private static boolean keepOrientation(float[] in, float[] out){

        if(out==null) return true;



        for(int i=0; i<in.length; i++){
            if(Math.abs(in[i]-out[i])>orThreshold[i]) return true;
        }

        return false;
    }


    private static float[] smoothSensorMeasurement(float[] in, float[] out){

        if(out==null) return in;

        float[] output = {0.0f, 0.0f, 0.0f};
        float diff = 0.0f;


        for(int i=0; i<in.length; i++){

            diff = (in[i] - out[i]);

            if(Math.abs(diff)>Math.PI){
                float diff_f = 0.0f;
                if(diff>Math.PI){
                    diff = 2*_PI_ - diff;
                    diff_f = out[i]-filterLevel * diff;
                    if(diff_f < -Math.PI){
                        output[i] = (2*_PI_ + diff_f);
                    }else{
                        output[i] = diff_f;
                    }
                }else if(diff<-Math.PI){
                    diff = 2*_PI_ + diff;
                    diff_f = out[i] + filterLevel * diff;
                    if(diff_f > Math.PI){
                        output[i] = -2*_PI_+diff_f;
                    }else{
                        output[i] = diff_f;
                    }

                }

            }else{
                output[i] = out[i] + filterLevel * diff;
            }
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