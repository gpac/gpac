/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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
 *	\file <gpac/rtp_streamer.h>
 *	\brief RTP streaming (packetizer and RTP socket).
 */

/*!
 *	\ingroup media_grp
 *	\brief RTPStreamer object
 *
 *	This section documents the RTP streamer object of the GPAC framework.
 *	@{
 */

#include <gpac/ietf.h>
#include <gpac/isomedia.h>

#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_ISOM)

typedef struct __rtp_streamer GF_RTPStreamer;


/*!
 *	\brief RTP Streamer constructor with extended parameters
 *
 *	Constructs a new RTP file streamer
 *\param streamType type of the stream (GF_STREAM_* as defined in <gpac/constants.h>)
 *\param codecid codec ID for the stream (GF_CODEC_* as defined in <gpac/constants.h>)
 *\param timeScale unit to express timestamps
 *\param ip_dest IP address of the destination
 *\param port port number of the destination
 *\param MTU Maximum Transmission Unit size to use
 *\param TTL Time To Leave
 *\param ifce_addr IP of the local interface to use (may be NULL)
 *\param flags set of RTP flags passed to the streamer
 *\param dsi MPEG-4 Decoder Specific Info for the stream
 *\param dsi_len length of the dsi parameter
 *\param PayloadType RTP payload type
 *\param sample_rate audio sample rate
 *\param nb_ch number of channels in audio streams
 *\param is_crypted Boolean indicating if the stream is crypted
 *\param IV_length lenght of the Initialisation Vector used for encryption
 *\param KI_length length of the key index
 *\param MinSize minimum AU size, 0 if unknown
 *\param MaxSize maximum AU size, 0 if unknown
 *\param avgTS average TS delta in timeScale, 0 if unknown
 *\param maxDTSDelta maximum DTS delta in timeScale, 0 if unknown
 *\param const_dur constant duration in timeScale, 0 if unknown
 *\param bandwidth bandwidth, 0 if unknown
 *\param max_ptime maximum packet duration in timeScale, 0 if unknown
 *\param au_sn_len length of the MPEG-4 SL descriptor AU sequence number field, 0 if unknown
 *\param for_rtsp indicates this is an RTP channel in an RTSP session, RTP channel will not be created, use \ref gf_rtp_streamer_init_rtsp
 *\return new object
 */
GF_RTPStreamer *gf_rtp_streamer_new(u32 streamType, u32 codecid, u32 timeScale,
        const char *ip_dest, u16 port, u32 MTU, u8 TTL, const char *ifce_addr,
        u32 flags, u8 *dsi, u32 dsi_len,
        u32 PayloadType, u32 sample_rate, u32 nb_ch,
        Bool is_crypted, u32 IV_length, u32 KI_length,
        u32 MinSize, u32 MaxSize, u32 avgTS, u32 maxDTSDelta, u32 const_dur, u32 bandwidth, u32 max_ptime, u32 au_sn_len, Bool for_rtsp);

/*!
 *	\brief RTP file streamer destructor
 *
 *	Destructs an RTP file streamer
 *	\param streamer object to destruct
 */
void gf_rtp_streamer_del(GF_RTPStreamer *streamer);

/*!
 *	\brief gets the SDP file
 *
 *	Gets the SDP asscoiated with all media in the streaming session (only media parts are returned)
 *	\param rtp RTP streamer object
 *	\param ESID The MPEG-4 elementary stream id of the stream to process
 *	\param dsi The MPEG-4 decoder specific info data
 *	\param dsi_len length of The MPEG-4 decoder specific info data
 *	\param KMS_URI URI of the Key Management System
 *	\param out_sdp_buffer location to the SDP buffer to allocate and fill
 */
GF_Err gf_rtp_streamer_append_sdp(GF_RTPStreamer *rtp, u16 ESID, u8 *dsi, u32 dsi_len, char *KMS_URI, char **out_sdp_buffer);

GF_Err gf_rtp_streamer_append_sdp_extended(GF_RTPStreamer *rtp, u16 ESID, u8 *dsi, u32 dsi_len, u8 *dsi_enh, u32 dsi_enh_len, char *KMS_URI, u32 width, u32 height, u32 tw, u32 th, s32 tx, s32 ty, s16 tl, Bool for_rtsp, char **out_sdp_buffer);

GF_Err gf_rtp_streamer_send_au(GF_RTPStreamer *rtp, u8 *data, u32 size, u64 cts, u64 dts, Bool is_rap);

GF_Err gf_rtp_streamer_send_au_with_sn(GF_RTPStreamer *rtp, u8 *data, u32 size, u64 cts, u64 dts, Bool is_rap, u32 inc_au_sn);

GF_Err gf_rtp_streamer_send_data(GF_RTPStreamer *rtp, u8 *data, u32 size, u32 fullsize, u64 cts, u64 dts, Bool is_rap, Bool au_start, Bool au_end, u32 au_sn, u32 sampleDuration, u32 sampleDescIndex);

char *gf_rtp_streamer_format_sdp_header(char *app_name, char *ip_dest, char *session_name, char *iod64);

void gf_rtp_streamer_disable_auto_rtcp(GF_RTPStreamer *streamer);

GF_Err gf_rtp_streamer_send_bye(GF_RTPStreamer *streamer);

GF_Err gf_rtp_streamer_send_rtcp(GF_RTPStreamer *streamer, Bool force_ts, u32 rtp_ts, u32 force_ntp_type, u32 ntp_sec, u32 ntp_frac);

u8 gf_rtp_streamer_get_payload_type(GF_RTPStreamer *streamer);

GF_Err gf_rtp_streamer_init_channel(GF_RTPStreamer *rtp, u32 path_mtu, const char * dest, int port, int ttl, const char *ifce_addr);

GF_Err gf_rtp_streamer_init_rtsp(GF_RTPStreamer *rtp, u32 path_mtu, GF_RTSPTransport  *tr, const char *ifce_addr);

u16 gf_rtp_streamer_get_next_rtp_sn(GF_RTPStreamer *streamer);

GF_Err gf_rtp_streamer_set_interleave_callbacks(GF_RTPStreamer *streamer, GF_Err (*RTP_TCPCallback)(void *cbk1, void *cbk2, Bool is_rtcp, u8 *pck, u32 pck_size), void *cbk1, void *cbk2);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif //GPAC_DISABLE_ISOM && GPAC_DISABLE_STREAMING

#endif		/*_GF_RTPSTREAMER_H_*/

