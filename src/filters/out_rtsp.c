/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 20019
 *					All rights reserved
 *
 *  This file is part of GPAC / rtsp output filter
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

#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/maths.h>

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)

#include <gpac/filters.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/rtp_streamer.h>

typedef struct
{
	GF_RTPStreamer *rtp;
	u16 port;

	/*scale from TimeStamps in media timescales to TimeStamps in microseconds*/
	Double microsec_ts_scale;

	u32 id, codecid;
	Bool is_encrypted;

	/*NALU size for H264/AVC parsing*/
	u32 avc_nalu_size;

	GF_FilterPid *pid;

	u32 timescale;
	u32 nb_aus;

	u32 depends_on;
	u32 cfg_crc;


	/*loaded AU info*/
	GF_FilterPacket *pck;
	u64 current_dts, current_cts, min_dts;
	u32 current_sap, current_duration;

	u32 pck_num;
	u32 sample_duration;
	u32 sample_desc_index;

	Bool inject_ps;

	/*normalized DTS in micro-sec*/
	u64 microsec_dts;

	/*offset of CTS/DTS in media timescale, used when looping the pid*/
	u64 ts_offset;
	/*offset of CTS/DTS in microseconds, used when looping the pid*/
	u64 microsec_ts_offset;

	GF_AVCConfig *avcc;
	GF_HEVCConfig *hvcc;
	u32 rtp_ts_offset;

	s32 ts_delay;
	Bool selected;
	Bool bye_sent;
} GF_RTPOutStream;

typedef struct
{
	GF_RTSPSession *rtsp;
	GF_RTSPCommand *command;
	GF_RTSPResponse *response;
	GF_List *streams;

	char *service_name;
	char *sessionID;

	Bool is_playing;
	Double start_range;
	u32 last_cseq;
} GF_RTSPOutSession;

typedef struct
{
	//options
	char *dst, *user_agent;
	GF_List *mounts;
	u32 port, firstport;
	Bool xps;
	u32 mtu;
	u32 ttl;
	char *ifce;
	u32 payt, tt;
	s32 delay;
	char *info, *url, *email;
	s32 runfor, tso;
	u32 maxc;
	u32 block_size;

	/*timeline origin of our session (all tracks) in microseconds*/
	u64 sys_clock_at_init;

	/*list of streams in session*/
	GF_List *streams;

	/*base stream if this stream contains a media decoding dependency, 0 otherwise*/
	u32 base_pid_id;

	Bool first_RTCP_sent;

	GF_RTPOutStream *active_stream;
	u32 active_stream_idx;
	u64 active_min_ts_microsec;

	GF_Socket *server_sock;
	GF_List *sessions;

	Bool wait_for_loop;
	u64 microsec_ts_init;

	u32 next_wake_us;
	char *server_path;
	char *ip;
} GF_RTSPOutCtx;


static GF_Err rtspout_send_sdp(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	FILE *sdp_out;
	Double max_dur=0;
	char sdpLine[20000];
	u32 i, count, fsize;
	Bool disable_seek = GF_FALSE;
	const char *ip = ctx->ip;
	if (!ip) ip = ctx->ifce;
	if (!ip) ip = "127.0.0.1";

	sdp_out = gf_temp_file_new(NULL);
	if (!sdp_out) return GF_IO_ERR;

	count = gf_list_count(sess->streams);

	sprintf(sdpLine, "v=0");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "o=gpac 3357474383 1148485440000 IN IP%d %s", gf_net_is_ipv6(ip) ? 6 : 4, ip);
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "s=livesession");
	fprintf(sdp_out, "%s\n", sdpLine);

	if (ctx->info) {
		sprintf(sdpLine, "i=%s", ctx->info);
	} else {
		GF_RTPOutStream *stream = gf_list_get(sess->streams, 0);
		const char *src = gf_filter_pid_orig_src_args(stream->pid);
		if (!src) src = gf_filter_pid_get_source_filter_name(stream->pid);
		else {
			src = gf_file_basename(src);
		}
		if (src)
			sprintf(sdpLine, "i=%s", src);
	}
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "u=%s", ctx->url ? ctx->url : "http://gpac.io");
	fprintf(sdp_out, "%s\n", sdpLine);
	if (ctx->email) {
		sprintf(sdpLine, "e=%s", ctx->email);
		fprintf(sdp_out, "%s\n", sdpLine);
	}
	fprintf(sdp_out, "c=IN IP4 0.0.0.0\n");
	fprintf(sdp_out, "t=0 0\n");
	fprintf(sdp_out, "a=control=*\n");

	if (gf_sys_is_test_mode()) {
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC - http://gpac.io");
	} else {
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC %s - %s", gf_gpac_version(), gf_gpac_copyright() );
	}
	fprintf(sdp_out, "%s\n", sdpLine);

	if (ctx->base_pid_id)
	{
		sprintf(sdpLine, "a=group:DDP L%d", ctx->base_pid_id);
		fprintf(sdp_out, "%s", sdpLine);
		for (i = 0; i < count; i++) {
			GF_RTPOutStream *st = gf_list_get(sess->streams, i);
			if (st->depends_on == ctx->base_pid_id) {
				sprintf(sdpLine, " L%d", i+1);
				fprintf(sdp_out, "%s", sdpLine);
			}
		}
		fprintf(sdp_out, "\n");
	}

	for (i=0; i<count; i++) {
        const GF_PropertyValue *p;
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		if (!stream->rtp) continue;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DURATION);
		if (p) {
			Double dur = p->value.frac.num;
			dur /= p->value.frac.den;
			if (dur>max_dur) max_dur = dur;
		}
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_PLAYBACK_MODE);
		if (!p || (p->value.uint<GF_PLAYBACK_MODE_SEEK))
			disable_seek = GF_TRUE;
	}

	if (!disable_seek && max_dur) {
		fprintf(sdp_out, "a=range:npt=0-%g\n", max_dur);
	}


	for (i=0; i<count; i++) {
		char *sdp_media=NULL;
		const char *KMS = NULL;
		char *dsi = NULL;
		char *dsi_enh = NULL;
		u32 w, h, tw, th;
		s32 tx, ty;
		s16 tl;
		u32 dsi_len = 0;
		u32 dsi_enh_len = 0;
        const GF_PropertyValue *p;
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		if (!stream->rtp) continue;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DECODER_CONFIG);
		if (p && p->value.data.ptr) {
			dsi = p->value.data.ptr;
			dsi_len = p->value.data.size;
		}

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
		if (p && p->value.data.ptr) {
			dsi_enh = p->value.data.ptr;
			dsi_enh_len = p->value.data.size;
		}


		w = h = tw = th = 0;
		tx = ty = 0;
		tl = 0;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_WIDTH);
		if (p) w = p->value.uint;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_HEIGHT);
		if (p) h = p->value.uint;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_PROTECTION_KMS_URI);
		if (p) KMS = p->value.string;

		if (stream->codecid == GF_CODECID_TX3G) {
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_TRANS_X);
			if (p) tx = p->value.sint;
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_TRANS_Y);
			if (p) ty = p->value.sint;
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ZORDER);
			if (p) tl = p->value.sint;
			tw = w;
			th = h;

			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_WIDTH_MAX);
			if (p) w = p->value.uint;

			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_HEIGHT_MAX);
			if (p) h = p->value.uint;
		}

		gf_rtp_streamer_append_sdp_extended(stream->rtp, stream->id, dsi, dsi_len, dsi_enh, dsi_enh_len, (char *)KMS, w, h, tw, th, tx, ty, tl, GF_TRUE, &sdp_media);


		if (ctx->base_pid_id) {
			u32 size, j;
			char sdp[20000], sdpLine[10000];

			sprintf(sdp, "a=mid:L%d\n", i+1);
			sprintf(sdpLine, "a=depend:%d lay", gf_rtp_streamer_get_payload_type(stream->rtp) );
			strcat(sdp, sdpLine);

			for (j=0; j<count; j++) {
				GF_RTPOutStream *tk = gf_list_get(sess->streams, j);
				if (tk == stream) continue;
				if (tk->depends_on == stream->id) {
					sprintf(sdpLine, " L%d:%d", j+1, gf_rtp_streamer_get_payload_type(tk->rtp) );
					strcat(sdp, sdpLine);
				}
			}
			strcat(sdp, "\n");

			size = (u32) strlen(sdp) + (sdp_media ? (u32) strlen(sdp_media) : 0) + 1;
			if ( ! sdp_media) {
				sdp_media = (char*)gf_malloc(sizeof(char)*size);
				if (! sdp_media) return GF_OUT_OF_MEM;
				strcpy(sdp_media, sdp);
			} else {
				sdp_media = (char*)gf_realloc(sdp_media, sizeof(char)*size);
				if (! sdp_media) return GF_OUT_OF_MEM;
				strcat(sdp_media, sdp);
			}
		}
		if (sdp_media) {
			fprintf(sdp_out, "%s", sdp_media);
			gf_free(sdp_media);
		}

		fprintf(sdp_out, "a=control:trackID=%d\n", stream->id);

	}
	fprintf(sdp_out, "\n");


	fsize = (u32) gf_ftell(sdp_out);
	char *sdp_output = gf_malloc(sizeof(char)*(fsize+1));
	gf_fseek(sdp_out, 0, SEEK_SET);
	u32 read = (u32) fread(sdp_output, 1, fsize, sdp_out);
	sdp_output[read]=0;
	gf_fclose(sdp_out);


	gf_rtsp_response_reset(sess->response);
	sess->response->ResponseCode = NC_RTSP_OK;
	sess->response->CSeq = sess->command->CSeq;
	sess->response->body = sdp_output;
	sess->response->User_Agent = ctx->user_agent;
	gf_rtsp_send_response(sess->rtsp, sess->response);
	sess->response->User_Agent = NULL;
	sess->response->body = NULL;
	gf_free(sdp_output);

	return GF_OK;
}


#if 0
static Double rtspout_get_current_time(GF_RTSPOutCtx *ctx)
{
    Double res = (Double) (ctx->last_min_dts - ctx->sys_clock_at_init);
    res /= 1000000;
    return res;
}
#endif


static u16 rtspout_check_next_port(GF_RTSPOutSession *sess, u16 first_port)
{
	u32 i, count = gf_list_count(sess->streams);
	for (i=0;i<count; i++) {
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		if (stream->port==first_port) {
			return rtspout_check_next_port(sess, (u16) (first_port+2) );
		}
	}
	return first_port;
}

static void rtspout_del_stream(GF_RTPOutStream *st)
{
	if (st->rtp) gf_rtp_streamer_del(st->rtp);
	if (st->pck) gf_filter_pid_drop_packet(st->pid);
	if (st->avcc)
		gf_odf_avc_cfg_del(st->avcc);
	if (st->hvcc)
		gf_odf_hevc_cfg_del(st->hvcc);
	gf_free(st);
}

static GF_Err rtspout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_RTSPOutCtx *ctx = (GF_RTSPOutCtx *) gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	u32 flags, average_size, max_size, max_tsdelta, codecid, const_dur, nb_ch, streamType, samplerate, max_cts_offset, bandwidth, IV_length, KI_length, dsi_len, max_ptime, au_sn_len, payt;
	char *dsi;
	Bool is_crypted;
	const GF_PropertyValue *p;

	max_ptime = au_sn_len = 0;

	if (is_remove) {
		GF_RTPOutStream *t = gf_filter_pid_get_udta(pid);
		if (t) {
			if (ctx->active_stream==t) ctx->active_stream = NULL;
			gf_list_del_item(ctx->streams, t);
			rtspout_del_stream(t);
		}
		if (!gf_list_count(ctx->streams)) {
			return GF_EOS;
		}
		return GF_OK;
	}
	stream = gf_filter_pid_get_udta(pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	streamType = p ? p->value.uint : 0;

	switch (streamType) {
	case GF_STREAM_VISUAL:
	case GF_STREAM_AUDIO:
		break;
	case GF_STREAM_FILE:
	case GF_STREAM_UNKNOWN:
		if (stream) {
			if (ctx->active_stream==stream) ctx->active_stream = NULL;
			gf_list_del_item(ctx->streams, stream);
			rtspout_del_stream(stream);
		}
		return GF_FILTER_NOT_SUPPORTED;
		break;
	default:
		break;
	}
	if (!stream) {
		GF_SAFEALLOC(stream, GF_RTPOutStream);
		if (!stream) return GF_OUT_OF_MEM;
		gf_list_add(ctx->streams, stream);
		stream->pid = pid;
		stream->min_dts = GF_FILTER_NO_TS;
	 	gf_filter_pid_set_udta(pid, stream);
	}

	dsi_len = samplerate = nb_ch = IV_length = KI_length = 0;
	is_crypted = 0;
	dsi = NULL;

	flags = 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	codecid = p ? p->value.uint : 0;
	if (stream->codecid && (stream->codecid != codecid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Dynamic change of codec in RTP session not supported !\n"));
		return GF_FILTER_NOT_SUPPORTED;
	}
	stream->codecid = codecid;

	stream->is_encrypted = GF_FALSE;
	if (streamType==GF_STREAM_ENCRYPTED) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
		if (p) streamType = p->value.uint;
		stream->is_encrypted = GF_TRUE;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
		if (!p || (p->value.uint != GF_ISOM_ISMACRYP_SCHEME)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Protected track with scheme type %s, cannot stream (only ISMA over RTP is supported !\n", p ? gf_4cc_to_str(p->value.uint) : "unknwon" ));
			return GF_FILTER_NOT_SUPPORTED;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	stream->timescale = p ? p->value.uint : 1000;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CONFIG_IDX);
	stream->sample_desc_index = p ? p->value.uint : 0;

	u32 cfg_crc=0;
	dsi = NULL;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		dsi = p->value.data.ptr;
		dsi_len = p->value.data.size;
		cfg_crc = gf_crc_32(dsi, dsi_len);
	}
	if (stream->rtp && (cfg_crc==stream->cfg_crc))
		return GF_OK;

	if (ctx->xps)
		stream->inject_ps = GF_TRUE;
	else if (stream->cfg_crc)
		stream->inject_ps = GF_TRUE;
	stream->cfg_crc = cfg_crc;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
	stream->nb_aus = p ? p->value.uint : 0;

	stream->port = ctx->firstport;

	/*init packetizer*/
	switch (streamType) {
	case GF_STREAM_OD:
	case GF_STREAM_SCENE:
		//todo, check if sync shadow is used
//		if (gf_isom_has_sync_shadows(ctx->isom, stream->track_num) || gf_isom_has_sample_dependency(ctx->isom, stream->track_num))
//			flags |= GP_RTP_PCK_SYSTEMS_CAROUSEL;
		break;
	case GF_STREAM_AUDIO:
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) samplerate = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) nb_ch = p->value.uint;
		break;
	case GF_STREAM_VISUAL:
		break;
	default:
		break;
	}

	if (stream->avcc) gf_odf_avc_cfg_del(stream->avcc);
	stream->avcc = NULL;
	if (stream->hvcc) gf_odf_hevc_cfg_del(stream->hvcc);
	stream->hvcc = NULL;

	stream->avc_nalu_size = 0;
	switch (codecid) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		if (dsi) {
			GF_AVCConfig *avcc = gf_odf_avc_cfg_read(dsi, dsi_len);
			if (avcc) {
				stream->avc_nalu_size = avcc->nal_unit_size;
				if (stream->inject_ps)
					stream->avcc = avcc;
				else
					gf_odf_avc_cfg_del(avcc);
			}
		}
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		if (dsi) {
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(dsi, dsi_len, (codecid==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE );
			if (hvcc) {
				stream->avc_nalu_size = hvcc->nal_unit_size;
				if (stream->inject_ps) {
					u32 i, count = gf_list_count(hvcc->param_array);
					GF_HEVCParamArray *vpsa=NULL, *spsa=NULL;
					stream->hvcc = hvcc;
					for (i=0; i<count; i++) {
						GF_HEVCParamArray *pa = gf_list_get(hvcc->param_array, i);
						if (!vpsa && (pa->type == GF_HEVC_NALU_VID_PARAM)) {
							vpsa = pa;
							gf_list_rem(hvcc->param_array, i);
							count--;
						}
						else if (!spsa && (pa->type == GF_HEVC_NALU_SEQ_PARAM)) {
							spsa = pa;
							gf_list_rem(hvcc->param_array, i);
							count--;
						}
					}
					//insert SPS at begining
					gf_list_insert(hvcc->param_array, spsa, 0);
					//insert VPS at begining - we now have VPS, SPS and other (PPS, SEI...)
					gf_list_insert(hvcc->param_array, vpsa, 0);
				} else
					gf_odf_hevc_cfg_del(hvcc);
			}
		}
		break;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ID);
	stream->id = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_BITRATE);
	bandwidth = p ? p->value.uint : 0;

	/*get sample info*/
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_FRAME_SIZE);
	max_size = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_AVG_FRAME_SIZE);
	average_size = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_TS_DELTA);
	max_tsdelta = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_CTS_OFFSET);
	max_cts_offset = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_CONSTANT_DURATION);
	const_dur = p ? p->value.uint : 0;

	if (is_crypted) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_SELECTIVE_ENC);
		if (p->value.boolean) {
			flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_IV_LENGTH);
		IV_length = p ? p->value.uint : 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_KI_LENGTH);
		KI_length = p ? p->value.uint : 0;
	}

	payt = ctx->payt + gf_list_find(ctx->streams, stream);

	stream->rtp = gf_rtp_streamer_new_extended(streamType, codecid, stream->timescale,
				 (char *) ctx->ifce ? ctx->ifce : "127.0.0.1", stream->port, ctx->mtu, ctx->ttl, ctx->ifce,
				 flags, dsi, dsi_len,
				 payt, samplerate, nb_ch,
				 is_crypted, IV_length, KI_length,
				 average_size, max_size, max_tsdelta, max_cts_offset, const_dur, bandwidth, max_ptime, au_sn_len, GF_TRUE);

	if (!stream->rtp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Could not initialize RTP for stream %s: %s\n", gf_filter_pid_get_name(pid), gf_error_to_string(e)));
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DELAY);
	stream->ts_delay = p ? p->value.sint : 0;

	payt++;
	stream->microsec_ts_scale = 1000000;
	stream->microsec_ts_scale /= stream->timescale;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) {
		ctx->base_pid_id = p->value.uint;
		gf_rtp_streamer_disable_auto_rtcp(stream->rtp);
	}

	{
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
		if (!p || (p->value.uint<GF_PLAYBACK_MODE_SEEK)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] PID %s cannot be seek, disabling loop\n", gf_filter_pid_get_name(pid) ));
		}
	}
	return GF_OK;
}

static GF_Err rtspout_initialize(GF_Filter *filter)
{
	char szIP[1024];
	GF_Err e;
	u16 port;
	char *ip;
	GF_RTSPOutCtx *ctx = (GF_RTSPOutCtx *) gf_filter_get_udta(filter);
	if (!ctx->payt) ctx->payt = 96;
	if (!ctx->port) ctx->port = 554;
	if (!ctx->firstport) ctx->firstport = 7000;
	if (!ctx->mtu) ctx->mtu = 1450;
	if (ctx->payt<96) ctx->payt = 96;
	if (ctx->payt>127) ctx->payt = 127;
	ctx->sessions = gf_list_new();

	ctx->streams = gf_list_new();

	port = ctx->port;
	ip = ctx->ifce;

	if (!ctx->dst) {
		if (!ctx->mounts) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] No root dir for server, cannot run\n" ));
			return GF_BAD_PARAM;
		}
		gf_filter_make_sticky(filter);
	} else {
		char *sep = strchr(ctx->dst+7, '/');
		if (sep) {
			ctx->server_path = sep+1;
			strncpy(szIP, ctx->dst+7, sep-ctx->dst-7);
			sep = strchr(szIP, ':');
			if (sep) {
				port = atoi(sep+1);
				if (!port) port = ctx->port;
				sep[0] = 0;
			}
			if (strlen(szIP)) ip = szIP;
		}
	}

	if (ip)
		ctx->ip = gf_strdup(ip);

	ctx->server_sock = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(ctx->server_sock, NULL, port, ip, 0, GF_SOCK_REUSE_PORT);
	if (!e) e = gf_sk_listen(ctx->server_sock, ctx->maxc);
	if (!e) {
		gf_filter_post_process_task(filter);
		gf_sk_server_mode(ctx->server_sock, GF_TRUE);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] failed to start server on port %d: %s\n", ctx->port, gf_error_to_string(e) ));
		return e;
	}
	return GF_OK;
}

static void rtspout_del_session(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	//server mode, cleanup
	if (!ctx->dst) {
	}
	if (sess->service_name)
		gf_free(sess->service_name);
	gf_rtsp_session_del(sess->rtsp);
	gf_rtsp_command_del(sess->command);
	gf_rtsp_response_del(sess->response);
	gf_list_del_item(ctx->sessions, sess);
	gf_free(sess);
}

static void rtspout_finalize(GF_Filter *filter)
{
	GF_RTSPOutCtx *ctx = (GF_RTSPOutCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->sessions)) {
		GF_RTSPOutSession *tmp = gf_list_get(ctx->sessions, 0);
		rtspout_del_session(filter, ctx, tmp);
	}
	gf_list_del(ctx->sessions);

	while (gf_list_count(ctx->streams)) {
		GF_RTPOutStream *tmp = gf_list_pop_back(ctx->streams);
		rtspout_del_stream(tmp);
	}
	gf_list_del(ctx->streams);

	gf_sk_del(ctx->server_sock);
	if (ctx->ip) gf_free(ctx->ip);	
}

static GF_Err rtspout_send_xps(GF_RTPOutStream *stream, GF_List *pslist, Bool *au_start, u32 pck_size, u32 cts, u32 dts, u32 duration)
{
	GF_Err e;
	u32 i, count = gf_list_count(pslist);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(pslist, i);
		e = gf_rtp_streamer_send_data(stream->rtp, (char *) sl->data, sl->size, pck_size, cts, dts, stream->current_sap ? 1 : 0, *au_start, GF_FALSE, stream->pck_num, duration, stream->sample_desc_index);
		if (e) return e;
		*au_start = GF_FALSE;
	}
	return GF_OK;
}

static Bool rtspout_init_clock(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	u64 min_dts = GF_FILTER_NO_TS;
	u32 i, count = gf_list_count(sess->streams);

	for (i=0; i<count; i++) {
		u64 dts;
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		GF_FilterPacket *pck;
		while (1) {
			pck = gf_filter_pid_get_packet(stream->pid);
			if (!pck) return GF_FALSE;
			if (gf_filter_pck_get_seek_flag(pck)) {
				gf_filter_pid_drop_packet(stream->pid);
				continue;
			}
			break;
		}

		dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS)
			dts = gf_filter_pck_get_cts(pck);

		if (dts==GF_FILTER_NO_TS) dts=0;

		dts *= 1000000;
		dts /= stream->timescale;
		if (min_dts > dts)
			min_dts = dts;

		if (ctx->tso>0) {
			u64 offset = ctx->tso;
			offset *= stream->timescale;
			offset /= 1000000;
			stream->rtp_ts_offset = (u32) offset;
		}
		stream->current_cts = gf_filter_pck_get_cts(pck);
	}
	ctx->sys_clock_at_init = gf_sys_clock_high_res();
	ctx->microsec_ts_init = min_dts;
	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] RTP clock initialized - time origin set to "LLU" us (sys clock) / "LLU" us (media clock)\n", ctx->sys_clock_at_init, ctx->microsec_ts_init));
	if (ctx->tso<0) {
		gf_rand_init(0);
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
			stream->rtp_ts_offset = gf_rand();
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] RTP stream %d initial RTP TS set to %d\n", i+1, stream->rtp_ts_offset));
		}
	}


	gf_rtsp_response_reset(sess->response);
	sess->response->ResponseCode = NC_RTSP_OK;
	for (i=0; i<count; i++) {
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		GF_RTPInfo *rtpi;
		GF_SAFEALLOC(rtpi, GF_RTPInfo);
		rtpi->url = gf_malloc(sizeof(char) * (strlen(sess->service_name)+50));
		sprintf(rtpi->url, "%s/trackID=%d", sess->service_name, stream->id);
		rtpi->seq = gf_rtp_streamer_get_next_rtp_sn(stream->rtp);
		rtpi->rtp_time = (u32) (stream->current_cts + stream->ts_offset + stream->rtp_ts_offset);

		gf_list_add(sess->response->RTP_Infos, rtpi);
	}
	GF_SAFEALLOC(sess->response->Range, GF_RTSPRange);
	sess->response->Range->start = sess->start_range;
	sess->response->CSeq = sess->last_cseq;
	sess->response->User_Agent = ctx->user_agent;
	gf_rtsp_send_response(sess->rtsp, sess->response);
	sess->response->User_Agent = NULL;

	return GF_TRUE;
}

static void rtspout_send_event(GF_RTSPOutSession *sess, Bool send_stop, Bool send_play, Double start_range)
{
	GF_FilterEvent fevt;
	u32 i, count = gf_list_count(sess->streams);

	memset(&fevt, 0, sizeof(GF_FilterEvent));

	for (i=0; i<count; i++) {
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		if (!stream->selected) continue;

		fevt.base.on_pid = stream->pid;
		if (send_stop) {
			fevt.base.type = GF_FEVT_STOP;
			gf_filter_pid_send_event(stream->pid, &fevt);
		}
		if (send_play) {
			fevt.base.type = GF_FEVT_PLAY;
			fevt.play.start_range = start_range;
			gf_filter_pid_send_event(stream->pid, &fevt);
		}
	}

}

static GF_Err rtspout_process_rtp(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	u32 duration, i, count;
	s64 diff;
	u64 clock;
	const char *pck_data;
	u32 pck_size;
	u32 dts, cts;

	clock = gf_sys_clock_high_res();

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!ctx->sys_clock_at_init) {
		if (!rtspout_init_clock(ctx, sess)) return GF_OK;
	}

	if (ctx->runfor>0) {
		s64 diff = clock;
		diff -= ctx->sys_clock_at_init;
		diff /= 1000;
		if ((s32) diff > ctx->runfor) {
			count = gf_list_count(sess->streams);
			for (i=0; i<count; i++) {
				stream = gf_list_get(ctx->streams, i);
				gf_filter_pid_set_discard(stream->pid, GF_TRUE);
				stream->pck = NULL;
			}
			return GF_EOS;
		}
	}

	/*browse all inputs and locate most mature stream*/
	if (!ctx->active_stream) {
		u32 nb_eos = 0;
		count = gf_list_count(sess->streams);

		ctx->active_min_ts_microsec = (u64) -1;
		for (i=0; i<count; i++) {
			stream = gf_list_get(sess->streams, i);
			if (!stream->rtp) continue;

			/*load next AU*/
			if (!stream->pck) {
				stream->pck = gf_filter_pid_get_packet(stream->pid);

				if (!stream->pck) {
					if (gf_filter_pid_is_eos(stream->pid)) {
						//flush stream
						if (!stream->bye_sent) {
							stream->bye_sent = GF_TRUE;
							gf_rtp_streamer_send_au(stream->rtp, NULL, 0, 0, 0, GF_FALSE);
							gf_rtp_streamer_send_bye(stream->rtp);
						}
						nb_eos++;
					}
					continue;
				}
				stream->current_dts = gf_filter_pck_get_dts(stream->pck);
				stream->current_cts = gf_filter_pck_get_cts(stream->pck);
				stream->current_sap = gf_filter_pck_get_sap(stream->pck);
				duration = gf_filter_pck_get_duration(stream->pck);
				if (duration) stream->current_duration = duration;
				if (stream->current_dts==GF_FILTER_NO_TS)
					stream->current_dts = stream->current_cts;

				if (stream->min_dts==GF_FILTER_NO_TS) {
					stream->min_dts = stream->current_dts;
				}

				stream->microsec_dts = (u64) (stream->microsec_ts_scale * (s64) (stream->current_dts) + stream->microsec_ts_offset + ctx->sys_clock_at_init);
				if (stream->microsec_dts < ctx->microsec_ts_init) {
					stream->microsec_dts = 0;
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTSPOut] next RTP packet (stream %d) has a timestamp "LLU" less than initial timestamp "LLU" - forcing to 0\n", i+1, stream->microsec_dts, ctx->microsec_ts_init));
				} else {
					stream->microsec_dts -= ctx->microsec_ts_init;
				}

				if (stream->current_sap>GF_FILTER_SAP_3) stream->current_sap = 0;
				stream->pck_num++;
				ctx->wait_for_loop = GF_FALSE;
			}

			/*check timing*/
			if (stream->pck) {
				if (ctx->active_min_ts_microsec > stream->microsec_dts) {
					ctx->active_min_ts_microsec = stream->microsec_dts;
					ctx->active_stream = stream;
					ctx->active_stream_idx = i+1;
				}
			}
		}

		/*no input data ...*/
		if (!ctx->active_stream) {
			if (nb_eos==count) {
//				u64 max_dur = 0;
//				if (!ctx->loop)
					return GF_EOS;

#if 0
				for (i=0; i<count; i++) {
					GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
					u64 dur = stream->current_dts + stream->current_duration - stream->min_dts;

					dur *= 1000000;
					dur /= stream->timescale;

					if (max_dur < dur) {
						max_dur = dur;
					}
				}
				if (ctx->wait_for_loop)
					return GF_OK;

				GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] RTP session done, looping source\n"));
				ctx->wait_for_loop = GF_TRUE;
				for (i=0; i<count; i++) {
					GF_FilterEvent evt;
					u64 new_ts;
					GF_RTPOutStream *stream = gf_list_get(sess->streams, i);

					new_ts = max_dur;
					new_ts *= stream->timescale;
					new_ts /= 1000000;
					stream->ts_offset += new_ts;
					stream->microsec_ts_offset = (u64) (stream->ts_offset*(1000000.0/stream->timescale) + ctx->sys_clock_at_init);

					//loop pid: stop and play
					GF_FEVT_INIT(evt, GF_FEVT_STOP, stream->pid);
					gf_filter_pid_send_event(stream->pid, &evt);

					GF_FEVT_INIT(evt, GF_FEVT_PLAY, stream->pid);
					gf_filter_pid_send_event(stream->pid, &evt);
				}
#endif

			}
			return GF_OK;
		}
	}

	stream = ctx->active_stream;

	clock = gf_sys_clock_high_res();
	diff = (s64) ctx->active_min_ts_microsec;
	diff += (s64) ctx->delay * 1000;
	diff -= (s64) clock;

	if (diff > 1000) {
		u64 repost_in;
		//if more than 11 msecs ahead of time, ask for delay minus one second, otherwise ask for half the delay
		if (diff<=11000) repost_in = diff/2;
		else repost_in = diff - 10000;
		if (ctx->next_wake_us>repost_in)
			ctx->next_wake_us = (u32) repost_in;

		return GF_OK;
	} else if (diff<=-1000) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTSPOut] RTP session stream %d - sending packet %d too late by %d us - clock "LLU" us\n", ctx->active_stream_idx, stream->pck_num, -diff, clock));
	} else if (diff>0){
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTSPOut] RTP session stream %d - sending packet %d ahead of %d us - clock "LLU" us\n", ctx->active_stream_idx, stream->pck_num, diff, clock));
	}
	ctx->next_wake_us = 0;

	/*send packets*/
	pck_data = gf_filter_pck_get_data(stream->pck, &pck_size);
	if (!pck_size) {
		gf_filter_pid_drop_packet(stream->pid);
		stream->pck = NULL;
		ctx->active_stream = NULL;
		return GF_OK;
	}

	dts = (u32) (stream->current_dts + stream->ts_offset);
	cts = (u32) (stream->current_cts + stream->ts_offset);
	duration = stream->current_duration;

	dts += stream->rtp_ts_offset;
	cts += stream->rtp_ts_offset;
	if (stream->ts_delay>=0) {
		dts += stream->ts_delay;
		cts += stream->ts_delay;
	} else {
		if ((s32) dts >= -stream->ts_delay)
			dts += stream->ts_delay;
		else
			dts = 0;

		if ((s32) cts >= -stream->ts_delay )
			cts += stream->ts_delay;
		else
			cts = 0;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTSPOut] Sending RTP packets for stream %d pck %d/%d DTS "LLU" - CTS "LLU" - RTP TS "LLU" - size %d - SAP %d - clock "LLU" us\n", ctx->active_stream_idx, stream->pck_num, stream->nb_aus, stream->current_dts, stream->current_dts, cts, pck_size, stream->current_sap, clock) );
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] Runtime %08u ms send stream %d pck %08u/%08u DTS %08u CTS %08u RTP-TS %08u size %08u SAP %d\r", (u32) (clock - ctx->sys_clock_at_init)/1000, ctx->active_stream_idx, stream->pck_num, stream->nb_aus, (u32) stream->current_dts, (u32) stream->current_dts, (u32) cts, pck_size, stream->current_sap) );
	}
#endif

	/*we are about to send scalable base: trigger RTCP reports with the same NTP. This avoids
	NTP drift due to system clock precision which could break sync decoding*/
	if (!ctx->first_RTCP_sent || (ctx->base_pid_id && ctx->base_pid_id== stream->id)) {
		u32 ntp_sec, ntp_frac;
		/*force sending RTCP SR every RAP ? - not really compliant but we cannot perform scalable tuning otherwise*/
		u32 ntp_type = stream->current_sap ? 2 : 1;
		gf_net_get_ntp(&ntp_sec, &ntp_frac);
		count = gf_list_count(sess->streams);

		for (i=0; i<count; i++) {
			GF_RTPOutStream *astream = gf_list_get(sess->streams, i);
			if (!astream->pck) break;

			u32 ts = (u32) (astream->current_cts + astream->ts_offset + astream->rtp_ts_offset);
			gf_rtp_streamer_send_rtcp(stream->rtp, GF_TRUE, ts, ntp_type, ntp_sec, ntp_frac);
		}
		ctx->first_RTCP_sent = 1;
	}

	/*unpack nal units*/
	if (stream->avc_nalu_size) {
		Bool au_start, au_end;
		u32 v, size;
		u32 remain = pck_size;
		const char *ptr = pck_data;

		au_start = 1;

		if (stream->avcc && stream->current_sap) {
			e = rtspout_send_xps(stream, stream->avcc->sequenceParameterSets, &au_start, pck_size, cts, dts, duration);
			e = rtspout_send_xps(stream, stream->avcc->sequenceParameterSetExtensions, &au_start, pck_size, cts, dts, duration);
			e = rtspout_send_xps(stream, stream->avcc->pictureParameterSets, &au_start, pck_size, cts, dts, duration);
		}
		else if (stream->hvcc && stream->current_sap) {
			u32 nbps = gf_list_count(stream->hvcc->param_array);
			for (i=0; i<nbps; i++) {
				GF_HEVCParamArray *pa = gf_list_get(stream->hvcc->param_array, i);
				e = rtspout_send_xps(stream, pa->nalus, &au_start, pck_size, cts, dts, duration);
			}
		}

		while (remain) {
			size = 0;
			v = ctx->active_stream->avc_nalu_size;
			while (v) {
				size |= (u8) *ptr;
				ptr++;
				remain--;
				v-=1;
				if (v) size<<=8;
			}
			if (remain < size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Broken AVC nalu encapsulation: NALU size is %d but only %d bytes left in sample %d\n", size, remain, ctx->active_stream->pck_num));
				break;
			}
			remain -= size;
			au_end = remain ? 0 : 1;

			e = gf_rtp_streamer_send_data(stream->rtp, (char *) ptr, size, pck_size, cts, dts, stream->current_sap ? 1 : 0, au_start, au_end, stream->pck_num, duration, stream->sample_desc_index);
			ptr += size;
			au_start = 0;
		}
	} else {
		e = gf_rtp_streamer_send_data(stream->rtp, (char *) pck_data, pck_size, pck_size, cts, dts, stream->current_sap ? 1 : 0, 1, 1, stream->pck_num, duration, stream->sample_desc_index);
	}
	gf_filter_pid_drop_packet(stream->pid);
	stream->pck = NULL;

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Error sending RTP packet %d: %s\n", stream->pck_num, gf_error_to_string(e) ));
	}

	ctx->active_stream = NULL;
	return e;
}


static GF_Err rtspout_process_session_signaling(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	GF_Err e;
	e = gf_rtsp_get_command(sess->rtsp, sess->command);
	if (e==GF_IP_NETWORK_EMPTY) {
		return GF_OK;
	}
	if (e==GF_IP_CONNECTION_CLOSED) {
		rtspout_del_session(filter, ctx, sess);
		return GF_OK;
	}
	if (e) return e;

	fprintf(stderr, "got request %s\n", sess->command->method);

	//process describe
	if (!strcmp(sess->command->method, GF_RTSP_DESCRIBE)) {
		u32 rsp_code = NC_RTSP_OK;
		char *sep = NULL;
		if (sess->command->service_name) {
			sep = strstr(sess->command->service_name, "://");
			if (sep) sep = strchr(sep+3, '/');
		}
		if (!sep) {
			rsp_code = NC_RTSP_Not_Found;
		} else if (ctx->dst) {
			if (ctx->server_path) {
				if (strcmp(ctx->server_path, sep+1))
					rsp_code = NC_RTSP_Not_Found;
			}
			sess->streams = ctx->streams;
		}
		//TODO
		else {
			rsp_code = NC_RTSP_Not_Found;
		}

		if (rsp_code != NC_RTSP_OK) {
			gf_rtsp_response_reset(sess->response);
			sess->response->ResponseCode = rsp_code;
			sess->response->CSeq = sess->command->CSeq;
			sess->response->User_Agent = ctx->user_agent;
			gf_rtsp_send_response(sess->rtsp, sess->response);
			sess->response->User_Agent = NULL;
			return GF_OK;
		}
		if (sess->service_name) gf_free(sess->service_name);
		sess->service_name = gf_strdup(sess->command->service_name);

		//single session, create SDP
		rtspout_send_sdp(ctx, sess);
		return GF_OK;
	}

	//process describe
	if (!strcmp(sess->command->method, GF_RTSP_SETUP)) {
	 	char remoteIP[GF_MAX_IP_NAME_LEN];
		GF_RTPOutStream *stream = NULL;
		GF_RTSPTransport *transport = gf_list_get(sess->command->Transports, 0);
		char *ctrl=NULL;
		u32 stream_id=0;
		u32 rsp_code=NC_RTSP_OK;
		if (sess->service_name) {
			char *sep = strstr(sess->service_name, "://");
			if (sep) sep = strchr(sep+3, '/');
			if (sep) sep = strstr(sess->command->service_name, sep);

			if (!sep) rsp_code=NC_RTSP_Bad_Request;
			else {
				ctrl = strrchr(sess->command->service_name, '/');
			}
		} else {
			ctrl = strrchr(sess->command->service_name, '/');
		}
		if (ctrl) {
			sscanf(ctrl, "/trackID=%d", &stream_id);
		}
		if (!ctrl || !transport) {
			rsp_code = NC_RTSP_Bad_Request;
		} else if (sess->sessionID && sess->command->Session && strcmp(sess->sessionID, sess->command->Session)) {
			rsp_code = NC_RTSP_Bad_Request;
		} else if (sess->sessionID && !sess->command->Session) {
			rsp_code = NC_RTSP_Not_Implemented;
		} else {
			u32 i, count = gf_list_count(sess->streams);
			for (i=0; i<count; i++) {
				stream = gf_list_get(sess->streams, i);
				if (stream_id==stream->id)
					break;
				stream=NULL;
			}
			if (!stream_id) rsp_code = NC_RTSP_Not_Found;
		}

		if (!stream) {
			gf_rtsp_response_reset(sess->response);
			sess->response->ResponseCode = rsp_code;
			sess->response->CSeq = sess->command->CSeq;
			sess->response->User_Agent = ctx->user_agent;
			gf_rtsp_send_response(sess->rtsp, sess->response);
			sess->response->User_Agent = NULL;
			return GF_OK;
		}

		gf_rtsp_response_reset(sess->response);
		sess->response->CSeq = sess->command->CSeq;

		stream->selected = GF_TRUE;
		if (!transport->IsInterleaved) {

			transport->port_first = rtspout_check_next_port(sess, ctx->firstport);
			transport->port_last = transport->port_first + 1;
		}
		transport->SSRC = rand();
		transport->is_sender = GF_TRUE;
		fprintf(stderr, "ssrc is %u\n", transport->SSRC);
		
		if (transport->destination) gf_free(transport->destination);
		transport->destination = NULL;

		if (transport->IsUnicast) {
			gf_rtsp_get_remote_address(sess->rtsp, remoteIP);
			transport->destination = remoteIP;
		}
		transport->destination = "127.0.0.1";

		e = gf_rtp_streamer_init_rtsp(stream->rtp, ctx->mtu, transport, ctx->ifce);
		if (e) {
			sess->response->ResponseCode = NC_RTSP_Internal_Server_Error;
		} else {
			if (!sess->sessionID)
				sess->sessionID = gf_rtsp_generate_session_id(sess->rtsp);

			sess->response->ResponseCode = NC_RTSP_OK;
			sess->response->Session = sess->sessionID;

			gf_rtsp_get_session_ip(sess->rtsp, remoteIP);
			transport->destination = NULL;
			transport->source = remoteIP;
			gf_list_add(sess->response->Transports, transport);
		}

		sess->response->User_Agent = ctx->user_agent;
		gf_rtsp_send_response(sess->rtsp, sess->response);
		sess->response->User_Agent = NULL;
		gf_list_reset(sess->response->Transports);
		transport->destination = NULL;
		transport->source = NULL;
		return GF_OK;
	}

	//process describe
	if (!strcmp(sess->command->method, GF_RTSP_PLAY)) {
		Double start_range=0;
		u32 rsp_code=NC_RTSP_OK;
		if (sess->sessionID && sess->command->Session && strcmp(sess->sessionID, sess->command->Session)) {
			rsp_code = NC_RTSP_Bad_Request;
		} else if (!sess->command->Session || !sess->sessionID) {
			rsp_code = NC_RTSP_Bad_Request;
		}
		if (sess->command->Range)
			start_range = sess->command->Range->start;

		if (rsp_code!=NC_RTSP_OK) {
			gf_rtsp_response_reset(sess->response);
			sess->response->ResponseCode = rsp_code;
			sess->response->CSeq = sess->command->CSeq;
			sess->response->User_Agent = ctx->user_agent;
			gf_rtsp_send_response(sess->rtsp, sess->response);
			sess->response->User_Agent = NULL;
			return GF_OK;
		} else {
			sess->is_playing = GF_TRUE;
			sess->start_range = start_range;
			sess->last_cseq = sess->command->CSeq;
			rtspout_send_event(sess, GF_FALSE, GF_TRUE, start_range);
		}
	}

	return GF_OK;
}

static GF_Err rtspout_check_new_session(GF_RTSPOutCtx *ctx)
{
	GF_RTSPOutSession *sess;
	GF_RTSPSession *new_sess = gf_rtsp_session_new_server(ctx->server_sock);

	if (!new_sess) return GF_OK;

	GF_SAFEALLOC(sess, GF_RTSPOutSession);
	sess->rtsp = new_sess;
	sess->command = gf_rtsp_command_new();
	sess->response = gf_rtsp_response_new();

	gf_rtsp_set_buffer_size(new_sess, ctx->block_size);

	gf_list_add(ctx->sessions, sess);
	return GF_OK;
}

static GF_Err rtspout_process(GF_Filter *filter)
{
	GF_Err e=GF_OK;
	u32 i, count;
	GF_RTSPOutCtx *ctx = gf_filter_get_udta(filter);

	ctx->next_wake_us = 50000;
	e = rtspout_check_new_session(ctx);
	if (e==GF_IP_NETWORK_EMPTY) {
		e = GF_OK;
	}

 	count = gf_list_count(ctx->sessions);
	for (i=0; i<count; i++) {
		GF_Err sess_err;
		GF_RTSPOutSession *sess = gf_list_get(ctx->sessions, i);
		sess_err = rtspout_process_session_signaling(filter, ctx, sess);
		if (sess_err) e |= sess_err;

		if (sess->is_playing) {
			sess_err = rtspout_process_rtp(filter, ctx, sess);
			if (sess_err) e |= sess_err;
		}
	}

	if (e==GF_EOS) return GF_EOS;

	if (ctx->next_wake_us)
		gf_filter_ask_rt_reschedule(filter, ctx->next_wake_us);

	return e;
}

static GF_FilterProbeScore rtspout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "rtsp://", 7)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static const GF_FilterCapability RTSPOutCaps[] =
{
	//anything else (not file and framed) result in manifest PID
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "sdp"),
	{0},
	//anything else (not file and framed) result in media pids not file
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_RTSPOutCtx, _n)
static const GF_FilterArgs RTSPOutArgs[] =
{
	{ OFFS(dst), "location of destination file - see filter help ", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(port), "RTSP port", GF_PROP_UINT, "554", NULL, 0},
	{ OFFS(firstport), "port for first stream in session", GF_PROP_UINT, "6000", NULL, 0},
	{ OFFS(mtu), "RTP MTU size in bytes", GF_PROP_UINT, "1460", NULL, 0},
	{ OFFS(ttl), "time-to-live for muticast packets", GF_PROP_UINT, "1", NULL, 0},
	{ OFFS(ifce), "IP adress of network inteface to use", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(payt), "RTP payload type to use for dynamic configs.", GF_PROP_UINT, "96", "96-127", 0},
	{ OFFS(delay), "send delay for packet (negative means send earlier)", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(tt), "time tolerance in microseconds. Whenever schedule time minus realtime is below this value, the packet is sent right away", GF_PROP_UINT, "1000", NULL, 0},
	{ OFFS(runfor), "run for the given time in ms. Negative value means run for ever (if loop) or source duration, 0 only outputs the sdp", GF_PROP_SINT, "-1", NULL, 0},
	{ OFFS(tso), "sets timestamp offset in microsecs. Negative value means random initial timestamp", GF_PROP_SINT, "-1", NULL, 0},
	{ OFFS(xps), "force parameter set injection at each SAP. If not set, only inject if different from SDP ones", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(mounts), "list of directories to expose in server mode", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read TCP socket", GF_PROP_UINT, "4096", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(user_agent), "User agent string, by default solved from GPAC preferences", GF_PROP_STRING, "$GPAC_UA", NULL, 0},
	{0}
};


GF_FilterRegister RTSPOutRegister = {
	.name = "rtspout",
	GF_FS_SET_DESCRIPTION("RTSP Server")
	GF_FS_SET_HELP("The RTSP server partially implements RTSP 1.0")
	.private_size = sizeof(GF_RTSPOutCtx),
	.max_extra_pids = -1,
	.args = RTSPOutArgs,
	.probe_url = rtspout_probe_url,
	.initialize = rtspout_initialize,
	.finalize = rtspout_finalize,
	SETCAPS(RTSPOutCaps),
	.configure_pid = rtspout_configure_pid,
	.process = rtspout_process
};


const GF_FilterRegister *rtspout_register(GF_FilterSession *session)
{
	return &RTSPOutRegister;
}

#else

const GF_FilterRegister *rtspout_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /* !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING) */
