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
	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	Bool is_bmp;
} GF_ReframeImgCtx;


GF_Err img_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_ReframeImgCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	ctx->ipid = pid;
	return GF_OK;
}

GF_Err img_process(GF_Filter *filter)
{
	GF_ReframeImgCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	GF_Err e;
	char *data, *output;
	u32 size, w=0, h=0, pf=0;
	char *pix;
	u32 i, j, irow, in_stride, out_stride;
	GF_BitStream *bs;
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) return GF_EOS;

	data = (char *) gf_filter_pck_get_data(pck, &size);

	if (!ctx->opid) {
#ifndef GPAC_DISABLE_AV_PARSERS
		u32 dsi_size;
		char *dsi=NULL;
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
		if (!codecid) return GF_NOT_SUPPORTED;

		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_SERVICE_ERROR;
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(codecid));
		if (pf) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, & PROP_UINT(pf));
		if (w) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(w));
		if (h) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(h));
		if (dsi) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi, dsi_size));

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_NB_FRAMES, &PROP_UINT(1) );

		if (ext || mime)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
	}
	if (! ctx->is_bmp) {
		e = gf_filter_pck_forward(pck, ctx->opid);
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

static const GF_FilterCapability ReframeImgInputs[] =
{
	CAP_INC_STRING(GF_PROP_PID_MIME, "image/jpg|image/jp2|image/bmp|image/png|image/x-png+depth|image/x-png+depth+mask|image/x-png+stereo"),
	{},
	CAP_INC_STRING(GF_PROP_PID_FILE_EXT, "jpg|jpeg|jp2|bmp|png|pngd|pngds|pngs"),
};


static const GF_FilterCapability ReframeImgOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_PNG),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_JPEG),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_J2K),
	{}
};

GF_FilterRegister ReframeImgRegister = {
	.name = "img_reframe",
	.description = "Image Reframer ",
	.private_size = sizeof(GF_ReframeImgCtx),
	INCAPS(ReframeImgInputs),
	OUTCAPS(ReframeImgOutputs),
	.configure_pid = img_configure_pid,
	.process = img_process,
};

const GF_FilterRegister *img_reframe_register(GF_FilterSession *session)
{
	return &ReframeImgRegister;
}


