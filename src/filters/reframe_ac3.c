/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / AC3 reframer filter
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

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	u64 pos;
	Double duration;
} AC3Idx;

#define AC3_FRAME_SIZE 1536

typedef struct
{
	//filter args
	Double index;

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
	u32 timescale;

	GF_AC3Config hdr;
	u8 *ac3_buffer;
	u32 ac3_buffer_size, ac3_buffer_alloc, resume_from;
	u64 byte_offset;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	Bool is_eac3;
	Bool (*ac3_parser_bs)(GF_BitStream*, GF_AC3Config*, Bool);

	GF_FilterPacket *src_pck;

	AC3Idx *indexes;
	u32 index_alloc_size, index_size;
	u32 bitrate;
} GF_AC3DmxCtx;




GF_Err ac3dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_AC3DmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) ctx->timescale = p->value.uint;

	ctx->ac3_parser_bs = gf_ac3_parser_bs;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p && p->value.uint==GF_CODECID_EAC3) ctx->is_eac3 = GF_TRUE;
	else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && p->value.string && strstr(p->value.string, "eac3")) ctx->is_eac3 = GF_TRUE;
		else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string && strstr(p->value.string, "eac3")) ctx->is_eac3 = GF_TRUE;
		}
	}
	if (ctx->is_eac3) {
		ctx->ac3_parser_bs = gf_eac3_parser_bs;
	}

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	return GF_OK;
}

static void ac3dmx_check_dur(GF_Filter *filter, GF_AC3DmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	GF_AC3Config hdr;
	u64 duration, cur_dur, rate;
	s32 sr = -1;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	if (ctx->index<=0) {
		ctx->file_loaded = GF_TRUE;
		return;
	}

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

	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	duration = 0;
	cur_dur = 0;
	while (	ctx->ac3_parser_bs(bs, &hdr, GF_FALSE) ) {
		if ((sr>=0) && (sr != hdr.sample_rate)) {
			duration *= hdr.sample_rate;
			duration /= sr;

			cur_dur *= hdr.sample_rate;
			cur_dur /= sr;
		}
		sr = hdr.sample_rate;
		duration += AC3_FRAME_SIZE;
		cur_dur += AC3_FRAME_SIZE;
		if (cur_dur > ctx->index * sr) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(AC3Idx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = gf_bs_get_position(bs);
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= sr;
			ctx->index_size ++;
			cur_dur = 0;
		}

		gf_bs_skip_bytes(bs, hdr.framesize);
	}
	rate = gf_bs_get_position(bs);
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * sr != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = sr;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));

		if (duration && !gf_sys_is_test_mode() ) {
			rate *= 8 * ctx->duration.den;
			rate /= ctx->duration.num;
			ctx->bitrate = (u32) rate;
		}
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void ac3dmx_check_pid(GF_Filter *filter, GF_AC3DmxCtx *ctx)
{
	u8 *data;
	u32 size;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		ac3dmx_check_dur(filter, ctx);
	}
	if ((ctx->sample_rate == ctx->hdr.sample_rate) && (ctx->nb_ch == ctx->hdr.channels) ) return;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	//don't change codec type if reframing an ES (for HLS SAES)
	if (!ctx->timescale)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(AC3_FRAME_SIZE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, & PROP_BOOL(GF_FALSE) );

	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));


	ctx->nb_ch = ctx->hdr.channels;
	if (!ctx->timescale) {
		//we change sample rate, change cts
		if (ctx->cts && (ctx->sample_rate != ctx->hdr.sample_rate)) {
			ctx->cts = gf_timestamp_rescale(ctx->cts, ctx->sample_rate, ctx->hdr.sample_rate);
		}
	}
	ctx->sample_rate = ctx->hdr.sample_rate;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(ctx->is_eac3 ? GF_CODECID_EAC3 : GF_CODECID_AC3) );

	ctx->hdr.is_ec3 = ctx->is_eac3;
	gf_odf_ac3_cfg_write(&ctx->hdr, &data, &size);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(data, size) );

	if (ctx->bitrate) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(ctx->bitrate));
	}

	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
}

static Bool ac3dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_AC3DmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
			ctx->ac3_buffer_size = 0;
			ctx->resume_from = 0;
		}
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->sample_rate);
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
		ctx->ac3_buffer_size = 0;
		ctx->resume_from = 0;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//don't cancel event
		ctx->is_playing = GF_FALSE;
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

static GFINLINE void ac3dmx_update_cts(GF_AC3DmxCtx *ctx)
{
	if (ctx->timescale) {
		u64 inc = AC3_FRAME_SIZE;
		inc *= ctx->timescale;
		inc /= ctx->sample_rate;
		ctx->cts += inc;
	} else {
		ctx->cts += AC3_FRAME_SIZE;
	}
}

GF_Err ac3dmx_process(GF_Filter *filter)
{
	GF_AC3DmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *output;
	u8 *start;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num)
		ac3dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->ac3_buffer_size) {
				if (ctx->opid)
					gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->ac3_buffer_size;
	if (pck && !ctx->resume_from) {
		const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
		if (!pck_size) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(pck);
			if (!ctx->ac3_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->ac3_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->ac3_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->ac3_buffer_size;
				}
			}
		}

		if (ctx->ac3_buffer_size + pck_size > ctx->ac3_buffer_alloc) {
			ctx->ac3_buffer_alloc = ctx->ac3_buffer_size + pck_size;
			ctx->ac3_buffer = gf_realloc(ctx->ac3_buffer, ctx->ac3_buffer_alloc);
		}
		memcpy(ctx->ac3_buffer + ctx->ac3_buffer_size, data, pck_size);
		ctx->ac3_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->ac3_buffer_size;
	start = ctx->ac3_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	if (!ctx->bs) {
		ctx->bs = gf_bs_new(start, remain, GF_BITSTREAM_READ);
	} else {
		gf_bs_reassign_buffer(ctx->bs, start, remain);
	}
	while (remain) {
		u8 *sync;
		Bool res;
		u32 sync_pos, bytes_to_drop=0;


		res = ctx->ac3_parser_bs(ctx->bs, &ctx->hdr, GF_TRUE);

		sync_pos = (u32) gf_bs_get_position(ctx->bs);

		//startcode not found or not enough bytes, gather more
		if (!res || (remain < sync_pos + ctx->hdr.framesize))
			break;

		ac3dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) {
			ctx->resume_from = 1 + ctx->ac3_buffer_size - remain;
			return GF_OK;
		}

		if (sync_pos) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[AC3Dmx] %d bytes unrecovered before sync word\n", sync_pos));
		}
		sync = start + sync_pos;

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * ctx->hdr.sample_rate);
			if (ctx->cts + AC3_FRAME_SIZE >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		bytes_to_drop = sync_pos + ctx->hdr.framesize;
		if (ctx->timescale && !prev_pck_size &&  (cts != GF_FILTER_NO_TS) ) {
			//trust input CTS if diff is more than one sec
			if ((cts > ctx->cts + ctx->timescale) || (ctx->cts > cts + ctx->timescale))
				ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->hdr.framesize, &output);
			if (!dst_pck) return GF_OUT_OF_MEM;
			
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

			memcpy(output, sync, ctx->hdr.framesize);
			gf_filter_pck_set_dts(dst_pck, ctx->cts);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, AC3_FRAME_SIZE);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, ctx->byte_offset + ctx->hdr.framesize);
			}

			gf_filter_pck_send(dst_pck);
		}
		ac3dmx_update_cts(ctx);

		//truncated last frame
		if (bytes_to_drop>remain) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] truncated AC3 frame!\n"));
			bytes_to_drop=remain;
		}

		if (!bytes_to_drop) {
			bytes_to_drop = 1;
		}
		start += bytes_to_drop;
		remain -= bytes_to_drop;
		gf_bs_reassign_buffer(ctx->bs, start, remain);

		if (prev_pck_size) {
			if (prev_pck_size > bytes_to_drop) prev_pck_size -= bytes_to_drop;
			else {
				prev_pck_size=0;
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = pck;
				if (pck)
					gf_filter_pck_ref_props(&ctx->src_pck);
			}
		}
		if (ctx->byte_offset != GF_FILTER_NO_BO)
			ctx->byte_offset += bytes_to_drop;
	}

	if (!pck) {
		ctx->ac3_buffer_size = 0;
		return ac3dmx_process(filter);
	} else {
		if (remain) {
			memmove(ctx->ac3_buffer, start, remain);
		}
		ctx->ac3_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}

static void ac3dmx_finalize(GF_Filter *filter)
{
	GF_AC3DmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->ac3_buffer) gf_free(ctx->ac3_buffer);
	if (ctx->indexes) gf_free(ctx->indexes);
}

static const char *ac3dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_frames=0;
	Bool has_broken_frames = GF_FALSE;
	u32 pos=0;
	while (1) {
		GF_AC3Config ahdr;
		if (! gf_ac3_parser((u8 *) data, size, &pos, &ahdr, GF_FALSE) )
		 	break;
		u32 fsize = ahdr.framesize;
		if (pos) {
			nb_frames=0;
			has_broken_frames = GF_TRUE;
			//what is before is bigger than max ac3 frame size (1920), this is packaged ac3 (mkv) at best
			if (pos > 2000)
				break;
		}
		nb_frames++;
		if (fsize > size+pos) break;
		if (nb_frames>4) break;
		if (size < fsize+pos) break;
		size -= fsize+pos;
		data += fsize+pos;
	}
	if (nb_frames>2) {
		*score = has_broken_frames ? GF_FPROBE_MAYBE_NOT_SUPPORTED : GF_FPROBE_SUPPORTED;
		return "audio/ac3";
	}
	return NULL;
}

static const GF_FilterCapability AC3DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ac3|eac3"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/x-ac3|audio/ac3|audio/x-eac3|audio/eac3"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_EAC3),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EAC3),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_HLS_SAMPLE_AES_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EAC3),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_AC3DmxCtx, _n)
static const GF_FilterArgs AC3DmxArgs[] =
{
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister AC3DmxRegister = {
	.name = "rfac3",
	GF_FS_SET_DESCRIPTION("AC3 reframer")
	GF_FS_SET_HELP("This filter parses AC3 and E-AC3 files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_AC3DmxCtx),
	.args = AC3DmxArgs,
	.finalize = ac3dmx_finalize,
	SETCAPS(AC3DmxCaps),
	.configure_pid = ac3dmx_configure_pid,
	.process = ac3dmx_process,
	.probe_data = ac3dmx_probe_data,
	.process_event = ac3dmx_process_event
};


const GF_FilterRegister *ac3dmx_register(GF_FilterSession *session)
{
	return &AC3DmxRegister;
}

#else

const GF_FilterRegister *ac3dmx_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_AV_PARSERS
