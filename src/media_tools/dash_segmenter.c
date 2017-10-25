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

#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/mpegts.h>
#include <gpac/config_file.h>
#include <gpac/network.h>
#ifdef _WIN32_WCE
#include <winbase.h>
#else
#include <time.h>
#endif
#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/mpd.h>

#ifndef GPAC_DISABLE_ISOM_WRITE
#ifdef GPAC_DISABLE_ISOM
/*we should need a better way to work with sidx when no isom is defined*/
#define GPAC_DISABLE_MPEG2TS
#endif

//#define GENERATE_VIRTUAL_REP_SRD

typedef struct _dash_segment_input GF_DashSegInput;

struct _dash_component
{
	u32 ID;/*audio/video/text/ ...*/
	u32 media_type;/*audio/video/text/ ...*/
	char szCodec[RFC6381_CODEC_NAME_SIZE_MAX];
	/*for video */
	u32 width, height, fps_num, fps_denum, sar_num, sar_denum, max_sap;

	/*for audio*/
	u32 sample_rate, channels;
	/*apply to any media. We use 5 bytes because we may use copy data converted from gf_4cc_to_str which is 5 bytes*/
	char *lang;
	Double duration;
};

struct __gf_dash_segmenter
{
	GF_MPD *mpd;
	char *mpd_name;
       char *m3u8_name;
	GF_DashSegInput *inputs;
	u32 nb_inputs;
	GF_DashProfile profile;

	GF_DashDynamicMode dash_mode;
	u32 use_url_template;
	Bool use_segment_timeline;
	Bool single_segment;
	Bool single_file;
	GF_DashSwitchingMode bitstream_switching_mode;
	Bool segments_start_with_rap;

	Double segment_duration;
	Bool segment_duration_strict;
	Double fragment_duration;
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
	Double min_buffer;
	s32 ast_offset_ms;
	u32 dash_scale;
	Bool fragments_in_memory;
	u32 initial_moof_sn;
	u64 initial_tfdt;
	Bool no_fragments_defaults;
	Bool pssh_moof;
	Bool samplegroups_in_traf;
	Bool single_traf_per_moof;
	Double mpd_live_duration;
	Bool insert_utc;
	Bool real_time;
	u32 start_date_sec_ntp, start_date_sec_ntp_ms_frac;
	u32 nb_secs_to_discard;
	const char *dash_profile_extension;

	GF_Config *dash_ctx;


	Double subduration;

	FILE *mpd_file;
	FILE *period_mpd_file;
	Bool segment_alignment_disabled;
	u32 single_file_mode;
	const char *bs_switch_segment_file;
	Bool inband_param_set;
	Bool force_period_end;

	/*set if seg_rad_name depends on input file name (had %s in it). In this case, SegmentTemplate cannot be used at adaptation set level*/
	Bool variable_seg_rad_name;
	Bool mpd_header_is_init;

	/*If true, disable generation date printing in mpd headers*/
	Bool force_test_mode;

	GF_DASH_ContentLocationMode cp_location_mode;

	Double max_segment_duration;
	Bool no_cache;
	Bool disable_loop;
};

struct _dash_segment_input
{
	char *file_name;
	char representationID[100];
	char *periodID;
	Double media_duration; /*forced media duration*/
	u32 nb_baseURL;
	char **baseURL;
	char *xlink;
	u32 nb_roles;
	char **roles;
	u32 nb_rep_descs;
	char **rep_descs;
	u32 nb_as_descs;
	char **as_descs;
	u32 nb_as_c_descs;
	char **as_c_descs;
	u32 nb_p_descs;
	char **p_descs;
	u32 bandwidth;
	u32 dependency_bandwidth;
	char *dependencyID;

	/*if 0, input is disabled*/
	u32 adaptation_set;
	/*if 0, input is disabled*/
	u32 period;
	u32 group_id;
	/*only set for the first representation in the AdaptationSet*/
	u32 nb_rep_in_adaptation_set;
	Double period_duration;

	/*media-format specific functions*/
	/*assigns the different media to the same adaptation set or group than the input_idx one*/
	GF_Err (* dasher_input_classify) (GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 input_idx, u32 *current_group_id, u32 *max_sap_type);
	GF_Err ( *dasher_get_components_info) (GF_DashSegInput *dash_input, GF_DASHSegmenter *opts);
	GF_Err ( *dasher_create_init_segment) (GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 adaptation_set, char *szInitName, const char *tmpdir, GF_DASHSegmenter *dash_opts, GF_DashSwitchingMode bs_switch_mode, Bool *disable_bs_switching);
	GF_Err ( *dasher_segment_file) (GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenter *opts, GF_MPD_AdaptationSet *adaptation_set_obj, Bool first_in_set);

	/*shall be set after call to dasher_input_classify*/
	char szMime[50];

	u32 single_track_num;
	//information for scalability
	u32 trackNum, lower_layer_track, nb_representations, idx_representations;
	//increase sequence number between consecutive segments by this amount (for scalable reps)
	u32 moof_seqnum_increase;

	u32 protection_scheme_type;

#ifdef GENERATE_VIRTUAL_REP_SRD
	Bool virtual_representation;
#endif

	Double segment_duration;
	/*all these shall be set after call to dasher_get_components_info*/
	Double duration;
	struct _dash_component components[20];
	u32 nb_components;

	//spatial info for tiling
	u32 x, y, w, h;

	Bool disable_inband_param_set;
	//0: PID specified, 1 or more: specified, indicates the niumber of time the period was instanciated
	u32 period_id_not_specified;

	Bool init_segment_generated;
	char *init_seg_url;
	Bool use_bs_switching;


	Bool get_component_info_done;
	//cached isobmf input
	GF_ISOFile *isobmf_input;
	Bool no_cache;

	Double clamp_duration;
};

#define EXTRACT_FORMAT(_nb_chars)	\
			strcpy(szFmt, "%d");	\
			char_template+=_nb_chars;	\
			if (seg_rad_name[char_template]=='%') {	\
				char *sep = strchr(seg_rad_name+char_template, '$');	\
				if (sep) {	\
					sep[0] = 0;	\
					strcpy(szFmt, seg_rad_name+char_template);	\
					char_template += (u32) strlen(seg_rad_name+char_template);	\
					sep[0] = '$';	\
				}	\
			}	\
			char_template+=1;	\

GF_EXPORT
GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *output_file_name, const char *rep_id, const char *base_url, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number, Bool use_segment_timeline)
{
	char szFmt[20];
	Bool has_number= GF_FALSE;
	Bool is_index = (seg_type==GF_DASH_TEMPLATE_REPINDEX) ? GF_TRUE : GF_FALSE;
	Bool is_init = (seg_type==GF_DASH_TEMPLATE_INITIALIZATION) ? GF_TRUE : GF_FALSE;
	Bool is_template = (seg_type==GF_DASH_TEMPLATE_TEMPLATE) ? GF_TRUE : GF_FALSE;
	Bool is_init_template = (seg_type==GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE) ? GF_TRUE : GF_FALSE;
	Bool needs_init=((is_init || is_init_template) && !is_bs_switching) ? GF_TRUE : GF_FALSE;
	u32 char_template = 0;
	char tmp[100];
	strcpy(segment_name, "");

	if (seg_rad_name && (strstr(seg_rad_name, "$RepresentationID$") || strstr(seg_rad_name, "$Bandwidth$")))
		needs_init = GF_FALSE;

	if (!seg_rad_name) {
		strcpy(segment_name, output_file_name); //already contains base_url
	} else {
		char c;
		const size_t seg_rad_name_len = strlen(seg_rad_name);
		while (char_template <= seg_rad_name_len) {
			c = seg_rad_name[char_template];

			if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$RepresentationID$", 18) ) {
				char_template += 18;
				strcat(segment_name, rep_id);
				needs_init = GF_FALSE;
			}
			else if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$Bandwidth", 10)) {
				EXTRACT_FORMAT(10);

				sprintf(tmp, szFmt, bandwidth);
				strcat(segment_name, tmp);
				needs_init = GF_FALSE;
			}
			else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Time", 5)) {
				EXTRACT_FORMAT(5);
				if (is_init || is_init_template) continue;
				/*replace %d to LLD*/
				szFmt[strlen(szFmt)-1]=0;
				strcat(szFmt, &LLD[1]);
				sprintf(tmp, szFmt, start_time);
				strcat(segment_name, tmp);
				has_number = GF_TRUE;
			}
			else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Number", 7)) {
				EXTRACT_FORMAT(7);

				if (is_init || is_init_template) continue;
				sprintf(tmp, szFmt, segment_number);
				strcat(segment_name, tmp);
				has_number = GF_TRUE;
			}
			else if (!strnicmp(& seg_rad_name[char_template], "$Init=", 6)) {
				char *sep = strchr(seg_rad_name + char_template+6, '$');
				if (sep) sep[0] = 0;
				if (is_init || is_init_template) {
					strcat(segment_name, seg_rad_name + char_template+6);
					needs_init = GF_FALSE;
				}
				char_template += (u32) strlen(seg_rad_name + char_template)+1;
				if (sep) sep[0] = '$';
			}
			else if (!strnicmp(& seg_rad_name[char_template], "$Index=", 7)) {
				char *sep = strchr(seg_rad_name + char_template+7, '$');
				if (sep) sep[0] = 0;
				if (is_index) {
					strcat(segment_name, seg_rad_name + char_template+6);
					needs_init = GF_FALSE;
				}
				char_template += (u32) strlen(seg_rad_name + char_template)+1;
				if (sep) sep[0] = '$';
			}
			else if (!strnicmp(& seg_rad_name[char_template], "$Path=", 6)) {
				char *sep = strchr(seg_rad_name + char_template+6, '$');
				if (sep) sep[0] = 0;
				if (!is_template && !is_init_template) {
					strcat(segment_name, seg_rad_name + char_template+6);
				}
				char_template += (u32) strlen(seg_rad_name + char_template)+1;
				if (sep) sep[0] = '$';
			}
			else if (!strnicmp(& seg_rad_name[char_template], "$Segment=", 9)) {
				char *sep = strchr(seg_rad_name + char_template+9, '$');
				if (sep) sep[0] = 0;
				if (!is_init && !is_init_template) {
					strcat(segment_name, seg_rad_name + char_template+9);
				}
				char_template += (u32) strlen(seg_rad_name + char_template)+1;
				if (sep) sep[0] = '$';
			}

			else {
				char_template+=1;
				if (c=='\\') c = '/';

				sprintf(tmp, "%c", c);
				strcat(segment_name, tmp);
			}
		}
	}

	if (is_template && !strstr(seg_rad_name, "$Number") && !strstr(seg_rad_name, "$Time")) {
		if (use_segment_timeline) {
			strcat(segment_name, "$Time$");
		} else {
			strcat(segment_name, "$Number$");
		}
	}

	if (needs_init)
		strcat(segment_name, "init");

	if (!is_init && !is_template && !is_init_template && !is_index && !has_number) {
		sprintf(tmp, "%d", segment_number);
		strcat(segment_name, tmp);
	}
	if (seg_ext) {
		strcat(segment_name, ".");
		strcat(segment_name, seg_ext);
	}

	if ((seg_type != GF_DASH_TEMPLATE_TEMPLATE) && (seg_type != GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE)) {
		char *sep = strrchr(segment_name, '/');
		if (sep) {
			char c = sep[0];
			sep[0] = 0;
			if (!gf_dir_exists(segment_name)) {
				gf_mkdir(segment_name);
			}
			sep[0] = c;
		}
	}

	return GF_OK;
}

const char *gf_dasher_strip_output_dir(const char *mpd_url, const char *path)
{
	char c;
	const char *res;
	char *sep;
	if (!mpd_url || !path) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[DASH] invalid call to strip_output_dir\n"));
		return "";
	}
	sep = strrchr(mpd_url, '/');
	if (!sep) sep = strrchr(mpd_url, '\\');
	if (!sep) return path;
	c = sep[0];
	sep[0] = 0;
	if (!strncmp(mpd_url, path, strlen(mpd_url))) {
		res = path + strlen(mpd_url) + 1;
	} else {
		res = path;
	}
	sep[0] = c;
	return res;
}

GF_Err gf_dasher_store_segment_info(GF_DASHSegmenter *dasher, const char *representationID, const char *SegmentName, u64 segStartTime, u64 segEndTime)
{
	char szKey[512];
	if (!dasher->dash_ctx) return GF_OK;

	sprintf(szKey, ""LLU"-"LLU"@%s", segStartTime, segEndTime-segStartTime, representationID);
	return gf_cfg_set_key(dasher->dash_ctx, "SegmentsStartTimes", SegmentName, szKey);
}


#ifndef GPAC_DISABLE_ISOM

GF_EXPORT
GF_Err gf_media_get_rfc_6381_codec_name(GF_ISOFile *movie, u32 track, char *szCodec, Bool force_inband, Bool force_sbr)
{
	GF_ESD *esd;
	GF_AVCConfig *avcc;
#ifndef GPAC_DISABLE_HEVC
	GF_HEVCConfig *hvcc;
#endif
	u32 subtype = gf_isom_get_media_subtype(movie, track, 1);

	if (subtype == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
		GF_Err e;
		u32 originalFormat=0;
		if (gf_isom_is_ismacryp_media(movie, track, 1)) {
			e = gf_isom_get_ismacryp_info(movie, track, 1, &originalFormat, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		} else if (gf_isom_is_omadrm_media(movie, track, 1)) {
			e = gf_isom_get_omadrm_info(movie, track, 1, &originalFormat, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		} else if(gf_isom_is_cenc_media(movie, track, 1)) {
			e = gf_isom_get_cenc_info(movie, track, 1, &originalFormat, NULL, NULL, NULL);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISOM Tools] Unkown protection scheme type %s\n", gf_4cc_to_str( gf_isom_is_media_encrypted(movie, track, 1)) ));
			e = gf_isom_get_original_format_type(movie, track, 1, &originalFormat);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISOM Tools] Error fecthing protection information\n"));
			return e;
		}

		if (originalFormat) subtype = originalFormat;
	}

	switch (subtype) {
	case GF_ISOM_SUBTYPE_MPEG4:
		esd = gf_isom_get_esd(movie, track, 1);
		if (esd) {
			switch (esd->decoderConfig->streamType) {
			case GF_STREAM_AUDIO:
				if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
					/*5 first bits of AAC config*/
					u8 audio_object_type = (esd->decoderConfig->decoderSpecificInfo->data[0] & 0xF8) >> 3;
	#ifndef GPAC_DISABLE_AV_PARSERS
					if (force_sbr && (audio_object_type==2) ) {
						GF_M4ADecSpecInfo a_cfg;
						GF_Err e = gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
						if (e==GF_OK) {
							if (a_cfg.sbr_sr)
								audio_object_type = a_cfg.sbr_object_type;
							if (a_cfg.has_ps)
								audio_object_type = 29;
						}
					}
	#endif
					snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4a.%02X.%01d", esd->decoderConfig->objectTypeIndication, audio_object_type);
				} else {
					snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4a.%02X", esd->decoderConfig->objectTypeIndication);
				}
				break;
			case GF_STREAM_VISUAL:
	#ifndef GPAC_DISABLE_AV_PARSERS
				if (esd->decoderConfig->decoderSpecificInfo) {
					GF_M4VDecSpecInfo dsi;
					gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
					snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4v.%02X.%01x", esd->decoderConfig->objectTypeIndication, dsi.VideoPL);
				} else
	#endif
				{
					snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4v.%02X", esd->decoderConfig->objectTypeIndication);
				}
				break;
			default:
				snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "mp4s.%02X", esd->decoderConfig->objectTypeIndication);
				break;
			}
			gf_odf_desc_del((GF_Descriptor *)esd);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[RFC6381] Cannot find ESD. Aborting.\n"));
			return GF_ISOM_INVALID_FILE;
		}
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
		//FIXME: in avc1 with multiple descriptor, we should take the right description index
		avcc = gf_isom_avc_config_get(movie, track, 1);
		if (force_inband) {
			if (subtype==GF_ISOM_SUBTYPE_AVC_H264)
				subtype = GF_ISOM_SUBTYPE_AVC3_H264;
			else if (subtype==GF_ISOM_SUBTYPE_AVC2_H264)
				subtype = GF_ISOM_SUBTYPE_AVC4_H264;
		}
		if (avcc) {
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.%02X%02X%02X", gf_4cc_to_str(subtype), avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
			gf_odf_avc_cfg_del(avcc);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot find AVC configuration box"));
			return GF_ISOM_INVALID_FILE;
		}
	case GF_ISOM_SUBTYPE_SVC_H264:
	case GF_ISOM_SUBTYPE_MVC_H264:
		avcc = gf_isom_mvc_config_get(movie, track, 1);
		if (!avcc) avcc = gf_isom_svc_config_get(movie, track, 1);
		if (avcc) {
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.%02X%02X%02X", gf_4cc_to_str(subtype), avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
			gf_odf_avc_cfg_del(avcc);
			return GF_OK;
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot find AVC configuration box"));
			return GF_ISOM_INVALID_FILE;
		}
#ifndef GPAC_DISABLE_HEVC
	case GF_ISOM_SUBTYPE_HVC1:
	case GF_ISOM_SUBTYPE_HEV1:
	case GF_ISOM_SUBTYPE_HVC2:
	case GF_ISOM_SUBTYPE_HEV2:
	case GF_ISOM_SUBTYPE_HVT1:
	case GF_ISOM_SUBTYPE_LHV1:
	case GF_ISOM_SUBTYPE_LHE1:

		if (force_inband) {
			if (subtype==GF_ISOM_SUBTYPE_HVC1) subtype = GF_ISOM_SUBTYPE_HEV1;
			else if (subtype==GF_ISOM_SUBTYPE_HVC2) subtype = GF_ISOM_SUBTYPE_HEV2;
		}
		hvcc = gf_isom_hevc_config_get(movie, track, 1);
		if (!hvcc) {
			hvcc = gf_isom_lhvc_config_get(movie, track, 1);
		}
		if (subtype==GF_ISOM_SUBTYPE_HVT1) {
			u32 refTrack;
			gf_isom_get_reference(movie, track, GF_ISOM_REF_TBAS, 1, &refTrack);
			if (hvcc) gf_odf_hevc_cfg_del(hvcc);
			hvcc = gf_isom_hevc_config_get(movie, refTrack, 1);
		}
		if (hvcc) {
			u8 c;
			char szTemp[RFC6381_CODEC_NAME_SIZE_MAX];
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
		} else {
			snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
		}
		return GF_OK;
#endif

	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUTHOR, ("[ISOM Tools] codec parameters not known - setting codecs string to default value \"%s\"\n", gf_4cc_to_str(subtype) ));
		snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
		return GF_OK;
	}
	return GF_OK;
}

#endif //GPAC_DISABLE_ISOM


#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

typedef struct
{
	/* indicates if the track under fragmentation is the reference for sidx or prft boxes*/
	Bool is_ref_track;

	Bool done;
	u32 TrackID;
	u32 SampleNum, SampleCount;
	u64 FragmentLength;
	u32 OriginalTrack;
	u32 finalSampleDescriptionIndex;
	u32 TimeScale, MediaType, sample_duration;
	u64 last_sample_cts, next_sample_dts, InitialTSOffset, loop_ts_offset;
	Bool all_sample_raps, splitable;
	/* carry of the amount of time of the current sample already used in the previous segment */
	u32 split_sample_dts_shift;
	s32 media_time_to_pres_time_shift;
	u64 min_cts_in_segment;
	u64 start_tfdt;
} GF_ISOMTrackFragmenter;

static u64 isom_get_next_sap_time(GF_ISOFile *input, u32 track, u32 sample_count, u32 sample_num)
{
	GF_ISOSample *samp;
	u64 time;
	Bool is_rap, has_roll;
	u32 i, found_sample = 0;
	for (i=sample_num; i<=sample_count; i++) {
		if (gf_isom_get_sample_sync(input, track, i)) {
			found_sample = i;
			break;
		}
		gf_isom_get_sample_rap_roll_info(input, track, i, &is_rap, &has_roll, NULL);
		if (is_rap || has_roll) {
			found_sample = i;
			break;
		}
	}
	//TRICK: if we don't found next RAP, return time of the last sample in track
	if (!found_sample)
		found_sample = sample_count;
	samp = gf_isom_get_sample_info(input, track, found_sample, NULL, NULL);
	time = samp->DTS;
	gf_isom_sample_del(&samp);
	return time;
}

static GF_MPD_Descriptor *gf_isom_get_content_protection_desc(GF_ISOFile *input, u32 protected_track)
{
	u32 prot_scheme	= gf_isom_is_media_encrypted(input, protected_track, 1);
	if (gf_isom_is_cenc_media(input, protected_track, 1)) {
		GF_MPD_Descriptor *desc;
		GF_XMLAttribute *att;
		char cenc_value[256];
		cenc_value[0]='\0';
		bin128 default_KID;
		u8 i;
		gf_isom_cenc_get_default_info(input, protected_track, 1, NULL, NULL, &default_KID);
		desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:dash:mp4protection:2011", gf_4cc_to_str(prot_scheme));
		/* Output canonical UIID form */
		for (i=0; i<4; i++) sprintf(cenc_value, "%s%02x",cenc_value, default_KID[i]);
		sprintf(cenc_value, "%s-",cenc_value);
		for (i=4; i<6; i++) sprintf(cenc_value, "%s%02x",cenc_value, default_KID[i]);
		sprintf(cenc_value, "%s-",cenc_value);
		for (i=6; i<8; i++) sprintf(cenc_value, "%s%02x",cenc_value, default_KID[i]);
		sprintf(cenc_value, "%s-",cenc_value);
		for (i=8; i<10; i++) sprintf(cenc_value, "%s%02x",cenc_value, default_KID[i]);
		sprintf(cenc_value, "%s-",cenc_value);
		for (i=10; i<16; i++) sprintf(cenc_value, "%s%02x",cenc_value, default_KID[i]);
		att = gf_xml_dom_create_attribute("cenc:default_KID", cenc_value);
		if (!desc->attributes) {
			desc->attributes = gf_list_new();
		}
		gf_list_add(desc->attributes, att);
		return desc;
	}
	//todo for ISMA or OMA DRM

	return NULL;
}

static void gf_dash_append_segment_timeline(GF_MPD_SegmentTimeline *seg_tl, u64 segment_start, u64 segment_end, u64 *previous_segment_duration , Bool *first_segment_in_timeline,u32 *segment_timeline_repeat_count)
{
	GF_MPD_SegmentTimelineEntry *entry;
	u64 segment_dur = segment_end - segment_start;
	if (*previous_segment_duration == segment_dur) {
		*segment_timeline_repeat_count = *segment_timeline_repeat_count + 1;
		return;
	}

	if (*previous_segment_duration) {
		if (*segment_timeline_repeat_count) {
			entry = gf_list_last(seg_tl->entries);
			entry->repeat_count = *segment_timeline_repeat_count;
		}
	}
	*previous_segment_duration = segment_dur;
	if(segment_dur){
		GF_SAFEALLOC(entry, GF_MPD_SegmentTimelineEntry);
		if(!seg_tl->entries)seg_tl->entries=gf_list_new();
		gf_list_add(seg_tl->entries, entry);
		if (*first_segment_in_timeline) {
			entry->start_time = segment_start;
			entry->duration = (u32)segment_dur;
			*first_segment_in_timeline = GF_FALSE;
		} else {
			entry->duration = (u32)segment_dur;
		}
	}
	*segment_timeline_repeat_count = 0;
}

static void gf_dash_load_segment_timeline(GF_DASHSegmenter *dasher, GF_MPD_SegmentTimeline *seg_tl, const char *representationID, u64 *previous_segment_duration , Bool *first_segment_in_timeline,u32 *segment_timeline_repeat_count)
{
	u32 i, count;
	char szRepID[100];

	*first_segment_in_timeline = GF_TRUE;
	*segment_timeline_repeat_count = 0;
	*previous_segment_duration = 0;

	count = gf_cfg_get_key_count(dasher->dash_ctx, "SegmentsStartTimes");
	for (i=0; i<count; i++) {
		u64 start, dur;
		const char *fileName = gf_cfg_get_key_name(dasher->dash_ctx, "SegmentsStartTimes", i);
		const char *MPDTime = gf_cfg_get_key(dasher->dash_ctx, "SegmentsStartTimes", fileName);
		if (!fileName)
			break;

		sscanf(MPDTime, ""LLU"-"LLU"@%s", &start, &dur, szRepID);
		if (strcmp(representationID, szRepID)) continue;

		gf_dash_append_segment_timeline(seg_tl, start, start+dur, previous_segment_duration, first_segment_in_timeline, segment_timeline_repeat_count);
	}
}

static u64 get_presentation_time(u64 media_time, s32 ts_shift)
{
	if ((ts_shift<0) && (media_time < -ts_shift)) {
		media_time = 0;
	} else {
		media_time += ts_shift;
	}
	return media_time ;
}

static Bool is_splitable(u32 media_type)
{
	switch (media_type) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
	case GF_ISOM_MEDIA_MPEG_SUBT:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

static GF_Err isom_get_audio_info_with_m4a_sbr_ps(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *SampleRate, u32 *Channels, u8 *bitsPerSample)
{
	GF_M4ADecSpecInfo a_cfg;
	GF_ESD *esd;
	GF_Err e = gf_isom_get_audio_info(movie, trackNumber, StreamDescriptionIndex, SampleRate, Channels, bitsPerSample);
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("DASH input: could not get audio info (2), %s\n", gf_error_to_string(e)));
		return e;
	}
	if (gf_isom_get_media_subtype(movie, trackNumber, StreamDescriptionIndex) != GF_ISOM_SUBTYPE_MPEG4)
		return GF_OK;

	esd = gf_isom_get_esd(movie, trackNumber, 1);
	if (!esd) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("DASH input: broken MPEG-4 Track, no ESD found\n"));
		return GF_OK;
	}
	e = gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("DASH input: corrupted AAC Config, %s\n", gf_error_to_string(e)));
		return GF_NOT_SUPPORTED;
	}
	if (SampleRate && a_cfg.has_sbr) {
		*SampleRate = a_cfg.sbr_sr;
	}
	if (Channels) *Channels = a_cfg.nb_chan;
	gf_odf_desc_del((GF_Descriptor*)esd);
	return e;
}


static u32 cicp_get_channel_config(u32 nb_chan,u32 nb_surr, u32 nb_lfe)
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

static GF_Err gf_media_isom_segment_file(GF_ISOFile *input, const char *output_file, GF_DASHSegmenter *dasher, GF_DashSegInput *dash_input, GF_MPD_AdaptationSet *adaptation_set_obj, Bool first_in_set)
{
	u8 NbBits;
	u32 i, TrackNum, descIndex, j, count, nb_sync, ref_track_id, nb_tracks_done;
	u32 sample_duration, defaultSize, defaultDescriptionIndex, defaultRandomAccess, nb_samp, nb_done;
	u32 nb_video, nb_audio, nb_text, nb_scene, mpd_timescale;
	u8 defaultPadding;
	u16 defaultDegradationPriority;
	GF_Err e;
	char sOpt[100], sKey[100];
	char szCodecs[200], szCodec[RFC6381_CODEC_NAME_SIZE_MAX];
	u32 cur_seg, fragment_index, max_sap_type;
	GF_ISOFile *output, *bs_switch_segment;
	GF_ISOSample *sample, *next;
	GF_List *fragmenters;
	u64 MaxFragmentDuration, MaxSegmentDuration, period_duration;
	Double segment_start_time, SegmentDuration, maxFragDurationOverSegment;
	u64 presentationTimeOffset = 0;
	Double file_duration, max_segment_duration;
	u32 nb_segments, width, height, sample_rate, nb_channels, nb_surround, nb_lfe, sar_w, sar_h, fps_num, fps_denum, startNumber;
	u32 nbFragmentInSegment = 0;
	char *langCode = NULL;
	u32 index_start_range, index_end_range;
	Bool force_switch_segment = GF_FALSE;
	Bool switch_segment = GF_FALSE;
	Bool split_seg_at_rap = dasher->segments_start_with_rap;
	Bool split_at_rap = GF_FALSE;
	Bool has_rap = GF_FALSE;
	Bool next_sample_rap = GF_FALSE;
	Bool flush_all_samples = GF_FALSE;
	Bool simulation_pass = GF_FALSE;
	Bool init_segment_deleted = GF_FALSE;
	Bool first_segment_in_timeline = GF_TRUE;
	Bool store_utc = GF_FALSE;
	u64 previous_segment_duration = 0;
	u32 segment_timeline_repeat_count = 0;
	//u64 last_ref_cts = 0;
	u64 start_range, end_range, file_size, init_seg_size, ref_track_first_dts, ref_track_next_cts;
	u32 tfref_timescale = 0;
	u32 bandwidth = 0;
	GF_ISOMTrackFragmenter *tf, *tfref;
	char szMPDTempLine[2048];
	char SegmentName[GF_MAX_PATH];
	char RepSecName[200];
	char RepURLsSecName[200];
	const char *opt;
	GF_Fraction max_track_duration = {0, 1};
	Bool bs_switching_is_output = GF_FALSE;
	Bool store_dash_params = GF_FALSE;
	Bool dash_moov_setup = GF_FALSE;
	Bool segments_start_with_sap = GF_TRUE;
	Bool first_sample_in_segment = GF_FALSE;
	u32 *segments_info = NULL;
	u32 nb_segments_info = 0;
	u32 protected_track = 0;
	Double min_seg_dur, max_seg_dur, total_seg_dur, last_seg_dur;
	Bool is_bs_switching = GF_FALSE;
	Bool use_url_template = dasher->use_url_template;
	const char *seg_rad_name = dasher->seg_rad_name;
	const char *seg_ext = dasher->seg_ext;
	const char *bs_switching_segment_name = NULL;
	u64 generation_start_utc = 0;
	u64 ntpts = 0;
	GF_MPD_Representation *representation_obj = NULL;
	GF_MPD_SegmentTimeline *seg_tl = NULL;
	GF_List *segment_urls = NULL;
	Bool seg_dur_adjusted = GF_FALSE;
	SegmentName[0] = 0;
	SegmentDuration = 0;
	nb_samp = 0;
	fragmenters = NULL;

	if (!dash_input) return GF_BAD_PARAM;
	if (!seg_ext) seg_ext = "m4s";

	if (dasher->real_time && dasher->dash_ctx) {
		u32 sec, frac;
		opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "GenerationNTP");
		sscanf(opt, "%u:%u", &sec, &frac);

		generation_start_utc = sec - GF_NTP_SEC_1900_TO_1970;
		generation_start_utc *= 1000;
		generation_start_utc += ((u64) frac) * 1000 / 0xFFFFFFFFUL;
	}

	if (dasher->insert_utc) {
		ntpts = gf_net_get_ntp_ts();
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Sampling NTP TS "LLU" at "LLU" us, at UTC "LLU"\n", ntpts, gf_sys_clock_high_res(), gf_net_get_utc()));
	}

	bs_switch_segment = NULL;
	if (dasher->bs_switch_segment_file) {
		bs_switch_segment = gf_isom_open(dasher->bs_switch_segment_file, GF_ISOM_OPEN_READ, NULL);
		if (bs_switch_segment) {
			bs_switching_segment_name = gf_dasher_strip_output_dir(dasher->mpd_name, dasher->bs_switch_segment_file);
			is_bs_switching = GF_TRUE;
		}
	}

	sprintf(RepSecName, "Representation_%s", dash_input ? dash_input->representationID : "");
	sprintf(RepURLsSecName, "URLs_%s", dash_input ? dash_input->representationID : "");

	bandwidth = dash_input ? dash_input->bandwidth : 0;
	file_duration = 0;

	startNumber = 1;

	//create output file
	/*need to precompute bandwidth*/
	if ((!bandwidth && seg_rad_name && strstr(seg_rad_name, "$Bandwidth$"))
#ifdef GENERATE_VIRTUAL_REP_SRD
	        || dash_input->virtual_representation
#endif
	   ) {
		for (i=0; i<gf_isom_get_track_count(input); i++) {
			Double tk_dur = (Double) gf_isom_get_media_duration(input, i+1);
			tk_dur /= gf_isom_get_media_timescale(input, i+1);
			if (file_duration < tk_dur ) {
				file_duration = tk_dur;
			}
		}
		bandwidth = (u32) (gf_isom_get_file_size(input) / file_duration * 8);
	}

	if (dasher->dash_ctx) {
		opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "Setup");
		if (!opt || stricmp(opt, "yes") ) {
			store_dash_params=GF_TRUE;
			gf_cfg_set_key(dasher->dash_ctx, RepSecName, "ID", dash_input->representationID);
		}
	}

	opt = dasher->dash_ctx ? gf_cfg_get_key(dasher->dash_ctx, RepSecName, "InitializationSegment") : NULL;
	if (opt) {
		output = gf_isom_open(opt, GF_ISOM_OPEN_CAT_FRAGMENTS, dasher->tmpdir);
		dash_moov_setup = GF_TRUE;
	} else {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, dash_input->representationID, dash_input->nb_baseURL ? dash_input->baseURL[0] : NULL, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0, dasher->use_segment_timeline);
		output = gf_isom_open(SegmentName, GF_ISOM_OPEN_WRITE, dasher->tmpdir);
	}
	if (!output) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISOBMF DASH] Cannot open %s for writing\n", opt ? opt : SegmentName));
		e = gf_isom_last_error(NULL);
		goto err_exit;
	}
	else if (dasher->force_test_mode) {
		gf_isom_no_version_date_info(output, 1);
	}

	if (store_dash_params) {
		const char *name;

		if (!gf_cfg_get_key(dasher->dash_ctx, "DASH", "SegmentTemplate"))
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "SegmentTemplate", seg_rad_name);
		gf_cfg_set_key(dasher->dash_ctx, RepSecName, "Source", gf_isom_get_filename(input) );

		gf_cfg_set_key(dasher->dash_ctx, RepSecName, "Setup", "yes");
		name = SegmentName;
		if (bs_switch_segment) name = gf_isom_get_filename(bs_switch_segment);
		gf_cfg_set_key(dasher->dash_ctx, RepSecName, "InitializationSegment", name);

		/*store BS flag per rep - it should be stored per adaptation set but we dson't have a key for adaptation sets right now*/
		if (/*first_in_set && */ is_bs_switching)
			gf_cfg_set_key(dasher->dash_ctx, RepSecName, "BitstreamSwitching", "yes");
	} else if (dasher->dash_ctx) {
		opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "BitstreamSwitching");
		if (opt && !strcmp(opt, "yes")) {
			is_bs_switching = GF_TRUE;
			if (bs_switch_segment)
				gf_isom_delete(bs_switch_segment);
			bs_switch_segment = output;
			bs_switching_is_output = GF_TRUE;
			bs_switching_segment_name = gf_dasher_strip_output_dir(dasher->mpd_name, gf_isom_get_filename(bs_switch_segment));
		}

		opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "Bandwidth");
		if (opt) sscanf(opt, "%u", &bandwidth);

		if (dasher->use_segment_timeline) {
			opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "NbSegmentsRemoved");
			if (opt) {
				u32 nb_removed = atoi(opt);
				startNumber = 1 + nb_removed;
			}
		}
	}

	//if segment alignment not set or if first in AS, create SegmentTimeline
	if (dasher->use_segment_timeline && (first_in_set || dasher->segment_alignment_disabled) ) {
		seg_tl=gf_mpd_segmentimeline_new();

		if (dasher->dash_ctx) {
			gf_dash_load_segment_timeline(dasher, seg_tl, dash_input->representationID, &previous_segment_duration, &first_segment_in_timeline, &segment_timeline_repeat_count);
		}
	}
	else {
		segment_urls = gf_list_new();
	}

	szCodecs[0] = 0;
	nb_sync = 0;
	nb_samp = 0;
	fragmenters = gf_list_new();


#ifdef GENERATE_VIRTUAL_REP_SRD
	if (dash_input->virtual_representation) {
		char *virtual_url;
		FILE *vseg;
		GF_BitStream *bs;
		gf_media_get_rfc_6381_codec_name(input, dash_input->trackNum, szCodec, dasher->inband_param_set, GF_TRUE);
		if (strlen(szCodecs)) strcat(szCodecs, ",");
		strcpy(szCodecs, szCodec);

		sprintf(SegmentName, "virtual_rep_%d_segment.m4s", dash_input->idx_representations);
		virtual_url = gf_url_concatenate(output_file, SegmentName);
		vseg = gf_fopen(virtual_url, "w");
		bs = gf_bs_from_file(vseg, GF_BITSTREAM_WRITE);
		gf_bs_write_u32(bs, 20);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_STYP);
		gf_bs_write_u32(bs, GF_ISOM_BRAND_MSDH);
		gf_bs_write_u32(bs, 0);
		gf_bs_write_u32(bs, GF_ISOM_BRAND_MSDH);
		gf_bs_del(bs);
		gf_fclose(vseg);

		gf_free(virtual_url);

		output = NULL;
		goto restart_fragmentation_pass;
	}
#endif

	if (! dash_moov_setup) {
		e = gf_isom_clone_movie(input, output, GF_FALSE, GF_FALSE, !dasher->pssh_moof );
		if (e) goto err_exit;

		/*because of movie fragments MOOF based offset, ISOM <4 is forbidden*/
		gf_isom_set_brand_info(output, GF_ISOM_BRAND_ISO5, 1);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_ISOM, 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_ISO1, 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_ISO2, 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_ISO3, 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_MP41, 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_MP42, 0);

	}

	MaxFragmentDuration = (u32) (dasher->dash_scale * dasher->fragment_duration);
	MaxSegmentDuration = (u32) (dasher->dash_scale * dasher->segment_duration);

	/*in single segment mode, only one big SIDX is written between the end of the moov and the first fragment.
	To speed-up writing, we do a first fragmentation pass without writing any sample to compute the number of segments and fragments per segment
	in order to allocate / write to file the sidx before the fragmentation. The sidx will then be rewritten when closing the last segment*/
	if (dasher->single_file_mode==1) {
		simulation_pass = GF_TRUE;
		seg_rad_name = NULL;
	}
	/*if single file is requested, store all segments in the same file*/
	else if (dasher->single_file_mode==2) {
		seg_rad_name = NULL;
	}

	index_start_range = index_end_range = 0;

	tf = tfref = NULL;
	file_duration = 0;
	width = height = sample_rate = nb_channels = nb_surround = nb_lfe = sar_w = sar_h = fps_num = fps_denum = 0;
	langCode = NULL;


	mpd_timescale = nb_video = nb_audio = nb_text = nb_scene = 0;
	//duplicates all tracks
	for (i=0; i<gf_isom_get_track_count(input); i++) {
		u32 _w, _h, _nb_ch, vidtype;
		u32 _sr = 0;

		u32 mtype = gf_isom_get_media_type(input, i+1);
		if (mtype == GF_ISOM_MEDIA_HINT) continue;

		if (dash_input->trackNum && ((i+1) != dash_input->trackNum))
			continue;

		if (dash_input->single_track_num && ((i+1) != dash_input->single_track_num))
			continue;

		if (!dash_moov_setup) {
			e = gf_isom_clone_track(input, i+1, output, GF_FALSE, &TrackNum);
			if (e) goto err_exit;

			if (gf_isom_is_track_in_root_od(input, i+1)) gf_isom_add_track_to_root_od(output, TrackNum);

			/*remove sgpd in stbl; it would be in traf*/
			if (dasher->samplegroups_in_traf) {
				GF_TrackBox *trak = (GF_TrackBox *)gf_isom_get_track_from_file(output, TrackNum);
				if (!trak) continue;
				while (gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription)) {
					GF_Box* box = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, 0);
					gf_isom_box_del(box);
					gf_list_rem(trak->Media->information->sampleTable->sampleGroupsDescription, 0);
				}
			}

// Commenting it the code for Timed Text tracks, it may happen that we have only one long sample, fragmentation is useful
#if 0
			//if only one sample, don't fragment track
			if (gf_isom_get_sample_count(input, i+1) == 1) {
				sample = gf_isom_get_sample(input, i+1, 1, &descIndex);
				e = gf_isom_add_sample(output, TrackNum, 1, sample);
				gf_isom_sample_del(&sample);
				if (e) goto err_exit;

				continue;
			}
#endif
		} else {
			TrackNum = gf_isom_get_track_by_id(output, gf_isom_get_track_id(input, i+1));
		}

		/*set extraction mode whether setup or not*/
		vidtype = gf_isom_get_avc_svc_type(input, i+1, 1);
		if (vidtype==GF_ISOM_AVCTYPE_AVC_ONLY) {
			/*for AVC we concatenate SPS/PPS unless SVC base*/
			if (!dash_input->trackNum && dasher->inband_param_set && !dash_input->disable_inband_param_set)
				gf_isom_set_nalu_extract_mode(input, i+1, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		}
		else if (vidtype > GF_ISOM_AVCTYPE_AVC_ONLY) {
			/*for SVC we don't want any rewrite of extractors, and we don't concatenate SPS/PPS*/
			gf_isom_set_nalu_extract_mode(input, i+1, GF_ISOM_NALU_EXTRACT_INSPECT);
		}

		vidtype = gf_isom_get_hevc_lhvc_type(input, i+1, 1);
		if (vidtype == GF_ISOM_HEVCTYPE_HEVC_ONLY) {

			u32 mode = GF_ISOM_NALU_EXTRACT_INSPECT;	//because of tile tracks

			/*concatenate SPS/PPS unless LHVC base*/
			if (!dash_input->trackNum && dasher->inband_param_set && !dash_input->disable_inband_param_set)
				mode |= GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG;

			gf_isom_set_nalu_extract_mode(input, i+1, mode);
		}
		else if (vidtype > GF_ISOM_HEVCTYPE_HEVC_ONLY) {
			gf_isom_set_nalu_extract_mode(input, i+1, GF_ISOM_NALU_EXTRACT_INSPECT);
		}

		if (mtype == GF_ISOM_MEDIA_VISUAL) nb_video++;
		else if (mtype == GF_ISOM_MEDIA_AUDIO) nb_audio++;
		else if (mtype == GF_ISOM_MEDIA_TEXT || mtype == GF_ISOM_MEDIA_MPEG_SUBT || mtype == GF_ISOM_MEDIA_SUBT) nb_text++;
		else if (mtype == GF_ISOM_MEDIA_SCENE) nb_scene++;
		else if (mtype == GF_ISOM_MEDIA_DIMS) nb_scene++;

		//setup fragmenters
		if (! dash_moov_setup) {
			//new initialization segment, setup fragmentation
			gf_isom_get_fragment_defaults(input, i+1,
			                              &sample_duration, &defaultSize, &defaultDescriptionIndex,
			                              &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
			if (! dasher->no_fragments_defaults) {
				e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum),
				                                 defaultDescriptionIndex, sample_duration,
				                                 defaultSize, (u8) defaultRandomAccess,
				                                 defaultPadding, defaultDegradationPriority);

			} else {
				e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum), defaultDescriptionIndex, 0, 0, 0, 0, 0);
			}
			if (e) goto err_exit;
		} else {
			gf_isom_get_fragment_defaults(output, TrackNum,
			                              &sample_duration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
		}

		gf_media_get_rfc_6381_codec_name(input, i+1, szCodec, dasher->inband_param_set, GF_TRUE);
		if (strlen(szCodecs)) strcat(szCodecs, ",");
		strcat(szCodecs, szCodec);

		GF_SAFEALLOC(tf, GF_ISOMTrackFragmenter);
		tf->TrackID = gf_isom_get_track_id(output, TrackNum);
		tf->SampleCount = gf_isom_get_sample_count(input, i+1);
		tf->OriginalTrack = i+1;
		tf->TimeScale = gf_isom_get_media_timescale(input, i+1);
		tf->MediaType = gf_isom_get_media_type(input, i+1);
		tf->sample_duration = sample_duration;
		mpd_timescale = tf->TimeScale;

		if ( (max_track_duration.num / max_track_duration.den) < gf_isom_get_track_duration(input, i+1)) {
			max_track_duration.num = gf_isom_get_track_duration(input, i+1);
			max_track_duration.den = 1;
		}

		/* We set the reference track (for use in the sidx or prft) to be the track that has the greatest number of sync samples,
		  but not all of them. E.g. in audio+video we want the video to be used
		  if for all tracks all samples are sync, we will pick anyone (see below)
		  NOTE: This might be problematic if the file has multiple video tracks that are not sync sample aligned */
		if (gf_isom_get_sync_point_count(input, i+1)>nb_sync) {
			tfref = tf;
			nb_sync = gf_isom_get_sync_point_count(input, i+1);
		} else if (!gf_isom_has_sync_points(input, i+1)) {
			tf->all_sample_raps = GF_TRUE;
		}

		tf->finalSampleDescriptionIndex = 1;

		/*figure out if we have an initial TS*/
		if (!dash_moov_setup) {
			u32 j, edit_count = gf_isom_get_edit_segment_count(input, i+1);
			Double scale = tf->TimeScale;
			scale /= gf_isom_get_timescale(input);

			/*remove all segments*/
			gf_isom_remove_edit_segments(output, TrackNum);
			tf->media_time_to_pres_time_shift = 0;
			//clone all edits before first nomal, and stop at first normal setting duration to 0
			for (j=0; j<edit_count; j++) {
				u64 EditTime, SegDuration, MediaTime;
				u8 EditMode;

				/*get first edit*/
				gf_isom_get_edit_segment(input, i+1, j+1, &EditTime, &SegDuration, &MediaTime, &EditMode);

				if (EditMode==GF_ISOM_EDIT_NORMAL)
					SegDuration = 0;

				gf_isom_set_edit_segment(output, TrackNum, EditTime, SegDuration, MediaTime, EditMode);

				if (EditMode==GF_ISOM_EDIT_NORMAL) {
					tf->media_time_to_pres_time_shift -= (s32) MediaTime;
					break;
				}
				tf->media_time_to_pres_time_shift += (u32) (scale*SegDuration);
			}

			gf_isom_set_brand_info(output, GF_ISOM_BRAND_ISO5, 1);

			/*DASH self-init media segment*/
			if (dasher->single_file_mode==1) {
				gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_DSMS, 1);
			} else {
				gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_DASH, 1);
			}

			/*locate sample description in list if given*/
			if (bs_switch_segment) {
				u32 s_count;
				u32 sample_descs_track = gf_isom_get_track_by_id(bs_switch_segment, tf->TrackID);
				if (!sample_descs_track) {
					e = GF_BAD_PARAM;
					goto err_exit;

				}

				//the initialization segment is not yet setup for fragmentation
				if (! gf_isom_is_track_fragmented(bs_switch_segment, tf->TrackID)) {
					e = GF_BAD_PARAM;
					if (e) goto err_exit;
				}
				/*otherwise override the fragment defauls so that we are consistent with the shared init segment*/
				else {
					e = gf_isom_get_fragment_defaults(bs_switch_segment, sample_descs_track,
					                                  &sample_duration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
					if (e) goto err_exit;

					e = gf_isom_change_track_fragment_defaults(output, tf->TrackID,
					        defaultDescriptionIndex, sample_duration, defaultSize, defaultRandomAccess, defaultPadding, defaultDegradationPriority);
					if (e) goto err_exit;
				}

				/*and search in new ones the new index*/
				s_count = gf_isom_get_sample_description_count(bs_switch_segment, sample_descs_track);

				/*more than one sampleDesc, we will need to indicate all the sample descs in the file in case we produce a single file,
				otherwise the file would not be compliant*/
				if (s_count > 1) {
					gf_isom_clone_sample_descriptions(output, TrackNum, bs_switch_segment, sample_descs_track, GF_TRUE);
				}

				/*and search in new ones the new index*/
				s_count = gf_isom_get_sample_description_count(bs_switch_segment, sample_descs_track);
				if (s_count>1) {
					u32 k;
					/*remove all sample descs*/
					for (k=0; k<s_count; k++) {
						/*search in original file if we have the sample desc*/
						if (gf_isom_is_same_sample_description(input, TrackNum, 1, bs_switch_segment, sample_descs_track, k+1)) {
							tf->finalSampleDescriptionIndex = k+1;
						}
					}
				}
			}
		}
		/*restore track decode times*/
		else {
			char *opt, sKey[100];
			sprintf(sKey, "TKID_%d_NextDecodingTime", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dasher->dash_ctx, RepSecName, sKey);
			if (opt) sscanf(opt, LLU, & tf->InitialTSOffset);

			/*store presentationTimeOffset on the first rep*/
			if (store_dash_params)
				presentationTimeOffset = tf->InitialTSOffset;
		}

		tf->splitable = is_splitable(mtype);

		/*get language, width/height/layout info, audio info*/
		switch (mtype) {
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_MPEG_SUBT:
			gf_isom_get_media_language(input, i+1, &langCode);
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_DIMS:
			e = gf_isom_get_visual_info(input, i+1, 1, &_w, &_h);
			if (e == GF_OK) {
				if (_w>width) width = _w;
				if (_h>height) height = _h;
			} else {
				width  = 0;
				height = 0;
			}
			break;
		case GF_ISOM_MEDIA_AUDIO:
			//DASH-IF and MPEG disagree here:
			if (dasher->profile == GF_DASH_PROFILE_AVC264_LIVE || dasher->profile == GF_DASH_PROFILE_AVC264_ONDEMAND)
			{
				isom_get_audio_info_with_m4a_sbr_ps(input, i+1, 1, &_sr, &_nb_ch, NULL);
			}
			else {
				isom_get_audio_info_with_m4a_sbr_ps(input, i+1, 1, NULL, &_nb_ch, NULL);
			}
#ifndef GPAC_DISABLE_AV_PARSERS
			if (gf_isom_get_media_subtype(input, i+1, 1)==GF_ISOM_SUBTYPE_AC3) {
				GF_AC3Config *ac3 = gf_isom_ac3_config_get(input, i+1, 1);
				if (ac3) {
					int i;
					nb_lfe = ac3->streams[0].lfon ? 1 : 0;
					_nb_ch = gf_ac3_get_channels(ac3->streams[0].acmod);
					for (i=0; i<ac3->streams[0].nb_dep_sub; ++i) {
						assert(ac3->streams[0].nb_dep_sub == 1);
						_nb_ch += gf_ac3_get_channels(ac3->streams[0].chan_loc);
					}
					gf_free(ac3);
				}
			}
#endif
			if (_sr>sample_rate) sample_rate = _sr;
			if (_nb_ch>nb_channels) nb_channels = _nb_ch;
			gf_isom_get_media_language(input, i+1, &langCode);
			break;
		}

		if (mtype==GF_ISOM_MEDIA_VISUAL) {
			/*get duration of 2nd sample*/
			u32 sample_dur = gf_isom_get_sample_duration(input, i+1, 2);
			gf_isom_get_pixel_aspect_ratio(input, i+1, 1, &sar_w, &sar_h);
			if (! fps_num || ! fps_denum) {
				fps_num = tf->TimeScale;
				fps_denum = sample_dur;
			}
			else if (fps_num *sample_dur < tf->TimeScale * fps_denum) {
				fps_num = tf->TimeScale;
				fps_denum = sample_dur;
			}
		}


		if (file_duration < ((Double) gf_isom_get_media_duration(input, i+1)) / tf->TimeScale ) {
			file_duration = ((Double) gf_isom_get_media_duration(input, i+1)) / tf->TimeScale;
		}
		gf_list_add(fragmenters, tf);

		if (gf_isom_is_media_encrypted(input, i+1, 1)) {
			protected_track = i+1;
		}

		nb_samp += tf->SampleCount;
	}

	if (gf_list_count(fragmenters)>1)
		mpd_timescale = 1000;

	/* Finalize the selection of the reference track for sidx computations */
	if (!tfref) {
		/* if we did not find a track in which all samples are not sync samples, we pick the first track to be the reference */
		tfref = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, 0);
		assert(tfref);
	} else {
		//put tfref in first pos
		gf_list_del_item(fragmenters, tfref);
		gf_list_insert(fragmenters, tfref, 0);
	}
	tfref->is_ref_track = GF_TRUE;
	tfref_timescale = tfref->TimeScale;
	ref_track_id = tfref->TrackID;
	if (tfref->all_sample_raps)
		split_seg_at_rap = GF_TRUE;

	if (!dash_moov_setup) {
		max_track_duration.den *= gf_isom_get_timescale(input);
		max_track_duration.num *= gf_isom_get_timescale(output);
		gf_isom_set_movie_duration(output, max_track_duration.num / max_track_duration.den );
	}

	//if single segment, add msix brand if we use indexes
	gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_MSIX, ((dasher->single_file_mode==1) && dasher->enable_sidx) ? 1 : 0);

	//flush movie
	e = gf_isom_finalize_for_fragment(output, 1);
	if (e) goto err_exit;

	nb_done = 0;
	for (i=0; i<gf_list_count(fragmenters); i++) {
		tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
		if (tf->SampleCount)
			nb_done += tf->SampleCount - tf->SampleNum;
	}
	if (!nb_done) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] No samples in movie, rewriting moof and exit\n"));
		goto err_exit;
	}

	start_range = 0;
	file_size = gf_isom_get_file_size(bs_switch_segment ? bs_switch_segment : output);
	init_seg_size = file_size;

	if (dasher->dash_ctx) {
		if (store_dash_params) {
			char szVal[1024];
			sprintf(szVal, LLU, init_seg_size);
			gf_cfg_set_key(dasher->dash_ctx, RepSecName, "InitializationSegmentSize", szVal);
		} else {
			const char *opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "InitializationSegmentSize");
			if (opt) init_seg_size = atoi(opt);
		}
	}

restart_fragmentation_pass:

	for (i=0; i<gf_list_count(fragmenters); i++) {
		tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
		tf->split_sample_dts_shift = 0;
	}
	segment_start_time = 0;
	nb_segments = 0;

	nb_tracks_done = 0;
	ref_track_first_dts = (u64) -1;
	nb_done = 0;

	switch_segment=GF_TRUE;

	if (!seg_rad_name) use_url_template = GF_FALSE;

	cur_seg = 1;
	fragment_index = 1;
	if (dash_input->moof_seqnum_increase) {
		fragment_index = dash_input->moof_seqnum_increase * dash_input->idx_representations + 1;
	}
	period_duration = 0;
	split_at_rap = GF_FALSE;
	has_rap = GF_FALSE;
	next_sample_rap = GF_FALSE;
	flush_all_samples = GF_FALSE;
	force_switch_segment = GF_FALSE;
	max_sap_type = 0;
	ref_track_next_cts = 0;

	/*setup previous URL list*/
	if (dasher->dash_ctx) {
		const char *opt;
		char sKey[100];
		count = gf_cfg_get_key_count(dasher->dash_ctx, RepURLsSecName);		
		for (i=0; i<count; i++) {
			const char *key_name = gf_cfg_get_key_name(dasher->dash_ctx, RepURLsSecName, i);
			opt = gf_cfg_get_key(dasher->dash_ctx, RepURLsSecName, key_name);
			sprintf(szMPDTempLine, "     %s\n", opt);
			GF_DOMParser *segurl_parser;
			GF_XMLNode *Xml_node;
			segurl_parser = gf_xml_dom_new();
			e = gf_xml_dom_parse_string(segurl_parser, szMPDTempLine);
			if(e!=GF_OK){
			    GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load old segment URL, something went wrong \n"));
			    goto err_exit;
			}
			Xml_node=gf_xml_dom_get_root(segurl_parser);
			if(!Xml_node){
			    GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load old segment URL, something went wrong \n"));
			    goto err_exit;
			}
			gf_mpd_parse_segment_url(segment_urls,Xml_node);
			gf_xml_dom_del(segurl_parser);
		}

		opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "NextSegmentIndex");
		if (opt) {
			cur_seg = atoi(opt);
		}

		opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "NextFragmentIndex");
		if (opt) fragment_index = atoi(opt);
		opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "CumulatedDuration");
		if (opt) sscanf(opt, LLU, &period_duration);

		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
			sprintf(sKey, "TKID_%d_NextSampleNum", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dasher->dash_ctx, RepSecName, sKey);
			if (opt) tf->SampleNum = atoi(opt);

			sprintf(sKey, "TKID_%d_LastSampleCTS", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dasher->dash_ctx, RepSecName, sKey);
			if (opt) sscanf(opt, LLU, &tf->last_sample_cts);

			sprintf(sKey, "TKID_%d_NextSampleDTS", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dasher->dash_ctx, RepSecName, sKey);
			if (opt) {
				sscanf(opt, LLU, &tf->next_sample_dts);
			}

			sprintf(sKey, "TKID_%d_MediaTimeToPresTime", tf->TrackID);
			opt = gf_cfg_get_key(dasher->dash_ctx, RepSecName, sKey);
			if (opt) tf->media_time_to_pres_time_shift = atoi(opt);

			sprintf(sKey, "TKID_%d_LoopTSOffset", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dasher->dash_ctx, RepSecName, sKey);
			if (opt) sscanf(opt, LLU, &tf->loop_ts_offset);

			if (dasher->nb_secs_to_discard) {
				GF_ISOSample *sample = gf_isom_get_sample_info(input, tf->OriginalTrack, tf->SampleCount, &descIndex, NULL);
				u32 last_sample_dur = gf_isom_get_sample_duration(input, tf->OriginalTrack, tf->SampleCount);
				u64 track_dur = (u64) ((s64) last_sample_dur + sample->DTS + sample->CTS_Offset + tf->media_time_to_pres_time_shift);
				u64 pos = dasher->nb_secs_to_discard;
				u64 period_dur = track_dur * dasher->dash_scale / tf->TimeScale;
				u32 nb_seg_in_track = (u32) ( period_dur / (dasher->segment_duration * dasher->dash_scale) );
				if (nb_seg_in_track * dasher->segment_duration * dasher->dash_scale < period_dur)
					nb_seg_in_track++;
				pos *= tf->TimeScale;
				while (1) {
					if (pos < track_dur) break;
					pos -= track_dur;
					tf->InitialTSOffset += sample->DTS + last_sample_dur;
					cur_seg += nb_seg_in_track;
					period_duration += period_dur;
				}
				gf_isom_sample_del(&sample);

				sprintf(sKey, "TKID_%d_NextDecodingTime", tf->TrackID);
				sprintf(sOpt, LLU, tf->InitialTSOffset);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, sOpt);
			}
		}
	}

	gf_isom_set_next_moof_number(output, dasher->initial_moof_sn + fragment_index);


	max_segment_duration = 0;
	min_seg_dur = max_seg_dur = total_seg_dur = last_seg_dur = 0;


	while ( (count = gf_list_count(fragmenters)) ) {
		Bool store_pssh = GF_FALSE;
		u32 ref_SAP_type = 0;
#ifdef GENERATE_VIRTUAL_REP_SRD
		if (dash_input->virtual_representation)
			break;
#endif

		if (switch_segment) {
			if (dasher->subduration && (segment_start_time + MaxSegmentDuration/2 >= dasher->dash_scale * dasher->subduration)) {
				/*done with file (next segment will exceed of more than half the requested subduration : store all fragmenters state and abord*/
				break;
			}

                       SegmentDuration = 0;
			switch_segment = GF_FALSE;
			first_sample_in_segment = GF_TRUE;

			if (simulation_pass) {
				segments_info = (u32 *)gf_realloc(segments_info, sizeof(u32) * (nb_segments_info+1) );
				segments_info[nb_segments_info] = 0;
				nb_segments_info++;
				e = GF_OK;
			} else {
				start_range = gf_isom_get_file_size(output);
				if (seg_rad_name) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, is_bs_switching, SegmentName, output_file, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, period_duration + (u64)segment_start_time*mpd_timescale/dasher->dash_scale, bandwidth, cur_seg, dasher->use_segment_timeline);
					e = gf_isom_start_segment(output, SegmentName, dasher->fragments_in_memory);

					/*we are in bitstream switching mode, delete init segment*/
					if (is_bs_switching && !init_segment_deleted) {
						init_segment_deleted = GF_TRUE;
						if (dasher->bs_switch_segment_file && strcmp(dasher->bs_switch_segment_file, gf_isom_get_filename(output))) {
							gf_delete_file(gf_isom_get_filename(output));
						}
					}

				} else {
					e = gf_isom_start_segment(output, NULL, dasher->fragments_in_memory);
				}
				if (dasher->pssh_moof)
					store_pssh = GF_TRUE;

				if (tfref /* tfref set to NULL after its last sample is processed */ && dasher->insert_utc) {
					store_utc = GF_TRUE;
				}
			}

			cur_seg++;
			if (e) goto err_exit;
		}


		maxFragDurationOverSegment=0;

		sample = NULL;

		if (simulation_pass) {
			segments_info[nb_segments_info-1] ++;
			e = GF_OK;
		} else {

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, starting fragment %d\n", SegmentName, nbFragmentInSegment));
			e = gf_isom_start_fragment(output, GF_TRUE);
			if (e) goto err_exit;

			for (i=0; i<count; i++) {
				tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
				if (tf->done) continue;

				if (dasher->initial_tfdt && (tf->TimeScale != dasher->dash_scale)) {
					Double scale = tf->TimeScale;
					scale /= dasher->dash_scale;
					tf->start_tfdt = (u64) (dasher->initial_tfdt*scale) + tf->InitialTSOffset + tf->next_sample_dts;
				} else {
					tf->start_tfdt = tf->InitialTSOffset + tf->next_sample_dts;
				}

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, fragment %d, tfdt "LLD" on TrackID %d\n", SegmentName, nbFragmentInSegment, tf->start_tfdt, tf->TrackID));

				gf_isom_set_traf_base_media_decode_time(output, tf->TrackID, tf->start_tfdt);
				if (!SegmentDuration) tf->min_cts_in_segment = (u64)-1;

				if (!seg_tl && use_url_template) {
					Double sdur = ((Double)MaxSegmentDuration) / dasher->dash_scale;
					Double diff = (cur_seg-2) * sdur;
					diff -= ((Double)tf->start_tfdt) / tf->TimeScale;
					if (ABS(diff) > sdur/2) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment %s (Number #%d): drift between MPD timeline and tfdt exceeds 50%% of segment duraion (MPD time minus TFDT %lf secs) - bitstream will not be compliant, try using segment template or reencode\n", SegmentName, cur_seg-1, diff));
					}
				}
			}
			if (store_pssh) {
				e = gf_isom_clone_pssh(output, input, GF_TRUE);
			}
		}

		//process track by track
		for (i=0; i<count; i++) {
			Bool has_roll, is_rap;
			s32 roll_distance;
			u32 SAP_type = 0;

			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
			if (tf->is_ref_track)
				has_rap = GF_FALSE;

			if (tf->done) continue;
			//if not the first TRAF in our moof and single traf per moof is requested, start a new moof
			if (!simulation_pass && dasher->single_traf_per_moof && (i>0) ) {
				e = gf_isom_start_fragment(output, GF_TRUE);
				if (e) goto err_exit;
			}

			//ok write samples
			while (1) {
				Bool loop_track = GF_FALSE;
				Bool force_eos = GF_FALSE;
				Bool stop_frag = GF_FALSE;
				Bool is_redundant_sample = GF_FALSE;
				u32 split_sample_duration = 0;
				Double clamp_duration = dash_input->clamp_duration;
				if (!clamp_duration) clamp_duration = dash_input->media_duration;

				/*first sample in the fragment */
				if (!sample) {
					sample = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 1, &descIndex);
					if (!sample) {
						e = gf_isom_last_error(input);
						goto err_exit;
					}
					sample->DTS += tf->loop_ts_offset;

					/*FIXME - use negative ctts to indicate "past" DTS for splitted sample*/
					/* take into account the possible split of the last sample in the previous fragment */
					if (tf->split_sample_dts_shift) {
						sample->DTS += tf->split_sample_dts_shift;
						is_redundant_sample = GF_TRUE;
					}

					/*also get SAP type - this is not needed if sample is not NULL as SAP type was computed for "next sample" in previous loop*/
					if (sample->IsRAP) {
						SAP_type = sample->IsRAP;
					} else {
						SAP_type = 0;
						e = gf_isom_get_sample_rap_roll_info(input, tf->OriginalTrack, tf->SampleNum + 1, &is_rap, &has_roll, &roll_distance);
						if (e==GF_OK) {
							if (is_rap) SAP_type = 3;
							else if (has_roll && (roll_distance>=0) ) SAP_type = 4;
						}
					}
				}

				gf_isom_get_sample_padding_bits(input, tf->OriginalTrack, tf->SampleNum+1, &NbBits);

				next = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 2, &j);

				if (clamp_duration && next && clamp_duration*tf->TimeScale <= next->DTS) {
					gf_isom_sample_del(&next);
					next = NULL;
				}

				if (next) {
					next->DTS += tf->loop_ts_offset;
					sample_duration = (u32) (next->DTS - sample->DTS);
				}
				//in dynamic mode we will loop, pay attention to the timing
				else if (!dasher->disable_loop) {
					if (clamp_duration) {
						/* simple round with (int)+.5 to avoid trucating .99999 to 0 */
						sample_duration = (u32)(clamp_duration*tf->TimeScale - (sample->DTS - tf->loop_ts_offset) + 0.5);
						//it may happen that the sample duration is 0 if the clamp duration is right after the sample DTS and timescale is not big enough to express it - force to 1
						if (sample_duration==0)
							sample_duration=1;
					} else {
						sample_duration = (u32) (gf_isom_get_media_duration(input, tf->OriginalTrack) - (sample->DTS - tf->loop_ts_offset) );

						assert(tf->SampleNum+1 == tf->SampleCount);
					}

					tf->loop_ts_offset = tf->next_sample_dts + sample_duration;
					loop_track = GF_TRUE;
					next = gf_isom_get_sample(input, tf->OriginalTrack, 1, &j);
					next->DTS += tf->loop_ts_offset;
				} else if (clamp_duration) {
					sample_duration = (u32)(clamp_duration*tf->TimeScale - (sample->DTS - tf->loop_ts_offset) + 0.5);
					force_eos = GF_TRUE;
				} else {
					sample_duration = (u32) (gf_isom_get_media_duration(input, tf->OriginalTrack) - (sample->DTS - tf->loop_ts_offset));
				}

				if (tf->splitable) {
					/* Evaluate if we need to split the current sample
					(if it goes beyond the segment boundary,
					we do not split a sample if it exceeds the fragment boundary) */
					if (tf->is_ref_track) {
						u64 frag_dur = (tf->FragmentLength + sample_duration) * dasher->dash_scale / tf->TimeScale;
						/*if media segment about to be produced is longer than max segment length, force segment split*/
						if (SegmentDuration + frag_dur > MaxSegmentDuration) {
							split_sample_duration = sample_duration;
							sample_duration = (u32) (tf->TimeScale * (MaxSegmentDuration - SegmentDuration) / dasher->dash_scale - tf->FragmentLength);
							assert(sample_duration);
							split_sample_duration -= sample_duration;

							/*since we split this sample we have to stop fragmenting afterwards*/
							stop_frag = GF_TRUE;
							nb_samp++;
						}
					} else if (tfref
					           /* we split the sample not at the "perfect" segment boundary
							   but at the "real" segment boundary given by the end of the last sample of the reference track (if any)
							   there may not be a reference track anymore if all samples have been used */
					           && ((tf->next_sample_dts + sample_duration) * tfref_timescale > tfref->next_sample_dts * tf->TimeScale)) {
						split_sample_duration = sample_duration;
						sample_duration = (u32) ( (tfref->next_sample_dts * tf->TimeScale)/tfref_timescale - tf->next_sample_dts );
						split_sample_duration -= sample_duration;

						/*since we split this sample we have to stop fragmenting afterwards*/
						stop_frag = GF_TRUE;
						nb_samp++;
					}
				}

				assert((s32) sample_duration > 0);

				if (tf->is_ref_track) {
					if (segments_start_with_sap && first_sample_in_segment) {
						first_sample_in_segment = GF_FALSE;
						if (!SAP_type) segments_start_with_sap = GF_FALSE;
					}
					if (ref_track_first_dts > sample->DTS)
						ref_track_first_dts = sample->DTS;

					if (next) {
						u64 next_cts = next->DTS + next->CTS_Offset;
						if (split_sample_duration)
							next_cts -= split_sample_duration;

						if (ref_track_next_cts<next_cts) {
							ref_track_next_cts = next_cts;
						}
					} else {
						u64 cts = gf_isom_get_media_duration(input, tf->OriginalTrack);
						if (cts>ref_track_next_cts) ref_track_next_cts = cts;
						else ref_track_next_cts += sample_duration;
					}
				}

				if (simulation_pass) {
					e = GF_OK;
				} else {
					if (tf->is_ref_track && store_utc) {
						//frame timestamp is composition time (tfdt + cts_offset) in movie timeline (eg take edit list into account) still in media timescale
						u64 associated_pts = tf->start_tfdt + sample->CTS_Offset + tf->media_time_to_pres_time_shift;
						gf_isom_set_fragment_reference_time(output, tf->TrackID, ntpts, associated_pts);
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, storing NTP TS "LLU" at "LLU" us, at UTC "LLU"\n", SegmentName, ntpts, gf_sys_clock_high_res(), gf_net_get_utc()));
						ntpts = gf_net_get_ntp_ts();

						store_utc = GF_FALSE;
					}

					/*override descIndex with final index used in file*/
					descIndex = tf->finalSampleDescriptionIndex;
					e = gf_isom_fragment_add_sample(output, tf->TrackID, sample, descIndex,
					                                sample_duration, NbBits, 0, is_redundant_sample);
					if (e)
						goto err_exit;

					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, fragment %d, current fragment length %d, adding sample with DTS %d from TrackID %d\n", SegmentName, nbFragmentInSegment, tf->FragmentLength, sample->DTS, tf->TrackID));

					if (sample->DTS + sample->CTS_Offset < tf->min_cts_in_segment)
						tf->min_cts_in_segment = sample->DTS + sample->CTS_Offset;

					e = gf_isom_fragment_add_sai(output, input, tf->TrackID, tf->SampleNum + 1);
					if (e) goto err_exit;

					/*copy subsample information*/
					e = gf_isom_fragment_copy_subsample(output, tf->TrackID, input, tf->OriginalTrack, tf->SampleNum + 1, dasher->samplegroups_in_traf);
					if (e)
						goto err_exit;

					gf_set_progress("ISO File Fragmenting", nb_done, nb_samp);
					nb_done++;
				}

				tf->last_sample_cts = sample->DTS + sample->CTS_Offset;
				tf->next_sample_dts = sample->DTS + sample_duration;

				if (split_sample_duration) {
					gf_isom_sample_del(&next);
					sample->DTS += sample_duration;
				} else {
					gf_isom_sample_del(&sample);
					sample = next;
					tf->SampleNum += 1;
					tf->split_sample_dts_shift = 0;
				}
				tf->FragmentLength += sample_duration;

				//done with current sample

				if (loop_track)
					tf->SampleNum = 0;

				if (force_eos)
					tf->SampleNum = tf->SampleCount;

				/*compute SAP type*/
				if (sample) {
					if (sample->IsRAP) {
						SAP_type = sample->IsRAP;
					} else {
						SAP_type = 0;
						e = gf_isom_get_sample_rap_roll_info(input, tf->OriginalTrack, tf->SampleNum + 1, &is_rap, &has_roll, NULL);
						if (e==GF_OK) {
							if (is_rap)
								SAP_type = 3;
							else if (has_roll && (roll_distance>=0) )
								SAP_type = 4;
						}
					}
				}

				if (next && SAP_type) {
					if (tf->is_ref_track) {
						if (split_seg_at_rap) {
							u64 next_sap_time;
							u64 frag_dur, next_dur;
							next_dur = gf_isom_get_sample_duration(input, tf->OriginalTrack, tf->SampleNum + 1);
							if (!next_dur) next_dur = sample_duration;
							/*duration of fragment if we add this rap*/
							frag_dur = (tf->FragmentLength+next_dur)*dasher->dash_scale / tf->TimeScale;
							next_sample_rap = GF_TRUE;
							next_sap_time = isom_get_next_sap_time(input, tf->OriginalTrack, tf->SampleCount, tf->SampleNum + 2);
							/*if no more SAP after this one, do not switch segment*/
							if (next_sap_time) {
								Double SegmentStart;
								u64 next_sap_dur;
								Bool do_split = GF_FALSE;

								next_sap_time += tf->loop_ts_offset;
								if (dasher->segment_duration_strict) {
									SegmentStart = 0;
								} else {
									SegmentStart = segment_start_time;
								}
								/*this is the fragment duration from last sample added to next SAP*/
								next_sap_dur = frag_dur + (s64) (next_sap_time - tf->next_sample_dts - next_dur) * dasher->dash_scale / tf->TimeScale;

								if (!tf->splitable) {
									/*all samples RAP: if adding up to next sap exceeds max segment length, force segment split*/
									if (SegmentDuration + next_sap_dur >  MaxSegmentDuration) {
										if (tf->all_sample_raps) {
											do_split = GF_TRUE;
										} else {
										/*not all samples RAP: if adding up to next sap equals or exceeds 1.5*max segment length, we can split here since next seg after this one would at least be 0.5*segment_duration
										this allows flexibility in segment length to cope with varying GOP length
										*/
											if (SegmentDuration + next_sap_dur >= 3*MaxSegmentDuration/2) {
												do_split = GF_TRUE;
											}
										}
									}
								}

								/* we need to force the creation of a new segment if the next split would result in a zero duration sample*/
								if (tf->splitable && (SegmentStart + SegmentDuration + next_sap_dur == SegmentStart + MaxSegmentDuration)) {
									do_split = GF_TRUE;
								}
								/*avoid drift with segment template*/
								if (!tf->all_sample_raps && !seg_tl && use_url_template) {
									Double sdur = ((Double)MaxSegmentDuration) / dasher->dash_scale;
									Double diff;
									//tfdt of first sample in next segment
									diff = ((Double)next_sap_time) / tf->TimeScale;
									//minus segment start time of next segment
									diff -= (cur_seg-1) * sdur;
									//should be less than 50% of duration. If not we need to break
									//into smaller segments
									if (ABS(diff) > sdur/2) {
										do_split = GF_TRUE;
										seg_dur_adjusted = GF_TRUE;
									}
								}

								if (do_split) {
									split_at_rap = GF_TRUE;
									/*force new segment*/
									force_switch_segment = GF_TRUE;
									stop_frag = GF_TRUE;
								}
							}
						} else if (!has_rap) {
							if (tf->FragmentLength == sample_duration) has_rap = 2;
							else has_rap = 1;
						}
					}
				} else {
					next_sample_rap = GF_FALSE;
				}

				if (tf->SampleNum==tf->SampleCount) {
					stop_frag = GF_TRUE;
				} else if (tf->is_ref_track) {
					/* NOTE: we don't need to check for split_sample_duration because if it is used, stop_frag should already be TRUE */
					/*fragmenting on "clock" track: no drift control*/
					if (!dasher->fragments_start_with_rap || ( (next && next->IsRAP) || split_at_rap) ) {
						Bool segment_dur_exceeded = GF_FALSE;
						Bool last_frag_in_seg = GF_FALSE;
						u64 seg_dur = (u64) (SegmentDuration + (tf->FragmentLength * dasher->dash_scale / tf->TimeScale));

						if (seg_dur >= MaxSegmentDuration) {
							last_frag_in_seg = GF_TRUE;
							if (!split_seg_at_rap) {
								segment_dur_exceeded = GF_TRUE;
							} else if (seg_dur >= 3*MaxSegmentDuration/2) {
								segment_dur_exceeded = GF_TRUE;
							}
						}

						if ((!last_frag_in_seg && (tf->FragmentLength * dasher->dash_scale >= MaxFragmentDuration * tf->TimeScale))
						        /* if we don't split segment at rap and if the current fragment makes the segment longer than required, stop the current fragment */
						        || (!split_seg_at_rap && segment_dur_exceeded)
						   ) {
							stop_frag = GF_TRUE;
							if (split_seg_at_rap && (next && !next->IsRAP) && segment_dur_exceeded) {
								GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Require segment starts with RAP but next segment will not be compatible\n"));
							}
						}
					}
				}
				/*do not abort fragment if ref track is done, put everything in the last fragment*/
				else if (!flush_all_samples) {
					u64 ept_next;
					u64 tref_ept_next = get_presentation_time(ref_track_next_cts, tfref->media_time_to_pres_time_shift);
					/*get next sample dts: if greater than tref EPT, abort. This ensures that we have at most
					one au wihth EPT less than ref EPT*/
					u64 next_dur = gf_isom_get_sample_duration(input, tf->OriginalTrack, tf->SampleNum + 1);
					if (!next_dur) next_dur = sample_duration;
					ept_next = get_presentation_time(tf->next_sample_dts + next_dur, tf->media_time_to_pres_time_shift);
					/*fragmenting on "non-clock" track: drift control*/
					if (ept_next * tfref_timescale > tref_ept_next * tf->TimeScale)
						stop_frag = GF_TRUE;
				}

				if (stop_frag) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, done with fragment %d, fragment length %d\n", SegmentName, nbFragmentInSegment, tf->FragmentLength));
					gf_isom_sample_del(&sample);
					sample = next = NULL;

					ref_SAP_type = SAP_type;

					//only compute max dur over segment for the track used for indexing / deriving MPD start time
					if (!tfref || (tf->is_ref_track)) {
						Double f_dur = (Double)( tf->FragmentLength ) * dasher->dash_scale / tf->TimeScale;
						if (maxFragDurationOverSegment <= f_dur) {
							maxFragDurationOverSegment = f_dur;
						}
					}
					tf->FragmentLength = 0;
					/* propagate the portion of the current 'split' sample that has already been used in this fragment
					   to the next fragment */
					if (split_sample_duration)
						tf->split_sample_dts_shift += sample_duration;

					//if (tf==tfref)
					//last_ref_cts = tf->last_sample_cts;

					break;
				}
			}

			if (tf->SampleNum==tf->SampleCount) {
				tf->done = GF_TRUE;
				nb_tracks_done++;
				if (tf->is_ref_track) {
					tfref = NULL;
					flush_all_samples = GF_TRUE;
				}
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, done with fragment %d, fragment length %d\n", SegmentName, nbFragmentInSegment, tf->FragmentLength));

		SegmentDuration += maxFragDurationOverSegment;
               if (!use_url_template) {
                   GF_MPD_SegmentURL *seg_url;
                   const char *name = gf_dasher_strip_output_dir(dasher->mpd_name, SegmentName);
                   seg_url = gf_mpd_segmenturl_new(name, 0, 0, NULL, 0, 0);
                   seg_url->duration=SegmentDuration;
                   if (dasher->dash_ctx) {
                       char szKey[100], szVal[4046];
                       sprintf(szKey, "UrlInfo%d", cur_seg );
                       sprintf(szVal, "<SegmentURL media=\"%s\"/>", name);
                       gf_cfg_set_key(dasher->dash_ctx, RepURLsSecName, szKey, szVal);
                   }
               gf_list_add(segment_urls, seg_url);
               }

		/*if no simulation and no SIDX or realtime is used, flush fragments as we write them*/
		if (!simulation_pass && (!dasher->enable_sidx || dasher->real_time) ) {

			if (tfref && dasher->real_time) {
				u64 utc, end_time = tfref->InitialTSOffset + tfref->next_sample_dts - tfref->sample_duration;
				end_time *= 1000;
				end_time /= tfref->TimeScale;

				assert(tfref->InitialTSOffset + tfref->next_sample_dts >= tfref->sample_duration);

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, going to sleep before flushing fragment %d at "LLU" us, at UTC "LLU" (flush target UTC "LLU")\n", SegmentName, nbFragmentInSegment, gf_sys_clock_high_res(), gf_net_get_utc(), generation_start_utc + end_time));
				while (1) {
					utc = gf_net_get_utc();
					if (utc >= generation_start_utc + end_time) break;
					gf_sleep(1);
				}

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, flushing fragment %d at "LLU" us, at UTC "LLU" (flush target UTC "LLU")\n", SegmentName, nbFragmentInSegment,
				                                   gf_sys_clock_high_res(), gf_net_get_utc(), generation_start_utc + end_time));
			}

			e = gf_isom_flush_fragments(output, flush_all_samples ? GF_TRUE : GF_FALSE);
			if (e) goto err_exit;

			nbFragmentInSegment++;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s, fragment %d flushed\n", SegmentName, nbFragmentInSegment));

		if (force_switch_segment || flush_all_samples || ((SegmentDuration >= MaxSegmentDuration) && (!split_seg_at_rap || next_sample_rap || tf->splitable))) {
			//don't update min_seg_dur if this is the last segment
			if (!min_seg_dur || (!flush_all_samples && (min_seg_dur>SegmentDuration)))
				min_seg_dur = SegmentDuration;

			//remember max sap type at start of segment
			if (ref_SAP_type > max_sap_type)
				max_sap_type = ref_SAP_type;

			if (max_seg_dur < SegmentDuration)
				max_seg_dur = SegmentDuration;

			total_seg_dur += SegmentDuration;
			last_seg_dur = SegmentDuration;

			if (seg_tl) {
				u64 s_start, s_end;

				u32 tick_adjust = 0;
				//since dash scale and ref track used for segmentation may not have the same timescale we will have drift in segment timelines. Adjust it
				if (tfref) {
					s64 seg_start_time_min_cts = (s64) (tfref->min_cts_in_segment + tfref->media_time_to_pres_time_shift) * dasher->dash_scale;
					u64 seg_start_time_mpd = (period_duration + (u64)segment_start_time) * tfref->TimeScale;

					//if first CTS in segment is less than 0, this means that we have AUs that are not presented due to edit list.
					//adjust the segment duration accordingly.
					if (seg_start_time_min_cts<0) {
						s64 mpd_time_adjust = seg_start_time_min_cts / tfref->TimeScale;
						if ((s64) SegmentDuration < - mpd_time_adjust) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error: Segment duration is less than media time from edit list (%d vs %d)\n", MaxSegmentDuration, -tfref->media_time_to_pres_time_shift));
							e = GF_BAD_PARAM;
							goto err_exit;
						}
						SegmentDuration += mpd_time_adjust;
						seg_start_time_min_cts=0;
					}

					if (seg_start_time_mpd != seg_start_time_min_cts) {
						//compute diff in dash timescale
						Double diff = (Double) seg_start_time_min_cts;
						diff -= (Double) seg_start_time_mpd;
						diff /= tfref->TimeScale;

						//if we are ahead we will adjust keep track of how many ticks we miss
						if (diff >= 1) {
							tick_adjust = (u32) diff;
							if (tick_adjust > 1) {
								GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Drift between minCTS of segment and MPD start time is %g s\n", diff/dasher->dash_scale));
							}
						}
					}
				}

				s_start = (u64) ( ((Double)period_duration + segment_start_time) * mpd_timescale );
				s_end = (u64) ( ((Double)period_duration + segment_start_time + SegmentDuration + (Double) tick_adjust)*mpd_timescale);
				s_start /= dasher->dash_scale;
				s_end /= dasher->dash_scale;
				//adjust
				gf_dash_append_segment_timeline(seg_tl, s_start, s_end, &previous_segment_duration, &first_segment_in_timeline, &segment_timeline_repeat_count);
				period_duration += tick_adjust;
			}
			if (dasher->mpd->max_segment_duration * dasher->dash_scale < SegmentDuration) {
				dasher->mpd->max_segment_duration = (u32)(SegmentDuration);
				dasher->mpd->max_segment_duration /= dasher->dash_scale;

			}

			gf_dasher_store_segment_info(dasher, dash_input->representationID, SegmentName, period_duration + (u64)segment_start_time, (u64)(period_duration + segment_start_time + SegmentDuration));

			segment_start_time += SegmentDuration;
			nb_segments++;
			/*if (max_segment_duration * dasher->dash_scale <= SegmentDuration) {
				max_segment_duration = (Double) (s64) SegmentDuration;
				max_segment_duration /= dasher->dash_scale;
			}*/
			force_switch_segment = GF_FALSE;
			switch_segment = GF_TRUE;

			split_at_rap = GF_FALSE;
			has_rap = GF_FALSE;
			/*restore fragment duration*/
			MaxFragmentDuration = (u32) (dasher->fragment_duration * dasher->dash_scale);

			if (!simulation_pass) {
				u64 idx_start_range, idx_end_range;
				Bool last_segment = flush_all_samples ? GF_TRUE : GF_FALSE;

				if (dasher->subduration && (segment_start_time + MaxSegmentDuration/2 >= dasher->dash_scale * dasher->subduration)) {
					//onDemand with subdur: if we are done dashing the content set last segment to TRUE to flush sidx
					//live and end of period forced, force last segment
					if (dasher->force_period_end || (dasher->single_file_mode==1) )
						last_segment = GF_TRUE;
				}

				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Closing segment %s at "LLU" us, at UTC "LLU" - segment AST "LLU" (MPD AST "LLU")\n", SegmentName, gf_sys_clock_high_res(), gf_net_get_utc(), generation_start_utc + period_duration + (u64)segment_start_time, generation_start_utc ));
				gf_isom_close_segment(output, dasher->enable_sidx ? dasher->subsegs_per_sidx : 0, dasher->enable_sidx ? ref_track_id : 0, ref_track_first_dts, tfref ? tfref->media_time_to_pres_time_shift : tf->media_time_to_pres_time_shift, ref_track_next_cts, dasher->daisy_chain_sidx, last_segment, GF_FALSE, dasher->segment_marker_4cc, &idx_start_range, &idx_end_range, NULL);
				nbFragmentInSegment = 0;

				//take care of scalable reps
				if (dash_input->moof_seqnum_increase) {
					u32 frag_index = gf_isom_get_next_moof_number(output) + dash_input->nb_representations * dash_input->moof_seqnum_increase;
					gf_isom_set_next_moof_number(output, dasher->initial_moof_sn + frag_index);
				}

				ref_track_first_dts = (u64) -1;

				if (!seg_rad_name) {
					file_size = gf_isom_get_file_size(output);
					end_range = file_size - 1;
					if (dasher->single_file_mode!=1) {
						GF_MPD_SegmentURL *seg_url;
						seg_url = gf_mpd_segmenturl_new(NULL, start_range, end_range, NULL, idx_start_range, idx_end_range);
                                               seg_url->duration=SegmentDuration;
						gf_list_add(segment_urls, seg_url);
						if (dasher->dash_ctx) {
							char szKey[100], szVal[4046];
							sprintf(szKey, "UrlInfo%d", cur_seg );
							sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
							gf_cfg_set_key(dasher->dash_ctx, RepURLsSecName, szKey, szVal);
						}
					}
				} else {
					file_size += gf_isom_get_file_size(output);
				}
			}

                        SegmentDuration = 0;

			/*next fragment will exceed segment length, abort fragment at next rap (possibly after MaxSegmentDuration)*/
			if (split_seg_at_rap && SegmentDuration && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
				if (!split_at_rap) {
					split_at_rap = GF_TRUE;
				}
			}
		}
		if (nb_tracks_done==count)
			break;
	}

	if (simulation_pass) {
		/*OK, we have all segments and frags per segments*/
		gf_isom_allocate_sidx(output, dasher->subsegs_per_sidx, dasher->daisy_chain_sidx, nb_segments_info, segments_info, &index_start_range, &index_end_range );
		gf_free(segments_info);
		segments_info = NULL;
		simulation_pass = GF_FALSE;
		/*reset fragmenters*/
		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
			tf->done = GF_FALSE;
			tf->last_sample_cts = 0;
			tf->next_sample_dts = 0;
			tf->FragmentLength = 0;
			tf->SampleNum = 0;
			if (tf->is_ref_track)
				tfref = tf;
		}
		goto restart_fragmentation_pass;
	}

	/*flush last segment*/
	if (!switch_segment) {
		u64 idx_start_range, idx_end_range;

		segment_start_time += SegmentDuration;
		total_seg_dur += SegmentDuration;
		last_seg_dur = SegmentDuration;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Closing segment %s at "LLU" us, at UTC "LLU"\n", SegmentName, gf_sys_clock_high_res(), gf_net_get_utc()));
		gf_isom_close_segment(output, dasher->enable_sidx ? dasher->subsegs_per_sidx : 0, dasher->enable_sidx ? ref_track_id : 0, ref_track_first_dts, tfref ? tfref->media_time_to_pres_time_shift : tf->media_time_to_pres_time_shift, ref_track_next_cts, dasher->daisy_chain_sidx, GF_TRUE, GF_FALSE, dasher->segment_marker_4cc, &idx_start_range, &idx_end_range, NULL);
		nb_segments++;

		if (!seg_rad_name) {
			file_size = gf_isom_get_file_size(output);
			end_range = file_size - 1;
			if (dasher->single_file_mode!=1) {
				GF_MPD_SegmentURL *seg_url;
				seg_url = gf_mpd_segmenturl_new(NULL, start_range, end_range, NULL, idx_start_range, idx_end_range);
				gf_list_add(segment_urls, seg_url);
				if (dasher->dash_ctx) {
					char szKey[100], szVal[4046];
					sprintf(szKey, "UrlInfo%d", cur_seg );
					sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
					gf_cfg_set_key(dasher->dash_ctx, RepURLsSecName, szKey, szVal);
				}
			}
		} else {
			file_size += gf_isom_get_file_size(output);
		}
	}
	//close timeline
	if (seg_tl) {
		gf_dash_append_segment_timeline(seg_tl, (u64)segment_start_time, (u64)(segment_start_time+SegmentDuration), &previous_segment_duration, &first_segment_in_timeline, &segment_timeline_repeat_count);
	}
	else if (!dasher->use_segment_timeline) {
		if (3*min_seg_dur < max_seg_dur) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segment duration variation is higher than the +/- 50%% allowed by DASH-IF (min %g, max %g) - please reconsider encoding\n", (Double) min_seg_dur / dasher->dash_scale, (Double) max_seg_dur / dasher->dash_scale));
		}
		if (dasher->dash_ctx) {
			max_segment_duration = dasher->segment_duration;
			if (!seg_dur_adjusted && ((Double) max_seg_dur / dasher->dash_scale < dasher->segment_duration/2)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segment duration is smaller than required (require %g s but DASH-ing only %g s)\n", dasher->segment_duration, (Double) max_seg_dur / dasher->dash_scale));
			}
		} else {
			if (nb_segments == 1) {
				max_segment_duration = (Double) total_seg_dur;
				max_segment_duration /= nb_segments * dasher->dash_scale;
			} else {
				max_segment_duration = (Double) (total_seg_dur - last_seg_dur);
				max_segment_duration /= (nb_segments - 1) * dasher->dash_scale;
			}
		}
	}


    if (!bandwidth)
            bandwidth = (u32) (file_size * 8 / file_duration);

    bandwidth += dash_input->dependency_bandwidth;
    dash_input->bandwidth = bandwidth;

	/* max segment duration */
	if (dasher->dash_ctx) {
		Double seg_dur;
		opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration");
		if (opt) {
			seg_dur = atof(opt);
			if (seg_dur < max_segment_duration) {
				sprintf(sOpt, "%f", max_segment_duration);
				gf_cfg_set_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
			} else {
				max_segment_duration = seg_dur;
			}
		} else {
			sprintf(sOpt, "%f", max_segment_duration);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
		}
	}

	if (!max_segment_duration || !dasher->segment_duration_strict) {
		max_segment_duration = dasher->segment_duration;
	}

	/* Write adaptation set content protection element */
	if (protected_track && first_in_set && (dasher->cp_location_mode != GF_DASH_CPMODE_REPRESENTATION)) {
		GF_MPD_Descriptor *desc = gf_isom_get_content_protection_desc(input, protected_track);		
		gf_list_add(adaptation_set_obj->content_protection, desc);
	}

	if (use_url_template) {
		GF_MPD_SegmentTemplate *seg_template;
		/*segment template does not depend on file name, write the template at the adaptationSet level*/
		if (!dasher->variable_seg_rad_name && first_in_set) {
			GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
			seg_template->start_number=(u32)-1;
			adaptation_set_obj->segment_template = seg_template;
			const char *rad_name = gf_dasher_strip_output_dir(dasher->mpd_name, seg_rad_name);
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, NULL, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, 0, 0, dasher->use_segment_timeline);
			seg_template->timescale = mpd_timescale;
			seg_template->media = gf_strdup(SegmentName);
			seg_template->start_number = startNumber;
			if (dasher->ast_offset_ms<0) {
				seg_template->availability_time_offset = -(Double)dasher->ast_offset_ms / 1000.0;
			}
			if (!dasher->use_segment_timeline) {
				if (!max_segment_duration) {
					max_segment_duration = dasher->segment_duration;
				}
				seg_template->duration = (u64)(max_segment_duration * mpd_timescale + 0.5);
			}
			/*in BS switching we share the same IS for all reps*/
			if (is_bs_switching) {
				strcpy(SegmentName, bs_switching_segment_name);
			} else {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, NULL, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0, dasher->use_segment_timeline);
			}
			seg_template->initialization = gf_strdup(SegmentName);
			seg_template->presentation_time_offset = presentationTimeOffset;

            /*Old code version was printing mpd_seg_info here, what is it ?*/
		}
		/*in BS switching we share the same IS for all reps, write the SegmentTemplate for the init segment*/
		else if ((is_bs_switching || seg_tl) && first_in_set && !dasher->segment_alignment_disabled) {
			GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
			seg_template->start_number=(u32)-1;
			adaptation_set_obj->segment_template = seg_template;
			if (is_bs_switching) {
				seg_template->initialization = gf_strdup(bs_switching_segment_name);
				if (presentationTimeOffset) {
					seg_template->presentation_time_offset = presentationTimeOffset;
				}
				seg_template->start_number=(u32)-1;
			}
			if (dasher->ast_offset_ms<0) {
				seg_template->availability_time_offset = -(Double)dasher->ast_offset_ms / 1000.0;
			}

			if (seg_tl) {
				seg_template->timescale = mpd_timescale;
				seg_template->segment_timeline = seg_tl;
				seg_tl = NULL;
			} 
		}
	}
	else if ((dasher->single_file_mode!=1) && seg_tl) {
		GF_MPD_SegmentList *seg_list;
		GF_SAFEALLOC(seg_list, GF_MPD_SegmentList);
		seg_list->start_number=(u32)-1;
		adaptation_set_obj->segment_list = seg_list;
		seg_list->timescale = dasher->dash_scale;
		if (dasher->ast_offset_ms<0) {
			seg_list->availability_time_offset = -(Double)dasher->ast_offset_ms / 1000.0;
		}
		seg_list->segment_timeline = seg_tl;
		seg_tl = NULL;
	}

	representation_obj = gf_mpd_representation_new();
	gf_list_add(adaptation_set_obj->representations, representation_obj);
	if (strlen(dash_input->representationID)) {
		representation_obj->id = gf_strdup(dash_input->representationID);
	}
	else {
		char szId[256];
		sprintf(szId, "%p", output);
		representation_obj->id = gf_strdup(szId);
	}
	{
		char mime[256];
		sprintf(mime, "%s/mp4", (!nb_audio && !nb_video) ? "application" : (!nb_video ? "audio" : "video"));
		representation_obj->mime_type = gf_strdup(mime);
		representation_obj->codecs = gf_strdup(szCodecs);
	}
	if (width && height) {
		representation_obj->width = width;
		representation_obj->height = height;

		/*this is a video track*/
		if (fps_num || fps_denum) {
			gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);
			GF_SAFEALLOC(representation_obj->framerate, GF_MPD_Fractional);
			representation_obj->framerate->num = fps_num;
			representation_obj->framerate->den = fps_denum;

			if (!sar_w) sar_w = 1;
			if (!sar_h) sar_h = 1;
			GF_SAFEALLOC(representation_obj->sar, GF_MPD_Fractional);
			representation_obj->sar->num = sar_w;
			representation_obj->sar->den = sar_h;
		}
	}
	if (sample_rate) {
		representation_obj->samplerate = sample_rate;
	}

	//single segment (onDemand profiles, assumes we always start with an IDR)
	if (dasher->single_file_mode==1) {
		representation_obj->starts_with_sap = 1;
	}
	//regular segmenting
	else {
		if (segments_start_with_sap || split_seg_at_rap) {
			representation_obj->starts_with_sap = max_sap_type;
		} else {
			representation_obj->starts_with_sap = 0;
		}
	}

	//only appears at AdaptationSet level - need to rewrite the DASH segementer to allow writing this at the proper place
//		if ((single_file_mode==1) && segments_start_with_sap) fprintf(dash_cfg->mpd_file, " subsegmentStartsWithSAP=\"%d\"", max_sap_type);

	representation_obj->bandwidth = bandwidth;
	if (dash_input->dependencyID) {
		representation_obj->dependency_id = gf_strdup(dash_input->dependencyID);
	}

	/* baseURLs */
	if (dash_input->nb_baseURL) {
		for (i=0; i<dash_input->nb_baseURL; i++) {
			GF_MPD_BaseURL *base_url;
			GF_SAFEALLOC(base_url, GF_MPD_BaseURL);
			base_url->URL = gf_strdup(dash_input->baseURL[i]);
			gf_list_add(representation_obj->base_URLs, base_url);
		}
	}

	/* writing Representation level descriptors */
	//if (dash_input->nb_rep_descs) {
	//	for (i=0; i<dash_input->nb_rep_descs; i++) {
	//		fprintf(dash_cfg->mpd_file, "    %s\n", dash_input->rep_descs[i]);
	//	}
	//}

	if (dash_input->nb_rep_descs) {
		for (i=0; i<dash_input->nb_rep_descs; i++) {			
			if (strchr(dash_input->rep_descs[i], '<') != NULL) {
				GF_MPD_other_descriptors *Desc;
				GF_SAFEALLOC(Desc,GF_MPD_other_descriptors);
				Desc->xml_desc=gf_strdup(dash_input->rep_descs[i]);
				gf_list_add(representation_obj->other_descriptors,Desc);								
			}			
		}
	}

	if (nb_channels && !is_bs_switching) {
		GF_MPD_Descriptor *desc;
		char value[256];
		sprintf(value, "%d", (!nb_surround && !nb_lfe)?nb_channels:cicp_get_channel_config(nb_channels, nb_surround, nb_lfe));
		if (!nb_surround && !nb_lfe) {
			desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:dash:23003:3:audio_channel_configuration:2011", value);
		}
		else{
			desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:mpegB:cicp:ChannelConfiguration", value);
		}
		gf_list_add(representation_obj->audio_channels, desc);
	}

	/* Write content protection element in representation */
	if (protected_track && (dasher->cp_location_mode != GF_DASH_CPMODE_ADAPTATION_SET)) {
		GF_MPD_Descriptor *desc = gf_isom_get_content_protection_desc(input, protected_track);
		gf_list_add(representation_obj->content_protection, desc);
	}

	if (use_url_template) {
		/*segment template depends on file name, but the template at the representation level*/
		if (dasher->variable_seg_rad_name) {
			const char *rad_name = gf_dasher_strip_output_dir(dasher->mpd_name, seg_rad_name);
#ifdef GENERATE_VIRTUAL_REP_SRD
			//if virtual rep, SegmentName already contains the name of the empty media segment used
			if (!dash_input->virtual_representation)
#endif
			{
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, NULL, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, bandwidth, 0, dasher->use_segment_timeline);
			}
			GF_MPD_SegmentTemplate *seg_template;
			GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
			seg_template->start_number=(u32)-1;
			representation_obj->segment_template = seg_template;
			seg_template->timescale = mpd_timescale;
			seg_template->media = gf_strdup(SegmentName);
			seg_template->start_number = startNumber;
			if (!dasher->use_segment_timeline) {
				seg_template->duration = (u32)(max_segment_duration * mpd_timescale + 0.5);
			}
			//don't write init if scalable rep
			if (!is_bs_switching && !dash_input->idx_representations) {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, NULL, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0, dasher->use_segment_timeline);
				seg_template->initialization = gf_strdup(SegmentName);
			}
			if (presentationTimeOffset) {
				seg_template->presentation_time_offset = presentationTimeOffset;
			}
			if (dasher->ast_offset_ms<0) {
				seg_template->availability_time_offset = -(Double)dasher->ast_offset_ms / 1000.0;
			}

			if (seg_tl && (!first_in_set || dasher->segment_alignment_disabled) ) {
				seg_template->segment_timeline = seg_tl;
			} 
		}
	}
	else if (dasher->single_file_mode==1) {
		GF_MPD_BaseURL *baseurl;
		GF_SAFEALLOC(baseurl, GF_MPD_BaseURL);
		if (!representation_obj->base_URLs) {
			representation_obj->base_URLs = gf_list_new();
		}
		gf_list_add(representation_obj->base_URLs, baseurl);
#ifdef GENERATE_VIRTUAL_REP_SRD
		if (dash_input->virtual_representation) {
			baseurl->URL = gf_strdup(SegmentName);
		} else
#endif
		{
			baseurl->URL = gf_strdup(gf_dasher_strip_output_dir(dasher->mpd_name, gf_isom_get_filename(output)));
			{
				GF_MPD_SegmentBase *segmentbase;
				GF_SAFEALLOC(segmentbase, GF_MPD_SegmentBase);
				representation_obj->segment_base = segmentbase;
				GF_SAFEALLOC(segmentbase->index_range, GF_MPD_ByteRange);
				segmentbase->index_range_exact=GF_TRUE;
				segmentbase->index_range->start_range = index_start_range;
				segmentbase->index_range->end_range = index_end_range;
				segmentbase->presentation_time_offset = presentationTimeOffset;
				if (!is_bs_switching) {
					GF_MPD_URL *URL;
					GF_SAFEALLOC(URL, GF_MPD_URL);
					segmentbase->initialization_segment=URL;
					GF_MPD_ByteRange *ByteRange;
					GF_SAFEALLOC(ByteRange, GF_MPD_ByteRange);
					URL->byte_range=ByteRange;
					ByteRange->start_range = 0;
					ByteRange->end_range = index_start_range - 1;
				}
			}
		}
	} 
	else {
		if (!seg_rad_name) {
			GF_MPD_BaseURL *baseurl;
			GF_SAFEALLOC(baseurl, GF_MPD_BaseURL);
			if (!representation_obj->base_URLs) {
				representation_obj->base_URLs = gf_list_new();
			}
			gf_list_add(representation_obj->base_URLs, baseurl);
			baseurl->URL = gf_strdup(gf_dasher_strip_output_dir(dasher->mpd_name, gf_isom_get_filename(output)));
		}
		GF_MPD_SegmentList *seg_list;
		GF_SAFEALLOC(seg_list, GF_MPD_SegmentList);
		representation_obj->segment_list = seg_list;
		seg_list->start_number=(u32)-1;
		if (!seg_tl) {
			seg_list->timescale = mpd_timescale;
			seg_list->duration = (u32)(max_segment_duration * mpd_timescale + 0.5);
			seg_list->segment_URLs = segment_urls;
			segment_urls = NULL;
		}
		if (presentationTimeOffset) {
			seg_list->presentation_time_offset = presentationTimeOffset;
		}
		if (dasher->ast_offset_ms<0) {
			seg_list->availability_time_offset = -(Double)dasher->ast_offset_ms / 1000.0;
		}
		/*we are not in bitstreamSwitching mode*/
		if (!is_bs_switching) {
			GF_MPD_URL *initialization_segment;
			GF_SAFEALLOC(initialization_segment, GF_MPD_URL);
			seg_list->initialization_segment = initialization_segment;
			if (!seg_rad_name) {
				GF_SAFEALLOC(initialization_segment->byte_range, GF_MPD_ByteRange);
				initialization_segment->byte_range->end_range = init_seg_size - 1;
			} else {
#ifdef GENERATE_VIRTUAL_REP_SRD
				//if virtual rep, SegmentName already contains the name of the empty media segment used
				if (!dash_input->virtual_representation)
#endif
				{
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, dash_input->representationID, NULL, gf_dasher_strip_output_dir(dasher->mpd_name,  seg_rad_name) , !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0, dasher->use_segment_timeline);
				}
				initialization_segment->sourceURL = gf_strdup(SegmentName);
			}
		}
	}

	/*store context*/
	if (dasher->dash_ctx) {
		period_duration += (u64)segment_start_time; //change to get a Double period duration

		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);

			/*InitialTSOffset is used when joining different files - if we are still in the same file , do not update it*/
			if (tf->done) {
				sprintf(sKey, "TKID_%d_NextDecodingTime", tf->TrackID);
				sprintf(sOpt, LLU, tf->InitialTSOffset + tf->next_sample_dts);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, sOpt);
			}

			if (dasher->subduration) {
				sprintf(sKey, "TKID_%d_NextSampleNum", tf->TrackID);
				sprintf(sOpt, "%d", tf->SampleNum);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, tf->done ? NULL : sOpt);

				sprintf(sKey, "TKID_%d_LastSampleCTS", tf->TrackID);
				sprintf(sOpt, LLU, tf->last_sample_cts);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, tf->done ? NULL : sOpt);

				sprintf(sKey, "TKID_%d_NextSampleDTS", tf->TrackID);
				sprintf(sOpt, LLU, tf->next_sample_dts);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, tf->done ? NULL : sOpt);

				sprintf(sKey, "TKID_%d_MediaTimeToPresTime", tf->TrackID);
				sprintf(sOpt, "%d", tf->media_time_to_pres_time_shift);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, sOpt);

				sprintf(sKey, "TKID_%d_LoopTSOffset", tf->TrackID);
				sprintf(sOpt, LLU, tf->loop_ts_offset);
				gf_cfg_set_key(dasher->dash_ctx, RepSecName, sKey, (tf->done && dasher->disable_loop) ? NULL : sOpt);

			}
		}
		sprintf(sOpt, "%d", cur_seg);
		gf_cfg_set_key(dasher->dash_ctx, RepSecName, "NextSegmentIndex", sOpt);

		fragment_index = gf_isom_get_next_moof_number(output);
		sprintf(sOpt, "%d", fragment_index);
		gf_cfg_set_key(dasher->dash_ctx, RepSecName, "NextFragmentIndex", sOpt);


		sprintf(sOpt, LLU, period_duration);
		gf_cfg_set_key(dasher->dash_ctx, RepSecName, "CumulatedDuration", sOpt);

		if (store_dash_params) {
			sprintf(sOpt, "%u", bandwidth);
			gf_cfg_set_key(dasher->dash_ctx, RepSecName, "Bandwidth", sOpt);
		}
		//done with this file
		if ((nb_tracks_done==count) && dasher->disable_loop) {
			dasher->force_period_end = GF_TRUE;
		}

	}

err_exit:
	if (langCode) {
		gf_free(langCode);
	}
	if (fragmenters) {
		while (gf_list_count(fragmenters)) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, 0);
			gf_free(tf);
			gf_list_rem(fragmenters, 0);
		}
		gf_list_del(fragmenters);
	}
	if (segment_urls) {
		gf_mpd_segment_url_list_free(segment_urls);
	}

	if (output) {
		if (e) gf_isom_delete(output);
		else gf_isom_close(output);
	}
	if (!bs_switching_is_output && bs_switch_segment)
		gf_isom_delete(bs_switch_segment);
	gf_set_progress("ISO File Fragmenting", nb_samp, nb_samp);
	return e;
}


static GF_Err dasher_isom_get_components_info(GF_DashSegInput *input, GF_DASHSegmenter *opts)
{
	u32 i;
	GF_ISOFile *in;
	Double dur;
	GF_Err e;

	in = gf_isom_open(input->file_name, GF_ISOM_OPEN_READ, NULL);
	input->duration = 0;
	for (i=0; i<gf_isom_get_track_count(in); i++) {
		u32 mtype = gf_isom_get_media_type(in, i+1);

		if (input->trackNum && (input->trackNum != i+1))
			continue;

		if (input->single_track_num && (input->single_track_num != i+1))
			continue;

		if (mtype == GF_ISOM_MEDIA_HINT)
			continue;

		//we allow single sample tracks to be dashed, might want to disable BIFS and OD ...

		dur = 0;
		if (gf_isom_get_edit_segment_count(in, i+1)) {
			dur = (Double) gf_isom_get_track_duration(in, i+1);
			dur /= gf_isom_get_timescale(in);
		}
		if (!dur) {
			dur = (Double) gf_isom_get_media_duration(in, i+1);
			dur /= gf_isom_get_media_timescale(in, i+1);
		}
		if (dur > input->duration) input->duration = dur;

		input->components[input->nb_components].duration = dur;

		input->components[input->nb_components].ID = gf_isom_get_track_id(in, i+1);
		input->components[input->nb_components].media_type = mtype;
		gf_isom_get_media_language(in, i+1, &input->components[input->nb_components].lang);

		if (mtype == GF_ISOM_MEDIA_VISUAL) {
			gf_isom_get_visual_info(in, i+1, 1, &input->components[input->nb_components].width, &input->components[input->nb_components].height);

			input->components[input->nb_components].fps_num = gf_isom_get_media_timescale(in, i+1);
			/*get duration of track or of 2nd sample otherwise*/
			if (gf_isom_get_track_duration(in, i + 1)) {
				input->components[input->nb_components].fps_denum = (u32)(gf_isom_get_media_duration(in, i+1) / gf_isom_get_sample_count(in, i+1) );
			} else {
				input->components[input->nb_components].fps_denum = gf_isom_get_sample_duration(in, i+1, 2);
			}
			gf_isom_get_pixel_aspect_ratio(in, i+1, 1, &input->components[input->nb_components].sar_num, &input->components[input->nb_components].sar_denum);
		}
		/*non-video tracks, get lang*/
		else if (mtype == GF_ISOM_MEDIA_AUDIO) {
			u8 bps;
			e = isom_get_audio_info_with_m4a_sbr_ps(in, i+1, 1, &input->components[input->nb_components].sample_rate, &input->components[input->nb_components].channels, &bps);
			if (e) {
				if (opts->profile == GF_DASH_PROFILE_AVC264_LIVE || opts->profile == GF_DASH_PROFILE_AVC264_ONDEMAND) {
					//DASH-IF IOP 3.3 mandates the SBR/PS info
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("DASH input: could not get audio info, %s\n", gf_error_to_string(e)));
					goto error_exit;
				}
			}
		}

		input->nb_components++;
	}
	return gf_isom_close(in);

error_exit:
	gf_isom_close(in);
	return e;
}

static Bool dasher_inputs_have_same_roles(GF_DashSegInput *d1, GF_DashSegInput *d2)
{
	if (d1->roles && d2->roles) {
		if (d1->nb_roles != d2->nb_roles)
			return GF_FALSE;
		else {
			u32 r1, r2;
			for (r1=0; r1<d1->nb_roles; r1++) {
				Bool found = GF_FALSE;
				for (r2=0; r2<d2->nb_roles; r2++) {
					if (!strcmp(d1->roles[r1], d2->roles[r2])) {
						found = GF_TRUE;
						break;
					}
				}
				if (!found) return GF_FALSE;
			}
			return GF_TRUE;
		}
	}
	if (!d1->roles && !d2->roles)
		return GF_TRUE;

	return GF_FALSE;
}

static GF_Err dasher_isom_classify_input(GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 input_idx, u32 *current_group_id, u32 *max_sap_type)
{
	u32 i, j;
	GF_ISOFile *set_file;

	if (input_idx+1>=nb_dash_inputs)
		return GF_OK;

	set_file = gf_isom_open(dash_inputs[input_idx].file_name, GF_ISOM_OPEN_READ, NULL);

	for (i=input_idx+1; i<nb_dash_inputs; i++) {
		Bool has_same_desc = GF_TRUE;
		Bool valid_in_adaptation_set = GF_TRUE;
		Bool assign_to_group = GF_TRUE;
		GF_ISOFile *in;

		if (dash_inputs[input_idx].period != dash_inputs[i].period)
			continue;

		if (strcmp(dash_inputs[input_idx].szMime, dash_inputs[i].szMime))
			continue;

		if (! dasher_inputs_have_same_roles(&dash_inputs[input_idx], &dash_inputs[i]) ) {
			continue;
		}

		/* if two inputs don't have the same (number and value) as_desc they don't belong to the same AdaptationSet
		   (use c_as_desc for AdaptationSet descriptors common to all inputs in an AS) */
		if (dash_inputs[input_idx].nb_as_descs != dash_inputs[i].nb_as_descs)
			continue;
		for (j = 0; j < dash_inputs[input_idx].nb_as_descs; j++) {
			if (strcmp(dash_inputs[input_idx].as_descs[j], dash_inputs[i].as_descs[j])) {
				has_same_desc= GF_FALSE;
				break;
			}
		}
		if (!has_same_desc) continue;

		if (dash_inputs[input_idx].x != dash_inputs[i].x) continue;
		if (dash_inputs[input_idx].y != dash_inputs[i].y) continue;
		if (dash_inputs[input_idx].w != dash_inputs[i].w) continue;
		if (dash_inputs[input_idx].h != dash_inputs[i].h) continue;
#ifdef GENERATE_VIRTUAL_REP_SRD
		if (dash_inputs[i].virtual_representation) continue;
#endif

		in = gf_isom_open(dash_inputs[i].file_name, GF_ISOM_OPEN_READ, NULL);

		for (j=0; j<gf_isom_get_track_count(set_file); j++) {
			u32 mtype, msub_type;
			Bool same_codec = GF_TRUE;
			u32 track = gf_isom_get_track_by_id(in, gf_isom_get_track_id(set_file, j+1));

			if (dash_inputs[input_idx].single_track_num && ((j + 1) != dash_inputs[input_idx].single_track_num))
				continue;

			if (!track) {
				valid_in_adaptation_set = GF_FALSE;
				assign_to_group = GF_FALSE;
				break;
			}

			if (dash_inputs[input_idx].single_track_num && (dash_inputs[input_idx].single_track_num != j+1))
				continue;

			if (dash_inputs[i].single_track_num) {
				track = dash_inputs[i].single_track_num;
			}

			mtype = gf_isom_get_media_type(set_file, j+1);
			if (mtype != gf_isom_get_media_type(in, track)) {
				valid_in_adaptation_set = GF_FALSE;
				assign_to_group = GF_FALSE;
				break;
			}
			msub_type = gf_isom_get_media_subtype(set_file, j+1, 1);
			if (msub_type != gf_isom_get_media_subtype(in, track, 1)) same_codec = GF_FALSE;
			if ((msub_type==GF_ISOM_SUBTYPE_MPEG4)
			        || (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP)
			        || (msub_type==GF_ISOM_SUBTYPE_AVC_H264)
			        || (msub_type==GF_ISOM_SUBTYPE_AVC2_H264)
			        || (msub_type==GF_ISOM_SUBTYPE_AVC3_H264)
			        || (msub_type==GF_ISOM_SUBTYPE_AVC4_H264)
			        || (msub_type==GF_ISOM_SUBTYPE_SVC_H264)
			        || (msub_type==GF_ISOM_SUBTYPE_MVC_H264)
			        || (msub_type==GF_ISOM_SUBTYPE_HVC1)
			        || (msub_type==GF_ISOM_SUBTYPE_HEV1)
			        || (msub_type==GF_ISOM_SUBTYPE_HVC2)
			        || (msub_type==GF_ISOM_SUBTYPE_HEV2)
			        || (msub_type==GF_ISOM_SUBTYPE_LSR1)
			   ) {
				GF_DecoderConfig *dcd1 = gf_isom_get_decoder_config(set_file, j+1, 1);
				GF_DecoderConfig *dcd2 = gf_isom_get_decoder_config(in, track, 1);
				if (dcd1 && dcd2 && (dcd1->streamType==dcd2->streamType) && (dcd1->objectTypeIndication==dcd2->objectTypeIndication)) {
					same_codec = GF_TRUE;
				} else {
					same_codec = GF_FALSE;
				}
				if (dcd1) gf_odf_desc_del((GF_Descriptor *)dcd1);
				if (dcd2) gf_odf_desc_del((GF_Descriptor *)dcd2);
			}

			if (!same_codec) {
				valid_in_adaptation_set = GF_FALSE;
				break;
			}

			if ((mtype!=GF_ISOM_MEDIA_VISUAL) && (mtype!=GF_ISOM_MEDIA_HINT)) {
				char *lang1, *lang2;
				lang1 = lang2 = NULL;
				gf_isom_get_media_language(set_file, j+1, &lang1);
				gf_isom_get_media_language(in, track, &lang2);
				if (lang1 && lang2 && stricmp(lang1, lang2)) {
					valid_in_adaptation_set = GF_FALSE;
					gf_free(lang1);
					gf_free(lang2);
					break;
				}
				if (lang1) gf_free(lang1);
				if (lang2) gf_free(lang2);
			}
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
				u32 w1, h1, w2, h2, sap_type;
				Bool rap, roll;
				s32 roll_dist;

				gf_isom_get_track_layout_info(set_file, j+1, &w1, &h1, NULL, NULL, NULL);
				gf_isom_get_track_layout_info(in, track, &w2, &h2, NULL, NULL, NULL);

				if (h1*w2 != h2*w1) {
					u32 hs1, hs2, vs1, vs2, vw1, vh1, vw2, vh2;
					u64 ar1, ar2;

					//check same sample aspect ratio
					gf_isom_get_pixel_aspect_ratio(set_file, j+1, 1, &hs1, &vs1);
					gf_isom_get_pixel_aspect_ratio(in, track, 1, &hs2, &vs2);

					//check same image aspect ratio
					gf_isom_get_visual_info(set_file, j+1, 1, &vw1, &vh1);
					gf_isom_get_visual_info(in, track, 1, &vw2, &vh2);

					ar1 = vh1*vw2;
					ar1*= hs2*vs1;

					ar2 = vh2*vw1;
					ar2 *= hs1*vs2;
					if (ar1 != ar2) {
						gf_isom_get_track_layout_info(in, track, &w2, &h2, NULL, NULL, NULL);
						valid_in_adaptation_set = GF_FALSE;
						break;
					} else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Files have non-proportional track layouts (%dx%d vs %dx%d) but sample size and aspect ratio match, assuming precision issue\n", w1, h1, w2, h2));
					}
				}

				gf_isom_get_sample_rap_roll_info(in, track, 0, &rap, &roll, &roll_dist);
				if (roll_dist>0) sap_type = 4;
				else if (roll) sap_type = 3;
				else if (rap) sap_type = 1;
				else sap_type = 1;
				if (*max_sap_type < sap_type) *max_sap_type = sap_type;
			}
		}
		gf_isom_close(in);
		if (valid_in_adaptation_set) {
			dash_inputs[i].adaptation_set = dash_inputs[input_idx].adaptation_set;
			dash_inputs[input_idx].nb_rep_in_adaptation_set ++;
		} else if (assign_to_group) {
			if (!dash_inputs[input_idx].group_id) {
				(*current_group_id) ++;
				dash_inputs[input_idx].group_id = (*current_group_id);
			}
			dash_inputs[i].group_id = (*current_group_id);
		}
	}
	gf_isom_close(set_file);
	return GF_OK;
}

static GF_Err dasher_isom_create_init_segment(GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 adaptation_set, char *szInitName, const char *tmpdir, GF_DASHSegmenter *dash_opts, GF_DashSwitchingMode bs_switch_mode, Bool *disable_bs_switching)
{
	GF_Err e = GF_OK;
	u32 i;
	u32 single_track_id = 0;
	Bool sps_merge_failed = GF_FALSE;
	Bool use_avc3 = GF_FALSE;
	Bool use_hevc = GF_FALSE;
	Bool use_inband_param_set;
	u32 probe_inband_param_set;
	u32 first_track = 0;
	Bool single_segment = (dash_opts->single_file_mode==1) ? GF_TRUE : GF_FALSE;
	GF_ISOFile *init_seg;


	probe_inband_param_set = dash_opts->inband_param_set ? 1 : 0;

restart_init:

	init_seg = gf_isom_open(szInitName, GF_ISOM_OPEN_WRITE, tmpdir);
	if (dash_opts->force_test_mode) {
		gf_isom_no_version_date_info(init_seg, 1);
	}
	for (i=0; i<nb_dash_inputs; i++) {
		u32 j;
		GF_ISOFile *in;

		if (dash_inputs[i].adaptation_set != adaptation_set)
			continue;

		//no inband param set for scalable files
		use_inband_param_set = dash_inputs[i].trackNum ? GF_FALSE : dash_opts->inband_param_set;

		in = gf_isom_open(dash_inputs[i].file_name, GF_ISOM_OPEN_READ, NULL);
		if (!in) {
			e = gf_isom_last_error(NULL);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error while opening %s: %s\n", dash_inputs[i].file_name, gf_error_to_string(e)));
			return e;
		}

		if (bs_switch_mode == GF_DASH_BSMODE_MULTIPLE_ENTRIES) {
			if (gf_isom_get_track_count(in)>1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot use multi-stsd mode on files with multiple tracks\n"));
				return GF_NOT_SUPPORTED;
			}
		}


		for (j=0; j<gf_isom_get_track_count(in); j++) {
			u32 track;

			if (dash_inputs[i].single_track_num) {
				if (dash_inputs[i].single_track_num != j+1)
					continue;

				if (!single_track_id)
					single_track_id = gf_isom_get_track_id(in, j+1);
				//if not the same ID between the two tracks we cannot create a legal file with bitstream switching enabled
				else if (single_track_id != gf_isom_get_track_id(in, j+1)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Track IDs different between representations, disabling bitstream switching\n"));
					*disable_bs_switching = GF_TRUE;
					gf_isom_delete(init_seg);
					gf_delete_file(szInitName);
					return GF_OK;
				}
			}

			track = gf_isom_get_track_by_id(init_seg, gf_isom_get_track_id(in, j+1));

			if (track) {
				u32 outDescIndex;
				if ( gf_isom_get_sample_description_count(in, j+1) != 1) {
					e = GF_NOT_SUPPORTED;
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot merge track with multiple sample descriptions (file %s) - try disabling bitstream switching\n", dash_inputs[i].file_name ));
					gf_isom_delete(init_seg);
					gf_isom_delete(in);
					return e;
				}
retry_track:

				if (bs_switch_mode == GF_DASH_BSMODE_MULTIPLE_ENTRIES) {
					gf_isom_clone_sample_description(init_seg, track, in, j+1, 1, NULL, NULL, &outDescIndex);
				}

				/*if not the same sample desc we might need to clone it*/
				else if (! gf_isom_is_same_sample_description(in, j+1, 1, init_seg, track, 1)) {
					u32 merge_mode = 1;
					u32 stype1, stype2;
					stype1 = gf_isom_get_media_subtype(in, j+1, 1);
					stype2 = gf_isom_get_media_subtype(init_seg, track, 1);
					if (stype1 != stype2) merge_mode = 0;
					switch (stype1) {
					case GF_ISOM_SUBTYPE_AVC_H264:
					case GF_ISOM_SUBTYPE_AVC2_H264:
					case GF_ISOM_SUBTYPE_MPEG4_CRYP:
						if (use_avc3)
							merge_mode = 2;
						break;
					case GF_ISOM_SUBTYPE_SVC_H264:
					case GF_ISOM_SUBTYPE_MVC_H264:
						break;
					case GF_ISOM_SUBTYPE_AVC3_H264:
					case GF_ISOM_SUBTYPE_AVC4_H264:
						/*we don't want to clone SPS/PPS since they are already inside the samples*/
						merge_mode = 2;
						break;
					case GF_ISOM_SUBTYPE_HVC1:
					case GF_ISOM_SUBTYPE_HVC2:
						if (use_hevc)
							merge_mode = 2;
						break;
					case GF_ISOM_SUBTYPE_HEV1:
					case GF_ISOM_SUBTYPE_HEV2:
						/*we don't want to clone SPS/PPS since they are already inside the samples*/
						merge_mode = 2;
						break;
					case GF_ISOM_SUBTYPE_LHV1:
						break;
					case GF_ISOM_SUBTYPE_LHE1:
						merge_mode = 2;
						break;
					default:
						merge_mode = 0;
						break;
					}
					if (merge_mode==1) {
#ifndef GPAC_DISABLE_AV_PARSERS
						u32 k, l, sps_id1, sps_id2;
#endif
						GF_AVCConfig *avccfg1 = gf_isom_avc_config_get(in, j+1, 1);
						GF_AVCConfig *avccfg2 = gf_isom_avc_config_get(init_seg, track, 1);
#ifndef GPAC_DISABLE_AV_PARSERS
						for (k=0; k<gf_list_count(avccfg2->sequenceParameterSets); k++) {
							GF_AVCConfigSlot *slc = (GF_AVCConfigSlot *)gf_list_get(avccfg2->sequenceParameterSets, k);
							gf_avc_get_sps_info(slc->data, slc->size, &sps_id1, NULL, NULL, NULL, NULL);
							for (l=0; l<gf_list_count(avccfg1->sequenceParameterSets); l++) {
								GF_AVCConfigSlot *slc_orig = (GF_AVCConfigSlot *)gf_list_get(avccfg1->sequenceParameterSets, l);
								gf_avc_get_sps_info(slc_orig->data, slc_orig->size, &sps_id2, NULL, NULL, NULL, NULL);
								if (sps_id2==sps_id1) {
									merge_mode = 0;
									break;
								}
							}
						}
#endif
						/*no conflicts in SPS ids, merge all SPS in a single sample desc*/
						if (merge_mode==1) {
							while (gf_list_count(avccfg1->sequenceParameterSets)) {
								GF_AVCConfigSlot *slc = (GF_AVCConfigSlot *)gf_list_get(avccfg1->sequenceParameterSets, 0);
								gf_list_rem(avccfg1->sequenceParameterSets, 0);
								gf_list_add(avccfg2->sequenceParameterSets, slc);
							}
							while (gf_list_count(avccfg1->pictureParameterSets)) {
								GF_AVCConfigSlot *slc = (GF_AVCConfigSlot *)gf_list_get(avccfg1->pictureParameterSets, 0);
								gf_list_rem(avccfg1->pictureParameterSets, 0);
								gf_list_add(avccfg2->pictureParameterSets, slc);
							}
							gf_isom_avc_config_update(init_seg, track, 1, avccfg2);
						} else {
							sps_merge_failed = GF_TRUE;
						}
						gf_odf_avc_cfg_del(avccfg1);
						gf_odf_avc_cfg_del(avccfg2);
					}

					//check if we can get rid of inband params
					if (!merge_mode && use_inband_param_set && probe_inband_param_set) {
						probe_inband_param_set = 0;

						switch (gf_isom_get_media_subtype(init_seg, first_track, 1)) {
						case GF_ISOM_SUBTYPE_HVC1:
						case GF_ISOM_SUBTYPE_HVC2:
							gf_isom_hevc_set_inband_config(init_seg, track, 1);
							use_hevc = GF_TRUE;
							gf_isom_set_brand_info(init_seg, GF_ISOM_BRAND_ISO6, 1);
							sps_merge_failed = GF_FALSE;
							break;
						case GF_ISOM_SUBTYPE_AVC_H264:
						case GF_ISOM_SUBTYPE_AVC2_H264:
						case GF_ISOM_SUBTYPE_SVC_H264:
						case GF_ISOM_SUBTYPE_MVC_H264:
							gf_isom_avc_set_inband_config(init_seg, track, 1);
							use_avc3 = GF_TRUE;
							gf_isom_set_brand_info(init_seg, GF_ISOM_BRAND_ISO6, 1);
							sps_merge_failed = GF_FALSE;
							break;
						}
						goto retry_track;
					}


					/*cannot merge, clone*/
					if (merge_mode==0) {

						if (use_inband_param_set && (use_avc3 || use_hevc) ) {
							GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Couldn't merge %s parameter sets - using in-band config\n", use_hevc ? "HEVC" : "AVC" ));
							gf_isom_delete(init_seg);
							goto restart_init;
						}
						gf_isom_clone_sample_description(init_seg, track, in, j+1, 1, NULL, NULL, &outDescIndex);
					}
				}
			} else {
				u32 sample_duration, defaultSize, defaultDescriptionIndex, defaultRandomAccess;
				u8 defaultPadding;
				u16 defaultDegradationPriority;

				gf_isom_clone_track(in, j+1, init_seg, GF_FALSE, &track);

				switch (gf_isom_get_media_subtype(in, j+1, 1)) {
				case GF_ISOM_SUBTYPE_AVC_H264:
				case GF_ISOM_SUBTYPE_AVC2_H264:
				case GF_ISOM_SUBTYPE_SVC_H264:
				case GF_ISOM_SUBTYPE_MVC_H264:
					if (!probe_inband_param_set) {
						if (use_inband_param_set) {
							gf_isom_avc_set_inband_config(init_seg, track, 1);
							use_avc3 = GF_TRUE;
						}
					} else {
						first_track = track;
					}
					break;
				case GF_ISOM_SUBTYPE_HVC1:
				case GF_ISOM_SUBTYPE_HVC2:
					//todo enble comparison of hevc xPS
					probe_inband_param_set=0;

					if (use_inband_param_set) {
						gf_isom_hevc_set_inband_config(init_seg, track, 1);
					}
					use_hevc = GF_TRUE;
					break;

				case GF_ISOM_SUBTYPE_AVC3_H264:
				case GF_ISOM_SUBTYPE_AVC4_H264:
				case GF_ISOM_SUBTYPE_HEV1:
				case GF_ISOM_SUBTYPE_HEV2:
					probe_inband_param_set = 0;
					break;
				}
				if (gf_isom_has_sync_points(in, j+1))
					gf_isom_set_sync_table(init_seg, track);


				gf_isom_get_fragment_defaults(in, j+1, &sample_duration, &defaultSize,
				                              &defaultDescriptionIndex, &defaultRandomAccess,
				                              &defaultPadding, &defaultDegradationPriority);
				//setup for fragmentation
				e = gf_isom_setup_track_fragment(init_seg, gf_isom_get_track_id(init_seg, track),
				                                 defaultDescriptionIndex, sample_duration,
				                                 defaultSize, (u8) defaultRandomAccess,
				                                 defaultPadding, defaultDegradationPriority);
				if (e) break;
			}
			if (!e) {
				u32 k, edit_count=gf_isom_get_edit_segment_count(in, j+1);

				/*remove all segments*/
				gf_isom_remove_edit_segments(init_seg, track);
				//clone all edits before first nomal, and stop at first normal setting duration to 0
				for (k=0; k<edit_count; k++) {
					u64 EditTime, SegDuration, MediaTime;
					u8 EditMode;

					/*get first edit*/
					gf_isom_get_edit_segment(in, j+1, k+1, &EditTime, &SegDuration, &MediaTime, &EditMode);

					if (EditMode==GF_ISOM_EDIT_NORMAL) {
						gf_isom_set_edit_segment(init_seg, track, EditTime, 0, MediaTime, EditMode);
						break;
					}

					gf_isom_set_edit_segment(init_seg, track, EditTime, SegDuration, MediaTime, EditMode);
				}
			}
		}
		if (!i) {
			if (use_hevc || use_avc3) {
				gf_isom_set_brand_info(init_seg, GF_ISOM_BRAND_ISO6, 1);
			} else {
				gf_isom_set_brand_info(init_seg, GF_ISOM_BRAND_ISO5, 1);
			}
			/*DASH self-init media segment*/
			if (single_segment) {
				gf_isom_modify_alternate_brand(init_seg, GF_ISOM_BRAND_DSMS, 1);
			} else {
				gf_isom_modify_alternate_brand(init_seg, GF_ISOM_BRAND_DASH, 1);
			}

			if (!dash_opts->pssh_moof) {
				e = gf_isom_clone_pssh(init_seg, in, GF_FALSE);
			}
		}
		gf_isom_close(in);
	}
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Couldn't create initialization segment: error %s\n", gf_error_to_string(e) ));
		*disable_bs_switching = GF_TRUE;
		gf_isom_delete(init_seg);
		gf_delete_file(szInitName);
		return GF_OK;
	} else if (sps_merge_failed) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Could not merge AVC|H264 SPS from different files (same SPS ID used) - disabling bitstream switching\n"));
		*disable_bs_switching = GF_TRUE;
		gf_isom_delete(init_seg);
		gf_delete_file(szInitName);
		return GF_OK;
	} else {
		e = gf_isom_close(init_seg);

		if (probe_inband_param_set) {
			for (i=0; i<nb_dash_inputs; i++) {
				if (dash_inputs[i].adaptation_set != adaptation_set)
					continue;

				dash_inputs[i].disable_inband_param_set = GF_TRUE;
			}
		}
	}
	return e;
}

static GF_Err dasher_isom_segment_file(GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenter *dasher, GF_MPD_AdaptationSet *adaptation_set_obj, Bool first_in_set)
{
	GF_Err e = GF_OK;

	if (!dash_input->isobmf_input) {
		GF_ISOFile *in = gf_isom_open(dash_input->file_name, GF_ISOM_OPEN_READ, dasher->tmpdir);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] ISOBMFF opened\n"));

		if (!gf_isom_get_track_count(in)) {
			gf_isom_delete(in);
			return GF_BAD_PARAM;
		}

#if 0
		if (dash_input->media_duration) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Forcing media duration to %lfs.\n", dash_input->media_duration));
			e = dasher_isom_force_duration(in, dash_input->media_duration, dasher->fragment_duration);
			if (e) {
				gf_isom_delete(in);
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Media duration couldn't be forced. Aborting.\n"));
				return e;
			}
		}
#endif

		dash_input->isobmf_input = in;
	}

	e= gf_media_isom_segment_file(dash_input->isobmf_input, szOutName, dasher, dash_input, adaptation_set_obj, first_in_set);
	if(dash_input->no_cache){
		gf_isom_delete(dash_input->isobmf_input);
		dash_input->isobmf_input=NULL;
	}
	return e;
}

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

#ifndef GPAC_DISABLE_MPEG2TS

static GF_Err dasher_generic_classify_input(GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 input_idx, u32 *current_group_id, u32 *max_sap_type)
{
#ifdef GPAC_DISABLE_MEDIA_IMPORT
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	u32 i, j, k;
	GF_MediaImporter in, probe;

	memset(&in, 0, sizeof(GF_MediaImporter));
	in.flags = GF_IMPORT_PROBE_ONLY;
	in.in_name = (char *)dash_inputs[input_idx].file_name;
	e = gf_media_import(&in);
	if (e) return e;

	for (i=input_idx+1; i<nb_dash_inputs; i++) {
		Bool valid_in_adaptation_set = GF_TRUE;
		Bool assign_to_group = GF_TRUE;

		if (dash_inputs[input_idx].period != dash_inputs[i].period)
			continue;

		if (strcmp(dash_inputs[input_idx].szMime, dash_inputs[i].szMime))
			continue;

		if (! dasher_inputs_have_same_roles(&dash_inputs[input_idx], &dash_inputs[i]))
			continue;

		memset(&probe, 0, sizeof(GF_MediaImporter));
		probe.flags = GF_IMPORT_PROBE_ONLY;
		probe.in_name = (char *)dash_inputs[i].file_name;
		e = gf_media_import(&probe);
		if (e) return e;

		if (in.nb_progs != probe.nb_progs) valid_in_adaptation_set = GF_FALSE;
		if (in.nb_tracks != probe.nb_tracks) valid_in_adaptation_set = GF_FALSE;

		for (j=0; j<in.nb_tracks; j++) {
			struct __track_import_info *src_tk, *probe_tk;
			Bool found = GF_FALSE;
			/*make sure we have the same track*/
			for (k=0; k<probe.nb_tracks; k++) {
				if (in.tk_info[j].track_num==probe.tk_info[k].track_num) {
					found = GF_TRUE;
					break;
				}
			}
			if (!found) {
				valid_in_adaptation_set = GF_FALSE;
				assign_to_group = GF_FALSE;
				break;
			}
			src_tk = &in.tk_info[j];
			probe_tk = &probe.tk_info[k];

			/*make sure we use the same media type*/
			if (src_tk->type != probe_tk->type) {
				valid_in_adaptation_set = GF_FALSE;
				assign_to_group = GF_FALSE;
				break;
			}
			/*make sure we use the same codec type*/
			if (src_tk->media_type != probe_tk->media_type) {
				valid_in_adaptation_set = GF_FALSE;
				break;
			}
			/*make sure we use the same aspect ratio*/
			if (src_tk->type==GF_ISOM_MEDIA_VISUAL) {
				if (src_tk->video_info.width * probe_tk->video_info.height != src_tk->video_info.height * probe_tk->video_info.width) {
					valid_in_adaptation_set = GF_FALSE;
					break;
				}
			} else if (src_tk->lang != probe_tk->lang) {
				valid_in_adaptation_set = GF_FALSE;
				break;
			}

			if (!valid_in_adaptation_set) break;
		}
		if (valid_in_adaptation_set) {
			dash_inputs[i].adaptation_set = dash_inputs[input_idx].adaptation_set;
			dash_inputs[input_idx].nb_rep_in_adaptation_set ++;
		} else if (assign_to_group) {
			if (!dash_inputs[input_idx].group_id) {
				(*current_group_id) ++;
				dash_inputs[input_idx].group_id = (*current_group_id);
			}
			dash_inputs[i].group_id = (*current_group_id);
		}
	}
#endif
	return GF_OK;
}

static GF_Err dasher_generic_get_components_info(GF_DashSegInput *input, GF_DASHSegmenter *opts)
{
#ifdef GPAC_DISABLE_MEDIA_IMPORT
	return GF_NOT_SUPPORTED;
#else
	u32 i;
	GF_Err e;
	GF_MediaImporter in;
	memset(&in, 0, sizeof(GF_MediaImporter));
	in.flags = GF_IMPORT_PROBE_ONLY;
	in.in_name = (char *)input->file_name;
	e = gf_media_import(&in);
	if (e) return e;

	input->nb_components = in.nb_tracks;

	for (i=0; i<in.nb_tracks; i++) {
		input->components[i].width = in.tk_info[i].video_info.width;
		input->components[i].height = in.tk_info[i].video_info.height;
		input->components[i].media_type = in.tk_info[i].type;
		input->components[i].channels = in.tk_info[i].audio_info.nb_channels;
		input->components[i].sample_rate = in.tk_info[i].audio_info.sample_rate;
		input->components[i].ID = in.tk_info[i].track_num;
		if (in.tk_info[i].lang) {
			input->components[i].lang = gf_strdup( gf_4cc_to_str(in.tk_info[i].lang) );
		}
		strcpy(input->components[i].szCodec, in.tk_info[i].szCodecProfile);
		input->components[i].fps_denum = 1000;
		input->components[i].fps_num = (u32) (in.tk_info[i].video_info.FPS*1000);
	}
#endif
	return GF_OK;
}

typedef struct
{
	FILE *src;
	GF_M2TS_Demuxer *ts;
	u64 file_size;
	u32 bandwidth;
	Bool has_seen_pat;

	u64 suspend_indexing;

	/* For indexing the TS*/
	Double segment_duration;
	Bool segment_at_rap;

	FILE *index_file;
	GF_BitStream *index_bs;

	u32 subduration;
	u32 skip_nb_segments;
	u64 duration_at_last_pass;

	GF_SegmentIndexBox *sidx;
	GF_PcrInfoBox *pcrb;

	u32 reference_pid;
	GF_M2TS_PES *reference_stream;

	u32 nb_pes_in_segment;
	/* earliest presentation time for the whole segment */
	u64 first_PTS;
	/* DTS-PCR diff for the first PES packet */
	u64 PCR_DTS_initial_diff;

	/* earliest presentation time for the subsegment being processed */
	u64 base_PTS;
	/* byte offset for the start of subsegment being processed */
	u64 base_offset;
	/* last presentation time for the subsegment being processed (before the next subsegment is started) */
	u64 last_PTS;
	/* last decoding time for the subsegment being processed */
	u64 last_DTS;
	/* byte offset for the last PES packet for the subsegment being processed */
	u32 last_offset;
	u32 last_frame_duration;

	/* earliest presentation time for the previous subsegment */
	u64 prev_base_PTS;
	/* byte offset for the start of the previous subsegment */
	u64 prev_base_offset;
	/* last presentation time for the previous subsegment */
	u64 prev_last_PTS;
	/* byte offset for the last PES packet for the previous subsegment */
	u32 prev_last_offset;

	/* indicates if the current subsegment contains a SAP and its SAP type*/
	u32 SAP_type;
	/* indicates if the first PES in the current subsegment is a SAP*/
	Bool first_pes_sap;
	/* Presentation time for the first RAP encountered in the subsegment */
	u64 first_SAP_PTS;
	/* byte offset for the first RAP encountered in the subsegment */
	u32 first_SAP_offset;
	u64 prev_last_SAP_PTS;
	u32 prev_last_SAP_offset;
	u64 last_SAP_PTS;
	u32 last_SAP_offset;

	Bool pes_after_last_pat_is_sap;

	/*Interpolated PCR value for the pcrb*/
	u64 interpolated_pcr_value;
	u64 last_pcr_value;

	/* information about the first PAT found in the subsegment */
	u32 last_pat_position;
	u32 first_pat_position;
	u32 prev_last_pat_position;
	Bool first_pat_position_valid, first_pes_after_last_pat;
	u32 pat_version;

	/* information about the first CAT found in the subsegment */
	u32 last_cat_position;
	u32 first_cat_position;
	u32 prev_last_cat_position;
	Bool first_cat_position_valid;
	u32 cat_version;

	/* information about the first PMT found in the subsegment */
	u32 last_pmt_position;
	u32 first_pmt_position;
	u32 prev_last_pmt_position;
	Bool first_pmt_position_valid;
	u32 pmt_version;

	/* information about the first PCR found in the subsegment */
	u32 last_pcr_position;
	u32 first_pcr_position;
	Bool first_pcr_position_valid;
	u32 prev_last_pcr_position;

} GF_TSSegmenter;

static void m2ts_sidx_add_entry(GF_SegmentIndexBox *sidx, Bool ref_type,
                                u32 size, u32 duration, Bool first_is_SAP, u32 sap_type, u32 RAP_delta_time)
{
	GF_SIDXReference *ref;
	sidx->nb_refs++;
	sidx->refs = (GF_SIDXReference *)gf_realloc(sidx->refs, sidx->nb_refs*sizeof(GF_SIDXReference));
	ref = &(sidx->refs[sidx->nb_refs-1]);
	ref->reference_type = ref_type;
	ref->reference_size = size;
	ref->subsegment_duration = duration;
	ref->starts_with_SAP = first_is_SAP;
	ref->SAP_type = sap_type;
	ref->SAP_delta_time = (sap_type ? RAP_delta_time: 0);
}

static void m2ts_sidx_finalize_size(GF_TSSegmenter *index_info, u64 file_size)
{
	GF_SIDXReference *ref;
	if (index_info->suspend_indexing) return;
	if (index_info->sidx->nb_refs == 0) return;
	ref = &(index_info->sidx->refs[index_info->sidx->nb_refs-1]);
	ref->reference_size = (u32)(file_size - index_info->prev_base_offset);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Subsegment: position-range ajdustment:%d-%d (%d bytes)\n", index_info->prev_base_offset, (u32)file_size, ref->reference_size));
}

static void m2ts_sidx_flush_entry(GF_TSSegmenter *index_info)
{
	u32 end_offset, size, duration;
	Bool store_segment = GF_TRUE;

	if (index_info->suspend_indexing)
		return;

	if (index_info->skip_nb_segments) {
		index_info->skip_nb_segments --;
		store_segment = GF_FALSE;
	}

	/* determine the end of the current index */
	if (index_info->segment_at_rap) {
		/*split at PAT*/
		end_offset = index_info->last_pat_position;
	} else {
		/* split at PES header */
		end_offset = index_info->last_offset;
	}
	size = (u32)(end_offset - index_info->base_offset);
	duration = (u32)(index_info->last_PTS - index_info->base_PTS);

	if (store_segment) {
		u32 prev_duration, SAP_delta_time, SAP_offset;
		if (!index_info->sidx) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Segment: Reference PID: %d, EPTime: "LLU", Start Offset: %d bytes\n", index_info->reference_pid, index_info->base_PTS, index_info->base_offset));
			index_info->sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
			index_info->sidx->reference_ID = index_info->reference_pid;
			/* timestamps in MPEG-2 are expressed in 90 kHz timescale */
			index_info->sidx->timescale = 90000;
			/* first encountered PTS on the PID for this subsegment */
			index_info->sidx->earliest_presentation_time = index_info->base_PTS;
			index_info->sidx->first_offset = index_info->base_offset;

		}

		if (!index_info->pcrb) {
			index_info->pcrb = (GF_PcrInfoBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_PCRB);
		}

		/* close the current index */
		SAP_delta_time = (u32)(index_info->first_SAP_PTS - index_info->base_PTS);
		SAP_offset = (u32)(index_info->first_SAP_offset - index_info->base_offset);
		m2ts_sidx_add_entry(index_info->sidx, GF_FALSE, size, duration, index_info->first_pes_sap, index_info->SAP_type, SAP_delta_time);

		/*add pcrb entry*/
		index_info->pcrb->subsegment_count++;
		index_info->pcrb->pcr_values = (u64*)gf_realloc(index_info->pcrb->pcr_values, index_info->pcrb->subsegment_count*sizeof(u64));
		index_info->pcrb->pcr_values[index_info->pcrb->subsegment_count-1] = index_info->interpolated_pcr_value;

		/* adjust the previous index duration */
		if (index_info->sidx->nb_refs > 0 && (index_info->base_PTS < index_info->prev_last_PTS) ) {
			prev_duration = (u32)(index_info->base_PTS-index_info->prev_base_PTS);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("           time-range adj.: %.3f-%.3f / %.3f sec.\n",
			                                        (index_info->prev_base_PTS - index_info->first_PTS)/90000.0,
			                                        (index_info->base_PTS - index_info->first_PTS)/90000.0, prev_duration/90000.0));

			/*update previous duration*/
			if (index_info->sidx->nb_refs) {
				index_info->sidx->refs[index_info->sidx->nb_refs-1].subsegment_duration = prev_duration;
			}

		}

		/* Printing result */
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Subsegment:"));
		//time-range:position-range:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (" %.3f-%.3f / %.3f sec., %d-%d / %d bytes, ",
		                                        (index_info->base_PTS - index_info->first_PTS)/90000.0,
		                                        (index_info->last_PTS - index_info->first_PTS)/90000.0, duration/90000.0,
		                                        index_info->base_offset, end_offset, size));
		if (index_info->SAP_type) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("RAP @ %.3f sec. / %d bytes", (index_info->first_SAP_PTS - index_info->first_PTS)/90000.0,
			                                        SAP_offset));
		}
		if (index_info->first_pat_position_valid) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", PAT @ %d bytes", (u32)(index_info->first_pat_position - index_info->base_offset)));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No PAT"));
		}
		if (index_info->first_cat_position_valid) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", CAT @ %d bytes", (u32)(index_info->first_cat_position - index_info->base_offset)));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No CAT"));
		}
		if (index_info->first_pmt_position_valid) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", PMT @ %d bytes", (u32)(index_info->first_pmt_position - index_info->base_offset)));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No PMT"));
		}
		if (index_info->first_pcr_position_valid) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", PCR @ %d bytes", (u32)(index_info->first_pcr_position - index_info->base_offset)));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (", No PCR"));
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\n"));

	}

	/* save the current values for later adjustments */
	index_info->prev_last_SAP_PTS = index_info->last_SAP_PTS;
	index_info->prev_last_SAP_offset = index_info->last_SAP_offset;
	index_info->prev_last_PTS = index_info->last_PTS;
	index_info->prev_last_offset = index_info->last_offset;
	index_info->prev_base_PTS = index_info->base_PTS;
	index_info->base_PTS = index_info->last_PTS;
	index_info->prev_base_offset = index_info->base_offset;
	index_info->prev_last_pat_position = index_info->last_pat_position;
	index_info->prev_last_cat_position = index_info->last_cat_position;
	index_info->prev_last_pmt_position = index_info->last_pmt_position;
	index_info->prev_last_pcr_position = index_info->last_pcr_position;

	/* update the values for the new index*/
	index_info->base_offset = end_offset;

	if (index_info->pes_after_last_pat_is_sap) {
		index_info->SAP_type = 1;
		index_info->first_SAP_PTS = index_info->last_SAP_PTS;
		index_info->first_SAP_offset = index_info->last_SAP_offset;
		index_info->first_pes_sap = GF_TRUE;
		index_info->nb_pes_in_segment = 1;
		index_info->base_PTS = index_info->first_SAP_PTS;

		index_info->pes_after_last_pat_is_sap = GF_FALSE;
	} else {
		index_info->SAP_type = 0;
		index_info->first_SAP_PTS = 0;
		index_info->first_SAP_offset = 0;
		index_info->first_pes_sap = GF_FALSE;
		index_info->nb_pes_in_segment = 0;
	}
	index_info->last_DTS = 0;

	if (index_info->last_pat_position >= index_info->base_offset) {
		index_info->first_pat_position_valid = GF_TRUE;
		index_info->first_pat_position = index_info->last_pat_position;
	} else {
		index_info->first_pat_position_valid = GF_FALSE;
		index_info->first_pat_position = 0;
	}
	if (index_info->last_cat_position >= index_info->base_offset) {
		index_info->first_cat_position_valid = GF_TRUE;
		index_info->first_cat_position = index_info->last_cat_position;
	} else {
		index_info->first_cat_position_valid = GF_FALSE;
		index_info->first_cat_position = 0;
	}
	if (index_info->last_pmt_position >= index_info->base_offset) {
		index_info->first_pmt_position_valid = GF_TRUE;
		index_info->first_pmt_position = index_info->last_pmt_position;
	} else {
		index_info->first_pmt_position_valid = GF_FALSE;
		index_info->first_pmt_position = 0;
	}
	if (index_info->last_pcr_position >= index_info->base_offset) {
		index_info->first_pcr_position_valid = GF_TRUE;
		index_info->first_pcr_position = index_info->last_pcr_position;
	} else {
		index_info->first_pcr_position_valid = GF_FALSE;
		index_info->first_pcr_position = 0;
	}

	if (index_info->subduration && (index_info->last_PTS - index_info->first_PTS >= index_info->duration_at_last_pass + index_info->subduration)) {
		index_info->suspend_indexing = end_offset;
		index_info->duration_at_last_pass = (u32) (index_info->last_PTS - index_info->first_PTS);
	}
}

static void m2ts_check_indexing(GF_TSSegmenter *index_info)
{
	u32 segment_duration = (u32)(index_info->segment_duration*90000);
	u64 current_duration = (index_info->last_PTS - index_info->base_PTS);
	u64 target_duration = segment_duration;

	//using drift control to ensure that nb_seg*segment_duration is roughly aligned with segment(nb_seg).first_PTS
	if (index_info->sidx) {
		current_duration = (index_info->last_PTS - index_info->first_PTS) - index_info->duration_at_last_pass;
		target_duration += index_info->sidx->nb_refs * segment_duration;
	}

	if (index_info->segment_at_rap) {
		s32 diff = (s32) ((s64) current_duration - (s64) segment_duration);
		if (diff<0)diff=-diff;
		//check if below 10% limit
		if (diff/900 < 5) {
			//below 10% limit of segment duration, if we have a RAP flush now
			if (index_info->pes_after_last_pat_is_sap)
				m2ts_sidx_flush_entry(index_info);
			//no rap and below 10% limit, wait for rap
			return;
		}
	}

	/* we exceed the segment duration flush sidx entry*/
	if (current_duration >= target_duration) {
		m2ts_sidx_flush_entry(index_info);
	}
}
static void dash_m2ts_event_check_pat(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_TSSegmenter *ts_seg = (GF_TSSegmenter*)ts->user;
	GF_M2TS_PES_PCK *pck;
	switch (evt_type) {
	case GF_M2TS_EVT_PAT_REPEAT:
		ts_seg->has_seen_pat = GF_TRUE;
		break;
	case GF_M2TS_EVT_PMT_FOUND:
	{
		u32 i, count;
		GF_M2TS_Program *prog = (GF_M2TS_Program*)par;
		count = gf_list_count(prog->streams);
		for (i=0; i<count; i++) {
			GF_M2TS_ES *es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
			gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_DEFAULT);
		}
	}
	break;
	case GF_M2TS_EVT_PMT_REPEAT:
		if (ts_seg->has_seen_pat) {
			u32 i, count;
			Bool done = GF_TRUE;
			GF_M2TS_Program *prog = (GF_M2TS_Program*)par;
			count = gf_list_count(prog->streams);
			for (i=0; i<count; i++) {
				GF_M2TS_ES *es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
				if (es->flags&GF_M2TS_ES_IS_PES) {
					GF_M2TS_PES *pes = (GF_M2TS_PES *)es;
					if (!pes->aud_sr && !pes->vid_w) {
						done = GF_FALSE;
						break;
					} else {
						/*stream is configured, we only need PES header now*/
						gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_RAW);
					}
				}
			}
			if (done) ts_seg->has_seen_pat = 2;
		}
		break;
	case GF_M2TS_EVT_PES_PCK:
		pck = (GF_M2TS_PES_PCK*)par;
		/* we process packets only for the given PID */
		if (pck->stream->pid == pck->stream->program->pcr_pid) {
			if (!ts_seg->nb_pes_in_segment || (ts_seg->first_PTS > pck->PTS)) {
				ts_seg->first_PTS = pck->PTS;
				ts_seg->nb_pes_in_segment++;
			}
			if (pck->PTS > ts_seg->last_PTS) {
				ts_seg->last_PTS = pck->PTS;
			}
			if (ts_seg->last_DTS != pck->DTS) {
				ts_seg->last_frame_duration = (u32) (pck->DTS - ts_seg->last_DTS);
				ts_seg->last_DTS = pck->DTS;
			}
		}
		break;
	}
}

static void dash_m2ts_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_M2TS_Program *prog;
	GF_M2TS_PES_PCK *pck;
	GF_M2TS_PES *pes;
	u64 interpolated_pcr_value;
	u32 count, i;
	GF_TSSegmenter *ts_seg = (GF_TSSegmenter*)ts->user;

	switch (evt_type) {
	case GF_M2TS_EVT_PAT_FOUND:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = GF_TRUE;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		ts_seg->first_pes_after_last_pat = GF_FALSE;
		ts_seg->pes_after_last_pat_is_sap = GF_FALSE;
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = GF_TRUE;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		ts_seg->first_pes_after_last_pat = GF_FALSE;
		ts_seg->pes_after_last_pat_is_sap = GF_FALSE;
		break;
	case GF_M2TS_EVT_PAT_REPEAT:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = GF_TRUE;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		ts_seg->first_pes_after_last_pat = GF_FALSE;
		ts_seg->pes_after_last_pat_is_sap = GF_FALSE;
		break;
	case GF_M2TS_EVT_CAT_FOUND:
		if (!ts_seg->first_cat_position_valid) {
			ts_seg->first_cat_position_valid = GF_TRUE;
			ts_seg->first_cat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_cat_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_CAT_UPDATE:
		if (!ts_seg->first_cat_position_valid) {
			ts_seg->first_cat_position_valid = GF_TRUE;
			ts_seg->first_cat_position = (ts->pck_number-1)*188;
		}
		break;
	case GF_M2TS_EVT_CAT_REPEAT:
		if (!ts_seg->first_cat_position_valid) {
			ts_seg->first_cat_position_valid = GF_TRUE;
			ts_seg->first_cat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_cat_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		ts_seg->has_seen_pat = GF_TRUE;
		prog = (GF_M2TS_Program*)par;
		if (!ts_seg->first_pmt_position_valid) {
			ts_seg->first_pmt_position_valid = GF_TRUE;
			ts_seg->first_pmt_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pmt_position = (ts->pck_number-1)*188;
		/* we create indexing information on the stream used for carrying the PCR */
		ts_seg->reference_pid = prog->pcr_pid;
		ts_seg->reference_stream = (GF_M2TS_PES *) ts_seg->ts->ess[prog->pcr_pid];
		count = gf_list_count(prog->streams);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program number %d found - %d streams:\n", prog->number, count));
		for (i=0; i<count; i++) {
			GF_M2TS_ES *es = (GF_M2TS_ES*)gf_list_get(prog->streams, i);
			gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_DEFAULT);
		}
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		if (!ts_seg->first_pmt_position_valid) {
			ts_seg->first_pmt_position_valid = GF_TRUE;
			ts_seg->first_pmt_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pmt_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
		if (!ts_seg->first_pmt_position_valid) {
			ts_seg->first_pmt_position_valid = GF_TRUE;
			ts_seg->first_pmt_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pmt_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PES_PCK:
		pck = (GF_M2TS_PES_PCK*)par;
		/*We need the interpolated PCR for the pcrb, hence moved this calculus out, and saving the calculated value in ts_seg to put it in the pcrb*/
		pes = pck->stream;
		if (pes->last_pcr_value && pes->before_last_pcr_value_pck_number && pes->last_pcr_value > pes->before_last_pcr_value) {
			u32 delta_pcr_pck_num = pes->last_pcr_value_pck_number - pes->before_last_pcr_value_pck_number;
			u32 delta_pts_pcr_pck_num = pes->pes_start_packet_number - pes->last_pcr_value_pck_number;
			u64 delta_pcr_value = pes->last_pcr_value - pes->before_last_pcr_value;
			if ((pes->pes_start_packet_number > pes->last_pcr_value_pck_number)
			        && (pes->last_pcr_value > pes->before_last_pcr_value)) {

				pes->last_pcr_value = pes->before_last_pcr_value;
			}
			/* we can compute the interpolated pcr value for the packet containing the PES header */
			interpolated_pcr_value = pes->last_pcr_value + (u64)((delta_pcr_value*delta_pts_pcr_pck_num*1.0)/delta_pcr_pck_num);
			ts_seg->interpolated_pcr_value = interpolated_pcr_value;
			ts_seg->last_pcr_value = pes->last_pcr_value;
		}
		/* we process packets only for the given PID */
		if (pck->stream->pid != ts_seg->reference_pid) {
			break;
		} else {
			if (ts_seg->last_DTS != pck->DTS) {
				if (ts_seg->last_DTS)
					ts_seg->last_frame_duration = (u32) (pck->DTS - ts_seg->last_DTS);
				ts_seg->last_DTS = pck->DTS;
				ts_seg->nb_pes_in_segment++;

				if (!ts_seg->first_pes_after_last_pat) {
					ts_seg->first_pes_after_last_pat = ts_seg->nb_pes_in_segment;
					ts_seg->pes_after_last_pat_is_sap = GF_FALSE;
				}
			}

			/* we store the fact that there is at least a RAP for the index
			and we store the PTS of the first encountered RAP in the index*/
			if (pck->flags & GF_M2TS_PES_PCK_RAP) {
				ts_seg->SAP_type = 1;
				if (!ts_seg->first_SAP_PTS || (ts_seg->first_SAP_PTS > pck->PTS)) {
					ts_seg->first_SAP_PTS = pck->PTS;
					ts_seg->first_SAP_offset = (pck->stream->pes_start_packet_number-1)*188;
				}
				ts_seg->last_SAP_PTS = pck->PTS;
				ts_seg->last_SAP_offset = (pck->stream->pes_start_packet_number-1)*188;

				if (ts_seg->nb_pes_in_segment==1) {
					ts_seg->first_pes_sap = GF_TRUE;
				}
				if ((ts_seg->nb_pes_in_segment>1) && (ts_seg->first_pes_after_last_pat==ts_seg->nb_pes_in_segment)) {
					ts_seg->pes_after_last_pat_is_sap=GF_TRUE;
				}
			}
			/* we need to know the earliest PTS value (RAP or not) in the index*/
			if (!ts_seg->base_PTS || (ts_seg->base_PTS > pck->PTS)) {
				ts_seg->base_PTS = pck->PTS;
			}
			/* we need to know the earliest PTS value for the whole file (segment) */
			if (!ts_seg->first_PTS || (ts_seg->first_PTS > pck->PTS)) {
				ts_seg->first_PTS = pck->PTS;
			}
			if (pck->PTS > ts_seg->last_PTS) {
				/* we use the last PTS for first approximation of the duration */
				ts_seg->last_PTS = pck->PTS;
				ts_seg->last_offset = (ts_seg->reference_stream->pes_start_packet_number-1)*188;
			}

			if (ts_seg->PCR_DTS_initial_diff == (u64) -1) {
				ts_seg->PCR_DTS_initial_diff = ts_seg->last_DTS - pes->last_pcr_value / 300;
			}

			m2ts_check_indexing(ts_seg);
		}
		break;
	case GF_M2TS_EVT_PES_PCR:
		//pck = (GF_M2TS_PES_PCK*)par;
		if (!ts_seg->first_pcr_position_valid) {
			ts_seg->first_pcr_position_valid = GF_TRUE;
			ts_seg->first_pcr_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pcr_position = (ts->pck_number-1)*188;
		break;
	}
}

static GF_Err dasher_get_ts_demux(GF_TSSegmenter *ts_seg, const char *file, u32 probe_mode)
{
	memset(ts_seg, 0, sizeof(GF_TSSegmenter));
	ts_seg->src = gf_fopen(file, "rb");
	if (!ts_seg->src) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot open input %s: no such file\n", file));
		return GF_URL_ERROR;
	}
	ts_seg->ts = gf_m2ts_demux_new();
	ts_seg->ts->on_event = dash_m2ts_event_check_pat;
	ts_seg->ts->notify_pes_timing = GF_TRUE;
	ts_seg->ts->user = ts_seg;

	gf_fseek(ts_seg->src, 0, SEEK_END);
	ts_seg->file_size = gf_ftell(ts_seg->src);
	gf_fseek(ts_seg->src, 0, SEEK_SET);

	if (probe_mode) {
		/* first loop to process all packets between two PAT, and assume all signaling was found between these 2 PATs */
		while (!feof(ts_seg->src)) {
			char data[188];
			s32 size = (s32) fread(data, 1, 188, ts_seg->src);
			if (size<0) {
				return GF_IO_ERR;
			}
			if (size<188) break;

			gf_m2ts_process_data(ts_seg->ts, data, size);
			if (ts_seg->has_seen_pat==probe_mode)
				break;
		}
		gf_m2ts_reset_parsers(ts_seg->ts);
		gf_fseek(ts_seg->src, 0, SEEK_SET);
	}
	ts_seg->ts->on_event = dash_m2ts_event;
	return GF_OK;
}

static void dasher_del_ts_demux(GF_TSSegmenter *ts_seg)
{
	if (ts_seg->ts) gf_m2ts_demux_del(ts_seg->ts);
	if (ts_seg->src) gf_fclose(ts_seg->src);
	memset(ts_seg, 0, sizeof(GF_TSSegmenter));
}

static GF_Err dasher_mp2t_get_components_info(GF_DashSegInput *dash_input, GF_DASHSegmenter *dash_opts)
{
	char sOpt[40];
	GF_TSSegmenter ts_seg;
	GF_Err e = dasher_generic_get_components_info(dash_input, dash_opts);
	if (e) return e;

	dash_input->duration = 0;
	if (dash_opts->dash_ctx) {
		const char *opt = gf_cfg_get_key(dash_opts->dash_ctx, "DASH", "LastFileName");
		if (opt && !strcmp(opt, dash_input->file_name)) {
			const char *opt = gf_cfg_get_key(dash_opts->dash_ctx, "DASH", "LastFileDuration");
			if (opt) dash_input->duration = atof(opt);
		}
	}

	e = dasher_get_ts_demux(&ts_seg, dash_input->file_name, dash_input->duration ? 2 : 3);
	if (e) return e;

	if (!dash_input->duration)
		dash_input->duration = (ts_seg.last_PTS + ts_seg.last_frame_duration - ts_seg.first_PTS)/90000.0;

	gf_cfg_set_key(dash_opts->dash_ctx, "DASH", "LastFileName", dash_input->file_name);
	sprintf(sOpt, "%g", dash_input->duration);
	gf_cfg_set_key(dash_opts->dash_ctx, "DASH", "LastFileDuration", sOpt);

	dasher_del_ts_demux(&ts_seg);

	return GF_OK;
}

#define NB_TSPCK_IO_BYTES 18800

static GF_Err dasher_mp2t_segment_file(GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenter *dasher, GF_MPD_AdaptationSet *adaptation_set_obj, Bool first_in_set)
{
	GF_TSSegmenter ts_seg;
	Bool rewrite_input = GF_FALSE;
	u8 is_pes[GF_M2TS_MAX_STREAMS];
	char szOpt[100];
	char SegName[GF_MAX_PATH], IdxName[GF_MAX_PATH];
	char szSectionName[100], szRepURLsSecName[100];
	char szCodecs[100];
	const char *opt;
	u32 i;
	GF_Err e;
	u64 start, pcr_shift, next_pcr_shift, presentationTimeOffset;
	Double cumulated_duration = 0;
	u32 bandwidth = 0;
	u32 segment_index;
	/*compute name for indexed segments*/
	const char *basename = gf_dasher_strip_output_dir(dasher->mpd_name, szOutName);
	GF_MPD_Representation *representation_obj = NULL;
	GF_MPD_SegmentTimeline *seg_tl = NULL;
	GF_List *segment_urls = NULL;

	if (dash_input->media_duration) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] media duration cannot be forced with MPEG2-TS segmenter. Ignoring.\n"));
	}

	/*perform indexation of the file, this info will be destroyed at the end of the segment file routine*/
	e = dasher_get_ts_demux(&ts_seg, dash_input->file_name, 0);
	if (e) return e;

	ts_seg.segment_duration = dasher->segment_duration;
	ts_seg.segment_at_rap = dasher->segments_start_with_rap;

	ts_seg.bandwidth = (u32) (ts_seg.file_size * 8 / dash_input->duration);

	/*create bitstreams*/

	segment_index = 1;
	ts_seg.index_file = NULL;
	ts_seg.index_bs = NULL;
	presentationTimeOffset=0;
	if (!dasher->dash_ctx && (dasher->use_url_template != 2)) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		GF_SegmentTypeBox *styp;

		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, GF_TRUE, IdxName, szOutName, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, dasher->seg_rad_name, "six", 0, 0, 0, dasher->use_segment_timeline);

		ts_seg.index_file = gf_fopen(IdxName, "wb");
		if (!ts_seg.index_file) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot create index file %s\n", IdxName));
			e = GF_IO_ERR;
			goto exit;
		}
		ts_seg.index_bs = gf_bs_from_file(ts_seg.index_file, GF_BITSTREAM_WRITE);

		styp = (GF_SegmentTypeBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_STYP);
		styp->majorBrand = GF_ISOM_BRAND_RISX;
		styp->minorVersion = 0;
		styp->altBrand = (u32*)gf_malloc(sizeof(u32));
		styp->altBrand[0] = styp->majorBrand;
		styp->altCount = 1;
		gf_isom_box_size((GF_Box *)styp);
		gf_isom_box_write((GF_Box *)styp, ts_seg.index_bs);
		gf_isom_box_del((GF_Box *)styp);
#endif
	}

	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, GF_TRUE, IdxName, basename, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, dasher->seg_rad_name, "six", 0, 0, 0, dasher->use_segment_timeline);

	ts_seg.PCR_DTS_initial_diff = (u64) -1;
	ts_seg.subduration = (u32) (dasher->subduration * 90000);

	szSectionName[0] = 0;
	if (dasher->dash_ctx) {
		sprintf(szSectionName, "Representation_%s", dash_input->representationID);
		sprintf(szRepURLsSecName, "URLs_%s", dash_input->representationID);

		/*restart where we left last time*/
		opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "ByteOffset");
		if (opt) {
			u64 offset;
			sscanf(opt, LLU, &offset);

			while (!feof(ts_seg.src) && !ts_seg.has_seen_pat) {
				char data[NB_TSPCK_IO_BYTES];
				s32 size = (s32) fread(data, 1, NB_TSPCK_IO_BYTES, ts_seg.src);
				if (size<0) {
					e = GF_IO_ERR;
					goto exit;
				}
				gf_m2ts_process_data(ts_seg.ts, data, size);

				if (size<NB_TSPCK_IO_BYTES) break;
			}
			gf_m2ts_reset_parsers(ts_seg.ts);
			ts_seg.base_PTS = 0;
			gf_fseek(ts_seg.src, offset, SEEK_SET);
			ts_seg.base_offset = offset;
			ts_seg.ts->pck_number = (u32) (offset/188);
		}

		opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "InitialDTSOffset");
		if (opt) sscanf(opt, LLU, &ts_seg.PCR_DTS_initial_diff);

		opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "PresentationTimeOffset");
		if (opt) {
			sscanf(opt, LLU, &presentationTimeOffset);
			presentationTimeOffset++;
		}

		opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "DurationAtLastPass");
		if (opt) sscanf(opt, LLD, &ts_seg.duration_at_last_pass);
	}

	/*index the file*/
	while (!feof(ts_seg.src) && !ts_seg.suspend_indexing) {
		char data[NB_TSPCK_IO_BYTES];
		s32 size = (s32) fread(data, 1, NB_TSPCK_IO_BYTES, ts_seg.src);
		if (size<0) {
			e = GF_IO_ERR;
			goto exit;
		}
		gf_m2ts_process_data(ts_seg.ts, data, size);
		if (size<NB_TSPCK_IO_BYTES) break;
	}
	if (feof(ts_seg.src)) ts_seg.suspend_indexing = 0;

	if (!presentationTimeOffset) {
		presentationTimeOffset = 1 + ts_seg.first_PTS;
		if (dasher->dash_ctx) {
			sprintf(szOpt, LLU, ts_seg.first_PTS);
			gf_cfg_set_key(dasher->dash_ctx, szSectionName, "PresentationTimeOffset", szOpt);
		}
	}

	next_pcr_shift = 0;
	if (!ts_seg.suspend_indexing) {
		next_pcr_shift = ts_seg.last_DTS + ts_seg.last_frame_duration - ts_seg.PCR_DTS_initial_diff;
	}

	/* flush SIDX entry for the last packets */
	m2ts_sidx_flush_entry(&ts_seg);
	m2ts_sidx_finalize_size(&ts_seg, ts_seg.file_size);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Indexing done (1 sidx, %d entries).\n", ts_seg.sidx->nb_refs));

	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, GF_TRUE, IdxName, basename, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, gf_dasher_strip_output_dir(dasher->mpd_name, dasher->seg_rad_name ? dasher->seg_rad_name : szOutName ), "six", 0, 0, 0, dasher->use_segment_timeline);


	memset(is_pes, 0, sizeof(u8)*GF_M2TS_MAX_STREAMS);
	for (i=0; i<dash_input->nb_components; i++) {
		is_pes[ dash_input->components[i].ID ] = 1;
	}
	pcr_shift = 0;

	bandwidth = dash_input->bandwidth;
	if (!bandwidth) bandwidth = ts_seg.bandwidth;

	if (dasher->dash_ctx) {

		opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "Setup");
		if (!opt || strcmp(opt, "yes")) {
			gf_cfg_set_key(dasher->dash_ctx, szSectionName, "Setup", "yes");
			gf_cfg_set_key(dasher->dash_ctx, szSectionName, "ID", dash_input->representationID);
		} else {
			if (!bandwidth) {
				opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "Bandwidth");
				if (opt) sscanf(opt, "%u", &bandwidth);
			}

			opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "StartIndex");
			if (opt) sscanf(opt, "%u", &segment_index);

			opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "PCR90kOffset");
			if (opt) sscanf(opt, LLU, &pcr_shift);

			opt = gf_cfg_get_key(dasher->dash_ctx, szSectionName, "CumulatedDuration");
			if (opt) {
				u64 val;
				sscanf(opt, LLU, &val);
				cumulated_duration = ((Double) val) / dasher->dash_scale;
			}
		}
	}

	/*write segment template for all representations*/
	if (first_in_set && dasher->seg_rad_name && dasher->use_url_template && !dasher->variable_seg_rad_name) {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, GF_TRUE, SegName, basename, dash_input->representationID, NULL, gf_dasher_strip_output_dir(dasher->mpd_name, dasher->seg_rad_name), "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);
		//fprintf(dash_cfg->mpd_file, "   <SegmentTemplate timescale=\"90000\" duration=\"%d\" startNumber=\"%d\" media=\"%s\"", (u32) (90000*dash_cfg->segment_duration), segment_index, SegName);
		//the SIDX we have is not compatible with the spec - until fixed, disable this
		GF_MPD_SegmentTemplate *seg_template;
		GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
		seg_template->start_number=(u32)-1;
		adaptation_set_obj->segment_template = seg_template;
		seg_template->timescale = 90000;
		seg_template->duration= (u32) (90000*dasher->segment_duration);
		seg_template->start_number= segment_index;
		seg_template->media = gf_strdup(SegName);

#if 0
		if (!dasher->dash_ctx) {
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, 1, IdxName, basename, dash_input->representationID, gf_dasher_strip_output_dir(dasher->mpd_name, dasher->seg_rad_name), "six", 0, bandwidth, segment_index, dasher->use_segment_timeline);
			fprintf(dasher->mpd, " index=\"%s\"", IdxName);
		}
#endif

		if (dasher->ast_offset_ms<0) {
			//fprintf(dash_cfg->mpd_file, " availabilityTimeOffset=\"%g\"", - (Double) dash_cfg->ast_offset_ms / 1000.0);
			seg_template->availability_time_offset=-(Double) dasher->ast_offset_ms / 1000.0;
		}
		//fprintf(dash_cfg->mpd_file, "/>\n");
	}

	representation_obj = gf_mpd_representation_new();
	gf_list_add(adaptation_set_obj->representations, representation_obj);

	if (strlen(dash_input->representationID)) {
		representation_obj->id = gf_strdup(dash_input->representationID);
	}

	char mime[256];
	sprintf(mime, "video/mp2t");
	representation_obj->mime_type = gf_strdup(mime);

	//fprintf(dash_cfg->mpd_file, "   <Representation id=\"%s\" mimeType=\"video/mp2t\"", dash_input->representationID);

	szCodecs[0] = 0;
	for (i=0; i<dash_input->nb_components; i++) {
		if (strlen(dash_input->components[i].szCodec))
			if (strlen(szCodecs))
				strcat(szCodecs, ",");
		strcat(szCodecs, dash_input->components[i].szCodec);

		if (dash_input->components[i].width && dash_input->components[i].height){
			//fprintf(dash_cfg->mpd_file, " width=\"%u\" height=\"%u\"", dash_input->components[i].width, dash_input->components[i].height);
			representation_obj->width=dash_input->components[i].width;
			representation_obj->height=dash_input->components[i].height;
		}

		if (dash_input->components[i].sample_rate){
			//fprintf(dash_cfg->mpd_file, " audioSamplingRate=\"%d\"", dash_input->components[i].sample_rate);
			representation_obj->samplerate = dash_input->components[i].sample_rate;
		}
	}
	if (strlen(szCodecs)){
		//fprintf(dash_cfg->mpd_file, " codecs=\"%s\"", szCodecs);
		representation_obj->codecs=gf_strdup(szCodecs);
	}

	//fprintf(dash_cfg->mpd_file, " startWithSAP=\"%d\"", dash_cfg->segments_start_with_rap ? 1 : 0);
	//fprintf(dash_cfg->mpd_file, " bandwidth=\"%d\"", bandwidth);
	//fprintf(dash_cfg->mpd_file, ">\n");
	representation_obj->starts_with_sap=dasher->segments_start_with_rap ? 1 : 0;
	representation_obj->bandwidth=bandwidth;

	/* writing Representation level descriptors */
//	if (dash_input->nb_rep_descs) {
//		for (i=0; i<dash_input->nb_rep_descs; i++) {
//			fprintf(dash_cfg->mpd_file, "    %s\n", dash_input->rep_descs[i]);
//		}
//	}
	if (dash_input->nb_rep_descs) {
		for (i=0; i<dash_input->nb_rep_descs; i++) {
			if (strchr(dash_input->rep_descs[i], '<') != NULL) {
				GF_MPD_other_descriptors *Desc;
				GF_SAFEALLOC(Desc,GF_MPD_other_descriptors);
				Desc->xml_desc=gf_strdup(dash_input->rep_descs[i]);
				gf_list_add(representation_obj->other_descriptors,Desc);
			}
		}
	}


	for (i=0; i<ts_seg.sidx->nb_refs; i++) {
		GF_SIDXReference *ref = &ts_seg.sidx->refs[i];
		if (dasher->mpd->max_segment_duration * ts_seg.sidx->timescale < ref->subsegment_duration) {
			dasher->mpd->max_segment_duration = ref->subsegment_duration;
			dasher->mpd->max_segment_duration /= ts_seg.sidx->timescale;
		}
	}

	if (dasher->single_file_mode==1) {
		GF_MPD_BaseURL *baseurl;
		GF_SAFEALLOC(baseurl, GF_MPD_BaseURL);
		if (!representation_obj->base_URLs) {
			representation_obj->base_URLs = gf_list_new();
		}
		gf_list_add(representation_obj->base_URLs, baseurl);
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, GF_TRUE, SegName, basename, dash_input->representationID, NULL, gf_dasher_strip_output_dir(dasher->mpd_name, dasher->seg_rad_name), "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);
		//fprintf(dash_cfg->mpd_file, "    <BaseURL>%s</BaseURL>\n", SegName);
		baseurl->URL = gf_strdup(SegName);

		//fprintf(dash_cfg->mpd_file, "    <SegmentBase>\n");
		//fprintf(dash_cfg->mpd_file, "     <RepresentationIndex sourceURL=\"%s\"/>\n", IdxName);
		//fprintf(dash_cfg->mpd_file, "    </SegmentBase>\n");
		GF_MPD_SegmentBase *segmentbase;
		GF_SAFEALLOC(segmentbase, GF_MPD_SegmentBase);
		representation_obj->segment_base = segmentbase;
		GF_MPD_URL *URL;
		GF_SAFEALLOC(URL, GF_MPD_URL);
		segmentbase->initialization_segment=URL;
		URL->sourceURL=gf_strdup(IdxName);

		/*we rewrite the file*/
		rewrite_input = GF_TRUE;
	} else {
		if (dasher->seg_rad_name && dasher->use_url_template) {
			if (dasher->variable_seg_rad_name) {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, GF_TRUE, SegName, basename, dash_input->representationID, NULL, gf_dasher_strip_output_dir(dasher->mpd_name, dasher->seg_rad_name), "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);
				//fprintf(dash_cfg->mpd_file, "    <SegmentTemplate timescale=\"90000\" duration=\"%d\" startNumber=\"%d\" media=\"%s\"", (u32) (90000*dash_cfg->segment_duration), segment_index, SegName);
				GF_MPD_SegmentTemplate *seg_template;
				GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
				seg_template->start_number=(u32)-1;
				adaptation_set_obj->segment_template = seg_template;
				seg_template->timescale = 90000;
				seg_template->duration= (u32) (90000*dasher->segment_duration);
				seg_template->start_number= segment_index;
				seg_template->media = gf_strdup(SegName);

				//the SIDX we have is not compatible with the spec - until fixed, disable this
#if 0
				if (!dash_cfg->dash_ctx) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, 1, IdxName, basename, dash_input->representationID, gf_dasher_strip_output_dir(dash_cfg->mpd_name, dash_cfg->seg_rad_name), "six", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
					fprintf(dash_cfg->mpd_file, " index=\"%s\"", IdxName);
				}
#endif

				if (presentationTimeOffset > 1){
					//fprintf(dasher->mpd_file, " presentationTimeOffset=\""LLD"\"", presentationTimeOffset - 1);
					seg_template->presentation_time_offset=presentationTimeOffset - 1;
				}

				if (dasher->ast_offset_ms<0) {
					//fprintf(dash_cfg->mpd_file, " availabilityTimeOffset=\"%g\"", - (Double) dasher->ast_offset_ms / 1000.0);
					seg_template->availability_time_offset= - (Double) dasher->ast_offset_ms / 1000.0;
				}
				//fprintf(dasher->mpd_file, "/>\n");
			} else if (presentationTimeOffset > 1) {
				GF_MPD_SegmentTemplate *seg_template;
				GF_SAFEALLOC(seg_template, GF_MPD_SegmentTemplate);
				seg_template->start_number=(u32)-1;
				adaptation_set_obj->segment_template = seg_template;
				//fprintf(dasher->mpd_file, "    <SegmentTemplate presentationTimeOffset=\""LLD"\"/>\n", presentationTimeOffset - 1);
				seg_template->presentation_time_offset=presentationTimeOffset - 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] PTSOffset "LLD" - startNumber %d - time %g\n", presentationTimeOffset - 1, segment_index, (Double) (s64) (ts_seg.sidx->earliest_presentation_time + pcr_shift) / 90000.0));
			}
		} else {
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, GF_TRUE, SegName, basename, dash_input->representationID, NULL, gf_dasher_strip_output_dir(dasher->mpd_name, dasher->seg_rad_name ? dasher->seg_rad_name : szOutName), "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);
			if (dasher->single_file_mode){
				GF_MPD_BaseURL *baseurl;
				GF_SAFEALLOC(baseurl, GF_MPD_BaseURL);
				if (!representation_obj->base_URLs) {
					representation_obj->base_URLs = gf_list_new();
				}
				gf_list_add(representation_obj->base_URLs, baseurl);

				//fprintf(dasher->mpd_file, "    <BaseURL>%s</BaseURL>\n",SegName);
				baseurl->URL = gf_strdup(SegName);
			}
			//fprintf(dasher->mpd_file, "    <SegmentList timescale=\"90000\" duration=\"%d\"", (u32) (90000*dasher->segment_duration));
			GF_MPD_SegmentList *seg_list;
			GF_SAFEALLOC(seg_list, GF_MPD_SegmentList);
			representation_obj->segment_list = seg_list;
			seg_list->start_number=(u32)-1;
			seg_list->timescale=90000;
			seg_list->duration=(u32) (90000*dasher->segment_duration);

			if (presentationTimeOffset > 1){
				seg_list->presentation_time_offset=presentationTimeOffset - 1;
				//fprintf(dasher->mpd_file, " presentationTimeOffset=\""LLD"\"", presentationTimeOffset - 1);
			}

			if (dasher->ast_offset_ms<0) {
				//fprintf(dasher->mpd_file, " availabilityTimeOffset=\"%g\"", - (Double) dasher->ast_offset_ms / 1000.0);
				seg_list->availability_time_offset=- (Double) dasher->ast_offset_ms / 1000.0;
			}
			//fprintf(dasher->mpd_file, ">\n");

			if (!dasher->dash_ctx) {
				//GF_MPD_Representation
				//seg_list->representation_index->byte_range
				GF_MPD_URL *rep_index;
				GF_SAFEALLOC(rep_index,GF_MPD_URL);
				seg_list->representation_index=rep_index;
				rep_index->sourceURL=gf_strdup(IdxName);
				//fprintf(dasher->mpd_file, "     <RepresentationIndex sourceURL=\"%s\"/>\n", IdxName);
			}
		}

		/*rewrite previous SegmentList entries*/
		if ( dasher->dash_ctx && ((dasher->single_file_mode==2) || (!dasher->single_file_mode && !dasher->use_url_template))) {
			/*rewrite previous URLs*/
			const char *opt;
			u32 count, i;
			count = gf_cfg_get_key_count(dasher->dash_ctx, szRepURLsSecName);
			for (i=0; i<count; i++) {
				const char *key_name = gf_cfg_get_key_name(dasher->dash_ctx, szRepURLsSecName, i);
				opt = gf_cfg_get_key(dasher->dash_ctx, szRepURLsSecName, key_name);
				//fprintf(dasher->mpd_file, "     %s\n", opt);
			}
		}

		if (dasher->single_file_mode==2) {

			start = ts_seg.sidx->first_offset;
			for (i=0; i<ts_seg.sidx->nb_refs; i++) {
				GF_SIDXReference *ref = &ts_seg.sidx->refs[i];
				GF_MPD_SegmentURL *seg_url;
				GF_MPD_SegmentList *seg_list=representation_obj->segment_list;
				seg_url = gf_mpd_segmenturl_new(NULL, start, start + ref->reference_size - 1, NULL, 0, 0);
				if(!seg_list->segment_URLs)
					seg_list->segment_URLs=gf_list_new();
				gf_list_add(seg_list->segment_URLs, seg_url);

				//fprintf(dasher->mpd_file, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"/>\n", start, start+ref->reference_size-1);
				//seg_url->media_range=ByteRange;
				//seg_url = gf_mpd_segmenturl_new(NULL, start, start + ref->reference_size - 1, NULL, 0, 0);
				start += ref->reference_size;
			}
			rewrite_input = GF_TRUE;

		} else {
			FILE *src, *dst;
			u64 pos, end;
			Double current_time = cumulated_duration, dur;
			src = gf_fopen(dash_input->file_name, "rb");
			start = ts_seg.sidx->first_offset;
			for (i=0; i<ts_seg.sidx->nb_refs; i++) {
				char buf[NB_TSPCK_IO_BYTES];
				GF_SIDXReference *ref = &ts_seg.sidx->refs[i];

				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, GF_TRUE, SegName, szOutName, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, dasher->seg_rad_name, "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);

				/*warning - we may introduce repeated sequence number when concatenating files. We should use switching
				segments to force reset of the continuity counter for all our pids - we don't because most players don't car ...*/
				if (dasher->use_url_template != 2) {
					dst = gf_fopen(SegName, "wb");
					if (!dst) {
						gf_fclose(src);
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot create segment file %s\n", SegName));
						e = GF_IO_ERR;
						goto exit;
					}

					dur = ref->subsegment_duration;
					dur /= 90000;

					gf_dasher_store_segment_info(dasher, dash_input->representationID, SegName, (u64) (current_time*dasher->dash_scale), (u64) ((current_time+dur)*dasher->dash_scale));

					current_time += dur;

					gf_fseek(src, start, SEEK_SET);
					pos = start;
					end = start+ref->reference_size;
					while (pos<end) {
						s32 res;
						u32 to_read = NB_TSPCK_IO_BYTES;
						if (pos+NB_TSPCK_IO_BYTES >= end) {
							to_read = (u32) (end-pos);
						}
						res = (s32) fread(buf, 1, to_read, src);
						if (res == (s32) to_read) {
							gf_m2ts_restamp(buf, res, pcr_shift, is_pes);
							res = (u32) fwrite(buf, 1, to_read, dst);
						}
						if (res!=to_read) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] IO error while Extracting segment %03d / %03d\r", i+1, ts_seg.sidx->nb_refs));
							break;
						}
						pos += res;
					}
					gf_fclose(dst);
				}
				start += ref->reference_size;

				if (!dasher->use_url_template) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, GF_TRUE, SegName, basename, dash_input->representationID, NULL, dasher->seg_rad_name, "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);
					GF_MPD_SegmentURL *seg_url;
					GF_MPD_SegmentList *seg_list=representation_obj->segment_list;
					seg_url = gf_mpd_segmenturl_new(SegName, 0, 0, NULL, 0, 0);
					if(!seg_list->segment_URLs)
						seg_list->segment_URLs=gf_list_new();
					gf_list_add(seg_list->segment_URLs, seg_url);
					
					if (dasher->dash_ctx) {
						char szKey[100], szVal[4046];
						sprintf(szKey, "UrlInfo%d", segment_index);
						sprintf(szVal, "<SegmentURL media=\"%s\"/>", SegName);
						gf_cfg_set_key(dasher->dash_ctx, szRepURLsSecName, szKey, szVal);
					}
				}

				segment_index++;
				gf_set_progress("Extracting segment ", i+1, ts_seg.sidx->nb_refs);
			}
			gf_fclose(src);
		}
	}

	if (rewrite_input) {
		FILE *in, *out;
		u64 fsize, done;
		char buf[NB_TSPCK_IO_BYTES];
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, GF_TRUE, SegName, szOutName, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, dasher->seg_rad_name, "ts", 0, bandwidth, segment_index, dasher->use_segment_timeline);
		out = gf_fopen(SegName, "wb");
		if (!out) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot create segment file %s\n", SegName));
			e = GF_IO_ERR;
			goto exit;
		}
		in = gf_fopen(dash_input->file_name, "rb");
		gf_fseek(in, 0, SEEK_END);
		fsize = gf_ftell(in);
		gf_fseek(in, 0, SEEK_SET);
		done = 0;
		while (1) {
			s32 read = (s32) fread(buf, 1, NB_TSPCK_IO_BYTES, in);
			if (read<0) {
				e = GF_IO_ERR;
				goto exit;
			}
			gf_m2ts_restamp(buf, read, pcr_shift, is_pes);
			fwrite(buf, 1, read, out);
			done+=read;
			gf_set_progress("Extracting segment ", done/188, fsize/188);
			if (read<NB_TSPCK_IO_BYTES) break;
		}
		gf_fclose(in);
		gf_fclose(out);
	}


	if (dasher->dash_ctx) {
		sprintf(szOpt, "%u", bandwidth);
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "Bandwidth", szOpt);

		sprintf(szOpt, "%u", segment_index);
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "StartIndex", szOpt);

		sprintf(szOpt, LLU, (u64) (dasher->dash_scale*ts_seg.segment_duration) );
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "CumulatedDuration", szOpt);

		sprintf(szOpt, LLU, next_pcr_shift + pcr_shift);
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "PCR90kOffset", szOpt);

		sprintf(szOpt, LLU, ts_seg.suspend_indexing);
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "ByteOffset", ts_seg.suspend_indexing ? szOpt : NULL);

		sprintf(szOpt, LLD, ts_seg.duration_at_last_pass);
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "DurationAtLastPass", ts_seg.suspend_indexing ? szOpt : NULL);

		sprintf(szOpt, LLU, ts_seg.PCR_DTS_initial_diff);
		gf_cfg_set_key(dasher->dash_ctx, szSectionName, "InitialDTSOffset", ts_seg.suspend_indexing ? szOpt : NULL);
	}

	if (ts_seg.sidx && ts_seg.index_bs) {
		gf_isom_box_size((GF_Box *)ts_seg.sidx);
		gf_isom_box_write((GF_Box *)ts_seg.sidx, ts_seg.index_bs);
	}
	if (ts_seg.pcrb && ts_seg.index_bs) {
		gf_isom_box_size((GF_Box *)ts_seg.pcrb);
		gf_isom_box_write((GF_Box *)ts_seg.pcrb, ts_seg.index_bs);
	}

exit:
	if (ts_seg.sidx) gf_isom_box_del((GF_Box *)ts_seg.sidx);
	if (ts_seg.pcrb) gf_isom_box_del((GF_Box *)ts_seg.pcrb);
	if (ts_seg.index_bs) gf_bs_del(ts_seg.index_bs);
	if (ts_seg.index_file) {
		gf_fclose(ts_seg.index_file);
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, GF_TRUE, IdxName, szOutName, dash_input->representationID, dash_input->baseURL ? dash_input->baseURL[0] : NULL, dasher->seg_rad_name, "six", 0, 0, 0, dasher->use_segment_timeline);
		gf_delete_file(IdxName);
	}
	dasher_del_ts_demux(&ts_seg);
	return e;
}

#endif //GPAC_DISABLE_MPEG2TS

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
static char * gf_dash_get_representationID(GF_DashSegInput *inputs, u32 nb_dash_inputs, const char *file_name, u32 trackNum) {
	u32 i;

	for (i = 0; i < nb_dash_inputs; i++) {
		if (!strcmp(inputs[i].file_name, file_name) && (inputs[i].trackNum == trackNum))
			return inputs[i].representationID;
	}

	return 0; //we should never be here !!!!
}
#endif

static GF_Err gf_dash_segmenter_probe_input(GF_DashSegInput **io_dash_inputs, u32 *nb_dash_inputs, u32 idx)
{
	GF_DashSegInput *dash_inputs = *io_dash_inputs;
	GF_DashSegInput *dash_input = & dash_inputs[idx];
	char *uri_frag = strchr(dash_input->file_name, '#');
	FILE *t;

	if (uri_frag) uri_frag[0] = 0;

	t = gf_fopen(dash_input->file_name, "rb");
	if (!t) return GF_URL_ERROR;
	gf_fclose(t);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (gf_isom_probe_file(dash_input->file_name)) {
		GF_ISOFile *file;
		Bool has_tiling = GF_FALSE;
		u32 nb_track, j, k, max_nb_deps, cur_idx, rep_idx;
		const char *dep_representation_id;
		const char *dep_representation_file=NULL;

		strcpy(dash_input->szMime, "video/mp4");
		dash_input->dasher_create_init_segment = dasher_isom_create_init_segment;
		dash_input->dasher_input_classify = dasher_isom_classify_input;
		dash_input->dasher_get_components_info = dasher_isom_get_components_info;
		dash_input->dasher_segment_file = dasher_isom_segment_file;

		file = gf_isom_open(dash_input->file_name, GF_ISOM_OPEN_READ, NULL);
		if (!file) return gf_isom_last_error(NULL);

		nb_track = gf_isom_get_track_count(file);

		/*if this dash input file has only one track, or this has not the scalable track: handle as one representation*/
		if ((nb_track == 1) || !gf_isom_needs_layer_reconstruction(file)) {

			if (uri_frag) {
				u32 id = 0;
				if (!strnicmp(uri_frag+1, "trackID=", 8))
					id = atoi(uri_frag+9);
				else if (!strnicmp(uri_frag+1, "ID=", 3))
					id = atoi(uri_frag+4);
				else if (!stricmp(uri_frag+1, "audio") || !stricmp(uri_frag+1, "video")) {
					Bool check_video = !stricmp(uri_frag+1, "video") ? GF_TRUE : GF_FALSE;
					u32 i;
					for (i=0; i<nb_track; i++) {
						switch (gf_isom_get_media_type(file, i+1)) {
						case GF_ISOM_MEDIA_VISUAL:
							if (check_video) id = gf_isom_get_track_id(file, i+1);
							break;
						case GF_ISOM_MEDIA_AUDIO:
							if (!check_video) id = gf_isom_get_track_id(file, i+1);
							break;
						}
						if (id) break;
					}
				}
				dash_input->single_track_num = gf_isom_get_track_by_id(file, id);
				nb_track = dash_input->single_track_num;
			}
			dash_input->protection_scheme_type = gf_isom_is_media_encrypted(file, nb_track, 1);

			dash_input->moof_seqnum_increase = 0;

			gf_isom_close(file);
			return GF_OK;
		}

		max_nb_deps = 0;
		for (j = 0; j < nb_track; j++) {
			//check scalability
			if (gf_isom_get_media_type(file, j+1)==GF_ISOM_MEDIA_VISUAL) {
				u32 c1 = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_BASE);
				u32 c2 = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_TBAS);
				if (c2) has_tiling = GF_TRUE;
				if (c1 + c2) {
					max_nb_deps ++;
				}
			}
		}
#ifdef GENERATE_VIRTUAL_REP_SRD
		if (has_tiling) max_nb_deps++;
#endif
		//scalable input file, realloc
		j = *nb_dash_inputs + max_nb_deps;
		dash_inputs = (GF_DashSegInput*)gf_realloc(dash_inputs, sizeof(GF_DashSegInput) * j);
		memset(&dash_inputs[*nb_dash_inputs], 0, sizeof(GF_DashSegInput) * max_nb_deps);
		*io_dash_inputs = dash_inputs;

		dash_input = & dash_inputs[idx];
		dash_input->nb_representations = 1 + max_nb_deps;
		dash_input->idx_representations=0;
		rep_idx = 1;
		cur_idx = idx+1;
		dep_representation_id = dash_input->representationID;

		//create dash inputs
		for (j = 0; j < nb_track; j++) {
			u32 default_sample_group_index, id, independent;
			Bool full_frame;
			GF_DashSegInput *di;
			u32 count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_BASE);
			count += gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_TBAS);
			if (!count) {
				u32 x, y, w, h;
				Bool found_same_base = GF_FALSE;

				//special case for HEVC tile, do not dublicate base representation if same res
				count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_SABT);
				count += gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_SCAL);
				if (!count) {
					switch (gf_isom_get_media_subtype(file, j+1, 1)) {
					case GF_ISOM_SUBTYPE_LHE1:
					case GF_ISOM_SUBTYPE_LHV1:
						count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_BASE);
						break;
					}
				}

				if (!has_tiling || !count) {
					dash_input->trackNum = j+1;
					continue;
				}

				w = h = 0;
				gf_isom_get_tile_info(file, j+1, 1, &default_sample_group_index, &id, &independent, &full_frame, &x, &y, &w, &h);
				if (!w && !h) {
					gf_isom_get_visual_info(file, j+1, 1, &w, &h);
					x = y = 0;
				}
				//look in all previous dash inputs
				for (k=0; k<idx; k++) {
					GF_DashSegInput *prev_dash_input = & dash_inputs[k];
					if ( (prev_dash_input->x==x) && (prev_dash_input->y==y) && (prev_dash_input->w==w) && (prev_dash_input->h==h) ) {
						GF_ISOFile *temp = gf_isom_open(prev_dash_input->file_name, GF_ISOM_OPEN_READ, NULL);

						//same rep ?
						if (gf_isom_is_same_sample_description(file, j+1, 1, temp, prev_dash_input->trackNum, 1)) {
							found_same_base = GF_TRUE;
							dep_representation_id = prev_dash_input->representationID;
							dep_representation_file=prev_dash_input->file_name;
						}
						gf_isom_delete(temp);
						if (found_same_base) break;
					}
				}
				if (found_same_base) {
					cur_idx --;
					(*nb_dash_inputs) --;
					continue;
				}
				dash_input->trackNum = j+1;
				continue;
			}

			di = &dash_inputs [cur_idx];
			*nb_dash_inputs += 1;
			cur_idx++;
			//copy over values from base rep
			memcpy(di, dash_input, sizeof(GF_DashSegInput));

			/*representationID*/
			snprintf(di->representationID, sizeof(di->representationID), "%s_%d", dep_representation_id, cur_idx);
			di->trackNum = j+1;

			di->protection_scheme_type = gf_isom_is_media_encrypted(file, di->trackNum, 1);

			di->idx_representations = rep_idx;
			rep_idx ++;

			if (gf_isom_get_reference_count(file, di->trackNum, GF_ISOM_REF_TBAS)) {
				gf_isom_get_tile_info(file, di->trackNum, 1, &default_sample_group_index, &id, &independent, &full_frame, &di->x, &di->y, &di->w, &di->h);

				if (!dash_input->w) {
					gf_isom_get_visual_info(file, dash_input->trackNum, 1, &dash_input->w, &dash_input->h);
				}
			}
		}
#ifdef GENERATE_VIRTUAL_REP_SRD
		if (has_tiling) {
			GF_DashSegInput *di = &dash_inputs [cur_idx];
			*nb_dash_inputs += 1;
			cur_idx++;
			//copy over values from base rep
			memcpy(di, dash_input, sizeof(GF_DashSegInput));
			/*representationID*/
			sprintf(di->representationID, "%s_%d", dep_representation_id, cur_idx);

			di->trackNum = 0;
			for (k = 0; k < nb_track; k++) {
				//check scalability
				if (gf_isom_get_media_type(file, k+1)==GF_ISOM_MEDIA_VISUAL) {
					if (gf_isom_get_reference_count(file, k+1, GF_ISOM_REF_SABT)) {
						di->trackNum = k+1;
						break;
					}
				}
			}

			di->protection_scheme_type = gf_isom_is_media_encrypted(file, di->trackNum, 1);

			di->idx_representations = rep_idx;
			rep_idx ++;

			di->virtual_representation = GF_TRUE;
		}
#endif

		/*dependencyID - FIXME - the delaration of dependency and new dash_input entries should be in DEDENDENCY ORDER*/
		for (k=0; k<2; k++) {
			for (j = idx; j < *nb_dash_inputs; j++) {
				GF_DashSegInput *di;
				u32 count, t, ref_track;
				char *depID = NULL;
				u32 dep_type;

				di = &dash_inputs[j];

#ifdef GENERATE_VIRTUAL_REP_SRD
				if (k==0) dep_type = (di->virtual_representation) ? GF_ISOM_REF_SABT : GF_ISOM_REF_TBAS;
#else
				if (k==0) dep_type = GF_ISOM_REF_TBAS;
#endif
				else dep_type = GF_ISOM_REF_SCAL;

				count = gf_isom_get_reference_count(file, di->trackNum, dep_type);
				if (!count) {
					if (k==0) {
						switch (gf_isom_get_media_subtype(file, j+1, 1)) {
						case GF_ISOM_SUBTYPE_LHE1:
						case GF_ISOM_SUBTYPE_LHV1:
							dep_type = GF_ISOM_REF_BASE;
							count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_BASE);
							break;
						}
					}
					if (!count)
						continue;
				}

				di->lower_layer_track = dash_input->trackNum;
				depID = (char*)gf_malloc(2);
				strcpy(depID, "");
				for (t=0; t < count; t++) {
					u32 al_len = 0;
					char *rid;
					if (t) al_len++;
					gf_isom_get_reference(file, di->trackNum, dep_type, t+1, &ref_track);
					rid = gf_dash_get_representationID(dash_inputs, *nb_dash_inputs, dep_representation_file ? dep_representation_file : di->file_name, ref_track);

					if (!rid) continue;
					al_len += (u32) strlen(rid);
					al_len += (u32) strlen(depID)+1;

					depID = (char*)gf_realloc(depID, sizeof(char)*al_len);
					if (t)
						strcat(depID, " ");
					strcat(depID, rid);

					di->lower_layer_track = ref_track;
				}
				di->dependencyID = depID;
			}
		}

		gf_isom_close(file);
		return GF_OK;
	}
#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

	dash_input->moof_seqnum_increase = 0;

#ifndef GPAC_DISABLE_MPEG2TS
	if (gf_m2ts_probe_file(dash_input->file_name)) {
		GF_TSSegmenter ts_seg;
		u32 count = 0;
		GF_Err e = dasher_get_ts_demux(&ts_seg, dash_input->file_name, 1);
		if (e) return e;
		count = gf_list_count(ts_seg.ts->programs);
		dasher_del_ts_demux(&ts_seg);
		if (count>1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] MPEG-2 TS file %s has several programs - this is not supported in MPEG-DASH - skipping\n", dash_input->file_name));
			return GF_NOT_SUPPORTED;
		}

		strcpy(dash_input->szMime, "video/mp2t");
		/*we don't use init segments with MPEG-2 TS*/
		dash_input->dasher_create_init_segment = NULL;
		dash_input->dasher_input_classify = dasher_generic_classify_input;
		dash_input->dasher_get_components_info = dasher_mp2t_get_components_info;
		dash_input->dasher_segment_file = dasher_mp2t_segment_file;
		return GF_OK;
	}
#endif

	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] File %s not supported for dashing - skipping\n", dash_input->file_name));
	return GF_NOT_SUPPORTED;
}

static u64 cfg_get_key_ms(GF_Config *iniFile, const char *secName, const char *keyName) {
	u32 sec, frac;
	Double time_ms,msec;
	//we only support profiles for which AST has to be the same
	const char *opt = gf_cfg_get_key(iniFile, secName, keyName);
	sscanf(opt, "%u:%u", &sec, &frac);

	time_ms = sec;
	time_ms *= 1000;
	msec = frac*1000.0;
	msec /= 0xFFFFFFFF;

	time_ms += (u32)msec;

	return (u64)time_ms;

}

static GF_Err mpd_set_header(GF_DASHSegmenter *dasher, Bool is_mpeg2, Double mpd_duration, Bool use_cenc, Bool use_xlink)
{
	char profiles_string[256];
	GF_XMLAttribute *att = NULL;

	dasher->mpd->publishTime = gf_net_get_ntp_ms();
	if (dasher->ast_offset_ms > 0) {
		dasher->mpd->publishTime += (u32)dasher->ast_offset_ms;
	}

	if (dasher->dash_mode != GF_DASH_STATIC) {
		if (dasher->dash_ctx) {
			dasher->mpd->availabilityStartTime = cfg_get_key_ms(dasher->dash_ctx, "DASH", "GenerationNTP");
		}
	}

	if (dasher->profile==GF_DASH_PROFILE_LIVE) {
		if (use_xlink && !is_mpeg2) {
			strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-ext-live:2014");
		} else {
			sprintf(profiles_string, "urn:mpeg:dash:profile:%s:2011", is_mpeg2 ? "mp2t-simple" : "isoff-live");
		}
	} else if (dasher->profile==GF_DASH_PROFILE_ONDEMAND) {
		if (use_xlink) {
			strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-ext-on-demand:2014");
		} else {
			strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-on-demand:2011");
		}
	} else if (dasher->profile==GF_DASH_PROFILE_MAIN) {
		sprintf(profiles_string, "urn:mpeg:dash:profile:%s:2011", is_mpeg2 ? "mp2t-main" : "isoff-main");
	} else if (dasher->profile==GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE) {
		strcpy(profiles_string, "urn:hbbtv:dash:profile:isoff-live:2012");
	} else if (dasher->profile==GF_DASH_PROFILE_AVC264_LIVE) {
		strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264");
	} else if (dasher->profile==GF_DASH_PROFILE_AVC264_ONDEMAND) {
		strcpy(profiles_string, "urn:mpeg:dash:profile:isoff-on-demand:2011,http://dashif.org/guidelines/dash264");
	} else {
		strcpy(profiles_string, "urn:mpeg:dash:profile:full:2011");
	}

	if (dasher->dash_profile_extension) {
		char profiles_w_ext[256];
		sprintf(profiles_w_ext, "%s,%s", profiles_string, dasher->dash_profile_extension);
		att = gf_xml_dom_create_attribute("profiles", profiles_w_ext);
		gf_list_add(dasher->mpd->attributes, att);
	} else {
		att = gf_xml_dom_create_attribute("profiles", profiles_string);
		gf_list_add(dasher->mpd->attributes, att);
	}

	if (use_cenc) {
		att = gf_xml_dom_create_attribute("xmlns:cenc", "urn:mpeg:cenc:2013");
		gf_list_add(dasher->mpd->attributes, att);
	}
	if (use_xlink) {
		att = gf_xml_dom_create_attribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
		gf_list_add(dasher->mpd->attributes, att);
	}

/*	fprintf(mpd, " <ProgramInformation moreInformationURL=\"%s\">\n", dasher->mpd->moreInfoURL ? dasher->moreInfoURL : "http://gpac.io");
	if (dasher->title)
		fprintf(mpd, "  <Title>%s</Title>\n", dasher->title);
	else
		fprintf(mpd, "  <Title>%s generated by GPAC</Title>\n", dasher->mpd_name);

	if (dasher->source)
		fprintf(mpd, "  <Source>%s</Source>\n", dasher->source);
	if (dasher->location)
		fprintf(mpd, "  <Location>%s</Location>\n", dasher->location);
	if (dasher->copyright)
		fprintf(mpd, "  <Copyright>%s</Copyright>\n", dasher->copyright);
	fprintf(mpd, " </ProgramInformation>\n");

	for (i=0; i<dasher->nb_base_urls; i++) {
		fprintf(mpd, " <BaseURL>%s</BaseURL>\n", dasher->base_urls[i]);
	}
	fprintf(mpd, "\n");*/
	return GF_OK;
}

static GF_Err set_period_header(GF_DASHSegmenter *dasher, GF_MPD_Period *period, const char *szID, Double period_start, Double period_duration,
                                  const char *xlink, u32 period_num, Bool insert_xmlns)
{
	u32 i,j;
	
	//if (insert_xmlns) {
		//fprintf(mpd, " xmlns=\"urn:mpeg:dash:schema:mpd:2011\" ");
	//}
	if (szID && strlen(szID)) {
		period->ID = gf_strdup(szID);
	}

	if (period_start || dasher->dash_mode) {
		period->start = (u64)(period_start*1000);
	}
	if (!dasher->dash_mode && period_duration) {
		period->duration = (u64)(period_duration*1000);
	}
	if (xlink) {
		period->xlink_href = gf_strdup(xlink);
	}

	/* writing Period level descriptors */
	for (i=0; i< dasher->nb_inputs; i++) {
		if (dasher->inputs[i].adaptation_set && (dasher->inputs[i].period==period_num)) {
			for (j = 0; j < dasher->inputs[i].nb_p_descs; j++) {
				if (strchr(dasher->inputs[i].p_descs[j], '<') != NULL) {
					GF_MPD_other_descriptors *Desc;
					GF_SAFEALLOC(Desc,GF_MPD_other_descriptors);
					Desc->xml_desc=gf_strdup(dasher->inputs[i].p_descs[j]);
					gf_list_add(period->other_descriptors,Desc);
					//fprintf(mpd, "  %s\n", dasher->inputs[i].p_descs[j]);										
				}				
			}
		}
	}
	
	return GF_OK;
}

static GF_Err set_adaptation_header(GF_MPD_AdaptationSet *adaptation_set_obj, GF_DashProfile profile, Bool use_url_template, u32 single_file_mode,
                                      GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 period_num, u32 adaptation_set_num, u32 first_rep_in_set,
                                      Bool bitstream_switching_mode, u32 max_width, u32 max_height, u32 dar_num, u32 dar_den, char *szMaxFPS, char *szLang, const char *szInitSegment, Bool segment_alignment_disabled, const char *mpd_name)
{
	u32 i, j;
	Bool is_on_demand = ((profile==GF_DASH_PROFILE_ONDEMAND) || (profile==GF_DASH_PROFILE_AVC264_ONDEMAND));
	GF_DashSegInput *first_rep = &dash_inputs[first_rep_in_set];
	GF_MPD_other_descriptors Desc;

	//force segmentAlignment in onDemand
	adaptation_set_obj->segment_alignment = (!is_on_demand  && segment_alignment_disabled) ? GF_FALSE : GF_TRUE;
	if (bitstream_switching_mode) {
		adaptation_set_obj->bitstream_switching = GF_TRUE;
	}
	if (first_rep->group_id) {
		adaptation_set_obj->group = first_rep->group_id;
	}
	if (max_width && max_height) {
		adaptation_set_obj->max_width = max_width;
		adaptation_set_obj->max_height = max_height;
		if (strlen(szMaxFPS)) {
			adaptation_set_obj->max_framerate = atoi(szMaxFPS);
		}

		gf_media_reduce_aspect_ratio(&dar_num, &dar_den);
		adaptation_set_obj->par->num = dar_num;
		adaptation_set_obj->par->den = dar_den;
	}


	//add lang even if "und" to comply with dash-if
	if (szLang) {
		adaptation_set_obj->lang = gf_strdup(szLang);
	}
	if ((profile==GF_DASH_PROFILE_ONDEMAND) || (profile==GF_DASH_PROFILE_AVC264_ONDEMAND)) {
		adaptation_set_obj->subsegment_alignment = !segment_alignment_disabled;
		//FIXME - we need inspection of the segments to figure out the SAP type !
		adaptation_set_obj->subsegment_starts_with_sap = GF_TRUE;
	}
	
	/* writing AdaptationSet level descriptors specified only on one input (non discriminating during classification)*/
	for (i=0; i< nb_dash_inputs; i++) {
		if ((dash_inputs[i].adaptation_set == adaptation_set_num) && (dash_inputs[i].period == period_num)) {
			for (j = 0; j < dash_inputs[i].nb_as_c_descs; j++) {
				if (strchr(dash_inputs[i].as_c_descs[j], '<') != NULL) {
					GF_MPD_other_descriptors *Desc;
					GF_SAFEALLOC(Desc,GF_MPD_other_descriptors);
					Desc->xml_desc=gf_strdup(dash_inputs[i].as_c_descs[j]);
					gf_list_add(adaptation_set_obj->other_descriptors,Desc);
					//fprintf(mpd, "   %s\n", dash_inputs[i].as_c_descs[j]);					
				}
			}
		}
	}

	//check SRD
	for (i=0; i< nb_dash_inputs; i++) {
		if ((dash_inputs[i].adaptation_set == adaptation_set_num) && (dash_inputs[i].period == period_num)) {
			if (dash_inputs[i].w && dash_inputs[i].h
#ifdef GENERATE_VIRTUAL_REP_SRD
			        && !dash_inputs[i].virtual_representation
#endif
			   ) {
				if (dash_inputs[i].dependencyID) {
					char value[256];
					GF_MPD_Descriptor *desc;
					sprintf(value, "1,%d,%d,%d,%d", dash_inputs[i].x, dash_inputs[i].y, dash_inputs[i].w, dash_inputs[i].h);
					desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:dash:srd:2014", value);
					gf_list_add(adaptation_set_obj->supplemental_properties, desc);
					break;
				}
				//this is the base layer
				else {
					GF_MPD_Descriptor *desc;
					desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:dash:srd:2014", "1,0,0,0,0");
					gf_list_add(adaptation_set_obj->essential_properties, desc);
					break;
				}
			}
		}
	}

	if (first_rep) {

		/* writing AdaptationSet level descriptors specified only all inputs for that AdaptationSet*/

		for (i=0; i<first_rep->nb_as_descs; i++) {
			if (strchr(first_rep->as_descs[i], '<') != NULL) {
				GF_MPD_other_descriptors *Desc;
				GF_SAFEALLOC(Desc,GF_MPD_other_descriptors);
				Desc->xml_desc=gf_strdup(first_rep->as_descs[i]);
				gf_list_add(adaptation_set_obj->other_descriptors,Desc);
				//fprintf(mpd, "   %s\n", first_rep->as_descs[i]);
			}
		}
		

		if (bitstream_switching_mode) {
			for (i=0; i<first_rep->nb_components; i++) {
				struct _dash_component *comp = &first_rep->components[i];
				if (comp->media_type == GF_ISOM_MEDIA_AUDIO) {
					GF_MPD_Descriptor *desc;
					char value[256];
					sprintf(value, "%d", comp->channels);
					desc = gf_mpd_descriptor_new(NULL, "urn:mpeg:dash:23003:3:audio_channel_configuration:2011", value);
					gf_list_add(adaptation_set_obj->audio_channels, desc);
				}
			}
		}

		/*set role*/
		if (first_rep->roles) {
			u32 r;
			for (r=0; r<first_rep->nb_roles; r++) {
				char *role = first_rep->roles[r];
				GF_MPD_Descriptor *desc;
				char *uri;
				GF_SAFEALLOC(desc, GF_MPD_Descriptor);
				if (!strcmp(role, "caption") || !strcmp(role, "subtitle") || !strcmp(role, "main")
			        || !strcmp(role, "alternate") || !strcmp(role, "supplementary") || !strcmp(role, "commentary")
			        || !strcmp(role, "dub") || !strcmp(role, "description") || !strcmp(role, "sign")
					 || !strcmp(role, "metadata") || !strcmp(role, "enhanced-audio- intelligibility")
				) {
					uri = "urn:mpeg:dash:role:2011";
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Unrecognized role %s - using GPAC urn for schemaID\n", role));
					uri = "urn:gpac:dash:role:2013";
				}
				desc = gf_mpd_descriptor_new(NULL, uri, role);
				gf_list_add(adaptation_set_obj->role, desc);
			}
		}

		if (first_rep->nb_components>1) {
			for (i=0; i<first_rep->nb_components; i++) {
				struct _dash_component *comp = &first_rep->components[i];
				GF_MPD_ContentComponent *component;
				GF_SAFEALLOC(component, GF_MPD_ContentComponent);
				component->id = comp->ID;
				switch (comp->media_type ) {
				case GF_ISOM_MEDIA_TEXT:
					component->type = gf_strdup("text");
					break;
				case GF_ISOM_MEDIA_VISUAL:
					component->type = gf_strdup("video");
					break;
				case GF_ISOM_MEDIA_AUDIO:
					component->type = gf_strdup("audio");
					break;
				case GF_ISOM_MEDIA_SCENE:
				case GF_ISOM_MEDIA_DIMS:
				default:
					component->type = gf_strdup("application");
					break;
				}
				/*if lang not specified at adaptationSet level, put it here*/
				if ((!szLang) && (comp->lang && strcmp(comp->lang, "und"))) {
					component->lang = gf_strdup(comp->lang);
				}
				gf_list_add(adaptation_set_obj->content_component, component);
			}
		}

		if (bitstream_switching_mode && !use_url_template && (single_file_mode!=1) && strlen(szInitSegment) ) {
			GF_MPD_SegmentList *seg;
			GF_SAFEALLOC(seg, GF_MPD_SegmentList);
			seg->start_number=(u32)-1;
			GF_SAFEALLOC(seg->initialization_segment, GF_MPD_URL);
			seg->initialization_segment->sourceURL = gf_strdup(gf_dasher_strip_output_dir(mpd_name, szInitSegment));
			adaptation_set_obj->segment_list = seg;
		}
	}
	return GF_OK;
}

static GF_Err gf_dasher_init_context(GF_DASHSegmenter *dasher, GF_DashDynamicMode *dash_mode, s32 *timeShiftBufferDepth, const char *periodID, s32 ast_shift_ms)
{
	const char *opt;
	char szVal[100];
	Bool first_run = GF_FALSE;
	GF_Config *dash_ctx = dasher->dash_ctx;

	if (!dash_ctx) return GF_BAD_PARAM;

	opt = gf_cfg_get_key(dash_ctx, "DASH", "SessionType");
	/*first run, init all options*/
	if (!opt) {
		first_run = GF_TRUE;
		sprintf(szVal, "%d", *timeShiftBufferDepth);
		gf_cfg_set_key(dash_ctx, "DASH", "SessionType", (*dash_mode == GF_DASH_DYNAMIC_DEBUG) ? "dynamic-debug" : ( *dash_mode ? "dynamic" : "static" ) );
		gf_cfg_set_key(dash_ctx, "DASH", "TimeShiftBufferDepth", szVal);
		gf_cfg_set_key(dash_ctx, "DASH", "StoreParams", "yes");
	}
	//switching from live to static
	else if (! (*dash_mode) && !strncmp(opt, "dynamic", 7)) {
		gf_cfg_set_key(dash_ctx, "DASH", "SessionType", "static");
	} else {
		*dash_mode = GF_DASH_STATIC;
		if (!strcmp(opt, "dynamic")) *dash_mode = GF_DASH_DYNAMIC;
		else if (!strcmp(opt, "dynamic-debug")) *dash_mode = GF_DASH_DYNAMIC_DEBUG;

		opt = gf_cfg_get_key(dash_ctx, "DASH", "TimeShiftBufferDepth");
		*timeShiftBufferDepth = atoi(opt);
		gf_cfg_set_key(dash_ctx, "DASH", "StoreParams", "no");
	}

	if (first_run) {
		u32 sec, frac;
#ifndef _WIN32_WCE
		time_t gtime;
#endif
		gf_net_get_ntp(&sec, &frac);

		if (dasher->start_date_sec_ntp) {
			dasher->nb_secs_to_discard = sec;
			dasher->nb_secs_to_discard -= dasher->start_date_sec_ntp;
			dasher->nb_secs_to_discard -= dasher->time_shift_depth;

			sec = dasher->start_date_sec_ntp;
			frac = dasher->start_date_sec_ntp_ms_frac;
		}


#ifndef _WIN32_WCE
		gtime = sec;
		gf_cfg_set_key(dash_ctx, "DASH", "GenerationTime", asctime(gmtime(&gtime)));
#endif
		sprintf(szVal, "%u:%u", sec, frac);
		gf_cfg_set_key(dash_ctx, "DASH", "GenerationNTP", szVal);

		if (periodID)
			gf_cfg_set_key(dash_ctx, "DASH", "PeriodID", periodID);
	} else {

		/*check if we change period*/
		if (periodID) {
			opt = gf_cfg_get_key(dash_ctx, "DASH", "PeriodID");
			if (!opt || strcmp(opt, periodID)) {
				gf_cfg_set_key(dash_ctx, "DASH", "PeriodID", periodID);
			}
		}
	}

	/*perform cleanup of previous representations*/

	return GF_OK;
}

GF_EXPORT
u32 gf_dasher_next_update_time(GF_DASHSegmenter *dasher, u64 *ms_in_session)
{
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
	return 0;
}

/*peform all file cleanup*/
static Bool gf_dasher_cleanup(GF_DASHSegmenter *dasher)
{
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
	if (dasher->mpd->time_shift_buffer_depth >= 0) {
		for (i=0; i<gf_cfg_get_key_count(dasher->dash_ctx, "SegmentsStartTimes"); i++) {
			Double seg_time;
			u64 start, dur;
			char szRepID[100];
			char szSecName[200];
			u32 j;
			const char *fileName = gf_cfg_get_key_name(dasher->dash_ctx, "SegmentsStartTimes", i);
			const char *opt = gf_cfg_get_key(dasher->dash_ctx, "SegmentsStartTimes", fileName);
			if (!fileName)
				break;

			sscanf(opt, ""LLU"-"LLU"@%s", &start, &dur, szRepID);
			seg_time = (Double) start;
			seg_time /= dasher->dash_scale;
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
	return GF_TRUE;
}

static u32 gf_dash_get_dependency_bandwidth(GF_DashSegInput *inputs, u32 nb_dash_inputs, const char *file_name, u32 trackNum) {
	u32 i;

	for (i = 0; i < nb_dash_inputs; i++) {
		if (!strcmp(inputs[i].file_name, file_name) && (inputs[i].trackNum == trackNum))
			return inputs[i].bandwidth;
	}

	return 0; //we should never be here !!!!
}

static GF_Err dash_insert_period_xml(FILE *mpd, char *szPeriodXML)
{
	FILE *period_mpd;
	u32 xml_size;

	period_mpd = gf_fopen(szPeriodXML, "rb");
	if (!period_mpd) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Error opening period MPD file %s\n", szPeriodXML));
		return GF_IO_ERR;
	}
	gf_fseek(period_mpd, 0, SEEK_END);
	xml_size = (u32) gf_ftell(period_mpd);
	gf_fseek(period_mpd, 0, SEEK_SET);

	while (xml_size) {
		char buf[4096];
		u32 read;
		u32 size = 4096;
		if (xml_size<4096) size = xml_size;
		read = (s32) fread(buf, 1, size, period_mpd);
		if (read != (s32) size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Error reading from period MPD file: got %d but requested %d bytes\n", read, size ));
			gf_fclose(period_mpd);
			return GF_IO_ERR;
		}

		fwrite(buf, 1, size, mpd);
		xml_size -= size;
	}
	gf_fclose(period_mpd);
	//we cannot delet the file because we don't know if we will be called again with a new period, in which case we need to reinsert this XML ...
	return GF_OK;
}

static void purge_dash_context(GF_Config *dash_ctx)
{
	u32 i, count;
	//purge dash context
	count = gf_cfg_get_section_count(dash_ctx);
	for (i=0; i<count; i++) {
		const char *opt = gf_cfg_get_section_name(dash_ctx, i);
		if (!opt) continue;
		if( !strnicmp(opt, "Representation_", 15) || !strcmp(opt, "SegmentsStartTimes")) {
			gf_cfg_del_section(dash_ctx, opt);
			count--;
			i--;
		}
	}
}

typedef struct
{
	char szPeriodXML[GF_MAX_PATH];
	const char *xlink;
	Double period_duration;
	Bool is_xlink;
	u32 period_idx;
	const char *id;
} PeriodEntry;

GF_EXPORT
GF_DASHSegmenter *gf_dasher_new(const char *mpdName, GF_DashProfile dash_profile, const char *tmp_dir, u32 dash_timescale, GF_Config *dasher_context_file)
{
	GF_DASHSegmenter *dasher;
	GF_SAFEALLOC(dasher, GF_DASHSegmenter);
	if (!dasher) return NULL;

	dasher->mpd_name = gf_strdup(mpdName);
	dasher->mpd = gf_mpd_new();
	dasher->mpd->periods = gf_list_new();
	dasher->mpd->attributes = gf_list_new();
	dasher->mpd->children = gf_list_new();
	dasher->mpd->program_infos = gf_list_new();
	dasher->mpd->base_URLs = gf_list_new();
	dasher->mpd->locations = gf_list_new();
	dasher->mpd->metrics = gf_list_new();

	dasher->dash_scale = dash_timescale ? dash_timescale : 1000;
	if (tmp_dir) dasher->tmpdir = gf_strdup(tmp_dir);
	dasher->profile = dash_profile;
	dasher->dash_ctx = dasher_context_file;
	dasher->mpd_header_is_init = GF_FALSE;

	return dasher;
}

GF_EXPORT
void gf_dasher_set_start_date(GF_DASHSegmenter *dasher, u64 dash_utc_start_date)
{
	if (dash_utc_start_date) {
		Double ms;
		u64 secs = dash_utc_start_date/1000;
		dasher->start_date_sec_ntp = (u32) secs;
		dasher->start_date_sec_ntp += GF_NTP_SEC_1900_TO_1970;

		ms = (Double) (dash_utc_start_date - secs*1000);
		ms /= 1000.0;
		ms *= 0xFFFFFFFF;
		dasher->start_date_sec_ntp_ms_frac = (u32) ms;
	} else {
		dasher->start_date_sec_ntp = 0;
		dasher->start_date_sec_ntp_ms_frac = 0;
	}
}

GF_EXPORT
void gf_dasher_clean_inputs(GF_DASHSegmenter *dasher)
{
	u32 i, j;
	for (i=0; i < dasher->nb_inputs; i++) {
		for (j = 0; j < dasher->inputs[i].nb_components; j++) {
			if (dasher->inputs[i].components[j].lang) {
				gf_free(dasher->inputs[i].components[j].lang);
			}
		}
		if (dasher->inputs[i].dependencyID) gf_free(dasher->inputs[i].dependencyID);
		if (dasher->inputs[i].init_seg_url) gf_free(dasher->inputs[i].init_seg_url);
		if (dasher->inputs[i].period_id_not_specified && dasher->inputs[i].periodID) gf_free(dasher->inputs[i].periodID);

		if (dasher->inputs[i].isobmf_input) {
			//we don't want to save any modif due to duration adjustments
			gf_isom_delete(dasher->inputs[i].isobmf_input);
		}
	}
	gf_free(dasher->inputs);
	dasher->inputs = NULL;
	dasher->nb_inputs = 0;
}

GF_EXPORT
void gf_dasher_del(GF_DASHSegmenter *dasher)
{
	if (dasher->seg_rad_name) gf_free(dasher->seg_rad_name);
	gf_dasher_clean_inputs(dasher);
	gf_mpd_del(dasher->mpd);
	gf_free(dasher->tmpdir);
	gf_free(dasher->mpd_name);
	gf_free(dasher->m3u8_name);
	gf_free(dasher);
}

GF_EXPORT
GF_Err gf_dasher_set_info(GF_DASHSegmenter *dasher, const char *title, const char *copyright, const char *moreInfoURL, const char *sourceInfo, const char *lang)
{
	GF_MPD_ProgramInfo *info;
	if (!dasher) return GF_BAD_PARAM;
	GF_SAFEALLOC(info, GF_MPD_ProgramInfo);
	gf_list_add(dasher->mpd->program_infos, info);	 
	if (title) info->title = gf_strdup(title);
	else {
		char tmp[256];
		sprintf(tmp,"%s generated by GPAC",dasher->mpd_name);
		info->title=gf_strdup(tmp);
	}
	if (copyright) info->copyright = gf_strdup(copyright);
	if (moreInfoURL) info->more_info_url = gf_strdup(moreInfoURL);
	else info->more_info_url = gf_strdup("http://gpac.io");
	if (sourceInfo) info->source = gf_strdup(sourceInfo);
	if (lang) info->lang = gf_strdup(lang);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_test_mode(GF_DASHSegmenter *dasher, Bool forceTestMode){
	dasher->force_test_mode=forceTestMode;
	dasher->mpd->force_test_mode=forceTestMode;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_location(GF_DASHSegmenter *dasher, const char *location)
{
	if (!dasher) return GF_BAD_PARAM;
	if (location) gf_list_add(dasher->mpd->locations,gf_strdup(location));
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_add_base_url(GF_DASHSegmenter *dasher, const char *base_url)
{
	GF_MPD_BaseURL *base;
	if (!dasher) return GF_BAD_PARAM;
	GF_SAFEALLOC(base, GF_MPD_BaseURL);
	gf_list_add(dasher->mpd->base_URLs, base);
	base->URL = gf_strdup(base_url);
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
GF_Err gf_dasher_set_durations(GF_DASHSegmenter *dasher, Double default_segment_duration, Bool segment_duration_strict, Double default_fragment_duration)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->segment_duration = default_segment_duration * 1000 / dasher->dash_scale;
	dasher->segment_duration_strict = segment_duration_strict;
	if (default_fragment_duration)
		dasher->fragment_duration = default_fragment_duration * 1000 / dasher->dash_scale;
	else
		dasher->fragment_duration = dasher->segment_duration;
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
	if (dasher->dash_mode == GF_DASH_STATIC) {
		dasher->mpd->type = GF_MPD_TYPE_STATIC;
	} else {
		dasher->mpd->type = GF_MPD_TYPE_DYNAMIC;
	}
	dasher->mpd->minimum_update_period = (u32)mpd_update_time;
	dasher->mpd->time_shift_buffer_depth = time_shift_depth;
	dasher->mpd_live_duration = mpd_live_duration;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dasher_set_min_buffer(GF_DASHSegmenter *dasher, Double min_buffer)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->mpd->min_buffer_time = (u32)(min_buffer*1000);
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
GF_Err gf_dasher_configure_isobmf_default(GF_DASHSegmenter *dasher, Bool no_fragments_defaults, Bool pssh_moof, Bool samplegroups_in_traf, Bool single_traf_per_moof)
{
	if (!dasher) return GF_BAD_PARAM;
	dasher->no_fragments_defaults = no_fragments_defaults;
	dasher->pssh_moof = pssh_moof;
	dasher->samplegroups_in_traf = samplegroups_in_traf;
	dasher->single_traf_per_moof = single_traf_per_moof;
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


static void dash_input_check_period_id(GF_DASHSegmenter *dasher, GF_DashSegInput *dash_input)
{
	if (dash_input->period_id_not_specified) {
		if (dash_input->periodID) gf_free(dash_input->periodID);
		dash_input->periodID = NULL;
		dash_input->period_id_not_specified = 0;
	}

	if (! dash_input->periodID) {
		dash_input->period_id_not_specified = 1;
		//assign ID if dynamic - if dash_ctx also assign ID since we could have moved from dynamic to static
		if ((dasher->dash_mode!=GF_DASH_STATIC) || dasher->dash_ctx) {
			char szPName[50];
			sprintf(szPName, "DID1");
			if (dasher->dash_ctx) {
				while (1) {
					const char *p = gf_cfg_get_key(dasher->dash_ctx, "PastPeriods", szPName);
					if (!p) break;
					dash_input->period_id_not_specified++;
					sprintf(szPName, "DID%d", dash_input->period_id_not_specified);
				}
			}
			dash_input->periodID = gf_strdup(szPName);
		}
	}
}

GF_EXPORT
GF_Err gf_dasher_add_input(GF_DASHSegmenter *dasher, GF_DashSegmenterInput *input)
{
	GF_Err e;
	GF_DashSegInput *dash_input;
	if (!dasher) return GF_BAD_PARAM;

	dasher->inputs = gf_realloc(dasher->inputs, sizeof(GF_DashSegInput) * (dasher->nb_inputs+1));
	dash_input = &dasher->inputs[dasher->nb_inputs];
	dasher->nb_inputs++;
	memset(dash_input, 0, sizeof(GF_DashSegInput));

	dash_input->file_name = input->file_name;
	if (input->representationID)
		strcpy(dash_input->representationID, input->representationID);

	dash_input->periodID = input->periodID;
	dash_input->media_duration = input->media_duration;
	dash_input->nb_baseURL = input->nb_baseURL;
	dash_input->baseURL = input->baseURL;
	dash_input->xlink = input->xlink;
	dash_input->nb_roles = input->nb_roles;
	dash_input->roles = input->roles;

	dash_input->nb_rep_descs = input->nb_rep_descs;
	dash_input->rep_descs = input->rep_descs;
	dash_input->nb_as_descs = input->nb_as_descs;
	dash_input->as_descs = input->as_descs;
	dash_input->nb_as_c_descs = input->nb_as_c_descs;
	dash_input->as_c_descs = input->as_c_descs;
	dash_input->nb_p_descs = input->nb_p_descs;
	dash_input->p_descs = input->p_descs;
	dash_input->no_cache = dasher->no_cache;

	dash_input->bandwidth = input->bandwidth;

	if (!strcmp(dash_input->file_name, "NULL") || !strcmp(dash_input->file_name, "") || !dash_input->file_name) {
		if (!strcmp(dash_input->xlink, "")) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No input file specified and no xlink set - cannot dash\n"));
			dasher->nb_inputs --;
			return GF_BAD_PARAM;
		}
		dash_input->duration = input->period_duration;
	} else {
		dash_input->segment_duration = input->period_duration;

		e = gf_dash_segmenter_probe_input(&dasher->inputs, &dasher->nb_inputs, dasher->nb_inputs-1);
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Cannot open file %s for dashing: %s\n", dash_input->file_name, gf_error_to_string(e) ));
			dasher->nb_inputs --;
			return e;
		}
	}
	return GF_OK;
}

static const char *role_default = "main";

GF_EXPORT
GF_Err gf_dasher_process(GF_DASHSegmenter *dasher, Double sub_duration)
{
	u32 i, j, max_period, cur_period;
	Bool has_role = GF_FALSE;
       char *sep, szSolvedSegName[GF_MAX_PATH], szTempMPD[GF_MAX_PATH], szOpt[GF_MAX_PATH];
	const char *opt;
	GF_Err e;
	Bool uses_xlink = GF_FALSE;
	Bool has_mpeg2 = GF_FALSE;
	Bool none_supported = GF_TRUE;
	u32 valid_inputs = 0;
	u32 cur_adaptation_set;
	u32 max_adaptation_set = 0;
	u32 cur_group_id = 0;
	u32 max_sap_type = 0;
	u32 nb_sap_type_greater_than_3 = 0;
	u32 nb_sap_type_greater_than_4 = 0;
	u32 max_comp_per_input;
	Bool use_cenc = GF_FALSE;
	Bool segment_alignment = GF_TRUE;
	GF_List *period_links = NULL;
	GF_MPD_Period *period_obj;
	Double presentation_duration = 0;
	Double active_period_start = 0;
	u32 last_period_rep_idx_plus_one = 0;
	PeriodEntry *p;
	if (!dasher) return GF_BAD_PARAM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Dashing starting\n"));

	dasher->force_period_end = GF_FALSE;

	if (dasher->dash_mode && !dasher->mpd->minimum_update_period && !dasher->mpd_live_duration) {
		if (dasher->dash_mode == GF_DASH_DYNAMIC_LAST) {
			dasher->force_period_end = GF_TRUE;
			dasher->dash_mode = GF_DASH_DYNAMIC;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Either MPD refresh (update) period or MPD duration shall be set in dynamic mode.\n"));
			return GF_BAD_PARAM;
		}
	}
	if (dasher->real_time && (dasher->nb_inputs>1)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] real-time simulation is only supported with a single representation.\n"));
		return GF_BAD_PARAM;
	}


	/*update dash context*/
	if (dasher->dash_ctx) {
		Bool regenerate;
		e = gf_dasher_init_context(dasher, &dasher->dash_mode, &dasher->mpd->time_shift_buffer_depth, NULL, dasher->ast_offset_ms);
		if (e) return e;
		opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration");
		if (opt) {
			Double seg_dur = atof(opt);
			dasher->mpd->max_segment_duration = (u32)seg_dur;
		} else {
			sprintf(szOpt, "%f", dasher->segment_duration);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration", szOpt);
		}
		sprintf(szOpt, "%u", dasher->dash_scale);
		gf_cfg_set_key(dasher->dash_ctx, "DASH", "TimeScale", szOpt);

		/*peform all file cleanup*/
		regenerate = gf_dasher_cleanup(dasher);
		if (!regenerate) return GF_OK;

		opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "HasXLINK");
		if (opt && !strcmp(opt, "yes")) uses_xlink = GF_TRUE;
	}

	max_period = 0;

	// Check if at least one input is valid
	// Also checks if some general features are used (xlink, mpeg-2, roles ...)
	for (i=0; i<dasher->nb_inputs; i++) {
		u32 r;
		GF_DashSegInput *dash_input = &dasher->inputs[i];
		dash_input->period = 0;
		dash_input_check_period_id(dasher, dash_input);

		dash_input->moof_seqnum_increase = dash_input->nb_representations;
		if (!dash_input->moof_seqnum_increase) dash_input->moof_seqnum_increase=1;

		/*layered coded representation*/
		if (dash_input->idx_representations) {
			continue;
		}

		if (dash_input->xlink) uses_xlink = GF_TRUE;

		for (r=0; r<dash_input->nb_roles; r++) {
			if (dash_input->roles[r] && strcmp(dash_input->roles[r], "main")) {
				has_role = GF_TRUE;
				break;
			}
		}

		if (dash_input->period_id_not_specified) {
			max_period = 1;
		}

		if (!strcmp(dash_input->szMime, "video/mp2t")) has_mpeg2 = GF_TRUE;
		valid_inputs++;
	}
	if (!valid_inputs) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error: no suitable file found for dashing.\n"));
		return GF_BAD_PARAM;
	}

	//assign periods for each input file
	for (i=0; i<dasher->nb_inputs; i++) {
		GF_DashSegInput *dash_input = &dasher->inputs[i];

		/*set all default roles to main if needed*/
		if (has_role && !dash_input->roles) {
			dash_input->roles = (char**) &role_default;
			dash_input->nb_roles = 1;
		}
		//reset adaptation classifier
		dash_input->adaptation_set = 0;

		if (dash_input->period)
			continue;

		if (dash_input->period_id_not_specified) {
			dash_input->period = 1;
		}
		else if (dash_input->periodID) {
			max_period++;
			dash_input->period = max_period;

			for (j=i+1; j<dasher->nb_inputs; j++) {
				if (!strcmp(dasher->inputs[j].periodID, dasher->inputs[i].periodID))
					dasher->inputs[j].period = dasher->inputs[i].period;
			}
		}
	}
	if (dasher->dash_mode && (max_period > 1) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Multiple periods in live mode not supported\n"));
		return GF_NOT_SUPPORTED;
	}

	// assign adaptation sets within a period for each input file
	for (cur_period=0; cur_period<max_period; cur_period++) {
		Double seg_duration_in_period = 0;
		/*classify all input in possible adaptation sets*/
		for (i=0; i<dasher->nb_inputs; i++) {
			GF_DashSegInput *dash_input = &dasher->inputs[i];
			/*this file does not belong to our current period*/
			if (dash_input->period != cur_period+1) continue;

			if (!seg_duration_in_period) {
				seg_duration_in_period = dash_input->segment_duration ? dash_input->segment_duration : dasher->segment_duration;
			} else if (dash_input->segment_duration && (dash_input->segment_duration != seg_duration_in_period)) {
				segment_alignment = GF_FALSE;
			}

			/*this file already belongs to an adaptation set*/
			if (dash_input->adaptation_set) continue;

			/*we cannot fragment the input file*/
			if (!dash_input->dasher_input_classify || !dash_input->dasher_segment_file) {
				//this is a remote period insertion
				if (dash_input->xlink) {
					//assign a AS for this fake source (otherwise first active period detection will fail)
					dash_input->adaptation_set = 1;
					none_supported = GF_FALSE;
				}
				continue;
			}

			max_adaptation_set++;
			dash_input->adaptation_set = max_adaptation_set;
			dash_input->nb_rep_in_adaptation_set = 1;

			e = dash_input->dasher_input_classify(dasher->inputs, dasher->nb_inputs, i, &cur_group_id, &max_sap_type);
			if (e) return e;
			none_supported = GF_FALSE;
			if (max_sap_type>=3) nb_sap_type_greater_than_3++;
			if (max_sap_type>3) nb_sap_type_greater_than_4++;
		}
		if (none_supported) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] None of the input types are supported for DASHing - nothing to do ...\n"));
			return GF_OK;
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH classification done\n"));

	/*check requested profiles can be generated, or adjust them*/
	if (nb_sap_type_greater_than_4 || (nb_sap_type_greater_than_3 > 1)) {
		if (dasher->profile) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] WARNING! Max SAP type %d detected\n\tswitching to FULL profile\n", max_sap_type));
		}
		dasher->profile = GF_DASH_PROFILE_FULL;
	}

	switch (dasher->profile) {
	case GF_DASH_PROFILE_AVC264_LIVE:
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		if (dasher->cp_location_mode == GF_DASH_CPMODE_REPRESENTATION) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] ERROR! The selected DASH profile (DASH-IF IOP) requires the ContentProtection element to be present in the AdaptationSet element.\n"));
			e = GF_BAD_PARAM;
			goto exit;
		}
	default:
		break;
	}

	/*adjust params based on profiles*/
	switch (dasher->profile) {
	case GF_DASH_PROFILE_LIVE:
		dasher->segments_start_with_rap = GF_TRUE;
		dasher->use_url_template = 1;
		dasher->single_segment = dasher->single_file = GF_FALSE;
		break;
	case GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE: {
		Bool role_main_found=GF_FALSE;
		dasher->bitstream_switching_mode = GF_DASH_BSMODE_MULTIPLE_ENTRIES;
		for (i=0; i<dasher->nb_inputs; i++) {
			u32 r;
			for (r=0; r<dasher->inputs[i].nb_roles; r++) {
				if (!strcmp(dasher->inputs[i].roles[r], "main")) {
					role_main_found=GF_TRUE;
					break;
				}
			}
		}
		if (! role_main_found) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] HbbTV 1.5 ISO live profile requires to have at least one Adaptation Set\nlabelled with a Role@value of \"main\". Consider adding \":role=main\" to your inputs.\n"));
			e = GF_BAD_PARAM;
			goto exit;
		}
	}
	case GF_DASH_PROFILE_AVC264_LIVE:
		dasher->segments_start_with_rap = GF_TRUE;
		dasher->no_fragments_defaults = GF_TRUE;
		dasher->use_url_template = 1;
		dasher->single_segment = dasher->single_file = GF_FALSE;
		for (i=0; i<dasher->nb_inputs; i++) {
			if (strcmp(dasher->inputs[i].szMime, "video/mp4")) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment container %s forbidden in DASH-AVC/264 live profile.\n", dasher->inputs[i].szMime));
				e = GF_BAD_PARAM;
				goto exit;
			}
		}
		break;
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		dasher->segments_start_with_rap = GF_TRUE;
		dasher->no_fragments_defaults = GF_TRUE;
		if (dasher->seg_rad_name) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment-name not allowed in DASH-AVC/264 onDemand profile.\n"));
			return GF_BAD_PARAM;
		}
		for (i=0; i<dasher->nb_inputs; i++) {
			if (strcmp(dasher->inputs[i].szMime, "video/mp4")) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment container %s forbidden in DASH-AVC/264 onDemand profile.\n", dasher->inputs[i].szMime));
				e = GF_BAD_PARAM;
				goto exit;
			}
		}
	case GF_DASH_PROFILE_ONDEMAND:
		dasher->segments_start_with_rap = GF_TRUE;
		dasher->single_segment = GF_TRUE;
		if ((dasher->bitstream_switching_mode != GF_DASH_BSMODE_DEFAULT) && (dasher->bitstream_switching_mode != GF_DASH_BSMODE_NONE)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] onDemand profile, bitstream switching mode cannot be used.\n"));
			e = GF_BAD_PARAM;
			goto exit;
		}
		/*BS switching is meaningless in onDemand profile*/
		dasher->bitstream_switching_mode = GF_DASH_BSMODE_NONE;
		dasher->use_url_template = dasher->single_file = GF_FALSE;
		break;
	case GF_DASH_PROFILE_MAIN:
		dasher->segments_start_with_rap = GF_TRUE;
		dasher->single_segment = GF_FALSE;
		break;
	default:
		break;
	}
	if (dasher->bitstream_switching_mode == GF_DASH_BSMODE_DEFAULT) {
		dasher->bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
	}

	//we allow using inband param set when not time aligned. set the option now before overriding flags for non time aligned
	dasher->inband_param_set = ((dasher->bitstream_switching_mode == GF_DASH_BSMODE_INBAND) || (dasher->bitstream_switching_mode == GF_DASH_BSMODE_SINGLE) ) ? GF_TRUE : GF_FALSE;

	if (!segment_alignment) {
		if (dasher->profile) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segments are not time-aligned in each representation of each period\n\tswitching to FULL profile\n"));
			dasher->profile = GF_DASH_PROFILE_FULL;
		}
		if (dasher->bitstream_switching_mode != GF_DASH_BSMODE_NONE) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segments are not time-aligned in each representation of each period\n\tdisabling bitstream switching\n"));
			dasher->bitstream_switching_mode = GF_DASH_BSMODE_NONE;
		}
	}

	//check we have a segment template
	if (dasher->use_url_template && !dasher->seg_rad_name) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] DASH Live profile requested but no -segment-name - using \"%%s_dash\" by default\n"));
		dasher_format_seg_name(dasher, "%s_dash");
	}

	if (dasher->single_segment) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASH-ing file%s - single segment\nSubsegment duration %.3f - Fragment duration: %.3f secs\n", (dasher->nb_inputs>1) ? "s" : "", dasher->segment_duration, dasher->fragment_duration));
		dasher->subsegs_per_sidx = 0;
	} 
	else {
		if (!dasher->seg_ext) dasher->seg_ext = "m4s";

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASH-ing file%s: %.2fs segments %.2fs fragments ", (dasher->nb_inputs>1) ? "s" : "", dasher->segment_duration, dasher->fragment_duration));
		if (!dasher->enable_sidx) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("no sidx used"));
		} else if (dasher->subsegs_per_sidx) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("%d subsegments per sidx", dasher->subsegs_per_sidx));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("single sidx per segment"));
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\n"));
	}

	if (dasher->fragments_start_with_rap) dasher->segments_start_with_rap = GF_TRUE;

	if (dasher->segments_start_with_rap) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Spliting segments %sat GOP boundaries\n", dasher->fragments_start_with_rap ? "and fragments " : ""));
	}

	dasher->single_file_mode = (dasher->single_segment) ? 1 : (dasher->single_file ? 2 : 0);

	dasher->subduration = sub_duration * 1000 / dasher->dash_scale;

	// Get component information (type (audio/video/...), duration, audio params, video params) for each useful input 
	max_comp_per_input = 0;
	for (cur_period=0; cur_period<max_period; cur_period++) {
		u32 first_in_period = 0;
		u32 k;
		Double period_duration=0;
		Double min_media_duration = -1;
		Double max_media_duration = -1;
		Double max_audio_duration = -1;
		Double min_audio_duration = -1;

		for (i=0; i<dasher->nb_inputs; i++) {
			Double dur = 0;
			GF_DashSegInput *dash_input = &dasher->inputs[i];
			/*this file does not belongs to any adaptation set*/
			if (dash_input->period != cur_period+1) continue;

			/*this file does not belongs to any adaptation set*/
			if (!dash_input->adaptation_set) continue;

			if (!first_in_period) {
				first_in_period = i+1;
				last_period_rep_idx_plus_one = first_in_period;
			}

			//only get component info once
			if (!dash_input->get_component_info_done) {
				dash_input->get_component_info_done = GF_TRUE;
				if (dash_input->dasher_get_components_info) {
					e = dash_input->dasher_get_components_info(dash_input, dasher);
					if (e) return e;
				}
			}

			/*force media duration if requested by the author*/
			if (dash_input->media_duration) {
				dash_input->duration = dash_input->media_duration;
			}

			dur = dash_input->duration;

			for (k=0; k<dash_input->nb_components;k++) {
				Double cdur = dash_input->components[k].duration;
				if (dash_input->media_duration)
					cdur = dash_input->media_duration;
				if (!cdur)
					cdur = dur;

				if (dash_input->components[k].media_type == GF_ISOM_MEDIA_AUDIO) {
					if (min_audio_duration == -1 || min_audio_duration > cdur) {
						min_audio_duration = cdur;
					}
					if (max_audio_duration == -1 || max_audio_duration < cdur) {
						max_audio_duration = cdur;
					}
				}
			}

			if (sub_duration && (sub_duration < dur) ) {
				dur = dasher->subduration;
			}

			if (dur > period_duration)
				period_duration = dur;

			if (min_media_duration == -1 || min_media_duration > dur) {
				min_media_duration = dur;
			}
			if (max_media_duration == -1 || max_media_duration < dur) {
				max_media_duration = dur;
			}

			switch (dash_input->protection_scheme_type) {
			case GF_ISOM_CENC_SCHEME:
			case GF_ISOM_CBC_SCHEME:
			case GF_ISOM_CENS_SCHEME:
			case GF_ISOM_CBCS_SCHEME:
				use_cenc = GF_TRUE;
				break;
			}
			if (max_comp_per_input < dash_input->nb_components)
				max_comp_per_input = dash_input->nb_components;
		}
		if (first_in_period) {
			dasher->inputs[first_in_period-1].period_duration = period_duration;
		}
		presentation_duration += period_duration;
		if (max_media_duration - min_media_duration > dasher->segment_duration) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] The difference between the durations of the longest and shortest representations (%f) is higher than the segment duration (%f)\n", max_media_duration - min_media_duration, dasher->segment_duration));
		}

		if (! dasher->subduration)
			dasher->disable_loop = GF_TRUE;

		if (!dasher->disable_loop && dasher->subduration && (max_audio_duration>0)) {
			if (max_audio_duration != min_audio_duration) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Audio streams in the period have different durations (min %lf, max %lf), may result in bad synchronization\n", min_audio_duration, max_audio_duration));
			}
			for (i=0; i<dasher->nb_inputs; i++) {
				GF_DashSegInput *dash_input = &dasher->inputs[i];
				if (dash_input->period != cur_period+1) continue;
				if (!dash_input->adaptation_set) continue;
				dash_input->clamp_duration = max_audio_duration;

				if (dash_input->duration > max_audio_duration) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Input %s: max audio duration (%lf) in the period is less than duration (%lf), clamping will happen\n", dash_input->file_name, max_audio_duration, dash_input->media_duration ? dash_input->media_duration : dash_input->duration));
				}
			}
		}
	}

	if (max_comp_per_input > 1) {
		if (dasher->profile == GF_DASH_PROFILE_AVC264_LIVE) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Muxed representations not allowed in DASH-IF AVC264 live profile\n\tswitching to regular live profile\n"));
			dasher->profile = GF_DASH_PROFILE_LIVE;
		} else if (dasher->profile == GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Muxed representations not allowed in HbbTV 1.5 ISOBMF live profile\n\tswitching to regular live profile\n"));
			dasher->profile = GF_DASH_PROFILE_LIVE;
		} else if (dasher->profile == GF_DASH_PROFILE_AVC264_ONDEMAND) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Muxed representations not allowed in DASH-IF AVC264 onDemand profile\n\tswitching to regular onDemand profile\n"));
			dasher->profile = GF_DASH_PROFILE_ONDEMAND;
		}
	}

	/*HbbTV 1.5 ISO live specific checks*/
	if (dasher->profile == GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE) {
		if (max_adaptation_set > 16) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Max 16 adaptation sets in HbbTV 1.5 ISO live profile\n\tswitching to DASH AVC/264 live profile\n"));
			dasher->profile = GF_DASH_PROFILE_AVC264_LIVE;
		}
		if (max_period > 32) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Max 32 periods in HbbTV 1.5 ISO live profile\n\tswitching to regular DASH AVC/264 live profile\n"));
			dasher->profile = GF_DASH_PROFILE_AVC264_LIVE;
		}
		if (dasher->segment_duration < 1.0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Min segment duration 1s in HbbTV 1.5 ISO live profile\n\tcapping to 1s\n"));
			dasher->segment_duration = 1.0;
		}
		if (dasher->segment_duration > 15.0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Max segment duration 15s in HbbTV 1.5 ISO live profile\n\tcapping to 15s\n"));
			dasher->segment_duration = 15.0;
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH fetched component infos\n"));

	// Restoring DASH context
	active_period_start = 0;
	if (dasher->dash_ctx) {
		Double duration;
		char *id = "";
		if (last_period_rep_idx_plus_one) id = dasher->inputs[last_period_rep_idx_plus_one-1].periodID;
		if (!id) id = "";

		//restore max segment duration
		opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration");
		if (opt) dasher->mpd->max_segment_duration = (u32)atof(opt);

		opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastActivePeriod");
		//period has changed
		if (opt && strcmp(opt, id)) {
			Bool prev_period_not_done = GF_FALSE;
			const char *prev_id = opt;
			Double last_period_dur = 0;
			opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "CumulatedPastPeriodsDuration");
			duration = opt ? atof(opt) : 0.0;

			opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastPeriodDuration");
			if (opt) last_period_dur = atof(opt);

			if (!last_period_dur) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] last period has no known duration, cannot insert new period. Try re-dashing previous period and specifying its duration\n"));
				return GF_BAD_PARAM;
			}

			//we may have called the segmenter with the end of the previous active period and the start of the new one - locate the
			//previous period and update cumulated duration
			for (i=0; i<dasher->nb_inputs; i++) {
				char *p_id = dasher->inputs[i].periodID;
				if (!p_id) p_id = "";
				if (!strcmp(p_id, prev_id)) {
					last_period_dur += dasher->inputs[last_period_rep_idx_plus_one-1].period_duration;
					presentation_duration -= dasher->inputs[last_period_rep_idx_plus_one-1].period_duration;;
					prev_period_not_done = GF_TRUE;
					break;
				}
			}
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastPeriodDuration", NULL);

			duration += last_period_dur;
			presentation_duration += duration;

			sprintf(szOpt, "%g", duration);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "CumulatedPastPeriodsDuration", szOpt);

			//get active period start
			opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastActivePeriodStart");
			if (opt) active_period_start = atof(opt);
			active_period_start += last_period_dur;

			//and add period to past periods, storing their duration and xlink
			if (! prev_period_not_done) {
				opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastPeriodXLINK");
				sprintf(szOpt, "%g-%g,%s", last_period_dur, active_period_start-last_period_dur,  opt ? opt : "INLINE");
				gf_cfg_set_key(dasher->dash_ctx, "PastPeriods", prev_id, szOpt);
				if (opt) {
					gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastPeriodXLINK", NULL);
				}

				//update active period start
				sprintf(szOpt, "%g", active_period_start);
				gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastActivePeriodStart", szOpt);

				purge_dash_context(dasher->dash_ctx);
			}

			//and finally switch active period
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastActivePeriod", id);

			sprintf(szOpt, "%g", dasher->inputs[last_period_rep_idx_plus_one-1].period_duration);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastPeriodDuration", szOpt);
		} else {
			GF_DashSegInput *dash_input = &dasher->inputs[last_period_rep_idx_plus_one-1];
			opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastPeriodDuration");
			if (opt) {
				Double d = dash_input->duration;
				if (dash_input->clamp_duration) d = dash_input->clamp_duration;
				else if (dash_input->media_duration) d = dash_input->media_duration;

				duration = atof(opt);

				if (dash_input->period_duration + duration > d) {
					dash_input->period_duration = d - duration;
				}
				dash_input->period_duration += duration;
				presentation_duration += duration;
			}
			sprintf(szOpt, "%g", dash_input->period_duration);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastPeriodDuration", szOpt);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastActivePeriod", id);

			opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "CumulatedPastPeriodsDuration");
			presentation_duration += opt ? atof(opt) : 0.0;

			opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastActivePeriodStart");
			if (opt) active_period_start = atof(opt);
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH context restored\n"));
	}

	strcpy(szTempMPD, dasher->mpd_name);
	if (dasher->dash_mode) strcat(szTempMPD, ".tmp");

	dasher->mpd_file = gf_fopen(szTempMPD, "wt");
	if (!dasher->mpd_file) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MPD] Cannot open MPD file %s for writing\n", szTempMPD));
		return GF_IO_ERR;
	}

	/* The segmenter needs to take past periods into account when generating an MPD
	   but we don't want to reprocess those periods all the time
	   we serialize them once, the first time it is called and then simply copy/paste when the segmenter is called again */

	/* Building the list of past periods */
	period_links = gf_list_new();
	if (dasher->dash_ctx) {
		u32 count = gf_cfg_get_key_count(dasher->dash_ctx, "PastPeriods");
		for (i=0; i<count; i++) {
			const char *id = gf_cfg_get_key_name(dasher->dash_ctx, "PastPeriods", i);
			const char *xlink = gf_cfg_get_key(dasher->dash_ctx, "PastPeriods", id);
			char *sep = (char *)strchr(xlink, ',');
			if (!sep) continue;

			GF_SAFEALLOC(p, PeriodEntry);
			gf_list_add(period_links, p);
			p->id = id;

			sep[0] = 0;
			p->period_duration = (Double) atof(xlink);
			sep[0]=',';
			xlink = &sep[1];
			if (!strcmp(xlink, "INLINE")) {
				strcpy(p->szPeriodXML, dasher->mpd_name);
				sep = strrchr(p->szPeriodXML, '.');
				if (sep) sep[0] = 0;
				strcat(p->szPeriodXML, "-Period_");
				strcat(p->szPeriodXML, id);
				strcat(p->szPeriodXML, ".mpd");
			} else {
				char *url = gf_url_concatenate(dasher->mpd_name, xlink);
				strcpy(p->szPeriodXML, url ? url : xlink);
				if (url) gf_free(url);
				p->xlink = xlink;
				p->is_xlink = GF_TRUE;
			}
		}
	}

	// Check all periods and process segments for the active one
	for (cur_period=0; cur_period<max_period; cur_period++) {
		Bool flush_period = GF_FALSE;
		Double period_duration = 0;
		const char *id=NULL;
		const char *xlink = NULL;

		/* Find period information for this period */
		for (i=0; i< dasher->nb_inputs; i++) {
			if (dasher->inputs[i].adaptation_set && (dasher->inputs[i].period==cur_period+1)) {
				period_duration = dasher->inputs[i].period_duration;
				id = dasher->inputs[i].periodID;
				if (dasher->inputs[i].xlink)
					xlink = dasher->inputs[i].xlink;
				break;
			}
		}

		GF_SAFEALLOC(p, PeriodEntry);
		gf_list_add(period_links, p);
		p->period_duration = period_duration;
		p->period_idx = cur_period+1;
		p->id = id;

		if (xlink) {
			char *url = gf_url_concatenate(dasher->mpd_name, xlink);
			strcpy(p->szPeriodXML, url ? url : xlink);
			if (url) gf_free(url);
			p->xlink = xlink;
			p->is_xlink = GF_TRUE;
		} 
		else {
			strcpy(p->szPeriodXML, dasher->mpd_name);
			sep = strrchr(p->szPeriodXML, '.');
			if (sep) sep[0] = 0;
			strcat(p->szPeriodXML, "-Period_");
			strcat(p->szPeriodXML, id ? id : "");
			strcat(p->szPeriodXML, ".mpd");
		}

		//only open file if we are to dash something (max_adaptation_set=0: no source file, only period xlink insertion)
		if (max_adaptation_set) {
			dasher->period_mpd_file = gf_fopen(p->szPeriodXML, "wb");

			if (!dasher->period_mpd_file) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot open period MPD %s for writing, aborting\n", p->szPeriodXML));
				e = GF_IO_ERR;
				goto exit;
			}

			period_obj = gf_mpd_period_new();
			e = set_period_header(dasher, period_obj, id, active_period_start, period_duration, NULL, cur_period+1, (xlink!=NULL) ? GF_TRUE : GF_FALSE);
			if (e) goto exit;
		}
		else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] MPD uses only xlink periods (no local AdaptationSet)\n"));
		}
		//keep track of the last period xlink
		if (dasher->dash_ctx) {
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastPeriodXLINK", xlink);
		}

		/*for each identified adaptationSets, write MPD and perform segmentation of input files*/
		for (cur_adaptation_set=0; cur_adaptation_set < max_adaptation_set; cur_adaptation_set++) {
			GF_MPD_AdaptationSet *adaptation_set_obj;
			char szInit[GF_MAX_PATH], tmp[GF_MAX_PATH];
			u32 first_rep_in_set=0;
			u32 max_width = 0;
			u32 max_height = 0;
			u32 dar_num = 1;
			u32 dar_den = 1;
			u32 fps_num = 0;
			u32 fps_denum = 0;
			Double seg_duration_in_as = 0;
			Bool has_scalability = GF_FALSE;
			Bool use_bs_switching = (dasher->bitstream_switching_mode==GF_DASH_BSMODE_NONE) ? GF_FALSE : GF_TRUE;
			char *lang;
			char szFPS[100];
			Bool is_first_rep = GF_FALSE;
			Bool skip_init_segment_creation = GF_FALSE;

			dasher->segment_alignment_disabled = GF_FALSE;

			// find the first input that corresponds to the current adaptation set
			while (dasher->inputs[first_rep_in_set].adaptation_set != cur_adaptation_set + 1) {
				first_rep_in_set++;
			}

			/*not in our period*/
			if (dasher->inputs[first_rep_in_set].period != cur_period + 1) {
				continue;
			}

			if (dasher->inputs[first_rep_in_set].idx_representations) {
				skip_init_segment_creation = GF_TRUE;
			}

			if (!dasher->inputs[first_rep_in_set].init_segment_generated) {

				strcpy(tmp, dasher->mpd_name);
				sep = strrchr(tmp, '.');
				if (sep) sep[0] = 0;

				if (max_adaptation_set==1) {
					strcpy(szInit, tmp);
					strcat(szInit, "_init.mp4");
				} else {
					sprintf(szInit, "%s_set%d_init.mp4", tmp, cur_adaptation_set+1);
				}
				/*unless asked to do BS switching on single rep, don't do it ...*/
				if ((dasher->bitstream_switching_mode < GF_DASH_BSMODE_SINGLE)
				        && (dasher->inputs[first_rep_in_set].nb_rep_in_adaptation_set==1)
				        /*unless the representation is a scalable one split across several adaptation sets ...*/
				        && (dasher->inputs[first_rep_in_set].nb_representations<=1)
				   ) {
					use_bs_switching = GF_FALSE;
				}

				if (! use_bs_switching) {
					skip_init_segment_creation = GF_TRUE;
				}
				else if (! dasher->inputs[first_rep_in_set].dasher_create_init_segment) {
					skip_init_segment_creation = GF_TRUE;
					strcpy(szInit, "");
				}
				/*if we already wrote the InitializationSegment, get rid of it (no overwrite) and let the dashing routine open it*/
				else if (dasher->dash_ctx) {
					char RepSecName[GF_MAX_PATH];
					const char *init_seg_name;
					sprintf(RepSecName, "Representation_%s", dasher->inputs[first_rep_in_set].representationID);
					if ((init_seg_name = gf_cfg_get_key(dasher->dash_ctx, RepSecName, "InitializationSegment")) != NULL) {
						skip_init_segment_creation = GF_TRUE;
					}
				}

				if (!skip_init_segment_creation) {
					Bool disable_bs_switching = GF_FALSE;
					e = dasher->inputs[first_rep_in_set].dasher_create_init_segment(dasher->inputs, dasher->nb_inputs, cur_adaptation_set+1, szInit, dasher->tmpdir, dasher, dasher->bitstream_switching_mode, &disable_bs_switching);
					if (e) goto exit;
					if (disable_bs_switching) {
						use_bs_switching = GF_FALSE;
					}
				}

				dasher->inputs[first_rep_in_set].init_seg_url = use_bs_switching ? gf_strdup(szInit) : NULL;
				dasher->inputs[first_rep_in_set].use_bs_switching = use_bs_switching;
				dasher->inputs[first_rep_in_set].init_segment_generated = GF_TRUE;
			} 
			else {
				use_bs_switching = dasher->inputs[first_rep_in_set].use_bs_switching;
			}

			dasher->bs_switch_segment_file = use_bs_switching ? dasher->inputs[first_rep_in_set].init_seg_url : NULL;

			dasher->segment_alignment_disabled = GF_FALSE;
			szFPS[0] = 0;
			lang = NULL;
			/*compute some max defaults*/
			for (i=0; i<dasher->nb_inputs && !e; i++) {
				GF_DashSegInput *dash_input = &dasher->inputs[i];

				if (dash_input->adaptation_set != cur_adaptation_set + 1) {
					continue;
				}

				if (dash_input->idx_representations) {
					has_scalability = GF_TRUE;
				}

				for (j=0; j<dash_input->nb_components; j++) {
					struct _dash_component *comp = & dash_input->components[j];
					if (comp->width > max_width) {
						max_width = comp->width;
					}
					if (comp->height > max_height) {
						max_height = comp->height;
					}
					if (!fps_num || !fps_denum) {
						fps_num = comp->fps_num;
						fps_denum = comp->fps_denum;
					}
					else if (fps_num * comp->fps_denum < comp->fps_num * fps_denum) {
						fps_num = comp->fps_num;
						fps_denum = comp->fps_denum;
					}

					if (dar_num * comp->sar_denum * comp->height != dar_den * comp->sar_num * comp->width) {
						dar_num = comp->sar_num * comp->width;
						dar_den = comp->sar_denum * comp->height;
					}

					if (comp->lang) {
						if (lang && strcmp(lang, comp->lang) ) {
							if (!strcmp(lang, "und") || !strcmp(comp->lang, "lang")) {
								GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] two languages in adaptation set: %s will be kept %s will be ignored\n", lang, comp->lang));
							}
						} else if (!lang) {
							lang = gf_strdup(comp->lang);
						}
					}

					if (dash_input->segment_duration) {
						if (!seg_duration_in_as) {
							seg_duration_in_as = dash_input->segment_duration;
						}
						else if (dash_input->segment_duration != seg_duration_in_as) {
							dasher->segment_alignment_disabled = GF_TRUE;
						}
					}
				}
			}

			if (max_width && max_height) {
				gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);
				if (fps_denum > 1) {
					sprintf(szFPS, "%d/%d", fps_num, fps_denum);
				} 
				else if (fps_num) {
					sprintf(szFPS, "%d", fps_num);
				}
			}

			adaptation_set_obj = gf_mpd_adaptation_set_new();
			gf_list_add(period_obj->adaptation_sets, adaptation_set_obj);
			e = set_adaptation_header(adaptation_set_obj, dasher->profile, dasher->use_url_template, dasher->single_file_mode, dasher->inputs, dasher->nb_inputs, cur_period+1, cur_adaptation_set+1, first_rep_in_set,
			                            use_bs_switching, max_width, max_height, dar_num, dar_den, szFPS, lang, szInit, dasher->segment_alignment_disabled, dasher->mpd_name);
			gf_free(lang);

			if (e) goto exit;

			is_first_rep = GF_TRUE;
			for (i=0; i<dasher->nb_inputs && !e; i++) {
				char szOutName[GF_MAX_PATH], *segment_name, *orig_seg_name;
				GF_DashSegInput *dash_input = &dasher->inputs[i];
				Double segdur, fragdur;
				if (dash_input->adaptation_set != cur_adaptation_set + 1) {
					continue;
				}

				orig_seg_name = segment_name = dasher->seg_rad_name;

				strcpy(szOutName, gf_url_get_resource_name(dash_input->file_name));
				sep = strrchr(szOutName, '.');
				if (sep) sep[0] = 0;

				/*in scalable case: seg_name is variable*/
				dasher->variable_seg_rad_name = has_scalability ? GF_TRUE : GF_FALSE;
				if (segment_name) {
					if (strstr(segment_name, "%s")) {
						sprintf(szSolvedSegName, segment_name, szOutName);
						dasher->variable_seg_rad_name = GF_TRUE;
						if (dash_input->single_track_num) {
							char szTkNum[50];
							sprintf(szTkNum, "_track%d_", dash_input->single_track_num);
							strcat(szSolvedSegName, szTkNum);
						}
					} else {
						strcpy(szSolvedSegName, segment_name);
					}

					segment_name = szSolvedSegName;
				}

				/* user added a relative-path baseURL */
				if (dash_input->nb_baseURL) {
					if (gf_url_is_local(dash_input->baseURL[0])) {
						const char *name;
						char tmp_segment_name[GF_MAX_PATH];
						if (dash_input->nb_baseURL > 1)
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Several relative path baseURL for input %s: selecting \"%s\"\n", dash_input->file_name, dash_input->baseURL[0]));
						name = gf_url_get_resource_name(szOutName);
						strcpy(tmp_segment_name, name);
						gf_url_get_resource_path(dash_input->baseURL[0], szOutName);
						if (szOutName[strlen(szOutName)] != GF_PATH_SEPARATOR) {
							char ext[2];
							ext[0] = GF_PATH_SEPARATOR;
							ext[1] = 0;
							strcat(szOutName, ext);
						}
						strcat(szOutName, tmp_segment_name);
					} else {
						GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Found baseURL for input %s but not a relative path (%s)\n", dash_input->file_name, dash_input->baseURL[0]));
					}
				}

				if ((dash_input->trackNum || dash_input->single_track_num) && (!dasher->seg_rad_name || !strstr(dasher->seg_rad_name, "$RepresentationID$") ) ) {
					char tmp[20];
					sprintf(tmp, "_track%d", dash_input->trackNum ? dash_input->trackNum : dash_input->single_track_num);
					strcat(szOutName, tmp);
				}
				strcat(szOutName, "_dash");

				if (gf_url_get_resource_path(dasher->mpd_name, tmp)) {
					strcat(tmp, szOutName);
					strcpy(szOutName, tmp);
				}
				if (segment_name && dash_input->trackNum && !strstr(dasher->seg_rad_name, "$RepresentationID$") ) {
					char tmp[20];
					sprintf(tmp, "_track%d_", dash_input->trackNum);
					strcat(segment_name, tmp);
				}
				//dynamic loop of input creates same segment ID in new periods, force pid identification
				//to have different names in periods
				if (dash_input->period_id_not_specified>1) {
					char tmp[20];
					sprintf(tmp, "_p%d_", dash_input->period_id_not_specified);
					strcat(segment_name, tmp);
				}
				dasher->seg_rad_name = segment_name;

				/*in scalable case, we need also the bandwidth of dependent representation*/
				if (dash_input->dependencyID) {
					dash_input->dependency_bandwidth = gf_dash_get_dependency_bandwidth(dasher->inputs, dasher->nb_inputs, dash_input->file_name, dash_input->lower_layer_track);
				}

				segdur = dasher->segment_duration;
				fragdur = dasher->fragment_duration;

				if (dash_input->segment_duration)
					dasher->segment_duration = dash_input->segment_duration;

				if (dash_input->segment_duration && (dasher->fragment_duration * dasher->dash_scale > dash_input->segment_duration)) {
					dasher->fragment_duration = dasher->segment_duration;
				}


				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASHing file %s\n", dash_input->file_name));
				e = dash_input->dasher_segment_file(dash_input, szOutName, dasher, adaptation_set_obj, is_first_rep);

				dasher->seg_rad_name = orig_seg_name;
				dasher->segment_duration = segdur;
				dasher->fragment_duration = fragdur;

				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while DASH-ing file: %s\n", gf_error_to_string(e)));
					goto exit;
				}
				is_first_rep = GF_FALSE;
				//gf_mpd_period_free(period_obj);
			}
			/*close adaptation set*/
		}

		if (max_adaptation_set) {
			gf_mpd_print_period(period_obj, GF_TRUE, dasher->period_mpd_file);
			gf_mpd_period_free(period_obj);
			gf_fclose(dasher->period_mpd_file);
			period_obj = NULL;
		}

		//and add periods done to past periods, storing their start time
		if (!id) id="";
		if (last_period_rep_idx_plus_one && dasher->inputs[last_period_rep_idx_plus_one-1].periodID && strcmp(id, dasher->inputs[last_period_rep_idx_plus_one-1].periodID) ) {
			flush_period = GF_TRUE;
		}
		else if (dasher->force_period_end) {
			flush_period = GF_TRUE;
		}
		if (flush_period && dasher->dash_ctx) {
			//get largest duration
			u32 k, scount = gf_cfg_get_section_count(dasher->dash_ctx);
			for (k=0; k<scount; k++) {
				const char *sec = gf_cfg_get_section_name(dasher->dash_ctx, k);
				if (!sec) continue;
				if( !strnicmp(sec, "Representation_", 15) ) {
					const char *key = gf_cfg_get_key(dasher->dash_ctx, sec, "CumulatedDuration");
					if (key) {
						Double v = atof(key) / dasher->dash_scale;
						if (v>period_duration) period_duration = v;
					}
				}
			}

			sprintf(szOpt, "%g-%g", active_period_start, period_duration);
			gf_cfg_set_key(dasher->dash_ctx, "PastPeriods", id, szOpt);

			sprintf(szOpt, "%g", active_period_start);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastActivePeriodStart", szOpt);
			//also update last period duration with computed val
			sprintf(szOpt, "%g", period_duration);
			gf_cfg_set_key(dasher->dash_ctx, "DASH", "LastPeriodDuration", szOpt);

			if (dasher->dash_ctx) purge_dash_context(dasher->dash_ctx);
		}
	}

	if (dasher->dash_ctx) {
		sprintf(szOpt, "%g", dasher->mpd->max_segment_duration*1.0);
		gf_cfg_set_key(dasher->dash_ctx, "DASH", "MaxSegmentDuration", szOpt);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH Segment generation done\n"));


	/*ready to write the MPD*/
	dasher->mpd->media_presentation_duration = (u64)(presentation_duration*1000); // seconds to milliseconds

	if (dasher->dash_mode && !dasher->mpd->minimum_update_period) {
		presentation_duration = dasher->mpd_live_duration;
	}

	if (dasher->force_period_end && !dasher->mpd->minimum_update_period && !presentation_duration) {
		presentation_duration = 0;
		i=0;
		while ((p = (PeriodEntry *) gf_list_enum(period_links, &i))) {
			presentation_duration = p->period_duration;
		}
	}

	dasher->mpd->media_presentation_duration = (u64)(presentation_duration*1000);
	if(!dasher->mpd_header_is_init){
		e = mpd_set_header(dasher, has_mpeg2, presentation_duration, use_cenc, uses_xlink);
		if (e) goto exit;
		dasher->mpd_header_is_init=GF_TRUE;
	}


	i=0;
	while ((p = (PeriodEntry *) gf_list_enum(period_links, &i))) {

		if (p->is_xlink) {
			set_period_header(dasher, period_obj, p->id, 0.0, p->period_duration, p->xlink, p->period_idx, GF_FALSE);
		} else {
			GF_DOMParser *mpd_parser;
			GF_XMLNode *Xml_node;
			mpd_parser = gf_xml_dom_new();
			e = gf_xml_dom_parse(mpd_parser, p->szPeriodXML, NULL, NULL);
			Xml_node=gf_xml_dom_get_root(mpd_parser);
			if(!Xml_node)goto exit;
			gf_mpd_parse_period(dasher->mpd, Xml_node);
			gf_xml_dom_del(mpd_parser);
		}

		if (!p->period_idx) {
			if (!dasher->dash_ctx && !p->is_xlink) {
				gf_delete_file(p->szPeriodXML);
			}
		}
	}

	gf_mpd_write_file(dasher->mpd, szTempMPD);
       if(dasher->m3u8_name)
           gf_mpd_write_m3u8_file(dasher->mpd, dasher->m3u8_name);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH MPD done\n"));


	if (dasher->dash_ctx && dasher->dash_mode) {
		const char *opt = gf_cfg_get_key(dasher->dash_ctx, "DASH", "LastPeriodDuration");
		if (opt) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Current Period Duration: %s\n", opt) );
		}
	}

	if (dasher->profile == GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE) {
		if (gf_ftell(dasher->mpd_file) > 100*1024)
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[DASH] Manifest MPD is too big for HbbTV 1.5. Limit is 100kB, current size is "LLU"kB\n", gf_ftell(dasher->mpd_file)/1024));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Done dashing\n"));
	dasher->nb_secs_to_discard = 0;

exit:
	if (dasher->mpd) {
		gf_mpd_reset_periods(dasher->mpd);
	}
	if (dasher->mpd_file) {
		gf_fclose(dasher->mpd_file);
		if (!e && dasher->dash_mode) {
			gf_delete_file(dasher->mpd_name);
			e = gf_move_file(szTempMPD, dasher->mpd_name);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[DASH] Error moving file %s to %s: %s\n", szTempMPD, dasher->mpd_name, gf_error_to_string(e) ));
			}
		}
	}

	if (period_links) {
		while (gf_list_count(period_links)) {
			PeriodEntry *p = (PeriodEntry *) gf_list_pop_back(period_links);
			if (p) gf_free(p);
		}
		gf_list_del(period_links);
	}

	return e;
}

#endif /* GPAC_DISABLE_ISOM_WRITE */
