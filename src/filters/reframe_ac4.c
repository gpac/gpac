/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / AC4 reframer filter
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

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_RFAC4)

typedef struct
{
	u64 pos;
	Double duration;
} AC4Idx;

typedef struct
{
	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts;
	u32 sample_rate, nb_ch;
	GF_Fraction64 duration;
	Double start_range;
	Bool in_seek;
	u32 in_timescale, out_timescale;

	GF_AC4Config hdr;
	u8 *ac4_buffer;
	u32 ac4_buffer_size, ac4_buffer_alloc, resume_from;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	AC4Idx *indexes;
	u32 index_alloc_size, index_size;
	u32 bitrate;
	Bool copy_props;
} GF_AC4DmxCtx;

GF_Err ac4dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_AC4DmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}

	if (!gf_filter_pid_check_caps(pid)) {
		return GF_NOT_SUPPORTED;
	}

	ctx->ipid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) {
		ctx->in_timescale = p->value.uint;
	}

	if (ctx->in_timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);

		//make sure we move to audio (may happen if source filter is writegen)
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_STREAM_TYPE);
		if (!p || (p->value.uint==GF_STREAM_FILE)) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
		}
	}
	if (ctx->in_timescale) {
		ctx->copy_props = GF_TRUE;
	}

	return GF_OK;
}

static void ac4dmx_check_dur(GF_Filter *filter, GF_AC4DmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	GF_AC4Config hdr;
	u64 duration, rate;
	u32 sample_rate = 0;
	const GF_PropertyValue *p;

	// ac4dmx_check_dur() is only done once
	if (!ctx->opid || ctx->in_timescale || ctx->file_loaded) {
		return;
	}

	// open the stream
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string || !strncmp(p->value.string, "gmem://", 7)) {
		ctx->is_file = GF_FALSE;
		ctx->file_loaded = GF_TRUE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen_ex(p->value.string, NULL, "rb", GF_TRUE);
	if (!stream) {
		if (gf_fileio_is_main_thread(p->value.string))
			ctx->file_loaded = GF_TRUE;
		return;
	}

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	duration = 0;

	// duration
	while (gf_ac4_parser_bs(bs, &hdr, GF_FALSE)) {
		duration += ctx->hdr.sample_duration;
		sample_rate = hdr.sample_rate;

		if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
		else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;

		ctx->indexes = gf_realloc(ctx->indexes, sizeof(AC4Idx)*ctx->index_alloc_size);
		ctx->indexes[ctx->index_size].pos = gf_bs_get_position(bs) + hdr.header_size;
		ctx->indexes[ctx->index_size].duration = (Double) duration;
		ctx->indexes[ctx->index_size].duration /= hdr.sample_rate;
		ctx->index_size ++;

		gf_bs_skip_bytes(bs, hdr.frame_size + hdr.header_size + hdr.crc_size);
	}

	rate = gf_bs_get_position(bs);
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * sample_rate != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = sample_rate;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));

		// bitrate
		if (duration && !gf_sys_is_test_mode() ) {
			rate *= 8 * ctx->duration.den;
			rate /= ctx->duration.num;
			ctx->bitrate = (u32) rate;
		}
	}

	// set ctx->file_loaded
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
}

static void ac4dmx_check_pid(GF_Filter *filter, GF_AC4DmxCtx *ctx)
{
	u8 *data;
	u32 size;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		ac4dmx_check_dur(filter, ctx);
	}

	// return if key parameters remain unchanged
	if ((ctx->sample_rate == ctx->hdr.sample_rate) && ctx->nb_ch >= ctx->hdr.channel_count && !ctx->copy_props) {
		return;
	}

	ctx->copy_props = GF_FALSE;

	// copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);

	const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_STREAM_TYPE);
	if (!p || (p->value.uint==GF_STREAM_FILE)) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->hdr.sample_duration));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, & PROP_BOOL(GF_FALSE) );

	if (ctx->duration.num) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));
	}

	if (!ctx->in_timescale && !gf_sys_is_test_mode()) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE));
	}

	if (!ctx->copy_props && ctx->hdr.channel_count > ctx->nb_ch) {
		ctx->nb_ch = ctx->hdr.channel_count;
	}

	if (!ctx->in_timescale) {
		// we change sample rate, change cts
		if (ctx->cts && (ctx->out_timescale != ctx->hdr.media_time_scale)) {
			ctx->cts = gf_timestamp_rescale(ctx->cts, ctx->out_timescale, ctx->hdr.media_time_scale);
		}
	}
	ctx->sample_rate = ctx->hdr.sample_rate;
	ctx->out_timescale = ctx->hdr.media_time_scale;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->out_timescale));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(GF_CODECID_AC4) );
	gf_odf_ac4_cfg_write(&ctx->hdr, &data, &size);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(data, size) );

	if (ctx->bitrate) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(ctx->bitrate));
	}
}

static Bool ac4dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_AC4DmxCtx *ctx = gf_filter_get_udta(filter);

	if (evt->base.type == GF_FEVT_PLAY) {
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
		}

		if (!ctx->is_file) {
			return GF_FALSE;
		}

		// locate the start position and cts
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i = 1; i < ctx->index_size; i++) {
				if (ctx->indexes[i].duration > ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i - 1].duration * ctx->out_timescale);
					ctx->file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;

			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos) {
				return GF_TRUE;
			}
		}

		ctx->ac4_buffer_size = 0;
		ctx->resume_from = 0;

		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		return GF_TRUE; // cancel event
	} else if (evt->base.type == GF_FEVT_STOP) {
		ctx->is_playing = GF_FALSE;
		ctx->cts = 0;

		return GF_FALSE; // don't cancel event
	} else if (evt->base.type == GF_FEVT_SET_SPEED) {
		return GF_TRUE; // cancel event
	}

	// by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

GF_Err ac4dmx_process(GF_Filter *filter)
{
	GF_AC4DmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *output;
	u8 *start;
	u32 pck_size, remain, prev_pck_size;
	u64 cts;
	u32 sync_framesize = 0;
	Bool is_first = GF_TRUE;

restart:
	cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num) {
		ac4dmx_check_dur(filter, ctx);
	}

	if (ctx->opid && !ctx->is_playing) {
		return GF_OK;
	}

	// get new pck from input
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->ac4_buffer_size) {
				if (ctx->opid) {
					gf_filter_pid_set_eos(ctx->opid);
				}
				if (ctx->src_pck) {
					gf_filter_pck_unref(ctx->src_pck);
				}
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->ac4_buffer_size;
	if (pck && !ctx->resume_from) {
		// get data from pck
		const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
		if (!pck_size) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}

		// copy to ctx->ac4_buffer
		if (ctx->ac4_buffer_size + pck_size > ctx->ac4_buffer_alloc) {
			ctx->ac4_buffer_alloc = ctx->ac4_buffer_size + pck_size;
			ctx->ac4_buffer = gf_realloc(ctx->ac4_buffer, ctx->ac4_buffer_alloc);
		}
		memcpy(ctx->ac4_buffer + ctx->ac4_buffer_size, data, pck_size);
		ctx->ac4_buffer_size += pck_size;
	}

	remain = ctx->ac4_buffer_size;
	start = ctx->ac4_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	if (!ctx->bs) {
		ctx->bs = gf_bs_new(start, remain, GF_BITSTREAM_READ);
	}
	else {
		gf_bs_reassign_buffer(ctx->bs, start, remain);
	}

	while (remain) {
		u8 *sync;
		Bool res;
		u32 sync_pos, bytes_to_drop=0;

		res = gf_ac4_parser_bs(ctx->bs, &(ctx->hdr), is_first);

		sync_pos = (u32) gf_bs_get_position(ctx->bs);
		sync_framesize = ctx->hdr.frame_size + ctx->hdr.header_size + ctx->hdr.crc_size;

		//if not end of stream or no valid frame
		if (pck || !sync_framesize) {
			//startcode not found or not enough bytes, gather more
			if (!res || (remain < sync_pos + sync_framesize)) {
				if (sync_pos && sync_framesize) {
					start += sync_pos;
					remain -= sync_pos;
				}
				break;
			}
			ac4dmx_check_pid(filter, ctx);
		}

		if (is_first && ctx->in_timescale) {
			// input pid sets some timescale - we flushed pending data , update cts
			if (pck) {
				cts = gf_filter_pck_get_cts(pck);
				//move to output timescale
				if (cts != GF_FILTER_NO_TS)
					cts = gf_timestamp_rescale(cts, ctx->in_timescale, ctx->out_timescale);

				//init cts at first packet
				if (!ctx->cts && (cts != GF_FILTER_NO_TS)) {
					ctx->cts = cts;
				}
			}
			// avoids updating cts
			if (cts == GF_FILTER_NO_TS) {
				prev_pck_size = 0;
			}
		}


		is_first = GF_FALSE;

		if (!ctx->is_playing) {
			ctx->resume_from = 1 + ctx->ac4_buffer_size - remain;
			return GF_OK;
		}

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * ctx->hdr.media_time_scale);
			if (ctx->cts + ctx->hdr.sample_duration >= nb_samples_at_seek) {
				ctx->in_seek = GF_FALSE;
			}
		}

		if (ctx->in_timescale && !prev_pck_size && (cts != GF_FILTER_NO_TS)) {
			//trust input CTS if diff is more than one sec
			if ((cts > ctx->cts + ctx->out_timescale) || (ctx->cts > cts + ctx->out_timescale)) {
				ctx->cts = cts;
			}
			cts = GF_FILTER_NO_TS;
		}

		// generate the dst_pck
		if (!ctx->in_seek && remain >= sync_pos + sync_framesize) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->hdr.frame_size, &output);
			if (!dst_pck) {
				return GF_OUT_OF_MEM;
			}

			if (ctx->src_pck) {
				gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
			}

			// sync is the position of raw_ac4_frame()
			sync = start + sync_pos + ctx->hdr.header_size;
			memcpy(output, sync, ctx->hdr.frame_size);

			gf_filter_pck_set_dts(dst_pck, ctx->cts);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			//ctx->hdr.sample_duration is always in out_timescale units
			gf_filter_pck_set_duration(dst_pck, ctx->hdr.sample_duration);

			gf_filter_pck_set_sap(dst_pck, ctx->hdr.stream.b_iframe_global ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

			// send dst_pck
			gf_filter_pck_send(dst_pck);
		}

		// update cts - sample_duration is always in output timescale
		ctx->cts += ctx->hdr.sample_duration;

		// calculate memory offset
		bytes_to_drop = sync_pos + sync_framesize;
		if (bytes_to_drop > remain) {
			bytes_to_drop = remain; // truncated last frame
		}
		if (bytes_to_drop == 0) {
			bytes_to_drop = 1;
		}

		start += bytes_to_drop;
		remain -= bytes_to_drop;
		gf_bs_reassign_buffer(ctx->bs, start, remain);

		if (prev_pck_size) {
			if (prev_pck_size > bytes_to_drop) {
				prev_pck_size -= bytes_to_drop;
			}
			else {
				prev_pck_size = 0;
				if (ctx->src_pck) {
					gf_filter_pck_unref(ctx->src_pck);
				}
				ctx->src_pck = pck;
				if (pck) {
					gf_filter_pck_ref_props(&ctx->src_pck);
				}
			}
		}
	}

	if (!pck) {
		ctx->ac4_buffer_size = 0;
		//avoid recursive call
		goto restart;
	} else {
		if (remain && (remain < ctx->ac4_buffer_size)) {
			memmove(ctx->ac4_buffer, start, remain);
		}
		if (!ctx->src_pck && pck) {
			ctx->src_pck = pck;
			gf_filter_pck_ref_props(&ctx->src_pck);
		}
		ctx->ac4_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}

static void ac4dmx_finalize(GF_Filter *filter)
{
	GF_AC4DmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->ac4_buffer) gf_free(ctx->ac4_buffer);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);

	gf_odf_ac4_cfg_clean_list(&(ctx->hdr));
}

static const char *ac4dmx_probe_data(const u8 *_data, u32 _size, GF_FilterProbeScore *score)
{
	u32 nb_frames = 0, sync_framesize = 0, pos = 0;
	u32 nb_broken_frames = GF_FALSE;
	GF_AC4Config ahdr = {0};

	u32 lt = gf_log_get_tool_level(GF_LOG_CODING);
	gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_QUIET);

	GF_BitStream *bs = gf_bs_new(_data, _size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs) && nb_frames <= 4) {
		Bool bytes_lost=GF_FALSE;
		if (!gf_ac4_parser_bs(bs, &ahdr, GF_FALSE)) {
			if (ahdr.sample_rate) nb_frames++;
			break;
		}

		if (pos != (u32) gf_bs_get_position(bs)) {
			bytes_lost=GF_TRUE;
			nb_broken_frames++;
		}

		nb_frames += 1;
		sync_framesize = ahdr.frame_size + ahdr.header_size + ahdr.crc_size;
		gf_bs_skip_bytes(bs, sync_framesize);
		if (!pos && !bytes_lost && (nb_frames==1) && !gf_bs_available(bs)) nb_frames++;
		pos += sync_framesize;
	}

	gf_log_set_tool_level(GF_LOG_CODING, lt);
	gf_bs_del(bs);

	if (nb_frames>=2) {
		*score = nb_broken_frames ? GF_FPROBE_MAYBE_NOT_SUPPORTED : GF_FPROBE_SUPPORTED;
		return "audio/ac4";
	}

	return NULL;
}

static const GF_FilterCapability AC4DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ac4"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/x-ac4|audio/ac4"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AC4),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_FALSE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC4),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
/*	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_HLS_SAMPLE_AES_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC4),
*/

};

#define OFFS(_n)	#_n, offsetof(GF_AC4DmxCtx, _n)
static const GF_FilterArgs AC4DmxArgs[] =
{
	{0}
};

GF_FilterRegister AC4DmxRegister = {
	.name = "rfac4",
	GF_FS_SET_DESCRIPTION("AC4 reframer")
	GF_FS_SET_HELP("This filter parses AC4 files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_AC4DmxCtx),
	.args = AC4DmxArgs,
	.finalize = ac4dmx_finalize,
	SETCAPS(AC4DmxCaps),
	.configure_pid = ac4dmx_configure_pid,
	.process = ac4dmx_process,
	.probe_data = ac4dmx_probe_data,
	.process_event = ac4dmx_process_event,
	.hint_class_type = GF_FS_CLASS_FRAMING
};


const GF_FilterRegister *rfac4_register(GF_FilterSession *session)
{
	return &AC4DmxRegister;
}
#else
const GF_FilterRegister *rfac4_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_RFAC4)
