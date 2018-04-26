/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-DASH/HLS segmenter
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
#include <gpac/iso639.h>
#include <gpac/internal/mpd.h>
#include <gpac/internal/media_dev.h>


typedef struct
{
	GF_List *streams;

	//period element we will fill
	GF_MPD_Period *period;
} GF_DasherPeriod;

enum
{
	DASHER_BS_SWITCH_DEF=0,
	DASHER_BS_SWITCH_OFF,
	DASHER_BS_SWITCH_ON,
	DASHER_BS_SWITCH_INBAND,
	DASHER_BS_SWITCH_FORCE,
	DASHER_BS_SWITCH_MULTI,
};

typedef struct
{
	u32 bs_switch, profile, cp, subs_per_sidx;
	s32 buf, timescale;
	Bool forcep, dynamic, single_file, single_segment, no_sar, mix_codecs, stl, tpl, no_seg_align, no_sap, no_frag_def, sidx;
	Double dur, fdur;
	char *avcp;
	char *hvcp;
	char *template;
	char *ext;
	char *profX;
	s32 asto;
	char *title, *source, *info, *cprt, *lang;
	GF_List *location, *base;
	Bool for_test, check_dur, skip_seg;

	//not yet exposed
	Bool mpeg2;

	//internal
	GF_FilterPid *opid;

	GF_MPD *mpd;

	GF_DasherPeriod *current_period, *next_period;
	GF_List *pids;

	Bool use_xlink, use_cenc, check_main_role;

	//options for muxers, constrained by profile
	Bool no_fragments_defaults;

	Bool is_eos;
	u32 nb_seg_url_pending;
	Bool on_demand_done;
} GF_DasherCtx;


typedef struct _dash_stream
{
	GF_FilterPid *ipid, *opid;

	//stream properties
	u32 codec_id, timescale, stream_type, dsi_crc, dsi_enh_crc, id, dep_id;
	GF_Fraction sar, fps;
	u32 width, height;
	u32 sr, nb_ch;
	const char *lang;
	Bool interlaced;
	const GF_PropertyValue *p_role;
	const GF_PropertyValue *p_period_desc;
	const GF_PropertyValue *p_as_desc;
	const GF_PropertyValue *p_as_any_desc;
	const GF_PropertyValue *p_rep_desc;
	const GF_PropertyValue *p_base_url;

	//TODO: get the values for all below
	u32 ch_layout, nb_surround, nb_lfe;

	GF_PropVec4i srd;
	u32 view_id;
	//end of TODO

	u32 bitrate;
	GF_DasherPeriod *period;

	char *period_id;
	Bool done;
	Bool seg_done;

	u32 nb_comp, nb_comp_done;

	u32 nb_rep, nb_rep_done;
	Double set_seg_duration;

	//repID for this stream
	char *rep_id;
	struct _dash_stream *share_rep;
	GF_List *complementary_reps;

	//the one and only representation element
	GF_MPD_Representation *rep;
	//the parent adaptation set
	GF_MPD_AdaptationSet *set;
	Bool owns_set;
	//set to true to use inband params
	Bool inband_params;
	GF_List *multi_pids;
	//in case we share the same init segment, we MUST use the same timescale
	u32 force_timescale;


	u32 startNumber, seg_number;
	Bool rep_init;
	u64 first_cts;

	//target MPD timescale
	u32 mpd_timescale;
	//segment start time in target MPD timescale
	u64 seg_start_time;
	Bool split_set_names;
	u64 max_period_dur;

	GF_Filter *dst_filter;

	const char *src_url;

	char *init_seg, *seg_template;
	u32 nb_sap_3, nb_sap_4;
	u32 pid_id;

	//seg urls not yet handled (waiting for size/index callbacks)
	GF_List *seg_urls;
	//next segment start time in this stream timescale (NOT MPD timescale)
	u64 next_seg_start;
	//adjusted next segment start time in this stream timescale (NOT MPD timescale)
	//the value is the same as next_seg_start until the end of segment is found (SAP)
	//in which case it is adjusted to the SAP time
	u64 adjusted_next_seg_start;

	//force representation time end in this stream timescale (NOT MPD timescale)
	u64 force_rep_end;

	Bool segment_started;
	u64 first_cts_in_seg;
	u64 first_cts_in_next_seg;
	//used for last segment computation of segmentTimeline
	u64 est_first_cts_in_next_seg;
} GF_DashStream;


static GF_DasherPeriod *dasher_new_period()
{
	GF_DasherPeriod *period;
	GF_SAFEALLOC(period, GF_DasherPeriod);
	period->streams = gf_list_new();
	return period;
}

static GF_Err dasher_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool period_switch = GF_FALSE;
	const GF_PropertyValue *p;
	u32 dc_crc, dc_enh_crc;
	GF_DashStream *ds;
	GF_DasherCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		return GF_OK;
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);

		//copy properties at init or reconfig
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		p = gf_filter_pid_caps_query(pid, GF_PROP_PID_FILE_EXT);
		if (p)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, p );
		else
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("mpd") );
		gf_filter_pid_set_name(ctx->opid, "manifest" );
	}

	ds = gf_filter_pid_get_udta(pid);
	if (!ds) {
		GF_SAFEALLOC(ds, GF_DashStream);
		ds->ipid = pid;
		gf_list_add(ctx->pids, ds);
		ds->complementary_reps = gf_list_new();
		period_switch = GF_TRUE;
		//don't create pid at this time
	}

#define CHECK_PROP(_type, _mem, _e) \
	p = gf_filter_pid_get_property(pid, _type); \
	if (!p && (_e<=0) ) return _e; \
	if (p && (p->value.uint != _mem) && _mem) period_switch = GF_TRUE; \
	if (p) _mem = p->value.uint; \

#define CHECK_PROP_FRAC(_type, _mem, _e) \
	p = gf_filter_pid_get_property(pid, _type); \
	if (!p && (_e<=0) ) return _e; \
	if (p && (p->value.frac.num * _mem.den != p->value.frac.den * _mem.num) && _mem.den && _mem.num) period_switch = GF_TRUE; \
	if (p) _mem = p->value.frac; \

#define CHECK_PROP_STR(_type, _mem, _e) \
	p = gf_filter_pid_get_property(pid, _type); \
	if (!p && (_e<=0) ) return _e; \
	if (p && _mem && !strcmp(_mem, p->value.string)) period_switch = GF_TRUE; \
	if (p) _mem = p->value.string; \

	CHECK_PROP(GF_PROP_PID_CODECID, ds->codec_id, GF_NOT_SUPPORTED)
	CHECK_PROP(GF_PROP_PID_STREAM_TYPE, ds->stream_type, GF_NOT_SUPPORTED)
	CHECK_PROP(GF_PROP_PID_TIMESCALE, ds->timescale, GF_NOT_SUPPORTED)
	CHECK_PROP(GF_PROP_PID_BITRATE, ds->bitrate, GF_EOS)

	if (ds->stream_type==GF_STREAM_VISUAL) {
		CHECK_PROP(GF_PROP_PID_WIDTH, ds->width, GF_OK)
		CHECK_PROP(GF_PROP_PID_HEIGHT, ds->height, GF_OK)
		//don't return if not defined
		CHECK_PROP_FRAC(GF_PROP_PID_SAR, ds->sar, GF_EOS)
		if (!ds->sar.num) ds->sar.num = ds->sar.den = 1;
		CHECK_PROP_FRAC(GF_PROP_PID_FPS, ds->fps, GF_EOS)
	} else if (ds->stream_type==GF_STREAM_AUDIO) {
		CHECK_PROP(GF_PROP_PID_SAMPLE_RATE, ds->sr, GF_OK)
		CHECK_PROP(GF_PROP_PID_NUM_CHANNELS, ds->nb_ch, GF_OK)
		CHECK_PROP(GF_PROP_PID_CHANNEL_LAYOUT, ds->ch_layout, GF_EOS)
	}
	CHECK_PROP_STR(GF_PROP_PERIOD_ID, ds->period_id, GF_EOS)

	CHECK_PROP(GF_PROP_PID_ID, ds->id, GF_EOS)
	CHECK_PROP(GF_PROP_PID_DEPENDENCY_ID, ds->dep_id, GF_EOS)

	CHECK_PROP_STR(GF_PROP_PID_URL, ds->src_url, GF_EOS)


	dc_crc = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) dc_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
	dc_enh_crc = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
	if (p) dc_enh_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);

	if (((dc_crc != ds->dsi_crc) && ds->dsi_crc)
		|| ((dc_enh_crc != ds->dsi_enh_crc) && ds->dsi_enh_crc)
	) {
		//check which codecs can support inband param sets
		switch (ds->codec_id) {
		case GF_CODECID_AVC:
		case GF_CODECID_SVC:
		case GF_CODECID_MVC:
		case GF_CODECID_HEVC:
		case GF_CODECID_LHVC:
			if (!ctx->bs_switch)
				period_switch = GF_TRUE;
			break;
		default:
			period_switch = GF_TRUE;
			break;
		}
	}
	ds->dsi_crc = dc_crc;
	//our stream is already scheduled for next period, don't do anything
	if (gf_list_find(ctx->next_period->streams, ds)>=0)
		period_switch = GF_FALSE;

	//assign default ID
	if (!ds->period_id)
		ds->period_id = "_gpac_dasher_default_period_id";

	//other props
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
	if (p) ds->lang = p->value.string;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_INTERLACED);
	if (p) ds->interlaced = p->value.boolean;
	else ds->interlaced = GF_FALSE;

	ds->p_role = gf_filter_pid_get_property(pid, GF_PROP_PID_ROLE);
	ds->p_period_desc = gf_filter_pid_get_property(pid, GF_PROP_PID_PERIOD_DESC);
	ds->p_as_desc = gf_filter_pid_get_property(pid, GF_PROP_PID_AS_COND_DESC);
	ds->p_as_any_desc = gf_filter_pid_get_property(pid, GF_PROP_PID_AS_ANY_DESC);
	ds->p_rep_desc = gf_filter_pid_get_property(pid, GF_PROP_PID_REP_DESC);
	ds->p_base_url = gf_filter_pid_get_property(pid, GF_PROP_PID_BASE_URL);

	if (!period_switch) return GF_OK;
	if (period_switch) {
		gf_list_del_item(ctx->current_period->streams, ds);
		gf_list_add(ctx->next_period->streams, ds);
		ds->period = ctx->next_period;
	}
	return GF_OK;
}

static GF_Err dasher_update_mpd(GF_DasherCtx *ctx)
{
	char profiles_string[GF_MAX_PATH];
	GF_XMLAttribute *cenc_att = NULL;
	GF_XMLAttribute *xlink_att = NULL;
	GF_XMLAttribute *prof_att = NULL;

	u32 i, count=gf_list_count(ctx->mpd->attributes);
	for (i=0; i<count; i++) {
		GF_XMLAttribute * att = gf_list_get(ctx->mpd->attributes, i);
		if (!strcmp(att->name, "profiles")) prof_att = att;
		if (!strcmp(att->name, "xmlns:cenc")) cenc_att = att;
		if (!strcmp(att->name, "xmlns:xlink")) xlink_att = att;

	}
	if (ctx->dynamic) {
		ctx->mpd->type = GF_MPD_TYPE_DYNAMIC;
		//TODO
//		ctx->mpd->availabilityStartTime = cfg_get_key_ms(dasher->dash_ctx, "DASH", "GenerationNTP");
	} else {
		ctx->mpd->type = GF_MPD_TYPE_STATIC;
	}

	if (ctx->profile==GF_DASH_PROFILE_LIVE) {
		if (ctx->use_xlink && !ctx->mpeg2) {
			strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-ext-live:2014");
		} else {
			sprintf(profiles_string, "urn:mpeg:dash:profile:%s:2011", ctx->mpeg2 ? "mp2t-simple" : "isoff-live");
		}
	} else if (ctx->profile==GF_DASH_PROFILE_ONDEMAND) {
		if (ctx->use_xlink) {
			strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-ext-on-demand:2014");
		} else {
			strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-on-demand:2011");
		}
	} else if (ctx->profile==GF_DASH_PROFILE_MAIN) {
		sprintf(profiles_string, "urn:mpeg:dash:profile:%s:2011", ctx->mpeg2 ? "mp2t-main" : "isoff-main");
	} else if (ctx->profile==GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE) {
		strcpy(profiles_string, "urn:hbbtv:dash:profile:isoff-live:2012");
	} else if (ctx->profile==GF_DASH_PROFILE_AVC264_LIVE) {
		strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264");
	} else if (ctx->profile==GF_DASH_PROFILE_AVC264_ONDEMAND) {
		strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-on-demand:2011,http://dashif.org/guidelines/dash264");
	} else {
		strcpy(profiles_string, "urn:mpeg:dash:profile:full:2011");
	}

	if (ctx->profX) {
		char profiles_w_ext[256];
		sprintf(profiles_w_ext, "%s,%s", profiles_string, ctx->profX);
		if (!prof_att) {
			prof_att = gf_xml_dom_create_attribute("profiles", profiles_w_ext);
			gf_list_add(ctx->mpd->attributes, prof_att);
		} else {
			if (prof_att->value) gf_free(prof_att->value);
			prof_att->value = gf_strdup(profiles_w_ext);
		}
	} else {
		if (!prof_att) {
			prof_att = gf_xml_dom_create_attribute("profiles", profiles_string);
			gf_list_add(ctx->mpd->attributes, prof_att);
		} else {
			if (prof_att->value) gf_free(prof_att->value);
			prof_att->value = gf_strdup(profiles_string);
		}
	}

	if (ctx->use_cenc && !cenc_att) {
		cenc_att = gf_xml_dom_create_attribute("xmlns:cenc", "urn:mpeg:cenc:2013");
		gf_list_add(ctx->mpd->attributes, cenc_att);
	}
	if (ctx->use_xlink && !xlink_att) {
		xlink_att = gf_xml_dom_create_attribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
		gf_list_add(ctx->mpd->attributes, xlink_att);
	}
	return GF_OK;
}
static GF_Err dasher_setup_mpd(GF_DasherCtx *ctx)
{
	u32 i, count;
	ctx->mpd = gf_mpd_new();
	ctx->mpd->xml_namespace = "urn:mpeg:dash:schema:mpd:2011";
	ctx->mpd->base_URLs = gf_list_new();
	ctx->mpd->locations = gf_list_new();
	ctx->mpd->program_infos = gf_list_new();
	ctx->mpd->periods = gf_list_new();
	ctx->mpd->attributes = gf_list_new();
	if (ctx->buf<0) {
		s32 buf = -ctx->buf;
		ctx->mpd->min_buffer_time = ctx->dur*10 * buf; //*1000 (ms) / 100 (percent)
	}
	else ctx->mpd->min_buffer_time = ctx->buf;

	if (ctx->for_test)
		ctx->mpd->force_test_mode = GF_TRUE;

	if (ctx->title || ctx->cprt || ctx->info || ctx->source) {
		GF_MPD_ProgramInfo *info;
		GF_SAFEALLOC(info, GF_MPD_ProgramInfo);
		gf_list_add(ctx->mpd->program_infos, info);
		if (ctx->title)
			info->title = gf_strdup(ctx->title);
		else {
			char tmp[256];
			sprintf(tmp,"MPD file generated by GPAC");
			info->title = gf_strdup(tmp);
		}
		if (ctx->cprt) info->copyright = gf_strdup(ctx->cprt);
		if (ctx->info) info->more_info_url = gf_strdup(ctx->info);
		else info->more_info_url = gf_strdup("http://gpac.io");
		if (ctx->source) info->source = gf_strdup(ctx->source);
		if (ctx->lang) info->lang = gf_strdup(ctx->lang);
	}
	count = ctx->location ? gf_list_count(ctx->location) : 0;
	for (i=0; i<count; i++) {
		char *l = gf_list_get(ctx->location, i);
		gf_list_add(ctx->mpd->locations, gf_strdup(l));
	}
	count = ctx->base ? gf_list_count(ctx->base) : 0;
	for (i=0; i<count; i++) {
		GF_MPD_BaseURL *base;
		char *b = gf_list_get(ctx->base, i);
		GF_SAFEALLOC(base, GF_MPD_BaseURL);
		base->URL = gf_strdup(b);
		gf_list_add(ctx->mpd->base_URLs, base);
	}
	return dasher_update_mpd(ctx);
}


static u32 dasher_cicp_get_channel_config(u32 nb_chan,u32 nb_surr, u32 nb_lfe)
{
	if ( !nb_chan && !nb_surr && !nb_lfe) return 0;
	else if ((nb_chan==1) && !nb_surr && !nb_lfe) return 1;
	else if ((nb_chan==2) && !nb_surr && !nb_lfe) return 2;
	else if ((nb_chan==3) && !nb_surr && !nb_lfe) return 3;
	else if ((nb_chan==3) && (nb_surr==1) && !nb_lfe) return 4;
	else if ((nb_chan==3) && (nb_surr==2) && !nb_lfe) return 5;
	else if ((nb_chan==3) && (nb_surr==2) && (nb_lfe==1)) return 6;
	else if ((nb_chan==5) && (nb_surr==0) && (nb_lfe==1)) return 6;

	else if ((nb_chan==5) && (nb_surr==2) && (nb_lfe==1)) return 7;
	else if ((nb_chan==2) && (nb_surr==1) && !nb_lfe) return 9;
	else if ((nb_chan==2) && (nb_surr==2) && !nb_lfe) return 10;
	else if ((nb_chan==3) && (nb_surr==3) && (nb_lfe==1)) return 11;
	else if ((nb_chan==3) && (nb_surr==4) && (nb_lfe==1)) return 12;
	else if ((nb_chan==11) && (nb_surr==11) && (nb_lfe==2)) return 13;
	else if ((nb_chan==5) && (nb_surr==2) && (nb_lfe==1)) return 14;
	else if ((nb_chan==5) && (nb_surr==5) && (nb_lfe==2)) return 15;
	else if ((nb_chan==5) && (nb_surr==4) && (nb_lfe==1)) return 16;
	else if ((nb_surr==5) && (nb_lfe==1) && (nb_chan==6)) return 17;
	else if ((nb_surr==7) && (nb_lfe==1) && (nb_chan==6)) return 18;
	else if ((nb_chan==5) && (nb_surr==6) && (nb_lfe==1)) return 19;
	else if ((nb_chan==7) && (nb_surr==6) && (nb_lfe==1)) return 20;

	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("Unkown CICP mapping for channel config %d/%d.%d\n", nb_chan, nb_surr, nb_lfe));
	return 0;
}

static GF_Err dasher_get_rfc_6381_codec_name(GF_DasherCtx *ctx, GF_DashStream *ds, char *szCodec, Bool force_inband, Bool force_sbr)
{
	u32 subtype=0;
	const GF_PropertyValue *dcd, *dcd_enh;

	dcd = gf_filter_pid_get_property(ds->ipid, GF_PROP_PID_DECODER_CONFIG);
	dcd_enh = gf_filter_pid_get_property(ds->ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	switch (ds->codec_id) {
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		if (dcd && dcd->value.data.ptr) {
			/*5 first bits of AAC config*/
			u8 audio_object_type = (dcd->value.data.ptr[0] & 0xF8) >> 3;
#ifndef GPAC_DISABLE_AV_PARSERS
			if (force_sbr && (audio_object_type==2) ) {
				GF_M4ADecSpecInfo a_cfg;
				GF_Err e = gf_m4a_get_config(dcd->value.data.ptr, dcd->value.data.size, &a_cfg);
				if (e==GF_OK) {
					if (a_cfg.sbr_sr)
						audio_object_type = a_cfg.sbr_object_type;
					if (a_cfg.has_ps)
						audio_object_type = 29;
				}
			}
#endif
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4a.%02X.%01d", ds->codec_id, audio_object_type);
		} else {
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4a.%02X", ds->codec_id);
		}
		break;
	case GF_CODECID_MPEG4_PART2:
#ifndef GPAC_DISABLE_AV_PARSERS
		if (dcd) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(dcd->value.data.ptr, dcd->value.data.size, &dsi);
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4v.%02X.%01x", ds->codec_id, dsi.VideoPL);
		} else
#endif
		{
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4v.%02X", ds->codec_id);
		}
		break;
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		if (dcd_enh) dcd = dcd_enh;
		subtype = (ds->codec_id==GF_CODECID_SVC) ? GF_ISOM_SUBTYPE_SVC_H264 : GF_ISOM_SUBTYPE_MVC_H264;
	case GF_CODECID_AVC:
		if (!subtype) {
			if (force_inband) {
				subtype = dcd_enh ? GF_ISOM_SUBTYPE_AVC4_H264 : GF_ISOM_SUBTYPE_AVC3_H264;
			} else {
				subtype = dcd_enh ? GF_ISOM_SUBTYPE_AVC2_H264 : GF_ISOM_SUBTYPE_AVC_H264;
			}
		}

		if (dcd && (!ctx->forcep || !ctx->avcp) ) {
			GF_AVCConfig *avcc = gf_odf_avc_cfg_read(dcd->value.data.ptr, dcd->value.data.size);
			if (avcc) {
				snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.%02X%02X%02X", gf_4cc_to_str(subtype), avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
				gf_odf_avc_cfg_del(avcc);
				return GF_OK;
			}
		}
		if (ctx->avcp)
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.%s", gf_4cc_to_str(subtype), ctx->avcp);
		else
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
		if (!ctx->forcep) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[Dasher] Cannot find AVC config, using default %s\n", szCodec));
		}
		return GF_OK;
#ifndef GPAC_DISABLE_HEVC
	case GF_CODECID_LHVC:
		if (dcd_enh) dcd = dcd_enh;
		subtype = force_inband ? GF_ISOM_SUBTYPE_LHE1 : GF_ISOM_SUBTYPE_LHV1;
	case GF_CODECID_HEVC:
		if (!subtype) {
			if (dcd_enh) {
				subtype = force_inband ? GF_ISOM_SUBTYPE_HEV2 : GF_ISOM_SUBTYPE_HVC2;
			} else {
				subtype = force_inband ? GF_ISOM_SUBTYPE_HEV1 : GF_ISOM_SUBTYPE_HVC1;
			}
		}
		if (dcd && (!ctx->forcep || !ctx->hvcp)) {
			u8 c;
			char szTemp[RFC6381_CODEC_NAME_SIZE_MAX];
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(dcd->value.data.ptr, dcd->value.data.size, (dcd==dcd_enh) ? GF_TRUE : GF_FALSE);
			//TODO - check we do expose hvcC for tiled tracks !

			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.", gf_4cc_to_str(subtype));
			if (hvcc->profile_space==1) strcat(szCodec, "A");
			else if (hvcc->profile_space==2) strcat(szCodec, "B");
			else if (hvcc->profile_space==3) strcat(szCodec, "C");
			//profile idc encoded as a decimal number
			sprintf(szTemp, "%d", hvcc->profile_idc);
			strcat(szCodec, szTemp);
			//general profile compatibility flags: hexa, bit-reversed
			{
				u32 val = hvcc->general_profile_compatibility_flags;
				u32 i, res = 0;
				for (i=0; i<32; i++) {
					res |= val & 1;
					if (i==31) break;
					res <<= 1;
					val >>=1;
				}
				sprintf(szTemp, ".%X", res);
				strcat(szCodec, szTemp);
			}

			if (hvcc->tier_flag) strcat(szCodec, ".H");
			else strcat(szCodec, ".L");
			sprintf(szTemp, "%d", hvcc->level_idc);
			strcat(szCodec, szTemp);

			c = hvcc->progressive_source_flag << 7;
			c |= hvcc->interlaced_source_flag << 6;
			c |= hvcc->non_packed_constraint_flag << 5;
			c |= hvcc->frame_only_constraint_flag << 4;
			c |= (hvcc->constraint_indicator_flags >> 40);
			sprintf(szTemp, ".%X", c);
			strcat(szCodec, szTemp);
			if (hvcc->constraint_indicator_flags & 0xFFFFFFFF) {
				c = (hvcc->constraint_indicator_flags >> 32) & 0xFF;
				sprintf(szTemp, ".%X", c);
				strcat(szCodec, szTemp);
				if (hvcc->constraint_indicator_flags & 0x00FFFFFF) {
					c = (hvcc->constraint_indicator_flags >> 24) & 0xFF;
					sprintf(szTemp, ".%X", c);
					strcat(szCodec, szTemp);
					if (hvcc->constraint_indicator_flags & 0x0000FFFF) {
						c = (hvcc->constraint_indicator_flags >> 16) & 0xFF;
						sprintf(szTemp, ".%X", c);
						strcat(szCodec, szTemp);
						if (hvcc->constraint_indicator_flags & 0x000000FF) {
							c = (hvcc->constraint_indicator_flags >> 8) & 0xFF;
							sprintf(szTemp, ".%X", c);
							strcat(szCodec, szTemp);
							c = (hvcc->constraint_indicator_flags ) & 0xFF;
							sprintf(szTemp, ".%X", c);
							strcat(szCodec, szTemp);
						}
					}
				}
			}
			gf_odf_hevc_cfg_del(hvcc);
			return GF_OK;
		}

		if (ctx->hvcp)
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.%s", gf_4cc_to_str(subtype), ctx->hvcp);
		else
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
		if (!ctx->forcep) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[Dasher] Cannot find HEVC config, using default %s\n", szCodec));
		}
		return GF_OK;
#endif

	default:
		subtype = gf_codecid_4cc_type(ds->codec_id);
		if (!subtype) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[Dasher] codec parameters not known, cannot set codec string\n" ));
			strcpy(szCodec, "unkn");
			return GF_OK;
		}
		if (ds->codec_id<GF_CODECID_LAST_MPEG4_MAPPING) {
			if (ds->stream_type==GF_STREAM_VISUAL) {
				snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4v.%02X", ds->codec_id);
			} else if (ds->stream_type==GF_STREAM_AUDIO) {
				snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4a.%02X", ds->codec_id);
			} else {
				snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4s.%02X", ds->codec_id);
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[Dasher] codec parameters not known - setting codecs string to default value \"%s\"\n", gf_4cc_to_str(subtype) ));
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
		}
	}
	return GF_OK;
}

static void dasher_setup_rep(GF_DasherCtx *ctx, GF_DashStream *ds)
{
	char szCodec[RFC6381_CODEC_NAME_SIZE_MAX];
	const GF_PropertyValue *p;
	assert(ds->rep==NULL);
	ds->rep = gf_mpd_representation_new();
	ds->rep->playback.udta = ds;
	ds->startNumber = 1;

	ds->rep->bandwidth = ds->bitrate;
	if (ds->stream_type==GF_STREAM_VISUAL) {
		ds->rep->width = ds->width;
		ds->rep->height = ds->height;
		ds->rep->mime_type = gf_strdup("video/mp4");
	}
	else if (ds->stream_type==GF_STREAM_AUDIO) {
		GF_MPD_Descriptor *desc;
		char value[256];
		ds->rep->samplerate = ds->sr;
		if (!ds->nb_surround && !ds->nb_lfe) {
			sprintf(value, "%d", ds->nb_ch);
			desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:dash:23003:3:audio_channel_configuration:2011", value);
		} else {
			sprintf(value, "%d", dasher_cicp_get_channel_config(ds->nb_ch, ds->nb_surround, ds->nb_lfe));
			desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:mpegB:cicp:ChannelConfiguration", value);
		}
		gf_list_add(ds->rep->audio_channels, desc);
		ds->rep->mime_type = gf_strdup("audio/mp4");
	} else {
		ds->rep->mime_type = gf_strdup("application/mp4");
	}
	dasher_get_rfc_6381_codec_name(ctx, ds, szCodec, (ctx->bs_switch==DASHER_BS_SWITCH_INBAND) ? GF_TRUE : GF_FALSE, GF_TRUE);
	ds->rep->codecs = gf_strdup(szCodec);

	p = gf_filter_pid_get_property(ds->ipid, GF_PROP_REPRESENTATION_ID);
	if (p) {
		if (ds->rep_id) gf_free(ds->rep_id);
		ds->rep_id = gf_strdup(p->value.string);
	} else if (!ds->rep_id) {
		char szRepID[20];
		sprintf(szRepID, "%d", 1 + gf_list_find(ctx->pids, ds));
		ds->rep_id = gf_strdup(szRepID);
	}
	ds->rep->id = gf_strdup(ds->rep_id);

	if (ds->interlaced) ds->rep->scan_type = GF_MPD_SCANTYPE_INTERLACED;
}


static Bool dasher_same_roles(GF_DashStream *ds1, GF_DashStream *ds2)
{
	GF_List *list;
	if (ds1->p_role && ds2->p_role) {
		if (gf_props_equal(ds1->p_role, ds2->p_role)) return GF_TRUE;
	}
	if (!ds1->p_role && !ds2->p_role)
		return GF_TRUE;

	//special case, if one is set and the other is not, compare with "main" role
	list = ds2->p_role ?  ds2->p_role->value.string_list : ds1->p_role->value.string_list;
	if (gf_list_count(list)==1) {
		char *s = gf_list_get(list, 0);
		if (!strcmp(s, "main")) return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool dasher_same_adaptation_set(GF_DasherCtx *ctx, GF_DashStream *ds, GF_DashStream *ds_test)
{
	//muxed representations
	if (ds_test->share_rep == ds) return GF_TRUE;
	//otherwise we have to be of same type
	if (ds->stream_type != ds_test->stream_type) return GF_FALSE;

	//not the same roles
	if (!dasher_same_roles(ds, ds_test)) return GF_FALSE;

	/* if two inputs don't have the same (number and value) as_desc they don't belong to the same AdaptationSet
	   (use c_as_desc for AdaptationSet descriptors common to all inputs in an AS) */
	if (! gf_props_equal(ds->p_as_desc, ds_test->p_as_desc))
		return GF_FALSE;

	if (ds->srd.x != ds_test->srd.x) return GF_FALSE;
	if (ds->srd.y != ds_test->srd.y) return GF_FALSE;
	if (ds->srd.z != ds_test->srd.z) return GF_FALSE;
	if (ds->srd.w != ds_test->srd.w) return GF_FALSE;

	if (ds->view_id != ds_test->view_id) return GF_FALSE;
	//according to DASH spec mixing interlaced and progressive is OK
	//if (ds->interlaced != ds_test->interlaced) return GF_FALSE;
	if (ds->nb_ch != ds_test->nb_ch) return GF_FALSE;
	if (ds->lang != ds_test->lang) return GF_FALSE;

	if (ds->stream_type==GF_STREAM_VISUAL) {
		u32 w, h, tw, th;
		if (ctx->no_sar) {
			w = ds->width;
			h = ds->height;
			tw = ds_test->width;
			th = ds_test->height;
		} else {
			w = ds->width * ds->sar.num;
			h = ds->height * ds->sar.den;
			tw = ds_test->width * ds_test->sar.num;
			th = ds_test->height * ds_test->sar.den;
		}

		//not the same aspect ratio
		if (w * th != h * tw)
			return GF_FALSE;
	} else if (ds->stream_type==GF_STREAM_AUDIO) {
		if (!ctx->mix_codecs && (ds->codec_id != ds_test->codec_id) )
			return GF_FALSE;
		//we allow mix of channels config
	} else {
		if (!ctx->mix_codecs && strcmp(ds->rep->codecs, ds_test->rep->codecs)) return GF_FALSE;
		return GF_TRUE;
	}
	//ok, we are video or audio with mixed codecs
	if (ctx->mix_codecs) return GF_TRUE;
	//we need dependencies
	if (ds_test->dep_id && gf_list_find(ds->complementary_reps, ds_test->rep) < 0)
		return GF_FALSE;
	//we should be good
	return GF_TRUE;
}

static void dasher_add_descriptors(GF_List **p_dst_list, const GF_PropertyValue *desc_val)
{
	u32 j, count;
	GF_List *dst_list;
	if (!desc_val) return;
	if (desc_val->type != GF_PROP_STRING_LIST) return;
	count = gf_list_count(desc_val->value.string_list);
	if (!count) return;
	if ( ! (*p_dst_list)) *p_dst_list = gf_list_new();
	dst_list = *p_dst_list;
	for (j=0; j<count; j++) {
		char *desc = gf_list_get(desc_val->value.string_list, j);
		if (desc[0] == '<') {
			GF_MPD_other_descriptors *d;
			GF_SAFEALLOC(d, GF_MPD_other_descriptors);
			d->xml_desc = gf_strdup(desc);
			gf_list_add(dst_list, d);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Invalid descriptor %s, expecting '<' as first character\n", desc));
		}
	}
}

static void dasher_setup_set_defaults(GF_DasherCtx *ctx, GF_MPD_AdaptationSet *set)
{
	u32 i, count;
	Bool role_set = GF_FALSE;
	//by default setup alignment
	if (ctx->single_segment) set->subsegment_alignment = ctx->no_seg_align ? GF_FALSE : GF_TRUE;
	else set->segment_alignment = ctx->no_seg_align ? GF_FALSE : GF_TRUE;

	//startWithSAP is set when the first packet comes in

	//the rest depends on the various profiles/iop, to check
	count = gf_list_count(set->representations);
	for (i=0; i<count; i++) {
		GF_MPD_Representation *rep = gf_list_get(set->representations, i);
		GF_DashStream *ds = rep->playback.udta;

		if (set->max_width < ds->width) set->max_width = ds->width;
		if (set->max_height < ds->height) set->max_height = ds->height;
/*		if (set->max_bandwidth < ds->rep->bandwidth) set->max_bandwidth = ds->rep->bandwidth;
		if (set->max_framerate * ds->fps.den < ds->fps.num) set->max_framerate = (u32) (ds->fps.num / ds->fps.den);
*/

		/*set role*/
		if (ds->p_role) {
			u32 j, count;
			role_set = GF_TRUE;
			count = gf_list_count(ds->p_role->value.string_list);
			for (j=0; j<count; j++) {
				char *role = gf_list_get(ds->p_role->value.string_list, j);
				GF_MPD_Descriptor *desc;
				char *uri;
				if (!strcmp(role, "caption") || !strcmp(role, "subtitle") || !strcmp(role, "main")
			        || !strcmp(role, "alternate") || !strcmp(role, "supplementary") || !strcmp(role, "commentary")
			        || !strcmp(role, "dub") || !strcmp(role, "description") || !strcmp(role, "sign")
					 || !strcmp(role, "metadata") || !strcmp(role, "enhanced-audio- intelligibility")
				) {
					uri = "urn:mpeg:dash:role:2011";
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Unrecognized role %s - using GPAC urn for schemaID\n", role));
					uri = "urn:gpac:dash:role:2013";
				}
				desc = gf_mpd_descriptor_new(NULL, uri, role);
				gf_list_add(set->role, desc);
			}
		}
	}
}

static void dasher_check_bitstream_swicthing(GF_DasherCtx *ctx, GF_MPD_AdaptationSet *set)
{
	u32 i, j, count;
	Bool use_inband = (ctx->bs_switch==DASHER_BS_SWITCH_INBAND) ? GF_TRUE : GF_FALSE;
	Bool use_multi = (ctx->bs_switch==DASHER_BS_SWITCH_MULTI) ? GF_TRUE : GF_FALSE;
	GF_MPD_Representation *base_rep = gf_list_get(set->representations, 0);
	GF_DashStream *base_ds;

	if (ctx->bs_switch==DASHER_BS_SWITCH_OFF) return;
	if (!base_rep) return;
	base_ds = base_rep->playback.udta;

	count = gf_list_count(set->representations);
	if (count==1) {
		if (ctx->bs_switch==DASHER_BS_SWITCH_FORCE) set->bitstream_switching=GF_TRUE;
		else if (ctx->bs_switch==DASHER_BS_SWITCH_INBAND) {
			GF_DashStream *ds = base_rep->playback.udta;
			ds->inband_params = GF_TRUE;
		}
		return;
	}

	for (i=1; i<count; i++) {
		GF_MPD_Representation *rep = gf_list_get(set->representations, i);
		GF_DashStream *ds = rep->playback.udta;
		//same codec ID
		if (ds->codec_id == base_ds->codec_id) {
			//we will use inband params, so bs switching is OK
			if (use_inband || use_multi) continue;
			//we consider we can switch in non-inband only if we have same CRC for the decoder config
			if (base_ds->dsi_crc == ds->dsi_crc) continue;
			//not the same config, no BS switching
			return;
		}
		//dependencies / different codec IDs, cannot use bitstream switching
		return;
	}
	//ok we can use BS switching, ensure we use the same timescale for every stream
	set->bitstream_switching = GF_TRUE;

	for (i=0; i<count; i++) {
		GF_MPD_Representation *rep = gf_list_get(set->representations, i);
		GF_DashStream *ds = rep->playback.udta;
		for (j=i+1; j<count; j++) {
			GF_DashStream *a_ds;
			rep = gf_list_get(set->representations, j);
			a_ds = rep->playback.udta;
			if (a_ds->stream_type != ds->stream_type) continue;
			if (a_ds->timescale != ds->timescale)
				a_ds->force_timescale = ds->timescale;
		}
	}
}

static void dasher_open_destination(GF_Filter *filter, GF_DasherCtx *ctx, GF_MPD_Representation *rep, const char *szInitURL, Bool trash_init)
{
	GF_Err e;
	Bool has_frag=GF_FALSE;
	Bool has_subs=GF_FALSE;
	const char *dst_args;
	char szDST[GF_MAX_PATH];
	char szSRC[100];

	GF_DashStream *ds = rep->playback.udta;
	if (ds->share_rep) return;


	dst_args = gf_filter_get_dst_args(filter);
	strcpy(szDST, szInitURL);
	if (dst_args) {
		char szKey[20];
		sprintf(szSRC, "%c", gf_filter_get_sep(filter, GF_FS_SEP_ARGS));
		strcat(szDST, szSRC);
		strcat(szDST, dst_args);
		//look for frag arg
		sprintf(szKey, "%cfrag", gf_filter_get_sep(filter, GF_FS_SEP_ARGS));
		if (strstr(dst_args, szKey)) has_frag = GF_TRUE;
		else {
			sprintf(szKey, "%csfrag", gf_filter_get_sep(filter, GF_FS_SEP_ARGS));
			if (strstr(dst_args, szKey)) has_frag = GF_TRUE;
		}
		//look for subs_sidx arg
		sprintf(szKey, "%csubs_sidx", gf_filter_get_sep(filter, GF_FS_SEP_ARGS));
		if (strstr(dst_args, szKey)) has_subs = GF_TRUE;
	}
	if (trash_init) {
		sprintf(szSRC, "%cnoinit", gf_filter_get_sep(filter, GF_FS_SEP_ARGS));
		strcat(szDST, szSRC);
	}
	if (!has_frag) {
		sprintf(szSRC, "%cfrag", gf_filter_get_sep(filter, GF_FS_SEP_ARGS));
		strcat(szDST, szSRC);
	}
	if (!has_subs && ctx->single_segment) {
		sprintf(szSRC, "%csubs_sidx%c0", gf_filter_get_sep(filter, GF_FS_SEP_ARGS), gf_filter_get_sep(filter, GF_FS_SEP_NAME));
		strcat(szDST, szSRC);
	}
	//override xps inband declaration in args
	sprintf(szSRC, "%cxps_inband%c%s", gf_filter_get_sep(filter, GF_FS_SEP_ARGS), gf_filter_get_sep(filter, GF_FS_SEP_NAME), ds->inband_params ? "all" : "no");
	strcat(szDST, szSRC);

	if (ctx->no_fragments_defaults) {
		sprintf(szSRC, "%cno_frags_def", gf_filter_get_sep(filter, GF_FS_SEP_ARGS) );
		strcat(szDST, szSRC);
	}

	ds->dst_filter = gf_filter_connect_destination(filter, szDST, &e);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[Dasher] Couldn't create output file %s: %s\n", szInitURL, gf_error_to_string(e) ));
		return;
	}
	sprintf(szSRC, "MuxSrc%cdasher_%p", gf_filter_get_sep(filter, GF_FS_SEP_NAME), ds->dst_filter);
	//assigne sourceID to be this
	gf_filter_set_source(ds->dst_filter, filter, szSRC);
}

static void dasher_open_pid(GF_Filter *filter, GF_DasherCtx *ctx, GF_DashStream *ds, GF_List *multi_pids)
{
	GF_DashStream *base_ds = ds->share_rep ? ds->share_rep : ds;
	char szSRC[1024];
	assert(!ds->opid);
	assert(base_ds->dst_filter);

	sprintf(szSRC, "dasher_%p", base_ds->dst_filter);
	ds->opid = gf_filter_pid_new(filter);
	gf_filter_pid_copy_properties(ds->opid, ds->ipid);

	//force PID ID
	gf_filter_pid_set_property(ds->opid, GF_PROP_PID_ID, &PROP_UINT(ds->pid_id) );
	gf_filter_pid_set_info(ds->opid, GF_PROP_MUX_SRC, &PROP_STRING(szSRC) );
	gf_filter_pid_set_info(ds->opid, GF_PROP_DASH_MODE, &PROP_UINT(ctx->single_segment ? 2 : 1) );
	gf_filter_pid_set_info(ds->opid, GF_PROP_DASH_DUR, &PROP_DOUBLE(ctx->dur) );

	gf_filter_pid_force_cap(ds->opid, GF_PROP_DASH_MODE);

	/*timescale forced (bitstream switching) */
	if (ds->force_timescale)
		gf_filter_pid_set_property(ds->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ds->force_timescale) );

	if (multi_pids) {
		s32 idx = 1+gf_list_find(multi_pids, ds->ipid);
		assert(idx>0);
		gf_filter_pid_set_property(ds->opid, GF_PROP_DASH_MULTI_PID, &PROP_POINTER(multi_pids) );
		gf_filter_pid_set_property(ds->opid, GF_PROP_DASH_MULTI_PID_IDX, &PROP_UINT(idx) );
	}
}

static Bool dasher_template_use_source_url(GF_DasherCtx *ctx)
{
	if (strstr(ctx->template, "$File$") != NULL) return GF_TRUE;
	else if (strstr(ctx->template, "$FSRC$") != NULL) return GF_TRUE;
	else if (strstr(ctx->template, "$SourcePath$") != NULL) return GF_TRUE;
	else if (strstr(ctx->template, "$FURL$") != NULL) return GF_TRUE;
	else if (strstr(ctx->template, "$URL$") != NULL) return GF_TRUE;
	return GF_FALSE;
}

static void dasher_set_content_components(GF_DashStream *ds)
{
	GF_MPD_ContentComponent *component;
	GF_DashStream *base_ds = ds->share_rep ? ds->share_rep : ds;

	GF_SAFEALLOC(component, GF_MPD_ContentComponent);
	component->id = ds->id;
	switch (ds->stream_type) {
	case GF_STREAM_TEXT:
		component->type = gf_strdup("text");
		break;
	case GF_STREAM_VISUAL:
		component->type = gf_strdup("video");
		break;
	case GF_STREAM_AUDIO:
		component->type = gf_strdup("audio");
		break;
	case GF_STREAM_SCENE:
	case GF_STREAM_OD:
	default:
		component->type = gf_strdup("application");
		break;
	}
	/*if lang not specified at adaptationSet level, put it here*/
	if (!base_ds->set->lang && ds->lang && strcmp(ds->lang, "und")) {
		component->lang = gf_strdup(ds->lang);
	}
	gf_list_add(base_ds->set->content_component, component);
}

static void dasher_setup_sources(GF_Filter *filter, GF_DasherCtx *ctx, GF_MPD_AdaptationSet *set)
{
	char szDASHTemplate[GF_MAX_PATH];
	char szTemplate[GF_MAX_PATH];
	char szSegmentName[GF_MAX_PATH];
	char szInitSegmentName[GF_MAX_PATH];
	Bool single_template = GF_TRUE;
	GF_MPD_Representation *rep = gf_list_get(set->representations, 0);
	GF_DashStream *ds = rep->playback.udta;
	u32 i, j, count, nb_base;
	GF_List *multi_pids = NULL;
	u32 set_timescale = 0;
	Bool init_template_done=GF_FALSE;
	Bool use_inband = (ctx->bs_switch==DASHER_BS_SWITCH_INBAND) ? GF_TRUE : GF_FALSE;

	count = gf_list_count(set->representations);

	assert(ctx->template);
	if (count==1) single_template = GF_TRUE;
	//for regular reps, if we depend on filename we cannot mutualize the template
	else if (dasher_template_use_source_url(ctx) ) single_template = GF_FALSE;
	//and for scalable reps, if we don't have bandwidth /repID we cannot mutualize the template
	else if (ds->complementary_reps) {
		if (strstr(ctx->template, "$Bandwidth$") != NULL) single_template = GF_FALSE;
		else if (strstr(ctx->template, "$RepresentationId$") != NULL) single_template = GF_FALSE;
	}
	strcpy(szTemplate, ctx->template);

	if (ctx->timescale>0) set_timescale = ctx->timescale;
	else if (ctx->timescale<0) {
		u32 first_timescale;
		rep = gf_list_get(set->representations, 0);
		ds = rep->playback.udta;
		first_timescale = ds->timescale;
		for (i=1; i<count; i++) {
			rep = gf_list_get(set->representations, i);
			ds = rep->playback.udta;
			if (ds->timescale != first_timescale) {
				//we cannot use a single template if enforcing timescales which are not identical
				single_template = GF_FALSE;
				break;
			}
		}
	}

	//assign PID IDs - we assume only one component of a given media type per adaptation set
	//and assign the same PID ID for each component of the same type
	//we could refine this using roles, but most HAS solutions don't use roles at the mulitplexed level
	for (i=0; i<count; i++) {
		u32 j;
		rep = gf_list_get(set->representations, i);
		ds = rep->playback.udta;
		if (ds->pid_id) continue;
		ds->pid_id = gf_list_find(ctx->pids, ds) + 1;

		for (j=i+1; j<count; j++) {
			GF_DashStream *a_ds;
			rep = gf_list_get(set->representations, j);
			a_ds = rep->playback.udta;
			if (a_ds->pid_id) continue;
			if (a_ds->stream_type == ds->stream_type) a_ds->pid_id = ds->pid_id;
		}
	}
	//this is crude because we don't copy the properties, we just pass a list of pids to the destination muxer !!
	//we should cleanup one of these days
	if (set->bitstream_switching && (ctx->bs_switch==DASHER_BS_SWITCH_MULTI)) {
		multi_pids = gf_list_new();
		for (i=0; i<count; i++) {
			rep = gf_list_get(set->representations, i);
			ds = rep->playback.udta;
			if (ds->owns_set) ds->multi_pids = multi_pids;
			gf_list_add(multi_pids, ds->ipid);
		}
	}

	for (i=0; i<count; i++) {
		GF_Err e;
		u32 init_template_mode = GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE;
		rep = gf_list_get(set->representations, i);
		ds = rep->playback.udta;

		//remove representations for streams muxed with others, but still open the output
		if (ds->share_rep) {
			GF_DashStream *ds_set = set->udta;
			gf_list_rem(set->representations, i);
			i--;
			count--;
			assert(ds_set->nb_rep);
			ds_set->nb_rep--;
			assert(ds->share_rep->dst_filter);
			gf_list_transfer(ds->share_rep->rep->audio_channels, rep->audio_channels);
			gf_list_transfer(ds->share_rep->rep->base_URLs, rep->base_URLs);
			gf_list_transfer(ds->share_rep->rep->content_protection , rep->content_protection);
			gf_list_transfer(ds->share_rep->rep->essential_properties , rep->essential_properties);
			gf_list_transfer(ds->share_rep->rep->frame_packing , rep->frame_packing);
			gf_list_transfer(ds->share_rep->rep->other_descriptors , rep->other_descriptors);
			gf_list_transfer(ds->share_rep->rep->supplemental_properties , rep->supplemental_properties);

			gf_mpd_representation_free(ds->rep);
			ds->rep = NULL;

			if (!gf_list_count(ds->set->content_component)) {
				dasher_set_content_components(ds->share_rep);
			}
			dasher_set_content_components(ds);
			assert(!multi_pids);
			//open PID
			dasher_open_pid(filter, ctx, ds, NULL);
			continue;
		}


		if (use_inband) ds->inband_params = GF_TRUE;

		//if bitstream switching and templating, only set for the first one
		if (i && set->bitstream_switching && ctx->stl && single_template) continue;

		if (!set_timescale) set_timescale = ds->timescale;

		if (ctx->timescale<0) ds->mpd_timescale = ds->timescale;
		else ds->mpd_timescale = set_timescale;

		//resolve segment template
		e = gf_filter_pid_resolve_file_template(ds->ipid, szTemplate, szDASHTemplate, 0);
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Cannot resolve template name, cannot derive output segment names, disabling rep %s\n", ds->src_url));
			ds->done = GF_TRUE;
			continue;

		}
		if (single_template && ds->split_set_names) {
			char szStrName[20];
			sprintf(szStrName, "_set%d", 1 + gf_list_find(ctx->current_period->period->adaptation_sets, set)  );
			strcat(szDASHTemplate, szStrName);
			//don't bother forcing an "init" since we rename the destinations
			init_template_mode = GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE_SKIPINIT;
		}

		//get final segment template - output file name is NULL, we already have solved this
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, set->bitstream_switching, szSegmentName, NULL, ds->rep_id, NULL, szDASHTemplate, (ctx->ext && !stricmp(ctx->ext, "null")) ? NULL : ctx->ext, 0, 0, 0, ctx->stl);

		//get final init name - output file name is NULL, we already have solved this
		gf_media_mpd_format_segment_name(init_template_mode, set->bitstream_switching, szInitSegmentName, NULL, ds->rep_id, NULL, szDASHTemplate, (ctx->ext && !stricmp(ctx->ext, "null")) ? NULL : "mp4", 0, 0, 0, ctx->stl);

		ds->init_seg = gf_strdup(szInitSegmentName);
		ds->seg_template = gf_strdup(szSegmentName);

		/* baseURLs */
		nb_base = ds->p_base_url ? gf_list_count(ds->p_base_url->value.string_list) : 0;
		for (j=0; j<nb_base; j++) {
			GF_MPD_BaseURL *base_url;
			char *url = gf_list_get(ds->p_base_url->value.string_list, j);
			GF_SAFEALLOC(base_url, GF_MPD_BaseURL);
			base_url->URL = gf_strdup(url);
			gf_list_add(rep->base_URLs, base_url);
		}

		//we use segment template
		if (ctx->tpl) {
			GF_MPD_SegmentTemplate *seg_template;
			//bs switching but multiple templates
			if ((count==1) || (!i && set->bitstream_switching)) {
				init_template_done = GF_TRUE;
				GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
				seg_template->initialization = gf_strdup(szInitSegmentName);
				dasher_open_destination(filter, ctx, rep, seg_template->initialization, GF_FALSE);

				if (single_template) {
					seg_template->media = gf_strdup(szSegmentName);
					seg_template->timescale = ds->mpd_timescale;
					seg_template->start_number = ds->startNumber ? ds->startNumber : 1;
					seg_template->duration = (u64)(ctx->dur * ds->mpd_timescale);
					if (ctx->asto < 0) {
						seg_template->availability_time_offset = - (Double) ctx->asto / 1000.0;
					}
				} else {
					seg_template->start_number = (u32)-1;

				}
				set->segment_template = seg_template;
			}
			if (i || !single_template) {
				GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
				if (!init_template_done) {
					seg_template->initialization = gf_strdup(szInitSegmentName);
					dasher_open_destination(filter, ctx, rep, seg_template->initialization, GF_FALSE);
				} else if (i) {
					dasher_open_destination(filter, ctx, rep, szInitSegmentName, GF_TRUE);
				}
				seg_template->media = gf_strdup(szSegmentName);
				seg_template->duration = (u64)(ctx->dur * ds->mpd_timescale);
				seg_template->timescale = ds->mpd_timescale;
				seg_template->start_number = ds->startNumber ? ds->startNumber : 1;
				if (ctx->asto < 0) {
					seg_template->availability_time_offset = - (Double) ctx->asto / 1000.0;
				}
				rep->segment_template = seg_template;
			}
		}
		/*we are using a single file or segment, use base url*/
		else if (ctx->single_segment || ctx->single_file) {
			GF_MPD_BaseURL *baseURL;

			//get rid of default "init" added for init templates
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_SKIPINIT, set->bitstream_switching, szInitSegmentName, NULL, ds->rep_id, NULL, szDASHTemplate, (ctx->ext && !stricmp(ctx->ext, "null")) ? NULL : "mp4", 0, 0, 0, ctx->stl);

			if (ds->init_seg) gf_free(ds->init_seg);
			ds->init_seg = gf_strdup(szInitSegmentName);

			GF_SAFEALLOC(baseURL, GF_MPD_BaseURL);
			if (!rep->base_URLs) rep->base_URLs = gf_list_new();
			gf_list_add(rep->base_URLs, baseURL);

			if (ctx->single_segment) {
				GF_MPD_SegmentBase *segment_base;
				baseURL->URL = gf_strdup(szInitSegmentName);
				GF_SAFEALLOC(segment_base, GF_MPD_SegmentBase);
				rep->segment_base = segment_base;
				dasher_open_destination(filter, ctx, rep, szInitSegmentName, GF_FALSE);
			} else {
				GF_MPD_SegmentList *seg_list;
				GF_SAFEALLOC(seg_list, GF_MPD_SegmentList);
				GF_SAFEALLOC(seg_list->initialization_segment, GF_MPD_URL);
				baseURL->URL = gf_strdup(szInitSegmentName);
				seg_list->dasher_segment_name = gf_strdup(szSegmentName);
				seg_list->timescale = ds->mpd_timescale;
				seg_list->segment_URLs = gf_list_new();
				rep->segment_list = seg_list;
				ds->seg_urls = gf_list_new();

				dasher_open_destination(filter, ctx, rep, szInitSegmentName, GF_FALSE);
			}
		}
		//no template, no single file, we need a file list
		else {
			GF_MPD_SegmentList *seg_list;
			GF_SAFEALLOC(seg_list, GF_MPD_SegmentList);
			GF_SAFEALLOC(seg_list->initialization_segment, GF_MPD_URL);
			seg_list->initialization_segment->sourceURL = gf_strdup(szInitSegmentName);
			seg_list->dasher_segment_name = gf_strdup(szSegmentName);
			seg_list->timescale = ds->mpd_timescale;
			seg_list->segment_URLs = gf_list_new();
			rep->segment_list = seg_list;
			ds->seg_urls = gf_list_new();

			dasher_open_destination(filter, ctx, rep, szInitSegmentName, GF_FALSE);
		}
		//open PID
		dasher_open_pid(filter, ctx, ds, multi_pids);
	}
}

static void dasher_update_period_duration(GF_DasherCtx *ctx)
{
	u32 i, count;
	u64 pdur = 0;
	u64 min_dur = 0;
	count = gf_list_count(ctx->pids);
	for (i=0; i<count; i++) {
		GF_DashStream *ds = gf_list_get(ctx->pids, i);
		if (!min_dur || (min_dur>ds->max_period_dur)) min_dur = ds->max_period_dur;
		if (pdur< ds->max_period_dur) pdur = ds->max_period_dur;
	}

	if (!ctx->check_dur) {
		s32 diff = (s32) ((s64) pdur - (s64) min_dur);
		if (ABS(diff)>2000) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Adaptation sets in period are of unequal duration min %g max %g seconds\n", ((Double)min_dur)/1000, ((Double)pdur)/1000));
		}
	}

	ctx->current_period->period->duration = pdur;
	if (!ctx->dynamic) {
		ctx->mpd->media_presentation_duration += pdur;
	}
}


GF_Err dasher_send_mpd(GF_Filter *filter, GF_DasherCtx *ctx)
{
	GF_FilterPacket *pck;
	u32 size, nb_read;
	char *output;
	GF_Err e;
	FILE *tmp = gf_temp_file_new(NULL);

	ctx->mpd->publishTime = gf_net_get_ntp_ms();
	dasher_update_mpd(ctx);
	e = gf_mpd_write(ctx->mpd, tmp);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[Dasher] failed to write MPD file: %s\n", gf_error_to_string(e) ));
		gf_fclose(tmp);
		return e;
	}
	gf_fseek(tmp, 0, SEEK_END);
	size = (u32) gf_ftell(tmp);
	gf_fseek(tmp, 0, SEEK_SET);

	pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	nb_read = gf_fread(output, 1, size, tmp);
	if (nb_read != size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[Dasher] Error reading temp MPD file, read %d bytes but file size is %d\n", nb_read, size ));
	}
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_send(pck);
	gf_fclose(tmp);
	return GF_OK;
}

static void dasher_reset_stream(GF_DashStream *ds, Bool is_destroy)
{
	if (ds->seg_template) gf_free(ds->seg_template);
	if (ds->init_seg) gf_free(ds->init_seg);
	if (ds->multi_pids) gf_list_del(ds->multi_pids);
	ds->multi_pids = NULL;

	if (is_destroy) {
		gf_list_del(ds->complementary_reps);
		gf_free(ds->rep_id);
		return;
	}
	ds->init_seg = ds->seg_template = NULL;
	ds->split_set_names = GF_FALSE;
	ds->nb_sap_3 = 0;
	ds->nb_sap_4 = 0;
	ds->pid_id = 0;
	ds->force_timescale = 0;
	ds->set = NULL;
	ds->owns_set = GF_FALSE;
	ds->rep = NULL;
	ds->share_rep = NULL;
	ds->nb_comp = ds->nb_comp_done = 0;
	gf_list_reset(ds->complementary_reps);
	ds->inband_params = GF_FALSE;
	ds->seg_start_time = 0;
	ds->seg_number = ds->startNumber;
}

static GF_Err dasher_switch_period(GF_Filter *filter, GF_DasherCtx *ctx)
{
	u32 i, count, nb_done;
	char *period_id;
	GF_DasherPeriod *p;
	GF_DashStream *first_in_period=NULL;
	p = ctx->current_period;

	if (ctx->current_period->period) {
		//update duration
		dasher_update_period_duration(ctx);
	}
	//we have a MPD ready, flush it
	if (ctx->mpd)
		dasher_send_mpd(filter, ctx);

	//reset - don't destroy, it is in the MPD
	ctx->current_period->period = NULL;
	//switch
	ctx->current_period = ctx->next_period;
	ctx->next_period = p;
	ctx->on_demand_done = GF_FALSE;
	//reset MPD pointers
	count = gf_list_count(ctx->current_period->streams);
	for (i=0; i<count;i++) {
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);
		dasher_reset_stream(ds, GF_FALSE);

		//remove output pids
		if (ds->opid) {
			gf_filter_pid_set_eos(ds->opid);
			gf_filter_pid_remove(ds->opid);
			ds->opid = NULL;
		}
	}


	//filter out PIDs not for this period
	count = gf_list_count(ctx->current_period->streams);
	period_id = NULL;
	for (i=0; i<count; i++) {
		Bool in_period=GF_TRUE;
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);

		if (ds->done) {
			in_period=GF_FALSE;
		} else if (!period_id) {
			period_id = ds->period_id;
			first_in_period = ds;
		} else if (strcmp(period_id, ds->period_id)) {
			in_period = GF_FALSE;
		}
		//if not in period, move to next period
		if (!in_period) {
			gf_list_rem(ctx->current_period->streams, i);
			i--;
			count--;
			gf_list_add(ctx->next_period->streams, ds);
			continue;
		}
		//setup representation - the representation is created independetly from the period
		dasher_setup_rep(ctx, ds);
	}
	count = gf_list_count(ctx->current_period->streams);
	if (!count) {
		count = gf_list_count(ctx->next_period->streams);
		nb_done = 0;
		for (i=0; i<count; i++) {
			GF_DashStream *ds = gf_list_get(ctx->next_period->streams, i);
			if (ds->done) nb_done++;
		}
		if (nb_done == count) {
			return GF_EOS;
		}
	}

	//we need a new period, create it
	ctx->current_period->period = gf_mpd_period_new();
	if (!ctx->mpd) dasher_setup_mpd(ctx);
	gf_list_add(ctx->mpd->periods, ctx->current_period->period);

	//TODO: setup duration / start and xlink for remote periods
/*	if (period_start || dasher->dash_mode) {
		period->start = (u64)(period_start*1000);
	}
	if (!dasher->dash_mode && period_duration) {
		period->duration = (u64)(period_duration*1000);
	}
	if (xlink) {
		period->xlink_href = gf_strdup(xlink);
	}
*/

	assert(period_id);

	if (strcmp(period_id, "_gpac_dasher_default_period_id"))
		ctx->current_period->period->ID = gf_strdup(period_id);

	//setup representation dependency / components (muxed)
	count = gf_list_count(ctx->current_period->streams);
	for (i=0; i<count; i++) {
		u32 j;
		GF_DashStream *ds_video=NULL;
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);

		//add period descriptors
		dasher_add_descriptors(&ctx->current_period->period->other_descriptors, ds->p_period_desc);
		//add representation descriptors
		dasher_add_descriptors(&ds->rep->other_descriptors, ds->p_rep_desc);

		if (ds->share_rep) continue;

		if (ds->stream_type==GF_STREAM_VISUAL)
			ds_video = ds;
		ds->nb_comp = 1;

		for (j=0; j<count; j++) {
			GF_DashStream *a_ds;
			if (i==j) continue;
			a_ds = gf_list_get(ctx->current_period->streams, j);
			if (a_ds->dep_id && (a_ds->dep_id==ds->id) ) {
				gf_list_add(ds->complementary_reps, a_ds);
			}
			if (!a_ds->share_rep && !strcmp(a_ds->rep_id, ds->rep_id) ) {
				char szCodecs[1024];
				a_ds->share_rep = ds;
				ds->nb_comp++;

				if (ctx->bs_switch==DASHER_BS_SWITCH_MULTI) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Bitstream Swicthing mode \"multi\" is not supported with multiplexed representations, disabling bitstream switching\n"));
					ctx->bs_switch = DASHER_BS_SWITCH_OFF;
				}
				strcpy(szCodecs, ds->rep->codecs);
				strcat(szCodecs, ",");
				strcat(szCodecs, a_ds->rep->codecs);
				gf_free(ds->rep->codecs);
				ds->rep->codecs = gf_strdup(szCodecs);
			}
		}
		//use video as main stream for segmentation of muxed sources
		if (ds_video != ds) {
			for (j=0; j<count; j++) {
				GF_DashStream *a_ds = gf_list_get(ctx->current_period->streams, j);
				if ((a_ds->share_rep==ds) || (a_ds==ds)) {
					if (a_ds == ds_video) a_ds->share_rep = NULL;
					else a_ds->share_rep = ds_video;
				}
			}
		}
	}

	//setup reps in adaptation sets
	for (i=0; i<count; i++) {
		u32 j;
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);
		if (ds->share_rep) continue;

		if (!ds->set) {
			ds->set = gf_mpd_adaptation_set_new();
			ds->owns_set = GF_TRUE;
			ds->set->udta = ds;

			if (!ds->set->representations)
			 	ds->set->representations = gf_list_new();
			if (!ds->period->period->adaptation_sets)
				ds->period->period->adaptation_sets = gf_list_new();
			gf_list_add(ds->period->period->adaptation_sets, ds->set);

			gf_list_add(ds->set->representations, ds->rep);
			ds->nb_rep++;

			//add non-conditional adaptation set descriptors
			dasher_add_descriptors(&ds->set->other_descriptors, ds->p_as_any_desc);
			//new AS, add conditionnal adaptation set descriptors
			dasher_add_descriptors(&ds->set->other_descriptors, ds->p_as_desc);
		}
		for (j=i+1; j<count; j++) {
			GF_DashStream *a_ds;
			a_ds = gf_list_get(ctx->current_period->streams, j);
			//we add to the adaptation set even if shared rep, we will remove it when assigning templates and pids
			if (dasher_same_adaptation_set(ctx, ds, a_ds)) {
				a_ds->set = ds->set;
				gf_list_add(ds->set->representations, a_ds->rep);
				ds->nb_rep++;
				//add non-conditional adaptation set descriptors
				dasher_add_descriptors(&ds->set->other_descriptors, a_ds->p_as_any_desc);
			}
		}
	}
	//we need a pass on adaptation sets to figure out if they share the same source URL
	//in case we use file name in templates
	if (dasher_template_use_source_url(ctx)) {
		u32 i, j, nb_sets = gf_list_count(ctx->current_period->period->adaptation_sets);

		for (i=0; i<nb_sets; i++) {
			GF_MPD_AdaptationSet *set = gf_list_get(ctx->current_period->period->adaptation_sets, i);
			GF_MPD_Representation *rep = gf_list_get(set->representations, 0);
			GF_DashStream *ds = rep->playback.udta;
			for (j=0; j<nb_sets; j++) {
				Bool split_init = GF_FALSE;
				const GF_PropertyValue *p1, *p2;
				GF_DashStream *a_ds;
				if (i==j) continue;
				set = gf_list_get(ctx->current_period->period->adaptation_sets, j);
				rep = gf_list_get(set->representations, 0);
				a_ds = rep->playback.udta;
				p1 = gf_filter_pid_get_property(ds->ipid, GF_PROP_PID_FILEPATH);
				p2 = gf_filter_pid_get_property(a_ds->ipid, GF_PROP_PID_FILEPATH);
				if (gf_props_equal(p1, p2)) split_init = GF_TRUE;
				p1 = gf_filter_pid_get_property(ds->ipid, GF_PROP_PID_URL);
				p2 = gf_filter_pid_get_property(a_ds->ipid, GF_PROP_PID_URL);
				if (gf_props_equal(p1, p2)) split_init = GF_TRUE;

				if (split_init) {
					ds->split_set_names = GF_TRUE;
					a_ds->split_set_names = GF_TRUE;
				}
			}
		}
	}
	//setup adaptation sets bitstream switching
	for (i=0; i<count; i++) {
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);
		if (!ds->owns_set) continue;
		//check bitstream switching
		dasher_check_bitstream_swicthing(ctx, ds->set);
		//setup AS defaults, roles and co
		dasher_setup_set_defaults(ctx, ds->set);
		//setup sources, templates & co
		dasher_setup_sources(filter, ctx, ds->set);
	}
	//good to go !
	//setup adaptation sets bitstream switching
	for (i=0; i<count; i++) {
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);
		//setup segmentation
		ds->rep_init = GF_FALSE;
		ds->seg_done = GF_FALSE;
		ds->next_seg_start = ctx->dur * ds->timescale;
		ds->adjusted_next_seg_start = ds->next_seg_start;
		ds->segment_started = GF_FALSE;
		ds->seg_number = ds->startNumber;
		ds->first_cts = ds->max_period_dur = 0;
		//send init filename
		if (ds->init_seg)
			gf_filter_pid_set_property(ds->opid, GF_PROP_PID_OUTPATH, &PROP_STRING(ds->init_seg));
	}

	return GF_OK;
}

static void dasher_insert_timeline_entry(GF_DasherCtx *ctx, GF_DashStream *ds)
{
	GF_MPD_SegmentTimelineEntry *s;
	u64 duration;
	Bool is_first = GF_FALSE;
	Bool seg_align = GF_FALSE;
	GF_MPD_SegmentTimeline *tl=NULL;

	//we only store segment timeline for the main component in the representation
	if (ds->share_rep) return;

	//we only use segment timeline with templates
	if (!ctx->stl) return;

	if (gf_list_find(ds->set->representations, ds->rep)==0) is_first = GF_TRUE;
	assert(ds->first_cts_in_next_seg > ds->first_cts_in_seg);
	duration = ds->first_cts_in_next_seg - ds->first_cts_in_seg;
	if (ds->timescale != ds->mpd_timescale) {
		duration *= ds->mpd_timescale;
		duration /= ds->timescale;
	}
	seg_align = (ds->set->segment_alignment || ds->set->subsegment_alignment) ? GF_TRUE : GF_FALSE;
	//not first and segment alignment, ignore
	if (!is_first && seg_align) {
		return;
	}

	//no segment alignment store in each rep
	if (!seg_align) {
		GF_MPD_SegmentTimeline **p_tl=NULL;
		if (ctx->tpl) {
			p_tl = &ds->rep->segment_template->segment_timeline;
			ds->rep->segment_template->duration = 0;
		} else {
			p_tl = &ds->rep->segment_list->segment_timeline;
			ds->rep->segment_list->duration = 0;
		}
		if (! (*p_tl)) {
			(*p_tl) = gf_mpd_segmentimeline_new();
		}
		tl = (*p_tl);
	} else {
		GF_MPD_SegmentTimeline **p_tl=NULL;
		if (ctx->tpl) {
			//in case we had no template at set level
			if (!ds->set->segment_template) {
				GF_SAFEALLOC(ds->set->segment_template, GF_MPD_SegmentTemplate);
			}
			p_tl = &ds->set->segment_template->segment_timeline;
			ds->set->segment_template->duration = 0;
		} else {
			//in case we had no template at set level
			if (!ds->set->segment_list) {
				GF_SAFEALLOC(ds->set->segment_list, GF_MPD_SegmentList);
			}
			p_tl = &ds->set->segment_list->segment_timeline;
			ds->set->segment_list->duration = 0;
		}

		if (! (*p_tl) ) {
			(*p_tl)  = gf_mpd_segmentimeline_new();
		}
		tl = (*p_tl);
	}

	//append to previous entry if possible
	s = gf_list_last(tl->entries);
	if (s && (s->duration == duration) && (s->start_time + (s->repeat_count+1) * s->duration == ds->seg_start_time)) {
		s->repeat_count++;
		return;
	}
	//nope, allocate
	GF_SAFEALLOC(s, GF_MPD_SegmentTimelineEntry);
	s->start_time = ds->seg_start_time;
	s->duration = duration;
	gf_list_add(tl->entries, s);
}

static void dasher_copy_segment_timelines(GF_DasherCtx *ctx, GF_MPD_AdaptationSet *set)
{
	GF_MPD_SegmentTimeline *src_tl = NULL;
	u32 i, j, count, nb_s;
	if (!ctx->stl) return;
	//get as level segment timeline, set it to NULL, reassign it to first rep and clone for other reps
	if (ctx->tpl) {
		assert(set->segment_template->segment_timeline);
		src_tl = set->segment_template->segment_timeline;
		set->segment_template->segment_timeline = NULL;
	} else {
		assert(set->segment_list->segment_timeline);
		src_tl = set->segment_list->segment_timeline;
		set->segment_list->segment_timeline = NULL;
	}
	nb_s = gf_list_count(src_tl->entries);

	count = gf_list_count(set->representations);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentTimeline *tl = NULL;
		GF_MPD_Representation *rep = gf_list_get(set->representations, i);
		if (ctx->tpl) {
			if (!rep->segment_template) {
				GF_SAFEALLOC(rep->segment_template, GF_MPD_SegmentTemplate);
			}
			if (!i) {
				rep->segment_template->segment_timeline = src_tl;
				continue;
			}
			if (!rep->segment_template->segment_timeline) {
				rep->segment_template->segment_timeline = gf_mpd_segmentimeline_new();
			}
			tl = rep->segment_template->segment_timeline;
		} else {
			if (!rep->segment_list) {
				GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
			}
			if (!i) {
				rep->segment_list->segment_timeline = src_tl;
				continue;
			}
			if (!rep->segment_list->segment_timeline) {
				rep->segment_list->segment_timeline = gf_mpd_segmentimeline_new();
			}
			tl = rep->segment_list->segment_timeline;
		}
		assert(tl);
		for (j=0; j<nb_s; j++) {
			GF_MPD_SegmentTimelineEntry *s;
			GF_MPD_SegmentTimelineEntry *src_s = gf_list_get(src_tl->entries, j);
			GF_SAFEALLOC(s, GF_MPD_SegmentTimelineEntry);
			s->duration = src_s->duration;
			s->repeat_count = src_s->repeat_count;
			s->start_time = src_s->start_time;
			gf_list_add(tl->entries, s);
		}
	}
}

static void dasher_flush_segment(GF_DasherCtx *ctx, GF_DashStream *ds)
{
	u32 i, count;
	GF_DashStream *ds_done = NULL, *ds_not_done = NULL;
	GF_DashStream *set_ds = ds->set->udta;
	GF_DashStream *base_ds = ds->share_rep ? ds->share_rep : ds;


	if (ds->segment_started) {
		Double seg_duration = base_ds->first_cts_in_next_seg - ds->first_cts_in_seg;
		seg_duration /= base_ds->timescale;
		assert(seg_duration);

		if (!base_ds->done && !ctx->stl && ctx->tpl) {

			if (seg_duration< ctx->dur/2) {

				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Segment %d duration %g less than half DASH duration, consider reencoding or using segment timeline\n", ds->seg_number, seg_duration));
			} else if (seg_duration > 3*ctx->dur/2) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Segment %d duration %g more than 3/2 DASH duration, consider reencoding or using segment timeline\n", ds->seg_number, seg_duration));
			}
		}
		dasher_insert_timeline_entry(ctx, base_ds);

		if (! ctx->no_seg_align) {
			if (!set_ds->nb_rep_done || !set_ds->set_seg_duration) {
				set_ds->set_seg_duration = seg_duration;
			} else {
				Double diff = set_ds->set_seg_duration - seg_duration;

				if (ABS(diff) > 0.001) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Segments are not aligned across representations: first rep segment duration %g but new segment duration %g for the same segment %d\n", set_ds->set_seg_duration, seg_duration, set_ds->seg_number));

					if (ctx->profile != GF_DASH_PROFILE_FULL) {
						set_ds->set->segment_alignment = GF_FALSE;
						set_ds->set->subsegment_alignment = GF_FALSE;
						ctx->profile = GF_DASH_PROFILE_FULL;
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] No segment alignment, switching to full profile\n"));
						dasher_copy_segment_timelines(ctx, set_ds->set);
					}
				}
			}
			set_ds->nb_rep_done++;
			if (set_ds->nb_rep_done < set_ds->nb_rep) return;

			set_ds->set_seg_duration = 0;
			set_ds->nb_rep_done = 0;
		}
	} else {
		if (! ctx->no_seg_align) {
			set_ds->nb_rep_done++;
			if (set_ds->nb_rep_done < set_ds->nb_rep) return;

			set_ds->set_seg_duration = 0;
			set_ds->nb_rep_done = 0;
		}
	}


	count = gf_list_count(ctx->current_period->streams);
	//reset all streams from our rep or our set
	for (i=0; i<count; i++) {
		ds = gf_list_get(ctx->current_period->streams, i);
		//reset all in set if segment alignment
		if (! ctx->no_seg_align) {
			if (ds->set != set_ds->set) continue;
		} else {
			//otherwise reset only media components for this rep
			if ((ds->share_rep != base_ds) && (ds != base_ds)) continue;
		}

		if (!ds->done) {
			ds->first_cts_in_next_seg = ds->first_cts_in_seg = ds->est_first_cts_in_next_seg = 0;
		}

		if (ds->share_rep) {
			if (!ds->done) {
				ds->segment_started = GF_FALSE;
				ds->seg_done = GF_FALSE;
			}
			continue;
		}
		base_ds = ds;

		if (base_ds->done)
			ds_done = base_ds;
		else if (base_ds->nb_comp_done==base_ds->nb_comp) ds_not_done = base_ds;

		if (!base_ds->done && base_ds->seg_done) {
			base_ds->seg_done = GF_FALSE;
			base_ds->nb_comp_done = 0;

			assert(base_ds->segment_started);
			base_ds->segment_started = GF_FALSE;

			base_ds->next_seg_start += ctx->dur*base_ds->timescale;
			while (base_ds->next_seg_start <= base_ds->adjusted_next_seg_start) {
				base_ds->next_seg_start += ctx->dur*base_ds->timescale;
				if (ctx->skip_seg)
					base_ds->seg_number++;
			}
			base_ds->adjusted_next_seg_start = base_ds->next_seg_start;
			base_ds->seg_number++;
		}
	}

	//some reps are done, other not, force a max time on all AS in the period
	if (ds_done && ds_not_done) {
		for (i=0; i<count; i++) {
			ds = gf_list_get(ctx->current_period->streams, i);

			if (ds->done) {
				if (ds->set->udta == set_ds)
					set_ds->nb_rep_done++;
			} else if (ctx->check_dur && !ds->force_rep_end) {
				ds->force_rep_end = ds_done->first_cts_in_next_seg * ds->timescale / ds_done->timescale;
			}
		}
	}
}

static void dasher_mark_segment_start(GF_DasherCtx *ctx, GF_DashStream *ds, GF_FilterPacket *pck)
{
	char szSegmentName[GF_MAX_PATH];
	GF_DashStream *base_ds = ds->share_rep ? ds->share_rep : ds;

	gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENUM, &PROP_UINT(base_ds->seg_number ) );

	//only signal file name & insert timelines on one stream for muxed representations
	if (ds->share_rep) return;

	if (ctx->single_file) {
		if (ds->rep->segment_list) {
			GF_MPD_SegmentURL *seg_url;
			GF_SAFEALLOC(seg_url, GF_MPD_SegmentURL);
			gf_list_add(ds->rep->segment_list->segment_URLs, seg_url);
			gf_list_add(ds->seg_urls, seg_url);
			ctx->nb_seg_url_pending++;
		}
		return;
	}

	ds->seg_start_time = ds->first_cts_in_seg;
	if (ds->timescale != ds->mpd_timescale) {
		ds->seg_start_time *= ds->mpd_timescale;
		ds->seg_start_time /= ds->timescale;
	}

	if (!ctx->stl) {
		Double drift, seg_start = ds->seg_start_time;
		seg_start /= ds->mpd_timescale;
		drift = seg_start - (ds->seg_number - ds->startNumber)*ctx->dur;

		if (ABS(drift) > ctx->dur/2) {
			u64 cts = gf_filter_pck_get_cts(pck);
			cts -= ds->first_cts;
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] First CTS "LLU" in segment %d drifting by %g (more than half a second duration) from segment time, consider reencoding or using segment timeline\n", cts, ds->seg_number,  drift));
		}

	}

	//get final segment template - output file name is NULL, we already have solved this
	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, ds->set->bitstream_switching, szSegmentName, NULL, base_ds->rep_id, NULL, base_ds->seg_template, NULL, base_ds->seg_start_time, base_ds->rep->bandwidth, base_ds->seg_number, ctx->stl);

	if (ds->rep->segment_list) {
		GF_MPD_SegmentURL *seg_url;
		GF_SAFEALLOC(seg_url, GF_MPD_SegmentURL);
		gf_list_add(ds->rep->segment_list->segment_URLs, seg_url);
		seg_url->media = gf_strdup(szSegmentName);
		gf_list_add(ds->seg_urls, seg_url);
		ctx->nb_seg_url_pending++;
	}

	gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENAME, &PROP_STRING(szSegmentName) );
}

static void dasher_update_pck_times(GF_DashStream *ds, GF_FilterPacket *dst)
{
	u64 ts;
	ts = gf_filter_pck_get_dts(dst);
	if (ts!=GF_FILTER_NO_TS) {
		ts *= ds->force_timescale;
		ts /= ds->timescale;
		gf_filter_pck_set_dts(dst, ts);
	}
	ts = gf_filter_pck_get_cts(dst);
	if (ts!=GF_FILTER_NO_TS) {
		ts *= ds->force_timescale;
		ts /= ds->timescale;
		gf_filter_pck_set_cts(dst, ts);
	}
	ts = (u64) gf_filter_pck_get_duration(dst);
	if (ts!=GF_FILTER_NO_TS) {
		ts *= ds->force_timescale;
		ts /= ds->timescale;
		gf_filter_pck_set_duration(dst, (u32) ts);
	}
}

static GF_Err dasher_process(GF_Filter *filter)
{
	u32 i, count, nb_init, has_init;
	GF_DasherCtx *ctx = gf_filter_get_udta(filter);
	GF_Err e;

	if (ctx->is_eos) return GF_EOS;

	nb_init = has_init = 0;
	count = gf_list_count(ctx->current_period->streams);
	for (i=0; i<count; i++) {
		GF_DashStream *base_ds;
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);

		if (ds->done) continue;
		base_ds = ds->share_rep ? ds->share_rep : ds;
		if (ds->seg_done) continue;

		//flush as mush as possible
		while (1) {
			u32 sap_type;
			u64 cts, ncts;
			Bool seg_over = GF_FALSE;
			GF_FilterPacket *pck;

			assert(ds->period == ctx->current_period);
			pck = gf_filter_pid_get_packet(ds->ipid);
			//we may change period after a packet fecth (reconfigure of input pid)
			if (ds->period != ctx->current_period) {
				assert(gf_list_find(ctx->current_period->streams, ds)<0);
				count = gf_list_count(ctx->current_period->streams);
				i--;
				break;
			}

			//mux rep, wait for indexing one to be over
			if ((base_ds != ds) && !base_ds->seg_done)
				break;

			if (!pck) {
				if (gf_filter_pid_is_eos(ds->ipid)) {
					gf_filter_pid_set_eos(ds->opid);
					ds->done = GF_TRUE;
					ds->seg_done = GF_TRUE;
					ds->first_cts_in_next_seg = ds->est_first_cts_in_next_seg;
					ds->est_first_cts_in_next_seg = 0;
					base_ds->nb_comp_done ++;
					if (base_ds->nb_comp_done == base_ds->nb_comp) {
						dasher_flush_segment(ctx, base_ds);
					}
				}
				break;
			}
			sap_type = gf_filter_pck_get_sap(pck);

			cts = gf_filter_pck_get_cts(pck);
			if (!ds->rep_init) {
				if (!sap_type) {
					gf_filter_pid_drop_packet(ds->ipid);
					break;
				}
				if (!ds->share_rep) {
					//set AS sap type
					if (!ds->set->starts_with_sap) {
						//don't set SAP type if not a base rep - could be further checked
						if (!gf_list_count(ds->complementary_reps) )
							ds->set->starts_with_sap = sap_type;
					}
					else if (ds->set->starts_with_sap != sap_type) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[Dasher] Segments do not start with the same SAP types: set initialized with %d but first packet got %d - bitstream will not be compliant\n", ds->set->starts_with_sap, sap_type));
					}
					if (ds->rep->segment_list)
						ds->rep->segment_list->presentation_time_offset = cts;
					else if (ds->rep->segment_template)
						ds->rep->segment_template->presentation_time_offset = cts;
				}

				ds->first_cts = cts;
				ds->rep_init++;
				has_init++;
			}
			nb_init++;
			//ready to write MPD for the first time in dynamic mode
			if (has_init && (nb_init==count) && ctx->dynamic) {
				e = dasher_send_mpd(filter, ctx);
				if (e) return e;
			}
			cts -= ds->first_cts;

			//forcing max time
			if (base_ds->force_rep_end && (cts * base_ds->timescale >= base_ds->force_rep_end * ds->timescale) ) {
				seg_over = GF_TRUE;
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Inputs duration do not match, %s truncated to %g duration\n", ds->src_url, ((Double)base_ds->force_rep_end)/base_ds->timescale ));
				ds->done = GF_TRUE;
				gf_filter_pid_set_eos(ds->opid);
				gf_filter_pid_set_discard(ds->ipid, GF_TRUE);
				
			} else if (cts * base_ds->timescale >= base_ds->adjusted_next_seg_start * ds->timescale ) {
				//no sap, segment is over
				if (ctx->no_sap) {
					seg_over = GF_TRUE;
				}
				// sap, segment is over
				else if (sap_type) {

					if (sap_type==3)
						ds->nb_sap_3 ++;
					else if (sap_type>3)
						ds->nb_sap_4 ++;

					/*check requested profiles can be generated, or adjust them*/
					if ((ctx->profile != GF_DASH_PROFILE_FULL) && (ds->nb_sap_4 || (ds->nb_sap_3 > 1)) ) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] WARNING! Max SAP type %d detected - switching to FULL profile\n", ds->nb_sap_4 ? 4 : 3));
						ctx->profile = GF_DASH_PROFILE_FULL;
						ds->set->starts_with_sap = sap_type;
					}

					seg_over = GF_TRUE;
					if (ds == base_ds) {
						base_ds->adjusted_next_seg_start = cts;
					}
				}
			}
			if (seg_over) {
				assert(!ds->seg_done);
				ds->seg_done = GF_TRUE;
				ds->first_cts_in_next_seg = cts;
				base_ds->nb_comp_done ++;
				if (base_ds->nb_comp_done == base_ds->nb_comp) {
					dasher_flush_segment(ctx, base_ds);
				}
				break;
			}

			ncts = cts + gf_filter_pck_get_duration(pck);
			if (ncts>ds->est_first_cts_in_next_seg)
				ds->est_first_cts_in_next_seg = ncts;

			ncts *= 1000;
			ncts /= ds->timescale;
			if (ncts>base_ds->max_period_dur)
				base_ds->max_period_dur = ncts;


			if (!ds->segment_started) {
				GF_FilterPacket *dst = gf_filter_pck_new_ref(ds->opid, NULL, 0, pck);
				gf_filter_pck_merge_properties(pck, dst);
				ds->first_cts_in_seg = cts;
				dasher_mark_segment_start(ctx, ds, dst);

				if (ds->force_timescale) {
					dasher_update_pck_times(ds, dst);
				}
				gf_filter_pck_send(dst);

				ds->segment_started = GF_TRUE;
			} else if (ds->force_timescale) {
				GF_FilterPacket *dst = gf_filter_pck_new_ref(ds->opid, NULL, 0, pck);
				gf_filter_pck_merge_properties(pck, dst);
				dasher_update_pck_times(ds, dst);
				gf_filter_pck_send(dst);
			} else {
				//forward packet
				gf_filter_pck_forward(pck, ds->opid);
			}
			gf_filter_pid_drop_packet(ds->ipid);
		}
	}
	nb_init=0;
	for (i=0; i<count; i++) {
		GF_DashStream *ds = gf_list_get(ctx->current_period->streams, i);
		if (ds->done) nb_init++;
	}
	//still some running steams in period
	if (count && (nb_init<count)) return GF_OK;

	//we need to wait for full flush of packets before switching periods in order to get the
	//proper segment size for segment_list+byte_range mode
	if (ctx->nb_seg_url_pending)
		return GF_OK;
	if (ctx->single_segment && !ctx->on_demand_done) return GF_OK;

	//done with this period, do period switch - this will update the MPD if needed
	e = dasher_switch_period(filter, ctx);
	//no more periods
	if (e==GF_EOS) {
		ctx->is_eos = GF_TRUE;
		gf_filter_pid_set_eos(ctx->opid);
	}
	return e;
}

static Bool dasher_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i, count;
	Bool flush_mpd = GF_FALSE;
	GF_DasherCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type != GF_FEVT_SEGMENT_SIZE) return GF_FALSE;

	count = gf_list_count(ctx->pids);
	for (i=0; i<count; i++) {
		u64 r_start, r_end;
		GF_DashStream *ds = gf_list_get(ctx->pids, i);
		if (ds->opid != evt->base.on_pid) continue;

		if (ds->share_rep) continue;

		//don't set segment sizes in template mode
		if (ctx->tpl) continue;
		//only set  size/index size for init segment when doing onDemand/single index
		if (ctx->single_segment && !evt->seg_size.is_init) continue;

		if (evt->seg_size.media_range_end) {
			r_start = evt->seg_size.media_range_start;
			r_end = evt->seg_size.media_range_end;
		} else {
			r_start = evt->seg_size.idx_range_start;
			r_end = evt->seg_size.idx_range_end;
		}
		//init segment or representation index, set it in on demand and main single source
		if (ctx->single_file && (evt->seg_size.is_init==1))  {
			GF_MPD_URL *url, **s_url;

			if (ds->rep->segment_base && !evt->seg_size.media_range_end) {
				if (! ds->rep->segment_base->index_range) {
					GF_SAFEALLOC(ds->rep->segment_base->index_range, GF_MPD_ByteRange);
				}
				ds->rep->segment_base->index_range->start_range = r_start;
				ds->rep->segment_base->index_range->end_range = r_end;
				ds->rep->segment_base->index_range_exact = GF_TRUE;
				flush_mpd = GF_TRUE;
				continue;
			}

			GF_SAFEALLOC(url, GF_MPD_URL);
			GF_SAFEALLOC(url->byte_range, GF_MPD_ByteRange);
			url->byte_range->start_range = r_start;
			url->byte_range->end_range = r_end;

			s_url = NULL;
			if (ds->rep->segment_base) {
				if (evt->seg_size.media_range_end) s_url = &ds->rep->segment_base->initialization_segment;
			} else {
				assert(ds->rep->segment_list);
				if (evt->seg_size.media_range_end) s_url = &ds->rep->segment_list->initialization_segment;
				else s_url = &ds->rep->segment_list->representation_index;
			}
			assert(s_url);
			if (*s_url) gf_mpd_url_free(*s_url);
			*s_url = url;
		} else if (ds->rep->segment_list && !evt->seg_size.is_init) {
			GF_MPD_SegmentURL *url = gf_list_pop_front(ds->seg_urls);
			assert(url);
			assert(ctx->nb_seg_url_pending);
			ctx->nb_seg_url_pending--;

			if (!url->media && ctx->single_file) {
				GF_SAFEALLOC(url->media_range, GF_MPD_ByteRange);
				url->media_range->start_range = evt->seg_size.media_range_start;
				url->media_range->end_range = evt->seg_size.media_range_end;
			}
			if (evt->seg_size.idx_range_end) {
				GF_SAFEALLOC(url->index_range, GF_MPD_ByteRange);
				url->index_range->start_range = evt->seg_size.idx_range_start;
				url->index_range->end_range = evt->seg_size.idx_range_end;
			}
		}
	}
	if (!ctx->single_segment || !flush_mpd) return GF_TRUE;

	flush_mpd = GF_TRUE;
	for (i=0; i<count; i++) {
		GF_DashStream *ds = gf_list_get(ctx->pids, i);
		if (!ds->rep) continue;
		if (! ds->rep->segment_base) continue;
		if (ds->rep->segment_base->index_range) continue;
		flush_mpd = GF_FALSE;
		break;
	}
	if (flush_mpd) {
		ctx->on_demand_done = GF_TRUE;
	}
	return GF_TRUE;
}

static GF_Err dasher_setup_profile(GF_DasherCtx *ctx)
{
	switch (ctx->profile) {
	case GF_DASH_PROFILE_AVC264_LIVE:
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		if (ctx->cp == GF_DASH_CPMODE_REPRESENTATION) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[Dasher] ERROR! The selected DASH profile (DASH-IF IOP) requires the ContentProtection element to be present in the AdaptationSet element.\n"));
			return GF_BAD_PARAM;
		}
	default:
		break;
	}

	/*adjust params based on profiles*/
	switch (ctx->profile) {
	case GF_DASH_PROFILE_LIVE:
		ctx->no_sap = ctx->no_seg_align = GF_FALSE;
		ctx->tpl = GF_TRUE;
		ctx->single_segment = ctx->single_file = GF_FALSE;
		break;
	case GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE:
		ctx->check_main_role = GF_TRUE;
		ctx->bs_switch = DASHER_BS_SWITCH_MULTI;
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[Dasher] HBBTV1.5 profile not yet ported to filter architecture.\n"));
		//FALLTHROUGH
	case GF_DASH_PROFILE_AVC264_LIVE:
		ctx->no_sap = ctx->no_seg_align = GF_FALSE;
		ctx->no_fragments_defaults = GF_TRUE;
		ctx->tpl = GF_TRUE;
		ctx->single_segment = ctx->single_file = GF_FALSE;
		break;
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		ctx->no_sap = ctx->no_seg_align = GF_FALSE;
		ctx->no_fragments_defaults = GF_TRUE;
		ctx->single_segment = GF_TRUE;
		ctx->tpl = GF_FALSE;
		break;
	case GF_DASH_PROFILE_ONDEMAND:
		ctx->no_sap = ctx->no_seg_align = GF_FALSE;
		ctx->single_segment = GF_TRUE;
		ctx->tpl = GF_FALSE;
		if ((ctx->bs_switch != DASHER_BS_SWITCH_DEF) && (ctx->bs_switch != DASHER_BS_SWITCH_OFF)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] onDemand profile, bitstream switching mode cannot be used, defaulting to off.\n"));
		}
		/*BS switching is meaningless in onDemand profile*/
		ctx->bs_switch = DASHER_BS_SWITCH_OFF;
		ctx->single_file = GF_TRUE;
		break;
	case GF_DASH_PROFILE_MAIN:
		ctx->no_sap = ctx->no_seg_align = GF_FALSE;
		ctx->single_segment = GF_FALSE;
		ctx->tpl = GF_FALSE;
		break;
	default:
		break;
	}
		//commented out, not sure why we had inband by default in live
	if (ctx->bs_switch == DASHER_BS_SWITCH_DEF) {
#if 0
		ctx->bs_switch = DASHER_BS_SWITCH_INBAND;
#else
		ctx->bs_switch = DASHER_BS_SWITCH_ON;
#endif

	}

	if (ctx->no_seg_align) {
		if (ctx->profile) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Segments are not time-aligned in each representation of each period\n\tswitching to FULL profile\n"));
			ctx->profile = GF_DASH_PROFILE_FULL;
		}
		//commented out, this does not seem correct since BS switching is orthogonal to segment alignment
		//one could have inband params working even in non time-aligned setup
#if 0
		if (ctx->bs_switch != DASHER_BS_SWITCH_OFF) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] Segments are not time-aligned in each representation of each period\n\tdisabling bitstream switching\n"));
			ctx->bs_switch = DASHER_BS_SWITCH_OFF;
		}
#endif

	}

	//check we have a segment template
	if (!ctx->template) {
		ctx->template = gf_strdup( ctx->single_file ? "$File$_dash" : "$File$_$Number$" );
		if (ctx->tpl) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[Dasher] DASH Live profile requested but no template specified, using %s\n", ctx->template));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[Dasher] No template assigned, using %s\n", ctx->template));
		}
	}

	if (ctx->single_segment) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASH-ing single segment\nSubsegment duration %.3f - Fragment duration: %.3f secs\n", ctx->dur, ctx->fdur));
		ctx->subs_per_sidx = 0;
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASH-ing %.2fs segments %.2fs fragments ", ctx->dur, ctx->fdur));
		if (!ctx->sidx) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("no sidx used"));
		} else if (ctx->subs_per_sidx) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("%d subsegments per sidx", ctx->subs_per_sidx));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("single sidx per segment"));
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\n"));
	}
	return GF_OK;
}

static GF_Err dasher_initialize(GF_Filter *filter)
{
	GF_Err e;
	GF_DasherCtx *ctx = gf_filter_get_udta(filter);
	gf_filter_sep_max_extra_input_pids(filter, -1);

	ctx->pids = gf_list_new();

	e = dasher_setup_profile(ctx);
	if (e) return e;

	if (!ctx->ext) ctx->ext = "m4s";
	if (ctx->single_file && ctx->tpl)
		ctx->tpl = GF_FALSE;

	ctx->current_period = dasher_new_period();
	ctx->next_period = dasher_new_period();
	ctx->on_demand_done = GF_TRUE;
	return GF_OK;
}


static void dasher_finalize(GF_Filter *filter)
{
	GF_DasherCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->pids)) {
		GF_DashStream *ds = gf_list_pop_back(ctx->pids);
		dasher_reset_stream(ds, GF_TRUE);
		gf_free(ds);
	}
	gf_list_del(ctx->pids);
	if (ctx->mpd) gf_mpd_del(ctx->mpd);

	if (ctx->next_period->period) gf_mpd_period_free(ctx->next_period->period);
	gf_list_del(ctx->current_period->streams);
	gf_free(ctx->current_period);
	gf_list_del(ctx->next_period->streams);
	gf_free(ctx->next_period);
}

static const GF_FilterCapability DasherCaps[] =
{
	//for now don't accept files as input, although we could store them as items, to refine
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//only framed
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mpd|m3u8"),
};


#define OFFS(_n)	#_n, offsetof(GF_DasherCtx, _n)
static const GF_FilterArgs DasherArgs[] =
{
	{ OFFS(dur), "DASH target duration in seconds", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{ OFFS(single_segment), "single segment is used", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(tpl), "use template mode (multiple segment, template URLs)", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(stl), "use segment timeline (ignored in on_demand mode)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(dynamic), "MPD is dynamic (live generation)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(single_file), "Segments are contained in a single file (default in on_demand)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(no_seg_align), "Disables segment time alignment between representations", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(no_sap), "Disables spliting segments at SAP boundaries", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(mix_codecs), "Enables mixing different codecs in an adaptation set", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(no_sar), "Does not check for identical sample aspect ratio for adaptation sets", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(for_test), "sets all dates and version info to 0 to enforce same binary result generation", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(forcep), "forces profile string for avc/hevc/aac", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(bs_switch), "Bitstream switching mode (single init segment):\n\tdef: resolves to off for onDemand and inband for live\n\toff: disables BS switching\n\ton: enables it if same decoder configuration is possible\n\tinband: moves decoder config inband if possible\n\tforce: enables it even if only one representation\n\tmulti: uses multiple stsd entries in ISOBMFF", GF_PROP_UINT, "def", "def|off|on|inband|force|multi", GF_FALSE},
	{ OFFS(avcp), "AVC|H264 profile to use if no profile could be found. If forcep is set, enforces this profile", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(hvcp), "HEVC profile to use if no profile could be found. If forcep is set, enforces this profile", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(template), "DASH template string to use to generate segment name - see filter help", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(ext), "File extension to use for segments", GF_PROP_STRING, "m4s", NULL, GF_FALSE},
	{ OFFS(asto), "AvailabilityStartTime offset to use", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(profile), "Specifies the target DASH profile. This will set default option values to ensure conformance to the desired profile. Auto turns profile to live for dynamic and full for non-dynamic.", GF_PROP_UINT, "auto", "auto|live|onDemand|main|full|hbbtv1.5.live|dashavc264.live|dashavc264.onDemand", GF_FALSE },
	{ OFFS(profX), "specifies a list of profile extensions, as used by DASH-IF and DVB. The string will be colon-concatenated with the profile used", GF_PROP_STRING, NULL, NULL, GF_FALSE },
	{ OFFS(cp), "Specifies the content protection element location", GF_PROP_UINT, "set", "set|rep|both", GF_FALSE },
	{ OFFS(fdur), "DASH fragment duration in seconds", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{ OFFS(buf), "DASH min buffer duration in ms. negative value means percent of segment duration (eg -150 = 1.5*seg_dur)", GF_PROP_SINT, "-100", NULL, GF_FALSE},
	{ OFFS(timescale), "sets timescales for timeline and segment list/template. A value of 0 picks up the first timescale of the first stream in an adaptation set. A negative value forces using stream timescales for each timed element (multiplication of segment list/template/timelines). A positive value enforces the MPD timescale", GF_PROP_SINT, "0", NULL, GF_FALSE},
	{ OFFS(check_dur), "checks duration of sources in period, trying to have roughly equal duration", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(skip_seg), "increments segment number whenever an empty segment would be produced - NOT DASH COMPLIANT", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(title), "sets MPD title", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(source), "sets MPD Source", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(info), "sets MPD info url", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(cprt), "adds copyright string to MPD", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(lang), "sets lang of MPD Info", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(location), "sets MPD locations to given URL", GF_PROP_STRING_LIST, NULL, NULL, GF_FALSE},
	{ OFFS(base), "sets base URLs of MPD", GF_PROP_STRING_LIST, NULL, NULL, GF_FALSE},
	{0}
};


GF_FilterRegister DasherRegister = {
	.name = "dasher",
	.description = "MPEG-DASH / HLS / Smooth segmenter",
	.comment = "GPAC DASH segmenter\n"\
			"The segmenter uses the template string to derive output file names, regardless of the DASH mode (even when templates are not used)\n"\
			"\tEX: template=Great_$File$_$Width$_$Number$ on 640x360 foo.mp4 source will resolve in Great_foo_640_$Number$ for the DASH template\n"\
			"\tEX: template=Great_$File$_$Width$ on 640x360 foo.mp4 source will resolve in Great_foo_640.mp4 for onDemand case\n"\
			"The default template is $File$_dash for ondemand and single file modes, and $File$_$Number$ for seperate segment files\n"\
			"\n"\
			"To assign PIDs into periods and adaptation sets, the dasher looks for these properties on each input pid:\n"\
			"\tRepresentation: assigns representation ID to input pid. If not set, the default behaviour is to have each media compenent in different adaptation sets. Setting the RepresentationID allows explicit multiplexing of the source(s)\n"\
			"\tPeriod: assigns period ID to input pid. If not set, the default behaviour is to have all media in the same period\n"
			"\tPID properties: Bitrate, SAR, Language, Width, Height, SampleRate, NumChannels, Language, ID, DependencyID, FPS, Interlaced, Role, PDesc, ASDesc, ASCDesc, RDesc, BUrl. These properties are used to setup the manifest and can be overriden on input PIDs using the general PID property settings (cf global help).\n"\
			"\tEX: \"src=test.mp4:#Bitrate=1M dst=test.mpd\" will force declaring a bitrate of 1M for the representation, regardless of actual source bitrate\n"\
			"\tEX: \"src=muxav.mp4 dst=test.mpd\" will create unmuxed DASH segments\n"\
			"\tEX: \"src=muxav.mp4:#Representation=1 dst=test.mpd\" will create muxed DASH segments\n"\
			"\tEX: \"src=m1.mp4 src=m2.mp4:#Period=Yep dst=test.mpd\" will put src m1.mp4 in first period, m2.mp4 in second period\n"\
			"\tEX: \"src=m1.mp4:#BUrl=http://foo/bar dst=test.mpd\" will assign a base URL to src m1.mp4\n"\
			"\tEX: \"src=m1.mp4:#ASCDesc=<ElemName val=\"attval\">text</ElemName> dst=test.mpd\" will assign the specified XML descriptor to the adaptation set.\n"\
			"\t\tNote that most XML descriptor injector can be used to inject most DASH descriptors not natively handled by the dasher\n"\
			"\t\tThe dasher handles the XML descriptor as a string and does not attempt to validate it.\n"\
			"\t\tDescriptors, as well as some dasher filter arguments, are string lists (comma-separated), so that multiple descriptors can be added:\n"\
			"\tEX: \"src=m1.mp4:#RDesc=<Elem attribute=\"1\"/>,<Elem2>text</Elem2> dst=test.mpd\" will insert two descriptors in the representation(s) of m1.mp4\n"\
			"The dasher will create filter chains for the segmenter based on the destination URL path, and will reassign PID IDs\n"\
			"so that each media component (video, audio, ...) in an adaptation set has the same ID\n"\
			"\n"\
			"Note to developpers: output muxers allowing segmented output must obey the following:\n"\
			"* add a \"DashMode\" capability to their input caps (value of the cap is ignored, only its presence is required)\n"\
			"* inspect packet properties, \"FileNumber\" giving the signal of a new DASH segment, \"FileName\" giving the optional file name (if not present, ouput shall be a single file)\n"\
			"* for each segment done, send a downstream event on the first connected PID signaling the size of the segment and the size of its index if any\n"\
			"* for muxers with init data, send a downstream event signaling the size of the init and the size of the global index if any\n"\
			"* the following filter options are passed to muxers, which should declare them as arguments:\n"\
			"\t\tnoinit: disables output of init segment for the muxer (used to handle bitstream switching with single init in DASH)\n"\
			"\t\tfrag: indicates muxer shall used fragmented format (used for ISOBMFF mostly)\n"\
			"\t\tsubs_sidx=0: indicates an SIDX shall be generated - only added if not already specified by user\n"\
			"\t\txps_inband=all|no: indicates AVC/HEVC/... parameter sets shall be sent inband or out of band\n"\
			"\t\tno_frags_def: indicates fragment defaults should be set in each segment rather than in init segment\n"\
			"\n"\
			"The dasher will add the following properties to the output PIDs:\n"\
			"* DashMode: identifies VoD or regular DASH mode used by dasher\n"\
			"* DashDur: identifies target DASH segment duration - this can be used to estimate the SIDX size for example\n"\
			,
	.private_size = sizeof(GF_DasherCtx),
	.args = DasherArgs,
	.initialize = dasher_initialize,
	.finalize = dasher_finalize,
	SETCAPS(DasherCaps),
	.configure_pid = dasher_configure_pid,
	.process = dasher_process,
	.process_event = dasher_process_event,
};


const GF_FilterRegister *dasher_register(GF_FilterSession *session)
{
	return &DasherRegister;
}
