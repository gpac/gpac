/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) ENST 2000-200X
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4-to-ts (mp42ts) application
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


#define UDP_BUFFER_SIZE	0x40000

#define MP42TS_PRINT_FREQ 634 /*refresh printed info every CLOCK_REFRESH ms*/

static GFINLINE void usage() 
{
	fprintf(stderr, "usage: mp42ts {rate {prog}*} [audio] [mpeg4-carousel] [mpeg4] [time] [src] [segment-options] dst\n"
					"\n"
					"-rate=R                specifies target rate in kbps of the multiplex\n"
					"                        If not set transport stream will be of variable bitrate\n"
					"-prog=filename         specifies an input file used for a TS service\n"
					"                        * currently only supports ISO files and SDP files\n"
					"                        * can be used several times, once for each program\n"
					"-audio=url             may be mp3/udp or aac/http\n"
					"-mpeg4-carousel=n      carousel period in ms\n"
                    "-mpeg4                 forces usage of MPEG-4 signaling (IOD and SL Config)\n"
					"-time=n                request the program to stop after n ms\n"
					"-src=filename          update file: must be either an .sdp or a .bt file\n"
					"\n"
					"-dst-udp               \\\n"
					"-dst-file               } unicast/multicast destination\n"
					"-dst-rtp               /\n"
					"\n"
					"-segment-dir=dir       server local address to store segments\n"
					"-segment-duration=dur  segment duration\n"
					"-segment-manifest=file m3u8 file basename\n"
					"-segment-http-prefix=p client address for accessing server segments\n"
					"-segment-number=n      only n segments are used using a cyclic pattern\n"
					"\n"
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
	u32 mpeg4_signaling;
	Bool audio_configured;
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

typedef struct
{
	u32 size;
	char *data;
} GF_SimpleDataDescriptor;

//TODO: find a clean way to save this data
static u32 audio_OD_stream_id = (u32)-1;
#define AUDIO_OD_ESID	100
#define AUDIO_DATA_ESID	101

/*output types*/
enum
{
	GF_MP42TS_FILE, /*open mpeg2ts file*/
	GF_MP42TS_UDP,  /*open udp socket*/
	GF_MP42TS_RTP,  /*open rtp socket*/
	GF_MP42TS_HTTP,	/*open http downloader*/
};

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
	u64 avg_rate, duration;

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
	if (0!=(duration=gf_isom_get_media_duration(mp4, track_num)))
		avg_rate /= duration;

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
		//TODO: free my data
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

static GF_Err udp_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	return GF_OK;
}

/*AAC import features*/
void *audio_prog = NULL;
static void SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts);
#define DONT_USE_TERMINAL_MODULE_API
#include "../modules/aac_in/aac_in.c"
AACReader *aac_reader = NULL;

/*create an OD codec and encode the descriptor*/
static GF_Err encode_audio_desc(GF_ESD *esd, GF_SimpleDataDescriptor *audio_desc) 
{
	GF_Err e;
	GF_ODCodec *odc = gf_odf_codec_new();
	GF_ODUpdate *od_com = (GF_ODUpdate*)gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	gf_list_add(od->ESDescriptors, esd);
	od->objectDescriptorID = AUDIO_DATA_ESID;
	gf_list_add(od_com->objectDescriptors, od);

	e = gf_odf_codec_add_com(odc, (GF_ODCom*)od_com);
	if (e) {
		fprintf(stderr, "Audio input error add the command to be encoded\n");
		return e;
	}
	e = gf_odf_codec_encode(odc, 0);
	if (e) {
		fprintf(stderr, "Audio input error encoding the descriptor\n");
		return e;
	}
	e = gf_odf_codec_get_au(odc, &audio_desc->data, &audio_desc->size);
	if (e) {
		fprintf(stderr, "Audio input error getting the descriptor\n");
		return e;
	}
	e = gf_odf_com_del((GF_ODCom**)&od_com);
	if (e) {
		fprintf(stderr, "Audio input error deleting the command\n");
		return e;
	}
	gf_odf_codec_del(odc);

	return GF_OK;
}

static void SampleCallBack(void *calling_object, u16 ESID, char *data, u32 size, u64 ts)
{		
	u32 i;
	//fprintf(stderr, "update: ESID=%d - size=%d - ts="LLD"\n", ESID, size, ts);

	if (calling_object) {
		M2TSProgram *prog = (M2TSProgram *)calling_object;

		if (ESID == AUDIO_DATA_ESID) {
			if (audio_OD_stream_id != (u32)-1) {
				/*send the audio descriptor when present*/
				GF_SimpleDataDescriptor *audio_desc = prog->streams[audio_OD_stream_id].input_udta;
				if (audio_desc && !audio_desc->data) /*intended for HTTP/AAC: an empty descriptor was set (vs already filled for RTP/UDP MP3)*/
				{
					GF_ESD *esd = AAC_GetESD(aac_reader);
					assert(esd->slConfig->timestampResolution);
					esd->slConfig->useAccessUnitStartFlag = 1;
					esd->slConfig->useAccessUnitEndFlag = 1;
					esd->slConfig->useTimestampsFlag = 1;
					esd->slConfig->timestampLength = 33;
					/*audio stream, all samples are RAPs*/
					esd->slConfig->useRandomAccessPointFlag = 0;
					esd->slConfig->hasRandomAccessUnitsOnlyFlag = 1;
					for (i=0; i<prog->nb_streams; i++) {
						if (prog->streams[i].stream_id == AUDIO_DATA_ESID) {
							GF_Err e;
							prog->streams[i].timescale = esd->slConfig->timestampResolution;
							e = gf_m2ts_program_stream_update_ts_scale(&prog->streams[i], esd->slConfig->timestampResolution);
							assert(!e);
							gf_m2ts_program_stream_update_sl_config(&prog->streams[i], esd->slConfig);
							break;
						}
					}
					esd->ESID = AUDIO_DATA_ESID;
					assert(audio_OD_stream_id != (u32)-1);
					encode_audio_desc(esd, audio_desc);
					prog->repeat = 1;
					SampleCallBack(prog, AUDIO_OD_ESID, audio_desc->data, audio_desc->size, gf_sys_clock());
					prog->repeat = 0;
					gf_free(audio_desc->data);
					gf_free(audio_desc);
					prog->streams[audio_OD_stream_id].input_udta = NULL;
	
					gf_odf_desc_del((GF_Descriptor *)esd);
				}
			}
			/*update the timescale if needed*/
			else if (!prog->audio_configured) {
				GF_ESD *esd = AAC_GetESD(aac_reader);
				assert(esd->slConfig->timestampResolution);
				for (i=0; i<prog->nb_streams; i++) {
					if (prog->streams[i].stream_id == AUDIO_DATA_ESID) {
						GF_Err e;
						prog->streams[i].timescale = esd->slConfig->timestampResolution;
						prog->streams[i].decoder_config = esd->decoderConfig->decoderSpecificInfo->data;
						prog->streams[i].decoder_config_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
						esd->decoderConfig->decoderSpecificInfo->data = NULL;
						esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
						e = gf_m2ts_program_stream_update_ts_scale(&prog->streams[i], esd->slConfig->timestampResolution);
						assert(!e);
						prog->audio_configured = 1;
						break;
					}
				}
				gf_odf_desc_del((GF_Descriptor *)esd);
			}
		}

		i=0;
		while (i<prog->nb_streams){
			if (prog->streams[i].output_ctrl==NULL) {
				fprintf(stderr, "MULTIPLEX NOT YET CREATED\n");
				return;
			}
			if (prog->streams[i].stream_id == ESID) {
				GF_ESIStream *priv = (GF_ESIStream *)prog->streams[i].input_udta;
				GF_ESIPacket pck;
				memset(&pck, 0, sizeof(GF_ESIPacket));
				pck.data = data;
				pck.data_len = size;
				pck.flags |= GF_ESI_DATA_HAS_CTS;
				pck.flags |= GF_ESI_DATA_HAS_DTS;
				pck.flags |= GF_ESI_DATA_AU_START;
				pck.flags |= GF_ESI_DATA_AU_END;
				if (ts) pck.cts = pck.dts = ts;

				if (priv->rap)
					pck.flags |= GF_ESI_DATA_AU_RAP;
				if (prog->repeat || !priv->vers_inc) {
					pck.flags |= GF_ESI_DATA_REPEAT;
					fprintf(stderr, "RAP carousel from scene engine sent: ESID=%d - size=%d - ts="LLD"\n", ESID, size, ts);
				} else {
					if (ESID != AUDIO_DATA_ESID)	/*don't log audio input*/
						fprintf(stderr, "Update from scene engine sent: ESID=%d - size=%d - ts="LLD"\n", ESID, size, ts); 
				}
				prog->streams[i].output_ctrl(&prog->streams[i], GF_ESI_OUTPUT_DATA_DISPATCH, &pck);
				return;
			}
		i++;
		}
	} 
	return;
}

//static gf_seng_callback * SampleCallBack = &mySampleCallBack;


static volatile Bool run = 1;

static GF_ESIStream * set_broadcast_params(M2TSProgram *prog, u16 esid, u32 period, u32 ts_delta, u16 aggregate_on_stream, Bool adjust_carousel_time, Bool force_rap, Bool aggregate_au, Bool discard_pending, Bool signal_rap, Bool signal_critical, Bool version_inc)
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
		/*TODO: stream not found*/
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

static Bool seng_output(void *param)
{
	GF_Err e;
	u64 last_src_modif, mod_time;
	Bool has_carousel=0;
	M2TSProgram *prog = (M2TSProgram *)param;
	GF_SceneEngine *seng = prog->seng;
	GF_SimpleDataDescriptor *audio_desc;
	Bool update_context=0;
	Bool force_rap, adjust_carousel_time, discard_pending, signal_rap, signal_critical, version_inc, aggregate_au;
	u32 period, ts_delta;
	u16 es_id, aggregate_on_stream;

	if (prog->rate){
		has_carousel = 1;
	}
	gf_sleep(2000); /*TODO: events instead? What are we waiting for?*/
	gf_seng_encode_context(seng, SampleCallBack);
	
	last_src_modif = prog->src_name ? gf_file_modification_time(prog->src_name) : 0;

	/*send the audio descriptor*/
	if (prog->mpeg4_signaling==GF_M2TS_MPEG4_SIGNALING_FULL) {
		audio_desc = prog->streams[audio_OD_stream_id].input_udta;
		if (audio_desc && audio_desc->data) /*RTP/UDP + MP3 case*/
		{
			assert(audio_OD_stream_id != (u32)-1);
			prog->repeat = 1;
			SampleCallBack(prog, AUDIO_OD_ESID, audio_desc->data, audio_desc->size, gf_sys_clock());
			prog->repeat = 0;
			gf_free(audio_desc->data);
			gf_free(audio_desc);
			prog->streams[audio_OD_stream_id].input_udta = NULL;
		}
	}

	while (run) {
		if (!gf_prompt_has_input()) {
			if (prog->src_name) {
				mod_time = gf_file_modification_time(prog->src_name);
				if (mod_time != last_src_modif) {
					FILE *srcf;
					char flag_buf[201], *flag;
					fprintf(stderr, "Update file modified - processing\n");
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
							if (!strnicmp(flag, "esid=", 5)) {
								/*ESID on which the update is applied*/
								es_id = atoi(flag+5);
							} else if (!strnicmp(flag, "period=", 7)) {
								/*TODO: target period carousel for ESID ??? (ESID/carousel)*/
								period = atoi(flag+7);
							} else if (!strnicmp(flag, "ts=", 3)) {
								/*TODO: */
								ts_delta = atoi(flag+3);
							} else if (!strnicmp(flag, "carousel=", 9)) {
								/*TODO: why? => sends the update on carousel id specified by this argument*/
								aggregate_on_stream = atoi(flag+9);
							} else if (!strnicmp(flag, "restamp=", 8)) {
								/*CTS is updated when carouselled*/
								adjust_carousel_time = atoi(flag+8);
							} else if (!strnicmp(flag, "discard=", 8)) {
								/*when we receive several updates during a single carousel period, this attribute specifies whether the current update discard pending ones*/
								discard_pending = atoi(flag+8);
							} else if (!strnicmp(flag, "aggregate=", 10)) {
								/*Boolean*/
								aggregate_au = atoi(flag+10);
							} else if (!strnicmp(flag, "force_rap=", 10)) {
								/*TODO: */
								force_rap = atoi(flag+10);
							} else if (!strnicmp(flag, "rap=", 4)) {
								/*TODO: */
								signal_rap = atoi(flag+4);
							} else if (!strnicmp(flag, "critical=", 9)) {
								/*TODO: */
								signal_critical = atoi(flag+9);
							} else if (!strnicmp(flag, "vers_inc=", 9)) {
								/*Boolean to increment m2ts section version number*/
								version_inc = atoi(flag+9);
							}
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
					if (e){
						fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
					} else
						gf_seng_aggregate_context(seng, 0);

					update_context=1;

					

				}
			}
			if (update_context) {
				prog->repeat = 1;
				e = gf_seng_encode_context(seng, SampleCallBack);
				prog->repeat = 0;
				update_context = 0;
			}

			gf_sleep(10);
		} else { /*gf_prompt_has_input()*/
			char c = gf_prompt_get_char();
			switch (c) {
				case 'u':
				{
					GF_Err e;
					char szCom[8192];
					fprintf(stderr, "Enter command to send:\n");
					fflush(stdin);
					szCom[0] = 0;
					scanf("%[^\t\n]", szCom);
					e = gf_seng_encode_from_string(seng, 0, 0, szCom, SampleCallBack);
					if (e) { 
						fprintf(stderr, "Processing command failed: %s\n", gf_error_to_string(e));
					}
					update_context=1;
				}
					break;
				case 'p':
				{
					char rad[GF_MAX_PATH];
					fprintf(stderr, "Enter output file name - \"std\" for stdout: ");
					scanf("%s", rad);
					e = gf_seng_save_context(seng, !strcmp(rad, "std") ? NULL : rad);
					fprintf(stderr, "Dump done (%s)\n", gf_error_to_string(e)); 
				}
					break;
				case 'q':
				{
					run = 0;
				}
			}
			e = GF_OK;
		}
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

static void fill_rtp_es_ifce(GF_ESInterface *ifce, GF_SDPMedia *media, GF_SDPInfo *sdp)
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
		fprintf(stderr, "Cannot initialize RTP transport\n");
		return;
	}
	/*setup depacketizer*/
	rtp->depacketizer = gf_rtp_depacketizer_new(media, rtp_sl_packet_cbk, rtp);
	if (!rtp->depacketizer) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stderr, "Cannot create RTP depacketizer\n");
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
		fprintf(stderr, "Cannot initialize RTP channel: %s\n", gf_error_to_string(e));
		return;
	}
	fprintf(stderr, "RTP interface initialized\n");
}

void fill_seng_es_ifce(GF_ESInterface *ifce, u32 i, GF_SceneEngine *seng, u32 period)
{
	GF_Err e = GF_OK;
	u32 len;
	GF_ESIStream *stream;
	const char ** config_buffer = NULL;

	memset(ifce, 0, sizeof(GF_ESInterface));
	e = gf_seng_get_stream_config(seng, i, (u16*) &(ifce->stream_id), config_buffer, &len, (u32*) &(ifce->stream_type), (u32*) &(ifce->object_type_indication), &(ifce->timescale)); 
	if (e) {
		fprintf(stderr, "Cannot set the stream config for stream %d to %d: %s\n", ifce->stream_id, period, gf_error_to_string(e));
	}

	ifce->repeat_rate = period;
	GF_SAFEALLOC(stream, GF_ESIStream);
	stream->rap = 1;
	ifce->input_udta = stream;
	
	//fprintf(stderr, "Caroussel period: %d\n", period);
//	e = gf_seng_set_carousel_time(seng, ifce->stream_id, period);
	if (e) {
		fprintf(stderr, "Cannot set carousel time on stream %d to %d: %s\n", ifce->stream_id, period, gf_error_to_string(e));
	}
	ifce->input_ctrl = seng_input_ctrl;

}

static Bool open_program(M2TSProgram *prog, char *src, u32 carousel_rate, Bool force_mpeg4, char *update, char *audio_input_ip, u16 audio_input_port)
{
	GF_SDPInfo *sdp;
	u32 i;
	GF_Err e;
	
	memset(prog, 0, sizeof(M2TSProgram));
	prog->mpeg4_signaling = force_mpeg4 ? GF_M2TS_MPEG4_SIGNALING_FULL : GF_M2TS_MPEG4_SIGNALING_NONE;

	/*open ISO file*/
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
					prog->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;
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
				default:
					/*log not supported stream type: %s*/
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
				prog->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;
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
	} 
	else if (strstr(src, ".bt")) //open .bt file
	{
		u32 load_type=0;
		prog->seng = gf_seng_init(prog, src, load_type, NULL, (load_type == GF_SM_LOAD_DIMS) ? 1 : 0);
		
		if (!prog->seng) {
			fprintf(stderr, "Cannot create scene engine\n");
			exit(1);
		}
		else{
			fprintf(stderr, "Scene engine created.\n");
		}

		prog->iod = gf_seng_get_iod(prog->seng);

		prog->nb_streams = gf_seng_get_stream_count(prog->seng);
		prog->rate = carousel_rate;
		prog->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_FULL;

		for (i=0; i<prog->nb_streams; i++) {
			fill_seng_es_ifce(&prog->streams[i], i, prog->seng, prog->rate);
			//fprintf(stderr, "Fill interface\n");
			if (!prog->pcr_idx && (prog->streams[i].stream_type == GF_STREAM_AUDIO)) {
				prog->pcr_idx = i+1;
			}
		}

		/*when an audio input is present, declare it and store OD + ESD_U*/
		if (audio_input_ip) {
			/*add the audio program*/
			prog->pcr_idx = prog->nb_streams;
			prog->streams[prog->nb_streams].stream_type = GF_STREAM_AUDIO;
			/*hack: http urls are not decomposed therefore audio_input_port remains null*/
			if (audio_input_port) {	/*UDP/RTP*/
				prog->streams[prog->nb_streams].object_type_indication = GPAC_OTI_AUDIO_MPEG1;
			} else { /*HTTP*/
				aac_reader->oti = prog->streams[prog->nb_streams].object_type_indication = GPAC_OTI_AUDIO_AAC_MPEG4;
			}
			prog->streams[prog->nb_streams].input_ctrl = udp_input_ctrl;
			prog->streams[prog->nb_streams].stream_id = AUDIO_DATA_ESID;
			prog->streams[prog->nb_streams].timescale = 1000;

			GF_SAFEALLOC(prog->streams[i].input_udta, GF_ESIStream);
			((GF_ESIStream*)prog->streams[i].input_udta)->vers_inc = 1;	/*increment version number at every audio update*/

			if ((prog->iod->tag!=GF_ODF_IOD_TAG) || ((GF_InitialObjectDescriptor*)prog->iod)->OD_profileAndLevel!=GPAC_MAGIC_OD_PROFILE_FOR_MPEG4_SIGNALING) {
				/*create the descriptor*/
				GF_ESD *esd;
				GF_SimpleDataDescriptor *audio_desc;
				GF_SAFEALLOC(audio_desc, GF_SimpleDataDescriptor);
				if (audio_input_port) {	/*UDP/RTP*/
					esd = gf_odf_desc_esd_new(0);
					esd->decoderConfig->streamType = prog->streams[prog->nb_streams].stream_type;
					esd->decoderConfig->objectTypeIndication = prog->streams[prog->nb_streams].object_type_indication;
				} else {				/*HTTP*/
					esd = AAC_GetESD(aac_reader);		/*in case of AAC, we have to wait the first ADTS chunk*/
				}
				esd->ESID = prog->streams[prog->nb_streams].stream_id;
				if (esd->slConfig->timestampResolution) /*in case of AAC, we have to wait the first ADTS chunk*/
					encode_audio_desc(esd, audio_desc);

				/*find the audio OD stream and attach its descriptor*/
				for (i=0; i<prog->nb_streams; i++) {
					if (prog->streams[i].stream_id == AUDIO_OD_ESID) {
						prog->streams[i].input_udta = (void*)audio_desc;
						audio_OD_stream_id = i;
						break;
					}
				}
				if (audio_OD_stream_id == (u32)-1) {
					fprintf(stderr, "Error: could not find an audio OD stream with ESID=100 in '%s'\n", src);
					return 0;
				}
			} else {
				prog->mpeg4_signaling = GF_M2TS_MPEG4_SIGNALING_SCENE;
			}
			prog->nb_streams++;
		}

		if (!prog->pcr_idx) prog->pcr_idx=1;
		prog->th = gf_th_new("Carousel");
		prog->src_name = update;
		gf_th_run(prog->th, seng_output, prog);
		return 1;
	} else {
		fprintf(stderr, "Error opening %s - not a supported input media, skipping.\n", src);
		return 0;
	}
}

/*parse MP42TS arguments*/
static GFINLINE GF_Err parse_args(int argc, char **argv, u32 *mux_rate, u32 *carrousel_rate, 
								  M2TSProgram *progs, u32 *nb_progs, char **src_name, 
								  Bool *real_time, u32 *run_time,
								  u32 *audio_input_type, char **audio_input_ip, u16 *audio_input_port,
								  u32 *output_type, char **ts_out, char **udp_out, char **rtp_out, u16 *output_port, 
								  char** segment_dir, u32 *segment_duration, char **segment_manifest, u32 *segment_number, char **segment_http_prefix)
{
	Bool rate_found=0, mpeg4_carousel_found=0, prog_found=0, mpeg4_found=0, time_found=0, src_found=0, dst_found=0, audio_input_found=0, 
		 seg_dur_found=0, seg_dir_found=0, seg_manifest_found=0, seg_number_found=0, seg_http_found = 0, real_time_found=0;
	char *prog_name, *arg, *error_msg;
	Bool mpeg4_signaling = 0; 
	s32 i;
	/*first pass: find audio*/
	for (i=1; i<argc; i++) {
		arg = argv[i];
		if (!strnicmp(arg, "-audio=", 7)) {
			if (audio_input_found) {
				error_msg = "multiple '-audio' found";
				arg = NULL;
				goto error;
			}
			audio_input_found = 1;
			arg+=7;
			if (!strnicmp(arg, "udp://", 6) || !strnicmp(arg, "rtp://", 6) || !strnicmp(arg, "http://", 7)) {
				char *sep;
				/*set audio input type*/
				if (!strnicmp(arg, "udp://", 6))
					*audio_input_type = GF_MP42TS_UDP;
				else if (!strnicmp(arg, "rtp://", 6))
					*audio_input_type = GF_MP42TS_RTP;
				else if (!strnicmp(arg, "http://", 7))
					*audio_input_type = GF_MP42TS_HTTP;
				/*http needs to get the complete URL*/
				switch(*audio_input_type) {
					case GF_MP42TS_UDP:
					case GF_MP42TS_RTP:
						sep = strchr(arg+6, ':');
						*real_time=1;
						if (sep) {
							*audio_input_port = atoi(sep+1);
							sep[0]=0;
							*audio_input_ip = gf_strdup(arg+6);
							sep[0]=':';
						} else {
							*audio_input_ip = gf_strdup(arg+6);
						}
						break;
					case GF_MP42TS_HTTP:
						*audio_input_ip = gf_strdup(arg);
						assert(audio_input_port != 0);
						break;
					default:
						assert(0);
				}
			} else if (!strnicmp(arg, "-mpeg4", 6)) {
				if (mpeg4_found) {
					error_msg = "multiple '-mpeg4' found";
					arg = NULL;
					goto error;
				}
				mpeg4_found = 1;
				mpeg4_signaling = 1;
			}
		}
	}
	/*second pass: other*/
	for (i=1; i<argc; i++) {
		arg = argv[i];
		if (arg[0]=='-') {
			if (!strnicmp(arg, "-rate=", 6)) {
				if (rate_found) {
					error_msg = "multiple '-rate' found";
					arg = NULL;
					goto error;
				}
				rate_found = 1;
				*mux_rate = 1024*atoi(arg+6);
			} else if (!strnicmp(arg, "-mpeg4-carousel=", 16)) {
				if (mpeg4_carousel_found) {
					error_msg = "multiple '-mpeg4-carousel' found";
					arg = NULL;
					goto error;
				}
				mpeg4_carousel_found = 1;
				*carrousel_rate = atoi(arg+16);
			} else if (!strnicmp(arg, "-ll=", 4)) {
				gf_log_set_level(gf_log_parse_level(argv[i]+4));
			} else if (!strnicmp(arg, "-lt=", 4)) {
				gf_log_set_tools(gf_log_parse_tools(argv[i]+4));
			} else if (!strnicmp(arg, "-prog=", 6)) {
				prog_found = 1;
				prog_name = arg+6;
				if (prog_found && rate_found) {
					u32 res;
					assert(prog_name);
					res = open_program(&progs[*nb_progs], prog_name, *carrousel_rate, mpeg4_signaling, *src_name, *audio_input_ip, *audio_input_port);
					if (res) {
						(*nb_progs)++;
						if (res==2) *real_time=1;
					}
				}
			} else if (!strnicmp(arg, "-real-time", 10)) {
				if (real_time_found) {
					goto error;
				}
				real_time_found = 1;
				*real_time = 1;
			} else if (!strnicmp(arg, "-time=", 6)) {
				if (time_found) {
					error_msg = "multiple '-time' found";
					arg = NULL;
					goto error;
				}
				time_found = 1;
				*run_time = atoi(arg+6);
			} else if (!strnicmp(arg, "-segment-dir=", 13)) {
				if (seg_dir_found) {
					goto error;
				}
				seg_dir_found = 1;
				*segment_dir = arg+13;
			} else if (!strnicmp(arg, "-segment-duration=", 18)) {
				if (seg_dur_found) {
					goto error;
				}
				seg_dur_found = 1;
				*segment_duration = atoi(arg+18);
			} else if (!strnicmp(arg, "-segment-manifest=", 18)) {
				if (seg_manifest_found) {
					goto error;
				}
				seg_manifest_found = 1;
				*segment_manifest = arg+18;
			} else if (!strnicmp(arg, "-segment-http-prefix=", 21)) {
				if (seg_http_found) {
					goto error;
				}
				seg_http_found = 1;
				*segment_http_prefix = arg+21;
			} else if (!strnicmp(arg, "-segment-number=", 16)) {
				if (seg_number_found) {
					goto error;
				}
				seg_number_found = 1;
				*segment_number = atoi(arg+16);
			} 
			else if (!strnicmp(arg, "-src=", 5)) {
				if (src_found) {
					error_msg = "multiple '-src' found";
					arg = NULL;
					goto error;
				}
				src_found = 1;
				*src_name = arg+5;
			}
			else if (!strnicmp(arg, "-dst-file=", 10)) {
				dst_found = 1;
				*ts_out = gf_strdup(arg+10);
			}
			else if (!strnicmp(arg, "-dst-udp=", 9)) {
				char *sep = strchr(arg+9, ':');
				dst_found = 1;
				*real_time=1;
				if (sep) {
					*output_port = atoi(sep+1);
					sep[0]=0;
					*udp_out = gf_strdup(arg+9);
					sep[0]=':';
				} else {
					*udp_out = gf_strdup(arg+9);
				}
			}
			else if (!strnicmp(arg, "-dst-rtp=", 9)) {
				char *sep = strchr(arg+9, ':');
				dst_found = 1;
				*real_time=1;
				if (sep) {
					*output_port = atoi(sep+1);
					sep[0]=0;
					*rtp_out = gf_strdup(arg+9);
					sep[0]=':';
				} else {
					*rtp_out = gf_strdup(arg+9);
				}
			}
			else if (!strnicmp(arg, "-audio=", 7))
				; /*already treated on the first pass*/
			else {
				error_msg = "unknown option \"%s\"";
				goto error;
			}
		} 
#if 0
		else { /*"dst" argument (output)*/
			if (dst_found) {
				error_msg = "multiple output arguments (no '-') found";
				arg = NULL;
				goto error;
			}
			dst_found = 1;
			if (!strnicmp(arg, "rtp://", 6) || !strnicmp(arg, "udp://", 6)) {
				char *sep = strchr(arg+6, ':');
				*output_type = (arg[0]=='r') ? 2 : 1;
				*real_time=1;
				if (sep) {
					*output_port = atoi(sep+1);
					sep[0]=0;
					*ts_out = gf_strdup(arg+6);
					sep[0]=':';
				} else {
					*ts_out = gf_strdup(arg+6);
				}
			} else {
				*output_type = 0;
				*ts_out = gf_strdup(arg);
			}
		}
#endif
	}

	/*testing the only mandatory argument*/
	if (dst_found && prog_found && rate_found)
		return GF_OK;

error:
	if (!arg) {
		fprintf(stderr, "Error: %s\n", error_msg);
	} else {
		fprintf(stderr, "Error: %s \"%s\"\n", error_msg, arg);
	}
	return GF_BAD_PARAM;
}

/* adapted from http://svn.assembla.com/svn/legend/segmenter/segmenter.c */
static GF_Err write_manifest(char *manifest, char *segment_dir, u32 segment_duration, char *segment_prefix, char *http_prefix, 
							u32 first_segment, u32 last_segment, Bool end) {
	FILE *manifest_fp;
	u32 i;
	char manifest_tmp_name[GF_MAX_PATH];
	char manifest_name[GF_MAX_PATH];
	char *tmp_manifest = manifest_tmp_name;

	sprintf(manifest_tmp_name, "%stmp.m3u8", segment_dir);
	sprintf(manifest_name, "%s%s", segment_dir, manifest);

	manifest_fp = fopen(tmp_manifest, "w");
	if (!manifest_fp) {
		fprintf(stderr, "Could not create m3u8 manifest file (%s)\n", tmp_manifest);
		return GF_BAD_PARAM;
	}

	fprintf(manifest_fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n#EXT-X-MEDIA-SEQUENCE:%u\n", segment_duration, first_segment);

	for (i = first_segment; i <= last_segment; i++) {
		fprintf(manifest_fp, "#EXTINF:%u,\n%s%s_%u.ts\n", segment_duration, http_prefix, segment_prefix, i);
	}

	if (end) {
		fprintf(manifest_fp, "#EXT-X-ENDLIST\n");
	}
	fclose(manifest_fp);

	if (!rename(tmp_manifest, manifest_name)) {
		return GF_OK;
	} else {
		fprintf(stderr, "Could not rename temporary m3u8 manifest file (%s) into %s\n", tmp_manifest, manifest_name);
		return GF_BAD_PARAM;
	}
}

int main(int argc, char **argv)
{
	/********************/
	/*   declarations   */
	/********************/
	const char *ts_pck;
	GF_Err e;
	u32 res, run_time;
	Bool real_time;
	GF_M2TS_Mux *muxer=NULL;
	u32 i, j, mux_rate, nb_progs, cur_pid, carrousel_rate, last_print_time;
	char *ts_out = NULL, *udp_out = NULL, *rtp_out = NULL, *audio_input_ip = NULL;
	FILE *ts_output_file = NULL;
	GF_Socket *ts_output_udp_sk = NULL, *audio_input_udp_sk = NULL;
	GF_RTPChannel *ts_output_rtp = NULL;
	GF_RTSPTransport tr;
	GF_RTPHeader hdr;
	u16 output_port = 0, audio_input_port = 0;
	u32 output_type, audio_input_type;
	char *audio_input_buffer = NULL;
	u32 audio_input_buffer_length=65536;
	char *src_name;
	M2TSProgram progs[MAX_MUX_SRC_PROG];
	u32 segment_duration, segment_index, segment_number;
	char segment_manifest_default[GF_MAX_PATH];
	char *segment_manifest, *segment_http_prefix, *segment_dir;
	char segment_prefix[GF_MAX_PATH];
	char segment_name[GF_MAX_PATH];
	GF_M2TS_Time prev_seg_time;

	/***********************/
	/*   initialisations   */
	/***********************/
	real_time=0;	
	ts_output_file = NULL;
	ts_output_udp_sk = NULL;
	ts_output_rtp = NULL;
	src_name = NULL;
	ts_out = NULL;
	udp_out = NULL;
	rtp_out = NULL;
	nb_progs = 0;
	mux_rate = 0;
	run_time = 0;
	carrousel_rate = 500;
	output_port = 1234;
	segment_duration = 0;
	segment_number = 10; /* by default, we keep the 10 previous segments */
	segment_index = 0;
	segment_manifest = NULL;
	segment_http_prefix = NULL;
	segment_dir = NULL;
	prev_seg_time.sec = 0;
	prev_seg_time.nanosec = 0;
	GF_SAFEALLOC(aac_reader, AACReader);
	
	/*****************/
	/*   gpac init   */
	/*****************/
	gf_sys_init(0);
	gf_log_set_level(GF_LOG_ERROR);
	gf_log_set_tools(0xFFFFFFFF);
	//gf_log_set_level(GF_LOG_DEBUG);
	//gf_log_set_tools(GF_LOG_CONTAINER);
	
	/***********************/
	/*   parse arguments   */
	/***********************/
	if (GF_OK != parse_args(argc, argv, &mux_rate, &carrousel_rate, progs, &nb_progs, &src_name, 
							&real_time, &run_time,
							&audio_input_type, &audio_input_ip, &audio_input_port,
							&output_type, &ts_out, &udp_out, &rtp_out, &output_port, 
							&segment_dir, &segment_duration, &segment_manifest, &segment_number, &segment_http_prefix)) {
		usage();
		goto exit;
	}
	
	if (run_time && !mux_rate) {
		fprintf(stderr, "Cannot specify TS run time for VBR multiplex - disabling run time\n");
		run_time = 0;
	}

	/***************************/
	/*   create mp42ts muxer   */
	/***************************/
	muxer = gf_m2ts_mux_new(mux_rate, real_time);
	if (ts_out != NULL) {
		if (segment_duration) {
			char *dot;
			strcpy(segment_prefix, ts_out);
			dot = strrchr(segment_prefix, '.');
			dot[0] = 0;
			sprintf(segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index);
			ts_out = segment_name;
			if (!segment_manifest) { 
				sprintf(segment_manifest_default, "%s.m3u8", segment_prefix);
				segment_manifest = segment_manifest_default;
			}
			//write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix, segment_index, 0, 0);
		} 
		ts_output_file = fopen(ts_out, "wb");
		if (!ts_output_file) {
			fprintf(stderr, "Error opening %s\n", ts_out);
			goto exit;
		}
	}
	if (udp_out != NULL) {
		ts_output_udp_sk = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (gf_sk_is_multicast_address((char *)udp_out)) {
			e = gf_sk_setup_multicast(ts_output_udp_sk, (char *)udp_out, output_port, 0, 0, NULL);
		} else {
			e = gf_sk_bind(ts_output_udp_sk, NULL, output_port, (char *)udp_out, output_port, GF_SOCK_REUSE_PORT);
		}
		if (e) {
			fprintf(stderr, "Error initializing UDP socket: %s\n", gf_error_to_string(e));
			goto exit;
		}
	}
	if (rtp_out != NULL) {
		ts_output_rtp = gf_rtp_new();
		gf_rtp_set_ports(ts_output_rtp, output_port);
		tr.IsUnicast = gf_sk_is_multicast_address((char *)rtp_out) ? 0 : 1;
		tr.Profile="RTP/AVP";
		tr.destination = (char *)rtp_out;
		tr.source = "0.0.0.0";
		tr.IsRecord = 0;
		tr.Append = 0;
		tr.SSRC = rand();
		tr.port_first = output_port;
		tr.port_last = output_port+1;
		if (tr.IsUnicast) {
			tr.client_port_first = output_port;
			tr.client_port_last = output_port+1;
		} else {
			tr.source = (char *)rtp_out;
		}
		res = gf_rtp_setup_transport(ts_output_rtp, &tr, (char *)ts_out);
		if (res !=0) {
			fprintf(stderr, "Cannot setup RTP transport info\n");
			goto exit;
		}
		res = gf_rtp_initialize(ts_output_rtp, 0, 1, 1500, 0, 0, NULL);
		if (res !=0) {
			fprintf(stderr, "Cannot initialize RTP sockets\n");
			goto exit;
		}
		memset(&hdr, 0, sizeof(GF_RTPHeader));
		hdr.Version = 2;
		hdr.PayloadType = 33;	/*MP2T*/
		hdr.SSRC = tr.SSRC;
		hdr.Marker = 0;
	}

	/************************************/
	/*   create streaming audio input   */
	/************************************/
	if (audio_input_ip)
	switch(audio_input_type) {
		case GF_MP42TS_UDP:
			audio_input_udp_sk = gf_sk_new(GF_SOCK_TYPE_UDP);
			if (gf_sk_is_multicast_address((char *)audio_input_ip)) {
				e = gf_sk_setup_multicast(audio_input_udp_sk, (char *)audio_input_ip, audio_input_port, 0, 0, NULL);
			} else {
				e = gf_sk_bind(audio_input_udp_sk, NULL, audio_input_port, (char *)audio_input_ip, audio_input_port, GF_SOCK_REUSE_PORT);
			}
			if (e) {
				fprintf(stdout, "Error initializing UDP socket for %s:%d : %s\n", audio_input_ip, audio_input_port, gf_error_to_string(e));
				goto exit;
			}
			gf_sk_set_buffer_size(audio_input_udp_sk, 0, UDP_BUFFER_SIZE);
			gf_sk_set_block_mode(audio_input_udp_sk, 0);

			/*allocate data buffer*/
			audio_input_buffer = malloc(audio_input_buffer_length);
			assert(audio_input_buffer);
			break;
		case GF_MP42TS_RTP:
			/*TODO: not implemented*/
			assert(0);
			break;
		case GF_MP42TS_HTTP:
			audio_prog = (void*)&progs[nb_progs-1];
			aac_download_file(aac_reader, audio_input_ip);
			break;
		case GF_MP42TS_FILE:
			assert(0); /*audio live input is restricted to realtime/streaming*/
			break;
		default:
			assert(0);
	}

	/****************************************/
	/*   declare all streams to the muxer   */
	/****************************************/
	cur_pid = 100;	/*PIDs start from 100*/
	for (i=0; i<nb_progs; i++) {
		GF_M2TS_Mux_Program *program = gf_m2ts_mux_program_add(muxer, i+1, cur_pid, progs[i].mpeg4_signaling);
		if (progs[i].mpeg4_signaling) program->iod = progs[i].iod;


		for (j=0; j<progs[i].nb_streams; j++) {
			gf_m2ts_program_stream_add(program, &progs[i].streams[j], cur_pid+j+1, (progs[i].pcr_idx==j) ? 1 : 0);
		}
		cur_pid += progs[i].nb_streams;
		while (cur_pid % 10)
			cur_pid ++;
	}

	gf_m2ts_mux_update_config(muxer, 1);

	/*****************/
	/*   main loop   */
	/*****************/
	last_print_time = gf_sys_clock();
	while (run) {
		u32 ts, status;

		/*check for some audio input from the network*/
		if (audio_input_ip) {
			u32 read;
			switch (audio_input_type) {
				case GF_MP42TS_UDP:
				case GF_MP42TS_RTP:
					/*e =*/gf_sk_receive(audio_input_udp_sk, audio_input_buffer, audio_input_buffer_length, 0, &read);
					if (read) {
						SampleCallBack((void*)&progs[nb_progs-1], AUDIO_DATA_ESID, audio_input_buffer, read, gf_sys_clock());
					}
					break;
				case GF_MP42TS_HTTP:
					/*nothing to do: AAC_OnLiveData is called automatically*/
					break;
				default:
					assert(0);
			}
		}

		/*flush all packets*/
		while ((ts_pck = gf_m2ts_mux_process(muxer, &status)) != NULL) {
			if (ts_output_file != NULL) {
				fwrite(ts_pck, 1, 188, ts_output_file); 
				if (segment_duration && (muxer->time.sec > prev_seg_time.sec + segment_duration)) {
					prev_seg_time = muxer->time;
					fclose(ts_output_file);
					segment_index++;
					sprintf(segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index);
					ts_output_file = fopen(segment_name, "wb");
					if (!ts_output_file) {
						fprintf(stderr, "Error opening %s\n", segment_name);
						goto exit;
					}
					/* delete the oldest segment */
					if (segment_number && (segment_index - segment_number >= 0)){
						char old_segment_name[GF_MAX_PATH];
						sprintf(old_segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index - segment_number);
						gf_delete_file(old_segment_name);
					}
					write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix, 
								   (segment_index >= segment_number/2 ? segment_index - segment_number/2 : 0), segment_index >1 ? segment_index-1 : 0, 0);
				} 
			}

			if (ts_output_udp_sk != NULL) {
				e = gf_sk_send(ts_output_udp_sk, (char*)ts_pck, 188); 
				if (e) {
					fprintf(stderr, "Error %s sending UDP packet\n", gf_error_to_string(e));
				}
			}
			if (ts_output_rtp != NULL) {
				hdr.SequenceNumber++;
				/*muxer clock at 90k*/
				ts = muxer->time.sec*90000 + muxer->time.nanosec*9/100000;
				/*FIXME - better discontinuity check*/
				hdr.Marker = (ts < hdr.TimeStamp) ? 1 : 0;
				hdr.TimeStamp = ts;
				e = gf_rtp_send_packet(ts_output_rtp, &hdr, (char*)ts_pck, 188, 0);
				if (e) {
					fprintf(stderr, "Error %s sending RTP packet\n", gf_error_to_string(e));
				}
			}
			if (status>=GF_M2TS_STATE_PADDING) {
				break;
			}
		}
		if (real_time) {
			/*refresh every MP42TS_PRINT_FREQ ms*/
			u32 now=gf_sys_clock();
			if (now/MP42TS_PRINT_FREQ != last_print_time/MP42TS_PRINT_FREQ) {
				last_print_time = now;
				fprintf(stderr, "M2TS: time %d - TS time %d - avg bitrate %d\r", gf_m2ts_get_sys_clock(muxer), gf_m2ts_get_ts_clock(muxer), muxer->avg_br);
			}
		}

		/*cpu load regulation*/
		gf_sleep(1);

		if (run_time) {
			if (gf_m2ts_get_ts_clock(muxer) > run_time) {
				fprintf(stderr, "Stopping multiplex at %d ms (requested runtime %d ms)\n", gf_m2ts_get_ts_clock(muxer), run_time);
				break;
			}
		}
		if (status==GF_M2TS_STATE_EOS) {
			break;
		}
	}


exit:
	run = 0;
	if (segment_duration) {
		write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix, segment_index - segment_number, segment_index, 1);
	}
	if (ts_output_file) fclose(ts_output_file);
	if (ts_output_udp_sk) gf_sk_del(ts_output_udp_sk);
	if (ts_output_rtp) gf_rtp_del(ts_output_rtp);
	if (ts_out) gf_free(ts_out);
	if (audio_input_udp_sk) gf_sk_del(audio_input_udp_sk);
	if (audio_input_buffer) free (audio_input_buffer);
	if (udp_out) gf_free(udp_out);
	if (rtp_out) gf_free(rtp_out);
	if (muxer) gf_m2ts_mux_del(muxer);
	
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
