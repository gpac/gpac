/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / M2TS demux filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>


#ifndef GPAC_DISABLE_MPEG2TS

#include <gpac/mpegts.h>
#include <gpac/thread.h>
#include <gpac/internal/media_dev.h>

typedef struct {
	char *fragment;
	u32 id;
	/*if only pid is requested*/
	u32 pid;
} GF_M2TSDmxCtx_Prog;

typedef struct {
	u32 timeline_id;
	Bool is_loc;
	u32 len;
	u8 *data;
} GF_TEMIInfo;

enum
{
	DMX_TUNE_DONE=0,
	DMX_TUNE_INIT,
	DMX_TUNE_WAIT_PROGS,
	DMX_TUNE_WAIT_SEEK,

};

typedef struct
{
	//opts
	const char *temi_url;
	Bool dsmcc, seeksrc, sigfrag, dvbtxt;

	GF_Filter *filter;
	GF_FilterPid *ipid;

	GF_M2TS_Demuxer *ts;

	GF_FilterPid *eit_pid;

	Bool is_file;
	u64 file_size;
	Bool in_seek;
	Bool initial_play_done;
	u32 nb_playing, nb_stop_pending;

	//duration estimation
	GF_Fraction64 duration;
	u64 first_pcr_found;
	u16 pcr_pid;
	u64 nb_pck_at_pcr;

	u32 map_time_on_prog_id;
	Double media_start_range;

	u32 mux_tune_state;
	u32 wait_for_progs;

	Bool is_dash;
} GF_M2TSDmxCtx;


static void m2tsdmx_estimate_duration(GF_M2TSDmxCtx *ctx, GF_M2TS_ES *stream)
{
	Bool changed;
	Double pck_dur;
	const GF_PropertyValue *p;

	if (ctx->duration.num) return;
	if (!ctx->file_size) {
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
		if (p) {
			ctx->file_size = p->value.longuint;
		} else {
			ctx->duration.num = 1;
			return;
		}
	}

	if (!ctx->first_pcr_found) {
		ctx->first_pcr_found = stream->program->last_pcr_value;
		ctx->pcr_pid = stream->pid;
		ctx->nb_pck_at_pcr = ctx->ts->pck_number;
		return;
	}
	if (ctx->pcr_pid != stream->pid) return;
	if (stream->program->last_pcr_value < ctx->first_pcr_found) {
		ctx->first_pcr_found = stream->program->last_pcr_value;
		ctx->pcr_pid = stream->pid;
		ctx->nb_pck_at_pcr = ctx->ts->pck_number;
		return;
	}
	if (stream->program->last_pcr_value - ctx->first_pcr_found <= 2*27000000)
		return;

	changed = GF_FALSE;

	pck_dur = (Double) (stream->program->last_pcr_value - ctx->first_pcr_found);
	pck_dur /= (ctx->ts->pck_number - ctx->nb_pck_at_pcr);
	pck_dur /= 27000;

	pck_dur *= ctx->file_size;
	pck_dur /= ctx->ts->prefix_present ? 192 : 188;
	if ((u32) ctx->duration.num != (u32) pck_dur) {
		ctx->duration.num = (s32) pck_dur;
		ctx->duration.den = 1000;
		changed = GF_TRUE;
	}
	ctx->first_pcr_found = stream->program->last_pcr_value;
	ctx->pcr_pid = stream->pid;
	ctx->nb_pck_at_pcr = ctx->ts->pck_number;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TSDmx] Estimated duration based on instant bitrate: %g sec\n", pck_dur/1000));

	if (changed) {
		u32 i, nb_streams = gf_filter_get_opid_count(ctx->filter);
		for (i=0; i<nb_streams; i++) {
			GF_FilterPid *opid = gf_filter_get_opid(ctx->filter, i);
			gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, &PROP_FRAC64(ctx->duration) );
		}
	}
}

static void m2tsdmx_on_event_duration_probe(GF_M2TS_Demuxer *ts, u32 evt_type, void *param)
{
	GF_Filter *filter = (GF_Filter *) ts->user;
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);

	if (evt_type == GF_M2TS_EVT_PES_PCR) {
		GF_M2TS_PES_PCK *pck = ((GF_M2TS_PES_PCK *) param);

		if (pck->stream) m2tsdmx_estimate_duration(ctx, (GF_M2TS_ES *) pck->stream);
	}
}

static void m2tsdmx_update_sdt(GF_M2TS_Demuxer *ts, void *for_pid)
{
	u32 i, count = gf_list_count(ts->programs);
	for (i=0; i<count; i++) {
		u32 j, nb_streams;
		GF_M2TS_Program *prog = gf_list_get(ts->programs, i);
		GF_M2TS_SDT *sdt = gf_m2ts_get_sdt_info(ts, prog->number);
		if (!sdt) continue;

		nb_streams = gf_list_count(prog->streams);
		for (j=0; j<nb_streams; j++) {
			GF_M2TS_ES *es = gf_list_get(prog->streams, j);
			if (!es->user) continue;
			if (for_pid && (es->user != for_pid)) continue;
			//TODO, translate non standard character maps to UTF8
			//we for now comment in test mode to avoid non UTF characters in text dumps
			if (isalnum(sdt->service[0]) || !gf_sys_is_test_mode())
				gf_filter_pid_set_property((GF_FilterPid *)es->user, GF_PROP_PID_SERVICE_NAME, &PROP_STRING(sdt->service ) );

			if (isalnum(sdt->provider[0]) || !gf_sys_is_test_mode())
				gf_filter_pid_set_property((GF_FilterPid *)es->user, GF_PROP_PID_SERVICE_PROVIDER, &PROP_STRING( sdt->provider ) );
		}
	}
}

static void m2tsdmx_declare_pid(GF_M2TSDmxCtx *ctx, GF_M2TS_PES *stream, GF_ESD *esd)
{
	u32 i, count, codecid=0, stype=0, orig_stype=0;
	GF_FilterPid *opid;
	Bool m4sys_stream = GF_FALSE;
	Bool m4sys_iod_stream = GF_FALSE;
	Bool has_scal_layer = GF_FALSE;
	Bool unframed = GF_FALSE;
	Bool unframed_latm = GF_FALSE;
	char szName[20];
	const char *stname;
	if (stream->user) return;

	if (stream->flags & GF_M2TS_GPAC_CODEC_ID) {
		codecid = stream->stream_type;
		stype = gf_codecid_type(codecid);
		if (stream->gpac_meta_dsi)
			stype = stream->gpac_meta_dsi[4];
		if (!stype) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSDmx] Unrecognized gpac codec %s - ignoring pid\n", gf_4cc_to_str(codecid) ));
			return;
		}
	} else {
		switch (stream->stream_type) {
		case GF_M2TS_VIDEO_MPEG1:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_MPEG1;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_MPEG2:
		case GF_M2TS_VIDEO_DCII:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_MPEG2_MAIN;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_MPEG4:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_MPEG4_PART2;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_H264:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_AVC;
			unframed = GF_TRUE;
			if (stream->program->is_scalable)
				has_scal_layer = GF_TRUE;
			break;
		case GF_M2TS_HLS_AVC_CRYPT:
			stype = GF_STREAM_ENCRYPTED;
			orig_stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_AVC;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_SVC:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_SVC;
			has_scal_layer = GF_TRUE;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_HEVC:
		case GF_M2TS_VIDEO_HEVC_TEMPORAL:
		case GF_M2TS_VIDEO_HEVC_MCTS:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_HEVC;
			unframed = GF_TRUE;
			if (stream->program->is_scalable)
				has_scal_layer = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_SHVC:
		case GF_M2TS_VIDEO_SHVC_TEMPORAL:
		case GF_M2TS_VIDEO_MHVC:
		case GF_M2TS_VIDEO_MHVC_TEMPORAL:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_LHVC;
			has_scal_layer = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_VVC:
		case GF_M2TS_VIDEO_VVC_TEMPORAL:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_VVC;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_VIDEO_VC1:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_SMPTE_VC1;
			stream->flags |= GF_M2TS_CHECK_VC1;
			break;
		case GF_M2TS_VIDEO_AV1:
			stype = GF_STREAM_VISUAL;
			codecid = GF_CODECID_AV1;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_MPEG1:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_MPEG_AUDIO;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_MPEG2:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_MPEG2_PART3;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_AAC:
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_AAC_MPEG4;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_LATM_AAC:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_AAC_MPEG4;
			unframed = GF_TRUE;
			unframed_latm = GF_TRUE;
			break;

		case GF_M2TS_MHAS_MAIN:
		case GF_M2TS_MHAS_AUX:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_MHAS;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_AC3:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_AC3;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_EC3:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_EAC3;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_TRUEHD:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_TRUEHD;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_AUDIO_DTS:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_DTS_X;
			break;
		case GF_M2TS_AUDIO_OPUS:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_OPUS;
			break;
		case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
			((GF_M2TS_ES*)stream)->flags |= GF_M2TS_ES_SEND_REPEATED_SECTIONS;
			//fallthrough
		case GF_M2TS_SYSTEMS_MPEG4_PES:
			if (!esd) {
				m4sys_iod_stream = GF_TRUE;
				count = stream->program->pmt_iod ? gf_list_count(stream->program->pmt_iod->ESDescriptors) : 0;
				for (i=0; i<count; i++) {
					esd = gf_list_get(stream->program->pmt_iod->ESDescriptors, i);
					if (esd->ESID == stream->mpeg4_es_id) break;
					esd = NULL;
				}
			}
			m4sys_stream = GF_TRUE;
			//cannot setup stream yet
			if (!esd) return;
			break;
		case GF_M2TS_METADATA_PES:
		case GF_M2TS_METADATA_ID3_HLS:
			stype = GF_STREAM_METADATA;
			codecid = GF_CODECID_SIMPLE_TEXT;
			break;
		case 0xA1:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_EAC3;
			break;

		case GF_M2TS_HLS_AAC_CRYPT:
			stype = GF_STREAM_ENCRYPTED;
			orig_stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_AAC_MPEG4;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_HLS_AC3_CRYPT:
			stype = GF_STREAM_ENCRYPTED;
			orig_stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_AC3;
			unframed = GF_TRUE;
			break;
		case GF_M2TS_HLS_EC3_CRYPT:
			stype = GF_STREAM_ENCRYPTED;
			orig_stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_EAC3;
			break;
		case GF_M2TS_DVB_SUBTITLE:
			stype = GF_STREAM_TEXT;
			codecid = GF_CODECID_DVB_SUBS;
			stream->flags |= GF_M2TS_ES_FULL_AU;
			break;
		case GF_M2TS_DVB_TELETEXT:
			if (!ctx->dvbtxt) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSDmx] DVB teletext pid skipped, use --dvbtxt to enable\n", stream->stream_type));
				return;
			}
			stype = GF_STREAM_TEXT;
			codecid = GF_CODECID_DVB_TELETEXT;
			stream->flags |= GF_M2TS_ES_FULL_AU;
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSDmx] Stream type 0x%02X not supported - ignoring pid\n", stream->stream_type));
			return;
		}
	}

	opid = NULL;
	for (i=0; i<gf_filter_get_opid_count(ctx->filter); i++) {
		opid = gf_filter_get_opid(ctx->filter, i);
		const GF_PropertyValue *p = gf_filter_pid_get_property(opid, GF_PROP_PID_ID);
		if (p && (p->value.uint == stream->pid))
			break;
		opid = NULL;
	}

	if (!opid)
		opid = gf_filter_pid_new(ctx->filter);

	stream->user = opid;
	stream->flags |= GF_M2TS_ES_ALREADY_DECLARED;

	u32 d_type = orig_stype ? orig_stype : stype;
	switch (d_type) {
	case GF_STREAM_AUDIO:
	case GF_STREAM_VISUAL:
		stream->flags |= GF_M2TS_CHECK_DISC;
		break;
	default:
		stream->flags &= ~GF_M2TS_CHECK_DISC;
		break;
	}

	stname = gf_stream_type_name(stype);
	sprintf(szName, "P%d%c%d", stream->program->number, stname[0], 1+gf_list_find(stream->program->streams, stream));
	gf_filter_pid_set_name(opid, szName);

	gf_filter_pid_set_property(opid, GF_PROP_PID_ID, &PROP_UINT(stream->pid) );
	gf_filter_pid_set_property(opid, GF_PROP_PID_ESID, stream->mpeg4_es_id ? &PROP_UINT(stream->mpeg4_es_id) : NULL);

	if (m4sys_stream) {
		if (stream->slcfg) gf_free(stream->slcfg);

		stream->slcfg = esd->slConfig;
		esd->slConfig = NULL;

		gf_filter_pid_set_property(opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(esd->decoderConfig ? esd->decoderConfig->streamType : GF_STREAM_SCENE) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_CODECID, &PROP_UINT(esd->decoderConfig ? esd->decoderConfig->objectTypeIndication : GF_CODECID_BIFS) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(esd->OCRESID ? esd->OCRESID : esd->ESID) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(esd->dependsOnESID) );
		if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->dataLength)
			gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength) );

		gf_filter_pid_set_property(opid, GF_PROP_PID_IN_IOD, &PROP_BOOL(m4sys_iod_stream) );

		gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(((GF_M2TS_ES*)stream)->slcfg->timestampResolution) );
		if (esd->decoderConfig && (esd->decoderConfig->streamType==GF_STREAM_OD))
			stream->flags |= GF_M2TS_ES_IS_MPEG4_OD;
	} else {
		gf_filter_pid_set_property(opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(stype) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_CODECID, &PROP_UINT(codecid) );

		gf_filter_pid_set_property(opid, GF_PROP_PID_UNFRAMED, unframed ? &PROP_BOOL(GF_TRUE) : NULL);
		gf_filter_pid_set_property(opid, GF_PROP_PID_UNFRAMED_LATM, unframed_latm ? &PROP_BOOL(GF_TRUE) : NULL );

		if (orig_stype) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_ORIG_STREAM_TYPE, &PROP_UINT(orig_stype) );
			gf_filter_pid_set_property(opid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, &PROP_UINT(GF_HLS_SAMPLE_AES_SCHEME) );
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_ORIG_STREAM_TYPE, NULL);
			gf_filter_pid_set_property(opid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, NULL);
		}

		gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(stream->program->pcr_pid) );

		if ((stream->flags&GF_M2TS_ES_IS_PES) && stream->gpac_meta_dsi) {
			char *cname;
			GF_BitStream *bs = gf_bs_new(stream->gpac_meta_dsi, stream->gpac_meta_dsi_size, GF_BITSTREAM_READ);
			u32 val = gf_bs_read_u32(bs); //codec ID (meta codec identifier)
			gf_filter_pid_set_property(opid, GF_PROP_PID_CODECID, &PROP_UINT(val) );
			gf_bs_read_u8(bs); //stream type
			gf_bs_read_u8(bs); //version
			val = gf_bs_read_u32(bs); //codecID for meta codec, e.g. an AVCodecID or other
			gf_filter_pid_set_property(opid, GF_PROP_PID_META_DEMUX_CODEC_ID, &PROP_UINT(val) );
			cname = gf_bs_read_utf8(bs); //meta codec name
			gf_filter_pid_set_property(opid, GF_PROP_PID_META_DEMUX_CODEC_NAME, cname ? &PROP_STRING_NO_COPY(cname) : NULL );
			val = gf_bs_read_u32(bs); //meta opaque
			gf_filter_pid_set_property(opid, GF_PROP_PID_META_DEMUX_OPAQUE, &PROP_UINT(val) );
			u32 dsi_len = gf_bs_read_u32(bs);
			if (dsi_len) {
				u32 pos = (u32) gf_bs_get_position(bs);
				gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(stream->gpac_meta_dsi+pos, dsi_len) );
				gf_bs_skip_bytes(bs, dsi_len);
			} else {
				gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, NULL);
			}
			if (stype==GF_STREAM_VISUAL) {
				val = gf_bs_read_u32(bs);
				gf_filter_pid_set_property(opid, GF_PROP_PID_WIDTH, &PROP_UINT(val) );
				val = gf_bs_read_u32(bs);
				gf_filter_pid_set_property(opid, GF_PROP_PID_HEIGHT, &PROP_UINT(val) );
			} else if (stype==GF_STREAM_AUDIO) {
				val = gf_bs_read_u32(bs);
				gf_filter_pid_set_property(opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(val) );
				val = gf_bs_read_u32(bs);
				gf_filter_pid_set_property(opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(val) );
			}
			gf_bs_del(bs);
		}
	}
	gf_filter_pid_set_property(opid, GF_PROP_PID_SCALABLE, has_scal_layer ? &PROP_BOOL(GF_TRUE) : NULL);

	gf_filter_pid_set_property(opid, GF_PROP_PID_SERVICE_ID, &PROP_UINT(stream->program->number) );

	if ((stream->flags&GF_M2TS_ES_IS_PES) && stream->lang) {
		char szLang[4];
		szLang[0] = (stream->lang>>16) & 0xFF;
		szLang[1] = (stream->lang>>8) & 0xFF;
		szLang[2] = stream->lang & 0xFF;
		szLang[3] = 0;
		if (szLang[2]==' ') szLang[2] = 0;
		gf_filter_pid_set_property(opid, GF_PROP_PID_LANGUAGE, &PROP_STRING(szLang) );
	}
	if (codecid == GF_CODECID_DVB_SUBS) {
		char szLang[4];
		memcpy(szLang, stream->sub.language, 3);
		szLang[3]=0;
		gf_filter_pid_set_property(opid, GF_PROP_PID_LANGUAGE, &PROP_STRING(szLang) );

		u8 dsi[5];
		dsi[0] = stream->sub.composition_page_id>>8;
		dsi[1] = stream->sub.composition_page_id & 0xFF;
		dsi[2] = stream->sub.ancillary_page_id>>8;
		dsi[3] = stream->sub.ancillary_page_id & 0xFF;
		dsi[4] = stream->sub.type;
		gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi, 5));
	}

	if (ctx->duration.num>1) {
		gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, &PROP_FRAC64(ctx->duration) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD ) );
	}
	/*indicate our coding dependencies if any*/
	if (!m4sys_stream) {
		if ((stream->flags&GF_M2TS_ES_IS_PES) && stream->depends_on_pid) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(stream->depends_on_pid) );
			if ((stream->stream_type == GF_M2TS_VIDEO_HEVC_TEMPORAL) || (stream->stream_type == GF_M2TS_VIDEO_HEVC_MCTS)) {
				gf_filter_pid_set_property(opid, GF_PROP_PID_SUBLAYER, &PROP_BOOL(GF_TRUE) );
			}
		} else {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DEPENDENCY_ID, NULL);
			gf_filter_pid_set_property(opid, GF_PROP_PID_SUBLAYER, NULL);
		}
	}

	if ((stream->flags & GF_M2TS_ES_IS_PES) && (stream->dv_info[0])) {
		gf_filter_pid_set_property(opid, GF_PROP_PID_DOLBY_VISION, &PROP_DATA(stream->dv_info, 24) );
		u32 dvtype=0;
		if (stream->dv_info[24]) {
			if (stream->stream_type == GF_M2TS_VIDEO_H264)
				dvtype = GF_4CC('d','a','v','1');
			else
				dvtype = GF_4CC('d','v','h','1');
		}
		gf_filter_pid_set_property(opid, GF_PROP_PID_ISOM_SUBTYPE, dvtype ? &PROP_4CC(dvtype) : NULL);
	} else {
		gf_filter_pid_set_property(opid, GF_PROP_PID_DOLBY_VISION, NULL);
	}

	m2tsdmx_update_sdt(ctx->ts, opid);

	gf_m2ts_set_pes_framing((GF_M2TS_PES *)stream, GF_M2TS_PES_FRAMING_DEFAULT);
}

static void m2tsdmx_setup_program(GF_M2TSDmxCtx *ctx, GF_M2TS_Program *prog)
{
	u32 i, count;

	count = gf_list_count(prog->streams);
	for (i=0; i<count; i++) {
		GF_M2TS_PES *es = gf_list_get(prog->streams, i);
		if (es->pid==prog->pmt_pid) continue;
		if (! (es->flags & GF_M2TS_ES_IS_PES)) continue;

		if (es->stream_type == GF_M2TS_VIDEO_HEVC_TEMPORAL ) continue;
		if (es->depends_on_pid ) {
			prog->is_scalable = GF_TRUE;
			break;
		}
	}

	for (i=0; i<count; i++) {
		u32 ncount;
		GF_M2TS_ES *es = gf_list_get(prog->streams, i);
		if (es->pid==prog->pmt_pid) continue;

		if (! (es->flags & GF_M2TS_ES_ALREADY_DECLARED)) {
			m2tsdmx_declare_pid(ctx, (GF_M2TS_PES *)es, NULL);
		}
		ncount = gf_list_count(prog->streams);
		while (ncount<count) {
			i--;
			count--;
		}
	}
}

static void m2tdmx_merge_temi(GF_FilterPid *pid, GF_M2TS_ES *stream, GF_FilterPacket *pck)
{
	if (stream->props) {
		char szID[100];
		while (gf_list_count(stream->props)) {
			GF_TEMIInfo *t = gf_list_pop_front(stream->props);
			snprintf(szID, 100, "%s:%d", t->is_loc ? "temi_l" : "temi_t", t->timeline_id);

			gf_filter_pck_set_property_dyn(pck, szID, &PROP_DATA_NO_COPY(t->data, t->len));
			gf_free(t);
		}
		gf_list_del(stream->props);
		stream->props = NULL;

		if (!(stream->flags & GF_M2TS_ES_TEMI_INFO)) {
			stream->flags |= GF_M2TS_ES_TEMI_INFO;
			gf_filter_pid_set_property(pid, GF_PROP_PID_HAS_TEMI, &PROP_BOOL(GF_TRUE) );
		}
		
	}
}

static void m2tsdmx_send_packet(GF_M2TSDmxCtx *ctx, GF_M2TS_PES_PCK *pck)
{
	GF_FilterPid *opid;
	GF_FilterPacket *dst_pck;
	u8 * data;
	//we don't have end of frame signaling by default
	Bool au_end = GF_FALSE;
	GF_FilterSAPType sap_type = GF_FILTER_SAP_NONE;

	/*pcr not initialized, don't send any data*/
//	if (! pck->stream->program->first_dts) return;
	if (!pck->stream->user) return;
	opid = pck->stream->user;

	u8 *ptr = pck->data;
	u32 len = pck->data_len;

	//skip dataID and stream ID
	if (pck->stream->stream_type==GF_M2TS_DVB_SUBTITLE) {
		ptr+=2;
		len-=2;
	}
	//for now GF_M2TS_ES_FULL_AU is only used for text, all rap
	if (pck->stream->flags & GF_M2TS_ES_FULL_AU) {
		au_end = GF_TRUE;
		sap_type = GF_FILTER_SAP_1;
	}

	if (pck->stream->flags & GF_M2TS_CHECK_VC1) {
		//extract seq header
		u32 start, next, sc_size, sc_size2, sc_size3, hdr_len=0;

		start = next = gf_media_nalu_next_start_code(ptr, len, &sc_size);
		if ((next<len) && (ptr[next+sc_size]==0x0F)) {
			u32 ephdr = gf_media_nalu_next_start_code (ptr+next+sc_size, len-next-sc_size, &sc_size2);
			if ((ephdr + next + sc_size < len) && (ptr[next+sc_size+ephdr+sc_size2]==0x0E)) {
				u32 end = gf_media_nalu_next_start_code (ptr+next+sc_size+ephdr+sc_size2, len-next-sc_size-ephdr-sc_size2, &sc_size3);
				if (end + ephdr + next + sc_size + sc_size2 < len)
					hdr_len = end + ephdr + sc_size2 + next + sc_size;
			} else if ((ephdr + next + sc_size < len) && (ptr[next+sc_size+ephdr+sc_size2]==0x0D)) {
				hdr_len = ephdr + next + sc_size;
			}
		}
		if (hdr_len) {
			u8 *dsi=NULL;
			u32 dsi_len;
			ptr += start;
			len -= start;
			gf_media_vc1_seq_header_to_dsi(ptr, len, &dsi, &dsi_len);
			if (dsi)
				gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_len));

			ptr += hdr_len;
			len -= hdr_len;
			pck->stream->flags &= ~GF_M2TS_CHECK_VC1;
		}
	}


	dst_pck = gf_filter_pck_new_alloc(opid, len, &data);
	if (!dst_pck) return;
	memcpy(data, ptr, len);

	gf_filter_pck_set_framing(dst_pck, (pck->flags & GF_M2TS_PES_PCK_AU_START) ? GF_TRUE : GF_FALSE, au_end);

	if (pck->flags & GF_M2TS_PES_PCK_AU_START) {
		if (pck->flags & GF_M2TS_PES_PCK_RAP)
			sap_type = GF_FILTER_SAP_1;

		gf_filter_pck_set_cts(dst_pck, pck->PTS);
		if (pck->DTS != pck->PTS) {
			gf_filter_pck_set_dts(dst_pck, pck->DTS);
		}
		gf_filter_pck_set_sap(dst_pck, sap_type);

		if (pck->stream->flags & GF_M2TS_ES_IS_PES) {
			GF_M2TS_PES *pes = (GF_M2TS_PES *)pck->stream;
			if (pes->map_utc) {
				s64 diff = pck->PTS;
				diff -= pes->map_utc_pcr;
				diff = gf_timestamp_rescale_signed(diff, 90000, 1000);
				gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_UTC_TIME, &PROP_LONGUINT(pes->map_utc+diff) );
				pes->map_utc=0;
			}
			if (pes->map_pcr) {
				Double diff = (Double) pck->PTS;
				diff -= pes->map_pcr;
				diff /= 90000;
				gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_MEDIA_TIME, &PROP_DOUBLE(ctx->media_start_range+diff) );
				pes->map_pcr=0;
			}
		}
	}
	m2tdmx_merge_temi(opid, (GF_M2TS_ES *)pck->stream, dst_pck);

	if (pck->stream->is_seg_start) {
		pck->stream->is_seg_start = GF_FALSE;
		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
	}

	gf_filter_pck_send(dst_pck);
	ctx->nb_stop_pending = 0;
}

static GF_M2TS_ES *m2tsdmx_get_m4sys_stream(GF_M2TSDmxCtx *ctx, u32 m4sys_es_id)
{
	u32 i, j, count, count2;
	count = gf_list_count(ctx->ts->programs);
	for (i=0; i<count; i++) {
		GF_M2TS_Program *prog = gf_list_get(ctx->ts->programs, i);
		count2 = gf_list_count(prog->streams);
		for (j=0; j<count2; j++) {
			GF_M2TS_ES *pes = (GF_M2TS_ES *)gf_list_get(prog->streams, j);
			if (pes->mpeg4_es_id == m4sys_es_id) return pes;
		}
	}
	return NULL;
}
static GFINLINE void m2tsdmx_send_sl_packet(GF_M2TSDmxCtx *ctx, GF_M2TS_SL_PCK *pck)
{
	GF_SLConfig *slc = ((GF_M2TS_ES*)pck->stream)->slcfg;
	GF_FilterPid *opid;
	GF_FilterPacket *dst_pck;
	u8 * data;
	Bool start, end;
	GF_SLHeader slh;
	u32 slh_len = 0;

	if (!pck->stream->user) return;
	opid = pck->stream->user;

	/*depacketize SL Header*/
	if (((GF_M2TS_ES*)pck->stream)->slcfg) {
		gf_sl_depacketize(slc, &slh, pck->data, pck->data_len, &slh_len);
		slh.m2ts_version_number_plus_one = pck->version_number + 1;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDmx] MPEG-4 SL-packetized stream without SLConfig assigned - ignoring packet\n") );
		return;
	}

	dst_pck = gf_filter_pck_new_alloc(opid, pck->data_len - slh_len, &data);
	if (!dst_pck) return;

	memcpy(data, pck->data + slh_len, pck->data_len - slh_len);
	start = end = GF_FALSE;
	if (slc->useAccessUnitStartFlag && slh.accessUnitStartFlag) start = GF_TRUE;
	if (slc->useAccessUnitEndFlag && slh.accessUnitEndFlag) end = GF_TRUE;
	gf_filter_pck_set_framing(dst_pck, start, end);

	if (slc->useTimestampsFlag && slh.decodingTimeStampFlag)
		gf_filter_pck_set_dts(dst_pck, slh.decodingTimeStamp);

	if (slc->useTimestampsFlag && slh.compositionTimeStampFlag)
		gf_filter_pck_set_cts(dst_pck, slh.compositionTimeStamp);

	if (slc->hasRandomAccessUnitsOnlyFlag || slh.randomAccessPointFlag)
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);

	gf_filter_pck_set_carousel_version(dst_pck, pck->version_number);

	m2tdmx_merge_temi(opid, pck->stream, dst_pck);
	if (pck->stream->is_seg_start) {
		pck->stream->is_seg_start = GF_FALSE;
		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
	}
	gf_filter_pck_send(dst_pck);

	if (pck->version_number + 1 == pck->stream->slcfg->carousel_version)
		return;
	pck->stream->slcfg->carousel_version = 1 + pck->version_number;


	if (pck->stream->flags & GF_M2TS_ES_IS_MPEG4_OD) {
		/* We need to decode OD streams to get the SL config for other streams :( */
		GF_ESD *esd;
		GF_ODUpdate* odU;
		GF_ESDUpdate* esdU;
		u32 com_count, com_index, od_count, od_index, esd_index;
		GF_ODCodec *od_codec = gf_odf_codec_new();

		gf_odf_codec_set_au(od_codec, pck->data + slh_len, pck->data_len - slh_len);
		gf_odf_codec_decode(od_codec);
		com_count = gf_list_count(od_codec->CommandList);
		for (com_index = 0; com_index < com_count; com_index++) {
			GF_ODCom *com;
			com = (GF_ODCom *)gf_list_get(od_codec->CommandList, com_index);
			switch (com->tag) {
			case GF_ODF_OD_UPDATE_TAG:
				odU = (GF_ODUpdate*)com;
				od_count = gf_list_count(odU->objectDescriptors);
				for (od_index=0; od_index<od_count; od_index++) {
					GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, od_index);
					esd_index = 0;
					while ( (esd = gf_list_enum(od->ESDescriptors, &esd_index)) ) {
						GF_M2TS_ES *es = m2tsdmx_get_m4sys_stream(ctx, esd->ESID);

						if (es && ! (es->flags & GF_M2TS_ES_ALREADY_DECLARED)) {
							m2tsdmx_declare_pid(ctx, (GF_M2TS_PES *)es, esd);
						}
					}
				}
				break;
			case GF_ODF_ESD_UPDATE_TAG:
				esdU = (GF_ESDUpdate*)com;
				esd_index = 0;
				while ( (esd = gf_list_enum(esdU->ESDescriptors, &esd_index)) ) {
					GF_M2TS_ES *es = m2tsdmx_get_m4sys_stream(ctx, esd->ESID);
						if (es && ! (es->flags & GF_M2TS_ES_ALREADY_DECLARED)) {
							m2tsdmx_declare_pid(ctx, (GF_M2TS_PES *)es, esd);
						}
				}
				break;
			}
		}
		gf_odf_codec_del(od_codec);
	}
}

#if 0  //unused
static void m2tsdmx_declare_epg_pid(GF_M2TSDmxCtx *ctx)
{
	assert(ctx->eit_pid == NULL);
	ctx->eit_pid = gf_filter_pid_new(ctx->filter);
	gf_filter_pid_set_property(ctx->eit_pid, GF_PROP_PID_ID, &PROP_UINT(GF_M2TS_PID_EIT_ST_CIT) );
	gf_filter_pid_set_property(ctx->eit_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_PRIVATE_SCENE) );
	gf_filter_pid_set_property(ctx->eit_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_DVB_EIT) );
	gf_filter_pid_set_property(ctx->eit_pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000) );
	gf_filter_pid_set_property(ctx->eit_pid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(GF_M2TS_PID_EIT_ST_CIT) );
}
#endif


static void m2tsdmx_on_event(GF_M2TS_Demuxer *ts, u32 evt_type, void *param)
{
	u32 i, count;
	GF_Filter *filter = (GF_Filter *) ts->user;
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt_type) {
	case GF_M2TS_EVT_PAT_UPDATE:
		break;
	case GF_M2TS_EVT_AIT_FOUND:
		break;
	case GF_M2TS_EVT_PAT_FOUND:
		if (ctx->mux_tune_state==DMX_TUNE_INIT) {
			ctx->mux_tune_state = DMX_TUNE_WAIT_PROGS;
			ctx->wait_for_progs = gf_list_count(ts->programs);
		}
		break;
	case GF_M2TS_EVT_DSMCC_FOUND:
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		m2tsdmx_setup_program(ctx, param);
		if (ctx->mux_tune_state == DMX_TUNE_WAIT_PROGS) {
			assert(ctx->wait_for_progs);
			ctx->wait_for_progs--;
			if (!ctx->wait_for_progs) {
				ctx->mux_tune_state = DMX_TUNE_WAIT_SEEK;
			}
		}
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		m2tsdmx_setup_program(ctx, param);
		break;

	case GF_M2TS_EVT_SDT_FOUND:
	case GF_M2TS_EVT_SDT_UPDATE:
//	case GF_M2TS_EVT_SDT_REPEAT:
		m2tsdmx_update_sdt(ts, NULL);
		break;
	case GF_M2TS_EVT_DVB_GENERAL:
		if (ctx->eit_pid) {
			GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)param;
			u8 *data;
			GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(ctx->eit_pid, pck->data_len, &data);
			if (dst_pck) {
				memcpy(data, pck->data, pck->data_len);
				gf_filter_pck_send(dst_pck);
			}
		}
		break;
	case GF_M2TS_EVT_PES_PCK:
		if (ctx->mux_tune_state) break;
		m2tsdmx_send_packet(ctx, param);
		break;
	case GF_M2TS_EVT_SL_PCK: /* DMB specific */
		if (ctx->mux_tune_state) break;
		m2tsdmx_send_sl_packet(ctx, param);
		break;
	case GF_M2TS_EVT_PES_PCR:
		if (ctx->mux_tune_state) break;
	{
		u64 pcr;
		Bool map_time = GF_FALSE;
		GF_M2TS_PES_PCK *pck = ((GF_M2TS_PES_PCK *) param);
		Bool discontinuity = ( ((GF_M2TS_PES_PCK *) param)->flags & GF_M2TS_PES_PCK_DISCONTINUITY) ? 1 : 0;

		assert(pck->stream);
		m2tsdmx_estimate_duration(ctx, (GF_M2TS_ES *) pck->stream);

		if (ctx->map_time_on_prog_id && (ctx->map_time_on_prog_id==pck->stream->program->number)) {
			map_time = GF_TRUE;
		}

		//we forward the PCR on each pid
		pcr = ((GF_M2TS_PES_PCK *) param)->PTS;
		pcr /= 300;
		count = gf_list_count(pck->stream->program->streams);
		for (i=0; i<count; i++) {
			GF_FilterPacket *dst_pck;
			GF_M2TS_PES *stream = gf_list_get(pck->stream->program->streams, i);
			if (!stream->user) continue;

			dst_pck = gf_filter_pck_new_shared(stream->user, NULL, 0, NULL);
			if (!dst_pck) continue;

			gf_filter_pck_set_cts(dst_pck, pcr);
			gf_filter_pck_set_clock_type(dst_pck, discontinuity ? GF_FILTER_CLOCK_PCR_DISC : GF_FILTER_CLOCK_PCR);
			if (pck->stream->is_seg_start) {
				pck->stream->is_seg_start = GF_FALSE;
				gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));
			}
			gf_filter_pck_send(dst_pck);

			if (map_time && (stream->flags & GF_M2TS_ES_IS_PES) ) {
				((GF_M2TS_PES*)stream)->map_pcr = pcr;
			}
		}

		if (map_time) {
			ctx->map_time_on_prog_id = 0;
		}
	}
		break;

	case GF_M2TS_EVT_TDT:
		if (ctx->mux_tune_state) break;
	{
		GF_M2TS_TDT_TOT *tdt = (GF_M2TS_TDT_TOT *)param;
		u64 utc_ts = gf_net_get_utc_ts(tdt->year, tdt->month, tdt->day, tdt->hour, tdt->minute, tdt->second);
		count = gf_list_count(ts->programs );
		for (i=0; i<count; i++) {
			GF_M2TS_Program *prog = gf_list_get(ts->programs, i);
			u32 j, count2 = gf_list_count(prog->streams);
			for (j=0; j<count2; j++) {
				GF_M2TS_ES * stream = gf_list_get(prog->streams, j);
				if (stream->user && (stream->flags & GF_M2TS_ES_IS_PES)) {
					GF_M2TS_PES*pes = (GF_M2TS_PES*)stream;
					pes->map_utc = utc_ts;
					pes->map_utc_pcr = prog->last_pcr_value/300;
				}
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TS In] Mapping TDT Time %04d-%02d-%02dT%02d:%02d:%02d and PCR time "LLD" on program %d\n",
				                                       tdt->year, tdt->month+1, tdt->day, tdt->hour, tdt->minute, tdt->second, prog->last_pcr_value/300, prog->number));
		}
	}
		break;
	case GF_M2TS_EVT_TOT:
		break;

	case GF_M2TS_EVT_DURATION_ESTIMATED:
	{
		u64 duration = ((GF_M2TS_PES_PCK *) param)->PTS;
		count = gf_list_count(ts->programs);
		for (i=0; i<count; i++) {
			GF_M2TS_Program *prog = gf_list_get(ts->programs, i);
			u32 j, count2;
			count2 = gf_list_count(prog->streams);
			for (j=0; j<count2; j++) {
				GF_M2TS_ES * stream = gf_list_get(prog->streams, j);
				if (stream->user) {
					gf_filter_pid_set_property(stream->user, GF_PROP_PID_DURATION, & PROP_FRAC64_INT(duration, 1000) );
				}
			}
		}
	}
	break;

	case GF_M2TS_EVT_TEMI_LOCATION:
	{
		GF_M2TS_TemiLocationDescriptor *temi_l = (GF_M2TS_TemiLocationDescriptor *)param;
		const char *url;
		u32 len;
		GF_BitStream *bs;
		GF_M2TS_ES *es=NULL;
		GF_TEMIInfo *t;
		if ((temi_l->pid<8192) && (ctx->ts->ess[temi_l->pid])) {
			es = ctx->ts->ess[temi_l->pid];
		}
		if (!es || !es->user) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TSDmx] TEMI location not assigned to a given PID, not supported\n"));
			break;
		}
		GF_SAFEALLOC(t, GF_TEMIInfo);
		if (!t) break;
		t->timeline_id = temi_l->timeline_id;
		t->is_loc = GF_TRUE;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		if (ctx->temi_url)
			url = ctx->temi_url;
		else
			url = temi_l->external_URL;
		len = url ? (u32) strlen(url) : 0;
		gf_bs_write_data(bs, url, len);
		gf_bs_write_u8(bs, 0);
		gf_bs_write_int(bs, temi_l->is_announce, 1);
		gf_bs_write_int(bs, temi_l->is_splicing, 1);
		gf_bs_write_int(bs, temi_l->reload_external, 1);
		gf_bs_write_int(bs, 0, 5);
		if (temi_l->is_announce) {
			gf_bs_write_u32(bs, temi_l->activation_countdown.den);
			gf_bs_write_u32(bs, temi_l->activation_countdown.num);
		}
		gf_bs_get_content(bs, &t->data, &t->len);
		gf_bs_del(bs);

		if (!es->props) {
			es->props = gf_list_new();
		}
		gf_list_add(es->props, t);
	}
	break;
	case GF_M2TS_EVT_TEMI_TIMECODE:
	{
		GF_M2TS_TemiTimecodeDescriptor *temi_t = (GF_M2TS_TemiTimecodeDescriptor*)param;
		GF_BitStream *bs;
		GF_TEMIInfo *t;
		GF_M2TS_ES *es=NULL;
		if ((temi_t->pid<8192) && (ctx->ts->ess[temi_t->pid])) {
			es = ctx->ts->ess[temi_t->pid];
		}
		if (!es || !es->user) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TSDmx] TEMI timing not assigned to a given PID, not supported\n"));
			break;
		}
		GF_SAFEALLOC(t, GF_TEMIInfo);
		if (!t) break;
		t->timeline_id = temi_t->timeline_id;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u32(bs, temi_t->media_timescale);
		gf_bs_write_u64(bs, temi_t->media_timestamp);
		gf_bs_write_u64(bs, temi_t->pes_pts);
		gf_bs_write_int(bs, temi_t->force_reload, 1);
		gf_bs_write_int(bs, temi_t->is_paused, 1);
		gf_bs_write_int(bs, temi_t->is_discontinuity, 1);
		gf_bs_write_int(bs, temi_t->ntp ? 1 : 0, 1);
		gf_bs_write_int(bs, 0, 4);
		if (temi_t->ntp)
			gf_bs_write_u64(bs, temi_t->ntp);

		gf_bs_get_content(bs, &t->data, &t->len);
		gf_bs_del(bs);

		if (!es->props) {
			es->props = gf_list_new();
		}
		gf_list_add(es->props, t);
	}
	break;
	case GF_M2TS_EVT_STREAM_REMOVED:
	{
		GF_M2TS_ES *es = (GF_M2TS_ES *)param;
		if (es && es->props) {
			while (gf_list_count(es->props)) {
				GF_TEMIInfo *t = gf_list_pop_back(es->props);
				gf_free(t->data);
				gf_free(t);
			}
			gf_list_del(es->props);
		}
	}
		break;
	}
}

static GF_Err m2tsdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;;
	FILE *stream = NULL;
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		u32 i, count = gf_filter_get_opid_count(filter);
		for (i=0; i<count; i++) {
			GF_FilterPid *opid = gf_filter_get_opid(filter, i);
			gf_filter_pid_remove( opid);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	//by default for all URLs, send packets as soon as the program is configured
	ctx->mux_tune_state = DMX_TUNE_DONE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (p && p->value.string && !ctx->duration.num && strncmp(p->value.string, "gmem://", 7)) {
		stream = gf_fopen(p->value.string, "rb");
	}

	if (stream) {
		if (ctx->seeksrc) {
			//for local file we will send a seek and stop once all programs are configured, and reparse from start
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
			if (p && p->value.string && gf_file_exists(p->value.string)) {
				ctx->mux_tune_state = DMX_TUNE_INIT;
			}
		}
		
		ctx->ipid = pid;
		ctx->is_file = GF_TRUE;
		ctx->ts->seek_mode = GF_TRUE;
		ctx->ts->on_event = m2tsdmx_on_event_duration_probe;
		while (!gf_feof(stream)) {
			char buf[1880];
			u32 nb_read = (u32) gf_fread(buf, 1880, stream);
			gf_m2ts_process_data(ctx->ts, buf, nb_read);
			if (ctx->duration.num || (nb_read!=1880)) break;
		}
		gf_m2ts_demux_del(ctx->ts);
		gf_fclose(stream);
		ctx->ts = gf_m2ts_demux_new();
		ctx->ts->on_event = m2tsdmx_on_event;
		ctx->ts->user = filter;
	} else if (!p) {
		GF_FilterEvent evt;
		ctx->duration.num = 1;

		//not-file base TS, we need to start demuxing the first time we see the PID
		if (!ctx->ipid) {
			GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
			gf_filter_pid_send_event(pid, &evt);
		}
	}
	ctx->ipid = pid;
	return GF_OK;
}


static GF_M2TS_PES *m2tsdmx_get_stream(GF_M2TSDmxCtx *ctx, GF_FilterPid *pid)
{
	u32 i, j, count, count2;
	count = gf_list_count(ctx->ts->programs);
	for (i=0; i<count; i++) {
		GF_M2TS_Program *prog = gf_list_get(ctx->ts->programs, i);
		count2 = gf_list_count(prog->streams);
		for (j=0; j<count2; j++) {
			GF_M2TS_PES *pes = (GF_M2TS_PES *)gf_list_get(prog->streams, j);
			if (pes->user == pid) return pes;
		}
	}
	return NULL;
}

static void m2tsdmx_switch_quality(GF_M2TS_Program *prog, GF_M2TS_Demuxer *ts, Bool switch_up)
{
	GF_M2TS_ES *es;
	u32 i, count;

	if (!prog->is_scalable)
		return;

	if (switch_up) {
		for (i = 0; i < GF_M2TS_MAX_STREAMS; i++) {
			es = ts->ess[i];
			if (es && (es->flags & GF_M2TS_ES_IS_PES) && (((GF_M2TS_PES *)es)->depends_on_pid == prog->pid_playing)) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("Turn on ES%d\n", es->pid));
				gf_m2ts_set_pes_framing((GF_M2TS_PES *)ts->ess[es->pid], GF_M2TS_PES_FRAMING_DEFAULT);
				prog->pid_playing = es->pid;
				return;
			}
		}
	}
	else {
		count = gf_list_count(prog->streams);
		for (i = 0; i < count; i++) {
			es = (GF_M2TS_ES *)gf_list_get(prog->streams, i);
			if (es && (es->pid == prog->pid_playing) && ((GF_M2TS_PES *)es)->depends_on_pid) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("Turn off ES%d - playing ES%d\n", es->pid, ((GF_M2TS_PES *)es)->depends_on_pid));
				gf_m2ts_set_pes_framing((GF_M2TS_PES *)ts->ess[es->pid], GF_M2TS_PES_FRAMING_SKIP);

				//do we want to send a reset ?
				prog->pid_playing = ((GF_M2TS_PES *)es)->depends_on_pid;
				return;
			}
		}
	}
}

static Bool m2tsdmx_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	GF_M2TS_PES *pes;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_M2TS_Demuxer *ts = ctx->ts;

	if (com->base.type == GF_FEVT_QUALITY_SWITCH) {
		u32 i, count;
		count = gf_list_count(ts->programs);
		for (i = 0; i < count; i++) {
			GF_M2TS_Program *prog = (GF_M2TS_Program *)gf_list_get(ts->programs, i);
			m2tsdmx_switch_quality(prog, ts, com->quality_switch.up);
		}
		//don't cancel event for RTP source
		return GF_FALSE;
	}

		//don't cancel event for RTP source
	if (!com->base.on_pid) return GF_FALSE;
	switch (com->base.type) {
	case GF_FEVT_PLAY:
		if (com->play.initial_broadcast_play==2)
			return GF_TRUE;
		pes = m2tsdmx_get_stream(ctx, com->base.on_pid);
		if (!pes) {
			if (com->base.on_pid == ctx->eit_pid) {
				return GF_FALSE;
			}
			return GF_FALSE;
		}
		if (com->play.no_byterange_forward)
			ctx->is_dash = GF_TRUE;
		/*mark pcr as not initialized*/
		if (pes->program->pcr_pid==pes->pid) pes->program->first_dts=0;
		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TSDmx] Setting default reframing for PID %d\n", pes->pid));

		/*this is a multiplex, only trigger the play command for the first activated stream*/
		ctx->nb_playing++;

		if (ctx->nb_playing>1) {
			Bool skip_com = GF_TRUE;
			//PLAY/STOP may arrive at different times depending on the length of filter chains on each PID
			//we stack number of STOP received and trigger seek when we have the same amount of play
			if (ctx->nb_stop_pending==ctx->nb_playing) {
				skip_com = GF_FALSE;
			}
			if (skip_com) {
				return GF_TRUE;
			}
		}

		ctx->nb_stop_pending = 0;
		//not file, don't cancel the event
		if (!ctx->is_file) {
			ctx->initial_play_done = GF_TRUE;
			return GF_FALSE;
		}

		ctx->map_time_on_prog_id = pes->program->number;
		ctx->media_start_range = com->play.start_range;


		if (ctx->is_file && ctx->duration.num) {
			file_pos = (u64) (ctx->file_size * com->play.start_range);
			file_pos *= ctx->duration.den;
			file_pos /= ctx->duration.num;
			if (file_pos > ctx->file_size) return GF_TRUE;
		}

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos) return GF_TRUE;
		}

		//file and seek, cancel the event and post a seek event to source
		ctx->in_seek = GF_TRUE;
		//we seek so consider the mux tuned in
		ctx->mux_tune_state = DMX_TUNE_DONE;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);
		return GF_TRUE;

	case GF_FEVT_STOP:
		pes = m2tsdmx_get_stream(ctx, com->base.on_pid);
		if (!pes) {
			if (com->base.on_pid == ctx->eit_pid) {
				return GF_FALSE;
			}
			return GF_FALSE;
		}
		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);

		if (com->play.initial_broadcast_play==2)
			return GF_TRUE;

		ctx->nb_stop_pending++;
		if (ctx->nb_playing) ctx->nb_playing--;

		//don't cancel event if still playing
		return ctx->nb_playing ? GF_TRUE : GF_FALSE;

	case GF_FEVT_PAUSE:
	case GF_FEVT_RESUME:
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
}



static GF_Err m2tsdmx_initialize(GF_Filter *filter)
{
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);

	ctx->ts = gf_m2ts_demux_new();
	if (!ctx->ts) return GF_OUT_OF_MEM;

	ctx->ts->on_event = m2tsdmx_on_event;
	ctx->ts->user = filter;

	ctx->filter = filter;
	if (ctx->dsmcc) {
		gf_m2ts_demux_dmscc_init(ctx->ts);
	}

	return GF_OK;
}


static void m2tsdmx_finalize(GF_Filter *filter)
{
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->ts) gf_m2ts_demux_del(ctx->ts);

}

static GF_Err m2tsdmx_process(GF_Filter *filter)
{
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	Bool check_block = GF_TRUE;
	const char *data;
	u32 size;

restart:
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			u32 i, nb_streams = gf_filter_get_opid_count(filter);

			gf_m2ts_flush_all(ctx->ts, ctx->is_dash);
			for (i=0; i<nb_streams; i++) {
				GF_FilterPid *opid = gf_filter_get_opid(filter, i);
				gf_filter_pid_set_eos(opid);
			}
			return GF_EOS;
		}
		return GF_OK;
	}
	if (ctx->sigfrag) {
		Bool is_start;
		gf_filter_pck_get_framing(pck, &is_start, NULL);
		if (is_start) {
			gf_m2ts_mark_seg_start(ctx->ts);
		}
	}
	//we process even if no stream playing: since we use unframed dispatch we may need to send packets to configure reframers
	//which will in turn connect to the sink which will send the PLAY event marking stream(s) as playing
	if (ctx->in_seek) {
		gf_m2ts_reset_parsers(ctx->ts);
		ctx->in_seek = GF_FALSE;
	} else if (check_block && !ctx->wait_for_progs) {
		u32 i, nb_streams, would_block = 0;
		nb_streams = gf_filter_get_opid_count(filter);
		for (i=0; i<nb_streams; i++) {
			GF_FilterPid *opid = gf_filter_get_opid(filter, i);
			if (!gf_filter_pid_is_playing(opid)) {
				would_block++;
			} else if ( gf_filter_pid_would_block(opid) ) {
				would_block++;
			}
		}
		if (would_block && (would_block==nb_streams)) {
			//keep filter alive
			if (ctx->nb_playing) {
				gf_filter_ask_rt_reschedule(filter, 0);
			}
			return GF_OK;
		}

		check_block = GF_FALSE;
	}

	data = gf_filter_pck_get_data(pck, &size);
	if (data && size)
		gf_m2ts_process_data(ctx->ts, (char*) data, size);

	gf_filter_pid_drop_packet(ctx->ipid);

	if (ctx->mux_tune_state==DMX_TUNE_WAIT_SEEK) {
		GF_FilterEvent fevt;
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		gf_filter_pid_send_event(ctx->ipid, &fevt);
		ctx->mux_tune_state = DMX_TUNE_DONE;
		gf_m2ts_reset_parsers(ctx->ts);
	} else {
		goto restart;
	}
	return GF_OK;
}

static const char *m2tsdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (gf_m2ts_probe_data(data, size)) {
		*score = GF_FPROBE_SUPPORTED;
		return "video/mp2t";
	}
	return NULL;
}

static const GF_FilterCapability M2TSDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT_STATIC, GF_PROP_PID_FILE_EXT, "ts|m2t|mts|dmb|trp"),
	CAP_STRING(GF_CAPS_INPUT_STATIC, GF_PROP_PID_MIME, "video/mpeg-2|video/mp2t|video/mpeg|audio/mpeg-2|audio/mp2t"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_METADATA),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_PRIVATE_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_M2TSDmxCtx, _n)
static const GF_FilterArgs M2TSDmxArgs[] =
{
	{ OFFS(temi_url), "force TEMI URL", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dsmcc), "enable DSMCC receiver", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(seeksrc), "seek local source file back to origin once all programs are setup", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(sigfrag), "signal segment boundaries on output packets for DASH or HLS sources", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dvbtxt), "export DVB teletext streams", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};


GF_FilterRegister M2TSDmxRegister = {
	.name = "m2tsdmx",
	GF_FS_SET_DESCRIPTION("MPEG-2 TS demultiplexer")
	GF_FS_SET_HELP("This filter demultiplexes MPEG-2 Transport Stream files/data into a set of media PIDs and frames.")
	.private_size = sizeof(GF_M2TSDmxCtx),
	.initialize = m2tsdmx_initialize,
	.finalize = m2tsdmx_finalize,
	.args = M2TSDmxArgs,
	.flags = GF_FS_REG_DYNAMIC_PIDS,
	SETCAPS(M2TSDmxCaps),
	.configure_pid = m2tsdmx_configure_pid,
	.process = m2tsdmx_process,
	.process_event = m2tsdmx_process_event,
	.probe_data = m2tsdmx_probe_data,
};


#endif

const GF_FilterRegister *m2tsdmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_MPEG2TS
	return &M2TSDmxRegister;
#else
	return NULL;
#endif
}
