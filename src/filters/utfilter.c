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

typedef struct
{
	GF_FilterPid *src_pid;
	GF_FilterPid *dst_pid;

	//0: source, 1: sink, 2: filter
	u32 mode;

	u32 nb_packets, pck_del, max_pck;
	s32 max_out;
	Bool initialized;
	u32 ctr;

	GF_SHA1Context *sha_ctx;

} GF_UnitTestFilter;


static void dump_property(GF_FilterPacket *pck, u32 nb_recv, u32 p4cc, const char *pname)
{
	const GF_PropertyValue *att = gf_filter_pck_get_property(pck, p4cc, pname);
	if (!att) return;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\tPacket %d Prop %s: ", nb_recv, pname ? pname : gf_4cc_to_str(p4cc)));
	switch (att->type) {
	case GF_PROP_SINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(sint) %d", att->value.sint));
		break;
	case GF_PROP_UINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(uint) %u", att->value.uint));
		break;
	case GF_PROP_LONGSINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(slongint) "LLD, att->value.longsint));
		break;
	case GF_PROP_LONGUINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(ulongint) "LLU, att->value.longuint));
		break;
	case GF_PROP_FRACTION:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(frac) %d/%d", att->value.frac.num, att->value.frac.den));
		break;
	case GF_PROP_BOOL:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(bool) %s", att->value.boolean ? "true" : "fale"));
		break;
	case GF_PROP_FLOAT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(float) %f", FIX2FLT(att->value.fnumber) ));
		break;
	case GF_PROP_DOUBLE:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(double) %g", att->value.number));
		break;
	case GF_PROP_NAME:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(const) %s", att->value.string));
		break;
	case GF_PROP_STRING:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(string) %s", att->value.string));
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(data) %d bytes", att->data_len));
		break;
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(pointer) %p", att->value.ptr));
		break;
	case GF_PROP_FORBIDEN:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(forbiden)"));
		break;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\n"));
}

static void ut_filter_destruct(GF_Filter *filter)
{
	u8 digest[GF_SHA1_DIGEST_SIZE];
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->sha_ctx) {
		gf_sha1_finish(stack->sha_ctx, digest);

		if (!stack->src_pid) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("UTFilter: Source PID not available while dumping SHA1\n"));
		} else {
			const GF_PropertyValue *p = gf_filter_pid_get_property(stack->src_pid, GF_4CC('s','h','a','1'), NULL);
			if (!p) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("UTFilter: sha1 property not found on input pid\n"));
			} else if (p->data_len != GF_SHA1_DIGEST_SIZE) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("UTFilter: wrong size for sha1 property\n"));
			} else if (memcmp(p->value.data, digest, p->data_len )) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("UTFilter: wrong hash after execution\n"));
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("UTFilter: hash OK after execution :)\n"));
			}
		}
	}

	gf_free(stack);
}

static GF_Err ut_filter_process_filter(GF_Filter *filter)
{
	const char *data;
	u32 size;
	GF_FilterPacket *pck_dst;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

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
	gf_filter_pck_merge_properties(pck, pck_dst);
	gf_filter_pck_send(pck_dst);
	stack->ctr = (stack->ctr+1)%4;

	gf_filter_pid_drop_packet(stack->src_pid);
	return GF_OK;

}

static void test_pck_del(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);
	stack->pck_del++;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: Deleting packet - %d out there\n", stack->nb_packets - stack->pck_del));
}

static GF_Err ut_filter_process_source(GF_Filter *filter)
{
	GF_PropertyValue p;
	GF_FilterPacket *pck;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->nb_packets==stack->max_pck) {
		return GF_EOS;
	}

	if ((stack-> max_out>=0) && (stack->nb_packets - stack->pck_del >= (u32) stack->max_out) ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: No packets to emit, waiting for destruction\n"));
		return GF_OK;
	}
	stack->nb_packets++;
	gf_sha1_update(stack->sha_ctx, "PacketShared", 12);
	pck = gf_filter_pck_new_shared(stack->dst_pid, "PacketShared", 12, test_pck_del);

	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: pck %d PacketShared\n", stack->nb_packets));

	p.type = GF_PROP_NAME;
	p.value.string = "custom_value";
	gf_filter_pck_set_property(pck, GF_4CC('c','u','s','t'), &p);

	if (stack->nb_packets==stack->max_pck) {
		if (stack->sha_ctx) {
			u8 digest[GF_SHA1_DIGEST_SIZE];
			gf_sha1_finish(stack->sha_ctx, digest);
			stack->sha_ctx = NULL;
			p.type = GF_PROP_DATA;
			p.data_len = GF_SHA1_DIGEST_SIZE;
			p.value.data = (char *) digest;
			//with this we test both:
			//- SHA of send data is correct at the receiver side
			//- property update on a PID
			gf_filter_pid_set_property(stack->dst_pid, GF_4CC('s','h','a','1'), &p);
		}
	}
	gf_filter_pck_send(pck);

	return GF_OK;
}


static GF_Err ut_filter_process_sink(GF_Filter *filter)
{
	u32 size;
	const char *data;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);
	GF_FilterPacket *pck = gf_filter_pid_get_packet(stack->src_pid);
	if (!pck)
		return GF_OK;

	data = gf_filter_pck_get_data(pck, &size);

	gf_sha1_update(stack->sha_ctx, (u8*)data, size);

	stack->nb_packets++;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSink: Consuming packet %d bytes\n", size));

	dump_property(pck, stack->nb_packets, GF_4CC('c','u','s','t'), NULL);
	dump_property(pck, stack->nb_packets, 0, "custom_attr_2");

	gf_filter_pid_drop_packet(stack->src_pid);
	return GF_OK;
}


static GF_Err ut_filter_process(GF_Filter *filter)
{
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->mode==0) {
		return ut_filter_process_source(filter);
	}
	else if (stack->mode==1) {
		return ut_filter_process_sink(filter);
	}
	else if (stack->mode==2) {
		return ut_filter_process_filter(filter);
	} else {
		return GF_BAD_PARAM;
	}

}

static GF_Err ut_filter_config_input(GF_Filter *filter, GF_FilterPid *pid, GF_PID_ConfigState state)
{
	const GF_PropertyValue *format, *att;
	GF_PropertyValue p;
	char *str;
	GF_UnitTestFilter  *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->mode==0) return GF_BAD_PARAM;
	//for both filter and sink modes, check input format

	att = gf_filter_get_property(filter, "pid_att");
	str = att->value.string;

	//something is being reconfigured. We check we have the same custum arg, otherwise wwe do not support
	if (stack->src_pid == pid) {
		format = gf_filter_pid_get_property(stack->src_pid, GF_4CC('c','u','s','t'), NULL);
		if (!format || !format->value.string || strcmp(format->value.string, str)) {
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}


	stack->src_pid = pid;


	format = gf_filter_pid_get_property(stack->src_pid, GF_4CC('c','u','s','t'), NULL);
	if (!format || !format->value.string || strcmp(format->value.string, str)) {
		return GF_NOT_SUPPORTED;
	}
	//filter mode, setup output
	if (stack->mode==2) {
		stack->dst_pid = gf_filter_pid_new(filter);
		p.type=GF_PROP_NAME;
		p.value.string = str;
		gf_filter_pid_set_property(stack->dst_pid, GF_4CC('c','u','s','t'), &p);
	}
	//sink mode, request full reconstruction of input blocks
	else {
		gf_filter_pid_set_framing_mode(stack->src_pid, GF_TRUE);
		stack->sha_ctx = gf_sha1_starts();
	}

	return GF_OK;
}

static GF_Err ut_filter_config_source(GF_Filter *filter)
{
	GF_PropertyValue p;
	const GF_PropertyValue *att;

	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	//create a pid
	stack->dst_pid = gf_filter_pid_new(filter);

	att = gf_filter_get_property(filter, "pid_att");

	//set a custum property
	p.type = GF_PROP_NAME;
	p.value.string = (att && att->value.string) ? att->value.string : "UTSourceData";
	gf_filter_pid_set_property(stack->dst_pid, GF_4CC('c','u','s','t'), &p);

	att = gf_filter_get_property(filter, "max_pck");
	stack->max_pck = att->value.uint;
	att = gf_filter_get_property(filter, "max_out");
	stack->max_out = att->value.sint;

	stack->sha_ctx = gf_sha1_starts();
	return GF_OK;
}


static GF_Err ut_filter_set_args(GF_Filter *filter)
{
	const GF_PropertyValue *mode;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);
	if (stack->initialized) return GF_OK;

	stack->initialized = 1;
	mode = gf_filter_get_property(filter, "mode");
	if (!strcmp(mode->value.string, "sink")) {
		stack->mode=1;
		gf_filter_set_name(filter, "UTSink");
	}
	else if (!strcmp(mode->value.string, "filter")) {
		stack->mode=2;
		gf_filter_set_name(filter, "UTFilter");
	} else if (!strcmp(mode->value.string, "source")) {
		gf_filter_set_name(filter, "UTSource");
	} else {
		return GF_BAD_PARAM;
	}

	//configure source
	if (stack->mode==0) {
		return ut_filter_config_source(filter);
	}


	return GF_OK;
}


GF_Err utfilter_load(GF_Filter *filter)
{
	GF_UnitTestFilter *stack;
	GF_SAFEALLOC(stack, GF_UnitTestFilter);
	gf_filter_set_udta(filter, stack);
	return GF_OK;
}

static const GF_FilterArgs UTFilterArgs[] =
{
	{"mode", "Set filter operation mode", GF_PROP_NAME, "source", "source|sink|filter", GF_FALSE},
	{"pid_att", "Sets default value for PID \"cust\" attribute", GF_PROP_NAME, "UTSourceData", NULL, GF_FALSE},
	{"max_pck", "Maximum number of packets to send in source mode", GF_PROP_UINT, "1000", NULL, GF_FALSE},
	{"max_out", "Maximum number of shared packets not released in source mode", GF_PROP_SINT, "-1", NULL, GF_TRUE},
	{ NULL }
};

const GF_FilterRegister UTFilterRegister = {
	.name = "UTFilter",
	.description = "Unit Test Filter, only used for unit testing of filter framework",
	.args = UTFilterArgs,
	.construct = utfilter_load,
	.destruct = ut_filter_destruct,
	.process = ut_filter_process,
	.configure_pid = ut_filter_config_input,
	.update_args = ut_filter_set_args
};


const GF_FilterRegister *ut_filter_register()
{
	return &UTFilterRegister;
}



