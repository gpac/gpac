/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg AVDevice filter
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

#include <gpac/filters.h>
#include <gpac/list.h>
#include <gpac/constants.h>

#include "ff_common.h"


typedef struct
{
	//options
	const char *src;
	const char *fmt, *dev;
	u32 buffer_size;


	//internal data
	Bool initialized;
	//input file
	AVFormatContext *ctx;
	//demux options
	AVDictionary *options;

	GF_FilterPid **pids;

	u32 nb_playing;
} GF_FFAVInCtx;

static void ffavin_finalize(GF_Filter *filter)
{
	GF_FFAVInCtx *ctx = (GF_FFAVInCtx *) gf_filter_get_udta(filter);
	gf_free(ctx->pids);
	if (ctx->options) av_dict_free(&ctx->options);
	return;
}


static GF_Err ffavin_process(GF_Filter *filter)
{
	return GF_OK;
}


static GF_Err ffavin_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFAVInCtx *ctx = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ctx->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ctx->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFAVIn] Failed to set option %s:%s\n", arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFAVIn] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg demuxers
	return GF_NOT_SUPPORTED;
}

static GF_Err ffavin_initialize(GF_Filter *filter)
{
	s32 res, vstream_idx, i;
	AVInputFormat *dev=NULL;
	GF_FFAVInCtx *ctx = gf_filter_get_udta(filter);
	AVFormatContext *fmt_ctx=NULL;

//	av_log_set_level(AV_LOG_DEBUG);

	avdevice_register_all();
	ctx->initialized = GF_TRUE;
	if (!ctx->src) return GF_SERVICE_ERROR;

	dev = NULL;
	if (ctx->fmt) {
		dev = av_find_input_format("avfoundation");
		if (dev == NULL) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Cannot find the input format %s\n", ctx->fmt));
		} else if (dev->priv_class->category!=AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Format %s is not a video input device\n", ctx->fmt));
			dev = NULL;
		}
	}
	while (!dev) {
		dev = av_input_video_device_next(dev);
		if (!dev) break;
    	if (!dev || !dev->priv_class  || (dev->priv_class->category!=AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) )
        	continue;

		//listing devices is on its way of implementation in ffmpeg ...
/*
    	if (dev->get_device_list) {

    	} else {
		}
*/
		break;
	}
	if (!dev) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Cannot find the format\n"));
	}

#if defined(__APPLE__) && !defined(GPAC_IPHONE)
#endif

	fmt_ctx = NULL;

	/* Open video */
	res = avformat_open_input(&fmt_ctx, ctx->dev, dev, &ctx->options);
	if ( (res < 0) && !stricmp(ctx->src+8, "screen-capture-recorder") ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Buggy screen capture input (open failed with code %d), retrying without specifying resolution\n", res));
		av_dict_set(&ctx->options, "video_size", NULL, 0);
		res = avformat_open_input(&fmt_ctx, ctx->src+8, dev, &ctx->options);
	}

	if ( (res < 0) && ctx->options) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Error %d opening input - retrying without options\n", res));

		res = avformat_open_input(&fmt_ctx, ctx->src+8, dev, NULL);
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Cannot open file %s\n", ctx->src+8));
		return -1;
	}

	/* Retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Cannot find stream information\n"));
		return -1;
	}

	av_dump_format(fmt_ctx, 0, ctx->src+8, 0);

	/* Find the first video stream */
	vstream_idx = -1;
	for (i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			vstream_idx = i;
			break;
		}
	}
	if (vstream_idx == -1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("Cannot find a video stream\n"));
		return -1;
	}


	return GF_OK;
}

static Bool ffavin_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	GF_FFAVInCtx *ctx = gf_filter_get_udta(filter);

	switch (com->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->nb_playing) {
			int res = av_seek_frame(ctx->ctx, -1, (AV_TIME_BASE*com->play.start_range), AVSEEK_FLAG_BACKWARD);
			if (res<0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FFAVIn] Fail to seek %s to %g - error %s\n", ctx->src, com->play.start_range, av_err2str(res) ));
			}
		}
		ctx->nb_playing++;

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (ctx->nb_playing) ctx->nb_playing--;
		//cancel event
		return GF_TRUE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static GF_FilterProbeScore ffavin_probe_url(const char *url, const char *mime)
{
	if (!strncmp(url, "video://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "audio://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

#define OFFS(_n)	#_n, offsetof(GF_FFAVInCtx, _n)


GF_FilterRegister FFAVInRegister = {
	.name = "ffavin",
	.description = "FFMPEG AVDevice "LIBAVDEVICE_IDENT,
	.private_size = sizeof(GF_FFAVInCtx),
	.initialize = ffavin_initialize,
	.finalize = ffavin_finalize,
	.process = ffavin_process,
	.update_arg = ffavin_update_arg,
	.probe_url = ffavin_probe_url,
	.process_event = ffavin_process_event
};


void ffavin_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_registry_free(session, reg, 3);
}

static const GF_FilterArgs FFAVInArgs[] =
{
	{ OFFS(src), "url of device, video:// or audio://", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(fmt), "name of device class. If not set, defaults to first device class", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(dev), "name of device or index of device", GF_PROP_STRING, "0", NULL, GF_FALSE},
	{ "*", -1, "Any possible args defined for AVFormatContext and sub-classes", GF_PROP_UINT, NULL, NULL, GF_FALSE, GF_TRUE},
	{}
};

const GF_FilterRegister *ffavin_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	ffmpeg_initialize();

	if (!load_meta_filters) {
		FFAVInRegister.args = FFAVInArgs;
		return &FFAVInRegister;
	}

	FFAVInRegister.registry_free = ffavin_regfree;
	args = gf_malloc(sizeof(GF_FilterArgs)*(5));
	memset(args, 0, sizeof(GF_FilterArgs)*(5));
	FFAVInRegister.args = args;
	args[0] = (GF_FilterArgs){ OFFS(src), "url of device", GF_PROP_STRING, NULL, NULL, GF_FALSE} ;
	args[1] = (GF_FilterArgs){ OFFS(fmt), "name of device class. If not set, defaults to first device class", GF_PROP_STRING, NULL, NULL, GF_FALSE} ;
	args[2] = (GF_FilterArgs){ OFFS(dev), "name of device or index of device", GF_PROP_STRING, "0", NULL, GF_FALSE} ;
	args[3] = (GF_FilterArgs) { "*", -1, "Options depend on input type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FALSE};

	ffmpeg_expand_registry(session, &FFAVInRegister, 2);

	return &FFAVInRegister;
}



