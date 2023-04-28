/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / uncv pixel format translator filter
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
#include <gpac/avparse.h>

enum {
	SAMPLING_NONE=0,
	SAMPLING_422=1,
	SAMPLING_420=2,
	SAMPLING_411=3,
};

enum {
	INTERLEAVE_COMPONENT=0,
	INTERLEAVE_PIXEL=1,
	INTERLEAVE_MIXED=2,
	INTERLEAVE_ROW=3,
	INTERLEAVE_TILE=4,
	INTERLEAVE_MULTIY=5,
};

enum
{
	UNCV_OUT_NONE=0,
	UNCV_OUT_YUV=1,
	UNCV_OUT_RGB,
	UNCV_OUT_MONO,
};

typedef struct
{
	u32 type;
	char *uri;
} UNCVComponentDefinition;

typedef struct
{
	u16 idx;
	u32 bits, format, align_size;
	u32 type; //copied from def

	//internal
	u64 max_val;
	s32 p_idx;
	u32 line_size, plane_size;
	u32 row_align_size;
	u32 comp_idx;
	u8 value;
} UNCVComponentInfo;

typedef struct
{
	u32 nb_comps;
	UNCVComponentInfo *comps;
	u32 nb_values;
	u8 *values;

} UNCVPalette;

typedef struct
{
	u32 nb_comp_defs;
	UNCVComponentDefinition *comp_defs;
	UNCVPalette *palette;

	u8 version;
	u32 flags;
	u32 profile;
	u32 nb_comps;
	UNCVComponentInfo *comps;
	u32 sampling, interleave, block_size;
	Bool components_little_endian, block_pad_lsb, block_little_endian, block_reversed, pad_unknown;
	u32 pixel_size;
	u32 row_align_size, tile_align_size, num_tile_cols, num_tile_rows;


	u16 fa_width, fa_height;
	u16 *fa_map;
} UNCVConfig;

typedef struct
{
	UNCVComponentInfo *component;
	u32 val;
} BlockComp;

typedef struct
{
	GF_BitStream *bs;
	u32 init_offset;
	u32 line_start_pos;
	u32 row_align_size;
	u32 plane_size, tile_size, comp_row_size;

	u32 loaded_comps;
	u32 first_comp_idx;

	u8 *le_buf;
	GF_BitStream *le_bs;
	BlockComp *block_comps;
} BSRead;

typedef struct __uncvdec
{
	Bool force_pf, no_tile;

	GF_FilterPid *ipid, *opid;
	u32 width, height, pixel_format, bpp, stride;
	Bool passthrough;

	u32 output_size, tile_width, tile_height;

	UNCVConfig *cfg;
	u32 tile_size, row_line_size;
	u32 color_type;
	Bool alpha;
	u32 blocksize_bits;

	u32 subsample_x;
	BSRead *bsrs;
	u32 nb_bsrs;
	u32 max_comp_per_block;
	Bool use_palette;
	u8 *pixel;

	u8 *comp_le_buf;
	GF_BitStream *comp_le_bs;

	void (*read_pixel)(struct __uncvdec* ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
} UNCVDecCtx;

static u8 uncv_get_val(GF_BitStream *bs, UNCVComponentInfo *comp, UNCVDecCtx *ctx);

static void read_pixel_interleave_comp_yuv_420(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_comp_yuv(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_comp(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_pixel(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_mixed(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_multiy(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_comp(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);
static void read_pixel_interleave_comp(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *output, u32 offset);

static void uncv_del(UNCVConfig *cfg)
{
	u32 i;
	if (cfg->comp_defs) {
		for (i=0; i<cfg->nb_comp_defs; i++) {
			if (cfg->comp_defs[i].uri) gf_free(cfg->comp_defs[i].uri);
		}
		gf_free(cfg->comp_defs);
	}
	if (cfg->comps) gf_free(cfg->comps);
	if (cfg->palette) {
		if (cfg->palette->comps) gf_free(cfg->palette->comps);
		if (cfg->palette->values) gf_free(cfg->palette->values);
		gf_free(cfg->palette);
	}
	gf_free(cfg);
}


static UNCVConfig *uncv_parse_config(u8 *dsi, u32 dsi_size, GF_Err *out_err)
{
	Bool has_pal=GF_FALSE;
	Bool has_fa=GF_FALSE;
	*out_err = GF_OK;
	u32 i;
	Bool has_cmpd=0, has_uncc=0;
	if (!dsi) {
		*out_err = GF_BAD_PARAM;
		return NULL;
	}
	UNCVConfig *uncv;
	GF_SAFEALLOC(uncv, UNCVConfig);
	GF_BitStream *bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		s32 size = gf_bs_read_u32(bs);
		u32 type = gf_bs_read_u32(bs);
		u32 pos = (u32) gf_bs_get_position(bs);

		if (type==GF_4CC('c','m','p','d')) {
			uncv->nb_comp_defs = gf_bs_read_u16(bs);
			uncv->comp_defs = gf_malloc(sizeof(UNCVComponentDefinition) * uncv->nb_comp_defs);
			memset(uncv->comp_defs, 0, sizeof(UNCVComponentDefinition) * uncv->nb_comp_defs);
			if (!uncv->comp_defs) {
				*out_err = GF_OUT_OF_MEM;
				break;
			}
			for (i=0; i<uncv->nb_comp_defs; i++) {
				uncv->comp_defs[i].type = gf_bs_read_u16(bs);
				if (uncv->comp_defs[i].type>=0x8000) {
					uncv->comp_defs[i].uri = gf_bs_read_utf8(bs);
				}
			}
			has_cmpd=1;
		} else if (type==GF_4CC('u','n','c','C')) {
			uncv->version = gf_bs_read_u8(bs);
			uncv->flags = gf_bs_read_int(bs, 24);
			uncv->profile = gf_bs_read_u32(bs);
			//todo, validate profile ?

			uncv->nb_comps = gf_bs_read_u16(bs);
			uncv->comps = gf_malloc(sizeof(UNCVComponentInfo) * uncv->nb_comps);
			memset(uncv->comps, 0, sizeof(UNCVComponentInfo) * uncv->nb_comps);
			for (i=0; i<uncv->nb_comps; i++) {
				uncv->comps[i].idx = gf_bs_read_u16(bs);
				uncv->comps[i].bits = 1 + gf_bs_read_u8(bs);
				uncv->comps[i].format = gf_bs_read_u8(bs);
				uncv->comps[i].align_size = gf_bs_read_u8(bs);
			}
			uncv->sampling = gf_bs_read_u8(bs);
			uncv->interleave = gf_bs_read_u8(bs);
			uncv->block_size = gf_bs_read_u8(bs);
			uncv->components_little_endian = gf_bs_read_int(bs, 1);
			uncv->block_pad_lsb = gf_bs_read_int(bs, 1);
			uncv->block_little_endian = gf_bs_read_int(bs, 1);
			uncv->block_reversed = gf_bs_read_int(bs, 1);
			uncv->pad_unknown = gf_bs_read_int(bs, 1);
			gf_bs_read_int(bs, 3);
			uncv->pixel_size = gf_bs_read_u8(bs);
			uncv->row_align_size = gf_bs_read_u32(bs);
			uncv->tile_align_size = gf_bs_read_u32(bs);
			uncv->num_tile_cols = 1 + gf_bs_read_u32(bs);
			uncv->num_tile_rows = 1 + gf_bs_read_u32(bs);
			has_uncc = 1;
		} else if (type==GF_4CC('c','p','a','l')) {
			u32 bit_size=0;
			u32 bsize=size;
			u32 max_bits=0;
			u8 version = gf_bs_read_u8(bs);
			if (version)
				*out_err = GF_NON_COMPLIANT_BITSTREAM;

			gf_bs_read_int(bs, 24);
			GF_SAFEALLOC(uncv->palette, UNCVPalette);
			uncv->palette->nb_comps = gf_bs_read_u16(bs);
			bsize-=6;
			uncv->palette->comps = gf_malloc(sizeof(UNCVComponentInfo)*uncv->palette->nb_comps);
			memset(uncv->palette->comps, 0, sizeof(UNCVComponentInfo)*uncv->palette->nb_comps);
			for (i=0; i<uncv->palette->nb_comps; i++) {
				UNCVComponentInfo *comp = &uncv->palette->comps[i];
				comp->idx = gf_bs_read_u16(bs);
				comp->bits = gf_bs_read_u8(bs) + 1;
				comp->format = gf_bs_read_u8(bs);
				if (!comp->format && (max_bits<comp->bits))
					max_bits = comp->bits;
				bsize-=4;
				bit_size+=comp->bits;
				while (bit_size%8) bit_size++;
			}
			uncv->palette->nb_values = gf_bs_read_u32(bs);
			bsize-=4;
			if (bsize<0)
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
			if (bsize*8 < uncv->palette->nb_values * bit_size)
				*out_err = GF_NON_COMPLIANT_BITSTREAM;

			uncv->palette->values = gf_malloc(sizeof(u8) * uncv->palette->nb_values * uncv->palette->nb_comps);
			for (i=0; i<uncv->palette->nb_values; i++) {
				u8 *vals = uncv->palette->values + i*uncv->palette->nb_comps;
				for (u32 j=0; j<uncv->palette->nb_comps; j++) {
					UNCVComponentInfo *c = &uncv->palette->comps[j];
					vals[j] = uncv_get_val(bs, c, NULL);
					gf_bs_align(bs);
				}
			}
		} else if (type==GF_4CC('c','p','a','t')) {
			u8 version = gf_bs_read_u8(bs);
			if (version)
				*out_err = GF_NON_COMPLIANT_BITSTREAM;

			gf_bs_read_int(bs, 24);
			uncv->fa_width = gf_bs_read_u16(bs);
			uncv->fa_height = gf_bs_read_u16(bs);
			if (uncv->fa_width * uncv->fa_height * 6 > size-8) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid pattern box\n"));
				goto exit;
			}
			uncv->fa_map = gf_malloc(sizeof(u16)*uncv->fa_width * uncv->fa_height);
			for (i=0; i<uncv->fa_height; i++) {
				u32 j;
				for (j=0; j<uncv->fa_width; j++) {
					uncv->fa_map[j + i*uncv->fa_height] = gf_bs_read_u16(bs);
					gf_bs_read_float(bs);
				}
			}
		}
		pos = (u32) gf_bs_get_position(bs) - pos;
		pos += 8;
		if ((pos > (u32) size) || gf_bs_is_overflow(bs) )
			*out_err = GF_NON_COMPLIANT_BITSTREAM;
		else if (pos < (u32) size)
			gf_bs_skip_bytes(bs, size-pos);
	}
	gf_bs_del(bs);

	if (!has_cmpd) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Missing cmpd box\n"));
		goto exit;
	}
	if (!has_uncc) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Missing uncC box\n"));
		goto exit;
	}

	for (i=0; i<uncv->nb_comps; i++) {
		if (uncv->comps[i].idx>=uncv->nb_comp_defs) {
			*out_err = GF_NON_COMPLIANT_BITSTREAM;
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid component index %d\n", uncv->comps[i].idx))
		}
		uncv->comps[i].type = uncv->comp_defs[uncv->comps[i].idx].type;
		if (uncv->comps[i].type == 10) has_pal = 1;
		if (uncv->comps[i].type == 11) has_fa = 1;
		if (uncv->comps[i].align_size && (uncv->comps[i].align_size*8 < uncv->comps[i].bits)) {
			*out_err = GF_NON_COMPLIANT_BITSTREAM;
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Component idx %d align size (%d) less than its bit depth (%d)\n", uncv->comps[i].idx, uncv->comps[i].align_size, uncv->comps[i].bits));
		}
		if (uncv->block_size) {
			if (uncv->comps[i].align_size && (uncv->block_size<uncv->comps[i].align_size)) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Component idx %d align size (%d) is larger than block size\n", uncv->comps[i].idx, uncv->comps[i].align_size, uncv->block_size));
			}
			if (!uncv->comps[i].align_size && (uncv->block_size*8<uncv->comps[i].bits)) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Component idx %d bit depth (%d) is larger than block size\n", uncv->comps[i].idx, uncv->comps[i].bits, uncv->block_size));
			}
		}
	}

	if (uncv->block_reversed && !uncv->block_little_endian) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Usage of block_reversed with big-endian block is forbidden\n"));
	}

	if (has_pal && !uncv->palette) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Missing palette box\n"));
	}
	else if (uncv->palette) {
		for (i=0; i<uncv->palette->nb_comps; i++) {
			if (uncv->palette->comps[i].idx>=uncv->nb_comp_defs) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid component index %d in palette\n", uncv->comps[i].idx))
			}
			uncv->palette->comps[i].type = uncv->comp_defs[uncv->palette->comps[i].idx].type;
			if (uncv->palette->comps[i].type == 10) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Palette cannot refer to palette component type"));
			}
			else if (uncv->palette->comps[i].type == 11) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Palette cannot refer to filter array type"));
			}
		}
	}

	if (has_fa && !uncv->fa_map) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Missing pattern box\n"));
	} else if (uncv->fa_map) {
		u32 nb_maps = uncv->fa_width * uncv->fa_height;
		for (i=0; i<nb_maps; i++) {
			if (uncv->fa_map[i] >= uncv->nb_comp_defs) {
				*out_err = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid component index %d in palette\n", uncv->comps[i].idx))
			}
		}
	}

	if (uncv->sampling > SAMPLING_411) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Unrecognized sampling mode %d", uncv->sampling));
	}
	if (uncv->interleave > INTERLEAVE_MULTIY) {
		*out_err = GF_NON_COMPLIANT_BITSTREAM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Unrecognized  interleave mode %d", uncv->interleave));
	}

exit:
	if (*out_err) {
		uncv_del(uncv);
		uncv = NULL;
	}
	return uncv;
}


GF_Err rfc_6381_get_codec_uncv(char *szCodec, u32 subtype, u8 *dsi, u32 dsi_size)
{
	GF_Err e;
	u32 i;
	if (!dsi) {
		snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
		return GF_OK;
	}

	UNCVConfig *uncv = uncv_parse_config(dsi, dsi_size, &e);
	if (!uncv) return e;

	if (!uncv->profile) {
		snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s.gene.%x.%x.%x.%xT%x", gf_4cc_to_str(subtype)
			, uncv->sampling, uncv->interleave, uncv->block_size, uncv->num_tile_cols, uncv->num_tile_rows);

		for (i=0; i<uncv->nb_comps; i++) {
			char szComp[100];
			if (uncv->comps[i].idx>uncv->nb_comp_defs) continue;
			sprintf(szComp, ".%xL%x", uncv->comps[i].type, uncv->comps[i].bits);
			if (strlen(szCodec) + strlen(szComp) + 1 <= RFC6381_CODEC_NAME_SIZE_MAX) {
				strcat(szCodec, szComp);
			} else {
				break;
			}
		}
	} else {
		snprintf(szCodec, RFC6381_CODEC_NAME_SIZE_MAX, "%s", gf_4cc_to_str(subtype));
	}
	uncv_del(uncv);
	return e;
}

static void uncv_check_comp_type(u32 type, Bool *has_mono, Bool *has_yuv, Bool *has_rgb, Bool *has_alpha, Bool *has_depth, Bool *has_disp, Bool *has_pal, Bool *has_fa, Bool *has_pad, Bool *has_non_int)
{
	switch (type) {
	case 0: *has_mono=1; break;
	case 1:
	case 2:
	case 3:
		*has_yuv=1;
		break;
	case 4:
	case 5:
	case 6:
		*has_rgb=1;
		break;
	case 7: *has_alpha = 1; break;
	case 8: *has_depth = 1; break;
	case 9: *has_disp = 1; break;
	case 10: *has_pal = 1; break;
	case 11: *has_fa = 1; break;
	case 12: *has_pad = 1; break;
	}
}
static void uncv_check_comps_type(UNCVComponentInfo *comps, u32 nb_comps, Bool *has_mono, Bool *has_yuv, Bool *has_rgb, Bool *has_alpha, Bool *has_depth, Bool *has_disp, Bool *has_pal, Bool *has_fa, Bool *has_pad, Bool *has_non_int)
{
	u32 i;
	for (i=0;i<nb_comps; i++) {
		uncv_check_comp_type(comps[i].type, has_mono, has_yuv, has_rgb, has_alpha, has_depth, has_disp, has_pal, has_fa, has_pad, has_non_int);
		if (comps[i].format) *has_non_int = 1;
	}
}

static u32 uncv_get_compat(UNCVDecCtx *ctx)
{
	Bool has_mono=0, has_yuv=0, has_rgb=0, has_alpha=0, has_depth=0, has_disp=0, has_pal=0, has_fa=0, has_pad=0;
	Bool has_non_int=0;
	UNCVConfig *cfg = ctx->cfg;
	UNCVComponentInfo *c = cfg->comps;

	uncv_check_comps_type(cfg->comps, cfg->nb_comps, &has_mono, &has_yuv, &has_rgb, &has_alpha, &has_depth, &has_disp, &has_pal, &has_fa, &has_pad, &has_non_int);

	if (has_pal) {
		assert(cfg->palette);
		uncv_check_comps_type(cfg->palette->comps, cfg->palette->nb_comps, &has_mono, &has_yuv, &has_rgb, &has_alpha, &has_depth, &has_disp, &has_pal, &has_fa, &has_pad, &has_non_int);
	}
	if (cfg->fa_map) {
		u32 i, nb_map = cfg->fa_width*cfg->fa_height;
		for (i=0;i<nb_map; i++) {
			u32 type = cfg->comp_defs[cfg->fa_map[i]].type;
			uncv_check_comp_type(type, &has_mono, &has_yuv, &has_rgb, &has_alpha, &has_depth, &has_disp, &has_pal, &has_fa, &has_pad, &has_non_int);
		}
	}

	if (has_yuv) ctx->color_type = UNCV_OUT_YUV;
	else if (has_rgb) ctx->color_type = UNCV_OUT_RGB;
	else if (has_mono) ctx->color_type = UNCV_OUT_MONO;
	else ctx->color_type = UNCV_OUT_NONE;
	ctx->alpha = has_alpha;

	if (has_non_int) return 0;
	if (has_pal) {
		ctx->use_palette=GF_TRUE;
		return 0;
	}
	if (has_fa) return 0;
	if (cfg->row_align_size) return 0;
	if (cfg->tile_align_size) return 0;
	if (cfg->pixel_size) return 0;
	if (cfg->num_tile_cols>1) return 0;
	if (cfg->num_tile_rows>1) return 0;
	if (cfg->components_little_endian) return 0;

	if (has_mono) {
		if ((cfg->nb_comps==1) && (c[0].bits==7)) return GF_PIXEL_GREYSCALE;
		if (has_alpha && (cfg->nb_comps==2) && (c[0].bits==7) && (c[1].bits==7)) {
			if (c[0].type==0) return GF_PIXEL_GREYALPHA;
			return GF_PIXEL_ALPHAGREY;
		}
		return 0;
	}

	if (cfg->nb_comps==3) {
		if ((c[0].type==4) && (c[0].bits==8) && (c[1].type==5) && (c[1].bits==8) && (c[2].type==6) && (c[2].bits==8)
			&& (!cfg->block_size || (cfg->block_size==3))
			&& (cfg->interleave==INTERLEAVE_PIXEL) && !cfg->block_little_endian && !cfg->block_reversed
		) {
			return GF_PIXEL_RGB;
		}
		if ((c[0].type==6) && (c[0].bits==8) && (c[1].type==5) && (c[1].bits==8) && (c[2].type==4) && (c[2].bits==8)
			&& (!cfg->block_size || (cfg->block_size==3))
			&& (cfg->interleave==INTERLEAVE_PIXEL) && !cfg->block_little_endian
			&& !cfg->block_reversed && !cfg->sampling
		) {
			return GF_PIXEL_BGR;
		}
		if ((c[0].type==4) && (c[0].bits==5) && (c[1].type==5) && (c[1].bits==6) && (c[2].type==6) && (c[2].bits==5)
			&& (!cfg->block_size || (cfg->block_size==2))
			&& (cfg->interleave==INTERLEAVE_PIXEL) && !cfg->block_little_endian
			&& !cfg->block_reversed && !cfg->sampling
		) {
			return GF_PIXEL_RGB_565;
		}
		if ((c[0].type==4) && (c[0].bits==5) && (c[1].type==5) && (c[1].bits==5) && (c[2].type==6) && (c[2].bits==5)
			&& (cfg->block_size==2) && (cfg->interleave==INTERLEAVE_PIXEL)
			&& !cfg->block_little_endian && !cfg->block_pad_lsb
			&& !cfg->block_reversed && !cfg->sampling
		) {
			return GF_PIXEL_RGB_555;
		}

		if ((c[0].type==1) && (c[0].bits==8) && (c[1].type==2) && (c[1].bits==8) && (c[2].type==3) && (c[2].bits==8)
			&& !cfg->block_size
		) {
			if (cfg->interleave==INTERLEAVE_COMPONENT) {
				if (cfg->sampling==SAMPLING_NONE) return GF_PIXEL_YUV444;
				if (cfg->sampling==SAMPLING_422) return GF_PIXEL_YUV422;
				if (cfg->sampling==SAMPLING_420) return GF_PIXEL_YUV;
			}
			if (cfg->interleave==INTERLEAVE_MIXED) {
				if (cfg->sampling==SAMPLING_420) return GF_PIXEL_NV12;
			}
		}
		if ((c[0].type==1) && (c[0].bits==8) && (c[1].type==3) && (c[1].bits==8) && (c[2].type==2) && (c[2].bits==8)
			&& !cfg->block_size
		) {
			if (cfg->interleave==INTERLEAVE_COMPONENT) {
				if (cfg->sampling==SAMPLING_420) return GF_PIXEL_YVU;
			}
			if (cfg->interleave==INTERLEAVE_MIXED) {
				if (cfg->sampling==SAMPLING_420) return GF_PIXEL_NV21;
			}
		}
	}

	if (cfg->nb_comps==4) {
		if ((c[0].type==12) && (c[0].bits==1)
			&& (c[1].type==4) && (c[1].bits==5)
			&& (c[2].type==5) && (c[2].bits==5)
			&& (c[3].type==6) && (c[3].bits==5)
			&& !cfg->block_size
			&& (cfg->interleave==INTERLEAVE_PIXEL)
		) {
			return GF_PIXEL_RGB_565;
		}

		if ((cfg->sampling==SAMPLING_422) && (cfg->interleave==INTERLEAVE_MULTIY)
			&& (!cfg->block_size || (cfg->block_size==4)) && !cfg->block_little_endian
			&& (c[0].type==1) && (c[0].bits==8) && (c[1].type==2) && (c[1].bits==8)
			&& (c[2].type==1) && (c[2].bits==8) && (c[3].type==3) && (c[3].bits==8)
		) {
			return GF_PIXEL_YUYV;
		}
		if ((cfg->sampling==SAMPLING_422) && (cfg->interleave==INTERLEAVE_MULTIY)
			&& (!cfg->block_size || (cfg->block_size==4)) && !cfg->block_little_endian
			&& (c[0].type==2) && (c[0].bits==8) && (c[1].type==1) && (c[1].bits==8)
			&& (c[2].type==3) && (c[2].bits==8) && (c[3].type==1) && (c[3].bits==8)
		) {
			return GF_PIXEL_UYVY;
		}

		if ((cfg->sampling==SAMPLING_422) && (cfg->interleave==INTERLEAVE_MULTIY)
			&& (!cfg->block_size || (cfg->block_size==4)) && !cfg->block_little_endian
			&& (c[0].type==1) && (c[0].bits==8) && (c[1].type==3) && (c[1].bits==8)
			&& (c[2].type==1) && (c[2].bits==8) && (c[3].type==2) && (c[3].bits==8)
		) {
			return GF_PIXEL_YVYU;
		}
		if ((cfg->sampling==SAMPLING_422) && (cfg->interleave==INTERLEAVE_MULTIY)
			&& (!cfg->block_size || (cfg->block_size==4)) && !cfg->block_little_endian
			&& (c[0].type==3) && (c[0].bits==8) && (c[1].type==1) && (c[1].bits==8)
			&& (c[2].type==2) && (c[2].bits==8) && (c[3].type==1) && (c[3].bits==8)
		) {
			return GF_PIXEL_VYUY;
		}
	}
	return 0;
}

static u32 uncv_get_line_size(UNCVDecCtx *ctx, u32 *comp_bits, u32 clen)
{
	UNCVConfig *config = ctx->cfg;
	u32 size=0;

	if (!config->block_size) {
		for (u32 i=0;i<ctx->tile_width; i++) {
			for (u32 c=0; c < clen; c+=2) {
				u32 bits = comp_bits[c];
				u32 align = comp_bits[c+1];
				if (align) {
					while (size%8) size++;
					bits = align*8;
				}
				size += bits;
			}
		}
		while (size % 8) size++;
		return size / 8;
	}
	//block size, figure out how many blocks
	u32 nb_bits = 0;
	for (u32 i=0; i<ctx->tile_width; i++) {
		for (u32 c=0; c<clen; c+=2) {
			u32 cbits = comp_bits[c];
			u32 align = comp_bits[c+1];
			if (align) {
				while (nb_bits%8) nb_bits++;
				cbits = align*8;
			}

			if (nb_bits + cbits > ctx->blocksize_bits) {
				nb_bits=0;
				size += config->block_size;
				//if first comp in block is first comp, pattern is done
				if (c==0) {
					u32 nb_pix = i+1;
					u32 nb_blocks = 1;
					while ((nb_blocks+1) * nb_pix < ctx->tile_width)
						nb_blocks++;

					size *= nb_blocks;
					//jump to last non-full pattern
					i += nb_pix * (nb_blocks-1);
				}
			}
			nb_bits += cbits;
		}
	}
	if (nb_bits)
		size += config->block_size;
	return size;
}

static void uncv_reset(UNCVDecCtx *ctx)
{
	if (ctx->cfg) uncv_del(ctx->cfg);
	ctx->cfg = NULL;

	if (ctx->bsrs) {
		for (u32 i=0; i<ctx->nb_bsrs; i++) {
			BSRead *bsr = &ctx->bsrs[i];
			if (bsr->bs) gf_bs_del(bsr->bs);
			if (bsr->le_bs) gf_bs_del(bsr->le_bs);
			if (bsr->le_buf) gf_free(bsr->le_buf);
			if (bsr->block_comps) gf_free(bsr->block_comps);
		}
		gf_free(ctx->bsrs);
		ctx->bsrs = NULL;
	}
	if (ctx->pixel) {
		gf_free(ctx->pixel);
		ctx->pixel = NULL;
	}
	if (ctx->comp_le_bs) {
		gf_bs_del(ctx->comp_le_bs);
		ctx->comp_le_bs = NULL;
	}
	if (ctx->comp_le_buf) {
		gf_free(ctx->comp_le_buf);
		ctx->comp_le_buf = NULL;
	}
}

static GF_Err uncv_config(UNCVDecCtx *ctx, u8 *dsi, u32 dsi_size)
{
	GF_Err e;

	uncv_reset(ctx);

	ctx->cfg = uncv_parse_config(dsi, dsi_size, &e);
	if (!ctx->cfg) return e;
	UNCVConfig *config = ctx->cfg;

	if (ctx->no_tile) {
		config->num_tile_cols = config->num_tile_rows = 1;
		if (config->interleave==INTERLEAVE_TILE)
			config->interleave = INTERLEAVE_COMPONENT;
	}

	//get our compatible configs
	ctx->pixel_format = uncv_get_compat(ctx);
	if (ctx->force_pf) ctx->pixel_format = 0;
	if (ctx->pixel_format) {
		ctx->passthrough = GF_TRUE;
		return GF_OK;
	}

	//not natively supported, we need a format - we only use 8 bits in reconstruction
	if (ctx->color_type==UNCV_OUT_YUV) {
		ctx->pixel_format = ctx->alpha ? GF_PIXEL_YUVA444_PACK : GF_PIXEL_YUV444_PACK;
		ctx->bpp = ctx->alpha ? 4 : 3;
	} else if (ctx->color_type==UNCV_OUT_RGB) {
		ctx->pixel_format = ctx->alpha ? GF_PIXEL_RGBA : GF_PIXEL_RGB;
		ctx->bpp = ctx->alpha ? 4 : 3;
	} else if (ctx->color_type==UNCV_OUT_MONO) {
		ctx->pixel_format = ctx->alpha ? GF_PIXEL_GREYALPHA : GF_PIXEL_GREYSCALE;
		ctx->bpp = ctx->alpha ? 2 : 1;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] No visual components present, cannot convert\n"));
		return GF_NOT_SUPPORTED;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[UNCV] No native format found, reconstructing as %s\n", gf_pixel_fmt_name(ctx->pixel_format)));

	ctx->stride = ctx->bpp * ctx->width;
	ctx->output_size = ctx->stride * ctx->height;
	ctx->tile_width = ctx->width / config->num_tile_cols;
	ctx->tile_height = ctx->height / config->num_tile_rows;
	ctx->blocksize_bits = config->block_size * 8;

	if (config->interleave==INTERLEAVE_COMPONENT) {
		if (config->sampling==SAMPLING_420) ctx->read_pixel = read_pixel_interleave_comp_yuv_420;
		else if (config->sampling) ctx->read_pixel = read_pixel_interleave_comp_yuv;
		else ctx->read_pixel = read_pixel_interleave_comp;
	}
	else if (config->interleave==INTERLEAVE_PIXEL) {
		ctx->read_pixel = read_pixel_interleave_pixel;
	}
	else if (config->interleave==INTERLEAVE_MIXED) {
		if (config->sampling)
			ctx->read_pixel = read_pixel_interleave_mixed;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Interleave mode 2 is set but no sampling type\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	else if (config->interleave==INTERLEAVE_MULTIY) {
		if ((config->sampling==SAMPLING_422) || (config->sampling==SAMPLING_411))
			ctx->read_pixel = read_pixel_interleave_multiy;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid interleave mode 5 for sampling type %d\n", config->sampling));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	} else if (config->interleave==INTERLEAVE_ROW) {
		if (config->sampling) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid interleave mode 3 for sampling type %d\n", config->sampling));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		ctx->read_pixel = read_pixel_interleave_comp;
	} else if (config->interleave==INTERLEAVE_TILE) {
		if (config->sampling) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid interleave mode 4 for sampling type %d\n", config->sampling));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (config->num_tile_cols * config->num_tile_rows == 1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid interleave mode 4 with no tiling"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		ctx->read_pixel = read_pixel_interleave_comp;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid interleave mode %d\n", config->interleave));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (config->pixel_size) {
		if ((config->interleave!=INTERLEAVE_PIXEL) && (config->interleave!=INTERLEAVE_MULTIY)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] pixel_size shall be 0 for interleave mode %d\n", config->interleave));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	UNCVComponentInfo *comps = config->comps;
/*	pixel = [];
*/
	u32 *bits = gf_malloc(sizeof(u32) * config->nb_comps*2);
	u32 nb_bits=0;
	u32 i, min_bits = 0;
	ctx->subsample_x = 1;
	u32 nb_out_comp=0;
	s32 first_comp_uv_idx = 0;
	if (config->sampling==SAMPLING_411) ctx->subsample_x = 4;
	else if (config->sampling) ctx->subsample_x = 2;

	ctx->tile_size = 0;

	if (config->palette) {
		for (i=0; i<config->palette->nb_comps; i++) {
			UNCVComponentInfo *comp = &config->palette->comps[i];
			if ((comp->type==0) || (comp->type==1) || (comp->type==4))
				comp->p_idx = 0;
			else if ((comp->type==2) || (comp->type==5))
				comp->p_idx = 1;
			else if ((comp->type==3) || (comp->type==6))
				comp->p_idx = 2;
			else if (comp->type==7)
				comp->p_idx = 3;
			else
				comp->p_idx = -1;
		}
	}
	u32 max_align_size=0;
	for (i=0; i<config->nb_comps; i++) {
		UNCVComponentInfo *comp = &comps[i];
		u32 nbb = comp->bits;
		comp->max_val = 1;
		while (nbb) {
			comp->max_val *= 2;
			nbb--;
		}
		comp->max_val--;

		if (!min_bits || (min_bits > comp->bits))
			min_bits = comp->bits;

		if ((comp->type==0) || (comp->type==1) || (comp->type==4))
			comp->p_idx = 0;
		else if ((comp->type==2) || (comp->type==5))
			comp->p_idx = 1;
		else if ((comp->type==3) || (comp->type==6))
			comp->p_idx = 2;
		else if (comp->type==7)
			comp->p_idx = 3;
		//palette, set to 'r' index but not used as is
		else if (comp->type==10)
			comp->p_idx = 0;
		//filtera array, set to 'r' index but not used as is
		else if (comp->type==11)
			comp->p_idx = 0;
		else
			comp->p_idx = -1;

		if (nb_out_comp < (u32) (comp->p_idx+1))
			nb_out_comp = comp->p_idx+1;

		if (max_align_size < comp->align_size)
			max_align_size = comp->align_size;

		comp->line_size=0;
		comp->row_align_size = 0;
		if ((config->interleave==INTERLEAVE_COMPONENT)
			|| (config->interleave==INTERLEAVE_MIXED)
			|| (config->interleave==INTERLEAVE_ROW)
			|| (config->interleave==INTERLEAVE_TILE)
		) {
			bits[0] = comp->bits;
			bits[1] = comp->align_size;
			comp->line_size = uncv_get_line_size(ctx, bits, 2);
			u32 row_align = config->row_align_size;

			if (config->interleave==INTERLEAVE_MIXED) {

				if ((comp->type==2) || (comp->type==3)) {
					if (!first_comp_uv_idx) {
						first_comp_uv_idx = i+1;
						//skip below check
						row_align = 0;
					} else {
						if (first_comp_uv_idx-1 + 1 != i) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid interleave 2 with non-consecutive U and V components"));
							return GF_NON_COMPLIANT_BITSTREAM;
						}
						bits[0] = comps[first_comp_uv_idx-1].bits;
						bits[1] = comps[first_comp_uv_idx-1].align_size;
						bits[2] = comp->bits;
						bits[3] = comp->align_size;
						comp->line_size = uncv_get_line_size(ctx, bits, 4);
						if (config->sampling==SAMPLING_422) comp->line_size /= 2;
						else if (config->sampling==SAMPLING_420) comp->line_size /= 2;
						else if (config->sampling==SAMPLING_411) comp->line_size /= 4;
					}
				}
			} else {
				if ((comp->type==2) || (comp->type==3)) {
					comp->line_size /= ctx->subsample_x;
					row_align /= ctx->subsample_x;
				}
			}

			if (row_align) {
				while (comp->line_size % row_align) {
					comp->line_size++;
				}
				comp->row_align_size = comp->line_size;
			}

			comp->plane_size = comp->line_size * ctx->tile_height;

			if ((config->sampling==SAMPLING_420) && ((comp->type==2) || (comp->type==3)))
				comp->plane_size /= 2;

			if (config->interleave!=INTERLEAVE_TILE) {
				ctx->tile_size += comp->plane_size;
			}
			else if (config->tile_align_size) {
				while (comp->plane_size % config->tile_align_size) {
					comp->plane_size++;
				}
			}
			if (((comp->type==2) || (comp->type==3)) && first_comp_uv_idx && (first_comp_uv_idx-1 != i) ) {
				comps[first_comp_uv_idx-1].line_size = comp->line_size;
				comps[first_comp_uv_idx-1].plane_size = comp->plane_size;
				comp->line_size = 0;
				comp->plane_size = 0;
			}
		} else {
			bits[nb_bits] = comp->bits;
			bits[nb_bits+1] = comp->align_size;
			nb_bits+=2;
		}

//		pixel.push(0);
		comp->comp_idx = i;
	}

	ctx->row_line_size=0;
	u32 lsize = uncv_get_line_size(ctx, bits, nb_bits);
	gf_free(bits);

	if (config->pixel_size &&
		((config->interleave==INTERLEAVE_PIXEL) || (config->interleave==INTERLEAVE_MULTIY))
	) {
		u32 lsize2 = config->pixel_size * ctx->tile_width;
		if (lsize2 < lsize) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid pixel size %d\n", config->pixel_size));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		lsize = lsize2;
	}

	if (config->row_align_size) {
		while (lsize % config->row_align_size) {
			lsize++;
		}
		ctx->row_line_size = lsize;
	}

	if ((config->interleave==INTERLEAVE_PIXEL) || (config->interleave==INTERLEAVE_MULTIY)) {
		ctx->tile_size = lsize * ctx->tile_height;
	}

	if (config->tile_align_size && (config->interleave!=INTERLEAVE_TILE)) {
		while (ctx->tile_size % config->tile_align_size) {
			ctx->tile_size++;
		}
	}

	ctx->max_comp_per_block = (u32) gf_floor(ctx->blocksize_bits / min_bits);


	ctx->nb_bsrs = 1;
	if ((config->interleave==INTERLEAVE_COMPONENT)
		|| (config->interleave==INTERLEAVE_MIXED)
		|| (config->interleave==INTERLEAVE_ROW)
		|| (config->interleave==INTERLEAVE_TILE)
	) {
		ctx->nb_bsrs = config->nb_comps;
	}

	ctx->bsrs = gf_malloc(sizeof(BSRead) * ctx->nb_bsrs);
	memset(ctx->bsrs, 0, sizeof(BSRead) * ctx->nb_bsrs);
	for (i=0; i<ctx->nb_bsrs; i++) {
		BSRead *bsr = &ctx->bsrs[i];
		bsr->bs = gf_bs_new((u8*)&ctx, 1, GF_BITSTREAM_READ);

		if (config->block_size && config->block_little_endian) {
			bsr->le_buf = gf_malloc(sizeof(u8) * config->block_size);
			bsr->le_bs = gf_bs_new((u8*)&ctx, 1, GF_BITSTREAM_READ);
		}

		if (ctx->blocksize_bits) {
			bsr->block_comps = gf_malloc(sizeof(BlockComp) * ctx->max_comp_per_block);
		}
	}

	if (ctx->use_palette) nb_out_comp += ctx->cfg->palette->nb_comps;
	else if (ctx->cfg->fa_map) nb_out_comp += 3;
	//we always write 3 comp in pixels
	if (nb_out_comp<ctx->bpp) nb_out_comp = ctx->bpp;

	ctx->pixel = gf_malloc(sizeof(u8)*nb_out_comp);
	memset(ctx->pixel, 0, sizeof(u8)*nb_out_comp);

	if (config->components_little_endian && max_align_size) {
		ctx->comp_le_buf = gf_malloc(sizeof(u8)*max_align_size);
		ctx->comp_le_bs = gf_bs_new(ctx->comp_le_buf, max_align_size, GF_BITSTREAM_READ);
	}
	return GF_OK;
}

static GF_Err uncvdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	UNCVDecCtx *ctx = (UNCVDecCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NOT_SUPPORTED;
	if (prop->value.uint != GF_CODECID_RAW_UNCV) return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (!prop) return GF_NOT_SUPPORTED;
	ctx->width = prop->value.uint;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (!prop) return GF_NOT_SUPPORTED;
	ctx->height = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_NOT_SUPPORTED;
	ctx->ipid = pid;

	GF_Err e = uncv_config(ctx, prop->value.data.ptr, prop->value.data.size);
	if (e) return e;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_RAW ));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, & PROP_UINT( ctx->pixel_format ));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	return GF_OK;
}


static void uncv_setup_block(UNCVDecCtx *ctx, BSRead *bsr)
{
	if (!ctx->blocksize_bits) return;

	bsr->loaded_comps = 0;
	bsr->first_comp_idx = 0;
	if (ctx->cfg->block_little_endian) {
		gf_bs_reassign_buffer(bsr->le_bs, bsr->le_buf, ctx->cfg->block_size);
	}
	for (u32 i=0; i<ctx->max_comp_per_block; i++) {
		bsr->block_comps[i].component = NULL;
		bsr->block_comps[i].val = 0;
	}
}


static void uncv_start_frame(UNCVDecCtx *ctx, const u8 *data, u32 size)
{
	UNCVConfig *config = ctx->cfg;

	if (ctx->nb_bsrs>1) {
		u32 offset = 0;
		u32 comp_row_size=0;
		for (u32 i=0; i<config->nb_comps; i++) {
			UNCVComponentInfo *comp = &config->comps[i];
			BSRead *bsr = &ctx->bsrs[i];
			gf_bs_reassign_buffer(bsr->bs, data, size);
			gf_bs_seek(bsr->bs, offset);
			bsr->init_offset = offset;
			bsr->line_start_pos = offset;
			bsr->row_align_size = comp->row_align_size;
			bsr->tile_size = 0;
			uncv_setup_block(ctx, bsr);

			if (config->interleave==INTERLEAVE_ROW)
				offset += comp->line_size;
			else if (config->interleave==INTERLEAVE_TILE)
				offset += comp->plane_size * config->num_tile_cols * config->num_tile_rows;
			else
				offset += comp->plane_size;

			bsr->plane_size = comp->plane_size;
			comp_row_size += comp->line_size;
			//tile size is per comp in this mode, not for the cumulated size
			if (config->interleave==INTERLEAVE_TILE) {
				bsr->tile_size = bsr->plane_size;
			} else if (config->interleave==INTERLEAVE_MIXED) {
				if ((comp->type==2) || (comp->type==3)) i++;
			}
		}

		for (u32 i=0; i<config->nb_comps; i++) {
			UNCVComponentInfo *comp = &config->comps[i];
			BSRead *bsr = &ctx->bsrs[i];
			bsr->comp_row_size = comp_row_size;
			if (config->interleave!=INTERLEAVE_TILE) {
				bsr->tile_size = ctx->tile_size;
			} else if (config->interleave==INTERLEAVE_MIXED) {
				if ((comp->type==2) || (comp->type==3)) i++;
			}
		}
	} else {
		BSRead *bsr = &ctx->bsrs[0];
		gf_bs_reassign_buffer(bsr->bs, data, size);
		uncv_setup_block(ctx, bsr);
		bsr->line_start_pos = 0;
		bsr->init_offset = 0;
		bsr->row_align_size = ctx->row_line_size;
		bsr->tile_size = ctx->tile_size;
	}
}


static void uncv_start_tile(UNCVDecCtx *ctx, UNCVConfig *config, u32 tile_x, u32 tile_y)
{
	if ((config->num_tile_cols==1) && (config->num_tile_rows==1)) return;
	for (u32 i=0; i<ctx->nb_bsrs; i++) {
		BSRead *bsr = &ctx->bsrs[i];
		u32 offs = bsr->init_offset + bsr->tile_size * (tile_x + config->num_tile_cols * tile_y);
		gf_bs_seek(bsr->bs, offs);
	}
}

static void uncv_end_tile(UNCVDecCtx *ctx)
{
}

static void uncv_end_line(UNCVDecCtx *ctx, UNCVConfig *config)
{
	for (u32 i=0; i<ctx->nb_bsrs; i++) {
		BSRead *bsr = &ctx->bsrs[i];

		gf_bs_align(bsr->bs);
		if (config->block_size) {
			bsr->loaded_comps = 0;
		}

		if (bsr->row_align_size) {
			u32 remain = (u32) (gf_bs_get_position(bsr->bs) - bsr->line_start_pos);
			while (remain < bsr->row_align_size) {
				gf_bs_skip_bytes(bsr->bs, 1);
				remain++;
			}
		}

		if (config->interleave==INTERLEAVE_ROW) {
			gf_bs_seek(bsr->bs, bsr->line_start_pos + bsr->comp_row_size);
		}
		bsr->line_start_pos = (u32) gf_bs_get_position(bsr->bs);
	}
}


static u8 uncv_get_val(GF_BitStream *bs, UNCVComponentInfo *comp, UNCVDecCtx *ctx)
{
	if (comp->format == 1) {
		Double d;
		if (comp->bits==32)
			d = gf_bs_read_float(bs);
		else if (comp->bits==64)
			d = gf_bs_read_double(bs);
		else
			d=0;
		if (comp->type<9)
			d *= 255;
		return (u8) d;
	}
	u64 c;
	if (comp->align_size) {
		if (!ctx) return 0;
		gf_bs_align(bs);
		if (ctx->comp_le_buf) {
			u32 i;
			for (i=0;i<comp->align_size; i++) {
				ctx->comp_le_buf[comp->align_size-i-1] = gf_bs_read_u8(bs);
			}
			gf_bs_seek(ctx->comp_le_bs, 0);
			c = gf_bs_read_long_int(ctx->comp_le_bs, comp->align_size*8);
		} else {
			c = gf_bs_read_long_int(bs, comp->align_size*8);
		}
	} else {
		c = gf_bs_read_long_int(bs, comp->bits);
	}

	if (comp->bits != 8) {
		c *= 255;
		c /= comp->max_val;
	}
	return (u8) c;
}

static void uncv_pull_block(UNCVDecCtx *ctx, UNCVConfig *config, BSRead *bsr, u32 comp_idx)
{
	u32 i, bits=0, nb_vals=0, bk_idx=0;

	for (i=comp_idx; i< config->nb_comps; ) {
		UNCVComponentInfo *comp = &config->comps[i];
		if (bits + comp->bits > ctx->blocksize_bits)
			break;

		if (comp->align_size) {
			while (bits % 8) bits++;
			bits += comp->align_size*8;
		} else {
			bits += comp->bits;
		}

		bsr->block_comps[bk_idx].component = comp;
		bk_idx++;
		nb_vals++;

		if (config->interleave) {
			i++;
			if (i==config->nb_comps) {
				//interleave mode with pixel size, do not loop over components
				if (config->pixel_size) break;
				i=0;
			}
		}
	}

	u32 pad = ctx->blocksize_bits - bits;

	if (config->block_little_endian) {
		GF_BitStream *bs_le = bsr->le_bs;
		gf_bs_seek(bs_le, 0);

		for (i=config->block_size; i>0; i--) {
			bsr->le_buf[i-1] = gf_bs_read_u8(bsr->bs);
		}

		if (!config->block_pad_lsb)
			gf_bs_read_int(bs_le, pad);

		for (i=0;i<nb_vals; i++) {
			//check block reverse
			u32 b_idx = config->block_reversed ? (nb_vals-i-1) : i;
			BlockComp *bcomp = &bsr->block_comps[b_idx];
			bcomp->val = uncv_get_val(bs_le, bcomp->component, ctx);
		}
		if (config->block_pad_lsb)
			gf_bs_read_int(bs_le, pad);
	} else {
		if (!config->block_pad_lsb)
			gf_bs_read_int(bsr->bs, pad);

		//no block reverse in big endian
		for (i=0; i<nb_vals; i++) {
			BlockComp *bcomp = &bsr->block_comps[i];
			bcomp->val = uncv_get_val(bsr->bs, bcomp->component, ctx);
		}

		if (config->block_pad_lsb)
			gf_bs_read_int(bsr->bs, pad);
	}
	bsr->loaded_comps = nb_vals;
	bsr->first_comp_idx = 0;
}

static void uncv_set_pix_val(UNCVDecCtx *ctx, UNCVComponentInfo *comp, u8 val, u32 x, u32 y)
{
	if (comp->p_idx<0) return;
	if (comp->type==10) {
		UNCVPalette *pal = ctx->cfg->palette;
		if (val >= pal->nb_values) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid palette index %d\n", val));
			return;
		}

		u8 *values = pal->values + val * pal->nb_comps;
		for (u32 i=0; i<pal->nb_comps; i++) {
			UNCVComponentInfo *p_comp = &pal->comps[i];
			uncv_set_pix_val(ctx, p_comp, values[i], x, y);
		}
	} else if (comp->type==11) {
		x = x % ctx->cfg->fa_width;
		y = y % ctx->cfg->fa_height;
		UNCVComponentDefinition *cdef = &ctx->cfg->comp_defs[ctx->cfg->fa_map[x + y*ctx->cfg->fa_height]];
		//only write if R, G or B
		if ((cdef->type>=4) && (cdef->type<=6)) {
			u32 c_idx = cdef->type-4;
			ctx->pixel[c_idx] = val;
		}
	} else {
		ctx->pixel[comp->p_idx] = val;
	}
}

static void uncv_pull_val(UNCVDecCtx *ctx, UNCVConfig *config, BSRead *bsr, UNCVComponentInfo *comp, Bool no_write, u32 x, u32 y)
{
	if (config->block_size) {
		if (bsr->first_comp_idx >= bsr->loaded_comps)
			bsr->loaded_comps = 0;

		if (!bsr->loaded_comps)
			uncv_pull_block(ctx, config, bsr, comp->comp_idx);

		BlockComp *bcomp = &bsr->block_comps[ bsr->first_comp_idx ];

		if (bcomp->component->p_idx>=0) {
			if (no_write)
				bcomp->component->value = bcomp->val;
			else
				uncv_set_pix_val(ctx, bcomp->component, bcomp->val, x, y);
		}

		bsr->first_comp_idx++;
	} else {
		u8 c = uncv_get_val(bsr->bs, comp, ctx);
		if (no_write) {
			comp->value = c;
		}
		else if (comp->p_idx>=0) {
			uncv_set_pix_val(ctx, comp, c, x, y);
		}
	}
}

static void read_pixel_interleave_pixel(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *out_data, u32 offset)
{
	BSRead *bsr = &ctx->bsrs[0];
	u32 psize = (u32) gf_bs_get_position(bsr->bs);
	for (u32 i=0; i<config->nb_comps; i++) {
		uncv_pull_val(ctx, config, bsr, &ctx->cfg->comps[i], GF_FALSE, x, y);
	}
	if (config->pixel_size) {
		if (ctx->blocksize_bits) {
			bsr->loaded_comps = 0;
		} else {
			gf_bs_align(bsr->bs);
		}
		psize = (u32) ( gf_bs_get_position(bsr->bs) - psize );
		if (psize > config->pixel_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid pixel_size %u, less than total size of components %u\n", config->pixel_size, psize));
		}
		while (psize < config->pixel_size) {
			gf_bs_read_u8(bsr->bs);
			psize++;
		}
	}
}

static void read_pixel_interleave_multiy(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *out_data, u32 offset)
{
	BSRead *bsr = &ctx->bsrs[0];
	u32 psize =(u32) gf_bs_get_position(bsr->bs);
	Bool load_uv = GF_FALSE;
	u32 pix_idx = x % ctx->subsample_x;
	if (pix_idx == 0) {
		for (u32 i=0; i<config->nb_comps; i++) {
			uncv_pull_val(ctx, config, bsr, &config->comps[i], GF_TRUE, x, y);
		}
		load_uv=GF_TRUE;
	}

	if (config->pixel_size) {
		gf_bs_align(bsr->bs);
		psize = (u32) (gf_bs_get_position(bsr->bs) - psize);
		if (psize > config->pixel_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[UNCV] Invalid pixel_size %d, less than total size of components %d\n", config->pixel_size, psize));
		}
		while (psize < config->pixel_size) {
			gf_bs_read_u8(bsr->bs);
			psize++;
		}
	}

	u32 idx=0;
	for (u32 i=0; i < config->nb_comps; i++) {
		UNCVComponentInfo *comp = &config->comps[i];
		if (comp->p_idx<0) continue;

		if ((comp->type==2) || (comp->type==3)) {
			if (load_uv) {
				uncv_set_pix_val(ctx, comp, comp->value, x, y);
			}
			continue;
		}
		if (idx == pix_idx) {
			uncv_set_pix_val(ctx, comp, comp->value, x, y);
		}
		idx++;
	}
}


static void read_pixel_interleave_comp(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *out_data, u32 offset)
{
	for (u32 i=0; i<config->nb_comps; i++) {
		uncv_pull_val(ctx, config, &ctx->bsrs[i], &config->comps[i], GF_FALSE, x, y);
	}
}

static void read_pixel_interleave_comp_yuv(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *out_data, u32 offset)
{
	Bool pull_uv = (x % ctx->subsample_x) ? GF_FALSE : GF_TRUE;
	for (u32 i=0; i<config->nb_comps; i++) {
		UNCVComponentInfo *comp = &config->comps[i];
		if (pull_uv || ((comp->type!=2) && (comp->type!=3)))
			uncv_pull_val(ctx, config, &ctx->bsrs[i], comp, GF_FALSE, x, y);
	}
}

static void read_pixel_interleave_comp_yuv_420(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *out_data, u32 offset)
{
	Bool pull_uv = (x % ctx->subsample_x) ? GF_FALSE : GF_TRUE;
	if (y%2) {
		pull_uv = GF_FALSE;
		ctx->pixel[1] = out_data[offset - ctx->stride + 1];
		ctx->pixel[2] = out_data[offset - ctx->stride + 2];
	}

	for (u32 i=0; i<config->nb_comps; i++) {
		UNCVComponentInfo *comp = &config->comps[i];
		if (pull_uv || ((comp->type!=2) && (comp->type!=3)))
			uncv_pull_val(ctx, config, &ctx->bsrs[i], comp, GF_FALSE, x, y);
	}
}

static void read_pixel_interleave_mixed(UNCVDecCtx *ctx, UNCVConfig *config, u32 x, u32 y, u8 *out_data, u32 offset)
{
	Bool pull_uv = (x % ctx->subsample_x) ? GF_FALSE : GF_TRUE;
	if ((config->sampling==SAMPLING_420) && (y%2)) {
		pull_uv = GF_FALSE;
		ctx->pixel[1] = out_data[offset - ctx->stride + 1];
		ctx->pixel[2] = out_data[offset - ctx->stride + 2];
	}

	for (u32 i=0; i<config->nb_comps; i++) {
		UNCVComponentInfo *comp = &config->comps[i];
		if ((comp->type!=2) && (comp->type!=3)) {
			uncv_pull_val(ctx, config, &ctx->bsrs[i], comp, GF_FALSE, x, y);
			continue;
		}
		if (pull_uv && comp->plane_size) {
			uncv_pull_val(ctx, config, &ctx->bsrs[i], comp, GF_FALSE, x, y);
			comp = &config->comps[i+1];
			uncv_pull_val(ctx, config, &ctx->bsrs[i], comp, GF_FALSE, x, y);
			i++;
		}
	}
}



static GF_Err uncvdec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	UNCVDecCtx *ctx = (UNCVDecCtx *) gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
			if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	if (ctx->passthrough) {
		gf_filter_pck_forward(pck, ctx->opid);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	u32 in_size;
	const u8 *in_data = gf_filter_pck_get_data(pck, &in_size);
	if (!in_data) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_SERVICE_ERROR;
	}

	u8 *out_data;
	GF_FilterPacket *dst = gf_filter_pck_new_alloc(ctx->opid, ctx->output_size, &out_data);
	gf_filter_pck_merge_properties(pck, dst);
	if (ctx->cfg->fa_map)
		memset(out_data, 0, ctx->output_size);

	uncv_start_frame(ctx, in_data, in_size);

	for (u32 t_j=0; t_j<ctx->cfg->num_tile_rows; t_j++) {
		for (u32 t_i=0; t_i<ctx->cfg->num_tile_cols; t_i++) {
			u32 t_y = t_j * ctx->tile_height;
			u32 t_x = t_i * ctx->tile_width;

			uncv_start_tile(ctx, ctx->cfg, t_i, t_j);
			u32 offset_tile = ctx->stride * t_y + ctx->bpp * t_x;

			for (u32 j=0; j<ctx->tile_height; j++) {
				u32 offset = offset_tile + ctx->stride * j;
				for (u32 i=0; i<ctx->tile_width; i++) {
					ctx->read_pixel(ctx, ctx->cfg, t_x + i, t_y+j, out_data, offset);
					out_data[offset] = ctx->pixel[0];
					if (ctx->bpp==2) {
						out_data[offset+1] = ctx->pixel[3];
					} else if (ctx->bpp>2) {
						out_data[offset+1] = ctx->pixel[1];
						out_data[offset+2] = ctx->pixel[2];
						if (ctx->bpp==4)
							out_data[offset+3] = ctx->pixel[3];
					}

					offset += ctx->bpp;
				}
				uncv_end_line(ctx, ctx->cfg);
			}
			uncv_end_tile(ctx);
		}
	}
	gf_filter_pck_send(dst);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static void uncvdec_finalize(GF_Filter *filter)
{
	UNCVDecCtx *ctx = (UNCVDecCtx *) gf_filter_get_udta(filter);
	uncv_reset(ctx);
}

static const GF_FilterCapability UNCVDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW_UNCV),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PIXFMT, GF_PIXEL_UNCV),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(UNCVDecCtx, _n)

static const GF_FilterArgs UNCVDecArgs[] =
{
	{ OFFS(force_pf), "ignore possible mapping to GPAC pixel formats", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(no_tile), "ignore tiling info (debug)", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};


GF_FilterRegister UNCVDecRegister = {
	.name = "uncvdec",
	GF_FS_SET_DESCRIPTION("UNCV decoder")
	GF_FS_SET_HELP("This filter translates UNCV pixel format to a usable pixel format.")
	.private_size = sizeof(UNCVDecCtx),
	.priority = 1,
	.args = UNCVDecArgs,
	SETCAPS(UNCVDecCaps),
	.finalize = uncvdec_finalize,
	.configure_pid = uncvdec_configure_pid,
	.process = uncvdec_process,
};

const GF_FilterRegister *uncvdec_register(GF_FilterSession *session)
{
	return &UNCVDecRegister;
}
