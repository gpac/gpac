/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / common ffmpeg filters
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

#define JPEGLIB_H
#include <gpac/filters.h>
#include <gpac/list.h>
#include <gpac/constants.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/dict.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>

#define GF_FFMPEG_DECODER_CONFIG GF_4CC('f','f','D','C')

#define GF_CODECID_FFMPEG GF_4CC('F','F','I','D')


//rendering/translating to internal supported formats for text streams is not yet implemented
//#define FF_SUB_SUPPORT


void ffmpeg_initialize();
void ffmpeg_registry_free(GF_FilterSession *session, GF_FilterRegister *reg, u32 nb_skip_begin);
GF_FilterArgs ffmpeg_arg_translate(const struct AVOption *opt);
void ffmpeg_setup_logs(u32 log_class);

enum{
	FF_REG_TYPE_DEMUX=0,
	FF_REG_TYPE_DECODE,
	FF_REG_TYPE_DEV_IN,
	FF_REG_TYPE_ENCODE,
};
void ffmpeg_expand_registry(GF_FilterSession *session, GF_FilterRegister *orig_reg, u32 reg_type);

u32 ffmpeg_pixfmt_from_gpac(u32 pfmt);
u32 ffmpeg_pixfmt_to_gpac(u32 pfmt);
u32 ffmpeg_audio_fmt_from_gpac(u32 sfmt);
u32 ffmpeg_audio_fmt_to_gpac(u32 sfmt);
u32 ffmpeg_codecid_from_gpac(u32 codec_id);
u32 ffmpeg_codecid_to_gpac(u32 codec_id);

void ffmpeg_set_enc_dec_flags(const AVDictionary *options, AVCodecContext *ctx);
void ffmpeg_set_mx_dmx_flags(const AVDictionary *options, AVFormatContext *ctx);

u32 ffmpeg_channel_layout_from_gpac(u32 gpac_ch_layout);
u32 ffmpeg_channel_layout_to_gpac(u32 ff_ch_layout);

