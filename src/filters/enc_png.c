/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / libpng encoder filter
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

#ifdef GPAC_HAS_PNG

#include <png.h>

typedef struct
{
	//opts
	u32 dctmode;
	u32 quality;

	GF_FilterPid *ipid, *opid;
	u32 width, height, pixel_format, stride, stride_uv, nb_planes, uv_height;
	u32 nb_alloc_rows;

	u32 max_size, pos, alloc_size;
	u32 png_type;
	png_bytep *row_pointers;

	GF_FilterPacket *dst_pck;
	u8 *output;

} GF_PNGEncCtx;

static GF_Err pngenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_PNGEncCtx *ctx = (GF_PNGEncCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		//one in one out, this is simple
		gf_filter_pid_remove(ctx->opid);
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_PNG ));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);

	gf_filter_set_name(filter, "encpng:"PNG_LIBPNG_VER_STRING );
	//not yeat ready
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (!prop) return GF_OK;
	ctx->width = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (!prop) return GF_OK;
	ctx->height = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (!prop) return GF_OK;
	ctx->pixel_format = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (prop) ctx->stride = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE_UV);
	if (prop) ctx->stride_uv = prop->value.uint;

	gf_pixel_get_size_info(ctx->pixel_format, ctx->width, ctx->height, NULL, &ctx->stride, &ctx->stride_uv, &ctx->nb_planes, &ctx->uv_height);

	switch (ctx->pixel_format) {
	case GF_PIXEL_GREYSCALE:
		ctx->png_type = PNG_COLOR_TYPE_GRAY;
		break;
	case GF_PIXEL_GREYALPHA:
		ctx->png_type = PNG_COLOR_TYPE_GRAY_ALPHA;
		break;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XBGR:
		ctx->png_type = PNG_COLOR_TYPE_RGB;
		break;
	case GF_PIXEL_RGBA:
		ctx->png_type = PNG_COLOR_TYPE_RGB_ALPHA;
		break;
	default:
		gf_filter_pid_negociate_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));
		break;
	}
	if (ctx->height > ctx->nb_alloc_rows) {
		ctx->nb_alloc_rows = ctx->height;
		ctx->row_pointers = gf_realloc(ctx->row_pointers, sizeof(png_bytep) * ctx->height);
	}
	return GF_OK;
}

static void pngenc_finalize(GF_Filter *filter)
{
	GF_PNGEncCtx *ctx = (GF_PNGEncCtx *) gf_filter_get_udta(filter);
	if (ctx->row_pointers) gf_free(ctx->row_pointers);
}

#define PNG_BLOCK_SIZE	4096

static void pngenc_write(png_structp png, png_bytep data, png_size_t size)
{
	GF_PNGEncCtx *ctx = (GF_PNGEncCtx *)png_get_io_ptr(png);
	if (!ctx->dst_pck) {
		while (ctx->alloc_size<size) ctx->alloc_size+=PNG_BLOCK_SIZE;
		ctx->dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->alloc_size, &ctx->output);
	} else if (ctx->pos + size > ctx->alloc_size) {
		u8 *data;
		u32 new_size;
		u32 old_size = ctx->alloc_size;
		while (ctx->pos + size > ctx->alloc_size)
			ctx->alloc_size += PNG_BLOCK_SIZE;
		
		if (gf_filter_pck_expand(ctx->dst_pck, ctx->alloc_size - old_size, &ctx->output, &data, &new_size) != GF_OK) {
			return;
		}
	}

	memcpy(ctx->output + ctx->pos, data, sizeof(char)*size);
	ctx->pos += (u32) size;
}

void pngenc_flush(png_structp png)
{
	if (!png) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[PNGEnc] coverage test\n"));
	}
}

static void pngenc_error(png_structp cbk, png_const_charp msg)
{
	if (msg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[PNGEnc] Error %s", msg));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[PNGEnc] coverage test\n"));
	}
}
static void pngenc_warn(png_structp cbk, png_const_charp msg)
{
	if (msg) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[PNGEnc] Warning %s", msg));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[PNGEnc] coverage test\n"));
	}
}

static GF_Err pngenc_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	GF_PNGEncCtx *ctx = (GF_PNGEncCtx *) gf_filter_get_udta(filter);
	png_color_8 sig_bit;
	u32 k;
	GF_Err e = GF_OK;
	png_structp png_ptr;
	png_infop info_ptr;
	char *in_data;
	u32 size, stride;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	stride = ctx->stride;
	in_data = (char *) gf_filter_pck_get_data(pck, &size);
	if (!in_data) {
		GF_FilterFrameInterface *frame_ifce = gf_filter_pck_get_frame_interface(pck);
		if (!frame_ifce || !frame_ifce->get_plane) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NOT_SUPPORTED;
		}
		e = frame_ifce->get_plane(frame_ifce, 0, (const u8 **) &in_data, &stride);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[PNGEnc] Failed to fetch first plane in hardware frame\n"));
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NOT_SUPPORTED;
		}
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, ctx, pngenc_error, pngenc_warn);

	if (png_ptr == NULL) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

	/* Allocate/initialize the image information data.  REQUIRED */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

	/* Set error handling.  REQUIRED if you aren't supplying your own
	* error handling functions in the png_create_write_struct() call.
	*/
	if (setjmp(png_jmpbuf(png_ptr))) {
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit;
	}

	ctx->output = NULL;
	ctx->pos = 0;
	if (ctx->max_size) {
		ctx->dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->max_size, &ctx->output);
		ctx->alloc_size = ctx->max_size;
	}
	png_set_write_fn(png_ptr, ctx, pngenc_write, pngenc_flush);

	png_set_IHDR(png_ptr, info_ptr, ctx->width, ctx->height, 8, ctx->png_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	memset(&sig_bit, 0, sizeof(sig_bit));
	switch (ctx->png_type) {
	case PNG_COLOR_TYPE_GRAY:
		sig_bit.gray = 8;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		sig_bit.gray = 8;
		sig_bit.alpha = 8;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		sig_bit.alpha = 8;
	case PNG_COLOR_TYPE_RGB:
		sig_bit.red = 8;
		sig_bit.green = 8;
		sig_bit.blue = 8;
		break;
	default:
		break;
	}
		sig_bit.gray = 8;
		sig_bit.alpha = 8;
		sig_bit.red = 8;
		sig_bit.green = 8;
		sig_bit.blue = 8;
	png_set_sBIT(png_ptr, info_ptr, &sig_bit);

	//todo add support for tags
#if 0
	{
	png_text text_ptr[3];
	/* Optionally write comments into the image */
	text_ptr[0].key = "Title";
	text_ptr[0].text = "Mona Lisa";
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[1].key = "Author";
	text_ptr[1].text = "Leonardo DaVinci";
	text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[2].key = "Description";
	text_ptr[2].text = "<long text>";
	text_ptr[2].compression = PNG_TEXT_COMPRESSION_zTXt;
	png_set_text(png_ptr, info_ptr, text_ptr, 3);
	}
#endif

	png_write_info(png_ptr, info_ptr);

	/* Shift the pixels up to a legal bit depth and fill in
	* as appropriate to correctly scale the image.
	*/
	png_set_shift(png_ptr, &sig_bit);

	/* pack pixels into bytes */
	png_set_packing(png_ptr);

	switch (ctx->pixel_format) {
	case GF_PIXEL_ARGB:
		png_set_bgr(png_ptr);
		break;
	case GF_PIXEL_RGBX:
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
		png_set_bgr(png_ptr);
		break;
	case GF_PIXEL_BGRX:
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
		break;
	case GF_PIXEL_BGR:
		png_set_bgr(png_ptr);
		break;
	}
	for (k=0; k<ctx->height; k++) {
		ctx->row_pointers[k] = (png_bytep) in_data + k*stride;
	}

	png_write_image(png_ptr, ctx->row_pointers);
	png_write_end(png_ptr, info_ptr);

exit:
	/* clean up after the write, and free any memory allocated */
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (ctx->dst_pck) {
		if (!e) {
			gf_filter_pck_truncate(ctx->dst_pck, ctx->pos);
			gf_filter_pck_merge_properties(pck, ctx->dst_pck);
			gf_filter_pck_send(ctx->dst_pck);
		} else {
			gf_filter_pck_discard(ctx->dst_pck);
		}
	}
	if (ctx->max_size<ctx->pos)
		ctx->max_size = ctx->pos;

	ctx->dst_pck = NULL;
	ctx->output = NULL;
	ctx->pos = ctx->alloc_size = 0;
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err pngenc_initialize(GF_Filter *filter)
{
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		pngenc_flush(NULL);
		pngenc_error(NULL, NULL);
		pngenc_warn(NULL, NULL);
	}
#endif
	return GF_OK;
}

static const GF_FilterCapability PNGEncCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_PNG)
};

GF_FilterRegister PNGEncRegister = {
	.name = "pngenc",
	GF_FS_SET_DESCRIPTION("PNG encoder")
	GF_FS_SET_HELP("This filter encodes a single uncompressed video PID to PNG using libpng.")
	.private_size = sizeof(GF_PNGEncCtx),
	.initialize = pngenc_initialize,
	.finalize = pngenc_finalize,
	SETCAPS(PNGEncCaps),
	.configure_pid = pngenc_configure_pid,
	.process = pngenc_process,
};

#endif

const GF_FilterRegister *pngenc_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_PNG
	return &PNGEncRegister;
#else
	return NULL;
#endif
}
