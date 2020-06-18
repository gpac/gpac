/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / XIPH Theora decoder filter
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

#ifdef GPAC_HAS_THEORA

#include <theora/theora.h>

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "libtheora_static")
# endif
#endif

typedef struct
{
	GF_FilterPid *ipid, *opid;
	theora_info ti, the_ti;
	theora_state td;
	theora_comment tc;
	ogg_packet op;

	u32 cfg_crc;

	GF_List *src_packets;
	u64 next_cts;
	Bool has_reconfigured;
} GF_TheoraDecCtx;

static GF_Err theoradec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	ogg_packet oggpacket;
	GF_BitStream *bs;
	GF_TheoraDecCtx *ctx = gf_filter_get_udta(filter);


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
		if (strncmp(&p->value.data.ptr[3], "theora", 6)) return GF_NON_COMPLIANT_BITSTREAM;
		ex_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (ctx->cfg_crc == ex_crc) return GF_OK;
		ctx->cfg_crc = ex_crc;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[Theora] Reconfiguring without DSI not yet supported\n"));
		return GF_NOT_SUPPORTED;
	}
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );

	if (ctx->ipid) {
		theora_clear(&ctx->td);
		theora_info_clear(&ctx->ti);
		theora_comment_clear(&ctx->tc);
	}
	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);

	oggpacket.granulepos = -1;
	oggpacket.b_o_s = 1;
	oggpacket.e_o_s = 0;
	oggpacket.packetno = 0;

	theora_info_init(&ctx->ti);
	theora_comment_init(&ctx->tc);

	bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_Err e = GF_OK;
		oggpacket.bytes = gf_bs_read_u16(bs);
		oggpacket.packet = gf_malloc(sizeof(char) * oggpacket.bytes);
		gf_bs_read_data(bs, oggpacket.packet, oggpacket.bytes);
		if (theora_decode_header(&ctx->ti, &ctx->tc, &oggpacket) < 0 ) {
			e = GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_free(oggpacket.packet);
		if (e)	{
			gf_bs_del(bs);
			return e;
		}
	}
	theora_decode_init(&ctx->td, &ctx->ti);
	gf_bs_del(bs);

	return GF_OK;
}

static GF_Err theoradec_process(GF_Filter *filter)
{
	ogg_packet op;
	yuv_buffer yuv;
	u32 i, count;
	u8 *buffer;
	u8 *pYO, *pUO, *pVO;
	u8 *pYD, *pUD, *pVD;
	GF_TheoraDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck, *src_pck, *pck_ref;
	Bool is_seek;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck && !gf_filter_pid_is_eos(ctx->ipid))
		return GF_OK;

	memset(&yuv, 0, sizeof(yuv_buffer));
	memset(&op, 0, sizeof(ogg_packet));
	op.granulepos = -1;

	if (pck) {
		u64 cts = gf_filter_pck_get_cts(pck);
		u32 psize;
		op.packet = (char *) gf_filter_pck_get_data(pck, &psize);
		op.bytes = psize;


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
		//weird bug in theora dec: even though we feed empty packets, we still keep getting frames !
		if (!gf_list_count(ctx->src_packets)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		op.packet = NULL;
		op.bytes = 0;
	}

	if (theora_decode_packetin(&ctx->td, &op) != 0) {
		src_pck = gf_list_pop_front(ctx->src_packets);
		gf_filter_pck_unref(src_pck);
		if (pck) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	//no frame
	if (theora_decode_YUVout(&ctx->td, &yuv) != 0) {
		if (pck) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}

	if (memcmp(&ctx->ti, &ctx->the_ti, sizeof(theora_info))) {
		memcpy(&ctx->the_ti, &ctx->ti, sizeof(theora_info));

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->ti.width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->ti.height));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->ti.width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, &PROP_FRAC_INT(ctx->ti.fps_numerator, ctx->ti.fps_denominator) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_YUV) );
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->ti.width*ctx->ti.height * 3 / 2, &buffer);

	pYO = yuv.y;
	pUO = yuv.u;
	pVO = yuv.v;
	pYD = buffer;
	pUD = buffer + ctx->ti.width * ctx->ti.height;
	pVD = buffer + 5 * ctx->ti.width * ctx->ti.height / 4;

	for (i=0; i<(u32)yuv.y_height; i++) {
		memcpy(pYD, pYO, sizeof(char) * yuv.y_width);
		pYD += ctx->ti.width;
		pYO += yuv.y_stride;
		if (i%2) continue;

		memcpy(pUD, pUO, sizeof(char) * yuv.uv_width);
		memcpy(pVD, pVO, sizeof(char) * yuv.uv_width);
		pUD += ctx->ti.width/2;
		pVD += ctx->ti.width/2;
		pUO += yuv.uv_stride;
		pVO += yuv.uv_stride;
	}

	src_pck = gf_list_pop_front(ctx->src_packets);
	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst_pck);
		is_seek = gf_filter_pck_get_seek_flag(src_pck);
		ctx->next_cts = gf_filter_pck_get_cts(src_pck);
		ctx->next_cts += gf_filter_pck_get_duration(src_pck);
		gf_filter_pck_unref(src_pck);
	} else {
		is_seek = 0;
		gf_filter_pck_set_cts(dst_pck, ctx->next_cts);
	}

	if (!pck || !is_seek )
		gf_filter_pck_send(dst_pck);
	else
		gf_filter_pck_discard(dst_pck);

	if (pck) gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err theoradec_initialize(GF_Filter *filter)
{
	GF_TheoraDecCtx *ctx = gf_filter_get_udta(filter);
	ctx->src_packets = gf_list_new();
	return GF_OK;
}
static void theoradec_finalize(GF_Filter *filter)
{
	GF_TheoraDecCtx *ctx = gf_filter_get_udta(filter);

	theora_clear(&ctx->td);
	theora_info_clear(&ctx->ti);
	theora_comment_clear(&ctx->tc);

	while (gf_list_count(ctx->src_packets)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_packets);
}

static const GF_FilterCapability TheoraDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister TheoraDecRegister = {
	.name = "theoradec",
	GF_FS_SET_DESCRIPTION("Theora decoder")
	GF_FS_SET_HELP("This filter decodes Theora streams through libtheora library.")
	.private_size = sizeof(GF_TheoraDecCtx),
	.priority = 1,
	SETCAPS(TheoraDecCaps),
	.initialize = theoradec_initialize,
	.finalize = theoradec_finalize,
	.configure_pid = theoradec_configure_pid,
	.process = theoradec_process,
};

#endif

const GF_FilterRegister *theoradec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_THEORA
	return &TheoraDecRegister;
#else
	return NULL;
#endif
}

