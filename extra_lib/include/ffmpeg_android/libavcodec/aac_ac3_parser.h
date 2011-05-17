/*
 * Common AAC and AC-3 parser prototypes
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2003 Michael Niedermayer
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_AAC_AC3_PARSER_H
#define AVCODEC_AAC_AC3_PARSER_H

#include <stdint.h>
#include "avcodec.h"
#include "parser.h"

typedef enum {
    AAC_AC3_PARSE_ERROR_SYNC        = -1,
    AAC_AC3_PARSE_ERROR_BSID        = -2,
    AAC_AC3_PARSE_ERROR_SAMPLE_RATE = -3,
    AAC_AC3_PARSE_ERROR_FRAME_SIZE  = -4,
    AAC_AC3_PARSE_ERROR_FRAME_TYPE  = -5,
    AAC_AC3_PARSE_ERROR_CRC         = -6,
    AAC_AC3_PARSE_ERROR_CHANNEL_CFG = -7,
} AACAC3ParseError;

typedef struct AACAC3ParseContext {
    ParseContext pc;
    int frame_size;
    int header_size;
    int (*sync)(uint64_t state, struct AACAC3ParseContext *hdr_info,
            int *need_next_header, int *new_frame_start);

    int channels;
    int sample_rate;
    int bit_rate;
    int samples;
    int64_t channel_layout;

    int remaining_size;
    uint64_t state;

    int need_next_header;
    enum CodecID codec_id;
} AACAC3ParseContext;

int ff_aac_ac3_parse(AVCodecParserContext *s1,
                     AVCodecContext *avctx,
                     const uint8_t **poutbuf, int *poutbuf_size,
                     const uint8_t *buf, int buf_size);

#endif /* AVCODEC_AAC_AC3_PARSER_H */
