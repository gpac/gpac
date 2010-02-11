#include <gpac/bifsengine.h>
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

static setup_rtp_streams(GF_BifsEngine *beng, GF_List *streams, char *ip, u16 port)
{
	BRTP *rtpst;
	u32 count = gf_beng_get_stream_count(beng);
	u32 i;
	char *sdp = NULL;
	
	for (i=0; i<count; i++) {
		u16 ESID;
		u32 st, oti, ts;
		char *config;
		u32 config_len;
		gf_beng_get_stream_config(beng, i, &ESID, &config, &config_len, &st, &oti, &ts);
		
		GF_SAFEALLOC(rtpst, BRTP);
		rtpst->rtp = gf_rtp_streamer_new(st, oti, ts, ip, port, 1400, 1, NULL, GP_RTP_PCK_SIGNAL_RAP, config, config_len);
		rtpst->ESID = ESID;
		gf_list_add(streams, rtpst);

		gf_rtp_streamer_append_sdp(rtpst->rtp, ESID, config, config_len, NULL, &sdp);
	}
    if (sdp) {
        fprintf(stdout, sdp);
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
	int i;
	const char * dst = "224.0.0.1";
	const u16 dst_port = 7000;
	const char *update1 = NULL;
	const char *update2 = NULL;

	GF_List *streams = NULL;
	GF_BifsEngine *codec1 = NULL;
	GF_BifsEngine *codec2 = NULL;

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

		codec1 = gf_beng_init_from_string(streams, scene, GF_SM_LOAD_DIMS, 0, 0, 1);
		if (streams) setup_rtp_streams(codec1, streams, dst, dst_port);
		gf_beng_encode_context(codec1, SampleCallBack);

        gf_beng_encode_from_string(codec1, update, SampleCallBack);

		gf_beng_aggregate_context(codec1);

        gf_beng_encode_context(codec1, SampleCallBack);

		if (streams) shutdown_rtp_streams(streams);
		gf_beng_save_context(codec1, "scene.svg");
		gf_beng_terminate(codec1);
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

		codec1 = gf_beng_init(streams, argv[1], 0);
		if (streams) setup_rtp_streams(codec1, streams, dst, dst_port);

		gf_beng_encode_context(codec1, SampleCallBack);
		gf_beng_save_context(codec1, "initial_context.bt");
		gf_beng_encode_from_string(codec1, (char*)update1, SampleCallBack);
		gf_beng_save_context(codec1, "non_aggregated_context1.xmt");
		gf_beng_encode_from_string(codec1, (char*)update2, SampleCallBack);
		gf_beng_save_context(codec1, "non_aggregated_context2.xmt");
		gf_beng_mark_for_aggregation(codec1, 2); /*mark ESID 2 for aggregation*/
		gf_beng_aggregate_context(codec1);
		gf_beng_save_context(codec1, "aggregated_context2.xmt");

		if (streams) shutdown_rtp_streams(streams);
		gf_beng_terminate(codec1);
	} else if (1) {
		char scene[] = "OrderedGroup {children [Background2D {backColor 1 1 1}Shape {appearance Appearance {material DEF M Material2D {emissiveColor 0 0 1 filled TRUE } } geometry Rectangle { size 100 75 } } ] }";
		char update[] = "\n AT \n 500 \n { \n REPLACE \n M.emissiveColor BY 1 0 0 \n REPLACE \n M.filled BY FALSE} \n";

		codec1 = gf_beng_init_from_string(streams, scene, 0, 200, 200, 1);
		if (streams) setup_rtp_streams(codec1, streams, dst, dst_port);

		gf_beng_encode_context(codec1, SampleCallBack);
		gf_beng_save_context(codec1, "initial_context.mp4");
		gf_beng_encode_from_string(codec1, (char *) update, SampleCallBack);
		gf_beng_save_context(codec1, "non_aggregated_context.mp4");
		gf_beng_aggregate_context(codec1);
		gf_beng_save_context(codec1, "aggregated_context.mp4");

		if (streams) shutdown_rtp_streams(streams);
		gf_beng_terminate(codec1);
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

			codec2 = gf_beng_init(NULL, in_context, 0);

			sprintf(timed_update, "AT %i { %s }", 1000 + i, update);
			
			gf_beng_encode_from_string(codec2, timed_update, SampleCallBack);

			sprintf(mp4_out_na_context, "na_%s_%i.mp4", context_rootname, i+1);
			sprintf(bt_out_na_context, "na_%s_%i.bt", context_rootname, i+1);
			sprintf(mp4_out_agg_context, "agg_%s_%i.mp4", context_rootname, i+1);
			sprintf(bt_out_agg_context, "agg_%s_%i.bt", context_rootname, i+1);

			gf_beng_save_context(codec2, mp4_out_na_context);
			gf_beng_save_context(codec2, bt_out_na_context);
			gf_beng_aggregate_context(codec2);
			gf_beng_save_context(codec2, mp4_out_agg_context);
			gf_beng_save_context(codec2, bt_out_agg_context);
		
			gf_beng_terminate(codec2);
		}
		fprintf(stdout, "Done.\n");
	}

	gf_sys_close();

	return 0;
}
