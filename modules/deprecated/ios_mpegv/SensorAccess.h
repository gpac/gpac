//
//  ios_mpegv.h
//  ios_mpegv
//
//  Created by mac on 3/8/13.
//
//

#import <Foundation/Foundation.h>
#import <CoreMotion/CoreMotion.h>
#import <CoreLocation/CoreLocation.h>

#include "sensor_wrap.h"

@interface SensorAccess : NSObject <CLLocationManagerDelegate>
{
	SensorDataCallback* sensorCallbak;
	void* userData;
	int sensorType;
	int gpsActive;
}

@property (nonatomic, retain) CMMotionManager *motionManager;
@property (nonatomic, retain) NSOperationQueue *queue;
@property (nonatomic, retain) CLLocationManager *locMngr;

- (int) setSensorType :(int) type;
- (int) getSensorType;
- (int) setSensorCallback:(SensorDataCallback*)callback :(void*) user;
- (int) startSensor;
- (int) stopSensor;
- (int) isSensorStarted;

@end
