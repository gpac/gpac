/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR decoder filter
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

#ifndef GPAC_DISABLE_LASER

#include <gpac/internal/compositor_dev.h>
#include <gpac/laser.h>
#include <gpac/constants.h>

typedef struct
{
	GF_Scene *scene;
	GF_ObjectManager *odm;
	GF_LASeRCodec *codec;
	u32 PL, nb_streams;
	Bool is_playing;
	GF_FilterPid *out_pid;
} GF_LSRDecCtx;

static GF_Err lsrdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_FilterPid *out_pid;
	u32 ESID=0;
	GF_LSRDecCtx *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *prop;

	//we must have streamtype SCENE
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop || (prop->value.uint != GF_STREAM_SCENE)) {
		return GF_NOT_SUPPORTED;
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop || (prop->value.uint != GF_CODECID_LASER) ) {
		return GF_NOT_SUPPORTED;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (prop) ESID = prop->value.uint;
	else {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (prop) ESID = prop->value.uint;
	}

	//we must have a dsi
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop || !prop->value.data.ptr || !prop->value.data.size) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (is_remove) {
		out_pid = gf_filter_pid_get_udta(pid);
		if (ctx->out_pid==out_pid)
			ctx->out_pid = NULL;
		if (out_pid)
			gf_filter_pid_remove(out_pid);
		ctx->nb_streams--;
		if (ctx->codec && ESID) {
			return gf_laser_decoder_remove_stream(ctx->codec, ESID);
		}
		return GF_OK;
	}
	//this is a reconfigure
	if (gf_filter_pid_get_udta(pid)) {
		gf_laser_decoder_remove_stream(ctx->codec, ESID);
		return gf_laser_decoder_configure_stream(ctx->codec, ESID, prop->value.data.ptr, prop->value.data.size);
	}

	//check our namespace
	if (ctx->scene && ! gf_filter_pid_is_filter_in_parents(pid, ctx->scene->root_od->scene_ns->source_filter)) {
		return GF_REQUIRES_NEW_INSTANCE;
	}

	//declare a new output PID of type scene, codecid RAW
	out_pid = gf_filter_pid_new(filter);
	ctx->nb_streams++;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(out_pid, pid);
	gf_filter_pid_set_property(out_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_udta(pid, out_pid);

	if (!ctx->out_pid)
		ctx->out_pid = out_pid;
	return GF_OK;
}

static Bool lsrdec_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	u32 count, i;
	GF_LSRDecCtx *ctx = gf_filter_get_udta(filter);
	//check for scene attach
	switch (com->base.type) {
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (!com->attach_scene.on_pid) return GF_TRUE;

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(ipid);
		//we found our pid, set it up
		if (opid == com->attach_scene.on_pid) {
			if (!ctx->odm) {
				ctx->odm = com->attach_scene.object_manager;
				ctx->scene = ctx->odm->subscene ? ctx->odm->subscene : ctx->odm->parentscene;

				ctx->codec = gf_laser_decoder_new(ctx->scene->graph);
				/*attach the clock*/
				gf_laser_decoder_set_clock(ctx->codec, gf_scene_get_time, ctx->scene);
				gf_filter_pid_set_udta(opid, com->attach_scene.object_manager);
				lsrdec_configure_pid(filter, ipid, GF_FALSE);
			}
			return GF_TRUE;
		}
	}

	return GF_TRUE;
}

static GF_Err lsrdec_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_FilterPacket *pck;
	const char *data;
	u32 size, ESID=0;
	u64 now, cts;
	u32 i, count;
	const GF_PropertyValue *prop;
	GF_LSRDecCtx *ctx = gf_filter_get_udta(filter);
	GF_Scene *scene = ctx->scene;

	if (!scene) {
		if (ctx->is_playing) {
			if (ctx->out_pid && gf_laser_decode_has_conditionnals(ctx->codec)) {
				gf_filter_pid_set_info(ctx->out_pid, GF_PROP_PID_KEEP_AFTER_EOS, &PROP_BOOL(GF_TRUE));
			}
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}
	if (!ctx->codec) return GF_OK;


	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(pid);

		GF_ObjectManager *odm = gf_filter_pid_get_udta(opid);
		if (!odm) continue;
		//object clock shall be valid
		assert(odm->ck);

		pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			Bool is_eos = gf_filter_pid_is_eos(pid);
			if (is_eos) {
				if (ctx->out_pid && gf_laser_decode_has_conditionnals(ctx->codec)) {
					gf_filter_pid_set_info(ctx->out_pid, GF_PROP_PID_KEEP_AFTER_EOS, &PROP_BOOL(GF_TRUE));
				}
				gf_filter_pid_set_eos(opid);
			}
			continue;
		}
		data = gf_filter_pck_get_data(pck, &size);

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (prop) ESID = prop->value.uint;

		cts = gf_filter_pck_get_cts( pck );
		cts = gf_timestamp_to_clocktime(cts, gf_filter_pck_get_timescale(pck));

		if (!gf_sc_check_sys_frame(scene, odm, pid, filter, cts, 0))
			continue;

		now = gf_sys_clock_high_res();
		e = gf_laser_decode_au(ctx->codec, ESID, data, size);
		now = gf_sys_clock_high_res() - now;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[BIFS] ODM%d #CH%d at %d decoded AU TS %u in "LLU" us\n", odm->ID, ESID, cts, now));

		gf_filter_pid_drop_packet(pid);

		if (e) return e;
		if (odm == ctx->odm)
			gf_scene_attach_to_compositor(scene);
	}
	return GF_OK;
}

static GF_Err lsrdec_initialize(GF_Filter *filter)
{
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		gf_svg_get_system_paint_server_type("");
		gf_svg_get_system_paint_server_name(0);
	}
#endif
	return GF_OK;
}

static void lsrdec_finalize(GF_Filter *filter)
{
	GF_LSRDecCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->codec) gf_laser_decoder_del(ctx->codec);
}


static const GF_FilterCapability LSRDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_LASER),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


GF_FilterRegister LSRDecRegister = {
	.name = "lsrdec",
	GF_FS_SET_DESCRIPTION("MPEG-4 LASeR decoder")
	GF_FS_SET_HELP("This filter decodes MPEG-4 LASeR binary frames directly into the scene graph of the compositor.\n"
	"Note: This filter cannot be used to dump LASeR content to text or xml, use `MP4Box` for that.")
	.private_size = sizeof(GF_LSRDecCtx),
	.flags = GF_FS_REG_MAIN_THREAD,
	.priority = 1,
	SETCAPS(LSRDecCaps),
	.initialize = lsrdec_initialize,
	.finalize = lsrdec_finalize,
	.process = lsrdec_process,
	.configure_pid = lsrdec_configure_pid,
	.process_event = lsrdec_process_event,
};

#endif /*GPAC_DISABLE_LASER*/

const GF_FilterRegister *lsrdec_register(GF_FilterSession *session)
{
#ifdef GPAC_DISABLE_LASER
	return NULL;
#else
	return &LSRDecRegister;
#endif /*GPAC_DISABLE_LASER*/
}

