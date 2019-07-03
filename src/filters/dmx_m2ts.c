/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2017
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

typedef struct {
	char *fragment;
	u32 id;
	/*if only pid is requested*/
	u32 pid;
} GF_M2TSDmxCtx_Prog;

typedef struct
{
	//opts
	const char *temi_url;
	Bool dsmcc;

	GF_Filter *filter;
	GF_FilterPid *ipid;

	GF_M2TS_Demuxer *ts;

	GF_FilterPid *eit_pid;

	Bool is_file;
	u64 file_size;
	Bool in_seek;
	Bool initial_play_done;
	u32 nb_playing;

	//duration estimation
	GF_Fraction duration;
	u64 first_pcr_found;
	u16 pcr_pid;
	u64 nb_pck_at_pcr;

	u32 map_time_on_prog_id;
	Double media_start_range;
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
			gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, &PROP_FRAC(ctx->duration) );
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

static void m2tsdmx_declare_pid(GF_M2TSDmxCtx *ctx, GF_M2TS_PES *stream, GF_ESD *esd)
{
	u32 i, count, codecid=0, stype=0;
	GF_FilterPid *opid;
	Bool m4sys_stream = GF_FALSE;
	Bool m4sys_iod_stream = GF_FALSE;
	Bool has_scal_layer = GF_FALSE;
	Bool unframed = GF_FALSE;
	if (stream->user) return;

	if (stream->flags & GF_M2TS_GPAC_CODEC_ID) {
		codecid = stream->stream_type;
		stype = gf_codecid_type(codecid);
		unframed = GF_TRUE;
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
		case GF_M2TS_AUDIO_LATM_AAC:
		case GF_M2TS_AUDIO_AAC:
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
			stype = GF_STREAM_AUDIO;
			codecid = GF_CODECID_AAC_MPEG4;
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
			break;
		case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
			((GF_M2TS_ES*)stream)->flags |= GF_M2TS_ES_SEND_REPEATED_SECTIONS;
			//fallthrough
		case GF_M2TS_SYSTEMS_MPEG4_PES:
			if (!esd) {
				m4sys_iod_stream = GF_TRUE;
				count = gf_list_count(stream->program->pmt_iod->ESDescriptors);
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
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSDmx] Stream type %d not supported - ignoring pid\n", stream->stream_type));
			return;
		}
	}

	opid = gf_filter_pid_new(ctx->filter);
	stream->user = opid;
	gf_filter_pid_set_udta(opid, stream);
	stream->flags |= GF_M2TS_ES_ALREADY_DECLARED;

	gf_filter_pid_set_property(opid, GF_PROP_PID_ID, &PROP_UINT(stream->pid) );
	if (stream->mpeg4_es_id)
		gf_filter_pid_set_property(opid, GF_PROP_PID_ESID, &PROP_UINT(stream->mpeg4_es_id) );

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
		if (unframed)
			gf_filter_pid_set_property(opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

		gf_filter_pid_set_property(opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(stream->program->pcr_pid) );
	}
	if (has_scal_layer)
		gf_filter_pid_set_property(opid, GF_PROP_PID_SCALABLE, &PROP_BOOL(GF_TRUE));

	gf_filter_pid_set_property(opid, GF_PROP_PID_SERVICE_ID, &PROP_UINT(stream->program->number) );

	if (ctx->duration.num>1)
		gf_filter_pid_set_property(opid, GF_PROP_PID_DURATION, &PROP_FRAC(ctx->duration) );

	/*indicate our coding dependencies if any*/
	if (!m4sys_stream) {
		if (stream->depends_on_pid) {
			gf_filter_pid_set_property(opid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(stream->depends_on_pid) );
			if ((stream->stream_type == GF_M2TS_VIDEO_HEVC_TEMPORAL) || (stream->stream_type == GF_M2TS_VIDEO_HEVC_MCTS)) {
				gf_filter_pid_set_property(opid, GF_PROP_PID_SUBLAYER, &PROP_BOOL(GF_TRUE) );
			}
		}
	}
	gf_m2ts_set_pes_framing((GF_M2TS_PES *)stream, GF_M2TS_PES_FRAMING_DEFAULT);
}

static void m2tsdmx_setup_program(GF_M2TSDmxCtx *ctx, GF_M2TS_Program *prog)
{
	u32 i, count;

	count = gf_list_count(prog->streams);
#ifdef FILTER_FIXME
	if (ctx->ts->tuner) {
		Bool found = 0;
		for (i=0; i<count; i++) {
			GF_M2TS_PES *pes = gf_list_get(prog->streams, i);
			if (pes->pid==ctx->ts->tuner->vpid) found = 1;
			else if (pes->pid==ctx->ts->tuner->apid) found = 1;
		}
		if (!found) return;
	}
#endif

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
		GF_M2TS_ES *es = gf_list_get(prog->streams, i);
		if (es->pid==prog->pmt_pid) continue;

		if (! (es->flags & GF_M2TS_ES_ALREADY_DECLARED)) {
			m2tsdmx_declare_pid(ctx, (GF_M2TS_PES *)es, NULL);
		}
	}
}

static void m2tsdmx_send_packet(GF_M2TSDmxCtx *ctx, GF_M2TS_PES_PCK *pck)
{
	GF_FilterPid *opid;
	GF_FilterPacket *dst_pck;
	u8 * data;

	/*pcr not initialized, don't send any data*/
//	if (! pck->stream->program->first_dts) return;
	if (!pck->stream->user) return;
	opid = pck->stream->user;

	dst_pck = gf_filter_pck_new_alloc(opid, pck->data_len, &data);
	memcpy(data, pck->data, pck->data_len);
	//we don't have end of frame signaling
	gf_filter_pck_set_framing(dst_pck, (pck->flags & GF_M2TS_PES_PCK_AU_START) ? GF_TRUE : GF_FALSE, GF_FALSE);

	if (pck->flags & GF_M2TS_PES_PCK_AU_START) {
		gf_filter_pck_set_cts(dst_pck, pck->PTS);
		if (pck->DTS != pck->PTS) {
			gf_filter_pck_set_dts(dst_pck, pck->DTS);
		}
		gf_filter_pck_set_sap(dst_pck, (pck->flags & GF_M2TS_PES_PCK_RAP) ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
	}
	gf_filter_pck_send(dst_pck);
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

	gf_filter_pck_send(dst_pck);

	if (pck->version_number + 1 == pck->stream->slcfg->carousel_version)
		return;
	pck->stream->slcfg->carousel_version = 1 + pck->version_number;


	if (pck->stream->flags & GF_M2TS_ES_IS_MPEG4_OD) {
		/* We need to decode OD streams to get the SL config for other streams :( */
		GF_ODCom *com;
		GF_ESD *esd;
		GF_ODUpdate* odU;
		GF_ESDUpdate* esdU;
		u32 com_count, com_index, od_count, od_index, esd_index;
		GF_ODCodec *od_codec = gf_odf_codec_new();

		gf_odf_codec_set_au(od_codec, pck->data + slh_len, pck->data_len - slh_len);
		gf_odf_codec_decode(od_codec);
		com_count = gf_list_count(od_codec->CommandList);
		for (com_index = 0; com_index < com_count; com_index++) {
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

#ifdef FILTER_FIXME
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
		break;
	case GF_M2TS_EVT_DSMCC_FOUND:
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		m2tsdmx_setup_program(ctx, param);
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
		break;
	case GF_M2TS_EVT_PMT_UPDATE:
		m2tsdmx_setup_program(ctx, param);
		break;

	case GF_M2TS_EVT_SDT_FOUND:
	case GF_M2TS_EVT_SDT_UPDATE:
//	case GF_M2TS_EVT_SDT_REPEAT:
		count = gf_list_count(ts->programs);
		for (i=0; i<count; i++) {
			GF_M2TS_Program *prog = gf_list_get(ts->programs, i);
			GF_M2TS_SDT *sdt = gf_m2ts_get_sdt_info(ts, prog->number);
			if (sdt) {
				u32 j, nb_streams;
				nb_streams = gf_list_count(prog->streams);
				for (j=0; j<nb_streams; j++) {
					GF_M2TS_ES *es = gf_list_get(prog->streams, j);
					if (es->user) {
						//TODO, translate non standard character maps to UTF8
						//we for now comment in test mode to avoid non UTF characters in text dumps
						if (isalnum(sdt->service[0]) || !gf_sys_is_test_mode())
							gf_filter_pid_set_property((GF_FilterPid *)es->user, GF_PROP_PID_SERVICE_NAME, &PROP_NAME( sdt->service ) );

						if (isalnum(sdt->provider[0]) || !gf_sys_is_test_mode())
							gf_filter_pid_set_property((GF_FilterPid *)es->user, GF_PROP_PID_SERVICE_PROVIDER, &PROP_NAME( sdt->provider ) );
					}
				}
			}
		}
		break;
	case GF_M2TS_EVT_DVB_GENERAL:
		if (ctx->eit_pid) {
			GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)param;
			u8 *data;
			GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(ctx->eit_pid, pck->data_len, &data);
			memcpy(data, pck->data, pck->data_len);
			gf_filter_pck_send(dst_pck);
		}
		break;
	case GF_M2TS_EVT_PES_PCK:
		m2tsdmx_send_packet(ctx, param);
		break;
	case GF_M2TS_EVT_SL_PCK: /* DMB specific */
		m2tsdmx_send_sl_packet(ctx, param);
		break;
	case GF_M2TS_EVT_PES_PCR:
	{
		u32 i, count;
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
			gf_filter_pck_set_cts(dst_pck, pcr);
			gf_filter_pck_set_clock_type(dst_pck, discontinuity ? GF_FILTER_CLOCK_PCR_DISC : GF_FILTER_CLOCK_PCR);
			gf_filter_pck_send(dst_pck);

			if (map_time) {
				gf_filter_pid_set_info_str(stream->user, "time:timestamp", &PROP_LONGUINT(pcr) );
				gf_filter_pid_set_info_str(stream->user, "time:media", &PROP_DOUBLE(ctx->media_start_range) );
			}
		}

		if (map_time) {
			ctx->map_time_on_prog_id = 0;
		}
	}
		break;

	case GF_M2TS_EVT_TDT:
	{
		GF_M2TS_TDT_TOT *tdt = (GF_M2TS_TDT_TOT *)param;
		u64 utc_ts = gf_net_get_utc_ts(tdt->year, tdt->month, tdt->day, tdt->hour, tdt->minute, tdt->second);
		count = gf_list_count(ts->programs );
		for (i=0; i<count; i++) {
			GF_M2TS_Program *prog = gf_list_get(ts->programs, i);
			u32 j, count2 = gf_list_count(prog->streams);
			for (j=0; j<count2; j++) {
				GF_M2TS_ES * stream = gf_list_get(prog->streams, j);
				if (stream->user) {
					gf_filter_pid_set_property(stream->user, GF_PROP_PID_UTC_TIME, & PROP_LONGUINT(utc_ts) );
					gf_filter_pid_set_property(stream->user, GF_PROP_PID_UTC_TIMESTAMP, & PROP_LONGUINT(prog->last_pcr_value / 300) );
				}
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[M2TS In] Mapping TDT Time %04d/%02d/%02d %02d:%02d:%02d and PCR time "LLD" on program %d\n",
				                                       tdt->year, tdt->month, tdt->day, tdt->hour, tdt->minute, tdt->second, prog->last_pcr_value/300, prog->number));
		}
	}
		break;
	case GF_M2TS_EVT_TOT:
		break;

	case GF_M2TS_EVT_DURATION_ESTIMATED:
	{
		Double duration = (Double) ((GF_M2TS_PES_PCK *) param)->PTS;
		duration /= 1000;
		count = gf_list_count(ts->programs);
		for (i=0; i<count; i++) {
			GF_M2TS_Program *prog = gf_list_get(ts->programs, i);
			u32 j, count2;
			count2 = gf_list_count(prog->streams);
			for (j=0; j<count2; j++) {
				GF_M2TS_ES * stream = gf_list_get(prog->streams, j);
				if (stream->user) {
					gf_filter_pid_set_property(stream->user, GF_PROP_PID_DURATION, & PROP_DOUBLE(duration) );
				}
			}
		}
	}
	break;

#ifdef FILTER_FIXME
	case GF_M2TS_EVT_TEMI_LOCATION:
	{
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(com));
		com.addon_info.command_type = GF_NET_ASSOCIATED_CONTENT_LOCATION;
		com.addon_info.external_URL = ((GF_M2TS_TemiLocationDescriptor*)param)->external_URL;
		if (ctx->force_temi_url)
			com.addon_info.external_URL = ctx->force_temi_url;
		com.addon_info.is_announce = ((GF_M2TS_TemiLocationDescriptor*)param)->is_announce;
		com.addon_info.is_splicing = ((GF_M2TS_TemiLocationDescriptor*)param)->is_splicing;
		com.addon_info.activation_countdown = ((GF_M2TS_TemiLocationDescriptor*)param)->activation_countdown;
//		com.addon_info.reload_external = ((GF_M2TS_TemiLocationDescriptor*)param)->reload_external;
		com.addon_info.timeline_id = ((GF_M2TS_TemiLocationDescriptor*)param)->timeline_id;
		gf_service_command(ctx->service, &com, GF_OK);
	}
	break;
	case GF_M2TS_EVT_TEMI_TIMECODE:
	{
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(com));
		com.addon_time.command_type = GF_NET_ASSOCIATED_CONTENT_TIMING;
		com.addon_time.timeline_id = ((GF_M2TS_TemiTimecodeDescriptor*)param)->timeline_id;
		com.addon_time.media_pts = ((GF_M2TS_TemiTimecodeDescriptor*)param)->pes_pts;
		com.addon_time.media_timescale = ((GF_M2TS_TemiTimecodeDescriptor*)param)->media_timescale;
		com.addon_time.media_timestamp = ((GF_M2TS_TemiTimecodeDescriptor*)param)->media_timestamp;
		com.addon_time.force_reload = ((GF_M2TS_TemiTimecodeDescriptor*)param)->force_reload;
		com.addon_time.is_paused = ((GF_M2TS_TemiTimecodeDescriptor*)param)->is_paused;
		com.addon_time.ntp = ((GF_M2TS_TemiTimecodeDescriptor*)param)->ntp;
		gf_service_command(ctx->service, &com, GF_OK);
	}
	break;
#endif
	}
}

static GF_Err m2tsdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;;
	FILE *stream = NULL;
	GF_M2TSDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
//		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (p && p->value.string && !ctx->duration.num) {
		stream = gf_fopen(p->value.string, "rb");
	}

	if (stream) {
		ctx->ipid = pid;
		ctx->is_file = GF_TRUE;
		ctx->ts->seek_mode = GF_TRUE;
		ctx->ts->on_event = m2tsdmx_on_event_duration_probe;
		while (!feof(stream)) {
			char buf[1880];
			u32 nb_read = (u32) fread(buf, 1, 1880, stream);
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
		pes = m2tsdmx_get_stream(ctx, com->base.on_pid);
		if (!pes) {
			if (com->base.on_pid == ctx->eit_pid) {
				return GF_FALSE;
			}
			return GF_FALSE;
		}
		/*mark pcr as not initialized*/
		if (pes->program->pcr_pid==pes->pid) pes->program->first_dts=0;
		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GF_M2TSDmxCtx] Setting default reframing for PID %d\n", pes->pid));

		/*this is a multplex, only trigger the play command for the first stream activated*/
		ctx->nb_playing++;
		if (ctx->nb_playing>1) return GF_TRUE;

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
			if (!file_pos)
				return GF_TRUE;
		}

		//file and seek, cancel the event and post a seek event to source
		ctx->in_seek = GF_TRUE;

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
		/* In case of EOS, we may receive a stop command after no one is playing */
		if (ctx->nb_playing)
			ctx->nb_playing--;

		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);
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
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);
	const char *data;
	u32 size;

	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			u32 i, nb_streams = gf_filter_get_opid_count(filter);
			for (i=0; i<nb_streams; i++) {
				GF_FilterPid *opid = gf_filter_get_opid(filter, i);
				GF_M2TS_PES *stream = gf_filter_pid_get_udta(opid);

				if (stream->flags & GF_M2TS_ES_IS_PES) {
					gf_m2ts_flush_pes(ctx->ts, (GF_M2TS_PES *) stream);
				}
				gf_filter_pid_set_eos(opid);
			}
			return GF_EOS;
		}
		return GF_OK;
	}
	//we process even if no stream playing: since we use unframed dispatch we may need to send packets to configure reframers
	//which will in turn connect to the sink which will send the PLAY event marking stream(s) as playing
	if (ctx->in_seek) {
		gf_m2ts_reset_parsers(ctx->ts);
		ctx->in_seek = GF_FALSE;
	} else {
		u32 i, nb_streams, would_block = 0;
		nb_streams = gf_filter_get_opid_count(filter);
		for (i=0; i<nb_streams; i++) {
			GF_FilterPid *opid = gf_filter_get_opid(filter, i);
			if ( gf_filter_pid_would_block(opid) ) {
				would_block++;
			}
		}
		if (would_block && (would_block==nb_streams))
			return GF_OK;
	}

	data = gf_filter_pck_get_data(pck, &size);
	if (data)
		gf_m2ts_process_data(ctx->ts, (char*) data, size);

	gf_filter_pid_drop_packet(ctx->ipid);
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
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/mpeg-2|video/mp2t|video/mpeg"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_PRIVATE_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ts|m2t|mts|dmb|trp"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_M2TSDmxCtx, _n)
static const GF_FilterArgs M2TSDmxArgs[] =
{
	{ OFFS(temi_url), "force TEMI URL", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dsmcc), "enable DSMCC receiver", GF_PROP_BOOL, "no", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};


GF_FilterRegister M2TSDmxRegister = {
	.name = "m2tsdmx",
	GF_FS_SET_DESCRIPTION("MPEG-2 TS demuxer")
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
