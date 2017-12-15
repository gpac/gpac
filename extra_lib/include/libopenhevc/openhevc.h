/*
 * openhevc.h wrapper to openhevc or ffmpeg
 * Copyright (c) 2012-2013 Micka�l Raulet, Wassim Hamidouche, Gildas Cocherel, Pierre Edouard Lepere
 *
 * This file is part of openhevc.
 *
 * openHevc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * openhevc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with openhevc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef OPENHEVC_H
#define OPENHEVC_H

#define NV_VERSION  "3.0" ///< Current software version

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdarg.h>
#include "ohconfig.h"

#if OHCONFIG_AVCBASE
    #define OPENHEVC_HAS_AVC_BASE
#endif

#define USE_SDL 1


typedef void* OHHandle;

typedef struct OHRational{
    int num; ///< numerator
    int den; ///< denominator
} OHRational;

//TODO try to remove
enum OHChromaFormat {
    OH_YUV420 = 0,
    OH_YUV422,
    OH_YUV444,
};

typedef enum {
    OH_THREAD_FRAME = 1,
    OH_THREAD_SLICE = 2,
    OH_THREAD_FRAMESLICE = 4
} OHThreadType;

typedef enum {
    OH_DECODER_HEVC = 0,
    OH_DECODER_SHVC,
    OH_DECODER_LHVC,
    OH_DECODER_HYBRID
} OHDecoderType;

typedef enum {
    OHEVC_LOG_PANIC   = 0,
    OHEVC_LOG_FATAL   = 8,
    OHEVC_LOG_ERROR   = 16,
    OHEVC_LOG_WARNING = 24,
    OHEVC_LOG_INFO    = 32,
    OHEVC_LOG_VERBOSE = 40,
    OHEVC_LOG_DEBUG   = 48,
    OHEVC_LOG_TRACE   = 56
} OHEVC_LogLevel;

//TODO unused
typedef struct OHScalabilityMask {
    uint8_t multiview;
    uint8_t scalability;
    uint8_t aux_picture;
} OHScalabilityMask;


//TODO unused
typedef enum {
    OH_PROFILE_MAIN = 1,
    OH_PROFILE_MAIN10,
    OH_PROFILE_MAIN_STILL_PICTURE,
    OH_PROFILE_REXT,
}OHProfile;

//TODO (generic wrapper for sequence specific HEVC info (scalability mask, profile...))
typedef struct OHInfo {
    OHProfile profile;
    OHScalabilityMask scal_mask;
}OHInfo;


//TODO Refactor
typedef struct OHFrameInfo {
    int         width;
    int         height;
    int         linesize_y;
    int         linesize_cb;
    int         linesize_cr;
    int         bitdepth;
    int         chromat_format;
    OHRational  sample_aspect_ratio;
    OHRational  framerate;
    int         display_picture_number; //poc
    int         flag; //progressive, interlaced, interlaced top field first, interlaced bottom field first.
    int64_t     pts;
} OHFrameInfo;


typedef struct OHPacket {
    /**
     * Container for packets from different layers when using layered HEVC
     */
    uint8_t **pkt_list;
    /**
     * Sizes of each packets
     */
    int *pkt_size_list;
    /**
     * Time stamps list of each packets
     */
    int *pts_list;
}OHPacket;


typedef struct OHFrame {
    void**       data_y_p;
    void**       data_cb_p;
    void**       data_cr_p;
    OHFrameInfo  frame_par;
} OHFrame;

typedef struct OHFrame_cpy {
    void*        data_y;
    void*        data_cb;
    void*        data_cr;
    OHFrameInfo  frame_par;
} OHFrame_cpy;


//TODO: refactor into only one init function + comment
/**
 * Allocate a general decoder context list for each layer (up to MAX_DECODERS)
 * Allocate a decoder context for each layers
 * Find a registered HEVC decoder matching AV_CODEC_ID_HEVC.
 * Init packets for each layer decoders
 * @param number nb_pthreads of the requested decoder
 * @param type thread_type of the threading design in use for decoding
 * @return A decoder list if a decoder of each layer were found, NULL otherwise.
 */
OHHandle oh_init(int nb_threads, int thread_type);

/**
 * Allocate a general decoder context list for each layer (up to MAX_DECODERS)
 * Allocate a decoder context for each layer
 * Find a registered H264 decoder matching AV_CODEC_ID_H264 for base layer.
 * Find a registered LHVC decoder matching AV_CODEC_ID_HEVC for other layers.
 * Init packets for each layer decoders
 * @param number nb_pthreads of the requested decoder
 * @param type thread_type of the threading design in use for decoding
 * @return A decoder list if a decoder of each layer were found, NULL otherwise.
 */
OHHandle oh_init_lhvc(int nb_threads, int thread_type);

#if OHCONFIG_AVCBASE
/**
 * Allocate a general decoder context list for each layer (up to MAX_DECODERS)
 * Allocate a decoder context for each layer
 * Find a registered H264 decoder matching AV_CODEC_ID_H264.
 * Init packets for each decoders
 * @param number nb_threads of the requested decoder
 * @param type thread_type of the threading design in use for decoding
 * @return A decoder list if a decoder of each layer were found, NULL otherwise.
 */
OHHandle oh_init_h264(int nb_threads, int thread_type);
#endif

/**
 * Initialize the decoders contexts for each layers.
 *
 * The different layers decoders must have been initialized prior to the call to
 * this function.
 *
 * @param oh_hdl The codec context list of current decoders to be used
 * @return one on success, a negative value on error
 */
int oh_start(OHHandle oh_hdl);

//TODO use OHPacket structure to handle lhvc, hevc and shvc into a same function
// call
/**
 * Decode the video frame of size pkt_size from pkt_data into picture.
 * @note The packet can be a conglomerate of NAL from different layer since
 * each NAL header contains a layer ID, each HEVC parser can ignore NALs
 * non related to its layer.
 * @param oh_hdl the codec context list of current decoders
 * @param[in] pkt_data The input NAL raw data to be decoded.
 * @param[in] pkt_size The input NAL raw data length in bytes.
 * @param[in] pkt_pts the packet time stamp
 * @return On error a negative value is returned, otherwise the number of bytes
 * used or zero if no frame could be decompressed
 * @deprecated Use oh_send_packet() and oh_receive_frame().
 */
int  oh_decode(OHHandle oh_hdl, const unsigned char *pkt_data,
               int pkt_size, int64_t pkt_pts);

#if OHCONFIG_AVCBASE
/**
 * Decode the video frame of size pkt_size from pkt_data into picture, in case
 * of non HEVC base layer (LHVC)
 * @param oh_hdl the codec context list of current decoders
 * @param[in] pkt_data The input NAL raw data to be decoded.
 * @param[in] pkt_size The input NAL raw data length in bytes.
 * @param[in] pts the packet time stamp
 * @return On error a negative value is returned, otherwise the number of bytes
 * used or zero if no frame could be decompressed
 * @deprecated Use oh_send_packet() and oh_receive_frame().
 */
int  oh_decode_lhvc(OHHandle oh_hdl, const unsigned char *pkt_data_bl,
                    const unsigned char *pkt_data_el, int pkt_size_bl,
                    int pkt_size_el, int64_t pts_bl, int64_t pts_el);

//TODO define an OHPacket struct to handle both lhevc and hevc case
//TODO oh_send/receive_packet
#endif

void oh_extradata_cpy(OHHandle oh_hdl, unsigned char *extra_data,
                      int extra_size_alloc);

#if OHCONFIG_AVCBASE
void oh_extradata_cpy_lhvc(OHHandle oh_hdl, unsigned char *extra_data_linf,
                           unsigned char *extra_data_lsup,
                           int extra_size_alloc_linf,
                           int extra_size_allocl_sup);
#endif

/**
 * Flush all decoders.
 *
 * @param oh_hdl The codec context list of current decoders
 */
void oh_flush(OHHandle oh_hdl);

/**
 * Flush a decoder given its ID.
 *
 * @param oh_hdl     The codec context list of current decoders
 * @param layer_idx  The LAYER_ID of the decoder to be flushed
 */
void oh_flush_shvc(OHHandle oh_hdl, int layer_idx);

/**
 * Close and free all decoders, parsers and pictures.
 *
 * @param oh_hdl The codec context list of current decoders
 */
void oh_close(OHHandle oh_hdl);

/**
 * Update informations on the output frame
 *
 * @param oh_hdl      The codec context list of current decoders
 * @param got_picture 1 if the decoder outputed a picture, 0 otherwise
 * @param oh_frame    pointer to the output OHFrame
 */
int  oh_output_update(OHHandle oh_hdl, int got_picture, OHFrame *oh_frame);

int  oh_output_update_from_layer(OHHandle oh_hdl, OHFrame *oh_frame, int layer_id);

/**
 * Update the informations on the output frame
 *
 * @param oh_hdl       The codec context list of current decoders
 * @param oh_frameinfo pointer to the output frame info to be updated
 */
void oh_frameinfo_update(OHHandle oh_hdl, OHFrameInfo *oh_frameinfo);

void oh_frameinfo_update_from_layer(OHHandle oh_hdl, OHFrameInfo *oh_frameinfo,int layer_id);


/**
 * Reinterpret frame information for a cropped copy of a frame
 *
 * @param oh_hdl      The codec context list of current decoders
 * @param got_picture 1 if the decoder outputed a picture, 0 otherwise
 * @param oh_frame    pointer to the output OHFrame
 */
void oh_cropped_frameinfo(OHHandle oh_hdl, OHFrameInfo *oh_frame_info);

int  oh_cropped_frameinfo_from_layer(OHHandle oh_hdl, OHFrameInfo *oh_frame_info, int layer_id);

/**
 * Copy and crop the ouput output to a new one
 *
 * @param oh_hdl      The codec context list of current decoders
 * @param got_picture 1 if the decoder outputed a picture, 0 otherwise
 * @param oh_frame    pointer to the output OHFrame
 */
int oh_output_cropped_cpy(OHHandle oh_hdl, OHFrame_cpy *oh_frame);

int oh_output_cropped_cpy_from_layer(OHHandle openHevcHandle, OHFrame_cpy *oh_frame, int layer_id);

/**
 * Enable frame level SEI checksum for all layers
 *
 * @param oh_hdl The codec context list of current decoders
 * @param val    1 if enabled, 0 otherwise
 */
void oh_enable_sei_checksum(OHHandle oh_hdl, int val);

/**
 * Disable frame cropping SEI for all layers
 *
 * @param oh_hdl The codec context list of current decoders
 * @param val    1 if enabled, 0 otherwise
 */
void oh_disable_cropping(OHHandle oh_hdl, int val);

/**
 * Set the level of the logging output
 *
 * @param oh_hdl    The codec context list of current decoders
 * @param log_level The desired log_level
 */
void oh_set_log_level(OHHandle oh_hdl, OHEVC_LogLevel log_level);

/**
 * Enable frame level SEI checksum for
 *
 * @param oh_hdl    The codec context list of current decoders
 * @param log_level The desired log_level
 */
void oh_set_log_callback(OHHandle oh_hdl, void (*callback)(void*, int, const char*, va_list));

/**
 * Select temporal layer for HEVC streams
 *
 * @param oh_hdl The codec context list of current decoders
 * @param val    The desired temporal id
 */
void oh_select_temporal_layer(OHHandle oh_hdl, int val);

/**
 * Select the higher active layer for HEVC streams and disable decoding of all
 * upper layers
 *
 * @param oh_hdl The codec context list of current decoders
 * @param val    The desired active layer id
 */
void oh_select_active_layer(OHHandle oh_hdl, int val);

/**
 * Select view layer output for SHVC streams
 *
 * @param oh_hdl The codec context list of current decoders
 * @param val    The desired temporal id
 */
void oh_select_view_layer(OHHandle oh_hdl, int val);

#if OHCONFIG_ENCRYPTION
void oh_set_crypto_mode(OHHandle oh_hdl, int val);
void oh_set_crypto_key(OHHandle oh_hdl, uint8_t *val);
#endif
/**
 * Enable Green filters
 *
 * @param oh_hdl The codec context list of current decoders
 * @param green_param The desired green parameters
 * @param green_verbose The desired green verbose level
 */
void oh_enable_green(OHHandle oh_hdl, const char *green_param, int green_verbose);

/**
 * @return val    The current OpenHEVC version
 */
const unsigned oh_version(OHHandle oh_hdl);

unsigned openhevc_version(void);

const char *openhevc_ident(void);
const char *openhevc_configuration(void);

#ifdef __cplusplus
}
#endif

#endif // OPENHEVC_H
