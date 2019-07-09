/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Romain Bouqueau, Cyril Concolato
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

#include <gpac/filters.h>
#include <gpac/constants.h>


#include <gpac/internal/media_dev.h>

#include <gpac/xml.h>


#ifndef GPAC_DISABLE_MEDIA_IMPORT

GF_Err gf_import_message(GF_MediaImporter *import, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_AUTHOR, e ? GF_LOG_WARNING : GF_LOG_INFO)) {
		va_list args;
		char szMsg[1024];
		va_start(args, format);
		vsnprintf(szMsg, 1024, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_WARNING : GF_LOG_INFO), GF_LOG_AUTHOR, ("%s\n", szMsg) );
	}
#endif
	return e;
}


static void gf_media_update_bitrate_ex(GF_ISOFile *file, u32 track, Bool use_esd)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 i, count, timescale, db_size, cdur, csize;
	u64 time_wnd, rate, max_rate, avg_rate, bitrate;
	Double br;
	GF_ISOSample sample;

	db_size = 0;
	rate = max_rate = avg_rate = time_wnd = bitrate = 0;

	csize = 0;
	cdur = 0;
	if (gf_isom_get_media_type(file, track)==GF_ISOM_MEDIA_AUDIO) {
		csize = gf_isom_get_constant_sample_size(file, track);
		cdur = gf_isom_get_constant_sample_duration(file, track);
		if (cdur > 1) cdur = 0;
	}

	memset(&sample, 0, sizeof(GF_ISOSample));
	timescale = gf_isom_get_media_timescale(file, track);
	count = gf_isom_get_sample_count(file, track);

	if (csize && cdur) {
		db_size = 0;
		avg_rate = 8 * csize * timescale / cdur;
		bitrate = rate = avg_rate;
	} else {
		for (i=0; i<count; i++) {
			u32 di;
			GF_ISOSample *samp = gf_isom_get_sample_info_ex(file, track, i+1, &di, NULL, &sample);
			if (!samp) break;

			if (samp->dataLength > db_size) db_size = samp->dataLength;

			avg_rate += samp->dataLength;
			rate += samp->dataLength;
			if (samp->DTS > time_wnd + timescale) {
				if (rate > max_rate) max_rate = rate;
				time_wnd = samp->DTS;
				rate = 0;
			}
		}
	}

	br = (Double) (s64) gf_isom_get_media_duration(file, track);
	br /= timescale;
	if (br) {
		GF_ESD *esd = NULL;
		if (!csize || !cdur) {
			bitrate = (u32) ((Double) (s64)avg_rate / br);
			bitrate *= 8;
			max_rate *= 8;
		}
		if (!max_rate) max_rate = bitrate;

		if (use_esd) esd = gf_isom_get_esd(file, track, 1);
		if (esd) {
			esd->decoderConfig->avgBitrate = (u32) bitrate;
			esd->decoderConfig->maxBitrate = (u32) max_rate;
			esd->decoderConfig->bufferSizeDB = db_size;
			gf_isom_change_mpeg4_description(file, track, 1, esd);
			gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
			/*move to bps*/
			gf_isom_update_bitrate(file, track, 1, (u32) bitrate, (u32) max_rate, db_size);
		}
	}
	
#endif
}

GF_EXPORT
void gf_media_update_bitrate(GF_ISOFile *file, u32 track)
{
	gf_media_update_bitrate_ex(file, track, GF_FALSE);

}
void gf_media_get_video_timing(Double fps, u32 *timescale, u32 *dts_inc)
{
	u32 fps_1000 = (u32) (fps*1000 + 0.5);
	/*handle all drop-frame formats*/
	if (fps_1000==29970) {
		*timescale = 30000;
		*dts_inc = 1001;
	}
	else if (fps_1000==23976) {
		*timescale = 24000;
		*dts_inc = 1001;
	}
	else if (fps_1000==59940) {
		*timescale = 60000;
		*dts_inc = 1001;
	} else {
		*timescale = fps_1000;
		*dts_inc = 1000;
	}
}


static GF_Err gf_import_afx_sc3dmc(GF_MediaImporter *import, Bool mult_desc_allowed)
{
	GF_Err e;
	Bool destroy_esd;
	u32 size, track, di, dsi_len;
	GF_ISOSample *samp;
	u32 codecid;
	char *dsi, *data;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->tk_info[0].track_num = 1;
		import->tk_info[0].stream_type = GF_STREAM_SCENE;
		import->tk_info[0].codecid = GF_CODECID_AFX;
		import->nb_tracks = 1;
		return GF_OK;
	}

	e = gf_file_load_data(import->in_name, (u8 **) &data, &size);
	if (e) return gf_import_message(import, e, "Opening file %s failed", import->in_name);

	if ((s32) size < 0) return GF_IO_ERR;

	codecid = GF_CODECID_AFX;

	dsi = (char *)gf_malloc(1);
	dsi_len = 1;
	dsi[0] = GPAC_AFX_SCALABLE_COMPLEXITY;

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(0);
		destroy_esd = GF_TRUE;
	}
	/*update stream type/oti*/
	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->decoderConfig->streamType = GF_STREAM_SCENE;
	import->esd->decoderConfig->objectTypeIndication = codecid;
	import->esd->decoderConfig->bufferSizeDB = size;
	import->esd->decoderConfig->avgBitrate = 8*size;
	import->esd->decoderConfig->maxBitrate = 8*size;
	import->esd->slConfig->timestampResolution = 1000;

	if (dsi) {
		if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		if (import->esd->decoderConfig->decoderSpecificInfo->data) gf_free(import->esd->decoderConfig->decoderSpecificInfo->data);
		import->esd->decoderConfig->decoderSpecificInfo->data = dsi;
		import->esd->decoderConfig->decoderSpecificInfo->dataLength = dsi_len;
	}


	track = 0;
	if (mult_desc_allowed)
		track = gf_isom_get_track_by_id(import->dest, import->esd->ESID);
	if (!track)
		track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_SCENE, 1000);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;

	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	if (e) goto exit;
	//gf_isom_set_visual_info(import->dest, track, di, w, h);
	samp = gf_isom_sample_new();
	samp->IsRAP = RAP;
	samp->dataLength = size;
	if (import->initial_time_offset) samp->DTS = (u64) (import->initial_time_offset*1000);

	gf_import_message(import, GF_OK, "%s import %s", "SC3DMC", import->in_name);

	/*we must start a track from DTS = 0*/
	if (!gf_isom_get_sample_count(import->dest, track) && samp->DTS) {
		/*todo - we could add an edit list*/
		samp->DTS=0;
	}

	gf_set_progress("Importing SC3DMC", 0, 1);
	if (import->flags & GF_IMPORT_USE_DATAREF) {
		e = gf_isom_add_sample_reference(import->dest, track, di, samp, (u64) 0);
	} else {
		samp->data = data;
		e = gf_isom_add_sample(import->dest, track, di, samp);
		samp->data = NULL;
	}
	gf_set_progress("Importing SC3DMC", 1, 1);

	gf_isom_sample_del(&samp);

exit:
	gf_free(data);
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	return e;
}

GF_Err gf_import_isomedia(GF_MediaImporter *import)
{
	GF_Err e;
	u64 offset, sampDTS, duration, dts_offset;
	Bool is_nalu_video=GF_FALSE, has_seig;
	u32 track, di, trackID, track_in, i, num_samples, mtype, w, h, sr, sbr_sr, ch, mstype, cur_extract_mode, cdur, bps;
	s32 trans_x, trans_y;
	s16 layer;
	char *lang;
	const char *orig_name = gf_url_get_resource_name(gf_isom_get_filename(import->orig));
	Bool sbr, ps;
	GF_ISOSample *samp;
	GF_ESD *origin_esd;
	GF_InitialObjectDescriptor *iod;
	Bool is_cenc;
	u32 clone_flags;
	sampDTS = 0;
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		for (i=0; i<gf_isom_get_track_count(import->orig); i++) {
			u32 mtype;
			import->tk_info[i].track_num = gf_isom_get_track_id(import->orig, i+1);
			mtype = gf_isom_get_media_type(import->orig, i+1);
			switch (mtype) {
			case GF_ISOM_MEDIA_VISUAL:
				import->tk_info[i].stream_type = GF_STREAM_VISUAL;
				break;
			case GF_ISOM_MEDIA_AUDIO:
				import->tk_info[i].stream_type = GF_STREAM_AUDIO;
				break;
			case GF_ISOM_MEDIA_TEXT:
				import->tk_info[i].stream_type = GF_STREAM_TEXT;
				break;
			case GF_ISOM_MEDIA_SCENE:
				import->tk_info[i].stream_type = GF_STREAM_SCENE;
				break;
			default:
				import->tk_info[i].stream_type = mtype;
				break;
			}
			if (import->tk_info[i].stream_type == GF_STREAM_VISUAL) {
				gf_isom_get_visual_info(import->orig, i+1, 1, &import->tk_info[i].video_info.width, &import->tk_info[i].video_info.height);
			} else if (import->tk_info[i].stream_type == GF_STREAM_AUDIO) {
				gf_isom_get_audio_info(import->orig, i+1, 1, &import->tk_info[i].audio_info.sample_rate, &import->tk_info[i].audio_info.nb_channels, NULL);
			}
			lang = NULL;
			gf_isom_get_media_language(import->orig, i+1, &lang);
			if (lang) {
				import->tk_info[i].lang = GF_4CC(' ', lang[0], lang[1], lang[2]);
				gf_free(lang);
				lang = NULL;
			}
			gf_media_get_rfc_6381_codec_name(import->orig, i+1, import->tk_info[i].szCodecProfile, GF_FALSE, GF_FALSE);

			import->nb_tracks ++;
		}
		return GF_OK;
	}

	trackID = import->trackID;
	if (!trackID) {
		if (gf_isom_get_track_count(import->orig) != 1) return gf_import_message(import, GF_BAD_PARAM, "Several tracks in MP4 - please indicate track to import");
		trackID = gf_isom_get_track_id(import->orig, 1);
	}
	track_in = gf_isom_get_track_by_id(import->orig, trackID);
	if (!track_in) return gf_import_message(import, GF_URL_ERROR, "Cannot find track ID %d in file", trackID);

	origin_esd = gf_isom_get_esd(import->orig, track_in, 1);

	if (import->esd && origin_esd) {
		origin_esd->OCRESID = import->esd->OCRESID;
		/*there may be other things to import...*/
	}
	ps = GF_FALSE;
	sbr = GF_FALSE;
	sbr_sr = 0;
	cur_extract_mode = gf_isom_get_nalu_extract_mode(import->orig, track_in);
	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(import->orig);
	if (iod && (iod->tag != GF_ODF_IOD_TAG)) {
		gf_odf_desc_del((GF_Descriptor *) iod);
		iod = NULL;
	}
	mtype = gf_isom_get_media_type(import->orig, track_in);
	if (mtype==GF_ISOM_MEDIA_VISUAL) {
		u8 PL = iod ? iod->visual_profileAndLevel : 0xFE;
		w = h = 0;
		gf_isom_get_visual_info(import->orig, track_in, 1, &w, &h);
#ifndef GPAC_DISABLE_AV_PARSERS
		/*for MPEG-4 visual, always check size (don't trust input file)*/
		if (origin_esd && (origin_esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2)) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(origin_esd->decoderConfig->decoderSpecificInfo->data, origin_esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			w = dsi.width;
			h = dsi.height;
			PL = dsi.VideoPL;
		}
#endif
		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_VISUAL, PL);
	}
	else if (mtype==GF_ISOM_MEDIA_AUDIO) {
		u8 PL = iod ? iod->audio_profileAndLevel : 0xFE;
		bps = 16;
		sr = ch = sbr_sr = 0;
		sbr = GF_FALSE;
		ps = GF_FALSE;
		gf_isom_get_audio_info(import->orig, track_in, 1, &sr, &ch, &bps);
#ifndef GPAC_DISABLE_AV_PARSERS
		if (origin_esd && (origin_esd->decoderConfig->objectTypeIndication==GF_CODECID_AAC_MPEG4)) {
			if (origin_esd->decoderConfig->decoderSpecificInfo) {
				GF_M4ADecSpecInfo dsi;
				gf_m4a_get_config(origin_esd->decoderConfig->decoderSpecificInfo->data, origin_esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				sr = dsi.base_sr;
				if (dsi.has_sbr) sbr_sr = dsi.sbr_sr;
				ch = dsi.nb_chan;
				PL = dsi.audioPL;
				sbr = dsi.has_sbr ? ((dsi.base_object_type==GF_M4A_AAC_SBR || dsi.base_object_type==GF_M4A_AAC_PS) ? 2 : 1) : GF_FALSE;
				ps = dsi.has_ps;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Missing DecoderSpecificInfo in MPEG-4 AAC stream\n"));
			}
		}
#endif
		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_AUDIO, PL);
	}
	else if (mtype==GF_ISOM_MEDIA_SUBPIC) {
		w = h = 0;
		trans_x = trans_y = 0;
		layer = 0;
		if (origin_esd && origin_esd->decoderConfig->objectTypeIndication == GF_CODECID_SUBPIC) {
			gf_isom_get_track_layout_info(import->orig, track_in, &w, &h, &trans_x, &trans_y, &layer);
		}
	}

	gf_odf_desc_del((GF_Descriptor *) iod);
	if ( ! gf_isom_get_track_count(import->dest)) {
		u32 timescale = gf_isom_get_timescale(import->orig);
		gf_isom_set_timescale(import->dest, timescale);
	}
	clone_flags = GF_ISOM_CLONE_TRACK_NO_QT;
	if (import->asemode == GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF)
		clone_flags = 0;

	if (import->flags & GF_IMPORT_USE_DATAREF) clone_flags |= GF_ISOM_CLONE_TRACK_KEEP_DREF;
	e = gf_isom_clone_track(import->orig, track_in, import->dest, clone_flags, &track);
	if (e) goto exit;

	di = 1;

	if (import->esd && import->esd->ESID) {
		e = gf_isom_set_track_id(import->dest, track, import->esd->ESID);
		if (e) goto exit;
	}

	import->final_trackID = gf_isom_get_track_id(import->dest, track);
	if (import->esd && import->esd->dependsOnESID) {
		gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_DECODE, import->esd->dependsOnESID);
	}
	if (import->trackID && !(import->flags & GF_IMPORT_KEEP_REFS)) {
		gf_isom_remove_track_references(import->dest, track);
	}

	mstype = gf_isom_get_media_subtype(import->orig, track_in, di);
	switch (mtype) {
	case GF_ISOM_MEDIA_VISUAL:
		gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - Video (size %d x %d)", orig_name, trackID, w, h);
		break;
	case GF_ISOM_MEDIA_AUDIO:
	{
		if (ps) {
			gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - HE-AACv2 (SR %d - SBR-SR %d - %d channels)", orig_name, trackID, sr, sbr_sr, ch);
		} else if (sbr) {
			gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - HE-AAC (SR %d - SBR-SR %d - %d channels)", orig_name, trackID, sr, sbr_sr, ch);
		} else {
			gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - Audio (SR %d - %d channels)", orig_name, trackID, sr, ch);
		}
		if (import->asemode != GF_IMPORT_AUDIO_SAMPLE_ENTRY_NOT_SET) {
			gf_isom_get_audio_info(import->orig, track_in, 1, &sr, &ch, &bps);
			gf_isom_set_audio_info(import->dest, track, 1, sr, ch, bps, import->asemode);
		}
	}
	break;
	case GF_ISOM_MEDIA_SUBPIC:
		gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - VobSub (size %d x %d)", orig_name, trackID, w, h);
		break;
	default:
	{
		char szT[5];
		mstype = gf_isom_get_mpeg4_subtype(import->orig, track_in, di);
		if (!mstype) mstype = gf_isom_get_media_subtype(import->orig, track_in, di);
		strcpy(szT, gf_4cc_to_str(mtype));
		gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - media type \"%s:%s\"", orig_name, trackID, szT, gf_4cc_to_str(mstype));
	}
	break;
	}

	//this may happen with fragmented files
	dts_offset = 0;
	samp = gf_isom_get_sample_info(import->orig, track_in, 1, &di, &offset);
	if (samp) {
		dts_offset = samp->DTS;
		gf_isom_sample_del(&samp);
	}

	is_cenc = gf_isom_is_cenc_media(import->orig, track_in, 1);
	if (gf_isom_is_media_encrypted(import->orig, track_in, 1)) {
		gf_isom_get_original_format_type(import->orig, track_in, 1, &mstype);
	}
	has_seig = GF_FALSE;
	if (is_cenc && gf_isom_has_cenc_sample_group(import->orig, track_in)) {
		has_seig = GF_TRUE;
	}

	cdur = gf_isom_get_constant_sample_duration(import->orig, track_in);
	gf_isom_enable_raw_pack(import->orig, track_in, 2048);
	
	duration = (u64) (((Double)import->duration * gf_isom_get_media_timescale(import->orig, track_in)) / 1000);
	gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT);

	if (import->xps_inband) {
		if (is_cenc ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] CENC media detected - cannot switch parameter set storage mode\n"));
		} else if (import->flags & GF_IMPORT_USE_DATAREF) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] Cannot switch parameter set storage mode when using data reference\n"));
		} else {
			switch (mstype) {
			case GF_ISOM_SUBTYPE_AVC_H264:
				gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
				gf_isom_avc_set_inband_config(import->dest, track, 1, (import->xps_inband==2) ? GF_TRUE : GF_FALSE);
				break;
			case GF_ISOM_SUBTYPE_HVC1:
				gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
				gf_isom_hevc_set_inband_config(import->dest, track, 1, (import->xps_inband==2) ? GF_TRUE : GF_FALSE);
				break;
			}
		}
	}
	switch (mstype) {
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_SVC_H264:
	case GF_ISOM_SUBTYPE_MVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
	case GF_ISOM_SUBTYPE_HEV1:
	case GF_ISOM_SUBTYPE_HVC1:
	case GF_ISOM_SUBTYPE_LHE1:
	case GF_ISOM_SUBTYPE_LHV1:
	case GF_ISOM_SUBTYPE_HVT1:
		is_nalu_video = GF_TRUE;
		break;
	}
	num_samples = gf_isom_get_sample_count(import->orig, track_in);

	if (is_cenc) {
		u32 container_type;
		e = gf_isom_cenc_get_sample_aux_info(import->orig, track_in, 0, NULL, &container_type);
		if (e)
			goto exit;
		e = gf_isom_cenc_allocate_storage(import->dest, track, container_type, 0, 0, NULL);
		if (e) goto exit;
		e = gf_isom_clone_pssh(import->dest, import->orig, GF_FALSE);
		if (e) goto exit;
	}
	for (i=0; i<num_samples; i++) {
		if (import->flags & GF_IMPORT_USE_DATAREF) {
			samp = gf_isom_get_sample_info(import->orig, track_in, i+1, &di, &offset);
			if (!samp) {
				e = gf_isom_last_error(import->orig);
				goto exit;
			}
			samp->DTS -= dts_offset;
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else {
			samp = gf_isom_get_sample(import->orig, track_in, i+1, &di);
			if (!samp) {
				/*couldn't get the sample, but still move on*/
				goto exit;
			}
			samp->DTS -= dts_offset;

			/*if packed samples (raw) and import duration is set, adjust the number of samples to add*/
			if (samp->nb_pack && duration && (samp->DTS + samp->nb_pack*cdur > duration) ) {
				u32 nb_samp = (u32) ( (duration - samp->DTS) / cdur);
				u32 csize = samp->dataLength / samp->nb_pack;
				if (!nb_samp) {
					gf_isom_sample_del(&samp);
					break;
				}
				samp->dataLength = csize*nb_samp;
				samp->nb_pack = nb_samp;
				duration = samp->DTS-1;
			}

			/*if not first sample and same DTS as previous sample, force DTS++*/
			if (i && (samp->DTS<=sampDTS)) {
				if (i+1 < num_samples) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] 0-duration sample detected at DTS %u - adjusting\n", samp->DTS));
				}
				samp->DTS = sampDTS + 1;
			}
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		sampDTS = samp->DTS;
		if (samp->nb_pack)
			i+= samp->nb_pack-1;
		gf_isom_sample_del(&samp);

		gf_isom_copy_sample_info(import->dest, track, import->orig, track_in, i+1);

		if (e)
			goto exit;
		if (is_cenc) {
			GF_CENCSampleAuxInfo *sai;
			u32 container_type, len, j;
			Bool Is_Encrypted;
			u8 IV_size;
			bin128 KID;
			u8 crypt_byte_block, skip_byte_block;
			u8 constant_IV_size;
			bin128 constant_IV;
			GF_BitStream *bs;
			u8 *buffer;

			sai = NULL;
			e = gf_isom_cenc_get_sample_aux_info(import->orig, track_in, i+1, &sai, &container_type);
			if (e)
				goto exit;

			e = gf_isom_get_sample_cenc_info(import->orig, track_in, i+1, &Is_Encrypted, &IV_size, &KID, &crypt_byte_block, &skip_byte_block, &constant_IV_size, &constant_IV);
			if (e) goto exit;

			if (Is_Encrypted) {
				bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				gf_bs_write_data(bs, (const char *)sai->IV, IV_size);
				if (sai->subsample_count) {
					gf_bs_write_u16(bs, sai->subsample_count);
					for (j = 0; j < sai->subsample_count; j++) {
						gf_bs_write_u16(bs, sai->subsamples[j].bytes_clear_data);
						gf_bs_write_u32(bs, sai->subsamples[j].bytes_encrypted_data);
					}
				}
				gf_isom_cenc_samp_aux_info_del(sai);
				gf_bs_get_content(bs, &buffer, &len);
				gf_bs_del(bs);
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, IV_size, buffer, len, is_nalu_video, NULL, GF_FALSE);
				gf_free(buffer);
			} else {
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, IV_size, NULL, 0, is_nalu_video, NULL, GF_FALSE);
			}
			if (e) goto exit;

			if (has_seig) {
				e = gf_isom_set_sample_cenc_group(import->dest, track, i+1, Is_Encrypted, IV_size, KID, crypt_byte_block, skip_byte_block, constant_IV_size, constant_IV);
				if (e) goto exit;
			}
		}

		gf_set_progress("Importing ISO File", i+1, num_samples);
		if (duration && (sampDTS > duration) ) break;
	}

	//adjust last sample duration
	if (i==num_samples) {
		u32 dur = gf_isom_get_sample_duration(import->orig, track_in, num_samples);
		gf_isom_set_last_sample_duration(import->dest, track, dur);
	} else {
		s64 mediaOffset;
		if (gf_isom_get_edit_list_type(import->orig, track_in, &mediaOffset)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISOBMF Import] Multiple edits found in source media, import may be broken\n"));
		}
		gf_isom_update_edit_list_duration(import->dest, track);
		gf_isom_update_duration(import->dest);
	}

	if (gf_isom_has_time_offset(import->orig, track_in)==2) {
		e = gf_isom_set_composition_offset_mode(import->dest, track, GF_TRUE);
		if (e)
			goto exit;
	}


	if (import->esd) {
		if (!import->esd->slConfig) {
			import->esd->slConfig = origin_esd ? origin_esd->slConfig : NULL;
			if (origin_esd) origin_esd->slConfig = NULL;
		}
		if (!import->esd->decoderConfig) {
			import->esd->decoderConfig = origin_esd ? origin_esd->decoderConfig : NULL;
			if (origin_esd) origin_esd->decoderConfig = NULL;
		}
	}
	gf_media_update_bitrate_ex(import->dest, track, GF_TRUE);
	if (mtype == GF_ISOM_MEDIA_VISUAL) {
		if (import->flags & GF_IMPORT_USE_CCST) {
			e = gf_isom_set_image_sequence_coding_constraints(import->dest, track, di, GF_FALSE, GF_FALSE, GF_TRUE, 15);
			if (e) goto exit;
		}
		if (import->is_alpha) {
			e = gf_isom_set_image_sequence_alpha(import->dest, track, di, GF_FALSE);
			if (e) goto exit;
		}
	}

exit:
	if (origin_esd) gf_odf_desc_del((GF_Descriptor *) origin_esd);
	gf_isom_set_nalu_extract_mode(import->orig, track_in, cur_extract_mode);
	return e;
}

GF_EXPORT
GF_Err gf_media_nal_rewrite_samples(GF_ISOFile *file, u32 track, u32 new_size)
{
	u32 i, count, di, remain, msize;
	char *buffer;

	msize = 4096;
	buffer = (char*)gf_malloc(sizeof(char)*msize);
	count = gf_isom_get_sample_count(file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(file, track, i+1, &di);
		GF_BitStream *oldbs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
		GF_BitStream *newbs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		u32 prev_size = 8*gf_isom_get_nalu_length_field(file, track, di);
		if (!prev_size) return GF_NON_COMPLIANT_BITSTREAM;
		
		remain = samp->dataLength;
		while (remain) {
			u32 size = gf_bs_read_int(oldbs, prev_size);
			gf_bs_write_int(newbs, size, new_size);
			remain -= prev_size/8;
			if (size>msize) {
				msize = size;
				buffer = (char*)gf_realloc(buffer, sizeof(char)*msize);
			}
			gf_bs_read_data(oldbs, buffer, size);
			gf_bs_write_data(newbs, buffer, size);
			remain -= size;
		}
		gf_bs_del(oldbs);
		gf_free(samp->data);
		samp->data = NULL;
		samp->dataLength = 0;
		gf_bs_get_content(newbs, &samp->data, &samp->dataLength);
		gf_bs_del(newbs);
		gf_isom_update_sample(file, track, i+1, samp, GF_TRUE);
		gf_isom_sample_del(&samp);
	}
	gf_free(buffer);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_media_import_chapters_file(GF_MediaImporter *import)
{
	s32 read=0;
	GF_Err e;
	u32 state, offset;
	u32 cur_chap;
	u64 ts;
	u32 i, h, m, s, ms, fr, fps;
	char line[1024];
	char szTitle[1024];
	FILE *f = gf_fopen(import->in_name, "rt");
	if (!f) return GF_URL_ERROR;

	read = (s32) fread(line, 1, 4, f);
	if (read < 0) {
		e = GF_IO_ERR;
		goto err_exit;
	}
	if (read < 4) {
		e = GF_URL_ERROR;
		goto err_exit;
	}

	if ((line[0]==(char)(0xFF)) && (line[1]==(char)(0xFE))) {
		if (!line[2] && !line[3]) {
			e = GF_NOT_SUPPORTED;
			goto err_exit;
		}
		offset = 2;
	} else if ((line[0]==(char)(0xFE)) && (line[1]==(char)(0xFF))) {
		if (!line[2] && !line[3]) {
			e = GF_NOT_SUPPORTED;
			goto err_exit;
		}
		offset = 2;
	} else if ((line[0]==(char)(0xEF)) && (line[1]==(char)(0xBB)) && (line[2]==(char)(0xBF))) {
		/*we handle UTF8 as asci*/
		offset = 3;
	} else {
		offset = 0;
	}
	gf_fseek(f, offset, SEEK_SET);

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		Bool is_chap_or_sub = GF_FALSE;
		import->nb_tracks = 0;
		while (!is_chap_or_sub && (fgets(line, 1024, f) != NULL)) {
			char *sep;
			strlwr(line);

			if (strstr(line, "addchapter(")) is_chap_or_sub = GF_TRUE;
			else if (strstr(line, "-->")) is_chap_or_sub = GF_TRUE;
			else if ((sep = strstr(line, "chapter")) != NULL) {
				sep+=7;
				if (!strncmp(sep+1, "name", 4)) is_chap_or_sub = GF_TRUE;
				else if (!strncmp(sep+2, "name", 4)) is_chap_or_sub = GF_TRUE;
				else if (!strncmp(sep+3, "name", 4)) is_chap_or_sub = GF_TRUE;
				else if (strstr(line, "Zoom") || strstr(line, "zoom")) is_chap_or_sub = GF_TRUE;
			}
		}
		gf_fclose(f);
		if (is_chap_or_sub) {
			import->nb_tracks = 1;
			import->tk_info[0].stream_type = GF_STREAM_TEXT;
			import->tk_info[0].is_chapter = GF_TRUE;
			return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	}

	e = gf_isom_remove_chapter(import->dest, 0, 0);
	if (e) goto err_exit;

	if (!import->video_fps.num || !import->video_fps.den ) {
		/*try to figure out the frame rate*/
		for (i=0; i<gf_isom_get_track_count(import->dest); i++) {
			GF_ISOSample *samp;
			u32 ts, inc;
            u32 mtype = gf_isom_get_media_type(import->dest, i+1);
			if (!gf_isom_is_video_subtype(mtype)) continue;
			if (gf_isom_get_sample_count(import->dest, i+1) < 20) continue;
			samp = gf_isom_get_sample_info(import->dest, 1, 2, NULL, NULL);
			inc = (u32) samp->DTS;
			if (!inc) inc=1;
			ts = gf_isom_get_media_timescale(import->dest, i+1);
			import->video_fps.num = ts;
			import->video_fps.den = inc;
			gf_isom_sample_del(&samp);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[Chapter import] Guessed video frame rate %u/%u\n", ts, inc));
			break;
		}
		if (!import->video_fps.num || !import->video_fps.den) {
			import->video_fps.num = 25;
			import->video_fps.den = 1;
		}
	}

	cur_chap = 0;
	ts = 0;
	state = 0;
	while (fgets(line, 1024, f) != NULL) {
		char *title = NULL;
		u32 off = 0;
		char *sL;
		while (1) {
			u32 len = (u32) strlen(line);
			if (!len) break;
			switch (line[len-1]) {
			case '\n':
			case '\t':
			case '\r':
			case ' ':
				line[len-1] = 0;
				continue;
			}
			break;
		}

		while (line[off]==' ') off++;
		if (!strlen(line+off)) continue;
		sL = line+off;

		szTitle[0] = 0;
		/*ZoomPlayer chapters*/
		if (!strnicmp(sL, "AddChapter(", 11)) {
			u32 nb_fr;
			sscanf(sL, "AddChapter(%u,%s)", &nb_fr, szTitle);
			ts = nb_fr;
			ts *= 1000;
			ts = (u64) (((s64) ts )  *import->video_fps.den / import->video_fps.num);
			sL = strchr(sL, ',');
			strcpy(szTitle, sL+1);
			sL = strrchr(szTitle, ')');
			if (sL) sL[0] = 0;
		} else if (!strnicmp(sL, "AddChapterBySecond(", 19)) {
			u32 nb_s;
			sscanf(sL, "AddChapterBySecond(%u,%s)", &nb_s, szTitle);
			ts = nb_s;
			ts *= 1000;
			sL = strchr(sL, ',');
			strcpy(szTitle, sL+1);
			sL = strrchr(szTitle, ')');
			if (sL) sL[0] = 0;
		} else if (!strnicmp(sL, "AddChapterByTime(", 17)) {
			u32 h, m, s;
			sscanf(sL, "AddChapterByTime(%u,%u,%u,%s)", &h, &m, &s, szTitle);
			ts = 3600*h + 60*m + s;
			ts *= 1000;
			sL = strchr(sL, ',');
			if (sL) sL = strchr(sL+1, ',');
			if (sL) sL = strchr(sL+1, ',');
			if (sL) strcpy(szTitle, sL+1);
			sL = strrchr(szTitle, ')');
			if (sL) sL[0] = 0;
		}
		/*regular or SMPTE time codes*/
		else if ((strlen(sL)>=8) && (sL[2]==':') && (sL[5]==':')) {
			title = NULL;
			if (strlen(sL)==8) {
				sscanf(sL, "%02u:%02u:%02u", &h, &m, &s);
				ts = (h*3600 + m*60+s)*1000;
			}
			else {
				char szTS[20], *tok;
				strncpy(szTS, sL, 18);
				tok = strrchr(szTS, ' ');
				if (tok) {
					title = strchr(sL, ' ') + 1;
					while (title[0]==' ') title++;
					if (strlen(title)) strcpy(szTitle, title);
					tok[0] = 0;
				}
				ts = 0;
				h = m = s = ms = 0;

				if (sscanf(szTS, "%u:%u:%u;%u/%u", &h, &m, &s, &fr, &fps)==5) {
					ts = (h*3600 + m*60+s)*1000 + 1000*fr/fps;
				} else if (sscanf(szTS, "%u:%u:%u;%u", &h, &m, &s, &fr)==4) {
					ts = (h*3600 + m*60+s);
					ts = (s64) (((import->video_fps.num*((s64)ts) + import->video_fps.den*fr) * 1000 ) / import->video_fps.num ) ;
				} else if (sscanf(szTS, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					//microseconds used
					if (ms>=1000) ms /= 1000;
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					//microseconds used
					if (ms>=1000) ms /= 1000;
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u:%u", &h, &m, &s, &ms) == 4) {
					//microseconds used
					if (ms>=1000) ms /= 1000;
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u", &h, &m, &s) == 3) {
					ts = (h*3600 + m*60+s) * 1000;
				}
			}
		}
		/*CHAPTERX= and CHAPTERXNAME=*/
		else if (!strnicmp(sL, "CHAPTER", 7)) {
			u32 idx;
			char szTemp[20], *str;
			strncpy(szTemp, sL, 19);
			str = strrchr(szTemp, '=');
			if (!str) continue;
			str[0] = 0;
			strlwr(szTemp);
			idx = cur_chap;
			str = strchr(sL, '=');
			str++;
			if (strstr(szTemp, "name")) {
				sscanf(szTemp, "chapter%uname", &idx);
				strcpy(szTitle, str);
				if (idx!=cur_chap) {
					cur_chap=idx;
					state = 0;
				}
				state++;
			} else {
				sscanf(szTemp, "chapter%u", &idx);
				if (idx!=cur_chap) {
					cur_chap=idx;
					state = 0;
				}
				state++;

				ts = 0;
				h = m = s = ms = 0;
				if (sscanf(str, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					//microseconds used
					if (ms>=1000) ms/=1000;
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(str, "%u:%u:%u:%u", &h, &m, &s, &ms) == 4) {
					//microseconds used
					if (ms>=1000) ms/=1000;
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(str, "%u:%u:%u", &h, &m, &s) == 3) {
					ts = (h*3600 + m*60+s) * 1000;
				}
			}
			if (state==2) {
				e = gf_isom_add_chapter(import->dest, 0, ts, szTitle);
				if (e) goto err_exit;
				state = 0;
			}
			continue;
		}
		else continue;

		if (strlen(szTitle)) {
			e = gf_isom_add_chapter(import->dest, 0, ts, szTitle);
		} else {
			e = gf_isom_add_chapter(import->dest, 0, ts, NULL);
		}
		if (e) goto err_exit;
	}

err_exit:
	gf_fclose(f);
	return e;
}

GF_EXPORT
GF_Err gf_media_import_chapters(GF_ISOFile *file, char *chap_file, GF_Fraction import_fps)
{
	GF_MediaImporter import;
	memset(&import, 0, sizeof(GF_MediaImporter));
	import.dest = file;
	import.in_name = chap_file;
	import.video_fps = import_fps;
	import.streamFormat = "CHAP";
	return gf_media_import(&import);
}

static void on_import_setup_failure(GF_Filter *f, void *on_setup_error_udta, GF_Err e)
{
	GF_MediaImporter *importer = (GF_MediaImporter *)on_setup_error_udta;
	if (importer)
		importer->last_error = e;
}

GF_EXPORT
GF_Err gf_media_import(GF_MediaImporter *importer)
{
	GF_Err e;
	u32 i, count;
	GF_FilterSession *fsess;
	GF_Filter *prober, *src_filter;
	char *ext;
	char *fmt = "";
	if (!importer || (!importer->dest && (importer->flags!=GF_IMPORT_PROBE_ONLY)) || (!importer->in_name && !importer->orig) ) return GF_BAD_PARAM;

	if (importer->orig) return gf_import_isomedia(importer);

	if (importer->force_ext) {
		ext = importer->force_ext;
	} else {
		ext = strrchr(importer->in_name, '.');
		if (!ext) ext = "";
	}

	if (importer->streamFormat) fmt = importer->streamFormat;

	/*if isobmf and probing or no filter, direct copy*/
	if (gf_isom_probe_file(importer->in_name) && (!importer->filter_chain || (importer->flags & GF_IMPORT_PROBE_ONLY) ) ) {
		importer->orig = gf_isom_open(importer->in_name, GF_ISOM_OPEN_READ, NULL);
		if (importer->orig) {
			e = gf_import_isomedia(importer);
			gf_isom_delete(importer->orig);
			importer->orig = NULL;
			return e;
		}
	}

	/*SC3DMC*/
	if (!strnicmp(ext, ".s3d", 4) || !stricmp(fmt, "SC3DMC") )
		return gf_import_afx_sc3dmc(importer, GF_TRUE);
	/* chapter */
	else if (!strnicmp(ext, ".txt", 4) || !strnicmp(ext, ".chap", 5) || !stricmp(fmt, "CHAP") )
		return gf_media_import_chapters_file(importer);

#ifdef FILTER_FIXME
	#error "importer TO CHECK: SAF, TS"
#endif

	e = GF_OK;
	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		return gf_import_message(importer, GF_BAD_PARAM, "[Importer] Cannot load filter session");
	}
	importer->last_error = GF_OK;

	if (importer->flags & GF_IMPORT_PROBE_ONLY) {
		prober = gf_fs_load_filter(fsess, "probe");

		src_filter = gf_fs_load_source(fsess, importer->in_name, "index_dur=0", NULL, &e);
		if (e) {
			gf_fs_run(fsess);
			gf_fs_del(fsess);
			return gf_import_message(importer, e, "[Importer] Cannot load filter for input file \"%s\"", importer->in_name);
		}
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_test_mode()) {
			on_import_setup_failure(NULL, NULL, GF_OK);
		}
#endif

		gf_filter_set_setup_failure_callback(prober, src_filter, on_import_setup_failure, importer);
		gf_fs_run(fsess);

		if (importer->last_error) {
			gf_fs_del(fsess);
			return gf_import_message(importer, importer->last_error, "[Importer] Error probing %s", importer->in_name);
		}

		importer->nb_tracks = 0;
		count = gf_filter_get_ipid_count(prober);
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			struct __track_import_info *tki = &importer->tk_info[importer->nb_tracks];
			GF_FilterPid *pid = gf_filter_get_ipid(prober, i);

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
			tki->stream_type = p ? p->value.uint : GF_STREAM_UNKNOWN;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
			tki->codecid = p ? p->value.uint : GF_CODECID_NONE;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
			if (p && p->value.string) tki->lang = GF_4CC(p->value.string[0], p->value.string[1], p->value.string[2], ' ');
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
			tki->track_num = p ? p->value.uint : 1;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
			if (p) tki->mpeg4_es_id = p->value.uint;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
			if (p) tki->prog_num = p->value.uint;

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
			if (p) {
				Double d = p->value.frac.num;
				d*=1000;
				if (p->value.frac.den) d /= p->value.frac.den;
				if (d > importer->probe_duration) importer->probe_duration = (u64) d;
			}

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
			if (p) {
				tki->video_info.width = p->value.uint;
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
				if (p) tki->video_info.height = p->value.uint;
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
				if (p) {
					tki->video_info.FPS = p->value.frac.num;
					if (p->value.frac.den) tki->video_info.FPS /= p->value.frac.den;
				}
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
				if (p) tki->video_info.par = (p->value.frac.num << 16) | p->value.frac.den;
			}
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
			if (p) {
				tki->audio_info.sample_rate = p->value.uint;
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
				if (p) tki->audio_info.nb_channels = p->value.uint;
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLES_PER_FRAME);
				if (p) tki->audio_info.samples_per_frame = p->value.uint;
			}
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_CAN_DATAREF);
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_SUBTYPE);
			if (p) tki->media_subtype = p->value.uint;

			importer->nb_tracks++;
		}
	} else {
		char *args = NULL;
		char szSubArg[1024];
		Bool source_id_set = GF_FALSE;
		GF_Filter *isobmff_mux, *source;
		GF_Filter *filter_orig=NULL;

		//mux args
		e = gf_dynstrcat(&args, "mp4mx:importer", ":");

		sprintf(szSubArg, "file=%p", importer->dest);
		e |= gf_dynstrcat(&args, szSubArg, ":");
		if (importer->trackID) {
			sprintf(szSubArg, "SID=1#PID=%d", importer->trackID);
			e |= gf_dynstrcat(&args, szSubArg, ":");
		}
		if (importer->filter_dst_opts)
			e |= gf_dynstrcat(&args, importer->filter_dst_opts, ":");

		if (importer->flags & GF_IMPORT_FORCE_MPEG4)
			e |= gf_dynstrcat(&args, "m4sys", ":");
		if (importer->flags & GF_IMPORT_USE_DATAREF)
			e |= gf_dynstrcat(&args, "dref", ":");
		if (importer->flags & GF_IMPORT_NO_EDIT_LIST)
			e |= gf_dynstrcat(&args, "noedit", ":");
		if (importer->flags & GF_IMPORT_FORCE_PACKED)
			e |= gf_dynstrcat(&args, "pack_nal", ":");
		if (importer->xps_inband==1)
			e |= gf_dynstrcat(&args, "xps_inband=all", ":");
		else if (importer->xps_inband==2)
			e |= gf_dynstrcat(&args, "xps_inband=both", ":");
		if (importer->esd && importer->esd->ESID) {
			sprintf(szSubArg, "tkid=%d", importer->esd->ESID);
			e |= gf_dynstrcat(&args, szSubArg, ":");
		}

		if (importer->duration) {
			sprintf(szSubArg, "idur=%d/1000", importer->duration);
			e |= gf_dynstrcat(&args, szSubArg, ":");
		}
		if (importer->frames_per_sample) {
			sprintf(szSubArg, "pack3gp=%d", importer->frames_per_sample);
			e |= gf_dynstrcat(&args, szSubArg, ":");
		}
		if (importer->asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2) { e |= gf_dynstrcat(&args, "ase=v0s", ":"); }
		else if (importer->asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG) { e |= gf_dynstrcat(&args, "ase=v1", ":"); }
		else if (importer->asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF) { e |= gf_dynstrcat(&args, "ase=v1qt", ":"); }

		if (importer->flags & GF_IMPORT_USE_CCST) {e |= gf_dynstrcat(&args, "ccst", ":"); }

		if (e) {
			gf_free(args);
			return gf_import_message(importer, e, "[Importer] Cannot load ISOBMFF muxer arguments");
		}

		isobmff_mux = gf_fs_load_filter(fsess, args);
		gf_free(args);
		args = NULL;
		if (!isobmff_mux) {
			gf_fs_del(fsess);
			return gf_import_message(importer, GF_FILTER_NOT_FOUND, "[Importer] Cannot load ISOBMFF muxer");
		}

		//filter chain
		if (importer->filter_chain) {
			GF_Filter *prev_filter=NULL;
			char *fargs = (char *) importer->filter_chain;
			while (fargs) {
				GF_Filter *f;
				char *sep = strstr(fargs, "@@");
				if (sep) sep[0] = 0;
				f = gf_fs_load_filter(fsess, fargs);
				if (!f) {
					gf_fs_del(fsess);
					return gf_import_message(importer, GF_FILTER_NOT_FOUND, "[Importer] Cannot load filter %s", fargs);
				}
				if (prev_filter) {
					gf_filter_set_source(f, prev_filter, NULL);
				}
				prev_filter = f;
				if (!filter_orig) filter_orig = f;
				if (!sep) break;
				sep[0] = '@';
				fargs = sep+2;
			}
			if (prev_filter) {
				if (importer->trackID) {
					if (gf_filter_get_id(prev_filter)) {
						gf_fs_del(fsess);
						return gf_import_message(importer, GF_FILTER_NOT_FOUND, "[Importer] last filter in chain cannot use filter ID (%s)", fargs);
					}
					gf_filter_assign_id(prev_filter, "1");
					source_id_set=GF_TRUE;
				}
				gf_filter_set_source(isobmff_mux, prev_filter, NULL);
			}
		}

		//source args
		e = gf_dynstrcat(&args, "importer:index_dur=0", ":");
		if (importer->trackID && !source_id_set) e |= gf_dynstrcat(&args, "FID=1", ":");
		if (importer->filter_src_opts) e |= gf_dynstrcat(&args, importer->filter_src_opts, ":");

		if (importer->flags & GF_IMPORT_SBR_IMPLICIT) e |= gf_dynstrcat(&args, "sbr=imp", ":");
		else if (importer->flags & GF_IMPORT_SBR_EXPLICIT) e |= gf_dynstrcat(&args, "sbr=exp", ":");
		if (importer->flags & GF_IMPORT_PS_IMPLICIT) e |= gf_dynstrcat(&args, "ps=imp", ":");
		else if (importer->flags & GF_IMPORT_PS_EXPLICIT) e |= gf_dynstrcat(&args, "ps=exp", ":");
		if (importer->flags & GF_IMPORT_OVSBR) e |= gf_dynstrcat(&args, "ovsbr", ":");
		//avoids message at end of import
		if (importer->flags & GF_IMPORT_FORCE_PACKED) e |= gf_dynstrcat(&args, "nal_length=0", ":");
		if (importer->flags & GF_IMPORT_SET_SUBSAMPLES) e |= gf_dynstrcat(&args, "subsamples", ":");
		if (importer->flags & GF_IMPORT_NO_SEI) e |= gf_dynstrcat(&args, "nosei", ":");
		if (importer->flags & GF_IMPORT_SVC_NONE) e |= gf_dynstrcat(&args, "nosvc", ":");
		if (importer->flags & GF_IMPORT_SAMPLE_DEPS) e |= gf_dynstrcat(&args, "deps", ":");
		if (importer->flags & GF_IMPORT_FORCE_MPEG4) e |= gf_dynstrcat(&args, "mpeg4", ":");
		if (importer->keep_audelim) e |= gf_dynstrcat(&args, "audelim", ":");
		if (importer->video_fps.num && importer->video_fps.den) {
			sprintf(szSubArg, "fps=%d/%d", importer->video_fps.num, importer->video_fps.den);
			e |= gf_dynstrcat(&args, szSubArg, ":");
		}
		if (importer->is_alpha) e |= gf_dynstrcat(&args, "#Alpha", ":");

		if (importer->streamFormat && !strcmp(importer->streamFormat, "VTT")) e |= gf_dynstrcat(&args, "webvtt", ":");

		if (e) {
			gf_free(args);
			return gf_import_message(importer, e, "[Importer] Cannot load arguments for input %s", importer->in_name);
		}

		source = gf_fs_load_source(fsess, importer->in_name, args, NULL, &e);
		gf_free(args);
		args = NULL;

		if (e) {
			gf_fs_del(fsess);
			return gf_import_message(importer, e, "[Importer] Cannot load filter for input file \"%s\"", importer->in_name);
		}

		if (filter_orig)
			gf_filter_set_source(filter_orig, source, NULL);

		gf_fs_run(fsess);
		if (!importer->last_error) importer->last_error = gf_fs_get_last_connect_error(fsess);
		if (!importer->last_error) importer->last_error = gf_fs_get_last_process_error(fsess);

		if (importer->last_error) {
			gf_fs_del(fsess);
			return gf_import_message(importer, importer->last_error, "[Importer] Error probing %s", importer->in_name);
		}

		importer->final_trackID = gf_isom_get_last_created_track_id(importer->dest);

		if (importer->esd && importer->final_trackID) {
			u32 tk_num = gf_isom_get_track_by_id(importer->dest, importer->final_trackID);
			GF_ESD *esd = gf_isom_get_esd(importer->dest, tk_num, 1);
			if (esd && !importer->esd->decoderConfig) {
				importer->esd->decoderConfig = esd->decoderConfig;
				esd->decoderConfig = NULL;
			}
			if (esd && !importer->esd->slConfig) {
				importer->esd->slConfig = esd->slConfig;
				esd->slConfig = NULL;
			}
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
		}
	}
	if (importer->print_stats_graph & 1) gf_fs_print_stats(fsess);
	if (importer->print_stats_graph & 2) gf_fs_print_connections(fsess);
	gf_fs_del(fsess);
	return GF_OK;
}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


