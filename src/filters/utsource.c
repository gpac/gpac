/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac application
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


typedef struct
{
	u32 initialized;
	u32 nb_packets;

	u32 pck_del;

	GF_FilterPid *pid;
} GF_TestSource;



static void test_source_destruct(GF_Filter *filter)
{
	GF_TestSource *stack = (GF_TestSource *) gf_filter_get_udta(filter);
	gf_free(stack);
}

static void test_pck_del(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	GF_TestSource *stack = (GF_TestSource *) gf_filter_get_udta(filter);
	const char *data = gf_filter_pck_get_data(pck, &size);
	stack->pck_del++;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: Deleting packet %s - %d out there\n", data, stack->nb_packets - stack->pck_del));
}

static GF_Err test_source_process(GF_Filter *filter)
{
	GF_PropertyValue p;
	GF_FilterPacket *pck;
	GF_TestSource *stack = (GF_TestSource *) gf_filter_get_udta(filter);

	if (stack->nb_packets==5000) return GF_EOS;
	if (0 && stack->nb_packets - stack->pck_del > 4) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: No packets to emit, waiting for destruction\n"));
		return GF_OK;
	}
	stack->nb_packets++;
	pck = gf_filter_pck_new_shared(stack->pid, "PacketShared", 12, test_pck_del);
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: pck %d PacketShared\n", stack->nb_packets));

	p.type = GF_PROP_NAME;
	p.value.string = "custom_value";
	gf_filter_pck_set_property(pck, GF_4CC('c','u','s','t'), &p);
	gf_filter_pck_send(pck);

#if 0
	{
	char *data;
	pck = gf_filter_pck_new_alloc(stack->pid, 12, &data);
	strcpy(data, "PacketAlloc");
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: Sending PacketAlloc\n"));
	return gf_filter_pck_send(pck);
	}
#endif
	return GF_OK;
}

static GF_Err test_source_set_args(GF_Filter *filter)
{
	GF_PropertyValue p;
	GF_TestSource *stack = (GF_TestSource *) gf_filter_get_udta(filter);
	if (stack->initialized) return GF_OK;

	stack->initialized = 1;
	
	stack->pid = gf_filter_pid_new(filter);
	p.type = GF_PROP_NAME;
	p.value.string = "testSourceData";
	gf_filter_pid_set_property(stack->pid, GF_4CC('c','u','s','t'), &p);
	return GF_OK;
}

GF_Err utsource_load(GF_Filter *filter)
{
	GF_TestSource *stack;
	GF_SAFEALLOC(stack, GF_TestSource);
	gf_filter_set_udta(filter, stack);
	return GF_OK;
}

static const GF_FilterArgs testSourceArgs[] =
{
	{"pck", "Packet content data", GF_PROP_NAME, "Packet", NULL, GF_TRUE},
	{"fps", "Framerate for generated packets. 0 means no timing", GF_PROP_FRACTION, "0/100", "1000", GF_TRUE},
	{ NULL }
};


const GF_FilterRegister testSourceRegister = {
	.name = "UTSource",
	.description = "Unit Test Source, only used for unit testing of filter framework",
	.args = (GF_FilterArgs **)&testSourceArgs,
	.construct = utsource_load,
	.destruct = test_source_destruct,
	.process = test_source_process,
	.update_args = test_source_set_args
};

const GF_FilterRegister *test_source_register()
{
	return &testSourceRegister;
}

