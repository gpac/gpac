/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
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
	if (gf_log_tool_level_on(GF_LOG_AUTHOR, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		va_list args;
		char szMsg[1024];
		va_start(args, format);
		vsnprintf(szMsg, 1024, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_AUTHOR, ("%s\n", szMsg) );
	}
#endif
	return e;
}

#ifndef GPAC_DISABLE_AV_PARSERS
/*that's very very crude, we only support vorbis & theora in MP4 - this will need cleanup as soon as possible*/
static GF_Err gf_dump_to_ogg(GF_MediaExporter *dumper, char *szName, u32 track)
{
#ifdef GPAC_DISABLE_OGG
	return GF_NOT_SUPPORTED;
#else
	FILE *out;
	ogg_stream_state os;
	ogg_packet op;
	ogg_page og;
	u32 count, i, di, theora_kgs, nb_i, nb_p;
	Bool flush_first = GF_TRUE;
	GF_BitStream *bs;
	GF_ISOSample *samp;
	GF_ESD *esd = gf_isom_get_esd(dumper->file, track, 1);


	memset(&os, 0, sizeof(ogg_stream_state));
	memset(&og, 0, sizeof(ogg_page));
	memset(&op, 0, sizeof(ogg_packet));

	if (gf_sys_is_test_mode()) {
		ogg_stream_init(&os, 1);
	} else {
		gf_rand_init(1);
		ogg_stream_init(&os, gf_rand());
	}

	out = szName ? gf_fopen(szName, "wb") : stdout;
	if (!out) return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);

	theora_kgs = 0;
	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	if (esd->decoderConfig->objectTypeIndication==GF_CODECID_OPUS) {
		GF_BitStream *bs_out;
		GF_OpusSpecificBox *dops = (GF_OpusSpecificBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DOPS);
		dops->size = gf_bs_read_u32(bs);
		gf_bs_read_u32(bs);
		gf_isom_box_read((GF_Box *)dops, bs);
		bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs_out, "OpusHead", 8);
		gf_bs_write_u8(bs_out, 1);//version
		gf_bs_write_u8(bs_out, dops->OutputChannelCount);
		gf_bs_write_u16_le(bs_out, dops->PreSkip);
		gf_bs_write_u32_le(bs_out, dops->InputSampleRate);
		gf_bs_write_u16_le(bs_out, dops->OutputGain);
		gf_bs_write_u8(bs_out, dops->ChannelMappingFamily);
		if (dops->ChannelMappingFamily) {
			gf_bs_write_u8(bs_out, dops->StreamCount);
			gf_bs_write_u8(bs_out, dops->CoupledCount);
			gf_bs_write_data(bs, (char *) dops->ChannelMapping, dops->OutputChannelCount);
		}
		gf_isom_box_del((GF_Box*)dops);

		gf_bs_get_content(bs_out, &op.packet, &op.bytes);
		gf_bs_del(bs_out);
		ogg_stream_packetin(&os, &op);
		gf_free(op.packet);
		op.packetno ++;

	} else {
		while (gf_bs_available(bs)) {
			op.bytes = gf_bs_read_u16(bs);
			op.packet = (unsigned char*)gf_malloc(sizeof(char) * op.bytes);
			gf_bs_read_data(bs, (char*)op.packet, op.bytes);
			ogg_stream_packetin(&os, &op);

			if (flush_first) {
				ogg_stream_pageout(&os, &og);
				gf_fwrite(og.header, 1, og.header_len, out);
				gf_fwrite(og.body, 1, og.body_len, out);
				flush_first = 0;

				if (esd->decoderConfig->objectTypeIndication==GF_CODECID_THEORA) {
					u32 kff;
					GF_BitStream *vbs = gf_bs_new((char*)op.packet, op.bytes, GF_BITSTREAM_READ);
					gf_bs_skip_bytes(vbs, 40);
					gf_bs_read_int(vbs, 6); /* quality */
					kff = 1 << gf_bs_read_int(vbs, 5);
					gf_bs_del(vbs);

					theora_kgs = 0;
					kff--;
					while (kff) {
						theora_kgs ++;
						kff >>= 1;
					}
				}
			}
			gf_free(op.packet);
			op.packetno ++;
		}
	}
	gf_bs_del(bs);
	gf_odf_desc_del((GF_Descriptor *)esd);

	while (ogg_stream_pageout(&os, &og)>0) {
		gf_fwrite(og.header, 1, og.header_len, out);
		gf_fwrite(og.body, 1, og.body_len, out);
	}

	op.granulepos = -1;

	count = gf_isom_get_sample_count(dumper->file, track);

	nb_i = nb_p = 0;
	samp = gf_isom_get_sample(dumper->file, track, 1, &di);
	for (i=0; i<count; i++) {
		GF_ISOSample *next_samp = gf_isom_get_sample(dumper->file, track, i+2, &di);
		if (!samp) break;
		op.bytes = samp->dataLength;
		op.packet = (unsigned char*)samp->data;
		op.packetno ++;

		if (theora_kgs) {
			if (samp->IsRAP) {
				if (i) nb_i+=nb_p+1;
				nb_p = 0;
			} else {
				nb_p++;
			}
			op.granulepos = nb_i;
			op.granulepos <<= theora_kgs;
			op.granulepos |= nb_p;
		} else {
			if (next_samp) op.granulepos = next_samp->DTS;
		}
		if (!next_samp) op.e_o_s = 1;

		ogg_stream_packetin(&os, &op);

		gf_isom_sample_del(&samp);
		samp = next_samp;
		next_samp = NULL;
		gf_set_progress("OGG Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;

		while (ogg_stream_pageout(&os, &og)>0) {
			gf_fwrite(og.header, 1, og.header_len, out);
			gf_fwrite(og.body, 1, og.body_len, out);
		}
	}
	if (samp) gf_isom_sample_del(&samp);

	while (ogg_stream_flush(&os, &og)>0) {
		gf_fwrite(og.header, 1, og.header_len, out);
		gf_fwrite(og.body, 1, og.body_len, out);
	}
	ogg_stream_clear(&os);
	if (szName) gf_fclose(out);
	return GF_OK;
#endif
}
#endif


#ifndef GPAC_DISABLE_AV_PARSERS
static GF_Err gf_dump_to_vobsub(GF_MediaExporter *dumper, char *szName, u32 track, char *dsi, u32 dsiSize)
{
#ifndef GPAC_DISABLE_VOBSUB
	FILE *fidx, *fsub;
	u32 width, height, i, count, di;
	GF_ISOSample *samp;
	char *lang = NULL;
	char szPath[GF_MAX_PATH];

	/* Check decoder specific information (palette) size - should be 64 */
	if (dsiSize != 64) {
		return gf_export_message(dumper, GF_CORRUPTED_DATA, "Invalid decoder specific info size - must be 64 but is %d", dsiSize);
	}

	/* Create an idx file */
	if (!gf_file_ext_start(szName)) {
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
	fputs("# VobSub index file, v7 (do not modify this line!)\n#\n", fidx);

	/* Write original subpicture resolution */
	fprintf(fidx, "size: %ux%u\n", width, height);

	/* Write palette */
	fputs("palette:", fidx);
	for (i = 0; i < 16; i++) {
		s32 y, u, v, r, g, b;

		y = (s32)(u8)dsi[(i<<2)+1] - 0x10;
		u = (s32)(u8)dsi[(i<<2)+3] - 0x80;
		v = (s32)(u8)dsi[(i<<2)+2] - 0x80;
		r = (298 * y           + 409 * v + 128) >> 8;
		g = (298 * y - 100 * u - 208 * v + 128) >> 8;
		b = (298 * y + 516 * u           + 128) >> 8;

		if (i) fputc(',', fidx);

#define CLIP(x) (((x) >= 0) ? (((x) < 256) ? (x) : 255) : 0)
		fprintf(fidx, " %02x%02x%02x", CLIP(r), CLIP(g), CLIP(b));
#undef CLIP
	}
	fputc('\n', fidx);

	/* Write some other useful values */
	fputs("# ON: displays only forced subtitles, OFF: shows everything\n", fidx);
	fputs("forced subs: OFF\n\n", fidx);

	/* Write current language index */
	fputs("# Language index in use\nlangidx: 0\n", fidx);

	/* Write language header */
	gf_isom_get_media_language(dumper->file, track, &lang);
	fprintf(fidx, "id: %s, index: 0\n", vobsub_lang_id(lang));
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
#if defined(WIN32)  && !defined(__GNUC__)
		fprintf(fidx, "timestamp: %02u:%02u:%02u:%03u, filepos: %09I64x\n", hh, mm, ss, ms, gf_ftell(fsub));
#else
		fprintf(fidx, "timestamp: %02u:%02u:%02u:%03u, filepos: %09"LLX_SUF"\n", hh, mm, ss, ms, gf_ftell(fsub));
#endif
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
	gf_isom_set_track_enabled(outfile, newTk, 1);

	if (esd) {
		gf_isom_new_mpeg4_description(outfile, newTk, esd, NULL, NULL, &descIndex);
		if ((esd->decoderConfig->streamType == GF_STREAM_VISUAL) || (esd->decoderConfig->streamType == GF_STREAM_SCENE)) {
			u32 w, h;
			gf_isom_get_visual_info(infile, inTrackNum, 1, &w, &h);
#ifndef GPAC_DISABLE_AV_PARSERS
			/*this is because so many files have reserved values of 320x240 from v1 ... */
			if (esd->decoderConfig->objectTypeIndication == GF_CODECID_MPEG4_PART2) {
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
	} else {
		gf_isom_clone_sample_description(outfile, newTk, infile, inTrackNum, 1, NULL, NULL, &descIndex);
	}

	pos = 0;
	rate = 0;
	ts = gf_isom_get_media_timescale(infile, inTrackNum);
	count = gf_isom_get_sample_count(infile, inTrackNum);
	for (i=0; i<count; i++) {
		samp = gf_isom_get_sample(infile, inTrackNum, i+1, &di);
		gf_isom_add_sample(outfile, newTk, descIndex, samp);
		if (esd) {
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
	else if (!esd) return gf_isom_remove_root_od(outfile);

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
		else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2) {
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
		else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_AAC_MPEG4) {
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
	char szName[1000], *ext;
	u32 track;
	u8 mode;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	if (gf_isom_get_media_type(dumper->file, dumper->trackID)==GF_ISOM_MEDIA_OD) {
		return gf_export_message(dumper, GF_BAD_PARAM, "Cannot extract OD track, result is  meaningless");
	}

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		dumper->flags |= GF_EXPORT_MERGE;
		return GF_OK;
	}
	if (strrchr(dumper->out_name, '.')) {
		strcpy(szName, dumper->out_name);
	} else {
		ext = (char *) gf_isom_get_filename(dumper->file);
		if (ext) ext = strrchr(ext, '.');
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

	e = gf_export_isom_copy_track(dumper, dumper->file, track, outfile, 1, add_to_iod);
	if (!add_to_iod) {
		u32 i;
		for (i=0; i<gf_isom_get_track_count(outfile); i++) {
			gf_isom_remove_track_from_root_od(outfile, i+1);
		}
	}
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
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
	fprintf(vtt, "WEBVTT Metadata track generated by GPAC MP4Box %s\n", gf_sys_is_test_mode() ? "" : gf_gpac_version());

	fprintf(vtt, "kind:metadata\n");
	{
		char *lang;
		gf_isom_get_media_language(dumper->file, track, &lang);
		fprintf(vtt, "language:%s\n", lang);
		gf_free(lang);
	}
	{
		const char *handler;
		gf_isom_get_handler_name(dumper->file, track, &handler);
		fprintf(vtt, "label: %s\n", handler);
	}
	if (gf_isom_is_track_in_root_od(dumper->file, track)) fprintf(vtt, "inRootOD: yes\n");
	fprintf(vtt, "trackID: %d\n", dumper->trackID);
	if (med) {
		fprintf(vtt, "baseMediaFile: %s\n", gf_file_basename(szMedia));
	}
	if (esd) {
		/* TODO: export the MPEG-4 Stream type only if it is not a GPAC internal value */
		fprintf(vtt, "MPEG-4-streamType: %d\n", esd->decoderConfig->streamType);
		/* TODO: export the MPEG-4 Object Type Indication only if it is not a GPAC internal value */
		fprintf(vtt, "MPEG-4-objectTypeIndication: %d\n", esd->decoderConfig->objectTypeIndication);
		if (gf_isom_is_video_subtype(mtype) ) {
			gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
			fprintf(vtt, "width:%d\n", w);
			fprintf(vtt, "height:%d\n", h);
		}
		else if (mtype==GF_ISOM_MEDIA_AUDIO) {
			u32 sr, nb_ch, bps;
			gf_isom_get_audio_info(dumper->file, track, 1, &sr, &nb_ch, &bps);
			fprintf(vtt, "sampleRate: %d\n", sr);
			fprintf(vtt, "numChannels: %d\n", nb_ch);
		} else if (isText) {
			u32 w, h;
			s32 tx, ty;
			s16 layer;
			gf_isom_get_track_layout_info(dumper->file, track, &w, &h, &tx, &ty, &layer);
			fprintf(vtt, "width:%d\n", w);
			fprintf(vtt, "height:%d\n", h);
			if (tx || ty) fprintf(vtt, "translation:%d,%d\n", tx, ty);
			if (layer) fprintf(vtt, "layer:%d\n", layer);
		}
		if (esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->data) {
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
							fprintf(vtt, "text-header: \n");
							gf_webvtt_dump_header_boxed(vtt, esd->decoderConfig->decoderSpecificInfo->data+4, esd->decoderConfig->decoderSpecificInfo->dataLength, &headerLength);
						}
					}
				} else {
					gf_webvtt_dump_header_boxed(med, esd->decoderConfig->decoderSpecificInfo->data+4, esd->decoderConfig->decoderSpecificInfo->dataLength, &headerLength);
					fprintf(vtt, "text-header-length: %d\n", headerLength);
				}
			} else {
				char b64[200];
				u32 size = gf_base64_encode(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, b64, 200);
				useBase64 = GF_TRUE;
				if (size != (u32)-1 && size != 0) {
					b64[size] = 0;
					fprintf(vtt, "MPEG-4-DecoderSpecificInfo: %s\n", b64);
				}
			}
		}
		gf_odf_desc_del((GF_Descriptor *) esd);
	} else {
		GF_GenericSampleDescription *sdesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
		fprintf(vtt, "mediaType: %s\n", gf_4cc_to_str(mtype));
		fprintf(vtt, "mediaSubType: %s\n", gf_4cc_to_str(mstype ));
		if (sdesc) {
			if (gf_isom_is_video_subtype(mtype) ) {
				fprintf(vtt, "codecVendor: %s\n", gf_4cc_to_str(sdesc->vendor_code));
				fprintf(vtt, "codecVersion: %d\n", sdesc->version);
				fprintf(vtt, "codecRevision: %d\n", sdesc->revision);
				fprintf(vtt, "width: %d\n", sdesc->width);
				fprintf(vtt, "height: %d\n", sdesc->height);
				fprintf(vtt, "compressorName: %s\n", sdesc->compressor_name);
				fprintf(vtt, "temporalQuality: %d\n", sdesc->temporal_quality);
				fprintf(vtt, "spatialQuality: %d\n", sdesc->spatial_quality);
				fprintf(vtt, "horizontalResolution: %d\n", sdesc->h_res);
				fprintf(vtt, "verticalResolution: %d\n", sdesc->v_res);
				fprintf(vtt, "bitDepth: %d\n", sdesc->depth);
			} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
				fprintf(vtt, "codecVendor: %s\n", gf_4cc_to_str(sdesc->vendor_code));
				fprintf(vtt, "codecVersion: %d\n", sdesc->version);
				fprintf(vtt, "codecRevision: %d\n", sdesc->revision);
				fprintf(vtt, "sampleRate: %d\n", sdesc->samplerate);
				fprintf(vtt, "numChannels: %d\n", sdesc->nb_channels);
				fprintf(vtt, "bitsPerSample: %d\n", sdesc->bits_per_sample);
			}
			if (sdesc->extension_buf) {
				char b64[200];
				u32 size = gf_base64_encode(sdesc->extension_buf, sdesc->extension_buf_size, b64, 200);
				useBase64 = GF_TRUE;
				if (size != (u32)-1) {
					b64[size] = 0;
					fprintf(vtt, "specificInfo: %s\n", b64);
					gf_free(sdesc->extension_buf);
				}
			}
			gf_free(sdesc);
		}
	}
	fprintf(vtt, "inBandMetadataTrackDispatchType: %s\n", (mime ? mime : (isText? "text/plain" : "application/octet-stream")));
	if (useBase64) fprintf(vtt, "encoding: base64\n");

	fprintf(vtt, "\n");

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
			fprintf(vtt, " --> ");
			gf_webvtt_timestamp_dump(&end, vtt, GF_TRUE);
			fprintf(vtt, " ");
			if (med) {
				fprintf(vtt, "mediaOffset:%d ", pos+headerLength);
				fprintf(vtt, "dataLength:%d ", samp->dataLength);
			}
			if (samp->CTS_Offset) fprintf(vtt, "CTS: "LLD"", samp->DTS+samp->CTS_Offset);
			if (samp->IsRAP==RAP) fprintf(vtt, "isRAP:true ");
			else if (samp->IsRAP==RAP_REDUNDANT) fprintf(vtt, "isSyncShadow: true ");
			else fprintf(vtt, "isRAP:false ");
			fprintf(vtt, "\n");
		}
		if (med) {
			gf_fwrite(samp->data, samp->dataLength, 1, med);
		} else if (dumper->flags & GF_EXPORT_WEBVTT_META_EMBEDDED) {
			if (isText) {
				samp->data = (char *)gf_realloc(samp->data, samp->dataLength+1);
				samp->data[samp->dataLength] = 0;
				fprintf(vtt, "%s\n", samp->data);
			} else {
				u32 b64_size;
				char *b64;
				b64 = (char *)gf_malloc(samp->dataLength*3);
				b64_size = gf_base64_encode(samp->data, samp->dataLength, b64, samp->dataLength*3);
				if (b64_size != (u32)-1) {
					b64[b64_size] = 0;
					fprintf(vtt, "%s\n", b64);
				}
				gf_free(b64);
			}
		}
		fprintf(vtt, "\n");

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
	u32 mstype;
	const char *szRootName;
	//Bool isText;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	if (!track) return gf_export_message(dumper, GF_BAD_PARAM, "Invalid track ID %d", dumper->trackID);

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		dumper->flags |= GF_EXPORT_NHML_FULL;
		return GF_OK;
	}
	esd = gf_isom_get_esd(dumper->file, track, 1);
	media = NULL;
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
	mstype = gf_isom_get_media_subtype(dumper->file, track, 1);

	/*write header*/
	fprintf(six, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(six, "<%s timescale=\"%d\" ", szRootName, gf_isom_get_media_timescale(dumper->file, track) );
	fprintf(six, "file=\"%s\" ", szMedia);
	fprintf(six, ">\n");
	header_size = 0;
	if (esd) {
		if (esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->data) {
#if !defined(GPAC_DISABLE_TTXT) && !defined(GPAC_DISABLE_VTT)
			if (mstype == GF_ISOM_SUBTYPE_WVTT || mstype == GF_ISOM_SUBTYPE_STXT) {
				gf_webvtt_dump_header_boxed(media,
				                            esd->decoderConfig->decoderSpecificInfo->data+4,
				                            esd->decoderConfig->decoderSpecificInfo->dataLength,
				                            &header_size);
			} else
#endif
			{
				gf_fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 1, media);
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
	fprintf(six, "<header range-begin=\"0\" range-end=\"%d\"/>\n", header_size-1);

	pos = header_size;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		if (media) {
			gf_fwrite(samp->data, samp->dataLength, 1, media);
		}

		fprintf(six, "<unit time=\""LLU"\" ", LLU_CAST samp->DTS);
		if (samp->IsRAP==RAP) fprintf(six, "rap=\"1\" ");
		else if (samp->IsRAP==RAP_NO) fprintf(six, "rap=\"0\" ");
		fprintf(six, "range-begin=\"%d\" ", pos);
		fprintf(six, "range-end=\"%d\" ", pos+samp->dataLength-1);
		fprintf(six, "/>\n");

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		gf_set_progress("SIX Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	fprintf(six, "</%s>\n", szRootName);
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
		u32 time_scale, mtype, stream_id;
		GF_ESD *esd;
		mtype = gf_isom_get_media_type(dumper->file, i+1);
		if (mtype==GF_ISOM_MEDIA_OD) continue;
		if (mtype==GF_ISOM_MEDIA_HINT) continue;

		time_scale = gf_isom_get_media_timescale(dumper->file, i+1);
		esd = gf_isom_get_esd(dumper->file, i+1, 1);
		if (esd) {
			stream_id = gf_isom_find_od_for_track(dumper->file, i+1);
			if (!stream_id) stream_id = esd->ESID;

			/*translate OD IDs to ESIDs !!*/
			if (esd->decoderConfig->decoderSpecificInfo) {
				gf_saf_mux_stream_add(mux, stream_id, time_scale, esd->decoderConfig->bufferSizeDB, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication, NULL, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->URLString);
			} else {
				gf_saf_mux_stream_add(mux, stream_id, time_scale, esd->decoderConfig->bufferSizeDB, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication, NULL, NULL, 0, esd->URLString);
			}
			gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
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
			gf_saf_mux_add_au(mux, safs[i].stream_id, (u32) (samp->DTS+samp->CTS_Offset), samp->data, samp->dataLength, (samp->IsRAP==RAP) ? 1 : 0);
			/*data is kept by muxer!!*/
			gf_free(samp);
			safs[i].last_sample++;
			samp_done ++;
		}
		while (1) {
			gf_saf_mux_for_time(mux, (u32) -1, 0, &data, &size);
			if (!data) break;
			gf_fwrite(data, size, 1, saf_f);
			gf_free(data);
		}
		gf_set_progress("SAF Export", samp_done, tot_samp);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	gf_saf_mux_for_time(mux, (u32) -1, 1, &data, &size);
	if (data) {
		gf_fwrite(data, size, 1, saf_f);
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
	const char *src;
	char *args, szSubArgs[1024], szExt[30];
	GF_Filter *file_out, *reframer, *remux=NULL, *src_filter;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;
	u32 codec_id=0;
	u32 sample_count=0;
	Bool skip_write_filter = GF_FALSE;

	args = NULL;
	strcpy(szExt, "");
	if (dumper->trackID && dumper->file) {
		u32 msubtype = 0;
		u32 mtype = 0;
		u32 afmt = 0;
		GF_ESD *esd;
		const char *export_ext = dumper->out_name ? gf_file_ext_start(dumper->out_name) : NULL;
		u32 track_num = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
		if (!track_num) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] No tracks with ID %d in file\n", dumper->trackID));
			return GF_BAD_PARAM;
		}
		esd = gf_media_map_esd(dumper->file, track_num, 0);
		sample_count = gf_isom_get_sample_count(dumper->file, dumper->trackID);
		if (esd) {
			if (esd->decoderConfig->objectTypeIndication<GF_CODECID_LAST_MPEG4_MAPPING) {
				codec_id = gf_codecid_from_oti(esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
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
			strcpy(szExt, gf_4cc_to_str(msubtype));
		} else if (codec_id==GF_CODECID_RAW) {
			switch (mtype) {
			case GF_ISOM_MEDIA_VISUAL:
			case GF_ISOM_MEDIA_AUXV:
			case GF_ISOM_MEDIA_PICT:
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
			char *sep;
			const char *sname = gf_codecid_file_ext(codec_id);
			if (export_ext && strstr(sname, export_ext+1)) {
				szExt[0]=0;
			} else {
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
			skip_write_filter = GF_TRUE;
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
				//szExt[0] = 0;
				strcpy(szExt, "*");
				break;
			}
			break;
		}
		//TODO, move these two to filters one of these days
		if ((codec_id==GF_CODECID_VORBIS) || (codec_id==GF_CODECID_THEORA) || (codec_id==GF_CODECID_OPUS)) {
			char *outname = dumper->out_name;
			if (outname && !strcmp(outname, "std")) outname=NULL;
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return gf_dump_to_ogg(dumper, outname, track_num);
		}
		if (codec_id==GF_CODECID_SUBPIC) {
			char *dsi = NULL;
			u32 dsi_size = 0;
			if (esd && esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo) {
				dsi = esd->decoderConfig->decoderSpecificInfo->data;
				dsi_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
			}
			e = gf_dump_to_vobsub(dumper, dumper->out_name, track_num, dsi, dsi_size);
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return e;
		}
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
	}

	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}
	file_out = NULL;
	args = NULL;
	//except in nhml inband file dump, create a sink filter
	if (!dumper->dump_file && !(dumper->flags & GF_EXPORT_AVI)) {
		Bool no_ext = GF_FALSE;
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
		} else if (dumper->flags & GF_EXPORT_AVI_NATIVE) {
			if (!strlen(szExt)) {
				e |= gf_dynstrcat(&args, ":dynext", NULL);
			}
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
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load arguments for output file dumper\n"));
			if (args) gf_free(args);
			gf_fs_del(fsess);
			return e;
		}

		file_out = gf_fs_load_filter(fsess, args);
		if (!file_out) {
			gf_fs_del(fsess);
			if (args) gf_free(args);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load output file dumper\n"));
			return GF_FILTER_NOT_FOUND;
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
		remux = e ? NULL : gf_fs_load_filter(fsess, args);
		if (!remux || e) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load stream->file filter\n"));
			if (args) gf_free(args);
			return e ? e : GF_FILTER_NOT_FOUND;
		}
	}
	else if (dumper->flags & GF_EXPORT_NHNT) {
		remux = gf_fs_load_filter(fsess, "nhntw");
		if (!remux) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load NHNT write filter\n"));
			return GF_FILTER_NOT_FOUND;
		}
	}
	else if (dumper->flags & GF_EXPORT_NHML) {
		e = gf_dynstrcat(&args, "nhmlw:name=", NULL);
		e |= gf_dynstrcat(&args, dumper->out_name, NULL);
		if (dumper->flags & GF_EXPORT_NHML_FULL)
			e |= gf_dynstrcat(&args, ":full", NULL);
		if (dumper->dump_file) {
			sprintf(szSubArgs, ":nhmlonly:filep=%p", dumper->dump_file);
			e |= gf_dynstrcat(&args, szSubArgs, NULL);
		}
		remux = e ? NULL : gf_fs_load_filter(fsess, args);
		if (!remux || e) {
			gf_fs_del(fsess);
			if (args) gf_free(args);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load NHML write filter\n"));
			return e ? e : GF_FILTER_NOT_FOUND;
		}
	}
	else if (dumper->flags & GF_EXPORT_AVI) {
		e = gf_dynstrcat(&args, "avimx:dst=", NULL);
		e |= gf_dynstrcat(&args, dumper->out_name, NULL);
		if (!strstr(args, ".avi")) e |= gf_dynstrcat(&args, ".avi", NULL);
		e |= gf_dynstrcat(&args, ":noraw", NULL);

		file_out = e ? NULL : gf_fs_load_filter(fsess, args);
		if (!file_out || e) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load AVI output filter\n"));
			if (args) gf_free(args);
			return e ? e : GF_FILTER_NOT_FOUND;
		}
	} else if (!skip_write_filter) {
		remux = gf_fs_load_filter(fsess, "writegen");
		if (!remux) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load stream->file filter\n"));
			return GF_FILTER_NOT_FOUND;
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

	reframer = gf_fs_load_filter(fsess, args);
	if (!reframer || e) {
		gf_fs_del(fsess);
		if (args) gf_free(args);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load reframer filter\n"));
		return e ? e : GF_FILTER_NOT_FOUND;
	}
	if (args) gf_free(args);
	args = NULL;

	src = dumper->in_name ? dumper->in_name : gf_isom_get_filename(dumper->file);
	src_filter = gf_fs_load_source(fsess, src, "FID=1:noedit", NULL, &e);
	if (!src_filter || e) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load filter for input file \"%s\": %s\n", dumper->in_name, gf_error_to_string(e) ));
		return e;
	}

	if (dumper->trackID) {
		sprintf(szSubArgs, "PID=%d", dumper->trackID);
	}
	if (remux) {
		gf_filter_set_source(file_out, remux, dumper->trackID ? szSubArgs : NULL);
		gf_filter_set_source(remux, reframer, dumper->trackID ? szSubArgs : NULL);
	} else {
		gf_filter_set_source(file_out, reframer, dumper->trackID ? szSubArgs : NULL);
	}

	e = gf_fs_run(fsess);
	if (e>GF_OK) e = GF_OK;
	if (!e) e = gf_fs_get_last_connect_error(fsess);
	if (!e) e = gf_fs_get_last_process_error(fsess);
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
