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

#include <gpac/bitstream.h>

GF_EXPORT
u32 gf_rtp_read_rtcp(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size)
{
	GF_Err e;
	u32 res;

	//only if the socket exist (otherwise RTSP interleaved channel)
	if (!ch || !ch->rtcp) return 0;
	if (ch->no_select) {
		e = gf_sk_receive_no_select(ch->rtcp, buffer, buffer_size, &res);
	} else {
		e = gf_sk_receive(ch->rtcp, buffer, buffer_size, &res);
	}
	if (e) return 0;
	return res;
}

GF_EXPORT
GF_Err gf_rtp_decode_rtcp(GF_RTPChannel *ch, u8 *pck, u32 pck_size, Bool *has_sr)
{
	GF_RTCPHeader rtcp_hdr;
	char sdes_buffer[300];
	u32 i, sender_ssrc, cur_ssrc, val, sdes_type, sdes_len, res, first;
	GF_Err e = GF_OK;

	if (has_sr) *has_sr = GF_FALSE;

	//bad RTCP packet
	if (pck_size < 4 ) return GF_NON_COMPLIANT_BITSTREAM;
	gf_bs_reassign_buffer(ch->bs_r, pck, pck_size);

	first = 1;
	while (pck_size) {
		//global header
		rtcp_hdr.Version = gf_bs_read_int(ch->bs_r, 2);
		if (rtcp_hdr.Version != 2 ) {
			return GF_NOT_SUPPORTED;
		}
		rtcp_hdr.Padding = gf_bs_read_int(ch->bs_r, 1);
		rtcp_hdr.Count = gf_bs_read_int(ch->bs_r, 5);
		rtcp_hdr.PayloadType = gf_bs_read_u8(ch->bs_r);
		rtcp_hdr.Length = 1 + gf_bs_read_u16(ch->bs_r);

		//check pck size
		if (pck_size < (u32) rtcp_hdr.Length * 4) {
			//we return OK
			return GF_CORRUPTED_DATA;
		}
		//substract this RTCP pck size
		pck_size -= rtcp_hdr.Length * 4;

		/*we read the RTCP header*/
		rtcp_hdr.Length -= 1;

		//in all RTCP Compounds (>1 pck), the first RTCP report SHALL be SR or RR without padding
		if (first) {
			if ( ( (rtcp_hdr.PayloadType!=200) && (rtcp_hdr.PayloadType!=201) )
			        || rtcp_hdr.Padding
			   ) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTCP] Corrupted RTCP packet: payload type %d (200 or 201 expected) - Padding %d (0 expected)\n", rtcp_hdr.PayloadType, rtcp_hdr.Padding));
				return GF_CORRUPTED_DATA;
			}
			first = 0;
		}

		//specific extensions
		switch (rtcp_hdr.PayloadType) {
		//Sender report - we assume there's only one sender
		case 200:
			/*sender ssrc*/
			sender_ssrc = gf_bs_read_u32(ch->bs_r);
			rtcp_hdr.Length -= 1;
			/*not for us...*/
			if (ch->SenderSSRC && (ch->SenderSSRC != sender_ssrc)) break;

			if (ch->first_SR) {
				gf_rtp_get_next_report_time(ch);
				ch->SenderSSRC = sender_ssrc;
			}
			ch->last_report_time = gf_rtp_get_report_time();

			ch->last_SR_NTP_sec = gf_bs_read_u32(ch->bs_r);
			ch->last_SR_NTP_frac = gf_bs_read_u32(ch->bs_r);
			ch->last_SR_rtp_time = gf_bs_read_u32(ch->bs_r);
			/*nb_pck =  */gf_bs_read_u32(ch->bs_r);
			/*nb_bytes =*/gf_bs_read_u32(ch->bs_r);

			rtcp_hdr.Length -= 5;
			if (has_sr) *has_sr = GF_TRUE;

#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_INFO))  {
#ifndef _WIN32_WCE
				time_t gtime = ch->last_SR_NTP_sec - GF_NTP_SEC_1900_TO_1970;
				const char *ascTime = asctime(gf_gmtime(&gtime));
#else
				const char *ascTime = "Not Available";
#endif
				GF_LOG(ch->first_SR ? GF_LOG_INFO : GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] RTCP %sSR: SSRC %d - RTP Time %u - Nb Pck %d - Nb Bytes %d - Time %s\n",
											   ch->first_SR ? "Initial " : "",
				                                 ch->SenderSSRC,
				                                 ch->last_SR_rtp_time,
				                                 ch->total_pck,
				                                 ch->total_bytes,
				                                 ascTime
				                                ));
			}
#endif

			ch->first_SR = 0;

			//common encoding for SR and RR
			goto process_reports;


		case 201:
			//sender ssrc
			/*sender_ssrc = */gf_bs_read_u32(ch->bs_r);
			rtcp_hdr.Length -= 1;

process_reports:

#if 0
			//process all reports - we actually don't since we do not handle sources
			//to add
			for (i=0; i<rtcp_hdr.Count; i++) {
				//ssrc slot
				cur_ssrc = gf_bs_read_u32(ch->bs_r);
				//frac lost
				gf_bs_read_u8(ch->bs_r);
				//cumulative lost
				gf_bs_read_u24(ch->bs_r);
				//extended seq num
				gf_bs_read_u32(ch->bs_r);
				//jitter
				gf_bs_read_u32(ch->bs_r);
				//LSR
				gf_bs_read_u32(ch->bs_r);
				//DLSR
				gf_bs_read_u32(ch->bs_r);

				rtcp_hdr.Length -= 6;
			}
			//remaining bytes? we skip (this includes padding and crypto - not supported)
#endif
			break;

		//SDES
		case 202:
			for (i=0; i<rtcp_hdr.Count; i++) {
				/*cur_ssrc = */gf_bs_read_u32(ch->bs_r);
				rtcp_hdr.Length -= 1;

				val = 0;
				while (1) {
					sdes_type = gf_bs_read_u8(ch->bs_r);
					val += 1;
					if (!sdes_type) break;
					sdes_len = gf_bs_read_u8(ch->bs_r);
					val += 1;
					gf_bs_read_data(ch->bs_r, sdes_buffer, sdes_len);
					sdes_buffer[sdes_len] = 0;
					val += sdes_len;
				}

				//re-align on 32bit
				res = val%4;
				if (res) {
					gf_bs_skip_bytes(ch->bs_r, (4-res));
					val = val/4 + 1;
				} else {
					val = val/4;
				}
				rtcp_hdr.Length -= val;
			}
			break;

		//BYE packet - close the channel - we work with 1 SSRC only */
		case 203:
			for (i=0; i<rtcp_hdr.Count; i++) {
				cur_ssrc = gf_bs_read_u32(ch->bs_r);
				rtcp_hdr.Length -= 1;
				if (ch->SenderSSRC == cur_ssrc) {
					e = GF_EOS;
					break;
				}
			}
			//extra info - skip it
			while (rtcp_hdr.Length) {
				gf_bs_read_u32(ch->bs_r);
				rtcp_hdr.Length -= 1;
			}
			break;
		/*
				//APP packet
				case 204:


					//sender ssrc
					sender_ssrc = gf_bs_read_u32(bs);
					//ASCI 4 char type
					gf_bs_read_u8(bs);
					gf_bs_read_u8(bs);
					gf_bs_read_u8(bs);
					gf_bs_read_u8(bs);

					rtcp_hdr.Length -= 2;

					//till endd of pck
					gf_bs_read_data(bs, sdes_buffer, rtcp_hdr.Length*4);
					rtcp_hdr.Length = 0;
					break;
		*/
		default:
			//read all till end
			gf_bs_read_data(ch->bs_r, sdes_buffer, rtcp_hdr.Length*4);
			rtcp_hdr.Length = 0;
			break;
		}
		//WE SHALL CONSUME EVERYTHING otherwise the packet is bad
		if (rtcp_hdr.Length) {
			return GF_CORRUPTED_DATA;
		}
	}
	return e;
}

static u32 RTCP_FormatReport(GF_RTPChannel *ch, GF_BitStream *bs, u32 NTP_Time)
{
	u32 length, is_sr, sec, frac, expected, val, size;
	s32 extended, expect_diff, loss_diff;
	Double f;

	is_sr = ch->pck_sent_since_last_sr ? 1 : 0;

	if (ch->forced_ntp_sec) {
		sec = ch->forced_ntp_sec;
		frac = ch->forced_ntp_frac;
		is_sr = 1;
	} else {
		gf_net_get_ntp(&sec, &frac);
	}

	//common header
	//version
	gf_bs_write_int(bs, 2, 2);
	//padding - reports are aligned
	gf_bs_write_int(bs, 0, 1);
	//count - only one for now in RR, 0 in sender mode
	gf_bs_write_int(bs, !is_sr, 5);
	//if we have sent stuff send an SR, otherwise an RR. We need to determine whether
	//we are active or not
	//type
	gf_bs_write_u8(bs, is_sr ? 200 : 201);
	//length = (num of 32bit words in full pck) - 1
	//we're updating only one ssrc for now in RR and none in SR
	length = is_sr ? 6 : (1 + 6 * 1);
	gf_bs_write_u16(bs, length);

	//sender SSRC
	gf_bs_write_u32(bs, ch->SSRC);

	size = 8;


	//SenderReport part
	if (is_sr) {
		//sender time
		gf_bs_write_u32(bs, sec);
		gf_bs_write_u32(bs, frac);
		//RTP time at this time
		f = 1000 * (sec - ch->last_pck_ntp_sec);
		f += ((frac - ch->last_pck_ntp_frac) >> 4) / 0x10000;
		f /= 1000;
		f *= ch->TimeScale;
		val = (u32) f + ch->last_pck_ts;
		gf_bs_write_u32(bs, val);
		//num pck sent
		gf_bs_write_u32(bs, ch->num_pck_sent);
		//num payload bytes sent
		gf_bs_write_u32(bs, ch->num_payload_bytes);


		size += 20;
		//nota: as we only support single-way channels we are done for SR...
		return size;
	}
	//loop through all our sources (1) and send information...
	gf_bs_write_u32(bs, ch->SenderSSRC);

	//Fraction lost and cumulative lost
	extended = ( (ch->num_sn_loops << 16) | ch->last_pck_sn);
	expected = extended - ch->rtp_first_SN;
	expect_diff = expected - ch->tot_num_pck_expected;
	loss_diff = expect_diff - ch->last_num_pck_rcv;

	if (!expect_diff || (loss_diff <= 0)) loss_diff = 0;
	else loss_diff = (loss_diff<<8) / expect_diff;

	gf_bs_write_u8(bs, loss_diff);

	//update and write cumulative loss
	ch->tot_num_pck_rcv += ch->last_num_pck_rcv;
	ch->tot_num_pck_expected = expected;
	gf_bs_write_u24(bs, (expected - ch->tot_num_pck_rcv));

	//Extend sequence number
	gf_bs_write_u32(bs, extended);


	//Jitter
	//RTP specs annexe A.8
	gf_bs_write_u32(bs, ( ch->Jitter >> 4));
	//LSR
	if (ch->last_SR_NTP_sec) {
		val = ( ((ch->last_SR_NTP_sec  & 0x0000ffff) << 16) |  ((ch->last_SR_NTP_frac & 0xffff0000) >> 16));
	} else {
		val = 0;
	}
	gf_bs_write_u32(bs, val);

	// DLSR
	gf_bs_write_u32(bs, (NTP_Time - ch->last_report_time));


#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_DEBUG))  {
#ifndef _WIN32_WCE
		time_t gtime = ch->last_SR_NTP_sec - GF_NTP_SEC_1900_TO_1970;
		const char *ascTime = asctime(gf_gmtime(&gtime));
#else
		const char *ascTime = "Not Available";
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] RTCP-RR: SSRC %d Jitter %d extended %d expect_diff %d loss_diff %d time %s\n",
		                                  ch->SSRC,
		                                  ch->Jitter >> 4,
		                                  extended,
		                                  expect_diff,
		                                  loss_diff,
		                                  ascTime
		                                 ));
	}
#endif

	size += 24;
	return size;
}


static u32 RTCP_FormatSDES(GF_RTPChannel *ch, GF_BitStream *bs)
{
	u32 length, padd;

	//we start with header and SSRC: 2x32 bits word
	length = 8;
	//ten only send one CName item is type (1byte) , len (1byte), data (len byte) +  NULL marker (0 on 1 byte) at the end of the item list
	length += 2 + (u32) strlen(ch->CName) + 1;
	//we padd the end of the content to 32-bit boundary, and set length to the number of 32 bit words
	padd = length % 4;
	if (padd*4 != 0) {
		//padding octets
		padd = 4 - padd;
		length = length/4 + 1;
	} else {
		padd = 0;
		length = length/4;
	}

	//common part as usual
	gf_bs_write_int(bs, 2, 2);
	//notify padding? according to RFC1889 "In a compound RTCP packet, padding should
	//only be required on the last individual packet because the compound packet is
	//encrypted as a whole" -> we write it without notifying it (this is a bit messy in
	//the spec IMO)
	gf_bs_write_int(bs, 0, 1);
	//report count is one
	gf_bs_write_int(bs, 1, 5);
	//SDES pck type
	gf_bs_write_u8(bs, 202);
	//write length minus one
	gf_bs_write_u16(bs, length - 1);

	//SSRC
	gf_bs_write_u32(bs, ch->SSRC);

	//CNAME type
	gf_bs_write_u8(bs, 1);
	//length and cname
	gf_bs_write_u8(bs, (u32) strlen(ch->CName));
	gf_bs_write_data(bs, ch->CName, (u32) strlen(ch->CName));

	gf_bs_write_u8(bs, 0);

	//32-align field with 0
	gf_bs_write_int(bs, 0, 8*padd);
	return (length + 1)*4;
}


static u32 RTCP_FormatBYE(GF_RTPChannel *ch, GF_BitStream *bs)
{
	//version
	gf_bs_write_int(bs, 2, 2);
	//no padding
	gf_bs_write_int(bs, 0, 1);
	//count - only one for now
	gf_bs_write_int(bs, 1, 5);
	//type=BYE
	gf_bs_write_u8(bs, 203);
	//length = (num of 32bit words in full pck) - 1
	gf_bs_write_u16(bs, 1);

	//sender SSRC
	gf_bs_write_u32(bs, ch->SSRC);
	return 8;
}

GF_EXPORT
GF_Err gf_rtp_send_bye(GF_RTPChannel *ch)
{
	GF_BitStream *bs;
	u32 report_size;
	u8 *report_buf;
	GF_Err e = GF_OK;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	/*k were received/sent send the RR/SR - note we don't wait for next Repor and force its emission now*/
	if (ch->last_num_pck_rcv || ch->pck_sent_since_last_sr) {
		RTCP_FormatReport(ch, bs, gf_rtp_get_report_time());
	}

	//always send SDES (CNAME shall be sent at each RTCP)
	RTCP_FormatSDES(ch, bs);

	//send BYE
	RTCP_FormatBYE(ch, bs);


	report_buf = NULL;
	report_size = 0;
	gf_bs_get_content(bs, &report_buf, &report_size);
	gf_bs_del(bs);

	if (ch->rtcp) {
		e = gf_sk_send(ch->rtcp, report_buf, report_size);
	} else {
		if (ch->send_interleave)
			e = ch->send_interleave(ch->interleave_cbk1, ch->interleave_cbk2, GF_TRUE, report_buf, report_size);
		else
			e = GF_BAD_PARAM;
	}
	gf_free(report_buf);
	return e;
}

GF_EXPORT
GF_Err gf_rtp_send_rtcp_report(GF_RTPChannel *ch)
{
	u32 Time, report_size;
	GF_BitStream *bs;
	u8 *report_buf;
	GF_Err e = GF_OK;
	

	/*skip first SR when acting as a receiver*/
	if (!ch->forced_ntp_sec && ch->first_SR) return GF_OK;
	Time = gf_rtp_get_report_time();
	if ( Time < ch->next_report_time) return GF_OK;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	//pck were received/sent send the RR/SR
	if (ch->last_num_pck_rcv || ch->pck_sent_since_last_sr || ch->forced_ntp_sec) {
		RTCP_FormatReport(ch, bs, Time);
	}

	//always send SDES (CNAME shall be sent at each RTCP)
	RTCP_FormatSDES(ch, bs);


	//get content
	report_buf = NULL;
	report_size = 0;
	gf_bs_get_content(bs, &report_buf, &report_size);
	gf_bs_del(bs);

	if (ch->rtcp) {
		e = gf_sk_send(ch->rtcp, report_buf, report_size);
	} else {
		if (ch->send_interleave)
			e = ch->send_interleave(ch->interleave_cbk1, ch->interleave_cbk2, GF_TRUE, report_buf, report_size);
		else
			e = GF_BAD_PARAM;
	}

	ch->rtcp_bytes_sent += report_size;

	gf_free(report_buf);

	if (!e) {
		//Update the channel record if no error - otherwise next RTCP will triger an RR
		ch->last_num_pck_rcv = ch->last_num_pck_expected = ch->last_num_pck_loss = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTCP] SSRC %d: sending RTCP report\n", ch->SSRC));
	}
	else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTCP] SSRC %d: error when sending RTCP report\n", ch->SSRC));
	}
	gf_rtp_get_next_report_time(ch);
	return e;
}

#if 0 //unused

#define RTCP_SAFE_FREE(p) if (p) gf_free(p);	\
					p = NULL;

GF_Err gf_rtp_set_info_rtcp(GF_RTPChannel *ch, u32 InfoCode, char *info_string)
{
	if (!ch) return GF_BAD_PARAM;

	switch (InfoCode) {
	case GF_RTCP_INFO_NAME:
		RTCP_SAFE_FREE(ch->s_name);
		if (info_string) ch->s_name = gf_strdup(info_string);
		break;
	case GF_RTCP_INFO_EMAIL:
		RTCP_SAFE_FREE(ch->s_email);
		if (info_string) ch->s_email = gf_strdup(info_string);
		break;
	case GF_RTCP_INFO_PHONE:
		RTCP_SAFE_FREE(ch->s_phone);
		if (info_string) ch->s_phone = gf_strdup(info_string);
		break;
	case GF_RTCP_INFO_LOCATION:
		RTCP_SAFE_FREE(ch->s_location);
		if (info_string) ch->s_location = gf_strdup(info_string);
		break;
	case GF_RTCP_INFO_TOOL:
		RTCP_SAFE_FREE(ch->s_tool);
		if (info_string) ch->s_tool = gf_strdup(info_string);
		break;
	case GF_RTCP_INFO_NOTE:
		RTCP_SAFE_FREE(ch->s_note);
		if (info_string) ch->s_note = gf_strdup(info_string);
		break;
	case GF_RTCP_INFO_PRIV:
		RTCP_SAFE_FREE(ch->s_priv);
		if (info_string) ch->s_name = gf_strdup(info_string);
		break;
	default:
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

#endif


#endif /*GPAC_DISABLE_STREAMING*/
