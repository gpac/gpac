/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / MHAS write filter
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

#include <gpac/avparse.h>


typedef struct
{
	//opts
	Bool exporter, syncp;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	Bool is_mpha;

	GF_BitStream *bs_w;

	u8 *dsi;
	u32 dsi_size;
	u32 dsi_crc;
	Bool update_dsi;
	GF_Fraction fdsi;
	u64 last_cts;
	u32 timescale;
} GF_MHASMxCtx;




GF_Err mhasmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 crc;
	const GF_PropertyValue *p;
	GF_MHASMxCtx *ctx = gf_filter_get_udta(filter);

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

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	ctx->is_mpha = (p->value.uint==GF_CODECID_MPHA) ? GF_TRUE : GF_FALSE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (!p) return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) {
		if (ctx->is_mpha)
			return GF_NOT_SUPPORTED;
		ctx->dsi_crc = 0;
		ctx->dsi = NULL;
		ctx->dsi_size = 0;
	} else if (p->value.data.size<=5) {
		return GF_NON_COMPLIANT_BITSTREAM;
	} else {
		crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (crc != ctx->dsi_crc) {
			ctx->dsi_crc = crc;
			ctx->update_dsi = GF_TRUE;
			ctx->dsi = p->value.data.ptr + 5;
			ctx->dsi_size = p->value.data.size - 5;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (!p) return GF_NOT_SUPPORTED;
	ctx->timescale = p->value.uint;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	ctx->ipid = pid;
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);
	return GF_OK;
}



GF_Err mhasmx_process(GF_Filter *filter)
{
	GF_MHASMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 pck_size, size;
	Bool sap;
	Bool has_sync = GF_FALSE;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	if (!pck_size) {
		//if output and packet properties, forward - this is required for sinks using packets for state signaling
		//such as TS muxer in dash mode looking for EODS property
		if (ctx->opid && gf_filter_pck_has_properties(pck))
			gf_filter_pck_forward(pck, ctx->opid);

		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	sap = (gf_filter_pck_get_sap(pck)==GF_FILTER_SAP_1) ? GF_TRUE : GF_FALSE;

	if (ctx->is_mpha) {
		u32 hdr_size = 0;
		if (ctx->syncp || sap) {
			hdr_size += 3;
		}
		if (sap) {
			hdr_size += ctx->dsi_size + 2; //base header needs 2 bytes
			if (ctx->dsi_size > 2046)
				hdr_size += 3;
		}
		hdr_size += 2; //base header needs 2 bytes
		if (pck_size>2046)
			hdr_size += 3;

		size = pck_size + hdr_size;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		if (!dst_pck) return GF_OUT_OF_MEM;

		gf_bs_reassign_buffer(ctx->bs_w, output, size);

		//write MASH headers
		if (ctx->syncp || sap) {
			gf_bs_write_u8(ctx->bs_w, 0xC0);
			gf_bs_write_u8(ctx->bs_w, 0x01);
			gf_bs_write_u8(ctx->bs_w, 0xA5);
		}
		if (sap) {
			gf_bs_write_int(ctx->bs_w, 1, 3); //pck type = config
			gf_bs_write_int(ctx->bs_w, 0, 2); //label = 0
			//size
			if (ctx->dsi_size > 2046) {
				gf_bs_write_int(ctx->bs_w, 2047, 11);
				gf_bs_write_int(ctx->bs_w, ctx->dsi_size - 2047, 24);
			} else {
				gf_bs_write_int(ctx->bs_w, ctx->dsi_size, 11);
			}
			gf_bs_write_data(ctx->bs_w, ctx->dsi, ctx->dsi_size);
		}
		gf_bs_write_int(ctx->bs_w, 2, 3); //pck type = frame
		gf_bs_write_int(ctx->bs_w, 1, 2); //label
		if (pck_size > 2046) {
			gf_bs_write_int(ctx->bs_w, 2047, 11);
			gf_bs_write_int(ctx->bs_w, pck_size - 2047, 24);
		} else {
			gf_bs_write_int(ctx->bs_w, pck_size, 11);
		}
		//copy payload
		memcpy(output+hdr_size, data, pck_size);

	} else {
		if ((data[0]==0xC0) && (data[1]==0x01) && (data[2]==0xA5))
			has_sync = GF_TRUE;

		size = pck_size;
		if ((ctx->syncp && !has_sync) || (sap && !has_sync)) {
			size += 3;
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
			if (output) {
				output[0] = 0xC0;
				output[1] = 0x01;
				output[2] = 0xA5;
				memcpy(output+3, data, pck_size);
			}
		} else {
			dst_pck = gf_filter_pck_new_ref(ctx->opid, 0, 0, pck);
		}
		if (!dst_pck) return GF_OUT_OF_MEM;
	}

	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static GF_Err mhasmx_initialize(GF_Filter *filter)
{
	GF_MHASMxCtx *ctx = gf_filter_get_udta(filter);
	ctx->bs_w = gf_bs_new((u8*)ctx, 1, GF_BITSTREAM_WRITE);
	return GF_OK;
}

static void mhasmx_finalize(GF_Filter *filter)
{
	GF_MHASMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
}

static const GF_FilterCapability MHASMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MPHA),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_MHASMxCtx, _n)
static const GF_FilterArgs MHASMxArgs[] =
{
	{ OFFS(syncp), "if set, insert sync packet at each frame, otherwise only at SAP", GF_PROP_BOOL, "yes", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister MHASMxRegister = {
	.name = "ufmhas",
	GF_FS_SET_DESCRIPTION("MHAS writer")
	GF_FS_SET_HELP("This filter converts MPEG-H Audio streams into MHAS encapsulated data.")
	.private_size = sizeof(GF_MHASMxCtx),
	.args = MHASMxArgs,
	.initialize = mhasmx_initialize,
	.finalize = mhasmx_finalize,
	SETCAPS(MHASMxCaps),
	.configure_pid = mhasmx_configure_pid,
	.process = mhasmx_process
};


const GF_FilterRegister *ufmhas_register(GF_FilterSession *session)
{
	return &MHASMxRegister;
}

