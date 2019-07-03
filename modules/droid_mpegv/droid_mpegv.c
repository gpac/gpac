/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-V Input sensor for android
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

#include <jni.h>
#include <android/log.h>

/*driver interfaces*/

#include <gpac/list.h>
#include <gpac/constants.h>

#include <gpac/setup.h>

#include <gpac/modules/codec.h>
#include <gpac/scenegraph_vrml.h>

#include <gpac/thread.h>

#define LOG_TAG "MPEG-V_IN"
#ifdef ANDROID
#  define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#  define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#  define QUOTEME_(x) #x
#  define QUOTEME(x) QUOTEME_(x)
#  define LOGI(...) fprintf(stderr, "I/" LOG_TAG " (" __FILE__ ":" QUOTEME(__LINE__) "): " __VA_ARGS__)
#  define LOGE(...) fprintf(stderr, "E/" LOG_TAG "(" ")" __VA_ARGS__)
#endif

JNIEXPORT void JNICALL Java_com_gpac_Osmo4_MPEGVSensor_sendData( JNIEnv* env, jobject thiz, jint ptr, jstring data);

static JavaVM* javaVM = 0;
static jclass sensCtrlClass;
static jmethodID cid;
static jmethodID startSensor;
static jmethodID stopSensor;

#ifndef GPAC_STATIC_MODULES

//----------------------------------------------------------------------
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env = 0;
	javaVM = vm;

	if ( (*javaVM)->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2) != JNI_OK )
		return -1;

	// Get the class and its methods in the main env

	// Get the CameraController class
	// This is just a local refenrece. Cannot be used in the other JNI calls.
	sensCtrlClass = (*env)->FindClass(env, "com/gpac/Osmo4/MPEGVSensor");
	if (sensCtrlClass == 0)
	{
		LOGE("[MPEG-V_IN] Class MPEGVSensor not found\n");
		return -1;
	}

	// Get Global Reference to be able to use the class
	sensCtrlClass = (*env)->NewGlobalRef(env, sensCtrlClass);
	if ( sensCtrlClass == 0 )
	{
		LOGE("[MPEG-V_IN] Caanot create Global Reference\n");
		return -1;
	}

	// Get the method ID for the CameraController constructor.
	cid = (*env)->GetMethodID(env, sensCtrlClass, "<init>", "()V");
	if (cid == 0)
	{
		LOGE("[MPEG-V_IN] MPEGVSensor Constructor not found\n");
		return -1;
	}

	// Get startCamera() method from class CameraController
	startSensor = (*env)->GetMethodID(env, sensCtrlClass, "startSensor", "(II)V");
	if (startSensor == 0)
	{
		LOGE("[MPEG-V_IN] Function startSensor not found\n");
		return -1;
	}

	stopSensor = (*env)->GetMethodID(env, sensCtrlClass, "stopSensor", "()V");
	if (stopSensor == 0)
	{
		LOGE("[MPEG-V_IN] Function stopSensor not found\n");
		return -1;
	}

	return JNI_VERSION_1_2;
}

void JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv* env = 0;

	if ( (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_2) != JNI_OK )
		return;

	(*env)->DeleteGlobalRef(env, sensCtrlClass);
}

//----------------------------------------------------------------------
JavaVM* GetJavaVM()
{
	return javaVM;
}
//----------------------------------------------------------------------
#else
JavaVM* GetJavaVM();
JNIEnv* GetEnv();

#endif

typedef struct
{
	char sensor[50];
	u16 sensorAndroidType;
	u8 isAttached;

	GF_Thread *trd;
	u8 stop;

	JNIEnv* env;
	jclass sensCtrlClass;
	jmethodID cid;
	jobject sensCtrlObj;
	jmethodID startSensor;
	jmethodID stopSensor;
} MPEGVSensorContext;

#define MPEGVSCTX	MPEGVSensorContext *rc = (MPEGVSensorContext *)dr->udta

void unloadSensorController(MPEGVSensorContext *rc)
{
	if ( rc->isAttached )
	{
		(*rc->env)->PopLocalFrame(rc->env, NULL);
		(*GetJavaVM())->DetachCurrentThread(GetJavaVM());
		rc->isAttached = 0;
	}

	rc->env = NULL;
}

void loadSensorControler(MPEGVSensorContext *rc)
{
	JNIEnv* env = NULL;
	jint res = 0;

	// Here we can be in a thread

	res = (*GetJavaVM())->GetEnv(javaVM, (void**)&env, JNI_VERSION_1_2);
	if ( res == JNI_EDETACHED )
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] The current thread is not attached to the VM, assuming native thread\n"));
		res = (*GetJavaVM())->AttachCurrentThread(GetJavaVM(), &env, NULL);
		if ( res )
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] Attach current thread failed: %d\n", res));
			return;
		}
		rc->isAttached = 1;
		(*env)->PushLocalFrame(env, 2);
	}
	else if ( res == JNI_EVERSION )
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] The specified version is not supported\n"));
		return;
	}

	rc->env = env;
	rc->sensCtrlClass = sensCtrlClass;
	rc->cid = cid;
	rc->startSensor = startSensor;
	rc->stopSensor = stopSensor;

	// Create the sensor object in the thread
	rc->sensCtrlObj = (*rc->env)->NewObject(rc->env, rc->sensCtrlClass, rc->cid);
	if (rc->sensCtrlObj == 0)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] Cannot create MPEGVSensor object\n"));
		return;
	}
}

Bool MPEGVS_RegisterDevice(struct __input_device *dr, const char *urn, const char *dsi, u32 dsi_size, void (*AddField)(struct __input_device *_this, u32 fieldType, const char *name))
{
	MPEGVSCTX;

	//"MPEG-V:siv:OrientationSensorType"

	if ( strnicmp(urn, "MPEG-V", 6) )
		return 0;

	if ( strlen(urn) <= 6 )
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[MPEG-V] No sensor type specified\n"));
		return 0;
	}

	if ( strnicmp(urn+6, ":siv:", 5) )
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[MPEG-V] Not valid sensor type specified\n"));
		return 0;
	}

	strcpy(rc->sensor, urn+11);

	if ( !strcmp(rc->sensor, "OrientationSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "Orientation");

		rc->sensorAndroidType = 3;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "AccelerationSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "Acceleration");

		rc->sensorAndroidType = 1;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "AngularVelocitySensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "AngularVelocity");

		rc->sensorAndroidType = 4;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "MagneticFieldSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "MagneticField");

		rc->sensorAndroidType = 2;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "LightSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFFLOAT, "Light");

		rc->sensorAndroidType = 5;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "AtmosphericPressureSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFFLOAT, "AtmosphericPressure");

		rc->sensorAndroidType = 6;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "RotationVectorSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "RotationVector");

		rc->sensorAndroidType = 11;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "GlobalPositionSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "Position");
		AddField(dr, GF_SG_VRML_SFFLOAT, "Accuracy");
		AddField(dr, GF_SG_VRML_SFFLOAT, "Bearing");

		rc->sensorAndroidType = 100;

		return 1;
	}
	else
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[MPEG-V_IN] Unsupported sensor type: %s\n", rc->sensor));
		return 0;
	}
}

u32 MPEGVS_OnData(struct __input_device * dr, const char* data)
{
	GF_BitStream *bs;
	u8 *buf;
	u32 buf_size;
	float x, y, z, q, a, b;
	MPEGVSCTX;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if ( rc->sensorAndroidType == 1
	        || rc->sensorAndroidType == 2
	        || rc->sensorAndroidType == 3
	        || rc->sensorAndroidType == 4 )
	{
		sscanf(data, "%f;%f;%f;", &x, &y, &z);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
		gf_bs_write_float(bs, y);
		gf_bs_write_float(bs, z);
	}
	else if ( rc->sensorAndroidType == 5
	          || rc->sensorAndroidType == 6 )
	{
		sscanf(data, "%f;", &x);

		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
	}
	else if ( rc->sensorAndroidType == 11 )
	{
		sscanf(data, "%f;%f;%f;", &x, &y, &z);

		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
		gf_bs_write_float(bs, y);
		gf_bs_write_float(bs, z);
		/*gf_bs_write_float(bs, q);*/
	}
	else if ( rc->sensorAndroidType == 100 )
	{
		sscanf(data, "%f;%f;%f;%f;%f;", &x, &y, &z, &a, &b);

		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
		gf_bs_write_float(bs, y);
		gf_bs_write_float(bs, z);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, a);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, b);
		/*gf_bs_write_float(bs, q);*/
	}

	gf_bs_align(bs);
	gf_bs_get_content(bs, &buf, &buf_size);
	gf_bs_del(bs);

	dr->DispatchFrame(dr, (u8*)buf, buf_size);
	gf_free(buf);

	return GF_OK;
}

JNIEXPORT void Java_com_gpac_Osmo4_MPEGVSensor_sendData( JNIEnv* env, jobject thiz, jint ptr, jstring data)
{
	jboolean isCopy;
	const char * cData = (*env)->GetStringUTFChars(env, data, &isCopy);

	MPEGVS_OnData((struct __input_device *)ptr, cData);

	(*env)->ReleaseStringUTFChars(env, data, cData);
}

u32 ThreadRun(void* param)
{
	struct __input_device * dr = (struct __input_device *)param;
	MPEGVSCTX;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] Start: %d\n", gf_th_id()));

	loadSensorControler(rc);

	if (!rc->env || !rc->sensCtrlObj)
		return GF_OK;

	(*rc->env)->CallNonvirtualVoidMethod(rc->env, rc->sensCtrlObj, rc->sensCtrlClass, rc->startSensor, (s32)dr, rc->sensorAndroidType);

	while (!rc->stop)
		gf_sleep(10);

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] Stop: %d\n", gf_th_id()));

	if (!rc->env)
		return GF_OK;

	if ( rc->sensCtrlObj )
	{
		(*rc->env)->CallNonvirtualVoidMethod(rc->env, rc->sensCtrlObj, rc->sensCtrlClass, rc->stopSensor);

		(*rc->env)->DeleteLocalRef( rc->env, rc->sensCtrlObj );
	}

	unloadSensorController(rc);
	return GF_OK;
}

void MPEGVS_Start(struct __input_device * dr)
{
	MPEGVSCTX;

	rc->trd = gf_th_new("MPEG-V_IN");
	gf_th_run(rc->trd, ThreadRun, dr);
}

void MPEGVS_Stop(struct __input_device * dr)
{
	MPEGVSCTX;

	if ( rc->trd )
	{
		rc->stop = 1;
		while ( gf_th_status(rc->trd) == GF_THREAD_STATUS_RUN )
			gf_sleep(5);

		gf_th_del(rc->trd);
		rc->trd = NULL;
		rc->stop = 0;
	}
}

GF_InputSensorDevice* NewMPEGVSInputSesor()
{
	MPEGVSensorContext* ctx = NULL;
	GF_InputSensorDevice* driv = NULL;

	driv = (GF_InputSensorDevice *) gf_malloc(sizeof(GF_InputSensorDevice));
	memset(driv, 0, sizeof(GF_InputSensorDevice));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_INPUT_DEVICE_INTERFACE, "MPEG-V Sensors Input Module", "gpac distribution");

	driv->RegisterDevice = MPEGVS_RegisterDevice;
	driv->Start = MPEGVS_Start;
	driv->Stop = MPEGVS_Stop;

	ctx = (MPEGVSensorContext*) gf_malloc (sizeof(MPEGVSensorContext));
	memset(ctx, 0, sizeof(MPEGVSensorContext));

	driv->udta = (void*)ctx;

	return driv;
}

void DeleteMPEGVSInputSensor(GF_InputSensorDevice* dev)
{
	MPEGVS_Stop(dev);
	gf_free(dev->udta);
	gf_free(dev);
}

/*interface query*/
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_INPUT_DEVICE_INTERFACE,
		0
	};
	return si;
}

/*interface create*/
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_INPUT_DEVICE_INTERFACE) return (GF_BaseInterface *) NewMPEGVSInputSesor();
	return NULL;
}

/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_INPUT_DEVICE_INTERFACE:
		DeleteMPEGVSInputSensor((GF_InputSensorDevice *)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( droid_mpegv )
