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
	GF_FilterPid *opid;

	u32 idx;
	u16 nb_pck;
} GF_GSFStream;

typedef struct
{
	//opts
	const char *magic;
	GF_PropData key;


	//only one output pid declared
	GF_FilterPid *ipid;

	GF_List *streams;

	bin128 crypt_IV;
	GF_Crypt *crypt;
	u16 crypt_blocks, skip_blocks;

	u32 nb_playing;
	Double start_range;
	Bool is_file;
	u64 file_pos, file_size;
	GF_Fraction duration;
	Bool initial_play_done;

	GF_BitStream *bs_r;

	char *buffer;
	u32 alloc_size, buf_size;

	u32 missing_bytes;
	Bool tuned;
	Bool tune_error;
	Bool wait_for_play;
} GF_GSFDemuxCtx;


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
	GFS_PCKTYPE_UNDEF8
	/*NO MORE PACKET TYPE AVAILABLE*/
} GF_GSFPacketType;



GF_Err gsfdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_GSFDemuxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		GF_GSFStream *st;
		ctx->ipid = NULL;

		while (gf_list_count(ctx->streams)) {
			st = gf_list_pop_back(ctx->streams);
			if (st->opid)
				gf_filter_pid_remove(st->opid);
			gf_free(st);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	return GF_OK;
}

static Bool gsfdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterEvent fevt;
	GF_GSFDemuxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->nb_playing && (ctx->start_range == evt->play.start_range)) {
			return GF_TRUE;
		}
		ctx->nb_playing++;
		ctx->wait_for_play = GF_FALSE;

		if (! ctx->is_file) {
			return GF_FALSE;
		}
//		safdmx_check_dur(ctx);

		ctx->start_range = evt->play.start_range;
		ctx->file_pos = 0;
		if (ctx->duration.num) {
			ctx->file_pos = (u64) (ctx->file_size * ctx->start_range);
			ctx->file_pos *= ctx->duration.den;
			ctx->file_pos /= ctx->duration.num;
			if (ctx->file_pos>ctx->file_size) return GF_TRUE;
		}

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos)
				return GF_TRUE;
		}
		//post a seek to 0 - we would need to build an index of AUs to find the next place to seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = 0;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->nb_playing--;
		//don't cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static void gsfdmx_decrypt(GF_GSFDemuxCtx *ctx, u32 bytes_in_packets)
{
	u32 pos = (u32) gf_bs_get_position(ctx->bs_r);
	u32 clear_tail = bytes_in_packets%16;
	u32 bytes_crypted = bytes_in_packets - clear_tail;

	gf_crypt_set_IV(ctx->crypt, ctx->crypt_IV, 16);
	if (ctx->crypt_blocks && ctx->skip_blocks) {
		u32 pattern_length = 16 * (ctx->crypt_blocks + ctx->skip_blocks);
		u32 cryp_len = 16 * ctx->crypt_blocks;
		while (bytes_crypted) {
			gf_crypt_decrypt(ctx->crypt, ctx->buffer+pos, bytes_crypted >= cryp_len ? cryp_len : bytes_crypted);
			if (bytes_crypted >= pattern_length) {
				pos += pattern_length;
				bytes_crypted -= pattern_length;
			} else {
				bytes_crypted = 0;
			}
		}
	} else {
		gf_crypt_decrypt(ctx->crypt, ctx->buffer+pos, bytes_crypted);
	}
}

static GFINLINE u32 gsfdmx_read_vlen(GF_GSFDemuxCtx *ctx)
{
	if (gf_bs_read_int(ctx->bs_r, 1)) {
		return gf_bs_read_int(ctx->bs_r, 31);
	} else {
		return gf_bs_read_int(ctx->bs_r, 7);
	}
}

static GF_Err gsfdmx_read_prop(GF_GSFDemuxCtx *ctx, GF_PropertyValue *p)
{
	u32 len, len2, i;

	switch (p->type) {
	case GF_PROP_SINT:
	case GF_PROP_UINT:
	case GF_PROP_PIXFMT:
	case GF_PROP_PCMFMT:
		p->value.uint = gf_bs_read_u32(ctx->bs_r);
		break;
	case GF_PROP_LSINT:
	case GF_PROP_LUINT:
		p->value.longuint = gf_bs_read_u64(ctx->bs_r);
		break;
	case GF_PROP_BOOL:
		p->value.boolean = gf_bs_read_u8(ctx->bs_r) ? 1 : 0;
		break;
	case GF_PROP_FRACTION:
		p->value.frac.num = gf_bs_read_u32(ctx->bs_r);
		p->value.frac.den = gf_bs_read_u32(ctx->bs_r);
		break;
	case GF_PROP_FRACTION64:
		p->value.lfrac.num = gf_bs_read_u64(ctx->bs_r);
		p->value.lfrac.den = gf_bs_read_u64(ctx->bs_r);
		break;
	case GF_PROP_FLOAT:
		p->value.fnumber = FLT2FIX( gf_bs_read_float(ctx->bs_r) );
		break;
	case GF_PROP_DOUBLE:
		p->value.number = gf_bs_read_double(ctx->bs_r);
		break;
	case GF_PROP_VEC2I:
		p->value.vec2i.x = gf_bs_read_u32(ctx->bs_r);
		p->value.vec2i.y = gf_bs_read_u32(ctx->bs_r);
		break;
	case GF_PROP_VEC2:
		p->value.vec2.x = gf_bs_read_double(ctx->bs_r);
		p->value.vec2.y = gf_bs_read_double(ctx->bs_r);
		break;
	case GF_PROP_VEC3I:
		p->value.vec3i.x = gf_bs_read_u32(ctx->bs_r);
		p->value.vec3i.y = gf_bs_read_u32(ctx->bs_r);
		p->value.vec3i.z = gf_bs_read_u32(ctx->bs_r);
		break;
	case GF_PROP_VEC3:
		p->value.vec3.x = gf_bs_read_double(ctx->bs_r);
		p->value.vec3.y = gf_bs_read_double(ctx->bs_r);
		p->value.vec3.z = gf_bs_read_double(ctx->bs_r);
		break;
	case GF_PROP_VEC4I:
		p->value.vec4i.x = gf_bs_read_u32(ctx->bs_r);
		p->value.vec4i.y = gf_bs_read_u32(ctx->bs_r);
		p->value.vec4i.z = gf_bs_read_u32(ctx->bs_r);
		p->value.vec4i.w = gf_bs_read_u32(ctx->bs_r);
		break;
	case GF_PROP_VEC4:
		p->value.vec4.x = gf_bs_read_double(ctx->bs_r);
		p->value.vec4.y = gf_bs_read_double(ctx->bs_r);
		p->value.vec4.z = gf_bs_read_double(ctx->bs_r);
		p->value.vec4.w = gf_bs_read_double(ctx->bs_r);
		break;
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
	case GF_PROP_NAME:
		p->type = GF_PROP_STRING_NO_COPY;
		len = gsfdmx_read_vlen(ctx);
		p->value.string = gf_malloc(sizeof(char)*(len+1));
		gf_bs_read_data(ctx->bs_r, p->value.string, len);
		p->value.string[len]=0;
		break;

	case GF_PROP_DATA:
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_CONST_DATA:
		p->type = GF_PROP_DATA_NO_COPY;
		p->value.data.size = gsfdmx_read_vlen(ctx);
		p->value.data.ptr = gf_malloc(sizeof(char) * p->value.data.size);
		gf_bs_read_data(ctx->bs_r, p->value.data.ptr, p->value.data.size);
		break;

	//string list: memory is ALWAYS duplicated
	case GF_PROP_STRING_LIST:
		p->value.string_list = gf_list_new();
		len2 = gsfdmx_read_vlen(ctx);
		for (i=0; i<len2; i++) {
			len = gsfdmx_read_vlen(ctx);
			char *str = gf_malloc(sizeof(char)*(len+1));
			gf_bs_read_data(ctx->bs_r, str, len);
			str[len] = 0;
			gf_list_add(p->value.string_list, str);
		}
		break;

	case GF_PROP_UINT_LIST:
		p->value.uint_list.nb_items = len = gsfdmx_read_vlen(ctx);
		p->value.uint_list.vals = gf_malloc(sizeof(u32)*len);
		for (i=0; i<len; i++) {
			p->value.uint_list.vals[i] = gf_bs_read_u32(ctx->bs_r);
		}
		break;
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] pointer property found in serialized stream, illegal\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Cannot deserialize property of unknown type\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}

static GF_GSFStream *gsfdmx_read_stream(GF_GSFDemuxCtx *ctx)
{
	u32 idx=0;
	u32 i, count;
	idx = gsfdmx_read_vlen(ctx);
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		GF_GSFStream *gst = gf_list_get(ctx->streams, i);
		if (gst->idx == idx) return gst;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFDemux] no stream found for idx %d\n", idx));
	return NULL;
}
static GF_Err gsfdmx_parse_pid_info(GF_Filter *filter, GF_GSFDemuxCtx *ctx, Bool is_info_update)
{
	GF_Err e;
	u32 nb_4cc_props=0;
	u32 nb_str_props=0;
	u32 idx=0;
	u32 i, count;
	GF_GSFStream *gst=NULL;

	//pid ID
	idx = gsfdmx_read_vlen(ctx);
	nb_4cc_props = gsfdmx_read_vlen(ctx);
	nb_str_props = gsfdmx_read_vlen(ctx);

	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		gst = gf_list_get(ctx->streams, i);
		if (gst->idx == idx) break;
		gst = NULL;
	}
	if (!gst) {
		GF_SAFEALLOC(gst, GF_GSFStream);
		gst->idx = idx;
		gf_list_add(ctx->streams, gst);
		gst->opid = gf_filter_pid_new(filter);
	}

	for (i=0; i<nb_4cc_props; i++) {
		GF_PropertyValue p;
		u32 p4cc = gf_bs_read_u32(ctx->bs_r);

		memset(&p, 0, sizeof(GF_PropertyValue));
		p.type = gf_props_4cc_get_type(p4cc);
		if (p.type==GF_PROP_FORBIDEN) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC property type for property 4CC %s\n", gf_4cc_to_str(p4cc) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		e = gsfdmx_read_prop(ctx, &p);
		if (e) return e;

		if (is_info_update) gf_filter_pid_set_info(gst->opid, p4cc, &p);
		else gf_filter_pid_set_property(gst->opid, p4cc, &p);
	}
	for (i=0; i<nb_str_props; i++) {
		GF_PropertyValue p;

		u32 len = gsfdmx_read_vlen(ctx);
		char *pname = gf_malloc(sizeof(char)*(len+1));
		gf_bs_read_data(ctx->bs_r, pname, len);
		pname[len]=0;

		memset(&p, 0, sizeof(GF_PropertyValue));
		p.type = gf_bs_read_u8(ctx->bs_r);

		e = gsfdmx_read_prop(ctx, &p);
		if (e) return e;

		if (is_info_update) gf_filter_pid_set_info_dyn(gst->opid, pname, &p);
		else gf_filter_pid_set_property_dyn(gst->opid, pname, &p);
	}
	return GF_OK;
}

static GF_Err gsfdmx_tune(GF_Filter *filter, GF_GSFDemuxCtx *ctx, u32 pck_size, Bool is_crypted)
{
	u32 i, len, pos;
	u32 sig = gf_bs_read_u32(ctx->bs_r);
	if (sig != GF_4CC('G','S','S','F') ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC serialized signature %s\n", gf_4cc_to_str(sig) ));
		ctx->tune_error = GF_TRUE;
		return GF_NOT_SUPPORTED;
	}
	sig = gf_bs_read_u8(ctx->bs_r);
	if (sig != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC serialized version %d\n", sig ));
		ctx->tune_error = GF_TRUE;
		return GF_NOT_SUPPORTED;
	}
	if (is_crypted) {
		if (pck_size<25) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong serialized header size %d, should be at least 25 bytes for encrypted streams\n", pck_size ));
			ctx->tune_error = GF_TRUE;
			return GF_NOT_SUPPORTED;
		}
		gf_bs_read_data(ctx->bs_r, ctx->crypt_IV, 16);
		ctx->crypt_blocks = gf_bs_read_u16(ctx->bs_r);
		ctx->skip_blocks = gf_bs_read_u16(ctx->bs_r);

		if (!ctx->key.size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] stream is encrypted but no key provided\n" ));
			ctx->tune_error = GF_TRUE;
			return GF_NOT_SUPPORTED;
		}
		ctx->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!ctx->crypt) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] failed to create decryptor\n" ));
			ctx->tune_error = GF_TRUE;
			return GF_NOT_SUPPORTED;
		}
		gf_crypt_init(ctx->crypt, ctx->key.ptr, ctx->crypt_IV);

		gsfdmx_decrypt(ctx, pck_size - 25);
	}

	//header:magic
	len = gsfdmx_read_vlen(ctx);
	if (len) {
		Bool wrongm=GF_FALSE;
		char *magic = gf_malloc(sizeof(char)*len);
		gf_bs_read_data(ctx->bs_r, magic, len);

		if (ctx->magic && memcmp(ctx->magic, magic, len)) wrongm=GF_TRUE;
		gf_free(magic);
		if (!wrongm) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] wrong magic word in stream config\n" ));
			ctx->tune_error = GF_TRUE;
			return GF_NOT_SUPPORTED;
		}
 	}

	//header:version
	len = gsfdmx_read_vlen(ctx);
	if (len) {
		char *vers = gf_malloc(sizeof(char)*(len+1));
		gf_bs_read_data(ctx->bs_r, vers, len);
		vers[len]=0;
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[GSFDemux] tuning in stream generated with %s\n", vers));
		gf_free(vers);
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[GSFDemux] tuning in stream, no info version\n"));
	}
	ctx->tuned = GF_TRUE;
	//header:nb_streams
	len = gsfdmx_read_vlen(ctx);
	for (i=0; i<len; i++) {
		GF_Err e = gsfdmx_parse_pid_info(filter, ctx, GF_FALSE);
		if (e) return e;
	}
	pos = gf_bs_get_position(ctx->bs_r);
	return GF_OK;
}


GF_Err gsfdmx_read_packet(GF_GSFDemuxCtx *ctx, u32 pck_len)
{
	GF_FilterPacket *dst_pck;
	u32 out_pck_size;
	u64 dts, cts, bo;
	u32 dur, dep_flags, full_au_size, tsmodebits, durmodebits, spos;
	s16 roll;
	u8 carv;
	char *output;

	spos = gf_bs_get_position(ctx->bs_r);
	GF_GSFStream *gst = gsfdmx_read_stream(ctx);
	//not yet setup
	if (!gst) return GF_NOT_FOUND;

	//first flags byte
	u8 has_dts = gf_bs_read_int(ctx->bs_r, 1);
	u8 has_cts = gf_bs_read_int(ctx->bs_r, 1);
	u8 has_bo = gf_bs_read_int(ctx->bs_r, 1);
	u8 corr = gf_bs_read_int(ctx->bs_r, 1);
	u8 seek = gf_bs_read_int(ctx->bs_r, 1);
	u8 crypt = gf_bs_read_int(ctx->bs_r, 2);
	u8 has_carv = gf_bs_read_int(ctx->bs_r, 1);

	//second flags byte
	u8 interl = gf_bs_read_int(ctx->bs_r, 2);
	u8 cktype = gf_bs_read_int(ctx->bs_r, 2);
	u8 sap = gf_bs_read_int(ctx->bs_r, 3);
	u8 has_dep = gf_bs_read_int(ctx->bs_r, 1);

	//third flags byte
	u8 tsmode = gf_bs_read_int(ctx->bs_r, 2);
	u8 durmode = gf_bs_read_int(ctx->bs_r, 2);
	u8 sizemode = gf_bs_read_int(ctx->bs_r, 2);
	u8 has_4cc_props = gf_bs_read_int(ctx->bs_r, 1);
	u8 has_str_props = gf_bs_read_int(ctx->bs_r, 1);

	//done with flags

	full_au_size = gf_bs_read_int(ctx->bs_r, 8*(sizemode+1) );

	if (tsmode==3) tsmodebits = 64;
	else if (tsmode==2) tsmodebits = 32;
	else if (tsmode==1) tsmodebits = 24;
	else tsmodebits = 16;

	if (has_dts) {
		if (tsmode==3) dts = gf_bs_read_long_int(ctx->bs_r, tsmodebits);
		else dts = gf_bs_read_int(ctx->bs_r, tsmodebits);
	}
	if (has_cts) {
		if (tsmode==3) cts = gf_bs_read_long_int(ctx->bs_r, tsmodebits);
		else cts = gf_bs_read_int(ctx->bs_r, tsmodebits);
	}

	if (durmode==3) durmodebits = 32;
	else if (durmode==2) durmodebits = 16;
	else if (durmode==1) durmodebits = 8;
	else durmodebits = 0;

	if (durmode) {
		dur = gf_bs_read_int(ctx->bs_r, durmodebits);
	}

	if (sap==GF_FILTER_SAP_4) {
		roll = gf_bs_read_u16(ctx->bs_r);
	}
	if (has_dep)
		dep_flags = gf_bs_read_u8(ctx->bs_r);

	if (has_carv) {
		carv = gf_bs_read_u8(ctx->bs_r);
	}
	if (has_bo) {
		bo = gf_bs_read_u64(ctx->bs_r);
	}

	out_pck_size = full_au_size;
	if (out_pck_size > pck_len) {
		u32 consummed = gf_bs_get_position(ctx->bs_r) - spos;
		out_pck_size = pck_len - consummed;
	}
	dst_pck = gf_filter_pck_new_alloc(gst->opid, out_pck_size, &output);

	if (has_4cc_props) {
		u32 nb_4cc = gsfdmx_read_vlen(ctx);
		while (nb_4cc) {
			GF_Err e;
			GF_PropertyValue p;
			memset(&p, 0, sizeof(GF_PropertyValue));
			u32 p4cc = gf_bs_read_u32(ctx->bs_r);
			p.type = gf_props_4cc_get_type(p4cc);
			if (p.type==GF_PROP_FORBIDEN) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC property type for property 4CC %s\n", gf_4cc_to_str(p4cc) ));
				gf_filter_pck_discard(dst_pck);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			nb_4cc--;

			e = gsfdmx_read_prop(ctx, &p);
			if (e) {
				gf_filter_pck_discard(dst_pck);
				return e;
			}
			gf_filter_pck_set_property(dst_pck, p4cc, &p);
		}
	}
	if (has_str_props) {
		u32 nb_props = gsfdmx_read_vlen(ctx);
		while (nb_props) {
			GF_Err e;
			GF_PropertyValue p;
			char *pname;
			memset(&p, 0, sizeof(GF_PropertyValue));
			u32 len = gsfdmx_read_vlen(ctx);
			pname = gf_malloc(sizeof(char)*(len+1) );
			gf_bs_read_data(ctx->bs_r, pname, len);
			pname[len] = 0;
			p.type = gf_bs_read_u8(ctx->bs_r);
			if (p.type==GF_PROP_FORBIDEN) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC property type for property %s\n", pname ));
				gf_free(pname);
				gf_filter_pck_discard(dst_pck);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			nb_props--;

			e = gsfdmx_read_prop(ctx, &p);
			if (e) {
				gf_filter_pck_discard(dst_pck);
				gf_free(pname);
				return e;
			}
			gf_filter_pck_set_property_dyn(dst_pck, pname, &p);
		}
	}

	u32 consummed = gf_bs_get_position(ctx->bs_r) - spos;
	pck_len -= consummed;
	if (out_pck_size>pck_len)
		out_pck_size = pck_len;
	gf_bs_read_data(ctx->bs_r, output, out_pck_size);
	gf_filter_pck_truncate(dst_pck, out_pck_size);

	gf_filter_pck_set_framing(dst_pck, GF_TRUE, (full_au_size==out_pck_size) ? GF_TRUE : GF_FALSE);
	if (has_dts) gf_filter_pck_set_dts(dst_pck, dts);
	if (has_cts) gf_filter_pck_set_cts(dst_pck, cts);
	if (durmode) gf_filter_pck_set_duration(dst_pck, dur);
	if (has_bo) gf_filter_pck_set_byte_offset(dst_pck, bo);
	if (corr) gf_filter_pck_set_corrupted(dst_pck, corr);
	if (interl) gf_filter_pck_set_interlaced(dst_pck, interl);
	if (has_carv) gf_filter_pck_set_carousel_version(dst_pck, carv);
	if (has_dep) gf_filter_pck_set_dependency_flag(dst_pck, dep_flags);
	if (cktype) gf_filter_pck_set_clock_type(dst_pck, cktype);
	if (seek) gf_filter_pck_set_seek_flag(dst_pck, seek);
	if (crypt) gf_filter_pck_set_crypt_flags(dst_pck, crypt);
	if (sap) gf_filter_pck_set_sap(dst_pck, sap);
	if (sap==GF_FILTER_SAP_4) gf_filter_pck_set_roll_info(dst_pck, roll);

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}

static const char *gsfdmx_pck_name(u32 pck_type)
{
	switch (pck_type) {
	case GFS_PCKTYPE_HDR: return "TUNE";
	case GFS_PCKTYPE_PID_CONFIG: return "CONFIG";
	case GFS_PCKTYPE_PID_INFO_UPDATE: return "INFO";
	case GFS_PCKTYPE_PID_REMOVE: return "PIDREM";
	case GFS_PCKTYPE_PID_EOS: return "PIDEOS";
	case GFS_PCKTYPE_PCK: return "PCK";
	case GFS_PCKTYPE_PCK_CONT: return "PCK_CONT";
	case GFS_PCKTYPE_PCK_LAST: return "PCK_END";
	default: return "FORBIDDEN";
	}
}

static GF_Err gsfdmx_demux(GF_Filter *filter, GF_GSFDemuxCtx *ctx, char *data, u32 data_size)
{
	u32 last_pck_end=0;
	if (ctx->alloc_size < ctx->buf_size + data_size) {
		ctx->buffer = (char*)gf_realloc(ctx->buffer, sizeof(char)*(ctx->buf_size + data_size) );
		ctx->alloc_size = ctx->buf_size + data_size;
	}

	memcpy(ctx->buffer + ctx->buf_size, data, sizeof(char)*data_size);
	ctx->buf_size += data_size;

	gf_bs_reassign_buffer(ctx->bs_r, ctx->buffer, ctx->buf_size);
	while (gf_bs_available(ctx->bs_r)) {
		GF_Err e = GF_OK;

		u32 hdr_pos = gf_bs_get_position(ctx->bs_r);

		Bool has_sn = gf_bs_read_int(ctx->bs_r, 1);
		u32 lmode = gf_bs_read_int(ctx->bs_r, 2);
		Bool is_crypted = gf_bs_read_int(ctx->bs_r, 1);
		u32 pck_type = gf_bs_read_int(ctx->bs_r, 4);
		u16 sn = 0;

		if (has_sn) sn = gf_bs_read_u16(ctx->bs_r);
		u32 pck_len = gf_bs_read_int(ctx->bs_r, 8*(lmode+1) );
		if (pck_len > gf_bs_available(ctx->bs_r)) {
			break;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFDemux] found packet type %s cryped %d sn %d size %d hdr size %d at position %d\n", gsfdmx_pck_name(pck_type), is_crypted, sn, pck_len,  gf_bs_get_position(ctx->bs_r)-hdr_pos, hdr_pos));

		u32 pck_start = gf_bs_get_position(ctx->bs_r);

		//bootstreap, special handling in case we are tune in, or in case we need to init decypher
		if (pck_type==GFS_PCKTYPE_HDR) {
			if (ctx->tuned) {
				gf_bs_skip_bytes(ctx->bs_r, pck_len);
				last_pck_end = gf_bs_get_position(ctx->bs_r);
				continue;
			}
			e = gsfdmx_tune(filter, ctx, pck_len, is_crypted);
			if (e) {
				gf_bs_seek(ctx->bs_r, pck_start);
				gf_bs_skip_bytes(ctx->bs_r, pck_len);
			}
			last_pck_end = gf_bs_get_position(ctx->bs_r);
			continue;
		}
		if (ctx->tune_error) {
			gf_bs_seek(ctx->bs_r, pck_start);
			gf_bs_skip_bytes(ctx->bs_r, pck_len);
			last_pck_end = gf_bs_get_position(ctx->bs_r);
			continue;
		}

		if (is_crypted) {
			gsfdmx_decrypt(ctx, pck_len);
			//remove crypted flag otherwise we may decrypt twice!
			ctx->buffer[hdr_pos] &= ~(1<<4);
		}

		if ((pck_type==GFS_PCKTYPE_PID_CONFIG) || (pck_type==GFS_PCKTYPE_PID_INFO_UPDATE) ) {
	 		e = gsfdmx_parse_pid_info(filter, ctx, (pck_type==GFS_PCKTYPE_PID_INFO_UPDATE) ? GF_TRUE : GF_FALSE);
		}
		else if (pck_type==GFS_PCKTYPE_PID_REMOVE) {
			GF_GSFStream *gst = gsfdmx_read_stream(ctx);
			if (gst) {
				gf_filter_pid_remove(gst->opid);
				gf_list_del_item(ctx->streams, gst);
				gf_free(gst);
			} else {
				e = GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		else if (pck_type==GFS_PCKTYPE_PID_EOS) {
			GF_GSFStream *gst = gsfdmx_read_stream(ctx);
			if (gst) {
				gf_filter_pid_set_eos(gst->opid);
			} else {
				e = GF_NON_COMPLIANT_BITSTREAM;
			}
		} else if (pck_type==GFS_PCKTYPE_PCK) {
			if (!ctx->nb_playing && ctx->tuned && gf_list_count(ctx->streams) ) {
				ctx->wait_for_play = GF_TRUE;
				break;
			}

			e = gsfdmx_read_packet(ctx, pck_len);
		} else if ((pck_type==GFS_PCKTYPE_PCK_CONT) || (pck_type==GFS_PCKTYPE_PCK_LAST)) {
			GF_GSFStream *gst = gsfdmx_read_stream(ctx);
			if (gst) {
				char *output;
				GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(gst->opid, pck_len, &output);
				gf_bs_read_data(ctx->bs_r, output, pck_len);
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, (pck_type==GFS_PCKTYPE_PCK_LAST) );
			} else {
				e = GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			//ignore unrecognized
			e = GF_NON_COMPLIANT_BITSTREAM;
		}

		if (!e && (pck_len + pck_start != gf_bs_get_position(ctx->bs_r))) {
			e = GF_CORRUPTED_DATA;
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFDemux] mismatch in packet end (%d) vs parsed data (%d), realigning\n", pck_len + pck_start, gf_bs_get_position(ctx->bs_r) ));
		}

		if (e) {
			gf_bs_seek(ctx->bs_r, pck_start);
			gf_bs_skip_bytes(ctx->bs_r, pck_len);
		}
		last_pck_end = gf_bs_get_position(ctx->bs_r);
	}

	if (last_pck_end) {
		assert(ctx->buf_size>=last_pck_end);
		memmove(ctx->buffer, ctx->buffer+last_pck_end, sizeof(char) * (ctx->buf_size-last_pck_end));
		ctx->buf_size -= last_pck_end;
	}
	return GF_OK;
}

GF_Err gsfdmx_process(GF_Filter *filter)
{
	GF_Err e;
	GF_GSFDemuxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_GSFStream *st;
	u32 i=0, pkt_size;
	const char *data;
	u32 would_block = 0;

	if (ctx->wait_for_play) return GF_OK;

	//update duration
//	safdmx_check_dur(ctx);

	//check if all the streams are in block state, if so return.
	//we need to check for all output since one pid could still be buffering
	while ((st = gf_list_enum(ctx->streams, &i))) {
		if (st->opid && gf_filter_pid_would_block(st->opid))
			would_block++;
	}
	if (would_block && (would_block+1==i))
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
//		if (gf_filter_pid_is_eos(ctx->ipid)) oggdmx_signal_eos(ctx);
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &pkt_size);
	e = gsfdmx_demux(filter, ctx, (char *) data, pkt_size);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static const char *gsfdmx_probe_data(const u8 *data, u32 data_size)
{
	char szSig[5];
	strcpy(szSig, "GSSF");
	char *found_sig = memmem(data, data_size, szSig, 4);
	if (!found_sig) return NULL;

	if (found_sig[4]!=1)return NULL;
	return "application/x-gpac-sf";
}

static GF_Err gsfdmx_initialize(GF_Filter *filter)
{
	GF_GSFDemuxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	if (!ctx->streams) return GF_OUT_OF_MEM;
	ctx->bs_r = gf_bs_new((char *) ctx, 1, GF_BITSTREAM_READ);
	return GF_OK;
}

static void gsfdmx_finalize(GF_Filter *filter)
{
	GF_GSFDemuxCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->streams)) {
		GF_GSFStream *gst = gf_list_pop_back(ctx->streams);
		gf_free(gst);
	}
	gf_list_del(ctx->streams);
	if (ctx->crypt) gf_crypt_close(ctx->crypt);
	if (ctx->buffer) gf_free(ctx->buffer);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
}

static const GF_FilterCapability GSFDemuxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-gpac-sf"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "gsf"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};



#define OFFS(_n)	#_n, offsetof(GF_GSFDemuxCtx, _n)
static const GF_FilterArgs GSFDemuxArgs[] =
{
	{ OFFS(key), "key for decrypting packets", GF_PROP_DATA, NULL, NULL, GF_FALSE},
	{0}
};


GF_FilterRegister GSFDemuxRegister = {
	.name = "gsfd",
	.description = "GPAC stream format demultiplexer",
	.comment = "This filter deserialize the stream states (config/reconfig/info update/remove/eos) and packets of input PIDs.\n"\
			"This allows either reading a session saved to file, or receiving the state/data of streams from another instance of GPAC\n"\
			"using either pipes or sockets\n"\
			"\n"\
			"The stream format can be encrypted in AES 128 CBC mode, in which case the demux filters has to be given a 128 bit key.",
	.private_size = sizeof(GF_GSFDemuxCtx),
	.max_extra_pids = (u32) -1,
	.args = GSFDemuxArgs,
	SETCAPS(GSFDemuxCaps),
	.initialize = gsfdmx_initialize,
	.finalize = gsfdmx_finalize,
	.configure_pid = gsfdmx_configure_pid,
	.process = gsfdmx_process,
	.process_event = gsfdmx_process_event,
	.probe_data = gsfdmx_probe_data,
};


const GF_FilterRegister *gsfdmx_register(GF_FilterSession *session)
{
	return &GSFDemuxRegister;
}
