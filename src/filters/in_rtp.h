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

#ifndef _IN_RTP_H_
#define _IN_RTP_H_

/*module interface*/
#include <gpac/filters.h>
#include <gpac/constants.h>
#ifndef GPAC_DISABLE_STREAMING

#include <gpac/base_coding.h>
#include <gpac/mpeg4_odf.h>
/*IETF lib*/
#include <gpac/internal/ietf_dev.h>


#define RTSP_BUFFER_SIZE		5000

typedef struct _rtsp_session GF_RTPInRTSP;
typedef struct __rtpin_stream GF_RTPInStream;

/*the rtsp/rtp client*/
typedef struct
{
	//opts
	char *src;
	u32 firstport, ttl, satip_port;
	const char *ifce, *force_mcast, *user_agent, *languages;
	Bool use_client_ports;
	u32 bandwidth, reorder_len, reorder_delay, nat_keepalive, block_size;
	Bool disable_rtcp;
	u32 default_port;
	u32 rtsp_timeout, udp_timeout, rtcp_timeout, stats;
	/*transport mode. 0 is udp, 1 is tcp, 3 is tcp if unreliable media */
	u32 interleave;
	s32 max_sleep;
	Bool autortsp, rtcpsync;

	//internal

	GF_Filter *filter;
	
	/*the service we're responsible for*/
	GF_FilterPid *ipid;
	u32 sdp_url_crc;
	Bool sdp_loaded;

	/*one and only RTSP session*/
	GF_RTPInRTSP *session;

	/*RTP/RTCP media streams*/
	GF_List *streams;

	/*RTSP config*/

	/*packet drop emulation*/
	u32 first_packet_drop;
	u32 frequency_drop;

	/*for single-object control*/
	u32 stream_type;

	GF_Descriptor *iod_desc;

	GF_RTPInStream *postponed_play_stream;
	GF_FEVT_Play postponed_play;

	/*if set ANNOUNCE (sent by server) will be handled*/
//	Bool handle_announce;

	Double last_ntp;

	Bool is_scalable, done, retry_tcp;

	Double last_start_range;

	u32 cur_mid;

	u32 min_frame_dur_ms;

	GF_SockGroup *sockgroup;
	Bool is_eos;
	u32 eos_probe_start;
} GF_RTPIn;

enum
{
	RTSP_AGG_CONTROL = 1,
	RTSP_TCP_FLUSH = 1<<1,
	RTSP_FORCE_INTER = 1<<2,
	RTSP_WAIT_REPLY = 1<<3,
	RTSP_DSS_SERVER = 1<<4,
	RTSP_AGG_ONLY = 1<<5,
};

/*rtsp session*/
struct _rtsp_session
{
	u32 flags;

	/*owner*/
	GF_RTPIn *rtpin;

	/*RTSP session object*/
	GF_RTSPSession *session;
	/*session ID for aggregated stream control*/
	char *session_id;

	/*session control string*/
	char *control;

	/*response object*/
	GF_RTSPResponse *rtsp_rsp;

	Double last_range;
	u32 command_time;
	GF_List *rtsp_commands;
	GF_Err connect_error;

	/*SAT>IP uses a non-conformant version of RTSP*/
	Bool satip;
	char *satip_server;
};

/*creates new RTSP session handler*/
GF_RTPInRTSP *rtpin_rtsp_new(GF_RTPIn *rtp, char *session_control);
/*disconnects and destroy RTSP session handler - if immediate_shutdown do not wait for response*/
void rtpin_rtsp_del(GF_RTPInRTSP *sess);
/*check session by control string*/
GF_RTPInRTSP *rtpin_rtsp_check(GF_RTPIn *rtp, char *control);

void rtpin_rtsp_process_commands(GF_RTPInRTSP *sess);

void rtpin_send_message(GF_RTPIn *ctx, GF_Err e, const char *message);

/*RTP channel state*/
enum
{
	/*channel is setup and waits for connection request*/
	RTP_Setup,
	/*waiting for server reply*/
	RTP_WaitingForAck,
	/*connection OK*/
	RTP_Connected,
	/*data exchange on this service/channel*/
	RTP_Running,
	/*deconnection OK - a download channel can automatically disconnect when download is done*/
	RTP_Disconnected,
	/*service/channel is not (no longer) available/found and should be removed*/
	RTP_Unavailable,
	/*service/channel is not (no longer) available/found and should be removed*/
	RTP_WaitForTCPRetry,
};


/*rtp channel flags*/
enum
{
	/*static RTP channel flags*/

	/*set if sending RTCP reports is enabled (default)*/
	RTP_ENABLE_RTCP = 1,
	/*set if stream control possible*/
	RTP_HAS_RANGE = (1<<1),
	/*set if RTP over RTSP*/
	RTP_INTERLEAVED = (1<<2),
	/*broadcast emultaion is on (no time control for stream)*/
	RTP_FORCE_BROADCAST = (1<<3),

	/*RTP channel runtime flags*/

	/*set if next command (PLAY/PAUSE) is to be skipped (aggregation control)*/
	RTP_SKIP_NEXT_COM = (1<<4),
	/*indicates whether channel creation has been acknowledged or not
	this is needed to filter real channel_connect calls from RTSP re-setup (after STOP) ones*/
	RTP_CONNECTED = (1<<5),
	/*EOS signaled (RTCP or range-based)*/
	RTP_EOS = (1<<6),
	RTP_EOS_FLUSHED = (1<<7),
};

enum
{
	RTP_SET_TIME_NONE = 0,
	RTP_SET_TIME_RTP,
	RTP_SET_TIME_RTP_SEEK,
};

/*rtp channel*/
struct __rtpin_stream
{
	/*module*/
	GF_RTPIn *rtpin;

	/*channel flags*/
	u32 flags;

	/*control session (may be null)*/
	GF_RTPInRTSP *rtsp;
	/*session ID for independent stream control*/
	char *session_id;

	/*RTP channel*/
	GF_RTPChannel *rtp_ch;

	/*depacketizer*/
	GF_RTPDepacketizer *depacketizer;

	GF_FilterPid *opid;

	u32 status;

	u32 last_stats_time;
	
	u32 ES_ID, OD_ID;
	char *control;

	/*rtp receive buffer*/
	char *buffer;
	/*set at play/seek stages to sync app NPT to RTP time (RTSP) or NTP to RTP (RTCP)
	*/
	u32 check_rtp_time;

	/*can we control the stream ?*/
	Double range_start, range_end;
	/*current start time in npt (for pause/resume)*/
	Double current_start;

	Bool paused;
	Bool rtcp_init;
	/*UDP time-out detection*/
	u32 last_udp_time;
	/*RTP stats*/
	u32 rtp_bytes, rtcp_bytes, stat_start_time, stat_stop_time;
	u32 ts_res;

	//MPEG-4 deinterleaver
	Bool first_in_rtp_pck;
	GF_List *pck_queue;

	/*stream id*/
	u32 mid;

	u32 prev_stream;
	u32 next_stream;
	u32 base_stream;

	u32 rtcp_check_start;
	u32 first_rtp_ts;
	s64 ts_adjust;
	//source NTP of first packet received, ercomputed at first RTCP sender report
	u64 init_ntp_us;

	u32 min_dur_us, min_dur_rtp;
	u32 prev_cts;

	u32 sr, nb_ch;
};

/*creates new RTP stream from SDP info*/
GF_RTPInStream *rtpin_stream_new(GF_RTPIn *rtp, GF_SDPMedia *media, GF_SDPInfo *sdp, GF_RTPInStream *input_stream);
/*creates new SAT>IP stream*/
GF_RTPInStream *rtpin_stream_new_satip(GF_RTPIn *rtp, const char *server_ip);
/*destroys RTP stream */
void rtpin_stream_del(GF_RTPInStream *stream);
/*resets stream state and inits RTP sockets if ResetOnly is false*/
GF_Err rtpin_stream_init(GF_RTPInStream *stream, Bool ResetOnly);

void rtpin_stream_reset_queue(GF_RTPInStream *stream);

/*RTSP -> RTP de-interleaving callback*/
GF_Err rtpin_rtsp_data_cbk(GF_RTSPSession *sess, void *cbck, u8 *buffer, u32 bufferSize, Bool IsRTCP);
/*send confirmation of connection - if no error, also setup SL based on payload*/
void rtpin_stream_ack_connect(GF_RTPInStream *stream, GF_Err e);

/*locate RTP stream by channel or ES_ID or control*/
GF_RTPInStream *rtpin_find_stream(GF_RTPIn *rtp, GF_FilterPid *opid, u32 ES_ID, char *es_control, Bool remove_stream);
/*adds channel to session identified by session_control. If no session exists, the session is created if needed*/
GF_Err rtpin_add_stream(GF_RTPIn *rtp, GF_RTPInStream *stream, char *session_control);
/*removes stream from session*/
void rtpin_remove_stream(GF_RTPIn *rtp, GF_RTPInStream *stream);
/*reads input socket and process*/
u32 rtpin_stream_read(GF_RTPInStream *stream);

/*load SDP and setup described media in SDP. If stream is null this is the root
SDP and IOD will be extracted, otherwise this a channel SDP*/
void rtpin_load_sdp(GF_RTPIn *rtp, char *sdp, u32 sdp_len, GF_RTPInStream *stream);

/*RTSP signaling is handled by stacking commands and processing them
in the main session thread. Each RTSP command has an associated private stack as follows*/

/*describe stack for single channel (not for session)*/
typedef struct
{
	u32 ES_ID;
	GF_FilterPid *opid;
	char *esd_url;
} RTPIn_StreamDescribe;

typedef struct
{
	GF_RTPInStream *stream;
	GF_FilterEvent evt;
} RTPIn_StreamControl;

/*RTSP signaling */
Bool rtpin_rtsp_describe_preprocess(GF_RTPInRTSP *sess, GF_RTSPCommand *com);
GF_Err rtpin_rtsp_describe_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e);
void rtpin_rtsp_setup_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e);
void rtpin_rtsp_teardown_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e);
Bool rtpin_rtsp_usercom_preprocess(GF_RTPInRTSP *sess, GF_RTSPCommand *com);
void rtpin_rtsp_usercom_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e);

/*send describe - if esd_url is given, this is a describe on es*/
void rtpin_rtsp_describe_send(GF_RTPInRTSP *sess, char *esd_url, GF_FilterPid *opid);
/*send setup for stream*/
void rtpin_rtsp_setup_send(GF_RTPInStream *stream);
/*filter setup if no session (rtp only), otherwise setup channel - ch_desc may be NULL
if channel association is already done*/
GF_Err rtpin_stream_setup(GF_RTPInStream *stream, RTPIn_StreamDescribe *ch_desc);
/*send command for stream - handles aggregation*/
void rtpin_rtsp_usercom_send(GF_RTPInRTSP *sess, GF_RTPInStream *stream, const GF_FilterEvent *fevt);
/*disconnect the session - if @ch, only the channel is teardown*/
void rtpin_rtsp_teardown(GF_RTPInRTSP *sess, GF_RTPInStream *stream);


void rtpin_stream_on_rtp_pck(GF_RTPInStream *stream, char *pck, u32 size);

#endif /*GPAC_DISABLE_STREAMING*/

#endif //_IN_RTP_H_
