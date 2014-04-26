#include "ts_muxer.h"
#include <gpac/mpegts.h>

struct avr_ts_muxer {
	GF_M2TS_Mux *muxer;
	GF_ESIPacket videoCurrentTSPacket;
	GF_ESIPacket audioCurrentTSPacket;
	GF_ESInterface * video, * audio;
	u64 frameTimeSentOverTS;
	GF_Socket * ts_output_udp_sk;
};

/*!
 * Sends the TS mux to socket
 * \param avr The AVRedirect structure pointer
 */
static GF_Err sendTSMux(GF_AbstractTSMuxer * ts)
{
	u32 status;
	const char * pkt;
	GF_Err e;
	u32 padding, data;
	padding = data = 0;
	while ( (NULL!= ( pkt = gf_m2ts_mux_process ( ts->muxer, &status ))))
	{
		switch (status) {
		case GF_M2TS_STATE_IDLE:
			break;
		case GF_M2TS_STATE_DATA:
			data+=188;
			break;
		case GF_M2TS_STATE_PADDING:
			padding+=188;
			break;
		default:
			break;
		}
		if (ts->ts_output_udp_sk) {
			e = gf_sk_send ( ts->ts_output_udp_sk, pkt, 188);
			if ( e )
			{
				GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Unable to send TS data : %s\n", gf_error_to_string(e)) );
				return e;
			}
		}
	}
	if (data || padding)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[AVRedirect] : Sent TS data=%u/padding=%u\n", data, padding));
	return GF_OK;
}

//Bool *add_video_stream(GF_AbstractTSMuxer * ts, int codec_id)

Bool ts_encode_audio_frame(GF_AbstractTSMuxer * ts, AVFrame * encodedFrame, uint8_t * data, int encoded) {
	ts->audioCurrentTSPacket.data = data;
	ts->audioCurrentTSPacket.data_len = encoded;
	ts->audioCurrentTSPacket.flags = GF_ESI_DATA_AU_START|GF_ESI_DATA_AU_END | GF_ESI_DATA_HAS_CTS | GF_ESI_DATA_HAS_DTS;
	//avr->audioCurrentTSPacket.cts = avr->audioCurrentTSPacket.dts = myTime;
	ts->videoCurrentTSPacket.dts = ts->videoCurrentTSPacket.cts = gf_m2ts_get_sys_clock(ts->muxer);
	ts->audio->output_ctrl(ts->audio, GF_ESI_OUTPUT_DATA_DISPATCH, &(ts->audioCurrentTSPacket));
	return 0;
}

static GF_Err void_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	GF_AbstractTSMuxer * ts = (GF_AbstractTSMuxer *)  ifce->input_udta;
	if (!ts)
		return GF_BAD_PARAM;
	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
		break;
	case GF_ESI_INPUT_DESTROY:
		break;
	/*    case GF_ESI_INPUT_DATA_PULL:
	        gf_mx_p(ts->encodingMutex);
	        if (ts->frameTimeEncoded > ts->frameTimeSentOverTS) {
	            //fprintf(stderr, "Data PULL, avr=%p, avr->video=%p, encoded="LLU", sent over TS="LLU"\n", avr, &avr->video, avr->frameTimeEncoded, avr->frameTimeSentOverTS);
	            ts->video->output_ctrl( ts->video, GF_ESI_OUTPUT_DATA_DISPATCH, &(ts->videoCurrentTSPacket));
	            ts->frameTimeSentOverTS = ts->frameTime;
	        } else {
	            //fprintf(stderr, "Data PULL IGNORED : encoded = "LLU", sent on TS="LLU"\n", avr->frameTimeEncoded, avr->frameTimeSentOverTS);
	        }
	        gf_mx_v(avr->encodingMutex);
	        break;*/
	case GF_ESI_INPUT_DATA_RELEASE:
		break;
	default:
		fprintf(stderr, "Asking unknown : %u\n", act_type);
	}
	return GF_OK;
}

Bool ts_encode_video_frame(GF_AbstractTSMuxer * ts, AVFrame * encodedFrame, uint8_t * data, int encoded, AVCodecContext * videoCodecContext) {
	if ( ts->muxer )
	{	//  currentFrameTimeProcessed / 1000;  //
		ts->videoCurrentTSPacket.dts = videoCodecContext->coded_frame->coded_picture_number;
		ts->videoCurrentTSPacket.cts = videoCodecContext->coded_frame->display_picture_number;
		//avr->videoCurrentTSPacket.dts = avr->videoCurrentTSPacket.cts = sysclock;
		//ts->videoCurrentTSPacket.dts = ts->videoCurrentTSPacket.cts = currentFrameTimeProcessed;
		ts->videoCurrentTSPacket.dts = ts->videoCurrentTSPacket.cts = gf_m2ts_get_sys_clock(ts->muxer);
		ts->videoCurrentTSPacket.data = data;
		ts->videoCurrentTSPacket.data_len = encoded;
		ts->videoCurrentTSPacket.flags = GF_ESI_DATA_HAS_CTS | GF_ESI_DATA_HAS_DTS;
		//if (ts_packets_sent == 0) {
		ts->videoCurrentTSPacket.flags|=GF_ESI_DATA_AU_START|GF_ESI_DATA_AU_END ;
		//}
		void_input_ctrl(ts->video, GF_ESI_INPUT_DATA_PULL, NULL);
		sendTSMux(ts);
	}
	return 0;
}

GF_AbstractTSMuxer * ts_amux_new(GF_AVRedirect * avr, u32 videoBitrateInBitsPerSec, u32 audioBitRateInBitsPerSec, GF_Socket * ts_output_udp_sk) {
	GF_AbstractTSMuxer * ts = malloc(sizeof(GF_AbstractTSMuxer));
	ts->muxer = gf_m2ts_mux_new ( videoBitrateInBitsPerSec * audioBitRateInBitsPerSec + 1000000, 0, 1 );
	ts->ts_output_udp_sk = ts_output_udp_sk;
	{
		//u32 cur_pid = 100;	/*PIDs start from 100*/
		GF_M2TS_Mux_Program *program = gf_m2ts_mux_program_add ( ts->muxer, 1, 100, 0, 0 );

		ts->video = gf_malloc( sizeof( GF_ESInterface));
		memset( ts->video, 0, sizeof( GF_ESInterface));
		//audio.stream_id = 101;
		//gf_m2ts_program_stream_add ( program, &audifero, 101, 1 );
		ts->video->stream_id = 101;
		ts->video->stream_type = GF_STREAM_VISUAL;
		ts->video->bit_rate = 410000;
		/* ms resolution */
		ts->video->timescale = 1000;

		ts->video->object_type_indication = GPAC_OTI_VIDEO_MPEG2_SIMPLE;
		ts->video->input_udta = ts;
#ifdef TS_MUX_MODE_PUT
		ts->video->caps = GF_ESI_SIGNAL_DTS;
#else
		avr->video->input_ctrl = void_input_ctrl;
		avr->video->caps = GF_ESI_AU_PULL_CAP | GF_ESI_SIGNAL_DTS;
#endif /* TS_MUX_MODE_PUT */


		ts->audio = gf_malloc( sizeof( GF_ESInterface));
		memset( ts->audio, 0, sizeof( GF_ESInterface));
		ts->audio->stream_id = 102;
		ts->audio->stream_type = GF_STREAM_AUDIO;
		ts->audio->bit_rate = audioBitRateInBitsPerSec;
		/* ms resolution */
		ts->audio->timescale = 1000;

		ts->audio->object_type_indication = GPAC_OTI_AUDIO_MPEG2_PART3;
		ts->audio->input_udta = ts;
#ifdef TS_MUX_MODE_PUT
		ts->audio->caps = 0;
#else
		ts->audio->input_ctrl = void_input_ctrl;
		ts->audio->caps = GF_ESI_AU_PULL_CAP;
#endif /* TS_MUX_MODE_PUT */

		gf_m2ts_program_stream_add ( program, ts->video, 101, 0 );
		gf_m2ts_program_stream_add ( program, ts->audio, 102, 1 );

		assert(program->streams->mpeg2_stream_type == GF_M2TS_VIDEO_MPEG2 ||
		       program->streams->mpeg2_stream_type == GF_M2TS_VIDEO_DCII);
	}
	gf_m2ts_mux_update_config ( ts->muxer, 1 );
	return ts;
}

void ts_amux_del(GF_AbstractTSMuxer * ts) {
	if (!ts)
		return;
	if ( ts->ts_output_udp_sk )
	{
		gf_sk_del ( ts->ts_output_udp_sk );
		ts->ts_output_udp_sk = NULL;
	}


}
