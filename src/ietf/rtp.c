/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/internal/ietf_dev.h>

#ifndef GPAC_DISABLE_STREAMING

#define MAX_RTP_SN	0x10000


GF_EXPORT
GF_RTPChannel *gf_rtp_new()
{
	GF_RTPChannel *tmp;
	GF_SAFEALLOC(tmp, GF_RTPChannel);
	if (!tmp)
		return NULL;
	tmp->first_SR = 1;
	tmp->SSRC = gf_rand();
	tmp->bs_r = gf_bs_new("d", 1, GF_BITSTREAM_READ);
	tmp->bs_w = gf_bs_new("d", 1, GF_BITSTREAM_WRITE);
	return tmp;
}

GF_EXPORT
void gf_rtp_del(GF_RTPChannel *ch)
{
	if (!ch) return;
	if (ch->rtp) gf_sk_del(ch->rtp);
	if (ch->rtcp) gf_sk_del(ch->rtcp);
	if (ch->net_info.source) gf_free(ch->net_info.source);
	if (ch->net_info.destination) gf_free(ch->net_info.destination);
	if (ch->net_info.Profile) gf_free(ch->net_info.Profile);
	if (ch->po) gf_rtp_reorderer_del(ch->po);
	if (ch->send_buffer) gf_free(ch->send_buffer);

	if (ch->CName) gf_free(ch->CName);
	if (ch->s_name) gf_free(ch->s_name);
	if (ch->s_email) gf_free(ch->s_email);
	if (ch->s_location) gf_free(ch->s_location);
	if (ch->s_phone) gf_free(ch->s_phone);
	if (ch->s_tool) gf_free(ch->s_tool);
	if (ch->s_note) gf_free(ch->s_note);
	if (ch->s_priv) gf_free(ch->s_priv);
	if (ch->bs_r) gf_bs_del(ch->bs_r);
	if (ch->bs_w) gf_bs_del(ch->bs_w);
	memset(ch, 0, sizeof(GF_RTPChannel));
	gf_free(ch);
}


GF_EXPORT
GF_Err gf_rtp_setup_transport(GF_RTPChannel *ch, GF_RTSPTransport *trans_info, const char *remote_address)
{
	if (!ch || !trans_info) return GF_BAD_PARAM;
	//assert we have at least ONE source ID
	if (!trans_info->source && !remote_address) return GF_BAD_PARAM;

	if (ch->net_info.destination) gf_free(ch->net_info.destination);
	ch->net_info.destination = NULL;
	if (ch->net_info.Profile) gf_free(ch->net_info.Profile);
	ch->net_info.Profile = NULL;
	if (ch->net_info.source) gf_free(ch->net_info.source);
	ch->net_info.source = NULL;
	memcpy(&ch->net_info, trans_info, sizeof(GF_RTSPTransport));

	if (trans_info->destination)
		ch->net_info.destination = gf_strdup(trans_info->destination);

	if (trans_info->Profile)
		ch->net_info.Profile = gf_strdup(trans_info->Profile);

	if (!ch->net_info.IsUnicast && trans_info->destination) {
		assert( trans_info->destination );
		ch->net_info.source = gf_strdup(trans_info->destination);
		if (ch->net_info.client_port_first) {
			ch->net_info.port_first = ch->net_info.client_port_first;
			ch->net_info.port_last = ch->net_info.client_port_last;
		}
	} else if (trans_info->source) {
		ch->net_info.source = gf_strdup(trans_info->source);
	} else {
		ch->net_info.source = gf_strdup(remote_address);
	}
	if (trans_info->SSRC) {
		if (trans_info->is_sender) {
			ch->SSRC = trans_info->SSRC;
		} else {
			ch->SenderSSRC = trans_info->SSRC;
		}
	}
	//check we REALLY have unicast or multicast
	if (gf_sk_is_multicast_address(ch->net_info.source) && ch->net_info.IsUnicast) return GF_SERVICE_ERROR;
	return GF_OK;
}


GF_EXPORT
void gf_rtp_reset_buffers(GF_RTPChannel *ch)
{
	if (ch->rtp) gf_sk_reset(ch->rtp);
	if (ch->rtcp) gf_sk_reset(ch->rtcp);
	if (ch->po) gf_rtp_reorderer_reset(ch->po);
	ch->first_SR = 1;
}

GF_EXPORT
void gf_rtp_reset_ssrc(GF_RTPChannel *ch)
{
	if (ch) ch->SenderSSRC = 0;
}

GF_EXPORT
void gf_rtp_enable_nat_keepalive(GF_RTPChannel *ch, u32 nat_timeout)
{
	if (ch) {
		ch->nat_keepalive_time_period = nat_timeout;
		ch->last_nat_keepalive_time = 0;
	}
}



GF_EXPORT
GF_Err gf_rtp_set_info_rtp(GF_RTPChannel *ch, u32 seq_num, u32 rtp_time, u32 ssrc)
{
	if (!ch) return GF_BAD_PARAM;
	ch->CurrentTime = ch->rtp_time = seq_num ? (1 + rtp_time) : 0;
	ch->last_pck_sn = 0;
	ch->rtp_first_SN = seq_num;
	ch->num_sn_loops = 0;
	//reset RTCP
	ch->ntp_init = 0;
	ch->first_SR = 1;
	if (ssrc) ch->SenderSSRC = ssrc;
	ch->total_pck = ch->total_bytes = ch->last_num_pck_rcv = ch->last_num_pck_expected = ch->last_num_pck_loss = ch->tot_num_pck_rcv = ch->tot_num_pck_expected = ch->rtcp_bytes_sent = 0;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_rtp_stop(GF_RTPChannel *ch)
{
	if (!ch) return GF_BAD_PARAM;
	if (ch->rtp) gf_sk_del(ch->rtp);
	ch->rtp = NULL;
	if (ch->rtcp) gf_sk_del(ch->rtcp);
	ch->rtcp = NULL;
	if (ch->po) gf_rtp_reorderer_del(ch->po);
	ch->po = NULL;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_rtp_initialize(GF_RTPChannel *ch, u32 UDPBufferSize, Bool IsSource, u32 PathMTU, u32 ReorederingSize, u32 MaxReorderDelay, char *local_ip)
{
	u16 port;
	GF_Err e;

	if (!ch || (IsSource && !PathMTU)) return GF_BAD_PARAM;

	if (ch->rtp) gf_sk_del(ch->rtp);
	ch->rtp = NULL;
	if (ch->rtcp) gf_sk_del(ch->rtcp);
	ch->rtcp = NULL;
	if (ch->po) gf_rtp_reorderer_del(ch->po);
	ch->po = NULL;

	ch->CurrentTime = 0;
	ch->rtp_time = 0;

	//create sockets for RTP/AVP profile only
	if (ch->net_info.Profile &&
	        ( !stricmp(ch->net_info.Profile, GF_RTSP_PROFILE_RTP_AVP)
	          || !stricmp(ch->net_info.Profile, "RTP/AVP/UDP")
	          || !stricmp(ch->net_info.Profile, "RTP/SAVP")
	        )
	   ) {
		//destination MUST be specified for unicast
		if (IsSource && ch->net_info.IsUnicast && !ch->net_info.destination) return GF_BAD_PARAM;

		/* forcing unicast when the address is not a multicast */
		if (!ch->net_info.IsUnicast) {
			if (IsSource) {
				if (ch->net_info.destination && !gf_sk_is_multicast_address(ch->net_info.destination)) {
					ch->net_info.IsUnicast = GF_TRUE;
				}
			} else {
				if (ch->net_info.source && !gf_sk_is_multicast_address(ch->net_info.source)) {
					ch->net_info.IsUnicast = GF_TRUE;
				}
			}
		}
		//
		//	RTP
		//
		ch->rtp = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (!ch->rtp) return GF_IP_NETWORK_FAILURE;
		if (ch->net_info.IsUnicast) {
			//if client, bind and connect the socket
			if (!IsSource) {
				port = ch->net_info.port_first;
				if (!port) port = ch->net_info.client_port_first;
				/*if a destination address was given (rtsd) use it*/
				if (!local_ip && ch->net_info.destination) local_ip = ch->net_info.destination;

				e = gf_sk_bind(ch->rtp, local_ip, ch->net_info.client_port_first, ch->net_info.source, port, GF_SOCK_REUSE_PORT);
				if (e) return e;
			}
			//else bind and set remote destination
			else {
				if (!ch->net_info.port_first) ch->net_info.port_first = ch->net_info.client_port_first;
				e = gf_sk_bind(ch->rtp, local_ip,ch->net_info.port_first, ch->net_info.destination, ch->net_info.client_port_first, GF_SOCK_REUSE_PORT | GF_SOCK_FAKE_BIND);
				if (e) return e;
			}
		} else {
			//Bind to multicast (auto-join the group).
			//we do not bind the socket if this is a source-only channel because some servers
			//don't like that on local loop ...
			e = gf_sk_setup_multicast(ch->rtp, ch->net_info.source, ch->net_info.port_first, ch->net_info.TTL, GF_FALSE, local_ip);
			if (e) return e;
		}
		if (UDPBufferSize) gf_sk_set_buffer_size(ch->rtp, IsSource, UDPBufferSize);

		//create re-ordering queue for UDP only, and receive
		if (ReorederingSize && !IsSource) {
			if (!MaxReorderDelay) MaxReorderDelay = 200;
			ch->po = gf_rtp_reorderer_new(ReorederingSize, MaxReorderDelay);
		}

		//
		//	RTCP
		//
		ch->rtcp = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (!ch->rtcp) return GF_IP_NETWORK_FAILURE;
		if (ch->net_info.IsUnicast) {
			if (!IsSource) {
				port = ch->net_info.port_last;
				if (!port) port = ch->net_info.client_port_last;
				/*if a destination address was given (rtsd) use it*/
				if (!local_ip && ch->net_info.destination) local_ip = ch->net_info.destination;

				e = gf_sk_bind(ch->rtcp, local_ip, ch->net_info.client_port_last, ch->net_info.source, port, GF_SOCK_REUSE_PORT);
				if (e) return e;
			} else {
				e = gf_sk_bind(ch->rtcp, local_ip, ch->net_info.port_last, ch->net_info.destination, ch->net_info.client_port_last, GF_SOCK_REUSE_PORT | GF_SOCK_FAKE_BIND);
				if (e) return e;
			}
		} else {
			if (!ch->net_info.port_last) ch->net_info.port_last = ch->net_info.client_port_last;
			//Bind to multicast (auto-join the group)
			e = gf_sk_setup_multicast(ch->rtcp, ch->net_info.source, ch->net_info.port_last, ch->net_info.TTL, GF_FALSE, local_ip);
			if (e) return e;
		}
	}

	if (IsSource) {
		if (ch->send_buffer) gf_free(ch->send_buffer);
		ch->send_buffer_size = PathMTU + 12;
		ch->send_buffer = (char *) gf_malloc(sizeof(char) * ch->send_buffer_size);
	}

	//format CNAME if not done yet
	if (!ch->CName) {
		//this is the real CName setup
		if (!ch->rtp) {
			ch->CName = gf_strdup("mpeg4rtp");
		} else {
			char name[GF_MAX_IP_NAME_LEN];

			size_t start;
			gf_get_user_name(name, 1024);
			if (strlen(name)) strcat(name, "@");
			start = strlen(name);
			//get host IP or loopback if error
			if (gf_sk_get_local_ip(ch->rtp, name+start) != GF_OK) strcpy(name+start, "127.0.0.1");
			ch->CName = gf_strdup(name);
		}
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_DEBUG))  {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] Packet Log Format: SSRC SequenceNumber TimeStamp NTP@recvTime deviance, Jiter, PckLost PckTotal BytesTotal\n"));
	}
#endif

	return GF_OK;
}

/*get the UTC time expressed in RTP timescale*/
u32 gf_rtp_channel_time(GF_RTPChannel *ch)
{
	u32 sec, frac, res;
	gf_net_get_ntp(&sec, &frac);
	res = ( (u32) ( (frac>>26)*ch->TimeScale) ) >> 6;
	res += ch->TimeScale*(sec - ch->ntp_init);
	return (u32) res;
}

u32 gf_rtp_get_report_time()
{
	u32 sec, frac;
	gf_net_get_ntp(&sec, &frac);
	/*in units of 1/65536 seconds*/
	return (u32) ( (frac>>16) + 0x10000L*sec );
}


void gf_rtp_get_next_report_time(GF_RTPChannel *ch)
{
	Double d;

	/*offset between .5 and 1.5 sec*/
	d = 0.5 + ((Double) gf_rand()) / ((Double) RAND_MAX);
	/*of a minimal 5sec interval expressed in 1/65536 of a sec*/
	d = 5.0 * d * 65536;
	/*we should estimate bandwidth sharing too*/
	ch->next_report_time = gf_rtp_get_report_time() + (u32) d;
}

GF_EXPORT
u32 gf_rtp_read_flush(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size)
{
	char *pck;
	u32 res = 0;
	if (!ch->po) return 0;

	//pck queue may need to be flushed
	pck = (char *) gf_rtp_reorderer_get(ch->po, &res, GF_TRUE);
	if (pck) {
		memcpy(buffer, pck, res);
		gf_free(pck);
	}
	return res;
}

GF_EXPORT
u32 gf_rtp_flush_rtp(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size)
{
	u32 res;
	char *pck;

	//only if the socket exist (otherwise RTSP interleaved channel)
	if (!ch || !ch->rtp || !ch->po) return 0;

	//pck queue may need to be flushed
	pck = (char *) gf_rtp_reorderer_get(ch->po, &res, GF_FALSE);
	if (pck) {
		memcpy(buffer, pck, res);
		gf_free(pck);
		return res;
	}
	return 0;
}

GF_EXPORT
u32 gf_rtp_read_rtp(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size)
{
	GF_Err e;
	u32 seq_num, res;
	char *pck;

	//only if the socket exist (otherwise RTSP interleaved channel)
	if (!ch || !ch->rtp) return 0;

	if (ch->no_select) {
		e = gf_sk_receive_no_select(ch->rtp, buffer, buffer_size, &res);
	} else {
		e = gf_sk_receive(ch->rtp, buffer, buffer_size, &res);
	}
	if (!res || e || (res < 12)) {
		assert(res==0);
		res = 0;
	}
	if (res) {
		ch->total_bytes+=res;
		ch->total_pck++;
	}
	//add the packet to our Queue if any
	if (ch->po) {
		if (res) {
			seq_num = ((((u32)buffer[2]) << 8) & 0xFF00) | (buffer[3] & 0xFF);
			gf_rtp_reorderer_add(ch->po, (void *) buffer, res, seq_num);
		}

		//pck queue may need to be flushed
		pck = (char *) gf_rtp_reorderer_get(ch->po, &res, GF_FALSE);
		if (pck) {
			memcpy(buffer, pck, res);
			gf_free(pck);
		}
	}
	/*monitor keep-alive period*/
	if (ch->nat_keepalive_time_period && !ch->send_interleave) {
		u32 now = gf_sys_clock();
		if (res) {
			ch->last_nat_keepalive_time = now;
		} else {
			if (now - ch->last_nat_keepalive_time >= ch->nat_keepalive_time_period) {
				char rtp_nat[12];
				rtp_nat[0] = (u8) 0xC0;
				rtp_nat[1] = ch->PayloadType;
				rtp_nat[2] = (ch->last_pck_sn>>8)&0xFF;
				rtp_nat[3] = (ch->last_pck_sn)&0xFF;
				rtp_nat[4] = (ch->last_pck_ts>>24)&0xFF;
				rtp_nat[5] = (ch->last_pck_ts>>16)&0xFF;
				rtp_nat[6] = (ch->last_pck_ts>>8)&0xFF;
				rtp_nat[7] = (ch->last_pck_ts)&0xFF;
				rtp_nat[8] = (ch->SenderSSRC>>24)&0xFF;
				rtp_nat[9] = (ch->SenderSSRC>>16)&0xFF;
				rtp_nat[10] = (ch->SenderSSRC>>8)&0xFF;
				rtp_nat[11] = (ch->SenderSSRC)&0xFF;
				e = gf_sk_send(ch->rtp, rtp_nat, 12);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Error sending NAT keep-alive packet: %s - disabling NAT\n", gf_error_to_string(e) ));
					ch->nat_keepalive_time_period = 0;
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] Sending NAT keep-alive packet - response %s\n", gf_error_to_string(e) ));
				}
				ch->last_nat_keepalive_time = now;
			}
		}
	}
	return res;
}


GF_EXPORT
GF_Err gf_rtp_decode_rtp(GF_RTPChannel *ch, u8 *pck, u32 pck_size, GF_RTPHeader *rtp_hdr, u32 *PayloadStart)
{
	GF_Err e;
	s32 deviance, delta;
	u32 CurrSeq, LastSeq;
	u32 ntp, lost, low16;

	if (!rtp_hdr) return GF_BAD_PARAM;
	e = GF_OK;

	gf_bs_reassign_buffer(ch->bs_r, pck, pck_size);
	//we need to uncompress the RTP header
	rtp_hdr->Version = gf_bs_read_int(ch->bs_r, 2);
	if (rtp_hdr->Version != 2) return GF_NOT_SUPPORTED;

	rtp_hdr->Padding = gf_bs_read_int(ch->bs_r, 1);
	rtp_hdr->Extension = gf_bs_read_int(ch->bs_r, 1);
	rtp_hdr->CSRCCount = gf_bs_read_int(ch->bs_r, 4);
	rtp_hdr->Marker = gf_bs_read_int(ch->bs_r, 1);
	rtp_hdr->PayloadType = gf_bs_read_int(ch->bs_r, 7);

	/*we don't support multiple CSRC now. Only one source (the server) is allowed*/
	if (rtp_hdr->CSRCCount) return GF_NOT_SUPPORTED;
	/*SeqNum*/
	rtp_hdr->SequenceNumber = gf_bs_read_u16(ch->bs_r);
	/*TS*/
	rtp_hdr->TimeStamp = gf_bs_read_u32(ch->bs_r);
	/*SSRC*/
	rtp_hdr->SSRC = gf_bs_read_u32(ch->bs_r);

	/*update RTP time if we didn't get the info*/
	if (!ch->rtp_time) {
		ch->rtp_time = 1 + rtp_hdr->TimeStamp;
		ch->rtp_first_SN = rtp_hdr->SequenceNumber;
		ch->num_sn_loops = 0;
	}
	if (ch->first_SR && !ch->SenderSSRC && rtp_hdr->SSRC) {
		ch->SenderSSRC = rtp_hdr->SSRC;
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTP] Assigning SSRC to %d because none was specified through SDP/RTSP\n", ch->SenderSSRC));
	}

	if (!ch->ntp_init && ch->SenderSSRC && (ch->SenderSSRC != rtp_hdr->SSRC) ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] SSRC mismatch: %d vs %d\n", rtp_hdr->SSRC, ch->SenderSSRC));
		return GF_IP_NETWORK_EMPTY;
	}


	/*RTP specs annexe A.8*/
	if (!ch->ntp_init) {
		gf_net_get_ntp(&ch->ntp_init, &lost);
		ch->last_pck_sn = (u32) rtp_hdr->SequenceNumber-1;
	}
	/*this is a loop in SN - add it*/
	if ( (ch->last_pck_sn + 1 > rtp_hdr->SequenceNumber)
	        && (rtp_hdr->SequenceNumber >= ch->last_pck_sn + MAX_RTP_SN/2)) {
		ch->num_sn_loops += 1;
	}

	if (ch->last_SR_rtp_time) {
		s64 diff_sec;
		u32 sec = ch->last_SR_NTP_sec;
		s64 frac;

		frac = (s64) rtp_hdr->TimeStamp;
		frac -= (s64) ch->last_SR_rtp_time;
		if (frac<0) frac += 0xFFFFFFFF;
		diff_sec = frac / ch->TimeScale;

		frac -= diff_sec*ch->TimeScale;
		frac *= 0xFFFFFFFF;
		frac /= ch->TimeScale;
		frac += ch->last_SR_NTP_frac;
		if (frac>0xFFFFFFFF) {
			sec += 1;
			frac -= 0xFFFFFFFF;
		}
		rtp_hdr->recomputed_ntp_ts = sec + (s32) diff_sec;
		rtp_hdr->recomputed_ntp_ts <<= 32;
		rtp_hdr->recomputed_ntp_ts |= frac;
	}

	ntp = gf_rtp_channel_time(ch);
	deviance = ntp - rtp_hdr->TimeStamp;
	delta = deviance - ch->last_deviance;
	ch->last_deviance = deviance;

	if (delta < 0) delta = -delta;
	ch->Jitter += delta - ( (ch->Jitter + 8) >> 4);

	lost = 0;
	LastSeq = ch->last_pck_sn;
	CurrSeq = (u32) rtp_hdr->SequenceNumber;
	ch->packet_loss = GF_FALSE;
	/*next sequential pck*/
	if ( ( (LastSeq + 1) & 0xffff ) == CurrSeq ) {
		ch->last_num_pck_rcv += 1;
		ch->last_num_pck_expected += 1;
	}
	/*repeated pck*/
	else if ( (LastSeq & 0xffff ) == CurrSeq ) {
		ch->last_num_pck_rcv += 1;
	}
	/*drop pck*/
	else {
		low16 = LastSeq & 0xffff;
		if ( CurrSeq > low16 )
			lost = CurrSeq - low16;
		else
			lost = 0xffff - low16 + CurrSeq + 1;

		ch->last_num_pck_expected += lost;
		ch->last_num_pck_rcv += 1;
		ch->last_num_pck_loss += lost;
		ch->packet_loss = GF_TRUE;
	}
	ch->last_pck_sn = CurrSeq;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_DEBUG))  {
		ch->total_bytes += pck_size-12;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP]\t%d\t%d\t%u\t%d\t%d\t%d\t%d\t%d\t%d\n",
		                                  ch->SenderSSRC,
		                                  rtp_hdr->SequenceNumber,
		                                  rtp_hdr->TimeStamp,
		                                  ntp,
		                                  delta,
		                                  ch->Jitter >> 4,
		                                  lost,
		                                  ch->total_pck,
		                                  ch->total_bytes
		                                 ));
	}
#endif

	//we work with no CSRC so payload offset is always 12
	*PayloadStart = 12;

	if (rtp_hdr->Extension) {
		u16 ext_size;
		char *payl = pck + *PayloadStart;
		ext_size = payl[2];
		ext_size <<= 8;
		ext_size |= payl[3];
		*PayloadStart += 4 + (ext_size * 4);
	}

	//store the time
	ch->CurrentTime = rtp_hdr->TimeStamp;
	return e;
}


GF_EXPORT
Double gf_rtp_get_current_time(GF_RTPChannel *ch)
{
	Double ret;
	if (!ch || !ch->rtp_time) return 0.0;
	ret = (Double) ch->CurrentTime;
	ret -= (Double) (ch->rtp_time-1);
	ret /= ch->TimeScale;
	return ret;
}






GF_EXPORT
GF_Err gf_rtp_send_packet(GF_RTPChannel *ch, GF_RTPHeader *rtp_hdr, u8 *pck, u32 pck_size, Bool fast_send)
{
	GF_Err e;
	u32 i, Start;
	char *hdr = NULL;

	if (!ch || !rtp_hdr
	        || !ch->send_buffer
	        || !pck
	        || (rtp_hdr->CSRCCount > 15)) return GF_BAD_PARAM;

	if (rtp_hdr->CSRCCount) fast_send = GF_FALSE;

	if (12 + pck_size + 4*rtp_hdr->CSRCCount > ch->send_buffer_size)
		return GF_IO_ERR;

	if (fast_send) {
		hdr = pck - 12;
		gf_bs_reassign_buffer(ch->bs_w, hdr, 12);
	} else {
		gf_bs_reassign_buffer(ch->bs_w, ch->send_buffer, ch->send_buffer_size);
	}
	//write header
	gf_bs_write_int(ch->bs_w, rtp_hdr->Version, 2);
	gf_bs_write_int(ch->bs_w, rtp_hdr->Padding, 1);
	gf_bs_write_int(ch->bs_w, rtp_hdr->Extension, 1);
	gf_bs_write_int(ch->bs_w, rtp_hdr->CSRCCount, 4);
	gf_bs_write_int(ch->bs_w, rtp_hdr->Marker, 1);
	gf_bs_write_int(ch->bs_w, rtp_hdr->PayloadType, 7);
	gf_bs_write_u16(ch->bs_w, rtp_hdr->SequenceNumber);
	gf_bs_write_u32(ch->bs_w, rtp_hdr->TimeStamp);
	gf_bs_write_u32(ch->bs_w, ch->SSRC);

	for (i=0; i<rtp_hdr->CSRCCount; i++) {
		gf_bs_write_u32(ch->bs_w, rtp_hdr->CSRC[i]);
	}
	//nb: RTP header is always aligned
	Start = (u32) gf_bs_get_position(ch->bs_w);

	if (ch->send_interleave) {
		if (fast_send) {
			e = ch->send_interleave(ch->interleave_cbk1, ch->interleave_cbk2, GF_FALSE, hdr, pck_size+12);
		} else {
			memcpy(ch->send_buffer + Start, pck, pck_size);
			e = ch->send_interleave(ch->interleave_cbk1, ch->interleave_cbk2, GF_FALSE, ch->send_buffer, Start + pck_size);
		}
	}
	//copy payload
	else if (fast_send) {
		e = gf_sk_send(ch->rtp, hdr, pck_size+12);
	} else {
		memcpy(ch->send_buffer + Start, pck, pck_size);
		e = gf_sk_send(ch->rtp, ch->send_buffer, Start + pck_size);
	}
	if (e) return e;

	//Update RTCP for sender reports
	ch->pck_sent_since_last_sr += 1;
	if (ch->first_SR) {
		//get a new report time
		gf_rtp_get_next_report_time(ch);
		ch->num_payload_bytes = 0;
		ch->num_pck_sent = 0;
		ch->first_SR = 0;
	}

	ch->num_payload_bytes += pck_size;
	ch->num_pck_sent += 1;
	//store timing
	ch->last_pck_ts = rtp_hdr->TimeStamp;
	gf_net_get_ntp(&ch->last_pck_ntp_sec, &ch->last_pck_ntp_frac);

	if (!ch->no_auto_rtcp) gf_rtp_send_rtcp_report(ch);
	return GF_OK;
}

GF_EXPORT
u32 gf_rtp_is_unicast(GF_RTPChannel *ch)
{
	if (!ch) return 0;
	return ch->net_info.IsUnicast;
}

GF_EXPORT
u32 gf_rtp_is_interleaved(GF_RTPChannel *ch)
{
	if (!ch || !ch->net_info.Profile) return 0;
	return ch->net_info.IsInterleaved;
}

GF_EXPORT
u32 gf_rtp_get_clockrate(GF_RTPChannel *ch)
{
	if (!ch || !ch->TimeScale) return 0;
	return ch->TimeScale;
}

#if 0 //unused
u32 gf_rtp_is_active(GF_RTPChannel *ch)
{
	if (!ch) return 0;
	if (!ch->rtp_first_SN && !ch->rtp_time) return 0;
	return 1;
}
#endif

GF_EXPORT
u8 gf_rtp_get_low_interleave_id(GF_RTPChannel *ch)
{
	if (!ch || !ch->net_info.IsInterleaved) return 0;
	return ch->net_info.rtpID;
}

GF_EXPORT
u8 gf_rtp_get_hight_interleave_id(GF_RTPChannel *ch)
{
	if (!ch || !ch->net_info.IsInterleaved) return 0;
	return ch->net_info.rtcpID;
}


#define RTP_DEFAULT_FIRSTPORT		7040

static u16 NextAvailablePort = 0;

GF_EXPORT
GF_Err gf_rtp_set_ports(GF_RTPChannel *ch, u16 first_port)
{
	u16 p;
	GF_Socket *sock;
	if (!ch) return GF_BAD_PARAM;

	if (!NextAvailablePort) {
		NextAvailablePort = first_port ? first_port : RTP_DEFAULT_FIRSTPORT;
	}
	p = NextAvailablePort;
	if (ch->net_info.client_port_first) return GF_OK;

	sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (!sock) return GF_IO_ERR;

	/*should be way enough (more than 100 rtp streams open on the machine)*/
	while (1) {
		/*try to bind without reuse. If fails this means the port is used on the machine, don't reuse it*/
		GF_Err e = gf_sk_bind(sock, NULL, p, NULL, 0, 0);
		if (e==GF_OK) break;
		if (e!=GF_IP_CONNECTION_FAILURE) {
			gf_sk_del(sock);
			return GF_IP_NETWORK_FAILURE;
		}
		p+=2;
	}
	gf_sk_del(sock);
	ch->net_info.client_port_first = p;
	ch->net_info.client_port_last = p + 1;
	NextAvailablePort = p + 2;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_rtp_setup_payload(GF_RTPChannel *ch, u32 PayloadType, u32 ClockRate)
{
	if (!ch || !PayloadType || !ClockRate) return GF_BAD_PARAM;
	ch->PayloadType = PayloadType;
	ch->TimeScale = ClockRate;
	return GF_OK;
}

GF_EXPORT
GF_RTSPTransport *gf_rtp_get_transport(GF_RTPChannel *ch)
{
	if (!ch) return NULL;
	return &ch->net_info;
}

#if 0 //unused
u32 gf_rtp_get_local_ssrc(GF_RTPChannel *ch)
{
	if (!ch) return 0;
	return ch->SSRC;
}
#endif

#if 0
"#RTP log format:\n"
"#RTP SenderSSRC RTP_TimeStamp RTP_SeqNum NTP@Recv Deviance Jitter NbLost NbTotPck NbTotBytes\n"
"#RTCP Sender reports log format:\n"
"#RTCP-SR SenderSSRC RTP_TimeStamp@NTP NbTotPck NbTotBytes NTP\n"
"#RTCP Receiver reports log format:\n"
"#RTCP-RR StreamSSRC Jitter ExtendedSeqNum ExpectDiff LossDiff NTP\n"
#endif

GF_EXPORT
Float gf_rtp_get_loss(GF_RTPChannel *ch)
{
	if (!ch->tot_num_pck_expected) return 0.0f;
	return 100.0f - (100.0f * ch->tot_num_pck_rcv) / ch->tot_num_pck_expected;
}

GF_EXPORT
u32 gf_rtp_get_tcp_bytes_sent(GF_RTPChannel *ch)
{
	return ch->rtcp_bytes_sent;
}

GF_EXPORT
void gf_rtp_get_ports(GF_RTPChannel *ch, u16 *rtp_port, u16 *rtcp_port)
{
	if (ch->net_info.client_port_first) {
		if (rtp_port) *rtp_port = ch->net_info.client_port_first;
		if (rtcp_port) *rtcp_port = ch->net_info.client_port_last;
	} else {
		if (rtp_port) *rtp_port = ch->net_info.port_first;
		if (rtcp_port) *rtcp_port = ch->net_info.port_last;
	}
}


/*
	RTP packet reorderer
*/

#define SN_CHECK_OFFSET		0x0A

GF_EXPORT
GF_RTPReorder *gf_rtp_reorderer_new(u32 MaxCount, u32 MaxDelay)
{
	GF_RTPReorder *tmp;

	if (MaxCount <= 1 || !MaxDelay) return NULL;

	GF_SAFEALLOC(tmp , GF_RTPReorder);
	if (!tmp) return NULL;
	tmp->MaxCount = MaxCount;
	tmp->MaxDelay = MaxDelay;
	return tmp;
}

GF_EXPORT
void gf_rtp_reorderer_del(GF_RTPReorder *po)
{
	gf_rtp_reorderer_reset(po);
	gf_free(po);
}

GF_EXPORT
void gf_rtp_reorderer_reset(GF_RTPReorder *po)
{
	GF_POItem *item;
	if (!po) return;

	item = po->in;
	while (item) {
		GF_POItem *next = item->next;
		gf_free(item->pck);
		gf_free(item);
		item = next;
	}

	po->head_seqnum = 0;
	po->Count = 0;
	po->IsInit = 0;
	po->in = NULL;
}

GF_EXPORT
GF_Err gf_rtp_reorderer_add(GF_RTPReorder *po, const void * pck, u32 pck_size, u32 pck_seqnum)
{
	GF_POItem *it, *cur;
	u32 bounds;

	if (!po) return GF_BAD_PARAM;

	it = (GF_POItem *) gf_malloc(sizeof(GF_POItem));
	it->pck_seq_num = pck_seqnum;
	it->next = NULL;
	it->size = pck_size;
	it->pck = gf_malloc(pck_size);
	memcpy(it->pck, pck, pck_size);
	/*reset timeout*/
	po->LastTime = 0;

	//no input, this packet will be the input
	if (!po->in) {
		//the seq num was not initialized
		if (!po->head_seqnum) {
			po->head_seqnum = pck_seqnum;
		} else if (!po->IsInit) {
			//this is not in our current range for init
			if (ABSDIFF(po->head_seqnum, pck_seqnum) > SN_CHECK_OFFSET) goto discard;
			po->IsInit = 1;
		}

		po->in = it;
		po->Count += 1;
		return GF_OK;
	}

	//this is 16 bitr seq num, as we work with RTP only for now
	bounds = 0;
	if ( (po->head_seqnum >= 0xf000 ) || (po->head_seqnum <= 0x1000) ) bounds = 0x2000;

	//first check the head of the list
	//same seq num, we drop
	if (po->in->pck_seq_num == pck_seqnum) goto discard;

	if ( ( (u16) (pck_seqnum + bounds) <= (u16) (po->in->pck_seq_num + bounds) ) ) {

		it->next = po->in;
		po->in = it;
		po->Count += 1;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[rtp] Packet Reorderer: inserting packet %d at head\n", pck_seqnum));
		return GF_OK;
	}

	//no, insert at the right place
	cur = po->in;

	while (1) {
		//same seq num, we drop
		if (cur->pck_seq_num == pck_seqnum) goto discard;

		//end of list
		if (!cur->next) {
			cur->next = it;
			po->Count += 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[rtp] Packet Reorderer: Appending packet %d (last %d)\n", pck_seqnum, cur->pck_seq_num));
			return GF_OK;
		}

		//are we in the bounds ??
		if ( ( (u16) (cur->pck_seq_num + bounds) < (u16) (pck_seqnum + bounds) )
		        && ( (u16) (pck_seqnum + bounds) < (u16) (cur->next->pck_seq_num + bounds)) ) {

			//insert
			it->next = cur->next;
			cur->next = it;
			po->Count += 1;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[rtp] Packet Reorderer: Inserting packet %d\n", pck_seqnum));
			//done
			return GF_OK;
		}
		cur = cur->next;
	}


discard:
	gf_free(it->pck);
	gf_free(it);
	GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[rtp] Packet Reorderer: Dropping packet %d\n", pck_seqnum));
	return GF_OK;
}

//retrieve the first available packet. Note that the behavior will be undefined if the first
//ever received packet if its SeqNum was unknown
//the BUFFER is yours, you must delete it
GF_EXPORT
void *gf_rtp_reorderer_get(GF_RTPReorder *po, u32 *pck_size, Bool force_flush)
{
	GF_POItem *t;
	u32 bounds;
	void *ret;

	if (!po || !pck_size) return NULL;

	*pck_size = 0;

	//empty queue
	if (!po->in) return NULL;

	//check we have received the first packet
	if ( po->head_seqnum && po->MaxCount
	        && (po->MaxCount > po->Count)
	        && (po->in->pck_seq_num != po->head_seqnum))
		return NULL;

	//no entry
	if (!po->in->next) goto check_timeout;

	bounds = 0;
	if ( (po->head_seqnum >= 0xf000 ) || (po->head_seqnum <= 0x1000) ) bounds = 0x2000;

	//release the output if SN in order or maxCount reached
	if (( (u16) (po->in->pck_seq_num + bounds + 1) == (u16) (po->in->next->pck_seq_num + bounds))
	        || (po->MaxCount && (po->Count >= po->MaxCount)) ) {

#ifndef GPAC_DISABLE_LOG
		if (po->in->pck_seq_num + 1 != po->in->next->pck_seq_num)
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[rtp] WARNING Packet Loss: Sending %d out of the queue but next is %d\n", po->in->pck_seq_num, po->in->next->pck_seq_num ));
#endif
		goto send_it;
	}
	//update timing
	else {
check_timeout:

		if (force_flush) goto send_it;

		if (!po->LastTime) {
			po->LastTime = gf_sys_clock();
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[rtp] Packet Reorderer: starting timeout at %d\n", po->LastTime));
			return NULL;
		}
		//if exceeding the delay send the head
		if (gf_sys_clock() - po->LastTime >= po->MaxDelay) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[rtp] Packet Reorderer: Forcing output after %d ms wait (max allowed %d)\n", gf_sys_clock() - po->LastTime, po->MaxDelay));
			goto send_it;
		}
	}
	return NULL;


send_it:
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[rtp] Packet Reorderer: Fetching %d\n", po->in->pck_seq_num));
	*pck_size = po->in->size;
	t = po->in;
	po->in = po->in->next;
	//no other output. reset the head seqnum
	po->head_seqnum = po->in ? po->in->pck_seq_num : 0;
	po->Count -= 1;
	//release the item
	ret = t->pck;
	gf_free(t);
	return ret;
}

GF_Err gf_rtp_set_interleave_callbacks(GF_RTPChannel *ch, GF_Err (*RTP_TCPCallback)(void *cbk1, void *cbk2, Bool is_rtcp, u8 *pck, u32 pck_size), void *cbk1, void *cbk2)
{
	if (!ch) return GF_BAD_PARAM;
	ch->send_interleave = RTP_TCPCallback;
	ch->interleave_cbk1 = cbk1;
	ch->interleave_cbk2 = cbk2;
	return GF_OK;
}


#endif /*GPAC_DISABLE_STREAMING*/

