/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2019-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg avfilter filter
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

#if defined(GPAC_HAS_FFMPEG) && !defined(FFMPEG_DISABLE_AVFILTER)

#include "ff_common.h"
#include <gpac/network.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>

#if (LIBAVFILTER_VERSION_MAJOR < 7)
#undef GPAC_HAS_FFMPEG
#endif

#endif

#if defined(GPAC_HAS_FFMPEG) && !defined(FFMPEG_DISABLE_AVFILTER)

typedef struct
{
	AVFilterContext *io_filter_ctx;
	GF_FilterPid *io_pid;
	u32 timescale, width, height, sr, nb_ch, bps, bpp;
	Bool planar;
	u32 pfmt; //ffmpeg pixel or audio format
	u64 ch_layout; //ffmpeg channel layout
	GF_Fraction sar;
	u32 stride, stride_uv, nb_planes;
	//output only
	Bool is_video;
	u32 gf_pfmt, out_size, uv_height, uv_width, tb_num;
} GF_FFAVPid;


typedef struct
{
	//options
	u32 pfmt, afmt, sr, ch;
	Bool dump;

	//internal

	GF_List *ipids;
	GF_List *opids;

	AVFilterGraph *filter_graph;
	char *filter_desc;

	GF_FilterCapability filter_caps[7];
	//0: not loaded, 1: graph config requested but graph not loaded, 2: graph loaded
	u32 configure_state;
	u32 nb_v_out, nb_a_out, nb_inputs;

	AVFilterInOut *outputs;
	AVFrame *frame;

	//0: no flush, 1: graph flush (push EOS in input), 2: wait for EOS in output
	u32 flush_state;
	GF_Err in_error;

	u32 nb_playing;
	Bool done;
} GF_FFAVFilterCtx;


static void ffavf_reset_graph(GF_FFAVFilterCtx *ctx)
{
	if (ctx->outputs) {
		avfilter_inout_free(&ctx->outputs);
		ctx->outputs = NULL;
	}
	if (ctx->filter_graph) {
		avfilter_graph_free(&ctx->filter_graph);
		ctx->filter_graph = NULL;
	}
}


static GF_Err ffavf_setup_input(GF_FFAVFilterCtx *ctx, GF_FFAVPid *avpid)
{
	int ret;
	char args[1024];
	const AVFilter *avf = NULL;
	const char *pid_name = gf_filter_pid_get_name(avpid->io_pid);

	if (avpid->width) {
		avf = avfilter_get_by_name("buffer");
		snprintf(args, sizeof(args),
				"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
				avpid->width, avpid->height, avpid->pfmt, 1, avpid->timescale, avpid->sar.num, avpid->sar.den);
	} else {
		avf = avfilter_get_by_name("abuffer");
		snprintf(args, sizeof(args),
			   "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x"LLU,
			   1, avpid->timescale, avpid->sr, av_get_sample_fmt_name(avpid->pfmt), avpid->ch_layout);
	}
	//destroy filter (will remove from graph)
	if (avpid->io_filter_ctx) avfilter_free(avpid->io_filter_ctx);
	avpid->io_filter_ctx = NULL;
	ret = avfilter_graph_create_filter(&avpid->io_filter_ctx, avf, pid_name, args, NULL, ctx->filter_graph);
	if (ret<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to create filter graph: %s\n", av_err2str(ret) ));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

static GF_Err ffavf_setup_outputs(GF_Filter *filter, GF_FFAVFilterCtx *ctx)
{
	AVFilterInOut *io;
	u32 i, nb_outputs;
	int ret;

	//outputs are configured
	if (ctx->outputs) return GF_OK;

	//allocate output pids
	ctx->outputs = avfilter_inout_alloc();
	io = ctx->outputs;
	nb_outputs = ctx->nb_v_out + ctx->nb_a_out;
	for (i=0; i<nb_outputs; i++) {
		u32 k;
		char szName[20];
		const AVFilter *avf = NULL;
		GF_FFAVPid *opid = NULL;
		Bool is_video = i<ctx->nb_v_out ? GF_TRUE : GF_FALSE;

		for (k=0; k<gf_list_count(ctx->opids); k++) {
			opid = gf_list_get(ctx->opids, k);
			if (opid->is_video && is_video) break;
			if (!opid->is_video && !is_video) break;
			opid = NULL;
		}
		if (!opid) {
			GF_SAFEALLOC(opid, GF_FFAVPid);
			if (!opid) continue;

			gf_list_add(ctx->opids, opid);
			opid->io_pid = gf_filter_pid_new(filter);
			opid->is_video = i<ctx->nb_v_out ? GF_TRUE : GF_FALSE;
			//remove properties since we may have change format
			if (is_video) {
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_SAMPLE_RATE, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_CHANNEL_LAYOUT, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_NUM_CHANNELS, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_AUDIO_BPS, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_AUDIO_FORMAT, NULL);

				//until configured do not advertize width/height to avoid filters setup down the chain
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_WIDTH, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_HEIGHT, NULL);
			} else {
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_WIDTH, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_HEIGHT, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_PIXFMT, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_FPS, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STRIDE, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STRIDE_UV, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_SAR, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_COLR_MX, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_COLR_RANGE, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_COLR_TRANSFER, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_COLR_PRIMARIES, NULL);

				//until configured do not advertize SR/channels to avoid filters setup down the chain
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_SAMPLE_RATE, NULL);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_NUM_CHANNELS, NULL);
			}
			gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
			gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_FILE_EXT, NULL);
			gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_MIME, NULL);
		}
		opid->io_filter_ctx = NULL;

		if (opid->is_video) {
			sprintf(szName, "vout%d", i+1);
			avf = avfilter_get_by_name("buffersink");
		} else {
			sprintf(szName, "aout%d", (i-ctx->nb_v_out) + 1);
			avf = avfilter_get_by_name("abuffersink");
		}

		if (nb_outputs==1)
			sprintf(szName, "out");

		ret = avfilter_graph_create_filter(&opid->io_filter_ctx, avf, szName, NULL, NULL, ctx->filter_graph);
		if (ret<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to create %s filter: %s\n", avf->name, av_err2str(ret) ));
			return GF_BAD_PARAM;
		}
		if (opid->is_video) {
			if (ctx->pfmt) {
				enum AVPixelFormat pfmt = ffmpeg_pixfmt_from_gpac(ctx->pfmt, GF_FALSE);
				ret = av_opt_set_bin(opid->io_filter_ctx, "pix_fmts", (uint8_t*)&pfmt, sizeof(pfmt), AV_OPT_SEARCH_CHILDREN);
				if (ret < 0) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[FFAVF] Fail to set %s pixel format: %s\n", avf->name, av_err2str(ret) ));
				}
			}
		} else {
			if (ctx->afmt) {
				enum AVSampleFormat afmt = ffmpeg_audio_fmt_from_gpac(ctx->afmt);
				ret = av_opt_set_bin(opid->io_filter_ctx, "sample_fmts", (uint8_t*)&afmt, sizeof(afmt), AV_OPT_SEARCH_CHILDREN);
				if (ret < 0) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[FFAVF] Fail to set %s audio format: %s\n", avf->name, av_err2str(ret) ));
				}
			}
			if (ctx->sr) {
				ret = av_opt_set_bin(opid->io_filter_ctx, "sample_rates", (uint8_t*)&ctx->sr, sizeof(ctx->sr), AV_OPT_SEARCH_CHILDREN);
				if (ret < 0) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[FFAVF] Fail to set %s audio sample rate: %s\n", avf->name, av_err2str(ret) ));
				}
			}
			if (ctx->ch) {
				ret = av_opt_set_bin(opid->io_filter_ctx, "channels", (uint8_t*)&ctx->ch, sizeof(ctx->ch), AV_OPT_SEARCH_CHILDREN);
				if (ret < 0) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[FFAVF] Fail to set %s audio sample rate: %s\n", avf->name, av_err2str(ret) ));
				}
			}

		}
		io->name = av_strdup(szName);
		io->filter_ctx = opid->io_filter_ctx;
		io->pad_idx = 0;
		io->next = NULL;
		if (i+1==nb_outputs) break;
		io->next = avfilter_inout_alloc();
		io = io->next;
	}
	return GF_OK;
}


static GF_Err ffavf_reconfigure_graph(GF_Filter *filter, GF_FFAVFilterCtx *ctx)
{
	u32 i, count;
	GF_Err e = GF_OK;
	ctx->flush_state = 0;
	ffavf_reset_graph(ctx);
	ctx->filter_graph = avfilter_graph_alloc();

	count = gf_list_count(ctx->ipids);
	for (i=0; i<count; i++) {
		GF_FFAVPid *ipid = gf_list_get(ctx->ipids, i);
		e = ffavf_setup_input(ctx, ipid);
		if (e) break;
	}
	ctx->configure_state = 2;
	if (!e)
		e = ffavf_setup_outputs(filter, ctx);

	if (e) ctx->in_error = e;
	return e;
}

static GF_Err ffavf_initialize(GF_Filter *filter)
{
	u32 nb_v_in=0, nb_a_in=0;
	u32 i;
	AVFilterInOut *inputs;
	AVFilterInOut *outputs;
	AVFilterInOut *io;
	int ret;
	Bool dyn_inputs = GF_FALSE;
	GF_FFAVFilterCtx *ctx = (GF_FFAVFilterCtx *) gf_filter_get_udta(filter);
	if (!ctx->filter_desc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Missing filter graph description, cannot load\n"));
		return GF_BAD_PARAM;
	}

	ctx->ipids = gf_list_new();
	ctx->opids = gf_list_new();
	ctx->frame = av_frame_alloc();

	ffmpeg_setup_logs(GF_LOG_MEDIA);

	ctx->filter_graph = avfilter_graph_alloc();
	ret = avfilter_graph_parse2(ctx->filter_graph, ctx->filter_desc, &inputs, &outputs);
	if (ret<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to parse filter description: %s\nFilter description was %s\n", av_err2str(ret), ctx->filter_desc));
		return GF_BAD_PARAM;
	}

	char *desc = NULL;
	for (i=0; i<ctx->filter_graph->nb_filters; i++) {
		AVFilterContext *avf = ctx->filter_graph->filters[i];
		if (desc) {
			char *sep = strstr(desc, avf->filter->name);
			if (sep) {
				u32 slen = (u32) strlen(avf->filter->name);
				if ((sep[slen]==',') || !sep[slen]) continue;
			}
		}
		gf_dynstrcat(&desc, avf->filter->name, " ");
	}
	gf_filter_meta_set_instances(filter, desc);
	gf_free(desc);

	ctx->nb_inputs=0;
	io = inputs;
	while (io) {
		if (io->filter_ctx->filter->flags & AVFILTER_FLAG_DYNAMIC_INPUTS)
			dyn_inputs = GF_TRUE;

		enum AVMediaType mt = avfilter_pad_get_type(io->filter_ctx->input_pads, io->pad_idx);
		u32 streamtype = ffmpeg_stream_type_to_gpac(mt);

		switch (streamtype) {
		case GF_STREAM_VISUAL: nb_v_in++; break;
		case GF_STREAM_AUDIO: nb_a_in++; break;
		}
		ctx->nb_inputs++;
		io = io->next;
	}

	ctx->nb_v_out = ctx->nb_a_out = 0;
	io = outputs;
	while (io) {
		for (i=0; i<io->filter_ctx->nb_outputs; i++) {
			enum AVMediaType mt = avfilter_pad_get_type(io->filter_ctx->output_pads, i);
			u32 streamtype = ffmpeg_stream_type_to_gpac(mt);
			switch (streamtype) {
			case GF_STREAM_VISUAL: ctx->nb_v_out++; break;
			case GF_STREAM_AUDIO: ctx->nb_a_out++; break;
			}
		}
		io = io->next;
	}
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);
	avfilter_graph_free(&ctx->filter_graph);
	ctx->filter_graph = avfilter_graph_alloc();

	if (dyn_inputs) {
		gf_filter_set_max_extra_input_pids(filter, -1);
	} else if (nb_v_in + nb_a_in > 1)
		gf_filter_set_max_extra_input_pids(filter, nb_v_in + nb_a_in - 1);

	/*update filter caps*/
	memset(ctx->filter_caps, 0, sizeof(GF_FilterCapability) * 7);
	ctx->filter_caps[0].flags = ctx->nb_inputs ? GF_CAPS_INPUT_OUTPUT : GF_CAPS_OUTPUT;
	ctx->filter_caps[0].code = GF_PROP_PID_CODECID;
	ctx->filter_caps[0].val = PROP_UINT(GF_CODECID_RAW);
	i=1;
	if (nb_v_in) {
		ctx->filter_caps[i].flags = ctx->nb_v_out ? GF_CAPS_INPUT_OUTPUT : GF_CAPS_INPUT;
		ctx->filter_caps[i].code = GF_PROP_PID_STREAM_TYPE;
		ctx->filter_caps[i].val = PROP_UINT(GF_STREAM_VISUAL);
		i++;
	}
	if (nb_a_in) {
		ctx->filter_caps[i].flags = ctx->nb_a_out ? GF_CAPS_INPUT_OUTPUT : GF_CAPS_INPUT;
		ctx->filter_caps[i].code = GF_PROP_PID_STREAM_TYPE;
		ctx->filter_caps[i].val = PROP_UINT(GF_STREAM_AUDIO);
		i++;
	}
	if (ctx->nb_v_out && !nb_v_in) {
		ctx->filter_caps[i].flags = GF_CAPS_OUTPUT;
		ctx->filter_caps[i].code = GF_PROP_PID_STREAM_TYPE;
		ctx->filter_caps[i].val = PROP_UINT(GF_STREAM_VISUAL);
		i++;
	}
	if (ctx->nb_a_out && !nb_a_in) {
		ctx->filter_caps[i].flags = GF_CAPS_OUTPUT;
		ctx->filter_caps[i].code = GF_PROP_PID_STREAM_TYPE;
		ctx->filter_caps[i].val = PROP_UINT(GF_STREAM_AUDIO);
		i++;
	}
	gf_filter_override_caps(filter, ctx->filter_caps, i);

	if (!ctx->nb_inputs) {
		ctx->configure_state = 1;
		gf_filter_post_process_task(filter);
		return ffavf_setup_outputs(filter, ctx);
	}
	return GF_OK;
}

static void ffavf_dump_graph(GF_FFAVFilterCtx *ctx, const char *opt)
{
	char *graphdump = avfilter_graph_dump(ctx->filter_graph, opt);

	if (graphdump) {
#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_MEDIA, GF_LOG_INFO)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[FFAVF] Graph dump:\n%s\n\n", graphdump ));
		} else
#endif
			fprintf(stderr, "[FFAVF] Graph dump:\n%s\n\n", graphdump);

		av_free(graphdump);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Failed to dump graph %s\n", ctx->filter_desc));
	}

}
static GF_Err ffavf_setup_filter(GF_Filter *filter, GF_FFAVFilterCtx *ctx)
{
	int ret;
	AVFilterInOut *io, *inputs;
	u32 i, count = gf_list_count(ctx->ipids);

	//wait until we have one packet on each input
	for (i=0; i<count; i++) {
		GF_FFAVPid *pid_ctx = gf_list_get(ctx->ipids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid_ctx->io_pid);
		if (!pck) return GF_OK;
	}
	ctx->configure_state = 2;

	/*create inputs*/
	inputs = avfilter_inout_alloc();
	io = inputs;
	for (i=0; i<count; i++) {
		char szName[20];
		GF_FFAVPid *pid_ctx = gf_list_get(ctx->ipids, i);

		if (count==1)
			io->name = av_strdup("in");
		else {
			const GF_PropertyValue *p = gf_filter_pid_get_property_str(pid_ctx->io_pid, "ffid");
			if (p && p->value.string) {
				io->name = av_strdup(p->value.string);
			} else {
				if (i)
					sprintf(szName, "in%d", i+1);
				else
					sprintf(szName, "in");
				io->name = av_strdup(szName);
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[FFAVF] Multiple input for graph but no names assigned to input PIDs (missing ffid property), using %s as default name. Filter linking might fail\n", szName));
			}
		}
		io->filter_ctx = pid_ctx->io_filter_ctx;

		if (i+1==count) break;
		io->next = avfilter_inout_alloc();
		io = io->next;
	}
	//our outputs describe the filter graph outputs and our inputs desscribe the filter graph input
	//however avfilter_graph_parse_ptr expects:
	// inputs: the inputs of the next filter graph to connect to, hence our outputs
	// outputs: the outputs of the previous filter graph to connect to, hence our inputs
	ret = avfilter_graph_parse_ptr(ctx->filter_graph, ctx->filter_desc,  &ctx->outputs, &inputs, NULL);
	avfilter_inout_free(&inputs);
	if (ret < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to parse filter description: %s\nFilter description was %s\n", av_err2str(ret), ctx->filter_desc));
        return ctx->in_error = GF_BAD_PARAM;
	}
	ret = avfilter_graph_config(ctx->filter_graph, NULL);
	if (ret < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to validate filter graph: %s\n", av_err2str(ret) ));
        return ctx->in_error = GF_BAD_PARAM;
	}

	if (ctx->dump)
		ffavf_dump_graph(ctx, NULL);
	return GF_OK;
}

static GF_Err ffavf_process(GF_Filter *filter)
{
	int ret;
	GF_Err e = GF_OK;
	u32 i, count, nb_eos;
	GF_FFAVFilterCtx *ctx = (GF_FFAVFilterCtx *) gf_filter_get_udta(filter);

	if (ctx->in_error)
		return ctx->in_error;
	if (ctx->done)
		return GF_EOS;

	//graph needs to be loaded
	if (ctx->configure_state==1) {
		if (gf_filter_connections_pending(filter))
			return GF_OK;
		if (ctx->nb_inputs > gf_list_count(ctx->ipids))
			return GF_OK;
		return ffavf_setup_filter(filter, ctx);
	}
	if (!ctx->nb_playing)
		return GF_OK;

	//push input
	nb_eos = 0;
	count = gf_list_count(ctx->ipids);
	if (ctx->flush_state==2)
		count = 0;

	for (i=0; i<count; i++) {
		const u8 *data;
		u32 data_size;
		Bool frame_ok = GF_FALSE;
		GF_FFAVPid *ipid = gf_list_get(ctx->ipids, i);
		GF_FilterPacket *pck;
		pck = gf_filter_pid_get_packet(ipid->io_pid);

		//config changed at this packet, start flushing the graph
		if (ctx->flush_state==1) {
			ret = av_buffersrc_add_frame_flags(ipid->io_filter_ctx, NULL, 0);
			if (ret<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to flush filter graph: %s\n", av_err2str(ret) ));
			}
			continue;
		}

		if (!pck) {
			if (gf_filter_pid_is_eos(ipid->io_pid)) {
				ret = av_buffersrc_add_frame_flags(ipid->io_filter_ctx, NULL, 0);
				if (ret<0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to signal EOS: %s\n", av_err2str(ret) ));
					e = GF_SERVICE_ERROR;
				}
				nb_eos++;
			}
			continue;
		}
		data = gf_filter_pck_get_data(pck, &data_size);
		if (data) {
			ctx->frame->data[0] = (uint8_t *) data;
			if (ipid->width) {
				ctx->frame->linesize[0] = ipid->stride;
				if (ipid->stride_uv) {
					ctx->frame->data[1] = ctx->frame->data[0] + ipid->height * ipid->stride;
					ctx->frame->linesize[1] = ipid->stride_uv;
					if (ipid->nb_planes==3) {
						ctx->frame->data[2] = ctx->frame->data[1] + ipid->uv_height * ipid->stride_uv;
						ctx->frame->linesize[2] = ipid->stride_uv;
					}
				}
			} else {
				ctx->frame->linesize[0] = data_size;
			}
			frame_ok = GF_TRUE;
		} else {
			u32 j;
			GF_FilterFrameInterface *fifce = gf_filter_pck_get_frame_interface(pck);
			if (fifce->get_plane) {
				frame_ok = GF_TRUE;
				for (j=0; j<ipid->nb_planes; j++) {
					e = fifce->get_plane(fifce, j, (const u8 **) &ctx->frame->data[j], &ctx->frame->linesize[j]);
					if (e) {
						frame_ok = GF_FALSE;
						break;
					}
				}
			}
		}

		if (frame_ok) {
			u64 cts = gf_filter_pck_get_cts(pck);
			ctx->frame->pts = cts;
			if (ipid->width) {
				ctx->frame->width = ipid->width;
				ctx->frame->height = ipid->height;
				ctx->frame->format = ipid->pfmt;
				ctx->frame->sample_aspect_ratio.num = ipid->sar.num;
				ctx->frame->sample_aspect_ratio.den = ipid->sar.den;
			} else {
#ifdef FFMPEG_OLD_CHLAYOUT
				ctx->frame->channel_layout = ipid->ch_layout;
				ctx->frame->channels = ipid->nb_ch;
#else
				ctx->frame->ch_layout.order = AV_CHANNEL_ORDER_NATIVE;
				ctx->frame->ch_layout.nb_channels = ipid->nb_ch;
				ctx->frame->ch_layout.u.mask = ipid->ch_layout;
#endif
				ctx->frame->sample_rate = ipid->sr;
				ctx->frame->format = ipid->pfmt;
				ctx->frame->nb_samples = data_size / ipid->nb_ch / ipid->bps;
				if (ipid->planar) {
					u32 ch_idx;
					for (ch_idx=0; ch_idx<ipid->nb_ch; ch_idx++) {
						ctx->frame->extended_data[ch_idx] = (uint8_t *) data + ctx->frame->nb_samples*ipid->bps*ch_idx;
					}
				}
			}
			/* push the decoded frame into the filtergraph */
			ret = av_buffersrc_add_frame_flags(ipid->io_filter_ctx, ctx->frame, 0);
			if (ret < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to push frame to filtergaph: %s\n", av_err2str(ret) ));
				e = GF_SERVICE_ERROR;
				break;
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to fetch data from frame\n"));
			e = GF_SERVICE_ERROR;
			break;
		}
		gf_filter_pid_drop_packet(ipid->io_pid);
	}
	if (count>nb_eos) nb_eos=0;

	if (ctx->flush_state==1) {
		ctx->flush_state = 2;
		nb_eos = 0;
	}

	//pull output
	count = gf_list_count(ctx->opids);
	for (i=0; i<count; i++) {
		GF_FFAVPid *opid = gf_list_get(ctx->opids, i);
		if (!nb_eos && gf_filter_pid_would_block(opid->io_pid)) {
			continue;
		}

		AVFrame *frame = av_frame_alloc();

		ret = av_buffersink_get_frame(opid->io_filter_ctx, frame);
        if (ret < 0) {
			if (ret == AVERROR_EOF) {
				if (ctx->flush_state==2) {
					nb_eos++;
				} else if (nb_eos) {
					gf_filter_pid_set_eos(opid->io_pid);
				} else if (!ctx->nb_inputs) {
					gf_filter_pid_set_eos(opid->io_pid);
					nb_eos++;
				}
			} else if (ret != AVERROR(EAGAIN)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Fail to pull frame from filtergaph: %s\n", av_err2str(ret) ));
				e = GF_SERVICE_ERROR;
			}
			av_frame_free(&frame);
			break;
        }
        if (opid->is_video) {
			u8 *buffer;
			u32 j;
			GF_FilterPacket *pck;
			Bool update_props=GF_TRUE;
			if (frame->width!=opid->width) {}
			else if (frame->height!=opid->height) {}
			else if (frame->format != opid->pfmt) {}
			else {
				update_props = GF_FALSE;
			}
			if (update_props) {
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_WIDTH, &PROP_UINT(frame->width));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_HEIGHT, &PROP_UINT(frame->height));
				opid->gf_pfmt = ffmpeg_pixfmt_to_gpac(frame->format, GF_FALSE);
				if (ffmpeg_pixfmt_is_fullrange(frame->format)) {
					gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL(GF_TRUE));
				} else {
					gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_COLR_RANGE, NULL);
				}
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_PIXFMT, &PROP_UINT(opid->gf_pfmt));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STRIDE, &PROP_UINT(frame->linesize[0]));
				if (frame->linesize[1])
					gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STRIDE_UV, &PROP_UINT(frame->linesize[1]));
				else
					gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_STRIDE_UV, NULL);

				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(opid->io_filter_ctx->inputs[0]->time_base.den) );
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_FPS, &PROP_FRAC_INT(opid->io_filter_ctx->inputs[0]->time_base.den, opid->io_filter_ctx->inputs[0]->time_base.num) );

				opid->width = frame->width;
				opid->height = frame->height;
				opid->pfmt = frame->format;
				opid->tb_num = opid->io_filter_ctx->inputs[0]->time_base.num;
				opid->stride = 0;
				opid->stride_uv = 0;
				opid->bpp = gf_pixel_get_bytes_per_pixel(opid->gf_pfmt);
				gf_pixel_get_size_info(opid->gf_pfmt, opid->width, opid->height, &opid->out_size, &opid->stride, &opid->stride_uv, NULL, &opid->uv_height);
				if ((opid->gf_pfmt==GF_PIXEL_YUV444) || (opid->gf_pfmt==GF_PIXEL_YUV444_10)) {
					opid->uv_width = opid->width;
				} else if (opid->uv_height) {
					opid->uv_width = opid->width/2;
				} else {
					opid->uv_width = 0;
				}
				if (ctx->nb_a_out+ctx->nb_v_out>1) {
					gf_filter_pid_set_property_str(opid->io_pid, "ffid", &PROP_STRING(opid->io_filter_ctx->name));
				}
			}
			pck = gf_filter_pck_new_alloc(opid->io_pid, opid->out_size, &buffer);
			if (!pck) return GF_OUT_OF_MEM;

			for (j=0; j<opid->height; j++) {
				memcpy(buffer + j*opid->stride, frame->data[0] + j*frame->linesize[0], opid->width*opid->bpp);
			}
			if (frame->linesize[1]) {
				buffer += opid->height*opid->stride;
				for (j=0; j<opid->uv_height; j++) {
					memcpy(buffer + j*opid->stride_uv, frame->data[1] + j*frame->linesize[1], opid->uv_width*opid->bpp);
				}
			}
			if (frame->linesize[2]) {
				buffer += opid->uv_height*opid->stride_uv;
				for (j=0; j<opid->uv_height; j++) {
					memcpy(buffer + j*opid->stride_uv, frame->data[2] + j*frame->linesize[2], opid->uv_width*opid->bpp);
				}
			}
			if (frame->linesize[3]) {
				buffer += opid->uv_height*opid->stride_uv;
				for (j=0; j<opid->height; j++) {
					memcpy(buffer + j*opid->stride, frame->data[3] + j*frame->linesize[3], opid->width*opid->bpp);
				}
			}
			if (frame->interlaced_frame)
				gf_filter_pck_set_interlaced(pck, frame->top_field_first ? 1 : 2);

			gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_cts(pck, frame->pts * opid->tb_num);
			gf_filter_pck_send(pck);
		} else {
			u8 *buffer;
			u32 j, out_size;
			GF_FilterPacket *pck;
			Bool update_props=GF_TRUE;
			if (frame->sample_rate!=opid->sr) {}
#ifdef FFMPEG_OLD_CHLAYOUT
			else if (frame->channel_layout!=opid->ch_layout) {}
			else if (frame->channels != opid->nb_ch) {}
#else
			else if (frame->ch_layout.u.mask!=opid->ch_layout) {}
			else if (frame->ch_layout.nb_channels != opid->nb_ch) {}
#endif
			else if (frame->format != opid->pfmt) {}
			else {
				update_props = GF_FALSE;
			}
			if (update_props) {
#ifdef FFMPEG_OLD_CHLAYOUT
				u32 nb_ch = frame->channels;
				u64 ff_ch_layout = frame->channel_layout;
#else
				u32 nb_ch = frame->ch_layout.nb_channels;
				u64 ff_ch_layout = (frame->ch_layout.order>=AV_CHANNEL_ORDER_CUSTOM) ? 0 : frame->ch_layout.u.mask;
#endif
				u64 gpac_ch_layout = ffmpeg_channel_layout_to_gpac(ff_ch_layout);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(frame->sample_rate));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(gpac_ch_layout));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
				opid->gf_pfmt = ffmpeg_audio_fmt_to_gpac(frame->format);
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(opid->gf_pfmt));
				gf_filter_pid_set_property(opid->io_pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(opid->io_filter_ctx->inputs[0]->time_base.den) );

				opid->sr = frame->sample_rate;
				opid->ch_layout = ff_ch_layout;
				opid->nb_ch = nb_ch;
				opid->pfmt = frame->format;
				opid->tb_num = opid->io_filter_ctx->inputs[0]->time_base.num;
				opid->bps = gf_audio_fmt_bit_depth(opid->gf_pfmt) / 8;
				if (ctx->nb_a_out+ctx->nb_v_out>1) {
					gf_filter_pid_set_property_str(opid->io_pid, "ffid", &PROP_STRING(opid->io_filter_ctx->name));
				}
			}
			out_size = 0;
			for (j=0; j<8; j++) {
				if (!frame->linesize[j]) break;
				out_size += frame->linesize[j];
			}

			pck = gf_filter_pck_new_alloc(opid->io_pid, out_size, &buffer);
			if (!pck) return GF_OUT_OF_MEM;

			for (j=0; j<8; j++) {
				if (!frame->linesize[j]) break;
				memcpy(buffer, frame->data[0], frame->linesize[j]);
				buffer += frame->linesize[j];
			}
			gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_cts(pck, frame->pts * opid->tb_num);
			gf_filter_pck_send(pck);
		}
		av_frame_free(&frame);
	}
	if (e) return e;
	if (ctx->flush_state==2) {
		if (nb_eos<count) return GF_OK;
		return ffavf_reconfigure_graph(filter, ctx);
	}
	if (nb_eos) {
		if (ctx->nb_inputs) {
			return GF_EOS;
		} else if (nb_eos == count) {
			ctx->done = GF_TRUE;
			return GF_EOS;
		}
	}
	return GF_OK;
}

static GF_Err ffavf_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 streamtype;
	GF_FFAVPid *pid_ctx;
	Bool check_recfg = GF_FALSE;
	const GF_PropertyValue *p;
	GF_Fraction timebase;
	GF_FFAVFilterCtx *ctx = (GF_FFAVFilterCtx *) gf_filter_get_udta(filter);

	gf_filter_pid_check_caps(pid);
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_BAD_PARAM;
	streamtype = p->value.uint;

	pid_ctx = gf_filter_pid_get_udta(pid);

	if (is_remove) {
		if (pid_ctx) {
			gf_filter_pid_set_udta(pid, NULL);
			ctx->nb_inputs--;
			if (!ctx->nb_inputs) {
				ffavf_reset_graph(ctx);
				while (gf_list_count(ctx->opids)) {
					GF_FFAVPid *opid = gf_list_pop_back(ctx->opids);
					//io_filter_ctx is destroyed while resetting the graph
					gf_filter_pid_remove(opid->io_pid);
					gf_free(opid);
				}
			}
		}
		return GF_OK;
	}
	if (!pid_ctx) {
		GF_SAFEALLOC(pid_ctx, GF_FFAVPid);
		if (!pid_ctx) return GF_OUT_OF_MEM;
		
		pid_ctx->io_pid = pid;
		gf_filter_pid_set_udta(pid, pid_ctx);
		gf_list_add(ctx->ipids, pid_ctx);
	} else {
		check_recfg = GF_TRUE;
	}

	timebase.num = 1;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (!p) return GF_BAD_PARAM;
	timebase.den = p->value.uint;

	ctx->in_error = GF_OK;

	if (streamtype==GF_STREAM_VISUAL) {
		u32 width, height, pix_fmt, gf_pfmt;
		GF_Fraction sar={1,1};
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
		if (!p) return GF_OK; //not ready yet
		width = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
		if (!p) return GF_OK; //not ready yet
		height = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
		if (!p) return GF_OK; //not ready yet
		gf_pfmt = p->value.uint;
		pix_fmt = ffmpeg_pixfmt_from_gpac(gf_pfmt, GF_FALSE);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
		if (p && p->value.frac.num && p->value.frac.den) sar = p->value.frac;

		pid_ctx->stride = pid_ctx->stride_uv = 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
		if (p) pid_ctx->stride = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE_UV);
		if (p) pid_ctx->stride_uv = p->value.uint;
		gf_pixel_get_size_info(gf_pfmt, width, height, NULL, &pid_ctx->stride, &pid_ctx->stride_uv, &pid_ctx->nb_planes, &pid_ctx->uv_height);

		if (check_recfg) {
			check_recfg = GF_FALSE;
			if (width!=pid_ctx->width) {}
			else if (height!=pid_ctx->height) {}
			else if (pix_fmt!=pid_ctx->pfmt) {}
			else if (timebase.den!=pid_ctx->timescale) {}
			else if (sar.den * pid_ctx->sar.num != sar.num * pid_ctx->sar.den) {}
			else {
				return GF_OK;
			}
		}
		pid_ctx->width = width;
		pid_ctx->height = height;
		pid_ctx->pfmt = pix_fmt;
		pid_ctx->timescale = timebase.den;
		pid_ctx->sar = sar;
	} else if (streamtype==GF_STREAM_AUDIO) {
		u64 ch_layout=0;
		u32 sr, afmt, nb_ch;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
		if (p) ch_layout = ffmpeg_channel_layout_from_gpac(p->value.longuint);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (!p) return GF_OK; //not ready yet
		nb_ch = p->value.uint;
		if (!ch_layout) {
#ifdef FFMPEG_OLD_CHLAYOUT
			ch_layout = av_get_default_channel_layout(p->value.uint);
#else
			AVChannelLayout ff_ch_layout;
			av_channel_layout_default(&ff_ch_layout, p->value.uint);
			ch_layout = ff_ch_layout.u.mask;
#endif
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (!p) return GF_OK; //not ready yet
		sr = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
		if (!p) return GF_OK; //not ready yet
		afmt = ffmpeg_audio_fmt_from_gpac(p->value.uint);
		pid_ctx->bps = gf_audio_fmt_bit_depth(p->value.uint) / 8;
		pid_ctx->planar = gf_audio_fmt_is_planar(p->value.uint);

		if (check_recfg) {
			if (sr!=pid_ctx->sr) {}
			else if (afmt!=pid_ctx->pfmt) {}
			else if (nb_ch!=pid_ctx->nb_ch) {}
			else if (ch_layout!=pid_ctx->ch_layout) {}
			else if (timebase.den!=pid_ctx->timescale) {}
			else {
				return GF_OK;
			}
		}
		pid_ctx->sr = sr;
		pid_ctx->ch_layout = ch_layout;
		pid_ctx->pfmt = afmt;
		pid_ctx->nb_ch = nb_ch;
		pid_ctx->timescale = timebase.den;
	} else {
		return GF_NOT_SUPPORTED;
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	//we have not yet configured graph, no need to flush
	if (ctx->configure_state==1) {
		check_recfg = GF_FALSE;
	}

	//pid config change, flush current graph and resetup
	if (check_recfg) ctx->flush_state = 1;
	//graph has already been loaded, we need to flush/resetup to add the pid
	if (ctx->configure_state==2) ctx->flush_state = 1;
	//in flush state, either because of this reconfig or a previous one
	if (ctx->flush_state)
		return GF_OK;

	//setup input connections
	ctx->in_error = ffavf_setup_input(ctx, pid_ctx);
	if (ctx->in_error) return ctx->in_error;

	//mark graph to be rebuild
	ctx->configure_state = 1;
	//setup output connections
	ctx->in_error = ffavf_setup_outputs(filter, ctx);
	return ctx->in_error;
}


static void ffavf_finalize(GF_Filter *filter)
{
	GF_FFAVFilterCtx *ctx = (GF_FFAVFilterCtx *) gf_filter_get_udta(filter);

	ffavf_reset_graph(ctx);
	while (gf_list_count(ctx->ipids)) {
		GF_FFAVPid *ipid = gf_list_pop_back(ctx->ipids);
		//io_filter_ctx is destroyed while resetting the graph
		gf_free(ipid);
	}
	gf_list_del(ctx->ipids);
	while (gf_list_count(ctx->opids)) {
		GF_FFAVPid *opid = gf_list_pop_back(ctx->opids);
		//io_filter_ctx is destroyed while resetting the graph
		gf_free(opid);
	}
	gf_list_del(ctx->opids);
	if (ctx->filter_desc) gf_free(ctx->filter_desc);
	if (ctx->frame) av_frame_free(&ctx->frame);
}

static GF_Err ffavf_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	int ret;
	char *arg_value;

	GF_FFAVFilterCtx *ctx = gf_filter_get_udta(filter);

	if (!strcmp(arg_name, "f")) {
		if (ctx->filter_graph) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Cannot update filter description while running, not supported\n"));
			return GF_NOT_SUPPORTED;
		}
		if (ctx->filter_desc) gf_free(ctx->filter_desc);
		ctx->filter_desc = gf_strdup(arg_val->value.string);
		return GF_OK;
	}

	arg_value = NULL;
	if  (arg_val->type == GF_PROP_STRING) {
		arg_value = arg_val->value.string;
	}

	if (!strcmp(arg_name, "dump")) {
		ffavf_dump_graph(ctx, (arg_value && strlen(arg_value) ) ? arg_value : NULL);
		//do not change the dump value
		return GF_NOT_FOUND;
	}

	if (!arg_value) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
		return GF_NOT_SUPPORTED;
	}

	if (ctx->filter_graph) {
		char *arg = (char *) arg_name;
		char szTargetName[101];
		char szCommandRes[1025];
		char *target = strchr(arg_name, gf_filter_get_sep(filter, GF_FS_SEP_FRAG));
		if (target) {
			u32 len = (u32) (target - arg_name);
			if (len>=100) len=100;
			strncpy(szTargetName, arg_name, len);
			szTargetName[100] = 0;
			arg = target+1;
		} else {
			strcpy(szTargetName, "all");
		}
		ret = avfilter_graph_send_command(ctx->filter_graph, szTargetName, arg, arg_value, szCommandRes, 1024, 0);
		if (ret<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[FFAVF] Failed to execute command %s: %s\n", arg_name, av_err2str(ret) ));
			return GF_BAD_PARAM;
		}
		return GF_OK;
	}
	//other options are not allowed, they MUST be passed as part of `f` option
	return GF_NOT_FOUND;
}

static Bool ffavf_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FFAVFilterCtx *ctx = gf_filter_get_udta(filter);

	if (evt->base.type == GF_FEVT_PLAY) {
		if (!ctx->nb_playing && !ctx->nb_inputs) ctx->done = GF_FALSE;
		ctx->nb_playing++;
	}
	else if (evt->base.type == GF_FEVT_STOP) {
		if (ctx->nb_playing) {
			ctx->nb_playing--;
			if (!ctx->nb_playing && !ctx->nb_inputs) ctx->done = GF_TRUE;
		}
	}

	if (ctx->nb_inputs) return GF_FALSE;
	return GF_TRUE;
}

static const GF_FilterCapability FFAVFilterCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister FFAVFilterRegister = {
	.name = "ffavf",
	.version = LIBAVFILTER_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG AVFilter")
	GF_FS_SET_HELP("This filter provides libavfilter raw audio and video tools.\n"
		"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details\n"
		"To list all supported avfilters for your GPAC build, use `gpac -h ffavf:*`.\n"
		"\n"
		"# Declaring a filter\n"
		"The filter loads a filter or a filter chain description from the [-f]() option.\n"
		"EX ffavf:f=showspectrum\n"
		"\n"
		"Unlike other FFMPEG bindings in GPAC, this filter does not parse other libavfilter options, you must specify them directly in the filter chain, and the [-f]() option will have to be escaped.\n"
		"EX ffavf::f=showspectrum=size=320x320 or ffavf::f=showspectrum=size=320x320::pfmt=rgb\n"
		"EX ffavf::f=anullsrc=channel_layout=5.1:sample_rate=48000\n"
		"\n"
		"For complex filter graphs, it is possible to store options in a file (e.g. `opts.txt`):\n"
		"EX :f=anullsrc=channel_layout=5.1:sample_rate=48000\n"
		"And load arguments from file:\n"
		"EX ffavf:opts.txt aout\n"
		"\n"
		"The filter will automatically create `buffer` and `buffersink` AV filters for data exchange between GPAC and libavfilter.\n"
		"The builtin options ( [-pfmt](), [-afmt]() ...) can be used to configure the `buffersink` filter to set the output format of the filter.\n"
		"\n"
		"# Naming of PIDs\n"
		"For simple filter graphs with only one input and one output, the input PID is assigned the avfilter name `in` and the output PID is assigned the avfilter name `out`\n"
		"\n"
		"When a graph has several inputs, input PID names shall be assigned by the user using the `ffid` property, and mapping must be done in the filter.\n"
		"EX gpac -i video:#ffid=a -i logo:#ffid=b ffavf::f=[a][b]overlay=main_w-overlay_w-10:main_h-overlay_h-10 vout\n"
		"In this example:\n"
		"- the video source is identified as `a`\n"
		"- the logo source is identified as `b`\n"
		"- the filter declaration maps `a` to its first input (in this case, main video) and `b` to its second input (in this case the overlay)\n"
	   "\n"
		"When a graph has several outputs, output PIDs will be identified using the `ffid` property set to the output avfilter name.\n"
		"EX gpac -i source ffavf::f=split inspect:SID=#ffid=out0 vout#SID=out1\n"
		"In this example:\n"
		"- the splitter produces 2 video streams `out0` and `out1`\n"
		"- the inspector only process stream with ffid `out0`\n"
		"- the video output only displays stream with ffid `out1`\n"
		"\n"
		"The name(s) of the final output of the avfilter graph cannot be configured in GPAC. You can however name intermediate output(s) in a complex filter chain as usual.\n"
		"\n"
		"# Filter graph commands\n"
		"The filter handles option updates as commands passed to the AV filter graph. The syntax expected in the option name is:\n"
		"- com_name=value: sends command `com_name` with value `value` to all filters\n"
		"- name#com_name=value: sends command `com_name` with value `value` to filter named `name`\n"
		"\n"
	)
	.flags =  GF_FS_REG_META | GF_FS_REG_EXPLICIT_ONLY | GF_FS_REG_ALLOW_CYCLIC | GF_FS_REG_TEMP_INIT,
	.private_size = sizeof(GF_FFAVFilterCtx),
	SETCAPS(FFAVFilterCaps),
	.initialize = ffavf_initialize,
	.finalize = ffavf_finalize,
	.configure_pid = ffavf_configure_pid,
	.process = ffavf_process,
	.process_event = ffavf_process_event,
	.update_arg = ffavf_update_arg,
};

#define OFFS(_n)	#_n, offsetof(GF_FFAVFilterCtx, _n)

static const GF_FilterArgs FFAVFilterArgs[] =
{
	{ "f", -1, "filter or filter chain description", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{ OFFS(pfmt), "pixel format of output. If not set, let AVFilter decide", GF_PROP_PIXFMT, "none", NULL, 0},
	{ OFFS(afmt), "audio format of output. If not set, let AVFilter decide", GF_PROP_PCMFMT, "none", NULL, 0},
	{ OFFS(sr), "sample rate of output. If not set, let AVFilter decide", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(ch), "number of channels of output. If not set, let AVFilter decide", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(dump), "dump graph as log media@info or stderr if not set", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ "*", -1, "any possible options defined for AVFilter and sub-classes (see `gpac -hx ffavf` and `gpac -hx ffavf:*`)", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

const int FFAVF_STATIC_ARGS = (sizeof (FFAVFilterArgs) / sizeof (GF_FilterArgs)) - 1;

const GF_FilterRegister *ffavf_register(GF_FilterSession *session)
{
	return ffmpeg_build_register(session, &FFAVFilterRegister, FFAVFilterArgs, FFAVF_STATIC_ARGS, FF_REG_TYPE_AVF);
}

#else
#include <gpac/filters.h>
const GF_FilterRegister *ffavf_register(GF_FilterSession *session)
{
	return NULL;
}
#endif
