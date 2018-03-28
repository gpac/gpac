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

typedef struct{
	u64 cts;
	u32 duration;
	u8 sap_type;
	u8 seek_flag;
} TheoraFrameInfo;

typedef struct
{
	GF_FilterPid *ipid, *opid;
	theora_info ti, the_ti;
	theora_state td;
	theora_comment tc;
	ogg_packet op;

	u32 cfg_crc;

	TheoraFrameInfo *frame_infos;
	u32 frame_infos_alloc, frame_infos_size;

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[XVID] Reconfiguring without DSI not yet supported\n"));
		return GF_NOT_SUPPORTED;
	}
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	}
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

static void theoradec_drop_frame(GF_TheoraDecCtx *ctx)
{
	if (ctx->frame_infos_size) {
		ctx->frame_infos_size--;
		memmove(&ctx->frame_infos[0], &ctx->frame_infos[1], sizeof(TheoraFrameInfo)*ctx->frame_infos_size);
	}
}

static GF_Err theoradec_process(GF_Filter *filter)
{
	ogg_packet op;
	yuv_buffer yuv;
	u32 i;
	char *buffer;
	char *pYO, *pUO, *pVO;
	unsigned char *pYD, *pUD, *pVD;
	GF_TheoraDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	Bool is_seek;

	pck = gf_filter_pid_get_packet(ctx->ipid);

	op.granulepos = -1;
	op.b_o_s = 0;
	op.e_o_s = 0;
	op.packetno = 0;

	if (pck) {
		u64 cts = gf_filter_pck_get_cts(pck);
		u32 psize;
		op.packet = (char *) gf_filter_pck_get_data(pck, &psize);
		op.bytes = psize;

		if (ctx->frame_infos_size==ctx->frame_infos_alloc) {
			ctx->frame_infos_alloc += 10;
			ctx->frame_infos = gf_realloc(ctx->frame_infos, sizeof(TheoraFrameInfo)*ctx->frame_infos_alloc);
		}
		for (i=0; i<ctx->frame_infos_size; i++) {
			if (ctx->frame_infos[i].cts > cts) {
				memmove(&ctx->frame_infos[i+1], &ctx->frame_infos[i], sizeof(TheoraFrameInfo) * (ctx->frame_infos_size-i));
				ctx->frame_infos[i].cts = cts;
				ctx->frame_infos[i].duration = gf_filter_pck_get_duration(pck);
				ctx->frame_infos[i].sap_type = gf_filter_pck_get_sap(pck);
				ctx->frame_infos[i].seek_flag = gf_filter_pck_get_seek_flag(pck);
				break;
			}
		}
		if (i==ctx->frame_infos_size) {
			ctx->frame_infos[i].cts = cts;
			ctx->frame_infos[i].duration = gf_filter_pck_get_duration(pck);
			ctx->frame_infos[i].sap_type = gf_filter_pck_get_sap(pck);
			ctx->frame_infos[i].seek_flag = gf_filter_pck_get_seek_flag(pck);
		}
		ctx->frame_infos_size++;
	} else {
		op.packet = NULL;
		op.bytes = 0;
	}

	is_seek = ctx->frame_infos[0].seek_flag;

	if (theora_decode_packetin(&ctx->td, &op)<0) {
		theoradec_drop_frame(ctx);
		if (pck) gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (is_seek) {
		theoradec_drop_frame(ctx);
		if (pck) gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	//no frame
	if (theora_decode_YUVout(&ctx->td, &yuv)<0) {
		if (pck) gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (memcmp(&ctx->ti, &ctx->the_ti, sizeof(theora_info))) {
		memcpy(&ctx->the_ti, &ctx->ti, sizeof(theora_info));

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->ti.width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->ti.height));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->ti.width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, &PROP_FRAC_INT(ctx->ti.fps_numerator, ctx->ti.fps_denominator) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_YV12) );
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

	gf_filter_pck_set_cts(dst_pck, ctx->frame_infos[0].cts);
	gf_filter_pck_set_sap(dst_pck, ctx->frame_infos[0].sap_type);
	gf_filter_pck_set_duration(dst_pck, ctx->frame_infos[0].duration);

	theoradec_drop_frame(ctx);

	if (!pck || !is_seek )
		gf_filter_pck_send(dst_pck);
	else
		gf_filter_pck_discard(dst_pck);

	if (pck) gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}


static void theoradec_finalize(GF_Filter *filter)
{
	GF_TheoraDecCtx *ctx = gf_filter_get_udta(filter);

	theora_clear(&ctx->td);
	theora_info_clear(&ctx->ti);
	theora_comment_clear(&ctx->tc);
	if (ctx->frame_infos) gf_free(ctx->frame_infos);
}

static const GF_FilterCapability TheoraDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister TheoraDecRegister = {
	.name = "theora_dec",
	.description = "OGG/Theora decoder",
	.private_size = sizeof(GF_TheoraDecCtx),
	SETCAPS(TheoraDecCaps),
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

