/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#include <gpac/internal/compositor_dev.h>

/*
	Notes about the mixer:
	1- spatialization is out of scope for the mixer (eg that's the sound node responsability)
	2- mixing is performed by resampling input source & deinterleaving its channels into dedicated buffer.
	We could directly deinterleave in the main mixer ouput buffer, but this would prevent any future
	gain correction.
*/
typedef struct
{
	GF_AudioInterface *src;

	s32 *ch_buf[GF_AUDIO_MIXER_MAX_CHANNELS];
	/*resampled buffer*/
	u32 buffer_size;

	u32 bytes_per_sec, bit_depth;

	Bool has_prev;
	s32 last_channels[GF_AUDIO_MIXER_MAX_CHANNELS];

	u32 in_bytes_used, out_samples_written, out_samples_to_write;

	Fixed speed;
	Fixed pan[GF_AUDIO_MIXER_MAX_CHANNELS];

	s32 (*get_sample)(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride);
	Bool is_planar;
	Bool muted;
} MixerInput;

struct __audiomix
{
	/*src*/
	GF_List *sources;
	/*output config*/
	u32 sample_rate;
	u32 nb_channels;
	u32 afmt, bit_depth;
	u64 channel_layout;
	GF_Mutex *mx;
	/*if set forces stereo/mono*/
	Bool force_channel_out;
	/*set to true by mixer when detecting an audio config change*/
	Bool must_reconfig;
	Bool isEmpty;
	Bool source_buffering;
	/*set to non null if this outputs directly to the driver, in which case audio formats have to be checked*/
	struct _audio_render *ar;

	Fixed max_speed;

	s32 *output;
	u32 output_size;
};

#define GF_S24_MAX	8388607
#define GF_S24_MIN	-8388608


GF_EXPORT
GF_AudioMixer *gf_mixer_new(struct _audio_render *ar)
{
	GF_AudioMixer *am;
	am = (GF_AudioMixer *) gf_malloc(sizeof(GF_AudioMixer));
	if (!am) return NULL;
	memset(am, 0, sizeof(GF_AudioMixer));
	am->mx = gf_mx_new("AudioMix");
	am->sources = gf_list_new();
	am->isEmpty = GF_TRUE;
	am->ar = ar;
	am->sample_rate = 44100;
	am->afmt = GF_AUDIO_FMT_S16;
	am->bit_depth = gf_audio_fmt_bit_depth(am->afmt);
	am->nb_channels = 2;
	am->output = NULL;
	am->output_size = 0;
	am->max_speed = FIX_MAX;
	return am;
}

Bool gf_mixer_must_reconfig(GF_AudioMixer *am)
{
	return am->must_reconfig;
}

void gf_mixer_set_max_speed(GF_AudioMixer *am, Double max_speed)
{
	am->max_speed = FLT2FIX(max_speed);
}

GF_EXPORT
void gf_mixer_del(GF_AudioMixer *am)
{
	gf_mixer_remove_all(am);
	gf_list_del(am->sources);
	gf_mx_del(am->mx);
	if (am->output) gf_free(am->output);
	gf_free(am);
}

void gf_mixer_remove_all(GF_AudioMixer *am)
{
	u32 j;
	gf_mixer_lock(am, GF_TRUE);
	while (gf_list_count(am->sources)) {
		MixerInput *in = (MixerInput *)gf_list_get(am->sources, 0);
		gf_list_rem(am->sources, 0);
		for (j=0; j<GF_AUDIO_MIXER_MAX_CHANNELS; j++) {
			if (in->ch_buf[j]) gf_free(in->ch_buf[j]);
		}
		gf_free(in);
	}
	am->isEmpty = GF_TRUE;
	gf_mixer_lock(am, GF_FALSE);
}

Bool gf_mixer_is_src_present(GF_AudioMixer *am, GF_AudioInterface *ifce)
{
	MixerInput *in;
	u32 i = 0;
	while ((in = (MixerInput *)gf_list_enum(am->sources, &i))) {
		if (in->src == ifce) return GF_TRUE;
	}
	return GF_FALSE;
}
u32 gf_mixer_get_src_count(GF_AudioMixer *am)
{
	return gf_list_count(am->sources);
}

void gf_mixer_force_chanel_out(GF_AudioMixer *am, u32 num_channels)
{
	am->force_channel_out = GF_TRUE;
	am->nb_channels = num_channels;
}

u32 gf_mixer_get_block_align(GF_AudioMixer *am)
{
	return am->nb_channels * am->bit_depth/8;
}

GF_EXPORT
void gf_mixer_lock(GF_AudioMixer *am, Bool lockIt)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[AudioMixer] Thread ID %d is %s the audio mixer\n", gf_th_id(), lockIt ? "locking" : "unlocking" ));
	if (lockIt) {
		gf_mx_p(am->mx);
	} else {
		gf_mx_v(am->mx);
	}
}

GF_EXPORT
Bool gf_mixer_empty(GF_AudioMixer *am)
{
	return am->isEmpty;
}

GF_EXPORT
Bool gf_mixer_buffering(GF_AudioMixer *am)
{
	return am->source_buffering;
}

GF_EXPORT
void gf_mixer_add_input(GF_AudioMixer *am, GF_AudioInterface *src)
{
	MixerInput *in;
	if (gf_mixer_is_src_present(am, src)) return;
	gf_mixer_lock(am, GF_TRUE);
	GF_SAFEALLOC(in, MixerInput);
	if (!in) {
		gf_mixer_lock(am, GF_FALSE);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("[AudioMixer] Cannot allocate input source\n"));
		return;
	}
	in->src = src;
	gf_list_add(am->sources, in);
	am->must_reconfig = GF_TRUE;
	am->isEmpty = GF_FALSE;
	gf_mixer_lock(am, GF_FALSE);
}

void gf_mixer_remove_input(GF_AudioMixer *am, GF_AudioInterface *src)
{
	u32 i, j, count;
	if (am->isEmpty) return;
	gf_mixer_lock(am, GF_TRUE);
	count = gf_list_count(am->sources);
	for (i=0; i<count; i++) {
		MixerInput *in = (MixerInput *)gf_list_get(am->sources, i);
		if (in->src != src) continue;
		gf_list_rem(am->sources, i);
		for (j=0; j<GF_AUDIO_MIXER_MAX_CHANNELS; j++) {
			if (in->ch_buf[j]) gf_free(in->ch_buf[j]);
		}
		gf_free(in);
		break;
	}
	am->isEmpty = gf_list_count(am->sources) ? GF_FALSE : GF_TRUE;
	/*we don't ask for reconfig when removing a node*/
	gf_mixer_lock(am, GF_FALSE);
}


static GF_Err get_best_samplerate(GF_AudioMixer *am, u32 *out_sr, u32 *out_ch, u32 *out_fmt)
{
	if (!am->ar) return GF_OK;
#ifdef ENABLE_AOUT
	if (!am->ar->audio_out || !am->ar->audio_out->QueryOutputSampleRate) return GF_OK;
	return am->ar->audio_out->QueryOutputSampleRate(am->ar->audio_out, out_sr, out_ch, out_fmt);
#endif
	return GF_OK;
}

GF_EXPORT
void gf_mixer_get_config(GF_AudioMixer *am, u32 *outSR, u32 *outCH, u32 *outFMT, u64 *outChCfg)
{
	(*outFMT) = am->afmt;
	(*outCH) = am->nb_channels;
	(*outSR) = am->sample_rate;
	(*outChCfg) = am->channel_layout;
}

GF_EXPORT
void gf_mixer_set_config(GF_AudioMixer *am, u32 outSR, u32 outCH, u32 outFMT, u64 outChCfg)
{
	if ((am->afmt == outFMT) && (am->nb_channels == outCH)
	        && (am->sample_rate==outSR) && (am->channel_layout == outChCfg)) return;

	gf_mixer_lock(am, GF_TRUE);
	am->afmt = outFMT;
	am->bit_depth = gf_audio_fmt_bit_depth(am->afmt);
	if (!am->force_channel_out) am->nb_channels = outCH;
	if (get_best_samplerate(am, &outSR, &outCH, &outFMT) == GF_OK) {
		am->sample_rate = outSR;
		if (outCH>2) am->channel_layout = outChCfg;
		else if (outCH==2) am->channel_layout = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		else am->channel_layout = GF_AUDIO_CH_FRONT_LEFT;
	}
	/*if main mixer recfg output*/
	if (am->ar)	am->ar->need_reconfig = GF_TRUE;
	am->must_reconfig = GF_FALSE;
	gf_mixer_lock(am, GF_FALSE);
}

static GFINLINE s32 make_s24_int(u8 *ptr)
{
	s32 val;
	s16 res;
	memcpy(&res, ptr+1, 2);
	val = res * 256;
	val |= ptr[0];
	return val;
}

#define MIX_S16_SCALE	65535
#define MIX_S24_SCALE	255
#define MIX_U8_SCALE	16777215

s32 input_sample_s32(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	s32 *src = (s32 *)data;
	return src[sample_offset*nb_ch + channel];
}
s32 input_sample_s32p(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	s32 *src = (s32 *)data;
	return src[sample_offset + planar_stride*channel/4];
}
s32 input_sample_s24(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	return make_s24_int(&data[sample_offset*nb_ch*3 + 3*channel]) * MIX_S24_SCALE;
}
s32 input_sample_s24p(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	return make_s24_int(&data[sample_offset*3 + channel*planar_stride]) * MIX_S24_SCALE;
}

#define TRUNC_FLT_DBL(_a) \
	if (_a<-1.0) return GF_INT_MIN;\
	else if (_a>1.0) return GF_INT_MAX;\
	return (s32) (_a * GF_INT_MAX);\

s32 input_sample_flt(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	Float *src = (Float *)data;
	Float samp = src[sample_offset*nb_ch + channel];
	TRUNC_FLT_DBL(samp);
}
s32 input_sample_fltp(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	Float *src = (Float *)data;
	Float samp = src[sample_offset + planar_stride*channel/4];
	TRUNC_FLT_DBL(samp);
}
s32 input_sample_dbl(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	Double *src = (Double *)data;
	Double samp = src[sample_offset*nb_ch + channel];
	TRUNC_FLT_DBL(samp);
}
s32 input_sample_dblp(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	Double *src = (Double *)data;
	Double samp = src[sample_offset + planar_stride * channel / 8];
	TRUNC_FLT_DBL(samp);
}
s32 input_sample_s16(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	s16 *src = (s16 *)data;
	s32 res = src[sample_offset*nb_ch + channel];
	return res * MIX_S16_SCALE;
}
s32 input_sample_s16p(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	s16 *src = (s16 *)data;
	s32 res = src[sample_offset + planar_stride*channel / 2];
	return res * MIX_S16_SCALE;
}
s32 input_sample_u8(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	s32 res = data[sample_offset*nb_ch + channel];
	res -= 128;
	return res * MIX_U8_SCALE;
}
s32 input_sample_u8p(u8 *data, u32 nb_ch, u32 sample_offset, u32 channel, u32 planar_stride)
{
	s32 res = data[sample_offset + planar_stride*channel];
	res -= 128;
	return res * MIX_U8_SCALE;
}

static void gf_am_configure_source(MixerInput *in)
{
	in->bit_depth = gf_audio_fmt_bit_depth(in->src->afmt);
	in->bytes_per_sec = in->src->samplerate * in->src->chan * in->bit_depth / 8;
	in->is_planar = gf_audio_fmt_is_planar(in->src->afmt);
	switch (in->src->afmt) {
	case GF_AUDIO_FMT_S32:
		in->get_sample = input_sample_s32;
		break;
	case GF_AUDIO_FMT_S32P:
		in->get_sample = input_sample_s32p;
		break;
	case GF_AUDIO_FMT_S24:
		in->get_sample = input_sample_s24;
		break;
	case GF_AUDIO_FMT_S24P:
		in->get_sample = input_sample_s24p;
		break;
	case GF_AUDIO_FMT_FLT:
		in->get_sample = input_sample_flt;
		break;
	case GF_AUDIO_FMT_FLTP:
		in->get_sample = input_sample_fltp;
		break;
	case GF_AUDIO_FMT_DBL:
		in->get_sample = input_sample_dbl;
		break;
	case GF_AUDIO_FMT_DBLP:
		in->get_sample = input_sample_dblp;
		break;
	case GF_AUDIO_FMT_S16:
		in->get_sample = input_sample_s16;
		break;
	case GF_AUDIO_FMT_S16P:
		in->get_sample = input_sample_s16p;
		break;
	case GF_AUDIO_FMT_U8:
		in->get_sample = input_sample_u8;
		break;
	case GF_AUDIO_FMT_U8P:
		in->get_sample = input_sample_u8p;
		break;
	}
}

GF_EXPORT
Bool gf_mixer_reconfig(GF_AudioMixer *am)
{
	u32 i, count, numInit, max_sample_rate, max_channels, max_afmt, cfg_changed;
	u64 ch_layout;
	gf_mixer_lock(am, GF_TRUE);
	if (am->isEmpty || !am->must_reconfig) {
		gf_mixer_lock(am, GF_FALSE);
		return GF_FALSE;
	}

	if (am->ar && am->ar->config_forced) {
		am->must_reconfig = GF_FALSE;
		gf_mixer_lock(am, GF_FALSE);
		return GF_FALSE;
	}

	numInit = 0;
	max_channels = am->nb_channels;
	max_afmt = am->afmt;
	cfg_changed = 0;
//	max_sample_rate = am->sample_rate;
	max_sample_rate = 0;

	ch_layout = 0;

	count = gf_list_count(am->sources);
	assert(count);
	for (i=0; i<count; i++) {
		Bool has_cfg;
		MixerInput *in = (MixerInput *) gf_list_get(am->sources, i);
		has_cfg = in->src->GetConfig(in->src, GF_TRUE);
		if (has_cfg) {
			/*check same cfg...*/
			if (in->src->samplerate * in->src->chan * in->bit_depth == 8*in->bytes_per_sec) {
				numInit++;
				continue;
			}
		} else continue;
		/*update out cfg*/
		if ((count==1) && (max_sample_rate != in->src->samplerate)) {
//			cfg_changed = 1;
			max_sample_rate = in->src->samplerate;
		} else if (max_sample_rate<in->src->samplerate) {
//			cfg_changed = 1;
			max_sample_rate = in->src->samplerate;
		}
		if ((count==1) && (max_afmt != in->src->afmt)) {
			cfg_changed = 1;
			max_afmt = in->src->afmt;
		} else if (max_afmt<in->src->afmt) {
			cfg_changed = 1;
			max_afmt = in->src->afmt;
		}
		if (!am->force_channel_out) {
			if ((count==1) && (max_channels!=in->src->chan)) {
				cfg_changed = 1;
				max_channels = in->src->chan;
//				if (in->src->forced_layout)
					ch_layout |= in->src->ch_layout;
			}
			else {
				u32 nb_ch = in->src->chan;
				if (in->src->forced_layout) {
					u64 cfg = in->src->ch_layout;
					nb_ch = 0;
					while (cfg) {
						nb_ch++;
						cfg >>= 1;
					}
					ch_layout |= in->src->ch_layout;
				}
				if (max_channels < nb_ch) {
					cfg_changed = 1;
					max_channels = nb_ch;
					if (nb_ch > 2) ch_layout |= in->src->ch_layout;
				}
			}
		}

		numInit++;
		gf_am_configure_source(in);

		/*cfg has changed, we must reconfig everything*/
		if (cfg_changed || (max_sample_rate != am->sample_rate) ) {
			in->has_prev = GF_FALSE;
			memset(&in->last_channels, 0, sizeof(s32)*GF_AUDIO_MIXER_MAX_CHANNELS);
		}
	}

	if (cfg_changed || (max_sample_rate && (max_sample_rate != am->sample_rate)) ) {
		if (max_channels>2) {
			if (!ch_layout) {
				//TODO pickup default layout ?
			} else if (ch_layout != am->channel_layout) {
				/*recompute num channel based on all input channels*/
				max_channels = 0;
				if (ch_layout & GF_AUDIO_CH_FRONT_LEFT) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_FRONT_RIGHT) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_FRONT_CENTER) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_LFE) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_SURROUND_LEFT) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_SURROUND_RIGHT) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_REAR_CENTER) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_REAR_SURROUND_LEFT) max_channels ++;
				if (ch_layout & GF_AUDIO_CH_REAR_SURROUND_RIGHT) max_channels ++;
			}
		} else {
			ch_layout = GF_AUDIO_CH_FRONT_LEFT;
			if (max_channels==2) ch_layout |= GF_AUDIO_CH_FRONT_RIGHT;
		}
		gf_mixer_set_config(am, max_sample_rate, max_channels, max_afmt, ch_layout);
	}

	if (numInit == count) am->must_reconfig = GF_FALSE;
	if (am->ar) cfg_changed = 1;

	gf_mixer_lock(am, GF_FALSE);
	return cfg_changed;
}

static GFINLINE u32 get_channel_out_pos(u32 in_ch, u64 out_ch_layout)
{
	u32 i, cfg, pos;
	pos = 0;
	for (i=0; i<9; i++) {
		cfg = 1<<(i);
		if (out_ch_layout & cfg) {
			if (cfg == in_ch) return pos;
			pos++;
		}
	}
	return GF_AUDIO_MIXER_MAX_CHANNELS;
}

/*this is crude, we'd need a matrix or something*/
static GFINLINE void gf_mixer_map_channels(s32 *inChan, u32 nb_in, u64 in_ch_layout, Bool forced_layout, u32 nb_out, u64 out_ch_layout)
{
	u32 i;
	if (nb_in==1) {
		/*mono to stereo*/
		if (nb_out==2) {
			//layout forced, don't copy
			if (in_ch_layout && forced_layout) {
				u32 idx = 0;
				while (1) {
					in_ch_layout >>= 1;
					if (!in_ch_layout) break;
					idx++;
				}
				if (idx) {
//					inChan[idx] = inChan[0];
//					inChan[0] = 0;
					inChan[1] = inChan[0];
				} else {
					inChan[1] = inChan[0];
				}
			} else {
				inChan[1] = inChan[0];
			}
		}
		else if (nb_out>2) {
			/*if center channel use it (we assume we always have stereo channels)*/
			if (out_ch_layout & GF_AUDIO_CH_FRONT_CENTER) {
				inChan[2] = inChan[0];
				inChan[0] = 0;
				for (i=3; i<nb_out; i++) inChan[i] = 0;
			} else {
				/*mono to stereo*/
				inChan[1] = inChan[0];
				for (i=2; i<nb_out; i++) inChan[i] = 0;
			}
		}
	} else if (nb_in==2) {
		if (nb_out==1) {
			inChan[0] = (inChan[0]+inChan[1])/2;
		} else {
			for (i=2; i<nb_out; i++) inChan[i] = 0;
		}
	}
	/*same output than input channels, nothing to reorder*/

	/*more output than input channels*/
	else if (nb_in<nb_out) {
		s32 bckup[GF_AUDIO_MIXER_MAX_CHANNELS];
		u32 pos;
		u64 cfg = in_ch_layout;
		u32 ch = 0;
		memcpy(bckup, inChan, sizeof(s32)*nb_in);
		for (i=0; i<nb_in; i++) {
			/*get first in channel*/
			while (! (cfg & 1)) {
				ch++;
				cfg>>=1;
				/*done*/
				if (ch==10) return;
			}
			pos = get_channel_out_pos((1<<ch), out_ch_layout);
			assert(pos != GF_AUDIO_MIXER_MAX_CHANNELS);
			inChan[pos] = bckup[i];
			ch++;
			cfg>>=1;
		}
		for (i=nb_in; i<nb_out; i++) inChan[i] = 0;
	}
	/*less output than input channels (eg sound card doesn't support requested format*/
	else if (nb_in>nb_out) {
		s32 bckup[GF_AUDIO_MIXER_MAX_CHANNELS];
		u32 pos;
		u64 cfg = in_ch_layout;
		u32 ch = 0;
		memcpy(bckup, inChan, sizeof(s32)*nb_in);
		for (i=0; i<nb_in; i++) {
			/*get first in channel*/
			while (! (cfg & 1)) {
				ch++;
				cfg>>=1;
				/*done*/
				if (ch==10) return;
			}
			pos = get_channel_out_pos( (1<<ch), out_ch_layout);
			/*this channel is present in output, copy over*/
			if (pos < GF_AUDIO_MIXER_MAX_CHANNELS) {
				inChan[pos] = bckup[i];
			} else {
				/*map to stereo (we assume that the driver cannot handle ANY multichannel cfg)*/
				switch (1<<ch) {
				case GF_AUDIO_CH_FRONT_CENTER:
				case GF_AUDIO_CH_LFE:
				case GF_AUDIO_CH_REAR_CENTER:
					inChan[0] += bckup[i]/2;
					inChan[1] += bckup[i]/2;
					break;
				case GF_AUDIO_CH_SURROUND_LEFT:
				case GF_AUDIO_CH_REAR_SURROUND_LEFT:
					inChan[0] += bckup[i];
					break;
				case GF_AUDIO_CH_SURROUND_RIGHT:
				case GF_AUDIO_CH_REAR_SURROUND_RIGHT:
					inChan[1] += bckup[i];
					break;
				}
			}
			ch++;
			cfg>>=1;
		}
	}
}

static void gf_mixer_fetch_input(GF_AudioMixer *am, MixerInput *in, u32 audio_delay)
{
	u32 i, j, in_ch, out_ch, prev, next, src_samp, ratio, src_size;
	Bool use_prev;
	u32 planar_stride=0;
	s8 *in_data;
	s32 frac, inChan[GF_AUDIO_MIXER_MAX_CHANNELS], inChanNext[GF_AUDIO_MIXER_MAX_CHANNELS];

	in_data = (s8 *) in->src->FetchFrame(in->src->callback, &src_size, &planar_stride, audio_delay);
	if (!in_data || !src_size) {
		in->has_prev = GF_FALSE;
		/*done, stop fill*/
		in->out_samples_to_write = 0;
		return;
	}
	
	ratio = (u32) (in->src->samplerate * FIX2INT(255*in->speed) / am->sample_rate);
	src_samp = (u32) (src_size * 8 / in->bit_depth / in->src->chan);
	in_ch = in->src->chan;
	out_ch = am->nb_channels;

	/*just in case, if only 1 sample available in src, copy over and discard frame since we cannot
	interpolate audio*/
	if (src_samp==1) {
		in->has_prev = GF_TRUE;
		for (j=0; j<in_ch; j++) {
			in->last_channels[j] = in->get_sample(in_data, in_ch, 0, j, planar_stride);
		}
		in->in_bytes_used = src_size;
		return;
	}

	/*while space to fill and input data, convert*/
	use_prev = in->has_prev;
	memset(inChan, 0, sizeof(s32)*GF_AUDIO_MIXER_MAX_CHANNELS);
	memset(inChanNext, 0, sizeof(s32)*GF_AUDIO_MIXER_MAX_CHANNELS);
	i = 0;
	next = prev = 0;
	while (1) {
		prev = (u32) (i*ratio) / 255;
		if (prev>=src_samp) break;

		next = prev+1;
		frac = (i*ratio) - 255*prev;
		if (frac && (next==src_samp)) break;
		if (use_prev && prev)
			use_prev = GF_FALSE;

		for (j = 0; j < in_ch; j++) {
			inChan[j] = use_prev ? in->last_channels[j] : in->get_sample(in_data, in_ch, prev, j, planar_stride);
			if (frac) {
				inChanNext[j] = in->get_sample(in_data, in_ch, next, j, planar_stride);
				inChan[j] = (s32) ( ( ((s64) inChanNext[j])*frac + ((s64)inChan[j])*(255-frac)) / 255 );
			}
			//don't apply pan when forced layout is used
			if (!in->src->forced_layout && (in->pan[j]!=FIX_ONE) ) {
				inChan[j] = (s32) ( ((s64) inChan[j]) * FIX2INT(100 * in->pan[j]) / 100);
			}
		}

		if (in->speed <= am->max_speed) {
			//map inChannel to the output channel config
			gf_mixer_map_channels(inChan, in_ch, in->src->ch_layout, in->src->forced_layout, out_ch, am->channel_layout);

			for (j=0; j<out_ch ; j++) {
				*(in->ch_buf[j] + in->out_samples_written) = (s32) inChan[j];
			}
		} else {
			for (j=0; j<out_ch ; j++) {
				*(in->ch_buf[j] + in->out_samples_written) = 0;
			}

		}
		
		in->out_samples_written ++;
		if (in->out_samples_written == in->out_samples_to_write) break;
		i++;
	}

	if (!(ratio%255)) {
		in->has_prev = GF_FALSE;
		if (next==src_samp) {
			in->in_bytes_used = src_size;
		} else {
			in->in_bytes_used = MIN(src_size, prev * in->bit_depth * in->src->chan / 8);
		}
	} else {
		in->has_prev = GF_TRUE;
		if (next==src_samp) {
			for (j=0; j<in_ch; j++) in->last_channels[j] = inChanNext[j];
			in->in_bytes_used = src_size;
		} else {
			in->in_bytes_used = prev * in->bit_depth * in->src->chan / 8;
			if (in->in_bytes_used>src_size) {
				in->in_bytes_used = src_size;
				for (j=0; j<in_ch; j++) in->last_channels[j] = inChanNext[j];
			} else {
				u32 idx;
				idx = (prev>=src_samp) ? (src_samp-1) : prev;
				for (j=0; j<in_ch; j++) {
					in->last_channels[j] = in->get_sample(in_data, in_ch, idx, j, planar_stride);
				}
			}
		}
	}
	/*cf below, make sure we call release*/
	in->in_bytes_used += 1;
}

GF_EXPORT
u32 gf_mixer_get_output(GF_AudioMixer *am, void *buffer, u32 buffer_size, u32 delay)
{
	MixerInput *in, *single_source;
	Fixed pan[GF_AUDIO_MIXER_MAX_CHANNELS];
	Bool is_muted, force_mix;
	u32 i, j, count, size, in_size, nb_samples, nb_written;
	s32 *out_mix, nb_act_src;
	char *data, *ptr;

	am->source_buffering = GF_FALSE;
	//reset buffer whatever the state of the mixer is
	memset(buffer, 0, buffer_size);

	/*the config has changed we don't write to output since settings change*/
	if (gf_mixer_reconfig(am)) return 0;

	gf_mixer_lock(am, GF_TRUE);
	count = gf_list_count(am->sources);
	if (!count) {
		gf_mixer_lock(am, GF_FALSE);
		return 0;
	}

	size=0;
	single_source = NULL;
	if (count!=1) goto do_mix;
	if (am->force_channel_out) goto do_mix;
	single_source = (MixerInput *) gf_list_get(am->sources, 0);
	/*if cfg changed or unknown, reconfigure the mixer if the audio renderer is attached. Otherwise,  the mixer config never changes internally*/
	if (!single_source->src->GetConfig(single_source->src, GF_FALSE)) {
		if (am->ar) {
			am->must_reconfig = GF_TRUE;
			gf_mixer_reconfig(am);
			gf_mixer_lock(am, GF_FALSE);
			return 0;
		}
	}

	single_source->muted = single_source->src->IsMuted(single_source->src->callback);
	/*this happens if input SR cannot be mapped to output audio hardware*/
	if (single_source->src->samplerate != am->sample_rate) goto do_mix;
	/*note we don't check output cfg: if the number of channel is the same then the channel cfg is the
	same*/
	if (single_source->src->chan != am->nb_channels) goto do_mix;
	if (single_source->src->GetSpeed(single_source->src->callback)!=FIX_ONE) goto do_mix;
	if (single_source->src->GetChannelVolume(single_source->src->callback, pan)) goto do_mix;
	if (single_source->src->afmt != am->afmt) goto do_mix;

single_source_mix:

	ptr = (char *)buffer;
	in_size = buffer_size;
	is_muted = single_source->muted;

	while (buffer_size) {
		u32 planar_stride;
		size = 0;
		data = single_source->src->FetchFrame(single_source->src->callback, &size, &planar_stride, delay);
		if (!data || !size) {
			am->source_buffering = single_source->src->is_buffering;
			break;
		}
		/*don't copy more than possible*/
		if (size > buffer_size) size = buffer_size;
		if (!is_muted) {
			memcpy(ptr, data, size);
		}
		buffer_size -= size;
		ptr += size;
		single_source->src->ReleaseFrame(single_source->src->callback, size);
		delay += size * 8000 / am->bit_depth / am->sample_rate / am->nb_channels;
	}

	/*not completely filled*/
	if (!size || !buffer_size) {
		if (buffer_size) {
			if (!data) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioMixer] not enough input data (%d still to fill)\n", buffer_size));
			}
		}
		gf_mixer_lock(am, GF_FALSE);
		return (in_size - buffer_size);
	}

	//otherwise, we do have some data but we had a change in config while writing the sample - fallthrough to full mix mode
	buffer = ptr;


do_mix:
	nb_act_src = 0;
	nb_samples = buffer_size / (am->nb_channels * am->bit_depth / 8);
	/*step 1, cfg*/
	if (am->output_size<buffer_size) {
		am->output = (s32*)gf_realloc(am->output, sizeof(s32) * buffer_size);
		am->output_size = buffer_size;
	}

	single_source = NULL;
	force_mix = GF_FALSE;
	for (i=0; i<count; i++) {
		in = (MixerInput *)gf_list_get(am->sources, i);
		in->muted = in->src->IsMuted(in->src->callback);
		if (in->muted) continue;
		if (!in->bit_depth) gf_am_configure_source(in);

		if (in->buffer_size < nb_samples) {
			for (j=0; j<GF_AUDIO_MIXER_MAX_CHANNELS; j++) {
				in->ch_buf[j] = (s32 *) gf_realloc(in->ch_buf[j], sizeof(s32) * nb_samples);
			}
			in->buffer_size = nb_samples;
		}
		in->speed = in->src->GetSpeed(in->src->callback);
		if (in->speed<0) in->speed *= -1;
		in->out_samples_written = 0;
		in->in_bytes_used = 0;

		/*if cfg unknown or changed (AudioBuffer child...) invalidate cfg settings*/
		if (!in->src->GetConfig(in->src, GF_FALSE)) {
			if (am->ar) {
				if (!am->must_reconfig) {
					am->must_reconfig = GF_TRUE;
					/*if main mixer reconfig asap*/
					gf_mixer_reconfig(am);
				}
				in->muted = GF_TRUE;
				continue;
			}
			//otherwise fallthrough, mixer keeps the same config
			force_mix = GF_TRUE;
		}

		if (in->speed==0) {
			in->out_samples_to_write = 0;
		} else {
			assert(in->src->samplerate);
			in->out_samples_to_write = nb_samples;
			if (in->src->IsMuted(in->src->callback)) {
				memset(in->pan, 0, sizeof(Fixed)*GF_AUDIO_MIXER_MAX_CHANNELS);
			} else {
				if (!force_mix  && !in->src->GetChannelVolume(in->src->callback, in->pan)) {
					/*track first active source with same cfg as mixer*/
					if (!single_source && (in->src->samplerate == am->sample_rate)
					        && (in->src->chan == am->nb_channels)
					        && (in->speed == FIX_ONE)
					        && (in->src->afmt == am->afmt)
					   )
						single_source = in;
				}
			}
			nb_act_src ++;
		}
	}
	if (!nb_act_src) {
		gf_mixer_lock(am, GF_FALSE);
		return 0;
	}

	/*if only one active source in native format, process as single source (direct copy)
	this is needed because mediaControl on an audio object doesn't deactivate it (eg the audio
	object is still present in the mixer). this opt is typically useful for language selection
	content (cf mp4menu)*/
	if ((nb_act_src==1) && single_source) goto single_source_mix;

	/*step 2, fill all buffers*/
	while (1) {
		u32 nb_to_fill = 0;
		/*fill*/
		for (i=0; i<count; i++) {
			in = (MixerInput *)gf_list_get(am->sources, i);
			if (in->muted) {
				in->out_samples_to_write = 0;
				continue;
			}
			if (in->out_samples_to_write > in->out_samples_written) {
				gf_mixer_fetch_input(am, in, delay /*+ 8000 * i / am->bits_per_sample / am->sample_rate / am->nb_channels*/ );
				if (in->out_samples_to_write > in->out_samples_written) nb_to_fill++;
			}
		}
		/*release - this is done in 2 steps in case 2 audio object use the same source...*/
		for (i=0; i<count; i++) {
			in = (MixerInput *)gf_list_get(am->sources, i);
			if (in->muted) continue;
			if (in->in_bytes_used) in->src->ReleaseFrame(in->src->callback, in->in_bytes_used-1);
			in->in_bytes_used = 0;
		}
		if (!nb_to_fill) break;
		//only resync on the first fill
		delay=0;
	}
	/*step 3, mix the final buffer*/
	memset(am->output, 0, sizeof(s32) * buffer_size);

	nb_written = 0;
	for (i=0; i<count; i++) {
		u32 k;
		out_mix = am->output;
		in = (MixerInput *)gf_list_get(am->sources, i);
		if (!in->out_samples_written) continue;
		/*only write what has been filled in the source buffer (may be less than output size)*/
		for (j = 0; j < in->out_samples_written; j++) {
			for (k = 0; k < am->nb_channels; k++) {
				(*out_mix) += *(in->ch_buf[k] + j);
				out_mix += 1;
			}
		}
		if (nb_written < in->out_samples_written) nb_written = in->out_samples_written;
	}

	if (!nb_written) {
		gf_mixer_lock(am, GF_FALSE);
		return 0;
	}

	//TODO big-endian support (output is assumed to be little endian PCM)

	//we do not re-normalize based on the number of input, this is the author's responsability
	out_mix = am->output;
	if (am->afmt == GF_AUDIO_FMT_S32) {
		s32 *out_s32 = (s32 *)buffer;
		for (i = 0; i < nb_written; i++) {
			for (j = 0; j < am->nb_channels; j++) {
				s32 samp = (*out_mix);
				(*out_s32) = samp;
				out_s32 += 1;
				out_mix += 1;
			}
		}
	}
	else if (am->afmt == GF_AUDIO_FMT_S32P) {
		s32 *out_s32 = (s32 *) buffer;
		for (j = 0; j < am->nb_channels; j++) {
			out_mix = am->output + j;
			for (i = 0; i < nb_written; i++) {
				s32 samp = (*out_mix);
				(*out_s32) = samp;
				out_s32 += 1;
				out_mix += am->nb_channels;
			}
		}
	}
	else if (am->afmt == GF_AUDIO_FMT_FLT) {
		Float *out_flt = (Float *)buffer;
		for (i = 0; i < nb_written; i++) {
			for (j = 0; j < am->nb_channels; j++) {
				s32 samp = (*out_mix);
				(*out_flt) = ((Float)samp) / GF_INT_MAX;
				out_flt += 1;
				out_mix += 1;
			}
		}
	}
	else if (am->afmt == GF_AUDIO_FMT_FLTP) {
		Float *out_flt = (Float *)buffer;
		for (j = 0; j < am->nb_channels; j++) {
			out_mix = am->output + j;
			for (i = 0; i < nb_written; i++) {
				s32 samp = (*out_mix);
				(*out_flt) = ((Float)samp) / GF_INT_MAX;
				out_flt += 1;
				out_mix += am->nb_channels;
			}
		}
	}
	else if (am->afmt == GF_AUDIO_FMT_DBL) {
		Double *out_dbl = (Double *)buffer;
		for (i = 0; i < nb_written; i++) {
			for (j = 0; j < am->nb_channels; j++) {
				s32 samp = (*out_mix);
				(*out_dbl) = ((Double)samp) / GF_INT_MAX;
				out_dbl += 1;
				out_mix += 1;
			}
		}
	}
	else if (am->afmt == GF_AUDIO_FMT_DBLP) {
		Double *out_dbl = (Double *)buffer;
		for (j = 0; j < am->nb_channels; j++) {
			out_mix = am->output + j;
			for (i = 0; i < nb_written; i++) {
				s32 samp = (*out_mix);
				(*out_dbl) = ((Double)samp) / GF_INT_MAX;
				out_dbl += 1;
				out_mix += am->nb_channels;
			}
		}
	}
	else if (am->afmt == GF_AUDIO_FMT_S24) {
		s8 *out_s24 = (s8 *)buffer;
		for (i = 0; i<nb_written; i++) {
			for (j = 0; j<am->nb_channels; j++) {
				s32 samp = (*out_mix);
//				char samp_c[4];
				samp /= MIX_S24_SCALE;
				if (samp > GF_S24_MAX) samp = GF_S24_MAX;
				else if (samp < GF_S24_MIN) samp = GF_S24_MIN;
//				memcpy(samp_c, &samp, 3);
				out_s24[2] = (samp>>16) & 0xFF;
				out_s24[1] = (samp>>8) & 0xFF;
				out_s24[0] = samp & 0xFF;
				out_s24 += 3;
				out_mix += 1;
			}
		}
	} else if (am->afmt == GF_AUDIO_FMT_S24P) {
		s8 *out_s24 = (s8 *)buffer;
		for (j = 0; j<am->nb_channels; j++) {
			out_mix = am->output + j;
			for (i = 0; i<nb_written; i++) {
				s32 samp = (*out_mix);
				char s16tmp[2];
				u8 lsb;
				samp /= MIX_S24_SCALE;
				if (samp > GF_S24_MAX) samp = GF_S24_MAX;
				else if (samp < GF_S24_MIN) samp = GF_S24_MIN;
				lsb = (samp) & 0xFF;
				samp >>= 8;
//				*((s16 *) (out_s24 +1) ) = (s16)samp;
				*((s16 *) s16tmp) = (s16)samp;
				memcpy(out_s24 +1, s16tmp, 2);
				out_s24[0] = lsb;
				out_s24 += 3;
				out_mix += am->nb_channels;
			}
		}
	} else if (am->afmt == GF_AUDIO_FMT_S16) {
		s16 *out_s16 = (s16 *)buffer;
		for (i = 0; i<nb_written; i++) {
			for (j = 0; j<am->nb_channels; j++) {
				s32 samp = (*out_mix);
				samp /= MIX_S16_SCALE;
				if (samp > GF_SHORT_MAX) samp = GF_SHORT_MAX;
				else if (samp < GF_SHORT_MIN) samp = GF_SHORT_MIN;
				(*out_s16) = samp;
				out_s16 += 1;
				out_mix += 1;
			}
		}
	} else if (am->afmt == GF_AUDIO_FMT_S16P) {
		s16 *out_s16 = (s16 *)buffer;
		for (j = 0; j<am->nb_channels; j++) {
			out_mix = am->output + j;
			for (i = 0; i<nb_written; i++) {
				s32 samp = (*out_mix);
				samp /= MIX_S16_SCALE;
				if (samp > GF_SHORT_MAX) samp = GF_SHORT_MAX;
				else if (samp < GF_SHORT_MIN) samp = GF_SHORT_MIN;
				(*out_s16) = samp;
				out_s16 += 1;
				out_mix += am->nb_channels;
			}
		}
	} else if (am->afmt == GF_AUDIO_FMT_U8) {
		u8 *out_u8 = (u8 *) buffer;
		for (i=0; i<nb_written; i++) {
			for (j=0; j<am->nb_channels; j++) {
				s32 samp = (*out_mix ) / MIX_U8_SCALE;
				if (samp > 127) samp = 127;
				else if (samp < -128) samp = -128;
				samp += 128;
				(*out_u8) = (u8) samp;
				out_u8 += 1;
				out_mix += 1;
			}
		}
	} else if (am->afmt == GF_AUDIO_FMT_U8P) {
		s8 *out_s8 = (s8 *) buffer;
		for (j=0; j<am->nb_channels; j++) {
			out_mix = am->output + j;
			for (i=0; i<nb_written; i++) {
				s32 samp = (*out_mix ) / MIX_U8_SCALE;
				if (samp > 127) samp = 127;
				else if (samp < -128) samp = -128;
				samp += 128;
				(*out_s8) = samp;
				out_s8 += 1;
				out_mix += am->nb_channels;
			}
		}
	}

	nb_written *= am->nb_channels * am->bit_depth / 8;

	gf_mixer_lock(am, GF_FALSE);
	return nb_written;
}


