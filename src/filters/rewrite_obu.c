/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / AV1 OBU rewrite filter
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
	Bool rcfg, tsep;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 crc;
	char *dsi;
	u32 dsi_size;
	Bool ivf_hdr;
	u32 mode;
	GF_BitStream *bs_w;
	GF_BitStream *bs_r;
	u32 w, h;
	GF_Fraction fps;
	GF_AV1Config *av1c;
	u32 av1b_cfg_size;
	u32 codec_id;
} GF_OBUMxCtx;

#ifdef GPAC_ENABLE_COVERAGE
static Bool obumx_test_filter_prop(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop)
{
	return GF_TRUE;
}
#endif

GF_Err obumx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 crc;
	const GF_PropertyValue *p, *dcd;
	GF_OBUMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	dcd = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!dcd) return GF_NON_COMPLIANT_BITSTREAM;

	crc = gf_crc_32(dcd->value.data.ptr, dcd->value.data.size);
	if (ctx->crc == crc) return GF_OK;
	ctx->crc = crc;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
#ifdef GPAC_ENABLE_COVERAGE
	//for coverage
	if (gf_sys_is_test_mode()) {
		gf_filter_pid_merge_properties(ctx->opid, pid, obumx_test_filter_prop, NULL);
	} else
#endif
	{
		gf_filter_pid_copy_properties(ctx->opid, pid);
	}
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

	ctx->ipid = pid;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);

	//offset of OBUs in AV1 config
	if (dcd && (dcd->value.data.size > 4)) {
		ctx->dsi = dcd->value.data.ptr + 4;
		ctx->dsi_size = dcd->value.data.size - 4;
	} else {
		ctx->dsi = NULL;
		ctx->dsi_size = 0;
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_CODECID);
	ctx->codec_id = p ? p->value.uint : 0;
	switch (ctx->codec_id) {
	case GF_CODECID_AV1:
		//check output type OBU vs av1b
		p = gf_filter_pid_caps_query(ctx->opid, GF_PROP_PID_FILE_EXT);
		if (p) {
			if (!strcmp(p->value.string, "obu")) ctx->mode = 0;
			else if (!strcmp(p->value.string, "av1b") || !strcmp(p->value.string, "av1")) ctx->mode = 1;
			//we might want to add a generic IVF read/write at some point
			else if (!strcmp(p->value.string, "ivf")) {
				ctx->mode = 2;
				ctx->ivf_hdr = 1;
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[OBUWrite] Couldn't guess desired output format type, assuming plain OBU\n"));
		}
		break;
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
	case GF_CODECID_VP10:
		ctx->dsi_size = 0;
		ctx->dsi = NULL;
		ctx->mode = 2;
		ctx->ivf_hdr = 1;
		break;
	}

	if (ctx->av1c) gf_odf_av1_cfg_del(ctx->av1c);
	ctx->av1c = NULL;
	if (ctx->mode==1) {
		u32 i=0;
		GF_AV1_OBUArrayEntry *obu;
		ctx->av1c = gf_odf_av1_cfg_read(dcd->value.data.ptr, dcd->value.data.size);
		if (!ctx->av1c) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OBUWrite] Invalid av1 config\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		ctx->av1b_cfg_size = 0;

		while ((obu = gf_list_enum(ctx->av1c->obu_array, &i))) {
			ctx->av1b_cfg_size += (u32) obu->obu_length;
			ctx->av1b_cfg_size += gf_av1_leb128_size(obu->obu_length);
		}
	}


	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_WIDTH);
	if (p) ctx->w = p->value.uint;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_HEIGHT);
	if (p) ctx->h = p->value.uint;
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FPS);
	if (p) ctx->fps = p->value.frac;
	if (!ctx->fps.num || !ctx->fps.den) {
		ctx->fps.num = 25;
		ctx->fps.den = 1;
	}
	gf_filter_pid_set_property_str(ctx->opid, "obu:mode", &PROP_UINT(ctx->mode) );
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);

	return GF_OK;
}


GF_Err obumx_process(GF_Filter *filter)
{
	u32 i;
	u32 frame_sizes[128], max_frames;
	GF_OBUMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 pck_size, size, sap_type, hdr_size, av1b_frame_size=0;


	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	hdr_size = 0;
	size = pck_size;
	//always add temporal delim
	if (ctx->codec_id==GF_CODECID_AV1)
		size += 2;

	sap_type = gf_filter_pck_get_sap(pck);
	if (!sap_type) {
		u8 flags = gf_filter_pck_get_dependency_flags(pck);
		if (flags) {
			//get dependsOn
			flags>>=4;
			flags &= 0x3;
			if (flags==2) sap_type = 3; //could be 1, 2 or 3
		}
	}

	if (ctx->mode==2) {
		if (ctx->ivf_hdr) hdr_size += 32;
		hdr_size += 12;
	}
	if (ctx->mode==1) {
		u32 obu_sizes=0;
		u32 frame_idx=0;

		obu_sizes = frame_sizes[0] = 3;
		if (sap_type && ctx->dsi) {
			frame_sizes[0] += ctx->av1b_cfg_size;
			obu_sizes += ctx->av1b_cfg_size;
		}

		if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
		else gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

		while (gf_bs_available(ctx->bs_r)) {
			ObuType obu_type;
			u32 obu_size;
			Bool obu_extension_flag, obu_has_size_field;
			u8 temporal_id, spatial_id;
			u32 hdr_size = (u32) gf_bs_get_position(ctx->bs_r);

			gf_av1_parse_obu_header(ctx->bs_r, &obu_type, &obu_extension_flag, &obu_has_size_field, &temporal_id, &spatial_id);

			if (!obu_has_size_field) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[OBUWrite] OBU without size field, bug in demux filter !!\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			obu_size = (u32)gf_av1_leb128_read(ctx->bs_r, NULL);
			hdr_size = (u32) gf_bs_get_position(ctx->bs_r) - hdr_size;
			gf_bs_skip_bytes(ctx->bs_r, obu_size);

			obu_size += hdr_size;
			obu_sizes += obu_size + gf_av1_leb128_size(obu_size);

			if (obu_type==OBU_FRAME) {
				frame_idx++;
				if (frame_idx==128) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[OBUWrite] more than 128 frames in a temporal unit not supported\n"));
					return GF_NOT_SUPPORTED;
				}
				if (frame_idx>1)
					frame_sizes[frame_idx-1] = 0;
			}
			frame_sizes[frame_idx ? (frame_idx-1) : 0] += obu_size + gf_av1_leb128_size(obu_size);
		}
		max_frames = frame_idx;
		size = obu_sizes;
		for (i=0;i<max_frames; i++) {
			size += gf_av1_leb128_size(frame_sizes[i]);
		}
		av1b_frame_size = size;
		size += gf_av1_leb128_size(size);
	} else {
		if (sap_type && ctx->dsi) size += ctx->dsi_size;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, hdr_size+size, &output);

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, hdr_size+size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, output, hdr_size+size);

	if (ctx->mode==1) {
		GF_AV1_OBUArrayEntry *obu;
		u32 frame_idx = 0;
		//temporal unit
		gf_av1_leb128_write(ctx->bs_w, av1b_frame_size);

		gf_av1_leb128_write(ctx->bs_w, frame_sizes[0]);

		//write temporal delim with obu size set
		gf_av1_leb128_write(ctx->bs_w, 2);
		gf_bs_write_u8(ctx->bs_w, 0x12);
		gf_bs_write_u8(ctx->bs_w, 0);

		if (sap_type && ctx->dsi) {
			i=0;
			while ((obu = gf_list_enum(ctx->av1c->obu_array, &i))) {
				gf_av1_leb128_write(ctx->bs_w, obu->obu_length);
				gf_bs_write_data(ctx->bs_w, obu->obu, (u32) obu->obu_length);
			}
		}

		gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

		while (gf_bs_available(ctx->bs_r)) {
			ObuType obu_type;
			u32 obu_size;
			Bool obu_extension_flag, obu_has_size_field;
			u8 temporal_id, spatial_id;
			u32 hdr_size, start = (u32) gf_bs_get_position(ctx->bs_r);

			gf_av1_parse_obu_header(ctx->bs_r, &obu_type, &obu_extension_flag, &obu_has_size_field, &temporal_id, &spatial_id);
			obu_size = (u32)gf_av1_leb128_read(ctx->bs_r, NULL);

			hdr_size = (u32) gf_bs_get_position(ctx->bs_r) - start;
			gf_bs_skip_bytes(ctx->bs_r, obu_size);

			if (obu_type==OBU_FRAME) {
				frame_idx++;
				if (frame_idx>1) {
					gf_av1_leb128_write(ctx->bs_w, frame_sizes[frame_idx-1] );
				}
			}

			obu_size += hdr_size;
			gf_av1_leb128_write(ctx->bs_w, obu_size);
			gf_bs_write_data(ctx->bs_w, data+start, obu_size);

		}
		assert(gf_bs_get_position(ctx->bs_w) == size);
	} else {

		//write IVF headers
		if (ctx->ivf_hdr) {
			gf_bs_write_u32(ctx->bs_w, GF_4CC('D', 'K', 'I', 'F'));
			gf_bs_write_u16_le(ctx->bs_w, 0);
			gf_bs_write_u16_le(ctx->bs_w, 32);

			gf_bs_write_u32(ctx->bs_w, GF_4CC('A', 'V', '0', '1') ); //codec_fourcc
			gf_bs_write_u16_le(ctx->bs_w, ctx->w);
			gf_bs_write_u16_le(ctx->bs_w, ctx->h);
			gf_bs_write_u32_le(ctx->bs_w, ctx->fps.num);
			gf_bs_write_u32_le(ctx->bs_w, ctx->fps.den);
			gf_bs_write_u32_le(ctx->bs_w, 0); //nb frames
			gf_bs_write_u32_le(ctx->bs_w, 0);
			ctx->ivf_hdr = 0;
		}
		if (ctx->mode==2) {
			u64 cts = gf_filter_pck_get_cts(pck);
			cts *= ctx->fps.den;
			cts /= ctx->fps.num;
			cts /= gf_filter_pck_get_timescale(pck);
			gf_bs_write_u32_le(ctx->bs_w, size);
			gf_bs_write_u64(ctx->bs_w, cts);
		}
		if (ctx->codec_id==GF_CODECID_AV1) {
			//write temporal delim with obu size set
			gf_bs_write_u8(ctx->bs_w, 0x12);
			gf_bs_write_u8(ctx->bs_w, 0);

			if (sap_type && ctx->dsi) {
				gf_bs_write_data(ctx->bs_w, ctx->dsi, ctx->dsi_size);
			}
		}
		gf_bs_write_data(ctx->bs_w, data, pck_size);
	}

	if (!ctx->rcfg) {
		ctx->dsi = NULL;
		ctx->dsi_size = 0;
	}
	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}
static void obumx_finalize(GF_Filter *filter)
{
	GF_OBUMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->av1c) gf_odf_av1_cfg_del(ctx->av1c);
}

static const GF_FilterCapability OBUMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_VP8),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_VP9),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_STATIC_OPT, 	GF_PROP_PID_DASH_MODE, 0),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};



#define OFFS(_n)	#_n, offsetof(GF_OBUMxCtx, _n)
static const GF_FilterArgs OBUMxArgs[] =
{
	{ OFFS(rcfg), "force repeating decoder config at each I-frame", GF_PROP_BOOL, "true", NULL, 0},
	{0}
};


GF_FilterRegister OBUMxRegister = {
	.name = "ufobu",
	GF_FS_SET_DESCRIPTION("IVF/OBU/annexB writer")
	GF_FS_SET_HELP("This filter is used to rewrite AV1 OBU bitstream into IVF, annex B or OBU sequence, reinserting the temporal delimiter OBU.")
	.private_size = sizeof(GF_OBUMxCtx),
	.args = OBUMxArgs,
	SETCAPS(OBUMxCaps),
	.finalize = obumx_finalize,
	.configure_pid = obumx_configure_pid,
	.process = obumx_process
};


const GF_FilterRegister *obumx_register(GF_FilterSession *session)
{
	return &OBUMxRegister;
}
