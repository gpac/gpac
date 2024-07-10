/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / IETF RTP/RTSP/SDP sub-project
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

#ifndef	_GF_IETF_DEV_H_
#define _GF_IETF_DEV_H_

#include <gpac/ietf.h>

#ifndef GPAC_DISABLE_STREAMING

/*
			RTP intern
*/

typedef struct
{
	/*version of the packet. Must be 2*/
	u8 Version;
	/*padding bits at the end of the payload*/
	u8 Padding;
	/*number of reports*/
	u8 Count;
	/*payload type of RTCP pck*/
	u8 PayloadType;
	/*The length of this RTCP packet in 32-bit words minus one including the header and any padding*/
	u16 Length;
	/*sync source identifier*/
	u32 SSRC;
} GF_RTCPHeader;


typedef struct __PRO_item
{
	struct __PRO_item *next;
	u32 pck_seq_num;
	void *pck;
	u32 size;
} GF_POItem;

typedef struct __PO
{
	struct __PRO_item *in;
	u32 head_seqnum;
	u32 Count;
	u32 MaxCount;
	u32 IsInit;
	u32 MaxDelay, LastTime;
	u32 TimeScale;
	struct __PRO_item *disc;
} GF_RTPReorder;

/* creates new RTP reorderer
	@MaxCount: forces automatic packet flush. 0 means no flush
	@MaxDelay: is the max time in ms the queue will wait for a missing packet
*/
GF_RTPReorder *gf_rtp_reorderer_new(u32 MaxCount, u32 MaxDelay, u32 rtp_timescale);
void gf_rtp_reorderer_del(GF_RTPReorder *po);
/*reset the Queue*/
void gf_rtp_reorderer_reset(GF_RTPReorder *po);

/*Adds a packet to the queue. Packet Data is memcopied*/
GF_Err gf_rtp_reorderer_add(GF_RTPReorder *po, const void * pck, u32 pck_size, u32 pck_seqnum);
/*gets the output of the queue. Packet Data IS YOURS to delete*/
void *gf_rtp_reorderer_get(GF_RTPReorder *po, u32 *pck_size, Bool force_flush, Bool *is_disc);

#define MAX_RTCP_RR	4

typedef struct
{
	u32 ssrc;
	u32 frac_lost;
	u32 total_loss;
	u32 last_rtp_sn;
	u64 jitter;
	u32 last_sr, delay_last_sr;
} GF_RTCP_Report;

/*the RTP channel with both RTP and RTCP sockets and buffers
each channel is identified by a control string given in RTSP Describe
this control string is used with Darwin
*/
struct __tag_rtp_channel
{
	/*global transport info for the session*/
	GF_RTSPTransport net_info;

	/*RTP CHANNEL*/
	GF_Socket *rtp;
	/*RTCP CHANNEL*/
	GF_Socket *rtcp;

	/*RTP Packet reordering. Turned on/off during initialization. The library forces a 200 ms
	max latency at the reordering queue*/
	GF_RTPReorder *po;

	/*RTCP report times*/
	u32 last_report_time;
	u32 next_report_time;

	/*NAT keep-alive*/
	u32 last_nat_keepalive_time, nat_keepalive_time_period;


	/*the seq number of the first packet as signaled by the server if any, or first
	RTP SN received (RTP multicast)*/
	u32 rtp_first_SN;
	/*the TS of the associated first packet as signaled by the server if any, or first
	RTP TS received (RTP multicast)*/
	u32 rtp_time;
	/*NPT from the rtp_time*/
	u32 CurrentTime;
	/*num loops of pck sn*/
	u32 num_sn_loops;
	/*some mapping info - we should support # payloads*/
	u8 PayloadType;
	u32 TimeScale;

	/*static buffer for RTP sending*/
	u8 *send_buffer;
	u32 send_buffer_size;
	u32 pck_sent_since_last_sr;
	u32 last_pck_ts;
	u32 last_pck_ntp_sec, last_pck_ntp_frac;
	u32 num_pck_sent, num_payload_bytes;
	u32 forced_ntp_sec, forced_ntp_frac;

	u32 force_loss_rate;
	Bool no_auto_rtcp;
	/*RTCP info*/
	char *s_name, *s_email, *s_location, *s_phone, *s_tool, *s_note, *s_priv;
//	s8 first_rtp_pck;
	s8 first_SR;
	u32 SSRC;
	u32 SenderSSRC;

	u32 last_pck_sn;
	/*indicates if a packet loss is detected between current and previous packet*/
	Bool packet_loss;

	char *CName;

	u32 rtcp_bytes_sent;
	/*total pck rcv*/
	u32 tot_num_pck_rcv, tot_num_pck_expected;
	/*stats since last SR*/
	u32 last_num_pck_rcv, last_num_pck_expected, last_num_pck_loss;
	/*jitter compute*/
	u32 Jitter, ntp_init;
	s32 last_deviance;
	/*NTP of last SR*/
	u32 last_SR_NTP_sec, last_SR_NTP_frac;
	/*RTP time at last SR as indicated in SR*/
	u32 last_SR_rtp_time;
	/*payload info*/
	u32 total_pck, total_bytes;

	GF_BitStream *bs_r, *bs_w;
	Bool no_select;

	gf_rtp_tcp_callback send_interleave;
	void *interleave_cbk1, *interleave_cbk2;

	GF_RTCP_Report rtcp_rr[MAX_RTCP_RR];
	u32 nb_rctp_rr;

	const char *netcap_id;
	const char **ssm, **ssmx;
	u32 nb_ssm, nb_ssmx;
	u8 disc_state;
};

/*gets UTC in the channel RTP timescale*/
u32 gf_rtp_channel_time(GF_RTPChannel *ch);
/*gets time in 1/65536 seconds (for reports)*/
u32 gf_rtp_get_report_time();
/*updates the time for the next report (SR, RR)*/
void gf_rtp_get_next_report_time(GF_RTPChannel *ch);

Bool gf_rtp_is_disc(GF_RTPChannel *ch);


/*
			RTSP intern
*/

#define GF_RTSP_DEFAULT_BUFFER		2048
#define GF_RTSP_VERSION		"RTSP/1.0"

/*macros for RTSP command and response formmating*/
#define RTSP_WRITE_STEPALLOC	250

#define RTSP_WRITE_ALLOC_STR_WITHOUT_CHECK(buf, buf_size, pos, str)		\
	if (strlen((const char *) str)+pos >= buf_size) {	\
		buf_size += (u32) strlen((const char *) str);	\
		buf = (char *) gf_realloc(buf, buf_size);		\
	}	\
	strcpy(buf+pos, (const char *) str);		\
	pos += (u32) strlen((const char *) str); \
 
#define RTSP_WRITE_ALLOC_STR(buf, buf_size, pos, str)		\
	if (str){	\
		RTSP_WRITE_ALLOC_STR_WITHOUT_CHECK(buf, buf_size, pos, str);	\
	}	\
	
#define RTSP_WRITE_HEADER(buf, buf_size, pos, type, str)		\
	if( str ) {	\
	RTSP_WRITE_ALLOC_STR(buf, buf_size, pos, type);		\
	RTSP_WRITE_ALLOC_STR(buf, buf_size, pos, ": ");		\
	RTSP_WRITE_ALLOC_STR(buf, buf_size, pos, str);		\
	RTSP_WRITE_ALLOC_STR(buf, buf_size, pos, "\r\n");	\
	}	\
 
#define RTSP_WRITE_INT(buf, buf_size, pos, d, sig)	{	\
	char temp[50]; \
	if (sig < 0) { \
		sprintf(temp, "%d", d);		\
	} else { \
		sprintf(temp, "%u", d);		\
	}	\
	RTSP_WRITE_ALLOC_STR_WITHOUT_CHECK(buf, buf_size, pos, temp); \
	}

#define RTSP_WRITE_HEX(buf, buf_size, pos, d, sig)	{	\
	char temp[50]; \
	sprintf(temp, "%X", d);		\
	RTSP_WRITE_ALLOC_STR_WITHOUT_CHECK(buf, buf_size, pos, temp);\
	}

#define RTSP_WRITE_FLOAT_WITHOUT_CHECK(buf, buf_size, pos, d) {		\
	char temp[50]; \
	sprintf(temp, "%.4f", d);		\
	RTSP_WRITE_ALLOC_STR_WITHOUT_CHECK(buf, buf_size, pos, temp);\
	}

#define RTSP_WRITE_FLOAT(buf, buf_size, pos, d)	{	\
	char temp[50]; \
	sprintf(temp, "%.4f", d);		\
	RTSP_WRITE_ALLOC_STR(buf, buf_size, pos, temp); \
	}


typedef struct
{
	u8 rtpID;
	u8 rtcpID;
	void *ch_ptr;
} GF_TCPChan;

typedef enum
{
	RTSP_HTTP_NONE = 0,
	RTSP_HTTP_CLIENT,
	RTSP_HTTP_SERVER,
	RTSP_HTTP_DISABLE
} RTSP_HTTP_Tunnel;

#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#endif

/**************************************
		RTSP Session
***************************************/
struct _tag_rtsp_session
{
	/*service name (extracted from URL) ex: news/latenight.mp4, vod.mp4 ...*/
	char *Service;
	/*server name (extracted from URL)*/
	char *Server;
	/*server port (extracted from URL)*/
	u16 Port;

	char *User, *Pass;

	/*if RTSP is on UDP*/
	u8 ConnectionType;
	/*TCP interleaving ID*/
	u8 InterID;
	/*http tunnel*/
	RTSP_HTTP_Tunnel tunnel_mode;
	GF_Socket *http;
	char *HTTP_Cookie;
	u32 tunnel_state;

	const char *netcap_id;

	/*RTSP CHANNEL*/
	GF_Socket *connection;
	u32 SockBufferSize;
	/*needs connection*/
	u32 NeedConnection;
	u32 timeout_in;
	/*the RTSP sequence number*/
	u32 CSeq;
	/*this is for aggregated request in order to check SeqNum*/
	u32 NbPending;
	u32 nb_retry;
	/*RTSP sessionID, arbitrary length, alpha-numeric*/
	const char *last_session_id;

	/*RTSP STATE machine*/
	u32 RTSP_State;
	char RTSPLastRequest[40];

	/*current buffer from TCP if any*/
	u8 *tcp_buffer;
	u32 CurrentSize, CurrentPos;

	/*RTSP interleaving*/
	GF_Err (*RTSP_SignalData)(GF_RTSPSession *sess, void *chan, u8 *buffer, u32 bufferSize, Bool IsRTCP);

	/*buffer for pck reconstruction*/
	u8 *rtsp_pck_buf;
	u32 rtsp_pck_size;
	u32 pck_start, payloadSize;

	/*all RTP channels in an interleaved RTP on RTSP session*/
	GF_List *TCPChannels;
	Bool interleaved;

	u8 *async_buf;
	u32 async_buf_size, async_buf_alloc;

#ifdef GPAC_HAS_SSL
	Bool use_ssl, ssl_connect_pending;
	SSL_CTX *ssl_ctx;
	SSL *ssl, *ssl_http;
#endif

};

GF_RTSPSession *gf_rtsp_session_new(char *sURL, u16 DefaultPort);

/*send data on RTSP*/
GF_Err gf_rtsp_send_data(GF_RTSPSession *sess, u8 *buffer, u32 Size);

GF_Err gf_rstp_do_read_sock(GF_RTSPSession *sess, GF_Socket *sock, u8 *data, u32 data_size, u32 *out_read);

/*
			Common RTSP tools
*/

/*locate body-start and body size in response/commands*/
void gf_rtsp_get_body_info(GF_RTSPSession *sess, u32 *body_start, u32 *body_size, Bool skip_tunnel);
/*read TCP until a full command/response is received*/
GF_Err gf_rtsp_read_reply(GF_RTSPSession *sess);
/*fill the TCP buffer*/
GF_Err gf_rtsp_fill_buffer(GF_RTSPSession *sess);
/*force a fill on TCP buffer - used for de-interleaving and TCP-fragmented RTSP messages*/
GF_Err gf_rtsp_refill_buffer(GF_RTSPSession *sess);
/*parses a transport string and returns a transport structure*/
GF_RTSPTransport *gf_rtsp_transport_parse(u8 *buffer);
/*parsing of header for com and rsp*/
GF_Err gf_rtsp_parse_header(u8 *buffer, u32 BufferSize, u32 BodyStart, GF_RTSPCommand *com, GF_RTSPResponse *rsp);
void gf_rtsp_set_command_value(GF_RTSPCommand *com, char *Header, char *Value);
void gf_rtsp_set_response_value(GF_RTSPResponse *rsp, char *Header, char *Value);





/*
		RTP -> SL packetization tool
	You should ONLY modify the GF_SLHeader while packetizing, all the rest is private
	to the tool.
	Also note that AU start/end is automatically updated, therefore you should only
	set CTS-DTS-OCR-sequenceNumber (which is automatically incremented when splitting a payload)
	-padding-idle infos
	SL flags are computed on the fly, but you may wish to modify them in case of
	packet drop/... at the encoder side

*/
struct __tag_rtp_packetizer
{
	/*input packet sl header cfg. modify only if needed*/
	GF_SLHeader sl_header;
	u32 nb_aus;
	/*

		PRIVATE _ DO NOT TOUCH
	*/

//! @cond Doxygen_Suppress

	/*RTP payload type (RFC type, NOT the RTP hdr payT)*/
	u32 rtp_payt;
	/*packetization flags*/
	u32 flags;
	/*Path MTU size without 12-bytes RTP header*/
	u32 Path_MTU;
	/*max packet duration in RTP TS*/
	u32 max_ptime;

	/*payload type of RTP packets - only one payload type can be used in GPAC*/
	u8 PayloadType;

	/*RTP header of current packet*/
	GF_RTPHeader rtp_header;

	/*RTP packet handling callbacks*/
	void (*OnNewPacket)(void *cbk_obj, GF_RTPHeader *header);
	void (*OnPacketDone)(void *cbk_obj, GF_RTPHeader *header);
	void (*OnDataReference)(void *cbk_obj, u32 payload_size, u32 offset_from_orig);
	void (*OnData)(void *cbk_obj, u8 *data, u32 data_size, Bool is_header);
	void *cbk_obj;

	/*********************************
		MPEG-4 Generic hinting
	*********************************/

	/*SL to RTP map*/
	GP_RTPSLMap slMap;
	/*SL conf and state*/
	GF_SLConfig sl_config;

	/*set to 1 if firstSL in RTP packet*/
	Bool first_sl_in_rtp;
	Bool has_AU_header;
	/*current info writers*/
	GF_BitStream *pck_hdr, *payload;

	/*AU SN of last au*/
	u32 last_au_sn;

	/*info for the current packet*/
	u32 auh_size, bytesInPacket;

	/*********************************
			ISMACryp info
	*********************************/
	Bool force_flush, is_encrypted;
	u64 IV, first_AU_IV;
	char *key_indicator;

	/*********************************
			AVC-H264 info
	*********************************/
	/*AVC non-IDR flag: set if all NAL in current packet are non-IDR (disposable)*/
	Bool avc_non_idr;

	/*********************************
			AC3 info
	*********************************/
	/*ac3 ft flags*/
	u8 ac3_ft;

	/*********************************
			HEVC-H265 info
	*********************************/
	/*HEVC Payload Header. It will be use in case of Aggreation Packet where we must add payload header for packet after having added of NALU to AP*/
	char hevc_payload_hdr[2];

//! @endcond

};

/*packetization routines*/
GF_Err gp_rtp_builder_do_mpeg4(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_h263(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_amr(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
#ifndef GPAC_DISABLE_AV_PARSERS
GF_Err gp_rtp_builder_do_mpeg12_video(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
#endif
GF_Err gp_rtp_builder_do_mpeg12_audio(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_tx3g(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration, u8 descIndex);
GF_Err gp_rtp_builder_do_avc(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_qcelp(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_smv(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_latm(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration);
GF_Err gp_rtp_builder_do_ac3(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_hevc(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_mp2t(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_vvc(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);
GF_Err gp_rtp_builder_do_opus(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize);

#define RTP_VVC_AGG_NAL		0x1C //28
#define RTP_VVC_FRAG_NAL	0x1D //29

/*! RTP depacketization tool*/
struct __tag_rtp_depacketizer
{
	/*! depacketize routine*/
	void (*depacketize)(struct __tag_rtp_depacketizer *rtp, GF_RTPHeader *hdr, u8 *payload, u32 size);

	/*! output packet sl header cfg*/
	GF_SLHeader sl_hdr;

	/*! RTP payload type (RFC type, NOT the RTP hdr payT)*/
	u32 payt;
	/*! depacketization flags*/
	u32 flags;

	//! RTP static map may be NULL
	const GF_RTPStaticMap *static_map;

	/*! callback routine*/
	gf_rtp_packet_cbk on_sl_packet;
	/*! callback udta*/
	void *udta;

	/*! SL <-> RTP map*/
	GP_RTPSLMap sl_map;
	/*! RTP clock rate*/
	u32 clock_rate;
	/*! audio channels from RTP map*/
	u32 audio_channels;

	//! clip rect X
	u32 x;
	//! clip rect Y
	u32 y;
	//! clip rect or full size width
	u32 w;
	//! clip rect or full size height
	u32 h;

	/*! inter-packet reconstruction bitstream (for 3GP text and H264)*/
	GF_BitStream *inter_bs;

	/*! H264/AVC config*/
	u32 h264_pck_mode;

	/*3GP text reassembler state*/
	/*! number of 3GPP text fragments*/
	u8 nb_txt_frag;
	/*! current 3GPP text fragments*/
	u8 cur_txt_frag;
	/*! current 3GPP text sample desc index*/
	u8 sidx;
	/*! 3GPP text total sample text len*/
	u8 txt_len;
	/*! number of 3GPP text modifiers*/
	u8 nb_mod_frag;

	/*! ISMACryp scheme*/
	u32 isma_scheme;
	/*! ISMACryp key*/
	char *key;
};

#endif /*GPAC_DISABLE_STREAMING*/

#endif	/*_GF_IETF_DEV_H_*/

