/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / GPAC stream serializer filter
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
#include <gpac/network.h>
#include <gpac/internal/media_dev.h>
#include <gpac/crypt.h>

typedef struct
{
	GF_FilterPid *pid;

	u32 idx;
	u16 nb_pck;
	Bool eos;
} GF_GSFStream;

typedef struct
{
	//opts
	Bool for_test;
	Bool sigsn, sigdur, sigbo, sigdts;
	u32 dbg;
	const char *magic;
	GF_PropData key;
	GF_PropData IV;
	GF_Fraction pattern;
	u32 mpck;

	//only one output pid declared
	GF_FilterPid *opid;

	GF_List *streams;

	char *buffer;
	u32 alloc_size;
	GF_BitStream *bs_w;

	Bool is_start;
	u32 max_pid_idx;

	bin128 crypt_IV;
	GF_Crypt *crypt;
	Bool regenerate_tunein_info;
} GF_GSFMxCtx;


typedef enum
{
	GFS_PCKTYPE_HDR=0,
	GFS_PCKTYPE_PID_CONFIG,
	GFS_PCKTYPE_PID_INFO_UPDATE,
	GFS_PCKTYPE_PID_REMOVE,
	GFS_PCKTYPE_PID_EOS,
	GFS_PCKTYPE_PCK,
	GFS_PCKTYPE_PCK_CONT,
	GFS_PCKTYPE_PCK_LAST,

	GFS_PCKTYPE_UNDEF1,
	GFS_PCKTYPE_UNDEF2,
	GFS_PCKTYPE_UNDEF3,
	GFS_PCKTYPE_UNDEF4,
	GFS_PCKTYPE_UNDEF5,
	GFS_PCKTYPE_UNDEF6,
	GFS_PCKTYPE_UNDEF7,
	GFS_PCKTYPE_UNDEF8,
	/*NO MORE PACKET TYPE AVAILABLE*/
} GF_GSFPacketType;

static void gsfmx_send_packet(GF_GSFMxCtx *ctx, GF_GSFStream *gst, GF_GSFPacketType pck_type, Bool is_end, Bool is_redundant)
{
	u32 crypt_offset=0;
	char *output;
	u32 psize, osize;
	Bool do_encrypt = GF_FALSE;
	u32 lmode = 0, hdr_size=1;
	GF_FilterPacket *dst_pck;

	gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->buffer, &psize, &ctx->alloc_size);

	if (psize >= 1<<24) lmode=3;
	else if (psize >= 1<<16) lmode=2;
	else if (psize >= 1<<8) lmode=1;

	//psize is without packet header, SN and len field
	osize = hdr_size + psize + (lmode+1);
	if (ctx->sigsn) osize += 2;

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
	//format header
	gf_bs_reassign_buffer(ctx->bs_w, output, osize);
	gf_bs_write_int(ctx->bs_w, (gst && ctx->sigsn) ? 1 : 0, 1);
	gf_bs_write_int(ctx->bs_w, lmode, 2);

	if (pck_type == GFS_PCKTYPE_HDR) {
		if (ctx->crypt) {
			crypt_offset=25; //sig + version + IV + pattern
			do_encrypt = GF_TRUE;
		}
	} else {
		if (ctx->crypt) {
			switch (pck_type) {
			case GFS_PCKTYPE_PID_EOS:
			case GFS_PCKTYPE_PID_REMOVE:
				break;
			default:
				do_encrypt = GF_TRUE;
				break;
			}
		}
	}

	gf_bs_write_int(ctx->bs_w, do_encrypt ? 1 : 0, 1);

	gf_bs_write_int(ctx->bs_w, pck_type, 4);
	if (gst && ctx->sigsn) {
		gf_bs_write_u32(ctx->bs_w, gst->nb_pck);
		hdr_size+=2;
		gst->nb_pck++;
	}
	gf_bs_write_int(ctx->bs_w, psize, 8*(lmode+1) );
	hdr_size += (lmode+1);

	assert(hdr_size+psize==osize);
	memcpy(output+hdr_size, ctx->buffer, psize);

	gf_bs_get_content_no_truncate(ctx->bs_w, &output, &psize, NULL);

	if (do_encrypt) {
		hdr_size += crypt_offset;
		u32 nb_crypt_bytes = osize - hdr_size;
		u32 clear_trail = nb_crypt_bytes % 16;
		nb_crypt_bytes -= clear_trail;

		gf_crypt_set_IV(ctx->crypt, ctx->crypt_IV, 16);
		if (ctx->pattern.den && ctx->pattern.num) {
			u32 pos = hdr_size;
			while (nb_crypt_bytes) {
				u32 bbytes = 16 * (ctx->pattern.num + ctx->pattern.den);
				gf_crypt_encrypt(ctx->crypt, output + pos, nb_crypt_bytes >= (u32) (16*ctx->pattern.num) ? 16*ctx->pattern.num : nb_crypt_bytes);
				if (nb_crypt_bytes >= bbytes) {
					pos += bbytes;
					nb_crypt_bytes -= bbytes;
				} else {
					nb_crypt_bytes = 0;
				}
			}
		} else {
			gf_crypt_encrypt(ctx->crypt, output+hdr_size, nb_crypt_bytes);
		}
	}

	if (is_redundant) {
		gf_filter_pck_set_dependency_flag(dst_pck, 1);
		gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
	} else {
		gf_filter_pck_set_framing(dst_pck, ctx->is_start, is_end);
	}
	gf_filter_pck_send(dst_pck);
}

static GFINLINE void gsfmx_write_vlen(GF_GSFMxCtx *ctx, u32 len)
{
	if (len<=0x7F) {
		gf_bs_write_u8(ctx->bs_w, len);
	} else {
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, len, 31);
	}
}

static void gsfmx_send_pid_rem(GF_GSFMxCtx *ctx, GF_GSFStream *gst)
{
	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	//pid ID
	gsfmx_write_vlen(ctx, gst->idx);
	gsfmx_send_packet(ctx, gst, GFS_PCKTYPE_PID_REMOVE, GF_FALSE, GF_FALSE);

}

static void gsfmx_send_pid_eos(GF_GSFMxCtx *ctx, GF_GSFStream *gst, Bool is_eos)
{
	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	//pid ID
	gsfmx_write_vlen(ctx, gst->idx);
	gsfmx_send_packet(ctx, gst, GFS_PCKTYPE_PID_EOS, is_eos, GF_FALSE);

}


static void gsfmx_write_prop(GF_GSFMxCtx *ctx, const GF_PropertyValue *p)
{
	u32 len, len2, i;
	switch (p->type) {
	case GF_PROP_SINT:
	case GF_PROP_UINT:
	case GF_PROP_PIXFMT:
	case GF_PROP_PCMFMT:
		gf_bs_write_u32(ctx->bs_w, p->value.uint);
		break;
	case GF_PROP_LSINT:
	case GF_PROP_LUINT:
		gf_bs_write_u64(ctx->bs_w, p->value.longuint);
		break;
	case GF_PROP_BOOL:
		gf_bs_write_u8(ctx->bs_w, p->value.boolean ? 1 : 0);
		break;
	case GF_PROP_FRACTION:
		gf_bs_write_u32(ctx->bs_w, p->value.frac.num);
		gf_bs_write_u32(ctx->bs_w, p->value.frac.den);
		break;
	case GF_PROP_FRACTION64:
		gf_bs_write_u64(ctx->bs_w, p->value.lfrac.num);
		gf_bs_write_u64(ctx->bs_w, p->value.lfrac.den);
		break;
	case GF_PROP_FLOAT:
		gf_bs_write_float(ctx->bs_w, FIX2FLT(p->value.fnumber) );
		break;
	case GF_PROP_DOUBLE:
		gf_bs_write_double(ctx->bs_w, p->value.number);
		break;
	case GF_PROP_VEC2I:
		gf_bs_write_u32(ctx->bs_w, p->value.vec2i.x);
		gf_bs_write_u32(ctx->bs_w, p->value.vec2i.y);
		break;
	case GF_PROP_VEC2:
		gf_bs_write_double(ctx->bs_w, p->value.vec2.x);
		gf_bs_write_double(ctx->bs_w, p->value.vec2.y);
		break;
	case GF_PROP_VEC3I:
		gf_bs_write_u32(ctx->bs_w, p->value.vec3i.x);
		gf_bs_write_u32(ctx->bs_w, p->value.vec3i.y);
		gf_bs_write_u32(ctx->bs_w, p->value.vec3i.z);
		break;
	case GF_PROP_VEC3:
		gf_bs_write_double(ctx->bs_w, p->value.vec3.x);
		gf_bs_write_double(ctx->bs_w, p->value.vec3.y);
		gf_bs_write_double(ctx->bs_w, p->value.vec3.z);
		break;
	case GF_PROP_VEC4I:
		gf_bs_write_u32(ctx->bs_w, p->value.vec4i.x);
		gf_bs_write_u32(ctx->bs_w, p->value.vec4i.y);
		gf_bs_write_u32(ctx->bs_w, p->value.vec4i.z);
		gf_bs_write_u32(ctx->bs_w, p->value.vec4i.w);
		break;
	case GF_PROP_VEC4:
		gf_bs_write_double(ctx->bs_w, p->value.vec4.x);
		gf_bs_write_double(ctx->bs_w, p->value.vec4.y);
		gf_bs_write_double(ctx->bs_w, p->value.vec4.z);
		gf_bs_write_double(ctx->bs_w, p->value.vec4.w);
		break;
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
	case GF_PROP_NAME:
		len = strlen(p->value.string);
		gsfmx_write_vlen(ctx, len);
		gf_bs_write_data(ctx->bs_w, p->value.string, len);
		break;

	case GF_PROP_DATA:
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_CONST_DATA:
		len = p->value.data.size;
		gsfmx_write_vlen(ctx, len);
		gf_bs_write_data(ctx->bs_w, p->value.data.ptr, len);
		break;

	//string list: memory is ALWAYS duplicated
	case GF_PROP_STRING_LIST:
		len2 = gf_list_count(p->value.string_list);
		gsfmx_write_vlen(ctx, len2);
		for (i=0; i<len2; i++) {
			const char *str = gf_list_get(p->value.string_list, i);
			len = strlen(str);
			gsfmx_write_vlen(ctx, len);
			gf_bs_write_data(ctx->bs_w, str, len);
		}
		break;

	case GF_PROP_UINT_LIST:
		len = p->value.uint_list.nb_items;
		gsfmx_write_vlen(ctx, len);
		for (i=0; i<len; i++) {
			gf_bs_write_u32(ctx->bs_w, p->value.uint_list.vals[i] );
		}
		break;
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Cannot serialize pointer property, ignoring !!\n"));
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Cannot serialize property of unknown type, ignoring !!\n"));
		break;
	}
}

static void gsfmx_write_pid_config(GF_GSFMxCtx *ctx, GF_GSFStream *gst)
{
	u32 nb_4cc_props=0;
	u32 nb_str_props=0;
	u32 idx=0;

	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pid_enum_properties(gst->pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (prop_4cc) nb_4cc_props++;
		if (prop_name) nb_str_props++;
	}

	//pid ID
	gsfmx_write_vlen(ctx, gst->idx);
	gsfmx_write_vlen(ctx, nb_4cc_props);
	gsfmx_write_vlen(ctx, nb_str_props);

	idx=0;
	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pid_enum_properties(gst->pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (prop_name) continue;

		gf_bs_write_u32(ctx->bs_w, prop_4cc);

		gsfmx_write_prop(ctx, p);
	}

	idx=0;
	while (1) {
		u32 prop_4cc, len;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pid_enum_properties(gst->pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (prop_4cc) continue;

		len = strlen(prop_name);
		gsfmx_write_vlen(ctx, len);
		gf_bs_write_data(ctx->bs_w, prop_name, len);

		gf_bs_write_u8(ctx->bs_w, p->type);
		gsfmx_write_prop(ctx, p);
	}
}



static void gsfmx_send_header(GF_GSFMxCtx *ctx, Bool is_tunein_packet)
{
	u32 vlen=0;
	u32 mlen=0;
	const char *gpac_version = NULL;

	if (!ctx->bs_w) {
 		ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		if (!ctx->bs_w) return;
	} else {
		gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	}

	if (!ctx->for_test) {
		gpac_version = GPAC_FULL_VERSION;
		vlen = strlen(gpac_version);
	}
	if (ctx->magic) {
		mlen = strlen(ctx->magic);
	}

	//header:signature
	gf_bs_write_u32(ctx->bs_w, GF_4CC('G','S','S','F') );
	//header protocol version
	gf_bs_write_u8(ctx->bs_w, 1);

	if (ctx->crypt) {
		gf_bs_write_data(ctx->bs_w, ctx->crypt_IV, 16);
		gf_bs_write_u16(ctx->bs_w, ctx->pattern.num);
		gf_bs_write_u16(ctx->bs_w, ctx->pattern.den);
	}

	//header:magic
	gsfmx_write_vlen(ctx, mlen);
	if (ctx->magic) {
		gf_bs_write_data(ctx->bs_w, ctx->magic, mlen);
	}

	//header:version
	gsfmx_write_vlen(ctx, vlen);
	if (gpac_version) {
		gf_bs_write_data(ctx->bs_w, gpac_version, vlen);
	}

	if (is_tunein_packet) {
		u32 i, count = gf_list_count(ctx->streams);
		//number of tunein configs
		gsfmx_write_vlen(ctx, count);
		for (i=0; i<count; i++) {
			GF_GSFStream *gst = gf_list_get(ctx->streams, i);
			gsfmx_write_pid_config(ctx, gst);
		}
	} else {
		//number of tunein configs
		gsfmx_write_vlen(ctx, 0);
	}

	gsfmx_send_packet(ctx, NULL, GFS_PCKTYPE_HDR, GF_FALSE, is_tunein_packet);
	ctx->is_start = GF_FALSE;
}

static void gsfmx_send_pid_config(GF_GSFMxCtx *ctx, GF_GSFStream *gst, GF_GSFPacketType pck_type)
{
	if (ctx->is_start) {
		gsfmx_send_header(ctx, GF_FALSE);
	}

	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	gsfmx_write_pid_config(ctx, gst);
	gsfmx_send_packet(ctx, gst, pck_type, GF_FALSE, GF_FALSE);
	ctx->regenerate_tunein_info = GF_TRUE;
}

static u32 gsfmx_get_header_size(GF_GSFMxCtx *ctx, u32 psize)
{
	u32 lmode = 0;
	if (psize >= 1<<24) lmode=3;
	else if (psize >= 1<<16) lmode=2;
	else if (psize >= 1<<8) lmode=1;
	return 1 + lmode+1 + (ctx->sigsn ? 2 : 0);
}

static void gsfmx_write_packet(GF_GSFMxCtx *ctx, GF_GSFStream *gst, GF_FilterPacket *pck)
{
	u32 w=0, h=0, stride=0, stride_uv=0, pf=0;
	u32 nb_planes=0, uv_height=0;
	const char *data;
	u32 psize;
	GF_FilterHWFrame *hwframe=NULL;
	u32 nb_4cc_props=0;
	u32 nb_str_props=0;
	u32 idx=0;
	u32 tsmode=0;
	u32 tsmodebits=0;
	u32 sizemode=0;
	u32 durmode=0;
	u32 durmodebits=0;
	const GF_PropertyValue *p;

	if (ctx->dbg==2) return;

	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (prop_4cc) nb_4cc_props++;
		if (prop_name) nb_str_props++;
	}

	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);

	data = gf_filter_pck_get_data(pck, &psize);
	if (!data) {
		hwframe = gf_filter_pck_get_hw_frame(pck);
		if (hwframe) {
			p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_WIDTH);
			w = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_HEIGHT);
			h = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_PIXFMT);
			pf = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_STRIDE);
			stride = p ? p->value.uint : 0;
			p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_STRIDE_UV);
			stride_uv = p ? p->value.uint : 0;

			if (! gf_pixel_get_size_info(pf, w, h, &psize, &stride, &stride_uv, &nb_planes, &uv_height)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFMux] Raw packet with unsupported pixel format, cannot send\n"));
				return;
			}
		}
	}

	if (ctx->dbg) psize=0;

	if (psize < 1<<8) sizemode=0;
	else if (psize < 1<<16) sizemode=1;
	else if (psize < 1<<24) sizemode=2;
	else sizemode=3;

	u64 dts = gf_filter_pck_get_dts(pck);
	u64 cts = gf_filter_pck_get_cts(pck);
	u8 has_dts=0;
	u8 has_cts=0;

	//packet structure:
	//vlen stream idx
	//flags first byte: 1(has_dts) 1(has_ctx) 1(has_byteoffset) 1(corrupted) 1(seek) 2(encrypted) 1(has_carousel)
	//flags second byte: 2(ilaced) 2(cktype) 3(sap) 1(has_sample_deps)
	//flags third byte: 2(tsmode) 2(durmode) 2(sizemode) 1(has builtin props) 1(has props)
	//size on 8*(sizemode+1)
	//if has_dts, dts on tsmodebits
	//if has_cts, cts on tsmodebits
	//if durmode>0, dur on durmodbits
	//if sap==4, roll on signed 16 bits
	//if (has sample deps) sample_deps_flags on 8 bits
	//if has_carousel, carrousel version on 8 bits
	//if has_byteoffset, byte offset on 64 bits
	//if (has builtin) vlen nb builtin_props then props[builtin_props]
	//if (has props) vlen nb_str_props then props[nb_str_props]
	//data

	gsfmx_write_vlen(ctx, gst->idx);
	if (ctx->sigdts && (dts!=GF_FILTER_NO_TS) && (dts!=cts)) {
		has_dts=1;

		if (dts>0xFFFFFFFFUL) tsmode=3;
		else if (dts>0xFFFFFF) tsmode=2;
		else if (dts>0xFFFF) tsmode=1;
	}
	if (cts!=GF_FILTER_NO_TS) {
		has_cts=1;
		if (cts>0xFFFFFFFFUL) tsmode=3;
		else if (cts>0xFFFFFF) tsmode=2;
		else if (cts>0xFFFF) tsmode=1;
	}

	if (tsmode==3) tsmodebits=64;
	else if (tsmode==2) tsmodebits=32;
	else if (tsmode==1) tsmodebits=24;
	else tsmodebits=16;

	//first flags byte
	gf_bs_write_int(ctx->bs_w, has_dts ? 1 : 0, 1);
	gf_bs_write_int(ctx->bs_w, has_cts ? 1 : 0, 1);

	u64 bo = ctx->sigbo ? gf_filter_pck_get_byte_offset(pck) : GF_FILTER_NO_BO;
	gf_bs_write_int(ctx->bs_w, (bo==GF_FILTER_NO_BO) ? 0 : 1, 1);

	u8 corr = gf_filter_pck_get_corrupted(pck);
	gf_bs_write_int(ctx->bs_w, corr ? 1 : 0, 1);

	u8 seek = gf_filter_pck_get_seek_flag(pck);
	gf_bs_write_int(ctx->bs_w, seek ? 1 : 0, 1);

	u8 crypt = gf_filter_pck_get_crypt_flags(pck);
	gf_bs_write_int(ctx->bs_w, crypt, 2);

	u8 carv = gf_filter_pck_get_carousel_version(pck);
	gf_bs_write_int(ctx->bs_w, carv ? 1 : 0, 1);

	//second flags byte
	u8 interl = gf_filter_pck_get_interlaced(pck);
	gf_bs_write_int(ctx->bs_w, interl, 2);

	u8 cktype = gf_filter_pck_get_clock_type(pck);
	gf_bs_write_int(ctx->bs_w, cktype, 2);

	u8 sap = gf_filter_pck_get_sap(pck);
	gf_bs_write_int(ctx->bs_w, sap, 3);

	u8 depflags = gf_filter_pck_get_dependency_flags(pck);
	gf_bs_write_int(ctx->bs_w, depflags ? 1 : 0, 1);

	//third flags byte
	u32 duration = ctx->sigdur ? gf_filter_pck_get_duration(pck) : 0;
	if (!duration) durmode=0;
	else if (duration<1<<8) { durmode=1; durmodebits=8; }
	else if (duration<1<<16) { durmode=2; durmodebits=16; }
	else { durmode=3; durmodebits=32; }

	gf_bs_write_int(ctx->bs_w, tsmode, 2);
	gf_bs_write_int(ctx->bs_w, durmode, 2);
	gf_bs_write_int(ctx->bs_w, sizemode, 2);
	gf_bs_write_int(ctx->bs_w, nb_4cc_props ? 1 : 0, 1);
	gf_bs_write_int(ctx->bs_w, nb_str_props ? 1 : 0, 1);

	//done with flags

	gf_bs_write_int(ctx->bs_w, psize, 8*(sizemode+1) );

	if (has_dts) {
		if (tsmode==3) gf_bs_write_long_int(ctx->bs_w, dts, tsmodebits);
		else gf_bs_write_int(ctx->bs_w, dts, tsmodebits);
	}
	if (has_cts) {
		if (tsmode==3) gf_bs_write_long_int(ctx->bs_w, cts, tsmodebits);
		else gf_bs_write_int(ctx->bs_w, cts, tsmodebits);
	}
	if (durmode) {
		gf_bs_write_int(ctx->bs_w, duration, durmodebits);
	}

	if (sap==GF_FILTER_SAP_4) {
		s16 roll = gf_filter_pck_get_roll_info(pck);
		gf_bs_write_u16(ctx->bs_w, roll);
	}
	if (depflags)
		gf_bs_write_u8(ctx->bs_w, depflags);

	if (carv) {
		gf_bs_write_u8(ctx->bs_w, carv);
	}
	if (bo!=GF_FILTER_NO_BO) {
		gf_bs_write_u64(ctx->bs_w, bo);
	}
	if (nb_4cc_props) {
		gsfmx_write_vlen(ctx, nb_4cc_props);

		//write packet properties
		idx=0;
		while (1) {
			u32 prop_4cc;
			const char *prop_name;
			p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			if (prop_name) continue;

			gf_bs_write_u32(ctx->bs_w, prop_4cc);

			gsfmx_write_prop(ctx, p);
		}
	}
	if (nb_str_props) {
		idx=0;
		while (1) {
			u32 prop_4cc, len;
			const char *prop_name;
			p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			if (prop_4cc) continue;

			len = strlen(prop_name);
			gsfmx_write_vlen(ctx, len);
			gf_bs_write_data(ctx->bs_w, prop_name, len);

			gf_bs_write_u8(ctx->bs_w, p->type);
			gsfmx_write_prop(ctx, p);
		}
		gsfmx_write_vlen(ctx, nb_str_props);
	}


	//write packet data
	if (ctx->dbg) {

	} else if (data) {
		u32 pck_size = 0;
		u32 hsize = 0;
		Bool do_split = GF_FALSE;
		if (ctx->mpck) {
			pck_size = gf_bs_get_position(ctx->bs_w) + psize;
			hsize = gsfmx_get_header_size(ctx, pck_size);
			if (hsize + pck_size > ctx->mpck) {
				do_split = GF_TRUE;
			}
		}

		if (!do_split) {
			u32 nb_write = gf_bs_write_data(ctx->bs_w, data, psize);
			if (nb_write != psize) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GSFMux] Write error, wrote %d bytes but had %d to write\n", nb_write, psize));
			}
			gsfmx_send_packet(ctx, gst, GFS_PCKTYPE_PCK, GF_FALSE, GF_FALSE);
		} else {
			Bool first = GF_TRUE;
			const char *ptr = data;
			hsize = gsfmx_get_header_size(ctx, ctx->mpck);
			while (psize) {
				u32 pck_type = GFS_PCKTYPE_PCK_CONT;
				u32 to_write = ctx->mpck - hsize;

				if (first) pck_type = GFS_PCKTYPE_PCK;

				if (to_write > psize) {
					to_write = psize;
					pck_type = GFS_PCKTYPE_PCK_LAST;
				}

				u32 nb_write = gf_bs_write_data(ctx->bs_w, ptr, to_write);
				if (nb_write != to_write) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GSFMux] Write error, wrote %d bytes but had %d to write\n", nb_write, to_write));
				}
				gsfmx_send_packet(ctx, gst, pck_type, GF_FALSE, GF_FALSE);
				ptr += to_write;
				psize -= to_write;
				if (!psize) break;
				first = GF_FALSE;

				gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
			}
		}
	} else if (hwframe) {
		u32 pck_size = 0;
		u32 hsize = 0;
		Bool do_split = GF_FALSE;
		Bool first=GF_TRUE;
		Bool last=GF_FALSE;
		if (ctx->mpck) {
			pck_size = gf_bs_get_position(ctx->bs_w) + psize;
			hsize = gsfmx_get_header_size(ctx, pck_size);
			if (hsize + pck_size > ctx->mpck) {
				do_split = GF_TRUE;
				hsize = gsfmx_get_header_size(ctx, ctx->mpck);
				pck_size = ctx->mpck - gf_bs_get_position(ctx->bs_w) - hsize;
			}
		}

		u32 i, bpp = gf_pixel_get_bytes_per_pixel(pf);
		for (i=0; i<nb_planes; i++) {
			u32 j, write_h, lsize;
			const u8 *out_ptr;
			u32 out_stride = i ? stride_uv : stride;
			GF_Err e = hwframe->get_plane(hwframe, i, &out_ptr, &out_stride);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GSFMux] Failed to fetch plane data from hardware frame, cannot write\n"));
				break;
			}
			write_h = h;
			if (i) write_h = uv_height;
			lsize = bpp * (i ? stride : stride_uv);
			for (j=0; j<write_h; j++) {

				if (do_split) {
					const char *ptr = out_ptr;
					u32 nb_bytes = lsize;
					//write entire line
					while (nb_bytes) {
						//try to write remaining packet size
						u32 to_write = pck_size;
						//clamp to number of bytes remaining in line
						if (to_write>nb_bytes) {
							to_write=nb_bytes;
							//last line of last plane, we are done
							if ((i+1==write_h) && (j+1==nb_planes)) last=GF_TRUE;
						}
						u32 nb_write = (u32) gf_bs_write_data(ctx->bs_w, ptr, to_write);
						if (nb_write != to_write) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GSFMux] Write error, wrote %d bytes but had %d to write\n", nb_write, to_write));
						}
						//move line pointer forward
						ptr+=to_write;
						nb_bytes-=to_write;
						pck_size -= to_write;
						psize -= to_write;
						//done with current packet, send it and reassign new bitstream for next one if any
						if (!pck_size) {
							u32 pck_type = GFS_PCKTYPE_PCK_CONT;
							if (first) pck_type = GFS_PCKTYPE_PCK;
							else if (last) pck_type = GFS_PCKTYPE_PCK_LAST;

							gsfmx_send_packet(ctx, gst, pck_type, GF_FALSE, GF_FALSE);
							if (last) break;

							pck_size = ctx->mpck - hsize;
							//this will be our last packet
							if (pck_size>psize) pck_size = psize;

							gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
						}
					}
				} else {
					u32 nb_write = (u32) gf_bs_write_data(ctx->bs_w, out_ptr, lsize);
					if (nb_write != lsize) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GSFMux] Write error, wrote %d bytes but had %d to write\n", nb_write, lsize));
					}
				}
				out_ptr += out_stride;
			}
		}
		//not fragmented, send packet
		if (!last)
			gsfmx_send_packet(ctx, gst, GFS_PCKTYPE_PCK, GF_FALSE, GF_FALSE);
	}
}

GF_Err gsfmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_GSFStream *gst=NULL;
	GF_GSFMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		gst = gf_filter_pid_get_udta(pid);
		assert(gst);
		gsfmx_send_pid_rem(ctx, gst);
		gf_list_del_item(ctx->streams, gst);
		gf_free(gst);
		if (!gf_list_count(ctx->streams)) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		//copy properties at init or reconfig
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT( GF_STREAM_FILE ) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ENCRYPTED, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING( "gsf" ) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/x-gpac-sf") );
	}

	gst = gf_filter_pid_get_udta(pid);
	if (!gst) {
		GF_SAFEALLOC(gst, GF_GSFStream);
		gf_list_add(ctx->streams, gst);
		gst->pid = pid;
		gf_filter_pid_set_udta(pid, gst);
		gst->idx = ctx->max_pid_idx;
		ctx->max_pid_idx++;

		gsfmx_send_pid_config(ctx, gst, GFS_PCKTYPE_PID_CONFIG);

	} else {
		gsfmx_send_pid_config(ctx, gst, GFS_PCKTYPE_PID_CONFIG);

	}
	return GF_OK;
}


GF_Err gsfmx_process(GF_Filter *filter)
{
	GF_GSFMxCtx *ctx = gf_filter_get_udta(filter);
	u32 i, nb_eos, count = gf_list_count(ctx->streams);

	nb_eos = 0;
	for (i=0; i<count; i++) {
		GF_GSFStream *gst = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(gst->pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(gst->pid)) {
				nb_eos++;
				if (!gst->eos) {
					gsfmx_send_pid_eos(ctx, gst, (nb_eos==count) ? GF_TRUE : GF_FALSE);
					gst->eos = GF_TRUE;
				}
			}
			continue;
		}
		gst->eos = GF_FALSE;
		gsfmx_write_packet(ctx, gst, pck);
		gf_filter_pid_drop_packet(gst->pid);
	}
	if (ctx->regenerate_tunein_info) {
		ctx->regenerate_tunein_info = GF_FALSE;
		gsfmx_send_header(ctx, GF_TRUE);
	}

	if (count && (nb_eos==count) ) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	return GF_OK;
}

static Bool gsfmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_GSFMxCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type==GF_FEVT_INFO_UPDATE) {
		GF_GSFStream *gst = gf_filter_pid_get_udta(evt->base.on_pid);
		if (gst)
			gsfmx_send_pid_config(ctx, gst, GFS_PCKTYPE_PID_INFO_UPDATE);
	}
	//don't cancel events
	return GF_FALSE;
}

static GF_Err gsfmx_initialize(GF_Filter *filter)
{
	GF_GSFMxCtx *ctx = gf_filter_get_udta(filter);

	gf_rand_init(GF_FALSE);

	if (ctx->key.size==16) {
		GF_Err e;
		if (ctx->IV.size==16) {
			memcpy(ctx->crypt_IV, ctx->IV.ptr, 16);
		} else if (ctx->IV.size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Wrong IV value, size %d expecting 16\n", ctx->key.size));
			return GF_BAD_PARAM;
		} else if (ctx->for_test) {
			memset(ctx->crypt_IV, 1, 16);
		} else {
			char szIV[64];
			u32 i;
			* (u32 *) &ctx->crypt_IV[0] = gf_rand();
			* (u32 *) &ctx->crypt_IV[4] = gf_rand();
			* (u32 *) &ctx->crypt_IV[8] = gf_rand();
			* (u32 *) &ctx->crypt_IV[12] = gf_rand();
			szIV[0] = 0;
			for (i=0; i<16; i++) {
				char szC[3];
				sprintf(szC, "%02X", ctx->crypt_IV[i]);
				strcat(szIV, szC);
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[GSFMux] Generated IV value Ox%s\n", szIV));
		}
		ctx->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!ctx->crypt) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Failed to allocate crypt context\n"));
			return GF_IO_ERR;
		}
		e = gf_crypt_init(ctx->crypt, ctx->key.ptr, ctx->crypt_IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Failed to setup encryption: %s\n", gf_error_to_string(e) ));
			return GF_IO_ERR;
		}
	} else if (ctx->key.size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Wrong key value, size %d expecting 16\n", ctx->key.size));
		return GF_BAD_PARAM;
	}

	ctx->streams = gf_list_new();
	if (!ctx->streams) return GF_OUT_OF_MEM;

	ctx->is_start = GF_TRUE;

	return GF_OK;
}

static void gsfmx_finalize(GF_Filter *filter)
{
	GF_GSFMxCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->streams)) {
		GF_GSFStream *gst = gf_list_pop_back(ctx->streams);
		gf_free(gst);
	}
	gf_list_del(ctx->streams);

	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->buffer) gf_free(ctx->buffer);
	if (ctx->crypt) gf_crypt_close(ctx->crypt);
}

static const GF_FilterCapability GSFMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-gpac-sf"),
	{0},
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "gsf")
};



#define OFFS(_n)	#_n, offsetof(GF_GSFMxCtx, _n)
static const GF_FilterArgs GSFMxArgs[] =
{
	{ OFFS(sigsn), "signal packet sequence number after header field and before size field. Sequence number is per PID, encoded on 16 bits. Header packet does not have a SN", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(sigdur), "signal duration", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(sigbo), "signal byte offset", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(sigdts), "signal decoding timestamp", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(dbg), "debug mode, skips packet or force packet size to 0", GF_PROP_UINT, "no", "no|nodata|nopck", GF_FALSE},
	{ OFFS(key), "encrypts packets using given key - see filter helps", GF_PROP_DATA, NULL, NULL, GF_FALSE},
	{ OFFS(IV), "sets IV for encryption - a constant IV is used to keep packet overhead small (cbcs-like)", GF_PROP_DATA, NULL, NULL, GF_FALSE},
	{ OFFS(pattern), "sets nb crypt / nb_skip block pattern. default is all encrypted", GF_PROP_FRACTION, "1/0", NULL, GF_FALSE},
	{ OFFS(mpck), "sets max packet size. 0 means no fragmentation (each AU is sent in one packet)", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(for_test), "do not include gpac version number in header for regression tests hashing", GF_PROP_BOOL, "false", NULL, GF_FALSE},

	{0}
};


GF_FilterRegister GSFMxRegister = {
	.name = "gsfm",
	.description = "GPAC stream format writer",
	.comment = "This filter serialize the stream states (config/reconfig/info update/remove/eos) and packets of input PIDs.\n"\
			"This allows either saving to file a session, or forwarding the state/data of streams to another instance of GPAC\n"\
			"using either pipes or sockets\n"\
			"\n"\
			"Upstream events are not serialized\n"\
			"The stream format can be encrypted in AES 128 CBC mode. For most packets, the packet header (header, size and optionnal seq num) are in the clear\n"\
			"and the followings byte until the last byte of the last multiple of block size (16) fitting in the payload are encrypted.\n"\
			"For header and tunein update packets, the first 25 bytes after the header are in the clear (signature,version,IV and pattern).\n"\
			"The IV is constant to avoid packet overhead, randomly generated if not set and sent in the initial stream header\n"\
			"Pattern mode can be used (cf CENC cbcs) to encrypt K block and leave N blocks in the clear\n"
			"\n"\
			"The serializer sends a tunein packet whenever a PID is modified. This packet is marked as redundant so that it can be discarded by output filters if needed\n",
	.private_size = sizeof(GF_GSFMxCtx),
	.max_extra_pids = (u32) -1,
	.args = GSFMxArgs,
	SETCAPS(GSFMxCaps),
	.initialize = gsfmx_initialize,
	.finalize = gsfmx_finalize,
	.configure_pid = gsfmx_configure_pid,
	.process = gsfmx_process,
	.process_event = gsfmx_process_event,
};


const GF_FilterRegister *gsfmx_register(GF_FilterSession *session)
{
	return &GSFMxRegister;
}
