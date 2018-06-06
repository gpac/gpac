/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre , Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/filters.h>


struct __gf_dash_segmenter
{
	GF_FilterSession *fsess;
	GF_Filter *filter;

	GF_List *inputs;

	char *title, *copyright, *moreInfoURL, *sourceInfo, *lang;
	char *locations, *base_urls;
	char *mpd_name;
	char *m3u8_name;

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
	u32 segment_marker_4cc;
	Bool enable_sidx;
	u32 subsegs_per_sidx;
	Bool daisy_chain_sidx;

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

	/*If true, disable generation date printing in mpd headers*/
	Bool force_test_mode;

	GF_DASH_ContentLocationMode cp_location_mode;

	Bool no_cache;

	Bool disable_loop;

	/* indicates if a segment must contain its theorical boundary */
	Bool split_on_bound;

	/* used to segment video as close to the boundary as possible */
	Bool split_on_closest;

	//not yet exposed through API
	Bool disable_segment_alignment;
	Bool enable_mix_codecs;
	Bool enable_sar_mix;
	Bool check_duration;

	const char *dash_state;
};


GF_EXPORT
u32 gf_dasher_next_update_time(GF_DASHSegmenter *dasher, u64 *ms_in_session)
{
#ifdef FILTER_FIXME

	Double past_period_dur = 0, max_dur = 0;
//	Double safety_dur;
	Double ms_elapsed;
	u32 i, ntp_sec, frac, prev_sec, prev_frac, dash_scale;
	const char *opt, *section;

	if (!dasher || !dasher->dash_ctx) return -1;

	opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration");
	if (!opt) return 0;

/*
	safety_dur = atof(opt) / 2;
	if (safety_dur > dasher->mpd->minimum_update_period)
		safety_dur = dasher->mpd->minimum_update_period;

	safety_dur = 0;
*/

	opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "GenerationNTP");
	sscanf(opt, "%u:%u", &prev_sec, &prev_frac);

	opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "TimeScale");
	sscanf(opt, "%u", &dash_scale);


	/*compute cumulated duration*/
	for (i=0; i<gf_cfg_get_section_count(dasher->dash_ctx); i++) {
		Double dur = 0;
		section = gf_cfg_get_section_name(dasher->dash_ctx, i);
		if (section && !strncmp(section, "Representation_", 15)) {
			opt = gf_cfg_get_key(dasher->dash_ctx, section, "CumulatedDuration");
			if (opt) {
				u64 val;
				sscanf(opt, LLU, &val);
				dur = ((Double) val) / dash_scale;
			}
			if (dur>max_dur) max_dur = dur;
		}
	}
	opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "CumulatedPastPeriodsDuration");
	if (opt) {
		sscanf(opt, "%lf", &past_period_dur);
		max_dur += past_period_dur;
	}

	if (!max_dur) return 0;
	gf_net_get_ntp(&ntp_sec, &frac);

	ms_elapsed = frac;
	ms_elapsed -= prev_frac;
	ms_elapsed /= 0xFFFFFFFF;
	ms_elapsed *= 1000;
	ms_elapsed += ((u64)(ntp_sec - prev_sec))*1000;

	if (ms_in_session) *ms_in_session = (u64) ( 1000*max_dur );

	/*check if we need to generate */
	if (ms_elapsed < (max_dur /* - safety_dur*/)*1000 ) {
		return (u32) (1000*max_dur - ms_elapsed);
	}
#endif
	return 0;
}

/*peform all file cleanup*/
static Bool gf_dasher_cleanup(GF_DASHSegmenter *dasher)
{
#ifdef FILTER_FIXME
	Double max_dur = 0;
	Double elapsed = 0;
	Double dash_duration = 1000*dasher->segment_duration / dasher->dash_scale;
	Double safety_dur = MAX(dasher->mpd->minimum_update_period, dash_duration);
	u32 i, ntp_sec, frac, prev_sec, prev_frac;
	const char *opt, *section;
	GF_Err e;

	if (!dasher->dash_mode) return GF_TRUE;

	opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "StoreParams");
	if (opt && !strcmp(opt, "yes")) return GF_TRUE;

	opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "GenerationNTP");
	if (!opt) return GF_TRUE;
	sscanf(opt, "%u:%u", &prev_sec, &prev_frac);

	/*compute cumulated duration*/
	for (i=0; i<gf_cfg_get_section_count(dasher->dash_ctx); i++) {
		Double dur = 0;
		section = gf_cfg_get_section_name(dasher->dash_ctx, i);
		if (section && !strncmp(section, "Representation_", 15)) {
			opt = gf_cfg_get_key(dasher->dash_ctx, section, "CumulatedDuration");
			if (opt) {
				u64 val;
				sscanf(opt, LLU, &val);
				dur = ((Double) val) / dasher->dash_scale;
			}
			if (dur>max_dur) max_dur = dur;
		}
	}
	if (!max_dur) return GF_TRUE;
	gf_net_get_ntp(&ntp_sec, &frac);

	if (dasher->dash_mode == GF_DASH_DYNAMIC_DEBUG) {
		elapsed = (u32)-1;
	} else {
		elapsed = ntp_sec;
		elapsed += (frac*1.0)/0xFFFFFFFF;
		elapsed -= prev_sec;
		elapsed -= (prev_frac*1.0)/0xFFFFFFFF;
		/*check if we need to generate */
		if (!dasher->dash_mode && elapsed < max_dur - safety_dur ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Asked to regenerate segments before expiration of the current segment list, please wait %g seconds - ignoring\n", max_dur + prev_sec - ntp_sec ));
			return GF_FALSE;
		}
		if (elapsed > max_dur) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Generating segments and MPD %g seconds too late\n", elapsed - (u32) max_dur));
		} else {
			/*generate as if max_dur has been reached*/
			elapsed = max_dur;
		}
	}

	/*cleanup old segments*/
	if ((s32) dasher->mpd->time_shift_buffer_depth >= 0) {
		for (i=0; i<gf_cfg_get_key_count(dasher->dash_ctx, "SegmentsStartTimes"); i++) {
			Double seg_time;
			u64 start, dur, scale;
			char szRepID[100];
			char szSecName[200];
			u32 j;
			const char *fileName = gf_cfg_get_key_name(dasher->dash_ctx, "SegmentsStartTimes", i);
			const char *opt = gf_cfg_get_key(dasher->dash_ctx, "SegmentsStartTimes", fileName);
			if (!fileName)
				break;

			sscanf(opt, ""LLU"-"LLU"-"LLU"@%s", &start, &dur, &scale, szRepID);
			seg_time = (Double) start;
			seg_time /= scale;

			if (dasher->ast_offset_ms > 0)
				seg_time += ((Double) dasher->ast_offset_ms) / 1000;


			seg_time += 2 * dash_duration + dasher->mpd->time_shift_buffer_depth;
			seg_time -= elapsed;
			if (seg_time >= 0)
				continue;

			if (! (dasher->dash_mode == GF_DASH_DYNAMIC_DEBUG) ) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Removing segment %s - %g sec too late\n", fileName, -seg_time - dash_duration));
			}

			e = gf_delete_file(fileName);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Could not remove file %s: %s\n", fileName, gf_error_to_string(e) ));
				break;
			}

			sprintf(szSecName, "URLs_%s", szRepID);

			/*remove URLs*/
			for (j=0; j<gf_cfg_get_key_count(dasher->dash_ctx, szSecName); j++) {
				const char *entry = gf_cfg_get_key_name(dasher->dash_ctx, szSecName, j);
				const char *name = gf_cfg_get_key(dasher->dash_ctx, szSecName, entry);
				if (strstr(name, fileName)) {
					gf_cfg_set_key(dasher->dash_ctx, szSecName, entry, NULL);
					break;
				}
			}

			/*adjust seg removed count - this is needed to adjust startNumber for SegmentTimeline case*/
			sprintf(szSecName, "Representation_%s", szRepID);
			j = 1;
			opt = gf_cfg_get_key(dasher->dash_ctx, szSecName, "NbSegmentsRemoved");
			if (opt) j += atoi(opt);
			sprintf(szRepID, "%d", j);
			gf_cfg_set_key(dasher->dash_ctx, szSecName, "NbSegmentsRemoved", szRepID);

			gf_cfg_set_key(dasher->dash_ctx, "SegmentsStartTimes", fileName, NULL);
			i--;
		}
	}
#endif
	return GF_TRUE;
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
	gf_free(dasher->m3u8_name);
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
GF_Err gf_dasher_set_test_mode(GF_DASHSegmenter *dasher, Bool forceTestMode){
	dasher->force_test_mode=forceTestMode;
	return GF_OK;
}

#define SET_LIST_STR(_field, _val)	\
	l1 = dasher->_field ? strlen(dasher->_field) : 0;\
	l2 = strlen(_val);\
	if (l1) dasher->_field = gf_realloc(dasher->_field, sizeof(char)*(l1+l2+2));\
	else dasher->_field = gf_realloc(dasher->_field, sizeof(char)*(l1+l2+1));\
	if (!dasher->_field) return GF_OUT_OF_MEM;\
	if (l1) strcat(dasher->_field, ",");\
	strcat(dasher->_field, _val);\


GF_EXPORT
GF_Err gf_dasher_set_location(GF_DASHSegmenter *dasher, const char *location)
{
	u32 l1, l2;
	if (!dasher) return GF_BAD_PARAM;

	if (!location) return GF_OK;
	SET_LIST_STR(locations, location)
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_add_base_url(GF_DASHSegmenter *dasher, const char *base_url)
{
	u32 l1, l2;
	if (!dasher) return GF_BAD_PARAM;

	if (!base_url) return GF_OK;
	SET_LIST_STR(base_urls, base_url)
	return GF_OK;
}

static void dasher_format_seg_name(GF_DASHSegmenter *dasher, const char *inName)
{
	char szName[GF_MAX_PATH];
	/*if output is not in the current working dir, concatenate output path to segment name*/
	szName[0] = 0;

	if (inName && gf_url_get_resource_path(dasher->mpd_name, szName)) {
		strcat(szName, inName);
		dasher->seg_rad_name = gf_strdup(szName);
	} else {
		if (inName) dasher->seg_rad_name = gf_strdup(inName);
	}
}

GF_EXPORT
GF_Err gf_dasher_enable_url_template(GF_DASHSegmenter *dasher, Bool enable, const char *default_template, const char *default_extension)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->use_url_template = enable;
	dasher->seg_ext = default_extension;
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
GF_Err gf_dasher_enable_sidx(GF_DASHSegmenter *dasher, Bool enable_sidx, u32 subsegs_per_sidx, Bool daisy_chain_sidx)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->enable_sidx = enable_sidx;
	dasher->subsegs_per_sidx = subsegs_per_sidx;
	dasher->daisy_chain_sidx = daisy_chain_sidx;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_dynamic_mode(GF_DASHSegmenter *dasher, GF_DashDynamicMode dash_mode, Double mpd_update_time, s32 time_shift_depth, Double mpd_live_duration)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->dash_mode = dash_mode;
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
GF_Err gf_dasher_configure_isobmf_default(GF_DASHSegmenter *dasher, Bool no_fragments_defaults, GF_DASHPSSHMode pssh_mode, Bool samplegroups_in_traf, Bool single_traf_per_moof, Bool tfdt_per_traf)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->no_fragments_defaults = no_fragments_defaults;
	dasher->pssh_mode = pssh_mode;
	dasher->samplegroups_in_traf = samplegroups_in_traf;
	dasher->single_traf_per_moof = single_traf_per_moof;
    dasher->tfdt_per_traf = tfdt_per_traf;
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
GF_Err gf_dasher_set_m3u8info(GF_DASHSegmenter *dasher, const char *m3u8_name)
{
       if (!dasher) return GF_BAD_PARAM;
       if (m3u8_name)dasher->m3u8_name = gf_strdup(m3u8_name);
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
GF_Err gf_dasher_add_input(GF_DASHSegmenter *dasher, const GF_DashSegmenterInput *input)
{
	if (!dasher) return GF_BAD_PARAM;

	if (!stricmp(input->file_name, "NULL") || !strcmp(input->file_name, "") || !input->file_name) {
		if (!strcmp(input->xlink, "")) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No input file specified and no xlink set - cannot dash\n"));
			return GF_BAD_PARAM;
		}
	}

	gf_list_add(dasher->inputs, (void *) input);
	return GF_OK;
}

static GF_Err gf_dasher_setup(GF_DASHSegmenter *dasher)
{
	GF_Err e;
	u32 i, count;
	u32 l1, l2;
	char *args=NULL, szArg[1024];

#define APPEND_ARG(_an_arg) \
		l1 = args ? strlen(args) : 0; \
		l2 = strlen(_an_arg);\
		if (l1) args = gf_realloc(args, sizeof(char)*(l1+l2+2));\
		else args = gf_realloc(args, sizeof(char)*(l1+l2+1));\
		if (!args) return GF_OUT_OF_MEM;\
		if (l1) strcat(args, ":"); \
		strcat(args, _an_arg); \

	if (!dasher->mpd_name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Missing MPD name\n"));
		return GF_OUT_OF_MEM;
	}

	dasher->fsess = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, NULL, GF_FALSE, GF_FALSE, NULL);
	if (!dasher->fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}

	sprintf(szArg, "dur=%g", dasher->segment_duration);
	APPEND_ARG(szArg)
	if (dasher->single_segment) { APPEND_ARG("sseg") }
	if (dasher->single_file) { APPEND_ARG("sfile") }
	if (dasher->use_url_template) { APPEND_ARG("tpl") }
	if (dasher->use_segment_timeline) { APPEND_ARG("stl") }
	if (dasher->dash_mode) {APPEND_ARG((dasher->dash_mode == GF_DASH_DYNAMIC_LAST) ? "dynlast" : "dynamic") }
	if (dasher->disable_segment_alignment) { APPEND_ARG("!align") }
	if (dasher->enable_mix_codecs) { APPEND_ARG("mix_codecs") }
	if (dasher->insert_utc) { APPEND_ARG("ntp=yes") }
	if (dasher->enable_sar_mix) { APPEND_ARG("no_sar") }
	if (dasher->force_test_mode) { APPEND_ARG("for_test") }
	//forcep not mapped
	switch (dasher->bitstream_switching_mode) {
	case GF_DASH_BSMODE_DEFAULT:
		break;
	case GF_DASH_BSMODE_NONE:
		APPEND_ARG("bs_switch=off")
		break;
	case GF_DASH_BSMODE_INBAND:
		APPEND_ARG("bs_switch=inband")
		break;
	case GF_DASH_BSMODE_MERGED:
		APPEND_ARG("bs_switch=on")
		break;
	case GF_DASH_BSMODE_MULTIPLE_ENTRIES:
		APPEND_ARG("bs_switch=multi")
		break;
	case GF_DASH_BSMODE_SINGLE:
		APPEND_ARG("bs_switch=force")
		break;
	}
	//avcp, hvcp, aacp not mapped
	if (dasher->seg_rad_name) { sprintf(szArg, "template=%s", dasher->seg_rad_name); APPEND_ARG(szArg) }
	if (dasher->seg_ext) { sprintf(szArg, "ext=%s", dasher->seg_ext); APPEND_ARG(szArg) }
	if (dasher->ast_offset_ms) { sprintf(szArg, "asto=%d", dasher->ast_offset_ms); APPEND_ARG(szArg) }
	switch (dasher->profile) {
	case GF_DASH_PROFILE_AUTO:
		break;
	case GF_DASH_PROFILE_LIVE:
		APPEND_ARG("profile=live")
		break;
 	case GF_DASH_PROFILE_ONDEMAND:
		APPEND_ARG("profile=onDemand")
		break;
	case GF_DASH_PROFILE_MAIN:
		APPEND_ARG("profile=main")
		break;
	case GF_DASH_PROFILE_FULL:
		APPEND_ARG("profile=full")
		if (!dasher->segments_start_with_rap) { APPEND_ARG("!sap") }
		break;
	case GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE:
		APPEND_ARG("profile=hbbtv1.5.live")
		break;
	case GF_DASH_PROFILE_AVC264_LIVE:
		APPEND_ARG("profile=dashavc264.live")
		break;
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		APPEND_ARG("profile=dashavc264.onDemand")
		break;
	}
	if (dasher->dash_profile_extension) { sprintf(szArg, "profX=%s", dasher->dash_profile_extension); APPEND_ARG(szArg) }
	if (dasher->cp_location_mode==GF_DASH_CPMODE_REPRESENTATION) { APPEND_ARG("cp=rep") }
	else if (dasher->cp_location_mode==GF_DASH_CPMODE_BOTH) { APPEND_ARG("cp=both") }

	if (dasher->min_buffer_time) { sprintf(szArg, "buf=%d", dasher->min_buffer_time); APPEND_ARG(szArg) }
	if (dasher->dash_scale) { sprintf(szArg, "timescale=%d", dasher->dash_scale); APPEND_ARG(szArg) }
	if (!dasher->check_duration) { APPEND_ARG("!check_dur") }
	//skip_seg not exposed

	if (dasher->title) { sprintf(szArg, "title=%s", dasher->title); APPEND_ARG(szArg) }
	if (dasher->sourceInfo) { sprintf(szArg, "source=%s", dasher->sourceInfo); APPEND_ARG(szArg) }
	if (dasher->moreInfoURL) { sprintf(szArg, "info=%s", dasher->moreInfoURL); APPEND_ARG(szArg) }
	if (dasher->copyright) { sprintf(szArg, "cprt=%s", dasher->copyright); APPEND_ARG(szArg) }
	if (dasher->lang) { sprintf(szArg, "lang=%s", dasher->lang); APPEND_ARG(szArg) }
	if (dasher->locations) { sprintf(szArg, "location=%s", dasher->locations); APPEND_ARG(szArg) }
	if (dasher->base_urls) { sprintf(szArg, "base=%s", dasher->base_urls); APPEND_ARG(szArg) }

	if (dasher->dash_mode >= GF_DASH_DYNAMIC) {
		if (dasher->time_shift_depth<0) { APPEND_ARG("tsb=-1") }
		else { sprintf(szArg, "tsb=%g", ((Double)dasher->time_shift_depth)/1000); APPEND_ARG(szArg)}

		if (dasher->utc_start_date) { sprintf(szArg, "ast=%s", dasher->utc_start_date); APPEND_ARG(szArg) }
		if (dasher->mpd_update_time) { sprintf(szArg, "refresh=%g", dasher->mpd_update_time); APPEND_ARG(szArg) }
		else { sprintf(szArg, "refresh=-%g", dasher->mpd_live_duration); APPEND_ARG(szArg) }
	}
	if (dasher->sub_duration) { sprintf(szArg, "subdur=%g", dasher->sub_duration); APPEND_ARG(szArg)}
	if (dasher->dash_state) { sprintf(szArg, "state=%s", dasher->dash_state); APPEND_ARG(szArg)}
	if (! dasher->disable_loop && dasher->dash_state) { APPEND_ARG("loop")}
	//split not yet exposed

	//append ISOBMFF options
	if (dasher->fragment_duration) { sprintf(szArg, "cdur=%g", dasher->fragment_duration); APPEND_ARG(szArg)}
	if (dasher->segment_marker_4cc) { sprintf(szArg, "m4cc=%s", gf_4cc_to_str(dasher->segment_marker_4cc) ); APPEND_ARG(szArg)}
	if (dasher->daisy_chain_sidx) { APPEND_ARG("chain_sidx")}
	if (dasher->initial_moof_sn) { sprintf(szArg, "moof_sn=%d", dasher->initial_moof_sn ); APPEND_ARG(szArg)}
	if (dasher->initial_tfdt) { sprintf(szArg, "tfdt="LLU"", dasher->initial_tfdt ); APPEND_ARG(szArg)}
	if (dasher->no_fragments_defaults) {APPEND_ARG("no_def")}
	if (dasher->single_traf_per_moof) {APPEND_ARG("straf")}
	if (dasher->single_trun_per_traf) {APPEND_ARG("strun")}
	switch (dasher->pssh_mode) {
	case GF_DASH_PSSH_MOOV:
	case GF_DASH_PSSH_MOOV_MPD:
		break;
	case GF_DASH_PSSH_MOOF:
	case GF_DASH_PSSH_MOOF_MPD:
		APPEND_ARG("pssh=moof")
		break;
	case GF_DASH_PSSH_MPD:
		APPEND_ARG("pssh=none")
		break;
	}
	if (dasher->samplegroups_in_traf) {APPEND_ARG("sgpd_traf")}
	if (dasher->enable_sidx) { sprintf(szArg, "subs_sidx=%d", dasher->subsegs_per_sidx ); APPEND_ARG(szArg)}

	if (dasher->fragments_start_with_rap) { APPEND_ARG("sfrag")}
	if (dasher->tmpdir) { sprintf(szArg, "tmpd=%s", dasher->tmpdir ); APPEND_ARG(szArg)}

	dasher->filter = gf_fs_load_destination(dasher->fsess, dasher->mpd_name, args, NULL, &e);

	if (args) gf_free(args);

	if (!dasher->filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load DASH filer\n"));
		return e;
	}

#define APPEND_STR(_an_arg) \
		l1 = args ? strlen(args) : 0; \
		l2 = strlen(_an_arg);\
		args = gf_realloc(args, sizeof(char)*(l1+l2+2));\
		if (!args) return GF_OUT_OF_MEM;\
		strcat(args, ","); \
		strcat(args, _an_arg); \

	//and setup sources
	count = gf_list_count(dasher->inputs);
	for (i=0; i<count; i++) {
		u32 j;
		GF_Filter *src = NULL;
		GF_Filter *rt = NULL;
		const char *url = "null";
		GF_DashSegmenterInput *di = gf_list_get(dasher->inputs, i);

		if (dasher->real_time) {
			rt = gf_fs_load_filter(dasher->fsess, "reframer:rt=sync");
		}
		if (di->file_name && strlen(di->file_name)) url = di->file_name;
		if (!stricmp(url, "null")) url = NULL;

		args = NULL;

		//set all args
		if (di->representationID)  { sprintf(szArg, "#Representation=%s", di->representationID ); APPEND_ARG(szArg)}
		if (di->periodID)  { sprintf(szArg, "#Period=%s", di->periodID ); APPEND_ARG(szArg)}
		//period start not exposed
		if (di->period_duration) {
			if (!url) { sprintf(szArg, "#PDur=%g", di->period_duration ); APPEND_ARG(szArg)}
			else { sprintf(szArg, "#DashDur=%g", di->period_duration ); APPEND_ARG(szArg)}
		}
		if (url && di->media_duration) { sprintf(szArg, "#CDur=%g", di->media_duration ); APPEND_ARG(szArg)}

		if (di->xlink)  { sprintf(szArg, "#xlink=%s", di->xlink ); APPEND_ARG(szArg)}
		if (di->bandwidth)  { sprintf(szArg, "#Bitrate=%d", di->bandwidth ); APPEND_ARG(szArg)}

		//di->no_cache is deprecated, always on

		for (j=0;j<di->nb_baseURL; j++) {
			if (!j) { sprintf(szArg, "#BUrl=%s", di->baseURL[j] ); APPEND_ARG(szArg)}
			else { APPEND_STR(di->baseURL[j]) }
		}
		for (j=0;j<di->nb_roles; j++) {
			if (!j) { sprintf(szArg, "#Role=%s", di->roles[j] ); APPEND_ARG(szArg)}
			else { APPEND_STR(di->roles[j]) }
		}

		for (j=0;j<di->nb_rep_descs; j++) {
			if (!j) { sprintf(szArg, "#RDesc=%s", di->rep_descs[j] ); APPEND_ARG(szArg)}
			else { APPEND_STR(di->rep_descs[j]) }
		}

		for (j=0;j<di->nb_p_descs; j++) {
			if (!j) { sprintf(szArg, "#PDesc=%s", di->p_descs[j] ); APPEND_ARG(szArg)}
			else { APPEND_STR(di->p_descs[j]) }
		}

		for (j=0;j<di->nb_as_descs; j++) {
			if (!j) { sprintf(szArg, "#ASDesc=%s", di->as_descs[j] ); APPEND_ARG(szArg)}
			else { APPEND_STR(di->as_descs[j]) }
		}

		for (j=0;j<di->nb_as_c_descs; j++) {
			if (!j) { sprintf(szArg, "#ASCDesc=%s", di->as_c_descs[j] ); APPEND_ARG(szArg)}
			else { APPEND_STR(di->as_c_descs[j]) }
		}

		//TODO: ASID, start number, template

		if (!url) url = "null";
		src = gf_fs_load_source(dasher->fsess, url, args, NULL, &e);
		if (args) gf_free(args);

		if (!src) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load source filer for %s\n", di->file_name));
			return e;
		}

		if (rt) {
			gf_filter_set_source(rt, src, NULL);
		}
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_process(GF_DASHSegmenter *dasher)
{
	GF_Err e;
	if (!dasher->fsess) {
		e = gf_dasher_setup(dasher);
		if (e) return e;
	}

	/*update dash context*/
	if (dasher->dash_state) {
		Bool regenerate;

		/*peform all file cleanup*/
		regenerate = gf_dasher_cleanup(dasher);
		if (!regenerate) return GF_OK;
	}

	e = gf_fs_run(dasher->fsess);
	if (e<0) return e;
	return GF_OK;
}

