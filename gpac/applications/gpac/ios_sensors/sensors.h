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
 
#import <Foundation/Foundation.h>
#import <CoreMotion/CoreMotion.h>
#import <CoreLocation/CoreLocation.h>

#include "sensors_def.h"

@interface SensorAccess : NSObject <CLLocationManagerDelegate>
{
	SensorDataCallback* sensorCallbak;
	int sensorType;
	int gpsActive;
	int sensorActive;
}

@property (nonatomic, retain) CMMotionManager *motionManager;
@property (nonatomic, retain) NSOperationQueue *queue;
@property (nonatomic, retain) CLLocationManager *locMngr;

- (int) setSensorType :(int) type;
- (int) getSensorType;
- (int) setSensorCallback:(SensorDataCallback*)callback ;
- (int) startSensor;
- (int) stopSensor;
- (int) isSensorStarted;

@end
