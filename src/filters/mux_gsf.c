/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
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
	u16 nb_frames;
	Bool eos;
	u8 config_version;
	u64 last_cts_config;
	u32 timescale;
	u32 is_file;
} GSFStream;

typedef struct
{
	//opts
	Bool sigsn, sigdur, sigbo, sigdts, minp, mixed;
	u32 dbg;
	const char *magic;
	const char *skp;
	const char *ext, *mime, *dst;
	GF_PropData key;
	GF_PropData IV;
	GF_Fraction pattern;
	u32 mpck;
	Double crate;

	//only one output pid declared
	GF_FilterPid *opid;

	GF_List *streams;

	u8 *buffer;
	u32 alloc_size;
	GF_BitStream *bs_w;

	Bool is_start;
	u32 max_pid_idx;

	bin128 crypt_IV;
	GF_Crypt *crypt;
	Bool regenerate_tunein_info;

	u32 nb_frames;
	u32 nb_pck;

	GF_FilterCapability caps[4];
	Bool filemode;
} GSFMxCtx;


typedef enum
{
	GFS_PCKTYPE_HDR=0,
	GFS_PCKTYPE_PID_CONFIG,
	GFS_PCKTYPE_PID_INFO_UPDATE,
	GFS_PCKTYPE_PID_REMOVE,
	GFS_PCKTYPE_PID_EOS,
	GFS_PCKTYPE_PCK,

	GFS_PCKTYPE_UNDEF1,
	GFS_PCKTYPE_UNDEF2,
	GFS_PCKTYPE_UNDEF3,
	GFS_PCKTYPE_UNDEF4,
	GFS_PCKTYPE_UNDEF5,
	GFS_PCKTYPE_UNDEF6,
	GFS_PCKTYPE_UNDEF7,
	GFS_PCKTYPE_UNDEF8,
	GFS_PCKTYPE_UNDEF9,
	GFS_PCKTYPE_UNDEF10,

	/*NO MORE PACKET TYPE AVAILABLE*/
} GF_GSFPacketType;


static GFINLINE u32 get_vlen_size(u32 len)
{
	if (len<=0x7F) return 1;
	if (len<=0x3FFF) return 2;
	if (len<=0x1FFFFF) return 3;
	if (len<=0xFFFFFFF) return 4;
	return 5;
}

static GFINLINE void gsfmx_write_vlen(GSFMxCtx *ctx, u32 len)
{
	if (len<=0x7F) {
		gf_bs_write_int(ctx->bs_w, 0, 1);
		gf_bs_write_int(ctx->bs_w, len, 7);
	} else if (len <= 0x3FFF){
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 0, 1);
		gf_bs_write_int(ctx->bs_w, len, 14);
	} else if (len <= 0x1FFFFF){
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 0, 1);
		gf_bs_write_int(ctx->bs_w, len, 21);
	} else if (len <= 0xFFFFFFF){
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 0, 1);
		gf_bs_write_int(ctx->bs_w, len, 28);
	} else {
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_int(ctx->bs_w, 1, 1);
		gf_bs_write_long_int(ctx->bs_w, len, 36);
	}
}

static GFINLINE u32 gsfmx_get_header_size(GSFMxCtx *ctx, GSFStream *gst, Bool use_seq_num, Bool first_frag, Bool no_frag, u32 pck_size, u32 block_size, u32 block_offset)
{
	u32 hdr_size = 1 + get_vlen_size(pck_size);
	hdr_size += get_vlen_size(gst ? gst->idx : 0);
	if (use_seq_num) hdr_size += 2;

	if (!no_frag) {
		hdr_size += get_vlen_size(block_size);
		if (!first_frag)
			hdr_size += get_vlen_size(block_offset);
	}
	return hdr_size;
}

static void gsfmx_encrypt(GSFMxCtx *ctx, char *data, u32 nb_crypt_bytes)
{
#ifndef GPAC_DISABLE_CRYPTO
	u32 clear_trail;

	clear_trail = nb_crypt_bytes % 16;
	nb_crypt_bytes -= clear_trail;
	if (! nb_crypt_bytes) return;

	//reset IV at each packet
	gf_crypt_set_IV(ctx->crypt, ctx->crypt_IV, 16);
	if (ctx->pattern.den && ctx->pattern.num) {
		u32 bytes_per_pattern = 16 * (ctx->pattern.num + ctx->pattern.den);
		u32 offset = 0;
		while (nb_crypt_bytes) {
			gf_crypt_encrypt(ctx->crypt, data + offset, nb_crypt_bytes >= (u32) (16*ctx->pattern.num) ? 16*ctx->pattern.num : nb_crypt_bytes);
			if (nb_crypt_bytes >= bytes_per_pattern) {
				offset += bytes_per_pattern;
				nb_crypt_bytes -= bytes_per_pattern;
			} else {
				nb_crypt_bytes = 0;
			}
		}
	} else {
		gf_crypt_encrypt(ctx->crypt, data, nb_crypt_bytes);
	}
#endif // GPAC_DISABLE_CRYPTO
}

static void gsfmx_send_packets(GSFMxCtx *ctx, GSFStream *gst, GF_GSFPacketType pck_type, Bool is_end, Bool is_redundant, u32 frame_size, u32 frame_hdr_size)
{
	u32 pck_size, bytes_remain, pck_offset, block_offset;
	Bool first_frag = GF_TRUE;
	Bool no_frag = GF_TRUE;
	//for now only data packets are encrypted on a per-fragment base, others are fully encrypted
	Bool frag_encryption = (pck_type==GFS_PCKTYPE_PCK) ? GF_TRUE : GF_FALSE;
	Bool use_seq_num = (pck_type==GFS_PCKTYPE_HDR) ? GF_FALSE : ctx->sigsn;

	gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->buffer, &pck_size, &ctx->alloc_size);

	first_frag = GF_TRUE;
	bytes_remain = pck_size;
	block_offset = pck_offset = 0;
	if (!frame_size) frame_size = pck_size;

	if (ctx->crypt && !frag_encryption) {
		u32 crypt_offset=0;
		if (pck_type == GFS_PCKTYPE_HDR) {
			crypt_offset=25; //sig + version + IV + pattern
		}
		gsfmx_encrypt(ctx, ctx->buffer + crypt_offset, pck_size - crypt_offset);
	}

	while (bytes_remain) {
		u8 *output;
		GF_FilterPacket *dst_pck;
		Bool do_encrypt = GF_FALSE;
		u32 crypt_offset=0;
		u32 osize;
		u32 to_write = bytes_remain;
		u32 hdr_size = gsfmx_get_header_size(ctx, gst, use_seq_num, first_frag, no_frag, to_write, frame_size, block_offset);
		if (ctx->mpck && (ctx->mpck < to_write + hdr_size)) {
			no_frag = GF_FALSE;
			hdr_size = gsfmx_get_header_size(ctx, gst, use_seq_num, first_frag, no_frag, ctx->mpck - hdr_size, frame_size, block_offset);
			to_write = ctx->mpck - hdr_size;
		}
		osize = hdr_size + to_write;

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
		if (!dst_pck) return;

		//format header
		gf_bs_reassign_buffer(ctx->bs_w, output, osize);

		if (ctx->crypt) do_encrypt = GF_TRUE;

		gf_bs_write_int(ctx->bs_w, 0, 1); //reserved
		//fragment flag
		if (no_frag) gf_bs_write_int(ctx->bs_w, 0, 2);
		else if (first_frag) gf_bs_write_int(ctx->bs_w, 1, 2);
		else gf_bs_write_int(ctx->bs_w, 2, 2);
		//encrypt flag
		gf_bs_write_int(ctx->bs_w, do_encrypt ? 1 : 0, 1);
		//packet type
		gf_bs_write_int(ctx->bs_w, pck_type, 4);

		//packet size and seq num
		gsfmx_write_vlen(ctx, gst ? gst->idx : 0);
		if (use_seq_num) {
			gf_bs_write_u16(ctx->bs_w, gst ? gst->nb_frames : ctx->nb_frames);
		}

		//block size and block offset
		if (!no_frag) {
			gsfmx_write_vlen(ctx, frame_size);
			if (!first_frag) gsfmx_write_vlen(ctx, block_offset);
		}

		gsfmx_write_vlen(ctx, to_write);

		hdr_size = (u32) gf_bs_get_position(ctx->bs_w);
		assert(hdr_size + to_write == osize);
		memcpy(output+hdr_size, ctx->buffer + pck_offset, to_write);
		bytes_remain -= to_write;
		pck_offset += to_write;
		block_offset += to_write;
		if (frame_hdr_size) {
			if (frame_hdr_size > block_offset) {
				frame_hdr_size -= block_offset;
				block_offset = 0;
			} else {
				block_offset -= frame_hdr_size;
				frame_hdr_size = 0;
			}
		}

		if (ctx->mpck && (ctx->mpck < to_write)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFMux] packet type %d size %d exceeds max packet size %d!\n", pck_type, osize, ctx->mpck));
		}
		//this is just to detach the buffer from the bit writer
		gf_bs_get_content_no_truncate(ctx->bs_w, &output, &to_write, NULL);

		if (do_encrypt && frag_encryption) {
			//encrypt start after fragmentation block info, plus any additional offset based on packet type
			hdr_size += crypt_offset;
			gsfmx_encrypt(ctx, output+hdr_size, osize - hdr_size);
		}

		if (is_redundant) {
			gf_filter_pck_set_dependency_flags(dst_pck, 1);
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
		} else if (first_frag && bytes_remain) {
			gf_filter_pck_set_framing(dst_pck, ctx->is_start, GF_FALSE);
		} else if (!first_frag && bytes_remain) {
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
		} else if (first_frag) {
			gf_filter_pck_set_framing(dst_pck, ctx->is_start, is_end);
		} else {
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, is_end);
		}
		gf_filter_pck_send(dst_pck);

		first_frag = GF_FALSE;
	}
}

static void gsfmx_send_pid_rem(GSFMxCtx *ctx, GSFStream *gst)
{
	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	gst->nb_frames++;
	gsfmx_send_packets(ctx, gst, GFS_PCKTYPE_PID_REMOVE, GF_FALSE, GF_FALSE, 0, 0);
}

static void gsfmx_send_pid_eos(GSFMxCtx *ctx, GSFStream *gst, Bool is_eos)
{
	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	gst->nb_frames++;
	gsfmx_send_packets(ctx, gst, GFS_PCKTYPE_PID_EOS, is_eos, GF_FALSE, 0, 0);
}

static Bool gsfmx_can_serialize_prop(const GF_PropertyValue *p, u32 prop_4cc)
{
	if (prop_4cc) {
		u32 prop_type = gf_props_4cc_get_type(prop_4cc);
		//prop_type can be 0 for unit test filters !!
		if (prop_type
			&& !gf_props_type_is_enum(prop_type)
			&& (gf_props_get_base_type(prop_type) != gf_props_get_base_type(p->type))
		) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Mismatch between property advertised type (%s) and built-in type (%s) for %s, not serializing !\n\tPlease contact GPAC team or the developers of third-party filters used if any (run with -graph)\n", gf_props_get_type_name(p->type), gf_props_get_type_name(prop_type), gf_props_4cc_get_name(prop_4cc)));
			return GF_FALSE;
		}
	}

	switch (p->type) {
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFMux] Cannot serialize pointer property, ignoring !!\n"));
	case GF_PROP_FORBIDEN:
		return GF_FALSE;
	default:
		if (p->type>=GF_PROP_LAST_DEFINED)
			return GF_FALSE;
		return GF_TRUE;
	}
}

static void gsfmx_write_prop(GSFMxCtx *ctx, const GF_PropertyValue *p)
{
	u32 len, len2, i;
	switch (p->type) {
	case GF_PROP_SINT:
	case GF_PROP_UINT:
		gsfmx_write_vlen(ctx, p->value.uint);
		break;
	case GF_PROP_4CC:
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
		gsfmx_write_vlen(ctx, p->value.frac.num);
		gsfmx_write_vlen(ctx, p->value.frac.den);
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
		gsfmx_write_vlen(ctx, p->value.vec2i.x);
		gsfmx_write_vlen(ctx, p->value.vec2i.y);
		break;
	case GF_PROP_VEC2:
		gf_bs_write_double(ctx->bs_w, p->value.vec2.x);
		gf_bs_write_double(ctx->bs_w, p->value.vec2.y);
		break;
	case GF_PROP_VEC3I:
		gsfmx_write_vlen(ctx, p->value.vec3i.x);
		gsfmx_write_vlen(ctx, p->value.vec3i.y);
		gsfmx_write_vlen(ctx, p->value.vec3i.z);
		break;
	case GF_PROP_VEC4I:
		gsfmx_write_vlen(ctx, p->value.vec4i.x);
		gsfmx_write_vlen(ctx, p->value.vec4i.y);
		gsfmx_write_vlen(ctx, p->value.vec4i.z);
		gsfmx_write_vlen(ctx, p->value.vec4i.w);
		break;
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
	case GF_PROP_NAME:
		len = (u32) strlen(p->value.string);
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
		len2 = p->value.string_list.nb_items;
		gsfmx_write_vlen(ctx, len2);
		for (i=0; i<len2; i++) {
			const char *str = p->value.string_list.vals[i];
			len = (u32) strlen(str);
			gsfmx_write_vlen(ctx, len);
			gf_bs_write_data(ctx->bs_w, str, len);
		}
		break;

	case GF_PROP_UINT_LIST:
	case GF_PROP_SINT_LIST:
		len = p->value.uint_list.nb_items;
		gsfmx_write_vlen(ctx, len);
		for (i=0; i<len; i++) {
			gsfmx_write_vlen(ctx, p->value.uint_list.vals[i] );
		}
		break;
	case GF_PROP_4CC_LIST:
		len = p->value.uint_list.nb_items;
		gsfmx_write_vlen(ctx, len);
		for (i=0; i<len; i++) {
			gf_bs_write_u32(ctx->bs_w, p->value.uint_list.vals[i] );
		}
		break;
	case GF_PROP_VEC2I_LIST:
		len = p->value.v2i_list.nb_items;
		gsfmx_write_vlen(ctx, len);
		for (i=0; i<len; i++) {
			gsfmx_write_vlen(ctx, p->value.v2i_list.vals[i].x );
			gsfmx_write_vlen(ctx, p->value.v2i_list.vals[i].y );
		}
		break;
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFMux] Cannot serialize pointer property, ignoring !!\n"));
		break;
	default:
		if (gf_props_type_is_enum(p->type)) {
			gsfmx_write_vlen(ctx, p->value.uint);
			break;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Cannot serialize property of unknown type, ignoring !!\n"));
		break;
	}
}

static GFINLINE Bool gsfmx_is_prop_skip(GSFMxCtx *ctx, u32 prop_4cc, const char *prop_name, u8 sep_l)
{
	if (prop_name && !strcmp(prop_name, "reframer_rem_edits"))
		return GF_TRUE;

	if (ctx->minp) {
		u8 flags;
		if (prop_name) return GF_TRUE;

		flags = gf_props_4cc_get_flags(prop_4cc);
		if (flags & GF_PROP_FLAG_GSF_REM) return GF_TRUE;
		return GF_FALSE;
	}
	if (ctx->skp) {
		const char *pname = prop_name ? prop_name : gf_4cc_to_str(prop_4cc);
		u32 plen = (u32) strlen(pname);
		const char *sep = strstr(ctx->skp, pname);
		if (sep && ((sep[plen]==sep_l) || !sep[plen]))
			return GF_TRUE;
		if (prop_4cc) {
			pname = gf_props_4cc_get_name(prop_4cc);
			if (!pname) pname = gf_4cc_to_str(prop_4cc);
			plen = (u32) strlen(pname);
			sep = strstr(ctx->skp, pname);
			if (sep && ((sep[plen]==sep_l) || !sep[plen]))
				return GF_TRUE;
		}
	}
	return GF_FALSE;
}

static void gsfmx_write_pid_config(GF_Filter *filter, GSFMxCtx *ctx, GSFStream *gst)
{
	const char *force_fext=NULL;
	const char *force_mime=NULL;
	const char *force_url=NULL;
	u32 nb_4cc_props=0;
	u32 nb_str_props=0;
	u32 idx=0;
	u8 sep_l = gf_filter_get_sep(filter, GF_FS_SEP_LIST);

	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pid_enum_properties(gst->pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (!gsfmx_can_serialize_prop(p, prop_4cc)) continue;
		if ( gsfmx_is_prop_skip(ctx, prop_4cc, prop_name, sep_l) )
			continue;
		if (prop_4cc) {
			if (gf_props_4cc_get_type(prop_4cc) == GF_PROP_FORBIDEN) {
				nb_str_props++;
			}
			//file, only send mime, url, ext and streamtype
			else if (!gst->is_file) {
				if (prop_4cc != GF_PROP_PID_MUX_SRC)
					nb_4cc_props++;
			}
		}
		else if (prop_name)
			nb_str_props++;
	}

	//file, send mime, url, ext and streamtype
	if (gst->is_file) {
		GSFMxCtx *alias_ctx = gf_filter_pid_get_alias_udta(gst->pid);
		if (alias_ctx) {
			force_fext = gf_file_ext_start(alias_ctx->dst);
			if (force_fext) force_fext++;
			force_url = alias_ctx->dst ? alias_ctx->dst : NULL;
			force_mime = alias_ctx->mime;
		} else {
			const GF_PropertyValue *p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_FILE_EXT);
			force_fext = (p && p->value.string) ? p->value.string : ctx->ext;
			p = gf_filter_pid_get_property(gst->pid, GF_PROP_PID_MIME);
			force_mime = (p && p->value.string) ? p->value.string : ctx->mime;
			force_url = ctx->dst;
			if (!force_url) force_url = "file";
		}
		nb_4cc_props++;
		if (force_url)
			nb_4cc_props++;
		if (force_mime)
			nb_4cc_props++;
		if (force_fext)
			nb_4cc_props++;
	}
	gf_bs_write_u8(ctx->bs_w, gst->config_version);
	gsfmx_write_vlen(ctx, nb_4cc_props);
	gsfmx_write_vlen(ctx, nb_str_props);

	if (gst->is_file) {
		GF_PropertyValue prop;

		gf_bs_write_u32(ctx->bs_w, GF_PROP_PID_STREAM_TYPE);
		prop.type = GF_PROP_UINT;
		prop.value.uint = GF_STREAM_FILE;
		gsfmx_write_prop(ctx, &prop);

		if (force_url) {
			gf_bs_write_u32(ctx->bs_w, GF_PROP_PID_URL);
			prop.type = GF_PROP_STRING;
			prop.value.string = (char *) force_url;
			gsfmx_write_prop(ctx, &prop);
		}

		if (force_mime) {
			gf_bs_write_u32(ctx->bs_w, GF_PROP_PID_MIME);
			prop.type = GF_PROP_STRING;
			prop.value.string = (char *) force_mime;
			gsfmx_write_prop(ctx, &prop);
		}
		if (force_fext) {
			gf_bs_write_u32(ctx->bs_w, GF_PROP_PID_FILE_EXT);
			prop.type = GF_PROP_STRING;
			prop.value.string = (char *) force_fext;
			gsfmx_write_prop(ctx, &prop);
		}
	}


	idx=0;
	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pid_enum_properties(gst->pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (!gsfmx_can_serialize_prop(p, prop_4cc)) continue;

		if (prop_name) continue;
		if (gf_props_4cc_get_type(prop_4cc) == GF_PROP_FORBIDEN)
			continue;

		if ( gsfmx_is_prop_skip(ctx, prop_4cc, prop_name, sep_l) )
			continue;

		if (gst->is_file) {
			continue;
		}
		if (prop_4cc == GF_PROP_PID_MUX_SRC)
			continue;

		gf_bs_write_u32(ctx->bs_w, prop_4cc);

		gsfmx_write_prop(ctx, p);

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG)) {
			char dump[GF_PROP_DUMP_ARG_SIZE];
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFMux] Write pid %d %s property to %s\n", gst->idx, gf_props_4cc_get_name(prop_4cc), gf_props_dump(prop_4cc, p, dump, GF_PROP_DUMP_DATA_NONE) ) );
		}
#endif
	}

	idx=0;
	while (1) {
		u32 prop_4cc, len;
		const char *prop_name;
		const GF_PropertyValue *p = gf_filter_pid_enum_properties(gst->pid, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (!gsfmx_can_serialize_prop(p, prop_4cc)) continue;
		if (prop_4cc && (gf_props_4cc_get_type(prop_4cc) != GF_PROP_FORBIDEN)) continue;

		if ( gsfmx_is_prop_skip(ctx, prop_4cc, prop_name, sep_l) )
			continue;
		if (!prop_name)
			prop_name = gf_4cc_to_str(prop_4cc);

		len = (u32) strlen(prop_name);
		gsfmx_write_vlen(ctx, len);
		gf_bs_write_data(ctx->bs_w, prop_name, len);

		gf_bs_write_u8(ctx->bs_w, p->type);
		gsfmx_write_prop(ctx, p);

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG)) {
			char dump[GF_PROP_DUMP_ARG_SIZE];
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFMux] Write pid %d %s property to %s\n", gst->idx, prop_name ? prop_name : gf_props_4cc_get_name(prop_4cc), gf_props_dump(prop_4cc, p, dump, GF_PROP_DUMP_DATA_NONE) ) );
		}
#endif
	}
}



static void gsfmx_send_header(GF_Filter *filter, GSFMxCtx *ctx, Bool is_carousel_update)
{
	u32 mlen=0;

	if (!ctx->bs_w) {
 		ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		if (!ctx->bs_w) return;
	} else {
		gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	}

	ctx->nb_frames++;
	if (ctx->magic) {
		mlen = (u32) strlen(ctx->magic);
	}

	//header:signature
	gf_bs_write_u32(ctx->bs_w, GF_4CC('G','S','5','F') );
	//header protocol version
	gf_bs_write_u8(ctx->bs_w, GF_GSF_VERSION);

	if (ctx->crypt) {
		gf_bs_write_data(ctx->bs_w, ctx->crypt_IV, 16);
		gf_bs_write_u16(ctx->bs_w, ctx->pattern.num);
		gf_bs_write_u16(ctx->bs_w, ctx->pattern.den);
	}
	gf_bs_write_int(ctx->bs_w, ctx->sigsn ? 1 : 0, 1);
	gf_bs_write_int(ctx->bs_w, 0, 7);

	//header:magic
	gsfmx_write_vlen(ctx, mlen);
	if (ctx->magic) {
		gf_bs_write_data(ctx->bs_w, ctx->magic, mlen);
	}

	gsfmx_send_packets(ctx, NULL, GFS_PCKTYPE_HDR, GF_FALSE, is_carousel_update ? GF_TRUE : GF_FALSE, 0, 0);
	ctx->is_start = GF_FALSE;
}

static void gsfmx_send_pid_config(GF_Filter *filter, GSFMxCtx *ctx, GSFStream *gst, Bool is_info, Bool is_carousel_update)
{
	if (ctx->is_start) {
		gsfmx_send_header(filter, ctx, GF_FALSE);
	}

	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);
	if (!is_info && !is_carousel_update) {
		gst->config_version++;
	}
	gsfmx_write_pid_config(filter, ctx, gst);
	gst->nb_frames++;
	gsfmx_send_packets(ctx, gst, is_info ? GFS_PCKTYPE_PID_INFO_UPDATE : GFS_PCKTYPE_PID_CONFIG, GF_FALSE, is_carousel_update ? GF_TRUE : GF_FALSE, 0, 0);
}

static void gsfmx_write_data_packet(GSFMxCtx *ctx, GSFStream *gst, GF_FilterPacket *pck)
{
	u32 w=0, h=0, stride=0, stride_uv=0, pf=0;
	u32 nb_planes=0, uv_height=0;
	const char *data;
	u32 frame_size, frame_hdr_size;
	GF_FilterFrameInterface *frame_ifce=NULL;
	u32 nb_4cc_props=0;
	u32 nb_str_props=0;
	u32 idx=0;
	u32 tsmode=0;
	u32 tsmodebits=0;
	u32 tsdiffmode=0;
	u32 tsdiffmodebits=0;
	Bool start, end;
	const GF_PropertyValue *p;

	if (ctx->dbg==2) return;

	gst->nb_frames++;

	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (!gsfmx_can_serialize_prop(p, prop_4cc)) continue;
		if (prop_4cc) {
			if (gf_props_4cc_get_type(prop_4cc) == GF_PROP_FORBIDEN)
				nb_str_props++;
			else
				nb_4cc_props++;
		}
		else if (prop_name)
			nb_str_props++;
	}

	gf_bs_reassign_buffer(ctx->bs_w, ctx->buffer, ctx->alloc_size);

	data = gf_filter_pck_get_data(pck, &frame_size);
	if (!data) {
		frame_ifce = gf_filter_pck_get_frame_interface(pck);
		if (frame_ifce) {
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

			if (! gf_pixel_get_size_info(pf, w, h, &frame_size, &stride, &stride_uv, &nb_planes, &uv_height)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFMux] Raw packet with unsupported pixel format, cannot send\n"));
				return;
			}
		}
	}

	if (ctx->dbg) frame_size=0;

	u64 dts = gf_filter_pck_get_dts(pck);
	u64 cts = gf_filter_pck_get_cts(pck);
	u32 cts_diff=0;
	u8 has_dts=0;
	u8 has_cts=0;
	u8 neg_cts=0;

	//packet structure:
	//flags first byte: 1(has_dts) 1(has_cts) 1(has_dur) 1(cts_diff_neg) 2(ts_mode) 2(ts_diff_mode)
	//flags second byte: 3(sap) 2(encrypted) 1(has_sample_deps) 1(has builtin props) 1(has_ext)
	//if (ext) {
	// 	flags third byte: 1(has_byteoffset) 1(corrupted) 1(seek) 1(has_carousel) 2(ilaced) 2(cktype)
	// 	flags fourth byte: 1(au start) 1(au end) 1(has props) reserved(5)
	//}
	//if has_dts, dts on tsmodebits
	//if has_cts, has_dts ? cts_diff on ts_diff_mode : cts on tsmodebits
	//if durmode>0, dur on ts_diff_mode
	//if sap==4, roll on signed 16 bits
	//if (has sample deps) sample_deps_flags on 8 bits
	//if has_carousel, carousel version on 8 bits
	//if has_byteoffset, byte offset on 64 bits
	//if (has builtin) vlen nb builtin_props then props[builtin_props]
	//if (has props) vlen nb_str_props then props[nb_str_props]
	//data


	u32 duration = ctx->sigdur ? gf_filter_pck_get_duration(pck) : 0;

	if (ctx->sigdts && (dts!=GF_FILTER_NO_TS) && (dts!=cts)) {
		has_dts=1;

		if (dts>0xFFFFFFFFUL) tsmode=3;
		else if (dts>0xFFFFFF) tsmode=2;
		else if (dts>0xFFFF) tsmode=1;
	}
	if (cts!=GF_FILTER_NO_TS) {
		has_cts=1;
		if (has_dts) {
			s64 diff = ( (s64) cts - (s64) dts);
			if (diff>0) cts_diff = (u32) diff;
			else {
				neg_cts=1;
				cts_diff = (u32) (-diff);
			}
		} else {
			if (cts>0xFFFFFFFFUL) tsmode=3;
			else if (cts>0xFFFFFF) tsmode=2;
			else if (cts>0xFFFF) tsmode=1;
		}
	}

	if (tsmode==3) tsmodebits=64;
	else if (tsmode==2) tsmodebits=32;
	else if (tsmode==1) tsmodebits=24;
	else tsmodebits=16;

	u32 max_dur = MAX(duration, cts_diff);
	if (max_dur < 0xFF) { tsdiffmode=0; tsdiffmodebits=8; }
	else if (max_dur < 0xFFFF) { tsdiffmode=1; tsdiffmodebits=16; }
	else if (max_dur < 0xFFFFFF) { tsdiffmode=2; tsdiffmodebits=24; }
	else { tsdiffmode=3; tsdiffmodebits=32; }


	assert(tsmodebits<=32);

	//first flags byte
	//flags first byte: 1(has_dts) 1(has_cts) 1(has_dur) 1(cts_diff_neg) 2(ts_mode) 2(ts_diff_mode)
	gf_bs_write_int(ctx->bs_w, has_dts, 1);
	gf_bs_write_int(ctx->bs_w, has_cts, 1);
	gf_bs_write_int(ctx->bs_w, duration ? 1 : 0, 1);
	gf_bs_write_int(ctx->bs_w, neg_cts, 1);
	gf_bs_write_int(ctx->bs_w, tsmode, 2);
	gf_bs_write_int(ctx->bs_w, tsdiffmode, 2);

	//flags second byte: 3(sap) 2(encrypted) 1(has_sample_deps) 1(has builtin props) 1(has_ext)
	u8 sap = gf_filter_pck_get_sap(pck);
	gf_bs_write_int(ctx->bs_w, sap, 3);
	u8 crypt = gf_filter_pck_get_crypt_flags(pck);
	gf_bs_write_int(ctx->bs_w, crypt, 2);
	u8 depflags = gf_filter_pck_get_dependency_flags(pck);
	gf_bs_write_int(ctx->bs_w, depflags ? 1 : 0, 1);
	gf_bs_write_int(ctx->bs_w, nb_4cc_props ? 1 : 0, 1);

	u8 needs_ext = nb_str_props ? 1 : 0;
	u64 bo = ctx->sigbo ? gf_filter_pck_get_byte_offset(pck) : GF_FILTER_NO_BO;
	if (bo != GF_FILTER_NO_BO) needs_ext=1;
	u8 corr = gf_filter_pck_get_corrupted(pck);
	if (corr) needs_ext=1;
	u8 seek = gf_filter_pck_get_seek_flag(pck);
	if (seek) needs_ext=1;
	u8 carv = gf_filter_pck_get_carousel_version(pck);
	if (carv) needs_ext=1;
	u8 interl = gf_filter_pck_get_interlaced(pck);
	if (interl) needs_ext=1;
	u8 cktype = gf_filter_pck_get_clock_type(pck);
	if (cktype) needs_ext=1;
	gf_filter_pck_get_framing(pck, &start, &end);
	if (!start || !end) needs_ext=1;

	gf_bs_write_int(ctx->bs_w, needs_ext, 1);

	//extension header
	if (needs_ext) {
		// 	flags third byte: 1(has_byteoffset) 1(corrupted) 1(seek) 1(has_carousel) 2(ilaced) 2(cktype)
		gf_bs_write_int(ctx->bs_w, (bo==GF_FILTER_NO_BO) ? 0 : 1, 1);
		gf_bs_write_int(ctx->bs_w, corr ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, seek ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, carv ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, interl, 2);
		gf_bs_write_int(ctx->bs_w, cktype, 2);

		// 	flags fourth byte: 1(au start) 1(au end) 1(has props) reserved(5)
		gf_bs_write_int(ctx->bs_w, start ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, end ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, nb_str_props ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, 0, 5);
	}
	//done with flags

	if (has_dts) {
		if (tsmode==3) gf_bs_write_long_int(ctx->bs_w, dts, tsmodebits);
		else gf_bs_write_int(ctx->bs_w, (u32) dts, tsmodebits);
	}
	if (has_cts) {
		if (has_dts) {
			gf_bs_write_int(ctx->bs_w, cts_diff, tsdiffmodebits);
		} else {
			if (tsmode==3) gf_bs_write_long_int(ctx->bs_w, cts, tsmodebits);
			else gf_bs_write_int(ctx->bs_w, (u32) cts, tsmodebits);
		}
	}

	if (duration) {
		gf_bs_write_int(ctx->bs_w, duration, tsdiffmodebits);
	}

	if ((sap==GF_FILTER_SAP_4) || (sap==GF_FILTER_SAP_4_PROL)) {
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
			if (!gsfmx_can_serialize_prop(p, prop_4cc)) continue;
			if (prop_name) continue;
			if (gf_props_4cc_get_type(prop_4cc) == GF_PROP_FORBIDEN) continue;

			gf_bs_write_u32(ctx->bs_w, prop_4cc);

			gsfmx_write_prop(ctx, p);
		}
	}
	if (nb_str_props) {
		gsfmx_write_vlen(ctx, nb_str_props);
		idx=0;
		while (1) {
			u32 prop_4cc, len;
			const char *prop_name;
			p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			if (!gsfmx_can_serialize_prop(p, prop_4cc)) continue;
			if (prop_4cc && (gf_props_4cc_get_type(prop_4cc) != GF_PROP_FORBIDEN)) continue;
			if (!prop_name) prop_name = gf_4cc_to_str(prop_4cc);
			len = (u32) strlen(prop_name);
			gsfmx_write_vlen(ctx, len);
			gf_bs_write_data(ctx->bs_w, prop_name, len);

			gf_bs_write_u8(ctx->bs_w, p->type);
			gsfmx_write_prop(ctx, p);
		}
		gsfmx_write_vlen(ctx, nb_str_props);
	}

	frame_hdr_size = (u32) gf_bs_get_position(ctx->bs_w);

	if (has_cts && ctx->crate>0) {
		if (!gst->last_cts_config) {
			gst->last_cts_config = has_cts+1;
		} else if ( cts - (gst->last_cts_config-1) > ctx->crate * gst->timescale) {
			ctx->regenerate_tunein_info = GF_TRUE;
		}
	}


	//write packet data
	if (ctx->dbg) {

	} else if (data) {
		u32 nb_write = gf_bs_write_data(ctx->bs_w, data, frame_size);
		if (nb_write != frame_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Write error, wrote %d bytes but had %d to write\n", nb_write, frame_size));
		}
		gsfmx_send_packets(ctx, gst, GFS_PCKTYPE_PCK, GF_FALSE, GF_FALSE, frame_size, frame_hdr_size);
	} else if (frame_ifce) {
		u32 i, bpp = gf_pixel_get_bytes_per_pixel(pf);
		for (i=0; i<nb_planes; i++) {
			u32 j, write_h, lsize;
			const u8 *out_ptr;
			u32 out_stride = i ? stride_uv : stride;
			GF_Err e = frame_ifce->get_plane(frame_ifce, i, &out_ptr, &out_stride);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Failed to fetch plane data from hardware frame, cannot write\n"));
				break;
			}
			write_h = h;
			if (i) write_h = uv_height;
			lsize = bpp * (i ? stride_uv : stride);
			for (j=0; j<write_h; j++) {
				u32 nb_write = (u32) gf_bs_write_data(ctx->bs_w, out_ptr, lsize);
				if (nb_write != lsize) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Write error, wrote %d bytes but had %d to write\n", nb_write, lsize));
				}
				out_ptr += out_stride;
			}
		}
		gsfmx_send_packets(ctx, gst, GFS_PCKTYPE_PCK, GF_FALSE, GF_FALSE, frame_size, frame_hdr_size);
	}
}

GF_Err gsfmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GSFStream *gst=NULL;
	const GF_PropertyValue *p;
	GSFMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		gst = gf_filter_pid_get_udta(pid);
		if (!gst) return GF_OK;

		gsfmx_send_pid_rem(ctx, gst);
		gf_list_del_item(ctx->streams, gst);
		gf_free(gst);
		if (!gf_list_count(ctx->streams) && ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_name(ctx->opid, "gsf_mux");
		//copy properties at init or reconfig
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT( GF_STREAM_FILE ) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ENCRYPTED, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING( "*" ) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/x-gpac-sf") );
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);

	gst = gf_filter_pid_get_udta(pid);
	if (!gst) {
		GF_SAFEALLOC(gst, GSFStream);
		if (!gst) return GF_OUT_OF_MEM;
		gf_list_add(ctx->streams, gst);
		gst->pid = pid;
		gf_filter_pid_set_udta(pid, gst);
		gst->idx = 1+ctx->max_pid_idx;
		ctx->max_pid_idx++;
		gst->timescale = 1000;
		if (p && p->value.uint) gst->timescale = p->value.uint;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		if (p && (p->value.uint==GF_STREAM_FILE)) gst->is_file = GF_TRUE;
	} else {
		if (p && p->value.uint) gst->timescale = p->value.uint;
		gst->last_cts_config = 0;
	}
	gsfmx_send_pid_config(filter, ctx, gst, GF_FALSE, GF_FALSE);
	return GF_OK;
}


GF_Err gsfmx_process(GF_Filter *filter)
{
	GSFMxCtx *ctx = gf_filter_get_udta(filter);
	u32 i, nb_eos, count = gf_list_count(ctx->streams);

	nb_eos = 0;
	for (i=0; i<count; i++) {
		GSFStream *gst = gf_list_get(ctx->streams, i);
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
		gsfmx_write_data_packet(ctx, gst, pck);
		gf_filter_pid_drop_packet(gst->pid);
	}
	if (ctx->regenerate_tunein_info) {
		ctx->regenerate_tunein_info = GF_FALSE;
		gsfmx_send_header(filter, ctx, GF_TRUE);
		for (i=0; i<count; i++) {
			GSFStream *gst = gf_list_get(ctx->streams, i);
			gsfmx_send_pid_config(filter, ctx, gst, GF_FALSE, GF_TRUE);
		}
	}

	if (count && (nb_eos==count) ) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	return GF_OK;
}

static Bool gsfmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GSFMxCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type==GF_FEVT_INFO_UPDATE) {
		GSFStream *gst = gf_filter_pid_get_udta(evt->base.on_pid);
		if (gst)
			gsfmx_send_pid_config(filter, ctx, gst, GF_TRUE, GF_FALSE);
	}
	//don't cancel events
	return GF_FALSE;
}





static GF_Err gsfmx_initialize(GF_Filter *filter)
{
	GSFMxCtx *ctx = gf_filter_get_udta(filter);
	const char *ext = ctx->ext;

	if (!ext && ctx->dst) {
		ext = gf_file_ext_start(ctx->dst);
		if (ext) ext++;
	}

	if (!gf_filter_is_dynamic(filter) && (ext || ctx->mime)) {
		ctx->caps[0].code =	GF_PROP_PID_STREAM_TYPE;
		ctx->caps[0].val = PROP_UINT(GF_STREAM_FILE);
		ctx->caps[0].flags = GF_CAPS_INPUT_OUTPUT;

		ctx->caps[1].code =	ctx->mime ? GF_PROP_PID_MIME : GF_PROP_PID_FILE_EXT;
		ctx->caps[1].val = ctx->mime ? PROP_STRING(ctx->mime) : PROP_STRING(ext);
		ctx->caps[1].flags = GF_CAPS_INPUT;

		ctx->caps[2].code =	GF_PROP_PID_FILE_EXT;
		ctx->caps[2].val = PROP_STRING("gsf");
		ctx->caps[2].flags = GF_CAPS_OUTPUT;

		ctx->caps[3].code =	GF_PROP_PID_MIME;
		ctx->caps[3].val = PROP_STRING("application/x-gpac-sf");
		ctx->caps[3].flags = GF_CAPS_OUTPUT;

		gf_filter_override_caps(filter, ctx->caps, 4);

		if (gf_filter_is_alias(filter) && ctx->mixed) {
			ctx->caps[0].code =	GF_PROP_PID_STREAM_TYPE;
			ctx->caps[0].val = PROP_UINT(GF_STREAM_UNKNOWN);
			ctx->caps[0].flags = GF_CAPS_INPUT_EXCLUDED;
			return GF_OK;
		}

		gf_filter_act_as_sink(filter);
		ctx->filemode = GF_TRUE;
	}
	if (gf_filter_is_temporary(filter))
		return GF_OK;


	gf_rand_init(GF_FALSE);

	if (ctx->key.size==16) {
#ifdef GPAC_DISABLE_CRYPTO
		return GF_NOT_SUPPORTED;
#else
		GF_Err e;
		if (ctx->IV.size==16) {
			memcpy(ctx->crypt_IV, ctx->IV.ptr, 16);
		} else if (ctx->IV.size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFMux] Wrong IV value, size %d expecting 16\n", ctx->key.size));
			return GF_BAD_PARAM;
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
#endif
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
	GSFMxCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->streams)) {
		GSFStream *gst = gf_list_pop_back(ctx->streams);
		gf_free(gst);
	}
	gf_list_del(ctx->streams);

	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->buffer) gf_free(ctx->buffer);
#ifndef GPAC_DISABLE_CRYPTO
	if (ctx->crypt) gf_crypt_close(ctx->crypt);
#endif
}

static Bool gsfmx_use_alias(GF_Filter *filter, const char *url, const char *mime)
{
	GSFMxCtx *ctx = gf_filter_get_udta(filter);
	return ctx->filemode;
}

static const GF_FilterCapability GSFMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "gsf"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "application/x-gpac-sf"),
};



#define OFFS(_n)	#_n, offsetof(GSFMxCtx, _n)
static const GF_FilterArgs GSFMxArgs[] =
{
	{ OFFS(sigsn), "signal packet sequence number after header field and before size field. Sequence number is per PID, encoded on 16 bits. Header packet does not have a SN", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(sigdur), "signal duration", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sigbo), "signal byte offset", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sigdts), "signal decoding timestamp", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dbg), "set debug mode\n"
	"- no: disable debug\n"
	"- nodata: force packet size to 0\n"
	"- nopck: skip packet", GF_PROP_UINT, "no", "no|nodata|nopck", GF_FS_ARG_HINT_EXPERT},
#ifndef GPAC_DISABLE_CRYPTO
	{ OFFS(key), "encrypt packets using given key", GF_PROP_DATA, NULL, NULL, 0},
	{ OFFS(IV), "set IV for encryption - a constant IV is used to keep packet overhead small (cbcs-like)", GF_PROP_DATA, NULL, NULL, 0},
	{ OFFS(pattern), "set nb_crypt / nb_skip block pattern. default is all encrypted", GF_PROP_FRACTION, "1/0", NULL, GF_FS_ARG_HINT_ADVANCED},
#endif // GPAC_DISABLE_CRYPTO
	{ OFFS(mpck), "set max packet size. 0 means no fragmentation (each AU is sent in one packet)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(magic), "magic string to append in setup packet", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(skp), "comma separated list of PID property names to skip", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(minp), "include only the minimum set of properties required for stream processing", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(crate), "carousel period for tune-in info in seconds", GF_PROP_DOUBLE, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ext), "file extension for file mode", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dst), "target URL in file mode", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_SINK_ALIAS},
	{ OFFS(mime), "file mime for file mode", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{ OFFS(mixed), "allow GSF to contain both files and media streams", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_SINK_ALIAS},

	{0}
};


GF_FilterRegister GSFMxRegister = {
	.name = "gsfmx",
	GF_FS_SET_DESCRIPTION("GSF Multiplexer")
#ifndef GPAC_DISABLE_DOC
	.help = "This filter provides GSF (__GPAC Serialized Format__) multiplexing.\n"
			"It serializes the stream states (config/reconfig/info update/remove/eos) and packets of input PIDs. "
			"This allows either saving to file a session, or forwarding the state/data of streams to another instance of GPAC "
			"using either pipes or sockets. Upstream events are not serialized.\n"
			"\n"
			"The default behavior does not insert sequence numbers. When running over general protocols not ensuring packet order, this should be inserted.\n"
			"The serializer sends tune-in packets (global and per PID) at the requested carousel rate - if 0, no carousel. These packets are marked as redundant so that they can be discarded by output filters if needed.\n"
			"\n"
#ifndef GPAC_DISABLE_CRYPTO
			"# Encryption\n"
			"The stream format can be encrypted in AES 128 CBC mode. For all packets, the packet header (header, size, frame size/block offset and optional seq num) are in the clear "
			"and the following bytes until the last byte of the last multiple of block size (16) fitting in the payload are encrypted.\n"
			"For data packets, each fragment is encrypted individually to avoid error propagation in case of losses.\n"
			"For other packets, the entire packet is encrypted before fragmentation (fragments cannot be processed individually).\n"
			"For header/tunein packets, the first 25 bytes after the header are in the clear (signature,version,IV and pattern).\n"
			"The [-IV]() is constant to avoid packet overhead, randomly generated if not set and sent in the initial stream header. "
			"Pattern mode can be used (cf CENC cbcs) to encrypt K block and leave N blocks in the clear.\n"
			"\n"
#endif
			"# Filtering properties\n"
			"The header/tunein packet may get quite big when all PID properties are kept. In order to help reduce its size, the [-minp]() option can be used: "
			"this will remove all built-in properties marked as droppable (cf property help) as well as all non built-in properties.\n"
			"The [-skp]() option may also be used to specify which property to drop:\n"
			"EX skp=\"4CC1,Name2\n"
			"This will remove properties of type `4CC1` and properties (built-in or not) of name `Name2`.\n"
			"\n"
			"# File mode\n"
			"By default the filter only accepts framed media streams as input PID, not files. This can be changed by explicitly loading the filter with [-ext]() or [-dst]() set.\n"
			"EX gpac -i source.mp4 gsfmx:dst=manifest.mpd -o dump.gsf\n"
			"This will DASH the source and store every files produced as PIDs in the GSF mux.\n"
			"In order to demultiplex such a file, the `gsfdmx`filter will likely need to be explicitly loaded:\n"
			"EX gpac -i mux.gsf gsfdmx -o dump/$File$:dynext:clone\n"
			"This will extract all files from the GSF mux.\n"
			"\n"
			"By default when working in file mode, the filter only accepts PIDs of type `file` as input.\n"
			"To allow a mix of files and streams, use [-mixed]():\n"
			"EX gpac -i source.mp4 gsfmx:dst=manifest.mpd:mixed -o dump.gsf\n"
			"This will DASH the source, store the manifest file and the media streams with their packet properties in the GSF mux.\n"
		,
#endif
	.private_size = sizeof(GSFMxCtx),
	.max_extra_pids = (u32) -1,
	.args = GSFMxArgs,
	.flags = GF_FS_REG_DYNAMIC_REDIRECT | GF_FS_REG_TEMP_INIT,
	SETCAPS(GSFMxCaps),
	.initialize = gsfmx_initialize,
	.finalize = gsfmx_finalize,
	.configure_pid = gsfmx_configure_pid,
	.process = gsfmx_process,
	.process_event = gsfmx_process_event,
	.use_alias = gsfmx_use_alias
};


const GF_FilterRegister *gsfmx_register(GF_FilterSession *session)
{
	return &GSFMxRegister;
}
