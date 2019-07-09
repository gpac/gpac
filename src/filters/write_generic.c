/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / generic stream to file filter
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
#include <gpac/bitstream.h>

#include <gpac/internal/isomedia_dev.h>

enum
{
	DECINFO_NO=0,
	DECINFO_FIRST,
	DECINFO_SAP,
	DECINFO_AUTO
};

typedef struct
{
	//opts
	Bool exporter, frame, split;
	u32 sstart, send;
	u32 pfmt, afmt, decinfo;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid;
	Bool is_mj2k;
	u32 sample_num;

	const char *dcfg;
	u32 dcfg_size;
	Bool cfg_sent;

	GF_Fraction duration;
	Bool first;

	GF_BitStream *bs;

	u32 target_pfmt, target_afmt;
	Bool is_bmp;
	u32 is_wav;
	u32 w, h, stride;
	u64 nb_bytes;

} GF_GenDumpCtx;


GF_Err writegen_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cid, chan, sr, w, h, stype, pf, sfmt, av1mode, nb_bps;
	const char *name, *mimetype;
	char szExt[10], szCodecExt[30], *sep;
	const GF_PropertyValue *p;
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	cid = p->value.uint;

	ctx->codecid = cid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		ctx->first = GF_TRUE;
	}

	ctx->ipid = pid;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	stype = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	sr = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	chan = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
	sfmt = p ? p->value.uint : GF_AUDIO_FMT_S16;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_BPS);
	nb_bps = p ? p->value.uint : 0;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	ctx->w = w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	ctx->h = h = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	pf = p ? p->value.uint : 0;
	if (!pf) pf = GF_PIXEL_YUV;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		ctx->dcfg = p->value.data.ptr;
		ctx->dcfg_size = p->value.data.size;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );

	//special case for xml text, override to xml
	switch (cid) {
	case GF_CODECID_META_XML:
	case GF_CODECID_SUBS_XML:
		strcpy(szCodecExt, "xml");
		break;
	default:
		strncpy(szCodecExt, gf_codecid_file_ext(cid), 29);
		szCodecExt[29]=0;
		sep = strchr(szCodecExt, '|');
		if (sep) sep[0] = 0;
		break;
	}
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING(szCodecExt) );

	mimetype = gf_codecid_mime(cid);

	switch (cid) {
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED_LATM);
		if (p && p->value.boolean) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("latm") );
		}
		break;
	case GF_CODECID_PNG:
	case GF_CODECID_JPEG:
		ctx->split = GF_TRUE;
		break;
	case GF_CODECID_J2K:
		ctx->split = GF_TRUE;
		ctx->is_mj2k = GF_TRUE;
		break;
	case GF_CODECID_AMR:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		ctx->dcfg = "#!AMR\n";
		ctx->dcfg_size = 6;
		ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_AMR_WB:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		ctx->dcfg = "#!AMR-WB\n";
		ctx->dcfg_size = 9;
		ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_SMV:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		ctx->dcfg = "#!SMV\n";
		ctx->dcfg_size = 6;
		ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_EVRC_PV:
	case GF_CODECID_EVRC:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		ctx->dcfg = "#!EVRC\n";
		ctx->dcfg_size = 7;
		ctx->decinfo = DECINFO_FIRST;
		break;

	case GF_CODECID_FLAC:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		ctx->decinfo = DECINFO_FIRST;
		break;


	case GF_CODECID_SIMPLE_TEXT:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		if (ctx->decinfo == DECINFO_AUTO)
			ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_META_TEXT:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		if (ctx->decinfo == DECINFO_AUTO)
			ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_META_XML:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		if (ctx->decinfo == DECINFO_AUTO)
			ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_SUBS_TEXT:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		if (ctx->decinfo == DECINFO_AUTO)
			ctx->decinfo = DECINFO_FIRST;
		break;
	case GF_CODECID_SUBS_XML:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );

		if (!ctx->frame) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Subtitles re-assembling is not supported yet\n"));
			return GF_NOT_SUPPORTED;
		}
		ctx->split = GF_TRUE;
		if (ctx->decinfo == DECINFO_AUTO)
			ctx->decinfo = DECINFO_FIRST;
		break;

	case GF_CODECID_AV1:
		av1mode = 0;
		p = gf_filter_pid_get_property_str(ctx->ipid, "obu:mode");
		if (p) av1mode = p->value.uint;
		if (av1mode==1) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("av1b") );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/x-av1b") );
		} else if (av1mode==2) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("ivf") );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/x-ivf") );
		} else {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("obu") );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/x-av1") );
		}
		break;
	case GF_CODECID_RAW:
		ctx->dcfg = NULL;
		ctx->dcfg_size = 0;
		if (stype==GF_STREAM_VISUAL) {
			strcpy(szExt, gf_pixel_fmt_sname(ctx->target_pfmt ? ctx->target_pfmt : pf));
			p = gf_filter_pid_caps_query(ctx->opid, GF_PROP_PID_FILE_EXT);
			if (p) {
				strncpy(szExt, p->value.string, 10);
				if (!strcmp(szExt, "bmp")) {
					ctx->is_bmp = GF_TRUE;
					//request BGR
					ctx->target_pfmt = GF_PIXEL_BGR;
					ctx->split = GF_TRUE;
				} else {
					ctx->target_pfmt = gf_pixel_fmt_parse(szExt);
					if (!ctx->target_pfmt) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Cannot guess pixel format from extension type %s\n", szExt));
						return GF_NOT_SUPPORTED;
					}
					strcpy(szExt, gf_pixel_fmt_sname(ctx->target_pfmt));
				}
				//forcing pixel format regardless of extension
				if (ctx->pfmt) {
					if (pf != ctx->pfmt) {
						gf_filter_pid_negociate_property(ctx->ipid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pfmt));
						//make sure we reconfigure
						ctx->codecid = 0;
						pf = ctx->pfmt;
					}
				}
				//use extension to derive pixel format
				else if (pf != ctx->target_pfmt) {
					gf_filter_pid_negociate_property(ctx->ipid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->target_pfmt));
					strcpy(szExt, gf_pixel_fmt_sname(ctx->target_pfmt));
					//make sure we reconfigure
					ctx->codecid = 0;
				}
			}

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
			ctx->stride = p ? p->value.uint : 0;

			if (!ctx->stride) {
				gf_pixel_get_size_info(ctx->target_pfmt, ctx->w, ctx->h, NULL, &ctx->stride, NULL, NULL, NULL);
			}

		} else if (stype==GF_STREAM_AUDIO) {
			strcpy(szExt, gf_audio_fmt_sname(ctx->target_pfmt ? ctx->target_afmt : sfmt));
			p = gf_filter_pid_caps_query(ctx->opid, GF_PROP_PID_FILE_EXT);
			if (p) {
				strncpy(szExt, p->value.string, 10);
				if (!strcmp(szExt, "wav")) {
					ctx->is_wav = GF_TRUE;
					//request PCMs16 ?
//					ctx->target_afmt = GF_AUDIO_FMT_S16;
					ctx->target_afmt = sfmt;
				} else {
					ctx->target_afmt = gf_audio_fmt_parse(szExt);
					strcpy(szExt, gf_audio_fmt_sname(ctx->target_afmt));
				}
				//forcing sample format regardless of extension
				if (ctx->afmt) {
					if (sfmt != ctx->afmt) {
						gf_filter_pid_negociate_property(ctx->ipid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->afmt));
						//make sure we reconfigure
						ctx->codecid = 0;
						sfmt = ctx->afmt;
					}
				}
				//use extension to derive sample format
				else if (sfmt != ctx->target_afmt) {
					gf_filter_pid_negociate_property(ctx->ipid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->target_afmt));
					strcpy(szExt, gf_audio_fmt_sname(ctx->target_afmt));
					//make sure we reconfigure
					ctx->codecid = 0;
				}
			}

		} else {
			strcpy(szExt, gf_4cc_to_str(cid));
		}
		if (ctx->is_bmp) strcpy(szExt, "bmp");
		else if (ctx->is_wav) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_BOOL(GF_TRUE) );
			strcpy(szExt, "wav");
		} else if (!strlen(szExt)) strcpy(szExt, "raw");

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING( szExt ) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/octet-string") );
		if (!ctx->codecid) return GF_OK;
		break;

	default:
		if (!strcmp(szCodecExt, "raw")) {
			strcpy(szExt, gf_4cc_to_str(cid));
			if (!strlen(szExt)) strcpy(szExt, "raw");
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING( szExt ) );
		} else {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING( szCodecExt ) );
		}
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		break;
	}

	if (ctx->decinfo == DECINFO_AUTO)
		ctx->decinfo = DECINFO_NO;

	name = gf_codecid_name(cid);
	if (ctx->exporter) {
		if (w && h) {
			if (cid==GF_CODECID_RAW) name = gf_pixel_fmt_name(pf);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - Size %dx%d\n", name, w, h));
		} else if (sr && chan) {
			if (cid==GF_CODECID_RAW) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting PCM %s SampleRate %d %d channels %d bits per sample\n", gf_audio_fmt_name(sfmt), sr, chan, gf_audio_fmt_bit_depth(sfmt) ));
			} else {
				if (!nb_bps)
					nb_bps = gf_audio_fmt_bit_depth(sfmt);

				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - SampleRate %d %d channels %d bits per sample\n", name, sr, chan, nb_bps ));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s\n", name));
		}
	}

	//avoid creating a file when dumping individual samples
	if (ctx->split) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
		if (!p || (p->value.uint>1))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PCK_FILENUM, &PROP_UINT(0) );
		else
			ctx->split = GF_FALSE;
	} else if (ctx->frame) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PCK_FILENUM, &PROP_UINT(0) );
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) ctx->duration = p->value.frac;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static GF_FilterPacket *writegen_write_j2k(GF_GenDumpCtx *ctx, char *data, u32 data_size, GF_FilterPacket *in_pck)
{
	u32 size;
	u8 *output;
	GF_FilterPacket *dst_pck;
	char sig[8];
	sig[0] = sig[1] = sig[2] = 0;
	sig[3] = 0xC;
	sig[4] = 'j';
	sig[5] = 'P';
	sig[6] = sig[7] = ' ';

	if ((data_size>16) && !memcmp(data, sig, 8)) {
		return gf_filter_pck_new_ref(ctx->opid, NULL, 0, in_pck);
	}

	size = data_size + 8*4 /*jP%20%20 + ftyp*/ + ctx->dcfg_size + 8;

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);


	if (!ctx->bs) ctx->bs = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs, output, size);

	gf_bs_write_u32(ctx->bs, 12);
	gf_bs_write_u32(ctx->bs, GF_ISOM_BOX_TYPE_JP);
	gf_bs_write_u32(ctx->bs, 0x0D0A870A);
	gf_bs_write_u32(ctx->bs, 20);
	gf_bs_write_u32(ctx->bs, GF_ISOM_BOX_TYPE_FTYP);
	gf_bs_write_u32(ctx->bs, GF_ISOM_BRAND_JP2);
	gf_bs_write_u32(ctx->bs, 0);
	gf_bs_write_u32(ctx->bs, GF_ISOM_BRAND_JP2);

	//todo, write colr and other info ?
	if (ctx->dcfg) {
		gf_bs_write_u32(ctx->bs, 8+ctx->dcfg_size);
		gf_bs_write_u32(ctx->bs, GF_ISOM_BOX_TYPE_JP2H);
		gf_bs_write_data(ctx->bs, ctx->dcfg, ctx->dcfg_size);
	}
	gf_bs_write_data(ctx->bs, data, data_size);

	return dst_pck;
}

#ifdef WIN32
#include <windows.h>
#else
typedef struct tagBITMAPFILEHEADER
{
	u16	bfType;
	u32	bfSize;
	u16	bfReserved1;
	u16	bfReserved2;
	u32 bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
	u32	biSize;
	s32	biWidth;
	s32	biHeight;
	u16	biPlanes;
	u16	biBitCount;
	u32	biCompression;
	u32	biSizeImage;
	s32	biXPelsPerMeter;
	s32	biYPelsPerMeter;
	u32	biClrUsed;
	u32	biClrImportant;
} BITMAPINFOHEADER;

#define BI_RGB        0L

#endif

static GF_FilterPacket *writegen_write_bmp(GF_GenDumpCtx *ctx, char *data, u32 data_size)
{
	u32 size;
	u8 *output;
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;
	GF_FilterPacket *dst_pck;
	u32 i;
	char *ptr;

	size = ctx->w*ctx->h*3 + 54; //14 + 40 = size of BMP file header and BMP file info;
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

	memset(&fh, 0, sizeof(fh));
	fh.bfType = 19778;
	fh.bfOffBits = 54;

	if (!ctx->bs) ctx->bs = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs, output, size);

	gf_bs_write_data(ctx->bs, (const char *) &fh.bfType, 2);
	gf_bs_write_data(ctx->bs, (const char *) &fh.bfSize, 4);
	gf_bs_write_data(ctx->bs, (const char *) &fh.bfReserved1, 2);
	gf_bs_write_data(ctx->bs, (const char *) &fh.bfReserved2, 2);
	gf_bs_write_data(ctx->bs, (const char *) &fh.bfOffBits, 4);

	memset(&fi, 0, sizeof(char)*40);
	fi.biSize = 40;
	fi.biWidth = ctx->w;
	fi.biHeight = ctx->h;
	fi.biPlanes = 1;
	fi.biBitCount = 24;
	fi.biCompression = BI_RGB;
	fi.biSizeImage = ctx->w * ctx->h * 3;

	memcpy(output+14, &fi, sizeof(char)*40);
	//reverse lines
	output += sizeof(char) * 54;
	for (i=ctx->h; i>0; i--) {
		ptr = data + (i-1)*ctx->stride;
		memcpy(output, ptr, sizeof(char)*3*ctx->w);
		output += 3*ctx->w;
	}
	return dst_pck;
}

static void writegen_write_wav_header(GF_GenDumpCtx *ctx)
{
	u32 size;
	u8 *output;
	GF_FilterPacket *dst_pck;
	const GF_PropertyValue *p;
	u32 nb_ch, sample_rate, afmt, bps;
	const char chunkID[] = {'R', 'I', 'F', 'F'};
	const char format[] = {'W', 'A', 'V', 'E'};
	const char subchunk1ID[] = {'f', 'm', 't', ' '};
	const char subchunk2ID[] = {'d', 'a', 't', 'a'};

	if (ctx->is_wav!=1) return;
	ctx->is_wav = 2;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_SAMPLE_RATE);
	if (!p) return;
	sample_rate = p->value.uint;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_NUM_CHANNELS);
	if (!p) return;
	nb_ch = p->value.uint;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_AUDIO_FORMAT);
	if (!p) return;
	afmt = p->value.uint;

	bps = gf_audio_fmt_bit_depth(afmt);

	size = 44;
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

	if (!ctx->bs) ctx->bs = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs, output, size);

	gf_bs_write_data(ctx->bs, chunkID, 4);
	gf_bs_write_u32_le(ctx->bs, (u32) ctx->nb_bytes + 36);
	gf_bs_write_data(ctx->bs, format, 4);

	gf_bs_write_data(ctx->bs, subchunk1ID, 4);
	gf_bs_write_u32_le(ctx->bs, 16); //subchunk1 size
	gf_bs_write_u16_le(ctx->bs, 1); //audio format
	gf_bs_write_u16_le(ctx->bs, nb_ch);
	gf_bs_write_u32_le(ctx->bs, sample_rate);
	gf_bs_write_u32_le(ctx->bs, sample_rate * nb_ch * bps / 8); //byte rate
	gf_bs_write_u16_le(ctx->bs, nb_ch * bps / 8); // block align
	gf_bs_write_u16_le(ctx->bs, bps);

	gf_bs_write_data(ctx->bs, subchunk2ID, 4);
	gf_bs_write_u32_le(ctx->bs, (u32) ctx->nb_bytes);

	gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
	gf_filter_pck_set_byte_offset(dst_pck, 0);
	gf_filter_pck_set_seek_flag(dst_pck, 1);
	gf_filter_pck_send(dst_pck);
}

GF_Err writegen_process(GF_Filter *filter)
{
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	char *data;
	u32 pck_size;
	Bool split = ctx->split;
	if (!ctx->ipid) return GF_EOS;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (ctx->is_wav) writegen_write_wav_header(ctx);
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	ctx->sample_num++;

	if (ctx->sstart) {
		if (ctx->sstart > ctx->sample_num) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		if ((ctx->sstart <= ctx->send) && (ctx->sample_num>ctx->send) ) {
			GF_FilterEvent evt;
			gf_filter_pid_drop_packet(ctx->ipid);
			GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->ipid);
			gf_filter_pid_send_event(ctx->ipid, &evt);
			return GF_OK;
		}
	}

	if (ctx->frame) {
		split = GF_TRUE;
	} else if (ctx->dcfg_size && gf_filter_pck_get_sap(pck) && !ctx->is_mj2k && (ctx->decinfo!=DECINFO_NO) && !ctx->cfg_sent) {
		dst_pck = gf_filter_pck_new_shared(ctx->opid, ctx->dcfg, ctx->dcfg_size, NULL);
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;
		gf_filter_pck_set_readonly(dst_pck);
		gf_filter_pck_send(dst_pck);
		if ((ctx->decinfo==DECINFO_FIRST) && !ctx->split) {
			ctx->dcfg_size = 0;
			ctx->dcfg = NULL;
		}
		ctx->cfg_sent = GF_TRUE;
		return GF_OK;
	}
	ctx->cfg_sent = GF_FALSE;
	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	if (ctx->is_mj2k) {
		dst_pck = writegen_write_j2k(ctx, data, pck_size, pck);
	} else if (ctx->is_bmp) {
		dst_pck = writegen_write_bmp(ctx, data, pck_size);
	} else if (ctx->is_wav && ctx->first) {
		u8 * output;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, 44, &output);
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
		gf_filter_pck_set_corrupted(dst_pck, GF_TRUE);
		ctx->first = GF_FALSE;
		gf_filter_pck_send(dst_pck);
		ctx->nb_bytes += 44;
		return GF_OK;
	} else {
		dst_pck = gf_filter_pck_new_ref(ctx->opid, NULL, 0, pck);
	}
	gf_filter_pck_merge_properties(pck, dst_pck);
	//don't keep byte offset
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	if (split) {
		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->sample_num) );
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
	} else {
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;
	}

	gf_filter_pck_send(dst_pck);
	gf_filter_pck_set_seek_flag(dst_pck, 0);
	ctx->nb_bytes += pck_size;

	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts = gf_filter_pck_get_cts(pck);
		ts += gf_filter_pck_get_duration(pck);
		ts *= ctx->duration.den;
		ts /= timescale;
		gf_set_progress("Exporting", ts, ctx->duration.num);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

/*
	We list capabilities per codec type, associating the codec type to the file extension. This allows,
	when matching to an output filter accepting a given file extension to match the codec type in write_gen caps, and
	setup the right filter chain
*/
static GF_FilterCapability GenDumpCaps[] =
{
	//raw color dump YUV and RGB - keep it as first for field extension assignment
	//cf below
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "gpac"),
	{0},
	//raw audio dump - keep it as second for field extension assignment
	//cf below
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "gpac" ),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "bmp"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "wav"),
	{0},

	//we accept unframed AVC (annex B format)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC_PS),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "264|h264|avc|svc|mvc"),
	{0},

	//we accept unframed HEVC (annex B format)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "265|h265|hvc|hevc|shvc|lhvc"),
	{0},

	//we accept unframed OBU (without size field)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "obu|av1|av1b|ivf"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "cmp|m4ve"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_H263),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_S263),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "h263|263"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG1),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "m1v"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_422),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SNR),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_HIGH),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SIMPLE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SPATIAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "m2v"),
	{0},

	//we accept unframed AAC + ADTS
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,GF_PROP_PID_UNFRAMED_LATM, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "aac"),
	{0},

	//we accept unframed AAC + LATM
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED_LATM, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "latm"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_PNG),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "png"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_JPEG),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "jpg|jpeg"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_J2K),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "jp2|j2k"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mp3|mp1|mp2"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AMR),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AMR_WB),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "amr"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SMV),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "smv"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EVRC_PV),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EVRC),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "evc"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "ac3"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EAC3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "eac3"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_META_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_TEXT),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "txt"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_META_XML),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "txt"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_QCELP),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "qcelp"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "theo"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VORBIS),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "vorb"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SPEEX),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "spx"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "flac"),
	{0},

	//anything else: only for explicit dump (filter loaded on purpose), otherwise don't load for filter link resolution
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "*"),
	//for the rest, we include everything, only specifies excluded ones from above and non-handled ones
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),

	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AVC_PS),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),

	//WebVTT needs rewrite
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	//systems streams not dumpable to file
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_BIFS),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_BIFS_V2),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_OD_V1),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_OD_V2),
	//this one need QCP
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_QCELP),

	//other not supported (yet)
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_VORBIS)
};


#define OFFS(_n)	#_n, offsetof(GF_GenDumpCtx, _n)
static GF_FilterArgs GenDumpArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pfmt), "pixel format for raw extract. If not set, derived from extension", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(afmt), "audio format for raw extract. If not set, derived from extension", GF_PROP_PCMFMT, "none", NULL, 0},
	{ OFFS(decinfo), "decoder config insert mode\n"
	"- no: never inserted\n"
	"- first: inserted on first packet\n"
	"- sap: inserted at each SAP\n"
	"- auto: selects between no and first based on media type", GF_PROP_UINT, "auto", "no|first|sap|auto", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(split), "force one file per decoded frame.", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(frame), "force single frame dump with no rewrite. In this mode, all codecids are supported", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(sstart), "start number of frame to dump. If 0, all samples are dumped", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(send), "end number of frame to dump. If start<end, all samples after start are dumped", GF_PROP_UINT, "0", NULL, 0},
	{0}
};

static GF_Err writegen_initialize(GF_Filter *filter);

void writegen_finalize(GF_Filter *filter)
{
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
}

GF_FilterRegister GenDumpRegister = {
	.name = "writegen",
	GF_FS_SET_DESCRIPTION("Stream to file")
	GF_FS_SET_HELP("Generic single stream to file converter, used when extracting/converting PIDs.\n"
	"The writegen filter should usually not be explicetly loaded without a source ID specified, since the filter would likely match any pid connection.")
	.private_size = sizeof(GF_GenDumpCtx),
	.args = GenDumpArgs,
	.initialize = writegen_initialize,
	.finalize = writegen_finalize,
	SETCAPS(GenDumpCaps),
	.configure_pid = writegen_configure_pid,
	.process = writegen_process
};

static const GF_FilterCapability FrameDumpCaps[] =
{
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE)
};

static GF_Err writegen_initialize(GF_Filter *filter)
{
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->frame) {
		return gf_filter_override_caps(filter, (const GF_FilterCapability *) FrameDumpCaps, sizeof(FrameDumpCaps) / sizeof(GF_FilterCapability));
	}
	return GF_OK;
}


const GF_FilterRegister *writegen_register(GF_FilterSession *session)
{

	//assign runtime caps on first load
	if (!strcmp(GenDumpCaps[3].val.value.string, "gpac")) {
		GenDumpCaps[3].val.value.string = (char *) gf_pixel_fmt_all_shortnames();
		GenDumpCaps[8].val.value.string = (char *) gf_audio_fmt_all_shortnames();
		GenDumpArgs[1].min_max_enum = gf_pixel_fmt_all_names();
		GenDumpArgs[2].min_max_enum = gf_audio_fmt_all_names();
	}

	return &GenDumpRegister;
}

