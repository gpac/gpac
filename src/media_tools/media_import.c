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
		vsprintf(szMsg, format, args);
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


void gf_media_update_bitrate(GF_ISOFile *file, u32 track)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 i, count, timescale, db_size;
	u64 time_wnd, rate, max_rate, avg_rate, bitrate;
	Double br;
	GF_ISOSample sample;
	db_size = 0;

	rate = max_rate = avg_rate = time_wnd = 0;

	memset(&sample, 0, sizeof(GF_ISOSample));
	timescale = gf_isom_get_media_timescale(file, track);
	count = gf_isom_get_sample_count(file, track);
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

	br = (Double) (s64) gf_isom_get_media_duration(file, track);
	br /= timescale;
	bitrate = (u32) ((Double) (s64)avg_rate / br);
	bitrate *= 8;
	max_rate *= 8;

	/*move to bps*/
	gf_isom_update_bitrate(file, track, 1, (u32) bitrate, (u32) max_rate, db_size);

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
		import->tk_info[0].stream_type = GF_STREAM_SCENE;
		import->tk_info[0].media_oti = GPAC_OTI_SCENE_AFX;
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


#ifdef FILTER_FIXME

#ifndef GPAC_DISABLE_AV_PARSERS

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

static GF_Err gf_import_aac_loas(GF_MediaImporter *import)
{
	u8 oti;
	Bool destroy_esd;
	GF_Err e;
	Bool sync_frame;
	u16 sr, dts_inc;
	u32 timescale;
	GF_BitStream *bs, *dsi;
	GF_M4ADecSpecInfo acfg;
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
		import->tk_info[0].stream_type = GF_STREAM_AUDIO;
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
	gf_isom_set_audio_info(import->dest, track, di, timescale, (acfg.nb_chan>2) ? 2 : acfg.nb_chan, 16);

	/*add first sample*/
	samp = gf_isom_sample_new();
	samp->IsRAP = RAP;
	samp->dataLength = nbbytes;
	samp->data = (char *) aac_buf;

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


#endif /*GPAC_DISABLE_AV_PARSERS*/

#endif // FILTER_FIXME


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
		import->tk_info[0].stream_type = GF_STREAM_VISUAL;
		import->tk_info[0].flags = GF_IMPORT_USE_DATAREF | GF_IMPORT_NO_FRAME_DROP | GF_IMPORT_OVERRIDE_FPS;
		import->tk_info[0].video_info.FPS = AVI_frame_rate(in);
		import->tk_info[0].video_info.width = AVI_video_width(in);
		import->tk_info[0].video_info.height = AVI_video_height(in);
		comp = AVI_video_compressor(in);
		import->tk_info[0].media_4cc = GF_4CC((u8)comp[0], (u8)comp[1], (u8)comp[2], (u8)comp[3]);

		import->nb_tracks = 1;
		for (i=0; i<(u32) AVI_audio_tracks(in); i++) {
			import->tk_info[i+1].track_num = i+2;
			import->tk_info[i+1].stream_type = GF_STREAM_AUDIO;
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

static GF_Err gf_import_avi_audio(GF_MediaImporter *import)
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
	u32 track, di, trackID, track_in, i, num_samples, mtype, w, h, sr, sbr_sr, ch, mstype, cur_extract_mode;
	s32 trans_x, trans_y;
	s16 layer;
	u8 bps;
	char *lang;
	const char *orig_name = gf_url_get_resource_name(gf_isom_get_filename(import->orig));
	Bool sbr, ps;
	GF_ISOSample *samp;
	GF_ESD *origin_esd;
	GF_InitialObjectDescriptor *iod;
	Bool is_cenc;
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
			import->tk_info[i].flags = GF_IMPORT_USE_DATAREF;
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
		if (origin_esd && (origin_esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2)) {
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

	e = gf_isom_clone_track(import->orig, track_in, import->dest, (import->flags & GF_IMPORT_USE_DATAREF) ? GF_TRUE : GF_FALSE, &track);
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

	duration = (u64) (((Double)import->duration * gf_isom_get_media_timescale(import->orig, track_in)) / 1000);
	gf_isom_set_nalu_extract_mode(import->orig, track_in, GF_ISOM_NALU_EXTRACT_INSPECT);

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
		gf_isom_sample_del(&samp);

		gf_isom_copy_sample_info(import->dest, track, import->orig, track_in, i+1);

		gf_set_progress("Importing ISO File", i+1, num_samples);


		if (duration && (sampDTS > duration) ) break;
		if (import->flags & GF_IMPORT_DO_ABORT) break;
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
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, IV_size, buffer, len);
				gf_free(buffer);
			} else {
				e = gf_isom_track_cenc_add_sample_info(import->dest, track, container_type, IV_size, NULL, 0);
			}
			if (e) goto exit;

			e = gf_isom_set_sample_cenc_group(import->dest, track, i+1, Is_Encrypted, IV_size, KID, crypt_byte_block, skip_byte_block, constant_IV_size, constant_IV);
			if (e) goto exit;
		}
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

static GF_Err gf_import_raw_unit(GF_MediaImporter *import)
{
	GF_Err e;
	GF_ISOSample *samp;
	u32 mtype, track, di, timescale, read;
	FILE *src;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->flags |= GF_IMPORT_USE_DATAREF;
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


static GF_Err gf_import_vobsub(GF_MediaImporter *import)
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
				import->tk_info[import->nb_tracks].media_oti = GPAC_OTI_MEDIA_SUBPIC;
				import->tk_info[import->nb_tracks].stream_type = GF_STREAM_VISUAL;
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
			gf_import_message(import, GF_CORRUPTED_DATA, "Corrupted data found in file %s", filename);
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
			gf_import_message(import, GF_CORRUPTED_DATA, "Corrupted data found in file %s", filename);
			continue;
		}

		if (vobsub_get_subpic_duration(packet, psize, dsize, &duration) != GF_OK) {
			gf_import_message(import, GF_CORRUPTED_DATA, "Corrupted data found in file %s", filename);
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
			import->tk_info[0].media_4cc = GF_MEDIA_TYPE_CHAP;
			import->tk_info[0].stream_type = GF_STREAM_TEXT;
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
			if (gf_isom_get_media_type(import->dest, i+1) != GF_ISOM_MEDIA_VISUAL) continue;
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
GF_Err gf_media_import_chapters(GF_ISOFile *file, char *chap_file, Double import_fps)
{
	GF_MediaImporter import;
	memset(&import, 0, sizeof(GF_MediaImporter));
	import.dest = file;
	import.in_name = chap_file;
	import.video_fps = import_fps;
	import.streamFormat = "CHAP";
	return gf_media_import(&import);
}

void on_import_setup_failure(GF_Filter *f, void *on_setup_error_udta, GF_Err e)
{
	GF_MediaImporter *importer = (GF_MediaImporter *)on_setup_error_udta;
	importer->last_error = e;
}

GF_EXPORT
GF_Err gf_media_import(GF_MediaImporter *importer)
{
#ifndef GPAC_DISABLE_TTXT
	GF_Err gf_import_timed_text(GF_MediaImporter *import);
#endif
	GF_Err e;
	u32 i, count;
	GF_FilterSession *fsess;
	GF_Filter *prober, *src_filter;
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
	/*old specific importers not remapped to filter session*/
	/*raw importer*/
	if (!stricmp(fmt, "RAW")) {
		return gf_import_raw_unit(importer);
	}
	/*SC3DMC*/
	if (!strnicmp(ext, ".s3d", 4) || !stricmp(fmt, "SC3DMC") )
		return gf_import_afx_sc3dmc(importer, GF_TRUE);

#ifdef FILTER_FIXME
	#error "importer TODO: SAF, TS"
#endif

	e = GF_OK;
	fsess = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, NULL, GF_FALSE);
	importer->last_error = GF_OK;

	if (importer->flags & GF_IMPORT_PROBE_ONLY) {
		prober = gf_fs_load_filter(fsess, "probe:pid_only");

		src_filter = gf_fs_load_source(fsess, importer->in_name, NULL, NULL, &e);
		if (e) {
			gf_fs_del(fsess);
			return gf_import_message(importer, e, "[Importer] Cannot load filter for input file \"%s\"", importer->in_name);
		}
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
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
			tki->media_oti = p ? p->value.uint : GPAC_OTI_FORBIDDEN;
			//todo
			tki->flags=0;
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
				Double d = 1000 * p->value.frac.num;
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

			if (p && p->value.boolean) tki->flags |= GF_IMPORT_USE_DATAREF;

			importer->nb_tracks++;
		}
	} else {
		char szArgs[4096];
		char szSubArg[1024];
		GF_Filter *isobmff_mux;

		//mux args
		sprintf(szArgs, "mp4mx:mov=%p:importer", importer->dest);
		if (importer->flags & GF_IMPORT_FORCE_MPEG4) strcat(szArgs, ":m4sys:mpeg4");
		if (importer->flags & GF_IMPORT_USE_DATAREF) strcat(szArgs, ":dref");
		if (importer->flags & GF_IMPORT_NO_EDIT_LIST) strcat(szArgs, ":noedit");
		if (importer->flags & GF_IMPORT_FORCE_PACKED) strcat(szArgs, ":pack_nal");
		if (importer->flags & GF_IMPORT_FORCE_XPS_INBAND) strcat(szArgs, ":xps_inband=1");

		if (importer->duration) {
			sprintf(szSubArg, ":dur=%d/1000", importer->duration);
			strcat(szArgs, szSubArg);
		}
		if (importer->frames_per_sample) {
			sprintf(szSubArg, ":pack3gp=%d", importer->frames_per_sample);
			strcat(szArgs, szSubArg);
		}

		isobmff_mux = gf_fs_load_filter(fsess, szArgs);
		if (!isobmff_mux) {
			gf_fs_del(fsess);
			return gf_import_message(importer, GF_FILTER_NOT_FOUND, "[Importer] Cannot load ISOBMFF muxer");
		}

		//source args
		strcpy(szArgs, "importer");
		if (importer->flags & GF_IMPORT_SBR_IMPLICIT) strcat(szArgs, ":sbr=imp");
		else if (importer->flags & GF_IMPORT_SBR_EXPLICIT) strcat(szArgs, ":sbr=exp");
		if (importer->flags & GF_IMPORT_PS_IMPLICIT) strcat(szArgs, ":ps=imp");
		else if (importer->flags & GF_IMPORT_PS_EXPLICIT) strcat(szArgs, ":ps=exp");
		if (importer->flags & GF_IMPORT_OVSBR) strcat(szArgs, ":ovsbr");
		//avoids message at end of import
		if (importer->flags & GF_IMPORT_FORCE_PACKED) strcat(szArgs, ":nal_length=0");
		if (importer->flags & GF_IMPORT_SET_SUBSAMPLES) strcat(szArgs, ":subsamples");
		if (importer->flags & GF_IMPORT_NO_SEI) strcat(szArgs, ":nosei");
		if (importer->flags & GF_IMPORT_SVC_NONE) strcat(szArgs, ":nosvc");


		gf_fs_load_source(fsess, importer->in_name, szArgs, NULL, &e);
		if (e) {
			gf_fs_del(fsess);
			return gf_import_message(importer, e, "[Importer] Cannot load filter for input file \"%s\"", importer->in_name);
		}
		gf_fs_run(fsess);
		if (!importer->last_error) importer->last_error = gf_fs_get_last_connect_error(fsess);
		if (!importer->last_error) importer->last_error = gf_fs_get_last_process_error(fsess);

		if (importer->last_error) {
			gf_fs_del(fsess);
			return gf_import_message(importer, importer->last_error, "[Importer] Error probing %s", importer->in_name);
		}

		importer->final_trackID = gf_isom_get_last_created_track_id(importer->dest);
	}
	gf_fs_del(fsess);
	return GF_OK;

#if FILTER_FIXME

#ifndef GPAC_DISABLE_AVILIB
	/*AVI audio/video*/
	if (!strnicmp(ext, ".avi", 4) || !stricmp(fmt, "AVI") ) {
		e = gf_import_avi_video(importer);
		if (e) return e;
		return gf_import_avi_audio(importer);
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

#ifndef GPAC_DISABLE_AV_PARSERS
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
#endif

	/*NHNT*/
	if (!strnicmp(ext, ".media", 5) || !strnicmp(ext, ".info", 5) || !strnicmp(ext, ".nhnt", 5) || !stricmp(fmt, "NHNT") )
		return gf_import_nhnt(importer);
	/*NHML*/
	if (!strnicmp(ext, ".nhml", 5) || !stricmp(fmt, "NHML") )
		return gf_import_nhml_dims(importer, GF_FALSE);
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

	/*DIMS*/
	if (!strnicmp(ext, ".dml", 4) || !stricmp(fmt, "DIMS") )
		return gf_import_nhml_dims(importer, GF_TRUE);

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

	return gf_import_message(importer, e, "[Importer] Unknown input file type for \"%s\"", importer->in_name);


#endif


}



#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


