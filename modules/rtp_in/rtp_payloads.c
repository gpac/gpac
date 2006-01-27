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

#include "rtp_in.h"
#include <gpac/avparse.h>


u32 payt_get_type(RTPClient *rtp, GF_RTPMap *map, GF_SDPMedia *media)
{
	u32 i, j;

	if (!stricmp(map->payload_name, "MP4V-ES") ) return GP_RTP_PAYT_MPEG4;
	else if (!stricmp(map->payload_name, "mpeg4-generic")) return GP_RTP_PAYT_MPEG4;
	else if (!stricmp(map->payload_name, "enc-mpeg4-generic")) return GP_RTP_PAYT_MPEG4;
	/*optibase mm400 card hack*/
	else if (!stricmp(map->payload_name, "enc-generic-mp4") ) {
		free(map->payload_name);
		map->payload_name = strdup("enc-mpeg4-generic");
		return GP_RTP_PAYT_MPEG4;
	}

	/*LATM: only without multiplexing (not tested but should be straight AUs)*/
	else if (!stricmp(map->payload_name, "MP4A-LATM")) {
		GF_SDP_FMTP *fmtp;
		i=0;
		while ((fmtp = gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			//this is our payload. check cpresent is 0
			j=0;
			while ((att = gf_list_enum(fmtp->Attributes, &j))) {
				if (!stricmp(att->Name, "cpresent") && atoi(att->Value)) return 0;
			}
		}
		return GP_RTP_PAYT_LATM;
	}
	else if (!stricmp(map->payload_name, "MPA") || !stricmp(map->payload_name, "MPV")) return GP_RTP_PAYT_MPEG12;
	else if (!stricmp(map->payload_name, "H263-1998") || !stricmp(map->payload_name, "H263-2000")) return GP_RTP_PAYT_H263;
	else if (!stricmp(map->payload_name, "AMR")) return GP_RTP_PAYT_AMR;
	else if (!stricmp(map->payload_name, "AMR-WB")) return GP_RTP_PAYT_AMR_WB;
	else if (!stricmp(map->payload_name, "3gpp-tt")) return GP_RTP_PAYT_3GPP_TEXT;
	else if (!stricmp(map->payload_name, "H264")) return GP_RTP_PAYT_H264_AVC;
	else return 0;
}


static GF_Err payt_set_param(RTPStream *ch, char *param_name, char *param_val)
{
	u32 i, val;
	char valS[3];
	GF_BitStream *bs;

	if (!ch || !param_name) return GF_BAD_PARAM;

	/*1 - mpeg4-generic / RFC 3016 payload type items*/

	/*PL (not needed when IOD is here)*/
	if (!stricmp(param_name, "Profile-level-id")) {
		if (ch->rtptype==GP_RTP_PAYT_H264_AVC) {
			sscanf(param_val, "%x", &ch->sl_map.PL_ID);
		} else {
			ch->sl_map.PL_ID = atoi(param_val);
		}
	}
	/*decoder specific info (not needed when IOD is here)*/
	else if (!stricmp(param_name, "config")) {
		//decode the buffer - the string buffer is MSB hexadecimal
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		valS[2] = 0;
		for (i=0; i<strlen(param_val);i+=2) {
			valS[0] = param_val[i];
			valS[1] = param_val[i+1];
			sscanf(valS, "%x", &val);
			gf_bs_write_u8(bs, val);
		}
		if (ch->sl_map.config) free(ch->sl_map.config);
		ch->sl_map.config = NULL;
		gf_bs_get_content(bs, (unsigned char **) &ch->sl_map.config, &ch->sl_map.configSize);
		gf_bs_del(bs);
	}
	/*mpeg4-generic payload type items required*/
	
	/*constant size (size of all AUs) */
	else if (!stricmp(param_name, "ConstantSize")) {
		ch->sl_map.ConstantSize = atoi(param_val);
	}
	/*constant size (size of all AUs) */
	else if (!stricmp(param_name, "ConstantDuration")) {
		ch->sl_map.ConstantDuration = atoi(param_val);
	}
	/*object type indication (not needed when IOD is here)*/
	else if (!stricmp(param_name, "ObjectType")) {
		ch->sl_map.ObjectTypeIndication = atoi(param_val);
	}
	else if (!stricmp(param_name, "StreamType")) ch->sl_map.StreamType = atoi(param_val);
	else if (!stricmp(param_name, "mode")) {
		strcpy(ch->sl_map.mode, param_val);
		/*in case no IOD and no streamType/OTI in the file*/
		if (!stricmp(param_val, "AAC-hbr") || !stricmp(param_val, "AAC-lbr") || !stricmp(param_val, "CELP-vbr") || !stricmp(param_val, "CELP-cbr")) {
			ch->sl_map.StreamType = GF_STREAM_AUDIO;
			ch->sl_map.ObjectTypeIndication = 0x40;
		}

	}

	else if (!stricmp(param_name, "DTSDeltaLength")) ch->sl_map.DTSDeltaLength = atoi(param_val);
	else if (!stricmp(param_name, "CTSDeltaLength")) ch->sl_map.CTSDeltaLength = atoi(param_val);
	else if (!stricmp(param_name, "SizeLength")) ch->sl_map.SizeLength = atoi(param_val);
	else if (!stricmp(param_name, "IndexLength")) ch->sl_map.IndexLength = atoi(param_val);
	else if (!stricmp(param_name, "IndexDeltaLength")) ch->sl_map.IndexDeltaLength = atoi(param_val);
	else if (!stricmp(param_name, "RandomAccessIndication")) ch->sl_map.RandomAccessIndication = atoi(param_val);
	else if (!stricmp(param_name, "StreamStateIndication")) ch->sl_map.StreamStateIndication = atoi(param_val);
	else if (!stricmp(param_name, "AuxiliaryDataSizeLength")) ch->sl_map.AuxiliaryDataSizeLength = atoi(param_val);

	/*H264/AVC config - we only handle mode 0 and 1*/
	else if (!stricmp(param_name, "packetization-mode")) ch->packetization_mode = 1;
	/*AMR config*/
	else if (!stricmp(param_name, "octet-align")) ch->flags |= CH_AMR_Align;
	/*ISMACryp config*/
	else if (!stricmp(param_name, "ISMACrypCryptoSuite")) {
		if (!stricmp(param_val, "AES_CTR_128")) ch->isma_scheme = GF_FOUR_CHAR_INT('i','A','E','C');
		else ch->isma_scheme = 0;
	}
	else if (!stricmp(param_name, "ISMACrypSelectiveEncryption")) {
		if (!stricmp(param_val, "1") || !stricmp(param_val, "true"))
			ch->flags |= CH_UseSelEnc;
		else
			ch->flags &= ~CH_UseSelEnc;
	}
	else if (!stricmp(param_name, "ISMACrypIVLength")) ch->sl_map.IV_length = atoi(param_val);
	else if (!stricmp(param_name, "ISMACrypDeltaIVLength")) ch->sl_map.IV_delta_length = atoi(param_val);
	else if (!stricmp(param_name, "ISMACrypKeyIndicatorLength")) ch->sl_map.KI_length = atoi(param_val);
	else if (!stricmp(param_name, "ISMACrypKeyIndicatorPerAU")) {
		if (!stricmp(param_val, "1") || !stricmp(param_val, "true"))
			ch->flags |= CH_UseKeyIDXPerAU;
		else 
			ch->flags &= ~CH_UseKeyIDXPerAU;
	}
	else if (!stricmp(param_name, "ISMACrypKey")) ch->key = strdup(param_val);
	
	
	return GF_OK;
}

u32 payt_setup(RTPStream *ch, GF_RTPMap *map, GF_SDPMedia *media)
{
	u32 i, j;
	GF_SDP_FMTP *fmtp;

	/*reset sl map*/
	memset(&ch->sl_map, 0, sizeof(GP_RTPSLMap));
	
	/*setup channel*/
	gf_rtp_setup_payload(ch->rtp_ch, map);

	if (!stricmp(map->payload_name, "enc-mpeg4-generic")) ch->flags |= CH_HasISMACryp;

	/*then process all FMTPs*/
	i=0;
	while ((fmtp = gf_list_enum(media->FMTP, &i))) {
		GF_X_Attribute *att;
		//we work with only one PayloadType for now
		if (fmtp->PayloadType != map->PayloadType) continue;
		j=0;
		while ((att = gf_list_enum(fmtp->Attributes, &j))) {
			payt_set_param(ch, att->Name, att->Value);
		}
	}

	switch (ch->rtptype) {
	case GP_RTP_PAYT_LATM:
	{
		u32 AudioMuxVersion, AllStreamsSameTime, numSubFrames, numPrograms, numLayers;
		GF_M4ADecSpecInfo cfg;
		char *latm_dsi = ch->sl_map.config;
		GF_BitStream *bs = gf_bs_new(latm_dsi, ch->sl_map.configSize, GF_BITSTREAM_READ);
		AudioMuxVersion = gf_bs_read_int(bs, 1);
		AllStreamsSameTime = gf_bs_read_int(bs, 1);
		numSubFrames = gf_bs_read_int(bs, 6);
		numPrograms = gf_bs_read_int(bs, 4);
		numLayers = gf_bs_read_int(bs, 3);

		if (AudioMuxVersion || !AllStreamsSameTime || numSubFrames || numPrograms || numLayers) {
			gf_bs_del(bs);
			return 0;
		}
		memset(&cfg, 0, sizeof(cfg));
		cfg.base_object_type = gf_bs_read_int(bs, 5);
		cfg.base_sr_index = gf_bs_read_int(bs, 4);
		if (cfg.base_sr_index == 0x0F) {
			cfg.base_sr = gf_bs_read_int(bs, 24);
		} else {
			cfg.base_sr = GF_M4ASampleRates[cfg.base_sr_index];
		}
		cfg.nb_chan = gf_bs_read_int(bs, 4);
		if (cfg.base_object_type==5) {
			cfg.has_sbr = 1;
			cfg.sbr_sr_index = gf_bs_read_int(bs, 4);
			if (cfg.sbr_sr_index == 0x0F) {
				cfg.sbr_sr = gf_bs_read_int(bs, 24);
			} else {
				cfg.sbr_sr = GF_M4ASampleRates[cfg.sbr_sr_index];
			}
			cfg.sbr_object_type = gf_bs_read_int(bs, 5);
		}
		gf_bs_del(bs);
		free(ch->sl_map.config);
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*write as regular AAC*/
		gf_bs_write_int(bs, cfg.base_object_type, 5);
		gf_bs_write_int(bs, cfg.base_sr_index, 4);
		gf_bs_write_int(bs, cfg.nb_chan, 4);
		gf_bs_align(bs);
		gf_bs_get_content(bs, (unsigned char **) &ch->sl_map.config, &ch->sl_map.configSize);
		gf_bs_del(bs);
		ch->sl_map.StreamType = GF_STREAM_AUDIO;
		ch->sl_map.ObjectTypeIndication = 0x40;
	}
		break;
	case GP_RTP_PAYT_MPEG4:
		/*mark if AU header is present*/
		ch->sl_map.auh_first_min_len = 0;
		if (ch->flags & CH_HasISMACryp) {
			if (!ch->isma_scheme) ch->isma_scheme = GF_FOUR_CHAR_INT('i','A','E','C');
			if (!ch->sl_map.IV_length) ch->sl_map.IV_length = 4;

			if (ch->flags & CH_UseSelEnc) ch->sl_map.auh_first_min_len += 8;
			else ch->sl_map.auh_first_min_len += 8*(ch->sl_map.IV_length+ch->sl_map.KI_length);
		}
		ch->sl_map.auh_first_min_len += ch->sl_map.CTSDeltaLength;
		ch->sl_map.auh_first_min_len += ch->sl_map.DTSDeltaLength;
		ch->sl_map.auh_first_min_len += ch->sl_map.SizeLength;
		ch->sl_map.auh_first_min_len += ch->sl_map.RandomAccessIndication;
		ch->sl_map.auh_first_min_len += ch->sl_map.StreamStateIndication;
		ch->sl_map.auh_min_len = ch->sl_map.auh_first_min_len;
		ch->sl_map.auh_first_min_len += ch->sl_map.IndexLength;
		ch->sl_map.auh_min_len += ch->sl_map.IndexDeltaLength;
		/*RFC3016 flags*/
		if (!stricmp(map->payload_name, "MP4V-ES")) {
			ch->sl_map.StreamType = GF_STREAM_VISUAL;
			ch->sl_map.ObjectTypeIndication = 0x20;
		}
		else if (!stricmp(map->payload_name, "MP4A-LATM")) {
			ch->sl_map.StreamType = GF_STREAM_AUDIO;
			ch->sl_map.ObjectTypeIndication = 0x40;
		}
		/*MPEG-4 video, check RAPs if not indicated*/
		if ((ch->sl_map.StreamType == GF_STREAM_VISUAL) && (ch->sl_map.ObjectTypeIndication == 0x20) && !ch->sl_map.RandomAccessIndication) {
			ch->flags |= CH_M4V_CheckRAP;
		}
		break;
	case GP_RTP_PAYT_MPEG12:
		if (!stricmp(map->payload_name, "MPA")) {
			ch->sl_map.StreamType = GF_STREAM_AUDIO;
			ch->sl_map.ObjectTypeIndication = 0x69;
		}
		else if (!stricmp(map->payload_name, "MPV")) {
			/*we signal RAPs*/
			ch->sl_map.RandomAccessIndication = 1;
			ch->sl_map.StreamType = GF_STREAM_VISUAL;
			/*FIXME: how to differentiate MPEG1 from MPEG2 video before any frame is received??*/
			ch->sl_map.ObjectTypeIndication = 0x6A;
		}
		break;
	case GP_RTP_PAYT_AMR:
	case GP_RTP_PAYT_AMR_WB:
		{
			GF_BitStream *bs;
			ch->sl_map.StreamType = GF_STREAM_AUDIO;
			ch->sl_map.ObjectTypeIndication = GPAC_EXTRA_CODECS_OTI;
			/*create DSI*/
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (ch->rtptype == GP_RTP_PAYT_AMR) {
				gf_bs_write_u32(bs, GF_FOUR_CHAR_INT('s', 'a', 'm', 'r'));
			} else {
				gf_bs_write_u32(bs, GF_FOUR_CHAR_INT('s', 'a', 'w', 'b'));
			}
			gf_bs_write_int(bs, 0, 5*8);
			gf_bs_get_content(bs, (unsigned char **) &ch->sl_map.config, &ch->sl_map.configSize);
			gf_bs_del(bs);
		}
		break;
	case GP_RTP_PAYT_H263:
		{
			u32 x, y, w, h;
			GF_X_Attribute *att;
			GF_BitStream *bs;
			x = y = w = h = 0;
			j=0;
			while ((att = gf_list_enum(media->Attributes, &j))) {
				if (stricmp(att->Name, "cliprect")) continue;
				/*only get the display area*/
				sscanf(att->Value, "%d,%d,%d,%d", &y, &x, &h, &w);
			}

			ch->sl_map.StreamType = GF_STREAM_VISUAL;
			ch->sl_map.ObjectTypeIndication = GPAC_EXTRA_CODECS_OTI;
			/*create DSI*/
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, GF_FOUR_CHAR_INT('h', '2', '6', '3'));
			gf_bs_write_u16(bs, w);
			gf_bs_write_u16(bs, h);
			gf_bs_get_content(bs, (unsigned char **) &ch->sl_map.config, &ch->sl_map.configSize);
			gf_bs_del(bs);
			/*we signal RAPs*/
			ch->sl_map.RandomAccessIndication = 1;
		}
		break;
	case GP_RTP_PAYT_3GPP_TEXT:
	{
		char *tx3g, *a_tx3g;
		GF_BitStream *bs;
		u32 nb_desc;
		GF_SDP_FMTP *fmtp;
		GF_TextConfig tcfg;
		memset(&tcfg, 0, sizeof(GF_TextConfig));
		tcfg.tag = GF_ODF_TEXT_CFG_TAG;
		tcfg.Base3GPPFormat = 0x10;
		tcfg.MPEGExtendedFormat = 0x10;
		tcfg.profileLevel = 0x10;
		tcfg.timescale = ch->clock_rate;
		tcfg.sampleDescriptionFlags = 1;
		tx3g = NULL;

		i=0;
		while ((fmtp = gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			j=0;
			while ((att = gf_list_enum(fmtp->Attributes, &j))) {

				if (!stricmp(att->Name, "width")) tcfg.text_width = atoi(att->Value);
				else if (!stricmp(att->Name, "height")) tcfg.text_height = atoi(att->Value);
				else if (!stricmp(att->Name, "tx")) tcfg.horiz_offset = atoi(att->Value);
				else if (!stricmp(att->Name, "ty")) tcfg.vert_offset = atoi(att->Value);
				else if (!stricmp(att->Name, "layer")) tcfg.layer = atoi(att->Value);
				else if (!stricmp(att->Name, "max-w")) tcfg.video_width = atoi(att->Value);
				else if (!stricmp(att->Name, "max-h")) tcfg.video_height = atoi(att->Value);
				else if (!stricmp(att->Name, "tx3g")) tx3g = att->Value;
			}
		}
		if (!tx3g) return 0;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, tcfg.Base3GPPFormat);
		gf_bs_write_u8(bs, tcfg.MPEGExtendedFormat); /*MPEGExtendedFormat*/
		gf_bs_write_u8(bs, tcfg.profileLevel); /*profileLevel*/
		gf_bs_write_u24(bs, tcfg.timescale);
		gf_bs_write_int(bs, 0, 1);	/*no alt formats*/
		gf_bs_write_int(bs, tcfg.sampleDescriptionFlags, 2);
		gf_bs_write_int(bs, 1, 1);	/*we will write sample desc*/
		gf_bs_write_int(bs, 1, 1);	/*video info*/
		gf_bs_write_int(bs, 0, 3);	/*reserved, spec doesn't say the values*/
		gf_bs_write_u8(bs, tcfg.layer);
		gf_bs_write_u16(bs, tcfg.text_width);
		gf_bs_write_u16(bs, tcfg.text_height);
		/*get all tx3g (comma separated)*/
		nb_desc = 1;
		a_tx3g = tx3g;
		while ((a_tx3g = strchr(a_tx3g, ',')) ) {
			a_tx3g ++;
			nb_desc ++;
		}
		a_tx3g = tx3g;
		gf_bs_write_u8(bs, nb_desc);
		while (1) {
			char *next_tx3g, szOut[1000];
			u32 len;
			strcpy(a_tx3g, tx3g);
			next_tx3g = strchr(a_tx3g, ',');
			if (next_tx3g) next_tx3g[0] = 0;
			len = gf_base64_decode(a_tx3g, strlen(a_tx3g), szOut, 1000);
			gf_bs_write_data(bs, szOut, len);
			tx3g = strchr(tx3g, ',');
			if (!tx3g) break;
			tx3g += 1;
			while (tx3g[0] == ' ') tx3g += 1;
		}

		/*write video cfg*/
		gf_bs_write_u16(bs, tcfg.video_width);
		gf_bs_write_u16(bs, tcfg.video_height);
		gf_bs_write_u16(bs, tcfg.horiz_offset);
		gf_bs_write_u16(bs, tcfg.vert_offset);
		gf_bs_get_content(bs, (unsigned char **)&ch->sl_map.config, &ch->sl_map.configSize);
		ch->sl_map.StreamType = GF_STREAM_TEXT;
		ch->sl_map.ObjectTypeIndication = 0x08;
		gf_bs_del(bs);
	}
		break;
	case GP_RTP_PAYT_H264_AVC:
	{
		GF_SDP_FMTP *fmtp;
		GF_AVCConfig *avcc = gf_odf_avc_cfg_new();
		avcc->AVCProfileIndication = (ch->sl_map.PL_ID>>16) & 0xFF;
		avcc->profile_compatibility = (ch->sl_map.PL_ID>>8) & 0xFF;
		avcc->AVCLevelIndication = ch->sl_map.PL_ID & 0xFF;
		avcc->configurationVersion = 1;
		avcc->nal_unit_size = 4;
		ch->sl_map.StreamType = 4;
		ch->sl_map.ObjectTypeIndication = 0x21;
		/*we will signal RAPs*/
		ch->sl_map.RandomAccessIndication = 1;
		/*rewrite sps and pps*/
		i=0;
		while ((fmtp = gf_list_enum(media->FMTP, &i))) {
			GF_X_Attribute *att;
			if (fmtp->PayloadType != map->PayloadType) continue;
			j=0;
			while ((att = gf_list_enum(fmtp->Attributes, &j))) {
				char *nal_ptr;
				if (stricmp(att->Name, "sprop-parameter-sets")) continue;

				nal_ptr = att->Value;
				while (nal_ptr) {
					u32 nalt, b64size, ret, idx = 0;
					char *b64_d;

					while (nal_ptr[idx]) {
						if (nal_ptr[idx]==',') break;
						idx++;
					}
					if (!nal_ptr[idx]) {
						idx = 0;
						b64size = strlen(nal_ptr);
					} else {
						assert(idx);
						b64size = idx;
					}
					b64_d = malloc(sizeof(char)*b64size);
					ret = gf_base64_decode(nal_ptr, b64size, b64_d, b64size); 
					b64_d[ret] = 0;
					nalt = b64_d[0] & 0x1F;
					if (/*SPS*/(nalt==0x07) || /*PPS*/(nalt==0x08)) {
						GF_AVCConfigSlot *sl = malloc(sizeof(GF_AVCConfigSlot));
						sl->size = ret;
						sl->data = malloc(sizeof(char)*sl->size);
						memcpy(sl->data, b64_d, sizeof(char)*sl->size);
						if (nalt==0x07) {
							gf_list_add(avcc->sequenceParameterSets, sl);
						} else {
							gf_list_add(avcc->pictureParameterSets, sl);
						}
					}
					free(b64_d);
					if (!idx) break;
					nal_ptr += idx+1;
				}
			}
		}
		gf_odf_avc_cfg_write(avcc, &ch->sl_map.config, &ch->sl_map.configSize);
		gf_odf_avc_cfg_del(avcc);
	}
		break;

	}
	return 1;
}


void RP_ParsePayloadMPEG4(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	u32 aux_size, au_size, first_idx, au_hdr_size, pay_start, num_au;
	s32 au_idx;
	GF_BitStream *hdr_bs, *aux_bs;

	hdr_bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	aux_bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);

//	printf("parsing packet %d size %d ts %d M %d\n", hdr->SequenceNumber, size, hdr->TimeStamp, hdr->Marker);

	/*global AU header len*/
	au_hdr_size = 0;
	if (ch->sl_map.auh_first_min_len) {
		au_hdr_size = gf_bs_read_u16(hdr_bs);
		gf_bs_read_u16(aux_bs);
	}

	/*jump to aux section, skip it and get payload start*/
	gf_bs_read_int(aux_bs, au_hdr_size);
	gf_bs_align(aux_bs);
	if (ch->sl_map.AuxiliaryDataSizeLength) {
		aux_size = gf_bs_read_int(aux_bs, ch->sl_map.AuxiliaryDataSizeLength);
		gf_bs_read_int(aux_bs, aux_size);
		gf_bs_align(aux_bs);
	}
	pay_start = (u32) gf_bs_get_position(aux_bs);
	gf_bs_del(aux_bs);

	first_idx = 0;
	au_idx = 0;

	ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	ch->sl_hdr.decodingTimeStamp = hdr->TimeStamp;

	num_au = 0;

	ch->sl_hdr.accessUnitEndFlag = hdr->Marker;
	/*override some defaults for RFC 3016*/
	if (ch->flags & CH_NewAU) {
		ch->sl_hdr.accessUnitStartFlag = 1;
	} else {
		ch->sl_hdr.accessUnitStartFlag = 0;
	}
	ch->sl_hdr.randomAccessPointFlag = 0;

	while (1) {
		/*get default AU size*/
		au_size = ch->sl_map.ConstantSize;
		/*not signaled, assume max one AU per packet*/
		if (!au_size) au_size = size - pay_start;
		
		if ((!num_au && ch->sl_map.auh_first_min_len) || (num_au && ch->sl_map.auh_min_len)) {
			/*ISMACryp*/
			if (ch->flags & CH_HasISMACryp) {
				ch->sl_hdr.isma_encrypted = 1;
				if (ch->flags & CH_UseSelEnc) {
					ch->sl_hdr.isma_encrypted = gf_bs_read_int(hdr_bs, 1);
					gf_bs_read_int(hdr_bs, 7);
				}
				/*Note: ISMACryp ALWAYS indicates IV (BSO) and KEYIDX, even when sample is not encrypted. 
				This is quite a waste when using selective encryption....*/
				if (!num_au) {
					ch->sl_hdr.isma_BSO = gf_bs_read_int(hdr_bs, 8*ch->sl_map.IV_length);
				}
				/*NOT SUPPORTED YET*/
				else if (ch->sl_map.IV_delta_length) {
					ch->sl_hdr.isma_BSO += gf_bs_read_int(hdr_bs, 8*ch->sl_map.IV_delta_length);
				}
				if (ch->sl_map.KI_length) {
					/*NOT SUPPORTED YET*/
					if (!num_au || !(ch->flags & CH_UseKeyIDXPerAU) ) {
						gf_bs_skip_bytes(hdr_bs, ch->sl_map.KI_length);
					}
				}
			}

			/*AU size*/
			if (ch->sl_map.SizeLength) {
				au_size = gf_bs_read_int(hdr_bs, ch->sl_map.SizeLength);
				if (au_size > size - pay_start) au_size = size - pay_start;
				au_hdr_size -= ch->sl_map.SizeLength;
			}
			/*AU index*/
			if (! num_au) {
				au_idx = first_idx = gf_bs_read_int(hdr_bs, ch->sl_map.IndexLength);
				au_hdr_size -= ch->sl_map.IndexLength;
			} else {
				au_idx += 1 + (s32) gf_bs_read_int(hdr_bs, ch->sl_map.IndexDeltaLength);
				au_hdr_size -= ch->sl_map.IndexDeltaLength;
			}
			/*CTS flag*/
			if (ch->sl_map.CTSDeltaLength) {
				ch->sl_hdr.compositionTimeStampFlag = gf_bs_read_int(hdr_bs, 1);
				au_hdr_size -= 1;
			} else {
				/*get CTS from IDX*/
				if (ch->sl_map.ConstantDuration) {
					ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp + (au_idx - first_idx) * ch->sl_map.ConstantDuration;
				} else {
					ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp + (au_idx - first_idx) * ch->unit_duration;
				}
			}

			/*CTS in-band*/
			if (ch->sl_hdr.compositionTimeStampFlag) {
				ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp + (s32) gf_bs_read_int(hdr_bs, ch->sl_map.CTSDeltaLength);
				au_hdr_size -= ch->sl_map.CTSDeltaLength;
			}
			/*DTS flag is always present (needed for reconstruction of TSs in case of packet loss)*/
			if (ch->sl_map.DTSDeltaLength) {
				ch->sl_hdr.decodingTimeStampFlag = gf_bs_read_int(hdr_bs, 1);
				au_hdr_size -= 1;
			} else {
				/*NO DTS otherwise*/
				ch->sl_hdr.decodingTimeStampFlag = 0;
			}
			if (ch->sl_hdr.decodingTimeStampFlag) {
				s32 ts = hdr->TimeStamp - (s32) gf_bs_read_int(hdr_bs, ch->sl_map.DTSDeltaLength);
				ch->sl_hdr.decodingTimeStamp = (ts>0) ? ts : 0;
				au_hdr_size -= ch->sl_map.DTSDeltaLength;
			}
			/*RAP flag*/
			if (ch->sl_map.RandomAccessIndication) {
				ch->sl_hdr.randomAccessPointFlag = gf_bs_read_int(hdr_bs, 1);
				au_hdr_size -= 1;
			}
			/*stream state - map directly to seqNum*/
			if (ch->sl_map.StreamStateIndication) {
				ch->sl_hdr.AU_sequenceNumber = gf_bs_read_int(hdr_bs, ch->sl_map.StreamStateIndication);
				au_hdr_size -= ch->sl_map.StreamStateIndication;
			} else {
				ch->sl_hdr.AU_sequenceNumber = au_idx;
			}
		}
		/*no header present, update CTS/DTS - note we're sure there's no interleaving*/
		else {
			if (num_au) {
				ch->sl_hdr.compositionTimeStamp += ch->sl_map.ConstantDuration;
				ch->sl_hdr.decodingTimeStamp += ch->sl_map.ConstantDuration;
			}
		}
		/*we cannot map RTP SN to SL SN since an RTP packet may carry several SL ones - only inc by 1 seq nums*/
		ch->sl_hdr.packetSequenceNumber += 1;

		/*force indication of CTS whenever we have a new AU*/
		
		ch->sl_hdr.compositionTimeStampFlag = (ch->flags & CH_NewAU) ? 1 : 0;

		/*locate VOP start code*/
		if (ch->sl_hdr.accessUnitStartFlag && (ch->flags & CH_M4V_CheckRAP)) {
			u32 i;
			Bool is_rap = 0;
			unsigned char *pay = payload + pay_start;
			i=0;
			while (i<au_size-4) {
				if (!pay[i] && !pay[i+1] && (pay[i+2]==1) && (pay[i+3]==0xB6)) {
					is_rap = ((pay[i+4] & 0xC0)==0) ? 1 : 0;
					break;
				}
				i++;
			}
			if (is_rap) ch->sl_hdr.randomAccessPointFlag = 1;
		}

		if (ch->owner->first_packet_drop && (ch->sl_hdr.packetSequenceNumber >= ch->owner->first_packet_drop) ) {
			if ( (ch->sl_hdr.packetSequenceNumber - ch->owner->first_packet_drop) % ch->owner->frequency_drop)
				gf_term_on_sl_packet(ch->owner->service, ch->channel, payload + pay_start, au_size, &ch->sl_hdr, GF_OK);
		} else {
			gf_term_on_sl_packet(ch->owner->service, ch->channel, payload + pay_start, au_size, &ch->sl_hdr, GF_OK);
		}

		ch->sl_hdr.compositionTimeStampFlag = 0;
		
		if (ch->flags & CH_HasISMACryp) ch->sl_hdr.isma_BSO += au_size;

		if (au_hdr_size < ch->sl_map.auh_min_len) break;
		pay_start += au_size;
		if (pay_start >= size) break;
		num_au ++;
	}

	if (hdr->Marker)
		ch->flags |= CH_NewAU;
	else
		ch->flags &= ~CH_NewAU;

	gf_bs_del(hdr_bs);
}


void RP_ParsePayloadMPEG12Audio(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	u16 offset;
	u32 mp3hdr, ts;
	GF_BitStream *bs;

	ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	ch->sl_hdr.decodingTimeStamp = hdr->TimeStamp;

	ch->sl_hdr.accessUnitStartFlag = ch->sl_hdr.accessUnitEndFlag ? 1 : 0;
	if (ch->flags & CH_NewAU) ch->sl_hdr.accessUnitStartFlag = 1;

	/*get frag header*/
	bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	gf_bs_read_u16(bs);
	offset = gf_bs_read_u16(bs);
	gf_bs_del(bs);
	payload += 4;
	size -= 4;
	mp3hdr = 0;
	while (1) {

		/*frame start if no offset*/
		ch->sl_hdr.accessUnitStartFlag = offset ? 0 : 1;

		/*new frame, store size*/
		ch->sl_hdr.compositionTimeStampFlag = 0;
		if (ch->sl_hdr.accessUnitStartFlag) {
			mp3hdr = GF_FOUR_CHAR_INT((u8) payload[0], (u8) payload[1], (u8) payload[2], (u8) payload[3]);
			ch->sl_hdr.accessUnitLength = gf_mp3_frame_size(mp3hdr);
			ch->sl_hdr.compositionTimeStampFlag = 1;
		}
		if (!ch->sl_hdr.accessUnitLength) break;
		/*fragmented frame*/
		if (ch->sl_hdr.accessUnitLength>size) {
			gf_term_on_sl_packet(ch->owner->service, ch->channel, payload, ch->sl_hdr.accessUnitLength, &ch->sl_hdr, GF_OK);
			ch->sl_hdr.accessUnitLength -= size;
			ch->sl_hdr.accessUnitStartFlag = ch->sl_hdr.accessUnitEndFlag = 0;
			return;
		}
		/*complete frame*/
		ch->sl_hdr.accessUnitEndFlag = 1;
		gf_term_on_sl_packet(ch->owner->service, ch->channel, payload, ch->sl_hdr.accessUnitLength, &ch->sl_hdr, GF_OK);
		payload += ch->sl_hdr.accessUnitLength;
		size -= ch->sl_hdr.accessUnitLength;
		ch->sl_hdr.accessUnitLength = 0;
		
		/*if fragmented there shall not be other frames in the packet*/
		if (!ch->sl_hdr.accessUnitStartFlag) return;
		if (!size) break;
		offset = 0;
		/*get ts*/
		ts = gf_mp3_window_size(mp3hdr);
		ch->sl_hdr.compositionTimeStamp += ts;
		ch->sl_hdr.decodingTimeStamp += ts;
	}
	ch->flags |= CH_NewAU;
}

void RP_ParsePayloadMPEG12Video(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	u8 pic_type;

	ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	ch->sl_hdr.decodingTimeStamp = hdr->TimeStamp;


	pic_type = payload[2] & 0x7;
	payload += 4;
	size -= 4;

	/*missed something*/
	if (ch->sl_hdr.compositionTimeStamp != hdr->TimeStamp) ch->flags |= CH_NewAU;

	ch->sl_hdr.accessUnitStartFlag = (ch->flags & CH_NewAU) ? 1 : 0;
	ch->sl_hdr.accessUnitEndFlag = hdr->Marker ? 1 : 0;
	ch->sl_hdr.randomAccessPointFlag = (pic_type==1) ? 1 : 0;

	if (ch->sl_hdr.accessUnitStartFlag) {
		ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		ch->sl_hdr.compositionTimeStampFlag = 1;
	} else {
		ch->sl_hdr.compositionTimeStampFlag = 0;
	}
	gf_term_on_sl_packet(ch->owner->service, ch->channel, payload, size, &ch->sl_hdr, GF_OK);
	if (hdr->Marker) {
		ch->flags |= CH_NewAU;
	} else {
		ch->flags &= ~CH_NewAU;
	}
}


void RP_ParsePayloadMPEG12(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	switch (ch->sl_map.StreamType) {
	case GF_STREAM_VISUAL:
		RP_ParsePayloadMPEG12Video(ch, hdr, payload, size);
		break;
	case GF_STREAM_AUDIO:
		RP_ParsePayloadMPEG12Audio(ch, hdr, payload, size);
		break;
	}
}

void RP_ParsePayloadAMR(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	unsigned char c, type;
	char *data;
	/*we support max 30 frames in one RTP packet...*/
	u32 nbFrame, i, frame_size;
	/*not supported yet*/
	if (!(ch->flags & CH_AMR_Align) ) return;

	/*process toc and locate start of payload data*/
	nbFrame = 0;
	while (1) {
		c = payload[nbFrame + 1];
		nbFrame++;
		if (!(c & 0x80)) break;
	}
	data = payload + nbFrame + 1;
	ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	/*then each frame*/
	for (i=0; i<nbFrame; i++) {
		c = payload[i + 1];
		type = ((c & 0x78) >> 3);
		if (ch->rtptype==GP_RTP_PAYT_AMR) {
			frame_size = GF_AMR_FRAME_SIZE[type];
		} else {
			frame_size = GF_AMR_WB_FRAME_SIZE[type];
		}

		ch->sl_hdr.compositionTimeStampFlag = 1;
		ch->sl_hdr.accessUnitStartFlag = 1;
		ch->sl_hdr.accessUnitEndFlag = 0;
		/*send TOC*/
		gf_term_on_sl_packet(ch->owner->service, ch->channel, &payload[i+1], 1, &ch->sl_hdr, GF_OK);
		ch->sl_hdr.packetSequenceNumber ++;
		ch->sl_hdr.compositionTimeStampFlag = 0;
		ch->sl_hdr.accessUnitStartFlag = 0;
		ch->sl_hdr.accessUnitEndFlag = 1;
		/*send payload*/
		gf_term_on_sl_packet(ch->owner->service, ch->channel, data, frame_size, &ch->sl_hdr, GF_OK);
		data += frame_size;
		ch->sl_hdr.compositionTimeStamp += 160;
	}
}


void RP_ParsePayloadH263(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	GF_BitStream *bs;
	Bool P_bit, V_bit;
	u32 plen, plen_bits, offset;
	char blank[2];

	bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	/*reserved*/
	gf_bs_read_int(bs, 5);
	P_bit = gf_bs_read_int(bs, 1);
	V_bit = gf_bs_read_int(bs, 1);
	plen = gf_bs_read_int(bs, 6);
	plen_bits = gf_bs_read_int(bs, 3);

	/*VRC not supported yet*/
	if (V_bit) {
		gf_bs_read_u8(bs);
	}
	/*extra picture header not supported yet*/
	if (plen) {
		gf_bs_skip_bytes(bs, plen);
	}
	offset = (u32) gf_bs_get_position(bs);
	gf_bs_del(bs);

	blank[0] = blank[1] = 0;
	/*start*/
	if (P_bit) {
		ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		ch->sl_hdr.compositionTimeStampFlag = 1;
		ch->sl_hdr.accessUnitStartFlag = 1;
		ch->sl_hdr.accessUnitEndFlag = 0;

		if (ch->sl_hdr.accessUnitStartFlag) {
			/*the first 16 bytes are NOT sent on the wire*/
			ch->sl_hdr.randomAccessPointFlag = (payload[offset+2]&0x02) ? 0 : 1;
		}
		/*send missing start code*/
		gf_term_on_sl_packet(ch->owner->service, ch->channel, (char *) blank, 2, &ch->sl_hdr, GF_OK);

		/*send payload*/
		ch->sl_hdr.compositionTimeStampFlag = 0;
		ch->sl_hdr.accessUnitStartFlag = 0;
		ch->sl_hdr.randomAccessPointFlag = 0;

		/*if M bit set, end of frame*/
		ch->sl_hdr.accessUnitEndFlag = hdr->Marker;
		gf_term_on_sl_packet(ch->owner->service, ch->channel, payload + offset, size - offset, &ch->sl_hdr, GF_OK);
	} else {
		/*middle/end of frames - if M bit set, end of frame*/
		ch->sl_hdr.accessUnitEndFlag = hdr->Marker;
		gf_term_on_sl_packet(ch->owner->service, ch->channel, payload + offset, size - offset, &ch->sl_hdr, GF_OK);
	}
}

void rtp_ttxt_flush(RTPStream *ch, u32 ts)
{
	GF_BitStream *bs;
	char *data;
	u32 data_size;
	if (!ch->inter_bs) return;

	ch->sl_hdr.compositionTimeStamp = ts;
	ch->sl_hdr.compositionTimeStampFlag = 1;
	ch->sl_hdr.accessUnitStartFlag = 1;
	ch->sl_hdr.accessUnitEndFlag = 0;
	ch->sl_hdr.randomAccessPointFlag = 1;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bs, ch->sl_hdr.idleFlag, 1);
	ch->sl_hdr.idleFlag = 0;
	gf_bs_write_int(bs, 0, 4);
	gf_bs_write_int(bs, 1, 3);
	gf_bs_write_u16(bs, 8 + (u16) gf_bs_get_position(ch->inter_bs));
	gf_bs_write_u8(bs, ch->sidx);
	gf_bs_write_u24(bs, ch->sl_hdr.au_duration);
	gf_bs_write_u16(bs, ch->txt_len);
	gf_bs_get_content(bs, (unsigned char **)&data, &data_size);
	gf_bs_del(bs);

	gf_term_on_sl_packet(ch->owner->service, ch->channel, data, data_size, &ch->sl_hdr, GF_OK);
	free(data);
	ch->sl_hdr.accessUnitStartFlag = 0;
	ch->sl_hdr.accessUnitEndFlag = 1;
	gf_bs_get_content(ch->inter_bs, (unsigned char **)&data, &data_size);
	gf_term_on_sl_packet(ch->owner->service, ch->channel, data, data_size, &ch->sl_hdr, GF_OK);
	free(data);

	gf_bs_del(ch->inter_bs);
	ch->inter_bs = NULL;
	ch->nb_txt_frag = ch->cur_txt_frag = ch->sidx = ch->txt_len = ch->nb_mod_frag = 0;
}

void RP_ParsePayloadText(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	Bool is_utf_16;
	u32 type, ttu_len, pay_start, duration, ts, sidx, txt_size;
	u32 nb_frag, cur_frag;
	GF_BitStream *bs;

	ts = hdr->TimeStamp;

	bs = gf_bs_new(payload, size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		pay_start = (u32) gf_bs_get_position(bs);
		is_utf_16 = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 4);
		type = gf_bs_read_int(bs, 3);
		ttu_len = gf_bs_read_u16(bs);
		if (ttu_len<2) break;

		if (type==1) {
			/*flush any existing packet*/
			rtp_ttxt_flush(ch, (u32) ch->sl_hdr.compositionTimeStamp);
			
			/*bad ttu(1)*/
			if (ttu_len<8) break;
			ch->sl_hdr.compositionTimeStamp = ts;
			ch->sl_hdr.compositionTimeStampFlag = 1;
			ch->sl_hdr.accessUnitStartFlag = 1;
			ch->sl_hdr.accessUnitEndFlag = 1;
			ch->sl_hdr.randomAccessPointFlag = 1;
			gf_bs_read_u8(bs);
			ch->sl_hdr.au_duration = gf_bs_read_u24(bs);
			gf_term_on_sl_packet(ch->owner->service, ch->channel, payload + pay_start, ttu_len + 1, &ch->sl_hdr, GF_OK);
			gf_bs_skip_bytes(bs, ttu_len - 6);
			ts += ch->sl_hdr.au_duration;
		}
		/*text segment*/
		else if (type==2) {
			/*TS changed, flush packet*/
			if (ch->sl_hdr.compositionTimeStamp < ts) {
				rtp_ttxt_flush(ch, (u32) ch->sl_hdr.compositionTimeStamp);
			}
			if (ttu_len<9) break;
			ch->sl_hdr.compositionTimeStamp = ts;
			ch->sl_hdr.idleFlag = is_utf_16;
			nb_frag = gf_bs_read_int(bs, 4);
			cur_frag = gf_bs_read_int(bs, 4);
			duration = gf_bs_read_u24(bs);
			sidx = gf_bs_read_u8(bs);
			gf_bs_read_u16(bs);/*complete text sample size, ignored*/
			txt_size = size - 10;
			
			/*init - 3GPP/MPEG-4 spliting is IMHO stupid: 
				- nb frag & cur frags are not needed: rtp reordering insures packet are in order, and 
			!!!we assume fragments are sent in order!!!
				- any other TTU suffices to indicate end of text string (modifiers or != RTP TS)
				- replacing these 8bits field with a 16 bit absolute character offset would add error recovery
			*/
			if (!ch->nb_txt_frag) {
				ch->nb_txt_frag = nb_frag;
				ch->cur_txt_frag = 0;
				ch->sidx = sidx;
			}
			/*flush prev if any mismatch*/
			if ((nb_frag != ch->nb_txt_frag) || (ch->cur_txt_frag>cur_frag)) {
				rtp_ttxt_flush(ch, (u32) ch->sl_hdr.compositionTimeStamp);
				ch->nb_txt_frag = nb_frag;
				ch->sidx = sidx;
			}
			if (!ch->inter_bs) ch->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

			/*we don't reorder - RTP reordering is done at lower level, if this is out of order too bad*/
			ch->cur_txt_frag = cur_frag;
			gf_bs_write_data(ch->inter_bs, payload+10, txt_size);
			gf_bs_skip_bytes(bs, txt_size);

			ch->sl_hdr.au_duration = duration;
			/*done*/
			if (hdr->Marker) {
				ch->txt_len = (u32) gf_bs_get_position(ch->inter_bs);
				rtp_ttxt_flush(ch, ts);
			}
		} else if ((type==3) || (type==4)) {
			if (!ch->inter_bs) ch->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			/*first modifier, store effective written text*/
			if (type==3) ch->txt_len = (u32) gf_bs_get_position(ch->inter_bs);
			if (ttu_len<6) break;

			nb_frag = gf_bs_read_int(bs, 4);
			if (!ch->nb_mod_frag) ch->nb_mod_frag = nb_frag;
			else if (ch->nb_mod_frag != nb_frag) {
				rtp_ttxt_flush(ch, (u32) ch->sl_hdr.compositionTimeStamp);
				ch->nb_mod_frag = nb_frag;
			}
			gf_bs_read_int(bs, 4);  /*cur_frag, ignore*/
			ch->sl_hdr.au_duration = gf_bs_read_u24(bs);
			gf_bs_write_data(ch->inter_bs, payload+7, ttu_len-6);
			gf_bs_skip_bytes(bs, ttu_len-6);

			/*done*/
			if (hdr->Marker) rtp_ttxt_flush(ch, ts);
		}
	}
	gf_bs_del(bs);
}

static void rtp_avc_flush(RTPStream*ch, GF_RTPHeader *hdr, Bool missed_end)
{
	char *data;
	u32 data_size, nal_s;
	if (!ch->inter_bs) return;

	data = NULL;
	data_size = 0;
	gf_bs_get_content(ch->inter_bs, (unsigned char **) &data, &data_size);
	gf_bs_del(ch->inter_bs);
	ch->inter_bs = NULL;
	nal_s = data_size-4;
	data[0] = nal_s>>24; data[1] = nal_s>>16; data[2] = nal_s>>8; data[3] = nal_s&0xFF;
	/*set F-bit since nal is corrupted*/
	if (missed_end) data[4] |= 0x80;

	ch->sl_hdr.accessUnitEndFlag = hdr->Marker;
	ch->sl_hdr.compositionTimeStampFlag = 1;
	ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	ch->sl_hdr.decodingTimeStamp = hdr->TimeStamp;
	ch->sl_hdr.decodingTimeStampFlag = 1;
	gf_term_on_sl_packet(ch->owner->service, ch->channel, data, data_size, &ch->sl_hdr, GF_OK);
	ch->sl_hdr.accessUnitStartFlag = 0;
	ch->sl_hdr.randomAccessPointFlag = 0;
	free(data);
}

void RP_ParsePayloadH264(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	char nalhdr[4];
	u32 nal_type;
	if (ch->packetization_mode==2) return;

	nal_type = payload[0] & 0x1F;

	/*set start*/
	if (ch->sl_hdr.compositionTimeStamp != hdr->TimeStamp) {
		ch->sl_hdr.accessUnitEndFlag = 0;
		ch->sl_hdr.accessUnitStartFlag = 1;
		ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
		ch->sl_hdr.compositionTimeStampFlag = 1;
		ch->sl_hdr.decodingTimeStamp = hdr->TimeStamp;
		ch->sl_hdr.decodingTimeStampFlag = 1;
		ch->sl_hdr.randomAccessPointFlag = 0;
	}

	/*single NALU*/
	if (nal_type<23) {
		if (nal_type==5) {
			ch->sl_hdr.randomAccessPointFlag = 1;
			ch->flags &= ~CH_AVC_WaitRAP;
		}
		else if (ch->flags & CH_AVC_WaitRAP) return;

		ch->sl_hdr.accessUnitEndFlag = 0;
		/*signal NALU size on 4 bytes*/
		nalhdr[0] = size>>24; nalhdr[1] = size>>16; nalhdr[2] = size>>8; nalhdr[3] = size&0xFF;
		gf_term_on_sl_packet(ch->owner->service, ch->channel, nalhdr, 4, &ch->sl_hdr, GF_OK);
		ch->sl_hdr.accessUnitStartFlag = 0;
		ch->sl_hdr.compositionTimeStampFlag = 0;
		ch->sl_hdr.accessUnitEndFlag = hdr->Marker;
		/*send NAL payload*/
		gf_term_on_sl_packet(ch->owner->service, ch->channel, payload, size, &ch->sl_hdr, GF_OK);
	}
	/*STAP-A NALU*/
	else if (nal_type==24) {
		u32 offset = 1;
		while (offset<size) {
			Bool send = 1;
			u32 nal_size = (u8) payload[offset]; nal_size<<=8; nal_size |= (u8) payload[offset+1]; 
			offset += 2;
			if ((payload[offset] & 0x1F) == 5) {
				ch->sl_hdr.randomAccessPointFlag = 1;
				ch->flags &= ~CH_AVC_WaitRAP;
			}
			if (ch->flags & CH_AVC_WaitRAP) send = 0;

			/*signal NALU size on 4 bytes*/
			nalhdr[0] = nal_size>>24; nalhdr[1] = nal_size>>16; nalhdr[2] = nal_size>>8; nalhdr[3] = nal_size&0xFF;
			if (send) {
				gf_term_on_sl_packet(ch->owner->service, ch->channel, nalhdr, 4, &ch->sl_hdr, GF_OK);
				ch->sl_hdr.accessUnitStartFlag = 0;
				ch->sl_hdr.compositionTimeStampFlag = 0;
			}
			ch->sl_hdr.accessUnitEndFlag = (hdr->Marker && (offset+nal_size==size)) ? 1 : 0;
			if (send) gf_term_on_sl_packet(ch->owner->service, ch->channel, payload+offset, nal_size, &ch->sl_hdr, GF_OK);
			offset += nal_size;
		}
	}
	/*FU-A NALU*/
	else if (nal_type==28) {
		Bool is_start = payload[1] & 0x80;
		Bool is_end = payload[1] & 0x40;
		/*flush*/
		if (is_start) rtp_avc_flush(ch, hdr, 1);

		if ((payload[1] & 0x1F) == 5) {
			ch->flags &= ~CH_AVC_WaitRAP;
			ch->sl_hdr.randomAccessPointFlag = 1;
		} else if (ch->flags & CH_AVC_WaitRAP) return;

		/*setup*/
		if (!ch->inter_bs) {
			u8 nal_hdr;
			ch->inter_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			/*copy F and NRI*/
			nal_hdr = payload[0] & 0xE0;
			/*start bit not set, signal corrupted data (we missed start packet)*/
			if (!is_start) nal_hdr |= 0x80;
			/*copy NALU type*/
			nal_hdr |= (payload[1] & 0x1F);
			/*dummy size field*/
			gf_bs_write_u32(ch->inter_bs, 0);
			gf_bs_write_u8(ch->inter_bs, nal_hdr);
		}
		gf_bs_write_data(ch->inter_bs, payload+2, size-2);
		if (is_end || hdr->Marker) rtp_avc_flush(ch, hdr, 0);
	}
}


void RP_ParsePayloadLATM(RTPStream *ch, GF_RTPHeader *hdr, char *payload, u32 size)
{
	u32 remain, latm_hdr_size, latm_size;

	ch->sl_hdr.compositionTimeStamp = hdr->TimeStamp;
	ch->sl_hdr.compositionTimeStampFlag = 1;
	ch->sl_hdr.accessUnitStartFlag = ch->sl_hdr.accessUnitEndFlag = 1;
	ch->sl_hdr.randomAccessPointFlag = 1;

	remain = size;
	while (remain) {
		latm_hdr_size = latm_size = 0;
		while (1) {
			u8 c = *payload;
			latm_hdr_size += 1;
			latm_size += c;
			payload ++;
			if (c < 0xFF) break;
		}

		gf_term_on_sl_packet(ch->owner->service, ch->channel, (char *) payload, latm_size, &ch->sl_hdr, GF_OK);
		payload += latm_size;
		remain -= (latm_size+latm_hdr_size);
		ch->sl_hdr.compositionTimeStamp += ch->unit_duration;
	}
}
