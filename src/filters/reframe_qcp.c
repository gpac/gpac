/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / AMR&EVRC&SMV reframer filter
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
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

typedef struct
{
	u64 pos;
	Double duration;
} QCPIdx;

typedef struct
{
	//filter args
	Double index;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid, sample_rate, block_size;
	Bool done;

	u64 cts;
	GF_Fraction duration;
	Double start_range;

	Bool in_seek;
	u32 timescale;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	u32 data_chunk_offset, data_chunk_size, data_chunk_remain;
	u32 resume_from;
	u32 remaining;
	u32 skip_bytes;
	u32 vrat_rate_flag, pck_size, rate_table_count;
	QCPRateTable rate_table[8];

	Bool hdr_processed;

	char *buffer;
	u32 buffer_alloc, buffer_size;

	GF_BitStream *bs;


	QCPIdx *indexes;
	u32 index_alloc_size, index_size;
} GF_QCPDmxCtx;




GF_Err qcpdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_QCPDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	return GF_OK;
}

static GF_Err qcpdmx_process_header(GF_Filter *filter, GF_QCPDmxCtx *ctx, char *data, u32 size, GF_BitStream *file_bs);

static void qcpdmx_check_dur(GF_Filter *filter, GF_QCPDmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	u32 i, chunk_size;
	GF_Err e;
	u32 data_chunk_size = 0;
	u64 duration, cur_dur;
	char magic[4];
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen(p->value.string, "rb");
	if (!stream) return;

	ctx->codecid = 0;
	ctx->sample_rate = 8000;
	ctx->block_size = 160;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	if (!ctx->hdr_processed ) {
		e = qcpdmx_process_header(filter, ctx, NULL, 0, bs);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[QCPDmx] Header parsed error %s\n", gf_error_to_string(e) ));
		}
	} else {
		gf_bs_skip_bytes(bs, 170);
	}
	while (gf_bs_available(bs) ) {
		gf_bs_read_data(bs, magic, 4);
		chunk_size = gf_bs_read_u32_le(bs);

		if (strncmp(magic, "data", 4)) {
			gf_bs_skip_bytes(bs, chunk_size);
			if (chunk_size%2) gf_bs_skip_bytes(bs, 1);
			continue;
		}
		data_chunk_size = chunk_size;
		break;
	}
	if (!data_chunk_size) {
		gf_bs_del(bs);
		gf_fclose(stream);
		return;
	}

	ctx->index_size = 0;
	ctx->data_chunk_offset = (u32) gf_ftell(stream);
	ctx->data_chunk_size = data_chunk_size;

	duration = 0;
	cur_dur = 0;
	while (data_chunk_size) {
		u32 idx, size=0;
		u64 pos;
		pos = gf_ftell(stream);
		/*get frame rate idx*/
		if (ctx->vrat_rate_flag) {
			idx = fgetc(stream);
			chunk_size-=1;
			for (i=0; i<ctx->rate_table_count; i++) {
				if (ctx->rate_table[i].rate_idx==idx) {
					size = ctx->rate_table[i].pck_size;
					break;
				}
			}
			gf_fseek(stream, size, SEEK_CUR);
			size++;
		} else {
			size = ctx->pck_size;
			gf_fseek(stream, size, SEEK_CUR);
		}
		data_chunk_size-= size;
		
		duration += ctx->block_size;
		cur_dur += ctx->block_size;
		if (cur_dur > ctx->index * ctx->sample_rate) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(QCPIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= ctx->sample_rate;
			ctx->index_size ++;
			cur_dur = 0;
		}
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->sample_rate != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->sample_rate;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static Bool qcpdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_QCPDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
			ctx->remaining = 0;
		}
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		qcpdmx_check_dur(filter, ctx);

		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->sample_rate);
					file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos) {
				return GF_TRUE;
			}
		}
		if (!file_pos) {
			file_pos = ctx->data_chunk_offset;
			ctx->data_chunk_remain = ctx->data_chunk_size;
		}

		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
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

/*QCP codec GUIDs*/
static const char *QCP_QCELP_GUID_1 = "\x41\x6D\x7F\x5E\x15\xB1\xD0\x11\xBA\x91\x00\x80\x5F\xB4\xB9\x7E";
static const char *QCP_QCELP_GUID_2 = "\x42\x6D\x7F\x5E\x15\xB1\xD0\x11\xBA\x91\x00\x80\x5F\xB4\xB9\x7E";
static const char *QCP_EVRC_GUID = "\x8D\xD4\x89\xE6\x76\x90\xB5\x46\x91\xEF\x73\x6A\x51\x00\xCE\xB4";
static const char *QCP_SMV_GUID = "\x75\x2B\x7C\x8D\x97\xA7\x46\xED\x98\x5E\xD5\x3C\x8C\xC7\x5F\x84";

static GF_Err qcpdmx_process_header(GF_Filter *filter, GF_QCPDmxCtx *ctx, char *data, u32 size, GF_BitStream *file_bs)
{
	char magic[12], GUID[17], name[81], fmt[162];
	u32 riff_size, chunk_size, i, avg_bps;
	Bool has_pad;
	const GF_PropertyValue *p;
	GF_BitStream *bs;

	bs = file_bs ? file_bs : gf_bs_new(data, size, GF_BITSTREAM_READ);

	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "RIFF", 4)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[QCPDmx] Broken file: RIFF header not found\n"));
		if (!file_bs) gf_bs_del(bs);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	riff_size = gf_bs_read_u32_le(bs);
	gf_bs_read_data(bs, fmt, 162);
	gf_bs_seek(bs, 8);
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "QLCM", 4)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[QCPDmx] Broken file: QLCM header not found\n"));
		if (!file_bs) gf_bs_del(bs);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
	if (p && p->value.longuint != riff_size+8) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[QCPDmx] Broken file:  RIFF-Size %d got %d\n", p->value.uint - 8, riff_size));
	}
	/*fmt*/
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "fmt ", 4)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[QCPDmx] Broken file: FMT not found\n"));
		if (!file_bs) gf_bs_del(bs);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	chunk_size = gf_bs_read_u32_le(bs);
	has_pad = (chunk_size%2) ? GF_TRUE : GF_FALSE;
	/*major = */gf_bs_read_u8(bs);
	/*minor = */gf_bs_read_u8(bs);
	chunk_size -= 2;
	/*codec info*/
	gf_bs_read_data(bs, GUID, 16);
	GUID[16]=0;
	/*version = */gf_bs_read_u16_le(bs);
	chunk_size -= 18;
	gf_bs_read_data(bs, name, 80);
	name[80]=0;
	chunk_size -= 80;
	avg_bps = gf_bs_read_u16_le(bs);
	ctx->pck_size = gf_bs_read_u16_le(bs);
	ctx->block_size = gf_bs_read_u16_le(bs);
	ctx->sample_rate = gf_bs_read_u16_le(bs);
	/*bps = */gf_bs_read_u16_le(bs);
	ctx->rate_table_count = gf_bs_read_u32_le(bs);
	chunk_size -= 14;
	/*skip var rate*/
	for (i=0; i<8; i++) {
		ctx->rate_table[i].pck_size = gf_bs_read_u8(bs);
		ctx->rate_table[i].rate_idx = gf_bs_read_u8(bs);
	}
	chunk_size -= 16;
	gf_bs_skip_bytes(bs, 5*4);/*reserved*/
	chunk_size -= 20;
	gf_bs_skip_bytes(bs, chunk_size);
	if (has_pad) gf_bs_read_u8(bs);

	if (!strncmp(GUID, QCP_QCELP_GUID_1, 16) || !strncmp(GUID, QCP_QCELP_GUID_2, 16)) {
		ctx->codecid = GF_CODECID_QCELP;
	} else if (!strncmp(GUID, QCP_EVRC_GUID, 16)) {
		ctx->codecid = GF_CODECID_EVRC;
	} else if (!strncmp(GUID, QCP_SMV_GUID, 16)) {
		ctx->codecid = GF_CODECID_SMV;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[QCPDmx] Unsupported codec GUID %s\n", GUID));
		if (!file_bs) gf_bs_del(bs);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	/*vrat*/
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "vrat", 4)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[QCPDmx] Broken file: VRAT not found\n"));
		if (!file_bs) gf_bs_del(bs);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	chunk_size = gf_bs_read_u32_le(bs);
	has_pad = (chunk_size%2) ? GF_TRUE : GF_FALSE;
	ctx->vrat_rate_flag = gf_bs_read_u32_le(bs);
	/*size_in_packet =*/gf_bs_read_u32_le(bs);
	chunk_size -= 8;
	gf_bs_skip_bytes(bs, chunk_size);
	if (has_pad) gf_bs_read_u8(bs);

	if (file_bs) return GF_OK;

	gf_bs_del(bs);


	ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(1) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(ctx->codecid ) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->block_size ) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(avg_bps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FRAME_SIZE, & PROP_UINT(ctx->pck_size));

	qcpdmx_check_dur(filter, ctx);

	return GF_OK;

}

GF_Err qcpdmx_process(GF_Filter *filter)
{
	GF_QCPDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u64 byte_offset;
	u8 *data, *output;
	u8 *start;
	u32 pck_size, remain;
	GF_Err e;
	//update duration
	qcpdmx_check_dur(filter, ctx);

	if (ctx->done) return GF_EOS;

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

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, ctx->remaining ? GF_FALSE : GF_TRUE);
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
			}
			gf_filter_pck_send(dst_pck);
		}
		assert (ctx->data_chunk_remain >= to_send);
		ctx->data_chunk_remain -= to_send;

		if (ctx->remaining) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		ctx->cts += ctx->block_size;
		start += to_send;
		remain -= to_send;

		if (!ctx->data_chunk_remain) {
			ctx->done = GF_TRUE;
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
	}

	if (ctx->resume_from) {
		start += ctx->resume_from;
		remain -= ctx->resume_from;
		ctx->resume_from = 0;
	}


	while (remain) {
		u32 i, chunk_size=0;
		u32 idx = 0;
		u32 size = 0;
		u64 b_offset;
		GF_FilterPacket *dst_pck;
		char magic[4];
		u8 *pck_data;
		Bool has_pad;

		if (!ctx->hdr_processed) {
			if (ctx->buffer_size + remain < 170) {
				if (ctx->buffer_alloc < ctx->buffer_size + remain) {
					ctx->buffer_alloc = ctx->buffer_size + remain;
					ctx->buffer = gf_realloc(ctx->buffer, ctx->buffer_alloc);
				}
				memcpy(ctx->buffer + ctx->buffer_size, start, remain);
				ctx->buffer_size += remain;
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_OK;
			}
			ctx->hdr_processed = GF_TRUE;
			if (ctx->buffer_size) {
				e = qcpdmx_process_header(filter, ctx, ctx->buffer, ctx->buffer_size, NULL);
			} else {
				e = qcpdmx_process_header(filter, ctx, start, remain, NULL);
			}
			start += 170 - ctx->buffer_size;
			remain -= 170 - ctx->buffer_size;
			ctx->buffer_size = 0;

			if (e) {
				gf_filter_setup_failure(filter, e);
				ctx->done = GF_TRUE;
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_EOS;
			}
			continue;
		}
		//skip current chunk
		if (ctx->skip_bytes) {
			if (remain<ctx->skip_bytes) {
				ctx->skip_bytes -= remain;
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_OK;
			}
			start += ctx->skip_bytes;
			remain -= ctx->skip_bytes;
			ctx->skip_bytes = 0;
		}

		//load chunk tag
		if (!ctx->data_chunk_remain) {
			//load chunk
			if (remain<8) {
				if (ctx->buffer_alloc < ctx->buffer_size + 8) {
					ctx->buffer_alloc = ctx->buffer_size + 8;
					ctx->buffer = gf_realloc(ctx->buffer, ctx->buffer_alloc);
				}
				memcpy(ctx->buffer + ctx->buffer_size, start, remain);
				ctx->buffer_size += remain;
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_OK;
			}
			if (!ctx->buffer_size) {
				if (!ctx->bs) {
					ctx->bs = gf_bs_new((u8 *) start, remain, GF_BITSTREAM_READ);
				} else {
					gf_bs_reassign_buffer(ctx->bs, start, remain);
				}
			} else {
				if (!ctx->bs) {
					ctx->bs = gf_bs_new((u8 *) ctx->buffer, ctx->buffer_size, GF_BITSTREAM_READ);
				} else {
					gf_bs_reassign_buffer(ctx->bs, ctx->buffer, ctx->buffer_size);
				}
			}

			gf_bs_read_data(ctx->bs, magic, 4);
			chunk_size = gf_bs_read_u32_le(ctx->bs);
			has_pad = (chunk_size%2) ? GF_TRUE : GF_FALSE;
			start += 8-ctx->buffer_size;
			remain -= 8-ctx->buffer_size;
			ctx->buffer_size = 0;

			//wait until we reach data chunk
			if (strnicmp(magic, "data", 4)) {
				ctx->skip_bytes = chunk_size;
				if (has_pad) ctx->skip_bytes++;
				continue;
			} else {
				ctx->data_chunk_size = ctx->data_chunk_remain = chunk_size;
			}
		}

		//we are in the data chunk
		if (!ctx->is_playing) {
			ctx->resume_from = (u32) ( (char *)start -  (char *)data);
			return GF_OK;
		}

		b_offset = gf_filter_pck_get_byte_offset(pck);
		if (b_offset != GF_FILTER_NO_BO) {
			b_offset += (start - (u8 *) data);
		}
		/*get frame rate idx*/
		if (ctx->vrat_rate_flag) {
			idx = start[0];
			//chunk_size-=1;
			for (i=0; i<ctx->rate_table_count; i++) {
				if (ctx->rate_table[i].rate_idx==idx) {
					size = ctx->rate_table[i].pck_size;
					break;
				}
			}
			size++;
		} else {
			size = ctx->pck_size;
		}

		if (size > remain) {
			ctx->remaining = size - remain;
			size = remain;
		} else {
			ctx->remaining = 0;
		}

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * ctx->sample_rate);
			if (ctx->cts + ctx->block_size >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->block_size ) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &pck_data);
			memcpy(pck_data, start, size);

			gf_filter_pck_set_framing(dst_pck, GF_TRUE, ctx->remaining ? GF_FALSE : GF_TRUE);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_duration(dst_pck, ctx->block_size);
			if (b_offset != GF_FILTER_NO_BO)
				gf_filter_pck_set_byte_offset(dst_pck, b_offset);

			gf_filter_pck_send(dst_pck);
		}

		assert (ctx->data_chunk_remain >= size);
		ctx->data_chunk_remain -= size;
		if (!ctx->data_chunk_remain) {
			ctx->done = GF_TRUE;
			gf_filter_pid_set_eos(ctx->opid);
			break;
		}

		if (ctx->remaining) break;
		ctx->cts += ctx->block_size;
		start += size;
		remain -= size;


		//don't demux too much of input, abort when we would block. This avoid dispatching
		//a huge number of frames in a single call
		if (gf_filter_pid_would_block(ctx->opid)) {
			ctx->resume_from = (u32) ((char *)start -  (char *)data);
			return GF_OK;
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void qcpdmx_finalize(GF_Filter *filter)
{
	GF_QCPDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->buffer) gf_free(ctx->buffer);
}

static const char *qcpdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	char magic[5];
	Bool is_qcp = GF_TRUE;
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);

	magic[4] = 0;
	gf_bs_read_data(bs, magic, 4);
	if (strnicmp(magic, "RIFF", 4)) {
		is_qcp = GF_FALSE;
	} else {
		/*riff_size = */gf_bs_read_u32_le(bs);
		gf_bs_read_data(bs, magic, 4);
		if (strnicmp(magic, "QLCM", 4)) {
			is_qcp = GF_FALSE;
		}
	}
	gf_bs_del(bs);
	if (!is_qcp) return NULL;
	*score = GF_FPROBE_SUPPORTED;
	return "audio/qcp";
}

static const GF_FilterCapability QCPDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/qcp"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_QCELP),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_SMV),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_EVRC),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "qcp"),
};


#define OFFS(_n)	#_n, offsetof(GF_QCPDmxCtx, _n)
static const GF_FilterArgs QCPDmxArgs[] =
{
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister QCPDmxRegister = {
	.name = "rfqcp",
	GF_FS_SET_DESCRIPTION("QCP reframer")
	GF_FS_SET_HELP("This filter parses QCP files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_QCPDmxCtx),
	.args = QCPDmxArgs,
	.finalize = qcpdmx_finalize,
	SETCAPS(QCPDmxCaps),
	.configure_pid = qcpdmx_configure_pid,
	.process = qcpdmx_process,
	.probe_data = qcpdmx_probe_data,
	.process_event = qcpdmx_process_event
};


const GF_FilterRegister *qcpdmx_register(GF_FilterSession *session)
{
	return &QCPDmxRegister;
}

