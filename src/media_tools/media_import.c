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

#include <gpac/internal/media_dev.h>
#ifndef GPAC_DISABLE_AVILIB
#include <gpac/internal/avilib.h>
#endif
#ifndef GPAC_DISABLE_OGG
#include <gpac/internal/ogg.h>
#endif
#ifndef GPAC_DISABLE_VOBSUB
#include <gpac/internal/vobsub.h>
#endif
#include <gpac/xml.h>
#include <gpac/mpegts.h>
#include <gpac/constants.h>
#include <gpac/base_coding.h>
#include <gpac/internal/isomedia_dev.h>


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


static GF_Err gf_media_update_par(GF_ISOFile *file, u32 track)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	u32 tk_w, tk_h, stype;
	GF_Err e;

	e = gf_isom_get_visual_info(file, track, 1, &tk_w, &tk_h);
	if (e) return e;

	stype = gf_isom_get_media_subtype(file, track, 1);
	if ((stype==GF_ISOM_SUBTYPE_AVC_H264) || (stype==GF_ISOM_SUBTYPE_AVC2_H264)
	        || (stype==GF_ISOM_SUBTYPE_AVC3_H264) || (stype==GF_ISOM_SUBTYPE_AVC4_H264)
	   ) {
		s32 par_n, par_d;
		GF_AVCConfig *avcc = gf_isom_avc_config_get(file, track, 1);
		GF_AVCConfigSlot *slc = (GF_AVCConfigSlot *)gf_list_get(avcc->sequenceParameterSets, 0);
		par_n = par_d = 1;
		if (slc) gf_avc_get_sps_info(slc->data, slc->size, NULL, NULL, NULL, &par_n, &par_d);
		gf_odf_avc_cfg_del(avcc);

		if ((par_n>1) && (par_d>1))
			tk_w = tk_w * par_n / par_d;
	}
	else if ((stype==GF_ISOM_SUBTYPE_MPEG4) || (stype==GF_ISOM_SUBTYPE_MPEG4_CRYP) ) {
		GF_M4VDecSpecInfo dsi;
		GF_ESD *esd = gf_isom_get_esd(file, track, 1);
		if (!esd || !esd->decoderConfig || (esd->decoderConfig->streamType!=4) || (esd->decoderConfig->objectTypeIndication!=GPAC_OTI_VIDEO_MPEG4_PART2)) {
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return GF_NOT_SUPPORTED;
		}
		gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);

		if ((dsi.par_num>1) && (dsi.par_den>1))
			tk_w = dsi.width * dsi.par_num / dsi.par_den;
	} else {
		return GF_OK;
	}
	return gf_isom_set_track_layout_info(file, track, tk_w<<16, tk_h<<16, 0, 0, 0);
#else
	return GF_OK;
#endif /*GPAC_DISABLE_AV_PARSERS*/
}


GF_EXPORT
void gf_media_update_bitrate(GF_ISOFile *file, u32 track)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 i, count, timescale, db_size, ofmt, cdur, csize;
	u64 time_wnd, rate, max_rate, avg_rate, bitrate;
	Double br;
	GF_ESD *esd = NULL;

	switch (gf_isom_get_media_subtype(file, track, 1)) {
	case GF_ISOM_SUBTYPE_MPEG4:
		esd = gf_isom_get_esd(file, track, 1);
		break;
	case GF_ISOM_SUBTYPE_MPEG4_CRYP:
		gf_isom_get_original_format_type(file, track, 1, &ofmt);
		switch (ofmt) {
		case GF_ISOM_BOX_TYPE_MP4S:
		case GF_ISOM_BOX_TYPE_MP4A:
		case GF_ISOM_BOX_TYPE_MP4V:
			esd = gf_isom_get_esd(file, track, 1);
			break;
		}
	}

	db_size = 0;
	rate = max_rate = avg_rate = time_wnd = 0;

	csize = 0;
	cdur = 0;
	if (gf_isom_get_media_type(file, track)==GF_ISOM_MEDIA_AUDIO) {
		csize = gf_isom_get_constant_sample_size(file, track);
		cdur = gf_isom_get_constant_sample_duration(file, track);
		if (cdur > 1) cdur = 0;
	}

	timescale = gf_isom_get_media_timescale(file, track);
	count = gf_isom_get_sample_count(file, track);
	if (csize && cdur) {
		db_size = 0;
		avg_rate = 8 * csize * timescale / cdur;
		bitrate = rate = avg_rate;
	} else {
		for (i=0; i<count; i++) {
			GF_ISOSample *samp = gf_isom_get_sample_info(file, track, i+1, NULL, NULL);

			if (samp->dataLength > db_size) db_size = samp->dataLength;

			avg_rate += samp->dataLength;
			rate += samp->dataLength;
			if (samp->DTS > time_wnd + timescale) {
				if (rate > max_rate) max_rate = rate;
				time_wnd = samp->DTS;
				rate = 0;
			}
			gf_isom_sample_del(&samp);
		}
		br = (Double) (s64) gf_isom_get_media_duration(file, track);
		br /= timescale;
		bitrate = (u32) ((Double) (s64)avg_rate / br);
		bitrate *= 8;
		max_rate *= 8;
	}
	if (!max_rate) max_rate = bitrate;

	/*move to bps*/
	if (esd) {
		esd->decoderConfig->avgBitrate = (u32) bitrate;
		esd->decoderConfig->maxBitrate = (u32) max_rate;
		esd->decoderConfig->bufferSizeDB = db_size;
		gf_isom_change_mpeg4_description(file, track, 1, esd);
		gf_odf_desc_del((GF_Descriptor *)esd);
	} else {
		gf_isom_update_bitrate(file, track, 1, (u32) bitrate, (u32) max_rate, db_size);
	}

#endif
}

static void get_video_timing(Double fps, u32 *timescale, u32 *dts_inc)
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

static GF_Err gf_import_still_image(GF_MediaImporter *import, Bool mult_desc_allowed)
{
	GF_BitStream *bs;
	GF_Err e;
	Bool destroy_esd;
	u32 size, track, di, w, h, dsi_len, mtype, id;
	GF_ISOSample *samp;
	u8 OTI;
	char *dsi, *data;
	FILE *src;
	Bool import_mpeg4 = GF_FALSE;

	if (import->flags & GF_IMPORT_FORCE_MPEG4)
		import_mpeg4 = GF_TRUE;
	else if (import->esd)
		import_mpeg4 = GF_TRUE;

	src = gf_fopen(import->in_name, "rb");
	if (!src) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	gf_fseek(src, 0, SEEK_END);
	size = (u32) gf_ftell(src);
	gf_fseek(src, 0, SEEK_SET);
	data = (char*)gf_malloc(sizeof(char)*size);
	size = (u32) fread(data, sizeof(char), size, src);
	gf_fclose(src);
	if ((s32) size <= 0) {
		gf_free(data);
		return gf_import_message(import, GF_URL_ERROR, "Reading file %s failed", import->in_name);
	}
	/*get image size*/
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	dsi = NULL;
	gf_img_parse(bs, &OTI, &mtype, &w, &h, import_mpeg4 ? &dsi : NULL, import_mpeg4 ? &dsi_len : NULL);
	gf_bs_del(bs);

	if (!OTI) {
		gf_free(data);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Unrecognized file %s", import->in_name);
	}

	if (!w || !h) {
		gf_free(data);
		if (dsi) gf_free(dsi);
		return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Invalid %s file", (OTI==GPAC_OTI_IMAGE_JPEG) ? "JPEG" : (OTI==GPAC_OTI_IMAGE_PNG) ? "PNG" : "JPEG2000");
	}


	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].media_type = mtype;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_NO_DURATION;
		import->tk_info[0].video_info.width = w;
		import->tk_info[0].video_info.height = h;
		import->nb_tracks = 1;
		gf_free(data);
		if (dsi) gf_free(dsi);
		return GF_OK;
	}

	destroy_esd = GF_FALSE;

	id = 0;
	if (import_mpeg4) {

		if (!import->esd) {
			import->esd = gf_odf_desc_esd_new(2);
			destroy_esd = GF_TRUE;
		}
		/*update stream type/oti*/
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		import->esd->decoderConfig->objectTypeIndication = OTI;
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
		id = import->esd->ESID;
	}


	track = 0;
	if (mult_desc_allowed)
		track = gf_isom_get_track_by_id(import->dest, id);
	if (!track)
		track = gf_isom_new_track(import->dest, id, GF_ISOM_MEDIA_VISUAL, 1000);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);

	if (import_mpeg4) {
		if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
		import->final_trackID = import->esd->ESID;

		e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
		if (e) goto exit;
	} else {
		GF_GenericSampleDescription udesc;
		memset(&udesc, 0, sizeof(GF_GenericSampleDescription));
		switch (OTI) {
		case GPAC_OTI_IMAGE_JPEG:
			udesc.codec_tag = GF_ISOM_BOX_TYPE_JPEG;
			memcpy(udesc.compressor_name, " JPEG", 5);
			udesc.compressor_name[0] = 4;
			break;
		case GPAC_OTI_IMAGE_PNG:
			udesc.codec_tag = GF_ISOM_BOX_TYPE_PNG;
			memcpy(udesc.compressor_name, " PNG", 4);
			udesc.compressor_name[0] = 3;
			break;
		case GPAC_OTI_IMAGE_JPEG_2000:
			udesc.codec_tag = GF_ISOM_BOX_TYPE_JP2K;
			memcpy(udesc.compressor_name, " JPEG2000", 9);
			udesc.compressor_name[0] = 8;
			break;
		default:
			memcpy(udesc.compressor_name, " UNKNOWN", 8);
			udesc.compressor_name[0] = 7;
			break;
		}
		udesc.width = w;
		udesc.height = h;
		udesc.v_res = 72;
		udesc.h_res = 72;
		udesc.depth = 24;

		gf_isom_new_generic_sample_description(import->dest, track, NULL, NULL, &udesc, &di);
		import->final_trackID = gf_isom_get_track_id(import->dest, track);
	}

	gf_isom_set_visual_info(import->dest, track, di, w, h);
	samp = gf_isom_sample_new();
	samp->IsRAP = RAP;
	samp->dataLength = size;
	if (import->initial_time_offset) samp->DTS = (u64) (import->initial_time_offset*1000);

	gf_import_message(import, GF_OK, "%s import %s - size %d x %d", (OTI==GPAC_OTI_IMAGE_JPEG) ? "JPEG" : (OTI==GPAC_OTI_IMAGE_PNG) ? "PNG" : "JPEG2000", import->in_name, w, h);

	/*we must start a track from DTS = 0*/
	if (!gf_isom_get_sample_count(import->dest, track) && samp->DTS) {
		/*todo - we could add an edit list*/
		samp->DTS=0;
	}

	gf_set_progress("Importing Image", 0, 1);
	if (import->flags & GF_IMPORT_USE_DATAREF) {
		e = gf_isom_add_sample_reference(import->dest, track, di, samp, (u64) 0);
	} else {
		samp->data = data;
		e = gf_isom_add_sample(import->dest, track, di, samp);
		samp->data = NULL;
	}
	gf_set_progress("Importing Image", 1, 1);

	gf_isom_sample_del(&samp);
	if (import->duration) {
		gf_isom_set_last_sample_duration(import->dest, track, import->duration);
	}

exit:
	gf_free(data);
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	return e;
}

static GF_Err gf_import_afx_sc3dmc(GF_MediaImporter *import, Bool mult_desc_allowed)
{
	GF_Err e;
	Bool destroy_esd;
	u32 size, track, di, dsi_len;
	GF_ISOSample *samp;
	u8 OTI;
	char *dsi, *data;
	FILE *src;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_SCENE;
		import->tk_info[0].media_type = GPAC_OTI_SCENE_AFX;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_NO_DURATION;
		import->nb_tracks = 1;
		return GF_OK;
	}

	src = gf_fopen(import->in_name, "rb");
	if (!src) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	gf_fseek(src, 0, SEEK_END);
	size = (u32) gf_ftell(src);
	gf_fseek(src, 0, SEEK_SET);
	data = (char*)gf_malloc(sizeof(char)*size);
	size = (u32) fread(data, sizeof(char), size, src);
	gf_fclose(src);
	if ((s32) size < 0) return GF_IO_ERR;

	OTI = GPAC_OTI_SCENE_AFX;

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
	import->esd->decoderConfig->objectTypeIndication = OTI;
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

#ifndef GPAC_DISABLE_AV_PARSERS

GF_Err gf_import_mp3(GF_MediaImporter *import)
{
	u8 oti;
	Bool destroy_esd;
	GF_Err e;
	u16 sr;
	u32 nb_chan;
	Bool force_mpeg4 = GF_FALSE;
	FILE *in;
	u32 hdr, size, max_size, track, di, id3_end = 0;
	u64 done, tot_size, offset, duration;
	GF_ISOSample *samp;

	in = gf_fopen(import->in_name, "rb");
	if (!in) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);


	{
		unsigned char id3v2[10];
		u32 pos = (u32) fread(id3v2, sizeof(unsigned char), 10, in);
		if ((s32) pos < 0) return gf_import_message(import, GF_IO_ERR, "IO error reading file %s", import->in_name);

		if (pos == 10) {
			/* Did we read an ID3v2 ? */
			if (id3v2[0] == 'I' && id3v2[1] == 'D' && id3v2[2] == '3') {
				u32 sz = ((id3v2[9] & 0x7f) + ((id3v2[8] & 0x7f) << 7) + ((id3v2[7] & 0x7f) << 14) + ((id3v2[6] & 0x7f) << 21));

				while (sz) {
					u32 r = (u32) fread(id3v2, sizeof(unsigned char), 1, in);
					if (r != 1) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[MP3 import] failed to read ID3\n"));
					}
					sz--;
				}
				id3_end = (u32) gf_ftell(in);
			}
		}
		fseek(in, id3_end, SEEK_SET);
	}

	hdr = gf_mp3_get_next_header(in);
	if (!hdr) {
		gf_fclose(in);
		return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Audio isn't MPEG-1/2 audio");
	}
	sr = gf_mp3_sampling_rate(hdr);
	oti = gf_mp3_object_type_indication(hdr);
	if (!oti) {
		gf_fclose(in);
		return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Audio isn't MPEG-1/2 audio");
	}

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		gf_fclose(in);
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_AUDIO;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF;
		import->tk_info[0].audio_info.sample_rate = sr;
		import->tk_info[0].audio_info.nb_channels = gf_mp3_num_channels(hdr);
		import->nb_tracks = 1;
		return GF_OK;
	}

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(2);
		destroy_esd = GF_TRUE;
	} else {
		force_mpeg4 = GF_TRUE;
	}
	if (import->flags & GF_IMPORT_FORCE_MPEG4)
		force_mpeg4 = GF_TRUE;

	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	/*update stream type/oti*/
	import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	import->esd->decoderConfig->objectTypeIndication = oti;
	import->esd->decoderConfig->bufferSizeDB = 20;
	import->esd->slConfig->timestampResolution = sr;

	samp = NULL;
	nb_chan = gf_mp3_num_channels(hdr);
	gf_import_message(import, GF_OK, "MP3 import - sample rate %d - %s audio - %d channel%s", sr, (oti==GPAC_OTI_AUDIO_MPEG1) ? "MPEG-1" : "MPEG-2", nb_chan, (nb_chan>1) ? "s" : "");

	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, sr);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;
	if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
	import->esd->decoderConfig->decoderSpecificInfo = NULL;

	if (force_mpeg4) {
		gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	} else {
		GF_GenericSampleDescription udesc;
		memset(&udesc, 0, sizeof(GF_GenericSampleDescription));
		udesc.codec_tag = GF_ISOM_BOX_TYPE_MP3;
		memcpy(udesc.compressor_name, "\3MP3", 4);
		udesc.samplerate = sr;
		udesc.nb_channels = nb_chan;
		gf_isom_new_generic_sample_description(import->dest, track, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &udesc, &di);
	}

	gf_isom_set_audio_info(import->dest, track, di, sr, nb_chan, 16, import->asemode);

	gf_fseek(in, 0, SEEK_END);
	tot_size = gf_ftell(in);
	gf_fseek(in, id3_end, SEEK_SET);

	e = GF_OK;
	samp = gf_isom_sample_new();
	samp->IsRAP = RAP;

	duration = import->duration;
	duration *= sr;
	duration /= 1000;

	max_size = 0;
	done = 0;
	while (tot_size > done) {
		/* get the next MP3 frame header */
		hdr = gf_mp3_get_next_header(in);
		/*MP3 stream truncated*/
		if (!hdr) break;

		offset = gf_ftell(in) - 4;
		size = gf_mp3_frame_size(hdr);
		assert(size);
		if (size>max_size) {
			samp->data = (char*)gf_realloc(samp->data, sizeof(char) * size);
			max_size = size;
		}
		samp->data[0] = (hdr >> 24) & 0xFF;
		samp->data[1] = (hdr >> 16) & 0xFF;
		samp->data[2] = (hdr >> 8) & 0xFF;
		samp->data[3] = hdr & 0xFF;
		samp->dataLength = size;

		/* read the frame data into the buffer */
		if (fread(&samp->data[4], 1, size - 4, in) != size - 4) break;

		if (import->flags & GF_IMPORT_USE_DATAREF) {
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else {
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		if (e) goto exit;

		gf_set_progress("Importing MP3", done, tot_size);

		samp->DTS += gf_mp3_window_size(hdr);
		done += samp->dataLength;
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}
	gf_media_update_bitrate(import->dest, track);
	gf_set_progress("Importing MP3", tot_size, tot_size);

exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	if (samp) gf_isom_sample_del(&samp);
	gf_fclose(in);
	return e;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	Bool is_mp2, no_crc;
	u32 profile, sr_idx, nb_ch, frame_size;
} ADTSHeader;

static SAPType xHEAAC_isRAP(u32 profile, char *data, u32 dataLength)
{
	if (profile == 42) { /*xHE-AAC*/
		if (dataLength > 0 && (data[0] & 0x80))
			return RAP;
		else
			return RAP_NO;
	}

	return RAP;
}

static Bool ADTS_SyncFrame(GF_BitStream *bs, ADTSHeader *hdr, u32 *frame_skipped)
{
	u32 val, hdr_size;
	u64 pos;
	*frame_skipped = 0;
	while (gf_bs_available(bs)) {
		val = gf_bs_read_u8(bs);
		if (val!=0xFF) continue;
		val = gf_bs_read_int(bs, 4);
		if (val != 0x0F) {
			gf_bs_read_int(bs, 4);
			continue;
		}
		hdr->is_mp2 = (Bool)gf_bs_read_int(bs, 1);
		/*layer*/ gf_bs_read_int(bs, 2);
		hdr->no_crc = (Bool)gf_bs_read_int(bs, 1);
		pos = gf_bs_get_position(bs) - 2;

		hdr->profile = 1 + gf_bs_read_int(bs, 2);
		hdr->sr_idx = gf_bs_read_int(bs, 4);
		/*private_bit*/ gf_bs_read_int(bs, 1);
		hdr->nb_ch = gf_bs_read_int(bs, 3);
		/*original_copy*/ gf_bs_read_int(bs, 1);
		/*home*/ gf_bs_read_int(bs, 1);
		/*copyright_identification_bit*/gf_bs_read_int(bs, 1);
		/*copyright_identification_start*/gf_bs_read_int(bs, 1);
		hdr->frame_size = gf_bs_read_int(bs, 13);
		/*adts_buffer_fullness*/gf_bs_read_int(bs, 11);
		/*number_of_raw_data_blocks_in_frame*/gf_bs_read_int(bs, 2);
		hdr_size = hdr->no_crc ? 7 : 9;
		if (!hdr->no_crc) gf_bs_read_int(bs, 16);
		if (hdr->frame_size < hdr_size) {
			gf_bs_seek(bs, pos+1);
			*frame_skipped += 1;
			continue;
		}
		hdr->frame_size -= hdr_size;
		if (gf_bs_available(bs) == hdr->frame_size) return GF_TRUE;

		gf_bs_skip_bytes(bs, hdr->frame_size);
		val = gf_bs_read_u8(bs);
		if (val!=0xFF) {
			gf_bs_seek(bs, pos+1);
			*frame_skipped += 1;
			continue;
		}
		val = gf_bs_read_int(bs, 4);
		if (val!=0x0F) {
			gf_bs_read_int(bs, 4);
			gf_bs_seek(bs, pos+2);
			*frame_skipped += 1;
			continue;
		}
		gf_bs_seek(bs, pos+hdr_size);
		return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool LOAS_LoadFrame(GF_BitStream *bs, GF_M4ADecSpecInfo *acfg, u32 *nb_bytes, u8 *buffer)
{
	u32 val, size;
	u64 pos, mux_size;
	if (!acfg) return 0;
	memset(acfg, 0, sizeof(GF_M4ADecSpecInfo));
	while (gf_bs_available(bs)) {
		val = gf_bs_read_u8(bs);
		if (val!=0x56) continue;
		val = gf_bs_read_int(bs, 3);
		if (val != 0x07) {
			gf_bs_read_int(bs, 5);
			continue;
		}
		mux_size = gf_bs_read_int(bs, 13);
		pos = gf_bs_get_position(bs);

		/*use same stream mux*/
		if (!gf_bs_read_int(bs, 1)) {
			Bool amux_version, amux_versionA;

			amux_version = (Bool)gf_bs_read_int(bs, 1);
			amux_versionA = GF_FALSE;
			if (amux_version) amux_versionA = (Bool)gf_bs_read_int(bs, 1);
			if (!amux_versionA) {
				u32 i, allStreamsSameTimeFraming, numProgram;
				if (amux_version) gf_latm_get_value(bs);

				allStreamsSameTimeFraming = gf_bs_read_int(bs, 1);
				/*numSubFrames = */gf_bs_read_int(bs, 6);
				numProgram = gf_bs_read_int(bs, 4);
				for (i=0; i<=numProgram; i++) {
					u32 j, num_lay;
					num_lay = gf_bs_read_int(bs, 3);
					for (j=0; j<=num_lay; j++) {
						u32 frameLengthType;
						Bool same_cfg = GF_FALSE;
						if (i || j) same_cfg = (Bool)gf_bs_read_int(bs, 1);

						if (!same_cfg) {
							if (amux_version==1) gf_latm_get_value(bs);
							gf_m4a_parse_config(bs, acfg, GF_FALSE);
						}
						frameLengthType = gf_bs_read_int(bs, 3);
						if (!frameLengthType) {
							/*latmBufferFullness = */gf_bs_read_int(bs, 8);
							if (!allStreamsSameTimeFraming) {
							}
						} else {
							/*not supported*/
						}
					}

				}
				/*other data present*/
				if (gf_bs_read_int(bs, 1)) {
//					u32 k = 0;
				}
				/*CRCcheck present*/
				if (gf_bs_read_int(bs, 1)) {
				}
			}
		}

		size = 0;
		while (1) {
			u32 tmp = gf_bs_read_int(bs, 8);
			size += tmp;
			if (tmp!=255) break;
		}
		if (nb_bytes && buffer) {
			*nb_bytes = (u32) size;
			gf_bs_read_data(bs, (char *) buffer, size);
		} else {
			gf_bs_skip_bytes(bs, size);
		}

		/*parse amux*/
		gf_bs_seek(bs, pos + mux_size);

		if (gf_bs_peek_bits(bs, 11, 0) != 0x2B7) {
			gf_bs_seek(bs, pos + 1);
			continue;
		}

		return GF_TRUE;
	}
	return GF_FALSE;
}

GF_Err gf_import_aac_loas(GF_MediaImporter *import)
{
	u8 oti;
	Bool destroy_esd;
	GF_Err e;
	Bool sync_frame;
	u16 sr, dts_inc;
	u32 timescale;
	GF_BitStream *bs, *dsi;
	GF_M4ADecSpecInfo acfg;
	u32 base_object_type = 0;
	FILE *in;
	u32 nbbytes=0;
	u8 aac_buf[4096];
	u64 tot_size, done, duration;
	u32 track, di;
	GF_ISOSample *samp;

	in = gf_fopen(import->in_name, "rb");
	if (!in) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	bs = gf_bs_from_file(in, GF_BITSTREAM_READ);

	/*sync_frame = */LOAS_LoadFrame(bs, &acfg, &nbbytes, (u8 *)aac_buf);

	/*keep MPEG-2 AAC OTI even for HE-SBR (that's correct according to latest MPEG-4 audio spec)*/
	oti = GPAC_OTI_AUDIO_AAC_MPEG4;
	timescale = sr = acfg.base_sr;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_AUDIO;
		import->tk_info[0].flags = GF_IMPORT_SBR_IMPLICIT | GF_IMPORT_SBR_EXPLICIT | GF_IMPORT_PS_IMPLICIT | GF_IMPORT_PS_EXPLICIT | GF_IMPORT_FORCE_MPEG4;
		import->nb_tracks = 1;
		import->tk_info[0].audio_info.sample_rate = sr;
		import->tk_info[0].audio_info.nb_channels = acfg.nb_chan;
		gf_bs_del(bs);
		gf_fclose(in);
		return GF_OK;
	}

	dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_m4a_write_config_bs(dsi, &acfg);
	base_object_type = acfg.base_object_type;

	if (import->flags & GF_IMPORT_PS_EXPLICIT) {
		import->flags &= ~GF_IMPORT_PS_IMPLICIT;
		import->flags |= GF_IMPORT_SBR_EXPLICIT;
		import->flags &= ~GF_IMPORT_SBR_IMPLICIT;
	}

	dts_inc = 1024;

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(2);
		destroy_esd = GF_TRUE;
	}
	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	import->esd->decoderConfig->objectTypeIndication = oti;
	import->esd->decoderConfig->bufferSizeDB = 20;
	import->esd->slConfig->timestampResolution = timescale;
	if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	if (import->esd->decoderConfig->decoderSpecificInfo->data) gf_free(import->esd->decoderConfig->decoderSpecificInfo->data);
	gf_bs_get_content(dsi, &import->esd->decoderConfig->decoderSpecificInfo->data, &import->esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(dsi);

	samp = NULL;
	gf_import_message(import, GF_OK, "MPEG-4 AAC in LOAS import - sample rate %d - %d channel%s", sr, acfg.nb_chan, (acfg.nb_chan > 1) ? "s" : "");

	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;
	gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	gf_isom_set_audio_info(import->dest, track, di, timescale, acfg.nb_chan, 16, import->asemode);

	/*add first sample*/
	samp = gf_isom_sample_new();
	samp->dataLength = nbbytes;
	samp->data = (char *) aac_buf;
	samp->IsRAP = xHEAAC_isRAP(acfg.base_object_type, samp->data, samp->dataLength);

	e = gf_isom_add_sample(import->dest, track, di, samp);
	if (e) goto exit;
	samp->DTS+=dts_inc;

	duration = import->duration;
	duration *= sr;
	duration /= 1000;

	tot_size = gf_bs_get_size(bs);
	done = 0;
	while (gf_bs_available(bs) ) {
		sync_frame = LOAS_LoadFrame(bs, &acfg, &nbbytes, (u8 *)aac_buf);
		if (!sync_frame) break;

		samp->data = (char*)aac_buf;
		samp->dataLength = nbbytes;
		samp->IsRAP = xHEAAC_isRAP(base_object_type, samp->data, samp->dataLength);

		e = gf_isom_add_sample(import->dest, track, di, samp);
		if (e) break;

		gf_set_progress("Importing AAC", done, tot_size);
		samp->DTS += dts_inc;
		done += samp->dataLength;
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}
	gf_media_update_bitrate(import->dest, track);
	gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_AUDIO, acfg.audioPL);
	gf_set_progress("Importing AAC", tot_size, tot_size);

exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	if (samp) {
		samp->data = NULL;
		gf_isom_sample_del(&samp);
	}
	gf_bs_del(bs);
	gf_fclose(in);
	return e;
}

GF_Err gf_import_aac_adts(GF_MediaImporter *import)
{
	u8 oti;
	Bool destroy_esd;
	GF_Err e;
	Bool sync_frame;
	u16 sr, sbr_sr, sbr_sr_idx, dts_inc;
	u32 timescale;
	u32 frames_skipped = 0;
	GF_BitStream *bs, *dsi;
	ADTSHeader hdr;
	GF_M4ADecSpecInfo acfg;
	FILE *in;
	u64 offset, tot_size, done, duration;
	u32 max_size, track, di, i;
	GF_ISOSample *samp;
	u32 cur_samp = 0;

	in = gf_fopen(import->in_name, "rb");
	if (!in) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	bs = gf_bs_from_file(in, GF_BITSTREAM_READ);

	sync_frame = ADTS_SyncFrame(bs, &hdr, &frames_skipped);
	if (!sync_frame) {
		gf_bs_del(bs);
		gf_fclose(in);
		return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Audio isn't MPEG-2/4 AAC with ADTS");
	}

	if (frames_skipped) {
		gf_bs_seek(bs, 0);
		sync_frame = LOAS_LoadFrame(bs, &acfg, NULL, NULL);
		if (sync_frame) {
			gf_bs_del(bs);
			gf_fclose(in);
			return gf_import_aac_loas(import);
		}
	}

	if (import->flags & GF_IMPORT_FORCE_MPEG4) hdr.is_mp2 = GF_FALSE;

	/*keep MPEG-2 AAC OTI even for HE-SBR (that's correct according to latest MPEG-4 audio spec)*/
	oti = hdr.is_mp2 ? hdr.profile+GPAC_OTI_AUDIO_AAC_MPEG2_MP-1 : GPAC_OTI_AUDIO_AAC_MPEG4;
	sr = GF_M4ASampleRates[hdr.sr_idx];

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_AUDIO;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_SBR_IMPLICIT | GF_IMPORT_SBR_EXPLICIT | GF_IMPORT_PS_IMPLICIT | GF_IMPORT_PS_EXPLICIT | GF_IMPORT_FORCE_MPEG4;
		import->nb_tracks = 1;
		import->tk_info[0].audio_info.sample_rate = sr;
		import->tk_info[0].audio_info.nb_channels = hdr.nb_ch;
		gf_bs_del(bs);
		gf_fclose(in);
		return GF_OK;
	}

	dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	sbr_sr = sr;
	sbr_sr_idx = hdr.sr_idx;
	if ((import->flags & GF_IMPORT_OVSBR) == 0) {
		for (i=0; i<16; i++) {
			if (GF_M4ASampleRates[i] == (u32) 2*sr) {
				sbr_sr_idx = i;
				sbr_sr = 2*sr;
				break;
			}
		}
	}

	if (import->flags & GF_IMPORT_PS_EXPLICIT) {
		import->flags &= ~GF_IMPORT_PS_IMPLICIT;
		import->flags |= GF_IMPORT_SBR_EXPLICIT;
		import->flags &= ~GF_IMPORT_SBR_IMPLICIT;
	}

	/*no provision for explicit indication of MPEG-2 AAC through MPEG-4 PLs, so force implicit*/
	if (hdr.is_mp2) {
		if (import->flags & GF_IMPORT_SBR_EXPLICIT) {
			import->flags &= ~GF_IMPORT_SBR_EXPLICIT;
			import->flags |= GF_IMPORT_SBR_IMPLICIT;
		}
		if (import->flags & GF_IMPORT_PS_EXPLICIT) {
			import->flags &= ~GF_IMPORT_PS_EXPLICIT;
			import->flags |= GF_IMPORT_PS_IMPLICIT;
		}
	}

	dts_inc = 1024;

	memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
	acfg.base_object_type = hdr.profile;
	acfg.base_sr = sr;
	acfg.nb_chan = gf_m4a_get_channel_cfg(hdr.nb_ch);
	acfg.sbr_object_type = 0;
	if (import->flags & GF_IMPORT_SBR_EXPLICIT) {
		acfg.has_sbr = GF_TRUE;
		acfg.base_object_type = 5;
		acfg.sbr_object_type = hdr.profile;

		/*for better interop, always store using full SR when using explict signaling*/
		dts_inc = 2048;
		sr = sbr_sr;
	} else if (import->flags & GF_IMPORT_SBR_IMPLICIT) {
		acfg.has_sbr = GF_TRUE;
	}
	if (import->flags & GF_IMPORT_PS_EXPLICIT) {
		acfg.has_ps = GF_TRUE;
		acfg.base_object_type = 29;
	} else if (import->flags & GF_IMPORT_PS_IMPLICIT) {
		acfg.has_ps = GF_TRUE;
	}

	acfg.audioPL = gf_m4a_get_profile(&acfg);
	/*explicit SBR or PS signal (non backward-compatible)*/
	if (import->flags & GF_IMPORT_PS_EXPLICIT) {
		gf_bs_write_int(dsi, 29, 5);
		gf_bs_write_int(dsi, hdr.sr_idx, 4);
		gf_bs_write_int(dsi, hdr.nb_ch, 4);
		gf_bs_write_int(dsi, sbr_sr ? sbr_sr_idx : hdr.sr_idx, 4);
		gf_bs_write_int(dsi, hdr.profile, 5);
	}
	/*explicit SBR signal (non backward-compatible)*/
	else if (import->flags & GF_IMPORT_SBR_EXPLICIT) {
		gf_bs_write_int(dsi, 5, 5);
		gf_bs_write_int(dsi, hdr.sr_idx, 4);
		gf_bs_write_int(dsi, hdr.nb_ch, 4);
		gf_bs_write_int(dsi, sbr_sr ? sbr_sr_idx : hdr.sr_idx, 4);
		gf_bs_write_int(dsi, hdr.profile, 5);
	} else {
		/*regular AAC*/
		gf_bs_write_int(dsi, hdr.profile, 5);
		gf_bs_write_int(dsi, hdr.sr_idx, 4);
		gf_bs_write_int(dsi, hdr.nb_ch, 4);
		gf_bs_align(dsi);
		/*implicit AAC SBR signal*/
		if (import->flags & GF_IMPORT_SBR_IMPLICIT) {
			gf_bs_write_int(dsi, 0x2b7, 11); /*sync extension type*/
			gf_bs_write_int(dsi, 5, 5);	/*SBR objectType*/
			gf_bs_write_int(dsi, 1, 1);	/*SBR present flag*/
			gf_bs_write_int(dsi, sbr_sr_idx, 4);
		}
		if (import->flags & GF_IMPORT_PS_IMPLICIT) {
			gf_bs_write_int(dsi, 0x548, 11); /*sync extension type*/
			gf_bs_write_int(dsi, 1, 1);	/* PS present flag */
		}
	}
	/*not MPEG4 tool*/
	if (0 && hdr.is_mp2) acfg.audioPL = 0xFE;
	gf_bs_align(dsi);

	timescale = sr;
	if (import->flags & GF_IMPORT_OVSBR)
		timescale = 2*sr;

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(2);
		destroy_esd = GF_TRUE;
	}
	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	import->esd->decoderConfig->objectTypeIndication = oti;
	import->esd->decoderConfig->bufferSizeDB = 20;
	import->esd->slConfig->timestampResolution = timescale;
	if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	if (import->esd->decoderConfig->decoderSpecificInfo->data) gf_free(import->esd->decoderConfig->decoderSpecificInfo->data);
	gf_bs_get_content(dsi, &import->esd->decoderConfig->decoderSpecificInfo->data, &import->esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(dsi);

	samp = NULL;
	gf_import_message(import, GF_OK, "AAC ADTS import %s%s%s- sample rate %d - %s audio - %d channel%s",
	                  (import->flags & (GF_IMPORT_SBR_IMPLICIT|GF_IMPORT_SBR_EXPLICIT)) ? "SBR" : "",
	                  (import->flags & (GF_IMPORT_PS_IMPLICIT|GF_IMPORT_PS_EXPLICIT)) ? "+PS" : "",
	                  ((import->flags & (GF_IMPORT_SBR_EXPLICIT|GF_IMPORT_PS_EXPLICIT)) ? " (explicit) " : " "),
	                  sr,
	                  (oti==GPAC_OTI_AUDIO_AAC_MPEG4) ? "MPEG-4" : "MPEG-2",
	                  hdr.nb_ch,
	                  (hdr.nb_ch>1) ? "s" : "");

	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;
	gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	gf_isom_set_audio_info(import->dest, track, di, timescale, hdr.nb_ch, 16, import->asemode);

	/*add first sample*/
	samp = gf_isom_sample_new();
	max_size = samp->dataLength = hdr.frame_size;
	samp->data = (char*)gf_malloc(sizeof(char)*hdr.frame_size);
	offset = gf_bs_get_position(bs);
	gf_bs_read_data(bs, samp->data, hdr.frame_size);
	samp->IsRAP = RAP;

	if (import->flags & GF_IMPORT_USE_DATAREF) {
		e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
	} else {
		e = gf_isom_add_sample(import->dest, track, di, samp);
	}
	if (e) goto exit;
	samp->DTS+=dts_inc;

	cur_samp++;
	if (import->audio_roll_change) {
		e = gf_isom_set_sample_roll_group(import->dest, track, cur_samp, import->audio_roll);
	}

	duration = import->duration;
	duration *= sr;
	duration /= 1000;

	tot_size = gf_bs_get_size(bs);
	done = 0;
	while (gf_bs_available(bs) ) {
		sync_frame = ADTS_SyncFrame(bs, &hdr, &frames_skipped);
		if (!sync_frame) break;
		if (hdr.frame_size>max_size) {
			samp->data = (char*)gf_realloc(samp->data, sizeof(char) * hdr.frame_size);
			max_size = hdr.frame_size;
		}
		samp->dataLength = hdr.frame_size;

		offset = gf_bs_get_position(bs);
		gf_bs_read_data(bs, samp->data, hdr.frame_size);
		if (import->flags & GF_IMPORT_USE_DATAREF) {
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else {
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		if (e) break;

		cur_samp++;
		if (import->audio_roll_change) {
			e = gf_isom_set_sample_roll_group(import->dest, track, cur_samp, import->audio_roll);
		}
		cur_samp++;

		gf_set_progress("Importing AAC", done, tot_size);
		samp->DTS += dts_inc;
		done += samp->dataLength;
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}
	gf_media_update_bitrate(import->dest, track);
	gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_AUDIO, acfg.audioPL);
	gf_set_progress("Importing AAC", tot_size, tot_size);

exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	if (samp) gf_isom_sample_del(&samp);
	gf_bs_del(bs);
	gf_fclose(in);
	return e;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/



static void update_edit_list_for_bframes(GF_ISOFile *file, u32 track)
{
	u32 i, count, di;
	u64 max_cts, min_cts, doff;

	count = gf_isom_get_sample_count(file, track);
	max_cts = 0;
	min_cts = (u64) -1;
	for (i=0; i<count; i++) {
		GF_ISOSample *s = gf_isom_get_sample_info(file, track, i+1, &di, &doff);
		if (s->DTS + s->CTS_Offset > max_cts)
			max_cts = s->DTS + s->CTS_Offset;

		if (min_cts > s->DTS + s->CTS_Offset)
			min_cts = s->DTS + s->CTS_Offset;

		gf_isom_sample_del(&s);
	}

	if (min_cts) {
		max_cts -= min_cts;
		max_cts += gf_isom_get_sample_duration(file, track, count);

		max_cts *= gf_isom_get_timescale(file);
		max_cts /= gf_isom_get_media_timescale(file, track);
		gf_isom_set_edit_segment(file, track, 0, max_cts, min_cts, GF_ISOM_EDIT_NORMAL);
	}
}


#ifndef GPAC_DISABLE_AV_PARSERS

static GF_Err gf_import_cmp(GF_MediaImporter *import, Bool mpeg12)
{
	GF_Err e;
	Double FPS;
	FILE *mdia;
	GF_ISOSample *samp;
	Bool is_vfr, erase_pl, has_cts_offset, is_packed, destroy_esd, do_vfr, forced_packed;
	u32 nb_samp, i, timescale, max_size, track, di, PL, max_b, nbI, nbP, nbB, nbNotCoded, dts_inc, ref_frame, b_frames;
	u64 pos, tot_size, done_size, samp_offset, duration;
	GF_M4VDecSpecInfo dsi;
	GF_M4VParser *vparse;
	GF_BitStream *bs;

	destroy_esd = forced_packed = GF_FALSE;
	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Opening %s failed", import->in_name);
	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);

	samp = NULL;
	vparse = gf_m4v_parser_bs_new(bs, mpeg12);
	e = gf_m4v_parse_config(vparse, &dsi);
	if (e) {
		gf_import_message(import, e, "Cannot load MPEG-4 decoder config");
		goto exit;
	}

	tot_size = gf_bs_get_size(bs);
	done_size = 0;
	destroy_esd = GF_FALSE;
	FPS = mpeg12 ? dsi.fps : GF_IMPORT_DEFAULT_FPS;
	if (import->video_fps) FPS = (Double) import->video_fps;
	get_video_timing(FPS, &timescale, &dts_inc);

	duration = (u64) (import->duration*FPS);

	is_packed = GF_FALSE;
	nbNotCoded = nbI = nbP = nbB = max_b = 0;
	is_vfr = erase_pl = GF_FALSE;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].media_type = mpeg12 ? ((dsi.VideoPL==GPAC_OTI_VIDEO_MPEG1) ? GF_MEDIA_TYPE_MPG1 : GF_MEDIA_TYPE_MPG2 ) : GF_MEDIA_TYPE_MP4V ;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_OVERRIDE_FPS;
		if (!mpeg12) import->tk_info[0].flags |= GF_IMPORT_NO_FRAME_DROP | GF_IMPORT_FORCE_PACKED;
		import->tk_info[0].video_info.width = dsi.width;
		import->tk_info[0].video_info.height = dsi.height;
		import->tk_info[0].video_info.par = (dsi.par_num<<16) | dsi.par_den;
		import->nb_tracks = 1;
		goto exit;
	}

	samp = gf_isom_sample_new();
	max_size = 4096;
	samp->data = (char*)gf_malloc(sizeof(char)*max_size);
	/*no auto frame-rate detection*/
	if (import->video_fps == GF_IMPORT_AUTO_FPS)
		import->video_fps = GF_IMPORT_DEFAULT_FPS;

	PL = dsi.VideoPL;
	if (!PL) {
		PL = 0x01;
		erase_pl = GF_TRUE;
	}
	samp_offset = 0;
	/*MPEG-4 visual*/
	if (!mpeg12) samp_offset = gf_m4v_get_object_start(vparse);
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(0);
		destroy_esd = GF_TRUE;
	}
	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_VISUAL, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);

	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->slConfig->timestampResolution = timescale;
	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
	import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	import->esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	if (mpeg12) {
		import->esd->decoderConfig->objectTypeIndication = dsi.VideoPL;
	} else {
		import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_MPEG4_PART2;
	}
	if (samp_offset) {
		import->esd->decoderConfig->decoderSpecificInfo->data = (char*)gf_malloc(sizeof(char) * (size_t)samp_offset);
		assert(samp_offset < 1<<31);
		import->esd->decoderConfig->decoderSpecificInfo->dataLength = (u32) samp_offset;
		pos = gf_bs_get_position(bs);
		gf_bs_seek(bs, 0);
		assert(samp_offset < 1<<31);
		gf_bs_read_data(bs, import->esd->decoderConfig->decoderSpecificInfo->data, (u32)samp_offset);
		gf_bs_seek(bs, pos);

		/*remove packed flag if any (VOSH user data)*/
		forced_packed = GF_FALSE;
		i=0;
		while (1) {
			char *frame = import->esd->decoderConfig->decoderSpecificInfo->data;
			while ((i+3<samp_offset)  && ((frame[i]!=0) || (frame[i+1]!=0) || (frame[i+2]!=1))) i++;
			if (i+4>=samp_offset) break;
			if (strncmp(frame+i+4, "DivX", 4)) {
				i += 4;
				continue;
			}
			frame = import->esd->decoderConfig->decoderSpecificInfo->data + i + 4;
			frame = strchr(frame, 'p');
			if (frame) {
				forced_packed = GF_TRUE;
				frame[0] = 'n';
			}
			break;
		}
	}

	if (import->flags & GF_IMPORT_FORCE_PACKED) forced_packed = GF_TRUE;

	gf_isom_set_cts_packing(import->dest, track, GF_TRUE);

	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name: NULL, NULL, &di);
	if (e) goto exit;
	gf_isom_set_visual_info(import->dest, track, di, dsi.width, dsi.height);
	if (mpeg12) {
		gf_import_message(import, GF_OK, "MPEG-%d Video import - %d x %d @ %02.4f FPS", (dsi.VideoPL==GPAC_OTI_VIDEO_MPEG1) ? 1 : 2, dsi.width, dsi.height, FPS);
	} else {
		gf_import_message(import, GF_OK, "MPEG-4 Video import - %d x %d @ %02.4f FPS\nIndicated Profile: %s", dsi.width, dsi.height, FPS, gf_m4v_get_profile_name((u8) PL));
	}

	gf_media_update_par(import->dest, track);

	has_cts_offset = GF_FALSE;
	nb_samp = b_frames = ref_frame = 0;
	do_vfr = !(import->flags & GF_IMPORT_NO_FRAME_DROP);

	while (gf_bs_available(bs)) {
		u8 ftype;
		u32 tinc;
		u64 frame_start, sample_size;
		Bool is_coded;
		/*pos = */gf_m4v_get_object_start(vparse);
		e = gf_m4v_parse_frame(vparse, dsi, &ftype, &tinc, &sample_size, &frame_start, &is_coded);
		assert(sample_size < 1<<31);
		samp->dataLength = (u32) sample_size;
		if (e==GF_EOS) e = GF_OK;
		if (e) goto exit;

		if (!is_coded) {
			nbNotCoded ++;
			/*if prev is B and we're parsing a packed bitstream discard n-vop*/
			if (forced_packed && b_frames) {
				is_packed = GF_TRUE;
				continue;
			}
			/*policy is to import at variable frame rate, skip*/
			if (do_vfr) {
				is_vfr = GF_TRUE;
				samp->DTS += dts_inc;
				continue;
			}
			/*policy is to keep non coded frame (constant frame rate), add*/
		}
		samp->IsRAP = RAP_NO;

		if (ftype==2) {
			b_frames++;
			nbB++;
			/*adjust CTS*/
			if (!has_cts_offset) {
				u32 i;
				for (i=0; i<gf_isom_get_sample_count(import->dest, track); i++) {
					gf_isom_modify_cts_offset(import->dest, track, i+1, dts_inc);
				}
				has_cts_offset = GF_TRUE;
			}
		} else {
			if (ftype==0) {
				samp->IsRAP = RAP;
				nbI++;
			} else {
				nbP++;
			}
			/*even if no out-of-order frames we MUST adjust CTS if cts_offset is present is */
			if (ref_frame && has_cts_offset)
				gf_isom_modify_cts_offset(import->dest, track, ref_frame, (1+b_frames)*dts_inc);

			ref_frame = nb_samp+1;
			if (max_b<b_frames) max_b = b_frames;
			b_frames = 0;
		}
		if (!nb_samp) samp->DTS = 0;

		if (import->flags & GF_IMPORT_USE_DATAREF) {
			samp->data = NULL;
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, frame_start);
		} else {
			if (samp->dataLength>max_size) {
				max_size = samp->dataLength;
				samp->data = (char*)gf_realloc(samp->data, sizeof(char)*max_size);
			}
			gf_bs_seek(bs, frame_start);
			gf_bs_read_data(bs, samp->data, samp->dataLength);
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		samp->DTS += dts_inc;
		nb_samp++;
		gf_set_progress("Importing M4V", done_size/1024, tot_size/1024);
		done_size += samp->dataLength;
		if (e) break;
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;

	}
	/*final flush*/
	if (ref_frame && has_cts_offset)
		gf_isom_modify_cts_offset(import->dest, track, ref_frame, (1+b_frames)*dts_inc);

	gf_set_progress("Importing M4V", nb_samp, nb_samp);
	if (has_cts_offset) {
		gf_import_message(import, GF_OK, "Has B-Frames (%d max consecutive B-VOPs)", max_b);
		gf_isom_set_cts_packing(import->dest, track, GF_FALSE);


		if (!(import->flags & GF_IMPORT_NO_EDIT_LIST))
			update_edit_list_for_bframes(import->dest, track);

		/*this is plain ugly but since some encoders (divx) don't use the video PL correctly
		we force the system video_pl to ASP@L5 since we have I, P, B in base layer*/
		if (PL<=3) {
			PL = 0xF5;
			erase_pl = GF_TRUE;
			gf_import_message(import, GF_OK, "WARNING: indicated profile doesn't include B-VOPs - forcing %s", gf_m4v_get_profile_name((u8) PL));
		}
		gf_import_message(import, GF_OK, "Import results: %d VOPs (%d Is - %d Ps - %d Bs)", nb_samp, nbI, nbP, nbB);
	} else {
		/*no B-frames, remove CTS offsets*/
		gf_isom_remove_cts_info(import->dest, track);
		gf_import_message(import, GF_OK, "Import results: %d VOPs (%d Is - %d Ps)", nb_samp, nbI, nbP);
	}
	if (erase_pl) {
		gf_m4v_rewrite_pl(&import->esd->decoderConfig->decoderSpecificInfo->data, &import->esd->decoderConfig->decoderSpecificInfo->dataLength, (u8) PL);
		gf_isom_change_mpeg4_description(import->dest, track, 1, import->esd);
	}
	gf_media_update_bitrate(import->dest, track);

	if (is_vfr) {
		if (!nbB) {
			if (do_vfr) {
				gf_import_message(import, GF_OK, "import using Variable Frame Rate - Removed %d N-VOPs", nbNotCoded);
			} else {
				if (nbNotCoded) gf_import_message(import, GF_OK, "Stream has %d N-VOPs", nbNotCoded);
			}
			nbNotCoded = 0;
		}
	}

	if (nbNotCoded) gf_import_message(import, GF_OK, "Removed %d N-VOPs%s", nbNotCoded,is_packed ? " (Packed Bitstream)" : "");
	gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_VISUAL, (u8) PL);

	if (dsi.par_den && dsi.par_num)
		gf_media_change_par(import->dest, track, dsi.par_num, dsi.par_den, GF_FALSE, GF_FALSE);

exit:
	if (samp) gf_isom_sample_del(&samp);
	if (destroy_esd) gf_odf_desc_del((GF_Descriptor *) import->esd);

	/*this destroys the bitstream as well*/
	gf_m4v_parser_del(vparse);
	gf_fclose(mdia);
	return e;
}

#endif /*GPAC_DISABLE_AV_PARSERS*/


#ifndef GPAC_DISABLE_AVILIB

static GF_Err gf_import_avi_video(GF_MediaImporter *import)
{
	GF_Err e;
	Double FPS;
	FILE *test;
	GF_ISOSample *samp;
	u32 i, num_samples, timescale, track, di, PL, max_b, nb_f, ref_frame, b_frames;
	u64 samp_offset, size, max_size;
	u32 nbI, nbP, nbB, nbDummy, nbNotCoded, dts_inc, cur_samp;
	Bool is_vfr, erase_pl;
	GF_M4VDecSpecInfo dsi;
	GF_M4VParser *vparse;
	s32 key;
	u64 duration;
	Bool destroy_esd, is_packed, is_init, has_cts_offset;
	char *comp, *frame;
	avi_t *in;

	test = gf_fopen(import->in_name, "rb");
	if (!test) return gf_import_message(import, GF_URL_ERROR, "Opening %s failed", import->in_name);
	gf_fclose(test);
	in = AVI_open_input_file(import->in_name, 1);
	if (!in) return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Unsupported avi file");

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		char *comp;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_NO_FRAME_DROP | GF_IMPORT_OVERRIDE_FPS;
		import->tk_info[0].video_info.FPS = AVI_frame_rate(in);
		import->tk_info[0].video_info.width = AVI_video_width(in);
		import->tk_info[0].video_info.height = AVI_video_height(in);
		comp = AVI_video_compressor(in);
		import->tk_info[0].media_type = GF_4CC((u8)comp[0], (u8)comp[1], (u8)comp[2], (u8)comp[3]);

		import->nb_tracks = 1;
		for (i=0; i<(u32) AVI_audio_tracks(in); i++) {
			import->tk_info[i+1].track_num = i+2;
			import->tk_info[i+1].type = GF_ISOM_MEDIA_AUDIO;
			import->tk_info[i+1].flags = GF_IMPORT_USE_DATAREF;
			import->tk_info[i+1].audio_info.sample_rate = (u32) AVI_audio_rate(in);
			import->tk_info[i+1].audio_info.nb_channels = (u32) AVI_audio_channels(in);
			import->nb_tracks ++;
		}
		AVI_close(in);
		return GF_OK;
	}
	if (import->trackID>1) {
		AVI_close(in);
		return GF_OK;
	}
	destroy_esd = GF_FALSE;
	frame = NULL;
	AVI_seek_start(in);

	erase_pl = GF_FALSE;
	comp = AVI_video_compressor(in);
	if (!comp) {
		e = GF_NOT_SUPPORTED;
		goto exit;
	}

	/*these are/should be OK*/
	if (!stricmp(comp, "DIVX") || !stricmp(comp, "DX50")	/*DivX*/
	        || !stricmp(comp, "XVID") /*XviD*/
	        || !stricmp(comp, "3iv2") /*3ivX*/
	        || !stricmp(comp, "fvfw") /*ffmpeg*/
	        || !stricmp(comp, "NDIG") /*nero*/
	        || !stricmp(comp, "MP4V") /*!! not tested*/
	        || !stricmp(comp, "M4CC") /*Divio - not tested*/
	        || !stricmp(comp, "PVMM") /*PacketVideo - not tested*/
	        || !stricmp(comp, "SEDG") /*Samsung - not tested*/
	        || !stricmp(comp, "RMP4") /*Sigma - not tested*/
	        || !stricmp(comp, "MP43") /*not tested*/
	        || !stricmp(comp, "FMP4") /*not tested*/
	   ) {

	}
	else if (!stricmp(comp, "DIV3") || !stricmp(comp, "DIV4")) {
		gf_import_message(import, GF_NOT_SUPPORTED, "Video format %s not compliant with MPEG-4 Visual - please recompress the file first", comp);
		e = GF_NOT_SUPPORTED;
		goto exit;
	} else if (!stricmp(comp, "H264") || !stricmp(comp, "X264")) {
		gf_import_message(import, GF_NOT_SUPPORTED, "H264/AVC Video format not supported in AVI - please extract to raw format first", comp);
		e = GF_NOT_SUPPORTED;
		goto exit;
	} else {
		gf_import_message(import, GF_NOT_SUPPORTED, "Video format %s not supported - recompress the file first", comp);
		e = GF_NOT_SUPPORTED;
		goto exit;
	}
	/*no auto frame-rate detection*/
	if (import->video_fps == GF_IMPORT_AUTO_FPS)
		import->video_fps = GF_IMPORT_DEFAULT_FPS;

	FPS = AVI_frame_rate(in);
	if (import->video_fps) FPS = (Double) import->video_fps;
	get_video_timing(FPS, &timescale, &dts_inc);
	duration = (u64) (import->duration*FPS);

	e = GF_OK;
	max_size = 0;
	samp_offset = 0;
	frame = NULL;
	num_samples = (u32) AVI_video_frames(in);
	samp = gf_isom_sample_new();
	PL = 0;
	track = 0;
	is_vfr = GF_FALSE;

	is_packed = GF_FALSE;
	nbDummy = nbNotCoded = nbI = nbP = nbB = max_b = 0;
	has_cts_offset = GF_FALSE;
	cur_samp = b_frames = ref_frame = 0;

	is_init = GF_FALSE;

	for (i=0; i<num_samples; i++) {
		size = AVI_frame_size(in, i);
		if (!size) {
			AVI_read_frame(in, NULL, &key);
			continue;
		}

		if (size > max_size) {
			frame = (char*)gf_realloc(frame, sizeof(char) * (size_t)size);
			max_size = size;
		}
		AVI_read_frame(in, frame, &key);

		/*get DSI*/
		if (!is_init) {
			is_init = GF_TRUE;
			vparse = gf_m4v_parser_new(frame, size, GF_FALSE);
			e = gf_m4v_parse_config(vparse, &dsi);
			PL = dsi.VideoPL;
			if (!PL) {
				PL = 0x01;
				erase_pl = GF_TRUE;
			}
			samp_offset = gf_m4v_get_object_start(vparse);
			assert(samp_offset < 1<<31);
			gf_m4v_parser_del(vparse);
			if (e) {
				gf_import_message(import, e, "Cannot import decoder config in first frame");
				goto exit;
			}

			if (!import->esd) {
				import->esd = gf_odf_desc_esd_new(0);
				destroy_esd = GF_TRUE;
			}
			track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_VISUAL, timescale);
			if (!track) {
				e = gf_isom_last_error(import->dest);
				goto exit;
			}
			gf_isom_set_track_enabled(import->dest, track, 1);
			if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
			import->final_trackID = gf_isom_get_track_id(import->dest, track);

			if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
			import->esd->slConfig->timestampResolution = timescale;

			if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
			if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
			import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
			import->esd->decoderConfig->streamType = GF_STREAM_VISUAL;
			import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_MPEG4_PART2;
			import->esd->decoderConfig->decoderSpecificInfo->data = (char *) gf_malloc(sizeof(char) * (size_t)samp_offset);
			memcpy(import->esd->decoderConfig->decoderSpecificInfo->data, frame, sizeof(char) * (size_t)samp_offset);
			import->esd->decoderConfig->decoderSpecificInfo->dataLength = (u32) samp_offset;

			gf_isom_set_cts_packing(import->dest, track, GF_TRUE);

			/*remove packed flag if any (VOSH user data)*/
			while (1) {
				char *divx_mark;
				while ((i+3<samp_offset)  && ((frame[i]!=0) || (frame[i+1]!=0) || (frame[i+2]!=1))) i++;
				if (i+4>=samp_offset) break;

				if (strncmp(frame+i+4, "DivX", 4)) {
					i += 4;
					continue;
				}
				divx_mark = import->esd->decoderConfig->decoderSpecificInfo->data + i + 4;
				divx_mark = strchr(divx_mark, 'p');
				if (divx_mark) divx_mark[0] = 'n';
				break;
			}
			i = 0;

			e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name: NULL, NULL, &di);
			if (e) goto exit;
			gf_isom_set_visual_info(import->dest, track, di, dsi.width, dsi.height);
			gf_import_message(import, GF_OK, "AVI %s video import - %d x %d @ %02.4f FPS - %d Frames\nIndicated Profile: %s", comp, dsi.width, dsi.height, FPS, num_samples, gf_m4v_get_profile_name((u8) PL));

			gf_media_update_par(import->dest, track);
		}


		if (size > samp_offset) {
			u8 ftype;
			u32 tinc;
			u64 framesize, frame_start;
			u64 file_offset;
			Bool is_coded;

			size -= samp_offset;
			file_offset = (u64) AVI_get_video_position(in, i);

			vparse = gf_m4v_parser_new(frame + samp_offset, size, GF_FALSE);

			samp->dataLength = 0;
			/*removing padding frames*/
			if (size<4) {
				nbDummy ++;
				size = 0;
			}

			nb_f=0;
			while (size) {
				GF_Err e = gf_m4v_parse_frame(vparse, dsi, &ftype, &tinc, &framesize, &frame_start, &is_coded);
				if (e<0) goto exit;

				if (!is_coded) {
					if (!gf_m4v_is_valid_object_type(vparse)) gf_import_message(import, GF_OK, "WARNING: AVI frame %d doesn't look like MPEG-4 Visual", i+1);
					nbNotCoded ++;
					if (!is_packed) {
						is_vfr = GF_TRUE;
						/*policy is to import at constant frame rate from AVI*/
						if (import->flags & GF_IMPORT_NO_FRAME_DROP) goto proceed;
						/*policy is to import at variable frame rate from AVI*/
						samp->DTS += dts_inc;
					}
					/*assume this is packed bitstream n-vop and discard it.*/
				} else {
proceed:
					if (e==GF_EOS) size = 0;
					else is_packed = GF_TRUE;
					nb_f++;

					samp->IsRAP = RAP_NO;

					if (ftype==2) {
						b_frames ++;
						nbB++;
						/*adjust CTS*/
						if (!has_cts_offset) {
							u32 i;
							for (i=0; i<gf_isom_get_sample_count(import->dest, track); i++) {
								gf_isom_modify_cts_offset(import->dest, track, i+1, dts_inc);
							}
							has_cts_offset = GF_TRUE;
						}
					} else {
						if (!ftype) {
							samp->IsRAP = RAP;
							nbI++;
						} else {
							nbP++;
						}
						/*even if no out-of-order frames we MUST adjust CTS if cts_offset is present is */
						if (ref_frame && has_cts_offset)
							gf_isom_modify_cts_offset(import->dest, track, ref_frame, (1+b_frames)*dts_inc);

						ref_frame = cur_samp+1;
						if (max_b<b_frames) max_b = b_frames;
						b_frames = 0;
					}
					/*frame_start indicates start of VOP (eg we always remove VOL from each I)*/
					samp->data = frame + samp_offset + frame_start;
					assert(framesize < 1<<31);
					samp->dataLength = (u32) framesize;

					if (import->flags & GF_IMPORT_USE_DATAREF) {
						samp->data = NULL;
						e = gf_isom_add_sample_reference(import->dest, track, di, samp, file_offset + samp_offset + frame_start);
					} else {
						e = gf_isom_add_sample(import->dest, track, di, samp);
					}
					cur_samp++;
					samp->DTS += dts_inc;
					if (e) {
						gf_import_message(import, GF_OK, "Error importing AVI frame %d", i+1);
						goto exit;
					}
				}
				if (!size || (size == framesize + frame_start)) break;
			}
			gf_m4v_parser_del(vparse);
			if (nb_f>2) gf_import_message(import, GF_OK, "Warning: more than 2 frames packed together");
		}
		samp_offset = 0;
		gf_set_progress("Importing AVI Video", i, num_samples);
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT)
			break;
	}

	/*final flush*/
	if (ref_frame && has_cts_offset)
		gf_isom_modify_cts_offset(import->dest, track, ref_frame, (1+b_frames)*dts_inc);

	gf_set_progress("Importing AVI Video", num_samples, num_samples);


	num_samples = gf_isom_get_sample_count(import->dest, track);
	if (has_cts_offset) {
		gf_import_message(import, GF_OK, "Has B-Frames (%d max consecutive B-VOPs%s)", max_b, is_packed ? " - packed bitstream" : "");
		/*repack CTS tables and adjust offsets for B-frames*/
		gf_isom_set_cts_packing(import->dest, track, GF_FALSE);

		if (!(import->flags & GF_IMPORT_NO_EDIT_LIST))
			update_edit_list_for_bframes(import->dest, track);

		/*this is plain ugly but since some encoders (divx) don't use the video PL correctly
		we force the system video_pl to ASP@L5 since we have I, P, B in base layer*/
		if (PL<=3) {
			PL = 0xF5;
			erase_pl = GF_TRUE;
			gf_import_message(import, GF_OK, "WARNING: indicated profile doesn't include B-VOPs - forcing %s", gf_m4v_get_profile_name((u8) PL));
		}
		gf_import_message(import, GF_OK, "Import results: %d VOPs (%d Is - %d Ps - %d Bs)", num_samples, nbI, nbP, nbB);
	} else {
		/*no B-frames, remove CTS offsets*/
		gf_isom_remove_cts_info(import->dest, track);
		gf_import_message(import, GF_OK, "Import results: %d VOPs (%d Is - %d Ps)", num_samples, nbI, nbP);
	}

	samp->data = NULL;
	gf_isom_sample_del(&samp);

	if (erase_pl) {
		gf_m4v_rewrite_pl(&import->esd->decoderConfig->decoderSpecificInfo->data, &import->esd->decoderConfig->decoderSpecificInfo->dataLength, (u8) PL);
		gf_isom_change_mpeg4_description(import->dest, track, 1, import->esd);
	}
	gf_media_update_bitrate(import->dest, track);

	if (is_vfr) {
		if (nbB) {
			if (is_packed) gf_import_message(import, GF_OK, "Warning: Mix of non-coded frames: packed bitstream and encoder skiped - unpredictable timing");
		} else {
			if (import->flags & GF_IMPORT_NO_FRAME_DROP) {
				if (nbNotCoded) gf_import_message(import, GF_OK, "Stream has %d N-VOPs", nbNotCoded);
			} else {
				gf_import_message(import, GF_OK, "import using Variable Frame Rate - Removed %d N-VOPs", nbNotCoded);
			}
			nbNotCoded = 0;
		}
	}

	if (nbDummy || nbNotCoded) gf_import_message(import, GF_OK, "Removed Frames: %d VFW delay frames - %d N-VOPs", nbDummy, nbNotCoded);
	gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_VISUAL, (u8) PL);

exit:
	if (destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	if (frame) gf_free(frame);
	AVI_close(in);
	return e;
}

GF_Err gf_import_avi_audio(GF_MediaImporter *import)
{
	GF_Err e;
	FILE *test;
	u64 duration;
	u32 hdr, di, track, i, tot_size;
	s64 offset;
	s32 size, max_size, done;
	u16 sampleRate;
	Double dur;
	Bool is_cbr;
	u8 oti;
	GF_ISOSample *samp;
	char *frame;
	Bool destroy_esd;
	s32 continuous;
	unsigned char temp[4];
	avi_t *in;

	/*video only, ignore*/
	if (import->trackID==1) return GF_OK;


	test = gf_fopen(import->in_name, "rb");
	if (!test) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);
	gf_fclose(test);
	in = AVI_open_input_file(import->in_name, 1);
	if (!in) return gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported avi file");

	AVI_seek_start(in);

	e = GF_OK;
	if (import->trackID)  AVI_set_audio_track(in, import->trackID-2);

	if (AVI_read_audio(in, (char *) temp, 4, &continuous) != 4) {
		AVI_close(in);
		return gf_import_message(import, GF_OK, "No audio track found");
	}

	hdr = GF_4CC(temp[0], temp[1], temp[2], temp[3]);
	if ((hdr &0xFFE00000) != 0xFFE00000) {
		AVI_close(in);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported AVI audio format");
	}

	sampleRate = gf_mp3_sampling_rate(hdr);
	oti = gf_mp3_object_type_indication(hdr);
	if (!oti || !sampleRate) {
		AVI_close(in);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Error: invalid MPEG Audio track");
	}

	frame = NULL;
	destroy_esd = GF_FALSE;
	if (!import->esd) {
		destroy_esd = GF_TRUE;
		import->esd = gf_odf_desc_esd_new(0);
	}
	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, sampleRate);
	if (!track) goto exit;
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;

	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->slConfig->timestampResolution = sampleRate;
	if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
	import->esd->decoderConfig->decoderSpecificInfo = NULL;
	import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	import->esd->decoderConfig->objectTypeIndication = oti;
	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	if (e) goto exit;

	gf_import_message(import, GF_OK, "AVI Audio import - sample rate %d - %s audio - %d channel%s", sampleRate, (oti==GPAC_OTI_AUDIO_MPEG1) ? "MPEG-1" : "MPEG-2", gf_mp3_num_channels(hdr), (gf_mp3_num_channels(hdr)>1) ? "s" : "");

	AVI_seek_start(in);
	AVI_set_audio_position(in, 0);

	i = 0;
	tot_size = max_size = 0;
	while ((size = (s32) AVI_audio_size(in, i) )>0) {
		if (max_size<size) max_size=size;
		tot_size += size;
		i++;
	}

	frame = (char*)gf_malloc(sizeof(char) * max_size);
	AVI_seek_start(in);
	AVI_set_audio_position(in, 0);

	dur = import->duration;
	dur *= sampleRate;
	dur /= 1000;
	duration = (u32) dur;

	samp = gf_isom_sample_new();
	done=max_size=0;
	is_cbr = GF_TRUE;
	while (1) {
		if (AVI_read_audio(in, frame, 4, (int*)&continuous) != 4) break;
		offset = gf_ftell(in->fdes) - 4;
		hdr = GF_4CC((u8) frame[0], (u8) frame[1], (u8) frame[2], (u8) frame[3]);

		size = gf_mp3_frame_size(hdr);
		if (size>max_size) {
			frame = (char*)gf_realloc(frame, sizeof(char) * size);
			if (max_size) is_cbr = GF_FALSE;
			max_size = size;
		}
		size = 4 + (s32) AVI_read_audio(in, &frame[4], size - 4, &continuous);

		if ((import->flags & GF_IMPORT_USE_DATAREF) && !continuous) {
			gf_import_message(import, GF_IO_ERR, "Cannot use media references, splitted input audio frame found");
			e = GF_IO_ERR;
			goto exit;
		}
		samp->IsRAP = RAP;
		samp->data = frame;
		samp->dataLength = size;
		if (import->flags & GF_IMPORT_USE_DATAREF) {
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else {
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		if (e) goto exit;

		samp->DTS += gf_mp3_window_size(hdr);
		gf_set_progress("Importing AVI Audio", done, tot_size);

		done += size;
		if (duration && (samp->DTS > duration) ) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}

	gf_set_progress("Importing AVI Audio", tot_size, tot_size);

	gf_import_message(import, GF_OK, "Import done - %s bit rate MP3 detected", is_cbr ? "constant" : "variable");
	samp->data = NULL;
	gf_isom_sample_del(&samp);

	gf_media_update_bitrate(import->dest, track);

	gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_AUDIO, 0xFE);


exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	if (frame) gf_free(frame);
	AVI_close(in);
	return e;
}
#endif /*GPAC_DISABLE_AVILIB*/


GF_Err gf_import_isomedia(GF_MediaImporter *import)
{
	GF_Err e;
	u64 offset, sampDTS, duration, dts_offset;
	Bool is_nalu_video = GF_FALSE;
	u32 track, di, trackID, track_in, i, num_samples, mtype, w, h, sr, sbr_sr, ch, mstype, cur_extract_mode, cdur;
	s32 trans_x, trans_y;
	s16 layer;
	u8 bps;
	char *lang;
	const char *orig_name = gf_url_get_resource_name(gf_isom_get_filename(import->orig));
	Bool sbr, ps, has_seig;
	GF_ISOSample *samp;
	GF_ESD *origin_esd;
	GF_InitialObjectDescriptor *iod;
	Bool is_cenc;
	u32 clone_flags=0;
	sampDTS = 0;
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		for (i=0; i<gf_isom_get_track_count(import->orig); i++) {
			import->tk_info[i].track_num = gf_isom_get_track_id(import->orig, i+1);
			import->tk_info[i].type = gf_isom_get_media_type(import->orig, i+1);
			import->tk_info[i].flags = GF_IMPORT_USE_DATAREF;
			if (gf_isom_is_video_subtype(import->tk_info[i].type) ) {
				gf_isom_get_visual_info(import->orig, i+1, 1, &import->tk_info[i].video_info.width, &import->tk_info[i].video_info.height);
			} else if (import->tk_info[i].type == GF_ISOM_MEDIA_AUDIO) {
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
	if (gf_isom_is_video_subtype(mtype)) {
		u8 PL = iod ? iod->visual_profileAndLevel : 0xFE;
		w = h = 0;
		gf_isom_get_visual_info(import->orig, track_in, 1, &w, &h);
#ifndef GPAC_DISABLE_AV_PARSERS
		/*for MPEG-4 visual, always check size (don't trust input file)*/
		if (origin_esd && (origin_esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) ) {
			if (!origin_esd->decoderConfig->decoderSpecificInfo) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] File %s has invalid track #%d: decoderSpecificInfo is missing.\n", orig_name, trackID));
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] extracting track (with -raw %d) and reimporting might fix it.\n", trackID));
			}
			else {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(origin_esd->decoderConfig->decoderSpecificInfo->data, origin_esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				w = dsi.width;
				h = dsi.height;
				PL = dsi.VideoPL;
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
        if (origin_esd && (origin_esd->decoderConfig->objectTypeIndication==GPAC_OTI_AUDIO_AAC_MPEG4)) {
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
		if (origin_esd && origin_esd->decoderConfig->objectTypeIndication == GPAC_OTI_MEDIA_SUBPIC) {
			gf_isom_get_track_layout_info(import->orig, track_in, &w, &h, &trans_x, &trans_y, &layer);
		}
	}

	gf_odf_desc_del((GF_Descriptor *) iod);
	if ( ! gf_isom_get_track_count(import->dest)) {
		u32 timescale = gf_isom_get_timescale(import->orig);
		gf_isom_set_timescale(import->dest, timescale);
	}

	clone_flags = GF_ISOM_CLONE_TRACK_NO_QT;
	if (import->asemode == GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF) {
		clone_flags = 0;
	}

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

	is_cenc = gf_isom_is_cenc_media(import->orig, track_in, 0);
	if (gf_isom_is_media_encrypted(import->orig, track_in, 0)) {
		gf_isom_get_original_format_type(import->orig, track_in, 0, &mstype);
	}
	has_seig = GF_FALSE;
	if (is_cenc && gf_isom_has_cenc_sample_group(import->dest, track) ) {
		has_seig = GF_TRUE;
	}

	duration = (u64) (((Double)import->duration * gf_isom_get_media_timescale(import->orig, track_in)) / 1000);
	gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT);

	cdur = gf_isom_get_constant_sample_duration(import->orig, track_in);
	gf_isom_enable_raw_pack(import->orig, track_in, 2048);

	if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
		if (is_cenc ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] CENC media detected - cannot switch parameter set storage mode\n"));
		} else if (import->flags & GF_IMPORT_USE_DATAREF) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ISOM import] Cannot switch parameter set storage mode when using data reference\n"));
		} else {
			switch (mstype) {
			case GF_ISOM_SUBTYPE_AVC_H264:
				gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
				gf_isom_avc_set_inband_config(import->dest, track, 1);
				break;
			case GF_ISOM_SUBTYPE_HVC1:
				gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT | GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
				gf_isom_hevc_set_inband_config(import->dest, track, 1);
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
		e = gf_isom_cenc_get_sample_aux_info(import->orig, track_in, 0, 1, NULL, &container_type);
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

		if (import->audio_roll_change) {
			gf_isom_set_sample_roll_group(import->dest, track, i+1, import->audio_roll);
		}

		gf_set_progress("Importing ISO File", i+1, num_samples);


		if (e)
			goto exit;
		if (is_cenc) {
			GF_CENCSampleAuxInfo *sai;
			u32 container_type, len, j, Is_Encrypted;
			u8 IV_size;
			bin128 KID;
			u8 crypt_byte_block, skip_byte_block;
			u8 constant_IV_size;
			bin128 constant_IV;
			GF_BitStream *bs;
			char *buffer;

			sai = NULL;
			e = gf_isom_cenc_get_sample_aux_info(import->orig, track_in, i+1, di, &sai, &container_type);
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
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, IV_size, buffer, len, is_nalu_video, NULL);
				gf_free(buffer);
			} else {
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, IV_size, NULL, samp->dataLength, is_nalu_video, NULL);
			}
			if (e) goto exit;

			if (has_seig) {
				e = gf_isom_set_sample_cenc_group(import->dest, track, i+1, Is_Encrypted, IV_size, KID, crypt_byte_block, skip_byte_block, constant_IV_size, constant_IV);
				if (e) goto exit;
			}
		}
		if (duration && (sampDTS > duration) ) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
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

	gf_media_update_bitrate(import->dest, track);
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

#ifndef GPAC_DISABLE_MPEG2PS

#include "mpeg2_ps.h"

GF_Err gf_import_mpeg_ps_video(GF_MediaImporter *import)
{
	GF_Err e;
	mpeg2ps_t *ps;
	Double FPS;
	char *buf;
	u8 ftype;
	u32 track, di, streamID, mtype, w, h, ar, nb_streams, buf_len, frames, ref_frame, timescale, dts_inc, last_pos;
	u64 file_size, duration;
	Bool destroy_esd;

	if (import->flags & GF_IMPORT_USE_DATAREF)
		return gf_import_message(import, GF_NOT_SUPPORTED, "Cannot use data referencing with MPEG-1/2 files");

	/*no auto frame-rate detection*/
	if (import->video_fps == GF_IMPORT_AUTO_FPS)
		import->video_fps = GF_IMPORT_DEFAULT_FPS;

	ps = mpeg2ps_init(import->in_name);
	if (!ps) return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Failed to open MPEG file %s", import->in_name);

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		u32 i, nb_v_str;
		import->nb_tracks = 0;
		nb_v_str = nb_streams = mpeg2ps_get_video_stream_count(ps);
		for (i=0; i<nb_streams; i++) {
			import->tk_info[import->nb_tracks].track_num = i+1;
			import->tk_info[import->nb_tracks].type = GF_ISOM_MEDIA_VISUAL;
			import->tk_info[import->nb_tracks].flags = GF_IMPORT_OVERRIDE_FPS;

			import->tk_info[import->nb_tracks].video_info.FPS = mpeg2ps_get_video_stream_framerate(ps, i);
			import->tk_info[import->nb_tracks].video_info.width = mpeg2ps_get_video_stream_width(ps, i);
			import->tk_info[import->nb_tracks].video_info.height = mpeg2ps_get_video_stream_height(ps, i);
			import->tk_info[import->nb_tracks].video_info.par = mpeg2ps_get_video_stream_aspect_ratio(ps, i);

			import->tk_info[import->nb_tracks].media_type = GF_MEDIA_TYPE_MPG1;
			if (mpeg2ps_get_video_stream_type(ps, i) == MPEG_VIDEO_MPEG2) import->tk_info[import->nb_tracks].media_type ++;

			import->nb_tracks++;
		}
		nb_streams = mpeg2ps_get_audio_stream_count(ps);
		for (i=0; i<nb_streams; i++) {
			import->tk_info[import->nb_tracks].track_num = nb_v_str + i+1;
			import->tk_info[import->nb_tracks].type = GF_ISOM_MEDIA_AUDIO;
			switch (mpeg2ps_get_audio_stream_type(ps, i)) {
			case MPEG_AUDIO_MPEG:
				import->tk_info[import->nb_tracks].media_type = GF_MEDIA_TYPE_MPGA;
				break;
			case MPEG_AUDIO_AC3:
				import->tk_info[import->nb_tracks].media_type = GF_MEDIA_TYPE_AC3;
				break;
			case MPEG_AUDIO_LPCM:
				import->tk_info[import->nb_tracks].media_type = GF_MEDIA_TYPE_LPCM;
				break;
			default:
				import->tk_info[import->nb_tracks].media_type = GF_MEDIA_TYPE_UNK;
				break;
			}
			import->tk_info[import->nb_tracks].audio_info.sample_rate = mpeg2ps_get_audio_stream_sample_freq(ps, i);
			import->tk_info[import->nb_tracks].audio_info.nb_channels = mpeg2ps_get_audio_stream_channels(ps, i);
			import->nb_tracks ++;
		}
		mpeg2ps_close(ps);
		return GF_OK;
	}


	streamID = 0;
	nb_streams = mpeg2ps_get_video_stream_count(ps);
	if ((nb_streams>1) && !import->trackID) {
		mpeg2ps_close(ps);
		return gf_import_message(import, GF_BAD_PARAM, "%d video tracks in MPEG file - please indicate track to import", nb_streams);
	}
	/*audio*/
	if (import->trackID>nb_streams) {
		mpeg2ps_close(ps);
		return GF_OK;
	}
	if (import->trackID) streamID = import->trackID - 1;

	if (streamID>=nb_streams) {
		mpeg2ps_close(ps);
		return gf_import_message(import, GF_BAD_PARAM, "Desired video track not found in MPEG file (%d visual streams)", nb_streams);
	}
	w = mpeg2ps_get_video_stream_width(ps, streamID);
	h = mpeg2ps_get_video_stream_height(ps, streamID);
	ar = mpeg2ps_get_video_stream_aspect_ratio(ps, streamID);
	mtype = (mpeg2ps_get_video_stream_type(ps, streamID) == MPEG_VIDEO_MPEG2) ? GPAC_OTI_VIDEO_MPEG2_MAIN : GPAC_OTI_VIDEO_MPEG1;
	FPS = mpeg2ps_get_video_stream_framerate(ps, streamID);
	if (import->video_fps) FPS = (Double) import->video_fps;
	get_video_timing(FPS, &timescale, &dts_inc);

	duration = import->duration;
	duration *= timescale;
	duration /= 1000;

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		destroy_esd = GF_TRUE;
		import->esd = gf_odf_desc_esd_new(0);
	}
	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_VISUAL, timescale);
	e = gf_isom_last_error(import->dest);
	if (!track) goto exit;
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;

	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->slConfig->timestampResolution = timescale;
	if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
	import->esd->decoderConfig->decoderSpecificInfo = NULL;
	import->esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	import->esd->decoderConfig->objectTypeIndication = mtype;
	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, NULL, NULL, &di);
	if (e) goto exit;

	gf_import_message(import, GF_OK, "%s Video import - Resolution %d x %d @ %02.4f FPS", (mtype==GPAC_OTI_VIDEO_MPEG1) ? "MPEG-1" : "MPEG-2", w, h, FPS);
	gf_isom_set_visual_info(import->dest, track, di, w, h);

	if (!gf_isom_get_media_timescale(import->dest, track)) {
		e = gf_import_message(import, GF_BAD_PARAM, "No timescale for imported track - ignoring");
		if (e) goto exit;
	}

	gf_isom_set_cts_packing(import->dest, track, GF_TRUE);

	file_size = mpeg2ps_get_ps_size(ps);
	last_pos = 0;
	frames = 1;
	ref_frame = 1;
	while (mpeg2ps_get_video_frame(ps, streamID, (u8 **) &buf, &buf_len, &ftype, TS_90000, NULL)) {
		GF_ISOSample *samp;
		if ((buf[buf_len - 4] == 0) && (buf[buf_len - 3] == 0) && (buf[buf_len - 2] == 1)) buf_len -= 4;
		samp = gf_isom_sample_new();
		samp->data = buf;
		samp->dataLength = buf_len;
		samp->DTS = (u64)dts_inc*(frames-1);
		samp->IsRAP = (ftype==1) ? RAP : RAP_NO;
		samp->CTS_Offset = 0;
		e = gf_isom_add_sample(import->dest, track, di, samp);
		samp->data = NULL;
		gf_isom_sample_del(&samp);
		if (e) goto exit;

		last_pos = (u32) mpeg2ps_get_video_pos(ps, streamID);
		gf_set_progress("Importing MPEG-PS Video", last_pos/1024, file_size/1024);

		if (ftype != 3) {
			gf_isom_modify_cts_offset(import->dest, track, ref_frame, (frames-ref_frame)*dts_inc);
			ref_frame = frames;
		}
		frames++;

		if (duration && (dts_inc*(frames-1) >= duration) ) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}
	gf_isom_set_cts_packing(import->dest, track, GF_FALSE);
	if (!(import->flags & GF_IMPORT_NO_EDIT_LIST))
		update_edit_list_for_bframes(import->dest, track);

	if (last_pos!=file_size) gf_set_progress("Importing MPEG-PS Video", frames, frames);

	gf_media_update_bitrate(import->dest, track);
	if (ar) gf_media_change_par(import->dest, track, ar>>16, ar&0xffff, GF_FALSE, GF_FALSE);

exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	mpeg2ps_close(ps);
	return e;
}

GF_Err gf_import_mpeg_ps_audio(GF_MediaImporter *import)
{
	GF_Err e;
	mpeg2ps_t *ps;
	char *buf;
	u32 track, di, streamID, mtype, sr, nb_ch, nb_streams, buf_len, frames, hdr, last_pos;
	u64 file_size, duration;
	Bool destroy_esd;
	GF_ISOSample *samp;

	if (import->flags & GF_IMPORT_PROBE_ONLY) return GF_OK;

	if (import->flags & GF_IMPORT_USE_DATAREF)
		return gf_import_message(import, GF_NOT_SUPPORTED, "Cannot use data referencing with MPEG-1/2 files");

	ps = mpeg2ps_init(import->in_name);
	if (!ps) return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Failed to open MPEG file %s", import->in_name);


	streamID = 0;
	nb_streams = mpeg2ps_get_audio_stream_count(ps);
	if ((nb_streams>1) && !import->trackID) {
		mpeg2ps_close(ps);
		return gf_import_message(import, GF_BAD_PARAM, "%d audio tracks in MPEG file - please indicate track to import", nb_streams);
	}

	if (import->trackID) {
		u32 nb_v = mpeg2ps_get_video_stream_count(ps);
		/*video*/
		if (import->trackID<=nb_v) {
			mpeg2ps_close(ps);
			return GF_OK;
		}
		streamID = import->trackID - 1 - nb_v;
	}

	if (streamID>=nb_streams) {
		mpeg2ps_close(ps);
		return gf_import_message(import, GF_BAD_PARAM, "Desired audio track not found in MPEG file (%d audio streams)", nb_streams);
	}

	mtype = mpeg2ps_get_audio_stream_type(ps, streamID);
	if (mtype != MPEG_AUDIO_MPEG) {
		mpeg2ps_close(ps);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Audio format not supported in MP4");
	}

	if (mpeg2ps_get_audio_frame(ps, streamID, (u8**) &buf, &buf_len, TS_90000, NULL, NULL) == 0) {
		mpeg2ps_close(ps);
		return gf_import_message(import, GF_IO_ERR, "Cannot fetch audio frame from MPEG file");
	}

	hdr = GF_4CC((u8)buf[0],(u8)buf[1],(u8)buf[2],(u8)buf[3]);
	mtype = gf_mp3_object_type_indication(hdr);
	sr = gf_mp3_sampling_rate(hdr);
	nb_ch = gf_mp3_num_channels(hdr);

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		destroy_esd = GF_TRUE;
		import->esd = gf_odf_desc_esd_new(0);
	}
	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, sr);
	e = gf_isom_last_error(import->dest);
	if (!track) goto exit;
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;

	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	import->esd->slConfig->timestampResolution = sr;
	if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
	import->esd->decoderConfig->decoderSpecificInfo = NULL;
	import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	import->esd->decoderConfig->objectTypeIndication = mtype;
	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, NULL, NULL, &di);
	if (e) goto exit;

	gf_isom_set_audio_info(import->dest, track, di, sr, nb_ch, 16, import->asemode);
	gf_import_message(import, GF_OK, "%s Audio import - sample rate %d - %d channel%s", (mtype==GPAC_OTI_AUDIO_MPEG1) ? "MPEG-1" : "MPEG-2", sr, nb_ch, (nb_ch>1) ? "s" : "");


	duration = (u64) ((Double)import->duration/1000.0 * sr);

	samp = gf_isom_sample_new();
	samp->IsRAP = RAP;
	samp->DTS = 0;

	file_size = mpeg2ps_get_ps_size(ps);
	frames = 0;
	do {
		samp->data = buf;
		samp->dataLength = buf_len;
		e = gf_isom_add_sample(import->dest, track, di, samp);
		if (e) goto exit;
		samp->DTS += gf_mp3_window_size(hdr);
		last_pos = (u32) mpeg2ps_get_audio_pos(ps, streamID);
		gf_set_progress("Importing MPEG-PS Audio", last_pos/1024, file_size/1024);
		frames++;
		if (duration && (samp->DTS>=duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}  while (mpeg2ps_get_audio_frame(ps, streamID, (u8**)&buf, &buf_len, TS_90000, NULL, NULL));

	samp->data = NULL;
	gf_isom_sample_del(&samp);
	if (last_pos!=file_size) gf_set_progress("Importing MPEG-PS Audio", frames, frames);
	gf_media_update_bitrate(import->dest, track);

exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	mpeg2ps_close(ps);
	return e;
}
#endif /*GPAC_DISABLE_MPEG2PS*/


GF_Err gf_import_nhnt(GF_MediaImporter *import)
{
	GF_Err e;
	Bool destroy_esd, next_is_start;
	u32 track, di, mtype, max_size, count, w, h, sig;
	GF_ISOSample *samp;
	s64 media_size, media_done;
	u64 offset, duration;
	GF_BitStream *bs;
	FILE *nhnt, *mdia, *info;
	char *ext, szName[1000], szMedia[1000], szNhnt[1000];

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = 0;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF;
		return GF_OK;
	}

	strcpy(szName, import->in_name);
	ext = strrchr(szName, '.');
	if (ext) ext[0] = 0;

	strcpy(szMedia, szName);
	strcat(szMedia, ".nhnt");
	nhnt = gf_fopen(szMedia, "rb");
	if (!nhnt) return gf_import_message(import, GF_URL_ERROR, "Cannot find NHNT file %s", szMedia);

	strcpy(szMedia, szName);
	strcat(szMedia, ".media");
	mdia = gf_fopen(szMedia, "rb");
	if (!mdia) {
		gf_fclose(nhnt);
		return gf_import_message(import, GF_URL_ERROR, "Cannot find MEDIA file %s", szMedia);
	}

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(2);
		destroy_esd = GF_TRUE;
	}
	/*update stream type/oti*/
	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);


	strcpy(szNhnt, szName);
	strcat(szNhnt, ".info");
	info = gf_fopen(szNhnt, "rb");
	if (info) {
		if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
		import->esd->decoderConfig->decoderSpecificInfo = NULL;
		import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		gf_fseek(info, 0, SEEK_END);
		import->esd->decoderConfig->decoderSpecificInfo->dataLength = (u32) gf_ftell(info);
		import->esd->decoderConfig->decoderSpecificInfo->data = (char*)gf_malloc(sizeof(char) * import->esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_fseek(info, 0, SEEK_SET);
		if (0==fread(import->esd->decoderConfig->decoderSpecificInfo->data, 1, import->esd->decoderConfig->decoderSpecificInfo->dataLength, info)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER,
			       ("[NHML import] Failed to read dataLength\n"));
		}
		gf_fclose(info);
	}
	/*keep parsed dsi (if any) if no .info file exists*/

	bs = gf_bs_from_file(nhnt, GF_BITSTREAM_READ);
	sig = GF_4CC(gf_bs_read_u8(bs), gf_bs_read_u8(bs), gf_bs_read_u8(bs), gf_bs_read_u8(bs));
	if (sig == GF_MEDIA_TYPE_NHNT) sig = 0;
	else if (sig == GF_MEDIA_TYPE_NHNL) sig = 1;
	else {
		gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Invalid NHNT signature");
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit;
	}
	/*version*/
	gf_bs_read_u8(bs);
	import->esd->decoderConfig->streamType = gf_bs_read_u8(bs);
	import->esd->decoderConfig->objectTypeIndication = gf_bs_read_u8(bs);
	gf_bs_read_u16(bs);
	import->esd->decoderConfig->bufferSizeDB = gf_bs_read_u24(bs);
	import->esd->decoderConfig->avgBitrate = gf_bs_read_u32(bs);
	import->esd->decoderConfig->maxBitrate = gf_bs_read_u32(bs);
	import->esd->slConfig->timestampResolution = gf_bs_read_u32(bs);

	w = h = 0;
	switch (import->esd->decoderConfig->streamType) {
	case GF_STREAM_SCENE:
		mtype = GF_ISOM_MEDIA_SCENE;
		/*we don't know PLs from NHNT...*/
		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_SCENE, 0xFE);
		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_GRAPHICS, 0xFE);
		break;
	case GF_STREAM_VISUAL:
		mtype = GF_ISOM_MEDIA_VISUAL;
#ifndef GPAC_DISABLE_AV_PARSERS
		if (import->esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
			GF_M4VDecSpecInfo dsi;
			if (!import->esd->decoderConfig->decoderSpecificInfo) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			e = gf_m4v_get_config(import->esd->decoderConfig->decoderSpecificInfo->data, import->esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			if (e) goto exit;
			w = dsi.width;
			h = dsi.height;
		}
#endif
		break;
	case GF_STREAM_AUDIO:
		mtype = GF_ISOM_MEDIA_AUDIO;
		break;
	case GF_STREAM_MPEG7:
		mtype = GF_ISOM_MEDIA_MPEG7;
		break;
	case GF_STREAM_IPMP:
		mtype = GF_ISOM_MEDIA_IPMP;
		break;
	case GF_STREAM_OCI:
		mtype = GF_ISOM_MEDIA_OCI;
		break;
	case GF_STREAM_MPEGJ:
		mtype = GF_ISOM_MEDIA_MPEGJ;
		break;
	/*note we cannot import OD from NHNT*/
	case GF_STREAM_OD:
		e = GF_NOT_SUPPORTED;
		goto exit;
	case GF_STREAM_INTERACT:
		mtype = GF_ISOM_MEDIA_SCENE;
		break;
	default:
		mtype = GF_ISOM_MEDIA_ESM;
		break;
	}
	track = gf_isom_new_track(import->dest, import->esd->ESID, mtype, import->esd->slConfig->timestampResolution);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;
	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? szMedia : NULL, NULL, &di);
	if (e) goto exit;

	if (w && h) {
		gf_isom_set_visual_info(import->dest, track, di, w, h);
		gf_media_update_par(import->dest, track);
	}

	gf_import_message(import, GF_OK, "NHNT import - Stream Type %s - ObjectTypeIndication 0x%02x", gf_odf_stream_type_name(import->esd->decoderConfig->streamType), import->esd->decoderConfig->objectTypeIndication);

	duration = (u64) ( ((Double) import->duration)/ 1000 * import->esd->slConfig->timestampResolution);

	samp = gf_isom_sample_new();
	samp->data = (char*)gf_malloc(sizeof(char) * 1024);
	max_size = 1024;
	count = 0;
	gf_fseek(mdia, 0, SEEK_END);
	media_size = gf_ftell(mdia);
	gf_fseek(mdia, 0, SEEK_SET);
	media_done = 0;
	next_is_start = GF_TRUE;

	while (!feof(nhnt)) {
		Bool is_start, is_end;
		samp->dataLength = gf_bs_read_u24(bs);
		samp->IsRAP = gf_bs_read_int(bs, 1);
		is_start = (Bool)gf_bs_read_int(bs, 1);
		if (next_is_start) {
			is_start = GF_TRUE;
			next_is_start = GF_FALSE;
		}
		is_end = (Bool)gf_bs_read_int(bs, 1);
		if (is_end) next_is_start = GF_TRUE;
		/*3 reserved + AU type (2)*/
		gf_bs_read_int(bs, 5);
		if (sig) {
			u64 CTS;
			offset = gf_bs_read_u64(bs);
			CTS = gf_bs_read_u64(bs);
			samp->DTS = gf_bs_read_u64(bs);
			samp->CTS_Offset = (u32) (CTS - samp->DTS);
		} else {
			offset = gf_bs_read_u32(bs);
			samp->CTS_Offset = gf_bs_read_u32(bs);
			samp->DTS = gf_bs_read_u32(bs);
			samp->CTS_Offset -= (u32) samp->DTS;
		}
		if (!count && samp->DTS) samp->DTS = 0;
		count++;

		if (import->flags & GF_IMPORT_USE_DATAREF) {
			if (!is_start) {
				e = gf_import_message(import, GF_NOT_SUPPORTED, "Fragmented NHNT file detected - cannot use data referencing");
				goto exit;
			}
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else {
			if (samp->dataLength>max_size) {
				samp->data = (char*)gf_realloc(samp->data, sizeof(char) * samp->dataLength);
				max_size = samp->dataLength;
			}
			gf_fseek(mdia, offset, SEEK_SET);
			if (0==fread( samp->data, 1, samp->dataLength, mdia)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Failed to read samp->dataLength\n"));
			}
			if (is_start) {
				e = gf_isom_add_sample(import->dest, track, di, samp);
			} else {
				e = gf_isom_append_sample_data(import->dest, track, samp->data, samp->dataLength);
			}
		}
		media_done += samp->dataLength;
		gf_set_progress("Importing NHNT", (u32) (media_done/1024), (u32) (media_size/1024));
		if (e) goto exit;
		if (!gf_bs_available(bs)) break;
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}
	if (media_done!=media_size) gf_set_progress("Importing NHNT", (u32) (media_size/1024), (u32) (media_size/1024));
	gf_isom_sample_del(&samp);
	gf_media_update_bitrate(import->dest, track);

exit:
	gf_bs_del(bs);
	gf_fclose(nhnt);
	gf_fclose(mdia);
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	return e;
}


typedef struct
{
	Bool from_is_start, from_is_end, to_is_start, to_is_end;
	u64 from_pos, to_pos;
	char *from_id, *to_id;
	GF_List *id_stack;
	GF_SAXParser *sax;
} XMLBreaker;


static void nhml_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	XMLBreaker *breaker = (XMLBreaker *)sax_cbck;
	char *node_id;
	u32 i;
	GF_XMLAttribute *att;
	node_id = NULL;
	for (i=0; i<nb_attributes; i++) {
		att = (GF_XMLAttribute *) &attributes[i];
		if (stricmp(att->name, "DEF") && stricmp(att->name, "id")) continue;
		node_id = gf_strdup(att->value);
		break;
	}
	if (!node_id) {
		node_id = gf_strdup("__nhml__none");
		gf_list_add(breaker->id_stack, node_id);
		return;
	}
	gf_list_add(breaker->id_stack, node_id);

	if (breaker->from_is_start && breaker->from_id && !strcmp(breaker->from_id, node_id)) {
		breaker->from_pos = gf_xml_sax_get_node_start_pos(breaker->sax);
		breaker->from_is_start = GF_FALSE;
	}
	if (breaker->to_is_start && breaker->to_id && !strcmp(breaker->to_id, node_id)) {
		breaker->to_pos = gf_xml_sax_get_node_start_pos(breaker->sax);
		breaker->to_is_start = GF_FALSE;
	}
	if (!breaker->to_is_start && !breaker->from_is_start && !breaker->to_is_end && !breaker->from_is_end) {
		gf_xml_sax_suspend(breaker->sax, GF_TRUE);
	}

}

static void nhml_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	XMLBreaker *breaker = (XMLBreaker *)sax_cbck;
	char *node_id = (char *)gf_list_last(breaker->id_stack);
	gf_list_rem_last(breaker->id_stack);
	if (breaker->from_is_end && breaker->from_id && !strcmp(breaker->from_id, node_id)) {
		breaker->from_pos = gf_xml_sax_get_node_end_pos(breaker->sax);
		breaker->from_is_end = GF_FALSE;
	}
	if (breaker->to_is_end && breaker->to_id && !strcmp(breaker->to_id, node_id)) {
		breaker->to_pos = gf_xml_sax_get_node_end_pos(breaker->sax);
		breaker->to_is_end = GF_FALSE;
	}
	gf_free(node_id);
	if (!breaker->to_is_start && !breaker->from_is_start && !breaker->to_is_end && !breaker->from_is_end) {
		gf_xml_sax_suspend(breaker->sax, GF_TRUE);
	}
}


GF_Err gf_import_sample_from_xml(GF_MediaImporter *import, GF_ISOSample *samp, char *xml_file, char *xmlFrom, char *xmlTo, u32 *max_size)
{
	GF_Err e;
	u32 read;
	XMLBreaker breaker;
	char *tmp;
	FILE *xml;
	u8 szBOM[3];
	if (!xml_file || !xmlFrom || !xmlTo) return GF_BAD_PARAM;

	memset(&breaker, 0, sizeof(XMLBreaker));

	xml = gf_fopen(xml_file, "rb");
	if (!xml) {
		e = gf_import_message(import, GF_BAD_PARAM, "NHML import failure: file %s not found", xml_file);
		goto exit;
	}
	//we cannot use files with BOM since the XML position we get from the parser are offsets in the UTF-8 version of the XML.
	//TODO: to support files with BOM we would need to serialize on the fly the callback from the sax parser
	read = (u32) fread(szBOM, 1, 3, xml);
	if (read==3) {
		fseek(xml, 0, SEEK_SET);
		if ((szBOM[0]==0xFF) || (szBOM[0]==0xFE) || (szBOM[0]==0xEF)) {
			e = gf_import_message(import, GF_NOT_SUPPORTED, "NHML import failure: XML file %s uses BOM, please convert to plin UTF-8 or ANSI first", xml_file);
			goto exit;
		}
	}


	memset(&breaker, 0, sizeof(XMLBreaker));
	breaker.id_stack = gf_list_new();

	if (strstr(xmlFrom, ".start")) breaker.from_is_start = GF_TRUE;
	else breaker.from_is_end = GF_TRUE;
	tmp = strchr(xmlFrom, '.');
	*tmp = 0;
	if (stricmp(xmlFrom, "doc")) breaker.from_id = gf_strdup(xmlFrom);
	/*doc start pos is 0, no need to look for it*/
	else if (breaker.from_is_start) breaker.from_is_start = GF_FALSE;
	*tmp = '.';

	if (strstr(xmlTo, ".start")) breaker.to_is_start = GF_TRUE;
	else breaker.to_is_end = GF_TRUE;
	tmp = strchr(xmlTo, '.');
	*tmp = 0;
	if (stricmp(xmlTo, "doc")) breaker.to_id = gf_strdup(xmlTo);
	/*doc end pos is file size, no need to look for it*/
	else if (breaker.to_is_end) breaker.to_is_end = GF_FALSE;
	*tmp = '.';

	breaker.sax = gf_xml_sax_new(nhml_node_start, nhml_node_end, NULL, &breaker);
	e = gf_xml_sax_parse_file(breaker.sax, xml_file, NULL);
	gf_xml_sax_del(breaker.sax);
	if (e<0) goto exit;
	e = GF_OK;

	if (!breaker.to_id) {
		gf_fseek(xml, 0, SEEK_END);
		breaker.to_pos = gf_ftell(xml);
		gf_fseek(xml, 0, SEEK_SET);
	}
	if(breaker.to_pos < breaker.from_pos) {
		e = gf_import_message(import, GF_BAD_PARAM, "NHML import failure: xmlFrom %s is located after xmlTo %s", xmlFrom, xmlTo);
		goto exit;
	}

	assert(breaker.to_pos > breaker.from_pos);

	samp->dataLength = (u32) (breaker.to_pos - breaker.from_pos);
	if (*max_size < samp->dataLength) {
		*max_size = samp->dataLength;
		samp->data = (char*)gf_realloc(samp->data, sizeof(char)*samp->dataLength);
	}
	gf_fseek(xml, breaker.from_pos, SEEK_SET);
	if (0==fread(samp->data, 1, samp->dataLength, xml)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Failed to read samp->dataLength\n"));
	}

exit:
	if (xml) gf_fclose(xml);
	while (gf_list_count(breaker.id_stack)) {
		char *id = (char *)gf_list_last(breaker.id_stack);
		gf_list_rem_last(breaker.id_stack);
		gf_free(id);
	}
	gf_list_del(breaker.id_stack);
	if (breaker.from_id) gf_free(breaker.from_id);
	if (breaker.to_id) gf_free(breaker.to_id);
	return e;
}


#ifndef GPAC_DISABLE_ZLIB

/*since 0.2.2, we use zlib for xmt/x3d reading to handle gz files*/
#include <zlib.h>

#define ZLIB_COMPRESS_SAFE	4

static GF_Err compress_sample_data(GF_ISOSample *samp, u32 *max_size, char **dict, u32 offset)
{
	z_stream stream;
	int err;
	char *dest = (char *)gf_malloc(sizeof(char)*samp->dataLength*ZLIB_COMPRESS_SAFE);
	stream.next_in = (Bytef*)samp->data + offset;
	stream.avail_in = (uInt)samp->dataLength - offset;
	stream.next_out = ( Bytef*)dest;
	stream.avail_out = (uInt)samp->dataLength*ZLIB_COMPRESS_SAFE;
	stream.zalloc = (alloc_func)NULL;
	stream.zfree = (free_func)NULL;
	stream.opaque = (voidpf)NULL;

	err = deflateInit(&stream, 9);
	if (err != Z_OK) {
		gf_free(dest);
		return GF_IO_ERR;
	}
	if (dict && *dict) {
		err = deflateSetDictionary(&stream, (Bytef *)*dict, (u32) strlen(*dict));
		if (err != Z_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHML import] Error assigning dictionary\n"));
			deflateEnd(&stream);
			gf_free(dest);
			return GF_IO_ERR;
		}
	}
	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		gf_free(dest);
		return GF_IO_ERR;
	}
	if (samp->dataLength - offset<stream.total_out) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHML import] compressed data (%d) bigger than input data (%d)\n", (u32) stream.total_out, (u32) samp->dataLength - offset));
	}
	if (dict) {
		if (*dict) gf_free(*dict);
		*dict = (char*)gf_malloc(sizeof(char)*samp->dataLength);
		memcpy(*dict, samp->data, samp->dataLength);
	}
	if (*max_size < stream.total_out) {
		*max_size = samp->dataLength*ZLIB_COMPRESS_SAFE;
		samp->data = (char*)gf_realloc(samp->data, *max_size * sizeof(char));
	}

	memcpy(samp->data + offset, dest, sizeof(char)*stream.total_out);
	samp->dataLength = (u32) (offset + stream.total_out);
	gf_free(dest);

	deflateEnd(&stream);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ZLIB*/

static void nhml_on_progress(void *cbk, u64 done, u64 tot)
{
	gf_set_progress("NHML Loading", done, tot);
}

#define NHML_SCAN_INT(_fmt, _value)	\
	{\
	if (strstr(att->value, "0x")) { u32 __i; sscanf(att->value+2, "%x", &__i); _value = __i; }\
	else if (strstr(att->value, "0X")) { u32 __i; sscanf(att->value+2, "%X", &__i); _value = __i; }\
	else sscanf(att->value, _fmt, &_value); \
	}\


/*FIXME - need LARGE FILE support in NHNT - add a new version*/
GF_Err gf_import_nhml_dims(GF_MediaImporter *import, Bool dims_doc)
{
	GF_Err e;
	GF_DIMSDescription dims;
	Bool destroy_esd, inRootOD, do_compress, is_dims;
	u32 i, track, tkID, di, mtype, max_size, count, streamType, oti, timescale, specInfoSize, header_end, dts_inc, par_den, par_num;
	GF_ISOSample *samp;
	GF_XMLAttribute *att;
	s64 media_size, media_done, offset;
	u64 duration, sample_duration;
	FILE *nhml, *mdia, *info;
	char *dictionary = NULL, *auxiliary_mime_types = NULL;
	char *ext, szName[1000], szMedia[GF_MAX_PATH], szMediaTemp[GF_MAX_PATH], szInfo[GF_MAX_PATH], szXmlFrom[1000], szXmlTo[1000], szXmlHeaderEnd[1000];
	char *specInfo;
	GF_GenericSampleDescription sdesc;
	GF_DOMParser *parser;
	GF_XMLNode *root, *node, *childnode;
	char *szRootName, *szSampleName, *szSubSampleName, *szImpName;
#ifndef GPAC_DISABLE_ZLIB
	Bool use_dict = GF_FALSE;
#endif

	szRootName = dims_doc ? "DIMSStream" : "NHNTStream";
	szSampleName = dims_doc ? "DIMSUnit" : "NHNTSample";
	szSubSampleName = dims_doc ? "DIMSSubUnit" : "NHNTSubSample";
	szImpName = dims_doc ? "DIMS" : "NHML";

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = 0;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF;
		return GF_OK;
	}
	nhml = gf_fopen(import->in_name, "rt");
	if (!nhml) return gf_import_message(import, GF_URL_ERROR, "Cannot find %s file %s", szImpName, import->in_name);

	strcpy(szName, import->in_name);
	ext = strrchr(szName, '.');
	if (ext) ext[0] = 0;

	strcpy(szMedia, szName);
	strcat(szMedia, ".media");
	strcpy(szInfo, szName);
	strcat(szInfo, ".info");

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, import->in_name, nhml_on_progress, import);
	if (e) {
		gf_fclose(nhml);
		gf_import_message(import, e, "Error parsing %s file: Line %d - %s", szImpName, gf_xml_dom_get_line(parser), gf_xml_dom_get_error(parser));
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);
	if (!root) {
		e = gf_import_message(import, GF_BAD_PARAM, "Error parsing %s file - no root node found", szImpName);
		gf_xml_dom_del(parser);
		return e;
	}

	mdia = NULL;
	destroy_esd = GF_FALSE;
	dts_inc = 0;
	inRootOD = GF_FALSE;
	do_compress = GF_FALSE;
	is_dims = GF_FALSE;
	specInfo = NULL;
	samp = NULL;
	memset(&dims, 0, sizeof(GF_DIMSDescription));
	dims.profile = dims.level = 255;
	dims.streamType = GF_TRUE;
	dims.containsRedundant = 1;

	if (stricmp(root->name, szRootName)) {
		e = gf_import_message(import, GF_BAD_PARAM, "Error parsing %s file - \"%s\" root expected, got \"%s\"", szImpName, szRootName, root->name);
		goto exit;
	}

	memset(&sdesc, 0, sizeof(GF_GenericSampleDescription));
	tkID = mtype = streamType = oti = par_den = par_num = 0;
	timescale = 1000;
	i=0;
	strcpy(szXmlHeaderEnd, "");
	header_end = 0;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		if (!stricmp(att->name, "streamType")) {
			NHML_SCAN_INT("%u", streamType)
		} else if (!stricmp(att->name, "mediaType") && (strlen(att->value)==4)) {
			mtype = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
		} else if (!stricmp(att->name, "mediaSubType") && (strlen(att->value)==4)) {
			sdesc.codec_tag = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
		} else if (!stricmp(att->name, "objectTypeIndication")) {
			NHML_SCAN_INT("%u", oti)
		} else if (!stricmp(att->name, "timeScale")) {
			NHML_SCAN_INT("%u", timescale)
		} else if (!stricmp(att->name, "width")) {
			NHML_SCAN_INT("%hu", sdesc.width)
		} else if (!stricmp(att->name, "height")) {
			NHML_SCAN_INT("%hu", sdesc.height)
		} else if (!stricmp(att->name, "parNum")) {
			NHML_SCAN_INT("%u", par_num)
		} else if (!stricmp(att->name, "parDen")) {
			NHML_SCAN_INT("%u", par_den)
		} else if (!stricmp(att->name, "sampleRate")) {
			NHML_SCAN_INT("%u", sdesc.samplerate)
		} else if (!stricmp(att->name, "numChannels")) {
			NHML_SCAN_INT("%hu", sdesc.nb_channels)
		} else if (!stricmp(att->name, "baseMediaFile")) {
			char *url = gf_url_concatenate(import->in_name, att->value);
			strcpy(szMedia, url ? url : att->value);
			if (url) gf_free(url);
		} else if (!stricmp(att->name, "specificInfoFile")) {
			char *url = gf_url_concatenate(import->in_name, att->value);
			strcpy(szInfo, url ? url : att->value);
			if (url) gf_free(url);
		} else if (!stricmp(att->name, "headerEnd")) {
			NHML_SCAN_INT("%u", header_end)
		} else if (!stricmp(att->name, "trackID")) {
			NHML_SCAN_INT("%u", tkID)
		} else if (!stricmp(att->name, "inRootOD")) {
			inRootOD = (!stricmp(att->value, "yes") );
		} else if (!stricmp(att->name, "DTS_increment")) {
			NHML_SCAN_INT("%u", dts_inc)
		} else if (!stricmp(att->name, "gzipSamples")) {
			do_compress = (!stricmp(att->value, "yes")) ? GF_TRUE : GF_FALSE;
		} else if (!stricmp(att->name, "auxiliaryMimeTypes")) {
			auxiliary_mime_types = gf_strdup(att->name);
		}
#ifndef GPAC_DISABLE_ZLIB
		else if (!stricmp(att->name, "gzipDictionary")) {
			u64 d_size;
			if (stricmp(att->value, "self")) {
				char *url = gf_url_concatenate(import->in_name, att->value);
				FILE *d = gf_fopen(url ? url : att->value, "rb");
				if (url) gf_free(url);
				if (!d) {
					gf_import_message(import, GF_IO_ERR, "Cannot open dictionary file %s", att->value);
					continue;
				}
				gf_fseek(d, 0, SEEK_END);
				d_size = gf_ftell(d);
				dictionary = (char*)gf_malloc(sizeof(char)*(size_t)(d_size+1));
				gf_fseek(d, 0, SEEK_SET);
				d_size = fread(dictionary, sizeof(char), (size_t)d_size, d);
				dictionary[d_size]=0;
			}
			use_dict = GF_TRUE;
		}
#endif
		/*unknown desc related*/
		else if (!stricmp(att->name, "compressorName")) {
			strcpy(sdesc.compressor_name, att->value);
		} else if (!stricmp(att->name, "codecVersion")) {
			NHML_SCAN_INT("%hu", sdesc.version)
		} else if (!stricmp(att->name, "codecRevision")) {
			NHML_SCAN_INT("%hu", sdesc.revision)
		} else if (!stricmp(att->name, "codecVendor") && (strlen(att->value)==4)) {
			sdesc.vendor_code = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
		} else if (!stricmp(att->name, "temporalQuality")) {
			NHML_SCAN_INT("%u", sdesc.temporal_quality)
		} else if (!stricmp(att->name, "spatialQuality")) {
			NHML_SCAN_INT("%u", sdesc.spatial_quality)
		} else if (!stricmp(att->name, "horizontalResolution")) {
			NHML_SCAN_INT("%u", sdesc.h_res)
		} else if (!stricmp(att->name, "verticalResolution")) {
			NHML_SCAN_INT("%u", sdesc.v_res)
		} else if (!stricmp(att->name, "bitDepth")) {
			NHML_SCAN_INT("%hu", sdesc.depth)
		} else if (!stricmp(att->name, "bitsPerSample")) {
			NHML_SCAN_INT("%hu", sdesc.bits_per_sample)
		}
		/*DIMS stuff*/
		else if (!stricmp(att->name, "profile")) {
			NHML_SCAN_INT("%c", dims.profile)
		} else if (!stricmp(att->name, "level")) {
			NHML_SCAN_INT("%c", dims.level)
		} else if (!stricmp(att->name, "pathComponents")) {
			NHML_SCAN_INT("%c", dims.pathComponents)
		} else if (!stricmp(att->name, "useFullRequestHost") && !stricmp(att->value, "yes")) {
			dims.fullRequestHost = GF_TRUE;
		} else if (!stricmp(att->name, "stream_type") && !stricmp(att->value, "secondary")) {
			dims.streamType = GF_FALSE;
		} else if (!stricmp(att->name, "contains_redundant")) {
			if (!stricmp(att->value, "main")) {
				dims.containsRedundant = 1;
			} else if (!stricmp(att->value, "redundant")) {
				dims.containsRedundant = 2;
			} else if (!stricmp(att->value, "main+redundant")) {
				dims.containsRedundant = 3;
			}
		} else if (!stricmp(att->name, "text_encoding") || !stricmp(att->name, "encoding")) {
			dims.textEncoding = att->value;
		} else if (!stricmp(att->name, "content_encoding")) {
			if (!strcmp(att->value, "deflate")) {
				dims.contentEncoding = att->value;
				do_compress = GF_TRUE;
			}
		} else if (!stricmp(att->name, "content_script_types")) {
			dims.content_script_types = att->value;
		} else if (!stricmp(att->name, "mime_type")) {
			dims.mime_type = att->value;
		} else if (!stricmp(att->name, "media_namespace")) {
			dims.mime_type = att->value;
		} else if (!stricmp(att->name, "media_schema_location")) {
			dims.xml_schema_loc = att->value;
		} else if (!stricmp(att->name, "xml_namespace")) {
			dims.mime_type = att->value;
		} else if (!stricmp(att->name, "xml_schema_location")) {
			dims.xml_schema_loc = att->value;
		} else if (!stricmp(att->name, "xmlHeaderEnd")) {
			strcpy(szXmlHeaderEnd, att->value);
		}
	}
	if (sdesc.samplerate && !timescale) {
		timescale = sdesc.samplerate;
	}
	if (!sdesc.bits_per_sample) {
		sdesc.bits_per_sample = 16;
	}

	if (dims_doc || (sdesc.codec_tag==GF_ISOM_SUBTYPE_3GP_DIMS)) {
		mtype = GF_ISOM_MEDIA_DIMS;
		sdesc.codec_tag=GF_ISOM_SUBTYPE_3GP_DIMS;
		is_dims = GF_TRUE;
		streamType = 0;
		import->flags &= ~GF_IMPORT_USE_DATAREF;
	}

	mdia = gf_fopen(szMedia, "rb");

	specInfoSize = 0;
	if (!streamType && !mtype && !sdesc.codec_tag) {
		e = gf_import_message(import, GF_NOT_SUPPORTED, "Error parsing %s file - StreamType or MediaType not specified", szImpName);
		goto exit;
	}

	info = gf_fopen(szInfo, "rb");
	if (info) {
		gf_fseek(info, 0, SEEK_END);
		specInfoSize = (u32) gf_ftell(info);
		specInfo = (char*)gf_malloc(sizeof(char) * (specInfoSize+1));
		gf_fseek(info, 0, SEEK_SET);
		specInfoSize = (u32) fread(specInfo, sizeof(char), specInfoSize, info);
		specInfo[specInfoSize] = 0;
		gf_fclose(info);
	} else if (header_end) {
		/* for text based streams, the decoder specific info can be at the beginning of the file */
		specInfoSize = header_end;
		specInfo = (char*)gf_malloc(sizeof(char) * (specInfoSize+1));
		specInfoSize = (u32) fread(specInfo, sizeof(char), specInfoSize, mdia);
		specInfo[specInfoSize] = 0;
		header_end = specInfoSize;
	} else if (strlen(szXmlHeaderEnd)) {
		/* for XML based streams, the decoder specific info can be up to some element in the file */
		samp = gf_isom_sample_new();
		max_size = 0;
		strcpy(szXmlFrom, "doc.start");
		e = gf_import_sample_from_xml(import, samp, szMedia, szXmlFrom, szXmlHeaderEnd, &max_size);
		if (e) {
			gf_isom_sample_del(&samp);
			goto exit;
		}
		specInfo = (char*)gf_malloc(sizeof(char) * (samp->dataLength+1));
		memcpy(specInfo, samp->data, samp->dataLength);
		specInfoSize = samp->dataLength;
		specInfo[specInfoSize] = 0;
		gf_isom_sample_del(&samp);
	}

	i=0;
	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		if (node->type) continue;
		if (stricmp(node->name, "DecoderSpecificInfo") ) continue;

		e = gf_xml_parse_bit_sequence(node, &specInfo, &specInfoSize);
		if (e) goto exit;
		break;
	}


	/*compressing samples, remove data ref*/
	if (do_compress) import->flags &= ~GF_IMPORT_USE_DATAREF;

	if (streamType) {
		if (!import->esd) {
			import->esd = gf_odf_desc_esd_new(2);
			destroy_esd = GF_TRUE;
			import->esd->ESID = tkID;
		}
		/*update stream type/oti*/
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->decoderConfig->streamType = streamType;
		import->esd->decoderConfig->objectTypeIndication = oti;

		if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
		import->esd->decoderConfig->decoderSpecificInfo = NULL;

		import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		import->esd->decoderConfig->decoderSpecificInfo->dataLength = specInfoSize;
		import->esd->decoderConfig->decoderSpecificInfo->data = specInfo;
		specInfo = NULL;
		specInfoSize = 0;
		import->esd->slConfig->timestampResolution = timescale;


		switch (import->esd->decoderConfig->streamType) {
		case GF_STREAM_SCENE:
			mtype = GF_ISOM_MEDIA_SCENE;
			break;
		case GF_STREAM_VISUAL:
			mtype = GF_ISOM_MEDIA_VISUAL;
#ifndef GPAC_DISABLE_AV_PARSERS
			if (import->esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
				GF_M4VDecSpecInfo dsi;
				if (!import->esd->decoderConfig->decoderSpecificInfo) {
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
				e = gf_m4v_get_config(import->esd->decoderConfig->decoderSpecificInfo->data, import->esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				sdesc.width = dsi.width;
				sdesc.height = dsi.height;
				if (e) goto exit;
			}
#endif
			break;
		case GF_STREAM_AUDIO:
			mtype = GF_ISOM_MEDIA_AUDIO;
			if (!sdesc.samplerate) sdesc.samplerate = 44100;
			if (!sdesc.nb_channels) sdesc.nb_channels = 2;
			break;
		case GF_STREAM_MPEG7:
			mtype = GF_ISOM_MEDIA_MPEG7;
			break;
		case GF_STREAM_IPMP:
			mtype = GF_ISOM_MEDIA_IPMP;
			break;
		case GF_STREAM_OCI:
			mtype = GF_ISOM_MEDIA_OCI;
			break;
		case GF_STREAM_MPEGJ:
			mtype = GF_ISOM_MEDIA_MPEGJ;
			break;
		/*note we cannot import OD from NHNT*/
		case GF_STREAM_OD:
			e = GF_NOT_SUPPORTED;
			goto exit;
		case GF_STREAM_INTERACT:
			mtype = GF_ISOM_MEDIA_SCENE;
			break;
		default:
			if (!mtype) mtype = GF_ISOM_MEDIA_ESM;
			break;
		}

		track = gf_isom_new_track(import->dest, import->esd->ESID, mtype, timescale);
		if (!track) {
			e = gf_isom_last_error(import->dest);
			goto exit;
		}
		e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? szMedia : NULL, NULL, &di);
		if (e) goto exit;

		gf_import_message(import, GF_OK, "NHML import - Stream Type %s - ObjectTypeIndication 0x%02x", gf_odf_stream_type_name(import->esd->decoderConfig->streamType), import->esd->decoderConfig->objectTypeIndication);

	} else if (is_dims) {
		track = gf_isom_new_track(import->dest, tkID, mtype, timescale);
		if (!track) {
			e = gf_isom_last_error(import->dest);
			goto exit;
		}
		e = gf_isom_new_dims_description(import->dest, track, &dims, NULL, NULL, &di);
		if (e) goto exit;

		gf_import_message(import, GF_OK, "3GPP DIMS import");
	} else if (mtype == GF_ISOM_MEDIA_MPEG_SUBT || mtype == GF_ISOM_MEDIA_SUBT || mtype == GF_ISOM_MEDIA_TEXT) {
		track = gf_isom_new_track(import->dest, tkID, mtype, timescale);
		if (!track) {
			e = gf_isom_last_error(import->dest);
			goto exit;
		}
		if (sdesc.codec_tag == GF_ISOM_SUBTYPE_STPP) {
			e = gf_isom_new_xml_subtitle_description(import->dest, track,
			        dims.mime_type, dims.xml_schema_loc, auxiliary_mime_types,
			        &di);
		} else if (sdesc.codec_tag == GF_ISOM_SUBTYPE_SBTT) {
			e = gf_isom_new_stxt_description(import->dest, track, GF_ISOM_SUBTYPE_SBTT,
			                                 dims.mime_type, dims.contentEncoding, specInfo,
			                                 &di);
		} else if (sdesc.codec_tag == GF_ISOM_SUBTYPE_STXT) {
			e = gf_isom_new_stxt_description(import->dest, track, GF_ISOM_SUBTYPE_STXT,
			                                 dims.mime_type, dims.contentEncoding, specInfo,
			                                 &di);
		} else {
			e = GF_NOT_SUPPORTED;
		}
		if (e) goto exit;
	} else if (mtype == GF_ISOM_MEDIA_META) {
		track = gf_isom_new_track(import->dest, tkID, mtype, timescale);
		if (!track) {
			e = gf_isom_last_error(import->dest);
			goto exit;
		}
		if(sdesc.codec_tag == GF_ISOM_SUBTYPE_METX) {
			e = gf_isom_new_xml_metadata_description(import->dest, track,
			        dims.mime_type, dims.xml_schema_loc, dims.textEncoding,
			        &di);
		} else if (sdesc.codec_tag == GF_ISOM_SUBTYPE_METT) {
			e = gf_isom_new_stxt_description(import->dest, track, GF_ISOM_SUBTYPE_METT,
			                                 dims.mime_type, dims.textEncoding, specInfo,
			                                 &di);
		} else {
			e = GF_NOT_SUPPORTED;
		}
		if (e) goto exit;
	} else {
		char szT[5];
		sdesc.extension_buf = specInfo;
		sdesc.extension_buf_size = specInfoSize;
		if (!sdesc.vendor_code) sdesc.vendor_code = GF_VENDOR_GPAC;

		track = gf_isom_new_track(import->dest, tkID, mtype, timescale);
		if (!track) {
			e = gf_isom_last_error(import->dest);
			goto exit;
		}

		e = gf_isom_new_generic_sample_description(import->dest, track, (import->flags & GF_IMPORT_USE_DATAREF) ? szMedia : NULL, NULL, &sdesc, &di);
		if (e) goto exit;

		strcpy(szT, gf_4cc_to_str(mtype));
		gf_import_message(import, GF_OK, "NHML import - MediaType \"%s\" - SubType \"%s\"", szT, gf_4cc_to_str(sdesc.codec_tag));
	}

	gf_isom_set_track_enabled(import->dest, track, 1);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);
	if (import->esd && !import->esd->ESID) import->esd->ESID = import->final_trackID;

	if (sdesc.width && sdesc.height) {
		u32 w = sdesc.width;
		gf_isom_set_visual_info(import->dest, track, di, sdesc.width, sdesc.height);
		if (par_den && par_num) {
			gf_media_change_par(import->dest, track, par_num, par_den, GF_FALSE, GF_FALSE);
			w *= par_num;
			w /= par_den;
		} else {
			gf_media_update_par(import->dest, track);
		}
		gf_isom_set_track_layout_info(import->dest, track, w << 16, sdesc.height << 16, 0, 0, 0);
	}
	else if (sdesc.samplerate && sdesc.nb_channels) {
		gf_isom_set_audio_info(import->dest, track, di, sdesc.samplerate, sdesc.nb_channels, (u8) sdesc.bits_per_sample, import->asemode);
	}

	duration = (u64) ( ((Double) import->duration)/ 1000 * timescale);

	samp = gf_isom_sample_new();
	samp->data = (char*)gf_malloc(sizeof(char) * 1024);
	max_size = 1024;
	count = 0;
	media_size = 0;
	if (mdia) {
		gf_fseek(mdia, 0, SEEK_END);
		media_size = gf_ftell(mdia);
		gf_fseek(mdia, 0, SEEK_SET);
	}
	/* if we've read the header from the same file, mark the header data as used */
	media_done = header_end;

	sample_duration = 0;
	samp->IsRAP = RAP;
	i=0;
	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		u32 j, dims_flags, sap_type;
		Bool append, compress, has_subbs;
		char *base_data = NULL;
		if (node->type) continue;
		if (stricmp(node->name, szSampleName) ) continue;

		strcpy(szMediaTemp, "");
		strcpy(szXmlFrom, "");
		strcpy(szXmlTo, "");

		/*by default handle all samples as contiguous*/
		offset = 0;
		samp->dataLength = 0;
		dims_flags = 0;
		append = GF_FALSE;
		compress = do_compress;
		sample_duration = 0;
		sap_type = 0;

		j=0;
		while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!stricmp(att->name, "DTS") || !stricmp(att->name, "time")) {
				u32 h, m, s, ms;
				u64 dst_val;
				if (strchr(att->value, ':') && sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					samp->DTS = (u64) ( (Double) ( ((h*3600.0 + m*60.0 + s)*1000 + ms) / 1000.0) * timescale );
				} else if (sscanf(att->value, ""LLU, &dst_val)==1) {
					samp->DTS = dst_val;
				}
			}
			else if (!stricmp(att->name, "CTSOffset")) samp->CTS_Offset = atoi(att->value);
			else if (!stricmp(att->name, "isRAP") && !samp->IsRAP) {
				samp->IsRAP = (!stricmp(att->value, "yes")) ? RAP : RAP_NO;
			}
			else if (!stricmp(att->name, "isSyncShadow")) samp->IsRAP = !stricmp(att->value, "yes") ? RAP_REDUNDANT : RAP_NO;
			else if (!stricmp(att->name, "SAPType") && !samp->IsRAP) sap_type = atoi(att->value);
			else if (!stricmp(att->name, "mediaOffset")) offset = (s64) atof(att->value) ;
			else if (!stricmp(att->name, "dataLength")) samp->dataLength = atoi(att->value);
			else if (!stricmp(att->name, "mediaFile")) {
				if (!strncmp(att->value, "data:", 5)) {
					char *base = strstr(att->value, "base64,");
					if (!base) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHML import] Data encoding scheme not recognized - skipping\n"));
					} else {
						base_data = att->value;
					}
				} else {
					char *url = gf_url_concatenate(import->in_name, att->value);
					strcpy(szMediaTemp, url ? url : att->value);
					if (url) gf_free(url);
				}
			}
			else if (!stricmp(att->name, "xmlFrom")) strcpy(szXmlFrom, att->value);
			else if (!stricmp(att->name, "xmlTo")) strcpy(szXmlTo, att->value);
			/*DIMS flags*/
			else if (!stricmp(att->name, "is-Scene") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_S;
			else if (!stricmp(att->name, "is-RAP") && !stricmp(att->value, "yes")) {
				dims_flags |= GF_DIMS_UNIT_M;
				samp->IsRAP = RAP;
			}
			else if (!stricmp(att->name, "is-redundant") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_I;
			else if (!stricmp(att->name, "redundant-exit") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_D;
			else if (!stricmp(att->name, "priority") && !stricmp(att->value, "high"))
				dims_flags |= GF_DIMS_UNIT_P;
			else if (!stricmp(att->name, "compress") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_C;
			else if (!stricmp(att->name, "duration") )
				sscanf(att->value, ""LLU, &sample_duration);
		}
		if (samp->IsRAP==RAP)
			dims_flags |= GF_DIMS_UNIT_M;
		if (!count && samp->DTS) samp->DTS = 0;

		if (!(dims_flags & GF_DIMS_UNIT_C)) compress = GF_FALSE;
		count++;

		has_subbs = GF_FALSE;
		j=0;
		while ((childnode = (GF_XMLNode *) gf_list_enum(node->content, &j))) {
			if (childnode->type) continue;
			if (!stricmp(childnode->name, "BS")) {
				has_subbs = GF_TRUE;
				break;
			}
		}


		if (import->flags & GF_IMPORT_USE_DATAREF) {
			if (offset) offset = media_done;
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else if (strlen(szXmlFrom) && strlen(szXmlTo)) {
			char *xml_file;
			if (strlen(szMediaTemp)) xml_file = szMediaTemp;
			else xml_file = szMedia;
			samp->dataLength = max_size;
			e = gf_import_sample_from_xml(import, samp, xml_file, szXmlFrom, szXmlTo, &max_size);
		} else if (is_dims && !strlen(szMediaTemp)) {
			GF_BitStream *bs;
			char *content = gf_xml_dom_serialize(node, GF_TRUE);

			samp->dataLength = 3 + (u32) strlen(content);

			if (samp->dataLength>max_size) {
				samp->data = (char*)gf_realloc(samp->data, sizeof(char) * samp->dataLength);
				max_size = samp->dataLength;
			}

			bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_WRITE);
			gf_bs_write_u16(bs, samp->dataLength-2);
			gf_bs_write_u8(bs, (u8) dims_flags);
			gf_bs_write_data(bs, content, (samp->dataLength-3));
			gf_free(content);
			gf_bs_del(bs);
			/*same DIMS unit*/
			if (gf_isom_get_sample_from_dts(import->dest, track, samp->DTS))
				append = GF_TRUE;

		} else if (has_subbs) {
			if (samp->data) gf_free(samp->data );
			samp->data = 0;
			samp->dataLength = 0;
			e = gf_xml_parse_bit_sequence(node, &samp->data, &samp->dataLength);
			max_size = samp->dataLength;
		} else if (base_data) {
			char *start = strchr(base_data, ',');
			if (start) {
				u32 len = (u32)strlen(start+1);
				if (len>max_size) {
					max_size=len;
					samp->data = gf_realloc(samp->data, sizeof(char)*max_size);
				}
				samp->dataLength = gf_base64_decode(start, len, samp->data, len);
			}
		} else {
			Bool close = GF_FALSE, has_subsamples = GF_FALSE;
			FILE *f = mdia;

			j = 0;
			while ((childnode = (GF_XMLNode *)gf_list_enum(node->content, &j))) {
				if (childnode->type) continue;
				if (!stricmp(childnode->name, szSubSampleName)) {
					has_subsamples = GF_TRUE;
					break;
				}
			}
			//JLF: not sure why this test is here, it could be usefull to describe subsamples but using data source
			//from sample or baseMediaFile ...
			if (has_subsamples && (mdia != NULL) ) {
				e = gf_import_message(import, GF_BAD_PARAM, "%s import failure: you shall have either mediaFile (sample) or subsamples. Aborting.", szImpName);
				goto exit;
			}

			if (strlen(szMediaTemp)) {
				f = gf_fopen(szMediaTemp, "rb");
				if (!f) {
					e = gf_import_message(import, GF_BAD_PARAM, "%s import failure: file %s not found", szImpName, szMediaTemp);
					goto exit;
				}
				close = GF_TRUE;
				if (offset) gf_fseek(f, offset, SEEK_SET);
			} else {
				if (!offset) offset = media_done;
			}

			if (f) {
				if (!samp->dataLength) {
					//u64 cur_pos = gf_ftell(f);
					gf_fseek(f, 0, SEEK_END);
					assert(gf_ftell(f) < 1<<31);
					samp->dataLength = (u32) gf_ftell(f);
					//not needed, seek override below : gf_fseek(f, cur_pos, SEEK_SET);
				}
				gf_fseek(f, offset, SEEK_SET);

				if (is_dims) {
					u32 read;
					GF_BitStream *bs;
					if (samp->dataLength+3>max_size) {
						samp->data = (char*)gf_realloc(samp->data, sizeof(char) * (samp->dataLength+3));
						max_size = samp->dataLength+3;
					}
					bs = gf_bs_new(samp->data, samp->dataLength+3, GF_BITSTREAM_WRITE);
					gf_bs_write_u16(bs, samp->dataLength+1);
					gf_bs_write_u8(bs, (u8) dims_flags);
					read = (u32) fread( samp->data+3, sizeof(char), samp->dataLength, f);
					if (samp->dataLength != read) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHML import dims] Failed to fully read sample: dataLength %d read %d\n", samp->dataLength, read));
					}
					gf_bs_del(bs);
					samp->dataLength+=3;

					/*same DIMS unit*/
					if (gf_isom_get_sample_from_dts(import->dest, track, samp->DTS))
						append = GF_TRUE;
				} else {
					u32 read;
					if (samp->dataLength>max_size) {
						samp->data = (char*)gf_realloc(samp->data, sizeof(char) * samp->dataLength);
						max_size = samp->dataLength;
					}
					read = (u32) fread(samp->data, sizeof(char), samp->dataLength, f);
					if (samp->dataLength != read) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHML import] Failed to fully read sample: dataLength %d read %d\n", samp->dataLength, read));
					}
				}
				if (close) gf_fclose(f);
			}
		}
		if (e) goto exit;

		if (is_dims) {
			if (strstr(samp->data+3, "svg ")) dims_flags |= GF_DIMS_UNIT_S;
			if (dims_flags & GF_DIMS_UNIT_S) dims_flags |= GF_DIMS_UNIT_P;
			samp->data[2] = dims_flags;
		}

		if (compress) {
#ifndef GPAC_DISABLE_ZLIB
			e = compress_sample_data(samp, &max_size, use_dict ? &dictionary : NULL, is_dims ? 3 : 0);
			if (e) goto exit;
			if (is_dims) {
				GF_BitStream *bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_WRITE);
				gf_bs_write_u16(bs, samp->dataLength-2);
				gf_bs_del(bs);
			}
#else
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: your version of GPAC was compile with no libz support. Abort."));
			e = GF_NOT_SUPPORTED;
			goto exit;
#endif
		}
		if (is_dims && (samp->dataLength > 0xFFFF)) {
			e = gf_import_message(import, GF_BAD_PARAM, "DIMS import failure: sample data is too long - maximum size allowed: 65532 bytes");
			goto exit;
		}


		if ((samp->IsRAP==RAP_REDUNDANT) && !is_dims) {
			e = gf_isom_add_sample_shadow(import->dest, track, samp);
		} else if (append) {
			e = gf_isom_append_sample_data(import->dest, track, samp->data, samp->dataLength);
		} else {
			e = gf_isom_add_sample(import->dest, track, di, samp);
			if (e) goto exit;

			j = 0;
			while ((childnode = (GF_XMLNode *)gf_list_enum(node->content, &j))) {
				if (childnode->type) continue;
				if (!stricmp(childnode->name, szSubSampleName)) {
					u32 k = 0;
					while ((att = (GF_XMLAttribute *)gf_list_enum(childnode->attributes, &k))) {
						if (!stricmp(att->name, "mediaFile")) {
							u32 subsMediaFileSize = 0;
							char *subsMediaFileData = NULL;
							char *sub_file_url = gf_url_concatenate(import->in_name, att->value);
							FILE *f = sub_file_url ? gf_fopen(sub_file_url, "rb") : NULL;
							if (sub_file_url) gf_free(sub_file_url);

							if (!f) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: mediaFile \"%s\" not found for subsample. Abort.\n", att->value));
								e = GF_BAD_PARAM;
								goto exit;
							}
							gf_fseek(f, 0, SEEK_END);
							assert(gf_ftell(f) < (1 << 31));
							subsMediaFileSize = (u32)gf_ftell(f);
							subsMediaFileData = gf_malloc(subsMediaFileSize);
							gf_fseek(f, 0, SEEK_SET);
							gf_fread(subsMediaFileData, 1, subsMediaFileSize, f);
							gf_fclose(f);
							e = gf_isom_add_subsample(import->dest, track, gf_isom_get_sample_count(import->dest, track), 0, subsMediaFileSize, 0, 0, GF_FALSE);
							if (e) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: couldn't add subsample (mediaFile=\"%s\", size=%u. Abort.\n", att->value, subsMediaFileSize));
								gf_free(subsMediaFileData);
								goto exit;
							}
							e = gf_isom_append_sample_data(import->dest, track, subsMediaFileData, subsMediaFileSize);
							if (e) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: couldn't append subsample data (mediaFile=\"%s\", size=%u. Abort.\n", att->value, subsMediaFileSize));
								gf_free(subsMediaFileData);
								goto exit;
							}
							gf_free(subsMediaFileData);
						}
					}
				}
			}
		}
		if (sap_type==SAP_TYPE_3) {
			gf_isom_set_sample_rap_group(import->dest, track, gf_isom_get_sample_count(import->dest, track), 0);
		}
		samp->IsRAP = RAP_NO;
		samp->CTS_Offset = 0;
		if (sample_duration)
			samp->DTS += sample_duration;
		else
			samp->DTS += dts_inc;
		media_done += samp->dataLength;
		gf_set_progress(is_dims ? "Importing DIMS" : "Importing NHML", (u32) media_done, (u32) (media_size ? media_size : media_done+1) );
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}

	if (sample_duration) {
		gf_isom_set_last_sample_duration(import->dest, track, (u32) sample_duration);
	}

	if (media_done!=media_size) gf_set_progress(is_dims ? "Importing DIMS" : "Importing NHML", (u32) media_size, (u32) media_size);
	gf_media_update_bitrate(import->dest, track);

	if (inRootOD) gf_isom_add_track_to_root_od(import->dest, track);

exit:
	gf_fclose(nhml);
	if (samp) {
		samp->dataLength = 1;
		gf_isom_sample_del(&samp);
	}
	if (mdia) gf_fclose(mdia);
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	gf_xml_dom_del(parser);
	if (specInfo) gf_free(specInfo);
	if (dictionary) gf_free(dictionary);
	if (auxiliary_mime_types) gf_free(auxiliary_mime_types);
	return e;
}


GF_Err gf_import_amr_evrc_smv(GF_MediaImporter *import)
{
	GF_Err e;
	u32 track, trackID, di, sample_rate, block_size, i, read;
	GF_ISOSample *samp;
	char magic[20], *msg;
	Bool delete_esd, update_gpp_cfg;
	u32 media_done, mtype, oti, nb_frames;
	u64 offset, media_size, duration;
	GF_3GPConfig gpp_cfg;
	FILE *mdia;
	msg = NULL;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_AUDIO;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_FORCE_MPEG4 | GF_IMPORT_3GPP_AGGREGATION;
		return GF_OK;
	}

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Cannot find file %s", import->in_name);

	update_gpp_cfg = GF_FALSE;
	oti = mtype = 0;
	sample_rate = 8000;
	block_size = 160;
	if (6 > fread(magic, sizeof(char), 20, mdia)) {
		gf_fclose(mdia);
		return gf_import_message(import, GF_URL_ERROR, "Cannot guess type for file %s, size lower than 6", import->in_name);

	}
	if (!strnicmp(magic, "#!AMR\n", 6)) {
		gf_import_message(import, GF_OK, "Importing AMR Audio");
		gf_fseek(mdia, 6, SEEK_SET);
		mtype = GF_ISOM_SUBTYPE_3GP_AMR;
		update_gpp_cfg = GF_TRUE;
		msg = "Importing AMR";
	}
	else if (!strnicmp(magic, "#!EVRC\n", 7)) {
		gf_import_message(import, GF_OK, "Importing EVRC Audio");
		gf_fseek(mdia, 7, SEEK_SET);
		mtype = GF_ISOM_SUBTYPE_3GP_EVRC;
		oti = GPAC_OTI_AUDIO_EVRC_VOICE;
		msg = "Importing EVRC";
	}
	else if (!strnicmp(magic, "#!SMV\n", 6)) {
		gf_import_message(import, GF_OK, "Importing SMV Audio");
		gf_fseek(mdia, 6, SEEK_SET);
		mtype = GF_ISOM_SUBTYPE_3GP_SMV;
		oti = GPAC_OTI_AUDIO_SMV_VOICE;
		msg = "Importing SMV";
	}
	else if (!strnicmp(magic, "#!AMR_MC1.0\n", 12)) {
		gf_fclose(mdia);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Multichannel AMR Audio Not Supported");
	}
	else if (!strnicmp(magic, "#!AMR-WB\n", 9)) {
		gf_import_message(import, GF_OK, "Importing AMR WideBand Audio");
		gf_fseek(mdia, 9, SEEK_SET);
		mtype = GF_ISOM_SUBTYPE_3GP_AMR_WB;
		sample_rate = 16000;
		block_size = 320;
		update_gpp_cfg = GF_TRUE;
		msg = "Importing AMR-WB";
	}
	else if (!strnicmp(magic, "#!AMR-WB_MC1.0\n", 15)) {
		gf_fclose(mdia);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Multichannel AMR WideBand Audio Not Supported");
	}
	else {
		char *ext = strrchr(import->in_name, '.');
		if (ext && !stricmp(ext, ".amr")) {
			mtype = GF_ISOM_SUBTYPE_3GP_AMR;
			update_gpp_cfg = GF_TRUE;
			ext = "AMR";
			msg = "Importing AMR";
		}
		else if (ext && !stricmp(ext, ".evc")) {
			mtype = GF_ISOM_SUBTYPE_3GP_EVRC;
			oti = GPAC_OTI_AUDIO_EVRC_VOICE;
			ext = "EVRC";
			msg = "Importing EVRC";
		}
		else if (ext && !stricmp(ext, ".smv")) {
			mtype = GF_ISOM_SUBTYPE_3GP_SMV;
			oti = GPAC_OTI_AUDIO_SMV_VOICE;
			ext = "SMV";
			msg = "Importing SMV";
		}
		else {
			gf_fclose(mdia);
			return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Corrupted AMR/SMV/EVRC file header");
		}

		gf_fseek(mdia, 0, SEEK_SET);
		gf_import_message(import, GF_OK, "Importing %s Audio (File header corrupted, missing \"#!%s\\n\")", ext, ext);
	}

	delete_esd = GF_FALSE;
	trackID = 0;

	if (import->esd) trackID = import->esd->ESID;

	track = gf_isom_new_track(import->dest, trackID, GF_ISOM_MEDIA_AUDIO, sample_rate);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);

	memset(&gpp_cfg, 0, sizeof(GF_3GPConfig));
	gpp_cfg.type = mtype;
	gpp_cfg.vendor = GF_VENDOR_GPAC;
	gpp_cfg.frames_per_sample = import->frames_per_sample;
	if (!gpp_cfg.frames_per_sample) gpp_cfg.frames_per_sample  = 1;
	else if (gpp_cfg.frames_per_sample >15) gpp_cfg.frames_per_sample = 15;

	if (import->flags & GF_IMPORT_USE_DATAREF) gpp_cfg.frames_per_sample  = 1;


	if (oti && (import->flags & GF_IMPORT_FORCE_MPEG4)) {
		if (!import->esd) {
			delete_esd = GF_TRUE;
			import->esd = gf_odf_desc_esd_new(2);
			import->esd->ESID = trackID;
		}
		import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		import->esd->decoderConfig->objectTypeIndication = oti;
		e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
		if (e) goto exit;
	} else {
		import->flags &= ~GF_IMPORT_FORCE_MPEG4;
		e = gf_isom_3gp_config_new(import->dest, track, &gpp_cfg, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
		if (e) goto exit;
	}
	gf_isom_set_audio_info(import->dest, track, di, sample_rate, 1, 16, import->asemode);
	duration = import->duration;
	duration *= sample_rate;
	duration /= 1000;

	samp = gf_isom_sample_new();
	samp->data = (char*)gf_malloc(sizeof(char) * 200);
	samp->IsRAP = RAP;
	offset = gf_ftell(mdia);
	gf_fseek(mdia, 0, SEEK_END);
	media_size = gf_ftell(mdia) - offset;
	gf_fseek(mdia, offset, SEEK_SET);

	media_done = 0;
	nb_frames = 0;

	while (!feof(mdia)) {
		u8 ft, toc;

		offset = gf_ftell(mdia);
		toc = fgetc(mdia);
		switch (gpp_cfg.type) {
		case GF_ISOM_SUBTYPE_3GP_AMR:
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
			ft = (toc >> 3) & 0x0F;
			if (gpp_cfg.type==GF_ISOM_SUBTYPE_3GP_AMR_WB) {
				samp->dataLength = (u32)GF_AMR_WB_FRAME_SIZE[ft];
			} else {
				samp->dataLength = (u32)GF_AMR_FRAME_SIZE[ft];
			}
			samp->data[0] = toc;
			if (samp->dataLength) {
				/*update mode set (same mechanism for both AMR and AMR-WB*/
				gpp_cfg.AMR_mode_set |= (1<<ft);
			}
			break;
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_SMV:
			for (i=0; i<GF_SMV_EVRC_RATE_TO_SIZE_NB; i++) {
				if (GF_SMV_EVRC_RATE_TO_SIZE[2*i]==toc) {
					/*remove rate_type byte*/
					samp->dataLength = (u32)GF_SMV_EVRC_RATE_TO_SIZE[2*i+1] - 1;
					break;
				}
			}
			if (!samp->dataLength) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Corrupted TOC (%d)", toc);
				goto exit;
			}
			samp->data[0] = toc;
			break;
		}

		if (samp->dataLength) {
			read = (u32) fread( samp->data + 1, sizeof(char), samp->dataLength, mdia);
			if (read != samp->dataLength) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Failed to read logs");
				goto exit;
			}
		}
		samp->dataLength += 1;
		/*if last frame is "no data", abort - this happens in many files with constant mode (ie constant files), where
		adding this last frame will result in a non-compact version of the stsz table, hence a bigger file*/
		if ((samp->dataLength==1) && feof(mdia))
			break;


		if (import->flags & GF_IMPORT_USE_DATAREF) {
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else if (!nb_frames) {
			e = gf_isom_add_sample(import->dest, track, di, samp);
		} else {
			e = gf_isom_append_sample_data(import->dest, track, samp->data, samp->dataLength);
		}
		if (e) goto exit;
		nb_frames++;
		if (nb_frames==gpp_cfg.frames_per_sample) nb_frames=0;

		samp->DTS += block_size;
		media_done += samp->dataLength;
		gf_set_progress(msg, (u32) media_done, (u32) media_size);
		if (duration && (samp->DTS > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
	}
	gf_isom_sample_del(&samp);
	gf_isom_refresh_size_info(import->dest, track);

	if (import->flags & GF_IMPORT_FORCE_MPEG4) gf_media_update_bitrate(import->dest, track);

	if (update_gpp_cfg) gf_isom_3gp_config_update(import->dest, track, &gpp_cfg, 1);

exit:
	if (delete_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	gf_fclose(mdia);
	return e;
}

/*QCP codec GUIDs*/
static const char *QCP_QCELP_GUID_1 = "\x41\x6D\x7F\x5E\x15\xB1\xD0\x11\xBA\x91\x00\x80\x5F\xB4\xB9\x7E";
static const char *QCP_QCELP_GUID_2 = "\x42\x6D\x7F\x5E\x15\xB1\xD0\x11\xBA\x91\x00\x80\x5F\xB4\xB9\x7E";
static const char *QCP_EVRC_GUID = "\x8D\xD4\x89\xE6\x76\x90\xB5\x46\x91\xEF\x73\x6A\x51\x00\xCE\xB4";
static const char *QCP_SMV_GUID = "\x75\x2B\x7C\x8D\x97\xA7\x46\xED\x98\x5E\xD5\x3C\x8C\xC7\x5F\x84";

GF_Err gf_import_qcp(GF_MediaImporter *import)
{
	GF_Err e;
	u32 track, trackID, di, i, nb_pck, riff_size, chunk_size, pck_size, block_size, bps, samplerate, vrat_rate_flag, size_in_packets, nb_frames;
	u64 max_size, duration;
	GF_BitStream *bs;
	GF_ISOSample *samp;
	char magic[12], GUID[16], name[81], fmt[162];
	u32 rtable_cnt;
	u64 offset, media_size, media_done;
	Bool has_pad;
	QCPRateTable rtable[8];
	Bool delete_esd;
	GF_3GPConfig gpp_cfg;
	FILE *mdia;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_AUDIO;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_FORCE_MPEG4 | GF_IMPORT_3GPP_AGGREGATION;
		return GF_OK;
	}

	memset(&gpp_cfg, 0, sizeof(GF_3GPConfig));
	gpp_cfg.vendor = GF_VENDOR_GPAC;
	delete_esd = GF_FALSE;

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Cannot find file %s", import->in_name);

	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "RIFF", 4)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Broken QCP file: RIFF header not found");
		goto exit;
	}
	riff_size = gf_bs_read_u32_le(bs);
	gf_bs_read_data(bs, fmt, 162);
	gf_bs_seek(bs, 8);
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "QLCM", 4)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Broken QCP file: QLCM header not found");
		goto exit;
	}
	max_size = gf_bs_get_size(bs);
	if (max_size != riff_size+8) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Broken QCP file: Expecting RIFF-Size %d got %d", max_size-8, riff_size);
		goto exit;
	}
	/*fmt*/
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "fmt ", 4)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Broken QCP file: FMT not found");
		goto exit;
	}
	chunk_size = gf_bs_read_u32_le(bs);
	has_pad = (chunk_size%2) ? GF_TRUE : GF_FALSE;
	/*major = */gf_bs_read_u8(bs);
	/*minor = */gf_bs_read_u8(bs);
	chunk_size -= 2;
	/*codec info*/
	gf_bs_read_data(bs, GUID, 16);
	/*version = */gf_bs_read_u16_le(bs);
	chunk_size -= 18;
	gf_bs_read_data(bs, name, 80);
	name[80]=0;
	chunk_size -= 80;
	/*avg_bps = */gf_bs_read_u16_le(bs);
	pck_size = gf_bs_read_u16_le(bs);
	block_size = gf_bs_read_u16_le(bs);
	samplerate = gf_bs_read_u16_le(bs);
	bps = gf_bs_read_u16_le(bs);
	rtable_cnt = gf_bs_read_u32_le(bs);
	chunk_size -= 14;
	/*skip var rate*/
	for (i=0; i<8; i++) {
		rtable[i].pck_size = gf_bs_read_u8(bs);
		rtable[i].rate_idx = gf_bs_read_u8(bs);
	}
	chunk_size -= 16;
	gf_bs_skip_bytes(bs, 5*4);/*reserved*/
	chunk_size -= 20;
	gf_bs_skip_bytes(bs, chunk_size);
	if (has_pad) gf_bs_read_u8(bs);

	if (!strncmp(GUID, QCP_QCELP_GUID_1, 16) || !strncmp(GUID, QCP_QCELP_GUID_2, 16)) {
		gpp_cfg.type = GF_ISOM_SUBTYPE_3GP_QCELP;
		strcpy(name, "QCELP-13K");
	} else if (!strncmp(GUID, QCP_EVRC_GUID, 16)) {
		gpp_cfg.type = GF_ISOM_SUBTYPE_3GP_EVRC;
		strcpy(name, "EVRC");
	} else if (!strncmp(GUID, QCP_SMV_GUID, 16)) {
		gpp_cfg.type = GF_ISOM_SUBTYPE_3GP_SMV;
		strcpy(name, "SMV");
	} else {
		gpp_cfg.type = 0;
	}
	/*vrat*/
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "vrat", 4)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Broken QCP file: VRAT not found");
		goto exit;
	}
	chunk_size = gf_bs_read_u32_le(bs);
	has_pad = (chunk_size%2) ? GF_TRUE : GF_FALSE;
	vrat_rate_flag = gf_bs_read_u32_le(bs);
	size_in_packets = gf_bs_read_u32_le(bs);
	chunk_size -= 8;
	gf_bs_skip_bytes(bs, chunk_size);
	if (has_pad) gf_bs_read_u8(bs);

	if (!gpp_cfg.type) {
		e = gf_import_message(import, GF_NOT_SUPPORTED, "Unknown QCP file codec %s", name);
		goto exit;
	}

	gf_import_message(import, GF_OK, "Importing %s Audio - SampleRate %d", name, samplerate);

	trackID = 0;

	if (import->esd) trackID = import->esd->ESID;

	track = gf_isom_new_track(import->dest, trackID, GF_ISOM_MEDIA_AUDIO, samplerate);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);

	if (import->flags & GF_IMPORT_FORCE_MPEG4) {
		if (!import->esd) {
			import->esd = gf_odf_desc_esd_new(2);
			delete_esd = GF_TRUE;
		}
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig*)gf_odf_desc_new(GF_ODF_DCD_TAG);
		import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		switch (gpp_cfg.type) {
		case GF_ISOM_SUBTYPE_3GP_QCELP:
			import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_13K_VOICE;
			/*DSI is fmt*/
			if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
			if (import->esd->decoderConfig->decoderSpecificInfo->data) gf_free(import->esd->decoderConfig->decoderSpecificInfo->data);
			import->esd->decoderConfig->decoderSpecificInfo->dataLength = 162;
			import->esd->decoderConfig->decoderSpecificInfo->data = (char*)gf_malloc(sizeof(char)*162);
			memcpy(import->esd->decoderConfig->decoderSpecificInfo->data, fmt, 162);
			break;
		case GF_ISOM_SUBTYPE_3GP_EVRC:
			import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_EVRC_VOICE;
			break;
		case GF_ISOM_SUBTYPE_3GP_SMV:
			import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_SMV_VOICE;
			break;
		}
		e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
		if (e) goto exit;
	} else {
		if (import->frames_per_sample<=1) import->frames_per_sample=1;
		else if (import->frames_per_sample>15) import->frames_per_sample=15;
		gpp_cfg.frames_per_sample = import->frames_per_sample;
		e = gf_isom_3gp_config_new(import->dest, track, &gpp_cfg, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
		if (e) goto exit;
	}
	gf_isom_set_audio_info(import->dest, track, di, samplerate, 1, (u8) bps, import->asemode);

	duration = import->duration;
	duration *= samplerate;
	duration /= 1000;

	samp = gf_isom_sample_new();
	samp->data = (char*)gf_malloc(sizeof(char) * 200);
	samp->IsRAP = RAP;
	max_size = 200;
	offset = gf_ftell(mdia);
	gf_fseek(mdia, 0, SEEK_END);
	media_size = gf_ftell(mdia) - offset;
	gf_fseek(mdia, offset, SEEK_SET);

	nb_pck = 0;
	media_done = 0;
	nb_frames = 0;
	while (gf_bs_available(bs)) {
		gf_bs_read_data(bs, magic, 4);
		chunk_size = gf_bs_read_u32_le(bs);
		has_pad = (chunk_size%2) ? GF_TRUE : GF_FALSE;
		/*process chunk by chunk*/
		if (!strnicmp(magic, "data", 4)) {

			while (chunk_size) {
				u32 idx = 0;
				u32 size = 0;

				offset = gf_bs_get_position(bs);
				/*get frame rate idx*/
				if (vrat_rate_flag) {
					idx = gf_bs_read_u8(bs);
					chunk_size-=1;
					for (i=0; i<rtable_cnt; i++) {
						if (rtable[i].rate_idx==idx) {
							size = rtable[i].pck_size;
							break;
						}
					}
					samp->dataLength = size+1;
				} else {
					size = pck_size;
					samp->dataLength = size;
				}
				if (max_size<samp->dataLength) {
					samp->data = (char*)gf_realloc(samp->data, sizeof(char)*samp->dataLength);
					max_size=samp->dataLength;
				}
				if (import->flags & GF_IMPORT_USE_DATAREF) {
					e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
					gf_bs_skip_bytes(bs, size);
				} else {
					if (vrat_rate_flag) {
						samp->data[0] = idx;
						gf_bs_read_data(bs, &samp->data[1], size);
					} else {
						gf_bs_read_data(bs, samp->data, size);
					}
					if (!nb_frames) {
						e = gf_isom_add_sample(import->dest, track, di, samp);
					} else {
						e = gf_isom_append_sample_data(import->dest, track, samp->data, samp->dataLength);
					}
					nb_frames++;
					if (nb_frames==import->frames_per_sample) nb_frames=0;
				}
				if (e) goto exit;
				chunk_size -= size;
				samp->DTS += block_size;
				if (size_in_packets) {
					gf_set_progress("Importing QCP", (u32) nb_pck, (u32) size_in_packets);
				} else {
					gf_set_progress("Importing QCP", (u32) media_done, (u32) media_size);
				}
				nb_pck++;
				media_done += samp->dataLength;
				if (duration && (samp->DTS > duration)) break;
				if (import->flags & GF_IMPORT_DO_ABORT) break;
			}
		} else if (!strnicmp(magic, "labl", 4)) {
		} else if (!strnicmp(magic, "offs", 4)) {
		} else if (!strnicmp(magic, "cnfg", 4)) {
		} else if (!strnicmp(magic, "text", 4)) {
		}
		gf_bs_skip_bytes(bs, chunk_size);
		if (has_pad) gf_bs_read_u8(bs);
	}
	gf_isom_sample_del(&samp);
	gf_isom_set_brand_info(import->dest, GF_ISOM_BRAND_3G2A, 65536);
	if (import->flags & GF_IMPORT_FORCE_MPEG4) gf_media_update_bitrate(import->dest, track);
	gf_set_progress("Importing QCP", size_in_packets, size_in_packets);

exit:
	if (delete_esd && import->esd) {
		gf_odf_desc_del((GF_Descriptor *)import->esd);
		import->esd = NULL;
	}
	gf_bs_del(bs);
	gf_fclose(mdia);
	return e;
}

/*read that amount of data at each IO access rather than fetching byte by byte...*/
Bool H263_IsStartCode(GF_BitStream *bs)
{
	u32 c;
	c = gf_bs_peek_bits(bs, 22, 0);
	if (c==0x20) return GF_TRUE;
	return GF_FALSE;
}

#define H263_CACHE_SIZE	4096
u32 H263_NextStartCode(GF_BitStream *bs)
{
	u32 v, bpos;
	unsigned char h263_cache[H263_CACHE_SIZE];
	u64 end, cache_start, load_size;
	u64 start = gf_bs_get_position(bs);

	/*skip 16b header*/
	gf_bs_read_u16(bs);
	bpos = 0;
	load_size = 0;
	cache_start = 0;
	end = 0;
	v = 0xffffffff;
	while (!end) {
		/*refill cache*/
		if (bpos == (u32) load_size) {
			if (!gf_bs_available(bs)) break;
			load_size = gf_bs_available(bs);
			if (load_size>H263_CACHE_SIZE) load_size=H263_CACHE_SIZE;
			bpos = 0;
			cache_start = gf_bs_get_position(bs);
			gf_bs_read_data(bs, (char *) h263_cache, (u32) load_size);
		}
		v = (v<<8) | h263_cache[bpos];
		bpos++;
		if ((v >> (32-22)) == 0x20) end = cache_start+bpos-4;
	}
	gf_bs_seek(bs, start);
	if (!end) end = gf_bs_get_size(bs);
	return (u32) (end-start);
}
static void h263_get_pic_size(GF_BitStream *bs, u32 fmt, u32 *w, u32 *h)
{
	switch (fmt) {
	case 1:
		*w = 128;
		*h = 96;
		break;
	case 2:
		*w = 176;
		*h = 144;
		break;
	case 3:
		*w = 352;
		*h = 288;
		break;
	case 4:
		*w = 704;
		*h = 576;
		break;
	case 5:
		*w = 1409;
		*h = 1152 ;
		break;
	default:
		*w = *h = 0;
		break;
	}
}

GF_Err gf_import_h263(GF_MediaImporter *import)
{
	GF_Err e;
	u32 track, trackID, di, max_size, timescale, w, h, fmt, nb_samp, dts_inc;
	u64 offset, media_size, media_done, duration;
	GF_ISOSample *samp;
	char *samp_data;
	GF_3GPConfig gpp_cfg;
	Double FPS;
	FILE *mdia;
	GF_BitStream *bs;

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Cannot find file %s", import->in_name);

	e = GF_OK;
	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);
	if (!H263_IsStartCode(bs)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Cannot find H263 Picture Start Code");
		goto exit;
	}
	/*no auto frame-rate detection*/
	if (import->video_fps == GF_IMPORT_AUTO_FPS)
		import->video_fps = GF_IMPORT_DEFAULT_FPS;

	FPS = (Double) import->video_fps;
	/*for H263 we use 15 fps by default!!*/
	if (!FPS) FPS = 15;
	get_video_timing(FPS, &timescale, &dts_inc);

	/*parse header*/
	gf_bs_read_int(bs, 22);
	gf_bs_read_int(bs, 8);
	/*spare+0+split_screen_indicator+document_camera_indicator+freeze_picture_release*/
	gf_bs_read_int(bs, 5);

	fmt = gf_bs_read_int(bs, 3);
	h263_get_pic_size(bs, fmt, &w, &h);
	if (!w || !h) {
		e = gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported H263 frame header");
		goto exit;
	}
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_OVERRIDE_FPS;
		import->tk_info[0].video_info.width = w;
		import->tk_info[0].video_info.height = h;
		goto exit;
	}

	trackID = 0;

	if (import->esd) {
		trackID = import->esd->ESID;
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig*) gf_odf_desc_new(GF_ODF_SLC_TAG);
	}
	track = gf_isom_new_track(import->dest, trackID, GF_ISOM_MEDIA_VISUAL, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);

	memset(&gpp_cfg, 0, sizeof(GF_3GPConfig));
	gpp_cfg.type = GF_ISOM_SUBTYPE_3GP_H263;
	gpp_cfg.vendor = GF_VENDOR_GPAC;
	/*FIXME - we need more in-depth parsing of the bitstream to detect P3@L10 (streaming wireless)*/
	gpp_cfg.H263_profile = 0;
	gpp_cfg.H263_level = 10;
	e = gf_isom_3gp_config_new(import->dest, track, &gpp_cfg, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	if (e) goto exit;
	gf_isom_set_visual_info(import->dest, track, di, w, h);
	gf_import_message(import, GF_OK, "Importing H263 video - %d x %d @ %02.4f", w, h, FPS);

	samp = gf_isom_sample_new();

	duration = (u64) ( ((Double)import->duration) * timescale / 1000.0);
	media_size = gf_bs_get_size(bs);
	nb_samp = 0;
	media_done = 0;

	max_size = 4096;
	samp_data = (char*)gf_malloc(sizeof(char)*max_size);
	gf_bs_seek(bs, 0);
	offset = 0;
	while (gf_bs_available(bs)) {
		samp->dataLength = H263_NextStartCode(bs);
		if (samp->dataLength>max_size) {
			max_size = samp->dataLength;
			samp_data = (char*)gf_realloc(samp_data, sizeof(char)*max_size);
		}
		gf_bs_read_data(bs, samp_data, samp->dataLength);
		/*we ignore pict number and import at const FPS*/
		samp->IsRAP = (samp_data[4]&0x02) ? RAP_NO : RAP;
		samp->data = samp_data;
		if (import->flags & GF_IMPORT_USE_DATAREF) {
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, offset);
		} else {
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		if (e) goto exit;
		samp->data = NULL;
		samp->DTS += dts_inc;
		nb_samp ++;
		offset += samp->dataLength;
		gf_set_progress("Importing H263", media_done, media_size);
		media_done += samp->dataLength;

		if ((duration && (samp->DTS > duration) ) || (import->flags & GF_IMPORT_DO_ABORT)) {
			break;
		}
	}
	gf_free(samp_data);
	gf_isom_sample_del(&samp);
	gf_set_progress("Importing H263", nb_samp, nb_samp);
	gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_3GG6, 1);
	gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_3GG5, 1);

exit:
	gf_bs_del(bs);
	gf_fclose(mdia);
	return e;
}

GF_EXPORT
GF_Err gf_media_avc_rewrite_samples(GF_ISOFile *file, u32 track, u32 prev_size, u32 new_size)
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

#ifndef GPAC_DISABLE_AV_PARSERS

static GF_Err gf_import_avc_h264(GF_MediaImporter *import)
{
	u64 nal_start, nal_end, total_size;
	u32 nal_size, track, trackID, di, cur_samp, nb_i, nb_idr, nb_p, nb_b, nb_sp, nb_si, nb_sei, max_w, max_h, max_total_delay, nb_nalus;
	s32 idx, sei_recovery_frame_count;
	u64 duration;
	u8 nal_type;
	GF_Err e;
	FILE *mdia;
	AVCState avc;
	GF_AVCConfigSlot *slc;
	GF_AVCConfig *avccfg, *svccfg, *dstcfg;
	GF_BitStream *bs;
	GF_BitStream *sample_data;
	Bool flush_sample, sample_is_rap, sample_has_islice, sample_has_slice, is_islice, first_nal, slice_is_ref, has_cts_offset, detect_fps, is_paff, set_subsamples, slice_force_ref;
	u32 ref_frame, timescale, copy_size, size_length, dts_inc;
	s32 last_poc, max_last_poc, max_last_b_poc, poc_diff, prev_last_poc, min_poc, poc_shift;
	Bool first_avc;
	u32 use_opengop_gdr = 0;
	u32 last_svc_sps;
	u32 prev_nalu_prefix_size, res_prev_nalu_prefix;
	u8 priority_prev_nalu_prefix;
	Double FPS;
	char *buffer;
	Bool sample_is_ref, has_redundant;
	u32 max_size = 4096;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_OVERRIDE_FPS | GF_IMPORT_FORCE_PACKED;
		return GF_OK;
	}

	set_subsamples = (import->flags & GF_IMPORT_SET_SUBSAMPLES) ? GF_TRUE : GF_FALSE;

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Cannot find file %s", import->in_name);

	detect_fps = GF_TRUE;
	FPS = (Double) import->video_fps;
	if (!FPS) {
		FPS = GF_IMPORT_DEFAULT_FPS;
	} else {
		if (import->video_fps == GF_IMPORT_AUTO_FPS)
			import->video_fps = GF_IMPORT_DEFAULT_FPS; /*fps=auto is handled as auto-detection in h264*/
		else
			detect_fps = GF_FALSE;                     /*fps is forced by the caller*/
	}
	get_video_timing(FPS, &timescale, &dts_inc);

	poc_diff = 0;


restart_import:

	memset(&avc, 0, sizeof(AVCState));
	avc.sps_active_idx = -1;
	avccfg = gf_odf_avc_cfg_new();
	svccfg = gf_odf_avc_cfg_new();
	/*we don't handle split import (one track / layer)*/
	svccfg->complete_representation = 1;
	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	sample_data = NULL;
	first_avc = GF_TRUE;
	last_svc_sps = 0;
	sei_recovery_frame_count = -1;
	sample_is_ref = GF_FALSE;
	has_redundant = GF_FALSE;

	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);
	if (!gf_media_nalu_is_start_code(bs)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Cannot find H264 start code");
		goto exit;
	}

	/*NALU size packing disabled*/
	if (!(import->flags & GF_IMPORT_FORCE_PACKED)) size_length = 32;
	/*if import in edit mode, use smallest NAL size and adjust on the fly*/
	else if (gf_isom_get_mode(import->dest)!=GF_ISOM_OPEN_WRITE) size_length = 8;
	else size_length = 32;

	trackID = 0;

	if (import->esd) trackID = import->esd->ESID;

	track = gf_isom_new_track(import->dest, trackID, GF_ISOM_MEDIA_VISUAL, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);
	if (import->esd && import->esd->dependsOnESID) {
		gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_DECODE, import->esd->dependsOnESID);
	}

	e = gf_isom_avc_config_new(import->dest, track, avccfg, NULL, NULL, &di);
	if (e) goto exit;

	gf_isom_set_nalu_extract_mode(import->dest, track, GF_ISOM_NALU_EXTRACT_INSPECT);

	sample_data = NULL;
	sample_is_rap = GF_FALSE;
	sample_has_islice = GF_FALSE;
	sample_has_slice = GF_FALSE;
	cur_samp = 0;
	is_paff = GF_FALSE;
	total_size = gf_bs_get_size(bs);
	nal_start = gf_bs_get_position(bs);
	duration = (u64) ( ((Double)import->duration) * timescale / 1000.0);

	nb_i = nb_idr = nb_p = nb_b = nb_sp = nb_si = nb_sei = 0;
	max_w = max_h = 0;
	first_nal = GF_TRUE;
	ref_frame = 0;
	last_poc = max_last_poc = max_last_b_poc = prev_last_poc = 0;
	max_total_delay = 0;

	gf_isom_set_cts_packing(import->dest, track, GF_TRUE);
	has_cts_offset = GF_FALSE;
	min_poc = 0;
	poc_shift = 0;
	prev_nalu_prefix_size = 0;
	res_prev_nalu_prefix = 0;
	priority_prev_nalu_prefix = 0;
	nb_nalus = 0;

	while (gf_bs_available(bs)) {
		u8 nal_hdr, skip_nal, is_subseq, add_sps, nal_ref_idc;
		u32 nal_and_trailing_size;

		nal_and_trailing_size = nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (!(import->flags & GF_IMPORT_KEEP_TRAILING)) {
			nal_size = gf_media_nalu_payload_end_bs(bs);
		}

		if (nal_size>max_size) {
			buffer = (char*)gf_realloc(buffer, sizeof(char)*nal_size);
			max_size = nal_size;
		}

		/*read the file, and work on a memory buffer*/
		gf_bs_read_data(bs, buffer, nal_size);

		gf_bs_seek(bs, nal_start);
		nal_hdr = gf_bs_read_u8(bs);
		nal_type = nal_hdr & 0x1F;

		is_subseq = 0;
		skip_nal = 0;
		copy_size = flush_sample = GF_FALSE;
		is_islice = GF_FALSE;

		if (nal_type == GF_AVC_NALU_SVC_SUBSEQ_PARAM || nal_type == GF_AVC_NALU_SVC_PREFIX_NALU || nal_type == GF_AVC_NALU_SVC_SLICE) {
			avc.is_svc = GF_TRUE;
		}
		nb_nalus ++;

		switch (gf_media_avc_parse_nalu(bs, nal_hdr, &avc)) {
		case 1:
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				if (sample_has_slice) flush_sample = GF_TRUE;
			} else {
				flush_sample = GF_TRUE;
			}
			break;
		case -1:
			gf_import_message(import, GF_OK, "Warning: Error parsing NAL unit");
			skip_nal = 1;
			break;
		case -2:
			skip_nal = 1;
			break;
		default:
			break;
		}
		switch (nal_type) {
		case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
			is_subseq = 1;
		case GF_AVC_NALU_SEQ_PARAM:
			idx = gf_media_avc_read_sps(buffer, nal_size, &avc, is_subseq, NULL);
			if (idx<0) {
				if (avc.sps[0].profile_idc) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Error parsing SeqInfo"));
				} else {
					e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing SeqInfo");
					goto exit;
				}
			}
			add_sps = 0;
			dstcfg = (import->flags & GF_IMPORT_SVC_EXPLICIT) ? svccfg : avccfg;
			if (is_subseq) {
				if ((avc.sps[idx].state & AVC_SUBSPS_PARSED) && !(avc.sps[idx].state & AVC_SUBSPS_DECLARED)) {
					avc.sps[idx].state |= AVC_SUBSPS_DECLARED;
					add_sps = 1;
					avc.sps[idx].sbusps_crc = gf_crc_32(buffer, nal_size);
				}
				dstcfg = svccfg;
				if (import->flags & GF_IMPORT_SVC_NONE) {
					add_sps = 0;
				}
			} else {
				if ((avc.sps[idx].state & AVC_SPS_PARSED) && !(avc.sps[idx].state & AVC_SPS_DECLARED)) {
					avc.sps[idx].state |= AVC_SPS_DECLARED;
					add_sps = 1;
				}
			}
			if (avc.sps[idx].state & AVC_SUBSPS_DECLARED) {
				if (import->flags & GF_IMPORT_SVC_NONE) {
					copy_size = 0;
				} else {
					/*some streams are not really nice and reuse sps idx with differnet parameters (typically
					when concatenated bitstreams). Since we cannot put two SPS with the same idx in the decoder config, we keep them in the
					video bitstream*/
					if (avc.sps[idx].sbusps_crc != gf_crc_32(buffer, nal_size) ) {
						copy_size = nal_size;
					}
				}
			}

			//always keep NAL
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				copy_size = nal_size;
				if (sample_has_slice) flush_sample = GF_TRUE;
			}

			//first declaration of SPS,
			if (add_sps) {
				dstcfg->configurationVersion = 1;
				dstcfg->profile_compatibility = avc.sps[idx].prof_compat;
				dstcfg->AVCProfileIndication = avc.sps[idx].profile_idc;
				dstcfg->AVCLevelIndication = avc.sps[idx].level_idc;

				dstcfg->chroma_format = avc.sps[idx].chroma_format;
				dstcfg->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
				dstcfg->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;
				/*try to patch ?*/
				if (!gf_avc_is_rext_profile(dstcfg->AVCProfileIndication)
					&& ((dstcfg->chroma_format>1) || (dstcfg->luma_bit_depth>8) || (dstcfg->chroma_bit_depth>8))
				) {
					if ((dstcfg->luma_bit_depth>8) || (dstcfg->chroma_bit_depth>8)) {
						dstcfg->AVCProfileIndication=110;
					} else {
						dstcfg->AVCProfileIndication = (dstcfg->chroma_format==3) ? 244 : 122;
					}
				}

				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					copy_size = nal_size;
				} else {
					slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
					slc->size = nal_size;
					slc->id = idx;
					slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
					memcpy(slc->data, buffer, sizeof(char)*slc->size);
					gf_list_add(dstcfg->sequenceParameterSets, slc);
				}

				/*disable frame rate scan, most bitstreams have wrong values there*/
				if (detect_fps && avc.sps[idx].vui.timing_info_present_flag
				        /*if detected FPS is greater than 1000, assume wrong timing info*/
				        && (avc.sps[idx].vui.time_scale <= 1000*avc.sps[idx].vui.num_units_in_tick)
				   ) {
					/*ISO/IEC 14496-10 n11084 Table E-6*/
					/* not used :				u8 DeltaTfiDivisorTable[] = {1,1,1,2,2,2,2,3,3,4,6}; */
					u8 DeltaTfiDivisorIdx;
					if (!avc.sps[idx].vui.pic_struct_present_flag) {
						DeltaTfiDivisorIdx = 1 + (1-avc.s_info.field_pic_flag);
					} else {
						if (!avc.sei.pic_timing.pic_struct)
							DeltaTfiDivisorIdx = 2;
						else if (avc.sei.pic_timing.pic_struct == 8)
							DeltaTfiDivisorIdx = 6;
						else
							DeltaTfiDivisorIdx = (avc.sei.pic_timing.pic_struct+1) / 2;
					}
					timescale = 2 * avc.sps[idx].vui.time_scale;
					dts_inc =   2 * avc.sps[idx].vui.num_units_in_tick * DeltaTfiDivisorIdx;
					FPS = (Double)timescale / dts_inc;
					detect_fps = GF_FALSE;

					if (!avc.sps[idx].vui.fixed_frame_rate_flag)
						GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[avc-h264] Possible Variable Frame Rate: VUI \"fixed_frame_rate_flag\" absent.\n"));

					gf_isom_remove_track(import->dest, track);
					if (sample_data) gf_bs_del(sample_data);
					gf_odf_avc_cfg_del(avccfg);
					avccfg = NULL;
					gf_odf_avc_cfg_del(svccfg);
					svccfg = NULL;
					gf_free(buffer);
					buffer = NULL;
					gf_bs_del(bs);
					bs = NULL;
					gf_fseek(mdia, 0, SEEK_SET);
					goto restart_import;
				}

				if (is_subseq) {
					if (last_svc_sps<(u32) idx) {
						if (import->flags & GF_IMPORT_SVC_EXPLICIT) {
							gf_import_message(import, GF_OK, "SVC-H264 import - frame size %d x %d at %02.3f FPS", avc.sps[idx].width, avc.sps[idx].height, FPS);
						} else {
							gf_import_message(import, GF_OK, "SVC Detected - SSPS ID %d - frame size %d x %d", idx-GF_SVC_SSPS_ID_SHIFT, avc.sps[idx].width, avc.sps[idx].height);
						}
						last_svc_sps = idx;
					}
					/* prevent from adding the subseq PS to the samples */
					copy_size = 0;
				} else {
					if (first_avc) {
						first_avc = GF_FALSE;
						if (!(import->flags & GF_IMPORT_SVC_EXPLICIT)) {
							gf_import_message(import, GF_OK, "AVC-H264 import - frame size %d x %d at %02.3f FPS", avc.sps[idx].width, avc.sps[idx].height, FPS);
						}
					}
				}
				if (!is_subseq || (import->flags & GF_IMPORT_SVC_EXPLICIT)) {
					if ((max_w <= avc.sps[idx].width) && (max_h <= avc.sps[idx].height)) {
						max_w = avc.sps[idx].width;
						max_h = avc.sps[idx].height;
					}
				}
			}
			break;
		case GF_AVC_NALU_PIC_PARAM:
			idx = gf_media_avc_read_pps(buffer, nal_size, &avc);
			if (idx<0) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing Picture Param");
				goto exit;
			}
			/*some streams are not really nice and reuse sps idx with differnet parameters (typically
			when concatenated bitstreams). Since we cannot put two SPS with the same idx in the decoder config, we keep them in the
			video bitstream - if same CRC for the PPS, this is the same PPS, don't copy over*/
			if (avc.pps[idx].status > 1) {
				u32 pps_crc = gf_crc_32(buffer, nal_size);
				if (pps_crc != avc.pps[idx].status) {
					copy_size = nal_size;
				}
			}

			//always keep NAL
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				copy_size = nal_size;
				if (sample_has_slice) flush_sample = GF_TRUE;
			} else {
				if (avc.pps[idx].status==1) {
					avc.pps[idx].status = gf_crc_32(buffer, nal_size);
					slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
					slc->size = nal_size;
					slc->id = idx;
					slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
					memcpy(slc->data, buffer, sizeof(char)*slc->size);

					/* by default, we put all PPS in the base AVC layer,
					  they will be moved to the SVC layer upon analysis of SVC slice. */
					//dstcfg = (import->flags & GF_IMPORT_SVC_EXPLICIT) ? svccfg : avccfg;
					dstcfg = avccfg;

					if (import->flags & GF_IMPORT_SVC_EXPLICIT)
						dstcfg = svccfg;

					gf_list_add(dstcfg->pictureParameterSets, slc);
				}
			}
			break;
		case GF_AVC_NALU_SEI:
			if (import->flags & GF_IMPORT_NO_SEI) {
				copy_size = 0;
			} else {
				if (avc.sps_active_idx != -1) {
					copy_size = gf_media_avc_reformat_sei(buffer, nal_size, &avc);
				} else {
					//if no state yet, import SEI
					copy_size = nal_size;
				}
				if (copy_size)
					nb_sei++;
			}
			break;

		case GF_AVC_NALU_NON_IDR_SLICE:
		case GF_AVC_NALU_DP_A_SLICE:
		case GF_AVC_NALU_DP_B_SLICE:
		case GF_AVC_NALU_DP_C_SLICE:
		case GF_AVC_NALU_IDR_SLICE:
			if (!skip_nal) {
				copy_size = nal_size;
				switch (avc.s_info.slice_type) {
				case GF_AVC_TYPE_P:
				case GF_AVC_TYPE2_P:
					nb_p++;
					break;
				case GF_AVC_TYPE_I:
				case GF_AVC_TYPE2_I:
					nb_i++;
					is_islice = GF_TRUE;
					break;
				case GF_AVC_TYPE_B:
				case GF_AVC_TYPE2_B:
					nb_b++;
					break;
				case GF_AVC_TYPE_SP:
				case GF_AVC_TYPE2_SP:
					nb_sp++;
					break;
				case GF_AVC_TYPE_SI:
				case GF_AVC_TYPE2_SI:
					nb_si++;
					break;
				}
			}
			break;

		/*remove*/
		case GF_AVC_NALU_ACCESS_UNIT:
			if (import->keep_audelim) {
				copy_size = nal_size;
			} else {
				copy_size = 0;
			}
			break;
		case GF_AVC_NALU_FILLER_DATA:
		case GF_AVC_NALU_END_OF_SEQ:
		case GF_AVC_NALU_END_OF_STREAM:
			break;

		case GF_AVC_NALU_SVC_PREFIX_NALU:
			if (import->flags & GF_IMPORT_SVC_NONE) {
				copy_size = 0;
				break;
			}
			copy_size = nal_size;
			break;
		case GF_AVC_NALU_SVC_SLICE:
		{
			u32 i;
			for (i = 0; i < gf_list_count(avccfg->pictureParameterSets); i ++) {
				slc = (GF_AVCConfigSlot*)gf_list_get(avccfg->pictureParameterSets, i);
				if (avc.s_info.pps->id == slc->id) {
					/* This PPS is used by an SVC NAL unit, it should be moved to the SVC Config Record) */
					gf_list_rem(avccfg->pictureParameterSets, i);
					i--;
					gf_list_add(svccfg->pictureParameterSets, slc);
				}
			}
		}
		if (import->flags & GF_IMPORT_SVC_NONE) {
			copy_size = 0;
			break;
		}
		if (! skip_nal) {
			copy_size = nal_size;
			switch (avc.s_info.slice_type) {
			case GF_AVC_TYPE_P:
			case GF_AVC_TYPE2_P:
				avc.s_info.sps->nb_ep++;
				break;
			case GF_AVC_TYPE_I:
			case GF_AVC_TYPE2_I:
				avc.s_info.sps->nb_ei++;
				break;
			case GF_AVC_TYPE_B:
			case GF_AVC_TYPE2_B:
				avc.s_info.sps->nb_eb++;
				break;
			}
		}
		break;

		case GF_AVC_NALU_SEQ_PARAM_EXT:
			idx = gf_media_avc_read_sps_ext(buffer, nal_size);
			if (idx<0 || idx>31) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing Sequence Param Extension");
				goto exit;
			}

			if (! (avc.sps[idx].state & AVC_SPS_EXT_DECLARED)) {
				avc.sps[idx].state |= AVC_SPS_EXT_DECLARED;

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				slc->size = nal_size;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				memcpy(slc->data, buffer, sizeof(char)*slc->size);

				if (!avccfg->sequenceParameterSetExtensions)
					avccfg->sequenceParameterSetExtensions = gf_list_new();

				gf_list_add(avccfg->sequenceParameterSetExtensions, slc);
			}
			break;

		case GF_AVC_NALU_SLICE_AUX:

		default:
			gf_import_message(import, GF_OK, "WARNING: NAL Unit type %d not handled - adding", nal_type);
			copy_size = nal_size;
			break;
		}

		if (!nal_size) break;

		if (flush_sample && sample_data) {
			Bool is_rap=GF_FALSE;;
			GF_ISOSample *samp = gf_isom_sample_new();
			samp->DTS = (u64)dts_inc*cur_samp;
			samp->IsRAP = sample_is_rap ? RAP : RAP_NO;
			if (!sample_is_rap) {
				if (sample_has_islice && (import->flags & GF_IMPORT_FORCE_SYNC) && (sei_recovery_frame_count==0)) {
					samp->IsRAP = RAP;
					if (!use_opengop_gdr) {
						use_opengop_gdr = 1;
						GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AVC Import] Forcing non-IDR samples with I slices to be marked as sync points - resulting file will not be ISO conformant\n"));
					}
				}
			}
			gf_bs_get_content(sample_data, &samp->data, &samp->dataLength);
			gf_bs_del(sample_data);
			sample_data = NULL;

			if (prev_nalu_prefix_size) {
				u32 size, reserved, nb_subs;
				u8 priority;
				Bool discardable;

				samp->dataLength -= size_length/8 + prev_nalu_prefix_size;

				if (set_subsamples) {
					/* determine the number of subsamples */
					nb_subs = gf_isom_sample_has_subsamples(import->dest, track, cur_samp+1, 0);
					if (nb_subs) {
						/* fetch size, priority, reserved and discardable info for last subsample */
						gf_isom_sample_get_subsample(import->dest, track, cur_samp+1, 0, nb_subs, &size, &priority, &reserved, &discardable);

						/*remove last subsample entry!*/
						gf_isom_add_subsample(import->dest, track, cur_samp+1, 0, 0, 0, 0, GF_FALSE);
					}
				}

				/*rewrite last NALU prefix at the beginning of next sample*/
				sample_data = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				gf_bs_write_data(sample_data, samp->data + samp->dataLength, size_length/8 + prev_nalu_prefix_size);

				if (set_subsamples) {
					/*add subsample entry to next sample*/
					gf_isom_add_subsample(import->dest, track, cur_samp+2, 0, size_length/8 + prev_nalu_prefix_size, priority, reserved, discardable);
				}

				prev_nalu_prefix_size = 0;
			}
			/*CTS recomuting is much trickier than with MPEG-4 ASP due to b-slice used as references - we therefore
			store the POC as the CTS offset and update the whole table at the end*/
			samp->CTS_Offset = last_poc - poc_shift;
			assert(last_poc >= poc_shift);
			e = gf_isom_add_sample(import->dest, track, di, samp);
			if (e) goto exit;

			sample_has_slice = GF_FALSE;
			cur_samp++;

			if (samp->IsRAP) is_rap = GF_TRUE;

			/*write sampleGroups info*/
			if (!samp->IsRAP && ( (sei_recovery_frame_count>=0) || sample_has_islice) ) {
				/*generic GDR*/
				if (sei_recovery_frame_count > 0) {
					if (!use_opengop_gdr) use_opengop_gdr = 1;
					e = gf_isom_set_sample_roll_group(import->dest, track, cur_samp, (s16) sei_recovery_frame_count);
				}
				/*open-GOP*/
				else if ((sei_recovery_frame_count == 0) && sample_has_islice) {
					if (!use_opengop_gdr) use_opengop_gdr = 2;
					e = gf_isom_set_sample_rap_group(import->dest, track, cur_samp, 0);
					is_rap = GF_TRUE;
				}
				if (e) goto exit;
			}
			//write sample deps
			if (import->flags & GF_IMPORT_SAMPLE_DEPS) {
				u32 isLeading, dependsOn, dependedOn, hasRedundant;
				isLeading = 0; //for avc we would need to parse sub-seq info SEI
				dependsOn = is_rap ? 2 : 1;
				dependedOn = sample_is_ref ? 1 : 2;
				hasRedundant = has_redundant ? 1 : 2;

				e = gf_isom_sample_set_dep_info(import->dest, track, cur_samp, isLeading, dependsOn, dependedOn, hasRedundant);
				if (e) goto exit;
			}
			sample_is_ref = GF_FALSE;
			has_redundant = GF_FALSE;

			gf_isom_sample_del(&samp);
			gf_set_progress("Importing AVC-H264", (u32) (nal_start/1024), (u32) (total_size/1024) );
			first_nal = GF_TRUE;

			if (min_poc > last_poc)
				min_poc = last_poc;

			sample_has_islice = GF_FALSE;
			sei_recovery_frame_count = -1;
		}


		if (copy_size) {
			//nal in current sample is a ref
			nal_ref_idc = (nal_hdr & 0x60) >> 5;
			if (nal_ref_idc) {
				sample_is_ref = GF_TRUE;
			}

			if (is_islice)
				sample_has_islice = GF_TRUE;

			if ((size_length<32) && ( (u32) (1<<size_length)-1 < copy_size)) {
				u32 diff_size = 8;
				while ((size_length<32) && ( (u32) (1<<(size_length+diff_size))-1 < copy_size)) diff_size+=8;
				/*only 8bits, 16bits and 32 bits*/
				if (size_length+diff_size == 24) diff_size+=8;

				gf_import_message(import, GF_OK, "Adjusting AVC SizeLength to %d bits", size_length+diff_size);
				gf_media_avc_rewrite_samples(import->dest, track, size_length, size_length+diff_size);

				/*rewrite current sample*/
				if (sample_data) {
					char *sd;
					u32 sd_l;
					GF_BitStream *prev_sd;
					gf_bs_get_content(sample_data, &sd, &sd_l);
					gf_bs_del(sample_data);
					sample_data = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
					prev_sd = gf_bs_new(sd, sd_l, GF_BITSTREAM_READ);
					while (gf_bs_available(prev_sd)) {
						char *buf;
						u32 s = gf_bs_read_int(prev_sd, size_length);
						gf_bs_write_int(sample_data, s, size_length+diff_size);
						buf = (char*)gf_malloc(sizeof(char)*s);
						gf_bs_read_data(prev_sd, buf, s);
						gf_bs_write_data(sample_data, buf, s);
						gf_free(buf);
					}
					gf_bs_del(prev_sd);
					gf_free(sd);
				}
				size_length+=diff_size;

			}
			if (!sample_data) sample_data = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_int(sample_data, copy_size, size_length);
			gf_bs_write_data(sample_data, buffer, copy_size);

			/*fixme - we need finer grain for priority*/
			if ((nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) || (nal_type==GF_AVC_NALU_SVC_SLICE)) {
				u32 res = 0;
				u8 prio;
				unsigned char *p = (unsigned char *) buffer;
				res |= (p[0] & 0x60) ? 0x80000000 : 0; // RefPicFlag
				res |= 0 ? 0x40000000 : 0;             // RedPicFlag TODO: not supported, would require to parse NAL unit payload
				res |= (1<=nal_type && nal_type<=5) || (nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) || (nal_type==GF_AVC_NALU_SVC_SLICE) ? 0x20000000 : 0;  // VclNALUnitFlag
				res |= p[1] << 16;                     // use values of IdrFlag and PriorityId directly from SVC extension header
				res |= p[2] << 8;                      // use values of DependencyId and QualityId directly from SVC extension header
				res |= p[3] & 0xFC;                    // use values of TemporalId and UseRefBasePicFlag directly from SVC extension header
				res |= 0 ? 0x00000002 : 0;             // StoreBaseRepFlag TODO: SVC FF mentions a store_base_rep_flag which cannot be found in SVC spec

				// priority_id (6 bits) in SVC has inverse meaning -> lower value means higher priority - invert it and scale it to 8 bits
				prio = (63 - (p[1] & 0x3F)) << 2;

				if (set_subsamples) {
					gf_isom_add_subsample(import->dest, track, cur_samp+1, 0, copy_size+size_length/8, prio, res, GF_TRUE);
				}

				if (nal_type==GF_AVC_NALU_SVC_PREFIX_NALU) {
					/* remember reserved and priority value */
					res_prev_nalu_prefix = res;
					priority_prev_nalu_prefix = prio;
				}
			} else if (set_subsamples) {
				/* use the res and priority value of last prefix NALU */
				gf_isom_add_subsample(import->dest, track, cur_samp+1, 0, copy_size+size_length/8, priority_prev_nalu_prefix, res_prev_nalu_prefix, GF_FALSE);
			}
			if (nal_type!=GF_AVC_NALU_SVC_PREFIX_NALU) {
				res_prev_nalu_prefix = 0;
				priority_prev_nalu_prefix = 0;
			}

			if (nal_type != GF_AVC_NALU_SVC_PREFIX_NALU) {
				prev_nalu_prefix_size = 0;
			} else {
				prev_nalu_prefix_size += nal_size;
			}

			switch (nal_type) {
			case GF_AVC_NALU_SVC_SLICE:
			case GF_AVC_NALU_NON_IDR_SLICE:
			case GF_AVC_NALU_DP_A_SLICE:
			case GF_AVC_NALU_DP_B_SLICE:
			case GF_AVC_NALU_DP_C_SLICE:
			case GF_AVC_NALU_IDR_SLICE:
			case GF_AVC_NALU_SLICE_AUX:
				sample_has_slice = GF_TRUE;
				if (!is_paff && avc.s_info.bottom_field_flag)
					is_paff = GF_TRUE;

				slice_is_ref = (avc.s_info.nal_unit_type==GF_AVC_NALU_IDR_SLICE) ? GF_TRUE : GF_FALSE;
				if (slice_is_ref)
					nb_idr++;
				slice_force_ref = GF_FALSE;

				if (avc.s_info.redundant_pic_cnt>0) {
					has_redundant = GF_TRUE;
				}

				/*we only indicate TRUE IDRs for sync samples (cf AVC file format spec).
				SEI recovery should be used to build sampleToGroup & RollRecovery tables*/
				if (first_nal) {
					first_nal = GF_FALSE;
					if (avc.sei.recovery_point.valid || (import->flags & GF_IMPORT_FORCE_SYNC)) {
						Bool bIntraSlice = gf_media_avc_slice_is_intra(&avc);
						assert(avc.s_info.nal_unit_type!=GF_AVC_NALU_IDR_SLICE || bIntraSlice);

						sei_recovery_frame_count = avc.sei.recovery_point.frame_cnt;

						/*we allow to mark I-frames as sync on open-GOPs (with sei_recovery_frame_count=0) when forcing sync even when the SEI RP is not available*/
						if (!avc.sei.recovery_point.valid && bIntraSlice) {
							sei_recovery_frame_count = 0;
							if (use_opengop_gdr == 1) {
								use_opengop_gdr = 2; /*avoid message flooding*/
								GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[AVC Import] No valid SEI Recovery Point found although needed - forcing\n"));
							}
						}
						avc.sei.recovery_point.valid = 0;
						if (bIntraSlice && (import->flags & GF_IMPORT_FORCE_SYNC) && (sei_recovery_frame_count==0))
							slice_force_ref = GF_TRUE;
					}
					sample_is_rap = gf_media_avc_slice_is_IDR(&avc);
				}

				if (avc.s_info.poc<poc_shift) {
					u32 j;
					if (ref_frame) {
						for (j=ref_frame; j<=cur_samp; j++) {
							GF_ISOSample *samp = gf_isom_get_sample_info(import->dest, track, j, NULL, NULL);
							if (!samp) break;
							samp->CTS_Offset += poc_shift;
							samp->CTS_Offset -= avc.s_info.poc;
							gf_isom_modify_cts_offset(import->dest, track, j, samp->CTS_Offset);
							gf_isom_sample_del(&samp);
						}
					}
					poc_shift = avc.s_info.poc;
				}

				/*if #pics, compute smallest POC increase*/
				if (avc.s_info.poc != last_poc) {
					if (!poc_diff || (poc_diff > abs(avc.s_info.poc-last_poc))) {
						poc_diff = abs(avc.s_info.poc-last_poc);/*ideally we would need to start the parsing again as poc_diff helps computing max_total_delay*/
					}
					last_poc = avc.s_info.poc;
				}

				/*ref slice, reset poc*/
				if (slice_is_ref) {
					ref_frame = cur_samp+1;
					max_last_poc = last_poc = max_last_b_poc = 0;
					poc_shift = 0;
				}
				/*forced ref slice*/
				else if (slice_force_ref) {
					ref_frame = cur_samp+1;
					/*adjust POC shift as sample will now be marked as sync, so wo must store poc as if IDR (eg POC=0) for our CTS offset computing to be correct*/
					poc_shift = avc.s_info.poc;
				}
				/*strictly less - this is a new P slice*/
				else if (max_last_poc<last_poc) {
					max_last_b_poc = 0;
					//prev_last_poc = max_last_poc;
					max_last_poc = last_poc;
				}
				/*stricly greater*/
				else if (max_last_poc>last_poc) {
					/*need to store TS offsets*/
					has_cts_offset = GF_TRUE;
					switch (avc.s_info.slice_type) {
					case GF_AVC_TYPE_B:
					case GF_AVC_TYPE2_B:
						if (!max_last_b_poc) {
							max_last_b_poc = last_poc;
						}
						/*if same poc than last max, this is a B-slice*/
						else if (last_poc>max_last_b_poc) {
							max_last_b_poc = last_poc;
						}
						/*otherwise we had a B-slice reference: do nothing*/

						break;
					}
				}

				/*compute max delay (applicable when B slice are present)*/
				if (ref_frame && poc_diff && (s32)(cur_samp-(ref_frame-1)-last_poc/poc_diff)>(s32)max_total_delay) {
					max_total_delay = cur_samp - (ref_frame-1) - last_poc/poc_diff;
				}
			}
		}

		gf_bs_align(bs);
		nal_end = gf_bs_get_position(bs);
		assert(nal_start <= nal_end);
		assert(nal_end <= nal_start + nal_and_trailing_size);
		if (nal_end != nal_start + nal_and_trailing_size)
			gf_bs_seek(bs, nal_start + nal_and_trailing_size);

		if (!gf_bs_available(bs)) break;
		if (duration && (dts_inc*cur_samp > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;

		/*consume next start code*/
		nal_start = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] invalid nal_size (%u)? Skipping "LLU" bytes to reach next start code\n", nal_size, nal_start));
			gf_bs_skip_bytes(bs, nal_start);
		}
		nal_start = gf_media_nalu_is_start_code(bs);
		if (!nal_start) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] error: no start code found ("LLU" bytes read out of "LLU") - leaving\n", gf_bs_get_position(bs), gf_bs_get_size(bs)));
			break;
		}
		nal_start = gf_bs_get_position(bs);
	}

	/*final flush*/
	if (sample_data) {
		GF_ISOSample *samp = gf_isom_sample_new();
		samp->DTS = (u64)dts_inc*cur_samp;
		samp->IsRAP = sample_is_rap ? RAP : RAP_NO;
		if (!sample_is_rap && sample_has_islice && (import->flags & GF_IMPORT_FORCE_SYNC)) {
			samp->IsRAP = RAP;
		}
		/*we store the frame order (based on the POC) as the CTS offset and update the whole table at the end*/
		samp->CTS_Offset = last_poc - poc_shift;
		gf_bs_get_content(sample_data, &samp->data, &samp->dataLength);
		gf_bs_del(sample_data);
		sample_data = NULL;
		e = gf_isom_add_sample(import->dest, track, di, samp);
		if (e) goto exit;
		gf_isom_sample_del(&samp);
		gf_set_progress("Importing AVC-H264", (u32) cur_samp, cur_samp+1);
		cur_samp++;

		//write sample deps
		if (import->flags & GF_IMPORT_SAMPLE_DEPS) {
			u32 isLeading, dependsOn, dependedOn;
			isLeading = 0;
			dependsOn = (sample_is_rap || sample_has_islice) ? 2 : 1;
			dependedOn = sample_is_ref ? 1 : 2;

			e = gf_isom_sample_set_dep_info(import->dest, track, cur_samp, isLeading, dependsOn, dependedOn, 2);
			if (e) goto exit;
		}
		sample_is_ref = GF_FALSE;

	}


	/*recompute all CTS offsets*/
	if (has_cts_offset) {
		u32 i, last_cts_samp;
		u64 last_dts, max_cts, min_cts, min_cts_offset;
		if (!poc_diff) poc_diff = 1;
		/*no b-frame references, no need to cope with negative poc*/
		if (!max_total_delay) {
			min_poc=0;
			max_total_delay = 1;
		}
		cur_samp = gf_isom_get_sample_count(import->dest, track);
		min_poc *= -1;
		last_dts = 0;
		max_cts = 0;
		min_cts = (u64) -1;
		min_cts_offset = (u64) -1;
		last_cts_samp = 0;

		for (i=0; i<cur_samp; i++) {
			u64 cts;
			/*not using descIdx and data_offset will only fetch DTS, CTS and RAP which is all we need*/
			GF_ISOSample *samp = gf_isom_get_sample_info(import->dest, track, i+1, NULL, NULL);
			/*poc re-init (RAP and POC to 0, otherwise that's SEI recovery), update base DTS*/
			if (samp->IsRAP /*&& !samp->CTS_Offset*/)
				last_dts = samp->DTS * (1+is_paff);

			/*CTS offset is frame POC (refers to last IDR)*/
			cts = ( (min_poc + (s32) samp->CTS_Offset) * dts_inc ) / poc_diff + (u32) last_dts;

			/*if PAFF, 2 pictures (eg poc) <=> 1 aggregated frame (eg sample), divide by 2*/
			if (is_paff) {
				cts /= 2;
				/*in some cases the poc is not on the top field - if that is the case, round up*/
				if (cts%dts_inc) {
					cts = ((cts/dts_inc)+1)*dts_inc;
				}
			}

			/*B-frames offset*/
			cts += (u32) (max_total_delay*dts_inc);

			samp->CTS_Offset = (u32) (cts - samp->DTS);

			if (samp->CTS_Offset < min_cts_offset)
				min_cts_offset = samp->CTS_Offset;

			if (max_cts < samp->DTS + samp->CTS_Offset) {
				max_cts = samp->DTS + samp->CTS_Offset;
				last_cts_samp = i;
			}
			if (min_cts >= samp->DTS + samp->CTS_Offset)
				min_cts = samp->DTS + samp->CTS_Offset;


			/*this should never happen, however some streams seem to do weird POC increases (cf sorenson streams, last 2 frames),
			this should hopefully take care of some bugs and ensure proper CTS...*/
			if ((s32)samp->CTS_Offset<0) {
				u32 j, k;
				samp->CTS_Offset = 0;
				gf_isom_modify_cts_offset(import->dest, track, i+1, samp->CTS_Offset);
				for (j=last_cts_samp; j<i; j++) {
					GF_ISOSample *asamp = gf_isom_get_sample_info(import->dest, track, j+1, NULL, NULL);
					for (k=j+1; k<=i; k++) {
						GF_ISOSample *bsamp = gf_isom_get_sample_info(import->dest, track, k+1, NULL, NULL);
						if (asamp->CTS_Offset+asamp->DTS==bsamp->CTS_Offset+bsamp->DTS) {
							max_cts += dts_inc;
							bsamp->CTS_Offset = (u32) (max_cts - bsamp->DTS);
							gf_isom_modify_cts_offset(import->dest, track, k+1, bsamp->CTS_Offset);
						}
						gf_isom_sample_del(&bsamp);
					}
					gf_isom_sample_del(&asamp);
				}
				max_cts = samp->DTS + samp->CTS_Offset;
			} else {
				gf_isom_modify_cts_offset(import->dest, track, i+1, samp->CTS_Offset);
			}
			gf_isom_sample_del(&samp);
		}

		if (min_cts_offset > 0) {
			gf_isom_shift_cts_offset(import->dest, track, (s32)min_cts_offset);
			max_cts -= min_cts_offset;
			min_cts -= min_cts_offset;
		}
		/*and repack table*/
		gf_isom_set_cts_packing(import->dest, track, GF_FALSE);

		if (!(import->flags & GF_IMPORT_NO_EDIT_LIST) && min_cts) {
			last_dts = max_cts - min_cts + gf_isom_get_sample_duration(import->dest, track, gf_isom_get_sample_count(import->dest, track) );

			last_dts *= gf_isom_get_timescale(import->dest);
			last_dts /= gf_isom_get_media_timescale(import->dest, track);
			gf_isom_set_edit_segment(import->dest, track, 0, last_dts, min_cts, GF_ISOM_EDIT_NORMAL);
		}
	} else {
		gf_isom_remove_cts_info(import->dest, track);
	}

	if (gf_isom_get_sample_count(import->dest,track) == 1) {
	    gf_isom_set_last_sample_duration(import->dest, track, dts_inc );
	}

	gf_set_progress("Importing AVC-H264", (u32) cur_samp, cur_samp);

	gf_isom_set_visual_info(import->dest, track, di, max_w, max_h);
	avccfg->nal_unit_size = size_length/8;
	svccfg->nal_unit_size = size_length/8;


	if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
		gf_isom_avc_config_update(import->dest, track, 1, avccfg);
		gf_isom_avc_set_inband_config(import->dest, track, 1);
	} else if (gf_list_count(avccfg->sequenceParameterSets) || !gf_list_count(svccfg->sequenceParameterSets) ) {
		gf_isom_avc_config_update(import->dest, track, 1, avccfg);
		if (gf_list_count(svccfg->sequenceParameterSets)) {
			gf_isom_svc_config_update(import->dest, track, 1, svccfg, GF_TRUE);
		}
	} else {
		gf_isom_svc_config_update(import->dest, track, 1, svccfg, GF_FALSE);
	}

	if (import->flags & GF_IMPORT_USE_CCST) {
		e = gf_isom_set_image_sequence_coding_constraints(import->dest, track, di, GF_FALSE, GF_FALSE, GF_TRUE, 15);
		if (e) goto exit;
	}
	if (import->is_alpha) {
		e = gf_isom_set_image_sequence_alpha(import->dest, track, di, GF_FALSE);
		if (e) goto exit;
	}

	e = gf_media_update_par(import->dest, track);
	if (e) goto exit;
	/*arbitrary: use the last active SPS*/
	if (avc.sps[avc.sps_active_idx].vui_parameters_present_flag && avc.sps[avc.sps_active_idx].vui.colour_description_present_flag) {
		e = gf_isom_set_visual_color_info(import->dest, track, di, GF_ISOM_SUBTYPE_NCLX, avc.sps[avc.sps_active_idx].vui.colour_primaries, avc.sps[avc.sps_active_idx].vui.transfer_characteristics, avc.sps[avc.sps_active_idx].vui.matrix_coefficients, avc.sps[avc.sps_active_idx].vui.video_full_range_flag, NULL, 0);
		if (e) goto exit;
	}
	gf_media_update_bitrate(import->dest, track);

	gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_VISUAL, 0x7F);
	gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_AVC1, 1);

	if (!gf_list_count(avccfg->sequenceParameterSets) && !gf_list_count(svccfg->sequenceParameterSets) && !(import->flags & GF_IMPORT_FORCE_XPS_INBAND)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Import results: No SPS or PPS found in the bitstream ! Nothing imported\n");
	} else {
		u32 i;
		if (nb_sp || nb_si) {
			gf_import_message(import, GF_OK, "AVC Import results: %d samples (%d NALUs) - Slices: %d I %d P %d B %d SP %d SI - %d SEI - %d IDR",
			                  cur_samp, nb_nalus, nb_i, nb_p, nb_b, nb_sp, nb_si, nb_sei, nb_idr);
		} else {
			gf_import_message(import, GF_OK, "AVC Import results: %d samples (%d NALUs) - Slices: %d I %d P %d B - %d SEI - %d IDR",
			                  cur_samp, nb_nalus, nb_i, nb_p, nb_b, nb_sei, nb_idr);
		}

		for (i=0; i<gf_list_count(svccfg->sequenceParameterSets); i++) {
			AVC_SPS *sps;
			GF_AVCConfigSlot *svcc = (GF_AVCConfigSlot*)gf_list_get(svccfg->sequenceParameterSets, i);
			sps = & avc.sps[svcc->id];
			if (sps && (sps->state & AVC_SUBSPS_PARSED)) {
				gf_import_message(import, GF_OK, "SVC (SSPS ID %d) Import results: Slices: %d I %d P %d B", svcc->id - GF_SVC_SSPS_ID_SHIFT, sps->nb_ei, sps->nb_ep, sps->nb_eb);
			}
		}

		if (max_total_delay>1) {
			gf_import_message(import, GF_OK, "Stream uses forward prediction - stream CTS offset: %d frames", max_total_delay);
		}
	}

	if (use_opengop_gdr==2) {
		gf_import_message(import, GF_OK, "OpenGOP detected - adjusting file brand");
		gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_ISO6, 1);
	}

	/*rewrite ESD*/
	if (import->esd) {
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig*) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->predefined = 2;
		import->esd->slConfig->timestampResolution = timescale;
		if (import->esd->decoderConfig) gf_odf_desc_del((GF_Descriptor *)import->esd->decoderConfig);
		import->esd->decoderConfig = gf_isom_get_decoder_config(import->dest, track, 1);
		gf_isom_change_mpeg4_description(import->dest, track, 1, import->esd);
	}

exit:
	if (sample_data) gf_bs_del(sample_data);
	gf_odf_avc_cfg_del(avccfg);
	gf_odf_avc_cfg_del(svccfg);
	gf_free(buffer);
	gf_bs_del(bs);
	gf_fclose(mdia);
	return e;
}

#ifndef GPAC_DISABLE_HEVC
static GF_HEVCParamArray *get_hevc_param_array(GF_HEVCConfig *hevc_cfg, u8 type)
{
	u32 i, count = hevc_cfg->param_array ? gf_list_count(hevc_cfg->param_array) : 0;
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(hevc_cfg->param_array, i);
		if (ar->type==type) return ar;
	}
	return NULL;
}


static void hevc_set_parall_type(GF_HEVCConfig *hevc_cfg)
{
	u32 use_tiles, use_wpp, nb_pps, i, count;
	HEVCState hevc;
	GF_HEVCParamArray *ar = get_hevc_param_array(hevc_cfg, GF_HEVC_NALU_PIC_PARAM);
	if (!ar)
		return;

	count = gf_list_count(ar->nalus);

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;

	use_tiles = 0;
	use_wpp = 0;
	nb_pps = 0;

	for (i=0; i<count; i++) {
		HEVC_PPS *pps;
		GF_AVCConfigSlot *slc = (GF_AVCConfigSlot*)gf_list_get(ar->nalus, i);
		s32 idx = gf_media_hevc_read_pps(slc->data, slc->size, &hevc);

		if (idx>=0) {
			nb_pps++;
			pps = &hevc.pps[idx];
			if (!pps->entropy_coding_sync_enabled_flag && pps->tiles_enabled_flag)
				use_tiles++;
			else if (pps->entropy_coding_sync_enabled_flag && !pps->tiles_enabled_flag)
				use_wpp++;
		}
	}
	if (!use_tiles && !use_wpp) hevc_cfg->parallelismType = 1;
	else if (!use_wpp && (use_tiles==nb_pps) ) hevc_cfg->parallelismType = 2;
	else if (!use_tiles && (use_wpp==nb_pps) ) hevc_cfg->parallelismType = 3;
	else hevc_cfg->parallelismType = 0;
}

#endif

static GF_Err gf_lhevc_set_operating_points_information(GF_ISOFile *file, u32 hevc_track, u32 track, HEVC_VPS *vps, u8 *max_temporal_id)
{
	GF_OperatingPointsInformation *oinf;
	u32 di = 0;
	GF_BitStream *bs;
	char *data;
	u32 data_size;
	u32 i;

	if (!vps->vps_extension_found) return GF_OK;

	oinf = gf_isom_oinf_new_entry();
	if (!oinf) return GF_OUT_OF_MEM;

	oinf->scalability_mask = 0;
	for (i = 0; i < 16; i++) {
		if (vps->scalability_mask[i])
			oinf->scalability_mask |= 1 << i;
	}

	for (i = 0; i < vps->num_profile_tier_level; i++) {
		HEVC_ProfileTierLevel ptl = (i == 0) ? vps->ptl : vps->ext_ptl[i-1];
		LHEVC_ProfileTierLevel *lhevc_ptl;
		GF_SAFEALLOC(lhevc_ptl, LHEVC_ProfileTierLevel);
		lhevc_ptl->general_profile_space = ptl.profile_space;
		lhevc_ptl->general_tier_flag = ptl.tier_flag;
		lhevc_ptl->general_profile_idc = ptl.profile_idc;
		lhevc_ptl->general_profile_compatibility_flags = ptl.profile_compatibility_flag;
		lhevc_ptl->general_constraint_indicator_flags = 0;
		if (ptl.general_progressive_source_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 47;
		if (ptl.general_interlaced_source_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 46;
		if (ptl.general_non_packed_constraint_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 45;
		if (ptl.general_frame_only_constraint_flag)
			lhevc_ptl->general_constraint_indicator_flags |= ((u64)1) << 44;
		lhevc_ptl->general_constraint_indicator_flags |= ptl.general_reserved_44bits;
		lhevc_ptl->general_level_idc = ptl.level_idc;
		gf_list_add(oinf->profile_tier_levels, lhevc_ptl);
	}

	for (i = 0; i < vps->num_output_layer_sets; i++) {
		LHEVC_OperatingPoint *op;
		u32 j;
		u16 minPicWidth, minPicHeight, maxPicWidth, maxPicHeight;
		u8 maxChromaFormat, maxBitDepth;
		u8 maxTemporalId;
		GF_SAFEALLOC(op, LHEVC_OperatingPoint);
		op->output_layer_set_idx = i;
		op->layer_count = vps->num_necessary_layers[i];
		minPicWidth = minPicHeight = maxPicWidth = maxPicHeight = maxTemporalId = 0;
		maxChromaFormat = maxBitDepth = 0;
		for (j = 0; j < op->layer_count; j++) {
			u32 format_idx;
			u32 bitDepth;
			op->layers_info[j].ptl_idx = vps->profile_tier_level_idx[i][j];
			op->layers_info[j].layer_id = j;
			op->layers_info[j].is_outputlayer = vps->output_layer_flag[i][j];
			//FIXME: we consider that this flag is never set
			op->layers_info[j].is_alternate_outputlayer = GF_FALSE;
			if (!maxTemporalId || (maxTemporalId < max_temporal_id[op->layers_info[j].layer_id]))
				maxTemporalId = max_temporal_id[op->layers_info[j].layer_id];
			format_idx = vps->rep_format_idx[op->layers_info[j].layer_id];
			if (!minPicWidth || (minPicWidth > vps->rep_formats[format_idx].pic_width_luma_samples))
				minPicWidth = vps->rep_formats[format_idx].pic_width_luma_samples;
			if (!minPicHeight || (minPicHeight > vps->rep_formats[format_idx].pic_height_luma_samples))
				minPicHeight = vps->rep_formats[format_idx].pic_height_luma_samples;
			if (!maxPicWidth || (maxPicWidth < vps->rep_formats[format_idx].pic_width_luma_samples))
				maxPicWidth = vps->rep_formats[format_idx].pic_width_luma_samples;
			if (!maxPicHeight || (maxPicHeight < vps->rep_formats[format_idx].pic_height_luma_samples))
				maxPicHeight = vps->rep_formats[format_idx].pic_height_luma_samples;
			if (!maxChromaFormat || (maxChromaFormat < vps->rep_formats[format_idx].chroma_format_idc))
				maxChromaFormat = vps->rep_formats[format_idx].chroma_format_idc;
			bitDepth = vps->rep_formats[format_idx].bit_depth_chroma > vps->rep_formats[format_idx].bit_depth_luma ? vps->rep_formats[format_idx].bit_depth_chroma : vps->rep_formats[format_idx].bit_depth_luma;
			if (!maxChromaFormat || (maxChromaFormat < bitDepth))
				maxChromaFormat = bitDepth;
		}
		op->max_temporal_id = maxTemporalId;
		op->minPicWidth = minPicWidth;
		op->minPicHeight = minPicHeight;
		op->maxPicWidth = maxPicWidth;
		op->maxPicHeight = maxPicHeight;
		op->maxChromaFormat = maxChromaFormat;
		op->maxBitDepth = maxBitDepth;
		op->frame_rate_info_flag = GF_FALSE; //FIXME: should fetch this info from VUI
		op->bit_rate_info_flag = GF_FALSE; //we don't use it
		gf_list_add(oinf->operating_points, op);
	}

	for (i = 0; i < vps->max_layers; i++) {
		LHEVC_DependentLayer *dep;
		u32 j, k;
		GF_SAFEALLOC(dep, LHEVC_DependentLayer);
		dep->dependent_layerID = vps->layer_id_in_nuh[i];
		for (j = 0; j < vps->max_layers; j++) {
			if (vps->direct_dependency_flag[dep->dependent_layerID][j]) {
				dep->dependent_on_layerID[dep->num_layers_dependent_on] = j;
				dep->num_layers_dependent_on ++;
			}
		}
		k = 0;
		for (j = 0; j < 16; j++) {
			if (oinf->scalability_mask & (1 << j)) {
				dep->dimension_identifier[j] = vps->dimension_id[i][k];
				k++;
			}
		}
		gf_list_add(oinf->dependency_layers, dep);
	}

	//write Operating Points Information Sample Group
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_oinf_write_entry(oinf, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	gf_isom_oinf_del_entry(oinf);
	gf_isom_add_sample_group_info(file, hevc_track ? hevc_track : track, GF_ISOM_SAMPLE_GROUP_OINF, data, data_size, GF_TRUE, &di);

	if (hevc_track) {
		gf_isom_set_track_reference(file, track, GF_ISOM_REF_OREF, gf_isom_get_track_id(file, hevc_track) );
	}
	gf_free(data);
	return GF_OK;
}


typedef struct
{
	u32 layer_id_plus_one;
	u32 min_temporal_id, max_temporal_id;
} LHVCLayerInfo;

static void gf_lhevc_set_layer_information(GF_ISOFile *file, u32 track, LHVCLayerInfo *linf)
{
	u32 i, nb_layers=0, di=0;
	char *data;
	u32 data_size;

	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	for (i=0; i<64; i++) {
		if (linf[i].layer_id_plus_one) nb_layers++;
	}
	gf_bs_write_int(bs, 0, 2);
	gf_bs_write_int(bs, nb_layers, 6);
	for (i=0; i<nb_layers; i++) {
		if (! linf[i].layer_id_plus_one) continue;
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, linf[i].layer_id_plus_one - 1, 6);
		gf_bs_write_int(bs, linf[i].min_temporal_id, 3);
		gf_bs_write_int(bs, linf[i].max_temporal_id, 3);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 0xFF, 7);

	}
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	gf_isom_add_sample_group_info(file, track, GF_ISOM_SAMPLE_GROUP_LINF, data, data_size, GF_TRUE, &di);
	gf_free(data);
}

static GF_Err gf_import_hevc(GF_MediaImporter *import)
{
#ifdef GPAC_DISABLE_HEVC
	return GF_NOT_SUPPORTED;
#else
	Bool detect_fps;
	u64 nal_start, nal_end, total_size;
	u32 i, nal_size, track, trackID, di, cur_samp, nb_i, nb_idr, nb_p, nb_b, nb_sp, nb_si, nb_sei, max_w, max_h, max_w_b, max_h_b, max_total_delay, nb_nalus, hevc_base_track;
	s32 idx, sei_recovery_frame_count;
	u64 duration;
	GF_Err e;
	FILE *mdia;
	HEVCState hevc;
	GF_AVCConfigSlot *slc;
	GF_HEVCConfig *hevc_cfg, *lhvc_cfg, *dst_cfg;
	GF_HEVCParamArray *spss, *ppss, *vpss;
	GF_BitStream *bs;
	GF_BitStream *sample_data;
	Bool flush_sample, flush_next_sample, is_empty_sample, sample_has_islice, sample_has_vps, sample_has_sps, is_islice, first_nal, slice_is_ref, has_cts_offset, is_paff, set_subsamples, slice_force_ref;
	u32 ref_frame, timescale, copy_size, size_length, dts_inc;
	s32 last_poc, max_last_poc, max_last_b_poc, poc_diff, prev_last_poc, min_poc, poc_shift;
	Bool first_hevc, /*has_hevc, */ has_lhvc;
	u32 use_opengop_gdr = 0;
	u8 layer_ids[64];
	SAPType sample_rap_type;
	s32 cur_vps_id = -1;
	u8 max_temporal_id[64];
	u32 min_layer_id = (u32) -1;
	LHVCLayerInfo linf[64];
	Bool sample_is_ref;


	Double FPS;
	char *buffer;
	u32 max_size = 4096;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_OVERRIDE_FPS | GF_IMPORT_FORCE_PACKED;
		return GF_OK;
	}

	memset(linf, 0, sizeof(linf));

	set_subsamples = (import->flags & GF_IMPORT_SET_SUBSAMPLES) ? GF_TRUE : GF_FALSE;

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Cannot find file %s", import->in_name);

	detect_fps = GF_TRUE;
	FPS = (Double) import->video_fps;
	if (!FPS) {
		FPS = GF_IMPORT_DEFAULT_FPS;
	} else {
		if (import->video_fps == GF_IMPORT_AUTO_FPS) {
			import->video_fps = GF_IMPORT_DEFAULT_FPS;	/*fps=auto is handled as auto-detection in h264*/
		} else {
			/*fps is forced by the caller*/
			detect_fps = GF_FALSE;
		}
	}
	get_video_timing(FPS, &timescale, &dts_inc);

	poc_diff = 0;

restart_import:

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.sps_active_idx = -1;
	dst_cfg = hevc_cfg = gf_odf_hevc_cfg_new();
	lhvc_cfg = gf_odf_hevc_cfg_new();
	lhvc_cfg->complete_representation = GF_TRUE;
	lhvc_cfg->is_lhvc = GF_TRUE;
	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	sample_data = NULL;
	first_hevc = GF_TRUE;
	sei_recovery_frame_count = -1;
	spss = ppss = vpss = NULL;
	nb_nalus = 0;
	hevc_base_track = 0;
	/*has_hevc = */has_lhvc = GF_FALSE;
	sample_is_ref = 0;

	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);
	if (!gf_media_nalu_is_start_code(bs)) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Cannot find HEVC start code");
		goto exit;
	}

	/*NALU size packing disabled*/
	if (!(import->flags & GF_IMPORT_FORCE_PACKED)) size_length = 32;
	/*if import in edit mode, use smallest NAL size and adjust on the fly*/
	else if (gf_isom_get_mode(import->dest)!=GF_ISOM_OPEN_WRITE) size_length = 8;
	else size_length = 32;

	trackID = 0;

	if (import->esd) trackID = import->esd->ESID;

	track = gf_isom_new_track(import->dest, trackID, GF_ISOM_MEDIA_VISUAL, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);
	if (import->esd && import->esd->dependsOnESID) {
		gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_DECODE, import->esd->dependsOnESID);
	}

	e = gf_isom_hevc_config_new(import->dest, track, hevc_cfg, NULL, NULL, &di);
	if (e) goto exit;

	gf_isom_set_nalu_extract_mode(import->dest, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	memset(layer_ids, 0, sizeof(u8)*64);

	sample_data = NULL;
	sample_rap_type = RAP_NO;
	sample_has_islice = GF_FALSE;
	sample_has_sps = GF_FALSE;
	sample_has_vps = GF_FALSE;
	cur_samp = 0;
	is_paff = GF_FALSE;
	total_size = gf_bs_get_size(bs);
	nal_start = gf_bs_get_position(bs);
	duration = (u64) ( ((Double)import->duration) * timescale / 1000.0);

	nb_i = nb_idr = nb_p = nb_b = nb_sp = nb_si = nb_sei = 0;
	max_w = max_h = max_w_b = max_h_b = 0;
	first_nal = GF_TRUE;
	ref_frame = 0;
	last_poc = max_last_poc = max_last_b_poc = prev_last_poc = 0;
	max_total_delay = 0;

	gf_isom_set_cts_packing(import->dest, track, GF_TRUE);
	has_cts_offset = GF_FALSE;
	min_poc = 0;
	poc_shift = 0;
	flush_next_sample = GF_FALSE;
	is_empty_sample = GF_TRUE;
	memset(max_temporal_id, 0, 64*sizeof(u8));

	while (gf_bs_available(bs)) {
		s32 res;
		GF_HEVCConfig *prev_cfg;
		u8 nal_unit_type, temporal_id, layer_id;
		Bool skip_nal, add_sps, is_slice, has_vcl_nal;
		u32 nal_and_trailing_size;

		has_vcl_nal = GF_FALSE;
		nal_and_trailing_size = nal_size = gf_media_nalu_next_start_code_bs(bs);
		if (!(import->flags & GF_IMPORT_KEEP_TRAILING)) {
			nal_size = gf_media_nalu_payload_end_bs(bs);
		}


		if (nal_size>max_size) {
			buffer = (char*)gf_realloc(buffer, sizeof(char)*nal_size);
			max_size = nal_size;
		}

		/*read the file, and work on a memory buffer*/
		gf_bs_read_data(bs, buffer, nal_size);

//		gf_bs_seek(bs, nal_start);

		res = gf_media_hevc_parse_nalu(buffer, nal_size, &hevc, &nal_unit_type, &temporal_id, &layer_id);

		if (max_temporal_id[layer_id] < temporal_id)
			max_temporal_id[layer_id] = temporal_id;

		if (layer_id && (import->flags & GF_IMPORT_SVC_NONE)) {
			goto next_nal;
		}

		nb_nalus++;

		is_islice = GF_FALSE;

		prev_cfg = dst_cfg;

		if (layer_id) {
			dst_cfg = lhvc_cfg;
			has_lhvc = GF_TRUE;
		} else {
			dst_cfg = hevc_cfg;
			//has_hevc = GF_TRUE;
		}

		if (prev_cfg != dst_cfg) {
			vpss = get_hevc_param_array(dst_cfg, GF_HEVC_NALU_VID_PARAM);
			spss = get_hevc_param_array(dst_cfg, GF_HEVC_NALU_SEQ_PARAM);
			ppss = get_hevc_param_array(dst_cfg, GF_HEVC_NALU_PIC_PARAM);
		}

		skip_nal = GF_FALSE;
		copy_size = flush_sample = GF_FALSE;
		is_slice = GF_FALSE;

		switch (res) {
		case 1:
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				if (!is_empty_sample)
					flush_sample = GF_TRUE;
			} else {
				flush_sample = GF_TRUE;
			}
			break;
		case -1:
			gf_import_message(import, GF_OK, "Warning: Error parsing NAL unit");
			skip_nal = GF_TRUE;
			break;
		case -2:
			skip_nal = GF_TRUE;
			break;
		default:
			break;
		}

		if ( (layer_id == min_layer_id) && flush_next_sample && (nal_unit_type!=GF_HEVC_NALU_SEI_SUFFIX)) {
			flush_next_sample = GF_FALSE;
			flush_sample = GF_TRUE;
		}

		switch (nal_unit_type) {
		case GF_HEVC_NALU_VID_PARAM:
			if (import->flags & GF_IMPORT_NO_VPS_EXTENSIONS) {
				//this may modify nal_size, but we don't use it for bitstream reading
				idx = gf_media_hevc_read_vps_ex(buffer, &nal_size, &hevc, GF_TRUE);
			} else {
				idx = hevc.last_parsed_vps_id;
			}
			if (idx<0) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing Video Param");
				goto exit;
			}
			/*if we get twice the same VPS put in the the bitstream and set array_completeness to 0 ...*/
			if (hevc.vps[idx].state == 2) {
				if (hevc.vps[idx].crc != gf_crc_32(buffer, nal_size)) {
					copy_size = nal_size;
					assert(vpss);
					vpss->array_completeness = 0;
				}
			}

			if (hevc.vps[idx].state==1) {
				hevc.vps[idx].state = 2;
				hevc.vps[idx].crc = gf_crc_32(buffer, nal_size);

				dst_cfg->avgFrameRate = hevc.vps[idx].rates[0].avg_pic_rate;
				dst_cfg->constantFrameRate = hevc.vps[idx].rates[0].constand_pic_rate_idc;
				dst_cfg->numTemporalLayers = hevc.vps[idx].max_sub_layers;
				dst_cfg->temporalIdNested = hevc.vps[idx].temporal_id_nesting;
				//TODO set scalability mask

				if (!vpss) {
					GF_SAFEALLOC(vpss, GF_HEVCParamArray);
					vpss->nalus = gf_list_new();
					gf_list_add(dst_cfg->param_array, vpss);
					vpss->array_completeness = 1;
					vpss->type = GF_HEVC_NALU_VID_PARAM;
				}

				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					vpss->array_completeness = 0;
					copy_size = nal_size;
					sample_has_vps = GF_TRUE;
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				slc->size = nal_size;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				memcpy(slc->data, buffer, sizeof(char)*slc->size);

				gf_list_add(vpss->nalus, slc);

			}

			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				copy_size = nal_size;
				sample_has_vps = GF_TRUE;
				if (!is_empty_sample)
					flush_sample = GF_TRUE;
			}

			cur_vps_id = idx;
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			idx = hevc.last_parsed_sps_id;
			if (idx<0) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing SeqInfo");
				break;
			}
			add_sps = GF_FALSE;
			if ((hevc.sps[idx].state & AVC_SPS_PARSED) && !(hevc.sps[idx].state & AVC_SPS_DECLARED)) {
				hevc.sps[idx].state |= AVC_SPS_DECLARED;
				add_sps = GF_TRUE;
				hevc.sps[idx].crc = gf_crc_32(buffer, nal_size);
			}

			/*if we get twice the same SPS put it in the bitstream and set array_completeness to 0 ...*/
			else if (hevc.sps[idx].state & AVC_SPS_DECLARED) {
				if (hevc.sps[idx].crc != gf_crc_32(buffer, nal_size)) {
					copy_size = nal_size;
					assert(spss);
					spss->array_completeness = 0;
				}
			}

			if (add_sps) {
				dst_cfg->configurationVersion = 1;
				dst_cfg->profile_space = hevc.sps[idx].ptl.profile_space;
				dst_cfg->tier_flag = hevc.sps[idx].ptl.tier_flag;
				dst_cfg->profile_idc = hevc.sps[idx].ptl.profile_idc;
				dst_cfg->general_profile_compatibility_flags = hevc.sps[idx].ptl.profile_compatibility_flag;
				dst_cfg->progressive_source_flag = hevc.sps[idx].ptl.general_progressive_source_flag;
				dst_cfg->interlaced_source_flag = hevc.sps[idx].ptl.general_interlaced_source_flag;
				dst_cfg->non_packed_constraint_flag = hevc.sps[idx].ptl.general_non_packed_constraint_flag;
				dst_cfg->frame_only_constraint_flag = hevc.sps[idx].ptl.general_frame_only_constraint_flag;

				dst_cfg->constraint_indicator_flags = hevc.sps[idx].ptl.general_reserved_44bits;
				dst_cfg->level_idc = hevc.sps[idx].ptl.level_idc;

				dst_cfg->chromaFormat = hevc.sps[idx].chroma_format_idc;
				dst_cfg->luma_bit_depth = hevc.sps[idx].bit_depth_luma;
				dst_cfg->chroma_bit_depth = hevc.sps[idx].bit_depth_chroma;

				if (!spss) {
					GF_SAFEALLOC(spss, GF_HEVCParamArray);
					spss->nalus = gf_list_new();
					gf_list_add(dst_cfg->param_array, spss);
					spss->array_completeness = 1;
					spss->type = GF_HEVC_NALU_SEQ_PARAM;
				}

				/*disable frame rate scan, most bitstreams have wrong values there*/
				if (detect_fps && hevc.sps[idx].has_timing_info
				        /*if detected FPS is greater than 1000, assume wrong timing info*/
				        && (hevc.sps[idx].time_scale <= 1000*hevc.sps[idx].num_units_in_tick)
				   ) {
					timescale = hevc.sps[idx].time_scale;
					dts_inc =   hevc.sps[idx].num_units_in_tick;
					FPS = (Double)timescale / dts_inc;
					detect_fps = GF_FALSE;
					gf_isom_remove_track(import->dest, track);
					if (sample_data) gf_bs_del(sample_data);
					gf_odf_hevc_cfg_del(hevc_cfg);
					hevc_cfg = NULL;
					gf_odf_hevc_cfg_del(lhvc_cfg);
					lhvc_cfg = NULL;
					gf_free(buffer);
					buffer = NULL;
					gf_bs_del(bs);
					bs = NULL;
					gf_fseek(mdia, 0, SEEK_SET);
					goto restart_import;
				}

				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					spss->array_completeness = 0;
					copy_size = nal_size;
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				slc->size = nal_size;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				memcpy(slc->data, buffer, sizeof(char)*slc->size);
				gf_list_add(spss->nalus, slc);

				if (first_hevc) {
					first_hevc = GF_FALSE;
					gf_import_message(import, GF_OK, "HEVC import - frame size %d x %d at %02.3f FPS", hevc.sps[idx].width, hevc.sps[idx].height, FPS);
				} else {
					gf_import_message(import, GF_OK, "LHVC detected - %d x %d at %02.3f FPS", hevc.sps[idx].width, hevc.sps[idx].height, FPS);
				}

				if ((max_w <= hevc.sps[idx].width) && (max_h <= hevc.sps[idx].height)) {
					max_w = hevc.sps[idx].width;
					max_h = hevc.sps[idx].height;
				}
				if (!layer_id && (max_w_b <= hevc.sps[idx].width) && (max_h_b <= hevc.sps[idx].height)) {
					max_w_b = hevc.sps[idx].width;
					max_h_b = hevc.sps[idx].height;
				}
			}
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				copy_size = nal_size;
				sample_has_sps = GF_TRUE;
				if (!is_empty_sample)
					flush_sample = GF_TRUE;
			}
			break;

		case GF_HEVC_NALU_PIC_PARAM:
			idx = hevc.last_parsed_pps_id;
			if (idx<0) {
				e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing Picture Param");
				goto exit;
			}
			/*if we get twice the same PPS put it in the bitstream and set array_completeness to 0 ...*/
			if (hevc.pps[idx].state == 2) {
				if (hevc.pps[idx].crc != gf_crc_32(buffer, nal_size)) {
					copy_size = nal_size;
					if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					} else if (!ppss) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[HEVC Import] Redefinition of PPS with id %d on a different layer but using out-of-band parameter set storage, resulting file might be broken\n", idx));
					} else {
						ppss->array_completeness = 0;
					}
				}
			}

			if (hevc.pps[idx].state==1) {
				hevc.pps[idx].state = 2;
				hevc.pps[idx].crc = gf_crc_32(buffer, nal_size);

				if (!ppss) {
					GF_SAFEALLOC(ppss, GF_HEVCParamArray);
					ppss->nalus = gf_list_new();
					gf_list_add(dst_cfg->param_array, ppss);
					ppss->array_completeness = 1;
					ppss->type = GF_HEVC_NALU_PIC_PARAM;
				}

				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					ppss->array_completeness = 0;
					copy_size = nal_size;
				}

				slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				slc->size = nal_size;
				slc->id = idx;
				slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
				memcpy(slc->data, buffer, sizeof(char)*slc->size);

				gf_list_add(ppss->nalus, slc);
			}
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				copy_size = nal_size;
				if (!is_empty_sample && !layer_id)
					flush_sample = GF_TRUE;
			}

			break;
		case GF_HEVC_NALU_SEI_SUFFIX:
			if (import->flags & GF_IMPORT_NO_SEI) {
					copy_size = 0;
			} else {
				if (hevc.sps_active_idx != -1) {
					copy_size = nal_size;
					if (!layer_id) {
						if (!is_empty_sample) flush_next_sample = GF_TRUE;
						else copy_size = 0;
					}
					if (copy_size)
						nb_sei++;
				}
			}
			break;
		case GF_HEVC_NALU_SEI_PREFIX:
			if (import->flags & GF_IMPORT_NO_SEI) {
				copy_size = 0;
			} else {
				if (hevc.sps_active_idx != -1) {
					//todo: inspect SEI and decide what we import
					copy_size = nal_size;
				} else {
					//if no state yet, import SEI
					copy_size = nal_size;
				}
			}
			if (copy_size) {
				nb_sei++;
			}
			if (nal_size) {
				//FIXME should not be minus 1 in layer_ids[layer_id - 1] but the previous layer in the tree
				if (!layer_id || !layer_ids[layer_id - 1])
					flush_sample = GF_TRUE;
			}
			break;

		/*slice_segment_layer_rbsp*/
		case GF_HEVC_NALU_SLICE_STSA_N:
		case GF_HEVC_NALU_SLICE_STSA_R:
		case GF_HEVC_NALU_SLICE_RADL_R:
		case GF_HEVC_NALU_SLICE_RASL_R:
		case GF_HEVC_NALU_SLICE_RADL_N:
		case GF_HEVC_NALU_SLICE_RASL_N:
		case GF_HEVC_NALU_SLICE_TRAIL_N:
		case GF_HEVC_NALU_SLICE_TRAIL_R:
		case GF_HEVC_NALU_SLICE_TSA_N:
		case GF_HEVC_NALU_SLICE_TSA_R:
		case GF_HEVC_NALU_SLICE_BLA_W_LP:
		case GF_HEVC_NALU_SLICE_BLA_W_DLP:
		case GF_HEVC_NALU_SLICE_BLA_N_LP:
		case GF_HEVC_NALU_SLICE_IDR_W_DLP:
		case GF_HEVC_NALU_SLICE_IDR_N_LP:
		case GF_HEVC_NALU_SLICE_CRA:
			is_slice = GF_TRUE;
			if (min_layer_id > layer_id)
				min_layer_id = layer_id;
			/*			if ((hevc.s_info.slice_segment_address<=100) || (hevc.s_info.slice_segment_address>=200))
							skip_nal = 1;
						if (!hevc.s_info.slice_segment_address)
							skip_nal = 0;
			*/
			if (! skip_nal) {
				copy_size = nal_size;
				has_vcl_nal = GF_TRUE;
				switch (hevc.s_info.slice_type) {
				case GF_HEVC_SLICE_TYPE_P:
					nb_p++;
					break;
				case GF_HEVC_SLICE_TYPE_I:
					nb_i++;
					is_islice = GF_TRUE;
					break;
				case GF_HEVC_SLICE_TYPE_B:
					nb_b++;
					break;
				}
			}
			break;

		case GF_HEVC_NALU_ACCESS_UNIT:
			if (import->keep_audelim) {
				copy_size = nal_size;
			} else {
				copy_size = 0;
			}
			break;
		/*remove*/
		case GF_HEVC_NALU_FILLER_DATA:
		case GF_HEVC_NALU_END_OF_SEQ:
		case GF_HEVC_NALU_END_OF_STREAM:
			break;

		default:
			gf_import_message(import, GF_OK, "WARNING: NAL Unit type %d not handled - adding", nal_unit_type);
			copy_size = nal_size;
			break;
		}

		if (!nal_size) break;
		if (copy_size) {
			linf[layer_id].layer_id_plus_one = layer_id + 1;
			if (! linf[layer_id].max_temporal_id ) linf[layer_id].max_temporal_id = temporal_id;
			else if (linf[layer_id].max_temporal_id < temporal_id) linf[layer_id].max_temporal_id = temporal_id;

			if (! linf[layer_id].min_temporal_id ) linf[layer_id].min_temporal_id = temporal_id;
			else if (linf[layer_id].min_temporal_id > temporal_id) linf[layer_id].min_temporal_id = temporal_id;
		}

		if (flush_sample && is_empty_sample)
			flush_sample = GF_FALSE;

		if (flush_sample && sample_data) {
			Bool is_rap=GF_FALSE;
			GF_ISOSample *samp = gf_isom_sample_new();
			samp->DTS = (u64)dts_inc*cur_samp;
			samp->IsRAP = ((sample_rap_type==SAP_TYPE_1) || (sample_rap_type==SAP_TYPE_2)) ? RAP : RAP_NO;
			if (! samp->IsRAP) {
				if (sample_has_islice && (import->flags & GF_IMPORT_FORCE_SYNC) && (sei_recovery_frame_count==0)) {
					samp->IsRAP = RAP;
					if (!use_opengop_gdr) {
						use_opengop_gdr = 1;
						GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[HEVC Import] Forcing non-IDR samples with I slices to be marked as sync points - resulting file will not be ISO conformant\n"));
					}
				}
			}
			gf_bs_get_content(sample_data, &samp->data, &samp->dataLength);
			gf_bs_del(sample_data);
			sample_data = NULL;

			if (samp->IsRAP) is_rap = GF_TRUE;

			//fixme, we should check sps and vps IDs when missing
			if ((import->flags & GF_IMPORT_FORCE_XPS_INBAND) && sample_rap_type && (!sample_has_vps || !sample_has_sps) ) {
				u32 k;
				GF_BitStream *fbs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				if (!sample_has_vps) {
					if (!vpss)
						vpss = get_hevc_param_array(hevc_cfg, GF_HEVC_NALU_VID_PARAM);
					assert(vpss);
					for (k=0;k<gf_list_count(vpss->nalus); k++) {
						GF_AVCConfigSlot *slc = gf_list_get(vpss->nalus, k);
						gf_bs_write_int(fbs, slc->size, size_length);
						gf_bs_write_data(fbs, slc->data, slc->size);
					}
				}
				if (!sample_has_sps) {
					if (!spss)
						spss = get_hevc_param_array(hevc_cfg, GF_HEVC_NALU_SEQ_PARAM);
					assert(spss);
					for (k=0;k<gf_list_count(spss->nalus); k++) {
						GF_AVCConfigSlot *slc = gf_list_get(spss->nalus, k);
						gf_bs_write_int(fbs, slc->size, size_length);
						gf_bs_write_data(fbs, slc->data, slc->size);
					}
				}
				gf_bs_write_data(fbs, samp->data, samp->dataLength);
				gf_free(samp->data);
				gf_bs_get_content(fbs, &samp->data, &samp->dataLength);
				gf_bs_del(fbs);
			}

			/*CTS recomuting is much trickier than with MPEG-4 ASP due to b-slice used as references - we therefore
			store the POC as the CTS offset and update the whole table at the end*/
			samp->CTS_Offset = last_poc - poc_shift;
			assert(last_poc >= poc_shift);
			e = gf_isom_add_sample(import->dest, track, di, samp);
			if (e) goto exit;

			cur_samp++;

			/*write sampleGroups info*/
			if (!samp->IsRAP && ((sei_recovery_frame_count>=0) || sample_has_islice || (sample_rap_type && (sample_rap_type<=SAP_TYPE_3)) ) ) {
				/*generic GDR*/
				if (sei_recovery_frame_count > 0) {
					if (!use_opengop_gdr) use_opengop_gdr = 1;
					e = gf_isom_set_sample_roll_group(import->dest, track, cur_samp, (s16) sei_recovery_frame_count);
				}
				/*open-GOP*/
				else if (sample_rap_type==SAP_TYPE_3) {
					if (!min_layer_id && !use_opengop_gdr) use_opengop_gdr = 2;
					e = gf_isom_set_sample_rap_group(import->dest, track, cur_samp, 0);

					is_rap = GF_TRUE;
				}
				if (e) goto exit;
			}

			//write sample deps
			if (import->flags & GF_IMPORT_SAMPLE_DEPS) {
				u32 isLeading, dependsOn, dependedOn;
				isLeading = 0;
				dependsOn = is_rap ? 2 : 1;
				dependedOn = sample_is_ref ? 1 : 2;

				e = gf_isom_sample_set_dep_info(import->dest, track, cur_samp, isLeading, dependsOn, dependedOn, 2);
				if (e) goto exit;
			}
			sample_is_ref = GF_FALSE;


			gf_isom_sample_del(&samp);
			gf_set_progress("Importing HEVC", (u32) (nal_start/1024), (u32) (total_size/1024) );
			first_nal = GF_TRUE;

			if (min_poc > last_poc)
				min_poc = last_poc;

			sample_has_islice = GF_FALSE;
			sample_has_vps = GF_FALSE;
			sample_has_sps = GF_FALSE;
			sei_recovery_frame_count = -1;
			is_empty_sample = GF_TRUE;
		}

		if (copy_size) {
			if (!sample_is_ref) {
				HEVC_VPS *vps;
				switch (nal_unit_type) {
				case GF_HEVC_NALU_SLICE_TRAIL_N:
				case GF_HEVC_NALU_SLICE_TSA_N:
				case GF_HEVC_NALU_SLICE_STSA_N:
				case GF_HEVC_NALU_SLICE_RADL_N:
				case GF_HEVC_NALU_SLICE_RASL_N:
				case GF_HEVC_NALU_SLICE_RSV_VCL_N10:
				case GF_HEVC_NALU_SLICE_RSV_VCL_N12:
				case GF_HEVC_NALU_SLICE_RSV_VCL_N14:
					vps = &hevc.vps[hevc.s_info.sps->vps_id];
					if ((u32) temporal_id + 1 < vps->max_sub_layers) {
						sample_is_ref = GF_TRUE;
					}
					break;
				default:
					if (nal_unit_type<GF_HEVC_NALU_VID_PARAM)
						sample_is_ref = GF_TRUE;
				}
			}

			if (is_islice)
				sample_has_islice = GF_TRUE;

			if ((size_length<32) && ( (u32) (1<<size_length)-1 < copy_size)) {
				u32 diff_size = 8;
				while ((size_length<32) && ( (u32) (1<<(size_length+diff_size))-1 < copy_size)) diff_size+=8;
				/*only 8bits, 16bits and 32 bits*/
				if (size_length+diff_size == 24) diff_size+=8;

				gf_import_message(import, GF_OK, "Adjusting HEVC SizeLength to %d bits", size_length+diff_size);
				gf_media_avc_rewrite_samples(import->dest, track, size_length, size_length+diff_size);

				/*rewrite current sample*/
				if (sample_data) {
					char *sd;
					u32 sd_l;
					GF_BitStream *prev_sd;
					gf_bs_get_content(sample_data, &sd, &sd_l);
					gf_bs_del(sample_data);
					sample_data = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
					prev_sd = gf_bs_new(sd, sd_l, GF_BITSTREAM_READ);
					while (gf_bs_available(prev_sd)) {
						char *buf;
						u32 s = gf_bs_read_int(prev_sd, size_length);
						gf_bs_write_int(sample_data, s, size_length+diff_size);
						buf = (char*)gf_malloc(sizeof(char)*s);
						gf_bs_read_data(prev_sd, buf, s);
						gf_bs_write_data(sample_data, buf, s);
						gf_free(buf);
					}
					gf_bs_del(prev_sd);
					gf_free(sd);
				}
				size_length+=diff_size;

			}
			if (!sample_data) sample_data = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_int(sample_data, copy_size, size_length);
			gf_bs_write_data(sample_data, buffer, copy_size);

			if (set_subsamples) {
				/* use the res and priority value of last prefix NALU */
				gf_isom_add_subsample(import->dest, track, cur_samp+1, 0, copy_size+size_length/8, 0, 0, GF_FALSE);
			}

			if (has_vcl_nal) {
				is_empty_sample = GF_FALSE;
			}
			layer_ids[layer_id] = 1;

			if ((layer_id == min_layer_id) && is_slice) {
				slice_is_ref = gf_media_hevc_slice_is_IDR(&hevc);
				if (slice_is_ref)
					nb_idr++;
				slice_force_ref = GF_FALSE;

				/*we only indicate TRUE IDRs for sync samples (cf AVC file format spec).
				SEI recovery should be used to build sampleToGroup & RollRecovery tables*/
				if (first_nal) {
					first_nal = GF_FALSE;
					if (hevc.sei.recovery_point.valid || (import->flags & GF_IMPORT_FORCE_SYNC)) {
						Bool bIntraSlice = gf_media_hevc_slice_is_intra(&hevc);
						sei_recovery_frame_count = hevc.sei.recovery_point.frame_cnt;

						/*we allow to mark I-frames as sync on open-GOPs (with sei_recovery_frame_count=0) when forcing sync even when the SEI RP is not available*/
						if (!hevc.sei.recovery_point.valid && bIntraSlice) {
							sei_recovery_frame_count = 0;
							if (use_opengop_gdr == 1) {
								use_opengop_gdr = 2; /*avoid message flooding*/
								GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[HEVC Import] No valid SEI Recovery Point found although needed - forcing\n"));
							}
						}
						hevc.sei.recovery_point.valid = 0;
						if (bIntraSlice && (import->flags & GF_IMPORT_FORCE_SYNC) && (sei_recovery_frame_count==0))
							slice_force_ref = GF_TRUE;
					}
					sample_rap_type = RAP_NO;
					if (gf_media_hevc_slice_is_IDR(&hevc)) {
						sample_rap_type = SAP_TYPE_1;
					}
					else {
						switch (hevc.s_info.nal_unit_type) {
						case GF_HEVC_NALU_SLICE_BLA_W_LP:
						case GF_HEVC_NALU_SLICE_BLA_W_DLP:
							sample_rap_type = SAP_TYPE_3;
							break;
						case GF_HEVC_NALU_SLICE_BLA_N_LP:
							sample_rap_type = SAP_TYPE_1;
							break;
						case GF_HEVC_NALU_SLICE_CRA:
							sample_rap_type = SAP_TYPE_3;
							break;
						}
					}
				}

				if (hevc.s_info.poc<poc_shift) {
					u32 j;
					if (ref_frame) {
						for (j=ref_frame; j<=cur_samp; j++) {
							GF_ISOSample *samp = gf_isom_get_sample_info(import->dest, track, j, NULL, NULL);
							if (!samp) break;
							samp->CTS_Offset += poc_shift;
							samp->CTS_Offset -= hevc.s_info.poc;
							gf_isom_modify_cts_offset(import->dest, track, j, samp->CTS_Offset);
							gf_isom_sample_del(&samp);
						}
					}
					poc_shift = hevc.s_info.poc;
				}

				/*if #pics, compute smallest POC increase*/
				if (hevc.s_info.poc != last_poc) {
					if (!poc_diff || (poc_diff > abs(hevc.s_info.poc-last_poc))) {
						poc_diff = abs(hevc.s_info.poc - last_poc);/*ideally we would need to start the parsing again as poc_diff helps computing max_total_delay*/
					}
					last_poc = hevc.s_info.poc;
					assert(is_slice);
				}

				/*ref slice, reset poc*/
				if (slice_is_ref) {
					ref_frame = cur_samp+1;
					max_last_poc = last_poc = max_last_b_poc = 0;
					poc_shift = 0;
				}
				/*forced ref slice*/
				else if (slice_force_ref) {
					ref_frame = cur_samp+1;
					/*adjust POC shift as sample will now be marked as sync, so wo must store poc as if IDR (eg POC=0) for our CTS offset computing to be correct*/
					poc_shift = hevc.s_info.poc;
				}
				/*strictly less - this is a new P slice*/
				else if (max_last_poc<last_poc) {
					max_last_b_poc = 0;
					//prev_last_poc = max_last_poc;
					max_last_poc = last_poc;
				}
				/*stricly greater*/
				else if (max_last_poc>last_poc) {
					/*need to store TS offsets*/
					has_cts_offset = GF_TRUE;
					switch (hevc.s_info.slice_type) {
					case GF_AVC_TYPE_B:
					case GF_AVC_TYPE2_B:
						if (!max_last_b_poc) {
							max_last_b_poc = last_poc;
						}
						/*if same poc than last max, this is a B-slice*/
						else if (last_poc>max_last_b_poc) {
							max_last_b_poc = last_poc;
						}
						/*otherwise we had a B-slice reference: do nothing*/

						break;
					}
				}

				/*compute max delay (applicable when B slice are present)*/
				if (ref_frame && poc_diff && (s32)(cur_samp-(ref_frame-1)-last_poc/poc_diff)>(s32)max_total_delay) {
					max_total_delay = cur_samp - (ref_frame-1) - last_poc/poc_diff;
				}
			}
		}

next_nal:
		gf_bs_align(bs);
		nal_end = gf_bs_get_position(bs);
		assert(nal_start <= nal_end);
		assert(nal_end <= nal_start + nal_and_trailing_size);
		if (nal_end != nal_start + nal_and_trailing_size)
			gf_bs_seek(bs, nal_start + nal_and_trailing_size);

		if (!gf_bs_available(bs)) break;
		if (duration && (dts_inc*cur_samp > duration)) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;

		/*consume next start code*/
		nal_start = gf_media_nalu_next_start_code_bs(bs);
		if (nal_start) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[hevc] invalid nal_size (%u)? Skipping "LLU" bytes to reach next start code\n", nal_size, nal_start));
			gf_bs_skip_bytes(bs, nal_start);
		}
		nal_start = gf_media_nalu_is_start_code(bs);
		if (!nal_start) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[hevc] error: no start code found ("LLU" bytes read out of "LLU") - leaving\n", gf_bs_get_position(bs), gf_bs_get_size(bs)));
			break;
		}
		nal_start = gf_bs_get_position(bs);
	}

	/*final flush*/
	if (sample_data) {
		GF_ISOSample *samp = gf_isom_sample_new();
		samp->DTS = (u64)dts_inc*cur_samp;
		samp->IsRAP = (sample_rap_type == SAP_TYPE_1) ? RAP : RAP_NO;
		if (!sample_rap_type && sample_has_islice && (import->flags & GF_IMPORT_FORCE_SYNC)) {
			samp->IsRAP = RAP;
		}
		/*we store the frame order (based on the POC) as the CTS offset and update the whole table at the end*/
		samp->CTS_Offset = last_poc - poc_shift;
		gf_bs_get_content(sample_data, &samp->data, &samp->dataLength);
		gf_bs_del(sample_data);
		sample_data = NULL;
		e = gf_isom_add_sample(import->dest, track, di, samp);
		if (e) goto exit;

		gf_isom_sample_del(&samp);
		gf_set_progress("Importing HEVC", (u32) cur_samp, cur_samp+1);
		cur_samp++;

		//write sample deps
		if (import->flags & GF_IMPORT_SAMPLE_DEPS) {
			u32 isLeading, dependsOn, dependedOn;
			isLeading = 0;
			dependsOn = (sample_rap_type && (sample_rap_type <= SAP_TYPE_3)) ? 2 : 1;
			dependedOn = sample_is_ref ? 1 : 2;

			e = gf_isom_sample_set_dep_info(import->dest, track, cur_samp, isLeading, dependsOn, dependedOn, 2);
			if (e) goto exit;
		}
		sample_is_ref = GF_FALSE;
	}


	/*recompute all CTS offsets*/
	if (has_cts_offset) {
		u32 last_cts_samp;
		u64 last_dts, max_cts, min_cts, min_cts_offset;
		if (!poc_diff) poc_diff = 1;
		/*no b-frame references, no need to cope with negative poc*/
		if (!max_total_delay) {
			min_poc=0;
			max_total_delay = 1;
		}
		cur_samp = gf_isom_get_sample_count(import->dest, track);
		min_poc *= -1;
		last_dts = 0;
		max_cts = 0;
		min_cts = (u64) -1;
		min_cts_offset = (u64) -1;
		last_cts_samp = 0;

		for (i=0; i<cur_samp; i++) {
			u64 cts;
			/*not using descIdx and data_offset will only fetch DTS, CTS and RAP which is all we need*/
			GF_ISOSample *samp = gf_isom_get_sample_info(import->dest, track, i+1, NULL, NULL);
			/*poc re-init (RAP and POC to 0, otherwise that's SEI recovery), update base DTS*/
			if (samp->IsRAP /*&& !samp->CTS_Offset*/)
				last_dts = samp->DTS * (1+is_paff);

			/*CTS offset is frame POC (refers to last IDR)*/
			cts = (min_poc + (s32) samp->CTS_Offset) * dts_inc/poc_diff + (u32) last_dts;

			/*if PAFF, 2 pictures (eg poc) <=> 1 aggregated frame (eg sample), divide by 2*/
			if (is_paff) {
				cts /= 2;
				/*in some cases the poc is not on the top field - if that is the case, round up*/
				if (cts%dts_inc) {
					cts = ((cts/dts_inc)+1)*dts_inc;
				}
			}

			/*B-frames offset*/
			cts += (u32) (max_total_delay*dts_inc);

			samp->CTS_Offset = (u32) (cts - samp->DTS);

			if (samp->CTS_Offset < min_cts_offset)
				min_cts_offset = samp->CTS_Offset;

			if (max_cts < samp->DTS + samp->CTS_Offset) {
				max_cts = samp->DTS + samp->CTS_Offset;
				last_cts_samp = i;
			}
			if (min_cts > samp->DTS + samp->CTS_Offset) {
				min_cts = samp->DTS + samp->CTS_Offset;
			}

			/*this should never happen, however some streams seem to do weird POC increases (cf sorenson streams, last 2 frames),
			this should hopefully take care of some bugs and ensure proper CTS...*/
			if ((s32)samp->CTS_Offset<0) {
				u32 j, k;
				samp->CTS_Offset = 0;
				gf_isom_modify_cts_offset(import->dest, track, i+1, samp->CTS_Offset);
				for (j=last_cts_samp; j<i; j++) {
					GF_ISOSample *asamp = gf_isom_get_sample_info(import->dest, track, j+1, NULL, NULL);
					for (k=j+1; k<=i; k++) {
						GF_ISOSample *bsamp = gf_isom_get_sample_info(import->dest, track, k+1, NULL, NULL);
						if (asamp->CTS_Offset+asamp->DTS==bsamp->CTS_Offset+bsamp->DTS) {
							max_cts += dts_inc;
							bsamp->CTS_Offset = (u32) (max_cts - bsamp->DTS);
							gf_isom_modify_cts_offset(import->dest, track, k+1, bsamp->CTS_Offset);
						}
						gf_isom_sample_del(&bsamp);
					}
					gf_isom_sample_del(&asamp);
				}
				max_cts = samp->DTS + samp->CTS_Offset;
			} else {
				gf_isom_modify_cts_offset(import->dest, track, i+1, samp->CTS_Offset);
			}
			gf_isom_sample_del(&samp);
		}
		if (min_cts_offset > 0) {
			gf_isom_shift_cts_offset(import->dest, track, (s32)min_cts_offset);
			max_cts -= min_cts_offset;
			min_cts -= min_cts_offset;
		}
		/*and repack table*/
		gf_isom_set_cts_packing(import->dest, track, GF_FALSE);

		if (!(import->flags & GF_IMPORT_NO_EDIT_LIST) && min_cts) {
			last_dts = max_cts - min_cts + gf_isom_get_sample_duration(import->dest, track, gf_isom_get_sample_count(import->dest, track) );
			last_dts *= gf_isom_get_timescale(import->dest);
			last_dts /= gf_isom_get_media_timescale(import->dest, track);
			gf_isom_set_edit_segment(import->dest, track, 0, last_dts, min_cts, GF_ISOM_EDIT_NORMAL);
		}
	} else {
		gf_isom_remove_cts_info(import->dest, track);
	}

	gf_set_progress("Importing HEVC", (u32) cur_samp, cur_samp);

	hevc_cfg->nal_unit_size = lhvc_cfg->nal_unit_size = size_length/8;


	//LHVC bitstream with external base layer
	if (min_layer_id != 0) {
		gf_isom_set_visual_info(import->dest, track, di, max_w, max_h);
		//Because layer_id of vps is 0, we need to clone vps from hevc_cfg to lhvc_cfg first
		for (i = 0; i < gf_list_count(hevc_cfg->param_array); i++) {
			u32 j, k, count2;
			GF_HEVCParamArray *s_ar = NULL;
			GF_HEVCParamArray *ar = gf_list_get(hevc_cfg->param_array, i);
			if (ar->type != GF_HEVC_NALU_VID_PARAM) continue;
			count2 = gf_list_count(ar->nalus);
			for (j=0; j<count2; j++) {
				GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, j);
				GF_AVCConfigSlot *sl2;
				u8 layer_id = ((sl->data[0] & 0x1) << 5) | (sl->data[1] >> 3);
				if (layer_id) continue;

				for (k=0; k < gf_list_count(lhvc_cfg->param_array); k++) {
					s_ar = gf_list_get(lhvc_cfg->param_array, k);
					if (s_ar->type==GF_HEVC_NALU_VID_PARAM)
						break;
					s_ar = NULL;
				}
				if (!s_ar) {
					GF_SAFEALLOC(s_ar, GF_HEVCParamArray);
					s_ar->nalus = gf_list_new();
					s_ar->type = GF_HEVC_NALU_VID_PARAM;
					gf_list_insert(lhvc_cfg->param_array, s_ar, 0);
				}
				s_ar->array_completeness = ar->array_completeness;

				GF_SAFEALLOC(sl2, GF_AVCConfigSlot);
				sl2->data = gf_malloc(sl->size);
				memcpy(sl2->data, sl->data, sl->size);
				sl2->id = sl->id;
				sl2->size = sl->size;
				gf_list_add(s_ar->nalus, sl2);
			}
		}
		hevc_set_parall_type(lhvc_cfg);
		//must use LHV1/LHC1 since no base HEVC in the track
		gf_isom_lhvc_config_update(import->dest, track, 1, lhvc_cfg, GF_ISOM_LEHVC_ONLY);
	}
	//HEVC with optional lhvc
	else {
		gf_isom_set_visual_info(import->dest, track, di, max_w_b, max_h_b);
		hevc_set_parall_type(hevc_cfg);
		gf_isom_hevc_config_update(import->dest, track, 1, hevc_cfg);

		if (has_lhvc) {
			hevc_set_parall_type(lhvc_cfg);

			lhvc_cfg->avgFrameRate = hevc_cfg->avgFrameRate;
			lhvc_cfg->constantFrameRate = hevc_cfg->constantFrameRate;
			lhvc_cfg->numTemporalLayers = hevc_cfg->numTemporalLayers;
			lhvc_cfg->temporalIdNested = hevc_cfg->temporalIdNested;

			if (import->flags&GF_IMPORT_SVC_EXPLICIT) {
				gf_isom_lhvc_config_update(import->dest, track, 1, lhvc_cfg, GF_ISOM_LEHVC_WITH_BASE);
				gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_HVCE, 1);
			} else {
				gf_isom_lhvc_config_update(import->dest, track, 1, lhvc_cfg, GF_ISOM_LEHVC_WITH_BASE_BACKWARD);
			}
		}
	}

	if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
		gf_isom_hevc_set_inband_config(import->dest, track, 1);
	}

	if (import->flags & GF_IMPORT_USE_CCST) {
		e = gf_isom_set_image_sequence_coding_constraints(import->dest, track, di, GF_FALSE, GF_FALSE, GF_TRUE, 15);
		if (e) goto exit;
	}
	if (import->is_alpha) {
		e = gf_isom_set_image_sequence_alpha(import->dest, track, di, GF_FALSE);
		if (e) goto exit;
	}

	e = gf_media_update_par(import->dest, track);
	if (e) goto exit;
	{
		/*arbitrary: use the last active SPS*/
		if (hevc.sps[hevc.sps_active_idx].colour_description_present_flag) {
			e = gf_isom_set_visual_color_info(import->dest, track, di, GF_ISOM_SUBTYPE_NCLX, hevc.sps[hevc.sps_active_idx].colour_primaries, hevc.sps[hevc.sps_active_idx].transfer_characteristic, hevc.sps[hevc.sps_active_idx].matrix_coeffs, hevc.sps[hevc.sps_active_idx].video_full_range_flag, NULL, 0);
			if (e) goto exit;
		}
	}
	gf_media_update_bitrate(import->dest, track);

	gf_isom_set_brand_info(import->dest, GF_ISOM_BRAND_ISO4, 1);
	gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_ISOM, 0);

	if (!vpss && !ppss && !spss) {
		e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Import results: No SPS or PPS found in the bitstream ! Nothing imported\n");
	} else {
		if (nb_sp || nb_si) {
			gf_import_message(import, GF_OK, "HEVC Import results: %d samples (%d NALUs) - Slices: %d I %d P %d B %d SP %d SI - %d SEI - %d IDR",
			                  cur_samp, nb_nalus, nb_i, nb_p, nb_b, nb_sp, nb_si, nb_sei, nb_idr);
		} else {
			gf_import_message(import, GF_OK, "HEVC Import results: %d samples (%d NALUs) - Slices: %d I %d P %d B - %d SEI - %d IDR",
			                  cur_samp, nb_nalus, nb_i, nb_p, nb_b, nb_sei, nb_idr);
		}

		if (max_total_delay>1) {
			gf_import_message(import, GF_OK, "Stream uses forward prediction - stream CTS offset: %d frames", max_total_delay);
		}
	}

	if (use_opengop_gdr==2) {
		gf_import_message(import, GF_OK, "OpenGOP detected - adjusting file brand");
		gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_ISO6, 1);
	}

	/*rewrite ESD*/
	if (import->esd) {
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig*) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->predefined = 2;
		import->esd->slConfig->timestampResolution = timescale;
		if (import->esd->decoderConfig) gf_odf_desc_del((GF_Descriptor *)import->esd->decoderConfig);
		import->esd->decoderConfig = gf_isom_get_decoder_config(import->dest, track, 1);
		gf_isom_change_mpeg4_description(import->dest, track, 1, import->esd);
	}

	//base layer (i.e layer with layer_id = 0) not found in bitstream
	//we are importing an LHVC bitstream with external base layer
	//find this base layer with the imported tracks.
	//if we find more than one HEVC/AVC track, return an warning
	if (min_layer_id != 0) {
		u32 avc_base_track, ref_track_id;
		avc_base_track = hevc_base_track = 0;
		for (i = 1; i <= gf_isom_get_track_count(import->dest); i++) {
			u32 subtype = gf_isom_get_media_subtype(import->dest, i, 1);
			switch (subtype) {
			case GF_ISOM_SUBTYPE_AVC_H264:
			case GF_ISOM_SUBTYPE_AVC2_H264:
			case GF_ISOM_SUBTYPE_AVC3_H264:
			case GF_ISOM_SUBTYPE_AVC4_H264:
				if (!avc_base_track) {
					avc_base_track = i;
				} else {
					gf_import_message(import, GF_BAD_PARAM, "Warning: More than one AVC bitstream found, use track %d as base layer", avc_base_track);
				}
				break;
			case GF_ISOM_SUBTYPE_HVC1:
			case GF_ISOM_SUBTYPE_HEV1:
			case GF_ISOM_SUBTYPE_HVC2:
			case GF_ISOM_SUBTYPE_HEV2:
				if (!hevc_base_track) {
					hevc_base_track = i;
					if (avc_base_track) {
						gf_import_message(import, GF_BAD_PARAM, "Warning: Found both AVC and HEVC tracks, using HEVC track %d as base layer", hevc_base_track);
					}
				} else {
					gf_import_message(import, GF_BAD_PARAM, "Warning: More than one HEVC bitstream found, use track %d as base layer", avc_base_track);
				}
				break;
			}
		}
		if (!hevc_base_track && !avc_base_track) {
			gf_import_message(import, GF_BAD_PARAM, "Using LHVC external base layer, but no base layer not found - NOT SETTING SBAS TRACK REFERENCE!");
		} else {
			ref_track_id = gf_isom_get_track_id(import->dest, hevc_base_track ? hevc_base_track : avc_base_track);
			gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_BASE, ref_track_id);
		}
	}

	// This is a L-HEVC bitstream, add linf/cstg. If we have a base HEVC with several temporal sublayers, we don't set linf until we split
	//the sublayers in different tracks
	if (has_lhvc && (cur_vps_id >= 0) && (cur_vps_id < 16) && (hevc.vps[cur_vps_id].max_layers > 1) ) {
		gf_lhevc_set_operating_points_information(import->dest, hevc_base_track, track, &hevc.vps[cur_vps_id], max_temporal_id);
		gf_lhevc_set_layer_information(import->dest, track, &linf[0]);

		//sets track in group of type group_type and id track_group_id. If do_add is GF_FALSE, track is removed from that group
		e = gf_isom_set_track_group(import->dest, track, 1000+gf_isom_get_track_id(import->dest, track), GF_ISOM_BOX_TYPE_CSTG, GF_TRUE);

	}

exit:
	if (sample_data) gf_bs_del(sample_data);
	gf_odf_hevc_cfg_del(hevc_cfg);
	gf_odf_hevc_cfg_del(lhvc_cfg);
	gf_free(buffer);
	gf_bs_del(bs);
	gf_fclose(mdia);
	return e;
#endif //GPAC_DISABLE_HEVC
}

void av1_reset_frame_state(AV1StateFrame *frame_state) {
	if (frame_state->header_obus) {
		while (gf_list_count(frame_state->header_obus)) {
			GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_get(frame_state->header_obus, 0);
			if (a->obu) gf_free(a->obu);
			gf_list_rem(frame_state->header_obus, 0);
			gf_free(a);
		}
		gf_list_del(frame_state->header_obus);
	}

	if (frame_state->frame_obus) {
		while (gf_list_count(frame_state->frame_obus)) {
			GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_get(frame_state->frame_obus, 0);
			if (a->obu) gf_free(a->obu);
			gf_list_rem(frame_state->frame_obus, 0);
			gf_free(a);
		}
		gf_list_del(frame_state->frame_obus);
	}

	memset(frame_state, 0, sizeof(AV1StateFrame));
	frame_state->is_first_frame = GF_TRUE;
}

static Bool probe_webm_matrovska(GF_BitStream *bs)
{
	char probe[64], *found = NULL;
	u64 pos = gf_bs_get_position(bs);
	u32 read = gf_bs_read_data(bs, probe, sizeof(probe) - 1);
	gf_bs_seek(bs, pos);
	probe[read] = 0;
	found = strstr(probe, "webm");
	if (found) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("AV1: guessed unsupported WebM container. Aborting.\n"));
		return GF_TRUE;
	}
	found = strstr(probe, "matroska");
	if (found) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("AV1: guessed unsupported Matrovska container. Aborting.\n"));
		return GF_TRUE;
	}

	return GF_FALSE;
}

static const char* av1_get_bs_syntax_name(GF_Err(*av1_bs_syntax)(GF_BitStream*, AV1State*))
{
	if (av1_bs_syntax == aom_av1_parse_temporal_unit_from_section5) {
		return "OBU section 5";
	} else if (av1_bs_syntax == aom_av1_parse_temporal_unit_from_annexb) {
		return "AnnexB";
	} else if (av1_bs_syntax == aom_av1_parse_temporal_unit_from_ivf) {
		return "IVF";
	} else {
		assert(0);
		return "Unknown";
	}
}

static GF_Err gf_import_aom_av1(GF_MediaImporter *import)
{
#ifdef GPAC_DISABLE_AV1
	return GF_NOT_SUPPORTED;
#else
	GF_Err e = GF_OK;
	GF_AV1Config *av1_cfg = NULL;
	AV1State state;
	FILE *mdia = NULL;
	GF_BitStream *bs = NULL;
	u32 timescale = 0, dts_inc = 0, track_num = 0, track_id = 0, di = 0, cur_samp = 0;
	Bool detect_fps;
	Double FPS = 0.0;
	u64 fsize, pos = 0;
	GF_Err (*parse_temporal_unit)(GF_BitStream*, AV1State*) = NULL;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_OVERRIDE_FPS | GF_IMPORT_FORCE_PACKED;
		return GF_OK;
	}

	memset(&state, 0, sizeof(AV1State));
	av1_cfg = gf_odf_av1_cfg_new();
	state.config = av1_cfg;
	if (import->flags & GF_IMPORT_KEEP_AV1_TEMPORAL_OBU)
		state.keep_temporal_delim = GF_TRUE;

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "AV1: cannot find file %s", import->in_name);

	detect_fps = GF_TRUE;
	FPS = (Double)import->video_fps;
	if (!FPS) {
		FPS = GF_IMPORT_DEFAULT_FPS;
	} else {
		if (import->video_fps == GF_IMPORT_AUTO_FPS) {
			import->video_fps = GF_IMPORT_DEFAULT_FPS;	/*fps=auto is handled as auto-detection in h264*/
		} else {
			/*fps is forced by the caller*/
			detect_fps = GF_FALSE;
		}
	}
	get_video_timing(FPS, &timescale, &dts_inc);

	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);
	fsize = gf_bs_get_size(bs);
	if (!fsize) {
		gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[AV1] Error: bitstream size is 0 byte", import->in_name);
		goto exit;
	}

	if (probe_webm_matrovska(bs))
		goto exit;

	if (import->streamFormat) {
		gf_import_message(import, GF_OK, "AV1: forcing format \"%s\".", import->streamFormat);
		if (!stricmp(import->streamFormat, "obu")) {
			parse_temporal_unit = aom_av1_parse_temporal_unit_from_section5;
		} else if (!stricmp(import->streamFormat, "annexB")) {
			parse_temporal_unit = aom_av1_parse_temporal_unit_from_annexb;
		} else if (!stricmp(import->streamFormat, "ivf")) {
			parse_temporal_unit = aom_av1_parse_temporal_unit_from_ivf;
		} else {
			gf_import_message(import, GF_NOT_SUPPORTED, "AV1: unknown bitstream format \"%s\" found. Only \"obu\", \"annexB\" and \"ivf\" found.", import->streamFormat);
			goto exit;
		}
	} else {
		if (gf_media_probe_ivf(bs)) {
			e = gf_media_aom_parse_ivf_file_header(bs, &state);
			parse_temporal_unit = aom_av1_parse_temporal_unit_from_ivf;

			if (detect_fps && (state.FPS != FPS)) {
				import->video_fps = FPS = state.FPS;
				get_video_timing(FPS, &timescale, &dts_inc);
			}
			pos = gf_bs_get_position(bs);
		} else if (gf_media_aom_probe_annexb(bs)) {
			parse_temporal_unit = aom_av1_parse_temporal_unit_from_annexb;
		} else {
			gf_bs_seek(bs, pos);
			e = aom_av1_parse_temporal_unit_from_section5(bs, &state);
			if (e) {
				gf_import_message(import, GF_NOT_SUPPORTED, "AV1: couldn't guess bitstream format (IVF then Annex B then Section 5 tested).");
				goto exit;
			}
			parse_temporal_unit = aom_av1_parse_temporal_unit_from_section5;
		}
	}
	gf_bs_seek(bs, pos);

	track_id = 0;
	if (import->esd) track_id = import->esd->ESID;
	track_num = gf_isom_new_track(import->dest, track_id, GF_ISOM_MEDIA_VISUAL, timescale);
	if (!track_num) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track_num, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track_num);
	import->final_trackID = gf_isom_get_track_id(import->dest, track_num);
	if (import->esd && import->esd->dependsOnESID) {
		gf_isom_set_track_reference(import->dest, track_num, GF_ISOM_REF_DECODE, import->esd->dependsOnESID);
	}

	while (gf_bs_available(bs)) {
		av1_reset_frame_state(&state.frame_state);
		pos = gf_bs_get_position(bs);

		/*we process each TU and extract only the necessary OBUs*/
		if (parse_temporal_unit(bs, &state) != GF_OK) {
			gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Error parsing %s", av1_get_bs_syntax_name(parse_temporal_unit));
			goto exit;
		}

		/*add sample*/
		{
			u32 frame_obus_idx = 0;
			GF_ISOSample *samp = gf_isom_sample_new();
			samp->DTS = (u64)dts_inc*cur_samp;
			samp->IsRAP = state.frame_state.key_frame ? SAP_TYPE_1 : 0;
			samp->CTS_Offset = 0;

			for (frame_obus_idx = 0; frame_obus_idx < gf_list_count(state.frame_state.frame_obus); ++frame_obus_idx)
				samp->dataLength += (u32)((GF_AV1_OBUArrayEntry*)gf_list_get(state.frame_state.frame_obus, frame_obus_idx))->obu_length;
			samp->data = gf_malloc(samp->dataLength);

			samp->dataLength = 0;
			while (gf_list_count(state.frame_state.frame_obus)) {
				GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_get(state.frame_state.frame_obus, 0);
				if (a->obu) {
					memcpy(samp->data + samp->dataLength, a->obu, (size_t)a->obu_length);
					samp->dataLength += (u32)a->obu_length;
					gf_free(a->obu);
				}
				gf_list_rem(state.frame_state.frame_obus, 0);
				gf_free(a);
			}

			if (cur_samp == 0) {
				while (gf_list_count(state.frame_state.header_obus)) {
					GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_get(state.frame_state.header_obus, 0);
					gf_list_add(av1_cfg->obu_array, a);
					gf_list_rem(state.frame_state.header_obus, 0);
				}

				e = gf_isom_av1_config_new(import->dest, track_num, av1_cfg, NULL, NULL, &di);
				if (e) goto exit;

				gf_import_message(import, GF_OK, "Importing AV1 from %s file - size %dx%d bit-depth %d FPS %d/%d", av1_get_bs_syntax_name(parse_temporal_unit), state.width, state.height, state.bit_depth, timescale, dts_inc);

			} else {
				/*safety check: we only support static metadata*/
				if (gf_list_count(state.frame_state.header_obus) > gf_list_count(av1_cfg->obu_array)) {
					gf_import_message(import, GF_NOT_SUPPORTED, "More header OBUs in frame state than in config");
					goto exit;
				}

				while (gf_list_count(state.frame_state.header_obus)) {
					u32 obu_array_index = 0;
					GF_AV1_OBUArrayEntry *a_hdr = (GF_AV1_OBUArrayEntry*)gf_list_get(state.frame_state.header_obus, 0);
					for (obu_array_index = 0; obu_array_index < gf_list_count(av1_cfg->obu_array); ++obu_array_index) {
						GF_AV1_OBUArrayEntry *a_cfg = (GF_AV1_OBUArrayEntry*)gf_list_get(av1_cfg->obu_array, obu_array_index);
						if (a_cfg->obu_type == a_hdr->obu_type) {
							if (a_cfg->obu_length != a_hdr->obu_length || memcmp(a_cfg->obu, a_hdr->obu, (size_t)a_hdr->obu_length)) {
								gf_import_message(import, GF_NOT_SUPPORTED, "Changing AV1 header OBUs detected for file %s", import->in_name);
								goto exit;
							}
						}
					}
					gf_list_rem(state.frame_state.header_obus, 0);
					if (a_hdr->obu) gf_free(a_hdr->obu);
					gf_free(a_hdr);
				}

			}

			e = gf_isom_add_sample(import->dest, track_num, di, samp);
			if (e) goto exit;
			cur_samp++;

			//write sample deps
			if (import->flags & GF_IMPORT_SAMPLE_DEPS) {
				u32 isLeading, dependsOn, dependedOn, hasRedundant;
				isLeading = 0;
				dependsOn = samp->IsRAP ? 2 : 1;
				dependedOn = state.frame_state.refresh_frame_flags ? 1 : 2;
				hasRedundant = 0;

				e = gf_isom_sample_set_dep_info(import->dest, track_num, cur_samp, isLeading, dependsOn, dependedOn, hasRedundant);
				if (e) goto exit;
			}

			gf_isom_sample_del(&samp);

			gf_set_progress("Importing AV1", gf_bs_get_position(bs), fsize);
		}
	}

	gf_set_progress("Importing AV1", (u32)cur_samp, cur_samp);
	e = gf_isom_set_visual_info(import->dest, track_num, di, state.width, state.height);
	if (e) goto exit;
	e = gf_media_update_par(import->dest, track_num);
	if (e) goto exit;

	e = gf_isom_set_visual_color_info(import->dest, track_num, di, GF_ISOM_SUBTYPE_NCLX, state.color_primaries, state.transfer_characteristics, state.matrix_coefficients, state.color_range, NULL, 0);
	if (e) goto exit;

	gf_media_update_bitrate(import->dest, track_num);
	if (import->flags & GF_IMPORT_USE_CCST) {
		e = gf_isom_set_image_sequence_coding_constraints(import->dest, track_num, di, GF_FALSE, GF_FALSE, GF_TRUE, 15);
		if (e) goto exit;
	}
	if (import->is_alpha) {
		e = gf_isom_set_image_sequence_alpha(import->dest, track_num, di, GF_FALSE);
		if (e) goto exit;
	}

	/*rewrite ESD*/
	if (import->esd) {
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig*)gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->predefined = 2;
		import->esd->slConfig->timestampResolution = timescale;
		if (import->esd->decoderConfig) gf_odf_desc_del((GF_Descriptor *)import->esd->decoderConfig);
		import->esd->decoderConfig = gf_isom_get_decoder_config(import->dest, track_num, 1);
		gf_isom_change_mpeg4_description(import->dest, track_num, 1, import->esd);
	}

	gf_isom_set_brand_info(import->dest, GF_ISOM_BRAND_ISO4, 1);
	gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_ISOM, 0);
	gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_AV01, 1);

exit:
	av1_reset_frame_state(&state.frame_state);
	gf_odf_av1_cfg_del(av1_cfg);
	gf_bs_del(bs);
	gf_fclose(mdia);
	return e;
#endif /*GPAC_DISABLE_AV1*/
}


static GF_Err gf_import_vp9(GF_MediaImporter *import)
{
#ifdef GPAC_DISABLE_VP9
	return GF_NOT_SUPPORTED;
#else
	GF_Err e = GF_OK;
	GF_VPConfig *vp9_cfg = NULL;
	FILE *mdia = NULL;
	GF_BitStream *bs = NULL;
	u32 timescale = 0, dts_inc = 0, track_num = 0, track_id = 0, di = 0, cur_samp = 0, codec_fourcc = 0, num_frames = 0;
	int width = 0, height = 0, renderWidth, renderHeight;
	Double FPS = 0.0;
	u64 pos = 0, fsize = 0;
	Bool forced_fps = GF_FALSE;
	u64 last_pts=0, cumulated_loop_dur=0;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_OVERRIDE_FPS | GF_IMPORT_FORCE_PACKED;
		return GF_OK;
	}

	vp9_cfg = gf_odf_vp_cfg_new();

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "[VP9] cannot find file %s", import->in_name);
	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);
	fsize = gf_bs_get_size(bs);
	if (!fsize) {
		gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[VP9] Error: bitstream size is 0 byte", import->in_name);
		goto exit;
	}

	if (probe_webm_matrovska(bs))
		goto exit;

	e = gf_media_parse_ivf_file_header(bs, &width, &height, &codec_fourcc, &timescale, &dts_inc, &num_frames);
	if (e)
		goto exit;

	FPS = (Double)import->video_fps;
	if (!FPS || import->video_fps == GF_IMPORT_AUTO_FPS) {
		FPS = (double)timescale / dts_inc;
	} else {
		/*fps is forced by the caller*/
		get_video_timing(FPS, &timescale, &dts_inc);
		forced_fps = GF_TRUE;
	}

	track_id = 0;
	if (import->esd) track_id = import->esd->ESID;
	track_num = gf_isom_new_track(import->dest, track_id, GF_ISOM_MEDIA_VISUAL, timescale);
	if (!track_num) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track_num, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track_num);
	import->final_trackID = gf_isom_get_track_id(import->dest, track_num);
	if (import->esd && import->esd->dependsOnESID) {
		gf_isom_set_track_reference(import->dest, track_num, GF_ISOM_REF_DECODE, import->esd->dependsOnESID);
	}

	while (gf_bs_available(bs)) {
		Bool key_frame = GF_FALSE;
		u64 frame_size = 0, pts = 0;
		int num_frames_in_superframe = 0, superframe_index_size = 0, i = 0;
		u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME];

		e = gf_media_parse_ivf_frame_header(bs, &frame_size, &pts);
		if (e) goto exit;

		if (!forced_fps) {
			pts += cumulated_loop_dur;
			if (last_pts && (pts < last_pts) ) {
				pts -= cumulated_loop_dur;
				gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[IVF] Corrupted timestamp "LLU" less than previous timestamp "LLU", assuming concatenation\n", pts, last_pts);
				cumulated_loop_dur = last_pts + gf_isom_get_sample_duration(import->dest, track_num, cur_samp);
				cumulated_loop_dur -= pts;
				pts = cumulated_loop_dur;
			}
			last_pts = pts;
		}
		pos = gf_bs_get_position(bs);
		if (gf_bs_available(bs) < frame_size) {
			gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[VP9] IVF frame size is %u but there is only "LLU" bytes left.", frame_size, gf_bs_available(bs));
			goto exit;
		}

		/*check if it is a superframe*/
		if (vp9_parse_superframe(bs, frame_size, &num_frames_in_superframe, frame_sizes, &superframe_index_size) != GF_OK) {
			gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[VP9] Error parsing sample %u superframe structure", cur_samp);
			goto exit;
		}

		for (i = 0; i < num_frames_in_superframe; ++i) {
			u64 pos2 = gf_bs_get_position(bs);
			if (vp9_parse_sample(bs, vp9_cfg, &key_frame, &width, &height, &renderWidth, &renderHeight) != GF_OK) {
				gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[VP9] Error parsing sample %u", cur_samp);
				goto exit;
			}
			e = gf_bs_seek(bs, pos2 + frame_sizes[i]);
			if (e) {
				gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[VP9] Seek bad param (offset "LLU") at sample %u (1)", pos2 + frame_sizes[i], cur_samp);
				goto exit;
			}
		}
		if (gf_bs_get_position(bs) + superframe_index_size != pos + frame_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[VP9] Inconsistent IVF frame size of "LLU" bytes at sample %u.\n", frame_size, cur_samp));
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("      Detected %d frames (+ %d bytes for the superframe index):\n", num_frames_in_superframe, superframe_index_size));
			for (i = 0; i < num_frames_in_superframe; ++i) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("         superframe %d, size is %u bytes\n", i, frame_sizes[i]));
			}
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("\n"));
		}
		e = gf_bs_seek(bs, pos + frame_size);
		if (e) {
			gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "[VP9] Seek bad param (offset "LLU") at sample %u (2)", pos + frame_size, cur_samp);
			goto exit;
		}

		/*add sample*/
		{
			GF_ISOSample *samp = gf_isom_sample_new();
			samp->DTS = forced_fps ? (u64)dts_inc*cur_samp : pts;
			samp->IsRAP = key_frame ? SAP_TYPE_1 : 0;
			samp->CTS_Offset = 0;
			samp->dataLength = (u32)(gf_bs_get_position(bs) - pos);
			assert(samp->dataLength == frame_size);
			samp->data = gf_malloc(samp->dataLength);
			gf_bs_seek(bs, pos);
			gf_bs_read_data(bs, samp->data, samp->dataLength);

			if (cur_samp == 0) {
				e = gf_isom_vp_config_new(import->dest, track_num, vp9_cfg, NULL, NULL, &di, GF_TRUE);
				if (e) goto exit;
			}

			e = gf_isom_add_sample(import->dest, track_num, di, samp);
			if (e) goto exit;

			gf_isom_sample_del(&samp);

			gf_set_progress("Importing VP9", gf_bs_get_position(bs), fsize);
			cur_samp++;
		}
	}

	gf_set_progress("Importing VP9", cur_samp, cur_samp);
	e = gf_isom_set_visual_info(import->dest, track_num, di, width, height);
#if 0 //TODO: find streams when this happens in render_size()
	if (width != renderWidth) {
		e = gf_isom_set_pixel_aspect_ratio(import->dest, track_num, di, , );
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[VP9] Error setting aspect ratio (%d:%d) with resolution %dx%d\n", xxx, yyy, width, height));
		}
	}
#endif

	e = gf_isom_set_track_layout_info(import->dest, track_num, renderWidth << 16, renderHeight << 16, 0, 0, 0);
	if (e) goto exit;

	gf_media_update_bitrate(import->dest, track_num);

	if (import->flags & GF_IMPORT_USE_CCST) {
		e = gf_isom_set_image_sequence_coding_constraints(import->dest, track_num, di, GF_FALSE, GF_FALSE, GF_TRUE, 15);
		if (e) goto exit;
	}
	if (import->is_alpha) {
		e = gf_isom_set_image_sequence_alpha(import->dest, track_num, di, GF_FALSE);
		if (e) goto exit;
	}

	/*rewrite ESD*/
	if (import->esd) {
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig*)gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->predefined = 2;
		import->esd->slConfig->timestampResolution = timescale;
		if (import->esd->decoderConfig) gf_odf_desc_del((GF_Descriptor *)import->esd->decoderConfig);
		import->esd->decoderConfig = gf_isom_get_decoder_config(import->dest, track_num, 1);
		gf_isom_change_mpeg4_description(import->dest, track_num, 1, import->esd);
	}

exit:
	gf_odf_vp_cfg_del(vp9_cfg);
	gf_bs_del(bs);
	gf_fclose(mdia);
	return e;
#endif /*GPAC_DISABLE_VP9*/
}

static GF_Err gf_import_ivf(GF_MediaImporter *import)
{
	GF_Err e = GF_OK;
	int width = 0, height = 0;
	u32 codec_fourcc = 0, frame_rate = 0, time_scale = 0, num_frames = 0;
	FILE *mdia = NULL;
	GF_BitStream *bs = NULL;

	mdia = gf_fopen(import->in_name, "rb");
	if (!mdia) return gf_import_message(import, GF_URL_ERROR, "Cannot find file %s", import->in_name);
	bs = gf_bs_from_file(mdia, GF_BITSTREAM_READ);

	e = gf_media_parse_ivf_file_header(bs, &width, &height, &codec_fourcc, &frame_rate, &time_scale, &num_frames);
	gf_fclose(mdia);
	gf_bs_del(bs);
	if (e)
		return e;

	switch (codec_fourcc) {
	case GF_4CC('A', 'V', '0', '1'):
		return gf_import_aom_av1(import);
	case GF_4CC('V', 'P', '9', '0'):
		return gf_import_vp9(import);
	default: {
		char *FourCC = (char*)&codec_fourcc;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IVF] Wrong codec FourCC. Only 'AV01' supported, got '%c%c%c%c'\n", FourCC[3], FourCC[2], FourCC[1], FourCC[0]));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	}
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

#ifndef GPAC_DISABLE_OGG

#define OGG_BUFFER_SIZE 4096

Bool OGG_ReadPage(FILE *f_in, ogg_sync_state *oy, ogg_page *oggpage)
{
	if (feof(f_in)) return GF_FALSE;
	while (ogg_sync_pageout(oy, oggpage ) != 1 ) {
		char *buffer;
		if (feof(f_in)) return GF_TRUE;
		buffer = ogg_sync_buffer(oy, OGG_BUFFER_SIZE);
		u32 bytes = (u32) fread(buffer, sizeof(char), OGG_BUFFER_SIZE, f_in);
		if (ogg_sync_wrote(oy, bytes)) return GF_FALSE;
	}
	return GF_TRUE;
}

static u32 get_ogg_serial_no_for_stream(char *fileName, u32 stream_num, Bool is_video)
{
	ogg_sync_state oy;
	u32 track, serial_no;
	ogg_page oggpage;
	ogg_packet oggpacket;
	ogg_stream_state os;
	FILE *f_in;

	/*means first one*/
	if (!stream_num) return 0;

	f_in = gf_fopen(fileName, "rb");
	if (!f_in) return 0;

	track = 0;
	serial_no = 0;
	ogg_sync_init(&oy);
	while (1) {
		if (!OGG_ReadPage(f_in, &oy, &oggpage)) break;
		if (!ogg_page_bos(&oggpage)) break;
		track ++;
		if (track != stream_num) continue;

		serial_no = ogg_page_serialno(&oggpage);
		ogg_stream_init(&os, serial_no);
		ogg_stream_pagein(&os, &oggpage);
		ogg_stream_packetpeek(&os, &oggpacket);

		if (is_video && (oggpacket.bytes >= 7) && !strncmp((char *)&oggpacket.packet[1], "theora", 6)) {
			ogg_stream_clear(&os);
			break;
		}
		if (!is_video && (oggpacket.bytes >= 7) && !strncmp((char *)&oggpacket.packet[1], "vorbis", 6)) {
			ogg_stream_clear(&os);
			break;
		}
		if (!is_video && (oggpage.body_len >= 8) && !strncmp((char *)oggpage.body, "OpusHead", 6)) {
			ogg_stream_clear(&os);
			break;
		}
		ogg_stream_clear(&os);
		serial_no = 0;
	}
	ogg_sync_clear(&oy);
	gf_fclose(f_in);
	return serial_no;
}

GF_Err gf_import_ogg_video(GF_MediaImporter *import)
{
	GF_Err e;
	ogg_sync_state oy;
	u32 di, track;
	u64 tot_size, done, duration;
	u32 w, h, fps_num, fps_den, keyframe_freq_force, theora_kgs, flag, dts_inc, timescale;
	Double FPS;
	Bool destroy_esd, go;
	u32 serial_no, sno, num_headers;
	ogg_packet oggpacket;
	ogg_page oggpage;
	ogg_stream_state os;
	oggpack_buffer opb;
	GF_BitStream *bs;
	FILE *f_in;
	GF_ISOSample *samp;


	dts_inc = 0;
	/*assume audio or simple AV file*/
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		f_in = gf_fopen(import->in_name, "rb");
		if (!f_in) return GF_URL_ERROR;

		import->nb_tracks = 0;
		go = GF_TRUE;
		ogg_sync_init(&oy);
		while (go) {
			if (!OGG_ReadPage(f_in, &oy, &oggpage)) break;

			if (!ogg_page_bos(&oggpage)) {
				go = GF_FALSE;
				continue;
			}
			serial_no = ogg_page_serialno(&oggpage);
			ogg_stream_init(&os, serial_no);
			ogg_stream_pagein(&os, &oggpage);
			ogg_stream_packetpeek(&os, &oggpacket);

			import->tk_info[import->nb_tracks].track_num = import->nb_tracks+1;
			if ((oggpacket.bytes >= 7) && !strncmp((char *)&oggpacket.packet[1], "theora", 6)) {
				import->tk_info[import->nb_tracks].type = GF_ISOM_MEDIA_VISUAL;
				import->tk_info[import->nb_tracks].flags = GF_IMPORT_OVERRIDE_FPS;

				bs = gf_bs_new((char*)oggpacket.packet, oggpacket.bytes, GF_BITSTREAM_READ);
				gf_bs_read_int(bs, 80);
				import->tk_info[import->nb_tracks].video_info.width = gf_bs_read_u16(bs) << 4;
				import->tk_info[import->nb_tracks].video_info.height = gf_bs_read_u16(bs) << 4;
				gf_bs_read_int(bs, 64);
				fps_num = gf_bs_read_u32(bs);
				fps_den = gf_bs_read_u32(bs);
				gf_bs_del(bs);
				import->tk_info[import->nb_tracks].video_info.FPS = fps_num;
				import->tk_info[import->nb_tracks].video_info.FPS /= fps_den;
				import->tk_info[import->nb_tracks].media_type = GF_MEDIA_TYPE_THEO;
			} else if ((oggpacket.bytes >= 7)
				&& (!strncmp((char *)&oggpacket.packet[1], "vorbis", 6) || !strncmp((char *)&oggpacket.packet[1], "Opus", 4))
				) {
				import->tk_info[import->nb_tracks].type = GF_ISOM_MEDIA_AUDIO;
				import->tk_info[import->nb_tracks].flags = 0;
			}
			ogg_stream_clear(&os);
			import->nb_tracks++;
		}
		ogg_sync_clear(&oy);
		gf_fclose(f_in);
		return GF_OK;
	}

	if (import->flags & GF_IMPORT_USE_DATAREF) return gf_import_message(import, GF_NOT_SUPPORTED, "Cannot use data referencing with OGG files");

	sno = get_ogg_serial_no_for_stream(import->in_name, import->trackID, GF_TRUE);
	/*not our stream*/
	if (!sno && import->trackID) return GF_OK;

	f_in = gf_fopen(import->in_name, "rb");
	if (!f_in) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	e = GF_OK;
	done = 0;
	gf_fseek(f_in, 0, SEEK_END);
	tot_size = gf_ftell(f_in);
	gf_fseek(f_in, 0, SEEK_SET);


	destroy_esd = GF_FALSE;
	samp = gf_isom_sample_new();

	/*avoids gcc warnings*/
	duration = 0;
	FPS = 0;
	num_headers = w = h = track = 0;

	ogg_sync_init(&oy);

	bs = NULL;
	serial_no = 0;
	go = GF_TRUE;
	while (go) {
		if (!OGG_ReadPage(f_in, &oy, &oggpage)) break;

		if (ogg_page_bos(&oggpage)) {
			if (serial_no) continue;
			serial_no = ogg_page_serialno(&oggpage);
			ogg_stream_init(&os, serial_no);
			ogg_stream_pagein(&os, &oggpage);
			ogg_stream_packetpeek(&os, &oggpacket);

			/*not our stream*/
			if (sno && (sno!=serial_no)) {
				ogg_stream_clear(&os);
				serial_no = 0;
				continue;
			}
			if ((oggpacket.bytes < 7) || strncmp((char *)&oggpacket.packet[1], "theora", 6)) {
				ogg_stream_clear(&os);
				serial_no = 0;
				continue;
			}
			/*that's ogg-theora*/
			bs = gf_bs_new((char *)oggpacket.packet, oggpacket.bytes, GF_BITSTREAM_READ);
			gf_bs_read_int(bs, 80);
			w = gf_bs_read_u16(bs) << 4;
			h = gf_bs_read_u16(bs) << 4;
			gf_bs_read_int(bs, 64);
			fps_num = gf_bs_read_u32(bs);
			fps_den = gf_bs_read_u32(bs);
			gf_bs_read_int(bs, 80);
			gf_bs_read_int(bs, 6);
			keyframe_freq_force = 1 << gf_bs_read_int(bs, 5);
			theora_kgs = 0;
			keyframe_freq_force--;
			while (keyframe_freq_force) {
				theora_kgs ++;
				keyframe_freq_force >>= 1;
			}
			gf_bs_del(bs);

			FPS = ((Double)fps_num) / fps_den;

			/*note that we don't rewrite theora headers (just like in MPEG-4 video, systems timing overrides stream one)*/
			if (import->video_fps) FPS = import->video_fps;
			num_headers = 0;

			gf_import_message(import, GF_OK, "OGG Theora import - %2.4f FPS - Resolution %d x %d", FPS, w, h);
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			continue;
		}
		/*FIXME - check end of stream for concatenated files?*/

		/*not our stream*/
		if (ogg_stream_pagein(&os, &oggpage) != 0) continue;



		while (ogg_stream_packetout(&os, &oggpacket ) > 0 ) {
			if (num_headers<3) {
				if(!w || !h) {
					e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Couldn't find Theora main header");
					goto exit;
				}
				/*copy headers*/
				gf_bs_write_u16(bs, oggpacket.bytes);
				gf_bs_write_data(bs, (char *)oggpacket.packet, oggpacket.bytes);
				num_headers++;

				/*let's go, create the track*/
				if (num_headers==3) {
					if (!import->esd) {
						destroy_esd = GF_TRUE;
						import->esd = gf_odf_desc_esd_new(0);
					}
					get_video_timing(FPS, &timescale, &dts_inc);
					track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_VISUAL, timescale);
					if (!track) goto exit;
					gf_isom_set_track_enabled(import->dest, track, 1);
					if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
					import->final_trackID = import->esd->ESID;
					if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
					if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
					import->esd->slConfig->timestampResolution = timescale;
					if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
					gf_bs_get_content(bs, &import->esd->decoderConfig->decoderSpecificInfo->data, &import->esd->decoderConfig->decoderSpecificInfo->dataLength);
					gf_bs_del(bs);
					bs = NULL;
					import->esd->decoderConfig->streamType = GF_STREAM_VISUAL;
					import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_THEORA;

					e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, NULL, NULL, &di);
					if (e) goto exit;
					gf_isom_set_visual_info(import->dest, track, di, w, h);

					{
						Double d = import->duration;
						d *= import->esd->slConfig->timestampResolution;
						d /= 1000;
						duration = (u64) d;
					}
				}

				continue;
			}

			/*we don't need adedicated parser for theora, just check it's a theora frame and get its key type
			WATCHOUT theora bitsteram is in BE*/
			oggpackB_readinit(&opb, oggpacket.packet, oggpacket.bytes);
			flag = oggpackB_read(&opb, 1);
			if (flag==0) {
				/*add packet*/
				samp->IsRAP = oggpackB_read(&opb, 1) ? RAP_NO : RAP;
				samp->data = (char *)oggpacket.packet;
				samp->dataLength = oggpacket.bytes;
				e = gf_isom_add_sample(import->dest, track, di, samp);
				if (e) goto exit;
				samp->DTS += dts_inc;
			}

			gf_set_progress("Importing OGG Video", (u32) (done/1024), (u32) (tot_size/1024));
			done += oggpacket.bytes;
			if ((duration && (samp->DTS > duration) ) || (import->flags & GF_IMPORT_DO_ABORT)) {
				go = GF_FALSE;
				break;
			}
		}
	}
	gf_set_progress("Importing OGG Video", (u32) (tot_size/1024), (u32) (tot_size/1024));

	if (!serial_no) {
		gf_import_message(import, GF_OK, "OGG: No supported video found");
	} else {
		gf_media_update_bitrate(import->dest, track);

		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_VISUAL, 0xFE);
	}

exit:
	if (bs) gf_bs_del(bs);
	samp->data = NULL;
	gf_isom_sample_del(&samp);
	ogg_sync_clear(&oy);
	if (serial_no) ogg_stream_clear(&os);
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	gf_fclose(f_in);
	return e;
}

static void vorbis_release(ogg_audio_codec_desc *codec)
{
	if (codec->parserPrivateState) {
		GF_VorbisParser *vp = (GF_VorbisParser*)codec->parserPrivateState;
		if (vp->vbs) gf_bs_del(vp->vbs);
		gf_free(vp);
	}
}

static GF_Err vorbis_process(ogg_audio_codec_desc *codec, char *data, u32 data_length,
	void *importer, Bool *destroy_esd, u32 *track, u32 *di, u64 *duration, int *block_size)
{
	GF_VorbisParser *vp = NULL;
	GF_Err e = GF_OK;
	GF_MediaImporter *import = (GF_MediaImporter*)importer;

	if (!codec || !import || !track || !di || !destroy_esd || !duration || !block_size)
		return GF_BAD_PARAM;

	vp = (GF_VorbisParser*)codec->parserPrivateState;

	if (!vp || vp->num_headers<3) {
		if (!gf_vorbis_parse_header(codec, (char*)data, data_length)) {
			e = GF_NON_COMPLIANT_BITSTREAM;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("Corrupted OGG Vorbis header"));
			goto exit;
		}
		vp = (GF_VorbisParser*)codec->parserPrivateState;

		/*copy headers*/
		gf_bs_write_u16(vp->vbs, data_length);
		gf_bs_write_data(vp->vbs, (char *)data, data_length);
		vp->num_headers++;

		/*let's go, create the track*/
		if (vp->num_headers == 3) {
			if (!codec->parserPrivateState) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("Corrupted OGG Vorbis headers found"));
				goto exit;
			}

			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("OGG Vorbis import - sample rate %d - %d channel%s", codec->sample_rate, codec->channels, (codec->channels>1) ? "s" : ""));

			if (!import->esd) {
				*destroy_esd = GF_TRUE;
				import->esd = gf_odf_desc_esd_new(0);
			}
			*track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, codec->sample_rate);
			if (!*track) {
				e = gf_isom_last_error(import->dest);
				goto exit;
			}
			gf_isom_set_track_enabled(import->dest, *track, 1);
			if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, *track);
			import->final_trackID = import->esd->ESID;
			if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *)gf_odf_desc_new(GF_ODF_DCD_TAG);
			if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);
			import->esd->slConfig->timestampResolution = codec->sample_rate;
			if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *)gf_odf_desc_new(GF_ODF_DSI_TAG);
			gf_bs_get_content(vp->vbs, &import->esd->decoderConfig->decoderSpecificInfo->data, &import->esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_bs_del(vp->vbs);
			vp->vbs = NULL;
			import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
			import->esd->decoderConfig->avgBitrate = vp->avg_r;
			import->esd->decoderConfig->maxBitrate = (vp->max_r>0) ? vp->max_r : vp->avg_r;
			import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_OGG;

			e = gf_isom_new_mpeg4_description(import->dest, *track, import->esd, NULL, NULL, di);
			if (e) goto exit;
			gf_isom_set_audio_info(import->dest, *track, *di, codec->sample_rate, (codec->channels>1) ? 2 : 1, 16, import->asemode);

			{
				Double d = import->duration;
				d *= codec->sample_rate;
				d /= 1000;
				*duration = (u64)d;
			}
		}

		*block_size = 0;
		return GF_OK;
	}

	*block_size = gf_vorbis_check_frame(vp, (char *)data, data_length);

exit:
	return e;
}

static void opus_release(ogg_audio_codec_desc *codec)
{
	if (codec->parserPrivateState) {
		GF_OpusSpecificBox *opus = (GF_OpusSpecificBox*)codec->parserPrivateState;
		gf_free(opus);
	}
}

static GF_Err opus_process(ogg_audio_codec_desc *codec, char *data, u32 data_length,
	void *importer, Bool *destroy_esd, u32 *track, u32 *di, u64 *duration, int *block_size)
{
	GF_OpusSpecificBox *opus = NULL;
	GF_Err e = GF_OK;
	GF_MediaImporter *import = (GF_MediaImporter*)importer;
	GF_BitStream *bs = NULL;
	char tag[8];

	if (!codec || !import || !track || !di || !destroy_esd || !duration || !block_size)
		return GF_BAD_PARAM;

	*block_size = 0;
	opus = (GF_OpusSpecificBox*)codec->parserPrivateState;
	bs = gf_bs_new(data, data_length, GF_BITSTREAM_READ);
	gf_bs_read_data(bs, tag, 8);

	if (!opus) {
		/*Identification Header*/
		u8 val;
		val = gf_bs_read_u8(bs); /*version*/
		if (val != 1) {
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}

		GF_SAFEALLOC(opus, GF_OpusSpecificBox);
		codec->parserPrivateState = (void*)opus;
		codec->channels = opus->OutputChannelCount = gf_bs_read_u8(bs);
		opus->PreSkip = gf_bs_read_u16_le(bs);
		opus->InputSampleRate = gf_bs_read_u32_le(bs);
		codec->sample_rate = 48000; /*Opus always outputs 48000 but stores the original opus->InputSampleRate rate*/
		opus->OutputGain = gf_bs_read_u16_le(bs);

		//TODO: parse and link to MP4 channel layouts - for now we just copy it binary as it is the same from Ogg to MP4
		opus->ChannelMappingFamily = gf_bs_read_u8(bs);
		if (opus->ChannelMappingFamily != 0) {
			opus->StreamCount = gf_bs_read_u8(bs);
			opus->CoupledCount = gf_bs_read_u8(bs);
			gf_bs_read_data(bs, (char *) opus->ChannelMapping, opus->OutputChannelCount);
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("OGG Opus import - sample rate %d - %d channel%s", codec->sample_rate, codec->channels, (codec->channels>1) ? "s" : ""));

		if (!import->esd) {
			*destroy_esd = GF_TRUE;
			import->esd = gf_odf_desc_esd_new(0);
		}
		*track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, 48000/*block sizes are alway in always in 48kHz*/);
		if (!*track) {
			e = gf_isom_last_error(import->dest);
			goto exit;
		}
		gf_isom_set_track_enabled(import->dest, *track, 1);
		if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, *track);
		import->final_trackID = import->esd->ESID;
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *)gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->timestampResolution = 48000; /*block sizes are alway in always in 48kHz*/

		import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_OPUS;

		if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *)gf_odf_desc_new(GF_ODF_DSI_TAG);
		e = gf_isom_opus_config_new(import->dest, *track, opus, NULL, NULL, di);
		if (e) goto exit;

		gf_isom_set_audio_info(import->dest, *track, *di, codec->sample_rate, codec->channels, 16, import->asemode);
		gf_isom_modify_alternate_brand(import->dest, GF_ISOM_BRAND_OPUS, 0);

		{
			Double d = import->duration;
			d *= codec->sample_rate;
			d /= 1000;
			*duration = (u64)d;
		}
	} else if (!memcmp(tag, "OpusTags", sizeof(tag))) {
		/*skip*/
		goto exit;
	} else {
		/*consider the whole packet as Ogg packets and ISOBMFF samples for Opus are framed similarly*/
		static const int OpusFrameDurIn48k[] = { 480, 960, 1920, 2880, 480, 960, 1920, 2880, 480, 960, 1920, 2880,
			480, 960, 480, 960,
			120, 240, 480, 960, 120, 240, 480, 960, 120, 240, 480, 960, 120, 240, 480, 960,
		};
		int TOC_config = (data[0] & 0xf8) >> 3;
		//int s = (data[0] & 0x04) >> 2;
		*block_size = OpusFrameDurIn48k[TOC_config];

		int c = data[0] & 0x03;
		if (c == 1 || c == 2) {
			*block_size *= 2;
		} else if (c == 3) {
			/*unknown number of frames*/
			int num_frames = data[1] & 0x3f;
			*block_size *= num_frames;
		}
	}

exit:
	gf_bs_del(bs);
	return GF_OK;
}

static ogg_audio_codec_desc ogg_audio_codec_descs[] = {
	{ "\1vorbis", NULL/*GF_VorbisParser*/, 0, 0, vorbis_process, vorbis_release },
	{ "OpusHead", NULL/*GF_OpusParser*/, 0, 0, opus_process, opus_release },
};

GF_Err gf_import_ogg_audio(GF_MediaImporter *import)
{
#if defined(GPAC_DISABLE_AV_PARSERS)
	return GF_NOT_SUPPORTED;
#else

	GF_Err e;
	ogg_sync_state oy;
	u32 di, track;
	u64 done, tot_size, duration;
	s32 block_size;
	GF_ISOSample *samp;
	Bool destroy_esd, go;
	u32 serial_no, sno;
	ogg_packet oggpacket;
	ogg_page oggpage;
	ogg_stream_state os;
	ogg_audio_codec_desc *codec = NULL;
	FILE *f_in;

	if (import->flags & GF_IMPORT_PROBE_ONLY) return GF_OK;

	if (import->flags & GF_IMPORT_USE_DATAREF) return gf_import_message(import, GF_NOT_SUPPORTED, "Cannot use data referencing with OGG files");

	sno = get_ogg_serial_no_for_stream(import->in_name, import->trackID, GF_FALSE);
	/*not our stream*/
	if (!sno && import->trackID) return GF_OK;

	f_in = gf_fopen(import->in_name, "rb");
	if (!f_in) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	e = GF_OK;

	done = 0;
	gf_fseek(f_in, 0, SEEK_END);
	tot_size = gf_ftell(f_in);
	gf_fseek(f_in, 0, SEEK_SET);

	destroy_esd = import->esd ? GF_FALSE : GF_TRUE;
	samp = gf_isom_sample_new();
	/*avoids gcc warnings*/
	track = 0;
	duration = 0;

	ogg_sync_init(&oy);

	serial_no = 0;
	go = GF_TRUE;
	while (go) {
		if (!OGG_ReadPage(f_in, &oy, &oggpage)) break;

		if (ogg_page_bos(&oggpage)) {
			if (serial_no) continue;
			serial_no = ogg_page_serialno(&oggpage);
			ogg_stream_init(&os, serial_no);
			ogg_stream_pagein(&os, &oggpage);
			ogg_stream_packetpeek(&os, &oggpacket);
			/*not our stream*/
			if (sno && (sno!=serial_no)) {
				ogg_stream_clear(&os);
				serial_no = 0;
				continue;
			}
			if (oggpacket.bytes < 7) {
				ogg_stream_clear(&os);
				serial_no = 0;
				continue;
			}

			/*find codec*/
			{
				size_t size = sizeof(ogg_audio_codec_descs) / sizeof(ogg_audio_codec_desc);
				size_t i;
				for (i = 0; i < size; ++i) {
					if (!strncmp((char *)oggpacket.packet, ogg_audio_codec_descs[i].codec_name, strlen(ogg_audio_codec_descs[i].codec_name))) {
						codec = &ogg_audio_codec_descs[i];
						break;
					}
				}
				if (i == size) {
					ogg_stream_clear(&os);
					serial_no = 0;
					continue;
				}
			}

			continue;
		}
		/*FIXME - check end of stream for concatenated files?*/

		/*not our stream*/
		if (ogg_stream_pagein(&os, &oggpage) != 0) continue;

		while (ogg_stream_packetout(&os, &oggpacket ) > 0 ) {
			if (!codec) {
				e = gf_import_message(import, GF_NOT_SUPPORTED, "OGG: unrecognized codec");
				goto exit;
			}

			e = codec->process(codec, oggpacket.packet, oggpacket.bytes, import, &destroy_esd, &track, &di, &duration, &block_size);
			if (e) goto exit;
			if (!block_size) continue;

			/*add packet*/
			samp->IsRAP = RAP;
			samp->data = (char *)oggpacket.packet;
			samp->dataLength = oggpacket.bytes;
			e = gf_isom_add_sample(import->dest, track, di, samp);
			if (e) goto exit;
			samp->DTS += block_size;

			gf_set_progress("Importing OGG Audio", (u32) done, (u32) tot_size);
			done += oggpacket.bytes;
			if ((duration && (samp->DTS > duration) ) || (import->flags & GF_IMPORT_DO_ABORT)) {
				go = GF_FALSE;
				break;
			}
		}
	}
	gf_set_progress("Importing OGG Audio", (u32) tot_size, (u32) tot_size);

	if (!serial_no) {
		gf_import_message(import, GF_OK, "OGG: No supported audio found");
	} else {
		samp->data = NULL;
		gf_isom_set_pl_indication(import->dest, GF_ISOM_PL_AUDIO, 0xFE);
		gf_set_progress("Importing OGG Audio", (u32) tot_size, (u32) tot_size);

		gf_media_update_bitrate(import->dest, track);

		/*rewrite ESD*/
		if (import->esd) {
			if (import->esd->decoderConfig) gf_odf_desc_del((GF_Descriptor *)import->esd->decoderConfig);
			import->esd->decoderConfig = gf_isom_get_decoder_config(import->dest, track, 1);
			gf_isom_change_mpeg4_description(import->dest, track, 1, import->esd);
		}
	}

exit:
	gf_isom_sample_del(&samp);
	if (serial_no) ogg_stream_clear(&os);
	ogg_sync_clear(&oy);
	if (codec && codec->release) codec->release(codec);

	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	gf_fclose(f_in);
	return e;
#endif /*defined(GPAC_DISABLE_AV_PARSERS) */
}

#endif /*GPAC_DISABLE_OGG*/

GF_Err gf_import_raw_unit(GF_MediaImporter *import)
{
	GF_Err e;
	GF_ISOSample *samp;
	u32 mtype, track, di, timescale, read;
	FILE *src;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->flags |= GF_IMPORT_USE_DATAREF;
		import->nb_tracks = 1;
		 import->tk_info[0].track_num = 1;
		return GF_OK;
	}

	if (!import->esd || !import->esd->decoderConfig) {
		return gf_import_message(import, GF_BAD_PARAM, "Raw stream needs ESD and DecoderConfig for import");
	}

	src = gf_fopen(import->in_name, "rb");
	if (!src) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	switch (import->esd->decoderConfig->streamType) {
	case GF_STREAM_SCENE:
		mtype = GF_ISOM_MEDIA_SCENE;
		break;
	case GF_STREAM_VISUAL:
		mtype = GF_ISOM_MEDIA_VISUAL;
		break;
	case GF_STREAM_AUDIO:
		mtype = GF_ISOM_MEDIA_AUDIO;
		break;
	case GF_STREAM_TEXT:
		mtype = GF_ISOM_MEDIA_TEXT;
		break;
	case GF_STREAM_MPEG7:
		mtype = GF_ISOM_MEDIA_MPEG7;
		break;
	case GF_STREAM_IPMP:
		mtype = GF_ISOM_MEDIA_IPMP;
		break;
	case GF_STREAM_OCI:
		mtype = GF_ISOM_MEDIA_OCI;
		break;
	case GF_STREAM_MPEGJ:
		mtype = GF_ISOM_MEDIA_MPEGJ;
		break;
	case GF_STREAM_INTERACT:
		mtype = GF_STREAM_SCENE;
		break;
	/*not sure about this one...*/
	case GF_STREAM_IPMP_TOOL:
		mtype = GF_ISOM_MEDIA_IPMP;
		break;
	/*not sure about this one...*/
	case GF_STREAM_FONT:
		mtype = GF_ISOM_MEDIA_MPEGJ;
		break;
	default:
		mtype = GF_ISOM_MEDIA_ESM;
	}
	timescale = import->esd->slConfig ? import->esd->slConfig->timestampResolution : 1000;
	track = gf_isom_new_track(import->dest, import->esd->ESID, mtype, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;
	e = gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	if (e) goto exit;

	gf_import_message(import, GF_OK, "Raw Access Unit import (StreamType %s)", gf_odf_stream_type_name(import->esd->decoderConfig->streamType));

	samp = gf_isom_sample_new();
	gf_fseek(src, 0, SEEK_END);
	assert(gf_ftell(src) < 1<<31);
	samp->dataLength = (u32) gf_ftell(src);
	gf_fseek(src, 0, SEEK_SET);
	samp->IsRAP = RAP;
	samp->data = (char *)gf_malloc(sizeof(char)*samp->dataLength);
	read = (u32) fread(samp->data, sizeof(char), samp->dataLength, src);
	if ( read != samp->dataLength ) {
		e = gf_import_message(import, GF_IO_ERR, "Failed to read raw unit %d bytes", samp->dataLength);
		goto exit;

	}
	e = gf_isom_add_sample(import->dest, track, di, samp);
	gf_isom_sample_del(&samp);
	gf_media_update_bitrate(import->dest, track);
exit:
	gf_fclose(src);
	return e;
}


GF_Err gf_import_saf(GF_MediaImporter *import)
{
#ifndef GPAC_DISABLE_SAF
	GF_Err e;
	u32 track;
	u64 tot;
	FILE *saf;
	GF_BitStream *bs;
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->flags |= GF_IMPORT_USE_DATAREF;
	}

	saf = gf_fopen(import->in_name, "rb");
	if (!saf) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	track = 0;

	bs = gf_bs_from_file(saf, GF_BITSTREAM_READ);
	tot = gf_bs_get_size(bs);

	while (gf_bs_available(bs)) {
		Bool is_rap;
		u32 cts, au_size, type, stream_id;
		is_rap = (Bool)gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 15);
		gf_bs_read_int(bs, 2);
		cts = gf_bs_read_int(bs, 30);
		au_size = gf_bs_read_u16(bs);
		if (au_size<2) {
			gf_bs_del(bs);
			gf_fclose(saf);
			return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Invalid SAF Packet Header");
		}
		type = gf_bs_read_int(bs, 4);
		stream_id = gf_bs_read_int(bs, 12);
		au_size-=2;
		if (!stream_id) stream_id = 1000;

		if ((type==1) || (type==2) || (type==7)) {
			Bool in_root_od = GF_FALSE;
			u32 mtype, stype;
			char *name = "Unknown";

			u8 oti = gf_bs_read_u8(bs);
			u8 st = gf_bs_read_u8(bs);
			u32 ts_res = gf_bs_read_u24(bs);
			u32 buffersize_db = gf_bs_read_u16(bs);

			if (!ts_res) ts_res = 1000;

			au_size -= 7;


			mtype = GF_ISOM_MEDIA_ESM;
			stype = 0;
			if (st==GF_STREAM_SCENE) {
				mtype = GF_ISOM_MEDIA_SCENE;
				name = (char *) ( (oti==GPAC_OTI_SCENE_LASER) ? "LASeR Scene" : "BIFS Scene" );
				stype = (oti==GPAC_OTI_SCENE_LASER) ? GF_MEDIA_TYPE_LASR : GF_MEDIA_TYPE_BIFS;
				in_root_od = GF_TRUE;
			}
			else if (st==GF_STREAM_VISUAL) {
				mtype = GF_ISOM_MEDIA_VISUAL;
				switch (oti) {
				case GPAC_OTI_VIDEO_AVC:
				case GPAC_OTI_VIDEO_SVC:
				case GPAC_OTI_VIDEO_MVC:
					name = "AVC/H264 Video";
					stype = GF_MEDIA_TYPE_H264;
					break;
				case GPAC_OTI_VIDEO_HEVC:
				case GPAC_OTI_VIDEO_LHVC:
					name = "HEVC Video";
					stype = GF_MEDIA_TYPE_HEVC;
					break;
				case GPAC_OTI_VIDEO_MPEG4_PART2:
					name = "MPEG-4 Video";
					stype = GF_MEDIA_TYPE_MP4V;
					break;
				case GPAC_OTI_VIDEO_MPEG1:
					name = "MPEG-1 Video";
					stype = GF_MEDIA_TYPE_MP1V;
					break;
				case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
				case GPAC_OTI_VIDEO_MPEG2_MAIN:
				case GPAC_OTI_VIDEO_MPEG2_SNR:
				case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
				case GPAC_OTI_VIDEO_MPEG2_HIGH:
				case GPAC_OTI_VIDEO_MPEG2_422:
					name = "MPEG-2 Video";
					stype = GF_MEDIA_TYPE_MP2V;
					break;
				case GPAC_OTI_IMAGE_JPEG:
					name = "JPEG Image";
					stype = GF_MEDIA_TYPE_JPEG;
					break;
				case GPAC_OTI_IMAGE_PNG:
					name = "PNG Image";
					stype = GF_MEDIA_TYPE_PNG;
					break;
				}
			}
			else if (st==GF_STREAM_AUDIO) {
				mtype = GF_ISOM_MEDIA_AUDIO;
				switch (oti) {
				case GPAC_OTI_AUDIO_MPEG2_PART3:
					name = "MPEG-2 Audio";
					stype = GF_MEDIA_TYPE_MP2A;
					break;
				case GPAC_OTI_AUDIO_MPEG1:
					name = "MPEG-1 Audio";
					stype = GF_MEDIA_TYPE_MP1A;
					break;
				case GPAC_OTI_AUDIO_AAC_MPEG4:
					name = "MPEG-4 Audio";
					stype = GF_MEDIA_TYPE_MP4A;
					break;
				}
			}


			if (import->flags & GF_IMPORT_PROBE_ONLY) {
				u32 i, found;
				found = 0;
				for (i=0; i<import->nb_tracks; i++) {
					if (import->tk_info[i].track_num==stream_id) {
						found = 1;
						break;
					}
				}
				if (!found) {
					import->tk_info[import->nb_tracks].media_type = stype;
					import->tk_info[import->nb_tracks].type = mtype;
					import->tk_info[import->nb_tracks].flags = GF_IMPORT_USE_DATAREF;
					import->tk_info[import->nb_tracks].track_num = stream_id;
					import->nb_tracks++;
				}
			} else if ((stream_id==import->trackID) && !track) {
				Bool delete_esd = GF_FALSE;
				if (!import->esd) {
					import->esd = gf_odf_desc_esd_new(0);
					delete_esd = GF_TRUE;
					if (import->esd->URLString) gf_free(import->esd->URLString);
					import->esd->URLString = NULL;
				}
				import->esd->decoderConfig->streamType = st;
				import->esd->decoderConfig->objectTypeIndication = oti;
				import->esd->decoderConfig->bufferSizeDB = buffersize_db;
				if ((st==0xFF) && (oti==0xFF)) {
					assert(0);
				}
				if (type==7) {
					u32 url_len = gf_bs_read_u16(bs);
					import->esd->URLString = (char *)gf_malloc(sizeof(char)*(url_len+1));
					gf_bs_read_data(bs, import->esd->URLString, url_len);
					import->esd->URLString[url_len] = 0;
					au_size-=2+url_len;
				}
				if (au_size) {
					if (!import->esd->decoderConfig->decoderSpecificInfo) import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
					if (import->esd->decoderConfig->decoderSpecificInfo->data ) gf_free(import->esd->decoderConfig->decoderSpecificInfo->data);
					import->esd->decoderConfig->decoderSpecificInfo->dataLength = au_size;
					import->esd->decoderConfig->decoderSpecificInfo->data = (char *)gf_malloc(sizeof(char)*au_size);
					gf_bs_read_data(bs, import->esd->decoderConfig->decoderSpecificInfo->data, au_size);
					au_size = 0;
				}
				if (gf_isom_get_track_by_id(import->dest, stream_id)) stream_id = 0;
				track = gf_isom_new_track(import->dest, stream_id, mtype, ts_res);
				gf_isom_set_track_enabled(import->dest, track, 1);
				stream_id = import->final_trackID = import->esd->ESID = gf_isom_get_track_id(import->dest, track);
				gf_isom_new_mpeg4_description(import->dest, track, import->esd, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &mtype);
				if (delete_esd) {
					gf_odf_desc_del((GF_Descriptor *) import->esd);
					import->esd = NULL;
				}
				if (in_root_od) gf_isom_add_track_to_root_od(import->dest, track);
				gf_import_message(import, GF_OK, "Importing SAF stream %d: %s", stream_id, name);
			}
		}
		else if ((type==4) && (stream_id==import->trackID) && track) {
			GF_ISOSample *samp = gf_isom_sample_new();
			samp->dataLength = au_size;
			samp->DTS = cts;
			samp->IsRAP = is_rap ? RAP : RAP_NO;
			if (import->flags & GF_IMPORT_USE_DATAREF) {
				e = gf_isom_add_sample_reference(import->dest, track, 1, samp, gf_bs_get_position(bs) );
			} else {
				samp->data = (char *)gf_malloc(sizeof(char)*samp->dataLength);
				gf_bs_read_data(bs, samp->data, samp->dataLength);
				au_size = 0;
				e = gf_isom_add_sample(import->dest, track, 1, samp);
			}
			gf_isom_sample_del(&samp);
			if (e) {
				gf_bs_del(bs);
				gf_fclose(saf);
				return e;
			}
			gf_set_progress("Importing SAF", gf_bs_get_position(bs), tot);
		}
		gf_bs_skip_bytes(bs, au_size);
	}
	gf_bs_del(bs);
	gf_fclose(saf);
	if (import->flags & GF_IMPORT_PROBE_ONLY) return GF_OK;

	gf_set_progress("Importing SAF", tot, tot);
	gf_media_update_bitrate(import->dest, track);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

typedef struct
{
	GF_MediaImporter *import;
	u32 track;
	u32 nb_i, nb_p, nb_b;
	u64 last_dts;
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_AVCConfig *avccfg;
	AVCState avc;

#ifndef GPAC_DISABLE_HEVC
	GF_HEVCConfig *hevccfg;
	HEVCState hevc;
#endif //GPAC_DISABLE_HEVC

#endif
	Bool force_next_au_start;
	Bool stream_setup;
	u32 nb_video, nb_video_configured;
	u32 nb_audio, nb_audio_configured;

	Bool is_substream;
} GF_TSImport;

#ifndef GPAC_DISABLE_MPEG2TS

/* Determine the ESD corresponding to the current track info based on the PID and sets the additional info
   in the track info as described in this esd */
static void m2ts_set_track_mpeg4_probe_info(GF_M2TS_ES *es, GF_ESD *esd,
        struct __track_import_info* tk_info)
{
	if (esd && tk_info) {
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_SCENE:
			tk_info->type = GF_ISOM_MEDIA_SCENE;
			break;
		case GF_STREAM_VISUAL:
			tk_info->type = GF_ISOM_MEDIA_VISUAL;
			break;
		case GF_STREAM_AUDIO:
			tk_info->type = GF_ISOM_MEDIA_AUDIO;
			break;
		case GF_STREAM_MPEG7:
			tk_info->type = GF_ISOM_MEDIA_MPEG7;
			break;
		case GF_STREAM_IPMP:
			tk_info->type = GF_ISOM_MEDIA_IPMP;
			break;
		case GF_STREAM_OCI:
			tk_info->type = GF_ISOM_MEDIA_OCI;
			break;
		case GF_STREAM_MPEGJ:
			tk_info->type = GF_ISOM_MEDIA_MPEGJ;
			break;
		case GF_STREAM_OD:
			tk_info->type = GF_ISOM_MEDIA_OD;
			break;
		case GF_STREAM_INTERACT:
			tk_info->type = GF_ISOM_MEDIA_SCENE;
			break;
		default:
			tk_info->type = GF_ISOM_MEDIA_ESM;
			break;
		}
	}
}

static void m2ts_set_tracks_mpeg4_probe_info(GF_MediaImporter *import, GF_M2TS_Program *prog, GF_List *ESDescriptors)
{
	u32 i, k, esd_count, stream_count;
	s32 tk_idx;

	esd_count = gf_list_count(ESDescriptors);
	stream_count = gf_list_count(prog->streams);
	for (k = 0; k < esd_count; k++) {
		GF_M2TS_ES *es = NULL;
		GF_ESD *esd = (GF_ESD *)gf_list_get(ESDescriptors, k);

		for (i = 0; i < stream_count; i++) {
			GF_M2TS_ES *es_tmp = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
			if (es_tmp->mpeg4_es_id == esd->ESID) {
				es = es_tmp;
				break;
			}
		}
		if (es == NULL) continue;

		if (esd->decoderConfig->streamType==GF_STREAM_OD)
			es->flags |= GF_M2TS_ES_IS_MPEG4_OD;


		tk_idx = -1;
		for (i = 0; i < import->nb_tracks; i++) {
			if (import->tk_info[i].track_num == es->pid) {
				tk_idx = i;
				break;
			}
		}

		if (tk_idx == -1) continue;
		if (import->tk_info[tk_idx].type != 0 && import->tk_info[tk_idx].type != GF_ISOM_MEDIA_ESM) continue;

		m2ts_set_track_mpeg4_probe_info(es, esd, &import->tk_info[tk_idx]);
	}

}

static void m2ts_set_track_mpeg4_creation_info(GF_MediaImporter *import, u32 *mtype, u32 *stype, u32 *oti)
{
	if (import->esd) {
		*stype = import->esd->decoderConfig->streamType;
		*oti = import->esd->decoderConfig->objectTypeIndication;
		switch (*stype) {
		case GF_STREAM_SCENE:
			*mtype = GF_ISOM_MEDIA_SCENE;
			break;
		case GF_STREAM_OD:
			*mtype = GF_ISOM_MEDIA_OD;
			break;
		case GF_STREAM_INTERACT:
			*mtype = GF_ISOM_MEDIA_SCENE;
			break;
		case GF_STREAM_VISUAL:
			*mtype = GF_ISOM_MEDIA_VISUAL;
			break;
		case GF_STREAM_AUDIO:
			*mtype = GF_ISOM_MEDIA_AUDIO;
			break;
		default:
			*mtype = GF_ISOM_MEDIA_ESM;
			break;
		}
	}
	if (*mtype == 0) {
		*mtype = GF_ISOM_MEDIA_ESM;
		*oti = 0;
		*stype = 0;
	}

}

static void m2ts_create_track(GF_TSImport *tsimp, u32 mtype, u32 stype, u32 oti, u32 mpeg4_es_id, Bool is_in_iod)
{
	GF_MediaImporter *import = (GF_MediaImporter *)tsimp->import;
	if (mtype != GF_ISOM_MEDIA_ESM) {
		u32 di;
		Bool destroy_esd = GF_FALSE;
		if (import->esd) mpeg4_es_id = import->esd->ESID;
		else if (!mpeg4_es_id) mpeg4_es_id = import->trackID;

		tsimp->track = gf_isom_new_track(import->dest, mpeg4_es_id, mtype, 90000);
		if (!tsimp->track) {
			tsimp->track = gf_isom_new_track(import->dest, 0, mtype, 90000);
			if (!tsimp->track) {
				//error
			}
		}
		if (!import->esd) {
			import->esd = gf_odf_desc_esd_new(2);
			destroy_esd = GF_TRUE;
		}
		/*update stream type/oti*/
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->decoderConfig->streamType = stype;
		import->esd->decoderConfig->objectTypeIndication = oti;
		import->esd->slConfig->timestampResolution = 90000;

		gf_isom_set_track_enabled(import->dest, tsimp->track, 1);
		//we store annexB format until we rewrite the sample, so don't inspect NALUs
		gf_isom_set_nalu_extract_mode(import->dest, tsimp->track, GF_ISOM_NALU_EXTRACT_INSPECT);

		if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, tsimp->track);
		gf_isom_new_mpeg4_description(import->dest, tsimp->track, import->esd, NULL, NULL, &di);
		if (destroy_esd) {
			gf_odf_desc_del((GF_Descriptor *)import->esd);
			import->esd = NULL;
		}

		if (is_in_iod) gf_isom_add_track_to_root_od(import->dest, tsimp->track);

		import->final_trackID = gf_isom_get_track_id(import->dest, tsimp->track);
	}
}

/*rewrite last AVC sample currently stored in Annex-B format to ISO format (rewrite start code)*/
void m2ts_rewrite_nalu_sample(GF_MediaImporter *import, GF_TSImport *tsimp)
{
	GF_Err e;
	u32 sc_pos, start;
	GF_BitStream *bs;
	GF_ISOSample *samp;
	u32 count = gf_isom_get_sample_count(import->dest, tsimp->track);
	if (!count) return;

	samp = gf_isom_get_sample(import->dest, tsimp->track, count, NULL);
	sc_pos = 1;
	start = 0;
	bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_WRITE);
	while (1) {
		if (!samp->data[sc_pos] && !samp->data[sc_pos+1] && !samp->data[sc_pos+2] && (samp->data[sc_pos+3]==1)) {
			gf_bs_seek(bs, start);
			gf_bs_write_u32(bs, (u32) sc_pos-start-4);
			start = sc_pos;
		}
		sc_pos++;
		if (start+sc_pos>=samp->dataLength) break;
	}
	gf_bs_seek(bs, start);
	gf_bs_write_u32(bs, samp->dataLength-start-4);

	gf_bs_del(bs);

	e = gf_isom_update_sample(import->dest, tsimp->track, count, samp, GF_TRUE);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] Error rewriting AVC NALUs: %s\n", gf_error_to_string(e) ));
	}
	gf_isom_sample_del(&samp);
}

#ifndef GPAC_DISABLE_HEVC
static void hevc_cfg_add_nalu(GF_MediaImporter *import, GF_HEVCConfig *hevccfg, u8 nal_type, char *data, u32 data_len)
{
	u32 i, count;
	GF_AVCConfigSlot *sl;
	GF_HEVCParamArray *ar = NULL;

	count = gf_list_count(hevccfg->param_array);
	for (i=0; i<count; i++) {
		ar = (GF_HEVCParamArray*)gf_list_get(hevccfg->param_array, i);
		if (ar->type == nal_type) break;
		ar = NULL;
	}
	if (!ar) {
		GF_SAFEALLOC(ar, GF_HEVCParamArray);
		if (!ar) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[HEVCParse] Failed to allocate parameter set array\n"));
			return;
		}
		ar->array_completeness = 1;
		ar->type = nal_type;
		ar->nalus = gf_list_new();
		gf_list_add(hevccfg->param_array, ar);
	}

	if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
		ar->array_completeness = 0;
		return;
	}

	if (data) {
		GF_SAFEALLOC(sl, GF_AVCConfigSlot);
		if (!sl) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[HEVCParse] Failed to allocate parameter set container\n"));
			return;
		}
		sl->data = (char*)gf_malloc(sizeof(char)*data_len);
		if (!sl) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[HEVCParse] Failed to allocate parameter set data\n"));
			gf_free(sl);
			return;
		}
		sl->size = data_len;
		memcpy(sl->data, data, data_len);
		gf_list_add(ar->nalus, sl);
	} else {
		ar->array_completeness = 0;
	}
}
#endif //GPAC_DISABLE_HEVC

void on_m2ts_import_data(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_Err e;
	GF_ISOSample *samp;
	Bool is_au_start;
	u32 i, count, idx;
	GF_M2TS_Program *prog;
	GF_TSImport *tsimp = (GF_TSImport *) ts->user;
	GF_MediaImporter *import= (GF_MediaImporter *)tsimp->import;
	GF_M2TS_ES *es = NULL;
	GF_M2TS_PES *pes = NULL;

	switch (evt_type) {
	case GF_M2TS_EVT_PAT_FOUND:
		break;
//	case GF_M2TS_EVT_PAT_REPEAT:
//	case GF_M2TS_EVT_SDT_REPEAT:
	case GF_M2TS_EVT_PMT_REPEAT:
		/*abort upon first PMT repeat if not using 4on2. Otherwise we must parse the entire
		bitstream to locate ODs sent in OD updates in order to get their stream types...*/
		/*		if (!ts->has_4on2 && (import->flags & GF_IMPORT_PROBE_ONLY) && !import->track_id)
					import->flags |= GF_IMPORT_DO_ABORT;
		*/
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		gf_import_message(import, GF_OK, "[MPEG-2 TS] PMT Update found - cannot import any further");
		import->flags |= GF_IMPORT_DO_ABORT;
		break;
	case GF_M2TS_EVT_DURATION_ESTIMATED:
		if (import->flags & GF_IMPORT_PROBE_ONLY) {
			import->probe_duration = ((GF_M2TS_PES_PCK *) par)->PTS;
		}
		break;

	/*case GF_M2TS_EVT_SDT_FOUND:
		import->nb_progs = gf_list_count(ts->SDTs);
		for (i=0; i<import->nb_progs; i++) {
			GF_M2TS_SDT *sdt = (GF_M2TS_SDT *)gf_list_get(ts->SDTs, i);
			strcpy(import->pg_info[i].name, sdt->service);
			import->pg_info[i].number = sdt->service_id;
		}
		if (!ts->has_4on2 && import->flags & GF_IMPORT_PROBE_ONLY)
			//import->flags |= GF_IMPORT_DO_ABORT;
		break;*/
	case GF_M2TS_EVT_PMT_FOUND:
		prog = (GF_M2TS_Program*)par;

		if (import->flags & GF_IMPORT_PROBE_ONLY) {
			/*
				we scan all the streams declared in this PMT to fill the tk_info structures
				NOTE: in the T-DMB case, we also need to decode ObjectDescriptor Updates see "case GF_M2TS_EVT_SL_PCK"
			*/
			count = gf_list_count(prog->streams);
			for (i=0; i<count; i++) {
				es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
				if (es->pid == prog->pmt_pid) continue;
				if (es->flags & GF_M2TS_ES_IS_SECTION) {
					//ses = (GF_M2TS_SECTION_ES *)es;
				} else {
					pes = (GF_M2TS_PES *)es;
					gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);
				}
				idx = import->nb_tracks;
				import->tk_info[idx].track_num = es->pid;
				import->tk_info[idx].prog_num = prog->number;
				import->tk_info[idx].mpeg4_es_id = es->mpeg4_es_id;

				switch (es->stream_type) {
				case GF_M2TS_VIDEO_MPEG1:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_MPG1;
					import->tk_info[idx].type = GF_ISOM_MEDIA_VISUAL;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_video++;
					break;
				case GF_M2TS_VIDEO_MPEG2:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_MPG2;
					import->tk_info[idx].type = GF_ISOM_MEDIA_VISUAL;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_video++;
					break;
				case GF_M2TS_VIDEO_MPEG4:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_MP4V;
					import->tk_info[idx].type = GF_ISOM_MEDIA_VISUAL;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_video++;
					break;
				case GF_M2TS_VIDEO_H264:
				case GF_M2TS_VIDEO_SVC:
					import->tk_info[idx].media_type = (es->stream_type==GF_M2TS_VIDEO_SVC) ? GF_MEDIA_TYPE_SVC : GF_MEDIA_TYPE_H264;
					import->tk_info[idx].type = GF_ISOM_MEDIA_VISUAL;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_video++;
					break;
				case GF_M2TS_VIDEO_HEVC:
				case GF_M2TS_VIDEO_HEVC_TEMPORAL:
				case GF_M2TS_VIDEO_HEVC_MCTS:
				case GF_M2TS_VIDEO_SHVC:
				case GF_M2TS_VIDEO_SHVC_TEMPORAL:
				case GF_M2TS_VIDEO_MHVC:
				case GF_M2TS_VIDEO_MHVC_TEMPORAL:
					import->tk_info[idx].media_type = (es->stream_type==GF_M2TS_VIDEO_HEVC) ? GF_MEDIA_TYPE_HEVC : GF_MEDIA_TYPE_LHVC;
					import->tk_info[idx].type = GF_ISOM_MEDIA_VISUAL;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_video++;

					switch (es->stream_type) {
					case GF_M2TS_VIDEO_HEVC_MCTS:
						import->tk_info[idx].media_type = GF_MEDIA_TYPE_HEVC;
						break;
					case GF_M2TS_VIDEO_HEVC_TEMPORAL:
						import->tk_info[idx].media_type = GF_MEDIA_TYPE_HEVC;
					case GF_M2TS_VIDEO_SHVC_TEMPORAL:
					case GF_M2TS_VIDEO_MHVC_TEMPORAL:
						import->tk_info[idx].video_info.temporal_enhancement = GF_TRUE;
					}
					break;
				case GF_M2TS_AUDIO_MPEG1:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_MPG1;
					import->tk_info[idx].type = GF_ISOM_MEDIA_AUDIO;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_audio++;
					break;
				case GF_M2TS_AUDIO_MPEG2:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_MPG2;
					import->tk_info[idx].type = GF_ISOM_MEDIA_AUDIO;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_audio++;
					break;
				case GF_M2TS_AUDIO_AAC:
				case GF_M2TS_AUDIO_LATM_AAC:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_MP4A;
					import->tk_info[idx].type = GF_ISOM_MEDIA_AUDIO;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_audio++;
					break;
				case GF_M2TS_AUDIO_AC3:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_DAC3;
					import->tk_info[idx].type = GF_ISOM_MEDIA_AUDIO;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_audio++;
					break;
				case GF_M2TS_AUDIO_EC3:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_DEC3;
					import->tk_info[idx].type = GF_ISOM_MEDIA_AUDIO;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_audio++;
					break;
				case GF_M2TS_AUDIO_DTS:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_DTS;
					import->tk_info[idx].type = GF_ISOM_MEDIA_AUDIO;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					tsimp->nb_audio++;
					break;
				case GF_M2TS_SYSTEMS_MPEG4_PES:
				case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
					if (es->stream_type == GF_M2TS_SYSTEMS_MPEG4_PES) {
						import->tk_info[idx].media_type = GF_MEDIA_TYPE_M4SP;
					} else {
						import->tk_info[idx].media_type = GF_MEDIA_TYPE_M4SS;
					}
					if (prog->pmt_iod) {
						GF_ESD *esd = gf_m2ts_get_esd(es);
						m2ts_set_track_mpeg4_probe_info(es, esd, &import->tk_info[idx]);
						if (esd && esd->decoderConfig->streamType == GF_STREAM_OD) {
							es->flags |= GF_M2TS_ES_IS_MPEG4_OD;
						}
					} else {
						import->tk_info[idx].type = GF_ISOM_MEDIA_ESM;
					}
					import->nb_tracks++;
					break;
				case GF_M2TS_METADATA_ID3_HLS:
					import->tk_info[idx].media_type = GF_MEDIA_TYPE_ID3;
					import->tk_info[idx].type = GF_ISOM_MEDIA_META;
					import->tk_info[idx].lang = pes->lang;
					import->nb_tracks++;
					break;
				default:
					gf_import_message(import, GF_OK, "[MPEG-2 TS] Ignoring stream of type %d", es->stream_type);
				}
			}
		} else {
			/* We are not in PROBE mode, we are importing only one stream and don't care about the other streams */
			u32 mtype, stype, oti;
			Bool is_in_iod, found;

			/* Since the GF_M2TS_ES_IS_MPEG4_OD flag is stored at the ES level and ES are reset after probe,
			   we need to set it again as in probe mode */
			found = GF_FALSE;
			count = gf_list_count(prog->streams);
			for (i=0; i<count; i++) {
				GF_ESD *esd;
				es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
				if (es->pid == prog->pmt_pid) continue;
				if (es->pid == import->trackID) found = GF_TRUE;
				esd = gf_m2ts_get_esd(es);
				if (esd && esd->decoderConfig->streamType == GF_STREAM_OD) {
					es->flags |= GF_M2TS_ES_IS_MPEG4_OD;
				}
			}
			/*this PMT is not the one of our stream*/
			if (!found || !ts->ess[import->trackID]) return;

			/*make sure all the streams in this programe are in RAW pes framing mode, so that we get notified of the
			DTS/PTS*/
			for (i=0; i<count; i++) {
				es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
				if (!(es->flags & GF_M2TS_ES_IS_SECTION)) {
					gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_RAW);
				}
			}

			es = ts->ess[import->trackID]; /* import->track_id == pid */

			if (es->flags & GF_M2TS_ES_IS_SECTION) {
				//ses = (GF_M2TS_SECTION_ES *)es;
			} else {
				pes = (GF_M2TS_PES *)es;
				gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT_NAL);
			}

			mtype = stype = oti = 0;
			is_in_iod = GF_FALSE;

			switch (es->stream_type) {
			case GF_M2TS_VIDEO_MPEG1:
				mtype = GF_ISOM_MEDIA_VISUAL;
				stype = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_MPEG1;
				break;
			case GF_M2TS_VIDEO_MPEG2:
				mtype = GF_ISOM_MEDIA_VISUAL;
				stype = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_MPEG2_422;
				break;
			case GF_M2TS_VIDEO_MPEG4:
				mtype = GF_ISOM_MEDIA_VISUAL;
				stype = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_MPEG4_PART2;
				break;
			case GF_M2TS_VIDEO_H264:
				mtype = GF_ISOM_MEDIA_VISUAL;
				stype = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_AVC;
				tsimp->avccfg = gf_odf_avc_cfg_new();
				break;
			case GF_M2TS_VIDEO_HEVC:
			case GF_M2TS_VIDEO_HEVC_TEMPORAL:
			case GF_M2TS_VIDEO_HEVC_MCTS:
			case GF_M2TS_VIDEO_SHVC:
			case GF_M2TS_VIDEO_SHVC_TEMPORAL:
			case GF_M2TS_VIDEO_MHVC:
			case GF_M2TS_VIDEO_MHVC_TEMPORAL:
				mtype = GF_ISOM_MEDIA_VISUAL;
				stype = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_HEVC;
#ifndef GPAC_DISABLE_HEVC
				tsimp->hevccfg = gf_odf_hevc_cfg_new();
				if (es->stream_type != GF_M2TS_VIDEO_HEVC) tsimp->is_substream = GF_TRUE;
#endif //GPAC_DISABLE_HEVC
				break;
			case GF_M2TS_VIDEO_SVC:
				mtype = GF_ISOM_MEDIA_VISUAL;
				stype = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_SVC;
				tsimp->avccfg = gf_odf_avc_cfg_new();
				break;
			case GF_M2TS_AUDIO_MPEG1:
				mtype = GF_ISOM_MEDIA_AUDIO;
				stype = GF_STREAM_AUDIO;
				oti = GPAC_OTI_AUDIO_MPEG1;
				break;
			case GF_M2TS_AUDIO_MPEG2:
				mtype = GF_ISOM_MEDIA_AUDIO;
				stype = GF_STREAM_AUDIO;
				oti = GPAC_OTI_AUDIO_MPEG2_PART3;
				break;
			case GF_M2TS_AUDIO_LATM_AAC:
			case GF_M2TS_AUDIO_AAC:
				mtype = GF_ISOM_MEDIA_AUDIO;
				stype = GF_STREAM_AUDIO;
				oti = GPAC_OTI_AUDIO_AAC_MPEG4;
				break;
			case GF_M2TS_AUDIO_AC3:
				mtype = GF_ISOM_MEDIA_AUDIO;
				stype = GF_STREAM_AUDIO;
				oti = GPAC_OTI_AUDIO_AC3;
				break;
			case GF_M2TS_AUDIO_EC3:
				mtype = GF_ISOM_MEDIA_AUDIO;
				stype = GF_STREAM_AUDIO;
				oti = GPAC_OTI_AUDIO_EAC3;
				break;
			case GF_M2TS_SYSTEMS_MPEG4_PES:
			case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
				if (prog->pmt_iod && !import->esd) {
					import->esd = gf_m2ts_get_esd(es);
					m2ts_set_track_mpeg4_creation_info(import, &mtype, &stype, &oti);
					is_in_iod = GF_TRUE;
				}
				break;
			}
			m2ts_create_track(tsimp, mtype, stype, oti, es->mpeg4_es_id, is_in_iod);
		}
		break;
	case GF_M2TS_EVT_AAC_CFG:
		if (!(import->flags & GF_IMPORT_PROBE_ONLY) && !tsimp->stream_setup) {
			GF_ESD *esd = gf_isom_get_esd(import->dest, tsimp->track, 1);
			if (esd) {
				if (!esd->decoderConfig->decoderSpecificInfo) esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
				if (esd->decoderConfig->decoderSpecificInfo->data) gf_free(esd->decoderConfig->decoderSpecificInfo->data);
				esd->decoderConfig->decoderSpecificInfo->data = ((GF_M2TS_PES_PCK*)par)->data;
				esd->decoderConfig->decoderSpecificInfo->dataLength = ((GF_M2TS_PES_PCK*)par)->data_len;
				gf_isom_change_mpeg4_description(import->dest, tsimp->track, 1, esd);
				esd->decoderConfig->decoderSpecificInfo->data = NULL;
				gf_odf_desc_del((GF_Descriptor *)esd);
				tsimp->stream_setup = GF_TRUE;
				gf_isom_set_audio_info(import->dest, tsimp->track, 1, ((GF_M2TS_PES_PCK*)par)->stream->aud_sr, ((GF_M2TS_PES_PCK*)par)->stream->aud_nb_ch, 8, import->asemode);
			}
		}
		break;
	case GF_M2TS_EVT_PES_PCK:
	{
		GF_M2TS_PES_PCK *pck = (GF_M2TS_PES_PCK *)par;
		is_au_start = (pck->flags & GF_M2TS_PES_PCK_AU_START);

		if (import->flags & GF_IMPORT_PROBE_ONLY) {
			for (i=0; i<import->nb_tracks; i++) {
				if (import->tk_info[i].track_num == pck->stream->pid) {
					if (pck->stream->aud_sr && ! import->tk_info[i].audio_info.sample_rate) {
						import->tk_info[i].audio_info.sample_rate = pck->stream->aud_sr;
						import->tk_info[i].audio_info.nb_channels = pck->stream->aud_nb_ch;
						if ((pck->stream->stream_type==GF_M2TS_AUDIO_AAC) || (pck->stream->stream_type==GF_M2TS_AUDIO_LATM_AAC)) {
							sprintf(import->tk_info[i].szCodecProfile, "mp4a.40.%02x", (u8) pck->stream->aud_aac_obj_type);
						}
						import->tk_info[i].audio_info.sample_rate = pck->stream->aud_sr;
						import->tk_info[i].audio_info.nb_channels = pck->stream->aud_nb_ch;
						tsimp->nb_audio_configured++;
					}
					/*unpack AVC config*/
					else if (((pck->stream->stream_type==GF_M2TS_VIDEO_H264) || (pck->stream->stream_type==GF_M2TS_VIDEO_SVC)) && !pck->data[0] && !pck->data[1]) {
						u32 nal_type = pck->data[4] & 0x1F;
						if (nal_type == GF_AVC_NALU_SEQ_PARAM) {
							sprintf(import->tk_info[i].szCodecProfile, "avc1.%02x%02x%02x", (u8) pck->data[5], (u8) pck->data[6], (u8) pck->data[7]);
						}
					}
					else if (pck->stream->stream_type==GF_M2TS_VIDEO_HEVC) {
						u32 nal_type = (pck->data[4] & 0x7E) >> 1;
						if (nal_type == GF_HEVC_NALU_SEQ_PARAM) {
							//todo ..;
							sprintf(import->tk_info[i].szCodecProfile, "hvc1");
						}
					}
					else if ((pck->stream->stream_type==GF_M2TS_AUDIO_EC3) || (pck->stream->stream_type==GF_M2TS_AUDIO_AC3) || (pck->stream->stream_type==GF_M2TS_AUDIO_DTS)) {
						if (!import->tk_info[i].audio_info.sample_rate) {
							//todo ...
							import->tk_info[i].audio_info.sample_rate = 44100;
							import->tk_info[i].audio_info.nb_channels = 2;
							tsimp->nb_audio_configured++;
						}
					}

					if (pck->stream->vid_w && ! import->tk_info[i].video_info.width ) {
						import->tk_info[i].video_info.width = pck->stream->vid_w;
						import->tk_info[i].video_info.height = pck->stream->vid_h;
						tsimp->nb_video_configured++;
					}

					/*consider we are done if not using 4 on 2*/
					if (!ts->has_4on2
					        && (tsimp->nb_video_configured == tsimp->nb_video)
					        && (tsimp->nb_audio_configured == tsimp->nb_audio)
					        && import->probe_duration
					   ) {
						import->flags |= GF_IMPORT_DO_ABORT;
					}
					break;
				}
			}
			if (!ts->has_4on2 && (import->trackID==pck->stream->pid) && (pck->stream->vid_h || pck->stream->aud_sr) )
				import->flags |= GF_IMPORT_DO_ABORT;
			return;
		}

		/* Even if we don't import this stream we need to check the first dts of the program */
		if (!(pck->stream->flags & GF_M2TS_ES_FIRST_DTS) && is_au_start) {
			pck->stream->flags |= GF_M2TS_ES_FIRST_DTS;
			pck->stream->first_dts = (pck->PTS!=pck->DTS) ? pck->DTS : pck->PTS;
			if (!pck->stream->program->first_dts || pck->stream->program->first_dts > pck->stream->first_dts) {
				pck->stream->program->first_dts = 1 + pck->stream->first_dts;

				if (pck->stream->pid != import->trackID) {
					gf_m2ts_set_pes_framing((GF_M2TS_PES *)pck->stream, GF_M2TS_PES_FRAMING_SKIP);
				}
			}
		}
		if (pck->stream->pid != import->trackID) return;

		/*avc data for the current sample is stored in annex-B, as we don't know the size of each nal
		when called back (depending on PES packetization, the end of the nal could be in following pes)*/
		if (tsimp->avccfg && !pck->data[0] && !pck->data[1]) {
			GF_AVCConfigSlot *slc;
			s32 idx;
			Bool add_sps, is_subseq = GF_FALSE;
			u32 nal_type = pck->data[4] & 0x1F;

			switch (nal_type) {
			case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
				is_subseq = GF_TRUE;
			case GF_AVC_NALU_SEQ_PARAM:
				idx = gf_media_avc_read_sps(pck->data+4, pck->data_len-4, &tsimp->avc, is_subseq, NULL);

				add_sps = GF_FALSE;
				if (idx>=0) {
					if (is_subseq) {
						if ((tsimp->avc.sps[idx].state & AVC_SUBSPS_PARSED) && !(tsimp->avc.sps[idx].state & AVC_SUBSPS_DECLARED)) {
							tsimp->avc.sps[idx].state |= AVC_SUBSPS_DECLARED;
							add_sps = GF_TRUE;
						}
					} else {
						if ((tsimp->avc.sps[idx].state & AVC_SPS_PARSED) && !(tsimp->avc.sps[idx].state & AVC_SPS_DECLARED)) {
							tsimp->avc.sps[idx].state |= AVC_SPS_DECLARED;
							add_sps = GF_TRUE;
						}
					}
					if (add_sps) {
						/*always store nalu size on 4 bytes*/
						tsimp->avccfg->nal_unit_size = 4;
						tsimp->avccfg->configurationVersion = 1;
						tsimp->avccfg->profile_compatibility = tsimp->avc.sps[idx].prof_compat;
						tsimp->avccfg->AVCProfileIndication = tsimp->avc.sps[idx].profile_idc;
						tsimp->avccfg->AVCLevelIndication = tsimp->avc.sps[idx].level_idc;

						if (pck->stream->vid_w < tsimp->avc.sps[idx].width)
							pck->stream->vid_w = tsimp->avc.sps[idx].width;
						if (pck->stream->vid_h < tsimp->avc.sps[idx].height)
							pck->stream->vid_h = tsimp->avc.sps[idx].height;

						if (!(import->flags & GF_IMPORT_FORCE_XPS_INBAND)) {
							slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
							slc->size = pck->data_len-4;
							slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
							memcpy(slc->data, pck->data+4, sizeof(char)*slc->size);
							gf_list_add(tsimp->avccfg->sequenceParameterSets, slc);
						}
					}
				}
				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					break;
				}
				return;
			case GF_AVC_NALU_PIC_PARAM:
				idx = gf_media_avc_read_pps(pck->data+4, pck->data_len-4, &tsimp->avc);
				if ((idx>=0) && (tsimp->avc.pps[idx].status==1)) {
					tsimp->avc.pps[idx].status = 2;
					if (!(import->flags & GF_IMPORT_FORCE_XPS_INBAND)) {
						slc = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
						slc->size = pck->data_len-4;
						slc->data = (char*)gf_malloc(sizeof(char)*slc->size);
						memcpy(slc->data, pck->data+4, sizeof(char)*slc->size);
						gf_list_add(tsimp->avccfg->pictureParameterSets, slc);
					}
				}
				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					break;
				}
				/*else discard because of invalid PPS*/
				return;
			/*remove*/
			case GF_AVC_NALU_ACCESS_UNIT:
				tsimp->force_next_au_start = GF_TRUE;
				return;
			case GF_AVC_NALU_FILLER_DATA:
			case GF_AVC_NALU_END_OF_SEQ:
			case GF_AVC_NALU_END_OF_STREAM:
				return;
			case GF_AVC_NALU_SEI:
				break;

			}

			if (tsimp->force_next_au_start) {
				is_au_start = GF_TRUE;
				tsimp->force_next_au_start = GF_FALSE;
			}
		}

		/*avc data for the current sample is stored in annex-B, as we don't know the size of each nal
		when called back (depending on PES packetization, the end of the nal could be in following pes)*/
#ifndef GPAC_DISABLE_HEVC
		else if (tsimp->hevccfg && !pck->data[0] && !pck->data[1]) {
			s32 idx;
			Bool add_sps, is_subseq = GF_FALSE;
			u32 nal_type = (pck->data[4] & 0x7E) >> 1;

			switch (nal_type) {
			case GF_HEVC_NALU_SEQ_PARAM:
				idx = gf_media_hevc_read_sps(pck->data+4, pck->data_len-4, &tsimp->hevc);
				add_sps = GF_FALSE;
				if (idx>=0) {
					if (is_subseq) {
						if ((tsimp->hevc.sps[idx].state & AVC_SUBSPS_PARSED) && !(tsimp->hevc.sps[idx].state & AVC_SUBSPS_DECLARED)) {
							tsimp->hevc.sps[idx].state |= AVC_SUBSPS_DECLARED;
							add_sps = GF_TRUE;
						}
					} else {
						if ((tsimp->hevc.sps[idx].state & AVC_SPS_PARSED) && !(tsimp->hevc.sps[idx].state & AVC_SPS_DECLARED)) {
							tsimp->hevc.sps[idx].state |= AVC_SPS_DECLARED;
							add_sps = GF_TRUE;
						}
					}
					if (add_sps) {
						/*always store nalu size on 4 bytes*/
						tsimp->hevccfg->nal_unit_size = 4;
						tsimp->hevccfg->configurationVersion = 1;

						tsimp->hevccfg->configurationVersion = 1;
						tsimp->hevccfg->profile_space = tsimp->hevc.sps[idx].ptl.profile_space;
						tsimp->hevccfg->profile_idc = tsimp->hevc.sps[idx].ptl.profile_idc;
						tsimp->hevccfg->constraint_indicator_flags = 0;
						tsimp->hevccfg->level_idc = tsimp->hevc.sps[idx].ptl.level_idc;
						tsimp->hevccfg->general_profile_compatibility_flags = tsimp->hevc.sps[idx].ptl.profile_compatibility_flag;
						tsimp->hevccfg->chromaFormat = tsimp->hevc.sps[idx].chroma_format_idc;
						tsimp->hevccfg->luma_bit_depth = tsimp->hevc.sps[idx].bit_depth_luma;
						tsimp->hevccfg->chroma_bit_depth = tsimp->hevc.sps[idx].bit_depth_chroma;

						hevc_cfg_add_nalu(import, tsimp->hevccfg, nal_type, pck->data+4, pck->data_len-4);

						if (pck->stream->vid_w < tsimp->avc.sps[idx].width)
							pck->stream->vid_w = tsimp->avc.sps[idx].width;
						if (pck->stream->vid_h < tsimp->avc.sps[idx].height)
							pck->stream->vid_h = tsimp->avc.sps[idx].height;
					}
				}
				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					is_au_start = GF_TRUE;
					break;
				}
				return;
			case GF_HEVC_NALU_PIC_PARAM:
				idx = gf_media_hevc_read_pps(pck->data+4, pck->data_len-4, &tsimp->hevc);
				if ((idx>=0) && (tsimp->hevc.pps[idx].state==1)) {
					tsimp->hevc.pps[idx].state = 2;
					hevc_cfg_add_nalu(import, tsimp->hevccfg, nal_type, pck->data+4, pck->data_len-4);
				}
				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					is_au_start = GF_TRUE;
					break;
				}
				return;
			case GF_HEVC_NALU_VID_PARAM:
				idx = gf_media_hevc_read_vps(pck->data+4, pck->data_len-4, &tsimp->hevc);
				if ((idx>=0) && (tsimp->hevc.vps[idx].state==1)) {
					tsimp->hevc.vps[idx].state = 2;
					tsimp->hevccfg->avgFrameRate = tsimp->hevc.vps[idx].rates[0].avg_pic_rate;
					tsimp->hevccfg->constantFrameRate = tsimp->hevc.vps[idx].rates[0].constand_pic_rate_idc;
					tsimp->hevccfg->numTemporalLayers = tsimp->hevc.vps[idx].max_sub_layers;
					hevc_cfg_add_nalu(import, tsimp->hevccfg, nal_type, pck->data+4, pck->data_len-4);
				}
				if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
					is_au_start = GF_TRUE;
					break;
				}
				return;
			/*remove*/
			case GF_HEVC_NALU_ACCESS_UNIT:
				tsimp->force_next_au_start = GF_TRUE;
				return;
			case GF_HEVC_NALU_FILLER_DATA:
			case GF_HEVC_NALU_END_OF_SEQ:
			case GF_HEVC_NALU_END_OF_STREAM:
				return;
			case GF_HEVC_NALU_SEI_PREFIX:
				is_au_start = GF_TRUE;
				break;
			}

			if (tsimp->force_next_au_start) {
				is_au_start = GF_TRUE;
				tsimp->force_next_au_start = GF_FALSE;
			}
		}
#endif //GPAC_DISABLE_HEVC

		if (!is_au_start) {
			e = gf_isom_append_sample_data(import->dest, tsimp->track, (char*)pck->data, pck->data_len);
			if (e) {
				if (!gf_isom_get_sample_count(import->dest, tsimp->track)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] missed beginning of sample data\n"));
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] Error appending sample data\n"));
				}
			}
			if (pck->flags & GF_M2TS_PES_PCK_I_FRAME) tsimp->nb_i++;
			if (pck->flags & GF_M2TS_PES_PCK_P_FRAME) tsimp->nb_p++;
			if (pck->flags & GF_M2TS_PES_PCK_B_FRAME) tsimp->nb_b++;

			if (pck->flags & GF_M2TS_PES_PCK_RAP) {
				e = gf_isom_set_sample_rap(import->dest, tsimp->track);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] Error setting RAP flag\n"));
				}
			}
			return;
		}

		samp = gf_isom_sample_new();
		samp->DTS = pck->DTS;
		samp->CTS_Offset = (u32) (pck->PTS - samp->DTS);

		if (pck->stream->first_dts==samp->DTS) {
			switch (pck->stream->stream_type) {
			case GF_M2TS_VIDEO_MPEG1:
				gf_import_message(import, GF_OK, "MPEG-1 Video import (TS PID %d)", pck->stream->pid);
				break;
			case GF_M2TS_VIDEO_MPEG2:
				gf_import_message(import, GF_OK, "MPEG-2 Video import (TS PID %d)", pck->stream->pid);
				break;
			case GF_M2TS_VIDEO_MPEG4:
				gf_import_message(import, GF_OK, "MPEG-4 Video import (TS PID %d)", pck->stream->pid);
				break;
			case GF_M2TS_VIDEO_H264:
				gf_import_message(import, GF_OK, "MPEG-4 AVC/H264 Video import (TS PID %d)", pck->stream->pid);
				break;
			case GF_M2TS_VIDEO_HEVC:
				gf_import_message(import, GF_OK, "MPEG-H HEVC Video import (TS PID %d)", pck->stream->pid);
				break;
			case GF_M2TS_VIDEO_SVC:
				gf_import_message(import, GF_OK, "H264-SVC Video import (TS PID %d)", pck->stream->pid);
				break;
			case GF_M2TS_AUDIO_MPEG1:
				gf_import_message(import, GF_OK, "MPEG-1 Audio import - SampleRate %d Channels %d Language %s (TS PID %d)", pck->stream->aud_sr, pck->stream->aud_nb_ch, gf_4cc_to_str(pck->stream->lang), pck->stream->pid);
				break;
			case GF_M2TS_AUDIO_MPEG2:
				gf_import_message(import, GF_OK, "MPEG-2 Audio import - SampleRate %d Channels %d Language %s (TS PID %d)", pck->stream->aud_sr, pck->stream->aud_nb_ch, gf_4cc_to_str(pck->stream->lang), pck->stream->pid);
				break;
			case GF_M2TS_AUDIO_AAC:
				gf_import_message(import, GF_OK, "MPEG-4 AAC Audio import - SampleRate %d Channels %d Language %s (TS PID %d)", pck->stream->aud_sr, pck->stream->aud_nb_ch, gf_4cc_to_str(pck->stream->lang), pck->stream->pid);
				break;
			case GF_M2TS_AUDIO_AC3:
				gf_import_message(import, GF_OK, "Dolby AC3 Audio import - SampleRate %d Channels %d Language %s (TS PID %d)", pck->stream->aud_sr, pck->stream->aud_nb_ch, gf_4cc_to_str(pck->stream->lang), pck->stream->pid);
				break;
			case GF_M2TS_AUDIO_EC3:
				gf_import_message(import, GF_OK, "Dolby E-AC3 Audio import - SampleRate %d Channels %d Language %s (TS PID %d)", pck->stream->aud_sr, pck->stream->aud_nb_ch, gf_4cc_to_str(pck->stream->lang), pck->stream->pid);
				break;
			}
			if (pck->stream->lang)
				gf_isom_set_media_language(import->dest, tsimp->track, (char *) gf_4cc_to_str(pck->stream->lang)+1);
		}
		if (!tsimp->stream_setup) {
			if (pck->stream->aud_sr) {
				gf_isom_set_audio_info(import->dest, tsimp->track, 1, pck->stream->aud_sr, pck->stream->aud_nb_ch, 16, import->asemode);
				tsimp->stream_setup = GF_TRUE;
			}
			else if (pck->stream->vid_w) {
				u32 w = pck->stream->vid_w;
				if (pck->stream->vid_par) w = w * (pck->stream->vid_par>>16) / (pck->stream->vid_par&0xffff);
				gf_isom_set_visual_info(import->dest, tsimp->track, 1, pck->stream->vid_w, pck->stream->vid_h);
				gf_isom_set_track_layout_info(import->dest, tsimp->track, w<<16, pck->stream->vid_h<<16, 0, 0, 0);
				if (w != pck->stream->vid_w) {
					e = gf_isom_set_pixel_aspect_ratio(import->dest, tsimp->track, 1, pck->stream->vid_par>>16, pck->stream->vid_par&0xff, GF_FALSE);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] Error setting aspect ratio\n"));
					}
				}
				tsimp->stream_setup = GF_TRUE;
			}
		}

		if (samp->DTS < pck->stream->first_dts) {
			u32 sample_num = gf_isom_get_sample_count(import->dest, tsimp->track);
			u32 dur = gf_isom_get_sample_duration(import->dest, tsimp->track, sample_num);

			pck->stream->first_dts = samp->DTS - (tsimp->last_dts + 1 + dur);
			pck->stream->program->first_dts = pck->stream->first_dts;
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] negative time sample - PCR loop/discontinuity, adjusting\n"));
		}
		if (samp->DTS >= pck->stream->first_dts) {
			samp->DTS -= pck->stream->first_dts;
			samp->IsRAP = (pck->flags & GF_M2TS_PES_PCK_RAP) ? RAP : RAP_NO;
			samp->data = pck->data;
			samp->dataLength = pck->data_len;

			if ((pck->stream->flags & GF_M2TS_ES_FIRST_DTS) && (samp->DTS + 1 == tsimp->last_dts)) {
				e = gf_isom_append_sample_data(import->dest, tsimp->track, (char*)pck->data, pck->data_len);
			} else {

				if (tsimp->avccfg || tsimp->hevccfg) m2ts_rewrite_nalu_sample(import, tsimp);

				e = gf_isom_add_sample(import->dest, tsimp->track, 1, samp);
			}
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] PID %d: Error adding sample: %s\n", pck->stream->pid, gf_error_to_string(e)));
				//import->flags |= GF_IMPORT_DO_ABORT;
				import->last_error = e;
			}
			if (import->duration && (import->duration<=(samp->DTS+samp->CTS_Offset)/90)) {
				//import->flags |= GF_IMPORT_DO_ABORT;
			}

			if (pck->flags & GF_M2TS_PES_PCK_I_FRAME) tsimp->nb_i++;
			if (pck->flags & GF_M2TS_PES_PCK_P_FRAME) tsimp->nb_p++;
			if (pck->flags & GF_M2TS_PES_PCK_B_FRAME) tsimp->nb_b++;
			tsimp->last_dts = samp->DTS + 1;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] negative time sample - skipping\n"));
		}
		samp->data = NULL;
		gf_isom_sample_del(&samp);
	}
	break;
	case GF_M2TS_EVT_SL_PCK:
	{
		GF_M2TS_SL_PCK *sl_pck = (GF_M2TS_SL_PCK *)par;

		/* if there is no IOD for this program we cannot handle SL packets */
		if (!sl_pck->stream->program->pmt_iod) return;

		if (sl_pck->stream->flags & GF_M2TS_ES_IS_SECTION) {
			//ses = (GF_M2TS_SECTION_ES *)sl_pck->stream;
		} else {
			pes = (GF_M2TS_PES *)sl_pck->stream;
		}

		if (sl_pck->stream->flags & GF_M2TS_ES_IS_MPEG4_OD) {
			/* We need to handle OD streams even if this is not the stream we are importing */
			GF_ESD *esd = gf_m2ts_get_esd(sl_pck->stream);
			if (esd) {
				GF_SLHeader hdr;
				u32 hdr_len;
				GF_ODCodec *od_codec = gf_odf_codec_new();
				GF_ODCom *com;
				GF_ODUpdate* odU;
				u32 com_count, com_index, od_count, od_index;

				gf_sl_depacketize(esd->slConfig, &hdr, sl_pck->data, sl_pck->data_len, &hdr_len);
				gf_odf_codec_set_au(od_codec, sl_pck->data+hdr_len, sl_pck->data_len - hdr_len);
				gf_odf_codec_decode(od_codec);
				com_count = gf_list_count(od_codec->CommandList);
				for (com_index = 0; com_index < com_count; com_index++) {
					com = (GF_ODCom *)gf_list_get(od_codec->CommandList, com_index);
					switch (com->tag) {
					case GF_ODF_OD_UPDATE_TAG:
						odU = (GF_ODUpdate*)com;
						od_count = gf_list_count(odU->objectDescriptors);
						for (od_index=0; od_index<od_count; od_index++) {
							GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, od_index);
							gf_list_add(sl_pck->stream->program->additional_ods, od);

							/* We need to set the remaining unset track info for the streams declared in this OD */
							m2ts_set_tracks_mpeg4_probe_info(import, sl_pck->stream->program, od->ESDescriptors);
						}
						gf_list_reset(odU->objectDescriptors);
					}
				}
				gf_odf_codec_del(od_codec);
			}

		}

		if (import->flags & GF_IMPORT_PROBE_ONLY) {
			if (pes) {
				for (i=0; i<import->nb_tracks; i++) {
					if (import->tk_info[i].track_num == sl_pck->stream->pid) {
						if (pes->aud_sr) {
							import->tk_info[i].audio_info.sample_rate = pes->aud_sr;
							import->tk_info[i].audio_info.nb_channels = pes->aud_nb_ch;
						} else {
							import->tk_info[i].video_info.width = pes->vid_w;
							import->tk_info[i].video_info.height = pes->vid_h;
						}
						break;
					}
				}
//					if (pes->vid_h || pes->aud_sr) import->flags |= GF_IMPORT_DO_ABORT;
			}
			return;
		}

		if (sl_pck->stream->pid != import->trackID) return;

		/* we create a track for the stream to import only if it was not created */
		if (!gf_isom_get_track_by_id(import->dest, (import->esd?import->esd->ESID:import->trackID))) {
			u32 mtype, stype, oti;
			mtype = stype = oti = 0;
			import->esd = gf_m2ts_get_esd(sl_pck->stream);
			m2ts_set_track_mpeg4_creation_info(import, &mtype, &stype, &oti);
			m2ts_create_track(tsimp, mtype, stype, oti, sl_pck->stream->mpeg4_es_id, GF_FALSE);
		}

		if (import->esd) {
			GF_SLHeader hdr;
			u32 hdr_len;
			gf_sl_depacketize(import->esd->slConfig, &hdr, sl_pck->data, sl_pck->data_len, &hdr_len);

			if (!hdr.accessUnitStartFlag) {
				e = gf_isom_append_sample_data(import->dest, tsimp->track, sl_pck->data + hdr_len, sl_pck->data_len - hdr_len);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] Error appending sample data\n"));
				}
			} else {
				if (!(sl_pck->stream->flags & GF_M2TS_ES_FIRST_DTS)) {
					sl_pck->stream->flags |= GF_M2TS_ES_FIRST_DTS;

					if (!hdr.compositionTimeStampFlag) {
						hdr.compositionTimeStamp = sl_pck->stream->program->first_dts - 1;
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] PID %d First SL Access unit start flag set without any composition time stamp - defaulting to last CTS seen on program\n", sl_pck->stream->pid));
					}
					sl_pck->stream->first_dts = (hdr.decodingTimeStamp?hdr.decodingTimeStamp:hdr.compositionTimeStamp);
					if (!sl_pck->stream->program->first_dts || (sl_pck->stream->program->first_dts > sl_pck->stream->first_dts)) {
						sl_pck->stream->program->first_dts = sl_pck->stream->first_dts + 1;
					}
				} else {
					if (!hdr.compositionTimeStampFlag) {
						hdr.compositionTimeStamp = sl_pck->stream->first_dts + tsimp->last_dts - 1;
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] PID %d SL Access unit start flag set without any composition time stamp - defaulting to last CTS seen on stream + 1\n", sl_pck->stream->pid));
					}
				}

				samp = gf_isom_sample_new();
				samp->DTS = (hdr.decodingTimeStamp?hdr.decodingTimeStamp:hdr.compositionTimeStamp);
				samp->CTS_Offset = (u32) (hdr.compositionTimeStamp - samp->DTS);
				if (samp->DTS >= sl_pck->stream->first_dts) {
					samp->DTS -= sl_pck->stream->first_dts;
					samp->IsRAP = import->esd->slConfig->useRandomAccessPointFlag ? hdr.randomAccessPointFlag : RAP;

					/*fix for some DMB streams where TSs are not coded*/
					if ((tsimp->last_dts == 1 + samp->DTS) && gf_isom_get_sample_count(import->dest, tsimp->track))
						samp->DTS += gf_isom_get_media_timescale(import->dest, tsimp->track);

					samp->data = sl_pck->data + hdr_len;
					samp->dataLength = sl_pck->data_len - hdr_len;

					e = gf_isom_add_sample(import->dest, tsimp->track, 1, samp);
					/*if CTS was not specified, samples will simply be skipped*/
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] PID %d Error adding sample\n", sl_pck->stream->pid));
					}
					if (import->duration && (import->duration<=(samp->DTS+samp->CTS_Offset)/90)) {
						//import->flags |= GF_IMPORT_DO_ABORT;
					}
					tsimp->last_dts = samp->DTS + 1;

				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Import] negative time sample - skipping\n"));
					sl_pck->stream->first_dts = samp->DTS;
					if (!sl_pck->stream->program->first_dts || (sl_pck->stream->program->first_dts > sl_pck->stream->first_dts)) {
						sl_pck->stream->program->first_dts = sl_pck->stream->first_dts + 1;
					}
				}
				samp->data = NULL;
				gf_isom_sample_del(&samp);
			}
		}
	}
	break;
	}
}

extern void gf_m2ts_flush_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes);

/* Warning: we start importing only after finding the PMT */
GF_Err gf_import_mpeg_ts(GF_MediaImporter *import)
{
	GF_M2TS_Demuxer *ts;
	GF_M2TS_ES *es;
	char data[188];
	GF_TSImport tsimp;
	u64 fsize, done;
	u32 size, i;
	Bool do_import = GF_TRUE;
	FILE *mts;
	char progress[1000];

	if (import->trackID > GF_M2TS_MAX_STREAMS)
		return gf_import_message(import, GF_BAD_PARAM, "Invalid PID %d", import->trackID );

	mts = gf_fopen(import->in_name, "rb");
	if (!mts) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);

	gf_fseek(mts, 0, SEEK_END);
	fsize = gf_ftell(mts);
	gf_fseek(mts, 0, SEEK_SET);
	done = 0;

	memset(&tsimp, 0, sizeof(GF_TSImport));
	tsimp.avc.sps_active_idx = -1;
	tsimp.import = import;
	ts = gf_m2ts_demux_new();
	ts->on_event = on_m2ts_import_data;
	ts->user = &tsimp;
	ts->file_size = fsize;

	ts->dvb_h_demux = (import->flags & GF_IMPORT_MPE_DEMUX) ? GF_TRUE : GF_FALSE;

	if (import->flags & GF_IMPORT_PROBE_ONLY) do_import = GF_FALSE;

	sprintf(progress, "Importing MPEG-2 TS (PID %d)", import->trackID);
	if (do_import) gf_import_message(import, GF_OK, progress);

	while (!feof(mts)) {
		size = (u32) fread(data, sizeof(char), 188, mts);
		if (size<188)
			break;

		gf_m2ts_process_data(ts, data, size);
		ts->nb_pck++;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
		done += size;
		if (do_import) gf_set_progress(progress, (u32) (done/1024), (u32) (fsize/1024));
	}
	import->flags &= ~GF_IMPORT_DO_ABORT;

	if (import->last_error) {
		GF_Err e = import->last_error;
		import->last_error = GF_OK;
		if (tsimp.avccfg) gf_odf_avc_cfg_del(tsimp.avccfg);
		if (tsimp.hevccfg) gf_odf_hevc_cfg_del(tsimp.hevccfg);
		gf_m2ts_demux_del(ts);
		gf_fclose(mts);
		return e;
	}

	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		if (ts->ess[i]) {
			if (ts->ess[i]->flags & GF_M2TS_ES_IS_PES) {
				gf_m2ts_flush_pes(ts, (GF_M2TS_PES *) ts->ess[i]);
				ts->on_event(ts, GF_M2TS_EVT_EOS, (GF_M2TS_PES *) ts->ess[i]);
			}
		}
	}

	import->esd = NULL;
	if (do_import) gf_set_progress(progress, (u32) (fsize/1024), (u32) (fsize/1024));

	if (! (import->flags & GF_IMPORT_MPE_DEMUX)) {
		gf_m2ts_print_info(ts);
	}

	if (!(import->flags & GF_IMPORT_PROBE_ONLY)) {
		es = (GF_M2TS_ES *)ts->ess[import->trackID];
		if (!es) {
			gf_m2ts_demux_del(ts);
			gf_fclose(mts);
			return gf_import_message(import, GF_BAD_PARAM, "Unknown PID %d", import->trackID);
		}

		if (tsimp.avccfg) {
			u32 w = ((GF_M2TS_PES*)es)->vid_w;
			u32 h = ((GF_M2TS_PES*)es)->vid_h;
			gf_isom_avc_config_update(import->dest, tsimp.track, 1, tsimp.avccfg);

			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				gf_isom_avc_set_inband_config(import->dest, tsimp.track, 1);
			}

			gf_isom_set_visual_info(import->dest, tsimp.track, 1, w, h);
			gf_isom_set_track_layout_info(import->dest, tsimp.track, w<<16, h<<16, 0, 0, 0);


			m2ts_rewrite_nalu_sample(import, &tsimp);

			gf_odf_avc_cfg_del(tsimp.avccfg);
		}

		if (tsimp.hevccfg) {
			GF_M2TS_PES *pes = (GF_M2TS_PES*) es;
 			u32 w = pes->vid_w;
			u32 h = pes->vid_h;
			hevc_set_parall_type(tsimp.hevccfg);
			gf_isom_hevc_config_update(import->dest, tsimp.track, 1, tsimp.hevccfg);

			if (tsimp.is_substream) {
				u32 tk = gf_isom_get_track_by_id(import->dest, pes->depends_on_pid);
				if (tk) {
					GF_HEVCConfig *hcfg = gf_isom_hevc_config_get(import->dest, tk, 1);
					gf_isom_set_track_reference(import->dest, tsimp.track, GF_ISOM_REF_BASE, pes->depends_on_pid);
					gf_isom_get_visual_info(import->dest, tk, 1, &w, &h);
					if (hcfg && tsimp.hevccfg) {
						GF_List *ar = tsimp.hevccfg->param_array;
						memcpy(tsimp.hevccfg, hcfg, sizeof(GF_HEVCConfig));
						tsimp.hevccfg->param_array = ar;
						gf_isom_hevc_config_update(import->dest, tsimp.track, 1, tsimp.hevccfg);
					}
					if (hcfg) gf_odf_hevc_cfg_del(hcfg);

				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("Importing HEVC substream but base track not found\n"));
				}
				if ((es->stream_type==GF_M2TS_VIDEO_HEVC_TEMPORAL) || (es->stream_type==GF_M2TS_VIDEO_HEVC_MCTS)) {
					//NULL config: we keep hvcC but change to hvc2 the sample entry
					gf_isom_lhvc_config_update(import->dest, tsimp.track, 1, NULL, GF_ISOM_LEHVC_WITH_BASE);
				} else {
					gf_isom_lhvc_config_update(import->dest, tsimp.track, 1, tsimp.hevccfg, GF_ISOM_LEHVC_ONLY);
				}
			}
			if (import->flags & GF_IMPORT_FORCE_XPS_INBAND) {
				gf_isom_hevc_set_inband_config(import->dest, tsimp.track, 1);
			}

			gf_isom_set_visual_info(import->dest, tsimp.track, 1, w, h);
			gf_isom_set_track_layout_info(import->dest, tsimp.track, w<<16, h<<16, 0, 0, 0);

			m2ts_rewrite_nalu_sample(import, &tsimp);

			gf_odf_hevc_cfg_del(tsimp.hevccfg);
		}


		if (tsimp.track) {
			gf_media_update_bitrate(import->dest, tsimp.track);
			/* creation of the edit lists */
			if ((es->first_dts != es->program->first_dts) && gf_isom_get_sample_count(import->dest, tsimp.track) ) {
				u32 media_ts, moov_ts, offset;
				u64 dur;
				Double pdur, poffset;
				media_ts = gf_isom_get_media_timescale(import->dest, tsimp.track);
				moov_ts = gf_isom_get_timescale(import->dest);
				assert(es->program->first_dts - 1 <= es->first_dts);
				poffset = (es->first_dts - (es->program->first_dts - 1) ) * 1.0 * moov_ts / media_ts;
				offset = (u32)poffset;
				pdur = gf_isom_get_media_duration(import->dest, tsimp.track) * 1.0 * moov_ts / media_ts;
				dur = (u64)pdur;
				if (poffset != offset || pdur != dur) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("Movie timescale (%u) not precise enough to store edit (media timescale: %u)\n", moov_ts, media_ts));
				}
				gf_isom_set_edit_segment(import->dest, tsimp.track, 0, offset, 0, GF_ISOM_EDIT_EMPTY);
				gf_isom_set_edit_segment(import->dest, tsimp.track, offset, dur, 0, GF_ISOM_EDIT_NORMAL);
				gf_import_message(import, GF_OK, "Timeline offset: %u ms", (offset * 1000) / moov_ts);
			}

			if (tsimp.nb_p) {
				gf_import_message(import, GF_OK, "Import results: %d VOPs (%d Is - %d Ps - %d Bs)", gf_isom_get_sample_count(import->dest, tsimp.track), tsimp.nb_i, tsimp.nb_p, tsimp.nb_b);
			}

			if (es->program->pmt_iod)
				gf_isom_set_brand_info(import->dest, GF_ISOM_BRAND_MP42, 1);
		}
	}

	gf_m2ts_demux_del(ts);
	gf_fclose(mts);
	return GF_OK;
}

#endif /*GPAC_DISABLE_MPEG2TS*/

GF_Err gf_import_vobsub(GF_MediaImporter *import)
{
#ifndef GPAC_DISABLE_VOBSUB
	static const u8 null_subpic[] = { 0x00, 0x09, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0xFF };
	char		  filename[GF_MAX_PATH];
	FILE		 *file = NULL;
	int		  version;
	vobsub_file	  *vobsub = NULL;
	u32		  c, trackID, track, di;
	Bool		  destroy_esd = GF_FALSE;
	GF_Err		  err = GF_OK;
	GF_ISOSample	 *samp = NULL;
	GF_List		 *subpic;
	u64 last_dts = 0;
	u32 total, last_samp_dur = 0;
	unsigned char buf[0x800];

	strcpy(filename, import->in_name);
	vobsub_trim_ext(filename);
	strcat(filename, ".idx");

	file = gf_fopen(filename, "r");
	if (!file) {
		err = gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", filename);
		goto error;
	}

	GF_SAFEALLOC(vobsub, vobsub_file);
	if (!vobsub) {
		err = gf_import_message(import, GF_OUT_OF_MEM, "Memory allocation failed");
		goto error;
	}

	err = vobsub_read_idx(file, vobsub, &version);
	gf_fclose(file);

	if (err != GF_OK) {
		err = gf_import_message(import, err, "Reading VobSub file %s failed", filename);
		goto error;
	} else if (version < 6) {
		err = gf_import_message(import, err, "Unsupported VobSub version", filename);
		goto error;
	}

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 0;
		for (c = 0; c < 32; c++) {
			if (vobsub->langs[c].id != 0) {
				import->tk_info[import->nb_tracks].track_num = c + 1;
				import->tk_info[import->nb_tracks].type		 = GF_ISOM_MEDIA_SUBPIC;
				import->tk_info[import->nb_tracks].flags	 = 0;
				import->nb_tracks++;
			}
		}
		vobsub_free(vobsub);
		return GF_OK;
	}

	strcpy(filename, import->in_name);
	vobsub_trim_ext(filename);
	strcat(filename, ".sub");

	file = gf_fopen(filename, "rb");
	if (!file) {
		err = gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", filename);
		goto error;
	}

	trackID = import->trackID;
	if (!trackID) {
		trackID = 0-1U;
		if (vobsub->num_langs != 1) {
			err = gf_import_message(import, GF_BAD_PARAM, "Several tracks in VobSub - please indicate track to import");
			goto error;
		}
		for (c = 0; c < 32; c++) {
			if (vobsub->langs[c].id != 0) {
				trackID = c;
				break;
			}
		}
		if (trackID == 0-1U) {
			err = gf_import_message(import, GF_URL_ERROR, "Cannot find track ID %d in file", trackID);
			goto error;
		}
	}
	trackID--;

	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(2);
		destroy_esd = GF_TRUE;
	}
	if (!import->esd->decoderConfig) {
		import->esd->decoderConfig = (GF_DecoderConfig*)gf_odf_desc_new(GF_ODF_DCD_TAG);
	}
	if (!import->esd->slConfig) {
		import->esd->slConfig = (GF_SLConfig*)gf_odf_desc_new(GF_ODF_SLC_TAG);
	}
	if (!import->esd->decoderConfig->decoderSpecificInfo) {
		import->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor*)gf_odf_desc_new(GF_ODF_DSI_TAG);
	}

	import->esd->decoderConfig->streamType		 = GF_STREAM_ND_SUBPIC;
	import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_SUBPIC;

	import->esd->decoderConfig->decoderSpecificInfo->dataLength = sizeof(vobsub->palette);
	import->esd->decoderConfig->decoderSpecificInfo->data		= (char*)&vobsub->palette[0][0];

	gf_import_message(import, GF_OK, "VobSub import - subpicture stream '%s'", vobsub->langs[trackID].name);

	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_SUBPIC, 90000);
	if (!track) {
		err = gf_isom_last_error(import->dest);
		err = gf_import_message(import, err, "Could not create new track");
		goto error;
	}

	gf_isom_set_track_enabled(import->dest, track, 1);

	if (!import->esd->ESID) {
		import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	}
	import->final_trackID = import->esd->ESID;

	gf_isom_new_mpeg4_description(import->dest, track, import->esd, NULL, NULL, &di);
	gf_isom_set_track_layout_info(import->dest, track, vobsub->width << 16, vobsub->height << 16, 0, 0, 0);
	gf_isom_set_media_language(import->dest, track, vobsub->langs[trackID].name);

	samp = gf_isom_sample_new();
	samp->IsRAP = SAP_TYPE_1;
	samp->dataLength = sizeof(null_subpic);
	samp->data	= (char*)null_subpic;

	subpic = vobsub->langs[trackID].subpos;
	total  = gf_list_count(subpic);

	last_dts = 0;
	for (c = 0; c < total; c++)
	{
		u32		i, left, size, psize, dsize, hsize, duration;
		char *packet;
		vobsub_pos *pos = (vobsub_pos*)gf_list_get(subpic, c);

		if (import->duration && pos->start > import->duration) {
			break;
		}

		gf_fseek(file, pos->filepos, SEEK_SET);
		if (gf_ftell(file) != pos->filepos) {
			err = gf_import_message(import, GF_IO_ERR, "Could not seek in file");
			goto error;
		}

		if (!fread(buf, sizeof(buf), 1, file)) {
			err = gf_import_message(import, GF_IO_ERR, "Could not read from file");
			goto error;
		}

		if (*(u32*)&buf[0x00] != 0xba010000		   ||
		        *(u32*)&buf[0x0e] != 0xbd010000		   ||
		        !(buf[0x15] & 0x80)				   ||
		        (buf[0x17] & 0xf0) != 0x20			   ||
		        (buf[buf[0x16] + 0x17] & 0xe0) != 0x20)
		{
			gf_import_message(import, GF_CORRUPTED_DATA, "[VobSub] Corrupted data found in file %s (1)", filename);
			continue;
		}

		psize = (buf[buf[0x16] + 0x18] << 8) + buf[buf[0x16] + 0x19];
		dsize = (buf[buf[0x16] + 0x1a] << 8) + buf[buf[0x16] + 0x1b];
		packet = (char *) gf_malloc(sizeof(char)*psize);
		if (!packet) {
			err = gf_import_message(import, GF_OUT_OF_MEM, "Memory allocation failed");
			goto error;
		}

		for (i = 0, left = psize; i < psize; i += size, left -= size) {
			hsize = 0x18 + buf[0x16];
			size  = MIN(left, 0x800 - hsize);
			memcpy(packet + i, buf + hsize, size);

			if (size != left) {
				while (fread(buf, 1, sizeof(buf), file)) {
					if (buf[buf[0x16] + 0x17] == (trackID | 0x20)) {
						break;
					}
				}
			}
		}

		if (i != psize || left > 0) {
			gf_import_message(import, GF_CORRUPTED_DATA, "[VobSub] Corrupted data found in file %s (2)", filename);
			continue;
		}

		if (vobsub_get_subpic_duration(packet, psize, dsize, &duration) != GF_OK) {
			gf_import_message(import, GF_CORRUPTED_DATA, "[VobSub] Corrupted data found in file %s (3)", filename);
			continue;
		}

		last_samp_dur = duration;

		/*first sample has non-0 DTS, add an empty one*/
		if (!c && (pos->start != 0)) {
			err = gf_isom_add_sample(import->dest, track, di, samp);
			if (err) goto error;
		}

		samp->data	 = packet;
		samp->dataLength = psize;
		samp->DTS	 = pos->start * 90;

		if (last_dts && (last_dts >= samp->DTS)) {
			err = gf_import_message(import, GF_CORRUPTED_DATA, "Out of order timestamps in vobsub file");
			goto error;
		}

		err = gf_isom_add_sample(import->dest, track, di, samp);
		if (err) goto error;
		gf_free(packet);

		gf_set_progress("Importing VobSub", c, total);
		last_dts = samp->DTS;

		if (import->flags & GF_IMPORT_DO_ABORT) {
			break;
		}
	}

	gf_isom_set_last_sample_duration(import->dest, track, last_samp_dur);

	gf_media_update_bitrate(import->dest, track);
	gf_set_progress("Importing VobSub", total, total);

	err = GF_OK;

error:
	if (import->esd && destroy_esd) {
		import->esd->decoderConfig->decoderSpecificInfo->data = NULL;
		gf_odf_desc_del((GF_Descriptor *)import->esd);
		import->esd = NULL;
	}
	if (samp) {
		samp->data = NULL;
		gf_isom_sample_del(&samp);
	}
	if (vobsub) {
		vobsub_free(vobsub);
	}
	if (file) {
		gf_fclose(file);
	}

	return err;
#else
	return GF_NOT_SUPPORTED;
#endif
}

#ifndef GPAC_DISABLE_AV_PARSERS

GF_Err gf_import_ac3(GF_MediaImporter *import, Bool is_EAC3)
{
	GF_AC3Header hdr;
	GF_AC3Config cfg;
	Bool destroy_esd;
	GF_Err e;
	u16 sr;
	u32 nb_chan;
	FILE *in;
	GF_BitStream *bs;
	u32 max_size, track, di;
	u64 tot_size, done, duration;
	GF_ISOSample *samp;
	Bool (*ac3_parser_bs)(GF_BitStream*, GF_AC3Header*, Bool) = gf_ac3_parser_bs;

	in = gf_fopen(import->in_name, "rb");
	if (!in) return gf_import_message(import, GF_URL_ERROR, "Opening file %s failed", import->in_name);
	bs = gf_bs_from_file(in, GF_BITSTREAM_READ);

	memset(&hdr, 0, sizeof(GF_AC3Header));
	memset(&cfg, 0, sizeof(GF_AC3Config));
	if (is_EAC3 || !gf_ac3_parser_bs(bs, &hdr, GF_TRUE)) {
		if (!gf_eac3_parser_bs(bs, &hdr, GF_TRUE)) {
			gf_bs_del(bs);
			gf_fclose(in);
			return gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Audio is neither AC3 or E-AC3 audio");
		}
		is_EAC3 = GF_TRUE;
		ac3_parser_bs = gf_eac3_parser_bs;
	}
	sr = hdr.sample_rate;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		gf_bs_del(bs);
		gf_fclose(in);
		import->tk_info[0].track_num = 1;
		import->tk_info[0].type = GF_ISOM_MEDIA_AUDIO;
		import->tk_info[0].media_type = GF_MEDIA_TYPE_AC3;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF;
		import->tk_info[0].audio_info.sample_rate = sr;
		import->tk_info[0].audio_info.nb_channels = hdr.channels;
		import->nb_tracks = 1;
		return GF_OK;
	}

	destroy_esd = GF_FALSE;
	if (!import->esd) {
		import->esd = gf_odf_desc_esd_new(2);
		destroy_esd = GF_TRUE;
	}
	if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	/*update stream type/oti*/
	import->esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	import->esd->decoderConfig->objectTypeIndication = is_EAC3 ? GPAC_OTI_AUDIO_EAC3 : GPAC_OTI_AUDIO_AC3;
	import->esd->decoderConfig->bufferSizeDB = 20;
	import->esd->slConfig->timestampResolution = sr;

	samp = NULL;
	nb_chan = hdr.channels;
	gf_import_message(import, GF_OK, "%sAC3 import - sample rate %d - %d%s channel%s", is_EAC3 ? "Enhanced " : "", sr, hdr.lfon ? (nb_chan-1) : nb_chan, hdr.lfon?".1":"", (nb_chan>1) ? "s" : "");

	track = gf_isom_new_track(import->dest, import->esd->ESID, GF_ISOM_MEDIA_AUDIO, sr);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
	import->final_trackID = import->esd->ESID;
	if (import->esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) import->esd->decoderConfig->decoderSpecificInfo);
	import->esd->decoderConfig->decoderSpecificInfo = NULL;

	cfg.is_ec3 = is_EAC3;
	cfg.nb_streams = 1;
	cfg.brcode = hdr.brcode;
	cfg.streams[0].acmod = hdr.acmod;
	cfg.streams[0].bsid = hdr.bsid;
	cfg.streams[0].bsmod = hdr.bsmod;
	cfg.streams[0].fscod = hdr.fscod;
	cfg.streams[0].lfon = hdr.lfon;

	gf_isom_ac3_config_new(import->dest, track, &cfg, (import->flags & GF_IMPORT_USE_DATAREF) ? import->in_name : NULL, NULL, &di);
	gf_isom_set_audio_info(import->dest, track, di, sr, nb_chan, 16, import->asemode);

	gf_bs_seek(bs, 0);
	tot_size = gf_bs_get_size(bs);

	e = GF_OK;
	samp = gf_isom_sample_new();
	samp->IsRAP = RAP;

	duration = import->duration;
	duration *= sr;
	duration /= 1000;

	max_size = 0;
	done = 0;
	while (ac3_parser_bs(bs, &hdr, GF_FALSE)) {
		if (is_EAC3)
			samp->dataLength = 2*(1+hdr.framesize);
		else
			samp->dataLength = hdr.framesize;

		if (import->flags & GF_IMPORT_USE_DATAREF) {
			e = gf_isom_add_sample_reference(import->dest, track, di, samp, gf_bs_get_position(bs) );
			gf_bs_skip_bytes(bs, samp->dataLength);
		} else {
			if (samp->dataLength > max_size) {
				samp->data = (char*)gf_realloc(samp->data, sizeof(char) * samp->dataLength);
				max_size = samp->dataLength;
			}
			if (!gf_bs_read_data(bs, samp->data, samp->dataLength)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[AC3 import] Truncated file - want to read %d bytes but remain only %d bytes\n", samp->dataLength, gf_bs_get_size(bs) - gf_bs_get_position(bs)));
				break;
			}
			e = gf_isom_add_sample(import->dest, track, di, samp);
		}
		if (e)
			goto exit;

		gf_set_progress("Importing AC3", done, tot_size);

		samp->DTS += 1536;
		done = gf_bs_get_position(bs);
		if (duration && (samp->DTS > duration))
			break;
		if (import->flags & GF_IMPORT_DO_ABORT)
			break;
	}
	gf_media_update_bitrate(import->dest, track);
	gf_set_progress("Importing AC3", tot_size, tot_size);

exit:
	if (import->esd && destroy_esd) {
		gf_odf_desc_del((GF_Descriptor *) import->esd);
		import->esd = NULL;
	}
	if (samp) gf_isom_sample_del(&samp);
	gf_bs_del(bs);
	gf_fclose(in);
	return e;
}
#endif

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
			import->tk_info[0].media_type = GF_MEDIA_TYPE_CHAP;
			import->tk_info[0].type = GF_ISOM_MEDIA_TEXT;
			return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	}

	e = gf_isom_remove_chapter(import->dest, 0, 0);
	if (e) goto err_exit;

	if (!import->video_fps) {
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
			import->video_fps = ts;
			import->video_fps /= inc;
			gf_isom_sample_del(&samp);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[Chapter import] Guessed video frame rate %g (%u:%u)\n", import->video_fps, ts, inc));
			break;
		}
		if (!import->video_fps)
			import->video_fps = 25;
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
			ts = (u64) (((s64) ts ) / import->video_fps);
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
					ts = (s64) (((import->video_fps*((s64)ts) + fr) * 1000 ) / import->video_fps);
				} else if (sscanf(szTS, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u:%u", &h, &m, &s, &ms) == 4) {
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
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(str, "%u:%u:%u:%u", &h, &m, &s, &ms) == 4) {
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
GF_Err gf_media_import_chapters(GF_ISOFile *file, char *chap_file, Double import_fps, Bool use_qt)
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

GF_EXPORT
GF_Err gf_media_import(GF_MediaImporter *importer)
{
#ifndef GPAC_DISABLE_TTXT
	GF_Err gf_import_timed_text(GF_MediaImporter *import);
#endif
	GF_Err e;
	char *ext, *xml_type;
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


	/*always try with MP4 - this allows using .m4v extension for both raw CMP and iPod's files*/
	if (gf_isom_probe_file(importer->in_name)) {
		importer->orig = gf_isom_open(importer->in_name, GF_ISOM_OPEN_READ, NULL);
		if (importer->orig) {
			e = gf_import_isomedia(importer);
			gf_isom_delete(importer->orig);
			importer->orig = NULL;
			return e;
		}
	}

#ifndef GPAC_DISABLE_AVILIB
	/*AVI audio/video*/
	if (!strnicmp(ext, ".avi", 4) || !stricmp(fmt, "AVI") ) {
		e = gf_import_avi_video(importer);
		if (e) return e;
		return gf_import_avi_audio(importer);
	}
#endif

#ifndef GPAC_DISABLE_OGG
	/*OGG audio/video*/
	if (!strnicmp(ext, ".ogg", 4) || !stricmp(fmt, "OGG")) {
		e = gf_import_ogg_video(importer);
		if (e) return e;
		return gf_import_ogg_audio(importer);
	}
	/*Opus audio*/
	if (!strnicmp(ext, ".opus", 5)) {
		if (importer->flags & GF_IMPORT_PROBE_ONLY)
			return gf_import_ogg_video(importer);
		return gf_import_ogg_audio(importer);
	}
#endif

#ifndef GPAC_DISABLE_MPEG2PS
	/*MPEG PS*/
	if (!strnicmp(ext, ".mpg", 4) || !strnicmp(ext, ".mpeg", 5)
	        || !strnicmp(ext, ".vob", 4) || !strnicmp(ext, ".vcd", 4) || !strnicmp(ext, ".svcd", 5)
	        || !stricmp(fmt, "MPEG1") || !stricmp(fmt, "MPEG-PS")  || !stricmp(fmt, "MPEG2-PS")
	   ) {
		e = gf_import_mpeg_ps_video(importer);
		if (e) return e;
		return gf_import_mpeg_ps_audio(importer);
	}
#endif

#ifndef GPAC_DISABLE_MPEG2TS
	/*MPEG-2 TS*/
	if (!strnicmp(ext, ".ts", 3) || !strnicmp(ext, ".m2t", 4) || !strnicmp(ext, ".trp", 4) || !strnicmp(ext, ".mts", 4)
	        || !stricmp(fmt, "MPEGTS") || !stricmp(fmt, "MPEG-TS")
	        || !stricmp(fmt, "MPGTS") || !stricmp(fmt, "MPG-TS")
	        || !stricmp(fmt, "MPEG2TS")  || !stricmp(fmt, "MPEG2-TS")
	        || !stricmp(fmt, "MPG2TS")  || !stricmp(fmt, "MPG2-TS")
	   ) {
		return gf_import_mpeg_ts(importer);
	}
#endif

#ifndef GPAC_DISABLE_AV_PARSERS
	/*MPEG1/2 Audio*/
	if (!strnicmp(ext, ".mp2", 4) || !strnicmp(ext, ".mp3", 4) || !strnicmp(ext, ".m1a", 4) || !strnicmp(ext, ".m2a", 4) || !stricmp(fmt, "MP3") || !stricmp(fmt, "MPEG-AUDIO") )
		return gf_import_mp3(importer);
	/*MPEG-2/4 AAC*/
	if (!strnicmp(ext, ".aac", 4) || !strnicmp(ext, ".xhe", 4) || !stricmp(fmt, "AAC") || !stricmp(fmt, "MPEG4-AUDIO") )
		return gf_import_aac_adts(importer);
	/*MPEG-4 video*/
	if (!strnicmp(ext, ".cmp", 4) || !strnicmp(ext, ".m4v", 4) || !stricmp(fmt, "CMP") || !stricmp(fmt, "MPEG4-Video") )
		return gf_import_cmp(importer, GF_FALSE);
	/*MPEG-1/2 video*/
	if (!strnicmp(ext, ".m2v", 4) || !strnicmp(ext, ".m1v", 4) || !stricmp(fmt, "MPEG2-Video") || !stricmp(fmt, "MPEG1-Video") )
		return gf_import_cmp(importer, 2);
	/*H2632 video*/
	if (!strnicmp(ext, ".263", 4) || !strnicmp(ext, ".h263", 5) || !stricmp(fmt, "H263") )
		return gf_import_h263(importer);
	/*H264/AVC video*/
	if (!strnicmp(ext, ".h264", 5) || !strnicmp(ext, ".264", 4) || !strnicmp(ext, ".x264", 5)
	        || !strnicmp(ext, ".h26L", 5) || !strnicmp(ext, ".26l", 4) || !strnicmp(ext, ".avc", 4)
	        || !stricmp(fmt, "AVC") || !stricmp(fmt, "H264") )
		return gf_import_avc_h264(importer);
	/*HEVC video*/
	if (!strnicmp(ext, ".hevc", 5) || !strnicmp(ext, ".hvc", 4) || !strnicmp(ext, ".265", 4) || !strnicmp(ext, ".h265", 5)
		|| !strnicmp(ext, ".shvc", 5) || !strnicmp(ext, ".lhvc", 5) || !strnicmp(ext, ".mhvc", 5)
	        || !stricmp(fmt, "HEVC") || !stricmp(fmt, "SHVC") || !stricmp(fmt, "MHVC") || !stricmp(fmt, "LHVC") || !stricmp(fmt, "H265") )
		return gf_import_hevc(importer);
	/*IVF container (may contain VP9, AV1, ...)*/
	if (!strnicmp(ext, ".ivf", 4))
		return gf_import_ivf(importer);
	/*AOM AV1 video*/
	if (!strnicmp(ext, ".av1", 4) || !strnicmp(ext, ".obu", 4))
		return gf_import_aom_av1(importer);
	/*AC3 and E-AC3*/
	if (!strnicmp(ext, ".ac3", 4) || !stricmp(fmt, "AC3") )
		return gf_import_ac3(importer, GF_FALSE);
	if (!strnicmp(ext, ".ec3", 4) || !stricmp(fmt, "EC3") || !stricmp(fmt, "E-AC3") || !stricmp(fmt, "EAC3") )
		return gf_import_ac3(importer, GF_TRUE);
#endif

	/*NHNT*/
	if (!strnicmp(ext, ".media", 5) || !strnicmp(ext, ".info", 5) || !strnicmp(ext, ".nhnt", 5) || !stricmp(fmt, "NHNT") )
		return gf_import_nhnt(importer);
	/*NHML*/
	if (!strnicmp(ext, ".nhml", 5) || !stricmp(fmt, "NHML") )
		return gf_import_nhml_dims(importer, GF_FALSE);
	/*jpg & png & jp2*/
	if (!strnicmp(ext, ".jpg", 4) || !strnicmp(ext, ".jpeg", 5) || !strnicmp(ext, ".jp2", 4) || !strnicmp(ext, ".png", 4) || !stricmp(fmt, "JPEG") || !stricmp(fmt, "PNG") || !stricmp(fmt, "JP2") )
		return gf_import_still_image(importer, GF_TRUE);
	/*AMR & 3GPP2 speec codecs*/
	if (!strnicmp(ext, ".amr", 4) || !strnicmp(ext, ".awb", 4) || !strnicmp(ext, ".smv", 4) || !strnicmp(ext, ".evc", 4)
	        || !stricmp(fmt, "AMR") || !stricmp(fmt, "EVRC") || !stricmp(fmt, "SMV") )
		return gf_import_amr_evrc_smv(importer);
	/*QCelp & other in QCP file format*/
	if (!strnicmp(ext, ".qcp", 4) || !stricmp(fmt, "QCELP") )
		return gf_import_qcp(importer);
	/*MPEG-4 SAF multiplex*/
	if (!strnicmp(ext, ".saf", 4) || !strnicmp(ext, ".lsr", 4) || !stricmp(fmt, "SAF") )
		return gf_import_saf(importer);
	/*text subtitles*/
	if (!strnicmp(ext, ".srt", 4) || !strnicmp(ext, ".sub", 4) || !strnicmp(ext, ".ttxt", 5) || !strnicmp(ext, ".vtt", 4) || !strnicmp(ext, ".ttml", 5)
	        || !stricmp(fmt, "SRT") || !stricmp(fmt, "SUB") || !stricmp(fmt, "TEXT") || !stricmp(fmt, "VTT") || !stricmp(fmt, "TTML")) {
#ifndef GPAC_DISABLE_TTXT
		return gf_import_timed_text(importer);
#else
		return GF_NOT_SUPPORTED;
#endif
	}
	/*VobSub*/
	if (!strnicmp(ext, ".idx", 4) || !stricmp(fmt, "VOBSUB"))
		return gf_import_vobsub(importer);
	/*raw importer*/
	if (!stricmp(fmt, "RAW")) {
		return gf_import_raw_unit(importer);
	}
	/*DIMS*/
	if (!strnicmp(ext, ".dml", 4) || !stricmp(fmt, "DIMS") )
		return gf_import_nhml_dims(importer, GF_TRUE);
	/*SC3DMC*/
	if (!strnicmp(ext, ".s3d", 4) || !stricmp(fmt, "SC3DMC") )
		return gf_import_afx_sc3dmc(importer, GF_TRUE);

	if (!strnicmp(ext, ".txt", 4) || !strnicmp(ext, ".chap", 5) || !stricmp(fmt, "CHAP") )
		return gf_media_import_chapters_file(importer);

	if (!strnicmp(ext, ".swf", 4) || !strnicmp(ext, ".SWF", 4)) {
#ifndef GPAC_DISABLE_TTXT
		return gf_import_timed_text(importer);
#else
		return GF_NOT_SUPPORTED;
#endif
	}
	/*try XML things*/
	xml_type = gf_xml_get_root_type(importer->in_name, &e);
	if (xml_type) {
		if (!stricmp(xml_type, "TextStream") || !stricmp(xml_type, "text3GTrack") ) {
			gf_free(xml_type);
#ifndef GPAC_DISABLE_TTXT
			return gf_import_timed_text(importer);
#else
			return GF_NOT_SUPPORTED;
#endif
		}
		else if (!stricmp(xml_type, "NHNTStream")) {
			gf_free(xml_type);
			return gf_import_nhml_dims(importer, GF_FALSE);
		}
		else if (!stricmp(xml_type, "DIMSStream") ) {
			gf_free(xml_type);
			return gf_import_nhml_dims(importer, GF_TRUE);
		}
		gf_free(xml_type);
	}

	if (gf_m2ts_probe_file(importer->in_name))
		return gf_import_mpeg_ts(importer);

	return gf_import_message(importer, GF_NOT_SUPPORTED, "[Importer] Unknown input file type for \"%s\"", importer->in_name);
}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/
