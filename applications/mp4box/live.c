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


#include "mp4box.h"

#ifndef GPAC_DISABLE_SENG
#include <gpac/scene_engine.h>
#endif
#ifndef GPAC_DISABLE_STREAMING
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




typedef struct
{
	GF_RTPStreamer *rtp;
	Bool manual_rtcp;
	u16 ESID;

	u8 *carousel_data;
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
		RTPChannel *ch = (RTPChannel*)gf_list_get(sess->streams, i);
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


static void live_session_callback(void *calling_object, u16 ESID, u8 *data, u32 size, u64 ts)
{
	LiveSession *livesess = (LiveSession *) calling_object;
	RTPChannel *rtpch;
	u32 i=0;

	while ( (rtpch = (RTPChannel*)gf_list_enum(livesess->streams, &i))) {
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

static Bool live_session_setup(LiveSession *livesess, char *ip, u16 port, u32 path_mtu, u32 ttl, char *ifce_addr, char *sdp_name)
{
	RTPChannel *rtpch;
	u32 count = gf_seng_get_stream_count(livesess->seng);
	u32 i;
	char *iod64 = gf_seng_get_base64_iod(livesess->seng);
	char *sdp = gf_rtp_streamer_format_sdp_header("GPACSceneStreamer", ip, NULL, iod64);
	if (iod64) gf_free(iod64);

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		GF_Descriptor *desc = gf_seng_get_iod(livesess->seng);
		if (desc) gf_odf_desc_del(desc);
	}
#endif
	for (i=0; i<count; i++) {
		u16 ESID;
		u32 st, oti, ts;
		u8 *config = NULL;
		u32 config_len;
		gf_seng_get_stream_config(livesess->seng, i, &ESID, &config, &config_len, &st, &oti, &ts);

		GF_SAFEALLOC(rtpch, RTPChannel);
		if (!rtpch) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Cannot allocate rtp input handler\n"));
			continue;
		}
		rtpch->timescale = ts;
		rtpch->init_time = gf_sys_clock();

		switch (st) {
		case GF_STREAM_OD:
		case GF_STREAM_SCENE:
			rtpch->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, path_mtu, ttl, ifce_addr,
			             GP_RTP_PCK_SYSTEMS_CAROUSEL, config, config_len,
			             96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, GF_FALSE);

			if (rtpch->rtp) {
				gf_rtp_streamer_disable_auto_rtcp(rtpch->rtp);
				rtpch->manual_rtcp = 1;
			}
			break;
		default:
			rtpch->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, path_mtu, ttl, ifce_addr, GP_RTP_PCK_SIGNAL_RAP, config, config_len, 96, 0, 0, GF_FALSE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, GF_FALSE);
			break;
		}
		rtpch->ESID = ESID;
		rtpch->adjust_carousel_time = 1;
		gf_list_add(livesess->streams, rtpch);

		if (!rtpch->rtp)
			return GF_FALSE;

		gf_rtp_streamer_append_sdp(rtpch->rtp, ESID, config, config_len, NULL, &sdp);

		/*fetch initial config of the broadcast*/
		gf_seng_get_stream_carousel_info(livesess->seng, ESID, &rtpch->carousel_period, &rtpch->aggregate_on_stream);
		port += 2;
	}
	if (sdp) {
		FILE *out = gf_fopen(sdp_name, "wt");
		fprintf(out, "%s", sdp);
		gf_fclose(out);
		gf_free(sdp);
	}
	return GF_TRUE;
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
	const char *ifce_addr = NULL;
	char *sdp_name = "session.sdp";
	u16 dst_port = 7000;
	u32 load_type=0;
	u32 check;
	u32 ttl = 1;
	u32 path_mtu = 1450;
	s32 next_time;
	u64 last_src_modif, mod_time, runfor=0, start_time;
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
	gf_sys_init(GF_MemTrackerNone, NULL);

	memset(&livesess, 0, sizeof(LiveSession));

	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_INFO);

	gf_sys_set_args(argc, (const char **) argv);


	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (arg[0] != '-') filename = arg;
		else if (!strnicmp(arg, "-dst=", 5)) dst = arg+5;
		else if (!strnicmp(arg, "-port=", 6)) dst_port = atoi(arg+6);
		else if (!strnicmp(arg, "-sdp=", 5)) sdp_name = arg+5;
		else if (!strnicmp(arg, "-mtu=", 5)) path_mtu = atoi(arg+5);
		else if (!strnicmp(arg, "-ttl=", 5)) ttl = atoi(arg+5);
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
		else if (!stricmp(arg, "-run-for")) {
			runfor = 1 + 1000 * atoi(argv[i+1]);
			i++;
		}
	}
	if (!filename) {
		fprintf(stderr, "Missing filename\n");
		PrintLiveUsage();
		return 1;
	}
	ifce_addr = gf_opts_get_key("core", "ifce");

	if (dst_port && dst) livesess.streams = gf_list_new();

	livesess.seng = gf_seng_init(&livesess, filename, load_type, NULL, (load_type == GF_SM_LOAD_DIMS) ? 1 : 0);
	if (!livesess.seng) {
		fprintf(stderr, "Cannot create scene engine\n");
		return 1;
	}
	if (livesess.streams) {
		Bool res = live_session_setup(&livesess, dst, dst_port, path_mtu, ttl, (char *) ifce_addr, sdp_name);
		if (!res) {
			live_session_shutdown(&livesess);
			if (update_buffer) gf_free(update_buffer);
			if (sk) gf_sk_del(sk);
			gf_sys_close();
			return e ? 1 : 0;
		}
	}

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


#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		aggregate_on_stream = (u16) -1;
		adjust_carousel_time = force_rap = discard_pending = signal_rap = signal_critical = 0;
		aggregate_au = version_inc = 1;
		period = -1;
		ts_delta = 0;
		es_id = 0;

		set_broadcast_params(&livesess, es_id, period, ts_delta, aggregate_on_stream, adjust_carousel_time, force_rap, aggregate_au, discard_pending, signal_rap, signal_critical, version_inc);
	}
#endif

	start_time = gf_sys_clock_high_res();
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
					if (e) fprintf(stderr, "Aggregating context failed: %s\n", gf_error_to_string(e));
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
					if (e) fprintf(stderr, "Aggregating context failed: %s\n", gf_error_to_string(e));

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

				srcf = gf_fopen(src_name, "rt");
				if (!srcf) continue;

				/*checks if we have a broadcast config*/
				if (!fgets(flag_buf, 200, srcf))
					flag_buf[0] = '\0';
				gf_fclose(srcf);

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
			u8 buffer[2049];
			u32 bytes_read;
			u32 update_length;
			u32 bytes_received;


			e = gf_sk_receive(sk, buffer, 2048, &bytes_read);
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
					e = gf_sk_receive(sk, buffer, 2048, &bytes_read);
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

		if (runfor && (gf_sys_clock_high_res() > start_time+runfor))
			break;


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

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
/*		gf_seng_save_context(livesess.seng, NULL);
		gf_seng_aggregate_context
		gf_seng_encode_from_string
		gf_seng_encode_from_file
*/
	}
#endif

exit:
	live_session_shutdown(&livesess);
	if (update_buffer) gf_free(update_buffer);
	if (sk) gf_sk_del(sk);
	gf_sys_close();
	return e ? 1 : 0;
}


#endif /*!defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)*/

#endif /*defined(GPAC_DISABLE_ISOM) || defined(GPAC_DISABLE_ISOM_WRITE)*/

#ifndef GPAC_DISABLE_ATSC

#include <gpac/atsc.h>

static Bool inspect_mode = GF_FALSE;

static u32 nb_services=0;
void atsc_on_evt(void *udta, GF_ATSCEventType evt, u32 evt_param, GF_ATSCEventFileInfo *info)
{
	switch (evt) {
	case GF_ATSC_EVT_SERVICE_FOUND:
		fprintf(stderr, "found service id %d\n", evt_param);
		nb_services++;
		break;
	case GF_ATSC_EVT_SERVICE_SCAN:
		fprintf(stderr, "Done scaning all services\n");
		break;
	case GF_ATSC_EVT_SEG:
		if (inspect_mode) {
			gf_atsc3_dmx_remove_object_by_name( (GF_ATSCDmx *) udta, evt_param, (char *) info->filename, GF_FALSE);
		}
		break;
	case GF_ATSC_EVT_MPD:
		if (inspect_mode) {
			fprintf(stderr, "MPD update found\n");
		}
		break;
	default:
		break;
	}
}

static void atsc_stats(GF_ATSCDmx *atscd, u32 now)
{
	Double rate=0.0;
	u64 st = gf_atsc3_dmx_get_first_packet_time(atscd);
	u64 et = gf_atsc3_dmx_get_last_packet_time(atscd);
	u64 nb_pck = gf_atsc3_dmx_get_nb_packets(atscd);
	u64 nb_bytes = gf_atsc3_dmx_get_recv_bytes(atscd);

	et -= st;
	if (et) {
		rate = (Double)nb_bytes*8;
		rate /= et;
	}
	if (now) {
		fprintf(stderr, "                                                                                                 \r");
		fprintf(stderr, "At %us stats: "LLU" bytes "LLU" packets in "LLU" ms rate %.02f mbps\r", now/1000, nb_bytes, nb_pck, et/1000, rate);
	} else {
		fprintf(stderr, "\nFinal stats: "LLU" bytes "LLU" packets in "LLU" ms rate %.02f mbps\n", nb_bytes, nb_pck, et/1000, rate);
	}
}

u32 grab_atsc3_session(const char *dir, s32 serviceID, s32 atsc_max_segs, u32 stats_rate, u32 debug_tsi)
{
	GF_ATSCDmx *atscd;
	Bool run = GF_TRUE;
	GF_SystemRTInfo rti;
	u32 nb_stats=1;
	u32 start_time = gf_sys_clock();

	const char *ifce = gf_opts_get_key("core", "ifce");
	
	gf_sys_get_rti(0, &rti, 0);

	atscd = gf_atsc3_dmx_new(ifce, dir, 0);
	if (!atscd) {
		fprintf(stderr, "Failed to create ATSC3 demuxer\n");
		return 1;
	}
	gf_atsc3_set_callback(atscd, atsc_on_evt, atscd);
	gf_atsc3_tune_in(atscd, (u32) serviceID, GF_FALSE);
	if (atsc_max_segs>=0)
		gf_atsc3_set_max_objects_store(atscd, (u32) atsc_max_segs);

	if (debug_tsi) {
		fprintf(stderr, "Filtering objects from TSI %d only\n", debug_tsi);
		gf_atsc3_dmx_debug_tsi(atscd, debug_tsi);
	}

	if (!dir) {
		fprintf(stderr, "No output dir, ATSC3 demux inspect mode only\n");
		inspect_mode = GF_TRUE;
	}
	fprintf(stderr, "Starting ATSC3 demux, press 'q' to stop, 'm' for memory/cpu info\n");

	while (atscd && run) {
		Bool is_empty = GF_TRUE;
		GF_Err e = gf_atsc3_dmx_process(atscd);
		if (e != GF_IP_NETWORK_EMPTY) is_empty = GF_FALSE;

		if (is_empty) {
			u32 /*st, */now = gf_sys_clock()- start_time;
			gf_sleep(1);

			if (gf_prompt_has_input()) {
				u8 c = gf_prompt_get_char();
				switch (c) {
				case 'q':
					run = GF_FALSE;
					break;
				case 'm':
					gf_sys_get_rti(100, &rti, 0);
					fprintf(stderr, "CPU %02d - memory "LLU"\n", rti.process_cpu_usage,  rti.gpac_memory);
					break;
				}
			}

			if (!nb_services && (now >=10000)) {
				fprintf(stderr, "\nNo ATSC3 service found in %u ms, aborting\n", now);
				run = GF_FALSE;
			}
			if (stats_rate) {
				/*st = gf_sys_clock();*/
				if (now >= nb_stats*1000*stats_rate) {
					nb_stats+=1;
					atsc_stats(atscd, now);
				}
			}
		}
	}
	atsc_stats(atscd, 0);
	gf_atsc3_dmx_del(atscd);
	fprintf(stderr, "\n");
	return 0;
}

#endif /* GPAC_DISABLE_ATSC */
