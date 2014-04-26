/*
 *			GPAC - Multimedia Framework C SDK
 *
 *				Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2012
 *					All rights reserved
 *
 *
 *  This file is part of GPAC / sample audio filter module
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


#include <gpac/modules/audio_out.h>
#include <gpac/math.h>

#ifndef PI
#define PI GF_PI
#endif

typedef struct
{
	u32 type;
	Bool inplace;
	u32 sample_block_size;

	/*distorsion param*/
	Double gain, clip, volume;

	/*delai param*/
	char *delai_buffer, *block_buffer;
	u32 delai_ms, delai_buffer_size, block_size, nb_bytes;
	Double feedback;
} FilterContext;

static GF_Err ProcessDistorsion(GF_AudioFilter *af, void *in_block, u32 in_block_size, void *out_block, u32 *out_block_size)
{
	u32 i, count;
	register Double temp;
	Double gain, clip, volume;
	FilterContext *ctx = af->udta;

	gain=ctx->gain / 100.0;
	clip=ctx->clip * 32768.0 / 100.0;
	volume=ctx->volume / 100.0;
	count = in_block_size>>1;
	for(i=0; i<count; i++) {
		temp = (Double) ((s16 *) in_block)[i];
		temp = temp * gain;

		if (temp > clip) temp = clip;
		else if (temp < -clip) temp = -clip;
		temp = temp*volume;

		if (temp > 32767.0) temp = 32767.0;
		else if (temp < -32768.0) temp = -32768.0;
		((s16 *) out_block)[i] = (s16) temp;
	}
	*out_block_size = in_block_size;
	return GF_OK;
}

static GF_Err ProcessIdentity(GF_AudioFilter *af, void *in_block, u32 in_block_size, void *out_block, u32 *out_block_size)
{
	FilterContext *ctx = af->udta;
	if (!ctx->inplace)
		memcpy(out_block, in_block, in_block_size);

	*out_block_size = in_block_size;
	return GF_OK;
}

static GF_Err ProcessDelai(GF_AudioFilter *af, void *in_block, u32 in_block_size, void *out_block, u32 *out_block_size)
{
	s16 *in, *delay, *out;
	u32 i;
	register Double temp;
	Double ratio, vol;
	FilterContext *ctx = af->udta;
	assert(ctx->block_size==in_block_size);

	/*fill delai buffer*/
	if (ctx->nb_bytes<ctx->delai_buffer_size) {
		memcpy(ctx->delai_buffer + ctx->nb_bytes, in_block, in_block_size);
		ctx->nb_bytes += in_block_size;
		memcpy(out_block, in_block, in_block_size);
		*out_block_size = in_block_size;
		return GF_OK;
	}
	memcpy(ctx->block_buffer, ctx->delai_buffer, ctx->block_size);
	memmove(ctx->delai_buffer, ctx->delai_buffer + ctx->block_size, (ctx->delai_buffer_size-ctx->block_size) );

	vol = ctx->volume / 100;
	ratio = ctx->feedback/100;
	delay = (s16 *) ctx->block_buffer;
	in = (s16 *) in_block;
	out = (s16 *) out_block;
	for (i=0; i<ctx->block_size/2; i++) {
		temp = (1-ratio)*in[i] + ratio*delay[i];
		temp = temp*vol;
		out[i] = (s16) temp;
	}
	memcpy(ctx->delai_buffer + ctx->nb_bytes - in_block_size, out_block, in_block_size);

	*out_block_size = ctx->block_size;
	return GF_OK;
}


static GF_Err Configure(GF_AudioFilter *af, u32 in_sr, u32 in_bps, u32 in_nb_ch, u32 in_ch_cfg, u32 *out_nb_ch, u32 *out_ch_cfg, u32 *out_block_len_in_samples, u32 *delay_ms, Bool *inplace)
{
	FilterContext *ctx = af->udta;

	*inplace = ctx->inplace;
	*delay_ms = 0;
	*out_nb_ch = in_nb_ch;
	*out_ch_cfg = in_ch_cfg;

	switch (ctx->type) {
	case 2:
		ctx->block_size = ctx->sample_block_size * in_nb_ch * in_bps / 8;
		ctx->delai_buffer_size = in_nb_ch * in_bps * ctx->delai_ms * in_sr / 1000 / 8;
		ctx->delai_buffer_size /= ctx->block_size;
		ctx->delai_buffer_size *= ctx->block_size;

		if (ctx->delai_buffer) gf_free(ctx->delai_buffer);
		ctx->delai_buffer = gf_malloc(sizeof(char)*ctx->delai_buffer_size);
		memset(ctx->delai_buffer, 0, sizeof(char)*ctx->delai_buffer_size);

		if (ctx->block_buffer) gf_free(ctx->block_buffer);
		ctx->block_buffer = gf_malloc(sizeof(char)*ctx->block_size);
		memset(ctx->block_buffer, 0, sizeof(char)*ctx->block_size);
		break;
	}

	*out_block_len_in_samples = ctx->sample_block_size;
	return GF_OK;
}

static Bool SetFilter(GF_AudioFilter *af, char *filter)
{
	char *opts;
	FilterContext *ctx = af->udta;
	if (!filter) return 0;

	opts = strchr(filter, '@');
	if (opts) opts[0] = 0;

	ctx->sample_block_size = 0;
	ctx->inplace = 1;
	ctx->volume = 100.0;

	if (!stricmp(filter, "identity")) {
		af->Process = ProcessIdentity;
		ctx->type = 0;
	}
	else if (!stricmp(filter, "distorsion")) {
		ctx->gain=50.0;
		ctx->clip=100.0;
		af->Process = ProcessDistorsion;
		ctx->type = 1;
	}
	else if (!stricmp(filter, "delai")) {
		ctx->delai_ms = 100;
		ctx->feedback = 50;
		af->Process = ProcessDelai;
		ctx->type = 2;
		ctx->sample_block_size = 120;
	} else {
		if (opts) opts[0] = '@';
		return 0;
	}
	if (opts) {
		opts[0] = '@';
		opts++;

		while (1) {
			char *sep = strchr(opts, ';');
			if (sep) sep[0] = 0;

			if (!strnicmp(opts, "blocksize=", 10)) ctx->sample_block_size = atoi(opts+10);
			else if (!stricmp(opts, "noinplace")) ctx->inplace = 0;
			else if (!strnicmp(opts, "gain=", 5)) ctx->gain = atof(opts+5);
			else if (!strnicmp(opts, "clip=", 5)) ctx->clip = atof(opts+5);
			else if (!strnicmp(opts, "volume=", 7)) ctx->volume = atof(opts+7);
			else if (!strnicmp(opts, "delai=", 6)) ctx->delai_ms = atoi(opts+6);
			else if (!strnicmp(opts, "feedback=", 9)) {
				ctx->feedback = atof(opts+9);
				if (ctx->feedback>100) ctx->feedback=100;
			}

			if (!sep) break;
			sep[0] = ';';
			opts = sep+1;
		}
	}

	return 1;
}


static Bool SetOption(GF_AudioFilter *af, char *option, char *value)
{
	return 1;
}
static void Reset(GF_AudioFilter *af)
{
}


void *NewAudioFilter()
{
	FilterContext *ctx;
	GF_AudioFilter *mod;
	GF_SAFEALLOC(ctx, FilterContext);
	if(!ctx) return NULL;

	GF_SAFEALLOC(mod, GF_AudioFilter);
	if(!mod) {
		gf_free(ctx);
		return NULL;
	}
	mod->udta = ctx;
	mod->SetFilter = SetFilter;
	mod->Configure = Configure;
	mod->Process = ProcessIdentity;
	mod->SetOption = SetOption;
	mod->Reset = Reset;

	GF_REGISTER_MODULE_INTERFACE(mod, GF_AUDIO_FILTER_INTERFACE, "Sample Audio Filter", "gpac distribution");
	return mod;
}

void DeleteAudioFilter(void *ifce)
{
	GF_AudioFilter *dr = (GF_AudioFilter*) ifce;
	FilterContext *ctx = (FilterContext *)dr->udta;

	if (ctx->delai_buffer) gf_free(ctx->delai_buffer);
	gf_free(ctx);
	gf_free(dr);
}


/*
 * ********************************************************************
 * interface
 */
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_AUDIO_FILTER_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_AUDIO_FILTER_INTERFACE)
		return NewAudioFilter();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	if (ifce->InterfaceType==GF_AUDIO_FILTER_INTERFACE)
		DeleteAudioFilter((GF_AudioFilter*)ifce);
}

GPAC_MODULE_STATIC_DELARATION( audio_filter )
