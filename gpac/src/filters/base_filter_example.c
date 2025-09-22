/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
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
#include <gpac/list.h>


typedef struct
{
	u32 opt1;
	Bool opt2;

	GF_FilterPid *src_pid;
	GF_FilterPid *dst_pid;
} GF_BaseFilterExample;

static void example_filter_finalize(GF_Filter *filter)
{
	//peform any finalyze routine needed, including potential free in the filter context
	//if not needed, set the filter_finalize to NULL
}
static GF_Err example_filter_process(GF_Filter *filter)
{
	char *data_dst;
	const char *data_src;
	u32 size;

	GF_FilterPacket *pck_dst;
	GF_BaseFilterExample *stack = (GF_BaseFilterExample *) gf_filter_get_udta(filter);

	GF_FilterPacket *pck = gf_filter_pid_get_packet(stack->src_pid);
	if (!pck) return GF_OK;
	data_src = gf_filter_pck_get_data(pck, &size);

	//produce output packet using memory allocation
	pck_dst = gf_filter_pck_new_alloc(stack->dst_pid, size, &data_dst);
	if (!pck_dst) return GF_OUT_OF_MEM;
	memcpy(data_dst, data_src, size);

	//no need to adjust data framing
//	gf_filter_pck_set_framing(pck_dst, GF_TRUE, GF_TRUE);

	//copy over src props to dst
	gf_filter_pck_merge_properties(pck, pck_dst);
	gf_filter_pck_send(pck_dst);

	gf_filter_pid_drop_packet(stack->src_pid);
	return GF_OK;

}

static GF_Err example_filter_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *format;
	GF_PropertyValue p;
	GF_BaseFilterExample  *stack = (GF_BaseFilterExample *) gf_filter_get_udta(filter);

	if (stack->src_pid==pid) {
		//disconnect of src pid (not yet supported)
		if (is_remove) {
		}
		//update of caps, check everything is fine
		else {
		}
		return GF_OK;
	}
	//check input pid properties we are interested in
	format = gf_filter_pid_get_property(pid, GF_4CC('c','u','s','t') );
	if (!format || !format->value.string || strcmp(format->value.string, "myformat")) {
		return GF_NOT_SUPPORTED;
	}


	//setup output (if we are a filter not a sink)

	stack->dst_pid = gf_filter_pid_new(filter);
	gf_filter_pid_copy_properties(stack->dst_pid, stack->src_pid);
	p.type = GF_PROP_UINT;
	p.value.uint = 10;
	gf_filter_pid_set_property(stack->dst_pid, GF_4CC('c','u','s','2'), &p);

	//set framing mode if needed - by default all PIDs require complete data blocks as inputs
//	gf_filter_pid_set_framing_mode(pidctx->src_pid, GF_TRUE);

	return GF_OK;
}

static GF_Err example_filter_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	return GF_OK;
}

GF_Err example_filter_initialize(GF_Filter *filter)
{
	GF_BaseFilterExample *stack = gf_filter_get_udta(filter);
	if (stack->opt2) {
		//do something based on options

	}
	//if you filter is a source, this is the right place to start declaring output PIDs, such as above

	return GF_OK;
}

#define OFFS(_n)	#_n, offsetof(GF_BaseFilterExample, _n)
static const GF_FilterArgs ExampleFilterArgs[] =
{
	//example uint option using enum, result parsed ranges from 0(=v1) to 2(=v3)
	{ OFFS(opt1), "Example option 1", GF_PROP_UINT, "v1", "v1|v2|v3", GF_FALSE},
	{ OFFS(opt2), "Example option 2", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ NULL }
};

static const GF_FilterCapability ExampleFilterInputs[] =
{
	{ .cap_string="cust", .val=PROP_NAME("myformat")  },
	{ NULL }
};

static const GF_FilterCapability ExampleFilterOutputs[] =
{
	{GF_4CC('c','u','s','t'), PROP_UINT(10) },
	{ NULL }
};

const GF_FilterRegister ExampleFilterRegister = {
	.name = "ExampleFilter",
	.description = "Example Test Filter, not used",
	.private_size = sizeof(GF_BaseFilterExample),
	.input_caps = ExampleFilterInputs,
	.output_caps = ExampleFilterOutputs,
	.args = ExampleFilterArgs,
	.initialize = example_filter_initialize,
	.finalize = example_filter_finalize,
	.process = example_filter_process,
	.configure_pid = example_filter_config_input,
	.update_arg = example_filter_update_arg
};

const GF_FilterRegister *ex_filter_register(GF_FilterSession *session, Bool load_meta_filters)
{
	return &ExampleFilterRegister;
}



