/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2009-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Dummy input module
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


#include <gpac/modules/codec.h>
#include <gpac/scenegraph_vrml.h>

static Bool DEV_RegisterDevice(struct __input_device *ifce, const char *urn, const char *dsi, u32 dsi_size, void (*AddField)(struct __input_device *_this, u32 fieldType, const char *name))
{
	if (strcmp(urn, "DemoSensor")) return 0;

	AddField(ifce, GF_SG_VRML_SFSTRING, "content");

	return 1;
}

static void DEV_Start(struct __input_device *ifce)
{
	GF_BitStream *bs;
	char *buf, *szWord;
	u32 len, val, i, buf_size;

	szWord = "Hello InputSensor!";

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*HTK sensor buffer format: SFString - SFInt32 - SFFloat*/
	gf_bs_write_int(bs, 1, 1);
	len = strlen(szWord);
	val = gf_get_bit_size(len);
	gf_bs_write_int(bs, val, 5);
	gf_bs_write_int(bs, len, val);
	for (i=0; i<len; i++) gf_bs_write_int(bs, szWord[i], 8);

	gf_bs_align(bs);
	gf_bs_get_content(bs, &buf, &buf_size);
	gf_bs_del(bs);

	ifce->DispatchFrame(ifce, buf, buf_size);
	gf_free(buf);
}

static void DEV_Stop(struct __input_device *ifce)
{
}


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_INPUT_DEVICE_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	GF_InputSensorDevice *plug;
	if (InterfaceType != GF_INPUT_DEVICE_INTERFACE) return NULL;

	GF_SAFEALLOC(plug, GF_InputSensorDevice);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_INPUT_DEVICE_INTERFACE, "GPAC Demo InputSensor", "gpac distribution")

	plug->RegisterDevice = DEV_RegisterDevice;
	plug->Start = DEV_Start;
	plug->Stop = DEV_Stop;

	return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	GF_InputSensorDevice *ifcn = (GF_InputSensorDevice*)bi;
	if (ifcn->InterfaceType==GF_INPUT_DEVICE_INTERFACE) {
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DECLARATION( demo_is )
