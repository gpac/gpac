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
 
#ifndef _SENSORS_DEF_H_
#define _SENSORS_DEF_H_

#ifdef __cplusplus
extern "C" {
#endif

//accelerometer
#define SENSOR_ACCELEROMETER    0
//gyro
#define SENSOR_GYROSCOPE        1
//yaw-pitch-roll
#define SENSOR_ORIENTATION       2
//magnetometer
#define SENSOR_MAGNETOMETER     3
//GPS
#define SENSOR_GPS              4

typedef void (SensorDataCallback)(int sensor_type, float x, float y, float z, float w);

void *sensor_create(int type, SensorDataCallback *callback);
int sensor_destroy(void** inst);
int sensor_get_type(void* inst);
int sensor_start(void* inst);
int sensor_stop(void* inst);
int sensor_is_running(void* inst);

#ifdef __cplusplus
}
#endif

#endif //_SENSORS_DEF_H_
