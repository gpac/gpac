/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre , Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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

#include <gpac/media_tools.h>
#include <gpac/network.h>
#include <gpac/internal/mpd.h>
#include <gpac/filters.h>


struct __gf_dash_segmenter
{
	GF_FilterSession *fsess;
	GF_Filter *output;

	GF_List *inputs;

	char *title, *copyright, *moreInfoURL, *sourceInfo, *lang;
	char *locations, *base_urls;
	char *mpd_name;
	GF_DashProfile profile;

	GF_DashDynamicMode dash_mode;
	u32 use_url_template;
	Bool use_segment_timeline;
	Bool single_segment;
	Bool single_file;
	GF_DashSwitchingMode bitstream_switching_mode;
	Bool segments_start_with_rap;

	Double segment_duration;
	Double fragment_duration;
	Double sub_duration;
	//has to be freed
	char *seg_rad_name;
	const char *seg_ext;
	const char *seg_init_ext;
	const char *init_seg_ext;
	u32 segment_marker_4cc;
	Bool enable_sidx;
	u32 subsegs_per_sidx;
	Bool daisy_chain_sidx, use_ssix;

	Bool fragments_start_with_rap;
	char *tmpdir;
	Double mpd_update_time;
	s32 time_shift_depth;
	u32 min_buffer_time;
	s32 ast_offset_ms;
	u32 dash_scale;
	Bool fragments_in_memory;
	u32 initial_moof_sn;
	u64 initial_tfdt;
	Bool no_fragments_defaults;

	GF_DASHPSSHMode pssh_mode;
	Bool samplegroups_in_traf;
    Bool single_traf_per_moof, single_trun_per_traf;
	Bool tfdt_per_traf;
	Double mpd_live_duration;
	Bool insert_utc;
	Bool real_time;
	char *utc_start_date;

	const char *dash_profile_extension;

	GF_DASH_ContentLocationMode cp_location_mode;

	Bool no_cache;

	Bool disable_loop;

	/* NOT YET BACKPORTED indicates if a segment must contain its theorical boundary */
	Bool split_on_bound;

	/* NOT YET BACKPORTED  used to segment video as close to the boundary as possible */
	Bool split_on_closest;

	Bool mvex_after_traks;

	//some HLS options
	Bool hls_clock;

	const char *cues_file;
	Bool strict_cues;

	//not yet exposed through API
	Bool disable_segment_alignment;
	Bool enable_mix_codecs;
	Bool enable_sar_mix;
	Bool check_duration;

	const char *dash_state;

	u64 next_gen_ntp_ms;
	u64 mpd_time_ms;

	Bool dash_mode_changed;
	u32 print_stats_graph;
};


GF_EXPORT
u32 gf_dasher_next_update_time(GF_DASHSegmenter *dasher, u64 *ms_in_session)
{
	s64 diff = 0;
	if (dasher->next_gen_ntp_ms) {
		diff = (s64) dasher->next_gen_ntp_ms;
		diff -= (s64) gf_net_get_ntp_ms();
	}
	if (ms_in_session) *ms_in_session = dasher->mpd_time_ms;
	return diff>0 ? (u32) diff : 1;
}


GF_EXPORT
GF_DASHSegmenter *gf_dasher_new(const char *mpdName, GF_DashProfile dash_profile, const char *tmp_dir, u32 dash_timescale, const char *dasher_context_file)
{
	GF_DASHSegmenter *dasher;
	GF_SAFEALLOC(dasher, GF_DASHSegmenter);
	if (!dasher) return NULL;

	dasher->mpd_name = gf_strdup(mpdName);

	dasher->dash_scale = dash_timescale ? dash_timescale : 1000;
	if (tmp_dir) dasher->tmpdir = gf_strdup(tmp_dir);
	dasher->profile = dash_profile;
	dasher->dash_state = dasher_context_file;
	dasher->inputs = gf_list_new();
	return dasher;
}

GF_EXPORT
void gf_dasher_set_start_date(GF_DASHSegmenter *dasher, const char *dash_utc_start_date)
{
	if (!dasher) return;
	if (dasher->utc_start_date) gf_free(dasher->utc_start_date);
	dasher->utc_start_date = dash_utc_start_date ? gf_strdup(dash_utc_start_date) : NULL;
}

GF_EXPORT
void gf_dasher_clean_inputs(GF_DASHSegmenter *dasher)
{
	gf_list_reset(dasher->inputs);
	if (dasher->fsess) {
		gf_fs_del(dasher->fsess);
		dasher->fsess = NULL;
	}
}

GF_EXPORT
void gf_dasher_del(GF_DASHSegmenter *dasher)
{
	if (dasher->seg_rad_name) gf_free(dasher->seg_rad_name);
	gf_dasher_clean_inputs(dasher);
	gf_free(dasher->tmpdir);
	gf_free(dasher->mpd_name);
	if (dasher->title) gf_free(dasher->title);
	if (dasher->moreInfoURL) gf_free(dasher->moreInfoURL);
	if (dasher->sourceInfo) gf_free(dasher->sourceInfo);
	if (dasher->copyright) gf_free(dasher->copyright);
	if (dasher->lang) gf_free(dasher->lang);
	if (dasher->locations) gf_free(dasher->locations);
	if (dasher->base_urls) gf_free(dasher->base_urls);
	if (dasher->utc_start_date) gf_free(dasher->utc_start_date);
	gf_list_del(dasher->inputs);
	gf_free(dasher);
}

GF_EXPORT
GF_Err gf_dasher_set_info(GF_DASHSegmenter *dasher, const char *title, const char *copyright, const char *moreInfoURL, const char *sourceInfo, const char *lang)
{
	if (!dasher) return GF_BAD_PARAM;

#define DOSET(_field) \
	if (dasher->_field) gf_free(dasher->_field);\
	dasher->_field = _field ? gf_strdup(_field) : NULL;\

	DOSET(title)
	DOSET(copyright)
	DOSET(moreInfoURL)
	DOSET(sourceInfo);
	DOSET(lang);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_location(GF_DASHSegmenter *dasher, const char *location)
{
	if (!dasher) return GF_BAD_PARAM;

	if (!location) return GF_OK;
	return gf_dynstrcat(&dasher->locations, location, ",");
}

GF_EXPORT
GF_Err gf_dasher_add_base_url(GF_DASHSegmenter *dasher, const char *base_url)
{
	if (!dasher) return GF_BAD_PARAM;

	if (!base_url) return GF_OK;
	return gf_dynstrcat(&dasher->base_urls, base_url, ",");
}

static void dasher_format_seg_name(GF_DASHSegmenter *dasher, const char *inName)
{
	if (dasher->seg_rad_name) gf_free(dasher->seg_rad_name);
	dasher->seg_rad_name = NULL;
	if (inName) dasher->seg_rad_name = gf_strdup(inName);
}

GF_EXPORT
GF_Err gf_dasher_enable_url_template(GF_DASHSegmenter *dasher, Bool enable, const char *default_template, const char *default_extension, const char *default_init_extension)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->use_url_template = enable;
	dasher->seg_ext = default_extension;
	dasher->seg_init_ext = default_init_extension;
	dasher_format_seg_name(dasher, default_template);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_segment_timeline(GF_DASHSegmenter *dasher, Bool enable)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->use_segment_timeline = enable;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_single_segment(GF_DASHSegmenter *dasher, Bool enable)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->single_segment = enable;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_single_file(GF_DASHSegmenter *dasher, Bool enable)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->single_file = enable;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_switch_mode(GF_DASHSegmenter *dasher, GF_DashSwitchingMode bitstream_switching)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->bitstream_switching_mode = bitstream_switching;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_durations(GF_DASHSegmenter *dasher, Double default_segment_duration, Double default_fragment_duration, Double sub_duration)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->segment_duration = default_segment_duration * 1000 / dasher->dash_scale;
	if (default_fragment_duration)
		dasher->fragment_duration = default_fragment_duration * 1000 / dasher->dash_scale;
	else
		dasher->fragment_duration = dasher->segment_duration;
	dasher->sub_duration = sub_duration;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_rap_splitting(GF_DASHSegmenter *dasher, Bool segments_start_with_rap, Bool fragments_start_with_rap)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->segments_start_with_rap = segments_start_with_rap;
	dasher->fragments_start_with_rap = fragments_start_with_rap;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_segment_marker(GF_DASHSegmenter *dasher, u32 segment_marker_4cc)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->segment_marker_4cc = segment_marker_4cc;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_print_session_info(GF_DASHSegmenter *dasher, u32 fs_print_flags)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->print_stats_graph = fs_print_flags;
	return GF_OK;

}


GF_EXPORT
GF_Err gf_dasher_enable_sidx(GF_DASHSegmenter *dasher, Bool enable_sidx, u32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool use_ssix)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->enable_sidx = enable_sidx;
	dasher->subsegs_per_sidx = subsegs_per_sidx;
	dasher->daisy_chain_sidx = daisy_chain_sidx;
	dasher->use_ssix = use_ssix;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_dynamic_mode(GF_DASHSegmenter *dasher, GF_DashDynamicMode dash_mode, Double mpd_update_time, s32 time_shift_depth, Double mpd_live_duration)
{
	if (!dasher) return GF_BAD_PARAM;
	if (dasher->dash_mode != dash_mode) {
		dasher->dash_mode = dash_mode;
		dasher->dash_mode_changed = GF_TRUE;
	}
	dasher->time_shift_depth = time_shift_depth;
	dasher->mpd_update_time = mpd_update_time;
	dasher->mpd_live_duration = mpd_live_duration;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_min_buffer(GF_DASHSegmenter *dasher, Double min_buffer)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->min_buffer_time = (u32)(min_buffer*1000);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_ast_offset(GF_DASHSegmenter *dasher, s32 ast_offset_ms)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->ast_offset_ms = ast_offset_ms;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_memory_fragmenting(GF_DASHSegmenter *dasher, Bool fragments_in_memory)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->fragments_in_memory = fragments_in_memory;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_initial_isobmf(GF_DASHSegmenter *dasher, u32 initial_moof_sn, u64 initial_tfdt)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->initial_moof_sn = initial_moof_sn;
	dasher->initial_tfdt = initial_tfdt;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_configure_isobmf_default(GF_DASHSegmenter *dasher, Bool no_fragments_defaults, GF_DASHPSSHMode pssh_mode, Bool samplegroups_in_traf, Bool single_traf_per_moof, Bool tfdt_per_traf, Bool mvex_after_traks)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->no_fragments_defaults = no_fragments_defaults;
	dasher->pssh_mode = pssh_mode;
	dasher->samplegroups_in_traf = samplegroups_in_traf;
	dasher->single_traf_per_moof = single_traf_per_moof;
    dasher->tfdt_per_traf = tfdt_per_traf;
    dasher->mvex_after_traks = mvex_after_traks;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_utc_ref(GF_DASHSegmenter *dasher, Bool insert_utc)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->insert_utc = insert_utc;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_real_time(GF_DASHSegmenter *dasher, Bool real_time)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->real_time = real_time;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_content_protection_location_mode(GF_DASHSegmenter *dasher, GF_DASH_ContentLocationMode mode)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->cp_location_mode = mode;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_profile_extension(GF_DASHSegmenter *dasher, const char *dash_profile_extension)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->dash_profile_extension = dash_profile_extension;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_cached_inputs(GF_DASHSegmenter *dasher, Bool no_cache)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->no_cache = no_cache;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_enable_loop_inputs(GF_DASHSegmenter *dasher, Bool do_loop)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->disable_loop = do_loop ? GF_FALSE : GF_TRUE;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_hls_clock(GF_DASHSegmenter *dasher, Bool insert_clock)
{
       if (!dasher) return GF_BAD_PARAM;
       dasher->hls_clock = insert_clock;
       return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_split_on_bound(GF_DASHSegmenter *dasher, Bool split_on_bound)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->split_on_bound = split_on_bound;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_split_on_closest(GF_DASHSegmenter *dasher, Bool split_on_closest)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->split_on_closest = split_on_closest;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_cues(GF_DASHSegmenter *dasher, const char *cues_file, Bool strict_cues)
{
	dasher->cues_file = cues_file;
	dasher->strict_cues = strict_cues;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_add_input(GF_DASHSegmenter *dasher, const GF_DashSegmenterInput *input)
{
	if (!dasher) return GF_BAD_PARAM;

	if (!stricmp(input->file_name, "NULL") || !strcmp(input->file_name, "") || !input->file_name) {
		if (!input->xlink || !strcmp(input->xlink, "")) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No input file specified and no xlink set - cannot dash\n"));
			return GF_BAD_PARAM;
		}
	}

	gf_list_add(dasher->inputs, (void *) input);
	return GF_OK;
}

static Bool on_dasher_event(void *_udta, GF_Event *evt)
{
	u32 i, count;
	GF_DASHSegmenter *dasher = (GF_DASHSegmenter *)_udta;
	if (evt && (evt->type != GF_EVENT_PROGRESS)) return GF_FALSE;

	count = gf_fs_get_filters_count(dasher->fsess);
	for (i=0; i<count; i++) {
		GF_FilterStats stats;
		if (gf_fs_get_filter_stats(dasher->fsess, i, &stats) != GF_OK) continue;
		if (strcmp(stats.reg_name, "dasher")) continue;

		if (stats.status) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Dashing %s\r", stats.status));
		} else if (stats.percent>0) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Dashing: % 2.2f %%\r", ((Double)stats.percent)/100));
		}
		break;
	}
	return GF_FALSE;
}

/*create filter session, setup destination options and setup sources options*/
static GF_Err gf_dasher_setup(GF_DASHSegmenter *dasher)
{
	GF_Err e;
	u32 i, count;
	char *sep_ext;
	char *args=NULL, szArg[1024];

	if (!dasher->mpd_name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Missing MPD name\n"));
		return GF_OUT_OF_MEM;
	}

	dasher->fsess = gf_fs_new_defaults(0);

#ifndef GPAC_DISABLE_LOG
	if (!gf_sys_is_test_mode() && (gf_log_get_tool_level(GF_LOG_APP)!=GF_LOG_QUIET)) {
		gf_fs_enable_reporting(dasher->fsess, GF_TRUE);
		gf_fs_set_ui_callback(dasher->fsess, on_dasher_event, dasher);
	}
#endif

	if (!dasher->fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}

	sep_ext = gf_url_colon_suffix(dasher->mpd_name);
	if (sep_ext) {
		if (sep_ext[1] == '\\') sep_ext = strchr(sep_ext+1, ':');
		else if (sep_ext[1]=='/') {
			sep_ext = strchr(sep_ext+1, '/');
			if (sep_ext) sep_ext = strchr(sep_ext, ':');
		}
	}
	if (sep_ext) {
		sep_ext[0] = 0;
	}

	sprintf(szArg, "dur=%g", dasher->segment_duration);
	e = gf_dynstrcat(&args, szArg, ":");

	if (sep_ext)
		e |= gf_dynstrcat(&args, sep_ext+1, ":");

	if (dasher->single_segment) e |= gf_dynstrcat(&args, "sseg", ":");
	if (dasher->single_file) e |= gf_dynstrcat(&args, "sfile", ":");
	if (dasher->use_url_template) e |= gf_dynstrcat(&args, "tpl", ":");
	if (dasher->use_segment_timeline) e |= gf_dynstrcat(&args, "stl", ":");
	if (dasher->dash_mode) e |= gf_dynstrcat(&args, (dasher->dash_mode == GF_DASH_DYNAMIC_LAST) ? "dynlast" : "dynamic", ":");
	if (dasher->disable_segment_alignment) e |= gf_dynstrcat(&args, "!align", ":");
	if (dasher->enable_mix_codecs) e |= gf_dynstrcat(&args, "mix_codecs", ":");
	if (dasher->insert_utc) e |= gf_dynstrcat(&args, "ntp=yes", ":");
	if (dasher->enable_sar_mix) e |= gf_dynstrcat(&args, "no_sar", ":");
	//forcep not mapped
	switch (dasher->bitstream_switching_mode) {
	case GF_DASH_BSMODE_DEFAULT:
		break;
	case GF_DASH_BSMODE_NONE:
		e |= gf_dynstrcat(&args, "bs_switch=off", ":");
		break;
	case GF_DASH_BSMODE_INBAND:
		e |= gf_dynstrcat(&args, "bs_switch=inband", ":");
		break;
	case GF_DASH_BSMODE_MERGED:
		e |= gf_dynstrcat(&args, "bs_switch=on", ":");
		break;
	case GF_DASH_BSMODE_MULTIPLE_ENTRIES:
		e |= gf_dynstrcat(&args, "bs_switch=multi", ":");
		break;
	case GF_DASH_BSMODE_SINGLE:
		e |= gf_dynstrcat(&args, "bs_switch=force", ":");
		break;
	}
	//avcp, hvcp, aacp not mapped
	if (dasher->seg_rad_name) {
		sprintf(szArg, "template=%s", dasher->seg_rad_name);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->seg_ext) {
		sprintf(szArg, "segext=%s", dasher->seg_ext);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->ast_offset_ms) {
		sprintf(szArg, "asto=%d", -dasher->ast_offset_ms);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	switch (dasher->profile) {
	case GF_DASH_PROFILE_AUTO:
		break;
	case GF_DASH_PROFILE_LIVE:
		e |= gf_dynstrcat(&args, "profile=live", ":");
		break;
 	case GF_DASH_PROFILE_ONDEMAND:
		e |= gf_dynstrcat(&args, "profile=onDemand", ":");
		break;
	case GF_DASH_PROFILE_MAIN:
		e |= gf_dynstrcat(&args, "profile=main", ":");
		break;
	case GF_DASH_PROFILE_FULL:
		e |= gf_dynstrcat(&args, "profile=full", ":");
		if (!dasher->segments_start_with_rap) e |= gf_dynstrcat(&args, "!sap", ":");
		break;
	case GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE:
		e |= gf_dynstrcat(&args, "profile=hbbtv1.5.live", ":");
		break;
	case GF_DASH_PROFILE_AVC264_LIVE:
		e |= gf_dynstrcat(&args, "profile=dashavc264.live", ":");
		break;
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		e |= gf_dynstrcat(&args, "profile=dashavc264.onDemand", ":");
		break;
	}
	if (dasher->dash_profile_extension) {
		sprintf(szArg, "profX=%s", dasher->dash_profile_extension);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->cp_location_mode==GF_DASH_CPMODE_REPRESENTATION) e |= gf_dynstrcat(&args, "cp=rep", ":");
	else if (dasher->cp_location_mode==GF_DASH_CPMODE_BOTH) e |= gf_dynstrcat(&args, "cp=both", ":");

	if (dasher->min_buffer_time) {
		sprintf(szArg, "buf=%d", dasher->min_buffer_time);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->dash_scale != 1000) {
		sprintf(szArg, "timescale=%d", dasher->dash_scale);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (!dasher->check_duration) e |= gf_dynstrcat(&args, "!check_dur", ":");
	//skip_seg not exposed

	if (dasher->title) {
		sprintf(szArg, "title=%s", dasher->title);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->sourceInfo) {
		sprintf(szArg, "source=%s", dasher->sourceInfo);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->moreInfoURL) {
		sprintf(szArg, "info=%s", dasher->moreInfoURL);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->copyright) {
		sprintf(szArg, "cprt=%s", dasher->copyright);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->lang) {
		sprintf(szArg, "lang=%s", dasher->lang);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->locations) {
		sprintf(szArg, "location=%s", dasher->locations);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->base_urls) {
		sprintf(szArg, "base=%s", dasher->base_urls);
		e |= gf_dynstrcat(&args, szArg, ":");
	}

	if (dasher->dash_mode >= GF_DASH_DYNAMIC) {
		if (dasher->time_shift_depth<0) e |= gf_dynstrcat(&args, "tsb=-1", ":");
		else {
			sprintf(szArg, "tsb=%g", ((Double)dasher->time_shift_depth)/1000);
			e |= gf_dynstrcat(&args, szArg, ":");
		}

		if (dasher->utc_start_date) {
			sprintf(szArg, "ast=%s", dasher->utc_start_date);
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		if (dasher->mpd_update_time) {
			sprintf(szArg, "refresh=%g", dasher->mpd_update_time);
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		else {
			sprintf(szArg, "refresh=-%g", dasher->mpd_live_duration);
			e |= gf_dynstrcat(&args, szArg, ":");
		}
	}
	if (dasher->sub_duration) {
		sprintf(szArg, "subdur=%g", dasher->sub_duration);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->dash_state) {
		sprintf(szArg, "state=%s", dasher->dash_state);
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (! dasher->disable_loop && dasher->dash_state) e |= gf_dynstrcat(&args, "loop", ":");
	if (dasher->hls_clock) e |= gf_dynstrcat(&args, "hlsc", ":");

	//the rest is not yet exposed through the old api, but can be set through output file name

	if (dasher->dash_mode>=GF_DASH_DYNAMIC) {
	 	sprintf(szArg, "_p_gentime=%p", &dasher->next_gen_ntp_ms);
	 	e |= gf_dynstrcat(&args, szArg, ":");
	 	sprintf(szArg, "_p_mpdtime=%p", &dasher->mpd_time_ms);
	 	e |= gf_dynstrcat(&args, szArg, ":");
	}

	//append ISOBMFF options
	if (dasher->fragment_duration) {
		Double diff = dasher->fragment_duration;
		diff -= dasher->segment_duration;
		if (diff<0) diff = -diff;
		if (diff > 0.01) {
			sprintf(szArg, "cdur=%g", dasher->fragment_duration);
			e |= gf_dynstrcat(&args, szArg, ":");
		}
	}
	if (dasher->segment_marker_4cc) {
		sprintf(szArg, "m4cc=%s", gf_4cc_to_str(dasher->segment_marker_4cc) );
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->daisy_chain_sidx) e |= gf_dynstrcat(&args, "chain_sidx", ":");
	if (dasher->use_ssix) e |= gf_dynstrcat(&args, "ssix", ":");
	if (dasher->initial_moof_sn) {
		sprintf(szArg, "msn=%d", dasher->initial_moof_sn );
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->initial_tfdt) {
		sprintf(szArg, "tfdt="LLU"", dasher->initial_tfdt );
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->no_fragments_defaults) e |= gf_dynstrcat(&args, "nofragdef", ":");
	if (dasher->single_traf_per_moof) e |= gf_dynstrcat(&args, "straf", ":");
	if (dasher->single_trun_per_traf) e |= gf_dynstrcat(&args, "strun", ":");
	switch (dasher->pssh_mode) {
	case GF_DASH_PSSH_MOOV:
	case GF_DASH_PSSH_MOOV_MPD:
		break;
	case GF_DASH_PSSH_MOOF:
	case GF_DASH_PSSH_MOOF_MPD:
		e |= gf_dynstrcat(&args, "pssh=moof", ":");
		break;
	case GF_DASH_PSSH_MPD:
		e |= gf_dynstrcat(&args, "pssh=none", ":");
		break;
	}
	if (dasher->samplegroups_in_traf) e |= gf_dynstrcat(&args, "sgpd_traf", ":");
	if (dasher->enable_sidx) {
		sprintf(szArg, "subs_sidx=%d", dasher->subsegs_per_sidx );
		e |= gf_dynstrcat(&args, szArg, ":");
	}

	if (dasher->fragments_start_with_rap) e |= gf_dynstrcat(&args, "sfrag", ":");
	if (dasher->tmpdir) {
		sprintf(szArg, "tmpd=%s", dasher->tmpdir );
		e |= gf_dynstrcat(&args, szArg, ":");
	}

	if (dasher->cues_file) {
		sprintf(szArg, "cues=%s", dasher->cues_file );
		e |= gf_dynstrcat(&args, szArg, ":");
	}
	if (dasher->strict_cues) e |= gf_dynstrcat(&args, "strict_cues", ":");

	if (dasher->mvex_after_traks) e |= gf_dynstrcat(&args, "mvex", ":");


	dasher->dash_mode_changed = GF_FALSE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Instantiating dasher filter for dst %s with args %s\n", dasher->mpd_name, args));

	if (e) {
		if (args) gf_free(args);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to setup DASH filter arguments\n"));
		return e;
	}
	dasher->output = gf_fs_load_destination(dasher->fsess, dasher->mpd_name, args, NULL, &e);

	if (args) gf_free(args);

	if (sep_ext) {
		sep_ext[0] = ':';
	}

	if (!dasher->output) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load DASH filter\n"));
		return e;
	}

	//and setup sources
	count = gf_list_count(dasher->inputs);
	for (i=0; i<count; i++) {
		u32 j;
		GF_Filter *src = NULL;
		GF_Filter *rt = NULL;
		const char *url = "null";
		char *frag=NULL;
		GF_DashSegmenterInput *di = gf_list_get(dasher->inputs, i);

		if (dasher->real_time) {
			rt = gf_fs_load_filter(dasher->fsess, "reframer:rt=sync");
		}
		if (di->file_name && strlen(di->file_name)) url = di->file_name;
		if (!stricmp(url, "null")) url = NULL;
		if (url) {
			frag = strrchr(di->file_name, '#');
			if (frag) frag[0] = 0;
		}

		args = NULL;

		//if source is isobmf using extractors, we want to keep the extractors
		e = gf_dynstrcat(&args, "smode=splitx", ":");

		if (frag) {
			if (!strncmp(frag+1, "trackID=", 8)) {
				sprintf(szArg, "tkid=%s", frag+9 );
			} else {
				sprintf(szArg, "tkid=%s", frag+1);
			}
			e |= gf_dynstrcat(&args, szArg, ":");
		} else if (di->track_id) {
			sprintf(szArg, "tkid=%d", di->track_id );
			e |= gf_dynstrcat(&args, szArg, ":");
		}

		if (di->source_opts) {
			e |= gf_dynstrcat(&args, di->source_opts, ":");
		}

		//set all args
		if (di->representationID) {
			sprintf(szArg, "#Representation=%s", di->representationID );
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		if (di->periodID) {
			sprintf(szArg, "#Period=%s", di->periodID );
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		if (di->asID)  {
			sprintf(szArg, "#ASID=%d", di->asID );
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		//period start not exposed
		if (di->period_duration) {
			if (!url) {
				sprintf(szArg, "#PDur=%g", di->period_duration );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				sprintf(szArg, "#DashDur=%g", di->period_duration );
				e |= gf_dynstrcat(&args, szArg, ":");
			}
		}
		if (url && di->media_duration) {
			sprintf(szArg, "#CDur=%g", di->media_duration );
			e |= gf_dynstrcat(&args, szArg, ":");
		}

		if (di->xlink) {
			sprintf(szArg, "#xlink=%s", di->xlink );
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		if (di->bandwidth)  {
			sprintf(szArg, "#Bitrate=%d", di->bandwidth );
			e |= gf_dynstrcat(&args, szArg, ":");
			sprintf(szArg, "#Maxrate=%d", di->bandwidth );
			e |= gf_dynstrcat(&args, szArg, ":");
		}

		for (j=0;j<di->nb_baseURL; j++) {
			if (!j) {
				sprintf(szArg, "#BUrl=%s", di->baseURL[j] );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				e |= gf_dynstrcat(&args, di->baseURL[j], ",");
			}
		}
		for (j=0;j<di->nb_roles; j++) {
			if (!j) {
				sprintf(szArg, "#Role=%s", di->roles[j] );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				e |= gf_dynstrcat(&args, di->roles[j], ",");
			}
		}

		for (j=0;j<di->nb_rep_descs; j++) {
			if (!j) {
				sprintf(szArg, "#RDesc=%s", di->rep_descs[j] );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				e |= gf_dynstrcat(&args, di->rep_descs[j], ",");
			}
		}

		for (j=0;j<di->nb_p_descs; j++) {
			if (!j) {
				sprintf(szArg, "#PDesc=%s", di->p_descs[j] );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				e |= gf_dynstrcat(&args, di->p_descs[j], ",");
			}
		}

		for (j=0;j<di->nb_as_descs; j++) {
			if (!j) {
				sprintf(szArg, "#ASDesc=%s", di->as_descs[j] );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				e |= gf_dynstrcat(&args, di->as_descs[j], ",");
			}
		}

		for (j=0;j<di->nb_as_c_descs; j++) {
			if (!j) {
				sprintf(szArg, "#ASCDesc=%s", di->as_c_descs[j] );
				e |= gf_dynstrcat(&args, szArg, ":");
			} else {
				e |= gf_dynstrcat(&args, di->as_c_descs[j], ",");
			}
		}

		if (di->startNumber) {
			sprintf(szArg, "#StartNumber=%d", di->startNumber );
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		if (di->seg_template) {
			sprintf(szArg, "#Template=%s", di->seg_template );
			e |= gf_dynstrcat(&args, szArg, ":");
		}
		if (di->hls_pl) {
			sprintf(szArg, "#HLSPL=%s", di->hls_pl );
			e |= gf_dynstrcat(&args, szArg, ":");
		}

		if (di->sscale) e |= gf_dynstrcat(&args, "#SingleScale=true", ":");


		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to setup source arguments for %s\n", di->file_name));
			if (frag) frag[0] = '#';
			if (args) gf_free(args);
			return e;
		}

		if (!url) url = "null";
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Instantiating dasher source %s with args %s\n", url, args));
		src = gf_fs_load_source(dasher->fsess, url, args, NULL, &e);
		if (args) gf_free(args);
		if (frag) frag[0] = '#';

		if (!src) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load source filer for %s\n", di->file_name));
			return e;
		}

		if (rt) {
			gf_filter_set_source(rt, src, NULL);
			src = rt;
		}

		if (!di->filter_chain) continue;

		//create the filter chain between source (or rt if it was set) and dasher

		//filter chain
		GF_Filter *prev_filter=src;
		char *fargs = (char *) di->filter_chain;
		while (fargs) {
			GF_Filter *f;
			char *sep = strstr(fargs, "@@");
			if (sep) sep[0] = 0;
			f = gf_fs_load_filter(dasher->fsess, fargs);
			if (!f) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load filter %s\n", fargs));
				return GF_BAD_PARAM;
			}
			if (prev_filter) {
				gf_filter_set_source(f, prev_filter, NULL);
			}
			prev_filter = f;
			if (!sep) break;
			sep[0] = '@';
			fargs = sep+2;
		}
		if (prev_filter) {
			gf_filter_set_source(dasher->output, prev_filter, NULL);
		}
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_process(GF_DASHSegmenter *dasher)
{
	GF_Err e;
	Bool need_seek = GF_TRUE;

	/*first run, we need to extract the next gen time from context*/
	if (dasher->dash_state && gf_file_exists(dasher->dash_state) && (dasher->dash_mode>=GF_DASH_DYNAMIC) && !dasher->next_gen_ntp_ms) {
		GF_DOMParser *mpd_parser;
		GF_MPD *mpd;

		/* parse the MPD */
		mpd_parser = gf_xml_dom_new();
		e = gf_xml_dom_parse(mpd_parser, dasher->dash_state, NULL, NULL);
		if (!e) {
			mpd = gf_mpd_new();
			e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), mpd, dasher->dash_state);
			gf_xml_dom_del(mpd_parser);
			gf_mpd_del(mpd);
			dasher->next_gen_ntp_ms = mpd->gpac_next_ntp_ms;
		}

		if (dasher->next_gen_ntp_ms) {
			u64 ntp_ms = gf_net_get_ntp_ms();
			if (ntp_ms < dasher->next_gen_ntp_ms) {
				s64 diff = dasher->next_gen_ntp_ms - ntp_ms;
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] generation called too early by %d ms\n", (s32) diff));
				return GF_EOS;
			}
		}
	}

	if (!dasher->fsess) {
		e = gf_dasher_setup(dasher);
		if (e) return e;
		need_seek = GF_FALSE;
	}
	gf_fs_get_last_connect_error(dasher->fsess);
	gf_fs_get_last_process_error(dasher->fsess);

	//send change mode before sending the resume request, as the seek checks for last mode
	if (dasher->dash_mode_changed) {
		gf_filter_send_update(dasher->output, NULL, "dmode", (dasher->dash_mode == GF_DASH_DYNAMIC_LAST)  ? "dynlast" : "dynamic", GF_FILTER_UPDATE_DOWNSTREAM);
	}

	if (need_seek) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_RESUME, NULL);
		gf_filter_send_event(dasher->output, &evt);
	}

	e = gf_fs_run(dasher->fsess);
	if (e>0) e = GF_OK;

	if (dasher->print_stats_graph & 1) gf_fs_print_stats(dasher->fsess);
	if (dasher->print_stats_graph & 2) gf_fs_print_connections(dasher->fsess);

	if (!e) e = gf_fs_get_last_connect_error(dasher->fsess);
	if (!e) e = gf_fs_get_last_process_error(dasher->fsess);
	if (e<0) return e;

	on_dasher_event(dasher, NULL);
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\n"));

	if (dasher->no_cache) {
		gf_fs_del(dasher->fsess);
		dasher->fsess = NULL;
	}
	return GF_OK;
}

