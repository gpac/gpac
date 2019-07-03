/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC ADTS reframer filter
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
#include <gpac/internal/media_dev.h>

typedef struct
{
	u64 pos;
	Double duration;
} LATMIdx;

typedef struct
{
	//filter args
	u32 frame_size;
	Double index;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts;
	u32 sr_idx, nb_ch, base_object_type;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;

	GF_M4ADecSpecInfo acfg;
	
	char *latm_buffer;
	u32 latm_buffer_size, latm_buffer_alloc;
	u32 dts_inc;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	LATMIdx *indexes;
	u32 index_alloc_size, index_size;
	u32 resume_from;
} GF_LATMDmxCtx;


static Bool latm_dmx_sync_frame_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *acfg, u32 *nb_bytes, u8 *buffer, u32 *nb_skipped)
{
	u32 val, size;
	u64 pos, mux_size;
	if (nb_skipped) *nb_skipped = 0;
	if (!acfg) return 0;

	while (gf_bs_available(bs)>3) {
		val = gf_bs_read_u8(bs);
		if (val!=0x56) {
			if (nb_skipped) (*nb_skipped) ++;
			continue;
		}
		val = gf_bs_read_int(bs, 3);
		if (val != 0x07) {
			gf_bs_read_int(bs, 5);
			if (nb_skipped) (*nb_skipped) ++;
			continue;
		}
		mux_size = gf_bs_read_int(bs, 13);
		pos = gf_bs_get_position(bs);
		if (mux_size>gf_bs_available(bs) ) {
			gf_bs_seek(bs, pos-3);
			return GF_FALSE;
		}

		/*use same stream mux*/
		if (!gf_bs_read_int(bs, 1)) {
			Bool amux_version, amux_versionA;

			amux_version = (Bool)gf_bs_read_int(bs, 1);
			amux_versionA = GF_FALSE;
			if (amux_version) amux_versionA = (Bool)gf_bs_read_int(bs, 1);
			if (!amux_versionA) {
				u32 i, allStreamsSameTimeFraming, numProgram;
				if (amux_version) gf_latm_get_value(bs);

				allStreamsSameTimeFraming = gf_bs_read_int(bs, 1);
				/*numSubFrames = */gf_bs_read_int(bs, 6);
				numProgram = gf_bs_read_int(bs, 4);
				for (i=0; i<=numProgram; i++) {
					u32 j, num_lay;
					num_lay = gf_bs_read_int(bs, 3);
					for (j=0; j<=num_lay; j++) {
						u32 frameLengthType;
						Bool same_cfg = GF_FALSE;
						if (i || j) same_cfg = (Bool)gf_bs_read_int(bs, 1);

						if (!same_cfg) {
							if (amux_version==1) gf_latm_get_value(bs);
							gf_m4a_parse_config(bs, acfg, GF_FALSE);
						}
						frameLengthType = gf_bs_read_int(bs, 3);
						if (!frameLengthType) {
							/*latmBufferFullness = */gf_bs_read_int(bs, 8);
							if (!allStreamsSameTimeFraming) {
							}
						} else {
							/*not supported*/
						}
					}

				}
				/*other data present*/
				if (gf_bs_read_int(bs, 1)) {
//					u32 k = 0;
				}
				/*CRCcheck present*/
				if (gf_bs_read_int(bs, 1)) {
				}
			}
		}

		size = 0;
		while (1) {
			u32 tmp = gf_bs_read_int(bs, 8);
			size += tmp;
			if (tmp!=255) break;
		}
		if (gf_bs_available(bs) < size) {
			gf_bs_seek(bs, pos-3);
			return GF_FALSE;
		}

		if (nb_bytes) {
			*nb_bytes = (u32) size;
		}

		if (buffer) {
			gf_bs_read_data(bs, (char *) buffer, size);
		} else {
			while (size) {
				gf_bs_read_int(bs, 8);
				size--;
			}
		}

		/*parse amux*/
		gf_bs_seek(bs, pos + mux_size);

		if ((gf_bs_available(bs)>2) && gf_bs_peek_bits(bs, 11, 0) != 0x2B7) {
			gf_bs_seek(bs, pos + 1);
			if (nb_skipped) (*nb_skipped) ++;
			continue;
		}

		return GF_TRUE;
	}
	return GF_FALSE;
}



GF_Err latm_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_LATMDmxCtx *ctx = gf_filter_get_udta(filter);

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

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}

	return GF_OK;
}

static void latm_dmx_check_dur(GF_Filter *filter, GF_LATMDmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	GF_M4ADecSpecInfo acfg;
	u64 duration, cur_dur, cur_pos;
	s32 sr_idx = -1;
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

	ctx->index_size = 0;

	memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));


	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	duration = 0;
	cur_dur = 0;
	cur_pos = gf_bs_get_position(bs);
	while (latm_dmx_sync_frame_bs(bs, &acfg, 0, NULL, NULL)) {
		if ((sr_idx>=0) && (sr_idx != acfg.base_sr_index)) {
			duration *= GF_M4ASampleRates[acfg.base_sr_index];
			duration /= GF_M4ASampleRates[sr_idx];

			cur_dur *= GF_M4ASampleRates[acfg.base_sr_index];
			cur_dur /= GF_M4ASampleRates[sr_idx];
		}
		sr_idx = acfg.base_sr_index;
		duration += ctx->frame_size;
		cur_dur += ctx->frame_size;
		if (cur_dur > ctx->index * GF_M4ASampleRates[sr_idx]) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(LATMIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = cur_pos;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= GF_M4ASampleRates[sr_idx];
			ctx->index_size ++;
			cur_dur = 0;
		}

		cur_pos = gf_bs_get_position(bs);
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * GF_M4ASampleRates[sr_idx] != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = GF_M4ASampleRates[sr_idx];

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void latm_dmx_check_pid(GF_Filter *filter, GF_LATMDmxCtx *ctx)
{
	u8 *dsi_b;
	u32 dsi_s, sr, timescale=0;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		latm_dmx_check_dur(filter, ctx);
	}

	if ((ctx->sr_idx == ctx->acfg.base_sr_index) && (ctx->nb_ch == ctx->acfg.nb_chan )
		&& (ctx->base_object_type == ctx->acfg.base_object_type) ) return;

	//copy properties at init or reconfig
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_AAC_MPEG4));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->frame_size) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, & PROP_BOOL(GF_FALSE) );
	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));


	ctx->nb_ch = ctx->acfg.nb_chan;
	ctx->base_object_type = ctx->acfg.base_object_type;

	sr = GF_M4ASampleRates[ctx->acfg.base_sr_index];
	if (!ctx->timescale) {
		//we change sample rate, change cts
		if (ctx->cts && (ctx->sr_idx != ctx->acfg.base_sr_index)) {
			ctx->cts *= sr;
			ctx->cts /= GF_M4ASampleRates[ctx->sr_idx];
		}
	}
	ctx->sr_idx = ctx->acfg.base_sr_index;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(GF_CODECID_AAC_MPEG4) );

	ctx->dts_inc = ctx->frame_size;
	gf_m4a_write_config(&ctx->acfg, &dsi_b, &dsi_s);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi_b, dsi_s) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PROFILE_LEVEL, & PROP_UINT (ctx->acfg.audioPL) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(sr));

	timescale = sr;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : timescale));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );

}

static Bool latm_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_LATMDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
		}
		if (! ctx->is_file) {
			if (evt->play.start_range || ctx->initial_play_done) {
				ctx->latm_buffer_size = 0;
				ctx->resume_from = 0;
			}

			ctx->initial_play_done = GF_TRUE;
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * GF_M4ASampleRates[ctx->sr_idx]);
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
		ctx->latm_buffer_size = 0;
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

static GFINLINE void latm_dmx_update_cts(GF_LATMDmxCtx *ctx)
{
	assert(ctx->dts_inc);

	if (ctx->timescale) {
		u64 inc = ctx->dts_inc;
		inc *= ctx->timescale;
		inc /= GF_M4ASampleRates[ctx->sr_idx];
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->dts_inc;
	}
}

GF_Err latm_dmx_process(GF_Filter *filter)
{
	GF_LATMDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u32 pos;
	u8 *data, *output;
	u32 pck_size, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num)
		latm_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->latm_buffer_size) {
				gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
	}

	prev_pck_size = ctx->latm_buffer_size;

	if (pck && !ctx->resume_from) {
		if (ctx->latm_buffer_size + pck_size > ctx->latm_buffer_alloc) {
			ctx->latm_buffer_alloc = ctx->latm_buffer_size + pck_size;
			ctx->latm_buffer = gf_realloc(ctx->latm_buffer, ctx->latm_buffer_alloc);
		}
		memcpy(ctx->latm_buffer + ctx->latm_buffer_size, data, pck_size);
		ctx->latm_buffer_size += pck_size;
	}

	if (!ctx->bs) ctx->bs = gf_bs_new(ctx->latm_buffer, ctx->latm_buffer_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs, ctx->latm_buffer, ctx->latm_buffer_size);

	if (ctx->resume_from) {
		gf_bs_seek(ctx->bs, ctx->resume_from-1);
		ctx->resume_from = 0;
	}

	if (cts == GF_FILTER_NO_TS)
		prev_pck_size = 0;


	while (1) {
		u32 pos = (u32) gf_bs_get_position(ctx->bs);
		u8 latm_buffer[4096];
		u32 latm_frame_size = 4096;
		if (!latm_dmx_sync_frame_bs(ctx->bs,&ctx->acfg, &latm_frame_size, latm_buffer, NULL)) break;

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * GF_M4ASampleRates[ctx->sr_idx]);
			if (ctx->cts + ctx->dts_inc >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		latm_dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) {
			ctx->resume_from = pos+1;
			return GF_OK;
		}

		if (!ctx->in_seek) {
			GF_FilterSAPType sap = GF_FILTER_SAP_1;

			dst_pck = gf_filter_pck_new_alloc(ctx->opid, latm_frame_size, &output);
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

			memcpy(output, latm_buffer, latm_frame_size);

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, ctx->dts_inc);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

			/*xHE-AAC, check RAP*/
			if (ctx->acfg.base_object_type==42) {
				if (latm_frame_size && (output[0] & 0x80)) {
					sap = GF_FILTER_SAP_1;
				} else {
					sap = GF_FILTER_SAP_NONE;
				}
			}
			gf_filter_pck_set_sap(dst_pck, sap);

			gf_filter_pck_send(dst_pck);
		}
		latm_dmx_update_cts(ctx);

		if (prev_pck_size) {
			pos = (u32) gf_bs_get_position(ctx->bs);
			if (prev_pck_size<=pos) {
				prev_pck_size=0;
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = pck;
				if (pck)
					gf_filter_pck_ref_props(&ctx->src_pck);
			}
		}
	}

	if (pck) {
		pos = (u32) gf_bs_get_position(ctx->bs);
		assert(ctx->latm_buffer_size >= pos);
		memmove(ctx->latm_buffer, ctx->latm_buffer+pos, ctx->latm_buffer_size - pos);
		ctx->latm_buffer_size -= pos;
		gf_filter_pid_drop_packet(ctx->ipid);
		assert(!ctx->resume_from);
	} else {
		ctx->latm_buffer_size = 0;
		return latm_dmx_process(filter);
	}
	return GF_OK;
}

static void latm_dmx_finalize(GF_Filter *filter)
{
	GF_LATMDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->latm_buffer) gf_free(ctx->latm_buffer);
}

static const char *latm_dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_frames=0;
	GF_M4ADecSpecInfo acfg;
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	while (1) {
		u32 nb_skipped = 0;
		if (!latm_dmx_sync_frame_bs(bs, &acfg, 0, NULL, &nb_skipped)) break;
		if (nb_skipped)
			nb_frames=0;
		nb_frames++;
	}
	gf_bs_del(bs);
	if (nb_frames>=2) {
		*score = GF_FPROBE_SUPPORTED;
		return "audio/aac+latm";
	}
	return NULL;
}

static const GF_FilterCapability LATMDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/aac+latm"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "latm"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED_LATM, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_LATMDmxCtx, _n)
static const GF_FilterArgs LATMDmxArgs[] =
{
	{ OFFS(frame_size), "size of AAC frame in audio samples", GF_PROP_UINT, "1024", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister LATMDmxRegister = {
	.name = "rflatm",
	GF_FS_SET_DESCRIPTION("LATM reframer")
	GF_FS_SET_HELP("This filter parses AAC in LATM files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_LATMDmxCtx),
	.args = LATMDmxArgs,
	.finalize = latm_dmx_finalize,
	SETCAPS(LATMDmxCaps),
	.configure_pid = latm_dmx_configure_pid,
	.process = latm_dmx_process,
	.probe_data = latm_dmx_probe_data,
	.process_event = latm_dmx_process_event
};


const GF_FilterRegister *latm_dmx_register(GF_FilterSession *session)
{
	return &LATMDmxRegister;
}
