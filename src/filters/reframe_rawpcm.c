/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / RAW PCM reframer filter
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
	//opts
	u32 framelen, safmt, sr, ch;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	Bool file_loaded, is_playing, initial_play_done;
	u64 cts;
	u32 frame_size, nb_bytes_in_frame, Bps;
	u64 filepos, total_frames;
	GF_FilterPacket *out_pck;
	u8 *out_data;
	Bool reverse_play, done;

	u32 probe_wave, wav_hdr_size;
	u8 *probe_data;
	u32 probe_data_size;
} GF_PCMReframeCtx;




GF_Err pcmreframe_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_PCMReframeCtx *ctx = gf_filter_get_udta(filter);

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
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_EXT);
	if (!ctx->safmt) {
		if (p && p->value.string) ctx->safmt = gf_audio_fmt_parse(p->value.string);
	}
	if (!ctx->safmt) {
		ctx->probe_wave = 1;
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
		return GF_OK;
	} else {
		if (!ctx->sr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[PCMReframe] Missing audio sample rate, cannot parse\n"));
			return GF_BAD_PARAM;
		}
		if (!ctx->ch) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[PCMReframe] Missing audio ch, cannot parse\n"));
			return GF_BAD_PARAM;
		}
		ctx->Bps = gf_audio_fmt_bit_depth(ctx->safmt) / 8;
	}
	if (!ctx->framelen) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[PCMReframe] Missing audio framelen, using 1024\n"));
		ctx->framelen = 1024;
	}

	ctx->frame_size = ctx->framelen * ctx->Bps * ctx->ch;

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_FALSE);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->ch));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT(ctx->framelen));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->safmt));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_REWIND));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, &PROP_BOOL(GF_TRUE));

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;

	if (!gf_sys_is_test_mode() ) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(ctx->sr * ctx->Bps * ctx->ch));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
	if (p && p->value.longuint) {
		u64 nb_frames = p->value.longuint;
		if (ctx->probe_wave) nb_frames -= ctx->wav_hdr_size;
		nb_frames /= ctx->Bps * ctx->ch;
		ctx->total_frames = p->value.longuint;
		ctx->total_frames /= ctx->frame_size;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(nb_frames, ctx->sr));
	}

	return GF_OK;
}

static Bool pcmreframe_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 nb_frames;
	GF_FilterEvent fevt;
	GF_PCMReframeCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
		}
		ctx->done = GF_FALSE;

		if (!ctx->total_frames)
			return GF_TRUE;

		if (evt->play.start_range>=0) {
			ctx->cts = (u64) (evt->play.start_range * ctx->sr);
		} else {
			ctx->cts = (ctx->total_frames-1) * ctx->framelen;
		}
		nb_frames = (u32) (ctx->cts / ctx->framelen);
		if (nb_frames==ctx->total_frames) {
			if (evt->play.speed>=0) {
				ctx->done = GF_TRUE;
				return GF_TRUE;
			}
			nb_frames--;
			ctx->cts = nb_frames * ctx->framelen;
		}

		ctx->filepos = nb_frames * ctx->frame_size;
		ctx->reverse_play =  (evt->play.speed<0) ? GF_TRUE : GF_FALSE;

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->filepos)
				return GF_TRUE;
		}
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->filepos + ctx->wav_hdr_size;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//don't cancel event
		ctx->is_playing = GF_FALSE;
		if (ctx->out_pck) {
			gf_filter_pck_discard(ctx->out_pck);
			ctx->out_pck = NULL;
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

void pcmreframe_flush_packet(GF_PCMReframeCtx *ctx)
{
	if (ctx->reverse_play) {
		u32 i, nb_bytes_in_sample, nb_samples = ctx->nb_bytes_in_frame / ctx->Bps / ctx->ch;
		nb_bytes_in_sample = ctx->Bps * ctx->ch;
		for (i=0; i<nb_samples/2; i++) {
			char store[100];
			memcpy(store, ctx->out_data + i*nb_bytes_in_sample, nb_bytes_in_sample);
			memcpy(ctx->out_data + i*nb_bytes_in_sample, ctx->out_data + (nb_samples - i - 1)*nb_bytes_in_sample, nb_bytes_in_sample);
			memcpy(ctx->out_data + (nb_samples-i-1)*nb_bytes_in_sample, store, nb_bytes_in_sample);
		}
	}
	gf_filter_pck_send(ctx->out_pck);
	ctx->out_pck = NULL;
}
GF_Err pcmreframe_process(GF_Filter *filter)
{
	GF_PCMReframeCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	u64 byte_offset;
	u8 *data;
	u32 pck_size;

	if (ctx->done) return GF_EOS;

	if (!ctx->is_playing && ctx->opid) return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid) && !ctx->reverse_play) {
			if (ctx->out_pck) {
				gf_filter_pck_truncate(ctx->out_pck, ctx->nb_bytes_in_frame);
				gf_filter_pck_set_duration(ctx->out_pck, ctx->nb_bytes_in_frame/ctx->Bps/ctx->ch);
				pcmreframe_flush_packet(ctx);
			}
			if (ctx->opid)
				gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	if (ctx->probe_wave==1) {
		Bool wav_ok = GF_TRUE;
		Bool hdr_found = GF_FALSE;
		GF_BitStream *bs;
		if (ctx->probe_data) {
			ctx->probe_data = gf_realloc(ctx->probe_data, ctx->probe_data_size+pck_size);
			memcpy(ctx->probe_data + ctx->probe_data_size, data, pck_size);
			ctx->probe_data_size += pck_size;
			bs = gf_bs_new(ctx->probe_data, ctx->probe_data_size, GF_BITSTREAM_READ);
		} else {
			bs = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
		}
		u32 type = gf_bs_read_u32(bs);
		if (type!=GF_4CC('R', 'I', 'F', 'F')) {
			wav_ok = GF_FALSE;
		}
		gf_bs_read_u32(bs);
		u32 wtype = gf_bs_read_u32(bs);
		if (wtype!=GF_4CC('W', 'A', 'V', 'E')) {
			wav_ok = GF_FALSE;
		}
		while (gf_bs_available(bs)) {
			type = gf_bs_read_u32(bs);
			u32 csize = gf_bs_read_u32_le(bs); //subchunk size
			if (type==GF_4CC('d', 'a', 't', 'a')) {
				break;
			}

			if (type!=GF_4CC('f', 'm', 't', ' ')) {
				gf_bs_skip_bytes(bs, csize);
				continue;
			}
			//parse fmt
			hdr_found = GF_TRUE;
			u16 atype = gf_bs_read_u16_le(bs);
			ctx->ch = gf_bs_read_u16_le(bs);
			ctx->sr = gf_bs_read_u32_le(bs);
			gf_bs_read_u32_le(bs); //byte rate
			gf_bs_read_u16_le(bs); // block align
			u32 bps = gf_bs_read_u16_le(bs);
			if (atype==3) {
				if (bps==32) {
					ctx->safmt = GF_AUDIO_FMT_FLT;
				} else {
					wav_ok = GF_FALSE;
				}
			} else if (atype==1) {
				if (bps==32) {
					ctx->safmt = GF_AUDIO_FMT_S32;
				} else if (bps==24) {
					ctx->safmt = GF_AUDIO_FMT_S24;
				} else if (bps==16) {
					ctx->safmt = GF_AUDIO_FMT_S16;
				} else if (bps==8) {
					ctx->safmt = GF_AUDIO_FMT_U8;
				} else {
					wav_ok = GF_FALSE;
				}
			}
		}
		if (gf_bs_is_overflow(bs)) {
			if (!ctx->probe_data) {
				ctx->probe_data = gf_malloc(pck_size);
				memcpy(ctx->probe_data, data, pck_size);
				ctx->probe_data_size = pck_size;
			}
			if (!hdr_found) {
				if (ctx->probe_data_size<=10000) {
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_OK;
				}
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[PCMReframe] Cannot find wave data chunk after %d bytes, aborting\n", ctx->probe_data_size));
			}
			wav_ok = GF_FALSE;
		}

		ctx->wav_hdr_size = (u32) gf_bs_get_position(bs);

		gf_bs_del(bs);
		if (!wav_ok) {
			gf_filter_pid_drop_packet(ctx->ipid);
			if (ctx->opid)
				gf_filter_pid_set_eos(ctx->opid);
			gf_filter_pid_set_discard(ctx->ipid, GF_TRUE);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[PCMReframe] Invalid or unsupported WAVE header, aborting\n", ctx->probe_data_size));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		ctx->probe_wave = 2;
		pcmreframe_configure_pid(filter, ctx->ipid, GF_FALSE);
		if (ctx->probe_data) {
			pck_size = ctx->probe_data_size;
			data = ctx->probe_data;
		}
		pck_size -= ctx->wav_hdr_size;
		data+=ctx->wav_hdr_size;
		byte_offset = 0;
	}
	byte_offset+= ctx->wav_hdr_size;

	while (pck_size) {
		if (!ctx->out_pck) {
			ctx->out_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->frame_size, &ctx->out_data);
			if (!ctx->out_pck) return GF_OUT_OF_MEM;

			gf_filter_pck_set_cts(ctx->out_pck, ctx->cts);
			gf_filter_pck_set_sap(ctx->out_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_duration(ctx->out_pck, ctx->framelen);
			gf_filter_pck_set_byte_offset(ctx->out_pck, byte_offset);
		}

		if (pck_size + ctx->nb_bytes_in_frame < ctx->frame_size) {
			memcpy(ctx->out_data + ctx->nb_bytes_in_frame, data, pck_size);
			ctx->nb_bytes_in_frame += pck_size;
			pck_size = 0;
		} else {
			u32 remain = ctx->frame_size - ctx->nb_bytes_in_frame;
			memcpy(ctx->out_data + ctx->nb_bytes_in_frame, data, remain);
			ctx->nb_bytes_in_frame = ctx->frame_size;
			pcmreframe_flush_packet(ctx);

			pck_size -= remain;
			data += remain;
			byte_offset += remain;
			ctx->out_pck = NULL;
			ctx->nb_bytes_in_frame = 0;

			//reverse playback, the remaining data is for the next frame, we want the previous one.
			//Trash packet and seek to previous frame
			if (ctx->reverse_play) {
				GF_FilterEvent fevt;
				if (!ctx->cts) {
					if (ctx->opid)
						gf_filter_pid_set_eos(ctx->opid);
					GF_FEVT_INIT(fevt, GF_FEVT_STOP, ctx->ipid);
					gf_filter_pid_send_event(ctx->ipid, &fevt);
					ctx->done = GF_TRUE;
					return GF_EOS;
				}
				ctx->cts -= ctx->framelen;
				ctx->filepos -= ctx->frame_size;
				gf_filter_pid_drop_packet(ctx->ipid);
				//post a seek, this will trash remaining packets in buffers
				GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
				fevt.seek.start_offset = ctx->filepos + ctx->wav_hdr_size;
				gf_filter_pid_send_event(ctx->ipid, &fevt);
				return GF_OK;
			}
			ctx->cts += ctx->framelen;
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	if (ctx->probe_data) {
		gf_free(ctx->probe_data);
		ctx->probe_data = NULL;
		ctx->probe_data_size = 0;
	}
	return GF_OK;
}

static const char *pcmreframe_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (size<20) return NULL;

	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	u32 code = gf_bs_read_u32(bs);
	if (code == GF_4CC('R', 'I', 'F', 'F')) {
		gf_bs_read_u32(bs);
		code = gf_bs_read_u32(bs);
		if (code == GF_4CC('W', 'A', 'V', 'E')) {
			*score = GF_FPROBE_SUPPORTED;
			gf_bs_del(bs);
			return "audio/wav";
		}
	}
	gf_bs_del(bs);
	*score = GF_FPROBE_EXT_MATCH;
	return gf_audio_fmt_all_shortnames();
}


static void pcmreframe_finalize(GF_Filter *filter)
{
	GF_PCMReframeCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->out_pck) gf_filter_pck_discard(ctx->out_pck);
	if (ctx->probe_data) gf_free(ctx->probe_data);
}

static GF_FilterCapability PCMReframeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "pcm"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/x-pcm"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "wav"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/wav"),
};

#define OFFS(_n)	#_n, offsetof(GF_PCMReframeCtx, _n)
static GF_FilterArgs PCMReframeArgs[] =
{
	{ OFFS(sr), "sample rate", GF_PROP_UINT, "44100", NULL, 0},
	{ OFFS(safmt), "audio format", GF_PROP_PCMFMT, "none", NULL, 0},
	{ OFFS(ch), "number of channels", GF_PROP_UINT, "2", NULL, 0},
	{ OFFS(framelen), "number of samples to put in one audio frame. For planar formats, indicate plane size in samples", GF_PROP_UINT, "1024", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister PCMReframeRegister = {
	.name = "rfpcm",
	GF_FS_SET_DESCRIPTION("PCM reframer")
	GF_FS_SET_HELP("This filter parses raw PCM file/data or WAVE files and outputs corresponding raw audio PID and frames.")
	.private_size = sizeof(GF_PCMReframeCtx),
	.args = PCMReframeArgs,
	SETCAPS(PCMReframeCaps),
	.finalize = pcmreframe_finalize,
	.configure_pid = pcmreframe_configure_pid,
	.process = pcmreframe_process,
	.process_event = pcmreframe_process_event,
	.probe_data = pcmreframe_probe_data
};


const GF_FilterRegister *pcmreframe_register(GF_FilterSession *session)
{
	PCMReframeArgs[1].min_max_enum = gf_audio_fmt_all_names();
	PCMReframeCaps[1].val.value.string = (char *) gf_audio_fmt_all_shortnames();
	return &PCMReframeRegister;
}

