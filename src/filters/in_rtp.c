/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP/RTSP input filter
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

#include "in_rtp.h"

#ifndef GPAC_DISABLE_STREAMING


static void rtpin_reset(GF_RTPIn *ctx, Bool is_finalized)
{
	GF_RTPInStream *st;
	while (gf_list_count(ctx->streams)) {
		st = (GF_RTPInStream *)gf_list_get(ctx->streams, 0);
		gf_list_rem(ctx->streams, 0);
		if (!is_finalized && st->opid) gf_filter_pid_remove(st->opid);
		st->opid = NULL;
		rtpin_stream_del(st);
	}

	rtpin_rtsp_del(ctx->session);
	ctx->session = NULL;

	if (ctx->iod_desc) gf_odf_desc_del(ctx->iod_desc);
	ctx->iod_desc = NULL;
}

static GF_FilterProbeScore rtpin_probe_url(const char *url, const char *mime)
{
	/*embedded data - TO TEST*/
	if (strstr(url, "data:application/mpeg4-od-au;base64")
		|| strstr(url, "data:application/mpeg4-bifs-au;base64")
		|| strstr(url, "data:application/mpeg4-es-au;base64"))
	{
		return GF_FPROBE_SUPPORTED;
	}

	/*we need rtsp/tcp , rtsp/udp or direct RTP sender (no control)*/
	if (!strnicmp(url, "rtsp://", 7)
		|| !strnicmp(url, "rtspu://", 8)
		|| !strnicmp(url, "rtp://", 6)
		|| !strnicmp(url, "satip://", 6))
	{
		return GF_FPROBE_SUPPORTED;
	}

	return GF_FPROBE_NOT_SUPPORTED;
}

//simplified version of RTSP_UnpackURL for SAT>IP
static void rtpin_satip_get_server_ip(const char *sURL, char *Server)
{
	char schema[10], *test, text[1024], *retest;
	u32 i, len;
	Bool is_ipv6;

	strcpy(Server, "");

	//extract the schema
	i = 0;
	while (i <= strlen(sURL)) {
		if (sURL[i] == ':')
			goto found;
		schema[i] = sURL[i];
		i += 1;
	}
	return;

found:
	schema[i] = 0;
	if (stricmp(schema, "satip")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Wrong SATIP schema %s - not setting up\n", schema));
		return;
	}
	test = strstr(sURL, "://");
	test += 3;

	//check for port
	retest = strrchr(test, ':');
	/*IPV6 address*/
	if (retest && strchr(retest, ']')) retest = NULL;

	if (retest && strstr(retest, "/")) {
		retest += 1;
		i = 0;
		while (i<strlen(retest)) {
			if (retest[i] == '/') break;
			text[i] = retest[i];
			i += 1;
		}
		text[i] = 0;
	}
	//get the server name
	is_ipv6 = GF_FALSE;
	len = (u32)strlen(test);
	i = 0;
	while (i<len) {
		if (test[i] == '[') is_ipv6 = GF_TRUE;
		else if (test[i] == ']') is_ipv6 = GF_FALSE;
		if ((test[i] == '/') || (!is_ipv6 && (test[i] == ':'))) break;
		text[i] = test[i];
		i += 1;
	}
	text[i] = 0;
	strcpy(Server, text);
}


static GF_Err rtpin_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	const char *src = NULL;
	u32 crc;
	GF_RTPIn *ctx = gf_filter_get_udta(filter);

	if (ctx->src) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[RTPIn] Configure pid called on filter instanciated with SRC %s\n", ctx->src));
		return GF_BAD_PARAM;
	}

	if (is_remove) {
		ctx->ipid = NULL;
		//reset session, remove pids
		rtpin_reset(ctx, GF_FALSE);
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	//we must have a file path
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (prop && prop->value.string) src = prop->value.string;
	if (!src)
		return GF_NOT_SUPPORTED;

	crc = gf_crc_32(src, strlen(src) );

	if (!ctx->ipid) {
		ctx->ipid = pid;
	} else {
		if (pid != ctx->ipid) {
			return GF_REQUIRES_NEW_INSTANCE;
		}
		if (ctx->sdp_url_crc == crc) return GF_OK;

		//reset session, remove pids for now
		rtpin_reset(ctx, GF_FALSE);
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	ctx->sdp_url_crc = crc;
	ctx->sdp_loaded = GF_FALSE;
	return GF_OK;
}


static void gf_rtp_switch_quality(GF_RTPIn *rtp, Bool switch_up)
{
	u32 i,count;
	GF_RTPInStream *stream, *cur_stream;

	count = gf_list_count(rtp->streams);
	/*find the current stream*/
	stream = cur_stream = NULL;
	for (i = 0; i < count; i++) {
		cur_stream = (GF_RTPInStream *) gf_list_get(rtp->streams, i);
		if (cur_stream->mid != rtp->cur_mid) {
			cur_stream = NULL;
			continue;
		}
		break;
	}
	if (!cur_stream) return;

	if (switch_up) {
		/*this is the highest stream*/
		if (!cur_stream->next_stream) {
			cur_stream->status = RTP_Running;
			return;
		} else {
			for (i = 0; i < count; i++) {
				stream = (GF_RTPInStream *) gf_list_get(rtp->streams, i);
				if (stream->mid == cur_stream->next_stream) {
					/*resume streaming next channel*/
					rtpin_stream_init(stream, GF_FALSE);
					stream->status = RTP_Running;
					rtp->cur_mid = stream->mid;
					break;
				}

			}
		}
	} else {
		/*this is the lowest stream i.e base layer*/
		if (!cur_stream->prev_stream) {
			cur_stream->status = RTP_Running;
			return;
		} else {
			for (i = 0; i < count; i++) {
				stream = (GF_RTPInStream *) gf_list_get(rtp->streams, i);
				if (stream->mid == cur_stream->prev_stream) {
					/*stop streaming current channel*/
					gf_rtp_stop(cur_stream->rtp_ch);
					cur_stream->status = RTP_Connected;
					rtp->cur_mid = stream->mid;
					break;
				}
			}
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("Switch from ES%d to ES %d\n", cur_stream->mid, stream->mid));
	return;
}


static void rtpin_send_data_base64(GF_RTPInStream *stream)
{
	u32 size;
	char *pck_data;
	GF_FilterPacket *pck;
	char *data;

	if (stream->current_start<0) return;

	data = strstr(stream->control, ";base64");
	if (!data) return;

	/*decode data*/
	data = strstr(data, ",");
	data += 1;
	size = gf_base64_decode(data, (u32) strlen(data), stream->buffer, stream->rtpin->block_size);

	pck = gf_filter_pck_new_alloc(stream->opid, size, &pck_data);
	memcpy(pck_data, stream->buffer, size);
	gf_filter_pck_set_cts(pck, (stream->current_start * stream->ts_res));
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(pck);

	stream->flags &= ~GF_RTP_NEW_AU;
	stream->current_start = -1;
}

static void rtpin_check_setup(GF_RTPInStream *stream)
{
	RTPIn_StreamDescribe ch_desc;
	switch (stream->status) {
	case RTP_Connected:
	case RTP_Running:
		rtpin_stream_ack_connect(stream, GF_OK);
		return;
	default:
		break;
	}
	memset(&ch_desc, 0, sizeof(RTPIn_StreamDescribe));
	ch_desc.opid = stream->opid;
	rtpin_stream_setup(stream, &ch_desc);
}

static Bool rtpin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_RTPInStream *stream;
	GF_RTPIn *ctx = (GF_RTPIn *) gf_filter_get_udta(filter);
	Bool reset_stream = GF_FALSE;

	if (evt->base.type == GF_FEVT_QUALITY_SWITCH) {
		gf_rtp_switch_quality(ctx, evt->quality_switch.up);
		return GF_TRUE;
	}

	/*ignore commands other than channels one*/
	if (!evt->base.on_pid) {
		return GF_TRUE;
	}

	stream = rtpin_find_stream(ctx, evt->base.on_pid, 0, NULL, GF_FALSE);
	if (!stream) return GF_TRUE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if ((ctx->last_start_range >= 0) && (ctx->last_start_range==evt->play.start_range)) return GF_TRUE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] Processing play on channel @%08x - %s\n", stream, stream->rtsp ? "RTSP control" : "No control (RTP)" ));
		/*is this RTSP or direct RTP?*/
		stream->flags &= ~RTP_EOS;
		if (stream->rtsp) {
			//send a setup if needed
			rtpin_check_setup(stream);

			//if not aggregated control or no more queued events send a play
			if (! (stream->rtsp->flags & RTSP_AGG_CONTROL) )  {
				rtpin_rtsp_usercom_send(stream->rtsp, stream, evt);
				ctx->last_start_range = evt->play.start_range;
			} else {
			//tricky point here: the play events may get at different times depending on the length
			//of each filter chain connected to our output pids.
			//we store the event and wait for no more pending events
				if (!ctx->postponed_play_stream) {
					ctx->postponed_play = evt->play;
					ctx->postponed_play_stream = stream;
					ctx->last_start_range = evt->play.start_range;
				}
				return GF_TRUE;
			}
		} else {
			ctx->last_start_range = evt->play.start_range;
			stream->status = RTP_Running;
			if (!stream->next_stream)
				ctx->cur_mid = stream->mid;

			if (stream->rtp_ch) {
				//wait for RTCP to perform stream sync
				stream->rtcp_init = GF_FALSE;
				rtpin_stream_init(stream, (stream->flags & RTP_CONNECTED) ? GF_TRUE : GF_FALSE);
				gf_rtp_set_info_rtp(stream->rtp_ch, 0, 0, 0);
			} else {
				/*direct channel, store current start*/
				stream->current_start = evt->play.start_range;
				stream->flags |= GF_RTP_NEW_AU;
				gf_rtp_depacketizer_reset(stream->depacketizer, GF_FALSE);
			}
		}
		break;
	case GF_FEVT_STOP:
		/*is this RTSP or direct RTP?*/
		if (stream->rtsp) {
			rtpin_rtsp_usercom_send(stream->rtsp, stream, evt);
		} else {
			stream->status = RTP_Connected;
			stream->rtpin->last_ntp = 0;
		}
		ctx->last_start_range = -1.0;
		stream->rtcp_init = GF_FALSE;
		reset_stream = stream->pck_queue ? GF_TRUE : GF_FALSE;
		break;
	case GF_FEVT_SET_SPEED:
	case GF_FEVT_PAUSE:
	case GF_FEVT_RESUME:
		assert(stream->rtsp);
		rtpin_rtsp_usercom_send(stream->rtsp, stream, evt);
		break;
	default:
		break;
	}

	//flush rtsp commands
	if (ctx->session) {
		rtpin_rtsp_process_commands(ctx->session);
	}
	while (reset_stream && gf_list_count(stream->pck_queue)) {
		GF_FilterPacket *pck = gf_list_pop_front(stream->pck_queue);
		gf_filter_pck_discard(pck);
	}
	//cancel event
	return GF_TRUE;
}

static void rtpin_rtsp_flush(GF_RTPInRTSP *session)
{
	/*process teardown on all sessions*/
	while (!session->connect_error) {
		if (!gf_list_count(session->rtsp_commands))
			break;
		rtpin_rtsp_process_commands(session);
	}
}

static GF_Err rtpin_process(GF_Filter *filter)
{
	u32 i;
	GF_RTPInStream *stream;
	GF_RTPIn *ctx = gf_filter_get_udta(filter);

	if (ctx->ipid) {
		GF_FilterPacket *pck = NULL;

		if (!ctx->sdp_loaded) pck = gf_filter_pid_get_packet(ctx->ipid);

		if (pck) {
			Bool start, end;
			u32 sdp_len;
			const char *sdp_data;
			gf_filter_pck_get_framing(pck, &start, &end);
			assert(end);
		
			sdp_data = gf_filter_pck_get_data(pck, &sdp_len);
			rtpin_load_sdp(ctx, (char *)sdp_data, sdp_len, NULL);
			gf_filter_pid_drop_packet(ctx->ipid);
			ctx->sdp_loaded = GF_TRUE;
		}
		//we act as a source filter, request process task
		gf_filter_post_process_task(filter);
	}

	if (ctx->postponed_play_stream) {
		GF_FilterEvent evt;
		if (gf_filter_get_num_events_queued(filter))
			return GF_OK;
		stream = ctx->postponed_play_stream;
		ctx->postponed_play_stream = NULL;
		evt.play = ctx->postponed_play;
		rtpin_rtsp_usercom_send(stream->rtsp, stream, &evt);
	}


	if (ctx->retry_tcp) {
		GF_FilterEvent evt;
		Bool send_agg_play = GF_TRUE;
		GF_List *streams = gf_list_new();
		ctx->retry_tcp = GF_FALSE;
		ctx->interleave = 1;
		i=0;
		while ((stream = (GF_RTPInStream *)gf_list_enum(ctx->streams, &i))) {
			if (stream->status >= RTP_Setup) {
				gf_list_add(streams, stream);
			}
		}
		rtpin_rtsp_flush(ctx->session);
		/*send teardown*/
		rtpin_rtsp_teardown(ctx->session, NULL);
		rtpin_rtsp_flush(ctx->session);
		//for safety reset the session, some servers don't handle teardown that well
		gf_rtsp_session_reset(ctx->session->session, GF_TRUE);

		ctx->session->flags |= RTSP_FORCE_INTER;
		evt.play = ctx->postponed_play;
		if (!evt.base.type) evt.base.type = GF_FEVT_PLAY;
		gf_rtsp_set_buffer_size(ctx->session->session, ctx->block_size);

		stream = NULL;
		for (i=0; i<gf_list_count(streams); i++) {
			stream = (GF_RTPInStream *)gf_list_get(streams, i);
			//reset status
			stream->status = RTP_Disconnected;
			//reset all dynamic flags
			stream->flags &= ~(RTP_EOS | RTP_SKIP_NEXT_COM | RTP_CONNECTED);
			//mark as interleaved
			stream->flags |= RTP_INTERLEAVED;
			//reset SSRC since wome servers don't include it in interleave response
			gf_rtp_reset_ssrc(stream->rtp_ch);

			//send setup
			rtpin_check_setup(stream);
			rtpin_rtsp_flush(ctx->session);

			//if not aggregated control or no more queued events send a play
			if (! (stream->rtsp->flags & RTSP_AGG_CONTROL) )  {
				evt.base.on_pid = stream->opid;
				rtpin_rtsp_usercom_send(stream->rtsp, stream, &evt);
				send_agg_play = GF_FALSE;
			}
			rtpin_rtsp_flush(ctx->session);
		}
		if (stream && send_agg_play) {
			evt.base.on_pid = stream->opid;
			rtpin_rtsp_usercom_send(ctx->session, stream, &evt);
		}

		gf_list_del(streams);
	}


	/*fecth data on udp*/
	i=0;
	while ((stream = (GF_RTPInStream *)gf_list_enum(ctx->streams, &i))) {
		if ((stream->flags & RTP_EOS) || (stream->status!=RTP_Running) ) continue;
		/*for interleaved channels don't read too fast, query the buffer occupancy*/
		if (stream->flags & RTP_INTERLEAVED) {
			//pid would not block, flush TCP
			if (!gf_filter_pid_would_block(stream->opid))
				stream->rtsp->flags |= RTSP_TCP_FLUSH;
		} else {
			rtpin_stream_read(stream);
		}
	}

	/*and process commands / flush TCP*/
	if (ctx->session) {
		rtpin_rtsp_process_commands(ctx->session);

		if (ctx->session->connect_error) {
			gf_filter_setup_failure(filter, ctx->session->connect_error);
			ctx->session->connect_error = GF_OK;
		}
	}
	return GF_OK;
}


static GF_Err rtpin_initialize(GF_Filter *filter)
{
	GF_RTPIn *ctx = gf_filter_get_udta(filter);
	char *the_ext;

	ctx->streams = gf_list_new();
	ctx->filter = filter;
	//turn on interleave on http port
	if ((ctx->default_port == 80) || (ctx->default_port == 8080))
		ctx->interleave = 1;

	ctx->last_start_range = -1.0;

	//sdp mode, we will have a configure_pid
	if (!ctx->src) return GF_OK;

	/*rtsp and rtsp over udp*/

	the_ext = strrchr(ctx->src, '#');
	if (the_ext) {
		if (!stricmp(the_ext, "#audio")) ctx->stream_type = GF_STREAM_AUDIO;
		else if (!stricmp(the_ext, "#video")) ctx->stream_type = GF_STREAM_VISUAL;
		the_ext[0] = 0;
	}
	ctx->session = rtpin_rtsp_new(ctx, (char *) ctx->src);

	if (!strnicmp(ctx->src, "satip://", 8)) {
		ctx->session->satip = GF_TRUE;
		ctx->session->satip_server = gf_malloc(GF_MAX_PATH);
		rtpin_satip_get_server_ip(ctx->src, ctx->session->satip_server);
	}

	if (!ctx->session) {
		return GF_NOT_SUPPORTED;
	} else {
		rtpin_rtsp_describe_send(ctx->session, 0, NULL);
	}
	return GF_OK;
}


static void rtpin_finalize(GF_Filter *filter)
{
	GF_RTPIn *ctx = gf_filter_get_udta(filter);
	ctx->done = GF_TRUE;
	if (ctx->session) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] Closing RTSP service\n"));
		rtpin_rtsp_flush(ctx->session);
		/*send teardown*/
		rtpin_rtsp_teardown(ctx->session, NULL);
		rtpin_rtsp_flush(ctx->session);
	}

	rtpin_reset(ctx, GF_TRUE);
	gf_list_del(ctx->streams);
}


static const GF_FilterCapability RTPInCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/sdp"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	{},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "sdp"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	{},
};

#define OFFS(_n)	#_n, offsetof(GF_RTPIn, _n)
static const GF_FilterArgs RTPInArgs[] =
{
	{ OFFS(src), "RTSP location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(firstport), "default first port number to use. 0 lets the filter decide", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(mcast_ifce), "IP of the default interface to use for multicast. If NULL, the default system interface will be used", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(mcast_ttl), "Sets multicast TTL", GF_PROP_UINT, "127", "0-127", GF_FALSE},
	{ OFFS(reorder_len), "RTP reorder length in packets", GF_PROP_UINT, "1000", NULL, GF_FALSE},
	{ OFFS(reorder_delay), "Max delay in RTP reorderer, packets will be dispatched after that", GF_PROP_UINT, "200", NULL, GF_FALSE},
	{ OFFS(block_size), "buffer size fur RTP/UDP or RTSP when interleaved", GF_PROP_UINT, "0x200000", NULL, GF_FALSE},
	{ OFFS(disable_rtcp), "Disables RTCP reporting", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(nat_keepalive), "NAT keepalive delay in ms, disabled by default (except for SatIP, set to 30s by default)", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(force_mcast), "Force multicast on indicated IP in RTSP setup", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(use_client_ports), "Force using client ports  (hack for some RTSP servers overriding client ports)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(bandwidth), "sets bandwidth param for RTSP requests", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(default_port), "Sets default RTSP port", GF_PROP_UINT, "554", "0-65535", GF_FALSE},
	{ OFFS(satip_port), "Sets default port for SATIP", GF_PROP_UINT, "1400", "0-65535", GF_FALSE},

	{ OFFS(interleave), "Sets RTP over RTSP; critcial uses interleave only for MPEG-4 systems streams", GF_PROP_BOOL, "no", "no|yes|critical", GF_FALSE},
	{ OFFS(udp_timeout), "Default timeout before considering UDP is down", GF_PROP_UINT, "10000", NULL, GF_FALSE},
	{ OFFS(rtsp_timeout), "Default timeout before considering RTSP is down", GF_PROP_UINT, "3000", NULL, GF_FALSE},
	{ OFFS(rtcp_timeout), "Default timeout for RTCP trafic in ms. After this timeout, playback will start unsync. If 0 always wait for RTCP", GF_PROP_UINT, "5000", NULL, GF_FALSE},

	{ OFFS(first_packet_drop), "Sets number of first RTP packet to drop - 0 if no drop", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(frequency_drop), "Drop 1 out of N packet - 0 disable droping", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(user_agent), "User agent string", GF_PROP_STRING, "GPAC " GPAC_VERSION " RTSP Client", NULL, GF_FALSE},
	{ OFFS(languages), "User agent string, by default solved from GPAC preferences", GF_PROP_STRING, "$GPAC_LANGUAGES", NULL, GF_FALSE},
	{ OFFS(stats), "Updates statistics to the user every given MS, 0 disables reporting", GF_PROP_UINT, "500", NULL, GF_FALSE},


	{}
};


GF_FilterRegister RTPInRegister = {
	.name = "rtpin",
	.description = "RTP/RTSP Input",
	.private_size = sizeof(GF_RTPIn),
	.args = RTPInArgs,
	.initialize = rtpin_initialize,
	.finalize = rtpin_finalize,
	SETCAPS(RTPInCaps),
	.configure_pid = rtpin_configure_pid,
	.process = rtpin_process,
	.process_event = rtpin_process_event,
	.probe_url = rtpin_probe_url
};

#endif


const GF_FilterRegister *rtpin_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_STREAMING
	return &RTPInRegister;
#else
	return NULL;
#endif
}

