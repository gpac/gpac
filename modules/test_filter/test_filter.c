/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2021
 *					All rights reserved
 *
 *  This file is part of GPAC / test sink filter
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


#include <gpac/filters.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>


typedef struct
{
	Bool opt1;
} TestContext;


static GF_Err testfilter_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	TestContext *ctx = (TestContext *)gf_filter_get_udta(filter);
	//if first time we see the pid, send a play event 
	TestContext *pctx = (TestContext *)gf_filter_pid_get_udta(pid);
	if (!pctx && !is_remove) {
		GF_FilterEvent evt;
		gf_filter_pid_set_udta(pid, ctx);
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
	}

	return GF_OK;
}


static Bool testfilter_process_event(GF_Filter *filter, const GF_FilterEvent *fevt)
{
	return GF_FALSE;
}


static GF_Err testfilter_process(GF_Filter *filter)
{
	u32 i, count = gf_filter_get_ipid_count(filter);
	for (i = 0; i < count; i++) {
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		gf_filter_pid_drop_packet(pid);
	}
	return GF_OK;
}

static GF_Err testfilter_initialize(GF_Filter *filter)
{
	TestContext *ctx = (TestContext *) gf_filter_get_udta(filter);
	return GF_OK;
}

static void testfilter_finalize(GF_Filter *filter)
{
	TestContext *ctx = (TestContext *) gf_filter_get_udta(filter);
}

static const GF_FilterCapability TestFilterCaps[] =
{
	//anything but file
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE)
};

#define OFFS(_n)	#_n, offsetof(TestContext, _n)

static const GF_FilterArgs TestFilterArgs[] =
{
	{ OFFS(opt1), "some boolean option", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister TestFilterRegister = {
	.name = "testfilter",
	GF_FS_SET_DESCRIPTION("Test Filter")
	GF_FS_SET_HELP("This filter tests loading of filters from dynmaic libraries")
	.private_size = sizeof(TestContext),
	SETCAPS(TestFilterCaps),
	.args = TestFilterArgs,
	.initialize = testfilter_initialize,
	.finalize = testfilter_finalize,
	.configure_pid = testfilter_configure_pid,
	.process = testfilter_process,
	.process_event = testfilter_process_event,
};

GPAC_MODULE_EXPORT
GF_FilterRegister *RegisterFilter(GF_FilterSession *session)
{
	return &TestFilterRegister;
}

