/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR codec sub-project
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

#include <gpac/internal/laser_dev.h>
#include <gpac/bitstream.h>
#include <gpac/math.h>

static void lsr_enc_log_bits(GF_LASeRCodec *lsr, u32 val, u32 nb_bits, const char *name)
{
	if (!lsr->trace) return;
	fprintf(lsr->trace, "%s\t\t%d\t\t%d", name, nb_bits, val);
	fprintf(lsr->trace, "\n");
}


#define GF_LSR_WRITE_INT(_codec, _val, _nbBits, _str)	{\
	gf_bs_write_int(_codec->bs, _val, _nbBits);	\
	lsr_enc_log_bits(_codec, _val, _nbBits, _str); \
	}\


static void lsr_write_group_content(GF_LASeRCodec *lsr, SVGElement *elt, Bool skip_object_content);
static GF_Err lsr_write_command_list(GF_LASeRCodec *lsr, GF_List *comList, SVGscriptElement *script);
static GF_Err lsr_write_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list, Bool reset_encoding_context);
static void lsr_write_point_sequence(GF_LASeRCodec *lsr, GF_List *pts, const char *name);
static void lsr_write_path_type(GF_LASeRCodec *lsr, SVG_PathData *path, const char *name);

GF_LASeRCodec *gf_laser_encoder_new(GF_SceneGraph *graph)
{
	GF_LASeRCodec *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_LASeRCodec));
	if (!tmp) return NULL;
	tmp->streamInfo = gf_list_new();
	tmp->font_table = gf_list_new();
	tmp->sg = graph;
	return tmp;
}

void gf_laser_encoder_del(GF_LASeRCodec *codec)
{	
	/*destroy all config*/
	while (gf_list_count(codec->streamInfo)) {
		LASeRStreamInfo *p = gf_list_last(codec->streamInfo);
		free(p);
		gf_list_rem_last(codec->streamInfo);
	}
	gf_list_del(codec->streamInfo);
	if (codec->col_table) free(codec->col_table);
	while (gf_list_count(codec->font_table)) {
		char *ft = gf_list_last(codec->font_table);
		free(ft);
		gf_list_rem_last(codec->font_table);
	}
	gf_list_del(codec->font_table);
	free(codec);
}


void gf_laser_set_trace(GF_LASeRCodec *codec, FILE *trace)
{
	codec->trace = trace;
	if (trace) fprintf(codec->trace, "Name\t\tNbBits\t\tValue\t\t//comment\n\n");
}

static LASeRStreamInfo *lsr_get_stream(GF_LASeRCodec *codec, u16 ESID)
{
	LASeRStreamInfo *ptr;
	u32 i = 0;
	while ((ptr = gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}


GF_Err gf_laser_encoder_new_stream(GF_LASeRCodec *codec, u16 ESID, GF_LASERConfig *cfg)
{
	LASeRStreamInfo *pInfo;
	if (lsr_get_stream(codec, ESID) != NULL) return GF_BAD_PARAM;
	GF_SAFEALLOC(pInfo, sizeof(LASeRStreamInfo));
	pInfo->ESID = ESID;
	memcpy(&pInfo->cfg, cfg, sizeof(GF_LASERConfig));
	if (!pInfo->cfg.time_resolution) pInfo->cfg.time_resolution = 1000;
	if (!pInfo->cfg.colorComponentBits) pInfo->cfg.colorComponentBits = 8;
	if (!pInfo->cfg.coord_bits) pInfo->cfg.coord_bits = 12;
	if (pInfo->cfg.resolution<-8) pInfo->cfg.resolution=-8;
	else if (pInfo->cfg.resolution>7) pInfo->cfg.resolution=7;

	gf_list_add(codec->streamInfo, pInfo);
	return GF_OK;
}

GF_Err gf_laser_encoder_get_config(GF_LASeRCodec *codec, u16 ESID, char **out_data, u32 *out_data_length)
{
	GF_BitStream *bs;
	if (!codec || !out_data || !out_data_length) return GF_BAD_PARAM;

	codec->info = lsr_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bs, codec->info->cfg.profile, 8);
	gf_bs_write_int(bs, codec->info->cfg.level, 8);
	gf_bs_write_int(bs, 0 /*codec->info->cfg.reserved*/, 3);
	gf_bs_write_int(bs, codec->info->cfg.pointsCodec, 2);
	gf_bs_write_int(bs, codec->info->cfg.pathComponents, 4);
	gf_bs_write_int(bs, codec->info->cfg.fullRequestHost, 1);
	if (codec->info->cfg.time_resolution != 1000) {
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, codec->info->cfg.time_resolution , 16);
	} else {
		gf_bs_write_int(bs, 0, 1);
	}
	gf_bs_write_int(bs, codec->info->cfg.colorComponentBits - 1, 4);
	if (codec->info->cfg.resolution<0) 
		gf_bs_write_int(bs, 16 + codec->info->cfg.resolution, 4);
	else
		gf_bs_write_int(bs, codec->info->cfg.resolution, 4);
	gf_bs_write_int(bs, codec->info->cfg.coord_bits, 5);
	gf_bs_write_int(bs, codec->info->cfg.scale_bits_minus_coord_bits, 4);
	gf_bs_write_int(bs, codec->info->cfg.append ? 1 : 0, 1);
	gf_bs_write_int(bs, codec->info->cfg.has_string_ids ? 1 : 0, 1);
	gf_bs_write_int(bs, codec->info->cfg.has_private_data ? 1 : 0, 1);
	gf_bs_write_int(bs, codec->info->cfg.hasExtendedAttributes ? 1 : 0, 1);
	gf_bs_write_int(bs, codec->info->cfg.extensionIDBits, 4);

	gf_bs_align(bs);
	gf_bs_get_content(bs, (unsigned char **) out_data, out_data_length);
	gf_bs_del(bs);
	return GF_OK;
}


GF_Err gf_laser_encode_au(GF_LASeRCodec *codec, u16 ESID, GF_List *command_list, Bool reset_context, char **out_data, u32 *out_data_length)
{
	GF_Err e;
	if (!codec || !command_list || !out_data || !out_data_length) return GF_BAD_PARAM;

	codec->info = lsr_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits_minus_coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution>=0)
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1<<codec->info->cfg.resolution) );
	else 
		codec->res_factor = INT2FIX(1 << (-codec->info->cfg.resolution));

	codec->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	e = lsr_write_laser_unit(codec, command_list, reset_context);
	if (!e) {
		gf_bs_align(codec->bs);
		gf_bs_get_content(codec->bs, (unsigned char **) out_data, out_data_length);
	}
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}

GF_Err gf_laser_encoder_get_rap(GF_LASeRCodec *codec, char **out_data, u32 *out_data_length)
{
	GF_Err e;
	if (!codec->info) codec->info = gf_list_get(codec->streamInfo, 0);
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits_minus_coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution>=0)
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1<<codec->info->cfg.resolution) );
	else 
		codec->res_factor = INT2FIX(1 << (-codec->info->cfg.resolution));

	codec->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	e = lsr_write_laser_unit(codec, NULL, 0);
	if (e == GF_OK) gf_bs_get_content(codec->bs, (unsigned char **)out_data, out_data_length);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}


static void lsr_write_vluimsbf5(GF_LASeRCodec *lsr, u32 val, const char *name)
{
	u32 nb_words;
	u32 nb_tot, nb_bits = val ? gf_get_bit_size(val) : 1;
	nb_words = nb_bits / 4;
	if (nb_bits%4) nb_words++;
	assert(nb_words * 4 >= nb_bits);
	nb_bits = nb_words * 4;
	nb_tot = nb_words+nb_bits;
	while (nb_words) {
		nb_words--;
		gf_bs_write_int(lsr->bs, nb_words ? 1 : 0, 1);
	}
	gf_bs_write_int(lsr->bs, val, nb_bits);
	lsr_enc_log_bits(lsr, val, nb_tot, name);
}

static void lsr_write_vluimsbf8(GF_LASeRCodec *lsr, u32 val, const char *name)
{
	u32 nb_words;
	u32 nb_tot, nb_bits = val ? gf_get_bit_size(val) : 1;
	nb_words = nb_bits / 7;
	if (nb_bits%7) nb_words++;
	assert(nb_words * 7 >= nb_bits);
	nb_bits = nb_words * 7;
	nb_tot = nb_words+nb_bits;
	while (nb_words) {
		nb_words--;
		gf_bs_write_int(lsr->bs, nb_words ? 1 : 0, 1);
	}
	gf_bs_write_int(lsr->bs, val, nb_bits);
	lsr_enc_log_bits(lsr, val, nb_tot, name);
}

static void lsr_write_extension(GF_LASeRCodec *lsr, char *data, u32 len, const char *name)
{
	if (!len) len = strlen(name);
	lsr_write_vluimsbf5(lsr, len, name);
	gf_bs_write_data(lsr->bs, data, len);
}

static void lsr_write_extend_class(GF_LASeRCodec *lsr, char *data, u32 len, const char *name)
{
	u32 i=0;
	GF_LSR_WRITE_INT(lsr, 0, lsr->info->cfg.extensionIDBits, "reserved");
	lsr_write_vluimsbf5(lsr, len, "len");
	while (i<len) {
		gf_bs_write_int(lsr->bs, data[i], 8);
		i++;
	}
}

static void lsr_write_codec_IDREF(GF_LASeRCodec *lsr, SVG_IRI *href, const char *name)
{
	u32 nID = 0;
	if (href && href->target) nID = gf_node_get_id((GF_Node *)href->target);
	else if (name[0]=='#') {
		GF_Node *n = gf_sg_find_node_by_name(lsr->sg, (char *) name + 1);
		if (n) nID = gf_node_get_id((GF_Node *)href->target);
	}
	assert(nID);

	lsr_write_vluimsbf5(lsr, nID-1, name);
}

static void lsr_write_codec_IDREF_Node(GF_LASeRCodec *lsr, GF_Node *href, const char *name)
{
	u32 nID = gf_node_get_id(href);
	assert(nID);
	lsr_write_vluimsbf5(lsr, nID-1, name);
}

static void lsr_write_fixed_16_8(GF_LASeRCodec *lsr, Fixed fix, const char *name)
{
	u32 val;
	if (fix<0) {
		val = FIX2INT((-fix) * 256);
		val |= (1<<23);
	} else {
		val = FIX2INT(fix*256);
	}
	val &= 0x00FFFFFF;
	GF_LSR_WRITE_INT(lsr, val, 24, name);
}
static void lsr_write_fixed_16_8i(GF_LASeRCodec *lsr, SVG_Number *n, const char *name)
{
	if (n->type==SVG_NUMBER_INHERIT) {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		lsr_write_fixed_16_8(lsr, n->value, name);
	}
}

static s32 lsr_get_font_index(GF_LASeRCodec *lsr, SVG_FontFamily *font)
{
	u32 i, count;
	if ((font->type!=SVG_FONTFAMILY_VALUE) || !font->value) return -1;
	count = gf_list_count(lsr->font_table);
	for (i=0; i<count; i++) {
		char *n = gf_list_get(lsr->font_table, i);
		if (!strcmp(n, font->value)) return (s32) i;
	}
	return -2;
}

static s32 lsr_get_col_index(GF_LASeRCodec *lsr, SVG_Color *color)
{
	u16 r, g, b;
	u32 i;
	if (color->type!=SVG_COLOR_RGBCOLOR) return -1;
	r = FIX2INT(color->red*lsr->color_scale);
	g = FIX2INT(color->green*lsr->color_scale);
	b = FIX2INT(color->blue*lsr->color_scale);
	for (i=0; i<lsr->nb_cols; i++) {
		LSRCol *c = &lsr->col_table[i];
		if ((c->r == r) && (c->g == g) && (c->b == b)) return (s32) i;
	}
	return -2;
}

static void lsr_write_line_increment_type(GF_LASeRCodec *lsr, SVG_LineIncrement *li, const char *name)
{
	if (li->type==SVG_NUMBER_INHERIT) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, 1, 1, "inherit");
	} else if (li->type==SVG_NUMBER_AUTO) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, 0, 1, "auto");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_fixed_16_8(lsr, li->value, "line-increment-value");
	}
}

static void lsr_write_byte_align_string(GF_LASeRCodec *lsr, char *str, const char *name)
{
	u32 len = str ? strlen(str) : 0;
	gf_bs_align(lsr->bs);
	lsr_write_vluimsbf8(lsr, len, "len");
	if (len) gf_bs_write_data(lsr->bs, str, len);
	lsr_enc_log_bits(lsr, 0, 8*len, name);
}
static void lsr_write_byte_align_string_list(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	char text[4096];
	u32 i, count = gf_list_count(l);
	text[0] = 0;
	for (i=0; i<count; i++) {
		char *str = gf_list_get(l, i);
		strcat(text, str);
		if (i+1<count) strcat(text, ";");
	}
	lsr_write_byte_align_string(lsr, text, name);
}

static void lsr_write_any_uri(GF_LASeRCodec *lsr, SVG_IRI *iri, const char *name)
{
	if (iri->type==SVG_IRI_IRI) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasUri");
		if (!iri->iri || strnicmp(iri->iri, "data:", 5)) {
			lsr_write_byte_align_string(lsr, iri->iri, "uri");
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasData");
		} else {
			u32 len;
			char *sep = strchr(iri->iri, ',');
			sep[0] = 0;
			lsr_write_byte_align_string(lsr, iri->iri, "uri");
			sep[0] = ',';
			len = strlen(sep+1);
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasData");
			lsr_write_vluimsbf5(lsr, len, "len");
			gf_bs_write_data(lsr->bs, sep+1, len);
		}
    } else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasUri");
	}
	GF_LSR_WRITE_INT(lsr, (iri->type==SVG_IRI_ELEMENTID) ? 1 : 0, 1, "hasID");
	if (iri->type==SVG_IRI_ELEMENTID) lsr_write_codec_IDREF(lsr, iri, "idref");

	GF_LSR_WRITE_INT(lsr, 0, 1, "hasStreamID");
    //if (hasStreamID) lsr_write_codec_IDREF(lsr, iri, "idref");
}

static void lsr_write_any_uri_string(GF_LASeRCodec *lsr, char *uri, const char *name)
{
	SVG_IRI iri;
	iri.type = SVG_IRI_IRI;
	iri.iri = uri;
	lsr_write_any_uri(lsr, &iri, name);
}

static void lsr_write_paint(GF_LASeRCodec *lsr, SVG_Paint *paint, const char *name)
{
	if (paint->type==SVG_PAINT_COLOR) {
		s32 idx;
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasIndex");
		idx = lsr_get_col_index(lsr, &paint->color);
		if (idx<0) { idx = 0; fprintf(stdout, "Warning: color not in colorTable\n"); }
		GF_LSR_WRITE_INT(lsr, (u32) idx, lsr->colorIndexBits, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasIndex");
		if (paint->type==SVG_PAINT_URI) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "isEnum");
			if (1) {
				SVG_IRI iri;
				GF_LSR_WRITE_INT(lsr, 1, 1, "isURI");
				iri.type = SVG_IRI_IRI;
				iri.iri = paint->uri;
				lsr_write_any_uri(lsr, &iri, "uri");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isURI");
				lsr_write_extension(lsr, "ERROR", 5, "colorExType0");
			}
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, "isEnum");
			switch (paint->type) {
			case SVG_PAINT_INHERIT: GF_LSR_WRITE_INT(lsr, 0, 2, "choice"); break;
			case SVG_PAINT_COLOR: GF_LSR_WRITE_INT(lsr, 1, 2, "choice"); break;
			default: GF_LSR_WRITE_INT(lsr, 2, 2, "choice"); break;
			}
		}
	} 
}

static void lsr_write_private_element_container(GF_LASeRCodec *lsr)
{
	/*NO PRIVATE DATA ON ENCODING YET*/
	assert(0);
}

static void lsr_write_private_att_class(GF_LASeRCodec *lsr)
{
	/*NO PRIVATE DATA ON ENCODING YET*/
	assert(0);
}

static void lsr_write_private_attr_container(GF_LASeRCodec *lsr, u32 index, const char *name)
{
	assert(0);
}

static void lsr_write_any_attribute(GF_LASeRCodec *lsr, GF_Node *node, SVGElement *clone, Bool skippable)
{
	if (1) {
		if (skippable) GF_LSR_WRITE_INT(lsr, 0, 1, "has_attrs");
	} else {
		if (skippable) GF_LSR_WRITE_INT(lsr, 1, 1, "has_attrs");
/*
		do () {
			GF_LSR_WRITE_INT(lsr, 0, lsr->info->cfg.extensionIDBits, "reserved");
		    lsr_write_vluimsbf5(lsr, 0, "len");//len in BITS
			GF_LSR_WRITE_INT(lsr, 0, 0, "reserved_val");
		} while () 
*/
	}
}

static void lsr_write_private_attributes(GF_LASeRCodec *lsr, SVGElement *elt)
{
	if (1) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");
		lsr_write_private_att_class(lsr);
	}
}
static void lsr_write_string_attribute(GF_LASeRCodec *lsr, char *class_attr, char *name)
{
	if (class_attr) {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		lsr_write_byte_align_string(lsr, class_attr, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	}
}
static void lsr_write_id(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 id = gf_node_get_id(n);
	if (id) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "has__0_id");
	    lsr_write_vluimsbf5(lsr, id-1, "ID");
	    if (lsr->info->cfg.has_string_ids) lsr_write_byte_align_string(lsr, (char *) gf_node_get_name(n), "stringId");
		GF_LSR_WRITE_INT(lsr, 0, 1, "reserved");
#if TODO_LASER_EXTENSIONS	
		if (0) {
		    lsr_write_vluimsbf5(lsr, reserved_len, "len");
			GF_LSR_WRITE_INT(lsr, 0, reserved_len, "reserved");
		}
#endif
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has__0_id");
	}
}

static u32 lsr_translate_coords(GF_LASeRCodec *lsr, Fixed x, u32 nb_bits)
{
	s32 res;
	res = FIX2INT( gf_divfix(x, lsr->res_factor) );
	if (res>=0) {
		if (res & (1<<(nb_bits-1)) ) fprintf(stdout, "nb_bits %d not large enough to encode positive number %g!\n", nb_bits, FIX2FLT(x));
		return (u32) res;
	}
	res += (1<<nb_bits);
	if (res<0) fprintf(stdout, "nb_bits %d not large enough to encode negative number %g!\n", nb_bits, FIX2FLT(x));
	assert( res & (1<<(nb_bits-1)) );
	return res;
}

static u32 lsr_translate_scale(GF_LASeRCodec *lsr, Fixed v)
{
	/*always 8 bits for fractional part*/
	v = v*256;
	if (v<0) return FIX2INT(v) + (1<<lsr->coord_bits);
	return FIX2INT(v);
}
static void lsr_write_matrix(GF_LASeRCodec *lsr, GF_Matrix2D *mx)
{
	u32 res;
	if (!mx->m[0] && !mx->m[1] && !mx->m[3] && !mx->m[4]) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "isNotMatrix");
		if (1) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "isRef");
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasXY");
			lsr_write_fixed_16_8(lsr, mx->m[2], "valueX");
			lsr_write_fixed_16_8(lsr, mx->m[5], "valueY");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "isRef");
			lsr_write_extension(lsr, NULL, 0, "ext");
		}
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "isNotMatrix");
		lsr->coord_bits += lsr->scale_bits;

		if (mx->m[0] || mx->m[4]) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "xx_yy_present");
			res = lsr_translate_scale(lsr, mx->m[0]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "xx");
			res = lsr_translate_scale(lsr, mx->m[4]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "yy");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "xx_yy_present");
		}
		if (mx->m[1] || mx->m[3]) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "xy_yx_present");
			res = lsr_translate_scale(lsr, mx->m[1]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "xy");
			res = lsr_translate_scale(lsr, mx->m[3]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "yx");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "xy_yx_present");
		}
		lsr->coord_bits -= lsr->scale_bits;

		if (mx->m[2] || mx->m[5]) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "xz_yz_present");
			res = lsr_translate_coords(lsr, mx->m[2], lsr->coord_bits);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "xz");
			res = lsr_translate_coords(lsr, mx->m[5], lsr->coord_bits);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "yz");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "xz_yz_present");
		}
	}
}

static u32 lsr_get_rare_props_idx(SVGElement *n, GF_FieldInfo *info)
{
	if (n->properties) {
		if (n->core && (&n->core->_class == info->far_ptr)) return RARE_CLASS;
		else if (&n->properties->audio_level == info->far_ptr) return RARE_AUDIO_LEVEL;
		else if (&n->properties->color == info->far_ptr) return RARE_COLOR;
		else if (&n->properties->color_rendering == info->far_ptr) return RARE_COLOR_RENDERING;
		else if (&n->properties->display == info->far_ptr) return RARE_DISPLAY;
		else if (&n->properties->display_align == info->far_ptr) return RARE_DISPLAY_ALIGN;
		else if (&n->properties->fill_opacity == info->far_ptr) return RARE_FILL_OPACITY;
		else if (&n->properties->fill_rule == info->far_ptr) return RARE_FILL_RULE;
		else if (&n->properties->image_rendering == info->far_ptr) return RARE_IMAGE_RENDERING;
		else if (&n->properties->line_increment == info->far_ptr) return RARE_LINE_INCREMENT;
		else if (&n->properties->pointer_events == info->far_ptr) return RARE_POINTER_EVENTS;
		else if (&n->properties->shape_rendering == info->far_ptr) return RARE_SHAPE_RENDERING;
		else if (&n->properties->solid_color == info->far_ptr) return RARE_SOLID_COLOR;
		else if (&n->properties->solid_opacity == info->far_ptr) return RARE_SOLID_OPACITY;
		else if (&n->properties->stop_color == info->far_ptr) return RARE_STOP_COLOR;
		else if (&n->properties->stop_opacity == info->far_ptr) return RARE_STOP_OPACITY;
		else if (&n->properties->stroke_dasharray == info->far_ptr) return RARE_STROKE_DASHARRAY;
		else if (&n->properties->stroke_dashoffset == info->far_ptr) return RARE_STROKE_DASHOFFSET;
		else if (&n->properties->stroke_linecap == info->far_ptr) return RARE_STROKE_LINECAP;
		else if (&n->properties->stroke_linejoin == info->far_ptr) return RARE_STROKE_LINEJOIN;
		else if (&n->properties->stroke_miterlimit == info->far_ptr) return RARE_STROKE_MITERLIMIT;
		else if (&n->properties->stroke_opacity == info->far_ptr) return RARE_STROKE_OPACITY;
		else if (&n->properties->stroke_width == info->far_ptr) return RARE_STROKE_WIDTH;
		else if (&n->properties->text_anchor == info->far_ptr) return RARE_TEXT_ANCHOR;
		else if (&n->properties->text_rendering == info->far_ptr) return RARE_TEXT_RENDERING;
		else if (&n->properties->viewport_fill == info->far_ptr) return RARE_VIEWPORT_FILL;
		else if (&n->properties->viewport_fill_opacity == info->far_ptr) return RARE_VIEWPORT_FILL_OPACITY;
		else if (&n->properties->vector_effect == info->far_ptr) return RARE_VECTOR_EFFECT;
		else if (&n->properties->visibility == info->far_ptr) return RARE_VISIBILITY;
		else if (&n->properties->font_family == info->far_ptr) return RARE_FONT_FAMILY;
		else if (&n->properties->font_size == info->far_ptr) return RARE_FONT_SIZE;
		else if (&n->properties->font_style == info->far_ptr) return RARE_FONT_STYLE;
		else if (&n->properties->font_weight == info->far_ptr) return RARE_FONT_WEIGHT;
	}
	if (n->conditional) {
		if (&n->conditional->requiredExtensions == info->far_ptr) return RARE_REQUIREDEXTENSIONS;
		else if (&n->conditional->requiredFeatures == info->far_ptr) return RARE_REQUIREDFEATURES;
		else if (&n->conditional->requiredFormats == info->far_ptr) return RARE_REQUIREDFEATURES;
		else if (&n->conditional->systemLanguage == info->far_ptr) return RARE_SYSTEMLANGUAGE;
	}
	if (n->core) {
		if (&n->core->base == info->far_ptr) return RARE_XML_BASE;
		else if (&n->core->lang == info->far_ptr) return RARE_XML_LANG;
		else if (&n->core->space == info->far_ptr) return RARE_XML_SPACE;
	}
	if (n->focus) {
		if (&n->focus->nav_next == info->far_ptr) return RARE_FOCUSNEXT;
		else if (&n->focus->nav_up == info->far_ptr) return RARE_FOCUSNORTH;
		else if (&n->focus->nav_up_left == info->far_ptr) return RARE_FOCUSNORTHWEST;
		else if (&n->focus->nav_up_right == info->far_ptr) return RARE_FOCUSNORTHEAST;
		else if (&n->focus->nav_prev == info->far_ptr) return RARE_FOCUSPREV;
		else if (&n->focus->nav_down == info->far_ptr) return RARE_FOCUSSOUTH;
		else if (&n->focus->nav_down_left == info->far_ptr) return RARE_FOCUSSOUTHWEST;
		else if (&n->focus->nav_down_right == info->far_ptr) return RARE_FOCUSSOUTHEAST;
		else if (&n->focus->nav_left == info->far_ptr) return RARE_FOCUSWEST;
		else if (&n->focus->nav_right == info->far_ptr) return RARE_FOCUSEAST;
		else if (&n->focus->focusable == info->far_ptr) return RARE_FOCUSABLE;
	}
	if (n->xlink) {
		if (&n->xlink->title == info->far_ptr) return RARE_HREF_TITLE;
		else if (&n->xlink->type == info->far_ptr) return RARE_HREF_TYPE;
		else if (&n->xlink->role == info->far_ptr) return RARE_HREF_ROLE;
		else if (&n->xlink->arcrole == info->far_ptr) return RARE_HREF_ARCROLE;
		else if (&n->xlink->actuate == info->far_ptr) return RARE_HREF_ACTUATE;
		else if (&n->xlink->show == info->far_ptr) return RARE_HREF_SHOW;
	}
	if (n->timing) {
		if (&n->timing->end == info->far_ptr) return RARE_END;
		else if (&n->timing->max == info->far_ptr) return RARE_MAX;
		else if (&n->timing->min == info->far_ptr) return RARE_MIN;
	}
	return 0;
}

static void lsr_write_fixed_clamp(GF_LASeRCodec *lsr, Fixed f, const char *name)
{
#ifdef GPAC_FIXED_POINT
	s32 val = f >> 8;
#else
	s32 val = (u32) (255 * f);
#endif
	if (val<0) val = 0;
	else if (val>255) val = 255;
	GF_LSR_WRITE_INT(lsr, (u32) val, 8, name);
}

static void lsr_write_time_list(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	SMIL_Time *v;
	u32 r_count, i, evt_count, count = gf_list_count(l);
	Bool indef = 0;

	evt_count = r_count = 0;
	for (i=0; i<count; i++) {
		v = gf_list_get(l, i);
		if (v->type==SMIL_TIME_INDEFINITE) {
			indef = 1;
			break;
		}
		else if (v->type==SMIL_TIME_EVENT) evt_count++;
		else if (!v->dynamic_type) r_count++;
	}
	if (evt_count) {
		fprintf(stdout, "Warning: Event-base SMIL times are not supported in LASeR without code rewrite - skipping\n");
	}
	if (!r_count && !indef) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);
	GF_LSR_WRITE_INT(lsr, indef, 1, "choice");
	if (indef) return;
	lsr_write_vluimsbf5(lsr, r_count, "count");
	for (i=0; i<count; i++) {
		s32 now;
		v = gf_list_get(l, i);
		if ((v->type==SMIL_TIME_EVENT) || v->dynamic_type) continue;

		now = (s32) (v->clock * lsr->time_resolution);
		if (now<0) {
			now = -now;
			GF_LSR_WRITE_INT(lsr, 1, 1, "sign");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "sign");
		}
		lsr_write_vluimsbf5(lsr, now, "value");
	}
}

static void lsr_write_duration(GF_LASeRCodec *lsr, SMIL_Duration *v, const char *name)
{
	if (!v || !v->type) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);

	if (v->type==SMIL_DURATION_DEFINED) {
		s32 now = (s32) (v->clock_value * lsr->time_resolution);
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (now<0) ? 1 : 0, 1, "sign");
		if (now<0) now = -now;
		lsr_write_vluimsbf5(lsr, now, "value");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (v->type==SMIL_DURATION_INDEFINITE) ? 0 : 1, 1, "time");
	}
}

static void lsr_write_focus(GF_LASeRCodec *lsr, SVG_Focus *foc, const char *name)
{
	if (foc->type==SVG_FOCUS_IRI) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "isEnum");
		lsr_write_codec_IDREF(lsr, &foc->target, "id");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "isEnum");
		GF_LSR_WRITE_INT(lsr, (foc->type==SVG_FOCUS_SELF) ? 0 : 1, 1, "enum");
	}
}

static Bool lsr_elt_has_same_base(GF_LASeRCodec *lsr, SVGElement *elt, SVGElement *base, Bool no_stroke_check)
{
	GF_FieldInfo info, base_info;
	u32 i, count, nb_rare, field_rare;
	if (!base) return 0;
	if (elt->core->eRR != base->core->eRR) return 0;
	/*check stroke color*/
	if (!no_stroke_check) {
		info.fieldType = base_info.fieldType = SVG_Paint_datatype;
		info.far_ptr = &elt->properties->stroke;
		base_info.far_ptr = &base->properties->stroke;
		if (!gf_svg_attributes_equal(&info, &base_info)) return 0;
	}

	/*check rare attribs*/
	nb_rare = 0;
	count = gf_node_get_field_count((GF_Node *)elt);
	for (i=0; i<count; i++) {
		gf_node_get_field((GF_Node *)elt, i, &info);
		/*check if rare*/
		field_rare = lsr_get_rare_props_idx((SVGElement *)elt, &info);
		if (!field_rare) continue;
		/*check if default*/
		gf_node_get_field((GF_Node *)base, i, &base_info);
		if (gf_svg_attributes_equal(&info, &base_info)) continue;
		return 0;
	}
	/*TODO check anyAttribute*/
	return 1;
}
static Bool lsr_elt_has_same_fill(GF_LASeRCodec *lsr, SVGElement *elt, SVGElement *base)
{
	GF_FieldInfo f1, f2;
	f1.fieldType = f2.fieldType = SVG_Paint_datatype;
	f1.far_ptr = &elt->properties->fill;
	f2.far_ptr = &base->properties->fill;
	if (gf_svg_attributes_equal(&f1, &f2)) return 1;
	/*TODO compare anyAttribute class...*/
	return 0;
}
static Bool lsr_elt_has_same_stroke(GF_LASeRCodec *lsr, SVGElement *elt, SVGElement *base)
{
	GF_FieldInfo f1, f2;
	f1.fieldType = f2.fieldType = SVG_Paint_datatype;
	f1.far_ptr = &elt->properties->stroke;
	f2.far_ptr = &base->properties->stroke;
	if (gf_svg_attributes_equal(&f1, &f2)) return 1;
	/*TODO compare anyAttribute class...*/
	return 0;
}
static Bool lsr_transform_equal(GF_Matrix2D *t1, GF_Matrix2D *t2)
{
	if (t1->m[0] != t2->m[0]) return 0;
	if (t1->m[1] != t2->m[1]) return 0;
	if (t1->m[2] != t2->m[2]) return 0;
	if (t1->m[3] != t2->m[3]) return 0;
	if (t1->m[4] != t2->m[4]) return 0;
	if (t1->m[5] != t2->m[5]) return 0;
	return 1;
}
static Bool lsr_number_equal(SVG_Number *n1, SVG_Number *n2)
{
	if (n1->type != n2->type) return 0;
	if (n1->type != SVG_NUMBER_VALUE) return 1;
	if (n1->value != n2->value) return 0;
	return 1;
}
static Bool lsr_float_list_equal(GF_List *l1, GF_List *l2)
{
	u32 i, count = gf_list_count(l1);
	if (count != gf_list_count(l2)) return 0;
	for (i=0;i<count;i++) {
		Fixed *v1 = gf_list_get(l1, i);
		Fixed *v2 = gf_list_get(l2, i);
		if (*v1 != *v2) return 0;
	}
	return 1;
}

static void lsr_write_rare_full(GF_LASeRCodec *lsr, GF_Node *n, GF_Node *default_node, SVG_Matrix *matrix)
{
	GF_FieldInfo all_info[100];
	u32 i, count, nb_rare, field_rare;
	Bool has_transform = 0;

	nb_rare = 0;
	count = gf_node_get_field_count(n);
	for (i=0; i<count; i++) {
		GF_FieldInfo def_info;
		gf_node_get_field(n, i, &all_info[nb_rare]);
		/*check if rare*/
		field_rare = lsr_get_rare_props_idx((SVGElement *)n, &all_info[nb_rare]);
		if (!field_rare) continue;
		/*check if default*/
		gf_node_get_field(default_node, i, &def_info);
		if (gf_svg_attributes_equal(&all_info[nb_rare], &def_info)) continue;
		/*override field index to store rare val*/
		all_info[nb_rare].fieldIndex = field_rare;
		nb_rare++;
	}
	if (matrix) {
		has_transform = !gf_mx2d_is_identity(*matrix);
		if (has_transform) nb_rare++;
	}

	GF_LSR_WRITE_INT(lsr, nb_rare ? 1 : 0, 1, "has__0_rare");
	if (!nb_rare) return;

	GF_LSR_WRITE_INT(lsr, nb_rare, 6, "nbOfAttributes");
	if (has_transform) nb_rare--;
	for (i=0; i<nb_rare; i++) {
		GF_FieldInfo *fi = &all_info[i];
		GF_LSR_WRITE_INT(lsr, fi->fieldIndex, 6, "attributeRARE");
		switch (fi->fieldIndex) {
		/*rare*/
		case RARE_CLASS: lsr_write_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "class"); break;
		/*properties*/

		case RARE_AUDIO_LEVEL: lsr_write_fixed_clamp(lsr, ((SVG_Number *) fi->far_ptr)->value, "audio-level"); break;
	    case RARE_COLOR: lsr_write_paint(lsr, fi->far_ptr, "color"); break;
		case RARE_COLOR_RENDERING: GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)fi->far_ptr, 2, "color-rendering"); break;
		case RARE_DISPLAY: GF_LSR_WRITE_INT(lsr, *(SVG_Display*)fi->far_ptr, 5, "display"); break;
	    case RARE_DISPLAY_ALIGN: GF_LSR_WRITE_INT(lsr, *(SVG_DisplayAlign*)fi->far_ptr, 2, "display-align"); break;
		case RARE_FILL_OPACITY: lsr_write_fixed_clamp(lsr, ((SVG_Number *)fi->far_ptr)->value, "fill-opacity"); break;
	    case RARE_FILL_RULE: GF_LSR_WRITE_INT(lsr, *(SVG_FillRule*)fi->far_ptr, 2, "fill-rule"); break;
		case RARE_IMAGE_RENDERING: GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)fi->far_ptr, 2, "image-rendering"); break;
		case RARE_LINE_INCREMENT: lsr_write_line_increment_type(lsr, fi->far_ptr, "lineIncrement"); break;
	    case RARE_POINTER_EVENTS: GF_LSR_WRITE_INT(lsr, *(SVG_PointerEvents*)fi->far_ptr, 4, "pointer-events"); break;
		case RARE_SHAPE_RENDERING: GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)fi->far_ptr, 3, "shape-rendering"); break;
	    case RARE_SOLID_COLOR: lsr_write_paint(lsr, fi->far_ptr, "solid-color"); break;
		case RARE_SOLID_OPACITY: lsr_write_fixed_clamp(lsr, ((SVG_Number *)fi->far_ptr)->value, "solid-opacity"); break;
	    case RARE_STOP_COLOR: lsr_write_paint(lsr, fi->far_ptr, "stop-color"); break;
		case RARE_STOP_OPACITY: lsr_write_fixed_clamp(lsr, ((SVG_Number *)fi->far_ptr)->value, "stop-opacity"); break;
		case RARE_STROKE_DASHARRAY:  
		{
			u32 j;
			SVG_StrokeDashArray *da = (SVG_StrokeDashArray*)fi->far_ptr;
			if (da->type==SVG_STROKEDASHARRAY_INHERIT) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "dashArray");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "dashArray");
				lsr_write_vluimsbf5(lsr, da->array.count, "len");
		        for (j=0; j<da->array.count; j++) {
		            lsr_write_fixed_16_8(lsr, da->array.vals[j], "dash");
				}
			}
		}
			break;
		case RARE_STROKE_DASHOFFSET: 
			lsr_write_fixed_16_8i(lsr, fi->far_ptr, "dashOffset"); break;

		/*TODO FIXME - SVG values are not in sync with LASeR*/
		case RARE_STROKE_LINECAP: GF_LSR_WRITE_INT(lsr, *(SVG_StrokeLineCap*)fi->far_ptr, 2, "stroke-linecap"); break;
		case RARE_STROKE_LINEJOIN: GF_LSR_WRITE_INT(lsr, *(SVG_StrokeLineJoin*)fi->far_ptr, 2, "stroke-linejoin"); break;
		case RARE_STROKE_MITERLIMIT: lsr_write_fixed_16_8i(lsr, fi->far_ptr, "miterLimit"); break;
		case RARE_STROKE_OPACITY: lsr_write_fixed_clamp(lsr, ((SVG_Number *)fi->far_ptr)->value, "stroke-opacity"); break;
		case RARE_STROKE_WIDTH: lsr_write_fixed_16_8i(lsr, fi->far_ptr, "strokeWidth"); break;
		case RARE_TEXT_ANCHOR: GF_LSR_WRITE_INT(lsr, *(SVG_TextAnchor*)fi->far_ptr, 2, "text-achor"); break;
		case RARE_TEXT_RENDERING: GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)fi->far_ptr, 2, "text-rendering"); break;
		case RARE_VIEWPORT_FILL: lsr_write_paint(lsr, fi->far_ptr, "viewport-fill"); break;
		case RARE_VIEWPORT_FILL_OPACITY: lsr_write_fixed_clamp(lsr, ((SVG_Number *)fi->far_ptr)->value, "viewport-fill-opacity"); break;
		case RARE_VECTOR_EFFECT: GF_LSR_WRITE_INT(lsr, *(SVG_VectorEffect*)fi->far_ptr, 4, "vector-effect"); break;
	    case RARE_VISIBILITY: GF_LSR_WRITE_INT(lsr, *(SVG_PointerEvents*)fi->far_ptr, 2, "visibility"); break;
	    case RARE_REQUIREDEXTENSIONS: lsr_write_byte_align_string_list(lsr, *(GF_List **)fi->far_ptr, "requiredExtensions"); break;
	    case RARE_REQUIREDFORMATS: lsr_write_byte_align_string_list(lsr, *(GF_List **)fi->far_ptr, "requiredFormats"); break;
	    case RARE_REQUIREDFEATURES: 
		{
			GF_List *l = *(GF_List **)fi->far_ptr;
			u32 j, tot_count, count = gf_list_count(l);
			u8 *vals = malloc(sizeof(u8)*count);
			tot_count = 0;
			for (i=0; i<count; i++) {
				char *ext;
				SVG_IRI *iri = gf_list_get(l, i);
				if (iri->type != SVG_IRI_IRI) continue;
				ext = strchr(iri->iri, '#');
				if (!ext) continue;
				if (!stricmp(ext, "Animation")) { vals[tot_count] = 0; tot_count++; }
				else if (!stricmp(ext, "Audio")) { vals[tot_count] = 1; tot_count++; }
				else if (!stricmp(ext, "ComposedVideo")) { vals[tot_count] = 2; tot_count++; }
				else if (!stricmp(ext, "ConditionalProcessing")) { vals[tot_count] = 3; tot_count++; }
				else if (!stricmp(ext, "ConditionalProcessingAttribute")) { vals[tot_count] = 4; tot_count++; }
				else if (!stricmp(ext, "CoreAttribute")) { vals[tot_count] = 5; tot_count++; }
				else if (!stricmp(ext, "Extensibility")) { vals[tot_count] = 6; tot_count++; }
				else if (!stricmp(ext, "ExternalResourcesRequired")) { vals[tot_count] = 7; tot_count++; }
				else if (!stricmp(ext, "Font")) { vals[tot_count] = 8; tot_count++; }
				else if (!stricmp(ext, "Gradient")) { vals[tot_count] = 9; tot_count++; }
				else if (!stricmp(ext, "GraphicsAttribute")) { vals[tot_count] = 10; tot_count++; }
				else if (!stricmp(ext, "Handler")) { vals[tot_count] = 11; tot_count++; }
				else if (!stricmp(ext, "Hyperlinking")) { vals[tot_count] = 12; tot_count++; }
				else if (!stricmp(ext, "Image")) { vals[tot_count] = 13; tot_count++; }
				else if (!stricmp(ext, "OpacityAttribute")) { vals[tot_count] = 14; tot_count++; }
				else if (!stricmp(ext, "PaintAttribute")) { vals[tot_count] = 15; tot_count++; }
				else if (!stricmp(ext, "Prefetch")) { vals[tot_count] = 16; tot_count++; }
				else if (!stricmp(ext, "SVG")) { vals[tot_count] = 17; tot_count++; }
				else if (!stricmp(ext, "SVG-animation")) { vals[tot_count] = 18; tot_count++; }
				else if (!stricmp(ext, "SVG-dynamic")) { vals[tot_count] = 19; tot_count++; }
				else if (!stricmp(ext, "SVG-static")) { vals[tot_count] = 20; tot_count++; }
				else if (!stricmp(ext, "SVGDOM")) { vals[tot_count] = 21; tot_count++; }
				else if (!stricmp(ext, "SVGDOM-animation")) { vals[tot_count] = 22; tot_count++; }
				else if (!stricmp(ext, "SVGDOM-dynamic")) { vals[tot_count] = 23; tot_count++; }
				else if (!stricmp(ext, "SVGDOM-static")) { vals[tot_count] = 24; tot_count++; }
				else if (!stricmp(ext, "Script")) { vals[tot_count] = 25; tot_count++; }
				else if (!stricmp(ext, "Shape")) { vals[tot_count] = 26; tot_count++; }
				else if (!stricmp(ext, "SolidColor")) { vals[tot_count] = 27; tot_count++; }
				else if (!stricmp(ext, "Structure")) { vals[tot_count] = 28; tot_count++; }
				else if (!stricmp(ext, "Text")) { vals[tot_count] = 29; tot_count++; }
				else if (!stricmp(ext, "TimedAnimation")) { vals[tot_count] = 30; tot_count++; }
				else if (!stricmp(ext, "TransformedVideo")) { vals[tot_count] = 31; tot_count++; }
				else if (!stricmp(ext, "Video")) { vals[tot_count] = 32; tot_count++; }
				else if (!stricmp(ext, "XlinkAttribute")) { vals[tot_count] = 33; tot_count++; }
			}
			lsr_write_vluimsbf5(lsr, tot_count, "len");
	        for (j=0; j<tot_count; j++) {
				GF_LSR_WRITE_INT(lsr, vals[j], 6, "feature");
			}
			free(vals);
		}
			break;

	    case RARE_SYSTEMLANGUAGE: 
			lsr_write_byte_align_string_list(lsr, *(GF_List **)fi->far_ptr, "systemLanguage"); 
			break;
	    case RARE_XML_BASE: lsr_write_any_uri(lsr, fi->far_ptr, "xml:base"); break;
	    case RARE_XML_LANG: lsr_write_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xml:lang"); break;
	    case RARE_XML_SPACE: GF_LSR_WRITE_INT(lsr, *(XML_Space *)fi->far_ptr, 1, "xml:space"); break;
		case RARE_FOCUSNEXT: lsr_write_focus(lsr, fi->far_ptr, "focusNext"); break;
		case RARE_FOCUSNORTH: lsr_write_focus(lsr, fi->far_ptr, "focusNorth"); break;
		case RARE_FOCUSNORTHEAST: lsr_write_focus(lsr, fi->far_ptr, "focusNorthEast"); break;
		case RARE_FOCUSNORTHWEST: lsr_write_focus(lsr, fi->far_ptr, "focusNorthWest"); break;
		case RARE_FOCUSPREV: lsr_write_focus(lsr, fi->far_ptr, "focusPrev"); break;
		case RARE_FOCUSSOUTH: lsr_write_focus(lsr, fi->far_ptr, "focusSouth"); break;
		case RARE_FOCUSSOUTHEAST: lsr_write_focus(lsr, fi->far_ptr, "focusSouthEast"); break;
		case RARE_FOCUSSOUTHWEST: lsr_write_focus(lsr, fi->far_ptr, "focusSouthWest"); break;
		case RARE_FOCUSWEST: lsr_write_focus(lsr, fi->far_ptr, "focusWest"); break;
		case RARE_FOCUSEAST: lsr_write_focus(lsr, fi->far_ptr, "focusEast"); break;

     	case RARE_FONT_VARIANT: GF_LSR_WRITE_INT(lsr, *(SVG_FontVariant *)fi->far_ptr, 2, "font-variant"); break;
		case RARE_FONT_FAMILY:
		{
			s32 idx = lsr_get_font_index(lsr, fi->far_ptr);
			if (idx<0) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isInherit");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isInherit");
				GF_LSR_WRITE_INT(lsr, idx, lsr->fontIndexBits, "fontIndex");
			}
		}
			break;
		case RARE_FONT_SIZE: lsr_write_fixed_16_8i(lsr, fi->far_ptr, "fontSize"); break;
		/*TODO not specified in spec !!*/
		case RARE_FONT_STYLE: GF_LSR_WRITE_INT(lsr, *((SVG_FontStyle *)fi->far_ptr), 5, "fontStyle"); break;
		/*TODO not specified in spec !!*/
		case RARE_FONT_WEIGHT: GF_LSR_WRITE_INT(lsr, *((SVG_FontWeight *)fi->far_ptr), 4, "fontWeight"); break;

		case RARE_HREF_TITLE: lsr_write_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xlink:title"); break;
		case RARE_HREF_TYPE: lsr_write_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xlink:type"); break;
		case RARE_HREF_ROLE: lsr_write_any_uri(lsr, fi->far_ptr, "xlink:role"); break;
		case RARE_HREF_ARCROLE: lsr_write_any_uri(lsr, fi->far_ptr, "xlink:arcrole"); break;
		case RARE_HREF_ACTUATE: lsr_write_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xlink:actuate"); break;
		case RARE_HREF_SHOW: lsr_write_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xlink:show"); break;
		case RARE_END: lsr_write_time_list(lsr, *(GF_List **)fi->far_ptr, "end"); break;
		case RARE_MIN: lsr_write_duration(lsr, fi->far_ptr, "min"); break;
		case RARE_MAX: lsr_write_duration(lsr, fi->far_ptr, "max"); break;
		}
	}
	/*transform*/
	if (has_transform) {
		GF_LSR_WRITE_INT(lsr, RARE_TRANSFORM, 6, "attributeRARE");
		lsr_write_matrix(lsr, matrix);
	}
}

#define lsr_write_rare(_a, _b, _c)	lsr_write_rare_full(_a, _b, _c, NULL)

static void lsr_write_fill(GF_LASeRCodec *lsr, SVGElement *n, SVGElement *clone)
{
	GF_FieldInfo f1, f2;
	Bool has_fill = 1;
	if (clone) {
		f1.fieldType = f2.fieldType = SVG_Paint_datatype;
		f1.far_ptr = &n->properties->fill;
		f2.far_ptr = &clone->properties->fill;
		has_fill = gf_svg_attributes_equal(&f1, &f2) ? 0 : 1;
	}
	GF_LSR_WRITE_INT(lsr, has_fill, 1, "has_fill");
	if (has_fill) lsr_write_paint(lsr, f1.far_ptr, "fill");
}

static void lsr_write_stroke(GF_LASeRCodec *lsr, SVGElement *n, SVGElement *clone)
{
	GF_FieldInfo f1, f2;
	Bool has_stroke;
	f1.fieldType = f2.fieldType = SVG_Paint_datatype;
	f1.far_ptr = &n->properties->stroke;
	f2.far_ptr = &clone->properties->stroke;
	has_stroke = gf_svg_attributes_equal(&f1, &f2) ? 0 : 1;
	GF_LSR_WRITE_INT(lsr, has_stroke, 1, "has_stroke");
	if (has_stroke) lsr_write_paint(lsr, f1.far_ptr, "stroke");
}
static void lsr_write_href(GF_LASeRCodec *lsr, SVG_IRI *iri)
{
	Bool has_href = iri ? 1 : 0;
	if (iri->type==SVG_IRI_ELEMENTID) {
		if (!iri->target || !gf_node_get_id((GF_Node *)iri->target)) has_href = 0;
	} else if (!iri->iri) has_href = 0;

	GF_LSR_WRITE_INT(lsr, has_href, 1, "has_href");
	if (has_href) lsr_write_any_uri(lsr, iri, "href");
}

static void lsr_write_accumulate(GF_LASeRCodec *lsr, u8 accum_type)
{
	Bool v = accum_type ? 1 : 0;
	GF_LSR_WRITE_INT(lsr, v ? 1 : 0, 1, "has_accumulate");
	if (v) GF_LSR_WRITE_INT(lsr, v ? 1 : 0, 1, "accumulate");
}
static void lsr_write_additive(GF_LASeRCodec *lsr, u8 add_type)
{
	Bool v = add_type ? 1 : 0;
	GF_LSR_WRITE_INT(lsr, v ? 1 : 0, 1, "has_additive");
	if (v) GF_LSR_WRITE_INT(lsr, v ? 1 : 0, 1, "additive");
}
static void lsr_write_calc_mode(GF_LASeRCodec *lsr, u8 calc_mode)
{
	/*SMIL_CALCMODE_LINEAR is default and 0 in our code*/
	GF_LSR_WRITE_INT(lsr, (calc_mode==SMIL_CALCMODE_LINEAR) ? 0 : 1, 1, "has_calcMode");
	if (calc_mode!=SMIL_CALCMODE_LINEAR) {
		/*SMIL_CALCMODE_DISCRETE is 0 in LASeR, 1 in our code*/
		if (calc_mode==1) calc_mode = 0;
		GF_LSR_WRITE_INT(lsr, calc_mode, 2, "calcMode");
	}
}

static void lsr_write_animatable(GF_LASeRCodec *lsr, SMIL_AttributeName *anim_type, SVGElement *elt, const char *name)
{
	u32 i, count;
	s32 a_type = -1;
	count = gf_node_get_field_count((GF_Node *)elt);
	/*browse all anim types*/
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		a_type = gf_lsr_field_to_attrib_type((GF_Node *)elt, i);
		if (a_type<0) continue;
		gf_node_get_field((GF_Node*)elt, i, &info);
		if (info.far_ptr == anim_type->field_ptr) 
			break;
		a_type = -1;
	}
	
	if (a_type<0) fprintf(stdout, "Unsupported attributeName\n");
	GF_LSR_WRITE_INT(lsr, 1, 1, "hasAttributeName");
	GF_LSR_WRITE_INT(lsr, (u8) a_type, 8, "attributeType");
}

static void lsr_write_single_time(GF_LASeRCodec *lsr, SMIL_Time *v, const char *name)
{
	if (!v || v->dynamic_type) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);

	if (v->type==SMIL_TIME_CLOCK) {
		s32 now = (s32) (v->clock * lsr->time_resolution);
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (now<0) ? 1 : 0, 1, "sign");
		if (now<0) now = -now;
		lsr_write_vluimsbf5(lsr, now, "value");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (v->type==SMIL_TIME_INDEFINITE) ? 0 : 1, 1, "time");
	}
}


static void lsr_write_anim_fill(GF_LASeRCodec *lsr, u8 animFreeze, const char *name)
{
	/*don't write if default !!*/
	if (animFreeze==SMIL_FILL_FREEZE) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		/*enumeration freeze{0} remove{1}*/
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
	}
}
static void lsr_write_anim_repeat(GF_LASeRCodec *lsr, SMIL_RepeatCount *repeat, const char *name)
{
	GF_LSR_WRITE_INT(lsr, repeat->type ? 1 : 0, 1, name);
	if (!repeat->type) return;
	if (repeat->type==SMIL_REPEATCOUNT_DEFINED) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		lsr_write_fixed_16_8(lsr, repeat->count, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		/*enumeration indefinite{0}*/
	}
}
static void lsr_write_repeat_duration(GF_LASeRCodec *lsr, SMIL_Duration *smil, const char *name)
{
	GF_LSR_WRITE_INT(lsr, smil->type ? 1 : 0, 1, name);
	if (!smil->type) return;

	if (smil->type==SMIL_DURATION_DEFINED) {
		u32 now = (u32) (smil->clock_value * lsr->time_resolution);
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_vluimsbf5(lsr, now, "value");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		/*enumeration indefinite{0}*/
	}
}
static void lsr_write_anim_restart(GF_LASeRCodec *lsr, u8 animRestart, const char *name)
{
	if (animRestart == SMIL_RESTART_ALWAYS) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		/*enumeration always{0} never{1} whenNotActive{2}*/
		GF_LSR_WRITE_INT(lsr, animRestart, 2, name);
	}
}

static u32 svg_type_to_lsr_anim(u32 svg_type, u32 transform_type)
{
	switch (svg_type) {
	/*all string types*/
	case SVG_String_datatype:
		return 0;
	/*all length types*/
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		return 1;
	case SVG_PathData_datatype: 
		return 2;
	/*list of points*/
	case SMIL_KeyPoints_datatype: 
	case SVG_Points_datatype:
		return 3;
	/*all 0 - 1 types*/
	case SVG_Opacity_datatype:
		return 4;
	case SVG_Paint_datatype:
		return 5;
	/*all enums (u8) types*/ 
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_TransformType_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
		return 6;
	/*all list-of-int types*/ //return 7;
	/*all list-of-float types*/
	case SVG_StrokeDashArray_datatype:
	case SVG_ViewBox_datatype:
	case SVG_Coordinates_datatype:
		return 8;
	/*ID (u32) types*/ //return 10;
	case SVG_FontFamily_datatype:
		return 11;
	case SVG_IRI_datatype:
		return 12;
	case SVG_Motion_datatype:
		return 9;

	case SVG_Matrix_datatype:
		switch (transform_type) {
		/*ARG LOOKS LIKE THE SPEC IS BROKEN HERE*/
		case SVG_TRANSFORM_TRANSLATE: return 9;
		case SVG_TRANSFORM_SCALE: return 8;
		case SVG_TRANSFORM_ROTATE: return 8;
		case SVG_TRANSFORM_SKEWX: return 1;
		case SVG_TRANSFORM_SKEWY: return 1;
		//case SVG_TRANSFORM_MATRIX: return;
		}
		/*FALL THROUH*/		
	default:
		return 255;
	}
}
static void lsr_write_coordinate(GF_LASeRCodec *lsr, Fixed val, Bool skipable, const char *name)
{
	if (skipable && !val) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		u32 res = lsr_translate_coords(lsr, val, lsr->coord_bits);
		if (skipable) GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, name);
	}
}

static void lsr_write_an_anim_value(GF_LASeRCodec *lsr, void *val, u32 lsr_type, u32 svg_type, u32 transform_type, const char *name)
{
	if ((lsr_type==1) || (lsr_type==4)) {
		if (transform_type==SVG_TRANSFORM_ROTATE) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
		} else {
			SVG_Number  *n = (SVG_Number *) val;
			if (n->type != SVG_NUMBER_VALUE) {
				GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
				GF_LSR_WRITE_INT(lsr, n->type, 2, "escapeEnum");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
			}
		}
	} else if (svg_type==SVG_StrokeDashArray_datatype) {
		SVG_StrokeDashArray *da = val;
		if (da->type==SVG_STROKEDASHARRAY_INHERIT) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "escapeFlag");
			GF_LSR_WRITE_INT(lsr, 0, 2, "escapeEnum");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
		}
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
	}

    switch(lsr_type) {
    case 0: lsr_write_byte_align_string(lsr, *(DOM_String *)val, name); break;
    case 1: 
		if (transform_type==SVG_TRANSFORM_ROTATE) {
			lsr_write_fixed_16_8(lsr, ((SVG_Point_Angle *) val)->angle, name); 
		} else {
			lsr_write_fixed_16_8(lsr, ((SVG_Number *) val)->value, name); 
		}
		break;
    case 12: lsr_write_any_uri(lsr, val, name); break;
    case 2: lsr_write_path_type(lsr, val, name); break;
    case 3: lsr_write_point_sequence(lsr, *(GF_List **)val, name); break;
    case 4: lsr_write_fixed_clamp(lsr, ((SVG_Number *) val)->value, name); break;
    case 5: lsr_write_paint(lsr, val, name); break;
    case 6: lsr_write_vluimsbf5(lsr, (u32) *(u8 *) val, name); break;
    case 10: lsr_write_vluimsbf5(lsr, *(u32 *) val, name); break;
    case 11: 
	{
		s32 idx = lsr_get_font_index(lsr, val);
		if (idx<0) {
			fprintf(stdout, "WARNING: corrupted font table while encoding anim value\n");
			idx=0;
		}
		lsr_write_vluimsbf5(lsr, idx, name);
	}
        break;
    case 7:
	{
		GF_List *l = *(GF_List **)val;
		u32 i, count = gf_list_count(l);
		lsr_write_vluimsbf5(lsr, count, "count");
        for (i=0; i<count; i++) {
			u8 *v = gf_list_get(l, i);
			lsr_write_vluimsbf5(lsr, *v, "val");
        }
	}
        break;
    case 8: // floats
	{
		u32 i, count;
		if (svg_type==SVG_StrokeDashArray_datatype) {
			SVG_StrokeDashArray *da = val;
			lsr_write_vluimsbf5(lsr, da->array.count, "count");
			for (i=0; i<da->array.count; i++) {
				lsr_write_fixed_16_8(lsr, da->array.vals[i], "val");
			}
		} else if (svg_type==SVG_ViewBox_datatype) {
			SVG_ViewBox *vb = val;
			lsr_write_vluimsbf5(lsr, 4, "count");
			lsr_write_fixed_16_8(lsr, vb->x, "val");
			lsr_write_fixed_16_8(lsr, vb->y, "val");
			lsr_write_fixed_16_8(lsr, vb->width, "val");
			lsr_write_fixed_16_8(lsr, vb->height, "val");
		} else if (svg_type==SVG_Coordinates_datatype) {
			GF_List *l = *(GF_List **)val;
			count = gf_list_count(l);
			lsr_write_vluimsbf5(lsr, count, "count");
			for (i=0; i<count; i++) {
				SVG_Coordinate *v = gf_list_get(l, i);
				lsr_write_fixed_16_8(lsr, v->value, "val");
			}
		} else if ((svg_type==SVG_Matrix_datatype) && (transform_type==SVG_TRANSFORM_ROTATE)) {
			SVG_Point_Angle *p = val;
			count = (p->x || p->y) ? 3 : 1;
			lsr_write_vluimsbf5(lsr, count, "count");
			lsr_write_fixed_16_8(lsr, p->angle, "val");
			if (count==3) {
				lsr_write_fixed_16_8(lsr, p->x, "val");
				lsr_write_fixed_16_8(lsr, p->y, "val");
			}
		} else if ((svg_type==SVG_Matrix_datatype) && (transform_type==SVG_TRANSFORM_SCALE)) {
			SVG_Point *pt = val;
			count = (pt->x == pt->y) ? 2 : 1;
			lsr_write_vluimsbf5(lsr, count, "count");
			lsr_write_fixed_16_8(lsr, pt->x, "val");
			if (count==2) lsr_write_fixed_16_8(lsr, pt->y, "val");
		} else {
			GF_List *l = *(GF_List **)val;
			count = gf_list_count(l);
			lsr_write_vluimsbf5(lsr, count, "count");
			for (i=0; i<count; i++) {
				Fixed *v = gf_list_get(l, i);
				lsr_write_fixed_16_8(lsr, *v, "val");
			}
		}
	}
        break;
    case 9: // point
		lsr_write_coordinate(lsr, ((SVG_Point *)val)->x, 0, "valX");
		lsr_write_coordinate(lsr, ((SVG_Point *)val)->y, 0, "valY");
        break;
    default:
		lsr_write_extension(lsr, NULL, 0, name);
        break;
    }
}

static void lsr_write_anim_value(GF_LASeRCodec *lsr, SMIL_AnimateValue *val, const char *name)
{
	u32 type = val->type;
	if (!val->type) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		type = svg_type_to_lsr_anim(val->type, val->transform_type);
		if (type==255) {
			fprintf(stdout, "WARNING - unsupported anim type %d - skipping\n", val->type);
			GF_LSR_WRITE_INT(lsr, 0, 1, name);
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, name);
			GF_LSR_WRITE_INT(lsr, type, 4, "type");
			lsr_write_an_anim_value(lsr, val->value, type, val->type, val->transform_type, name);
		}
	}
}

static void lsr_write_anim_values(GF_LASeRCodec *lsr, SMIL_AnimateValues *anims, const char *name)
{
	u32 i, count = 0;
	u32 type = anims->type;
	if (anims->type) count = gf_list_count(anims->values);

	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	type = svg_type_to_lsr_anim(anims->type, anims->transform_type);
	if (type==255) {
		fprintf(stdout, "WARNING - unsupported anim type %d - skipping\n", anims->type);
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, type, 4, "type");
		lsr_write_vluimsbf5(lsr, count, "count");
		for (i=0; i<count; i++) {
			void *att = gf_list_get(anims->values, i);
			lsr_write_an_anim_value(lsr, att, type, anims->type, anims->transform_type, name);
		}
	}
}

static void lsr_write_fraction_12(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	u32 i, count = gf_list_count(l);
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);
	lsr_write_vluimsbf5(lsr, count, "name");
	for (i=0; i<count; i++) {
		Fixed f = * (Fixed *) gf_list_get(l, i);
		if (!f || (f == FIX_ONE)) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasShort");
			GF_LSR_WRITE_INT(lsr, f ? 0 : 1, 1, "isZero");
		} else {
			u32 ft = (u32) ( FIX2FLT(f) * 4096/*(1<<12)*/ );
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasShort");
			GF_LSR_WRITE_INT(lsr, ft, 12, "val");
		}
	}
}
static void lsr_write_float_list(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	u32 i, count = gf_list_count(l);
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);
	lsr_write_vluimsbf5(lsr, count, "count");
	for (i=0;i<count;i++) {
		Fixed *v = gf_list_get(l, i);
		lsr_write_fixed_16_8(lsr, *v, "val");
	}
}

static u32 lsr_get_bit_size(GF_LASeRCodec *lsr, Fixed v) 
{
	u32 val;
	v = gf_divfix(v, lsr->res_factor);
	val = (v<0) ? FIX2INT(-v) : FIX2INT(v);
	return 1 + gf_get_bit_size(val);
}

static void lsr_write_point_sequence(GF_LASeRCodec *lsr, GF_List *pts, const char *name)
{
	u32 i, count = gf_list_count(pts);
	lsr_write_vluimsbf5(lsr, count, "nbPoints");
	if (!count) return;
	/*TODO golomb coding*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "flag");
    if (1) {
        if (count < 3) {
			u32 nb_bits = 0;
			for (i=0; i<count; i++) {
				u32 k;
				SVG_Point *pt = gf_list_get(pts, i);
				k = lsr_get_bit_size(lsr, pt->x); if (k>nb_bits) nb_bits = k;
				k = lsr_get_bit_size(lsr, pt->y); if (k>nb_bits) nb_bits = k;
			}

			GF_LSR_WRITE_INT(lsr, nb_bits, 5, "bits");
            for (i=0; i<count; i++) {
				SVG_Point *pt = gf_list_get(pts, i);
				u32 v = lsr_translate_coords(lsr, pt->x, nb_bits);
				GF_LSR_WRITE_INT(lsr, v, nb_bits, "x");
				v = lsr_translate_coords(lsr, pt->y, nb_bits);
				GF_LSR_WRITE_INT(lsr, v, nb_bits, "y");
            }
        } else {
			Fixed c_x, c_y;
			u32 k, nb_dx, nb_dy;
			SVG_Point *pt = gf_list_get(pts, 0);
			nb_dx = 0;
			k = lsr_get_bit_size(lsr, pt->x); if (k>nb_dx) nb_dx = k;
			k = lsr_get_bit_size(lsr, pt->y); if (k>nb_dx) nb_dx = k;
			GF_LSR_WRITE_INT(lsr, nb_dx, 5, "bits");
			k = lsr_translate_coords(lsr, pt->x, nb_dx);
			GF_LSR_WRITE_INT(lsr, k, nb_dx, "x");
			k = lsr_translate_coords(lsr, pt->y, nb_dx);
			GF_LSR_WRITE_INT(lsr, k, nb_dx, "y");
			c_x = pt->x;
			c_y = pt->y;
			nb_dx = nb_dy = 0;
			for (i=1; i<count; i++) {
				SVG_Point *pt = gf_list_get(pts, i);
				k = lsr_get_bit_size(lsr, pt->x - c_x); if (k>nb_dx) nb_dx = k;
				k = lsr_get_bit_size(lsr, pt->y - c_y); if (k>nb_dy) nb_dy = k;
				c_x = pt->x;
				c_y = pt->y;
			}
			GF_LSR_WRITE_INT(lsr, nb_dx, 5, "bitsx");
			GF_LSR_WRITE_INT(lsr, nb_dy, 5, "bitsy");
			c_x = pt->x;
			c_y = pt->y;
			for (i=1; i<count; i++) {
				SVG_Point *pt = gf_list_get(pts, i);
				k = lsr_translate_coords(lsr, pt->x - c_x, nb_dx);
				GF_LSR_WRITE_INT(lsr, k, nb_dx, "dx");
				k = lsr_translate_coords(lsr, pt->y - c_y, nb_dy);
				GF_LSR_WRITE_INT(lsr, k, nb_dy, "dy");
				c_x = pt->x;
				c_y = pt->y;
			}
        }
	}
}
static void lsr_write_path_type(GF_LASeRCodec *lsr, SVG_PathData *path, const char *name)
{
	u32 i, count;
	lsr_write_point_sequence(lsr, path->points, "seq");
	count = gf_list_count(path->commands);
    lsr_write_vluimsbf5(lsr, count, "nbOfTypes");
    for (i=0; i<count; i++) {
        u8 type = *(u8 *) gf_list_get(path->commands, i);
		GF_LSR_WRITE_INT(lsr, type, 5, name);
    }
}

static void lsr_write_rotate_type(GF_LASeRCodec *lsr, SVG_Rotate rotate, const char *name)
{
	if ((rotate.type == SVG_NUMBER_AUTO) || (rotate.type == SVG_NUMBER_AUTO_REVERSE)) {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (rotate.type == SVG_NUMBER_AUTO) ? 0 : 1, 1, "rotate");
	} else if (rotate.type == SVG_NUMBER_VALUE) {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_fixed_16_8(lsr, rotate.value, "rotate");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	}
}
static void lsr_write_sync_behavior(GF_LASeRCodec *lsr, u8 sync, const char *name)
{
	if (sync==SMIL_SYNCBEHAVIOR_INHERIT) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, sync-1, 2, name);
	}
}
static void lsr_write_sync_tolerance(GF_LASeRCodec *lsr, SMIL_SyncTolerance *sync, const char *name)
{
	if (sync->type==SMIL_SYNCTOLERANCE_INHERIT) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		if (sync->type==SMIL_SYNCTOLERANCE_DEFAULT) {
			GF_LSR_WRITE_INT(lsr, 1, 1, name);
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, name);
		    lsr_write_vluimsbf5(lsr, (u32) (sync->value*lsr->time_resolution), "value");
		}
	}
}
static void lsr_write_coord_list(GF_LASeRCodec *lsr, GF_List *coords, const char *name)
{
	u32 i, count = gf_list_count(coords);
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
	    lsr_write_vluimsbf5(lsr, count, "nb_coords");
		for (i=0; i<count; i++) {
			SVG_Coordinate *c = gf_list_get(coords, i);
			u32 res = lsr_translate_coords(lsr, c->value, lsr->coord_bits);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, name);
		}
	}
}

static void lsr_write_transform_behavior(GF_LASeRCodec *lsr, u8 tr_type, const char *name)
{
	if (1) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, 1, 4, name);
	}
}

static void lsr_write_content_type(GF_LASeRCodec *lsr, char *type, const char *name)
{
	if (type) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasType");
		lsr_write_byte_align_string(lsr, type, "type");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasType");
	}
}
static void lsr_write_script_type(GF_LASeRCodec *lsr, char *type, const char *name)
{
	if (type) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasType");
		if (!strcmp(type, "application/ecmascript")) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
			GF_LSR_WRITE_INT(lsr, 0, 2, "script");
		} else if (!strcmp(type, "application/jar-archive")) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
			GF_LSR_WRITE_INT(lsr, 1, 2, "script");
		} else if (!strcmp(type, "application/laserscript")) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
			GF_LSR_WRITE_INT(lsr, 2, 2, "script");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
			lsr_write_byte_align_string(lsr, type, "type");
		}
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasType");
	}
}
static void lsr_write_value_with_units(GF_LASeRCodec *lsr, SVG_Number *n, const char *name)
{
#ifdef GPAC_FIXED_POINT
	s32 val = n->value >> 8;
#else
	s32 val = (s32) (n->value * (1<<8) );
#endif
	GF_LSR_WRITE_INT(lsr, val, 32, name);
    switch (n->type) {
	case SVG_NUMBER_IN: GF_LSR_WRITE_INT(lsr, 1, 3, "units"); break;
	case SVG_NUMBER_CM: GF_LSR_WRITE_INT(lsr, 2, 3, "units"); break;
	case SVG_NUMBER_MM: GF_LSR_WRITE_INT(lsr, 3, 3, "units"); break;
	case SVG_NUMBER_PT: GF_LSR_WRITE_INT(lsr, 4, 3, "units"); break;
	case SVG_NUMBER_PC: GF_LSR_WRITE_INT(lsr, 5, 3, "units"); break;
	case SVG_NUMBER_PERCENTAGE: GF_LSR_WRITE_INT(lsr, 6, 3, "units"); break;
	default: GF_LSR_WRITE_INT(lsr, 0, 3, "units"); break;
	}
}

static void lsr_write_event_type(GF_LASeRCodec *lsr, u32 evtType, u32 evtParam)
{
	if (evtParam && (evtType!=SVG_DOM_EVT_KEYPRESS) && (evtType!=SVG_DOM_EVT_LONGKEYPRESS) ) {
		char szName[1024];
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		sprintf(szName, "%s(%c)", gf_dom_event_get_name(evtType), evtParam);
		lsr_write_byte_align_string(lsr, szName, "evtString");
	} else if (evtType==SVG_DOM_EVT_MOUSEMOVE) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_byte_align_string(lsr, "mousemove", "evtString");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		/* Enumeration: abort{0} accesskey{1} activate{2} begin{3} click{4} end{5} error{6} focusin{7} 
		focusout{8} keydown{9} keyup{10} load{11} longaccesskey{12} mousedown{13} mouseout{14} mouseover{15} 
		mouseup{16} pause{17} repeat{18} resize{19} resume{20} scroll{21} textinput{22} unload{23} zoom{24} */
		switch (evtType) {
		case SVG_DOM_EVT_ABORT: GF_LSR_WRITE_INT(lsr, 0, 6, "event"); break;
		case SVG_DOM_EVT_KEYPRESS: GF_LSR_WRITE_INT(lsr, 1, 6, "event"); break;
		case SVG_DOM_EVT_ACTIVATE: GF_LSR_WRITE_INT(lsr, 2, 6, "event"); break;
		case SVG_DOM_EVT_BEGIN: GF_LSR_WRITE_INT(lsr, 3, 6, "event"); break;
		case SVG_DOM_EVT_CLICK: GF_LSR_WRITE_INT(lsr, 4, 6, "event"); break;
		case SVG_DOM_EVT_END: GF_LSR_WRITE_INT(lsr, 5, 6, "event"); break;
		case SVG_DOM_EVT_ERROR: GF_LSR_WRITE_INT(lsr, 6, 6, "event"); break;
		case SVG_DOM_EVT_FOCUSIN: GF_LSR_WRITE_INT(lsr, 7, 6, "event"); break;
		case SVG_DOM_EVT_FOCUSOUT: GF_LSR_WRITE_INT(lsr, 8, 6, "event"); break;
		case SVG_DOM_EVT_KEYDOWN: GF_LSR_WRITE_INT(lsr, 9, 6, "event"); break;
		case SVG_DOM_EVT_KEYUP: GF_LSR_WRITE_INT(lsr, 10, 6, "event"); break;
		case SVG_DOM_EVT_LOAD: GF_LSR_WRITE_INT(lsr, 11, 6, "event"); break;
		case SVG_DOM_EVT_LONGKEYPRESS: GF_LSR_WRITE_INT(lsr, 12, 6, "event"); break;
		case SVG_DOM_EVT_MOUSEDOWN: GF_LSR_WRITE_INT(lsr, 13, 6, "event"); break;
		case SVG_DOM_EVT_MOUSEOUT: GF_LSR_WRITE_INT(lsr, 14, 6, "event"); break;
		case SVG_DOM_EVT_MOUSEOVER: GF_LSR_WRITE_INT(lsr, 15, 6, "event"); break;
		case SVG_DOM_EVT_MOUSEUP: GF_LSR_WRITE_INT(lsr, 16, 6, "event"); break;
		case SVG_DOM_EVT_REPEAT: GF_LSR_WRITE_INT(lsr, 18, 6, "event"); break;
		case SVG_DOM_EVT_RESIZE: GF_LSR_WRITE_INT(lsr, 19, 6, "event"); break;
		case SVG_DOM_EVT_SCROLL: GF_LSR_WRITE_INT(lsr, 21, 6, "event"); break;
		case SVG_DOM_EVT_TEXTINPUT: GF_LSR_WRITE_INT(lsr, 22, 6, "event"); break;
		case SVG_DOM_EVT_UNLOAD: GF_LSR_WRITE_INT(lsr, 23, 6, "event"); break;
		case SVG_DOM_EVT_ZOOM: GF_LSR_WRITE_INT(lsr, 24, 6, "event"); break;
		default:
			fprintf(stdout, "Unsupported LASER event\n");
			GF_LSR_WRITE_INT(lsr, 0, 6, "event"); break;
			return;
		}
		if ((evtType==SVG_DOM_EVT_KEYPRESS) || (evtType==SVG_DOM_EVT_LONGKEYPRESS)) {
			if (evtParam) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "hasKeyCode");
				lsr_write_vluimsbf5(lsr, evtParam, "keyCode");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "hasKeyCode");
			}
		}
	}
}

static void lsr_write_clip_time(GF_LASeRCodec *lsr, SVG_Clock clock, const char *name)
{
	if (clock < 0) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, 0, 1, "isEnum");
		GF_LSR_WRITE_INT(lsr, 0, 1, "sign");
		lsr_write_vluimsbf5(lsr, (u32) (lsr->time_resolution*clock), "val");
	}
}

static void lsr_write_href_anim(GF_LASeRCodec *lsr, SVG_IRI *href, SVGElement *parent)
{
	if (href->target && (href->target==parent)) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_href");
	} else {
		lsr_write_href(lsr, href);
	}
}


static void lsr_write_a(GF_LASeRCodec *lsr, SVGaElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
	lsr_write_fill(lsr, (SVGElement *) elt, clone);
	lsr_write_stroke(lsr, (SVGElement *) elt, clone);
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	GF_LSR_WRITE_INT(lsr, (elt->target!=NULL) ? 1 : 0, 1, "hasTarget");
	if (elt->target) lsr_write_byte_align_string(lsr, elt->target, "target");
	lsr_write_href(lsr, & elt->xlink->href);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_animate(GF_LASeRCodec *lsr, SVGanimateElement *elt, SVGElement *parent)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_accumulate(lsr, elt->anim->accumulate);
	lsr_write_additive(lsr, elt->anim->additive);
	lsr_write_anim_value(lsr, &elt->anim->by, "has_by");
	lsr_write_calc_mode(lsr, elt->anim->calcMode);
	lsr_write_anim_value(lsr, &elt->anim->from, "has_from");
	lsr_write_fraction_12(lsr, elt->anim->keySplines, "has_keySplines");
	lsr_write_fraction_12(lsr, elt->anim->keyTimes, "has_keyTimes");
	lsr_write_anim_values(lsr, &elt->anim->values, "has_values");
	lsr_write_time_list(lsr, elt->timing->begin, "has_begin");
	lsr_write_duration(lsr, &elt->timing->dur, "has_dur");
	GF_LSR_WRITE_INT(lsr, elt->anim->lsr_enabled ? 1 : 0, 1, "enabled");
	lsr_write_anim_fill(lsr, elt->timing->fill, "has_fill");
	lsr_write_anim_repeat(lsr, &elt->timing->repeatCount, "repeatCount");
	lsr_write_repeat_duration(lsr, &elt->timing->repeatDur, "has_repeatDur");
	lsr_write_anim_restart(lsr, elt->timing->restart, "has_restart");
	lsr_write_anim_value(lsr, &elt->anim->to, "has_to");
	lsr_write_animatable(lsr, &elt->anim->attributeName, elt->xlink->href.target, "has_attributeName");
	lsr_write_href_anim(lsr, & elt->xlink->href, parent);

	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	/*and destroy proto*/
	gf_node_unregister((GF_Node *)clone, NULL);
}


static void lsr_write_animateMotion(GF_LASeRCodec *lsr, SVGanimateMotionElement *elt, SVGElement *parent)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_accumulate(lsr, elt->anim->accumulate);
	lsr_write_additive(lsr, elt->anim->additive);
	lsr_write_anim_value(lsr, &elt->anim->by, "has_by");
	lsr_write_calc_mode(lsr, elt->anim->calcMode);
	lsr_write_anim_value(lsr, &elt->anim->from, "has_from");
	lsr_write_fraction_12(lsr, elt->anim->keySplines, "has_keySplines");
	lsr_write_fraction_12(lsr, elt->anim->keyTimes, "has_keyTimes");
	lsr_write_anim_values(lsr, &elt->anim->values, "has_values");
	lsr_write_time_list(lsr, elt->timing->begin, "has_begin");
	lsr_write_duration(lsr, &elt->timing->dur, "has_dur");
	GF_LSR_WRITE_INT(lsr, elt->anim->lsr_enabled ? 1 : 0, 1, "enabled");
	lsr_write_anim_fill(lsr, elt->timing->fill, "has_fill");
	lsr_write_anim_repeat(lsr, &elt->timing->repeatCount, "has_repeatCount");
	lsr_write_repeat_duration(lsr, &elt->timing->repeatDur, "has_repeatDur");
	lsr_write_anim_restart(lsr, elt->timing->restart, "has_restart");
	lsr_write_anim_value(lsr, &elt->anim->to, "has_to");

	lsr_write_float_list(lsr, elt->keyPoints, "keyPoints");
	if (gf_list_count(elt->path.commands)) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasPath");
		lsr_write_path_type(lsr, &elt->path, "path");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasPath");
	}
	lsr_write_rotate_type(lsr, elt->rotate, "rotate");

	lsr_write_href_anim(lsr, & elt->xlink->href, parent);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	/*and destroy proto*/
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_animateTransform(GF_LASeRCodec *lsr, SVGanimateTransformElement *elt, SVGElement *parent)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_accumulate(lsr, elt->anim->accumulate);
	lsr_write_additive(lsr, elt->anim->additive);
	lsr_write_anim_value(lsr, &elt->anim->by, "has_by");
	lsr_write_calc_mode(lsr, elt->anim->calcMode);
	lsr_write_anim_value(lsr, &elt->anim->from, "has_from");
	lsr_write_fraction_12(lsr, elt->anim->keySplines, "has_keySplines");
	lsr_write_fraction_12(lsr, elt->anim->keyTimes, "has_keyTimes");
	lsr_write_anim_values(lsr, &elt->anim->values, "has_values");
	lsr_write_time_list(lsr, elt->timing->begin, "has_begin");
	lsr_write_duration(lsr, &elt->timing->dur, "has_dur");
	GF_LSR_WRITE_INT(lsr, elt->anim->lsr_enabled ? 1 : 0, 1, "enabled");
	lsr_write_anim_fill(lsr, elt->timing->fill, "has_fill");
	lsr_write_anim_repeat(lsr, &elt->timing->repeatCount, "has_repeatCount");
	lsr_write_repeat_duration(lsr, &elt->timing->repeatDur, "has_repeatDur");
	lsr_write_anim_restart(lsr, elt->timing->restart, "has_restart");
	lsr_write_anim_value(lsr, &elt->anim->to, "has_to");
	lsr_write_animatable(lsr, &elt->anim->attributeName, elt->xlink->href.target, "has_attributeName");

	/*enumeration rotate{0} scale{1} skewX{2} skewY{3} translate{4}*/
	switch (elt->anim->type) {
	case SVG_TRANSFORM_ROTATE: GF_LSR_WRITE_INT(lsr, 0, 3, "rotscatra"); break;
	case SVG_TRANSFORM_SCALE: GF_LSR_WRITE_INT(lsr, 1, 3, "rotscatra"); break;
	case SVG_TRANSFORM_SKEWX: GF_LSR_WRITE_INT(lsr, 2, 3, "rotscatra"); break;
	case SVG_TRANSFORM_SKEWY: GF_LSR_WRITE_INT(lsr, 3, 3, "rotscatra"); break;
	case SVG_TRANSFORM_TRANSLATE: GF_LSR_WRITE_INT(lsr, 4, 3, "rotscatra"); break;
	}

	lsr_write_href_anim(lsr, & elt->xlink->href, parent);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	/*and destroy proto*/
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_audio(GF_LASeRCodec *lsr, SVGaudioElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_time_list(lsr, elt->timing->begin, "has_begin");
	lsr_write_duration(lsr, &elt->timing->dur, "has_dur");
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_anim_repeat(lsr, &elt->timing->repeatCount, "has_repeatCount");
	lsr_write_repeat_duration(lsr, &elt->timing->repeatDur, "has_repeatDur");
	lsr_write_sync_behavior(lsr, elt->sync->syncBehavior, "syncBehavior");
	lsr_write_sync_tolerance(lsr, &elt->sync->syncTolerance, "syncBehavior");
	lsr_write_content_type(lsr, elt->xlink->type, "type");
	lsr_write_href(lsr, & elt->xlink->href);

	lsr_write_clip_time(lsr, elt->timing->clipBegin, "clipBegin");
	lsr_write_clip_time(lsr, elt->timing->clipEnd, "clipEnd");
	GF_LSR_WRITE_INT(lsr, elt->sync->syncReference ? 1 : 0, 1, "hasSyncReference");
	if (elt->sync->syncReference) lsr_write_any_uri_string(lsr, elt->sync->syncReference, "syncReference");

	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_circle(GF_LASeRCodec *lsr, SVGcircleElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
	lsr_write_fill(lsr, (SVGElement*)elt, clone);
	lsr_write_stroke(lsr, (SVGElement*)elt, clone);
	lsr_write_coordinate(lsr, elt->cx.value, 1, "cx");
	lsr_write_coordinate(lsr, elt->cy.value, 1, "cy");
	lsr_write_coordinate(lsr, elt->r.value, 0, "r");
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_cursor(GF_LASeRCodec *lsr, SVGElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_coordinate(lsr, /*elt->x*/ 0, 1, "x");
	lsr_write_coordinate(lsr, /*elt->x*/ 0, 1, "y");
	lsr_write_href(lsr, /*& elt->xlink->href*/ NULL);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_data(GF_LASeRCodec *lsr, SVGdescElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_defs(GF_LASeRCodec *lsr, SVGdefsElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_fill(lsr, (SVGElement*)elt, clone);
	lsr_write_stroke(lsr, (SVGElement*)elt, clone);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_ellipse(GF_LASeRCodec *lsr, SVGellipseElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
	lsr_write_fill(lsr, (SVGElement*)elt, clone);
	lsr_write_stroke(lsr, (SVGElement*)elt, clone);
	lsr_write_coordinate(lsr, elt->cx.value, 1, "cx");
	lsr_write_coordinate(lsr, elt->cy.value, 1, "cy");
	lsr_write_coordinate(lsr, elt->rx.value, 0, "rx");
	lsr_write_coordinate(lsr, elt->ry.value, 0, "ry");
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_foreignObject(GF_LASeRCodec *lsr, SVGforeignObjectElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
	lsr_write_fill(lsr, (SVGElement*)elt, clone);
	lsr_write_stroke(lsr, (SVGElement*)elt, clone);
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_coordinate(lsr, elt->height.value, 0, "height");
	lsr_write_coordinate(lsr, elt->width.value, 0, "width");
	lsr_write_coordinate(lsr, elt->x.value, 1, "x");
	lsr_write_coordinate(lsr, elt->y.value, 1, "y");

	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
/*	TODO
	bit(1) opt_group;
	if(opt_group) {
		vluimsbf5 occ1;
		for(int t=0;t<occ1;t++) {
			privateElementContainer child0[[t]];
		}
	}
*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
	gf_node_unregister((GF_Node *)clone, NULL);
}

static Bool lsr_group_equal(SVGgElement *g1, SVGgElement *g2)
{
	if (g1->lsr_size.height != g2->lsr_size.height) return 0;
	if (g1->lsr_size.width != g2->lsr_size.width) return 0;
	if (g1->lsr_choice.type != g2->lsr_choice.type) return 0;
	if (g1->lsr_choice.choice_index != g2->lsr_choice.choice_index) return 0;
	return 1;
}

static void lsr_write_g(GF_LASeRCodec *lsr, SVGgElement *elt, Bool ommit_tag)
{
	SVGElement *clone = NULL;
	Bool is_same = 0;

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_g, 0)
		&& lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_g) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_g->transform) 
		&& lsr_group_equal(elt, lsr->prev_g)
		) {
		/*samegType*/
		GF_LSR_WRITE_INT(lsr, 23, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		is_same = 1;
	} else {
		/*gType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, 12, 6, "ch4"); 
		clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
		gf_node_register((GF_Node *)clone, NULL);
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement*)elt, clone);
		lsr_write_stroke(lsr, (SVGElement*)elt, clone);
		if (elt->lsr_choice.type==LASeR_CHOICE_ALL) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasChoice");
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasChoice");
			if (elt->lsr_choice.type==LASeR_CHOICE_N) {
				GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
				GF_LSR_WRITE_INT(lsr, elt->lsr_choice.choice_index, 8, "value");
			} else {
				GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
				GF_LSR_WRITE_INT(lsr, elt->lsr_choice.type, 2, "type");
			}
		}
		GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
		if (elt->lsr_size.enabled) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "size");
			lsr_write_coordinate(lsr, elt->lsr_size.width, 0, "width");
			lsr_write_coordinate(lsr, elt->lsr_size.height, 0, "height");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "size");
		}

		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		gf_node_unregister((GF_Node *)clone, NULL);
		lsr->prev_g = elt;
	}
	lsr_write_group_content(lsr, (SVGElement *) elt, is_same);
}
static void lsr_write_image(GF_LASeRCodec *lsr, SVGimageElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_coordinate(lsr, elt->height.value, 1, "height");

	if (elt->properties->opacity.type == SVG_NUMBER_VALUE) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "opacity");
		lsr_write_fixed_clamp(lsr, elt->properties->opacity.value, "opacity");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "opacity");
	}

	lsr_write_transform_behavior(lsr, 0, "transformBehavior");
	lsr_write_content_type(lsr, elt->xlink->type, "type");
	lsr_write_coordinate(lsr, elt->width.value, 1, "width");
	lsr_write_coordinate(lsr, elt->x.value, 1, "x");
	lsr_write_coordinate(lsr, elt->y.value, 1, "y");
	lsr_write_href(lsr, &elt->xlink->href);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_line(GF_LASeRCodec *lsr, SVGlineElement *elt, Bool ommit_tag)
{
	SVGElement *clone = NULL;
	Bool is_same = 0;

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_line, 0)
		&& lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_line) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_line->transform) 
		) {
		/*samelineType*/
		GF_LSR_WRITE_INT(lsr, 24, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		is_same = 1;
	} else {
		/*lineType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, 14, 6, "ch4"); 
		clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
		gf_node_register((GF_Node *)clone, NULL);
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_stroke(lsr, (SVGElement *) elt, clone);
	}

	lsr_write_coordinate(lsr, elt->x1.value, 1, "x1");
	lsr_write_coordinate(lsr, elt->x2.value, 0, "x2");
	lsr_write_coordinate(lsr, elt->y1.value, 1, "y1");
	lsr_write_coordinate(lsr, elt->y2.value, 0, "y2");
	if (!is_same) {
		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		gf_node_unregister((GF_Node *)clone, NULL);
		lsr->prev_line = elt;
	}
	lsr_write_group_content(lsr, (SVGElement *) elt, is_same);
}
static void lsr_write_linearGradient(GF_LASeRCodec *lsr, SVGlinearGradientElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_fill(lsr, (SVGElement *) elt, clone);
	lsr_write_stroke(lsr, (SVGElement *) elt, clone);
	/*enumeration objectBoundingBox{0} userSpaceOnUse{1}*/
	if (elt->gradientUnits == SVG_GRADIENTUNITS_USER) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasGradientUnits");
		GF_LSR_WRITE_INT(lsr, 1, 1, "radientUnits");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasGradientUnits");
	}
	lsr_write_coordinate(lsr, elt->x1.value, 1, "x1");
	lsr_write_coordinate(lsr, elt->x2.value, 1, "x2");
	lsr_write_coordinate(lsr, elt->y1.value, 1, "y1");
	lsr_write_coordinate(lsr, elt->y2.value, 1, "y2");
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_mpath(GF_LASeRCodec *lsr, SVGmpathElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_href(lsr, &elt->xlink->href);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_path(GF_LASeRCodec *lsr, SVGpathElement *elt, Bool ommit_tag)
{
	SVGElement *clone;
	Bool is_same = 0;

	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_path, 0) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_path->transform) 
		&& lsr_number_equal(&elt->pathLength, &lsr->prev_path->pathLength) 
	) {
		is_same = 1;
		if (!lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_path)) {
			is_same = 2;
			/*samepathfillType*/
			GF_LSR_WRITE_INT(lsr, 26, 6, "ch4"); 
		} else {
			/*samepathType*/
			GF_LSR_WRITE_INT(lsr, 25, 6, "ch4"); 
		}
		lsr_write_id(lsr, (GF_Node *) elt);
		if (is_same == 2) lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_path_type(lsr, &elt->d, "d");
	} else {
		/*pathType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, 18, 6, "ch4"); 

		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_stroke(lsr, (SVGElement *) elt, clone);
		lsr_write_path_type(lsr, &elt->d, "d");
		if (elt->pathLength.value) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasPathLength");
			lsr_write_fixed_16_8(lsr, elt->pathLength.value, "pathLength");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasPathLength");
		}
		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		lsr->prev_path = elt;
	}
	gf_node_unregister((GF_Node *)clone, NULL);
	lsr_write_group_content(lsr, (SVGElement *) elt, is_same);
}
static void lsr_write_polygon(GF_LASeRCodec *lsr, SVGpolygonElement *elt, Bool is_polyline, Bool ommit_tag)
{
	SVGElement *clone;
	Bool same_type = 0;

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_polygon, 1) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_polygon->transform) 
		) {
		Bool same_fill = !lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_polygon);
		Bool same_stroke = !lsr_elt_has_same_stroke(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_polygon);
		if (!same_fill && !same_stroke) {
		} else if (same_fill) same_type = same_stroke ? 1 : 2;
		else if (same_stroke) same_type = 3;
	}

	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	if (same_type) {
		/*samepolygonType / samepolylineType */
		u8 type = is_polyline ? 30 : 27;
		/*samepolygonfillType / samepolylinefillType */
		if (same_type==2) type = is_polyline ? 31 : 28;
		/*samepolygonstrokeType / samepolylinestrokeType */
		else if (same_type==3) type = is_polyline ? 32 : 29;

		GF_LSR_WRITE_INT(lsr, type, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		if (same_type==2) lsr_write_fill(lsr, (SVGElement *) elt, clone);
		else if (same_type==3) lsr_write_stroke(lsr, (SVGElement *) elt, clone);
		lsr_write_point_sequence(lsr, elt->points, "points");
	} else {
		/*polygon/polyline*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, is_polyline ? 20 : 19, 6, "ch4"); 

		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_stroke(lsr, (SVGElement *) elt, clone);
		lsr_write_point_sequence(lsr, elt->points, "points");
		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		lsr->prev_polygon = elt;
	}
	gf_node_unregister((GF_Node *)clone, NULL);
	lsr_write_group_content(lsr, (SVGElement *) elt, same_type);
}
static void lsr_write_radialGradient(GF_LASeRCodec *lsr, SVGradialGradientElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_fill(lsr, (SVGElement *) elt, clone);
	lsr_write_stroke(lsr, (SVGElement *) elt, clone);
	lsr_write_coordinate(lsr, elt->cx.value, 1, "cx");
	lsr_write_coordinate(lsr, elt->cy.value, 1, "cy");

	/*enumeration objectBoundingBox{0} userSpaceOnUse{1}*/
	if (elt->gradientUnits == SVG_GRADIENTUNITS_USER) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasGradientUnits");
		GF_LSR_WRITE_INT(lsr, 1, 1, "radientUnits");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasGradientUnits");
	}
	lsr_write_coordinate(lsr, elt->r.value, 1, "r");
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_rect(GF_LASeRCodec *lsr, SVGrectElement *elt, Bool ommit_tag)
{
	SVGElement *clone;
	Bool same_type = 0;

	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_rect, 0) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_rect->transform) 
		&& lsr_number_equal(&elt->rx, &lsr->prev_rect->rx) 
		&& lsr_number_equal(&elt->ry, &lsr->prev_rect->ry) 
		) {
		if (!lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_rect)) {
			same_type = 2;
			/*samerectfillType*/
			GF_LSR_WRITE_INT(lsr, 34, 6, "ch4"); 
		} else {
			same_type = 1;
			/*samerectType*/
			GF_LSR_WRITE_INT(lsr, 33, 6, "ch4"); 
		}
		lsr_write_id(lsr, (GF_Node *) elt);
		if (same_type==2) lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_coordinate(lsr, elt->height.value, 0, "height");
		lsr_write_coordinate(lsr, elt->width.value, 0, "width");
		lsr_write_coordinate(lsr, elt->x.value, 1, "x");
		lsr_write_coordinate(lsr, elt->y.value, 1, "y");
	} else {
		/*rectType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, 22, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_stroke(lsr, (SVGElement *) elt, clone);
		lsr_write_coordinate(lsr, elt->height.value, 0, "height");
		lsr_write_coordinate(lsr, elt->rx.value, 1, "rx");
		lsr_write_coordinate(lsr, elt->ry.value, 1, "ry");
		lsr_write_coordinate(lsr, elt->width.value, 0, "width");
		lsr_write_coordinate(lsr, elt->x.value, 1, "x");
		lsr_write_coordinate(lsr, elt->y.value, 1, "y");
		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		lsr->prev_rect = elt;
	}
	gf_node_unregister((GF_Node *)clone, NULL);
	lsr_write_group_content(lsr, (SVGElement *) elt, same_type);
}
static void lsr_write_script(GF_LASeRCodec *lsr, SVGscriptElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_single_time(lsr, NULL/*&elt->timing->begin*/, "begin");
	GF_LSR_WRITE_INT(lsr, /*elt->anim->lsr_enabled ? 1 : 0*/ 1, 1, "enabled");
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_script_type(lsr, elt->xlink->type, "type");
	lsr_write_href(lsr, &elt->xlink->href);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_command_list(lsr, elt->lsr_script.com_list, elt);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_set(GF_LASeRCodec *lsr, SVGsetElement *elt, SVGElement *parent)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_time_list(lsr, elt->timing->begin, "has_begin");
	lsr_write_duration(lsr, &elt->timing->dur, "has_dur");
	GF_LSR_WRITE_INT(lsr, elt->anim->lsr_enabled ? 1 : 0, 1, "enabled");
	lsr_write_anim_fill(lsr, elt->timing->fill, "has_fill");
	lsr_write_anim_repeat(lsr, &elt->timing->repeatCount, "has_repeatCount");
	lsr_write_repeat_duration(lsr, &elt->timing->repeatDur, "has_repeatDur");
	lsr_write_anim_restart(lsr, elt->timing->restart, "has_restart");
	lsr_write_anim_value(lsr, &elt->anim->to, "has_to");
	lsr_write_animatable(lsr, &elt->anim->attributeName, elt->xlink->href.target, "has_attributeName");
	lsr_write_href_anim(lsr, &elt->xlink->href, parent);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_stop(GF_LASeRCodec *lsr, SVGstopElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_fill(lsr, (SVGElement *) elt, clone);
	lsr_write_stroke(lsr, (SVGElement *) elt, clone);
	lsr_write_fixed_16_8(lsr, elt->offset.value, "offset");
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}
static void lsr_write_svg(GF_LASeRCodec *lsr, SVGsvgElement *elt)
{
	SMIL_Duration snap;
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_fill(lsr, (SVGElement *) elt, clone);
	lsr_write_stroke(lsr, (SVGElement *) elt, clone);
	lsr_write_string_attribute(lsr, elt->baseProfile, "baseProfile");
	lsr_write_string_attribute(lsr, elt->contentScriptType, "contentScriptType");
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_value_with_units(lsr, &elt->height, "height");
	if (elt->playbackOrder==SVG_PLAYBACKORDER_FORWARDONLY) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasPlaybackOrder");
		GF_LSR_WRITE_INT(lsr, 1, 1, "playbackOrder");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasPlaybackOrder");
	}
	if (elt->preserveAspectRatio.align==SVG_PRESERVEASPECTRATIO_XMIDYMID) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasPreserveAR");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasPreserveAR");
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice (meetOrSlice)");
		GF_LSR_WRITE_INT(lsr, elt->preserveAspectRatio.defer ? 1 : 0, 1, "choice (defer)");
		switch (elt->preserveAspectRatio.align) {
		case SVG_PRESERVEASPECTRATIO_XMAXYMAX: GF_LSR_WRITE_INT(lsr, 1, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMAXYMID: GF_LSR_WRITE_INT(lsr, 2, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMAXYMIN: GF_LSR_WRITE_INT(lsr, 3, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMIDYMAX: GF_LSR_WRITE_INT(lsr, 4, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMIDYMID: GF_LSR_WRITE_INT(lsr, 5, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMIDYMIN: GF_LSR_WRITE_INT(lsr, 6, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMINYMAX: GF_LSR_WRITE_INT(lsr, 7, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMINYMID: GF_LSR_WRITE_INT(lsr, 8, 4, "alignXandY"); break;
		case SVG_PRESERVEASPECTRATIO_XMINYMIN: GF_LSR_WRITE_INT(lsr, 9, 4, "alignXandY"); break;
		default: GF_LSR_WRITE_INT(lsr, 0, 4, "alignXandY"); break;
		}
	}

	snap.type = elt->snapshotTime ? SMIL_DURATION_DEFINED : SMIL_DURATION_INDEFINITE;
	snap.clock_value = elt->snapshotTime;
	lsr_write_duration(lsr, &snap, "snapshotTime");

	if (!elt->sync->syncBehaviorDefault) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasSyncBehavior");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasSyncBehavior");
		switch (elt->sync->syncBehaviorDefault) {
		case SMIL_SYNCBEHAVIOR_CANSLIP: GF_LSR_WRITE_INT(lsr, 0, 2, "syncBehavior"); break;
		case SMIL_SYNCBEHAVIOR_INDEPENDENT: GF_LSR_WRITE_INT(lsr, 1, 2, "syncBehavior"); break;
		case SMIL_SYNCBEHAVIOR_LOCKED: GF_LSR_WRITE_INT(lsr, 3, 2, "syncBehavior"); break;
		default: GF_LSR_WRITE_INT(lsr, 0, 2, "syncBehavior"); break;
		}
	}
	if (elt->sync->syncToleranceDefault.type != SMIL_SYNCTOLERANCE_VALUE) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasSyncTolerance");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasSyncTolerance");
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
	    lsr_write_vluimsbf5(lsr, (u32) (elt->sync->syncToleranceDefault.value*lsr->time_resolution), "value");
	}
	if (elt->timelineBegin == SVG_TIMELINEBEGIN_ONSTART) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasTimelineBegin");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasTimelineBegin");
		GF_LSR_WRITE_INT(lsr, 1, 1, "timelineBegin");
	}
	lsr_write_string_attribute(lsr, elt->version, "version");
	if (!elt->viewBox.width && !elt->viewBox.height) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasViewBox");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasViewBox");
		lsr_write_fixed_16_8(lsr, elt->viewBox.x, "viewbox.x");
		lsr_write_fixed_16_8(lsr, elt->viewBox.y, "viewbox.y");
		lsr_write_fixed_16_8(lsr, elt->viewBox.width, "viewbox.width");
		lsr_write_fixed_16_8(lsr, elt->viewBox.height, "viewbox.height");
	}
	lsr_write_value_with_units(lsr, &elt->width, "width");
	/*zoom and pan must be encoded in our code...*/
	GF_LSR_WRITE_INT(lsr, 1, 1, "hasZoomAndPan");
	GF_LSR_WRITE_INT(lsr, (elt->zoomAndPan==SVG_ZOOMANDPAN_MAGNIFY) ? 1 : 0, 1, "zoomAndPan");	
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_switch(GF_LASeRCodec *lsr, SVGswitchElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
	lsr_write_fill(lsr, (SVGElement*)elt, clone);
	lsr_write_stroke(lsr, (SVGElement*)elt, clone);
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_text(GF_LASeRCodec *lsr, SVGtextElement *elt, Bool ommit_tag)
{
	SVGElement *clone;
	u32 same_type = 0;

	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_text, 0) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_text->transform) 
		&& (elt->editable==lsr->prev_text->editable) 
		&& lsr_float_list_equal(elt->rotate, lsr->prev_text->rotate) 
	) {
		if (!lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_text)) {
			same_type = 2;
			/*sametextfillType*/
			GF_LSR_WRITE_INT(lsr, 36, 6, "ch4"); 
		} else {
			same_type = 1;
			/*sametextType*/
			GF_LSR_WRITE_INT(lsr, 35, 6, "ch4"); 
		}
		lsr_write_id(lsr, (GF_Node *) elt);
		if (same_type==2) lsr_write_fill(lsr, (SVGElement *) elt, clone);
		lsr_write_coord_list(lsr, elt->x, "x");
		lsr_write_coord_list(lsr, elt->y, "y");
	} else {
		/*textType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, 42, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement*)elt, clone);
		lsr_write_stroke(lsr, (SVGElement*)elt, clone);

		GF_LSR_WRITE_INT(lsr, elt->editable ? 1 : 0, 1, "editable");
		lsr_write_float_list(lsr, elt->rotate, "rotate");
		lsr_write_coord_list(lsr, elt->x, "x");
		lsr_write_coord_list(lsr, elt->y, "y");
		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		lsr->prev_text = elt;
	}
	gf_node_unregister((GF_Node *)clone, NULL);
	lsr_write_group_content(lsr, (SVGElement *) elt, same_type);
}

static void lsr_write_tspan(GF_LASeRCodec *lsr, SVGtspanElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_fill(lsr, (SVGElement*)elt, clone);
	lsr_write_stroke(lsr, (SVGElement*)elt, clone);
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_use(GF_LASeRCodec *lsr, SVGuseElement *elt, Bool ommit_tag)
{
	SVGElement *clone;
	Bool is_same = 0;

	if (!ommit_tag && lsr_elt_has_same_base(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_use, 0) 
		&& lsr_elt_has_same_fill(lsr, (SVGElement *)elt, (SVGElement *)lsr->prev_use) 
		&& lsr_transform_equal(&elt->transform, &lsr->prev_use->transform) 
		&& (elt->core->eRR == lsr->prev_use->core->eRR)
		&& lsr_number_equal(&elt->x, &lsr->prev_use->x)
		&& lsr_number_equal(&elt->y, &lsr->prev_use->y)
		/*TODO check overflow*/
	) {
		is_same = 1;
		/*sameuseType*/
		GF_LSR_WRITE_INT(lsr, 37, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_href(lsr, &elt->xlink->href);
	} else {
		clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
		gf_node_register((GF_Node *)clone, NULL);
		/*useType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, 45, 6, "ch4"); 
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare_full(lsr, (GF_Node *) elt, (GF_Node *) clone, &elt->transform);
		lsr_write_fill(lsr, (SVGElement*)elt, clone);
		lsr_write_stroke(lsr, (SVGElement*)elt, clone);
		GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
		/*TODO ( one value only??) */
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasOverflow");
		lsr_write_coordinate(lsr, elt->x.value, 1, "x");
		lsr_write_coordinate(lsr, elt->y.value, 1, "y");
		lsr_write_href(lsr, &elt->xlink->href);
		lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
		gf_node_unregister((GF_Node *)clone, NULL);
		lsr->prev_use = elt;
	}
	lsr_write_group_content(lsr, (SVGElement *) elt, is_same);
}

static void lsr_write_video(GF_LASeRCodec *lsr, SVGvideoElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	/*no transform here (transformBehavior instead)*/
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);
	lsr_write_time_list(lsr, elt->timing->begin, "has_begin");
	lsr_write_duration(lsr, &elt->timing->dur, "has_dur");
	GF_LSR_WRITE_INT(lsr, elt->core->eRR, 1, "externalResourcesRequired");
	lsr_write_coordinate(lsr, elt->height.value, 1, "height");

	if (elt->overlay==SVG_OVERLAY_NONE) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasOverlay");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasOverlay");
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		switch (elt->overlay) {
		case SVG_OVERLAY_NONE: GF_LSR_WRITE_INT(lsr, 1, 1, "overlay"); break;
		case SVG_OVERLAY_TOP: GF_LSR_WRITE_INT(lsr, 2, 1, "overlay"); break;
		case SVG_OVERLAY_FULLSCREEN: 
		default: GF_LSR_WRITE_INT(lsr, 0, 1, "overlay");  break;
		}
	}

	lsr_write_anim_repeat(lsr, &elt->timing->repeatCount, "has_repeatCount");
	lsr_write_repeat_duration(lsr, &elt->timing->repeatDur, "has_repeatDur");
	lsr_write_sync_behavior(lsr, elt->sync->syncBehavior, "syncBehavior");
	lsr_write_sync_tolerance(lsr, &elt->sync->syncTolerance, "syncBehavior");
	lsr_write_transform_behavior(lsr, 0, "transformBehavior");
	lsr_write_content_type(lsr, elt->xlink->type, "type");
	lsr_write_coordinate(lsr, elt->width.value, 1, "width");
	lsr_write_coordinate(lsr, elt->x.value, 1, "x");
	lsr_write_coordinate(lsr, elt->y.value, 1, "y");
	lsr_write_href(lsr, & elt->xlink->href);

	lsr_write_clip_time(lsr, elt->timing->clipBegin, "clipBegin");
	lsr_write_clip_time(lsr, elt->timing->clipEnd, "clipEnd");
	GF_LSR_WRITE_INT(lsr, elt->sync->syncReference ? 1 : 0, 1, "hasSyncReference");
	if (elt->sync->syncReference) lsr_write_any_uri_string(lsr, elt->sync->syncReference, "syncReference");
	
	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_listener(GF_LASeRCodec *lsr, SVGlistenerElement *elt)
{
	SVGElement *clone;
	clone = (SVGElement *) gf_node_new(lsr->sg, gf_node_get_tag((GF_Node *)elt) );
	gf_node_register((GF_Node *)clone, NULL);
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt, (GF_Node *) clone);

	GF_LSR_WRITE_INT(lsr, elt->defaultAction ? 1 : 0, 1, "hasDefaultAction");
	if (elt->defaultAction) GF_LSR_WRITE_INT(lsr, 1, 1, "defaultAction");
	GF_LSR_WRITE_INT(lsr, 1/*elt->endabled*/, 1, "enabled");
	if (elt->event.type != SVG_DOM_EVT_UNKNOWN) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasEvent");
		lsr_write_event_type(lsr, elt->event.type, elt->event.parameter);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasEvent");
	}
	if (elt->handler.iri || (elt->handler.target && gf_node_get_id((GF_Node *)elt->handler.target) ) ) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasHandler");
		lsr_write_any_uri(lsr, &elt->handler, "handler");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasHandler");
	}
	if (elt->observer.target && gf_node_get_id((GF_Node *)elt->observer.target) ) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasObserver");
		lsr_write_codec_IDREF(lsr, &elt->observer, "observer");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasObserver");
	}

	if (elt->phase != XMLEVENT_PHASE_DEFAULT) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasPhase");
		GF_LSR_WRITE_INT(lsr, 0, 1, "phase");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasPhase");
	}
	if (elt->propagate) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasPropagate");
		GF_LSR_WRITE_INT(lsr, 1, 1, "propagate");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasPropagate");
	}
	if (elt->target.target && gf_node_get_id((GF_Node *)elt->target.target) ) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasTarget");
		lsr_write_codec_IDREF(lsr, &elt->target, "target");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasTarget");
	}
	if (elt->lsr_delay != 0) {
		s32 val = (s32) (lsr->time_resolution*elt->lsr_delay);
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasDelay");
		GF_LSR_WRITE_INT(lsr, (val>0) ? 0 : 11, 1, "sign");
		if (val<0) val = -val;
		lsr_write_vluimsbf5(lsr, val, "delay");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasDelay");
	}
	if (elt->lsr_timeAttribute) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasTimeAttribute");
		GF_LSR_WRITE_INT(lsr, (elt->lsr_timeAttribute==LASeR_TIMEATTRIBUTE_END) ? 1 : 0, 1, "timeAttribute");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasTimeAttribute");
	}

	lsr_write_any_attribute(lsr, (GF_Node *) elt, clone, 1);
	lsr_write_group_content(lsr, (SVGElement *) elt, 0);
	gf_node_unregister((GF_Node *)clone, NULL);
}

static void lsr_write_scene_content_model(GF_LASeRCodec *lsr, SVGElement *parent, void *node)
{
	u32 tag = gf_node_get_tag(node);

	switch(tag) {
	case TAG_SVG_a: GF_LSR_WRITE_INT(lsr, 0, 6, "ch4"); lsr_write_a(lsr, node); break;
	case TAG_SVG_animate: GF_LSR_WRITE_INT(lsr, 1, 6, "ch4"); lsr_write_animate(lsr, node, parent); break;
	case TAG_SVG_animateColor: GF_LSR_WRITE_INT(lsr, 2, 6, "ch4"); lsr_write_animate(lsr, node, parent); break;
	case TAG_SVG_animateMotion: GF_LSR_WRITE_INT(lsr, 3, 6, "ch4"); lsr_write_animateMotion(lsr, node, parent); break;
	case TAG_SVG_animateTransform: GF_LSR_WRITE_INT(lsr, 4, 6, "ch4"); lsr_write_animateTransform(lsr, node, parent); break;
	case TAG_SVG_audio: GF_LSR_WRITE_INT(lsr, 5, 6, "ch4"); lsr_write_audio(lsr, node); break;
	case TAG_SVG_circle: GF_LSR_WRITE_INT(lsr, 6, 6, "ch4"); lsr_write_circle(lsr, node); break;
#if TODO_LASER_EXTENSIONS	
	case TAG_SVG_cursor: GF_LSR_WRITE_INT(lsr, 7, 6, "ch4"); lsr_write_cursor(lsr, node); break;
#endif
	case TAG_SVG_defs: GF_LSR_WRITE_INT(lsr, 8, 6, "ch4"); lsr_write_defs(lsr, node); break;
	case TAG_SVG_desc: GF_LSR_WRITE_INT(lsr, 9, 6, "ch4"); lsr_write_data(lsr, node); break;
	case TAG_SVG_ellipse: GF_LSR_WRITE_INT(lsr, 10, 6, "ch4"); lsr_write_ellipse(lsr, node); break;
	case TAG_SVG_foreignObject: GF_LSR_WRITE_INT(lsr, 11, 6, "ch4"); lsr_write_foreignObject(lsr, node); break;
	
	case TAG_SVG_g:
		/*type is written in encoding fct for sameg handling*/
		lsr_write_g(lsr, node, 0); break;
	case TAG_SVG_image: GF_LSR_WRITE_INT(lsr, 13, 6, "ch4"); lsr_write_image(lsr, node); break;
	case TAG_SVG_line: 
		/*type is written in encoding fct for sameline handling*/
		lsr_write_line(lsr, node, 0); break;
	case TAG_SVG_linearGradient: GF_LSR_WRITE_INT(lsr, 15, 6, "ch4"); lsr_write_linearGradient(lsr, node); break;
	case TAG_SVG_metadata: GF_LSR_WRITE_INT(lsr, 16, 6, "ch4"); lsr_write_data(lsr, node); break;
	case TAG_SVG_mpath: GF_LSR_WRITE_INT(lsr, 17, 6, "ch4"); lsr_write_mpath(lsr, node); break;
	case TAG_SVG_path: 
		/*type is written in encoding fct for samepath handling*/
		lsr_write_path(lsr, node, 0); break;
	case TAG_SVG_polygon: 
		/*type is written in encoding fct for samepolygon handling*/
		lsr_write_polygon(lsr, node, 0, 0); break;
	case TAG_SVG_polyline: 
		/*type is written in encoding fct for samepolyline handling*/
		lsr_write_polygon(lsr, node, 1, 0); break;
	case TAG_SVG_radialGradient: GF_LSR_WRITE_INT(lsr, 21, 6, "ch4"); lsr_write_radialGradient(lsr, node); break;
	case TAG_SVG_rect:
		/*type is written in encoding fct for samepolyline handling*/
		lsr_write_rect(lsr, node, 0); break;
	case TAG_SVG_script: GF_LSR_WRITE_INT(lsr, 38, 6, "ch4"); lsr_write_script(lsr, node); break;
	case TAG_SVG_set: GF_LSR_WRITE_INT(lsr, 39, 6, "ch4"); lsr_write_set(lsr, node, parent); break;
	case TAG_SVG_stop: GF_LSR_WRITE_INT(lsr, 40, 6, "ch4"); lsr_write_stop(lsr, node); break;
	case TAG_SVG_switch: GF_LSR_WRITE_INT(lsr, 41, 6, "ch4"); lsr_write_switch(lsr, node); break;
	case TAG_SVG_text:
		/*type is written in encoding fct for sametext handling*/
		lsr_write_text(lsr, node, 0); break;
	case TAG_SVG_title: GF_LSR_WRITE_INT(lsr, 43, 6, "ch4"); lsr_write_data(lsr, node); break;
	case TAG_SVG_tspan: GF_LSR_WRITE_INT(lsr, 44, 6, "ch4"); lsr_write_tspan(lsr, node); break;
	case TAG_SVG_use: 
		/*type is written in encoding fct for sameuse handling*/
		lsr_write_use(lsr, node, 0); break;
	case TAG_SVG_video: GF_LSR_WRITE_INT(lsr, 46, 6, "ch4"); lsr_write_video(lsr, node); break;
	case TAG_SVG_listener: GF_LSR_WRITE_INT(lsr, 47, 6, "ch4"); lsr_write_listener(lsr, node); break;
#if 0
	/*case extend*/
		GF_LSR_WRITE_INT(lsr, 48, 6, "ch4"); 
		lsr_write_extend_class(lsr, NULL, 0, node); 
		break;
	/*case privateElement*/
	case TAG_SVG_extendElement: 
		GF_LSR_WRITE_INT(lsr, 49, 6, "ch4"); 
		lsr_write_private_element_container(lsr); 
		break;
#endif

	default:
		/*hack for encoding - needs cleaning*/
		fprintf(stdout, "Warning: node %s not part of LASeR children nodes - skipping\n", gf_node_get_class_name(node));
		GF_LSR_WRITE_INT(lsr, 50, 6, "ch4"); 
		lsr_write_byte_align_string(lsr, "", "textContent");
		break;
	}
}
static void lsr_write_content_model_36(GF_LASeRCodec *lsr, SVGElement *parent, void *node)
{
	u32 tag = gf_node_get_tag(node);
	GF_LSR_WRITE_INT(lsr, 0, 1, "ch4"); 
	switch(tag) {
	case TAG_SVG_a: GF_LSR_WRITE_INT(lsr, 0, 6, "ch6"); lsr_write_a(lsr, node); break;
	case TAG_SVG_animate: GF_LSR_WRITE_INT(lsr, 1, 6, "ch6"); lsr_write_animate(lsr, node, parent); break;
	case TAG_SVG_animateColor: GF_LSR_WRITE_INT(lsr, 2, 6, "ch6"); lsr_write_animate(lsr, node, parent); break;
	case TAG_SVG_animateMotion: GF_LSR_WRITE_INT(lsr, 3, 6, "ch6"); lsr_write_animateMotion(lsr, node, parent); break;
	case TAG_SVG_animateTransform: GF_LSR_WRITE_INT(lsr, 4, 6, "ch6"); lsr_write_animateTransform(lsr, node, parent); break;
	case TAG_SVG_audio: GF_LSR_WRITE_INT(lsr, 5, 6, "ch6"); lsr_write_audio(lsr, node); break;
	case TAG_SVG_circle: GF_LSR_WRITE_INT(lsr, 6, 6, "ch6"); lsr_write_circle(lsr, node); break;
#if TODO_LASER_EXTENSIONS	
	case TAG_SVG_cursor: GF_LSR_WRITE_INT(lsr, 7, 6, "ch6"); lsr_write_cursor(lsr, node); break;
#endif
	case TAG_SVG_defs: GF_LSR_WRITE_INT(lsr, 8, 6, "ch6"); lsr_write_defs(lsr, node); break;
	case TAG_SVG_desc: GF_LSR_WRITE_INT(lsr, 9, 6, "ch6"); lsr_write_data(lsr, node); break;
	case TAG_SVG_ellipse: GF_LSR_WRITE_INT(lsr, 10, 6, "ch6"); lsr_write_ellipse(lsr, node); break;
	case TAG_SVG_foreignObject: GF_LSR_WRITE_INT(lsr, 11, 6, "ch6"); lsr_write_foreignObject(lsr, node); break;
	case TAG_SVG_g: GF_LSR_WRITE_INT(lsr, 12, 6, "ch6"); lsr_write_g(lsr, node, 1); break;
	case TAG_SVG_image: GF_LSR_WRITE_INT(lsr, 13, 6, "ch6"); lsr_write_image(lsr, node); break;
	case TAG_SVG_line: GF_LSR_WRITE_INT(lsr, 14, 6, "ch6"); lsr_write_line(lsr, node, 1); break;
	case TAG_SVG_linearGradient: GF_LSR_WRITE_INT(lsr, 15, 6, "ch6"); lsr_write_linearGradient(lsr, node); break;
	case TAG_SVG_metadata: GF_LSR_WRITE_INT(lsr, 16, 6, "ch6"); lsr_write_data(lsr, node); break;
	case TAG_SVG_mpath: GF_LSR_WRITE_INT(lsr, 17, 6, "ch6"); lsr_write_mpath(lsr, node); break;
	case TAG_SVG_path: GF_LSR_WRITE_INT(lsr, 18, 6, "ch6"); lsr_write_path(lsr, node, 1); break;
	case TAG_SVG_polygon: GF_LSR_WRITE_INT(lsr, 19, 6, "ch6"); lsr_write_polygon(lsr, node, 0, 1); break;
	case TAG_SVG_polyline: GF_LSR_WRITE_INT(lsr, 20, 6, "ch6"); lsr_write_polygon(lsr, node, 1, 1); break;
	case TAG_SVG_radialGradient: GF_LSR_WRITE_INT(lsr, 21, 6, "ch6"); lsr_write_radialGradient(lsr, node); break;
	case TAG_SVG_rect: GF_LSR_WRITE_INT(lsr, 22, 6, "ch6"); lsr_write_rect(lsr, node, 1); break;
	case TAG_SVG_script: GF_LSR_WRITE_INT(lsr, 23, 6, "ch6"); lsr_write_script(lsr, node); break;
	case TAG_SVG_set: GF_LSR_WRITE_INT(lsr, 24, 6, "ch6"); lsr_write_set(lsr, node, parent); break;
	case TAG_SVG_stop: GF_LSR_WRITE_INT(lsr, 25, 6, "ch6"); lsr_write_stop(lsr, node); break;
	case TAG_SVG_svg: GF_LSR_WRITE_INT(lsr, 26, 6, "ch6"); lsr_write_svg(lsr, node); break;
	case TAG_SVG_switch: GF_LSR_WRITE_INT(lsr, 27, 6, "ch6"); lsr_write_switch(lsr, node); break;
	case TAG_SVG_text: GF_LSR_WRITE_INT(lsr, 28, 6, "ch6"); lsr_write_text(lsr, node, 1); break;
	case TAG_SVG_title: GF_LSR_WRITE_INT(lsr, 29, 6, "ch6"); lsr_write_data(lsr, node); break;
	case TAG_SVG_tspan: GF_LSR_WRITE_INT(lsr, 30, 6, "ch6"); lsr_write_tspan(lsr, node); break;
	case TAG_SVG_use: GF_LSR_WRITE_INT(lsr, 31, 6, "ch6"); lsr_write_use(lsr, node, 1); break;
	case TAG_SVG_video: GF_LSR_WRITE_INT(lsr, 32, 6, "ch6"); lsr_write_video(lsr, node); break;
	case TAG_SVG_listener: GF_LSR_WRITE_INT(lsr, 33, 6, "ch6"); lsr_write_listener(lsr, node); break;
	}
}

static void lsr_write_group_content(GF_LASeRCodec *lsr, SVGElement *elt, Bool skip_object_content)
{
	u32 i, count;
	if (!skip_object_content) lsr_write_private_attributes(lsr, elt);

	count = gf_list_count(elt->children);
	if (elt->textContent) count++;
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");
	lsr_write_vluimsbf5(lsr, count, "occ0");

	if (elt->textContent) {
		GF_LSR_WRITE_INT(lsr, 50, 6, "ch4"); 
		lsr_write_byte_align_string(lsr, elt->textContent, "textContent");
		count--;
	}
	for (i=0; i<count; i++) {
		void *n = gf_list_get(elt->children, i);
		lsr_write_scene_content_model(lsr, elt, n);
		if (lsr->trace) fprintf(lsr->trace, "//end %s\n", gf_node_get_class_name(n));
	}
}

static void lsr_write_update_value(GF_LASeRCodec *lsr, SVGElement *elt, u32 fieldType, u32 transformType, void *val, GF_Node *new_node, Bool is_indexed)
{
	SVG_Paint *paint;
	SVG_Number *n;
	if (is_indexed) {
		if (new_node) {
			lsr_write_content_model_36(lsr, elt, new_node);
			return;
		}
	} else {
		if (new_node) {
			lsr_write_content_model_36(lsr, elt, new_node);
			return;
		}
		switch (fieldType) {
		case SVG_Boolean_datatype:
			GF_LSR_WRITE_INT(lsr, *(SVG_Boolean*)val ? 1 : 0, 1, "val"); 
			break;
		case SVG_Paint_datatype:
			paint = (SVG_Paint *)val;
			lsr_write_paint(lsr, val, "val"); 
			break;
		case SVG_Opacity_datatype:
		case SVG_AudioLevel_datatype:
			n = val;
			if (n->type==SVG_NUMBER_INHERIT) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue"); 
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue"); 
				lsr_write_fixed_clamp(lsr, n->value, "val");
			}
			break;
		case SVG_Matrix_datatype:
			switch (transformType) {
			case SVG_TRANSFORM_SCALE:
				lsr_write_fixed_16_8(lsr, ((SVG_Point *)val)->x, "scale_x");
				lsr_write_fixed_16_8(lsr, ((SVG_Point *)val)->y, "scale_y");
				break;
			case SVG_TRANSFORM_TRANSLATE:
				lsr_write_coordinate(lsr, ((SVG_Point *)val)->x, 0, "translation_x");
				lsr_write_coordinate(lsr, ((SVG_Point *)val)->y, 0, "translation_y");
				break;
			case SVG_TRANSFORM_ROTATE:
				lsr_write_fixed_16_8(lsr, ((SVG_Point_Angle*)val)->angle, "rotate");
				break;
			default:
				lsr_write_matrix(lsr, val);
				break;
			}
			break;
		case SVG_Number_datatype:
		case SVG_StrokeMiterLimit_datatype:
		case SVG_FontSize_datatype:
		case SVG_StrokeDashOffset_datatype:
		case SVG_StrokeWidth_datatype:
		case SVG_Length_datatype:
			n = val;
			if (n->type==SVG_NUMBER_INHERIT) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue"); 
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue"); 
				GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag"); 
				lsr_write_fixed_16_8(lsr, n->value, "val");
			}
			break;
		case SVG_LineIncrement_datatype:
			n = val;
			if (n->type==SVG_NUMBER_INHERIT) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
				if (n->type==SVG_NUMBER_AUTO) {
					GF_LSR_WRITE_INT(lsr, 1, 1, "escapeFlag");
					GF_LSR_WRITE_INT(lsr, 0, 2, "auto");
				} else {
					GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
					lsr_write_fixed_16_8(lsr, n->value, "line-increment-value");
				}
			}
			break;
		case SVG_Rotate_datatype:
			n = val;
			if (n->type==SVG_NUMBER_INHERIT) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
				if ((n->type == SVG_NUMBER_AUTO) || (n->type == SVG_NUMBER_AUTO_REVERSE)) {
					GF_LSR_WRITE_INT(lsr, 1, 1, "escapeFlag");
					GF_LSR_WRITE_INT(lsr, (n->type == SVG_NUMBER_AUTO) ? 0 : 1, 2, "rotate");
				} else {
					GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
					lsr_write_fixed_16_8(lsr, n->value, "rotate");
				}
			}
			break;
		case SVG_NumberOrPercentage_datatype:
			lsr_write_value_with_units(lsr, val, "val");
			break;
		case SVG_Coordinate_datatype:
			n = val;
			lsr_write_coordinate(lsr, n->value, 0, "val");
			break;
		case SVG_Coordinates_datatype:
			GF_LSR_WRITE_INT(lsr, 0, 1, "isInherit"); 
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag"); 
			lsr_write_float_list(lsr, *(GF_List **)val, "val");
			break;
		case SVG_IRI_datatype:
			lsr_write_any_uri(lsr, val, "val");
			break;

		case SVG_String_datatype:
		case SVG_ContentType_datatype:
		case SVG_LanguageID_datatype:
			lsr_write_byte_align_string(lsr, * (DOM_String *)val, "val");
			break;
		case SVG_Motion_datatype:
			lsr_write_coordinate(lsr, ((SVG_Point *)val)->x, 0, "pointValueX");
			lsr_write_coordinate(lsr, ((SVG_Point *)val)->y, 0, "pointValueY");
			break;
		case SVG_Points_datatype:
			lsr_write_point_sequence(lsr, *(GF_List **)val, "val");
			break;
		case SVG_PathData_datatype:
			lsr_write_path_type(lsr, val, "val");
			break;
		default:
			if ((fieldType>=SVG_FillRule_datatype) && (fieldType<=SVG_TransformBehavior_datatype)) {
				u8 v = *(u8 *)val;
				/*TODO fixme, check inherit/default values*/
				if (!v) {
					GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue"); 
				} else {
					GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue"); 
					lsr_write_vluimsbf5(lsr, v, "val");
				}
			} else {
				fprintf(stdout, "Warning: update value not implemented for type %d - please fix or report\n", fieldType);
			}
		}
	}
}
/*FIXME - support for scale/translate/rotation*/
static GF_Err lsr_write_add_replace_insert(GF_LASeRCodec *lsr, GF_Command *com)
{
	GF_CommandField *field;
	u8 type = 0;
	u32 field_type, tr_type = 0;
	if (com->tag==GF_SG_LSR_REPLACE) type = LSR_UPDATE_REPLACE;
	else if (com->tag==GF_SG_LSR_ADD) type = LSR_UPDATE_ADD;
	else if (com->tag==GF_SG_LSR_INSERT) type = LSR_UPDATE_INSERT;
	else return GF_BAD_PARAM;

	GF_LSR_WRITE_INT(lsr, type, 4, "ch4");
	field = gf_list_get(com->command_fields, 0);
	field_type = 0;
	if (field && !field->new_node && !field->node_list) {
		u8 attType = 0;
		field_type = field->fieldType;
		/*textContent cannot be used directly*/
		if ((field->fieldIndex==(u32)-1) && (field->fieldType==SVG_String_datatype)) {
			attType = LSR_UPDATE_TYPE_TEXT_CONTENT;
		}
		/*transform*/
		else if ((field->fieldIndex==(u32)-2)) {
			if (field->fieldType == SVG_TRANSFORM_SCALE) attType = LSR_UPDATE_TYPE_SCALE;
			else if (field->fieldType == SVG_TRANSFORM_TRANSLATE) attType = LSR_UPDATE_TYPE_TRANSLATION;
			else if (field->fieldType == SVG_TRANSFORM_ROTATE) attType = LSR_UPDATE_TYPE_ROTATE;
			tr_type = field->fieldType;
			field_type = SVG_Matrix_datatype;
		} else {
			attType = gf_lsr_field_to_attrib_type(com->node, field->fieldIndex);
		}
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_attributeName");
		GF_LSR_WRITE_INT(lsr, attType, 8, "attributeName");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_attributeName");
	}
	/*if not add*/
	if (type!=LSR_UPDATE_ADD) {
		if (!field || field->pos<0) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "has_index");
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, "has_index");
			lsr_write_vluimsbf5(lsr, (u32) field->pos, "index");
		}
	}
	if (type!=LSR_UPDATE_INSERT) {
		if (com->fromNodeID) {
			GF_Node *opNode;
			u8 opAttType;
			opNode = gf_sg_find_node(lsr->sg, com->fromNodeID);
			opAttType = gf_lsr_field_to_attrib_type(opNode, com->fromFieldIndex);
			GF_LSR_WRITE_INT(lsr, 1, 1, "has_operandAttribute");
			GF_LSR_WRITE_INT(lsr, opAttType, 8, "operandAttribute");
			GF_LSR_WRITE_INT(lsr, 1, 1, "has_operandElementId");
			lsr_write_vluimsbf5(lsr, com->fromNodeID-1, "operandElementId");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "has_operandAttribute");
			GF_LSR_WRITE_INT(lsr, 0, 1, "has_operandElementId");
		}
	}
	lsr_write_codec_IDREF_Node(lsr, com->node, "ref");
	if (field && !field->node_list && !com->fromNodeID) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_value");
		lsr_write_update_value(lsr, (SVGElement *)com->node, field_type, tr_type, field->field_ptr, field->new_node, (field->pos>=0) ? 1 : 0);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_value");
	}
	lsr_write_any_attribute(lsr, NULL, NULL, 1);
	/*if not add*/
	if (type!=LSR_UPDATE_ADD) {
		if (field && field->node_list && !com->fromNodeID) {
			u32 i, count = gf_list_count(field->node_list);
			GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");
			lsr_write_vluimsbf5(lsr, count, "count");
			for (i=0; i<count; i++) {
				lsr_write_content_model_36(lsr, (SVGElement *) com->node, gf_list_get(field->node_list, i));
			}
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
		}
	}
	return GF_OK;
}

static GF_Err lsr_write_command_list(GF_LASeRCodec *lsr, GF_List *com_list, SVGscriptElement *script)
{
	GF_CommandField *field;
	u32 i, count = 0;
	GF_BitStream *old_bs;

	if (com_list) count += gf_list_count(com_list);
	if (script && script->textContent) count += 1;

	old_bs = NULL;
	if (script) {
		lsr_write_private_attributes(lsr, (SVGElement *) script);
		if (!count) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
			return GF_OK;
		}
		GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");

		old_bs = lsr->bs;
		lsr->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}
	lsr_write_vluimsbf5(lsr, count, "occ0");

	if (script && script->textContent) {
		GF_LSR_WRITE_INT(lsr, 11, 4, "ch4");
		lsr_write_byte_align_string(lsr, script->textContent, "textContent");
	}
	if (!com_list) goto exit;

	count = gf_list_count(com_list);
	for (i=0; i<count; i++) {
		GF_Command *com = gf_list_get(com_list, i);
		switch (com->tag) {
		case GF_SG_LSR_NEW_SCENE:
		case GF_SG_LSR_REFRESH_SCENE:
			GF_LSR_WRITE_INT(lsr, (com->tag==GF_SG_LSR_REFRESH_SCENE) ? LSR_UPDATE_REFRESH_SCENE : LSR_UPDATE_NEW_SCENE, 4, "ch4");
			if (com->tag==GF_SG_LSR_REFRESH_SCENE) lsr_write_vluimsbf5(lsr, /*refresh scene time*/0, "time");
			lsr_write_any_attribute(lsr, NULL, NULL, 1);
			lsr_write_svg(lsr, (SVGsvgElement *) com->node);
			break;
		case GF_SG_LSR_REPLACE:
		case GF_SG_LSR_ADD:
		case GF_SG_LSR_INSERT:
			lsr_write_add_replace_insert(lsr, com);
			break;
		case GF_SG_LSR_CLEAN:
			break;
		case GF_SG_LSR_DELETE:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_DELETE, 4, "ch4");
			field = gf_list_get(com->command_fields, 0);
			if (field && field->fieldType) {
				u8 attType;
				attType = gf_lsr_field_to_attrib_type(com->node, field->fieldIndex);
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_attributeName");
				GF_LSR_WRITE_INT(lsr, attType, 8, "attributeName");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "has_attributeName");
			}
			if (!field || (field->pos<0) ) {
				GF_LSR_WRITE_INT(lsr, 0, 1, "has_index");
			} else {
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_index");
				lsr_write_vluimsbf5(lsr, (u32) field->pos, "index");
			}
			lsr_write_codec_IDREF_Node(lsr, com->node, "ref");
			lsr_write_any_attribute(lsr, NULL, NULL, 1);
			break;
		case GF_SG_LSR_RESTORE:
			break;
		case GF_SG_LSR_SAVE:
			break;
		case GF_SG_LSR_SEND_EVENT:
			break;
		default:
			return GF_BAD_PARAM;
		}
		/*same-coding scope is command-based (to check in the spec)*/
		lsr->prev_g = NULL;
		lsr->prev_line = NULL;
		lsr->prev_path = NULL;
		lsr->prev_polygon = NULL;
		lsr->prev_rect = NULL;
		lsr->prev_text = NULL;
		lsr->prev_use = NULL;
	}

exit:
	/*script is aligned*/
	if (script)	{
		unsigned char *data;
		u32 data_size;
		gf_bs_get_content(lsr->bs, &data, &data_size);
		gf_bs_del(lsr->bs);
		lsr->bs = old_bs;
		lsr_write_vluimsbf5(lsr, data_size, "length");
		/*script is aligned*/
		gf_bs_align(lsr->bs);
		gf_bs_write_data(lsr->bs, data, data_size);
		free(data);
	}

	return GF_OK;
}

static void lsr_add_color(GF_LASeRCodec *lsr, SVG_Color *color)
{
	lsr->col_table = realloc(lsr->col_table, sizeof(LSRCol)*(lsr->nb_cols+1));
	lsr->col_table[lsr->nb_cols].r = FIX2INT(color->red*lsr->color_scale);
	lsr->col_table[lsr->nb_cols].g = FIX2INT(color->green*lsr->color_scale);
	lsr->col_table[lsr->nb_cols].b = FIX2INT(color->blue*lsr->color_scale);
	lsr->nb_cols++;
}

static void lsr_check_col_index(GF_LASeRCodec *lsr, SVG_Color *color, SVG_Paint *paint)
{
	s32 idx;
	if (color) {
		idx = lsr_get_col_index(lsr, color);
		if (idx==-2) lsr_add_color(lsr, color);
	}
	else if (paint && (paint->type==SVG_PAINT_COLOR) ) {
		idx = lsr_get_col_index(lsr, &paint->color);
		if (idx==-2) lsr_add_color(lsr, &paint->color);
	}
}

static void lsr_check_font_index(GF_LASeRCodec *lsr, SVG_FontFamily *font)
{
	u32 count, i;
	if (font && (font->type == SVG_FONTFAMILY_VALUE) && font->value) {
		Bool found = 0;
		count = gf_list_count(lsr->font_table);
		for (i=0; i<count; i++) {
			char *ff = gf_list_get(lsr->font_table, i);
			if (!strcmp(ff, font->value)) {
				found = 1;
				break;
			}
		}
		if (!found) gf_list_add(lsr->font_table, strdup(font->value));
	}
}

static void lsr_check_font_and_color(GF_LASeRCodec *lsr, SVGElement *elt)
{
	u32 i, count;
	if (elt->properties) {
		lsr_check_col_index(lsr, NULL, &elt->properties->color);
		lsr_check_col_index(lsr, NULL, &elt->properties->fill);
		lsr_check_col_index(lsr, NULL, &elt->properties->stroke);
		lsr_check_col_index(lsr, NULL, &elt->properties->solid_color);
		lsr_check_col_index(lsr, NULL, &elt->properties->stop_color);
		lsr_check_col_index(lsr, NULL, &elt->properties->viewport_fill);

		lsr_check_font_index(lsr, &elt->properties->font_family);
	}
	if (elt->anim) {
		if (elt->anim->attributeName.type == SVG_Paint_datatype) {
			if (elt->anim->from.value) lsr_check_col_index(lsr, NULL, elt->anim->from.value);
			if (elt->anim->by.value) lsr_check_col_index(lsr, NULL, elt->anim->by.value);
			if (elt->anim->to.value) lsr_check_col_index(lsr, NULL, elt->anim->to.value);
			if ( (count = gf_list_count(elt->anim->values.values)) ) {
				for (i=0; i<count; i++) {
					lsr_check_col_index(lsr, NULL, gf_list_get(elt->anim->values.values, i) );
				}
			}
		}
		if (elt->anim->attributeName.type == SVG_FontFamily_datatype) {
			if (elt->anim->from.value) lsr_check_font_index(lsr, elt->anim->from.value);
			if (elt->anim->by.value) lsr_check_font_index(lsr, elt->anim->by.value);
			if (elt->anim->to.value) lsr_check_font_index(lsr, elt->anim->to.value);
			if ( (count = gf_list_count(elt->anim->values.values)) ) {
				for (i=0; i<count; i++) {
					lsr_check_font_index(lsr, gf_list_get(elt->anim->values.values, i) );
				}
			}
		}
	}
	count = gf_list_count(elt->children);
	for (i=0; i<count; i++) {
		SVGElement *c = gf_list_get(elt->children, i);
		lsr_check_font_and_color(lsr, c);
	}
}

static GF_Err lsr_write_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list, Bool reset_encoding_context)
{
	u32 i, count, prev_col_count, prev_font_count;

	/*laser unit header*/
	if (!com_list) {
		if (gf_sg_get_root_node(lsr->sg) == NULL) return GF_BAD_PARAM;
		/*RAP generation, force reset encoding context*/
		GF_LSR_WRITE_INT(lsr, 1, 1, "resetEncodingContext");
	} else {
		GF_LSR_WRITE_INT(lsr, reset_encoding_context ? 1 : 0, 1, "resetEncodingContext");
	}
	GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
	if (0) lsr_write_extension(lsr, NULL, 0, "ext");

	/*codecInitialisations*/

	/*clean all tables*/
	if (reset_encoding_context) {
		lsr->nb_cols = 0;
		if (lsr->col_table) free(lsr->col_table);
		lsr->col_table = NULL;
		while (gf_list_count(lsr->font_table)) {
			char *ft = gf_list_last(lsr->font_table);
			free(ft);
			gf_list_rem_last(lsr->font_table);
		}
	}

	prev_col_count = lsr->nb_cols;
	prev_font_count = gf_list_count(lsr->font_table);
	/*RAP generation, send all fonts and colors*/
	if (!com_list) {
		prev_col_count = prev_font_count = 0;
		lsr_check_font_and_color(lsr, (SVGElement *)gf_sg_get_root_node(lsr->sg));
	} else {
		/*process all colors and fonts*/
		count = gf_list_count(com_list);
		for (i=0; i<count; i++) {
			GF_Command *com = gf_list_get(com_list, i);
			if (gf_list_count(com->command_fields)) {
				GF_CommandField *field = gf_list_get(com->command_fields, 0);
				if (field->fieldType==SVG_Paint_datatype) lsr_check_col_index(lsr, NULL, field->field_ptr);
				else if (field->fieldType==SVG_FontFamily_datatype) lsr_check_font_index(lsr, field->field_ptr);
				else if (field->new_node) lsr_check_font_and_color(lsr, (SVGElement*)field->new_node);
				else if (field->node_list) {
					count = gf_list_count(field->node_list);
					for (i=0; i<count; i++) lsr_check_font_and_color(lsr, gf_list_get(field->node_list, i) );
				}
			} else if (com->node && (com->tag!=GF_SG_LSR_DELETE) ) {
				lsr_check_font_and_color(lsr, (SVGElement *)com->node);
			}
		}
	}
	/*codec initialization*/
	/*1- color*/
	if (prev_col_count == lsr->nb_cols) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "colorInitialisation");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "colorInitialisation");
		lsr_write_vluimsbf5(lsr, lsr->nb_cols - prev_col_count, "count");
		for (i=prev_col_count; i<lsr->nb_cols; i++) {
			GF_LSR_WRITE_INT(lsr, lsr->col_table[i].r, lsr->info->cfg.colorComponentBits, "red");
			GF_LSR_WRITE_INT(lsr, lsr->col_table[i].g, lsr->info->cfg.colorComponentBits, "green");
			GF_LSR_WRITE_INT(lsr, lsr->col_table[i].b, lsr->info->cfg.colorComponentBits, "blue");
		}
	}
	lsr->colorIndexBits = gf_get_bit_size(lsr->nb_cols);
	/*2 - fonts*/
	count = gf_list_count(lsr->font_table);
	if (prev_font_count == count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "fontInitialisation");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "fontInitialisation");
		lsr_write_vluimsbf5(lsr, count - prev_font_count, "count");
		for (i=prev_font_count; i<count; i++) {
			char *ft = gf_list_get(lsr->font_table, i);
			lsr_write_byte_align_string(lsr, ft, "font");
		}
	}
	lsr->fontIndexBits = gf_get_bit_size(count);
	/*3 - private*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "privateDataIdentifierInitialisation");
	/*4 - anyXML*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "anyXMLInitialisation");
	/*5 - extended*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "extendedInitialisation");

	/*RAP generation, encode NewScene with root node*/
	if (!com_list) {
		lsr_write_vluimsbf5(lsr, 1, "occ0");
		GF_LSR_WRITE_INT(lsr, 4, 4, "ch4");
		lsr_write_any_attribute(lsr, NULL, NULL, 1);
		lsr_write_svg(lsr, (SVGsvgElement *) gf_sg_get_root_node(lsr->sg) );
	} else {
		GF_Err e = lsr_write_command_list(lsr, com_list, NULL);
		if (e) return e;
	}
	GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
	if (0) lsr_write_extension(lsr, NULL, 0, "ext");
	return GF_OK;
}

