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
	GF_Thread *th;
	Bool do_run;
	void * device;

	Float yaw, pitch, roll;
	GF_TermEventFilter evt_filter;
	Bool reset_mx;
	Bool is_active;
} GF_PSVR;

#define PSVR_VENDOR_ID	0x054c
#define PSVR_PRODUCT_ID	0x09af

#define ACCELERATION_COEF 0.00003125f * GF_PI / 180

static s16 read_s16(unsigned char *buffer, int offset)
{
	int16_t v;
	v = buffer[offset];
	v |= buffer[offset+1] << 8;
	return v;
}

Bool psvr_on_event(void *udta, GF_Event *event, Bool consumed_by_compositor)
{
	GF_PSVR *psvr = (GF_PSVR *)udta;
	if (event->type==GF_EVENT_KEYDOWN) {
		if (event->key.key_code==GF_KEY_HOME) {
			psvr->reset_mx = GF_TRUE;
		}
	}
	else if (event->type==GF_EVENT_SENSOR_REQUEST) {
		if (event->activate_sensor.sensor_type==GF_EVENT_SENSOR_ORIENTATION) {
			psvr->is_active = event->activate_sensor.activate;
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

static u32 psvr_run(void *__psvr)
{
	GF_PSVR *psvr = (GF_PSVR *)__psvr;

	hid_init();

	psvr->device = hid_open(PSVR_VENDOR_ID, PSVR_PRODUCT_ID, 0);
	if(!psvr->device) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[PSVR] Failed to open PSVR HID device\n"));
		return 0;
	}

	psvr->evt_filter.udta = psvr;
	psvr->evt_filter.on_event = psvr_on_event;
	gf_term_add_event_filter(psvr->term, &psvr->evt_filter);

	psvr->is_active = GF_TRUE;
	psvr->do_run = GF_TRUE;
	while (psvr->do_run && psvr->device) {
		unsigned char buffer[64];
		//read device with 1 ms timeout
		int size = hid_read_timeout(psvr->device, buffer, 64, 1);

		if (size == 64) {
			GF_Event evt;
			s32 x_acc = read_s16(buffer, 20) + read_s16(buffer, 36);
			s32 y_acc = read_s16(buffer, 22) + read_s16(buffer, 38);
			s32 z_acc = read_s16(buffer, 24) + read_s16(buffer, 40);

			if (psvr->reset_mx) {
				psvr->reset_mx = 0;
				psvr->yaw = psvr->pitch = psvr->roll = 0;
			}

			psvr->roll += -y_acc * ACCELERATION_COEF;
			psvr->yaw += x_acc * ACCELERATION_COEF;
			psvr->pitch += z_acc * ACCELERATION_COEF;

			if (!psvr->is_active ) continue;

			//post our event
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_SENSOR_ORIENTATION;
			evt.sensor.x = psvr->yaw - GF_PI;
			evt.sensor.y = psvr->pitch;
			evt.sensor.z = GF_PI2 - psvr->roll;
			evt.sensor.w = 0;

			gf_term_user_event(psvr->term, &evt);
		}
	}
	if (psvr->device) {
		hid_close(psvr->device);
		psvr->device = 0;
	}
	gf_term_remove_event_filter(psvr->term, &psvr->evt_filter);
	psvr->term = NULL;
	hid_exit();
	return 0;
}

static Bool psvr_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_PSVR *psvr = termext->udta;

	switch (action) {
	case GF_TERM_EXT_START:
		psvr->term = (GF_Terminal *) param;
		opt = gf_opts_get_key("PSVR", "Enabled");
		if (!opt || strcmp(opt, "yes")) return 0;

		gf_th_run(psvr->th, psvr_run, psvr);
		return 1;

	case GF_TERM_EXT_STOP:
		psvr->do_run = GF_FALSE;
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
	psvr->th = gf_th_new("PSVR");

	return dr;
}


void psvr_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_PSVR *psvr= dr->udta;
	gf_th_del(psvr->th);
	gf_free(psvr);
	gf_free(dr);
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

GPAC_MODULE_STATIC_DECLARATION( psvr )
