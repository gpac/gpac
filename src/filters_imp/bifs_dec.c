/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS decoder module
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
#include <gpac/bifs.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_BIFS

typedef struct
{
	GF_SceneGraph *scene_graph;

	GF_BifsDecoder *codec;
} GF_BIFSDecCtx;

static GF_Err bifsdec_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 ESID=0, OTI=0;
	const GF_PropertyValue *prop;
	GF_BIFSDecCtx *ctx = (GF_BIFSDecCtx *) gf_filter_get_udta(filter);
	GF_Err e;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;
	if (prop->value.uint != GF_STREAM_SCENE) return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (!prop) return GF_NOT_SUPPORTED;
	OTI = prop->value.uint;
	switch (OTI) {
	case GPAC_OTI_SCENE_BIFS:
	case GPAC_OTI_SCENE_BIFS_V2:
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) ESID = prop->value.uint;

	if (is_remove) {
		return gf_bifs_decoder_remove_stream(ctx->codec, ESID);
	}

	//we must have a dsi
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop || !prop->value.data || !prop->data_len) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	ctx->codec = gf_bifs_decoder_new(ctx->scene_graph, GF_FALSE);

	e = gf_bifs_decoder_configure_stream(ctx->codec, ESID, prop->value.data, prop->data_len, OTI);
	if (e) return e;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
	//in IOD, check if we need to setup the scene
	if (prop && prop->value.boolean) {
	} else {
		/*ignore all size info on anim streams*/
		gf_bifs_decoder_ignore_size_info(ctx->codec);
	}

	return GF_OK;
}

static GF_Err bifsdec_process(GF_Filter *filter)
{
	Double ts_offset;
	u32 timescale;

	GF_Err e = GF_OK;
	GF_BIFSDecCtx *ctx = (GF_BIFSDecCtx *) gf_filter_get_udta(filter);

	u32 i, count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		const GF_PropertyValue *prop;
		char *data;
		u32 size, ESID=0;
		GF_Filter *pid = gf_filter_get_ipid(filter, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) continue;
		data = gf_filter_pck_get_data(pck, &size);
		if (!data) {
			if (gf_filter_pck_get_eos(pck)) {
				gf_filter_pid_set_eos(pid);
				continue;
			}
		}
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (prop) ESID = prop->value.uint;

		ts_offset = (Double) gf_filter_pck_get_cts( pck );
		ts_offset /= gf_filter_pck_get_timescale(pck);

		e = gf_bifs_decode_au(ctx->codec, ESID, data, size, ts_offset);

		gf_filter_pid_drop_packet(pid);

		if (e) return e;
		/*if scene not attached do it*/
//		gf_scene_attach_to_compositor(priv->pScene);
	}

	return GF_OK;
}

static void bifsdec_finalize(GF_Filter *filter)
{
	GF_BIFSDecCtx *ctx = (GF_BIFSDecCtx *) gf_filter_get_udta(filter);

	if (ctx->codec) gf_bifs_decoder_del(ctx->codec);
}

static GF_Err bifsdec_initialize(GF_Filter *filter)
{
	GF_BIFSDecCtx *ctx = (GF_BIFSDecCtx *) gf_filter_get_udta(filter);
	ctx->codec = NULL;
	return GF_OK;
}


#endif /*GPAC_DISABLE_BIFS*/


/*
#define OFFS(_n)	#_n, offsetof(GF_BIFSDecCtx, _n)
static const GF_FilterArgs InspectArgs[] =
{
	{ OFFS(logfile), "Sets inspect log filename", GF_PROP_STRING, "stderr", "file/stderr/stdout", GF_FALSE},
	{ OFFS(framing), "Enables full frame/block reconstruction before inspection", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(interleave), "Dumps packets as they are received on each pid. If false, report per pid is generated", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(pid_only), "Only dumps PID state change, not packets", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dump_data), "Enables full data dump - heavy !", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};
*/


const GF_FilterRegister BIFSRegister = {
	.name = "bifsdec",
	.description = "MPEG-4 BIFS Decoder",
	.private_size = sizeof(GF_BIFSDecCtx),
	.args = NULL,
	.initialize = bifsdec_initialize,
	.finalize = bifsdec_finalize,
	.process = bifsdec_process,
	.configure_pid = bifsdec_config_input,
};

const GF_FilterRegister *bifsdec_register(GF_FilterSession *session, Bool load_meta_filters)
{
#ifdef GPAC_DISABLE_BIFS
	return NULL;
#else
	return &BIFSRegister;
#endif
}



