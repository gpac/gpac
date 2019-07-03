/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / NVidia Hardware decoder filter
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

#include <gpac/thread.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/filters.h>

#if (defined(WIN32) || defined(GPAC_CONFIG_LINUX) || defined(GPAC_CONFIG_DARWIN)) 

#include "dec_nvdec_sdk.h"

#ifndef GPAC_DISABLE_3D


#ifdef LOAD_GL_1_5
GLDECL_EXTERN(glGenBuffers);
GLDECL_EXTERN(glBindBuffer);
GLDECL_EXTERN(glBufferData);
#endif

#endif

typedef struct _nv_dec_inst NVDecInstance;

enum
{
	NVDEC_COPY = 0,
	NVDEC_SINGLE,
	NVDEC_GL
};

enum
{
	NVDEC_CUVID = 0,
	NVDEC_CUDA,
	NVDEC_DXVA
};

typedef struct _nv_dec_ctx
{
	u32 unload;
	u32 vmode, fmode;
	u32 num_surfaces;

	GF_FilterPid *ipid, *opid;
	u32 codec_id;
	Bool use_gl_texture;
	u32 width, height, bpp_luma, bpp_chroma;
	cudaVideoCodec codec_type;
	cudaVideoChromaFormat chroma_fmt;

	u32 out_size, stride, pix_fmt, stride_uv, nb_planes, uv_height;
	u32 reload_decoder_state;
	Bool skip_next_frame;
	CUresult decode_error, dec_create_error;
	Bool frame_size_changed;
	Bool needs_resetup;
	unsigned long prefer_dec_mode;

	NVDecInstance *dec_inst;

	GF_List *frames;
	GF_List *frames_res;
	GF_List *src_packets;

	struct __nv_frame *pending_frame;


	char *single_frame_data;
	u32 single_frame_data_alloc;

#ifndef GPAC_DISABLE_3D
	GLint y_tx_id, uv_tx_id;
	GLint y_pbo_id, uv_pbo_id;
#endif
} NVDecCtx;


struct _nv_dec_inst
{
	u32 width, height, bpp_luma, bpp_chroma, stride;
	cudaVideoCodec codec_type;
	cudaVideoChromaFormat chroma_fmt;
	u32 id;
	u32 th_id;

	//allocated video parser and decoder
	CUvideoparser cu_parser;
	CUvideodecoder cu_decoder;

	//current associated context, 0 is none
	NVDecCtx *ctx;
};


typedef struct __nv_frame
{
	CUVIDPARSERDISPINFO frame_info;
	NVDecCtx *ctx;
	GF_FilterFrameInterface gframe;
	Bool y_mapped, uv_mapped;
} NVDecFrame;

static GF_List *global_unactive_decoders=NULL;
static u32 global_nb_loaded_nvdec = 0;
static u32 global_nb_loaded_decoders = 0;
static GF_Mutex *global_inst_mutex = NULL;
static CUdevice  cuda_dev = -1;
static CUcontext cuda_ctx = NULL;
static Bool cuda_ctx_gl = GF_FALSE;


//#define ENABLE_10BIT_OUTPUT

static GF_Err nvdec_init_decoder(NVDecCtx *ctx)
{
	CUresult res;
	CUVIDDECODECREATEINFO cuvid_info;

	assert(ctx->dec_inst);

	memset(&cuvid_info, 0, sizeof(CUVIDDECODECREATEINFO));
	cuvid_info.CodecType = ctx->codec_type;
	cuvid_info.ulWidth = ctx->width;
	cuvid_info.ulHeight = ctx->height;
	cuvid_info.ulNumDecodeSurfaces = ctx->num_surfaces;
	cuvid_info.ChromaFormat = ctx->chroma_fmt;
	cuvid_info.OutputFormat = cudaVideoSurfaceFormat_NV12;
#ifdef ENABLE_10BIT_OUTPUT
	if (ctx->bpp_luma + ctx->bpp_chroma > 16)
		cuvid_info.OutputFormat = cudaVideoSurfaceFormat_P016;
#endif
    cuvid_info.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;
	cuvid_info.bitDepthMinus8 = ctx->bpp_luma - 8;
	cuvid_info.ulTargetWidth = ctx->width;
	cuvid_info.ulTargetHeight = ctx->height;
    cuvid_info.display_area.left = 0;
    cuvid_info.display_area.right = ctx->width;
    cuvid_info.display_area.top = 0;
	cuvid_info.display_area.bottom = ctx->height;

    cuvid_info.ulNumOutputSurfaces = 1;
	cuvid_info.ulCreationFlags = ctx->prefer_dec_mode;

    // create the decoder
	res = cuvidCreateDecoder(&ctx->dec_inst->cu_decoder, &cuvid_info);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to create cuvid decoder %s\n", cudaGetErrorEnum(res) ) );
		ctx->dec_create_error = res;
		return GF_IO_ERR;
	}
	global_nb_loaded_decoders++;
	assert(global_nb_loaded_decoders);
	ctx->dec_inst->id = global_nb_loaded_decoders;
	ctx->dec_inst->th_id = gf_th_id();
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] decoder instance %d created (%dx%d) - %d total decoders loaded\n", ctx->dec_inst->id, ctx->width, ctx->height, global_nb_loaded_decoders) );
	return GF_OK;
}

Bool load_inactive_dec(NVDecCtx *ctx)
{
	u32 i, count;
	//look for unactive decoder with same settings
	if (global_unactive_decoders) {

		gf_mx_p(global_inst_mutex);
		count = gf_list_count(global_unactive_decoders);
		for (i=0; i<count; i++) {
			NVDecInstance *inst = gf_list_get(global_unactive_decoders, i);
			if ((inst->width==ctx->width) && (inst->height==ctx->height) && (inst->bpp_luma == ctx->bpp_luma )
				&& (inst->bpp_chroma == ctx->bpp_chroma ) && (inst->codec_type == ctx->codec_type) && (inst->chroma_fmt == ctx->chroma_fmt )
				) {

					gf_list_rem(global_unactive_decoders, i);
					ctx->dec_inst = inst;
					inst->ctx = ctx;
					gf_mx_v(global_inst_mutex);
					return GF_TRUE;
			}
		}
		if (ctx->dec_inst && !ctx->dec_inst->cu_decoder) {
			ctx->dec_inst->ctx = ctx;
			gf_mx_v(global_inst_mutex);
			return GF_FALSE;
		}
		if (ctx->dec_inst) {
			NVDecInstance *inst = ctx->dec_inst;
			if ((inst->width==ctx->width) && (inst->height==ctx->height) && (inst->bpp_luma == ctx->bpp_luma )
				&& (inst->bpp_chroma == ctx->bpp_chroma ) && (inst->codec_type == ctx->codec_type) && (inst->chroma_fmt == ctx->chroma_fmt )
				) {
				ctx->dec_inst = inst;
				inst->ctx = ctx;
				gf_mx_v(global_inst_mutex);
				return GF_TRUE;
			}
		} else {
			ctx->dec_inst = gf_list_pop_back(global_unactive_decoders);
		}
		gf_mx_v(global_inst_mutex);
	}
	if (!ctx->dec_inst) {
		GF_SAFEALLOC(ctx->dec_inst, NVDecInstance);
	}
	ctx->dec_inst->ctx = ctx;
	return GF_FALSE;
}

static void nvdec_destroy_decoder(NVDecInstance *inst)
{
	if (inst->cu_decoder) {
		cuvidDestroyDecoder(inst->cu_decoder);
		inst->cu_decoder = NULL;
		global_nb_loaded_decoders--;
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] decoder instance %d destruction - %d decoders still loaded\n", inst->id, global_nb_loaded_decoders ) );
	}
}

static void update_pix_fmt(NVDecCtx *ctx, Bool use_10bits)
{
	switch (ctx->chroma_fmt) {
	case cudaVideoChromaFormat_420:
		ctx->pix_fmt = use_10bits ? GF_PIXEL_NV12_10 : GF_PIXEL_NV12;
		break;
	case cudaVideoChromaFormat_422:
		ctx->pix_fmt = use_10bits  ? GF_PIXEL_YUV422_10 : GF_PIXEL_YUV422;
		break;
	case cudaVideoChromaFormat_444:
		ctx->pix_fmt = use_10bits  ? GF_PIXEL_YUV444_10 : GF_PIXEL_YUV444;
		break;
	default:
		ctx->pix_fmt = 0;
		return;
	}
	gf_pixel_get_size_info(ctx->pix_fmt, ctx->width, ctx->height, &ctx->out_size, &ctx->stride, &ctx->stride_uv, &ctx->nb_planes, &ctx->uv_height);
}

static int CUDAAPI HandleVideoSequence(void *pUserData, CUVIDEOFORMAT *pFormat)
{
	Bool use_10bits=GF_FALSE;
	Bool skip_output_resize=GF_FALSE;
	NVDecInstance *inst= (NVDecInstance *)pUserData;
	NVDecCtx *ctx = inst->ctx;

	u32 w, h;
	w = pFormat->coded_width;
	h = pFormat->coded_height;

	if (pFormat->display_area.right && (pFormat->display_area.right<(s32)w)) w = pFormat->display_area.right;
	if (pFormat->display_area.bottom && (pFormat->display_area.bottom<(s32)h)) h = pFormat->display_area.bottom;

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] Decoder instance %d Video sequence change detected - new setup %u x %u, %u bpp\n", inst->id, pFormat->coded_width, pFormat->coded_height, pFormat->bit_depth_luma_minus8 + 8) );
	//no change in config
	if ((ctx->width == w)
		&& (ctx->height == h)
		&& (ctx->bpp_luma == 8 + pFormat->bit_depth_luma_minus8)
		&& (ctx->bpp_chroma == 8 + pFormat->bit_depth_chroma_minus8)
		&& (ctx->codec_type == pFormat->codec)
		&& (ctx->chroma_fmt == pFormat->chroma_format)
	) {
		if (ctx->dec_inst && ctx->dec_inst->cu_decoder)
			return 1;
		skip_output_resize = GF_TRUE;
	}

	//commented out since this falls back to soft decoding !
#ifdef ENABLE_10BIT_OUTPUT
	if (ctx->bpp_luma + ctx->bpp_chroma > 16)  use_10bits = GF_TRUE;
#endif

	ctx->width = w;
	ctx->height = h;
	ctx->bpp_luma = 8 + pFormat->bit_depth_luma_minus8;
	ctx->bpp_chroma = 8 + pFormat->bit_depth_chroma_minus8;
	ctx->codec_type = pFormat->codec;
	ctx->chroma_fmt = pFormat->chroma_format;
	ctx->stride = pFormat->coded_width;

	//if load_inatcive returns TRUE, we are reusing an existing decoder with the same config, no need to recreate one
	if (load_inactive_dec(ctx)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] reusing inactive decoder %dx%d - %d total decoders loaded\n", ctx->width, ctx->height, global_nb_loaded_decoders) );
		ctx->stride = ctx->dec_inst->stride;
		//initial config, need to trigger output resize
		if (!ctx->out_size) ctx->reload_decoder_state = 1;

		update_pix_fmt(ctx, use_10bits);
		return GF_OK;
	}
	if (!ctx->dec_inst) return GF_OUT_OF_MEM;
	//if we have an existing decoder but with a different config, let's reload
	nvdec_destroy_decoder(ctx->dec_inst);

	ctx->dec_inst->width = ctx->width;
	ctx->dec_inst->height = ctx->height;
	ctx->dec_inst->bpp_luma = ctx->bpp_luma;
	ctx->dec_inst->bpp_chroma = ctx->bpp_chroma;
	ctx->dec_inst->codec_type = ctx->codec_type;
	ctx->dec_inst->chroma_fmt = ctx->chroma_fmt;
	ctx->dec_inst->ctx = ctx;
	ctx->stride = use_10bits ? 2*ctx->width : ctx->width;

	update_pix_fmt(ctx, use_10bits);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->stride));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BIT_DEPTH_Y, &PROP_UINT(use_10bits ? ctx->bpp_luma : 8));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BIT_DEPTH_UV, &PROP_UINT(use_10bits ? ctx->bpp_chroma : 8));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pix_fmt));


	assert(ctx->out_size);
	assert(ctx->stride);
	ctx->dec_inst->stride = ctx->stride;

	if (! ctx->dec_inst->cu_decoder) {
		nvdec_init_decoder(ctx);
		if (!skip_output_resize) {
			ctx->reload_decoder_state = 1;
		}
	} else {
		ctx->reload_decoder_state = 2;
	}
	return 1;
}

static int CUDAAPI HandlePictureDecode(void *pUserData, CUVIDPICPARAMS *pPicParams)
{
	NVDecInstance *inst = (NVDecInstance *)pUserData;
	inst->ctx->decode_error = cuvidDecodePicture(inst->cu_decoder, pPicParams);

	if (inst->ctx->decode_error != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] decoder instance %d failed to decode picture %s\n", inst->id, cudaGetErrorEnum(inst->ctx->decode_error) ) );
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[NVDec] decoded picture %u OK\n", pPicParams->CurrPicIdx ) );

	return 1;
}

static int CUDAAPI HandlePictureDisplay(void *pUserData, CUVIDPARSERDISPINFO *pPicParams)
{
	u32 i, count;
	NVDecFrame *f;
	NVDecInstance *inst = (NVDecInstance *)pUserData;
	NVDecCtx *ctx = (NVDecCtx *)inst->ctx;

	if (pPicParams->timestamp > 0xFFFFFFFF) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[NVDec] picture %u CTS %u seek flag, discarding\n", pPicParams->picture_index, (u32) (pPicParams->timestamp & 0xFFFFFFFF)) );
		return 1;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[NVDec] picture %u CTS %u ready for display, queuing it\n", pPicParams->picture_index, (u32) (pPicParams->timestamp & 0xFFFFFFFF)) );

	f = gf_list_pop_back(ctx->frames_res);
	if (!f) {
		GF_SAFEALLOC(f, NVDecFrame);
	}
	f->frame_info = *pPicParams;
	f->frame_info.timestamp = (u32) (pPicParams->timestamp & 0xFFFFFFFF);
	f->ctx = ctx;
	count = gf_list_count(ctx->frames);
	for (i=0; i<count; i++) {
		NVDecFrame *af = gf_list_get(ctx->frames, i);
		if (af->frame_info.timestamp > f->frame_info.timestamp) {
			gf_list_insert(ctx->frames, f, i);
			return 1;
		}
	}
	gf_list_add(ctx->frames, f);
	return 1;
}

static GF_Err nvdec_configure_stream(GF_Filter *filter, NVDecCtx *ctx)
{
	CUresult res;
    CUVIDPARSERPARAMS oVideoParserParameters;

	//create a video parser and a video decoder
    memset(&oVideoParserParameters, 0, sizeof(CUVIDPARSERPARAMS));
	ctx->needs_resetup = GF_FALSE;

	switch (ctx->codec_id) {
	case GF_CODECID_MPEG1:
		ctx->codec_type = cudaVideoCodec_MPEG1;
		break;
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_SPATIAL:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_422:
		ctx->codec_type = cudaVideoCodec_MPEG2;
		break;
	case GF_CODECID_MPEG4_PART2:
		ctx->codec_type = cudaVideoCodec_MPEG4;
		break;
	case GF_CODECID_AVC:
		ctx->codec_type = cudaVideoCodec_H264;
		break;
	case GF_CODECID_HEVC:
		ctx->codec_type = cudaVideoCodec_HEVC;
		break;
	}

	if (load_inactive_dec(ctx)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] reusing inactive decoder %dx%d - %d total decoders loaded\n", ctx->width, ctx->height, global_nb_loaded_decoders ) );
		ctx->stride = ctx->dec_inst->stride;
	}
	if (!ctx->dec_inst) return GF_OUT_OF_MEM;

	ctx->decode_error = CUDA_SUCCESS;

	oVideoParserParameters.CodecType = ctx->codec_type;
    oVideoParserParameters.ulMaxNumDecodeSurfaces = ctx->num_surfaces;
    oVideoParserParameters.ulMaxDisplayDelay = 4;
	oVideoParserParameters.ulClockRate = 1000;
    oVideoParserParameters.pExtVideoInfo = NULL;
    oVideoParserParameters.pfnSequenceCallback = HandleVideoSequence;    // Called before decoding frames and/or whenever there is a format change
    oVideoParserParameters.pfnDecodePicture = HandlePictureDecode;    // Called when a picture is ready to be decoded (decode order)
    oVideoParserParameters.pfnDisplayPicture = HandlePictureDisplay;   // Called whenever a picture is ready to be displayed (display order)
	oVideoParserParameters.pUserData = ctx->dec_inst;

    res = cuCtxPushCurrent(cuda_ctx);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to push CUDA CTX %s\n", cudaGetErrorEnum(res) ) );
	}
	res = cuvidCreateVideoParser(&ctx->dec_inst->cu_parser, &oVideoParserParameters);
	cuCtxPopCurrent(NULL);

	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to create CUVID parserCTX %s\n", cudaGetErrorEnum(res) ) );
		return GF_PROFILE_NOT_SUPPORTED;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[NVDec] video parser init OK\n") );

	switch (ctx->codec_type) {
	case cudaVideoCodec_MPEG1:
		gf_filter_set_name(filter, ctx->use_gl_texture ? "NVidia HWGL MPEG-1" : "NVidia HW MPEG-1");
		break;
	case cudaVideoCodec_MPEG2:
		gf_filter_set_name(filter, ctx->use_gl_texture ? "NVidia HWGLMPEG-2" : "NVidia HW MPEG-2");
		break;
	case cudaVideoCodec_MPEG4:
		gf_filter_set_name(filter, ctx->use_gl_texture ? "NVidia HWGL MPEG-4 part2" : "NVidia HW MPEG-4 part2");
		break;
	case cudaVideoCodec_H264:
		gf_filter_set_name(filter, ctx->use_gl_texture ? "NVidia HWGL AVC|H264" : "NVidia HW AVC|H264");
		break;
	case cudaVideoCodec_HEVC:
		gf_filter_set_name(filter, ctx->use_gl_texture ? "NVidia HWGL HEVC" : "NVidia HW HEVC");
		break;
	case cudaVideoCodec_VC1:
		gf_filter_set_name(filter, ctx->use_gl_texture ? "NVidia HWGL VC1" : "NVidia HW VC1");
		break;
	default:
		break;
	}
	return GF_OK;
}

static GF_Err nvdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *cid;
	CUresult res;
	NVDecCtx *ctx = (NVDecCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		ctx->ipid = NULL;

		if (ctx->unload == 2) {
			global_nb_loaded_nvdec--;
			if (ctx->dec_inst) {
				assert(global_unactive_decoders);
				gf_mx_p(global_inst_mutex);
				ctx->dec_inst->ctx = NULL;
				gf_list_add(global_unactive_decoders, ctx->dec_inst);
				ctx->dec_inst = NULL;
				gf_mx_v(global_inst_mutex);
			}
		}
	}
	if (ctx->ipid && (ctx->ipid != pid)) return GF_REQUIRES_NEW_INSTANCE;

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;
	ctx->ipid = pid;
	ctx->use_gl_texture = (ctx->fmode== NVDEC_GL) ? GF_TRUE : GF_FALSE;

	cid = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!cid) return GF_NOT_SUPPORTED;
	ctx->codec_id = cid->value.uint;
	switch (ctx->codec_id) {
	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_SPATIAL:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG4_PART2:
		break;
	case GF_CODECID_AVC:
	case GF_CODECID_HEVC:
		cid = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (cid) return GF_BAD_PARAM;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );

#ifdef GPAC_DISABLE_3D
	ctx->use_gl_texture = GF_FALSE;
#endif

	if (! cuda_ctx) {
	    int major, minor;
	    char deviceName[256];
		res = cuDeviceGet(&cuda_dev, 0);
		if (res != CUDA_SUCCESS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to init cuda device %s\n", cudaGetErrorEnum(res) ) );
			return GF_IO_ERR;
		}

		cuDeviceComputeCapability(&major, &minor, cuda_dev);
		cuDeviceGetName(deviceName, 256, cuda_dev);

		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] GPU Device %s (idx 0) has SM %d.%d compute capability\n", deviceName, major, minor));

		if (ctx->use_gl_texture) {
#ifndef GPAC_DISABLE_3D
			res = cuGLCtxCreate(&cuda_ctx, CU_CTX_BLOCKING_SYNC, cuda_dev);

#ifdef LOAD_GL_1_5
			GET_GLFUN(glGenBuffers);
			GET_GLFUN(glBindBuffer);
			GET_GLFUN(glBufferData);
#endif
			cuda_ctx_gl = GF_TRUE;

#endif
		} else {
			res = cuCtxCreate(&cuda_ctx, CU_CTX_BLOCKING_SYNC, cuda_dev);
			cuda_ctx_gl = GF_FALSE;
		}
		if (res != CUDA_SUCCESS) {
			if (ctx->use_gl_texture) {
				cuda_ctx_gl = GF_FALSE;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec] Couldn't initialize cuda OpenGL context %s\n\tcheck you started the player without compositor thread (-no-cthread option)\n\tRetrying without OpenGL support\n", cudaGetErrorEnum(res) ) );
				res = cuCtxCreate(&cuda_ctx, CU_CTX_BLOCKING_SYNC, cuda_dev);
				if (res != CUDA_SUCCESS) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to init cuda context %s\n", cudaGetErrorEnum(res) ) );
				} else {
					ctx->use_gl_texture = GF_FALSE;
				}
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to init cuda context %s\n", cudaGetErrorEnum(res) ) );
			}

			if (res != CUDA_SUCCESS) {
				return GF_IO_ERR;
			}
		}
	}

	if (ctx->vmode == NVDEC_DXVA)
		ctx->prefer_dec_mode = cudaVideoCreate_PreferDXVA;
	else if (ctx->vmode == NVDEC_CUDA)
		ctx->prefer_dec_mode = cudaVideoCreate_PreferCUDA;
	else
		ctx->prefer_dec_mode = cudaVideoCreate_PreferCUVID;

	if (ctx->unload == 2) {
		global_nb_loaded_nvdec++;
		if (!global_inst_mutex ) global_inst_mutex  = gf_mx_new("NVDecGlobal");
		gf_mx_p(global_inst_mutex);
		if (!global_unactive_decoders) global_unactive_decoders = gf_list_new();
		gf_mx_v(global_inst_mutex);
	}


	ctx->needs_resetup = GF_TRUE;

	return GF_OK;
}

static Bool nvdec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	NVDecCtx *ctx = (NVDecCtx *)gf_filter_get_udta(filter);
	if (evt->base.type == GF_FEVT_PLAY) {
		while (gf_list_count(ctx->frames)) {
			NVDecFrame *f = gf_list_pop_back(ctx->frames);
			gf_list_add(ctx->frames_res, f);
		}
	}
	return GF_FALSE;
}


#if 0
	case GF_CODEC_ABORT:
		while (gf_list_count(ctx->frames)) {
			NVDecFrame *f = (NVDecFrame *) gf_list_pop_back(ctx->frames);
			memset(f, 0, sizeof(NVDecFrame));
			gf_list_add(ctx->frames_res, f);
		}
		if (ctx->unload == 2) {
			if (ctx->dec_inst) {
				assert(global_unactive_decoders);
				gf_mx_p(global_inst_mutex);
				if (ctx->decode_error) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] deactivating decoder %dx%d and destroying instance\n", ctx->width, ctx->height ) );
					nvdec_destroy_decoder(ctx->dec_inst);
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] deactivating decoder %dx%d\n", ctx->width, ctx->height ) );
				}
				ctx->dec_inst->ctx = NULL;
				gf_list_add(global_unactive_decoders, ctx->dec_inst);
				ctx->dec_inst = NULL;
				gf_mx_v(global_inst_mutex);
			}
			ctx->needs_resetup = GF_TRUE;
			ctx->dec_create_error = CUDA_SUCCESS;
		} else if (ctx->unload == 1) {
			if (ctx->dec_inst) {
				nvdec_destroy_decoder(ctx->dec_inst);
			}
			ctx->needs_resetup = GF_TRUE;
			ctx->dec_create_error = CUDA_SUCCESS;
		}
		return GF_OK;
	}
#endif

static GF_Err nvdec_send_hw_frame(NVDecCtx *ctx);

static void nvdec_reset_pcks(NVDecCtx *ctx)
{
	while (gf_list_count(ctx->src_packets)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
		gf_filter_pck_unref(pck);
	}
}

static void nvdec_merge_pck_props(NVDecCtx *ctx, NVDecFrame *f,  GF_FilterPacket *dst_pck)
{
	u32 i, count;
	GF_FilterPacket *src_pck = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i = 0; i<count; i++) {
		src_pck = gf_list_get(ctx->src_packets, i);
		if (gf_filter_pck_get_cts(src_pck) == f->frame_info.timestamp) {
			gf_filter_pck_merge_properties(src_pck, dst_pck);
			gf_list_rem(ctx->src_packets, i);
			gf_filter_pck_unref(src_pck);
			return;
		}
	}
	//not found !
	gf_filter_pck_set_cts(dst_pck, f->frame_info.timestamp);
	if (!gf_filter_pck_get_interlaced(dst_pck) && !f->frame_info.progressive_frame) {
		gf_filter_pck_set_interlaced(dst_pck, f->frame_info.top_field_first ? 1 : 2);
	}
}

static GF_Err nvdec_process(GF_Filter *filter)
{
	NVDecFrame *f;
    CUdeviceptr map_mem = 0;
	CUVIDPROCPARAMS params;
	unsigned int pitch = 0;
	GF_Err e;
	u32 pck_size;
	const u8 *data;
	u8 *output;
	GF_FilterPacket *ipck, *dst_pck;
    CUVIDSOURCEDATAPACKET cu_pkt;
	CUresult res;
	NVDecCtx *ctx = (NVDecCtx *) gf_filter_get_udta(filter);

	if (ctx->needs_resetup) {
		nvdec_configure_stream(filter, ctx);
	}

	ipck = gf_filter_pid_get_packet(ctx->ipid);

	memset(&cu_pkt, 0, sizeof(CUVIDSOURCEDATAPACKET));
	cu_pkt.flags = CUVID_PKT_TIMESTAMP;
	pck_size = 0;
	data = NULL;
	if (!ipck) {
		if (gf_filter_pid_is_eos(ctx->ipid))
			cu_pkt.flags |= CUVID_PKT_ENDOFSTREAM;
		ctx->skip_next_frame = GF_FALSE;
	} else {
		data = gf_filter_pck_get_data(ipck, &pck_size);
	}

	if (ctx->dec_create_error) {
		if (ipck) gf_filter_pid_drop_packet(ctx->ipid);
		else if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_IO_ERR;
	}

	cu_pkt.payload_size = pck_size;
	cu_pkt.payload = data;
	if (ipck) cu_pkt.timestamp = gf_filter_pck_get_cts(ipck);

    res = cuCtxPushCurrent(cuda_ctx);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to push CUDA CTX %s\n", cudaGetErrorEnum(res) ) );
	}
	if (ctx->skip_next_frame) {
		ctx->skip_next_frame = GF_FALSE;
	} else {
		res = cuvidParseVideoData(ctx->dec_inst->cu_parser, &cu_pkt);
		if (res != CUDA_SUCCESS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] decoder instance %d failed to parse data %s\n", ctx->dec_inst->id, cudaGetErrorEnum(res) ) );
		}
	}

	//queue reference to source packet props
	if (ipck) {
		gf_filter_pck_ref_props(&ipck);
		gf_list_add(ctx->src_packets, ipck);
	}

	e = GF_OK;
	if (ctx->reload_decoder_state) {
		if (ctx->reload_decoder_state==2) {
			nvdec_destroy_decoder(ctx->dec_inst);
		} else {
			ctx->skip_next_frame = GF_TRUE;
		}

		ctx->reload_decoder_state = 0;
		if (!ctx->out_size || !ctx->pix_fmt) {
			cuCtxPopCurrent(NULL);
			return GF_NOT_SUPPORTED;
		}

		//need to setup decoder
		if (! ctx->dec_inst->cu_decoder) {
			nvdec_init_decoder(ctx);
		}
		cuCtxPopCurrent(NULL);
		return GF_OK;
	}
	//drop packet
	if (ipck)
		gf_filter_pid_drop_packet(ctx->ipid);

	f = gf_list_pop_front(ctx->frames);
	if (!f) {
		cuCtxPopCurrent(NULL);
		if (!ipck && gf_filter_pid_is_eos(ctx->ipid)) {
			nvdec_reset_pcks(ctx);
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	if (ctx->use_gl_texture || (ctx->fmode==NVDEC_SINGLE) ) {
		assert(!ctx->pending_frame);
		ctx->pending_frame = f;
		return nvdec_send_hw_frame(ctx);
	}

	assert(ctx->out_size);
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &output);

	memset(&params, 0, sizeof(params));
	params.progressive_frame = f->frame_info.progressive_frame;
	params.top_field_first = f->frame_info.top_field_first;

	nvdec_merge_pck_props(ctx, f, dst_pck);
	e = GF_OK;
	if (gf_filter_pck_get_seek_flag(dst_pck)) {
		gf_filter_pck_discard(dst_pck);
	} else {
		res = cuvidMapVideoFrame(ctx->dec_inst->cu_decoder, f->frame_info.picture_index, &map_mem, &pitch, &params);
		if (res == CUDA_SUCCESS) {
			CUDA_MEMCPY2D mcpi;
			memset(&mcpi, 0, sizeof(CUDA_MEMCPY2D));
			mcpi.srcMemoryType = CU_MEMORYTYPE_DEVICE;
			mcpi.srcDevice = map_mem;
			mcpi.srcPitch = pitch;

			mcpi.dstMemoryType = CU_MEMORYTYPE_HOST;
			mcpi.dstHost = output;
			mcpi.dstPitch = ctx->stride;
			mcpi.WidthInBytes = MIN(pitch, ctx->stride);
			mcpi.Height = ctx->height;

			res = cuMemcpy2D(&mcpi);
			if (res != CUDA_SUCCESS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy Y video plane from GPU to host mem %s\n", cudaGetErrorEnum(res)));
				e = GF_IO_ERR;
			} else {

				mcpi.srcDevice = map_mem + ctx->height * pitch;
				mcpi.dstHost = output + ctx->stride * ctx->height;
				mcpi.dstPitch = ctx->stride_uv;
				mcpi.WidthInBytes = MIN(pitch, ctx->stride);
				mcpi.Height = ctx->uv_height;

				res = cuMemcpy2D(&mcpi);
				if (res != CUDA_SUCCESS) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy UV video plane from GPU to host mem %s\n", cudaGetErrorEnum(res)));
					e = GF_IO_ERR;
				}
			}
			cuvidUnmapVideoFrame(ctx->dec_inst->cu_decoder, map_mem);

			gf_filter_pck_send(dst_pck);

		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to map video frame data %s\n", cudaGetErrorEnum(res)));
			e = GF_IO_ERR;
			gf_filter_pck_discard(dst_pck);
		}
	}
	cuCtxPopCurrent(NULL);

	memset(f, 0, sizeof(NVDecFrame));
	gf_list_add(ctx->frames_res, f);

	return e;
}

void nvframe_release(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FilterFrameInterface *frame = gf_filter_pck_get_frame_interface(pck);
	NVDecFrame *f = (NVDecFrame*) frame->user_data;
	NVDecCtx *ctx = (NVDecCtx *)f->ctx;

	memset(f, 0, sizeof(NVDecFrame));
	gf_list_add(ctx->frames_res, f);
}

#ifndef GPAC_DISABLE_3D

/*Define codec matrix*/
typedef struct __matrix GF_NVCodecMatrix;

GF_Err nvframe_get_gl_texture(GF_FilterFrameInterface *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_NVCodecMatrix * texcoordmatrix)
{
	CUDA_MEMCPY2D mcpi;
	CUVIDPROCPARAMS params;
	CUresult res;
	GF_Err e = GF_OK;
	CUdeviceptr tx_data, vid_data;
	size_t tx_pitch;
	u32 vid_pitch;
	u32 pbo_id, tx_id, gl_fmt, gl_btype = GL_UNSIGNED_BYTE;
	NVDecFrame *f = (NVDecFrame *)frame->user_data;
	NVDecCtx *ctx = (NVDecCtx *)f->ctx;

	if (plane_idx>1) return GF_BAD_PARAM;

	res = cuCtxPushCurrent(cuda_ctx);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to push CUDA CTX %s\n", cudaGetErrorEnum(res) ) );
	}

	if (! *gl_tex_id && ! plane_idx && ctx->y_tx_id ) {
		cuGLUnregisterBufferObject(ctx->y_pbo_id);
		ctx->y_pbo_id = 0;
		ctx->y_tx_id = 0;
	}
	if (! *gl_tex_id && plane_idx && ctx->uv_tx_id ) {
		cuGLUnregisterBufferObject(ctx->uv_pbo_id);
		ctx->uv_pbo_id = 0;
		ctx->uv_tx_id = 0;
	}

#ifdef ENABLE_10BIT_OUTPUT
	if ((ctx->bpp_luma>8) || (ctx->bpp_chroma>8)) {
		gl_btype = GL_UNSIGNED_SHORT;
	}
#endif
	if (!plane_idx) {
		if (!ctx->y_pbo_id) {
			glGenBuffers(1, &ctx->y_pbo_id);
			glGenTextures(1, &ctx->y_tx_id);

	        glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->y_pbo_id);
			glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->stride * ctx->height, NULL, GL_STREAM_DRAW_ARB);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

			cuGLRegisterBufferObject(ctx->y_pbo_id);

			glBindTexture(GL_TEXTURE_2D, ctx->y_tx_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, ctx->width, ctx->height, 0, GL_LUMINANCE, gl_btype, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			f->y_mapped = GF_FALSE;
		}
		*gl_tex_format = GL_TEXTURE_2D;
		*gl_tex_id = tx_id = ctx->y_tx_id;
		if (f->y_mapped) {
			cuCtxPopCurrent(NULL);
			return GF_OK;
		}
		f->y_mapped = GF_TRUE;
		pbo_id = ctx->y_pbo_id;
		gl_fmt = GL_LUMINANCE;
	} else {
		if (!ctx->uv_pbo_id) {
			glGenBuffers(1, &ctx->uv_pbo_id);
			glGenTextures(1, &ctx->uv_tx_id);

	        glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->uv_pbo_id);
			glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->stride * ctx->height / 2, NULL, GL_STREAM_DRAW_ARB);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

			cuGLRegisterBufferObject(ctx->uv_pbo_id);

		    glBindTexture(GL_TEXTURE_2D, ctx->uv_tx_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, ctx->width/2, ctx->height/2, 0, GL_LUMINANCE_ALPHA, gl_btype, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);
			f->uv_mapped = GF_FALSE;
		}
		*gl_tex_format = GL_TEXTURE_2D;
		*gl_tex_id = tx_id = ctx->uv_tx_id;
		if (f->uv_mapped) {
			cuCtxPopCurrent(NULL);
			return GF_OK;
		}
		f->uv_mapped = GF_TRUE;
		pbo_id = ctx->uv_pbo_id;
		gl_fmt = GL_LUMINANCE_ALPHA;
	}

	cuGLMapBufferObject(&tx_data, &tx_pitch, pbo_id);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to map GL texture data %s\n", cudaGetErrorEnum(res) ) );
		return GF_IO_ERR;
	}
	assert(tx_pitch != 0);

	memset(&params, 0, sizeof(params));
	params.progressive_frame = f->frame_info.progressive_frame;
	//params.second_field = 0;
	params.top_field_first = f->frame_info.top_field_first;
	res = cuvidMapVideoFrame(ctx->dec_inst->cu_decoder, f->frame_info.picture_index, &vid_data, &vid_pitch, &params);

	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to map decoded picture data %s\n", cudaGetErrorEnum(res) ) );
		return GF_IO_ERR;
	}
	assert(vid_pitch != 0);

	memset(&mcpi, 0, sizeof(CUDA_MEMCPY2D));
	mcpi.srcMemoryType = CU_MEMORYTYPE_DEVICE;
	if (plane_idx) {
		mcpi.srcDevice = vid_data + ctx->height * vid_pitch;
		tx_pitch *= 2; //2 bytes per pixel
	} else {
		mcpi.srcDevice = vid_data;
	}
	mcpi.srcPitch = vid_pitch;

	mcpi.dstMemoryType = CU_MEMORYTYPE_DEVICE;
	mcpi.dstDevice = tx_data;
	mcpi.dstPitch = tx_pitch / ctx->height;

	mcpi.WidthInBytes = MIN(mcpi.dstPitch, vid_pitch);
	mcpi.Height = ctx->height;
	if (plane_idx) mcpi.Height /= 2;

	res = cuMemcpy2D(&mcpi);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy Y video plane from GPU to host mem %s\n", cudaGetErrorEnum(res) ) );
		e = GF_IO_ERR;
	}

	cuvidUnmapVideoFrame(ctx->dec_inst->cu_decoder, vid_data);
	cuGLUnmapBufferObject(pbo_id);


	cuCtxPopCurrent(NULL);

	/*bind PBO to texture and call glTexSubImage2D only after PBO transfer is queued, otherwise we'll have a one frame delay*/
	glBindTexture(GL_TEXTURE_2D, tx_id);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_id);

#ifdef ENABLE_10BIT_OUTPUT
	if (ctx->bpp_chroma+ctx->bpp_luma>16) {
		Float a, b;
#error "FIX NVDEC GL color mapping in 10 bit"
		a = 65535.0f / (65472.0f - 63.0f);
		b = -63.0f * a / 65535.0f;

		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		//glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
		//glPixelStorei(GL_UNPACK_LSB_FIRST, 1);
		//we use 10 bits but GL will normalise using 16 bits, so we need to multiply the nomralized result by 2^6
		//glPixelTransferf(GL_RED_BIAS, 0.00096317f);
		//glPixelTransferf(GL_RED_SCALE, 0.000015288f);

		glPixelTransferf(GL_RED_SCALE, a);
		glPixelTransferf(GL_RED_BIAS, b);

		if (plane_idx) {
			glPixelTransferf(GL_ALPHA_SCALE, a);
			glPixelTransferf(GL_ALPHA_BIAS, b);
		}
	}
#endif
	if (!plane_idx) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->width, ctx->height, gl_fmt , gl_btype, NULL);
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->width/2, ctx->height/2, gl_fmt , gl_btype, NULL);
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	return e;
}

#endif

GF_Err nvframe_get_frame(GF_FilterFrameInterface *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
	unsigned int pitch = 0;
	GF_Err e = GF_OK;
	NVDecFrame *f = (NVDecFrame *)frame->user_data;
	NVDecCtx *ctx = (NVDecCtx *)f->ctx;

	if (plane_idx>=ctx->nb_planes) return GF_BAD_PARAM;

	e = GF_OK;
	if (!f->y_mapped) {
		CUVIDPROCPARAMS params;
		CUdeviceptr map_mem = 0;
		CUresult res;

		if (ctx->out_size > ctx->single_frame_data_alloc) {
			ctx->single_frame_data_alloc = ctx->out_size;
			ctx->single_frame_data = gf_realloc(ctx->single_frame_data, ctx->out_size);
		}
		f->y_mapped = GF_TRUE;

		res = cuCtxPushCurrent(cuda_ctx);
		if (res != CUDA_SUCCESS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to push CUDA CTX %s\n", cudaGetErrorEnum(res)));
			return GF_IO_ERR;
		}

		memset(&params, 0, sizeof(params));
		params.progressive_frame = f->frame_info.progressive_frame;
		params.top_field_first = f->frame_info.top_field_first;

		res = cuvidMapVideoFrame(ctx->dec_inst->cu_decoder, f->frame_info.picture_index, &map_mem, &pitch, &params);
		if (res == CUDA_SUCCESS) {
			CUDA_MEMCPY2D mcpi;
			memset(&mcpi, 0, sizeof(CUDA_MEMCPY2D));
			mcpi.srcMemoryType = CU_MEMORYTYPE_DEVICE;
			mcpi.srcDevice = map_mem;
			mcpi.srcPitch = pitch;

			mcpi.dstMemoryType = CU_MEMORYTYPE_HOST;
			mcpi.dstHost = ctx->single_frame_data;
			mcpi.dstPitch = ctx->stride;
			mcpi.WidthInBytes = MIN(pitch, ctx->stride);
			mcpi.Height = ctx->height;

			res = cuMemcpy2D(&mcpi);
			if (res != CUDA_SUCCESS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy Y video plane from GPU to host mem %s\n", cudaGetErrorEnum(res)));
				e = GF_IO_ERR;
			}
			else {

				mcpi.srcDevice = map_mem + ctx->height * pitch;
				mcpi.dstHost = ctx->single_frame_data + ctx->stride * ctx->height;
				mcpi.dstPitch = ctx->stride_uv;
				mcpi.Height = ctx->uv_height;

				res = cuMemcpy2D(&mcpi);
				if (res != CUDA_SUCCESS) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy UV video plane from GPU to host mem %s\n", cudaGetErrorEnum(res)));
					e = GF_IO_ERR;
				}
			}
			cuvidUnmapVideoFrame(ctx->dec_inst->cu_decoder, map_mem);
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to map video frame %s\n", cudaGetErrorEnum(res)));
			e = GF_IO_ERR;
		}
		cuCtxPopCurrent(NULL);
	}
	if (e) return e;

	switch (plane_idx) {
	case 0:
		*outPlane = ctx->single_frame_data;
		*outStride = ctx->stride;
		break;
	case 1:
		*outPlane = ctx->single_frame_data + ctx->stride * ctx->height;
		*outStride = ctx->stride_uv;
		break;
	case 2:
		*outPlane = ctx->single_frame_data + ctx->stride * ctx->height + ctx->stride_uv * ctx->uv_height;
		*outStride = ctx->stride_uv;
		break;
	default:
		return GF_BAD_PARAM;
	}
	return GF_OK;
}


GF_Err nvdec_send_hw_frame(NVDecCtx *ctx)
{
	GF_FilterPacket *dst_pck;
	NVDecFrame *f;

	if (!ctx->pending_frame) return GF_BAD_PARAM;
	f = ctx->pending_frame;
	ctx->pending_frame = NULL;

	f->gframe.user_data = f;
	f->gframe.get_plane = nvframe_get_frame;
#ifndef GPAC_DISABLE_3D
	f->gframe.get_gl_texture = nvframe_get_gl_texture;
#endif

	if (ctx->frame_size_changed) {
		ctx->frame_size_changed = GF_FALSE;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->stride));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pix_fmt));
	}


	if (!gf_list_count(ctx->frames) && ctx->needs_resetup)
		f->gframe.blocking = GF_TRUE;

	dst_pck = gf_filter_pck_new_frame_interface(ctx->opid, &f->gframe, nvframe_release);

	nvdec_merge_pck_props(ctx, f, dst_pck);
	if (gf_filter_pck_get_seek_flag(dst_pck)) {
		gf_filter_pck_discard(dst_pck);
		memset(f, 0, sizeof(NVDecFrame));
		gf_list_add(ctx->frames_res, f);
	} else {
		gf_filter_pck_send(dst_pck);
	}

	return GF_OK;
}




static u32 cuvid_load_state = 0;
static u32 nb_cuvid_inst=0;
static void init_cuda_sdk()
{
	if (!cuvid_load_state) {
		CUresult res;
		int device_count;
	    res = cuInit(0, __CUDA_API_VERSION);
		nb_cuvid_inst++;
		cuvid_load_state = 1;
		if (res == CUDA_ERROR_SHARED_OBJECT_INIT_FAILED) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[NVDec] cuda lib not found on system\n") );
		} else if (res != CUDA_SUCCESS) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] failed to init cuda %s\n", cudaGetErrorEnum(res) ) );
		} else {
			res = cuDeviceGetCount(&device_count);
			if (res != CUDA_SUCCESS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to query cuda/nvidia cards %s\n", cudaGetErrorEnum(res) ) );
			} else {
				if (! device_count) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] no device found\n" ) );
				} else {
					cuvid_load_state = 2;
				}
			}
		}
	} else {
		nb_cuvid_inst++;
	}
}

static GF_Err nvdec_initialize(GF_Filter *filter)
{
	NVDecCtx *ctx = gf_filter_get_udta(filter);

	ctx->frames = gf_list_new();
	ctx->frames_res = gf_list_new();
	ctx->src_packets = gf_list_new();
	return GF_OK;
}

static void nvdec_finalize(GF_Filter *filter)
{
	NVDecCtx *ctx = gf_filter_get_udta(filter);

	nvdec_reset_pcks(ctx);
	gf_list_del(ctx->src_packets);

	if (!global_nb_loaded_nvdec && global_unactive_decoders) {
		while (gf_list_count(global_unactive_decoders)) {
			NVDecInstance *inst = gf_list_pop_back(global_unactive_decoders);
			nvdec_destroy_decoder(inst);
			if (inst->cu_parser) cuvidDestroyVideoParser(inst->cu_parser);
			gf_free(inst);
		}
		gf_list_del(global_unactive_decoders);

		gf_mx_del(global_inst_mutex);
	}

	if (ctx->dec_inst) {
		nvdec_destroy_decoder(ctx->dec_inst);
		if (ctx->dec_inst->cu_parser) cuvidDestroyVideoParser(ctx->dec_inst->cu_parser);
		gf_free(ctx->dec_inst);
	}


	assert(nb_cuvid_inst);
	nb_cuvid_inst--;
	if (!nb_cuvid_inst) {
		if (cuda_ctx) cuCtxDestroy(cuda_ctx);
		cuda_ctx = NULL;
		cuUninit();
		cuvid_load_state = 0;
	}
	while (gf_list_count(ctx->frames)) {
		NVDecFrame *f = (NVDecFrame *) gf_list_pop_back(ctx->frames);
		gf_free(f);
	}
	gf_list_del(ctx->frames);
	while (gf_list_count(ctx->frames_res)) {
		NVDecFrame *f = (NVDecFrame *) gf_list_pop_back(ctx->frames_res);
		gf_free(f);
	}
	gf_list_del(ctx->frames_res);

	if (ctx->single_frame_data) gf_free(ctx->single_frame_data);
}


static const GF_FilterCapability NVDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SNR),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SPATIAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_HIGH),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_422),
	//CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VC1),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	//we want annex B format for AVC and HEVC
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,GF_PROP_PID_TILE_BASE, GF_TRUE),

};

#define OFFS(_n)	#_n, offsetof(NVDecCtx, _n)

static const GF_FilterArgs NVDecArgs[] =
{
	{ OFFS(num_surfaces), "Number of hardware surfaces to allocate", GF_PROP_UINT, "20", NULL, GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(unload), "Unload inactive decoder.", GF_PROP_UINT, "no", "no|destroy|reuse", GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(vmode), "Video decoder mode.", GF_PROP_UINT, "cuvid", "cuvid|cuda|dxva", GF_FS_ARG_HINT_ADVANCED },
	{ OFFS(fmode), "Frame output mode:\n\tcopy: each frame is copied and dispatched\n\tsingle: frame data is only retrieved when used, single memory space for all frames (not safe if multiple consummers)\n\tgl: frame data is mapped to an OpenGL texture", GF_PROP_UINT, "gl", "copy|single|gl", 0 },

	{ 0 }
};

GF_FilterRegister NVDecRegister = {
	.name = "nvdec",
	GF_FS_SET_DESCRIPTION("NVidia decoder")
	GF_FS_SET_HELP("This filter decodes MPEG-2, MPEG-4 Part 2, AVC|H264 and HEVC streams through NVideia decoder. It allows GPU frame dispatch or direct frame copy.")
	.private_size = sizeof(NVDecCtx),
	SETCAPS(NVDecCaps),
	.flags = GF_FS_REG_CONFIGURE_MAIN_THREAD | GF_FS_REG_HIDE_WEIGHT,
	.initialize = nvdec_initialize,
	.finalize = nvdec_finalize,
	.args = NVDecArgs,
	.configure_pid = nvdec_configure_pid,
	.process = nvdec_process,
	.process_event = nvdec_process_event
};


const GF_FilterRegister *nvdec_register(GF_FilterSession *session)
{
	init_cuda_sdk();
	if (cuvid_load_state != 2) return NULL;

	return &NVDecRegister;
}

#else

const GF_FilterRegister *nvdec_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // defined(WIN32) || defined(GPAC_CONFIG_LINUX)


