//
//  sensor_wrap.h
//  gpac4ios
//
//  Created by mac on 3/8/13.
//
//

#ifndef sensor_wrap_h
#define sensor_wrap_h

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_ACCELEROMETER    0
#define SENSOR_GYROSCOPE        1
#define SENSOR_MAGNETOMETER     2
#define SENSOR_GPS              3

typedef void (SensorDataCallback)(void* user, const char* data);

void* SENS_CreateInstance();

int SENS_DestroyInstance(void** inst);

int SENS_SetSensorType(void* inst, int type);

int SENS_GetCurrentSensorType(void* inst);

int SENS_Start(void* inst);

int SENS_Stop(void* inst);

int SENS_IsStarted(void* inst);

int SENS_SetCallback(void* inst, SensorDataCallback *callback, void* user);

#ifdef __cplusplus
}
#endif

#endif
