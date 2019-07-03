/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / image (jpg/png/bmp/j2k) reframer filter
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
#include <gpac/avparse.h>


#if defined(WIN32) || defined(_WIN32_WCE)
#include <windows.h>
#else

#ifdef GPAC_CONFIG_LINUX
#include <arpa/inet.h>
#endif

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

typedef struct
{
	//options
	GF_Fraction fps;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;
	u32 src_timescale;
	Bool is_bmp;
	Bool owns_timescale;
	u32 codec_id;

	Bool initial_play_done;
	Bool is_playing;
} GF_ReframeImgCtx;


GF_Err img_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_ReframeImgCtx *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *p;
	
	if (is_remove) {
		ctx->ipid = NULL;
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	ctx->ipid = pid;
	//force retest of codecid
	ctx->codec_id = 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) ctx->src_timescale = p->value.uint;

	if (ctx->src_timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	ctx->is_playing = GF_TRUE;
	return GF_OK;
}

Bool img_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterEvent fevt;
	GF_ReframeImgCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.on_pid != ctx->opid) return GF_TRUE;
	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->is_playing) {
			return GF_TRUE;
		}

		ctx->is_playing = GF_TRUE;
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			return GF_TRUE;
		}

		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = 0;
		gf_filter_pid_send_event(ctx->ipid, &fevt);
		return GF_TRUE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		return GF_FALSE;
	default:
		break;
	}
	//cancel all events
	return GF_TRUE;
}

GF_Err img_process(GF_Filter *filter)
{
	GF_ReframeImgCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	GF_Err e;
	u8 *data, *output;
	u32 size, w=0, h=0, pf=0;
	u8 *pix;
	u32 i, j, irow, in_stride, out_stride;
	GF_BitStream *bs;
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			ctx->is_playing = GF_FALSE;
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (char *) gf_filter_pck_get_data(pck, &size);
	
	if (!ctx->opid || !ctx->codec_id) {
#ifndef GPAC_DISABLE_AV_PARSERS
		u32 dsi_size;
		u8 *dsi=NULL;
#endif
		const char *ext, *mime;
		const GF_PropertyValue *prop;
		u32 codecid = 0;

		if ((size >= 54) && (data[0] == 'B') && (data[1] == 'M')) {
			codecid = GF_CODECID_RAW;
			ctx->is_bmp = GF_TRUE;
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		else {
			bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
			gf_img_parse(bs, &codecid, &w, &h, &dsi, &dsi_size);
			gf_bs_del(bs);
		}
#endif

		prop = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_EXT);
		ext = (prop && prop->value.string) ? prop->value.string : "";
		prop = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_MIME);
		mime = (prop && prop->value.string) ? prop->value.string : "";

		if (!codecid) {
			if (!stricmp(ext, "jpeg") || !stricmp(ext, "jpg") || !strcmp(mime, "image/jpg")) {
				codecid = GF_CODECID_JPEG;
			} else if (!stricmp(ext, "png") || !strcmp(mime, "image/png")) {
				codecid = GF_CODECID_PNG;
			} else if (!stricmp(ext, "jp2") || !stricmp(ext, "j2k") || !strcmp(mime, "image/jp2")) {
				codecid = GF_CODECID_J2K;
			} else if (!stricmp(ext, "pngd")) {
				codecid = GF_CODECID_PNG;
				pf = GF_PIXEL_RGBD;
			} else if (!stricmp(ext, "pngds")) {
				codecid = GF_CODECID_PNG;
				pf = GF_PIXEL_RGBDS;
			} else if (!stricmp(ext, "pngs")) {
				codecid = GF_CODECID_PNG;
				pf = GF_PIXEL_RGBS;
			} else if (!stricmp(ext, "bmp") || !strcmp(mime, "image/png")) {
				codecid = GF_CODECID_RAW;
			}
		}
		if (!codecid) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NOT_SUPPORTED;
		}
		ctx->codec_id = codecid;
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_SERVICE_ERROR;
		}
		if (!ctx->fps.num*ctx->fps.den) {
			ctx->fps.num = 1000;
			ctx->fps.den = 1000;
		}
		//we don't have input reconfig for now
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(codecid));
		if (pf) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, & PROP_UINT(pf));
		if (w) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(w));
		if (h) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(h));
		if (dsi) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi, dsi_size));
		if (! gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_TIMESCALE)) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->fps.num) );
			ctx->owns_timescale = GF_TRUE;
		}

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NB_FRAMES, &PROP_UINT(1) );

		if (ext || mime)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
	}
	if (! ctx->is_bmp) {
		e = GF_OK;
		u32 start_offset = 0;
		if (ctx->codec_id==GF_CODECID_J2K) {

			if (size<8) {
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			if ((data[4]=='j') && (data[5]=='P') && (data[6]==' ') && (data[7]==' ')) {
				GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
				while (gf_bs_available(bs)) {
					u32 bsize = gf_bs_read_u32(bs);
					u32 btype = gf_bs_read_u32(bs);
					if (btype == GF_4CC('j','p','2','c') ) {
						start_offset = (u32) gf_bs_get_position(bs) - 8;
						break;
					}
					gf_bs_skip_bytes(bs, bsize-8);
				}
				gf_bs_del(bs);
				if (start_offset>=size) {
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
			}
		}
		dst_pck = gf_filter_pck_new_ref(ctx->opid, data+start_offset, size-start_offset, pck);
		if (!dst_pck) e = GF_OUT_OF_MEM;
		gf_filter_pck_merge_properties(pck, dst_pck);
		if (ctx->owns_timescale) {
			gf_filter_pck_set_cts(dst_pck, 0);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1 );
			gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
		}
		gf_filter_pck_send(dst_pck);
		gf_filter_pid_drop_packet(ctx->ipid);
		return e;
	}

	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);

	fh.bfType = gf_bs_read_u16(bs);
	fh.bfSize = gf_bs_read_u32(bs);
	fh.bfReserved1 = gf_bs_read_u16(bs);
	fh.bfReserved2 = gf_bs_read_u16(bs);
	fh.bfOffBits = gf_bs_read_u32(bs);
	fh.bfOffBits = ntohl(fh.bfOffBits);

	gf_bs_read_data(bs, (char *) &fi, 40);
	gf_bs_del(bs);

	if ((fi.biCompression != BI_RGB) || (fi.biPlanes!=1)) return GF_NOT_SUPPORTED;
	if ((fi.biBitCount!=24) && (fi.biBitCount!=32)) return GF_NOT_SUPPORTED;

	w = fi.biWidth;
	h = fi.biHeight;
	pf = (fi.biBitCount==24) ? GF_PIXEL_RGB : GF_PIXEL_RGBA;
	size = (fi.biBitCount==24) ? 3 : 4;
	size *= w;
	out_stride = size;
	size *= h;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, & PROP_UINT(pf));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(w));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(h));

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	gf_filter_pck_merge_properties(pck, dst_pck);
	if (ctx->owns_timescale) {
		gf_filter_pck_set_cts(dst_pck, 0);
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1 );
		gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
	}

	in_stride = out_stride;
	while (in_stride % 4) in_stride++;

	if (fi.biBitCount==24) {
		for (i=0; i<h; i++) {
			irow = (h-1-i)*out_stride;
			pix = data + fh.bfOffBits + i*in_stride;
			for (j=0; j<out_stride; j+=3) {
				output[j + irow] = pix[2];
				output[j+1 + irow] = pix[1];
				output[j+2 + irow] = pix[0];
				pix += 3;
			}
		}
	} else {
		for (i=0; i<h; i++) {
			irow = (h-1-i)*out_stride;
			pix = data + fh.bfOffBits + i*in_stride;
			for (j=0; j<out_stride; j+=4) {
				output[j + irow] = pix[2];
				output[j+1 + irow] = pix[1];
				output[j+2 + irow] = pix[0];
				output[j+3 + irow] = pix[3];
				pix += 4;
			}
		}
	}
	e = gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

#include <gpac/internal/isomedia_dev.h>

static const char * img_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	/*JPEG*/
	if ((data[0]==0xFF) && (data[1]==0xD8) && (data[2]==0xFF)) {
		*score = GF_FPROBE_SUPPORTED;
		return "image/jpg";
	}
	/*PNG*/
	if ((data[0]==0x89) && (data[1]==0x50) && (data[2]==0x4E)) {
		*score = GF_FPROBE_SUPPORTED;
		return "image/png";
	}
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	u32 bsize = gf_bs_read_u32(bs);
	u32 btype = gf_bs_read_u32(bs);
	if ( (bsize==12) && ( (btype==GF_ISOM_BOX_TYPE_JP ) || (btype==GF_ISOM_BOX_TYPE_JP2H) ) ) {
		if (btype==GF_ISOM_BOX_TYPE_JP2H) {
			*score = GF_FPROBE_FORCE;
			gf_bs_del(bs);
			return "image/jp2";
		}
		btype = gf_bs_read_u32(bs);
		if (btype==0x0D0A870A) {
			*score = GF_FPROBE_FORCE;
			gf_bs_del(bs);
			return "image/jp2";
		}
	}
	gf_bs_del(bs);
	if ((size >= 54) && (data[0] == 'B') && (data[1] == 'M')) {
		*score = GF_FPROBE_SUPPORTED;
		return "image/bmp";
	}
	return NULL;
}
static const GF_FilterCapability ReframeImgCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "image/jpg|image/jp2|image/bmp|image/png|image/x-png+depth|image/x-png+depth+mask|image/x-png+stereo"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_PNG),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_JPEG),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_J2K),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_RAW),
//	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "jpg|jpeg|jp2|bmp|png|pngd|pngds|pngs"),
};

#define OFFS(_n)	#_n, offsetof(GF_ReframeImgCtx, _n)
static const GF_FilterArgs ReframeImgArgs[] =
{
	{ OFFS(fps), "import frame rate (0 default to 1 Hz)", GF_PROP_FRACTION, "0/1000", NULL, GF_FS_ARG_HINT_HIDE},
	{0}
};

GF_FilterRegister ReframeImgRegister = {
	.name = "rfimg",
	GF_FS_SET_DESCRIPTION("JPG/J2K/PNG/BMP reframer")
	GF_FS_SET_HELP("This filter parses JPG/J2K/PNG/BMP files/data and outputs corresponding visual PID and frames.")
	.private_size = sizeof(GF_ReframeImgCtx),
	.args = ReframeImgArgs,
	SETCAPS(ReframeImgCaps),
	.configure_pid = img_configure_pid,
	.probe_data = img_probe_data,
	.process = img_process,
	.process_event = img_process_event
};

const GF_FilterRegister *img_reframe_register(GF_FilterSession *session)
{
	return &ReframeImgRegister;
}


