/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2021
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4box application
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


#include "mp4box.h"

#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/scene_manager.h>
#include <gpac/network.h>
#include <gpac/base_coding.h>

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_SVG)
#include <gpac/scenegraph.h>
#endif


#ifndef GPAC_DISABLE_BIFS
#include <gpac/bifs.h>
#endif
#ifndef GPAC_DISABLE_VRML
#include <gpac/nodes_mpeg4.h>
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE

#include <gpac/xml.h>
#include <gpac/isomedia.h>

typedef struct
{
	const char *root_file;
	const char *dir;
	GF_List *imports;
} WGTEnum;

GF_Err set_file_udta(GF_ISOFile *dest, u32 tracknum, u32 udta_type, char *src, Bool is_box_array, Bool is_string)
{
	u8 *data = NULL;
	GF_Err res = GF_OK;
	u32 size;
	bin128 uuid;
	memset(uuid, 0 , 16);

	if (!udta_type && !is_box_array) return GF_BAD_PARAM;

	if (!src || !strlen(src)) {
		GF_Err e = gf_isom_remove_user_data(dest, tracknum, udta_type, uuid);
		if (e==GF_EOS) {
			e = GF_OK;
			M4_LOG(GF_LOG_WARNING, ("No track.udta found, ignoring\n"));
		}
		return e;
	}

#ifndef GPAC_DISABLE_CORE_TOOLS
	if (!strnicmp(src, "base64", 6)) {
		src += 7;
		size = (u32) strlen(src);
		data = gf_malloc(sizeof(char) * size);
		size = gf_base64_decode((u8 *)src, size, data, size);
	} else
#endif
	if (is_string) {
		data = (u8 *) src;
		size = (u32) strlen(src)+1;
		is_box_array = 0;
	} else {
		GF_Err e = gf_file_load_data(src, (u8 **) &data, &size);
		if (e) return e;
	}

	if (size && data) {
		if (is_box_array) {
			res = gf_isom_add_user_data_boxes(dest, tracknum, data, size);
		} else {
			res = gf_isom_add_user_data(dest, tracknum, udta_type, uuid, data, size);
		}
		if (!is_string)
			gf_free(data);
	}
	return res;
}

#ifndef GPAC_DISABLE_MEDIA_IMPORT

extern u32 swf_flags;
extern Float swf_flatten_angle;
extern Bool keep_sys_tracks;
extern u32 fs_dump_flags;

void scene_coding_log(void *cbk, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char *fmt, va_list vlist);

void convert_file_info(char *inName, GF_ISOTrackID trackID)
{
	GF_Err e;
	u32 i;
	Bool found;
	GF_MediaImporter import;
	memset(&import, 0, sizeof(GF_MediaImporter));
	import.trackID = trackID;
	import.in_name = inName;
	import.flags = GF_IMPORT_PROBE_ONLY;
	import.print_stats_graph = fs_dump_flags;

	e = gf_media_import(&import);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error probing file %s: %s\n", inName, gf_error_to_string(e)));
		return;
	}
	if (trackID) {
		fprintf(stderr, "Import probing results for track %s#%d:\n", inName, trackID);
	} else {
		fprintf(stderr, "Import probing results for %s:\n", inName);
		if (!import.nb_tracks) {
			M4_LOG(GF_LOG_WARNING, ("File has no selectable tracks\n"));
			return;
		}
		fprintf(stderr, "File has %d tracks\n", import.nb_tracks);
	}
	if (import.probe_duration) {
		fprintf(stderr, "Duration: %g s\n", (Double) (import.probe_duration/1000.0));
	}
	found = 0;
	for (i=0; i<import.nb_tracks; i++) {
		if (trackID && (trackID != import.tk_info[i].track_num)) continue;
		if (!trackID) fprintf(stderr, "- Track %d type: ", import.tk_info[i].track_num);
		else fprintf(stderr, "Track type: ");

		switch (import.tk_info[i].stream_type) {
		case GF_STREAM_VISUAL:
			if (import.tk_info[i].media_subtype == GF_ISOM_MEDIA_AUXV)  fprintf(stderr, "Auxiliary Video");
			else if (import.tk_info[i].media_subtype == GF_ISOM_MEDIA_PICT)  fprintf(stderr, "Picture Sequence");
			else fprintf(stderr, "Video");
			if (import.tk_info[i].video_info.temporal_enhancement) fprintf(stderr, " Temporal Enhancement");
			break;
		case GF_STREAM_AUDIO:
			fprintf(stderr, "Audio");
			break;
		case GF_STREAM_TEXT:
			fprintf(stderr, "Text");
			break;
		case GF_STREAM_SCENE:
			fprintf(stderr, "Scene");
			break;
		case GF_STREAM_OD:
			fprintf(stderr, "OD");
			break;
		case GF_STREAM_METADATA:
			fprintf(stderr, "Metadata");
			break;
		default:
			fprintf(stderr, "Other (%s)", gf_4cc_to_str(import.tk_info[i].stream_type));
			break;
		}
		if (import.tk_info[i].codecid) fprintf(stderr, " - Codec %s", gf_codecid_name(import.tk_info[i].codecid));

		if (import.tk_info[i].lang) fprintf(stderr, " lang %s", gf_4cc_to_str(import.tk_info[i].lang));

		if (import.tk_info[i].mpeg4_es_id) fprintf(stderr, " MPEG-4 ESID %d", import.tk_info[i].mpeg4_es_id);

		if (import.tk_info[i].prog_num) {
			if (!import.nb_progs) {
				fprintf(stderr, " Program %d", import.tk_info[i].prog_num);
			} else {
				u32 j;
				for (j=0; j<import.nb_progs; j++) {
					if (import.tk_info[i].prog_num != import.pg_info[j].number) continue;
					fprintf(stderr, " Program %s", import.pg_info[j].name);
					break;
				}
			}
		}
		fprintf(stderr, "\n");

		if (import.tk_info[i].video_info.width && import.tk_info[i].video_info.height
		   ) {
			fprintf(stderr, "\tSize %dx%d", import.tk_info[i].video_info.width, import.tk_info[i].video_info.height);
			if (import.tk_info[i].video_info.FPS) fprintf(stderr, " @ %g FPS", import.tk_info[i].video_info.FPS);
			if (import.tk_info[i].stream_type==GF_STREAM_VISUAL) {
				if (import.tk_info[i].video_info.par) fprintf(stderr, " PAR: %d:%d", import.tk_info[i].video_info.par >> 16, import.tk_info[i].video_info.par & 0xFFFF);
			}

			fprintf(stderr, "\n");
		}
		else if ((import.tk_info[i].stream_type==GF_STREAM_AUDIO) && import.tk_info[i].audio_info.sample_rate) {
			fprintf(stderr, "\tSampleRate %d - %d channels\n", import.tk_info[i].audio_info.sample_rate, import.tk_info[i].audio_info.nb_channels);
		}

		if (trackID) {
			found = 1;
			break;
		}
	}
	fprintf(stderr, "\n");
	M4_LOG(GF_LOG_INFO, ("For more details, use `gpac -i %s inspect[:deep][:analyze=on|bs]`\n", gf_file_basename(inName)));
	if (!found && trackID) {
		M4_LOG(GF_LOG_ERROR, ("Cannot find track %d in file\n", trackID));
	}
}

static GF_Err set_chapter_track(GF_ISOFile *file, u32 track, u32 chapter_ref_trak)
{
	u64 ref_duration, chap_duration;
	Double scale;
	GF_Err e;

	e = gf_isom_set_track_reference(file, chapter_ref_trak, GF_ISOM_REF_CHAP, gf_isom_get_track_id(file, track) );
	if (e) return e;
	e = gf_isom_set_track_enabled(file, track, GF_FALSE);
	if (e) return e;

	ref_duration = gf_isom_get_media_duration(file, chapter_ref_trak);
	chap_duration = gf_isom_get_media_duration(file, track);
	scale = (Double) (s64) gf_isom_get_media_timescale(file, track);
	scale /= gf_isom_get_media_timescale(file, chapter_ref_trak);
	ref_duration = (u64) (ref_duration * scale);

	if (chap_duration < ref_duration) {
		chap_duration -= gf_isom_get_sample_duration(file, track, gf_isom_get_sample_count(file, track));
		chap_duration = ref_duration - chap_duration;
		e = gf_isom_set_last_sample_duration(file, track, (u32) chap_duration);
		if (e) return e;
	}
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		u8 NbBits;
		u32 switchGroupID, nb_crit, size, reserved;
		u8 priority;
		Bool discardable;
		GF_AudioChannelLayout layout;
		gf_isom_get_sample_size(file, track, 1);
		gf_isom_get_sample_dts(file, track, 1);
		gf_isom_get_sample_from_dts(file, track, 0);
		gf_isom_set_sample_padding(file, track, 0);
		gf_isom_has_padding_bits(file, track);
		gf_isom_get_sample_padding_bits(file, track, 1, &NbBits);
		gf_isom_keep_utc_times(file, 1);
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		gf_isom_set_single_moof_mode(file, GF_TRUE);
		gf_isom_reset_sample_count(NULL);
		gf_isom_set_traf_mss_timeext(NULL, 0, 0, 0);
		gf_isom_get_next_moof_number(NULL);
		gf_isom_set_fragment_reference_time(NULL, 0, 0, 0);
#endif
		//this one is not tested in master due to old-arch compat, to remove when we enable tests without old-arch
		gf_isom_get_audio_layout(file, track, 1, &layout);

		gf_isom_get_track_switch_parameter(file, track, 1, &switchGroupID, &nb_crit);
		gf_isom_sample_has_subsamples(file, track, 1, 0);
		gf_isom_sample_get_subsample(file, track, 1, 0, 1, &size, &priority, &reserved, &discardable);
#ifndef GPAC_DISABLE_ISOM_HINTING
		gf_isom_hint_blank_data(NULL, 0, 0);
		gf_isom_hint_sample_description_data(NULL, 0, 0, 1, 0, 0, 0);
		gf_isom_get_payt_info(NULL, 0, 0, NULL);
#endif
		gf_isom_estimate_size(file);

	}
#endif
	return GF_OK;
}

GF_Err parse_fracs(char *str, GF_Fraction64 *f, GF_Fraction64 *dur)
{
	GF_Err e = GF_OK;
	if (dur) {
		char *sep = strchr(str, '-');
		if (sep) {
			sep[0] = 0;
			if (!gf_parse_lfrac(str, f)) e = GF_BAD_PARAM;
			if (!gf_parse_lfrac(sep+1, dur)) e = GF_BAD_PARAM;
			sep[0] = '-';
			return e;
		}
		dur->num = 0;
		dur->den = 0;
	}
	if (!gf_parse_lfrac(str, f)) e = GF_BAD_PARAM;
	return e;
}

Bool scan_color(char *val, u32 *clr_prim, u32 *clr_tranf, u32 *clr_mx, Bool *clr_full_range)
{
	char *sep = strchr(val, ',');
	if (!sep) return GF_FALSE;
	sep[0] = 0;
	*clr_prim = gf_cicp_parse_color_primaries(val);
	sep[0] = ',';
	if (*clr_prim == (u32) -1) return GF_FALSE;
	val = sep+1;

	sep = strchr(val, ',');
	if (!sep) return GF_FALSE;
	sep[0] = 0;
	*clr_tranf = gf_cicp_parse_color_transfer(val);
	sep[0] = ',';
	if (*clr_tranf == (u32) -1) return GF_FALSE;
	val = sep+1;

	sep = strchr(val, ',');
	if (sep) sep[0] = 0;
	*clr_mx = gf_cicp_parse_color_matrix(val);
	if (sep) sep[0] = ',';
	if (*clr_mx == (u32) -1) return GF_FALSE;
	if (!sep) return GF_TRUE;

	val = sep+1;
	if (!strcmp(val, "yes") || !strcmp(val, "on")) {
		*clr_full_range = GF_TRUE;
		return GF_TRUE;
	}
	if (!strcmp(val, "no") || !strcmp(val, "off")) {
		*clr_full_range = GF_FALSE;
		return GF_TRUE;
	}

	if (sscanf(val, "%d", (s32*) clr_full_range) == 1)
		return GF_TRUE;

	return GF_FALSE;
}

static GF_Err set_dv_profile(GF_ISOFile *dest, u32 track, char *dv_profile_str)
{
	GF_Err e;
	u32 dv_profile = 0;
	u32 dv_compat_id=0;
	char *sep = strchr(dv_profile_str, '.');
	if (sep) {
		sep[0] = 0;
		if (!strcmp(sep+1, "none")) dv_compat_id=0;
		else if (!strcmp(sep+1, "hdr10")) dv_compat_id=1;
		else if (!strcmp(sep+1, "bt709")) dv_compat_id=2;
		else if (!strcmp(sep+1, "hlg709")) dv_compat_id=3;
		else if (!strcmp(sep+1, "hlg2100")) dv_compat_id=4;
		else if (!strcmp(sep+1, "bt2020")) dv_compat_id=5;
		else if (!strcmp(sep+1, "brd")) dv_compat_id=6;
		else {
			M4_LOG(GF_LOG_WARNING, ("DV compatibility mode %s not recognized, using none\n", sep+1));
		}
	}
	dv_profile = atoi(dv_profile_str);

	GF_DOVIDecoderConfigurationRecord *dovi = gf_isom_dovi_config_get(dest, track, 1);
	if (dovi) {
		dovi->dv_profile = dv_profile;
		dovi->dv_bl_signal_compatibility_id = dv_compat_id;
		e = gf_isom_set_dolby_vision_profile(dest, track, 1, dovi);
		gf_odf_dovi_cfg_del(dovi);
		return e;
	}
	if (!dv_profile) return GF_OK;
	u32 nb_samples = gf_isom_get_sample_count(dest, track);
	if (!nb_samples) {
		M4_LOG(GF_LOG_ERROR, ("No DV config in file and no samples, cannot guess DV config\n"));
		return GF_NOT_SUPPORTED;
	}

	GF_DOVIDecoderConfigurationRecord _dovi;
	memset(&_dovi, 0, sizeof(GF_DOVIDecoderConfigurationRecord));
	_dovi.dv_version_major = 1;
	_dovi.dv_version_minor = 0;
	_dovi.dv_profile = dv_profile;
	_dovi.dv_bl_signal_compatibility_id = dv_compat_id;

	u32 w, h;
	Bool is_avc = GF_FALSE;

	e = gf_isom_get_visual_info(dest, track, 1, &w, &h);
	if (e) return e;
	//no DV profile present in file, we need to guess
	GF_AVCConfig *avcc = gf_isom_avc_config_get(dest, track, 1);
	GF_HEVCConfig *hvcc = gf_isom_hevc_config_get(dest, track, 1);

	if (!avcc && !hvcc) {
		M4_LOG(GF_LOG_WARNING, ("DV profile can only be set on AVC or HEVC tracks\n"));
		return GF_BAD_PARAM;
	}

	u32 nalu_length = avcc ? avcc->nal_unit_size : hvcc->nal_unit_size;
	if (avcc) {
		is_avc = GF_TRUE;
		gf_odf_avc_cfg_del(avcc);
	}
	if (hvcc) gf_odf_hevc_cfg_del(hvcc);

	//inspect at most first 50 samples
	u32 i;
	for (i=0; i<50; i++) {
		u32 stsd_idx;
		GF_BitStream *bs;

		GF_ISOSample *samp = gf_isom_get_sample(dest, track, i+1, &stsd_idx);
		if (!samp) break;

		bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);

		while (gf_bs_available(bs)) {
			u32 nal_type;
			u32 nal_size = gf_bs_read_int(bs, nalu_length*8);
			if (is_avc) {
				nal_type = gf_bs_read_u8(bs);
				nal_type = nal_type & 0x1F;
				nal_size--;
				if (nal_type == GF_AVC_NALU_DV_RPU) _dovi.rpu_present_flag = 1;
				else if (nal_type == GF_AVC_NALU_DV_EL) _dovi.el_present_flag = 1;
				else if (nal_type <= GF_AVC_NALU_IDR_SLICE) _dovi.bl_present_flag = 1;
			} else {
				gf_bs_read_int(bs, 1);
				nal_type = gf_bs_read_int(bs, 6);
				gf_bs_read_int(bs, 1);
				nal_size--;
				if (nal_type == GF_HEVC_NALU_DV_RPU) _dovi.rpu_present_flag = 1;
				else if (nal_type == GF_HEVC_NALU_DV_EL) _dovi.el_present_flag = 1;
				else if (nal_type <= GF_HEVC_NALU_SLICE_CRA) _dovi.bl_present_flag = 1;
			}
			gf_bs_skip_bytes(bs, nal_size);
		}
		gf_bs_del(bs);
		gf_isom_sample_del(&samp);
	}

	u64 mdur = gf_isom_get_media_duration(dest, track);
	mdur /= nb_samples;
	u32 timescale = gf_isom_get_media_timescale(dest, track);

	_dovi.dv_level = gf_dolby_vision_level(w, h, timescale, mdur, is_avc ? GF_CODECID_AVC : GF_CODECID_HEVC);
	return gf_isom_set_dolby_vision_profile(dest, track, 1, &_dovi);
}



GF_Err apply_edits(GF_ISOFile *dest, u32 track, char *edits)
{
	u32 movie_ts = gf_isom_get_timescale(dest);
	u32 media_ts = gf_isom_get_media_timescale(dest, track);
	u64 media_dur = gf_isom_get_media_duration(dest, track);

	while (edits) {
		GF_Err e;
		char c=0;
		char *sep = strchr(edits+1, 'r');
		if (!sep) sep = strchr(edits+1, 'e');
		if (sep) {
			c = sep[0];
			sep[0] = 0;
		}

		//remove all edits
		if (edits[0] == 'r') {
			e = gf_isom_remove_edits(dest, track);
			if (e) goto error;
		}
		else if (edits[0]=='e') {
			u64 movie_t, media_t, edur;
			u32 rate;
			GF_Fraction64 movie_time, media_time, media_rate, edit_dur;
			char *mtime_sep;

			edits+=1;
			mtime_sep = strchr(edits, ',');
			movie_time.den = media_time.den = media_rate.den = 0;
			if (!mtime_sep) {
				e = parse_fracs(edits, &movie_time, &edit_dur);
				if (e) goto error;
			}
			else {
				mtime_sep[0] = 0;
				e = parse_fracs(edits, &movie_time, &edit_dur);
				if (e) goto error;
				mtime_sep[0] = ',';
				edits = mtime_sep+1;
				mtime_sep = strchr(edits, ',');
				if (!mtime_sep) {
					e = parse_fracs(edits, &media_time, NULL);
					if (e) goto error;
					media_rate.num = media_rate.den = 1;
				} else {
					mtime_sep[0] = 0;
					e = parse_fracs(edits, &media_time, NULL);
					if (e) goto error;
					mtime_sep[0] = ',';
					e = parse_fracs(mtime_sep+1, &media_rate, NULL);
					if (e) goto error;
				}
			}
			if (!movie_time.den || (movie_time.num<0)) {
				e = GF_BAD_PARAM;
				fprintf(stderr, "Wrong edit format %s, movie time must be valid and >= 0\n", edits);
				goto error;
			}
			movie_t = movie_time.num * movie_ts / movie_time.den;
			if (!edit_dur.den || !edit_dur.num) {
				edur = media_dur;
				edur *= movie_ts;
				edur /= media_ts;
			} else {
				edur = edit_dur.num;
				edur *= movie_ts;
				edur /= edit_dur.den;
				edur /= movie_time.den;
				if (edur>media_dur)
					edur = media_dur;
			}
			if (!media_time.den) {
				e = gf_isom_set_edit(dest, track, movie_t, edur, 0, GF_ISOM_EDIT_EMPTY);
			} else {
				rate = 0;
				if (media_rate.den) {
					u64 frac;
					rate = (u32) ( media_rate.num / media_rate.den );
					frac = media_rate.num - rate*media_rate.den;
					frac *= 0xFFFF;
					frac /= media_rate.den;
					rate = (rate<<16) | (u32) frac;
				}
				media_t = media_time.num * media_ts / media_time.den;
				e = gf_isom_set_edit_with_rate(dest, track, movie_t, edur, media_t, rate);
			}
			if (e==GF_EOS) {
				fprintf(stderr, "Inserted empty edit before edit at start time "LLD"/"LLU"\n", movie_time.num, movie_time.den);
				e = GF_OK;
			}
			if (e) goto error;
		} else {
			e = GF_BAD_PARAM;
			fprintf(stderr, "Wrong edit format %s, should start with 'e' or 'r'\n", edits);
			goto error;
		}
error:
		if (sep) sep[0] = c;
		if (e) return e;

		if (!sep) break;
		edits = sep;
	}
	return GF_OK;
}

static const char *videofmt_names[] = { "component", "pal", "ntsc", "secam", "mac", "undef"};


GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, GF_FilterSession *fsess, char **mux_args_if_first_pass, char **mux_sid_if_first_pass, u32 tk_idx)
{
	u32 track_id, i, j, timescale, track, stype, profile, compat, level, new_timescale, rescale_num, rescale_den, svc_mode, txt_flags, split_tile_mode, temporal_mode, nb_tracks;
	s32 par_d, par_n, prog_id, force_rate, moov_timescale;
	s32 tw, th, tx, ty, tz, txtw, txth, txtx, txty;
	Bool do_audio, do_video, do_auxv,do_pict, do_all, track_layout, text_layout, chap_ref, is_chap, is_chap_file, keep_handler, rap_only, refs_only, force_par, rewrite_bs;
	u32 group, handler, rvc_predefined, check_track_for_svc, check_track_for_lhvc, check_track_for_hevc, do_disable;
	const char *szLan;
	GF_Err e = GF_OK;
	GF_Fraction delay;
	u32 tmcd_track = 0, neg_ctts_mode=0;
	Bool keep_audelim = GF_FALSE;
	u32 print_stats_graph=fs_dump_flags;
	GF_MediaImporter import;
	char *ext, *final_name=NULL, *handler_name, *rvc_config, *chapter_name;
	GF_List *kinds;
	GF_TextFlagsMode txt_mode = GF_ISOM_TEXT_FLAGS_OVERWRITE;
	u8 max_layer_id_plus_one, max_temporal_id_plus_one;
	u32 clap_wn, clap_wd, clap_hn, clap_hd, clap_hon, clap_hod, clap_von, clap_vod;
	Bool has_clap=GF_FALSE;
	Bool use_stz2=GF_FALSE;
	Bool has_mx=GF_FALSE;
	s32 mx[9];
	u32 bitdepth=0;
	char dv_profile[100]; /*Dolby Vision*/
	u32 clr_type=0;
	u32 clr_prim;
	u32 clr_tranf;
	u32 clr_mx;
	Bool rescale_override=GF_FALSE;
	Bool clr_full_range=GF_FALSE;
	Bool fmt_ok = GF_TRUE;
	u32 icc_size=0, track_flags=0;
	u8 *icc_data = NULL;
	u32 tc_fps_num=0, tc_fps_den=0, tc_h=0, tc_m=0, tc_s=0, tc_f=0, tc_frames_per_tick=0;
	Bool tc_force_counter=GF_FALSE;
	Bool tc_drop_frame = GF_FALSE;
	char *ext_start;
	u32 xps_inband=0;
	u64 source_magic=0;
	char *opt_src = NULL;
	char *opt_dst = NULL;
	char *fchain = NULL;
	char *edits = NULL;
	const char *fail_msg = NULL;
	Bool set_ccst=GF_FALSE;
	Bool has_last_sample_dur=GF_FALSE;
	u32 fake_import = 0;
	GF_Fraction last_sample_dur = {0,0};
	s32 fullrange, videofmt, colorprim, colortfc, colormx;
	clap_wn = clap_wd = clap_hn = clap_hd = clap_hon = clap_hod = clap_von = clap_vod = 0;
	GF_ISOMTrackFlagOp track_flags_mode=0;
	u32 roll_change=0;
	u32 roll = 0;
	Bool src_is_isom = GF_FALSE;

	dv_profile[0] = 0;
	rvc_predefined = 0;
	chapter_name = NULL;
	new_timescale = 1;
	moov_timescale = 0;
	rescale_num = rescale_den = 0;
	text_layout = 0;
	/*0: merge all
	  1: split base and all SVC in two tracks
	  2: split all base and SVC layers in dedicated tracks
	 */
	svc_mode = 0;

	if (import_flags==0xFFFFFFFF) {
		import_flags = 0;
		fake_import = 1;
	}

	memset(&import, 0, sizeof(GF_MediaImporter));

	final_name = gf_strdup(inName);
#ifdef WIN32
	/*dirty hack for msys&mingw: when we use import options, the ':' separator used prevents msys from translating the path
	we do this for regular cases where the path starts with the drive letter. If the path start with anything else (/home , /opt, ...) we're screwed :( */
	if ( (final_name[0]=='/') && (final_name[2]=='/')) {
		final_name[0] = final_name[1];
		final_name[1] = ':';
	}
#endif

	is_chap_file = 0;
	handler = 0;
	do_disable = 0;
	chap_ref = 0;
	is_chap = 0;
	kinds = gf_list_new();
	track_layout = 0;
	szLan = NULL;
	delay.num = delay.den = 0;
	group = 0;
	stype = 0;
	profile = compat = level = 0;
	fullrange = videofmt = colorprim = colortfc = colormx = -1;
	split_tile_mode = 0;
	temporal_mode = 0;
	rap_only = 0;
	refs_only = 0;
	txt_flags = 0;
	max_layer_id_plus_one = max_temporal_id_plus_one = 0;
	force_rate = -1;

	tw = th = tx = ty = tz = txtw = txth = txtx = txty = 0;
	par_d = par_n = -1;
	force_par = rewrite_bs = GF_FALSE;

	ext_start = gf_file_ext_start(final_name);
	ext = strrchr(ext_start ? ext_start : final_name, '#');
	if (!ext) ext = gf_url_colon_suffix(final_name, '=');
	char c_sep = ext ? ext[0] : 0;
	if (ext) ext[0] = 0;
 	if (!strlen(final_name) || !strcmp(final_name, "self")) {
		fake_import = 2;
	}
	if (gf_isom_probe_file(final_name))
		src_is_isom = GF_TRUE;

	if (ext) ext[0] = c_sep;

	ext = gf_url_colon_suffix(final_name, '=');

#define GOTO_EXIT(_msg) if (e) { fail_msg = _msg; goto exit; }

#define CHECK_FAKEIMPORT(_opt) if (fake_import) { M4_LOG(GF_LOG_ERROR, ("Option %s not available for self-reference import\n", _opt)); e = GF_BAD_PARAM; goto exit; }
#define CHECK_FAKEIMPORT_2(_opt) if (fake_import==1) { M4_LOG(GF_LOG_ERROR, ("Option %s not available for self-reference import\n", _opt)); e = GF_BAD_PARAM; goto exit; }


	handler_name = NULL;
	rvc_config = NULL;
	while (ext) {
		char *ext2 = gf_url_colon_suffix(ext+1, '=');

		if (ext2) ext2[0] = 0;

		/*all extensions for track-based importing*/
		if (!strnicmp(ext+1, "dur=", 4)) {
			CHECK_FAKEIMPORT("dur")

			if (strchr(ext, '-')) {
				import.duration.num = atoi(ext+5);
				import.duration.den = 1;
			} else {
				gf_parse_frac(ext+5, &import.duration);
			}
		}
		else if (!strnicmp(ext+1, "start=", 6)) {
			CHECK_FAKEIMPORT("start")
			import.start_time = atof(ext+7);
		}
		else if (!strnicmp(ext+1, "lang=", 5)) {
			/* prevent leak if param is set twice */
			if (szLan)
				gf_free((char*) szLan);

			szLan = gf_strdup(ext+6);
		}
		else if (!strnicmp(ext+1, "delay=", 6)) {
			if (sscanf(ext+7, "%d/%u", &delay.num, &delay.den)!=2) {
				delay.num = atoi(ext+7);
				delay.den = 1000; //in ms
			}
		}
		else if (!strnicmp(ext+1, "par=", 4)) {
			if (!stricmp(ext + 5, "none")) {
				par_n = par_d = 0;
			} else if (!stricmp(ext + 5, "auto")) {
				force_par = GF_TRUE;
			} else if (!stricmp(ext + 5, "force")) {
				par_n = par_d = 1;
				force_par = GF_TRUE;
			} else {
				if (ext2) {
					ext2[0] = ':';
					ext2 = strchr(ext2+1, ':');
					if (ext2) ext2[0] = 0;
				}
				if (ext[5]=='w') {
					rewrite_bs = GF_TRUE;
					sscanf(ext+6, "%d:%d", &par_n, &par_d);
				} else {
					sscanf(ext+5, "%d:%d", &par_n, &par_d);
				}
			}
		}
		else if (!strnicmp(ext+1, "clap=", 5)) {
			if (!stricmp(ext+6, "none")) {
				has_clap=GF_TRUE;
			} else {
				if (sscanf(ext+6, "%d,%d,%d,%d,%d,%d,%d,%d", &clap_wn, &clap_wd, &clap_hn, &clap_hd, &clap_hon, &clap_hod, &clap_von, &clap_vod)==8) {
					has_clap=GF_TRUE;
				}
			}
		}
		else if (!strnicmp(ext+1, "mx=", 3)) {
			if (strstr(ext+4, "0x")) {
				if (sscanf(ext+4, "0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%d", &mx[0], &mx[1], &mx[2], &mx[3], &mx[4], &mx[5], &mx[6], &mx[7], &mx[8])==9) {
					has_mx=GF_TRUE;
				}
			} else if (sscanf(ext+4, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &mx[0], &mx[1], &mx[2], &mx[3], &mx[4], &mx[5], &mx[6], &mx[7], &mx[8])==9) {
				has_mx=GF_TRUE;
			}
		}
		else if (!strnicmp(ext+1, "name=", 5)) {
			handler_name = gf_strdup(ext+6);
		}
		else if (!strnicmp(ext+1, "ext=", 4)) {
			CHECK_FAKEIMPORT("ext")
			/*extensions begin with '.'*/
			if (*(ext+5) == '.')
				import.force_ext = gf_strdup(ext+5);
			else {
				import.force_ext = gf_calloc(1+strlen(ext+5)+1, 1);
				import.force_ext[0] = '.';
				strcat(import.force_ext+1, ext+5);
			}
		}
		else if (!strnicmp(ext+1, "hdlr=", 5)) handler = GF_4CC(ext[6], ext[7], ext[8], ext[9]);
		else if (!strnicmp(ext+1, "stype=", 6)) stype = GF_4CC(ext[7], ext[8], ext[9], ext[10]);
		else if (!strnicmp(ext+1, "tkhd", 4)) {
			char *flags = ext+6;
			if (flags[0]=='+') { track_flags_mode = GF_ISOM_TKFLAGS_ADD; flags += 1; }
			else if (flags[0]=='-') { track_flags_mode = GF_ISOM_TKFLAGS_REM; flags += 1; }
			else track_flags_mode = GF_ISOM_TKFLAGS_SET;

			if (!strnicmp(flags, "0x", 2)) flags += 2;
			sscanf(flags, "%X", &track_flags);
		} else if (!strnicmp(ext+1, "disable", 7)) {
			do_disable = !stricmp(ext+1, "disable=no") ? 2 : 1;
		}
		else if (!strnicmp(ext+1, "group=", 6)) {
			group = atoi(ext+7);
			if (!group) group = gf_isom_get_next_alternate_group_id(dest);
		}
		else if (!strnicmp(ext+1, "fps=", 4)) {
			u32 ticks, dts_inc;
			CHECK_FAKEIMPORT("fps")
			if (!strcmp(ext+5, "auto")) {
				M4_LOG(GF_LOG_ERROR, ("Warning, fps=auto option is deprecated\n"));
			} else if ((sscanf(ext+5, "%u-%u", &ticks, &dts_inc) == 2) || (sscanf(ext+5, "%u/%u", &ticks, &dts_inc) == 2)) {
				if (!dts_inc) dts_inc=1;
				force_fps.num = ticks;
				force_fps.den = dts_inc;
			} else {
				if (gf_sys_old_arch_compat()) {
					force_fps.den = 1000;
					force_fps.num = (u32) (atof(ext+5) * force_fps.den);
				} else {
					gf_parse_frac(ext+5, &force_fps);
				}
			}
		}
		else if (!stricmp(ext+1, "rap")) rap_only = 1;
		else if (!stricmp(ext+1, "refs")) refs_only = 1;
		else if (!stricmp(ext+1, "trailing")) { CHECK_FAKEIMPORT("trailing") import_flags |= GF_IMPORT_KEEP_TRAILING; }
		else if (!strnicmp(ext+1, "agg=", 4)) { CHECK_FAKEIMPORT("agg") frames_per_sample = atoi(ext+5); }
		else if (!stricmp(ext+1, "dref")) { CHECK_FAKEIMPORT("dref")  import_flags |= GF_IMPORT_USE_DATAREF; }
		else if (!stricmp(ext+1, "keep_refs")) { CHECK_FAKEIMPORT("keep_refs") import_flags |= GF_IMPORT_KEEP_REFS; }
		else if (!stricmp(ext+1, "nodrop")) { CHECK_FAKEIMPORT("nodrop") import_flags |= GF_IMPORT_NO_FRAME_DROP; }
		else if (!stricmp(ext+1, "packed")) { CHECK_FAKEIMPORT("packed") import_flags |= GF_IMPORT_FORCE_PACKED; }
		else if (!stricmp(ext+1, "sbr")) { CHECK_FAKEIMPORT("sbr") import_flags |= GF_IMPORT_SBR_IMPLICIT; }
		else if (!stricmp(ext+1, "sbrx")) { CHECK_FAKEIMPORT("sbrx") import_flags |= GF_IMPORT_SBR_EXPLICIT; }
		else if (!stricmp(ext+1, "ovsbr")) { CHECK_FAKEIMPORT("ovsbr") import_flags |= GF_IMPORT_OVSBR; }
		else if (!stricmp(ext+1, "ps")) { CHECK_FAKEIMPORT("ps") import_flags |= GF_IMPORT_PS_IMPLICIT; }
		else if (!stricmp(ext+1, "psx")) { CHECK_FAKEIMPORT("psx") import_flags |= GF_IMPORT_PS_EXPLICIT; }
		else if (!stricmp(ext+1, "mpeg4")) { CHECK_FAKEIMPORT("mpeg4") import_flags |= GF_IMPORT_FORCE_MPEG4; }
		else if (!stricmp(ext+1, "nosei")) { CHECK_FAKEIMPORT("nosei") import_flags |= GF_IMPORT_NO_SEI; }
		else if (!stricmp(ext+1, "svc") || !stricmp(ext+1, "lhvc") ) { CHECK_FAKEIMPORT("svc/lhvc") import_flags |= GF_IMPORT_SVC_EXPLICIT; }
		else if (!stricmp(ext+1, "nosvc") || !stricmp(ext+1, "nolhvc")) { CHECK_FAKEIMPORT("nosvc/nolhvc") import_flags |= GF_IMPORT_SVC_NONE; }

		/*split SVC layers*/
		else if (!strnicmp(ext+1, "svcmode=", 8) || !strnicmp(ext+1, "lhvcmode=", 9)) {
			char *mode = ext+9;
			CHECK_FAKEIMPORT_2("svcmode/lhvcmode")
			if (mode[0]=='=') mode = ext+10;

			if (!stricmp(mode, "splitnox"))
				svc_mode = 3;
			else if (!stricmp(mode, "splitnoxib"))
				svc_mode = 4;
			else if (!stricmp(mode, "splitall") || !stricmp(mode, "split"))
				svc_mode = 2;
			else if (!stricmp(mode, "splitbase"))
				svc_mode = 1;
			else if (!stricmp(mode, "merged") || !stricmp(mode, "merge"))
				svc_mode = 0;
		}
		/*split SHVC temporal sublayers*/
		else if (!strnicmp(ext+1, "temporal=", 9)) {
			char *mode = ext+10;
			CHECK_FAKEIMPORT_2("svcmode/lhvcmode")
			if (!stricmp(mode, "split"))
				temporal_mode = 2;
			else if (!stricmp(mode, "splitnox"))
				temporal_mode = 3;
			else if (!stricmp(mode, "splitbase"))
				temporal_mode = 1;
			else {
				M4_LOG(GF_LOG_ERROR, ("Unrecognized temporal mode %s, ignoring\n", mode));
				temporal_mode = 0;
			}
		}
		else if (!stricmp(ext+1, "subsamples")) { CHECK_FAKEIMPORT("subsamples") import_flags |= GF_IMPORT_SET_SUBSAMPLES; }
		else if (!stricmp(ext+1, "deps")) { CHECK_FAKEIMPORT("deps") import_flags |= GF_IMPORT_SAMPLE_DEPS; }
		else if (!stricmp(ext+1, "ccst")) { CHECK_FAKEIMPORT("ccst") set_ccst = GF_TRUE; }
		else if (!stricmp(ext+1, "alpha")) { CHECK_FAKEIMPORT("alpha") import.is_alpha = GF_TRUE; }
		else if (!stricmp(ext+1, "forcesync")) { CHECK_FAKEIMPORT("forcesync") import_flags |= GF_IMPORT_FORCE_SYNC; }
		else if (!stricmp(ext+1, "xps_inband")) { CHECK_FAKEIMPORT("xps_inband") xps_inband = 1; }
		else if (!stricmp(ext+1, "xps_inbandx")) { CHECK_FAKEIMPORT("xps_inbandx") xps_inband = 2; }
		else if (!stricmp(ext+1, "au_delim")) { CHECK_FAKEIMPORT("au_delim") keep_audelim = GF_TRUE; }
		else if (!strnicmp(ext+1, "max_lid=", 8) || !strnicmp(ext+1, "max_tid=", 8)) {
			s32 val = atoi(ext+9);
			CHECK_FAKEIMPORT_2("max_lid/lhvcmode")
			if (val < 0) {
				M4_LOG(GF_LOG_ERROR, ("Warning: request max layer/temporal id is negative - ignoring\n"));
			} else {
				if (!strnicmp(ext+1, "max_lid=", 8))
					max_layer_id_plus_one = 1 + (u8) val;
				else
					max_temporal_id_plus_one = 1 + (u8) val;
			}
		}
		else if (!stricmp(ext+1, "tiles")) { CHECK_FAKEIMPORT_2("tiles") split_tile_mode = 2; }
		else if (!stricmp(ext+1, "tiles_rle")) { CHECK_FAKEIMPORT_2("tiles_rle") split_tile_mode = 3; }
		else if (!stricmp(ext+1, "split_tiles")) { CHECK_FAKEIMPORT_2("split_tiles") split_tile_mode = 1; }

		/*force all composition offsets to be positive*/
		else if (!strnicmp(ext+1, "negctts", 7)) {
			neg_ctts_mode = !strnicmp(ext+1, "negctts=no", 10) ? 2 : 1;
		}
		else if (!stricmp(ext+1, "chap")) is_chap = 1;
		else if (!strnicmp(ext+1, "chapter=", 8)) {
			chapter_name = gf_strdup(ext+9);
		}
		else if (!strnicmp(ext+1, "chapfile=", 9)) {
			chapter_name = gf_strdup(ext+10);
			is_chap_file=1;
		}
		else if (!strnicmp(ext+1, "layout=", 7)) {
			track_layout = 1;
			if ( sscanf(ext+13, "%dx%dx%dx%dx%d", &tw, &th, &tx, &ty, &tz)==5) {
			} else if ( sscanf(ext+13, "%dx%dx%dx%d", &tw, &th, &tx, &ty)==4) {
				tz = 0;
			} else if ( sscanf(ext+13, "%dx%dx%d", &tw, &th, &tz)==3) {
				tx = ty = 0;
			} else if ( sscanf(ext+8, "%dx%d", &tw, &th)==2) {
				tx = ty = tz = 0;
			}
		}

		else if (!strnicmp(ext+1, "rescale=", 8)) {
			if (sscanf(ext+9, "%u/%u", &rescale_num, &rescale_den) != 2) {
				rescale_num = atoi(ext+9);
				rescale_den = 0;
			}
		}
		else if (!strnicmp(ext+1, "sampdur=", 8)) {
			if (sscanf(ext+9, "%u/%u", &rescale_den, &rescale_num) != 2) {
				rescale_den = atoi(ext+9);
				rescale_num = 0;
			}
			rescale_override = GF_TRUE;
		}
		else if (!strnicmp(ext+1, "timescale=", 10)) {
			new_timescale = atoi(ext+11);
		}
		else if (!strnicmp(ext+1, "moovts=", 7)) {
			moov_timescale = atoi(ext+8);
		}

		else if (!stricmp(ext+1, "noedit")) { import_flags |= GF_IMPORT_NO_EDIT_LIST; }


		else if (!strnicmp(ext+1, "rvc=", 4)) {
			if (sscanf(ext+5, "%d", &rvc_predefined) != 1) {
				rvc_config = gf_strdup(ext+5);
			}
		}
		else if (!strnicmp(ext+1, "fmt=", 4)) import.streamFormat = gf_strdup(ext+5);

		else if (!strnicmp(ext+1, "profile=", 8)) {
			if (!stricmp(ext+9, "high444")) profile = 244;
			else if (!stricmp(ext+9, "high")) profile = 100;
			else if (!stricmp(ext+9, "extended")) profile = 88;
			else if (!stricmp(ext+9, "main")) profile = 77;
			else if (!stricmp(ext+9, "baseline")) profile = 66;
			else profile = atoi(ext+9);
		}
		else if (!strnicmp(ext+1, "level=", 6)) {
			if( atof(ext+7) < 6 )
				level = (int)(10*atof(ext+7)+.5);
			else
				level = atoi(ext+7);
		}
		else if (!strnicmp(ext+1, "compat=", 7)) {
			compat = atoi(ext+8);
		}

		else if (!strnicmp(ext+1, "novpsext", 8)) { CHECK_FAKEIMPORT("novpsext") import_flags |= GF_IMPORT_NO_VPS_EXTENSIONS; }
		else if (!strnicmp(ext+1, "keepav1t", 8)) { CHECK_FAKEIMPORT("keepav1t") import_flags |= GF_IMPORT_KEEP_AV1_TEMPORAL_OBU; }

		else if (!strnicmp(ext+1, "font=", 5)) { CHECK_FAKEIMPORT("font") import.fontName = gf_strdup(ext+6); }
		else if (!strnicmp(ext+1, "size=", 5)) { CHECK_FAKEIMPORT("size") import.fontSize = atoi(ext+6); }
		else if (!strnicmp(ext+1, "text_layout=", 12)) {
			if ( sscanf(ext+13, "%dx%dx%dx%d", &txtw, &txth, &txtx, &txty)==4) {
				text_layout = 1;
			} else if ( sscanf(ext+8, "%dx%d", &txtw, &txth)==2) {
				track_layout = 1;
				txtx = txty = 0;
			}
		}

#ifndef GPAC_DISABLE_SWF_IMPORT
		else if (!stricmp(ext+1, "swf-global")) { CHECK_FAKEIMPORT("swf-global") import.swf_flags |= GF_SM_SWF_STATIC_DICT; }
		else if (!stricmp(ext+1, "swf-no-ctrl")) { CHECK_FAKEIMPORT("swf-no-ctrl") import.swf_flags &= ~GF_SM_SWF_SPLIT_TIMELINE; }
		else if (!stricmp(ext+1, "swf-no-text")) { CHECK_FAKEIMPORT("swf-no-text") import.swf_flags |= GF_SM_SWF_NO_TEXT; }
		else if (!stricmp(ext+1, "swf-no-font")) { CHECK_FAKEIMPORT("swf-no-font") import.swf_flags |= GF_SM_SWF_NO_FONT; }
		else if (!stricmp(ext+1, "swf-no-line")) { CHECK_FAKEIMPORT("swf-no-line") import.swf_flags |= GF_SM_SWF_NO_LINE; }
		else if (!stricmp(ext+1, "swf-no-grad")) { CHECK_FAKEIMPORT("swf-no-grad") import.swf_flags |= GF_SM_SWF_NO_GRADIENT; }
		else if (!stricmp(ext+1, "swf-quad")) { CHECK_FAKEIMPORT("swf-quad") import.swf_flags |= GF_SM_SWF_QUAD_CURVE; }
		else if (!stricmp(ext+1, "swf-xlp")) { CHECK_FAKEIMPORT("swf-xlp") import.swf_flags |= GF_SM_SWF_SCALABLE_LINE; }
		else if (!stricmp(ext+1, "swf-ic2d")) { CHECK_FAKEIMPORT("swf-ic2d") import.swf_flags |= GF_SM_SWF_USE_IC2D; }
		else if (!stricmp(ext+1, "swf-same-app")) { CHECK_FAKEIMPORT("swf-same-app") import.swf_flags |= GF_SM_SWF_REUSE_APPEARANCE; }
		else if (!strnicmp(ext+1, "swf-flatten=", 12)) { CHECK_FAKEIMPORT("swf-flatten") import.swf_flatten_angle = (Float) atof(ext+13); }
#endif

		else if (!strnicmp(ext+1, "kind=", 5)) {
			char *kind_scheme, *kind_value;
			char *kind_data = ext+6;
			char *sep = strchr(kind_data, '=');
			if (sep) {
				*sep = 0;
			}
			kind_scheme = gf_strdup(kind_data);
			if (sep) {
				*sep = '=';
				kind_value = gf_strdup(sep+1);
			} else {
				kind_value = NULL;
			}
			gf_list_add(kinds, kind_scheme);
			gf_list_add(kinds, kind_value);
		}
		else if (!strnicmp(ext+1, "txtflags", 8)) {
			if (!strnicmp(ext+1, "txtflags=", 9)) {
				sscanf(ext+10, "%x", &txt_flags);
			}
			else if (!strnicmp(ext+1, "txtflags+=", 10)) {
				sscanf(ext+11, "%x", &txt_flags);
				txt_mode = GF_ISOM_TEXT_FLAGS_TOGGLE;
			}
			else if (!strnicmp(ext+1, "txtflags-=", 10)) {
				sscanf(ext+11, "%x", &txt_flags);
				txt_mode = GF_ISOM_TEXT_FLAGS_UNTOGGLE;
			}
		}
		else if (!strnicmp(ext+1, "rate=", 5)) {
			force_rate = atoi(ext+6);
		}
		else if (!stricmp(ext+1, "stats") || !stricmp(ext+1, "fstat"))
			print_stats_graph |= 1;
		else if (!stricmp(ext+1, "graph") || !stricmp(ext+1, "graph"))
			print_stats_graph |= 2;
		else if (!strncmp(ext+1, "sopt", 4) || !strncmp(ext+1, "dopt", 4) || !strncmp(ext+1, "@", 1)) {
			if (ext2) ext2[0] = ':';
			opt_src = strstr(ext, ":sopt:");
			opt_dst = strstr(ext, ":dopt:");
			fchain = strstr(ext, ":@");
			if (opt_src) opt_src[0] = 0;
			if (opt_dst) opt_dst[0] = 0;
			if (fchain) fchain[0] = 0;

			if (opt_src) import.filter_src_opts = opt_src+6;
			if (opt_dst) import.filter_dst_opts = opt_dst+6;
			if (fchain) {
				//check for old syntax (0.9->1.0) :@@
				if (fchain[2]=='@') {
					import.filter_chain = fchain + 3;
					import.is_chain_old_syntax = GF_TRUE;
				} else {
					import.filter_chain = fchain + 2;
					import.is_chain_old_syntax = GF_FALSE;
				}
			}

			ext = NULL;
			break;
		}

		else if (!strnicmp(ext+1, "asemode=", 8)){
			char *mode = ext+9;
			if (!stricmp(mode, "v0-bs"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_BS;
			else if (!stricmp(mode, "v0-2"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2;
			else if (!stricmp(mode, "v1"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG;
			else if (!stricmp(mode, "v1-qt"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF;
			else
				M4_LOG(GF_LOG_ERROR, ("Unrecognized audio sample entry mode %s, ignoring\n", mode));
		}
		else if (!strnicmp(ext+1, "audio_roll=", 11)) { roll_change = 3; roll = atoi(ext+12); }
		else if (!strnicmp(ext+1, "roll=", 5)) { roll_change = 1; roll = atoi(ext+6); }
		else if (!strnicmp(ext+1, "proll=", 6)) { roll_change = 2; roll = atoi(ext+7); }
		else if (!strcmp(ext+1, "stz2")) {
			use_stz2 = GF_TRUE;
		} else if (!strnicmp(ext+1, "bitdepth=", 9)) {
			bitdepth=atoi(ext+10);
		}
		else if (!strnicmp(ext+1, "colr=", 5)) {
			char *cval = ext+6;
			if (strlen(cval)<6) {
				fmt_ok = GF_FALSE;
			} else {
				clr_type = GF_4CC(cval[0],cval[1],cval[2],cval[3]);
				cval+=4;
				if (cval[0] != ',') {
					fmt_ok = GF_FALSE;
				}
				else if ((clr_type==GF_ISOM_SUBTYPE_NCLX) || (clr_type==GF_ISOM_SUBTYPE_NCLC)) {
					fmt_ok = scan_color(cval+1, &clr_prim, &clr_tranf, &clr_mx, &clr_full_range);
				}
				else if ((clr_type==GF_ISOM_SUBTYPE_RICC) || (clr_type==GF_ISOM_SUBTYPE_PROF)) {
					FILE *f = gf_fopen(cval+1, "rb");
					if (!f) {
						M4_LOG(GF_LOG_ERROR, ("Failed to open file %s\n", cval+1));
						e = GF_BAD_PARAM;
						goto exit;
					} else {
						gf_fseek(f, 0, SEEK_END);
						icc_size = (u32) gf_ftell(f);
						icc_data = gf_malloc(sizeof(char)*icc_size);
						gf_fseek(f, 0, SEEK_SET);
						icc_size = (u32) gf_fread(icc_data, icc_size, f);
						gf_fclose(f);
					}
				} else {
					M4_LOG(GF_LOG_ERROR, ("Unrecognized colr profile %s\n", gf_4cc_to_str(clr_type) ));
					e = GF_BAD_PARAM;
					goto exit;
				}
			}
			if (!fmt_ok) {
				e = GF_BAD_PARAM;
				GOTO_EXIT("parsing colr option");
			}
		}
		else if (!strnicmp(ext + 1, "dv-profile=", 11)) {
			strncpy(dv_profile, ext + 12, 100);
		}
		else if (!strnicmp(ext+1, "fullrange=", 10)) {
			if (!stricmp(ext+11, "off") || !stricmp(ext+11, "no")) fullrange = 0;
			else if (!stricmp(ext+11, "on") || !stricmp(ext+11, "yes")) fullrange = 1;
			else {
				e = GF_BAD_PARAM;
				GOTO_EXIT("invalid format for fullrange")
			}
		}
		else if (!strnicmp(ext+1, "videofmt=", 10)) {
			u32 idx, count = GF_ARRAY_LENGTH(videofmt_names);
			for (idx=0; idx<count; idx++) {
				if (!strcmp(ext+11, videofmt_names[idx])) {
					videofmt = idx;
					break;
				}
			}
			if (videofmt==-1) {
				e = GF_BAD_PARAM;
				GOTO_EXIT("invalid format for videofmt")
			}
		}
		else if (!strnicmp(ext+1, "colorprim=", 10)) {
			colorprim = gf_cicp_parse_color_primaries(ext+11);
			if (colorprim==-1) {
				e = GF_BAD_PARAM;
				GOTO_EXIT("invalid format for colorprim")
			}
		}
		else if (!strnicmp(ext+1, "colortfc=", 9)) {
			colortfc = gf_cicp_parse_color_transfer(ext+10);
			if (colortfc==-1) {
				e = GF_BAD_PARAM;
				GOTO_EXIT("invalid format for colortfc")
			}
		}
		else if (!strnicmp(ext+1, "colormx=", 10)) {
			colormx = gf_cicp_parse_color_matrix(ext+11);
			if (colormx==-1) {
				e = GF_BAD_PARAM;
				GOTO_EXIT("invalid format for colormx")
			}
		}
		else if (!strnicmp(ext+1, "tc=", 3)) {
			char *tc_str = ext+4;
			
			if (tc_str[0] == 'd') {
				tc_drop_frame=GF_TRUE;
				tc_str+=1;
			}
			if (sscanf(tc_str, "%d/%d,%d,%d,%d,%d,%d", &tc_fps_num, &tc_fps_den, &tc_h, &tc_m, &tc_s, &tc_f, &tc_frames_per_tick) == 7) {
			} else if (sscanf(tc_str, "%d/%d,%d,%d,%d,%d", &tc_fps_num, &tc_fps_den, &tc_h, &tc_m, &tc_s, &tc_f) == 6) {
			} else if (sscanf(tc_str, "%d,%d,%d,%d,%d,%d", &tc_fps_num, &tc_h, &tc_m, &tc_s, &tc_f, &tc_frames_per_tick) == 6) {
				tc_fps_den = 1;
			} else if (sscanf(tc_str, "%d,%d,%d,%d,%d", &tc_fps_num, &tc_h, &tc_m, &tc_s, &tc_f) == 5) {
				tc_fps_den = 1;
			} else if (sscanf(tc_str, "%d/%d,%d,%d", &tc_fps_num, &tc_fps_den, &tc_f, &tc_frames_per_tick) == 4) {
				tc_force_counter = GF_TRUE;
				tc_h = tc_m = tc_s = 0;
			} else if (sscanf(tc_str, "%d/%d,%d", &tc_fps_num, &tc_fps_den, &tc_f) == 3) {
				tc_force_counter = GF_TRUE;
				tc_h = tc_m = tc_s = 0;
			} else if (sscanf(tc_str, "%d,%d,%d", &tc_fps_num, &tc_f, &tc_frames_per_tick) == 3) {
				tc_force_counter = GF_TRUE;
				tc_h = tc_m = tc_s = 0;
				tc_fps_den = 1;
			} else if (sscanf(tc_str, "%d,%d", &tc_fps_num, &tc_f) == 2) {
				tc_force_counter = GF_TRUE;
				tc_h = tc_m = tc_s = 0;
				tc_fps_den = 1;
			} else {
				M4_LOG(GF_LOG_ERROR, ("Bad format %s for timecode, ignoring\n", ext+1));
			}
		}
		else if (!strnicmp(ext+1, "edits=", 6)) {
			edits = gf_strdup(ext+7);
		}
		else if (!strnicmp(ext+1, "lastsampdur", 11)) {
			has_last_sample_dur = GF_TRUE;
			if (!strnicmp(ext+1, "lastsampdur=", 12)) {
				if (sscanf(ext+13, "%d/%u", &last_sample_dur.num, &last_sample_dur.den)==2) {
				} else {
					last_sample_dur.num = atoi(ext+13);
					last_sample_dur.den = 1000;
				}
			}
		}
		/*unrecognized, assume name has colon in it*/
		else {
			M4_LOG(GF_LOG_ERROR, ("Unrecognized import option %s, ignoring\n", ext+1));
			if (ext2) ext2[0] = ':';
			ext = ext2;
			continue;
		}
		if (src_is_isom) {
			char *opt = ext+1;
			char *sep_eq = strchr(opt, '=');
			if (sep_eq) sep_eq[0] = 0;
			if (!mp4box_check_isom_fileopt(opt)) {
				M4_LOG(GF_LOG_ERROR, ("\t! Import option `%s` not available for ISOBMFF/QT sources, ignoring !\n", ext+1));
			}
			if (sep_eq) sep_eq[0] = '=';
		}

		if (ext2) ext2[0] = ':';

		ext[0] = 0;

		/* restart from where we stopped
		 * if we didn't stop (ext2 null) then the end has been reached
		 * so we can stop the whole thing */
		ext = ext2;
	}

	/*check duration import (old syntax)*/
	ext = strrchr(final_name, '%');
	if (ext) {
		gf_parse_frac(ext+1, &import.duration);
		ext[0] = 0;
	}

	/*select switches for av containers import*/
	do_audio = do_video = do_auxv = do_pict = 0;
	track_id = prog_id = 0;
	do_all = 1;

	ext_start = gf_file_ext_start(final_name);
	ext = strrchr(ext_start ? ext_start : final_name, '#');
	if (ext) ext[0] = 0;

	if (fake_import && ext) {
		ext++;
		if (!strnicmp(ext, "audio", 5)) do_audio = 1;
		else if (!strnicmp(ext, "video", 5)) do_video = 1;
		else if (!strnicmp(ext, "auxv", 4)) do_auxv = 1;
		else if (!strnicmp(ext, "pict", 4)) do_pict = 1;
		else if (!strnicmp(ext, "trackID=", 8)) track_id = atoi(&ext[8]);
		else track_id = atoi(ext);
	}
	else if (ext) {
		ext++;
		char *sep = gf_url_colon_suffix(ext, '=');
		if (sep) sep[0] = 0;

		//we have a fragment, we need to check if the track or the program is present in source
		import.in_name = final_name;
		import.flags = GF_IMPORT_PROBE_ONLY;
		e = gf_media_import(&import);
		GOTO_EXIT("importing import");

		if (!strnicmp(ext, "audio", 5)) do_audio = 1;
		else if (!strnicmp(ext, "video", 5)) do_video = 1;
        else if (!strnicmp(ext, "auxv", 4)) do_auxv = 1;
        else if (!strnicmp(ext, "pict", 4)) do_pict = 1;
		else if (!strnicmp(ext, "trackID=", 8)) track_id = atoi(&ext[8]);
		else if (!strnicmp(ext, "PID=", 4)) track_id = atoi(&ext[4]);
		else if (!strnicmp(ext, "program=", 8)) {
			for (i=0; i<import.nb_progs; i++) {
				if (!stricmp(import.pg_info[i].name, ext+8)) {
					prog_id = import.pg_info[i].number;
					do_all = 0;
					break;
				}
			}
		}
		else if (!strnicmp(ext, "prog_id=", 8)) {
			prog_id = atoi(ext+8);
			do_all = 0;
		}
		else track_id = atoi(ext);

		//figure out trackID
		if (do_audio || do_video || do_auxv || do_pict || track_id) {
			Bool found = track_id ? GF_FALSE : GF_TRUE;
			for (i=0; i<import.nb_tracks; i++) {
				if (track_id && (import.tk_info[i].track_num==track_id)) {
					found=GF_TRUE;
					break;
				}
				if (do_audio && (import.tk_info[i].stream_type==GF_STREAM_AUDIO)) {
					track_id = import.tk_info[i].track_num;
					break;
				}
				else if (do_video && (import.tk_info[i].stream_type==GF_STREAM_VISUAL)) {
					track_id = import.tk_info[i].track_num;
					break;
				}
				else if (do_auxv && (import.tk_info[i].media_subtype==GF_ISOM_MEDIA_AUXV)) {
					track_id = import.tk_info[i].track_num;
					break;
				}
				else if (do_pict && (import.tk_info[i].media_subtype==GF_ISOM_MEDIA_PICT)) {
					track_id = import.tk_info[i].track_num;
					break;
				}
			}
			if (!track_id || !found) {
				M4_LOG(GF_LOG_ERROR, ("Cannot find track ID matching fragment #%s\n", ext));
				if (sep) sep[0] = ':';
				e = GF_NOT_FOUND;
				goto exit;
			}
		}
		if (sep) sep[0] = ':';
	}
	if (do_audio || do_video || do_auxv || do_pict || track_id) do_all = 0;

	if (track_layout || is_chap) {
		u32 w, h, sw, sh, fw, fh;
		w = h = sw = sh = fw = fh = 0;
		chap_ref = 0;
		for (i=0; i<gf_isom_get_track_count(dest); i++) {
			switch (gf_isom_get_media_type(dest, i+1)) {
			case GF_ISOM_MEDIA_SCENE:
			case GF_ISOM_MEDIA_VISUAL:
            case GF_ISOM_MEDIA_AUXV:
            case GF_ISOM_MEDIA_PICT:
				if (!chap_ref && gf_isom_is_track_enabled(dest, i+1) ) chap_ref = i+1;

				gf_isom_get_visual_info(dest, i+1, 1, &sw, &sh);
				gf_isom_get_track_layout_info(dest, i+1, &fw, &fh, NULL, NULL, NULL);
				if (w<sw) w = sw;
				if (w<fw) w = fw;
				if (h<sh) h = sh;
				if (h<fh) h = fh;
				break;
			case GF_ISOM_MEDIA_AUDIO:
				if (!chap_ref && gf_isom_is_track_enabled(dest, i+1) ) chap_ref = i+1;
				break;
			}
		}
		if (track_layout) {
			if (!tw) tw = w;
			if (!th) th = h;
			if (ty==-1) ty = (h>(u32)th) ? h-th : 0;
			import.text_width = tw;
			import.text_height = th;
		}
		if (is_chap && chap_ref) import_flags |= GF_IMPORT_NO_TEXT_FLUSH;
	}
	if (text_layout && txtw && txth) {
		import.text_track_width = import.text_width ? import.text_width : txtw;
		import.text_track_height = import.text_height ? import.text_height : txth;
		import.text_width = txtw;
		import.text_height = txth;
		import.text_x = txtx;
		import.text_y = txty;
	}

	check_track_for_svc = check_track_for_lhvc = check_track_for_hevc = 0;

	source_magic = (u64) gf_crc_32((u8 *)inName, (u32) strlen(inName));
	if (!fake_import && (!fsess || mux_args_if_first_pass)) {
		import.in_name = final_name;
		import.dest = dest;
		import.video_fps = force_fps;
		import.frames_per_sample = frames_per_sample;
		import.flags = import_flags;
		import.keep_audelim = keep_audelim;
		import.print_stats_graph = print_stats_graph;
		import.xps_inband = xps_inband;
		import.prog_id = prog_id;
		import.trackID = track_id;
		import.source_magic = source_magic;
		import.track_index = tk_idx;

		//if moov timescale is <0 (auto mode) set it at import time
		if (moov_timescale<0) {
			import.moov_timescale = moov_timescale;
		}
		//otherwise force it now
		else if (moov_timescale>0) {
			e = gf_isom_set_timescale(dest, moov_timescale);
			GOTO_EXIT("changing timescale")
		}

		import.run_in_session = fsess;
		import.update_mux_args = NULL;
		if (do_all)
			import.flags |= GF_IMPORT_KEEP_REFS;

		e = gf_media_import(&import);
		if (e) {
			if (import.update_mux_args) gf_free(import.update_mux_args);
			GOTO_EXIT("importing media");
		}

		if (fsess) {
			*mux_args_if_first_pass = import.update_mux_args;
			import.update_mux_args = NULL;
			*mux_sid_if_first_pass = import.update_mux_sid;
			import.update_mux_sid = NULL;
			goto exit;
		}
	}

	nb_tracks = gf_isom_get_track_count(dest);
	for (i=0; i<nb_tracks; i++) {
		u32 media_type;
		track = i+1;
		media_type = gf_isom_get_media_type(dest, track);
		e = GF_OK;
		if (!fake_import) {
			u64 tk_source_magic;
			tk_source_magic = gf_isom_get_track_magic(dest, track);

			if ((tk_source_magic & 0xFFFFFFFFUL) != source_magic)
				continue;
			tk_source_magic>>=32;		
			keep_handler = (tk_source_magic & 1) ? GF_TRUE : GF_FALSE;
		} else {
			keep_handler = GF_TRUE;

			if (do_audio && (media_type!=GF_ISOM_MEDIA_AUDIO)) continue;
			if (do_video && (media_type!=GF_ISOM_MEDIA_VISUAL)) continue;
			if (do_auxv && (media_type!=GF_ISOM_MEDIA_AUXV)) continue;
			if (do_pict && (media_type!=GF_ISOM_MEDIA_PICT)) continue;
			if (track_id && (gf_isom_get_track_id(dest, track) != track_id))
				continue;
		}

		timescale = gf_isom_get_timescale(dest);
		if (szLan) {
			e = gf_isom_set_media_language(dest, track, (char *) szLan);
			GOTO_EXIT("changing language")
		}
		if (do_disable) {
			e = gf_isom_set_track_enabled(dest, track, (do_disable==2) ? GF_TRUE : GF_FALSE);
			GOTO_EXIT("disabling track")
		}
		if (track_flags_mode) {
			e = gf_isom_set_track_flags(dest, track, track_flags, track_flags_mode);
			GOTO_EXIT("disabling track")
		}

		if (import_flags & GF_IMPORT_NO_EDIT_LIST) {
			e = gf_isom_remove_edits(dest, track);
			GOTO_EXIT("removing edits")
		}
		if (delay.num && delay.den) {
			u64 tk_dur;
			e = gf_isom_remove_edits(dest, track);
			tk_dur = gf_isom_get_track_duration(dest, track);
			if (delay.num>0) {
				//cast to s64, timescale*delay could be quite large before /1000
				e |= gf_isom_append_edit(dest, track, ((s64) delay.num) * timescale / delay.den, 0, GF_ISOM_EDIT_EMPTY);
				e |= gf_isom_append_edit(dest, track, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
			} else {
					//cast to s64, timescale*delay could be quite large before /1000
				u64 to_skip = ((s64) -delay.num) * timescale / delay.den;
				if (to_skip<tk_dur) {
					//cast to s64, timescale*delay could be quite large before /1000
					u64 media_time = ((s64) -delay.num) * gf_isom_get_media_timescale(dest, track) / delay.den;
					e |= gf_isom_append_edit(dest, track, tk_dur-to_skip, media_time, GF_ISOM_EDIT_NORMAL);
				} else {
					M4_LOG(GF_LOG_ERROR, ("Warning: request negative delay longer than track duration - ignoring\n"));
				}
			}
			GOTO_EXIT("assigning delay")
		}
		if (gf_isom_is_video_handler_type(media_type)) {
			if (((par_n>=0) && (par_d>=0)) || force_par) {
				e = gf_media_change_par(dest, track, par_n, par_d, force_par, rewrite_bs);
				GOTO_EXIT("changing PAR")
			}
			if ((fullrange>=0) || (videofmt>=0) || (colorprim>=0) || (colortfc>=0) || (colormx>=0)) {
				e = gf_media_change_color(dest, i+1, fullrange, videofmt, colorprim, colortfc, colormx);
				GOTO_EXIT("changing color in bitstream")
			}
			if (has_clap) {
				e = gf_isom_set_clean_aperture(dest, track, 1, clap_wn, clap_wd, clap_hn, clap_hd, clap_hon, clap_hod, clap_von, clap_vod);
				GOTO_EXIT("changing clean aperture")
			}
			if (bitdepth) {
				e = gf_isom_set_visual_bit_depth(dest, track, 1, bitdepth);
				GOTO_EXIT("changing bit depth")
			}
			if (clr_type) {
				e = gf_isom_set_visual_color_info(dest, track, 1, clr_type, clr_prim, clr_tranf, clr_mx, clr_full_range, icc_data, icc_size);
				GOTO_EXIT("changing color info")
			}
			if (dv_profile[0]) {
				e = set_dv_profile(dest, track, dv_profile);
				GOTO_EXIT("setting DV profile")
			}

			if (set_ccst) {
				e = gf_isom_set_image_sequence_coding_constraints(dest, track, 1, GF_FALSE, GF_FALSE, GF_TRUE, 15);
				GOTO_EXIT("setting image sequence constraints")
			}
		}
		if (has_mx) {
			e = gf_isom_set_track_matrix(dest, track, mx);
			GOTO_EXIT("setting track matrix")
		}
		if (use_stz2) {
			e = gf_isom_use_compact_size(dest, track, GF_TRUE);
			GOTO_EXIT("setting compact size")
		}

		if (gf_isom_get_media_subtype(dest, track, 1) == GF_ISOM_MEDIA_TIMECODE) {
			tmcd_track = track;
		}
		if (rap_only || refs_only) {
			e = gf_media_remove_non_rap(dest, track, refs_only);
			GOTO_EXIT("removing non RAPs")
		}
		if (handler_name) {
			e = gf_isom_set_handler_name(dest, track, handler_name);
			GOTO_EXIT("setting handler name")
		}
		else if (!keep_handler) {
			char szHName[1024];
			const char *fName = gf_url_get_resource_name((const  char *)inName);
			fName = strchr(fName, '.');
			if (fName) fName += 1;
			else fName = "?";

			sprintf(szHName, "%s@GPAC%s", fName, gf_gpac_version());
			e = gf_isom_set_handler_name(dest, track, szHName);
			GOTO_EXIT("setting handler name")
		}
		if (handler) {
			e = gf_isom_set_media_type(dest, track, handler);
			GOTO_EXIT("setting media type")
		}
		if (group) {
			e = gf_isom_set_alternate_group_id(dest, track, group);
			GOTO_EXIT("setting alternate group")
		}

		if (track_layout) {
			e = gf_isom_set_track_layout_info(dest, track, tw<<16, th<<16, tx<<16, ty<<16, tz);
			GOTO_EXIT("setting track layout")
		}
		if (stype) {
			e = gf_isom_set_media_subtype(dest, track, 1, stype);
			GOTO_EXIT("setting media subtype")
		}
		if (is_chap && chap_ref) {
			e = set_chapter_track(dest, track, chap_ref);
			GOTO_EXIT("setting chapter track")
		}

		for (j = 0; j < gf_list_count(kinds); j+=2) {
			char *kind_scheme = (char *)gf_list_get(kinds, j);
			char *kind_value = (char *)gf_list_get(kinds, j+1);
			e = gf_isom_add_track_kind(dest, i+1, kind_scheme, kind_value);
			GOTO_EXIT("setting track kind")
		}

		if (profile || compat || level) {
			e = gf_media_change_pl(dest, track, profile, compat, level);
			GOTO_EXIT("changing video PL")
		}
		if (gf_isom_get_mpeg4_subtype(dest, track, 1))
			keep_sys_tracks = 1;

		//if moov timescale is <0 (auto mode) set it at import time
		if (fake_import) {
			if (import_flags & GF_IMPORT_NO_EDIT_LIST)
				gf_isom_remove_edits(dest, track);

			if (moov_timescale<0) {
				moov_timescale = gf_isom_get_media_timescale(dest, track);
			}
			if (moov_timescale>0) {
				e = gf_isom_set_timescale(dest, moov_timescale);
				GOTO_EXIT("changing timescale")
			}

			if (import.asemode && (media_type==GF_ISOM_MEDIA_AUDIO)) {
				u32 sr, ch, bps;
				gf_isom_get_audio_info(dest, track, 1, &sr, &ch, &bps);
				gf_isom_set_audio_info(dest, track, 1, sr, ch, bps, import.asemode);
			}
		}

		if (roll_change) {
			if ((roll_change!=3) || (media_type==GF_ISOM_MEDIA_AUDIO)) {
				e = gf_isom_set_sample_roll_group(dest, track, (u32) -1, (roll_change==2) ? GF_ISOM_SAMPLE_PREROLL : GF_ISOM_SAMPLE_ROLL, roll);
				GOTO_EXIT("assigning roll")
			}
		}

		if (new_timescale>1) {
			e = gf_isom_set_media_timescale(dest, track, new_timescale, 0, 0);
			GOTO_EXIT("setting media timescale")
		}

		if (rescale_num > 1) {
			switch (gf_isom_get_media_type(dest, track)) {
			case GF_ISOM_MEDIA_AUDIO:
				if (!rescale_override) {
					M4_LOG(GF_LOG_WARNING, ("Cannot force media timescale for audio media types - ignoring\n"));
					break;
				}
			default:
				e = gf_isom_set_media_timescale(dest, track, rescale_num, rescale_den, rescale_override ? 2 : 1);
                if (e==GF_EOS) {
					M4_LOG(GF_LOG_WARNING, ("Rescale ignored, same config in source file\n"));
					e = GF_OK;
				}
				GOTO_EXIT("rescaling media track")
				break;
			}
		}

		if (has_last_sample_dur) {
			e = gf_isom_set_last_sample_duration_ex(dest, track, last_sample_dur.num, last_sample_dur.den);
			GOTO_EXIT("setting last sample duration")
		}
		if (rvc_config) {
#ifdef GPAC_DISABLE_ZLIB
			M4_LOG(GF_LOG_ERROR, ("Error: no zlib support - RVC not available\n"));
			e = GF_NOT_SUPPORTED;
			goto exit;
#else
			u8 *data;
			u32 size;
			e = gf_file_load_data(rvc_config, (u8 **) &data, &size);
			GOTO_EXIT("loading RVC config file")

			gf_gz_compress_payload(&data, size, &size);
			e |= gf_isom_set_rvc_config(dest, track, 1, 0, "application/rvc-config+xml+gz", data, size);
			gf_free(data);
			GOTO_EXIT("compressing and assigning RVC config")
#endif
		} else if (rvc_predefined>0) {
			e = gf_isom_set_rvc_config(dest, track, 1, rvc_predefined, NULL, NULL, 0);
			GOTO_EXIT("setting RVC predefined config")
		}

		if (neg_ctts_mode) {
			e = gf_isom_set_composition_offset_mode(dest, track, (neg_ctts_mode==1) ? GF_TRUE : GF_FALSE);
			GOTO_EXIT("setting composition offset mode")
		}

		if (gf_isom_get_avc_svc_type(dest, track, 1)>=GF_ISOM_AVCTYPE_AVC_SVC)
			check_track_for_svc = track;

		switch (gf_isom_get_hevc_lhvc_type(dest, track, 1)) {
		case GF_ISOM_HEVCTYPE_HEVC_LHVC:
		case GF_ISOM_HEVCTYPE_LHVC_ONLY:
			check_track_for_lhvc = i+1;
			break;
		case GF_ISOM_HEVCTYPE_HEVC_ONLY:
			check_track_for_hevc=1;
			break;
		default:
			break;
		}

		if (txt_flags) {
			e = gf_isom_text_set_display_flags(dest, track, 0, txt_flags, txt_mode);
			GOTO_EXIT("setting text track display flags")
		}

		if (edits) {
			e = apply_edits(dest, track, edits);
			GOTO_EXIT("applying edits")
		}

		if (force_rate>=0) {
			e = gf_isom_update_bitrate(dest, i+1, 1, force_rate, force_rate, 0);
			GOTO_EXIT("updating bitrate")
		}

		if (split_tile_mode) {
			switch (gf_isom_get_media_subtype(dest, track, 1)) {
			case GF_ISOM_SUBTYPE_HVC1:
			case GF_ISOM_SUBTYPE_HEV1:
			case GF_ISOM_SUBTYPE_HVC2:
			case GF_ISOM_SUBTYPE_HEV2:
				break;
			default:
				split_tile_mode = 0;
				break;
			}
		}
	}

	if (chapter_name) {
		if (is_chap_file) {
			GF_Fraction a_fps = {0,0};
			e = gf_media_import_chapters(dest, chapter_name, a_fps, GF_FALSE);
		} else {
			e = gf_isom_add_chapter(dest, 0, 0, chapter_name);
		}
		GOTO_EXIT("importing chapters")
	}

	if (tmcd_track) {
		u32 tmcd_id = gf_isom_get_track_id(dest, tmcd_track);
		for (i=0; i < gf_isom_get_track_count(dest); i++) {
			switch (gf_isom_get_media_type(dest, i+1)) {
			case GF_ISOM_MEDIA_VISUAL:
			case GF_ISOM_MEDIA_AUXV:
			case GF_ISOM_MEDIA_PICT:
				break;
			default:
				continue;
			}
			e = gf_isom_set_track_reference(dest, i+1, GF_ISOM_REF_TMCD, tmcd_id);
			GOTO_EXIT("assigning TMCD track references")
		}
	}

	/*force to rewrite all dependencies*/
	for (i = 1; i <= gf_isom_get_track_count(dest); i++)
	{
		e = gf_isom_rewrite_track_dependencies(dest, i);
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Warning: track ID %d has references to a track not imported\n", gf_isom_get_track_id(dest, i) ));
			e = GF_OK;
		}
	}

#ifndef GPAC_DISABLE_AV_PARSERS
	if (max_layer_id_plus_one || max_temporal_id_plus_one) {
		for (i = 1; i <= gf_isom_get_track_count(dest); i++)
		{
			e = gf_media_filter_hevc(dest, i, max_temporal_id_plus_one, max_layer_id_plus_one);
			if (e) {
				M4_LOG(GF_LOG_ERROR, ("Warning: track ID %d: error while filtering LHVC layers\n", gf_isom_get_track_id(dest, i)));
				e = GF_OK;
			}
		}
	}
#endif

	if (check_track_for_svc) {
		if (svc_mode) {
			e = gf_media_split_svc(dest, check_track_for_svc, (svc_mode==2) ? 1 : 0);
			GOTO_EXIT("splitting SVC track")
		} else {
			e = gf_media_merge_svc(dest, check_track_for_svc, 1);
			GOTO_EXIT("merging SVC/SHVC track")
		}
	}
#ifndef GPAC_DISABLE_AV_PARSERS
	if (check_track_for_lhvc) {
		if (svc_mode) {
			GF_LHVCExtractoreMode xmode = GF_LHVC_EXTRACTORS_ON;
			if (svc_mode==3) xmode = GF_LHVC_EXTRACTORS_OFF;
			else if (svc_mode==4) xmode = GF_LHVC_EXTRACTORS_OFF_FORCE_INBAND;
			e = gf_media_split_lhvc(dest, check_track_for_lhvc, GF_FALSE, (svc_mode==1) ? 0 : 1, xmode );
			GOTO_EXIT("splitting L-HEVC track")
		} else {
			//TODO - merge, temporal sublayers
		}
	}
#ifndef GPAC_DISABLE_HEVC
	if (check_track_for_hevc) {
		if (split_tile_mode) {
			e = gf_media_split_hevc_tiles(dest, split_tile_mode - 1);
			GOTO_EXIT("splitting HEVC tiles")
		}
		if (temporal_mode) {
			GF_LHVCExtractoreMode xmode = (temporal_mode==3) ? GF_LHVC_EXTRACTORS_OFF : GF_LHVC_EXTRACTORS_ON;
			e = gf_media_split_lhvc(dest, check_track_for_hevc, GF_TRUE, (temporal_mode==1) ? GF_FALSE : GF_TRUE, xmode );
			GOTO_EXIT("splitting HEVC temporal sublayers")
		}
	}
#endif

	if (tc_fps_num) {
		u32 desc_index=0;
		u32 tmcd_tk, tmcd_id;
		u32 video_ref = 0;
		GF_BitStream *bs;
		GF_ISOSample *samp;
		for (i=0; i<gf_isom_get_track_count(dest); i++) {
			if (gf_isom_is_video_handler_type(gf_isom_get_media_type(dest, i+1))) {
				video_ref = i+1;
				break;
			}
		}
		tmcd_tk = gf_isom_new_track(dest, 0, GF_ISOM_MEDIA_TIMECODE, tc_fps_num);
		if (!tmcd_tk) {
			e = gf_isom_last_error(dest);
			GOTO_EXIT("creating TMCD track")
		}
		e = gf_isom_set_track_enabled(dest, tmcd_tk, 1);
		if (e != GF_OK) {
			GOTO_EXIT("enabling TMCD track")
		}

		if (!tc_frames_per_tick) {
			tc_frames_per_tick = tc_fps_num;
			tc_frames_per_tick /= tc_fps_den;
			if (tc_frames_per_tick * tc_fps_den < tc_fps_num)
				tc_frames_per_tick++;
		}

		u32 tmcd_value = (tc_h * 3600 + tc_m*60 + tc_s)*tc_frames_per_tick+tc_f;
		tmcd_id = gf_isom_get_track_id(dest, tmcd_tk);

		e = gf_isom_tmcd_config_new(dest, tmcd_tk, tc_fps_num, tc_fps_den, tc_frames_per_tick, tc_drop_frame, tc_force_counter, &desc_index);
		GOTO_EXIT("configuring TMCD sample description")

		if (video_ref) {
			e = gf_isom_set_track_reference(dest, video_ref, GF_ISOM_REF_TMCD, tmcd_id);
			GOTO_EXIT("assigning TMCD track ref on video track")
		}
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u32(bs, tmcd_value);
		samp = gf_isom_sample_new();
		samp->IsRAP = SAP_TYPE_1;
		gf_bs_get_content(bs, &samp->data, &samp->dataLength);
		gf_bs_del(bs);
		e = gf_isom_add_sample(dest, tmcd_tk, desc_index, samp);
		gf_isom_sample_del(&samp);
		GOTO_EXIT("assigning TMCD sample")

		if (video_ref) {
			u64 video_ref_dur = gf_isom_get_media_duration(dest, video_ref);
			video_ref_dur *= tc_fps_num;
			video_ref_dur /= gf_isom_get_media_timescale(dest, video_ref);
			e = gf_isom_set_last_sample_duration(dest, tmcd_tk, (u32) video_ref_dur);
		} else {
			e = gf_isom_set_last_sample_duration(dest, tmcd_tk, tc_fps_den ? tc_fps_den : 1);
		}
		GOTO_EXIT("setting TMCD sample dur")
	}

#endif /*GPAC_DISABLE_AV_PARSERS*/

exit:
	while (gf_list_count(kinds)) {
		char *kind = (char *)gf_list_get(kinds, 0);
		gf_list_rem(kinds, 0);
		if (kind) gf_free(kind);
	}
	if (opt_src) opt_src[0] = ':';
	if (opt_dst) opt_dst[0] = ':';
	if (fchain) fchain[0] = ':';

	gf_list_del(kinds);
	if (handler_name) gf_free(handler_name);
	if (chapter_name ) gf_free(chapter_name);
	if (import.fontName) gf_free(import.fontName);
	if (import.streamFormat) gf_free(import.streamFormat);
	if (import.force_ext) gf_free(import.force_ext);
	if (rvc_config) gf_free(rvc_config);
	if (edits) gf_free(edits);
	if (szLan) gf_free((char *)szLan);
	if (icc_data) gf_free(icc_data);
	if (final_name) gf_free(final_name);

	if (!e) return GF_OK;
	if (fail_msg) {
		M4_LOG(GF_LOG_ERROR, ("Failure while %s: %s\n", fail_msg, gf_error_to_string(e) ));
	}
	return e;
}

typedef struct
{
	Double progress;
	u32 file_idx;
} SplitInfo;

static Bool on_split_event(void *_udta, GF_Event *evt)
{
	Double progress;
	SplitInfo *sinfo = (SplitInfo *)_udta;
	if (!_udta) return GF_FALSE;
	if (evt->type != GF_EVENT_PROGRESS) return GF_FALSE;
	if (!evt->progress.total) return GF_FALSE;

	progress = (Double) (100*evt->progress.done) / evt->progress.total;
	if (progress <= sinfo->progress)
		return GF_FALSE;

	if (evt->progress.done == evt->progress.total) {
		if (sinfo->progress <= 0)
			return GF_FALSE;
#ifndef GPAC_DISABLE_LOG
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("splitting: file %d done\n", sinfo->file_idx));
#else
		fprintf(stderr, "splitting: file %d done\n", sinfo->file_idx);
#endif
		sinfo->file_idx++;
		sinfo->progress = -1;
	} else {
		sinfo->progress = progress;
#ifndef GPAC_DISABLE_LOG
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("splitting: % 2.2f %%\r", progress));
#else
		fprintf(stderr, "splitting: % 2.2f %%\r", progress);
#endif
	}
	return GF_FALSE;
}

extern u32 do_flat;
extern Bool do_frag;
extern Double interleaving_time;

GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u64 split_size_kb, char *inName, Double InterleavingTime, Double chunk_start_time, u32 adjust_split_end, char *outName, Bool force_rap_split, const char *split_range_str, u32 fs_dump_flags)
{
	Bool chunk_extraction, rap_split, split_until_end;
	GF_Err e;
	char *ext, szName[GF_MAX_PATH], szFile[GF_MAX_PATH+100], szArgs[100];
	Double chunk_start = (Double) chunk_start_time;
	char *filter_args = NULL;
	GF_FilterSession *fs;
	GF_Filter *src, *reframe, *dst;
	SplitInfo sinfo;

	sinfo.progress = -1;
	sinfo.file_idx = 1;

	chunk_extraction = (chunk_start>=0) ? GF_TRUE : GF_FALSE;
	if (split_range_str)
		chunk_extraction = GF_TRUE;
	split_until_end = GF_FALSE;
	rap_split = GF_FALSE;
	if (split_size_kb == (u64)-1) rap_split = GF_TRUE;
	if (split_dur == -1) rap_split = GF_TRUE;
	else if (split_dur <= -2) {
		split_size_kb = 0;
		split_until_end = GF_TRUE;
	}
	else if (force_rap_split)
		rap_split = GF_TRUE;

	//split in same dir as source
	strcpy(szName, inName);
	ext = strrchr(szName, '.');
	if (ext) ext[0] = 0;

	fs = gf_fs_new_defaults(0);
	if (!fs) {
		M4_LOG(GF_LOG_ERROR, ("Failed to load filter session, aborting\n"));
		return GF_IO_ERR;
	}

	//load source with all tracks processing
	sprintf(szArgs, "mp4dmx:mov=%p:alltk", mp4);
	src = gf_fs_load_filter(fs, szArgs, &e);

	if (!src) {
		M4_LOG(GF_LOG_ERROR, ("Failed to load source filter: %s\n", gf_error_to_string(e) ));
		gf_fs_del(fs);
		return e;
	}
	//default output name formatting
	if (!outName) {
		strcpy(szFile, szName);
		strcat(szFile, "_$num%03d$");
	}

	gf_dynstrcat(&filter_args, "reframer:splitrange", NULL);
	if (rap_split) {
		gf_dynstrcat(&filter_args, ":xs=SAP", NULL);
	} else if (split_size_kb) {
		sprintf(szArgs, ":xs=S"LLU"k", split_size_kb);
		gf_dynstrcat(&filter_args, szArgs, NULL);
	} else if (chunk_extraction) {
		//we adjust end: start at the iframe at or after requested time and use xadjust (move end to next I-frame)
		//so that two calls with X:Y and Y:Z have the same Y boundary
		if (adjust_split_end==1) {
			gf_dynstrcat(&filter_args, ":xadjust:xround=after", NULL);
		} else if (adjust_split_end==2) {
			gf_dynstrcat(&filter_args, ":xadjust:xround=before", NULL);
		} else if (adjust_split_end==3) {
			gf_dynstrcat(&filter_args, ":xround=seek", NULL);
		} else if (!gf_sys_find_global_arg("xround")) {
			gf_dynstrcat(&filter_args, ":xround=closest", NULL);
		}
		if (!adjust_split_end || (adjust_split_end==3)) {
			if (!gf_sys_find_global_arg("probe_ref")) {
				gf_dynstrcat(&filter_args, ":probe_ref", NULL);
			}
		}

		if (split_range_str) {
			Bool is_time = GF_FALSE;
			char c;
			//S-E syntax
			char *end = (char *) strchr(split_range_str, '-');
			if (!end) {
				//S:E syntax
				end = (char *) strchr(split_range_str, ':');
				//if another `:` assume time format
				if (end && strchr(end+1, ':'))
					is_time = GF_TRUE;
			} else if (strchr(split_range_str, ':')) {
				is_time = GF_TRUE;
			}
			if (!end) {
				gf_free(filter_args);
				M4_LOG(GF_LOG_ERROR, ("Invalid range specifer %s, expecting START-END or START:END\n", split_range_str ));
				gf_fs_del(fs);
				return GF_BAD_PARAM;
			}

			c = end[0];
			end[0] = 0;
			if (is_time) {
				sprintf(szArgs, ":xs=T%s:xe=T%s", split_range_str, end+1);
			} else {
				sprintf(szArgs, ":xs=%s:xe=%s", split_range_str, end+1);
			}
			end[0] = c;
		} else if (split_until_end) {
			Double end=0;
			if (split_dur<-2) {
				end = (Double) gf_isom_get_duration(mp4);
				end /= gf_isom_get_timescale(mp4);
				split_dur = -2 - split_dur;
				if (end > split_dur) end-=split_dur;
				else end = 0;
			}
			if (end>0) {
				sprintf(szArgs, ":xs=%u/1000:xe=%u/1000", (u32) (chunk_start*1000), (u32) (end * 1000) );
			} else {
				sprintf(szArgs, ":xs=%u/1000", (u32) (chunk_start*1000));
			}
		} else {
			sprintf(szArgs, ":xs=%u/1000:xe=%u/1000", (u32) (chunk_start*1000), (u32) ((chunk_start+split_dur) * 1000) );
		}
		gf_dynstrcat(&filter_args, szArgs, NULL);
		if (!outName) {
			sprintf(szFile, "%s_$FS$", szName);
		}
	} else if (split_dur) {
		sprintf(szArgs, ":xs=D%u", (u32) (split_dur*1000));
		gf_dynstrcat(&filter_args, szArgs, NULL);
		if (adjust_split_end) {
			gf_dynstrcat(&filter_args, ":xadjust", NULL);
		}
	} else {
		gf_fs_del(fs);
		gf_free(filter_args);
		M4_LOG(GF_LOG_ERROR, ("Unrecognized split syntax\n"));
		return GF_BAD_PARAM;
	}

	reframe = gf_fs_load_filter(fs, filter_args, &e);
	gf_free(filter_args);
	filter_args = NULL;
	if (!reframe) {
		M4_LOG(GF_LOG_ERROR, ("Failed to load reframer filter: %s\n", gf_error_to_string(e) ));
		gf_fs_del(fs);
		return e;
	}

	if (!outName) {
		strcat(szFile, ".mp4");
	} else {
		strcpy(szFile, outName);
	}
	if (gf_dir_exists(szFile)) {
		char c = szFile[strlen(szFile)-1];
		if ((c!='/') && (c!='\\'))
			strcat(szFile, "/");

		strcat(szFile, szName);
		strcat(szFile, "_$num%03d$.mp4");
		M4_LOG(GF_LOG_WARNING, ("Split output is a directory, will use template %s\n", szFile));
	}
	else if (split_size_kb || split_dur) {
		if (!strchr(szFile, '$') && (stricmp(szFile, "null") || !strcmp(szFile, "/dev/null")) ) {
			char *sep = gf_file_ext_start(szFile);
			if (sep) sep[0] = 0;
			strcat(szFile, "_$num$.mp4");
			M4_LOG(GF_LOG_WARNING, ("Split by %s but output not a template, using %s as output\n", split_size_kb ? "size" : "duration", szFile));
		}
	}
	if (do_frag) {
		sprintf(szArgs, ":cdur=%g", interleaving_time);
		strcat(szFile, ":store=frag");
		strcat(szFile, szArgs);
	}
	else if (do_flat==1) {
		strcat(szFile, ":store=flat");
	}
	else if (do_flat || interleaving_time) {
		if (do_flat==3) {
			strcat(szFile, ":store=fstart");
		}
		sprintf(szArgs, ":cdur=%g", interleaving_time);
		strcat(szFile, szArgs);
	}

	dst = gf_fs_load_destination(fs, szFile, NULL, NULL, &e);
	if (!dst) {
		M4_LOG(GF_LOG_ERROR, ("Failed to load destination filter: %s\n", gf_error_to_string(e) ));
		gf_fs_del(fs);
		return e;
	}
	//link reframer to dst
	gf_filter_set_source(dst, reframe, NULL);

	if (!gf_sys_is_test_mode()
#ifndef GPAC_DISABLE_LOG
		&& (gf_log_get_tool_level(GF_LOG_APP)!=GF_LOG_QUIET)
#endif
		&& !gf_sys_is_quiet()
	) {
		gf_fs_enable_reporting(fs, GF_TRUE);
		gf_fs_set_ui_callback(fs, on_split_event, &sinfo);
	}
#ifdef GPAC_ENABLE_COVERAGE
	else if (gf_sys_is_cov_mode()) {
		on_split_event(NULL, NULL);
	}
#endif //GPAC_ENABLE_COVERAGE


	e = gf_fs_run(fs);
	if (e>=GF_OK) {
		e = gf_fs_get_last_connect_error(fs);
		if (e>=GF_OK)
			e = gf_fs_get_last_process_error(fs);
	}
	gf_fs_print_non_connected(fs);
	if (fs_dump_flags & 1) gf_fs_print_stats(fs);
	if (fs_dump_flags & 2) gf_fs_print_connections(fs);

	gf_fs_del(fs);
	
	if (e<GF_OK)
		M4_LOG(GF_LOG_ERROR, ("Split failed: %s\n", gf_error_to_string(e) ));
	return e;
}

GF_Err cat_multiple_files(GF_ISOFile *dest, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);

static Bool merge_parameter_set(GF_List *src, GF_List *dst, const char *name)
{
	u32 j, k;
	for (j=0; j<gf_list_count(src); j++) {
		Bool found = 0;
		GF_NALUFFParam *slc = gf_list_get(src, j);
		for (k=0; k<gf_list_count(dst); k++) {
			GF_NALUFFParam *slc_dst = gf_list_get(dst, k);
			if ( (slc->size==slc_dst->size) && !memcmp(slc->data, slc_dst->data, slc->size) ) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return GF_FALSE;
		}
	}
	return GF_TRUE;
}

static u32 merge_avc_config(GF_ISOFile *dest, u32 tk_id, GF_ISOFile **o_orig, u32 src_track, Bool force_cat, u32 *orig_nal_len, u32 *dst_nal_len)
{
	GF_AVCConfig *avc_src, *avc_dst;
	GF_ISOFile *orig = *o_orig;
	u32 dst_tk = gf_isom_get_track_by_id(dest, tk_id);

	avc_src = gf_isom_avc_config_get(orig, src_track, 1);
	avc_dst = gf_isom_avc_config_get(dest, dst_tk, 1);

	if (!force_cat && (avc_src->AVCLevelIndication!=avc_dst->AVCLevelIndication)) {
		dst_tk = 0;
	} else if (!force_cat && (avc_src->AVCProfileIndication!=avc_dst->AVCProfileIndication)) {
		dst_tk = 0;
	}
	else {
		/*rewrite all samples if using different NALU size*/
		if (avc_src->nal_unit_size > avc_dst->nal_unit_size) {
			gf_media_nal_rewrite_samples(dest, dst_tk, 8*avc_src->nal_unit_size);
			avc_dst->nal_unit_size = avc_src->nal_unit_size;
		} else if (avc_src->nal_unit_size < avc_dst->nal_unit_size) {
			*orig_nal_len = avc_src->nal_unit_size;
			*dst_nal_len = avc_dst->nal_unit_size;
		}

		/*merge PS*/
		if (!merge_parameter_set(avc_src->sequenceParameterSets, avc_dst->sequenceParameterSets, "SPS"))
			dst_tk = 0;
		if (!merge_parameter_set(avc_src->pictureParameterSets, avc_dst->pictureParameterSets, "PPS"))
			dst_tk = 0;

		gf_isom_avc_config_update(dest, dst_tk, 1, avc_dst);
	}

	gf_odf_avc_cfg_del(avc_src);
	gf_odf_avc_cfg_del(avc_dst);

	if (!dst_tk) {
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);
		gf_isom_set_nalu_extract_mode(orig, src_track, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		if (!force_cat) {
			gf_isom_avc_set_inband_config(dest, dst_tk, 1, GF_FALSE);
		} else {
			M4_LOG(GF_LOG_WARNING, ("WARNING: Concatenating track ID %d even though sample descriptions do not match\n", tk_id));
		}
	}
	return dst_tk;
}

#ifndef GPAC_DISABLE_HEVC
static u32 merge_hevc_config(GF_ISOFile *dest, u32 tk_id, GF_ISOFile **o_orig, u32 src_track, Bool force_cat, u32 *orig_nal_len, u32 *dst_nal_len)
{
	u32 i;
	GF_HEVCConfig *hevc_src, *hevc_dst;
	GF_ISOFile *orig = *o_orig;
	u32 dst_tk = gf_isom_get_track_by_id(dest, tk_id);

	hevc_src = gf_isom_hevc_config_get(orig, src_track, 1);
	hevc_dst = gf_isom_hevc_config_get(dest, dst_tk, 1);

	if (hevc_src->profile_idc != hevc_dst->profile_idc) dst_tk = 0;
	else if (hevc_src->level_idc != hevc_dst->level_idc) dst_tk = 0;
	else if (hevc_src->general_profile_compatibility_flags != hevc_dst->general_profile_compatibility_flags ) dst_tk = 0;
	else {
		/*rewrite all samples if using different NALU size*/
		if (hevc_src->nal_unit_size > hevc_dst->nal_unit_size) {
			gf_media_nal_rewrite_samples(dest, dst_tk, 8*hevc_src->nal_unit_size);
			hevc_dst->nal_unit_size = hevc_src->nal_unit_size;
		} else if (hevc_src->nal_unit_size < hevc_dst->nal_unit_size) {
			*orig_nal_len = hevc_src->nal_unit_size;
			*dst_nal_len = hevc_dst->nal_unit_size;
		}

		/*merge PS*/
		for (i=0; i<gf_list_count(hevc_src->param_array); i++) {
			u32 k;
			GF_NALUFFParamArray *src_ar = gf_list_get(hevc_src->param_array, i);
			for (k=0; k<gf_list_count(hevc_dst->param_array); k++) {
				GF_NALUFFParamArray *dst_ar = gf_list_get(hevc_dst->param_array, k);
				if (dst_ar->type==src_ar->type) {
					if (!merge_parameter_set(src_ar->nalus, dst_ar->nalus, "SPS"))
						dst_tk = 0;
					break;
				}
			}
		}

		gf_isom_hevc_config_update(dest, dst_tk, 1, hevc_dst);
	}

	gf_odf_hevc_cfg_del(hevc_src);
	gf_odf_hevc_cfg_del(hevc_dst);

	if (!dst_tk) {
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);
		gf_isom_set_nalu_extract_mode(orig, src_track, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		if (!force_cat) {
			gf_isom_hevc_set_inband_config(dest, dst_tk, 1, GF_FALSE);
		} else {
			M4_LOG(GF_LOG_WARNING, ("WARNING: Concatenating track ID %d even though sample descriptions do not match\n", tk_id));
		}
	}
	return dst_tk;
}
#endif /*GPAC_DISABLE_HEVC */

static GF_Err rewrite_nal_size_field(GF_ISOSample *samp, u32 orig_nal_len, u32 dst_nal_len)
{
	GF_Err e = GF_OK;
	u32 msize=0, remain = samp->dataLength;
	GF_BitStream *oldbs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	GF_BitStream *newbs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	u8 *buffer = NULL;

	while (remain) {
		u32 size = gf_bs_read_int(oldbs, orig_nal_len*8);
		if (size > remain) {
			e = GF_ISOM_INVALID_FILE;
			goto exit;
		}
		gf_bs_write_int(newbs, size, dst_nal_len*8);
		remain -= orig_nal_len;
		if (size > msize) {
			msize = size;
			buffer = (u8*)gf_realloc(buffer, sizeof(u8)*msize);
			if (!buffer) {
				e = GF_OUT_OF_MEM;
				goto exit;
			}
		}
		gf_bs_read_data(oldbs, buffer, size);
		gf_bs_write_data(newbs, buffer, size);
		remain -= size;
	}
	gf_free(samp->data);
	samp->data = NULL;
	samp->dataLength = 0;
	gf_bs_get_content(newbs, &samp->data, &samp->dataLength);

exit:
	gf_bs_del(newbs);
	gf_bs_del(oldbs);
	if (buffer) gf_free(buffer);
	return e;
}


GF_Err cat_playlist(GF_ISOFile *dest, char *playlistName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);

GF_Err cat_isomedia_file(GF_ISOFile *dest, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command, Bool is_pl)
{
	u32 i, j, count, nb_tracks, nb_samp, nb_done;
	GF_ISOFile *orig;
	GF_Err e;
	char *opts, *multi_cat;
	Double ts_scale, media_ts_scale;
	Double dest_orig_dur;
	u32 dst_tk, tk_id, mtype;
	u64 insert_dts;
	Bool is_isom;
	GF_ISOSample *samp;
	GF_Fraction64 aligned_to_DTS_frac;

	if (is_pl) return cat_playlist(dest, fileName, import_flags, force_fps, frames_per_sample, force_cat, align_timelines, allow_add_in_command);

	if (strchr(fileName, '*') || (strchr(fileName, '@')) )
		return cat_multiple_files(dest, fileName, import_flags, force_fps, frames_per_sample, force_cat, align_timelines, allow_add_in_command);

	multi_cat = allow_add_in_command ? strchr(fileName, '+') : NULL;
	if (multi_cat) {
		multi_cat[0] = 0;
		multi_cat = &multi_cat[1];
	}

	opts = gf_url_colon_suffix(fileName, '=');
	e = GF_OK;

	/*if options are specified, reimport the file*/
	if (opts) opts[0] = 0;
	is_isom = gf_isom_probe_file(fileName);

	if (!is_isom || multi_cat) {
		orig = gf_isom_open("temp", GF_ISOM_WRITE_EDIT, NULL);
		if (opts) opts[0] = ':';
		e = import_file(orig, fileName, import_flags, force_fps, frames_per_sample, NULL, NULL, NULL, 0);
		if (e) return e;
	} else {
		//open read+edit mode to allow applying options on file
		orig = gf_isom_open(fileName, GF_ISOM_OPEN_READ_EDIT, NULL);
		if (opts) opts[0] = ':';
		if (!orig) return gf_isom_last_error(NULL);

		if (opts) {
			e = import_file(orig, fileName, 0xFFFFFFFF, force_fps, frames_per_sample, NULL, NULL, NULL, 0);
			if (e) return e;
		}
	}

	while (multi_cat) {
		char *sep = strchr(multi_cat, '+');
		if (sep) sep[0] = 0;

		e = import_file(orig, multi_cat, import_flags, force_fps, frames_per_sample, NULL, NULL, NULL, 0);
		if (e) {
			gf_isom_delete(orig);
			return e;
		}
		if (!sep) break;
		sep[0]=':';
		multi_cat = sep+1;
	}

	nb_samp = 0;
	nb_tracks = gf_isom_get_track_count(orig);
	for (i=0; i<nb_tracks; i++) {
		mtype = gf_isom_get_media_type(orig, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_FLASH:
			M4_LOG(GF_LOG_WARNING, ("WARNING: Track ID %d (type %s) not handled by concatenation - removing from destination\n", gf_isom_get_track_id(orig, i+1), gf_4cc_to_str(mtype)));
			continue;
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_MPEG_SUBT:
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_OCR:
		case GF_ISOM_MEDIA_OCI:
		case GF_ISOM_MEDIA_IPMP:
		case GF_ISOM_MEDIA_MPEGJ:
		case GF_ISOM_MEDIA_MPEG7:
		default:
			/*only cat self-contained files*/
			if (gf_isom_is_self_contained(orig, i+1, 1)) {
				nb_samp+= gf_isom_get_sample_count(orig, i+1);
				break;
			}
			break;
		}
	}
	if (!nb_samp) {
		M4_LOG(GF_LOG_ERROR, ("No suitable media tracks to cat in %s - skipping\n", fileName));
		goto err_exit;
	}

	dest_orig_dur = (Double) (s64) gf_isom_get_duration(dest);
	if (!gf_isom_get_timescale(dest)) {
		gf_isom_set_timescale(dest, gf_isom_get_timescale(orig));
	}
	dest_orig_dur /= gf_isom_get_timescale(dest);

	aligned_to_DTS_frac.num = 0;
	aligned_to_DTS_frac.den = 1;
	for (i=0; i<gf_isom_get_track_count(dest); i++) {
		u64 track_dur = gf_isom_get_media_duration(dest, i+1);
		u32 track_ts = gf_isom_get_media_timescale(dest, i+1);
		if (gf_timestamp_less(aligned_to_DTS_frac.num, aligned_to_DTS_frac.den, track_dur, track_ts)) {
			aligned_to_DTS_frac.num = track_dur;
			aligned_to_DTS_frac.den = track_ts;
		}
	}

	M4_LOG(GF_LOG_INFO, ("Appending file %s\n", fileName));
	nb_done = 0;
	for (i=0; i<nb_tracks; i++) {
		u64 last_DTS, dest_track_dur_before_cat;
		u32 nb_edits = 0;
		Bool skip_lang_test = 1;
		Bool use_ts_dur = 1;
		Bool merge_edits = 0;
		Bool new_track = 0;
		u32 dst_tk_sample_entry = 0;
		u32 orig_nal_len = 0;
		u32 dst_nal_len = 0;
		mtype = gf_isom_get_media_type(orig, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_FLASH:
			continue;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_MPEG_SUBT:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_TIMECODE:
			use_ts_dur = 0;
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_OCR:
		case GF_ISOM_MEDIA_OCI:
		case GF_ISOM_MEDIA_IPMP:
		case GF_ISOM_MEDIA_MPEGJ:
		case GF_ISOM_MEDIA_MPEG7:
		default:
			if (!gf_isom_is_self_contained(orig, i+1, 1)) continue;
			break;
		}

		dst_tk = 0;
		/*if we had a temporary import of the file, check if the original track ID matches the dst one. If so, skip all language detection code*/
		tk_id = gf_isom_get_track_original_id(orig, i+1);
		if (!tk_id) {
			tk_id = gf_isom_get_track_id(orig, i+1);
			skip_lang_test = 0;
		}
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);


		if (dst_tk) {
			if (mtype != gf_isom_get_media_type(dest, dst_tk))
				dst_tk = 0;
			else {
				u32 subtype_dst = gf_isom_get_media_subtype(dest, dst_tk, 1);
				u32 subtype_src = gf_isom_get_media_subtype(orig, i+1, 1);
				if (subtype_dst==GF_ISOM_SUBTYPE_AVC3_H264)
					subtype_dst=GF_ISOM_SUBTYPE_AVC_H264 ;
				if (subtype_src==GF_ISOM_SUBTYPE_AVC3_H264)
					subtype_src=GF_ISOM_SUBTYPE_AVC_H264;

				if (subtype_dst==GF_ISOM_SUBTYPE_HEV1)
					subtype_dst=GF_ISOM_SUBTYPE_HVC1;
				if (subtype_src==GF_ISOM_SUBTYPE_HEV1)
					subtype_src=GF_ISOM_SUBTYPE_HVC1;

				if (subtype_dst==GF_ISOM_SUBTYPE_VVI1)
					subtype_dst=GF_ISOM_SUBTYPE_VVC1;
				if (subtype_src==GF_ISOM_SUBTYPE_VVI1)
					subtype_src=GF_ISOM_SUBTYPE_VVC1;

				if (subtype_dst != subtype_src) {
					dst_tk_sample_entry = dst_tk;
					dst_tk = 0;
				}
			}
		}

		if (!dst_tk) {
			for (j=0; j<gf_isom_get_track_count(dest); j++) {
				if (mtype != gf_isom_get_media_type(dest, j+1)) continue;
				if (gf_isom_is_same_sample_description(orig, i+1, 0, dest, j+1, 0)) {
					if (gf_isom_is_video_handler_type(mtype) ) {
						u32 w, h, ow, oh;
						gf_isom_get_visual_info(orig, i+1, 1, &ow, &oh);
						gf_isom_get_visual_info(dest, j+1, 1, &w, &h);
						if ((ow==w) && (oh==h)) {
							dst_tk = j+1;
							dst_tk_sample_entry = 0;
							break;
						}
					}
					/*check language code*/
					else if (!skip_lang_test && (mtype==GF_ISOM_MEDIA_AUDIO)) {
						u32 lang_src, lang_dst;
						char *lang = NULL;
						gf_isom_get_media_language(orig, i+1, &lang);
						if (lang) {
							lang_src = GF_4CC(lang[0], lang[1], lang[2], lang[3]);
							gf_free(lang);
						} else {
							lang_src = 0;
						}
						gf_isom_get_media_language(dest, j+1, &lang);
						if (lang) {
							lang_dst = GF_4CC(lang[0], lang[1], lang[2], lang[3]);
							gf_free(lang);
						} else {
							lang_dst = 0;
						}
						if (lang_dst==lang_src) {
							dst_tk = j+1;
							dst_tk_sample_entry = 0;
							break;
						}
					} else {
						dst_tk = j+1;
						dst_tk_sample_entry = 0;
						break;
					}
				}
			}
		}

		if (dst_tk_sample_entry)
			dst_tk = dst_tk_sample_entry;

		if (dst_tk) {
			u32 found_dst_tk = dst_tk;
			u32 stype = gf_isom_get_media_subtype(dest, dst_tk, 1);
			/*we MUST have the same codec*/
			if (gf_isom_get_media_subtype(orig, i+1, 1) != stype) dst_tk = 0;
			/*we only support cat with the same number of sample descriptions*/
			if (gf_isom_get_sample_description_count(orig, i+1) != gf_isom_get_sample_description_count(dest, dst_tk)) dst_tk = 0;
			/*if not forcing cat, check the media codec config is the same*/
			if (!gf_isom_is_same_sample_description(orig, i+1, 0, dest, dst_tk, 0)) {
				dst_tk = 0;
			}
			/*we force the same visual resolution*/
			else if (gf_isom_is_video_handler_type(mtype) ) {
				u32 w, h, ow, oh;
				gf_isom_get_visual_info(orig, i+1, 1, &ow, &oh);
				gf_isom_get_visual_info(dest, dst_tk, 1, &w, &h);
				if ((ow!=w) || (oh!=h)) {
					dst_tk = 0;
				}
			}

			if (!dst_tk) {
				/*merge AVC config if possible*/
				if ((stype == GF_ISOM_SUBTYPE_AVC_H264)
				        || (stype == GF_ISOM_SUBTYPE_AVC2_H264)
				        || (stype == GF_ISOM_SUBTYPE_AVC3_H264)
				        || (stype == GF_ISOM_SUBTYPE_AVC4_H264) ) {
					dst_tk = merge_avc_config(dest, tk_id, &orig, i+1, force_cat, &orig_nal_len, &dst_nal_len);
				}
#ifndef GPAC_DISABLE_HEVC
				/*merge HEVC config if possible*/
				else if ((stype == GF_ISOM_SUBTYPE_HVC1)
				         || (stype == GF_ISOM_SUBTYPE_HEV1)
				         || (stype == GF_ISOM_SUBTYPE_HVC2)
				         || (stype == GF_ISOM_SUBTYPE_HEV2)) {
					dst_tk = merge_hevc_config(dest, tk_id, &orig, i+1, force_cat, &orig_nal_len, &dst_nal_len);
				}
#endif /*GPAC_DISABLE_HEVC*/
				else if (force_cat) {
					dst_tk = found_dst_tk;
				}
			}
		}

		if (dst_tk_sample_entry && !dst_tk) {
			u32 k, nb_sample_desc = gf_isom_get_sample_description_count(orig, i+1);
			dst_tk = dst_tk_sample_entry;
			M4_LOG(GF_LOG_INFO, ("Multiple sample entry required, merging\n"));
			for (k=0; k<nb_sample_desc; k++) {
				u32 sdesc_idx;
				e = gf_isom_clone_sample_description(dest, dst_tk, orig, i+1, k+1, NULL, NULL, &sdesc_idx);
				if (e) goto err_exit;
				//remember idx of first new added sample desc
				if (!k)
					dst_tk_sample_entry = sdesc_idx-1;
			}
		} else {
			dst_tk_sample_entry = 0;
		}

		/*looks like a new track*/
		if (!dst_tk) {
			M4_LOG(GF_LOG_INFO, ("No suitable destination track found - creating new one (type %s)\n", gf_4cc_to_str(mtype)));
			e = gf_isom_clone_track(orig, i+1, dest, 0, &dst_tk);
			if (e) goto err_exit;
			gf_isom_clone_pl_indications(orig, dest);
			new_track = 1;

			if (align_timelines) {
				u32 max_timescale = 0;
//				u32 dst_timescale = 0;
				u32 idx;
				for (idx=0; idx<nb_tracks; idx++) {
					if (max_timescale < gf_isom_get_media_timescale(orig, idx+1))
						max_timescale = gf_isom_get_media_timescale(orig, idx+1);
				}
#if 0
				if (dst_timescale < max_timescale) {
					dst_timescale = gf_isom_get_media_timescale(dest, dst_tk);
					idx = max_timescale / dst_timescale;
					if (dst_timescale * idx < max_timescale) idx ++;
					dst_timescale *= idx;

					gf_isom_set_media_timescale(dest, dst_tk, max_timescale, 0, 0);
				}
#else
				gf_isom_set_media_timescale(dest, dst_tk, max_timescale, 0, 0);
#endif
			}

			/*remove cloned edit list, as it will be rewritten after import*/
			gf_isom_remove_edits(dest, dst_tk);
		} else {
			nb_edits = gf_isom_get_edits_count(orig, i+1);
		}

		dest_track_dur_before_cat = gf_isom_get_media_duration(dest, dst_tk);
		count = gf_isom_get_sample_count(dest, dst_tk);

		if (align_timelines) {
			insert_dts = (u64) (aligned_to_DTS_frac.num * gf_isom_get_media_timescale(dest, dst_tk));
			insert_dts /= aligned_to_DTS_frac.den;
		} else if (use_ts_dur && (count>1)) {
			insert_dts = 2*gf_isom_get_sample_dts(dest, dst_tk, count) - gf_isom_get_sample_dts(dest, dst_tk, count-1);
		} else {
			insert_dts = dest_track_dur_before_cat;
			if (!count) insert_dts = 0;
		}

		ts_scale = gf_isom_get_media_timescale(dest, dst_tk);
		ts_scale /= gf_isom_get_media_timescale(orig, i+1);
		media_ts_scale = gf_isom_get_media_timescale(dest, dst_tk);
		media_ts_scale /= gf_isom_get_media_timescale(orig, i+1);

		/*if not a new track, see if we can merge the edit list - this is a crude test that only checks
		we have the same edit types*/
		if (nb_edits && (nb_edits == gf_isom_get_edits_count(dest, dst_tk)) ) {
			u64 editTime, segmentDuration, mediaTime, dst_editTime, dst_segmentDuration, dst_mediaTime;
			GF_ISOEditType dst_editMode, editMode;
			merge_edits = 1;
			for (j=0; j<nb_edits; j++) {
				gf_isom_get_edit(orig, i+1, j+1, &editTime, &segmentDuration, &mediaTime, &editMode);
				gf_isom_get_edit(dest, dst_tk, j+1, &dst_editTime, &dst_segmentDuration, &dst_mediaTime, &dst_editMode);

				if (dst_editMode!=editMode) {
					merge_edits=0;
					break;
				}
			}
		}

		gf_isom_enable_raw_pack(orig, i+1, 2048);

		last_DTS = 0;
		count = gf_isom_get_sample_count(orig, i+1);
		for (j=0; j<count; j++) {
			u32 di;
			samp = gf_isom_get_sample(orig, i+1, j+1, &di);
			if (!samp) {
				e = gf_isom_last_error(orig);
				if (!e) e = GF_OUT_OF_MEM;
				goto err_exit;
			}
			last_DTS = samp->DTS;
			samp->DTS =  (u64) (ts_scale * samp->DTS + (new_track ? 0 : insert_dts));
			samp->CTS_Offset =  (u32) (samp->CTS_Offset * ts_scale);

			if (gf_isom_is_self_contained(orig, i+1, di)) {
				if (orig_nal_len && dst_nal_len) {
					e = rewrite_nal_size_field(samp, orig_nal_len, dst_nal_len);
					if (e) {
						gf_isom_sample_del(&samp);
						goto err_exit;
					}
				}
				e = gf_isom_add_sample(dest, dst_tk, di + dst_tk_sample_entry, samp);
			} else {
				u64 offset;
				GF_ISOSample *s = gf_isom_get_sample_info(orig, i+1, j+1, &di, &offset);
				e = gf_isom_add_sample_reference(dest, dst_tk, di + dst_tk_sample_entry, samp, offset);
				gf_isom_sample_del(&s);
			}
			if (samp->nb_pack)
				j+= samp->nb_pack-1;

			gf_isom_sample_del(&samp);
			if (e) goto err_exit;

			e = gf_isom_copy_sample_info(dest, dst_tk, orig, i+1, j+1);
			if (e) goto err_exit;

			if (j+1==count) {
				u32 sdur = gf_isom_get_sample_duration(orig, i+1, j+1);
				gf_isom_set_last_sample_duration(dest, dst_tk, (u32) (ts_scale * sdur) );
			}

			gf_set_progress("Appending", nb_done, nb_samp);
			nb_done++;
		}
		/*scene description and text: compute last sample duration based on original media duration*/
		if (!use_ts_dur) {
			u64 extend_dur = gf_isom_get_media_duration(orig, i+1) - last_DTS;
			//extend the duration of the last sample, but don't insert any edit list entry
			gf_isom_set_last_sample_duration(dest, dst_tk, (u32) (ts_scale*extend_dur) );
		}

		if (new_track && insert_dts) {
			u64 media_dur = gf_isom_get_media_duration(orig, i+1);
			/*convert from media time to track time*/
			Double rescale = (Float) gf_isom_get_timescale(dest);
			rescale /= (Float) gf_isom_get_media_timescale(dest, dst_tk);
			/*convert from orig to dst time scale*/
			rescale *= ts_scale;

			gf_isom_set_edit(dest, dst_tk, 0, (u64) (s64) (insert_dts*rescale), 0, GF_ISOM_EDIT_EMPTY);
			gf_isom_set_edit(dest, dst_tk, (u64) (s64) (insert_dts*rescale), (u64) (s64) (media_dur*rescale), 0, GF_ISOM_EDIT_NORMAL);
		} else if (merge_edits) {
			/*convert from media time to track time*/
			u32 movts_dst = gf_isom_get_timescale(dest);
			u32 trackts_dst = gf_isom_get_media_timescale(dest, dst_tk);
			u32 trackts_orig = gf_isom_get_media_timescale(orig, i+1);

			/*get the first edit normal mode and add the new track dur*/
			for (j=nb_edits; j>0; j--) {
				u64 editTime, segmentDuration, mediaTime;
				GF_ISOEditType editMode;
				gf_isom_get_edit(dest, dst_tk, j, &editTime, &segmentDuration, &mediaTime, &editMode);

				if (editMode==GF_ISOM_EDIT_NORMAL) {
					Double prev_dur = (Double) (s64) dest_track_dur_before_cat;
					Double dur = (Double) (s64) gf_isom_get_media_duration(orig, i+1);

					/*safety test: some files have broken edit lists. If no more than 2 entries, check that the segment duration
					is less or equal to the movie duration*/
					if (prev_dur * movts_dst < segmentDuration * trackts_dst) {
						M4_LOG(GF_LOG_WARNING, ("Warning: suspicious edit list entry found: duration %g sec but longest track duration before cat is %g - fixing it\n", (Double) (s64) segmentDuration/movts_dst, prev_dur/trackts_dst));
						segmentDuration = (dest_track_dur_before_cat - mediaTime) * movts_dst;
						segmentDuration /= trackts_dst;
					}

					//express original dur in new timescale
					dur /= trackts_orig;
					dur *= movts_dst;

					segmentDuration += (u64) (s64) dur;
					gf_isom_modify_edit(dest, dst_tk, j, segmentDuration, mediaTime, editMode);
					break;
				}
			}
		} else {
			u64 editTime, segmentDuration, mediaTime, edit_offset;
			Double t;
			GF_ISOEditType editMode;

			count = gf_isom_get_edits_count(dest, dst_tk);
			if (count) {
				e = gf_isom_get_edit(dest, dst_tk, count, &editTime, &segmentDuration, &mediaTime, &editMode);
				if (e) {
					M4_LOG(GF_LOG_ERROR, ("Error: edit segment error on destination track %u could not be retrieved.\n", dst_tk));
					goto err_exit;
				}
			} else if (gf_isom_get_edits_count(orig, i+1)) {
				/*fake empty edit segment*/
				/*convert from media time to track time*/
				Double rescale = (Float) gf_isom_get_timescale(dest);
				rescale /= (Float) gf_isom_get_media_timescale(dest, dst_tk);
				segmentDuration = (u64) (dest_track_dur_before_cat * rescale);
				editTime = 0;
				mediaTime = 0;
				if (segmentDuration)
					gf_isom_set_edit(dest, dst_tk, editTime, segmentDuration, mediaTime, GF_ISOM_EDIT_NORMAL);
			} else {
				editTime = 0;
				segmentDuration = 0;
			}

			/*convert to dst time scale*/
			ts_scale = (Float) gf_isom_get_timescale(dest);
			ts_scale /= (Float) gf_isom_get_timescale(orig);

			edit_offset = editTime + segmentDuration;
			count = gf_isom_get_edits_count(orig, i+1);
			for (j=0; j<count; j++) {
				gf_isom_get_edit(orig, i+1, j+1, &editTime, &segmentDuration, &mediaTime, &editMode);
				t = (Double) (s64) editTime;
				t *= ts_scale;
				t += (s64) edit_offset;
				editTime = (s64) t;
				t = (Double) (s64) segmentDuration;
				t *= ts_scale;
				segmentDuration = (s64) t;
				t = (Double) (s64) mediaTime;
				//we are in media time, apply media ts scale, not file ts scale
				t *= media_ts_scale;
				t+= (s64) dest_track_dur_before_cat;
				mediaTime = (s64) t;
				if ((editMode == GF_ISOM_EDIT_EMPTY) && (mediaTime > 0)) {
					editMode = GF_ISOM_EDIT_NORMAL;
				}
				gf_isom_set_edit(dest, dst_tk, editTime, segmentDuration, mediaTime, editMode);
			}
		}
		gf_media_update_bitrate(dest, dst_tk);

	}
	for (i = 0; i < gf_isom_get_track_count(dest); ++i) {
		if (gf_isom_get_media_type(dest, i+1) == GF_ISOM_MEDIA_TIMECODE) {
			u32 video_ref = 0;
			for (j = 0; j < gf_isom_get_track_count(dest); ++j) {
				if (gf_isom_is_video_handler_type(gf_isom_get_media_type(dest, j+1))) {
					video_ref = j+1;
					break;
				}
			}
			if (video_ref) {
				u64 video_ref_dur = gf_isom_get_media_duration(dest, video_ref);
				u32 last_sample_dur = gf_isom_get_sample_duration(dest, i+1, gf_isom_get_sample_count(dest, i+1));
				u64 dur = video_ref_dur - gf_isom_get_media_duration(dest, i+1) + last_sample_dur;
				gf_isom_set_last_sample_duration(dest, i+1, (u32)dur);
			}
		}
	}
	gf_set_progress("Appending", nb_samp, nb_samp);

	/*check brands*/
	gf_isom_get_brand_info(orig, NULL, NULL, &j);
	for (i=0; i<j; i++) {
		u32 brand;
		gf_isom_get_alternate_brand(orig, i+1, &brand);
		gf_isom_modify_alternate_brand(dest, brand, GF_TRUE);
	}
	/*check chapters*/
	for (i=0; i<gf_isom_get_chapter_count(orig, 0); i++) {
		char *name;
		Double c_time;
		u64 chap_time;
		gf_isom_get_chapter(orig, 0, i+1, &chap_time, (const char **) &name);
		c_time = (Double) (s64) chap_time;
		c_time /= 1000;
		c_time += dest_orig_dur;

		/*check last file chapter*/
		if (!i && gf_isom_get_chapter_count(dest, 0)) {
			const char *last_name;
			u64 last_chap_time;
			gf_isom_get_chapter(dest, 0, gf_isom_get_chapter_count(dest, 0), &last_chap_time, &last_name);
			/*last and first chapters are the same, don't duplicate*/
			if (last_name && name && !stricmp(last_name, name)) continue;
		}

		chap_time = (u64) (c_time*1000);
		gf_isom_add_chapter(dest, 0, chap_time, name);
	}


err_exit:
	gf_isom_delete(orig);
	return e;
}

typedef struct
{
	char szPath[GF_MAX_PATH];
	char szRad1[1024], szRad2[1024], szOpt[200];
	GF_ISOFile *dest;
	u32 import_flags;
	GF_Fraction force_fps;
	u32 frames_per_sample;
	Bool force_cat, align_timelines, allow_add_in_command;
} CATEnum;

Bool cat_enumerate(void *cbk, char *szName, char *szPath, GF_FileEnumInfo *file_info)
{
	GF_Err e;
	u32 len_rad1;
	char szFileName[GF_MAX_PATH];
	CATEnum *cat_enum = (CATEnum *)cbk;
	len_rad1 = (u32) strlen(cat_enum->szRad1);
	if (strnicmp(szName, cat_enum->szRad1, len_rad1)) return 0;
	if (strlen(cat_enum->szRad2) && !strstr(szName + len_rad1, cat_enum->szRad2) ) return 0;

	strcpy(szFileName, szPath);
	strcat(szFileName, cat_enum->szOpt);

	e = cat_isomedia_file(cat_enum->dest, szFileName, cat_enum->import_flags, cat_enum->force_fps, cat_enum->frames_per_sample, cat_enum->force_cat, cat_enum->align_timelines, cat_enum->allow_add_in_command, GF_FALSE);
	if (e) return 1;
	return 0;
}

GF_Err cat_multiple_files(GF_ISOFile *dest, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command)
{
	CATEnum cat_enum;
	char *sep;

	cat_enum.dest = dest;
	cat_enum.import_flags = import_flags;
	cat_enum.force_fps = force_fps;
	cat_enum.frames_per_sample = frames_per_sample;
	cat_enum.force_cat = force_cat;
	cat_enum.align_timelines = align_timelines;
	cat_enum.allow_add_in_command = allow_add_in_command;

	if (strlen(fileName) >= sizeof(cat_enum.szPath)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", fileName));
		return GF_NOT_SUPPORTED;
	}
	strcpy(cat_enum.szPath, fileName);
	sep = strrchr(cat_enum.szPath, GF_PATH_SEPARATOR);
	if (!sep) sep = strrchr(cat_enum.szPath, '/');
	if (!sep) {
		strcpy(cat_enum.szPath, ".");
		if (strlen(fileName) >= sizeof(cat_enum.szRad1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", fileName));
			return GF_NOT_SUPPORTED;
		}
		strcpy(cat_enum.szRad1, fileName);
	} else {
		if (strlen(sep + 1) >= sizeof(cat_enum.szRad1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", (sep + 1)));
			return GF_NOT_SUPPORTED;
		}
		strcpy(cat_enum.szRad1, sep+1);
		sep[0] = 0;
	}
	sep = strchr(cat_enum.szRad1, '*');
	if (!sep) sep = strchr(cat_enum.szRad1, '@');
	if (strlen(sep + 1) >= sizeof(cat_enum.szRad2)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", (sep + 1)));
		return GF_NOT_SUPPORTED;
	}
	strcpy(cat_enum.szRad2, sep+1);
	sep[0] = 0;
	sep = strchr(cat_enum.szRad2, '%');
	if (!sep) sep = strchr(cat_enum.szRad2, '#');
	if (!sep) sep = gf_url_colon_suffix(cat_enum.szRad2, '=');
	strcpy(cat_enum.szOpt, "");
	if (sep) {
		if (strlen(sep) >= sizeof(cat_enum.szOpt)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Invalid option: %s.\n", sep));
			return GF_NOT_SUPPORTED;
		}
		strcpy(cat_enum.szOpt, sep);
		sep[0] = 0;
	}
	return gf_enum_directory(cat_enum.szPath, 0, cat_enumerate, &cat_enum, NULL);
}

GF_Err cat_playlist(GF_ISOFile *dest, char *playlistName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command)
{
	GF_Err e;
	FILE *pl = gf_fopen(playlistName, "r");
	if (!pl) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Failed to open playlist file %s\n", playlistName));
		return GF_URL_ERROR;
	}

	e = GF_OK;
	while (!feof(pl)) {
		char szLine[10000];
		char *url;
		u32 len;
		szLine[0] = 0;
		if (gf_fgets(szLine, 10000, pl) == NULL) break;
		if (szLine[0]=='#') continue;
		len = (u32) strlen(szLine);
		while (len && strchr("\r\n \t", szLine[len-1])) {
			szLine[len-1] = 0;
			len--;
		}
		if (!len) continue;

		url = gf_url_concatenate(playlistName, szLine);
		if (!url) url = gf_strdup(szLine);

		e = cat_isomedia_file(dest, url, import_flags, force_fps, frames_per_sample, force_cat, align_timelines, allow_add_in_command, GF_FALSE);


		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Failed to concatenate file %s\n", url));
			gf_free(url);
			break;
		}
		gf_free(url);
	}
	gf_fclose(pl);
	return e;
}

#ifndef GPAC_DISABLE_SCENE_ENCODER
/*
		MPEG-4 encoding
*/

GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs)
{
#ifdef GPAC_DISABLE_SMGR
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	GF_SceneLoader load;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
#ifndef GPAC_DISABLE_SCENE_STATS
	GF_StatManager *statsman = NULL;
#endif

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = in;
	load.ctx = ctx;
	load.swf_import_flags = swf_flags;
	load.swf_flatten_limit = swf_flatten_angle;
	/*since we're encoding we must get MPEG4 nodes only*/
	load.flags = GF_SM_LOAD_MPEG4_STRICT;
	
	e = gf_sm_load_init(&load);
	if (e<0) {
		gf_sm_load_done(&load);
		M4_LOG(GF_LOG_ERROR, ("Cannot load context %s - %s\n", in, gf_error_to_string(e)));
		goto err_exit;
	}
	e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);

#ifndef GPAC_DISABLE_SCENE_STATS
	if (opts->auto_quant) {
		fprintf(stderr, "Analysing Scene for Automatic Quantization\n");
		statsman = gf_sm_stats_new();
		e = gf_sm_stats_for_scene(statsman, ctx);
		if (!e) {
			const GF_SceneStatistics *stats = gf_sm_stats_get(statsman);
			/*LASeR*/
			if (opts->auto_quant==1) {
				if (opts->resolution > (s32)stats->frac_res_2d) {
					fprintf(stderr, " Given resolution %d is (unnecessarily) too high, using %d instead.\n", opts->resolution, stats->frac_res_2d);
					opts->resolution = stats->frac_res_2d;
				} else if (stats->int_res_2d + opts->resolution <= 0) {
					fprintf(stderr, " Given resolution %d is too low, using %d instead.\n", opts->resolution, stats->int_res_2d - 1);
					opts->resolution = 1 - stats->int_res_2d;
				}
				opts->coord_bits = stats->int_res_2d + opts->resolution;
				fprintf(stderr, " Coordinates & Lengths encoded using ");
				if (opts->resolution < 0) fprintf(stderr, "only the %d most significant bits (of %d).\n", opts->coord_bits, stats->int_res_2d);
				else fprintf(stderr, "a %d.%d representation\n", stats->int_res_2d, opts->resolution);

				fprintf(stderr, " Matrix Scale & Skew Coefficients ");
				if (opts->coord_bits - 8 < stats->scale_int_res_2d) {
					opts->scale_bits = stats->scale_int_res_2d - opts->coord_bits + 8;
					fprintf(stderr, "encoded using a %d.8 representation\n", stats->scale_int_res_2d);
				} else  {
					opts->scale_bits = 0;
					fprintf(stderr, "encoded using a %d.8 representation\n", opts->coord_bits - 8);
				}
			}
#ifndef GPAC_DISABLE_VRML
			/*BIFS*/
			else if (stats->base_layer) {
				GF_AUContext *au;
				GF_CommandField *inf;
				M_QuantizationParameter *qp;
				GF_Command *com = gf_sg_command_new(ctx->scene_graph, GF_SG_GLOBAL_QUANTIZER);
				qp = (M_QuantizationParameter *) gf_node_new(ctx->scene_graph, TAG_MPEG4_QuantizationParameter);

				inf = gf_sg_command_field_new(com);
				inf->new_node = (GF_Node *)qp;
				inf->field_ptr = &inf->new_node;
				inf->fieldType = GF_SG_VRML_SFNODE;
				gf_node_register(inf->new_node, NULL);
				au = gf_list_get(stats->base_layer->AUs, 0);
				gf_list_insert(au->commands, com, 0);
				qp->useEfficientCoding = 1;
				qp->textureCoordinateQuant = 0;
				if ((stats->count_2f+stats->count_2d) && opts->resolution) {
					qp->position2DMin = stats->min_2d;
					qp->position2DMax = stats->max_2d;
					qp->position2DNbBits = opts->resolution;
					qp->position2DQuant = 1;
				}
				if ((stats->count_3f+stats->count_3d) &&  opts->resolution) {
					qp->position3DMin = stats->min_3d;
					qp->position3DMax = stats->max_3d;
					qp->position3DNbBits = opts->resolution;
					qp->position3DQuant = 1;
					qp->textureCoordinateQuant = 1;
				}
				//float quantif is disabled since 2008, check if we want to re-enable it
#if 0
				if (stats->count_float && opts->resolution) {
					qp->scaleMin = stats->min_fixed;
					qp->scaleMax = stats->max_fixed;
					qp->scaleNbBits = 2*opts->resolution;
					qp->scaleQuant = 1;
				}
#endif
			}
#endif
		}
	}
#endif /*GPAC_DISABLE_SCENE_STATS*/

	if (e<0) {
		M4_LOG(GF_LOG_ERROR, ("Error loading file %s\n", gf_error_to_string(e)));
		goto err_exit;
	} else {
		gf_log_cbk prev_logs = NULL;
		if (logs) {
			gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_DEBUG);
			prev_logs = gf_log_set_callback(logs, scene_coding_log);
		}
		opts->src_url = in;
		e = gf_sm_encode_to_file(ctx, mp4, opts);
		if (logs) {
			gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_ERROR);
			gf_log_set_callback(NULL, prev_logs);
		}
	}

	gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_MP42, 1);
	gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_ISOM, GF_TRUE);

err_exit:
#ifndef GPAC_DISABLE_SCENE_STATS
	if (statsman) gf_sm_stats_del(statsman);
#endif
	gf_sm_del(ctx);
	gf_sg_del(sg);
	return e;

#endif /*GPAC_DISABLE_SMGR*/
}
#endif /*GPAC_DISABLE_SCENE_ENCODER*/


#ifndef GPAC_DISABLE_BIFS_ENC
/*
		MPEG-4 chunk encoding
*/

static u32 GetNbBits(u32 MaxVal)
{
	u32 k=0;
	while ((s32) MaxVal > ((1<<k)-1) ) k+=1;
	return k;
}

#ifndef GPAC_DISABLE_SMGR
GF_Err EncodeBIFSChunk(GF_SceneManager *ctx, char *bifsOutputFile, GF_Err (*AUCallback)(GF_ISOSample *))
{
	GF_Err			e;
	u8 *data;
	u32 data_len;
	GF_BifsEncoder *bifsenc;
	GF_InitialObjectDescriptor *iod;
	u32 i, j, count;
	Bool encode_names, delete_bcfg;
	GF_BIFSConfig *bcfg;
	GF_AUContext		*au;
	char szRad[GF_MAX_PATH], *ext;
	char szName[GF_MAX_PATH+100];
	FILE *f;

	strcpy(szRad, bifsOutputFile);
	ext = strrchr(szRad, '.');
	if (ext) ext[0] = 0;


	/* step3: encoding all AUs in ctx->streams starting at AU index 1 (0 is SceneReplace from previous context) */
	bifsenc = gf_bifs_encoder_new(ctx->scene_graph);
	e = GF_OK;

	iod = (GF_InitialObjectDescriptor *) ctx->root_od;
	/*if no iod check we only have one bifs*/
	if (!iod) {
		count = 0;
		for (i=0; i<gf_list_count(ctx->streams); i++) {
			GF_StreamContext *sc = gf_list_get(ctx->streams, i);
			if (sc->streamType == GF_STREAM_OD) count++;
		}
		if (count>1) return GF_NOT_SUPPORTED;
	}

	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		u32 nbb;
		Bool delete_esd = GF_FALSE;
		GF_ESD *esd = NULL;
		GF_StreamContext *sc = gf_list_get(ctx->streams, i);

		if (sc->streamType != GF_STREAM_SCENE) continue;

		if (iod) {
			for (j=0; j<gf_list_count(iod->ESDescriptors); j++) {
				esd = gf_list_get(iod->ESDescriptors, j);
				if (esd->decoderConfig && esd->decoderConfig->streamType == GF_STREAM_SCENE) {
					if (!sc->ESID) sc->ESID = esd->ESID;
					if (sc->ESID == esd->ESID) {
						break;
					}
				}
				/*special BIFS direct import from NHNT*/
				else if (gf_list_count(iod->ESDescriptors)==1) {
					sc->ESID = esd->ESID;
					break;
				}
				esd = NULL;
			}
		}

		if (!esd) {
			esd = gf_odf_desc_esd_new(2);
			if (!esd) return GF_OUT_OF_MEM;
			gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = NULL;
			esd->ESID = sc->ESID;
			esd->decoderConfig->streamType = GF_STREAM_SCENE;
			delete_esd = GF_TRUE;
		}
		if (!esd->decoderConfig) return GF_OUT_OF_MEM;

		/*should NOT happen (means inputctx is not properly setup)*/
		if (!esd->decoderConfig->decoderSpecificInfo) {
			bcfg = (GF_BIFSConfig*)gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
			delete_bcfg = 1;
		}
		/*regular retrieve from ctx*/
		else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_BIFS_CFG_TAG) {
			bcfg = (GF_BIFSConfig *)esd->decoderConfig->decoderSpecificInfo;
			delete_bcfg = 0;
		}
		/*should not happen either (unless loading from MP4 in which case BIFSc is not decoded)*/
		else {
			bcfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
			delete_bcfg = 1;
		}
		/*NO CHANGE TO BIFSC otherwise the generated update will not match the input context*/
		nbb = GetNbBits(ctx->max_node_id);
		if (!bcfg->nodeIDbits) bcfg->nodeIDbits=nbb;
		if (bcfg->nodeIDbits<nbb) M4_LOG(GF_LOG_WARNING, ("Warning: BIFSConfig.NodeIDBits TOO SMALL\n"));

		nbb = GetNbBits(ctx->max_route_id);
		if (!bcfg->routeIDbits) bcfg->routeIDbits = nbb;
		if (bcfg->routeIDbits<nbb) M4_LOG(GF_LOG_WARNING, ("Warning: BIFSConfig.RouteIDBits TOO SMALL\n"));

		nbb = GetNbBits(ctx->max_proto_id);
		if (!bcfg->protoIDbits) bcfg->protoIDbits=nbb;
		if (bcfg->protoIDbits<nbb) M4_LOG(GF_LOG_WARNING, ("Warning: BIFSConfig.ProtoIDBits TOO SMALL\n"));

		/*this is the real pb, not stored in cfg or file level, set at EACH replaceScene*/
		encode_names = 0;

		/* The BIFS Config that is passed here should be the BIFSConfig from the IOD */
		gf_bifs_encoder_new_stream(bifsenc, sc->ESID, bcfg, encode_names, 0);
		if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);

		if (!esd->slConfig) esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		if (sc->timeScale) esd->slConfig->timestampResolution = sc->timeScale;
		if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = 1000;
		esd->ESID = sc->ESID;
		gf_bifs_encoder_get_config(bifsenc, sc->ESID, &data, &data_len);

		if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
		esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		esd->decoderConfig->decoderSpecificInfo->data = data;
		esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
		esd->decoderConfig->objectTypeIndication = gf_bifs_encoder_get_version(bifsenc, sc->ESID);

		for (j=1; j<gf_list_count(sc->AUs); j++) {
			au = gf_list_get(sc->AUs, j);
			e = gf_bifs_encode_au(bifsenc, sc->ESID, au->commands, &data, &data_len);
			if (data) {
				sprintf(szName, "%s-%02d-%02d.bifs", szRad, sc->ESID, j);
				f = gf_fopen(szName, "wb");
				gf_fwrite(data, data_len, f);
				gf_fclose(f);
				gf_free(data);
			}
		}
		if (delete_esd) gf_odf_desc_del((GF_Descriptor*)esd);
	}
	gf_bifs_encoder_del(bifsenc);
	return e;
}
#endif /*GPAC_DISABLE_SMGR*/


#endif /*GPAC_DISABLE_BIFS_ENC*/

/**
\param chunkFile BT chunk to be encoded
\param bifs output file name for the BIFS data
\param inputContext initial BT upon which the chunk is based (shall not be NULL)
\param outputContext: file name to dump the context after applying the new chunk to the input context
                   can be NULL, without .bt
\param tmpdir can be NULL
 */
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext)
{
#if defined(GPAC_DISABLE_SMGR) || defined(GPAC_DISABLE_BIFS_ENC) || defined(GPAC_DISABLE_SCENE_ENCODER) || defined (GPAC_DISABLE_SCENE_DUMP)
	M4_LOG(GF_LOG_WARNING, ("BIFS encoding is not supported in this build of GPAC\n"));
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	GF_SceneGraph *sg;
	GF_SceneManager	*ctx;
	GF_SceneLoader load;

	/*Step 1: create context and load input*/
	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = inputContext;
	load.ctx = ctx;
	/*since we're encoding we must get MPEG4 nodes only*/
	load.flags = GF_SM_LOAD_MPEG4_STRICT;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e) {
		M4_LOG(GF_LOG_WARNING, ("Cannot load context %s: %s\n", inputContext, gf_error_to_string(e)));
		goto exit;
	}

	/* Step 2: make sure we have only ONE RAP for each stream*/
	e = gf_sm_aggregate(ctx, 0);
	if (e) goto exit;

	/*Step 3: loading the chunk into the context*/
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = chunkFile;
	load.ctx = ctx;
	load.flags = GF_SM_LOAD_MPEG4_STRICT | GF_SM_LOAD_CONTEXT_READY;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e) {
		M4_LOG(GF_LOG_WARNING, ("Cannot load scene commands chunk %s: %s\n", chunkFile, gf_error_to_string(e)));
		goto exit;
	}
	M4_LOG(GF_LOG_INFO, ("Context and chunks loaded\n"));

	/* Assumes that the first AU contains only one command a SceneReplace and
	   that is not part of the current chunk */
	/* Last argument is a callback to pass the encoded AUs: not needed here
	   Saving is not handled correctly */
	e = EncodeBIFSChunk(ctx, bifs, NULL);
	if (e) goto exit;


	if (outputContext) {
		u32 d_mode, do_enc;
		char szF[GF_MAX_PATH], *ext;

		/*make random access for storage*/
		e = gf_sm_aggregate(ctx, 0);
		if (e) goto exit;

		/*check if we dump to BT, XMT or encode to MP4*/
		strcpy(szF, outputContext);
		ext = strrchr(szF, '.');
		d_mode = GF_SM_DUMP_BT;
		do_enc = 0;
		if (ext) {
			if (!stricmp(ext, ".xmt") || !stricmp(ext, ".xmta")) d_mode = GF_SM_DUMP_XMTA;
			else if (!stricmp(ext, ".mp4")) do_enc = 1;
			ext[0] = 0;
		}

		if (do_enc) {
			GF_ISOFile *mp4;
			strcat(szF, ".mp4");
			mp4 = gf_isom_open(szF, GF_ISOM_WRITE_EDIT, NULL);
			e = gf_sm_encode_to_file(ctx, mp4, NULL);
			if (e) gf_isom_delete(mp4);
			else gf_isom_close(mp4);
		}
		else e = gf_sm_dump(ctx, szF, GF_FALSE, d_mode);
	}

exit:
	if (ctx) {
		sg = ctx->scene_graph;
		gf_sm_del(ctx);
		gf_sg_del(sg);
	}

	return e;

#endif /*defined(GPAC_DISABLE_BIFS_ENC) || defined(GPAC_DISABLE_SCENE_ENCODER) || defined (GPAC_DISABLE_SCENE_DUMP)*/

}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


#ifndef GPAC_DISABLE_CORE_TOOLS
void sax_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	char szCheck[100];
	GF_List *imports = sax_cbck;
	u32 i=0;

	/*do not process hyperlinks*/
	if (!strcmp(node_name, "a") || !strcmp(node_name, "Anchor")) return;

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
		if (stricmp(att->name, "xlink:href") && stricmp(att->name, "url")) continue;
		if (att->value[0]=='#') continue;
		if (!strnicmp(att->value, "od:", 3)) continue;
		sprintf(szCheck, "%d", atoi(att->value));
		if (!strcmp(szCheck, att->value)) continue;
		gf_list_add(imports, gf_strdup(att->value) );
	}
}

static Bool wgt_enum_files(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	WGTEnum *wgt = (WGTEnum *)cbck;

	if (!strcmp(wgt->root_file, file_path)) return 0;
	//remove hidden files
	if (file_path[0] == '.') return 0;
	gf_list_add(wgt->imports, gf_strdup(file_path) );
	return 0;
}
static Bool wgt_enum_dir(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	if (!stricmp(file_name, "cvs") || !stricmp(file_name, ".svn") || !stricmp(file_name, ".git")) return 0;
	gf_enum_directory(file_path, 0, wgt_enum_files, cbck, NULL);
	return gf_enum_directory(file_path, 1, wgt_enum_dir, cbck, NULL);
}

GF_ISOFile *package_file(char *file_name, char *fcc, Bool make_wgt)
{
	GF_ISOFile *file = NULL;
	GF_Err e;
	GF_SAXParser *sax;
	GF_List *imports;
	Bool ascii;
	char root_dir[GF_MAX_PATH];
	char *isom_src = NULL;
	u32 i, count, mtype, skip_chars;
	char *type;

	type = gf_xml_get_root_type(file_name, &e);
	if (!type) {
		M4_LOG(GF_LOG_ERROR, ("Cannot process XML file %s: %s\n", file_name, gf_error_to_string(e) ));
		return NULL;
	}
	if (make_wgt) {
		if (strcmp(type, "widget")) {
			M4_LOG(GF_LOG_ERROR, ("XML Root type %s differs from \"widget\" \n", type));
			gf_free(type);
			return NULL;
		}
		gf_free(type);
		type = gf_strdup("application/mw-manifest+xml");
		fcc = "mwgt";
	}
	imports = gf_list_new();


	root_dir[0] = 0;
	if (make_wgt) {
		WGTEnum wgt;
		char *sep = strrchr(file_name, '\\');
		if (!sep) sep = strrchr(file_name, '/');
		if (sep) {
			char c = sep[1];
			sep[1]=0;
			strcpy(root_dir, file_name);
			sep[1] = c;
		} else {
			strcpy(root_dir, "./");
		}
		wgt.dir = root_dir;
		wgt.root_file = file_name;
		wgt.imports = imports;
		gf_enum_directory(wgt.dir, 0, wgt_enum_files, &wgt, NULL);
		gf_enum_directory(wgt.dir, 1, wgt_enum_dir, &wgt, NULL);
		ascii = 1;
	} else {
		sax = gf_xml_sax_new(sax_node_start, NULL, NULL, imports);
		e = gf_xml_sax_parse_file(sax, file_name, NULL);
		ascii = !gf_xml_sax_binary_file(sax);
		gf_xml_sax_del(sax);
		if (e<0) goto exit;
		e = GF_OK;
	}

	if (fcc) {
		mtype = GF_4CC(fcc[0],fcc[1],fcc[2],fcc[3]);
	} else {
		mtype = 0;
		if (!stricmp(type, "svg")) mtype = ascii ? GF_META_TYPE_SVG : GF_META_TYPE_SVGZ;
		else if (!stricmp(type, "smil")) mtype = ascii ? GF_META_TYPE_SMIL : GF_META_TYPE_SMLZ;
		else if (!stricmp(type, "x3d")) mtype = ascii ? GF_META_TYPE_X3D  : GF_META_TYPE_X3DZ  ;
		else if (!stricmp(type, "xmt-a")) mtype = ascii ? GF_META_TYPE_XMTA : GF_META_TYPE_XMTZ;
	}
	if (!mtype) {
		M4_LOG(GF_LOG_ERROR, ("Missing 4CC code for meta name - please use ABCD:fileName\n"));
		e = GF_BAD_PARAM;
		goto exit;
	}


	if (!make_wgt) {
		count = gf_list_count(imports);
		for (i=0; i<count; i++) {
			char *item = gf_list_get(imports, i);
			char *res_url = NULL;

			FILE *test = gf_fopen(item, "rb");
			if (!test) {
				res_url = gf_url_concatenate(file_name, item);
				test = gf_fopen(res_url, "rb");
			}

			if (!test) {
				if (res_url) gf_free(res_url);
				gf_list_rem(imports, i);
				i--;
				count--;
				gf_free(item);
				continue;
			}
			gf_fclose(test);
			if (gf_isom_probe_file(res_url)) {
				if (res_url) gf_free(res_url);
				if (isom_src) {
					M4_LOG(GF_LOG_ERROR, ("Cannot package several IsoMedia files together\n"));
					e = GF_NOT_SUPPORTED;
					goto exit;
				}
				gf_list_rem(imports, i);
				i--;
				count--;
				isom_src = item;
				continue;
			}
			if (res_url) gf_free(res_url);
		}
	}

	if (isom_src) {
		file = gf_isom_open(isom_src, GF_ISOM_OPEN_EDIT, NULL);
	} else {
		file = gf_isom_open("package", GF_ISOM_WRITE_EDIT, NULL);
	}

	e = gf_isom_set_meta_type(file, 1, 0, mtype);
	if (e) goto exit;
	/*add self ref*/
	if (isom_src) {
		e = gf_isom_add_meta_item(file, 1, 0, 1, NULL, isom_src, 0, 0, NULL, NULL, NULL,  NULL, NULL);
		if (e) goto exit;
	}
	e = gf_isom_set_meta_xml(file, 1, 0, file_name, NULL, 0, !ascii);
	if (e) goto exit;

	skip_chars = (u32) strlen(root_dir);
	count = gf_list_count(imports);
	for (i=0; i<count; i++) {
		char *ext, *mime, *encoding, *name, *itemurl;
		char *item = gf_list_get(imports, i);

		name = gf_strdup(item + skip_chars);

		if (make_wgt) {
			while (1) {
				char *sep = strchr(name, '\\');
				if (!sep) break;
				sep[0] = '/';
			}
		}


		mime = encoding = NULL;
		ext = strrchr(item, '.');
		if (!stricmp(ext, ".gz")) ext = strrchr(ext-1, '.');

		if (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg")) mime = "image/jpeg";
		else if (!stricmp(ext, ".png")) mime = "image/png";
		else if (!stricmp(ext, ".svg")) mime = "image/svg+xml";
		else if (!stricmp(ext, ".x3d")) mime = "model/x3d+xml";
		else if (!stricmp(ext, ".xmt")) mime = "application/x-xmt";
		else if (!stricmp(ext, ".js")) {
			mime = "application/javascript";
		}
		else if (!stricmp(ext, ".svgz") || !stricmp(ext, ".svg.gz")) {
			mime = "image/svg+xml";
			encoding = "binary-gzip";
		}
		else if (!stricmp(ext, ".x3dz") || !stricmp(ext, ".x3d.gz")) {
			mime = "model/x3d+xml";
			encoding = "binary-gzip";
		}
		else if (!stricmp(ext, ".xmtz") || !stricmp(ext, ".xmt.gz")) {
			mime = "application/x-xmt";
			encoding = "binary-gzip";
		}

		itemurl = gf_url_concatenate(file_name, item);

		e = gf_isom_add_meta_item(file, 1, 0, 0, itemurl ? itemurl : item, name, 0, GF_META_ITEM_TYPE_MIME, mime, encoding, NULL,  NULL, NULL);
		gf_free(name);
		gf_free(itemurl);
		if (e) goto exit;
	}

exit:
	while (gf_list_count(imports)) {
		char *item = gf_list_last(imports);
		gf_list_rem_last(imports);
		gf_free(item);
	}
	gf_list_del(imports);
	if (isom_src) gf_free(isom_src);
	if (type) gf_free(type);
	if (e) {
		if (file) gf_isom_delete(file);
		return NULL;
	}
	return file;
}

GF_Err parse_high_dynamc_range_xml_desc(GF_ISOFile *movie, char *file_name)
{
	GF_DOMParser *parser;
	GF_XMLNode *root, *stream;
	GF_Err e;
	u32 i;
	GF_MasteringDisplayColourVolumeInfo mdcv;
	GF_ContentLightLevelInfo clli;

	memset(&mdcv, 0, sizeof(GF_MasteringDisplayColourVolumeInfo));
	memset(&clli, 0, sizeof(GF_ContentLightLevelInfo));

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, file_name, NULL, NULL);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error parsing HDR XML file: Line %d - %s. Abort.\n", gf_xml_dom_get_line(parser), gf_xml_dom_get_error(parser)));
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);
	if (!root) {
		M4_LOG(GF_LOG_ERROR, ("Error parsing HDR XML file: no \"root\" found. Abort.\n"));
		gf_xml_dom_del(parser);
		return e;
	}
	if (strcmp(root->name, "HDR")) {
		M4_LOG(GF_LOG_ERROR, ("Error parsing HDR XML file: root name is \"%s\", expecting \"HDR\"\n", root->name));
		gf_xml_dom_del(parser);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	i = 0;
	while ((stream = gf_list_enum(root->content, &i))) {
		u32 id = 0, j;
		GF_XMLAttribute* att = NULL;
		GF_XMLNode *box = NULL;

		if (stream->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(stream->name, "Track")) continue;

		j = 0;
		while ((att = gf_list_enum(stream->attributes, &j))) {
			if (!strcmp(att->name, "id")) id = atoi(att->value);
			else fprintf(stderr, "HDR XML: ignoring track attribute \"%s\"\n", att->name);
		}

		j = 0;
		while ((box = gf_list_enum(stream->content, &j))) {
			u32 k;

			if (box->type != GF_XML_NODE_TYPE) continue;

			if (!strcmp(box->name, "mdcv")) {
				k = 0;
				while ((att = gf_list_enum(box->attributes, &k))) {
					if (!strcmp(att->name, "display_primaries_0_x")) mdcv.display_primaries[0].x = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_0_y")) mdcv.display_primaries[0].y = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_1_x")) mdcv.display_primaries[1].x = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_1_y")) mdcv.display_primaries[1].y = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_2_x")) mdcv.display_primaries[2].x = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_2_y")) mdcv.display_primaries[2].y = atoi(att->value);
					else if (!strcmp(att->name, "white_point_x")) mdcv.white_point_x = atoi(att->value);
					else if (!strcmp(att->name, "white_point_y")) mdcv.white_point_y = atoi(att->value);
					else if (!strcmp(att->name, "max_display_mastering_luminance")) mdcv.max_display_mastering_luminance = atoi(att->value);
					else if (!strcmp(att->name, "min_display_mastering_luminance")) mdcv.min_display_mastering_luminance = atoi(att->value);
					else fprintf(stderr, "HDR XML: ignoring box \"%s\" attribute \"%s\"\n", box->name, att->name);
				}
			} else if (!strcmp(box->name, "clli")) {
				k = 0;
				while ((att = gf_list_enum(box->attributes, &k))) {
					if (!strcmp(att->name, "max_content_light_level")) clli.max_content_light_level = atoi(att->value);
					else if (!strcmp(att->name, "max_pic_average_light_level")) clli.max_pic_average_light_level = atoi(att->value);
					else fprintf(stderr, "HDR XML: ignoring box \"%s\" attribute \"%s\"\n", box->name, att->name);
				}
			} else {
				fprintf(stderr, "HDR XML: ignoring box element \"%s\"\n", box->name);
				continue;
			}
		}

		e = gf_isom_set_high_dynamic_range_info(movie, id, 1, &mdcv, &clli);
		if (e) {
			M4_LOG(GF_LOG_ERROR, ("HDR XML: error in gf_isom_set_high_dynamic_range_info()\n"));
			break;
		}
	}

	gf_xml_dom_del(parser);
	return e;
}

#else
GF_ISOFile *package_file(char *file_name, char *fcc, Bool make_wgt)
{
	M4_LOG(GF_LOG_ERROR, ("XML Not supported in this build of GPAC - cannot package file\n"));
	return NULL;
}

GF_Err parse_high_dynamc_range_xml_desc(GF_ISOFile* movie, char* file_name)
{
	M4_LOG(GF_LOG_ERROR, ("XML Not supported in this build of GPAC - cannot process HDR parameter file\n"));
	return GF_OK;
}
#endif //#ifndef GPAC_DISABLE_CORE_TOOLS


#endif /*GPAC_DISABLE_ISOM_WRITE*/

