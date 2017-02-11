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
#include <gpac/thread.h>


typedef struct
{
	u32 nb_packets;

	GF_FilterPid *pid;

	GF_SHA1Context *sha_ctx;
} GF_TestSink;



static void test_sink_destruct(GF_Filter *filter)
{
	u32 i;
	u8 digest[GF_SHA1_DIGEST_SIZE];
	GF_TestSink *stack = (GF_TestSink *) gf_filter_get_udta(filter);
	gf_sha1_finish(stack->sha_ctx, digest);

	fprintf(stderr, "UTSink %d packets, SHA1: ", stack->nb_packets);
	for (i=0; i<20; i++) fprintf(stderr, "%02X", digest[i]);
	fprintf(stderr, "\n");

	gf_free(stack);
}

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
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(data) %d bytes", att->data_len));
		break;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\n"));
}

static GF_Err test_sink_process(GF_Filter *filter)
{
	u32 size, i;
	const char *data;

	GF_TestSink *stack = (GF_TestSink *) gf_filter_get_udta(filter);
	GF_FilterPacket *pck = gf_filter_pid_get_packet(stack->pid);
	if (!pck) return GF_OK;

	data = gf_filter_pck_get_data(pck, &size);

	gf_sha1_update(stack->sha_ctx, (u8*)data, size);

	stack->nb_packets++;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSink: Consuming packet ", gf_th_id() ));
	for (i=0; i<size; i++) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("%c", data[i]));
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\n"));

	dump_property(pck, stack->nb_packets, GF_4CC('c','u','s','t'), NULL);
	dump_property(pck, stack->nb_packets, 0, "custom_attr_2");

	gf_filter_pck_drop(pck);
	return GF_OK;
}

static GF_Err test_sink_config_input(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *format;
	GF_TestSink *stack = (GF_TestSink *) gf_filter_get_udta(filter);
	stack->pid = pid;

	format = gf_filter_pid_get_property(stack->pid, GF_4CC('c','u','s','t'), NULL);
	if (!format || !format->value.string || strcmp(format->value.string, "testSourceData")) {
		return GF_NOT_SUPPORTED;
	}
	gf_filter_pid_set_framing_mode(stack->pid, GF_TRUE);

	stack->sha_ctx = gf_sha1_starts();
	return GF_OK;
}

GF_Err utsink_load(GF_Filter *filter)
{
	GF_TestSink *stack;
	GF_SAFEALLOC(stack, GF_TestSink);
	gf_filter_set_udta(filter, stack);

	return GF_OK;
}


static const GF_FilterArgs testSinkArgs[] =
{
	{"fps", "Framerate for consumming packets. 0 means no timing", GF_PROP_FRACTION, "0/100", "1000", GF_TRUE},
	{ NULL }
};


const GF_FilterRegister testSinkRegister = {
	.name = "UTSink",
	.description = "Unit Test Sink, only used for unit testing of filter framework",
	.args = (GF_FilterArgs **)&testSinkArgs,
	.construct = utsink_load,
	.destruct = test_sink_destruct,
	.process = test_sink_process,
	.configure_pid = test_sink_config_input
};

const GF_FilterRegister *test_sink_register()
{
	return &testSinkRegister;
}



