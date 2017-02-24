/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
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
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>


typedef struct
{
	//options
	const char *src;
	u32 buffer_size;


	//internal data
	Bool initialized;
	//input file
	AVFormatContext *ctx;
	//demux options
	AVDictionary *options;

	GF_FilterPid **pids;

} GF_FFDemuxCtx;

static void ffdmx_finalize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ffd = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	gf_free(ffd->pids);
	if (ffd->options) av_dict_free(&ffd->options);
	return;
}


static GF_Err ffdmx_process(GF_Filter *filter)
{
	GF_Err e;
	AVPacket pkt;
	char *data_dst;
	GF_FilterPacket *pck_dst;
	GF_FFDemuxCtx *ffd = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);

#if 0
		seek_to = (s64) (AV_TIME_BASE*seek_time);
		av_seek_frame(ffd->ctx, -1, seek_to, AVSEEK_FLAG_BACKWARD);
#endif

	pkt.stream_index = -1;
	/*EOF*/
	if (av_read_frame(ffd->ctx, &pkt) <0) return GF_EOS;

	assert(pkt.stream_index>=0);
	assert(pkt.stream_index<ffd->ctx->nb_streams);

	if (pkt.pts == AV_NOPTS_VALUE) {
		pkt.pts = pkt.dts;
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[FFDemux] No PTS for packet on stream %d\n", pkt.stream_index ));

	}

	if (! ffd->pids[pkt.stream_index] ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDemux] No PID defined for given stream %d\n", pkt.stream_index ));
		av_free_packet(&pkt);
	}

	//todo - check if we want to use shared memory here
	pck_dst = gf_filter_pck_new_alloc(ffd->pids[pkt.stream_index] , pkt.size, &data_dst);
	memcpy(data_dst, pkt.data, pkt.size);

	if (pkt.pts != AV_NOPTS_VALUE) {
		AVStream *stream = ffd->ctx->streams[pkt.stream_index];
		u64 ts = pkt.pts * stream->time_base.num;

		gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_CTS, PROP_LONGUINT(ts) );

		if (!pkt.dts) pkt.dts = pkt.pts;

		if (pkt.dts != AV_NOPTS_VALUE) {
			ts = pkt.dts * stream->time_base.num;
			gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_DTS, PROP_LONGUINT(ts) );
		}
	}
	if (pkt.flags & AV_PKT_FLAG_KEY) 
		gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_SAP, PROP_UINT(1) );

	if (pkt.flags & AV_PKT_FLAG_CORRUPT)
		gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_CORRUPTED, PROP_BOOL(GF_TRUE) );

	//TODO - check we are not blocking
	e = gf_filter_pck_send(pck_dst);

	av_free_packet(&pkt);

	return e;

}


static GF_Err ffdmx_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFDemuxCtx *ffd = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ffd->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ffd->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDemux] Failed to set option %s:%s\n", arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDemux] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg demuxers
	return GF_NOT_SUPPORTED;
}

enum {
	GF_FFMPEG_DECODER_CONFIG = GF_4CC('f','f','D','C'),
};


static GF_Err ffdmx_initialize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ffd = gf_filter_get_udta(filter);
	GF_Err e;
	s64 last_aud_pts;
	u32 i;
	u32 nb_a, nb_v;
	s32 res;
	GF_PropertyValue p;
	Bool is_local;
	const char *sOpt;
	char *ext;
	AVInputFormat *av_in = NULL;
	char szName[50];

//	av_log_set_level(AV_LOG_DEBUG);

	ffd->initialized = GF_TRUE;
	if (!ffd->src) return GF_SERVICE_ERROR;

	/*some extensions not supported by ffmpeg, overload input format*/
	ext = strrchr(ffd->src, '.');
	if (ext) {
		if (!stricmp(ext+1, "cmp")) av_in = av_find_input_format("m4v");
	}

	is_local = (strnicmp(ffd->src, "file://", 7) && strstr(ffd->src, "://")) ? GF_FALSE : GF_TRUE;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("[FFDemux] opening file %s - local %d - av_in %08x\n", ffd->src, is_local, av_in));

	if (!is_local) {
		//TODO
		return GF_NOT_SUPPORTED;
	}
	res = avformat_open_input(&ffd->ctx, ffd->src, av_in, ffd->options ? &ffd->options : NULL);


	switch (res) {
	case 0:
		e = GF_OK;
		break;
	case AVERROR_INVALIDDATA:
		e = GF_NON_COMPLIANT_BITSTREAM;
		break;
	case AVERROR_DEMUXER_NOT_FOUND:
		e = GF_URL_ERROR;
		break;
	default:
		e = GF_NOT_SUPPORTED;
		break;
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDemux] Fail to open %s - error %s\n", ffd->src, av_err2str(e) ));
		return e;
	}
	res = avformat_find_stream_info(ffd->ctx, ffd->options ? &ffd->options : NULL);

	if (res <0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFDemux] cannot locate streams - error %s\n", av_err2str(e)));
		e = GF_NOT_SUPPORTED;
		return e;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFDemux] file %s opened - %d streams\n", ffd->src, ffd->ctx->nb_streams));

	ffd->pids = gf_malloc(sizeof(GF_FilterPid *)*ffd->ctx->nb_streams);
	memset(ffd->pids, 0, sizeof(GF_FilterPid *)*ffd->ctx->nb_streams);

	nb_a = nb_v = 0;
	for (i = 0; i < ffd->ctx->nb_streams; i++) {
		GF_FilterPid *pid=NULL;
		Bool expose_ffdec=GF_FALSE;
		AVStream *stream = ffd->ctx->streams[i];
		AVCodecContext *codec = stream->codec;
		switch(codec->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO) );
			nb_a++;
			sprintf(szName, "audio%d", nb_a);
			gf_filter_pid_set_name(pid, szName);
			break;

		case AVMEDIA_TYPE_VIDEO:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL) );
			nb_v++;
			sprintf(szName, "video%d", nb_v);
			gf_filter_pid_set_name(pid, szName);
			break;
		default:
			break;
		}
		if (!pid) continue;
		ffd->pids[i] = pid;
		gf_filter_pid_set_udta(pid, stream);

		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, PROP_UINT(stream->id) );

		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, PROP_UINT(stream->time_base.den) );

		if (stream->sample_aspect_ratio.num && stream->sample_aspect_ratio.den)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAR, PROP_FRAC( stream->sample_aspect_ratio.num, stream->sample_aspect_ratio.den ) );

		if (stream->metadata) {
			AVDictionaryEntry *ent=NULL;
			while (1) {
				ent = av_dict_get(stream->metadata, "", ent, AV_DICT_IGNORE_SUFFIX);
				if (!ent) break;

				//we use the same syntax as ffmpeg here
				gf_filter_pid_set_property_str(pid, ent->key, PROP_STRING(ent->value) );
			}
		}

		switch (codec->codec_id) {
		case CODEC_ID_MP2:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_AUDIO_MPEG1) );
			break;
		case CODEC_ID_MP3:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_AUDIO_MPEG2_PART3) );
			break;
		case CODEC_ID_AAC:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_AUDIO_AAC_MPEG4) );
			expose_ffdec=GF_TRUE;
			break;
		case CODEC_ID_MPEG4:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_VIDEO_MPEG4_PART2) );
			expose_ffdec=GF_TRUE;
			break;
		case CODEC_ID_H264:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_VIDEO_AVC) );
			expose_ffdec=GF_TRUE;
			break;
		case CODEC_ID_MPEG1VIDEO:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_VIDEO_MPEG1) );
			break;
		case CODEC_ID_MPEG2VIDEO:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_VIDEO_MPEG2_MAIN) );
			break;
		case CODEC_ID_H263:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_VIDEO_H263) );
			break;
		//todo - map all possible MPEG and common types to internal GPAC OTI
		default:
			gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_MEDIA_FFMPEG) );
			expose_ffdec=GF_TRUE;
			break;
		}

		if (codec->extradata_size) {
			//expose as const data
			gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, PROP_CONST_DATA(codec->extradata, codec->extradata_size) );
		} else if (expose_ffdec) {
			gf_filter_pid_set_property(pid, GF_FFMPEG_DECODER_CONFIG, PROP_POINTER( (void*)codec ) );
		}

		if (codec->sample_rate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, PROP_UINT( codec->sample_rate ) );
		if (codec->frame_size)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLES_PER_FRAME, PROP_UINT( codec->frame_size ) );
		if (codec->channels)
			gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, PROP_UINT( codec->channels ) );

		if (codec->width)
			gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, PROP_UINT( codec->width ) );
		if (codec->height)
			gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, PROP_UINT( codec->height ) );
		if (codec->framerate.num && codec->framerate.den)
			gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, PROP_FRAC( codec->framerate.num, codec->framerate.den ) );

		if (codec->bit_rate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, PROP_UINT( codec->bit_rate ) );
	}


	return GF_OK;
}

#define OFFS(_n)	#_n, offsetof(GF_FFDemuxCtx, _n)


static const GF_FilterCapability FFDemuxOutputs[] =
{
	{"cus2", {.type=GF_PROP_UINT, .value.uint = 10 }, GF_FALSE},
	{ NULL }
};

GF_FilterRegister FFDemuxRegister = {
	.name = "ffdmx",
	.description = "FFMPEG demuxer "LIBAVFORMAT_IDENT,
	.private_size = sizeof(GF_FFDemuxCtx),
	.output_caps = FFDemuxOutputs,
	.args = NULL,
	.initialize = ffdmx_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.configure_pid = NULL,
	.update_arg = ffdmx_update_arg
};

void ffdmx_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	u32 i;
	GF_List *all_filters = reg->udta;
	if (all_filters) {
		while (gf_list_count(all_filters)) {
			i=0;
			GF_FilterRegister *f = gf_list_pop_back(all_filters);
			gf_free(f->name);

			while (f->args) {
				GF_FilterArgs *arg = &f->args[i];
				if (!arg || !arg->arg_name) break;
				i++;
				if (arg->arg_default_val) gf_free(arg->arg_default_val);
				if (arg->min_max_enum) gf_free(arg->min_max_enum);
			}
			gf_free(f->args);
			gf_fs_remove_filter_registry(session, f);
			gf_free(f);
		}
		gf_list_del(all_filters);
	}
	i=1;
	while (reg->args) {
		GF_FilterArgs *arg = &reg->args[i];
		if (!arg || !arg->arg_name) break;
		i++;
		if (arg->arg_default_val) gf_free(arg->arg_default_val);
		if (arg->min_max_enum) gf_free(arg->min_max_enum);
	}
	if (reg->args) gf_free(reg->args);

}


GF_FilterArgs ff_arg_translate(const struct AVOption *opt)
{
	char szDef[100];
	GF_FilterArgs arg;
	memset(&arg, 0, sizeof(GF_FilterArgs));
	arg.arg_name = opt->name;
	arg.arg_desc = opt->help;
	arg.offset_in_private=-1;
	arg.meta_arg = GF_TRUE;

	switch (opt->type) {
	case AV_OPT_TYPE_INT64:
	case AV_OPT_TYPE_INT:
		arg.arg_type = (opt->type==AV_OPT_TYPE_INT64) ? GF_PROP_LONGSINT : GF_PROP_SINT;
		sprintf(szDef, ""LLD, opt->default_val.i64);
		arg.arg_default_val = gf_strdup(szDef);
		sprintf(szDef, "%d-%d", (s32) opt->min, (s32) opt->max);
		arg.min_max_enum = gf_strdup(szDef);
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
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDemux] Unhandled option type %d\n", opt->type));
	}
	return arg;
}

static const GF_FilterArgs FFDemuxArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ "*", -1, "Any possible args defined for AVFormatContext and sub-classes", GF_PROP_UINT, "1000", NULL, GF_FALSE, GF_TRUE},
	{ NULL }
};

const GF_FilterRegister *ffdmx_register(GF_FilterSession *session, Bool load_meta_filters)
{
	GF_FilterArgs *args;
	u32 i=0;
	const struct AVOption *opt;
	AVFormatContext *ctx;

	av_register_all();

	if (!load_meta_filters) {
		FFDemuxRegister.args = FFDemuxArgs;
		return &FFDemuxRegister;
	}

	FFDemuxRegister.registry_free = ffdmx_regfree;
	ctx = avformat_alloc_context();

	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[i];
		if (!opt || !opt->name) break;
		i++;
	}
	i+=2;

	args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
	FFDemuxRegister.args = args;
	args[0] = (GF_FilterArgs){ OFFS(src), "Source URL", GF_PROP_STRING, NULL, NULL, GF_FALSE} ;
	i=0;
	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[i];
		if (!opt || !opt->name) break;
		args[i+1] = ff_arg_translate(opt);
		i++;
	}
	args[i+1] = (GF_FilterArgs) { "*", -1, "Options depend on input type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FALSE};


	if (load_meta_filters) {
		GF_List *all_filters = gf_list_new();
		AVInputFormat  *fmt = NULL;

		FFDemuxRegister.registry_free = ffdmx_regfree;
		FFDemuxRegister.udta = all_filters;

		while (1) {
			char szDef[100];
			GF_FilterRegister *freg;
			i=0;
			fmt = av_iformat_next(fmt);
			if (!fmt) break;

			GF_SAFEALLOC(freg, GF_FilterRegister);
			if (!freg) continue;
			gf_list_add(all_filters, freg);


			sprintf(szDef, "ffdmx:%s", fmt->name);
			freg->name = gf_strdup(szDef);
			freg->description = fmt->long_name;

			freg->output_caps = FFDemuxOutputs;

			freg->initialize = ffdmx_initialize;
			freg->finalize = ffdmx_finalize;
			freg->process = ffdmx_process;
			freg->configure_pid = NULL;
			freg->update_arg = ffdmx_update_arg;


			while (fmt->priv_class) {
				opt = &fmt->priv_class->option[i];
				if (!opt || !opt->name) break;
				i++;
			}
			if (i) {
				GF_FilterArgs *args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
				memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
				freg->args = args;

				i=0;
				while (fmt->priv_class) {
					opt = &fmt->priv_class->option[i];
					if (!opt || !opt->name) break;

					args[i] = ff_arg_translate(opt);
					i++;
				}
			}
			gf_fs_add_filter_registry(session, freg);
		}
	}

	return &FFDemuxRegister;
}



