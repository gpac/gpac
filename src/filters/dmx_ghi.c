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
	GF_FilterPid *ipid;
	GF_List *opids;
	GF_MPD_SegmentURL *surl;
	u32 nb_pck;
	Bool first_sent;
	u32 seg_num;
	Bool inactive;
	u32 active_mux_base_plus_one;
	GF_PropStringList mux_dst;

	GF_MPD_Period *period;
	GF_Filter *filter_src;
	GF_MPD_AdaptationSet *src_as;
	GF_MPD_Representation *src_rep;
} GHIStream;

typedef struct
{
	//options
	u32 genman;
	Bool force;
	GF_PropStringList mux;
	char *rep;
	u32 sn;

	//internal
	GF_FilterPid *ipid;
	Bool init;
	GF_List *streams;

	GF_MPD *mpd;
} GHIDmxCtx;


static void set_opids_props(GHIDmxCtx *ctx, GHIStream *st)
{
	u32 i, count = gf_list_count(st->opids);
	for (i=0; i<count; i++) {
		GF_FilterPid *opid = gf_list_get(st->opids, i);

		if (st->ipid)
			gf_filter_pid_copy_properties(opid, st->ipid);

		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_DUR, &PROP_FRAC_INT(ctx->mpd->segment_duration, 1000));
		gf_filter_pid_set_property(opid, GF_PROP_PID_PERIOD_DUR, &PROP_FRAC64_INT(st->period->duration, 1000));
		gf_filter_pid_set_property(opid, GF_PROP_PID_URL, &PROP_STRING( gf_file_basename(st->src_rep->res_url)) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_FILEPATH, &PROP_STRING( gf_file_basename(st->src_rep->res_url)) );
		if (st->src_rep->trackID)
			gf_filter_pid_set_property(opid, GF_PROP_PID_ID, &PROP_UINT(st->src_rep->trackID) );

		if (ctx->genman) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_man"));
			gf_filter_pid_set_property_str(opid, "mpd_duration", &PROP_LONGUINT(ctx->mpd->media_presentation_duration));
			gf_filter_pid_set_property_str(opid, "max_seg_dur", &PROP_UINT(ctx->mpd->max_segment_duration));
			gf_filter_pid_set_property_str(opid, "start_with_sap", &PROP_UINT(st->src_as->starts_with_sap));
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("idx_seg"));
			gf_filter_pid_set_property(opid, GF_PROP_PID_START_NUMBER, &PROP_UINT(st->seg_num));
		}
		if (ctx->mpd->segment_template)
			gf_filter_pid_set_property_str(opid, "idx_template", &PROP_STRING(ctx->mpd->segment_template));

		if (st->src_rep->segment_list->sample_duration)
			gf_filter_pid_set_property(opid, GF_PROP_PID_CONSTANT_DURATION, &PROP_UINT(st->src_rep->segment_list->sample_duration));

		if (st->mux_dst.nb_items) {
			if (st->active_mux_base_plus_one) {
				gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(st->mux_dst.vals[st->active_mux_base_plus_one-1]));
			} else {
				gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(st->mux_dst.vals[i]));
			}
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(st->src_rep->id));
		}
		gf_filter_pid_set_name(opid, st->src_rep->id);
	}
}

static void ghi_dmx_send_seg_times(GHIDmxCtx *ctx, GHIStream *st, GF_FilterPid *opid)
{
	u32 i, count=gf_list_count(st->src_rep->segment_list->segment_URLs);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentURL *surl = gf_list_get(st->src_rep->segment_list->segment_URLs, i);
		GF_FilterPacket *pck = gf_filter_pck_new_alloc(opid, 0, NULL);
		gf_filter_pck_set_dts(pck, surl->first_tfdt);
		gf_filter_pck_set_cts(pck, surl->first_tfdt);
		gf_filter_pck_set_sap(pck, st->src_as->starts_with_sap);
		gf_filter_pck_set_duration(pck, surl->duration);
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
			if (!gf_filter_pid_is_filter_in_parents(pid, st->filter_src)) {
				st = NULL;
				continue;
			}
			if (!st->src_rep->trackID) break;
			if (st->src_rep->trackID == p_id->value.uint) break;
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
		if (ctx->genman) {
			ghi_dmx_send_seg_times(ctx, st, evt->base.on_pid);
			return GF_TRUE;
		}
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, st->ipid);
		fevt.base.on_pid = st->ipid;
		if (st->surl->frag_start_offset) {
			fevt.base.type = GF_FEVT_SOURCE_SEEK;
			fevt.seek.start_offset = st->surl->frag_start_offset;
			if (st->surl->frag_start_offset) {
				fevt.seek.hint_first_tfdt = st->surl->frag_tfdt ? st->surl->frag_tfdt : st->surl->first_tfdt;
			}
			GF_MPD_SegmentURL *next = gf_list_get(st->src_rep->segment_list->segment_URLs, st->seg_num);
			if (next)
				fevt.seek.end_offset = next->frag_start_offset-1;
			else
				fevt.seek.end_offset = 0;
		} else {
			fevt.play.start_range = st->surl->first_tfdt;
			fevt.play.start_range /= st->src_rep->segment_list->timescale;
			fevt.play.from_pck = st->surl->first_pck_seq;
			fevt.play.to_pck = st->surl->first_pck_seq + st->nb_pck;
		}
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

static void ghi_dmx_declare_opid(GF_Filter *filter, GHIDmxCtx *ctx, GHIStream *st)
{
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
	u32 i, nb_props = gf_list_count(st->src_rep->x_children);
	for (i=0; i<nb_props; i++) {
		Bool is_str_list=GF_FALSE;
		Bool do_reset=GF_TRUE;
		u32 j=0, p4cc=0, ptype=0, vlen, obuf_size;
		const char *pname=NULL;
		char *value=NULL;
		GF_XMLAttribute *att;
		GF_PropertyValue p;

		GF_XMLNode *n = gf_list_get(st->src_rep->x_children, i);
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
}

GF_Err ghi_dmx_init(GF_Filter *filter, GHIDmxCtx *ctx)
{
	u32 size;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) return GF_OK;
	const u8 *data = gf_filter_pck_get_data(pck, &size);
	if (!data) {
		return GF_SERVICE_ERROR;
	}
	GF_DOMParser *mpd_parser = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string(mpd_parser, (char*) data);

	gf_filter_pid_drop_packet(ctx->ipid);

	if (e != GF_OK) {
		gf_xml_dom_del(mpd_parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Error in XML parsing %s\n", gf_error_to_string(e)));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	ctx->mpd = gf_mpd_new();
	e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), ctx->mpd, NULL);
	gf_xml_dom_del(mpd_parser);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Error in MPD creation %s\n", gf_error_to_string(e)));
		gf_mpd_del(ctx->mpd);
		ctx->mpd = NULL;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	gf_filter_pid_drop_packet(ctx->ipid);
	ctx->init = GF_TRUE;
	GF_MPD_Period *period = gf_list_get(ctx->mpd->periods, 0);
	if (!period) return GF_EOS;

	u32 i;
	for (i=0; i<gf_list_count(period->adaptation_sets); i++) {
		u32 j;
		GF_MPD_AdaptationSet *as = gf_list_get(period->adaptation_sets, i);
		for (j=0; j<gf_list_count(as->representations); j++) {
			GF_MPD_Representation *rep = gf_list_get(as->representations, j);
			if (!rep->res_url) continue;
			GHIStream *st;
			GF_SAFEALLOC(st, GHIStream);
			st->src_rep = rep;
			st->src_as = as;
			st->period = period;
			st->opids = gf_list_new();
			gf_list_add(ctx->streams, st);

			//check muxed
			u32 k;
			for (k=0; k<ctx->mux.nb_items; k++) {
				char *sep = strchr(ctx->mux.vals[k], '.');
				if (!sep) continue;
				sep[0] = 0;
				if (strcmp(rep->id, ctx->mux.vals[k])) {
					sep[0] = '.';
					continue;
				}
				sep[0] = '.';
				sep++;

				GF_PropertyValue p = gf_props_parse_value(GF_PROP_STRING_LIST, "mux", sep, NULL, '.');
				st->mux_dst = p.value.string_list;
			}
			//mark if inactive
			if (ctx->genman) continue;
			if (!ctx->rep) continue;

			if (strcmp(st->src_rep->id, ctx->rep)) {
				st->inactive = GF_TRUE;
				continue;
			}
			//locate segment
			if (ctx->sn > gf_list_count(st->src_rep->segment_list->segment_URLs)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Invalid segment index %d - only %d segments available\n", ctx->sn, gf_list_count(st->src_rep->segment_list->segment_URLs)));

				gf_filter_setup_failure(filter, GF_BAD_PARAM);
				return GF_BAD_PARAM;
			//todo: locate by other criteria ? (time)
			} else {
				st->seg_num = ctx->sn;
			}
			st->surl = gf_list_get(st->src_rep->segment_list->segment_URLs, st->seg_num-1);
			st->nb_pck = -1;
			GF_MPD_SegmentURL *next = gf_list_get(st->src_rep->segment_list->segment_URLs, st->seg_num);
			if (next)
				st->nb_pck = next->first_pck_seq - st->surl->first_pck_seq;
		}
	}

	//unmark muxed inactive
	if (!ctx->genman && ctx->rep && ctx->mux.nb_items) {
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

	//declare pids
	for (i=0; i<gf_list_count(ctx->streams); i++) {
		GHIStream *st = gf_list_get(ctx->streams, i);

		u32 k;
		for (k=0; k<gf_list_count(ctx->streams); k++) {
			GHIStream *st_a = gf_list_get(ctx->streams, k);
			if (!strcmp(st_a->src_rep->res_url, st->src_rep->res_url)) {
				st->filter_src = st_a->filter_src;
				break;
			}
		}
		if (st->filter_src) continue;
		if (st->inactive) continue;

		if (ctx->genman && !ctx->force) {
			ghi_dmx_declare_opid(filter, ctx, st);
			continue;
		}
		const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_URL);
		char *args = gf_url_concatenate (p ? p->value.string : "./", st->src_rep->res_url);
		GF_MPD_SegmentURL *first_seg = gf_list_get(st->src_rep->segment_list->segment_URLs, 0);
		if (first_seg && first_seg->frag_start_offset) {
			char szRange[100];
			char c = gf_filter_get_sep(filter, GF_FS_SEP_ARGS);
			sprintf(szRange, "%cgpac%crange=0-"LLU, c, c, first_seg->frag_start_offset-1);
			gf_dynstrcat(&args, szRange, NULL);
		}

		st->filter_src = gf_filter_connect_source(filter, args, NULL, GF_FALSE, &e);
		gf_free(args);
		if (!st->filter_src) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIXDmx] error locating source filter for rep %d - mime type %s name %s: %s\n", st->src_rep->id, st->src_rep->mime_type, st->src_rep->res_url, gf_error_to_string(e) ));
			gf_filter_setup_failure(filter, e);
			return e;
		}
		gf_filter_set_source(filter, st->filter_src, NULL);

		gf_filter_set_setup_failure_callback(filter, st->filter_src, ghi_dmx_on_filter_setup_error, filter);
	}
	return GF_OK;
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
		if (st->inactive || !st->ipid) continue;

		GF_FilterPid *opid = gf_list_get(st->opids, 0);
		pck = gf_filter_pid_get_packet(st->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(st->ipid))
				gf_filter_pid_set_eos(opid);
			continue;
		}
		u64 dts = gf_filter_pck_get_dts(pck);
		if (dts >= st->surl->first_tfdt)
		{
			if (st->nb_pck) {
				st->nb_pck--;
				if (!st->first_sent) {
					GF_FilterPacket *dst = gf_filter_pck_new_ref(opid, 0, 0, pck);
					if (dst) {
						gf_filter_pck_merge_properties(pck, dst);
						gf_filter_pck_set_property(dst, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
						gf_filter_pck_send(dst);
					}
						st->first_sent = GF_TRUE;
				} else {
					gf_filter_pck_forward(pck, opid);
				}
			}
		}

		gf_filter_pid_drop_packet(st->ipid);
	}

	return GF_OK;
}

GF_Err ghi_dmx_initialize(GF_Filter *filter)
{
	GHIDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();

	if (!ctx->genman) {
		if (!ctx->rep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Missing rep ID and not generating manifest\n"));
			return GF_BAD_PARAM;
		}
		if (!ctx->sn) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[GHIX] Missing segment number and not generating manifest\n"));
			return GF_BAD_PARAM;
		}
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

		gf_list_del(st->opids);
		gf_free(st);
	}
	gf_list_del(ctx->streams);

	gf_mpd_del(ctx->mpd);
}

#define OFFS(_n)	#_n, offsetof(GHIDmxCtx, _n)
static const GF_FilterArgs GHIDmxArgs[] =
{
	{ OFFS(genman), "manifest generation mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(force), "force loading sources in manifest generation", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(rep), "representation to generate", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(sn), "segment number to generate", GF_PROP_UINT, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mux), "representation to mux - cf filter help", GF_PROP_STRING_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability GHIDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ghix"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/vnd.gpac.dash-index+xml"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
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
	GF_FS_SET_HELP("This filter handles pre-indexed content for HAS manifest, init segment and segments just-in-time processing\n"
	"\n"
	"Indexes must be constructed using the dasher filter and a given segment duration:\n"
	"EX gpac -i SRC -o index.ghix:segdur=2\n"
	"\n"
	"To generate manifest(s) and init segment(s) from the index, use:\n"
	"EX gpac -i index.ghix:genman -o dash/vod.mpd\n"
	"\n"
	"To generate the 10th segment of representation with ID `FOO`, use:\n"
	"EX gpac -i index.ghix:rep=FOO:sn=10 -o dash/vod.mpd\n"
	"Note that the manifest file(s) and init segment(s) are not written when generating a segment.\n"
	"The manifest target (mpd or m3u8) is only used to setup the filter chain and target output path.\n"
	"\n"
	"The filter ouputs are PIDs using framed packets amrked with segment boundaries and can be chained to other filters before entering the dasher (e.g. for encryption, transcode...).\n"
	"\n"
	"Muxed Representations\n"
	"The filter can be used to generate muxed representations, either at manifest generation time or when geenrating a segment.\n"
	"To generate a manifest muxing representations `A` with representations `V1` and `V2` use:\n"
	"EX gpac -i index.ghix:genman:mux=A-V1-V2 -o dash/vod.mpd\n"
	"\n"
	"To generate a manifest muxing representations `A` and `T` with representations `V1` and `V2` use:\n"
	"EX gpac -i index.ghix:genman:mux=A-V1-V2,T-V1-V2 -o dash/vod.mpd\n"
	"\n"
	"To generate the 5th segment containing representations `A` and `V2` use:\n"
	"EX gpac -i index.ghix:rep=V2:sn=5:mux=A-V2 -o dash/vod.mpd\n"
	"\n"
	"The filter does not store any state, it is the user respansability to use consistent informations across calls:\n"
	"- do not change segment templates\n"
	"- do not change muxed representations to configurations not advertised in the generated manifests\n"
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
	//we accept as many input pids as loaded by the session
	.max_extra_pids = (u32) -1,
};

const GF_FilterRegister *ghi_dmx_register(GF_FilterSession *session)
{
	return &GHIDXDmxRegister;
}

