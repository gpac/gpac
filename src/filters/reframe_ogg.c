/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / XIPH OGG demux filter
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
#include <gpac/list.h>
#include <gpac/bitstream.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_OGG)
#include <ogg/ogg.h>
#include <gpac/avparse.h>


typedef struct
{
	u64 pos;
	Double duration;
} OGGIdx;

typedef struct
{
	u32 streamType;	/*MPEG-4 streamType*/
	u32 num_init_headers;
	u32 sample_rate, bitrate;

	u32 theora_kgs;
	Float frame_rate;
	u32 frame_rate_base;

	u32 type;
} OGGInfo;

typedef struct
{
	//only one output pid declared
	GF_FilterPid *opid;

	ogg_stream_state os;
	u32 serial_no;
	/*DSI for ogg in mp4 - cf constants.h*/
	char *dsi;
	u32 dsi_len;

	OGGInfo info;
	Bool got_headers;
	s64 seek_granule, last_granule;

	Bool is_running;
	u32 parse_headers;

	Bool eos_detected, map_time;

	u32 ogg_ts;

	GF_VorbisParser vp;
} OGGStream;

typedef struct
{

	//filter args
	Double index_dur;

	//only one input pid declared
	GF_FilterPid *ipid;

	u64 file_pos;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek, seek_file;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	OGGIdx *indexes;
	u32 index_alloc_size, index_size;

	GF_List *streams;

	u32 nb_playing, kill_demux, do_seek, init_remain;

	/*ogg ogfile state*/
	ogg_sync_state oy;

	Double dur;
	u32 data_buffer_ms;
} GF_OGGDmxCtx;


void oggdmx_signal_eos(GF_OGGDmxCtx *ctx)
{
	OGGStream *st;
	u32 i=0;
	while ((st = (OGGStream*)gf_list_enum(ctx->streams, &i))) {
		gf_filter_pid_set_eos(st->opid);
	}
}

#define OGG_BUFFER_SIZE 4096

#if 0


static GF_ObjectDescriptor *OGG_GetOD(OGGStream *st)
{
	GF_ObjectDescriptor *od;
	GF_ESD *esd;

	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	od->objectDescriptorID = (st->info.streamType==GF_STREAM_AUDIO) ? 3 : 2;
	esd = gf_odf_desc_esd_new(0);
	esd->decoderConfig->streamType = st->info.streamType;
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_OGG;
	esd->decoderConfig->avgBitrate = st->info.bitrate;
	esd->ESID = st->ESID;

	esd->slConfig->useTimestampsFlag = 1;
	esd->slConfig->useAccessUnitEndFlag = esd->slConfig->useAccessUnitStartFlag = 1;
	esd->slConfig->timestampResolution = st->info.sample_rate ? st->info.sample_rate : (u32) (1000*st->info.frame_rate);
	if (st->info.sample_rate) esd->slConfig->hasRandomAccessUnitsOnlyFlag = 1;
	else esd->slConfig->useRandomAccessPointFlag = 1;

	esd->decoderConfig->decoderSpecificInfo->dataLength = st->dsi_len;
	esd->decoderConfig->decoderSpecificInfo->data = (char *) gf_malloc(sizeof(char) * st->dsi_len);
	memcpy(esd->decoderConfig->decoderSpecificInfo->data, st->dsi, sizeof(char) * st->dsi_len);
	gf_list_add(od->ESDescriptors, esd);
	return od;
}
#endif


static OGGStream *OGG_FindStreamForPage(GF_OGGDmxCtx *ctx, ogg_page *oggpage)
{
	u32 i, count;
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		OGGStream *st = (OGGStream*)gf_list_get(ctx->streams, i);
		if (ogg_stream_pagein(&st->os, oggpage) == 0) return st;
	}
	return NULL;
}


u64 OGG_GranuleToTime(OGGInfo *cfg, s64 granule)
{
	if (cfg->sample_rate) {
		return granule;
	}
	if (cfg->frame_rate) {
		s64 iframe = granule>>cfg->theora_kgs;
		s64 pframe = granule - (iframe<<cfg->theora_kgs);
		pframe += iframe;
		pframe *= cfg->frame_rate_base;
		return (u64) (pframe / cfg->frame_rate);
	}
	return 0;
}

Double OGG_GranuleToMediaTime(OGGInfo *cfg, s64 granule)
{
	Double t = (Double) (s64) OGG_GranuleToTime(cfg, granule);
	if (cfg->sample_rate) t /= cfg->sample_rate;
	else t /= cfg->frame_rate_base;
	return t;
}


static void OGG_GetStreamInfo(ogg_packet *oggpacket, OGGInfo *info)
{
	oggpack_buffer opb;

	memset(info, 0, sizeof(OGGInfo));

	/*vorbis*/
	if ((oggpacket->bytes >= 7) && !strncmp((char *) &oggpacket->packet[1], "vorbis", 6)) {
		info->streamType = GF_STREAM_AUDIO;
		oggpack_readinit(&opb, oggpacket->packet, oggpacket->bytes);
		oggpack_adv( &opb, 88);
		oggpack_adv( &opb, 8);	/*nb chan*/
		info->sample_rate = oggpack_read(&opb, 32);
		oggpack_adv( &opb, 32);	/*max rate*/
		info->bitrate = oggpack_read(&opb, 32);
		info->num_init_headers = 3;
		info->type = GPAC_OTI_VORBIS;
	}
	/*speex*/
	else if ((oggpacket->bytes >= 7) && !strncmp((char *) &oggpacket->packet[0], "Speex", 5)) {
		info->streamType = GF_STREAM_AUDIO;
		oggpack_readinit(&opb, oggpacket->packet, oggpacket->bytes);
		oggpack_adv(&opb, 224);
		oggpack_adv(&opb, 32);
		oggpack_adv( &opb, 32);
		info->sample_rate = oggpack_read(&opb, 32);
		info->type = GPAC_OTI_SPEEX;
		info->num_init_headers = 1;
	}
	/*flac*/
	else if ((oggpacket->bytes >= 4) && !strncmp((char *) &oggpacket->packet[0], "fLaC", 4)) {
		info->streamType = GF_STREAM_AUDIO;
		info->type = 3;
		info->num_init_headers = GPAC_OTI_FLAC;
	}
	/*theora*/
	else if ((oggpacket->bytes >= 7) && !strncmp((char *) &oggpacket->packet[1], "theora", 6)) {
		GF_BitStream *bs;
		u32 fps_numerator, fps_denominator, keyframe_freq_force;

		info->streamType = GF_STREAM_VISUAL;
		info->type = GPAC_OTI_THEORA;
		bs = gf_bs_new((char *) oggpacket->packet, oggpacket->bytes, GF_BITSTREAM_READ);
		gf_bs_read_int(bs, 56);
		gf_bs_read_int(bs, 8); /* major version num */
		gf_bs_read_int(bs, 8); /* minor version num */
		gf_bs_read_int(bs, 8); /* subminor version num */
		gf_bs_read_int(bs, 16) /*<< 4*/; /* width */
		gf_bs_read_int(bs, 16) /*<< 4*/; /* height */
		gf_bs_read_int(bs, 24); /* frame width */
		gf_bs_read_int(bs, 24); /* frame height */
		gf_bs_read_int(bs, 8); /* x offset */
		gf_bs_read_int(bs, 8); /* y offset */
		fps_numerator = gf_bs_read_u32(bs);
		fps_denominator = gf_bs_read_u32(bs);
		gf_bs_read_int(bs, 24); /* aspect_numerator */
		gf_bs_read_int(bs, 24); /* aspect_denominator */
		gf_bs_read_int(bs, 8); /* colorspace */
		gf_bs_read_int(bs, 24);/* bitrate */
		gf_bs_read_int(bs, 6); /* quality */

		keyframe_freq_force = 1 << gf_bs_read_int(bs, 5);
		info->theora_kgs = 0;
		keyframe_freq_force--;
		while (keyframe_freq_force) {
			info->theora_kgs ++;
			keyframe_freq_force >>= 1;
		}
		info->frame_rate = ((Float)fps_numerator) / fps_denominator;
		info->num_init_headers = 3;
		gf_bs_del(bs);
		info->frame_rate_base = fps_denominator;
	}
}

static void OGG_ResetupStream(GF_OGGDmxCtx *ctx, OGGStream *st, ogg_page *oggpage)
{
	ogg_stream_clear(&st->os);
	ogg_stream_init(&st->os, st->serial_no);
	ogg_stream_pagein(&st->os, oggpage);
	st->parse_headers = st->info.num_init_headers;

	if (st->info.sample_rate) {
		st->seek_granule = (s64) (ctx->start_range * st->info.sample_rate);
	} else if (st->info.frame_rate) {
		s64 seek = (s64) (ctx->start_range * st->info.frame_rate) - 1;
		if (seek<0) seek=0;
		st->seek_granule = (seek)<<st->info.theora_kgs;
	}
	st->last_granule = -1;
}

static void OGG_DeclarePID(GF_Filter *filter, GF_OGGDmxCtx *ctx, OGGStream *st)
{
	if (!st->opid) {
		st->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(st->info.streamType) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_OTI, &PROP_UINT(st->info.type) );
	gf_filter_pid_set_info(st->opid, GF_PROP_PID_BITRATE, &PROP_UINT(st->info.bitrate) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(st->info.sample_rate ? st->info.sample_rate : (u32) (1000*st->info.frame_rate) ) );

	gf_filter_pid_set_info(st->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(st->dsi, st->dsi_len) );
}

static void OGG_NewStream(GF_Filter *filter, GF_OGGDmxCtx *ctx, ogg_page *oggpage)
{
	ogg_packet oggpacket;
	u32 serial_no, i;
	OGGStream *st;

	/*reannounce of stream (caroussel in live streams) - until now I don't think icecast uses this*/
	serial_no = ogg_page_serialno(oggpage);
	i=0;
	while ((st = (OGGStream*)gf_list_enum(ctx->streams, &i))) {
		if (st->serial_no==serial_no) {
			OGG_ResetupStream(ctx, st, oggpage);
			return;
		}
	}

	/*look if we have the same stream defined (eg, reuse first stream dead with same header page)*/
	i=0;
	while ((st = (OGGStream*)gf_list_enum(ctx->streams, &i))) {
		if (st->eos_detected) {
			ogg_stream_state os;
			ogg_stream_init(&os, serial_no);
			ogg_stream_pagein(&os, oggpage);
			ogg_stream_packetpeek(&os, &oggpacket);
			if (st->dsi && !memcmp(st->dsi, oggpacket.packet, oggpacket.bytes)) {
				ogg_stream_clear(&os);
				st->serial_no = serial_no;
				OGG_ResetupStream(ctx, st, oggpage);
				return;
			}
			ogg_stream_clear(&os);
			/*nope streams are different, signal eos on this one*/
//			gf_service_send_packet(ctx->service, st->ch, NULL, 0, NULL, GF_EOS);
		}
	}

	GF_SAFEALLOC(st, OGGStream);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OGG] Failed to allocate stream for demux\n"));
		return;
	}
	st->serial_no = serial_no;
	ogg_stream_init(&st->os, st->serial_no);
	ogg_stream_pagein(&st->os, oggpage);

	ogg_stream_packetpeek(&st->os, &oggpacket);
	OGG_GetStreamInfo(&oggpacket, &st->info);

	gf_list_add(ctx->streams, st);
	st->parse_headers = st->info.num_init_headers;
	if (st->parse_headers) ctx->init_remain++;

	if (st->info.sample_rate) {
		st->seek_granule = (s64) (ctx->start_range * st->info.sample_rate);
	} else if (st->info.frame_rate) {
		s64 seek = (s64) (ctx->start_range * st->info.frame_rate) - 1;
		if (seek<0) seek=0;
		st->seek_granule = (seek)<<st->info.theora_kgs;
	}
	st->last_granule = -1;

	if (st->got_headers) {
		OGG_DeclarePID(filter, ctx, st);
	}
}

/*GFINLINE*/
void OGG_SendPackets(GF_OGGDmxCtx *ctx, OGGStream *st, ogg_packet *oggpacket)
{
	char *data;
	GF_FilterPacket *pck;

	if (!st->opid) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OGG] Channel %d packet before configure done - discarding\n", st->serial_no));
		return;
	}

	if (st->info.type==GPAC_OTI_THEORA) {
		oggpack_buffer opb;
		oggpackB_readinit(&opb, oggpacket->packet, oggpacket->bytes);
		/*not a new frame*/
		if (oggpackB_read(&opb, 1) != 0) return;

		pck = gf_filter_pck_new_alloc(st->opid, oggpacket->bytes, &data);
		memcpy(data, (char *) oggpacket->packet, oggpacket->bytes);
		gf_filter_pck_set_cts(pck, st->ogg_ts);
		gf_filter_pck_set_sap(pck, oggpackB_read(&opb, 1) );
		st->ogg_ts += 1000;
	} else {
		pck = gf_filter_pck_new_alloc(st->opid, oggpacket->bytes, &data);
		memcpy(data, (char *) oggpacket->packet, oggpacket->bytes);
		gf_filter_pck_set_cts(pck, st->ogg_ts);
		gf_filter_pck_set_sap(pck, 1);

		if (st->info.type==GPAC_OTI_VORBIS) {
			st->ogg_ts += gf_vorbis_check_frame(&st->vp, (char *) oggpacket->packet, oggpacket->bytes);
		}
	}
	gf_filter_pck_send(pck);
}

#if 0
static u32 OggDemux(void *par)
{
	GF_NetworkCommand com;
	Bool go;
	u32 i, count;
	OGGStream *st;
	GF_OGGDmxCtx *ctx = (GF_OGGDmxCtx *) par;

	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_CHAN_BUFFER_QUERY;

	if (ctx->needs_connection) {
		ctx->needs_connection = GF_FALSE;
		gf_service_connect_ack(ctx->service, NULL, GF_OK);
	}

	ogg_sync_init(&ctx->oy);


	while (!ctx->kill_demux) {
		OGG_Process(ctx);

		/*wait for stream connection*/
		while (!ctx->kill_demux && !ctx->nb_playing) {
			gf_sleep(20);
		}

		/*(re)starting, seek*/
		if (ctx->do_seek) {
			ctx->do_seek = 0;
			ogg_sync_clear(&ctx->oy);
			ogg_sync_init(&ctx->oy);
//			OGG_SendStreams(ctx);

			if (ctx->ogfile) {
				u32 seek_to = 0;
				if (ctx->dur) seek_to = (u32) (ctx->file_size * (ctx->start_range/ctx->dur) * 0.6f);
				if ((s32) seek_to > gf_ftell(ctx->ogfile) ) {
					gf_fseek(ctx->ogfile, seek_to, SEEK_SET);
				} else {
					gf_fseek(ctx->ogfile, 0, SEEK_SET);
				}
			}
		}


		/*sleep untill the buffer occupancy is too low - note that this work because all streams in this
		demuxer are synchronized*/
		go = ctx->nb_playing;
		while (go && !ctx->kill_demux) {
			count = gf_list_count(ctx->streams);
			for (i=0; i<count; i++) {
				st = (OGGStream*)gf_list_get(ctx->streams, i);
				if (!st->ch) continue;
				com.base.on_channel = st->ch;
				gf_service_command(ctx->service, &com, GF_OK);
				if (com.buffer.occupancy < ctx->data_buffer_ms) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[OGG] channel %d needs fill (%d ms data, %d max buffer)\n", st->ESID, com.buffer.occupancy, ctx->data_buffer_ms));
					go = GF_FALSE;
					break;
				}
			}
			if (!i || !ctx->nb_playing) break;
			gf_sleep(0);
		}
	}
	ogg_sync_clear(&ctx->oy);
	ctx->kill_demux=2;
	return 0;
}

/*get streams & duration*/
Bool OGG_CheckFile(GF_OGGDmxCtx *ctx)
{
	OGGInfo info, the_info;
	ogg_page oggpage;
	ogg_packet oggpacket;
	ogg_stream_state os, the_os;
	u64 max_gran;
	Bool has_stream = GF_FALSE;
	gf_fseek(ctx->ogfile, 0, SEEK_SET);

	ogg_sync_init(&ctx->oy);
	memset(&the_info, 0, sizeof(OGGInfo));
	max_gran = 0;
	while (1) {
		if (!OGG_ReadPage(ctx, &oggpage)) break;

		if (ogg_page_bos(&oggpage)) {
			ogg_stream_init(&os, ogg_page_serialno(&oggpage));
			if (ogg_stream_pagein(&os, &oggpage) >= 0 ) {
				ogg_stream_packetpeek(&os, &oggpacket);
				if (ogg_stream_pagein(&os, &oggpage) >= 0 ) {
					ogg_stream_packetpeek(&os, &oggpacket);
					OGG_GetStreamInfo(&oggpacket, &info);
				}
				if (!has_stream) {
					has_stream = GF_TRUE;
					ogg_stream_init(&the_os, ogg_page_serialno(&oggpage));
					the_info = info;
				}
			}
			ogg_stream_clear(&os);
			continue;
		}
		if (has_stream && (ogg_stream_pagein(&the_os, &oggpage) >= 0) ) {
			while (ogg_stream_packetout(&the_os, &oggpacket ) > 0 ) {
				if ((oggpacket.granulepos>=0) && ((u64) oggpacket.granulepos>max_gran) ) {
					max_gran = oggpacket.granulepos;
				}
			}
		}
	}
	ogg_sync_clear(&ctx->oy);
	ctx->file_size = gf_ftell(ctx->ogfile);
	gf_fseek(ctx->ogfile, 0, SEEK_SET);
	ctx->dur = 0;
	if (has_stream) {
		ogg_stream_clear(&the_os);
		ctx->dur = (Double) (s64) OGG_GranuleToTime(&the_info, max_gran);
		if (the_info.sample_rate) ctx->dur /= the_info.sample_rate;
		else ctx->dur /= the_info.frame_rate_base;
	}
	return has_stream;
}


static GF_Err OGG_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	OGGStream *st;
	u32 i;
	GF_OGGDmxCtx *ctx = (GF_OGGDmxCtx*)plug->priv;

	if (!com->base.on_channel) {
		/*if live session we may cache*/
		if (ctx->is_live && (com->command_type==GF_NET_IS_CACHABLE)) return GF_OK;
		return GF_NOT_SUPPORTED;
	}
	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		/*no way to demux streams independently, and we keep OD as dynamic ogfile to handle
		chained streams*/
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		//live: return GF_NOT_SUPPORTED;
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.min = com->buffer.max = 0;
		if (ctx->is_live) com->buffer.max = ctx->data_buffer_ms;
		return GF_OK;
	case GF_NET_CHAN_SET_PADDING:
		return GF_NOT_SUPPORTED;

	case GF_NET_CHAN_DURATION:
		com->duration.duration = ctx->dur;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		ctx->start_range = com->play.start_range;
		ctx->end_range = com->play.end_range;
		i=0;
		while ((st = (OGGStream*)gf_list_enum(ctx->streams, &i))) {
			if (st->ch == com->base.on_channel) {
				st->is_running = GF_TRUE;
				st->map_time = ctx->dur ? GF_TRUE : GF_FALSE;
				if (!ctx->nb_playing) ctx->do_seek = 1;
				ctx->nb_playing ++;
				break;
			}
		}
		/*recfg duration in case*/
		if (!ctx->is_remote && ctx->dur) {
			GF_NetworkCommand rcfg;
			rcfg.base.on_channel = NULL;
			rcfg.base.command_type = GF_NET_CHAN_DURATION;
			rcfg.duration.duration = ctx->dur;
			gf_service_command(ctx->service, &rcfg, GF_OK);
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		i=0;
		while ((st = (OGGStream*)gf_list_enum(ctx->streams, &i))) {
			if (st->ch == com->base.on_channel) {
				st->is_running = GF_FALSE;
				ctx->nb_playing --;
				break;
			}
		}
		return GF_OK;
	default:
		return GF_OK;
	}
}

#endif



GF_Err oggdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
	const GF_PropertyValue *p;
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		OGGStream *st;
		ctx->ipid = NULL;

		while ((st = (OGGStream*)gf_list_enum(ctx->streams, &i))) {
			if (st->opid)
				gf_filter_pid_remove(st->opid);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	return GF_OK;
}

static void oggdmx_check_dur(GF_Filter *filter, GF_OGGDmxCtx *ctx)
{
	FILE *stream;
	u32 hdr;
	u64 duration, cur_dur;
	s32 prev_sr = -1;
	const GF_PropertyValue *p;
return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen(p->value.string, "r");
	if (!stream) return;

	ctx->index_size = 0;

	duration = 0;
	while (1) {
		u32 sr, dur;
		u64 pos;
		u32 hdr = 0; //gf_mp3_get_next_header(stream);
		if (!hdr) break;
//		sr = gf_mp3_sampling_rate(hdr);

		if ((prev_sr>=0) && (prev_sr != sr)) {
			duration *= sr;
			duration /= prev_sr;

			cur_dur *= sr;
			cur_dur /= prev_sr;
		}
		prev_sr = sr;
//		dur = gf_mp3_window_size(hdr);
		duration += dur;
		cur_dur += dur;
		pos = gf_ftell(stream);
		if (cur_dur > ctx->index_dur * prev_sr) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(OGGIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos - 4;
			ctx->indexes[ctx->index_size].duration = duration;
			ctx->indexes[ctx->index_size].duration /= prev_sr;
			ctx->index_size ++;
			cur_dur = 0;
		}

		pos = gf_ftell(stream);
//		gf_fseek(stream, pos + gf_mp3_frame_size(hdr) - 4, SEEK_SET);
	}
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * prev_sr != duration * ctx->duration.den)) {
		u32 i=0;
		OGGStream *st;
		ctx->duration.num = duration;
		ctx->duration.den = prev_sr ;
		while ( (st = gf_list_enum(ctx->streams, &i)) ) {
			gf_filter_pid_set_info(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration.num, ctx->duration.den));
		}
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
}

static void oggdmx_check_pid(GF_Filter *filter, GF_OGGDmxCtx *ctx)
{

}

static Bool oggdmx_process_event(GF_Filter *filter, GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
		} else if (ctx->start_range == evt->play.start_range) {
			return GF_TRUE;
		}
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		oggdmx_check_dur(filter, ctx);


		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos)
				return GF_TRUE;
		}
		ctx->seek_file = GF_TRUE;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		//don't cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

GF_Err oggdmx_process(GF_Filter *filter)
{
	ogg_page oggpage;
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	GF_Err e;
	u8 *start;
	u64 src_cts;
	OGGStream *st;

	//update duration
	oggdmx_check_dur(filter, ctx);


	if (ctx->seek_file) {
		ogg_sync_clear(&ctx->oy);
		ogg_sync_init(&ctx->oy);
		ctx->seek_file = GF_FALSE;
	} else {
		u32 i=0;
		u32 would_block = 0;
		//check if all the streams are in block state, if so return.
		//we need to check for all output since one pid could still be buffering
		while ((st = gf_list_enum(ctx->streams, &i))) {
			if (st->got_headers && gf_filter_pid_would_block(st->opid))
				would_block++;
		}
		if (would_block && (would_block+1==i))
			return GF_OK;
	}
	while (1) {
		ogg_packet oggpacket;

		if (ogg_sync_pageout(&ctx->oy, &oggpage ) != 1 ) {
			u32 pck_size;
			char *data, *buffer;

			pck = gf_filter_pid_get_packet(ctx->ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(ctx->ipid)) oggdmx_signal_eos(ctx);
				return GF_OK;
			}
			data = gf_filter_pck_get_data(pck, &pck_size);
			buffer = ogg_sync_buffer(&ctx->oy, pck_size);
			memcpy(buffer, data, pck_size);
			if (ogg_sync_wrote(&ctx->oy, pck_size) >= 0) {
				gf_filter_pid_drop_packet(ctx->ipid);
			}
			continue;
		}

		if (ogg_page_bos(&oggpage)) {
			OGG_NewStream(filter, ctx, &oggpage);
		}

		st = OGG_FindStreamForPage(ctx, &oggpage);
		if (!st) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OGG] cannot find stream for ogg page\n"));
			continue;
		}

		if (ogg_page_eos(&oggpage))
			st->eos_detected = GF_TRUE;

		while (ogg_stream_packetout(&st->os, &oggpacket ) > 0 ) {
			if (st->parse_headers && !st->got_headers) {
				GF_BitStream *bs;
				u32 bytes = oggpacket.bytes;
				//bug in some files where first header is repeated
				if ( (st->parse_headers + 1 == st->info.num_init_headers) && (st->dsi_len == 2 + bytes) )
					continue;

				if (st->info.type==GPAC_OTI_VORBIS)
					gf_vorbis_parse_header(&st->vp, (char *) oggpacket.packet, oggpacket.bytes);


				bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				if (st->dsi) {
					gf_bs_write_data(bs, st->dsi, st->dsi_len);
					gf_free(st->dsi);
					st->dsi = NULL;
					st->dsi_len=0;
				}
				gf_bs_write_u16(bs, oggpacket.bytes);
				gf_bs_write_data(bs, (char *) oggpacket.packet, oggpacket.bytes);
				gf_bs_get_content(bs, (char **)&st->dsi, &st->dsi_len);
				gf_bs_del(bs);
				st->parse_headers--;
				if (!st->parse_headers) {
					st->got_headers = GF_TRUE;
					OGG_DeclarePID(filter, ctx, st);
				}
			} else if (st->parse_headers && st->got_headers) {
				st->parse_headers--;
			} else {
				if (oggpacket.granulepos != -1) {
					st->last_granule = oggpacket.granulepos;
				}
#if 0
				if (st->map_time) {
					Double t;
					if (ctx->start_range && (oggpacket.granulepos==-1)) continue;
					t = OGG_GranuleToMediaTime(&st->info, st->last_granule);
					if (t>=ctx->start_range) {
						GF_NetworkCommand map;
						map.command_type = GF_NET_CHAN_MAP_TIME;
						map.map_time.on_channel = st->ch;
						map.map_time.reset_buffers = (ctx->start_range>0.2) ? GF_TRUE : GF_FALSE;
						map.map_time.timestamp = st->ogg_ts = 0;
						map.map_time.media_time = t;
						gf_service_command(ctx->service, &map, GF_OK);
						st->map_time = GF_FALSE;
						OGG_SendPackets(ctx, st, &oggpacket);
					}
				} else
#endif
				{
					OGG_SendPackets(ctx, st, &oggpacket);
				}
			}
		}
	}
	return GF_OK;
}

static GF_Err oggdmx_initialize(GF_Filter *filter)
{
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	ogg_sync_init(&ctx->oy);
	return GF_OK;
}

static void oggdmx_finalize(GF_Filter *filter)
{
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);

	/*just in case something went wrong*/
	while (gf_list_count(ctx->streams)) {
		OGGStream *st = (OGGStream*)gf_list_get(ctx->streams, 0);
		gf_list_rem(ctx->streams, 0);
		ogg_stream_clear(&st->os);
		if (st->dsi) gf_free(st->dsi);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	ogg_sync_clear(&ctx->oy);

	if (ctx->indexes) gf_free(ctx->indexes);

}

static const GF_FilterCapability OGGDmxInputs[] =
{
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/x-ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/x-vorbis+ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_FILE_EXT, PROP_STRING("oga|spx"), .start=GF_TRUE},

	{.code=GF_PROP_PID_MIME, PROP_STRING("application/ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("application/x-ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/x-ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("video/x-ogm+ogg"), .start=GF_TRUE},
	{.code=GF_PROP_PID_FILE_EXT, PROP_STRING("ogg|ogv|oggm"), .start=GF_TRUE},
	{}
};


static const GF_FilterCapability OGGDmxOutputs[] =
{
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_VORBIS )},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_FLAC )},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_SPEEX )},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_THEORA )},

	{}
};

#define OFFS(_n)	#_n, offsetof(GF_OGGDmxCtx, _n)
static const GF_FilterArgs OGGDmxArgs[] =
{
	{ OFFS(index_dur), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{}
};


GF_FilterRegister OGGDmxRegister = {
	.name = "reframe_ogg",
	.description = "OGG Demux",
	.private_size = sizeof(GF_OGGDmxCtx),
	.args = OGGDmxArgs,
	.initialize = oggdmx_initialize,
	.finalize = oggdmx_finalize,
	.input_caps = OGGDmxInputs,
	.output_caps = OGGDmxOutputs,
	.configure_pid = oggdmx_configure_pid,
	.process = oggdmx_process,
	.process_event = oggdmx_process_event
};

#endif // !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_OGG)

const GF_FilterRegister *oggdmx_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_OGG)
	return &OGGDmxRegister;
#else
	return NULL;
#endif

}
