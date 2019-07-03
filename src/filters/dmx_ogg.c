/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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
#include <gpac/internal/ogg.h>
#include <gpac/internal/isomedia_dev.h>
//#include <ogg/ogg.h>
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

	GF_VorbisParser *vorbis_parser;

	GF_OpusParser *opus_parser;
} GF_OGGStream;

typedef struct
{
	Double index;

	//only one input pid declared
	GF_FilterPid *ipid;

	u64 file_pos, file_size;
	u32 global_rate;
	GF_Fraction duration;
	Double start_range;
	Bool seek_file;
	u32 nb_playing;
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
		if (st->opid)
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
		info->type = GF_CODECID_VORBIS;
	}
	/*speex*/
	else if ((oggpacket->bytes >= 7) && !strncmp((char *) &oggpacket->packet[0], "Speex", 5)) {
		info->streamType = GF_STREAM_AUDIO;
		oggpack_readinit(&opb, oggpacket->packet, oggpacket->bytes);
		oggpack_adv(&opb, 224);
		oggpack_adv(&opb, 32);
		oggpack_adv( &opb, 32);
		info->sample_rate = oggpack_read(&opb, 32);
		info->type = GF_CODECID_SPEEX;
		info->num_init_headers = 1;
	}
	/*flac*/
	else if ((oggpacket->bytes >= 4) && !strncmp((char *) &oggpacket->packet[0], "fLaC", 4)) {
		info->streamType = GF_STREAM_AUDIO;
		info->type = GF_CODECID_FLAC;
		info->num_init_headers = 3;
	}
	/*opus*/
	else if ((oggpacket->bytes >= 8) && !strncmp((char *) &oggpacket->packet[0], "OpusHead", 8)) {
		info->streamType = GF_STREAM_AUDIO;
		info->type = GF_CODECID_OPUS;
		info->num_init_headers = 1;
		info->sample_rate = 48000;
	}
	/*theora*/
	else if ((oggpacket->bytes >= 7) && !strncmp((char *) &oggpacket->packet[1], "theora", 6)) {
		GF_BitStream *bs;
		u32 keyframe_freq_force;

		info->streamType = GF_STREAM_VISUAL;
		info->type = GF_CODECID_THEORA;
		bs = gf_bs_new((char *) oggpacket->packet, oggpacket->bytes, GF_BITSTREAM_READ);
		gf_bs_read_int(bs, 56);
		gf_bs_read_int(bs, 8); /* major version num */
		gf_bs_read_int(bs, 8); /* minor version num */
		gf_bs_read_int(bs, 8); /* subminor version num */
		info->width = gf_bs_read_int(bs, 16) << 4; /* width */
		info->height = gf_bs_read_int(bs, 16) << 4; /* height */
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

		/*patch for compatibility with old arch*/
		if ((info->frame_rate.den==25025) && (info->frame_rate.num==1001) ) {
			info->frame_rate.den = 25000;
			info->frame_rate.num = 1000;
		}

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

static void oggdmx_declare_pid(GF_Filter *filter, GF_OGGDmxCtx *ctx, GF_OGGStream *st)
{
	if (!st->opid) {
		st->opid = gf_filter_pid_new(filter);
	}
//	gf_filter_pid_set_property(st->opid, GF_PROP_PID_ID, &PROP_UINT(st->serial_no) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_ID, &PROP_UINT(1 + gf_list_find(ctx->streams, st) ) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(st->info.streamType) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT(st->info.type) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_BITRATE, &PROP_UINT(st->info.bitrate) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(st->info.sample_rate ? st->info.sample_rate : st->info.frame_rate.den) );
	gf_filter_pid_set_property(st->opid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(0xFE) );

	//opus DSI is formatted as box (ffmpeg compat) we might want to change that to avoid the box header
	if (st->info.type==GF_CODECID_OPUS) {
		GF_OpusSpecificBox *opus = (GF_OpusSpecificBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_DOPS);
		st->dsi_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		opus->version = 0;

		opus->OutputChannelCount = st->opus_parser->OutputChannelCount;
		opus->PreSkip = st->opus_parser->PreSkip;
		opus->InputSampleRate = st->opus_parser->InputSampleRate;
		opus->OutputGain = st->opus_parser->OutputGain;
		opus->ChannelMappingFamily = st->opus_parser->ChannelMappingFamily;
		opus->StreamCount = st->opus_parser->StreamCount;
		opus->CoupledCount = st->opus_parser->CoupledCount;
		memcpy(opus->ChannelMapping, st->opus_parser->ChannelMapping, sizeof(char)*255);
		gf_isom_box_size((GF_Box *) opus);
		gf_isom_box_write((GF_Box *) opus, st->dsi_bs);
		gf_isom_box_del((GF_Box *) opus);

		st->info.nb_chan = st->opus_parser->OutputChannelCount;
	}

	if (st->dsi_bs) {
		u8 *data;
		u32 size;
		gf_bs_get_content(st->dsi_bs, &data, &size);
		gf_bs_del(st->dsi_bs);
		st->dsi_bs = NULL;
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(data, size) );
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
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

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
			//resetup stream
			ogg_stream_clear(&st->os);
			ogg_stream_init(&st->os, st->serial_no);
			ogg_stream_pagein(&st->os, oggpage);
			st->parse_headers = st->info.num_init_headers;
			return;
		}
	}

	/*look if we have the same stream defined (eg, reuse first stream dead with same header page)*/
	i=0;
	while ((st = (GF_OGGStream*)gf_list_enum(ctx->streams, &i))) {
		if (st->eos_detected) {
			gf_filter_pid_set_eos(st->opid);
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
	switch (st->info.type) {
	case GF_CODECID_VORBIS:
		GF_SAFEALLOC(st->vorbis_parser, GF_VorbisParser);
		break;
	case GF_CODECID_OPUS:
		GF_SAFEALLOC(st->opus_parser, GF_OpusParser);
		break;
	default:
		break;
	}

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
			GF_PropertyEntry *pe=NULL;
			const GF_PropertyValue *p = gf_filter_pid_get_info(ctx->ipid, GF_PROP_PID_DOWN_SIZE, &pe);
			if (p) ctx->file_size = p->value.longuint;
			gf_filter_release_property(pe);
		}
		if (ctx->file_size) {
			ctx->duration.num = (u32) (8 * ctx->file_size);
			ctx->duration.den = ctx->global_rate;
		}
	}
}

GF_Err oggdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
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
	const GF_PropertyValue *p;
	OGGInfo info, the_info;
	ogg_page oggpage;
	ogg_packet oggpacket;
	ogg_stream_state os, the_os;
	u64 max_gran;
	Bool has_stream = GF_FALSE;
	GF_VorbisParser vp;
	GF_OpusParser op;
	u64 recompute_ts;
	GF_Fraction dur;

	if (!ctx->index || ctx->duration.num) return;

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

	stream = gf_fopen(p->value.string, "rb");
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
				if (the_info.type==GF_CODECID_VORBIS) {
					if (the_info.num_init_headers) {
						the_info.num_init_headers--;
						gf_vorbis_parse_header(&vp, oggpacket.packet, oggpacket.bytes);
					} else {
						recompute_ts += gf_vorbis_check_frame(&vp, (char *) oggpacket.packet, oggpacket.bytes);
					}
				} else if (the_info.type==GF_CODECID_OPUS) {
					if (the_info.num_init_headers) {
						the_info.num_init_headers--;
						gf_opus_parse_header(&op, oggpacket.packet, oggpacket.bytes);
					} else {
						recompute_ts += gf_opus_check_frame(&op, (char *) oggpacket.packet, oggpacket.bytes);
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
			dur.num = (u32) recompute_ts;
			dur.den = the_info.sample_rate;
		} else {
			//convert granule to time
			if (the_info.sample_rate) {
				dur.num = (s32) max_gran;
			} else if (the_info.frame_rate.num) {
				s64 iframe = max_gran >> the_info.theora_kgs;
				s64 pframe = max_gran - (iframe << the_info.theora_kgs);
				pframe += iframe;
				dur.num = (s32) (pframe / the_info.frame_rate.num);
			} else {
				dur.num = 0;
			}
			if (the_info.sample_rate) dur.den = the_info.sample_rate;
			else dur.den = the_info.frame_rate.den;
		}

		if (!ctx->duration.num || (ctx->duration.num  * dur.den != dur.num * ctx->duration.den)) {
			u32 i=0;
			GF_OGGStream *st;
			ctx->duration = dur;
			while ( (st = gf_list_enum(ctx->streams, &i)) ) {
				gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
			}
		}
	}
	gf_fclose(stream);
}

static Bool oggdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_OGGStream *st;
	GF_FilterEvent fevt;
	GF_OGGDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->nb_playing && (ctx->start_range == evt->play.start_range)) {
			ctx->nb_playing++;
			return GF_TRUE;
		}
		ctx->nb_playing++;
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		oggdmx_check_dur(filter, ctx);


		ctx->start_range = evt->play.start_range;
		ctx->file_pos = 0;
		if (ctx->duration.num) {
			ctx->file_pos = (u32) (ctx->file_size * ctx->start_range);
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
				st->recomputed_ts = (u32) (ctx->start_range * st->info.sample_rate);
			} else {
				st->recomputed_ts = (u32) (ctx->start_range * st->info.frame_rate.den);
			}
		}

		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->nb_playing --;
		//cancel event if not last stream
		if (ctx->nb_playing) return GF_TRUE;

		//cancel event if we didn't get all stream headers yet not last stream
		i=0;
		while ((st = gf_list_enum(ctx->streams, &i))) {
			if (!st->got_headers) return GF_TRUE;
		}
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
	GF_FilterPacket *pck;
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
			data = (char *) gf_filter_pck_get_data(pck, &pck_size);
			buffer = ogg_sync_buffer(&ctx->oy, pck_size);
			memcpy(buffer, data, pck_size);
			if (ogg_sync_wrote(&ctx->oy, pck_size) >= 0) {
				gf_filter_pid_drop_packet(ctx->ipid);
			}
			continue;
		}

		if (ogg_page_bos(&oggpage)) {
			oggdmx_new_stream(filter, ctx, &oggpage);
			continue;
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
				Bool res = GF_FALSE;
				Bool add_page = GF_FALSE;
				u32 bytes = oggpacket.bytes;
				//bug in some files where first header is repeated
				if ( (st->parse_headers + 1 == st->info.num_init_headers) && st->dsi_bs && (gf_bs_get_position(st->dsi_bs) == 2 + bytes) )
					continue;

				switch (st->info.type) {
				case GF_CODECID_VORBIS:
					res = gf_vorbis_parse_header(st->vorbis_parser, (char *) oggpacket.packet, oggpacket.bytes);
					add_page = res;
					break;
				case GF_CODECID_OPUS:
					res = gf_opus_parse_header(st->opus_parser, (char *) oggpacket.packet, oggpacket.bytes);
					break;
				case GF_CODECID_THEORA:
					add_page = GF_TRUE;
					break;
				}

				if (add_page) {
					if (!st->dsi_bs) st->dsi_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
					gf_bs_write_u16(st->dsi_bs, oggpacket.bytes);
					gf_bs_write_data(st->dsi_bs, (char *) oggpacket.packet, oggpacket.bytes);
				}
				
				st->parse_headers--;
				if (!st->parse_headers) {
					st->got_headers = GF_TRUE;
					oggdmx_declare_pid(filter, ctx, st);
				}
			} else if (st->parse_headers && st->got_headers) {
				st->parse_headers--;
			} else if (!st->opid) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OGG] Channel %d packet before configure done - discarding\n", st->serial_no));
			} else {
				u8 *output;
				GF_FilterPacket *dst_pck;

				if (st->info.type==GF_CODECID_THEORA) {
					oggpack_buffer opb;
					oggpackB_readinit(&opb, oggpacket.packet, oggpacket.bytes);
					/*not a new frame*/
					if (oggpackB_read(&opb, 1) != 0) continue;

					dst_pck = gf_filter_pck_new_alloc(st->opid, oggpacket.bytes, &output);
					memcpy(output, (char *) oggpacket.packet, oggpacket.bytes);
					gf_filter_pck_set_cts(dst_pck, st->recomputed_ts);
					gf_filter_pck_set_sap(dst_pck, oggpackB_read(&opb, 1) ? GF_FILTER_SAP_NONE : GF_FILTER_SAP_1);
					st->recomputed_ts += st->info.frame_rate.num;
				}
				//this is audio
				else {
					u32 block_size = 0;

					if (st->info.type==GF_CODECID_VORBIS) {
						block_size = gf_vorbis_check_frame(st->vorbis_parser, (char *) oggpacket.packet, oggpacket.bytes);
						if (!block_size) continue;
					}
					else if (st->info.type==GF_CODECID_OPUS) {
						block_size = gf_opus_check_frame(st->opus_parser, (char *) oggpacket.packet, oggpacket.bytes);
						if (!block_size) continue;
					}


					dst_pck = gf_filter_pck_new_alloc(st->opid, oggpacket.bytes, &output);
					memcpy(output, (char *) oggpacket.packet, oggpacket.bytes);
					gf_filter_pck_set_cts(dst_pck, st->recomputed_ts);
					gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);

					st->recomputed_ts += block_size;
				}
				gf_filter_pck_send(dst_pck);

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
		if (st->vorbis_parser) gf_free(st->vorbis_parser);
		if (st->opus_parser) gf_free(st->opus_parser);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	ogg_sync_clear(&ctx->oy);
}

static const char *oggdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (!strncmp(data, "OggS", 4)) {
		*score = GF_FPROBE_SUPPORTED;
		return "video/ogg";
	}
	return NULL;
}


static const GF_FilterCapability OGGDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/ogg|audio/x-ogg|audio/x-vorbis+ogg|application/ogg|application/x-ogg|video/ogg|video/x-ogg|video/x-ogm+ogg"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_VORBIS),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_SPEEX),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "oga|spx|ogg|ogv|oggm|opus"),
};

#define OFFS(_n)	#_n, offsetof(GF_OGGDmxCtx, _n)
static const GF_FilterArgs OGGDmxArgs[] =
{
	{ OFFS(index), "indexing window length (unimplemented, only 0 disables stream probing for duration), ", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister OGGDmxRegister = {
	.name = "oggdmx",
	GF_FS_SET_DESCRIPTION("OGG demuxer")
	GF_FS_SET_HELP("This filter demultiplexes OGG files/data into a set of media PIDs and frames.")
	.private_size = sizeof(GF_OGGDmxCtx),
	.initialize = oggdmx_initialize,
	.finalize = oggdmx_finalize,
	.args = OGGDmxArgs,
	.flags = GF_FS_REG_DYNAMIC_PIDS,
	SETCAPS(OGGDmxCaps),
	.configure_pid = oggdmx_configure_pid,
	.process = oggdmx_process,
	.process_event = oggdmx_process_event,
	.probe_data = oggdmx_probe_data,
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

