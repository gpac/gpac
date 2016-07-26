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

    //Startof Options
    private static final int SENSOR_DELAY = SensorManager.SENSOR_DELAY_FASTEST;   // 0: SENSOR_DELAY_FASTEST, 1: SENSOR_DELAY_GAME, 2: SENSOR_DELAY_UI, 3: SENSOR_DELAY_NORMAL

    private static final boolean USE_ORIENTATION_FILTER = true;       //if true smoothSensorMeasurement is applied to getOrientation result
    private static final boolean USE_ORIENTATION_THRESHOLD = true;    //if true keepOrientation() discards results within the error margin

    //the lower the value, the more smoothing is applied (lower response) - set to 1.0 for no filter
    private static final float ORIENTATION_FILTER_LVL = 0.04f;
    //threshold to discard orientation x, y, z
    private static final float[] ORIENTATION_THRESHOLD = {0.2f, 0.02f, 0.02f};

    //for sensors fusion filter
    public static final int TIME_CONSTANT = 30;
    public static final float FILTER_COEFFICIENT = 0.98f;
    //Endof Options


    private static SensorManager sensorManager;

    private static Sensor accelerometer;
    private static Sensor magnetometer;
    private static Sensor gyroscope;

    protected Osmo4Renderer rend;
    private Display displayDev;

    private boolean newOrientation = false; //auto-set to true when new Rotation arrives (from acc), auto-set to false when rotation is sent to player
    private boolean initGyro = true;   //when gyro is initialized it is set to false

    private Timer fuseTimer = new Timer();

    private float[] acceleration = {0.0f, 0.0f, 0.0f};  //holds acceleration values (g)
    private float[] magnetic = {0.0f, 0.0f, 0.0f};      //holds magnetic field values (μT)
    private float[] gyro = {0.0f, 0.0f, 0.0f};          //hold gyroscope values (rad/s)
    private float[] orientation = {0.0f, 0.0f, 0.0f};   //from acc+magn
    private float[] gyroOrientation = {0.0f, 0.0f, 0.0f};
    private float[] fusedOrientation = {0.0f, 0.0f, 0.0f};  //from acc+magn+gyro
    private float[] lastOrient = {0.0f, 0.0f, 0.0f}, prevOrient;
    private float gyroTimestamp = 0;

    private float rotationMx[] = new float[9];
    private float rotationMxRaw[] = new float[9];
    private float[] gyroMx = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};    //init to identity

    private static final String LOG_TAG = "GPAC SensorServices";
    private static final float _PI_ = (float) Math.PI;

    private static final float EPSILON = 0.000000001f;
    private static final float NS2S = 1.0f / 1000000000.0f;

    private boolean useGyroscope;   //automatically set to true, if gyroscope sensor is detected


    /**
     * Constructor (initialize sensors)
     *
     * @param context The parent Context
     * @return SensorServices object
     */
    public SensorServices(Context context) {
        sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);

        if (magnetometer == null || accelerometer == null)
            Log.e(LOG_TAG, "No Accelerometer and/or Magnetic Field sensors found"); //TODOk handle this - rollback to manual navigation

        if (gyroscope != null) {
            useGyroscope = true;
        } else {
            Log.i(LOG_TAG, "No Gyroscope found - using only Accelerometer and Magnetic Field");
        }

        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        displayDev = wm.getDefaultDisplay();

    }

    public void setRenderer(Osmo4Renderer renderer) {
        rend = renderer;
    }

    /**
     * Register sensors to start receiving data
     */
    public void registerSensors() {
        sensorManager.registerListener(this, magnetometer, SENSOR_DELAY);
        sensorManager.registerListener(this, accelerometer, SENSOR_DELAY);
        if (useGyroscope) {
            sensorManager.registerListener(this, gyroscope, SENSOR_DELAY);
            fuseTimer.scheduleAtFixedRate(new calculateFusedOrientationTask(), 1000, TIME_CONSTANT);
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

        switch (event.sensor.getType()) {
            case Sensor.TYPE_ACCELEROMETER:
                acceleration = event.values;
                newOrientation = calculateRotationMx();
                if (!newOrientation) return;
                calculateOrientation();
                if (!useGyroscope) updateOrientation();
                break;
            case Sensor.TYPE_MAGNETIC_FIELD:
                magnetic = event.values;
                break;
            case Sensor.TYPE_GYROSCOPE:
                gyro = event.values;
                if (newOrientation) updateGyroscope((float) event.timestamp);
                break;
            default:
                return;
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

        try {
            gotRotation = SensorManager.getRotationMatrix(rotationMxRaw, null, acceleration, magnetic);
        } catch (Exception e) {
            gotRotation = false;
            Log.e(LOG_TAG, "Error getting rotation matrix" + e.getMessage());
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
        Log.v(LOG_TAG, "Received Orientation - Yaw: " + orientation[0] + " , Pitch: " + orientation[1] + " , Roll: " + orientation[2]);
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
                Log.v(LOG_TAG, "Smoothed Orientation - Yaw: " + prevOrient[0] + " , Pitch: " + prevOrient[1] + " , Roll: " + prevOrient[2]);
            } else {
                prevOrient = lastOrient;
            }
        }

        //NOTE: we invert yaw and roll (for 360 navigation)
        rend.getInstance().onOrientationChange(-prevOrient[0], prevOrient[1], -prevOrient[2]);
        newOrientation = false;
    }

    /**
     * Get Rotation Vector from Gyroscope sample
     * Output is passed back to deltaRotation
     *
     * ref: https://developer.android.com/reference/android/hardware/SensorEvent.html#values
     */
    private void getRotationFromGyro(float[] in, float[] deltaRotation, float dT) {

        float[] axes = new float[3];    //0: X, 1: Y, 3: Z

        // Calculate the angular speed of the sample
        float omega = (float) Math.sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);

        // Normalize the rotation vector if it's big enough to get the axis
        if (omega > EPSILON) {
            axes[0] = in[0] / omega;
            axes[1] = in[1] / omega;
            axes[2] = in[2] / omega;
        }

        float thetaOverTwo = omega * dT / 2.0f;
        float sinThetaOverTwo = (float) Math.sin(thetaOverTwo);
        float cosThetaOverTwo = (float) Math.cos(thetaOverTwo);
        deltaRotation[0] = sinThetaOverTwo * axes[0];
        deltaRotation[1] = sinThetaOverTwo * axes[1];
        deltaRotation[2] = sinThetaOverTwo * axes[2];
        deltaRotation[3] = cosThetaOverTwo;
    }

    private void updateGyroscope(float newTime) {

        //check if initialization is needed
        if (initGyro) {
            float[] initMatrix = new float[9];
            initMatrix = getRotationMxFromOrientation(orientation);
            float[] tmp = new float[3];
            SensorManager.getOrientation(initMatrix, tmp);
            gyroMx = multiplyMx(gyroMx, initMatrix);
            initGyro = false;
        }

        float[] deltaVector = new float[4];

        if (gyroTimestamp != 0) {
            float dT = (newTime - gyroTimestamp) * NS2S;
            getRotationFromGyro(gyro, deltaVector, dT);
        }

        gyroTimestamp = newTime;

        float[] deltaMatrix = new float[9];
        SensorManager.getRotationMatrixFromVector(deltaMatrix, deltaVector);

        gyroMx = multiplyMx(gyroMx, deltaMatrix);

        SensorManager.getOrientation(gyroMx, gyroOrientation);

        newOrientation = false;

        rend.getInstance().onOrientationChange(-gyroOrientation[0], gyroOrientation[1], -gyroOrientation[2]);
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

    class calculateFusedOrientationTask extends TimerTask {
        public void run() {
            float oneMinusCoeff = 1.0f - FILTER_COEFFICIENT;
            fusedOrientation[0] =
                    FILTER_COEFFICIENT * gyroOrientation[0]
                            + oneMinusCoeff * orientation[0];

            fusedOrientation[1] =
                    FILTER_COEFFICIENT * gyroOrientation[1]
                            + oneMinusCoeff * orientation[1];

            fusedOrientation[2] =
                    FILTER_COEFFICIENT * gyroOrientation[2]
                            + oneMinusCoeff * orientation[2];

            // overwrite gyro matrix and orientation with fused orientation
            // to comensate gyro drift
            gyroMx = getRotationMxFromOrientation(fusedOrientation);
            System.arraycopy(fusedOrientation, 0, gyroOrientation, 0, 3);
        }
    }


}