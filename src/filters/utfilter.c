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
	GF_FilterPid *src_pid;
	GF_FilterPid *dst_pid;

	u32 ctr;
} GF_TestFilter;



static void test_filter_destruct(GF_Filter *filter)
{
	GF_TestFilter *stack = (GF_TestFilter *) gf_filter_get_udta(filter);
	gf_free(stack);
}

static GF_Err test_filter_process(GF_Filter *filter)
{
	const char *data;
	u32 size;
	GF_FilterPacket *pck_dst;
	GF_TestFilter *stack = (GF_TestFilter *) gf_filter_get_udta(filter);
	GF_FilterPacket *pck = gf_filter_pid_get_packet(stack->src_pid);
	if (!pck) return GF_OK;

	data = gf_filter_pck_get_data(pck, &size);

	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestFilter state %d forwards packet %s\n", stack->ctr, data));

	if (stack->ctr==0) {
		pck_dst = gf_filter_pck_new_ref(stack->dst_pid, data, size, pck);
		gf_filter_pck_set_framing(pck_dst, GF_TRUE, GF_TRUE);
	} else if (stack->ctr==1) {
		pck_dst = gf_filter_pck_new_ref(stack->dst_pid, data+6, 2, pck);
		gf_filter_pck_set_framing(pck_dst, GF_TRUE, GF_FALSE);
	} else if (stack->ctr==2) {
		pck_dst = gf_filter_pck_new_ref(stack->dst_pid, data+8, 2, pck);
		gf_filter_pck_set_framing(pck_dst, GF_FALSE, GF_FALSE);
	} else {
		pck_dst = gf_filter_pck_new_ref(stack->dst_pid, data+9, size-9, pck);
		gf_filter_pck_set_framing(pck_dst, GF_FALSE, GF_TRUE);
	}
	gf_filter_pck_copy_properties(pck, pck_dst);
	gf_filter_pck_send(pck_dst);
	stack->ctr = (stack->ctr+1)%4;


#if 0
	{
	GF_PropertyValue p;
	p.type=GF_PROP_NAME;
	p.value.string="custom_val_2";

	gf_filter_pck_set_property_str(pck_dst, "custom_attr_2", &p);
	gf_filter_pck_send(pck_dst);
	}
#endif

	gf_filter_pck_drop(pck);
	return GF_OK;
}

static GF_Err test_filter_config_input(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *format;
	GF_PropertyValue p;
	GF_TestFilter  *stack = (GF_TestFilter *) gf_filter_get_udta(filter);
	stack->src_pid = pid;

	format = gf_filter_pid_get_property(stack->src_pid, GF_4CC('c','u','s','t'), NULL);
	if (!format || !format->value.string || strcmp(format->value.string, "testSourceData")) {
		return GF_NOT_SUPPORTED;
	}
	stack->dst_pid = gf_filter_pid_new(filter);
	p.type=GF_PROP_NAME;
	p.value.string = "testSourceData";
	gf_filter_pid_set_property(stack->dst_pid, GF_4CC('c','u','s','t'), &p);

	return GF_OK;
}

GF_Err utfilter_load(GF_Filter *filter)
{
	GF_TestFilter *stack;
	GF_SAFEALLOC(stack, GF_TestFilter);
	gf_filter_set_udta(filter, stack);
	return GF_OK;
}

const GF_FilterRegister testFilterRegister = {
	.name = "UTFilter",
	.description = "Unit Test Filter, only used for unit testing of filter framework",
	.construct = utfilter_load,
	.destruct = test_filter_destruct,
	.process = test_filter_process,
	.configure_pid = test_filter_config_input
};


const GF_FilterRegister *test_filter_register()
{
	return &testFilterRegister;
}



