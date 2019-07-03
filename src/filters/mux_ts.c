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
#include <gpac/webvtt.h>

typedef struct
{
	u64 sap_time;
	u64 offset;
	u32 nb_pck;
	u32 sap_type;
	u64 min_pts_plus_one;
	u64 max_pts;
} TS_SIDX;

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
	s32 subs_sidx;

	//internal
	GF_FilterPid *opid;
	GF_FilterPid *idx_opid;
	GF_Filter *idx_filter;
	GF_M2TS_Mux *mux;

	GF_List *pids;

	Bool check_pcr;
	Bool update_mux;
	char *pack_buffer;
	u64 nb_pck;
	Bool init_buffering;
	u32 last_log_time;
	Bool pmt_update_pending;

	u32 dash_mode;
	Bool init_dash;
	u32 dash_seg_num;
	Bool wait_dash_flush;
	Bool dash_file_switch;
	Bool next_is_start;
	u32 nb_pck_in_seg;
	u64 pck_start_idx;

	char dash_file_name[GF_MAX_PATH];
	char idx_file_name[GF_MAX_PATH];

	//dash indexing
	u32 nb_sidx_entries, nb_sidx_alloc;
	TS_SIDX *sidx_entries;
	GF_BitStream *idx_bs;
	u32 nb_pck_in_file, nb_pck_first_sidx, ref_pid;
	u64 total_bytes_in;
} GF_TSMuxCtx;

typedef struct
{
	GF_ESInterface esi;
	GF_FilterPid *ipid;
	GF_M2TS_Mux_Stream *mstream;
	GF_M2TS_Mux_Program *prog;

	u32 sid;
	u32 codec_id;
	u32 pmt_pid;
	u32 nb_pck;
	Bool is_repeat;
	u32 nb_repeat_last;

	GF_TSMuxCtx *ctx;
	u32 last_cv;
	//ts media skip
	s32 media_delay;
	Bool done;

	u32 temi_id;
	char *temi_url;

	u32 last_temi_url;
	char af_data[188];

	Bool rewrite_odf;
	Bool has_seen_eods;
	u32 pck_duration;

	char *pck_data_buf;
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
	u64 cts, cts_diff;
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
				if (tspid->ctx->dash_mode)
					tspid->has_seen_eods = GF_TRUE;
			}
			return GF_OK;
		}
		if (tspid->ctx->dash_mode) {
			const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p && tspid->ctx->dash_seg_num && (tspid->ctx->dash_seg_num != p->value.uint)) {
				tspid->has_seen_eods = GF_TRUE;
				tspid->ctx->wait_dash_flush = GF_TRUE;
				tspid->ctx->dash_seg_num = p->value.uint;
				tspid->ctx->dash_file_name[0] = 0;

				p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
				if (p) {
					strcpy(tspid->ctx->dash_file_name, p->value.string);
					tspid->ctx->dash_file_switch = GF_TRUE;
				}
				return GF_OK;
			}

			if (tspid->has_seen_eods)
				return GF_OK;

			if (p)
				tspid->ctx->dash_seg_num = p->value.uint;

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_IDXFILENAME);
			if (p) {
				strcpy(tspid->ctx->idx_file_name, p->value.string);
			}

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_EODS);
			if (p && p->value.boolean) {
				tspid->has_seen_eods = GF_TRUE;
				tspid->ctx->wait_dash_flush = GF_TRUE;
				gf_filter_pid_drop_packet(tspid->ipid);
				return GF_OK;
			}
		}

		memset(&es_pck, 0, sizeof(GF_ESIPacket));
		es_pck.flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_AU_END | GF_ESI_DATA_HAS_CTS;
		es_pck.sap_type = gf_filter_pck_get_sap(pck);
		tspid->pck_duration = gf_filter_pck_get_duration(pck);
		cversion = gf_filter_pck_get_carousel_version(pck);
		if (cversion+1 == tspid->last_cv) {
			es_pck.flags |= GF_ESI_DATA_REPEAT;
		}
		tspid->last_cv = cversion+1;

		if (tspid->is_repeat)
			es_pck.flags |= GF_ESI_DATA_REPEAT;

		es_pck.cts = cts = gf_filter_pck_get_cts(pck);

		if (tspid->temi_id) {
			u64 ntp=0;
			//TOCHECK: do we want media timeline or composition timeline ?
			u64 tc = cts;
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
		es_pck.cts += tspid->prog->max_media_skip + tspid->media_delay;

		if (tspid->nb_repeat_last) {
			es_pck.cts += tspid->nb_repeat_last * ifce->timescale * tspid->ctx->repeat_img / 1000;
		}

		cts_diff = 0;
		if (tspid->prog->cts_offset) {
			cts_diff = tspid->prog->cts_offset;
			cts_diff *= tspid->esi.timescale;
			cts_diff /= 1000000;

			es_pck.cts += cts_diff;
		}

		es_pck.dts = es_pck.cts;
		dts = gf_filter_pck_get_dts(pck);
		if (dts != GF_FILTER_NO_TS) {
			es_pck.dts = dts;
			es_pck.dts += tspid->prog->max_media_skip + tspid->media_delay;

			if (es_pck.dts > es_pck.cts) {
				u64 diff;
				//we don't have reliable dts - double the diff should make sure we don't try to adjust too often
				diff = cts_diff = 2*(es_pck.dts - es_pck.cts);
				diff *= 1000000;
				diff /= tspid->esi.timescale;
				assert(tspid->prog->cts_offset <= diff);
				tspid->prog->cts_offset += (u32) diff;

				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TSMux] Packet CTS "LLU" is less than packet DTS "LLU", adjusting all CTS by %d / %d!\n", es_pck.cts, es_pck.dts, cts_diff, tspid->esi.timescale));

				es_pck.cts += cts_diff;
			}
			if (tspid->esi.stream_type!=GF_STREAM_VISUAL) {
				es_pck.dts += cts_diff;
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

		tspid->ctx->total_bytes_in += es_pck.data_len;

		if (tspid->pck_data_buf) gf_free(tspid->pck_data_buf);
		tspid->pck_data_buf = NULL;

		//drop formatting for TX3G
		if (tspid->codec_id == GF_CODECID_TX3G) {
			u16 len = es_pck.data[0];
			len<<=8;
			len |= es_pck.data[1];
			es_pck.data += 2;
			es_pck.data_len = len;
		}
		//serialize webvtt cue formatting for TX3G
		else if (tspid->codec_id == GF_CODECID_WEBVTT) {
			u32 i;
			u64 start_ts;
			void webvtt_write_cue(GF_BitStream *bs, GF_WebVTTCue *cue);
			GF_List *cues;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

			start_ts = es_pck.cts * 1000;
			start_ts /= tspid->esi.timescale;
			cues = gf_webvtt_parse_cues_from_data(es_pck.data, es_pck.data_len, start_ts);
			for (i = 0; i < gf_list_count(cues); i++) {
				GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, i);
				webvtt_write_cue(bs, cue);
				gf_webvtt_cue_del(cue);
			}
			gf_list_del(cues);
			gf_bs_get_content(bs, &es_pck.data, &es_pck.data_len);
			gf_bs_del(bs);
			tspid->pck_data_buf = es_pck.data;
		}
		//for TTML we keep the entire payload as a PES packet

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
			esd->decoderConfig->objectTypeIndication = stream->ifce->codecid;
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

	tspid->esi.codecid = tspid->codec_id;

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
	tspid->prog = prog;
}

static void tsmux_setup_temi(GF_TSMuxCtx *ctx, M2Pid *tspid)
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
	a_stream = tspid->mstream->program->streams;
	while (a_stream) {
		if (tspid->mstream == a_stream) break;
		st_idx++;
		a_stream = a_stream->next;
	}

	while (turl) {
		char *sep;
		if (turl[0]=='#') {
			sscanf(turl, "#%d#", &service_id);
			turl = strchr(turl+1, '#');
			if (!turl) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSMux] Invalid temi syntax, expecting #SID# but got nothing\n"));
				return;
			}
			turl += 1;
			idx = 0;
		}

		sep = strchr(turl, ',');
		if (sep) sep[0] = 0;

		if (strlen(turl) && (idx==st_idx) && (!service_id || (service_id == tspid->mstream->program->number) ))  {
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

static void tsmux_del_stream(M2Pid *tspid)
{
	if (tspid->temi_url) gf_free(tspid->temi_url);
	if (tspid->esi.sl_config) gf_free(tspid->esi.sl_config);
	if (tspid->pck_data_buf) gf_free(tspid->pck_data_buf);
	gf_free(tspid);
}

static GF_Err tsmux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 service_id, codec_id, streamtype;
	GF_M2TS_Mux_Stream *ts_stream;
	const GF_PropertyValue *p;
	GF_PropertyEntry *pe=NULL;
	M2Pid *tspid=NULL;
	char *sname, *pname;
	GF_M2TS_Mux_Program *prog;
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		tspid = gf_filter_pid_get_udta(pid);
		if (!tspid) return GF_OK;
		//remove stream - this will update PMT as well
		gf_m2ts_program_stream_remove(tspid->mstream);
		//destroy or object
		gf_list_del_item(ctx->pids, tspid);
		tsmux_del_stream(tspid);
		return GF_OK;
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
	//set output properties at init or reconfig
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("ts") );

	p = gf_filter_pid_get_info(pid, GF_PROP_PID_DASH_MODE, &pe);
	if (p) {
		if (!ctx->dash_mode && p->value.uint) ctx->init_dash = GF_TRUE;
		ctx->dash_mode = p->value.uint;
		if (ctx->dash_mode) {
			ctx->mux->flush_pes_at_rap = GF_TRUE;
			gf_m2ts_mux_set_initial_pcr(ctx->mux, 0);
		}
	}
	gf_filter_release_property(pe);

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
		u32 nb_progs;
		u32 pcr_offset=0;
		u32 pmt_id = ctx->pmt_id;

		if (!pmt_id) pmt_id = 100;
		nb_progs = gf_m2ts_mux_program_count(ctx->mux);
		if (nb_progs>1) {
			if (ctx->dash_mode) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSMux] Muxing several programs (%d) in DASH mode is not allowed\n", nb_progs+1));
				return GF_BAD_PARAM;
			}
			pmt_id += (nb_progs - 1) * 100;
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
		tspid->mstream = gf_m2ts_program_stream_add(prog, &tspid->esi, pes_pid, is_pcr, force_pes, GF_FALSE);
		tsmux_setup_temi(ctx, tspid);
	} else {
		tspid->codec_id = codec_id;
		tsmux_setup_esi(ctx, prog, tspid, streamtype);
		prog->pmt->table_needs_update = GF_TRUE;
		ctx->pmt_update_pending = GF_TRUE;
	}
	ctx->update_mux = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (p) {
		tspid->media_delay = p->value.sint;

		//compute max ts skip for this program
		prog->max_media_skip = 0;
		ts_stream = prog->streams;
		while (ts_stream) {
			M2Pid *atspid = ts_stream->ifce->input_udta;
			s32 media_skip = -atspid->media_delay;
			if (media_skip  > prog->max_media_skip)
				prog->max_media_skip = media_skip ;
			ts_stream = ts_stream->next;
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CTS_SHIFT);
	if (p) {
		u64 diff = p->value.uint;
		diff *= 1000000;
		diff /= tspid->esi.timescale;
		if (diff > tspid->prog->cts_offset)
			tspid->prog->cts_offset = (u32) diff;
	}
	return GF_OK;
}

static void tsmux_assign_pcr(GF_TSMuxCtx *ctx)
{
	GF_M2TS_Mux_Program *prog = ctx->mux->programs;
	while (prog) {
		GF_M2TS_Mux_Stream *stream;
		if (prog->pcr) {
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
		Bool buf_ok;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		buf_ok = gf_filter_pid_get_buffer_occupancy(pid, NULL, NULL, NULL, &buf);
		if (buf_ok && (buf < mbuf) && !gf_filter_pid_has_seen_eos(pid))
			return GF_FALSE;
	}
	ctx->init_buffering = GF_FALSE;
	gf_m2ts_mux_update_config(ctx->mux, GF_TRUE);
	return GF_TRUE;
}

static void tsmux_send_seg_event(GF_Filter *filter, GF_TSMuxCtx *ctx)
{
	GF_FilterEvent evt;
	u32 i;
	M2Pid *tspid = NULL;

	for (i=0; i<gf_list_count(ctx->pids); i++) {
		tspid = gf_list_get(ctx->pids, i);
		if (ctx->nb_sidx_entries) break;
		if (ctx->ref_pid == tspid->mstream->pid) break;
		tspid = NULL;
	}
	if (!tspid) tspid = gf_list_get(ctx->pids, 0);

	if (ctx->nb_sidx_entries) {
		GF_FilterPacket *idx_pck;
		u8 *output;
		Bool large_sidx = GF_FALSE;
		u32 segidx_size=0;
		u64 last_pck_dur = tspid->pck_duration;
		last_pck_dur *= 90000;
		last_pck_dur /= tspid->esi.timescale;

		if (ctx->sidx_entries[ctx->nb_sidx_entries-1].sap_time > 0xFFFFFFFFUL)
			large_sidx = GF_TRUE;
		if (ctx->sidx_entries[0].offset*188 > 0xFFFFFFFFUL)
			large_sidx = GF_TRUE;

		//styp box: 8(box) + 4(major) + 4(version) + 4(compat brand)
		segidx_size = 20;
		//sidx size: 12 (fullbox) + 8 + large ? 16 : 8 + 4 + nb_entries+12
		segidx_size += 24 +( large_sidx ? 16 : 8) + ctx->nb_sidx_entries*12;

		if (!ctx->idx_opid) {
			ctx->idx_filter = gf_filter_connect_destination(filter, ctx->idx_file_name, NULL);
			ctx->idx_opid = gf_filter_pid_new(filter);
			gf_filter_pid_set_property(ctx->idx_opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
			gf_filter_pid_set_property(ctx->idx_opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("*") );
		}
		idx_pck = gf_filter_pck_new_alloc(ctx->idx_opid, segidx_size, &output);

		if (!ctx->idx_bs) ctx->idx_bs = gf_bs_new(output, segidx_size, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->idx_bs, output, segidx_size);

		//write styp box
		gf_bs_write_u32(ctx->idx_bs, 20);
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','t','y','p') );
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','i','s','x') );
		gf_bs_write_u32(ctx->idx_bs, 0);
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','i','s','x') );

		//write sidx box
		gf_bs_write_u32(ctx->idx_bs, segidx_size - 20);
		gf_bs_write_u32(ctx->idx_bs, GF_4CC('s','i','d','x') );
		gf_bs_write_u8(ctx->idx_bs, large_sidx ? 1 : 0);
		gf_bs_write_int(ctx->idx_bs, 0, 24);
		//reference id
		gf_bs_write_u32(ctx->idx_bs, ctx->ref_pid);
		//timescale
		gf_bs_write_u32(ctx->idx_bs, 90000);
		if (large_sidx) {
			gf_bs_write_u64(ctx->idx_bs, ctx->sidx_entries[0].min_pts_plus_one-1);
			gf_bs_write_u64(ctx->idx_bs, ctx->sidx_entries[0].offset*188);
		} else {
			gf_bs_write_u32(ctx->idx_bs, (u32) ctx->sidx_entries[0].min_pts_plus_one-1);
			gf_bs_write_u32(ctx->idx_bs, (u32) ctx->sidx_entries[0].offset*188);
		}
		gf_bs_write_u16(ctx->idx_bs, 0);
		gf_bs_write_u16(ctx->idx_bs, ctx->nb_sidx_entries);
		for (i=0; i<ctx->nb_sidx_entries; i++) {
			u64 duration = ctx->sidx_entries[i].max_pts - (ctx->sidx_entries[i].min_pts_plus_one-1);
			if (i+1 == ctx->nb_sidx_entries) duration += last_pck_dur;

			gf_bs_write_int(ctx->idx_bs, 0, 1);
			gf_bs_write_int(ctx->idx_bs, ctx->sidx_entries[i].nb_pck * 188, 31);
			gf_bs_write_int(ctx->idx_bs, (u32) duration, 32);
			gf_bs_write_int(ctx->idx_bs, ctx->sidx_entries[i].sap_type ? 1 : 0, 1);
			gf_bs_write_int(ctx->idx_bs, ctx->sidx_entries[i].sap_type, 3);
			gf_bs_write_int(ctx->idx_bs, (u32) (ctx->sidx_entries[i].sap_time - (ctx->sidx_entries[i].min_pts_plus_one-1) ), 28);
		}
		gf_filter_pck_set_property(idx_pck, GF_PROP_PCK_FILENAME, &PROP_STRING(ctx->idx_file_name) );

		gf_filter_pck_send(idx_pck);
		ctx->nb_sidx_entries = 0;
	}


	GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, tspid->ipid);
	evt.seg_size.media_range_start = 188*ctx->pck_start_idx;
	evt.seg_size.media_range_end = evt.seg_size.media_range_start + 188*ctx->nb_pck_in_seg - 1;

	gf_filter_pid_send_event(tspid->ipid, &evt);
	ctx->nb_pck_in_seg = 0;
	ctx->nb_sidx_entries = 0;
}

static void tsmux_insert_sidx(GF_TSMuxCtx *ctx, Bool final_flush)
{
	if (ctx->subs_sidx<0) return;

	if (!ctx->ref_pid && ctx->mux->sap_inserted)
		ctx->ref_pid = ctx->mux->last_pid;
	if (!ctx->ref_pid) return;

	if (ctx->nb_sidx_entries) {
		TS_SIDX *tsidx = &ctx->sidx_entries[ctx->nb_sidx_entries-1];

		if (ctx->ref_pid == ctx->mux->last_pid) {
			if (!tsidx->min_pts_plus_one) tsidx->min_pts_plus_one = ctx->mux->last_pts + 1;
			else if (tsidx->min_pts_plus_one-1 > ctx->mux->last_pts) tsidx->min_pts_plus_one = ctx->mux->last_pts + 1;

			if (tsidx->max_pts < ctx->mux->last_pts) tsidx->max_pts = ctx->mux->last_pts;
		}

		if (!final_flush && !ctx->mux->sap_inserted) return;

		tsidx->nb_pck = ctx->nb_pck_in_seg - tsidx->nb_pck;
	}

	if (final_flush) return;
	if (!ctx->mux->sap_inserted) return;

	if (ctx->nb_sidx_entries == ctx->nb_sidx_alloc) {
		ctx->nb_sidx_alloc += 10;
		ctx->sidx_entries = gf_realloc(ctx->sidx_entries, sizeof(TS_SIDX)*ctx->nb_sidx_alloc);
	}
	ctx->sidx_entries[ctx->nb_sidx_entries].sap_time = ctx->mux->sap_time;
	ctx->sidx_entries[ctx->nb_sidx_entries].sap_type = ctx->mux->sap_type;
	ctx->sidx_entries[ctx->nb_sidx_entries].min_pts_plus_one  = ctx->mux->sap_time + 1;
	ctx->sidx_entries[ctx->nb_sidx_entries].max_pts  = ctx->mux->sap_time;
	ctx->sidx_entries[ctx->nb_sidx_entries].sap_time = ctx->mux->sap_time;
	ctx->sidx_entries[ctx->nb_sidx_entries].nb_pck = ctx->nb_sidx_entries ? ctx->nb_pck_in_seg : 0;
	ctx->sidx_entries[ctx->nb_sidx_entries].offset = ctx->nb_sidx_entries ? 0 : ctx->nb_pck_first_sidx;
	ctx->nb_sidx_entries ++;
}

static GF_Err tsmux_process(GF_Filter *filter)
{
	const char *ts_pck;
	u32 nb_pck_in_pack, nb_pck_in_call;
	u32 status, usec_till_next;
	GF_FilterPacket *pck;
	GF_TSMuxCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->check_pcr) {
		ctx->check_pcr = GF_FALSE;
		tsmux_assign_pcr(ctx);
	}

	if (ctx->init_buffering && !tsmux_init_buffering(filter, ctx)) return GF_OK;

	if (ctx->init_dash) {
		u32 i, count = gf_list_count(ctx->pids);
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tspid->ipid);
			if (!pck) return GF_OK;
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p)
				tspid->ctx->dash_seg_num = p->value.uint;
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (p)
				strcpy(tspid->ctx->dash_file_name, p->value.string);
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_IDXFILENAME);
			if (p)
				strcpy(tspid->ctx->idx_file_name, p->value.string);
		}
		ctx->init_dash = GF_FALSE;
		ctx->next_is_start = GF_TRUE;
		ctx->wait_dash_flush = GF_FALSE;
	}

	if (ctx->update_mux) {
		gf_m2ts_mux_update_config(ctx->mux, GF_FALSE);
	}


	if (ctx->wait_dash_flush) {
		u32 i, done=0, count = gf_list_count(ctx->pids);
		for (i=0; i<count; i++) {
			M2Pid *tspid = gf_list_get(ctx->pids, i);
			if (tspid->has_seen_eods) done++;
		}

		if (done==count) {
			for (i=0; i<count; i++) {
				M2Pid *tspid = gf_list_get(ctx->pids, i);
				tspid->has_seen_eods = 0;
			}
			ctx->dash_seg_num = 0;
			ctx->wait_dash_flush = GF_FALSE;
			ctx->next_is_start = ctx->dash_file_switch;
			ctx->mux->force_pat = GF_TRUE;
			ctx->dash_file_switch = GF_FALSE;
			if (ctx->nb_pck_in_seg) {
				tsmux_insert_sidx(ctx, GF_TRUE);
				tsmux_send_seg_event(filter, ctx);
			}

			ctx->nb_pck_in_seg = 0;
			ctx->pck_start_idx = ctx->nb_pck;
			if (ctx->next_is_start) ctx->nb_pck_in_file = 0;
			ctx->nb_pck_first_sidx = ctx->nb_pck_in_file;
		}
	}

	nb_pck_in_call = 0;
	nb_pck_in_pack=0;
	while (1) {
		u8 *output;
		u32 osize;
		Bool is_pack_flush = GF_FALSE;

		ts_pck = gf_m2ts_mux_process(ctx->mux, &status, &usec_till_next);
		if (ts_pck == NULL) {
			if (!nb_pck_in_pack)
				break;
			ts_pck = (const char *) ctx->pack_buffer;
			is_pack_flush = GF_TRUE;
		} else {

			tsmux_insert_sidx(ctx, GF_FALSE);

			if (ctx->nb_pack>1) {
				memcpy(ctx->pack_buffer + 188 * nb_pck_in_pack, ts_pck, 188);
				nb_pck_in_pack++;

				if (nb_pck_in_pack < ctx->nb_pack)
					continue;

				ts_pck = (const char *) ctx->pack_buffer;
			} else {
				nb_pck_in_pack = 1;
			}
		}
		osize = nb_pck_in_pack * 188;
		pck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
		memcpy(output, ts_pck, osize);
		gf_filter_pck_set_framing(pck, ctx->nb_pck ? ctx->next_is_start : GF_TRUE, (status==GF_M2TS_STATE_EOS) ? GF_TRUE : GF_FALSE);

		if (ctx->next_is_start && ctx->dash_mode) {
			gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->dash_seg_num) );
			if (ctx->dash_file_name[0])
				gf_filter_pck_set_property(pck, GF_PROP_PCK_FILENAME, &PROP_STRING(ctx->dash_file_name) ) ;

			ctx->dash_file_name[0] = 0;
			ctx->next_is_start = GF_FALSE;
		}

		gf_filter_pck_send(pck);
		ctx->nb_pck += nb_pck_in_pack;
		ctx->nb_pck_in_seg++;
		ctx->nb_pck_in_file++;
		nb_pck_in_call += nb_pck_in_pack;
		nb_pck_in_pack = 0;

		if (is_pack_flush)
			break;

		if (status>=GF_M2TS_STATE_PADDING) {
			break;
		}
		if (nb_pck_in_call>100)
			break;
	}
	if (gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];
		if (status==GF_M2TS_STATE_EOS) {
			Double ohead = 0;
			u64 total_bytes_out = ctx->nb_pck;
			total_bytes_out *= 188;

			if (ctx->total_bytes_in) ohead =  ((Double) (total_bytes_out - ctx->total_bytes_in)*100 / ctx->total_bytes_in);

			sprintf(szStatus, "done - TS clock % 6d ms bitrate %d kbps - bytes "LLD" in "LLD" out overhead %02.02f%%", gf_m2ts_get_ts_clock(ctx->mux), ctx->mux->bit_rate/1000, ctx->total_bytes_in, total_bytes_out, ohead);
			gf_filter_update_status(filter, 10000, szStatus);
		} else {

			sprintf(szStatus, "sysclock % 6d ms TS clock % 6d ms bitrate % 8d kbps", gf_m2ts_get_sys_clock(ctx->mux), gf_m2ts_get_ts_clock(ctx->mux), ctx->mux->bit_rate/1000);
			gf_filter_update_status(filter, -1, szStatus);
		}
	}

	if (status==GF_M2TS_STATE_EOS) {
		gf_filter_pid_set_eos(ctx->opid);
		if (ctx->nb_pck_in_seg) {
			tsmux_insert_sidx(ctx, GF_TRUE);
			tsmux_send_seg_event(filter, ctx);
		}
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
				gf_filter_ask_rt_reschedule(filter, (u32) sleep_for);
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

	if (gf_sys_is_test_mode() && ctx->pcr_init<0)
		ctx->pcr_init = 1000000;

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

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		gf_m2ts_get_sys_clock(ctx->mux);
	}
#endif
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
		tsmux_del_stream(tspid);
	}
	gf_list_del(ctx->pids);
	gf_m2ts_mux_del(ctx->mux);
	if (ctx->pack_buffer) gf_free(ctx->pack_buffer);
	if (ctx->sidx_entries) gf_free(ctx->sidx_entries);
	if (ctx->idx_bs) gf_bs_del(ctx->idx_bs);
}

static const GF_FilterCapability TSMuxCaps[] =
{
	//first set of caps describe streams that need reframing (NAL)
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	//unframed streams only
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_STATIC_OPT, 	GF_PROP_PID_DASH_MODE, 0),
	//for NAL-based, we want annexB format
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	//for m4vp2 we want DSI reinsertion
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	//for AAC we use the AAC->ADTS or AAC->LATM of the mux, so don't insert here

	//static output cap file extension
	CAP_UINT(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT_STATIC,  GF_PROP_PID_FILE_EXT, "ts|m2ts"),
	{0},
	
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
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	//no RAW support for now
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


#define OFFS(_n)	#_n, offsetof(GF_TSMuxCtx, _n)
static const GF_FilterArgs TSMuxArgs[] =
{
	{ OFFS(breq), "buffer requirements in ms for input pids", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pmt_id), "define the ID of the first PMT to use in the mux", GF_PROP_UINT, "100", NULL, 0},
	{ OFFS(rate), "target rate in bps of the multiplex. If not set, variable rate is used", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(pmt_rate), "interval between PMT in ms", GF_PROP_UINT, "200", NULL, 0},
	{ OFFS(pat_rate), "interval between PAT in ms", GF_PROP_UINT, "200", NULL, 0},
	{ OFFS(pcr_offset), "offset all timestamps from PCR by V, in 90kHz. Default value is computed based on input media", GF_PROP_UINT, "-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mpeg4), "force usage of MPEG-4 signaling (IOD and SL Config).\n"\
				"- none: disables 4on2\n"\
				"- full: sends AUs as SL packets over section for OD, section/pes for scene (cf bifs_pes)\n"\
				"- scene: sends only scene streams as 4on2 but uses regular PES without SL for audio and video"\
				, GF_PROP_UINT, "none", "none|full|scene", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(pmt_version), "set version number of the PMT", GF_PROP_UINT, "200", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(disc), "set the discontinuity marker for the first packet of each stream", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(repeat_rate), "interval in ms between two carousel send for MPEG-4 systems. Is overriden by carousel duration PID property if defined", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(repeat_img), "interval in ms between resending (as PES) of single-image streams. If 0, image data is sent once only", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(max_pcr), "set max interval in ms between 2 PCR", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nb_pack), "pack N TS packets in output packets", GF_PROP_UINT, "4", NULL, 0},
	{ OFFS(pes_pack), "set AU to PES packing mode.\n"\
		"- audio: will pack only multiple audio AUs in a PES\n"\
		"- none: make exactly one AU per PES\n"\
		"- all: will pack multiple AUs per PES for all streams", GF_PROP_UINT, "audio", "audio|none|all", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rt), "use real-time output", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(bifs_pes), "select BIFS streams packetization (PES vs sections)\n"
	"- on: uses BIFS PES\n"
	"- off: uses BIFS sections\n"
	"- copy: uses BIFS PES but removes timestamps in BIFS SL and only carries PES timestamps", GF_PROP_UINT, "off", "off|on|copy", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(flush_rap), "force flushing mux program when RAP is found on video, and injects PAT and PMT before the next video PES begin", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pcr_only), "enable PCR-only TS packets", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(pcr_init), "set initial PCR value for the programs. Negative value implies random value is picked", GF_PROP_LSINT, "-1", NULL, 0},
	{ OFFS(sid), "set service ID for the program - see filter help", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(name), "set service name for the program - see filter help", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(provider), "set service provider name for the program - see filter help", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(sdt_rate), "interval in ms between two DVB SDT tables. If 0, SDT is disabled", GF_PROP_UINT, "0", NULL, 0},

	{ OFFS(temi), "insert TEMI time codes in adaptation field - see filter help", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(temi_delay), "set delay in ms between two TEMI url descriptors", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(temi_offset), "set offset in ms  to add to TEMI timecodes", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(temi_ntp), "insert NTP timestamp in TEMI timeline descriptor", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(log_freq), "delay between logs for realtime mux", GF_PROP_UINT, "500", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(latm), "use LATM AAC encapsulation instead of regular ADTS", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(subs_sidx), "number of subsegments per sidx. negative value disables sidx", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister TSMuxRegister = {
	.name = "m2tsmx",
	GF_FS_SET_DESCRIPTION("MPEG-2 TS muxer")
	GF_FS_SET_HELP("GPAC TS multiplexer selects M2TS PID for media streams using the PID of the PMT plus the stream index.\n"\
	 	"For example, default config creates the first program with a PMT PID 100, the first stream will have a PID of 101.\n"\
		"Streams are grouped in programs based on input PID property ServiceID if present. If absent, stream will go in the program with service ID as indicated by [-sid]() option.\n"\
		"[-name]() option is overriden by input PID property ServiceName\n"\
		"[-provider]() option is overriden by input PID property ServiceProvider\n"\
		"\n"\
		"# Time and External Media Information (TEMI)\n"\
		"The [-temi]() option allows specifying a list of URLs or timeline IDs to insert in the program.\n"
		"Only a single TEMI timeline can be specified per PID.\n"
		"The syntax is a comma-separated list of one or more TEMI description, each of them separated by '#'\n"
		"Each TEMI description is formated as #ServiceID#ID_OR_URL, with:\n"\
		"- ServiceID: optional, number indicating the target serviceID\n"\
		"- ID_OR_URL: If numbern indicates the TEMI ID to use for external timeline. Otherwise, gives the URL to insert\n"\
		"Each comma-separated description designs a stream index in the target service.\n"
		"EX temi=\"url\"\nIinserts a TEMI URL+timecode in the first stream of all programs\n"\
		"EX temi=\"url,4\"\nInserts a TEMI URL+timecode in the first stream of all programs and an external TEMI with ID 4 in the second stream of all programs\n"\
		"EX temi=\"#20#4,#10#URL\"\nInserts an external TEMI with ID 4 in the first stream of program with ServiceID 20 and a TEMI URL to the second stream of program with ServiceID 10\n"\
		"EX temi=\"#20#4,,#10#URL\"\nInserts an external TEMI with ID 4 in the first stream of program with ServiceID 20 and a TEMI URL to the third stream of program with ServiceID 10 (and nothing on second stream)\n"\
		"\n"\
		"In DASH mode, the PCR is always initialized at 0, and [-flush_rap]() is automatically set.\n"
	)
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
