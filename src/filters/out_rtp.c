/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / rtp output filter
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

#include "out_rtp.h"


typedef struct
{
	//options
	char *ip;
	char *dst, *ext, *mime;
	u32 port;
	Bool loop, xps;
	Bool mpeg4;
	u32 mtu;
	u32 ttl;
	char *ifce;
	u32 payt, tt;
	s32 delay;
	char *info, *url, *email;
	s32 runfor, tso;
	Bool latm;

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

	GF_FilterPid *opid;

	Bool wait_for_loop;
	u64 microsec_ts_init;
	//0: not single stream, 1: input is raw media, 2: input is TS
	u32 single_stream;
	GF_FilterCapability in_caps[2];
	char szExt[10];
} GF_RTPOutCtx;


GF_Err rtpout_create_sdp(GF_List *streams, Bool is_rtsp, const char *ip, const char *info, const char *sess_name, const char *url, const char *email, u32 base_pid_id, FILE **sdp_tmp, u64 *session_id)
{
	FILE *sdp_out;
	u32 i, count;
	u64 session_version;
	sdp_out = gf_file_temp(NULL);
	if (!sdp_out) return GF_IO_ERR;
	*sdp_tmp = sdp_out;


	count = gf_list_count(streams);

	gf_fprintf(sdp_out, "v=0\n");
	if (gf_sys_is_test_mode()) {
		*session_id = 0;
		session_version = 0;
	} else {
		if (! *session_id) *session_id = gf_net_get_ntp_ts();
		session_version = gf_net_get_ntp_ts();
	}
	gf_fprintf(sdp_out, "o=gpac "LLU" "LLU" IN IP%d %s\n", *session_id, session_version, gf_net_is_ipv6(ip) ? 6 : 4, ip);
	gf_fprintf(sdp_out, "s=%s\n", sess_name);

	if (info) {
		gf_fprintf(sdp_out, "i=%s\n", info);
	} else {
		GF_RTPOutStream *stream = gf_list_get(streams, 0);
		const char *src = gf_filter_pid_orig_src_args(stream->pid, GF_FALSE);
		if (!src) src = gf_filter_pid_get_source_filter_name(stream->pid);
		else {
			src = gf_file_basename(src);
		}
		if (src)
			gf_fprintf(sdp_out, "i=%s\n", src);
	}
	gf_fprintf(sdp_out, "u=%s\n", url ? url : "http://gpac.io");
	if (email) {
		gf_fprintf(sdp_out, "e=%s\n", email);
	}
	if (is_rtsp) {
		gf_fprintf(sdp_out, "c=IN IP4 0.0.0.0\n");
	} else {
		gf_fprintf(sdp_out, "c=IN IP%d %s\n", gf_net_is_ipv6(ip) ? 6 : 4, ip);
	}
	gf_fprintf(sdp_out, "t=0 0\n");

	if (is_rtsp) {
		gf_fprintf(sdp_out, "a=control:*\n");
	}

	if (gf_sys_is_test_mode()) {
		gf_fprintf(sdp_out, "a=x-copyright: Streamed with GPAC - http://gpac.io\n");
	} else {
		gf_fprintf(sdp_out, "a=x-copyright: Streamed with GPAC %s - %s\n", gf_gpac_version(), gf_gpac_copyright() );
	}

	if (is_rtsp) {
		Double max_dur=0;
		Bool disable_seek = GF_FALSE;
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			GF_RTPOutStream *stream = gf_list_get(streams, i);
			if (!stream->rtp) continue;

			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DURATION);
			if (p && p->value.lfrac.den) {
				Double dur = (Double) p->value.lfrac.num;
				if (dur<0) dur = -dur;
				dur /= p->value.lfrac.den;
				if (dur>max_dur) max_dur = dur;
			}
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_PLAYBACK_MODE);
			if (!p || (p->value.uint<GF_PLAYBACK_MODE_FASTFORWARD))
				disable_seek = GF_TRUE;
		}

		if (!disable_seek && max_dur) {
			gf_fprintf(sdp_out, "a=range:npt=0-%g\n", max_dur);
		}
	}
	
	if (base_pid_id) {
		gf_fprintf(sdp_out, "a=group:DDP L%d", base_pid_id);
		for (i = 0; i < count; i++) {
			GF_RTPOutStream *st = gf_list_get(streams, i);
			if (st->depends_on == base_pid_id) {
				gf_fprintf(sdp_out, " L%d", i+1);
			}
		}
		gf_fprintf(sdp_out, "\n");
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
		u32 nb_chan = 0;
        const GF_PropertyValue *p;
		GF_RTPOutStream *stream = gf_list_get(streams, i);
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

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) nb_chan = p->value.uint;

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

		gf_rtp_streamer_append_sdp_extended(stream->rtp, stream->id, dsi, dsi_len, dsi_enh, dsi_enh_len, (char *)KMS, w, h, tw, th, tx, ty, tl, nb_chan, is_rtsp, &sdp_media);

		if (sdp_media) {
			gf_fprintf(sdp_out, "%s", sdp_media);
			gf_free(sdp_media);
		} else {
			continue;
		}
		if (base_pid_id) {
			u32 j;

			gf_fprintf(sdp_out, "a=mid:L%d\n", i+1);
			if ((base_pid_id != stream->id) && stream->depends_on) {
				GF_RTPOutStream *dep = stream;
				gf_fprintf(sdp_out, "a=depend:%d lay", gf_rtp_streamer_get_payload_type(stream->rtp) );

				while (dep) {
					GF_RTPOutStream *base=NULL;
					for (j=0; j<count; j++) {
						base = gf_list_get(streams, j);
						if (dep->depends_on == base->id) break;
						base = NULL;
					}
					if (!base) break;

					s32 idx = gf_list_find(streams, base);
					if (idx<0) break;
					gf_fprintf(sdp_out, " L%d:%d", idx+1, gf_rtp_streamer_get_payload_type(base->rtp) );
					dep = base;
				}
				gf_fprintf(sdp_out, "\n");
			}
		}

		if (is_rtsp) {
			gf_fprintf(sdp_out, "a=control:%s=%d\n", stream->ctrl_name ? stream->ctrl_name : "trackID", stream->ctrl_id);
		}
	}
	gf_fprintf(sdp_out, "\n");
	return GF_OK;
}

GF_Err rtpout_init_streamer(GF_RTPOutStream *stream, const char *ipdest, Bool inject_xps, Bool use_mpeg4_signaling, Bool use_latm, u32 payt, u32 mtu, u32 ttl, const char *ifce, Bool is_rtsp, u32 *base_pid_id, u32 file_mode)
{
	Bool disable_mpeg4 = GF_FALSE;
	u32 flags, average_size, max_size, max_tsdelta, codecid, const_dur, nb_ch, samplerate, max_cts_offset, bandwidth, IV_length, KI_length, dsi_len, max_ptime, au_sn_len;
	char *dsi;
	Bool is_crypted, is_enh_dsi = GF_FALSE;
	const GF_PropertyValue *p;

	*base_pid_id = 0;

	dsi_len = samplerate = nb_ch = IV_length = KI_length = 0;
	is_crypted = 0;
	dsi = NULL;
	flags = 0;
	max_ptime = au_sn_len = 0;

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_CODECID);
	codecid = p ? p->value.uint : 0;
	if (stream->codecid && (stream->codecid != codecid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Dynamic change of codec in RTP session not supported !\n"));
		return GF_FILTER_NOT_SUPPORTED;
	}
	stream->codecid = codecid;

	stream->is_encrypted = GF_FALSE;
	if (stream->streamtype == GF_STREAM_ENCRYPTED) {
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ORIG_STREAM_TYPE);
		if (p) stream->streamtype = p->value.uint;
		stream->is_encrypted = GF_TRUE;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
		if (!p || (p->value.uint != GF_ISOM_ISMACRYP_SCHEME)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Protected track with scheme type %s, cannot stream (only ISMA over RTP is supported !\n", p ? gf_4cc_to_str(p->value.uint) : "unknown" ));
			return GF_FILTER_NOT_SUPPORTED;
		}
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_TIMESCALE);
	stream->timescale = p ? p->value.uint : 1000;

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_CONFIG_IDX);
	stream->sample_desc_index = p ? p->value.uint : 0;


	u32 cfg_crc=0;
	dsi = NULL;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DECODER_CONFIG);

	if (!p) {
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
		is_enh_dsi = GF_TRUE;
	}
	if (p) {
		dsi = p->value.data.ptr;
		dsi_len = p->value.data.size;
		cfg_crc = gf_crc_32(dsi, dsi_len);
	}
	if (stream->rtp && (cfg_crc==stream->cfg_crc))
		return GF_OK;

	if (inject_xps)
		stream->inject_ps = GF_TRUE;
	else if (stream->cfg_crc)
		stream->inject_ps = GF_TRUE;
	stream->cfg_crc = cfg_crc;

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_NB_FRAMES);
	stream->nb_aus = p ? p->value.uint : 0;

	switch (stream->streamtype) {
	case GF_STREAM_OD:
	case GF_STREAM_SCENE:
		//todo, check if sync shadow is used
//		if (gf_isom_has_sync_shadows(ctx->isom, stream->track_num) || gf_isom_has_sample_dependency(ctx->isom, stream->track_num))
//			flags |= GP_RTP_PCK_SYSTEMS_CAROUSEL;
		break;
	case GF_STREAM_AUDIO:
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) samplerate = p->value.uint;
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) nb_ch = p->value.uint;
		break;
	case GF_STREAM_VISUAL:
		break;
	case GF_STREAM_FILE:
		if (file_mode==2) {
			stream->codecid = codecid = GF_CODECID_FAKE_MP2T;
			stream->timescale = 90000;
		}
		break;
	default:
		break;
	}

	gf_filter_pid_set_framing_mode(stream->pid, (stream->streamtype==GF_STREAM_FILE) ? GF_FALSE : GF_TRUE);

	/*get sample info*/
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_FRAME_SIZE);
	max_size = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_AVG_FRAME_SIZE);
	average_size = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_TS_DELTA);
	max_tsdelta = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_CTS_OFFSET);
	max_cts_offset = p ? p->value.uint : (u32) -1;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_CONSTANT_DURATION);
	const_dur = p ? p->value.uint : 0;


	if (stream->avcc) gf_odf_avc_cfg_del(stream->avcc);
	stream->avcc = NULL;
	if (stream->hvcc) gf_odf_hevc_cfg_del(stream->hvcc);
	stream->hvcc = NULL;
	if (stream->vvcc) gf_odf_vvc_cfg_del(stream->vvcc);
	stream->vvcc = NULL;

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
	case GF_CODECID_VVC:
		if (dsi) {
			GF_VVCConfig *vvcc = NULL;
			GF_HEVCConfig *hvcc = NULL;
			u8 vps_nut=0,sps_nut=0,pps_nut=0;
			GF_List *param_array = NULL;
			if (codecid == GF_CODECID_VVC) {
				vvcc = gf_odf_vvc_cfg_read(dsi, dsi_len);
				param_array = vvcc->param_array;
				sps_nut = GF_VVC_NALU_SEQ_PARAM;
				pps_nut = GF_VVC_NALU_PIC_PARAM;
				vps_nut = GF_VVC_NALU_VID_PARAM;
				stream->avc_nalu_size = vvcc->nal_unit_size;
			} else {
				hvcc = gf_odf_hevc_cfg_read(dsi, dsi_len, (is_enh_dsi && (codecid==GF_CODECID_LHVC)) ? GF_TRUE : GF_FALSE );
				param_array = hvcc->param_array;
				sps_nut = GF_HEVC_NALU_SEQ_PARAM;
				pps_nut = GF_HEVC_NALU_PIC_PARAM;
				vps_nut = GF_HEVC_NALU_VID_PARAM;
				stream->avc_nalu_size = hvcc->nal_unit_size;
			}
			if (param_array) {
				if (stream->inject_ps) {
					u32 i, count = gf_list_count(param_array);
					GF_NALUFFParamArray *vpsa=NULL, *spsa=NULL, *ppsa=NULL;
					stream->vvcc = vvcc;
					stream->hvcc = hvcc;
					for (i=0; i<count; i++) {
						GF_NALUFFParamArray *pa = gf_list_get(param_array, i);
						if (!vpsa && (pa->type == vps_nut)) {
							vpsa = pa;
							gf_list_rem(param_array, i);
							count--;
						}
						else if (!spsa && (pa->type == sps_nut)) {
							spsa = pa;
							gf_list_rem(param_array, i);
							count--;
						}
						else if (!ppsa && (pa->type == pps_nut)) {
							ppsa = pa;
							gf_list_rem(param_array, i);
							count--;
						}
					}
					//insert PPS at beginning
					if (ppsa) gf_list_insert(param_array, ppsa, 0);
					//insert SPS at beginning
					if (spsa) gf_list_insert(param_array, spsa, 0);
					//insert VPS at beginning - we now have VPS, SPS and other (PPS, SEI...)
					if (vpsa) gf_list_insert(param_array, vpsa, 0);
				} else {
					if (vvcc) gf_odf_vvc_cfg_del(vvcc);
					if (hvcc) gf_odf_hevc_cfg_del(hvcc);
				}
			}
		}
		break;
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		//we cannot disable mpeg4 payload type, compute default values !!
		if (!const_dur || !average_size || !max_tsdelta || !max_size) {
			const_dur = (u32) gf_timestamp_rescale(1024, samplerate, stream->timescale);
			max_tsdelta = const_dur;
			average_size = 500;
			max_size = 1000;
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPOut] AAC stream detected but not information available on average size/tsdelta/duration, assuming const dur %d max_tsdelta %d average size %d max size %d\n", const_dur, max_tsdelta, average_size, max_size));
		}
		if (use_latm)
			flags |= GP_RTP_PCK_USE_LATM_AAC;
		break;
	}

	if (max_cts_offset==(u32)-1) {
		if (stream->streamtype==GF_STREAM_VISUAL) {
			disable_mpeg4 = GF_TRUE;
		}
		max_cts_offset = 0;
	}

	if (!disable_mpeg4 && use_mpeg4_signaling)
		flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;


	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ID);
	stream->id = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_BITRATE);
	bandwidth = p ? p->value.uint : 0;

	if (codecid==GF_CODECID_AAC_MPEG4)

	if (is_crypted) {
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ISMA_SELECTIVE_ENC);
		if (p->value.boolean) {
			flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
		}
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ISMA_IV_LENGTH);
		IV_length = p ? p->value.uint : 0;
		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ISMA_KI_LENGTH);
		KI_length = p ? p->value.uint : 0;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) stream->depends_on = p->value.uint;


	/*init packetizer*/
	stream->rtp = gf_rtp_streamer_new(stream->streamtype, codecid, stream->timescale,
				 (char *) ipdest, stream->port, mtu, ttl, ifce,
				 flags, dsi, dsi_len,
				 payt, samplerate, nb_ch,
				 is_crypted, IV_length, KI_length,
				 average_size, max_size, max_tsdelta, max_cts_offset, const_dur, bandwidth, max_ptime, au_sn_len, is_rtsp);

	if (!stream->rtp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Could not initialize RTP for stream %s:  not supported\n", gf_filter_pid_get_name(stream->pid) ));
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DELAY);
	stream->ts_delay = p ? p->value.longsint : 0;

	stream->rtp_timescale = gf_rtp_streamer_get_timescale(stream->rtp);


	payt++;
	stream->microsec_ts_scale_frac.num = 1000000;
	stream->microsec_ts_scale_frac.den = stream->timescale;

	while (! (stream->microsec_ts_scale_frac.num % 10) && ! (stream->microsec_ts_scale_frac.den % 10)) {
		stream->microsec_ts_scale_frac.num /= 10;
		stream->microsec_ts_scale_frac.den /= 10;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) {
		*base_pid_id = p->value.uint;
		gf_rtp_streamer_disable_auto_rtcp(stream->rtp);
	}
	return GF_OK;
}

static GF_Err rtpout_setup_sdp(GF_RTPOutCtx *ctx)
{
	FILE *sdp_out;
	u32 fsize;
	GF_Err e;
	u64 sess_id=0;
	u8 *output;
	const char *ip = ctx->ip;
	if (!ip) ip = "127.0.0.1";

	if (ctx->single_stream) return GF_OK;

	e = rtpout_create_sdp(ctx->streams, GF_FALSE, ip, ctx->info, "livesession", ctx->url, ctx->email, ctx->base_pid_id, &sdp_out, &sess_id);
	if (e) return e;

	fsize = (u32) gf_ftell(sdp_out);
	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, fsize, &output);
	if (pck) {
		gf_fseek(sdp_out, 0, SEEK_SET);
		u32 read = (u32) gf_fread(output, fsize, sdp_out);
		if (read != fsize) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Failed to read SDP from temp file, got %d bytes but expecting %d\n", read, fsize));
			gf_filter_pck_discard(pck);
			e = GF_IO_ERR;
		} else {
			char c = output[fsize-1];
			output[fsize-1] = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] SDP file generated: %s\n", output));
			output[fsize-1] = c;
			gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_send(pck);
			e = GF_OK;
		}
	} else {
		e = GF_OUT_OF_MEM;
	}
	gf_fclose(sdp_out);
	return e;
}

static u16 rtpout_check_next_port(GF_RTPOutCtx *ctx, u16 first_port)
{
	u32 i, count = gf_list_count(ctx->streams);
	for (i=0;i<count; i++) {
		GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
		if (stream->port==first_port) {
			return rtpout_check_next_port(ctx, (u16) (first_port+2) );
		}
	}
	return first_port;
}

void rtpout_del_stream(GF_RTPOutStream *st)
{
	if (st->rtp) gf_rtp_streamer_del(st->rtp);
	if (st->has_pck) gf_filter_pid_drop_packet(st->pid);
	if (st->avcc)
		gf_odf_avc_cfg_del(st->avcc);
	if (st->hvcc)
		gf_odf_hevc_cfg_del(st->hvcc);
	if (st->vvcc)
		gf_odf_vvc_cfg_del(st->vvcc);
	gf_free(st);
}

static GF_Err rtpout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_RTPOutCtx *ctx = (GF_RTPOutCtx *) gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	u16 first_port;
	u32 streamType, payt;
	const GF_PropertyValue *p;

	first_port = ctx->port;

	if (is_remove) {
		GF_RTPOutStream *t =gf_filter_pid_get_udta(pid);
		if (t) {
			if (ctx->active_stream==t) ctx->active_stream = NULL;
			gf_list_del_item(ctx->streams, t);
			rtpout_del_stream(t);
		}
		if (!gf_list_count(ctx->streams)) {
			if (ctx->opid) gf_filter_pid_set_eos(ctx->opid);
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
			rtpout_del_stream(stream);
		}
		if (!ctx->dst)
			return GF_FILTER_NOT_SUPPORTED;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && p->value.string && !strcmp(p->value.string, "video/mpeg-2")) {
			ctx->single_stream = 2;
		} else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string && !strcmp(p->value.string, "ts")) {
				ctx->single_stream = 2;
			} else {
				return GF_FILTER_NOT_SUPPORTED;
			}
		}

		break;
	default:
		break;
	}
	if (!stream) {
		GF_SAFEALLOC(stream, GF_RTPOutStream);
		if (!stream) return GF_OUT_OF_MEM;
		gf_list_add(ctx->streams, stream);
		stream->pid = pid;
		stream->streamtype = streamType;
		stream->min_dts = GF_FILTER_NO_TS;
	 	gf_filter_pid_set_udta(pid, stream);
		if (ctx->single_stream) {
			GF_FilterEvent evt;
			gf_filter_pid_init_play_event(pid, &evt, 0, 1.0, "RTPOut");
			gf_filter_pid_send_event(pid, &evt);
		}
	}
	if (!ctx->single_stream) {
		if (!ctx->opid) {
			ctx->opid = gf_filter_pid_new(filter);
		}
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("sdp") );
		gf_filter_pid_set_name(ctx->opid, "SDP");
	} else {
		char *dst = ctx->dst+6;
		char *sep = strchr(dst, ':');
		if (sep) {
			first_port = ctx->port = atoi(sep+1);
			sep[0] = 0;
			if (ctx->ip) gf_free(ctx->ip);
			ctx->ip = gf_strdup(dst);
			sep[0] = ':';
		}
	}

	stream->port = rtpout_check_next_port(ctx, first_port);

	payt = ctx->payt + gf_list_find(ctx->streams, stream);
	//init rtp
	e = rtpout_init_streamer(stream,  ctx->ip ? ctx->ip : "127.0.0.1", ctx->xps, ctx->mpeg4, ctx->latm, payt, ctx->mtu, ctx->ttl, ctx->ifce, GF_FALSE, &ctx->base_pid_id, ctx->single_stream);
	if (e) return e;

	stream->selected = GF_TRUE;

	if (ctx->loop) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
		if (!p || (p->value.uint<GF_PLAYBACK_MODE_FASTFORWARD)) {
			ctx->loop = GF_FALSE;
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] PID %s cannot be seek, disabling loop\n", gf_filter_pid_get_name(pid) ));
		}
	}
	return GF_OK;
}

static GF_Err rtpout_initialize(GF_Filter *filter)
{
	GF_RTPOutCtx *ctx = (GF_RTPOutCtx *) gf_filter_get_udta(filter);
	if (!ctx->payt) ctx->payt = 96;
	if (!ctx->port) ctx->port = 7000;
	if (!ctx->mtu) ctx->mtu = 1450;
	if (ctx->payt<96) ctx->payt = 96;
	if (ctx->payt>127) ctx->payt = 127;
	ctx->streams = gf_list_new();

	if (ctx->dst && (ctx->ext || ctx->mime) ) {
		//static cap, streamtype = file
		ctx->in_caps[0].code = GF_PROP_PID_STREAM_TYPE;
		ctx->in_caps[0].val = PROP_UINT(GF_STREAM_FILE);
		ctx->in_caps[0].flags = GF_CAPS_INPUT_STATIC;

		if (ctx->mime) {
			ctx->in_caps[1].code = GF_PROP_PID_MIME;
			ctx->in_caps[1].val = PROP_NAME( ctx->mime );
			ctx->in_caps[1].flags = GF_CAPS_INPUT;
		} else {
			strncpy(ctx->szExt, ctx->ext, 9);
			ctx->szExt[9] = 0;
			strlwr(ctx->szExt);
			ctx->in_caps[1].code = GF_PROP_PID_FILE_EXT;
			ctx->in_caps[1].val = PROP_NAME( ctx->szExt );
			ctx->in_caps[1].flags = GF_CAPS_INPUT;
		}
		gf_filter_override_caps(filter, ctx->in_caps, 2);
		gf_filter_set_max_extra_input_pids(filter, 0);
		ctx->single_stream = GF_TRUE;
	}
	return GF_OK;
}

static void rtpout_finalize(GF_Filter *filter)
{
	GF_RTPOutCtx *ctx = (GF_RTPOutCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		GF_RTPOutStream *tmp = gf_list_pop_back(ctx->streams);
		rtpout_del_stream(tmp);
	}
	gf_list_del(ctx->streams);

}

static GF_Err rtpout_send_xps(GF_RTPOutStream *stream, GF_List *pslist, Bool *au_start, u32 pck_size, u32 cts, u32 dts, u32 duration)
{
	GF_Err e;
	u32 i, count = gf_list_count(pslist);
	for (i=0; i<count; i++) {
		GF_NALUFFParam *sl = gf_list_get(pslist, i);
		e = gf_rtp_streamer_send_data(stream->rtp, (char *) sl->data, sl->size, pck_size, cts, dts, stream->current_sap ? 1 : 0, *au_start, GF_FALSE, stream->pck_num, duration, stream->sample_desc_index);
		if (e) return e;
		*au_start = GF_FALSE;
	}
	return GF_OK;
}

static Bool rtpout_init_clock(GF_RTPOutCtx *ctx)
{
	GF_Err e;
	u64 min_dts = GF_FILTER_NO_TS;
	u32 i, count = gf_list_count(ctx->streams);

	for (i=0; i<count; i++) {
		u64 dts;
		GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(stream->pid);
		if (!pck) return GF_FALSE;

		dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS)
			dts = gf_filter_pck_get_cts(pck);

		if (dts==GF_FILTER_NO_TS) dts=0;

		dts = gf_timestamp_rescale(dts, stream->timescale, 1000000);
		if (min_dts > dts)
			min_dts = dts;

		if (ctx->tso>0) {
			u64 offset = gf_timestamp_rescale(ctx->tso, 1000000, stream->timescale);
			stream->rtp_ts_offset = (u32) offset;
		}
	}
	ctx->sys_clock_at_init = gf_sys_clock_high_res();
	ctx->microsec_ts_init = min_dts;
	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP clock initialized - time origin set to "LLU" us (sys clock) / "LLU" us (media clock)\n", ctx->sys_clock_at_init, ctx->microsec_ts_init));
	if (ctx->tso<0) {
		gf_rand_init(GF_FALSE);
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
			stream->rtp_ts_offset = gf_rand();
			while (stream->rtp_ts_offset>0xFFFFFFF)
				stream->rtp_ts_offset/=2;
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP stream %d initial RTP TS set to %d\n", i+1, stream->rtp_ts_offset));
		}
	}

	e = rtpout_setup_sdp(ctx);
	if (e) return e;

	if (ctx->runfor==0) {
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
			gf_filter_pid_set_discard(stream->pid, GF_TRUE);
		}
	}

	return GF_TRUE;
}

static void rtpout_process_rtcp(void *cbk, u32 ssrc, u32 rtt_ms, u64 jitter_rtp_ts, u32 loss_rate)
{
	GF_RTPOutStream *stream = cbk;
	if (ssrc) return;
	u64 jitter_us = gf_timestamp_rescale(jitter_rtp_ts, stream->rtp_timescale, 1000000);
	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTCP stats for PID %s: rtt %u ms jitter "LLU" us loss rate %d / 1000\n\n", gf_filter_pid_get_name(stream->pid), rtt_ms, jitter_us, loss_rate));

	gf_filter_pid_set_rt_stats(stream->pid, rtt_ms, (u32) jitter_us, loss_rate);

	if (stream->on_rtcp) {
		stream->on_rtcp(stream->on_rtcp_udta);
	}
}


GF_Err rtpout_process_rtp(GF_List *streams, GF_RTPOutStream **active_stream, Bool loop, s32 delay, u32 *active_stream_idx, u64 sys_clock_at_init, u64 *active_min_ts_microsec, u64 microsec_ts_init, Bool *wait_for_loop, u32 *repost_delay_us, Bool *first_RTCP_sent, u32 base_pid_id)
{
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	u32 duration, i, count;
	s64 diff;
	u64 clock;
	const char *pck_data;
	u32 pck_size=0;
	u32 dts, cts;

	/*browse all inputs and locate most mature stream*/
	if (! *active_stream) {
		u32 nb_eos = 0;
		count = gf_list_count(streams);

		*active_min_ts_microsec = (u64) -1;
		for (i=0; i<count; i++) {
			stream = gf_list_get(streams, i);
			if (!stream->rtp) {
				if (stream->state==RTPOUT_STREAM_STOP) nb_eos++;
				continue;
			}

			//process rtcp (we only do that once we are done sending a full au
			gf_rtp_streamer_read_rtcp(stream->rtp, rtpout_process_rtcp, stream);

			/*load next AU*/
			if (!stream->has_pck) {
				u64 ts;
				GF_FilterPacket *pck = gf_filter_pid_get_packet(stream->pid);

				if (!pck) {
					if (gf_filter_pid_is_eos(stream->pid) || (stream->state==RTPOUT_STREAM_STOP)) {
						//flush stream
						if (!stream->bye_sent) {
							stream->bye_sent = GF_TRUE;
							if (stream->rtp) {
								gf_rtp_streamer_send_au(stream->rtp, NULL, 0, 0, 0, GF_FALSE);
								gf_rtp_streamer_send_bye(stream->rtp);
							}
						}
						nb_eos++;
					}
					continue;
				}
				stream->current_dts = gf_filter_pck_get_dts(pck);
				//if CTS is not set, use prev packet CTS
				ts = gf_filter_pck_get_cts(pck);
				if (ts==GF_FILTER_NO_TS) ts = stream->current_cts;
				stream->current_cts = ts;

				stream->current_sap = gf_filter_pck_get_sap(pck);
				duration = gf_filter_pck_get_duration(pck);
				if (duration) stream->current_duration = duration;
				if (stream->current_dts==GF_FILTER_NO_TS)
					stream->current_dts = stream->current_cts;

				if (stream->min_dts==GF_FILTER_NO_TS) {
					stream->min_dts = stream->current_dts;
				}

				stream->microsec_dts = (u64) (stream->microsec_ts_scale_frac.num * (s64) stream->current_dts);
				stream->microsec_dts /= stream->microsec_ts_scale_frac.den;
				stream->microsec_dts += stream->microsec_ts_offset + sys_clock_at_init;

				if (stream->microsec_dts < microsec_ts_init) {
					stream->microsec_dts = 0;
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPOut] next RTP packet (stream %d) has a timestamp "LLU" less than initial timestamp "LLU" - forcing to 0\n", i+1, stream->microsec_dts, microsec_ts_init));
				} else {
					stream->microsec_dts -= microsec_ts_init;
				}

				if (stream->current_sap>GF_FILTER_SAP_3) stream->current_sap = 0;
				stream->pck_num++;
				*wait_for_loop = GF_FALSE;
				stream->has_pck = GF_TRUE;
			}

			/*check timing*/
			if (stream->has_pck) {
				if (*active_min_ts_microsec > stream->microsec_dts) {
					*active_min_ts_microsec = stream->microsec_dts;
					*active_stream = stream;
					*active_stream_idx = i+1;
				}
			}
		}

		/*no input data ...*/
		if (! *active_stream) {
			if (nb_eos==count) {
				u64 max_dur = 0;
				if (!loop)
					return GF_EOS;

				for (i=0; i<count; i++) {
					stream = gf_list_get(streams, i);
					u64 dur = stream->current_dts + stream->current_duration - stream->min_dts;

					dur = gf_timestamp_rescale(dur, stream->timescale, 1000000);

					if (max_dur < dur) {
						max_dur = dur;
					}
				}
				if (*wait_for_loop)
					return GF_OK;

				GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP session done, looping source\n"));
				*wait_for_loop = GF_TRUE;
				for (i=0; i<count; i++) {
					GF_FilterEvent evt;
					const GF_PropertyValue *p;
					stream = gf_list_get(streams, i);
					p = gf_filter_pid_get_property(stream->pid, GF_PROP_NO_TS_LOOP);
					if (!p || !p->value.boolean) {
						u64 new_ts = gf_timestamp_rescale(max_dur, 1000000, stream->timescale);
						stream->ts_offset += new_ts;
						stream->microsec_ts_offset = (u64) gf_timestamp_rescale(stream->ts_offset, stream->timescale, 1000000) + sys_clock_at_init;
					}

					//loop pid: stop and play
					GF_FEVT_INIT(evt, GF_FEVT_STOP, stream->pid);
					gf_filter_pid_send_event(stream->pid, &evt);

					GF_FEVT_INIT(evt, GF_FEVT_PLAY, stream->pid);
					gf_filter_pid_send_event(stream->pid, &evt);
				}
			}
			return GF_OK;
		}
	}

	stream = *active_stream;
	clock = gf_sys_clock_high_res();
	diff = (s64) *active_min_ts_microsec;
	diff += ((s64) delay) * 1000;
	diff -= (s64) clock;

	if (diff > 1000) {
		u64 repost_in;
		//if more than 11 secs ahead of time, ask for delay minus one second, otherwise ask for half the delay
		if (diff<=11000) repost_in = diff/3;
		else repost_in = diff - 10000;
		*repost_delay_us = (u32) repost_in;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTPOut] next RTP packet (stream %d DTS "LLU") scheduled in "LLU" us, requesting filter reschedule in "LLU" us - clock "LLU" us\n", *active_stream_idx, stream->current_dts, diff, repost_in, clock));
		return GF_OK;
	} else if (diff<=-2000) {
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP session stream %d - sending packet %d (DTS "LLU") too late by %d us - clock "LLU" us\n", *active_stream_idx, stream->pck_num, stream->current_dts, -diff, clock));
		*repost_delay_us=0;
	} else if (diff>0){
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTPOut] RTP session stream %d - sending packet %d (DTS "LLU") ahead of %d us - clock "LLU" us\n", *active_stream_idx, stream->pck_num, stream->current_dts, diff, clock));
	}

	/*send packets*/
	GF_FilterPacket *pck = gf_filter_pid_get_packet(stream->pid);
	pck_data = pck ? gf_filter_pck_get_data(pck, &pck_size) : NULL;
	if (!pck_size) {
		gf_filter_pid_drop_packet(stream->pid);
		stream->has_pck = GF_FALSE;
		*active_stream = NULL;
		return GF_OK;
	}

	dts = (u32) (stream->current_dts + stream->ts_offset);
	cts = (u32) (stream->current_cts + stream->ts_offset);
	duration = stream->current_duration;

	dts += stream->rtp_ts_offset;
	cts += stream->rtp_ts_offset;
	if (stream->ts_delay>=0) {
		dts += (u32) stream->ts_delay;
		cts += (u32) stream->ts_delay;
	} else {
		if ((s32) dts >= -stream->ts_delay)
			dts += (s32) stream->ts_delay;
		else
			dts = 0;

		if ((s32) cts >= -stream->ts_delay )
			cts += (s32) stream->ts_delay;
		else
			cts = 0;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTPOut] Sending RTP packets for stream %d pck %d/%d DTS "LLU" - CTS "LLU" - RTP TS "LLU" - size %d - SAP %d - clock "LLU" us\n", *active_stream_idx, stream->pck_num, stream->nb_aus, stream->current_dts, stream->current_dts, cts, pck_size, stream->current_sap, clock) );
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] Runtime %08u ms send stream %d pck %08u/%08u DTS %08u CTS %08u RTP-TS %08u size %08u SAP %d\r", (u32) (clock - sys_clock_at_init)/1000, *active_stream_idx, stream->pck_num, stream->nb_aus, (u32) stream->current_dts, (u32) stream->current_dts, (u32) cts, pck_size, stream->current_sap) );
	}
#endif

	/*we are about to send scalable base: trigger RTCP reports with the same NTP. This avoids
	NTP drift due to system clock precision which could break sync decoding*/
	if (! *first_RTCP_sent || (base_pid_id && (base_pid_id== stream->id) )) {
		u32 ntp_sec, ntp_frac;
		/*force sending RTCP SR every RAP ? - not really compliant but we cannot perform scalable tuning otherwise*/
		u32 ntp_type = stream->current_sap ? 2 : 1;
		gf_net_get_ntp(&ntp_sec, &ntp_frac);
		count = gf_list_count(streams);

		for (i=0; i<count; i++) {
			GF_RTPOutStream *astream = gf_list_get(streams, i);
			if (!astream->has_pck) break;

			u32 ts = (u32) (astream->current_cts + astream->ts_offset + astream->rtp_ts_offset);
			gf_rtp_streamer_send_rtcp(stream->rtp, GF_TRUE, ts, ntp_type, ntp_sec, ntp_frac);
		}
		*first_RTCP_sent = GF_TRUE;
	}

	/*unpack nal units*/
	if (stream->avc_nalu_size) {
		Bool au_start, au_end;
		u32 v, size;
		u32 remain = pck_size;
		const char *ptr = pck_data;

		au_start = 1;
		e = GF_OK;

		if (stream->avcc && stream->current_sap) {
			e = rtpout_send_xps(stream, stream->avcc->sequenceParameterSets, &au_start, pck_size, cts, dts, duration);
			if (!e)
				e = rtpout_send_xps(stream, stream->avcc->sequenceParameterSetExtensions, &au_start, pck_size, cts, dts, duration);

			if (!e)
				e = rtpout_send_xps(stream, stream->avcc->pictureParameterSets, &au_start, pck_size, cts, dts, duration);
		}
		else if ((stream->hvcc||stream->vvcc) && stream->current_sap) {
			GF_List *ar_list = stream->vvcc ? stream->vvcc->param_array : stream->hvcc->param_array;
			u32 nbps = gf_list_count(ar_list);
			for (i=0; i<nbps; i++) {
				GF_NALUFFParamArray *pa = gf_list_get(ar_list, i);
				if (!e)
					e = rtpout_send_xps(stream, pa->nalus, &au_start, pck_size, cts, dts, duration);
			}
		}

		while (!e && remain) {
			size = 0;
			v = (*active_stream)->avc_nalu_size;
			while (v) {
				size |= (u8) *ptr;
				ptr++;
				remain--;
				v-=1;
				if (v) size<<=8;
			}
			if (remain < size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Broken AVC nalu encapsulation: NALU size is %d but only %d bytes left in sample %d\n", size, remain, (*active_stream)->pck_num));
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
	stream->has_pck = GF_FALSE;

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Error sending RTP packet %d: %s\n", stream->pck_num, gf_error_to_string(e) ));
	}
	*active_stream = NULL;
	return e;

}

static GF_Err rtpout_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	u32 repost_delay_us=0;
	GF_RTPOutCtx *ctx = gf_filter_get_udta(filter);

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!ctx->sys_clock_at_init) {
		if (!rtpout_init_clock(ctx)) return GF_OK;
	}

	if (ctx->runfor>0) {
		s64 diff = gf_sys_clock_high_res();
		diff -= ctx->sys_clock_at_init;
		diff /= 1000;
		if ((s32) diff > ctx->runfor) {
			u32 i, count = gf_list_count(ctx->streams);
			for (i=0; i<count; i++) {
				GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
				gf_filter_pid_set_discard(stream->pid, GF_TRUE);
				stream->has_pck = GF_FALSE;
			}
			if (ctx->opid) gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
	}

	e = rtpout_process_rtp(ctx->streams, &ctx->active_stream, ctx->loop, ctx->delay, &ctx->active_stream_idx, ctx->sys_clock_at_init, &ctx->active_min_ts_microsec, ctx->microsec_ts_init, &ctx->wait_for_loop, &repost_delay_us, &ctx->first_RTCP_sent, ctx->base_pid_id);
	if (e) return e;

	if (repost_delay_us)
		gf_filter_ask_rt_reschedule(filter, repost_delay_us);

	return GF_OK;
}

static GF_FilterProbeScore rtpout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "rtp://", 6)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static const GF_FilterCapability RTPOutCaps[] =
{
	//anything else (not file and framed) result in manifest PID
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "sdp"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "application/sdp"),
	{0},
	//anything else (not file and framed) result in media pids not file
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ts|m2t|mts|dmb|trp"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/mpeg-2|video/mp2t|video/mpeg"),
};


#define OFFS(_n)	#_n, offsetof(GF_RTPOutCtx, _n)
static const GF_FilterArgs RTPOutArgs[] =
{
	{ OFFS(ip), "destination IP address (NULL is 127.0.0.1)", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(port), "port for first stream in session", GF_PROP_UINT, "7000", NULL, 0},
	{ OFFS(loop), "loop all streams in session (not always possible depending on source type)", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(mpeg4), "send all streams using MPEG-4 generic payload format if possible", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mtu), "size of RTP MTU in bytes", GF_PROP_UINT, "1460", NULL, 0},
	{ OFFS(ttl), "time-to-live for multicast packets", GF_PROP_UINT, "2", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ifce), "default network interface to use", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(payt), "payload type to use for dynamic decoder configurations", GF_PROP_UINT, "96", "96-127", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(delay), "send delay for packet (negative means send earlier)", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(tt), "time tolerance in microseconds. Whenever schedule time minus realtime is below this value, the packet is sent right away", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(runfor), "run for the given time in ms. Negative value means run for ever (if loop) or source duration, 0 only outputs the sdp", GF_PROP_SINT, "-1", NULL, 0},
	{ OFFS(tso), "set timestamp offset in microseconds. Negative value means random initial timestamp", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(xps), "force parameter set injection at each SAP. If not set, only inject if different from SDP ones", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(latm), "use latm for AAC payload format", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dst), "URL for direct RTP mode", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(ext), "file extension for direct RTP mode", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mime), "set mime type for direct RTP mode", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister RTPOutRegister = {
	.name = "rtpout",
	GF_FS_SET_DESCRIPTION("RTP Streamer")
	GF_FS_SET_HELP("The RTP streamer handles SDP/RTP output streaming.\n"
	"# SDP mode\n"
	"When the destination URL is an SDP, the filter outputs an SDP on a file PID and streams RTP packets over UDP, starting from the indicated [-port]().\n"
	"# Direct RTP mode\n"
	"When the destination URL uses the protocol scheme `rtp://IP:PORT`, the filter does not output any SDP and streams a single input over RTP, using PORT indicated in the destination URL, or the first [-port]() configured.\n"
	"In this mode, it is usually needed to specify the desired format using [-ext]() or [-mime]().\n"
	"EX gpac -i src -o rtp://localhost:1234/:ext=ts\n"
	"This will indicate that the RTP streamer expects a MPEG-2 TS mux as an input.\n"
	"# RTP Packets\n"
	"The RTP packets produced have a maximum payload set by the [-mtu]() option (IP packet will be MTU + 40 bytes of IP+UDP+RTP headers).\n"
	"The real-time scheduling algorithm works as follows:\n"
	"- first initialize the clock by:\n"
	"  - computing the smallest timestamp for all input PIDs\n"
	"  - mapping this media time to the system clock\n"
	"- determine the earliest packet to send next on each input PID, adding [-delay]() if any\n"
	"- finally compare the packet mapped timestamp __TS__ to the system clock __SC__. When __TS__ - __SC__ is less than [-tt](), the RTP packets for the source packet are sent\n"
	"\n"
	"The filter does not check for RTCP timeout and will run until all input PIDs reach end of stream.\n"
	)
	.private_size = sizeof(GF_RTPOutCtx),
	.max_extra_pids = -1,
	.args = RTPOutArgs,
	//dynamic redirect since RTP may be dynamically loaded when solving .sdp destinations
	.flags = GF_FS_REG_DYNAMIC_REDIRECT | GF_FS_REG_TEMP_INIT,
	.initialize = rtpout_initialize,
	.finalize = rtpout_finalize,
	SETCAPS(RTPOutCaps),
	.configure_pid = rtpout_configure_pid,
	.probe_url = rtpout_probe_url,
	.process = rtpout_process
};


const GF_FilterRegister *rtpout_register(GF_FilterSession *session)
{
	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_out_proto", RTPOutRegister.name, "rtp");
	}
	return &RTPOutRegister;
}

#else

const GF_FilterRegister *rtpout_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /* !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING) */
