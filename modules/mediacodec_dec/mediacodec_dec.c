#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaFormat.h"
#include "mediacodec_dec.h"

typedef struct 
{
	AMediaCodec *codec;
	AMediaFormat *format;
	ANativeWindow * window;
	char * frame;
	Bool use_gl_textures;
	u32 dequeue_timeout;
	u32 width, height, stride, out_size;
	u32 pixel_ar, pix_fmt;
	u32 crop_left, crop_right, crop_top, crop_bottom;
	int crop_unitX, crop_unitY;
	const char *mime;
	u8 chroma_format, luma_bit_depth, chroma_bit_depth;
	GF_ESD *esd;
	Float frame_rate;
	Bool surface_rendering;
	Bool frame_size_changed;
	Bool inputEOS, outputEOS;
	Bool raw_frame_dispatch;
	Bool can_init_decoder;
	MC_SurfaceTexture surfaceTex;
	//NAL-based specific
	GF_List *SPSs, *PPSs, *VPSs;
	s32 active_sps, active_pps, active_vps;
	AVCState avc;
	HEVCState hevc;
	u32 decoded_frames_pending;
	Bool reconfig_needed;
	Bool before_exit_registered;
	u32 nalu_size_length;
	//hevc
	char *vps;
	u32 luma_bpp, chroma_bpp, dec_frames;
	u8 chroma_format_idc;
	u16 ES_ID;
	AMediaCodecBufferInfo info;
	ssize_t outIndex;
	GLuint tex_id;
} MCDec;

typedef struct {
	char * frame;
	MCDec * ctx;
	ssize_t outIndex;
	Bool flushed;
} MC_Frame;

enum {
	PPS = 0,
	SPS,
	VPS,
};
u8 sdkInt() 
{
    char sdk_str[3] = "0";
    __system_property_get("ro.build.version.sdk", sdk_str, "0");
    return atoi(sdk_str);
}


//prepend the inBuffer with the start code i.e. 0x00 0x00 0x00 0x01
void prependStartCode(char *inBuffer, char *outBuffer, u32 *size) 
{
    *size += 4;
    u32 i;

    for(i = 0; i < *size; i++) {
        if(i < 3)
            outBuffer[i] = 0x00;   

        else if (i == 3) 
            outBuffer[i] = 0x01;

        else  outBuffer[i] = inBuffer[i-4];
    }
}


void initMediaFormat(MCDec *ctx, AMediaFormat *format) 
{
    AMediaFormat_setString(ctx->format, AMEDIAFORMAT_KEY_MIME, ctx->mime);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_WIDTH, ctx->width);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_HEIGHT, ctx->height);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_STRIDE, ctx->stride);  
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, ctx->width * ctx->height);
}


GF_Err MCDec_InitAvcDecoder(MCDec *ctx) 
{
	s32 idx;
	char *dsi_data = NULL;
	u32 dsi_data_size = 0;
	 
		if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs)) {
			u32 i;
			GF_AVCConfig *cfg;
			GF_AVCConfigSlot *sps = NULL;
			GF_AVCConfigSlot *pps = NULL;

			for (i=0; i<gf_list_count(ctx->SPSs); i++) {
				sps = gf_list_get(ctx->SPSs, i);
				if (ctx->active_sps<0) ctx->active_sps = sps->id;

				if (sps->id==ctx->active_sps) break;
				sps = NULL;
			}
			if (!sps) return GF_NON_COMPLIANT_BITSTREAM;
			for (i=0; i<gf_list_count(ctx->PPSs); i++) {
				pps = gf_list_get(ctx->PPSs, i);
				if (ctx->active_pps<0) ctx->active_pps = pps->id;

				if (pps->id==ctx->active_pps) break;
				pps = NULL;
			}
			if (!pps) return GF_NON_COMPLIANT_BITSTREAM;
			ctx->reconfig_needed = GF_FALSE;
			
		idx = ctx->active_sps;
        ctx->width = ctx->avc.sps[idx].width;
        ctx->height = ctx->avc.sps[idx].height;
		ctx->crop_left = ctx->avc.sps[idx].crop.left;
		ctx->crop_right = ctx->avc.sps[idx].crop.right;
		ctx->crop_top = ctx->avc.sps[idx].crop.top;
		ctx->crop_bottom = ctx->avc.sps[idx].crop.bottom;
		ctx->crop_left = ctx->avc.sps[idx].crop.left;
		ctx->width +=  ctx->crop_left  + ctx->crop_right;
		ctx->height += ctx->crop_top + ctx->crop_bottom;
		ctx->out_size = ctx->height * ctx->width * 3/2 ;

        if (ctx->avc.sps[idx].vui.par_num && ctx->avc.sps[idx].vui.par_den) {
            ctx->pixel_ar = ctx->avc.sps[idx].vui.par_num;
            ctx->pixel_ar <<= 16;
            ctx->pixel_ar |= ctx->avc.sps[idx].vui.par_den;
        }
        ctx->chroma_format = ctx->avc.sps[idx].chroma_format;
        ctx->luma_bit_depth = 8 + ctx->avc.sps[idx].luma_bit_depth_m8;
        ctx->chroma_bit_depth = 8 + ctx->avc.sps[idx].chroma_bit_depth_m8;
       switch (ctx->chroma_format) {
        case 2:
            if (ctx->luma_bit_depth>8) {
                ctx->pix_fmt = GF_PIXEL_YUV422_10;
            } else {
                ctx->pix_fmt = GF_PIXEL_YUV422;
            }
            break;
        case 3:
            if (ctx->luma_bit_depth>8) {
                ctx->pix_fmt = GF_PIXEL_YUV444_10;
            } else {
                ctx->pix_fmt = GF_PIXEL_YUV444;
            }
            break;
        default:
            if (ctx->luma_bit_depth>8) {
                ctx->pix_fmt = GF_PIXEL_YV12_10;
            }
            break;
        }
	} 
	ctx->frame_size_changed = GF_TRUE;
    return GF_OK;
}


GF_Err MCDec_InitMpeg4Decoder(MCDec *ctx) 
{
    char *dsi_data = NULL;
    u32 dsi_data_size = 0;

    if (!ctx->esd->decoderConfig->decoderSpecificInfo) {
        ctx->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
    }
    
    if (ctx->esd->decoderConfig->decoderSpecificInfo->data) {
        GF_M4VDecSpecInfo vcfg;
        GF_BitStream *bs;
        
        gf_m4v_get_config(ctx->esd->decoderConfig->decoderSpecificInfo->data, ctx->esd->decoderConfig->decoderSpecificInfo->dataLength, &vcfg);
        ctx->width = vcfg.width;
        ctx->height = vcfg.height;

        if (ctx->esd->slConfig) {
            ctx->esd->slConfig->predefined  = 2;
        }

        bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
        gf_bs_write_u32(bs, 0);
        gf_odf_desc_write_bs((GF_Descriptor *) ctx->esd, bs);
        gf_bs_get_content(bs, &dsi_data, &dsi_data_size);
        gf_bs_del(bs);

        ctx->mime = "video/mp4v-es";

        char *esds = (char *)malloc(dsi_data_size);
        memcpy(esds, dsi_data, dsi_data_size);

        esds[0] = 0x00;
        esds[1] = 0x00;
        esds[2] = 0x00;
        esds[3] = 0x01;
       
        AMediaFormat_setBuffer(ctx->format, "csd-0", esds, dsi_data_size);

        gf_free(dsi_data);
        return GF_OK;
    } 
    return GF_NOT_SUPPORTED;
}


GF_Err MCDec_InitHevcDecoder(MCDec *ctx) 
{
	s32 idx;
	char *dsi_data = NULL;
	u32 dsi_data_size = 0;
	
	if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) && gf_list_count(ctx->VPSs)) {
		u32 i;
		GF_HEVCConfig *cfg ;
		GF_AVCConfigSlot *sps = NULL;
		GF_AVCConfigSlot *pps = NULL;
		GF_AVCConfigSlot *vps = NULL;
		
		for (i=0; i<gf_list_count(ctx->SPSs); i++) {
			sps = gf_list_get(ctx->SPSs, i);
			if (ctx->active_sps<0) ctx->active_sps = sps->id;

			if (sps->id==ctx->active_sps) break;
			sps = NULL;
		}
		if (!sps) return GF_NON_COMPLIANT_BITSTREAM;
		for (i=0; i<gf_list_count(ctx->PPSs); i++) {
			pps = gf_list_get(ctx->PPSs, i);
			if (ctx->active_pps<0) ctx->active_pps = pps->id;

			if (pps->id==ctx->active_pps) break;
			pps = NULL;
		}
		for (i=0; i<gf_list_count(ctx->VPSs); i++) {
			vps = gf_list_get(ctx->VPSs, i);
			if (ctx->active_vps<0) ctx->active_vps = vps->id;

			if (vps->id==ctx->active_vps) break;
			vps = NULL;
		}
		if (!vps) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->reconfig_needed = GF_FALSE;
		idx = ctx->active_sps;
		ctx->width = ctx->hevc.sps[idx].width;
		ctx->height = ctx->hevc.sps[idx].height;
		ctx->luma_bpp = ctx->hevc.sps[idx].bit_depth_luma;
		ctx->chroma_bpp = ctx->hevc.sps[idx].bit_depth_chroma;
		ctx->chroma_format_idc  = ctx->hevc.sps[idx].chroma_format_idc;

		ctx->stride = ((ctx->luma_bpp==8) && (ctx->chroma_bpp==8)) ? ctx->width : ctx->width * 2;
		if ( ctx->chroma_format_idc  == 1) { // 4:2:0
			ctx->out_size = ctx->stride * ctx->height * 3 / 2;
		}
		else if ( ctx->chroma_format_idc  == 2) { // 4:2:2
			ctx->out_size = ctx->stride * ctx->height * 2 ;
		}
		else if ( ctx->chroma_format_idc  == 3) { // 4:4:4
			ctx->out_size = ctx->stride * ctx->height * 3;
		}
		else {
			return GF_NOT_SUPPORTED;
		}
    }
	ctx->frame_size_changed = GF_TRUE;
    return GF_OK;
}


static GF_Err MCDec_InitDecoder(MCDec *ctx) {

    GF_Err err;
    ctx->format = AMediaFormat_new();

    if(!ctx->format) {
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaFormat_new failed"));
        return GF_CODEC_NOT_FOUND;
    }

    ctx->pix_fmt = GF_PIXEL_NV12;

    switch (ctx->esd->decoderConfig->objectTypeIndication) {

        case GPAC_OTI_VIDEO_AVC :
            err = MCDec_InitAvcDecoder(ctx);
            break;

        case GPAC_OTI_VIDEO_MPEG4_PART2 :
            err = MCDec_InitMpeg4Decoder(ctx);            
            break;
            
        case GPAC_OTI_VIDEO_HEVC:
            err = MCDec_InitHevcDecoder(ctx);
            break;
     
        default:
            return GF_NOT_SUPPORTED;
    }

    if(err != GF_OK){
        return err;
    }

    ctx->frame_rate = 50.0f;
    ctx->dequeue_timeout = 5000;
    ctx->stride = ctx->width;
    initMediaFormat(ctx, ctx->format);

    ctx->codec = AMediaCodec_createDecoderByType(ctx->mime);

    if(!ctx->codec) {
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_createDecoderByType failed"));
        return GF_CODEC_NOT_FOUND;
    }
    
    if(!ctx->window){
		if(MCDec_CreateSurface(ctx->tex_id, &ctx->window, &ctx->surface_rendering, &ctx->surfaceTex) != GF_OK)
			return GF_BAD_PARAM;
	}
	
	if( AMediaCodec_configure(
        ctx->codec, // codec   
        ctx->format, // format
        (ctx->surface_rendering) ? ctx->window : NULL,  //surface 
        NULL, // crypto 
        0 // flags 
        )     != AMEDIA_OK) 
    {
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_configure failed"));
        return GF_BAD_PARAM;
    }

    if( AMediaCodec_start(ctx->codec) != AMEDIA_OK){
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_start failed"));
        return GF_BAD_PARAM;
    } 
	
	ctx->inputEOS = GF_FALSE;
    ctx->outputEOS = GF_FALSE;
	 GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,("Video size: %d x %d", ctx->width, ctx->height));
    return GF_OK;
}
static void MCDec_RegisterParameterSet(MCDec *ctx, char *data, u32 size, u8 xps)
{
	Bool add = GF_TRUE;
	u32 i, count;
	s32 ps_id;
	GF_List *dest = (xps == SPS) ? ctx->SPSs : ctx->PPSs;

	if (xps == SPS) {
		ps_id = gf_media_avc_read_sps(data, size, &ctx->avc, 0, NULL);
		if (ps_id<0) return;
	}
	else {
		ps_id = gf_media_avc_read_pps(data, size, &ctx->avc);
		if (ps_id<0) return;
	}
	count = gf_list_count(dest);
	for (i = 0; i<count; i++) {
		GF_AVCConfigSlot *a_slc = gf_list_get(dest, i);
		if (a_slc->id != ps_id) continue;
		//not same size or different content but same ID, remove old xPS
		if ((a_slc->size != size) || memcmp(a_slc->data, data, size)) {
			gf_free(a_slc->data);
			gf_free(a_slc);
			gf_list_rem(dest, i);
			break;
		}
		else {
			add = GF_FALSE;
		}
		break;
	}
	if (add) {
		GF_AVCConfigSlot *slc;
		GF_SAFEALLOC(slc, GF_AVCConfigSlot);
		slc->data = gf_malloc(size);
		memcpy(slc->data, data, size);
		slc->size = size;
		slc->id = ps_id;
		gf_list_add(dest, slc);
	
		//force re-activation of sps/pps
		if (xps == SPS) ctx->active_sps = -1;
		else ctx->active_pps = -1;
	}
}

static void MCDec_RegisterHEVCParameterSet(MCDec *ctx, char *data, u32 size, u8 xps)
{
	Bool add = GF_TRUE;
	u32 i, count;
	s32 ps_id;
	GF_List *dest = NULL; 
	
	switch(xps) {
		case SPS:
			dest = ctx->SPSs;
			ps_id = gf_media_hevc_read_sps(data, size, &ctx->hevc);
			if (ps_id<0) return;
			break;
		case PPS:
			dest = ctx->PPSs;
			ps_id = gf_media_hevc_read_pps(data, size, &ctx->hevc);
			if (ps_id<0) return;
			break;
		case VPS:
			dest = ctx->VPSs;
			ps_id = gf_media_hevc_read_vps(data, size, &ctx->hevc);
			if (ps_id<0) return;
			break;
		default:
			break;
	}
	
	count = gf_list_count(dest);
	for (i = 0; i<count; i++) {
		GF_AVCConfigSlot *a_slc = gf_list_get(dest, i);
		if (a_slc->id != ps_id) continue;
		//not same size or different content but same ID, remove old xPS
		if ((a_slc->size != size) || memcmp(a_slc->data, data, size)) {
			gf_free(a_slc->data);
			gf_free(a_slc);
			gf_list_rem(dest, i);
			break;
		}
		else {
			add = GF_FALSE;
		}
		break;
	}
	if (add && dest) {
		GF_AVCConfigSlot *slc;
		GF_SAFEALLOC(slc, GF_AVCConfigSlot);
		slc->data = gf_malloc(size);
		memcpy(slc->data, data, size);
		slc->size = size;
		slc->id = ps_id;
		gf_list_add(dest, slc);
		
		//force re-activation of sps/pps/vps
		if (xps == SPS) ctx->active_sps = -1;
		else if (xps == PPS) ctx->active_pps = -1;
		else  ctx->active_vps = -1;
	}
}

static GF_Err MCDec_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    ctx->esd = esd;
    GF_Err e;
	
	glGenTextures(1, &ctx->tex_id);
	//check AVC config
    if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_AVC) {
		ctx->SPSs = gf_list_new();
		ctx->PPSs = gf_list_new();
		ctx->mime = "video/avc";
		ctx->avc.sps_active_idx = -1;
		ctx->active_sps = ctx->active_pps = -1;
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
			ctx->width=ctx->height=128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_NV12;
            return GF_OK;
        } else {
			u32 i;
			GF_AVCConfigSlot *slc;
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			for (i = 0; i<gf_list_count(cfg->sequenceParameterSets); i++) {
				slc = gf_list_get(cfg->sequenceParameterSets, i);
				slc->id = -1;
				MCDec_RegisterParameterSet(ctx, slc->data, slc->size, SPS);
			}

			for (i = 0; i<gf_list_count(cfg->pictureParameterSets); i++) {
				slc = gf_list_get(cfg->pictureParameterSets, i);
				slc->id = -1;
				MCDec_RegisterParameterSet(ctx, slc->data, slc->size, PPS);
			}

			slc = gf_list_get(ctx->SPSs, 0);
			if (slc) ctx->active_sps = slc->id;

			slc = gf_list_get(ctx->PPSs, 0);
			if (slc) ctx->active_pps = slc->id;
			
			ctx->nalu_size_length = cfg->nal_unit_size;
		
			
			if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs)) {
				e = MCDec_InitDecoder(ctx);
				if (e) return e;
			}
			else {
				e = GF_OK;
			}
			gf_odf_avc_cfg_del(cfg);
			return e;
		}
    }

    if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_MPEG4_PART2) {
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
            ctx->width=ctx->height=128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_NV12;
        } else {
            GF_M4VDecSpecInfo vcfg;            
            gf_m4v_get_config(ctx->esd->decoderConfig->decoderSpecificInfo->data, ctx->esd->decoderConfig->decoderSpecificInfo->dataLength, &vcfg);
            ctx->width = vcfg.width;
            ctx->height = vcfg.height;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_NV12;
			return MCDec_InitDecoder(ctx);
        }
    }
  
    if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_HEVC) {
        ctx->esd= esd;
		ctx->SPSs = gf_list_new();
		ctx->PPSs = gf_list_new();
		ctx->VPSs = gf_list_new();
		ctx->mime = "video/hevc";
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
            ctx->width=ctx->height=128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_NV12;
	    return GF_OK
        } else {
			GF_HEVCConfig *hvcc = NULL;
			GF_AVCConfigSlot *sl;
			HEVCState hevc;
			u32 i,j;
			memset(&hevc, 0, sizeof(HEVCState));

			hvcc = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
			if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
			ctx->nalu_size_length = hvcc->nal_unit_size;

			for (i=0; i< gf_list_count(hvcc->param_array); i++) {
				GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);
				for (j=0; j< gf_list_count(ar->nalus); j++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
					s32 idx;
					u16 hdr = sl->data[0] << 8 | sl->data[1];

					if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
						sl->id = -1;
						MCDec_RegisterHEVCParameterSet(ctx, sl->data, sl->size, SPS);
					}
					else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
						sl->id = -1;
						MCDec_RegisterHEVCParameterSet(ctx, sl->data, sl->size, VPS);
					}
					else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
						sl->id = -1;
						MCDec_RegisterHEVCParameterSet(ctx, sl->data, sl->size, PPS);
					}
				}
			}
			sl = gf_list_get(ctx->SPSs, 0);
			if (sl) ctx->active_sps = sl->id;

			sl = gf_list_get(ctx->PPSs, 0);
			if (sl) ctx->active_pps = sl->id;
			
			sl = gf_list_get(ctx->VPSs, 0);
			if (sl) ctx->active_vps = sl->id;
			
			if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) && gf_list_count(ctx->VPSs) ) {
				e = MCDec_InitDecoder(ctx);
			}
			else {
				e = GF_OK;
			}
			gf_odf_hevc_cfg_del(hvcc);
            return e;
        }
    }
	return MCDec_InitDecoder(ctx);
}

static GF_Err MCDec_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    
    if(ctx->codec && AMediaCodec_stop(ctx->codec) != AMEDIA_OK) {
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_stop failed"));
    }
    
    return GF_OK;
}

static GF_Err MCDec_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    
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
        capability->cap.valueFloat = ctx->frame_rate;
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
        capability->cap.valueInt = 4;
        break;
    /*by default we use 4 bytes padding (otherwise it happens that XviD crashes on some videos...)*/
    case GF_CODEC_PADDING_BYTES:
        capability->cap.valueInt = 0;                   
        break;
    /*reorder is up to mediacodec*/
    case GF_CODEC_REORDER:
        capability->cap.valueInt = 1;                   
        break;
    case GF_CODEC_WANTS_THREAD:
        capability->cap.valueInt = 1;
	break;
    case GF_CODEC_FRAME_OUTPUT:
        capability->cap.valueInt = 1;                   
        break;
    case GF_CODEC_FORCE_ANNEXB:
	capability->cap.valueInt = 1;
	break;
    case GF_CODEC_TRUSTED_CTS:
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

static GF_Err MCDec_ParseNALs(MCDec *ctx, char *inBuffer, u32 inBufferLength, char **out_buffer, u32 *out_size)
{
	u32 i, sc_size;
	char *ptr = inBuffer;
	u32 nal_size;
	GF_Err e = GF_OK;
	GF_BitStream *bs = NULL;

	if (out_buffer) {
		*out_buffer = NULL;
		*out_size = 0;
	}
	if (!ctx->nalu_size_length) {
		sc_size = 0;
		nal_size = gf_media_nalu_next_start_code((u8 *)inBuffer, inBufferLength, &sc_size);
		if (!sc_size) return GF_NON_COMPLIANT_BITSTREAM;
		ptr += nal_size + sc_size;
		assert(inBufferLength >= nal_size + sc_size);
		inBufferLength -= nal_size + sc_size;
	}
	
	while (inBufferLength) {
		Bool add_nal = GF_TRUE;
		u8 nal_type, nal_hdr;
		GF_BitStream *nal_bs = NULL;
		if (ctx->nalu_size_length) {
			nal_size = 0;
			for (i = 0; i<ctx->nalu_size_length; i++) {
				nal_size = (nal_size << 8) + ((u8)ptr[i]);
			}
			ptr += ctx->nalu_size_length;
		}
		else {
			nal_size = gf_media_nalu_next_start_code(ptr, inBufferLength, &sc_size);
		}
		nal_bs = gf_bs_new(ptr, nal_size, GF_BITSTREAM_READ);
		nal_hdr = gf_bs_read_u8(nal_bs);
		nal_type = nal_hdr & 0x1F;
		switch (nal_type) {
			case GF_AVC_NALU_SEQ_PARAM:
				MCDec_RegisterParameterSet(ctx, ptr, nal_size, SPS);
				add_nal = GF_FALSE;
				break;
			case GF_AVC_NALU_PIC_PARAM:
				MCDec_RegisterParameterSet(ctx, ptr, nal_size, PPS);
				add_nal = GF_FALSE;
				break;
			case GF_AVC_NALU_ACCESS_UNIT:
			case GF_AVC_NALU_END_OF_SEQ:
			case GF_AVC_NALU_END_OF_STREAM:
			case GF_AVC_NALU_FILLER_DATA:
				add_nal = GF_FALSE;
				break;
			default:
				break;
		}
		gf_media_avc_parse_nalu(nal_bs, nal_hdr, &ctx->avc);
		gf_bs_del(nal_bs);
		if ((nal_type <= GF_AVC_NALU_IDR_SLICE) && ctx->avc.s_info.sps) {
			if (ctx->avc.sps_active_idx != ctx->active_sps) {
				ctx->reconfig_needed = 1;
				ctx->active_sps = ctx->avc.sps_active_idx;
				ctx->active_pps = ctx->avc.s_info.pps->id;
				return GF_OK;
			}
		}

		//if sps and pps are ready, init decoder
		if (!ctx->codec && gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs)) {
			ctx->reconfig_needed = 1;
			return GF_OK;
		}
		if (!out_buffer) add_nal = GF_FALSE;
		else if (add_nal && !ctx->codec) add_nal = GF_FALSE;
		if (add_nal) {
			if (!bs) bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, nal_size);
			gf_bs_write_data(bs, ptr, nal_size);
		}

		ptr += nal_size;
		if (ctx->nalu_size_length) {
			if (inBufferLength < nal_size + ctx->nalu_size_length) break;
			inBufferLength -= nal_size + ctx->nalu_size_length;
		}
		else {
			
			if (!sc_size || (inBufferLength < nal_size + sc_size)) break;
			inBufferLength -= nal_size + sc_size;
			ptr += sc_size;
		}
	}
	
	if (bs) {
		gf_bs_get_content(bs, out_buffer, out_size);
		gf_bs_del(bs);
	}
	return e;
}
static GF_Err MCDec_ParseHEVCNALs(MCDec *ctx, char *inBuffer, u32 inBufferLength)
{
	u32 i, sc_size;
	char *ptr = inBuffer;
	u32 nal_size;
	GF_Err e = GF_OK;
	GF_BitStream *bs = NULL;
	
	sc_size = 0;
	nal_size = gf_media_nalu_next_start_code((u8 *)inBuffer, inBufferLength, &sc_size);
	if (!sc_size) return GF_NON_COMPLIANT_BITSTREAM;
	ptr += nal_size + sc_size;
	assert(inBufferLength >= nal_size + sc_size);
	inBufferLength -= nal_size + sc_size;
	
	while (inBufferLength) {
		Bool add_nal = GF_TRUE;
		u8 nal_type;
		u16 nal_hdr ;
		u8 type, quality_id, temporal_id;
		
		nal_size = gf_media_nalu_next_start_code(ptr, inBufferLength, &sc_size);
		gf_media_hevc_parse_nalu(ptr, nal_size, &ctx->hevc, &type, &temporal_id, &quality_id);
		switch(type) {
			case GF_HEVC_NALU_SEQ_PARAM:
				MCDec_RegisterHEVCParameterSet(ctx, ptr, nal_size, SPS);
				break;
			case GF_HEVC_NALU_PIC_PARAM:
				MCDec_RegisterHEVCParameterSet(ctx, ptr, nal_size, PPS);
				break;
			case GF_HEVC_NALU_VID_PARAM:
				MCDec_RegisterHEVCParameterSet(ctx, ptr, nal_size, VPS);
				break;
			default:
				break;
		}
		
		if ((nal_type <= GF_HEVC_NALU_SLICE_CRA) && ctx->hevc.s_info.sps) {
			if ((ctx->hevc.sps_active_idx != ctx->active_sps) || (ctx->hevc.sps[ctx->active_sps].vps_id != ctx->active_vps)) {
				ctx->reconfig_needed = 1;
				ctx->active_sps = ctx->hevc.sps_active_idx;
				ctx->active_pps = ctx->hevc.s_info.pps->id;
				ctx->active_vps = ctx->hevc.sps[ctx->active_sps].vps_id;
				return GF_OK;
			}
		}

		//if sps, pps and vps are ready, init decoder
		if (!ctx->codec && gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) && gf_list_count(ctx->VPSs) ) {
			e = MCDec_InitDecoder(ctx);
			if (e) return e;
		}
		
		ptr += nal_size;
		if (!sc_size || (inBufferLength < nal_size + sc_size)) break;
			inBufferLength -= nal_size + sc_size;
			ptr += sc_size;
	}
	
	return e;
}


static GF_Err MCDec_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
	
	switch (capability.CapCode) {
		case GF_CODEC_FRAME_OUTPUT:
			ctx->raw_frame_dispatch = capability.cap.valueInt ? GF_TRUE : GF_FALSE;
			if (ctx->raw_frame_dispatch && (capability.cap.valueInt==2))
				ctx->use_gl_textures = GF_TRUE;

			return GF_OK;
	}

	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;

}


static GF_Err MCDec_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
	ctx->nalu_size_length = 0;
	Bool mcdec_buffer_available = GF_FALSE;
	
	if(!ctx->before_exit_registered) {
		ctx->before_exit_registered = GF_TRUE;
		if (gf_register_before_exit_function(gf_th_current(), &MCDec_BeforeExit) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("Failed to register exit function for the decoder thread %p, try to continue anyway...\n", gf_th_current()));
		}
	}
	if (!ctx->reconfig_needed) {
		if(ctx->esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_AVC)
			MCDec_ParseNALs(ctx, inBuffer, inBufferLength, NULL, NULL);
		else if(ctx->esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_HEVC)
			MCDec_ParseHEVCNALs(ctx, inBuffer, inBufferLength);
	}		
	
	if (ctx->reconfig_needed) {
		if (ctx->raw_frame_dispatch && ctx->decoded_frames_pending) {
			*outBufferLength = 1;
			return GF_BUFFER_TOO_SMALL;
		}
		
		if (ctx->codec) {
			AMediaCodec_flush(ctx->codec);
			AMediaCodec_stop(ctx->codec);
			AMediaCodec_delete(ctx->codec);
			ctx->codec = NULL;
		}
		MCDec_InitDecoder(ctx);
			 if (ctx->out_size != *outBufferLength) {
				 *outBufferLength = ctx->out_size;
				 return GF_BUFFER_TOO_SMALL;
			 }
	}
	
	if(!ctx->inputEOS) {
		ssize_t inIndex = AMediaCodec_dequeueInputBuffer(ctx->codec,ctx->dequeue_timeout);
		
		if (inIndex >= 0) {

            size_t inSize;
            char *buffer = (char *)AMediaCodec_getInputBuffer(ctx->codec, inIndex, &inSize);

            if (inBufferLength > inSize)  {
                 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("The returned buffer is too small"));
                 return GF_BUFFER_TOO_SMALL;
            }
			memcpy(buffer, inBuffer, inBufferLength);
			
            if(!inBuffer || inBufferLength == 0){
                 GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,("AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM input"));
				 ctx->inputEOS = GF_TRUE;
            }
			if(AMediaCodec_queueInputBuffer(ctx->codec,
                                    inIndex,
                                    0, 
                                    inBufferLength,
                                    *CTS,
                                    inBuffer ? 0 : AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
                ) != AMEDIA_OK)
                {
                 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_queueInputBuffer failed"));
                return GF_BAD_PARAM;
            }
            mcdec_buffer_available = GF_TRUE;
        }
    }

    if(!ctx->outputEOS) {
		
       
        ctx->outIndex = AMediaCodec_dequeueOutputBuffer(ctx->codec, &ctx->info, ctx->dequeue_timeout);
        *outBufferLength=0;
        
        switch(ctx->outIndex) {
	    case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
                GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,("AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED"));
                ctx->format = AMediaCodec_getOutputFormat(ctx->codec);
                break;

            case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,("AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED"));
                break;

            case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,("AMEDIACODEC_INFO_TRY_AGAIN_LATER"));
                break;

            default:
				if (ctx->outIndex >= 0) {
					*CTS = ctx->info.presentationTimeUs;
					if(ctx->info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
						 GF_LOG(GF_LOG_INFO, GF_LOG_CODEC,("AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM output"));
						ctx->outputEOS = true;
					}
					 
					if(!ctx->surface_rendering) {
						size_t outSize;
						uint8_t * buffer = AMediaCodec_getOutputBuffer(ctx->codec, ctx->outIndex, &outSize);
						ctx->frame = buffer + ctx->info.offset;
						
						if(!ctx->frame) {
							 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_getOutputBuffer failed"));
							*outBufferLength = 0;
						}
					}
					
					if(ctx->surface_rendering || ctx->frame) {
						if(ctx->info.size < ctx->out_size)
						*outBufferLength = ctx->info.size;
						else *outBufferLength = ctx->out_size;
					}
				}
			break;
			
			}

    }
	
	return (mcdec_buffer_available) ? GF_OK : GF_CODEC_BUFFER_UNAVAILABLE;      
}


static u32 MCDec_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
    if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;

    /*media type query*/
    if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

    switch (esd->decoderConfig->objectTypeIndication) {
        case GPAC_OTI_VIDEO_AVC:
            return GF_CODEC_SUPPORTED;
        case GPAC_OTI_VIDEO_HEVC:
			if(sdkInt() >= 21) {
				return GF_CODEC_SUPPORTED;
			}
			break;
		case GPAC_OTI_VIDEO_MPEG4_PART2:
            return GF_CODEC_SUPPORTED;
    }

    return GF_CODEC_NOT_SUPPORTED;
}
void MCFrame_Release(GF_MediaDecoderFrame *frame)
{	
	MC_Frame *f = (MC_Frame *)frame->user_data;
	if(f->ctx->codec && !f->flushed)  {
		if ( AMediaCodec_releaseOutputBuffer(f->ctx->codec, f->outIndex, GF_FALSE) != AMEDIA_OK) {
			 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("NOT Release Output Buffer Index: %d", f->outIndex));
		}
	}
	f->ctx->decoded_frames_pending--;
	gf_free(f);
	gf_free(frame);
}
GF_Err MCFrame_GetPlane(GF_MediaDecoderFrame *frame, u32 plane_idx, const char **outPlane, u32 *outStride)
{
	
	GF_Err e;
	MC_Frame *f = (MC_Frame *)frame->user_data;
	int color;
	if (! outPlane || !outStride) return GF_BAD_PARAM;
	*outPlane = NULL;
	*outStride = 0;
	f->ctx->format = AMediaCodec_getOutputFormat(f->ctx->codec);
	AMediaFormat_getInt32(f->ctx->format, AMEDIAFORMAT_KEY_STRIDE, outStride);
	
	switch(plane_idx) {
		case 0 :
			*outPlane = f->frame ;
			break;
		case 1 :
			*outPlane = f->frame + *outStride * f->ctx->height ;
			*outStride/=2;
			break;
		case 2: 
			*outPlane = f->frame + 5 * *outStride * f->ctx->height/4 ;
			*outStride/=2;
			break;
		default :
			return GF_BAD_PARAM;
			
	}
	
	return GF_OK;
}

GF_Err MCFrame_GetGLTexture(GF_MediaDecoderFrame *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_CodecMatrix * texcoordmatrix)
{		
	MC_Frame *f = (MC_Frame *)frame->user_data;
   	if (!gl_tex_format || !gl_tex_id) return GF_BAD_PARAM;
	*gl_tex_format = GL_TEXTURE_EXTERNAL_OES;
	*gl_tex_id = f->ctx->tex_id;
	
	if(!f->flushed && f->ctx->codec) {
		if (AMediaCodec_releaseOutputBuffer(f->ctx->codec, f->outIndex, GF_TRUE) != AMEDIA_OK) {
			 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("NOT Release Output Buffer Index: %d to surface", f->outIndex));
			 return GF_OK;
		}
		if(MCFrame_UpdateTexImage(f->ctx->surfaceTex) != GF_OK) return GF_BAD_PARAM;
		if(MCFrame_GetTransformMatrix(texcoordmatrix, f->ctx->surfaceTex) != GF_OK) return GF_BAD_PARAM;
		
		f->flushed = GF_TRUE;
	}
	return GF_OK;
}

GF_Err MCDec_GetOutputFrame(GF_MediaDecoder *dec, u16 ES_ID, GF_MediaDecoderFrame **frame, Bool *needs_resize)
{
	GF_MediaDecoderFrame *a_frame;
	MC_Frame *mc_frame;
	ssize_t outSize;
	MCDec *ctx = (MCDec *)dec->privateStack;

	*needs_resize = GF_FALSE;
	
	if(ctx->outIndex < 0 || (!ctx->frame && !ctx->surface_rendering))  return GF_BAD_PARAM;
	GF_SAFEALLOC(a_frame, GF_MediaDecoderFrame);
	if (!a_frame) return GF_OUT_OF_MEM;
	GF_SAFEALLOC(mc_frame, MC_Frame);
	if (!mc_frame) {
		gf_free(a_frame);
		return GF_OUT_OF_MEM;
	}
	a_frame->user_data = mc_frame;
	mc_frame->ctx = ctx;
	mc_frame->frame = ctx->frame;
	mc_frame->outIndex = ctx->outIndex;
	mc_frame->flushed = GF_FALSE;
	ctx->frame = NULL;
	a_frame->Release = MCFrame_Release;
	a_frame->GetPlane = MCFrame_GetPlane;
	if (ctx->use_gl_textures)
		a_frame->GetGLTexture = MCFrame_GetGLTexture;
	
	*frame = a_frame;
	if (ctx->frame_size_changed) {
		ctx->frame_size_changed = GF_FALSE;
		*needs_resize = GF_TRUE;
	}
	ctx->decoded_frames_pending++;
	return GF_OK;
}

static const char *MCDec_GetCodecName(GF_BaseDecoder *dec)
{   
    MCDec *ctx = (MCDec *) dec->privateStack;

    if(!strcmp(ctx->mime, "video/avc"))
        return "MediaCodec hardware AVC|H264";

    else if(!strcmp(ctx->mime, "video/hevc"))
        return "MediaCodec hardware HEVC|H265";

    else if(!strcmp(ctx->mime, "video/mp4v-es"))
        return "MediaCodec hardware MPEG-4 Part2";

    else return "MediaCodec not supported";
}


GF_BaseDecoder *NewMCDec()
{
    GF_MediaDecoder *ifcd;
    MCDec *dec;

    GF_SAFEALLOC(ifcd, GF_MediaDecoder);
    if (!ifcd) return NULL;
    GF_SAFEALLOC(dec, MCDec);
    if (!dec) {
        gf_free(ifcd);
        return NULL;
    }
    GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "Android MediaCodec Decoder", "gpac distribution")

    ifcd->privateStack = dec;

    /*setup our own interface*/
    ifcd->AttachStream = MCDec_AttachStream;
    ifcd->DetachStream = MCDec_DetachStream;
    ifcd->GetCapabilities = MCDec_GetCapabilities;
    ifcd->SetCapabilities = MCDec_SetCapabilities;
    ifcd->GetName = MCDec_GetCodecName;
    ifcd->CanHandleStream = MCDec_CanHandleStream;
    ifcd->ProcessData = MCDec_ProcessData;
	ifcd->GetOutputFrame = MCDec_GetOutputFrame;
    return (GF_BaseDecoder *) ifcd;
}
static void MCDec_DelParamList(GF_List *list)
{
	while (gf_list_count(list)) {
		GF_AVCConfigSlot *slc = gf_list_get(list, 0);
		gf_free(slc->data);
		gf_free(slc);
		gf_list_rem(list, 0);
	}
	
	gf_list_del(list);
	
}

void DeleteMCDec(GF_BaseDecoder *ifcg)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;

    if(ctx->format && AMediaFormat_delete(ctx->format) != AMEDIA_OK){
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaFormat_delete failed"));
    }
   
    if(ctx->codec && AMediaCodec_delete(ctx->codec) != AMEDIA_OK) {
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC,("AMediaCodec_delete failed"));
    }
    
    if(ctx->window) {
	ANativeWindow_release(ctx->window);
	ctx->window = NULL;
    }

    if(ctx->codec)
	MCDec_DeleteSurface(ctx->surfaceTex);
	
    if(ctx->tex_id) glDeleteTextures (1, &ctx->tex_id);

    MCDec_DelParamList(ctx->SPSs);
    ctx->SPSs = NULL;
    MCDec_DelParamList(ctx->PPSs);
    ctx->PPSs = NULL;
    MCDec_DelParamList(ctx->VPSs);
    ctx->VPSs = NULL;

    gf_free(ctx);
    gf_free(ifcg);
	
}



GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
    static u32 si [] = {
#ifndef GPAC_DISABLE_AV_PARSERS
        GF_MEDIA_DECODER_INTERFACE,
#endif
        0
    };
    return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{

#ifndef GPAC_DISABLE_AV_PARSERS
    if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewMCDec();
#endif
    return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
    switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_AV_PARSERS
    case GF_MEDIA_DECODER_INTERFACE:
        DeleteMCDec((GF_BaseDecoder*)ifce);
        break;
#endif
    }
}

GPAC_MODULE_STATIC_DECLARATION( mc )
