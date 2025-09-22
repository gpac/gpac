//
//  ios_mpegv.m
//  ios_mpegv
//
//  Created by mac on 3/8/13.
//
//

#import "SensorAccess.h"

@implementation SensorAccess

- (id)init {
	if ((self = [super init])) {
        @autoreleasepool {
            self.motionManager = [[[CMMotionManager alloc] init] autorelease];
            self.queue = [[[NSOperationQueue alloc] init] autorelease];
            
            self.locMngr = [[[CLLocationManager alloc] init] autorelease];
            self.locMngr.delegate = self;
            self.locMngr.desiredAccuracy = kCLLocationAccuracyBest;
            self.locMngr.distanceFilter = 2;
            
            sensorCallbak = NULL;
            userData = NULL;
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

- (int) setSensorCallback:(SensorDataCallback*)callback :(void*)user
{
    sensorCallbak = callback;
    userData = user;
    return 0;
}

- (int) startSensor
{
    switch (sensorType) {
        case SENSOR_ACCELEROMETER:
            if ( ![[self motionManager] isAccelerometerAvailable] )
                return 1;
            [[self motionManager] startAccelerometerUpdatesToQueue:[self queue] withHandler:^(CMAccelerometerData* data, NSError* error) {
                    if ( sensorCallbak ) {
                        char cdata[100];
                        sprintf(cdata, "%f;%f;%f;", data.acceleration.x, data.acceleration.y, data.acceleration.z);
                        sensorCallbak(userData, cdata);
                    }
                }
             ];
            return 0;
            break;
        case SENSOR_GYROSCOPE:
            if ( ![[self motionManager] isGyroAvailable] )
                return 1;
            [[self motionManager] startGyroUpdatesToQueue:[self queue] withHandler:^(CMGyroData* data, NSError* error) {
                if ( sensorCallbak ) {
                    char cdata[100];
                    sprintf(cdata, "%f;%f;%f;", data.rotationRate.x, data.rotationRate.y, data.rotationRate.z);
                    sensorCallbak(userData, cdata);
                }
            }
             ];
            return 0;
            break;
        case SENSOR_MAGNETOMETER:
            if ( ![[self motionManager] isMagnetometerAvailable] )
                return 1;
            [[self motionManager] startMagnetometerUpdatesToQueue:[self queue] withHandler:^(CMMagnetometerData* data, NSError* error) {
                if ( sensorCallbak ) {
                    char cdata[100];
                    sprintf(cdata, "%f;%f;%f;", data.magneticField.x, data.magneticField.y, data.magneticField.z);
                    sensorCallbak(userData, cdata);
                }
            }
             ];
            return 0;
            break;
        case SENSOR_GPS:
            [self.locMngr startUpdatingLocation];
            gpsActive = 1;
            return 0;
            break;
    }

    return 1;
}

- (int) stopSensor
{
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
    char cdata[100];
    sprintf(cdata, "%f;%f;%f;%f;%f;", (float)location.coordinate.latitude, (float)location.coordinate.longitude, (float)location.altitude,
            (float)location.horizontalAccuracy, 0.f);
    sensorCallbak(userData, cdata);
}

@end

void* SENS_CreateInstance() {
    void* obj = [[SensorAccess alloc] init];
    return obj;
}

int SENS_DestroyInstance(void** inst) {
    [(id)*inst dealloc];
    *inst = NULL;
    return 0;
}

int SENS_SetSensorType(void* inst, int type) {
    if ( !inst )
        return 1;
    
    return [(id)inst setSensorType:type];
}

int SENS_GetCurrentSensorType(void* inst) {
    if ( !inst )
        return 1;
    
    return [(id)inst getSensorType];
}

int SENS_Start(void* inst) {
    if ( !inst )
        return 1;
    
    return [(id)inst startSensor];
}

int SENS_Stop(void* inst) {
    if ( !inst )
        return 1;
    
    return [(id)inst stopSensor];
}

int SENS_IsStarted(void* inst) {
    if ( !inst )
        return 1;
    
    return [(id)inst isSensorStarted];
}

int SENS_SetCallback(void* inst, SensorDataCallback *callback, void* user) {
    if ( !inst )
        return 1;
    
    return [(id)inst setSensorCallback:callback :user];
}
