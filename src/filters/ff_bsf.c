/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg avbitstreamfilter filter
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
#include <gpac/network.h>

#endif

#ifdef FFMPEG_HAS_BSF

typedef struct
{
	GF_PropStringList f;

	GF_FilterPid *ipid, *opid;
	//decode options
	AVDictionary *options;

	AVBSFContext *bsfc;

	Bool flush_done, inject_cfg;
	Bool passthrough;
	u32 codec_id;
	Bool gen_dsi;
} GF_FFBSFCtx;


static GF_Err ffbsf_init_bsf(GF_Filter *filter, u32 ff_codecid)
{
	int res;
	u32 i, nb_filters=0;
	AVBSFList *bs_list;

	const AVBitStreamFilter *bsf;
	GF_FFBSFCtx *ctx = (GF_FFBSFCtx *) gf_filter_get_udta(filter);

	if (!ctx->f.nb_items) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFBSF] Missing bitstream filter name\n"));
		return GF_BAD_PARAM;
	}

	bs_list = av_bsf_list_alloc();
	if (!bs_list) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFBSF] Failed to allocate bitstream filter list\n"));
		return GF_OUT_OF_MEM;
	}
	char *desc = NULL;
	for (i=0; i<ctx->f.nb_items; i++) {
		u32 k;
		char *f = ctx->f.vals[i];
		bsf = av_bsf_get_by_name(f);
		if (!bsf) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFBSF] Unknown bitstream filter %s\n", f));
			av_bsf_list_free(&bs_list);
			return GF_BAD_PARAM;
		}
		if (!ff_codecid) {
			//declare bsf name as not found option to track which BSFs are used
			gf_filter_report_meta_option(filter, f, GF_FALSE, "f");
			gf_dynstrcat(&desc, bsf->name, " ");
			continue;
		}

		k=0;
		while (bsf->codec_ids[k]!=AV_CODEC_ID_NONE) {
			if (bsf->codec_ids[k]==ff_codecid) break;
			k++;
		}
		if (bsf->codec_ids[k]==AV_CODEC_ID_NONE)
			continue;

		AVDictionary *options=NULL;
		av_dict_copy(&options, ctx->options, 0);
		res = av_bsf_list_append2(bs_list, f, &options);
		if (res) {
			if (options) av_dict_free(&options);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFBSF] Failed to allocate bitstream filter context %s: %s\n", f, av_err2str(res) ));
			av_bsf_list_free(&bs_list);
			return GF_BAD_PARAM;
		}
		ffmpeg_report_options(filter, options, ctx->options);
		nb_filters++;

		//declare bsf name as found option
		gf_filter_report_meta_option(filter, f, GF_TRUE, "f");

	}
	if (!ff_codecid) {
		av_bsf_list_free(&bs_list);

		gf_filter_meta_set_instances(filter, desc);
		gf_free(desc);
		return GF_OK;
	}

	if (!nb_filters) {
		AVDictionary *options=NULL;
		av_dict_copy(&options, ctx->options, 0);
		ffmpeg_report_options(filter, options, ctx->options);
		ctx->passthrough = GF_TRUE;
		av_bsf_list_free(&bs_list);
		return GF_OK;
	}

	//finalize list
	ctx->passthrough = GF_FALSE;
	res = av_bsf_list_finalize(&bs_list, &ctx->bsfc);
	av_bsf_list_free(&bs_list);
	if (res) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFBSF] Failed to finalize bitstream filter context: %s\n", av_err2str(res) ));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

static GF_Err ffbsf_initialize(GF_Filter *filter)
{
	//just probe the bsf names
	return ffbsf_init_bsf(filter, 0);
}


static GF_Err ffbsf_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_FFBSFCtx *ctx = (GF_FFBSFCtx *) gf_filter_get_udta(filter);
	u32 codec;
	GF_Err e;
	Bool check_reconf = GF_FALSE;
	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	ctx->ipid = pid;
	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_BAD_PARAM;

	u32 ff_ctag;
	codec = p->value.uint;
	u32 cid = ffmpeg_codecid_from_gpac(codec, &ff_ctag);
	if (cid == 0) return GF_FILTER_NOT_SUPPORTED;
	//reevaluate if change of codec id
	if (ctx->codec_id != codec) {
		ctx->codec_id = codec;
		if (ctx->bsfc) {
			av_bsf_free(&ctx->bsfc);
			ctx->bsfc = NULL;
		}
	}

retry:
	//we can instantiate now that we have a codec id
	if (!ctx->bsfc) {
		e = ffbsf_init_bsf(filter, cid);
		if (e) return GF_FILTER_NOT_SUPPORTED;
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_OUT_OF_MEM;
	}
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);

	if (ctx->passthrough) return GF_OK;

	ctx->bsfc->par_in->codec_id = cid;
	ctx->bsfc->par_in->codec_tag = ff_ctag;
	e = ffmpeg_codec_par_from_gpac(ctx->ipid, ctx->bsfc->par_in, 0);
	if (e) return e;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		ffmpeg_extradata_from_gpac(codec, p->value.data.ptr, p->value.data.size, &ctx->bsfc->par_in->extradata, &ctx->bsfc->par_in->extradata_size);
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) {
		ctx->bsfc->time_base_in.num = 1;
		ctx->bsfc->time_base_in.den = p->value.uint;
	}

	//init
	int ret = av_bsf_init(ctx->bsfc);
	if (ret<0) {
		av_bsf_free(&ctx->bsfc);
		ctx->bsfc = NULL;
		if (!check_reconf) {
			check_reconf = GF_TRUE;
			goto retry;
		}
		return GF_FILTER_NOT_SUPPORTED;
	}

	codec = ffmpeg_codecid_to_gpac(ctx->bsfc->par_out->codec_id);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(codec));

	ffmpeg_codec_par_to_gpac(ctx->bsfc->par_out, ctx->opid, 0);


	ctx->gen_dsi = GF_FALSE;

	//even if given size-prefixed format, bsfilters always return annexB, we force a reframer (as with ffenc output) ...
	switch (codec) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
	case GF_CODECID_VVC:
	case GF_CODECID_AV1:
		if (ctx->bsfc->par_out->extradata_size) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE));
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED_FULL_AU, &PROP_BOOL(GF_TRUE) );
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
			ctx->inject_cfg = GF_TRUE;
		}
		break;

	//for these, we will need to generate dsi from first frame - this avoids using reframers only for DSI extraction
	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_SPATIAL:
	case GF_CODECID_MPEG4_PART2:
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
	case GF_CODECID_VP10:
	case GF_CODECID_AC3:
	case GF_CODECID_EAC3:
	case GF_CODECID_TRUEHD:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
		ctx->gen_dsi = GF_TRUE;
		break;

	default:
		if (ctx->bsfc->par_out->extradata) {
			u8 *dsi;
			u32 size;
			ffmpeg_extradata_to_gpac(codec, ctx->bsfc->par_out->extradata, ctx->bsfc->par_out->extradata_size, &dsi, &size);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, size));
		}
		break;
	}
	return GF_OK;
}


static GF_Err ffbsf_process(GF_Filter *filter)
{
	GF_FFBSFCtx *ctx = (GF_FFBSFCtx *) gf_filter_get_udta(filter);

	if (!ctx->ipid) return GF_BAD_PARAM;
	while (1) {
		GF_FilterPacket *src = gf_filter_pid_get_packet(ctx->ipid);
		if (!src) {
			if (gf_filter_pid_is_eos(ctx->ipid)) {
				if (ctx->flush_done || ctx->passthrough) {
					gf_filter_pid_set_eos(ctx->opid);
					return GF_EOS;
				}
			} else {
				return GF_OK;
			}
		}
		if (ctx->passthrough) {
			gf_filter_pck_forward(src, ctx->opid);
			gf_filter_pid_drop_packet(ctx->ipid);
			continue;
		}
		if (!ctx->bsfc) return GF_BAD_PARAM;

		AVPacket *avpck = av_packet_alloc();
		if (src) {
			u32 size;
			const u8 *data = gf_filter_pck_get_data(src, &size);
			avpck->size = size;
			avpck->data = av_malloc(size+8);
			memcpy(avpck->data, data, size);
			avpck->dts = gf_filter_pck_get_dts(src);
			avpck->pts = gf_filter_pck_get_cts(src);
			avpck->duration = gf_filter_pck_get_duration(src);
			u32 sap = gf_filter_pck_get_sap(src);
			if (sap==GF_FILTER_SAP_1) avpck->flags = AV_PKT_FLAG_KEY;
		}
		av_bsf_send_packet(ctx->bsfc, avpck);

		int res;
		while (1) {
			avpck = av_packet_alloc();
			res = av_bsf_receive_packet(ctx->bsfc, avpck);
			if (res<0) {
				av_packet_unref(avpck);
				break;
			}
			u8 *output;
			u32 size = avpck->size;
			if (ctx->inject_cfg)
				size+=ctx->bsfc->par_out->extradata_size;
			GF_FilterPacket *dst = gf_filter_pck_new_alloc(ctx->opid, size, &output);
			if (ctx->inject_cfg) {
				memcpy(output, ctx->bsfc->par_out->extradata, ctx->bsfc->par_out->extradata_size);
				output += ctx->bsfc->par_out->extradata_size;
				ctx->inject_cfg = GF_FALSE;
			}
			memcpy(output, avpck->data, avpck->size);

			if (ctx->gen_dsi) {
				ffmpeg_generate_gpac_dsi(ctx->opid, ctx->codec_id, ctx->bsfc->par_out->color_primaries, ctx->bsfc->par_out->color_trc, ctx->bsfc->par_out->color_space, output, size);
				ctx->gen_dsi = GF_FALSE;
			}

			//set sap first, and copy over src props
			gf_filter_pck_set_sap(dst, (avpck->flags & AV_PKT_FLAG_KEY) ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
			if (src)
				gf_filter_pck_merge_properties(src, dst);

			gf_filter_pck_set_cts(dst, avpck->pts);
			gf_filter_pck_set_dts(dst, avpck->dts);
			gf_filter_pck_set_duration(dst, (u32) avpck->duration);

			gf_filter_pck_send(dst);

			av_packet_unref(avpck);
		}
		if (src)
			gf_filter_pid_drop_packet(ctx->ipid);

		if (res == AVERROR(EAGAIN)) continue;
		if (res == AVERROR_EOF) {
			if (!src) {
				ctx->flush_done = GF_TRUE;
				return GF_EOS;
			}
			break;
		}
		if (res<0) return GF_SERVICE_ERROR;
	}

	return GF_OK;
}



static void ffbsf_finalize(GF_Filter *filter)
{
	GF_FFBSFCtx *ctx = (GF_FFBSFCtx *) gf_filter_get_udta(filter);
	if (ctx->bsfc)
		av_bsf_free(&ctx->bsfc);
	return;
}

static GF_Err ffbsf_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	GF_FFBSFCtx *ctx = gf_filter_get_udta(filter);
	return ffmpeg_update_arg("FFBSF", ctx->bsfc, &ctx->options, arg_name, arg_val);
}

static Bool ffbsf_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FFBSFCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type== GF_FEVT_STOP) {
		if (ctx->bsfc) av_bsf_flush(ctx->bsfc);
	}
	return GF_FALSE;
}

//for now we only accept compressed inputs
static const GF_FilterCapability FFBSFCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT|GF_CAPFLAG_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT|GF_CAPFLAG_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister FFBSFRegister = {
	.name = "ffbsf",
	.version = LIBAVCODEC_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG BitStream filter")
	GF_FS_SET_HELP("This filter provides bitstream filters (BSF) for compressed audio and video formats.\n"
		"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details\n"
		"To list all supported bitstream filters for your GPAC build, use `gpac -h ffbsf:*`.\n"
		"\n"
		"Several BSF may be specified in [-f]() for different coding types. BSF not matching the coding type are silently ignored.\n"
		"When no BSF matches the input coding type, or when [-f]() is empty, the filter acts as a passthrough filter.\n"
		"\n"
		"Options are specified after the desired filters:\n"
		"- `ffbsf:f=h264_metadata:video_full_range_flag=0`\n"
		"- `ffbsf:f=h264_metadata,av1_metadata:video_full_range_flag=0:color_range=tv`\n"
		"\n"
		"Note: Using BSFs on some media types (e.g. avc, hevc) may trigger creation of a reframer filter (e.g. rfnalu)\n"
	)
	.flags =  GF_FS_REG_META | GF_FS_REG_EXPLICIT_ONLY,
	.private_size = sizeof(GF_FFBSFCtx),
	SETCAPS(FFBSFCaps),
	.initialize = ffbsf_initialize,
	.finalize = ffbsf_finalize,
	.configure_pid = ffbsf_configure_pid,
	.process = ffbsf_process,
	.update_arg = ffbsf_update_arg,
	.process_event = ffbsf_process_event
};

#define OFFS(_n)	#_n, offsetof(GF_FFBSFCtx, _n)

static const GF_FilterArgs FFBSFArgs[] =
{
	{ OFFS(f), "bitstream filters name - see filter help", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ "*", -1, "any possible options defined for AVBitstreamFilter and sub-classes. See `gpac -hx ffbsf` and `gpac -hx ffbsf:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

const int FFBSF_STATIC_ARGS = (sizeof (FFBSFArgs) / sizeof (GF_FilterArgs)) - 1;

const GF_FilterRegister *ffbsf_register(GF_FilterSession *session)
{
	return ffmpeg_build_register(session, &FFBSFRegister, FFBSFArgs, FFBSF_STATIC_ARGS, FF_REG_TYPE_BSF);
}

#else //FFMPEG_HAS_BSF

#include <gpac/filters.h>
const GF_FilterRegister *ffbsf_register(GF_FilterSession *session)
{
	return NULL;
}
#endif
