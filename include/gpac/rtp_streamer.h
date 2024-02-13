/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / media tools sub-project
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

#ifndef _GF_RTPSTREAMER_H_
#define _GF_RTPSTREAMER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/rtp_streamer.h>
\brief RTP streaming (packetizer and RTP socket).
*/

/*!
\addtogroup rtp_grp RTP Streamer
\ingroup media_grp
\brief RTPStreamer object

This section documents the RTP streamer object of the GPAC framework.

@{
 */



#include <gpac/ietf.h>

#if !defined(GPAC_DISABLE_STREAMING)

/*! RTP streamer object*/
typedef struct __rtp_streamer GF_RTPStreamer;


/*!
\brief RTP Streamer constructor with extended parameters

Constructs a new RTP file streamer
\param streamType type of the stream (GF_STREAM_* as defined in <gpac/constants.h>)
\param codecid codec ID for the stream (GF_CODEC_* as defined in <gpac/constants.h>)
\param timeScale unit to express timestamps of access units
\param ip_dest IP address of the destination
\param port port number of the destination
\param MTU Maximum Transmission Unit size to use
\param TTL Time To Leave
\param ifce_addr IP of the local interface to use (may be NULL)
\param flags set of RTP flags passed to the streamer
\param dsi MPEG-4 Decoder Specific Info for the stream
\param dsi_len length of the dsi parameter
\param PayloadType RTP payload type
\param sample_rate audio sample rate
\param nb_ch number of channels in audio streams
\param is_crypted Boolean indicating if the stream is crypted
\param IV_length lenght of the Initialisation Vector used for encryption
\param KI_length length of the key index
\param MinSize minimum AU size, 0 if unknown
\param MaxSize maximum AU size, 0 if unknown
\param avgTS average TS delta in timeScale, 0 if unknown
\param maxDTSDelta maximum DTS delta in timeScale, 0 if unknown
\param const_dur constant duration in timeScale, 0 if unknown
\param bandwidth bandwidth, 0 if unknown
\param max_ptime maximum packet duration in timeScale, 0 if unknown
\param au_sn_len length of the MPEG-4 SL descriptor AU sequence number field, 0 if unknown
\param for_rtsp indicates this is an RTP channel in an RTSP session, RTP channel will not be created, use \ref gf_rtp_streamer_init_rtsp
\return a new RTP streamer, or NULL of error or not supported
 */
GF_RTPStreamer *gf_rtp_streamer_new(u32 streamType, u32 codecid, u32 timeScale,
        const char *ip_dest, u16 port, u32 MTU, u8 TTL, const char *ifce_addr,
        u32 flags, const u8 *dsi, u32 dsi_len,
        u32 PayloadType, u32 sample_rate, u32 nb_ch,
        Bool is_crypted, u32 IV_length, u32 KI_length,
        u32 MinSize, u32 MaxSize, u32 avgTS, u32 maxDTSDelta, u32 const_dur, u32 bandwidth, u32 max_ptime, u32 au_sn_len, Bool for_rtsp);

/*! RTP streamer configuration*/
typedef struct
{
	//! type of the stream (GF_STREAM_* as defined in <gpac/constants.h>
	u32 streamType;
	//! codec ID for the stream (GF_CODEC_* as defined in <gpac/constants.h>)
	u32 codecid;
	//! unit to express timestamps of access units
	u32 timeScale;
	//! IP address of the destination
	const char *ip_dest;
	//! port number of the destination
	u16 port;
	//! Maximum Transmission Unit size to use
	u32 MTU;
	//! Time To Leave
	u8 TTL;
	//! IP of the local interface to use (may be NULL)
	const char *ifce_addr;
	//! set of RTP flags passed to the streamer
	u32 flags;
	//! MPEG-4 Decoder Specific Info for the stream
	const u8 *dsi;
	//! length of the dsi parameter
	u32 dsi_len;
	//! RTP payload type
	u32 PayloadType;
	//!  audio sample rate
	u32 sample_rate;
	//! number of channels in audio streams
	u32 nb_ch;
	//! indicating if the stream is crypted
	Bool is_crypted;
	//! length of the Initialisation Vector used for ISMA encryption
	u32 IV_length;
	//! length of the key index
	u32 KI_length;
	//! minimum AU size, 0 if unknown
	u32 MinSize;
	//maximum AU size, 0 if unknown
	u32 MaxSize;
	//! average TS delta in timeScale, 0 if unknown
	u32 avgTS;
	//! maximum DTS delta in timeScale, 0 if unknown
	u32 maxDTSDelta;
	//! constant duration in timeScale, 0 if unknown
	u32 const_dur;
	//! bandwidth, 0 if unknown
	u32 bandwidth;
	//! maximum packet duration in timeScale, 0 if unknown
	u32 max_ptime;
	//! length of the MPEG-4 SL descriptor AU sequence number field, 0 if unknown
	u32 au_sn_len;
	//! ID of netcap configuration to use, may be null (see gpac -h netcap)
	const char *netcap_id;
} GF_RTPStreamerConfig;

/*!
\brief RTP Streamer constructor with extended parameters

Constructs a new RTP file streamer
\param cfg configuration of the streamer
\param for_rtsp indicates this is an RTP channel in an RTSP session, RTP channel will not be created, use \ref gf_rtp_streamer_init_rtsp
\return a new RTP streamer, or NULL of error or not supported
 */GF_RTPStreamer *gf_rtp_streamer_new_ex(const GF_RTPStreamerConfig *cfg, Bool for_rtsp);

/*!
\brief RTP file streamer destructor

Destructs an RTP file streamer
\param streamer the target RTP streamer
 */
void gf_rtp_streamer_del(GF_RTPStreamer *streamer);

/*!
\brief gets the SDP file

Gets the SDP asscoiated with all media in the streaming session (only media parts are returned)
\param rtp the target RTP streamer
\param ESID The MPEG-4 elementary stream id of the stream to process
\param dsi The decoder specific info data
\param dsi_len length of the decoder specific info data
\param KMS_URI URI of the Key Management System
\param out_sdp_buffer location to the SDP buffer to allocate and fill
\return error if any
 */
GF_Err gf_rtp_streamer_append_sdp(GF_RTPStreamer *rtp, u16 ESID, const u8 *dsi, u32 dsi_len, char *KMS_URI, char **out_sdp_buffer);

/*!
\brief gets the SDP file

Gets the SDP asscoiated with all media in the streaming session (only media parts are returned)
\param rtp the target RTP streamer
\param ESID The MPEG-4 elementary stream id of the stream to process
\param dsi decoder specific info data
\param dsi_len length of the decoder specific info data
\param dsi_enh enhancement layer decoder specific info data
\param dsi_enh_len length of enhancement layer decoder specific info data
\param KMS_URI URI of the Key Management System
\param width video width
\param height video height
\param tw text window width
\param th text window height
\param tx text window horizontal offset
\param ty text window vertical offset
\param tl text window z-index
\param nb_chan number of audio channels, 0 if unknown
\param for_rtsp if GF_TRUE, produces the SDP for an RTSP describe (no port info)
\param out_sdp_buffer location to the SDP buffer to allocate and fill
\return error if any
 */
GF_Err gf_rtp_streamer_append_sdp_extended(GF_RTPStreamer *rtp, u16 ESID, const u8 *dsi, u32 dsi_len, const u8 *dsi_enh, u32 dsi_enh_len, char *KMS_URI, u32 width, u32 height, u32 tw, u32 th, s32 tx, s32 ty, s16 tl, u32 nb_chan, Bool for_rtsp, char **out_sdp_buffer);

/*! sends a full Access Unit over RTP
\param rtp the target RTP streamer
\param data AU payload
\param size AU payload size
\param cts composition timestamp in timeScale unit
\param dts decoding timestamp in timeScale unit
\param is_rap indicates if the AU is a random access
\return error if any
*/
GF_Err gf_rtp_streamer_send_au(GF_RTPStreamer *rtp, u8 *data, u32 size, u64 cts, u64 dts, Bool is_rap);

/*! sends a full Access Unit over RTP
\param rtp the target RTP streamer
\param data AU payload
\param size AU payload size
\param cts composition timestamp in timeScale unit
\param dts decoding timestamp in timeScale unit
\param is_rap indicates if the AU is a random access
\param inc_au_sn increments the AU sequence number by the given value before packetizing (used for MPEG-4 systems carousel)
\return error if any
*/
GF_Err gf_rtp_streamer_send_au_with_sn(GF_RTPStreamer *rtp, u8 *data, u32 size, u64 cts, u64 dts, Bool is_rap, u32 inc_au_sn);

/*! sends a full or partial Access Unit over RTP
\param streamer the target RTP streamer
\param data AU payload
\param size AU payload size
\param fullsize the size of the complete AU
\param cts composition timestamp in timeScale unit
\param dts decoding timestamp in timeScale unit
\param is_rap indicates if the AU is a random access
\param au_start indicates if this chunk is the start of the AU
\param au_end indicates if this chunk is the end of the AU
\param au_sn indicates the AU sequence number
\param duration indicates the AU duration
\param sampleDescriptionIndex indicates the ISOBMFF sampleDescriptionIndex for this AU (needed for 3GPP timed text)
\return error if any
*/
GF_Err gf_rtp_streamer_send_data(GF_RTPStreamer *streamer, u8 *data, u32 size, u32 fullsize, u64 cts, u64 dts, Bool is_rap, Bool au_start, Bool au_end, u32 au_sn, u32 duration, u32 sampleDescriptionIndex);

/*! formats a generic SDP header
\param app_name application name, may be NULL
\param ip_dest destination IP address, shall NOT be NULL
\param session_name session name, may be NULL
\param iod64 base64 encoded MPEG-4 IOD, may be NULL
\return the SDP header - shall be destroyed by caller
*/
char *gf_rtp_streamer_format_sdp_header(char *app_name, char *ip_dest, char *session_name, char *iod64);

/*! disables RTCP keepalive
\param streamer the target RTP streamer
*/
void gf_rtp_streamer_disable_auto_rtcp(GF_RTPStreamer *streamer);

/*! sends RTCP bye packet
\param streamer the target RTP streamer
\return error if any
*/
GF_Err gf_rtp_streamer_send_bye(GF_RTPStreamer *streamer);

/*! sends RTCP server report
\param streamer the target RTP streamer
\param force_ts if GF_TRUE, forces using the indicatet ts
\param rtp_ts the force RTP timestamp to use
\param force_ntp_type if 0, computes NTP while sending. If 1 or 2, uses ntp_sec and ntp_frac for report. If 2, forces sending reports right away
\param ntp_sec NTP seconds
\param ntp_frac NTP fractional part
\return error if any
*/
GF_Err gf_rtp_streamer_send_rtcp(GF_RTPStreamer *streamer, Bool force_ts, u32 rtp_ts, u32 force_ntp_type, u32 ntp_sec, u32 ntp_frac);

/*! gets associated payload type
\param streamer the target RTP streamer
\return the payload type
*/
u8 gf_rtp_streamer_get_payload_type(GF_RTPStreamer *streamer);

/*! initializes the RTP channel for RTSP setup
\param streamer the target RTP streamer
\param path_mtu MTU path size in bytes
\param tr the RTSP transport description
\param ifce_addr IP address of network interface to use
\return error if any
*/
GF_Err gf_rtp_streamer_init_rtsp(GF_RTPStreamer *streamer, u32 path_mtu, GF_RTSPTransport *tr, const char *ifce_addr);

/*! gets the sequence number of the next RTP packet to be sent
\param streamer the target RTP streamer
\return the sequence number
*/
u16 gf_rtp_streamer_get_next_rtp_sn(GF_RTPStreamer *streamer);

/*! sets callback functions for RTP over RTSP sending
\param streamer the target RTP streamer
\param RTP_TCPCallback the callback function
\param cbk1 first opaque data passed to callback function
\param cbk2 second opaque data passed to callback function
\return error if any
*/
GF_Err gf_rtp_streamer_set_interleave_callbacks(GF_RTPStreamer *streamer, GF_Err (*RTP_TCPCallback)(void *cbk1, void *cbk2, Bool is_rtcp, u8 *pck, u32 pck_size), void *cbk1, void *cbk2);


/*! callback function for procesing RTCP  receiver reports
\param cbk user data passed to \ref  gf_rtp_streamer_read_rtcp
\param ssrc ssrc for this report, 0 if same as ssrc of channel
\param rtt_ms round-trip time estimate in ms
\param jitter_rtp_ts inter-arrival jitter in RTP channel timescale
\param loss_rate loss rate in per-thousands
*/
typedef void (*gf_rtcp_rr_callback)(void *cbk, u32 ssrc, u32 rtt_ms, u64 jitter_rtp_ts, u32 loss_rate);

/*! process rtcp reports if any
\param streamer the target RTP streamer
\param rtcp_cbk callback function for RTCP reports
\param udta user data to use for callback function
\return error if any, GF_EOS if empty
*/
GF_Err gf_rtp_streamer_read_rtcp(GF_RTPStreamer *streamer, gf_rtcp_rr_callback rtcp_cbk, void *udta);

/*! gets ssrc of this streamer
\param streamer the target RTP streamer
\return ssrc ID
*/
u32 gf_rtp_streamer_get_ssrc(GF_RTPStreamer *streamer);

/*! gets rtp timescale of this streamer
\param streamer the target RTP streamer
\return timescale
*/
u32 gf_rtp_streamer_get_timescale(GF_RTPStreamer *streamer);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif //GPAC_DISABLE_ISOM && GPAC_DISABLE_STREAMING

#endif		/*_GF_RTPSTREAMER_H_*/

