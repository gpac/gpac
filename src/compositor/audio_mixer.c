/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

/*max number of channels we support in mixer*/
#define GF_SR_MAX_CHANNELS	24

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

	s32 *ch_buf[GF_SR_MAX_CHANNELS];
	/*resampled buffer*/
	u32 buffer_size;

	u32 bytes_per_sec;

	Bool has_prev;
	s32 last_channels[GF_SR_MAX_CHANNELS];

	u32 in_bytes_used, out_samples_written, out_samples_to_write;

	Fixed speed;
	Fixed pan[6];

	Bool muted;
} MixerInput;

struct __audiomix
{
	/*src*/
	GF_List *sources;
	/*output config*/
	u32 sample_rate;
	u32 nb_channels;
	u32 bits_per_sample;
	u32 channel_cfg;
	GF_Mutex *mx;
	/*if set forces stereo/mono*/
	Bool force_channel_out;
	/*set to true by mixer when detecting an audio config change*/
	Bool must_reconfig;
	Bool isEmpty;
	/*set to non null if this outputs directly to the driver, in which case audio formats have to be checked*/
	struct _audio_render *ar;

	s32 *output;
	u32 output_size;
};

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
	am->bits_per_sample = 16;
	am->nb_channels = 2;
	am->output = NULL;
	am->output_size = 0;
	return am;
}

Bool gf_mixer_must_reconfig(GF_AudioMixer *am)
{
	return am->must_reconfig;
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
		for (j=0; j<GF_SR_MAX_CHANNELS; j++) {
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
	return am->nb_channels*am->bits_per_sample/8;
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
		for (j=0; j<GF_SR_MAX_CHANNELS; j++) {
			if (in->ch_buf[j]) gf_free(in->ch_buf[j]);
		}
		gf_free(in);
		break;
	}
	am->isEmpty = gf_list_count(am->sources) ? GF_FALSE : GF_TRUE;
	/*we don't ask for reconfig when removing a node*/
	gf_mixer_lock(am, GF_FALSE);
}


static GF_Err get_best_samplerate(GF_AudioMixer *am, u32 *out_sr, u32 *out_ch, u32 *out_bps)
{
	if (!am->ar) return GF_OK;
	if (!am->ar->audio_out || !am->ar->audio_out->QueryOutputSampleRate) return GF_OK;
	return am->ar->audio_out->QueryOutputSampleRate(am->ar->audio_out, out_sr, out_ch, out_bps);
}

GF_EXPORT
void gf_mixer_get_config(GF_AudioMixer *am, u32 *outSR, u32 *outCH, u32 *outBPS, u32 *outChCfg)
{
	(*outBPS) = am->bits_per_sample;
	(*outCH) = am->nb_channels;
	(*outSR) = am->sample_rate;
	(*outChCfg) = am->channel_cfg;
}

GF_EXPORT
void gf_mixer_set_config(GF_AudioMixer *am, u32 outSR, u32 outCH, u32 outBPS, u32 outChCfg)
{
	if ((am->bits_per_sample == outBPS) && (am->nb_channels == outCH)
	        && (am->sample_rate==outSR) && (am->channel_cfg == outChCfg)) return;

	gf_mixer_lock(am, GF_TRUE);
	am->bits_per_sample = outBPS;
	if (!am->force_channel_out) am->nb_channels = outCH;
	if (get_best_samplerate(am, &outSR, &outCH, &outBPS) == GF_OK) {
		am->sample_rate = outSR;
		if (outCH>2) am->channel_cfg = outChCfg;
		else if (outCH==2) am->channel_cfg = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		else am->channel_cfg = GF_AUDIO_CH_FRONT_LEFT;
	}
	/*if main mixer recfg output*/
	if (am->ar)	am->ar->need_reconfig = GF_TRUE;
	gf_mixer_lock(am, GF_FALSE);
}

GF_EXPORT
Bool gf_mixer_reconfig(GF_AudioMixer *am)
{
	u32 i, count, numInit, max_sample_rate, max_channels, max_bps, cfg_changed, ch_cfg;
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
	max_bps = am->bits_per_sample;
	cfg_changed = 0;
//	max_sample_rate = am->sample_rate;
	max_sample_rate = 0,

	ch_cfg = 0;

	count = gf_list_count(am->sources);
	assert(count);
	for (i=0; i<count; i++) {
		Bool has_cfg;
		MixerInput *in = (MixerInput *) gf_list_get(am->sources, i);
		has_cfg = in->src->GetConfig(in->src, GF_TRUE);
		if (has_cfg) {
			/*check same cfg...*/
			if (in->src->samplerate * in->src->chan * in->src->bps == 8*in->bytes_per_sec) {
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
		if ((count==1) && (max_bps!=in->src->bps)) {
			cfg_changed = 1;
			max_bps = in->src->bps;
		} else if (max_bps<in->src->bps) {
			cfg_changed = 1;
			max_bps = in->src->bps;
		}
		if (!am->force_channel_out) {
			if ((count==1) && (max_channels!=in->src->chan)) {
				cfg_changed = 1;
				max_channels = in->src->chan;
				if (in->src->forced_layout)
					ch_cfg |= in->src->ch_cfg;
			}
			else {
				u32 nb_ch = in->src->chan;
				if (in->src->forced_layout) {
					u32 cfg = in->src->ch_cfg;
					nb_ch = 0;
					while (cfg) {
						nb_ch++;
						cfg >>= 1;
					}
					ch_cfg |= in->src->ch_cfg;
				}
				if (max_channels < nb_ch) {
					cfg_changed = 1;
					max_channels = nb_ch;
					if (nb_ch > 2) ch_cfg |= in->src->ch_cfg;
				}
			}
		}

		numInit++;
		in->bytes_per_sec = in->src->samplerate * in->src->chan * in->src->bps / 8;
		/*cfg has changed, we must reconfig everything*/
		if (cfg_changed || (max_sample_rate != am->sample_rate) ) {
			in->has_prev = GF_FALSE;
			memset(&in->last_channels, 0, sizeof(s32)*GF_SR_MAX_CHANNELS);
		}
	}

	if (cfg_changed || (max_sample_rate && (max_sample_rate != am->sample_rate)) ) {
		if (max_channels>2) {
			if (ch_cfg != am->channel_cfg) {
				/*recompute num channel based on all input channels*/
				max_channels = 0;
				if (ch_cfg & GF_AUDIO_CH_FRONT_LEFT) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_FRONT_RIGHT) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_FRONT_CENTER) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_LFE) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_BACK_LEFT) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_BACK_RIGHT) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_BACK_CENTER) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_SIDE_LEFT) max_channels ++;
				if (ch_cfg & GF_AUDIO_CH_SIDE_RIGHT) max_channels ++;
			}
		} else {
			ch_cfg = GF_AUDIO_CH_FRONT_LEFT;
			if (max_channels==2) ch_cfg |= GF_AUDIO_CH_FRONT_RIGHT;
		}
		gf_mixer_set_config(am, max_sample_rate, max_channels, max_bps, ch_cfg);
	}

	if (numInit == count) am->must_reconfig = GF_FALSE;
	if (am->ar) cfg_changed = 1;

	gf_mixer_lock(am, GF_FALSE);
	return cfg_changed;
}

static GFINLINE u32 get_channel_out_pos(u32 in_ch, u32 out_cfg)
{
	u32 i, cfg, pos;
	pos = 0;
	for (i=0; i<9; i++) {
		cfg = 1<<(i);
		if (out_cfg & cfg) {
			if (cfg == in_ch) return pos;
			pos++;
		}
	}
	return GF_SR_MAX_CHANNELS;
}

/*this is crude, we'd need a matrix or something*/
static GFINLINE void gf_mixer_map_channels(s32 *inChan, u32 nb_in, u32 in_cfg, Bool forced_layout, u32 nb_out, u32 out_cfg)
{
	u32 i;
	if (nb_in==1) {
		/*mono to stereo*/
		if (nb_out==2) {
			//layout forced, don't copy
			if (in_cfg && forced_layout) {
				u32 idx = 0;
				while (1) {
					in_cfg >>= 1;
					if (!in_cfg) break;
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
			if (out_cfg & GF_AUDIO_CH_FRONT_CENTER) {
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
		s32 bckup[GF_SR_MAX_CHANNELS];
		u32 pos;
		u32 cfg = in_cfg;
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
			pos = get_channel_out_pos((1<<ch), out_cfg);
			assert(pos != GF_SR_MAX_CHANNELS);
			inChan[pos] = bckup[i];
			ch++;
			cfg>>=1;
		}
		for (i=nb_in; i<nb_out; i++) inChan[i] = 0;
	}
	/*less output than input channels (eg sound card doesn't support requested format*/
	else if (nb_in>nb_out) {
		s32 bckup[GF_SR_MAX_CHANNELS];
		u32 pos;
		u32 cfg = in_cfg;
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
			pos = get_channel_out_pos( (1<<ch), out_cfg);
			/*this channel is present in output, copy over*/
			if (pos < GF_SR_MAX_CHANNELS) {
				inChan[pos] = bckup[i];
			} else {
				/*map to stereo (we assume that the driver cannot handle ANY multichannel cfg)*/
				switch (1<<ch) {
				case GF_AUDIO_CH_FRONT_CENTER:
				case GF_AUDIO_CH_LFE:
				case GF_AUDIO_CH_BACK_CENTER:
					inChan[0] += bckup[i]/2;
					inChan[1] += bckup[i]/2;
					break;
				case GF_AUDIO_CH_BACK_LEFT:
				case GF_AUDIO_CH_SIDE_LEFT:
					inChan[0] += bckup[i];
					break;
				case GF_AUDIO_CH_BACK_RIGHT:
				case GF_AUDIO_CH_SIDE_RIGHT:
					inChan[1] += bckup[i];
					break;
				}
			}
			ch++;
			cfg>>=1;
		}
	}
}

static GFINLINE s32 make_s24_int(u8 *ptr) 
{
	s16 res = * (s16 *) (ptr+1);
	return (((s32)res) << 8 ) | ptr[0];
}

static void gf_mixer_fetch_input(GF_AudioMixer *am, MixerInput *in, u32 audio_delay)
{
	u32 i, j, in_ch, out_ch, prev, next, src_samp, ratio, src_size;
	Bool use_prev;
	s32 *in_s32 = NULL;
	s16 *in_s16 = NULL;
	s8 *in_s24 = NULL;
	s8 *in_s8 = NULL;
	s32 frac, inChan[GF_SR_MAX_CHANNELS], inChanNext[GF_SR_MAX_CHANNELS];
	
	in_s8 = (s8 *) in->src->FetchFrame(in->src->callback, &src_size, audio_delay);
	if (!in_s8 || !src_size) {
		in->has_prev = GF_FALSE;
		/*done, stop fill*/
		in->out_samples_to_write = 0;
		return;
	}
	
	ratio = (u32) (in->src->samplerate * FIX2INT(255*in->speed) / am->sample_rate);
	src_samp = (u32) (src_size * 8 / in->src->bps / in->src->chan);
	in_ch = in->src->chan;
	out_ch = am->nb_channels;
	if (in->src->bps == 16) {
		in_s16 = (s16 *)in_s8;
		in_s8 = NULL;
	}
	else if (in->src->bps == 24) {
		in_s24 = (s8 *)in_s8;
		in_s8 = NULL;
	} else if (in->src->bps == 32) {
		in_s32 = (s32 *)in_s8;
		in_s8 = NULL;
	}

	/*just in case, if only 1 sample available in src, copy over and discard frame since we cannot
	interpolate audio*/
	if (src_samp==1) {
		in->has_prev = GF_TRUE;
		for (j=0; j<in_ch; j++) in->last_channels[j] = in_s32 ? in_s32[j] : (in_s24 ? make_s24_int(&in_s24[3*j]) : (in_s16 ? in_s16[j] : in_s8[j] ) );
		in->in_bytes_used = src_size;
		return;
	}

	/*while space to fill and input data, convert*/
	use_prev = in->has_prev;
	memset(inChan, 0, sizeof(s32)*GF_SR_MAX_CHANNELS);
	memset(inChanNext, 0, sizeof(s32)*GF_SR_MAX_CHANNELS);
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

		if (in_s32) {
			for (j = 0; j < in_ch; j++) {
				inChan[j] = use_prev ? in->last_channels[j] : in_s32[in_ch*prev + j];
				inChanNext[j] = in_s32[in_ch*next + j];
				inChan[j] = (frac*inChanNext[j] + (255 - frac)*inChan[j]) / 255;
			}
		} else if (in_s24) {
			for (j = 0; j<in_ch; j++) {
				inChan[j] = use_prev ? in->last_channels[j] : make_s24_int(&in_s24[3*(in_ch*prev + j)]);
				inChanNext[j] = make_s24_int(&in_s24[3*(in_ch*next + j)]);
				inChan[j] = (frac*inChanNext[j] + (255 - frac)*inChan[j]) / 255;
			}
		} else if (in_s16) {
			for (j = 0; j<in_ch; j++) {
				inChan[j] = use_prev ? in->last_channels[j] : in_s16[in_ch*prev + j];
				inChanNext[j] = in_s16[in_ch*next + j];
				inChan[j] = (frac*inChanNext[j] + (255 - frac)*inChan[j]) / 255;
			}
		} else {
			for (j=0; j<in_ch; j++) {
				inChan[j] = use_prev ? in->last_channels[j] : in_s8[in_ch*prev + j];
				inChanNext[j] = in_s8[in_ch*next + j];
				inChan[j] = (frac*inChanNext[j] + (255-frac)*inChan[j]) / 255;
			}
		}
		//map inChannel to the output channel config
		gf_mixer_map_channels(inChan, in_ch, in->src->ch_cfg, in->src->forced_layout, out_ch, am->channel_cfg);
		//don't apply pan when forced layout is used
		if (!in->src->forced_layout) {
			for (j=0; j<out_ch ; j++) {
				*(in->ch_buf[j] + in->out_samples_written) = (s32) (inChan[j] * FIX2INT(100*in->pan[j]) / 100 );
			}
		} else {
			for (j=0; j<out_ch ; j++) {
				*(in->ch_buf[j] + in->out_samples_written) = (s32) inChan[j];
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
			in->in_bytes_used = MIN(src_size, prev*in->src->bps * in->src->chan / 8);
		}
	} else {
		in->has_prev = GF_TRUE;
		if (next==src_samp) {
			for (j=0; j<in_ch; j++) in->last_channels[j] = inChanNext[j];
			in->in_bytes_used = src_size;
		} else {
			in->in_bytes_used = prev*in->src->bps * in->src->chan / 8;
			if (in->in_bytes_used>src_size) {
				in->in_bytes_used = src_size;
				for (j=0; j<in_ch; j++) in->last_channels[j] = inChanNext[j];
			} else {
				u32 idx;
				idx = (prev>=src_samp) ? in_ch*(src_samp-1) : in_ch*prev;
				for (j=0; j<in_ch; j++) {
					assert(idx + j < src_size/2);
					in->last_channels[j] = in_s32 ? in_s32[idx + j] : (in_s24 ? make_s24_int(&in_s24[3*(idx + j)]) : (in_s16 ? in_s16[idx + j] : in_s8[idx + j]) );
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
	Fixed pan[6];
	Bool is_muted, force_mix;
	u32 i, j, count, size, in_size, nb_samples, nb_written;
	s32 *out_mix, nb_act_src;
	char *data, *ptr;

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

single_source_mix:

	ptr = (char *)buffer;
	in_size = buffer_size;
	is_muted = single_source->muted;

	while (buffer_size) {
		size = 0;
		data = single_source->src->FetchFrame(single_source->src->callback, &size, delay);
		if (!data || !size) break;
		/*don't copy more than possible*/
		if (size > buffer_size) size = buffer_size;
		if (!is_muted) {
			memcpy(ptr, data, size);
		}
		buffer_size -= size;
		ptr += size;
		single_source->src->ReleaseFrame(single_source->src->callback, size);
		delay += size * 8000 / am->bits_per_sample / am->sample_rate / am->nb_channels;
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
	nb_samples = buffer_size / (am->nb_channels * am->bits_per_sample / 8);
	/*step 1, cfg*/
	if (am->output_size<buffer_size) {
		if (am->output) gf_free(am->output);
		am->output = (s32*)gf_malloc(sizeof(s32) * buffer_size);
		am->output_size = buffer_size;
	}

	single_source = NULL;
	force_mix = GF_FALSE;
	for (i=0; i<count; i++) {
		in = (MixerInput *)gf_list_get(am->sources, i);
		in->muted = in->src->IsMuted(in->src->callback);
		if (in->muted) continue;

		if (in->buffer_size < nb_samples) {
			for (j=0; j<GF_SR_MAX_CHANNELS; j++) {
				if (in->ch_buf[j]) gf_free(in->ch_buf[j]);
				in->ch_buf[j] = (s32 *) gf_malloc(sizeof(s32) * nb_samples);
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
			//otherwise fallthrou, mixer keeps the same config
			force_mix = GF_TRUE;
		}

		if (in->speed==0) {
			in->out_samples_to_write = 0;
		} else {
			assert(in->src->samplerate);
			in->out_samples_to_write = nb_samples;
			if (in->src->IsMuted(in->src->callback)) {
				memset(in->pan, 0, sizeof(Fixed)*6);
			} else {
				if (!force_mix  && !in->src->GetChannelVolume(in->src->callback, in->pan)) {
					/*track first active source with same cfg as mixer*/
					if (!single_source && (in->src->samplerate == am->sample_rate)
					        && (in->src->chan == am->nb_channels) && (in->speed == FIX_ONE)
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
	while (nb_act_src) {
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
		if (!in->out_samples_to_write) continue;
		/*only write what has been filled in the source buffer (may be less than output size)*/
		if (am->bits_per_sample > in->src->bps) {
			s32 mul = am->bits_per_sample - in->src->bps;
			for (j = 0; j < in->out_samples_written; j++) {
				for (k = 0; k < am->nb_channels; k++) {
					s32 res = *(in->ch_buf[k] + j);
					res <<= mul;
					(*out_mix) += res;
					out_mix += 1;
				}
			}
		}
		else if (am->bits_per_sample < in->src->bps) {
			s32 div = in->src->bps - am->bits_per_sample;
			for (j = 0; j < in->out_samples_written; j++) {
				for (k = 0; k < am->nb_channels; k++) {
					s32 res = *(in->ch_buf[k] + j);
					res >>= div;
					(*out_mix) += res;
					out_mix += 1;
				}
			}
		} else {
			for (j = 0; j < in->out_samples_written; j++) {
				for (k = 0; k < am->nb_channels; k++) {
					(*out_mix) += *(in->ch_buf[k] + j);
					out_mix += 1;
				}
			}
		}
		if (nb_written < in->out_samples_written) nb_written = in->out_samples_written;
	}

	if (!nb_written) {
		gf_mixer_lock(am, GF_FALSE);
		return 0;
	}

	//TODO big-endian support (output is assumed to be little endian PCM)

	//we do not re-normalize based on the numbner of input, this is the author's responsability
	out_mix = am->output;
	if (am->bits_per_sample == 32) {
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
	else if (am->bits_per_sample == 24) {
#define GF_S24_MAX	8388607
#define GF_S24_MIN	-8388608
		s8 *out_s24 = (s8 *)buffer;
		for (i = 0; i<nb_written; i++) {
			for (j = 0; j<am->nb_channels; j++) {
				s32 samp = (*out_mix);
				u8 lsb;
				if (samp > GF_S24_MAX) samp = GF_S24_MAX;
				else if (samp < GF_S24_MIN) samp = GF_S24_MIN;
				lsb = (samp) & 0xFF;
				samp >>= 8;
				*((s16 *)&out_s24[1]) = (s16)samp;
				out_s24[0] = lsb;
				out_s24 += 3;
				out_mix += 1;
			}
		}
	} else if (am->bits_per_sample == 16) {
		s16 *out_s16 = (s16 *)buffer;
		for (i = 0; i<nb_written; i++) {
			for (j = 0; j<am->nb_channels; j++) {
				s32 samp = (*out_mix);
				if (samp > GF_SHORT_MAX) samp = GF_SHORT_MAX;
				else if (samp < GF_SHORT_MIN) samp = GF_SHORT_MIN;
				(*out_s16) = samp;
				out_s16 += 1;
				out_mix += 1;
			}
		}
	}
	else {
		s8 *out_s8 = (s8 *) buffer;
		for (i=0; i<nb_written; i++) {
			for (j=0; j<am->nb_channels; j++) {
				s32 samp = (*out_mix ) / 255;
				if (samp > 127) samp = 127;
				else if (samp < -128) samp = -128;
				(*out_s8) = samp;
				out_s8 += 1;
				out_mix += 1;
			}
		}
	}

	nb_written *= am->nb_channels*am->bits_per_sample/8;

	gf_mixer_lock(am, GF_FALSE);
	return nb_written;
}


