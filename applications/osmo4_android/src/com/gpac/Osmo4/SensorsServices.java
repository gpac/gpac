package com.gpac.Osmo4;

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


    @Override
    public void onSensorChanged(SensorEvent event) {
    	//new data arrives here
    }


    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    	//required - but not used
    }

}