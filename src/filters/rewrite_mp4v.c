/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 part2 video rewrite filter
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
#include <gpac/internal/media_dev.h>

typedef struct
{
	//opts
	Bool rcfg;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 crc;
	char *dsi;
	u32 dsi_size;

	Bool is_vc1;
	Bool vc1_ilaced;
} GF_M4VMxCtx;

GF_Err m4vmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 crc;
	const GF_PropertyValue *dcd;
	GF_M4VMxCtx *ctx = gf_filter_get_udta(filter);

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

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	ctx->ipid = pid;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);

	dcd = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	//not ready yet
	if (!dcd) return GF_OK;

	crc = gf_crc_32(dcd->value.data.ptr, dcd->value.data.size);
	if (ctx->crc == crc) return GF_OK;
	ctx->crc = crc;

	ctx->dsi = dcd->value.data.ptr;
	ctx->dsi_size = dcd->value.data.size;


	if (ctx->is_vc1 && ctx->dsi && (ctx->dsi_size>=7)) {
		ctx->vc1_ilaced = ((ctx->dsi[2] & 0x20) == 0x20) ? GF_FALSE : GF_TRUE;
		ctx->dsi += 7;
		ctx->dsi_size -= 7;
	}

	return GF_OK;
}


GF_Err m4vmx_process(GF_Filter *filter)
{
	GF_M4VMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	u8 *data, *output;
	u32 pck_size, size, sap_type;
	u8 inject_startcode=0;
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

	sap_type = gf_filter_pck_get_sap(pck);
	if (!sap_type) {
		u8 flags = gf_filter_pck_get_dependency_flags(pck);
		//get dependsOn
		if (flags) {
			flags>>=4;
			flags &= 0x3;
			if (flags==2) sap_type = 3; //could be 1, 2 or 3
		}
	}


	if (ctx->is_vc1 && (data[0]!=0) && (data[1]!=0) && (data[2]!=1)) {
		if (ctx->vc1_ilaced && ((data[0] & 0xC0) == 0xC0))
			inject_startcode = 0x0C;
		else
			inject_startcode = 0x0D;
	}

	if ((sap_type && ctx->dsi) || inject_startcode) {
		GF_FilterPacket *dst_pck;
		u32 offset=0;
		size = pck_size;
		if (inject_startcode) size += 4;
		if (sap_type) size += ctx->dsi_size;

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		if (!dst_pck) return GF_OUT_OF_MEM;
		if (sap_type) {
			memcpy(output, ctx->dsi, ctx->dsi_size);
			offset = ctx->dsi_size;
		}
		if (inject_startcode) {
			output[offset] = 0;
			output[offset+1] = 0;
			output[offset+2] = 1;
			output[offset+3] = inject_startcode;
			offset+=4;
		}

		memcpy(output+offset, data, pck_size);
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
		gf_filter_pck_send(dst_pck);

		if (!ctx->rcfg) {
			ctx->dsi = NULL;
			ctx->dsi_size = 0;
		}

	} else {
		gf_filter_pck_forward(pck, ctx->opid);
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static const GF_FilterCapability M4VMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};



#define OFFS(_n)	#_n, offsetof(GF_M4VMxCtx, _n)
static const GF_FilterArgs M4VMxArgs[] =
{
	{ OFFS(rcfg), "force repeating decoder config at each I-frame", GF_PROP_BOOL, "true", NULL, 0},
	{0}
};


GF_FilterRegister M4VMxRegister = {
	.name = "ufm4v",
	GF_FS_SET_DESCRIPTION("M4V writer")
	GF_FS_SET_HELP("This filter converts MPEG-4 part 2 visual streams into writable format (reinsert decoder config).")
	.private_size = sizeof(GF_M4VMxCtx),
	.args = M4VMxArgs,
	SETCAPS(M4VMxCaps),
	.configure_pid = m4vmx_configure_pid,
	.process = m4vmx_process
};


const GF_FilterRegister *m4vmx_register(GF_FilterSession *session)
{
	return &M4VMxRegister;
}


static const GF_FilterCapability VC1MxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_SMPTE_VC1),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


static GF_Err vc1mx_initialize(GF_Filter *filter)
{
#ifdef GPAC_ENABLE_COVERAGE
	if (!filter) return GF_OK;
#endif
	GF_M4VMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx) ctx->is_vc1 = GF_TRUE;
	return GF_OK;
}

GF_FilterRegister VC1VMxRegister = {
	.name = "ufvc1",
	GF_FS_SET_DESCRIPTION("VC1 writer")
	GF_FS_SET_HELP("This filter converts VC1 visual streams into writable format (reinsert decoder config and start codes if needed).")
	.private_size = sizeof(GF_M4VMxCtx),
	.args = M4VMxArgs,
	SETCAPS(VC1MxCaps),
	.initialize = vc1mx_initialize,
	.configure_pid = m4vmx_configure_pid,
	.process = m4vmx_process
};


const GF_FilterRegister *vc1mx_register(GF_FilterSession *session)
{
#ifdef GPAC_ENABLE_COVERAGE
	//we currently miss vc1 test sequence
	if (gf_sys_is_cov_mode())
		vc1mx_initialize(NULL);
#endif
	return &VC1VMxRegister;
}
