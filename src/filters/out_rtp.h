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

#ifndef _OUT_RTP_H_
#define _OUT_RTP_H_

/*module interface*/
#include <gpac/filters.h>
#include <gpac/constants.h>
#ifndef GPAC_DISABLE_STREAMING

#include <gpac/base_coding.h>
#include <gpac/mpeg4_odf.h>
/*IETF lib*/
#include <gpac/internal/ietf_dev.h>



typedef struct
{
	GF_RTPStreamer *rtp;
	u16 port;

	/*scale from TimeStamps in media timescales to TimeStamps in microseconds*/
	GF_Fraction64 microsec_ts_scale_frac;

	u32 id, codecid;
	Bool is_encrypted;

	/*NALU size for H264/AVC parsing*/
	u32 avc_nalu_size;

	GF_FilterPid *pid;
	u32 streamtype;
	u32 timescale;
	u32 nb_aus;
	Bool is_playing;

	u32 depends_on;
	u32 cfg_crc;


	/*loaded AU info*/
	Bool has_pck;
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
	GF_VVCConfig *vvcc;
	u32 rtp_ts_offset;

	s64 ts_delay;
	Bool bye_sent;

	/*RTSP state*/
	Bool selected, send_rtpinfo;
	u32 ctrl_id;
	u32 rtp_id, rtcp_id;
	u32 mcast_port;

	u32 rtp_timescale;
} GF_RTPOutStream;

GF_Err rtpout_create_sdp(GF_List *streams, Bool is_rtsp, const char *ip, const char *info, const char *sess_name, const char *url, const char *email, u32 base_pid_id, FILE **sdp_tmp, u64 *session_id);

GF_Err rtpout_init_streamer(GF_RTPOutStream *stream, const char *ipdest, Bool inject_xps, Bool use_mpeg4_signaling, Bool use_latm, u32 payt, u32 mtu, u32 ttl, const char *ifce, Bool is_rtsp, u32 *base_pid_id, u32 file_mode);

GF_Err rtpout_process_rtp(GF_List *streams, GF_RTPOutStream **active_stream, Bool loop, s32 delay, u32 *active_stream_idx, u64 sys_clock_at_init, u64 *active_min_ts_microsec, u64 microsec_ts_init, Bool *wait_for_loop, u32 *repost_delay_us, Bool *first_RTCP_sent, u32 base_pid_id);


void rtpout_del_stream(GF_RTPOutStream *st);

#endif /*GPAC_DISABLE_STREAMING*/

#endif //_IN_RTP_H_
