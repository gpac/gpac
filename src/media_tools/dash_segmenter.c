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

#ifdef GPAC_DISABLE_ISOM
/*we should need a better way to work with sidx when no isom is defined*/
#define GPAC_DISABLE_MPEG2TS
#endif


typedef struct _dash_segment_input GF_DashSegInput;

struct _dash_component
{
	u32 ID;/*audio/video/text/ ...*/
	u32 media_type;/*audio/video/text/ ...*/
	char szCodec[50];
	/*for video */
	u32 width, height, fps_num, fps_denum, sar_num, sar_denum, max_sap;

	/*for audio*/
	u32 sample_rate, channels;
	/*for anything*/
	char szLang[5];
};	

typedef struct
{
	Double segment_duration;
	Double subduration;
	Double fragment_duration;

	u32 dash_scale;

	Bool segments_start_with_rap;
	FILE *mpd;
	const char *mpd_name, *seg_rad_name, *seg_ext;
	s32 subsegs_per_sidx;
	Bool daisy_chain_sidx;
	u32 use_url_template;
	Bool use_segment_timeline;
	u32 single_file_mode;
	u32 segment_marker_4cc;
	s32 time_shift_depth;
	const char *bs_switch_segment_file;
	Bool inband_param_set;

	/*set if seg_rad_name depends on input file name (had %s in it). In this case, SegmentTemplate cannot be used at adaptation set level*/
	Bool variable_seg_rad_name;

	Bool fragments_start_with_rap;
	Bool memory_mode;
	Bool pssh_moof;

	GF_Config *dash_ctx;
	const char *tmpdir;
	u32 initial_moof_sn;
	u64 initial_tfdt;
	Bool no_fragments_defaults;
} GF_DASHSegmenterOptions;

struct _dash_segment_input
{
	char *file_name;
	char representationID[100];
	char periodID[100];
	char role[100];
	u32 bandwidth;
	u32 dependency_bandwidth;
	char dependencyID[100];

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
	GF_Err ( *dasher_get_components_info) (GF_DashSegInput *dash_input, GF_DASHSegmenterOptions *opts);
	GF_Err ( *dasher_create_init_segment) (GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 adaptation_set, char *szInitName, const char *tmpdir, GF_DASHSegmenterOptions *dash_opts, Bool *disable_bs_switching);
	GF_Err ( *dasher_segment_file) (GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenterOptions *opts, Bool first_in_set);

	/*shall be set after call to dasher_input_classify*/
	char szMime[50];

	u32 single_track_num;
	//information for scalability
	u32 trackNum, lower_layer_track, nb_representations, idx_representations;
	//increase sequence number between consecutive segments by this amount (for scalable reps)
	u32 moof_seqnum_increase;

	/*all these shall be set after call to dasher_get_components_info*/
	Double duration;
	struct _dash_component components[20];
	u32 nb_components;
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
GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *output_file_name, const char *rep_id, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number, Bool use_segment_timeline)
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

	if (seg_rad_name && (strstr(seg_rad_name, "$RepresentationID$") || strstr(seg_rad_name, "$ID$") || strstr(seg_rad_name, "$Bandwidth$"))) 
		needs_init = GF_FALSE;
	
	if (!seg_rad_name) {
		strcpy(segment_name, output_file_name);
	} else {

		while (1) {
			char c = seg_rad_name[char_template];
			if (!c) break;
			
			if (!is_template && !is_init_template 
				&& (!strnicmp(& seg_rad_name[char_template], "$RepresentationID$", 18) || !strnicmp(& seg_rad_name[char_template], "$ID$", 4))
			) {
				char_template+=18;
				strcat(segment_name, rep_id);
				needs_init=GF_FALSE;
			}
			else if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$Bandwidth", 10)) {
				EXTRACT_FORMAT(10);

				sprintf(tmp, szFmt, bandwidth);
				strcat(segment_name, tmp);
				needs_init=GF_FALSE;
			}
			else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Time", 5)) {
				EXTRACT_FORMAT(5);
				if (is_init || is_init_template) continue;
				/*replace %d to LLD*/
				szFmt[strlen(szFmt)-1]=0;
				strcat(szFmt, &LLD[1]);
				sprintf(tmp, szFmt, start_time);
				strcat(segment_name, tmp);
				has_number=1;
			}
			else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Number", 7)) {
				EXTRACT_FORMAT(7);

				if (is_init || is_init_template) continue;
				sprintf(tmp, szFmt, segment_number);
				strcat(segment_name, tmp);
				has_number=GF_TRUE;
			}
			else if (!strnicmp(& seg_rad_name[char_template], "$Init=", 6)) {
				char *sep = strchr(seg_rad_name + char_template+6, '$');
				if (sep) sep[0] = 0;
				if (is_init || is_init_template) {
					strcat(segment_name, seg_rad_name + char_template+6);
					needs_init=GF_FALSE;
				}
				char_template += (u32) strlen(seg_rad_name + char_template)+1;
				if (sep) sep[0] = '$';
			}
			else if (!strnicmp(& seg_rad_name[char_template], "$Index=", 7)) {
				char *sep = strchr(seg_rad_name + char_template+7, '$');
				if (sep) sep[0] = 0;
				if (is_index) {
					strcat(segment_name, seg_rad_name + char_template+6);
					needs_init=GF_FALSE;
				}
				char_template += (u32) strlen(seg_rad_name + char_template)+1;
				if (sep) sep[0] = '$';
			}
			
			else {
				char_template+=1;
				sprintf(tmp, "%c", c);
				strcat(segment_name, tmp);
			}
		}
	}

	if (is_template && ! use_segment_timeline && !strstr(seg_rad_name, "$Number"))
		strcat(segment_name, "$Number$");

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
	return GF_OK;
}

GF_Err gf_dasher_store_segment_info(GF_DASHSegmenterOptions *dash_cfg, const char *SegmentName, Double segStartTime)
{
	char szKey[512];
	if (!dash_cfg->dash_ctx) return GF_OK;

	sprintf(szKey, "%g", segStartTime);
	return gf_cfg_set_key(dash_cfg->dash_ctx, "SegmentsStartTimes", SegmentName, szKey);
}


#ifndef GPAC_DISABLE_ISOM

GF_Err gf_media_get_rfc_6381_codec_name(GF_ISOFile *movie, u32 track, char *szCodec)
{
	GF_ESD *esd;
	GF_AVCConfig *avcc;
#ifndef GPAC_DISABLE_HEVC
	GF_HEVCConfig *hvcc;
#endif

	u32 subtype = gf_isom_get_media_subtype(movie, track, 1);

	if (subtype==GF_ISOM_SUBTYPE_MPEG4_CRYP) {
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
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_AUDIO:
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				/*5 first bits of AAC config*/
				u8 audio_object_type = (esd->decoderConfig->decoderSpecificInfo->data[0] & 0xF8) >> 3;
				sprintf(szCodec, "mp4a.%02x.%01d", esd->decoderConfig->objectTypeIndication, audio_object_type);
			} else {
				sprintf(szCodec, "mp4a.%02x", esd->decoderConfig->objectTypeIndication);
			}
			break;
		case GF_STREAM_VISUAL:
#ifndef GPAC_DISABLE_AV_PARSERS
			if (esd->decoderConfig->decoderSpecificInfo) {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				sprintf(szCodec, "mp4v.%02x.%01x", esd->decoderConfig->objectTypeIndication, dsi.VideoPL);
			} else
#endif
			{
				sprintf(szCodec, "mp4v.%02x", esd->decoderConfig->objectTypeIndication);
			}
			break;
		default:
			sprintf(szCodec, "mp4s.%02x", esd->decoderConfig->objectTypeIndication);
			break;
		}
		gf_odf_desc_del((GF_Descriptor *)esd);
		return GF_OK;

	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
		avcc = gf_isom_avc_config_get(movie, track, 1);
		sprintf(szCodec, "%s.%02x%02x%02x", gf_4cc_to_str(subtype), avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
		gf_odf_avc_cfg_del(avcc);
		return GF_OK;
	case GF_ISOM_SUBTYPE_SVC_H264:
		avcc = gf_isom_svc_config_get(movie, track, 1);
		sprintf(szCodec, "%s.%02x%02x%02x", gf_4cc_to_str(subtype), avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
		gf_odf_avc_cfg_del(avcc);
		return GF_OK;
#ifndef GPAC_DISABLE_HEVC
	case GF_4CC('h','v','c','1'):
	case GF_4CC('h','e','v','1'):
		hvcc = gf_isom_hevc_config_get(movie, track, 1);
		if (hvcc) {
			u8 c;
			char szTemp[40];
			sprintf(szCodec, "%s.", gf_4cc_to_str(subtype));
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
					res <<= 1;
					val >>=1;
				}
				sprintf(szTemp, ".%x", res);
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
			sprintf(szTemp, ".%x", c);
			strcat(szCodec, szTemp);
			if (hvcc->constraint_indicator_flags & 0xFFFFFFFF) {
				c = (hvcc->constraint_indicator_flags >> 32) & 0xFF;
				sprintf(szTemp, ".%x", c);
				strcat(szCodec, szTemp);
				if (hvcc->constraint_indicator_flags & 0x00FFFFFF) {
					c = (hvcc->constraint_indicator_flags >> 24) & 0xFF;
					sprintf(szTemp, ".%x", c);
					strcat(szCodec, szTemp);
					if (hvcc->constraint_indicator_flags & 0x0000FFFF) {
						c = (hvcc->constraint_indicator_flags >> 16) & 0xFF;
						sprintf(szTemp, ".%x", c);
						strcat(szCodec, szTemp);
						if (hvcc->constraint_indicator_flags & 0x000000FF) {
							c = (hvcc->constraint_indicator_flags >> 8) & 0xFF;
							sprintf(szTemp, ".%x", c);
							strcat(szCodec, szTemp);
							c = (hvcc->constraint_indicator_flags ) & 0xFF;
							sprintf(szTemp, ".%x", c);
							strcat(szCodec, szTemp);
						}
					}
				}
			}
		
			gf_odf_hevc_cfg_del(hvcc);
		} else {
			sprintf(szCodec, "%s", gf_4cc_to_str(subtype));
		}
		return GF_OK;
#endif

	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUTHOR, ("[ISOM Tools] codec parameters not known - setting codecs string to default value \"%s\"\n", gf_4cc_to_str(subtype) ));
		sprintf(szCodec, "%s", gf_4cc_to_str(subtype));
		return GF_OK;
	}
	return GF_OK;
}

#endif //GPAC_DISABLE_ISOM


#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

typedef struct
{
	Bool is_ref_track;
	Bool done;
	u32 TrackID;
	u32 SampleNum, SampleCount;
	u64 FragmentLength;
	u32 OriginalTrack;
	u32 finalSampleDescriptionIndex;
	u32 TimeScale, MediaType, DefaultDuration, InitialTSOffset;
	u64 last_sample_cts, next_sample_dts;
	Bool all_sample_raps, splitable;
	u32 split_sample_dts_shift;
	s32 media_time_to_pres_time_shift;
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
	if (!found_sample) return 0;
	samp = gf_isom_get_sample_info(input, track, found_sample, NULL, NULL);
	time = samp->DTS;
	gf_isom_sample_del(&samp);
	return time;
}

static GF_Err gf_media_isom_segment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec, GF_DASHSegmenterOptions *dash_cfg, GF_DashSegInput *dash_input, Bool first_in_set)
{
	u8 NbBits;
	u32 i, TrackNum, descIndex, j, count, nb_sync, ref_track_id, nb_tracks_done;
	u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess, nb_samp, nb_done;
	u32 nb_video, nb_audio, nb_text, nb_scene, mpd_timescale;
	u8 defaultPadding;
	u16 defaultDegradationPriority;
	GF_Err e;
	char sOpt[100], sKey[100];
	char szCodecs[200], szCodec[100];
	u32 cur_seg, fragment_index, max_sap_type;
	GF_ISOFile *output, *bs_switch_segment;
	GF_ISOSample *sample, *next;
	GF_List *fragmenters;
	u64 MaxFragmentDuration, MaxSegmentDuration, SegmentDuration, maxFragDurationOverSegment;
	u32 presentationTimeOffset = 0;
	Double segment_start_time, file_duration, period_duration, max_segment_duration;
	u32 nb_segments, width, height, sample_rate, nb_channels, sar_w, sar_h, fps_num, fps_denum, startNumber, startNumberRewind;
	char langCode[5];
	u32 index_start_range, index_end_range;
	Bool force_switch_segment = GF_FALSE;
	Bool switch_segment = GF_FALSE;
	Bool split_seg_at_rap = dash_cfg->segments_start_with_rap;
	Bool split_at_rap = GF_FALSE;
	Bool has_rap = GF_FALSE;
	Bool next_sample_rap = GF_FALSE;
	Bool flush_all_samples = GF_FALSE;
	Bool simulation_pass = GF_FALSE;
	Bool init_segment_deleted = GF_FALSE;
	Bool first_segment_in_timeline = GF_TRUE;
	u64 previous_segment_duration = 0;
	u32 segment_timeline_repeat_count = 0;
	//u64 last_ref_cts = 0;
	u64 start_range, end_range, file_size, init_seg_size, ref_track_first_dts, ref_track_next_cts;
	u32 tfref_timescale = 0;
	u32 bandwidth = 0;
	GF_ISOMTrackFragmenter *tf, *tfref;
	GF_BitStream *mpd_bs = NULL;
	GF_BitStream *mpd_timeline_bs = NULL;
	char szMPDTempLine[2048];
	char SegmentName[GF_MAX_PATH];
	char RepSecName[200];
	char RepURLsSecName[200];
	const char *opt;
	Double max_track_duration = 0;
	Bool bs_switching_is_output = GF_FALSE;
	Bool store_dash_params = GF_FALSE;
	Bool dash_moov_setup = GF_FALSE;
	Bool segments_start_with_sap = GF_TRUE;
	Bool first_sample_in_segment = GF_FALSE;
	u32 *segments_info = NULL;
	u32 nb_segments_info = 0;
	u32 protected_track = 0;
	Bool audio_only = GF_TRUE;
	Bool is_bs_switching = GF_FALSE;
	Bool use_url_template = dash_cfg->use_url_template;
	const char *seg_rad_name = dash_cfg->seg_rad_name;
	const char *seg_ext = dash_cfg->seg_ext;
	const char *bs_switching_segment_name = NULL;

	SegmentName[0] = 0;
	SegmentDuration = 0;
	nb_samp = 0;
	fragmenters = NULL;
	if (!seg_ext) seg_ext = "m4s";

	bs_switch_segment = NULL;
	if (dash_cfg->bs_switch_segment_file) {
		bs_switch_segment = gf_isom_open(dash_cfg->bs_switch_segment_file, GF_ISOM_OPEN_READ, NULL);
		if (bs_switch_segment) {
			bs_switching_segment_name = gf_url_get_resource_name(dash_cfg->bs_switch_segment_file);
			is_bs_switching = 1;
		}
	}
	
	sprintf(RepSecName, "Representation_%s", dash_input ? dash_input->representationID : "");
	sprintf(RepURLsSecName, "URLs_%s", dash_input ? dash_input->representationID : "");

	bandwidth = dash_input ? dash_input->bandwidth : 0;
	file_duration = 0;

	startNumber = 1;
	startNumberRewind = 0;

	//create output file
	/*need to precompute bandwidth*/
	if (!bandwidth && seg_rad_name && strstr(seg_rad_name, "$Bandwidth$")) {
		for (i=0; i<gf_isom_get_track_count(input); i++) {
			Double tk_dur = (Double) gf_isom_get_media_duration(input, i+1);
			tk_dur /= gf_isom_get_media_timescale(input, i+1);
			if (file_duration < tk_dur ) {
				file_duration = tk_dur;
			}
		}
		bandwidth = (u32) (gf_isom_get_file_size(input) / file_duration * 8);
	}

	if (dash_cfg->dash_ctx) {
		opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "Setup");
		if (!opt || stricmp(opt, "yes") ) {
			store_dash_params=GF_TRUE;
			gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "ID", dash_input->representationID);
		}
		/*we are in time shift enabled mode so segments will get destroyed, set the start number to the current segment 
		and restore presentationTimeOffset (cf below)*/
		if (!store_dash_params && (dash_cfg->time_shift_depth >= 0)) {
			opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "NextSegmentIndex");
			sscanf(opt, "%u", &startNumber);

			/*adjust the startNumber according to the timeShiftBuffer depth*/
			if ((dash_cfg->time_shift_depth>0) && (startNumber>(u32)dash_cfg->time_shift_depth) ) {
				startNumberRewind = dash_cfg->time_shift_depth;
			}
		}
	}

	opt = dash_cfg->dash_ctx ? gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "InitializationSegment") : NULL;
	if (opt) {
		output = gf_isom_open(opt, GF_ISOM_OPEN_CAT_FRAGMENTS, NULL);
		dash_moov_setup = 1;
	} else {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, dash_input->representationID, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0, dash_cfg->use_segment_timeline);
		output = gf_isom_open(SegmentName, GF_ISOM_OPEN_WRITE, NULL);
	}
	if (!output) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISOBMF DASH] Cannot open %s for writing\n", opt ? opt : SegmentName));
		e = gf_isom_last_error(NULL);
		goto err_exit;
	}

	if (store_dash_params) {
		const char *name;

		if (!gf_cfg_get_key(dash_cfg->dash_ctx, "DASH", "SegmentTemplate"))
			gf_cfg_set_key(dash_cfg->dash_ctx, "DASH", "SegmentTemplate", seg_rad_name);
		gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "Source", gf_isom_get_filename(input) );

		gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "Setup", "yes");
		name = SegmentName;
		if (bs_switch_segment) name = gf_isom_get_filename(bs_switch_segment);
		gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "InitializationSegment", name);

		/*store BS flag per rep - it should be stored per adaptation set but we dson't have a key for adaptation sets right now*/
		if (/*first_in_set && */ is_bs_switching)
			gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "BitstreamSwitching", "yes");
	} else if (dash_cfg->dash_ctx) {
		opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "BitstreamSwitching");
		if (opt && !strcmp(opt, "yes")) {
			is_bs_switching = 1;
			if (bs_switch_segment) 
				gf_isom_delete(bs_switch_segment);
			bs_switch_segment = output;
			bs_switching_is_output = 1;
			bs_switching_segment_name = gf_url_get_resource_name(gf_isom_get_filename(bs_switch_segment));
		}

		opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "Bandwidth");
		if (opt) sscanf(opt, "%u", &bandwidth);
	}
	mpd_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (first_in_set && dash_cfg->use_segment_timeline) {
		mpd_timeline_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		sprintf(szMPDTempLine, "    <SegmentTimeline>\n");
		gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
	}
		
	nb_sync = 0;
	nb_samp = 0;
	fragmenters = gf_list_new();

	if (! dash_moov_setup) {
		e = gf_isom_clone_movie(input, output, GF_FALSE, GF_FALSE, !dash_cfg->pssh_moof );
		if (e) goto err_exit;

		/*because of movie fragments MOOF based offset, ISOM <4 is forbidden*/
		gf_isom_set_brand_info(output, GF_4CC('i','s','o','5'), 1);
		gf_isom_modify_alternate_brand(output, GF_4CC('i','s','o','m'), 0);
		gf_isom_modify_alternate_brand(output, GF_4CC('i','s','o','1'), 0);
		gf_isom_modify_alternate_brand(output, GF_4CC('i','s','o','2'), 0);
		gf_isom_modify_alternate_brand(output, GF_4CC('i','s','o','3'), 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_MP41, 0);
		gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_MP42, 0);

	}

	MaxFragmentDuration = (u32) (dash_cfg->dash_scale * max_duration_sec);	
	MaxSegmentDuration = (u32) (dash_cfg->dash_scale * dash_cfg->segment_duration);

	/*in single segment mode, only one big SIDX is written between the end of the moov and the first fragment. 
	To speed-up writing, we do a first fragmentation pass without writing any sample to compute the number of segments and fragments per segment
	in order to allocate / write to file the sidx before the fragmentation. The sidx will then be rewritten when closing the last segment*/
	if (dash_cfg->single_file_mode==1) {
		simulation_pass = GF_TRUE;
		seg_rad_name = NULL;
	}
	/*if single file is requested, store all segments in the same file*/
	else if (dash_cfg->single_file_mode==2) {
		seg_rad_name = NULL;
	}

	index_start_range = index_end_range = 0;	

	tf = tfref = NULL;
	file_duration = 0;
	width = height = sample_rate = nb_channels = sar_w = sar_h = fps_num = fps_denum = 0;
	langCode[0]=0;
	langCode[4]=0;
	szCodecs[0] = 0;


	mpd_timescale = nb_video = nb_audio = nb_text = nb_scene = 0;
	//duplicates all tracks
	for (i=0; i<gf_isom_get_track_count(input); i++) {
		u32 _w, _h, _sr, _nb_ch, avctype;

		u32 mtype = gf_isom_get_media_type(input, i+1);
		if (mtype == GF_ISOM_MEDIA_HINT) continue;

		if (dash_input->trackNum && ((i+1) != dash_input->trackNum))
			continue;

		if (dash_input->single_track_num && ((i+1) != dash_input->single_track_num))
			continue;

		if (! dash_moov_setup) {
			e = gf_isom_clone_track(input, i+1, output, GF_FALSE, &TrackNum);
			if (e) goto err_exit;

			if (gf_isom_is_track_in_root_od(input, i+1)) gf_isom_add_track_to_root_od(output, TrackNum);

			//if only one sample, don't fragment track
			count = gf_isom_get_sample_count(input, i+1);
			if (count==1) {
				sample = gf_isom_get_sample(input, i+1, 1, &descIndex);
				e = gf_isom_add_sample(output, TrackNum, 1, sample);
				gf_isom_sample_del(&sample);
				if (e) goto err_exit;

				continue;
			}
		} else {
			TrackNum = gf_isom_get_track_by_id(output, gf_isom_get_track_id(input, i+1));
			count = gf_isom_get_sample_count(input, i+1);
		}

		/*set extraction mode whether setup or not*/
		avctype = gf_isom_get_avc_svc_type(input, i+1, 1);
		if (avctype==GF_ISOM_AVCTYPE_AVC_ONLY) {
			/*for AVC we concatenate SPS/PPS unless SVC base*/
			if (!dash_input->trackNum && dash_cfg->inband_param_set) 
				gf_isom_set_nalu_extract_mode(input, i+1, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		}
		else if (avctype > GF_ISOM_AVCTYPE_AVC_ONLY) {
			/*for SVC we don't want any rewrite of extractors, and we don't concatenate SPS/PPS*/
			gf_isom_set_nalu_extract_mode(input, i+1, GF_ISOM_NALU_EXTRACT_INSPECT);
		}

		if (dash_cfg->inband_param_set && (gf_isom_get_media_subtype(input, i+1, 1)==GF_ISOM_SUBTYPE_HVC1)) {
			gf_isom_set_nalu_extract_mode(input, i+1, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		}

		if (mtype == GF_ISOM_MEDIA_VISUAL) nb_video++;
		else if (mtype == GF_ISOM_MEDIA_AUDIO) nb_audio++;
		else if (mtype == GF_ISOM_MEDIA_TEXT) nb_text++;
		else if (mtype == GF_ISOM_MEDIA_SCENE) nb_scene++;
		else if (mtype == GF_ISOM_MEDIA_DIMS) nb_scene++;

		if (mtype != GF_ISOM_MEDIA_AUDIO) audio_only = GF_FALSE;

		//setup fragmenters
		if (! dash_moov_setup) {
				//new initialization segment, setup fragmentation
			gf_isom_get_fragment_defaults(input, i+1,
										&defaultDuration, &defaultSize, &defaultDescriptionIndex, 
										&defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
			if (! dash_cfg->no_fragments_defaults) {
				e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum),
							defaultDescriptionIndex, defaultDuration,
							defaultSize, (u8) defaultRandomAccess,
							defaultPadding, defaultDegradationPriority);

			} else {
				e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum), defaultDescriptionIndex, 0, 0, 0, 0, 0);
			}
			if (e) goto err_exit;
		} else {
			gf_isom_get_fragment_defaults(output, TrackNum,
									 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
		}

		gf_media_get_rfc_6381_codec_name(bs_switch_segment ? bs_switch_segment : input, i+1, szCodec);
		if (strlen(szCodecs)) strcat(szCodecs, ",");
		strcat(szCodecs, szCodec);

		GF_SAFEALLOC(tf, GF_ISOMTrackFragmenter);
		tf->TrackID = gf_isom_get_track_id(output, TrackNum);
		tf->SampleCount = gf_isom_get_sample_count(input, i+1);
		tf->OriginalTrack = i+1;
		tf->TimeScale = gf_isom_get_media_timescale(input, i+1);
		tf->MediaType = gf_isom_get_media_type(input, i+1);
		tf->DefaultDuration = defaultDuration;
		mpd_timescale = tf->TimeScale;

		if (max_track_duration < gf_isom_get_track_duration(input, i+1)) {
			max_track_duration = (Double) gf_isom_get_track_duration(input, i+1);
		}

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

			gf_isom_set_brand_info(output, GF_4CC('i','s','o','5'), 1);

			/*DASH self-init media segment*/
			if (dash_cfg->single_file_mode==1) {
				gf_isom_modify_alternate_brand(output, GF_4CC('d','s','m','s'), 1);
			} else {
				gf_isom_modify_alternate_brand(output, GF_4CC('d','a','s','h'), 1);
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
											 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
					if (e) goto err_exit;

					e = gf_isom_change_track_fragment_defaults(output, tf->TrackID,
											 defaultDescriptionIndex, defaultDuration, defaultSize, defaultRandomAccess, defaultPadding, defaultDegradationPriority);
					if (e) goto err_exit;
				}

				/*and search in new ones the new index*/
				s_count = gf_isom_get_sample_description_count(bs_switch_segment, sample_descs_track); 

				/*more than one sampleDesc, we will need to indicate all the sample descs in the file in case we produce a single file, 
				otherwise the file would not be compliant*/
				if (s_count > 1) {
					gf_isom_clone_sample_descriptions(output, TrackNum, bs_switch_segment, sample_descs_track, 1);
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
			opt = (char *)gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, sKey);
			if (opt) tf->InitialTSOffset = atoi(opt);

			/*in time shift enabled, restore presentationTimeOffset */
			if (dash_cfg->time_shift_depth>=0)
				presentationTimeOffset = tf->InitialTSOffset;
		}

		/*get language, width/height/layout info, audio info*/
		switch (mtype) {
		case GF_ISOM_MEDIA_TEXT:
			tf->splitable = 1;
			gf_isom_get_media_language(input, i+1, langCode);
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_DIMS:
			gf_isom_get_track_layout_info(input, i+1, &_w, &_h, NULL, NULL, NULL);
			if (_w>width) width = _w;
			if (_h>height) height = _h;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			gf_isom_get_audio_info(input, i+1, 1, &_sr, &_nb_ch, NULL);
			if (_sr>sample_rate) sample_rate=_sr;
			if (_nb_ch>nb_channels) nb_channels = _nb_ch;
			gf_isom_get_media_language(input, i+1, langCode);
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

		nb_samp += count;
	}

	if (gf_list_count(fragmenters)>1)
		mpd_timescale = 1000;

	if (!tfref)
		tfref = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, 0);
	else {
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
		max_track_duration /= gf_isom_get_timescale(input);
		max_track_duration *= gf_isom_get_timescale(output);
		gf_isom_set_movie_duration(output, (u64) max_track_duration);
	}

	//if single segment, add msix brand if we use indexes
	gf_isom_modify_alternate_brand(output, GF_4CC('m','s','i','x'), ((dash_cfg->single_file_mode==1) && (dash_cfg->subsegs_per_sidx>=0)) ? 1 : 0);

	//flush movie
	e = gf_isom_finalize_for_fragment(output, 1);
	if (e) goto err_exit;

	start_range = 0;
	file_size = gf_isom_get_file_size(bs_switch_segment ? bs_switch_segment : output);
	end_range = file_size - 1;
	init_seg_size = file_size;

	if (dash_cfg->dash_ctx) {
		if (store_dash_params) {
			char szVal[1024];
			sprintf(szVal, LLU, init_seg_size);
			gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "InitializationSegmentSize", szVal);
		} else {
			const char *opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "InitializationSegmentSize");
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

	maxFragDurationOverSegment=0;
	switch_segment=GF_TRUE;

	if (!seg_rad_name) use_url_template = GF_FALSE;

	cur_seg=1;
	fragment_index=1;
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
	if (dash_cfg->dash_ctx) {
		const char *opt;
		char sKey[100];
		count = gf_cfg_get_key_count(dash_cfg->dash_ctx, RepURLsSecName);
		for (i=0; i<count; i++) {
			const char *key_name = gf_cfg_get_key_name(dash_cfg->dash_ctx, RepURLsSecName, i);
			opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepURLsSecName, key_name);
			sprintf(szMPDTempLine, "     %s\n", opt);	
			gf_bs_write_data(mpd_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
		}

		opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "NextSegmentIndex");
		if (opt) cur_seg = atoi(opt);
		opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "NextFragmentIndex");
		if (opt) fragment_index = atoi(opt);
		opt = gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, "CumulatedDuration");
		if (opt) period_duration = atof(opt);

		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
			sprintf(sKey, "TKID_%d_NextSampleNum", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, sKey);
			if (opt) tf->SampleNum = atoi(opt);

			sprintf(sKey, "TKID_%d_LastSampleCTS", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, sKey);
			if (opt) sscanf(opt, LLU, &tf->last_sample_cts);

			sprintf(sKey, "TKID_%d_NextSampleDTS", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dash_cfg->dash_ctx, RepSecName, sKey);
			if (opt) {
				sscanf(opt, LLU, &tf->next_sample_dts);
			}
		}
	}

	gf_isom_set_next_moof_number(output, dash_cfg->initial_moof_sn + fragment_index);


	max_segment_duration = 0;

	while ( (count = gf_list_count(fragmenters)) ) {
		Bool store_pssh = GF_FALSE;
		if (switch_segment) {
			if (dash_cfg->subduration && (segment_start_time + MaxSegmentDuration/2 >= dash_cfg->dash_scale * dash_cfg->subduration)) {
				/*done with file (next segment will exceppe of more than half the requested subduration : store all fragmenters state and abord*/
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
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, is_bs_switching, SegmentName, output_file, dash_input->representationID, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, (u64) ( period_duration * dash_cfg->dash_scale + segment_start_time), bandwidth, cur_seg, dash_cfg->use_segment_timeline);
					e = gf_isom_start_segment(output, SegmentName, dash_cfg->memory_mode);

					gf_dasher_store_segment_info(dash_cfg, SegmentName, period_duration + segment_start_time / (Double) dash_cfg->dash_scale);

					/*we are in bitstream switching mode, delete init segment*/
					if (is_bs_switching && !init_segment_deleted) {
						init_segment_deleted = GF_TRUE;
						gf_delete_file(gf_isom_get_filename(output));
					}

					if (!use_url_template) {
						const char *name = gf_url_get_resource_name(SegmentName);
						sprintf(szMPDTempLine, "     <SegmentURL media=\"%s\"/>\n", name );	
						gf_bs_write_data(mpd_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
						if (dash_cfg->dash_ctx) {
							char szKey[100], szVal[4046];
							sprintf(szKey, "UrlInfo%d", cur_seg );
							sprintf(szVal, "<SegmentURL media=\"%s\"/>", name);
							gf_cfg_set_key(dash_cfg->dash_ctx, RepURLsSecName, szKey, szVal);
						}
					}
				} else {
					e = gf_isom_start_segment(output, NULL, dash_cfg->memory_mode);
				}
				if (dash_cfg->pssh_moof)
					store_pssh = GF_TRUE;
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
			e = gf_isom_start_fragment(output, 1);
			if (e) goto err_exit;
	

			for (i=0; i<count; i++) {
				tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
				if (tf->done) continue;

				if (dash_cfg->initial_tfdt && (tf->TimeScale != dash_cfg->dash_scale)) {
					Double scale = tf->TimeScale; scale /= dash_cfg->dash_scale;
					gf_isom_set_traf_base_media_decode_time(output, tf->TrackID, (u64) (dash_cfg->initial_tfdt*scale) + tf->InitialTSOffset + tf->next_sample_dts);
				} else {
					gf_isom_set_traf_base_media_decode_time(output, tf->TrackID, tf->InitialTSOffset + tf->next_sample_dts);
				}
			}
			if (store_pssh) {
				store_pssh = GF_FALSE;
				e = gf_isom_clone_pssh(output, input, GF_TRUE);
			}
		}

		//process track by track
		for (i=0; i<count; i++) {
			Bool has_roll, is_rap;
			s32 roll_distance;
			u32 SAP_type = 0;

			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
			if (tf == tfref) 
				has_rap = GF_FALSE;

			if (tf->done) continue;

			//ok write samples
			while (1) {
				Bool stop_frag = GF_FALSE;
				Bool is_redundant_sample = GF_FALSE;
				u32 split_sample_duration = 0;

				/*first sample*/
				if (!sample) {
					sample = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 1, &descIndex);
					if (!sample) {
						e = gf_isom_last_error(input);
						goto err_exit;
					}

					/*FIXME - use negative ctts to indicate "past" DTS for splitted sample*/
					if (tf->split_sample_dts_shift) {
						sample->DTS += tf->split_sample_dts_shift;
						is_redundant_sample = GF_TRUE;
					}

					/*also get SAP type - this is not needed if sample is not NULL as SAP tye was computed for "next sample" in previous loop*/
					if (sample->IsRAP) {
						SAP_type = 1;
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
				if (next) {
					defaultDuration = (u32) (next->DTS - sample->DTS);
				} else {
 					defaultDuration = (u32) (gf_isom_get_media_duration(input, tf->OriginalTrack)- sample->DTS);
				}

				if (tf->splitable) {
					if (tfref==tf) {
						u64 frag_dur = (tf->FragmentLength + defaultDuration) * dash_cfg->dash_scale / tf->TimeScale;
						/*if media segment about to be produced is longer than max segment length, force segment split*/
						if (SegmentDuration + frag_dur > MaxSegmentDuration) {
							split_sample_duration = defaultDuration;
							defaultDuration = (u32) (tf->TimeScale * (MaxSegmentDuration - SegmentDuration) / dash_cfg->dash_scale - tf->FragmentLength);
							split_sample_duration -= defaultDuration;
						}
					} else if (tfref /* tfref set to NULL after the last sample of tfref is processed */ 
								/*&& next do not split if no next sample */

								/*next sample DTS */
//								&& ((tf->next_sample_dts /*+ 1 accuracy*/  ) * tfref_timescale < tfref->next_sample_dts * tf->TimeScale)
								&& ((tf->next_sample_dts + defaultDuration) * tfref_timescale > tfref->next_sample_dts * tf->TimeScale)) {
						split_sample_duration = defaultDuration;
						defaultDuration = (u32) ( (tfref->next_sample_dts * tf->TimeScale)/tfref_timescale - tf->next_sample_dts );
						split_sample_duration -= defaultDuration;

						/*since we split this sample we have to stop fragmenting afterwards*/
						stop_frag = GF_TRUE;
					}
				}

				if (tf==tfref) {
					if (segments_start_with_sap && first_sample_in_segment ) {
						first_sample_in_segment = GF_FALSE;
						if (! SAP_type) segments_start_with_sap = GF_FALSE;
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
						else ref_track_next_cts += defaultDuration;
					}
				}

				if (SAP_type > max_sap_type) max_sap_type = SAP_type;

				if (simulation_pass) {
					e = GF_OK;
				} else {
					/*override descIndex with final index used in file*/
					descIndex = tf->finalSampleDescriptionIndex;
					e = gf_isom_fragment_add_sample(output, tf->TrackID, sample, descIndex,
									 defaultDuration, NbBits, 0, is_redundant_sample);
					if (e) 
						goto err_exit;

					e = gf_isom_fragment_add_sai(output, input, tf->TrackID, tf->SampleNum + 1);
					if (e) goto err_exit;

					/*copy subsample information*/
					e = gf_isom_fragment_copy_subsample(output, tf->TrackID, input, tf->OriginalTrack, tf->SampleNum + 1);
					if (e) 
						goto err_exit;

					gf_set_progress("ISO File Fragmenting", nb_done, nb_samp);
					nb_done++;
				}

				tf->last_sample_cts = sample->DTS + sample->CTS_Offset;
				tf->next_sample_dts = sample->DTS + defaultDuration;

				if (split_sample_duration) {
					gf_isom_sample_del(&next);
					sample->DTS += defaultDuration;
				} else {
					gf_isom_sample_del(&sample);
					sample = next;
					tf->SampleNum += 1;
					tf->split_sample_dts_shift = 0;
				}
				tf->FragmentLength += defaultDuration;

				/*compute SAP type*/
				if (sample) {
					if (sample->IsRAP) {
						SAP_type = 1;
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
					if (tf==tfref) {
						if (split_sample_duration) {
							stop_frag = GF_TRUE;
						}
						else if (split_seg_at_rap) {
							u64 next_sap_time;
							u64 frag_dur, next_dur;
							next_dur = gf_isom_get_sample_duration(input, tf->OriginalTrack, tf->SampleNum + 1);
							if (!next_dur) next_dur = defaultDuration;
							/*duration of fragment if we add this rap*/
							frag_dur = (tf->FragmentLength+next_dur)*dash_cfg->dash_scale / tf->TimeScale;
							next_sample_rap = GF_TRUE;
							next_sap_time = isom_get_next_sap_time(input, tf->OriginalTrack, tf->SampleCount, tf->SampleNum + 2);
							/*if no more SAP after this one, do not switch segment*/
							if (next_sap_time) {
								u32 scaler;
								/*this is the fragment duration from last sample added to next SAP*/
								frag_dur += (u32) (next_sap_time - tf->next_sample_dts - next_dur) * dash_cfg->dash_scale / tf->TimeScale;
								/*if media segment about to be produced is longer than max segment length, force segment split*/
								if (SegmentDuration + frag_dur > MaxSegmentDuration) {
									split_at_rap = GF_TRUE;
									/*force new segment*/
									force_switch_segment = GF_TRUE;
									stop_frag = GF_TRUE;
								}

								if (! tf->all_sample_raps) {
									/*if adding this SAP will result in stoping the fragment "soon" after it, stop now and start with SAP
									if all samples are RAPs, just stop fragment if we exceed the requested duration by adding the next sample
									otherwise, take 3 samples (should be refined of course)*/
									scaler = 3;
									if ( (tf->FragmentLength + scaler * next_dur) * dash_cfg->dash_scale >= MaxFragmentDuration * tf->TimeScale)
										stop_frag = GF_TRUE;
								}
							}
						} else if (!has_rap) {
							if (tf->FragmentLength == defaultDuration) has_rap = 2;
							else has_rap = 1;
						}
					} 
				} else {
					next_sample_rap = GF_FALSE;
				}

				if (tf->SampleNum==tf->SampleCount) {
					stop_frag = GF_TRUE;
				} else if (tf==tfref) {
					/*fragmenting on "clock" track: no drift control*/
					if (!dash_cfg->fragments_start_with_rap || ( (next && next->IsRAP) || split_at_rap) ) {
						if (tf->FragmentLength * dash_cfg->dash_scale >= MaxFragmentDuration * tf->TimeScale) {
							stop_frag = GF_TRUE;
						}
					}
				}
				/*do not abort fragment if ref track is done, put everything in the last fragment*/
				else if (!flush_all_samples) {
					/*fragmenting on "non-clock" track: drift control*/
					if ((tf->next_sample_dts /*+1 accuracy*/) * tfref_timescale >= ref_track_next_cts * tf->TimeScale)
						stop_frag = GF_TRUE;
				}

				if (stop_frag) {
					gf_isom_sample_del(&sample);
					sample = next = NULL;
					if (maxFragDurationOverSegment <= tf->FragmentLength * dash_cfg->dash_scale / tf->TimeScale) {
						maxFragDurationOverSegment = tf->FragmentLength * dash_cfg->dash_scale / tf->TimeScale;
					}
					tf->FragmentLength = 0;
					if (split_sample_duration)
						tf->split_sample_dts_shift += defaultDuration;

					//if (tf==tfref)
						//last_ref_cts = tf->last_sample_cts;

					break;
				}
			}

			if (tf->SampleNum==tf->SampleCount) {
				tf->done = GF_TRUE;
				nb_tracks_done++;
				if (tf == tfref) {
					tfref = NULL;
					flush_all_samples = GF_TRUE;
				}
			}
		}

		SegmentDuration += maxFragDurationOverSegment;
		maxFragDurationOverSegment=0;

		/*if no simulation and no SIDX is used, flush fragments as we write them*/
		if (!simulation_pass && (dash_cfg->subsegs_per_sidx<0) ) {
			e = gf_isom_flush_fragments(output, flush_all_samples ? GF_TRUE : GF_FALSE);
			if (e) goto err_exit;
		}


		/*next fragment will exceed segment length, abort fragment now (all samples RAPs)*/
		if (tfref && tfref->all_sample_raps && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
			force_switch_segment = GF_TRUE;
		}

		if (force_switch_segment || ((SegmentDuration >= MaxSegmentDuration) && (!split_seg_at_rap || next_sample_rap))) {

			if (mpd_timeline_bs) {

				if (previous_segment_duration == SegmentDuration) {
					segment_timeline_repeat_count ++;
				} else {
					if (previous_segment_duration) {
						if (segment_timeline_repeat_count) {
							sprintf(szMPDTempLine, " r=\"%d\"/>\n", segment_timeline_repeat_count);
						} else {
							sprintf(szMPDTempLine, "/>\n");
						}
						gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
					}
					previous_segment_duration = SegmentDuration;
					if (first_segment_in_timeline) {
						sprintf(szMPDTempLine, "     <S t=\""LLU"\" d=\""LLU"\"", (u64) (segment_start_time*dash_cfg->dash_scale), (u64) SegmentDuration );
						first_segment_in_timeline = GF_FALSE;
					} else {
						sprintf(szMPDTempLine, "     <S d=\""LLU"\"", (u64) SegmentDuration);
					}
					gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
					segment_timeline_repeat_count = 0;
				}	
			}

			segment_start_time += SegmentDuration;
			nb_segments++;
			if (max_segment_duration * dash_cfg->dash_scale <= SegmentDuration) {
				max_segment_duration = (Double) (s64) SegmentDuration;
				max_segment_duration /= dash_cfg->dash_scale;
			}
			force_switch_segment=GF_FALSE;
			switch_segment=GF_TRUE;
			SegmentDuration=GF_FALSE;
			split_at_rap = GF_FALSE;
			has_rap = GF_FALSE;
			/*restore fragment duration*/
			MaxFragmentDuration = (u32) (max_duration_sec * dash_cfg->dash_scale);

			if (!simulation_pass) {
				u64 idx_start_range, idx_end_range;
					

				gf_isom_close_segment(output, dash_cfg->subsegs_per_sidx, ref_track_id, ref_track_first_dts, tfref ? tfref->media_time_to_pres_time_shift : tf->media_time_to_pres_time_shift, ref_track_next_cts, dash_cfg->daisy_chain_sidx, flush_all_samples ? GF_TRUE : GF_FALSE, dash_cfg->segment_marker_4cc, &idx_start_range, &idx_end_range);

				//take care of scalable reps
				if (dash_input->moof_seqnum_increase) {
					u32 frag_index = gf_isom_get_next_moof_number(output) + dash_input->nb_representations * dash_input->moof_seqnum_increase;
					gf_isom_set_next_moof_number(output, dash_cfg->initial_moof_sn + frag_index);
				}

				ref_track_first_dts = (u64) -1;

				if (!seg_rad_name) {
					file_size = gf_isom_get_file_size(output);
					end_range = file_size - 1;
					if (dash_cfg->single_file_mode!=1) {
						sprintf(szMPDTempLine, "      <SegmentURL mediaRange=\""LLD"-"LLD"\"", start_range, end_range);	
						gf_bs_write_data(mpd_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
						if (idx_start_range || idx_end_range) {
							sprintf(szMPDTempLine, " indexRange=\""LLD"-"LLD"\"", idx_start_range, idx_end_range);	
							gf_bs_write_data(mpd_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
						}
						gf_bs_write_data(mpd_bs, "/>\n", 3);

						if (dash_cfg->dash_ctx) {
							char szKey[100], szVal[4046];
							sprintf(szKey, "UrlInfo%d", cur_seg );
							sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
							gf_cfg_set_key(dash_cfg->dash_ctx, RepURLsSecName, szKey, szVal);
						}
					}
				} else {
					file_size += gf_isom_get_file_size(output);
				}
			}

			/*next fragment will exceed segment length, abort fragment at next rap (possibly after MaxSegmentDuration)*/
			if (split_seg_at_rap && SegmentDuration && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
				if (!split_at_rap) {
					split_at_rap = GF_TRUE;
				}
			}
		}
		if (nb_tracks_done==count) break;
	}

	if (simulation_pass) {
		/*OK, we have all segments and frags per segments*/
		gf_isom_allocate_sidx(output, dash_cfg->subsegs_per_sidx, dash_cfg->daisy_chain_sidx, nb_segments_info, segments_info, &index_start_range, &index_end_range );
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

		/*do not update on last segment, we're likely to have a different last GOP*/
#if 0
		if (max_segment_duration * dash_cfg->dash_scale <= SegmentDuration) {
			max_segment_duration = (Double) (s64) SegmentDuration;
			max_segment_duration /= dash_cfg->dash_scale;
		}
#endif

		segment_start_time += SegmentDuration;

		gf_isom_close_segment(output, dash_cfg->subsegs_per_sidx, ref_track_id, ref_track_first_dts, tfref ? tfref->media_time_to_pres_time_shift : tf->media_time_to_pres_time_shift, ref_track_next_cts, dash_cfg->daisy_chain_sidx, 1, dash_cfg->segment_marker_4cc, &idx_start_range, &idx_end_range);
		nb_segments++;

		if (!seg_rad_name) {
			file_size = gf_isom_get_file_size(output);
			end_range = file_size - 1;
			if (dash_cfg->single_file_mode!=1) {
				sprintf(szMPDTempLine, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"", start_range, end_range);
				gf_bs_write_data(mpd_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
				if (idx_start_range || idx_end_range) {
					sprintf(szMPDTempLine, " indexRange=\""LLD"-"LLD"\"", idx_start_range, idx_end_range);
					gf_bs_write_data(mpd_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
				}
				gf_bs_write_data(mpd_bs, "/>\n", 3);

				if (dash_cfg->dash_ctx) {
					char szKey[100], szVal[4046];
					sprintf(szKey, "UrlInfo%d", cur_seg );
					sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
					gf_cfg_set_key(dash_cfg->dash_ctx, RepURLsSecName, szKey, szVal);
				}
			}
		} else {
			file_size += gf_isom_get_file_size(output);
		}
	}
	if (mpd_timeline_bs) {
		if (previous_segment_duration == SegmentDuration) {
			segment_timeline_repeat_count ++;
			sprintf(szMPDTempLine, " r=\"%d\"/>\n", segment_timeline_repeat_count);
			gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
		} else {
			if (previous_segment_duration) {
				if (segment_timeline_repeat_count) {
					sprintf(szMPDTempLine, " r=\"%d\"/>\n", segment_timeline_repeat_count);
				} else {
					sprintf(szMPDTempLine, "/>\n");
				}
				gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
			}
			if (SegmentDuration) {
				if (first_segment_in_timeline) {
					sprintf(szMPDTempLine, "     <S t=\""LLU"\" d=\""LLU"\"/>\n", (u64) (segment_start_time), (u64) SegmentDuration );
					first_segment_in_timeline = GF_FALSE;
				} else {
					sprintf(szMPDTempLine, "     <S d=\""LLU"\"/>\n", (u64) SegmentDuration);
				}
				gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
			}
		}

		sprintf(szMPDTempLine, "    </SegmentTimeline>\n");
		gf_bs_write_data(mpd_timeline_bs, szMPDTempLine, (u32) strlen(szMPDTempLine));
	}

	if (!bandwidth)
		bandwidth = (u32) (file_size * 8 / file_duration);
	
	bandwidth += dash_input->dependency_bandwidth;
	dash_input->bandwidth = bandwidth;

	if (use_url_template) {
		/*segment template does not depend on file name, write the template at the adaptationSet level*/
		if (!dash_cfg->variable_seg_rad_name && first_in_set) {
			const char *rad_name = gf_url_get_resource_name(seg_rad_name);
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, 0, 0, dash_cfg->use_segment_timeline);
			fprintf(dash_cfg->mpd, "   <SegmentTemplate timescale=\"%d\" media=\"%s\" startNumber=\"%d\"", mpd_timeline_bs ? dash_cfg->dash_scale : mpd_timescale, SegmentName, startNumber - startNumberRewind);	
			if (!mpd_timeline_bs) {
				fprintf(dash_cfg->mpd, " duration=\"%d\"", (u32) (max_segment_duration * mpd_timescale));
			}
			/*in BS switching we share the same IS for all reps*/
			if (is_bs_switching) {
				strcpy(SegmentName, bs_switching_segment_name);
			} else {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0, dash_cfg->use_segment_timeline);
			}
			fprintf(dash_cfg->mpd, " initialization=\"%s\"", SegmentName);
			if (presentationTimeOffset) 
				fprintf(dash_cfg->mpd, " presentationTimeOffset=\"%d\"", presentationTimeOffset);

			if (mpd_timeline_bs) {
				char *mpd_seg_info = NULL;
				u32 size;
				fprintf(dash_cfg->mpd, ">\n");

				gf_bs_get_content(mpd_timeline_bs, &mpd_seg_info, &size);
				gf_fwrite(mpd_seg_info, 1, size, dash_cfg->mpd);
				gf_free(mpd_seg_info);
				fprintf(dash_cfg->mpd, "   </SegmentTemplate>\n");
			} else {
				fprintf(dash_cfg->mpd, "/>\n");
			}
		}
		/*in BS switching we share the same IS for all reps, write the SegmentTemplate for the init segment*/
		else if (is_bs_switching && first_in_set) {
			fprintf(dash_cfg->mpd, "   <SegmentTemplate initialization=\"%s\"", bs_switching_segment_name);
			if (presentationTimeOffset) 
				fprintf(dash_cfg->mpd, " presentationTimeOffset=\"%d\"", presentationTimeOffset);

			if (mpd_timeline_bs) {
				char *mpd_seg_info = NULL;
				u32 size;
				fprintf(dash_cfg->mpd, ">\n");

				gf_bs_get_content(mpd_timeline_bs, &mpd_seg_info, &size);
				gf_fwrite(mpd_seg_info, 1, size, dash_cfg->mpd);
				gf_free(mpd_seg_info);
				fprintf(dash_cfg->mpd, "   </SegmentTemplate>\n");
			} else {
				fprintf(dash_cfg->mpd, "/>\n");
			}
		}
	}
	else if ((dash_cfg->single_file_mode!=1) && mpd_timeline_bs) {
		char *mpd_seg_info = NULL;
		u32 size;

		fprintf(dash_cfg->mpd, "   <SegmentList timescale=\"%d\">\n", dash_cfg->dash_scale);

		gf_bs_get_content(mpd_timeline_bs, &mpd_seg_info, &size);
		gf_fwrite(mpd_seg_info, 1, size, dash_cfg->mpd);
		gf_free(mpd_seg_info);
		fprintf(dash_cfg->mpd, "   </SegmentList>\n");
	}


	fprintf(dash_cfg->mpd, "   <Representation ");
	if (dash_input->representationID) fprintf(dash_cfg->mpd, "id=\"%s\"", dash_input->representationID);
	else fprintf(dash_cfg->mpd, "id=\"%p\"", output);
		
	fprintf(dash_cfg->mpd, " mimeType=\"%s/mp4\" codecs=\"%s\"", audio_only ? "audio" : "video", szCodecs);
	if (width && height) {
		fprintf(dash_cfg->mpd, " width=\"%d\" height=\"%d\"", width, height);

		/*this is a video track*/
		if (fps_num || fps_denum) {
			gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);
			if (fps_denum>1)
				fprintf(dash_cfg->mpd, " frameRate=\"%d/%d\"", fps_num, fps_denum);
			else
				fprintf(dash_cfg->mpd, " frameRate=\"%d\"", fps_num);

			if (!sar_w) sar_w = 1;
			if (!sar_h) sar_h = 1;
			fprintf(dash_cfg->mpd, " sar=\"%d:%d\"", sar_w, sar_h);
		}

	}
	if (sample_rate) fprintf(dash_cfg->mpd, " audioSamplingRate=\"%d\"", sample_rate);
	if (segments_start_with_sap || split_seg_at_rap) {
		fprintf(dash_cfg->mpd, " startWithSAP=\"%d\"", max_sap_type);
	} else {
		fprintf(dash_cfg->mpd, " startWithSAP=\"0\"");
	}
	//only appears at AdaptationSet level - need to rewrite the DASH segementer to allow writing this at the proper place
//		if ((single_file_mode==1) && segments_start_with_sap) fprintf(dash_cfg->mpd, " subsegmentStartsWithSAP=\"%d\"", max_sap_type);

	fprintf(dash_cfg->mpd, " bandwidth=\"%d\"", bandwidth);	
	if (strlen(dash_input->dependencyID))
		fprintf(dash_cfg->mpd, " dependencyId=\"%s\"", dash_input->dependencyID);
	fprintf(dash_cfg->mpd, ">\n");

	if (nb_channels && !is_bs_switching) 
		fprintf(dash_cfg->mpd, "    <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n", nb_channels);


	if (protected_track) {
		u32 prot_scheme	= gf_isom_is_media_encrypted(input, protected_track, 1);
		if (gf_isom_is_cenc_media(input, protected_track, 1)) {
			bin128 default_KID;
			gf_isom_cenc_get_default_info(input, protected_track, 1, NULL, NULL, &default_KID);
			fprintf(dash_cfg->mpd, "    <ContentProtection schemeIdUri=\"urn:mpeg:dash:mp4protection:2011\" value=\"%s\" cenc:default_KID=\"", gf_4cc_to_str(prot_scheme) );
			for (i=0; i<16; i++) {
				fprintf(dash_cfg->mpd, "%02x", default_KID[i]);
			}
			fprintf(dash_cfg->mpd, "\"/>\n");
		} 
		//todo for ISMA or OMA DRM
	}

	if (dash_cfg->dash_ctx) {
		Double seg_dur;
		opt = gf_cfg_get_key(dash_cfg->dash_ctx, "DASH", "MaxSegmentDuration");
		if (opt) {
			seg_dur = atof(opt);
			if (seg_dur < max_segment_duration) {
				sprintf(sOpt, "%f", max_segment_duration);
				gf_cfg_set_key(dash_cfg->dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
				seg_dur = max_segment_duration;
			} else {
				max_segment_duration = seg_dur;
			}
		} else {
			sprintf(sOpt, "%f", max_segment_duration);
			gf_cfg_set_key(dash_cfg->dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
		}
	}

	if (use_url_template) {
		/*segment template depends on file name, but the template at the representation level*/
		if (dash_cfg->variable_seg_rad_name) {
			const char *rad_name = gf_url_get_resource_name(seg_rad_name);
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, bandwidth, 0, dash_cfg->use_segment_timeline);
			fprintf(dash_cfg->mpd, "    <SegmentTemplate timescale=\"%d\" duration=\"%d\" media=\"%s\" startNumber=\"%d\"", mpd_timescale, (u32) (max_segment_duration * mpd_timescale), SegmentName, startNumber - startNumberRewind);	
			if (!is_bs_switching) {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, dash_input->representationID, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0, dash_cfg->use_segment_timeline);
				fprintf(dash_cfg->mpd, " initialization=\"%s\"", SegmentName);
			}
			if (presentationTimeOffset) 
				fprintf(dash_cfg->mpd, " presentationTimeOffset=\"%d\"", presentationTimeOffset);
			fprintf(dash_cfg->mpd, "/>\n");
		}
	} else if (dash_cfg->single_file_mode==1) {
		fprintf(dash_cfg->mpd, "    <BaseURL>%s</BaseURL>\n", gf_url_get_resource_name( gf_isom_get_filename(output) ) );	
		fprintf(dash_cfg->mpd, "    <SegmentBase indexRangeExact=\"true\" indexRange=\"%d-%d\"", index_start_range, index_end_range);	
		if (presentationTimeOffset) 
			fprintf(dash_cfg->mpd, " presentationTimeOffset=\"%d\"", presentationTimeOffset);
		fprintf(dash_cfg->mpd, "/>\n");	
	} else {
		if (!seg_rad_name) {
			fprintf(dash_cfg->mpd, "    <BaseURL>%s</BaseURL>\n", gf_url_get_resource_name( gf_isom_get_filename(output) ) );	
		}
		fprintf(dash_cfg->mpd, "    <SegmentList");
		if (!mpd_timeline_bs) {
			fprintf(dash_cfg->mpd, " timescale=\"%d\" duration=\"%d\"", mpd_timescale, (u32) (max_segment_duration * mpd_timescale));	
		}
		if (presentationTimeOffset) {
			fprintf(dash_cfg->mpd, " presentationTimeOffset=\"%d\"", presentationTimeOffset);
		}
		fprintf(dash_cfg->mpd, ">\n");	
		/*we are not in bitstreamSwitching mode*/
		if (!is_bs_switching) {
			fprintf(dash_cfg->mpd, "     <Initialization");	
			if (!seg_rad_name) {
				fprintf(dash_cfg->mpd, " range=\"0-"LLD"\"", init_seg_size-1);
			} else {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, dash_input->representationID, gf_url_get_resource_name( seg_rad_name) , !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0, dash_cfg->use_segment_timeline);
				fprintf(dash_cfg->mpd, " sourceURL=\"%s\"", SegmentName);
			}
			fprintf(dash_cfg->mpd, "/>\n");
		}
	}
	if (mpd_bs) {
		char *mpd_seg_info = NULL;
		u32 size;
		gf_bs_get_content(mpd_bs, &mpd_seg_info, &size);
		gf_fwrite(mpd_seg_info, 1, size, dash_cfg->mpd);
		gf_free(mpd_seg_info);
	}

	if (!use_url_template && (dash_cfg->single_file_mode!=1)) {
		fprintf(dash_cfg->mpd, "    </SegmentList>\n");
	}

	fprintf(dash_cfg->mpd, "   </Representation>\n");


	/*store context*/
	if (dash_cfg->dash_ctx) {
		period_duration += (Double)segment_start_time / dash_cfg->dash_scale;

		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);

			/*InitialTSOffset is used when joining different files - if we are still in the same file , do not update it*/
			if (tf->done) {
				sprintf(sKey, "TKID_%d_NextDecodingTime", tf->TrackID);
				sprintf(sOpt, LLU, tf->InitialTSOffset + tf->next_sample_dts);
				gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, sKey, sOpt);
			}

			if (dash_cfg->subduration) {
				sprintf(sKey, "TKID_%d_NextSampleNum", tf->TrackID);
				sprintf(sOpt, "%d", tf->SampleNum);
				gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, sKey, tf->done ? NULL : sOpt);

				sprintf(sKey, "TKID_%d_LastSampleCTS", tf->TrackID);
				sprintf(sOpt, LLU, tf->last_sample_cts);
				gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, sKey, tf->done ? NULL : sOpt);

				sprintf(sKey, "TKID_%d_NextSampleDTS", tf->TrackID);
				sprintf(sOpt, LLU, tf->next_sample_dts);
				gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, sKey, tf->done ? NULL : sOpt);
			}
		}
		sprintf(sOpt, "%d", cur_seg);
		gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "NextSegmentIndex", sOpt);

		fragment_index = gf_isom_get_next_moof_number(output);
		sprintf(sOpt, "%d", fragment_index);
		gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "NextFragmentIndex", sOpt);


		sprintf(sOpt, "%f", period_duration);
		gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "CumulatedDuration", sOpt);

		if (store_dash_params) {
			sprintf(sOpt, "%u", bandwidth);
			gf_cfg_set_key(dash_cfg->dash_ctx, RepSecName, "Bandwidth", sOpt);
		}
	}

err_exit:
	if (fragmenters){
		while (gf_list_count(fragmenters)) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, 0);
			gf_free(tf);
			gf_list_rem(fragmenters, 0);
		}
		gf_list_del(fragmenters);
	}
	if (output) {
		if (e) gf_isom_delete(output);
		else gf_isom_close(output);
	}
	if (!bs_switching_is_output && bs_switch_segment) 
		gf_isom_delete(bs_switch_segment);
	gf_set_progress("ISO File Fragmenting", nb_samp, nb_samp);
	if (mpd_bs) gf_bs_del(mpd_bs);
	if (mpd_timeline_bs) gf_bs_del(mpd_timeline_bs);
	return e;
}


static GF_Err dasher_isom_get_input_components_info(GF_DashSegInput *input, GF_DASHSegmenterOptions *opts)
{
	u32 i;
	GF_ISOFile *in;
	Double dur;

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
		
		if (gf_isom_get_sample_count(in, i+1)<2)
			continue;
	
		dur = (Double) gf_isom_get_track_duration(in, i+1);
		dur /= gf_isom_get_timescale(in);
		if (!dur) {
			dur = (Double) gf_isom_get_media_duration(in, i+1);
			dur /= gf_isom_get_media_timescale(in, i+1);
		}
		if (dur > input->duration) input->duration = dur;

		input->components[input->nb_components].ID = gf_isom_get_track_id(in, i+1);
		input->components[input->nb_components].media_type = mtype;
		if (mtype == GF_ISOM_MEDIA_VISUAL) {
			gf_isom_get_visual_info(in, i+1, 1, &input->components[input->nb_components].width, &input->components[input->nb_components].height);

			input->components[input->nb_components].fps_num = gf_isom_get_media_timescale(in, i+1);
			/*get duration of 2nd sample*/
			input->components[input->nb_components].fps_denum = gf_isom_get_sample_duration(in, i+1, 2);

		}
		/*non-video tracks, get lang*/
		else if (mtype == GF_ISOM_MEDIA_AUDIO) {
			u8 bps;
			gf_isom_get_audio_info(in, i+1, 1, &input->components[input->nb_components].sample_rate, &input->components[input->nb_components].channels, &bps);
		}
		gf_isom_get_media_language(in, i+1, input->components[input->nb_components].szLang);

		input->nb_components++;
	}
	return gf_isom_close(in);
}

static GF_Err dasher_isom_classify_input(GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 input_idx, u32 *current_group_id, u32 *max_sap_type)
{
	u32 i, j;
	GF_ISOFile *set_file;
	
	if (input_idx+1>=nb_dash_inputs)
		return GF_OK;

	set_file = gf_isom_open(dash_inputs[input_idx].file_name, GF_ISOM_OPEN_READ, NULL);

	for (i=input_idx+1; i<nb_dash_inputs; i++) {
		Bool valid_in_adaptation_set = 1;
		Bool assign_to_group = 1;
		GF_ISOFile *in;

		if (dash_inputs[input_idx].period != dash_inputs[i].period) 
			continue;
		
		if (strcmp(dash_inputs[input_idx].szMime, dash_inputs[i].szMime))
			continue;

		if (strcmp(dash_inputs[input_idx].role, dash_inputs[i].role))
			continue;

		in = gf_isom_open(dash_inputs[i].file_name, GF_ISOM_OPEN_READ, NULL);

		for (j=0; j<gf_isom_get_track_count(set_file); j++) {
			u32 mtype, msub_type;
			Bool same_codec = 1;
			u32 track = gf_isom_get_track_by_id(in, gf_isom_get_track_id(set_file, j+1));

			if (!track) {
				valid_in_adaptation_set = 0;
				assign_to_group = 0;
				break;
			}

			if (dash_inputs[input_idx].single_track_num && (dash_inputs[input_idx].single_track_num != j+1))
				continue;

			if (dash_inputs[i].single_track_num) {
				track = dash_inputs[i].single_track_num;
			}

			mtype = gf_isom_get_media_type(set_file, j+1);
			if (mtype != gf_isom_get_media_type(in, track)) {
				valid_in_adaptation_set = 0;
				assign_to_group = 0;
				break;
			}
			msub_type = gf_isom_get_media_subtype(set_file, j+1, 1);
			if (msub_type != gf_isom_get_media_subtype(in, track, 1)) same_codec = 0;
			if ((msub_type==GF_ISOM_SUBTYPE_MPEG4) 
				|| (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP) 
				|| (msub_type==GF_ISOM_SUBTYPE_AVC_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_AVC2_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_AVC3_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_AVC4_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_SVC_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_HVC1)
				|| (msub_type==GF_ISOM_SUBTYPE_HEV1)
				|| (msub_type==GF_ISOM_SUBTYPE_LSR1)
				) {
					GF_DecoderConfig *dcd1 = gf_isom_get_decoder_config(set_file, j+1, 1);
					GF_DecoderConfig *dcd2 = gf_isom_get_decoder_config(in, track, 1);
					if (dcd1 && dcd2 && (dcd1->streamType==dcd2->streamType) && (dcd1->objectTypeIndication==dcd2->objectTypeIndication)) {
						same_codec = 1;
					} else {
						same_codec = 0;
					}
					if (dcd1) gf_odf_desc_del((GF_Descriptor *)dcd1);
					if (dcd2) gf_odf_desc_del((GF_Descriptor *)dcd2);
			}

			if (!same_codec) {
				valid_in_adaptation_set = 0;
				break;
			}

			if ((mtype!=GF_ISOM_MEDIA_VISUAL) && (mtype!=GF_ISOM_MEDIA_HINT)) {
				char szLang1[4], szLang2[4];
				szLang1[3] = szLang2[3] = 0;
				gf_isom_get_media_language(set_file, j+1, szLang1);
				gf_isom_get_media_language(in, track, szLang2);
				if (stricmp(szLang1, szLang2)) {
					valid_in_adaptation_set = 0;
					break;
				}
			}
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
				u32 w1, h1, w2, h2, sap_type;
				Bool rap, roll;
				s32 roll_dist;

				gf_isom_get_track_layout_info(set_file, j+1, &w1, &h1, NULL, NULL, NULL);
				gf_isom_get_track_layout_info(in, track, &w2, &h2, NULL, NULL, NULL);

				if (h1*w2 != h2*w1) {
					valid_in_adaptation_set = 0;
					break;
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

static GF_Err dasher_isom_create_init_segment(GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 adaptation_set, char *szInitName, const char *tmpdir, GF_DASHSegmenterOptions *dash_opts, Bool *disable_bs_switching)
{
	GF_Err e = GF_OK;
	u32 i;
	u32 single_track_id = 0;
	Bool sps_merge_failed = 0;
	Bool use_avc3 = 0;
	Bool use_hevc = 0;
	Bool use_inband_param_set;
	Bool single_segment = (dash_opts->single_file_mode==1) ? 1 : 0;
	GF_ISOFile *init_seg = gf_isom_open(szInitName, GF_ISOM_OPEN_WRITE, tmpdir);

	for (i=0; i<nb_dash_inputs; i++) {
		u32 j;
		GF_ISOFile *in;

		if (dash_inputs[i].adaptation_set != adaptation_set)
			continue;

		//no inband param set for scalable files
		use_inband_param_set = dash_inputs[i].trackNum ? 0 : dash_opts->inband_param_set;

		in = gf_isom_open(dash_inputs[i].file_name, GF_ISOM_OPEN_READ, NULL);
		if (!in) {
			e = gf_isom_last_error(NULL);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Error while opening %s: %s\n", dash_inputs[i].file_name, gf_error_to_string( e ) ));
			return e;
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
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Cannot merge track with multiple sample descriptions (file %s) - try disabling bitstream switching\n", dash_inputs[i].file_name ));
					gf_isom_delete(init_seg);
					gf_isom_delete(in);
					return e;
				}

				/*if not the same sample desc we might need to clone it*/
				if (! gf_isom_is_same_sample_description(in, j+1, 1, init_seg, track, 1)) {
					u32 merge_mode = 1;
					u32 stype1, stype2;
					stype1 = gf_isom_get_media_subtype(in, j+1, 1);
					stype2 = gf_isom_get_media_subtype(init_seg, track, 1);
					if (stype1 != stype2) merge_mode = 0;
					switch (stype1) {
						case GF_4CC( 'a', 'v', 'c', '1'):
						case GF_4CC( 'a', 'v', 'c', '2'):
							if (use_avc3) 
								merge_mode = 2;
							break;
						case GF_4CC( 's', 'v', 'c', '1'):
							break;
						case GF_4CC( 'a', 'v', 'c', '3'):
						case GF_4CC( 'a', 'v', 'c', '4'):
							/*we don't want to clone SPS/PPS since they are already inside the samples*/
							merge_mode = 2;
							break;
						case GF_4CC( 'h', 'v', 'c', '1'):
							if (use_hevc) 
								merge_mode = 2;
							break;
						case GF_4CC( 'h', 'e', 'v', '1'):
							/*we don't want to clone SPS/PPS since they are already inside the samples*/
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
							sps_merge_failed = 1;
						}
						gf_odf_avc_cfg_del(avccfg1);
						gf_odf_avc_cfg_del(avccfg2);
					}

					/*cannot merge, clone*/
					if (merge_mode==0)
						gf_isom_clone_sample_description(init_seg, track, in, j+1, 1, NULL, NULL, &outDescIndex);
				}
			} else {
				u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess;
				u8 defaultPadding;
				u16 defaultDegradationPriority;

				gf_isom_clone_track(in, j+1, init_seg, GF_FALSE, &track);

				switch (gf_isom_get_media_subtype(in, j+1, 1)) {
				case GF_4CC( 'a', 'v', 'c', '1'):
				case GF_4CC( 'a', 'v', 'c', '2'):
				case GF_4CC( 's', 'v', 'c', '1'):
					if (use_inband_param_set) {
						gf_isom_avc_set_inband_config(init_seg, track, 1);
						use_avc3 = GF_TRUE;
					}
					break;
				case GF_4CC( 'h', 'v', 'c', '1'):
					if (use_inband_param_set) {
						gf_isom_hevc_set_inband_config(init_seg, track, 1);
					}
					use_hevc = GF_TRUE;
					break;
				}


				gf_isom_get_fragment_defaults(in, j+1, &defaultDuration, &defaultSize, 
										&defaultDescriptionIndex, &defaultRandomAccess, 
										&defaultPadding, &defaultDegradationPriority);
				//setup for fragmentation
				e = gf_isom_setup_track_fragment(init_seg, gf_isom_get_track_id(init_seg, track),
							defaultDescriptionIndex, defaultDuration,
							defaultSize, (u8) defaultRandomAccess,
							defaultPadding, defaultDegradationPriority);
				if (e) break;
			}
		}
		if (!i) {
			if (use_hevc || use_avc3) {
				gf_isom_set_brand_info(init_seg, GF_4CC('i','s','o','6'), 1);
			} else {
				gf_isom_set_brand_info(init_seg, GF_4CC('i','s','o','5'), 1);
			}
			/*DASH self-init media segment*/
			if (single_segment) {
				gf_isom_modify_alternate_brand(init_seg, GF_4CC('d','s','m','s'), 1);
			} else {
				gf_isom_modify_alternate_brand(init_seg, GF_4CC('d','a','s','h'), 1);
			}

			if (!dash_opts->pssh_moof) {
				e = gf_isom_clone_pssh(init_seg, in, GF_TRUE);
			}
		}
		gf_isom_close(in);
	}
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: Couldn't create initialization segment: error %s\n", gf_error_to_string(e) ));
		*disable_bs_switching = GF_TRUE;
		gf_isom_delete(init_seg);
		gf_delete_file(szInitName);
		return GF_OK;
	} else if (sps_merge_failed) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: Couldnt merge AVC|H264 SPS from different files (same SPS ID used) - different sample descriptions will be used\n"));
		*disable_bs_switching = GF_TRUE;
		gf_isom_delete(init_seg);
		gf_delete_file(szInitName);
		return GF_OK;
	} else {
		e = gf_isom_close(init_seg);
	}
	return e;
}

static GF_Err dasher_isom_segment_file(GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenterOptions *dash_cfg, Bool first_in_set)
{
	GF_ISOFile *in = gf_isom_open(dash_input->file_name, GF_ISOM_OPEN_READ, dash_cfg->tmpdir);
	GF_Err e = gf_media_isom_segment_file(in, szOutName, dash_cfg->fragment_duration, dash_cfg, dash_input, first_in_set);
	gf_isom_close(in);
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

		if (strcmp(dash_inputs[input_idx].role, dash_inputs[i].role))
			continue;

		memset(&probe, 0, sizeof(GF_MediaImporter));
		probe.flags = GF_IMPORT_PROBE_ONLY;
		probe.in_name = (char *)dash_inputs[i].file_name;
		e = gf_media_import(&probe);
		if (e) return e;

		if (in.nb_progs != probe.nb_progs) valid_in_adaptation_set = 0;
		if (in.nb_tracks != probe.nb_tracks) valid_in_adaptation_set = 0;

		for (j=0; j<in.nb_tracks; j++) {
			struct __track_import_info *src_tk, *probe_tk;
			Bool found = GF_FALSE;
			/*make sure we have the same track*/
			for (k=0; k<probe.nb_tracks;k++) {
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

#endif

#ifndef GPAC_DISABLE_MPEG2TS

static GF_Err dasher_generic_get_components_info(GF_DashSegInput *input, GF_DASHSegmenterOptions *opts)
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
		if (in.tk_info[i].lang)
			strcpy(input->components[i].szLang, gf_4cc_to_str(in.tk_info[i].lang) );
		strcpy(input->components[i].szCodec, in.tk_info[i].szCodecProfile);
		input->components[i].fps_denum = 1000;
		input->components[i].fps_num = (u32) (in.tk_info[i].video_info.FPS*1000);
	}
#endif
	return GF_OK;
}
#endif


#ifndef GPAC_DISABLE_MPEG2TS

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
	Bool store_segment = 1;

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
		m2ts_sidx_add_entry(index_info->sidx, 0, size, duration, index_info->first_pes_sap, index_info->SAP_type, SAP_delta_time);

		/*add pcrb entry*/
		index_info->pcrb->subsegment_count++;
		index_info->pcrb->pcr_values = gf_realloc(index_info->pcrb->pcr_values, index_info->pcrb->subsegment_count*sizeof(u64));
		index_info->pcrb->pcr_values[index_info->pcrb->subsegment_count-1] = index_info->interpolated_pcr_value;

		/* adjust the previous index duration */
		if (index_info->sidx->nb_refs > 0 && (index_info->base_PTS < index_info->prev_last_PTS) ) {
			prev_duration = (u32)(index_info->base_PTS-index_info->prev_base_PTS);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("           time-range adj.: %.03f-%.03f / %.03f sec.\n", 		
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (" %.03f-%0.3f / %.03f sec., %d-%d / %d bytes, ",
			(index_info->base_PTS - index_info->first_PTS)/90000.0, 
			(index_info->last_PTS - index_info->first_PTS)/90000.0, duration/90000.0,
			index_info->base_offset, end_offset, size));
		if (index_info->SAP_type) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("RAP @ %.03f sec. / %d bytes", (index_info->first_SAP_PTS - index_info->first_PTS)/90000.0, 
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

		index_info->pes_after_last_pat_is_sap= 0;
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
		ts_seg->has_seen_pat = 1;
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
			Bool done = 1;
			GF_M2TS_Program *prog = (GF_M2TS_Program*)par;
			count = gf_list_count(prog->streams);
			for (i=0; i<count; i++) {
				GF_M2TS_ES *es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
				if (es->flags&GF_M2TS_ES_IS_PES) {
					GF_M2TS_PES *pes = (GF_M2TS_PES *)es; 
					if (!pes->aud_sr && !pes->vid_w) {
						done = 0;
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
		pck = par;
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
			ts_seg->first_pat_position_valid = 1;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		ts_seg->first_pes_after_last_pat = 0;
		ts_seg->pes_after_last_pat_is_sap = 0;
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = 1;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		ts_seg->first_pes_after_last_pat = 0;
		ts_seg->pes_after_last_pat_is_sap = 0;
		break;
	case GF_M2TS_EVT_PAT_REPEAT:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = 1;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		ts_seg->first_pes_after_last_pat = 0;
		ts_seg->pes_after_last_pat_is_sap = 0;
		break;
	case GF_M2TS_EVT_CAT_FOUND:
		if (!ts_seg->first_cat_position_valid) {
			ts_seg->first_cat_position_valid = 1;
			ts_seg->first_cat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_cat_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_CAT_UPDATE:
		if (!ts_seg->first_cat_position_valid) {
			ts_seg->first_cat_position_valid = 1;
			ts_seg->first_cat_position = (ts->pck_number-1)*188;
		}
		break;
	case GF_M2TS_EVT_CAT_REPEAT:
		if (!ts_seg->first_cat_position_valid) {
			ts_seg->first_cat_position_valid = 1;
			ts_seg->first_cat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_cat_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		ts_seg->has_seen_pat = 1;
		prog = (GF_M2TS_Program*)par;
		if (!ts_seg->first_pmt_position_valid) {
			ts_seg->first_pmt_position_valid = 1;
			ts_seg->first_pmt_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pmt_position = (ts->pck_number-1)*188;
		/* we create indexing information on the stream used for carrying the PCR */
		ts_seg->reference_pid = prog->pcr_pid;
		ts_seg->reference_stream = (GF_M2TS_PES *) ts_seg->ts->ess[prog->pcr_pid];
		count = gf_list_count(prog->streams);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Program number %d found - %d streams:\n", prog->number, count));
		for (i=0; i<count; i++) {
			GF_M2TS_ES *es = gf_list_get(prog->streams, i);
			gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_DEFAULT);
		}
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		if (!ts_seg->first_pmt_position_valid) {
			ts_seg->first_pmt_position_valid = 1;
			ts_seg->first_pmt_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pmt_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
		if (!ts_seg->first_pmt_position_valid) {
			ts_seg->first_pmt_position_valid = 1;
			ts_seg->first_pmt_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pmt_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PES_PCK:
		pck = par;
		/*We need the interpolated PCR for the pcrb, hence moved this calculus out, and saving the calculated value in ts_seg to put it in the pcrb*/
		pes = pck->stream;
		/* Interpolated PCR value for the TS packet containing the PES header start */
		interpolated_pcr_value = 0;
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
					ts_seg->pes_after_last_pat_is_sap=0;
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
					ts_seg->pes_after_last_pat_is_sap=1;
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
		pck = par;
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
	ts_seg->src = gf_f64_open(file, "rb");
	if (!ts_seg->src) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Cannot open input %s: no such file\n", file));
		return GF_URL_ERROR;
	}
	ts_seg->ts = gf_m2ts_demux_new();
	ts_seg->ts->on_event = dash_m2ts_event_check_pat;
	ts_seg->ts->notify_pes_timing = GF_TRUE;
	ts_seg->ts->user = ts_seg;

	gf_f64_seek(ts_seg->src, 0, SEEK_END);
	ts_seg->file_size = gf_f64_tell(ts_seg->src);
	gf_f64_seek(ts_seg->src, 0, SEEK_SET);

	if (probe_mode) {
		/* first loop to process all packets between two PAT, and assume all signaling was found between these 2 PATs */
		while (!feof(ts_seg->src)) {
			char data[188];
			u32 size = (u32) fread(data, 1, 188, ts_seg->src);
			if (size<188) break;

			gf_m2ts_process_data(ts_seg->ts, data, size);
			if (ts_seg->has_seen_pat==probe_mode)
				break;
		}
		gf_m2ts_reset_parsers(ts_seg->ts);
		gf_f64_seek(ts_seg->src, 0, SEEK_SET);
	}
	ts_seg->ts->on_event = dash_m2ts_event;
	return GF_OK;
}

static void dasher_del_ts_demux(GF_TSSegmenter *ts_seg)
{
	if (ts_seg->ts) gf_m2ts_demux_del(ts_seg->ts);
	if (ts_seg->src) fclose(ts_seg->src);
	memset(ts_seg, 0, sizeof(GF_TSSegmenter));
}

static GF_Err dasher_mp2t_get_components_info(GF_DashSegInput *dash_input, GF_DASHSegmenterOptions *dash_opts)
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

static GF_Err dasher_mp2t_segment_file(GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenterOptions *dash_cfg, Bool first_in_set)
{	
	GF_TSSegmenter ts_seg;
	Bool rewrite_input = 0;
	u8 is_pes[GF_M2TS_MAX_STREAMS];
	char szOpt[100];
	char SegName[GF_MAX_PATH], IdxName[GF_MAX_PATH];
	char szSectionName[100], szRepURLsSecName[100];
	char szCodecs[100];
	const char *opt;
	u32 i, startNumberRewind;
	GF_Err e;
	u64 start, pcr_shift, next_pcr_shift;
	Double cumulated_duration = 0;
	u32 bandwidth = 0;
	u32 segment_index;
	/*compute name for indexed segments*/
	const char *basename = gf_url_get_resource_name(szOutName);

	/*perform indexation of the file, this info will be destroyed at the end of the segment file routine*/
	e = dasher_get_ts_demux(&ts_seg, dash_input->file_name, 0);
	if (e) return e;
	
	ts_seg.segment_duration = dash_cfg->segment_duration;
	ts_seg.segment_at_rap = dash_cfg->segments_start_with_rap;

	ts_seg.bandwidth = (u32) (ts_seg.file_size * 8 / dash_input->duration);	

	/*create bitstreams*/

	segment_index = 1;
	startNumberRewind = 0;
	ts_seg.index_file = NULL;
	ts_seg.index_bs = NULL;
	if (!dash_cfg->dash_ctx && (dash_cfg->use_url_template != 2)) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		GF_SegmentTypeBox *styp;

		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, 1, IdxName, szOutName, dash_input->representationID, dash_cfg->seg_rad_name, "six", 0, 0, 0, dash_cfg->use_segment_timeline);	

		ts_seg.index_file = gf_f64_open(IdxName, "wb");
		if (!ts_seg.index_file) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Cannot create index file %s\n", IdxName));
			e = GF_IO_ERR;
			goto exit;
		}
		ts_seg.index_bs = gf_bs_from_file(ts_seg.index_file, GF_BITSTREAM_WRITE);

		styp = (GF_SegmentTypeBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_STYP);
		styp->majorBrand = GF_4CC('r','i','s','x');
		styp->minorVersion = 0;
		styp->altBrand = (u32*)gf_malloc(sizeof(u32));
		styp->altBrand[0] = styp->majorBrand;
		styp->altCount = 1;
		gf_isom_box_size((GF_Box *)styp);
		gf_isom_box_write((GF_Box *)styp, ts_seg.index_bs);
		gf_isom_box_del((GF_Box *)styp);
#endif
	}

	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, 1, IdxName, basename, dash_input->representationID, dash_cfg->seg_rad_name, "six", 0, 0, 0, dash_cfg->use_segment_timeline);	

	ts_seg.PCR_DTS_initial_diff = (u64) -1;
	ts_seg.subduration = (u32) (dash_cfg->subduration * 90000);

	szSectionName[0] = 0;
	if (dash_cfg->dash_ctx) {
		sprintf(szSectionName, "Representation_%s", dash_input->representationID);
		sprintf(szRepURLsSecName, "URLs_%s", dash_input->representationID);

		/*restart where we left last time*/
		opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "ByteOffset");
		if (opt) {
			u64 offset;
			sscanf(opt, LLU, &offset);

			while (!feof(ts_seg.src) && !ts_seg.has_seen_pat) {
				char data[NB_TSPCK_IO_BYTES];
				u32 size = (u32) fread(data, 1, NB_TSPCK_IO_BYTES, ts_seg.src);
				gf_m2ts_process_data(ts_seg.ts, data, size);

				if (size<NB_TSPCK_IO_BYTES) break;
			}
			gf_m2ts_reset_parsers(ts_seg.ts);
			ts_seg.base_PTS = 0;
			gf_f64_seek(ts_seg.src, offset, SEEK_SET);
			ts_seg.base_offset = offset;
			ts_seg.ts->pck_number = (u32) (offset/188);
		}

		opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "InitialDTSOffset");
		if (opt) sscanf(opt, LLU, &ts_seg.PCR_DTS_initial_diff);

		opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "DurationAtLastPass");
		if (opt) sscanf(opt, LLD, &ts_seg.duration_at_last_pass);
	}

	/*index the file*/
	while (!feof(ts_seg.src) && !ts_seg.suspend_indexing) {
		char data[NB_TSPCK_IO_BYTES];
		u32 size = (u32) fread(data, 1, NB_TSPCK_IO_BYTES, ts_seg.src);
		gf_m2ts_process_data(ts_seg.ts, data, size);
		if (size<NB_TSPCK_IO_BYTES) break;
	}
	if (feof(ts_seg.src)) ts_seg.suspend_indexing = 0;

	next_pcr_shift = 0;
	if (!ts_seg.suspend_indexing) {
		next_pcr_shift = ts_seg.last_DTS + ts_seg.last_frame_duration - ts_seg.PCR_DTS_initial_diff;
	}

	/* flush SIDX entry for the last packets */
	m2ts_sidx_flush_entry(&ts_seg);
	m2ts_sidx_finalize_size(&ts_seg, ts_seg.file_size);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH]: Indexing done (1 sidx, %d entries).\n", ts_seg.sidx->nb_refs));

	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, 1, IdxName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "six", 0, 0, 0, dash_cfg->use_segment_timeline);	


	memset(is_pes, 0, sizeof(u8)*GF_M2TS_MAX_STREAMS);
	for (i=0; i<dash_input->nb_components; i++) {
		is_pes[ dash_input->components[i].ID ] = 1;
	}
	pcr_shift = 0;

	bandwidth = dash_input->bandwidth;
	if (!bandwidth) bandwidth = ts_seg.bandwidth;

	if (dash_cfg->dash_ctx) {

		opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "Setup");
		if (!opt || strcmp(opt, "yes")) {
			gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "Setup", "yes");
			gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "ID", dash_input->representationID);
		} else {
			if (!bandwidth) {
				opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "Bandwidth");
				if (opt) sscanf(opt, "%u", &bandwidth);
			}

			opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "StartIndex");
			if (opt) sscanf(opt, "%u", &segment_index);

			/*adjust the startNumber according to the timeShiftBuffer depth*/
			if ((dash_cfg->time_shift_depth>0) && (segment_index > (u32)dash_cfg->time_shift_depth) ) {
				startNumberRewind = dash_cfg->time_shift_depth;
			}


			opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "PCR90kOffset");
			if (opt) sscanf(opt, LLU, &pcr_shift);

			opt = gf_cfg_get_key(dash_cfg->dash_ctx, szSectionName, "CumulatedDuration");
			if (opt) cumulated_duration = atof(opt);
		}
	}

	/*write segment template for all representations*/
	if (first_in_set && dash_cfg->seg_rad_name && dash_cfg->use_url_template && !dash_cfg->variable_seg_rad_name) {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
		fprintf(dash_cfg->mpd, "   <SegmentTemplate timescale=\"90000\" duration=\"%d\" startNumber=\"%d\" media=\"%s\"", (u32) (90000*dash_cfg->segment_duration), segment_index - startNumberRewind, SegName); 
		if (!dash_cfg->dash_ctx) {
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, 1, IdxName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "six", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
			fprintf(dash_cfg->mpd, " index=\"%s\"", IdxName); 
		} 
		fprintf(dash_cfg->mpd, "/>\n");
	}


	fprintf(dash_cfg->mpd, "   <Representation id=\"%s\" mimeType=\"video/mp2t\"", dash_input->representationID);
	szCodecs[0] = 0;
	for (i=0; i<dash_input->nb_components; i++) {
		if (strlen(dash_input->components[i].szCodec)) 
			if (strlen(szCodecs))
				strcat(szCodecs, ",");
		strcat(szCodecs, dash_input->components[i].szCodec);

		if (dash_input->components[i].width && dash_input->components[i].height) 
			fprintf(dash_cfg->mpd, " width=\"%d\" height=\"%d\"", dash_input->components[i].width, dash_input->components[i].height);

		if (dash_input->components[i].sample_rate) 
			fprintf(dash_cfg->mpd, " audioSamplingRate=\"%d\"", dash_input->components[i].sample_rate);
	}
	if (strlen(szCodecs))
		fprintf(dash_cfg->mpd, " codecs=\"%s\"", szCodecs);

	fprintf(dash_cfg->mpd, " startWithSAP=\"%d\"", dash_cfg->segments_start_with_rap ? 1 : 0);
	fprintf(dash_cfg->mpd, " bandwidth=\"%d\"", bandwidth);
	fprintf(dash_cfg->mpd, ">\n");


	if (dash_cfg->single_file_mode==1) {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
		fprintf(dash_cfg->mpd, "    <BaseURL>%s</BaseURL>\n", SegName);

		fprintf(dash_cfg->mpd, "    <SegmentBase>\n");
		fprintf(dash_cfg->mpd, "     <RepresentationIndex sourceURL=\"%s\"/>\n", IdxName);
		fprintf(dash_cfg->mpd, "    </SegmentBase>\n");

		/*we rewrite the file*/
		rewrite_input = 1;
	} else {
		if (dash_cfg->seg_rad_name && dash_cfg->use_url_template) {
			if (dash_cfg->variable_seg_rad_name) {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
				fprintf(dash_cfg->mpd, "    <SegmentTemplate timescale=\"90000\" duration=\"%d\" startNumber=\"%d\" media=\"%s\"", (u32) (90000*dash_cfg->segment_duration), segment_index, SegName); 
				if (!dash_cfg->dash_ctx) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, 1, IdxName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "six", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
					fprintf(dash_cfg->mpd, " index=\"%s\"", IdxName); 
				}

				if (dash_cfg->time_shift_depth>=0) 
					fprintf(dash_cfg->mpd, " presentationTimeOffset=\""LLD"\"", ts_seg.sidx->earliest_presentation_time + pcr_shift); 
				fprintf(dash_cfg->mpd, "/>\n"); 
			} else if (dash_cfg->time_shift_depth>=0) {
				fprintf(dash_cfg->mpd, "    <SegmentTemplate presentationTimeOffset=\""LLD"\"/>\n", ts_seg.sidx->earliest_presentation_time + pcr_shift); 
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH]: PTSOffset "LLD" - startNumber %d - time %g\n", ts_seg.sidx->earliest_presentation_time + pcr_shift, segment_index, (Double) (s64) (ts_seg.sidx->earliest_presentation_time + pcr_shift) / 90000.0));
			}
		} else {
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
			if (dash_cfg->single_file_mode)
				fprintf(dash_cfg->mpd, "    <BaseURL>%s</BaseURL>\n",SegName);
			fprintf(dash_cfg->mpd, "    <SegmentList timescale=\"90000\" duration=\"%d\"", (u32) (90000*dash_cfg->segment_duration)); 
			if (dash_cfg->time_shift_depth>=0) 
				fprintf(dash_cfg->mpd, " presentationTimeOffset=\""LLD"\"", ts_seg.sidx->earliest_presentation_time + pcr_shift); 
			fprintf(dash_cfg->mpd, ">\n"); 

			if (!dash_cfg->dash_ctx) {
				fprintf(dash_cfg->mpd, "     <RepresentationIndex sourceURL=\"%s\"/>\n", IdxName); 
			}
		}

		/*rewrite previous SegmentList entries*/
		if ( dash_cfg->dash_ctx && ((dash_cfg->single_file_mode==2) || (!dash_cfg->single_file_mode && !dash_cfg->use_url_template))) {
			/*rewrite previous URLs*/
			const char *opt;
			u32 count, i;
			count = gf_cfg_get_key_count(dash_cfg->dash_ctx, szRepURLsSecName);
			for (i=0; i<count; i++) {
				const char *key_name = gf_cfg_get_key_name(dash_cfg->dash_ctx, szRepURLsSecName, i);
				opt = gf_cfg_get_key(dash_cfg->dash_ctx, szRepURLsSecName, key_name);
				fprintf(dash_cfg->mpd, "     %s\n", opt);	
			}
		}

		if (dash_cfg->single_file_mode==2) {

			start = ts_seg.sidx->first_offset;
			for (i=0; i<ts_seg.sidx->nb_refs; i++) {
				GF_SIDXReference *ref = &ts_seg.sidx->refs[i];
				fprintf(dash_cfg->mpd, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"/>\n", start, start+ref->reference_size-1);
				start += ref->reference_size;
			}
			rewrite_input = GF_TRUE;

		} else {
			FILE *src, *dst;
			u64 pos, end;
			Double current_time = cumulated_duration, dur;
			src = gf_f64_open(dash_input->file_name, "rb");
			start = ts_seg.sidx->first_offset;
			for (i=0; i<ts_seg.sidx->nb_refs; i++) {
				char buf[NB_TSPCK_IO_BYTES];
				GF_SIDXReference *ref = &ts_seg.sidx->refs[i];

				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, szOutName, dash_input->representationID, dash_cfg->seg_rad_name, "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);

				/*warning - we may introduce repeated sequence number when concatenating files. We should use switching 
				segments to force reset of the continuity counter for all our pids - we don't because most players don't car ...*/
				if (dash_cfg->use_url_template != 2) {
					dst = gf_f64_open(SegName, "wb");
					if (!dst) {
						fclose(src);
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Cannot create segment file %s\n", SegName));
						e = GF_IO_ERR;
						goto exit;
					}

					gf_dasher_store_segment_info(dash_cfg, SegName, current_time);
					dur = ref->subsegment_duration;
					dur /= 90000;
					current_time += dur;

					gf_f64_seek(src, start, SEEK_SET);
					pos = start;
					end = start+ref->reference_size;
					while (pos<end) {
						u32 res;
						u32 to_read = NB_TSPCK_IO_BYTES;
						if (pos+NB_TSPCK_IO_BYTES >= end) {
							to_read = (u32) (end-pos);
						}
						res = (u32) fread(buf, 1, to_read, src);
						if (res==to_read) {
							gf_m2ts_restamp(buf, res, pcr_shift, is_pes);
							res = (u32) fwrite(buf, 1, to_read, dst);
						}
						if (res!=to_read) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: IO error while Extracting segment %03d / %03d\r", i+1, ts_seg.sidx->nb_refs));
							break;
						}
						pos += res;
					}
					fclose(dst);
				}
				start += ref->reference_size;

				if (!dash_cfg->use_url_template) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, basename, dash_input->representationID, dash_cfg->seg_rad_name, "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
					fprintf(dash_cfg->mpd, "     <SegmentURL media=\"%s\"/>\n", SegName);

					if (dash_cfg->dash_ctx) {
						char szKey[100], szVal[4046];
						sprintf(szKey, "UrlInfo%d", segment_index);
						sprintf(szVal, "<SegmentURL media=\"%s\"/>", SegName);
						gf_cfg_set_key(dash_cfg->dash_ctx, szRepURLsSecName, szKey, szVal);
					}
				}

				segment_index++;
				gf_set_progress("Extracting segment ", i+1, ts_seg.sidx->nb_refs);
			}
			fclose(src);
			cumulated_duration = current_time;
		}	
		if (!dash_cfg->seg_rad_name || !dash_cfg->use_url_template) {
			fprintf(dash_cfg->mpd, "    </SegmentList>\n");
		}
	}

	if (rewrite_input) {
		FILE *in, *out;
		u64 fsize, done;
		char buf[NB_TSPCK_IO_BYTES];

		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, GF_TRUE, SegName, dash_cfg->seg_rad_name ? basename : szOutName, dash_input->representationID, gf_url_get_resource_name(dash_cfg->seg_rad_name), "ts", 0, bandwidth, segment_index, dash_cfg->use_segment_timeline);
		out = gf_f64_open(SegName, "wb");
		if (!out) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Cannot create segment file %s\n", SegName));
			e = GF_IO_ERR;
			goto exit;
		}
		in = gf_f64_open(dash_input->file_name, "rb");
		gf_f64_seek(in, 0, SEEK_END);
		fsize = gf_f64_tell(in);
		gf_f64_seek(in, 0, SEEK_SET);
		done = 0;
		while (1) {
			u32 read = (u32) fread(buf, 1, NB_TSPCK_IO_BYTES, in);
			gf_m2ts_restamp(buf, read, pcr_shift, is_pes);
			fwrite(buf, 1, read, out);
			done+=read;
			gf_set_progress("Extracting segment ", done/188, fsize/188);
			if (read<NB_TSPCK_IO_BYTES) break;
		}
		fclose(in);
		fclose(out);
	}

	fprintf(dash_cfg->mpd, "   </Representation>\n");

	if (dash_cfg->dash_ctx) {
		sprintf(szOpt, "%u", bandwidth);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "Bandwidth", szOpt);

		sprintf(szOpt, "%u", segment_index);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "StartIndex", szOpt);

		sprintf(szOpt, "%g", ts_seg.segment_duration);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "CumulatedDuration", szOpt);

		sprintf(szOpt, LLU, next_pcr_shift + pcr_shift);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "PCR90kOffset", szOpt);

		sprintf(szOpt, LLU, ts_seg.suspend_indexing);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "ByteOffset", ts_seg.suspend_indexing ? szOpt : NULL);
		
		sprintf(szOpt, LLD, ts_seg.duration_at_last_pass);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "DurationAtLastPass", ts_seg.suspend_indexing ? szOpt : NULL);

		sprintf(szOpt, LLU, ts_seg.PCR_DTS_initial_diff);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "InitialDTSOffset", ts_seg.suspend_indexing ? szOpt : NULL);

		sprintf(szOpt, "%g", cumulated_duration);
		gf_cfg_set_key(dash_cfg->dash_ctx, szSectionName, "CumulatedDuration", szOpt);
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
	if (ts_seg.index_file) fclose(ts_seg.index_file);
	if (ts_seg.index_bs) gf_bs_del(ts_seg.index_bs);
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

GF_Err gf_dash_segmenter_probe_input(GF_DashSegInput **io_dash_inputs, u32 *nb_dash_inputs, u32 idx)
{
	GF_DashSegInput *dash_inputs = *io_dash_inputs;
	GF_DashSegInput *dash_input = & dash_inputs[idx];
	char *uri_frag = strchr(dash_input->file_name, '#');
	FILE *t = gf_f64_open(dash_input->file_name, "rb");
	
	if (uri_frag) uri_frag[0] = 0;

	t = gf_f64_open(dash_input->file_name, "rb");
	if (!t) return GF_URL_ERROR;
	fclose(t);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (gf_isom_probe_file(dash_input->file_name)) {
		GF_ISOFile *file;
		u32 nb_track, j, max_nb_deps, cur_idx, rep_idx;
				
		strcpy(dash_input->szMime, "video/mp4");
		dash_input->dasher_create_init_segment = dasher_isom_create_init_segment;
		dash_input->dasher_input_classify = dasher_isom_classify_input;
		dash_input->dasher_get_components_info = dasher_isom_get_input_components_info;
		dash_input->dasher_segment_file = dasher_isom_segment_file;

		file = gf_isom_open(dash_input->file_name, GF_ISOM_OPEN_READ, NULL);
		if (!file) return gf_isom_last_error(NULL);
		nb_track = gf_isom_get_track_count(file);

		/*if this dash input file has only one track, or this has not the scalable track: let it be*/
		if ((nb_track == 1) || !gf_isom_has_scalable_layer(file)) {

			if (uri_frag) {
				u32 id = 0;
				if (!strnicmp(uri_frag+1, "trackID=", 8)) 
					id = atoi(uri_frag+9);
				else if (!strnicmp(uri_frag+1, "ID=", 3)) 
					id = atoi(uri_frag+4);
				else if (!stricmp(uri_frag+1, "audio") || !stricmp(uri_frag+1, "video")) {
					Bool check_video = !stricmp(uri_frag+1, "video") ? 1 : 0;
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
			}
			gf_isom_close(file);
			dash_input->moof_seqnum_increase = 0;
			return GF_OK;
		}

		max_nb_deps = 0;
		for (j = 0; j < nb_track; j++) {
			if (gf_isom_get_media_type(file, j+1)==GF_ISOM_MEDIA_VISUAL) {
				u32 count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_SCAL);
				if (count)
					max_nb_deps ++;
			}
		}
		//scalable input file, realloc
		j = *nb_dash_inputs + max_nb_deps;
		dash_inputs = gf_realloc(dash_inputs, sizeof(GF_DashSegInput) * j);
		memset(&dash_inputs[*nb_dash_inputs], 0, sizeof(GF_DashSegInput) * max_nb_deps);
		*io_dash_inputs = dash_inputs;

		dash_input = & dash_inputs[idx];
		dash_input->nb_representations = 1 + max_nb_deps;
		dash_input->idx_representations=0;
		rep_idx = 1;
		cur_idx = idx+1;

		for (j = 0; j < nb_track; j++) {
			GF_DashSegInput *di;
			u32 count, t, ref_track;
			count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_SCAL);
			/*base track, done above*/
			if (!count) {
				dash_input->trackNum = j+1;
				continue;
			}

			di = &dash_inputs [cur_idx];
			*nb_dash_inputs += 1;
			cur_idx++;
			//copy over values from base rep
			memcpy(di, dash_input, sizeof(GF_DashSegInput));

			/*representationID*/
			sprintf(di->representationID, "%s_%d", dash_input->representationID, cur_idx);
			di->trackNum = j+1;

			/*dependencyID - FIXME - the delaration of dependency and new dash_input entries should be in DEDENDENCY ORDER*/
			di->idx_representations = rep_idx;
			rep_idx ++;
			sprintf(di->dependencyID, "%s", dash_input->representationID); //base track
			di->lower_layer_track = dash_input->trackNum;
			for (t = 1; t < count; t++) {
				gf_isom_get_reference(file, j+1, GF_ISOM_REF_SCAL, t+1, &ref_track);
				if (j) strcat(di->dependencyID, " ");
				strcat(di->dependencyID, gf_dash_get_representationID(dash_inputs, *nb_dash_inputs, di->file_name, ref_track));

				di->lower_layer_track = ref_track;
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
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: MPEG-2 TS file %s has several programs - this is not supported in MPEG-DASH - skipping\n", dash_input->file_name));
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

	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: File %s not supported for dashing - skipping\n", dash_input->file_name));
	return GF_NOT_SUPPORTED;
}

static GF_Err write_mpd_header(FILE *mpd, const char *mpd_name, GF_Config *dash_ctx, GF_DashProfile profile, Bool is_mpeg2, const char *title, const char *source, const char *copyright, const char *moreInfoURL, const char **mpd_base_urls, u32 nb_mpd_base_urls, Bool dash_dynamic, u32 time_shift_depth, Double mpd_duration, Double mpd_update_period, Double min_buffer, u32 ast_shift_sec)
{
	u32 sec, frac;
#ifdef _WIN32_WCE
	SYSTEMTIME syst;
	FILETIME filet;
#else
	time_t gtime;
	struct tm *t;
#endif
	u32 h, m, i;
	Double s;

	gf_net_get_ntp(&sec, &frac);
	sec += ast_shift_sec;
	
	fprintf(mpd, "<?xml version=\"1.0\"?>\n");
	fprintf(mpd, "<!-- MPD file Generated with GPAC version "GPAC_FULL_VERSION" ");

#ifndef _WIN32_WCE
	gtime = sec - GF_NTP_SEC_1900_TO_1970;
	t = gmtime(&gtime);
	fprintf(mpd, " on %d-%02d-%02dT%02d:%02d:%02dZ", 1900+t->tm_year, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Generating MPD at time %d-%02d-%02dT%02d:%02d:%02dZ\n", 1900+t->tm_year, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec) );
#else
	GetSystemTime(&syst);
	fprintf(mpd, " on %d-%02d-%02dT%02d:%02d:%02dZ", syst.wYear, syst.wMonth, syst.wDay, syst.wHour, syst.wMinute, syst.wSecond);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Generating MPD at time %d-%02d-%02dT%02d:%02d:%02dZ\n", syst.wYear, syst.wMonth, syst.wDay, syst.wHour, syst.wMinute, syst.wSecond) );
#endif
	fprintf(mpd, "-->\n");

	/*TODO what should we put for minBufferTime */
	fprintf(mpd, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" minBufferTime=\"PT%fS\" type=\"%s\"", min_buffer, dash_dynamic ? "dynamic" : "static"); 
	if (dash_dynamic) {
		/*otherwise timeshift is infinite, use original availability start time*/
		if ((s32)time_shift_depth<0) {
			const char *opt = gf_cfg_get_key(dash_ctx, "DASH", "GenerationNTP");
			sscanf(opt, "%u", &sec);
			sec += ast_shift_sec;
		}
#ifdef _WIN32_WCE
		*(LONGLONG *) &filet = (sec - GF_NTP_SEC_1900_TO_1970) * 10000000 + TIMESPEC_TO_FILETIME_OFFSET;
		FileTimeToSystemTime(&filet, &syst);
		fprintf(mpd, " on %d-%02d-%02dT%02d:%02d:%02dZ", syst.wYear, syst.wMonth, syst.wDay, syst.wHour, syst.wMinute, syst.wSecond);
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Generating MPD at time %d-%02d-%02dT%02d:%02d:%02dZ\n", syst.wYear, syst.wMonth, syst.wDay, syst.wHour, syst.wMinute, syst.wSecond) );
#else
		gtime = sec - GF_NTP_SEC_1900_TO_1970;
		t = gmtime(&gtime);
		fprintf(mpd, " availabilityStartTime=\"%d-%02d-%02dT%02d:%02d:%02dZ\"", 1900+t->tm_year, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		if (ast_shift_sec) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] MPD AvailabilityStartTime %d-%02d-%02dT%02d:%02d:%02dZ\n", 1900+t->tm_year, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec) );
		}
#endif

		if ((s32)time_shift_depth>=0) {
			h = (u32) (time_shift_depth/3600);
			m = (u32) (time_shift_depth/60 - h*60);
			s = time_shift_depth - h*3600 - m*60;
			fprintf(mpd, " timeShiftBufferDepth=\"PT%dH%dM%.2fS\"", h, m, s);
		}
		if (mpd_update_period) {
			h = (u32) (mpd_update_period/3600);
			m = (u32) (mpd_update_period/60 - h*60);
			s = mpd_update_period - h*3600 - m*60;
			fprintf(mpd, " minimumUpdatePeriod=\"PT%dH%dM%.2fS\"", h, m, s);
		}
		
	} else {
		h = (u32) (mpd_duration/3600);
		m = (u32) (mpd_duration/60 - h*60);
		s = mpd_duration - h*3600 - m*60;
		fprintf(mpd, " mediaPresentationDuration=\"PT%dH%dM%.2fS\"", h, m, s);
	}

	if (profile==GF_DASH_PROFILE_LIVE) {
		fprintf(mpd, " profiles=\"urn:mpeg:dash:profile:%s:2011\"", is_mpeg2 ? "mp2t-simple" : "isoff-live");
	} else if (profile==GF_DASH_PROFILE_ONDEMAND) {
		fprintf(mpd, " profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\"");
	} else if (profile==GF_DASH_PROFILE_MAIN) {
		fprintf(mpd, " profiles=\"urn:mpeg:dash:profile:%s:2011\"", is_mpeg2 ? "mp2t-main" : "isoff-main");
	} else if (profile==GF_DASH_PROFILE_AVC264_LIVE) {
		fprintf(mpd, " profiles=\"urn:mpeg:dash:profile:isoff-live:2011, http://dashif.org/guildelines/dash264\"");
	} else if (profile==GF_DASH_PROFILE_AVC264_ONDEMAND) {
		fprintf(mpd, " profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011, http://dashif.org/guildelines/dash264\"");
	} else {
		fprintf(mpd, " profiles=\"urn:mpeg:dash:profile:full:2011\"");	
	}

	fprintf(mpd, ">\n");

	fprintf(mpd, " <ProgramInformation moreInformationURL=\"%s\">\n", moreInfoURL ? moreInfoURL : "http://gpac.sourceforge.net");	
	if (title) 
		fprintf(mpd, "  <Title>%s</Title>\n", title);
	else 
		fprintf(mpd, "  <Title>%s generated by GPAC</Title>\n", mpd_name);

	if (source) 
		fprintf(mpd, "  <Source>%s</Source>\n", source);
	if (copyright)
		fprintf(mpd, "  <Copyright>%s</Copyright>\n", copyright);
	fprintf(mpd, " </ProgramInformation>\n");

	for (i=0; i<nb_mpd_base_urls; i++) {
		fprintf(mpd, " <BaseURL>%s</BaseURL>\n", mpd_base_urls[i]);
	}
	fprintf(mpd, "\n");
	return GF_OK;
}

static GF_Err write_period_header(FILE *mpd, const char *szID, Double period_start, Double period_duration, Bool dash_dynamic)
{
	u32 h, m;
	Double s;

	fprintf(mpd, " <Period");
	if (szID) 
		fprintf(mpd, " id=\"%s\"", szID);
	
	if (period_start || dash_dynamic) {
		h = (u32) (period_start/3600);
		m = (u32) (period_start/60 - h*60);
		s = period_start - h*3600 - m*60;
		fprintf(mpd, " start=\"PT%dH%dM%.2fS\"", h, m, s);	
	}
	if (!dash_dynamic && period_duration) {
		h = (u32) (period_duration/3600);
		m = (u32) (period_duration/60 - h*60);
		s = period_duration - h*3600 - m*60;
		fprintf(mpd, " duration=\"PT%dH%dM%.2fS\"", h, m, s);	
	}
	fprintf(mpd, ">\n");	
	return GF_OK;
}

static GF_Err write_adaptation_header(FILE *mpd, GF_DashProfile profile, Bool use_url_template, u32 single_file_mode, GF_DashSegInput *first_rep, Bool bitstream_switching_mode, u32 max_width, u32 max_height, char *szMaxFPS, char *szLang, const char *szInitSegment)
{
	fprintf(mpd, "  <AdaptationSet segmentAlignment=\"true\"");
	if (bitstream_switching_mode) {
		fprintf(mpd, " bitstreamSwitching=\"true\"");
	}
	if (first_rep->group_id) {
		fprintf(mpd, " group=\"%d\"", first_rep->group_id);
	}
	if (max_width && max_height) {
		fprintf(mpd, " maxWidth=\"%d\" maxHeight=\"%d\"", max_width, max_height);
		if (strlen(szMaxFPS))
			fprintf(mpd, " maxFrameRate=\"%s\"", szMaxFPS);
		gf_media_reduce_aspect_ratio(&max_width, &max_height);
		fprintf(mpd, " par=\"%d:%d\"", max_width, max_height);
	}
	if (szLang && szLang[0]) {
		fprintf(mpd, " lang=\"%s\"", szLang);
	}
	/*this should be fixed to use info collected during segmentation process*/
	if (profile==GF_DASH_PROFILE_ONDEMAND) 
		fprintf(mpd, " subsegmentStartsWithSAP=\"1\"");

	if (!strcmp(first_rep->szMime, "video/mp2t"))
		fprintf(mpd, " subsegmentAlignment=\"true\"");

	fprintf(mpd, ">\n");
	
	if (first_rep) {
		u32 i;

		if (bitstream_switching_mode) {
			for (i=0; i<first_rep->nb_components; i++) {
				struct _dash_component *comp = &first_rep->components[i];
				if (comp->media_type == GF_ISOM_MEDIA_AUDIO) {
					fprintf(mpd, "   <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n", comp->channels);
				}
			}
		}

		/*set role*/
		if (strlen(first_rep->role)) {
			if (!strcmp(first_rep->role, "caption") || !strcmp(first_rep->role, "subtitle") || !strcmp(first_rep->role, "main")
				 || !strcmp(first_rep->role, "alternate") || !strcmp(first_rep->role, "supplementary") || !strcmp(first_rep->role, "commentary")
				 || !strcmp(first_rep->role, "dub")
			) {
				fprintf(mpd, "   <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"%s\"/>\n", first_rep->role);
			} else {
				fprintf(mpd, "   <Role schemeIdUri=\"urn:gpac:dash:role:2013\" value=\"%s\"/>\n", first_rep->role);
			}
		}


		if (first_rep->nb_components>1) {
			for (i=0; i<first_rep->nb_components; i++) {
				struct _dash_component *comp = &first_rep->components[i];

				fprintf(mpd, "   <ContentComponent id=\"%d\" ", comp->ID);
				switch (comp->media_type ) {
				case GF_ISOM_MEDIA_TEXT:
					fprintf(mpd, "contentType=\"text\" ");
					break;
				case GF_ISOM_MEDIA_VISUAL:
					fprintf(mpd, "contentType=\"video\" ");
					break;
				case GF_ISOM_MEDIA_AUDIO:
					fprintf(mpd, "contentType=\"audio\" ");
					break;
				case GF_ISOM_MEDIA_SCENE:
				case GF_ISOM_MEDIA_DIMS:
				default:
					fprintf(mpd, "contentType=\"application\" ");
					break;
				}
				/*if lang not specified at adaptationSet level, put it here*/
				if ((!szLang || !szLang[0]) && comp->szLang[0]) {
					fprintf(mpd, " lang=\"%s\"", comp->szLang);
				}
				fprintf(mpd, "/>\n");
			}
		}

		if (bitstream_switching_mode && !use_url_template && (single_file_mode!=1) && strlen(szInitSegment) ) {
			fprintf(mpd, "   <SegmentList>\n");	
			fprintf(mpd, "    <Initialization sourceURL=\"%s\"/>\n", gf_url_get_resource_name( szInitSegment ));	
			fprintf(mpd, "   </SegmentList>\n");	
		}
	}
	return GF_OK;
}

static GF_Err gf_dasher_init_context(GF_Config *dash_ctx, u32 *dynamic, u32 *timeShiftBufferDepth, const char *periodID, u32 ast_shift_sec)
{
	const char *opt;
	char szVal[100];
	Bool first_run = GF_FALSE;
	u32 sec, frac;
#ifndef _WIN32_WCE
	time_t gtime;
#endif

	if (!dash_ctx) return GF_BAD_PARAM;
	
	opt = gf_cfg_get_key(dash_ctx, "DASH", "SessionType");
	/*first run, init all options*/
	if (!opt) {
		first_run = GF_TRUE;
		sprintf(szVal, "%d", *timeShiftBufferDepth);
		gf_cfg_set_key(dash_ctx, "DASH", "SessionType", (*dynamic==2) ? "dynamic-debug" : ( *dynamic ? "dynamic" : "static" ) );
		gf_cfg_set_key(dash_ctx, "DASH", "TimeShiftBufferDepth", szVal);
		gf_cfg_set_key(dash_ctx, "DASH", "StoreParams", "yes");
	} else {
		*dynamic = 0;
		if (!strcmp(opt, "dynamic")) *dynamic = 1;
		else if (!strcmp(opt, "dynamic-debug")) *dynamic = 2;
		opt = gf_cfg_get_key(dash_ctx, "DASH", "TimeShiftBufferDepth");
		*timeShiftBufferDepth = atoi(opt);
		gf_cfg_set_key(dash_ctx, "DASH", "StoreParams", "no");
	}

	gf_cfg_set_key(dash_ctx, "DASH", "PeriodSwitch", "no");
	
	gf_net_get_ntp(&sec, &frac);

	if (first_run) {
#ifndef _WIN32_WCE
		gtime = sec - GF_NTP_SEC_1900_TO_1970;
		gf_cfg_set_key(dash_ctx, "DASH", "GenerationTime", asctime(gmtime(&gtime)));
#endif
		sprintf(szVal, "%u", sec);
		gf_cfg_set_key(dash_ctx, "DASH", "GenerationNTP", szVal);
		sprintf(szVal, "%u", frac);
		gf_cfg_set_key(dash_ctx, "DASH", "GenerationNTPFraction", szVal);
	
		if (periodID)
			gf_cfg_set_key(dash_ctx, "DASH", "PeriodID", periodID);
	} else {

		/*check if we change period*/
		if (periodID) {
			opt = gf_cfg_get_key(dash_ctx, "DASH", "PeriodID");
			if (!opt || strcmp(opt, periodID)) {
				gf_cfg_set_key(dash_ctx, "DASH", "PeriodID", periodID);
				gf_cfg_set_key(dash_ctx, "DASH", "PeriodSwitch", "yes");
			}
		}
	}

	/*perform cleanup of previous representations*/

	return GF_OK;
}

GF_EXPORT
u32 gf_dasher_next_update_time(GF_Config *dash_ctx, u32 mpd_update_time)
{
	Double max_dur = 0;
	Double safety_dur;
	Double ms_elapsed;
	u32 i, ntp_sec, frac, prev_sec, prev_frac;
	const char *opt, *section;

	opt = gf_cfg_get_key(dash_ctx, "DASH", "MaxSegmentDuration");
	if (!opt) return 0;

	safety_dur = atof(opt);
	if (safety_dur > (Double) mpd_update_time)
		safety_dur = (Double) mpd_update_time;


	opt = gf_cfg_get_key(dash_ctx, "DASH", "GenerationNTP");
	sscanf(opt, "%u", &prev_sec);
	opt = gf_cfg_get_key(dash_ctx, "DASH", "GenerationNTPFraction");
	sscanf(opt, "%u", &prev_frac);

	/*compute cumulated duration*/
	for (i=0; i<gf_cfg_get_section_count(dash_ctx); i++) {
		Double dur = 0;
		section = gf_cfg_get_section_name(dash_ctx, i);
		if (section && !strncmp(section, "Representation_", 15)) {
			opt = gf_cfg_get_key(dash_ctx, section, "CumulatedDuration");
			if (opt) dur = atof(opt);
			if (dur>max_dur) max_dur = dur;
		}
	}

	if (!max_dur) return 0;
	gf_net_get_ntp(&ntp_sec, &frac);

	ms_elapsed = frac;
	ms_elapsed -= prev_frac;
	ms_elapsed /= 0xFFFFFFFF;
	ms_elapsed *= 1000;
	ms_elapsed += (ntp_sec - prev_sec)*1000;

	/*check if we need to generate */
	if (ms_elapsed < (max_dur - safety_dur)*1000 ) {
		return (u32) (1000*max_dur - ms_elapsed);
	}
	return 0;
}

/*peform all file cleanup*/
static Bool gf_dasher_cleanup(GF_Config *dash_ctx, u32 dash_dynamic, u32 mpd_update_time, u32 time_shift_depth, Double dash_duration, u32 ast_shift_sec)
{
	Double max_dur = 0;
	Double elapsed = 0;
	Double safety_dur = MAX(mpd_update_time, (u32) dash_duration);
	u32 i, ntp_sec, frac, prev_sec;
	const char *opt, *section;
	GF_Err e;

	if (!dash_dynamic) return 1;

	opt = gf_cfg_get_key(dash_ctx, "DASH", "StoreParams");
	if (opt && !strcmp(opt, "yes")) return 1;

	opt = gf_cfg_get_key(dash_ctx, "DASH", "GenerationNTP");
	if (!opt) return 1;
	sscanf(opt, "%u", &prev_sec);

	/*compute cumulated duration*/
	for (i=0; i<gf_cfg_get_section_count(dash_ctx); i++) {
		Double dur = 0;
		section = gf_cfg_get_section_name(dash_ctx, i);
		if (section && !strncmp(section, "Representation_", 15)) {
			opt = gf_cfg_get_key(dash_ctx, section, "CumulatedDuration");
			if (opt) dur = atof(opt);
			if (dur>max_dur) max_dur = dur;
		}
	}
	if (!max_dur) return 1;
	gf_net_get_ntp(&ntp_sec, &frac);

	if (dash_dynamic==2) {
		elapsed = (u32)-1;
	} else {
		elapsed = ntp_sec;
		elapsed -= prev_sec;
		/*check if we need to generate */
		if (elapsed < max_dur - safety_dur ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Asked to regenerate segments before expiration of the current segment list, please wait %g seconds - ignoring\n", max_dur + prev_sec - ntp_sec ));
			return 0;
		}
		if (elapsed > max_dur) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Generating segments and MPD %g seconds too late\n", elapsed - (u32) max_dur));
		} else {
			/*generate as if max_dur has been reached*/
			elapsed = max_dur;
		}
	}

	/*cleanup old segments*/
	if ((s32) time_shift_depth >= 0) {
		for (i=0; i<gf_cfg_get_key_count(dash_ctx, "SegmentsStartTimes"); i++) {
			Double seg_time;
			Bool found=0;
			u32 j;
			const char *fileName = gf_cfg_get_key_name(dash_ctx, "SegmentsStartTimes", i);
			const char *MPDTime = gf_cfg_get_key(dash_ctx, "SegmentsStartTimes", fileName);
			if (!fileName) 
				break;

			seg_time = atof(MPDTime);
			seg_time += ast_shift_sec;
			seg_time += dash_duration + time_shift_depth;
			seg_time -= elapsed;
			/*safety gard of one second*/
			if (seg_time >= -1)
				break;

			if (dash_dynamic!=2) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Removing segment %s - %g sec too late\n", fileName, -seg_time));
			}

			e = gf_delete_file(fileName);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Could not remove file %s: %s\n", fileName, gf_error_to_string(e) ));
				break;
			}

			/*check all reps*/
			for (j=0; j<gf_cfg_get_section_count(dash_ctx); j++) {
				u32 k;
				const char *secName = gf_cfg_get_section_name(dash_ctx, j);
				if (!strstr(secName, "URLs_")) continue;
				for (k=0; k<gf_cfg_get_key_count(dash_ctx, secName); k++) {
					const char *entry = gf_cfg_get_key_name(dash_ctx, secName, k);
					const char *name = gf_cfg_get_key(dash_ctx, secName, entry);
					if (strstr(name, fileName)) {
						gf_cfg_set_key(dash_ctx, secName, entry, NULL);
						found=1;
						break;
					}
				}
				if (found) break;
			}

			gf_cfg_set_key(dash_ctx, "SegmentsStartTimes", fileName, NULL);
			i--;
		}
	}
	return 1;
}

static u32 gf_dash_get_dependency_bandwidth(GF_DashSegInput *inputs, u32 nb_dash_inputs, const char *file_name, u32 trackNum) {
	u32 i;

	for (i = 0; i < nb_dash_inputs; i++) {
		if (!strcmp(inputs[i].file_name, file_name) && (inputs[i].trackNum == trackNum))
			return inputs[i].bandwidth;
	}

	return 0; //we should never be here !!!!
}

/*dash segmenter*/
GF_EXPORT
GF_Err gf_dasher_segment_files(const char *mpdfile, GF_DashSegmenterInput *inputs, u32 nb_inputs, GF_DashProfile dash_profile, 
							   const char *mpd_title, const char *mpd_source, const char *mpd_copyright,
							   const char *mpd_moreInfoURL, const char **mpd_base_urls, u32 nb_mpd_base_urls, 
							   u32 use_url_template, Bool use_segment_timeline,  Bool single_segment, Bool single_file, GF_DashSwitchingMode bitstream_switching, 
							   Bool seg_at_rap, Double dash_duration, char *seg_name, char *seg_ext, u32 segment_marker_4cc,
							   Double frag_duration, s32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool frag_at_rap, const char *tmpdir,
							   GF_Config *dash_ctx, u32 dash_dynamic, u32 mpd_update_time, u32 time_shift_depth, Double subduration, Double min_buffer, 
							   u32 ast_shift_sec, u32 dash_scale, Bool fragments_in_memory, u32 initial_moof_sn, u64 initial_tfdt, Bool no_fragments_defaults, Bool pssh_moof)
{
	u32 i, j, segment_mode;
	char *sep, szSegName[GF_MAX_PATH], szSolvedSegName[GF_MAX_PATH], szTempMPD[GF_MAX_PATH];
	u32 cur_adaptation_set;
	u32 max_adaptation_set = 0;
	u32 cur_period;
	u32 max_period = 0;
	u32 cur_group_id = 0;
	u32 max_sap_type = 0;
	Bool none_supported = GF_TRUE;
	Bool has_mpeg2 = GF_FALSE;
	Bool has_role = GF_FALSE;
	Double presentation_duration = 0;
	GF_Err e = GF_OK;
	FILE *mpd = NULL;
	GF_DashSegInput *dash_inputs;
	GF_DASHSegmenterOptions dash_opts;
	u32 nb_dash_inputs;
	
	/*init dash context if needed*/
	if (dash_ctx) {

		e = gf_dasher_init_context(dash_ctx, &dash_dynamic, &time_shift_depth, NULL, ast_shift_sec);
		if (e) return e;
		if (dash_ctx) {
			Bool regenerate;
			const char *opt = gf_cfg_get_key(dash_ctx, "DASH", "MaxSegmentDuration");
			if (opt) {
				Double seg_dur = atof(opt);
/*
				if (seg_dur != dash_duration) {
					return GF_NOT_SUPPORTED;
				}
*/
				dash_duration = seg_dur;
			} else {
				char sOpt[100];
				sprintf(sOpt, "%f", dash_duration);
				gf_cfg_set_key(dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
			}

			/*peform all file cleanup*/
			regenerate = gf_dasher_cleanup(dash_ctx, dash_dynamic, mpd_update_time, time_shift_depth, dash_duration, ast_shift_sec);
			if (!regenerate) return GF_OK;
		}
	}

	/*copy over input files to our internal structure*/
	max_period = 0;
	/* first, we assume that nb_dash_inputs is equal to nb_inputs. This value will be recompute then*/
	nb_dash_inputs = nb_inputs;
	dash_inputs = gf_malloc(sizeof(GF_DashSegInput)*nb_dash_inputs);
	memset(dash_inputs, 0, sizeof(GF_DashSegInput)*nb_dash_inputs);
	j = 0;
	for (i=0; i<nb_inputs; i++) {
		u32 nb_diff;
		dash_inputs[j].file_name = inputs[i].file_name;
		strcpy(dash_inputs[j].representationID, inputs[i].representationID);
		strcpy(dash_inputs[j].periodID, inputs[i].periodID);
		strcpy(dash_inputs[j].role, inputs[i].role);
		dash_inputs[j].bandwidth = inputs[i].bandwidth;

		if (strlen(inputs[i].role) && strcmp(inputs[i].role, "main"))
			has_role = 1;

		if (!strlen(dash_inputs[j].periodID)) {
			max_period = 1;
			dash_inputs[j].period = 1;
			if (dash_dynamic) {
				strcpy(dash_inputs[j].periodID, "GENID_DEF");
			}
		}
		nb_diff = nb_dash_inputs;
		dash_inputs[j].moof_seqnum_increase = 2 + (u32) (dash_duration/frag_duration);
		e = gf_dash_segmenter_probe_input(&dash_inputs, &nb_dash_inputs, j);

		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: Cannot open file %s for dashing: %s\n", dash_inputs[i].file_name, gf_error_to_string(e) ));
		}

		if (!strcmp(dash_inputs[j].szMime, "video/mp2t")) has_mpeg2 = 1;

		nb_diff = nb_dash_inputs - nb_diff;
		j += 1+nb_diff;
 	}
	memset(&dash_opts, 0, sizeof(GF_DASHSegmenterOptions));

	/*set all default roles to main if needed*/
	if (has_role) {
		for (i=0; i<nb_dash_inputs; i++) {
			if (!strlen(dash_inputs[i].role))
				strcpy(dash_inputs[i].role, "main");
		}
	}

	for (i=0; i<nb_dash_inputs; i++) {
		if (dash_inputs[i].period) 
			continue;
		if (strlen(dash_inputs[i].periodID)) {
			max_period++;
			dash_inputs[i].period = max_period;

			for (j=i+1; j<nb_dash_inputs; j++) {
				if (!strcmp(dash_inputs[j].periodID, dash_inputs[i].periodID))
					dash_inputs[j].period = dash_inputs[i].period;
			}
		}
	}
	if (dash_dynamic && max_period>1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Multiple periods in live mode not supported\n"));
		e = GF_NOT_SUPPORTED;
		goto exit;
	}

	for (cur_period=0; cur_period<max_period; cur_period++) {
		/*classify all input in possible adaptation sets*/
		for (i=0; i<nb_dash_inputs; i++) {
			/*this file does not belong to our current period*/
			if (dash_inputs[i].period != cur_period+1) continue;
			
			/*this file already belongs to an adaptation set*/
			if (dash_inputs[i].adaptation_set) continue;

			/*we cannot fragment the input file*/
			if (!dash_inputs[i].dasher_input_classify || !dash_inputs[i].dasher_segment_file) {
				continue;
			}

			max_adaptation_set ++;
			dash_inputs[i].adaptation_set = max_adaptation_set;
			dash_inputs[i].nb_rep_in_adaptation_set = 1;

			e = dash_inputs[i].dasher_input_classify(dash_inputs, nb_dash_inputs, i, &cur_group_id, &max_sap_type);
			if (e) goto exit;
			none_supported = 0;
		}
		if (none_supported) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: None of the input types are supported for DASHing - nothing to do ...\n"));
			e = GF_OK;
			goto exit;
		}
	}

	/*check requested profiles can be generated, or adjust them*/
	if (max_sap_type>=3) {
		if (dash_profile) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: WARNING! Max SAP type %d detected\n\tswitching to FULL profile\n", max_sap_type));
		}
		dash_profile = 0;
	}
	if ((dash_profile==GF_DASH_PROFILE_LIVE) && !seg_name) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: WARNING! DASH Live profile requested but no -segment-name\n\tusing \"%%s_dash\" by default\n\n"));
		seg_name = "%s_dash";
	}

	/*if output id not in the current working dir, concatenate output path to segment name*/
	szSegName[0] = 0;
	if (seg_name) {
		if (gf_url_get_resource_path(mpdfile, szSegName)) {
			strcat(szSegName, seg_name);
			seg_name = szSegName;
		}
	}

	/*adjust params based on profiles*/
	switch (dash_profile) {
	case GF_DASH_PROFILE_LIVE:
		seg_at_rap = 1;
		use_url_template = 1;
		single_segment = single_file = 0;
		break;
	case GF_DASH_PROFILE_AVC264_LIVE:
		seg_at_rap = 1;
		use_url_template = 1;
		single_segment = single_file = 0;
		break;
	case GF_DASH_PROFILE_AVC264_ONDEMAND:
		if (seg_name) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment-name not allowed in DASH-AVC/264 onDemand profile.\n"));
			e = GF_NOT_SUPPORTED;
			goto exit;
		}
	case GF_DASH_PROFILE_ONDEMAND:
		seg_at_rap = 1;
		single_segment = 1;
		/*BS switching is meaningless in onDemand profile*/
		bitstream_switching = GF_DASH_BSMODE_NONE;
		use_url_template = single_file = 0;
		break;
	case GF_DASH_PROFILE_MAIN:
		seg_at_rap = 1;
		single_segment = 0;
		break;
	default:
		break;
	}

	segment_mode = single_segment ? 1 : (single_file ? 2 : 0);

	if (single_segment) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASH-ing file%s - single segment\nSubsegment duration %.3f - Fragment duration: %.3f secs\n", (nb_dash_inputs>1) ? "s" : "", dash_duration, frag_duration));
		subsegs_per_sidx = 0;
	} else {
		if (!seg_ext) seg_ext = "m4s";

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASH-ing file%s: %.2fs segments %.2fs fragments ", (nb_dash_inputs>1) ? "s" : "", dash_duration, frag_duration));
		if (subsegs_per_sidx<0) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("no sidx used"));
		} else if (subsegs_per_sidx) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("%d subsegments per sidx", subsegs_per_sidx));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("single sidx per segment"));
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\n"));
	}
	if (frag_at_rap) seg_at_rap = 1;

	if (seg_at_rap) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("Spliting segments %sat GOP boundaries\n", frag_at_rap ? "and fragments " : ""));
	}

	dash_opts.mpd_name = mpdfile;
	dash_opts.segments_start_with_rap = seg_at_rap;
	dash_opts.seg_ext = seg_ext;
	dash_opts.daisy_chain_sidx = daisy_chain_sidx;
	dash_opts.subsegs_per_sidx = subsegs_per_sidx;
	dash_opts.use_url_template = use_url_template;
	dash_opts.use_segment_timeline = use_segment_timeline;
	dash_opts.single_file_mode = segment_mode;
	dash_opts.fragments_start_with_rap = frag_at_rap;
	dash_opts.dash_scale = dash_scale ? dash_scale : 1000;
	dash_opts.tmpdir = tmpdir;
	dash_opts.dash_ctx = dash_ctx;
	dash_opts.time_shift_depth = (s32) time_shift_depth;
	dash_opts.segment_marker_4cc = segment_marker_4cc;
	dash_opts.inband_param_set = ((bitstream_switching == GF_DASH_BSMODE_INBAND) || (bitstream_switching == GF_DASH_BSMODE_SINGLE) ) ? 1 : 0;
	dash_opts.memory_mode = fragments_in_memory;
	dash_opts.pssh_moof = pssh_moof;

	dash_opts.segment_duration = dash_duration * 1000 / dash_scale;
	dash_opts.subduration = subduration * 1000 / dash_scale;
	dash_opts.fragment_duration = frag_duration * 1000 / dash_scale;
	dash_opts.initial_moof_sn = initial_moof_sn;
	dash_opts.initial_tfdt = initial_tfdt;
	dash_opts.no_fragments_defaults = no_fragments_defaults;
	

	for (cur_period=0; cur_period<max_period; cur_period++) {
		u32 first_in_period = 0;
		Double period_duration=0;
		for (i=0; i<nb_dash_inputs; i++) {
			/*this file does not belongs to any adaptation set*/
			if (dash_inputs[i].period != cur_period+1) continue;

			/*this file does not belongs to any adaptation set*/
			if (!dash_inputs[i].adaptation_set) continue;

			if (!first_in_period) {
				first_in_period = i+1;
			}

			if (dash_inputs[i].dasher_get_components_info) {
				e = dash_inputs[i].dasher_get_components_info(&dash_inputs[i], &dash_opts);
				if (e) goto exit;
			}
			if (dash_inputs[i].duration > period_duration)
				period_duration = dash_inputs[i].duration;
		}
		if (first_in_period)
			dash_inputs[first_in_period-1].period_duration = period_duration;

		presentation_duration += period_duration;
	}

	strcpy(szTempMPD, mpdfile);
	if (dash_dynamic) strcat(szTempMPD, ".tmp");
	
	mpd = gf_f64_open(szTempMPD, "wt");
	if (!mpd) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MPD] Cannot open MPD file %s for writing\n", szTempMPD));
		e = GF_IO_ERR;
		goto exit;
	}

	dash_opts.mpd = mpd;

	e = write_mpd_header(mpd, mpdfile, dash_ctx, dash_profile, has_mpeg2, mpd_title, mpd_source, mpd_copyright, mpd_moreInfoURL, (const char **) mpd_base_urls, nb_mpd_base_urls, dash_dynamic, time_shift_depth, presentation_duration, mpd_update_time, min_buffer, ast_shift_sec);
	if (e) goto exit;

	for (cur_period=0; cur_period<max_period; cur_period++) {
		Double period_duration = 0;
		const char *id=NULL;
		/*for each identified adaptationSets, write MPD and perform segmentation of input files*/
		for (i=0; i< nb_dash_inputs; i++) {
			if (dash_inputs[i].adaptation_set && (dash_inputs[i].period==cur_period+1)) {
				period_duration = dash_inputs[i].period_duration;
				id = dash_inputs[i].periodID;
				break;
			}
		}
		e = write_period_header(mpd, id, 0.0, period_duration, dash_dynamic);
		if (e) goto exit;

		/*for each identified adaptationSets, write MPD and perform segmentation of input files*/
		for (cur_adaptation_set=0; cur_adaptation_set < max_adaptation_set; cur_adaptation_set++) {
			char szInit[GF_MAX_PATH], tmp[GF_MAX_PATH];
			u32 first_rep_in_set=0;
			u32 max_width = 0;
			u32 max_height = 0;
			u32 fps_num = 0;
			u32 fps_denum = 0;
			Bool use_bs_switching = bitstream_switching ? 1 : 0;
			char szLang[4];
			char szFPS[100];
			Bool is_first_rep=0;
			Bool skip_init_segment_creation = 0;

			while (dash_inputs[first_rep_in_set].adaptation_set!=cur_adaptation_set+1)
				first_rep_in_set++;

			/*not in our period*/
			if (dash_inputs[first_rep_in_set].period != cur_period+1)
				continue;

			strcpy(tmp, mpdfile);
			sep = strrchr(tmp, '.');
			if (sep) sep[0] = 0;

			if (max_adaptation_set==1) {
				strcpy(szInit, tmp);
				strcat(szInit, "_init.mp4");
			} else {
				sprintf(szInit, "%s_set%d_init.mp4", tmp, cur_adaptation_set+1);
			}
			/*unless asked to do BS switching on single rep, don't do it ...*/
			if ((bitstream_switching < GF_DASH_BSMODE_SINGLE) && dash_inputs[first_rep_in_set].nb_rep_in_adaptation_set==1)
				use_bs_switching = 0;

			if (! use_bs_switching) {
				skip_init_segment_creation = 1;
			}
			else if (! dash_inputs[first_rep_in_set].dasher_create_init_segment) {
				skip_init_segment_creation = 1;
				strcpy(szInit, "");
			}
			/*if we already wrote the InitializationSegment, get rid of it (no overwrite) and let the dashing routine open it*/
			else if (dash_ctx) {
				char RepSecName[GF_MAX_PATH];
				const char *init_seg_name;
				sprintf(RepSecName, "Representation_%s", dash_inputs[first_rep_in_set].representationID);
				if ((init_seg_name = gf_cfg_get_key(dash_ctx, RepSecName, "InitializationSegment")) != NULL) {
					skip_init_segment_creation = 1;
				}
			}

			if (!skip_init_segment_creation) {
				Bool disable_bs_switching = 0;
				e = dash_inputs[first_rep_in_set].dasher_create_init_segment(dash_inputs, nb_dash_inputs, cur_adaptation_set+1, szInit, tmpdir, &dash_opts, &disable_bs_switching);
				if (e) goto exit;
				if (disable_bs_switching)
					use_bs_switching = 0;
			}

			dash_opts.bs_switch_segment_file = use_bs_switching ? szInit : NULL;
			dash_opts.use_url_template = seg_name ? use_url_template : 0;

			szFPS[0] = 0;
			szLang[0] = 0;
			/*compute some max defaults*/
			for (i=0; i<nb_dash_inputs && !e; i++) {

				if (dash_inputs[i].adaptation_set!=cur_adaptation_set+1)
					continue;

				for (j=0; j<dash_inputs[i].nb_components; j++) {
					if (dash_inputs[i].components[j].width > max_width) 
						max_width = dash_inputs[i].components[j].width;
					if (dash_inputs[i].components[j].height > max_height) 
						max_height = dash_inputs[i].components[j].height;

					if (! fps_num || !fps_denum) {
						fps_num = dash_inputs[i].components[j].fps_num;
						fps_denum = dash_inputs[i].components[j].fps_denum;
					}
					else if (fps_num * dash_inputs[i].components[j].fps_denum < dash_inputs[i].components[j].fps_num * fps_denum) {
						fps_num = dash_inputs[i].components[j].fps_num;
						fps_denum = dash_inputs[i].components[j].fps_denum;
					}
				}
			}

			if (max_width && max_height) {
				gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);
				if (fps_denum>1) 
					sprintf(szFPS, "%d/%d", fps_num, fps_denum);
				else if (fps_num)
					sprintf(szFPS, "%d", fps_num);
			}

			e = write_adaptation_header(mpd, dash_profile, use_url_template, segment_mode, &dash_inputs[first_rep_in_set], use_bs_switching, max_width, max_height, szFPS, szLang, szInit);
			if (e) goto exit;

			is_first_rep = 1;
			for (i=0; i<nb_dash_inputs && !e; i++) {
				char szOutName[GF_MAX_PATH], *segment_name;

				if (dash_inputs[i].adaptation_set!=cur_adaptation_set+1)
					continue;

				segment_name = seg_name;

				strcpy(szOutName, gf_url_get_resource_name(dash_inputs[i].file_name));
				sep = strrchr(szOutName, '.');
				if (sep) sep[0] = 0;

				dash_opts.variable_seg_rad_name = 0;
				if (seg_name) {
					if (strstr(seg_name, "%s")) {
						sprintf(szSolvedSegName, seg_name, szOutName);
						dash_opts.variable_seg_rad_name = 1;
						if (dash_inputs[i].single_track_num) {
							char szTkNum[50];
							sprintf(szTkNum, "_track%d_", dash_inputs[i].single_track_num);
							strcat(szSolvedSegName, szTkNum);
						}
					} else {
						strcpy(szSolvedSegName, seg_name);
					}
					segment_name = szSolvedSegName;
				}
				if ((dash_inputs[i].trackNum || dash_inputs[i].single_track_num)
					&& (!seg_name || (!strstr(seg_name, "$RepresentationID$") && !strstr(seg_name, "$ID$"))) 
				) {
					char tmp[10];
					sprintf(tmp, "_track%d", dash_inputs[i].trackNum ? dash_inputs[i].trackNum : dash_inputs[i].single_track_num);
					strcat(szOutName, tmp);
				}
				strcat(szOutName, "_dash");

				if (gf_url_get_resource_path(mpdfile, tmp)) {
					strcat(tmp, szOutName);
					strcpy(szOutName, tmp);
				}
				if (segment_name && dash_inputs[i].trackNum && !strstr(seg_name, "$RepresentationID$") && !strstr(seg_name, "$ID$")) {
					char tmp[10];
					sprintf(tmp, "_track%d_", dash_inputs[i].trackNum);
					strcat(segment_name, tmp);
				}
				dash_opts.seg_rad_name = segment_name;

				/*in scalable case, we need also the bandwidth of dependent representation*/
				if (strlen(dash_inputs[i].dependencyID)) {
					dash_inputs[i].dependency_bandwidth = gf_dash_get_dependency_bandwidth(dash_inputs, nb_dash_inputs, dash_inputs[i].file_name, dash_inputs[i].lower_layer_track);
				}

				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("DASHing file %s\n", dash_inputs[i].file_name));
				e = dash_inputs[i].dasher_segment_file(&dash_inputs[i], szOutName, &dash_opts, is_first_rep);

				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Error while DASH-ing file: %s\n", gf_error_to_string(e)));
					goto exit;
				}
				is_first_rep = 0;
			}
			/*close adaptation set*/
			fprintf(mpd, "  </AdaptationSet>\n");
		}

		fprintf(mpd, " </Period>\n");
	}
	fprintf(mpd, "</MPD>");

exit:
	if (mpd) {
		fclose(mpd);
		if (!e && dash_dynamic)
			gf_move_file(szTempMPD, mpdfile);
	}
	gf_free(dash_inputs);
	return e;
}



