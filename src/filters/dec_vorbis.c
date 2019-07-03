/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / XIPH Vorbis decoder filter
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

#ifdef GPAC_HAS_VORBIS

#include <vorbis/codec.h>

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "libvorbis_static")
# endif
#endif

typedef struct
{
	GF_FilterPid *ipid, *opid;
	u32 cfg_crc, sample_rate, nb_chan, timescale;
	u64 last_cts;

	vorbis_info vi;
	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_comment vc;
	ogg_packet op;

	Bool has_reconfigured;
} GF_VorbisDecCtx;

static GF_Err vorbisdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	ogg_packet oggpacket;
	GF_BitStream *bs;
	GF_VorbisDecCtx *ctx = gf_filter_get_udta(filter);


	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p && p->value.data.ptr && p->value.data.size) {
		u32 ex_crc;
		if (strncmp(&p->value.data.ptr[3], "vorbis", 6)) return GF_NON_COMPLIANT_BITSTREAM;
		ex_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (ctx->cfg_crc == ex_crc) return GF_OK;
		ctx->cfg_crc = ex_crc;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[XVID] Reconfiguring without DSI not yet supported\n"));
		return GF_NOT_SUPPORTED;
	}
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );

	if (ctx->ipid) {
		vorbis_block_clear(&ctx->vb);
		vorbis_dsp_clear(&ctx->vd);
		vorbis_info_clear(&ctx->vi);
		vorbis_comment_clear(&ctx->vc);
	}
	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);

	vorbis_info_init(&ctx->vi);
	vorbis_comment_init(&ctx->vc);

	oggpacket.granulepos = -1;
	oggpacket.b_o_s = 1;
	oggpacket.e_o_s = 0;
	oggpacket.packetno = 0;

	bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_Err e = GF_OK;
		oggpacket.bytes = gf_bs_read_u16(bs);
		oggpacket.packet = gf_malloc(sizeof(char) * oggpacket.bytes);
		gf_bs_read_data(bs, oggpacket.packet, oggpacket.bytes);
		if (vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &oggpacket) < 0 ) {
			e = GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_free(oggpacket.packet);
		if (e) {
			gf_bs_del(bs);
			return e;
		}
	}
	vorbis_synthesis_init(&ctx->vd, &ctx->vi);
	vorbis_block_init(&ctx->vd, &ctx->vb);
	gf_bs_del(bs);

	return GF_OK;
}

static GFINLINE void vorbis_to_intern(u32 samples, Float **pcm, char *buf, u32 channels)
{
	u32 i, j;
	s32 val;
	ogg_int16_t *ptr, *data = (ogg_int16_t*)buf ;
	Float *mono;

	for (i=0 ; i<channels ; i++) {
		ptr = &data[i];
		if (!ptr) break;
		
		if (channels>2) {
			/*center is third in gpac*/
			if (i==1) ptr = &data[2];
			/*right is 2nd in gpac*/
			else if (i==2) ptr = &data[1];
			/*LFE is 4th in gpac*/
			if ((channels==6) && (i>3)) {
				if (i==6) ptr = &data[4];	/*LFE*/
				else ptr = &data[i+1];	/*back l/r*/
			}
		}

		mono = pcm[i];
		for (j=0; j<samples; j++) {
			val = (s32) (mono[j] * 32767.f);
			if (val > 32767) val = 32767;
			if (val < -32768) val = -32768;
			(*ptr) = val;
			ptr += channels;
		}
	}
}

static GF_Err vorbisdec_process(GF_Filter *filter)
{
	ogg_packet op;
	u8 *buffer=NULL;
	Float **pcm;
	u32 samples, total_samples, total_bytes, size;
	GF_VorbisDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck=NULL;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (!gf_filter_pid_is_eos(ctx->ipid)) {
			return GF_OK;
		}
	}
	op.granulepos = -1;
	op.b_o_s = 0;
	op.e_o_s = 0;
	op.packetno = 0;

	if (pck) {
		u32 psize;
		op.packet = (u8 *) gf_filter_pck_get_data(pck, &psize);
		op.bytes = psize;
	} else {
		op.packet = NULL;
		op.bytes = 0;
	}

	if (vorbis_synthesis(&ctx->vb, &op) == 0)
		vorbis_synthesis_blockin(&ctx->vd, &ctx->vb) ;

	if ( (ctx->vi.channels != ctx->nb_chan) || (ctx->vi.rate != ctx->sample_rate) ) {
		u32 chan_mask = 0;
		ctx->nb_chan = ctx->vi.channels;
		ctx->sample_rate = ctx->vi.rate;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sample_rate) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->nb_chan) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );

		switch (ctx->vi.channels) {
		case 1:
			chan_mask = GF_AUDIO_CH_FRONT_CENTER;
			break;
		case 2:
			chan_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
			break;
		case 3:
			chan_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER;
			break;
		case 4:
			chan_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT;
			break;
		case 5:
			chan_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT;
			break;
		case 6:
			chan_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT | GF_AUDIO_CH_LFE;
			break;
		}
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(chan_mask) );

	}
	size = (ctx->vd.pcm_current - ctx->vd.pcm_returned) * 2 * ctx->vi.channels;
	if (size) dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &buffer);

	if (pck && dst_pck) gf_filter_pck_merge_properties(pck, dst_pck);

	/*trust vorbis max block info*/
	total_samples = 0;
	total_bytes = 0;
	while ((samples = vorbis_synthesis_pcmout(&ctx->vd, &pcm)) > 0) {
		vorbis_to_intern(samples, pcm, (char*) buffer + total_bytes, ctx->vi.channels);
		total_bytes += samples * 2 * ctx->vi.channels;
		total_samples += samples;
		vorbis_synthesis_read(&ctx->vd, samples);
	}
	if (!size) {
		if (pck) return GF_OK;
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}

	if (pck) {
		ctx->last_cts = gf_filter_pck_get_cts(pck);
		ctx->timescale = gf_filter_pck_get_timescale(pck);
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	gf_filter_pck_set_cts(dst_pck, ctx->last_cts);

	if (ctx->timescale != ctx->sample_rate) {
		u64 dur = total_samples * ctx->timescale;
		dur /= ctx->sample_rate;
		gf_filter_pck_set_duration(dst_pck, (u32) dur);
		ctx->last_cts += dur;
	} else {
		gf_filter_pck_set_duration(dst_pck, total_samples);
		ctx->last_cts += total_samples;
	}

	assert(size == total_bytes);
	gf_filter_pck_send(dst_pck);
	return GF_OK;
}

static void vorbisdec_finalize(GF_Filter *filter)
{
	GF_VorbisDecCtx *ctx = gf_filter_get_udta(filter);

	vorbis_block_clear(&ctx->vb);
	vorbis_dsp_clear(&ctx->vd);
	vorbis_info_clear(&ctx->vi);
	vorbis_comment_clear(&ctx->vc);
}

static const GF_FilterCapability VorbisDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VORBIS),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister VorbisDecRegister = {
	.name = "vorbisdec",
	GF_FS_SET_DESCRIPTION("Vorbis decoder")
	GF_FS_SET_HELP("This filter decodes Vorbis streams through libvorbis library.")
	.private_size = sizeof(GF_VorbisDecCtx),
	.priority = 1,
	SETCAPS(VorbisDecCaps),
	.finalize = vorbisdec_finalize,
	.configure_pid = vorbisdec_configure_pid,
	.process = vorbisdec_process,
};

#endif

const GF_FilterRegister *vorbisdec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_VORBIS
	return &VorbisDecRegister;
#else
	return NULL;
#endif
}


