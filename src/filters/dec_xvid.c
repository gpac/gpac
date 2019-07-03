/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 visual p2 xvid decoder filter
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
#include <gpac/avparse.h>
#include <gpac/constants.h>


/*if we don't have M4V (A)SP parser, we con't get width and height and xvid is then unusable ...*/
#if !defined(GPAC_DISABLE_AV_PARSERS) && defined(GPAC_HAS_XVID)

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "libxvidcore")
# endif
#endif


#include <xvid.h>

#ifndef XVID_DEC_FRAME
#define XVID_DEC_FRAME xvid_dec_frame_t
#define XVID_DEC_PARAM xvid_dec_create_t
#else
#define XVID_USE_OLD_API
#endif

//#undef XVID_USE_OLD_API

static Bool xvid_is_init = GF_FALSE;

typedef struct
{
	Bool deblock_y;
	Bool deblock_uv;
#ifndef XVID_USE_OLD_API
	Bool film_effect;
	Bool dering_y;
	Bool dering_uv;
#endif

	GF_FilterPid *ipid, *opid;
	u32 cfg_crc;
	void *codec;

	u32 width, height, out_size;
	GF_Fraction pixel_ar;
	Bool first_frame;
	s32 base_filters;
	Float FPS;
	u32 offset;

	GF_List *src_packets;
	u64 next_cts;

} GF_XVIDCtx;

static GF_Err xviddec_initialize(GF_Filter *filter)
{
	GF_XVIDCtx *ctx = gf_filter_get_udta(filter);
	if (!xvid_is_init) {
#ifdef XVID_USE_OLD_API
		XVID_INIT_PARAM init;
		init.api_version = 0;
		init.core_build = 0;
		/*get info*/
		init.cpu_flags = XVID_CPU_CHKONLY;
		xvid_init(NULL, 0, &init, NULL);
		/*then init*/
		xvid_init(NULL, 0, &init, NULL);
#else
		xvid_gbl_init_t init;
		init.debug = 0;
		init.version = XVID_VERSION;
		init.cpu_flags = 0; /*autodetect*/
		xvid_global(NULL, 0, &init, NULL);
#endif
		xvid_is_init = GF_TRUE;
	}

#ifndef XVID_USE_OLD_API
	if (ctx->film_effect) ctx->base_filters |= XVID_FILMEFFECT;
#endif

#ifdef XVID_USE_OLD_API
	if (ctx->deblock_y) ctx->base_filters |= XVID_DEC_DEBLOCKY;
#else
	if (ctx->deblock_y) ctx->base_filters |= XVID_DEBLOCKY;
#endif

#ifdef XVID_USE_OLD_API
	if (ctx->deblock_uv) ctx->base_filters |= XVID_DEC_DEBLOCKUV;
#else
	if (ctx->deblock_uv) ctx->base_filters |= XVID_DEBLOCKUV;
#endif

#ifndef XVID_USE_OLD_API
	if (ctx->dering_y) ctx->base_filters |= XVID_DERINGY | XVID_DEBLOCKY;
	if (ctx->dering_uv) ctx->base_filters |= XVID_DERINGUV | XVID_DEBLOCKUV;
#endif
	ctx->src_packets = gf_list_new();
	return GF_OK;
}

static GF_Err xviddec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
#ifdef XVID_USE_OLD_API
	XVID_DEC_FRAME frame;
	XVID_DEC_PARAM par;
#else
	xvid_dec_frame_t frame;
	xvid_dec_create_t par;
#endif
	GF_XVIDCtx *ctx = gf_filter_get_udta(filter);

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
		u32 ex_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (ctx->cfg_crc == ex_crc) return GF_OK;

		//shoud we flush ?
		if (ctx->codec) xvid_decore(ctx->codec, XVID_DEC_DESTROY, NULL, NULL);
		ctx->codec = NULL;

		ctx->cfg_crc = ex_crc;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[XVID] Reconfiguring without DSI not yet supported\n"));
		return GF_NOT_SUPPORTED;
	}

	/*decode DSI*/
	e = gf_m4v_get_config(p->value.data.ptr, p->value.data.size, &dsi);
	if (e) return e;
	if (!dsi.width || !dsi.height) return GF_NON_COMPLIANT_BITSTREAM;

	memset(&par, 0, sizeof(par));
	par.width = dsi.width;
	par.height = dsi.height;
	/*note that this may be irrelevant when used through systems (FPS is driven by systems CTS)*/
	ctx->FPS = dsi.clock_rate;
	ctx->FPS /= 1000;
	if (!ctx->FPS) ctx->FPS = 30.0f;
	ctx->pixel_ar.num = dsi.par_num;
	ctx->pixel_ar.den = dsi.par_den;

#ifndef XVID_USE_OLD_API
	par.version = XVID_VERSION;
#endif

	if (xvid_decore(NULL, XVID_DEC_CREATE, &par, NULL) < 0) return GF_NON_COMPLIANT_BITSTREAM;

	ctx->width = par.width;
	ctx->height = par.height;
	ctx->codec = par.handle;

	/*init decoder*/
	memset(&frame, 0, sizeof(frame));
	frame.bitstream = (void *) p->value.data.ptr;
	frame.length = p->value.data.size;
#ifndef XVID_USE_OLD_API
	frame.version = XVID_VERSION;
	xvid_decore(ctx->codec, XVID_DEC_DECODE, &frame, NULL);
#else
	/*don't perform error check, XviD doesn't like DSI only frame ...*/
	xvid_decore(ctx->codec, XVID_DEC_DECODE, &frame, NULL);
#endif

	ctx->first_frame = GF_TRUE;
	ctx->out_size = ctx->width * ctx->height * 3 / 2;

	ctx->ipid = pid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PAR, &PROP_FRAC(ctx->pixel_ar) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_YUV) );

	return GF_OK;
}

static void xviddec_finalize(GF_Filter *filter)
{
	GF_XVIDCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->codec) xvid_decore(ctx->codec, XVID_DEC_DESTROY, NULL, NULL);
	while (gf_list_count(ctx->src_packets)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_packets);
}

static GF_Err xviddec_process(GF_Filter *filter)
{
#ifdef XVID_USE_OLD_API
	XVID_DEC_FRAME frame;
#else
	xvid_dec_frame_t frame;
#endif
	u8 *buffer;
	u32 i, count;
	Bool is_seek;
#if 0
	s32 postproc;
#endif
	s32 res;
	GF_XVIDCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *pck_ref, *src_pck, *dst_pck;

	if (!ctx->codec) return GF_SERVICE_ERROR;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	memset(&frame, 0, sizeof(frame));
	if (pck) {
		u64 cts = gf_filter_pck_get_cts(pck);;
		frame.bitstream = (char *) gf_filter_pck_get_data(pck, &frame.length);

		//append in cts order since we get output frames in cts order
		pck_ref = pck;
		gf_filter_pck_ref_props(&pck_ref);
		count = gf_list_count(ctx->src_packets);
		src_pck = NULL;
		for (i=0; i<count; i++) {
			u64 acts;
			src_pck = gf_list_get(ctx->src_packets, i);
			acts = gf_filter_pck_get_cts(src_pck);
			if (acts==cts) {
				gf_filter_pck_unref(pck_ref);
				break;
			}
			if (acts>cts) {
				gf_list_insert(ctx->src_packets, pck_ref, i);
				break;
			}
			src_pck = NULL;
		}
		if (!src_pck)
			gf_list_add(ctx->src_packets, pck_ref);


	} else {
		frame.bitstream = NULL;
		frame.length = -1;
	}

packed_frame :

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->width*ctx->height*3/2, &buffer);

#ifdef XVID_USE_OLD_API
	frame.colorspace = XVID_CSP_I420;
	frame.stride = ctx->width;
	frame.image = (void *) buffer;
#else
	frame.version = XVID_VERSION;
	frame.output.csp = XVID_CSP_I420;
	frame.output.stride[0] = ctx->width;
	frame.output.plane[0] = (void *) buffer;
#endif


#if 0
	postproc = ctx->base_filters;
	/*to check, not convinced yet by results...*/
	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		/*turn off all post-proc*/
#ifdef XVID_USE_OLD_API
		postproc &= ~XVID_DEC_DEBLOCKY;
		postproc &= ~XVID_DEC_DEBLOCKUV;
#else
		postproc &= ~XVID_DEBLOCKY;
		postproc &= ~XVID_DEBLOCKUV;
		postproc &= ~XVID_FILMEFFECT;
#endif
		break;
	case GF_CODEC_LEVEL_VERY_LATE:
		/*turn off post-proc*/
#ifdef XVID_USE_OLD_API
		postproc &= ~XVID_DEC_DEBLOCKY;
#else
		postproc &= ~XVID_FILMEFFECT;
		postproc &= ~XVID_DEBLOCKY;
#endif
		break;
	case GF_CODEC_LEVEL_LATE:
#ifdef XVID_USE_OLD_API
		postproc &= ~XVID_DEC_DEBLOCKUV;
#else
		postproc &= ~XVID_DEBLOCKUV;
		postproc &= ~XVID_FILMEFFECT;
#endif
		break;
	}
#endif

	/*xvid may keep the first I frame and force a 1-frame delay, so we simply trick it*/
	if (ctx->first_frame) {
		buffer[0] = 'v';
		buffer[1] = 'o';
		buffer[2] = 'i';
		buffer[3] = 'd';
	}
	src_pck = gf_list_get(ctx->src_packets, 0);

	res = xvid_decore(ctx->codec, XVID_DEC_DECODE, &frame, NULL);
	if (res < 0) {
		gf_filter_pck_discard(dst_pck);
		if (pck) gf_filter_pid_drop_packet(ctx->ipid);
		if (src_pck) {
			gf_filter_pck_unref(src_pck);
			gf_list_pop_front(ctx->src_packets);
		}
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return pck ? GF_NON_COMPLIANT_BITSTREAM : GF_OK;
	}

	if (ctx->first_frame) {
		ctx->first_frame = GF_FALSE;
		if ((buffer[0] == 'v') && (buffer[1] == 'o') && (buffer[2] == 'i') && (buffer[3] =='d')) {
			gf_filter_pck_discard(dst_pck);
			if (pck) gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
	}

	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst_pck);
		is_seek = gf_filter_pck_get_seek_flag(src_pck);
		ctx->next_cts = gf_filter_pck_get_cts(src_pck);
		ctx->next_cts += gf_filter_pck_get_duration(src_pck);
		gf_filter_pck_unref(src_pck);
		gf_list_pop_front(ctx->src_packets);
	} else {
		is_seek = 0;
		gf_filter_pck_set_cts(dst_pck, ctx->next_cts);
	}

	if (!pck || !is_seek )
		gf_filter_pck_send(dst_pck);
	else
		gf_filter_pck_discard(dst_pck);

	if (res + 6 < frame.length) {
		frame.bitstream = ((char *)frame.bitstream) + res;
		frame.length -= res;
		goto packed_frame;
	}

	if (pck) gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static const GF_FilterCapability XVIDCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_XVIDCtx, _n)

static const GF_FilterArgs XVIDArgs[] =
{
	{ OFFS(deblock_y), "enable Y deblocking", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(deblock_uv), "enable UV deblocking", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
#ifndef XVID_USE_OLD_API
	{ OFFS(film_effect), "enable film effect", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dering_y), "enable Y deblocking", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dering_uv), "enable UV deblocking", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
#endif
	{0}
};

GF_FilterRegister XVIDRegister = {
	.name = "xviddec",
	GF_FS_SET_DESCRIPTION("XVid decoder")
	GF_FS_SET_HELP("This filter decodes MPEG-4 part 2 (and DivX) through libxvidcore library.")
	.private_size = sizeof(GF_XVIDCtx),
	.args = XVIDArgs,
	SETCAPS(XVIDCaps),
	.initialize = xviddec_initialize,
	.finalize = xviddec_finalize,
	.configure_pid = xviddec_configure_pid,
	.process = xviddec_process,
	//use low priorty, below ffmpeg one, so that hardware decs/other native impl in gpac can take over if needed
	//don't use lowest one since we use this for scalable codecs
	.priority = 100
};

#endif

const GF_FilterRegister *xviddec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_XVID
	return &XVIDRegister;
#else
	return NULL;
#endif
}
