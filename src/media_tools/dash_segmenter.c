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
#include <gpac/config_file.h>


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
} TrackFragmenter;

static u64 get_next_sap_time(GF_ISOFile *input, u32 track, u32 sample_count, u32 sample_num)
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


#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

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
GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *output_file_name, const char *rep_id, const char *seg_rad_name, char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number)
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
			if (is_init) {
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


GF_EXPORT
GF_Err gf_media_segment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec, 
							  const char *mpd_name, u32 dash_mode, Double dash_duration_sec, char *seg_rad_name, char *seg_ext, 
							  s32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool use_url_template, u32 single_file_mode, 
							  const char *dash_ctx_file, GF_ISOFile *bs_switch_segment, char *rep_id, Bool first_in_set, Bool variable_seg_rad_name, Bool fragments_start_with_rap)
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
	GF_ISOFile *output;
	GF_ISOSample *sample, *next;
	GF_List *fragmenters;
	u32 MaxFragmentDuration, MaxSegmentDuration, SegmentDuration, maxFragDurationOverSegment;
	Double segment_start_time, file_duration, period_duration, max_segment_duration;
	u32 nb_segments, width, height, sample_rate, nb_channels, sar_w, sar_h, fps_num, fps_denum;
	char langCode[5];
	u32 index_start_range, index_end_range;
	Bool force_switch_segment = 0;
	Bool switch_segment = 0;
	Bool split_seg_at_rap = (dash_mode==2) ? 1 : 0;
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
	TrackFragmenter *tf, *tfref;
	FILE *mpd = NULL;
	FILE *mpd_segs = NULL;
	char SegmentName[GF_MAX_PATH];
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
	Bool is_bs_switching = (bs_switch_segment!=NULL) ? 1 : 0;
	const char *bs_switching_segment_name = NULL;

	SegmentName[0] = 0;
	SegmentDuration = 0;
	nb_samp = 0;
	fragmenters = NULL;
	if (!seg_ext) seg_ext = "m4s";

	if (bs_switch_segment) {
		bs_switching_segment_name = gf_url_get_resource_name(gf_isom_get_filename(bs_switch_segment));
	}

	/*need to precompute bandwidth*/
	bandwidth = 0;
	file_duration = 0;
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

	//create output file
	if (dash_mode) {
		if (dash_ctx_file) {
			dash_ctx = gf_cfg_new(NULL, dash_ctx_file);
			if (!dash_ctx) {
				FILE *t = fopen(dash_ctx_file, "wt");
				if (t) fclose(t);
				dash_ctx = gf_cfg_new(NULL, dash_ctx_file);
				
				if (dash_ctx) store_dash_params=1;
			}
		}

		opt = dash_ctx ? gf_cfg_get_key(dash_ctx, "DASH", "InitializationSegment") : NULL;
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
			const char *name = SegmentName;
			if (bs_switch_segment) name = bs_switching_segment_name;
			gf_cfg_set_key(dash_ctx, "DASH", "InitializationSegment", name);

			if (is_bs_switching)
				gf_cfg_set_key(dash_ctx, "DASH", "BitstreamSwitching", "yes");
		} else if (dash_ctx) {
			opt = gf_cfg_get_key(dash_ctx, "DASH", "BitstreamSwitching");
			if (opt && !strcmp(opt, "yes")) {
				is_bs_switching = 1;
				bs_switch_segment = output;
			}
		}
		mpd = gf_f64_open(mpd_name, "a+t");
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
	MaxSegmentDuration = (u32) (dash_duration_sec * 1000);

	/*in single segment mode, only one big SIDX is written between the end of the moov and the first fragment. 
	To speed-up writing, we do a first fragmentation pass without writing any sample to compute the number of segments and fragments per segment
	in order to allocate / write to file the sidx before the fragmentation. The sidx will then be rewritten when closing the last segment*/
	if (single_file_mode==1) {
		simulation_pass = 1;
		seg_rad_name = NULL;
	}
	/*if single file is requested, store all segments in the same file*/
	else if (single_file_mode==2) {
		seg_rad_name = NULL;
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

		GF_SAFEALLOC(tf, TrackFragmenter);
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
					e = gf_isom_setup_track_fragment(bs_switch_segment, tf->TrackID,
								defaultDescriptionIndex, defaultDuration,
								defaultSize, (u8) defaultRandomAccess,
								defaultPadding, defaultDegradationPriority);
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
			sprintf(sKey, "TrackID_%d", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dash_ctx, sKey, "NextDecodingTime");
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
	gf_isom_modify_alternate_brand(output, GF_4CC('m','s','i','x'), ((single_file_mode==1) && (subsegs_per_sidx>=0)) ? 1 : 0);

	//flush movie
	e = gf_isom_finalize_for_fragment(output, dash_mode ? 1 : 0);
	if (e) goto err_exit;

	start_range = 0;
	file_size = gf_isom_get_file_size(output);
	end_range = file_size - 1;
	init_seg_size = file_size;

	if (dash_ctx) {
		if (store_dash_params) {
			char szVal[1024];
			sprintf(szVal, LLU, init_seg_size);
			gf_cfg_set_key(dash_ctx, "DASH", "InitializationSegmentSize", szVal);
		} else {
			const char *opt = gf_cfg_get_key(dash_ctx, "DASH", "InitializationSegmentSize");
			if (opt) init_seg_size = atoi(opt);
		}
	}

restart_fragmentation_pass:

	for (i=0; i<gf_list_count(fragmenters); i++) {
		tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
		tf->split_sample_dts_shift = 0;
	}
	segment_start_time = 0;
	nb_segments = 0;

	nb_tracks_done = 0;
	ref_track_first_dts = (u64) -1;
	nb_done = 0;

	maxFragDurationOverSegment=0;
	if (dash_mode) switch_segment=1;

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

		opt = gf_cfg_get_key(dash_ctx, "DASH", "NextSegmentIndex");
		if (opt) cur_seg = atoi(opt);
		opt = gf_cfg_get_key(dash_ctx, "DASH", "NextFragmentIndex");
		if (opt) fragment_index = atoi(opt);
		opt = gf_cfg_get_key(dash_ctx, "DASH", "PeriodDuration");
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
				tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
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
					tf = (TrackFragmenter *)gf_list_get(fragmenters, i-1);
					if (tf == tfref) {
						tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
					}
				}
			} else {
				tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
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
							next_sap_time = get_next_sap_time(input, tf->OriginalTrack, tf->SampleCount, tf->SampleNum + 2);
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
					if (!fragments_start_with_rap || ( (next && next->IsRAP) || split_at_rap) ) {
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

		if (dash_mode) {

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


#if 0
				if (split_seg_at_rap) has_rap = 0;
				fprintf(stderr, "Seg#%d: Duration %d%s - Track times (ms): ", nb_segments, SegmentDuration, (has_rap == 2) ? " - Starts with RAP" : ((has_rap == 1) ? " - Contains RAP" : "") );
				for (i=0; i<count; i++) {
					tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
					fprintf(stderr, "%d ", (u32) ( (tf->last_sample_cts*1000.0)/tf->TimeScale) );
				}
				fprintf(stderr, "\n ");
#endif
				force_switch_segment=0;
				switch_segment=1;
				SegmentDuration=0;
				split_at_rap = 0;
				has_rap = 0;
				/*restore fragment duration*/
				MaxFragmentDuration = (u32) (max_duration_sec * 1000);

				if (!simulation_pass) {
					u64 idx_start_range, idx_end_range;
					
					gf_isom_close_segment(output, subsegs_per_sidx, ref_track_id, ref_track_first_dts, ref_track_next_cts, daisy_chain_sidx, flush_all_samples ? 1 : 0, &idx_start_range, &idx_end_range);
					ref_track_first_dts = (u64) -1;

					if (!seg_rad_name) {
						file_size = gf_isom_get_file_size(output);
						end_range = file_size - 1;
						if (single_file_mode!=1) {
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
		gf_isom_allocate_sidx(output, subsegs_per_sidx, daisy_chain_sidx, nb_segments_info, segments_info, &index_start_range, &index_end_range );
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

	if (dash_mode) {
		char buffer[1000];

		/*flush last segment*/
		if (!switch_segment) {
			u64 idx_start_range, idx_end_range;

			if (max_segment_duration * 1000 <= SegmentDuration) {
				max_segment_duration = SegmentDuration;
				max_segment_duration /= 1000;
			}

			gf_isom_close_segment(output, subsegs_per_sidx, ref_track_id, ref_track_first_dts, ref_track_next_cts, daisy_chain_sidx, 1, &idx_start_range, &idx_end_range);
			nb_segments++;

			if (!seg_rad_name) {
				file_size = gf_isom_get_file_size(output);
				end_range = file_size - 1;
				if (single_file_mode!=1) {
					fprintf(mpd_segs, "     <SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>\n", start_range, end_range, idx_start_range, idx_end_range);	
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
			if (!variable_seg_rad_name && first_in_set) {
				const char *rad_name = gf_url_get_resource_name(seg_rad_name);
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, 0, 0);
				fprintf(mpd, "   <SegmentTemplate timescale=\"1000\" duration=\"%d\" media=\"%s\" startNumber=\"1\"", (u32) (max_segment_duration*1000), SegmentName);	
				/*in BS switching we share the same IS for all reps*/
				if (is_bs_switching) {
					strcpy(SegmentName, bs_switching_segment_name);
				} else {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0);
				}
				fprintf(mpd, " initialization=\"%s\"", SegmentName);
				fprintf(mpd, "/>\n");
			}
			/*in BS switching we share the same IS for all reps, write the SegmentTemplate for the init segment*/
			else if (is_bs_switching && first_in_set) {
				fprintf(mpd, "   <SegmentTemplate initialization=\"%s\"/>\n", bs_switching_segment_name);
			}
		}


		fprintf(mpd, "   <Representation ");
		if (rep_id) fprintf(mpd, "id=\"%s\"", rep_id);
		else fprintf(mpd, "id=\"%p\"", output);
		fprintf(mpd, " mimeType=\"%s/mp4\" codecs=\"%s\"", audio_only ? "audio" : "video", szCodecs);
		if (width && height) {
			fprintf(mpd, " width=\"%d\" height=\"%d\"", width, height);
			gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);
			if (fps_denum>1)
				fprintf(mpd, " frameRate=\"%d/%d\"", fps_num, fps_denum);
			else
				fprintf(mpd, " frameRate=\"%d\"", fps_num);

			if (!sar_w) sar_w = 1;
			if (!sar_h) sar_h = 1;
			fprintf(mpd, " sar=\"%d:%d\"", sar_w, sar_h);

		}
		if (sample_rate) fprintf(mpd, " audioSamplingRate=\"%d\"", sample_rate);
		if (langCode[0]) fprintf(mpd, " lang=\"%s\"", langCode);
		if (segments_start_with_sap || split_seg_at_rap) {
			fprintf(mpd, " startWithSAP=\"%d\"", max_sap_type);
		} else {
			fprintf(mpd, " startWithSAP=\"false\"");
		}
		if ((single_file_mode==1) && segments_start_with_sap) fprintf(mpd, " subsegmentStartsWithSAP=\"%d\"", max_sap_type);
		
		fprintf(mpd, " bandwidth=\"%d\"", bandwidth);		
		fprintf(mpd, ">\n");

		if (nb_channels && !is_bs_switching) 
			fprintf(mpd, "    <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n", nb_channels);

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
			if (variable_seg_rad_name) {
				gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : seg_ext, 0, bandwidth, 0);
				fprintf(mpd, "    <SegmentTemplate timescale=\"1000\" duration=\"%d\" media=\"%s\" startNumber=\"1\"", (u32) (max_segment_duration*1000), SegmentName);	
				if (!is_bs_switching) {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE, is_bs_switching, SegmentName, output_file, rep_id, seg_rad_name, !stricmp(seg_ext, "null") ? NULL : "mp4", 0, 0, 0);
					fprintf(mpd, " initialization=\"%s\"", SegmentName);
				}
				fprintf(mpd, "/>\n");
			}
		} else if (single_file_mode==1) {
			fprintf(mpd, "    <BaseURL>%s</BaseURL>\n", gf_url_get_resource_name( gf_isom_get_filename(output) ) );	
			fprintf(mpd, "    <SegmentBase indexRangeExact=\"true\" indexRange=\"%d-%d\"/>\n", index_start_range, index_end_range);	
		} else {
			if (!seg_rad_name) {
				fprintf(mpd, "    <BaseURL>%s</BaseURL>\n", gf_url_get_resource_name( gf_isom_get_filename(output) ) );	
			}
			fprintf(mpd, "    <SegmentList timescale=\"1000\" duration=\"%d\">\n", (u32) (max_segment_duration*1000));	
			/*we are not in bitstreamSwitching mode*/
			if (!is_bs_switching) {
				fprintf(mpd, "     <Initialization");	
				if (!seg_rad_name) {
					fprintf(mpd, " range=\"0-"LLD"\"", init_seg_size-1);
				} else {
					gf_media_mpd_format_segment_name(GF_DASH_TEMPLATE_INITIALIZATION, is_bs_switching, SegmentName, output_file, rep_id, gf_url_get_resource_name( seg_rad_name) , !stricmp(seg_ext, "null") ? NULL : "mp4", 0, bandwidth, 0);
					fprintf(mpd, " sourceURL=\"%s\"", SegmentName);
				}
				fprintf(mpd, "/>\n");
			}
		}

		gf_f64_seek(mpd_segs, 0, SEEK_SET);
		while (!feof(mpd_segs)) {
			u32 r = fread(buffer, 1, 100, mpd_segs);
			gf_fwrite(buffer, 1, r, mpd);
		}

		if (!use_url_template && (single_file_mode!=1)) {
			fprintf(mpd, "    </SegmentList>\n");
		}

		fprintf(mpd, "   </Representation>\n");
	}

	/*store context*/
	if (dash_ctx) {
		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
			
			sprintf(sKey, "TrackID_%d", tf->TrackID);
			sprintf(sOpt, LLU, tf->InitialTSOffset + tf->next_sample_dts);
			gf_cfg_set_key(dash_ctx, sKey, "NextDecodingTime", sOpt);
		}
		sprintf(sOpt, "%d", cur_seg);
		gf_cfg_set_key(dash_ctx, "DASH", "NextSegmentIndex", sOpt);

		fragment_index = gf_isom_get_next_moof_number(output);
		sprintf(sOpt, "%d", fragment_index);
		gf_cfg_set_key(dash_ctx, "DASH", "NextFragmentIndex", sOpt);
		sprintf(sOpt, "%f", period_duration);
		gf_cfg_set_key(dash_ctx, "DASH", "PeriodDuration", sOpt);
	}

err_exit:
	if (fragmenters){
		while (gf_list_count(fragmenters)) {
			tf = (TrackFragmenter *)gf_list_get(fragmenters, 0);
			gf_free(tf);
			gf_list_rem(fragmenters, 0);
		}
		gf_list_del(fragmenters);
	}
	if (output) {
		if (e) gf_isom_delete(output);
		else gf_isom_close(output);
	}
	gf_set_progress("ISO File Fragmenting", nb_samp, nb_samp);
	if (mpd) fclose(mpd);
	if (mpd_segs) fclose(mpd_segs);
	if (dash_ctx) gf_cfg_del(dash_ctx);
	return e;
}

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

GF_EXPORT
GF_Err gf_media_mpd_start(char *mpd_name, GF_DashProfile profile, const char *title, const char *source, const char *copyright, const char *moreInfoURL, Bool use_url_template, u32 single_file_mode, char *dash_ctx, GF_ISOFile *init_segment, Bool bitstream_switching_mode, Double period_duration, Bool first_adaptation_set, u32 group_id, u32 max_width, u32 max_height, char *szMaxFPS, char *szLang, const char **mpd_base_urls, u32 nb_mpd_base_urls)
{
	u32 h, m, i;
	Double s;
	FILE *mpd = fopen(mpd_name, first_adaptation_set ? "wt" : "a+t");
	if (!mpd) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[MPD] Cannot open MPD file %s for writing\n", mpd_name));
		return GF_IO_ERR;
	}

	h = (u32) (period_duration/3600);
	m = (u32) (period_duration-h*60)/60;
	s = period_duration - h*3600 - m*60;

	if (first_adaptation_set) {
		fprintf(mpd, "<?xml version=\"1.0\"?>\n");
		fprintf(mpd, "<!-- MPD file Generated with GPAC version "GPAC_FULL_VERSION" -->\n");

		/*TODO what should we put for minBufferTime */
		fprintf(mpd, "<MPD type=\"static\" xmlns=\"urn:mpeg:DASH:schema:MPD:2011\" minBufferTime=\"PT1.5S\" mediaPresentationDuration=\"PT%dH%dM%.2fS\" ", 
			h, m, s);

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

		fprintf(mpd, " <Period start=\"PT0S\" duration=\"PT%dH%dM%.2fS\">\n", h, m, s);	
	}

	fprintf(mpd, "  <AdaptationSet segmentAlignment=\"true\"");
	if (bitstream_switching_mode) {
		fprintf(mpd, " bitstreamSwitching=\"true\"");
	}
	if (group_id) {
		fprintf(mpd, " group=\"%d\"", group_id);
	}
	if (max_width && max_height) {
		fprintf(mpd, " maxWidth=\"%d\" maxHeight=\"%d\" maxFrameRate=\"%s\"", max_width, max_height, szMaxFPS);
		gf_media_reduce_aspect_ratio(&max_width, &max_height);
		fprintf(mpd, " par=\"%d:%d\"", max_width, max_height);
	}
	if (szLang && szLang[0]) {
		fprintf(mpd, " lang=\"%s\"", szLang);
	}
	fprintf(mpd, ">\n");
	
	if (init_segment) {
		u32 i;
		char langCode[4];
		langCode[3] = 0;

		for (i=0; i<gf_isom_get_track_count(init_segment); i++) {
			u32 sr, nb_ch;
			u32 trackID = gf_isom_get_track_id(init_segment, i+1);

			gf_isom_get_media_language(init_segment, i+1, langCode);

			switch (gf_isom_get_media_type(init_segment, i+1) ) {
			case GF_ISOM_MEDIA_TEXT:
				fprintf(mpd, "    <ContentComponent id=\"%d\" contentType=\"text\" lang=\"%s\"/>\n", trackID, langCode);
				break;
			case GF_ISOM_MEDIA_VISUAL:
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"video\"/>\n", trackID);
				break;
			case GF_ISOM_MEDIA_SCENE:
			case GF_ISOM_MEDIA_DIMS:
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"application\" lang=\"%s\"/>\n", trackID, langCode);
				break;
			case GF_ISOM_MEDIA_AUDIO:
				gf_isom_get_audio_info(init_segment, i+1, 1, &sr, &nb_ch, NULL);
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"audio\" lang=\"%s\"/>\n", trackID, langCode);
				if (bitstream_switching_mode)
					fprintf(mpd, "   <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n", nb_ch);
				break;
			}
		}

		if (bitstream_switching_mode && !use_url_template && (single_file_mode!=1) ) {
			fprintf(mpd, "   <SegmentList>\n");	
			fprintf(mpd, "    <Initialization sourceURL=\"%s\"/>\n", gf_url_get_resource_name(gf_isom_get_filename(init_segment)));	
			fprintf(mpd, "   </SegmentList>\n");	
		}
	}

	fclose(mpd);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_media_mpd_end(char *mpd_name, Bool last_adaptation_set)
{
	FILE *mpd = fopen(mpd_name, "a+t");
	if (!mpd_name) return GF_IO_ERR;

    fprintf(mpd, "  </AdaptationSet>\n");
	if (last_adaptation_set) {
		fprintf(mpd, " </Period>\n");
		fprintf(mpd, "</MPD>");
	}
	fclose(mpd);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_media_fragment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec)
{
	return gf_media_segment_file(input, output_file, max_duration_sec, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0, NULL, NULL, 0, 0, 0, 0);	
}

#endif /*GPAC_DISABLE_ISOM*/

