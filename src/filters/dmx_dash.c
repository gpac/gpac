/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / DASH/HLS demux filter
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

#ifndef GPAC_DISABLE_DASH_CLIENT

#include <gpac/dash.h>

#ifdef GPAC_HAS_QJS
#include "../quickjs/quickjs.h"
#include "../scenegraph/qjs_common.h"
#endif

#define DASHIN_MIMES "application/dash+xml|video/vnd.3gpp.mpd|audio/vnd.3gpp.mpd|video/vnd.mpeg.dash.mpd|audio/vnd.mpeg.dash.mpd|audio/mpegurl|video/mpegurl|application/vnd.ms-sstr+xml|application/x-mpegURL|application/vnd.apple.mpegURL"

#define DASHIN_FILE_EXT "mpd|m3u8|3gm|ism"

enum
{
	DFWD_OFF = 0,
	DFWD_FILE,
	//all modes below forward frames, not files
	DFWD_SBOUND,
	DFWD_SBOUND_MANIFEST,
};


enum
{
	BMIN_NO = 0,
	BMIN_AUTO,
	BMIN_MPD,
};

enum {
	FLAG_PERIOD_SWITCH = 1,
	FLAG_TIME_OVERFLOW = 1<<1,
	FLAG_FIRST_IN_SEG = 1<<2,
};

typedef struct
{
	//opts
	s32 shift_utc, spd, route_shift;
	u32 max_buffer, tiles_rate, segstore, delay40X, exp_threshold, switch_count, bwcheck;
	s32 auto_switch;
	s32 init_timeshift;
	Bool server_utc, screen_res, aggressive, speedadapt, fmodefwd, skip_lqt, llhls_merge, filemode, chain_mode, asloop;
	u32 forward;
	GF_PropUIntList debug_as;
	GF_DASHInitialSelectionMode start_with;
	GF_DASHTileAdaptationMode tile_mode;
	char *algo;
	Bool max_res, abort;
	u32 use_bmin;
	char *query;
	Bool noxlink, split_as, noseek, groupsel;
	u32 lowlat;

	GF_FilterPid *mpd_pid;
	GF_Filter *filter;

	GF_FilterPid *output_mpd_pid;

	GF_DashClient *dash;
	//http io for manifest
	GF_DASHFileIO dash_io;
	GF_DownloadManager *dm;
	
	Bool reuse_download_session;

	Bool initial_setup_done;
	Bool in_error;
	u32 nb_playing;

	/*max width & height in all active representations*/
	u32 width, height;

	Double seek_request;
	Double media_start_range;

	Bool mpd_open;
	Bool initial_play;
	Bool check_eos;

	char *frag_url;

	char *manifest_payload;
	GF_List *hls_variants, *hls_variants_names;

	GF_Fraction64 time_discontinuity;
	Bool compute_min_dts;
	u64 timedisc_next_min_ts;

	Bool is_dash;
	Bool manifest_stop_sent;
#ifdef GPAC_HAS_QJS
	JSContext *js_ctx;
	Bool owns_context;
	JSValue js_obj, rate_fun, download_fun, new_group_fun, period_reset_fun;
#endif

	void *rt_udta;
	void (*on_period_reset)(void *udta, u32 reset_type);
	void (*on_new_group)(void *udta, u32 group_idx, void *dash);
	s32 (*on_rate_adaptation)(void *udta, u32 group_idx, u32 base_group_idx, Bool force_low_complex, void *stats);
	s32 (*on_download_monitor)(void *udta, u32 group_idx, void *stats);
} GF_DASHDmxCtx;

typedef struct
{
	GF_DASHDmxCtx *ctx;
	GF_Filter *seg_filter_src;

	u32 idx;
	Bool init_switch_seg_sent;
	Bool segment_sent, in_is_cryptfile;

	u32 nb_eos, nb_pids;
	Bool stats_uploaded;
	Bool wait_for_pck;
	Bool eos_detected;
	u32 next_dependent_rep_idx, current_dependent_rep_idx;
	u64 utc_map;
	
	GF_DownloadSession *sess;
	Bool is_timestamp_based, pto_setup;
	Bool prev_is_init_segment;
	//media timescale for which the pto, max_cts_in_period and timedisc_ts_offset were computed
	u32 timescale;
	s64 pto;
	u64 max_cts_in_period, timedisc_ts_offset;
	bin128 key_IV;

	Bool seg_was_not_ready;
	Bool in_error;
	Bool is_playing;
	Bool force_seg_switch;
	u32 nb_group_deps, current_group_dep;
	u32 last_bw_check;
	u64 us_at_seg_start;
	Bool signal_seg_name;
	Bool init_ok;
	Bool notify_quality_change;

	u32 seg_discard_state;
	u32 nb_pending;

	char *template;

	const char *hls_key_uri;
	bin128 hls_key_IV;

	char *current_url;
	Bool url_changed;
} GF_DASHGroup;

static void dashdmx_notify_group_quality(GF_DASHDmxCtx *ctx, GF_DASHGroup *group);

static void dashdmx_set_string_list_prop(GF_FilterPacket *ref, u32 prop_name, GF_List **str_list)
{
	u32 i, count;
	GF_PropertyValue v;
	GF_List *list = *str_list;
	if (!list) return;

	v.type = GF_PROP_STRING_LIST;
	v.value.string_list.nb_items = count = gf_list_count(list);
	v.value.string_list.vals = gf_malloc(sizeof(char *) * count);
	for (i=0; i<count;i++) {
		v.value.string_list.vals[i] = gf_list_pop_front(list);
	}
	gf_list_del(list);
	*str_list = NULL;
	gf_filter_pck_set_property(ref, prop_name, &v);
}


static void dashdmx_forward_packet(GF_DASHDmxCtx *ctx, GF_FilterPacket *in_pck, GF_FilterPid *in_pid, GF_FilterPid *out_pid, GF_DASHGroup *group)
{
	GF_FilterPacket *dst_pck;
	Bool do_map_time = GF_FALSE;
	Bool seek_flag = 0;
	u64 cts, dts;

	if (ctx->forward) {
		Bool is_filemode;
		Bool is_start = group->signal_seg_name;
		GF_FilterPacket *ref;

		if (ctx->forward==DFWD_FILE) {
			Bool is_end = GF_FALSE;
			if (ctx->fmodefwd) {
				ref = gf_filter_pck_new_ref(out_pid, 0, 0, in_pck);
			} else {
				u8 *output;
				ref = gf_filter_pck_new_copy(out_pid, in_pck, &output);
			}
			if (!ref) return;

			group->signal_seg_name = 0;
			gf_filter_pck_get_framing(in_pck, NULL, &is_end);
			gf_filter_pid_drop_packet(in_pid);
			if (gf_filter_pid_is_eos(in_pid))
				is_end = GF_TRUE;

			gf_filter_pck_set_framing(ref, is_start, is_end);
			is_filemode = GF_TRUE;
		} else {
			const GF_PropertyValue *p;
			ref = gf_filter_pck_new_ref(out_pid, 0, 0, in_pck);
			if (!ref) return;

			gf_filter_pck_merge_properties(in_pck, ref);
			p = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_CUE_START);
			if (p && p->value.boolean) {
				is_start = GF_TRUE;
			}
			group->pto_setup = GF_TRUE;
			gf_filter_pid_drop_packet(in_pid);
			is_filemode = GF_FALSE;
		}

		if (is_start) {
			if (group->prev_is_init_segment) {
				const char *init_segment = NULL;
				gf_dash_group_next_seg_info(ctx->dash, group->idx, 0, NULL, NULL, NULL, NULL, &init_segment);
				if (init_segment) {
					gf_filter_pck_set_property(ref, GF_PROP_PCK_FILENAME, &PROP_STRING(init_segment) );
					gf_filter_pck_set_property(ref, GF_PROP_PCK_INIT, &PROP_BOOL(GF_TRUE) );
				}
			} else {
				GF_Fraction64 seg_time;
				const char *seg_name = NULL;
				u32 seg_number, seg_dur;
				gf_dash_group_next_seg_info(ctx->dash, group->idx, group->current_dependent_rep_idx, &seg_name, &seg_number, &seg_time, &seg_dur, NULL);
				if (seg_name) {
					gf_filter_pck_set_property(ref, GF_PROP_PCK_FILENAME, &PROP_STRING(seg_name) );
					gf_filter_pck_set_property(ref, GF_PROP_PCK_FILENUM, &PROP_UINT(seg_number) );
					if (group->url_changed && group->current_url) {
						gf_filter_pck_set_property(ref, GF_PROP_PCK_FRAG_RANGE, NULL);
						gf_filter_pck_set_property(ref, GF_PROP_PID_URL, &PROP_STRING(group->current_url));
						group->url_changed = GF_FALSE;
					}

					if (is_filemode) {
						u64 ts;
						gf_filter_pck_set_duration(ref, seg_dur);
						//hack to avoid using a property, we set the carousel version on the packet to signal the duration applies to the entire segment, not the packet
						gf_filter_pck_set_carousel_version(ref, 1);
						if (seg_time.den) {
							ts = gf_timestamp_rescale(seg_time.num, seg_time.den, 1000);
						} else {
							ts = GF_FILTER_NO_TS;
						}
						gf_filter_pck_set_cts(ref, ts);
					}
				}
			}

			if (ctx->manifest_payload) {
				gf_filter_pck_set_property(ref, GF_PROP_PCK_DASH_MANIFEST, &PROP_STRING_NO_COPY(ctx->manifest_payload));
				ctx->manifest_payload = NULL;
			}
			dashdmx_set_string_list_prop(ref, GF_PROP_PCK_HLS_VARIANT, &ctx->hls_variants);
			dashdmx_set_string_list_prop(ref, GF_PROP_PCK_HLS_VARIANT_NAME, &ctx->hls_variants_names);
		}

		u32 flags = gf_filter_pid_get_udta_flags(out_pid);
		if (flags & FLAG_PERIOD_SWITCH) {
			gf_filter_pid_set_udta_flags(out_pid, flags & ~FLAG_PERIOD_SWITCH);
			gf_filter_pck_set_property(ref, GF_PROP_PID_DASH_PERIOD_START, &PROP_LONGUINT(0) );
		}
		gf_filter_pck_send(ref);
		return;
	}

	if (!ctx->is_dash) {
		GF_FilterPacket *dst_pck = gf_filter_pck_new_ref(out_pid, 0, 0, in_pck);
		if (!dst_pck) return;
		gf_filter_pck_merge_properties(in_pck, dst_pck);

		u32 flags = gf_filter_pid_get_udta_flags(out_pid);
		if (! (flags & FLAG_FIRST_IN_SEG) ) {
			if (!group->pto_setup) {
				gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_MEDIA_TIME, &PROP_DOUBLE(ctx->media_start_range) );
				group->pto_setup = GF_TRUE;
			}

			if (group->utc_map) {
				gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_UTC_TIME, & PROP_LONGUINT(group->utc_map) );
			}
			flags |= FLAG_FIRST_IN_SEG;
			gf_filter_pid_set_udta_flags(out_pid, flags);

		}
		gf_filter_pck_send(dst_pck);

		gf_filter_pid_drop_packet(in_pid);
		return;
	}

	//filter any packet outside the current period
	dts = gf_filter_pck_get_dts(in_pck);
	cts = gf_filter_pck_get_cts(in_pck);
	seek_flag = gf_filter_pck_get_seek_flag(in_pck);

	//if sync is based on timestamps do not adjust the timestamps back
	if (! group->is_timestamp_based) {
		u64 scale_max_cts, scale_timesdisc_offset;
		s64 scale_pto;
		u32 ts = gf_filter_pck_get_timescale(in_pck);

		if (!group->pto_setup) {
			Double scale;
			s64 start, dur;
			u64 pto;
			u32 mpd_timescale;
			gf_dash_group_get_presentation_time_offset(ctx->dash, group->idx, &pto, &mpd_timescale);
			group->pto_setup = 1;
			do_map_time = GF_TRUE;

			//we are forwarding segment boundaries and potentially rebuilding manifest based on the source time
			//do not adjust timestamps
			if (ctx->forward) {
				pto = 0;
			} else {
				if (mpd_timescale && (mpd_timescale != ts)) {
					pto = gf_timestamp_rescale(pto, mpd_timescale, ts);
				}
			}

			group->pto = (s64) pto;
			group->timescale = ts;
			scale = ts;
			scale /= 1000;


			dur = (u64) (scale * gf_dash_get_period_duration(ctx->dash));
			if (dur) {
				group->max_cts_in_period = group->pto + dur;
			} else {
				group->max_cts_in_period = 0;
			}

			//no max cts in HLS, and don't filter in forward modes
			if (gf_dash_is_m3u8(ctx->dash) || ctx->forward) {
				group->max_cts_in_period = 0;
			} else if (!group->pto && group->max_cts_in_period && !gf_dash_is_dynamic_mpd(ctx->dash) ) {
				u32 seg_number=0;
				if (((s64) dts > group->pto)
					&& (gf_dash_group_next_seg_info(ctx->dash, group->idx, 0, NULL, &seg_number, NULL, NULL, NULL) == GF_OK)
					&& (seg_number<=1)
				) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] First packet decode timestamp "LLU" but PTO is 0 - broken period timing, will not clamp\n", dts));
					group->max_cts_in_period = 0;
				}
			}


			if (ctx->time_discontinuity.den) {
				group->pto += gf_timestamp_rescale(ctx->timedisc_next_min_ts, ctx->time_discontinuity.den, ts);
				group->timedisc_ts_offset = gf_timestamp_rescale(ctx->time_discontinuity.num, ctx->time_discontinuity.den, ts);
			}

			start = (u64) (scale * gf_dash_get_period_start(ctx->dash));
			group->pto -= start;
		}

		if (ts == group->timescale) {
			scale_max_cts = group->max_cts_in_period;
			scale_pto = group->pto;
			scale_timesdisc_offset = group->timedisc_ts_offset;
		} else {
			scale_max_cts = gf_timestamp_rescale(group->max_cts_in_period, group->timescale, ts);
			scale_pto = gf_timestamp_rescale_signed(group->pto, group->timescale, ts);
			scale_timesdisc_offset = gf_timestamp_rescale(group->timedisc_ts_offset, group->timescale, ts);
		}


		if (scale_max_cts && (cts > scale_max_cts)) {
			u64 adj_cts = cts;
			s64 diff;
			const GF_PropertyValue *p = gf_filter_pid_get_property(in_pid, GF_PROP_PID_DELAY);
			if (p) adj_cts += p->value.longsint;
			diff = (s64)adj_cts;
			diff -= scale_max_cts;
			//move back to ms and then back to timescale to checko rounding issues
			if (diff>0) {
				diff *= 1000;
				diff /= ts;
				if (diff<=1) diff=0;
				
				diff *= ts;
				diff /= 1000;
			}

			if (diff>0) {
				u32 flags;
				u64 _dts = (dts==GF_FILTER_NO_TS) ? dts : adj_cts;

				//if DTS larger than max TS, drop packet
				if ( (s64) _dts > (s64) scale_max_cts) {
					flags = gf_filter_pid_get_udta_flags(out_pid);
					if (!(flags & FLAG_TIME_OVERFLOW)) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Packet decode timestamp "LLU" larger than max CTS in period "LLU" - dropping all further packets\n", adj_cts, scale_max_cts));
						gf_filter_pid_set_udta_flags(out_pid, flags | FLAG_TIME_OVERFLOW);
					}
					gf_filter_pid_drop_packet(in_pid);
					return;
				}
				//otherwise, packet may be required for decoding future frames, mark as drop
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] Packet timestamp "LLU" larger than max CTS in period "LLU" - forcing seek flag\n", adj_cts, scale_max_cts));

				seek_flag = 1;
			}
		}

		//remap timestamps to our timeline
		if (dts != GF_FILTER_NO_TS) {
			if ((s64) dts >= scale_pto)
				dts -= scale_pto;
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Packet DTS "LLU" less than PTO "LLU" - forcing DTS to 0\n", dts, scale_pto));
				dts = 0;
				seek_flag = 1;
			}
		}
		if (cts!=GF_FILTER_NO_TS) {
			if ((s64) cts >= scale_pto)
				cts -= scale_pto;
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Packet CTS "LLU" less than PTO "LLU" - forcing CTS to 0\n", cts, scale_pto));
				cts = 0;
				seek_flag = 1;
			}
		}
		dts += scale_timesdisc_offset;
		cts += scale_timesdisc_offset;
	} else if (!group->pto_setup) {
		do_map_time = GF_TRUE;
		group->pto_setup = GF_TRUE;
	}

	dst_pck = gf_filter_pck_new_ref(out_pid, 0, 0, in_pck);
	if (!dst_pck) return;
	
	//this will copy over clock info for PCR in TS
	gf_filter_pck_merge_properties(in_pck, dst_pck);
	gf_filter_pck_set_dts(dst_pck, dts);
	gf_filter_pck_set_cts(dst_pck, cts);
	gf_filter_pck_set_seek_flag(dst_pck, seek_flag);


	u32 flags = gf_filter_pid_get_udta_flags(out_pid);
	if (! (flags & FLAG_FIRST_IN_SEG) ) {
		flags |= FLAG_FIRST_IN_SEG;
		gf_filter_pid_set_udta_flags(out_pid, flags);
		if (do_map_time)
			gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_MEDIA_TIME, &PROP_DOUBLE(ctx->media_start_range) );

		if (group->utc_map)
			gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_UTC_TIME, & PROP_LONGUINT(group->utc_map) );
	}

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(in_pid);
}


static Bool dashdmx_on_filter_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	GF_DASHGroup *group = (GF_DASHGroup *)udta;
	if (!udta) return GF_FALSE;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] group %d download setup error %s\n", group->idx, gf_error_to_string(err) ));

	gf_dash_set_group_download_state(group->ctx->dash, group->idx, group->current_dependent_rep_idx, err);
	if (err) {
		Bool group_done = gf_dash_get_group_done(group->ctx->dash, group->idx);
		group->stats_uploaded = GF_TRUE;
		group->segment_sent = GF_FALSE;
		gf_filter_post_process_task(group->ctx->filter);
		if (group_done) {
			group->eos_detected = GF_TRUE;
		} else {
			group->in_error = GF_TRUE;

			if (group->nb_group_deps) {
				if (group->current_group_dep) group->current_group_dep--;
			} else if (group->next_dependent_rep_idx) {
				group->next_dependent_rep_idx--;
			}

			//failure at init, abort group
			if (!group->init_ok)
				group->seg_filter_src = NULL;
		}
	}
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_CRYPTO
void gf_cryptfin_set_kms(GF_Filter *f, const char *key_url, bin128 key_IV);
#endif

/*locates input service (demuxer) based on mime type or segment name*/
static GF_Err dashdmx_load_source(GF_DASHDmxCtx *ctx, u32 group_index, const char *mime, const char *init_segment_name, u64 start_range, u64 end_range)
{
	GF_DASHGroup *group;
	GF_Err e;
	u32 url_type=0;
	Bool has_sep = GF_FALSE;
	u32 crypto_type;
	bin128 key_IV;
	char szSep[2];
	const char *key_uri;
	char *sURL = NULL;
	const char *base_url;
	if (!init_segment_name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] group %d Error locating input filter: no segment URL, mime type %s\n", group_index, mime));
		return GF_FILTER_NOT_FOUND;
	}

	GF_SAFEALLOC(group, GF_DASHGroup);
	if (!group) return GF_OUT_OF_MEM;
	group->ctx = ctx;
	group->idx = group_index;
	gf_dash_set_group_udta(ctx->dash, group_index, group);

	base_url = gf_dash_get_url(ctx->dash);
	if (!strnicmp(base_url, "http://", 7)) url_type=1;
	else if (!strnicmp(base_url, "https://", 7)) url_type=2;
	else url_type=0;

	key_uri = gf_dash_group_get_segment_init_keys(ctx->dash, group_index, &crypto_type, &key_IV);
	if (crypto_type==1) {
		gf_dynstrcat(&sURL, "gcryp://", NULL);
		group->in_is_cryptfile = GF_TRUE;
	}

	if (!strncmp(init_segment_name, "isobmff://", 10)) {
		if (url_type==1)
			gf_dynstrcat(&sURL, "http://", NULL);
		else if (url_type==2)
			gf_dynstrcat(&sURL, "https://", NULL);
		else
			gf_dynstrcat(&sURL, "file://", NULL);
	}
	gf_dynstrcat(&sURL, init_segment_name, NULL);

	szSep[0] = gf_filter_get_sep(ctx->filter, GF_FS_SEP_ARGS);
	szSep[1] = 0;

	//not from file system, set cache option
	if (url_type) {
		char szOpt[100];
		char sep_name = gf_filter_get_sep(ctx->filter, GF_FS_SEP_NAME);
		if (!ctx->segstore) {
			if (!has_sep) { gf_dynstrcat(&sURL, "gpac", szSep); has_sep = GF_TRUE; }
			//if operating in mem mode and we load a file decryptor, only store in mem cache the first seg, and no cache for segments
			sprintf(szOpt, "cache%c%s", sep_name, (crypto_type==1) ? "none_keep" : "mem_keep");
			gf_dynstrcat(&sURL, szOpt, szSep);
		} else {
			sprintf(szOpt, "cache%c%s", sep_name, (ctx->segstore==2) ? "keep" : "disk");
			if (!has_sep) { gf_dynstrcat(&sURL, "gpac", szSep); has_sep = GF_TRUE; }
			gf_dynstrcat(&sURL, szOpt, szSep);
		}
	}

	if (start_range || end_range) {
		char szRange[500];
		if (!has_sep) { gf_dynstrcat(&sURL, "gpac", szSep); has_sep = GF_TRUE; }
		snprintf(szRange, 500, "range="LLU"-"LLU, start_range, end_range);
		gf_dynstrcat(&sURL, szRange, szSep);
	}
	if (ctx->forward>DFWD_FILE) {
		if (!has_sep) { gf_dynstrcat(&sURL, "gpac", szSep); }
		gf_dynstrcat(&sURL, "sigfrag", szSep);
	}

	group->seg_filter_src = gf_filter_connect_source(ctx->filter, sURL, NULL, GF_FALSE, &e);
	if (!group->seg_filter_src) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] group %d error locating plugin for segment - mime type %s name %s: %s\n", group_index, mime, sURL, gf_error_to_string(e) ));
		gf_free(sURL);
		gf_free(group);
		gf_dash_set_group_udta(ctx->dash, group_index, NULL);
		return e;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] setting up group %d from %s\n", group->idx, init_segment_name));
	group->signal_seg_name = (ctx->forward==DFWD_FILE) ? GF_TRUE : GF_FALSE;
	gf_filter_set_source(ctx->filter, group->seg_filter_src, NULL);

	gf_filter_set_setup_failure_callback(ctx->filter, group->seg_filter_src, dashdmx_on_filter_setup_error, group);

	if (gf_dash_group_init_segment_is_media(ctx->dash, group_index))
		group->prev_is_init_segment = GF_FALSE;
	else {
		group->prev_is_init_segment = GF_TRUE;
		//consider init is always in clear if AES-128, might need further checks
		if (crypto_type==1) {
			key_uri = NULL;
		}
	}

	//if HLS AES-CBC, set key BEFORE discarding segment URL (if TS, discarding the segment will discard the key uri)
	if (key_uri) {
		if (crypto_type==1) {
#ifndef GPAC_DISABLE_CRYPTO
			gf_cryptfin_set_kms(group->seg_filter_src, key_uri, key_IV);
#else
			gf_free(sURL);
			return GF_NOT_SUPPORTED;
#endif

		} else {
			group->hls_key_uri = key_uri;
			memcpy(group->hls_key_IV, key_IV, sizeof(bin128));
		}
	}

	gf_dash_group_discard_segment(ctx->dash, group->idx);
	group->nb_group_deps = gf_dash_group_get_num_groups_depending_on(ctx->dash, group_index);
	group->current_group_dep = 0;
	gf_free(sURL);

	return GF_OK;
}

void dashdmx_io_delete_cache_file(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *cache_url)
{
	if (!session) {
		return;
	}
	gf_dm_delete_cached_file_entry_session((GF_DownloadSession *)session, cache_url);
}

void gf_dm_sess_force_blocking(GF_DownloadSession *sess);

GF_DASHFileIOSession dashdmx_io_create(GF_DASHFileIO *dashio, Bool persistent, const char *url, s32 group_idx)
{
	GF_DownloadSession *sess;
	GF_Err e;
	u32 flags = GF_NETIO_SESSION_NOT_THREADED;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;

	//we work in non-threaded mode, only MPD fetcher is allowed
	if (group_idx>=0)
		return NULL;

	//crude hack when using gpac downloader to initialize the MPD pid: get the pointer to the download session
	//this should be safe unless the mpd_pid is destroyed, which should only happen upon destruction of the DASH session
	if (group_idx==-1) {
		const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->mpd_pid, GF_PROP_PID_DOWNLOAD_SESSION);
		if (p) {
			sess = (GF_DownloadSession *) p->value.ptr;
			gf_dm_sess_force_blocking(sess);
			if (!ctx->segstore) {
				gf_dm_sess_force_memory_mode(sess, 1);
			}
			if (ctx->reuse_download_session)
				gf_dm_sess_setup_from_url(sess, url, GF_FALSE);
			ctx->reuse_download_session = GF_TRUE;
			return (GF_DASHFileIOSession) sess;
		}
	}

	if (group_idx<-1) {
		flags |= GF_NETIO_SESSION_MEMORY_CACHE;
	} else {
		if (!ctx->segstore) flags |= GF_NETIO_SESSION_MEMORY_CACHE;
		if (persistent) flags |= GF_NETIO_SESSION_PERSISTENT;
	}
	sess = gf_dm_sess_new(ctx->dm, url, flags, NULL, NULL, &e);
	return (GF_DASHFileIOSession) sess;
}
void dashdmx_io_del(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;
	if (!ctx->reuse_download_session)
		gf_dm_sess_del((GF_DownloadSession *)session);
}
GF_Err dashdmx_io_init(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process_headers((GF_DownloadSession *)session);
}
GF_Err dashdmx_io_run(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_resource_name((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_cache_name(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_cache_name((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_mime(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_mime_type((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_header_value(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *header_name)
{
	return gf_dm_sess_get_header((GF_DownloadSession *)session, header_name);
}
u64 dashdmx_io_get_utc_start_time(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_utc_start((GF_DownloadSession *)session);
}
GF_Err dashdmx_io_setup_from_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *url, s32 group_idx)
{
	return gf_dm_sess_setup_from_url((GF_DownloadSession *)session, url, GF_FALSE);
}
GF_Err dashdmx_io_set_range(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	return gf_dm_sess_set_range((GF_DownloadSession *)session, start_range, end_range, discontinue_cache);
}

u32 dashdmx_io_get_bytes_per_sec(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 bps=0;
	if (session) {
		gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, NULL, &bps, NULL);
	} else {
		GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;
		bps = gf_dm_get_data_rate(ctx->dm);
		bps/=8;
	}
	return bps;
}
#if 0 //unused since we are in non threaded mode
void dashdmx_io_abort(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	gf_dm_sess_abort((GF_DownloadSession *)session);
}
u32 dashdmx_io_get_total_size(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u64 size=0;
	gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, &size, NULL, NULL, NULL);
	return (u32) size;
}
u32 dashdmx_io_get_bytes_done(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u64 size=0;
	gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, &size, NULL, NULL);
	return (u32) size;
}
#endif

#ifdef GPAC_HAS_QJS
#include <gpac/mpd.h>
void dashdmx_js_declare_group(GF_DASHDmxCtx *ctx, u32 group_idx)
{
	u32 i, count;
	u32 srd_id, srd_x, srd_y, srd_w, srd_h, srd_fw, srd_fh;
	JSValue res, reps;
	JSValue obj = JS_NewObject(ctx->js_ctx);

	JS_SetPropertyStr(ctx->js_ctx, obj, "idx", JS_NewInt32(ctx->js_ctx, group_idx));
	JS_SetPropertyStr(ctx->js_ctx, obj, "duration", JS_NewInt64(ctx->js_ctx, gf_dash_get_period_duration(ctx->dash) ));

	srd_id = srd_x = srd_y = srd_w = srd_h = srd_fw = srd_fh = 0;
	gf_dash_group_get_srd_info(ctx->dash, group_idx, &srd_id, &srd_x, &srd_y, &srd_w, &srd_h, &srd_fw, &srd_fh);

	if (srd_fw && srd_fh) {
		JSValue srd = JS_NewObject(ctx->js_ctx);
		JS_SetPropertyStr(ctx->js_ctx, srd, "ID", JS_NewInt32(ctx->js_ctx, srd_id));
		JS_SetPropertyStr(ctx->js_ctx, srd, "x", JS_NewInt32(ctx->js_ctx, srd_x));
		JS_SetPropertyStr(ctx->js_ctx, srd, "y", JS_NewInt32(ctx->js_ctx, srd_y));
		JS_SetPropertyStr(ctx->js_ctx, srd, "w", JS_NewInt32(ctx->js_ctx, srd_w));
		JS_SetPropertyStr(ctx->js_ctx, srd, "h", JS_NewInt32(ctx->js_ctx, srd_h));
		JS_SetPropertyStr(ctx->js_ctx, srd, "fw", JS_NewInt32(ctx->js_ctx, srd_fw));
		JS_SetPropertyStr(ctx->js_ctx, srd, "fh", JS_NewInt32(ctx->js_ctx, srd_fh));
		JS_SetPropertyStr(ctx->js_ctx, obj, "SRD", srd);
	} else {
		JS_SetPropertyStr(ctx->js_ctx, obj, "SRD", JS_NULL);
	}

	reps = JS_NewArray(ctx->js_ctx);

	count = gf_dash_group_get_num_qualities(ctx->dash, group_idx);
	for (i=0; i<count; i++) {
		GF_DASHQualityInfo qinfo;
		JSValue rep;
		GF_Err e;

		e = gf_dash_group_get_quality_info(ctx->dash, group_idx, i, &qinfo);
		if (e) break;
		if (!qinfo.ID) qinfo.ID="";
		if (!qinfo.mime) qinfo.mime="unknown";
		if (!qinfo.codec) qinfo.codec="codec";

		rep = JS_NewObject(ctx->js_ctx);
		JS_SetPropertyStr(ctx->js_ctx, rep, "ID", JS_NewString(ctx->js_ctx, qinfo.ID));
		JS_SetPropertyStr(ctx->js_ctx, rep, "mime", JS_NewString(ctx->js_ctx, qinfo.mime));
		JS_SetPropertyStr(ctx->js_ctx, rep, "codec", JS_NewString(ctx->js_ctx, qinfo.codec));
		JS_SetPropertyStr(ctx->js_ctx, rep, "bitrate", JS_NewInt32(ctx->js_ctx, qinfo.bandwidth));
		JS_SetPropertyStr(ctx->js_ctx, rep, "disabled", JS_NewBool(ctx->js_ctx, qinfo.disabled));
		if (qinfo.width && qinfo.height) {
			JSValue frac;
			JS_SetPropertyStr(ctx->js_ctx, rep, "width", JS_NewInt32(ctx->js_ctx, qinfo.width));
			JS_SetPropertyStr(ctx->js_ctx, rep, "height", JS_NewInt32(ctx->js_ctx, qinfo.height));
			JS_SetPropertyStr(ctx->js_ctx, rep, "interlaced", JS_NewBool(ctx->js_ctx, qinfo.interlaced));
			frac = JS_NewObject(ctx->js_ctx);
			JS_SetPropertyStr(ctx->js_ctx, frac, "n", JS_NewInt32(ctx->js_ctx, qinfo.fps_num));
			JS_SetPropertyStr(ctx->js_ctx, frac, "d", JS_NewInt32(ctx->js_ctx, qinfo.fps_den ? qinfo.fps_den : 1));
			JS_SetPropertyStr(ctx->js_ctx, rep, "fps", frac);

			frac = JS_NewObject(ctx->js_ctx);
			JS_SetPropertyStr(ctx->js_ctx, frac, "n", JS_NewInt32(ctx->js_ctx, qinfo.par_num));
			JS_SetPropertyStr(ctx->js_ctx, frac, "d", JS_NewInt32(ctx->js_ctx, qinfo.par_den ? qinfo.par_den : qinfo.par_num));
			JS_SetPropertyStr(ctx->js_ctx, rep, "sar", frac);
		}
		if (qinfo.sample_rate) {
			JS_SetPropertyStr(ctx->js_ctx, rep, "samplerate", JS_NewInt32(ctx->js_ctx, qinfo.sample_rate));
			JS_SetPropertyStr(ctx->js_ctx, rep, "channels", JS_NewInt32(ctx->js_ctx, qinfo.nb_channels));
		}
		JS_SetPropertyStr(ctx->js_ctx, rep, "ast_offset", JS_NewFloat64(ctx->js_ctx, qinfo.ast_offset));
		JS_SetPropertyStr(ctx->js_ctx, rep, "avg_duration", JS_NewFloat64(ctx->js_ctx, qinfo.average_duration));

		if (qinfo.seg_urls) {
			u32 k=0, nb_segs=gf_list_count(qinfo.seg_urls);
			JSValue seg_sizes = JS_NewArray(ctx->js_ctx);
			for (k=0; k<nb_segs; k++) {
				GF_MPD_SegmentURL *surl = gf_list_get((GF_List *) qinfo.seg_urls, k);
				u64 size = (1 + surl->media_range->end_range - surl->media_range->start_range);
				JS_SetPropertyUint32(ctx->js_ctx, seg_sizes, k, JS_NewInt64(ctx->js_ctx, size) );
			}
			JS_SetPropertyStr(ctx->js_ctx, rep, "sizes", seg_sizes);
		} else {
			JS_SetPropertyStr(ctx->js_ctx, rep, "sizes", JS_NULL);
		}
		JS_SetPropertyUint32(ctx->js_ctx, reps, i, rep);
	}

	JS_SetPropertyStr(ctx->js_ctx, obj, "qualities", reps);

	res = JS_Call(ctx->js_ctx, ctx->new_group_fun, ctx->js_obj, 1, &obj);
	JS_FreeValue(ctx->js_ctx, obj);
	JS_FreeValue(ctx->js_ctx, res);
}
#endif

static void dashdmx_declare_group(GF_DASHDmxCtx *ctx, u32 group_idx)
{
	if (ctx->on_new_group)
		ctx->on_new_group(ctx->rt_udta, group_idx, ctx->dash);
#ifdef GPAC_HAS_QJS
	else if (ctx->js_ctx && JS_IsFunction(ctx->js_ctx, ctx->new_group_fun)) {
		dashdmx_js_declare_group(ctx, group_idx);
	}
#endif
}

void dashdmx_io_manifest_updated(GF_DASHFileIO *dashio, const char *manifest_name, const char *cache_url, s32 group_idx)
{
	u8 *manifest_payload;
	u32 manifest_payload_len;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;

	if (gf_file_load_data(cache_url, &manifest_payload, &manifest_payload_len) == GF_OK) {
		u8 *output;

		//strip baseURL since we are recording, links are already resolved
		char *man_pay_start = manifest_payload;
		while (1) {
			u32 end_len, offset;
			manifest_payload_len = (u32) strlen(manifest_payload);
			char *base_url_start = strstr(man_pay_start, "<BaseURL>");
			if (!base_url_start) break;
			char *base_url_end = strstr(base_url_start, "</BaseURL>");
			if (!base_url_end) break;
			offset = 10;
			while (base_url_end[offset] == '\n')
				offset++;
			end_len = (u32) strlen(base_url_end+offset);
			memmove(base_url_start, base_url_end+offset, end_len);
			base_url_start[end_len]=0;
			man_pay_start = base_url_start;
		}

		if ((ctx->forward==DFWD_FILE) && ctx->output_mpd_pid) {
			GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->output_mpd_pid, manifest_payload_len, &output);
			if (pck) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] Manifest %s updated, forwarding\n", manifest_name));
				memcpy(output, manifest_payload, manifest_payload_len);
				gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
				gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENAME, &PROP_STRING(manifest_name));
				if (group_idx>=0)
					gf_filter_pck_set_property(pck, GF_PROP_PCK_HLS_REF, &PROP_LONGUINT( (u64) 1+group_idx) );

				gf_filter_pck_send(pck);
			}
		} else if (ctx->forward==DFWD_SBOUND_MANIFEST) {
			if (group_idx>=0) {
				assert(manifest_name);
				if (!ctx->hls_variants) ctx->hls_variants = gf_list_new();
				if (!ctx->hls_variants_names) ctx->hls_variants_names = gf_list_new();
				gf_list_add(ctx->hls_variants, manifest_payload);
				manifest_payload = NULL;
				gf_list_add(ctx->hls_variants_names, gf_strdup(manifest_name) );

			} else {
				if (ctx->manifest_payload) gf_free(ctx->manifest_payload);
				ctx->manifest_payload = manifest_payload;
				manifest_payload = NULL;
			}
		}
		if (manifest_payload)
			gf_free(manifest_payload);
	}
}

static GF_Err dashin_abort(GF_DASHDmxCtx *ctx);

GF_Err dashdmx_io_on_dash_event(GF_DASHFileIO *dashio, GF_DASHEventType dash_evt, s32 group_idx, GF_Err error_code)
{
	GF_Err e;
	u32 i;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;

	if (dash_evt==GF_DASH_EVENT_PERIOD_SETUP_ERROR) {
		if (!ctx->initial_setup_done) {
			gf_filter_setup_failure(ctx->filter, error_code);
			ctx->initial_setup_done = GF_TRUE;
		}
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_SELECT_GROUPS) {
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			gf_dash_groups_set_language(ctx->dash, gf_opts_get_key("core", "lang"));
			//not used in the test suite (require JS)
			gf_dash_switch_quality(ctx->dash, GF_TRUE);
			//not used relyably in the test suite (require fatal error in session)
			dashin_abort(NULL);
		}
#endif
		if (ctx->groupsel)
			gf_dash_groups_set_language(ctx->dash, gf_opts_get_key("core", "lang"));

		//let the player decide which group to play: we declare everything
		return GF_OK;
	}

	/*for all selected groups, create input service and connect to init/first segment*/
	if (dash_evt==GF_DASH_EVENT_CREATE_PLAYBACK) {
		u32 nb_groups_selected = 0;
		//coverage of a few functions from old arch not deprecated (yet)
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			gf_dash_is_group_selected(ctx->dash, 0);
			gf_dash_get_url(ctx->dash);
			gf_dash_group_loop_detected(ctx->dash, 0);
			gf_dash_is_dynamic_mpd(ctx->dash);
			gf_dash_group_get_language(ctx->dash, 0);
			gf_dash_group_get_num_components(ctx->dash, 0);
			gf_dash_get_utc_drift_estimate(ctx->dash);


			//these are not used in the test suite (require decoding)
			gf_dash_group_set_codec_stat(ctx->dash, 0, 0, 0, 0, 0, GF_FALSE, GF_FALSE);
			gf_dash_group_set_buffer_levels(ctx->dash, 0, 0, 0, 0);

			//these are not used in the test suite (require JS)
			if (!strcmp(ctx->algo, "none"))
				gf_dash_set_automatic_switching(ctx->dash, GF_FALSE);
			gf_dash_group_select_quality(ctx->dash, (u32) -1, NULL, 0);

			//these are not used yet in the test suite (require TEMI)
			gf_dash_override_ntp(ctx->dash, 0);

			//this is not used yet in the test suite (require user interaction)
			gf_dash_group_set_quality_degradation_hint(ctx->dash, 0, 0);
		}
#endif

		if (ctx->on_period_reset)
			ctx->on_period_reset(ctx->rt_udta, gf_dash_is_dynamic_mpd(ctx->dash) ? 2 : 1);
#ifdef GPAC_HAS_QJS
		else if (ctx->js_ctx && JS_IsFunction(ctx->js_ctx, ctx->period_reset_fun)) {
			JSValue arg;
			arg = JS_NewInt32(ctx->js_ctx, gf_dash_is_dynamic_mpd(ctx->dash) ? 2 : 1);
			JSValue res = JS_Call(ctx->js_ctx, ctx->period_reset_fun, ctx->js_obj, 1, &arg);
			JS_FreeValue(ctx->js_ctx, res);
		}
#endif


		/*select input services if possible*/
		for (i=0; i<gf_dash_get_group_count(ctx->dash); i++) {
			const char *mime, *init_segment;
			u64 start_range, end_range;
			u32 j;
			Bool playable = GF_TRUE;
			//let the player decide which group to play
			if (!gf_dash_is_group_selectable(ctx->dash, i))
				continue;

			if (ctx->groupsel && !gf_dash_is_group_selected(ctx->dash, i))
				continue;

			j=0;
			while (1) {
				const char *desc_id, *desc_scheme, *desc_value;
				if (! gf_dash_group_enum_descriptor(ctx->dash, i, GF_MPD_DESC_ESSENTIAL_PROPERTIES, j, &desc_id, &desc_scheme, &desc_value))
					break;
				j++;
				if (!strcmp(desc_scheme, "urn:mpeg:dash:srd:2014")) {
				} else if (!strcmp(desc_scheme, "http://dashif.org/guidelines/trickmode")) {
				} else {
					playable = GF_FALSE;
					break;
				}
			}
			if (!playable) {
				gf_dash_group_select(ctx->dash, i, GF_FALSE);
				continue;
			}

			nb_groups_selected++;

			if (gf_dash_group_has_dependent_group(ctx->dash, i) >=0 ) {
				gf_dash_group_select(ctx->dash, i, GF_TRUE);
				dashdmx_declare_group(ctx, i);
				continue;
			}

			mime = gf_dash_group_get_segment_mime(ctx->dash, i);
			init_segment = gf_dash_group_get_segment_init_url(ctx->dash, i, &start_range, &end_range);

			e = dashdmx_load_source(ctx, i, mime, init_segment, start_range, end_range);
			if (e != GF_OK) {
				gf_dash_group_select(ctx->dash, i, GF_FALSE);
			} else {
				u32 w, h;
				/*connect our media service*/
				gf_dash_group_get_video_info(ctx->dash, i, &w, &h);
				if (w && h && w>ctx->width && h>ctx->height) {
					ctx->width = w;
					ctx->height = h;
				}
				if (gf_dash_group_get_srd_max_size_info(ctx->dash, i, &w, &h)) {
					ctx->width = w;
					ctx->height = h;
				}
				dashdmx_declare_group(ctx, i);
			}
		}

		if (!ctx->initial_setup_done) {
			ctx->initial_setup_done = GF_TRUE;
		}

		if (!nb_groups_selected) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] No groups selectable, not playing !\n"));
		}
		return GF_OK;
	}

	/*for all running services, stop service*/
	if (dash_evt==GF_DASH_EVENT_DESTROY_PLAYBACK) {
		for (i=0; i<gf_dash_get_group_count(ctx->dash); i++) {
			GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
			if (!group) continue;
			if (group->seg_filter_src) {
				gf_filter_remove_src(ctx->filter, group->seg_filter_src);
				group->seg_filter_src = NULL;
			}
			if (group->template) gf_free(group->template);
			if (group->current_url) gf_free(group->current_url);
			gf_free(group);
			gf_dash_set_group_udta(ctx->dash, i, NULL);
		}
		for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
			gf_filter_pid_set_udta(opid, NULL);
		}

		if (ctx->on_period_reset)
			ctx->on_period_reset(ctx->rt_udta, 0);
#ifdef GPAC_HAS_QJS
		else if (ctx->js_ctx && JS_IsFunction(ctx->js_ctx, ctx->period_reset_fun)) {
			JSValue arg = JS_NewInt32(ctx->js_ctx, 0);
			JSValue res = JS_Call(ctx->js_ctx, ctx->period_reset_fun, ctx->js_obj, 1, &arg);
			JS_FreeValue(ctx->js_ctx, res);
		}
#endif

		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_ABORT_DOWNLOAD) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, group_idx);
		if (group) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_SOURCE_SWITCH,  NULL);
			evt.seek.start_offset = -1;
			evt.seek.source_switch = NULL;
			gf_filter_send_event(group->seg_filter_src, &evt, GF_FALSE);
		}
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_QUALITY_SWITCH) {
		if (group_idx<0) return GF_OK;

		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, group_idx);

		if (!group) {
			group_idx = gf_dash_group_has_dependent_group(ctx->dash, group_idx);
			group = gf_dash_get_group_udta(ctx->dash, group_idx);
		}
		//do not notify HAS status (selected qualities & co) right away, we are still potentially processing packets from previous segment(s)
		if (group)
			group->notify_quality_change = GF_TRUE;

		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_TIMESHIFT_UPDATE) {
		Double timeshift = gf_dash_get_timeshift_buffer_pos(ctx->dash);
		for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
			gf_filter_pid_set_info(opid, GF_PROP_PID_TIMESHIFT_TIME, &PROP_DOUBLE(timeshift) );
		}
		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_TIMESHIFT_OVERFLOW) {
		u32 evttype = (group_idx>=0) ? 2 : 1;
		for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
			gf_filter_pid_set_info(opid, GF_PROP_PID_TIMESHIFT_STATE, &PROP_UINT(evttype) );
		}
	}

	if (dash_evt==GF_DASH_EVENT_CODEC_STAT_QUERY) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, group_idx);
		for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
			if (gf_filter_pid_get_udta(opid) == group) {
				GF_FilterPidStatistics stats;
				Bool is_decoder = GF_TRUE;
				//collect decoder stats, or if not found direct output
				if (gf_filter_pid_get_statistics(opid, &stats, GF_STATS_DECODER_SINK) != GF_OK) {
					is_decoder = GF_FALSE;
					if (gf_filter_pid_get_statistics(opid, &stats, GF_STATS_LOCAL_INPUTS) != GF_OK)
						continue;
				}

				if (stats.disconnected)
					continue;

				if (is_decoder) {
					if (!stats.nb_processed) stats.nb_processed=1;
					if (!stats.nb_saps) stats.nb_saps=1;
					gf_dash_group_set_codec_stat(ctx->dash, group_idx, (u32) (stats.total_process_time/stats.nb_processed), stats.max_process_time, (u32) (stats.total_sap_process_time/stats.nb_saps), stats.max_sap_process_time, GF_FALSE, GF_FALSE);
				}

				gf_dash_group_set_buffer_levels(ctx->dash, group_idx, (u32) (stats.min_playout_time/1000), (u32) (stats.max_buffer_time/1000), (u32) (stats.buffer_time/1000) );

				break;
			}
		}
	}

	//discontinuity at next period, recompute timeline anchor at end of pres
	if ((dash_evt==GF_DASH_EVENT_END_OF_PERIOD) && group_idx) {
		ctx->time_discontinuity.num = 0;
		ctx->time_discontinuity.den = 0;

		for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);

			//in forward mode, signal period switch at packet level but don't touch timestamps for now
			if (ctx->forward) {
				gf_filter_pid_set_udta_flags(opid, FLAG_PERIOD_SWITCH);
				continue;
			}
			u32 timescale = gf_filter_pid_get_timescale(opid);
			u64 ts = gf_filter_pid_get_next_ts(opid);
			if (ts==GF_FILTER_NO_TS) continue;

			if (!ctx->time_discontinuity.den || gf_timestamp_greater(ts, timescale, ctx->time_discontinuity.num, ctx->time_discontinuity.den)) {
				ctx->time_discontinuity.num = ts;
				ctx->time_discontinuity.den = timescale;
			}
		}
		if (ctx->time_discontinuity.num && ctx->time_discontinuity.den) {
			gf_filter_post_process_task(ctx->filter);
			ctx->compute_min_dts = GF_TRUE;
		}
	}

	return GF_OK;
}

static void dashdmx_setup_buffer(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 buffer_ms, play_buf_ms;
	gf_filter_get_output_buffer_max(ctx->filter, &buffer_ms, &play_buf_ms);
	buffer_ms /= 1000;
	play_buf_ms /= 1000;

	//use min buffer from MPD
	if (ctx->use_bmin==BMIN_MPD) {
		u64 mpd_buffer_ms = gf_dash_get_min_buffer_time(ctx->dash);
		if (mpd_buffer_ms > buffer_ms)
			buffer_ms = (u32) mpd_buffer_ms;
	}
	if (buffer_ms) {
		gf_dash_set_user_buffer(ctx->dash, buffer_ms);
	}
	gf_dash_group_set_max_buffer_playout(ctx->dash, group->idx, play_buf_ms);
}

/*check in all groups if the service can support reverse playback (speed<0); return GF_OK only if service is supported in all groups*/
static u32 dashdmx_dash_playback_mode(GF_DASHDmxCtx *ctx)
{
	u32 pmode, mode, i, count = gf_filter_get_ipid_count(ctx->filter);
	mode = GF_PLAYBACK_MODE_REWIND;
	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
		GF_FilterPid *pid = gf_filter_get_ipid(ctx->filter, i);
		if (ctx->mpd_pid == pid) continue;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
		pmode = p ? p->value.uint : GF_PLAYBACK_MODE_REWIND;
		if (pmode < mode) mode = pmode;
	}
	return mode;
}


static s32 dashdmx_group_idx_from_pid(GF_DASHDmxCtx *ctx, GF_FilterPid *src_pid)
{
	s32 i;

	for (i=0; (u32) i < gf_dash_get_group_count(ctx->dash); i++) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
		if (!group) continue;
		if (gf_filter_pid_is_filter_in_parents(src_pid, group->seg_filter_src))
			return i;
	}
	return -1;
}

static GF_FilterPid *dashdmx_opid_from_group(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	s32 i;

	for (i=0; (u32) i < gf_filter_get_opid_count(ctx->filter); i++) {
		GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i );
		GF_DASHGroup *g = gf_filter_pid_get_udta( opid );
		if (g == group) return opid;
	}
	return NULL;
}

static GF_FilterPid *dashdmx_create_output_pid(GF_DASHDmxCtx *ctx, GF_FilterPid *input, u32 *run_status, GF_DASHGroup *for_group)
{
	u32 global_score=0;
	GF_FilterPid *output_pid = NULL;
	u32 i, count = gf_filter_get_opid_count(ctx->filter);
	const GF_PropertyValue *codec, *streamtype, *role, *lang;

	*run_status = 0;

	streamtype = gf_filter_pid_get_property(input, GF_PROP_PID_STREAM_TYPE);
	if (streamtype && streamtype->value.uint==GF_STREAM_ENCRYPTED)
		streamtype = gf_filter_pid_get_property(input, GF_PROP_PID_ORIG_STREAM_TYPE);

	codec = gf_filter_pid_get_property(input, GF_PROP_PID_CODECID);
	role = gf_filter_pid_get_property(input, GF_PROP_PID_ROLE);
	lang = gf_filter_pid_get_property(input, GF_PROP_PID_LANGUAGE);

	for (i=0; i<count; i++) {
		u32 score;
		const GF_PropertyValue *o_codec, *o_streamtype, *o_role, *o_lang;
		GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
		//in use by us
		if (gf_filter_pid_get_udta(opid)) continue;
		if (opid == ctx->output_mpd_pid) continue;

		o_streamtype = gf_filter_pid_get_property(opid, GF_PROP_PID_STREAM_TYPE);
		if (o_streamtype && o_streamtype->value.uint==GF_STREAM_ENCRYPTED)
			o_streamtype = gf_filter_pid_get_property(opid, GF_PROP_PID_ORIG_STREAM_TYPE);

		o_codec = gf_filter_pid_get_property(opid, GF_PROP_PID_CODECID);
		o_role = gf_filter_pid_get_property(opid, GF_PROP_PID_ROLE);
		o_lang = gf_filter_pid_get_property(opid, GF_PROP_PID_LANGUAGE);

		if (!o_streamtype || !streamtype || !gf_props_equal(streamtype, o_streamtype))
			continue;

		score = 1;
		//get highest priority for streams with same role
		if (o_role && role && gf_props_equal(role, o_role)) score += 10;
		//then high priority for streams with same lang
		if (o_lang && lang && gf_props_equal(lang, o_lang)) score += 5;

		//otherwise favour streams with same codec
		if (!o_codec && codec && gf_props_equal(codec, o_codec)) score++;

		if (global_score<score) {
			global_score = score;
			output_pid = opid;
		}
	}
	if (output_pid) {
		*run_status = gf_filter_pid_is_playing(output_pid) ? 1 : 2;
		return output_pid;
	}
	//none found create a new PID - mark pid as pending so we don't process it until we get play
	for_group->nb_pending++;
	return gf_filter_pid_new(ctx->filter);
}

Bool dashdmx_merge_prop(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop)
{
	const GF_PropertyValue *p;
	GF_FilterPid *pid = (GF_FilterPid *) cbk;

	if (prop_4cc) p = gf_filter_pid_get_property(pid, prop_4cc);
	else p = gf_filter_pid_get_property_str(pid, prop_name);
	if (p) return GF_FALSE;
	return GF_TRUE;
}


static void dashdm_format_qinfo(char **q_desc, GF_DASHQualityInfo *qinfo)
{
	GF_Err e;
	char szInfo[500];
	if (!qinfo->ID) qinfo->ID="";
	if (!qinfo->mime) qinfo->mime="unknown";
	if (!qinfo->codec) qinfo->codec="codec";

	snprintf(szInfo, 500, "id=%s", qinfo->ID);

	e = gf_dynstrcat(q_desc, szInfo, "::");
	if (e) return;

	snprintf(szInfo, 500, "codec=%s", qinfo->codec);
	e = gf_dynstrcat(q_desc, szInfo, "::");
	if (e) return;

	snprintf(szInfo, 500, "mime=%s", qinfo->mime);
	e = gf_dynstrcat(q_desc, szInfo, "::");
	if (e) return;

	snprintf(szInfo, 500, "bw=%d", qinfo->bandwidth);
	e = gf_dynstrcat(q_desc, szInfo, "::");
	if (e) return;

	if (qinfo->disabled) {
		e = gf_dynstrcat(q_desc, "disabled", "::");
		if (e) return;
	}

	if (qinfo->width && qinfo->height) {
		snprintf(szInfo, 500, "w=%d", qinfo->width);
		e = gf_dynstrcat(q_desc, szInfo, "::");
		if (e) return;

		snprintf(szInfo, 500, "h=%d", qinfo->height);
		e = gf_dynstrcat(q_desc, szInfo, "::");
		if (e) return;

		if (qinfo->interlaced) {
			e = gf_dynstrcat(q_desc, "interlaced", "::");
			if (e) return;
		}
		if (qinfo->fps_den) {
			snprintf(szInfo, 500, "fps=%d/%d", qinfo->fps_num, qinfo->fps_den);
		} else {
			snprintf(szInfo, 500, "fps=%d", qinfo->fps_num);
		}
		e = gf_dynstrcat(q_desc, szInfo, "::");
		if (e) return;

		if (qinfo->par_den && qinfo->par_num && (qinfo->par_den != qinfo->par_num)) {
			snprintf(szInfo, 500, "sar=%d/%d", qinfo->par_num, qinfo->par_den);
			e = gf_dynstrcat(q_desc, szInfo, "::");
			if (e) return;
		}
	}
	if (qinfo->sample_rate) {
		snprintf(szInfo, 500, "sr=%d", qinfo->sample_rate);
		e = gf_dynstrcat(q_desc, szInfo, "::");
		if (e) return;

		snprintf(szInfo, 500, "ch=%d", qinfo->nb_channels);
		e = gf_dynstrcat(q_desc, szInfo, "::");
		if (e) return;
	}
}

const char *gf_dash_group_get_clearkey_uri(GF_DashClient *dash, u32 group_idx, bin128 *def_kid);

static void dashdmx_declare_properties(GF_DASHDmxCtx *ctx, GF_DASHGroup *group, u32 group_idx, GF_FilterPid *opid, GF_FilterPid *ipid, Bool is_period_switch)
{
	GF_DASHQualityInfo qinfo;
	GF_PropertyValue qualities, srd, srdref;
	GF_Err e;
	u32 stream_type = GF_STREAM_UNKNOWN;
	u32 count, i;
	u32 dur, mode;

	gf_filter_pid_copy_properties(opid, ipid);

	if (!group->nb_group_deps) {
		s32 asid;
		char as_name[100];
		asid = gf_dash_group_get_as_id(ctx->dash, group_idx);
		//TODO: remove and regenerate hashes
		if (gf_sys_is_test_mode() && (asid<=0)) {
			sprintf(as_name, "AS%d", group_idx+1);
		} else if (asid<0) {
			sprintf(as_name, "AS_%d", group_idx+1);
		} else {
			sprintf(as_name, "AS%d", asid);
		}
		gf_filter_pid_set_name(opid, as_name);
	}

	mode = dashdmx_dash_playback_mode(ctx);
	gf_filter_pid_set_property(opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(mode));

	if (ctx->max_res && gf_filter_pid_get_property(ipid, GF_PROP_PID_WIDTH)) {
		gf_filter_pid_set_property(opid, GF_PROP_SERVICE_WIDTH, &PROP_UINT(ctx->width));
		gf_filter_pid_set_property(opid, GF_PROP_SERVICE_HEIGHT, &PROP_UINT(ctx->height));
	}

	dur = (u32) (1000*gf_dash_get_duration(ctx->dash) );
	if (dur>0)
		gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(dur, 1000) );
	else
		gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, NULL);

	dur = gf_dash_group_get_time_shift_buffer_depth(ctx->dash, group_idx);
	if ((s32) dur > 0)
		gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESHIFT_DEPTH, &PROP_FRAC_INT(1000*dur, 1000) );


	if (ctx->use_bmin==BMIN_MPD) {
		u32 max = gf_dash_get_min_buffer_time(ctx->dash);
		gf_filter_pid_set_property(opid, GF_PROP_PID_PLAY_BUFFER, &PROP_UINT(max));
	}
	//check if low latency is on, if not notify max segment duration as target min buffer
	//we don't do this in test mode, it breaks all inspect hashes
	else if ((ctx->use_bmin==BMIN_AUTO) && !gf_sys_is_test_mode()) {
		Bool do_set = GF_FALSE;
		u32 min_buf = gf_dash_get_max_segment_duration(ctx->dash);
		//low latency not enabled or not active
		if (!ctx->lowlat || !gf_dash_is_low_latency(ctx->dash, group_idx)) {
			do_set = GF_TRUE;
		}
		//or we have dependent groups, no low latency possible for now
		else if (group->nb_group_deps) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Low-Latency dependent group (tiling & co) not supported, using regular download\n"));
			do_set = GF_TRUE;
			ctx->lowlat = 0;
			gf_dash_set_low_latency_mode(ctx->dash, ctx->lowlat);
		}
		//set play buffer for all output pids
		if (do_set && min_buf) {
			for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
				GF_FilterPid *a_opid = gf_filter_get_opid(ctx->filter, i);
				gf_filter_pid_set_property(a_opid, GF_PROP_PID_PLAY_BUFFER, &PROP_UINT(min_buf));
			}
		}
	}

	memset(&qualities, 0, sizeof(GF_PropertyValue));
	qualities.type = GF_PROP_STRING_LIST;

	count = gf_dash_group_get_num_qualities(ctx->dash, group_idx);
	for (i=0; i<count; i++) {
		char *qdesc = NULL;

		e = gf_dash_group_get_quality_info(ctx->dash, group_idx, i, &qinfo);
		if (e) break;

		if (!qinfo.mime) qinfo.mime="unknown";
		if (!qinfo.codec) qinfo.codec="codec";

		if (qinfo.width && qinfo.height) {
			stream_type = GF_STREAM_VISUAL;
		} else if (qinfo.sample_rate || qinfo.nb_channels) {
			stream_type = GF_STREAM_AUDIO;
		} else if (strstr(qinfo.mime, "text")
			|| strstr(qinfo.codec, "vtt")
			|| strstr(qinfo.codec, "srt")
			|| strstr(qinfo.codec, "text")
			|| strstr(qinfo.codec, "tx3g")
			|| strstr(qinfo.codec, "stxt")
			|| strstr(qinfo.codec, "stpp")
		) {
			stream_type = GF_STREAM_TEXT;
		}
		dashdm_format_qinfo(&qdesc, &qinfo);

		qualities.value.string_list.vals = gf_realloc(qualities.value.string_list.vals, sizeof(char *) * (qualities.value.string_list.nb_items+1));

		qualities.value.string_list.vals[qualities.value.string_list.nb_items] = qdesc;
		qualities.value.string_list.nb_items++;
		qdesc = NULL;
	}
	gf_filter_pid_set_info_str(opid, "has:qualities", &qualities);

	if (group->nb_group_deps) {
		GF_PropertyValue srd_deps;
		gf_filter_pid_set_info_str(opid, "has:group_deps", &PROP_UINT(group->nb_group_deps) );
		memset(&srd_deps, 0, sizeof(GF_PropertyValue));
		srd_deps.type = GF_PROP_STRING_LIST;
		srd_deps.value.string_list.nb_items = group->nb_group_deps;
		srd_deps.value.string_list.vals = gf_malloc(sizeof(char *) * group->nb_group_deps);

		for (i=0; i<group->nb_group_deps; i++) {
			char szSRDInf[1024];
			GF_PropertyValue deps_q;
			u32 k, nb_q;
			u32 g_idx = gf_dash_get_dependent_group_index(ctx->dash, group_idx, i);
			szSRDInf[0] = 0;
			if (gf_dash_group_get_srd_info(ctx->dash, g_idx, NULL,
					&srd.value.vec4i.x,
					&srd.value.vec4i.y,
					&srd.value.vec4i.z,
					&srd.value.vec4i.w,
					&srdref.value.vec2i.x,
					&srdref.value.vec2i.y)
			) {
				sprintf(szSRDInf, "%dx%dx%dx%d@%dx%d", srd.value.vec4i.x, srd.value.vec4i.y, srd.value.vec4i.z, srd.value.vec4i.w, srdref.value.vec2i.x, srdref.value.vec2i.y);
			}
			srd_deps.value.string_list.vals[i] = gf_strdup(szSRDInf);

			memset(&deps_q, 0, sizeof(GF_PropertyValue));
			deps_q.type = GF_PROP_STRING_LIST;
			nb_q = gf_dash_group_get_num_qualities(ctx->dash, g_idx);
			deps_q.value.string_list.nb_items = nb_q;
			deps_q.value.string_list.vals = gf_malloc(sizeof(char *) * nb_q);

			for (k=0; k<nb_q; k++) {
				char *qdesc = NULL;
				e = gf_dash_group_get_quality_info(ctx->dash, g_idx, k, &qinfo);
				if (e) break;

				dashdm_format_qinfo(&qdesc, &qinfo);
				deps_q.value.string_list.vals[k] = qdesc;
			}
			sprintf(szSRDInf, "has:deps_%d_qualities", i+1);
			gf_filter_pid_set_info_dyn(opid, szSRDInf, &deps_q);
		}
		gf_filter_pid_set_info_str(opid, "has:groups_srd", &srd_deps);
	}

	if ((ctx->forward==DFWD_FILE) && (stream_type!=GF_STREAM_UNKNOWN)) {
		gf_filter_pid_set_property(opid, GF_PROP_PID_ORIG_STREAM_TYPE, &PROP_UINT(stream_type) );

		gf_filter_pid_set_property(opid, GF_PROP_PCK_HLS_REF, &PROP_LONGUINT( (u64) 1+group->idx) );

		if (!gf_dash_group_has_init_segment(ctx->dash, group_idx)) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_NO_INIT, &PROP_BOOL(GF_TRUE) );
		}
	}

	if (ctx->forward == DFWD_FILE) {
		if (gf_dash_is_dynamic_mpd(ctx->dash)) {
			u32 segdur, timescale;
			u64 tsb = (u64) gf_dash_group_get_time_shift_buffer_depth(ctx->dash, group_idx);
			gf_dash_group_get_segment_duration(ctx->dash, group_idx, &segdur, &timescale);
			if (segdur) {
				tsb *= timescale;
				tsb /= segdur;
				tsb /= 1000; //tsb given in ms
			} else {
				tsb = 0;
			}
			tsb++;
			gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESHIFT_SEGS, &PROP_UINT((u32) tsb) );
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESHIFT_SEGS, NULL );
		}
	}

	const char *title, *source;
	gf_dash_get_info(ctx->dash, &title, &source);
	gf_filter_pid_set_info(opid, GF_PROP_PID_SERVICE_NAME, &PROP_STRING(title) );
	gf_filter_pid_set_info(opid, GF_PROP_PID_SERVICE_PROVIDER, &PROP_STRING(source) );

	title = NULL;
	gf_dash_group_enum_descriptor(ctx->dash, group_idx, GF_MPD_DESC_ROLE, 0, NULL, NULL, &title);
	if (title)
		gf_filter_pid_set_property(opid, GF_PROP_PID_ROLE, &PROP_STRING(title) );

	title = NULL;
	gf_dash_group_enum_descriptor(ctx->dash, group_idx, GF_MPD_DESC_ACCESSIBILITY, 0, NULL, NULL, &title);
	if (title)
		gf_filter_pid_set_property_str(opid, "accessibility", &PROP_STRING(title) );

	title = NULL;
	gf_dash_group_enum_descriptor(ctx->dash, group_idx, GF_MPD_DESC_RATING, 0, NULL, NULL, &title);
	if (title)
		gf_filter_pid_set_property_str(opid, "rating", &PROP_STRING(title) );

	if (!gf_sys_is_test_mode()) {
		gf_filter_pid_set_info_str(opid, "ntpdiff", &PROP_SINT(gf_dash_get_utc_drift_estimate(ctx->dash) ) );
	}

	memset(&srd, 0, sizeof(GF_PropertyValue));
	memset(&srdref, 0, sizeof(GF_PropertyValue));
	srd.type = GF_PROP_VEC4I;
	srdref.type = GF_PROP_VEC2I;
	if (gf_dash_group_get_srd_info(ctx->dash, group_idx, NULL,
			&srd.value.vec4i.x,
			&srd.value.vec4i.y,
			&srd.value.vec4i.z,
			&srd.value.vec4i.w,
			&srdref.value.vec2i.x,
			&srdref.value.vec2i.y)) {

		gf_filter_pid_set_property(opid, GF_PROP_PID_SRD, &srd);
		gf_filter_pid_set_property(opid, GF_PROP_PID_SRD_REF, &srdref);
	}

	//setup initial quality - this is disabled in test mode for the time being (invalidates all dash playback hashes)
	//in forward mode, always send the event to setup dash templates
	if (!gf_sys_is_test_mode() || ctx->forward) {
		group->notify_quality_change = GF_TRUE;
		dashdmx_notify_group_quality(ctx, group);
	}

	//if MPD file pid is defined, merge its properties. This will allow forwarding user-defined properties,
	// eg -i dash.mpd:#MyProp=toto to all PIDs coming from media sources
	if (ctx->mpd_pid) {
		gf_filter_pid_merge_properties(opid, ctx->mpd_pid, dashdmx_merge_prop, ipid);
	}

	if (ctx->frag_url)
		gf_filter_pid_set_property(opid, GF_PROP_PID_ORIG_FRAG_URL, &PROP_NAME(ctx->frag_url) );

	if (stream_type == GF_STREAM_AUDIO) {
		Bool is_cont = GF_FALSE;
		if (is_period_switch) {
			u32 j=0;
			while (1) {
				const char *desc_id, *desc_scheme, *desc_value;
				if (! gf_dash_group_enum_descriptor(ctx->dash, group_idx, GF_MPD_DESC_SUPPLEMENTAL_PROPERTIES, j, &desc_id, &desc_scheme, &desc_value))
					break;
				j++;
				if (!strcmp(desc_scheme, "urn:mpeg:dash:period-continuity:2015") && desc_value) {
					is_cont = GF_TRUE;
					break;
				}
			}
		}
		gf_filter_pid_set_property(opid, GF_PROP_PID_NO_PRIMING, is_cont ? &PROP_BOOL(GF_TRUE) : NULL);
	}

	if (group->hls_key_uri) {
		gf_filter_pid_set_property(opid, GF_PROP_PID_HLS_KMS, &PROP_STRING(group->hls_key_uri));
		gf_filter_pid_set_property(opid, GF_PROP_PID_HLS_IV, &PROP_DATA(group->hls_key_IV, sizeof(bin128) ));
	} else {
		bin128 ck_kid;
		const char *ckuri = gf_dash_group_get_clearkey_uri(ctx->dash, group_idx, &ck_kid);
		if (ckuri) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_CLEARKEY_URI, &PROP_STRING(ckuri));
			gf_filter_pid_set_property(opid, GF_PROP_PID_CLEARKEY_KID, &PROP_DATA(ck_kid, sizeof(bin128) ));
		}
	}
	if (ctx->forward > DFWD_FILE) {
		u64 pstart;
		u32 timescale;
		const char *str = NULL;
		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_FWD, &PROP_UINT( (ctx->forward - DFWD_FILE) ) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_CUE, &PROP_STRING("inband") );

		gf_dash_group_get_segment_duration(ctx->dash, group->idx, &dur, &timescale);
		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_DUR, &PROP_FRAC_INT(dur, timescale) );

		gf_dash_group_next_seg_info(ctx->dash, group->idx, group->current_dependent_rep_idx, NULL, NULL, NULL, NULL, &str);
		if (str) {
			gf_filter_pid_set_property(opid, GF_PROP_PCK_FILENAME, &PROP_STRING(str) );
		}

		//forward representation ID so that dasher will match muxed streams and identify streams in the MPD
		str = gf_dash_group_get_representation_id(ctx->dash, group->idx);
		if (str) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(str) );
		}
		pstart = gf_dash_get_period_start(ctx->dash);
		gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_PERIOD_START, &PROP_LONGUINT(pstart) );
	}
}

static GF_Err dashdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	s32 group_idx;
	Bool is_period_switch = GF_FALSE;
	GF_FilterPid *opid;
	GF_Err e;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	GF_DASHGroup *group;

	if (is_remove) {
		//TODO
		if (pid==ctx->mpd_pid) {
			u32 i;
			for (i=0; i<gf_filter_get_opid_count(filter); i++) {
				opid = gf_filter_get_opid(filter, i);
				group = gf_filter_pid_get_udta(opid);
				if (group && group->seg_filter_src) {
					gf_filter_remove_src(filter, group->seg_filter_src);
				}
			}
			gf_dash_close(ctx->dash);
			ctx->mpd_pid = NULL;
		} else {
			opid = gf_filter_pid_get_udta(pid);
			if (opid) {
				if (!ctx->mpd_pid) {
					gf_filter_pid_remove(opid);
				} else if (gf_dash_all_groups_done(ctx->dash) && gf_dash_in_last_period(ctx->dash, GF_TRUE)) {
					gf_filter_pid_remove(opid);
				} else {
					gf_filter_pid_set_udta(opid, NULL);
					gf_filter_pid_set_udta(pid, NULL);
				}
			}
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Mismatch in input pid caps\n"));
		return GF_NOT_SUPPORTED;
	}

	//configure MPD pid
	if (!ctx->mpd_pid) {
		char *frag;
		const GF_PropertyValue *p;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
		if (!p || !p->value.string) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] no URL on MPD pid\n"));
			return GF_NOT_SUPPORTED;
		}

		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
		ctx->mpd_pid = pid;
		ctx->seek_request = -1;
		ctx->nb_playing = 0;

		if ((ctx->forward==DFWD_FILE) && !ctx->output_mpd_pid) {
			ctx->output_mpd_pid = gf_filter_pid_new(filter);
			gf_filter_pid_copy_properties(ctx->output_mpd_pid, pid);
			gf_filter_pid_set_name(ctx->output_mpd_pid, "manifest");
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] Creating manifest output PID\n"));
			//for route
			gf_filter_pid_set_property(ctx->output_mpd_pid, GF_PROP_PID_ORIG_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
			gf_filter_pid_set_property(ctx->output_mpd_pid, GF_PROP_PID_IS_MANIFEST, &PROP_BOOL(GF_TRUE));
		}

		e = gf_dash_open(ctx->dash, p->value.string);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error - cannot initialize DASH Client for %s: %s\n", p->value.string, gf_error_to_string(e)));
			gf_filter_setup_failure(filter, e);
			return e;
		}
		frag = strchr(p->value.string, '#');
		if (frag) {
			if (ctx->frag_url) gf_free(ctx->frag_url);
			ctx->frag_url = gf_strdup(frag+1);
		}
		if (gf_dash_is_m3u8(ctx->dash) || gf_dash_is_smooth_streaming(ctx->dash))
			ctx->is_dash = GF_FALSE;
		else
			ctx->is_dash = GF_TRUE;

		//we have a redirect URL on mpd pid, this means this comes from a service feeding the cache so we won't get any data on the pid.
		//request a process task
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_REDIRECT_URL);
		if (p && p->value.string) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, ctx->mpd_pid);
			gf_filter_pid_send_event(ctx->mpd_pid, &evt);
			gf_filter_post_process_task(filter);
		}
		return GF_OK;
	} else if (ctx->mpd_pid == pid) {
		return GF_OK;
	}

	//figure out group for this pid
	group_idx = dashdmx_group_idx_from_pid(ctx, pid);
	if (group_idx<0) {
		const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && strstr(DASHIN_MIMES, p->value.string)) return GF_REQUIRES_NEW_INSTANCE;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
		if (p && strstr(DASHIN_FILE_EXT, p->value.string)) return GF_REQUIRES_NEW_INSTANCE;

		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Failed to locate adaptation set for input pid\n"));
		return GF_SERVICE_ERROR;
	}
	group = gf_dash_get_group_udta(ctx->dash, group_idx);

	//initial configure
	opid = gf_filter_pid_get_udta(pid);
	if (opid == NULL) {
		u32 run_status;
		group = gf_dash_get_group_udta(ctx->dash, group_idx);
		assert(group);
		//for now we declare every component from the input source
		opid = dashdmx_create_output_pid(ctx, pid, &run_status, group);
		gf_filter_pid_set_udta(opid, group);
		gf_filter_pid_set_udta(pid, opid);
		//reste all flags
		gf_filter_pid_set_udta_flags(opid, 0);
		group->nb_pids ++;

		if (run_status) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
			gf_filter_pid_send_event(pid, &evt);
			group->is_playing = GF_TRUE;
			gf_dash_group_select(ctx->dash, group->idx, GF_TRUE);

			if (run_status==2) {
				gf_dash_set_group_done(ctx->dash, group->idx, GF_TRUE);
				gf_dash_group_select(ctx->dash, group->idx, GF_FALSE);

				GF_FEVT_INIT(evt, GF_FEVT_STOP, pid);
				gf_filter_pid_send_event(pid, &evt);
				group->is_playing = GF_FALSE;
			} else {
				is_period_switch = GF_TRUE;
			}
		}
	}
	dashdmx_declare_properties(ctx, group, group_idx, opid, pid, is_period_switch);


	//reset the file cache property (init segment could be cached but not the rest)
	gf_filter_pid_set_property(opid, GF_PROP_PID_FILE_CACHED, NULL);

	return GF_OK;
}

#ifdef GPAC_HAS_QJS

static s32 dashdmx_algo_js(void *udta, u32 group, u32 base_group, Bool force_lower_complexity, GF_DASHCustomAlgoInfo *stats)
{
	s32 res;
	JSValue ret, args[4];
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)udta;

	gf_js_lock(ctx->js_ctx, GF_TRUE);
	args[0] = JS_NewInt32(ctx->js_ctx, group);
	args[1] = JS_NewInt32(ctx->js_ctx, base_group);
	args[2] = JS_NewBool(ctx->js_ctx, force_lower_complexity);
	args[3] = JS_NewObject(ctx->js_ctx);
	JS_SetPropertyStr(ctx->js_ctx, args[3], "rate", JS_NewInt32(ctx->js_ctx, stats->download_rate) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "filesize", JS_NewInt32(ctx->js_ctx, stats->file_size) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "speed", JS_NewFloat64(ctx->js_ctx, stats->speed) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "max_speed", JS_NewFloat64(ctx->js_ctx, stats->max_available_speed) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "display_width", JS_NewInt32(ctx->js_ctx, stats->display_width) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "display_height", JS_NewInt32(ctx->js_ctx, stats->display_height) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "active_quality", JS_NewInt32(ctx->js_ctx, stats->active_quality_idx) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "buffer_min", JS_NewInt32(ctx->js_ctx, stats->buffer_min_ms) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "buffer_max", JS_NewInt32(ctx->js_ctx, stats->buffer_max_ms) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "buffer", JS_NewInt32(ctx->js_ctx, stats->buffer_occupancy_ms) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "degradation_hint", JS_NewInt32(ctx->js_ctx, stats->quality_degradation_hint) );
	JS_SetPropertyStr(ctx->js_ctx, args[3], "total_rate", JS_NewInt32(ctx->js_ctx, stats->total_rate) );
	ret = JS_Call(ctx->js_ctx, ctx->rate_fun, ctx->js_obj, 4, args);
	JS_FreeValue(ctx->js_ctx, args[3]);

	if (JS_IsException(ret)) {
		js_dump_error(ctx->js_ctx);
		res = -3;
	} else if (JS_ToInt32(ctx->js_ctx, &res, ret))
		res = -1;

	JS_FreeValue(ctx->js_ctx, ret);
	gf_js_lock(ctx->js_ctx, GF_FALSE);
	return res;
}

s32 dashdmx_download_monitor_js(void *udta, u32 group, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start, u32 buffer_dur_ms, u32 current_seg_dur)
{
	s32 res = -1;
	JSValue ret, args[2];
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)udta;

	gf_js_lock(ctx->js_ctx, GF_TRUE);

	args[0] = JS_NewInt32(ctx->js_ctx, group);
	args[1] = JS_NewObject(ctx->js_ctx);
	JS_SetPropertyStr(ctx->js_ctx, args[1], "bits_per_second", JS_NewInt32(ctx->js_ctx, bits_per_sec) );
	JS_SetPropertyStr(ctx->js_ctx, args[1], "total_bytes", JS_NewInt64(ctx->js_ctx, total_bytes) );
	JS_SetPropertyStr(ctx->js_ctx, args[1], "bytes_done", JS_NewInt64(ctx->js_ctx, bytes_done) );
	JS_SetPropertyStr(ctx->js_ctx, args[1], "us_since_start", JS_NewInt64(ctx->js_ctx, us_since_start) );
	JS_SetPropertyStr(ctx->js_ctx, args[1], "buffer_duration", JS_NewInt32(ctx->js_ctx, buffer_dur_ms) );
	JS_SetPropertyStr(ctx->js_ctx, args[1], "current_segment_duration", JS_NewInt32(ctx->js_ctx, current_seg_dur) );
	ret = JS_Call(ctx->js_ctx, ctx->download_fun, ctx->js_obj, 2, args);
	JS_FreeValue(ctx->js_ctx, args[1]);
	if (JS_ToInt32(ctx->js_ctx, &res, ret))
		res = -1;
	JS_FreeValue(ctx->js_ctx, ret);

	gf_js_lock(ctx->js_ctx, GF_FALSE);
	return res;
}
void dashdmx_cleanup_js(GF_DASHDmxCtx *ctx)
{
	if (ctx->js_ctx) {
		gf_js_lock(ctx->js_ctx, GF_TRUE);
		JS_FreeValue(ctx->js_ctx, ctx->rate_fun);
		JS_FreeValue(ctx->js_ctx, ctx->download_fun);
		JS_FreeValue(ctx->js_ctx, ctx->new_group_fun);
		JS_FreeValue(ctx->js_ctx, ctx->period_reset_fun);

		if (!ctx->owns_context)
			JS_FreeValue(ctx->js_ctx, ctx->js_obj);

		gf_js_lock(ctx->js_ctx, GF_FALSE);
		if (ctx->owns_context)
			gf_js_delete_context(ctx->js_ctx);
		ctx->js_ctx = NULL;
		ctx->owns_context = GF_FALSE;
		ctx->rate_fun = ctx->download_fun = ctx->new_group_fun= ctx->period_reset_fun = JS_UNDEFINED;
	}
}

#define GET_FUN(_field, _name) \
	dashctx->_field = JS_GetPropertyStr(ctx, dashctx->js_obj, _name); \
	if (! JS_IsFunction(ctx, dashctx->_field)) {\
		JS_FreeValue(dashctx->js_ctx, dashctx->_field); \
		dashctx->_field = JS_NULL;\
	}


JSValue dashdmx_bind_js(GF_Filter *f, JSContext *ctx, JSValueConst obj)
{
	GF_DASHDmxCtx *dashctx = (GF_DASHDmxCtx *) gf_filter_get_udta(f);
	JSValue rate_fun;

	rate_fun = JS_GetPropertyStr(ctx, obj, "rate_adaptation");
	if (! JS_IsFunction(ctx, rate_fun)) {
		JS_FreeValue(ctx, rate_fun);
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Object does not define a rate_adaptation function\n");
	}

	if (dashctx->js_ctx) {
		dashdmx_cleanup_js(dashctx);
	}
	dashctx->js_ctx = ctx;
	dashctx->js_obj = JS_DupValue(ctx, obj);
	dashctx->rate_fun = rate_fun;

	GET_FUN(download_fun, "download_monitor")
	GET_FUN(new_group_fun, "new_group")
	GET_FUN(period_reset_fun, "period_reset")

	if (JS_IsFunction(ctx, dashctx->download_fun)) {
		gf_dash_set_algo_custom(dashctx->dash, dashctx, dashdmx_algo_js, dashdmx_download_monitor_js);
		dashctx->abort = GF_TRUE;
	} else {
		gf_dash_set_algo_custom(dashctx->dash, dashctx, dashdmx_algo_js, NULL);
		dashctx->abort = GF_FALSE;
	}
	return JS_UNDEFINED;
}


static GF_Err dashdmx_initialize_js(GF_DASHDmxCtx *dashctx, char *jsfile)
{
    JSContext *ctx;
	JSValue global_obj, ret;
	u8 *buf;
	u32 buf_len;
	GF_Err e;
	u32 flags = JS_EVAL_TYPE_GLOBAL;

	e = gf_file_load_data(jsfile, &buf, &buf_len);
	if (e) return e;

	ctx = gf_js_create_context();
	if (!ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DASHDmx] Failed to load QuickJS context\n"));
		if (buf) gf_free(buf);
		return GF_IO_ERR;
	}
	JS_SetContextOpaque(ctx, dashctx);
	dashctx->owns_context = GF_TRUE;

    global_obj = JS_GetGlobalObject(ctx);
	js_load_constants(ctx, global_obj);
	dashctx->js_ctx = ctx;

	JS_SetPropertyStr(dashctx->js_ctx, global_obj, "_gpac_log_name", JS_NewString(dashctx->js_ctx, gf_file_basename(jsfile) ) );
	dashctx->js_obj = JS_NewObject(dashctx->js_ctx);
	JS_SetPropertyStr(dashctx->js_ctx, global_obj, "dashin", dashctx->js_obj);

 	if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((char *)buf, buf_len)) {
 		//init modules, except webgl
		qjs_init_all_modules(dashctx->js_ctx, GF_TRUE, GF_FALSE);
		flags = JS_EVAL_TYPE_MODULE;
	}

	ret = JS_Eval(dashctx->js_ctx, (char *)buf, buf_len, jsfile, flags);
	gf_free(buf);

	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DASHDmx] Error loading script %s\n", jsfile));
        js_dump_error(dashctx->js_ctx);
		JS_FreeValue(dashctx->js_ctx, ret);
		JS_FreeValue(dashctx->js_ctx, global_obj);
		return GF_BAD_PARAM;
	}
	JS_FreeValue(dashctx->js_ctx, ret);
    JS_FreeValue(dashctx->js_ctx, global_obj);

	dashctx->rate_fun = JS_GetPropertyStr(ctx, dashctx->js_obj, "rate_adaptation");
	if (! JS_IsFunction(ctx, dashctx->rate_fun)) {
		JS_FreeValue(dashctx->js_ctx, dashctx->rate_fun);
		dashctx->rate_fun = JS_UNDEFINED;
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DASHDmx] JS file does not define a rate_adaptation function in dasher object\n"));
		return GF_BAD_PARAM;
	}


	GET_FUN(download_fun, "download_monitor")
	GET_FUN(new_group_fun, "new_group")
	GET_FUN(period_reset_fun, "period_reset")
	return GF_OK;
}

#endif

static const GF_FilterCapability DASHDmxFileModeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mpd|m3u8|3gm|ism"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/dash+xml|video/vnd.3gpp.mpd|audio/vnd.3gpp.mpd|video/vnd.mpeg.dash.mpd|audio/vnd.mpeg.dash.mpd|audio/mpegurl|video/mpegurl|application/vnd.ms-sstr+xml|application/x-mpegURL|application/vnd.apple.mpegURL"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//accept only file streams and produce them
	{ .code=GF_PROP_PID_STREAM_TYPE, .val.type=GF_PROP_UINT, .val.value.uint=GF_STREAM_FILE, .flags=(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_LOADED_FILTER) },
};

static GF_Err dashdmx_initialize(GF_Filter *filter)
{
	u32 timeshift;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	GF_DASHAdaptationAlgorithm algo = GF_DASH_ALGO_NONE;
	const char *algo_str;
	ctx->filter = filter;
	ctx->dm = gf_filter_get_download_manager(filter);
	if (!ctx->dm) return GF_SERVICE_ERROR;

	//old syntax
	if (ctx->filemode) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] `filemode` option will soon be deprecated, update your script to use `:forward=file` option.\n"));
		ctx->forward = DFWD_FILE;
		ctx->filemode = GF_FALSE;
	}

	ctx->dash_io.udta = ctx;
	ctx->dash_io.delete_cache_file = dashdmx_io_delete_cache_file;
	ctx->dash_io.create = dashdmx_io_create;
	ctx->dash_io.del = dashdmx_io_del;
	ctx->dash_io.init = dashdmx_io_init;
	ctx->dash_io.run = dashdmx_io_run;
	ctx->dash_io.get_url = dashdmx_io_get_url;
	ctx->dash_io.get_cache_name = dashdmx_io_get_cache_name;
	ctx->dash_io.get_mime = dashdmx_io_get_mime;
	ctx->dash_io.get_header_value = dashdmx_io_get_header_value;
	ctx->dash_io.get_utc_start_time = dashdmx_io_get_utc_start_time;
	ctx->dash_io.setup_from_url = dashdmx_io_setup_from_url;
	ctx->dash_io.set_range = dashdmx_io_set_range;
	if ((ctx->forward==DFWD_FILE) || (ctx->forward==DFWD_SBOUND_MANIFEST))
		ctx->dash_io.manifest_updated = dashdmx_io_manifest_updated;

	ctx->dash_io.get_bytes_per_sec = dashdmx_io_get_bytes_per_sec;

#if 0 //unused since we are in non threaded mode
	ctx->dash_io.abort = dashdmx_io_abort;
	ctx->dash_io.get_total_size = dashdmx_io_get_total_size;
	ctx->dash_io.get_bytes_done = dashdmx_io_get_bytes_done;
#endif

	ctx->dash_io.on_dash_event = dashdmx_io_on_dash_event;

	if (ctx->init_timeshift<0) {
		timeshift = -ctx->init_timeshift;
		if (timeshift>100) timeshift = 100;
	} else {
		timeshift = ctx->init_timeshift;
	}

	algo_str = ctx->algo;

	if (ctx->forward > DFWD_FILE) {
		ctx->split_as = GF_TRUE;
		ctx->screen_res = GF_FALSE;
		algo_str = "none";
		ctx->auto_switch = 0;
		ctx->noseek = GF_TRUE;
	}

	if (!strcmp(algo_str, "none")) algo = GF_DASH_ALGO_NONE;
	else if (!strcmp(algo_str, "grate")) algo = GF_DASH_ALGO_GPAC_LEGACY_RATE;
	else if (!strcmp(algo_str, "gbuf")) algo = GF_DASH_ALGO_GPAC_LEGACY_BUFFER;
	else if (!strcmp(algo_str, "bba0")) algo = GF_DASH_ALGO_BBA0;
	else if (!strcmp(algo_str, "bolaf")) algo = GF_DASH_ALGO_BOLA_FINITE;
	else if (!strcmp(algo_str, "bolab")) algo = GF_DASH_ALGO_BOLA_BASIC;
	else if (!strcmp(algo_str, "bolau")) algo = GF_DASH_ALGO_BOLA_U;
	else if (!strcmp(algo_str, "bolao")) algo = GF_DASH_ALGO_BOLA_O;
	else {
#ifndef GPAC_HAS_QJS
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] No JS support, cannot use custom algo %s\n", algo_str));
		return GF_BAD_PARAM;
#else
		char szFile[GF_MAX_PATH];
		Bool found = GF_FALSE;
		GF_Err e;
		//init to default, overwrite later
		algo = GF_DASH_ALGO_GPAC_LEGACY_BUFFER;
		if (gf_file_exists(algo_str)) {
			strcpy(szFile, algo_str);
			found = GF_TRUE;
		} else if (!strstr(algo_str, ".js")) {
			strcpy(szFile, algo_str);
			strcat(szFile, ".js");
			if (gf_file_exists(szFile)) {
				found = GF_TRUE;
			}
		}
		if (!found) {
			gf_opts_default_shared_directory(szFile);
			strcat(szFile, "/scripts/");
			strcat(szFile, algo_str);
			if (gf_file_exists(szFile)) {
				found = GF_TRUE;
			} else if (!strstr(algo_str, ".js")) {
				strcat(szFile, ".js");
				if (gf_file_exists(szFile))
					found = GF_TRUE;
			}
		}
		if (!found) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Custom algo %s not found\n", algo_str));
			return GF_BAD_PARAM;
		}
		e = dashdmx_initialize_js(ctx, szFile);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Failed to setup custom algo %s\n", algo_str));
			return e;
		}
#endif
	}


	ctx->dash = gf_dash_new(&ctx->dash_io, 0, 0, (ctx->segstore==2) ? GF_TRUE : GF_FALSE, (algo==GF_DASH_ALGO_NONE) ? GF_TRUE : GF_FALSE, ctx->start_with, timeshift);

	if (!ctx->dash) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error - cannot create DASH Client\n"));
		return GF_IO_ERR;
	}

	if (ctx->screen_res) {
		GF_FilterSessionCaps caps;
		gf_filter_get_session_caps(ctx->filter, &caps);
		gf_dash_set_max_resolution(ctx->dash, caps.max_screen_width, caps.max_screen_height, caps.max_screen_bpp);
	}

	gf_dash_set_algo(ctx->dash, algo);
	gf_dash_set_utc_shift(ctx->dash, ctx->shift_utc);
	gf_dash_set_suggested_presentation_delay(ctx->dash, ctx->spd);
	gf_dash_set_route_ast_shift(ctx->dash, ctx->route_shift);
	gf_dash_enable_utc_drift_compensation(ctx->dash, ctx->server_utc);
	gf_dash_set_tile_adaptation_mode(ctx->dash, ctx->tile_mode, ctx->tiles_rate);

	gf_dash_set_min_timeout_between_404(ctx->dash, ctx->delay40X);
	gf_dash_set_segment_expiration_threshold(ctx->dash, ctx->exp_threshold);
	gf_dash_set_switching_probe_count(ctx->dash, ctx->switch_count);
	gf_dash_set_agressive_adaptation(ctx->dash, ctx->aggressive);
	gf_dash_enable_single_range_llhls(ctx->dash, ctx->llhls_merge);
	gf_dash_debug_groups(ctx->dash, ctx->debug_as.vals, ctx->debug_as.nb_items);
	gf_dash_disable_speed_adaptation(ctx->dash, !ctx->speedadapt);
	gf_dash_ignore_xlink(ctx->dash, ctx->noxlink);
	gf_dash_set_period_xlink_query_string(ctx->dash, ctx->query);
	gf_dash_set_low_latency_mode(ctx->dash, ctx->lowlat);
	if (ctx->split_as)
		gf_dash_split_adaptation_sets(ctx->dash);
	gf_dash_disable_low_quality_tiles(ctx->dash, ctx->skip_lqt);
	gf_dash_set_chaining_mode(ctx->dash, ctx->chain_mode);
	gf_dash_set_auto_switch(ctx->dash, ctx->auto_switch, ctx->asloop);

	//in test mode, we disable seeking inside the segment: this initial seek range is dependent from tune-in time and would lead to different start range
	//at each run, possibly breaking all tests
	if (gf_sys_is_test_mode())
		ctx->noseek = GF_TRUE;

	ctx->initial_play = GF_TRUE;
	gf_filter_block_eos(filter, GF_TRUE);

#ifdef GPAC_HAS_QJS
	if (ctx->js_ctx) {
		if (JS_IsFunction(ctx->js_ctx, ctx->download_fun)) {
			gf_dash_set_algo_custom(ctx->dash, ctx, dashdmx_algo_js, dashdmx_download_monitor_js);
			ctx->abort = GF_TRUE;
		} else {
			gf_dash_set_algo_custom(ctx->dash, ctx, dashdmx_algo_js, NULL);
			ctx->abort = GF_FALSE;
		}
	}
#endif

	if (ctx->forward==DFWD_FILE) {
		ctx->segstore = 0;
		gf_filter_override_caps(filter, DASHDmxFileModeCaps, GF_ARRAY_LENGTH(DASHDmxFileModeCaps) );
	}

	//for coverage
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		dashdmx_on_filter_setup_error(NULL, NULL, GF_OK);
	}
#endif

	//we are blocking in live mode for manifest update 
	gf_filter_set_blocking(filter, GF_TRUE);

	return GF_OK;
}

static void dashdmx_finalize(GF_Filter *filter)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	assert(ctx);

	if (ctx->dash)
		gf_dash_del(ctx->dash);

	if (ctx->frag_url)
		gf_free(ctx->frag_url);

	if (ctx->manifest_payload)
		gf_free(ctx->manifest_payload);

	if (ctx->hls_variants) {
		while (gf_list_count(ctx->hls_variants))
			gf_free(gf_list_pop_back(ctx->hls_variants));
		gf_list_del(ctx->hls_variants);
	}
	if (ctx->hls_variants_names) {
		while (gf_list_count(ctx->hls_variants_names))
			gf_free(gf_list_pop_back(ctx->hls_variants_names));
		gf_list_del(ctx->hls_variants_names);
	}

#ifdef GPAC_HAS_QJS
	dashdmx_cleanup_js(ctx);
#endif
}

static Bool dashdmx_process_event(GF_Filter *filter, const GF_FilterEvent *fevt)
{
	u32 i, count;
	GF_FilterEvent src_evt;
	GF_FilterPid *ipid;
	Bool initial_play;
	Double offset;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	GF_DASHGroup *group;


	switch (fevt->base.type) {
	case GF_FEVT_QUALITY_SWITCH:
		if (fevt->quality_switch.set_tile_mode_plus_one) {
			GF_DASHTileAdaptationMode tile_mode = fevt->quality_switch.set_tile_mode_plus_one - 1;
			gf_dash_set_tile_adaptation_mode(ctx->dash, tile_mode, 100);
		} else if (fevt->quality_switch.q_idx < 0) {
			gf_dash_set_automatic_switching(ctx->dash, 1);
		} else if (fevt->base.on_pid) {
			s32 idx;
			group = gf_filter_pid_get_udta(fevt->base.on_pid);
			if (!group) return GF_TRUE;
			idx = group->idx;

			gf_dash_group_set_quality_degradation_hint(ctx->dash, group->idx, fevt->quality_switch.quality_degradation);

			if (fevt->quality_switch.dependent_group_index) {
				if (fevt->quality_switch.dependent_group_index > gf_dash_group_get_num_groups_depending_on(ctx->dash, group->idx))
					return GF_TRUE;

				idx = gf_dash_get_dependent_group_index(ctx->dash, group->idx, fevt->quality_switch.dependent_group_index-1);
				if (idx==-1) return GF_TRUE;
			}

			gf_dash_set_automatic_switching(ctx->dash, 0);
			gf_dash_group_select_quality(ctx->dash, idx, NULL, fevt->quality_switch.q_idx);
		} else {
			gf_dash_switch_quality(ctx->dash, fevt->quality_switch.up);
		}
		return GF_TRUE;

	case GF_FEVT_NTP_REF:
		gf_dash_override_ntp(ctx->dash, fevt->ntp.ntp);
		return GF_TRUE;

	case GF_FEVT_FILE_DELETE:
		//check if we want that in other forward mode
		if (ctx->forward==DFWD_FILE) {
			for (i=0; i<gf_dash_get_group_count(ctx->dash); i++) {
				group = gf_dash_get_group_udta(ctx->dash, i);
				if (!group || !group->template) continue;
				
				if (!strncmp(group->template, fevt->file_del.url, strlen(group->template) )) {
					GF_FilterPid *pid = dashdmx_opid_from_group(ctx, group);
					if (pid) {
						GF_FilterEvent evt;
						GF_FEVT_INIT(evt, GF_FEVT_FILE_DELETE, pid);
						evt.file_del.url = fevt->file_del.url;
						gf_filter_pid_send_event(pid, &evt);
					}
					return GF_TRUE;
				}
			}
		}
		return GF_TRUE;
	default:
		break;
	}

	/*not supported*/
	if (!fevt->base.on_pid) return GF_TRUE;

	if (fevt->base.on_pid == ctx->output_mpd_pid) {
		return GF_TRUE;
	}
	group = gf_filter_pid_get_udta(fevt->base.on_pid);
	if (!group) return GF_TRUE;
	count = gf_filter_get_ipid_count(filter);
	ipid = NULL;
	for (i=0; i<count; i++) {
		ipid = gf_filter_get_ipid(filter, i);
		if (gf_filter_pid_get_udta(ipid) == fevt->base.on_pid) break;
		ipid = NULL;
	}

	switch (fevt->base.type) {
	case GF_FEVT_VISIBILITY_HINT:
		group = gf_filter_pid_get_udta(fevt->base.on_pid);
		if (!group) return GF_TRUE;

		gf_dash_group_set_visible_rect(ctx->dash, group->idx, fevt->visibility_hint.min_x, fevt->visibility_hint.max_x, fevt->visibility_hint.min_y, fevt->visibility_hint.max_y, fevt->visibility_hint.is_gaze);
		return GF_TRUE;

	case GF_FEVT_PLAY:
		src_evt = *fevt;
		group->is_playing = GF_TRUE;
		ctx->check_eos = GF_FALSE;

		//adjust play range from media timestamps to MPD time
		if (fevt->play.timestamp_based) {

			if (fevt->play.timestamp_based==1) {
				u64 pto;
				u32 timescale;
				gf_dash_group_get_presentation_time_offset(ctx->dash, group->idx, &pto, &timescale);
				offset = (Double) pto;
				offset /= timescale;
				src_evt.play.start_range -= offset;
				if (src_evt.play.start_range < 0) src_evt.play.start_range = 0;
			}

			group->is_timestamp_based = 1;
			group->pto_setup = 0;
			ctx->media_start_range = fevt->play.start_range;
		} else {
			group->is_timestamp_based = 0;
			group->pto_setup = 0;
			if (fevt->play.start_range<0) src_evt.play.start_range = 0;
			//in m3u8, we need also media start time for mapping time
			if (gf_dash_is_m3u8(ctx->dash))
				ctx->media_start_range = fevt->play.start_range;
		}

		//we cannot handle seek request outside of a period being setup, this messes up our internal service setup
		//we postpone the seek and will request it later on ...
		if (gf_dash_in_period_setup(ctx->dash)) {
			u64 p_end = gf_dash_get_period_duration(ctx->dash);
			if (p_end) {
				p_end += gf_dash_get_period_start(ctx->dash);
				if (p_end<1000*fevt->play.start_range) {
					ctx->seek_request = fevt->play.start_range;
					return GF_TRUE;
				}
			}
		}

		if (fevt->play.speed)
			gf_dash_set_speed(ctx->dash, fevt->play.speed);

		initial_play = ctx->initial_play;
		if (fevt->play.initial_broadcast_play==1)
			initial_play = GF_TRUE;

		/*don't seek if this command is the first PLAY request of objects declared by the subservice, unless start range is not default one (0) */
		if (!ctx->nb_playing) {
			if (!initial_play || (fevt->play.start_range>1.0)) {

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Received Play command on group %d\n", group->idx));

				if (fevt->play.end_range<=0) {
					u32 ms = (u32) ( 1000 * (-fevt->play.end_range) );
					if (ms<1000) ms = 0;
					gf_dash_set_timeshift(ctx->dash, ms);
				}
				gf_dash_seek(ctx->dash, fevt->play.start_range);

				//to remove once we manage to keep the service alive
				/*don't forward commands if a switch of period is to be scheduled, we are killing the service anyway ...*/
				if (gf_dash_get_period_switch_status(ctx->dash)) {
					ctx->nb_playing++;
					return GF_TRUE;
				}
			}
		}
		//otherwise in static mode, perform a group seek
		else if (!initial_play && !gf_dash_is_dynamic_mpd(ctx->dash) ) {
			/*don't forward commands if a switch of period is to be scheduled, we are killing the service anyway ...*/
			if (gf_dash_get_period_switch_status(ctx->dash)) {
				ctx->nb_playing++;
				return GF_TRUE;
			}
			//seek on a single group

			gf_dash_group_seek(ctx->dash, group->idx, fevt->play.start_range);
		}

		//check if current segment playback should be aborted
		src_evt.play.forced_dash_segment_switch = gf_dash_group_segment_switch_forced(ctx->dash, group->idx);

		gf_dash_group_select(ctx->dash, group->idx, GF_TRUE);
		gf_dash_set_group_done(ctx->dash, (u32) group->idx, 0);

		//adjust start range from MPD time to media time
		if (gf_dash_is_dynamic_mpd(ctx->dash) && ctx->noseek) {
			src_evt.play.start_range=0;
		} else {
			u64 pto;
			u32 timescale;
			src_evt.play.start_range = gf_dash_group_get_start_range(ctx->dash, group->idx);
			gf_dash_group_get_presentation_time_offset(ctx->dash, group->idx, &pto, &timescale);
			src_evt.play.start_range += ((Double)pto) / timescale;
		}
		src_evt.play.no_byterange_forward = 1;
		dashdmx_setup_buffer(ctx, group);

		gf_filter_prevent_blocking(filter, GF_TRUE);
		if (group->nb_pending)
			group->nb_pending--;

		ctx->nb_playing++;
		//forward new event to source pid
		src_evt.base.on_pid = ipid;

		gf_filter_pid_send_event(ipid, &src_evt);
		gf_filter_post_process_task(filter);
		//cancel the event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//there is still pending PIDs for this group, don't stop the group until we get a final stop
		//this happens when source is muxed content and one or more pids are not used by the chain
		if (group->nb_pending)
			return GF_TRUE;

		gf_dash_set_group_done(ctx->dash, (u32) group->idx, 1);
		gf_dash_group_select(ctx->dash, (u32) group->idx, GF_FALSE);
		//group was playing, force stop on source filter (to discard any non-processed data)
		//we need this because SOURCE_SWITCH event is sent directly on source filter
		if (group->is_playing) {
			src_evt = *fevt;
			src_evt.base.on_pid = NULL;
			gf_filter_send_event(group->seg_filter_src, &src_evt, GF_FALSE);
		}
		group->is_playing = GF_FALSE;
		group->prev_is_init_segment = GF_FALSE;
		if (ctx->nb_playing) {
			ctx->initial_play = GF_FALSE;
			group->force_seg_switch = GF_TRUE;
			ctx->nb_playing--;
			if (!ctx->nb_playing) ctx->check_eos = GF_TRUE;
		}
		//forward new event to source pid
		src_evt = *fevt;
		src_evt.base.on_pid = ipid;
		//send a stop but indicate source should not receive it
		src_evt.play.forced_dash_segment_switch = GF_TRUE;
		gf_filter_pid_send_event(ipid, &src_evt);

		//cancel the event
		return GF_TRUE;
	case GF_FEVT_SET_SPEED:
		gf_dash_set_speed(ctx->dash, fevt->play.speed);
		return GF_FALSE;

	case GF_FEVT_CAPS_CHANGE:
		if (ctx->screen_res) {
			GF_FilterSessionCaps caps;
			gf_filter_get_session_caps(ctx->filter, &caps);
			gf_dash_set_max_resolution(ctx->dash, caps.max_screen_width, caps.max_screen_height, caps.max_screen_bpp);
		}
		return GF_TRUE;
	case GF_FEVT_INFO_UPDATE:
		//propagate
		return GF_FALSE;
	default:
		break;
	}

	//by default cancel all events
	return GF_TRUE;
}


static void dashdmx_notify_group_quality(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 i;
	if (!group->notify_quality_change) return;
	group->notify_quality_change = GF_FALSE;

	for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
		s32 sel = -1;
		GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
		if (gf_filter_pid_get_udta(opid) != group) continue;

		sel = gf_dash_group_get_active_quality(ctx->dash, group->idx);
		if (!gf_sys_is_test_mode() || (ctx->forward==DFWD_FILE)) {
			if (sel>=0) {
				gf_filter_pid_set_property_str(opid, "has:selected", &PROP_UINT(sel) );
			}
			gf_filter_pid_set_property_str(opid, "has:auto", &PROP_UINT(gf_dash_get_automatic_switching(ctx->dash) ) );
			gf_filter_pid_set_property_str(opid, "has:tilemode", &PROP_UINT(gf_dash_get_tile_adaptation_mode(ctx->dash) ) );

			if (group->nb_group_deps) {
				u32 k;
				GF_PropertyValue deps_sel;

				memset(&deps_sel, 0, sizeof(GF_PropertyValue));
				deps_sel.type = GF_PROP_SINT_LIST;
				deps_sel.value.sint_list.nb_items = group->nb_group_deps;
				deps_sel.value.sint_list.vals = gf_malloc(sizeof(char *) * group->nb_group_deps);

				for (k=0; k<group->nb_group_deps; k++) {
					u32 g_idx = gf_dash_get_dependent_group_index(ctx->dash, group->idx, k);
					sel = gf_dash_group_get_active_quality(ctx->dash, g_idx);
					deps_sel.value.sint_list.vals[k] = sel;
				}
				gf_filter_pid_set_property_str(opid, "has:deps_selected", &deps_sel);
				gf_free(deps_sel.value.sint_list.vals);
			}
		}

		//setup some info for consuming filters
		if (ctx->forward) {
			GF_DASHQualityInfo q;
			const char *init_seg = NULL;
			const char *hls_variant = NULL;
			const char *dash_url = gf_dash_get_url(ctx->dash);
			u32 segment_timeline_timescale = 0;

			//we dispatch timing in milliseconds
			if (ctx->forward==DFWD_FILE) {
				gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000));
			}

			if (gf_dash_group_get_quality_info(ctx->dash, group->idx, sel, &q)==GF_OK) {
				if (q.bandwidth)
					gf_filter_pid_set_property(opid, GF_PROP_PID_BITRATE, &PROP_UINT(q.bandwidth));
				if (q.codec)
					gf_filter_pid_set_property(opid, GF_PROP_PID_CODEC, &PROP_STRING(q.codec));
			}
			if (group->template) gf_free(group->template);
			group->template = gf_dash_group_get_template(ctx->dash, group->idx, &segment_timeline_timescale, &init_seg, &hls_variant);
			if (!group->template) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] Cannot extract template string for %s\n", dash_url ));
				gf_filter_pid_set_property(opid, GF_PROP_PID_TEMPLATE, NULL);
			} else {
				gf_filter_pid_set_property(opid, GF_PROP_PID_TEMPLATE, &PROP_STRING(group->template) );
				char *sep = strchr(group->template, '$');
				if (sep) sep[0] = 0;
				gf_filter_pid_set_property_str(opid, "source_template", &PROP_BOOL(GF_TRUE) );
				gf_filter_pid_set_property_str(opid, "stl_timescale", segment_timeline_timescale ? &PROP_UINT(segment_timeline_timescale) : NULL);
				gf_filter_pid_set_property_str(opid, "init_url", init_seg ? &PROP_STRING(init_seg) : NULL);
				gf_filter_pid_set_property_str(opid, "manifest_url", dash_url ? &PROP_STRING(dash_url) : NULL);
				gf_filter_pid_set_property_str(opid, "hls_variant_name", hls_variant ? &PROP_STRING(hls_variant) : NULL);
			}
			gf_filter_pid_set_property(opid, GF_PROP_PID_REP_ID, &PROP_STRING(q.ID) );

			//file forward mode, we may need a dash duration later in the chain (route mux for ex), signal what we know
			//from manifest
			if (ctx->forward==DFWD_FILE) {
				u32 dur, timescale;

				gf_dash_group_get_segment_duration(ctx->dash, group->idx, &dur, &timescale);
				gf_filter_pid_set_property(opid, GF_PROP_PID_DASH_DUR, &PROP_FRAC_INT(dur, timescale) );
			}
		}
	}
}
GF_Err gf_dash_group_push_tfrf(GF_DashClient *dash, u32 idx, void *tfrf, u32 timescale);

static void dashdmx_update_group_stats(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 bytes_per_sec = 0;
	u64 file_size = 0;
	u32 dep_rep_idx;
	const GF_PropertyValue *p;
	GF_PropertyEntry *pe=NULL;
	Bool broadcast_flag = GF_FALSE;
	if (group->stats_uploaded) return;
	if (group->prev_is_init_segment) return;
	if (!group->seg_filter_src) return;

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_FILE_CACHED, &pe);
	if (!p || !p->value.boolean) {
		u64 us_since_start = 0;
		u64 down_bytes = 0;
		u32 bits_per_sec = 0;
		u32 now = gf_sys_clock();

		if (!ctx->abort || (now - group->last_bw_check < ctx->bwcheck)) {
			gf_filter_release_property(pe);
			return;
		}
		group->last_bw_check = now;
		//we allow file abort, check the download

		p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_RATE, &pe);
		if (p) bits_per_sec = p->value.uint;

		p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_SIZE, &pe);
		if (p) file_size = p->value.longuint;

		p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_BYTES, &pe);
		if (p) down_bytes = p->value.longuint;

		us_since_start = gf_sys_clock_high_res() - group->us_at_seg_start;
		gf_dash_group_check_bandwidth(ctx->dash, group->idx, bits_per_sec, file_size, down_bytes, us_since_start);

		gf_filter_release_property(pe);
		return;
	}
	group->stats_uploaded = GF_TRUE;

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_RATE, &pe);
	if (p) bytes_per_sec = p->value.uint / 8;

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_SIZE, &pe);
	if (p) file_size = p->value.longuint;

	p = gf_filter_get_info_str(group->seg_filter_src, "x-route", &pe);
	if (p && p->value.string && !strcmp(p->value.string, "yes")) {
		broadcast_flag = GF_TRUE;
	}
	if (group->nb_group_deps)
		dep_rep_idx = group->current_group_dep ? (group->current_group_dep-1) : group->nb_group_deps;
	else
		dep_rep_idx = group->current_dependent_rep_idx;

	gf_dash_group_store_stats(ctx->dash, group->idx, dep_rep_idx, bytes_per_sec, file_size, broadcast_flag, gf_sys_clock_high_res() - group->us_at_seg_start);

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_FILE_CACHED, &pe);
	if (p && p->value.boolean)
		group->stats_uploaded = GF_TRUE;

	gf_filter_release_property(pe);
}

static void dashdmx_switch_segment(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 dependent_representation_index;
	GF_Err e;
	Bool has_scalable_next;
	Bool seg_disabled;
	GF_FilterEvent evt;
	const char *next_url, *next_url_init_or_switch_segment, *src_url, *key_url;
	u64 start_range, end_range, switch_start_range, switch_end_range;
	bin128 key_IV;
	u32 group_idx;

	//for smooth if prev is init segment itis not connected to the real httpin yet...
	if (group->prev_is_init_segment && gf_dash_is_smooth_streaming(ctx->dash)) {

	} else {
		group->init_ok = GF_TRUE;
	}
	dashdmx_notify_group_quality(ctx, group);

fetch_next:
	assert(group->nb_eos || group->seg_was_not_ready || group->in_error);
	group->wait_for_pck = GF_TRUE;
	group->in_error = GF_FALSE;
	if (group->segment_sent) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] group %d drop current segment\n", group->idx));
		if (!group->current_group_dep && !group->next_dependent_rep_idx) {
			gf_dash_group_discard_segment(ctx->dash, group->idx);

			//special case for group dependencies (tiling): signal segment switch
			//so that tileagg may detect losses
			if (group->nb_group_deps) {
				GF_FilterPid *opid = dashdmx_opid_from_group(ctx, group);
				if (opid) {
					GF_FEVT_INIT(evt, GF_FEVT_PLAY_HINT, opid);
					evt.play.forced_dash_segment_switch = GF_TRUE;
					gf_filter_pid_send_event(opid, &evt);
				}
			}
			if (group->seg_discard_state==1)
				group->seg_discard_state = 2;
		}

		group->segment_sent = GF_FALSE;
		//no thread mode, we work with at most one entry in cache, call process right away to get the group next URL ready
		gf_dash_process(ctx->dash);
	}

#if 0
	if (group_done) {
		if (!gf_dash_get_period_switch_status(ctx->dash) && gf_dash_in_last_period(ctx->dash, GF_TRUE)) {
			return;
		}
	}
#endif

	//special case if we had a discard of a dependent seg: we wait for the output PIDs to be flushed
	//so that we don't send a GF_FEVT_PLAY_HINT event before the previous segment is completely produced
	//this is mostly for tileagg for the time being and could need further refinements
	if (group->seg_discard_state == 2) {
		u32 i;
		for (i=0; i < gf_filter_get_opid_count(ctx->filter); i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i );
			GF_DASHGroup *g = gf_filter_pid_get_udta( opid );
			if (g != group) continue;
			if (gf_filter_pid_would_block(opid)) {
				group->seg_was_not_ready = GF_TRUE;
				group->stats_uploaded = GF_TRUE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] some pids blocked on group with discard, waiting before fetching next segment\n"));
				return;
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] all pids unblocked on group with discard, fetching next segment\n"));
		group->seg_discard_state = 0;
	}


	dependent_representation_index = 0;
	group_idx = group->idx;
	if (group->nb_group_deps) {
		dependent_representation_index = group->current_group_dep;
	} else if (group->next_dependent_rep_idx) {
		dashdmx_update_group_stats(ctx, group);
		dependent_representation_index = group->current_dependent_rep_idx = group->next_dependent_rep_idx;
	} else if (group->current_dependent_rep_idx) {
		dashdmx_update_group_stats(ctx, group);
		group->current_dependent_rep_idx = 0;
	}

	group->stats_uploaded = GF_FALSE;

	e = gf_dash_group_get_next_segment_location(ctx->dash, group_idx, dependent_representation_index, &next_url, &start_range, &end_range,
		        NULL, &next_url_init_or_switch_segment, &switch_start_range , &switch_end_range,
		        &src_url, &has_scalable_next, &key_url, &key_IV, &group->utc_map);

	if (e == GF_EOS) {
		group->eos_detected = GF_TRUE;
		return;
	}
	seg_disabled = GF_FALSE;
	group->eos_detected = GF_FALSE;
	if (e == GF_URL_REMOVED) {
		seg_disabled = GF_TRUE;
		e = GF_OK;
	}

	if (e != GF_OK) {
		if (e == GF_BUFFER_TOO_SMALL) {
			group->seg_was_not_ready = GF_TRUE;
			group->stats_uploaded = GF_TRUE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] group %d next segment name not known yet!\n", group->idx));
			gf_filter_ask_rt_reschedule(ctx->filter, 10000);
//			gf_filter_post_process_task(ctx->filter);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] group %d error fetching next segment name: %s\n", group->idx, gf_error_to_string(e) ));
		}
		return;
	}

	if (!has_scalable_next) {
		group->next_dependent_rep_idx = 0;
	} else {
		group->next_dependent_rep_idx++;
	}

	if (group->nb_group_deps) {
		group->current_group_dep++;
		if (group->current_group_dep>group->nb_group_deps)
			group->current_group_dep = 0;
	}

	assert(next_url);
	group->seg_was_not_ready = GF_FALSE;

	if (next_url_init_or_switch_segment && !group->init_switch_seg_sent) {
		if (group->in_is_cryptfile) {
#ifndef GPAC_DISABLE_CRYPTO
			gf_cryptfin_set_kms(group->seg_filter_src, key_url, key_IV);
#endif
		}
		GF_FEVT_INIT(evt, GF_FEVT_SOURCE_SWITCH,  NULL);
		evt.seek.start_offset = switch_start_range;
		evt.seek.end_offset = switch_end_range;
		evt.seek.source_switch = next_url_init_or_switch_segment;
		evt.seek.is_init_segment = GF_TRUE;
		evt.seek.skip_cache_expiration = GF_TRUE;
		if (ctx->forward) {
			if (group->current_url) gf_free(group->current_url);
			group->current_url = gf_strdup(next_url_init_or_switch_segment);
		}

		group->prev_is_init_segment = GF_TRUE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] group %d queuing next init/switching segment %s\n", group->idx, next_url_init_or_switch_segment));

		group->signal_seg_name = (ctx->forward==DFWD_FILE) ? GF_TRUE : GF_FALSE;
		group->init_switch_seg_sent = GF_TRUE;
		gf_filter_send_event(group->seg_filter_src, &evt, GF_FALSE);
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] group %d queuing next media segment %s\n", group->idx, next_url));

	group->segment_sent = GF_TRUE;
	group->prev_is_init_segment = GF_FALSE;
	group->init_switch_seg_sent = GF_FALSE;
	group->signal_seg_name = (ctx->forward==DFWD_FILE) ? GF_TRUE : GF_FALSE;
	group->us_at_seg_start = gf_sys_clock_high_res();

	if (seg_disabled) {
		u32 dep_rep_idx;
		if (group->nb_group_deps)
			dep_rep_idx = group->current_group_dep ? (group->current_group_dep-1) : group->nb_group_deps;
		else
			dep_rep_idx = group->current_dependent_rep_idx;

		gf_dash_group_store_stats(ctx->dash, group->idx, dep_rep_idx, 0, 0, 0, 0);
		if (!group->seg_discard_state)
			group->seg_discard_state = 1;
		goto fetch_next;
	}

	if (group->in_is_cryptfile) {
#ifndef GPAC_DISABLE_CRYPTO
		gf_cryptfin_set_kms(group->seg_filter_src, key_url, key_IV);
#endif
	}

	if (ctx->forward) {
		if (group->current_url && strcmp(group->current_url, next_url)) {
			gf_free(group->current_url);
			group->current_url = NULL;
		}
		if (!group->current_url) {
			group->current_url = gf_strdup(next_url);
			group->url_changed = GF_TRUE;
		}
	}

	GF_FEVT_INIT(evt, GF_FEVT_SOURCE_SWITCH, NULL);
	evt.seek.source_switch = next_url;
	evt.seek.start_offset = start_range;
	evt.seek.end_offset = end_range;
	evt.seek.is_init_segment = GF_FALSE;
	gf_filter_send_event(group->seg_filter_src, &evt, GF_FALSE);
}

static GF_Err dashin_abort(GF_DASHDmxCtx *ctx)
{
	u32 i;
	if (!ctx || ctx->in_error) return GF_EOS;
	
	for (i=0; i<gf_filter_get_ipid_count(ctx->filter); i++) {
		GF_FilterEvent evt;
		GF_FilterPid *pid = gf_filter_get_ipid(ctx->filter, i);
		gf_filter_pid_set_discard(pid, GF_TRUE);
		GF_FEVT_INIT(evt, GF_FEVT_STOP, pid);
		gf_filter_pid_send_event(pid, &evt);
	}
	for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
		GF_FilterPid *pid = gf_filter_get_opid(ctx->filter, i);
		gf_filter_pid_set_eos(pid);
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Fatal error, aborting\n"));
	ctx->in_error = GF_TRUE;
	return GF_SERVICE_ERROR;
}

GF_Err dashdmx_process(GF_Filter *filter)
{
	u32 i, count;
	GF_FilterPacket *pck;
	GF_Err e;
	u32 inputs_fetched = 1;
	u32 next_time_ms = 0;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	Bool check_eos = ctx->check_eos;
	Bool has_pck = GF_FALSE;
	Bool switch_pending = GF_FALSE;

	//reset group states and update stats
	count = gf_dash_get_group_count(ctx->dash);
	for (i=0; i<count; i++) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
		if (!group) continue;
		group->nb_eos = 0;
		if (group->eos_detected) check_eos = GF_TRUE;
	}

	if (!ctx->mpd_pid)
		return GF_EOS;

	//this needs further testing
#if 0
	//we had a seek outside of the period we were setting up, during period setup !
	//force a stop/play event on each output pids
	if (ctx->seek_request>=0) {
		GF_FilterEvent evt;
		memset(&evt, 0, sizeof(GF_FilterEvent));

		count = gf_filter_get_opid_count(filter);
		for (i=0; i<count; i++) {
			evt.base.on_pid = gf_filter_get_opid(filter, i);
			evt.base.type = GF_FEVT_STOP;
			dashdmx_process_event(filter, &evt);

			evt.base.type = GF_FEVT_PLAY;
			evt.play.start_range = ctx->seek_request;
			dashdmx_process_event(filter, &evt);
		}
		ctx->seek_request = 0;
	}
#endif


	//check MPD pid
	pck = gf_filter_pid_get_packet(ctx->mpd_pid);
	if (pck) {
		gf_filter_pid_drop_packet(ctx->mpd_pid);
	}
	e = gf_dash_process(ctx->dash);
	if (e == GF_IP_NETWORK_EMPTY) {
		gf_filter_ask_rt_reschedule(filter, 100000);
		return GF_OK;
	}
	else if (e==GF_SERVICE_ERROR) {
		return dashin_abort(ctx);
	}
	if (e)
		return e;

	next_time_ms = gf_dash_get_min_wait_ms(ctx->dash);
	if (next_time_ms>1000)
		next_time_ms=1000;

	count = gf_filter_get_ipid_count(filter);

	if (ctx->compute_min_dts)
		ctx->timedisc_next_min_ts = 0;

	//flush all media input
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid;
		GF_DASHGroup *group;
		if (ipid == ctx->mpd_pid) continue;
		opid = gf_filter_pid_get_udta(ipid);
		group = gf_filter_pid_get_udta(opid);

		if (!group || group->nb_pending) {
			inputs_fetched = 0;
			continue;
		}

		while (1) {
			pck = gf_filter_pid_get_packet(ipid);
			if (!group->is_playing) {
				if (pck) {
					//in file mode, keep first packet (init seg) until we play
					if ((ctx->forward!=DFWD_FILE) || !group->prev_is_init_segment) {
						gf_filter_pid_drop_packet(ipid);
						continue;
					}
				}
//				inputs_fetched = 0;
				break;
			}

			if (!pck) {
				inputs_fetched = 0;
				if (gf_filter_pid_is_eos(ipid) || !gf_filter_pid_is_playing(opid) || group->force_seg_switch) {
					group->nb_eos++;

					//wait until all our inputs are done
					if (group->nb_eos == group->nb_pids) {
						u32 j, nb_block = 0;
						//check all pids in this group, postpone segment switch if blocking
						for (j=0; j<count; j++) {
							GF_FilterPid *an_ipid = gf_filter_get_ipid(filter, j);
							GF_FilterPid *an_opid = gf_filter_pid_get_udta(an_ipid);
							GF_DASHGroup *agroup;
							if (an_ipid == ctx->mpd_pid) continue;
							agroup = gf_filter_pid_get_udta(an_opid);
							if (!agroup || (agroup != group)) continue;

							if (gf_filter_pid_would_block(an_opid)) {
								nb_block++;
							}
						}
						if (nb_block == group->nb_pids) {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] End of segment for group %d but %d output pid(s) would block, postponing\n", group->idx, nb_block));
							switch_pending = GF_TRUE;
							break;
						}

						//good to switch, cancel all end of stream signals on pids from this group and switch
						GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] End of segment for group %d, updating stats and switching segment\n", group->idx));
						for (j=0; j<count; j++) {
							const GF_PropertyValue *p;
							GF_PropertyEntry *pe=NULL;
							GF_FilterPid *an_ipid = gf_filter_get_ipid(filter, j);
							GF_FilterPid *an_opid = gf_filter_pid_get_udta(an_ipid);
							GF_DASHGroup *agroup;
							if (an_ipid == ctx->mpd_pid) continue;
							agroup = gf_filter_pid_get_udta(an_opid);
							if (!agroup || (agroup != group)) continue;

							if (gf_filter_pid_is_eos(an_ipid) || group->force_seg_switch) {
								gf_filter_pid_clear_eos(an_ipid, GF_TRUE);
								GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Clearing EOS on pids from group %d\n", group->idx));
							}

							p = gf_filter_pid_get_info_str(an_ipid, "smooth_tfrf", &pe);
							if (p) {
								gf_dash_group_push_tfrf(ctx->dash, group->idx, p->value.ptr, gf_filter_pid_get_timescale(an_ipid));
							}
							gf_filter_release_property(pe);

							u32 flags = gf_filter_pid_get_udta_flags(an_opid);
							gf_filter_pid_set_udta_flags(an_opid, flags & ~FLAG_FIRST_IN_SEG);
						}
						dashdmx_update_group_stats(ctx, group);
						group->stats_uploaded = GF_TRUE;
						group->force_seg_switch = GF_FALSE;
						dashdmx_switch_segment(ctx, group);

						gf_filter_prevent_blocking(filter, GF_FALSE);
						if (group->eos_detected && !has_pck) check_eos = GF_TRUE;
					}
				}
				else {
					//still waiting for input packets, do not reschedule (let filter session do it)
					next_time_ms = 0;
					if (ctx->abort)
						dashdmx_update_group_stats(ctx, group);
					//GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] No source packet group %d and not in end of stream\n", group->idx));
				}
				if (group->in_error || group->seg_was_not_ready) {
					//reset EOS state on all input if we were in error
					u32 j;
					for (j=0; j<count && group->in_error; j++) {
						GF_FilterPid *an_ipid = gf_filter_get_ipid(filter, j);
						GF_FilterPid *an_opid = gf_filter_pid_get_udta(an_ipid);
						GF_DASHGroup *agroup;
						if (an_ipid == ctx->mpd_pid) continue;
						agroup = gf_filter_pid_get_udta(an_opid);
						if (!agroup || (agroup != group)) continue;

						gf_filter_pid_clear_eos(an_ipid, GF_TRUE);
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Clearing EOS on pids from group %d\n", group->idx));

						u32 flags = gf_filter_pid_get_udta_flags(an_opid);
						gf_filter_pid_set_udta_flags(an_opid, flags & ~FLAG_FIRST_IN_SEG);
					}
					dashdmx_switch_segment(ctx, group);
					gf_filter_prevent_blocking(filter, GF_FALSE);
					if (group->eos_detected && !has_pck) check_eos = GF_TRUE;
				}
				break;
			}

			if (ctx->compute_min_dts) {
				if (inputs_fetched) inputs_fetched++;
				u32 timescale = gf_filter_pid_get_timescale(ipid);
				u64 dts = gf_filter_pck_get_dts(pck);
				if (dts==GF_FILTER_NO_TS)
					dts = gf_filter_pck_get_cts(pck);
				if (dts==GF_FILTER_NO_TS)
					continue;
				dts = gf_timestamp_rescale(dts, timescale, ctx->time_discontinuity.den);
				if (!ctx->timedisc_next_min_ts || (ctx->timedisc_next_min_ts > dts)) ctx->timedisc_next_min_ts = dts;
				break;
			}
			has_pck = GF_TRUE;
			check_eos = GF_FALSE;
			dashdmx_forward_packet(ctx, pck, ipid, opid, group);
			group->wait_for_pck = GF_FALSE;
			dashdmx_update_group_stats(ctx, group);
		}
	}
	if (ctx->compute_min_dts) {
		if (inputs_fetched>1) {
			ctx->compute_min_dts = GF_FALSE;
		} else {
			gf_filter_ask_rt_reschedule(filter, 1000);
			return GF_OK;
		}
	}

	if (switch_pending) {
		gf_filter_ask_rt_reschedule(filter, 1000);
		return GF_OK;
	}
	if (has_pck)
		check_eos = GF_FALSE;
	//only check for eos if no pending events, otherwise we may miss a PLAY pending (due to seek)
	else if (check_eos && gf_filter_get_num_events_queued(filter))
		check_eos = GF_FALSE;

	if (check_eos) {
		Bool all_groups_done = GF_TRUE;
		Bool groups_not_playing = GF_TRUE;
		Bool is_in_last_period = gf_dash_in_last_period(ctx->dash, GF_TRUE);

		//not last period, check if we are done playing all groups due to stop requests
		if (!is_in_last_period && !ctx->nb_playing) {
			Bool groups_done=GF_TRUE;
			for (i=0; i<count; i++) {
				GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
				GF_FilterPid *opid;
				GF_DASHGroup *group;
				if (ipid == ctx->mpd_pid) continue;
				opid = gf_filter_pid_get_udta(ipid);
				group = gf_filter_pid_get_udta(opid);
				if (!group) continue;
				if (!group->is_playing && group->eos_detected) continue;
				groups_done=GF_FALSE;
			}
			if (groups_done)
				is_in_last_period = GF_TRUE;
		}

		for (i=0; i<count; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			GF_FilterPid *opid;
			GF_DASHGroup *group;
			if (ipid == ctx->mpd_pid) continue;
			opid = gf_filter_pid_get_udta(ipid);
			group = gf_filter_pid_get_udta(opid);
			//reset in progress
			if (!group) {
				all_groups_done = GF_FALSE;
				continue;
			}
			if (group->is_playing)
				groups_not_playing = GF_FALSE;

			if (!group->eos_detected && group->is_playing) {
				all_groups_done = GF_FALSE;
			} else if (is_in_last_period) {
				if (gf_filter_pid_is_eos(ipid) || group->eos_detected || gf_dash_get_group_done(ctx->dash, group->idx))
					gf_filter_pid_set_eos(opid);
				else
					all_groups_done = GF_FALSE;
			}
		}
		if (all_groups_done) {
			if (is_in_last_period || groups_not_playing) {
				if (!ctx->manifest_stop_sent) {
					GF_FilterEvent evt;
					ctx->manifest_stop_sent = GF_TRUE;
					GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->mpd_pid)
					gf_filter_pid_send_event(ctx->mpd_pid, &evt);
				}
				return GF_EOS;
			}
			if (!gf_dash_get_period_switch_status(ctx->dash)) {
				for (i=0; i<count; i++) {
					GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
					if (!group) continue;
					group->nb_eos = 0;
					group->eos_detected = GF_FALSE;
				}

				gf_dash_request_period_switch(ctx->dash);
			}
		}
	}

	if (gf_dash_is_in_setup(ctx->dash))
		gf_filter_post_process_task(filter);
	else if (ctx->abort)
		gf_filter_ask_rt_reschedule(filter, 50000);
	else if (next_time_ms)
		gf_filter_ask_rt_reschedule(filter, 1000 * next_time_ms);

	return GF_OK;
}

static const char *dashdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	char *d = (char *)data;
	char *res_dash, *res_m3u, *res_smooth;
	char last_c = d[size-1];
	d[size-1] = 0;
	res_dash = strstr(data, "<MPD ");
	res_m3u = strstr(data, "#EXTM3U");
	res_smooth = strstr(data, "<SmoothStreamingMedia");
	d[size-1] = last_c;

	if (res_dash) {
		*score = GF_FPROBE_SUPPORTED;
		return "application/dash+xml";
	}
	if (res_m3u) {
		*score = GF_FPROBE_SUPPORTED;
		//we don't use x-mpegURL to avoid complaints from HLS mediastreamvalidator
		return "application/vnd.apple.mpegURL";
	}
	if (res_smooth) {
		*score = GF_FPROBE_SUPPORTED;
		return "application/vnd.ms-sstr+xml";
	}
	return NULL;
}

#define OFFS(_n)	#_n, offsetof(GF_DASHDmxCtx, _n)
static const GF_FilterArgs DASHDmxArgs[] =
{
	{ OFFS(auto_switch), "switch quality every N segments\n"
		"- positive: go to higher quality or loop to lowest\n"
		"- negative: go to lower quality or loop to highest\n"
		"- 0: disabled", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(segstore), "enable file caching\n"
		"- mem: all files are stored in memory, no disk IO\n"
		"- disk: files are stored to disk but discarded once played\n"
		"- cache: all files are stored to disk and kept"
		"", GF_PROP_UINT, "mem", "mem|disk|cache", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(algo), "adaptation algorithm to use\n"
		"- none: no adaptation logic\n"
		"- grate: GPAC legacy algo based on available rate\n"
		"- gbuf: GPAC legacy algo based on buffer occupancy\n"
		"- bba0: BBA-0\n"
		"- bolaf: BOLA Finite\n"
		"- bolab: BOLA Basic\n"
		"- bolau: BOLA-U\n"
		"- bolao: BOLA-O\n"
		"- JS: use file JS (either with specified path or in $GSHARE/scripts/) for algo (.js extension may be omitted)"
		, GF_PROP_STRING, "gbuf", "none|grate|gbuf|bba0|bolaf|bolab|bolau|bolao|JS", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(start_with), "initial selection criteria\n"
		"- min_q: start with lowest quality\n"
		"- max_q: start with highest quality\n"
		"- min_bw: start with lowest bitrate\n"
		"- max_bw: start with highest bitrate; if tiles are used, all low priority tiles will have the lower (below max) bandwidth selected\n"
		"- max_bw_tiles: start with highest bitrate; if tiles are used, all low priority tiles will have their lowest bandwidth selected"
		, GF_PROP_UINT, "max_bw", "min_q|max_q|min_bw|max_bw|max_bw_tiles", 0},

	{ OFFS(max_res), "use max media resolution to configure display", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(abort), "allow abort during a segment download", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(use_bmin), "playout buffer handling\n"
		"- no: use default player settings\n"
		"- auto: notify player of segment duration if not low latency\n"
		"- mpd: use the indicated min buffer time of the MPD", GF_PROP_UINT, "auto", "no|auto|mpd", 0},

	{ OFFS(shift_utc), "shift DASH UTC clock in ms", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(spd), "suggested presentation delay in ms", GF_PROP_SINT, "-I", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(route_shift), "shift ROUTE requests time by given ms", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(server_utc), "use `ServerUTC` or `Date` HTTP headers instead of local UTC", GF_PROP_BOOL, "yes", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(screen_res), "use screen resolution in selection phase", GF_PROP_BOOL, "yes", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(init_timeshift), "set initial timeshift in ms (if >0) or in per-cent of timeshift buffer (if <0)", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tile_mode), "tile adaptation mode\n"
		"- none: bitrate is shared equally across all tiles\n"
		"- rows: bitrate decreases for each row of tiles starting from the top, same rate for each tile on the row\n"
		"- rrows: bitrate decreases for each row of tiles starting from the bottom, same rate for each tile on the row\n"
		"- mrows: bitrate decreased for top and bottom rows only, same rate for each tile on the row\n"
		"- cols: bitrate decreases for each columns of tiles starting from the left, same rate for each tile on the columns\n"
		"- rcols: bitrate decreases for each columns of tiles starting from the right, same rate for each tile on the columns\n"
		"- mcols: bitrate decreased for left and right columns only, same rate for each tile on the columns\n"
		"- center: bitrate decreased for all tiles on the edge of the picture\n"
		"- edges: bitrate decreased for all tiles on the center of the picture"
		, GF_PROP_UINT, "none", "none|rows|rrows|mrows|cols|rcols|mcols|center|edges", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tiles_rate), "indicate the amount of bandwidth to use at each quality level. The rate is recursively applied at each level, e.g. if 50%, Level1 gets 50%, level2 gets 25%, ... If 100, automatic rate allocation will be done by maximizing the quality in order of priority. If 0, bitstream will not be smoothed across tiles/qualities, and concurrency may happen between different media", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(delay40X), "delay in milliseconds to wait between two 40X on the same segment", GF_PROP_UINT, "500", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(exp_threshold), "delay in milliseconds to wait after the segment AvailabilityEndDate before considering the segment lost", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(switch_count), "indicate how many segments the client shall wait before switching up bandwidth. If 0, switch will happen as soon as the bandwidth is enough, but this is more prone to network variations", GF_PROP_UINT, "1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(aggressive), "if enabled, switching algo targets the closest bandwidth fitting the available download rate. If no, switching algo targets the lowest bitrate representation that is above the currently played (e.g. does not try to switch to max bandwidth)", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(debug_as), "play only the adaptation sets indicated by their indices (0-based) in the MPD", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(speedadapt), "enable adaptation based on playback speed", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(noxlink), "disable xlink if period has both xlink and adaptation sets", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(query), "set query string (without initial '?') to append to xlink of periods", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(split_as), "separate all qualities into different adaptation sets and stream all qualities. Dependent representations (scalable) are treated as independent", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(noseek), "disable seeking of initial segment(s) in dynamic mode (useful when UTC clocks do not match)", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(bwcheck), "minimum time in milliseconds between two bandwidth checks when allowing segment download abort", GF_PROP_UINT, "5", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(lowlat), "segment scheduling policy in low latency mode\n"
		"- no: disable low latency\n"
		"- strict: strict respect of AST offset in low latency\n"
		"- early: allow fetching segments earlier than their AST in low latency when input PID is empty", GF_PROP_UINT, "early", "no|strict|early", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(forward), "segment forwarding mode\n"
		"- none: regular DASH read\n"
		"- file: do not demultiplex files and forward them as file PIDs (imply `segstore=mem`)\n"
		"- segb: turn on [-split_as](), segment and fragment bounds signaling (`sigfrag`) in sources and DASH cue insertion\n"
		"- mani: same as `segb` and also forward manifests"
	, GF_PROP_UINT, "none", "none|file|segb|mani", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fmodefwd), "forward packet rather than copy them in `file` forward mode. Packet copy might improve performances in low latency mode", GF_PROP_BOOL, "yes", NULL, GF_FS_ARG_HINT_EXPERT},

	{ OFFS(skip_lqt), "disable decoding of tiles with highest degradation hints (not visible, not gazed at) for debug purposes", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(llhls_merge), "merge LL-HLS byte range parts into a single open byte range request", GF_PROP_BOOL, "yes", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(filemode), "alias for forward=file", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(groupsel), "select groups based on language (by default all playable groups are exposed)", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(chain_mode), "MPD chaining mode\n"
	"- off: do not use MPD chaining\n"
	"- on: use MPD chaining once over, fallback if MPD load failure\n"
	"- error: use MPD chaining once over or if error (MPD or segment download)", GF_PROP_UINT, "on", "off|on|error", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(asloop), "when auto switch is enabled, iterates back and forth from highest to lowest qualities", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};



static const GF_FilterCapability DASHDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, DASHIN_FILE_EXT),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, DASHIN_MIMES),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//accept any stream but files, framed
	{ .code=GF_PROP_PID_STREAM_TYPE, .val.type=GF_PROP_UINT, .val.value.uint=GF_STREAM_FILE, .flags=(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_LOADED_FILTER) },
	{ .code=GF_PROP_PID_UNFRAMED, .val.type=GF_PROP_BOOL, .val.value.boolean=GF_TRUE, .flags=(GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_LOADED_FILTER) },
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};


GF_FilterRegister DASHDmxRegister = {
	.name = "dashin",
	GF_FS_SET_DESCRIPTION("MPEG-DASH and HLS client")
	GF_FS_SET_HELP("This filter reads MPEG-DASH, HLS and MS Smooth manifests.\n"
	"\n"
	"# Regular mode\n"
	"This is the default mode, in which the filter produces media PIDs and frames from sources indicated in the manifest.\n"
	"The default behavior is to perform adaptation according to [-algo](), but the filter can:\n"
	"- run with no adaptation, to grab maximum quality.\n"
	"EX gpac -i MANIFEST_URL:algo=none:start_with=max_bw -o dest.mp4\n"
	"- run with no adaptation, fetching all qualities.\n"
	"EX gpac -i MANIFEST_URL:split_as -o dst=$File$.mp4:clone\n"
	"\n"
	"# File mode\n"
	"When [-forward]() is set to `file`, the client forwards media files without demultiplexing them.\n"
	"This is mostly used to expose the DASH session to a file server such as ROUTE or HTTP.\n"
	"In this mode, the manifest is forwarded as an output PID.\n"
	"Warning: This mode cannot be set through inheritance as it changes the link capabilities of the filter. The filter MUST be explicitly declared.\n"
	"\n"
	"To expose a live DASH session to route:\n"
	"EX gpac -i MANIFEST_URL dashin:forward=file -o route://225.0.0.1:8000/\n"
	"\n"
	"Note: This mode used to be trigger by [-filemode]() option, still recognized.\n"
	"\n"
	"If the source has dependent media streams (scalability) and all qualities and initialization segments need to be forwarded, add [-split_as]().\n"
	"\n"
	"# Segment bound modes\n"
	"When [-forward]() is set to `segb` or `mani`, the client forwards media frames (after demultiplexing) together with segment and fragment boundaries of source files.\n"
	"\n"
	"This mode can be used to process media data and regenerate the same manifest/segmentation.\n"
	"\n"
	"EX gpac -i MANIFEST_URL:forward=mani cecrypt:cfile=DRM.xml -o encrypted/live.mpd:pssh=mv\n"
	"This will encrypt an existing DASH session, inject PSSH in manifest and segments.\n"
	"\n"
	"EX gpac -i MANIFEST_URL:forward=segb cecrypt:cfile=DRM.xml -o encrypted/live.m3u8\n"
	"This will encrypt an existing DASH session and republish it as HLS, using same segment names and boundaries.\n"
	"\n"
	"This mode will force [-noseek]()=`true` to ensure the first segment fetched is complete, and [-split_as]()=`true` to fetch all qualities.\n"
	"\n"
	"Each first packet of a segment will have the following properties attached:\n"
	"- `CueStart`: indicate this is a segment start\n"
	"- `FileNumber`: current segment number\n"
	"- `FileName`: current segment file name without manifest (MPD or master HLS) base url\n"
	"- `DFPStart`: set with value `0` if this is the first packet in the period, absent otherwise\n"
	"\n"
	"If [-forward]() is set to `mani`, the first packet of a segment dispatched after a manifest update will also carry the manifest payload as a property:\n"
	"- `DFManifest`: contains main manifest (MPD, M3U8 master)\n"
	"- `DFVariant`: contains list of HLS child playlists as strings for the given quality\n"
	"- `DFVariantName`: contains list of associated HLS child playlists name, in same order as manifests in `DFVariant`\n"
	"\n"
	"Each output PID will have the following properties assigned:\n"
	"- `DFMode`: set to 1 for `segb` or 2 for `mani`\n"
	"- `DCue`: set to `inband`\n"
	"- `DFPStart`: set to current period start value\n"
	"- `FileName`: set to associated init segment if any\n"
	"- `Representation`: set to the associated representation ID in the manifest\n"
	"- `DashDur`: set to the average segment duration as indicated in the manifest\n"
	"- `source_template`: set to true to indicate the source template is known\n"
	"- `stl_timescale`: timescale used by SegmentTimeline, or 0 if no SegmentTimeline\n"
	"- `init_url`: unresolved intialization URL (as it appears in the MPD or in the variant playlist)\n"
	"- `manifest_url`: manifest URL\n"
	"- `hls_variant_name`: HLS variant playlist name (as it appears in the HLS master playlist)\n"
	"\n"
	"When the [dasher](dasher) is used together with this mode, this will force all generated segments to have the same name, duration and fragmentation properties as the input ones. It is therefore not recommended for sessions stored/generated on local storage to generate the output in the same directory.\n"
	)
	.private_size = sizeof(GF_DASHDmxCtx),
	.initialize = dashdmx_initialize,
	.finalize = dashdmx_finalize,
	.args = DASHDmxArgs,
	SETCAPS(DASHDmxCaps),
	//we need the resolver, and pids are declared dynamically
	.flags = GF_FS_REG_REQUIRES_RESOLVER | GF_FS_REG_DYNAMIC_PIDS,
	.configure_pid = dashdmx_configure_pid,
	.process = dashdmx_process,
	.process_event = dashdmx_process_event,
	.probe_data = dashdmx_probe_data,
	//we accept as many input pids as loaded by the session
	.max_extra_pids = (u32) -1,
};


#endif //GPAC_DISABLE_DASH_CLIENT

const GF_FilterRegister *dashdmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_DASH_CLIENT
	return &DASHDmxRegister;
#else
	return NULL;
#endif
}


#ifndef GPAC_DISABLE_DASH_CLIENT
static s32 dashdmx_rate_adaptation_ext(void *udta, u32 group_idx, u32 base_group_idx, Bool force_lower_complexity, GF_DASHCustomAlgoInfo *stats)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) udta;
	return ctx->on_rate_adaptation(ctx->rt_udta, group_idx, base_group_idx, force_lower_complexity, stats);
}


typedef struct
{
	u32 bits_per_sec;
	u64 total_bytes;
	u64 bytes_done;
	u64 us_since_start;
	u32 buffer_dur;
	u32 current_seg_dur;
} GF_DASHDownloadStats;

static s32 dashdmx_download_monitor_ext(void *udta, u32 group_idx, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start, u32 buffer_dur_ms, u32 current_seg_dur)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) udta;
	GF_DASHDownloadStats stats;
	memset(&stats, 0, sizeof(GF_DASHDownloadStats));
	stats.bits_per_sec = bits_per_sec;
	stats.total_bytes = total_bytes;
	stats.bytes_done = bytes_done;
	stats.us_since_start = us_since_start;
	stats.buffer_dur = buffer_dur_ms;
	stats.current_seg_dur = current_seg_dur;
	return ctx->on_download_monitor(ctx->rt_udta, group_idx, &stats);
}
#endif /*GPAC_DISABLE_DASH_CLIENT*/


GF_EXPORT
GF_Err gf_filter_bind_dash_algo_callbacks(GF_Filter *filter, void *udta,
		void (*period_reset)(void *rate_adaptation, u32 type),
		void (*new_group)(void *udta, u32 group_idx, void *dash),
		s32 (*rate_adaptation)(void *udta, u32 group_idx, u32 base_group_idx, Bool force_low_complex, void *stats),
		s32 (*download_monitor)(void *udta, u32 group_idx, void *stats)
)
{
#ifdef GPAC_DISABLE_DASH_CLIENT
	return GF_NOT_SUPPORTED;
#else
	if (!gf_filter_is_instance_of(filter, &DASHDmxRegister))
		return GF_BAD_PARAM;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);

	if (rate_adaptation) {
		ctx->on_period_reset = period_reset;
		ctx->on_new_group = new_group;
		ctx->on_rate_adaptation = rate_adaptation;
		ctx->on_download_monitor = download_monitor;
		ctx->rt_udta = udta;
		ctx->abort = download_monitor ? GF_TRUE : GF_FALSE;
		gf_dash_set_algo_custom(ctx->dash, ctx, dashdmx_rate_adaptation_ext, dashdmx_download_monitor_ext);
	} else {
		ctx->on_period_reset = NULL;
		ctx->on_new_group = NULL;
		ctx->on_rate_adaptation = NULL;
		ctx->on_download_monitor = NULL;
		ctx->rt_udta = NULL;
		ctx->abort = GF_FALSE;
		gf_dash_set_algo(ctx->dash, GF_DASH_ALGO_GPAC_LEGACY_BUFFER);
	}
	return GF_OK;

#endif
}
