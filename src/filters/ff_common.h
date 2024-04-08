/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
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
#ifndef FFMPEG_DISABLE_AVDEVICE
#include <libavdevice/avdevice.h>
#endif
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
#define AVFMT_URL(_mux) _mux->filename
#else
#define AVFMT_URL(_mux) _mux->url
#endif

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
#define FF_FREE_PCK(_pkt)	av_free_packet(_pkt);
#define FF_RELEASE_PCK(_pkt)
#define FF_INIT_PCK(ctx, _pkt) { pkt = &ctx->pkt; av_init_packet(pkt); }
#define FF_OFMT_CAST	(AVOutputFormat *)
#define FF_IFMT_CAST	(AVInputFormat *)
#else
#define FF_FREE_PCK(_pkt)	av_packet_unref(_pkt);
#define FF_RELEASE_PCK(_pkt) av_packet_unref(_pkt);
#define FF_INIT_PCK(ctx, _pkt) { pkt = ctx->pkt; }
#define FF_OFMT_CAST
#define FF_IFMT_CAST
#endif

//consider stable bsf api after major version 59
#if (LIBAVCODEC_VERSION_MAJOR > 58)
#define FFMPEG_HAS_BSF
#include <libavcodec/bsf.h>
#endif

#if AV_VERSION_INT(LIBAVFORMAT_VERSION_MAJOR, LIBAFORMAT_VERSION_MINOR, 0) < AV_VERSION_INT(59,19, 0)
#define FFMPEG_OLD_CHLAYOUT
#endif


GF_FilterArgs ffmpeg_arg_translate(const struct AVOption *opt);
void ffmpeg_setup_logs(u32 log_class);

enum{
	FF_REG_TYPE_DEMUX=0,
	FF_REG_TYPE_DECODE,
	FF_REG_TYPE_DEV_IN,
	FF_REG_TYPE_ENCODE,
	FF_REG_TYPE_MUX,
	FF_REG_TYPE_AVF,
#ifdef FFMPEG_HAS_BSF
	FF_REG_TYPE_BSF,
#endif
};

GF_FilterRegister *ffmpeg_build_register(GF_FilterSession *session, GF_FilterRegister *orig_reg, const GF_FilterArgs *default_args, u32 nb_def_args, u32 reg_type);

enum AVPixelFormat ffmpeg_pixfmt_from_gpac(u32 pfmt, Bool no_warn);
u32 ffmpeg_pixfmt_to_gpac(enum AVPixelFormat pfmt, Bool no_warn);
//return GPAC pix format from codec tag
u32 ffmpeg_pixfmt_from_codec_tag(u32 codec_tag, Bool *is_full_range);
//check if FF pixfmt is an old format fullrange
Bool ffmpeg_pixfmt_is_fullrange(u32 pfmt);

u32 ffmpeg_audio_fmt_from_gpac(u32 sfmt);
u32 ffmpeg_audio_fmt_to_gpac(u32 sfmt);
u32 ffmpeg_codecid_from_gpac(u32 codec_id, u32 *ff_codectag);
u32 ffmpeg_codecid_to_gpac(u32 codec_id);
u32 ffmpeg_codecid_to_gpac_audio_fmt(u32 codec_id);

u32 ffmpeg_stream_type_from_gpac(u32 streamtype);
u32 ffmpeg_stream_type_to_gpac(u32 streamtype);

void ffmpeg_set_enc_dec_flags(const AVDictionary *options, AVCodecContext *ctx);
void ffmpeg_set_mx_dmx_flags(const AVDictionary *options, AVFormatContext *ctx);

u64 ffmpeg_channel_layout_from_gpac(u64 gpac_ch_layout);
u64 ffmpeg_channel_layout_to_gpac(u64 ff_ch_layout);

//this will destroy unknown_options if set
void ffmpeg_report_options(GF_Filter *filter, AVDictionary *unknown_options, AVDictionary *all_options);

void ffmpeg_register_set_dyn_help(GF_FilterRegister *reg);

//output allocated with av_malloc
GF_Err ffmpeg_extradata_from_gpac(u32 gpac_codec_id, const u8 *dsi_in, u32 dsi_in_size, u8 **dsi_out, u32 *dsi_out_size);
//output allocated with gf_malloc
GF_Err ffmpeg_extradata_to_gpac(u32 gpac_codec_id, const u8 *data, u32 size, u8 **dsi_out, u32 *dsi_out_size);

void ffmpeg_tags_from_gpac(GF_FilterPid *pid, AVDictionary **metadata);
void ffmpeg_tags_to_gpac(AVDictionary *metadata, GF_FilterPid *pid);

void ffmpeg_generate_gpac_dsi(GF_FilterPid *out_pid, u32 gpac_codec_id, u32 color_primaries, u32 transfer_characteristics, u32 colorspace, const u8 *data, u32 size);

#if (LIBAVCODEC_VERSION_MAJOR > 56)
/*fills codecpar from pid properties. This assumes the codecpar was properly initialized.
codec_id, codec_type, and extradata are NOT set by this function
*/
GF_Err ffmpeg_codec_par_from_gpac(GF_FilterPid *pid, AVCodecParameters *codecpar, u32 ffmpeg_timescale);

/*sets pid properties from codecpar:
codec_id, codec_type, and extradata are NOT set by this function
*/
GF_Err ffmpeg_codec_par_to_gpac(AVCodecParameters *codecpar, GF_FilterPid *opid, u32 ffmpeg_timescale);
#endif

GF_Err ffmpeg_update_arg(const char *log_name, void *ctx, AVDictionary **options, const char *arg_name, const GF_PropertyValue *arg_val);

void ffmpeg_check_threads(GF_Filter *filter, AVDictionary *options, AVCodecContext *codecctx);
