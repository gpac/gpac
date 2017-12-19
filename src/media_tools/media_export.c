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

GF_Err gf_media_export_nhml(GF_MediaExporter *dumper, Bool dims_doc);

static GF_Err gf_export_message(GF_MediaExporter *dumper, GF_Err e, char *format, ...)
{
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return e;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_AUTHOR, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		va_list args;
		char szMsg[1024];
		va_start(args, format);
		vsprintf(szMsg, format, args);
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
	GF_BitStream *bs;
	GF_ISOSample *samp;
	GF_ESD *esd = gf_isom_get_esd(dumper->file, track, 1);


	gf_rand_init(1);
	ogg_stream_init(&os, gf_rand());

	op.granulepos = 0;
	op.packetno = 0;
	op.b_o_s = 1;
	op.e_o_s = 0;

	out = szName ? gf_fopen(szName, "wb") : stdout;
	if (!out) return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);

	theora_kgs = 0;
	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		op.bytes = gf_bs_read_u16(bs);
		op.packet = (unsigned char*)gf_malloc(sizeof(char) * op.bytes);
		gf_bs_read_data(bs, (char*)op.packet, op.bytes);
		ogg_stream_packetin(&os, &op);

		if (op.b_o_s) {
			ogg_stream_pageout(&os, &og);
			gf_fwrite(og.header, 1, og.header_len, out);
			gf_fwrite(og.body, 1, og.body_len, out);
			op.b_o_s = 0;

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

GF_Err gf_export_hint(GF_MediaExporter *dumper)
{
#ifndef GPAC_DISABLE_ISOM_HINTING
	GF_Err e;
	char szName[1000], szType[5];
	char *pck;
	FILE *out;
	u32 track, i, size, m_stype, sn, count;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	m_stype = gf_isom_get_media_subtype(dumper->file, track, 1);

	e = gf_isom_reset_hint_reader(dumper->file, track, dumper->sample_num ? dumper->sample_num : 1, 0, 0, 0);
	if (e) return gf_export_message(dumper, e, "Error initializing hint reader");

	gf_export_message(dumper, GF_OK, "Extracting hint track samples - type %s", szType);

	count = gf_isom_get_sample_count(dumper->file, track);
	if (dumper->sample_num) count = 0;

	i = 1;
	while (1) {
		e = gf_isom_next_hint_packet(dumper->file, track, &pck, &size, NULL, NULL, NULL, &sn);
		if (e==GF_EOS) break;
		if (dumper->sample_num && (dumper->sample_num != sn)) {
			gf_free(pck);
			break;
		}
		if (e) return gf_export_message(dumper, e, "Error fetching hint packet %d", i);
		sprintf(szName, "%s_pck_%04d.%s", dumper->out_name, i, gf_4cc_to_str(m_stype));
		out = gf_fopen(szName, "wb");
		gf_fwrite(pck, size, 1, out);
		gf_fclose(out);
		gf_free(pck);
		i++;
		if (count) gf_set_progress("Hint Export", sn, count);
	}
	if (count) gf_set_progress("Hint Export", count, count);

	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}


#ifndef GPAC_DISABLE_AV_PARSERS
static GF_Err gf_dump_to_vobsub(GF_MediaExporter *dumper, char *szName, u32 track, char *dsi, u32 dsiSize)
{
#ifndef GPAC_DISABLE_VOBSUB
	FILE			*fidx, *fsub;
	u32				 width, height, i, count, di;
	GF_ISOSample	*samp;
	char			 *lang = NULL;

	/* Check decoder specific information (palette) size - should be 64 */
	if (dsiSize != 64) {
		return gf_export_message(dumper, GF_CORRUPTED_DATA, "Invalid decoder specific info size - must be 64 but is %d", dsiSize);
	}

	/* Create an idx file */
	fidx = gf_fopen(szName, "w");
	if (!fidx) {
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	/* Create a sub file */
	vobsub_trim_ext(szName);
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
		fprintf(fidx, "timestamp: %02u:%02u:%02u:%03u, filepos: " LLXPAD("09") "\n", hh, mm, ss, ms, gf_ftell(fsub));
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


#include <gpac/filters.h>

GF_Err gf_media_export_native(GF_MediaExporter *dumper)
{
	const char *src;
	char szArgs[4096], szSubArgs[1024];
	GF_Filter *file_out, *reframer;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;

	fsess = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, NULL, GF_FALSE, GF_FALSE);

	//mux args, for now we only dump to file
	sprintf(szArgs, "fileout:dst=%s:dynext", dumper->out_name);

	file_out = gf_fs_load_filter(fsess, szArgs);
	if (!file_out) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load output file dumper\n"));
		return GF_FILTER_NOT_FOUND;
	}
	//raw sample frame, force loading filter generic write in frame mode
	if (dumper->flags & GF_EXPORT_RAW_SAMPLES) {
		GF_Filter *rframe;
		sprintf(szArgs, "write_gen:frame");
		if (dumper->sample_num) {
			sprintf(szSubArgs, ":sstart=%d:send=%d", dumper->sample_num, dumper->sample_num);
			strcat(szArgs, szSubArgs);
		}
		rframe = gf_fs_load_filter(fsess, szArgs);
		if (!rframe) {
			gf_fs_del(fsess);
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load stream->file filter\n"));
			return GF_FILTER_NOT_FOUND;
		}
	}

	//force a reframer filter, connected to our input
	sprintf(szArgs, "reframer:SID=1#PID=%d:exporter", dumper->trackID);
	if (dumper->flags & GF_EXPORT_SVC_LAYER)
		strcat(szArgs, ":extract=layer");
	if (dumper->flags & GF_EXPORT_WEBVTT_NOMERGE)
		strcat(szArgs, ":merge");


	reframer = gf_fs_load_filter(fsess, szArgs);
	if (!reframer) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load reframer filter\n"));
		return GF_FILTER_NOT_FOUND;
	}

	src = dumper->in_name ? dumper->in_name : gf_isom_get_filename(dumper->file);
	gf_fs_load_source(fsess, src, "FID=1", NULL, &e);
	if (e) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Exporter] Cannot load filter for input file \"%s\": %s\n", dumper->in_name, gf_error_to_string(e) ));
		return e;
	}
	gf_fs_run(fsess);
	gf_fs_del(fsess);
	return GF_OK;
}

#ifndef GPAC_DISABLE_AVILIB
static GF_Err gf_media_export_avi_track(GF_MediaExporter *dumper)
{
	GF_Err e;
	Bool is_stdout = 0;
	u32 max_size, tot_size, num_samples, i;
	s32 size;
	char *comp, *frame;
	char szOutFile[1024];
	avi_t *in;
	FILE *fout;

	in = AVI_open_input_file(dumper->in_name, 1);
	if (!in) return gf_export_message(dumper, GF_URL_ERROR, "Unsupported avi file");
	fout = NULL;

	e = GF_OK;
	if (dumper->trackID==1) {
		Bool key;
		comp = AVI_video_compressor(in);
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
		   ) {
			sprintf(szOutFile, "%s.cmp", dumper->out_name);
		} else if (!stricmp(comp, "VSSH") || strstr(comp, "264")) {
			sprintf(szOutFile, "%s.h264", dumper->out_name);
		} else {
			sprintf(szOutFile, "%s.%s", dumper->out_name, comp);
		}
		gf_export_message(dumper, GF_OK, "Extracting AVI video (format %s) to %s", comp, szOutFile);

		if (!strcmp(dumper->out_name, "std"))
			is_stdout = 1;
		else if (strrchr(dumper->out_name, '.')) {
			strcpy(szOutFile, dumper->out_name);
		}

		fout = is_stdout ? stdout : gf_fopen(szOutFile, "wb");

		max_size = 0;
		frame = NULL;
		num_samples = (u32) AVI_video_frames(in);
		for (i=0; i<num_samples; i++) {
			size = (s32) AVI_frame_size(in, i);
			if (!size) {
				AVI_read_frame(in, NULL, (int*)&key);
				continue;
			}
			if ((u32) size > max_size) {
				frame = (char*)gf_realloc(frame, sizeof(char) * size);
				max_size = size;
			}
			AVI_read_frame(in, frame, (int*)&key);
			if ((u32) size>4) gf_fwrite(frame, 1, size, fout);
			gf_set_progress("AVI Extract", i+1, num_samples);
		}
		gf_free(frame);
		if(!is_stdout)
			gf_fclose(fout);
		fout = NULL;
		goto exit;
	}
	i = 0;
	tot_size = max_size = 0;
	size = (s32) AVI_audio_size(in, i);
	if (size < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[AVIExport] Error reading AVI audio sample\n"));
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit;
	}

	while (size > 0) {
		if (max_size < (u32) size) max_size = size;
		tot_size += size;
		i++;
		size = (s32) AVI_audio_size(in, i);
	}

	frame = (char*)gf_malloc(sizeof(char) * max_size);
	AVI_seek_start(in);
	AVI_set_audio_position(in, 0);

	switch (in->track[in->aptr].a_fmt) {
	case WAVE_FORMAT_PCM:
		comp = "pcm";
		break;
	case WAVE_FORMAT_ADPCM:
		comp = "adpcm";
		break;
	case WAVE_FORMAT_IBM_CVSD:
		comp = "cvsd";
		break;
	case WAVE_FORMAT_ALAW:
		comp = "alaw";
		break;
	case WAVE_FORMAT_MULAW:
		comp = "mulaw";
		break;
	case WAVE_FORMAT_OKI_ADPCM:
		comp = "oki_adpcm";
		break;
	case WAVE_FORMAT_DVI_ADPCM:
		comp = "dvi_adpcm";
		break;
	case WAVE_FORMAT_DIGISTD:
		comp = "digistd";
		break;
	case WAVE_FORMAT_YAMAHA_ADPCM:
		comp = "yam_adpcm";
		break;
	case WAVE_FORMAT_DSP_TRUESPEECH:
		comp = "truespeech";
		break;
	case WAVE_FORMAT_GSM610:
		comp = "gsm610";
		break;
	case IBM_FORMAT_MULAW:
		comp = "ibm_mulaw";
		break;
	case IBM_FORMAT_ALAW:
		comp = "ibm_alaw";
		break;
	case IBM_FORMAT_ADPCM:
		comp = "ibm_adpcm";
		break;
	case 0x55:
		comp = "mp3";
		break;
	case 0x0000706d:
		comp = "aac";
		break;
	default:
		comp = "raw";
		break;
	}
	sprintf(szOutFile, "%s.%s", dumper->out_name, comp);
	gf_export_message(dumper, GF_OK, "Extracting AVI %s audio", comp);

	if (!strcmp(dumper->out_name, "std")) {
		is_stdout = 1;
	} else if (strrchr(dumper->out_name, '.')) {
		strcpy(szOutFile, dumper->out_name);
	}

	fout = is_stdout ? stdout : gf_fopen(szOutFile, "wb");
	num_samples = 0;
	while (1) {
		Bool continuous;
		size = (s32) AVI_read_audio(in, frame, max_size, (int*)&continuous);
		if (size < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[AVIExport] Error reading AVI audio sample\n"));
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		if (size == 0)
			break;

		num_samples += size;
		gf_fwrite(frame, 1, size, fout);
		gf_set_progress("AVI Extract", num_samples, tot_size);

	}


exit:
	if (fout && !is_stdout) gf_fclose(fout);
	AVI_close(in);
	return e;
}
#endif /*GPAC_DISABLE_AVILIB*/

GF_Err gf_media_export_nhnt(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	char szName[1000];
	FILE *out_med, *out_inf, *out_nhnt;
	GF_BitStream *bs;
	Bool has_b_frames;
	u32 track, i, di, count, pos;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	esd = gf_isom_get_esd(dumper->file, track, 1);
	if (!esd) return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Invalid MPEG-4 stream in track ID %d", dumper->trackID);

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		return GF_OK;
	}

	sprintf(szName, "%s.media", dumper->out_name);
	out_med = gf_fopen(szName, "wb");
	if (!out_med) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	sprintf(szName, "%s.nhnt", dumper->out_name);
	out_nhnt = gf_fopen(szName, "wb");
	if (!out_nhnt) {
		gf_fclose(out_med);
		gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}


	bs = gf_bs_from_file(out_nhnt, GF_BITSTREAM_WRITE);

	if (esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->data) {
		sprintf(szName, "%s.info", dumper->out_name);
		out_inf = gf_fopen(szName, "wb");
		if (out_inf) gf_fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 1, out_inf);
		gf_fclose(out_inf);
	}

	/*write header*/
	/*'NHnt' format*/
	gf_bs_write_data(bs, "NHnt", 4);
	/*version 1*/
	gf_bs_write_u8(bs, 0);
	/*streamType*/
	gf_bs_write_u8(bs, esd->decoderConfig->streamType);
	/*OTI*/
	gf_bs_write_u8(bs, esd->decoderConfig->objectTypeIndication);
	/*reserved*/
	gf_bs_write_u16(bs, 0);
	/*bufferDB*/
	gf_bs_write_u24(bs, esd->decoderConfig->bufferSizeDB);
	/*avg BitRate*/
	gf_bs_write_u32(bs, esd->decoderConfig->avgBitrate);
	/*max bitrate*/
	gf_bs_write_u32(bs, esd->decoderConfig->maxBitrate);
	/*timescale*/
	gf_bs_write_u32(bs, esd->slConfig->timestampResolution);

	gf_odf_desc_del((GF_Descriptor *) esd);

	has_b_frames = gf_isom_has_time_offset(dumper->file, track);

	pos = 0;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;
		gf_fwrite(samp->data, samp->dataLength, 1, out_med);

		/*dump nhnt info*/
		gf_bs_write_u24(bs, samp->dataLength);
		gf_bs_write_int(bs, samp->IsRAP, 1);
		/*AU start & end flag always true*/
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, 1, 1);
		/*reserved*/
		gf_bs_write_int(bs, 0, 3);
		/*type - try to guess it*/
		if (has_b_frames) {
			if (samp->IsRAP) gf_bs_write_int(bs, 0, 2);
			/*if CTS offset, assime P*/
			else if (samp->CTS_Offset) gf_bs_write_int(bs, 1, 2);
			else gf_bs_write_int(bs, 2, 2);
		} else {
			gf_bs_write_int(bs, samp->IsRAP ? 0 : 1, 2);
		}
		gf_bs_write_u32(bs, pos);
		/*TODO support for large files*/
		gf_bs_write_u32(bs, (u32) (samp->DTS + samp->CTS_Offset) );
		gf_bs_write_u32(bs, (u32) samp->DTS);

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		gf_set_progress("NHNT Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	gf_fclose(out_med);
	gf_bs_del(bs);
	gf_fclose(out_nhnt);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
static GF_Err MP4T_CopyTrack(GF_MediaExporter *dumper, GF_ISOFile *infile, u32 inTrackNum, GF_ISOFile *outfile, Bool ResetDependancies, Bool AddToIOD)
{
	GF_ESD *esd;
	GF_InitialObjectDescriptor *iod;
	u32 TrackID, newTk, descIndex, i, ts, rate, pos, di, count, msubtype;
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

	//get the ESD and remove dependancies
	esd = NULL;
	msubtype = gf_isom_get_media_subtype(infile, inTrackNum, 1);

	if (msubtype == GF_ISOM_SUBTYPE_MPEG4) {
		esd = gf_isom_get_esd(infile, inTrackNum, 1);
		if (esd && ResetDependancies) {
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

	e = MP4T_CopyTrack(dumper, dumper->file, track, outfile, 1, add_to_iod);
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



#ifndef GPAC_DISABLE_AVILIB

GF_Err gf_media_export_avi(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	GF_ISOSample *samp;
	char szName[1000], *v4CC;
	avi_t *avi_out;
	char dumdata[1];
	u32 track, i, di, count, w, h, frame_d;
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_M4VDecSpecInfo dsi;
#endif
	Double FPS;

	if (!(track = gf_isom_get_track_by_id(dumper->file, dumper->trackID))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong track ID %d for file %s \n", dumper->trackID, gf_isom_get_filename(dumper->file)));
		return GF_BAD_PARAM;
	}
	esd = gf_isom_get_esd(dumper->file, track, 1);
	if (!esd) return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Invalid MPEG-4 stream in track ID %d", dumper->trackID);

	if ((esd->decoderConfig->streamType!=GF_STREAM_VISUAL) ||
	        ( (esd->decoderConfig->objectTypeIndication!=GF_CODECID_MPEG4_PART2)
	          && (esd->decoderConfig->objectTypeIndication!=GF_CODECID_AVC)
	          && (esd->decoderConfig->objectTypeIndication!=GF_CODECID_SVC)
	          && (esd->decoderConfig->objectTypeIndication!=GF_CODECID_MVC)
	          && (esd->decoderConfig->objectTypeIndication!=GF_CODECID_HEVC)
	          && (esd->decoderConfig->objectTypeIndication!=GF_CODECID_LHVC)
	        ) ) {
		gf_odf_desc_del((GF_Descriptor*)esd);
		return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Track ID %d is not MPEG-4 Visual - cannot extract to AVI", dumper->trackID);
	}
	if (!esd->decoderConfig->decoderSpecificInfo) {
		gf_odf_desc_del((GF_Descriptor*)esd);
		return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Missing decoder config for track ID %d", dumper->trackID);
	}
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return GF_OK;

	if (strrchr(dumper->out_name, '.')) {
		strcpy(szName, dumper->out_name);
	} else {
		sprintf(szName, "%s.avi", dumper->out_name);
	}
	avi_out = AVI_open_output_file(szName);
	if (!avi_out) {
		gf_odf_desc_del((GF_Descriptor *)esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	/*compute FPS - note we assume constant frame rate without dropped frames...*/
	count = gf_isom_get_sample_count(dumper->file, track);
	FPS = gf_isom_get_media_timescale(dumper->file, track);
	FPS *= (count-1);
	samp = gf_isom_get_sample(dumper->file, track, count, &di);
	if (!samp) {
		return gf_export_message(dumper, GF_ISOM_INVALID_FILE, "Error fetching first sample");
	}
	FPS /= (s64) samp->DTS;
	gf_isom_sample_del(&samp);

	frame_d = 0;
	/*AVC - FIXME dump format is probably wrong...*/
	if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_AVC) || (esd->decoderConfig->objectTypeIndication==GF_CODECID_SVC) || (esd->decoderConfig->objectTypeIndication==GF_CODECID_MVC)) {
		gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
		v4CC = "h264";
	}
	/*HEVC - FIXME dump format is probably wrong...*/
	else if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_HEVC) || (esd->decoderConfig->objectTypeIndication==GF_CODECID_LHVC)) {
		gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
		v4CC = "hevc";
	}
	/*MPEG4*/
	else {
#ifndef GPAC_DISABLE_AV_PARSERS
		/*ignore visual size info, get it from dsi*/
		gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
		w = dsi.width;
		h = dsi.height;
#else
		gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
#endif

		v4CC = "XVID";

		/*compute VfW delay*/
		if (gf_isom_has_time_offset(dumper->file, track)) {
			s32 max_CTSO;
			u64 DTS;
			DTS = max_CTSO = 0;
			for (i=0; i<count; i++) {
				samp = gf_isom_get_sample_info(dumper->file, track, i+1, NULL, NULL);
				if (!samp) break;
				if (samp->CTS_Offset>max_CTSO) max_CTSO = samp->CTS_Offset;
				DTS = samp->DTS;
				gf_isom_sample_del(&samp);
			}
			if (count>1) DTS /= (count-1);
			if (DTS) frame_d = max_CTSO / (u32) DTS;
			frame_d -= 1;
			/*dummy delay frame for xvid unpacked bitstreams*/
			dumdata[0] = 127;
		}
	}

	gf_export_message(dumper, GF_OK, "Creating AVI file %d x %d @ %.2f FPS - 4CC \"%s\"", w, h, FPS,v4CC);
	if (frame_d) gf_export_message(dumper, GF_OK, "B-Frames detected - using unpacked bitstream with max B-VOP delta %d", frame_d);

	AVI_set_video(avi_out, w, h, FPS, v4CC);


	for (i=0; i<count; i++) {
		samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		/*add DSI before each I-frame in MPEG-4 SP*/
		if (samp->IsRAP && (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2)) {
			char *data = (char*) gf_malloc(sizeof(char) * (samp->dataLength + esd->decoderConfig->decoderSpecificInfo->dataLength));
			memcpy(data, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			memcpy(data + esd->decoderConfig->decoderSpecificInfo->dataLength, samp->data, samp->dataLength);
			AVI_write_frame(avi_out, data, samp->dataLength + esd->decoderConfig->decoderSpecificInfo->dataLength, 1);
			gf_free(data);
		} else {
			AVI_write_frame(avi_out, samp->data, samp->dataLength, samp->IsRAP);
		}
		gf_isom_sample_del(&samp);
		while (frame_d) {
			AVI_write_frame(avi_out, dumdata, 1, 0);
			frame_d--;
		}
		gf_set_progress("AVI Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}

	gf_odf_desc_del((GF_Descriptor *) esd);
	AVI_close(avi_out);
	return GF_OK;
}
#endif /*GPAC_DISABLE_AVILIB*/

#include <gpac/base_coding.h>

GF_EXPORT
GF_Err gf_media_export_nhml(GF_MediaExporter *dumper, Bool dims_doc)
{
	GF_ESD *esd;
	char szName[1000], szMedia[1000];
	FILE *med, *inf, *nhml;
	Bool full_dump;
	u32 w, h;
	Bool uncompress;
	u32 track, i, di, count, pos, mstype;
	Bool is_stpp;
	const char *szRootName;

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
	full_dump = (dumper->flags & GF_EXPORT_NHML_FULL) ? 1 : 0;
	szRootName = "NHNTStream";
	med = NULL;
	szMedia[0]=0;

	if (dims_doc) {
		sprintf(szName, "%s.dml", dumper->out_name);
		szRootName = "DIMSStream";
	} else if (!dumper->nhml_only) {
		sprintf(szMedia, "%s.media", dumper->out_name);
		med = gf_fopen(szMedia, "wb");
		if (!med) {
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szMedia);
		}

		sprintf(szName, "%s.nhml", dumper->out_name);
	}
	if (dumper->dump_file) {
		nhml = dumper->dump_file;
	} else {
		nhml = gf_fopen(szName, "wt");
		if (!nhml) {
			gf_fclose(med);
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
		}
		fprintf(nhml, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	}

	mstype = gf_isom_get_mpeg4_subtype(dumper->file, track, 1);
	if (!mstype) mstype = gf_isom_get_media_subtype(dumper->file, track, 1);

	is_stpp = (mstype==GF_ISOM_SUBTYPE_STPP) ? GF_TRUE : GF_FALSE;

	gf_export_message(dumper, GF_OK, "Exporting NHML for track %s", gf_4cc_to_str(mstype) );

	/*write header*/
	fprintf(nhml, "<%s version=\"1.0\" timeScale=\"%d\" ", szRootName, gf_isom_get_media_timescale(dumper->file, track) );
	if (esd) {
		fprintf(nhml, "streamType=\"%d\" objectTypeIndication=\"%d\" ", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
		if (!dumper->nhml_only && esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->data) {
			sprintf(szName, "%s.info", dumper->out_name);
			inf = gf_fopen(szName, "wb");
			if (inf) gf_fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 1, inf);
			gf_fclose(inf);
			fprintf(nhml, "specificInfoFile=\"%s\" ", szName);
		}
		gf_odf_desc_del((GF_Descriptor *) esd);

		if (gf_isom_get_media_type(dumper->file, track)==GF_ISOM_MEDIA_VISUAL) {
			gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
			fprintf(nhml, "width=\"%d\" height=\"%d\" ", w, h);
		}
		else if (gf_isom_get_media_type(dumper->file, track)==GF_ISOM_MEDIA_AUDIO) {
			u32 sr, nb_ch;
			u8 bps;
			gf_isom_get_audio_info(dumper->file, track, 1, &sr, &nb_ch, &bps);
			fprintf(nhml, "sampleRate=\"%d\" numChannels=\"%d\" ", sr, nb_ch);
		}
	} else if (!dims_doc) {
		GF_GenericSampleDescription *sdesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
		u32 mtype = gf_isom_get_media_type(dumper->file, track);
		fprintf(nhml, "mediaType=\"%s\" ", gf_4cc_to_str(mtype));
		fprintf(nhml, "mediaSubType=\"%s\" ", gf_4cc_to_str(mstype ));
		if (sdesc) {
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
				fprintf(nhml, "codecVendor=\"%s\" codecVersion=\"%d\" codecRevision=\"%d\" ", gf_4cc_to_str(sdesc->vendor_code), sdesc->version, sdesc->revision);
				fprintf(nhml, "width=\"%d\" height=\"%d\" compressorName=\"%s\" temporalQuality=\"%d\" spatialQuality=\"%d\" horizontalResolution=\"%d\" verticalResolution=\"%d\" bitDepth=\"%d\" ",
				        sdesc->width, sdesc->height, sdesc->compressor_name, sdesc->temporal_quality, sdesc->spatial_quality, sdesc->h_res, sdesc->v_res, sdesc->depth);
			} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
				fprintf(nhml, "codecVendor=\"%s\" codecVersion=\"%d\" codecRevision=\"%d\" ", gf_4cc_to_str(sdesc->vendor_code), sdesc->version, sdesc->revision);
				fprintf(nhml, "sampleRate=\"%d\" numChannels=\"%d\" bitsPerSample=\"%d\" ", sdesc->samplerate, sdesc->nb_channels, sdesc->bits_per_sample);
			}
			if (sdesc->extension_buf) {
				sprintf(szName, "%s.info", dumper->out_name);
				inf = gf_fopen(szName, "wb");
				if (inf) gf_fwrite(sdesc->extension_buf, sdesc->extension_buf_size, 1, inf);
				gf_fclose(inf);
				fprintf(nhml, "specificInfoFile=\"%s\" ", szName);
				gf_free(sdesc->extension_buf);
			}
			gf_free(sdesc);
		} else {
			const char *mime, *encoding, *config, *namespace, *location;
			switch (mstype) {
			case GF_ISOM_SUBTYPE_METT:
				if (gf_isom_stxt_get_description(dumper->file, track, 1, &mime, &encoding, &config) == GF_OK) {
					if (mime)
						fprintf(nhml, "mime_type=\"%s\" ", mime);
					if (config) {
						sprintf(szName, "%s.info", dumper->out_name);
						inf = gf_fopen(szName, "wb");
						if (inf) gf_fwrite(config, strlen(config), 1, inf);
						gf_fclose(inf);
						fprintf(nhml, "specificInfoFile=\"%s\" ", szName);
					}
					if (encoding)
						fprintf(nhml, "encoding=\"%s\" ", encoding);
				}
				break;
			case GF_ISOM_SUBTYPE_METX:
			case GF_ISOM_SUBTYPE_STPP:
				if (gf_isom_xml_subtitle_get_description(dumper->file, track, 1, &namespace, &location, &mime) == GF_OK) {
					if (mime)
						fprintf(nhml, "mime_type=\"%s\" ", mime);
					if (namespace)
						fprintf(nhml, "xml_namespace=\"%s\" ", namespace);
					if (location)
						fprintf(nhml, "xml_schema_location=\"%s\" ", location);
				}
				break;
			}
		}
	}

	if (gf_isom_is_track_in_root_od(dumper->file, track)) fprintf(nhml, "inRootOD=\"yes\" ");
	fprintf(nhml, "trackID=\"%d\" ", dumper->trackID);

	uncompress = 0;

	if (mstype == GF_ISOM_MEDIA_DIMS) {
		GF_DIMSDescription dims;

		fprintf(nhml, "xmlns=\"http://www.3gpp.org/richmedia\" ");
		gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
		fprintf(nhml, "width=\"%d\" height=\"%d\" ", w, h);

		gf_isom_get_dims_description(dumper->file, track, 1, &dims);
		fprintf(nhml, "profile=\"%d\" level=\"%d\" pathComponents=\"%d\" ", dims.profile, dims.level, dims.pathComponents);
		fprintf(nhml, "useFullRequestHost=\"%s\" stream_type=\"%s\" ", dims.fullRequestHost ? "yes" : "no", dims.streamType ? "primary" : "secondary");
		fprintf(nhml, "contains_redundant=\"%s\" ", (dims.containsRedundant==1) ? "main" : (dims.containsRedundant==2) ? "redundant" : "main+redundant");
		if (strlen(dims.textEncoding) ) fprintf(nhml, "text_encoding=\"%s\" ", dims.textEncoding);
		if (strlen(dims.contentEncoding) ) {
			fprintf(nhml, "content_encoding=\"%s\" ", dims.contentEncoding);
			if (!strcmp(dims.contentEncoding, "deflate")) uncompress = 1;
		}
		if (dims.content_script_types) fprintf(nhml, "content_script_types=\"%s\" ", dims.content_script_types);
	} else if (szMedia[0]) {
		fprintf(nhml, "baseMediaFile=\"%s\" ", gf_file_basename(szMedia));
	}


	fprintf(nhml, ">\n");


	pos = 0;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		if (med)
			gf_fwrite(samp->data, samp->dataLength, 1, med);

		if (dims_doc) {
			GF_BitStream *bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);

			while (gf_bs_available(bs)) {
				u64 pos = gf_bs_get_position(bs);
				u16 size = gf_bs_read_u16(bs);
				u8 flags = gf_bs_read_u8(bs);
				u8 prev;

				if (pos+size+2>samp->dataLength)
					break;

				prev = samp->data[pos+2+size];
				samp->data[pos+2+size] = 0;


				fprintf(nhml, "<DIMSUnit time=\""LLU"\"", LLU_CAST samp->DTS);
				/*DIMS flags*/
				if (flags & GF_DIMS_UNIT_S) fprintf(nhml, " is-Scene=\"yes\"");
				if (flags & GF_DIMS_UNIT_M) fprintf(nhml, " is-RAP=\"yes\"");
				if (flags & GF_DIMS_UNIT_I) fprintf(nhml, " is-redundant=\"yes\"");
				if (flags & GF_DIMS_UNIT_D) fprintf(nhml, " redundant-exit=\"yes\"");
				if (flags & GF_DIMS_UNIT_P) fprintf(nhml, " priority=\"high\"");
				if (flags & GF_DIMS_UNIT_C) fprintf(nhml, " compressed=\"yes\"");
				fprintf(nhml, ">");
				if (uncompress && (flags & GF_DIMS_UNIT_C)) {
#ifndef GPAC_DISABLE_ZLIB
					char svg_data[2049];
					int err;
					u32 done = 0;
					z_stream d_stream;
					d_stream.zalloc = (alloc_func)0;
					d_stream.zfree = (free_func)0;
					d_stream.opaque = (voidpf)0;
					d_stream.next_in  = (Bytef*)samp->data+pos+3;
					d_stream.avail_in = size-1;
					d_stream.next_out = (Bytef*)svg_data;
					d_stream.avail_out = 2048;

					err = inflateInit(&d_stream);
					if (err == Z_OK) {
						while ((s32) d_stream.total_in < size-1) {
							err = inflate(&d_stream, Z_NO_FLUSH);
							if (err < Z_OK) break;
							svg_data[d_stream.total_out - done] = 0;
							fprintf(nhml, "%s", svg_data);
							if (err== Z_STREAM_END) break;
							done = (u32) d_stream.total_out;
							d_stream.avail_out = 2048;
							d_stream.next_out = (Bytef*)svg_data;
						}
						inflateEnd(&d_stream);
					}
#else
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Error: your version of GPAC was compile with no libz support."));
					gf_bs_del(bs);
					gf_isom_sample_del(&samp);
					if (med) gf_fclose(med);
					gf_fclose(nhml);
					return GF_NOT_SUPPORTED;
#endif
				} else {
					gf_fwrite(samp->data+pos+3, size-1, 1, nhml);
				}
				fprintf(nhml, "</DIMSUnit>\n");

				samp->data[pos+2+size] = prev;
				gf_bs_skip_bytes(bs, size-1);
			}
			gf_bs_del(bs);

		} else {
			fprintf(nhml, "<NHNTSample DTS=\""LLU"\" dataLength=\"%d\" ", LLU_CAST samp->DTS, samp->dataLength);
			if (full_dump || samp->CTS_Offset) fprintf(nhml, "CTSOffset=\"%u\" ", samp->CTS_Offset);
			if (samp->IsRAP==RAP) fprintf(nhml, "isRAP=\"yes\" ");
			else if (samp->IsRAP==RAP_REDUNDANT) fprintf(nhml, "isSyncShadow=\"yes\" ");
			else if (full_dump) fprintf(nhml, "isRAP=\"no\" ");
			if (full_dump) fprintf(nhml, "mediaOffset=\"%d\" ", pos);

			fprintf(nhml, ">\n");
			if (is_stpp && dumper->nhml_only) {
				u32 n, k, scount = gf_isom_sample_has_subsamples(dumper->file, track, i+1, 0);
				u32 offset=0;
				if (scount) {
					for (k=0; k<scount;k++) {
						u32 ssize;
						char last;
						gf_isom_sample_get_subsample(dumper->file, track, i+1, 0, k+1, &ssize, NULL, NULL, NULL);
						if (offset+ssize>samp->dataLength) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong subsample info for sample %d on track %d: sample size %d vs subsample offset+size %dn", i+1, dumper->trackID, samp->dataLength, offset+ssize));
							break;
						}
						last = samp->data[offset+ssize];
						samp->data[offset+ssize]=0;

						if (!k) {
							fprintf(nhml, "<NHNTSubSample>\n");
							for (n=0; n<samp->dataLength;n++) fputc(samp->data[n], nhml);
							fprintf(nhml, "</NHNTSubSample>\n");
						} else {
							char *buf = gf_malloc(sizeof(char)*2*samp->dataLength);
							u32 size = gf_base64_encode(samp->data, samp->dataLength, buf, 2*samp->dataLength);
							buf[size] = 0;
							fprintf(nhml, "<NHNTSubSample data=\"data:application/octet-string;base64,%s\">\n", buf);
							gf_free(buf);
						}
						samp->data[offset+ssize]=last;
						offset += ssize;
					}
				} else {
					fprintf(nhml, "<NHNTSubSample><![CDATA[\n");
					for (n=0; n<samp->dataLength;n++) fputc(samp->data[n], nhml);
					fprintf(nhml, "]]></NHNTSubSample>\n");
				}
			}
			fprintf(nhml, "</NHNTSample>\n");
		}

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		gf_set_progress("NHML Export", i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	fprintf(nhml, "</%s>\n", szRootName);
	if (med) gf_fclose(med);
	if (!dumper->dump_file) {
		gf_fclose(nhml);
	}
	return GF_OK;
}

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
	fprintf(vtt, "WEBVTT Metadata track generated by GPAC MP4Box %s\n", GPAC_FULL_VERSION);
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
		if (mtype==GF_ISOM_MEDIA_VISUAL) {
			gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
			fprintf(vtt, "width:%d\n", w);
			fprintf(vtt, "height:%d\n", h);
		}
		else if (mtype==GF_ISOM_MEDIA_AUDIO) {
			u32 sr, nb_ch;
			u8 bps;
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
			if (mtype==GF_ISOM_MEDIA_VISUAL) {
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
	char *data;
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

#ifndef GPAC_DISABLE_MPEG2TS

void m2ts_export_check(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
//	if (evt_type == GF_M2TS_EVT_PAT_REPEAT) ts->user = NULL;
	if (evt_type == GF_M2TS_EVT_PMT_REPEAT) ts->user = NULL;
	else if (evt_type == GF_M2TS_EVT_PAT_FOUND) gf_m2ts_abort_parsing(ts, GF_FALSE);
}

struct _ts_export
{
	FILE *dst;
	Bool is_latm;
};

void m2ts_export_dump(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	struct _ts_export *tsx = (struct _ts_export *) ts->user;

	if (evt_type == GF_M2TS_EVT_PES_PCK) {
		GF_M2TS_PES_PCK *pck = (GF_M2TS_PES_PCK *)par;

		if (tsx->is_latm) {
			GF_BitStream *bs = gf_bs_from_file(tsx->dst, GF_BITSTREAM_WRITE);

			gf_bs_write_int(bs, 0xFFF, 12);/*sync*/
			gf_bs_write_int(bs, 0, 1);/*mpeg2 aac*/
			gf_bs_write_int(bs, 0, 2); /*layer*/
			gf_bs_write_int(bs, 1, 1); /* protection_absent*/

			gf_bs_write_int(bs, pck->stream->aud_aac_obj_type - 1 , 2);

			gf_bs_write_int(bs, pck->stream->aud_aac_sr_idx, 4);
			gf_bs_write_int(bs, 0, 1);
			gf_bs_write_int(bs, pck->stream->aud_nb_ch, 3);
			gf_bs_write_int(bs, 0, 4);
			gf_bs_write_int(bs, 7+pck->data_len, 13);
			gf_bs_write_int(bs, 0x7FF, 11);
			gf_bs_write_int(bs, 0, 2);

			gf_bs_del(bs);
		}
		gf_fwrite(pck->data, pck->data_len, 1, tsx->dst);
	}
	else if (evt_type == GF_M2TS_EVT_SL_PCK) {
		GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)par;
		gf_fwrite(pck->data + 5, pck->data_len - 5, 1, tsx->dst);
	}
}

GF_Err gf_media_export_ts_native(GF_MediaExporter *dumper)
{
	char data[188], szFile[GF_MAX_PATH];
	GF_M2TS_PES *stream;
	u32 i;
	u64 size, fsize, fdone;
	Bool is_stdout=0;
	GF_M2TS_Demuxer *ts;
	FILE *src;
	struct _ts_export tsx;

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return GF_OK;

	src = gf_fopen(dumper->in_name, "rb");
	if (!src) return gf_export_message(dumper, GF_FILTER_NOT_FOUND, "Error opening %s", dumper->in_name);

	gf_fseek(src, 0, SEEK_END);
	fsize = gf_ftell(src);
	gf_fseek(src, 0, SEEK_SET);

	ts = gf_m2ts_demux_new();
	ts->on_event = m2ts_export_check;
	ts->user = dumper;
	/*get PAT*/
	while (!feof(src)) {
		size = fread(data, 1, 188, src);
		if (size<188) break;
		assert(size == 188);
		gf_m2ts_process_data(ts, data, (u32)size);
		if (!ts->user) break;
	}
	if (!ts->abort_parsing && ts->user) {
		gf_fclose(src);
		gf_m2ts_demux_del(ts);
		return gf_export_message(dumper, GF_URL_ERROR, "Cannot locate program association table");
	}
	ts->abort_parsing = GF_FALSE;

	stream = NULL;
	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		GF_M2TS_PES *pes = (GF_M2TS_PES *)ts->ess[i];
		if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
		if (pes->pid == dumper->trackID) {
			stream = pes;
			gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);
			break;
		} else {
			gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);
		}
	}
	if (!stream) {
		gf_fclose(src);
		gf_m2ts_demux_del(ts);
		return gf_export_message(dumper, GF_URL_ERROR, "Cannot find PID %d in transport stream", dumper->trackID);
	}

	tsx.is_latm = 0;
	sprintf(szFile, "%s_pid%d", dumper->out_name ? dumper->out_name : "", stream->pid);
	switch (stream->stream_type) {
	case GF_M2TS_VIDEO_MPEG1:
		strcat(szFile, ".m1v");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-1 Visual stream to m1v");
		break;
	case GF_M2TS_VIDEO_MPEG2:
		strcat(szFile, ".m2v");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-2 Visual stream to m1v");
		break;
	case GF_M2TS_AUDIO_MPEG1:
		strcat(szFile, ".mp3");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-1 Audio stream to mp3");
		break;
	case GF_M2TS_AUDIO_MPEG2:
		strcat(szFile, ".mp3");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-2 Audio stream to mp3");
		break;
	case GF_M2TS_AUDIO_AAC:
		strcat(szFile, ".aac");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-4 Audio stream to aac");
		break;
	case GF_M2TS_AUDIO_LATM_AAC:
		strcat(szFile, ".aac");
		tsx.is_latm = 1;
		gf_export_message(dumper, GF_OK, "Extracting MPEG-4 Audio LATM stream to aac");
		break;
	case GF_M2TS_VIDEO_MPEG4:
		strcat(szFile, ".cmp");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-4 Visual stream to cmp");
		break;
	case GF_M2TS_VIDEO_H264:
		strcat(szFile, ".264");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-4 AVC/H264 Visual stream to h264");
		break;
	case GF_M2TS_VIDEO_HEVC:
		strcat(szFile, ".hvc");
		gf_export_message(dumper, GF_OK, "Extracting MPEG-H HEVC Visual stream to hvc");
		break;
	case GF_M2TS_VIDEO_SVC:
		strcat(szFile, ".264");
		gf_export_message(dumper, GF_OK, "Extracting H264-SVC Visual stream to h264");
		break;
	case GF_M2TS_METADATA_ID3_HLS:
		strcat(szFile, ".txt");
		gf_export_message(dumper, GF_OK, "Extracting ID3 tags from metadata stream");
		break;
	default:
		strcat(szFile, ".raw");
		gf_export_message(dumper, GF_OK, "Extracting Unknown stream to raw");
		break;
	}

	if (dumper->out_name && !strcmp(dumper->out_name, "std"))
		is_stdout=1;

	tsx.dst = is_stdout ? stdout : gf_fopen(szFile, "wb");
	if (!tsx.dst) {
		gf_fclose(src);
		gf_m2ts_demux_del(ts);
		return gf_export_message(dumper, GF_IO_ERR, "Cannot open file %s for writing", szFile);
	}

	gf_m2ts_reset_parsers(ts);
	gf_fseek(src, 0, SEEK_SET);
	fdone = 0;
	ts->user = &tsx;
	ts->on_event = m2ts_export_dump;
	while (!feof(src)) {
		size = fread(data, 1, 188, src);
		if (size<188) break;
		assert(size == 188);
		gf_m2ts_process_data(ts, data, (u32)size);
		fdone += size;
		gf_set_progress("MPEG-2 TS Extract", fdone, fsize);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	gf_set_progress("MPEG-2 TS Extract", fsize, fsize);

	if (!is_stdout)
		gf_fclose(tsx.dst);
	gf_fclose(src);
	gf_m2ts_demux_del(ts);
	return GF_OK;
}

#endif /*GPAC_DISABLE_MPEG2TS*/

GF_EXPORT
GF_Err gf_media_export(GF_MediaExporter *dumper)
{
	if (!dumper) return GF_BAD_PARAM;
	if (!dumper->out_name && !(dumper->flags & GF_EXPORT_PROBE_ONLY)) return GF_BAD_PARAM;

	if (dumper->flags & GF_EXPORT_NATIVE) {
#ifndef GPAC_DISABLE_MPEG2TS
		if (dumper->in_name) {
			char *ext = strrchr(dumper->in_name, '.');
			if (ext && (!strnicmp(ext, ".ts", 3) || !strnicmp(ext, ".m2t", 4)) ) {
				return gf_media_export_ts_native(dumper);
			}
		}
#endif /*GPAC_DISABLE_MPEG2TS*/
		return gf_media_export_native(dumper);
	}
	else if (dumper->flags & GF_EXPORT_RAW_SAMPLES)
		return gf_media_export_native(dumper);

	else if (dumper->flags & GF_EXPORT_NHNT) return gf_media_export_nhnt(dumper);
#ifndef GPAC_DISABLE_ISOM_WRITE
	else if (dumper->flags & GF_EXPORT_MP4) return gf_media_export_isom(dumper);
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_AVILIB
	else if (dumper->flags & GF_EXPORT_AVI) return gf_media_export_avi(dumper);
	else if (dumper->flags & GF_EXPORT_AVI_NATIVE) return gf_media_export_avi_track(dumper);
#endif /*GPAC_DISABLE_AVILIB*/
	else if (dumper->flags & GF_EXPORT_NHML) return gf_media_export_nhml(dumper, 0);
	else if (dumper->flags & GF_EXPORT_SAF) return gf_media_export_saf(dumper);
#ifndef GPAC_DISABLE_VTT
	else if (dumper->flags & GF_EXPORT_WEBVTT_META) return gf_media_export_webvtt_metadata(dumper);
#endif
	else if (dumper->flags & GF_EXPORT_SIX) return gf_media_export_six(dumper);
	else return GF_NOT_SUPPORTED;
}

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/
