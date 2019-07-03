/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / libjpeg encoder filter
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

#ifdef GPAC_HAS_JPEG

#ifdef WIN32
#define HAVE_UNSIGNED_CHAR
#endif

#include <jpeglib.h>
#include <setjmp.h>

typedef struct
{
	//opts
	u32 dctmode;
	u32 quality;

	GF_FilterPid *ipid, *opid;
	u32 width, height, pixel_format, stride, stride_uv, nb_planes, uv_height;

	GF_FilterPacket *dst_pck;
	u8 *output;

	/*io manager*/
	struct jpeg_destination_mgr dst;
	u32 dst_pck_size, max_size;

	struct jpeg_error_mgr pub;
	jmp_buf jmpbuf;

	Bool in_fmt_negotiate;
} GF_JPGEncCtx;

static GF_Err jpgenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	char n[100];
	const GF_PropertyValue *prop;
	GF_JPGEncCtx *ctx = (GF_JPGEncCtx *) gf_filter_get_udta(filter);

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
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_JPEG ));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE_UV, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);

#ifdef JPEG_LIB_VERSION_MAJOR
	sprintf(n, "encjpg:%d.%d", JPEG_LIB_VERSION_MAJOR, JPEG_LIB_VERSION_MINOR);
#else
	sprintf(n, "encjpg:%d", JPEG_LIB_VERSION);
#endif
	
	gf_filter_set_name(filter, n);

	//some props may not be set yet
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


	//TODO: for now we only allow YUV420p input, we should refine this to allow any YUV
	if (ctx->pixel_format != GF_PIXEL_YUV) {
		gf_filter_pid_negociate_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_YUV));
		ctx->in_fmt_negotiate = GF_TRUE;
		return GF_OK;
	}
	ctx->in_fmt_negotiate = GF_FALSE;
	return GF_OK;
}

static void jpgenc_output_message (j_common_ptr cinfo)
{
	if (cinfo) {
		char buffer[JMSG_LENGTH_MAX];
		/* Create the message */
		(*cinfo->err->format_message) (cinfo, buffer);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPGEnc]: %s\n", buffer));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[JPGEnc] coverage test\n"));
	}
}
static void jpgenc_nonfatal_error2(j_common_ptr cinfo, int lev)
{
	if (cinfo) {
		char buffer[JMSG_LENGTH_MAX];
		/* Create the message */
		(*cinfo->err->format_message) (cinfo, buffer);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPGEnc]: %s\n", buffer));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[JPGEnc] coverage test\n"));
	}
}

static void jpgenc_fatal_error(j_common_ptr cinfo)
{
	if (!cinfo) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[JPGEnc] coverage test\n"));
	} else {
		GF_JPGEncCtx *ctx = (GF_JPGEncCtx *) cinfo->client_data;
		jpgenc_output_message(cinfo);
		longjmp(ctx->jmpbuf, 1);
	}
}

//start with 4k when we have no clue about the size
#define ALLOC_STEP_SIZE 4096
static void jpgenc_init_dest(j_compress_ptr cinfo)
{
	GF_JPGEncCtx *ctx = (GF_JPGEncCtx *) cinfo->client_data;
	if (ctx->dst_pck)
		return;

	ctx->dst_pck = gf_filter_pck_new_alloc(ctx->opid, ALLOC_STEP_SIZE, &ctx->output);
    cinfo->dest->next_output_byte = ctx->output;
    cinfo->dest->free_in_buffer = ALLOC_STEP_SIZE;
    ctx->dst_pck_size += ALLOC_STEP_SIZE;
}

static boolean jpgenc_empty_output(j_compress_ptr cinfo)
{
	u8 *data;
	u32 new_size;
	GF_JPGEncCtx *ctx = (GF_JPGEncCtx *) cinfo->client_data;

	if (!ctx->dst_pck)
		return FALSE;

	if (gf_filter_pck_expand(ctx->dst_pck, ALLOC_STEP_SIZE, &ctx->output, &data, &new_size) != GF_OK) {
		return FALSE;
	}
    cinfo->dest->next_output_byte = data;
    cinfo->dest->free_in_buffer = ALLOC_STEP_SIZE;
    ctx->dst_pck_size += ALLOC_STEP_SIZE;
	return TRUE;
}

static void jpgenc_term_dest(j_compress_ptr cinfo)
{
	GF_JPGEncCtx *ctx = (GF_JPGEncCtx *) cinfo->client_data;

    ctx->dst_pck_size -= (u32) cinfo->dest->free_in_buffer;
	gf_filter_pck_truncate(ctx->dst_pck, ctx->dst_pck_size);
}

static GF_Err jpgenc_process(GF_Filter *filter)
{
    GF_JPGEncCtx *ctx = (GF_JPGEncCtx *) gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	struct jpeg_compress_struct cinfo;
	GF_FilterPacket *pck;
	char *in_data;
	GF_FilterFrameInterface *frame_ifce = NULL;
	u32 size, stride, stride_uv;
    u32 i, j;
    u8 *pY, *pU, *pV;
    JSAMPROW y[16],cb[16],cr[16];
    JSAMPARRAY block[3];

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	if (ctx->in_fmt_negotiate) return GF_OK;

	in_data = (char *) gf_filter_pck_get_data(pck, &size);
	if (!in_data) {
		frame_ifce = gf_filter_pck_get_frame_interface(pck);
		if (!frame_ifce || !frame_ifce->get_plane) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NOT_SUPPORTED;
		}
	}

    block[0] = y;
    block[1] = cb;
    block[2] = cr;

	cinfo.err = jpeg_std_error(&(ctx->pub));
	cinfo.client_data = ctx;
	ctx->pub.error_exit = jpgenc_fatal_error;
	ctx->pub.output_message = jpgenc_output_message;
	ctx->pub.emit_message = jpgenc_nonfatal_error2;
	if (setjmp(ctx->jmpbuf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPGEnc] : Failed to encode\n"));
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto exit;
	}

	ctx->dst.init_destination = jpgenc_init_dest;
	ctx->dst.empty_output_buffer = jpgenc_empty_output;
	ctx->dst.term_destination = jpgenc_term_dest;

	if (ctx->max_size) {
		ctx->dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->max_size, &ctx->output);
		ctx->dst.next_output_byte = ctx->output;
		ctx->dst.free_in_buffer = ctx->max_size;
		ctx->dst_pck_size = ctx->max_size;
	}

	jpeg_create_compress(&cinfo);
	cinfo.image_width = ctx->width;
	cinfo.image_height = ctx->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_YCbCr;
	if (ctx->dctmode==0) cinfo.dct_method = JDCT_ISLOW;
	else if (ctx->dctmode==2) cinfo.dct_method = JDCT_FLOAT;
	else cinfo.dct_method = JDCT_IFAST;
	cinfo.optimize_coding = TRUE;
	jpeg_set_defaults (&cinfo);

	cinfo.raw_data_in = TRUE;
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;
#ifdef JPEG_LIB_VERSION_MAJOR
	cinfo.do_fancy_downsampling = FALSE;
#endif
	jpeg_set_colorspace(&cinfo, JCS_YCbCr);
	jpeg_set_quality(&cinfo, MIN(100, ctx->quality), TRUE);

	cinfo.dest = &ctx->dst;

	jpeg_start_compress (&cinfo, TRUE);

	stride = ctx->stride;
	stride_uv = ctx->stride_uv;
	pY = pU = pV = 0;
	if (in_data) {
		pY = in_data;
		pU = pY + ctx->stride * ctx->height;
		pV = pU + ctx->stride_uv * ctx->height/2;
	} else {
		e = frame_ifce->get_plane(frame_ifce, 0, (const u8 **)&pY, &stride);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPGEnc] Failed to fetch first plane in hardware frame\n"));
			goto exit;
		}
		if (ctx->nb_planes>1) {
			e = frame_ifce->get_plane(frame_ifce, 1, (const u8 **)&pU, &stride_uv);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPGEnc] Failed to fetch first plane in hardware frame\n"));
				goto exit;
			}
			if (ctx->nb_planes>2) {
				e = frame_ifce->get_plane(frame_ifce, 2, (const u8 **)&pV, &stride_uv);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPGEnc] Failed to fetch first plane in hardware frame\n"));
					goto exit;
				}
			}
		}
	}

	for (j=0; j<ctx->height; j+=16) {
		for (i=0;i<16;i++) {
			y[i] = pY + stride*(i+j);
			if (i%2 == 0) {
				cb[i/2] = pU + stride_uv*((i+j)/2);
				cr[i/2] = pV + stride_uv*((i+j)/2);
			}
		}
		jpeg_write_raw_data (&cinfo, block, 16);
  	}
    jpeg_finish_compress(&cinfo);

exit:

    jpeg_destroy_compress(&cinfo);
	if (ctx->dst_pck) {
		if (!e) {
			gf_filter_pck_merge_properties(pck, ctx->dst_pck);
			gf_filter_pck_send(ctx->dst_pck);
		} else {
			gf_filter_pck_discard(ctx->dst_pck);
		}
	}
	if (ctx->max_size<ctx->dst_pck_size)
		ctx->max_size = ctx->dst_pck_size;

	ctx->dst_pck = NULL;
	ctx->output = NULL;
	ctx->dst_pck_size = 0;
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err jpgenc_initialize(GF_Filter *filter)
{
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		jpgenc_output_message(NULL);
		jpgenc_nonfatal_error2(NULL, 0);
		jpgenc_fatal_error(NULL);
	}
#endif
	return GF_OK;
}

static const GF_FilterCapability JPGEncCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_JPEG)
};

#define OFFS(_n)	#_n, offsetof(GF_JPGEncCtx, _n)
static GF_FilterArgs JPGEncArgs[] =
{
	{ OFFS(dctmode), "type of DCT used\n"
	"- slow: precise but slow integer DCT\n"
	"- fast: less precise but faster integer DCT\n"
	"- float: float DCT"
	"", GF_PROP_UINT, "fast", "slow|fast|float", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(quality), "compression quality", GF_PROP_UINT, "100", "0-100", GF_FS_ARG_UPDATE},
	{0}
};

GF_FilterRegister JPGEncRegister = {
	.name = "jpgenc",
	GF_FS_SET_DESCRIPTION("JPG encoder")
	GF_FS_SET_HELP("This filter encodes a single uncompressed video PID to JPEG using libjpeg.")
	.private_size = sizeof(GF_JPGEncCtx),
	.args = JPGEncArgs,
	SETCAPS(JPGEncCaps),
	.initialize = jpgenc_initialize,
	.configure_pid = jpgenc_configure_pid,
	.process = jpgenc_process,
};

#endif

const GF_FilterRegister *jpgenc_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_JPEG
	return &JPGEncRegister;
#else
	return NULL;
#endif
}
