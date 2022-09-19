/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
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
#include <gpac/xml.h>
#include <gpac/base_coding.h>

#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_CORE_TOOLS

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
	Bool exporter, frame, split, merge_region;
	u32 sstart, send;
	u32 pfmt, afmt, decinfo;
	GF_Fraction dur;

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

	GF_Fraction64 duration;
	Bool first;

	GF_BitStream *bs;

	u32 target_pfmt, target_afmt;
	Bool is_bmp;
	u32 is_y4m;
	u32 is_wav;
	u32 w, h, stride;
	u64 nb_bytes;
	Bool dash_mode;
	Bool trunc_audio;
	Bool webvtt;

	u64 first_dts_plus_one;

	Bool ttml_agg;
	GF_XMLNode *ttml_root;

	GF_FilterPacket *ttml_dash_pck;

	Bool dump_srt;
	Bool need_ttxt_footer;
	u8 *write_buf;
	u32 write_alloc;
} GF_GenDumpCtx;


GF_Err writegen_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cid, chan, sr, w, h, stype, pf, sfmt, av1mode, nb_bps;
	const char *name, *mimetype;
	char szExt[GF_4CC_MSIZE], szCodecExt[30], *sep;
	const GF_PropertyValue *p;
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid)) {
		p = ctx->frame ? gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED) : NULL;
		if (p && p->value.boolean) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("Option frame specified as inherited and writegen loaded dynamically, input data will be unframed ! Use `writegen:frame -o dst` for raw frame dump\n", szExt));

		} else {
			return GF_NOT_SUPPORTED;
		}
	}

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

	//get DSI except for unframed streams
	Bool get_dsi = GF_FALSE;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED);
	if (!p ) p = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED_LATM);
	if (!p || !p->value.boolean) {
		get_dsi = GF_TRUE;
	}
	//we need this one for tx3g and webvtt
	if ((cid==GF_CODECID_TX3G) || (cid==GF_CODECID_WEBVTT))
		get_dsi = GF_TRUE;

	if (get_dsi) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (p) {
			ctx->dcfg = p->value.data.ptr;
			ctx->dcfg_size = p->value.data.size;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
	ctx->dash_mode = (p && p->value.uint) ? GF_TRUE : GF_FALSE;

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

	ctx->dump_srt = GF_FALSE;
	p = gf_filter_pid_caps_query(ctx->opid, GF_PROP_PID_FILE_EXT);
	if (p && p->value.string) {
		if (!strcmp(p->value.string, "srt")) {
			ctx->dump_srt = GF_TRUE;
			strcpy(szCodecExt, "srt");
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING(szCodecExt) );

	mimetype = gf_codecid_mime(cid);

	switch (cid) {
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		//override extension to latm for aac if unframed latm data - NOT for usac
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

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED);
		if (p && p->value.boolean)
			ctx->dump_srt = GF_TRUE;
		break;

	case GF_CODECID_TX3G:
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
	case GF_CODECID_WEBVTT:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );
		if (ctx->decinfo == DECINFO_AUTO)
			ctx->decinfo = DECINFO_FIRST;
		ctx->webvtt = GF_TRUE;
		break;
	case GF_CODECID_SUBS_XML:
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING(mimetype) );

		if (ctx->dash_mode) {
			ctx->ttml_agg = GF_TRUE;
		}
		else if (!ctx->frame && !ctx->split) {
			ctx->ttml_agg = GF_TRUE;
		} else {
			ctx->split = GF_TRUE;
		}
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
				strncpy(szExt, p->value.string, GF_4CC_MSIZE-1);
				szExt[GF_4CC_MSIZE-1] = 0;
				if (!strcmp(szExt, "bmp")) {
					ctx->is_bmp = GF_TRUE;
					//request BGR
					ctx->target_pfmt = GF_PIXEL_BGR;
					ctx->split = GF_TRUE;
				} else if (!strcmp(szExt, "y4m")) {
					ctx->is_y4m = GF_TRUE;
					ctx->target_pfmt = GF_PIXEL_YUV;
				} else {
					ctx->target_pfmt = gf_pixel_fmt_parse(szExt);
					if (!ctx->target_pfmt) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Cannot guess pixel format from extension type %s\n", szExt));
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
				gf_pixel_get_size_info(ctx->target_pfmt ? ctx->target_pfmt : pf, ctx->w, ctx->h, NULL, &ctx->stride, NULL, NULL, NULL);
			}

		} else if (stype==GF_STREAM_AUDIO) {
			strcpy(szExt, gf_audio_fmt_sname(ctx->target_afmt ? ctx->target_afmt : sfmt));
			p = gf_filter_pid_caps_query(ctx->opid, GF_PROP_PID_FILE_EXT);
			if (p) {
				strncpy(szExt, p->value.string, GF_4CC_MSIZE-1);
				szExt[GF_4CC_MSIZE-1] = 0;
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
				if (ctx->dur.num && ctx->dur.den && !gf_audio_fmt_is_planar(ctx->target_afmt ? ctx->target_afmt : ctx->afmt))
					ctx->trunc_audio = GF_TRUE;
			}

		} else {
			strcpy(szExt, gf_4cc_to_str(cid));
		}
		if (ctx->is_bmp) strcpy(szExt, "bmp");
		else if (ctx->is_y4m) strcpy(szExt, "y4m");
		else if (ctx->is_wav) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_UINT(GF_PID_FILE_PATCH_REPLACE) );
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
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Exporting %s - Size %dx%d\n", name, w, h));
		} else if (sr && chan) {
			if (cid==GF_CODECID_RAW) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Exporting PCM %s SampleRate %d %d channels %d bits per sample\n", gf_audio_fmt_name(sfmt), sr, chan, gf_audio_fmt_bit_depth(sfmt) ));
			} else {
				if (!nb_bps)
					nb_bps = gf_audio_fmt_bit_depth(sfmt);

				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Exporting %s - SampleRate %d %d channels %d bits per sample\n", name, sr, chan, nb_bps ));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Exporting %s\n", name));
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
	if (p && (p->value.lfrac.num>0)) ctx->duration = p->value.lfrac;

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
		return gf_filter_pck_new_ref(ctx->opid, 0, 0, in_pck);
	}

	size = data_size + 8*4 /*jP%20%20 + ftyp*/ + ctx->dcfg_size + 8;

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dst_pck) return NULL;


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

	size = ctx->w*ctx->h*3 + 54; //14 + 40 = size of BMP file header and BMP file info;
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dst_pck) return NULL;

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
	output += sizeof(u8) * 54;
	for (i=ctx->h; i>0; i--) {
		u8 *ptr = data + (i-1)*ctx->stride;
		memcpy(output, ptr, sizeof(u8)*3*ctx->w);
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
	if (!dst_pck) return;

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
	gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
	gf_filter_pck_send(dst_pck);
}


GF_XMLAttribute *ttml_get_attr(GF_XMLNode *node, char *name)
{
	u32 i=0;
	GF_XMLAttribute *att;
	while (node && (att = gf_list_enum(node->attributes, &i)) ) {
		if (!strcmp(att->name, name)) return att;
	}
	return NULL;
}

static GF_XMLNode *ttml_locate_div(GF_XMLNode *body, const char *region_name, u32 div_idx)
{
	u32 i=0, loc_div_idx=0;
	GF_XMLNode *div;
	while ( (div = gf_list_enum(body->content, &i)) ) {
		if (div->type) continue;
		if (strcmp(div->name, "div")) continue;

		if (!region_name) {
			if (div_idx==loc_div_idx) return div;
			loc_div_idx++;
		} else {
			GF_XMLAttribute *att = ttml_get_attr(div, "region");
			if (att && att->value && !strcmp(att->value, region_name))
				return div;
		}
	}
	if (region_name) {
		return ttml_locate_div(body, NULL, div_idx);
	}
	return NULL;
}

static GF_XMLNode *ttml_get_head(GF_XMLNode *root)
{
	u32 i=0;
	GF_XMLNode *child;
	while (root && (child = gf_list_enum(root->content, &i)) ) {
		if (child->type) continue;
		if (!strcmp(child->name, "head")) return child;
	}
	return NULL;
}

GF_XMLNode *ttml_get_body(GF_XMLNode *root)
{
	u32 i=0;
	GF_XMLNode *child;
	while (root && (child = gf_list_enum(root->content, &i)) ) {
		if (child->type) continue;
		if (!strcmp(child->name, "body")) return child;
	}
	return NULL;
}

static Bool ttml_same_attr(GF_XMLNode *n1, GF_XMLNode *n2)
{
	u32 i=0;
	GF_XMLAttribute *att1;
	while ( (att1 = gf_list_enum(n1->attributes, &i))) {
		Bool found = GF_FALSE;
		u32 j=0;
		GF_XMLAttribute *att2;
		while ( (att2 = gf_list_enum(n2->attributes, &j))) {
			if (!strcmp(att1->name, att2->name) && !strcmp(att1->value, att2->value)) {
				found = GF_TRUE;
				break;
			}
		}
		if (!found) return GF_FALSE;
	}
	return GF_TRUE;
}

static void ttml_merge_head(GF_XMLNode *node_src, GF_XMLNode *node_dst)
{
	u32 i=0;
	GF_XMLNode *child_src;
	while ( (child_src = gf_list_enum(node_src->content, &i)) ) {
		Bool found = GF_FALSE;
		u32 j=0;
		GF_XMLNode *child_dst;
		while ( (child_dst = gf_list_enum(node_dst->content, &j)) ) {
			if (!strcmp(child_src->name, child_dst->name) && ttml_same_attr(child_dst, child_src)) {
				found = GF_TRUE;
				break;
			}
		}
		if (found) {
			ttml_merge_head(child_src, child_dst);
			continue;
		}
		i--;
		gf_list_rem(node_src->content, i);
		gf_list_add(node_dst->content, child_src);
	}
	return;
}

static GF_Err ttml_embed_data(GF_XMLNode *node, u8 *aux_data, u32 aux_data_size, u8 *subs_data, u32 subs_data_size)
{
	u32 i=0;
	GF_XMLNode *child;
	u32 subs_idx=0;
	GF_XMLAttribute *att;
	Bool is_source = GF_FALSE;
	Bool has_src_att = GF_FALSE;
	if (!node || node->type) return GF_BAD_PARAM;

	if (!strcmp(node->name, "source")) {
		is_source = GF_TRUE;
	}

	while ((att = gf_list_enum(node->attributes, &i))) {
		char *sep, *fext;
		if (strcmp(att->name, "src")) continue;
		has_src_att = GF_TRUE;
		if (strncmp(att->value, "urn:", 4)) continue;
		sep = strrchr(att->value, ':');
		if (!sep) continue;
		sep++;
		fext = gf_file_ext_start(sep);
		if (fext) fext[0] = 0;
		subs_idx = atoi(sep);
		if (fext) fext[0] = '.';
		if (!subs_idx || ((subs_idx-1) * 14 > subs_data_size)) {
			subs_idx = 0;
			continue;
		}
		break;
	}
	if (subs_idx) {
		GF_XMLNode *data;
		u32 subs_offset = 0;
		u32 subs_size = 0;
		u32 idx = 1;
		//fetch subsample info
		for (i=0; i<subs_data_size; i+=14) {
			u32 a_subs_size = subs_data[i+4];
			a_subs_size <<= 8;
			a_subs_size |= subs_data[i+5];
			a_subs_size <<= 8;
			a_subs_size |= subs_data[i+6];
			a_subs_size <<= 8;
			a_subs_size |= subs_data[i+7];
			if (idx==subs_idx) {
				subs_size = a_subs_size;
				break;
			}
			subs_offset += a_subs_size;
			idx++;
		}
		if (!subs_size || (subs_offset + subs_size > aux_data_size)) {
			if (!subs_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("No subsample with index %u in sample\n", subs_idx));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Corrupted subsample %u info, size %u offset %u but sample size %u\n", subs_idx, subs_size, subs_offset, aux_data_size));
			}
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		//remove src attribute
		gf_list_del_item(node->attributes, att);
		if (att->name) gf_free(att->name);
		if (att->value) gf_free(att->value);
		gf_free(att);

		//create a source node
		if (!is_source) {
			GF_XMLNode *s;
			GF_SAFEALLOC(s, GF_XMLNode);
			if (!s) return GF_OUT_OF_MEM;
			s->attributes = gf_list_new();
			s->content = gf_list_new();
			s->name = gf_strdup("source");
			gf_list_add(node->content, s);
			if (!s->name || !s->content || !s->attributes) return GF_OUT_OF_MEM;
			//move @type to source
			att = ttml_get_attr(node, "type");
			if (att) {
				gf_list_del_item(node->attributes, att);
				gf_list_add(s->attributes, att);
			}
			node = s;
		}

		//create a data node
		GF_SAFEALLOC(data, GF_XMLNode);
		if (!data) return GF_OUT_OF_MEM;
		data->attributes = gf_list_new();
		data->content = gf_list_new();
		data->name = gf_strdup("data");
		gf_list_add(node->content, data);
		if (!data->name || !data->content || !data->attributes) return GF_OUT_OF_MEM;
		//move @type to data
		att = ttml_get_attr(node, "type");
		if (att) {
			gf_list_del_item(node->attributes, att);
			gf_list_add(data->attributes, att);
		}
		//base64 encode in a child
		GF_SAFEALLOC(node, GF_XMLNode)
		if (!node) return GF_OUT_OF_MEM;
		node->type = GF_XML_TEXT_TYPE;
		u32 size_64 = (subs_size * 2) + 3;
		node->name = gf_malloc(sizeof(char) * size_64);
		if (!node->name) {
			gf_free(node);
			return GF_OUT_OF_MEM;
		}
		subs_size = gf_base64_encode(aux_data + subs_offset, subs_size, (u8*) node->name, size_64);
		node->name[subs_size] = 0;
		return gf_list_add(data->content, node);
	}
	//src was present but not an embedded data, do not recurse
	if (has_src_att) return GF_OK;

	i=0;
	while ((child = gf_list_enum(node->content, &i))) {
		if (child->type) continue;
		ttml_embed_data(child, aux_data, aux_data_size, subs_data, subs_data_size);
	}
	return GF_OK;
}

static GF_Err writegen_push_ttml(GF_GenDumpCtx *ctx, char *data, u32 data_size, GF_FilterPacket *in_pck)
{
	const GF_PropertyValue *subs = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_SUBS);
	char *ttml_text = NULL;
	GF_Err e = GF_OK;
	u32 ttml_text_size;
	GF_DOMParser *dom;
	u32 txt_size, nb_children;
	u32 i, k, div_idx;
	GF_XMLNode *root_pck, *root_global, *p_global, *p_pck, *body_pck, *body_global, *head_pck, *head_global;

	if (subs) {
		if (subs->value.data.size < 14)
			return GF_NON_COMPLIANT_BITSTREAM;
		txt_size = subs->value.data.ptr[4];
		txt_size <<= 8;
		txt_size |= subs->value.data.ptr[5];
		txt_size <<= 8;
		txt_size |= subs->value.data.ptr[6];
		txt_size <<= 8;
		txt_size |= subs->value.data.ptr[7];
		if (txt_size>data_size)
			return GF_NON_COMPLIANT_BITSTREAM;
	} else {
		txt_size = data_size;
	}
	ttml_text_size = txt_size + 2;
	ttml_text = gf_malloc(sizeof(char) * ttml_text_size);
	if (!ttml_text) return GF_OUT_OF_MEM;

	memcpy(ttml_text, data, txt_size);
	ttml_text[txt_size] = 0;
	ttml_text[txt_size+1] = 0;

	dom = gf_xml_dom_new();
	if (!dom) return GF_OUT_OF_MEM;
	e = gf_xml_dom_parse_string(dom, ttml_text);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[XML] Invalid TTML doc: %s\n\tXML text was:\n%s", gf_xml_dom_get_error(dom), ttml_text));
		goto exit;
	}
	root_pck = gf_xml_dom_get_root(dom);

	//subsamples, replace all data embedded as subsample with base64 encoding
	if (subs && root_pck) {
		e = ttml_embed_data(root_pck, data+txt_size, data_size-txt_size, subs->value.data.ptr+14, subs->value.data.size-14);
		if (e) goto exit;
	}

	if (!ctx->ttml_root) {
		root_global = gf_xml_dom_detach_root(dom);
		if (root_global) {
			if (gf_list_count(root_global->content) > 0) ctx->ttml_root = root_global;
			else gf_xml_dom_node_del(root_global);
		}
		goto exit;
	}
	root_global = ctx->ttml_root;

	head_global = ttml_get_head(root_global);
	head_pck = ttml_get_head(root_pck);
	if (head_pck) {
		if (!head_global) {
			gf_list_del_item(root_pck->content, head_pck);
			gf_list_add(root_global->content, head_pck);
		} else {
			ttml_merge_head(head_pck, head_global);
		}
	}

	body_pck = ttml_get_body(root_pck);
	body_global = ttml_get_body(root_global);
	if (body_pck && !body_global) {
		gf_list_del_item(root_pck->content, body_pck);
		gf_list_add(root_global->content, body_pck);
		goto exit;
	}

	div_idx = 0;
	nb_children = body_pck ? gf_list_count(body_pck->content) : 0;
	for (k=0; k<nb_children; k++) {
		GF_XMLAttribute *div_reg = NULL;
		GF_XMLNode *div_global, *div_pck;
		div_pck = gf_list_get(body_pck->content, k);
		if (div_pck->type) continue;
		if (strcmp(div_pck->name, "div")) continue;

		if (ctx->merge_region)
			div_reg = ttml_get_attr(div_pck, "region");

		//merge input doc
		div_global = ttml_locate_div(body_global, div_reg ? div_reg->value : NULL, div_idx);

		if (!div_global) {
			gf_list_rem(body_pck->content, k);
			gf_list_insert(body_global->content, div_pck, div_idx+1);
			div_idx++;
			continue;
		}
		div_idx++;

		i=0;
		while ( (p_pck = gf_list_enum(div_pck->content, &i)) ) {
			s32 idx;
			GF_XMLNode *txt;
			u32 j=0;
			Bool found = GF_FALSE;
			if (p_pck->type) continue;
			if (strcmp(p_pck->name, "p")) continue;
			while ( (p_global = gf_list_enum(div_global->content, &j)) ) {
				if (p_global->type) continue;
				if (strcmp(p_global->name, "p")) continue;

				if (ttml_same_attr(p_pck, p_global)) {
					found = GF_TRUE;
					break;
				}
			}
			if (found) continue;

			//insert this p - if last entry in global div is text, insert before
			txt = gf_list_last(div_global->content);
			if (txt && txt->type) {
				idx = gf_list_count(div_global->content) - 1;
			} else {
				idx = gf_list_count(div_global->content);
			}

			i--;
			gf_list_rem(div_pck->content, i);
			if (i) {
				txt = gf_list_get(div_pck->content, i-1);
				if (txt->type) {
					i--;
					gf_list_rem(div_pck->content, i);
					gf_list_insert(div_global->content, txt, idx);
					idx++;
				}
			}
			gf_list_insert(div_global->content, p_pck, idx);

		}
	}

exit:
	if (ttml_text) gf_free(ttml_text);
	gf_xml_dom_del(dom);
	return e;
}

static GF_Err writegen_flush_ttml(GF_GenDumpCtx *ctx)
{
	char *data;
	u8 *output;
	u32 size;
	GF_FilterPacket *pck;
	if (!ctx->ttml_root) return GF_OK;

	if (ctx->merge_region) {
		u32 i, count;
		GF_XMLNode *body_global = ttml_get_body(ctx->ttml_root);
		count = body_global ? gf_list_count(body_global->content) : 0;
		for (i=0; i<count; i++) {
			GF_XMLNode *child;
			GF_XMLNode *div = gf_list_get(body_global->content, i);
			if (div->type) continue;
			if (strcmp(div->name, "div")) continue;
			if (gf_list_count(div->content) > 1) continue;;

			child = gf_list_get(div->content, 0);
			if (child && !child->type) continue;;

			gf_list_rem(body_global->content, i);
			i--;
			count--;
			gf_xml_dom_node_del(div);
		}
	}

	data = gf_xml_dom_serialize_root(ctx->ttml_root, GF_FALSE, GF_FALSE);
	if (!data) return GF_OK;
	size = (u32) strlen(data);
	pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!pck) return GF_OUT_OF_MEM;

	memcpy(output, data, size);
	gf_free(data);

	if (ctx->ttml_dash_pck) {
		gf_filter_pck_merge_properties(ctx->ttml_dash_pck, pck);
		gf_filter_pck_set_byte_offset(pck, GF_FILTER_NO_BO);
		gf_filter_pck_set_dependency_flags(pck, 0);
		gf_filter_pck_unref(ctx->ttml_dash_pck);
		ctx->ttml_dash_pck = NULL;
	}
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
	gf_xml_dom_node_del(ctx->ttml_root);
	ctx->ttml_root = NULL;
	return gf_filter_pck_send(pck);
}

static GF_Err writegen_flush_ttxt(GF_GenDumpCtx *ctx)
{
	const char *footer = "</TextStream>";
	if (!ctx->need_ttxt_footer) return GF_OK;
	u32 size = (u32) strlen(footer);
	u8 *output;
	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	memcpy(output, footer, size);
	gf_filter_pck_set_framing(pck, GF_FALSE, GF_TRUE);
	gf_filter_pck_set_byte_offset(pck, GF_FILTER_NO_BO);
	ctx->need_ttxt_footer = GF_FALSE;
	return gf_filter_pck_send(pck);
}


#include <gpac/webvtt.h>
static void webvtt_timestamps_dump(GF_BitStream *bs, u64 start_ts, u64 end_ts, u32 timescale, Bool write_srt)
{
	char szTS[200];
	szTS[0] = 0;
	GF_WebVTTTimestamp start, end;
	gf_webvtt_timestamp_set(&start, gf_timestamp_rescale(start_ts, timescale, 1000) );
	gf_webvtt_timestamp_set(&end, gf_timestamp_rescale(end_ts, timescale, 1000) );

	Bool write_hour = GF_FALSE;
	if (gf_opts_get_bool("core", "webvtt-hours")) write_hour = GF_TRUE;
	else if (start.hour || end.hour) write_hour = GF_TRUE;
	else if (write_srt) write_hour = GF_TRUE;

	if (write_hour) {
		sprintf(szTS, "%02u:", start.hour);
		gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );
	}
	sprintf(szTS, "%02u:%02u%c%03u", start.min, start.sec, write_srt ? ',' : '.', start.ms);
	gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );

	sprintf(szTS, " --> ");
	gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );

	if (write_hour) {
		sprintf(szTS, "%02u:", end.hour);
		gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );
	}
	sprintf(szTS, "%02u:%02u%c%03u", end.min, end.sec, write_srt ? ',' : '.', end.ms);
	gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );
}

GF_Err writegen_process(GF_Filter *filter)
{
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck = NULL;
	char *data;
	u32 pck_size;
	Bool do_abort = GF_FALSE;
	Bool split = ctx->split;
	if (!ctx->ipid) return GF_EOS;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck || !ctx->codecid) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (ctx->is_wav) writegen_write_wav_header(ctx);
			else if (ctx->ttml_agg) writegen_flush_ttml(ctx);
			else if (ctx->codecid==GF_CODECID_TX3G) {
				writegen_flush_ttxt(ctx);
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	ctx->sample_num++;

	if (ctx->dash_mode && ctx->ttml_agg && gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM)) {
		if (ctx->ttml_dash_pck) {
			writegen_flush_ttml(ctx);
			gf_filter_pck_unref(ctx->ttml_dash_pck);
			ctx->ttml_dash_pck = NULL;
		}

		ctx->ttml_dash_pck = pck;
		gf_filter_pck_ref_props(&ctx->ttml_dash_pck);
	}

	if (ctx->sstart) {
		if (ctx->sstart > ctx->sample_num) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		if ((ctx->sstart <= ctx->send) && (ctx->sample_num>ctx->send) ) {
			do_abort = GF_TRUE;
		}
	} else if (ctx->dur.num && ctx->dur.den) {
		u64 dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS)
		dts = gf_filter_pck_get_cts(pck);

		if (!ctx->first_dts_plus_one) {
			ctx->first_dts_plus_one = dts+1;
		} else {
			if (gf_timestamp_greater(dts + 1 - ctx->first_dts_plus_one, gf_filter_pck_get_timescale(pck), ctx->dur.num, ctx->dur.den)) {
				do_abort = GF_TRUE;
			}
		}
	}
	if (do_abort) {
		GF_FilterEvent evt;
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->ipid);
		gf_filter_pid_send_event(ctx->ipid, &evt);
		return GF_OK;
	}

	//except in dash mode, force a new file if GF_PROP_PCK_FILENUM is set
	if (!ctx->dash_mode && (gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM) != NULL)) {
		ctx->cfg_sent = GF_FALSE;
		ctx->first = GF_TRUE;
	}

	if (ctx->frame) {
		split = GF_TRUE;
	} else if (ctx->dcfg_size && gf_filter_pck_get_sap(pck) && !ctx->is_mj2k && !ctx->webvtt && (ctx->decinfo!=DECINFO_NO) && !ctx->cfg_sent) {
		if (ctx->codecid==GF_CODECID_FLAC) {
			u8 *dsi_out;
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->dcfg_size+4, &dsi_out);
			if (!dst_pck) return GF_OUT_OF_MEM;
			dsi_out[0] = 'f';
			dsi_out[1] = 'L';
			dsi_out[2] = 'a';
			dsi_out[3] = 'C';
			memcpy(dsi_out+4, ctx->dcfg, ctx->dcfg_size);
		} else {
			dst_pck = gf_filter_pck_new_shared(ctx->opid, ctx->dcfg, ctx->dcfg_size, NULL);
			if (!dst_pck) return GF_OUT_OF_MEM;
			gf_filter_pck_set_readonly(dst_pck);
		}
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		if (ctx->first) {
			gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->sample_num) );
			ctx->sample_num--;
		}

		ctx->first = GF_FALSE;
		gf_filter_pck_send(dst_pck);
		if ((ctx->decinfo==DECINFO_FIRST) && !ctx->split) {
			ctx->dcfg_size = 0;
			ctx->dcfg = NULL;
		}
		ctx->cfg_sent = GF_TRUE;
		if (ctx->codecid==GF_CODECID_TX3G)
			ctx->need_ttxt_footer = GF_TRUE;
		return GF_OK;
	}
	ctx->cfg_sent = GF_FALSE;
	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	if (ctx->is_mj2k) {
		dst_pck = writegen_write_j2k(ctx, data, pck_size, pck);
	} else if (ctx->is_bmp) {
		dst_pck = writegen_write_bmp(ctx, data, pck_size);
	} else if (ctx->is_y4m) {
		char *y4m_hdr = NULL;
		if (ctx->is_y4m==1) {
			char szInfo[100];
			const GF_PropertyValue *p;
			gf_dynstrcat(&y4m_hdr, "YUV4MPEG2", NULL);
			sprintf(szInfo, " W%d", ctx->w);
			gf_dynstrcat(&y4m_hdr, szInfo, NULL);
			sprintf(szInfo, " H%d", ctx->h);
			gf_dynstrcat(&y4m_hdr, szInfo, NULL);
			p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FPS);
			if (p) {
				sprintf(szInfo, " F%d:%d", p->value.frac.num, p->value.frac.den);
				gf_dynstrcat(&y4m_hdr, szInfo, NULL);
			}
			p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_SAR);
			if (p) {
				sprintf(szInfo, " A%d:%d", p->value.frac.num, p->value.frac.den);
				gf_dynstrcat(&y4m_hdr, szInfo, NULL);
			}
			u32 ilce = gf_filter_pck_get_interlaced(pck);
			sprintf(szInfo, " I%c", (ilce==0) ? 'p' : ((ilce==1) ? 't' : 'b'));
			gf_dynstrcat(&y4m_hdr, szInfo, NULL);

			switch (ctx->target_pfmt) {
			case GF_PIXEL_YUV: gf_dynstrcat(&y4m_hdr, " C420mpeg2", NULL); break;
			case GF_PIXEL_YUV444: gf_dynstrcat(&y4m_hdr, " C444", NULL); break;
			case GF_PIXEL_YUV422: gf_dynstrcat(&y4m_hdr, " C422", NULL); break;
			case GF_PIXEL_GREYSCALE: gf_dynstrcat(&y4m_hdr, " Cmono", NULL); break;
			case GF_PIXEL_YUVA444: gf_dynstrcat(&y4m_hdr, " C444alpha", NULL); break;
			}
			p = gf_filter_pid_get_property_str(ctx->ipid, "yuv4meg_meta");
			if (p) {
				gf_dynstrcat(&y4m_hdr, p->value.string, " X");
			}
			gf_dynstrcat(&y4m_hdr, "\n", NULL);
			ctx->is_y4m=2;
		}
		if (ctx->is_y4m==2) {
			gf_dynstrcat(&y4m_hdr, "FRAME\n", NULL);
		}
		u8 * output;
		u32 len = y4m_hdr ? (u32) strlen(y4m_hdr) : 0;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, pck_size + len, &output);
		if (!dst_pck) return GF_OUT_OF_MEM;
		if (y4m_hdr) memcpy(output, y4m_hdr, len);
		memcpy(output+len, data, pck_size);
		if (ctx->split)
			ctx->is_y4m=1;

		if (y4m_hdr)
			gf_free(y4m_hdr);

	} else if (ctx->is_wav && ctx->first) {
		u8 * output;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, 44, &output);
		if (!dst_pck) return GF_OUT_OF_MEM;

		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
		gf_filter_pck_set_corrupted(dst_pck, GF_TRUE);
		ctx->first = GF_FALSE;
		gf_filter_pck_send(dst_pck);
		ctx->nb_bytes += 44;
		return GF_OK;
	} else if (ctx->ttml_agg) {
		GF_Err e = writegen_push_ttml(ctx, data, pck_size, pck);
		ctx->first = GF_FALSE;
		if (e) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return e;
		}
		goto no_output;
	} else if (ctx->trunc_audio) {
		u64 dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS)
			dts = gf_filter_pck_get_cts(pck);

		if (!ctx->first_dts_plus_one) {
			ctx->first_dts_plus_one = dts+1;
		} else {
			u32 timescale = gf_filter_pck_get_timescale(pck);
			u32 dur = gf_filter_pck_get_duration(pck);
			if (gf_timestamp_greater(dts + dur + 1 - ctx->first_dts_plus_one, timescale, ctx->dur.num, ctx->dur.den)) {
				u32 bpp;
				u8 *odata;
				const GF_PropertyValue *p;
				dur = ctx->dur.num * timescale / ctx->dur.den;
				dur -= (u32) (dts + 1 - ctx->first_dts_plus_one);

				bpp = gf_audio_fmt_bit_depth(ctx->target_afmt);
				p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_NUM_CHANNELS);
				if (p) bpp *= p->value.uint;
				bpp/= 8;

				p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_SAMPLE_RATE);
				if (p && (p->value.uint != timescale)) {
					dur *= p->value.uint;
					dur /= timescale;
				}
				assert(pck_size >= bpp * dur);
				pck_size = bpp * dur;

				dst_pck = gf_filter_pck_new_alloc(ctx->opid, pck_size, &odata);
				if (dst_pck)
					memcpy(odata, data, pck_size);
			}
		}

		if (!dst_pck) {
			dst_pck = gf_filter_pck_new_ref(ctx->opid, 0, 0, pck);
		}
	} else if (ctx->webvtt || ctx->dump_srt) {
		const GF_PropertyValue *p;

		if (!data || !pck_size) {
			ctx->sample_num--;
			goto no_output;
		}

		u64 start = gf_filter_pck_get_cts(pck);
		u64 end = start + gf_filter_pck_get_duration(pck);
		u32 timescale = gf_filter_pck_get_timescale(pck);

		if (!ctx->bs) ctx->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->bs, ctx->write_buf, ctx->write_alloc);

		if (ctx->first && !ctx->dump_srt) {
			if (ctx->dcfg && ctx->dcfg_size) {
				u32 len = ctx->dcfg_size;
				if (ctx->dcfg[len-1]==0) len--;
				gf_bs_write_data(ctx->bs, ctx->dcfg, len);
				if (ctx->dcfg[len-1]!='\n')
					gf_bs_write_data(ctx->bs, "\n", 1);
				gf_bs_write_data(ctx->bs, "\n", 1);
			} else
				gf_bs_write_data(ctx->bs, "WEBVTT\n\n", 8);
		}
		p = gf_filter_pck_get_property_str(pck, "vtt_pre");
		if (!ctx->dump_srt && p && p->value.string) {
			u32 len = (u32) strlen(p->value.string);
			gf_bs_write_data(ctx->bs, p->value.string, len);
			if (len && (p->value.string[len-1]!='\n'))
				gf_bs_write_data(ctx->bs, "\n", 1);
			gf_bs_write_data(ctx->bs, "\n", 1);
		}
		p = gf_filter_pck_get_property_str(pck, "vtt_cueid");
		if (!ctx->dump_srt && p && p->value.string) {
			u32 len = (u32) strlen(p->value.string) ;
			gf_bs_write_data(ctx->bs, p->value.string, len);
			if (len && (p->value.string[len-1]!='\n'))
				gf_bs_write_data(ctx->bs, "\n", 1);
		}

		if (ctx->dump_srt) {
			char szCID[100];
			sprintf(szCID, "%d\n", ctx->sample_num);
			gf_bs_write_data(ctx->bs, szCID, (u32) strlen(szCID));
		}
		webvtt_timestamps_dump(ctx->bs, start, end, timescale, ctx->dump_srt);

		p = gf_filter_pck_get_property_str(pck, "vtt_settings");
		if (!ctx->dump_srt && p && p->value.string) {
			gf_bs_write_data(ctx->bs, " ", 1);
			gf_bs_write_data(ctx->bs, p->value.string, (u32) strlen(p->value.string));
		}

		gf_bs_write_data(ctx->bs, "\n", 1);
		if (data) {
			gf_bs_write_data(ctx->bs, data, pck_size);
			if (pck_size && (data[pck_size-1] != '\n'))
				gf_bs_write_data(ctx->bs, "\n", 1);
		}
		gf_bs_write_data(ctx->bs, "\n", 1);

		u8 *odata;
		u32 vtt_data_size, alloc_size;
		gf_bs_get_content_no_truncate(ctx->bs, &ctx->write_buf, &vtt_data_size, &alloc_size);
		if (alloc_size>ctx->write_alloc) ctx->write_alloc = alloc_size;

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, vtt_data_size, &odata);
		memcpy(odata, ctx->write_buf, vtt_data_size);
	} else {
		dst_pck = gf_filter_pck_new_ref(ctx->opid, 0, 0, pck);
	}
	if (!dst_pck) return GF_OUT_OF_MEM;
	
	gf_filter_pck_merge_properties(pck, dst_pck);
	//don't keep byte offset
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	if (split) {
		if (ctx->first)
			gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->sample_num) );
		gf_filter_pck_set_framing(dst_pck, ctx->first, ctx->need_ttxt_footer ? GF_FALSE : GF_TRUE);
		ctx->first = GF_TRUE;
	} else {
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;
	}

	gf_filter_pck_set_seek_flag(dst_pck, 0);
	gf_filter_pck_send(dst_pck);

	if (split && ctx->need_ttxt_footer)
		writegen_flush_ttxt(ctx);
	ctx->nb_bytes += pck_size;

no_output:
	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_dts(pck);
		if (ts==GF_FILTER_NO_TS)
			ts = gf_filter_pck_get_cts(pck);
		if (ts!=GF_FILTER_NO_TS) {
			ts += gf_filter_pck_get_duration(pck);
			ts = gf_timestamp_rescale(ts, timescale, ctx->duration.den);
			gf_set_progress("Exporting", ts, ctx->duration.num);
		}
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
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "image/bmp"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "wav"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/wav|audio/wave"),
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
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/avc|video/h264|video/svc|video/mvc"),
	{0},

	//we accept unframed HEVC (annex B format)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "265|h265|hvc|hevc|shvc|lhvc"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/hevc|video/lhvc|video/shvc|video/mhvc"),
	{0},

	//we accept unframed OBU (without size field)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VP8),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VP9),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VP10),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "obu|av1|av1b|ivf"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/x-ivf|video/av1"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "cmp|m4ve|m4v"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/mp4v-es"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_H263),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_S263),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "h263|263|s263"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/h263"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG1),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "m1v"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/mpgv-es"),
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
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/mpgv-es"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4H),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4X),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCH),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCS),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCN),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "prores"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/prores"),
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
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "aac|adts"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/x-m4a|audio/aac|audio/aacp|audio/x-aac"),
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
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/aac+latm"),
	{0},

	//we accept unframed USAC + LATM
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_USAC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED_LATM, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "usac|xheaac|latm"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/xheaac+latm"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_PNG),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "png"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "image/png"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_JPEG),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "jpg|jpeg"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "image/jpg"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_J2K),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "jp2|j2k"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "image/jp2"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mp3"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/mp3|audio/x-mp3"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mp2"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/mpeg|audio/x-mp2"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO_L1),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mp1"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/mpeg|audio/x-mp1"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AMR),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AMR_WB),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "amr"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/amr"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SMV),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "smv"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/smv"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EVRC_PV),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EVRC),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "evc"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/evrc"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "ac3"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/x-ac3|audio/ac3"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EAC3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "eac3"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/x-eac3|audio/eac3"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_META_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_TEXT),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "txt"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "x-subtitle/srt|subtitle/srt|text/srt"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_META_XML),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "xml|txml|ttml"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "subtitle/ttml|text/ttml|application/xml+ttml"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "vtt"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "subtitle/vtt|text/vtt"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "srt"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "subtitle/srt|text/srt"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "ttxt"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "x-subtitle/ttxt|subtitle/ttxt|text/ttxt"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_QCELP),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "qcelp"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/qcelp"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "theo"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/x-theora"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VORBIS),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "vorb"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/x-vorbis"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SPEEX),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "spx"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/x-speex"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "flac"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/flac"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPHA),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mhas"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/mpegh"),
	{0},

	//we accept unframed VVC (annex B format)
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "266|h266|vvc|lvvc"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/vvc|video/lvvc"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TRUEHD),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "mlp"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "audio/truehd"),
	{0},

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_FFV1),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "ffv1"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/x-ffv1"),
	{0},

	//raw color dump YUV and RGB - keep it as first for field extension assignment
	//cf below
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "y4m"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "video/x-yuv4mpeg"),
	{0},

	//anything else: only for explicit dump (filter loaded on purpose), otherwise don't load for filter link resolution
	CAP_UINT(GF_CAPS_OUTPUT_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "*"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "*"),
	//for the rest, we include everything, only specifies excluded ones from above and non-handled ones
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),

	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AVC_PS),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_VVC),

	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO_L1),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
//	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),

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
	{ OFFS(split), "force one file per decoded frame", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(frame), "force single frame dump with no rewrite. In this mode, all codec types are supported", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(sstart), "start number of frame to forward. If 0, all samples are forwarded", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(send), "end number of frame to forward. If less than start frame, all samples after start are forwarded", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(dur), "duration of media to forward after first sample. If 0, all samples are forwarded", GF_PROP_FRACTION, "0", NULL, 0},
	{ OFFS(merge_region), "merge TTML regions with same ID while reassembling TTML doc", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};

static GF_Err writegen_initialize(GF_Filter *filter);

void writegen_finalize(GF_Filter *filter)
{
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->write_buf) gf_free(ctx->write_buf);
	if (ctx->ttml_root)
		gf_xml_dom_node_del(ctx->ttml_root);

	if (ctx->ttml_dash_pck) {
		gf_filter_pck_unref(ctx->ttml_dash_pck);
		ctx->ttml_dash_pck = NULL;
	}
}

GF_FilterRegister GenDumpRegister = {
	.name = "writegen",
	GF_FS_SET_DESCRIPTION("Stream to file")
	GF_FS_SET_HELP("Generic single stream to file converter, used when extracting/converting PIDs.\n"
	"The writegen filter should usually not be explicitly loaded without a source ID specified, since the filter would likely match any PID connection.")
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

#endif /*GPAC_DISABLE_CORE_TOOLS*/
