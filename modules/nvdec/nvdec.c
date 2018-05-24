/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / codec pack module
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
#include <gpac/modules/codec.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#include "../../src/compositor/gl_inc.h"

#include "cuda_sdk.h"

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#  pragma comment(lib, "opengl32")
#endif


#ifdef LOAD_GL_1_5
GLDECL_STATIC(glGenBuffers);
GLDECL_STATIC(glBindBuffer);
GLDECL_STATIC(glBufferData);
#endif

typedef struct _nv_dec_inst NVDecInstance;

typedef struct _nv_dec_ctx
{
	GF_ESD *esd;
	Bool use_gl_texture;
	u32 width, height, bpp_luma, bpp_chroma;
	cudaVideoCodec codec_type;
	cudaVideoChromaFormat chroma_fmt;
	
	u32 pixel_ar, out_size, stride, pix_fmt;
	u32 reload_decoder_state;
	Bool skip_next_frame;
	CUresult decode_error, dec_create_error;
	Bool frame_size_changed;
	u32 num_surfaces;
	Bool needs_resetup;
	u32 unload_inactive_mode;
	unsigned long prefer_dec_mode;

	NVDecInstance *dec_inst;

	GF_List *frames;
	GF_List *frames_res;

	struct __nv_frame *pending_frame;

	GLint y_tx_id, uv_tx_id;
	GLint y_pbo_id, uv_pbo_id;
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
	GF_MediaDecoderFrame gframe;
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
		ctx->out_size = ctx->stride * ctx->height * 3 / 2;
		break;
	case cudaVideoChromaFormat_422:
		ctx->pix_fmt = use_10bits  ? GF_PIXEL_YUV422_10 : GF_PIXEL_YUV422;
		ctx->out_size = ctx->stride * ctx->height * 2;
		break;
	case cudaVideoChromaFormat_444:
		ctx->pix_fmt = use_10bits  ? GF_PIXEL_YUV444_10 : GF_PIXEL_YUV444;
		ctx->out_size = ctx->stride * ctx->height * 3;
		break;
	default:
		ctx->pix_fmt = 0;
		ctx->out_size = 0;
	}
}

static int CUDAAPI HandleVideoSequence(void *pUserData, CUVIDEOFORMAT *pFormat)
{
	Bool use_10bits=GF_FALSE;
	Bool skip_output_resize=GF_FALSE;
	NVDecInstance *inst= (NVDecInstance *)pUserData;
	NVDecCtx *ctx = inst->ctx;

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] Decoder instance %d Video sequence change detected - new setup %u x %u, %u bpp\n", inst->id, pFormat->coded_width, pFormat->coded_height, pFormat->bit_depth_luma_minus8 + 8) );
	//no change in config
	if ((ctx->width == pFormat->coded_width) 
		&& (ctx->height == pFormat->coded_height)
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

	ctx->width = pFormat->coded_width;
	ctx->height = pFormat->coded_height;
	ctx->bpp_luma = 8 + pFormat->bit_depth_luma_minus8;
	ctx->bpp_chroma = 8 + pFormat->bit_depth_chroma_minus8;
	ctx->codec_type = pFormat->codec;
	ctx->chroma_fmt = pFormat->chroma_format;

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

static GF_Err NVDec_ConfigureStream(GF_BaseDecoder *ifcg)
{
	CUresult res;
    CUVIDPARSERPARAMS oVideoParserParameters;
	NVDecCtx *ctx = (NVDecCtx *)ifcg->privateStack;

	//create a video parser and a video decoder
    memset(&oVideoParserParameters, 0, sizeof(CUVIDPARSERPARAMS));
	ctx->needs_resetup = GF_FALSE;

	switch (ctx->esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_MPEG1: 
		ctx->codec_type = cudaVideoCodec_MPEG1;
		break;
	case GPAC_OTI_VIDEO_MPEG2_SIMPLE: 
	case GPAC_OTI_VIDEO_MPEG2_MAIN: 
	case GPAC_OTI_VIDEO_MPEG2_SNR: 
	case GPAC_OTI_VIDEO_MPEG2_SPATIAL: 
	case GPAC_OTI_VIDEO_MPEG2_HIGH: 
	case GPAC_OTI_VIDEO_MPEG2_422: 
		ctx->codec_type = cudaVideoCodec_MPEG2;
		break;
	case GPAC_OTI_VIDEO_MPEG4_PART2: 
		ctx->codec_type = cudaVideoCodec_MPEG4;
		break;
	case GPAC_OTI_VIDEO_AVC: 
		ctx->codec_type = cudaVideoCodec_H264;
		break;
	case GPAC_OTI_VIDEO_HEVC: 
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

	return GF_OK;
}

static GF_Err NVDec_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	const char *opt;
	CUresult res;
	NVDecCtx *ctx = (NVDecCtx *)ifcg->privateStack;
	
	if (ctx->esd) return GF_NOT_SUPPORTED;
	ctx->esd = esd;

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
			res = cuGLCtxCreate(&cuda_ctx, CU_CTX_BLOCKING_SYNC, cuda_dev);

#ifdef LOAD_GL_1_5
			GET_GLFUN(glGenBuffers);
			GET_GLFUN(glBindBuffer);
			GET_GLFUN(glBufferData);
#endif
			cuda_ctx_gl = GF_TRUE;

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

	opt = gf_modules_get_option((GF_BaseInterface *)ifcg, "NVDec", "NumSurfaces");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)ifcg, "NVDec", "NumSurfaces", "20");
		ctx->num_surfaces = 20;
	} else {
		ctx->num_surfaces = atoi(opt);
		if (!ctx->num_surfaces) ctx->num_surfaces = 20;
	}

	opt = gf_modules_get_option((GF_BaseInterface *)ifcg, "NVDec", "UnloadInactive");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)ifcg, "NVDec", "UnloadInactive", "no");
	} else if (!strcmp(opt, "destroy")) {
		ctx->unload_inactive_mode = 1;
	} else if (!strcmp(opt, "reuse")) {
		ctx->unload_inactive_mode = 2;
	}

	if (ctx->unload_inactive_mode == 2) {
		global_nb_loaded_nvdec++;
		if (!global_inst_mutex ) global_inst_mutex  = gf_mx_new("NVDecGlobal");
		gf_mx_p(global_inst_mutex);
		if (!global_unactive_decoders) global_unactive_decoders = gf_list_new();
		gf_mx_v(global_inst_mutex);
	}

    ctx->prefer_dec_mode = cudaVideoCreate_PreferCUVID;
	opt = gf_modules_get_option((GF_BaseInterface *)ifcg, "NVDec", "PreferMode");
	if (opt && !stricmp(opt, "dxva")) {
	    ctx->prefer_dec_mode = cudaVideoCreate_PreferDXVA;
	} else if (opt && !stricmp(opt, "cuda")) {
	    ctx->prefer_dec_mode = cudaVideoCreate_PreferCUDA;
	} else if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)ifcg, "NVDec", "PreferMode", "cuvid");
	}


	ctx->needs_resetup = GF_TRUE;

	return GF_OK;
}

static GF_Err NVDec_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	NVDecCtx *ctx = (NVDecCtx *)ifcg->privateStack;
	ctx->esd = NULL;
	ctx->dec_create_error = 0;

	if (ctx->unload_inactive_mode == 2) {
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
	return GF_OK;
}

static Bool NVDec_check_cuda_gl(NVDecCtx *ctx)
{
    int major, minor;
    char deviceName[256];
	CUresult res;

	if (cuda_ctx) return cuda_ctx_gl;

	res = cuDeviceGet(&cuda_dev, 0);
	if (res != CUDA_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to init cuda device %s\n", cudaGetErrorEnum(res) ) );
		return GF_IO_ERR;
	}

	cuDeviceComputeCapability(&major, &minor, cuda_dev);
	cuDeviceGetName(deviceName, 256, cuda_dev);

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] GPU Device %s (idx 0) has SM %d.%d compute capability\n", deviceName, major, minor));

	res = cuGLCtxCreate(&cuda_ctx, CU_CTX_BLOCKING_SYNC, cuda_dev);

	if (res == CUDA_SUCCESS) {
#ifdef LOAD_GL_1_5
		GET_GLFUN(glGenBuffers);
		GET_GLFUN(glBindBuffer);
		GET_GLFUN(glBufferData);
#endif
		cuda_ctx_gl = GF_TRUE;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec] Couldn't initialize cuda OpenGL context %s\n\tcheck you started the player without compositor thread (-no-cthread option)\n\tDisabling OpenGL support\n", cudaGetErrorEnum(res) ) );
	}
	return cuda_ctx_gl;
}

static GF_Err NVDec_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	NVDecCtx *ctx = (NVDecCtx *)ifcg->privateStack;
	const char *opt;
	
	switch (capability->CapCode) {
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->stride;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = 30.0;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ctx->pixel_ar;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = ctx->pix_fmt;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		//since we do the temporal de-interleaving ask for more CUs to avoid displaying refs before reordered frames
		capability->cap.valueInt = ctx->use_gl_texture ? 6 : 4;
		break;
	/*by default we use 4 bytes padding (otherwise it happens that XviD crashes on some videos...)*/
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 0;
		break;
	/*reorder is up to us*/
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_WANTS_THREAD:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_FRAME_OUTPUT:
		opt = gf_modules_get_option((GF_BaseInterface *)ifcg, "NVDec", "DisableGL");
		capability->cap.valueInt = (!opt || strcmp(opt, "yes")) ? 1 : 0;
		if (capability->cap.valueInt && !NVDec_check_cuda_gl(ctx)) {
			capability->cap.valueInt = 0;
		}
		break;
	case GF_CODEC_FORCE_ANNEXB:
		capability->cap.valueInt = 1;
		break;
	/*not known at our level...*/
	case GF_CODEC_CU_DURATION:
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}

static GF_Err NVDec_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	NVDecCtx *ctx = (NVDecCtx *)ifcg->privateStack;
	
	switch (capability.CapCode) {
	case GF_CODEC_FRAME_OUTPUT:
		if (capability.cap.valueInt == 2) {
			ctx->use_gl_texture = GF_TRUE;
		} else if (capability.cap.valueInt == 0) {
			ctx->use_gl_texture = GF_FALSE;
		} else {
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;

	case GF_CODEC_ABORT:
		while (gf_list_count(ctx->frames)) {
			NVDecFrame *f = (NVDecFrame *) gf_list_pop_back(ctx->frames);
			memset(f, 0, sizeof(NVDecFrame));
			gf_list_add(ctx->frames_res, f);
		}
		if (ctx->unload_inactive_mode == 2) {
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
		} else if (ctx->unload_inactive_mode == 1) {
			if (ctx->dec_inst) {
				nvdec_destroy_decoder(ctx->dec_inst);
			}
			ctx->needs_resetup = GF_TRUE;
			ctx->dec_create_error = CUDA_SUCCESS;
		}
		return GF_OK;
	}

	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}


static GF_Err NVDec_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
	NVDecFrame *f;
    CUdeviceptr map_mem = 0;
	CUVIDPROCPARAMS params;
	unsigned int pitch = 0;
	GF_Err e;
    CUVIDSOURCEDATAPACKET cu_pkt;
	CUresult res;
	NVDecCtx *ctx = (NVDecCtx *)ifcg->privateStack;

	if (ctx->needs_resetup) {
		NVDec_ConfigureStream((GF_BaseDecoder*)ifcg);
	}

	memset(&cu_pkt, 0, sizeof(CUVIDSOURCEDATAPACKET));
	cu_pkt.flags = CUVID_PKT_TIMESTAMP;
	if (!inBuffer) {
		cu_pkt.flags |= CUVID_PKT_ENDOFSTREAM;
		ctx->skip_next_frame = GF_FALSE;
	}

	if (ctx->dec_create_error) 
		return GF_IO_ERR;
	
	cu_pkt.payload_size = inBufferLength;
	cu_pkt.payload = inBuffer;
	cu_pkt.timestamp = *CTS;
	if (mmlevel == GF_CODEC_LEVEL_SEEK) 
		cu_pkt.timestamp |= 0x1FFFFFFFFUL;

	assert(!ctx->dec_inst->th_id || (ctx->dec_inst->th_id == gf_th_id() ) );

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
	
	*outBufferLength = 0;
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
		*outBufferLength = ctx->out_size;
		ctx->frame_size_changed = GF_TRUE;
		return GF_BUFFER_TOO_SMALL;
	}


	f = gf_list_pop_front(ctx->frames);
	if (!f) {
		cuCtxPopCurrent(NULL);
		return GF_OK;
	}
	if (ctx->use_gl_texture) {
		*outBufferLength = ctx->out_size;
		*CTS = (u32) f->frame_info.timestamp;
		assert(!ctx->pending_frame);
		ctx->pending_frame = f;
		return GF_OK;
	}

	assert(ctx->out_size);
	memset(&params, 0, sizeof(params));
	params.progressive_frame = f->frame_info.progressive_frame;
	//params.second_field = 0;
	params.top_field_first = f->frame_info.top_field_first;

	e = GF_OK;
	res = cuvidMapVideoFrame(ctx->dec_inst->cu_decoder, f->frame_info.picture_index, &map_mem , &pitch, &params);
	if (res == CUDA_SUCCESS) {
        CUDA_MEMCPY2D mcpi;
		memset(&mcpi, 0, sizeof(CUDA_MEMCPY2D));
		mcpi.srcMemoryType = CU_MEMORYTYPE_DEVICE;
		mcpi.srcDevice = map_mem;
		mcpi.srcPitch = pitch;

		mcpi.dstMemoryType = CU_MEMORYTYPE_HOST;
		mcpi.dstHost = outBuffer;
		mcpi.dstPitch = ctx->stride;
		mcpi.WidthInBytes = MIN(pitch, ctx->stride);
		mcpi.Height = ctx->height;

		res = cuMemcpy2D(&mcpi);
		if (res != CUDA_SUCCESS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy Y video plane from GPU to host mem %s\n", cudaGetErrorEnum(res) ) );
			e = GF_IO_ERR;
		} else {

			mcpi.srcDevice = map_mem + ctx->height * pitch;
			mcpi.dstHost = outBuffer + ctx->stride * ctx->height;
			mcpi.Height = ctx->height/2;

			res = cuMemcpy2D(&mcpi);
			if (res != CUDA_SUCCESS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to copy UV video plane from GPU to host mem %s\n", cudaGetErrorEnum(res) ) );
				e = GF_IO_ERR;
			} else {
				*outBufferLength = ctx->out_size;
				*CTS = (u32) f->frame_info.timestamp;
			}
		}
		cuvidUnmapVideoFrame(ctx->dec_inst->cu_decoder, map_mem);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to map video frame data %s\n", cudaGetErrorEnum(res) ) );
		e = GF_IO_ERR;
	}
	cuCtxPopCurrent(NULL);

	memset(f, 0, sizeof(NVDecFrame));
	gf_list_add(ctx->frames_res, f);

	return e;
}

void NVDecFrame_Release(GF_MediaDecoderFrame *frame)
{
	NVDecFrame *f = (NVDecFrame *)frame->user_data;
	NVDecCtx *ctx = (NVDecCtx *)f->ctx;
	memset(f, 0, sizeof(NVDecFrame));
	gf_list_add(ctx->frames_res, f);
}

GF_Err NVDecFrame_GetGLTexture(GF_MediaDecoderFrame *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_CodecMatrix * texcoordmatrix)
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

GF_Err NVDec_GetOutputFrame(struct _mediadecoder *dec, u16 ES_ID, GF_MediaDecoderFrame **frame, Bool *needs_resize)
{
	NVDecFrame *f;
	NVDecCtx *ctx = (NVDecCtx *)dec->privateStack;
	
	*needs_resize = GF_FALSE;
	
	if (!ctx->pending_frame) return GF_BAD_PARAM;
	f = ctx->pending_frame;
	ctx->pending_frame = NULL;

	f->gframe.user_data = f;
	f->gframe.Release = NVDecFrame_Release;
	f->gframe.GetPlane = NULL;
	f->gframe.GetGLTexture = NVDecFrame_GetGLTexture;

	*frame = &f->gframe;
	if (ctx->frame_size_changed) {
		ctx->frame_size_changed = GF_FALSE;
		*needs_resize = GF_TRUE;
	}
	return GF_OK;
}


static u32 NVDec_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	const char *opt;
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	opt = gf_modules_get_option((GF_BaseInterface *)dec, "NVDec", "Enabled");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)dec, "NVDec", "Enabled", "no");
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec] nvdec disabled by default, enable it in config file NVDec:Enabled=yes\n") );
		return GF_CODEC_NOT_SUPPORTED;
	} else if (!strcmp(opt, "no")) {
		return GF_CODEC_NOT_SUPPORTED;
	}

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_HEVC:
		return GF_CODEC_SUPPORTED * 2;
	case GPAC_OTI_VIDEO_AVC:
		return GF_CODEC_SUPPORTED * 2;

	case GPAC_OTI_VIDEO_MPEG4_PART2:
		return GF_CODEC_SUPPORTED * 2;

	case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
	case GPAC_OTI_VIDEO_MPEG2_MAIN:
	case GPAC_OTI_VIDEO_MPEG2_SNR:
	case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
	case GPAC_OTI_VIDEO_MPEG2_HIGH:
	case GPAC_OTI_VIDEO_MPEG2_422:
		return GF_CODEC_SUPPORTED * 2;
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static const char *NVDec_GetCodecName(GF_BaseDecoder *dec)
{
	NVDecCtx *ctx = (NVDecCtx *)dec->privateStack;
	switch (ctx->codec_type) {
	case cudaVideoCodec_MPEG1:
		return ctx->use_gl_texture ? "NVidia HWGL MPEG-1" :  "NVidia HW MPEG-1";
	case cudaVideoCodec_MPEG2:
		return ctx->use_gl_texture ? "NVidia HWGLMPEG-2" : "NVidia HW MPEG-2";
	case cudaVideoCodec_MPEG4:
		return ctx->use_gl_texture ? "NVidia HWGL MPEG-4 part2" : "NVidia HW MPEG-4 part2";
	case cudaVideoCodec_H264:
		return ctx->use_gl_texture ? "NVidia HWGL AVC|H264" : "NVidia HW AVC|H264";
	case cudaVideoCodec_HEVC:
		return ctx->use_gl_texture ? "NVidia HWGL HEVC" : "NVidia HW HEVC";
	case cudaVideoCodec_VC1:
		return ctx->use_gl_texture ? "NVidia HWGL VC1" : "NVidia HW VC1";
	default:
		break;
	}
	return ctx->use_gl_texture ? "NVidia HWGL unknown decoder" : "NVidia HW unknown decoder";
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
		if (res != CUDA_SUCCESS) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[NVDec] failed to init cuda %s\n", cudaGetErrorEnum(res) ) );
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

GF_BaseDecoder *NewNVDec()
{
	GF_MediaDecoder *ifcd;
	NVDecCtx *dec;

	init_cuda_sdk();

	if (cuvid_load_state != 2) return NULL;

	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	if (!ifcd) return NULL;
	GF_SAFEALLOC(dec, NVDecCtx);
	if (!dec) {
		gf_free(ifcd);
		return NULL;
	}
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "NVidia HW Decoder", "gpac distribution")

	dec->frames = gf_list_new();
	dec->frames_res = gf_list_new();
	ifcd->privateStack = dec;

	/*setup our own interface*/
	ifcd->AttachStream = NVDec_AttachStream;
	ifcd->DetachStream = NVDec_DetachStream;
	ifcd->GetCapabilities = NVDec_GetCapabilities;
	ifcd->SetCapabilities = NVDec_SetCapabilities;
	ifcd->GetName = NVDec_GetCodecName;
	ifcd->CanHandleStream = NVDec_CanHandleStream;

	ifcd->ProcessData = NVDec_ProcessData;
	ifcd->GetOutputFrame = NVDec_GetOutputFrame;

	return (GF_BaseDecoder *) ifcd;
}

void DeleteNVDec(GF_BaseDecoder *ifcg)
{
	NVDecCtx *ctx= (NVDecCtx *)ifcg->privateStack;

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
	gf_free(ctx);
	gf_free(ifcg);

}


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_MEDIA_DECODER_INTERFACE,
		0
	};

	init_cuda_sdk();

	if (cuvid_load_state != 2) return NULL;

	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewNVDec();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteNVDec((GF_BaseDecoder*)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( nvdec )
