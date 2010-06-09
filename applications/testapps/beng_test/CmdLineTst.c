#include <gpac/scene_engine.h>
#include <gpac/constants.h>
#include <gpac/rtp_streamer.h>

typedef struct 
{
	GF_RTPStreamer *rtp;
	u16 ESID;
} BRTP;

/*rap set to 1 initially because of tune in*/
static Bool rap_type = 1;
/*critical AU flag*/
static Bool critical = 0;

GF_Err SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{
	if (calling_object) {
		BRTP *rtpst;
		u32 i=0;
		GF_List *list = (GF_List *) calling_object;
		while ( (rtpst = gf_list_enum(list, &i))) {
			if (rtpst->ESID == ESID) {
				fprintf(stdout, "Received at time %I64d, buffer %d bytes long.\n", ts, size);
				gf_rtp_streamer_send_au_with_sn(rtpst->rtp, data, size, ts, ts, rap_type, critical);
				return GF_OK;
			}
		}
	} else {
		fprintf(stdout, "Received at time %I64d, buffer %d bytes long.\n", ts, size);
	}
	return GF_OK;
}

static setup_rtp_streams(GF_SceneEngine *seng, GF_List *streams, char *ip, u16 port, char *sdp_name)
{
	BRTP *rtpst;
	u32 count = gf_seng_get_stream_count(seng);
	u32 i;
	char *iod64 = gf_seng_get_base64_iod(seng);
	char *sdp = gf_rtp_streamer_format_sdp_header("GPACSceneStreamer", ip, NULL, iod64);
	if (iod64) gf_free(iod64);

	for (i=0; i<count; i++) {
		u16 ESID;
		u32 st, oti, ts;
		char *config;
		u32 config_len;
		gf_seng_get_stream_config(seng, i, &ESID, &config, &config_len, &st, &oti, &ts);
		
		GF_SAFEALLOC(rtpst, BRTP);

		switch (st) {
		case GF_STREAM_OD:
		case GF_STREAM_SCENE:
			rtpst->rtp = gf_rtp_streamer_new_extended(st, oti, ts, ip, port, 1400, 1, NULL, 
								 GP_RTP_PCK_SYSTEMS_CAROUSEL, (char *) config, config_len,
								 96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4);
			break;
		default:
			rtpst->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, 1400, 1, NULL, GP_RTP_PCK_SIGNAL_RAP, config, config_len);
			break;
		}
        if (!rtpst->rtp) {
            gf_free(rtpst);
            continue;
        }
		rtpst->ESID = ESID;
		gf_list_add(streams, rtpst);

		gf_rtp_streamer_append_sdp(rtpst->rtp, ESID, config, config_len, NULL, &sdp);
	}
    if (sdp) {
		FILE *out = fopen(sdp_name, "wt");
        fprintf(out, sdp);
		fclose(out);
	    gf_free(sdp);
    }
}

void shutdown_rtp_streams(GF_List *list)
{
	while (gf_list_count(list)) {
		BRTP *rtpst = gf_list_get(list, 0);
		gf_list_rem(list, 0);
		gf_rtp_streamer_del(rtpst->rtp);
		gf_free(rtpst);
	}
}
void Usage()
{
	fprintf(stdout, 
		"Demo live scene streamer\n"
		"Usage: [options] scene\n"
		"Options:\n"
		"-dst=ip    destination IP - default: NULL\n"
		"-port=num  destination port - default: 7000\n"
		"-sdp=name  ouput SDP file - default: session.sdp\n"
		"-dims      turns on DIMS mode for SVG input - default: off\n"
		"-src=file  source of updates - default: null\n"
		"-rap=time  duration in ms of base carousel - default: 0 (off)\n"
		"            you can specify the RAP period of a single ESID (not in DIMS):\n"
		"                -rap=ESID=X:time\n"
		"\n"
		"Runtime options:\n"
		"q:         quits application\n"
		"u:         inputs some commands to be sent\n"
		"a:         aggregates pending commands in the main scene\n"
		"e:         encodes main scene and stream it\n"
		"p:         dumps current scene\n"
		"\n"
		"GPAC version: " GPAC_FULL_VERSION "\n"
		"");
}

#define BUFFER_SIZE 2048

enum {
    SCENE_UPDATE_NONE = 0,
    SCENE_UPDATE_NORMAL = 1,
    SCENE_UPDATE_CRITICAL = 2,
    SCENE_UPDATE_RAP = 3,
    SCENE_UPDATE_RAP_CRITICAL = 4,
};

u32 seng_receive_update(GF_Socket *update_socket, u8 **update_buffer)
{
    GF_Err e;
    u32 bytes_read;
    char buffer[BUFFER_SIZE];
    Bool keep_receive;
    u32 update_length;
    u32 update_type;
    u32 bytes_received;
    u16 esid;

    update_type = SCENE_UPDATE_NONE;
    keep_receive = 1;
    bytes_received = 0;
    while (keep_receive) {
        memset(buffer, 0, BUFFER_SIZE);
        e = gf_sk_receive(update_socket, buffer, BUFFER_SIZE, 0, &bytes_read);
        switch (e) {
            case GF_IP_NETWORK_EMPTY:
                keep_receive = 0;
                break;
            case GF_OK:
                /* Process data updates */
                if (!bytes_received) {
                    GF_BitStream *bs = gf_bs_new(buffer, bytes_read, GF_BITSTREAM_READ);
                    esid = gf_bs_read_u16(bs);
                    update_type = gf_bs_read_u8(bs);
                    update_length = gf_bs_read_u32(bs);
                    *update_buffer = malloc(update_length+1);
                    if (bytes_read - 7 <= update_length) {
                        bytes_received = bytes_read - 7;
                        memcpy(*update_buffer, buffer+7, bytes_received);
                    }
                    gf_bs_del(bs);
                } else {
                    if (bytes_received+bytes_read <= update_length) {
                        memcpy(*update_buffer+bytes_received, buffer, bytes_read);
                        bytes_received+=bytes_read;
                    }
                }
                if (bytes_received >= update_length) {
                    (*update_buffer)[update_length] = 0;
                    keep_receive = 0;                                                        
                } 
                break;
            default:
                keep_receive = 0;
                fprintf(stderr, "Error with UDP socket : %s\n", gf_error_to_string(e));
                break;
        }
    }
    return update_type;
}

int main(int argc, char **argv)
{
	GF_Err e;
	int i;
	char *filename = NULL;
	char *dst = NULL;
	char *sdp_name = "session.sdp";
	u16 dst_port = 7000;
	u32 load_type=0;
	u16 ESID;
	s32 next_time;
	u64 last_src_modif, mod_time;
	char *src_name = NULL;
    u16 sk_port = 0;
    Bool udp = 0;
    GF_Socket *sk = NULL;

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
		else if (!strnicmp(arg, "-dims", 5)) load_type = GF_SM_LOAD_DIMS;
		else if (!strnicmp(arg, "-src=", 5)) src_name = arg+5;
        else if (!strnicmp(arg, "-udp=", 5)) { sk_port = atoi(arg+5); udp = 1; }
        else if (!strnicmp(arg, "-tcp=", 5)) { sk_port = atoi(arg+5); udp = 0; }
	}
	if (!filename) {
		fprintf(stdout, "Missing filename\n");
		Usage();
		exit(0);
	}

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

	if (dst_port && dst) streams = gf_list_new();

	seng = gf_seng_init(streams, filename, load_type, NULL, (load_type == GF_SM_LOAD_DIMS) ? 1 : 0);
    if (!seng) {
		fprintf(stdout, "Cannot create scene engine\n");
		exit(0);
    }
	if (streams) setup_rtp_streams(seng, streams, dst, dst_port, sdp_name);

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

	gf_seng_encode_context(seng, SampleCallBack);

	run = 1;
	while (run) {
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
				e = gf_seng_encode_from_string(seng, szCom, SampleCallBack);
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
				e = gf_seng_encode_context(seng, SampleCallBack);
				fprintf(stdout, "Context encoded: %s\n", gf_error_to_string(e));
				break;
			}
			e = GF_OK;
		}
		if (src_name) {
			mod_time = gf_file_modification_time(src_name);
			if (mod_time != last_src_modif) {
				fprintf(stdout, "Update file modified - processing\n");
				last_src_modif = mod_time;
				e = gf_seng_encode_from_file(seng, src_name, SampleCallBack);
				if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
				else gf_seng_aggregate_context(seng);
			}

		}
        if (sk) {
            u32 update_type;
            u8 *update_buffer = NULL;
        
            update_type = seng_receive_update(sk, &update_buffer);
            switch (update_type) {
                case SCENE_UPDATE_CRITICAL:
					critical=1;
                case SCENE_UPDATE_NORMAL:
                    /* TODO: add AUSN if SCENE_UPDATE_CRITICAL */
				    e = gf_seng_encode_from_string(seng, update_buffer, SampleCallBack);
                    break;
                case SCENE_UPDATE_RAP_CRITICAL:
					critical=1;
                case SCENE_UPDATE_RAP:
                    /* TODO: add AUSN if SCENE_UPDATE_RAP_CRITICAL */
			        e = gf_seng_aggregate_context(seng);
					rap_type = 1;
			        e = gf_seng_encode_context(seng, SampleCallBack);
					rap_type = 0;
                    break;
                case SCENE_UPDATE_NONE:
                default:
                    break;
            }
			critical=0;
			if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
            if (update_buffer) {
                free(update_buffer);
                update_buffer = NULL;
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
		rap_type = 1;
		gf_seng_encode_context(seng, SampleCallBack);
		rap_type = 0;
		gf_seng_update_rap_time(seng, ESID);
	}

exit:
	if (streams) shutdown_rtp_streams(streams);
	gf_seng_terminate(seng);
	gf_sys_close();
	return e ? 1 : 0;
}


