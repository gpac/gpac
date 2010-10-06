/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) ENST 2000-200X
 *					All rights reserved
 *
 *  This file is part of GPAC / mp42ts application
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
#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/base_coding.h>
#include <gpac/ietf.h>
#include <gpac/scene_engine.h>
#include <gpac/mpegts.h>

void usage() 
{
	fprintf(stderr, "usage: mp42ts [options] dst\n"
					"With options being: \n"
					"-prog=FILE     specifies an input file used for a TS service\n"
					"                * currently only supports ISO files and SDP files\n"
					"                * option can be used several times, once for each program\n"
					"-rate=R        specifies target rate in kbits/sec of the multiplex\n"
					"                If not set, transport stream will be of variable bitrate\n"
					"-mpeg4			forces usage of MPEG-4 signaling (use of IOD and SL Config)\n"
					"\n"
					"dst can be a file, an RTP or a UDP destination (unicast/multicast)\n"
		);
}




#define MAX_MUX_SRC_PROG	100
typedef struct
{
	GF_ISOFile *mp4;
	u32 nb_streams, pcr_idx;
	GF_ESInterface streams[40];
	GF_Descriptor *iod;
	GF_SceneEngine *seng;
	GF_Thread *th;
	char *src_name;
	u32 rate;
	Bool repeat;
} M2TSProgram;

typedef struct
{
	GF_ISOFile *mp4;
	u32 track, sample_number, sample_count;
	GF_ISOSample *sample;
	/*refresh rate for images*/
	u32 image_repeat_ms, nb_repeat_last;
	void *dsi;
	u32 dsi_size;
	u32 nalu_size;
	void *dsi_and_rap;
	Bool loop;
	u64 ts_offset;
} GF_ESIMP4;

typedef struct
{
	u32 carousel_period, ts_delta;
	u16 aggregate_on_stream;
	Bool adjust_carousel_time;
	Bool discard;
	Bool rap;
	Bool critical;
	Bool vers_inc;
} GF_ESIStream;

static GF_Err mp4_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	GF_ESIMP4 *priv = (GF_ESIMP4 *)ifce->input_udta;
	if (!priv) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
	{
		GF_ESIPacket pck;
		if (!priv->sample) 
			priv->sample = gf_isom_get_sample(priv->mp4, priv->track, priv->sample_number+1, NULL);

		if (!priv->sample) return GF_IO_ERR;

		pck.flags = 0;
		pck.flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_HAS_CTS;
		if (priv->sample->IsRAP) pck.flags |= GF_ESI_DATA_AU_RAP;
		pck.cts = priv->sample->DTS + priv->ts_offset;

		if (priv->nb_repeat_last) {
			pck.cts += priv->nb_repeat_last*ifce->timescale * priv->image_repeat_ms / 1000;
		}

		if (priv->sample->CTS_Offset) {
			pck.dts = pck.cts;
			pck.cts += priv->sample->CTS_Offset;
			pck.flags |= GF_ESI_DATA_HAS_DTS;
		}

		if (priv->sample->IsRAP && priv->dsi) {
			pck.data = priv->dsi;
			pck.data_len = priv->dsi_size;
			ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
			pck.flags = 0;
		}
		if (priv->nalu_size) {
			u32 remain = priv->sample->dataLength;
			char *ptr = priv->sample->data;
			u32 v, size;
			char sc[4];
			sc[0] = sc[1] = sc[2] = 0; sc[3] = 1;

			while (remain) {
				size = 0;
				v = priv->nalu_size;
				while (v) {
					size |= (u8) *ptr;
					ptr++;
					remain--;
					v-=1;
					if (v) size<<=8;
				}
				remain -= size;

				pck.data = sc;
				pck.data_len = 4;
				ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
				pck.flags &= ~GF_ESI_DATA_AU_START;

				if (!remain) pck.flags |= GF_ESI_DATA_AU_END;

				pck.data = ptr;
				pck.data_len = size;
				ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
				ptr += size;
			}

		} else {
			pck.flags |= GF_ESI_DATA_AU_END;
			pck.data = priv->sample->data;
			pck.data_len = priv->sample->dataLength;
			ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
		}

		gf_isom_sample_del(&priv->sample);
		priv->sample_number++;
		if (priv->sample_number==priv->sample_count) {
			if (priv->loop) {
				Double scale;
				u64 duration;
				/*increment ts offset*/
				scale = gf_isom_get_media_timescale(priv->mp4, priv->track);
				scale /= gf_isom_get_timescale(priv->mp4);
				duration = (u64) (gf_isom_get_duration(priv->mp4) * scale);
				priv->ts_offset += duration;
				priv->sample_number = 0;
			}
			else if (priv->image_repeat_ms) {
				priv->nb_repeat_last++;
				priv->sample_number--;
			} else {
				ifce->caps |= GF_ESI_STREAM_IS_OVER;
			}
		}
	}
		return GF_OK;

	case GF_ESI_INPUT_DESTROY:
		if (priv->dsi) gf_free(priv->dsi);
		if (ifce->decoder_config) {
			gf_free(ifce->decoder_config);
			ifce->decoder_config = NULL;
		}
		gf_free(priv);
		ifce->input_udta = NULL;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static void fill_isom_es_ifce(GF_ESInterface *ifce, GF_ISOFile *mp4, u32 track_num)
{
	GF_ESIMP4 *priv;
	char _lan[4];
	GF_DecoderConfig *dcd;
	u64 avg_rate;

	GF_SAFEALLOC(priv, GF_ESIMP4);

	priv->mp4 = mp4;
	priv->track = track_num;
	priv->loop = 1;
	priv->sample_count = gf_isom_get_sample_count(mp4, track_num);

	memset(ifce, 0, sizeof(GF_ESInterface));
	ifce->stream_id = gf_isom_get_track_id(mp4, track_num);
	dcd = gf_isom_get_decoder_config(mp4, track_num, 1);
	ifce->stream_type = dcd->streamType;
	ifce->object_type_indication = dcd->objectTypeIndication;
	if (dcd->decoderSpecificInfo && dcd->decoderSpecificInfo->dataLength) {
		switch (dcd->objectTypeIndication) {
		case GPAC_OTI_AUDIO_AAC_MPEG4:
			ifce->decoder_config = gf_malloc(sizeof(char)*dcd->decoderSpecificInfo->dataLength);
			ifce->decoder_config_size = dcd->decoderSpecificInfo->dataLength;
			memcpy(ifce->decoder_config, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			break;
		case GPAC_OTI_VIDEO_MPEG4_PART2:
			priv->dsi = gf_malloc(sizeof(char)*dcd->decoderSpecificInfo->dataLength);
			priv->dsi_size = dcd->decoderSpecificInfo->dataLength;
			memcpy(priv->dsi, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			break;
		case GPAC_OTI_VIDEO_AVC:
		{
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_AVCConfigSlot *slc;
			u32 i;
			GF_BitStream *bs;
			GF_AVCConfig *avccfg = gf_isom_avc_config_get(mp4, track_num, 1);
			priv->nalu_size = avccfg->nal_unit_size;
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			for (i=0; i<gf_list_count(avccfg->sequenceParameterSets);i++) {
				slc = gf_list_get(avccfg->sequenceParameterSets, i);
				gf_bs_write_u32(bs, 1);
				gf_bs_write_data(bs, slc->data, slc->size);
			}
			for (i=0; i<gf_list_count(avccfg->pictureParameterSets);i++) {
				slc = gf_list_get(avccfg->pictureParameterSets, i);
				gf_bs_write_u32(bs, 1);
				gf_bs_write_data(bs, slc->data, slc->size);
			}
			gf_bs_get_content(bs, (char **) &priv->dsi, &priv->dsi_size);
			gf_bs_del(bs);
#endif
		}
			break;
		}
	}
	gf_odf_desc_del((GF_Descriptor *)dcd);
	gf_isom_get_media_language(mp4, track_num, _lan);
	ifce->lang = GF_4CC(_lan[0],_lan[1],_lan[2],' ');

	ifce->timescale = gf_isom_get_media_timescale(mp4, track_num);
	ifce->duration = gf_isom_get_media_timescale(mp4, track_num);
	avg_rate = gf_isom_get_media_data_size(mp4, track_num);
	avg_rate *= ifce->timescale * 8;
	avg_rate /= gf_isom_get_media_duration(mp4, track_num);

	if (gf_isom_has_time_offset(mp4, track_num)) ifce->caps |= GF_ESI_SIGNAL_DTS;

	ifce->bit_rate = (u32) avg_rate;
	ifce->duration = (Double) (s64) gf_isom_get_media_duration(mp4, track_num);
	ifce->duration /= ifce->timescale;

	ifce->input_ctrl = mp4_input_ctrl;
	ifce->input_udta = priv;
}


static GF_Err seng_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	if (act_type==GF_ESI_INPUT_DESTROY) {
		//free my data
		ifce->input_udta = NULL;
		return GF_OK;
	}

	return GF_OK;
}

typedef struct
{
	/*RTP channel*/
	GF_RTPChannel *rtp_ch;

	/*depacketizer*/
	GF_RTPDepacketizer *depacketizer;

	GF_ESIPacket pck;

	GF_ESInterface *ifce;

	Bool cat_dsi;
	void *dsi_and_rap;

	Bool use_carousel;
	u32 au_sn;
} GF_ESIRTP;

static GF_Err rtp_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	u32 size, PayloadStart;
	GF_Err e;
	GF_RTPHeader hdr;
	char buffer[8000];
	GF_ESIRTP *rtp = (GF_ESIRTP*)ifce->input_udta;

	if (!ifce->input_udta) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
		/*flush rtp channel*/
		while (1) {
			size = gf_rtp_read_rtp(rtp->rtp_ch, buffer, 8000);
			if (!size) break;
			e = gf_rtp_decode_rtp(rtp->rtp_ch, buffer, size, &hdr, &PayloadStart);
			if (e) return e;
			gf_rtp_depacketizer_process(rtp->depacketizer, &hdr, buffer + PayloadStart, size - PayloadStart);
		}
		/*flush rtcp channel*/
		while (1) {
			size = gf_rtp_read_rtcp(rtp->rtp_ch, buffer, 8000);
			if (!size) break;
			e = gf_rtp_decode_rtcp(rtp->rtp_ch, buffer, size, NULL);
			if (e == GF_EOS) ifce->caps |= GF_ESI_STREAM_IS_OVER;
		}
		return GF_OK;
	case GF_ESI_INPUT_DESTROY:
		gf_rtp_depacketizer_del(rtp->depacketizer);
		if (rtp->dsi_and_rap) gf_free(rtp->dsi_and_rap);
		gf_rtp_del(rtp->rtp_ch);
		gf_free(rtp);
		ifce->input_udta = NULL;
		return GF_OK;
	}
	return GF_OK;
}

GF_Err SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{		
	u32 i=0;

	if (calling_object) {
		M2TSProgram *prog = (M2TSProgram *)calling_object;
			while (i<prog->nb_streams){
				if (prog->streams[i].output_ctrl==NULL) {
					fprintf(stdout, "MULTIPLEX NOT YET CREATED\n");				
					return GF_OK;
				}
				if (prog->streams[i].stream_id == ESID) {
					GF_ESIStream *priv = (GF_ESIStream *)prog->streams[i].input_udta;
					GF_ESIPacket pck;
					memset(&pck, 0, sizeof(GF_ESIPacket));
					pck.data = data;
					pck.data_len = size;
					pck.flags |= GF_ESI_DATA_HAS_CTS;
					pck.flags |= GF_ESI_DATA_AU_START;
					pck.flags |= GF_ESI_DATA_AU_END;
					if (priv->rap)
						pck.flags |= GF_ESI_DATA_AU_RAP;
					if (prog->repeat || !priv->vers_inc) {
						pck.flags |= GF_ESI_DATA_REPEAT;
						fprintf(stdout, "RAP carousel from scene engine sent\n"); 
					} else {
						fprintf(stdout, "Update from scene engine sent\n"); 
					}
					prog->streams[i].output_ctrl(&prog->streams[i], GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
					return GF_OK;
				}
			i++;
			}
	} 
	return GF_OK;
}

static Bool run = 1;

static void set_broadcast_params(M2TSProgram *prog, u16 esid, u32 period, u32 ts_delta, u16 aggregate_on_stream, Bool adjust_carousel_time, Bool force_rap, Bool aggregate_au, Bool discard_pending, Bool signal_rap, Bool signal_critical, Bool version_inc)
{
	u32 i=0;
	GF_ESIStream *priv=NULL;
	GF_ESInterface *esi=NULL;

	/*locate our stream*/
	if (esid) {
		while (i<prog->nb_streams) {
			if (prog->streams[i].stream_id == esid){
				priv = (GF_ESIStream *)prog->streams[i].input_udta; 
				esi = &prog->streams[i]; 
				break;
			}
			else{
				i++;
			}
		}
	}

	/*TODO - set/reset the ESID for the parsers*/
	if (!priv) return NULL;

	/*TODO - if discard is set, abort current carousel*/
	if (discard_pending) {
	}

	/*remember RAP flag*/
	priv->rap = signal_rap; 
	priv->critical = signal_critical;
	priv->vers_inc = version_inc;

	priv->ts_delta = ts_delta;
	priv->adjust_carousel_time = adjust_carousel_time;

	/*change stream aggregation mode*/
	if ((aggregate_on_stream != (u16)-1) && (priv->aggregate_on_stream != aggregate_on_stream)) {
		gf_seng_enable_aggregation(prog->seng, esid, aggregate_on_stream);
		priv->aggregate_on_stream = aggregate_on_stream;
	}
	/*change stream aggregation mode*/
	if (priv->aggregate_on_stream==esi->stream_id) {
		if (priv->aggregate_on_stream && (period!=(u32)-1) && (esi->repeat_rate != period)) {
			esi->repeat_rate = period;
		}
	} else {
		esi->repeat_rate = 0;
	}

	return priv;
}

u32 seng_output(void *param)
{
	GF_Err e;
	u64 last_src_modif, mod_time;
	Bool has_carousel=0;
	M2TSProgram *prog = (M2TSProgram *)param;
	GF_SceneEngine *seng = prog->seng;
	Bool update_context=0;
	u32 i=0;
	Bool force_rap, adjust_carousel_time, discard_pending, signal_rap, signal_critical, version_inc, aggregate_au;
	u32 period, ts_delta;
	u16 es_id, aggregate_on_stream;

	if (prog->rate){
		has_carousel = 1;
	}
	gf_sleep(2000);
	gf_seng_encode_context(seng, SampleCallBack);
	
	last_src_modif = prog->src_name ? gf_file_modification_time(prog->src_name) : 0;

	while (run) {
		if (gf_prompt_has_input()) {
			char c = gf_prompt_get_char();
			switch (c) {
			case 'u':
			{
				GF_Err e;
				char szCom[8192];
				fprintf(stdout, "Enter command to send:\n");
				fflush(stdin);
				szCom[0] = 0;
				scanf("%[^\t\n]", szCom);
				prog->repeat = 0;
				e = gf_seng_encode_from_string(seng, 0, 0, szCom, SampleCallBack);
				prog->repeat = 1;
				if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
				update_context=1;
			}
				break;
			case 'p':
			{
				char rad[GF_MAX_PATH];
				fprintf(stdout, "Enter output file name - \"std\" for stdout: ");
				scanf("%s", rad);
				e = gf_seng_save_context(seng, !strcmp(rad, "std") ? NULL : rad);
				fprintf(stdout, "Dump done (%s)\n", gf_error_to_string(e));
			}
				break;
			}
			e = GF_OK;
		}
		if (prog->src_name) {
			mod_time = gf_file_modification_time(prog->src_name);
			if (mod_time != last_src_modif) {
				FILE *srcf;
				char flag_buf[201], *flag;
				fprintf(stdout, "Update file modified - processing\n");
				last_src_modif = mod_time;

				srcf = fopen(prog->src_name, "rt");
				if (!srcf) continue;

				/*checks if we have a broadcast config*/
				fgets(flag_buf, 200, srcf);
				fclose(srcf);

				aggregate_au = force_rap = adjust_carousel_time = discard_pending = signal_rap = signal_critical = 0;
				version_inc = 1;
				period = -1;
				aggregate_on_stream = -1;
				ts_delta = 0;
				es_id = 0;

				/*find our keyword*/
				flag = strstr(flag_buf, "gpac_broadcast_config ");
				if (flag) {
					flag += strlen("gpac_broadcast_config ");
					/*move to next word*/
					while (flag && (flag[0]==' ')) flag++;

					while (1) {
						char *sep = strchr(flag, ' ');
						if (sep) sep[0] = 0;
						if (!strnicmp(flag, "esid=", 5)) es_id = atoi(flag+5);
						else if (!strnicmp(flag, "period=", 7)) period = atoi(flag+7);
						else if (!strnicmp(flag, "ts=", 3)) ts_delta = atoi(flag+3);
						else if (!strnicmp(flag, "carousel=", 9)) aggregate_on_stream = atoi(flag+9);
						else if (!strnicmp(flag, "restamp=", 8)) adjust_carousel_time = atoi(flag+8);
						else if (!strnicmp(flag, "discard=", 8)) discard_pending = atoi(flag+8);
						else if (!strnicmp(flag, "aggregate=", 10)) aggregate_au = atoi(flag+10);
						else if (!strnicmp(flag, "force_rap=", 10)) force_rap = atoi(flag+10);
						else if (!strnicmp(flag, "rap=", 4)) signal_rap = atoi(flag+4);
						else if (!strnicmp(flag, "critical=", 9)) signal_critical = atoi(flag+9);
						else if (!strnicmp(flag, "vers_inc=", 9)) version_inc = atoi(flag+9);
						if (sep) {
							sep[0] = ' ';
							flag = sep+1;
						} else {
							break;
						}
					}

					set_broadcast_params(prog, es_id, period, ts_delta, aggregate_on_stream, adjust_carousel_time, force_rap, aggregate_au, discard_pending, signal_rap, signal_critical, version_inc);
				}


				e = gf_seng_encode_from_file(seng, es_id, aggregate_au ? 0 : 1, prog->src_name, SampleCallBack);
				if (e) fprintf(stdout, "Processing command failed: %s\n", gf_error_to_string(e));
				else gf_seng_aggregate_context(seng, 0);
				update_context=1;

				

			}
		}
		if (update_context) {
			prog->repeat = 1;
			e = gf_seng_encode_context(seng, SampleCallBack	);
			prog->repeat = 0;
		}		

		gf_sleep(10);
	}
	
	
	return e ? 1 : 0;
}


static void rtp_sl_packet_cbk(void *udta, char *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	GF_ESIRTP *rtp = (GF_ESIRTP*)udta;
	rtp->pck.data = payload;
	rtp->pck.data_len = size;
	rtp->pck.dts = hdr->decodingTimeStamp;
	rtp->pck.cts = hdr->compositionTimeStamp;
	rtp->pck.flags = 0;
	if (hdr->compositionTimeStampFlag) rtp->pck.flags |= GF_ESI_DATA_HAS_CTS;
	if (hdr->decodingTimeStampFlag) rtp->pck.flags |= GF_ESI_DATA_HAS_DTS;
	if (hdr->randomAccessPointFlag) rtp->pck.flags |= GF_ESI_DATA_AU_RAP;
	if (hdr->accessUnitStartFlag) rtp->pck.flags |= GF_ESI_DATA_AU_START;
	if (hdr->accessUnitEndFlag) rtp->pck.flags |= GF_ESI_DATA_AU_END;

	if (rtp->use_carousel) {
		if ((hdr->AU_sequenceNumber==rtp->au_sn) && hdr->randomAccessPointFlag) rtp->pck.flags |= GF_ESI_DATA_REPEAT;
		rtp->au_sn = hdr->AU_sequenceNumber;
	}

	if (rtp->cat_dsi && hdr->randomAccessPointFlag && hdr->accessUnitStartFlag) {
		if (rtp->dsi_and_rap) gf_free(rtp->dsi_and_rap);
		rtp->pck.data_len = size + rtp->depacketizer->sl_map.configSize;
		rtp->dsi_and_rap = gf_malloc(sizeof(char)*(rtp->pck.data_len));
		memcpy(rtp->dsi_and_rap, rtp->depacketizer->sl_map.config, rtp->depacketizer->sl_map.configSize);
		memcpy((char *) rtp->dsi_and_rap + rtp->depacketizer->sl_map.configSize, payload, size);
		rtp->pck.data = rtp->dsi_and_rap;
	}


	rtp->ifce->output_ctrl(rtp->ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &rtp->pck);
}

void fill_rtp_es_ifce(GF_ESInterface *ifce, GF_SDPMedia *media, GF_SDPInfo *sdp)
{
	u32 i;
	GF_Err e;
	GF_X_Attribute*att;
	GF_ESIRTP *rtp;
	GF_RTPMap*map;
	GF_SDPConnection *conn;
	GF_RTSPTransport trans;

	/*check connection*/
	conn = sdp->c_connection;
	if (!conn) conn = (GF_SDPConnection*)gf_list_get(media->Connections, 0);

	/*check payload type*/
	map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);
	GF_SAFEALLOC(rtp, GF_ESIRTP);

	memset(ifce, 0, sizeof(GF_ESInterface));
	rtp->rtp_ch = gf_rtp_new();
	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(media->Attributes, &i))) {
		if (!stricmp(att->Name, "mpeg4-esid") && att->Value) ifce->stream_id = atoi(att->Value);
	}

	memset(&trans, 0, sizeof(GF_RTSPTransport));
	trans.Profile = media->Profile;
	trans.source = conn ? conn->host : sdp->o_address;
	trans.IsUnicast = gf_sk_is_multicast_address(trans.source) ? 0 : 1;
	if (!trans.IsUnicast) {
		trans.port_first = media->PortNumber;
		trans.port_last = media->PortNumber + 1;
		trans.TTL = conn ? conn->TTL : 0;
	} else {
		trans.client_port_first = media->PortNumber;
		trans.client_port_last = media->PortNumber + 1;
	}

	if (gf_rtp_setup_transport(rtp->rtp_ch, &trans, NULL) != GF_OK) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stdout, "Cannot initialize RTP transport\n");
		return;
	}
	/*setup depacketizer*/
	rtp->depacketizer = gf_rtp_depacketizer_new(media, rtp_sl_packet_cbk, rtp);
	if (!rtp->depacketizer) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stdout, "Cannot create RTP depacketizer\n");
		return;
	}
	/*setup channel*/
	gf_rtp_setup_payload(rtp->rtp_ch, map);
	ifce->input_udta = rtp;
	ifce->input_ctrl = rtp_input_ctrl;
	rtp->ifce = ifce;

	ifce->object_type_indication = rtp->depacketizer->sl_map.ObjectTypeIndication;
	ifce->stream_type = rtp->depacketizer->sl_map.StreamType;
	ifce->timescale = gf_rtp_get_clockrate(rtp->rtp_ch);
	if (rtp->depacketizer->sl_map.config) {
		switch (ifce->object_type_indication) {
		case GPAC_OTI_VIDEO_MPEG4_PART2:
			rtp->cat_dsi = 1;
			break;
		}
	}
	if (rtp->depacketizer->sl_map.StreamStateIndication) {
		rtp->use_carousel = 1;
		rtp->au_sn=0;
	}

	/*DTS signaling is only supported for MPEG-4 visual*/
	if (rtp->depacketizer->sl_map.DTSDeltaLength) ifce->caps |= GF_ESI_SIGNAL_DTS;

	gf_rtp_depacketizer_reset(rtp->depacketizer, 1);
	e = gf_rtp_initialize(rtp->rtp_ch, 0x100000ul, 0, 0, 10, 200, NULL);
	if (e!=GF_OK) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stdout, "Cannot initialize RTP channel: %s\n", gf_error_to_string(e));
		return;
	}
	fprintf(stdout, "RTP interface initialized\n");
}

void fill_seng_es_ifce(GF_ESInterface *ifce, u32 i, GF_SceneEngine *seng, u32 period)
{
	GF_Err e=GF_OK;
	char *config_buffer;
	u32 len;
	GF_ESIStream *stream;
						
	memset(ifce, 0, sizeof(GF_ESInterface));
	gf_seng_get_stream_config(seng, i, &ifce->stream_id, &config_buffer, &len, &ifce->stream_type, &ifce->object_type_indication, &ifce->timescale); 

	ifce->repeat_rate = period;
	GF_SAFEALLOC(stream, GF_ESIStream);
	stream->rap = 1;
	ifce->input_udta = stream;
	
//	e = gf_seng_set_carousel_time(seng, ifce->stream_id, period);
	fprintf(stdout, "Caroussel %d", &period);
	if (e) {
		fprintf(stdout, "Cannot set carousel time on stream %d to %d: %s\n", ifce->stream_id, period, gf_error_to_string(e));
	}
	ifce->input_ctrl = seng_input_ctrl;

}

Bool open_program(M2TSProgram *prog, const char *src, u32 carousel_rate, Bool *force_mpeg4, const char *update)
{
	GF_SDPInfo *sdp;
	u32 i;
	GF_Err e;
	
	memset(prog, 0, sizeof(M2TSProgram));

	/* Open ISO file  */
	if (gf_isom_probe_file(src)) {
		u32 nb_tracks;
		u32 first_audio = 0;
		prog->mp4 = gf_isom_open(src, GF_ISOM_OPEN_READ, 0);
		prog->nb_streams = 0;
		/*on MPEG-2 TS, carry 3GPP timed text as MPEG-4 Part17*/
		gf_isom_text_set_streaming_mode(prog->mp4, 1);
		nb_tracks = gf_isom_get_track_count(prog->mp4); 
		for (i=0; i<nb_tracks; i++) {
			if (gf_isom_get_media_type(prog->mp4, i+1) == GF_ISOM_MEDIA_HINT) 
				continue; 
			fill_isom_es_ifce(&prog->streams[i], prog->mp4, i+1);
			switch(prog->streams[i].stream_type) {
			case GF_STREAM_OD:
			case GF_STREAM_SCENE:
				*force_mpeg4 = 1;
				prog->streams[i].repeat_rate = carousel_rate;
				break;
			case GF_STREAM_VISUAL:
				/*turn on image repeat*/
				switch (prog->streams[i].object_type_indication) {
				case GPAC_OTI_IMAGE_JPEG:
				case GPAC_OTI_IMAGE_PNG:
					((GF_ESIMP4 *)prog->streams[i].input_udta)->image_repeat_ms = carousel_rate;
					break;
				}
				break;
			}
			prog->nb_streams++;
			/*get first visual stream as PCR*/
			if (!prog->pcr_idx && 
				(gf_isom_get_media_type(prog->mp4, i+1) == GF_ISOM_MEDIA_VISUAL) && 
				(gf_isom_get_sample_count(prog->mp4, i+1)>1) ) {
				prog->pcr_idx = i+1;
			}
			if (!first_audio && (gf_isom_get_media_type(prog->mp4, i+1) == GF_ISOM_MEDIA_AUDIO) ) {
				first_audio = i+1;
			}
		}
		/*if no visual PCR found, use first audio*/
		if (!prog->pcr_idx) prog->pcr_idx = first_audio;
		if (prog->pcr_idx) {
			GF_ESIMP4 *priv;
			prog->pcr_idx-=1;
			priv = prog->streams[prog->pcr_idx].input_udta;
			gf_isom_set_default_sync_track(prog->mp4, priv->track);
		}
		prog->iod = gf_isom_get_root_od(prog->mp4);
		return 1;
	}

	/*open SDP file*/
	if (strstr(src, ".sdp")) {
		GF_X_Attribute *att;
		char *sdp_buf;
		u32 sdp_size;
		FILE *_sdp = fopen(src, "rt");
		if (!_sdp) {
			fprintf(stderr, "Error opening %s - no such file\n", src);
			return 0;
		}
		fseek(_sdp, 0, SEEK_END);
		sdp_size = ftell(_sdp);
		fseek(_sdp, 0, SEEK_SET);
		sdp_buf = (char*)gf_malloc(sizeof(char)*sdp_size);
		memset(sdp_buf, 0, sizeof(char)*sdp_size);
		fread(sdp_buf, sdp_size, 1, _sdp);
		fclose(_sdp);

		sdp = gf_sdp_info_new();
		e = gf_sdp_info_parse(sdp, sdp_buf, sdp_size);
		gf_free(sdp_buf);
		if (e) {
			fprintf(stderr, "Error opening %s : %s\n", src, gf_error_to_string(e));
			gf_sdp_info_del(sdp);
			return 0;
		}

		i=0;
		while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
			char buf[2000];
			u32 size;
			char *buf64;
			u32 size64;
			char *iod_str;
			if (strcmp(att->Name, "mpeg4-iod") ) continue;
			iod_str = att->Value + 1;
			if (strnicmp(iod_str, "data:application/mpeg4-iod;base64", strlen("data:application/mpeg4-iod;base64"))) continue;

			buf64 = strstr(iod_str, ",");
			if (!buf64) break;
			buf64 += 1;
			size64 = strlen(buf64) - 1;
			size = gf_base64_decode(buf64, size64, buf, 2000);

			gf_odf_desc_read(buf, size, &prog->iod);
			break;
		}

		prog->nb_streams = gf_list_count(sdp->media_desc);
		for (i=0; i<prog->nb_streams; i++) {
			GF_SDPMedia *media = gf_list_get(sdp->media_desc, i);
			fill_rtp_es_ifce(&prog->streams[i], media, sdp);
			switch(prog->streams[i].stream_type) {
			case GF_STREAM_OD:
			case GF_STREAM_SCENE:
				*force_mpeg4 = 1;
				prog->streams[i].repeat_rate = carousel_rate;
				break;
			}
			if (!prog->pcr_idx && (prog->streams[i].stream_type == GF_STREAM_VISUAL)) {
				prog->pcr_idx = i+1;
			}
		}

		if (prog->pcr_idx) prog->pcr_idx-=1;
		gf_sdp_info_del(sdp);
		return 2;
	//open .bt file
	} else if (strstr(src, ".bt")) {
		u32 load_type=0;
		prog->seng = gf_seng_init(prog, src, load_type, NULL, (load_type == GF_SM_LOAD_DIMS) ? 1 : 0);
		
	    if (!prog->seng) {
			fprintf(stdout, "Cannot create scene engine\n");
			exit(0);
		}
		else{
			fprintf(stdout, "Scene engine created.\n");
		}

		prog->iod = gf_seng_get_iod(prog->seng);

		prog->nb_streams = gf_seng_get_stream_count(prog->seng);
		prog->rate = carousel_rate;
		*force_mpeg4 = 1;

		for (i=0; i<prog->nb_streams; i++) {
			fill_seng_es_ifce(&prog->streams[i], i, prog->seng, prog->rate);
			fprintf(stdout, "Fill interface\n");
			if (!prog->pcr_idx && (prog->streams[i].stream_type == GF_STREAM_VISUAL)) {
				prog->pcr_idx = i+1;
			}
		}
		if (!prog->pcr_idx) prog->pcr_idx=1;
		prog->pcr_idx-=1;
		prog->th = gf_th_new("Carousel");
		prog->src_name = update;
		gf_th_run(prog->th, seng_output, prog);
		return 1;
	} else {
		fprintf(stderr, "Error opening %s - not a supported input media, skipping.\n", src);
		return 0;
	}
}

int main(int argc, char **argv)
{
	const char *ts_pck;
	GF_Err e;
	u32 res, run_time;
	Bool real_time, mpeg4_signaling;
	GF_M2TS_Mux *muxer;
	u32 i, j, mux_rate, nb_progs, cur_pid, carrousel_rate;
	char *ts_out = NULL;
	FILE *ts_file;
	GF_Socket *ts_udp;
	GF_RTPChannel *ts_rtp;
	GF_RTSPTransport tr;
	GF_RTPHeader hdr;
	u16 port = 1234;
	u32 output_type;
	u32 check_count;
	char *src_name = NULL;
	M2TSProgram progs[MAX_MUX_SRC_PROG];

	real_time=0;	
	output_type = 0;
	ts_file = NULL;
	ts_udp = NULL;
	ts_rtp = NULL;
	ts_out = NULL;
	nb_progs = 0;
	mux_rate = 0;
	run_time = 0;
	mpeg4_signaling = 0;
	carrousel_rate = 500;
	
	gf_sys_init(0);
	gf_log_set_level(GF_LOG_ERROR);
	gf_log_set_tools(0xFFFFFFFF);
	
	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (arg[0]=='-') {
			if (!strnicmp(arg, "-rate=", 6)) {
				mux_rate = 1024*atoi(arg+6)*8;
			} else if (!strnicmp(arg, "-mpeg4-carousel=", 16)) {
				carrousel_rate = atoi(arg+16);
			} else if (!strnicmp(arg, "-ll=", 4)) {
				gf_log_set_level(gf_log_parse_level(argv[i]+4));
			} else if (!strnicmp(arg, "-lt=", 4)) {
				gf_log_set_tools(gf_log_parse_tools(argv[i]+4));
			} else if (!strnicmp(arg, "-prog=", 6)) {
				memset(&progs[nb_progs], 0, sizeof(M2TSProgram));
				res = open_program(&progs[nb_progs], arg+6, carrousel_rate, &mpeg4_signaling, src_name);
				if (res) {
					nb_progs++;
					if (res==2) real_time=1;
				}
			} else if (!strnicmp(arg, "-mpeg4", 6)) {
				mpeg4_signaling = 1;
			} else if (!strnicmp(arg, "-time=", 6)) {
				run_time = atoi(arg+6);
			} 
			else if (!strnicmp(arg, "-src=", 5)){
				src_name = arg+5;
			}
		} 
		/*output*/
		else {
			if (!strnicmp(arg, "rtp://", 6) || !strnicmp(arg, "udp://", 6)) {
				char *sep = strchr(arg+6, ':');
				output_type = (arg[0]=='r') ? 2 : 1;
				real_time=1;
				if (sep) {
					port = atoi(sep+1);
					sep[0]=0;
					ts_out = gf_strdup(arg+6);
					sep[0]=':';
				} else {
					ts_out = gf_strdup(arg+6);
				}
			} else {
				output_type = 0;
				ts_out = gf_strdup(arg);
			}
		}
	}
	if (!nb_progs || !ts_out) {
		usage();
		return 0;
	}
	
	if (run_time && !mux_rate) {
		fprintf(stdout, "Cannot specify TS run time for VBR multiplex - disabling run time\n");
		run_time = 0;
	}

	muxer = gf_m2ts_mux_new(mux_rate, real_time);
	muxer->mpeg4_signaling = mpeg4_signaling;
	/* Open mpeg2ts file*/
	switch(output_type) {
	case 0:
		ts_file = fopen(ts_out, "wb");
		if (!ts_file ) {
			fprintf(stderr, "Error opening %s\n", ts_out);
			goto exit;
		}
		break;
	case 1:
		ts_udp = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (gf_sk_is_multicast_address((char *)ts_out)) {
			e = gf_sk_setup_multicast(ts_udp, (char *)ts_out, port, 0, 0, NULL);
		} else {
			e = gf_sk_bind(ts_udp, NULL, port, (char *)ts_out, port, GF_SOCK_REUSE_PORT);
		}
		if (e) {
			fprintf(stdout, "Error inhitializing UDP socket: %s\n", gf_error_to_string(e));
			goto exit;
		}
		break;
	case 2:
		ts_rtp = gf_rtp_new();
		gf_rtp_set_ports(ts_rtp, port);
		tr.IsUnicast = gf_sk_is_multicast_address((char *)ts_out) ? 0 : 1;
		tr.Profile="RTP/AVP";
		tr.destination = (char *)ts_out;
		tr.source = "0.0.0.0";
		tr.IsRecord = 0;
		tr.Append = 0;
		tr.SSRC = rand();
		tr.port_first = port;
		tr.port_last = port+1;
		if (tr.IsUnicast) {
			tr.client_port_first = port;
			tr.client_port_last = port+1;
		} else {
			tr.source = (char *)ts_out;
		}
		res = gf_rtp_setup_transport(ts_rtp, &tr, (char *)ts_out);
		if (res !=0) {
			fprintf(stdout, "Cannot setup RTP transport info\n");
			goto exit;
		}
		res = gf_rtp_initialize(ts_rtp, 0, 1, 1500, 0, 0, NULL);
		if (res !=0) {
			fprintf(stdout, "Cannot initialize RTP sockets\n");
			goto exit;
		}
		memset(&hdr, 0, sizeof(GF_RTPHeader));
		hdr.Version = 2;
		hdr.PayloadType = 33;	/*MP2T*/
		hdr.SSRC = tr.SSRC;
		hdr.Marker = 0;
		break;
	}

	cur_pid = 100;
	for (i=0; i<nb_progs; i++) {
		GF_M2TS_Mux_Program *program = gf_m2ts_mux_program_add(muxer, i+1, cur_pid);
		if (muxer->mpeg4_signaling) program->iod = progs[i].iod;
		for (j=0; j<progs[i].nb_streams; j++) {
			gf_m2ts_program_stream_add(program, &progs[i].streams[j], cur_pid+j+1, (progs[i].pcr_idx==j) ? 1 : 0);
		}
		cur_pid += progs[i].nb_streams;
		while (cur_pid % 10)
			cur_pid ++;
	}

	gf_m2ts_mux_update_config(muxer, 1);

	check_count = 100;
	while (1) {
		u32 ts;
		u32 status;
		/*flush all packets*/
		switch (output_type) {
		case 0:
			while ((ts_pck = gf_m2ts_mux_process(muxer, &status)) != NULL) {
				fwrite(ts_pck, 1, 188, ts_file); 
				if (status>=GF_M2TS_STATE_PADDING) break;
			}
			break;
		case 1:
			while ((ts_pck = gf_m2ts_mux_process(muxer, &status)) != NULL) {
				e = gf_sk_send(ts_udp, (char*)ts_pck, 188); 
				if (e) 
					fprintf(stdout, "Error %s sending UDP packet\n", gf_error_to_string(e));
				if (status>=GF_M2TS_STATE_PADDING) break;
			}
			break;
		case 2:
			while ((ts_pck = gf_m2ts_mux_process(muxer, &status)) != NULL) {
				hdr.SequenceNumber++;
				/*muxer clock at 90k*/
				ts = muxer->time.sec*90000 + muxer->time.nanosec*9/100000;
				/*FIXME - better discontinuity check*/
				hdr.Marker = (ts < hdr.TimeStamp) ? 1 : 0;
				hdr.TimeStamp = ts;
				e = gf_rtp_send_packet(ts_rtp, &hdr, (char*)ts_pck, 188, 0);
				if (e) 
					fprintf(stdout, "Error %s sending RTP packet\n", gf_error_to_string(e));
				if (status>=GF_M2TS_STATE_PADDING) break;
			}
			break;
		}
		if (real_time) {
			check_count --;
			if (!check_count) {
				check_count=100;
				/*abort*/
				if (gf_prompt_has_input()) {
					char c = gf_prompt_get_char();
					if (c == 'q') break;
				}
				//fprintf(stdout, "M2TS: time %d - TS time %d - avg bitrate %d\r", gf_m2ts_get_sys_clock(muxer), gf_m2ts_get_ts_clock(muxer), muxer->avg_br);
			}
		} else if (run_time) {
			if (gf_m2ts_get_ts_clock(muxer) > run_time) {
				fprintf(stdout, "Stoping multiplex at %d ms (requested runtime %d ms)\n", gf_m2ts_get_ts_clock(muxer), run_time);
				break;
			}
		} else if (status==GF_M2TS_STATE_EOS) {
			break;
		}
	}


exit:
	run = 0;
	if (ts_file) fclose(ts_file);
	if (ts_udp) gf_sk_del(ts_udp);
	if (ts_rtp) gf_rtp_del(ts_rtp);
	if (ts_out) gf_free(ts_out);
	gf_m2ts_mux_del(muxer);
	
	for (i=0; i<nb_progs; i++) {
		for (j=0; j<progs[i].nb_streams; j++) {
			progs[i].streams[j].input_ctrl(&progs[i].streams[j], GF_ESI_INPUT_DESTROY, NULL);
		}
		if (progs[i].iod) gf_odf_desc_del((GF_Descriptor*)progs[i].iod);
		if (progs[i].mp4) gf_isom_close(progs[i].mp4);
	}
	gf_sys_close();
	return 1;
}

