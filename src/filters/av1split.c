/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Copyright (c) Motion Spell, Telecom Paris 2026
 *                      All rights reserved
 *
 *  This file is part of GPAC / AV1 tile split and rewrite filter
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING. If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gpac/filters.h>
#include <gpac/bitstream.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1SPLIT)

typedef struct
{
	GF_FilterPid *pid;
	GF_BitStream *packet;
	GF_FilterPacket *pending;
	u32 x, y, width, height;
	Bool playing;
} AV1SplitTile;

typedef struct
{
	GF_FilterPid *ipid;
	GF_List *tiles;
	AV1State state;
	AV1ParseOffsets offsets;
	GF_BitStream *bs, *rewrite, *payload, *prefix;
	u8 *sequence;
	u32 sequence_size, cfg_crc;
	u64 sequence_size_pos;
	u32 width, height, cols, rows, next_tile;
	Bool config_changed, started, in_error;
} GF_AV1SplitCtx;

static void av1split_seek_bits(GF_BitStream *bs, u64 pos)
{
	gf_bs_seek(bs, pos / 8);
	if (pos % 8) gf_bs_read_int(bs, pos % 8);
}

u64 av1_bit_position(GF_BitStream *bs);
static void av1split_copy_bits(GF_BitStream *src, GF_BitStream *dst, u64 end)
{
	while (av1_bit_position(src) < end) {
		u32 n = (u32) MIN(end - av1_bit_position(src), 32);
		gf_bs_write_int(dst, gf_bs_read_int(src, n), n);
	}
}

/* Validate the length before giving the parser a bitstream bounded to this OBU. */
static GF_Err av1split_obu_size(GF_BitStream *bs, const u8 *data, u32 available, ObuType *type, u32 *size, u32 *header)
{
	Bool extension, has_size;
	u8 temporal, spatial;
	u64 payload;
	GF_Err e;
	gf_bs_reassign_buffer(bs, data, available);
	gf_bs_mark_overflow(bs, GF_TRUE);
	e = gf_av1_parse_obu_header(bs, type, &extension, &has_size, &temporal, &spatial);
	if (e) return e;
	if (!has_size) return GF_NOT_SUPPORTED;
	payload = gf_av1_leb128_read(bs, NULL);
	*header = (u32) gf_bs_get_position(bs);
	if (gf_bs_is_overflow(bs) || *header > available || payload > available - *header)
		return GF_NON_COMPLIANT_BITSTREAM;
	*size = *header + (u32) payload;
	return GF_OK;
}

static void av1split_flush_obu(GF_AV1SplitCtx *ctx, AV1SplitTile *tile, const u8 *header)
{
	u8 *data = NULL;
	u32 size = 0;
	gf_bs_get_content(ctx->payload, &data, &size);
	gf_bs_reassign_buffer(ctx->payload, NULL, 0);
	gf_bs_write_data(tile->packet, header, (header[0] & 4) ? 2 : 1);
	gf_av1_leb128_write(tile->packet, size);
	gf_bs_write_data(tile->packet, data, size);
	gf_free(data);
}

/* Retain the original width/height field lengths, so the sequence header does
   not need any other changes. Frame size overrides use these same lengths. */
static void av1split_sequence(GF_AV1SplitCtx *ctx, GF_BitStream *dst, const u8 *data, u32 size, u32 width, u32 height)
{
	gf_bs_reassign_buffer(ctx->rewrite, data, size);
	av1split_copy_bits(ctx->rewrite, dst, ctx->sequence_size_pos);
	gf_bs_read_int(ctx->rewrite, ctx->state.frame_width_bits_minus_1+1);
	gf_bs_read_int(ctx->rewrite, ctx->state.frame_height_bits_minus_1+1);
	gf_bs_write_int(dst, width-1, ctx->state.frame_width_bits_minus_1+1);
	gf_bs_write_int(dst, height-1, ctx->state.frame_height_bits_minus_1+1);
	av1split_copy_bits(ctx->rewrite, dst, (u64) size * 8);
}

static GF_Err av1split_set_sequence(GF_AV1SplitCtx *ctx, const u8 *data, u32 size)
{
	u8 *copy = gf_malloc(size);
	if (!copy) return GF_OUT_OF_MEM;
	memcpy(copy, data, size);
	if (ctx->sequence) gf_free(ctx->sequence);
	ctx->sequence = copy;
	ctx->sequence_size = size;
	ctx->sequence_size_pos = ctx->offsets.sequence_size;
	ctx->config_changed = GF_TRUE;
	return GF_OK;
}

static GF_Err av1split_config_tile(GF_AV1SplitCtx *ctx, AV1SplitTile *tile, u32 id)
{
	GF_AV1Config cfg = *ctx->state.config;
	GF_AV1_OBUArrayEntry seq = {0};
	u8 *dsi = NULL;
	u32 dsi_size = 0, seq_size = 0, i;
	GF_Err e;
	cfg.obu_array = gf_list_new();
	if (!cfg.obu_array) return GF_OUT_OF_MEM;
	av1split_sequence(ctx, ctx->payload, ctx->sequence, ctx->sequence_size, tile->width, tile->height);
	gf_bs_get_content(ctx->payload, &seq.obu, &seq_size);
	seq.obu_length = seq_size;
	gf_bs_reassign_buffer(ctx->payload, NULL, 0);
	seq.obu_type = OBU_SEQUENCE_HEADER;
	gf_list_add(cfg.obu_array, &seq);
	for (i=0; i<gf_list_count(ctx->state.config->obu_array); i++) {
		GF_AV1_OBUArrayEntry *obu = gf_list_get(ctx->state.config->obu_array, i);
		if (obu->obu_type != OBU_SEQUENCE_HEADER) gf_list_add(cfg.obu_array, obu);
	}
	e = gf_odf_av1_cfg_write(&cfg, &dsi, &dsi_size);
	gf_list_del(cfg.obu_array);
	gf_free(seq.obu);
	if (e) return e;

	char name[64];
	sprintf(name, "tile%u", id);
	gf_filter_pid_copy_properties(tile->pid, ctx->ipid);
	gf_filter_pid_set_name(tile->pid, name);
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_ID, &PROP_UINT(id));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_TILE_ID, &PROP_UINT(id));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_WIDTH, &PROP_UINT(tile->width));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_HEIGHT, &PROP_UINT(tile->height));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(tile->x, tile->y));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(ctx->width, ctx->height));
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_ESID, NULL);
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_DEPENDENCY_ID, NULL);
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_BITRATE, NULL);
	gf_filter_pid_set_property(tile->pid, GF_PROP_PID_UNFRAMED, NULL);
	gf_filter_pid_set_property_str(tile->pid, "av1_split", &PROP_BOOL(GF_TRUE));
	return GF_OK;
}

static GF_Err av1split_grid(GF_Filter *filter, GF_AV1SplitCtx *ctx)
{
	AV1State *s = &ctx->state;
	u32 i, count, sb = s->use_128x128_superblock ? 128 : 64;
	Bool first = !ctx->cols;
	if (!ctx->sequence || !s->tileCols || !s->tileRows || s->tileCols > AV1_MAX_TILE_COLS || s->tileRows > AV1_MAX_TILE_ROWS)
		return GF_NON_COMPLIANT_BITSTREAM;
	if (s->width != s->UpscaledWidth || s->width != s->sequence_width || s->height != s->sequence_height)
		return GF_NOT_SUPPORTED;
	if (!first && (s->tileCols != ctx->cols || s->tileRows != ctx->rows || s->width != ctx->width || s->height != ctx->height))
		return GF_NOT_SUPPORTED;
	ctx->width = s->width;
	ctx->height = s->height;
	ctx->cols = s->tileCols;
	ctx->rows = s->tileRows;
	count = ctx->cols * ctx->rows;
	for (i=0; i<count; i++) {
		u32 col = i % ctx->cols, row = i / ctx->cols;
		u32 x = ctx->offsets.col_start[col] * sb, y = ctx->offsets.row_start[row] * sb;
		u32 right = MIN(ctx->offsets.col_start[col+1] * sb, ctx->width);
		u32 bottom = MIN(ctx->offsets.row_start[row+1] * sb, ctx->height);
		AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
		GF_Err e;
		if (right <= x || bottom <= y) return GF_NON_COMPLIANT_BITSTREAM;
		/* AV1 5.9.15: a standalone frame must be representable by one tile. */
		if (right-x > 4096 || (u64)((right-x+sb-1)/sb) * ((bottom-y+sb-1)/sb) * sb * sb > 4096*2304)
			return GF_NOT_SUPPORTED;
		if (!tile) {
			GF_SAFEALLOC(tile, AV1SplitTile);
			if (!tile) return GF_OUT_OF_MEM;
			tile->packet = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (!tile->packet) { gf_free(tile); return GF_OUT_OF_MEM; }
			gf_list_add(ctx->tiles, tile);
			tile->pid = gf_filter_pid_new(filter);
			if (!tile->pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_udta(tile->pid, tile);
			tile->x = x;
			tile->y = y;
			tile->width = right-x;
			tile->height = bottom-y;
		} else if (tile->x != x || tile->y != y || tile->width != right-x || tile->height != bottom-y) {
			return GF_NOT_SUPPORTED;
		}
		if (first || ctx->config_changed) {
			e = av1split_config_tile(ctx, tile, i+1);
			if (e) return e;
		}
	}
	ctx->config_changed = GF_FALSE;
	if (first) GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[AV1Split] Splitting %ux%u into %ux%u tiles\n", ctx->width, ctx->height, ctx->cols, ctx->rows));
	return GF_OK;
}

static GF_Err av1split_frame_header(GF_AV1SplitCtx *ctx, AV1SplitTile *tile, const u8 *data, u32 size, u32 header, Bool trailing)
{
	AV1ParseOffsets *p = &ctx->offsets;
	AV1State *s = &ctx->state;
	u32 sb = s->use_128x128_superblock ? 128 : 64;
	if (p->header_end > (u64) size*8 || p->header_end < (u64) header*8)
		return GF_NON_COMPLIANT_BITSTREAM;
	gf_bs_reassign_buffer(ctx->rewrite, data, size);
	gf_bs_seek(ctx->rewrite, header);
	if (!s->frame_state.show_existing_frame) {
		if (p->frame_size) {
			av1split_copy_bits(ctx->rewrite, ctx->payload, p->frame_size);
			gf_bs_read_int(ctx->rewrite, s->frame_width_bits_minus_1+1);
			gf_bs_read_int(ctx->rewrite, s->frame_height_bits_minus_1+1);
			gf_bs_write_int(ctx->payload, tile->width-1, s->frame_width_bits_minus_1+1);
			gf_bs_write_int(ctx->payload, tile->height-1, s->frame_height_bits_minus_1+1);
		}
		if (p->render_size) {
			av1split_copy_bits(ctx->rewrite, ctx->payload, p->render_size);
			Bool different = gf_bs_read_int(ctx->rewrite, 1);
			gf_bs_write_int(ctx->payload, different, 1);
			if (different) {
				u32 w = gf_bs_read_int(ctx->rewrite, 16)+1, h = gf_bs_read_int(ctx->rewrite, 16)+1;
				w = MAX(1, (u32) ((u64) w * tile->width / ctx->width));
				h = MAX(1, (u32) ((u64) h * tile->height / ctx->height));
				gf_bs_write_int(ctx->payload, w-1, 16);
				gf_bs_write_int(ctx->payload, h-1, 16);
			}
		}
		av1split_copy_bits(ctx->rewrite, ctx->payload, p->tile_info);
		av1split_seek_bits(ctx->rewrite, p->tile_info_end);
		//uniform spacing, zero column/row increments: exactly one tile
		gf_bs_write_int(ctx->payload, 1, 1);
		if (tile->width > sb) gf_bs_write_int(ctx->payload, 0, 1);
		if (tile->height > sb) gf_bs_write_int(ctx->payload, 0, 1);
	}
	av1split_copy_bits(ctx->rewrite, ctx->payload, p->header_end);
	if (trailing) gf_bs_write_int(ctx->payload, 1, 1);
	gf_bs_align(ctx->payload);
	return GF_OK;
}

static GF_Err av1split_prefix(GF_AV1SplitCtx *ctx)
{
	u8 *data = NULL;
	u32 size = 0, pos = 0, i;
	GF_Err e = GF_OK;
	gf_bs_get_content(ctx->prefix, &data, &size);
	gf_bs_reassign_buffer(ctx->prefix, NULL, 0);
	while (pos < size) {
		ObuType type;
		u32 len, header;
		e = av1split_obu_size(ctx->rewrite, data+pos, size-pos, &type, &len, &header);
		if (e) break;
		for (i=0; i<gf_list_count(ctx->tiles); i++) {
			AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
			if (type == OBU_SEQUENCE_HEADER)
				av1split_sequence(ctx, tile->packet, ctx->sequence, ctx->sequence_size, tile->width, tile->height);
			else
				gf_bs_write_data(tile->packet, data+pos, len);
		}
		pos += len;
	}
	gf_free(data);
	return e;
}

static GF_Err av1split_packet(GF_Filter *filter, GF_AV1SplitCtx *ctx, const u8 *data, u32 size)
{
	u32 pos = 0;
	while (pos < size) {
		ObuType type;
		u32 len, header, i;
		u64 parsed_size;
		AV1State *s = &ctx->state;
		GF_Err e = av1split_obu_size(ctx->bs, data+pos, size-pos, &type, &len, &header);
		if (e) return e;
		if (type == OBU_REDUNDANT_FRAME_HEADER || type == OBU_TILE_LIST) return GF_NOT_SUPPORTED;
		if (type == OBU_FRAME || type == OBU_FRAME_HEADER) {
			if (s->frame_state.seen_frame_header || ctx->next_tile) return GF_NON_COMPLIANT_BITSTREAM;
			if (!ctx->sequence) return GF_NON_COMPLIANT_BITSTREAM;
		} else if (type == OBU_TILE_GROUP && !s->frame_state.seen_frame_header) {
			return GF_NON_COMPLIANT_BITSTREAM;
		} else if ((type == OBU_TEMPORAL_DELIMITER || type == OBU_SEQUENCE_HEADER) && s->frame_state.seen_frame_header) {
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_bs_reassign_buffer(ctx->bs, data+pos, len);
		parsed_size = len;
		e = gf_av1_parse_obu(ctx->bs, &type, &parsed_size, NULL, s);
		if (e || gf_bs_is_overflow(ctx->bs)) return e ? e : GF_NON_COMPLIANT_BITSTREAM;
		if (s->spatial_id) return GF_NOT_SUPPORTED;
		if (type == OBU_SEQUENCE_HEADER) {
			e = av1split_set_sequence(ctx, data+pos, len);
			if (e) return e;
		}
		if ((type == OBU_FRAME || type == OBU_FRAME_HEADER) && !s->frame_state.show_existing_frame) {
			e = av1split_grid(filter, ctx);
			if (e) return e;
			e = av1split_prefix(ctx);
			if (e) return e;
		}
		if (!ctx->cols) {
			if (type == OBU_FRAME || type == OBU_FRAME_HEADER || type == OBU_TILE_GROUP) return GF_NON_COMPLIANT_BITSTREAM;
			gf_bs_write_data(ctx->prefix, data+pos, len);
			pos += len;
			continue;
		}
		Bool has_tiles = type == OBU_TILE_GROUP || (type == OBU_FRAME && !s->frame_state.show_existing_frame);
		u32 first = 0, last = gf_list_count(ctx->tiles);
		if (has_tiles) {
			first = ctx->offsets.tile_group_start;
			last = first + s->frame_state.nb_tiles_in_obu;
			if (first != ctx->next_tile || last <= first || last > gf_list_count(ctx->tiles)) return GF_NON_COMPLIANT_BITSTREAM;
			ctx->next_tile = (last == gf_list_count(ctx->tiles)) ? 0 : last;
		}
		for (i=first; i<last; i++) {
			AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
			if (type == OBU_SEQUENCE_HEADER) {
				av1split_sequence(ctx, tile->packet, data+pos, len, tile->width, tile->height);
				continue;
			}
			if (type == OBU_FRAME || type == OBU_FRAME_HEADER) {
				e = av1split_frame_header(ctx, tile, data+pos, len, header, type == OBU_FRAME_HEADER);
				if (e) return e;
			}
			if (has_tiles) {
				AV1Tile *t = &s->frame_state.tiles[i-first];
				if (!t->size || t->obu_start_offset > len || t->size > len-t->obu_start_offset) return GF_NON_COMPLIANT_BITSTREAM;
				gf_bs_write_data(ctx->payload, data+pos+t->obu_start_offset, t->size);
			}
			if (has_tiles || type == OBU_FRAME_HEADER || type == OBU_FRAME)
				av1split_flush_obu(ctx, tile, data+pos);
			else
				gf_bs_write_data(tile->packet, data+pos, len);
		}
		pos += len;
	}
	//Input PIDs are framed temporal units: never emit an incomplete tile frame.
	if (ctx->next_tile || ctx->state.frame_state.seen_frame_header) return GF_NON_COMPLIANT_BITSTREAM;
	return GF_OK;
}

static void av1split_remove_tiles(GF_AV1SplitCtx *ctx, Bool remove_pids)
{
	while (gf_list_count(ctx->tiles)) {
		AV1SplitTile *tile = gf_list_pop_back(ctx->tiles);
		if (tile->pending) gf_filter_pck_discard(tile->pending);
		if (tile->pid && remove_pids) {
			gf_filter_pid_set_udta(tile->pid, NULL);
			gf_filter_pid_remove(tile->pid);
		}
		gf_bs_del(tile->packet);
		gf_free(tile);
	}
	ctx->cols = ctx->rows = ctx->next_tile = 0;
}

static GF_Err av1split_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_AV1SplitCtx *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *dsi;
	u32 i, crc;
	if (is_remove) {
		av1split_remove_tiles(ctx, GF_TRUE);
		gf_av1_reset_state(&ctx->state, GF_TRUE);
		gf_odf_av1_cfg_del(ctx->state.config);
		gf_av1_init_state(&ctx->state);
		ctx->state.config = gf_odf_av1_cfg_new();
		ctx->state.parse_offsets = &ctx->offsets;
		gf_free(ctx->sequence);
		ctx->sequence = NULL;
		ctx->sequence_size = ctx->cfg_crc = 0;
		gf_bs_seek(ctx->prefix, 0);
		gf_bs_seek(ctx->payload, 0);
		ctx->ipid = NULL;
		ctx->started = GF_FALSE;
		ctx->in_error = GF_FALSE;
		return ctx->state.config ? GF_OK : GF_OUT_OF_MEM;
	}
	if (!gf_filter_pid_check_caps(pid) || (ctx->ipid && ctx->ipid != pid)) return GF_NOT_SUPPORTED;
	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	crc = dsi ? gf_crc_32(dsi->value.data.ptr, dsi->value.data.size) : 0;
	ctx->config_changed = GF_TRUE;
	if (dsi && (!ctx->sequence || crc != ctx->cfg_crc)) {
		GF_AV1Config *cfg = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		gf_odf_av1_cfg_del(ctx->state.config);
		gf_av1_reset_state(&ctx->state, GF_TRUE);
		gf_av1_init_state(&ctx->state);
		ctx->state.config = cfg;
		ctx->state.parse_offsets = &ctx->offsets;
		gf_free(ctx->sequence);
		ctx->sequence = NULL;
		ctx->sequence_size = 0;
		ctx->cfg_crc = crc;
		for (i=0; i<gf_list_count(cfg->obu_array); i++) {
			GF_AV1_OBUArrayEntry *obu = gf_list_get(cfg->obu_array, i);
			ObuType type;
			u64 size = obu->obu_length;
			GF_Err e;
			if (obu->obu_type != OBU_SEQUENCE_HEADER) continue;
			gf_bs_reassign_buffer(ctx->bs, obu->obu, obu->obu_length);
			e = gf_av1_parse_obu(ctx->bs, &type, &size, NULL, &ctx->state);
			if (e) return e;
			e = av1split_set_sequence(ctx, obu->obu, obu->obu_length);
			if (e) return e;
		}
	}
	if (!ctx->started) {
		//The grid is carried in frame headers. Fetch the first TU to expose PIDs.
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
		ctx->started = GF_TRUE;
	}
	return GF_OK;
}

static Bool av1split_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_AV1SplitCtx *ctx = gf_filter_get_udta(filter);
	AV1SplitTile *tile = evt->base.on_pid ? gf_filter_pid_get_udta(evt->base.on_pid) : NULL;
	if (evt->base.type == GF_FEVT_PLAY && tile) {
		Bool initial = !tile->playing;
		tile->playing = GF_TRUE;
		//Do not restart the source when each newly discovered output connects.
		if (initial && evt->play.start_range == 0) return GF_TRUE;
	}
	if (evt->base.type == GF_FEVT_STOP && tile) {
		u32 i;
		tile->playing = GF_FALSE;
		for (i=0; i<gf_list_count(ctx->tiles); i++) {
			AV1SplitTile *other = gf_list_get(ctx->tiles, i);
			if (other->playing) return GF_TRUE;
		}
	}
	return GF_FALSE;
}

static GF_Err av1split_process(GF_Filter *filter)
{
	GF_AV1SplitCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	const u8 *data;
	u32 size, i;
	GF_Err e;
	if (!ctx->ipid) return GF_OK;
	if (ctx->in_error) return GF_EOS;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (!gf_filter_pid_is_eos(ctx->ipid)) return GF_OK;
		for (i=0; i<gf_list_count(ctx->tiles); i++) {
			AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
			gf_filter_pid_set_eos(tile->pid);
		}
		return GF_EOS;
	}
	data = gf_filter_pck_get_data(pck, &size);
	if (!data) { gf_filter_pid_drop_packet(ctx->ipid); return GF_OK; }
	e = av1split_packet(filter, ctx, data, size);
	if (e) goto error;
	//Prepare all outputs before sending any. Keep the source packet alive until
	//every output has copied its properties, then consume it exactly once.
	for (i=0; i<gf_list_count(ctx->tiles); i++) {
		AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
		u8 *out = NULL, *dst;
		u32 out_size = 0;
		GF_FilterPacket *p;
		gf_bs_get_content(tile->packet, &out, &out_size);
		gf_bs_reassign_buffer(tile->packet, NULL, 0);
		if (!out_size) { gf_free(out); continue; }
		p = gf_filter_pck_new_alloc(tile->pid, out_size, &dst);
		if (!p) { gf_free(out); e = GF_OUT_OF_MEM; goto error; }
		tile->pending = p;
		memcpy(dst, out, out_size);
		gf_free(out);
		gf_filter_pck_merge_properties(pck, p);
		gf_filter_pck_set_byte_offset(p, GF_FILTER_NO_BO);
		gf_filter_pck_set_framing(p, GF_TRUE, GF_TRUE);
	}
	for (i=0; i<gf_list_count(ctx->tiles); i++) {
		AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
		if (tile->pending) gf_filter_pck_send(tile->pending);
		tile->pending = NULL;
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;

error:
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Split] Cannot split temporal unit: %s\n", gf_error_to_string(e)));
	for (i=0; i<gf_list_count(ctx->tiles); i++) {
		AV1SplitTile *tile = gf_list_get(ctx->tiles, i);
		if (tile->pending) gf_filter_pck_discard(tile->pending);
		tile->pending = NULL;
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	ctx->in_error = GF_TRUE;
	gf_filter_abort(filter);
	return e;
}

static GF_Err av1split_initialize(GF_Filter *filter)
{
	GF_AV1SplitCtx *ctx = gf_filter_get_udta(filter);
	gf_av1_init_state(&ctx->state);
	ctx->state.config = gf_odf_av1_cfg_new();
	ctx->state.parse_offsets = &ctx->offsets;
	ctx->tiles = gf_list_new();
	ctx->bs = gf_bs_new((u8 *) ctx, 1, GF_BITSTREAM_READ);
	ctx->rewrite = gf_bs_new((u8 *) ctx, 1, GF_BITSTREAM_READ);
	ctx->payload = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ctx->prefix = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!ctx->state.config || !ctx->tiles || !ctx->bs || !ctx->rewrite || !ctx->payload || !ctx->prefix) return GF_OUT_OF_MEM;
	return GF_OK;
}

static void av1split_finalize(GF_Filter *filter)
{
	GF_AV1SplitCtx *ctx = gf_filter_get_udta(filter);
	av1split_remove_tiles(ctx, GF_FALSE);
	gf_list_del(ctx->tiles);
	gf_av1_reset_state(&ctx->state, GF_TRUE);
	gf_odf_av1_cfg_del(ctx->state.config);
	gf_bs_del(ctx->bs);
	gf_bs_del(ctx->rewrite);
	gf_bs_del(ctx->payload);
	gf_bs_del(ctx->prefix);
	gf_free(ctx->sequence);
}

static const GF_FilterCapability AV1SplitCaps[] = {
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};

GF_FilterRegister AV1SplitRegister = {
	.name = "av1split",
	GF_FS_SET_DESCRIPTION("AV1 Tile extractor")
	GF_FS_SET_HELP("This filter splits a tiled AV1 PID into independent AV1 PIDs, one per tile in row-major order.\n"
		"The input must contain independently coded tiles, typically produced by av1merge. Motion prediction, entropy adaptation and loop restoration coding must not depend on other tiles. Ordinary tiled AV1 encodes may not satisfy these constraints.\n"
		"Each output has a rewritten decoder configuration and frame headers, a 1-based TileID, CropOrigin and OrigSize properties. Packet timestamps and properties are preserved.\n"
		"The tile grid and frame dimensions must remain constant, with frame dimensions matching the sequence dimensions. Super-resolution, spatial scalability, redundant frame headers and tile lists are not supported.\n"
		"Example: gpac -i merged.av1 av1split -o 'tile_$TileID$.obu'")
	.private_size = sizeof(GF_AV1SplitCtx),
	SETCAPS(AV1SplitCaps),
	.flags = GF_FS_REG_EXPLICIT_ONLY | GF_FS_REG_DYNAMIC_PIDS,
	.initialize = av1split_initialize,
	.finalize = av1split_finalize,
	.configure_pid = av1split_configure_pid,
	.process_event = av1split_process_event,
	.process = av1split_process,
	.hint_class_type = GF_FS_CLASS_STREAM
};

const GF_FilterRegister *av1split_register(GF_FilterSession *session)
{
	return &AV1SplitRegister;
}
#else
const GF_FilterRegister *av1split_register(GF_FilterSession *session)
{
	return NULL;
}
#endif
