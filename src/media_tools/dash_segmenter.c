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
#ifndef _WIN32_WCE
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
	char szLang[4];
};	

typedef struct
{
	Double segment_duration;
	Bool segments_start_with_rap;
	FILE *mpd;
	const char *mpd_name, *seg_rad_name, *seg_ext;
	s32 subsegs_per_sidx;
	Bool daisy_chain_sidx;
	Bool use_url_template;
	u32 single_file_mode;
	const char *bs_switch_segment_file;

	/*set if seg_rad_name depends on input file name (had %s in it). In this case, SegmentTemplate cannot be used at adaptation set level*/
	Bool variable_seg_rad_name;

	Bool fragments_start_with_rap;
	Double fragment_duration;

	const char *dash_ctx_file;

	const char *tmpdir;
} GF_DASHSegmenterOptions;

struct _dash_segment_input
{
	char *file_name;
	char representationID[100];
	char periodID[100];

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
	GF_Err ( *dasher_create_init_segment) (GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 adaptation_set, char *szInitName, const char *tmpdir, Bool *disable_bs_switching);
	GF_Err ( *dasher_segment_file) (GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenterOptions *opts, Bool first_in_set);

	/*shall be set after call to dasher_input_classify*/
	char szMime[50];

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
					char_template += strlen(seg_rad_name+char_template);	\
					sep[0] = '$';	\
				}	\
			}	\
			char_template+=1;	\

GF_EXPORT
GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *output_file_name, const char *rep_id, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number)
{
	char szFmt[20];
	Bool has_number=0;
	Bool is_index = (seg_type==GF_DASH_TEMPLATE_REPINDEX) ? 1 : 0;
	Bool is_init = (seg_type==GF_DASH_TEMPLATE_INITIALIZATION) ? 1 : 0;
	Bool is_template = (seg_type==GF_DASH_TEMPLATE_TEMPLATE) ? 1 : 0;
	Bool is_init_template = (seg_type==GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE) ? 1 : 0;
	Bool needs_init=((is_init || is_init_template) && !is_bs_switching) ? 1 : 0;
	u32 char_template = 0;
	char tmp[100];
	strcpy(segment_name, "");

	if (seg_rad_name && (strstr(seg_rad_name, "$RepresentationID$") || strstr(seg_rad_name, "$Bandwidth$"))) 
		needs_init = 0;
	
	if (!seg_rad_name) {
		sprintf(segment_name, "%s.%s", output_file_name, seg_ext);
		return GF_OK;
	}

	while (1) {
		char c = seg_rad_name[char_template];
		if (!c) break;

		
		if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$RepresentationID$", 18)) {
			char_template+=18;
			strcat(segment_name, rep_id);
			needs_init=0;
		}
		else if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$Bandwidth", 10)) {
			EXTRACT_FORMAT(10);

			sprintf(tmp, szFmt, bandwidth);
			strcat(segment_name, tmp);
			needs_init=0;
		}
		else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Time", 5)) {
			EXTRACT_FORMAT(5);
			if (is_init || is_init_template) continue;
			/*replace %d to LLD*/
			szFmt[strlen(szFmt)-1]=0;
			strcat(szFmt, &LLD[1]);
			sprintf(tmp, szFmt, start_time);
			strcat(segment_name, tmp);
		}
		else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Number", 7)) {
			EXTRACT_FORMAT(7);

			if (is_init || is_init_template) continue;
			sprintf(tmp, szFmt, segment_number);
			strcat(segment_name, tmp);
			has_number=1;
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$Init=", 6)) {
			char *sep = strchr(seg_rad_name + char_template+6, '$');
			if (sep) sep[0] = 0;
			if (is_init || is_init_template) {
				strcat(segment_name, seg_rad_name + char_template+6);
				needs_init=0;
			}
			char_template += strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$Index=", 7)) {
			char *sep = strchr(seg_rad_name + char_template+7, '$');
			if (sep) sep[0] = 0;
			if (is_index) {
				strcat(segment_name, seg_rad_name + char_template+6);
				needs_init=0;
			}
			char_template += strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}
		
		else {
			char_template+=1;
			sprintf(tmp, "%c", c);
			strcat(segment_name, tmp);
		}
	}

	if (is_template && !strstr(seg_rad_name, "$Number"))
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



#ifndef GPAC_DISABLE_ISOM

GF_Err gf_media_get_rfc_6381_codec_name(GF_ISOFile *movie, u32 track, char *szCodec)
{
	GF_ESD *esd;
	GF_AVCConfig *avcc;
	GF_AVCConfigSlot *sps;
	u32 subtype = gf_isom_is_media_encrypted(movie, track, 1);
	if (!subtype) subtype = gf_isom_get_media_subtype(movie, track, 1);

	switch (subtype) {
	case GF_ISOM_SUBTYPE_MPEG4:
		esd = gf_isom_get_esd(movie, track, 1);
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_AUDIO:
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				/*5 first bits of AAC config*/
				u8 audio_object_type = (esd->decoderConfig->decoderSpecificInfo->data[0] & 0xF8) >> 3;
				sprintf(szCodec, "mp4a.%02x.%02x", esd->decoderConfig->objectTypeIndication, audio_object_type);
			} else {
				sprintf(szCodec, "mp4a.%02x", esd->decoderConfig->objectTypeIndication);
			}
			break;
		case GF_STREAM_VISUAL:
#ifndef GPAC_DISABLE_AV_PARSERS
			if (esd->decoderConfig->decoderSpecificInfo) {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				sprintf(szCodec, "mp4v.%02x.%02x", esd->decoderConfig->objectTypeIndication, dsi.VideoPL);
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
	case GF_ISOM_SUBTYPE_SVC_H264:
		avcc = gf_isom_avc_config_get(movie, track, 1);
		sps = gf_list_get(avcc->sequenceParameterSets, 0);
		sprintf(szCodec, "%s.%02x%02x%02x", gf_4cc_to_str(subtype), (u8) sps->data[1], (u8) sps->data[2], (u8) sps->data[3]);
		gf_odf_avc_cfg_del(avcc);
		return GF_OK;
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
	u32 FragmentLength;
	u32 OriginalTrack;
	u32 finalSampleDescriptionIndex;
	u32 TimeScale, MediaType, DefaultDuration, InitialTSOffset;
	u64 last_sample_cts, next_sample_dts;
	Bool all_sample_raps, splitable;
	u32 split_sample_dts_shift;
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

static GF_Err gf_media_isom_segment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec, GF_DASHSegmenterOptions *dash_cfg, const char *rep_id, Bool first_in_set)
{
	u8 NbBits;
	u32 i, TrackNum, descIndex, j, count, nb_sync, ref_track_id, nb_tracks_done;
	u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess, nb_samp, nb_done;
	u32 nb_video, nb_audio, nb_text, nb_scene;
	u8 defaultPadding;
	u16 defaultDegradationPriority;
	GF_Err e;
	char sOpt[100], sKey[100];
	char szCodecs[200], szCodec[100];
	u32 cur_seg, fragment_index, max_sap_type;
	GF_ISOFile *output, *bs_switch_segment;
	GF_ISOSample *sample, *next;
	GF_List *fragmenters;
	u32 MaxFragmentDuration, MaxSegmentDuration, SegmentDuration, maxFragDurationOverSegment;
	Double segment_start_time, file_duration, period_duration, max_segment_duration;
	u32 nb_segments, width, height, sample_rate, nb_channels, sar_w, sar_h, fps_num, fps_denum, startNumber;
	char langCode[5];
	u32 index_start_range, index_end_range;
	Bool force_switch_segment = 0;
	Bool switch_segment = 0;
	Bool split_seg_at_rap = dash_cfg ? dash_cfg->segments_start_with_rap : 0;
	Bool split_at_rap = 0;
	Bool has_rap = 0;
	Bool next_sample_rap = 0;
	Bool flush_all_samples = 0;
	Bool simulation_pass = 0;
	Bool init_segment_deleted = 0;
	u64 last_ref_cts = 0;
	u64 start_range, end_range, file_size, init_seg_size, ref_track_first_dts, ref_track_next_cts;
	u32 tfref_timescale = 0;
	u32 bandwidth = 0;
	GF_ISOMTrackFragmenter *tf, *tfref;
	FILE *mpd_segs = NULL;
	char SegmentName[GF_MAX_PATH];
	char RepKeyName[GF_MAX_PATH];
	const char *opt;
	Double max_track_duration = 0;
	GF_Config *dash_ctx = NULL;
	Bool store_dash_params = 0;
	Bool dash_moov_setup = 0;
	Bool segments_start_with_sap = 1;
	Bool first_sample_in_segment = 0;
	u32 *segments_info = NULL;
	u32 nb_segments_info = 0;
	u32 timeline_timescale=1000;
	Bool audio_only = 1;
	Bool is_bs_switching = 0;
	Bool use_url_template = dash_cfg ? dash_cfg->use_url_template : 0;
	const char *seg_rad_name = dash_cfg ? dash_cfg->seg_rad_name : NULL;
	const char *seg_ext = dash_cfg ? dash_cfg->seg_ext : NULL;
	const char *bs_switching_segment_name = NULL;

	SegmentName[0] = 0;
	SegmentDuration = 0;
	nb_samp = 0;
	fragmenters = NULL;
	if (!seg_ext) seg_ext = "m4s";

	bs_switch_segment = NULL;
	if (dash_cfg && dash_cfg->bs_switch_segment_file) {
		bs_switch_segment = gf_isom_open(dash_cfg->bs_switch_segment_file, GF_ISOM_OPEN_READ, NULL);
		if (bs_switch_segment) {
			bs_switching_segment_name = gf_url_get_resource_name(dash_cfg->bs_switch_segment_file);
			is_bs_switching = 1;
		}
	}
	
	sprintf(RepKeyName, "Representation_%s", rep_id ? rep_id : "");

	bandwidth = 0;
	file_duration = 0;

	/*todo: adjust number based on current time and timeShiftBuffer*/
	startNumber = 1;

	//create output file
	if (dash_cfg) {

		/*need to precompute bandwidth*/
		if (seg_rad_name && strstr(seg_rad_name, "$Bandwidth$")) {
			for (i=0; i<gf_isom_get_track_count(input); i++) {
				Double tk_dur = (Double) gf_isom_get_media_duration(input, i+1);
				tk_dur /= gf_isom_get_media_timescale(input, i+1);
				if (file_duration < tk_dur ) {
					file_duration = tk_dur;
				}
			}
			bandwidth = (u32) (gf_isom_get_file_size(input) / file_duration * 8);
		}

		if (dash_cfg->dash_ctx_file) {
			dash_ctx = gf_cfg_new(NULL, dash_cfg->dash_ctx_file);

			if (dash_ctx) {
				opt = gf_cfg_get_key(dash_ctx, RepKeyName, "Setup");
				if (!opt || stricmp(opt, "yes") ) {
					store_dash_params=1;
				}
			}
		}

		opt = dash_ctx ? gf_cfg_get_key(dash_ctx, RepKeyName, "InitializationSegment") : NULL;
		if (opt) {
			output = gf_isom_open(opt, GF_ISOM_OPEN_CAT_FRAGMENTS, NULL);
			dash_moov_setup = 1;
		} else {
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, rep_id, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0);
			output = gf_isom_open(SegmentName, GF_ISOM_OPEN_WRITE, NULL);
		}
		if (!output) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISOBMF DASH] Cannot open %s for writing\n", opt ? opt : SegmentName));
			e = gf_isom_last_error(NULL);
			goto err_exit;
		}

		if (store_dash_params) {
			const char *name;

			if (!gf_cfg_get_key(dash_ctx, "DASH", "SegmentTemplate"))
				gf_cfg_set_key(dash_ctx, "DASH", "SegmentTemplate", seg_rad_name);
			gf_cfg_set_key(dash_ctx, RepKeyName, "Source", gf_isom_get_filename(input) );

			gf_cfg_set_key(dash_ctx, RepKeyName, "Setup", "yes");
			name = SegmentName;
			if (bs_switch_segment) name = gf_isom_get_filename(bs_switch_segment);
			gf_cfg_set_key(dash_ctx, RepKeyName, "InitializationSegment", name);

			/*store BS flag per rep - it should be stored per adaptation set but we dson't have a key for adaptation sets right now*/
			if (/*first_in_set && */ is_bs_switching)
				gf_cfg_set_key(dash_ctx, RepKeyName, "BitstreamSwitching", "yes");
		} else if (dash_ctx) {
			opt = gf_cfg_get_key(dash_ctx, RepKeyName, "BitstreamSwitching");
			if (opt && !strcmp(opt, "yes")) {
				is_bs_switching = 1;
				bs_switch_segment = output;
				bs_switching_segment_name = gf_url_get_resource_name(gf_isom_get_filename(bs_switch_segment));
			}
		}
		mpd_segs = gf_temp_file_new();
	} else {
		output = gf_isom_open(output_file, GF_ISOM_OPEN_WRITE, NULL);
		if (!output) return gf_isom_last_error(NULL);
	}
	
	nb_sync = 0;
	nb_samp = 0;
	fragmenters = gf_list_new();

	if (! dash_moov_setup) {
		e = gf_isom_clone_movie(input, output, 0, 0);
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

	MaxFragmentDuration = (u32) (max_duration_sec * 1000);	
	MaxSegmentDuration = (u32) (dash_cfg->segment_duration * 1000);

	if (dash_cfg) {
		/*in single segment mode, only one big SIDX is written between the end of the moov and the first fragment. 
		To speed-up writing, we do a first fragmentation pass without writing any sample to compute the number of segments and fragments per segment
		in order to allocate / write to file the sidx before the fragmentation. The sidx will then be rewritten when closing the last segment*/
		if (dash_cfg->single_file_mode==1) {
			simulation_pass = 1;
			seg_rad_name = NULL;
		}
		/*if single file is requested, store all segments in the same file*/
		else if (dash_cfg->single_file_mode==2) {
			seg_rad_name = NULL;
		}
	}
	index_start_range = index_end_range = 0;	

	tfref = NULL;
	file_duration = 0;
	width = height = sample_rate = nb_channels = sar_w = sar_h = fps_num = fps_denum = 0;
	langCode[0]=0;
	langCode[4]=0;
	szCodecs[0] = 0;

	nb_video = nb_audio = nb_text = nb_scene = 0;
	//duplicates all tracks
	for (i=0; i<gf_isom_get_track_count(input); i++) {
		u32 _w, _h, _sr, _nb_ch;

		u32 mtype = gf_isom_get_media_type(input, i+1);
		if (mtype == GF_ISOM_MEDIA_HINT) continue;

		if (! dash_moov_setup) {
			e = gf_isom_clone_track(input, i+1, output, 0, &TrackNum);
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

		if (mtype == GF_ISOM_MEDIA_VISUAL) nb_video++;
		else if (mtype == GF_ISOM_MEDIA_AUDIO) nb_audio++;
		else if (mtype == GF_ISOM_MEDIA_TEXT) nb_text++;
		else if (mtype == GF_ISOM_MEDIA_SCENE) nb_scene++;
		else if (mtype == GF_ISOM_MEDIA_DIMS) nb_scene++;

		if (mtype != GF_ISOM_MEDIA_AUDIO) audio_only = 0;

		//setup fragmenters
		if (! dash_moov_setup) {
			gf_isom_get_fragment_defaults(input, i+1,
									 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);

			//new initialization segment, setup fragmentation
			e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum),
						defaultDescriptionIndex, defaultDuration,
						defaultSize, (u8) defaultRandomAccess,
						defaultPadding, defaultDegradationPriority);
			if (e) goto err_exit;
		} else {
			gf_isom_get_fragment_defaults(output, TrackNum,
									 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
		}

		gf_media_get_rfc_6381_codec_name(output, TrackNum, szCodec);
		if (strlen(szCodecs)) strcat(szCodecs, ",");
		strcat(szCodecs, szCodec);

		GF_SAFEALLOC(tf, GF_ISOMTrackFragmenter);
		tf->TrackID = gf_isom_get_track_id(output, TrackNum);
		tf->SampleCount = gf_isom_get_sample_count(input, i+1);
		tf->OriginalTrack = i+1;
		tf->TimeScale = gf_isom_get_media_timescale(input, i+1);
		tf->MediaType = gf_isom_get_media_type(input, i+1);
		tf->DefaultDuration = defaultDuration;

		if (max_track_duration < gf_isom_get_track_duration(input, i+1)) {
			max_track_duration = (Double) gf_isom_get_track_duration(input, i+1);
		}

		if (gf_isom_get_sync_point_count(input, i+1)>nb_sync) { 
			tfref = tf;
			nb_sync = gf_isom_get_sync_point_count(input, i+1);
		} else if (!gf_isom_has_sync_points(input, i+1)) {
			tf->all_sample_raps = 1;
		}

		tf->finalSampleDescriptionIndex = 1;

		/*figure out if we have an initial TS*/
		if (!dash_moov_setup) {
			if (gf_isom_get_edit_segment_count(input, i+1)) {
				u64 EditTime, SegmentDuration, MediaTime;
				u8 EditMode;
				gf_isom_get_edit_segment(input, i+1, 1, &EditTime, &SegmentDuration, &MediaTime, &EditMode);
				if (EditMode==GF_ISOM_EDIT_EMPTY) {
					tf->InitialTSOffset = (u32) (SegmentDuration * tf->TimeScale / gf_isom_get_timescale(input));
				}
				/*and remove edit segments*/
				gf_isom_remove_edit_segments(output, TrackNum);
			}

			gf_isom_set_brand_info(output, GF_4CC('i','s','o','5'), 1);
			gf_isom_modify_alternate_brand(output, GF_4CC('d','a','s','h'), 1);

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
			opt = (char *)gf_cfg_get_key(dash_ctx, RepKeyName, sKey);
			if (opt) tf->InitialTSOffset = atoi(opt);
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

		nb_samp += count;
	}


	if (!tfref) tfref = gf_list_get(fragmenters, 0);
	tfref->is_ref_track = 1;
	tfref_timescale = tfref->TimeScale;
	ref_track_id = tfref->TrackID;
	if (tfref->all_sample_raps) split_seg_at_rap = 1;


	if (!dash_moov_setup) {
		max_track_duration /= gf_isom_get_timescale(input);
		max_track_duration *= gf_isom_get_timescale(output);
		gf_isom_set_movie_duration(output, (u64) max_track_duration);
	}

	//if single segment, add msix brand if we use indexes
	gf_isom_modify_alternate_brand(output, GF_4CC('m','s','i','x'), (dash_cfg && (dash_cfg->single_file_mode==1) && (dash_cfg->subsegs_per_sidx>=0)) ? 1 : 0);

	//flush movie
	e = gf_isom_finalize_for_fragment(output, dash_cfg ? 1 : 0);
	if (e) goto err_exit;

	start_range = 0;
	file_size = gf_isom_get_file_size(output);
	end_range = file_size - 1;
	init_seg_size = file_size;

	if (dash_ctx) {
		if (store_dash_params) {
			char szVal[1024];
			sprintf(szVal, LLU, init_seg_size);
			gf_cfg_set_key(dash_ctx, RepKeyName, "InitializationSegmentSize", szVal);
		} else {
			const char *opt = gf_cfg_get_key(dash_ctx, RepKeyName, "InitializationSegmentSize");
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
	if (dash_cfg) switch_segment=1;

	if (!seg_rad_name) use_url_template = 0;

	cur_seg=1;
	fragment_index=1;
	period_duration = 0;
	split_at_rap = 0;
	has_rap = 0;
	next_sample_rap = 0;
	flush_all_samples = 0;
	force_switch_segment = 0;
	max_sap_type = 0;
	ref_track_next_cts = 0;

	/*setup previous URL list*/
	if (dash_ctx) {
		const char *opt;
		count = gf_cfg_get_key_count(dash_ctx, "URLs");
		for (i=0; i<count; i++) {
			const char *key_name = gf_cfg_get_key_name(dash_ctx, "URLs", i);
			opt = gf_cfg_get_key(dash_ctx, "URLs", key_name);
			fprintf(mpd_segs, "    %s\n", opt);	
		}

		opt = gf_cfg_get_key(dash_ctx, RepKeyName, "NextSegmentIndex");
		if (opt) cur_seg = atoi(opt);
		opt = gf_cfg_get_key(dash_ctx, RepKeyName, "NextFragmentIndex");
		if (opt) fragment_index = atoi(opt);
		opt = gf_cfg_get_key(dash_ctx, RepKeyName, "CumulatedDuration");
		if (opt) period_duration = atof(opt);
	}
	gf_isom_set_next_moof_number(output, fragment_index);


	max_segment_duration = 0;

	while ( (count = gf_list_count(fragmenters)) ) {

		if (switch_segment) {
			SegmentDuration = 0;
			switch_segment = 0;
			first_sample_in_segment = 1;

			if (simulation_pass) {
				segments_info = gf_realloc(segments_info, sizeof(u32) * (nb_segments_info+1) );
				segments_info[nb_segments_info] = 0;
				nb_segments_info++;
				e = GF_OK;
			} else {
				start_range = gf_isom_get_file_size(output);
				if (seg_rad_name) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, is_bs_switching, SegmentName, output_file, rep_id, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, (u64) (segment_start_time * timeline_timescale), bandwidth, cur_seg);
					e = gf_isom_start_segment(output, SegmentName);

					/*we are in bitstream switching mode, delete init segment*/
					if (is_bs_switching && !init_segment_deleted) {
						init_segment_deleted = 1;
						gf_delete_file(gf_isom_get_filename(output));
					}

					if (!use_url_template) {
						const char *name = gf_url_get_resource_name(SegmentName);
						fprintf(mpd_segs, "    <SegmentURL media=\"%s\"/>\n", name );	
						if (dash_ctx) {
							char szKey[100], szVal[4046];
							sprintf(szKey, "UrlInfo%d", gf_cfg_get_key_count(dash_ctx, "URLs") + 1 );
							sprintf(szVal, "<SegmentURL media=\"%s\"/>", name);
							gf_cfg_set_key(dash_ctx, "URLs", szKey, szVal);
						}
					}
				} else {
					e = gf_isom_start_segment(output, NULL);
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
			e = gf_isom_start_fragment(output, 1);
			if (e) goto err_exit;
	

			for (i=0; i<count; i++) {
				tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
				if (tf->done) continue;
				/*FIXME - we need a way in the spec to specify "past" DTS for splitted sample*/
				gf_isom_set_traf_base_media_decode_time(output, tf->TrackID, tf->InitialTSOffset + tf->next_sample_dts);
			}
		
		}

		//process track by track
		for (i=0; i<count; i++) {
			Bool has_roll, is_rap;
			s32 roll_distance;
			u32 SAP_type = 0;
			/*start with ref*/
			if (tfref && split_seg_at_rap ) {
				if (i==0) {
					tf = tfref;
					has_rap=0;
				} else {
					tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i-1);
					if (tf == tfref) {
						tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
					}
				}
			} else {
				tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
				if (tf == tfref) 
					has_rap=0;
			} 
			if (tf->done) continue;

			//ok write samples
			while (1) {
				Bool stop_frag = 0;
				Bool is_redundant_sample = 0;
				u32 split_sample_duration = 0;

				/*first sample*/
				if (!sample) {
					sample = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 1, &descIndex);
					if (!sample) {
						e = gf_isom_last_error(input);
						goto err_exit;
					}
					if (tf->split_sample_dts_shift) {
						sample->DTS += tf->split_sample_dts_shift;
						is_redundant_sample = 1;
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
					defaultDuration = tf->DefaultDuration;
				}

				if (tf->splitable) {
					if (tfref==tf) {
						u32 frag_dur = (tf->FragmentLength + defaultDuration) * 1000 / tf->TimeScale;
						/*if media segment about to be produced is longer than max segment length, force segment split*/
						if (SegmentDuration + frag_dur > MaxSegmentDuration) {
							split_sample_duration = defaultDuration;
							defaultDuration = tf->TimeScale * (MaxSegmentDuration - SegmentDuration) / 1000 - tf->FragmentLength;
							split_sample_duration -= defaultDuration;
						}
					} else if ((tf->last_sample_cts + defaultDuration) * tfref_timescale > tfref->next_sample_dts * tf->TimeScale) {
						split_sample_duration = defaultDuration;
						defaultDuration = (u32) ( (last_ref_cts * tf->TimeScale)/tfref_timescale - tf->last_sample_cts );
						split_sample_duration -= defaultDuration;
					}
				}

				if (tf==tfref) {
					if (segments_start_with_sap && first_sample_in_segment ) {
						first_sample_in_segment = 0;
						if (! SAP_type) segments_start_with_sap = 0;
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
							stop_frag = 1;
						}
						else if (split_seg_at_rap) {
							u64 next_sap_time;
							u32 frag_dur, next_dur;
							next_dur = gf_isom_get_sample_duration(input, tf->OriginalTrack, tf->SampleNum + 1);
							if (!next_dur) next_dur = defaultDuration;
							/*duration of fragment if we add this rap*/
							frag_dur = (tf->FragmentLength+next_dur)*1000/tf->TimeScale;
							next_sample_rap = 1;
							next_sap_time = isom_get_next_sap_time(input, tf->OriginalTrack, tf->SampleCount, tf->SampleNum + 2);
							/*if no more SAP after this one, do not switch segment*/
							if (next_sap_time) {
								u32 scaler;
								/*this is the fragment duration from last sample added to next SAP*/
								frag_dur += (u32) (next_sap_time - tf->next_sample_dts - next_dur)*1000/tf->TimeScale;
								/*if media segment about to be produced is longer than max segment length, force segment split*/
								if (SegmentDuration + frag_dur > MaxSegmentDuration) {
									split_at_rap = 1;
									/*force new segment*/
									force_switch_segment = 1;
								}

								if (tf->all_sample_raps) {
									/*if adding this SAP will result in stoping the fragment "soon" after it, stop now and start with SAP
									if all samples are RAPs, just stop fragment if we exceed the requested duration by adding the next sample
									otherwise, take 3 samples (should be refined of course)*/
									scaler = 3;
									if ( (tf->FragmentLength + scaler * next_dur)*1000 >= MaxFragmentDuration * tf->TimeScale)
										stop_frag = 1;
								}
							}
						} else if (!has_rap) {
							if (tf->FragmentLength == defaultDuration) has_rap = 2;
							else has_rap = 1;
						}
					} 
				} else {
					next_sample_rap = 0;
				}

				if (tf->SampleNum==tf->SampleCount) {
					stop_frag = 1;
				} else if (tf==tfref) {
					/*fragmenting on "clock" track: no drift control*/
					if (!dash_cfg->fragments_start_with_rap || ( (next && next->IsRAP) || split_at_rap) ) {
						if (tf->FragmentLength*1000 >= MaxFragmentDuration*tf->TimeScale) {
							stop_frag = 1;
						}
					}
				}
				/*do not abort fragment if ref track is done, put everything in the last fragment*/
				else if (!flush_all_samples) {
					/*fragmenting on "non-clock" track: drift control*/
					if (tf->last_sample_cts * tfref_timescale >= last_ref_cts * tf->TimeScale)
						stop_frag = 1;
				}

				if (stop_frag) {
					gf_isom_sample_del(&sample);
					sample = next = NULL;
					if (maxFragDurationOverSegment<=tf->FragmentLength*1000/tf->TimeScale) {
						maxFragDurationOverSegment = tf->FragmentLength*1000/tf->TimeScale;
					}
					tf->FragmentLength = 0;
					if (split_sample_duration)
						tf->split_sample_dts_shift += defaultDuration;

					if (tf==tfref) last_ref_cts = tf->last_sample_cts;

					break;
				}
			}

			if (tf->SampleNum==tf->SampleCount) {
				tf->done = 1;
				nb_tracks_done++;
				if (tf == tfref) {
					tfref = NULL;
					flush_all_samples = 1;
				}
			}
		}

		if (dash_cfg) {

			SegmentDuration += maxFragDurationOverSegment;
			maxFragDurationOverSegment=0;

			/*next fragment will exceed segment length, abort fragment now (all samples RAPs)*/
			if (tfref && tfref->all_sample_raps && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
				force_switch_segment = 1;
			}

			if (force_switch_segment || ((SegmentDuration >= MaxSegmentDuration) && (!split_seg_at_rap || next_sample_rap))) {
				segment_start_time += SegmentDuration;
				nb_segments++;
				if (max_segment_duration * 1000 <= SegmentDuration) {
					max_segment_duration = SegmentDuration;
					max_segment_duration /= 1000;
				}

				force_switch_segment=0;
				switch_segment=1;
				SegmentDuration=0;
				split_at_rap = 0;
				has_rap = 0;
				/*restore fragment duration*/
				MaxFragmentDuration = (u32) (max_duration_sec * 1000);

				if (!simulation_pass) {
					u64 idx_start_range, idx_end_range;
					
					gf_isom_close_segment(output, dash_cfg->subsegs_per_sidx, ref_track_id, ref_track_first_dts, ref_track_next_cts, dash_cfg->daisy_chain_sidx, flush_all_samples ? 1 : 0, &idx_start_range, &idx_end_range);
					ref_track_first_dts = (u64) -1;

					if (!seg_rad_name) {
						file_size = gf_isom_get_file_size(output);
						end_range = file_size - 1;
						if (dash_cfg->single_file_mode!=1) {
							fprintf(mpd_segs, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"", start_range, end_range);	
							if (idx_start_range || idx_end_range) {
								fprintf(mpd_segs, " indexRange=\""LLD"-"LLD"\"", idx_start_range, idx_end_range);	
							}
							fprintf(mpd_segs, "/>\n");	
							if (dash_ctx) {
								char szKey[100], szVal[4046];
								sprintf(szKey, "UrlInfo%d", gf_cfg_get_key_count(dash_ctx, "URLs") + 1 );
								sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
								gf_cfg_set_key(dash_ctx, "URLs", szKey, szVal);
							}
						}
					} else {
						file_size += gf_isom_get_file_size(output);
					}
				}
			} 
			/*next fragment will exceed segment length, abort fragment at next rap (possibly after MaxSegmentDuration)*/
			if (split_seg_at_rap && SegmentDuration && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
				if (!split_at_rap) {
					split_at_rap = 1;
					MaxFragmentDuration = MaxSegmentDuration - SegmentDuration;
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
		simulation_pass = 0;
		/*reset fragmenters*/
		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = gf_list_get(fragmenters, i);
			tf->done = 0;
			tf->last_sample_cts = 0;
			tf->next_sample_dts = 0;
			tf->FragmentLength = 0;
			tf->SampleNum = 0;
			if (tf->is_ref_track) tfref = tf;
		}
		goto restart_fragmentation_pass;
	}

	if (dash_cfg) {
		char buffer[1000];

		/*flush last segment*/
		if (!switch_segment) {
			u64 idx_start_range, idx_end_range;

			if (max_segment_duration * 1000 <= SegmentDuration) {
				max_segment_duration = SegmentDuration;
				max_segment_duration /= 1000;
			}

			gf_isom_close_segment(output, dash_cfg->subsegs_per_sidx, ref_track_id, ref_track_first_dts, ref_track_next_cts, dash_cfg->daisy_chain_sidx, 1, &idx_start_range, &idx_end_range);
			nb_segments++;

			if (!seg_rad_name) {
				file_size = gf_isom_get_file_size(output);
				end_range = file_size - 1;
				if (dash_cfg->single_file_mode!=1) {
					fprintf(mpd_segs, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"", start_range, end_range);
					if (idx_start_range || idx_end_range) {
						fprintf(mpd_segs, " indexRange=\""LLD"-"LLD"\"", idx_start_range, idx_end_range);
					}
					fprintf(mpd_segs, "/>\n");

					if (dash_ctx) {
						char szKey[100], szVal[4046];
						sprintf(szKey, "UrlInfo%d", gf_cfg_get_key_count(dash_ctx, "URLs") + 1 );
						sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
						gf_cfg_set_key(dash_ctx, "URLs", szKey, szVal);
					}
				}
			} else {
				file_size += gf_isom_get_file_size(output);
			}
		}

		period_duration += file_duration;
		//u32 h = (u32) (period_duration/3600);
		//u32 m = (u32) (period_duration-h*60)/60;
		if (!bandwidth)
			bandwidth = (u32) (file_size * 8 / file_duration);

		if (use_url_template) {
			/*segment template does not depend on file name, write the template at the adaptationSet level*/
			if (!dash_cfg->variable_seg_rad_name && first_in_set) {
				const char *rad_name = gf_url_get_resource_name(seg_rad_name);
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, 0, 0);
				fprintf(dash_cfg->mpd, "   <SegmentTemplate timescale=\"1000\" duration=\"%d\" media=\"%s\" startNumber=\"%d\"", (u32) (max_segment_duration*1000), SegmentName, startNumber);	
				/*in BS switching we share the same IS for all reps*/
				if (is_bs_switching) {
					strcpy(SegmentName, bs_switching_segment_name);
				} else {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0);
				}
				fprintf(dash_cfg->mpd, " initialization=\"%s\"", SegmentName);
				fprintf(dash_cfg->mpd, "/>\n");
			}
			/*in BS switching we share the same IS for all reps, write the SegmentTemplate for the init segment*/
			else if (is_bs_switching && first_in_set) {
				fprintf(dash_cfg->mpd, "   <SegmentTemplate initialization=\"%s\"/>\n", bs_switching_segment_name);
			}
		}


		fprintf(dash_cfg->mpd, "   <Representation ");
		if (rep_id) fprintf(dash_cfg->mpd, "id=\"%s\"", rep_id);
		else fprintf(dash_cfg->mpd, "id=\"%p\"", output);
		fprintf(dash_cfg->mpd, " mimeType=\"%s/mp4\" codecs=\"%s\"", audio_only ? "audio" : "video", szCodecs);
		if (width && height) {
			fprintf(dash_cfg->mpd, " width=\"%d\" height=\"%d\"", width, height);
			gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);
			if (fps_denum>1)
				fprintf(dash_cfg->mpd, " frameRate=\"%d/%d\"", fps_num, fps_denum);
			else
				fprintf(dash_cfg->mpd, " frameRate=\"%d\"", fps_num);

			if (!sar_w) sar_w = 1;
			if (!sar_h) sar_h = 1;
			fprintf(dash_cfg->mpd, " sar=\"%d:%d\"", sar_w, sar_h);

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
		fprintf(dash_cfg->mpd, ">\n");

		if (nb_channels && !is_bs_switching) 
			fprintf(dash_cfg->mpd, "    <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n", nb_channels);

		if (dash_ctx) {
			Double seg_dur;
			opt = gf_cfg_get_key(dash_ctx, "DASH", "MaxSegmentDuration");
			if (opt) {
				seg_dur = atof(opt);
				if (seg_dur < max_segment_duration) {
					sprintf(sOpt, "%f", max_segment_duration);
					gf_cfg_set_key(dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
					seg_dur = max_segment_duration;
				} else {
					max_segment_duration = seg_dur;
				}
			} else {
				sprintf(sOpt, "%f", max_segment_duration);
				gf_cfg_set_key(dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
			}
		}

		if (use_url_template) {
			/*segment template depends on file name, but the template at the representation level*/
			if (dash_cfg->variable_seg_rad_name) {
				const char *rad_name = gf_url_get_resource_name(seg_rad_name);
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, bandwidth, 0);
				fprintf(dash_cfg->mpd, "    <SegmentTemplate timescale=\"1000\" duration=\"%d\" media=\"%s\" startNumber=\"%d\"", (u32) (max_segment_duration*1000), SegmentName, startNumber);	
				if (!is_bs_switching) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0);
					fprintf(dash_cfg->mpd, " initialization=\"%s\"", SegmentName);
				}
				fprintf(dash_cfg->mpd, "/>\n");
			}
		} else if (dash_cfg->single_file_mode==1) {
			fprintf(dash_cfg->mpd, "    <BaseURL>%s</BaseURL>\n", gf_url_get_resource_name( gf_isom_get_filename(output) ) );	
			fprintf(dash_cfg->mpd, "    <SegmentBase indexRangeExact=\"true\" indexRange=\"%d-%d\"/>\n", index_start_range, index_end_range);	
		} else {
			if (!seg_rad_name) {
				fprintf(dash_cfg->mpd, "    <BaseURL>%s</BaseURL>\n", gf_url_get_resource_name( gf_isom_get_filename(output) ) );	
			}
			fprintf(dash_cfg->mpd, "    <SegmentList timescale=\"1000\" duration=\"%d\">\n", (u32) (max_segment_duration*1000));	
			/*we are not in bitstreamSwitching mode*/
			if (!is_bs_switching) {
				fprintf(dash_cfg->mpd, "     <Initialization");	
				if (!seg_rad_name) {
					fprintf(dash_cfg->mpd, " range=\"0-"LLD"\"", init_seg_size-1);
				} else {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, rep_id, gf_url_get_resource_name( seg_rad_name) , !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0);
					fprintf(dash_cfg->mpd, " sourceURL=\"%s\"", SegmentName);
				}
				fprintf(dash_cfg->mpd, "/>\n");
			}
		}

		gf_f64_seek(mpd_segs, 0, SEEK_SET);
		while (!feof(mpd_segs)) {
			u32 r = fread(buffer, 1, 100, mpd_segs);
			gf_fwrite(buffer, 1, r, dash_cfg->mpd);
		}

		if (!use_url_template && (dash_cfg->single_file_mode!=1)) {
			fprintf(dash_cfg->mpd, "    </SegmentList>\n");
		}

		fprintf(dash_cfg->mpd, "   </Representation>\n");
	}

	/*store context*/
	if (dash_ctx) {
		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (GF_ISOMTrackFragmenter *)gf_list_get(fragmenters, i);
			
			sprintf(sKey, "TKID_%d_NextDecodingTime", tf->TrackID);
			sprintf(sOpt, LLU, tf->InitialTSOffset + tf->next_sample_dts);
			gf_cfg_set_key(dash_ctx, RepKeyName, sKey, sOpt);
		}
		sprintf(sOpt, "%d", cur_seg);
		gf_cfg_set_key(dash_ctx, RepKeyName, "NextSegmentIndex", sOpt);

		fragment_index = gf_isom_get_next_moof_number(output);
		sprintf(sOpt, "%d", fragment_index);
		gf_cfg_set_key(dash_ctx, RepKeyName, "NextFragmentIndex", sOpt);
		sprintf(sOpt, "%f", period_duration);
		gf_cfg_set_key(dash_ctx, RepKeyName, "CumulatedDuration", sOpt);
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
	if (bs_switch_segment) gf_isom_delete(bs_switch_segment);
	gf_set_progress("ISO File Fragmenting", nb_samp, nb_samp);
	if (mpd_segs) fclose(mpd_segs);
	if (dash_ctx) gf_cfg_del(dash_ctx);
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

		if (mtype == GF_ISOM_MEDIA_HINT) 
			continue;
		
		if (gf_isom_get_sample_count(in, i+1)<2)
			continue;
	
		dur = (Double) gf_isom_get_track_duration(in, i+1);
		dur /= gf_isom_get_timescale(in);
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
	GF_ISOFile *set_file = gf_isom_open(dash_inputs[input_idx].file_name, GF_ISOM_OPEN_READ, NULL);

	for (i=input_idx+1; i<nb_dash_inputs; i++) {
		Bool valid_in_adaptation_set = 1;
		Bool assign_to_group = 1;
		GF_ISOFile *in;

		if (dash_inputs[input_idx].period != dash_inputs[i].period) 
			continue;
		
		if (strcmp(dash_inputs[input_idx].szMime, dash_inputs[i].szMime))
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
			mtype = gf_isom_get_media_type(set_file, j+1);
			if (mtype != gf_isom_get_media_type(in, j+1)) {
				valid_in_adaptation_set = 0;
				assign_to_group = 0;
				break;
			}
			msub_type = gf_isom_get_media_subtype(set_file, j+1, 1);
			if (msub_type != gf_isom_get_media_subtype(in, j+1, 1)) same_codec = 0;
			if ((msub_type==GF_ISOM_SUBTYPE_MPEG4) 
				|| (msub_type==GF_ISOM_SUBTYPE_MPEG4_CRYP) 
				|| (msub_type==GF_ISOM_SUBTYPE_AVC_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_AVC2_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_SVC_H264)
				|| (msub_type==GF_ISOM_SUBTYPE_LSR1)
				) {
					GF_DecoderConfig *dcd1 = gf_isom_get_decoder_config(set_file, j+1, 1);
					GF_DecoderConfig *dcd2 = gf_isom_get_decoder_config(in, j+1, 1);
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
				gf_isom_get_media_language(in, j+1, szLang2);
				if (stricmp(szLang1, szLang2)) {
					valid_in_adaptation_set = 0;
					break;
				}
			}
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
				u32 w1, h1, w2, h2;
				Bool rap, roll;
				u32 roll_dist, sap_type;

				gf_isom_get_track_layout_info(set_file, j+1, &w1, &h1, NULL, NULL, NULL);
				gf_isom_get_track_layout_info(in, j+1, &w2, &h2, NULL, NULL, NULL);

				if (h1*w2 != h2*w1) {
					valid_in_adaptation_set = 0;
					break;
				}

				gf_isom_get_sample_rap_roll_info(in, j+1, 0, &rap, &roll, &roll_dist);
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
	return GF_OK;
}

static GF_Err dasher_isom_create_init_segment(GF_DashSegInput *dash_inputs, u32 nb_dash_inputs, u32 adaptation_set, char *szInitName, const char *tmpdir, Bool *disable_bs_switching)
{
	GF_Err e = GF_OK;
	u32 i;
	Bool sps_merge_failed = 0;
	GF_ISOFile *init_seg = gf_isom_open(szInitName, GF_ISOM_OPEN_WRITE, tmpdir);

	for (i=0; i<nb_dash_inputs; i++) {
		u32 j;
		GF_ISOFile *in;

		if (dash_inputs[i].adaptation_set != adaptation_set)
			continue;

		in = gf_isom_open(dash_inputs[i].file_name, GF_ISOM_OPEN_READ, NULL);
		if (!in) {
			e = gf_isom_last_error(NULL);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH]: Error while opening %s: %s\n", dash_inputs[i].file_name, gf_error_to_string( e ) ));
			return e;
		}

		for (j=0; j<gf_isom_get_track_count(in); j++) {
			u32 track = gf_isom_get_track_by_id(init_seg, gf_isom_get_track_id(in, j+1));
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
					Bool do_merge = 1;
					u32 stype1, stype2;
					stype1 = gf_isom_get_media_subtype(in, j+1, 1);
					stype2 = gf_isom_get_media_subtype(init_seg, track, 1);
					if (stype1 != stype2) do_merge = 0;
					switch (stype1) {
						case GF_4CC( 'a', 'v', 'c', '1'):
						case GF_4CC( 'a', 'v', 'c', '2'):
						case GF_4CC( 's', 'v', 'c', '1'):
							break;
						default:
							do_merge = 0;
							break;
					}
					if (do_merge) {
						u32 k, l, sps_id1, sps_id2;
						GF_AVCConfig *avccfg1 = gf_isom_avc_config_get(in, j+1, 1);
						GF_AVCConfig *avccfg2 = gf_isom_avc_config_get(init_seg, track, 1);
#ifndef GPAC_DISABLE_AV_PARSERS
						for (k=0; k<gf_list_count(avccfg2->sequenceParameterSets); k++) {
							GF_AVCConfigSlot *slc = gf_list_get(avccfg2->sequenceParameterSets, k);
							gf_avc_get_sps_info(slc->data, slc->size, &sps_id1, NULL, NULL, NULL, NULL);
							for (l=0; l<gf_list_count(avccfg1->sequenceParameterSets); l++) {
								GF_AVCConfigSlot *slc_orig = gf_list_get(avccfg1->sequenceParameterSets, l);
								gf_avc_get_sps_info(slc_orig->data, slc_orig->size, &sps_id2, NULL, NULL, NULL, NULL);
								if (sps_id2==sps_id1) {
									do_merge = 0;
									break;
								}
							}
						}
#endif
						/*no conflicts in SPS ids, merge all SPS in a single sample desc*/
						if (do_merge) {
							while (gf_list_count(avccfg1->sequenceParameterSets)) {
								GF_AVCConfigSlot *slc = gf_list_get(avccfg1->sequenceParameterSets, 0);
								gf_list_rem(avccfg1->sequenceParameterSets, 0);
								gf_list_add(avccfg2->sequenceParameterSets, slc);
							}
							while (gf_list_count(avccfg1->pictureParameterSets)) {
								GF_AVCConfigSlot *slc = gf_list_get(avccfg1->pictureParameterSets, 0);
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
					if (!do_merge)
						gf_isom_clone_sample_description(init_seg, track, in, j+1, 1, NULL, NULL, &outDescIndex);
				}
			} else {
				u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess;
				u8 defaultPadding;
				u16 defaultDegradationPriority;

				gf_isom_clone_track(in, j+1, init_seg, 0, &track);

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
		gf_isom_set_brand_info(init_seg, GF_4CC('i','s','o','5'), 1);
		gf_isom_modify_alternate_brand(init_seg, GF_4CC('d','a','s','h'), 1);
		if (i) gf_isom_close(in);
	}
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: Couldn't create initialization segment: error %s\n", gf_error_to_string(e) ));
		*disable_bs_switching = 1;
		gf_isom_delete(init_seg);
		gf_delete_file(szInitName);
		return GF_OK;
	} else if (sps_merge_failed) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH]: Couldnt merge AVC|H264 SPS from different files (same SPS ID used) - different sample descriptions will be used\n"));
		*disable_bs_switching = 1;
		gf_isom_delete(init_seg);
		gf_delete_file(szInitName);
	} else {
		gf_isom_close(init_seg);
	}
	return GF_OK;
}

static GF_Err dasher_isom_segment_file(GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenterOptions *opts, Bool first_in_set)
{
	GF_ISOFile *in = gf_isom_open(dash_input->file_name, GF_ISOM_OPEN_READ, opts->tmpdir);
	GF_Err e = gf_media_isom_segment_file(in, szOutName, opts->fragment_duration, opts, dash_input->representationID, first_in_set);
	gf_isom_close(in);
	return e;
}
#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

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
		Bool valid_in_adaptation_set = 1;
		Bool assign_to_group = 1;

		if (dash_inputs[input_idx].period != dash_inputs[i].period) 
			continue;

		if (strcmp(dash_inputs[input_idx].szMime, dash_inputs[i].szMime))
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
			Bool found = 0;
			/*make sure we have the same track*/
			for (k=0; k<probe.nb_tracks;k++) {
				if (in.tk_info[j].track_num==probe.tk_info[k].track_num) {
					found = 1;
					break;
				}
			}
			if (!found) {
				valid_in_adaptation_set = 0;
				assign_to_group = 0;
				break;
			}
			src_tk = &in.tk_info[j];
			probe_tk = &probe.tk_info[k];

			/*make sure we use the same media type*/
			if (src_tk->type != probe_tk->type) {
				valid_in_adaptation_set = 0;
				assign_to_group = 0;
				break;
			}
			/*make sure we use the same codec type*/
			if (src_tk->media_type != probe_tk->media_type) {
				valid_in_adaptation_set = 0;
				break;
			}
			/*make sure we use the same aspect ratio*/
			if (src_tk->type==GF_ISOM_MEDIA_VISUAL) {
				if (src_tk->video_info.width * probe_tk->video_info.height != src_tk->video_info.height * probe_tk->video_info.width) {
					valid_in_adaptation_set = 0;
					break;
				}
			} else if (src_tk->lang != probe_tk->lang) {
				valid_in_adaptation_set = 0;
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

#ifndef GPAC_DISABLE_MPEG2TS

typedef struct
{
	FILE *src;
	GF_M2TS_Demuxer *ts;
	u64 file_size;
	u32 bandwidth;
	Bool has_seen_pat;

	/* For indexing the TS*/
	Double segment_duration;
	Bool segment_at_rap;

	u32 segment_index;

	FILE *index_file;
	GF_BitStream *index_bs;

	GF_SegmentIndexBox *sidx;
	GF_PcrInfoBox *pcrb;

	u32 reference_pid;
	GF_M2TS_PES *reference_stream;
	
	u32 nb_pes_in_segment;
	/* earliest presentation time for the whole segment */
	u64 first_PTS;

	/* earliest presentation time for the subsegment being processed */
	u64 base_PTS;
	/* byte offset for the start of subsegment being processed */
	u32 base_offset;
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
	u32 prev_base_offset;
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

	/*Interpolated PCR value for the pcrb*/
	u64 interpolated_pcr_value;
	u64 last_pcr_value;

	/* information about the first PAT found in the subsegment */
	u32 last_pat_position;
	u32 first_pat_position;
	u32 prev_last_pat_position;
	Bool first_pat_position_valid;
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

/* Initializes an SIDX */
static GF_SegmentIndexBox *m2ts_sidx_new(u32 pid, u64 PTS, u64 position)
{						
	GF_SegmentIndexBox *sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
	sidx->reference_ID = pid;
	/* timestamps in MPEG-2 are expressed in 90 kHz timescale */
	sidx->timescale = 90000;
	/* first encountered PTS on the PID for this subsegment */
	sidx->earliest_presentation_time = PTS;
	sidx->first_offset = position;
	return sidx;
}

static void m2ts_sidx_add_entry(GF_SegmentIndexBox *sidx, Bool ref_type, 
								u32 size, u32 duration, Bool first_is_SAP, u32 sap_type, u32 RAP_delta_time)
{
	GF_SIDXReference *ref;
	sidx->nb_refs++;
	sidx->refs = gf_realloc(sidx->refs, sidx->nb_refs*sizeof(GF_SIDXReference));
	ref = &(sidx->refs[sidx->nb_refs-1]);
	ref->reference_type = ref_type;
	ref->reference_size = size;
	ref->subsegment_duration = duration;
	ref->starts_with_SAP = first_is_SAP;
	ref->SAP_type = sap_type;
	ref->SAP_delta_time = (sap_type ? RAP_delta_time: 0);
}

static void m2ts_pcrb_add_entry(GF_PcrInfoBox *pcrb, u64 interpolatedPCR)
{
	pcrb->subsegment_count++;
	pcrb->pcr_values = gf_realloc(pcrb->pcr_values, pcrb->subsegment_count*sizeof(u64));
	
	pcrb->pcr_values[pcrb->subsegment_count-1] = interpolatedPCR;
}

static void m2ts_sidx_update_prev_entry_duration(GF_SegmentIndexBox *sidx, u32 duration)
{
	GF_SIDXReference *ref;
	if (sidx->nb_refs == 0) return;
	ref = &(sidx->refs[sidx->nb_refs-1]);
	ref->subsegment_duration = duration;
}

static void m2ts_sidx_finalize_size(GF_TSSegmenter *index_info, u64 file_size)
{
	GF_SIDXReference *ref;
	if (index_info->sidx->nb_refs == 0) return;
	ref = &(index_info->sidx->refs[index_info->sidx->nb_refs-1]);
	ref->reference_size = (u32)(file_size - index_info->prev_base_offset);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Subsegment: position-range ajdustment:%d-%d (%d bytes)\n", index_info->prev_base_offset, (u32)file_size, ref->reference_size));
}

static void m2ts_sidx_flush_entry(GF_TSSegmenter *index_info) 
{
	u32 size; 
	u32 duration, prev_duration; 
	u32 SAP_delta_time; 
	u32 SAP_offset;

	u32 end_offset;

	if (!index_info->sidx) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Segment: Reference PID: %d, EPTime: "LLU", Start Offset: %d bytes\n", index_info->reference_pid, index_info->base_PTS, index_info->base_offset));
		index_info->sidx = m2ts_sidx_new(index_info->reference_pid, index_info->base_PTS, index_info->base_offset);
	}
	
	if (!index_info->pcrb) {
		index_info->pcrb = (GF_PcrInfoBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_PCRB);
	}

	/* determine the end of the current index */
	if (index_info->segment_at_rap) {
		/*split at PAT*/
		end_offset = index_info->last_pat_position;
	} else {
		/* split at PES header */
		end_offset = index_info->last_offset; 
	}

	/* close the current index */ 
	size = (u32)(end_offset - index_info->base_offset);
	duration = (u32)(index_info->last_PTS - index_info->base_PTS);
	SAP_delta_time= (u32)(index_info->first_SAP_PTS - index_info->base_PTS);
	SAP_offset = (u32)(index_info->first_SAP_offset - index_info->base_offset);
	m2ts_sidx_add_entry(index_info->sidx, 0, size, duration, index_info->first_pes_sap, index_info->SAP_type, SAP_delta_time);
	m2ts_pcrb_add_entry(index_info->pcrb, index_info->interpolated_pcr_value);

	/* adjust the previous index duration */
	if (index_info->sidx->nb_refs > 0 && (index_info->base_PTS < index_info->prev_last_PTS) ) {
		prev_duration = (u32)(index_info->base_PTS-index_info->prev_base_PTS);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("           time-range adj.: %.03f-%.03f / %.03f sec.\n", 		
			(index_info->prev_base_PTS - index_info->first_PTS)/90000.0, 
			(index_info->base_PTS - index_info->first_PTS)/90000.0, prev_duration/90000.0);
		m2ts_sidx_update_prev_entry_duration(index_info->sidx, prev_duration));
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
	index_info->SAP_type = 0;
	index_info->first_SAP_PTS = 0;
	index_info->first_SAP_offset = 0;
	index_info->first_pes_sap = 0;
	index_info->nb_pes_in_segment = 0;
	index_info->last_DTS = 0;

	if (index_info->last_pat_position >= index_info->base_offset) {
		index_info->first_pat_position_valid = 1;
		index_info->first_pat_position = index_info->last_pat_position;
	} else {
		index_info->first_pat_position_valid = 0;
		index_info->first_pat_position = 0;
	}
	if (index_info->last_cat_position >= index_info->base_offset) {
		index_info->first_cat_position_valid = 1;
		index_info->first_cat_position = index_info->last_cat_position;
	} else {
		index_info->first_cat_position_valid = 0;
		index_info->first_cat_position = 0;
	}
	if (index_info->last_pmt_position >= index_info->base_offset) {
		index_info->first_pmt_position_valid = 1;
		index_info->first_pmt_position = index_info->last_pmt_position;
	} else {
		index_info->first_pmt_position_valid = 0;
		index_info->first_pmt_position = 0;
	}
	if (index_info->last_pcr_position >= index_info->base_offset) {
		index_info->first_pcr_position_valid = 1;
		index_info->first_pcr_position = index_info->last_pcr_position;
	} else {
		index_info->first_pcr_position_valid = 0;
		index_info->first_pcr_position = 0;
	}
}

static void m2ts_check_indexing(GF_TSSegmenter *index_info)
{
	u32 delta_time = (u32)(index_info->last_PTS - index_info->base_PTS);
	u32 segment_duration = (u32)(index_info->segment_duration*90000);
	/* we need to create an SIDX entry when the duration of the previous entry is too big */
	if (delta_time >= segment_duration) {
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
			GF_M2TS_ES *es = gf_list_get(prog->streams, i);
			gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_DEFAULT);
		}
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
		break;
	case GF_M2TS_EVT_PAT_UPDATE:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = 1;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
		break;
	case GF_M2TS_EVT_PAT_REPEAT:
		if (!ts_seg->first_pat_position_valid) {
			ts_seg->first_pat_position_valid = 1;
			ts_seg->first_pat_position = (ts->pck_number-1)*188;
		}
		ts_seg->last_pat_position = (ts->pck_number-1)*188;
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
				ts_seg->last_frame_duration = (u32) (pck->DTS - ts_seg->last_DTS);
				ts_seg->last_DTS = pck->DTS;
				ts_seg->nb_pes_in_segment++;
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
					ts_seg->first_pes_sap = 1;
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

			m2ts_check_indexing(ts_seg);
		}
		break;
	case GF_M2TS_EVT_PES_PCR:
		pck = par;
		if (!ts_seg->first_pcr_position_valid) {
			ts_seg->first_pcr_position_valid = 1;
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
	ts_seg->ts->notify_pes_timing = 1;
	ts_seg->ts->user = ts_seg;

	gf_f64_seek(ts_seg->src, 0, SEEK_END);
	ts_seg->file_size = gf_f64_tell(ts_seg->src);
	gf_f64_seek(ts_seg->src, 0, SEEK_SET);

	if (probe_mode) {
		/* first loop to process all packets between two PAT, and assume all signaling was found between these 2 PATs */
		while (!feof(ts_seg->src)) {
			char data[188];
			u32 size = fread(data, 1, 188, ts_seg->src);
			if (size<188) break;

			gf_m2ts_process_data(ts_seg->ts, data, size);
			if ((probe_mode ==1) && ts_seg->has_seen_pat) break;
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

static GF_Err dasher_mp2t_get_components_info(GF_DashSegInput *dash_input, GF_DASHSegmenterOptions *opts)
{
	GF_TSSegmenter ts_seg;
	GF_Err e = dasher_generic_get_components_info(dash_input, opts);
	if (e) return e;

	e = dasher_get_ts_demux(&ts_seg, dash_input->file_name, 2);
	if (e) return e;

	dash_input->duration = (ts_seg.last_PTS + ts_seg.last_frame_duration - ts_seg.first_PTS)/90000.0;
	dasher_del_ts_demux(&ts_seg);

	return GF_OK;
}


static GF_Err dasher_mp2t_segment_file(GF_DashSegInput *dash_input, const char *szOutName, GF_DASHSegmenterOptions *opts, Bool first_in_set)
{	
	GF_TSSegmenter ts_seg;
	Bool rewrite_input = 0;
	char SegName[GF_MAX_PATH], IdxName[GF_MAX_PATH];
	char szCodecs[100];
	u32 i;
	GF_Err e;
	u64 start;
	/*compute name for indexed segments*/
	const char *basename = gf_url_get_resource_name(dash_input->file_name);

	/*perform indexation of the file, this info will be destroyed at the end of the segment file routine*/
	e = dasher_get_ts_demux(&ts_seg, dash_input->file_name, 0);
	if (e) return e;
	
	ts_seg.segment_duration = opts->segment_duration;
	ts_seg.segment_at_rap = opts->segments_start_with_rap;

	ts_seg.bandwidth = (u32) (ts_seg.file_size * 8 / dash_input->duration);	

	/*create bitstreams*/
	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, 1, IdxName, basename, dash_input->representationID, opts->seg_rad_name, "six", 0, 0, 0);	

	ts_seg.index_file = NULL;
	ts_seg.index_bs = NULL;
	if (opts->use_url_template != 2) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		GF_SegmentTypeBox *styp;
		ts_seg.index_file = gf_f64_open(IdxName, "wb");
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

	/*index the file*/
	while (!feof(ts_seg.src)) {
		char data[188];
		u32 size = fread(data, 1, 188, ts_seg.src);
		if (size<188) break;
		gf_m2ts_process_data(ts_seg.ts, data, size);
	}

	/* flush SIDX entry for the last packets */
	m2ts_sidx_flush_entry(&ts_seg);
	m2ts_sidx_finalize_size(&ts_seg, ts_seg.file_size);
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH]: Indexing done (1 sidx, %d entries).\n", ts_seg.sidx->nb_refs));

	gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_REPINDEX, 1, IdxName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "six", 0, 0, 0);	

	/*write segment template for all representations*/
	if (first_in_set && opts->seg_rad_name && opts->use_url_template && !opts->variable_seg_rad_name) {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "ts", 0, ts_seg.bandwidth, ts_seg.segment_index);
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, 1, IdxName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "six", 0, ts_seg.bandwidth, ts_seg.segment_index);
		fprintf(opts->mpd, "   <SegmentTemplate timescale=\"1000\" duration=\"%d\" startNumber=\"0\" media=\"%s\" index=\"%s\"/>\n", (u32) (1000*opts->segment_duration), SegName, IdxName); 
	}


	fprintf(opts->mpd, "   <Representation id=\"%s\" mimeType=\"video/mp2t\"", dash_input->representationID);
	szCodecs[0] = 0;
	for (i=0; i<dash_input->nb_components; i++) {
		if (strlen(dash_input->components[i].szCodec)) 
			if (strlen(szCodecs))
				strcat(szCodecs, ",");
		strcat(szCodecs, dash_input->components[i].szCodec);

		if (dash_input->components[i].width && dash_input->components[i].height) 
			fprintf(opts->mpd, " width=\"%d\" height=\"%d\"", dash_input->components[i].width, dash_input->components[i].height);

		if (dash_input->components[i].sample_rate) 
			fprintf(opts->mpd, " audioSamplingRate=\"%d\"", dash_input->components[i].sample_rate);

		if (dash_input->components[i].szLang[0]) 
			fprintf(opts->mpd, " lang=\"%s\"", dash_input->components[i].szLang);
	}
	if (strlen(szCodecs))
		fprintf(opts->mpd, " codecs=\"%s\"", szCodecs);

	fprintf(opts->mpd, " startWithSAP=\"%d\"", opts->segments_start_with_rap ? 1 : 0);
	fprintf(opts->mpd, " bandwidth=\"%d\"", ts_seg.bandwidth);
	fprintf(opts->mpd, ">\n");

	if (opts->single_file_mode==1) {
		gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "ts", 0, ts_seg.bandwidth, ts_seg.segment_index);
		fprintf(opts->mpd, "    <BaseURL>%s</BaseURL>\n", opts->seg_rad_name ? SegName : dash_input->file_name);

		fprintf(opts->mpd, "    <SegmentBase>\n");
		fprintf(opts->mpd, "     <RepresentationIndex sourceURL=\"%s\"/>\n", IdxName);
		fprintf(opts->mpd, "    </SegmentBase>\n");
		/*we rewrite the file*/
		if (opts->seg_rad_name) rewrite_input = 1;
	} else {
		if (opts->seg_rad_name && opts->use_url_template) {
			if (opts->variable_seg_rad_name) {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "ts", 0, ts_seg.bandwidth, ts_seg.segment_index);
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, 1, IdxName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "six", 0, ts_seg.bandwidth, ts_seg.segment_index);
				fprintf(opts->mpd, "    <SegmentTemplate timescale=\"1000\" duration=\"%d\" startNumber=\"0\" media=\"%s\" index=\"%s\"/>\n", (u32) (1000*opts->segment_duration), SegName, IdxName); 
			}
		} else {
			gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, basename, dash_input->representationID, gf_url_get_resource_name(opts->seg_rad_name), "ts", 0, ts_seg.bandwidth, ts_seg.segment_index);
			fprintf(opts->mpd, "    <BaseURL>%s</BaseURL>\n", opts->seg_rad_name ? SegName : dash_input->file_name);
			fprintf(opts->mpd, "    <SegmentList timescale=\"1000\" duration=\"%d\">\n", (u32) (1000*opts->segment_duration)); 
			fprintf(opts->mpd, "     <RepresentationIndex sourceURL=\"%s\"/>\n", IdxName); 
		}

		if (opts->single_file_mode==2) {
			start = ts_seg.sidx->first_offset;
			for (i=0; i<ts_seg.sidx->nb_refs; i++) {
				GF_SIDXReference *ref = &ts_seg.sidx->refs[i];
				fprintf(opts->mpd, "     <SegmentURL mediaRange=\""LLD"-"LLD"\"/>\n", start, start+ref->reference_size-1);
				start += ref->reference_size;
			}
			if (opts->seg_rad_name) rewrite_input = 1;

		} else {
			FILE *src, *dst;
			u64 pos, end;
			src = gf_f64_open(dash_input->file_name, "rb");
			start = ts_seg.sidx->first_offset;
			for (i=0; i<ts_seg.sidx->nb_refs; i++) {
				char buf[4096];
				GF_SIDXReference *ref = &ts_seg.sidx->refs[i];

				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_SEGMENT, 1, SegName, basename, dash_input->representationID, opts->seg_rad_name, "ts", 0, ts_seg.bandwidth, ts_seg.segment_index);

				if (opts->use_url_template != 2) {
					dst = gf_f64_open(SegName, "wb");

					gf_f64_seek(src, start, SEEK_SET);
					pos = start;
					end = start+ref->reference_size;
					while (pos<end) {
						u32 res;
						u32 to_read = 4096;
						if (pos+4096 >= end) {
							to_read = (u32) (end-pos);
						}
						res = fread(buf, 1, to_read, src);
						if (res==to_read) {
							res = fwrite(buf, 1, to_read, dst);
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
				ts_seg.segment_index++;

				if (!opts->use_url_template) 
					fprintf(opts->mpd, "     <SegmentURL media=\"%s\"/>\n", SegName);

				gf_set_progress("Extracting segment ", i+1, ts_seg.sidx->nb_refs);
			}
			fclose(src);
		}	
		if (!opts->seg_rad_name || !opts->use_url_template) {
			fprintf(opts->mpd, "    </SegmentList>\n");
		}
	}

	if (rewrite_input) {
		FILE *in, *out;
		char buf[3760];

		in = gf_f64_open(dash_input->file_name, "rb");
		out = gf_f64_open(SegName, "wb");
		while (1) {
			u32 read = fread(buf, 1, 3760, in);
			if (!read) break;
			fwrite(buf, 1, read, out);
		}
		fclose(in);
		fclose(out);
	}

	fprintf(opts->mpd, "   </Representation>\n");

	if (ts_seg.sidx && ts_seg.index_bs) {
		gf_isom_box_size((GF_Box *)ts_seg.sidx);
		gf_isom_box_write((GF_Box *)ts_seg.sidx, ts_seg.index_bs);
	}
	if (ts_seg.pcrb && ts_seg.index_bs) {
		gf_isom_box_size((GF_Box *)ts_seg.pcrb);
		gf_isom_box_write((GF_Box *)ts_seg.pcrb, ts_seg.index_bs);
	}

	if (ts_seg.sidx) gf_isom_box_del((GF_Box *)ts_seg.sidx);
	if (ts_seg.pcrb) gf_isom_box_del((GF_Box *)ts_seg.pcrb);
	if (ts_seg.index_file) fclose(ts_seg.index_file);
	if (ts_seg.index_bs) gf_bs_del(ts_seg.index_bs);
	dasher_del_ts_demux(&ts_seg);

	return GF_OK;
}

#endif //GPAC_DISABLE_MPEG2TS

GF_Err gf_dash_segmenter_probe_input(GF_DashSegInput *dash_input)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (gf_isom_probe_file(dash_input->file_name)) {
		strcpy(dash_input->szMime, "video/mp4");
		dash_input->dasher_create_init_segment = dasher_isom_create_init_segment;
		dash_input->dasher_input_classify = dasher_isom_classify_input;
		dash_input->dasher_get_components_info = dasher_isom_get_input_components_info;
		dash_input->dasher_segment_file = dasher_isom_segment_file;
		return GF_OK;
	}
#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

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

static GF_Err write_mpd_header(FILE *mpd, const char *mpd_name, GF_DashProfile profile, const char *title, const char *source, const char *copyright, const char *moreInfoURL, const char **mpd_base_urls, u32 nb_mpd_base_urls, Bool dash_dynamic, u32 time_shift_depth, Double mpd_duration)
{
	u32 h, m, i;
	Double s;

	fprintf(mpd, "<?xml version=\"1.0\"?>\n");
	fprintf(mpd, "<!-- MPD file Generated with GPAC version "GPAC_FULL_VERSION" -->\n");

	/*TODO what should we put for minBufferTime */
	fprintf(mpd, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" minBufferTime=\"PT1.5S\" type=\"%s\" ", dash_dynamic ? "dynamic" : "static"); 
	if (dash_dynamic) {
#ifndef _WIN32_WCE
		u32 sec, frac;
		time_t gtime;
		struct tm *t;
		gf_net_get_ntp(&sec, &frac);
		gtime = sec - GF_NTP_SEC_1900_TO_1970;
		if ((s32)time_shift_depth>=0) {
			gtime -= time_shift_depth;
		}
		t = gmtime(&gtime);
		fprintf(mpd, "availabilityStartTime=\"%d-%d-%dT%d:%d:%dZ\" ", 1900+t->tm_year, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
#endif

		if ((s32)time_shift_depth>=0) {
			h = (u32) (time_shift_depth/3600);
			m = (u32) (time_shift_depth-h*60)/60;
			s = time_shift_depth - h*3600 - m*60;
			fprintf(mpd, "timeShiftBufferDepth=\"PT%dH%dM%.2fS\" ", h, m, s);
		}
	} else {
		h = (u32) (mpd_duration/3600);
		m = (u32) (mpd_duration-h*60)/60;
		s = mpd_duration - h*3600 - m*60;
		fprintf(mpd, "mediaPresentationDuration=\"PT%dH%dM%.2fS\" ", h, m, s);
	}

	if (profile==GF_DASH_PROFILE_LIVE) 
		fprintf(mpd, "profiles=\"urn:mpeg:dash:profile:isoff-live:2011\">\n");
	else if (profile==GF_DASH_PROFILE_ONDEMAND) 
		fprintf(mpd, "profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\">\n");
	else if (profile==GF_DASH_PROFILE_MAIN) 
		fprintf(mpd, "profiles=\"urn:mpeg:dash:profile:isoff-main:2011\">\n");
	else 
		fprintf(mpd, "profiles=\"urn:mpeg:dash:profile:full:2011\">\n");	

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

static GF_Err write_period_header(FILE *mpd, Double period_start, Double period_duration)
{
	u32 h, m;
	Double s;

	fprintf(mpd, " <Period ");
	if (period_start) {
		h = (u32) (period_start/3600);
		m = (u32) (period_start-h*60)/60;
		s = period_start - h*3600 - m*60;
		fprintf(mpd, "start=\"PT%dH%dM%.2fS\" ", h, m, s);	
	}
	if (period_duration) {
		h = (u32) (period_duration/3600);
		m = (u32) (period_duration-h*60)/60;
		s = period_duration - h*3600 - m*60;
		fprintf(mpd, "duration=\"PT%dH%dM%.2fS\"", h, m, s);	
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
		char langCode[4];
		langCode[3] = 0;

		if (bitstream_switching_mode) {
			for (i=0; i<first_rep->nb_components; i++) {
				struct _dash_component *comp = &first_rep->components[i];
				if (comp->media_type == GF_ISOM_MEDIA_AUDIO) {
					fprintf(mpd, "   <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n", comp->channels);
				}
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
					fprintf(mpd, " lang=\"%s\"", langCode);
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

static GF_Err gf_dasher_init_context(const char *dash_ctx_file, Bool *dynamic, u32 *timeShiftBufferDepth, const char *periodID)
{
	const char *opt;
	char szVal[100];
	GF_Config *dash_ctx;
	Bool first_run = 0;
	u32 sec, frac;
#ifndef _WIN32_WCE
		time_t gtime;
#endif

	if (!dash_ctx_file) return GF_OK;
	
	dash_ctx = gf_cfg_new(NULL, dash_ctx_file);
	if (!dash_ctx) {
		FILE *t = fopen(dash_ctx_file, "wt");
		if (t) fclose(t);
		dash_ctx = gf_cfg_new(NULL, dash_ctx_file);
		
		if (!dash_ctx) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot create DASH live context\n"));
			return GF_IO_ERR;
		}
	}

	opt = gf_cfg_get_key(dash_ctx, "DASH", "SessionType");
	/*first run, init all options*/
	if (!opt) {
		Bool first_run = 1;
		sprintf(szVal, "%d", *timeShiftBufferDepth);
		gf_cfg_set_key(dash_ctx, "DASH", "SessionType", *dynamic ? "dynamic" : "static");
		gf_cfg_set_key(dash_ctx, "DASH", "TimeShiftBufferDepth", szVal);
		gf_cfg_set_key(dash_ctx, "DASH", "StoreParams", "yes");
	} else {
		*dynamic = !strcmp(opt, "dynamic") ? 1 : 0;
		opt = gf_cfg_get_key(dash_ctx, "DASH", "TimeShiftBufferDepth");
		*timeShiftBufferDepth = atoi(opt);
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

	gf_cfg_del(dash_ctx);

	return GF_OK;
}


/*dash segmenter*/
GF_Err gf_dasher_segment_files(const char *mpdfile, GF_DashSegmenterInput *inputs, u32 nb_dash_inputs, GF_DashProfile dash_profile, 
							   const char *mpd_title, const char *mpd_source, const char *mpd_copyright,
							   const char *mpd_moreInfoURL, const char **mpd_base_urls, u32 nb_mpd_base_urls, 
							   Bool use_url_template, Bool single_segment, Bool single_file, Bool bitstream_switching_mode, 
							   Bool seg_at_rap, Double dash_duration, char *seg_name, char *seg_ext,
							   Double frag_duration, s32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool frag_at_rap, const char *tmpdir,
							   char *dash_ctx, Bool dash_dynamic, u32 time_shift_depth)
{
	u32 i, j, segment_mode;
	char *sep, szSegName[GF_MAX_PATH], szSolvedSegName[GF_MAX_PATH];
	u32 cur_adaptation_set;
	u32 max_adaptation_set = 0;
	u32 cur_period;
	u32 max_period = 0;
	u32 cur_group_id = 0;
	u32 max_sap_type = 0;
	Bool none_supported = 1;
	Double presentation_duration = 0;
	GF_Err e = GF_OK;
	FILE *mpd = NULL;
	GF_DashSegInput *dash_inputs;
	GF_DASHSegmenterOptions dash_opts;

	/*copy over input files to our internal structure*/
	dash_inputs = gf_malloc(sizeof(GF_DashSegInput)*nb_dash_inputs);
	memset(dash_inputs, 0, sizeof(GF_DashSegInput)*nb_dash_inputs);
	for (i=0; i<nb_dash_inputs; i++) {
		dash_inputs[i].file_name = inputs[i].file_name;
		strcpy(dash_inputs[i].representationID, inputs[i].representationID);
		strcpy(dash_inputs[i].periodID, inputs[i].periodID);

		if (!strlen(dash_inputs[i].periodID)) dash_inputs[i].period = 1;

		gf_dash_segmenter_probe_input(&dash_inputs[i]);
 	}
	memset(&dash_opts, 0, sizeof(GF_DASHSegmenterOptions));


	/*if output id not in the current working dir, concatenate output path to segment name*/
	szSegName[0] = 0;
	if (seg_name) {
		if (gf_url_get_resource_path(mpdfile, szSegName)) {
			strcat(szSegName, seg_name);
			seg_name = szSegName;
		}
	}

	max_period = 1;
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
			return GF_OK;
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

	/*adjust params based on profiles*/
	switch (dash_profile) {
	case GF_DASH_PROFILE_LIVE:
		seg_at_rap = 1;
		use_url_template = 1;
		single_segment = single_file = 0;
		break;
	case GF_DASH_PROFILE_ONDEMAND:
		seg_at_rap = 1;
		single_segment = 1;
		/*BS switching is meaningless in onDemand profile*/
		bitstream_switching_mode = 0;
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
	dash_opts.segment_duration = dash_duration;
	dash_opts.seg_ext = seg_ext;
	dash_opts.daisy_chain_sidx = daisy_chain_sidx;
	dash_opts.subsegs_per_sidx = subsegs_per_sidx;
	dash_opts.use_url_template = use_url_template;
	dash_opts.single_file_mode = segment_mode;
	dash_opts.fragments_start_with_rap = frag_at_rap;
	dash_opts.dash_ctx_file = dash_ctx;
	dash_opts.fragment_duration = frag_duration;
	dash_opts.tmpdir = tmpdir;

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

	/*init dash context if needed*/
	if (dash_ctx)
		gf_dasher_init_context(dash_ctx, &dash_dynamic, &time_shift_depth, NULL);

	mpd = gf_f64_open(mpdfile, "wt");
	if (!mpd) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MPD] Cannot open MPD file %s for writing\n", mpdfile));
		return GF_IO_ERR;
	}

	dash_opts.mpd = mpd;

	e = write_mpd_header(mpd, mpdfile, dash_profile, mpd_title, mpd_source, mpd_copyright, mpd_moreInfoURL, (const char **) mpd_base_urls, nb_mpd_base_urls, dash_dynamic, time_shift_depth, presentation_duration);
	if (e) goto exit;

	for (cur_period=0; cur_period<max_period; cur_period++) {
		Double period_duration = 0;
		/*for each identified adaptationSets, write MPD and perform segmentation of input files*/
		for (i=0; i< nb_dash_inputs; i++) {
			if (dash_inputs[i].adaptation_set && (dash_inputs[i].period==cur_period+1)) {
				period_duration = dash_inputs[i].period_duration;
				break;
			}
		}
		e = write_period_header(mpd, 0.0, period_duration);
		if (e) goto exit;

		/*for each identified adaptationSets, write MPD and perform segmentation of input files*/
		for (cur_adaptation_set=0; cur_adaptation_set < max_adaptation_set; cur_adaptation_set++) {
			char szInit[GF_MAX_PATH], tmp[GF_MAX_PATH];
			u32 first_rep_in_set=0;
			u32 max_width = 0;
			u32 max_height = 0;
			u32 fps_num = 0;
			u32 fps_denum = 0;
			Bool use_bs_switching = bitstream_switching_mode ? 1 : 0;
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
			if ((bitstream_switching_mode==1) && dash_inputs[first_rep_in_set].nb_rep_in_adaptation_set==1)
				use_bs_switching = 0;

			if (! use_bs_switching) {
				skip_init_segment_creation = 1;
			}
			else if (! dash_inputs[first_rep_in_set].dasher_create_init_segment) {
				skip_init_segment_creation = 1;
				strcpy(szInit, "");
			}
			/*fixme - dash context should be cleaned up
			if we already wrote the InitializationSegment, get rid of it (no overwrite) and let the dashing routine open it*/
			else if (dash_ctx) {
				char RepKeyName[GF_MAX_PATH];
				const char *init_seg_name;
				GF_Config *dctx = gf_cfg_new(NULL, dash_ctx);
				sprintf(RepKeyName, "Representation_%s", dash_inputs[first_rep_in_set].representationID);
				if (dctx && ((init_seg_name = gf_cfg_get_key(dctx, RepKeyName, "InitializationSegment")) != NULL)) {
					skip_init_segment_creation = 1;
				}
				gf_cfg_del(dctx);
			}

			if (!skip_init_segment_creation) {
				Bool disable_bs_switching = 0;
				e = dash_inputs[first_rep_in_set].dasher_create_init_segment(dash_inputs, nb_dash_inputs, cur_adaptation_set+1, szInit, tmpdir, &disable_bs_switching);
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
					} else {
						strcpy(szSolvedSegName, seg_name);
					}
					segment_name = szSolvedSegName;
				}
				strcat(szOutName, "_dash");

				if (gf_url_get_resource_path(mpdfile, tmp)) {
					strcat(tmp, szOutName);
					strcpy(szOutName, tmp);
				}
				dash_opts.seg_rad_name = segment_name;

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
	if (mpd) fclose(mpd);
	return e;
}



#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
GF_EXPORT
GF_Err gf_media_fragment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec)
{
	return gf_media_isom_segment_file(input, output_file, max_duration_sec, NULL, NULL, 0);
}

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


