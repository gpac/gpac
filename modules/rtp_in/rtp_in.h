/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP input module
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

#ifndef RTP_IN_H
#define RTP_IN_H

#include <gpac/thread.h>
#include <gpac/constants.h>
#include <gpac/base_coding.h>
/*module interface*/
#include <gpac/modules/service.h>
/*IETF lib*/
#include <gpac/ietf.h>


#define RTP_BUFFER_SIZE			0x100000ul
#define RTSP_BUFFER_SIZE		5000
#define RTSP_TCP_BUFFER_SIZE    0x100000ul
#define RTSP_CLIENTNAME		"GPAC " GPAC_VERSION " RTSP Client"
#define RTSP_LANGUAGE		"English"


/*the rtsp/rtp client*/
typedef struct
{
	/*the service we're responsible for*/
	GF_ClientService *service;

	/*the one and only IOD*/
	GF_Descriptor *session_desc;

	/*RTSP sessions*/
	GF_List *sessions;
	/*RTP/RTCP media channels*/
	GF_List *channels;

	/*sdp downloader*/
	GF_DownloadSession * dnload;
	/*initial sdp download if any (temp storage)*/
	struct _sdp_fetch *sdp_temp;

	/*RTSP communication/deinterleaver thread*/
	GF_Mutex *mx;
	GF_Thread *th;
	u32 th_state;

	/*RTSP config*/
	/*transport mode. 0 is udp, 1 is tcp, 3 is tcp if unreliable media */
	u32 transport_mode;
	/*default RTSP port*/
	u16 default_port;
	/*signaling timeout in msec*/
	u32 time_out;
	/*udp timeout in msec*/
	u32 udp_time_out;

	/*packet drop emulation*/
	u32 first_packet_drop;
	u32 frequency_drop;

	/*for single-object control*/
	u32 media_type;

	/*if set ANNOUNCE (sent by server) will be handled*/
//	Bool handle_announce;
} RTPClient;

enum
{
	RTSP_AGG_CONTROL = 1,
	RTSP_TCP_FLUSH = 1<<1,
	RTSP_FORCE_INTER = 1<<2,
	RTSP_WAIT_REPLY = 1<<3
};

/*rtsp session*/
typedef struct _rtp_session
{
	u32 flags;

	/*owner*/
	RTPClient *owner;

	/*RTSP session object*/
	GF_RTSPSession *session;
	
	/*session control string*/
	char *control;

	/*response object*/
	GF_RTSPResponse *rtsp_rsp;

	Double last_range;
	u32 command_time;
	GF_List *rtsp_commands;
} RTSPSession;

/*creates new RTSP session handler*/
RTSPSession *RP_NewSession(RTPClient *rtp, char *session_control);
/*disconnects and destroy RTSP session handler - if immediate_shutdown do not wait for response*/
void RP_RemoveSession(RTSPSession *sess, Bool immediate_shutdown);
/*check session by control string*/
RTSPSession *RP_CheckSession(RTPClient *rtp, char *control);

void RP_SetupObjects(RTPClient *rtp);

void RP_ProcessCommands(RTSPSession *sess);

/*RTP channel state*/
enum
{
	/*channel is setup and waits for connection request*/
	RTP_Setup = 0,
	/*waiting for server reply*/
	RTP_WaitingForAck,
	/*connection OK*/
	RTP_Connected,
	/*data exchange on this service/channel*/
	RTP_Running,
	/*deconnection OK - a download channel can automatically disconnect when download is done*/
	RTP_Disconnected,
	/*service/channel is not (no longer) available/found and should be removed*/
	RTP_Unavailable
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
	
	/*RTP payload flags*/

	/*AWFULL hack at rtp level to cope with ffmpeg h264 crashes when jumping in stream without IDR*/
	RTP_AVC_WAIT_RAP = (1<<10),
	/*AMR config*/
	RTP_AMR_ALIGN = (1<<11),
	/*for RFC3016, signals bitstream inspection for RAP discovery*/
	RTP_M4V_CHECK_RAP = (1<<12),
	/*ISMACryp stuff*/
	RTP_HAS_ISMACRYP = (1<<13),
	RTP_ISMA_SEL_ENC = (1<<14),
	RTP_ISMA_HAS_KEY_IDX = (1<<15),
	

	/*RTP channel runtime flags*/

	/*set if next command (PLAY/PAUSE) is to be skipped (aggregation control)*/
	RTP_SKIP_NEXT_COM = (1<<20),
	/*AU end was detected (eg next packet is AU start)*/
	RTP_NEW_AU = (1<<21),
	/*indicates whether channel creation has been acknowledged or not
	this is needed to filter real channel_connect calls from RTSP re-setup (after STOP) ones*/
	RTP_CONNECTED = (1<<22),
	/*EOS signaled (RTCP or range-based)*/
	RTP_EOS = (1<<23),
};

/*rtp channel*/
typedef struct
{
	/*module*/
	RTPClient *owner;
	/*payloat type*/
	u32 rtptype;
	/*channel flags*/
	u32 flags;

	/*control session (may be null)*/
	RTSPSession *rtsp;

	/*logical app channel*/
	LPNETCHANNEL channel;
	u32 status;

	/*RTP channel*/
	GF_RTPChannel *rtp_ch;
	
	u32 ES_ID;
	char *control;

	/*depacketizer config*/
	GP_RTPSLMap sl_map;
	GF_SLHeader sl_hdr;

	/*rtp recieve buffer*/
	char buffer[RTP_BUFFER_SIZE];
	/*set at seek stages to resync app NPT to RTP time*/
	u32 check_rtp_time;

	/*can we control the stream ?*/
	Double range_start, range_end;

	/*RTSP control aggregation state*/

	/*current start time in npt (for pause/resume)*/
	Double current_start;
	u32 clock_rate;
	/*needed for DTS/CTS compute with some payloads type*/
	u32 unit_duration;

	/*UDP detection*/
	u32 last_udp_time;

	/*stats*/
	u32 rtp_bytes, rtcp_bytes, stat_start_time, stat_stop_time;

	/*inter-packet reconstruction bitstream (for 3GP text and H264)*/
	GF_BitStream *inter_bs;

	/*H264/AVC config*/
	u32 packetization_mode;
	
	/*3GP text reassembler state*/
	u8 nb_txt_frag, cur_txt_frag, sidx, txt_len, nb_mod_frag;
	u32 au_dur;
	
	/*ISMACryp*/
	u32 isma_scheme;
	char *key;
} RTPStream;

/*creates new RTP stream from SDP info*/
RTPStream *RP_NewStream(RTPClient *rtp, GF_SDPMedia *media, GF_SDPInfo *sdp, RTPStream *input_stream);
/*destroys RTP stream */
void RP_DeleteStream(RTPStream *ch);
/*resets stream state and inits RTP sockets if ResetOnly is false*/
GF_Err RP_InitStream(RTPStream *ch, Bool ResetOnly);
/*disconnect stream but keeps its config alive*/
void RP_DisconnectStream(RTPStream *ch);

/*RTSP -> RTP de-interleaving callback*/
GF_Err RP_DataOnTCP(GF_RTSPSession *sess, void *cbck, char *buffer, u32 bufferSize, Bool IsRTCP);
/*send confirmation of connection - if no error, also setup SL based on payload*/
void RP_ConfirmChannelConnect(RTPStream *ch, GF_Err e);

/*fetch sdp file - stream is the RTP channel this sdp describes, or NULL if session sdp*/
void RP_FetchSDP(GF_InputService *plug, char *url, RTPStream *stream);

/*locate RTP stream by channel or ES_ID or control*/
RTPStream *RP_FindChannel(RTPClient *rtp, LPNETCHANNEL ch, u32 ES_ID, char *es_control, Bool remove_stream);
/*adds channel to session identified by session_control. If no session exists, the session is created if needed*/
GF_Err RP_AddStream(RTPClient *rtp, RTPStream *stream, char *session_control);
/*removes stream from session*/
void RP_RemoveStream(RTPClient *rtp, RTPStream *ch);
/*reads input socket and process*/
void RP_ReadStream(RTPStream *ch);

/*parse RTP payload for MPEG4*/
void RP_ParsePayloadMPEG4(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);
/*parse RTP payload for MPEG12*/
void RP_ParsePayloadMPEG12(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);
/*parse RTP payload for AMR*/
void RP_ParsePayloadAMR(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);
/*parse RTP payload for H263+*/
void RP_ParsePayloadH263(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);
/*parse RTP payload for 3GPP Text*/
void RP_ParsePayloadText(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);
/*parse RTP payload for H264/AVC*/
void RP_ParsePayloadH264(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);
/*parse RTP payload for LATM audio*/
void RP_ParsePayloadLATM(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size);

/*load SDP and setup described media in SDP. If stream is null this is the root
SDP and IOD will be extracted, otherwise this a channel SDP*/
void RP_LoadSDP(RTPClient *rtp, char *sdp, u32 sdp_len, RTPStream *stream);

/*returns 1 if payload type is supported*/
u32 payt_get_type(RTPClient *rtp, GF_RTPMap *map, GF_SDPMedia *media);
/*setup payload type, returns 1 if success, 0 otherwise (in which case the stream will be deleted)*/
Bool payt_setup(RTPStream *st, GF_RTPMap *map, GF_SDPMedia *media);


/*RTSP signaling is handled by stacking commands and processing them
in the main session thread. Each RTSP command has an associated private stack as follows*/

/*describe stack for single channel (not for session)*/
typedef struct
{
	u32 ES_ID;
	LPNETCHANNEL channel;
	char *esd_url;
} ChannelDescribe;

typedef struct
{
	RTPStream *ch;
	GF_NetworkCommand com;
} ChannelControl;

/*RTSP signaling */
Bool RP_PreprocessDescribe(RTSPSession *sess, GF_RTSPCommand *com);
Bool RP_ProcessDescribe(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e);
void RP_ProcessSetup(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e);
void RP_ProcessTeardown(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e);
Bool RP_PreprocessUserCom(RTSPSession *sess, GF_RTSPCommand *com);
void RP_ProcessUserCommand(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e);

/*send describe - if esd_url is given, this is a describe on es*/
void RP_Describe(RTSPSession *sess, char *esd_url, LPNETCHANNEL channel);
/*send setup for stream*/
void RP_Setup(RTPStream *ch);
/*filter setup if no session (rtp only), otherwise setup channel - ch_desc may be NULL
if channel association is already done*/
GF_Err RP_SetupChannel(RTPStream *ch, ChannelDescribe *ch_desc);
/*send command for stream - handles aggregation*/
void RP_UserCommand(RTSPSession *sess, RTPStream *ch, GF_NetworkCommand *command);
/*disconnect the session - if @ch, only the channel is teardown*/
void RP_Teardown(RTSPSession *sess, RTPStream *ch);

/*emulate IOD*/
GF_Descriptor *RP_EmulateIOD(RTPClient *rtp, const char *sub_url);


/*sdp file downloader*/
typedef struct _sdp_fetch
{
	RTPClient *client;
	/*when loading a channel from SDP*/
	RTPStream *chan;

	char *remote_url;
} SDPFetch;

#endif


