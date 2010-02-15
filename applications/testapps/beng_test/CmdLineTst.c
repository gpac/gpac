#include <gpac/scene_engine.h>
#include <gpac/rtp_streamer.h>

typedef struct 
{
	GF_RTPStreamer *rtp;
	u16 ESID;
} BRTP;

GF_Err SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{
	if (calling_object) {
		BRTP *rtpst;
		u32 i=0;
		GF_List *list = (GF_List *) calling_object;
		while ( (rtpst = gf_list_enum(list, &i))) {
			if (rtpst->ESID == ESID) {
				fprintf(stdout, "Received at time %I64d, buffer %d bytes long.\n", ts, size);
				gf_rtp_streamer_send_au(rtpst->rtp, data, size, ts, ts, 1);
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
	if (iod64) free(iod64);

	for (i=0; i<count; i++) {
		u16 ESID;
		u32 st, oti, ts;
		char *config;
		u32 config_len;
		gf_seng_get_stream_config(seng, i, &ESID, &config, &config_len, &st, &oti, &ts);
		
		GF_SAFEALLOC(rtpst, BRTP);
		rtpst->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, 1400, 1, NULL, GP_RTP_PCK_SIGNAL_RAP, config, config_len);
		rtpst->ESID = ESID;
		gf_list_add(streams, rtpst);

		gf_rtp_streamer_append_sdp(rtpst->rtp, ESID, config, config_len, NULL, &sdp);
	}
    if (sdp) {
		FILE *out = fopen(sdp_name, "wt");
        fprintf(out, sdp);
		fclose(out);
	    free(sdp);
    }
}

void shutdown_rtp_streams(GF_List *list)
{
	while (gf_list_count(list)) {
		BRTP *rtpst = gf_list_get(list, 0);
		gf_list_rem(list, 0);
		gf_rtp_streamer_del(rtpst->rtp);
		free(rtpst);
	}
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
	Bool run, has_carousel;

	GF_List *streams = NULL;
	GF_SceneEngine *seng = NULL;

	gf_sys_init();

	gf_log_set_level(GF_LOG_INFO);
	gf_log_set_tools(0xFFFFFFFF);

	for (i=0; i<argc; i++) {
		char *arg = argv[i];
		if (arg[0] != '-') filename = arg;
		else if (!strnicmp(arg, "-dst=", 5)) dst = arg+5;
		else if (!strnicmp(arg, "-port=", 6)) dst_port = atoi(arg+6);
		else if (!strnicmp(arg, "-sdp=", 5)) sdp_name = arg+5;
		else if (!strnicmp(arg, "-dims", 5)) load_type = GF_SM_LOAD_DIMS;
		else if (!strnicmp(arg, "-src=", 5)) src_name = arg+5;
	}
	if (!filename) {
		fprintf(stdout, "Missing filename\n");
		exit(0);
	}

	if (dst_port && dst) streams = gf_list_new();

	seng = gf_seng_init(streams, filename, load_type);
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
			case 'c':
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
		gf_seng_encode_context(seng, SampleCallBack);
		gf_seng_update_rap_time(seng, ESID);
	}

exit:
	if (streams) shutdown_rtp_streams(streams);
	gf_seng_terminate(seng);
	gf_sys_close();
	return e ? 1 : 0;
}


int main2(int argc, char **argv)
{
	int i;
	const char * dst = "224.0.0.1";
	const u16 dst_port = 7000;
	const char *update1 = NULL;
	const char *update2 = NULL;

	GF_List *streams = NULL;
	GF_SceneEngine *codec1 = NULL;
	GF_SceneEngine *codec2 = NULL;

	gf_sys_init();

	gf_log_set_level(GF_LOG_INFO);
	gf_log_set_tools(0xFFFFFFFF);


	if (dst_port && dst) streams = gf_list_new();

    if (1) {
        char *scene = "<svg width='100%' height='100%' viewbox='0 0 640 480' \
                  xmlns='http://www.w3.org/2000/svg' version='1.2' baseProfile='tiny'> \
                    <rect id='r' x='20' y='20' width='20' height='20'/>\
                    <rect x='50' y='20' width='30' height='15'/>\
                    <rect x='20' y='50' width='20' height='20'/>\
                    <rect x='50' y='50' width='20' height='40'/>\
                    </svg>";
        char *update = "<Replace ref='r' attributeName='x' value='100'/>";

		codec1 = gf_seng_init_from_string(streams, scene, GF_SM_LOAD_DIMS, 0, 0, 1);
		if (streams) setup_rtp_streams(codec1, streams, dst, dst_port, "session.sdp");
		gf_seng_encode_context(codec1, SampleCallBack);

        gf_seng_encode_from_string(codec1, update, SampleCallBack);

		gf_seng_aggregate_context(codec1);

        gf_seng_encode_context(codec1, SampleCallBack);

		if (streams) shutdown_rtp_streams(streams);
		gf_seng_save_context(codec1, "scene.svg");
		gf_seng_terminate(codec1);
    } else if (1) {
		/*these default update are related to rect_aggregate.bt*/
		update1 = "AT 1000 IN 2 {\
						INSERT AT OG.children[0] DEF TR2 Transform2D {\
						translation -100 -50\
						children [\
						DEF S Shape {\
						appearance Appearance {\
						material Material2D {\
						emissiveColor 1 0 0\
						filled TRUE\
						} }\
						geometry DEF REC Rectangle {\
						size 50 100\
						} } ] } }";
		
		update2 = "AT 2000 IN 3 {\
						REPLACE REC.size BY 100 100\
						}";

		codec1 = gf_seng_init(streams, argv[1], 0);
		if (streams) setup_rtp_streams(codec1, streams, dst, dst_port, "session.sdp");

		gf_seng_encode_context(codec1, SampleCallBack);
		gf_seng_save_context(codec1, "initial_context.bt");
		gf_seng_encode_from_string(codec1, (char*)update1, SampleCallBack);
		gf_seng_save_context(codec1, "non_aggregated_context1.xmt");
		gf_seng_encode_from_string(codec1, (char*)update2, SampleCallBack);
		gf_seng_save_context(codec1, "non_aggregated_context2.xmt");
		gf_seng_enable_aggregation(codec1, 2, 1); /*mark ESID 2 for aggregation*/
		gf_seng_aggregate_context(codec1);
		gf_seng_save_context(codec1, "aggregated_context2.xmt");

		if (streams) shutdown_rtp_streams(streams);
		gf_seng_terminate(codec1);
	} else if (1) {
		char scene[] = "OrderedGroup {children [Background2D {backColor 1 1 1}Shape {appearance Appearance {material DEF M Material2D {emissiveColor 0 0 1 filled TRUE } } geometry Rectangle { size 100 75 } } ] }";
		char update[] = "\n AT \n 500 \n { \n REPLACE \n M.emissiveColor BY 1 0 0 \n REPLACE \n M.filled BY FALSE} \n";

		codec1 = gf_seng_init_from_string(streams, scene, 0, 200, 200, 1);
		if (streams) setup_rtp_streams(codec1, streams, dst, dst_port, "session.sdp");

		gf_seng_encode_context(codec1, SampleCallBack);
		gf_seng_save_context(codec1, "initial_context.mp4");
		gf_seng_encode_from_string(codec1, (char *) update, SampleCallBack);
		gf_seng_save_context(codec1, "non_aggregated_context.mp4");
		gf_seng_aggregate_context(codec1);
		gf_seng_save_context(codec1, "aggregated_context.mp4");

		if (streams) shutdown_rtp_streams(streams);
		gf_seng_terminate(codec1);
	} else {

		for (i = 0; i <10; i++) {
			char context_rootname[] = "rect_context";
			char in_context[100], 
				 bt_out_na_context[100], bt_out_agg_context[100],
				 mp4_out_na_context[100], mp4_out_agg_context[100];
			char update[1000];// = "REPLACE M.emissiveColor BY 1 1 0";
			char timed_update[1000];

			sprintf(update, "REPLACE M.emissiveColor BY %f 0 0", i/10.0f);

			if (i != 0) {
				sprintf(in_context, "na_%s_%i.bt", context_rootname, i);
			} else {
				strcpy(in_context, "rect.bt");
			}

			codec2 = gf_seng_init(NULL, in_context, 0);

			sprintf(timed_update, "AT %i { %s }", 1000 + i, update);
			
			gf_seng_encode_from_string(codec2, timed_update, SampleCallBack);

			sprintf(mp4_out_na_context, "na_%s_%i.mp4", context_rootname, i+1);
			sprintf(bt_out_na_context, "na_%s_%i.bt", context_rootname, i+1);
			sprintf(mp4_out_agg_context, "agg_%s_%i.mp4", context_rootname, i+1);
			sprintf(bt_out_agg_context, "agg_%s_%i.bt", context_rootname, i+1);

			gf_seng_save_context(codec2, mp4_out_na_context);
			gf_seng_save_context(codec2, bt_out_na_context);
			gf_seng_aggregate_context(codec2);
			gf_seng_save_context(codec2, mp4_out_agg_context);
			gf_seng_save_context(codec2, bt_out_agg_context);
		
			gf_seng_terminate(codec2);
		}
		fprintf(stdout, "Done.\n");
	}

	gf_sys_close();

	return 0;
}
