/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Romain Bouqueau, Jean Le Feuvre
*			Copyright (c) 2014-2022 GPAC Licensing
*			Copyright (c) 2016-2020 Telecom Paris
*					All rights reserved
*
*  This file is part of GPAC / Dektec SDI video output filter
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

#include "dektec_video.h"


#ifdef GPAC_HAS_DTAPI
#define OFFS(_n)	#_n, offsetof(GF_DTOutCtx, _n)
#else
#define OFFS(_n)	#_n, -1

static GF_Err dtout_config_dummy(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	return GF_OK;
}
static GF_Err dtout_process_dummy(GF_Filter *filter)
{
	return GF_OK;
}
#endif

static const GF_FilterArgs DTOutArgs[] =
{
	{ OFFS(bus), "PCI bus number. If not set, device discovery is used", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(slot), "PCI bus number. If not set, device discovery is used", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT },
	{ OFFS(fps), "default FPS to use if input stream fps cannot be detected", GF_PROP_FRACTION, "30/1", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(clip), "clip YUV data to valid SDI range, slower", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(port), "set sdi output port of card", GF_PROP_UINT, "1", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(start), "set playback start offset, [-1, 0] means percent of media dur, e.g. -1 == dur", GF_PROP_DOUBLE, "0.0", NULL, GF_FS_ARG_HINT_NORMAL },
	{ 0 }
};

static GF_FilterCapability DTOutCaps[3];

#include <gpac/module.h>
GF_FilterRegister DTOutRegister;

#ifdef GPAC_HAS_DTAPI
GPAC_MODULE_EXPORT
GF_FilterRegister *RegisterFilter(GF_FilterSession *session)
#else
GF_FilterRegister *dtout_register(GF_FilterSession *session)
#endif
{
	memset(DTOutCaps, 0, sizeof(DTOutCaps));
	memset(&DTOutRegister, 0, sizeof(GF_FilterRegister));

#ifndef GPAC_HAS_DTAPI
	if (!gf_opts_get_bool("temp", "gendoc"))
		return NULL;
	DTOutRegister.version = "! Warning: DekTek SDK NOT AVAILABLE IN THIS BUILD !";
#endif

	DTOutCaps[0].code = GF_PROP_PID_STREAM_TYPE;
	DTOutCaps[0].flags = GF_CAPS_INPUT;
	DTOutCaps[0].val.type = GF_PROP_UINT;
	DTOutCaps[0].val.value.uint = GF_STREAM_VISUAL;

	DTOutCaps[1].code = GF_PROP_PID_STREAM_TYPE;
	DTOutCaps[1].flags = GF_CAPS_INPUT;
	DTOutCaps[1].val.type = GF_PROP_UINT;
	DTOutCaps[1].val.value.uint = GF_STREAM_AUDIO;

	DTOutCaps[2].code = GF_PROP_PID_CODECID;
	DTOutCaps[2].flags = GF_CAPS_INPUT;
	DTOutCaps[2].val.type = GF_PROP_UINT;
	DTOutCaps[2].val.value.uint = GF_CODECID_RAW;

	DTOutRegister.name = "dtout";
#ifndef GPAC_DISABLE_DOC
	DTOutRegister.description = "DekTec SDIOut";
	DTOutRegister.help = "This filter provides SDI output to be used with __DTA 2174__ or __DTA 2154__ cards.";
#endif
	DTOutRegister.private_size = sizeof(GF_DTOutCtx);
	DTOutRegister.args = DTOutArgs;
	DTOutRegister.caps = DTOutCaps;
	DTOutRegister.nb_caps = 3;

#if defined(GPAC_HAS_DTAPI) && !defined(FAKE_DT_API)
	DTOutRegister.initialize = dtout_initialize;
	DTOutRegister.finalize = dtout_finalize;
	DTOutRegister.configure_pid = dtout_configure_pid;
	DTOutRegister.process = dtout_process;
#else
	DTOutRegister.configure_pid = dtout_config_dummy;
	DTOutRegister.process = dtout_process_dummy;

#ifdef GPAC_ENABLE_COVERAGE
	dtout_config_dummy(NULL, NULL, GF_FALSE);
	dtout_process_dummy(NULL);
#endif

#endif
	return &DTOutRegister;
}
