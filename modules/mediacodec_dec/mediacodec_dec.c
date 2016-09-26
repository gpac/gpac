#include <gpac/modules/codec.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#include "../../src/compositor/gl_inc.h"

#include <jni.h>

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaFormat.h"

#include <android/log.h>

#define TAG "mediacodec_dec"

#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, TAG,  __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, TAG,  __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, TAG,  __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, TAG,  __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, TAG,  __VA_ARGS__)


typedef struct 
{
    AMediaCodec *codec;
    AMediaFormat *format;
    u32 dequeue_timeout;

    u32 width, height, stride, out_size;
    u32 pixel_ar, pix_fmt;

    const char *mime;

    u8 chroma_format, luma_bit_depth, chroma_bit_depth;
    GF_ESD *esd;

    u32 counter;
    u16 frame_rate;

    Bool inputEOS, outputEOS;

    //NAL-based specific
    char *sps, *pps;
    u32 sps_size, pps_size;
    u32 nalu_size_length;

    //hevc
    char *vps;
    u32 luma_bpp, chroma_bpp, dec_frames;
    u8 chroma_format_idc;
    u16 ES_ID;
    u32 vps_size;

} MCDec;


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
    if (ctx->sps && ctx->pps) {
        AVCState avc;
        s32 idx;
        memset(&avc, 0, sizeof(AVCState));
        avc.sps_active_idx = -1;

        idx = gf_media_avc_read_sps(ctx->sps, ctx->sps_size, &avc, 0, NULL);

        assert(ctx->sps);
        ctx->width = avc.sps[idx].width;
        ctx->height = avc.sps[idx].height;

        ctx->out_size = ctx->height * ctx->width * 3/2 ;

        if (avc.sps[idx].vui.par_num && avc.sps[idx].vui.par_den) {
            ctx->pixel_ar = avc.sps[idx].vui.par_num;
            ctx->pixel_ar <<= 16;
            ctx->pixel_ar |= avc.sps[idx].vui.par_den;
        }
        ctx->chroma_format = avc.sps[idx].chroma_format;
        ctx->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
        ctx->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;
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
    
        ctx->mime = "video/avc";

        u32 sps_size = 4 + ctx->sps_size;
        u32 pps_size = 4 + ctx->pps_size;   
        char *sps = (char *)malloc(sps_size);
        char *pps = (char *)malloc(pps_size);
        
        prependStartCode(ctx->sps, sps, &ctx->sps_size);
        prependStartCode(ctx->pps, pps, &ctx->pps_size);

        AMediaFormat_setBuffer(ctx->format, "csd-0", sps, sps_size);
        AMediaFormat_setBuffer(ctx->format, "csd-1", pps, pps_size);
    }

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
    u32 i, j;
    GF_HEVCConfig *cfg = NULL;

    ctx->ES_ID = ctx->esd->ESID;
    ctx->width = ctx->height = ctx->out_size = ctx->luma_bpp = ctx->chroma_bpp = ctx->chroma_format_idc = 0;

    if (ctx->esd->decoderConfig->decoderSpecificInfo && ctx->esd->decoderConfig->decoderSpecificInfo->data) {
        HEVCState hevc;
        memset(&hevc, 0, sizeof(HEVCState));

        cfg = gf_odf_hevc_cfg_read(ctx->esd->decoderConfig->decoderSpecificInfo->data, ctx->esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
        if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
        ctx->nalu_size_length = cfg->nal_unit_size;

        for (i=0; i< gf_list_count(cfg->param_array); i++) {
            GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(cfg->param_array, i);
            for (j=0; j< gf_list_count(ar->nalus); j++) {
                GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
                s32 idx;
                u16 hdr = sl->data[0] << 8 | sl->data[1];

                if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
                    idx = gf_media_hevc_read_sps(sl->data, sl->size, &hevc);
                    ctx->width = MAX(hevc.sps[idx].width, ctx->width);
                    ctx->height = MAX(hevc.sps[idx].height, ctx->height);
                    ctx->luma_bpp = MAX(hevc.sps[idx].bit_depth_luma, ctx->luma_bpp);
                    ctx->chroma_bpp = MAX(hevc.sps[idx].bit_depth_chroma, ctx->chroma_bpp);
                    ctx->chroma_format_idc  = hevc.sps[idx].chroma_format_idc;

                    ctx->sps = (char *) malloc(4 + sl->size);
                    ctx->sps_size = sl->size;
                    prependStartCode(sl->data, ctx->sps, &ctx->sps_size);
                }
                else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
                    gf_media_hevc_read_vps(sl->data, sl->size, &hevc);

                    ctx->vps = (char *) malloc(4 + sl->size);
                    ctx->vps_size = sl->size;
                    prependStartCode(sl->data, ctx->vps, &ctx->vps_size);
                }
                else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
                    gf_media_hevc_read_pps(sl->data, sl->size, &hevc);

                    ctx->pps = (char *) malloc(4 + sl->size);
                    ctx->pps_size = sl->size;
                    prependStartCode(sl->data, ctx->pps, &ctx->pps_size);          
                }
            }
        }
        gf_odf_hevc_cfg_del(cfg);
    } else {
        ctx->nalu_size_length = 0;
    }

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
    
    ctx->mime = "video/hevc";
    
    u32 csd0_size = ctx->sps_size + ctx-> pps_size + ctx->vps_size;
    char *csd0 = (char *) malloc(csd0_size);

    u32 k;

    for(k = 0; k < csd0_size; k++) {

       if(k < ctx->vps_size) {
            csd0[k] = ctx->vps[k];
       }
       else if (k < ctx-> vps_size + ctx->sps_size ) {
            csd0[k] = ctx->sps[k - ctx->vps_size];
       }
       else csd0[k] = ctx->pps[k - ctx->vps_size - ctx->sps_size];
    }

    AMediaFormat_setBuffer(ctx->format, "csd-0", csd0, csd0_size);
    return GF_OK;
}


static GF_Err MCDec_InitDecoder(MCDec *ctx) {

    GF_Err err;
    ctx->format = AMediaFormat_new();

    if(!ctx->format) {
        LOGE("AMediaFormat_new failed");
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

    ctx->frame_rate = 30;
    ctx->dequeue_timeout = 100000;
    ctx->stride = ctx->width;
    initMediaFormat(ctx, ctx->format);

    ctx->codec = AMediaCodec_createDecoderByType(ctx->mime);

    if(!ctx->codec) {
        LOGE("AMediaCodec_createDecoderByType failed");
        return GF_CODEC_NOT_FOUND;
    }

    if( AMediaCodec_configure(
        ctx->codec, // codec   
        ctx->format, // format
        NULL,  //surface 
        NULL, // crypto 
        0 // flags 
        )     != AMEDIA_OK) 
    {
        LOGE("AMediaCodec_configure failed");
        return GF_BAD_PARAM;
    }

    if( AMediaCodec_start(ctx->codec) != AMEDIA_OK){
        LOGE("AMediaCodec_start failed");
        return GF_BAD_PARAM;
    }

    ctx->inputEOS = GF_FALSE;
    ctx->outputEOS = GF_FALSE;
    ctx->counter = 0;

    LOGI("Video size: %d x %d", ctx->width, ctx->height);
    return GF_OK;
}


static GF_Err MCDec_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    ctx->esd = esd;
    GF_Err e;

    //check AVC config
    if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_AVC) {
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
            ctx->width=ctx->height=128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_NV12;
            return GF_OK;
        } else {
            GF_AVCConfigSlot *slc;
            GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
            slc = gf_list_get(cfg->sequenceParameterSets, 0);
            if (slc) {
                ctx->sps = slc->data;
                ctx->sps_size = slc->size;
            }
            slc = gf_list_get(cfg->pictureParameterSets, 0);
            if (slc) {
                ctx->pps = slc->data;
                ctx->pps_size = slc->size;
            }
            
            if (ctx->sps && ctx->pps) {
                e = MCDec_InitDecoder(ctx);
            } else {
                ctx->nalu_size_length = cfg->nal_unit_size;
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
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
            ctx->width=ctx->height=128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_NV12;
        } else {
            return MCDec_InitDecoder(ctx);
        }
    }

    return MCDec_InitDecoder(ctx);
}



static GF_Err MCDec_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    
    if(AMediaCodec_stop(ctx->codec) != AMEDIA_OK) {
        LOGE("AMediaCodec_stop failed");
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
        capability->cap.valueInt = 6;
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


static GF_Err MCDec_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
    return GF_NOT_SUPPORTED;
}


static GF_Err MCDec_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    *outBufferLength = 0;

    if(!ctx->inputEOS) {

        ssize_t inIndex = AMediaCodec_dequeueInputBuffer(ctx->codec, ctx->dequeue_timeout);
        //LOGV("Input Buffer Index: %d", inIndex);

        if (inIndex >= 0) {

            size_t inSize;
            char *buffer = (char *)AMediaCodec_getInputBuffer(ctx->codec, inIndex, &inSize);

            if (inBufferLength > inSize)  {
                LOGE("The returned buffer is too small");
                return GF_BUFFER_TOO_SMALL;
            }

        switch (ctx->esd->decoderConfig->objectTypeIndication) {

            case GPAC_OTI_VIDEO_AVC:
                
                if(inBuffer[4] == 0x67) { //check for sps

                    u32 start = ctx->sps_size + ctx->pps_size;

                    u32 i;    
                    for (i = start; i < inBufferLength ; ++i) {
                        buffer[i - start] = inBuffer[i];
                    }
                }

                else {
                    memcpy(buffer, inBuffer, inBufferLength);
                }

                buffer[0] = 0x00;
                buffer[1] = 0x00;
                buffer[2] = 0x00;
                buffer[3] = 0x01;
                break;


            case GPAC_OTI_VIDEO_HEVC:

                if(inBuffer[4] == 0x40) { //check for vps
                    u32 start = ctx->vps_size + ctx->sps_size + ctx->pps_size;
                    u32 i;
                                    
                    for (i = start; i < inBufferLength ; ++i) {
                        buffer[i - start] = inBuffer[i];
                    }
                }
                else {
                    memcpy(buffer, inBuffer, inBufferLength);
                }

                buffer[0] = 0x00;
                buffer[1] = 0x00;
                buffer[2] = 0x00;
                buffer[3] = 0x01;
                break;

           default:
                memcpy(buffer, inBuffer, inBufferLength);
                break;
        }

            if(!inBuffer || inBufferLength == 0){
                LOGI("AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM input");
                ctx->inputEOS = true;
            }
    
            u64 presentationTimeUs = ctx->counter * 1000000 / ctx->frame_rate;
            ctx->counter++;
                
            if(AMediaCodec_queueInputBuffer(ctx->codec,
                                    inIndex,
                                    0, 
                                    inBufferLength,
                                    ctx->inputEOS ? 0 : presentationTimeUs,
                                    inBuffer ? 0 : AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
                ) != AMEDIA_OK)
                {
                LOGE("AMediaCodec_queueInputBuffer failed");
                return GF_BAD_PARAM;
            }

        } else {
            LOGI("Input Buffer not available.");
        }

    }

    if(!ctx->outputEOS) {

        AMediaCodecBufferInfo info;
        ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(ctx->codec, &info, ctx->dequeue_timeout);
        //LOGV("OutputIndex: %d", outIndex);

        switch(outIndex) {

            case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
                LOGI("AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED");
                ctx->format = AMediaCodec_getOutputFormat(ctx->codec);
                break;

            case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                LOGI("AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED");
                break;

            case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                LOGI("AMEDIACODEC_INFO_TRY_AGAIN_LATER");
                break;

            default:

                if (outIndex >= 0) {

                    if(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                        LOGI("AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM output");
                        ctx->outputEOS = true;
                    }

                    size_t outSize;
                    uint8_t *buffer = AMediaCodec_getOutputBuffer(ctx->codec, outIndex, &outSize);

                    if(!buffer) {
                        LOGE("AMediaCodec_getOutputBuffer failed");
                        *outBufferLength = 0;
                    } else {
                        
                        if(info.size < ctx->out_size)
                            *outBufferLength = info.size;

                        else *outBufferLength = ctx->out_size;
                    }

                    memcpy(outBuffer, buffer, *outBufferLength);
                    AMediaCodec_releaseOutputBuffer(ctx->codec, outIndex, false); 

                } else{
                    LOGE("Output Buffer not available");
                }
        }

    }
    return GF_OK;        
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

            if(sdkInt() >= 21){
                return GF_CODEC_SUPPORTED;
            }
            return GF_CODEC_NOT_SUPPORTED;


        case GPAC_OTI_VIDEO_MPEG4_PART2:
            return GF_CODEC_SUPPORTED;
    }

    return GF_CODEC_NOT_SUPPORTED;
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
    return (GF_BaseDecoder *) ifcd;
}


void DeleteMCDec(GF_BaseDecoder *ifcg)
{
    MCDec *ctx = (MCDec *)ifcg->privateStack;

    if(AMediaFormat_delete(ctx->format) != AMEDIA_OK){
        LOGE("AMediaFormat_delete failed");
    }
    
    if(AMediaCodec_delete(ctx->codec) != AMEDIA_OK) {
        LOGE("AMediaCodec_delete failed");
    }

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
