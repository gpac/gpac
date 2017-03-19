
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
