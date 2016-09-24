package com.gpac.Osmo4;

import android.content.Context;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;


import java.util.TimerTask;
import java.util.Timer;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

/**
 * Class for 360video sensors management
 *
 * @author Emmanouil Potetsianakis <emmanouil.potetsianakis@telecom-paristech.fr>
 * @version $Revision$
 */
public class SensorServices implements SensorEventListener, GPACInstanceInterface {

    //automatically set to true, if TYPE_ROTATION_VECTOR sensor is detected
    //NOTE: this is a composite sensor; does not work with fusion
    private boolean useRotationVector;

//Startof Options
    private static final int SENSOR_DELAY = SensorManager.SENSOR_DELAY_FASTEST;   // 0: SENSOR_DELAY_FASTEST, 1: SENSOR_DELAY_GAME, 2: SENSOR_DELAY_UI, 3: SENSOR_DELAY_NORMAL

    private static final boolean USE_ORIENTATION_FILTER = true;       //if true smoothSensorMeasurement is applied to getOrientation result
    private static final boolean USE_ORIENTATION_THRESHOLD = true;    //if true keepOrientation() discards results within the error margin

    //the lower the value, the more smoothing is applied (lower response) - set to 1.0 for no filter
    private static final float ORIENTATION_FILTER_LVL = 0.04f;
    //threshold to discard orientation x, y, z
    private static final float[] ORIENTATION_THRESHOLD = {0.2f, 0.02f, 0.02f};
//Endof Options


    private static SensorManager sensorManager;

    private static Sensor accelerometer;    //base sensor
    private static Sensor magnetometer;     //base sensor
    private static Sensor rotationSensor;   //composite sensor

    protected Osmo4Renderer rend;
    private Display displayDev;

    private boolean newOrientation = false; //auto-set to true when new Rotation arrives (from acc), auto-set to false when rotation is sent to player

    private Timer fuseTimer = new Timer();

    private float[] acceleration = {0.0f, 0.0f, 0.0f};  //holds acceleration values (g)
    private float[] magnetic = {0.0f, 0.0f, 0.0f};      //holds magnetic field values (μT)
    private float[] rotationValues = {0.0f, 0.0f, 0.0f};//holds rotation vector
    private float[] orientation = {0.0f, 0.0f, 0.0f};   //from acc+magn
    private float[] lastOrient = {0.0f, 0.0f, 0.0f}, prevOrient;

    private float rotationMx[] = new float[9];
    private float rotationMxRaw[] = new float[9];

    private static final String LOG_TAG = "GPAC SensorServices";
    private static final float _PI_ = (float) Math.PI;


    /**
     * Constructor (initialize sensors)
     *
     * @param context The parent Context
     * @return SensorServices object
     */
    public SensorServices(Context context) {
        //init sensors
        sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        rotationSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR);

        //init the rest
        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        displayDev = wm.getDefaultDisplay();

        //check sensor availability and select
        if(rotationSensor!=null){
            useRotationVector = true;
            return;
        }else{
            Log.w(LOG_TAG, "No Rotation Vector composit sensor found - Switching to Acc+Magn sensors");
        }

        if (magnetometer == null || accelerometer == null){
            Log.e(LOG_TAG, "No Accelerometer and/or Magnetic Field sensors found"); //TODOk handle this - rollback to manual navigation
        }

    }

    public void setRenderer(Osmo4Renderer renderer) {
        rend = renderer;
    }

    /**
     * Register sensors to start receiving data
     */
    public void registerSensors() {

        if(rotationSensor!=null){
            sensorManager.registerListener(this, rotationSensor, SENSOR_DELAY);
            Log.i(LOG_TAG, "Using Rotation Vector sensor for 360navigation");
            return;
        }

        if(magnetometer != null && accelerometer != null){
            sensorManager.registerListener(this, magnetometer, SENSOR_DELAY);
            sensorManager.registerListener(this, accelerometer, SENSOR_DELAY);
            Log.i(LOG_TAG, "Using Acceleration & Magnetic Field sensors for 360navigation");
        }
    }

    //TODOk we do not handle this yet - there is an issue with onResume (not working) and onPause (working)
    public void unregisterSensors() {
        sensorManager.unregisterListener(this);
    }

    /**
     * We calculate rotation/orientation on new acc measurements
     **/
    @Override
    public void onSensorChanged(SensorEvent event) {

        boolean newevent = false;

        switch (event.sensor.getType()) {
            case Sensor.TYPE_ROTATION_VECTOR:
                rotationValues = event.values.clone();
                newOrientation = calculateRotationMx();
                if (newOrientation){
                    float[] tmpOrient = new float[3];
                    SensorManager.getOrientation(rotationMx, tmpOrient);
                    rend.getInstance().onOrientationChange(-tmpOrient[0], tmpOrient[1], -tmpOrient[2]);
                }
                return;
            case Sensor.TYPE_ACCELEROMETER:
                newevent=true;
                acceleration = event.values.clone();
                break;
            case Sensor.TYPE_MAGNETIC_FIELD:
                newevent=true;
                magnetic = event.values.clone();
                break;
            default:
                return;
        }
        if(newevent){
            newOrientation = calculateRotationMx();
            if (!newOrientation) return;
            calculateOrientation();
            updateOrientation();
        }
    }

    /**
     * Calculate orientation X,Y,Z from Acc and Magn
     * Orientation is stored in prevOrient
     * Rotation matrix is stored in rotationMx
     *
     * @return true if rotationMatrix was updated
     */
    private boolean calculateRotationMx() {
        boolean gotRotation = false;


        if(useRotationVector){
            try {
                SensorManager.getRotationMatrixFromVector(rotationMxRaw, rotationValues);
                gotRotation = true;
            } catch (Exception e) {
                gotRotation = false;
                Log.e(LOG_TAG, "Error getting rotation matrix" + e.getMessage());
            }
        }else{
            try {
                gotRotation = SensorManager.getRotationMatrix(rotationMxRaw, null, acceleration, magnetic);
            } catch (Exception e) {
                gotRotation = false;
                Log.e(LOG_TAG, "Error getting rotation matrix" + e.getMessage());
            }
        }    

        if (gotRotation) {
            //NOTE: the rotation considered is according to device natural orientation
            //and it might NOT be the same as the screen orientation
            switch (displayDev.getRotation()) {
                case 1: //Surface.ROTATION_90
                    rotationMx = rotationMxRaw.clone();
                    break;
                case 2: //Surface.ROTATION_180
                    SensorManager.remapCoordinateSystem(rotationMxRaw, SensorManager.AXIS_Y, SensorManager.AXIS_MINUS_X, rotationMx);
                    break;
                case 3: //Surface.ROTATION_270
                    SensorManager.remapCoordinateSystem(rotationMxRaw, SensorManager.AXIS_MINUS_X, SensorManager.AXIS_MINUS_Y, rotationMx);
                    break;
                case 0: //no rotation (= Surface.ROTATION_0)
                    SensorManager.remapCoordinateSystem(rotationMxRaw, SensorManager.AXIS_MINUS_Y, SensorManager.AXIS_X, rotationMx);
                    break;
            }
        }

        return gotRotation;
    }

    private void calculateOrientation() {
        float[] tmpOrient = new float[3];
        SensorManager.getOrientation(rotationMx, tmpOrient);
        orientation = tmpOrient.clone();
    }

    private void updateOrientation() {
        lastOrient = orientation.clone();
        boolean refreshOrientation = true;
        if (USE_ORIENTATION_THRESHOLD) {
            refreshOrientation = keepOrientation(lastOrient, prevOrient);
        }

        if (refreshOrientation) {
            if (USE_ORIENTATION_FILTER) {
                prevOrient = smoothSensorMeasurement(lastOrient, prevOrient);
            } else {
                prevOrient = lastOrient;
            }
        }

        //NOTE: we invert yaw and roll (for 360 navigation)
        rend.getInstance().onOrientationChange(-prevOrient[0], prevOrient[1], -prevOrient[2]);
        newOrientation = false;
    }

    private float[] getRotationMxFromOrientation(float[] o) {
        float[] xM = new float[9];
        float[] yM = new float[9];
        float[] zM = new float[9];

        float sinX = (float) Math.sin(o[1]);
        float cosX = (float) Math.cos(o[1]);
        float sinY = (float) Math.sin(o[2]);
        float cosY = (float) Math.cos(o[2]);
        float sinZ = (float) Math.sin(o[0]);
        float cosZ = (float) Math.cos(o[0]);

        // rotation about x-axis (pitch)
        xM[0] = 1.0f;
        xM[1] = 0.0f;
        xM[2] = 0.0f;
        xM[3] = 0.0f;
        xM[4] = cosX;
        xM[5] = sinX;
        xM[6] = 0.0f;
        xM[7] = -sinX;
        xM[8] = cosX;

        // rotation about y-axis (roll)
        yM[0] = cosY;
        yM[1] = 0.0f;
        yM[2] = sinY;
        yM[3] = 0.0f;
        yM[4] = 1.0f;
        yM[5] = 0.0f;
        yM[6] = -sinY;
        yM[7] = 0.0f;
        yM[8] = cosY;

        // rotation about z-axis (azimuth)
        zM[0] = cosZ;
        zM[1] = sinZ;
        zM[2] = 0.0f;
        zM[3] = -sinZ;
        zM[4] = cosZ;
        zM[5] = 0.0f;
        zM[6] = 0.0f;
        zM[7] = 0.0f;
        zM[8] = 1.0f;

        // rotation order is y, x, z (roll, pitch, azimuth)
        float[] resultMatrix = multiplyMx(xM, yM);
        resultMatrix = multiplyMx(zM, resultMatrix);
        return resultMatrix;
    }

    private float[] multiplyMx(float[] A, float[] B) {
        float[] result = new float[9];

        result[0] = A[0] * B[0] + A[1] * B[3] + A[2] * B[6];
        result[1] = A[0] * B[1] + A[1] * B[4] + A[2] * B[7];
        result[2] = A[0] * B[2] + A[1] * B[5] + A[2] * B[8];

        result[3] = A[3] * B[0] + A[4] * B[3] + A[5] * B[6];
        result[4] = A[3] * B[1] + A[4] * B[4] + A[5] * B[7];
        result[5] = A[3] * B[2] + A[4] * B[5] + A[5] * B[8];

        result[6] = A[6] * B[0] + A[7] * B[3] + A[8] * B[6];
        result[7] = A[6] * B[1] + A[7] * B[4] + A[8] * B[7];
        result[8] = A[6] * B[2] + A[7] * B[5] + A[8] * B[8];

        return result;
    }

    private static boolean keepOrientation(float[] in, float[] out) {

        if (out == null) return true;

        for (int i = 0; i < in.length; i++) {
            if (Math.abs(in[i] - out[i]) > ORIENTATION_THRESHOLD[i]) return true;
        }

        return false;
    }


    private static float[] smoothSensorMeasurement(float[] in, float[] out) {

        if (out == null) return in;

        float[] output = {0.0f, 0.0f, 0.0f};
        float diff = 0.0f;


        for (int i = 0; i < in.length; i++) {

            diff = (in[i] - out[i]);

            if (Math.abs(diff) > Math.PI) {
                float diff_f = 0.0f;
                if (diff > Math.PI) {
                    diff = 2 * _PI_ - diff;
                    diff_f = out[i] - ORIENTATION_FILTER_LVL * diff;
                    if (diff_f < -Math.PI) {
                        output[i] = (2 * _PI_ + diff_f);
                    } else {
                        output[i] = diff_f;
                    }
                } else if (diff < -Math.PI) {
                    diff = 2 * _PI_ + diff;
                    diff_f = out[i] + ORIENTATION_FILTER_LVL * diff;
                    if (diff_f > Math.PI) {
                        output[i] = -2 * _PI_ + diff_f;
                    } else {
                        output[i] = diff_f;
                    }

                }

            } else {
                output[i] = out[i] + ORIENTATION_FILTER_LVL * diff;
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
    public void destroy() {
    }

    @Override
    public void connect(String pop) {
    }

    @Override
    public void disconnect() {
    }

}