/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-2 TS mux filter
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
#include <gpac/mpegts.h>
#include <gpac/iso639.h>

typedef struct
{
	//filter args
	u32 pmt_id, pmt_rate, pcr_offset, pmt_version, sdt_rate, breq, mpeg4;
	u32 rate, pat_rate, repeat_rate, repeat_img, max_pcr, nb_pack, sid, bifs_pes, temi_delay, temi_offset;
	GF_M2TS_PackMode pes_pack;
	Bool flush_rap, rt, pcr_only, disc, temi_ntp, latm;
	s64 pcr_init;
	char *name, *provider, *temi;
	u32 log_freq;

	//internal
	GF_FilterPid *opid;
	GF_M2TS_Mux *mux;

	GF_List *pids;

	Bool check_pcr;
	Bool update_mux;
	char *pack_buffer;
	u64 nb_pck;
	Bool init_buffering;
	u32 last_log_time;
	Bool pmt_update_pending;

} GF_TSMuxCtx;

typedef struct
{
	GF_ESInterface esi;
	GF_FilterPid *ipid;
	u32 sid;
	u32 codec_id;
	u32 pmt_pid;
	u32 nb_pck;
	Bool is_repeat;
	u32 nb_repeat_last;

	GF_TSMuxCtx *ctx;
	u32 last_cv;
	u32 cts_dts_shift;
	Bool done;

	u32 temi_id;
	char *temi_url;

	u32 last_temi_url;
	char af_data[188];

	Bool rewrite_odf;
} M2Pid;

static u32 tsmux_format_af_descriptor(char *af_data, u32 timeline_id, u64 timecode, u32 timescale, u64 ntp, const char *temi_url, u32 temi_delay, u32 *last_url_time)
{
	u32 res;
	u32 len;
	u32 last_time;
	GF_BitStream *bs = gf_bs_new(af_data, 188, GF_BITSTREAM_WRITE);

	if (ntp) {
		last_time = 1000*(ntp>>32);
		last_time += 1000*(ntp&0xFFFFFFFF)/0xFFFFFFFF;
	} else {
		last_time = (u32) (1000*timecode/timescale);
	}
	if (temi_url && (!*last_url_time || (last_time - *last_url_time + 1 >= temi_delay)) ) {
		*last_url_time = last_time + 1;
		len = 0;
		gf_bs_write_int(bs,	GF_M2TS_AFDESC_LOCATION_DESCRIPTOR, 8);
		gf_bs_write_int(bs,	len, 8);

		gf_bs_write_int(bs,	0, 1); //force_reload
		gf_bs_write_int(bs,	0, 1); //is_announcement
		gf_bs_write_int(bs,	0, 1); //splicing_flag
		gf_bs_write_int(bs,	0, 1); //use_base_temi_url
		gf_bs_write_int(bs,	0xFF, 5); //reserved
		gf_bs_write_int(bs,	timeline_id, 7); //timeline_id

		if (strlen(temi_url)) {
			char *url = (char *)temi_url;
			if (!strnicmp(temi_url, "http://", 7)) {
				gf_bs_write_int(bs,	1, 8); //url_scheme
				url = (char *) temi_url + 7;
			} else if (!strnicmp(temi_url, "https://", 8)) {
				gf_bs_write_int(bs,	2, 8); //url_scheme
				url = (char *) temi_url + 8;
			} else {
				gf_bs_write_int(bs,	0, 8); //url_scheme
			}
			gf_bs_write_u8(bs, (u32) strlen(url)); //url_path_len
			gf_bs_write_data(bs, url, (u32) strlen(url) ); //url
			gf_bs_write_u8(bs, 0); //nb_addons
		}
		//rewrite len
		len = (u32) gf_bs_get_position(bs) - 2;
		af_data[1] = len;
	}

	if (timescale || ntp) {
		Bool use64 = (timecode > 0xFFFFFFFFUL) ? GF_TRUE : GF_FALSE;
		len = 3; //3 bytes flags

		if (timescale) len += 4 + (use64 ? 8 : 4);
		if (ntp) len += 8;

		//write timeline descriptor
		gf_bs_write_int(bs,	GF_M2TS_AFDESC_TIMELINE_DESCRIPTOR, 8);
		gf_bs_write_int(bs,	len, 8);

		gf_bs_write_int(bs,	timescale ? (use64 ? 2 : 1) : 0, 2); //has_timestamp
		gf_bs_write_int(bs,	ntp ? 1 : 0, 1); //has_ntp
		gf_bs_write_int(bs,	0, 1); //has_ptp
		gf_bs_write_int(bs,	0, 2); //has_timecode
		gf_bs_write_int(bs,	0, 1); //force_reload
		gf_bs_write_int(bs,	0, 1); //paused
		gf_bs_write_int(bs,	0, 1); //discontinuity
		gf_bs_write_int(bs,	0xFF, 7); //reserved
		gf_bs_write_int(bs,	timeline_id, 8); //timeline_id
		if (timescale) {
			gf_bs_write_u32(bs,	timescale); //timescale
			if (use64)
				gf_bs_write_u64(bs,	timecode); //timestamp
			else
				gf_bs_write_u32(bs,	(u32) timecode); //timestamp
		}
		if (ntp) {
			gf_bs_write_u64(bs,	ntp); //ntp
		}
	}
	res = (u32) gf_bs_get_position(bs);
	gf_bs_del(bs);
	return res;
}


GF_SLConfig *tsmux_get_sl_config(GF_TSMuxCtx *ctx, u32 timescale, GF_SLConfig *slc)
{
	if (!slc) slc = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	slc->predefined = 0;
	slc->useAccessUnitStartFlag = 1;
	slc->useAccessUnitEndFlag = 1;
	slc->useRandomAccessPointFlag = 1;
	slc->useTimestampsFlag = 1;
	slc->timestampLength = 33;
	slc->timestampResolution = timescale;

	/*test mode in which time stamps are 90khz and not coded but copied over from PES header*/
	if (ctx->bifs_pes==2) {
		slc->timestampLength = 0;
		slc->timestampResolution = 90000;
	}
	return slc;
}

static void tsmux_rewrite_odf(GF_TSMuxCtx *ctx, GF_ESIPacket *es_pck)
{
	u32 com_count, com_index, od_count, esd_index, od_index;
	GF_ODCom *com;
	GF_ODUpdate *odU;
	GF_ESDUpdate *esdU;
	GF_ESD *esd;
	GF_ODCodec *od_codec = gf_odf_codec_new();

	gf_odf_codec_set_au(od_codec, es_pck->data, es_pck->data_len);
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
					assert(esd->slConfig);
					esd->slConfig = tsmux_get_sl_config(ctx, esd->slConfig->timestampResolution, esd->slConfig);
				}
			}
			break;
		case GF_ODF_ESD_UPDATE_TAG:
			esdU = (GF_ESDUpdate*)com;
			esd_index = 0;
			while ( (esd = gf_list_enum(esdU->ESDescriptors, &esd_index)) ) {
					assert(esd->slConfig);
					esd->slConfig = tsmux_get_sl_config(ctx, esd->slConfig->timestampResolution, esd->slConfig);
			}
			break;
		}
	}
	gf_odf_codec_encode(od_codec, 1);
	es_pck->data = NULL;
	es_pck->data_len = 0;
	gf_odf_codec_get_au(od_codec, &es_pck->data, &es_pck->data_len);
	gf_odf_codec_del(od_codec);

}

static GF_Err tsmux_esi_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	u32 cversion;
	M2Pid *tspid = (M2Pid *)ifce->input_udta;
	if (!tspid) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
	{
		u64 dts;
		GF_ESIPacket es_pck;
		//current pck
		GF_FilterPacket *pck;
		pck = gf_filter_pid_get_packet(tspid->ipid);
		//if PMT update is pending after this packet fetch (reconfigure), don't send the packet
		if (tspid->ctx->pmt_update_pending) return GF_OK;

		if (!pck) {
			if (gf_filter_pid_is_eos(tspid->ipid)) {
				if (tspid->ctx->rt
					&& tspid->ctx->repeat_img
					&& (tspid->nb_pck==1)
					&& (tspid->esi.stream_type==GF_STREAM_VISUAL)
				) {
					tspid->nb_repeat_last++;
					tspid->is_repeat = GF_TRUE;
				} else {
					tspid->done = GF_TRUE;
					ifce->caps |= GF_ESI_STREAM_IS_OVER;
				}
			}
			return GF_OK;
		}

		memset(&es_pck, 0, sizeof(GF_ESIPacket));
		es_pck.flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_AU_END | GF_ESI_DATA_HAS_CTS;
		if (gf_filter_pck_get_sap(pck))
			es_pck.flags |= GF_ESI_DATA_AU_RAP;

		cversion = gf_filter_pck_get_carousel_version(pck);
		if (cversion+1 == tspid->last_cv) {
			es_pck.flags |= GF_ESI_DATA_REPEAT;
		}
		tspid->last_cv = cversion+1;

		if (tspid->is_repeat)
			es_pck.flags |= GF_ESI_DATA_REPEAT;

		es_pck.cts = gf_filter_pck_get_cts(pck);

		if (tspid->temi_id) {
			u64 ntp=0;
			u64 tc = es_pck.cts;
			if (tspid->ctx->temi_offset) {
				tc += ((u64) tspid->ctx->temi_offset) * ifce->timescale / 1000;
			}

			if (tspid->ctx->temi_ntp) {
				u32 sec, frac;
				gf_net_get_ntp(&sec, &frac);
				ntp = sec;
				ntp <<= 32;
				ntp |= frac;
			}
			es_pck.mpeg2_af_descriptors_size = tsmux_format_af_descriptor(tspid->af_data, tspid->temi_id, tc, tspid->esi.timescale, ntp, tspid->temi_url, tspid->ctx->temi_delay, &tspid->last_temi_url);
			es_pck.mpeg2_af_descriptors = tspid->af_data;
		}

		if (tspid->nb_repeat_last) {
			es_pck.cts += tspid->nb_repeat_last * ifce->timescale * tspid->ctx->repeat_img / 1000;
		}

		es_pck.dts = es_pck.cts;
		dts = gf_filter_pck_get_dts(pck);
		if (dts != GF_FILTER_NO_TS) {
			es_pck.dts = dts;
			if (tspid->cts_dts_shift) {
				es_pck.dts += + tspid->cts_dts_shift;
			}
			if (es_pck.dts > es_pck.cts) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSMux] Packet CTS "LLU" is less than packet DTS "LLU" !\n", es_pck.cts, es_pck.dts));
			}
			if (es_pck.dts != es_pck.cts) {
				es_pck.flags |= GF_ESI_DATA_HAS_DTS;
			}
		}
		es_pck.data = (char *) gf_filter_pck_get_data(pck, &es_pck.data_len);
		es_pck.duration = gf_filter_pck_get_duration(pck);

		if (tspid->rewrite_odf) {
			tsmux_rewrite_odf(tspid->ctx, &es_pck);
		}

		tspid->nb_pck++;
		ifce->output_ctrl(ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &es_pck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[M2TSMux] PID %d: packet %d CTS "LLU"\n", tspid->esi.stream_id, tspid->nb_pck, es_pck.cts));

		//data is copied by muxer for now, should need rewrite to avoid un-needed allocations
		gf_filter_pid_drop_packet(tspid->ipid);

		if (tspid->rewrite_odf) {
			gf_free(es_pck.data);
		}

	}
	return GF_OK;

	case GF_ESI_INPUT_DESTROY:
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


void update_m4sys_info(GF_TSMuxCtx *ctx, GF_M2TS_Mux_Program *prog)
{
	GF_M2TS_Mux_Stream *stream = prog->streams;

	if (prog->iod) gf_odf_desc_del(prog->iod);
	prog->iod = gf_odf_desc_new(GF_ODF_IOD_TAG);
	while (stream) {
		M2Pid *tspid = (M2Pid *)stream->ifce->input_udta;
		const GF_PropertyValue *p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_IN_IOD);
		if (p && p->value.boolean) {
			GF_ESD *esd = gf_odf_desc_esd_new(0);
			esd->decoderConfig->objectTypeIndication = stream->ifce->object_type_indication;
			esd->decoderConfig->streamType = stream->ifce->stream_type;
			esd->ESID = stream->ifce->stream_id;
			esd->dependsOnESID = stream->ifce->depends_on_stream;
			if (stream->ifce->decoder_config_size) {
				esd->decoderConfig->decoderSpecificInfo->data = gf_malloc(sizeof(char)*stream->ifce->decoder_config_size);
				memcpy(esd->decoderConfig->decoderSpecificInfo->data, stream->ifce->decoder_config, stream->ifce->decoder_config_size);
				esd->decoderConfig->decoderSpecificInfo->dataLength = stream->ifce->decoder_config_size;
			}
			tsmux_get_sl_config(ctx, stream->ifce->timescale, esd->slConfig);
			gf_list_add( ((GF_ObjectDescriptor *)prog->iod)->ESDescriptors, esd);
		}
		stream->ifce->sl_config = tsmux_get_sl_config(ctx, stream->ifce->timescale, stream->ifce->sl_config);
		stream = stream->next;
	}
}

static void tsmux_setup_esi(GF_TSMuxCtx *ctx, GF_M2TS_Mux_Program *prog, M2Pid *tspid, u32 stream_type)
{
	const GF_PropertyValue *p;

	memset(&tspid->esi, 0, sizeof(GF_ESInterface));
	tspid->esi.stream_type = stream_type;
	
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_TIMESCALE);
	tspid->esi.timescale = p->value.uint;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		tspid->esi.decoder_config = p->value.data.ptr;
		tspid->esi.decoder_config_size = p->value.data.size;
	}
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_ID);
	if (p) tspid->esi.stream_id = p->value.uint;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) tspid->esi.depends_on_stream = p->value.uint;
	if (tspid->codec_id< GF_CODECID_LAST_MPEG4_MAPPING)
		tspid->esi.object_type_indication = tspid->codec_id;
	else
		tspid->esi.fourcc = tspid->codec_id;

	if (tspid->codec_id< GF_CODECID_LAST_MPEG4_MAPPING)
		tspid->esi.object_type_indication = tspid->codec_id;

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_LANGUAGE);
	if (p) {
		s32 idx = gf_lang_find(p->value.string);
		if (idx>=0) {
			const char *code = gf_lang_get_3cc(idx);
			if (code) tspid->esi.lang = GF_4CC(code[0], code[1], code[2], ' ');
		}
	}

	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_DURATION);
	if (p) {
		tspid->esi.duration = (Double) p->value.frac.num;
		tspid->esi.duration /= p->value.frac.den;
	}
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_BITRATE);
	if (p) tspid->esi.bit_rate = p->value.uint;
	/*repeat rate in ms for carrouseling - 0 if no repeat*/

	tspid->esi.repeat_rate = ctx->repeat_rate;
	p = gf_filter_pid_get_property(tspid->ipid, GF_PROP_PID_CAROUSEL_RATE);
	if (p) tspid->esi.repeat_rate = p->value.uint;

	tspid->rewrite_odf = GF_FALSE;
	if (tspid->esi.stream_type==GF_STREAM_OD) {
		tspid->rewrite_odf = GF_TRUE;
		update_m4sys_info(ctx, prog);
	} else if (prog->iod) {
		update_m4sys_info(ctx, prog);
	}

	tspid->esi.caps = 0;
	switch (tspid->esi.stream_type) {
	case GF_STREAM_AUDIO:
		if (ctx->latm) tspid->esi.caps |= GF_ESI_AAC_USE_LATM;
	case GF_STREAM_VISUAL:
		if (ctx->mpeg4==2) {
			tspid->esi.caps |= GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS;
		}
		break;
	}

	tspid->esi.input_ctrl = tsmux_esi_ctrl;
	tspid->esi.input_udta = tspid;
}

static void tsmux_setup_temi(GF_TSMuxCtx *ctx, GF_M2TS_Mux_Stream *stream, M2Pid *tspid)
{
	GF_M2TS_Mux_Stream *a_stream;
	u32 service_id=0;
	u32 st_idx=0;
	char *turl = ctx->temi;
	if (!turl) return;
	u32 idx = 0;
	u32 temi_id=0;
	assert(idx>=0);

	//find our stream index
	a_stream = stream->program->streams;
	while (a_stream) {
		if (stream==a_stream) break;
		st_idx++;
		a_stream = a_stream->next;
	}

	while (turl) {
		char *sep;
		if (turl[0]=='#') {
			sscanf(turl, "#%d#", &service_id);
			turl = strchr(turl+1, '#');
			assert(turl);
			turl += 1;
			idx = 0;
		}

		sep = strchr(turl, ',');
		if (sep) sep[0] = 0;

		if (strlen(turl) && (idx==st_idx) && (!service_id || (service_id==stream->program->number) ))  {
			tspid->temi_id = atoi(turl);
			if (!tspid->temi_id) {
				tspid->temi_url = gf_strdup(turl);
				tspid->temi_id = temi_id+1;
			}
		}
		idx++;
		if (!sep) break;
		sep[0] = ',';
		turl = sep+1;
	}
}

static GF_Err tsmux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 service_id, codec_id, streamtype;
	const GF_PropertyValue *p;
	M2Pid *tspid=NULL;
	char *sname, *pname;
	GF_M2TS_Mux_Program *prog;
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);
	if (is_remove) {

	}

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	codec_id = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	streamtype = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	service_id = p ? p->value.uint : ctx->sid;

	sname = ctx->name;
	pname = ctx->provider;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_NAME);
	if (p) sname = p->value.string;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_PROVIDER);
	if (p) pname = p->value.string;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("ts") );

	tspid = gf_filter_pid_get_udta(pid);
	if (!tspid) {
		GF_SAFEALLOC(tspid, M2Pid);
		gf_filter_pid_set_udta(pid, tspid);
		tspid->ipid = pid;
		tspid->sid = service_id;
		tspid->ctx = ctx;
		gf_list_add(ctx->pids, tspid);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

		if (ctx->breq) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_BUFFER_REQ, pid);
			evt.buffer_req.max_buffer_us = 1000*ctx->breq;
			evt.buffer_req.pid_only = GF_TRUE;
			gf_filter_pid_send_event(pid, &evt);
		}
	}

	//do we need a new program
	prog = gf_m2ts_mux_program_find(ctx->mux, service_id);
	if (!prog) {
		u32 pcr_offset=0;
		u32 pmt_id = ctx->pmt_id;

		if (!pmt_id) pmt_id = 100;
		if ( gf_m2ts_mux_program_count(ctx->mux)) {
			pmt_id += 100;
		}

		if (ctx->pcr_offset==(u32)-1) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_MAX_FRAME_SIZE);
			if (p && p->value.uint && ctx->rate) {
				Double r = p->value.uint * 8;
				r *= 90000;
				r/= ctx->rate;
				//add 10% of safety to cover TS signaling and other potential table update while sending the largest PES
				r *= 1.1;
				pcr_offset = (u32) r;
			}
		} else {
			pcr_offset = ctx->pcr_offset;
		}

		prog = gf_m2ts_mux_program_add(ctx->mux, service_id, pmt_id, ctx->pmt_rate, pcr_offset, ctx->mpeg4, ctx->pmt_version, ctx->disc);

		if (sname) gf_m2ts_mux_program_set_name(prog, sname, pname);

		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[M2TSMux] Setting up program ID %d - send rates: PSI %d ms PCR every %d ms max - PCR offset %d\n", service_id, ctx->pmt_rate, ctx->max_pcr, ctx->pcr_offset));
	}
	//no changes in codec ID or stream type
	if ((tspid->codec_id == codec_id) && (tspid->esi.stream_type == streamtype))
		return GF_OK;

	if (!tspid->codec_id) {
		Bool is_pcr=GF_FALSE;
		Bool force_pes=GF_FALSE;
		u32 pes_pid;
		GF_M2TS_Mux_Stream *mstream;
		assert(!tspid->esi.stream_type);
		tspid->codec_id = codec_id;
		tspid->esi.stream_type = streamtype;

		if (ctx->bifs_pes && (tspid->esi.stream_type==GF_STREAM_SCENE))
			force_pes = GF_TRUE;

		pes_pid = gf_m2ts_mux_program_get_pmt_pid(prog);
		pes_pid += 1 + gf_m2ts_mux_program_get_stream_count(prog);
		if (gf_m2ts_mux_program_get_pcr_pid(prog)==0) {
			if (streamtype==GF_STREAM_VISUAL)
				is_pcr = GF_TRUE;
			else
				ctx->check_pcr = GF_TRUE;
			//if no video track we will update the PCR at the first packets
		}

		tsmux_setup_esi(ctx, prog, tspid, streamtype);
		mstream = gf_m2ts_program_stream_add(prog, &tspid->esi, pes_pid, is_pcr, force_pes, GF_FALSE);
		tsmux_setup_temi(ctx, mstream, tspid);
	} else {
		tspid->codec_id = codec_id;
		tsmux_setup_esi(ctx, prog, tspid, streamtype);
		prog->pmt->table_needs_update = GF_TRUE;
		ctx->pmt_update_pending = GF_TRUE;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_MEDIA_SKIP);
	if (p) tspid->cts_dts_shift = p->value.sint;
	ctx->update_mux = GF_TRUE;
	return GF_OK;
}

static void tsmux_assign_pcr(GF_TSMuxCtx *ctx)
{
	GF_M2TS_Mux_Program *prog = ctx->mux->programs;
	while (prog) {
		GF_M2TS_Mux_Stream *stream;
		if (prog->pmt) {
			prog = prog->next;
			continue;
		}
		stream = prog->streams;
		while (stream) {
			if (stream->ifce->stream_type==GF_STREAM_VISUAL) {
				prog->pcr = stream;
				break;
			}
			stream = stream->next;
		}
		if (!prog->pcr) prog->pcr = prog->streams;
		ctx->update_mux = GF_TRUE;
		prog = prog->next;
	}
}

static Bool tsmux_init_buffering(GF_Filter *filter, GF_TSMuxCtx *ctx)
{
	u32 mbuf = ctx->breq*1000;
	u32 i, count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		u32 buf;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		gf_filter_pid_get_buffer_occupancy(pid, NULL, NULL, NULL, &buf);
		if ((buf < mbuf) && !gf_filter_pid_has_seen_eos(pid))
			return GF_FALSE;
	}
	ctx->init_buffering = GF_FALSE;
	gf_m2ts_mux_update_config(ctx->mux, GF_TRUE);
	return GF_TRUE;
}

static GF_Err tsmux_process(GF_Filter *filter)
{
	const char *ts_pck;
	u32 nb_pck_in_pack;
	u32 status, usec_till_next;
	GF_FilterPacket *pck;
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->init_buffering && !tsmux_init_buffering(filter, ctx)) return GF_OK;

	if (ctx->check_pcr) {
		ctx->check_pcr = GF_FALSE;
		tsmux_assign_pcr(ctx);
	}
	if (ctx->update_mux) {
		gf_m2ts_mux_update_config(ctx->mux, GF_FALSE);
	}
	nb_pck_in_pack=0;
	while ((ts_pck = gf_m2ts_mux_process(ctx->mux, &status, &usec_till_next)) != NULL) {
		char *output;
		u32 osize;

		if (ctx->nb_pack>1) {
			memcpy(ctx->pack_buffer + 188 * ctx->nb_pack, ts_pck, 188);
			nb_pck_in_pack++;

			if (nb_pck_in_pack < ctx->nb_pack)
				continue;

			ts_pck = (const char *) ctx->pack_buffer;
		} else {
			nb_pck_in_pack = 1;
		}
		osize = nb_pck_in_pack * 188;
		pck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
		memcpy(output, ts_pck, osize);
		gf_filter_pck_set_framing(pck, ctx->nb_pck ? GF_FALSE : GF_TRUE, (status==GF_M2TS_STATE_EOS) ? GF_TRUE : GF_FALSE);
		gf_filter_pck_send(pck);
		ctx->nb_pck++;
		nb_pck_in_pack = 0;

		if (status>=GF_M2TS_STATE_PADDING) {
			break;
		}
	}
	if (status==GF_M2TS_STATE_EOS) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}

	if (ctx->rt) {
		u32 now = gf_sys_clock();
		if (!ctx->last_log_time)
			ctx->last_log_time = now;
		else if (now > ctx->last_log_time + ctx->log_freq) {
			ctx->last_log_time = now;
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[M2TSMux] time % 6d TS time % 6d bitrate % 8d\r", gf_m2ts_get_sys_clock(ctx->mux), gf_m2ts_get_ts_clock(ctx->mux), ctx->mux->average_birate_kbps));
		}
		if (status == GF_M2TS_STATE_IDLE) {
			u64 sleep_for=0;
#if 0
			/*wait till next packet is ready to be sent*/
			if (usec_till_next>1000) {
				sleep_for = usec_till_next;
			}
#else
			//we don't have enough precision on usec counting and we end up eating one core on most machines, so let's just sleep
			//one second whenever we are idle - it's maybe too much but the muxer will catchup afterwards
			sleep_for = 1000;
#endif
			if (sleep_for)
				gf_filter_ask_rt_reschedule(filter, sleep_for);
		}
	}
	//PMT update management is still under progress...
	ctx->pmt_update_pending = 0;

	return GF_OK;
}

static GF_Err tsmux_initialize(GF_Filter *filter)
{
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);
	gf_filter_sep_max_extra_input_pids(filter, -1);

	ctx->mux = gf_m2ts_mux_new(ctx->rate, ctx->pat_rate, ctx->rt);
	ctx->mux->flush_pes_at_rap = ctx->flush_rap;

	gf_m2ts_mux_use_single_au_pes_mode(ctx->mux, ctx->pes_pack);
	if (ctx->pcr_init>=0) gf_m2ts_mux_set_initial_pcr(ctx->mux, (u64) ctx->pcr_init);
	gf_m2ts_mux_set_pcr_max_interval(ctx->mux, ctx->max_pcr);
	gf_m2ts_mux_enable_pcr_only_packets(ctx->mux, ctx->pcr_only);

	if (!ctx->sid) ctx->sid = 1;
	if (ctx->sdt_rate) {
		gf_m2ts_mux_enable_sdt(ctx->mux, ctx->sdt_rate);
	}

	if (!gf_filter_block_enabled(filter)) {
		ctx->breq = 0;
	} else {
		ctx->init_buffering = GF_TRUE;
	}
	ctx->pids = gf_list_new();
	if (ctx->nb_pack>1) ctx->pack_buffer = gf_malloc(sizeof(char)*188*ctx->nb_pack);
	return GF_OK;
}


static void tsmux_finalize(GF_Filter *filter)
{
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);

	u64 bits = ctx->mux->tot_pck_sent*8*188;
	u64 dur_ms = gf_m2ts_get_ts_clock(ctx->mux);
	if (!dur_ms) dur_ms = 1;
	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[M2TSMux] Done muxing - %.02f sec - %sbitrate %d kbps "LLD" packets written\nPadding: "LLD" packets (%g kbps) - "LLD" PES padded bytes (%g kbps)\n",
		((Double) dur_ms)/1000.0, ctx->rate ? "" : "average ", (u32) (bits/dur_ms), ctx->mux->tot_pck_sent,
		 ctx->mux->tot_pad_sent, (Double) (ctx->mux->tot_pad_sent*188*8.0/dur_ms) , ctx->mux->tot_pes_pad_bytes, (Double) (ctx->mux->tot_pes_pad_bytes*8.0/dur_ms)
	));

	while (gf_list_count(ctx->pids)) {
		M2Pid *tspid = gf_list_pop_back(ctx->pids);
		if (tspid->temi_url) gf_free(tspid->temi_url);
		if (tspid->esi.sl_config) gf_free(tspid->esi.sl_config);
		gf_free(tspid);
	}
	gf_list_del(ctx->pids);
	gf_m2ts_mux_del(ctx->mux);
	if (ctx->pack_buffer) gf_free(ctx->pack_buffer);
}

static const GF_FilterCapability TSMuxCaps[] =
{
	//first set of caps describe streams that need reframing (NAL, AAC ADTS)
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	//unframed streams only
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//for NAL-based, we want annexB format
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	//for m4vp2 we want DSI reinsertion
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	//for AAC we use the AAC->ADTS or AAC->LATM of the mux
	//static output cap file extension
	CAP_STRING(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_FILE_EXT, "ts|m2ts"),
	{},
	
	//for now don't accept files as input, although we could store them as items, to refine
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	//these caps are framed
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//exclude caps from above
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	//no RAW support for now
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


#define OFFS(_n)	#_n, offsetof(GF_TSMuxCtx, _n)
static const GF_FilterArgs TSMuxArgs[] =
{
	{ OFFS(breq), "buffer requirements in ms for input pids", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(pmt_id), "defines the ID of the first PMT to use in the mux.", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(rate), "target rate in bps of the multiplex. If not set, variable rate is used", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(pmt_rate), "interval between PMT in ms", GF_PROP_UINT, "200", NULL, GF_FALSE},
	{ OFFS(pat_rate), "interval between PAT in ms", GF_PROP_UINT, "200", NULL, GF_FALSE},
	{ OFFS(pcr_offset), "offsets all timestamps from PCR by V, in 90kHz. Default value is computed based on input media", GF_PROP_UINT, "-1", NULL, GF_FALSE},
	{ OFFS(mpeg4), "forces usage of MPEG-4 signaling (IOD and SL Config).\n"\
				"none disables 4on2. full sends AUs as SL packets over section for OD, section/pes for scene (cf bifs_pes)\n"\
				"scene sends only scene streams as 4on2 but uses regular PES without SL for audio and video\n"\
				, GF_PROP_UINT, "none", "none|full|scene", GF_FALSE},
	{ OFFS(pmt_version), "sets version number of the PMT", GF_PROP_UINT, "200", NULL, GF_FALSE},
	{ OFFS(disc), "sets the discontinuity marker for the first packet of each stream", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(repeat_rate), "interval in ms between two carousel send for MPEG-4 systems. Is overriden by carousel duration PID property if defined", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(repeat_img), "interval in ms between resending (as PES) of single-image streams. If 0, image data is sent once only", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(max_pcr), "sets max interval in ms between 2 PCR", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(nb_pack), "packs N TS packets in output packets", GF_PROP_UINT, "1", NULL, GF_FALSE},
	{ OFFS(pes_pack), "Sets AU to PES packing mode. audio will pack only multiple audio AUs in a PES, none make exactly one AU per PES, all will pack multiple AUs per PES for all streams", GF_PROP_UINT, "audio", "audio|none|all", GF_FALSE},
	{ OFFS(rt), "Forces real-time output", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(bifs_pes), "force sending BIFS streams as PES packets and not sections. copy mode disables timestamps in BIFS SL and only carries PES timestamps", GF_PROP_UINT, "off", "off|on|copy", GF_FALSE},
	{ OFFS(flush_rap), "force flushing mux program when RAP is found on video, and injects PAT and PMT before the next video PES begin", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(pcr_only), "enables PCR-only TS packets", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(pcr_init), "sets initial PCR value for the programs. Negative value implies random value is picked", GF_PROP_LSINT, "-1", NULL, GF_FALSE},
	{ OFFS(sid), "sets service ID for the program - see filter help", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(name), "sets service name for the program - see filter help", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(provider), "sets service provider name for the program - see filter help", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(sdt_rate), "interval in ms between two DVB SDT tables. If 0, SDT is disabled", GF_PROP_UINT, "0", NULL, GF_FALSE},

	{ OFFS(temi), "inserts TEMI time codes in adaptation field - see filter help", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(temi_delay), "sets delay in ms between two TEMI url descriptors", GF_PROP_UINT, "1000", NULL, GF_FALSE},
	{ OFFS(temi_offset), "sets offset in ms  to add to TEMI timecodes", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(temi_ntp), "inserts NTP timestamp in TEMI timeline descriptor", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(log_freq), "delay between logs for realtime mux", GF_PROP_UINT, "500", NULL, GF_FALSE},
	{ OFFS(latm), "uses LATM AAC encapsulation instead of regular ADTS", GF_PROP_BOOL, "false", NULL, GF_FALSE},

	{0}
};


GF_FilterRegister TSMuxRegister = {
	.name = "mxts",
	.description = "MPEG-2 Transport Stream muxer",
	.comment = "GPAC TS multiplexer selects PID for media streams using the PID of the PMT plus the stream index.\n"\
		"sid option is overriden by input PID property ServiceID if present\n"\
		"name option is overriden by input PID property ServiceName\n"\
		"provider option is overriden by input PID property ServiceProvider\n"\
		"\n"\
		"The temi option allows specifying a list of URLs or timeline IDs to insert in the program.\n"
		"Only a single TEMI timeline can be specified per PID.\n"
		"The syntax is\n"\
		"temi=\"url,4\": inserts a temi URL+timecode to the first stream of all progrrams and an external temi to the second stream of all programs\n"\
		"temi=\"#20#,4#10#URL\": inserts a temi URL+timecode to the first stream of program with ServiceID 10 and an external temi to the second stream of program with ServiceID 20\n"\
		"temi=\"#20#4#10#,,URL\": inserts a temi URL+timecode to the third stream of program with ServiceID 10 and an external temi to the first stream of program with ServiceID 20\n"\
	,
	.private_size = sizeof(GF_TSMuxCtx),
	.args = TSMuxArgs,
	.initialize = tsmux_initialize,
	.finalize = tsmux_finalize,
	SETCAPS(TSMuxCaps),
	.configure_pid = tsmux_configure_pid,
	.process = tsmux_process,
};


const GF_FilterRegister *tsmux_register(GF_FilterSession *session)
{
	return &TSMuxRegister;
}
