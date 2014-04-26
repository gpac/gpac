/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *			Authors: Jean Le Feuvre
 *
 *  This file is part of GPAC / mp4box application
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


#include <gpac/constants.h>
#include <gpac/isomedia.h>

#ifndef GPAC_DISABLE_SENG
#include <gpac/scene_engine.h>
#endif
#ifndef GPAC_DISABLE_STREAMING
#include <gpac/filestreamer.h>
#include <gpac/rtp_streamer.h>
#endif

#include <gpac/mpegts.h>

#ifndef GPAC_DISABLE_STREAMING
#include <gpac/internal/ietf_dev.h>
#endif

#if defined(GPAC_DISABLE_ISOM) || defined(GPAC_DISABLE_ISOM_WRITE)

#error "Cannot compile MP4Box if GPAC is not built with ISO File Format support"

#else

#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)

void PrintStreamerUsage()
{
	fprintf(stderr, "File Streamer Options\n"
	        "\n"
	        "MP4Box can stream ISO files to RTP. The streamer currently doesn't support\n"
	        "data carrouselling and will therefore not handle BIFS and OD streams properly.\n"
	        "\n"
	        "-rtp         enables streamer\n"
	        "-noloop      disables looping when streaming\n"
	        "-mpeg4       forces MPEG-4 ES Generic for all RTP streams\n"
	        "-dst=IP      IP destination (uni/multi-cast). Default: 127.0.0.1\n"
	        "-port=PORT   output port of the first stream. Default: 7000\n"
	        "-mtu=MTU     path MTU for RTP packets. Default is 1450 bytes\n"
	        "-ifce=IFCE   IP address of the physical interface to use. Default: NULL (ANY)\n"
	        "-ttl=TTL     time to live for multicast packets. Default: 1\n"
	        "-sdp=Name    file name of the generated SDP. Default: \"session.sdp\"\n"
	        "\n"
	       );
}

static void on_logs(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list)
{
	FILE *logs = cbk;
	vfprintf(logs, fmt, list);
	fflush(logs);
}

int stream_file_rtp(int argc, char **argv)
{
	GF_ISOMRTPStreamer *file_streamer;
	char *sdp_file = "session.sdp";
	char *ip_dest = "127.0.0.1";
	char *ifce_addr = NULL;
	char *inName = NULL;
	char *logs=NULL;
	FILE *logfile=NULL;
	u16 port = 7000;
	u32 ttl = 1;
	Bool loop = 1;
	Bool mem_track = 0;
	Bool force_mpeg4 = 0;
	u32 path_mtu = 1450;
	u32 i;

	for (i = 1; i < (u32) argc ; i++) {
		char *arg = argv[i];

		if (arg[0] != '-') {
			if (inName) {
				fprintf(stderr, "Error - 2 input names specified, please check usage\n");
				return 1;
			}
			inName = arg;
		}
		else if (!stricmp(arg, "-noloop")) loop = 0;
		else if (!stricmp(arg, "-mpeg4")) force_mpeg4 = 1;
		else if (!strnicmp(arg, "-port=", 6)) port = atoi(arg+6);
		else if (!strnicmp(arg, "-mtu=", 5)) path_mtu = atoi(arg+5);
		else if (!strnicmp(arg, "-dst=", 5)) ip_dest = arg+5;
		else if (!strnicmp(arg, "-ttl=", 5)) ttl = atoi(arg+5);
		else if (!strnicmp(arg, "-ifce=", 6)) ifce_addr = arg+6;
		else if (!strnicmp(arg, "-sdp=", 5)) sdp_file = arg+5;
		else if (!stricmp(arg, "-mem-track")) mem_track = 1;
		else if (!strnicmp(arg, "-logs=", 6)) logs = arg+6;
		else if (!strnicmp(arg, "-lf=", 4)) logfile = gf_f64_open(arg+4, "wt");
	}

	gf_sys_init(mem_track);
	if (logs)
		gf_log_set_tools_levels(logs);
	else
		gf_log_set_tool_level(GF_LOG_RTP, GF_LOG_INFO); //set to debug to have packet list
	if (logfile) {
		gf_log_set_callback(logfile, on_logs);
	}

	if (!gf_isom_probe_file(inName)) {
		fprintf(stderr, "File %s is not a valid ISO Media file and cannot be streamed\n", inName);
		if (logfile) fclose(logfile);
		gf_sys_close();
		return 1;
	}

	file_streamer = gf_isom_streamer_new(inName, ip_dest, port, loop, force_mpeg4, path_mtu, ttl, ifce_addr);
	if (!file_streamer) {
		fprintf(stderr, "Cannot create file streamer\n");
	} else {
		u32 check = 50;
		fprintf(stderr, "Starting streaming %s to %s:%d\n", inName, ip_dest, port);
		gf_isom_streamer_write_sdp(file_streamer, sdp_file);

		while (1) {
			gf_isom_streamer_send_next_packet(file_streamer, 0, 0);
			check--;
			if (!check) {
				if (gf_prompt_has_input()) {
					char c = (char) gf_prompt_get_char();
					if (c=='q') break;
				}
				check = 50;
			}
		}
		gf_isom_streamer_del(file_streamer);
	}
	if (logfile) fclose(logfile);
	gf_sys_close();
	return 0;
}


void PrintLiveUsage()
{
	fprintf(stderr,

	        "Live scene encoder options:\n"
	        "-dst=IP    destination IP - default: NULL\n"
	        "-port=PORT destination port - default: 7000\n"
	        "-mtu=MTU   path MTU for RTP packets. Default is 1450 bytes\n"
	        "-ifce=IFCE IP address of the physical interface to use. Default: NULL(ANY)\n"
	        "-ttl=TTL   time to live for multicast packets. Default: 1\n"
	        "-sdp=Name  ouput SDP file - default: session.sdp\n"
	        "\n"
	        "-dims      turns on DIMS mode for SVG input - default: off\n"
	        "-no-rap    disabled RAP sending - this also disables carousel generation. Default: off\n"
	        "-src=file  source of updates - default: null\n"
	        "-rap=time  duration in ms of base carousel - default: 0 (off)\n"
	        "            you can specify the RAP period of a single ESID (not in DIMS):\n"
	        "                -rap=ESID=X:time\n"
	        "\n"
	        "Runtime options:\n"
	        "q:         quits\n"
	        "u:         inputs some commands to be sent\n"
	        "U:         same as u but signals the updates as critical\n"
	        "e:         inputs some commands to be sent without being aggregated\n"
	        "E:         same as e but signals the updates as critical\n"
	        "f:         forces RAP sending\n"
	        "F:         forces RAP regeneration and sending\n"
	        "p:         dumps current scene\n"
	        "\n"
	        "GPAC version: " GPAC_FULL_VERSION "\n"
	        "");
}
typedef struct
{
	GF_RTPStreamer *rtp;
	Bool manual_rtcp;
	u16 ESID;

	char *carousel_data;
	u32 carousel_size, carousel_alloc;
	u32 last_carousel_time;
	u64 carousel_ts, time_at_carousel_store;

	u32 timescale, init_time;
	u32 carousel_period, ts_delta;
	u16 aggregate_on_stream;
	Bool adjust_carousel_time, discard, aggregate, rap, m2ts_vers_inc;
	u32 critical;
} RTPChannel;

typedef struct
{
	GF_SceneEngine *seng;
	Bool force_carousel, carousel_generation;
	GF_List *streams;
	u32 start_time;
	Bool critical;
} LiveSession;


RTPChannel *next_carousel(LiveSession *sess, u32 *timeout)
{
	RTPChannel *to_send = NULL;
	u32 i, time, count, now;

	if (!sess->start_time) sess->start_time = gf_sys_clock();
	now = gf_sys_clock() - sess->start_time;

	time = (u32) -1;
	count = gf_list_count(sess->streams);
	for (i=0; i<count; i++) {
		RTPChannel *ch = gf_list_get(sess->streams, i);
		if (!ch->carousel_period) continue;
		if (!ch->carousel_size) continue;

		if (!ch->last_carousel_time) ch->last_carousel_time = now;

		if (ch->last_carousel_time + ch->carousel_period < time) {
			to_send = ch;
			time = ch->last_carousel_time + ch->carousel_period;
		}
	}
	if (!to_send) {
		if (timeout) *timeout = 0;
		return NULL;
	}
	if (timeout) {
		if (time>now) time-=now;
		else time=0;
		*timeout = time;
	}
	return to_send;
}


static void live_session_callback(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{
	LiveSession *livesess = (LiveSession *) calling_object;
	RTPChannel *rtpch;
	u32 i=0;

	while ( (rtpch = gf_list_enum(livesess->streams, &i))) {
		if (rtpch->ESID == ESID) {

			/*store carousel data*/
			if (livesess->carousel_generation && rtpch->carousel_period) {
				if (rtpch->carousel_alloc < size) {
					rtpch->carousel_data = gf_realloc(rtpch->carousel_data, size);
					rtpch->carousel_alloc = size;
				}
				memcpy(rtpch->carousel_data, data, size);
				rtpch->carousel_size = size;
				rtpch->carousel_ts = ts;
				rtpch->time_at_carousel_store = gf_sys_clock();
				fprintf(stderr, "\nStream %d: Storing new carousel TS "LLD", %d bytes\n", ESID, ts, size);
			}
			/*send data*/
			else {
				u32 critical = 0;
				Bool rap = rtpch->rap;
				if (livesess->carousel_generation) rap = 1;
				ts += rtpch->timescale*((u64)gf_sys_clock()-rtpch->init_time + rtpch->ts_delta)/1000;
				if (rtpch->critical) critical = rtpch->critical;
				else if (livesess->critical) critical = 1;

				gf_rtp_streamer_send_au_with_sn(rtpch->rtp, data, size, ts, ts, rap, critical);

				fprintf(stderr, "Stream %d: Sending update at TS "LLD", %d bytes - RAP %d - critical %d\n", ESID, ts, size, rap, critical);
				rtpch->rap = rtpch->critical = 0;

				if (rtpch->manual_rtcp) gf_rtp_streamer_send_rtcp(rtpch->rtp, 0, 0, 0, 0, 0);
			}
			return;
		}
	}
}

static void live_session_send_carousel(LiveSession *livesess, RTPChannel *ch)
{
	u32 now = gf_sys_clock();
	u64 ts=0;
	if (ch) {
		if (ch->carousel_size) {
			ts = ch->carousel_ts + ch->timescale * ( (ch->adjust_carousel_time ? (u64)gf_sys_clock() : ch->time_at_carousel_store) - ch->init_time + ch->ts_delta)/1000;

			gf_rtp_streamer_send_au_with_sn(ch->rtp, ch->carousel_data, ch->carousel_size, ts, ts, 1, 0);
			ch->last_carousel_time = now - livesess->start_time;
			fprintf(stderr, "Stream %d: Sending carousel at TS "LLD", %d bytes\n", ch->ESID, ts, ch->carousel_size);

			if (ch->manual_rtcp) {
				ts = ch->carousel_ts + ch->timescale * ( gf_sys_clock() - ch->init_time + ch->ts_delta)/1000;
				gf_rtp_streamer_send_rtcp(ch->rtp, 1, (u32) ts, 0, 0, 0);
			}
		}
	} else {
		u32 i=0;
		while (NULL != (ch = gf_list_enum(livesess->streams, &i))) {
			if (ch->carousel_size) {
				if (ch->adjust_carousel_time) {
					ts = ch->carousel_ts + ch->timescale*(gf_sys_clock()-ch->init_time + ch->ts_delta)/1000;
				} else {
					ts = ch->carousel_ts;
				}
				gf_rtp_streamer_send_au_with_sn(ch->rtp, ch->carousel_data, ch->carousel_size, ts, ts, 1, 0);
				ch->last_carousel_time = now - livesess->start_time;
				fprintf(stderr, "Stream %d: Sending carousel at TS "LLD"	, %d bytes\n", ch->ESID, ts, ch->carousel_size);

				if (ch->manual_rtcp) {
					ts = ch->carousel_ts + ch->timescale*(gf_sys_clock()-ch->init_time + ch->ts_delta)/1000;
					gf_rtp_streamer_send_rtcp(ch->rtp, 1, (u32) ts, 0, 0, 0);
				}
			}
		}
	}
}

static void live_session_setup(LiveSession *livesess, char *ip, u16 port, u32 path_mtu, u32 ttl, char *ifce_addr, char *sdp_name)
{
	RTPChannel *rtpch;
	u32 count = gf_seng_get_stream_count(livesess->seng);
	u32 i;
	char *iod64 = gf_seng_get_base64_iod(livesess->seng);
	char *sdp = gf_rtp_streamer_format_sdp_header("GPACSceneStreamer", ip, NULL, iod64);
	if (iod64) gf_free(iod64);

	for (i=0; i<count; i++) {
		u16 ESID;
		u32 st, oti, ts;
		char *config = NULL;
		u32 config_len;
		gf_seng_get_stream_config(livesess->seng, i, &ESID, &config, &config_len, &st, &oti, &ts);

		GF_SAFEALLOC(rtpch, RTPChannel);
		rtpch->timescale = ts;
		rtpch->init_time = gf_sys_clock();

		switch (st) {
		case GF_STREAM_OD:
		case GF_STREAM_SCENE:
			rtpch->rtp = gf_rtp_streamer_new_extended(st, oti, ts, ip, port, path_mtu, ttl, ifce_addr,
			             GP_RTP_PCK_SYSTEMS_CAROUSEL, (char *) config, config_len,
			             96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4);

			if (rtpch->rtp) {
				gf_rtp_streamer_disable_auto_rtcp(rtpch->rtp);
				rtpch->manual_rtcp = 1;
			}
			break;
		default:
			rtpch->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, path_mtu, ttl, ifce_addr, GP_RTP_PCK_SIGNAL_RAP, (char *) config, config_len);
			break;
		}
		rtpch->ESID = ESID;
		rtpch->adjust_carousel_time = 1;
		gf_list_add(livesess->streams, rtpch);

		gf_rtp_streamer_append_sdp(rtpch->rtp, ESID, (char *) config, config_len, NULL, &sdp);

		/*fetch initial config of the broadcast*/
		gf_seng_get_stream_carousel_info(livesess->seng, ESID, &rtpch->carousel_period, &rtpch->aggregate_on_stream);

		port += 2;
	}
	if (sdp) {
		FILE *out = gf_f64_open(sdp_name, "wt");
		fprintf(out, "%s", sdp);
		fclose(out);
		gf_free(sdp);
	}
}

void live_session_shutdown(LiveSession *livesess)
{
	gf_seng_terminate(livesess->seng);

	if (livesess->streams) {
		while (gf_list_count(livesess->streams)) {
			RTPChannel *rtpch = gf_list_get(livesess->streams, 0);
			gf_list_rem(livesess->streams, 0);
			gf_rtp_streamer_del(rtpch->rtp);
			if (rtpch->carousel_data) gf_free(rtpch->carousel_data);
			gf_free(rtpch);
		}
		gf_list_del(livesess->streams);
	}
}


static RTPChannel *set_broadcast_params(LiveSession *livesess, u16 esid, u32 period, u32 ts_delta, u16 aggregate_on_stream, Bool adjust_carousel_time, Bool force_rap, Bool aggregate_au, Bool discard_pending, Bool signal_rap, u32 signal_critical, Bool version_inc)
{
	RTPChannel *rtpch = NULL;

	/*locate our stream*/
	if (esid) {
		u32 i=0;
		while ( (rtpch = gf_list_enum(livesess->streams, &i))) {
			if (rtpch->ESID == esid) break;
		}
	} else {
		rtpch = gf_list_get(livesess->streams, 0);
	}

	/*TODO - set/reset the ESID for the parsers*/
	if (!rtpch) return NULL;

	/*TODO - if discard is set, abort current carousel*/
	if (discard_pending) {
	}

	/*remember RAP flag*/
	rtpch->rap = signal_rap;
	rtpch->critical = signal_critical;
	rtpch->m2ts_vers_inc = version_inc;

	rtpch->ts_delta = ts_delta;
	rtpch->aggregate = aggregate_au;
	rtpch->adjust_carousel_time = adjust_carousel_time;

	/*change stream aggregation mode*/
	if ((aggregate_on_stream != (u16)-1) && (rtpch->aggregate_on_stream != aggregate_on_stream)) {
		gf_seng_enable_aggregation(livesess->seng, esid, aggregate_on_stream);
		rtpch->aggregate_on_stream = aggregate_on_stream;
	}
	/*change stream aggregation mode*/
	if ((period!=(u32)-1) && (rtpch->carousel_period != period)) {
		rtpch->carousel_period = period;
		rtpch->last_carousel_time = 0;
	}

	if (force_rap) {
		livesess->force_carousel = 1;
	}
	return rtpch;
}

int live_session(int argc, char **argv)
{
	GF_Err e;
	u32 i;
	char *filename = NULL;
	char *dst = NULL;
	char *ifce_addr = NULL;
	char *sdp_name = "session.sdp";
	u16 dst_port = 7000;
	u32 load_type=0;
	u32 check;
	u32 ttl = 1;
	u32 path_mtu = 1450;
	s32 next_time;
	u64 last_src_modif, mod_time;
	char *src_name = NULL;
	Bool run, has_carousel, no_rap;
	Bool udp = 0;
	u16 sk_port=0;
	GF_Socket *sk = NULL;
	LiveSession livesess;
	RTPChannel *ch;
	char *update_buffer = NULL;
	u32 update_buffer_size = 0;
	u16 aggregate_on_stream;
	Bool adjust_carousel_time, force_rap, aggregate_au, discard_pending, signal_rap, version_inc;
	Bool update_context;
	u32 period, ts_delta, signal_critical;
	u16 es_id;
	e = GF_OK;
	aggregate_au = 1;
	es_id = 0;
	no_rap = 0;
	gf_sys_init(GF_FALSE);

	memset(&livesess, 0, sizeof(LiveSession));

	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_INFO);

	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (arg[0] != '-') filename = arg;
		else if (!strnicmp(arg, "-dst=", 5)) dst = arg+5;
		else if (!strnicmp(arg, "-port=", 6)) dst_port = atoi(arg+6);
		else if (!strnicmp(arg, "-sdp=", 5)) sdp_name = arg+5;
		else if (!strnicmp(arg, "-mtu=", 5)) path_mtu = atoi(arg+5);
		else if (!strnicmp(arg, "-ttl=", 5)) ttl = atoi(arg+5);
		else if (!strnicmp(arg, "-ifce=", 6)) ifce_addr = arg+6;
		else if (!strnicmp(arg, "-no-rap", 7)) no_rap = 1;
		else if (!strnicmp(arg, "-dims", 5)) load_type = GF_SM_LOAD_DIMS;
		else if (!strnicmp(arg, "-src=", 5)) src_name = arg+5;
		else if (!strnicmp(arg, "-udp=", 5)) {
			sk_port = atoi(arg+5);
			udp = 1;
		}
		else if (!strnicmp(arg, "-tcp=", 5)) {
			sk_port = atoi(arg+5);
			udp = 0;
		}
	}
	if (!filename) {
		fprintf(stderr, "Missing filename\n");
		PrintLiveUsage();
		return 1;
	}

	if (dst_port && dst) livesess.streams = gf_list_new();

	livesess.seng = gf_seng_init(&livesess, filename, load_type, NULL, (load_type == GF_SM_LOAD_DIMS) ? 1 : 0);
	if (!livesess.seng) {
		fprintf(stderr, "Cannot create scene engine\n");
		return 1;
	}
	if (livesess.streams) live_session_setup(&livesess, dst, dst_port, path_mtu, ttl, ifce_addr, sdp_name);

	has_carousel = 0;
	last_src_modif = src_name ? gf_file_modification_time(src_name) : 0;

	if (sk_port) {
		sk = gf_sk_new(udp ? GF_SOCK_TYPE_UDP : GF_SOCK_TYPE_TCP);
		if (udp) {
			e = gf_sk_bind(sk, NULL, sk_port, NULL, 0, 0);
			if (e != GF_OK) {
				if (sk) gf_sk_del(sk);
				sk = NULL;
			}
		} else {
		}
	}


	for (i=0; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (!strnicmp(arg, "-rap=", 5)) {
			u32 period, id, j;
			RTPChannel *ch;
			period = id = 0;
			if (strchr(arg, ':')) {
				sscanf(arg, "-rap=ESID=%u:%u", &id, &period);
				e = gf_seng_enable_aggregation(livesess.seng, id, 1);
				if (e) {
					fprintf(stderr, "Cannot enable aggregation on stream %u: %s\n", id, gf_error_to_string(e));
					goto exit;
				}
			} else {
				sscanf(arg, "-rap=%u", &period);
			}

			j=0;
			while (NULL != (ch = gf_list_enum(livesess.streams, &j))) {
				if (!id || (ch->ESID==id))
					ch->carousel_period = period;
			}
			has_carousel = 1;
		}
	}

	i=0;
	while (NULL != (ch = gf_list_enum(livesess.streams, &i))) {
		if (ch->carousel_period) {
			has_carousel = 1;
			break;
		}
	}

	update_context = 0;

	if (has_carousel || !no_rap) {
		livesess.carousel_generation = 1;
		gf_seng_encode_context(livesess.seng, live_session_callback);
		livesess.carousel_generation = 0;
	}

	live_session_send_carousel(&livesess, NULL);

	check = 10;
	run = 1;
	while (run) {
		check--;
		if (!check) {
			check = 10;
			if (gf_prompt_has_input()) {
				char c = gf_prompt_get_char();
				switch (c) {
				case 'q':
					run=0;
					break;
				case 'U':
					livesess.critical = 1;
				case 'u':
				{
					GF_Err e;
					char szCom[8192];
					fprintf(stderr, "Enter command to send:\n");
					szCom[0] = 0;
					if (1 > scanf("%[^\t\n]", szCom)) {
						fprintf(stderr, "No command entered properly, aborting.\n");
						break;
					}
					/*stdin flush bug*/
					while (getchar()!='\n') {}
					e = gf_seng_encode_from_string(livesess.seng, 0, 0, szCom, live_session_callback);
					if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
					e = gf_seng_aggregate_context(livesess.seng, 0);
					livesess.critical = 0;
					update_context = 1;
				}
				break;
				case 'E':
					livesess.critical = 1;
				case 'e':
				{
					GF_Err e;
					char szCom[8192];
					fprintf(stderr, "Enter command to send:\n");
					szCom[0] = 0;
					if (1 > scanf("%[^\t\n]", szCom)) {
						printf("No command entered properly, aborting.\n");
						break;
					}
					/*stdin flush bug*/
					while (getchar()!='\n') {}
					e = gf_seng_encode_from_string(livesess.seng, 0, 1, szCom, live_session_callback);
					if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
					livesess.critical = 0;
					e = gf_seng_aggregate_context(livesess.seng, 0);

				}
				break;

				case 'p':
				{
					char rad[GF_MAX_PATH];
					fprintf(stderr, "Enter output file name - \"std\" for stderr: ");
					if (1 > scanf("%s", rad)) {
						fprintf(stderr, "No ouput file name entered, aborting.\n");
						break;
					}
					e = gf_seng_save_context(livesess.seng, !strcmp(rad, "std") ? NULL : rad);
					fprintf(stderr, "Dump done (%s)\n", gf_error_to_string(e));
				}
				break;
				case 'F':
					update_context = 1;
				case 'f':
					livesess.force_carousel = 1;
					break;
				}
				e = GF_OK;
			}
		}

		/*process updates from file source*/
		if (src_name) {
			mod_time = gf_file_modification_time(src_name);
			if (mod_time != last_src_modif) {
				FILE *srcf;
				char flag_buf[201], *flag;
				fprintf(stderr, "Update file modified - processing\n");
				last_src_modif = mod_time;

				srcf = gf_f64_open(src_name, "rt");
				if (!srcf) continue;

				/*checks if we have a broadcast config*/
				if (!fgets(flag_buf, 200, srcf))
					flag_buf[0] = '\0';
				fclose(srcf);

				aggregate_on_stream = (u16) -1;
				adjust_carousel_time = force_rap = discard_pending = signal_rap = signal_critical = 0;
				aggregate_au = version_inc = 1;
				period = -1;
				ts_delta = 0;
				es_id = 0;

				/*find our keyword*/
				flag = strstr(flag_buf, "gpac_broadcast_config ");
				if (flag) {
					flag += strlen("gpac_broadcast_config ");
					/*move to next word*/
					while (flag && (flag[0]==' ')) flag++;

					while (1) {
						char *sep = strchr(flag, ' ');
						if (sep) sep[0] = 0;
						if (!strnicmp(flag, "esid=", 5)) es_id = atoi(flag+5);
						else if (!strnicmp(flag, "period=", 7)) period = atoi(flag+7);
						else if (!strnicmp(flag, "ts=", 3)) ts_delta = atoi(flag+3);
						else if (!strnicmp(flag, "carousel=", 9)) aggregate_on_stream = atoi(flag+9);
						else if (!strnicmp(flag, "restamp=", 8)) adjust_carousel_time = atoi(flag+8);

						else if (!strnicmp(flag, "discard=", 8)) discard_pending = atoi(flag+8);
						else if (!strnicmp(flag, "aggregate=", 10)) aggregate_au = atoi(flag+10);
						else if (!strnicmp(flag, "force_rap=", 10)) force_rap = atoi(flag+10);
						else if (!strnicmp(flag, "rap=", 4)) signal_rap = atoi(flag+4);
						else if (!strnicmp(flag, "critical=", 9)) signal_critical = atoi(flag+9);
						else if (!strnicmp(flag, "vers_inc=", 9)) version_inc = atoi(flag+9);
						if (sep) {
							sep[0] = ' ';
							flag = sep+1;
						} else {
							break;
						}
					}

					set_broadcast_params(&livesess, es_id, period, ts_delta, aggregate_on_stream, adjust_carousel_time, force_rap, aggregate_au, discard_pending, signal_rap, signal_critical, version_inc);
				}

				e = gf_seng_encode_from_file(livesess.seng, es_id, aggregate_au ? 0 : 1, src_name, live_session_callback);
				if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
				e = gf_seng_aggregate_context(livesess.seng, 0);

				update_context = no_rap ? 0 : 1;
			}
		}

		/*process updates from socket source*/
		if (sk) {
			char buffer[2049];
			u32 bytes_read;
			u32 update_length;
			u32 bytes_received;


			e = gf_sk_receive(sk, buffer, 2048, 0, &bytes_read);
			if (e == GF_OK) {
				u32 hdr_length = 0;
				u8 cmd_type = buffer[0];
				bytes_received = 0;
				switch (cmd_type) {
				case 0:
				{
					GF_BitStream *bs = gf_bs_new(buffer, bytes_read, GF_BITSTREAM_READ);
					gf_bs_read_u8(bs);
					es_id = gf_bs_read_u16(bs);
					aggregate_on_stream = gf_bs_read_u16(bs);
					if (aggregate_on_stream==0xFFFF) aggregate_on_stream = -1;
					adjust_carousel_time = gf_bs_read_int(bs, 1);
					force_rap = gf_bs_read_int(bs, 1);
					aggregate_au = gf_bs_read_int(bs, 1);
					discard_pending = gf_bs_read_int(bs, 1);
					signal_rap = gf_bs_read_int(bs, 1);
					signal_critical = gf_bs_read_int(bs, 1);
					version_inc = gf_bs_read_int(bs, 1);
					gf_bs_read_int(bs, 1);
					period = gf_bs_read_u16(bs);
					if (period==0xFFFF) period = -1;
					ts_delta = gf_bs_read_u16(bs);
					update_length = gf_bs_read_u32(bs);
					hdr_length = 12;
					gf_bs_del(bs);
				}

				set_broadcast_params(&livesess, es_id, period, ts_delta, aggregate_on_stream, adjust_carousel_time, force_rap, aggregate_au, discard_pending, signal_rap, signal_critical, version_inc);
				break;
				default:
					update_length = 0;
					break;
				}

				if (update_buffer_size <= update_length) {
					update_buffer = gf_realloc(update_buffer, update_length+1);
					update_buffer_size = update_length+1;
				}
				if (update_length && (bytes_read>hdr_length) ) {
					memcpy(update_buffer, buffer+hdr_length, bytes_read-hdr_length);
					bytes_received = bytes_read-hdr_length;
				}
				while (bytes_received<update_length) {
					e = gf_sk_receive(sk, buffer, 2048, 0, &bytes_read);
					switch (e) {
					case GF_IP_NETWORK_EMPTY:
						break;
					case GF_OK:
						memcpy(update_buffer+bytes_received, buffer, bytes_read);
						bytes_received += bytes_read;
						break;
					default:
						fprintf(stderr, "Error with UDP socket : %s\n", gf_error_to_string(e));
						break;
					}
				}
				update_buffer[update_length] = 0;

				if (update_length) {
					e = gf_seng_encode_from_string(livesess.seng, es_id, aggregate_au ? 0 : 1, update_buffer, live_session_callback);
					if (e) fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
					e = gf_seng_aggregate_context(livesess.seng, 0);

					update_context = 1;
				}
			}
		}

		if (update_context) {
			livesess.carousel_generation=1;
			e = gf_seng_encode_context(livesess.seng, live_session_callback	);
			livesess.carousel_generation=0;
			update_context = 0;
		}

		if (livesess.force_carousel) {
			live_session_send_carousel(&livesess, NULL);
			livesess.force_carousel = 0;
			continue;
		}

		if (!has_carousel) {
			gf_sleep(10);
			continue;
		}
		ch = next_carousel(&livesess, (u32 *) &next_time);
		if ((ch==NULL) || (next_time > 20)) {
			gf_sleep(20);
			continue;
		}
		if (next_time) gf_sleep(next_time);
		live_session_send_carousel(&livesess, ch);
	}

exit:
	live_session_shutdown(&livesess);
	if (update_buffer) gf_free(update_buffer);
	if (sk) gf_sk_del(sk);
	gf_sys_close();
	return e ? 1 : 0;
}


#endif /*!defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)*/

#endif /*defined(GPAC_DISABLE_ISOM) || defined(GPAC_DISABLE_ISOM_WRITE)*/

u32 grab_live_m2ts(const char *grab_m2ts, const char *outName)
{
	char data[0x80000];
	u32 check = 50;
	u64 nb_pck;
	Bool first_run, is_rtp;
	FILE *output;
#ifndef GPAC_DISABLE_STREAMING
	u16 seq_num;
	GF_RTPReorder *ch = NULL;
#endif
	GF_Socket *sock;
	GF_Err e = gf_m2ts_get_socket(grab_m2ts, NULL, 0x80000, &sock);

	if (e) {
		fprintf(stderr, "Cannot open %s: %s\n", grab_m2ts, gf_error_to_string(e));
		return 1;
	}
	output = gf_f64_open(outName, "wb");
	if (!output) {
		fprintf(stderr, "Cannot open %s: check path and rights\n", outName);
		gf_sk_del(sock);
		return 1;
	}

	fprintf(stderr, "Dumping %s stream to %s - press q to abort\n", grab_m2ts, outName);

	first_run = 1;
	is_rtp = 0;
	while (1) {
		u32 size = 0;

		check--;
		if (!check) {
			if (gf_prompt_has_input()) {
				char c = (char) gf_prompt_get_char();
				if (c=='q') break;
			}
			check = 50;
		}

		/*m2ts chunks by chunks*/
		e = gf_sk_receive(sock, data, 0x40000, 0, &size);
		if (!size || e) {
			gf_sleep(1);
			continue;
		}
		if (first_run) {
			first_run = 0;
			/*FIXME: we assume only simple RTP packaging (no CSRC nor extensions)*/
			if ((data[0] != 0x47) && ((data[1] & 0x7F) == 33) ) {
				is_rtp = 1;
#ifndef GPAC_DISABLE_STREAMING
				ch = gf_rtp_reorderer_new(100, 500);
#endif
			}
		}
		/*process chunk*/
		if (is_rtp) {
#ifndef GPAC_DISABLE_STREAMING
			char *pck;
			seq_num = ((data[2] << 8) & 0xFF00) | (data[3] & 0xFF);
			gf_rtp_reorderer_add(ch, (void *) data, size, seq_num);

			pck = (char *) gf_rtp_reorderer_get(ch, &size);
			if (pck) {
				fwrite(pck+12, size-12, 1, output);
				gf_free(pck);
			}
#else
			fwrite(data+12, size-12, 1, output);
#endif
		} else {
			fwrite(data, size, 1, output);
		}
	}
	nb_pck = gf_f64_tell(output);
	nb_pck /= 188;
	fprintf(stderr, "Captured "LLU" TS packets\n", nb_pck );
	fclose(output);
	gf_sk_del(sock);

#ifndef GPAC_DISABLE_STREAMING
	if (ch)
		gf_rtp_reorderer_del(ch);
#endif
	return 0;
}



