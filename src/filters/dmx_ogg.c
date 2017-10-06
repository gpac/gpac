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
	u32 streamType;	/*MPEG-4 streamType*/
	u32 num_init_headers;
	u32 sample_rate, bitrate, nb_chan;
	u32 width, height;
	GF_Fraction sar;

	u32 theora_kgs;
	GF_Fraction frame_rate;

	u32 type;
} OGGInfo;

typedef struct
{
	//only one output pid declared
	GF_FilterPid *opid;

	ogg_stream_state os;
	u32 serial_no;
	/*DSI for ogg - cf constants.h*/
	GF_BitStream *dsi_bs;

	OGGInfo info;
	Bool got_headers;

	u32 parse_headers;

	Bool eos_detected;

	u32 recomputed_ts;
	GF_VorbisParser vp;
} GF_OGGStream;

typedef struct
{
	//only one input pid declared
	GF_FilterPid *ipid;

	u64 file_pos, file_size;
	u32 global_rate;
	GF_Fraction duration;
	Double start_range;
	Bool seek_file;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	GF_List *streams;

	/*ogg ogfile state*/
	ogg_sync_state oy;
} GF_OGGDmxCtx;


void oggdmx_signal_eos(GF_OGGDmxCtx *ctx)
{
	GF_OGGStream *st;
	u32 i=0;
	while ((st = (GF_OGGStream*)gf_list_enum(ctx->streams, &i))) {
		gf_filter_pid_set_eos(st->opid);
	}
}

static GF_OGGStream *oggdmx_find_stream_for_page(GF_OGGDmxCtx *ctx, ogg_page *oggpage)
{
	u32 i, count;
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		GF_OGGStream *st = (GF_OGGStream*)gf_list_get(ctx->streams, i);
		if (ogg_stream_pagein(&st->os, oggpage) == 0) return st;
	}
	return NULL;
}


u64 oggdmx_granule_to_time(OGGInfo *cfg, s64 granule)
{
	if (cfg->sample_rate) {
		return granule;
	}
	if (cfg->frame_rate.num) {
		s64 iframe = granule>>cfg->theora_kgs;
		s64 pframe = granule - (iframe<<cfg->theora_kgs);
		pframe += iframe;
		return (u64) (pframe / cfg->frame_rate.num);
	}
	return 0;
}

Double oggdmx_granule_to_media_time(OGGInfo *cfg, s64 granule)
{
	Double t = (Double) (s64) oggdmx_granule_to_time(cfg, granule);
	if (cfg->sample_rate) t /= cfg->sample_rate;
	else t /= cfg->frame_rate.den;
	return t;
}


static void oggdmx_get_stream_info(ogg_packet *oggpacket, OGGInfo *info)
{
	oggpack_buffer opb;

	memset(info, 0, sizeof(OGGInfo));

	/*vorbis*/
	if ((oggpacket->bytes >= 7) && !strncmp((char *) &oggpacket->packet[1], "vorbis", 6)) {
		info->streamType = GF_STREAM_AUDIO;
		oggpack_readinit(&opb, oggpacket->packet, oggpacket->bytes);
		oggpack_adv( &opb, 88);
		info->nb_chan = oggpack_read( &opb, 8);	/*nb chan*/
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
		u32 keyframe_freq_force;

		info->streamType = GF_STREAM_VISUAL;
		info->type = GPAC_OTI_THEORA;
		bs = gf_bs_new((char *) oggpacket->packet, oggpacket->bytes, GF_BITSTREAM_READ);
		gf_bs_read_int(bs, 56);
		gf_bs_read_int(bs, 8); /* major version num */
		gf_bs_read_int(bs, 8); /* minor version num */
		gf_bs_read_int(bs, 8); /* subminor version num */
		info->width = gf_bs_read_int(bs, 16) /*<< 4*/; /* width */
		info->height = gf_bs_read_int(bs, 16) /*<< 4*/; /* height */
		gf_bs_read_int(bs, 24); /* frame width */
		gf_bs_read_int(bs, 24); /* frame height */
		gf_bs_read_int(bs, 8); /* x offset */
		gf_bs_read_int(bs, 8); /* y offset */
		info->frame_rate.den = gf_bs_read_u32(bs);
		info->frame_rate.num = gf_bs_read_u32(bs);
		info->sar.num = gf_bs_read_int(bs, 24); /* aspect_numerator */
		info->sar.den =gf_bs_read_int(bs, 24); /* aspect_denominator */
		gf_bs_read_int(bs, 8); /* colorspace */
		info->bitrate = gf_bs_read_int(bs, 24);/* bitrate */
		gf_bs_read_int(bs, 6); /* quality */

		keyframe_freq_force = 1 << gf_bs_read_int(bs, 5);
		info->theora_kgs = 0;
		keyframe_freq_force--;
		while (keyframe_freq_force) {
			info->theora_kgs ++;
			keyframe_freq_force >>= 1;
		}
		info->num_init_headers = 3;
		gf_bs_del(bs);
	}
}

static void oggdmx_resetup_stream(GF_OGGDmxCtx *ctx, GF_OGGStream *st, ogg_page *oggpage)
{
	ogg_stream_clear(&st->os);
	ogg_stream_init(&st->os, st->serial_no);
	ogg_stream_pagein(&st->os, oggpage);
	st->parse_headers = st->info.num_init_headers;
}

static void oggdmx_declare_pid(GF_Filter *filter, GF_OGGDmxCtx *ctx, GF_OGGStream *st)
{
	if (!st->opid) {
		st->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(st->info.streamType) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_OTI, &PROP_UINT(st->info.type) );
	gf_filter_pid_set_info(st->opid, GF_PROP_PID_BITRATE, &PROP_UINT(st->info.bitrate) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(st->info.sample_rate ? st->info.sample_rate : st->info.frame_rate.den) );

	if (st->dsi_bs) {
		char *data;
		u32 size;
		gf_bs_get_content(st->dsi_bs, &data, &size);
		gf_bs_del(st->dsi_bs);
		st->dsi_bs = NULL;
		gf_filter_pid_set_info(st->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(data, size) );
	}

	if (st->info.sample_rate)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(st->info.sample_rate) );

	if (st->info.nb_chan)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(st->info.nb_chan) );

	if (st->info.width)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_WIDTH, &PROP_UINT(st->info.width) );
	if (st->info.height)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(st->info.height) );
	if (st->info.sar.den)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_SAR, &PROP_FRAC(st->info.sar) );
	if (st->info.frame_rate.den)
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_FPS, &PROP_FRAC(st->info.frame_rate) );

	if (ctx->duration.num)
		gf_filter_pid_set_info(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

}

static void oggdmx_new_stream(GF_Filter *filter, GF_OGGDmxCtx *ctx, ogg_page *oggpage)
{
	ogg_packet oggpacket;
	u32 serial_no, i;
	GF_OGGStream *st;

	/*reannounce of stream (caroussel in live streams) */
	serial_no = ogg_page_serialno(oggpage);
	i=0;
	while ((st = (GF_OGGStream*)gf_list_enum(ctx->streams, &i))) {
		if (st->serial_no==serial_no) {
			oggdmx_resetup_stream(ctx, st, oggpage);
			return;
		}
	}

	/*look if we have the same stream defined (eg, reuse first stream dead with same header page)*/
	i=0;
	while ((st = (GF_OGGStream*)gf_list_enum(ctx->streams, &i))) {
		if (st->eos_detected) {
			gf_filter_pid_set_eos(st->eos_detected);
			//and reuse the pid connection
			break;
		}
	}
	if (!st) {
		GF_SAFEALLOC(st, GF_OGGStream);
		if (!st) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OGG] Failed to allocate stream for demux\n"));
			return;
		}
	}
	st->eos_detected = GF_FALSE;
	st->serial_no = serial_no;
	ogg_stream_init(&st->os, st->serial_no);
	ogg_stream_pagein(&st->os, oggpage);

	ogg_stream_packetpeek(&st->os, &oggpacket);
	oggdmx_get_stream_info(&oggpacket, &st->info);

	gf_list_add(ctx->streams, st);
	st->parse_headers = st->info.num_init_headers;

	if (st->got_headers) {
		oggdmx_declare_pid(filter, ctx, st);
	}
	i=0;
	ctx->global_rate = 0;
	while ((st = (GF_OGGStream*)gf_list_enum(ctx->streams, &i))) {
		if (!st->eos_detected) ctx->global_rate += st->info.bitrate;
	}
	if (ctx->global_rate && ctx->is_file && !ctx->file_loaded) {
		if (!ctx->file_size) {
			const GF_PropertyValue *p = gf_filter_pid_get_info(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
			if (p) ctx->file_size = p->value.uint;
		}
		if (ctx->file_size) {
			ctx->duration.num = 8 * ctx->file_size;
			ctx->duration.den = ctx->global_rate;
		}
	}
}

void oggdmx_send_packet(GF_OGGDmxCtx *ctx, GF_OGGStream *st, ogg_packet *oggpacket)
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
		gf_filter_pck_set_cts(pck, st->recomputed_ts);
		gf_filter_pck_set_sap(pck, oggpackB_read(&opb, 1) );
		st->recomputed_ts += st->info.frame_rate.num;
	} else {
		pck = gf_filter_pck_new_alloc(st->opid, oggpacket->bytes, &data);
		memcpy(data, (char *) oggpacket->packet, oggpacket->bytes);
		gf_filter_pck_set_cts(pck, st->recomputed_ts);
		gf_filter_pck_set_sap(pck, 1);

		if (st->info.type==GPAC_OTI_VORBIS) {
			st->recomputed_ts += gf_vorbis_check_frame(&st->vp, (char *) oggpacket->packet, oggpacket->bytes);
		}
	}
	gf_filter_pck_send(pck);
}

GF_Err oggdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
	const GF_PropertyValue *p;
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		GF_OGGStream *st;
		ctx->ipid = NULL;

		while ((st = (GF_OGGStream*)gf_list_enum(ctx->streams, &i))) {
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
	ogg_sync_state oy;
	FILE *stream;
	u32 hdr;
	const GF_PropertyValue *p;
	OGGInfo info, the_info;
	ogg_page oggpage;
	ogg_packet oggpacket;
	ogg_stream_state os, the_os;
	u64 max_gran;
	Bool has_stream = GF_FALSE;
	GF_VorbisParser vp;
	u64 recompute_ts;
	GF_Fraction dur;

	if (ctx->duration.num) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		ctx->duration.num=1;
		return;
	}
	ctx->is_file = GF_TRUE;
	if (!ctx->file_loaded) return;

	stream = gf_fopen(p->value.string, "r");
	if (!stream) return;

	ogg_sync_init(&oy);
	memset(&the_info, 0, sizeof(OGGInfo));
	memset(&vp, 0, sizeof(GF_VorbisParser));
	recompute_ts = 0;
	max_gran = 0;
	while (1) {
		char buf[10000];
		while (ogg_sync_pageout(&oy, &oggpage) != 1 ) {
			char *buffer;
			u32 bytes;

			if (feof(stream))
				break;

			bytes = (u32) fread(buf, 1, 10000, stream);
			if (!bytes) break;
			buffer = ogg_sync_buffer(&oy, bytes);
			memcpy(buffer, buf, bytes);
			ogg_sync_wrote(&oy, bytes);
		}
		if (feof(stream))
			break;

		if (ogg_page_bos(&oggpage)) {
			ogg_stream_init(&os, ogg_page_serialno(&oggpage));
			if (ogg_stream_pagein(&os, &oggpage) >= 0 ) {
				ogg_stream_packetpeek(&os, &oggpacket);
				if (ogg_stream_pagein(&os, &oggpage) >= 0 ) {
					ogg_stream_packetpeek(&os, &oggpacket);
					oggdmx_get_stream_info(&oggpacket, &info);
				}
				if (!has_stream) {
					has_stream = GF_TRUE;
					ogg_stream_init(&the_os, ogg_page_serialno(&oggpage));
					the_info = info;
				}
			}
			ogg_stream_clear(&os);
		}
		if (has_stream && (ogg_stream_pagein(&the_os, &oggpage) >= 0) ) {
			while (ogg_stream_packetout(&the_os, &oggpacket ) > 0 ) {
				if (the_info.type==GPAC_OTI_VORBIS) {
					if (the_info.num_init_headers) {
						the_info.num_init_headers--;
						gf_vorbis_parse_header(&vp, oggpacket.packet, oggpacket.bytes);
					} else {
						recompute_ts += gf_vorbis_check_frame(&vp, (char *) oggpacket.packet, oggpacket.bytes);

					}
				} else if ((oggpacket.granulepos>=0) && ((u64) oggpacket.granulepos>max_gran) ) {
					max_gran = oggpacket.granulepos;
				}
			}
		}
	}
	ogg_sync_clear(&oy);
	ctx->file_size = gf_ftell(stream);
	if (has_stream) {
		ogg_stream_clear(&the_os);
		if (recompute_ts) {
			dur.num = recompute_ts;
			dur.den = the_info.sample_rate;
		} else {
			dur.num = oggdmx_granule_to_time(&the_info, max_gran);
			if (the_info.sample_rate) dur.den = the_info.sample_rate;
			else dur.den = the_info.frame_rate.den;
		}

		if (!ctx->duration.num || (ctx->duration.num  * dur.den != dur.num * ctx->duration.den)) {
			u32 i=0;
			GF_OGGStream *st;
			ctx->duration = dur;
			while ( (st = gf_list_enum(ctx->streams, &i)) ) {
				gf_filter_pid_set_info(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
			}
		}
	}
	gf_fclose(stream);
}

static Bool oggdmx_process_event(GF_Filter *filter, GF_FilterEvent *evt)
{
	u32 i;
	GF_OGGStream *st;
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
		ctx->file_pos = 0;
		if (ctx->duration.num) {
			ctx->file_pos = ctx->file_size * ctx->start_range;
			ctx->file_pos *= ctx->duration.den;
			ctx->file_pos /= ctx->duration.num;
			if (ctx->file_pos>ctx->file_size) return GF_TRUE;
		}

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos)
				return GF_TRUE;
		}
		ctx->seek_file = GF_TRUE;
		i=0;
		while ((st = gf_list_enum(ctx->streams, &i)) ) {
			if (st->info.sample_rate) {
				st->recomputed_ts = ctx->start_range * st->info.sample_rate;
			} else {
				st->recomputed_ts = ctx->start_range * st->info.frame_rate.den;
			}
		}

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
	GF_OGGStream *st;

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
			oggdmx_new_stream(filter, ctx, &oggpage);
		}

		st = oggdmx_find_stream_for_page(ctx, &oggpage);
		if (!st) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OGG] cannot find stream for ogg page\n"));
			continue;
		}

		if (ogg_page_eos(&oggpage))
			st->eos_detected = GF_TRUE;

		while (ogg_stream_packetout(&st->os, &oggpacket ) > 0 ) {
			if (st->parse_headers && !st->got_headers) {
				u32 bytes = oggpacket.bytes;
				//bug in some files where first header is repeated
				if ( (st->parse_headers + 1 == st->info.num_init_headers) && st->dsi_bs && (gf_bs_get_position(st->dsi_bs) == 2 + bytes) )
					continue;

				if (st->info.type==GPAC_OTI_VORBIS)
					gf_vorbis_parse_header(&st->vp, (char *) oggpacket.packet, oggpacket.bytes);


				if (!st->dsi_bs) st->dsi_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				gf_bs_write_u16(st->dsi_bs, oggpacket.bytes);
				gf_bs_write_data(st->dsi_bs, (char *) oggpacket.packet, oggpacket.bytes);

				st->parse_headers--;
				if (!st->parse_headers) {
					st->got_headers = GF_TRUE;
					oggdmx_declare_pid(filter, ctx, st);
				}
			} else if (st->parse_headers && st->got_headers) {
				st->parse_headers--;
			} else {
				oggdmx_send_packet(ctx, st, &oggpacket);
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
		GF_OGGStream *st = (GF_OGGStream*)gf_list_get(ctx->streams, 0);
		gf_list_rem(ctx->streams, 0);
		ogg_stream_clear(&st->os);
		if (st->dsi_bs) gf_bs_del(st->dsi_bs);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	ogg_sync_clear(&ctx->oy);
}

static const GF_FilterCapability OGGDmxInputs[] =
{
	CAP_INC_STRING(GF_PROP_PID_MIME, "audio/ogg|audio/x-ogg|audio/x-vorbis+ogg|application/ogg|application/x-ogg|video/ogg|video/x-ogg|video/x-ogm+ogg"),
	{},
	CAP_INC_STRING(GF_PROP_PID_FILE_EXT, "oga|spx|ogg|ogv|oggm"),
};


static const GF_FilterCapability OGGDmxOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VORBIS),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_FLAC),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_SPEEX),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_THEORA),
};


GF_FilterRegister OGGDmxRegister = {
	.name = "ogg",
	.description = "OGG Demux",
	.private_size = sizeof(GF_OGGDmxCtx),
	.initialize = oggdmx_initialize,
	.finalize = oggdmx_finalize,
	INCAPS(OGGDmxInputs),
	OUTCAPS(OGGDmxOutputs),
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

