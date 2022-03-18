/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include <gpac/internal/isomedia_dev.h>
#include <gpac/mpegts.h>
#include <gpac/constants.h>
#include <gpac/filters.h>

#ifndef GPAC_DISABLE_MEDIA_EXPORT

#ifndef GPAC_DISABLE_AVILIB
#include <gpac/internal/avilib.h>
#endif

#ifndef GPAC_DISABLE_OGG
#include <gpac/internal/ogg.h>
#endif

#ifndef GPAC_DISABLE_VOBSUB
#include <gpac/internal/vobsub.h>
#endif

#ifndef GPAC_DISABLE_ZLIB
#include <zlib.h>
#endif

static GF_Err gf_export_message(GF_MediaExporter *dumper, GF_Err e, char *format, ...)
{
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return e;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_MEDIA, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		va_list args;
		char szMsg[1024];
		va_start(args, format);
		vsnprintf(szMsg, 1024, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_MEDIA, ("%s\n", szMsg) );
	}
#endif
	return e;
}

#ifndef GPAC_DISABLE_AV_PARSERS
static GF_Err gf_dump_to_vobsub(GF_MediaExporter *dumper, char *szName, u32 track, char *dsi, u32 dsiSize)
{
#ifndef GPAC_DISABLE_VOBSUB
	FILE *fidx, *fsub;
	u32 width, height, i, count, di;
	GF_ISOSample *samp;
	char *lang = NULL;

	if (!szName) {
		szName = gf_file_basename(gf_isom_get_filename(dumper->file));
		if (!szName) return GF_BAD_PARAM;
	}
	/* Check decoder specific information (palette) size - should be 64 */
	if (!dsi || (dsiSize != 64)) {
		return gf_export_message(dumper, GF_CORRUPTED_DATA, "Invalid decoder specific info size - must be 64 but is %d", dsiSize);
	}

	/* Create an idx file */
	if (!gf_file_ext_start(szName)) {
		char szPath[GF_MAX_PATH];
		strcpy(szPath, szName);
		strcat(szPath, ".idx");
		fidx = gf_fopen(szPath, "wb");
	 } else {
		fidx = gf_fopen(szName, "wb");
	}
	if (!fidx) {
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	/* Create a sub file */
	char *ext = gf_file_ext_start(szName);
	if (ext && (!stricmp(ext, ".idx") || !stricmp(ext, ".sub")) ) {
		ext[0] = 0;
	}
	szName = strcat(szName, ".sub");
	fsub = gf_fopen(szName, "wb");
	if (!fsub) {
		gf_fclose(fidx);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	/* Retrieve original subpicture resolution */
	gf_isom_get_track_layout_info(dumper->file, track, &width, &height, NULL, NULL, NULL);

	/* Write header */
	gf_fputs("# VobSub index file, v7 (do not modify this line!)\n#\n", fidx);

	/* Write original subpicture resolution */
	gf_fprintf(fidx, "size: %ux%u\n", width, height);

	/* Write palette */
	gf_fputs("palette:", fidx);
	for (i = 0; i < 16; i++) {
		s32 y, u, v, r, g, b;

		y = (s32)(u8)dsi[(i<<2)+1] - 0x10;
		u = (s32)(u8)dsi[(i<<2)+3] - 0x80;
		v = (s32)(u8)dsi[(i<<2)+2] - 0x80;
		r = (298 * y           + 409 * v + 128) >> 8;
		g = (298 * y - 100 * u - 208 * v + 128) >> 8;
		b = (298 * y + 516 * u           + 128) >> 8;

		if (i) gf_fputc(',', fidx);

#define CLIP(x) (((x) >= 0) ? (((x) < 256) ? (x) : 255) : 0)
		gf_fprintf(fidx, " %02x%02x%02x", CLIP(r), CLIP(g), CLIP(b));
#undef CLIP
	}
	gf_fputc('\n', fidx);

	/* Write some other useful values */
	gf_fputs("# ON: displays only forced subtitles, OFF: shows everything\n", fidx);
	gf_fputs("forced subs: OFF\n\n", fidx);

	/* Write current language index */
	gf_fputs("# Language index in use\nlangidx: 0\n", fidx);

	/* Write language header */
	gf_isom_get_media_language(dumper->file, track, &lang);
	gf_fprintf(fidx, "id: %s, index: 0\n", vobsub_lang_id(lang));
	gf_free(lang);

	/* Retrieve sample count */
	count = gf_isom_get_sample_count(dumper->file, track);

	/* Process samples (skip first - because it is special) */
	for (i = 2; i <= count; i++)
	{
		u64 dts;
		u32 hh, mm, ss, ms;

		samp = gf_isom_get_sample(dumper->file, track, i, &di);
		if (!samp) {
			break;
		}

		dts = samp->DTS / 90;
		ms  = (u32)(dts % 1000);
		dts = dts / 1000;
		ss  = (u32)(dts % 60);
		dts = dts / 60;
		mm  = (u32)(dts % 60);
		hh  = (u32)(dts / 60);
		gf_fprintf(fidx, "timestamp: %02u:%02u:%02u:%03u, filepos: %09"LLX_SUF"\n", hh, mm, ss, ms, gf_ftell(fsub));
		if (vobsub_packetize_subpicture(fsub, samp->DTS, samp->data, samp->dataLength) != GF_OK) {
			gf_isom_sample_del(&samp);
			gf_fclose(fsub);
			gf_fclose(fidx);
			return gf_export_message(dumper, GF_IO_ERR, "Unable packetize subpicture into file %s\n", szName);
		}

		gf_isom_sample_del(&samp);
		gf_set_progress("VobSub Export", i + 1, count);

		if (dumper->flags & GF_EXPORT_DO_ABORT) {
			break;
		}
	}

	/* Delete sample if any */
	if (samp) {
		gf_isom_sample_del(&samp);
	}

	gf_fclose(fsub);
	gf_fclose(fidx);

	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

#endif // GPAC_DISABLE_AV_PARSERS


#ifndef GPAC_DISABLE_ISOM_WRITE
static GF_Err gf_export_isom_copy_track(GF_MediaExporter *dumper, GF_ISOFile *infile, u32 inTrackNum, GF_ISOFile *outfile, Bool ResetDependencies, Bool AddToIOD)
{
	GF_ESD *esd;
	GF_InitialObjectDescriptor *iod;
	GF_ISOTrackID TrackID;
	u32 newTk, descIndex, i, ts, rate, pos, di, count, msubtype;
	u64 dur;
	GF_ISOSample *samp;

	if (!inTrackNum) {
		if (gf_isom_get_track_count(infile) != 1) return gf_export_message(dumper, GF_BAD_PARAM, "Please specify trackID to export");
		inTrackNum = 1;
	}
	//check the ID is available
	TrackID = gf_isom_get_track_id(infile, inTrackNum);
	newTk = gf_isom_get_track_by_id(outfile, TrackID);
	if (newTk) TrackID = 0;

	//get the ESD and remove dependencies
	esd = NULL;
	msubtype = gf_isom_get_media_subtype(infile, inTrackNum, 1);

	if (msubtype == GF_ISOM_SUBTYPE_MPEG4) {
		esd = gf_isom_get_esd(infile, inTrackNum, 1);
		if (esd && ResetDependencies) {
			esd->dependsOnESID = 0;
			esd->OCRESID = 0;
		}
	}

	newTk = gf_isom_new_track(outfile, TrackID, gf_isom_get_media_type(infile, inTrackNum), gf_isom_get_media_timescale(infile, inTrackNum));
	gf_isom_set_track_enabled(outfile, newTk, GF_TRUE);

	if (gf_isom_has_keep_utc_times(infile)) {
		u64 cdate, mdate;
		gf_isom_get_track_creation_time(infile, inTrackNum, &cdate, &mdate);
		gf_isom_set_track_creation_time(outfile, newTk, cdate, mdate);
	}

	if (esd) {
		gf_isom_new_mpeg4_description(outfile, newTk, esd, NULL, NULL, &descIndex);
	} else {
		gf_isom_clone_sample_description(outfile, newTk, infile, inTrackNum, 1, NULL, NULL, &descIndex);
	}

	if (esd && esd->decoderConfig) {
		if ((esd->decoderConfig->streamType == GF_STREAM_VISUAL) || (esd->decoderConfig->streamType == GF_STREAM_SCENE)) {
			u32 w, h;
			gf_isom_get_visual_info(infile, inTrackNum, 1, &w, &h);
#ifndef GPAC_DISABLE_AV_PARSERS
			/*this is because so many files have reserved values of 320x240 from v1 ... */
			if ((esd->decoderConfig->objectTypeIndication == GF_CODECID_MPEG4_PART2)
				&& esd->decoderConfig->decoderSpecificInfo
			) {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				w = dsi.width;
				h = dsi.height;
			}
#endif
			gf_isom_set_visual_info(outfile, newTk, 1, w, h);
		}
		else if ((esd->decoderConfig->streamType == GF_STREAM_TEXT) && (esd->decoderConfig->objectTypeIndication == GF_CODECID_SUBPIC)) {
			u32 w, h;
			s32 trans_x, trans_y;
			s16 layer;
			gf_isom_get_track_layout_info(infile, inTrackNum, &w, &h, &trans_x, &trans_y, &layer);
			gf_isom_set_track_layout_info(outfile, newTk, w << 16, h << 16, trans_x, trans_y, layer);
		}
		esd->decoderConfig->avgBitrate = 0;
		esd->decoderConfig->maxBitrate = 0;
	}

	pos = 0;
	rate = 0;
	ts = gf_isom_get_media_timescale(infile, inTrackNum);
	count = gf_isom_get_sample_count(infile, inTrackNum);
	for (i=0; i<count; i++) {
		samp = gf_isom_get_sample(infile, inTrackNum, i+1, &di);
		gf_isom_add_sample(outfile, newTk, descIndex, samp);
		if (esd && esd->decoderConfig) {
			rate += samp->dataLength;
			esd->decoderConfig->avgBitrate += samp->dataLength;
			if (esd->decoderConfig->bufferSizeDB<samp->dataLength) esd->decoderConfig->bufferSizeDB = samp->dataLength;
			if (samp->DTS - pos > ts) {
				if (esd->decoderConfig->maxBitrate<rate) esd->decoderConfig->maxBitrate = rate;
				rate = 0;
				pos = 0;
			}
		}
		gf_isom_sample_del(&samp);
		gf_set_progress("ISO File Export", i, count);
	}
	gf_set_progress("ISO File Export", count, count);

	if (msubtype == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
		esd = gf_isom_get_esd(infile, inTrackNum, 1);
	} else if ((msubtype == GF_ISOM_SUBTYPE_AVC_H264)
	           || (msubtype == GF_ISOM_SUBTYPE_AVC2_H264)
	           || (msubtype == GF_ISOM_SUBTYPE_AVC3_H264)
	           || (msubtype == GF_ISOM_SUBTYPE_AVC4_H264)
	          ) {
		return gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, 0x0F);
	}

	/*likely 3gp or any non-MPEG-4 isomedia file*/
	if (!esd)
		return gf_isom_remove_root_od(outfile);

	//broken stream description, do not check for profiles & levels and remove from root
	if (!esd->decoderConfig) {
		gf_odf_desc_del((GF_Descriptor *)esd);
		return gf_isom_remove_root_od(outfile);
	}

	dur = gf_isom_get_media_duration(outfile, newTk);
	if (!dur) dur = ts;
	esd->decoderConfig->maxBitrate *= 8;
	esd->decoderConfig->avgBitrate = (u32) (esd->decoderConfig->avgBitrate * 8 * ts / dur);
	gf_isom_change_mpeg4_description(outfile, newTk, 1, esd);


	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(infile);
	switch (esd->decoderConfig->streamType) {
	case GF_STREAM_SCENE:
		if (iod && (iod->tag==GF_ODF_IOD_TAG)) {
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_SCENE, iod->scene_profileAndLevel);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_GRAPHICS, iod->graphics_profileAndLevel);
		} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2) {
			gf_export_message(dumper, GF_OK, "Warning: Scene PLs not found in original MP4 - defaulting to No Profile Specified");
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_SCENE, 0xFE);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_GRAPHICS, 0xFE);
		}
		break;
	case GF_STREAM_VISUAL:
		if (iod && (iod->tag==GF_ODF_IOD_TAG)) {
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, iod->visual_profileAndLevel);
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		else if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2)
			&& esd->decoderConfig->decoderSpecificInfo
		) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, dsi.VideoPL);
		}
#endif
		else {
			gf_export_message(dumper, GF_OK, "Warning: Visual PLs not found in original MP4 - defaulting to No Profile Specified");
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, 0xFE);
		}
		break;
	case GF_STREAM_AUDIO:
		if (iod && (iod->tag==GF_ODF_IOD_TAG)) {
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, iod->audio_profileAndLevel);
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		else if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_AAC_MPEG4)
			&& esd->decoderConfig->decoderSpecificInfo
		) {
			GF_M4ADecSpecInfo cfg;
			gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &cfg);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, cfg.audioPL);
		}
#endif
		else {
			gf_export_message(dumper, GF_OK, "Warning: Audio PLs not found in original MP4 - defaulting to No Profile Specified");
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, 0xFE);
		}
	default:
		break;
	}
	if (iod) gf_odf_desc_del((GF_Descriptor *) iod);
	gf_odf_desc_del((GF_Descriptor *)esd);

	if (AddToIOD) gf_isom_add_track_to_root_od(outfile, newTk);

	return GF_OK;
}


GF_Err gf_media_export_isom(GF_MediaExporter *dumper)
{
	GF_ISOFile *outfile;
	GF_Err e;
	Bool add_to_iod, is_stdout;
	char szName[1000];
	u32 track;
	GF_ISOOpenMode mode;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	if (gf_isom_get_media_type(dumper->file, dumper->trackID)==GF_ISOM_MEDIA_OD) {
		return gf_export_message(dumper, GF_BAD_PARAM, "Cannot extract OD track, result is  meaningless");
	}

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		dumper->flags |= GF_EXPORT_MERGE;
		return GF_OK;
	}
	if (dumper->out_name && gf_file_ext_start(dumper->out_name)) {
		strcpy(szName, dumper->out_name);
	} else {
		char *ext = (char *) gf_isom_get_filename(dumper->file);
		if (ext) ext = gf_file_ext_start(ext);
		sprintf(szName, "%s%s", dumper->out_name, ext ? ext : ".mp4");
	}
	is_stdout = (dumper->out_name && !strcmp(dumper->out_name, "std")) ? 1 : 0;
	add_to_iod = 1;
	mode = GF_ISOM_WRITE_EDIT;
	if (!is_stdout && (dumper->flags & GF_EXPORT_MERGE)) {
		FILE *t = gf_fopen(szName, "rb");
		if (t) {
			add_to_iod = 0;
			mode = GF_ISOM_OPEN_EDIT;
			gf_fclose(t);
		}
	}
	outfile = gf_isom_open(is_stdout ? "std" : szName, mode, NULL);

	if (mode == GF_ISOM_WRITE_EDIT) {
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_GRAPHICS, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_SCENE, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_OD, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_MPEGJ, 0xFF);
	}
	if (gf_isom_has_keep_utc_times(dumper->file)) {
		u64 cdate, mdate;
		gf_isom_get_creation_time(dumper->file, &cdate, &mdate);
		gf_isom_set_creation_time(outfile, cdate, mdate);
	}

	e = gf_export_isom_copy_track(dumper, dumper->file, track, outfile, 1, add_to_iod);
	if (!add_to_iod) {
		u32 i;
		for (i=0; i<gf_isom_get_track_count(outfile); i++) {
			gf_isom_remove_track_from_root_od(outfile, i+1);
		}
	}

	if (gf_isom_has_keep_utc_times(dumper->file))
		gf_isom_keep_utc_times(outfile, GF_TRUE);

	if (e) gf_isom_delete(outfile);
	else gf_isom_close(outfile);

	return e;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* Required for base64 encoding of DecoderSpecificInfo */
#include <gpac/base_coding.h>

#ifndef GPAC_DISABLE_VTT

/* Required for timestamp generation */
#include <gpac/webvtt.h>

GF_Err gf_media_export_webvtt_metadata(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	char szName[1000], szMedia[1000];
	FILE *med, *vtt;
	u32 w, h;
	u32 track, i, di, count, pos;
	u32 mtype, mstype;
	Bool isText;
	char *mime = NULL;
	Bool useBase64 = GF_FALSE;
	u32 headerLength = 0;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	if (!track) return gf_export_message(dumper, GF_BAD_PARAM, "Invalid track ID %d", dumper->trackID);

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		return GF_OK;
	}
	esd = gf_isom_get_esd(dumper->file, track, 1);
	med = NULL;
	if (dumper->flags & GF_EXPORT_WEBVTT_META_EMBEDDED) {
	} else {
		sprintf(szMedia, "%s.media", dumper->out_name);
		med = gf_fopen(szMedia, "wb");
		if (!med) {
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szMedia);
		}
	}

	sprintf(szName, "%s.vtt", dumper->out_name);
	vtt = gf_fopen(szName, "wt");
	if (!vtt) {
		gf_fclose(med);
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	mtype = gf_isom_get_media_type(dumper->file, track);
	if (mtype==GF_ISOM_MEDIA_TEXT || mtype == GF_ISOM_MEDIA_MPEG_SUBT || mtype == GF_ISOM_MEDIA_SUBT) {
		isText = GF_TRUE;
	} else {
		isText = GF_FALSE;
	}
	mstype = gf_isom_get_media_subtype(dumper->file, track, 1);

	/*write header*/
	gf_fprintf(vtt, "WEBVTT Metadata track generated by GPAC MP4Box %s\n", gf_sys_is_test_mode() ? "" : gf_gpac_version());

	gf_fprintf(vtt, "kind:metadata\n");
	{
		char *lang;
		gf_isom_get_media_language(dumper->file, track, &lang);
		gf_fprintf(vtt, "language:%s\n", lang);
		gf_free(lang);
	}
	{
		const char *handler;
		gf_isom_get_handler_name(dumper->file, track, &handler);
		gf_fprintf(vtt, "label: %s\n", handler);
	}
	if (gf_isom_is_track_in_root_od(dumper->file, track)) gf_fprintf(vtt, "inRootOD: yes\n");
	gf_fprintf(vtt, "trackID: %d\n", dumper->trackID);
	if (med) {
		gf_fprintf(vtt, "baseMediaFile: %s\n", gf_file_basename(szMedia));
	}
	if (esd) {
		if (esd->decoderConfig) {
			/* TODO: export the MPEG-4 Stream type only if it is not a GPAC internal value */
			gf_fprintf(vtt, "MPEG-4-streamType: %d\n", esd->decoderConfig->streamType);
			/* TODO: export the MPEG-4 Object Type Indication only if it is not a GPAC internal value */
			gf_fprintf(vtt, "MPEG-4-objectTypeIndication: %d\n", esd->decoderConfig->objectTypeIndication);
		}
		if (gf_isom_is_video_handler_type(mtype) ) {
			gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
			gf_fprintf(vtt, "width:%d\n", w);
			gf_fprintf(vtt, "height:%d\n", h);
		}
		else if (mtype==GF_ISOM_MEDIA_AUDIO) {
			u32 sr, nb_ch, bps;
			gf_isom_get_audio_info(dumper->file, track, 1, &sr, &nb_ch, &bps);
			gf_fprintf(vtt, "sampleRate: %d\n", sr);
			gf_fprintf(vtt, "numChannels: %d\n", nb_ch);
		} else if (isText) {
			s32 tx, ty;
			s16 layer;
			gf_isom_get_track_layout_info(dumper->file, track, &w, &h, &tx, &ty, &layer);
			gf_fprintf(vtt, "width:%d\n", w);
			gf_fprintf(vtt, "height:%d\n", h);
			if (tx || ty) gf_fprintf(vtt, "translation:%d,%d\n", tx, ty);
			if (layer) gf_fprintf(vtt, "layer:%d\n", layer);
		}
		if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			if (isText) {
				if (mstype == GF_ISOM_SUBTYPE_WVTT) {
					/* Warning: Just use -raw export */
					mime = "text/vtt";
				} else if (mstype == GF_ISOM_SUBTYPE_STXT) {
					/* TODO: find the mime type from the ESD, assume SVG for now */
					mime = "image/svg+xml";
				} else if (mstype == GF_ISOM_SUBTYPE_STPP) {
					/* TODO: find the mime type from the ESD, assume TTML for now */
					mime = "application/ttml+xml";
				}
				if (dumper->flags & GF_EXPORT_WEBVTT_META_EMBEDDED) {
					if (mstype == GF_ISOM_SUBTYPE_STXT) {
						if (esd->decoderConfig->decoderSpecificInfo->dataLength) {
							gf_fprintf(vtt, "text-header: \n");
							gf_webvtt_dump_header_boxed(vtt, esd->decoderConfig->decoderSpecificInfo->data+4, esd->decoderConfig->decoderSpecificInfo->dataLength, &headerLength);
						}
					}
				} else {
					gf_webvtt_dump_header_boxed(med, esd->decoderConfig->decoderSpecificInfo->data+4, esd->decoderConfig->decoderSpecificInfo->dataLength, &headerLength);
					gf_fprintf(vtt, "text-header-length: %d\n", headerLength);
				}
			} else {
				char b64[200];
				u32 size = gf_base64_encode(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, b64, 200);
				useBase64 = GF_TRUE;
				if (size != (u32)-1 && size != 0) {
					b64[size] = 0;
					gf_fprintf(vtt, "MPEG-4-DecoderSpecificInfo: %s\n", b64);
				}
			}
		}
		gf_odf_desc_del((GF_Descriptor *) esd);
	} else {
		GF_GenericSampleDescription *sdesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
		gf_fprintf(vtt, "mediaType: %s\n", gf_4cc_to_str(mtype));
		gf_fprintf(vtt, "mediaSubType: %s\n", gf_4cc_to_str(mstype ));
		if (sdesc) {
			if (gf_isom_is_video_handler_type(mtype) ) {
				gf_fprintf(vtt, "codecVendor: %s\n", gf_4cc_to_str(sdesc->vendor_code));
				gf_fprintf(vtt, "codecVersion: %d\n", sdesc->version);
				gf_fprintf(vtt, "codecRevision: %d\n", sdesc->revision);
				gf_fprintf(vtt, "width: %d\n", sdesc->width);
				gf_fprintf(vtt, "height: %d\n", sdesc->height);
				gf_fprintf(vtt, "compressorName: %s\n", sdesc->compressor_name);
				gf_fprintf(vtt, "temporalQuality: %d\n", sdesc->temporal_quality);
				gf_fprintf(vtt, "spatialQuality: %d\n", sdesc->spatial_quality);
				gf_fprintf(vtt, "horizontalResolution: %d\n", sdesc->h_res);
				gf_fprintf(vtt, "verticalResolution: %d\n", sdesc->v_res);
				gf_fprintf(vtt, "bitDepth: %d\n", sdesc->depth);
			} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
				gf_fprintf(vtt, "codecVendor: %s\n", gf_4cc_to_str(sdesc->vendor_code));
				gf_fprintf(vtt, "codecVersion: %d\n", sdesc->version);
				gf_fprintf(vtt, "codecRevision: %d\n", sdesc->revision);
				gf_fprintf(vtt, "sampleRate: %d\n", sdesc->samplerate);
				gf_fprintf(vtt, "numChannels: %d\n", sdesc->nb_channels);
				gf_fprintf(vtt, "bitsPerSample: %d\n", sdesc->bits_per_sample);
			}
			if (sdesc->extension_buf) {
				char b64[200];
				u32 size = gf_base64_encode(sdesc->extension_buf, sdesc->extension_buf_size, b64, 200);
				useBase64 = GF_TRUE;
				if (size != (u32)-1) {
					b64[size] = 0;
					gf_fprintf(vtt, "specificInfo: %s\n", b64);
					gf_free(sdesc->extension_buf);
				}
			}
			gf_free(sdesc);
		}
	}
	gf_fprintf(vtt, "inBandMetadataTrackDispatchType: %s\n", (mime ? mime : (isText? "text/plain" : "application/octet-stream")));
	if (useBase64) gf_fprintf(vtt, "encoding: base64\n");

	gf_fprintf(vtt, "\n");

	pos = 0;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		{
			GF_WebVTTTimestamp start, end;
			u64 dur = gf_isom_get_sample_duration(dumper->file, track, i+1);
			gf_webvtt_timestamp_set(&start, samp->DTS);
			gf_webvtt_timestamp_set(&end, samp->DTS+dur);
			gf_webvtt_timestamp_dump(&start, vtt, GF_TRUE);
			gf_fprintf(vtt, " --> ");
			gf_webvtt_timestamp_dump(&end, vtt, GF_TRUE);
			gf_fprintf(vtt, " ");
			if (med) {
				gf_fprintf(vtt, "mediaOffset:%d ", pos+headerLength);
				gf_fprintf(vtt, "dataLength:%d ", samp->dataLength);
			}
			if (samp->CTS_Offset) gf_fprintf(vtt, "CTS: "LLD"", samp->DTS+samp->CTS_Offset);
			if (samp->IsRAP==RAP) gf_fprintf(vtt, "isRAP:true ");
			else if (samp->IsRAP==RAP_REDUNDANT) gf_fprintf(vtt, "isSyncShadow: true ");
			else gf_fprintf(vtt, "isRAP:false ");
			gf_fprintf(vtt, "\n");
		}
		if (med) {
			gf_fwrite(samp->data, samp->dataLength, med);
		} else if (dumper->flags & GF_EXPORT_WEBVTT_META_EMBEDDED) {
			if (isText) {
				samp->data = (char *)gf_realloc(samp->data, samp->dataLength+1);
				samp->data[samp->dataLength] = 0;
				gf_fprintf(vtt, "%s\n", samp->data);
			} else {
				u32 b64_size = samp->dataLength*2 + 3;
				char *b64;
				b64 = (char *)gf_malloc(sizeof(char)*b64_size);
				b64_size = gf_base64_encode(samp->data, samp->dataLength, b64, b64_size);
				b64[b64_size] = 0;
				gf_fprintf(vtt, "%s\n", b64);
				gf_free(b64);
			}
		}
		gf_fprintf(vtt, "\n");

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		gf_set_progress("WebVTT metadata Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	if (med) gf_fclose(med);
	gf_fclose(vtt);
	return GF_OK;
}

#endif /*GPAC_DISABLE_VTT*/

/* Experimental Streaming Instructions XML export */
GF_Err gf_media_export_six(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	char szName[1000], szMedia[1000];
	FILE *media, *six;
	u32 track, i, di, count, pos, header_size;
	//u32 mtype;
#if !defined(GPAC_DISABLE_TTXT) && !defined(GPAC_DISABLE_VTT)
	u32 mstype;
#endif
	const char *szRootName;
	//Bool isText;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	if (!track) return gf_export_message(dumper, GF_BAD_PARAM, "Invalid track ID %d", dumper->trackID);

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		dumper->flags |= GF_EXPORT_NHML_FULL;
		return GF_OK;
	}
	esd = gf_isom_get_esd(dumper->file, track, 1);

	sprintf(szMedia, "%s.media", dumper->out_name);
	media = gf_fopen(szMedia, "wb");
	if (!media) {
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szMedia);
	}

	sprintf(szName, "%s.six", dumper->out_name);
	szRootName = "stream";

	six = gf_fopen(szName, "wt");
	if (!six) {
		gf_fclose(media);
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}
	/*
		mtype = gf_isom_get_media_type(dumper->file, track);
		if (mtype==GF_ISOM_MEDIA_TEXT || mtype == GF_ISOM_MEDIA_SUBM || mtype == GF_ISOM_MEDIA_SUBT) {
			isText = GF_TRUE;
		} else {
			isText = GF_FALSE;
		}
	*/
#if !defined(GPAC_DISABLE_TTXT) && !defined(GPAC_DISABLE_VTT)
	mstype = gf_isom_get_media_subtype(dumper->file, track, 1);
#endif

	/*write header*/
	gf_fprintf(six, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	gf_fprintf(six, "<%s timescale=\"%d\" ", szRootName, gf_isom_get_media_timescale(dumper->file, track) );
	gf_fprintf(six, "file=\"%s\" ", szMedia);
	gf_fprintf(six, ">\n");
	header_size = 0;
	if (esd) {
		if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
#if !defined(GPAC_DISABLE_TTXT) && !defined(GPAC_DISABLE_VTT)
			if (mstype == GF_ISOM_SUBTYPE_WVTT || mstype == GF_ISOM_SUBTYPE_STXT) {
				gf_webvtt_dump_header_boxed(media,
				                            esd->decoderConfig->decoderSpecificInfo->data+4,
				                            esd->decoderConfig->decoderSpecificInfo->dataLength,
				                            &header_size);
			} else
#endif
			{
				gf_fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, media);
				header_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
			}
		}
		gf_odf_desc_del((GF_Descriptor *) esd);
	} else {
		GF_GenericSampleDescription *sdesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
		if (sdesc) {
			header_size = sdesc->extension_buf_size;
			gf_free(sdesc);
		}
	}
	gf_fprintf(six, "<header range-begin=\"0\" range-end=\"%d\"/>\n", header_size-1);

	pos = header_size;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		if (media) {
			gf_fwrite(samp->data, samp->dataLength, media);
		}

		gf_fprintf(six, "<unit time=\""LLU"\" ", samp->DTS);
		if (samp->IsRAP==RAP) gf_fprintf(six, "rap=\"1\" ");
		else if (samp->IsRAP==RAP_NO) gf_fprintf(six, "rap=\"0\" ");
		gf_fprintf(six, "range-begin=\"%d\" ", pos);
		gf_fprintf(six, "range-end=\"%d\" ", pos+samp->dataLength-1);
		gf_fprintf(six, "/>\n");

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		gf_set_progress("SIX Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	gf_fprintf(six, "</%s>\n", szRootName);
	if (media) gf_fclose(media);
	gf_fclose(six);
	return GF_OK;

}

typedef struct
{
	u32 track_num, stream_id, last_sample, nb_samp;
} SAFInfo;

GF_Err gf_media_export_saf(GF_MediaExporter *dumper)
{
#ifndef GPAC_DISABLE_SAF
	u32 count, i, s_count, di, tot_samp, samp_done;
	char out_file[GF_MAX_PATH];
	GF_SAFMuxer *mux;
	u8 *data;
	u32 size;
	Bool is_stdout = 0;
	FILE *saf_f;
	SAFInfo safs[1024];

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return GF_OK;

	s_count = tot_samp = 0;

	mux = gf_saf_mux_new();
	count = gf_isom_get_track_count(dumper->file);
	for (i=0; i<count; i++) {
		u32 time_scale, mtype, stream_id = 0;
		GF_ESD *esd;
		mtype = gf_isom_get_media_type(dumper->file, i+1);
		if (mtype==GF_ISOM_MEDIA_OD) continue;
		if (mtype==GF_ISOM_MEDIA_HINT) continue;

		time_scale = gf_isom_get_media_timescale(dumper->file, i+1);
		esd = gf_isom_get_esd(dumper->file, i+1, 1);
		if (esd && esd->decoderConfig) {
			stream_id = gf_isom_find_od_id_for_track(dumper->file, i+1);
			if (!stream_id) stream_id = esd->ESID;

			/*translate OD IDs to ESIDs !!*/
			if (esd->decoderConfig->decoderSpecificInfo) {
				gf_saf_mux_stream_add(mux, stream_id, time_scale, esd->decoderConfig->bufferSizeDB, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication, NULL, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->URLString);
			} else {
				gf_saf_mux_stream_add(mux, stream_id, time_scale, esd->decoderConfig->bufferSizeDB, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication, NULL, NULL, 0, esd->URLString);
			}
		}
		if (esd)
			gf_odf_desc_del((GF_Descriptor *)esd);

		if (!stream_id) {
			char *mime = NULL;
			switch (gf_isom_get_media_subtype(dumper->file, i+1, 1)) {
			case GF_ISOM_SUBTYPE_3GP_H263:
				mime = "video/h263";
				break;
			case GF_ISOM_SUBTYPE_3GP_AMR:
				mime = "audio/amr";
				break;
			case GF_ISOM_SUBTYPE_3GP_AMR_WB:
				mime = "audio/amr-wb";
				break;
			case GF_ISOM_SUBTYPE_3GP_EVRC:
				mime = "audio/evrc";
				break;
			case GF_ISOM_SUBTYPE_3GP_QCELP:
				mime = "audio/qcelp";
				break;
			case GF_ISOM_SUBTYPE_3GP_SMV:
				mime = "audio/smv";
				break;
			}
			if (!mime) continue;
			stream_id = gf_isom_get_track_id(dumper->file, i+1);
			gf_saf_mux_stream_add(mux, stream_id, time_scale, 0, 0xFF, 0xFF, mime, NULL, 0, NULL);
		}

		safs[s_count].track_num = i+1;
		safs[s_count].stream_id = stream_id;
		safs[s_count].nb_samp = gf_isom_get_sample_count(dumper->file, i+1);
		safs[s_count].last_sample = 0;

		tot_samp += safs[s_count].nb_samp;

		s_count++;
	}

	if (!s_count) {
		gf_export_message(dumper, GF_OK, "No tracks available for SAF muxing");
		gf_saf_mux_del(mux);
		return GF_OK;
	}
	gf_export_message(dumper, GF_OK, "SAF: Multiplexing %d tracks", s_count);

	if (dumper->out_name && !strcmp(dumper->out_name, "std"))
		is_stdout = 1;
	strcpy(out_file, dumper->out_name ? dumper->out_name : "");
	strcat(out_file, ".saf");
	saf_f = is_stdout ? stdout : gf_fopen(out_file, "wb");

	samp_done = 0;
	while (samp_done<tot_samp) {
		for (i=0; i<s_count; i++) {
			GF_ISOSample *samp;
			if (safs[i].last_sample==safs[i].nb_samp) continue;
			samp = gf_isom_get_sample(dumper->file, safs[i].track_num, safs[i].last_sample + 1, &di);
			if (!samp) {
				gf_saf_mux_del(mux);
				return gf_isom_last_error(dumper->file);
			}

			gf_saf_mux_add_au(mux, safs[i].stream_id, (u32) (samp->DTS+samp->CTS_Offset), samp->data, samp->dataLength, (samp->IsRAP==RAP) ? 1 : 0);
			/*data is kept by muxer!!*/
			gf_free(samp);
			safs[i].last_sample++;
			samp_done ++;
		}
		while (1) {
			gf_saf_mux_for_time(mux, (u32) -1, 0, &data, &size);
			if (!data) break;
			gf_fwrite(data, size, saf_f);
			gf_free(data);
		}
		gf_set_progress("SAF Export", samp_done, tot_samp);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	gf_saf_mux_for_time(mux, (u32) -1, 1, &data, &size);
	if (data) {
		gf_fwrite(data, size, saf_f);
		gf_free(data);
	}
	if (!is_stdout)
		gf_fclose(saf_f);

	gf_saf_mux_del(mux);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}


static GF_Err gf_media_export_filters(GF_MediaExporter *dumper)
{
	char *args, szSubArgs[1024], szExt[30];
	GF_Filter *file_out, *reframer, *remux=NULL, *src_filter;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;
	u32 codec_id=0;
	u32 sample_count=0;
	Bool skip_write_filter = GF_FALSE;
	Bool ext_forced = GF_FALSE;
	Bool use_dynext = GF_FALSE;

	args = NULL;
	strcpy(szExt, "");
	if (dumper->trackID && dumper->file) {
		u32 msubtype = 0;
		u32 mtype = 0;
		u32 afmt = 0;
		GF_PixelFormat pfmt = 0;
		GF_ESD *esd;
		const char *export_ext = dumper->out_name ? gf_file_ext_start(dumper->out_name) : NULL;
		u32 track_num = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
		if (!track_num) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] No tracks with ID %d in file\n", dumper->trackID));
			return GF_BAD_PARAM;
		}
		esd = gf_media_map_esd(dumper->file, track_num, 0);
		sample_count = gf_isom_get_sample_count(dumper->file, dumper->trackID);
		if (esd && esd->decoderConfig) {
			if (esd->decoderConfig->objectTypeIndication<GF_CODECID_LAST_MPEG4_MAPPING) {
				codec_id = gf_codecid_from_oti(esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
#ifndef GPAC_DISABLE_AV_PARSERS
				if (esd->decoderConfig->decoderSpecificInfo && (codec_id==GF_CODECID_AAC_MPEG4)) {
					GF_M4ADecSpecInfo acfg;
					gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &acfg);
					if (acfg.base_object_type == GF_M4A_USAC)
						codec_id = GF_CODECID_USAC;
				}
#endif
			} else {
				codec_id = esd->decoderConfig->objectTypeIndication;
			}
		}
		if (!codec_id) {
			msubtype = gf_isom_get_media_subtype(dumper->file, track_num, 1);
			codec_id = gf_codec_id_from_isobmf(msubtype);
		}
		mtype = gf_isom_get_media_type(dumper->file, track_num);
		if (!codec_id) {
			pfmt = gf_pixel_fmt_from_qt_type(msubtype);
			if (pfmt) codec_id = GF_CODECID_RAW;
		}

		if (!codec_id) {
			strcpy(szExt, gf_4cc_to_str(msubtype));
			ext_forced = GF_TRUE;
		} else if (codec_id==GF_CODECID_RAW) {
			switch (mtype) {
			case GF_ISOM_MEDIA_VISUAL:
			case GF_ISOM_MEDIA_AUXV:
			case GF_ISOM_MEDIA_PICT:
				if (pfmt)
					strcpy(szExt, gf_pixel_fmt_sname(pfmt));
				break;
			case GF_ISOM_MEDIA_AUDIO:
				afmt = gf_audio_fmt_from_isobmf(msubtype);
				if (afmt)
					strcpy(szExt, gf_audio_fmt_name(afmt));
				break;
			default:
				strcpy(szExt, gf_4cc_to_str(msubtype));
				break;
			}
		} else {
			const char *sname = gf_codecid_file_ext(codec_id);
			if (export_ext && strstr(sname, export_ext+1)) {
				szExt[0]=0;
			} else {
				char *sep;
				strncpy(szExt, sname, 29);
				szExt[29]=0;
				sep = strchr(szExt, '|');
				if (sep) sep[0] = 0;
			}
		}
		switch (mtype) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_AUDIO:
			skip_write_filter = codec_id ? GF_TRUE : GF_FALSE;
			break;
		default:
			switch (codec_id) {
			case GF_CODECID_WEBVTT:
				skip_write_filter = GF_TRUE;
				break;
			case GF_CODECID_META_TEXT:
			case GF_CODECID_META_XML:
			case GF_CODECID_SUBS_TEXT:
			case GF_CODECID_SUBS_XML:
			case GF_CODECID_SIMPLE_TEXT:
				//use dynamic extension
				szExt[0] = 0;
				use_dynext = GF_TRUE;
				break;
			}
			break;
		}

		if ((codec_id==GF_CODECID_VORBIS) || (codec_id==GF_CODECID_THEORA) || (codec_id==GF_CODECID_SPEEX))
			strcpy(szExt, "ogg");
		else if (codec_id==GF_CODECID_OPUS)
			strcpy(szExt, "opus");

		if (codec_id==GF_CODECID_SUBPIC) {
#ifndef GPAC_DISABLE_AV_PARSERS
			char *dsi = NULL;
			u32 dsi_size = 0;
			if (esd && esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo) {
				dsi = esd->decoderConfig->decoderSpecificInfo->data;
				dsi_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
			}
			e = gf_dump_to_vobsub(dumper, dumper->out_name, track_num, dsi, dsi_size);
#else
			e = GF_NOT_SUPPORTED;
#endif
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return e;
		}
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
	} else {
		const char *export_ext = dumper->out_name ? gf_file_ext_start(dumper->out_name) : NULL;
		if (!export_ext) {
			use_dynext = GF_TRUE;
			if (dumper->in_name) {
				GF_MediaImporter import;
				memset(&import, 0, sizeof(GF_MediaImporter));
				import.flags = GF_IMPORT_PROBE_ONLY;
				import.in_name = dumper->in_name;
				e = gf_media_import(&import);
				if (e) return e;
				Bool found = GF_FALSE;
				u32 i;
				for (i=0; i<import.nb_tracks; i++) {
					struct __track_import_info *tki = &import.tk_info[i];
					if (!tki->codecid) continue;
					if (dumper->trackID) {
						if (dumper->trackID != tki->track_num) continue;
					} else if (dumper->track_type) {
						if ((dumper->track_type==1) && (tki->stream_type!=GF_STREAM_VISUAL)) continue;
						if ((dumper->track_type==2) && (tki->stream_type!=GF_STREAM_AUDIO)) continue;
					}

					found = GF_TRUE;
					skip_write_filter = GF_TRUE;

					const char *sname = gf_codecid_file_ext(tki->codecid);
					if (export_ext && strstr(sname, export_ext+1)) {
						szExt[0]=0;
					} else {
						char *sep;
						strncpy(szExt, sname, 29);
						szExt[29]=0;
						sep = strchr(szExt, '|');
						if (sep) sep[0] = 0;
						use_dynext = GF_FALSE;
					}
					dumper->trackID = tki->track_num;
					dumper->track_type = 0;
					break;
				}
				if (!found) return GF_NOT_FOUND;
			}
		} else {
			skip_write_filter = GF_TRUE;
		}
	}

	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}
	file_out = NULL;
	args = NULL;

	if (dumper->flags & GF_EXPORT_REMUX) {
		file_out = gf_fs_load_destination(fsess, dumper->out_name, NULL, NULL, &e);
		if (!file_out) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot open destination %s\n", dumper->out_name));
			return e;
		}
	}
	//except in nhml inband file dump, create a sink filter
	else if (!dumper->dump_file) {
		Bool no_ext = (dumper->flags & GF_EXPORT_NO_FILE_EXT) ? GF_TRUE : GF_FALSE;
		char *ext = gf_file_ext_start(dumper->out_name);
		//mux args, for now we only dump to file
		e = gf_dynstrcat(&args, "fout:dst=", NULL);
		e |= gf_dynstrcat(&args, dumper->out_name, NULL);

		if (dumper->flags & GF_EXPORT_NHNT) {
			strcpy(szExt, "nhnt");
			e |= gf_dynstrcat(&args, ":clone", NULL);
			no_ext = GF_TRUE;
			if (!ext)
				e |= gf_dynstrcat(&args, ":dynext", NULL);
		} else if (dumper->flags & GF_EXPORT_NHML) {
			strcpy(szExt, "nhml");
			e |= gf_dynstrcat(&args, ":clone", NULL);
			no_ext = GF_TRUE;
			if (!ext)
				e |= gf_dynstrcat(&args, ":dynext", NULL);
		}

		if (dumper->flags & GF_EXPORT_RAW_SAMPLES) {
			if (!dumper->sample_num) {

				ext = gf_file_ext_start(args);
				if (ext) ext[0] = 0;
				if (sample_count>=1000) {
					e |= gf_dynstrcat(&args, "_$num%08d$", NULL);
				} else if (sample_count) {
					e |= gf_dynstrcat(&args, "_$num%03d$", NULL);
				} else {
					e |= gf_dynstrcat(&args, "_$num$", NULL);
				}
				ext = gf_file_ext_start(dumper->out_name);
				if (ext) e |= gf_dynstrcat(&args, ext, NULL);
			}
			e |= gf_dynstrcat(&args, ":dynext", NULL);
		} else if (dumper->trackID && strlen(szExt) ) {
			if (!no_ext && !gf_file_ext_start(dumper->out_name)) {
				if (args) gf_free(args);
				args=NULL;
				e = gf_dynstrcat(&args, "fout:dst=", NULL);
				e |= gf_dynstrcat(&args, dumper->out_name, NULL);
				e |= gf_dynstrcat(&args, szExt, ".");
			} else {
				e |= gf_dynstrcat(&args, ":ext=", NULL);
				e |= gf_dynstrcat(&args, szExt, NULL);
			}
		} else if ((dumper->trackID || dumper->track_type) && use_dynext) {
			e |= gf_dynstrcat(&args, ":dynext", NULL);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load arguments for output file dumper\n"));
			if (args) gf_free(args);
			gf_fs_del(fsess);
			return e;
		}

		file_out = gf_fs_load_filter(fsess, args, &e);
		if (!file_out) {
			gf_fs_del(fsess);
			if (args) gf_free(args);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load output file dumper\n"));
			return e;
		}
	}
	if (args) gf_free(args);
	args = NULL;

	//raw sample frame, force loading filter generic write in frame mode
	if (dumper->flags & GF_EXPORT_RAW_SAMPLES) {
		e = gf_dynstrcat(&args, "writegen:frame", NULL);
		if (dumper->sample_num) {
			sprintf(szSubArgs, ":sstart=%d:send=%d", dumper->sample_num, dumper->sample_num);
			e |= gf_dynstrcat(&args, szSubArgs, NULL);
		}
		remux = e ? NULL : gf_fs_load_filter(fsess, args, &e);
		if (!remux || e) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load stream->file filter\n"));
			if (args) gf_free(args);
			return e ? e : GF_FILTER_NOT_FOUND;
		}
	}
	else if (dumper->flags & GF_EXPORT_NHNT) {
		remux = gf_fs_load_filter(fsess, "nhntw:exporter", &e);
		if (!remux) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load NHNT write filter\n"));
			return e;
		}
	}
	else if (dumper->flags & GF_EXPORT_NHML) {
		e = gf_dynstrcat(&args, "nhmlw:exporter:name=", NULL);
		e |= gf_dynstrcat(&args, dumper->out_name, NULL);
		if (dumper->flags & GF_EXPORT_NHML_FULL)
			e |= gf_dynstrcat(&args, ":pckp", NULL);
		if (dumper->dump_file) {
			sprintf(szSubArgs, ":nhmlonly:filep=%p", dumper->dump_file);
			e |= gf_dynstrcat(&args, szSubArgs, NULL);
		}
		remux = e ? NULL : gf_fs_load_filter(fsess, args, &e);
		if (!remux || e) {
			gf_fs_del(fsess);
			if (args) gf_free(args);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load NHML write filter\n"));
			return e ? e : GF_FILTER_NOT_FOUND;
		}
	} else if (!skip_write_filter) {
		e = gf_dynstrcat(&args, "writegen:exporter", NULL);
		//extension has been forced, override ext at output of writegen
		if (ext_forced) {
			e |= gf_dynstrcat(&args, ":#Extension=", NULL);
			e |= gf_dynstrcat(&args, szExt, NULL);
		}

		remux = e ? NULL : gf_fs_load_filter(fsess, args, &e);
		if (!remux) {
			gf_fs_del(fsess);
			if (args) gf_free(args);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load stream->file filter\n"));
			return e;
		}
	}
	if (args) gf_free(args);
	args = NULL;

	//force a reframer filter, connected to our input
	e = gf_dynstrcat(&args, "reframer:SID=1", NULL);
	if (dumper->trackID) {
		sprintf(szSubArgs, "#PID=%d", dumper->trackID);
		e |= gf_dynstrcat(&args, szSubArgs, NULL);
	}
	e |= gf_dynstrcat(&args, ":exporter", NULL);
	if (dumper->flags & GF_EXPORT_SVC_LAYER)
		e |= gf_dynstrcat(&args, ":extract=layer", NULL);
	if (dumper->flags & GF_EXPORT_WEBVTT_NOMERGE)
		e |= gf_dynstrcat(&args, ":merge", NULL);

	reframer = gf_fs_load_filter(fsess, args, &e);
	if (!reframer || e) {
		gf_fs_del(fsess);
		if (args) gf_free(args);
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load reframer filter\n"));
		return e ? e : GF_FILTER_NOT_FOUND;
	}
	if (args) gf_free(args);
	args = NULL;

	//we already have the file loaded, directly load the mp4dmx filter with this file
	if (dumper->file) {
		//we want to expose every track
		e = gf_dynstrcat(&args, "mp4dmx:FID=1:noedit:alltk:allt", NULL);
		if (!e) {
			sprintf(szSubArgs, ":mov=%p", dumper->file);
			e = gf_dynstrcat(&args, szSubArgs, NULL);
		}
		
		//we want to expose every track
		src_filter = gf_fs_load_filter(fsess, args, &e);

		gf_free(args);
		args = NULL;
	} else {
		//we want to expose every track
		src_filter = gf_fs_load_source(fsess, dumper->in_name, "FID=1:noedit:alltk:allt", NULL, &e);
	}

	if (!src_filter || e) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Exporter] Cannot load filter for input file \"%s\": %s\n", dumper->in_name, gf_error_to_string(e) ));
		return e;
	}

	if (dumper->track_type) {
		const char *mtype = (dumper->track_type==1) ? "video" : "audio";
		if (dumper->trackID) {
			sprintf(szSubArgs, "%s%d", mtype, dumper->trackID);
		} else {
			sprintf(szSubArgs, "%s", mtype);
		}
	}
	else if (dumper->trackID) {
		sprintf(szSubArgs, "PID=%d", dumper->trackID);
	}
	if (remux) {
		gf_filter_set_source(file_out, remux, (dumper->trackID || dumper->track_type) ? szSubArgs : NULL);
		gf_filter_set_source(remux, reframer, (dumper->trackID || dumper->track_type) ? szSubArgs : NULL);
	} else {
		gf_filter_set_source(file_out, reframer, (dumper->trackID || dumper->track_type) ? szSubArgs : NULL);
	}

	e = gf_fs_run(fsess);
	if (e>GF_OK) e = GF_OK;
	if (!e) e = gf_fs_get_last_connect_error(fsess);
	if (!e) e = gf_fs_get_last_process_error(fsess);

	if (!e) {
		if (dumper->file)
			gf_fs_print_unused_args(fsess, NULL);
		else
			gf_fs_print_unused_args(fsess, "alltk,allt,noedit");
	}
	gf_fs_print_non_connected(fsess);
	if (dumper->print_stats_graph & 1) gf_fs_print_stats(fsess);
	if (dumper->print_stats_graph & 2) gf_fs_print_connections(fsess);
	gf_fs_del(fsess);
	return e;
}

GF_EXPORT
GF_Err gf_media_export(GF_MediaExporter *dumper)
{
	if (!dumper) return GF_BAD_PARAM;
	if (!dumper->out_name && !(dumper->flags & GF_EXPORT_PROBE_ONLY) && !dumper->dump_file) return GF_BAD_PARAM;

	//internal export not using filters

#ifndef GPAC_DISABLE_ISOM_WRITE
	if (dumper->flags & GF_EXPORT_MP4) return gf_media_export_isom(dumper);
#endif /*GPAC_DISABLE_ISOM_WRITE*/
#ifndef GPAC_DISABLE_VTT
	else if (dumper->flags & GF_EXPORT_WEBVTT_META) return gf_media_export_webvtt_metadata(dumper);
#endif
	else if (dumper->flags & GF_EXPORT_SIX) return gf_media_export_six(dumper);

	//the following ones should be moved to muxing filters
	else if (dumper->flags & GF_EXPORT_SAF) return gf_media_export_saf(dumper);

	//the rest is handled by the generic exporter
	return gf_media_export_filters(dumper);
}

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/
