/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Telecom ParisTech 2010-
 *					All rights reserved
 *
 *			Authors: Jean Le Feuvre
 *
 *  This file is part of GPAC / mp4box application
 *
 *  GPAC is gf_free software; you can redistribute it and/or modify
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


#include <gpac/filestreamer.h>
#include <gpac/isomedia.h>
#include <gpac/scene_engine.h>
#include <gpac/rtp_streamer.h>

void PrintStreamerUsage()
{
	fprintf(stdout, "File Streamer Options\n"
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

int stream_file_rtp(int argc, char **argv) 
{
	GF_ISOMRTPStreamer *file_streamer;
	char *sdp_file = "session.sdp";
	char *ip_dest = "127.0.0.1";
	char *ifce_addr = NULL;
	char *inName = NULL;
	u16 port = 7000;
	u32 ttl = 1;
	Bool loop = 1, stream_rtp = 0;
	Bool force_mpeg4 = 0;
	u32 path_mtu = 1450;
	u32 i;

	for (i = 1; i < (u32) argc ; i++) {
		char *arg = argv[i];

		if (arg[0] != '-') {
			if (inName) { fprintf(stdout, "Error - 2 input names specified, please check usage\n"); return 1; }
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
	}

	if (!gf_isom_probe_file(inName)) {
		fprintf(stdout, "File %s is not a valid ISO Media file and cannot be streamed\n", inName);
		return 1;
	}

	gf_sys_init(0);

	gf_log_set_tools(GF_LOG_RTP);
	gf_log_set_level(GF_LOG_WARNING);	//set to debug to have packet list

	file_streamer = gf_isom_streamer_new(inName, ip_dest, port, loop, force_mpeg4, path_mtu, ttl, ifce_addr);
	if (!file_streamer) {
		fprintf(stdout, "Cannot create file streamer\n");
	} else {
		u32 check = 50;
		fprintf(stdout, "Starting streaming %s to %s:%d\n", inName, ip_dest, port);
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
	gf_sys_close();
	return 0;
}


void PrintLiveUsage()
{
	fprintf(stdout, 

		"Live scene encoder options:\n"
		"-dst=IP    destination IP - default: NULL\n"
		"-port=PORT destination port - default: 7000\n"
		"-mtu=MTU   path MTU for RTP packets. Default is 1450 bytes\n"
		"-ifce=IFCE IP address of the physical interface to use. Default: NULL(ANY)\n"
		"-ttl=TTL   time to live for multicast packets. Default: 1\n"
		"-sdp=Name  ouput SDP file - default: session.sdp\n"
		"\n"
		"-dims      turns on DIMS mode for SVG input - default: off\n"
		"-src=file  source of updates - default: null\n"
		"-rap=time  duration in ms of base carousel - default: 0 (off)\n"
		"            you can specify the RAP period of a single ESID (not in DIMS):\n"
		"                -rap=ESID=X:time\n"
		"\n"
		"Runtime options:\n"
		"q:         quits\n"
		"u:         inputs some commands to be sent\n"
		"a:         aggregates pending commands in the main scene\n"
		"e:         encodes main scene and stream it\n"
		"p:         dumps current scene\n"
		"\n"
		"GPAC version: " GPAC_FULL_VERSION "\n"
		"");
}
typedef struct 
{
	GF_RTPStreamer *rtp;
	u16 ESID;
} RTPChannel;


static void live_session_callback(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{
	if (calling_object) {
		RTPChannel *rtpch;
		u32 i=0;
		GF_List *list = (GF_List *) calling_object;
		while ( (rtpch = gf_list_enum(list, &i))) {
			if (rtpch->ESID == ESID) {
				fprintf(stdout, "Received at time %I64d, buffer %d bytes long.\n", ts, size);
				gf_rtp_streamer_send_au(rtpch->rtp, data, size, ts, ts, 1);
				return;
			}
		}
	} else {
		fprintf(stdout, "Received at time %I64d, buffer %d bytes long.\n", ts, size);
	}
}

static void live_session_setup(GF_SceneEngine *seng, GF_List *streams, char *ip, u16 port, u32 path_mtu, u32 ttl, char *ifce_addr, char *sdp_name)
{
	RTPChannel *rtpch;
	u32 count = gf_seng_get_stream_count(seng);
	u32 i;
	char *iod64 = gf_seng_get_base64_iod(seng);
	char *sdp = gf_rtp_streamer_format_sdp_header("GPACSceneStreamer", ip, NULL, iod64);
	if (iod64) gf_free(iod64);

	for (i=0; i<count; i++) {
		u16 ESID;
		u32 st, oti, ts;
		const char *config;
		u32 config_len;
		gf_seng_get_stream_config(seng, i, &ESID, &config, &config_len, &st, &oti, &ts);
		
		GF_SAFEALLOC(rtpch, RTPChannel);
		rtpch->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, path_mtu, ttl, ifce_addr, GP_RTP_PCK_SIGNAL_RAP, (char *) config, config_len);
		rtpch->ESID = ESID;
		gf_list_add(streams, rtpch);

		gf_rtp_streamer_append_sdp(rtpch->rtp, ESID, (char *) config, config_len, NULL, &sdp);
	}
    if (sdp) {
		FILE *out = fopen(sdp_name, "wt");
        fprintf(out, sdp);
		fclose(out);
	    gf_free(sdp);
    }
}

void live_session_shutdown(GF_List *list)
{
	while (gf_list_count(list)) {
		RTPChannel *rtpch = gf_list_get(list, 0);
		gf_list_rem(list, 0);
		gf_rtp_streamer_del(rtpch->rtp);
		gf_free(rtpch);
	}
}

int live_session(int argc, char **argv)
{
	GF_Err e;
	int i;
	char *filename = NULL;
	char *dst = NULL;
	char *ifce_addr = NULL;
	char *sdp_name = "session.sdp";
	u16 dst_port = 7000;
	u32 load_type=0;
	u32 check;
	u32 ttl = 1;
	u32 path_mtu = 1450;
	u16 ESID;
	s32 next_time;
	u64 last_src_modif, mod_time;
	char *src_name = NULL;
	Bool run, has_carousel;

	GF_List *streams = NULL;
	GF_SceneEngine *seng = NULL;

	gf_sys_init(0);

	gf_log_set_level(GF_LOG_INFO);
	gf_log_set_tools(0xFFFFFFFF);

	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (arg[0] != '-') filename = arg;
		else if (!strnicmp(arg, "-dst=", 5)) dst = arg+5;
		else if (!strnicmp(arg, "-port=", 6)) dst_port = atoi(arg+6);
		else if (!strnicmp(arg, "-sdp=", 5)) sdp_name = arg+5;
		else if (!strnicmp(arg, "-mtu=", 5)) path_mtu = atoi(arg+5);
		else if (!strnicmp(arg, "-ttl=", 5)) ttl = atoi(arg+5);
		else if (!strnicmp(arg, "-ifce=", 6)) ifce_addr = arg+6;

		else if (!strnicmp(arg, "-dims", 5)) load_type = GF_SM_LOAD_DIMS;
		else if (!strnicmp(arg, "-src=", 5)) src_name = arg+5;
	}
	if (!filename) {
		fprintf(stdout, "Missing filename\n");
		PrintLiveUsage();
		return 1;
	}

	if (dst_port && dst) streams = gf_list_new();

	seng = gf_seng_init(streams, filename, load_type, NULL);
    if (!seng) {
		fprintf(stdout, "Cannot create scene engine\n");
		return 1;
    }
	if (streams) live_session_setup(seng, streams, dst, dst_port, path_mtu, ttl, ifce_addr, sdp_name);

	has_carousel = 0;
	last_src_modif = 0;

	for (i=0; i<argc; i++) {
		char *arg = argv[i];
		if (!strnicmp(arg, "-rap=", 5)) {
			u32 period, id;
			period = id = 0;
			if (strchr(arg, ':')) {
				sscanf(arg, "-rap=ESID=%d:%d", &id, &period);
				e = gf_seng_enable_aggregation(seng, id, 1);
				if (e) {
					fprintf(stdout, "Cannot enable aggregation on stream %d: %s\n", id, gf_error_to_string(e));
					goto exit;
				}
			} else {
				sscanf(arg, "-rap=%d", &period);
			}
			e = gf_seng_set_carousel_time(seng, id, period);
			if (e) {
				fprintf(stdout, "Cannot set carousel time on stream %d to %d: %s\n", id, period, gf_error_to_string(e));
				goto exit;
			}
			has_carousel = 1;
		}
	}

	gf_seng_encode_context(seng, live_session_callback);

	check = 100;
	run = 1;
	while (run) {
		check--;
		if (!check) {
			check = 100;
			if (gf_prompt_has_input()) {
				char c = gf_prompt_get_char();
				switch (c) {
				case 'q':
					run=0;
					break;
				case 'u':
				{
					GF_Err e;
					char szCom[8192];
					fprintf(stdout, "Enter command to send:\n");
					fflush(stdin);
					szCom[0] = 0;
					scanf("%[^\t\n]", szCom);
					e = gf_seng_encode_from_string(seng, szCom, live_session_callback);
					if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
				}
					break;
				case 'p':
				{
					char rad[GF_MAX_PATH];
					fprintf(stdout, "Enter output file name - \"std\" for stdout: ");
					scanf("%s", rad);
					e = gf_seng_save_context(seng, !strcmp(rad, "std") ? NULL : rad);
					fprintf(stdout, "Dump done (%s)\n", gf_error_to_string(e));
				}
					break;
				case 'a':
					e = gf_seng_aggregate_context(seng);
					fprintf(stdout, "Context aggreagated: %s\n", gf_error_to_string(e));
					break;
				case 'e':
					e = gf_seng_encode_context(seng, live_session_callback);
					fprintf(stdout, "Context encoded: %s\n", gf_error_to_string(e));
					break;
				}
				e = GF_OK;
			}
		}
		if (src_name) {
			mod_time = gf_file_modification_time(src_name);
			if (mod_time != last_src_modif) {
				fprintf(stdout, "Update file modified - processing\n");
				last_src_modif = mod_time;
				e = gf_seng_encode_from_file(seng, src_name, live_session_callback);
				if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
				else gf_seng_aggregate_context(seng);
			}

		}
		if (!has_carousel) {
			gf_sleep(10);
			continue;
		}
		next_time = gf_seng_next_rap_time(seng, &ESID);
		if (next_time<0) {
			gf_sleep(10);
			continue;
		}
		if (next_time > 30) {
			gf_sleep(0);
			continue;
		}
		gf_sleep(next_time);
		gf_seng_aggregate_context(seng);
		gf_seng_encode_context(seng, live_session_callback);
		gf_seng_update_rap_time(seng, ESID);
	}

exit:
	if (streams) live_session_shutdown(streams);
	gf_seng_terminate(seng);
	gf_sys_close();
	return e ? 1 : 0;
}


