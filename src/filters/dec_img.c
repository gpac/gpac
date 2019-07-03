/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / libjpeg and libpng decoder filter
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

typedef struct
{
	u32 codecid;
	GF_FilterPid *ipid, *opid;
	u32 width, height, pixel_format, BPP;
} GF_IMGDecCtx;

static GF_Err imgdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_IMGDecCtx *ctx = (GF_IMGDecCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		//one in one out, this is simple
		gf_filter_pid_remove(ctx->opid);
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NOT_SUPPORTED;
	ctx->codecid = prop->value.uint;
	ctx->ipid = pid;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_RAW ));

	if (ctx->codecid==GF_CODECID_JPEG) {
		gf_filter_set_name(filter, "imgdec:libjpeg");
	} else if (ctx->codecid==GF_CODECID_PNG) {
		gf_filter_set_name(filter, "imgdec:libpng");
	}
	return GF_OK;
}

static GF_Err imgdec_process(GF_Filter *filter)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_Err e;
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 size;
	GF_IMGDecCtx *ctx = (GF_IMGDecCtx *) gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
			if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (char *) gf_filter_pck_get_data(pck, &size);

	if ((ctx->codecid == GF_CODECID_JPEG) || (ctx->codecid == GF_CODECID_PNG)) {
		u32 out_size = 0;
		u32 w = ctx->width;
		u32 h = ctx->height;
		u32 pf = ctx->pixel_format;

		if (ctx->codecid == GF_CODECID_JPEG) {
			e = gf_img_jpeg_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, NULL, &out_size, ctx->BPP);
		} else {
			e = gf_img_png_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, NULL, &out_size);
		}

		if (e != GF_BUFFER_TOO_SMALL) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return e;
		}
		if ((w != ctx->width) || (h!=ctx->height) || (pf!=ctx->pixel_format)) {
			switch (ctx->pixel_format) {
			case GF_PIXEL_GREYSCALE:
				ctx->BPP = 1;
				break;
			case GF_PIXEL_RGB:
				ctx->BPP = 3;
				break;
			case GF_PIXEL_RGBA:
				ctx->BPP = 4;
				break;
			}
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(ctx->width));
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(ctx->height));
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, & PROP_UINT(ctx->pixel_format));
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->BPP*ctx->width) );
		}
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, out_size, &output);
		if (!dst_pck) return GF_SERVICE_ERROR;

		if (ctx->codecid == GF_CODECID_JPEG) {
			e = gf_img_jpeg_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, output, &out_size, ctx->BPP);
		} else {
			e = gf_img_png_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, output, &out_size);
		}

		if (e) {
			gf_filter_pck_discard(dst_pck);
		} else {
			gf_filter_pck_merge_properties(pck, dst_pck);
			gf_filter_pck_send(dst_pck);
		}
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
#endif //GPAC_DISABLE_AV_PARSERS

	return GF_NOT_SUPPORTED;
}


static const GF_FilterCapability ImgDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_JPEG),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_PNG),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister ImgDecRegister = {
	.name = "imgdec",
	GF_FS_SET_DESCRIPTION("PNG/JPG decoder")
	GF_FS_SET_HELP("This filter decodes JPEG and PNG images.")
	.private_size = sizeof(GF_IMGDecCtx),
	.priority = 1,
	SETCAPS(ImgDecCaps),
	.configure_pid = imgdec_configure_pid,
	.process = imgdec_process,
};

const GF_FilterRegister *imgdec_register(GF_FilterSession *session)
{
	return &ImgDecRegister;
}
