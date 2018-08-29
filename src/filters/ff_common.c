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

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

#include "ff_common.h"


#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "avutil")
#  pragma comment(lib, "avformat")
#  pragma comment(lib, "avcodec")
#  pragma comment(lib, "avdevice")
#  pragma comment(lib, "swscale")
# endif
#endif


static Bool ffmpeg_init = GF_FALSE;

typedef struct
{
	u32 ff_pf;
	u32 gpac_pf;
} GF_FF_PFREG;

static const GF_FF_PFREG FF2GPAC_PixelFormats[] =
{
	{AV_PIX_FMT_YUV420P, GF_PIXEL_YUV},
	{AV_PIX_FMT_YUV420P10LE, GF_PIXEL_YUV_10},
	{AV_PIX_FMT_YUV422P, GF_PIXEL_YUV422},
	{AV_PIX_FMT_YUV422P10LE, GF_PIXEL_YUV422_10},
	{AV_PIX_FMT_YUV444P, GF_PIXEL_YUV444},
	{AV_PIX_FMT_YUV444P10LE, GF_PIXEL_YUV444_10},
	{AV_PIX_FMT_RGBA, GF_PIXEL_RGBA},
	{AV_PIX_FMT_RGB24, GF_PIXEL_RGB},
	{AV_PIX_FMT_BGR24, GF_PIXEL_BGR},
	{AV_PIX_FMT_UYVY422, GF_PIXEL_UYVY},
	{AV_PIX_FMT_YUYV422, GF_PIXEL_YUYV},
	{AV_PIX_FMT_NV12, GF_PIXEL_NV12},
	{AV_PIX_FMT_NV21, GF_PIXEL_NV21},
	{AV_PIX_FMT_0RGB, GF_PIXEL_XRGB},
	{AV_PIX_FMT_RGB0, GF_PIXEL_RGBX},
	{AV_PIX_FMT_0BGR, GF_PIXEL_XBGR},
	{AV_PIX_FMT_BGR0, GF_PIXEL_BGRX},
	{0},
};

u32 ffmpeg_pixfmt_from_gpac(u32 pfmt)
{
	u32 i=0;
	while (FF2GPAC_PixelFormats[i].gpac_pf) {
		if (FF2GPAC_PixelFormats[i].gpac_pf == pfmt)
			return FF2GPAC_PixelFormats[i].ff_pf;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFMPEG] Unmapped GPAC pixel format %s, patch welcome\n", gf_4cc_to_str(pfmt) ));
	return 0;
}

u32 ffmpeg_pixfmt_to_gpac(u32 pfmt)
{
	u32 i=0;
	while (FF2GPAC_PixelFormats[i].gpac_pf) {
		if (FF2GPAC_PixelFormats[i].ff_pf == pfmt)
			return FF2GPAC_PixelFormats[i].gpac_pf;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFMPEG] Unmapped FFMPEG pixel format %d, patch welcome\n", pfmt ));
	return 0;
}


typedef struct
{
	u32 ff_sf;
	u32 gpac_sf;
} GF_FF_AFREG;

static const GF_FF_AFREG FF2GPAC_AudioFormats[] =
{
	{AV_SAMPLE_FMT_U8, GF_AUDIO_FMT_U8},
	{AV_SAMPLE_FMT_S16, GF_AUDIO_FMT_S16},
	{AV_SAMPLE_FMT_S32, GF_AUDIO_FMT_S32},
	{AV_SAMPLE_FMT_FLT, GF_AUDIO_FMT_FLT},
	{AV_SAMPLE_FMT_DBL, GF_AUDIO_FMT_DBL},
	{AV_SAMPLE_FMT_U8P, GF_AUDIO_FMT_U8P},
	{AV_SAMPLE_FMT_S16P, GF_AUDIO_FMT_S16P},
	{AV_SAMPLE_FMT_S32P, GF_AUDIO_FMT_S32P},
	{AV_SAMPLE_FMT_FLTP, GF_AUDIO_FMT_FLTP},
	{AV_SAMPLE_FMT_DBLP, GF_AUDIO_FMT_DBLP},
	{0},
};

u32 ffmpeg_audio_fmt_from_gpac(u32 sfmt)
{
	u32 i=0;
	while (FF2GPAC_AudioFormats[i].gpac_sf) {
		if (FF2GPAC_AudioFormats[i].gpac_sf == sfmt)
			return FF2GPAC_AudioFormats[i].ff_sf;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFMPEG] Unmapped GPAC audio format %s, patch welcome\n", gf_4cc_to_str(sfmt) ));
	return 0;
}

u32 ffmpeg_audio_fmt_to_gpac(u32 sfmt)
{
	u32 i=0;
	while (FF2GPAC_AudioFormats[i].gpac_sf) {
		if (FF2GPAC_AudioFormats[i].ff_sf == sfmt)
			return FF2GPAC_AudioFormats[i].gpac_sf;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFMPEG] Unmapped FFMPEG audio format %d, patch welcome\n", sfmt ));
	return 0;
}

typedef struct
{
	u32 ff_ch_mask;
	u32 gpac_ch_mask;
} GF_FF_LAYOUTREG;

static const GF_FF_LAYOUTREG FF2GPAC_ChannelLayout[] =
{
	{AV_CH_FRONT_LEFT, GF_AUDIO_CH_FRONT_LEFT},
	{AV_CH_FRONT_RIGHT, GF_AUDIO_CH_FRONT_RIGHT},
	{AV_CH_FRONT_CENTER, GF_AUDIO_CH_FRONT_CENTER},
	{AV_CH_LOW_FREQUENCY, GF_AUDIO_CH_LFE},
	{AV_CH_BACK_LEFT, GF_AUDIO_CH_BACK_LEFT},
	{AV_CH_BACK_RIGHT, GF_AUDIO_CH_BACK_RIGHT},
	{AV_CH_FRONT_LEFT_OF_CENTER, GF_AUDIO_CH_LEFT_CENTER},
	{AV_CH_FRONT_RIGHT_OF_CENTER, GF_AUDIO_CH_RIGHT_CENTER},
	{AV_CH_BACK_CENTER, GF_AUDIO_CH_BACK_CENTER},
	{AV_CH_SIDE_LEFT, GF_AUDIO_CH_SIDE_LEFT},
	{AV_CH_SIDE_RIGHT, GF_AUDIO_CH_SIDE_RIGHT},
	{AV_CH_TOP_CENTER, GF_AUDIO_CH_TOP_CENTER},
	{AV_CH_TOP_FRONT_LEFT, GF_AUDIO_CH_TOP_FRONT_LEFT},
	{AV_CH_TOP_FRONT_CENTER, GF_AUDIO_CH_TOP_FRONT_CENTER},
	{AV_CH_TOP_FRONT_RIGHT, GF_AUDIO_CH_TOP_FRONT_RIGHT},
	{AV_CH_TOP_BACK_LEFT, GF_AUDIO_CH_TOP_BACK_LEFT},
	{AV_CH_TOP_BACK_CENTER, GF_AUDIO_CH_TOP_BACK_CENTER},
	{AV_CH_TOP_BACK_RIGHT, GF_AUDIO_CH_TOP_BACK_RIGHT},

/*	{AV_CH_STEREO_LEFT, },
	{AV_CH_STEREO_RIGHT, },
	{AV_CH_WIDE_LEFT, },
	{AV_CH_WIDE_RIGHT, },
	{AV_CH_SURROUND_DIRECT_LEFT, },
	{AV_CH_SURROUND_DIRECT_RIGHT, },
	{AV_CH_LOW_FREQUENCY_2, },
*/

};

u32 ffmpeg_channel_layout_from_gpac(u32 gpac_ch_layout)
{
	u32 i, nb_layout = sizeof(FF2GPAC_ChannelLayout) / sizeof(GF_FF_LAYOUTREG);
	u32 res = 0;
	for (i=0; i<nb_layout; i++) {
		if (FF2GPAC_ChannelLayout[i].gpac_ch_mask & gpac_ch_layout)
			res |= FF2GPAC_ChannelLayout[i].ff_ch_mask;
	}
	return res;
}
u32 ffmpeg_channel_layout_to_gpac(u32 ff_ch_layout)
{
	u32 i, nb_layout = sizeof(FF2GPAC_ChannelLayout) / sizeof(GF_FF_LAYOUTREG);
	u32 res = 0;
	for (i=0; i<nb_layout; i++) {
		if (FF2GPAC_ChannelLayout[i].ff_ch_mask & ff_ch_layout)
			res |= FF2GPAC_ChannelLayout[i].gpac_ch_mask;
	}
	return res;
}


typedef struct
{
	u32 ff_codec_id;
	u32 gpac_codec_id;
} GF_FF_CIDREG;

static const GF_FF_CIDREG FF2GPAC_CodecIDs[] =
{
	{AV_CODEC_ID_MP2, GF_CODECID_MPEG_AUDIO},
	{AV_CODEC_ID_MP3, GF_CODECID_MPEG2_PART3},
	{AV_CODEC_ID_AAC, GF_CODECID_AAC_MPEG4},
	{AV_CODEC_ID_AC3, GF_CODECID_AC3},
	{AV_CODEC_ID_EAC3, GF_CODECID_EAC3},
	{AV_CODEC_ID_AMR_NB, GF_CODECID_AMR},
	{AV_CODEC_ID_AMR_WB, GF_CODECID_AMR_WB},
	{AV_CODEC_ID_QCELP, GF_CODECID_QCELP},
	{AV_CODEC_ID_EVRC, GF_CODECID_EVRC},
	{AV_CODEC_ID_SMV, GF_CODECID_SMV},
	{AV_CODEC_ID_VORBIS, GF_CODECID_VORBIS},
	{AV_CODEC_ID_FLAC, GF_CODECID_FLAC},
	{AV_CODEC_ID_SPEEX, GF_CODECID_SPEEX},
	{AV_CODEC_ID_THEORA, GF_CODECID_THEORA},
	{AV_CODEC_ID_MPEG4, GF_CODECID_MPEG4_PART2},
	{AV_CODEC_ID_H264, GF_CODECID_AVC},
	{AV_CODEC_ID_HEVC, GF_CODECID_HEVC},
	{AV_CODEC_ID_MPEG1VIDEO, GF_CODECID_MPEG1},
	{AV_CODEC_ID_MPEG2VIDEO, GF_CODECID_MPEG2_SIMPLE},
	{AV_CODEC_ID_MPEG2VIDEO, GF_CODECID_MPEG2_MAIN},
	{AV_CODEC_ID_MPEG2VIDEO, GF_CODECID_MPEG2_HIGH},
	{AV_CODEC_ID_MPEG2VIDEO, GF_CODECID_MPEG2_SPATIAL},
	{AV_CODEC_ID_MPEG2VIDEO, GF_CODECID_MPEG2_SNR},
	{AV_CODEC_ID_MPEG2VIDEO, GF_CODECID_MPEG2_422},
	{AV_CODEC_ID_H263, GF_CODECID_S263},
	{AV_CODEC_ID_H263, GF_CODECID_H263},
	{AV_CODEC_ID_MJPEG, GF_CODECID_JPEG},
	{AV_CODEC_ID_PNG, GF_CODECID_PNG},
	{AV_CODEC_ID_JPEG2000, GF_CODECID_J2K},
	{AV_CODEC_ID_AV1, GF_CODECID_AV1},
	{AV_CODEC_ID_VP8, GF_CODECID_VP8},
	{AV_CODEC_ID_VP9, GF_CODECID_VP9},

	//RAW codecs
	{AV_CODEC_ID_RAWVIDEO, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S16LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S16BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U16LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U16BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S8, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U8, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_MULAW, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_ALAW, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S32LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S32BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U32LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U32BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S24LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S24BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U24LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_U24BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S24DAUD, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_ZORK, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S16LE_PLANAR, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_DVD, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_F32BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_F32LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_F64BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_F64LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_BLURAY, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_LXF, GF_CODECID_RAW},
	{AV_CODEC_ID_S302M, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S8_PLANAR, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S24LE_PLANAR, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S32LE_PLANAR, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S16BE_PLANAR, GF_CODECID_RAW},
#if LIBAVCODEC_VERSION_MAJOR >= 58
	{AV_CODEC_ID_PCM_S64LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_S64BE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_F16LE, GF_CODECID_RAW},
	{AV_CODEC_ID_PCM_F24LE, GF_CODECID_RAW},
#endif
	{0}
};

u32 ffmpeg_codecid_from_gpac(u32 codec_id)
{
	u32 i=0;
	while (FF2GPAC_CodecIDs[i].gpac_codec_id) {
		if (FF2GPAC_CodecIDs[i].gpac_codec_id == codec_id)
			return FF2GPAC_CodecIDs[i].ff_codec_id;
		i++;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[FFMPEG] Unmapped GPAC codec %s\n", gf_codecid_name(codec_id) ));
	return 0;
}

u32 ffmpeg_codecid_to_gpac(u32 codec_id)
{
	u32 i=0;
	while (FF2GPAC_CodecIDs[i].ff_codec_id) {
		if (FF2GPAC_CodecIDs[i].ff_codec_id == codec_id)
			return FF2GPAC_CodecIDs[i].gpac_codec_id;
		i++;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[FFMPEG] Unmapped FFMPEG codec ID %s\n", avcodec_get_name(codec_id) ));
	return 0;
}


//static void ff_log_callback(void *avcl, int level, const char *fmt, va_list vl) { }

void ffmpeg_setup_logs(u32 log_class)
{
	u32 level = gf_log_tool_get_level(log_class);
	switch (level) {
	case GF_LOG_DEBUG:
		av_log_set_level(AV_LOG_DEBUG);
		break;
	case GF_LOG_INFO:
		av_log_set_level(AV_LOG_INFO);
		break;
	case GF_LOG_WARNING:
		av_log_set_level(AV_LOG_WARNING);
		break;
	default:
		av_log_set_level(AV_LOG_ERROR);
		break;
	}
//	av_log_set_callback(ff_log_callback);
}

void ffmpeg_initialize()
{
	if (ffmpeg_init) return;
	av_register_all();
	avformat_network_init();
	ffmpeg_init = GF_TRUE;
}

void ffmpeg_registry_free(GF_FilterSession *session, GF_FilterRegister *reg, u32 nb_skip_begin)
{
	u32 i;
	GF_List *all_filters = reg->udta;
	if (all_filters) {
		while (gf_list_count(all_filters)) {
			i=0;
			GF_FilterRegister *f = gf_list_pop_back(all_filters);
			if (f->caps)
				gf_free((void *)f->caps);

			gf_free((char*) f->name);

			while (f->args) {
				GF_FilterArgs *arg = (GF_FilterArgs *) &f->args[i];
				if (!arg || !arg->arg_name) break;
				i++;
				if (arg->arg_default_val) gf_free((void *) arg->arg_default_val);
				if (arg->min_max_enum) gf_free((void *) arg->min_max_enum);
			}
			gf_free((void *) f->args);
			gf_fs_remove_filter_registry(session, f);
			gf_free(f);
		}
		gf_list_del(all_filters);
	}
	i=nb_skip_begin;
	while (reg->args) {
		GF_FilterArgs *arg = (GF_FilterArgs *) &reg->args[i];
		if (!arg || !arg->arg_name) break;
		i++;
		if (arg->arg_default_val) gf_free((void *) arg->arg_default_val);
		if (arg->min_max_enum) gf_free((void *) arg->min_max_enum);
	}
	if (reg->args) gf_free((void *) reg->args);

}

GF_FilterArgs ffmpeg_arg_translate(const struct AVOption *opt)
{
	char szDef[200];
	GF_FilterArgs arg;
	memset(&arg, 0, sizeof(GF_FilterArgs));
	arg.arg_name = opt->name;
	arg.arg_desc = opt->help;
	arg.offset_in_private=-1;
	arg.flags = GF_FS_ARG_META;
	if (opt->name[0] == 0)
		arg.flags = GF_FS_ARG_META;

	switch (opt->type) {
	case AV_OPT_TYPE_INT64:
	case AV_OPT_TYPE_INT:
		arg.arg_type = (opt->type==AV_OPT_TYPE_INT64) ? GF_PROP_LSINT : GF_PROP_SINT;
		sprintf(szDef, LLD, opt->default_val.i64);
		arg.arg_default_val = gf_strdup(szDef);
		sprintf(szDef, LLD"-"LLD, (s64) opt->min, (s64) opt->max);
		arg.min_max_enum = gf_strdup(szDef);
		break;
#if LIBAVCODEC_VERSION_MAJOR >= 58
	case AV_OPT_TYPE_UINT64:
//	case AV_OPT_TYPE_UINT:
		arg.arg_type = GF_PROP_LUINT;
		sprintf(szDef, LLU, opt->default_val.i64);
		arg.arg_default_val = gf_strdup(szDef);
		sprintf(szDef, LLU"-"LLU, (u64) opt->min, (u64) opt->max);
		arg.min_max_enum = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_BOOL:
		arg.arg_type = GF_PROP_BOOL;
		arg.arg_default_val = gf_strdup(opt->default_val.i64 ? "true" : "false");
		break;
#endif
	case AV_OPT_TYPE_FLOAT:
		arg.arg_type = GF_PROP_FLOAT;
		sprintf(szDef, "%g", opt->default_val.dbl);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_DOUBLE:
		arg.arg_type = GF_PROP_DOUBLE;
		sprintf(szDef, "%g", opt->default_val.dbl);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_VIDEO_RATE:
	case AV_OPT_TYPE_RATIONAL:
		arg.arg_type = GF_PROP_FRACTION;
		sprintf(szDef, "%d/%d", opt->default_val.q.num, opt->default_val.q.den);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_STRING:
		arg.arg_type = GF_PROP_STRING;
		if (opt->default_val.str)
			arg.arg_default_val = gf_strdup(opt->default_val.str);
		break;
	case AV_OPT_TYPE_IMAGE_SIZE:
		arg.arg_type = GF_PROP_STRING;
		break;
	case AV_OPT_TYPE_CONST:
		arg.arg_type = GF_PROP_BOOL;
		arg.arg_default_val = gf_strdup("false");
		break;
	case AV_OPT_TYPE_FLAGS:
		arg.arg_type = GF_PROP_UINT;
		sprintf(szDef, ""LLD, opt->default_val.i64);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_BINARY:
	case AV_OPT_TYPE_DURATION:
		arg.arg_type = GF_PROP_UINT;
		break;
	case AV_OPT_TYPE_SAMPLE_FMT:
		arg.arg_type = GF_PROP_UINT;
		arg.arg_default_val = gf_strdup("s16");
		arg.min_max_enum = gf_strdup("u8|s16|s32|flt|dbl|u8p|s16p|s32p|fltp|dblp");
		break;
	case AV_OPT_TYPE_PIXEL_FMT:
		arg.arg_type = GF_PROP_UINT;
		arg.arg_default_val = gf_strdup(av_get_pix_fmt_name(AV_PIX_FMT_YUV420P) );
		{
		u32 i, all_len=1, def_size=1000;
		char *enum_val = gf_malloc(sizeof(char)*def_size);
		enum_val[0] = 0;
		for (i=0; i<AV_PIX_FMT_NB; i++) {
			u32 len;
			const char *n = av_get_pix_fmt_name(i);
			if (!n) continue;

			len = (u32) strlen(n)+ (i ? 2 : 1);
			if (len+all_len>def_size) {
				def_size+=1000;
				enum_val = gf_realloc(enum_val, sizeof(char)*def_size);
			}
			if (i) strcat(enum_val, "|");
			strcat(enum_val, n);
			all_len+=len-1;
		}
		arg.min_max_enum = enum_val;
		}
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFMPEG] Unhandled option type %d\n", opt->type));
	}
	return arg;
}

static u32 ff_streamtype(u32 st)
{
	switch (st) {
	case AVMEDIA_TYPE_AUDIO: return GF_STREAM_AUDIO;
	case AVMEDIA_TYPE_VIDEO: return GF_STREAM_VISUAL;
	case AVMEDIA_TYPE_DATA: return GF_STREAM_METADATA;
	case AVMEDIA_TYPE_SUBTITLE: return GF_STREAM_TEXT;
	}
	return GF_STREAM_UNKNOWN;
}

void ffmpeg_expand_registry(GF_FilterSession *session, GF_FilterRegister *orig_reg, u32 type)
{
	u32 i=0, idx=0, flags=0;
	const struct AVOption *opt;
	GF_List *all_filters = gf_list_new();
	AVInputFormat  *fmt = NULL;
	AVCodec *codec = NULL;

	const char *fname = "";
	if (type==FF_REG_TYPE_DEMUX) fname = "ffdmx";
	else if (type==FF_REG_TYPE_DECODE) {
		fname = "ffdec";
		flags=AV_OPT_FLAG_DECODING_PARAM;
	}
	else if (type==FF_REG_TYPE_DEV_IN) {
		fname = "ffavin";
		avdevice_register_all();
		flags=AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_AUDIO_PARAM|AV_OPT_FLAG_DECODING_PARAM;
	}
	else if (type==FF_REG_TYPE_ENCODE) {
		fname = "ffenc";
		flags=AV_OPT_FLAG_ENCODING_PARAM;
	}
	else return;


	orig_reg->udta = all_filters;

	while (1) {
		const AVClass *av_class=NULL;
		const char *subname = NULL;
		const char *description = NULL;
		char szDef[100];
		GF_FilterRegister *freg;
		if (type==FF_REG_TYPE_DEMUX) {
			fmt = av_iformat_next(fmt);
			if (!fmt) break;
			av_class = fmt->priv_class;
			subname = fmt->name;
			description = fmt->long_name;
		} else if (type==FF_REG_TYPE_DECODE) {
			codec = av_codec_next(codec);
			if (!codec) break;
			av_class = codec->priv_class;
			subname = codec->name;
			description = codec->long_name;
		} else if (type==FF_REG_TYPE_DEV_IN) {
#if LIBAVCODEC_VERSION_MAJOR >= 58
			fmt = av_input_video_device_next(fmt);
			if (!fmt) break;
			av_class = fmt->priv_class;
			subname = fmt->name;
			description = fmt->long_name;
    		if (!av_class || (av_class->category!=AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) ) continue;
#else
			fmt = av_iformat_next(fmt);
			if (!fmt) break;
			av_class = fmt->priv_class;
			subname = fmt->name;
			description = fmt->long_name;
			//brute force ...
			if (!strcmp(subname, "dshow")) {}
			else if (!strcmp(subname, "avfoundation")) {}
			else if (!strcmp(subname, "video4linux2")) {}
			else if (!strcmp(subname, "x11grab")) {}
			else if (!strcmp(subname, "alsa")) {}
			else if (!strcmp(subname, "decklink")) {}
			else if (!strcmp(subname, "kmsgrab")) {}
			else if (!strcmp(subname, "libndi_newtek")) {}
			else if (!strcmp(subname, "fbdev")) {}
			else if (!strcmp(subname, "gdigrab")) {}
			else if (!strcmp(subname, "iec61883")) {}
			else if (!strcmp(subname, "lavfi")) {}
			else if (!strcmp(subname, "libcdio")) {}
			else if (!strcmp(subname, "lavfi")) {}
			else if (!strcmp(subname, "openal")) {}
			else if (!strcmp(subname, "openal")) {}
			else if (!strcmp(subname, "oss")) {}
			else if (!strcmp(subname, "pulse")) {}
			else if (!strcmp(subname, "sndio")) {}
			else if (!stricmp(subname, "vfw")) {}
			else continue;
#endif
		} else if (type==FF_REG_TYPE_ENCODE) {
			codec = av_codec_next(codec);
			if (!codec) break;
			av_class = codec->priv_class;
			subname = codec->name;
			description = codec->long_name;
		} else {
			break;
		}

		GF_SAFEALLOC(freg, GF_FilterRegister);
		if (!freg) continue;
		memcpy(freg, orig_reg, sizeof(GF_FilterRegister));
		freg->args = NULL;
		freg->caps = NULL;
		freg->nb_caps = 0;

		gf_list_add(all_filters, freg);

		sprintf(szDef, "%s:%s", fname, subname);
		freg->name = gf_strdup(szDef);
		freg->description = description;

		 if ((type==FF_REG_TYPE_ENCODE) || (type==FF_REG_TYPE_DECODE)) {
		 	GF_FilterCapability *caps;
		 	u32 cid = ffmpeg_codecid_to_gpac(codec->id);
		 	freg->nb_caps = 3;

		 	caps = gf_malloc(sizeof(GF_FilterCapability)*3);
		 	caps[0].code = GF_PROP_PID_STREAM_TYPE;
		 	caps[0].val.type = GF_PROP_UINT;
		 	caps[0].val.value.uint = ff_streamtype(codec->type);
		 	caps[0].flags = GF_CAPS_INPUT_OUTPUT;

		 	caps[1].code = GF_PROP_PID_CODECID;
		 	caps[1].val.type = GF_PROP_UINT;
		 	caps[1].val.value.uint = (type==FF_REG_TYPE_DECODE) ? cid : GF_CODECID_RAW;
		 	caps[1].flags = GF_CAPS_INPUT;

		 	caps[2].code = GF_PROP_PID_CODECID;
		 	caps[2].val.type = GF_PROP_UINT;
		 	caps[2].val.value.uint = (type==FF_REG_TYPE_DECODE) ? GF_CODECID_RAW : cid;
		 	caps[2].flags = GF_CAPS_OUTPUT;

		 	freg->caps =  caps;
		}
		else if ((type==FF_REG_TYPE_DEMUX)
#if LIBAVCODEC_VERSION_MAJOR >= 58
			&& (fmt->mime_type || fmt->extensions)
#else
			&& fmt->extensions
#endif
			){
		 	GF_FilterCapability *caps;
#if LIBAVCODEC_VERSION_MAJOR >= 58
			freg->nb_caps = (fmt->mime_type && fmt->extensions) ? 4 : 2;
#else
			freg->nb_caps = 2;
#endif
		 	caps = gf_malloc(sizeof(GF_FilterCapability)*freg->nb_caps);
		 	memset(caps, 0, sizeof(GF_FilterCapability)*freg->nb_caps);
		 	caps[0].code = GF_PROP_PID_STREAM_TYPE;
		 	caps[0].val.type = GF_PROP_UINT;
		 	caps[0].val.value.uint = GF_STREAM_FILE;
		 	caps[0].flags = GF_CAPS_INPUT_STATIC;

		 	caps[1].code = fmt->extensions ? GF_PROP_PID_FILE_EXT : GF_PROP_PID_MIME;
		 	caps[1].val.type = GF_PROP_NAME;
#if LIBAVCODEC_VERSION_MAJOR >= 58
			caps[1].val.value.string = (char *) ( fmt->extensions ? fmt->extensions : fmt->mime_type );
#else
			caps[1].val.value.string = (char *) fmt->extensions;
#endif
			caps[1].flags = GF_CAPS_INPUT;

#if LIBAVCODEC_VERSION_MAJOR >= 58
			if (fmt->mime_type && fmt->extensions) {
				caps[2].flags = 0;
				caps[3].code = GF_PROP_PID_MIME;
				caps[3].val.type = GF_PROP_NAME;
				caps[3].val.value.string = (char *) fmt->mime_type;
				caps[3].flags = GF_CAPS_INPUT;
			}
#endif
			//TODO map codec IDs if known ?
		 	freg->caps =  caps;
		}

		idx=0;
		i=0;
		while (av_class) {
			opt = &av_class->option[idx];
			if (!opt || !opt->name) break;
			if (!flags || (opt->flags & flags) )
				i++;
			idx++;
		}
		if (i) {
			GF_FilterArgs *args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
			memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
			freg->args = args;

			i=0;
			idx=0;
			while (av_class) {
				opt = &av_class->option[idx];
				if (!opt || !opt->name) break;

				if (!flags || (opt->flags & flags) ) {
					args[i] = ffmpeg_arg_translate(opt);
					i++;
				}
				idx++;
			}
		}

		gf_fs_add_filter_registry(session, freg);
	}
}


void ffmpeg_set_enc_dec_flags(const AVDictionary *options, AVCodecContext *ctx)
{
	AVDictionaryEntry *de=NULL;

	ctx->flags = 0;
	ctx->flags2 = 0;
	while (1) {
		u32 idx=0;
		de = av_dict_get((AVDictionary *)options, "", (const AVDictionaryEntry *) de, AV_DICT_IGNORE_SUFFIX);
		if (!de) break;

		while (ctx->av_class->option) {
			const struct AVOption *opt = &ctx->av_class->option[idx];
			if (!opt || !opt->name) break;
			if (opt->name && !strcmp(opt->name, de->key) && (!stricmp(de->value, "true") || !stricmp(de->value, "yes") || !stricmp(de->value, "1") )) {
				if (opt->unit && !strcmp(opt->unit, "flags"))
					ctx->flags |= (int) opt->default_val.i64;
				else if (opt->unit && !strcmp(opt->unit, "flags2"))
					ctx->flags2 |= (int) opt->default_val.i64;
				break;
			}
			idx++;
		}
	}
}

void ffmpeg_set_mx_dmx_flags(const AVDictionary *options, AVFormatContext *ctx)
{
	AVDictionaryEntry *de=NULL;

	while (1) {
		u32 idx=0;
		de = av_dict_get((AVDictionary *)options, "", de, AV_DICT_IGNORE_SUFFIX);
		if (!de) break;

		while (ctx->av_class->option) {
			const struct AVOption *opt = &ctx->av_class->option[idx];
			if (!opt || !opt->name) break;
			if (opt->name && !strcmp(opt->name, de->key)) {
				if (opt->unit && !strcmp(opt->unit, "fflags"))
					ctx->flags |= (int) opt->default_val.i64;
				else if (opt->unit && !strcmp(opt->unit, "avioflags"))
					ctx->avio_flags |= (int) opt->default_val.i64;
				break;
			}
			idx++;
		}
	}
}


#endif
