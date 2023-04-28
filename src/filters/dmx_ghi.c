/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / GHI demuxer filter
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
#include <gpac/constants.h>
#include <gpac/list.h>
#include <gpac/mpd.h>
#include <gpac/bitstream.h>
#include <gpac/base_coding.h>
#include <gpac/network.h>

typedef struct
{
	u64 first_tfdt, first_pck_seq, seg_duration, frag_start_offset, frag_tfdt;
	u32 split_first, split_last;
} GHISegInfo;

typedef struct
{
	GF_FilterPid *ipid;
	GF_List *opids;

	GHISegInfo seg_info;
	u32 nb_pck;

	Bool first_sent;
	u32 starts_with_sap;
	u32 seg_num;
	Bool inactive;
	Bool empty_seg;
	u32 active_mux_base_plus_one;
	GF_PropStringList mux_dst;

#ifndef GPAC_DISABLE_MPD
	//for xml-based
	GF_List *segs_xml;
	GF_List *x_children;
#endif

	u32 bitrate;
	u32 delay;
	Bool use_offsets;

	//for bin-based
	GF_List *segs_bin;

	GF_Filter *filter_src;

	char *rep_id, *res_url;
	u32 track_id, pid_timescale, mpd_timescale, sample_duration, first_frag_start_offset;
	s32 first_cts_offset;
	u32 props_size, props_offset, rep_flags, nb_segs;
} GHIStream;

enum
{
	GHI_GM_NONE=0,
	GHI_GM_ALL,
	GHI_GM_MAIN,
	GHI_GM_CHILD,
	GHI_GM_INIT,
};

typedef struct
{
	//options
	u32 gm;
	Bool force;
	GF_PropStringList mux;
	char *rep, *out;
	u32 sn;

	//internal
	GF_FilterPid *ipid;
	Bool init;
	GF_List *streams;

	u32 segment_duration;
	u64 max_segment_duration;
	u64 media_presentation_duration;
	u64 period_duration;
	char *segment_template;

	u64 min_ts_plus_one;
	u32 min_ts_timescale;
	u64 min_offset_plus_one, max_end_start_offset_plus_one;
} GHIDmxCtx;


static void set_opids_props(GHIDmxCtx *ctx, GHIStream *st)
{
	u32 i, count = gf_list_count(st->opids);
	for (i=0; i<count; i++) {
		GF_FilterPid *opid = gf_list_get(st->opids, i);

		if (st->ipid)
			gf_filter_pid_copy_properties(opid, st->ipid);

		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_DUR, &PROP_FRAC_INT(ctx->segment_duration, 1000));
		gf_filter_pid_set_property(opid, GF_PROP_PID_PERIOD_DUR, &PROP_FRAC64_INT(ctx->period_duration, 1000));
		gf_filter_pid_set_property(opid, GF_PROP_PID_URL, &PROP_STRING( gf_file_basename(st->res_url)) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_FILEPATH, &PROP_STRING( gf_file_basename(st->res_url)) );
		if (st->track_id)
			gf_filter_pid_set_property(opid, GF_PROP_PID_ID, &PROP_UINT(st->track_id) );

		if (ctx->gm) {
			switch (ctx->gm) {
			case GHI_GM_ALL:
				gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_all"));
				break;
			case GHI_GM_MAIN:
				gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_man"));
				break;
			case GHI_GM_CHILD:
				gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_child"));
				break;
			case GHI_GM_INIT:
				gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_init"));
				break;
			}
			gf_filter_pid_set_property_str(opid, "mpd_duration", &PROP_LONGUINT(ctx->media_presentation_duration));
			gf_filter_pid_set_property_str(opid, "max_seg_dur", &PROP_UINT((u32) ctx->max_segment_duration));
			gf_filter_pid_set_property_str(opid, "start_with_sap", &PROP_UINT(st->starts_with_sap));
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_seg"));
			gf_filter_pid_set_property(opid, GF_PROP_PID_START_NUMBER, &PROP_UINT(st->seg_num));
		}
		if (ctx->segment_template)
			gf_filter_pid_set_property_str(opid, "idx_template", &PROP_STRING(ctx->segment_template));
		if (ctx->out)
			gf_filter_pid_set_property_str(opid, "idx_out", &PROP_STRING(ctx->out));

		if (st->sample_duration)
			gf_filter_pid_set_property(opid, GF_PROP_PID_CONSTANT_DURATION, &PROP_UINT(st->sample_duration));

		if (st->bitrate)
			gf_filter_pid_set_property(opid, GF_PROP_PID_BITRATE, &PROP_UINT(st->bitrate));

		if (st->mux_dst.nb_items) {
			if (st->active_mux_base_plus_one) {
				gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(st->mux_dst.vals[st->active_mux_base_plus_one-1]));
			} else {
				gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(st->mux_dst.vals[i]));
			}
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(st->rep_id));
		}
		if (!st->ipid)
			gf_filter_pid_set_name(opid, st->rep_id);
	}
}

static void ghi_dmx_send_seg_times(GHIDmxCtx *ctx, GHIStream *st, GF_FilterPid *opid)
{
	u32 i, count;
#ifndef GPAC_DISABLE_MPD
	if (st->segs_xml) {
		count=gf_list_count(st->segs_xml);
	} else
#endif
	{
		count=gf_list_count(st->segs_bin);
	}
	for (i=0; i<count; i++) {
		GF_FilterPacket *pck = gf_filter_pck_new_alloc(opid, 0, NULL);
		u32 dur;
		u64 ts;
#ifndef GPAC_DISABLE_MPD
		if (st->segs_xml) {
			GF_MPD_SegmentURL *surl = gf_list_get(st->segs_xml, i);
			ts = surl->first_tfdt + surl->split_first_dur;
			dur = (u32) surl->duration;
		} else
#endif
		{
			GHISegInfo *si = gf_list_get(st->segs_bin, i);
			ts = si->first_tfdt + si->split_first;
			dur = (u32) si->seg_duration;
		}
		gf_filter_pck_set_dts(pck, ts);
		if (!i) ts += st->first_cts_offset;
		gf_filter_pck_set_cts(pck, ts);

		dur = (u32) gf_timestamp_rescale(dur, st->mpd_timescale, st->pid_timescale);
		gf_filter_pck_set_duration(pck, dur);
		gf_filter_pck_set_sap(pck, st->starts_with_sap);
		gf_filter_pck_set_property(pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
		gf_filter_pck_set_seek_flag(pck, GF_TRUE);
		gf_filter_pck_send(pck);
	}
	//dasher expects at least one packet to start, send a dummy one - this happens whn generating init segments only
	if (!count) {
		GF_FilterPacket *pck = gf_filter_pck_new_alloc(opid, 0, NULL);
		gf_filter_pck_set_dts(pck, 0);
		gf_filter_pck_set_cts(pck, 0);
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		gf_filter_pck_set_property(pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
		gf_filter_pck_set_seek_flag(pck, GF_TRUE);
		gf_filter_pck_send(pck);
	}
	gf_filter_pid_set_eos(opid);
}

GF_Err ghi_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GHIDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->ipid) {
		ctx->ipid = pid;
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}
	if (ctx->ipid == pid) return GF_OK;

	GHIStream *st = gf_filter_pid_get_udta(pid);
	if (!st) {
		const GF_PropertyValue *url = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
		if (!url) return GF_SERVICE_ERROR;
		const GF_PropertyValue *p_id = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p_id) return GF_SERVICE_ERROR;
		u32 i;
		for (i=0; i<gf_list_count(ctx->streams); i++) {
			st = gf_list_get(ctx->streams, i);
			if (!st->inactive && !gf_filter_pid_is_filter_in_parents(pid, st->filter_src)) {
				st = NULL;
				continue;
			}
			if (!st->track_id) break;
			if (st->track_id == p_id->value.uint) break;
			st = NULL;
		}
		if (!st) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Failed to locate source filter for pid %s\n", gf_filter_pid_get_name(pid)));
			return GF_SERVICE_ERROR;
		}
		if (st->inactive) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
			evt.play.initial_broadcast_play = 2;
			gf_filter_pid_send_event(pid, &evt);
			evt.base.type = GF_FEVT_STOP;
			gf_filter_pid_send_event(pid, &evt);
			return GF_OK;
		}
		gf_filter_pid_set_udta(pid, st);

		st->ipid = pid;
		if (!st->active_mux_base_plus_one) {
			for (i=0; i<st->mux_dst.nb_items; i++) {
				GF_FilterPid *opid = gf_filter_pid_new(filter);
				gf_list_add(st->opids, opid);
				gf_filter_pid_set_udta(opid, st);
			}
		}
		if (!gf_list_count(st->opids)) {
			GF_FilterPid *opid = gf_filter_pid_new(filter);
			gf_list_add(st->opids, opid);
			gf_filter_pid_set_udta(opid, st);
		}
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}

	set_opids_props(ctx, st);

	return GF_OK;
}

static Bool ghi_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterEvent fevt;
	GHIStream *st = evt->base.on_pid ? gf_filter_pid_get_udta(evt->base.on_pid) : NULL;
	GHIDmxCtx *ctx = gf_filter_get_udta(filter);
	if (!st) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->gm) {
			ghi_dmx_send_seg_times(ctx, st, evt->base.on_pid);
			return GF_TRUE;
		}
		if (st->empty_seg) {
			GF_FilterPid *opid = gf_list_get(st->opids, 0);
			GF_FilterPacket *dst = gf_filter_pck_new_alloc(opid, 0, NULL);
			gf_filter_pck_set_dts(dst, st->seg_info.first_tfdt);
			gf_filter_pck_set_cts(dst, st->seg_info.first_tfdt);
			gf_filter_pck_set_sap(dst, GF_FILTER_SAP_1);
			gf_filter_pck_set_property(dst, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
			u32 dur = (u32) gf_timestamp_rescale(st->seg_info.seg_duration, st->mpd_timescale, st->pid_timescale);
			gf_filter_pck_set_duration(dst, dur);
			gf_filter_pck_send(dst);

			gf_filter_pid_set_eos(opid);
			st->inactive = GF_TRUE;
			return GF_TRUE;
		}
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, st->ipid);
		fevt.base.on_pid = st->ipid;
		if (st->use_offsets) {
			fevt.play.hint_start_offset = ctx->min_offset_plus_one-1;
			if (st->seg_info.frag_start_offset) {
				fevt.play.hint_first_dts = st->seg_info.frag_tfdt ? st->seg_info.frag_tfdt : st->seg_info.first_tfdt;
			}
			if (ctx->max_end_start_offset_plus_one)
				fevt.play.hint_end_offset = ctx->max_end_start_offset_plus_one - 1;
		}
		if (ctx->min_ts_plus_one) {
			fevt.play.start_range = (Double) (ctx->min_ts_plus_one-1);
			fevt.play.start_range /= ctx->min_ts_timescale;
		} else {
			fevt.play.start_range = (Double) st->seg_info.first_tfdt;
			fevt.play.start_range /= st->pid_timescale;
		}
		fevt.play.from_pck = (u32) st->seg_info.first_pck_seq;
		fevt.play.to_pck = (u32) st->seg_info.first_pck_seq + st->nb_pck;
		fevt.play.orig_delay = st->delay;

		gf_filter_pid_send_event(st->ipid, &fevt);

		return GF_TRUE;

	case GF_FEVT_STOP:
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static Bool ghi_dmx_on_filter_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	GF_Filter *filter = (GF_Filter *)udta;
	if (udta) gf_filter_setup_failure(filter, err);
	return GF_FALSE;
}

static void ghi_dmx_declare_opid_xml(GF_Filter *filter, GHIDmxCtx *ctx, GHIStream *st)
{
#ifndef GPAC_DISABLE_MPD
	if (!gf_list_count(st->opids)) {
		u32 i;
		for (i=0; i<st->mux_dst.nb_items; i++) {
			GF_FilterPid *opid = gf_filter_pid_new(filter);
			gf_filter_pid_set_udta(opid, st);
			gf_list_add(st->opids, opid);
		}
		if (!st->mux_dst.nb_items) {
			GF_FilterPid *opid = gf_filter_pid_new(filter);
			gf_filter_pid_set_udta(opid, st);
			gf_list_add(st->opids, opid);
		}
	}

	u8 *obuf = NULL;
	u32 obuf_alloc = 0;
	GF_FilterPid *opid = gf_list_get(st->opids, 0);
	u32 i, nb_props = gf_list_count(st->x_children);
	for (i=0; i<nb_props; i++) {
		Bool is_str_list=GF_FALSE;
		Bool do_reset=GF_TRUE;
		u32 j=0, p4cc=0, ptype=0, vlen, obuf_size;
		const char *pname=NULL;
		char *value=NULL;
		GF_XMLAttribute *att;
		GF_PropertyValue p;

		GF_XMLNode *n = gf_list_get(st->x_children, i);
		if (n->type != GF_XML_NODE_TYPE) continue;
		if (!n->name || strcmp(n->name, "prop")) continue;
		while ( (att = gf_list_enum(n->attributes, &j))) {
			if (!strcmp(att->name, "type")) p4cc = gf_props_get_id(att->value);
			else if (!strcmp(att->name, "name")) pname = att->value;
			else if (!strcmp(att->name, "ptype")) ptype = gf_props_parse_type(att->value);
			else if (!strcmp(att->name, "value")) value = att->value;
		}
		if (p4cc) {
			ptype = gf_props_4cc_get_type(p4cc);
			pname = gf_props_4cc_get_name(p4cc);
		}
		else if (!pname) continue;

		if (!ptype || !value) continue;
		vlen = (u32) strlen(value);

		switch (ptype) {
		case GF_PROP_STRING_LIST:
			is_str_list = GF_TRUE;
		case GF_PROP_DATA:
		case GF_PROP_DATA_NO_COPY:
		case GF_PROP_CONST_DATA:
			obuf_size = (vlen+1)*2;
			if (obuf_alloc<obuf_size) {
				obuf = gf_realloc(obuf, obuf_size);
				obuf_alloc = obuf_size;
			}
			vlen = gf_base64_decode(value, vlen, obuf, obuf_size);
			if (is_str_list) {
				p = gf_props_parse_value(GF_PROP_STRING_LIST, pname, obuf, NULL, ',');
			} else {
				p.type = GF_PROP_DATA;
				p.value.data.ptr = obuf;
				p.value.data.size = vlen;
			}
			do_reset=GF_FALSE;
			break;
		default:
			if (p4cc==GF_PROP_PID_CODECID) {
				p.type = GF_PROP_UINT;
				p.value.uint = gf_codecid_parse(value);
			}
			//parse the property, based on its property type
			else if (p4cc==GF_PROP_PID_STREAM_TYPE) {
				p.type = GF_PROP_UINT;
				p.value.uint = gf_stream_type_by_name(value);
			} else {
				p = gf_props_parse_value(ptype, pname, value, NULL, ',');
			}
			break;
		}

		if (p4cc) {
			gf_filter_pid_set_property(opid, p4cc, &p);
		} else {
			gf_filter_pid_set_property_dyn(opid, (char *)pname, &p);
		}
		if (do_reset) gf_props_reset_single(&p);
	}
	if (obuf) gf_free(obuf);

	for (i=1; i<gf_list_count(st->opids); i++) {
		GF_FilterPid *a_opid = gf_list_get(st->opids, i);
		gf_filter_pid_copy_properties(a_opid, opid);
	}
	set_opids_props(ctx, st);
#endif
}

static void ghi_dmx_declare_opid_bin(GF_Filter *filter, GHIDmxCtx *ctx, GHIStream *st, GF_BitStream *bs)
{
	u32 i;
	if (!gf_list_count(st->opids)) {
		u32 i;
		for (i=0; i<st->mux_dst.nb_items; i++) {
			GF_FilterPid *opid = gf_filter_pid_new(filter);
			gf_filter_pid_set_udta(opid, st);
			gf_list_add(st->opids, opid);
		}
		if (!st->mux_dst.nb_items) {
			GF_FilterPid *opid = gf_filter_pid_new(filter);
			gf_filter_pid_set_udta(opid, st);
			gf_list_add(st->opids, opid);
		}
	}

	GF_FilterPid *opid = gf_list_get(st->opids, 0);
	gf_bs_seek(bs, st->props_offset);
	u32 end = st->props_offset + st->props_size;
	gf_bs_skip_bytes(bs, 4);

	//parse props
	while (1) {
		char *pname;
		u32 ptype;
		u32 pidx;
		Bool do_reset=GF_TRUE;
		GF_PropertyValue p;
		u32 p4cc = gf_bs_read_u32(bs);
		if (p4cc == 0xFFFFFFFF) break;
		if (!p4cc) {
			pname = gf_bs_read_utf8(bs);
			ptype = gf_bs_read_u32(bs);
		} else {
			ptype = gf_props_4cc_get_type(p4cc);
			if (!ptype) {
				//e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
		}
		p.type = ptype;
		switch (ptype) {
		case GF_PROP_SINT:
		case GF_PROP_UINT:
		case GF_PROP_4CC:
			p.value.uint = gf_bs_read_u32(bs);
			break;
		case GF_PROP_LSINT:
		case GF_PROP_LUINT:
			p.value.longuint = gf_bs_read_u64(bs);
			break;
		case GF_PROP_BOOL:
			p.value.boolean = gf_bs_read_u8(bs) ? 1 : 0;
			break;
		case GF_PROP_FRACTION:
			p.value.frac.num = gf_bs_read_u32(bs);
			p.value.frac.den = gf_bs_read_u32(bs);
			break;
		case GF_PROP_FRACTION64:
			p.value.lfrac.num = gf_bs_read_u64(bs);
			p.value.lfrac.den = gf_bs_read_u64(bs);
			break;
		case GF_PROP_FLOAT:
			p.value.fnumber = FLT2FIX( gf_bs_read_float(bs) );
			break;
		case GF_PROP_DOUBLE:
			p.value.number = gf_bs_read_double(bs);
			break;
		case GF_PROP_VEC2I:
			p.value.vec2i.x = gf_bs_read_u32(bs);
			p.value.vec2i.y = gf_bs_read_u32(bs);
			break;
		case GF_PROP_VEC2:
			p.value.vec2.x = gf_bs_read_double(bs);
			p.value.vec2.y = gf_bs_read_double(bs);
			break;
		case GF_PROP_VEC3I:
			p.value.vec3i.x = gf_bs_read_u32(bs);
			p.value.vec3i.y = gf_bs_read_u32(bs);
			p.value.vec3i.z = gf_bs_read_u32(bs);
			break;
		case GF_PROP_VEC4I:
			p.value.vec4i.x = gf_bs_read_u32(bs);
			p.value.vec4i.y = gf_bs_read_u32(bs);
			p.value.vec4i.z = gf_bs_read_u32(bs);
			p.value.vec4i.w = gf_bs_read_u32(bs);
			break;
		case GF_PROP_STRING:
		case GF_PROP_STRING_NO_COPY:
		case GF_PROP_NAME:
			p.value.string = gf_bs_read_utf8(bs);
			p.type = GF_PROP_STRING_NO_COPY;
			break;

		case GF_PROP_DATA:
		case GF_PROP_DATA_NO_COPY:
		case GF_PROP_CONST_DATA:
			p.value.data.size = gf_bs_read_u32(bs);
			if (p.value.data.size > gf_bs_available(bs)) {
				gf_bs_mark_overflow(bs, GF_FALSE);
				break;
			}
			p.value.data.ptr = gf_malloc(p.value.data.size);
			gf_bs_read_data(bs, p.value.data.ptr, p.value.data.size);
			p.type = GF_PROP_DATA_NO_COPY;
			do_reset = GF_FALSE;
			break;

		//string list: memory is ALWAYS duplicated
		case GF_PROP_STRING_LIST:
			p.value.string_list.nb_items = gf_bs_read_u32(bs);
			p.value.string_list.vals = gf_malloc(sizeof(char*) * p.value.string_list.nb_items);
			for (pidx=0; pidx<p.value.string_list.nb_items; pidx++) {
				p.value.string_list.vals[pidx] = gf_bs_read_utf8(bs);
			}
			do_reset = GF_FALSE;
			break;

		case GF_PROP_UINT_LIST:
		case GF_PROP_SINT_LIST:
		case GF_PROP_4CC_LIST:
			p.value.uint_list.nb_items = gf_bs_read_u32(bs);
			p.value.uint_list.vals = gf_malloc(sizeof(u32) * p.value.string_list.nb_items);
			for (pidx=0; pidx<p.value.uint_list.nb_items; pidx++) {
				p.value.uint_list.vals[pidx] = gf_bs_read_u32(bs);
			}
			break;
		case GF_PROP_VEC2I_LIST:
			p.value.v2i_list.nb_items = gf_bs_read_u32(bs);
			p.value.v2i_list.vals = gf_malloc(sizeof(u32) * p.value.string_list.nb_items);
			for (pidx=0; pidx<p.value.v2i_list.nb_items; pidx++) {
				p.value.v2i_list.vals[pidx].x = gf_bs_read_u32(bs);
				p.value.v2i_list.vals[pidx].y = gf_bs_read_u32(bs);
			}
			break;
		default:
			break;
		}

		if (p4cc) {
			gf_filter_pid_set_property(opid, p4cc, &p);
		} else {
			gf_filter_pid_set_property_dyn(opid, (char *)pname, &p);
			gf_free(pname);
		}
		if (do_reset) gf_props_reset_single(&p);

		if ((gf_bs_get_position(bs) > end) || gf_bs_is_overflow(bs)) {
			//e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}
	}

	//copy props to other
	for (i=1; i<gf_list_count(st->opids); i++) {
		GF_FilterPid *a_opid = gf_list_get(st->opids, i);
		gf_filter_pid_copy_properties(a_opid, opid);
	}
	set_opids_props(ctx, st);
}

static void ghi_dmx_unmark_muxed(GHIDmxCtx *ctx)
{
	u32 i;
	//unmark muxed inactive
	if (!ctx->mux.nb_items) return;
	if (!ctx->rep) return;
	switch (ctx->gm) {
	case GHI_GM_NONE:
	case GHI_GM_INIT:
		break;
	default:
		return;
	}

	for (i=0; i<gf_list_count(ctx->streams); i++) {
		GHIStream *st = gf_list_get(ctx->streams, i);
		if (!st->inactive) continue;
		if (!st->mux_dst.nb_items) continue;
		u32 j;
		st->active_mux_base_plus_one=0;
		for (j=0; j<st->mux_dst.nb_items; j++) {
			if (!strcmp(st->mux_dst.vals[j], ctx->rep)) {
				st->active_mux_base_plus_one=j+1;
				st->inactive = GF_FALSE;
				break;
			}
		}
	}
}

static Bool ghi_dmx_check_mux(GHIDmxCtx *ctx, GHIStream *st)
{
	//check muxed
	u32 k;
	for (k=0; k<ctx->mux.nb_items; k++) {
		char *sep = strchr(ctx->mux.vals[k], '@');
		if (!sep) continue;
		sep[0] = 0;
		if (strcmp(st->rep_id, ctx->mux.vals[k])) {
			sep[0] = '@';
			continue;
		}
		sep[0] = '@';
		sep++;

		GF_PropertyValue p = gf_props_parse_value(GF_PROP_STRING_LIST, "mux", sep, NULL, '@');
		st->mux_dst = p.value.string_list;
	}
	//mark if inactive
	switch (ctx->gm) {
	case GHI_GM_MAIN:
	case GHI_GM_ALL:
		return GF_FALSE;
	case GHI_GM_CHILD:
	case GHI_GM_INIT:
		if (strcmp(st->rep_id, ctx->rep)) {
			st->inactive = GF_TRUE;
		}
		return GF_FALSE;
	}
	if (!ctx->rep) return GF_FALSE;

	if (strcmp(st->rep_id, ctx->rep)) {
		st->inactive = GF_TRUE;
		return GF_TRUE; //do load seg info  !
	}
	return GF_TRUE;
}

void ghi_dmx_parse_seg(GHIDmxCtx *ctx, GF_BitStream *bs, GHIStream *st, s32 num_seg)
{
	if (!num_seg) {
		gf_bs_seek(bs, st->props_offset + st->props_size);
		num_seg = st->nb_segs;
	}

	while (1) {
		GHISegInfo *s;
		GF_SAFEALLOC(s, GHISegInfo);
		gf_list_add(st->segs_bin, s);

		if (st->rep_flags & 1) s->first_tfdt = gf_bs_read_u64(bs);
		else s->first_tfdt = gf_bs_read_u32(bs);

		if (st->rep_flags & (1<<1)) s->first_pck_seq = gf_bs_read_u64(bs);
		else s->first_pck_seq = gf_bs_read_u32(bs);

		s->seg_duration = gf_bs_read_u32(bs);

		if (st->rep_flags & (1<<2)) {
			if (st->rep_flags & (1<<3)) s->frag_start_offset = gf_bs_read_u64(bs);
			else s->frag_start_offset = gf_bs_read_u32(bs);
		}
		if (st->rep_flags & (1<<4)) {
			if (st->rep_flags & (1<<5)) s->frag_tfdt = gf_bs_read_u64(bs);
			else s->frag_tfdt = gf_bs_read_u32(bs);
		}
		if (st->rep_flags & (1<<6)) {
			s->split_first = gf_bs_read_u32(bs);
			s->split_last = gf_bs_read_u32(bs);
		}

		if (num_seg) {
			num_seg--;
			if (!num_seg) break;
		}
	}
}

GF_Err ghi_dmx_init_bin(GF_Filter *filter, GHIDmxCtx *ctx, GF_BitStream *bs)
{
	gf_bs_read_u32(bs);
	/*version*/gf_bs_read_u32(bs);
	ctx->segment_duration = gf_bs_read_u32(bs);
	ctx->max_segment_duration = gf_bs_read_u32(bs);
	ctx->media_presentation_duration = gf_bs_read_u64(bs);
	ctx->period_duration = gf_bs_read_u64(bs);
	ctx->segment_template = gf_bs_read_utf8(bs);

	u32 i, nb_reps = gf_bs_read_u32(bs);
	for (i=0; i<nb_reps; i++) {
		u32 skip, rep_end;
		u32 size=3; //3 32bit words
		GHIStream *st;
		GF_SAFEALLOC(st, GHIStream);
		if (!st) return GF_OUT_OF_MEM;
		gf_list_add(ctx->streams, st);

		st->opids = gf_list_new();
		if (!st->opids) return GF_OUT_OF_MEM;
		st->segs_bin = gf_list_new();
		if (!st->segs_bin) return GF_OUT_OF_MEM;

		u32 rep_start = (u32) gf_bs_get_position(bs);
		u32 rep_size = gf_bs_read_u32(bs);

		st->rep_id = gf_bs_read_utf8(bs);
		if (!st->rep_id) return GF_NON_COMPLIANT_BITSTREAM;
		st->res_url = gf_bs_read_utf8(bs);
		if (!st->res_url) return GF_NON_COMPLIANT_BITSTREAM;

		st->track_id = gf_bs_read_u32(bs);
		st->first_frag_start_offset = gf_bs_read_u32(bs);

		st->pid_timescale = gf_bs_read_u32(bs);
		st->mpd_timescale = gf_bs_read_u32(bs);
		st->bitrate = gf_bs_read_u32(bs);
		st->delay = gf_bs_read_u32(bs);
		st->sample_duration = gf_bs_read_u32(bs);
		st->first_cts_offset = (s32) gf_bs_read_u32(bs);
		st->nb_segs = gf_bs_read_u32(bs);
		st->starts_with_sap = gf_bs_read_u8(bs);
		st->rep_flags = gf_bs_read_u8(bs);
		gf_bs_read_u16(bs); //unused fo now

		//skip all props
		st->props_offset = (u32) gf_bs_get_position(bs);
		st->props_size = gf_bs_read_u32(bs);
		gf_bs_skip_bytes(bs, st->props_size-4);

		if (!ghi_dmx_check_mux(ctx, st)) {
			//skip all segs
			rep_end = rep_size + rep_start;
			skip = rep_end - (u32) gf_bs_get_position(bs);
			gf_bs_skip_bytes(bs, skip);
			continue;
		}

		if (st->rep_flags & 1) size += 1;
		if (st->rep_flags & (1<<1)) size += 1;
		if (st->rep_flags & (1<<2)) size += 1;
		if (st->rep_flags & (1<<3)) size += 1;
		if (st->rep_flags & (1<<4)) size += 1;
		if (st->rep_flags & (1<<5)) size += 1;
		if (st->rep_flags & (1<<6)) size += 2;

		//locate segment
		if (ctx->sn > st->nb_segs) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Invalid segment index %d - only %d segments available\n", ctx->sn, st->nb_segs));
			return GF_BAD_PARAM;
		//todo: locate by other criteria ? (time)
		} else {
			st->seg_num = ctx->sn;
		}

		size *= 4; //seg size in bytes
		//skip first n-1 segments
		skip = size * (st->seg_num-1);
		gf_bs_skip_bytes(bs, skip);
		ghi_dmx_parse_seg(ctx, bs, st, (st->seg_num==st->nb_segs) ? 1 : 2);

		GHISegInfo *surl = gf_list_get(st->segs_bin, 0);
		st->seg_info = *surl;
		st->nb_pck = -1;
		st->empty_seg = 0;
		if (st->seg_info.frag_start_offset)
			st->use_offsets=1;

		if (!st->seg_info.first_pck_seq) {
			st->empty_seg = 1;
		} else {
			surl = gf_list_get(st->segs_bin, 1);
			if (surl) {
				st->nb_pck = (u32) (surl->first_pck_seq - st->seg_info.first_pck_seq);
				if (!ctx->max_end_start_offset_plus_one
					|| (ctx->max_end_start_offset_plus_one-1 < surl->frag_start_offset)
				) {
					ctx->max_end_start_offset_plus_one = surl->frag_start_offset+1;
				}
				//first packet of next seg is split, we need this packet in current segment
				if (surl->split_first) st->nb_pck++;

				if (surl->frag_start_offset)
					st->use_offsets=1;
			}
		}

		//skip remaining segs
		rep_end = rep_size + rep_start;
		skip = rep_end - (u32) gf_bs_get_position(bs);
		gf_bs_skip_bytes(bs, skip);
	}
	return GF_OK;
}

GF_Err ghi_dmx_init_xml(GF_Filter *filter, GHIDmxCtx *ctx, const u8 *data)
{
#ifndef GPAC_DISABLE_MPD
	GF_MPD *mpd;
	GF_DOMParser *mpd_parser = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string(mpd_parser, (char*) data);

	if (e != GF_OK) {
		gf_xml_dom_del(mpd_parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Error in XML parsing %s\n", gf_error_to_string(e)));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	mpd = gf_mpd_new();
	e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), mpd, NULL);
	gf_xml_dom_del(mpd_parser);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Error in MPD creation %s\n", gf_error_to_string(e)));
		gf_mpd_del(mpd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	ctx->segment_duration = mpd->segment_duration;
	ctx->max_segment_duration = mpd->max_segment_duration;
	ctx->media_presentation_duration = mpd->media_presentation_duration;
	ctx->segment_template = mpd->segment_template;
	mpd->segment_template = NULL;
	GF_MPD_Period *period = gf_list_get(mpd->periods, 0);
	if (!period) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] No period in index file, aborting !\n"));
		gf_mpd_del(mpd);
		return GF_EOS;
	}
	ctx->period_duration = period->duration;

	u32 i;
	for (i=0; i<gf_list_count(period->adaptation_sets); i++) {
		u32 j;
		GF_MPD_AdaptationSet *as = gf_list_get(period->adaptation_sets, i);
		for (j=0; j<gf_list_count(as->representations); j++) {
			GF_MPD_Representation *rep = gf_list_get(as->representations, j);
			if (!rep->res_url) continue;
			GHIStream *st;
			GF_SAFEALLOC(st, GHIStream);
			st->rep_id = rep->id;
			rep->id = NULL;
			st->res_url = rep->res_url;
			rep->res_url = NULL;
			st->segs_xml = rep->segment_list->segment_URLs;
			rep->segment_list->segment_URLs = NULL;
			if (ctx->gm && !ctx->force) {
				st->x_children = rep->x_children;
				rep->x_children = NULL;
			}
			st->bitrate = rep->bandwidth;
			st->delay = rep->segment_list->pid_delay;

			st->track_id = rep->trackID;
			//mpd timescale
			st->pid_timescale = rep->segment_list->src_timescale;
			st->mpd_timescale = rep->segment_list->timescale;
			//in pid timescale units
			st->sample_duration = rep->segment_list->sample_duration;
			st->first_cts_offset = rep->segment_list->first_cts_offset;

			st->starts_with_sap = as->starts_with_sap ? as->starts_with_sap : rep->starts_with_sap;
			st->opids = gf_list_new();
			gf_list_add(ctx->streams, st);

			if (!ghi_dmx_check_mux(ctx, st))
				continue;

			//locate segment
			if (ctx->sn > gf_list_count(st->segs_xml)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Invalid segment index %d - only %d segments available\n", ctx->sn, gf_list_count(st->segs_xml)));
				gf_mpd_del(mpd);
				return GF_BAD_PARAM;
			//todo: locate by other criteria ? (time)
			} else {
				st->seg_num = ctx->sn;
			}

			GF_MPD_SegmentURL *surl = gf_list_get(st->segs_xml, st->seg_num-1);
			//in pid timescale units
			st->seg_info.first_tfdt = surl->first_tfdt;
			st->seg_info.first_pck_seq = surl->first_pck_seq;
			//in mpd timescale units
			st->seg_info.seg_duration = surl->duration;
			st->seg_info.frag_start_offset = surl->frag_start_offset;
			//in pid timescale units
			st->seg_info.frag_tfdt = surl->frag_tfdt;
			st->seg_info.split_first = surl->split_first_dur;
			st->seg_info.split_last = surl->split_last_dur;
			st->nb_pck = -1;
			st->empty_seg = 0;
			if (st->seg_info.frag_start_offset)
				st->use_offsets=1;

			if (!st->seg_info.first_pck_seq) {
				st->empty_seg = 1;
			} else {
				surl = gf_list_get(st->segs_xml, st->seg_num);
				if (surl) {
					st->nb_pck = (u32) (surl->first_pck_seq - st->seg_info.first_pck_seq);
					if (!ctx->max_end_start_offset_plus_one
						|| (ctx->max_end_start_offset_plus_one-1<surl->frag_start_offset)
					) {
						ctx->max_end_start_offset_plus_one = surl->frag_start_offset+1;
					}
					//first packet of next seg is split, we need this packet in current segment
					if (surl->split_first_dur) st->nb_pck++;
					if (surl->frag_start_offset)
						st->use_offsets=1;
				}
			}
			surl = gf_list_get(st->segs_xml, 0);
			st->first_frag_start_offset = (u32) surl->frag_start_offset;
		}
	}
	gf_mpd_del(mpd);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}
GF_Err ghi_dmx_init(GF_Filter *filter, GHIDmxCtx *ctx)
{
	u32 size, i, nb_active=0;
	GF_Err e;
	GF_BitStream *bs = NULL;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) return GF_OK;
	ctx->init = GF_TRUE;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	if (!data) {
		return GF_SERVICE_ERROR;
	}
	if ((data[0] == 'G') && (data[1] == 'H') && (data[2] == 'I') && (data[3] == 'D')) {
		bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
		e = ghi_dmx_init_bin(filter, ctx, bs);
		if (!e && gf_bs_is_overflow(bs)) e = GF_NON_COMPLIANT_BITSTREAM;
	} else {
		e = ghi_dmx_init_xml(filter, ctx, data);
	}
	if (e) {
		if (bs) gf_bs_del(bs);
		gf_filter_pid_drop_packet(ctx->ipid);
		gf_filter_setup_failure(filter, e);
		return e;
	}

	//unmark muxed inactive
	ghi_dmx_unmark_muxed(ctx);

	//declare pids
	for (i=0; i<gf_list_count(ctx->streams); i++) {
		GHIStream *st = gf_list_get(ctx->streams, i);

		u32 k;
		for (k=0; k<gf_list_count(ctx->streams); k++) {
			GHIStream *st_a = gf_list_get(ctx->streams, k);
			if (!strcmp(st_a->res_url, st->res_url)) {
				st->filter_src = st_a->filter_src;
				break;
			}
		}
		if (st->inactive) continue;
		nb_active++;
		if (st->filter_src) continue;

		//load all segments
		if (ctx->gm && bs)
			ghi_dmx_parse_seg(ctx, bs, st, 0);

		if (ctx->gm && !ctx->force) {
			if (st->segs_bin)
				ghi_dmx_declare_opid_bin(filter, ctx, st, bs);
			else
				ghi_dmx_declare_opid_xml(filter, ctx, st);
			continue;
		}
		const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_URL);
		char *args = gf_url_concatenate (p ? p->value.string : "./", st->res_url);

		if (st->first_frag_start_offset) {
			char szRange[100];
			char c = gf_filter_get_sep(filter, GF_FS_SEP_ARGS);
			sprintf(szRange, "%cgpac%crange=0-%u", c, c, st->first_frag_start_offset-1);
			gf_dynstrcat(&args, szRange, NULL);
		}

		if (!ctx->gm) {
			char szOpt[100];
			char c = gf_filter_get_sep(filter, GF_FS_SEP_ARGS);
			sprintf(szOpt, "%cgfopt%clightp%cindex=0", c, c, c);
			gf_dynstrcat(&args, szOpt, NULL);

			if (!ctx->min_ts_plus_one
				|| gf_timestamp_less(st->seg_info.first_tfdt+1, st->pid_timescale, ctx->min_ts_plus_one, ctx->min_ts_timescale)
			) {
				ctx->min_ts_plus_one = st->seg_info.first_tfdt+1;
				ctx->min_ts_timescale = st->pid_timescale;
			}

			if (st->use_offsets) {
				if (!ctx->min_offset_plus_one || (st->seg_info.frag_start_offset < ctx->min_offset_plus_one-1))
					ctx->min_offset_plus_one = st->seg_info.frag_start_offset+1;
			}
		}

		st->filter_src = gf_filter_connect_source(filter, args, NULL, GF_FALSE, &e);
		gf_free(args);
		if (!st->filter_src) {
			if (bs) gf_bs_del(bs);
			gf_filter_pid_drop_packet(ctx->ipid);

			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIXDmx] error locating source filter for rep %d name %s: %s\n", st->rep_id, st->res_url, gf_error_to_string(e) ));
			gf_filter_setup_failure(filter, e);
			return e;
		}
		gf_filter_set_source(filter, st->filter_src, NULL);

		gf_filter_set_setup_failure_callback(filter, st->filter_src, ghi_dmx_on_filter_setup_error, filter);
	}

	if (bs) gf_bs_del(bs);
	gf_filter_pid_drop_packet(ctx->ipid);
	gf_filter_pid_set_discard(ctx->ipid, GF_TRUE);

	if (nb_active) return GF_OK;
	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIXDmx] No ative representation to generate !\n"));
	return GF_BAD_PARAM;
}

GF_Err ghi_dmx_process(GF_Filter *filter)
{
	GHIDmxCtx *ctx = gf_filter_get_udta(filter);
	u32 i, count;
	GF_FilterPacket *pck;

	if (!ctx->init) {
		return ghi_dmx_init(filter, ctx);
	}

	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		GHIStream *st = gf_list_get(ctx->streams, i);
		if (st->inactive || !st->ipid || st->empty_seg) continue;

		GF_FilterPid *opid = gf_list_get(st->opids, 0);
		pck = gf_filter_pid_get_packet(st->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(st->ipid))
				gf_filter_pid_set_eos(opid);
			continue;
		}
		u64 dts = gf_filter_pck_get_dts(pck);
		if (dts >= st->seg_info.first_tfdt) {
			if (st->nb_pck) {
				st->nb_pck--;
				if (!st->first_sent || (!st->nb_pck && st->seg_info.split_last)) {
					GF_FilterPacket *dst = gf_filter_pck_new_ref(opid, 0, 0, pck);
					if (dst) {
						gf_filter_pck_merge_properties(pck, dst);
						if (!st->first_sent) {
							gf_filter_pck_set_property(dst, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
							if (st->seg_info.split_first) {
								gf_filter_pck_set_property(dst, GF_PROP_PCK_SPLIT_START, &PROP_UINT(st->seg_info.split_first));
								st->seg_info.split_first = 0;
							}
						}
						if (!st->nb_pck) {
							gf_filter_pck_set_property(dst, GF_PROP_PCK_SPLIT_END, &PROP_UINT(st->seg_info.split_last));
							st->seg_info.split_last = 0;
						}
						gf_filter_pck_send(dst);
					}
					st->first_sent = GF_TRUE;
				} else {
					gf_filter_pck_forward(pck, opid);
				}
				if (!st->nb_pck) {
					GF_FilterEvent evt;
					GF_FEVT_INIT(evt, GF_FEVT_STOP, st->ipid);
					gf_filter_pid_send_event(st->ipid, &evt);
					gf_filter_pid_set_eos(opid);
				}
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[GHIX] Packet dts "LLU" after segment range, discarding \n", dts));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[GHIX] Packet dts "LLU" before segment range, discarding \n", dts));
		}
		gf_filter_pid_drop_packet(st->ipid);
	}

	return GF_OK;
}

GF_Err ghi_dmx_initialize(GF_Filter *filter)
{
	GHIDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();

	if (ctx->sn && ctx->rep)
		ctx->gm = GHI_GM_NONE;

	if (!ctx->gm && !ctx->rep) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Missing rep ID, cannot generate manifest\n"));
		return GF_BAD_PARAM;
	}
	if ((ctx->gm>=GHI_GM_CHILD) && !ctx->rep) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Missing rep ID, cannot generating %s\n", (ctx->gm==GHI_GM_CHILD) ? "child manifest" : "init segment"));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

void ghi_dmx_finalize(GF_Filter *filter)
{
	GHIDmxCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		GHIStream *st = gf_list_pop_back(ctx->streams);
		GF_PropertyValue p;
		p.type = GF_PROP_STRING_LIST;
		p.value.string_list = st->mux_dst;
		gf_props_reset_single(&p);

		if (st->segs_bin) {
			while (gf_list_count(st->segs_bin)) {
				GHISegInfo *s = gf_list_pop_back(st->segs_bin);
				gf_free(s);
			}
			gf_list_del(st->segs_bin);
		}
#ifndef GPAC_DISABLE_MPD
		if (st->segs_xml) {
			while (gf_list_count(st->segs_xml)) {
				GF_MPD_SegmentURL *s = gf_list_pop_back(st->segs_xml);
				gf_mpd_segment_url_free(s);
			}
			gf_list_del(st->segs_xml);
		}
		if (st->x_children) {
			while (gf_list_count(st->x_children)) {
				GF_XMLNode *child = gf_list_pop_back(st->x_children);
				gf_xml_dom_node_del(child);
			}
			gf_list_del(st->x_children);
		}
#endif
		gf_list_del(st->opids);
		if (st->rep_id) gf_free(st->rep_id);
		if (st->res_url) gf_free(st->res_url);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	if (ctx->segment_template) gf_free(ctx->segment_template);

}


static const char *ghi_dmx_probe_data(const u8 *data, u32 data_size, GF_FilterProbeScore *score)
{
	if (data_size < 10) return NULL;
	if ((data[0] == 'G') && (data[1] == 'H') && (data[2] == 'I') && (data[3] == 'D')) {
		*score = GF_FPROBE_SUPPORTED;
		return "application/x-gpac-ghi";
	}
	return NULL;
}

#define OFFS(_n)	#_n, offsetof(GHIDmxCtx, _n)
static const GF_FilterArgs GHIDmxArgs[] =
{
	{ OFFS(gm), "manifest generation mode\n"
	"- none: no manifest generation (implied if sn is not 0)\n"
	"- all: generate all manifests and init segments\n"
	"- main: generate main manifest (MPD or master M3U8)\n"
	"- child: generate child playlist for HLS\n"
	"- init: generate init segment", GF_PROP_UINT, "main", "none|all|main|child|init", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(force), "force loading sources in manifest generation for debug", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(rep), "representation to generate", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(sn), "segment number to generate, 0 means init segment", GF_PROP_UINT, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mux), "representation to mux - cf filter help", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(out), "output filename to generate", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability GHIDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ghix"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-gpac-ghix|application/x-gpac-ghi"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//accept any stream but files, framed
	{ .code=GF_PROP_PID_STREAM_TYPE, .val.type=GF_PROP_UINT, .val.value.uint=GF_STREAM_FILE, .flags=(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_LOADED_FILTER) },
	{ .code=GF_PROP_PID_UNFRAMED, .val.type=GF_PROP_BOOL, .val.value.boolean=GF_TRUE, .flags=(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_LOADED_FILTER) },
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister GHIDXDmxRegister = {
	.name = "ghidmx",
	GF_FS_SET_DESCRIPTION("GHI demultiplexer")
	GF_FS_SET_HELP("This filter handles pre-indexed content for just-in-time processing of HAS manifest, init segment and segments\n"
	"\n"
	"Warning: This is work in progress, the format of indexes (binary or XML) may change until finalization of this filter.\n"
	"\n"
	"# Generating indexes\n"
	"Indexes are constructed using the dasher filter and a given segment duration.\n"
	"EX gpac -i SRC [... -i SRCn] -o index.ghi:segdur=2\n"
	"This constructs a binary index for the DASH session.\n"
	"EX gpac -i SRC -o index.ghix:segdur=2\n"
	"This constructs an XML index for the DASH session.\n"
	"\n"
	"Warning: XML indexes should only be used for debug purposes as they can take a significant amount of time to be parsed.\n"
	"\n"
	"When indexing, the default template used is `$Representation$_$Number$`. The template is stored in the index and shouldn't be re-specified when generating content.\n"
	"\n"
	"# Using indexes\n"
	"The index can be used to generate manifest, child variants for HLS, init segments and segments.\n"
	"EX gpac -i index.ghi:gm=all -o dash/vod.mpd\n"
	"This generates manifest(s) and init segment(s).\n"
	"\n"
	"EX gpac -i index.ghi:rep=FOO:sn=10 -o dash/vod.mpd\n"
	"This generates the 10th segment of representation with ID `FOO`.\n"
	"\n"
	"Note: The manifest file(s) and init segment(s) are not written when generating a segment. The manifest target (mpd or m3u8) is only used to setup the filter chain and target output path.\n"
	"\n"
	"EX gpac -i index.ghi:gm=main -o dash/vod.m3u8\n"
	"This generates main manifest only (MPD or master HLS playlist).\n"
	"\n"
	"EX gpac -i index.ghi:gm=child:rep=FOO:out=BAR -o dash/vod.m3u8\n"
	"This generates child manifest for representation `FOO` in file `BAR`.\n"
	"\n"
	"EX gpac -i index.ghi:gm=init:rep=FOO:out=BAR2 -o dash/vod.m3u8\n"
	"This generates init segment for representation `FOO` in file `BAR2`.\n"
	"\n"
	"The filter outputs are PIDs using framed packets marked with segment boundaries and can be chained to other filters before entering the dasher (e.g. for encryption, transcode...).\n"
	"\n"
	"If representation IDs are not assigned during index creation, they default to the 1-based index of the source. You can check them using:\n"
	"EX: `gpac -i src.ghi inspect:full`\n"
	"\n"
	"# Muxed Representations\n"
	"The filter can be used to generate muxed representations, either at manifest generation time or when generating a segment.\n"
	"EX gpac -i index.ghi:mux=A@V1@V2 -o dash/vod.mpd\n"
	"This will generate a manifest muxing representations `A` with representations `V1` and `V2`.\n"
	"\n"
	"EX gpac -i index.ghi:mux=A@V1@V2,T@V1@V2 -o dash/vod.mpd\n"
	"This will generate a manifest muxing representations `A` and `T` with representations `V1` and `V2`.\n"
	"\n"
	"EX gpac -i index.ghi:rep=V2:sn=5:mux=A@V2 -o dash/vod.mpd\n"
	"This will generate the 5th segment containing representations `A` and `V2`.\n"
	"\n"
	"The filter does not store any state, it is the user responsability to use consistent informations across calls:\n"
	"- do not change segment templates\n"
	"- do not change muxed representations to configurations not advertised in the generated manifests\n"
	"\n"
	"# Recommandations\n"
	"Indexing supports fragmented and non-fragmented MP4, MPEG-2 TS and seekable inputs.\n"
	"- It is recommended to use fragmented MP4 as input format since this greatly reduces file loading times.\n"
	"- If non-fragmented MP4 are used, it is recommended to use single-track files to decrease the movie box size and speedup parsing.\n"
	"- MPEG-2 TS sources will be slower since they require PES reframing and AU reformating, resulting in more IOs than with mp4.\n"
	"- other seekable sources will likely be slower (seeking, reframing) and are not recommended.\n"
	"\n"
	)
	.private_size = sizeof(GHIDmxCtx),
	.flags = GF_FS_REG_USE_SYNC_READ,
	.initialize = ghi_dmx_initialize,
	.finalize = ghi_dmx_finalize,
	.args = GHIDmxArgs,
	SETCAPS(GHIDmxCaps),
	.configure_pid = ghi_dmx_configure_pid,
	.process = ghi_dmx_process,
	.process_event = ghi_dmx_process_event,
	.probe_data = ghi_dmx_probe_data,
	//we accept as many input pids as loaded by the session
	.max_extra_pids = (u32) -1,
};

const GF_FilterRegister *ghidmx_register(GF_FilterSession *session)
{
	return &GHIDXDmxRegister;
}

