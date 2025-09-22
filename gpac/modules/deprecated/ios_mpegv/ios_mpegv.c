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

/*driver interfaces*/

#include <gpac/list.h>
#include <gpac/constants.h>

#include <gpac/setup.h>

#include <gpac/modules/codec.h>
#include <gpac/scenegraph_vrml.h>

#include <gpac/thread.h>

#include "sensor_wrap.h"

typedef struct
{
	char sensor[50];
	u16 sensorIOSType;
	u8 isAttached;

	GF_Mutex* mx;

	void* inst;

} MPEGVSensorContext;

#define MPEGVSCTX	MPEGVSensorContext *rc = (MPEGVSensorContext *)dr->udta

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
		//AddField(dr, GF_SG_VRML_SFVEC3F, "Orientation");

		//rc->sensorIOSType = 3;

		return 0;
	}
	else if ( !strcmp(rc->sensor, "AccelerationSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "Acceleration");

		rc->sensorIOSType = SENSOR_ACCELEROMETER;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "AngularVelocitySensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "AngularVelocity");

		rc->sensorIOSType = SENSOR_GYROSCOPE;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "MagneticFieldSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "MagneticField");

		rc->sensorIOSType = SENSOR_MAGNETOMETER;

		return 1;
	}
	else if ( !strcmp(rc->sensor, "LightSensorType") )
	{
		//AddField(dr, GF_SG_VRML_SFFLOAT, "Light");

		//rc->sensorIOSType = 5;

		return 0;
	}
	else if ( !strcmp(rc->sensor, "AtmosphericPressureSensorType") )
	{
		//AddField(dr, GF_SG_VRML_SFFLOAT, "AtmosphericPressure");

		//rc->sensorIOSType = 6;

		return 0;
	}
	else if ( !strcmp(rc->sensor, "RotationVectorSensorType") )
	{
		//AddField(dr, GF_SG_VRML_SFVEC3F, "RotationVector");

		//rc->sensorIOSType = 11;

		return 0;
	}
	else if ( !strcmp(rc->sensor, "GlobalPositionSensorType") )
	{
		AddField(dr, GF_SG_VRML_SFVEC3F, "Position");
		AddField(dr, GF_SG_VRML_SFFLOAT, "Accuracy");
		AddField(dr, GF_SG_VRML_SFFLOAT, "Bearing");

		rc->sensorIOSType = SENSOR_GPS;

		return 1;
	}
	else
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[MPEG-V_IN] Unsupported sensor type: %s\n", rc->sensor));
		return 0;
	}
}

void MPEGVSensorCallback( void* ptr, const char* data)
{
	GF_BitStream *bs;
	char *buf;
	u32 buf_size;
	float x, y, z, a, b;
	struct __input_device * dr = (struct __input_device *)ptr;
	MPEGVSCTX;

	if (!gf_mx_try_lock(rc->mx))
		return;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if ( rc->sensorIOSType == SENSOR_ACCELEROMETER
	        || rc->sensorIOSType == SENSOR_GYROSCOPE
	        //|| rc->sensorIOSType == 3
	        || rc->sensorIOSType == SENSOR_MAGNETOMETER )
	{
		sscanf(data, "%f;%f;%f;", &x, &y, &z);
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
		gf_bs_write_float(bs, y);
		gf_bs_write_float(bs, z);
	}
	else if ( rc->sensorIOSType == 5
	          || rc->sensorIOSType == 6 )
	{
		sscanf(data, "%f;", &x);

		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
	}
	else if ( rc->sensorIOSType == 11 )
	{
		sscanf(data, "%f;%f;%f;", &x, &y, &z);

		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_float(bs, x);
		gf_bs_write_float(bs, y);
		gf_bs_write_float(bs, z);
		/*gf_bs_write_float(bs, q);*/
	}
	else if ( rc->sensorIOSType == SENSOR_GPS )
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

	gf_mx_v(rc->mx);
}

void MPEGVS_Start(struct __input_device * dr)
{
	MPEGVSCTX;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] Start: %d\n", gf_th_id()));

	if ( rc->inst ) {
		SENS_Stop(rc->inst);
		SENS_DestroyInstance(&rc->inst);
	}

	rc->inst = SENS_CreateInstance();
	SENS_SetSensorType(rc->inst, rc->sensorIOSType);
	SENS_SetCallback(rc->inst, MPEGVSensorCallback, dr);
	SENS_Start(rc->inst);
}

void MPEGVS_Stop(struct __input_device * dr)
{
	MPEGVSCTX;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[MPEG-V_IN] Stop: %d\n", gf_th_id()));

	SENS_Stop(rc->inst);
	SENS_DestroyInstance(&rc->inst);
}

GF_InputSensorDevice* NewMPEGVSInputSesor()
{
	MPEGVSensorContext* ctx;
	GF_InputSensorDevice* driv;

	driv = (GF_InputSensorDevice *) gf_malloc(sizeof(GF_InputSensorDevice));
	memset(driv, 0, sizeof(GF_InputSensorDevice));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_INPUT_DEVICE_INTERFACE, "MPEG-V Sensors Input Module", "gpac distribution");

	driv->RegisterDevice = MPEGVS_RegisterDevice;
	driv->Start = MPEGVS_Start;
	driv->Stop = MPEGVS_Stop;

	ctx = (MPEGVSensorContext*) gf_malloc (sizeof(MPEGVSensorContext));
	memset(ctx, 0, sizeof(MPEGVSensorContext));
	ctx->mx = gf_mx_new(NULL);

	driv->udta = (void*)ctx;

	return driv;
}

void DeleteMPEGVSInputSensor(GF_InputSensorDevice* dev)
{
	MPEGVS_Stop(dev);
	MPEGVSensorContext* ctx = (MPEGVSensorContext*)dev->udta;
	gf_mx_del(ctx->mx);
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

GPAC_MODULE_STATIC_DECLARATION( ios_mpegv )
