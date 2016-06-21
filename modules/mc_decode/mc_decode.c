#include <gpac/modules/codec.h>

#include <jni.h>

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaFormat.h"


#include <android/log.h>

#define TAG "mc_decode"

#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, TAG,  __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, TAG,  __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, TAG,  __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, TAG,  __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, TAG,  __VA_ARGS__)


typedef struct 
{
    AMediaCodec *codec;
    AMediaFormat *format;

    u32 width;
    u32 height;
    u32 stride;

    u32 max_input_size;
    u32 pix_fmt;

    GF_ESD *esd;
    GF_Err last_error;

    Bool codec_specific_data;
    Bool inputEOS;
    Bool outputEOS;
    
    u32 out_size;   
    u32 pixel_ar;

} MCDec;



//todo initialise other items
static GF_Err MCDec_InitDecoder(MCDec *ctx, const char *mime) {

    LOGD("MCDec_InitDecoder called");

    ctx->codec = AMediaCodec_createDecoderByType(mime);
    ctx->format = AMediaFormat_new();

    if(!ctx->format) {
        LOGE("AMediaFormat_new() failed");
        return GF_CODEC_NOT_FOUND;
    }

    AMediaFormat_setString(ctx->format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_WIDTH, ctx->width);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_HEIGHT, ctx->height);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_STRIDE, ctx->stride);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_COLOR_FORMAT, ctx->pix_fmt);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, ctx->max_input_size);

    //set custom flags
    //uint8_t es[2] = {0x12, 0x12};
    //AMediaFormat_setBuffer(format, "csd-0", es, 2);

    ctx->inputEOS = false;
    ctx->outputEOS = false;

    media_status_t status = AMediaCodec_configure(
        ctx->codec, // codec   
        ctx->format, // format
        NULL,  //surface 
        NULL, // crypto 
        0 // flags 
        );

    if(status != AMEDIA_OK){
        LOGE("AMediaCodec_configure failed");
        return getGF_Err(status);
    }


    status = AMediaCodec_start(ctx->codec);

    if(status != AMEDIA_OK){
        LOGE("AMediaCodec_start failed");
        return getGF_Err(status);
    }

    return GF_OK;
}



static GF_Err MCDec_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
    LOGD("MCDec_AttachStream called");

    GF_Err e;
    MCDec *ctx = (MCDec *)ifcg->privateStack;
    ctx->esd = esd;

    //check AVC config
    if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) {
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
            //ctx->is_annex_b = GF_TRUE;
            ctx->width = ctx->height = 128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_YV12;
            return GF_OK;
        } else {
            ctx->codec_specific_data = true;

            return MCDec_InitDecoder(ctx, "video/avc");    // ??
        }

        return MCDec_InitDecoder(ctx, "video/avc");
    }


    //!!!add other video type files

/*
    //check VOSH config
    if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
        if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
            ctx->width=ctx->height=128;
            ctx->out_size = ctx->width*ctx->height*3/2;
            ctx->pix_fmt = GF_PIXEL_YV12;
        } else {
            return MCDec_InitDecoder(ctx, "vid");   
        }
    }
*/
    //return VTBDec_InitDecoder(ctx, GF_);
    return GF_NOT_SUPPORTED;
}




static GF_Err MCDec_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
    //MCDec *ctx = (MCDec *)ifcg->privateStack;
    LOGD("MCDec_DetachStream called");
    return GF_OK;
}


GF_Err getGF_Err(media_status_t err) {

    switch(err) {

    case AMEDIA_OK:                         return GF_OK;       
    case AMEDIA_ERROR_UNSUPPORTED:          return GF_NOT_SUPPORTED;
    case AMEDIA_ERROR_INVALID_PARAMETER:    return GF_BAD_PARAM;
    
    case AMEDIA_ERROR_MALFORMED:
    case AMEDIA_ERROR_INVALID_OBJECT:
    case AMEDIA_ERROR_BASE:
    default:                                return -1234;

    }
}


static GF_Err MCDec_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{

    LOGD("MCDec_GetCapabilities called");
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
        capability->cap.valueFloat = 30.0;              
        break;
    case GF_CODEC_PAR:
        capability->cap.valueInt = ctx->pixel_ar;       //!!
        break;
    case GF_CODEC_OUTPUT_SIZE:
        capability->cap.valueInt = ctx->out_size;       //!!
        break;
    case GF_CODEC_PIXEL_FORMAT:
        capability->cap.valueInt = ctx->pix_fmt;        
        break;
    case GF_CODEC_BUFFER_MIN:
        capability->cap.valueInt = 1;                   
        break;
    case GF_CODEC_BUFFER_MAX:
        capability->cap.valueInt = ctx->max_input_size;   
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
  /*  case GF_CODEC_FRAME_OUTPUT:
        capability->cap.valueInt = 1;                   
        break;
    */
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
    LOGV("MCDec_SetCapabilities called");
    return GF_OK;
}



static GF_Err MCDec_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
    LOGD("MCDec_ProcessData called");

    MCDec *ctx = (MCDec *)ifcg->privateStack;

    u32 DEQUEUE_TIMEOUT = 10000;

        if(!ctx->inputEOS && !inBuffer) {
            ssize_t inIndex = AMediaCodec_dequeueInputBuffer(ctx->codec, DEQUEUE_TIMEOUT);

            LOGV("Input Buffer Index: %d", inIndex);

            if (inIndex >= 0) {

                size_t inSize;
                uint8_t *buffer = AMediaCodec_getInputBuffer(ctx->codec, inIndex, &inSize);

                LOGV("Input Buffer size: %d", inSize);

                if (inBufferLength > inSize)  {
                    LOGE("The returned buffer is too small");
                    return GF_BUFFER_TOO_SMALL;
                }

                memcpy(buffer, inBuffer, inSize);

                //!!!!!!!get sample time of the video chunk'

                if(!inBuffer){
                    LOGI("AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM input");
                    ctx->inputEOS = true;
                }

                media_status_t status = AMediaCodec_queueInputBuffer(ctx->codec,
                                        inIndex,
                                        0, 
                                        inBufferLength,
                                        ctx->inputEOS ? 0 : 1000,   //!!!!!!!!!!!!!!presentation time
                                        !inBuffer ? 0 : AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
                    );
                
                

                if (status != AMEDIA_OK) {
                    LOGE("AMediaCodec_queueInputBuffer failed");
                    return GF_BAD_PARAM;
                }


            } else {
                LOGI("Input Buffer not available.");
            }

        }


        if(!ctx->outputEOS) {

            AMediaCodecBufferInfo info;
            ssize_t outIndex = AMediaCodec_dequeueOutputBuffer(ctx->codec, &info, DEQUEUE_TIMEOUT);
            
            switch(outIndex) {

                case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
                    LOGI("AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED");
                    ctx->format = AMediaCodec_getOutputFormat(ctx->codec);
                    
                    //AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_MIME, ctx->mime);
                    AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_WIDTH, (int32_t *)ctx->width);
                    AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_HEIGHT, (int32_t *)ctx->height);
                    AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_STRIDE, (int32_t *)ctx->stride);
                    AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_COLOR_FORMAT, (int32_t *)ctx->pix_fmt);
                    AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, (int32_t *)ctx->max_input_size);

                    break;

                case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                    LOGI("AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED");
                    break;

                case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                    LOGI("AMEDIACODEC_INFO_TRY_AGAIN_LATER");
                    break;

                default:

                    if (outIndex >=0) {

                        if(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                            LOGI("AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM output");
                            ctx->outputEOS = true;
                        }

                        size_t outSize;
                        uint8_t *buffer = AMediaCodec_getOutputBuffer(ctx->codec, outIndex, &outSize);
                        
                        LOGV("Output Buffer size: %d", outSize);

                        if(!buffer) {
                            LOGE("AMediaCodec_getOutputBuffer failed");
                            *outBufferLength = 0;
                        } else {
                            *outBufferLength = outSize;
                        }

                        //copying output
                        memcpy(outBuffer, buffer, *outBufferLength);

                        AMediaCodec_releaseOutputBuffer(ctx->codec, outIndex, false); 

                    } else{
                        LOGE("Output Buffer not available");
                    }
            }

        }


}


static u32 MCDec_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
    LOGD("MCDec_CanHandleStream called");

    if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;

    /*media type query*/
    if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

    switch (esd->decoderConfig->objectTypeIndication) {
    case GPAC_OTI_VIDEO_AVC:
    case GPAC_OTI_VIDEO_HEVC:
    case GPAC_OTI_VIDEO_SHVC:
        return GF_CODEC_SUPPORTED;

    }
    return GF_CODEC_NOT_SUPPORTED;
}



//TODO
static const char *MCDec_GetCodecName(GF_BaseDecoder *dec)
{
    LOGV("MCDec_GetCodecName called");
    return "hardware AVC|H264";
}



GF_BaseDecoder *NewMCDec()
{
    LOGD("NewMCDec called");

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
    //ifcd->GetOutputFrame = MCDec_GetOutputFrame;        //ifcd->GetOutputFrame not recognised
    return (GF_BaseDecoder *) ifcd;
}


void DeleteMCDec(GF_BaseDecoder *ifcg)
{
    LOGV("DeleteMCDec called");

    MCDec *ctx = (MCDec *)ifcg->privateStack;

    //AMediaExtractor_delete(ctx->extractor);
    AMediaCodec_delete(ctx->codec);
    //ANativeWindow_release(ctx->window);

    gf_free(ctx);
    gf_free(ifcg);
}



GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
    LOGV("QueryInterfaces called");

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
    LOGV("LoadInterface called");

#ifndef GPAC_DISABLE_AV_PARSERS
    if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewMCDec();
#endif
    return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
    LOGV("ShutdownInterface called");

    switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_AV_PARSERS
    case GF_MEDIA_DECODER_INTERFACE:
        DeleteMCDec((GF_BaseDecoder*)ifce);
        break;
#endif
    }
}

GPAC_MODULE_STATIC_DECLARATION( mc )



/*
//select track using MediaExtractor
u32 selectTrack(MCDec *dec) 
{
    int trackCount = (int) AMediaExtractor_getTrackCount(dec->extractor);
    size_t i;
    LOGV("Total num. of tracks: %d", trackCount);
    for (i = 0; i < trackCount; ++i) {
        AMediaFormat *format = AMediaExtractor_getTrackFormat(dec->extractor, i);

        const char *mime;

        bool b = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);

        if(!b) {
            LOGE("No mime type");
            return -1;
        }
        else LOGI("Track num.: %d, mime type: %s ", i, mime);

        if (!strncmp(mime, "video/", 6)) {
            LOGI("Selected Track num.: %d, mime type: %s", i, mime);
            AMediaExtractor_selectTrack(dec->extractor, i);

            AMediaExtractor_selectTrack(dec->extractor, i);
            dec->codec = AMediaCodec_createDecoderByType(mime);
            AMediaCodec_configure(dec->codec, format, dec->window, NULL, 0);
            AMediaFormat_delete(format);
            AMediaCodec_start(dec->codec);
            //dec->width = AMediaFormat_getInteger(AMediaFormat_KEY_WIDTH);
            //dec->height = AMediaFormat_getInteger(AMediaFormat_KEY_HEIGHT);
            LOGI("Video size = %d x %d ",dec->width, dec->height);
            AMediaFormat_delete(format);
            return i;
        }

    }
    LOGE("Video Track not found");
    return  -1;
}
*/



