/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / MP3 reframer filter
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

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/filters.h>

typedef struct
{
	u64 pos;
	Double duration;
} MP3Idx;

typedef struct
{
	//filter args
	Double index_dur;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts, prev_cts;
	u32 sr, nb_ch, codecid;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	u32 hdr;

	u32 remaining;
	char header[10];
	u32 bytes_in_header;

	MP3Idx *indexes;
	u32 index_alloc_size, index_size;
} GF_MP3DmxCtx;




GF_Err mp3_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) ctx->timescale = p->value.uint;
	return GF_OK;
}

static void mp3_dmx_check_dur(GF_Filter *filter, GF_MP3DmxCtx *ctx)
{
	FILE *stream;
	u64 duration, cur_dur;
	s32 prev_sr = -1;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

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
	cur_dur = 0;
	while (1) {
		u32 sr, dur;
		u64 pos;
		u32 hdr = gf_mp3_get_next_header(stream);
		if (!hdr) break;
		sr = gf_mp3_sampling_rate(hdr);

		if ((prev_sr>=0) && (prev_sr != sr)) {
			duration *= sr;
			duration /= prev_sr;

			cur_dur *= sr;
			cur_dur /= prev_sr;
		}
		prev_sr = sr;
		dur = gf_mp3_window_size(hdr);
		duration += dur;
		cur_dur += dur;
		pos = gf_ftell(stream);
		if (cur_dur > ctx->index_dur * prev_sr) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(MP3Idx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos - 4;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= prev_sr;
			ctx->index_size ++;
			cur_dur = 0;
		}

		pos = gf_ftell(stream);
		gf_fseek(stream, pos + gf_mp3_frame_size(hdr) - 4, SEEK_SET);
	}
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * prev_sr != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = prev_sr ;

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void mp3_dmx_check_pid(GF_Filter *filter, GF_MP3DmxCtx *ctx)
{
	u32 sr;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		mp3_dmx_check_dur(filter, ctx);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL );

	if ((ctx->sr == gf_mp3_sampling_rate(ctx->hdr)) && (ctx->nb_ch == gf_mp3_num_channels(ctx->hdr) )
		&& (ctx->codecid == gf_mp3_object_type_indication(ctx->hdr) )
	)
		return;


	ctx->nb_ch = gf_mp3_num_channels(ctx->hdr);
	ctx->codecid = gf_mp3_object_type_indication(ctx->hdr);
	sr = gf_mp3_sampling_rate(ctx->hdr);

	//we change sample rate, change cts
	if (ctx->cts && ctx->sr && (ctx->sr != sr)) {
		ctx->cts *= sr;
		ctx->cts /= ctx->sr;
	}
	ctx->sr = sr;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(gf_mp3_object_type_indication(ctx->hdr) ) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(gf_mp3_window_size(ctx->hdr) ) );
}

static Bool mp3_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
			ctx->remaining = 0;
			ctx->bytes_in_header = 0;
		}
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		mp3_dmx_check_dur(filter, ctx);

		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->sr);
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

static GFINLINE void mp3_dmx_update_cts(GF_MP3DmxCtx *ctx)
{
	u32 nb_samp = gf_mp3_window_size(ctx->hdr);
	if (ctx->timescale) {
		u64 inc = nb_samp;
		inc *= ctx->timescale;
		inc /= ctx->sr;
		ctx->cts += inc;
	} else {
		ctx->cts += nb_samp;
	}
}

GF_Err mp3_dmx_process(GF_Filter *filter)
{
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u64 byte_offset;
	char *data, *output;
	u8 *start;
	u32 pck_size, remain;
	u32 alread_sync = 0;

	//update duration
	mp3_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			assert(ctx->remaining == 0);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	start = data;
	remain = pck_size;

	//flush not previously dispatched data
	if (ctx->remaining) {
		u32 to_send = ctx->remaining;
		if (ctx->remaining > pck_size) {
			to_send = pck_size;
			ctx->remaining -= pck_size;
		} else {
			ctx->remaining = 0;
		}
		if (! ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, to_send, &output);
			memcpy(output, data, to_send);

			gf_filter_pck_set_framing(dst_pck, GF_FALSE, ctx->remaining ? GF_FALSE : GF_TRUE);
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
			}
			gf_filter_pck_send(dst_pck);
		}

		if (ctx->remaining) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		mp3_dmx_update_cts(ctx);
		start += to_send;
		remain -= to_send;
	}

	if (ctx->bytes_in_header) {
		if (ctx->bytes_in_header + remain < 4) {
			memcpy(ctx->header + ctx->bytes_in_header, start, remain);
			ctx->bytes_in_header += remain;
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		alread_sync = 4 - ctx->bytes_in_header;
		memcpy(ctx->header + ctx->bytes_in_header, start, alread_sync);
		start += alread_sync;
		remain -= alread_sync;
		ctx->bytes_in_header = 0;
		alread_sync = GF_TRUE;
	}
	//input pid sets some timescale - we flushed pending data , update cts
	else if (ctx->timescale) {
		u64 cts = gf_filter_pck_get_cts(pck);
		if (cts != GF_FILTER_NO_TS)
			ctx->cts = cts;
	}

#if FILTER_FIXME
	//check MP3 ID3 tags
	{
		unsigned char id3v2[10];
		u32 pos = (u32) fread(id3v2, sizeof(unsigned char), 10, in);
		if ((s32) pos < 0) return gf_import_message(import, GF_IO_ERR, "IO error reading file %s", import->in_name);

		if (pos == 10) {
			/* Did we read an ID3v2 ? */
			if (id3v2[0] == 'I' && id3v2[1] == 'D' && id3v2[2] == '3') {
				u32 sz = ((id3v2[9] & 0x7f) + ((id3v2[8] & 0x7f) << 7) + ((id3v2[7] & 0x7f) << 14) + ((id3v2[6] & 0x7f) << 21));

				while (sz) {
					u32 r = (u32) fread(id3v2, sizeof(unsigned char), 1, in);
					if (r != 1) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[MP3 import] failed to read ID3\n"));
					}
					sz--;
				}
				id3_end = (u32) gf_ftell(in);
			}
		}
		fseek(in, id3_end, SEEK_SET);
	}
#endif


	while (remain) {
		u8 *sync;
		u32 bytes_skipped=0, size, nb_samp;

		if (remain<4) {
			memcpy(ctx->header, start, remain);
			ctx->bytes_in_header = remain;
			break;
		}
		if (alread_sync) {
			ctx->hdr = gf_mp3_get_next_header_mem(ctx->header, 4, &bytes_skipped);
			if (!ctx->hdr) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[MP3Dmx] Lost sync since previous data block\n"));
				alread_sync = 0;
				continue;
			}
		} else {
			ctx->hdr = gf_mp3_get_next_header_mem(start, remain, &bytes_skipped);

			//couldn't find sync byte in this packet
			if (!ctx->hdr) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[MP3Dmx] Could not find sync word in packet, droping\n"));
				break;
			}
		}
		sync = start + bytes_skipped;

		size = gf_mp3_frame_size(ctx->hdr);

		//ready to send packet
		mp3_dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) return GF_OK;

		nb_samp = gf_mp3_window_size(ctx->hdr);

		if (alread_sync) {
			if (size - 4 > remain) {
				ctx->remaining = size - 4 - remain;
				size = remain+4;
			}
		} else {
			if (size > remain) {
				ctx->remaining = size - remain;
				size = remain;
			}
		}

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * ctx->sr);
			if (ctx->cts + nb_samp >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + nb_samp ) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}
		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

			if (alread_sync) {
				memcpy(output, ctx->header, 4);
				memcpy(output+4, sync, size-4);
			} else {
				memcpy(output, sync, size);
			}

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, nb_samp);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, ctx->remaining ? GF_FALSE : GF_TRUE);

			if (byte_offset != GF_FILTER_NO_BO) {
				u64 boffset = byte_offset;
				if (alread_sync) {
					boffset -= (4-alread_sync);
				} else {
					boffset += (char *) sync - data;
				}
				gf_filter_pck_set_byte_offset(dst_pck, boffset);
			}

			gf_filter_pck_send(dst_pck);
		}
		if (alread_sync) {
			alread_sync = 0;
			assert(size>4);
			size -= 4;
		}
		start += size;
		remain -= size;

		if (ctx->remaining) break;
		mp3_dmx_update_cts(ctx);
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void mp3_dmx_finalize(GF_Filter *filter)
{
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
}

static const GF_FilterCapability MP3DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/x-mp3|audio/mp3"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_UNFRAMED, GF_FALSE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mp3"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
};



#define OFFS(_n)	#_n, offsetof(GF_MP3DmxCtx, _n)
static const GF_FilterArgs MP3DmxArgs[] =
{
	{ OFFS(index_dur), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{0}
};


GF_FilterRegister MP3DmxRegister = {
	.name = "rfmp3",
	.description = "MPEG-1/2 audio reframer",
	.private_size = sizeof(GF_MP3DmxCtx),
	.args = MP3DmxArgs,
	.finalize = mp3_dmx_finalize,
	SETCAPS(MP3DmxCaps),
	.configure_pid = mp3_dmx_configure_pid,
	.process = mp3_dmx_process,
	.process_event = mp3_dmx_process_event
};


const GF_FilterRegister *mp3_dmx_register(GF_FilterSession *session)
{
	return &MP3DmxRegister;
}
