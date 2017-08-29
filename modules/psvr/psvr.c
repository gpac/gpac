/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2011-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / PSVR input sub-project
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

#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/nodes_mpeg4.h>

#include <hidapi/hidapi.h>

typedef struct
{
	GF_ObjectManager *odm;
	GF_Terminal *term;

	void * device;

	Float yaw, pitch, roll;
} GF_PSVR;

#define PSVR_VENDOR_ID	0x054c
#define PSVR_PRODUCT_ID	0x09af

#define ACCELERATION_COEF 0.00003125f

static s16 read_s16(unsigned char *buffer, int offset)
{
	int16_t v;
	v = buffer[offset];
	v |= buffer[offset+1] << 8;
	return v;
}
static Bool psvr_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_PSVR *psvr = termext->udta;

	switch (action) {
	case GF_TERM_EXT_START:
		psvr->term = (GF_Terminal *) param;
		opt = gf_modules_get_option((GF_BaseInterface*)termext, "PSVR", "Enabled");
		if (!opt || strcmp(opt, "yes")) return 0;

		psvr->device = hid_open(PSVR_VENDOR_ID, PSVR_PRODUCT_ID, 0);
		if(!psvr->device) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PSVR] Failed to open PSVR HID device\n"));
			return 0;
		}

		/*we are not threaded*/
		termext->caps |= GF_TERM_EXTENSION_NOT_THREADED;


		return 1;

	case GF_TERM_EXT_STOP:
		if (psvr->device) {
			hid_close(psvr->device);
			psvr->device = 0;
		}
		psvr->term = NULL;
		break;

	case GF_TERM_EXT_PROCESS:
		if (psvr->device) {
			unsigned char buffer[64];
			//read device with 1 ms timeout
			int size = hid_read_timeout(psvr->device, buffer, 64, 1);

			if(size == 64) {
				GF_Event evt;
				s32 x_acc = read_s16(buffer, 20) + read_s16(buffer, 36);
				s32 y_acc = read_s16(buffer, 22) + read_s16(buffer, 38);
				s32 z_acc = read_s16(buffer, 24) + read_s16(buffer, 40);

				psvr->yaw += -y_acc * ACCELERATION_COEF;
				psvr->pitch += -x_acc * ACCELERATION_COEF;
				psvr->roll += z_acc * ACCELERATION_COEF;

				//post our event
				memset(&evt, 0, sizeof(GF_Event));
				evt.type = GF_EVENT_SENSOR_ORIENTATION;
				evt.sensor.x = psvr->pitch;
				evt.sensor.y = psvr->yaw;
				evt.sensor.z = psvr->roll;
				evt.sensor.w = 0;
				gf_term_user_event(psvr->term, &evt);

			}
		}

		break;
	}
	return 0;
}


GF_TermExt *psvr_new()
{
	GF_TermExt *dr;
	GF_PSVR *psvr;
	dr = (GF_TermExt*)gf_malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "PSVR Input", "gpac distribution");

	GF_SAFEALLOC(psvr, GF_PSVR);
	dr->process = psvr_process;
	dr->udta = psvr;

	hid_init();

	return dr;
}


void psvr_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_PSVR *psvr= dr->udta;
	gf_free(psvr);
	gf_free(dr);

	hid_exit();
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_TERM_EXT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)psvr_new();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		psvr_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( osd )
