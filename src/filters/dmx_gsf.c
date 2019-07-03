/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2019
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
	u32 offset;
	u32 size;
} GSF_PacketFragment;

typedef struct
{
	GF_FilterPacket *pck;
	u8 *output;
	u32 full_block_size, nb_bytes, recv_bytes;
	Bool corrupted;
	u16 frame_sn;
	u8 pck_type;
	u8 crypted;

	u32 nb_frags, nb_alloc_frags, nb_recv_frags;
	Bool complete;
	GSF_PacketFragment *frags;

} GSF_Packet;


typedef struct
{
	GF_FilterPid *opid;
	GF_List *packets;

	u32 idx;
	u16 nb_pck;
	u8 config_version;

	u16 last_frame_sn;
} GSF_Stream;

typedef struct
{
	//opts
	const char *magic;
	GF_PropData key;
	u32 pad, mq;


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
	GF_BitStream *bs_pck;

	//signaling stream
	GSF_Stream *signal_st;

	//where we store incoming packets
	char *buffer;
	u32 alloc_size, buf_size;

	u32 missing_bytes;
	Bool tuned;
	Bool tune_error;
	Bool use_seq_num;
	Bool wait_for_play;

	GF_List *pck_res;
	Bool buffer_too_small;
} GSF_DemuxCtx;


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

} GSF_PacketType;



GF_Err gsfdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GSF_DemuxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		GSF_Stream *st;
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
	GSF_DemuxCtx *ctx = gf_filter_get_udta(filter);

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

static void gsfdmx_decrypt(GSF_DemuxCtx *ctx, char *data, u32 size)
{
	u32 pos=0;
	u32 clear_tail = size%16;
	u32 bytes_crypted = size - clear_tail;

	if (!bytes_crypted) return;

	gf_crypt_set_IV(ctx->crypt, ctx->crypt_IV, 16);
	if (ctx->crypt_blocks && ctx->skip_blocks) {
		u32 pattern_length = 16 * (ctx->crypt_blocks + ctx->skip_blocks);
		u32 cryp_len = 16 * ctx->crypt_blocks;
		while (bytes_crypted) {
			gf_crypt_decrypt(ctx->crypt, data+pos, bytes_crypted >= cryp_len ? cryp_len : bytes_crypted);
			if (bytes_crypted >= pattern_length) {
				pos += pattern_length;
				bytes_crypted -= pattern_length;
			} else {
				bytes_crypted = 0;
			}
		}
	} else {
		gf_crypt_decrypt(ctx->crypt, data, bytes_crypted);
	}
}

static GFINLINE u32 gsfdmx_read_vlen(GF_BitStream *bs)
{
	if (!gf_bs_read_int(bs, 1))
		return gf_bs_read_int(bs, 7);
	if (!gf_bs_read_int(bs, 1))
		return gf_bs_read_int(bs, 14);
	if (!gf_bs_read_int(bs, 1))
		return gf_bs_read_int(bs, 21);
	if (!gf_bs_read_int(bs, 1))
		return gf_bs_read_int(bs, 28);

	return (u32) gf_bs_read_long_int(bs, 36);
}

static GF_Err gsfdmx_read_prop(GF_BitStream *bs, GF_PropertyValue *p)
{
	u32 len, len2, i;

	switch (p->type) {
	case GF_PROP_SINT:
	case GF_PROP_UINT:
	case GF_PROP_PIXFMT:
	case GF_PROP_PCMFMT:
		p->value.uint = gsfdmx_read_vlen(bs);
		break;
	case GF_PROP_LSINT:
	case GF_PROP_LUINT:
		p->value.longuint = gf_bs_read_u64(bs);
		break;
	case GF_PROP_BOOL:
		p->value.boolean = gf_bs_read_u8(bs) ? 1 : 0;
		break;
	case GF_PROP_FRACTION:
		p->value.frac.num = gsfdmx_read_vlen(bs);
		p->value.frac.den = gsfdmx_read_vlen(bs);
		break;
	case GF_PROP_FRACTION64:
		p->value.lfrac.num = gf_bs_read_u64(bs);
		p->value.lfrac.den = gf_bs_read_u64(bs);
		break;
	case GF_PROP_FLOAT:
		p->value.fnumber = FLT2FIX( gf_bs_read_float(bs) );
		break;
	case GF_PROP_DOUBLE:
		p->value.number = gf_bs_read_double(bs);
		break;
	case GF_PROP_VEC2I:
		p->value.vec2i.x = gsfdmx_read_vlen(bs);
		p->value.vec2i.y = gsfdmx_read_vlen(bs);
		break;
	case GF_PROP_VEC2:
		p->value.vec2.x = gf_bs_read_double(bs);
		p->value.vec2.y = gf_bs_read_double(bs);
		break;
	case GF_PROP_VEC3I:
		p->value.vec3i.x = gsfdmx_read_vlen(bs);
		p->value.vec3i.y = gsfdmx_read_vlen(bs);
		p->value.vec3i.z = gsfdmx_read_vlen(bs);
		break;
	case GF_PROP_VEC3:
		p->value.vec3.x = gf_bs_read_double(bs);
		p->value.vec3.y = gf_bs_read_double(bs);
		p->value.vec3.z = gf_bs_read_double(bs);
		break;
	case GF_PROP_VEC4I:
		p->value.vec4i.x = gsfdmx_read_vlen(bs);
		p->value.vec4i.y = gsfdmx_read_vlen(bs);
		p->value.vec4i.z = gsfdmx_read_vlen(bs);
		p->value.vec4i.w = gsfdmx_read_vlen(bs);
		break;
	case GF_PROP_VEC4:
		p->value.vec4.x = gf_bs_read_double(bs);
		p->value.vec4.y = gf_bs_read_double(bs);
		p->value.vec4.z = gf_bs_read_double(bs);
		p->value.vec4.w = gf_bs_read_double(bs);
		break;
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
	case GF_PROP_NAME:
		p->type = GF_PROP_STRING_NO_COPY;
		len = gsfdmx_read_vlen(bs);
		p->value.string = gf_malloc(sizeof(char)*(len+1));
		gf_bs_read_data(bs, p->value.string, len);
		p->value.string[len]=0;
		break;

	case GF_PROP_DATA:
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_CONST_DATA:
		p->type = GF_PROP_DATA_NO_COPY;
		p->value.data.size = gsfdmx_read_vlen(bs);
		p->value.data.ptr = gf_malloc(sizeof(char) * p->value.data.size);
		gf_bs_read_data(bs, p->value.data.ptr, p->value.data.size);
		break;

	case GF_PROP_STRING_LIST:
		p->value.string_list = gf_list_new();
		len2 = gsfdmx_read_vlen(bs);
		for (i=0; i<len2; i++) {
			len = gsfdmx_read_vlen(bs);
			char *str = gf_malloc(sizeof(char)*(len+1));
			gf_bs_read_data(bs, str, len);
			str[len] = 0;
			gf_list_add(p->value.string_list, str);
		}
		break;

	case GF_PROP_UINT_LIST:
		p->value.uint_list.nb_items = len = gsfdmx_read_vlen(bs);
		p->value.uint_list.vals = gf_malloc(sizeof(u32)*len);
		for (i=0; i<len; i++) {
			p->value.uint_list.vals[i] = gsfdmx_read_vlen(bs);
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

static GSF_Stream *gsfdmx_get_stream(GF_Filter *filter, GSF_DemuxCtx *ctx, u32 idx, u32 pkt_type)
{
	GSF_Stream *gst;
	u32 i, count;

	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		gst = gf_list_get(ctx->streams, i);
		if (gst->idx == idx) return gst;
	}

	if ((pkt_type==GFS_PCKTYPE_PID_CONFIG) || (pkt_type==GFS_PCKTYPE_PID_INFO_UPDATE) ) {
		GF_SAFEALLOC(gst, GSF_Stream);
		gst->packets = gf_list_new();
		gst->idx = idx;
		gf_list_add(ctx->streams, gst);
		gst->opid = gf_filter_pid_new(filter);
		return gst;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFDemux] no stream found for idx %d\n", idx));
	return NULL;
}
static GF_Err gsfdmx_parse_pid_info(GF_Filter *filter, GSF_DemuxCtx *ctx, GSF_Stream *gst, GSF_Packet *pck, Bool is_info_update)
{
	GF_Err e;
	u32 nb_4cc_props=0;
	u32 nb_str_props=0;
	u32 i;
	u8 cfg_version;
	GF_BitStream *bs=NULL;

	if (pck->crypted) {
		gsfdmx_decrypt(ctx, pck->output, pck->full_block_size);
	}
	e = gf_bs_reassign_buffer(ctx->bs_pck, pck->output, pck->full_block_size);
	if (e) return e;
	bs = ctx->bs_pck;

	cfg_version = gf_bs_read_u8(bs);
	if ((gst->config_version == 1 + cfg_version) && !is_info_update)
		return GF_OK;
	gst->config_version = 1 + cfg_version;

	nb_4cc_props = gsfdmx_read_vlen(bs);
	nb_str_props = gsfdmx_read_vlen(bs);

	for (i=0; i<nb_4cc_props; i++) {
		GF_PropertyValue p;
		u32 p4cc = gf_bs_read_u32(bs);

		memset(&p, 0, sizeof(GF_PropertyValue));
		p.type = gf_props_4cc_get_type(p4cc);
		if (p.type==GF_PROP_FORBIDEN) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC property type for property 4CC %s\n", gf_4cc_to_str(p4cc) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		e = gsfdmx_read_prop(bs, &p);
		if (e) return e;

		if (is_info_update) gf_filter_pid_set_info(gst->opid, p4cc, &p);
		else gf_filter_pid_set_property(gst->opid, p4cc, &p);
	}
	for (i=0; i<nb_str_props; i++) {
		GF_PropertyValue p;

		u32 len = gsfdmx_read_vlen(bs);
		char *pname = gf_malloc(sizeof(char)*(len+1));
		gf_bs_read_data(bs, pname, len);
		pname[len]=0;

		memset(&p, 0, sizeof(GF_PropertyValue));
		p.type = gf_bs_read_u8(bs);

		e = gsfdmx_read_prop(bs, &p);
		if (e) {
			gf_free(pname);
			return e;
		}
		if (is_info_update) gf_filter_pid_set_info_dyn(gst->opid, pname, &p);
		else gf_filter_pid_set_property_dyn(gst->opid, pname, &p);
		gf_free(pname);
	}
	return GF_OK;
}

static GF_Err gsfdmx_tune(GF_Filter *filter, GSF_DemuxCtx *ctx, char *pck_data, u32 pck_size, Bool is_crypted)
{
	u32 len;
	GF_BitStream *bs;
	GF_Err e = gf_bs_reassign_buffer(ctx->bs_pck, pck_data, pck_size);
	if (e) return e;
	bs = ctx->bs_pck;

	u32 sig = gf_bs_read_u32(bs);
	if (sig != GF_4CC('G','S','5','F') ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC serialized signature %s, expecting \"GS5F\"\n", gf_4cc_to_str(sig) ));
		ctx->tune_error = GF_TRUE;
		return GF_NOT_SUPPORTED;
	}
	sig = gf_bs_read_u8(bs);
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
		gf_bs_read_data(bs, ctx->crypt_IV, 16);
		ctx->crypt_blocks = gf_bs_read_u16(bs);
		ctx->skip_blocks = gf_bs_read_u16(bs);

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

		gsfdmx_decrypt(ctx, pck_data+25, pck_size - 25);
	}
	ctx->use_seq_num = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);

	//header:magic
	len = gsfdmx_read_vlen(bs);
	if (len) {
		Bool wrongm=GF_FALSE;
		char *magic = gf_malloc(sizeof(char)*len);
		gf_bs_read_data(bs, magic, len);

		if (ctx->magic && !memcmp(ctx->magic, magic, len)) wrongm = GF_TRUE;
		if (!wrongm) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] wrong magic word in stream config\n" ));
			ctx->tune_error = GF_TRUE;
			gf_free(magic);
			return GF_NOT_SUPPORTED;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[GSFDemux] tuning in stream, magic %s\n", magic));
		gf_free(magic);
 	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[GSFDemux] tuning in stream\n"));
	}

	ctx->tuned = GF_TRUE;
	return GF_OK;
}

static GFINLINE GSF_Packet *gsfdmx_get_packet(GSF_DemuxCtx *ctx, GSF_Stream *gst, Bool pck_frag, s32 frame_sn, u8 pkt_type, u32 frame_size)
{
	u32 i=0, count;
	GSF_Packet *gpck = NULL;

	if ((frame_sn>=0) || pck_frag) {
		while (( gpck = gf_list_enum(gst->packets, &i))) {
			if (gpck->frame_sn == frame_sn) {
				assert(gpck->pck_type == pkt_type);
				assert(gpck->full_block_size == frame_size);

				break;
			}
			gpck = NULL;
		}
	}
	if (!gpck) {
		Bool inserted = GF_FALSE;
		gpck = gf_list_pop_back(ctx->pck_res);
		if (!gpck) {
 			GF_SAFEALLOC(gpck, GSF_Packet);
 			gpck->nb_alloc_frags = 10;
 			gpck->frags = gf_malloc(sizeof(GSF_PacketFragment) * gpck->nb_alloc_frags);
		}
		gpck->frame_sn = frame_sn;
		gpck->pck_type = pkt_type;
		gpck->full_block_size = frame_size;
		gpck->pck = gf_filter_pck_new_alloc(gst->opid, frame_size, &gpck->output);
		memset(gpck->output, (u8) ctx->pad, sizeof(char) * gpck->full_block_size);

		count = gf_list_count(gst->packets);
		for (i=0; i<count; i++) {
			GSF_Packet *apck = gf_list_get(gst->packets, i);

			if ( ( (apck->frame_sn > frame_sn) && (apck->frame_sn - frame_sn <= 32768) )
				|| ( (apck->frame_sn < frame_sn ) && (frame_sn - apck->frame_sn > 32768) )
			) {
				inserted = GF_TRUE;
				gf_list_insert(gst->packets, gpck, i);
			}
		}
		if (!inserted) gf_list_add(gst->packets, gpck);
	}
	return gpck;
}

static void gsfdmx_packet_append_frag(GSF_Packet *pck, u32 size, u32 offset)
{
	u32 i;
	Bool inserted = GF_FALSE;
	pck->recv_bytes += size;
	pck->nb_recv_frags++;

	assert(offset + size <= pck->full_block_size);

	for (i=0; i<pck->nb_frags; i++) {
		if ((pck->frags[i].offset <= offset) && (pck->frags[i].offset + pck->frags[i].size >= offset + size) ) {
			return;
		}

		//insert fragment
		if (pck->frags[i].offset > offset) {
			if (pck->nb_frags==pck->nb_alloc_frags) {
				pck->nb_alloc_frags *= 2;
				pck->frags = gf_realloc(pck->frags, sizeof(GSF_PacketFragment)*pck->nb_alloc_frags);
			}
			memmove(&pck->frags[i+1], &pck->frags[i], sizeof(GSF_PacketFragment) * (pck->nb_frags - i)  );
			pck->frags[i].offset = offset;
			pck->frags[i].size = size;
			pck->nb_bytes += size;
			pck->nb_frags++;
			inserted = GF_TRUE;
			break;
		}
		//expand fragment
		if (pck->frags[i].offset + pck->frags[i].size == offset) {
			pck->frags[i].size += size;
			pck->nb_bytes += size;
			inserted = GF_TRUE;
			break;
		}
	}

	if (!inserted) {
		if (pck->nb_frags==pck->nb_alloc_frags) {
			pck->nb_alloc_frags *= 2;
			pck->frags = gf_realloc(pck->frags, sizeof(GSF_PacketFragment)*pck->nb_alloc_frags);
		}
		pck->frags[pck->nb_frags].offset = offset;
		pck->frags[pck->nb_frags].size = size;
		pck->nb_frags++;
		pck->nb_bytes += size;
	}
	if (pck->nb_bytes >= pck->full_block_size) {
		if (pck->nb_bytes>pck->full_block_size) pck->corrupted=GF_TRUE;
		pck->complete = GF_TRUE;
	}
}

GF_Err gsfdmx_read_data_pck(GSF_DemuxCtx *ctx, GSF_Stream *gst, GSF_Packet *gpck, u32 pck_len, Bool full_pck, GF_BitStream *bs)
{
	u64 dts=GF_FILTER_NO_TS, cts=GF_FILTER_NO_TS, bo=GF_FILTER_NO_BO;
	u32 copy_size, consummed, dur, dep_flags=0, tsmodebits, durmodebits, spos;
	s16 roll=0;
	u8 carv=0;

	//not yet setup
	if (!gst || !gpck) return GF_NOT_FOUND;


//	gsfdmx_flush_dst_pck(gst, GF_TRUE);

	spos = (u32) gf_bs_get_position(bs);

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
	//if has_carousel, carrousel version on 8 bits
	//if has_byteoffset, byte offset on 64 bits
	//if (has builtin) vlen nb builtin_props then props[builtin_props]
	//if (has props) vlen nb_str_props then props[nb_str_props]

	//first flags byte
	u8 has_dts = gf_bs_read_int(bs, 1);
	u8 has_cts = gf_bs_read_int(bs, 1);
	u8 has_dur = gf_bs_read_int(bs, 1);
	u8 cts_diff_neg = gf_bs_read_int(bs, 1);
	u8 tsmode = gf_bs_read_int(bs, 2);
	u8 tsdiffmode = gf_bs_read_int(bs, 2);

	u8 sap = gf_bs_read_int(bs, 3);
	u8 crypt = gf_bs_read_int(bs, 2);
	u8 has_dep = gf_bs_read_int(bs, 1);
	u8 has_4cc_props = gf_bs_read_int(bs, 1);
	u8 has_ext = gf_bs_read_int(bs, 1);

	//default field values without ext
	u8 has_bo = 0, corr = 0, seek = 0, has_carv = 0, interl = 0, cktype = 0, has_str_props = 0;
	u8 is_start = 1, is_end = 1;
	if (has_ext) {
		has_bo = gf_bs_read_int(bs, 1);
		corr = gf_bs_read_int(bs, 1);
		seek = gf_bs_read_int(bs, 1);
		has_carv = gf_bs_read_int(bs, 1);
		interl = gf_bs_read_int(bs, 2);
		cktype = gf_bs_read_int(bs, 2);

		is_start = gf_bs_read_int(bs, 1);
		is_end = gf_bs_read_int(bs, 1);
		has_str_props = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 5); //reserved
	}

	if (tsmode==3) tsmodebits = 64;
	else if (tsmode==2) tsmodebits = 32;
	else if (tsmode==1) tsmodebits = 24;
	else tsmodebits = 16;

	if (tsdiffmode==3) durmodebits = 32;
	else if (tsdiffmode==2) durmodebits = 24;
	else if (tsdiffmode==1) durmodebits = 16;
	else durmodebits = 8;

	if (has_dts) {
		if (tsmode==3) dts = gf_bs_read_long_int(bs, tsmodebits);
		else dts = gf_bs_read_int(bs, tsmodebits);
	}
	if (has_cts) {
		if (has_dts) {
			s32 cts_diff = gf_bs_read_int(bs, durmodebits);
			if (cts_diff_neg) {
				cts = dts - cts_diff;
			} else {
				cts = dts + cts_diff;
			}
		} else {
			if (tsmode==3) cts = gf_bs_read_long_int(bs, tsmodebits);
			else cts = gf_bs_read_int(bs, tsmodebits);
		}
	}

	if (has_dur) {
		dur = gf_bs_read_int(bs, durmodebits);
	}

	if (sap==GF_FILTER_SAP_4) {
		roll = gf_bs_read_u16(bs);
	}
	if (has_dep)
		dep_flags = gf_bs_read_u8(bs);

	if (has_carv) {
		carv = gf_bs_read_u8(bs);
	}
	if (has_bo) {
		bo = gf_bs_read_u64(bs);
	}

	if (has_4cc_props) {
		u32 nb_4cc = gsfdmx_read_vlen(bs);
		while (nb_4cc) {
			GF_Err e;
			GF_PropertyValue p;
			memset(&p, 0, sizeof(GF_PropertyValue));
			u32 p4cc = gf_bs_read_u32(bs);
			p.type = gf_props_4cc_get_type(p4cc);
			if (p.type==GF_PROP_FORBIDEN) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC property type for property 4CC %s\n", gf_4cc_to_str(p4cc) ));
				gf_filter_pck_discard(gpck->pck);
				gpck->pck = NULL;
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			nb_4cc--;

			e = gsfdmx_read_prop(bs, &p);
			if (e) {
				gf_filter_pck_discard(gpck->pck);
				gpck->pck = NULL;
				return e;
			}
			gf_filter_pck_set_property(gpck->pck, p4cc, &p);
		}
	}
	if (has_str_props) {
		u32 nb_props = gsfdmx_read_vlen(bs);
		while (nb_props) {
			GF_Err e;
			GF_PropertyValue p;
			char *pname;
			memset(&p, 0, sizeof(GF_PropertyValue));
			u32 len = gsfdmx_read_vlen(bs);
			pname = gf_malloc(sizeof(char)*(len+1) );
			gf_bs_read_data(bs, pname, len);
			pname[len] = 0;
			p.type = gf_bs_read_u8(bs);
			if (p.type==GF_PROP_FORBIDEN) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] Wrong GPAC property type for property %s\n", pname ));
				gf_free(pname);
				gf_filter_pck_discard(gpck->pck);
				gpck->pck = NULL;
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			nb_props--;

			e = gsfdmx_read_prop(bs, &p);
			if (e) {
				gf_filter_pck_discard(gpck->pck);
				gf_free(pname);
				gpck->pck = NULL;
				return e;
			}
			gf_filter_pck_set_property_dyn(gpck->pck, pname, &p);
			gf_free(pname);
			if ((p.type==GF_PROP_UINT_LIST) && p.value.uint_list.vals)
				gf_free(p.value.uint_list.vals);
		}
	}

	consummed = (u32) gf_bs_get_position(bs) - spos;
	pck_len -= consummed;
	if (full_pck) {
		assert(gpck->full_block_size > consummed);
		gpck->full_block_size -= consummed;
		assert(gpck->full_block_size == pck_len);
		gf_filter_pck_truncate(gpck->pck, gpck->full_block_size);
	}
	copy_size = gpck->full_block_size;
	if (copy_size > pck_len)
		copy_size = pck_len;
	gf_bs_read_data(bs, gpck->output, copy_size);
	gsfdmx_packet_append_frag(gpck, copy_size, 0);

	gf_filter_pck_set_framing(gpck->pck, is_start, is_end);
	if (has_dts) gf_filter_pck_set_dts(gpck->pck, dts);
	if (has_cts) gf_filter_pck_set_cts(gpck->pck, cts);
	if (has_dur) gf_filter_pck_set_duration(gpck->pck, dur);
	if (has_bo) gf_filter_pck_set_byte_offset(gpck->pck, bo);
	if (corr) gf_filter_pck_set_corrupted(gpck->pck, corr);
	if (interl) gf_filter_pck_set_interlaced(gpck->pck, interl);
	if (has_carv) gf_filter_pck_set_carousel_version(gpck->pck, carv);
	if (has_dep) gf_filter_pck_set_dependency_flags(gpck->pck, dep_flags);
	if (cktype) gf_filter_pck_set_clock_type(gpck->pck, cktype);
	if (seek) gf_filter_pck_set_seek_flag(gpck->pck, seek);
	if (crypt) gf_filter_pck_set_crypt_flags(gpck->pck, crypt);
	if (sap) gf_filter_pck_set_sap(gpck->pck, sap);
	if (sap==GF_FILTER_SAP_4) gf_filter_pck_set_roll_info(gpck->pck, roll);
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
	default: return "FORBIDDEN";
	}
}

static GFINLINE void gsfdmx_pck_reset(GSF_Packet *pck)
{
	u32 alloc_frags = pck->nb_alloc_frags;
	GSF_PacketFragment *frags = pck->frags;
	memset(pck, 0, sizeof(GSF_Packet));
	pck->nb_alloc_frags = alloc_frags;
	pck->frags = frags;
}

static void gsfdmx_stream_del(GSF_DemuxCtx *ctx, GSF_Stream *gst, Bool is_flush)
{
	while (gf_list_count(gst->packets)) {
		GSF_Packet *gpck = gf_list_pop_front(gst->packets);

		if (gpck->pck) {
			if (is_flush && (gpck->pck_type == GFS_PCKTYPE_PCK)) {
				gf_filter_pck_set_corrupted(gpck->pck, GF_TRUE);
				gf_filter_pck_send(gpck->pck);
			} else {
				gf_filter_pck_discard(gpck->pck);
			}
		}
		gsfdmx_pck_reset(gpck);
		gf_list_add(ctx->pck_res, gpck);
	}
	if (is_flush)
		gf_filter_pid_remove(gst->opid);

	gf_list_del(gst->packets);
	gf_list_del_item(ctx->streams, gst);
	gf_free(gst);
}

static GF_Err gsfdmx_process_packets(GF_Filter *filter, GSF_DemuxCtx *ctx, GSF_Stream *gst)
{
	GSF_Packet *gpck;
	GF_Err e;

	if (ctx->tune_error) {
		return GF_OK;
	}
	while (1) {
		gpck = gf_list_get(gst->packets, 0);

		if (!gpck || !gpck->complete) {
			u32 pck_count = gf_list_count(gst->packets);
			if (ctx->mq && (pck_count > ctx->mq + 1)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFDemux] packets queue too large (%d vs %d max), processing incomplete packet size %d receive %d\n", pck_count, ctx->mq, gpck->full_block_size, gpck->nb_bytes));
				gpck->corrupted = 1;
			} else {
				return GF_OK;
			}
		}
		assert(gpck->pck);
		if (ctx->use_seq_num) {
			u32 frame_sn;
			if (!gst->last_frame_sn) frame_sn = gpck->frame_sn;
			else {
				frame_sn = gst->last_frame_sn;
				if (frame_sn>0xFFFF) frame_sn=0;
			}
			if (gpck->frame_sn != frame_sn) {
				u32 pck_count = gf_list_count(gst->packets);
				if (ctx->mq && (pck_count > 2*ctx->mq + 1)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[GSFDemux] packets queue too large (%d vs %d max), missed one full packet\n", pck_count, 2*ctx->mq));
				} else {
					return GF_OK;
				}
			}
			gst->last_frame_sn = gpck->frame_sn+1;
		}

		switch (gpck->pck_type) {
		case GFS_PCKTYPE_PID_CONFIG:
		case GFS_PCKTYPE_PID_INFO_UPDATE:
			if (!gpck->corrupted)
				e = gsfdmx_parse_pid_info(filter, ctx, gst, gpck, (gpck->pck_type==GFS_PCKTYPE_PID_INFO_UPDATE) ? GF_TRUE : GF_FALSE);
			else
				e = GF_CORRUPTED_DATA;
			if (gpck->pck) gf_filter_pck_discard(gpck->pck);
			break;
		case GFS_PCKTYPE_PID_REMOVE:
			if (gpck->pck) gf_filter_pck_discard(gpck->pck);
			gsfdmx_stream_del(ctx, gst, GF_TRUE);
			return GF_OK;

		case GFS_PCKTYPE_PID_EOS:
			if (gpck->pck) gf_filter_pck_discard(gpck->pck);
			gf_filter_pid_set_eos(gst->opid);
			break;
		case GFS_PCKTYPE_PCK:
			if (gpck->corrupted) gf_filter_pck_set_corrupted(gpck->pck, GF_TRUE);
			e = gf_filter_pck_send(gpck->pck);
			break;
		}
		gf_list_rem(gst->packets, 0);
		gsfdmx_pck_reset(gpck);
		gf_list_add(ctx->pck_res, gpck);
		if (e) return e;
	}
	return GF_OK;
}

static GF_Err gsfdmx_demux(GF_Filter *filter, GSF_DemuxCtx *ctx, char *data, u32 data_size)
{
	u32 last_pck_end=0;

	//always reset input buffer if not tuned - since in reliable (pipe/file/...) this is the first packet and it is less than 40 bytes at max whe should be fine
	if (!ctx->tuned)
		ctx->buf_size = 0;

	if (ctx->alloc_size < ctx->buf_size + data_size) {
		ctx->buffer = (char*)gf_realloc(ctx->buffer, sizeof(char)*(ctx->buf_size + data_size) );
		ctx->alloc_size = ctx->buf_size + data_size;
	}

	memcpy(ctx->buffer + ctx->buf_size, data, sizeof(char)*data_size);
	ctx->buf_size += data_size;

	gf_bs_reassign_buffer(ctx->bs_r, ctx->buffer, ctx->buf_size);
	while (gf_bs_available(ctx->bs_r) > 4) { //1 byte header + 3 vlen field at least 1 bytes
		GF_Err e = GF_OK;
		u32 pck_len, block_size, block_offset;
		u32 hdr_pos = (u32) gf_bs_get_position(ctx->bs_r);
		/*Bool reserved =*/ gf_bs_read_int(ctx->bs_r, 1);
		u32 frag_flags = gf_bs_read_int(ctx->bs_r, 2);
		Bool is_crypted = gf_bs_read_int(ctx->bs_r, 1);
		u32 pck_type = gf_bs_read_int(ctx->bs_r, 4);
		u16 sn = 0;
		u32 st_idx;
		Bool needs_agg = GF_FALSE;
		u16 frame_sn = 0;
		Bool has_sn = (pck_type==GFS_PCKTYPE_HDR) ? GF_FALSE : ctx->use_seq_num;

		Bool full_pck = (frag_flags==0) ? GF_TRUE : GF_FALSE;
		Bool pck_frag = (frag_flags>=2) ? GF_TRUE : GF_FALSE;

		//reset buffer too small flag, and blindly parse the following vlen fields
		ctx->buffer_too_small = GF_FALSE;

		st_idx = gsfdmx_read_vlen(ctx->bs_r);
		if (has_sn) {
			frame_sn = gf_bs_read_u16(ctx->bs_r);
		}

		block_size = block_offset = 0;
		if (!full_pck) {
			block_size = gsfdmx_read_vlen(ctx->bs_r);
			if (pck_frag) block_offset = gsfdmx_read_vlen(ctx->bs_r);
		}

		pck_len = gsfdmx_read_vlen(ctx->bs_r);
		if (pck_len > gf_bs_available(ctx->bs_r)) {
			break;
		}
		//buffer was not big enough to contain all the vlen fields, we need more data
		if (ctx->buffer_too_small)
			break;
			
		if (full_pck) {
			block_size = pck_len;
			block_offset = 0;
		}

		if ( (pck_type==GFS_PCKTYPE_PCK) && !ctx->nb_playing && ctx->tuned && gf_list_count(ctx->streams) ) {
			ctx->wait_for_play = GF_TRUE;
			break;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFDemux] found %s on stream %d type %s cryped %d sn %d size %d block_offset %d, hdr size %d at position %d\n",
					(pck_len==block_size) ? "full packet" : (pck_frag ? "packet fragment" : "packet start"),
					st_idx,
					gsfdmx_pck_name(pck_type),
					is_crypted, sn, pck_len, block_offset, gf_bs_get_position(ctx->bs_r)-hdr_pos, hdr_pos));

		e = GF_OK;

		if ((pck_type != GFS_PCKTYPE_PCK) && (pck_frag || (pck_len < block_size)))
			needs_agg = GF_TRUE;

		//tunein, we don't care about the seq num (for now, might chenge if we want key roll or other order-dependent features)
		if (!st_idx) {
			if (ctx->tuned) {
			} else if (needs_agg) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] tune-in packet shall not be fragmented\n"));
				e = GF_NON_COMPLIANT_BITSTREAM;
			} else {
				u32 pos = (u32) gf_bs_get_position(ctx->bs_r);
				e = gsfdmx_tune(filter, ctx, ctx->buffer + pos, pck_len, is_crypted);
			}
		}
		//stream signaling or packet
		else {
			u32 cur_pos = (u32) gf_bs_get_position(ctx->bs_r);

			GSF_Stream *gst = gsfdmx_get_stream(filter, ctx, st_idx, pck_type);
			if (!gst) {
				e = GF_OK;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[GSFDemux] cannot find stream idx %d\n", st_idx));
			} else {
				GSF_Packet *gpck = gsfdmx_get_packet(ctx, gst, pck_frag, frame_sn, pck_type, block_size);

				//aggregate data
				if (!gpck) {
					e = GF_OUT_OF_MEM;
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] cannot allocate packet\n"));
				} else if (!gpck->pck) {
					e = GF_CORRUPTED_DATA;
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] lost first packet in frame, cannot reaggregate fragment\n"));
				} else {
					Bool append = GF_TRUE;

					//packet: decrypt on per-packet base, and decode if not first fragment
					if (pck_type==GFS_PCKTYPE_PCK) {
						if (is_crypted) {
							gsfdmx_decrypt(ctx, ctx->buffer + cur_pos, pck_len);
						}
						if (!pck_frag) {
							gf_bs_reassign_buffer(ctx->bs_pck, ctx->buffer + cur_pos, pck_len);
							e = gsfdmx_read_data_pck(ctx, gst, gpck, pck_len, full_pck, ctx->bs_pck);
	 						append = GF_FALSE;
						}
					} else {
						//otherwise decryption will happen upon decoding the complete packet
						if (is_crypted) gpck->crypted = 1;
					}

					if (append) {
						if (block_offset + pck_len > gpck->full_block_size) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] packet fragment out of bounds of current frame (offset %d size %d max size %d)\n", block_offset,  pck_len, gpck->full_block_size));
							e = GF_NON_COMPLIANT_BITSTREAM;
						} else {
							//append fragment
							memcpy(gpck->output + block_offset, ctx->buffer + cur_pos, pck_len);

							gsfdmx_packet_append_frag(gpck, pck_len, block_offset);
						}
					}
					e = gsfdmx_process_packets(filter, ctx, gst);
				}
			}
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[GSFDemux] error decoding packet %s:\n", gf_error_to_string(e) ));
			if (ctx->tune_error) return e;

		}
		gf_bs_skip_bytes(ctx->bs_r, pck_len);
		last_pck_end = (u32) gf_bs_get_position(ctx->bs_r);
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
	GSF_DemuxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GSF_Stream *st;
	u32 i=0, pkt_size;
	const char *data;
	u32 would_block = 0;
	Bool is_eos = GF_FALSE;

	if (ctx->wait_for_play) return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) is_eos = GF_TRUE;
		else return GF_OK;
	}

	//check if all the streams are in block state, if so return.
	//we need to check for all output since one pid could still be buffering
	while ((st = gf_list_enum(ctx->streams, &i))) {
		if (st->opid) {
			if (is_eos) {
				gf_filter_pid_set_eos(st->opid);
			} else if (gf_filter_pid_would_block(st->opid)) {
				would_block++;
			}
		}
	}
	if (is_eos) return GF_EOS;

	if (would_block && (would_block+1==i))
		return GF_OK;

	data = gf_filter_pck_get_data(pck, &pkt_size);
	e = gsfdmx_demux(filter, ctx, (char *) data, pkt_size);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static const char *gsfdmx_probe_data(const u8 *data, u32 data_size, GF_FilterProbeScore *score)
{
	u32 avail = data_size;
	if (data_size < 10) return NULL;
	char *buf = (char *) data;
	while (buf) {
		char *start_sig = memchr(buf, 'G', avail);
		if (!start_sig) return NULL;
		//signature found and version is 1
		if (start_sig && !strncmp(start_sig, "GS5F", 4) && (start_sig[4] == 1)) {
			*score = GF_FPROBE_SUPPORTED;
			return "application/x-gpac-sf";
		}
		buf = start_sig+1;
		avail = data_size - (u32) ( buf - (char *) data);
	}
	return NULL;
}

static void gsfdmx_not_enough_bytes(void *par)
{
	GSF_DemuxCtx *ctx = (GSF_DemuxCtx *)par;
	ctx->buffer_too_small = GF_TRUE;
}

static GF_Err gsfdmx_initialize(GF_Filter *filter)
{
	GSF_DemuxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	if (!ctx->streams) return GF_OUT_OF_MEM;
	ctx->bs_r = gf_bs_new((char *) ctx, 1, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(ctx->bs_r, gsfdmx_not_enough_bytes, ctx);

	ctx->bs_pck = gf_bs_new((char *) ctx, 1, GF_BITSTREAM_READ);
	ctx->pck_res = gf_list_new();
	return GF_OK;
}

static void gsfdmx_finalize(GF_Filter *filter)
{
	GSF_DemuxCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->streams)) {
		GSF_Stream *gst = gf_list_pop_back(ctx->streams);
		gsfdmx_stream_del(ctx, gst, GF_FALSE);
	}
	gf_list_del(ctx->streams);

	while (gf_list_count(ctx->pck_res)) {
		GSF_Packet *gsp = gf_list_pop_back(ctx->pck_res);
		if (gsp->frags) gf_free(gsp->frags);
		gf_free(gsp);
	}
	gf_list_del(ctx->pck_res);

	if (ctx->crypt) gf_crypt_close(ctx->crypt);
	if (ctx->buffer) gf_free(ctx->buffer);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_pck) gf_bs_del(ctx->bs_pck);
}

static const GF_FilterCapability GSFDemuxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-gpac-sf"),
	//we deliver more than these two but this make the filter chain loading stop until we declare a pid
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "gsf"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};



#define OFFS(_n)	#_n, offsetof(GSF_DemuxCtx, _n)
static const GF_FilterArgs GSFDemuxArgs[] =
{
	{ OFFS(key), "key for decrypting packets", GF_PROP_DATA, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(magic), "magic string to check in setup packet", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mq), "set max packet queue length for loss detection. 0 will flush incomplete packet when a new one starts", GF_PROP_UINT, "4", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pad), "byte value used to pad lost packets", GF_PROP_UINT, "0", "0-255", GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister GSFDemuxRegister = {
	.name = "gsfdmx",
	GF_FS_SET_DESCRIPTION("GSF Demuxer")
	GF_FS_SET_HELP("Theis filter provides GSF (__GPAC Super/Simple/Serialized/Stream/State Format__) demultiplexing.\n"
			"It deserializes the stream states (config/reconfig/info update/remove/eos) and packets of input PIDs.\n"\
			"This allows either reading a session saved to file, or receiving the state/data of streams from another instance of GPAC\n"\
			"using either pipes or sockets\n"\
			"\n"\
			"The stream format can be encrypted in AES 128 CBC mode, in which case the demux filters must be given a 128 bit key.")
	.private_size = sizeof(GSF_DemuxCtx),
	.max_extra_pids = (u32) -1,
	.args = GSFDemuxArgs,
	.flags = GF_FS_REG_DYNAMIC_PIDS,
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
