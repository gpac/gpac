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
#include <gpac/thread.h>

#include <wiiuse.h>

typedef struct
{
	u32 nb_wiimotes;
	wiimote** wiimotes;
	Bool running;
	u32 prev_id;

	GF_Thread *th;
} GF_WiiMote;

static Bool WII_RegisterDevice(struct __input_device *ifce, const char *urn, GF_BitStream *dsi, void (*AddField)(struct __input_device *_this, u32 fieldType, const char *name))
{
	const char *opt;
	GF_WiiMote *wii = (GF_WiiMote *)ifce->udta;
	if (strcmp(urn, "WiiMote")) return 0;

	/*init wiiuse lib*/
	opt = gf_modules_get_option((GF_BaseInterface*)ifce, "WII", "MaxWiimotes");
	if (opt) wii->nb_wiimotes = atoi(opt);
	if (!wii->nb_wiimotes) wii->nb_wiimotes = 1;

	wii->wiimotes = wiiuse_init(wii->nb_wiimotes);
	if (!wii->wiimotes) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[Wii] Cannot initialize wiiuse library\n"));
		return 0;
	}

	/*declare the interface*/
	AddField(ifce, GF_SG_VRML_SFINT32, "uid");
	AddField(ifce, GF_SG_VRML_SFBOOL, "1");
	AddField(ifce, GF_SG_VRML_SFBOOL, "2");
	AddField(ifce, GF_SG_VRML_SFBOOL, "A");
	AddField(ifce, GF_SG_VRML_SFBOOL, "B");
	AddField(ifce, GF_SG_VRML_SFBOOL, "-");
	AddField(ifce, GF_SG_VRML_SFBOOL, "home");
	AddField(ifce, GF_SG_VRML_SFBOOL, "+");
	AddField(ifce, GF_SG_VRML_SFBOOL, "left");
	AddField(ifce, GF_SG_VRML_SFBOOL, "right");
	AddField(ifce, GF_SG_VRML_SFBOOL, "down");
	AddField(ifce, GF_SG_VRML_SFBOOL, "up");
	AddField(ifce, GF_SG_VRML_SFVEC3F, "ypr");
	AddField(ifce, GF_SG_VRML_SFVEC3F, "gravity");

	return 1;
}

#define WRITE_BUTTON(_b)	\
	if (IS_JUST_PRESSED(wm, _b)) {	gf_bs_write_int(bs, 1, 1); gf_bs_write_int(bs, 1, 1); }	\
	else if (IS_RELEASED(wm, _b)) {	gf_bs_write_int(bs, 1, 1); gf_bs_write_int(bs, 0, 1); }	\
	else gf_bs_write_int(bs, 0, 1); \
 

#define WII_PI	3.1415926535898f

static u32 WII_Run(void *par)
{
	GF_BitStream *bs;
	char *buf;
	u32 i, buf_size, count, scan_delay;
	const char *opt;

	GF_InputSensorDevice *ifce = (GF_InputSensorDevice *)par;
	GF_WiiMote *wii = (GF_WiiMote *)ifce->udta;


	scan_delay = 5;
	opt = gf_modules_get_option((GF_BaseInterface*)ifce, "WII", "ScanDelay");
	if (opt) scan_delay = atoi(opt);

	/*locate the wiimotes*/
	count = wiiuse_find(wii->wiimotes, wii->nb_wiimotes, scan_delay);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[Wii] Found %d wiimotes\n", count));
	count = wiiuse_connect(wii->wiimotes, wii->nb_wiimotes);
	if (count) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[Wii] Connected to %d connected wiimotes\n", count));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[Wii] Failed to connect to any wiimote\n"));
	}

	opt = gf_modules_get_option((GF_BaseInterface*)ifce, "WII", "MotionSensing");
	/*enable motion sensing*/
	if (!opt || !strcmp(opt, "yes")) {
		Float smooth_alpha = 0.5;
		Float ori_threshold = 10.0;
		opt = gf_modules_get_option((GF_BaseInterface*)ifce, "WII", "OrientationThreshold");
		if (opt) ori_threshold = (Float) atof(opt);
		opt = gf_modules_get_option((GF_BaseInterface*)ifce, "WII", "SmoothAlpha");
		if (opt) {
			smooth_alpha = (Float) atof(opt);
			if (smooth_alpha<0) smooth_alpha = 0.5;
			else if (smooth_alpha>1.0) smooth_alpha=0.5;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[Wii] Enabling motion sensing - Alpha smoothing %f - Orientation Threshold %f\n", smooth_alpha, ori_threshold));

		for (i=0; i<count; i++) {
			wiiuse_motion_sensing(wii->wiimotes[i], 1);
			wiiuse_set_smooth_alpha(wii->wiimotes[i],smooth_alpha);
			wiiuse_set_orient_threshold(wii->wiimotes[i], ori_threshold);
		}
	}

	while (wii->running) {
		count = wiiuse_poll(wii->wiimotes, wii->nb_wiimotes);
		if (!count) {
			continue;
		}
		for (i=0; i<count; i++) {
			struct wiimote_t* wm = wii->wiimotes[i];
			switch (wm->event) {
			case WIIUSE_EVENT:/* A generic event occured on the wiimote. */
				/*create the data frame*/
				bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				/*if not the same wiimote write the UID*/
				if (wii->prev_id != wm->unid) {
					gf_bs_write_int(bs, 1, 1);
					gf_bs_write_int(bs,  wm->unid, 32);
					wii->prev_id = wm->unid;
				} else {
					gf_bs_write_int(bs, 0, 1);
				}

				/*write buttons state*/
				WRITE_BUTTON(WIIMOTE_BUTTON_ONE);
				WRITE_BUTTON(WIIMOTE_BUTTON_TWO);
				WRITE_BUTTON(WIIMOTE_BUTTON_A);
				WRITE_BUTTON(WIIMOTE_BUTTON_B);
				WRITE_BUTTON(WIIMOTE_BUTTON_MINUS);
				WRITE_BUTTON(WIIMOTE_BUTTON_HOME);
				WRITE_BUTTON(WIIMOTE_BUTTON_PLUS);
				WRITE_BUTTON(WIIMOTE_BUTTON_LEFT);
				WRITE_BUTTON(WIIMOTE_BUTTON_RIGHT);
				WRITE_BUTTON(WIIMOTE_BUTTON_DOWN);
				WRITE_BUTTON(WIIMOTE_BUTTON_UP);

				/*write yaw-pitch-roll - FIXME: wiiuse seems to output NaN in these values upon init*/
				gf_bs_write_int(bs, 1, 1);
				gf_bs_write_float(bs, WII_PI * wm->orient.yaw / 24 );
				gf_bs_write_float(bs, WII_PI * wm->orient.pitch / 180);
				gf_bs_write_float(bs, WII_PI * wm->orient.roll / 180);

				/*write gravity - FIXME: wiiuse seems to output NaN in these values upon init*/
				gf_bs_write_int(bs, 1, 1);
				gf_bs_write_float(bs, wm->gforce.x);
				gf_bs_write_float(bs, wm->gforce.y);
				gf_bs_write_float(bs, wm->gforce.z);

				gf_bs_align(bs);
				gf_bs_get_content(bs, &buf, &buf_size);
				gf_bs_del(bs);

				ifce->DispatchFrame(ifce, buf, buf_size);
				gf_free(buf);
				break;
			case WIIUSE_STATUS: /*A status report was obtained from the wiimote. */
				break;
			case WIIUSE_DISCONNECT:/*The wiimote disconnected. */
				break;
			case WIIUSE_READ_DATA:/* Data was returned that was previously requested from the wiimote ROM/registers. */
				break;
			case WIIUSE_NUNCHUK_INSERTED:
			case WIIUSE_NUNCHUK_REMOVED:
			case  WIIUSE_CLASSIC_CTRL_INSERTED:
			case WIIUSE_CLASSIC_CTRL_REMOVED:
			case WIIUSE_GUITAR_HERO_3_CTRL_INSERTED:
			case WIIUSE_GUITAR_HERO_3_CTRL_REMOVED:
				break;
			}
		}
	}
	return 0;
}

static void WII_Start(struct __input_device *ifce)
{
	GF_WiiMote *wii = (GF_WiiMote *)ifce->udta;
	wii->running = 1;
	gf_th_run(wii->th, WII_Run, ifce);
}

static void WII_Stop(struct __input_device *ifce)
{
	GF_WiiMote *wii = (GF_WiiMote *)ifce->udta;
	wii->running = 0;
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
	GF_WiiMote *wii;
	GF_InputSensorDevice *plug;
	if (InterfaceType != GF_INPUT_DEVICE_INTERFACE) return NULL;

	GF_SAFEALLOC(plug, GF_InputSensorDevice);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_INPUT_DEVICE_INTERFACE, "GPAC Wiimote InputSensor", "gpac distribution")

	plug->RegisterDevice = WII_RegisterDevice;
	plug->Start = WII_Start;
	plug->Stop = WII_Stop;

	GF_SAFEALLOC(wii, GF_WiiMote);
	plug->udta = wii;
	wii->th = gf_th_new("WiiMote");
	return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	GF_WiiMote *wii;
	GF_InputSensorDevice *ifce = (GF_InputSensorDevice*)bi;
	if (ifce->InterfaceType!=GF_INPUT_DEVICE_INTERFACE) return;

	wii = ifce->udta;
	if (wii->wiimotes) {
		wiiuse_cleanup(wii->wiimotes, wii->nb_wiimotes);
	}
	gf_free(wii);
	gf_free(bi);
}

GPAC_MODULE_STATIC_DECLARATION( wiiis )
