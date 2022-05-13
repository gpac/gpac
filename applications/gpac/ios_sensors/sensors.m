/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac ios application
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#import "sensors.h"

@implementation SensorAccess

- (id)init {
	if ((self = [super init])) {
        @autoreleasepool {
            self.motionManager = [[[CMMotionManager alloc] init] autorelease];
			self.motionManager.deviceMotionUpdateInterval = 0.1f;
            self.queue = [[[NSOperationQueue alloc] init] autorelease];
            
            self.locMngr = [[[CLLocationManager alloc] init] autorelease];
            self.locMngr.delegate = self;
            self.locMngr.desiredAccuracy = kCLLocationAccuracyBest;
            self.locMngr.distanceFilter = 2;
            
            sensorCallbak = NULL;
            sensorType = -1;
            gpsActive = 0;
        }
	}
	return self;
}

- (void)dealloc{
    self.locMngr.delegate = NULL;
    [super dealloc];
}

- (int) setSensorType :(int) type
{
    sensorType = type;
    return 0;
}

- (int) getSensorType
{
    return sensorType;
}

- (int) setSensorCallback:(SensorDataCallback*)callback
{
    sensorCallbak = callback;
    return 0;
}

- (int) startSensor
{
	sensorActive=1;
    switch (sensorType) {
        case SENSOR_ACCELEROMETER:
            if ( ![[self motionManager] isAccelerometerAvailable] )
                return 1;
            [[self motionManager] startAccelerometerUpdatesToQueue:[self queue] withHandler:^(CMAccelerometerData* data, NSError* error) {
                    if ( sensorCallbak && sensorActive) {
                        sensorCallbak(sensorType, data.acceleration.x, data.acceleration.y, data.acceleration.z, 0);
                    }
                }
             ];
            return 0;

        case SENSOR_GYROSCOPE:
            if ( ![[self motionManager] isGyroAvailable] )
                return 1;
            [[self motionManager] startGyroUpdatesToQueue:[self queue] withHandler:^(CMGyroData* data, NSError* error) {
                if ( sensorCallbak  && sensorActive) {
                    sensorCallbak(sensorType, data.rotationRate.x, data.rotationRate.y, data.rotationRate.z, 0);
                }
            }
             ];
            return 0;

		case SENSOR_ORIENTATION:
            if ( ![[self motionManager] isGyroAvailable] )
                return 1;
            [[self motionManager] startDeviceMotionUpdatesToQueue:[self queue] withHandler:^(CMDeviceMotion* data, NSError* error) {
                if ( sensorCallbak  && sensorActive) {
                    sensorCallbak(sensorType, data.attitude.quaternion.x, data.attitude.quaternion.y, data.attitude.quaternion.z, data.attitude.quaternion.w);
                }
            }
             ];
            return 0;

        case SENSOR_MAGNETOMETER:
            if ( ![[self motionManager] isMagnetometerAvailable] )
                return 1;
            [[self motionManager] startMagnetometerUpdatesToQueue:[self queue] withHandler:^(CMMagnetometerData* data, NSError* error) {
                if ( sensorCallbak  && sensorActive) {
                    sensorCallbak(sensorType, data.magneticField.x, data.magneticField.y, data.magneticField.z, 0);
                }
            }
             ];
            return 0;

        case SENSOR_GPS:
            [self.locMngr startUpdatingLocation];
            gpsActive = 1;
            return 0;

    }

    return 1;
}

- (int) stopSensor
{
	sensorActive=0;
    switch (sensorType) {
        case SENSOR_ACCELEROMETER:
            [[self motionManager] stopAccelerometerUpdates];
            return 0;
            break;
        case SENSOR_GYROSCOPE:
            [[self motionManager] stopGyroUpdates];
            return 0;
            break;
        case SENSOR_MAGNETOMETER:
            [[self motionManager] stopMagnetometerUpdates];
            return 0;
        case SENSOR_GPS:
            [self.locMngr stopUpdatingLocation];
            gpsActive = 0;
            return 0;
            break;
    }
    return 1;
}

- (int) isSensorStarted
{
    switch (sensorType) {
        case SENSOR_ACCELEROMETER:
            return [[self motionManager] isAccelerometerActive];
            break;
        case SENSOR_GYROSCOPE:
            return [[self motionManager] isGyroActive];
            break;
        case SENSOR_MAGNETOMETER:
            return [[self motionManager] isMagnetometerActive];
        case SENSOR_GPS:
            return gpsActive;
            break;
    }
    return 0;
}

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)location fromLocation:(CLLocation *)oldLocation {
    if ( sensorCallbak ) {
		sensorCallbak(sensorType, (float)location.coordinate.latitude, (float)location.coordinate.longitude, (float)location.altitude, (float)location.horizontalAccuracy);
	}
}

@end


int sensor_get_type(void* inst);
int sensor_start(void* inst);
int sensor_stop(void* inst);
int sensor_is_running(void* inst);

void *sensor_create(int type, SensorDataCallback *callback) {
    void* obj = [[SensorAccess alloc] init];
	if (!obj) return NULL;
    [(id)obj setSensorType:type];
    [(id)obj setSensorCallback:callback ];
    return obj;
}

int sensor_destroy(void** inst) {
    [(id)*inst dealloc];
    *inst = NULL;
    return 0;
}

int sensor_get_type(void* inst) {
    if ( !inst ) return 1;
    return [(id)inst getSensorType];
}

int sensor_start(void* inst) {
    if ( !inst ) return 1;
    return [(id)inst startSensor];
}

int sensor_stop(void* inst) {
    if ( !inst ) return 1;
    return [(id)inst stopSensor];
}

int sensor_is_running(void* inst) {
    if ( !inst ) return 1;
    return [(id)inst isSensorStarted];
}
