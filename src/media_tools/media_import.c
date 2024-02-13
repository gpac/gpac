/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Romain Bouqueau, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2024
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
#include <gpac/network.h>


#ifndef GPAC_DISABLE_ISOM

static void gf_media_update_bitrate_ex(GF_ISOFile *file, u32 track, Bool use_esd)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 i, count, timescale, db_size, cdur, csize;
	u64 time_wnd, max_rate, avg_rate, bitrate;
	Double br;
	GF_ISOSample sample;

	db_size = 0;
	max_rate = avg_rate = time_wnd = bitrate = 0;

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
		bitrate = avg_rate;
	} else {
		u32 rate = 0;
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
	if (br>0) {
		GF_ESD *esd = NULL;
		if (!csize || !cdur) {
			bitrate = (u32) ((Double) (s64)avg_rate / br);
			bitrate *= 8;
			max_rate *= 8;
		}
		if (!max_rate) max_rate = bitrate;

		if (use_esd) esd = gf_isom_get_esd(file, track, 1);
		if (esd && esd->decoderConfig) {
			esd->decoderConfig->avgBitrate = (u32) bitrate;
			esd->decoderConfig->maxBitrate = (u32) max_rate;
			esd->decoderConfig->bufferSizeDB = db_size;
			gf_isom_change_mpeg4_description(file, track, 1, esd);
		} else {
			/*move to bps*/
			gf_isom_update_bitrate(file, track, 1, (u32) bitrate, (u32) max_rate, db_size);
		}
		if (esd) gf_odf_desc_del((GF_Descriptor *)esd);
	}
#endif
}

GF_EXPORT
void gf_media_update_bitrate(GF_ISOFile *file, u32 track)
{
	gf_media_update_bitrate_ex(file, track, GF_FALSE);
}
#endif


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


GF_EXPORT
u32 gf_dolby_vision_level(u32 width, u32 height, u64 fps_num, u64 fps_den, u32 codecid)
{
	u32 dv_level = 0;
	u32 fps = fps_den ? (u32) (fps_num/fps_den) : 25;
	u64 level_check = width;
	level_check *= height * fps;

	if (codecid==GF_CODECID_AVC) {
		if (level_check <= 1280*720*24) dv_level = 1;
		else if (level_check <= 1280*720*30) dv_level = 2;
		else if (level_check <= 1920*1080*24) dv_level = 3;
		else if (level_check <= 1920*1080*30) dv_level = 4;
		else if (level_check <= 1920*1080*60) dv_level = 5;
		else if (level_check <= 3840*2160*24) dv_level = 6;
		else if (level_check <= 3840*2160*30) dv_level = 7;
		else if (level_check <= 3840*2160*48) dv_level = 8;
		else if (level_check <= 3840*2160*60) dv_level = 9;
	} else {
		if (level_check <= 1280*720*24) dv_level = 1;
		else if (level_check<= 1280*720*30) dv_level = 2;
		else if (level_check <= 1920*1080*24) dv_level = 3;
		else if (level_check <= 1920*1080*30) dv_level = 4;
		else if (level_check <= 1920*1080*60) dv_level = 5;
		else if (level_check <= 3840*2160*24) dv_level = 6;
		else if (level_check <= 3840*2160*30) dv_level = 7;
		else if (level_check <= 3840*2160*48) dv_level = 8;
		else if (level_check <= 3840*2160*60) dv_level = 9;
        else if (level_check <= 3840*2160*120) {
            if (width == 7680) dv_level = 11;
            else dv_level = 10;
        }
        else if (level_check <= 7680*4320*60) dv_level = 12;
        else dv_level = 13;
	}
	return dv_level;
}

#ifndef GPAC_DISABLE_MEDIA_IMPORT

GF_Err gf_import_message(GF_MediaImporter *import, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_APP, e ? GF_LOG_WARNING : GF_LOG_INFO)) {
		va_list args;
		char szMsg[1024];
		va_start(args, format);
		vsnprintf(szMsg, 1024, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_WARNING : GF_LOG_INFO), GF_LOG_APP, ("%s\n", szMsg) );
	}
#endif
	return e;
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

	if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	if (import->esd->decoderConfig->decoderSpecificInfo->data) gf_free(import->esd->decoderConfig->decoderSpecificInfo->data);
	import->esd->decoderConfig->decoderSpecificInfo->data = dsi;
	import->esd->decoderConfig->decoderSpecificInfo->dataLength = dsi_len;


	track = 0;
	if (mult_desc_allowed)
		track = gf_isom_get_track_by_id(import->dest, import->esd->ESID);
	if (!track)
		track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_SCENE, 1000);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, GF_TRUE);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;

	if (import->source_magic)
		gf_isom_set_track_magic(import->dest, track, import->source_magic);

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
	gf_isom_update_duration(import->dest);

exit:
	gf_free(data);
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	return e;
}

static GF_Err gf_import_isomedia_track(GF_MediaImporter *import)
{
	GF_Err e;
	u64 offset, sampDTS, duration, dts_offset;
	Bool is_nalu_video=GF_FALSE;
	u32 track, di, trackID, track_in, i, num_samples, mtype, w, h, sr, sbr_sr, ch, mstype, cur_extract_mode, cdur, bps;
	u64 mtimescale;
	s32 trans_x, trans_y;
	s16 layer;
	char *lang;
	u8 *sai_buffer = NULL;
	u32 sai_buffer_size = 0, sai_buffer_alloc = 0;
	const char *orig_name = gf_url_get_resource_name(gf_isom_get_filename(import->orig));
	Bool sbr, ps;
	GF_ISOSample *samp;
	GF_ESD *origin_esd;
	GF_InitialObjectDescriptor *iod;
	Bool is_cenc;
	GF_ISOTrackCloneFlags clone_flags;
	sampDTS = 0;
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		for (i=0; i<gf_isom_get_track_count(import->orig); i++) {
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
			case GF_ISOM_MEDIA_SUBT:
			case GF_ISOM_MEDIA_MPEG_SUBT:
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
			gf_media_get_rfc_6381_codec_name(import->orig, i+1, 1, import->tk_info[i].szCodecProfile, GF_FALSE, GF_FALSE);

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
	w = h = 0;
	cur_extract_mode = gf_isom_get_nalu_extract_mode(import->orig, track_in);
	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(import->orig);
	if (iod && (iod->tag != GF_ODF_IOD_TAG)) {
		gf_odf_desc_del((GF_Descriptor *) iod);
		iod = NULL;
	}
	mtype = gf_isom_get_media_type(import->orig, track_in);
	if (mtype==GF_ISOM_MEDIA_VISUAL) {
		u8 PL = iod ? iod->visual_profileAndLevel : 0xFE;
		gf_isom_get_visual_info(import->orig, track_in, 1, &w, &h);
#ifndef GPAC_DISABLE_AV_PARSERS
		/*for MPEG-4 visual, always check size (don't trust input file)*/
		if (origin_esd
			&& origin_esd->decoderConfig
			&& (origin_esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2)
		) {
			if (origin_esd->decoderConfig->decoderSpecificInfo) {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(origin_esd->decoderConfig->decoderSpecificInfo->data, origin_esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				w = dsi.width;
				h = dsi.height;
				PL = dsi.VideoPL;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Missing DecoderSpecificInfo in MPEG-4 Visual (Part2) stream\n"));
			}
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
		if (origin_esd && origin_esd->decoderConfig && (origin_esd->decoderConfig->objectTypeIndication==GF_CODECID_AAC_MPEG4)) {
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
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Missing DecoderSpecificInfo in MPEG-4 AAC stream\n"));
			}
		}
#endif
		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_AUDIO, PL);
	}
	else if (mtype==GF_ISOM_MEDIA_SUBPIC) {
		w = h = 0;
		trans_x = trans_y = 0;
		layer = 0;
		if (origin_esd && origin_esd->decoderConfig && (origin_esd->decoderConfig->objectTypeIndication == GF_CODECID_SUBPIC)) {
			gf_isom_get_track_layout_info(import->orig, track_in, &w, &h, &trans_x, &trans_y, &layer);
		}
	}

	gf_odf_desc_del((GF_Descriptor *) iod);
	if ( ! gf_isom_get_track_count(import->dest)) {
		u32 timescale;
		if (import->moov_timescale<0)
			timescale = gf_isom_get_media_timescale(import->orig, track_in);
		else
			timescale = gf_isom_get_timescale(import->orig);
		gf_isom_set_timescale(import->dest, timescale);
	}
	clone_flags = GF_ISOM_CLONE_TRACK_NO_QT;
	if (import->asemode == GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF) {
		clone_flags = 0;
	} else {
		const char *dst = gf_isom_get_filename(import->dest);
		if (dst && strstr(dst, ".mov"))
			clone_flags = 0;
	}
	clone_flags |= GF_ISOM_CLONE_RESET_DURATION;
	
	if (import->flags & GF_IMPORT_USE_DATAREF)
		clone_flags |= GF_ISOM_CLONE_TRACK_KEEP_DREF;
	if (import->target_trackID && (import->target_trackID==(u32)-1))
		clone_flags |= GF_ISOM_CLONE_TRACK_DROP_ID;
		
	e = gf_isom_clone_track(import->orig, track_in, import->dest, clone_flags, &track);
	if (e) goto exit;

	if (import->target_trackID && (import->target_trackID!=(u32)-1)) {
		u32 new_tk_id = import->target_trackID;
		gf_isom_set_track_id(import->dest, track, new_tk_id);
	}

	if ((gf_isom_get_track_count(import->dest)==1) && gf_isom_has_keep_utc_times(import->dest)) {
		u64 cdate, mdate;
		gf_isom_get_creation_time(import->orig, &cdate, &mdate);
		gf_isom_set_creation_time(import->dest, cdate, mdate);
	}

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

	if (import->source_magic)
		gf_isom_set_track_magic(import->dest, track, import->source_magic);

	mstype = gf_isom_get_media_subtype(import->orig, track_in, di);
	switch (mtype) {
	case GF_ISOM_MEDIA_VISUAL:
		gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - Video (size %d x %d)", orig_name, trackID, w, h);
		break;
    case GF_ISOM_MEDIA_AUXV:
        gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - Auxiliary Video (size %d x %d)", orig_name, trackID, w, h);
        break;
    case GF_ISOM_MEDIA_PICT:
        gf_import_message(import, GF_OK, "IsoMedia import %s - track ID %d - Picture sequence (size %d x %d)", orig_name, trackID, w, h);
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
		char szT[GF_4CC_MSIZE];
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

	is_cenc = gf_isom_is_cenc_media(import->orig, track_in, 0);
	if (gf_isom_is_media_encrypted(import->orig, track_in, 0)) {
		gf_isom_get_original_format_type(import->orig, track_in, 0, &mstype);
	}

	cdur = gf_isom_get_constant_sample_duration(import->orig, track_in);
	gf_isom_enable_raw_pack(import->orig, track_in, 2048);

	mtimescale = gf_isom_get_media_timescale(import->orig, track_in);
	duration = 0;
	if ((import->duration.num>0) && import->duration.den) {
		duration = (u64) (((Double)import->duration.num * mtimescale) / import->duration.den);
	}
	gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT);

	if (import->xps_inband) {
		if (is_cenc ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOM import] CENC media detected - cannot switch parameter set storage mode\n"));
		} else if (import->flags & GF_IMPORT_USE_DATAREF) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOM import] Cannot switch parameter set storage mode when using data reference\n"));
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
			case GF_ISOM_SUBTYPE_VVC1:
				gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
				//gf_isom_vvc_set_inband_config(import->dest, track, 1, (import->xps_inband==2) ? GF_TRUE : GF_FALSE);
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
	case GF_ISOM_SUBTYPE_VVC1:
	case GF_ISOM_SUBTYPE_VVI1:
		is_nalu_video = GF_TRUE;
		break;
	}
	num_samples = gf_isom_get_sample_count(import->orig, track_in);

	if (is_cenc) {
		u32 container_type;
		e = gf_isom_cenc_get_sample_aux_info(import->orig, track_in, 0, 1, &container_type, NULL, NULL);
		if (e)
			goto exit;
		if (container_type==GF_ISOM_BOX_UUID_PSEC) {
			e = gf_isom_piff_allocate_storage(import->dest, track, 0, 0, NULL);
		} else {
			e = gf_isom_cenc_allocate_storage(import->dest, track);
		}
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
			if (duration && !gf_sys_old_arch_compat() && gf_timestamp_greater_or_equal(samp->DTS, mtimescale, import->duration.num, import->duration.den)) {
				gf_isom_sample_del(&samp);
				break;
			}
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
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOM import] 0-duration sample detected at DTS %u - adjusting\n", samp->DTS));
				}
				samp->DTS = sampDTS + 1;
			}

			if (duration && !gf_sys_old_arch_compat() && gf_timestamp_greater_or_equal(samp->DTS, mtimescale, import->duration.num, import->duration.den)) {
				gf_isom_sample_del(&samp);
				break;
			}

			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		sampDTS = samp->DTS;
		if (samp->nb_pack)
			i+= samp->nb_pack-1;
		gf_isom_sample_del(&samp);

		//this will also copy all sample to group mapping, including seig for CENC
		gf_isom_copy_sample_info(import->dest, track, import->orig, track_in, i+1);

		if (e)
			goto exit;
		if (is_cenc) {
			u32 container_type;
			Bool Is_Encrypted;
			Bool is_mkey=GF_FALSE;
			u32 crypt_byte_block, skip_byte_block;
			const u8 *key_info=NULL;
			u32 key_info_len = 0;

			e = gf_isom_get_sample_cenc_info(import->orig, track_in, i+1, &Is_Encrypted, &crypt_byte_block, &skip_byte_block, &key_info, &key_info_len);
			if (e) goto exit;
			if (key_info) {
				is_mkey = key_info[0];
			}

			if (Is_Encrypted) {
				sai_buffer_size = sai_buffer_alloc;
				e = gf_isom_cenc_get_sample_aux_info(import->orig, track_in, i+1, di, &container_type, &sai_buffer, &sai_buffer_size);
				if (e) goto exit;
				if (sai_buffer_size > sai_buffer_alloc)
					sai_buffer_alloc = sai_buffer_size;

				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, sai_buffer, sai_buffer_size, is_nalu_video, GF_FALSE, is_mkey);

			} else {
				//we don't set container type since we don't add data to the container (senc/...)
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, 0, NULL, 0, is_nalu_video, GF_FALSE, is_mkey);
			}
			if (e)
				goto exit;
		}

		gf_set_progress("Importing ISO File", i+1, num_samples);

		if (duration && gf_sys_old_arch_compat() && (sampDTS > duration))
			break;
	}

	//adjust last sample duration
	if (i==num_samples) {
		u32 dur = gf_isom_get_sample_duration(import->orig, track_in, num_samples);
		gf_isom_set_last_sample_duration(import->dest, track, dur);
	} else {
		s64 mediaOffset;
		if (gf_isom_get_edit_list_type(import->orig, track_in, &mediaOffset)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISOBMF Import] Multiple edits found in source media, import may be broken\n"));
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
		if (import->is_alpha) {
			e = gf_isom_set_image_sequence_alpha(import->dest, track, di, GF_FALSE);
			if (e) goto exit;
		}
	}
	gf_isom_update_duration(import->dest);

exit:
	if (sai_buffer) gf_free(sai_buffer);
	if (origin_esd) gf_odf_desc_del((GF_Descriptor *) origin_esd);
	gf_isom_set_nalu_extract_mode(import->orig, track_in, cur_extract_mode);
	return e;
}

static GF_Err gf_import_isomedia(GF_MediaImporter *import)
{
	u32 nb_tracks, i;
	if (import->trackID)
		return gf_import_isomedia_track(import);

	if (!import->orig) return GF_BAD_PARAM;

	nb_tracks = gf_isom_get_track_count(import->orig);
	for (i=0; i<nb_tracks; i++) {
		import->trackID = gf_isom_get_track_id(import->orig, i+1);
		if (import->trackID) {
			GF_Err e = gf_import_isomedia_track(import);
			import->trackID = 0;
			if (e) return e;
		}
	}
	return GF_OK;
}

#endif

#ifndef GPAC_DISABLE_ISOM_WRITE

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
#endif

#ifndef GPAC_DISABLE_MEDIA_IMPORT

GF_EXPORT
GF_Err gf_media_import_chapters_file(GF_MediaImporter *import)
{
	s32 read=0;
	GF_Err e;
	u32 state, offset;
	u32 cur_chap;
	u64 ts;
	Bool found_chap = GF_FALSE;
	u32 i, h, m, s, ms, fr, fps;
	char line[1024];
	char szTitle[1024];
	FILE *f = gf_fopen(import->in_name, "rt");
	if (!f) return GF_URL_ERROR;

	read = (s32) gf_fread(line, 4, f);
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
		while (!is_chap_or_sub && (gf_fgets(line, 1024, f) != NULL)) {
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
			u32 timescale, inc;
            u32 mtype = gf_isom_get_media_type(import->dest, i+1);
			if (!gf_isom_is_video_handler_type(mtype)) continue;
			if (gf_isom_get_sample_count(import->dest, i+1) < 20) continue;
			samp = gf_isom_get_sample_info(import->dest, 1, 2, NULL, NULL);
			inc = (u32) samp->DTS;
			if (!inc) inc=1;
			timescale = gf_isom_get_media_timescale(import->dest, i+1);
			import->video_fps.num = timescale;
			import->video_fps.den = inc;
			gf_isom_sample_del(&samp);
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[Chapter import] Guessed video frame rate %u/%u\n", timescale, inc));
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
	while (gf_fgets(line, 1024, f) != NULL) {
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
			sscanf(sL, "AddChapter(%u,%1023s)", &nb_fr, szTitle);
			ts = gf_timestamp_rescale(nb_fr, 1000 * import->video_fps.den, import->video_fps.num);
			sL = strchr(sL, ',');
			strcpy(szTitle, sL+1);
			sL = strrchr(szTitle, ')');
			if (sL) sL[0] = 0;
		} else if (!strnicmp(sL, "AddChapterBySecond(", 19)) {
			u32 nb_s;
			sscanf(sL, "AddChapterBySecond(%u,%1023s)", &nb_s, szTitle);
			ts = nb_s;
			ts *= 1000;
			sL = strchr(sL, ',');
			strcpy(szTitle, sL+1);
			sL = strrchr(szTitle, ')');
			if (sL) sL[0] = 0;
		} else if (!strnicmp(sL, "AddChapterByTime(", 17)) {
			sscanf(sL, "AddChapterByTime(%u,%u,%u,%1023s)", &h, &m, &s, szTitle);
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
				char szTS[1025], *tok;
				strncpy(szTS, sL, 1024);
				szTS[1024]=0;
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
			char szTemp[1025], *str;
			strncpy(szTemp, sL, 1024);
			szTemp[1024] = 0;
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
				found_chap = GF_TRUE;
				e = gf_isom_add_chapter(import->dest, 0, ts, szTitle);
				if (e) goto err_exit;
				state = 0;
			}
			continue;
		}
		else continue;

		found_chap = GF_TRUE;
		if (strlen(szTitle)) {
			e = gf_isom_add_chapter(import->dest, 0, ts, szTitle);
		} else {
			e = gf_isom_add_chapter(import->dest, 0, ts, NULL);
		}
		if (e) goto err_exit;
	}

err_exit:
	gf_fclose(f);
	if (!found_chap) return GF_NOT_FOUND;
	return e;
}

GF_EXPORT
GF_Err gf_media_import_chapters(GF_ISOFile *file, char *chap_file, GF_Fraction import_fps, Bool use_qt)
{
	GF_Err e;
	u32 i;
	GF_MediaImporter import;
	//remove all chapter info
	gf_isom_remove_chapter(file, 0, 0);

restart_check:
	//remove all chapter tracks
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_reference_count(file, i+1, GF_ISOM_REF_CHAP)) {
			u32 chap_track=0;
			gf_isom_get_reference(file, i+1, GF_ISOM_REF_CHAP, 1, &chap_track);
			if (chap_track) {
				gf_isom_remove_track(file, chap_track);
				goto restart_check;
			}
		}
	}

	memset(&import, 0, sizeof(GF_MediaImporter));
	import.dest = file;
	import.in_name = chap_file;
	import.video_fps = import_fps;
	import.streamFormat = "CHAP";
	e = gf_media_import(&import);
	if (e) return e;

	if (!import.final_trackID) return GF_OK;
	if (use_qt) {
		u32 chap_track = gf_isom_get_track_by_id(file, import.final_trackID);
		u32 nb_sdesc = gf_isom_get_sample_description_count(file, chap_track);
		for (i=0; i<nb_sdesc; i++) {
			gf_isom_set_media_subtype(file, chap_track, i+1, GF_ISOM_SUBTYPE_TEXT);
		}
	}
	//imported chapter is a track, set reference
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		u32 mtype = gf_isom_get_media_type(file, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_AUDIO:
			gf_isom_set_track_reference(file, i+1, GF_ISOM_REF_CHAP, import.final_trackID);
			break;
		}
	}
	return GF_OK;
}

static Bool on_import_setup_failure(GF_Filter *f, void *on_setup_error_udta, GF_Err e)
{
	GF_MediaImporter *importer = (GF_MediaImporter *)on_setup_error_udta;
	if (importer)
		importer->last_error = e;
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_media_import(GF_MediaImporter *importer)
{
	GF_Err e;
	u32 i, count;
	GF_FilterSession *fsess=NULL;
	char *args = NULL;
	char szSubArg[1024];
	char szFilterID[20];
	Bool source_id_set = GF_FALSE;
	Bool source_is_isom = GF_FALSE;
	GF_Filter *isobmff_mux, *source;
	GF_Filter *filter_orig;
	char *ext;
	char *fmt = NULL;

	if (!importer || (!importer->dest && (importer->flags!=GF_IMPORT_PROBE_ONLY)) || (!importer->in_name && !importer->orig) ) return GF_BAD_PARAM;

	if (importer->orig) return gf_import_isomedia(importer);

	if (importer->force_ext) {
		ext = importer->force_ext;
	} else {
		ext = gf_file_ext_start(importer->in_name);
		if (!ext) ext = "";
	}

	if (importer->streamFormat) fmt = importer->streamFormat;

	/*if isobmf and probing or no filter, direct copy*/
	if (gf_isom_probe_file(importer->in_name)) {
		u64 magic = 1;
		magic <<= 32;
		magic |= (importer->source_magic & 0xFFFFFFFFUL);
		importer->source_magic = magic;
		source_is_isom = GF_TRUE;
		if ((!importer->filter_chain && !importer->filter_dst_opts && !importer->run_in_session && !importer->start_time)
			|| (importer->flags & GF_IMPORT_PROBE_ONLY)
		) {
			importer->orig = gf_isom_open(importer->in_name, GF_ISOM_OPEN_READ, NULL);
			if (importer->orig) {
				e = gf_import_isomedia(importer);
				gf_isom_delete(importer->orig);
				importer->orig = NULL;
				return e;
			}
		}
	}

	/*SC3DMC*/
	if (!strnicmp(ext, ".s3d", 4) || (fmt && !stricmp(fmt, "SC3DMC")) )
		return gf_import_afx_sc3dmc(importer, GF_TRUE);
	/* chapter */
	else if (!strnicmp(ext, ".txt", 4) || !strnicmp(ext, ".chap", 5) || (fmt && !stricmp(fmt, "CHAP")) ) {
		e =  gf_media_import_chapters_file(importer);
		if (e==GF_NOT_FOUND) {
			fmt = NULL;
		} else {
			return e;
		}
	}

	e = GF_OK;
	importer->last_error = GF_OK;

	if (importer->flags & GF_IMPORT_PROBE_ONLY) {
		GF_Filter *prober, *src_filter;

		fsess = gf_fs_new_defaults(0);
		if (!fsess) {
			return gf_import_message(importer, GF_BAD_PARAM, "[Importer] Cannot load filter session for import");
		}
		prober = gf_fs_load_filter(fsess, "probe:log=null", &e);
		src_filter = gf_fs_load_source(fsess, importer->in_name, "index=0", NULL, &e);
		if (e) {
			gf_fs_run(fsess);
			gf_fs_del(fsess);
			return gf_import_message(importer, e, "[Importer] Cannot load filter for input file \"%s\"", importer->in_name);
		}
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
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
				Double d = (Double) p->value.lfrac.num;
				if (d<0) d = -d;
				d*=1000;
				if (p->value.lfrac.den) d /= p->value.lfrac.den;
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
/*			p = gf_filter_pid_get_property(pid, GF_PROP_PID_CAN_DATAREF);
			if (p) importer->flags |= GF_IMPORT_USE_DATAREF;
*/
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_SUBTYPE);
			if (p) tki->media_subtype = p->value.uint;

			importer->nb_tracks++;
		}
		gf_fs_del(fsess);
		return GF_OK;
	}

	if (importer->run_in_session) {
		fsess = importer->run_in_session;
	} else {
		fsess = gf_fs_new_defaults(fmt ? GF_FS_FLAG_NO_PROBE : 0);
		if (!fsess) {
			return gf_import_message(importer, GF_BAD_PARAM, "[Importer] Cannot load filter session for import");
		}
	}

	filter_orig=NULL;

	if (importer->run_in_session) {
		sprintf(szFilterID, "%u", (u32) ( (importer->source_magic & 0xFFFFFFFFUL) ) );
	} else {
		strcpy(szFilterID, "1");
	}

	if (!importer->run_in_session) {
		//mux args
		e = gf_dynstrcat(&args, "mp4mx:importer", ":");
		sprintf(szSubArg, "file=%p", importer->dest);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}

	if (importer->trackID) {
		sprintf(szSubArg, "SID=%s#PID=%d", szFilterID, importer->trackID);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->filter_dst_opts)
		e |= gf_dynstrcat(&args, importer->filter_dst_opts, ":gfloc:");

	if (importer->flags & GF_IMPORT_FORCE_MPEG4)
		e |= gf_dynstrcat(&args, "m4sys", ":");
	if (importer->flags & GF_IMPORT_USE_DATAREF)
		e |= gf_dynstrcat(&args, "dref", ":");
	if (importer->flags & GF_IMPORT_NO_EDIT_LIST)
		e |= gf_dynstrcat(&args, "edits=no", ":");
	if (importer->flags & GF_IMPORT_FORCE_PACKED)
		e |= gf_dynstrcat(&args, "pack_nal", ":");
	if (importer->xps_inband==1)
		e |= gf_dynstrcat(&args, "xps_inband=all", ":");
	else if (importer->xps_inband==2)
		e |= gf_dynstrcat(&args, "xps_inband=both", ":");
	if (importer->esd && importer->esd->ESID) {
		sprintf(szSubArg, "trackid=%d", importer->esd->ESID);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	else if (importer->target_trackID) {
		sprintf(szSubArg, "trackid=%u", importer->target_trackID);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->flags & GF_IMPORT_FORCE_SYNC)
		e |= gf_dynstrcat(&args, ":forcesync", NULL);

	if (importer->duration.den) {
		sprintf(szSubArg, "dur=%d/%d", importer->duration.num, importer->duration.den);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->frames_per_sample) {
		sprintf(szSubArg, "pack3gp=%d", importer->frames_per_sample);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->moov_timescale) {
		sprintf(szSubArg, "moovts=%d", importer->moov_timescale);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2) { e |= gf_dynstrcat(&args, "ase=v0s", ":"); }
	else if (importer->asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG) { e |= gf_dynstrcat(&args, "ase=v1", ":"); }
	else if (importer->asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF) { e |= gf_dynstrcat(&args, "ase=v1qt", ":"); }

	if (source_is_isom) {
		if (!gf_sys_find_global_arg("xps_inband")
			&& (!importer->filter_dst_opts || !strstr(importer->filter_dst_opts, "xps_inband"))
		) {
			e |= gf_dynstrcat(&args, "xps_inband=auto", ":");
		}
		if (gf_isom_has_keep_utc_times(importer->dest) ) { e |= gf_dynstrcat(&args, "keep_utc", ":"); }
	}

	if (importer->start_time) {
		sprintf(szSubArg, "start=%f", importer->start_time);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (e) {
		gf_fs_del(fsess);
		gf_free(args);
		return gf_import_message(importer, e, "[Importer] Cannot load ISOBMFF muxer arguments");
	}

	if (!importer->run_in_session) {
		isobmff_mux = gf_fs_load_filter(fsess, args, &e);
		gf_free(args);
		args = NULL;

		if (!isobmff_mux) {
			gf_fs_del(fsess);
			gf_free(args);
			return gf_import_message(importer, e, "[Importer] Cannot load ISOBMFF muxer");
		}
	} else {
		if (args)
			gf_dynstrcat(&importer->update_mux_args, args, ":");
		args = NULL;
		isobmff_mux = NULL;
	}

	//filter chain
	if (importer->filter_chain) {
		GF_Filter *prev_filter=NULL;
		char *fargs = (char *) importer->filter_chain;

		while (fargs) {
			GF_Filter *f;
			char *sep;
			Bool end_of_sub_chain = GF_FALSE;
			if (importer->is_chain_old_syntax) {
				sep = strstr(fargs, "@@");
			} else {
				sep = strstr(fargs, "@");
				if (sep && (sep[1] == '@'))
					end_of_sub_chain = GF_TRUE;
			}
			if (sep) sep[0] = 0;

			f = gf_fs_load_filter(fsess, fargs, &e);
			if (!f) {
				if (!importer->run_in_session)
					gf_fs_del(fsess);
				return gf_import_message(importer, e, "[Importer] Cannot load filter %s", fargs);
			}
			if (prev_filter) {
				gf_filter_set_source(f, prev_filter, NULL);
			}
			prev_filter = f;
			if (!filter_orig) filter_orig = f;

			if (!sep) end_of_sub_chain = GF_TRUE;
			//we cannot directly set the source id in case we run in an existing session
			if (end_of_sub_chain && prev_filter) {
				const char *prev_id;
				if (importer->trackID) {
					if (gf_filter_get_id(prev_filter)) {
						if (!importer->run_in_session)
							gf_fs_del(fsess);
						return gf_import_message(importer, GF_FILTER_NOT_FOUND, "[Importer] last filter in chain cannot use filter ID (%s)", fargs);
					}
					gf_filter_assign_id(prev_filter, szFilterID);
					source_id_set=GF_TRUE;
				}
				prev_id = gf_filter_get_id(prev_filter);
				if (!prev_id) {
					gf_filter_assign_id(prev_filter, NULL);
					prev_id = gf_filter_get_id(prev_filter);
				}
				if (importer->run_in_session) {
					gf_dynstrcat(&importer->update_mux_sid, prev_id, importer->update_mux_sid ? "," : ":SID=");
				} else {
					gf_assert(isobmff_mux);
					gf_filter_set_source(isobmff_mux, prev_filter, NULL);
				}
				prev_filter = NULL;
			}

			if (!sep) break;
			sep[0] = '@';

			if (importer->is_chain_old_syntax || end_of_sub_chain) {
				fargs = sep+2;
			} else {
				fargs = sep+1;
			}
		}
	}

	//source args
	e = gf_dynstrcat(&args, "importer:index=0", ":");
	if (importer->trackID && !source_id_set) {
		sprintf(szSubArg, "FID=%s", szFilterID);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (fmt) {
		sprintf(szSubArg, "ext=%s", fmt);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
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
	if (importer->flags & GF_IMPORT_KEEP_AV1_TEMPORAL_OBU) e |= gf_dynstrcat(&args, "temporal_delim", ":");
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

	//SRT-related legacy params
	if (importer->fontName) {
		e |= gf_dynstrcat(&args, "fontname=", ":");
		e |= gf_dynstrcat(&args, importer->fontName, NULL);
	}
	if (importer->fontName) {
		sprintf(szSubArg, "fontsize=%d", importer->fontSize);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->text_width && importer->text_height) {
		sprintf(szSubArg, "width=%d:height=%d:txtx=%d:txty=%d", importer->text_width, importer->text_height, importer->text_x, importer->text_y);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}

	if (importer->source_magic) {
		sprintf(szSubArg, "#SrcMagic="LLU, importer->source_magic);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (importer->track_index) {
		sprintf(szSubArg, "#MuxIndex=%d", importer->track_index);
		e |= gf_dynstrcat(&args, szSubArg, ":");
	}
	if (e) {
		if (!importer->run_in_session)
			gf_fs_del(fsess);
		gf_free(args);
		return gf_import_message(importer, e, "[Importer] Cannot load arguments for input %s", importer->in_name);
	}

	source = gf_fs_load_source(fsess, importer->in_name, args, NULL, &e);
	gf_free(args);
	args = NULL;

	if (e) {
		if (!importer->run_in_session)
			gf_fs_del(fsess);
		return gf_import_message(importer, e, "[Importer] Cannot load filter for input file \"%s\"", importer->in_name);
	}

	if (filter_orig)
		gf_filter_set_source(filter_orig, source, NULL);

	//source is setup, we run in an external session, nothing left to do
	if (importer->run_in_session)
		return GF_OK;

	gf_fs_run(fsess);

	if (!importer->last_error) importer->last_error = gf_fs_get_last_connect_error(fsess);
	if (!importer->last_error) importer->last_error = gf_fs_get_last_process_error(fsess);

	if (importer->last_error) {
		gf_fs_print_non_connected(fsess);
		if (importer->print_stats_graph & 1) gf_fs_print_stats(fsess);
		if (importer->print_stats_graph & 2) gf_fs_print_connections(fsess);
		if (!importer->run_in_session)
			gf_fs_del(fsess);
		return gf_import_message(importer, importer->last_error, "[Importer] Error importing %s", importer->in_name);
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
		if (esd) {
			gf_odf_desc_del((GF_Descriptor *) esd);
		}

		if (!importer->esd->ESID) {
			importer->esd->ESID = importer->final_trackID;
		}
	}

	if (!e) gf_fs_print_unused_args(fsess, "index,fps,mpeg4");
	gf_fs_print_non_connected(fsess);
	if (importer->print_stats_graph & 1) gf_fs_print_stats(fsess);
	if (importer->print_stats_graph & 2) gf_fs_print_connections(fsess);
	gf_fs_del(fsess);
	if (!importer->final_trackID) {
		return gf_import_message(importer, GF_NOT_SUPPORTED, "[Importer] No valid track to import in input file \"%s\"", importer->in_name);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/
