/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2019
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
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/bitstream.h>

#ifndef GPAC_DISABLE_LASER


#include <gpac/events.h>


#define GF_LSR_WRITE_INT(_codec, _val, _nbBits, _str)	{\
	gf_bs_write_int(_codec->bs, _val, _nbBits);	\
	assert(_str);\
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", _str, _nbBits, _val)); \
	}\
 
static void lsr_write_group_content(GF_LASeRCodec *lsr, SVG_Element *elt, Bool skip_object_content);
static GF_Err lsr_write_command_list(GF_LASeRCodec *lsr, GF_List *comList, SVG_Element *script, Bool first_implicit);
static GF_Err lsr_write_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list, Bool reset_encoding_context);
static void lsr_write_point_sequence(GF_LASeRCodec *lsr, GF_List **pts, const char *name);
static void lsr_write_path_type(GF_LASeRCodec *lsr, SVG_PathData *path, const char *name);

GF_EXPORT
GF_LASeRCodec *gf_laser_encoder_new(GF_SceneGraph *graph)
{
	GF_LASeRCodec *tmp;
	GF_SAFEALLOC(tmp, GF_LASeRCodec);
	if (!tmp) return NULL;
	tmp->streamInfo = gf_list_new();
	tmp->font_table = gf_list_new();
	tmp->sg = graph;
	return tmp;
}

GF_EXPORT
void gf_laser_encoder_del(GF_LASeRCodec *codec)
{
	/*destroy all config*/
	while (gf_list_count(codec->streamInfo)) {
		LASeRStreamInfo *p = (LASeRStreamInfo *)gf_list_last(codec->streamInfo);
		gf_free(p);
		gf_list_rem_last(codec->streamInfo);
	}
	gf_list_del(codec->streamInfo);
	if (codec->col_table) gf_free(codec->col_table);
	while (gf_list_count(codec->font_table)) {
		char *ft = (char *)gf_list_last(codec->font_table);
		gf_free(ft);
		gf_list_rem_last(codec->font_table);
	}
	gf_list_del(codec->font_table);
	gf_free(codec);
}


static LASeRStreamInfo *lsr_get_stream(GF_LASeRCodec *codec, u16 ESID)
{
	LASeRStreamInfo *ptr;
	u32 i = 0;
	while ((ptr = (LASeRStreamInfo *)gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_laser_encoder_new_stream(GF_LASeRCodec *codec, u16 ESID, GF_LASERConfig *cfg)
{
	LASeRStreamInfo *pInfo;
	if (lsr_get_stream(codec, ESID) != NULL) return GF_BAD_PARAM;
	GF_SAFEALLOC(pInfo, LASeRStreamInfo);
	if (!pInfo) return GF_OUT_OF_MEM;
	pInfo->ESID = ESID;
	memcpy(&pInfo->cfg, cfg, sizeof(GF_LASERConfig));
	if (!pInfo->cfg.time_resolution) pInfo->cfg.time_resolution = 1000;
	if (!pInfo->cfg.colorComponentBits) pInfo->cfg.colorComponentBits = 8;
	if (!pInfo->cfg.coord_bits) pInfo->cfg.coord_bits = 12;
	if (pInfo->cfg.resolution<-8) pInfo->cfg.resolution = (s8) -8;
	else if (pInfo->cfg.resolution>7) pInfo->cfg.resolution=7;

	gf_list_add(codec->streamInfo, pInfo);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_laser_encoder_get_config(GF_LASeRCodec *codec, u16 ESID, u8 **out_data, u32 *out_data_length)
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
	gf_bs_write_int(bs, codec->info->cfg.newSceneIndicator ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 3);
	gf_bs_write_int(bs, codec->info->cfg.extensionIDBits, 4);
	/*no extConfig*/
	gf_bs_write_int(bs, 0, 1);
	/*no extensions*/
	gf_bs_write_int(bs, 0, 1);
	gf_bs_align(bs);
	gf_bs_get_content(bs, out_data, out_data_length);
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_laser_encode_au(GF_LASeRCodec *codec, u16 ESID, GF_List *command_list, Bool reset_context, u8 **out_data, u32 *out_data_length)
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
		gf_bs_get_content(codec->bs, out_data, out_data_length);
	}
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}

GF_EXPORT
GF_Err gf_laser_encoder_get_rap(GF_LASeRCodec *codec, u8 **out_data, u32 *out_data_length)
{
	GF_Err e;
	if (!codec->info) codec->info = (LASeRStreamInfo*)gf_list_get(codec->streamInfo, 0);
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits_minus_coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution>=0)
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1<<codec->info->cfg.resolution) );
	else
		codec->res_factor = INT2FIX(1 << (-codec->info->cfg.resolution));

	codec->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	e = lsr_write_laser_unit(codec, NULL, GF_FALSE);
	if (e == GF_OK) gf_bs_get_content(codec->bs, out_data, out_data_length);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}

static void lsr_write_vluimsbf5(GF_LASeRCodec *lsr, u32 val, const char *name)
{
	u32 nb_words;
	u32 nb_bits = val ? gf_get_bit_size(val) : 1;
	nb_words = nb_bits / 4;
	if (nb_bits%4) nb_words++;
	assert(nb_words * 4 >= nb_bits);
	nb_bits = 4*nb_words;
	while (nb_words) {
		nb_words--;
		gf_bs_write_int(lsr->bs, nb_words ? 1 : 0, 1);
	}
	gf_bs_write_int(lsr->bs, val, nb_bits);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", name, nb_bits + (nb_bits/4), val));
}

static void lsr_write_vluimsbf5_ex(GF_LASeRCodec *lsr, u32 val, u32 extra_words, const char *name)
{
	u32 nb_words;
	u32 nb_bits = val ? gf_get_bit_size(val) : 1;
	nb_words = nb_bits / 4;
	if (nb_bits%4) nb_words++;
	nb_words += extra_words;

	assert(nb_words * 4 >= nb_bits);
	nb_bits = 4*nb_words;
	while (nb_words) {
		nb_words--;
		gf_bs_write_int(lsr->bs, nb_words ? 1 : 0, 1);
	}
	gf_bs_write_int(lsr->bs, val, nb_bits);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", name, nb_bits + (nb_bits/4), val));
}

static u32 lsr_get_vluimsbf5_size(u32 val, u32 extra_words)
{
	u32 nb_words;
	u32 nb_bits = val ? gf_get_bit_size(val) : 1;
	nb_words = nb_bits / 4;
	if (nb_bits%4) nb_words++;
	nb_words += extra_words;
	return 4*nb_words + nb_words;
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", name, nb_tot, val));
}

static void lsr_write_extension(GF_LASeRCodec *lsr, char *data, u32 len, const char *name)
{
	if (!len) len = (u32) strlen(name);
	lsr_write_vluimsbf5(lsr, len, name);
	gf_bs_write_data(lsr->bs, data, len);
}

static void lsr_write_codec_IDREF(GF_LASeRCodec *lsr, XMLRI *href, const char *name)
{
	u32 nID = 0;
	if (href && href->target) nID = gf_node_get_id((GF_Node *)href->target);
	else if (name[0]=='#') {
		GF_Node *n = gf_sg_find_node_by_name(lsr->sg, (char *) name + 1);
		if (n) nID = gf_node_get_id((GF_Node *)href->target);
	}
	else nID = 1+href->lsr_stream_id;

	assert(nID);

	lsr_write_vluimsbf5(lsr, nID-1, name);
	GF_LSR_WRITE_INT(lsr, 0, 1, "reserved");
}

static void lsr_write_codec_IDREF_Node(GF_LASeRCodec *lsr, GF_Node *href, const char *name)
{
	u32 nID = gf_node_get_id(href);
	assert(nID);
	lsr_write_vluimsbf5(lsr, nID-1, name);
	GF_LSR_WRITE_INT(lsr, 0, 1, "reserved");
}

static u32 lsr_get_IDREF_nb_bits(GF_LASeRCodec *lsr, GF_Node *href)
{
	u32 nb_bits, nb_words, nID;

	nID = gf_node_get_id(href);
	assert(nID);

	nb_bits = nID ? gf_get_bit_size(nID) : 1;
	nb_words = nb_bits / 4;
	if (nb_bits%4) nb_words++;
	assert(nb_words * 4 >= nb_bits);
	nb_bits = nb_words * 4;
	return nb_words+nb_bits /*IDREF part*/ + 1 /*reserevd bit*/;
}

static void lsr_write_fixed_16_8(GF_LASeRCodec *lsr, Fixed fix, const char *name)
{
	u32 val;
	if (fix<0) {
#ifdef GPAC_FIXED_POINT
		val = (1<<24) + fix / 256;
#else
		val = (1<<24) + FIX2INT(fix * 256);
#endif
	} else {
#ifdef GPAC_FIXED_POINT
		val = fix/256;
#else
		val = FIX2INT(fix*256);
#endif
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
		char *n = (char *)gf_list_get(lsr->font_table, i);
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

static void lsr_write_line_increment_type(GF_LASeRCodec *lsr, SVG_Number *li, const char *name)
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
	u32 len = str ? (u32) strlen(str) : 0;
	gf_bs_align(lsr->bs);
	lsr_write_vluimsbf8(lsr, len, "len");
	if (len) gf_bs_write_data(lsr->bs, str, len);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%s\n", name, 8*len, str ? str : ""));
}
static void lsr_write_byte_align_string_list(GF_LASeRCodec *lsr, GF_List *l, const char *name, Bool is_iri)
{
	char text[4096];
	u32 i, count = gf_list_count(l);
	text[0] = 0;
	for (i=0; i<count; i++) {
		char *str;
		if (is_iri) {
			XMLRI *iri = (XMLRI *)gf_list_get(l, i);
			str = iri->string;
		} else {
			str = (char*)gf_list_get(l, i);
		}
		strncat(text, str, 4096-strlen(text)-1);
		if (i+1<count) strcat(text, ";");
	}
	lsr_write_byte_align_string(lsr, text, name);
}

static void lsr_write_any_uri(GF_LASeRCodec *lsr, XMLRI *iri, const char *name)
{
	Bool is_iri = GF_FALSE;

	if (iri->type==XMLRI_STRING) {
		is_iri = GF_TRUE;
		if (iri->string[0]=='#') {
			iri->target = (SVG_Element*)gf_sg_find_node_by_name(lsr->sg, iri->string+1);
			if (iri->target) {
				is_iri = GF_FALSE;
				iri->type = XMLRI_ELEMENTID;
			}
		}
	}

	GF_LSR_WRITE_INT(lsr, is_iri, 1, "hasUri");
	if (is_iri) {
		if (!iri->string || strnicmp(iri->string, "data:", 5)) {
			lsr_write_byte_align_string(lsr, iri->string, "uri");
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasData");
		} else {
			u32 len;
			char *sep = strchr(iri->string, ',');
			sep[0] = 0;
			lsr_write_byte_align_string(lsr, iri->string, "uri");
			sep[0] = ',';
			len = (u32) strlen(sep+1);
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasData");
			lsr_write_vluimsbf5(lsr, len, "len");
			gf_bs_write_data(lsr->bs, sep+1, len);
		}
	}
	GF_LSR_WRITE_INT(lsr, (iri->type==XMLRI_ELEMENTID) ? 1 : 0, 1, "hasID");
	if (iri->type==XMLRI_ELEMENTID) lsr_write_codec_IDREF(lsr, iri, "idref");

	GF_LSR_WRITE_INT(lsr, (iri->type==XMLRI_STREAMID) ? 1 : 0, 1, "hasStreamID");
	if (iri->type==XMLRI_STREAMID)
		lsr_write_codec_IDREF(lsr, iri, "href");
}

static void lsr_write_paint(GF_LASeRCodec *lsr, SVG_Paint *paint, const char *name)
{
	if ((paint->type==SVG_PAINT_COLOR) && (paint->color.type==SVG_COLOR_RGBCOLOR)) {
		s32 idx;
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasIndex");
		idx = lsr_get_col_index(lsr, &paint->color);
		if (idx<0) {
			idx = 0;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] color not in colorTable\n"));
		}
		GF_LSR_WRITE_INT(lsr, (u32) idx, lsr->colorIndexBits, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasIndex");

		switch (paint->type) {
		case SVG_PAINT_INHERIT:
			GF_LSR_WRITE_INT(lsr, 0, 2, "enum");
			GF_LSR_WRITE_INT(lsr, 0, 2, "choice");
			break;
		case SVG_PAINT_NONE:
			GF_LSR_WRITE_INT(lsr, 0, 2, "enum");
			GF_LSR_WRITE_INT(lsr, 2, 2, "choice");
			break;
		case SVG_PAINT_COLOR:
			if (paint->color.type == SVG_COLOR_CURRENTCOLOR) {
				GF_LSR_WRITE_INT(lsr, 0, 2, "enum");
				GF_LSR_WRITE_INT(lsr, 1, 2, "choice");
			} else {
				GF_LSR_WRITE_INT(lsr, 2, 2, "enum");
				lsr_write_byte_align_string(lsr, (char*)gf_svg_get_system_paint_server_name(paint->color.type), "systemsPaint");
			}
			break;
		case SVG_PAINT_URI:
			GF_LSR_WRITE_INT(lsr, 1, 2, "enum");
			lsr_write_any_uri(lsr, &paint->iri, "uri");
			break;

		default:
			GF_LSR_WRITE_INT(lsr, 3, 2, "enum");
			lsr_write_extension(lsr, "ERROR", 5, "colorExType0");
			break;
		}
	}
}

#ifdef GPAC_UNUSED_FUNC
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

static void lsr_write_private_attr_container(GF_LASeRCodec *lsr, u32 index, const char *name)
{
	assert(0);
}

static Bool lsr_float_list_equal(GF_List *l1, GF_List *l2)
{
	u32 i, count = gf_list_count(l1);
	if (count != gf_list_count(l2)) return 0;
	for (i=0; i<count; i++) {
		Fixed *v1 = (Fixed *)gf_list_get(l1, i);
		Fixed *v2 = (Fixed *)gf_list_get(l2, i);
		if (*v1 != *v2) return 0;
	}
	return 1;
}
#endif /*GPAC_UNUSED_FUNC*/



static void lsr_write_any_attribute(GF_LASeRCodec *lsr, SVG_Element *node, Bool skippable)
{
#if 1
	if (skippable) GF_LSR_WRITE_INT(lsr, 0, 1, "has_attrs");
#else
	if (skippable) GF_LSR_WRITE_INT(lsr, 1, 1, "has_attrs");
	/*
			do () {
				GF_LSR_WRITE_INT(lsr, 0, lsr->info->cfg.extensionIDBits, "reserved");
				lsr_write_vluimsbf5(lsr, 0, "len");//len in BITS
				GF_LSR_WRITE_INT(lsr, 0, 0, "reserved_val");
			} while ()
	*/
#endif
}

static void lsr_write_private_attributes(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	if (1) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_private_attr");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_private_attr");
#ifdef GPAC_UNUSED_FUNC
		lsr_write_private_att_class(lsr);
#endif /*GPAC_UNUSED_FUNC*/
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
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_id");
		lsr_write_vluimsbf5(lsr, id-1, "ID");
#if TODO_LASER_EXTENSIONS
		if (0) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "reserved");
			lsr_write_vluimsbf5(lsr, reserved_len, "len");
			GF_LSR_WRITE_INT(lsr, 0, reserved_len, "reserved");
		} else
#endif
			GF_LSR_WRITE_INT(lsr, 0, 1, "reserved");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_id");
	}
}

static u32 lsr_translate_coords(GF_LASeRCodec *lsr, Fixed x, u32 nb_bits)
{
	s32 res, max;

	res = FIX2INT( gf_divfix(x, lsr->res_factor) );
	/*don't loose too much*/
	if (!res && x) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] resolution factor %g too small to allow coding of %g - adjusting to smallest integer!\n", lsr->res_factor, FIX2FLT(x) ));
		res = (x>0) ? 1 : -1;
	}
	max = (1<<(nb_bits-1)) - 1;
	if (res>=0) {
		if (res > max) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] nb_bits %d not large enough to encode positive number %g!\n", nb_bits, FIX2FLT(x) ));
			res = max;
		}
		assert( ! (res & (1<<(nb_bits-1)) ));
		return (u32) res;
	}
	res += 1<<(nb_bits);
	if (res<=max) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] nb_bits %d not large enough to encode negative number %g!\n", nb_bits, FIX2FLT(x) ));
		res = max+1;
	}
	assert( res & (1<<(nb_bits-1)) );
	return res;
}

static u32 lsr_translate_scale(GF_LASeRCodec *lsr, Fixed v)
{
	s32 res;
	/*always 8 bits for fractional part*/
	if (ABS(v) * 256 < 1) v = 0;
	v = v*256;
	if (v<0) {
		res = FIX2INT(v) + (1<<lsr->coord_bits);
		if (res<0) GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] nb_bits %d not large enough to encode negative number %d!\n", lsr->coord_bits, res));
		return res;
	}
	res = FIX2INT(v);
	if (res & (1<<(lsr->coord_bits-1)) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] nb_bits %d not large enough to encode positive number %d!\n", lsr->coord_bits, res));
	}
	return res;
}
static void lsr_write_matrix(GF_LASeRCodec *lsr, SVG_Transform *mx)
{
	u32 res;
	if (mx->is_ref) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "isNotMatrix");
		GF_LSR_WRITE_INT(lsr, 1, 1, "isRef");
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasXY");
		lsr_write_fixed_16_8(lsr, mx->mat.m[2], "valueX");
		lsr_write_fixed_16_8(lsr, mx->mat.m[5], "valueY");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "isNotMatrix");
		lsr->coord_bits += lsr->scale_bits;

		if ((mx->mat.m[0]!=FIX_ONE) || (mx->mat.m[4]!=FIX_ONE)) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "xx_yy_present");
			res = lsr_translate_scale(lsr, mx->mat.m[0]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "xx");
			res = lsr_translate_scale(lsr, mx->mat.m[4]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "yy");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "xx_yy_present");
		}
		if (mx->mat.m[1] || mx->mat.m[3]) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "xy_yx_present");
			res = lsr_translate_scale(lsr, mx->mat.m[1]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "xy");
			res = lsr_translate_scale(lsr, mx->mat.m[3]);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "yx");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "xy_yx_present");
		}

		if (mx->mat.m[2] || mx->mat.m[5]) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "xz_yz_present");
			res = lsr_translate_coords(lsr, mx->mat.m[2], lsr->coord_bits);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "xz");
			res = lsr_translate_coords(lsr, mx->mat.m[5], lsr->coord_bits);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, "yz");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "xz_yz_present");
		}
		lsr->coord_bits -= lsr->scale_bits;
	}
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

u32 dom_to_lsr_key(u32 dom_k)
{
	switch (dom_k) {
	case GF_KEY_STAR:
		return 0;
	case GF_KEY_0:
		return 1;
	case GF_KEY_1:
		return 2;
	case GF_KEY_2:
		return 3;
	case GF_KEY_3:
		return 4;
	case GF_KEY_4:
		return 5;
	case GF_KEY_5:
		return 6;
	case GF_KEY_6:
		return 7;
	case GF_KEY_7:
		return 8;
	case GF_KEY_8:
		return 9;
	case GF_KEY_9:
		return 10;
	case GF_KEY_DOWN:
		return 12;
	case GF_KEY_LEFT:
		return 14;
	case GF_KEY_RIGHT:
		return 16;
	case GF_KEY_UP:
		return 20;
	/*WHAT IS ANY_KEY (11) ??*/
	case GF_KEY_ENTER:
	case GF_KEY_EXECUTE:
		return 13;
	case GF_KEY_ESCAPE:
		return 15;
	case GF_KEY_NUMBER:
		return 17;
	case GF_KEY_CELL_SOFT1:
		return 18;
	case GF_KEY_CELL_SOFT2:
		return 19;
	default:
		return 100;
	}
}
static void lsr_write_event_type(GF_LASeRCodec *lsr, u32 evtType, u32 evtParam)
{
	u32 force_string = 0;
	switch (evtType) {
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
	case GF_EVENT_REPEAT_KEY:
	case GF_EVENT_SHORT_ACCESSKEY:
		if (dom_to_lsr_key(evtParam)==100) force_string = 2;
		break;
	case GF_EVENT_BEGIN:
	case GF_EVENT_END:
		force_string = 1;
		break;
	case GF_EVENT_REPEAT:
		force_string = 1;
		break;
	}

	if (force_string) {
		char szName[1024];
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		if (evtParam) {
			if (force_string==2) {
				sprintf(szName, "%s(%s)", gf_dom_event_get_name(evtType), gf_dom_get_key_name(evtParam) );
			} else {
				sprintf(szName, "%s(%d)", gf_dom_event_get_name(evtType), evtParam);
			}
		} else {
			sprintf(szName, "%s", gf_dom_event_get_name(evtType));
		}
		lsr_write_byte_align_string(lsr, szName, "evtString");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		switch (evtType) {
		case GF_EVENT_ABORT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_abort, 6, "event");
			break;
		case GF_EVENT_ACTIVATE:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_activate, 6, "event");
			break;
		case GF_EVENT_ACTIVATED:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_activatedEvent, 6, "event");
			break;
		case GF_EVENT_BEGIN:/*SPEC IS BROKEN, CANNOT ENCODE elt.begin !! */
		case GF_EVENT_BEGIN_EVENT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_beginEvent, 6, "event");
			break;
		case GF_EVENT_CLICK:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_click, 6, "event");
			break;
		case GF_EVENT_DEACTIVATED:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_deactivatedEvent, 6, "event");
			break;
		case GF_EVENT_END:/*SPEC IS BROKEN, CANNOT ENCODE elt.end !! */
		case GF_EVENT_END_EVENT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_endEvent, 6, "event");
			break;
		case GF_EVENT_ERROR:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_error, 6, "event");
			break;
		case GF_EVENT_EXECUTION_TIME:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_executionTime, 6, "event");
			break;
		case GF_EVENT_FOCUSIN:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_focusin, 6, "event");
			break;
		case GF_EVENT_FOCUSOUT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_focusout, 6, "event");
			break;
		case GF_EVENT_KEYDOWN:
			/*encode as accessKey() if param*/
			GF_LSR_WRITE_INT(lsr, evtParam ? LSR_EVT_accessKey : LSR_EVT_keydown, 6, "event");
			break;
		case GF_EVENT_KEYUP:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_keyup, 6, "event");
			break;
		case GF_EVENT_LOAD:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_load, 6, "event");
			break;
		case GF_EVENT_LONGKEYPRESS:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_longAccessKey, 6, "event");
			break;
		case GF_EVENT_MOUSEDOWN:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_mousedown, 6, "event");
			break;
		case GF_EVENT_MOUSEMOVE:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_mousemove, 6, "event");
			break;
		case GF_EVENT_MOUSEOUT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_mouseout, 6, "event");
			break;
		case GF_EVENT_MOUSEOVER:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_mouseover, 6, "event");
			break;
		case GF_EVENT_MOUSEUP:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_mouseup, 6, "event");
			break;
		case GF_EVENT_PAUSE:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_pause, 6, "event");
			break;
		case GF_EVENT_PAUSED_EVENT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_pausedEvent, 6, "event");
			break;
		case GF_EVENT_PLAY:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_play, 6, "event");
			break;
		case GF_EVENT_REPEAT_EVENT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_repeatEvent, 6, "event");
			break;
		case GF_EVENT_REPEAT_KEY:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_repeatKey, 6, "event");
			break;
		case GF_EVENT_RESIZE:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_resize, 6, "event");
			break;
		case GF_EVENT_RESUME_EVENT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_resumedEvent, 6, "event");
			break;
		case GF_EVENT_SCROLL:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_scroll, 6, "event");
			break;
		case GF_EVENT_SHORT_ACCESSKEY:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_shortAccessKey, 6, "event");
			break;
		case GF_EVENT_TEXTINPUT:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_textinput, 6, "event");
			break;
		case GF_EVENT_UNLOAD:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_unload, 6, "event");
			break;
		case GF_EVENT_ZOOM:
			GF_LSR_WRITE_INT(lsr, LSR_EVT_zoom, 6, "event");
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] Unsupported LASER event %d\n", evtType) );
			GF_LSR_WRITE_INT(lsr, 0, 6, "event");
			break;
			return;
		}
		switch (evtType) {
		case GF_EVENT_KEYDOWN:
			if (!evtParam) break;
		case GF_EVENT_LONGKEYPRESS:
		case GF_EVENT_REPEAT_KEY:
		case GF_EVENT_SHORT_ACCESSKEY:
			lsr_write_vluimsbf5(lsr, dom_to_lsr_key(evtParam), "keyCode");
			break;
		}
	}
}

static void lsr_write_smil_time(GF_LASeRCodec *lsr, SMIL_Time *t)
{
	s32 now;
	if (t->type==GF_SMIL_TIME_EVENT) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasEvent");
		if (t->element && gf_node_get_id((GF_Node*)t->element) ) {
			XMLRI iri;
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasIdentifier");
			iri.string = NULL;
			iri.type = XMLRI_ELEMENTID;
			iri.target =  t->element;
			lsr_write_codec_IDREF(lsr, &iri, "idref");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasIdentifier");
		}
		lsr_write_event_type(lsr, t->event.type, t->event.parameter);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasEvent");
	}
	if (!t->clock) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasClock");
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, "hasClock");

	now = (s32) (t->clock * lsr->time_resolution);
	if (now<0) {
		now = -now;
		GF_LSR_WRITE_INT(lsr, 1, 1, "sign");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "sign");
	}
	lsr_write_vluimsbf5(lsr, now, "value");
}


static void lsr_write_smil_times(GF_LASeRCodec *lsr, GF_List **l, const char *name, Bool skipable)
{
	SMIL_Time *v;
	u32 r_count, i, count;
	Bool indef = GF_FALSE;

	count = l ? gf_list_count(*l) : 0;

	r_count = 0;
	for (i=0; i<count; i++) {
		v = (SMIL_Time*)gf_list_get(*l, i);
		if (v->type==GF_SMIL_TIME_INDEFINITE) {
			indef = GF_TRUE;
			break;
		}
		else if (v->type!=GF_SMIL_TIME_EVENT_RESOLVED) r_count++;
	}
	if (skipable && !r_count && !indef) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	if (skipable) GF_LSR_WRITE_INT(lsr, 1, 1, name);
	GF_LSR_WRITE_INT(lsr, indef, 1, "choice");
	if (indef) return;
	lsr_write_vluimsbf5(lsr, r_count, "count");
	for (i=0; i<count; i++) {
		v = (SMIL_Time*)gf_list_get(*l, i);
		lsr_write_smil_time(lsr, v);
	}
}

static void lsr_write_duration_ex(GF_LASeRCodec *lsr, SMIL_Duration *v, const char *name, Bool skipable)
{
	if (skipable) {
		if (!v || !v->type) {
			GF_LSR_WRITE_INT(lsr, 0, 1, name);
			return;
		}
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
	}

	if (v->type==SMIL_DURATION_DEFINED) {
		s32 now = (s32) (v->clock_value * lsr->time_resolution);
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (now<0) ? 1 : 0, 1, "sign");
		if (now<0) now = -now;
		lsr_write_vluimsbf5(lsr, now, "value");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, v->type, 2, "time");
	}
}
#define lsr_write_duration(a, b, c) lsr_write_duration_ex(a, b, c, 1)

static void lsr_write_focus(GF_LASeRCodec *lsr, SVG_Focus *foc, const char *name)
{
	if (foc->type==SVG_FOCUS_IRI) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "isEnum");
		lsr_write_codec_IDREF(lsr, &foc->target, "id");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "isEnum");
		GF_LSR_WRITE_INT(lsr, foc->type, 1, "enum");
	}
}

static Bool lsr_elt_has_same_base(GF_LASeRCodec *lsr, SVGAllAttributes *atts, SVG_Element *base, Bool *same_fill, Bool *same_stroke, Bool no_stroke_check)
{
	SVGAllAttributes base_atts;
	GF_FieldInfo info, base_info;

	if (same_stroke) *same_stroke = GF_FALSE;
	if (same_fill) *same_fill = GF_FALSE;

	if (!base) return GF_FALSE;

	gf_svg_flatten_attributes(base, &base_atts);
	if (atts->externalResourcesRequired != base_atts.externalResourcesRequired) return GF_FALSE;

	info.fieldType = base_info.fieldType = SVG_Paint_datatype;
	info.far_ptr = atts->stroke;
	base_info.far_ptr = base_atts.stroke;
	/*check stroke color*/
	if (!gf_svg_attributes_equal(&info, &base_info)) {
		if (!no_stroke_check) return GF_FALSE;
	} else {
		if (same_stroke) *same_stroke = GF_TRUE;
	}
	if (same_fill) {
		info.fieldType = base_info.fieldType = SVG_Paint_datatype;
		info.far_ptr = atts->fill;
		base_info.far_ptr = base_atts.fill;
		/*check stroke color*/
		*same_fill = gf_svg_attributes_equal(&info, &base_info) ? GF_TRUE : GF_FALSE;
	}

	switch (gf_node_get_tag((GF_Node*) base)) {
	/*check path length*/
	case TAG_SVG_path:
		info.fieldType = base_info.fieldType = SVG_Number_datatype;
		info.far_ptr = atts->pathLength;
		base_info.far_ptr = base_atts.pathLength;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;
		break;
	/*check rx and ry for rect*/
	case TAG_SVG_rect:
		info.fieldType = base_info.fieldType = SVG_Length_datatype;
		info.far_ptr = atts->rx;
		base_info.far_ptr = base_atts.rx;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;

		info.fieldType = base_info.fieldType = SVG_Length_datatype;
		info.far_ptr = atts->ry;
		base_info.far_ptr = base_atts.ry;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;
		break;
	/*check x and y for use*/
	case TAG_SVG_use:
		info.fieldType = base_info.fieldType = SVG_Coordinate_datatype;
		info.far_ptr = atts->x;
		base_info.far_ptr = base_atts.x;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;

		info.fieldType = base_info.fieldType = SVG_Coordinate_datatype;
		info.far_ptr = atts->y;
		base_info.far_ptr = base_atts.y;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;
		break;
	/*check editable and rotate for text*/
	case TAG_SVG_text:
		info.fieldType = base_info.fieldType = SVG_Boolean_datatype;
		info.far_ptr = atts->editable;
		base_info.far_ptr = base_atts.editable;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;

		info.fieldType = base_info.fieldType = SVG_Numbers_datatype;
		info.far_ptr = atts->text_rotate;
		base_info.far_ptr = base_atts.text_rotate;
		if (!gf_svg_attributes_equal(&info, &base_info)) return GF_FALSE;
		break;
	}

	return gf_lsr_same_rare(atts, &base_atts);
}

static void lsr_write_rare(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 i, nb_rare;
	s32 field_rare;
	SVGAttribute *att;

	nb_rare = 0;
	att = ((SVG_Element*)n)->attributes;
	while (att) {
		field_rare = gf_lsr_rare_type_from_attribute(att->tag);
		if (field_rare>=0) nb_rare++;
		att = att->next;
	}

	GF_LSR_WRITE_INT(lsr, nb_rare ? 1 : 0, 1, "has_rare");
	if (!nb_rare) return;

	GF_LSR_WRITE_INT(lsr, nb_rare, 6, "nbOfAttributes");

	att = ((SVG_Element*)n)->attributes;
	while (att) {
		field_rare = gf_lsr_rare_type_from_attribute(att->tag);
		if (field_rare==-1) {
			att = att->next;
			continue;
		}
		/*RARE extension*/
		if (field_rare==49) {
			Bool is_string = GF_FALSE;
			u32 size, cur_bits;
			u32 len = 2+3;
			switch (att->tag) {
			case TAG_SVG_ATT_syncMaster:
				len +=1;
				break;
			case TAG_SVG_ATT_requiredFonts:
			{
				GF_List *l = *(GF_List **)att->data;
				u32 i, str_len = 0;
				for (i=0; i<gf_list_count(l); i++) {
					char *st = gf_list_get(l, i);
					str_len += (u32) strlen(st);
					if (i) str_len += 1;
				}

				len += 8 * (u32) str_len;

				/*get vluimsbf5 field size with one extra word (4 bits, enough to code string alignment)*/
				size = lsr_get_vluimsbf5_size(len, 1);
				cur_bits = gf_bs_get_bit_position(lsr->bs) + lsr->info->cfg.extensionIDBits + size + 5;
				/*count string alignment*/
				while (cur_bits%8) {
					len++;
					cur_bits++;
				}
				is_string = GF_TRUE;
			}
				break;
			default:
				len +=2;
				break;
			}
			GF_LSR_WRITE_INT(lsr, 49, 6, "attributeRARE");
			GF_LSR_WRITE_INT(lsr, 2, lsr->info->cfg.extensionIDBits, "extensionID");
			if (is_string) {
				lsr_write_vluimsbf5_ex(lsr, len, 1, "len");
			} else {
				lsr_write_vluimsbf5(lsr, len, "len");
			}
			GF_LSR_WRITE_INT(lsr, 1, 2, "nbOfAttributes");

			switch (att->tag) {
			case TAG_SVG_ATT_syncMaster:
				GF_LSR_WRITE_INT(lsr, 0, 3, "attributeRARE");
				GF_LSR_WRITE_INT(lsr, *(SVG_Boolean *)att->data ? 1 : 0, 1, "syncMaster");
				break;
			case TAG_SVG_ATT_focusHighlight:
				GF_LSR_WRITE_INT(lsr, 1, 3, "attributeRARE");
				GF_LSR_WRITE_INT(lsr, *(SVG_FocusHighlight*)att->data, 2, "focusHighlight");
				break;
			case TAG_SVG_ATT_initialVisibility:
				GF_LSR_WRITE_INT(lsr, 2, 3, "attributeRARE");
				GF_LSR_WRITE_INT(lsr, *(SVG_InitialVisibility*)att->data, 2, "initialVisibility");
				break;
			case TAG_SVG_ATT_fullscreen:
				GF_LSR_WRITE_INT(lsr, 3, 3, "attributeRARE");
				GF_LSR_WRITE_INT(lsr, *(SVG_Boolean *)att->data ? 1 : 0, 1, "fullscreen");
				break;
			case TAG_SVG_ATT_requiredFonts:
				GF_LSR_WRITE_INT(lsr, 4, 3, "attributeRARE");
				lsr_write_byte_align_string_list(lsr, *(GF_List **)att->data, "requiredFonts", GF_FALSE);
				break;
			}
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasNextExtension");
			att = att->next;
			continue;
		}

		GF_LSR_WRITE_INT(lsr, (u32)field_rare, 6, "attributeRARE");
		switch (att->tag) {
		case TAG_SVG_ATT__class:
			lsr_write_byte_align_string(lsr, *(SVG_String *)att->data, "class");
			break;

		case TAG_SVG_ATT_audio_level:
			lsr_write_fixed_clamp(lsr, ((SVG_Number *) att->data)->value, "audio-level");
			break;
		case TAG_SVG_ATT_color:
			lsr_write_paint(lsr, (SVG_Paint*) att->data, "color");
			break;
		case TAG_SVG_ATT_color_rendering:
			GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)att->data, 2, "color-rendering");
			break;
		case TAG_SVG_ATT_display:
			GF_LSR_WRITE_INT(lsr, *(SVG_Display*)att->data, 5, "display");
			break;
		case TAG_SVG_ATT_display_align:
			GF_LSR_WRITE_INT(lsr, *(SVG_DisplayAlign*)att->data, 3, "display-align");
			break;
		case TAG_SVG_ATT_fill_opacity:
			lsr_write_fixed_clamp(lsr, ((SVG_Number *)att->data)->value, "fill-opacity");
			break;
		case TAG_SVG_ATT_fill_rule:
			GF_LSR_WRITE_INT(lsr, *(SVG_FillRule*)att->data, 2, "fill-rule");
			break;
		case TAG_SVG_ATT_image_rendering:
			GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)att->data, 2, "image-rendering");
			break;
		case TAG_SVG_ATT_line_increment:
			lsr_write_line_increment_type(lsr, (SVG_Number*)att->data, "lineIncrement");
			break;
		case TAG_SVG_ATT_pointer_events:
			GF_LSR_WRITE_INT(lsr, *(SVG_PointerEvents*)att->data, 4, "pointer-events");
			break;
		case TAG_SVG_ATT_shape_rendering:
			GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)att->data, 3, "shape-rendering");
			break;
		case TAG_SVG_ATT_solid_color:
			lsr_write_paint(lsr, (SVG_Paint*)att->data, "solid-color");
			break;
		case TAG_SVG_ATT_solid_opacity:
			lsr_write_fixed_clamp(lsr, ((SVG_Number *)att->data)->value, "solid-opacity");
			break;
		case TAG_SVG_ATT_stop_color:
			lsr_write_paint(lsr, (SVG_Paint*)att->data, "stop-color");
			break;
		case TAG_SVG_ATT_stop_opacity:
			lsr_write_fixed_clamp(lsr, ((SVG_Number *)att->data)->value, "stop-opacity");
			break;
		case TAG_SVG_ATT_stroke_dasharray:
		{
			u32 j;
			SVG_StrokeDashArray *da = (SVG_StrokeDashArray*)att->data;
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
		case TAG_SVG_ATT_stroke_dashoffset:
			lsr_write_fixed_16_8i(lsr, (SVG_Number*)att->data, "dashOffset");
			break;

		case TAG_SVG_ATT_stroke_linecap:
			GF_LSR_WRITE_INT(lsr, *(SVG_StrokeLineCap*)att->data, 2, "stroke-linecap");
			break;
		case TAG_SVG_ATT_stroke_linejoin:
			GF_LSR_WRITE_INT(lsr, *(SVG_StrokeLineJoin*)att->data, 2, "stroke-linejoin");
			break;
		case TAG_SVG_ATT_stroke_miterlimit:
			lsr_write_fixed_16_8i(lsr, (SVG_Number*)att->data, "miterLimit");
			break;
		case TAG_SVG_ATT_stroke_opacity:
			lsr_write_fixed_clamp(lsr, ((SVG_Number *)att->data)->value, "stroke-opacity");
			break;
		case TAG_SVG_ATT_stroke_width:
			lsr_write_fixed_16_8i(lsr, (SVG_Number*)att->data, "strokeWidth");
			break;
		case TAG_SVG_ATT_text_anchor:
			GF_LSR_WRITE_INT(lsr, *(SVG_TextAnchor*)att->data, 2, "text-achor");
			break;
		case TAG_SVG_ATT_text_rendering:
			GF_LSR_WRITE_INT(lsr, *(SVG_RenderingHint*)att->data, 3, "text-rendering");
			break;
		case TAG_SVG_ATT_viewport_fill:
			lsr_write_paint(lsr, (SVG_Paint*)att->data, "viewport-fill");
			break;
		case TAG_SVG_ATT_viewport_fill_opacity:
			lsr_write_fixed_clamp(lsr, ((SVG_Number *)att->data)->value, "viewport-fill-opacity");
			break;
		case TAG_SVG_ATT_vector_effect:
			GF_LSR_WRITE_INT(lsr, *(SVG_VectorEffect*)att->data, 4, "vector-effect");
			break;
		case TAG_SVG_ATT_visibility:
			GF_LSR_WRITE_INT(lsr, *(SVG_PointerEvents*)att->data, 2, "visibility");
			break;
		case TAG_SVG_ATT_requiredExtensions:
			lsr_write_byte_align_string_list(lsr, *(GF_List **)att->data, "requiredExtensions", GF_TRUE);
			break;
		case TAG_SVG_ATT_requiredFormats:
			lsr_write_byte_align_string_list(lsr, *(GF_List **)att->data, "requiredFormats", GF_FALSE);
			break;
		case TAG_SVG_ATT_requiredFeatures:
		{
			GF_List *l = *(GF_List **)att->data;
			u32 j, tot_count, count = gf_list_count(l);
			u8 *vals = (u8*)gf_malloc(sizeof(u8)*count);
			tot_count = 0;
			for (i=0; i<count; i++) {
				char *ext;
				XMLRI *iri = (XMLRI*)gf_list_get(l, i);
				if (iri->type != XMLRI_STRING) continue;
				ext = strchr(iri->string, '#');
				if (!ext) continue;
				ext++;
				if (!stricmp(ext, "Animation")) {
					vals[tot_count] = 0;
					tot_count++;
				}
				else if (!stricmp(ext, "Audio")) {
					vals[tot_count] = 1;
					tot_count++;
				}
				else if (!stricmp(ext, "ComposedVideo")) {
					vals[tot_count] = 2;
					tot_count++;
				}
				else if (!stricmp(ext, "ConditionalProcessing")) {
					vals[tot_count] = 3;
					tot_count++;
				}
				else if (!stricmp(ext, "ConditionalProcessingAttribute")) {
					vals[tot_count] = 4;
					tot_count++;
				}
				else if (!stricmp(ext, "CoreAttribute")) {
					vals[tot_count] = 5;
					tot_count++;
				}
				else if (!stricmp(ext, "Extensibility")) {
					vals[tot_count] = 6;
					tot_count++;
				}
				else if (!stricmp(ext, "ExternalResourcesRequired")) {
					vals[tot_count] = 7;
					tot_count++;
				}
				else if (!stricmp(ext, "Font")) {
					vals[tot_count] = 8;
					tot_count++;
				}
				else if (!stricmp(ext, "Gradient")) {
					vals[tot_count] = 9;
					tot_count++;
				}
				else if (!stricmp(ext, "GraphicsAttribute")) {
					vals[tot_count] = 10;
					tot_count++;
				}
				else if (!stricmp(ext, "Handler")) {
					vals[tot_count] = 11;
					tot_count++;
				}
				else if (!stricmp(ext, "Hyperlinking")) {
					vals[tot_count] = 12;
					tot_count++;
				}
				else if (!stricmp(ext, "Image")) {
					vals[tot_count] = 13;
					tot_count++;
				}
				else if (!stricmp(ext, "OpacityAttribute")) {
					vals[tot_count] = 14;
					tot_count++;
				}
				else if (!stricmp(ext, "PaintAttribute")) {
					vals[tot_count] = 15;
					tot_count++;
				}
				else if (!stricmp(ext, "Prefetch")) {
					vals[tot_count] = 16;
					tot_count++;
				}
				else if (!stricmp(ext, "SVG")) {
					vals[tot_count] = 17;
					tot_count++;
				}
				else if (!stricmp(ext, "SVG-animation")) {
					vals[tot_count] = 18;
					tot_count++;
				}
				else if (!stricmp(ext, "SVG-dynamic")) {
					vals[tot_count] = 19;
					tot_count++;
				}
				else if (!stricmp(ext, "SVG-static")) {
					vals[tot_count] = 20;
					tot_count++;
				}
				else if (!stricmp(ext, "SVGDOM")) {
					vals[tot_count] = 21;
					tot_count++;
				}
				else if (!stricmp(ext, "SVGDOM-animation")) {
					vals[tot_count] = 22;
					tot_count++;
				}
				else if (!stricmp(ext, "SVGDOM-dynamic")) {
					vals[tot_count] = 23;
					tot_count++;
				}
				else if (!stricmp(ext, "SVGDOM-static")) {
					vals[tot_count] = 24;
					tot_count++;
				}
				else if (!stricmp(ext, "Script")) {
					vals[tot_count] = 25;
					tot_count++;
				}
				else if (!stricmp(ext, "Shape")) {
					vals[tot_count] = 26;
					tot_count++;
				}
				else if (!stricmp(ext, "SolidColor")) {
					vals[tot_count] = 27;
					tot_count++;
				}
				else if (!stricmp(ext, "Structure")) {
					vals[tot_count] = 28;
					tot_count++;
				}
				else if (!stricmp(ext, "Text")) {
					vals[tot_count] = 29;
					tot_count++;
				}
				else if (!stricmp(ext, "TimedAnimation")) {
					vals[tot_count] = 30;
					tot_count++;
				}
				else if (!stricmp(ext, "TransformedVideo")) {
					vals[tot_count] = 31;
					tot_count++;
				}
				else if (!stricmp(ext, "Video")) {
					vals[tot_count] = 32;
					tot_count++;
				}
				else if (!stricmp(ext, "XlinkAttribute")) {
					vals[tot_count] = 33;
					tot_count++;
				}
			}
			lsr_write_vluimsbf5(lsr, tot_count, "len");
			for (j=0; j<tot_count; j++) {
				GF_LSR_WRITE_INT(lsr, vals[j], 6, "feature");
			}
			gf_free(vals);
		}
		break;

		case TAG_SVG_ATT_systemLanguage:
			lsr_write_byte_align_string_list(lsr, *(GF_List **)att->data, "systemLanguage", GF_FALSE);
			break;
		case TAG_XML_ATT_base:
			lsr_write_byte_align_string(lsr, ((XMLRI*)att->data)->string, "xml:base");
			break;
		case TAG_XML_ATT_lang:
			lsr_write_byte_align_string(lsr, *(SVG_String *)att->data, "xml:lang");
			break;
		case TAG_XML_ATT_space:
			GF_LSR_WRITE_INT(lsr, *(XML_Space *)att->data, 1, "xml:space");
			break;
		case TAG_SVG_ATT_nav_next:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusNext");
			break;
		case TAG_SVG_ATT_nav_up:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusNorth");
			break;
		case TAG_SVG_ATT_nav_up_left:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusNorthEast");
			break;
		case TAG_SVG_ATT_nav_up_right:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusNorthWest");
			break;
		case TAG_SVG_ATT_nav_prev:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusPrev");
			break;
		case TAG_SVG_ATT_nav_down:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusSouth");
			break;
		case TAG_SVG_ATT_nav_down_left:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusSouthEast");
			break;
		case TAG_SVG_ATT_nav_down_right:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusSouthWest");
			break;
		case TAG_SVG_ATT_nav_right:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusWest");
			break;
		case TAG_SVG_ATT_nav_left:
			lsr_write_focus(lsr, (SVG_Focus*)att->data, "focusEast");
			break;

		case TAG_SVG_ATT_font_variant:
			GF_LSR_WRITE_INT(lsr, *(SVG_FontVariant *)att->data, 2, "font-variant");
			break;
		case TAG_SVG_ATT_font_family:
		{
			s32 idx = lsr_get_font_index(lsr, (SVG_FontFamily*)att->data);
			if (idx<0) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isInherit");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isInherit");
				GF_LSR_WRITE_INT(lsr, idx, lsr->fontIndexBits, "fontIndex");
			}
		}
		break;
		case TAG_SVG_ATT_font_size:
			lsr_write_fixed_16_8i(lsr, (SVG_Number*)att->data, "fontSize");
			break;
		case TAG_SVG_ATT_font_style:
			GF_LSR_WRITE_INT(lsr, *((SVG_FontStyle *)att->data), 3, "fontStyle");
			break;
		case TAG_SVG_ATT_font_weight:
			GF_LSR_WRITE_INT(lsr, *((SVG_FontWeight *)att->data), 4, "fontWeight");
			break;

		case TAG_XLINK_ATT_title:
			lsr_write_byte_align_string(lsr, *(SVG_String *)att->data, "xlink:title");
			break;
		/*TODO FIXME*/
		case TAG_XLINK_ATT_type:
			GF_LSR_WRITE_INT(lsr, 0, 3, "xlink:type");
			break;
		case TAG_XLINK_ATT_role:
			lsr_write_any_uri(lsr, (XMLRI*)att->data, "xlink:role");
			break;
		case TAG_XLINK_ATT_arcrole:
			lsr_write_any_uri(lsr, (XMLRI*)att->data, "xlink:arcrole");
			break;
		/*TODO FIXME*/
		case TAG_XLINK_ATT_actuate:
			GF_LSR_WRITE_INT(lsr, 0, 2, "xlink:actuate");
			break;
		case TAG_XLINK_ATT_show:
			GF_LSR_WRITE_INT(lsr, 0, 3, "xlink:show");
			break;
		case TAG_SVG_ATT_end:
			lsr_write_smil_times(lsr, (GF_List **)att->data, "end", GF_FALSE);
			break;
		case TAG_SVG_ATT_min:
			lsr_write_duration_ex(lsr, (SMIL_Duration*)att->data, "min", GF_FALSE);
			break;
		case TAG_SVG_ATT_max:
			lsr_write_duration_ex(lsr, (SMIL_Duration*)att->data, "max", GF_FALSE);
			break;
		case TAG_SVG_ATT_transform:
			lsr_write_matrix(lsr, (SVG_Transform*)att->data);
			break;
		case TAG_SVG_ATT_focusable:
			GF_LSR_WRITE_INT(lsr, *(SVG_Focusable*)att->data, 2, "focusable");
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR Enc] Rare Field declared but not encoded\n"));
			break;
		}
		att = att->next;
	}
}

static void lsr_write_fill(GF_LASeRCodec *lsr, SVG_Element *n, SVGAllAttributes *atts)
{
	if (atts->fill) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "fill");
		lsr_write_paint(lsr, atts->fill, "fill");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "fill");
	}
}

static void lsr_write_stroke(GF_LASeRCodec *lsr, SVG_Element *n, SVGAllAttributes *atts)
{
	if (atts->stroke) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_stroke");
		lsr_write_paint(lsr, atts->stroke, "stroke");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_stroke");
	}
}
static void lsr_write_href(GF_LASeRCodec *lsr, XMLRI *iri)
{
	Bool has_href = iri ? GF_TRUE : GF_FALSE;
	if (iri) {
		if (iri->type==XMLRI_ELEMENTID) {
			if (!iri->target && iri->string) iri->target = (SVG_Element *)gf_sg_find_node_by_name(lsr->sg, iri->string+1);
			if (!iri->target || !gf_node_get_id((GF_Node *)iri->target)) has_href = GF_FALSE;
		}
		else if (iri->type==XMLRI_STREAMID) {
			if (!iri->lsr_stream_id) has_href = GF_FALSE;
		}
		else if (!iri->string) has_href = GF_FALSE;
	}

	GF_LSR_WRITE_INT(lsr, has_href, 1, "has_href");
	if (has_href) lsr_write_any_uri(lsr, iri, "href");
}

static void lsr_write_accumulate(GF_LASeRCodec *lsr, SMIL_Accumulate *accum_type)
{
	GF_LSR_WRITE_INT(lsr, accum_type ? 1 : 0, 1, "has_accumulate");
	if (accum_type) GF_LSR_WRITE_INT(lsr, *accum_type, 1, "accumulate");
}
static void lsr_write_additive(GF_LASeRCodec *lsr, SMIL_Additive *add_type)
{
	GF_LSR_WRITE_INT(lsr, add_type ? 1 : 0, 1, "has_additive");
	if (add_type) GF_LSR_WRITE_INT(lsr, *add_type, 1, "additive");
}
static void lsr_write_calc_mode(GF_LASeRCodec *lsr, u8 *calc_mode)
{

	/*SMIL_CALCMODE_LINEAR is default and 0 in our code*/
	GF_LSR_WRITE_INT(lsr, (!calc_mode || (*calc_mode==SMIL_CALCMODE_LINEAR)) ? 0 : 1, 1, "has_calcMode");
	if (calc_mode && (*calc_mode!=SMIL_CALCMODE_LINEAR)) {
		GF_LSR_WRITE_INT(lsr, *calc_mode, 2, "calcMode");
	}
}

static void lsr_write_animatable(GF_LASeRCodec *lsr, SMIL_AttributeName *anim_type, XMLRI *iri, const char *name)
{
	s32 a_type = -1;

	if (!anim_type || !iri || !iri->target) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasAttributeName");
		return;
	}

	/*locate field - checkme, this may not work since anim is not setup...*/
	assert(anim_type->name || anim_type->tag);
	if (!anim_type->tag) anim_type->tag = gf_xml_get_attribute_tag((GF_Node*)iri->target, anim_type->name, 0);
	if (!anim_type->type) anim_type->type = gf_xml_get_attribute_type(anim_type->tag);
	a_type = gf_lsr_anim_type_from_attribute(anim_type->tag);
	if (a_type<0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] Unsupported attributeName %s for animatable type, skipping\n", anim_type->name));
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasAttributeName");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasAttributeName");
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (u8) a_type, 8, "attributeType");
	}
}

static void lsr_write_anim_fill(GF_LASeRCodec *lsr, u8 *animFreeze)
{
	GF_LSR_WRITE_INT(lsr, animFreeze ? 1 : 0, 1, "has_smil_fill");
	if (animFreeze) GF_LSR_WRITE_INT(lsr, *animFreeze, 1, "smil_fill");
}
static void lsr_write_anim_repeat(GF_LASeRCodec *lsr, SMIL_RepeatCount *repeat)
{
	GF_LSR_WRITE_INT(lsr, repeat ? 1 : 0, 1, "has_repeatCount");
	if (!repeat) return;

	if (repeat->type==SMIL_REPEATCOUNT_DEFINED) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "repeatCount");
		lsr_write_fixed_16_8(lsr, repeat->count, "repeatCount");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "repeatCount");
		/*enumeration indefinite{0}*/
	}
}
static void lsr_write_repeat_duration(GF_LASeRCodec *lsr, SMIL_Duration *smil)
{
	GF_LSR_WRITE_INT(lsr, smil ? 1 : 0, 1, "has_repeatDur");
	if (!smil) return;

	if (smil->type==SMIL_DURATION_DEFINED) {
		u32 now = (u32) (smil->clock_value * lsr->time_resolution);
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_vluimsbf5(lsr, now, "value");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		/*enumeration indefinite{0}*/
	}
}
static void lsr_write_anim_restart(GF_LASeRCodec *lsr, u8 *animRestart)
{
	GF_LSR_WRITE_INT(lsr, animRestart ? 1 : 0, 1, "has_restart");

	/*enumeration always{0} never{1} whenNotActive{2}*/
	if (animRestart) GF_LSR_WRITE_INT(lsr, *animRestart, 2, "restart");
}

static u32 svg_type_to_lsr_anim(u32 svg_type, u32 transform_type, GF_List *vals, void *a_val)
{
	switch (svg_type) {
	/*all string types*/
	case DOM_String_datatype:
		return 0;
	/*all length types*/
	case SVG_Number_datatype:
	case SVG_FontSize_datatype:
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
	/*
		case SVG_Opacity_datatype:
			return 4;
	*/
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
	case SVG_TextAlign_datatype:
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
	case XMLRI_datatype:
		return 12;
	case SVG_Motion_datatype:
		return 9;

	/*ARG LOOKS LIKE THE SPEC IS BROKEN HERE*/
	case SVG_Transform_Translate_datatype:
		return 9;
	case SVG_Transform_Scale_datatype:
		return 8;
	case SVG_Transform_Rotate_datatype:
		if (vals) {
			u32 i=0;
			SVG_Point_Angle *pt;
			while ((pt = (SVG_Point_Angle *) gf_list_enum(vals, &i))) {
				if (pt->x || pt->y) return 8;
			}
		} else if (a_val) {
			SVG_Point_Angle *pt = (SVG_Point_Angle *) a_val;
			if (pt->x || pt->y) return 8;
		}
		return 1;
	case SVG_Transform_SkewX_datatype:
		return 1;
	case SVG_Transform_SkewY_datatype:
		return 1;
	//case SVG_Transform_datatype: return;
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

static void lsr_write_coordinate_ptr(GF_LASeRCodec *lsr, SVG_Coordinate *val, Bool skipable, const char *name)
{
	if (skipable && !val) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		u32 res = lsr_translate_coords(lsr, val ? val->value : 0, lsr->coord_bits);
		if (skipable) GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, name);
	}
}

static void lsr_write_an_anim_value(GF_LASeRCodec *lsr, void *val, u32 lsr_type, u32 svg_type, u32 transform_type, const char *name)
{
	if ((lsr_type==1) || (lsr_type==4)) {
		switch (svg_type) {
		case SVG_Transform_Rotate_datatype:
		case SVG_Transform_SkewX_datatype:
		case SVG_Transform_SkewY_datatype:
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
			break;
		default:
		{
			SVG_Number  *n = (SVG_Number *) val;
			if (n->type != SVG_NUMBER_VALUE) {
				u8 val = 0;
				if (n->type==SVG_NUMBER_INHERIT) val=1;
				/*fixe me spec is not clear here regarding what values should be used ...*/
				
				GF_LSR_WRITE_INT(lsr, 1, 1, "escapeFlag");
				GF_LSR_WRITE_INT(lsr, val, 2, "escapeEnum");
				return;
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
			}
		}
		break;
		}
	} else if (svg_type==SVG_StrokeDashArray_datatype) {
		SVG_StrokeDashArray *da = (SVG_StrokeDashArray *)val;
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
	case 0:
		lsr_write_byte_align_string(lsr, *(DOM_String *)val, name);
		break;
	case 1:
		if (svg_type==SVG_Transform_Rotate_datatype) {
			Fixed angle = ((SVG_Point_Angle *) val)->angle;
			angle = gf_muldiv(angle, INT2FIX(180), GF_PI);
			lsr_write_fixed_16_8(lsr, angle, name);
		} else if ((svg_type==SVG_Transform_SkewX_datatype) || (svg_type==SVG_Transform_SkewY_datatype))  {
			lsr_write_fixed_16_8(lsr, *(Fixed *)val, name);
		} else {
			lsr_write_fixed_16_8(lsr, ((SVG_Number *) val)->value, name);
		}
		break;
	case 12:
		lsr_write_any_uri(lsr, (XMLRI*)val, name);
		break;
	case 2:
		lsr_write_path_type(lsr, (SVG_PathData*)val, name);
		break;
	case 3:
		lsr_write_point_sequence(lsr, (GF_List **)val, name);
		break;
	case 4:
		lsr_write_fixed_clamp(lsr, ((SVG_Number *) val)->value, name);
		break;
	case 5:
		lsr_write_paint(lsr, (SVG_Paint*)val, name);
		break;
	case 6:
		lsr_write_vluimsbf5(lsr, (u32) *(u8 *) val, name);
		break;
	case 10:
		lsr_write_vluimsbf5(lsr, *(u32 *) val, name);
		break;
	case 11:
	{
		s32 idx = lsr_get_font_index(lsr, (SVG_FontFamily*)val);
		if (idx<0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] corrupted font table while encoding anim value\n"));
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
			u8 *v = (u8 *)gf_list_get(l, i);
			lsr_write_vluimsbf5(lsr, *v, "val");
		}
	}
	break;
	case 8: // floats
	{
		u32 i, count;
		if (svg_type==SVG_StrokeDashArray_datatype) {
			SVG_StrokeDashArray *da = (SVG_StrokeDashArray *)val;
			lsr_write_vluimsbf5(lsr, da->array.count, "count");
			for (i=0; i<da->array.count; i++) {
				lsr_write_fixed_16_8(lsr, da->array.vals[i], "val");
			}
		} else if (svg_type==SVG_ViewBox_datatype) {
			SVG_ViewBox *vb = (SVG_ViewBox *)val;
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
				SVG_Coordinate *v = (SVG_Coordinate *)gf_list_get(l, i);
				lsr_write_fixed_16_8(lsr, v->value, "val");
			}
		} else if (svg_type==SVG_Transform_Rotate_datatype) {
			SVG_Point_Angle *p = (SVG_Point_Angle *)val;
			Fixed angle = gf_muldiv(p->angle, INT2FIX(180), GF_PI);
			count = (p->x || p->y) ? 3 : 1;
			lsr_write_vluimsbf5(lsr, count, "count");
			lsr_write_fixed_16_8(lsr, angle, "val");
			if (count==3) {
				lsr_write_fixed_16_8(lsr, p->x, "val");
				lsr_write_fixed_16_8(lsr, p->y, "val");
			}
		} else if (svg_type==SVG_Transform_Scale_datatype) {
			SVG_Point *pt = (SVG_Point *)val;
			count = (pt->x == pt->y) ? 1 : 2;
			lsr_write_vluimsbf5(lsr, count, "count");
			lsr_write_fixed_16_8(lsr, pt->x, "val");
			if (count==2) lsr_write_fixed_16_8(lsr, pt->y, "val");
		} else {
			GF_List *l = *(GF_List **)val;
			count = gf_list_count(l);
			lsr_write_vluimsbf5(lsr, count, "count");
			for (i=0; i<count; i++) {
				Fixed *v = (Fixed *)gf_list_get(l, i);
				lsr_write_fixed_16_8(lsr, *v, "val");
			}
		}
	}
	break;
	case 9: // point
		if (svg_type==SVG_Motion_datatype) {
			lsr_write_coordinate(lsr, ((GF_Matrix2D*)val)->m[2], GF_FALSE, "valX");
			lsr_write_coordinate(lsr, ((GF_Matrix2D*)val)->m[5], GF_FALSE, "valY");
		} else {
			lsr_write_coordinate(lsr, ((SVG_Point *)val)->x, GF_FALSE, "valX");
			lsr_write_coordinate(lsr, ((SVG_Point *)val)->y, GF_FALSE, "valY");
		}
		break;
	default:
		lsr_write_extension(lsr, NULL, 0, name);
		break;
	}
}

static void lsr_write_anim_value(GF_LASeRCodec *lsr, SMIL_AnimateValue *val, const char *name)
{
	if (!val || !val->type) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		u32 type = svg_type_to_lsr_anim(val->type, 0, NULL, val->value);
		if (type==255) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] unsupported anim type %d (%s) - skipping\n", val->type , gf_svg_attribute_type_to_string(val->type) ));
			GF_LSR_WRITE_INT(lsr, 0, 1, name);
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, name);
			GF_LSR_WRITE_INT(lsr, type, 4, "type");
			lsr_write_an_anim_value(lsr, val->value, type, val->type, 0, name);
		}
	}
}

static void lsr_write_anim_values(GF_LASeRCodec *lsr, SMIL_AnimateValues *anims, const char *name)
{
	u32 type, i, count = 0;
	if (anims && anims->type) count = gf_list_count(anims->values);

	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	type = svg_type_to_lsr_anim(anims->type, 0, anims->values, NULL);
	if (type==255) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] unsupported anim type %d (%s) - skipping\n", anims->type, gf_svg_attribute_type_to_string(anims->type) ));
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, type, 4, "type");
		lsr_write_vluimsbf5(lsr, count, "count");
		for (i=0; i<count; i++) {
			void *att = gf_list_get(anims->values, i);
			lsr_write_an_anim_value(lsr, att, type, anims->type, 0, "a_value");
		}
	}
}

static void lsr_write_fraction_12(GF_LASeRCodec *lsr, GF_List **l, const char *name)
{
	u32 i, count;
	count = l ? gf_list_count(*l) : 0;
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);
	lsr_write_vluimsbf5(lsr, count, "name");
	for (i=0; i<count; i++) {
		Fixed f = * (Fixed *) gf_list_get(*l, i);
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
static void lsr_write_float_list(GF_LASeRCodec *lsr, GF_List **l, const char *name)
{
	u32 i, count = l ? gf_list_count(*l) : 0;
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, name);
	lsr_write_vluimsbf5(lsr, count, "count");
	for (i=0; i<count; i++) {
		Fixed *v = (Fixed *)gf_list_get(*l, i);
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

static void lsr_write_point_sequence(GF_LASeRCodec *lsr, GF_List **pts, const char *name)
{
	u32 i, count = pts ? gf_list_count(*pts) : 0;
	lsr_write_vluimsbf5(lsr, count, "nbPoints");
	if (!count) return;
	/*TODO golomb coding*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "flag");
	if (1) {
		if (count < 3) {
			u32 nb_bits = 0;
			for (i=0; i<count; i++) {
				u32 k;
				SVG_Point *pt = (SVG_Point *)gf_list_get(*pts, i);
				k = lsr_get_bit_size(lsr, pt->x);
				if (k>nb_bits) nb_bits = k;
				k = lsr_get_bit_size(lsr, pt->y);
				if (k>nb_bits) nb_bits = k;
			}

			GF_LSR_WRITE_INT(lsr, nb_bits, 5, "bits");
			for (i=0; i<count; i++) {
				SVG_Point *pt = (SVG_Point *)gf_list_get(*pts, i);
				u32 v = lsr_translate_coords(lsr, pt->x, nb_bits);
				GF_LSR_WRITE_INT(lsr, v, nb_bits, "x");
				v = lsr_translate_coords(lsr, pt->y, nb_bits);
				GF_LSR_WRITE_INT(lsr, v, nb_bits, "y");
			}
		} else {
			Fixed c_x, c_y;
			u32 k, nb_dx, nb_dy;
			SVG_Point *pt = (SVG_Point *)gf_list_get(*pts, 0);
			nb_dx = 0;
			k = lsr_get_bit_size(lsr, pt->x);
			if (k>nb_dx) nb_dx = k;
			k = lsr_get_bit_size(lsr, pt->y);
			if (k>nb_dx) nb_dx = k;
			GF_LSR_WRITE_INT(lsr, nb_dx, 5, "bits");
			k = lsr_translate_coords(lsr, pt->x, nb_dx);
			GF_LSR_WRITE_INT(lsr, k, nb_dx, "x");
			k = lsr_translate_coords(lsr, pt->y, nb_dx);
			GF_LSR_WRITE_INT(lsr, k, nb_dx, "y");
			c_x = pt->x;
			c_y = pt->y;
			nb_dx = nb_dy = 0;
			for (i=1; i<count; i++) {
				SVG_Point *pt = (SVG_Point *)gf_list_get(*pts, i);
				k = lsr_get_bit_size(lsr, pt->x - c_x);
				if (k>nb_dx) nb_dx = k;
				k = lsr_get_bit_size(lsr, pt->y - c_y);
				if (k>nb_dy) nb_dy = k;
				c_x = pt->x;
				c_y = pt->y;
			}
			GF_LSR_WRITE_INT(lsr, nb_dx, 5, "bitsx");
			GF_LSR_WRITE_INT(lsr, nb_dy, 5, "bitsy");
			c_x = pt->x;
			c_y = pt->y;
			for (i=1; i<count; i++) {
				SVG_Point *pt = (SVG_Point *)gf_list_get(*pts, i);
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
#if USE_GF_PATH
	u32 i, *contour, nb_types;
	GF_List *pts;

	if (!path) {
		lsr_write_vluimsbf5(lsr, 0, "nbPoints");
		lsr_write_vluimsbf5(lsr, 0, "nbOfTypes");
		return;
	}
	pts = gf_list_new();

	contour = path->contours;
	nb_types = 0;
	for (i=0; i<path->n_points; ) {
		switch (path->tags[i]) {
		case GF_PATH_CURVE_ON:
			gf_list_add(pts, &path->points[i]);
			nb_types++;
			i++;
			break;
		case GF_PATH_CLOSE:
			nb_types++;
			i++;
			break;
		case GF_PATH_CURVE_CONIC:
			gf_list_add(pts, &path->points[i]);
			gf_list_add(pts, &path->points[i+1]);
			i+=2;
			nb_types++;
			break;
		case GF_PATH_CURVE_CUBIC:
			gf_list_add(pts, &path->points[i]);
			gf_list_add(pts, &path->points[i+1]);
			gf_list_add(pts, &path->points[i+2]);
			i+=3;
			nb_types++;
			break;
		}
	}
	lsr_write_point_sequence(lsr, &pts, "seq");
	gf_list_del(pts);
	/*first moveTo is skiped*/
	lsr_write_vluimsbf5(lsr, nb_types-1, "nbOfTypes");
	for (i=0; i<path->n_points; ) {
		switch (path->tags[i]) {
		case GF_PATH_CLOSE:
			/*close*/
			GF_LSR_WRITE_INT(lsr, LSR_PATH_COM_Z, 5, name);
			i++;
			break;
		case GF_PATH_CURVE_ON:
			if (!i) {
			} else if (*contour == i-1) {
				/*moveTo*/
				GF_LSR_WRITE_INT(lsr, LSR_PATH_COM_M, 5, name);
			} else {
				/*lineTo*/
				GF_LSR_WRITE_INT(lsr, LSR_PATH_COM_L, 5, name);
			}
			i++;
			break;
		case GF_PATH_CURVE_CONIC:
			/*Conic*/
			GF_LSR_WRITE_INT(lsr, LSR_PATH_COM_Q, 5, name);
			i+=2;
			break;
		case GF_PATH_CURVE_CUBIC:
			/*cubic*/
			GF_LSR_WRITE_INT(lsr, LSR_PATH_COM_C, 5, name);
			i+=3;
			break;
		}
	}
#else
	u32 i, count;
	lsr_write_point_sequence(lsr, path->points, "seq");
	/*initial move is not coded*/
	count = gf_list_count(path->commands);
	lsr_write_vluimsbf5(lsr, count, "nbOfTypes");
	for (i; i<count; i++) {
		u8 type = *(u8 *) gf_list_get(path->commands, i);
		GF_LSR_WRITE_INT(lsr, type, 5, name);
	}
#endif
}

static void lsr_write_rotate_type(GF_LASeRCodec *lsr, SVG_Rotate *rotate, const char *name)
{
	GF_LSR_WRITE_INT(lsr, rotate ? 1 : 0, 1, name);
	if (!rotate) return;

	if ((rotate->type == SVG_NUMBER_AUTO) || (rotate->type == SVG_NUMBER_AUTO_REVERSE)) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, (rotate->type == SVG_NUMBER_AUTO) ? 0 : 1, 1, "rotate");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_fixed_16_8(lsr, rotate->value, "rotate");
	}
}
static void lsr_write_sync_behavior(GF_LASeRCodec *lsr, SMIL_SyncBehavior *sync, const char *name)
{
	GF_LSR_WRITE_INT(lsr, sync ? 1 : 0, 1, name);
	if (!sync) return;
	assert(*sync!=SMIL_SYNCBEHAVIOR_INHERIT);
	GF_LSR_WRITE_INT(lsr, *sync-1, 2, name);
}
static void lsr_write_sync_tolerance(GF_LASeRCodec *lsr, SMIL_SyncTolerance *sync, const char *name)
{
	GF_LSR_WRITE_INT(lsr, sync ? 1 : 0, 1, name);
	if (!sync) return;
	assert(sync->type!=SMIL_SYNCTOLERANCE_INHERIT);

	if (sync->type==SMIL_SYNCTOLERANCE_DEFAULT) {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
		lsr_write_vluimsbf5(lsr, (u32) (sync->value*lsr->time_resolution), "value");
	}
}
static void lsr_write_coord_list(GF_LASeRCodec *lsr, GF_List **coords, const char *name)
{
	u32 i, count = coords ? gf_list_count(*coords) : 0;
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		lsr_write_vluimsbf5(lsr, count, "nb_coords");
		for (i=0; i<count; i++) {
			SVG_Coordinate *c = (SVG_Coordinate *)gf_list_get(*coords, i);
			u32 res = lsr_translate_coords(lsr, c->value, lsr->coord_bits);
			GF_LSR_WRITE_INT(lsr, res, lsr->coord_bits, name);
		}
	}
}

static void lsr_write_transform_behavior(GF_LASeRCodec *lsr, SVG_TransformBehavior *type)
{
	GF_LSR_WRITE_INT(lsr, type ? 1 : 0, 1, "hasTransformBehavior");
	if (!type) return;
	GF_LSR_WRITE_INT(lsr, *type, 4, "transformBehavior");
}

static void lsr_write_gradient_units(GF_LASeRCodec *lsr, SVG_GradientUnit *type)
{
	GF_LSR_WRITE_INT(lsr, type ? 1 : 0, 1, "hasGradientUnits");
	if (!type) return;
	GF_LSR_WRITE_INT(lsr, *type ? 1 : 0, 1, "gradientUnits");
}

static void lsr_write_content_type(GF_LASeRCodec *lsr, SVG_String *type, const char *name)
{
	if (type) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasType");
		lsr_write_byte_align_string(lsr, *type, "type");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasType");
	}
}
static void lsr_write_script_type(GF_LASeRCodec *lsr, SVG_String *type)
{
	GF_LSR_WRITE_INT(lsr, type ? 1 : 0, 1, "hasType");
	if (!type) return;
	if (!strcmp(*type, "application/ecmascript")) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, 0, 1, "script");
	} else if (!strcmp(*type, "application/jar-archive")) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, 1, 1, "script");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		lsr_write_byte_align_string(lsr, *type, "type");
	}
}

static void lsr_write_value_with_units(GF_LASeRCodec *lsr, SVG_Number *n, const char *name)
{
	s32 val;

	if (!n) {
		GF_LSR_WRITE_INT(lsr, 0, 32, name);
		GF_LSR_WRITE_INT(lsr, 0, 3, "units");
		return;
	}

#ifdef GPAC_FIXED_POINT
	val = n->value >> 8;
#else
	val = (s32) (n->value * (1<<8) );
#endif
	GF_LSR_WRITE_INT(lsr, val, 32, name);
	switch (n->type) {
	case SVG_NUMBER_IN:
		GF_LSR_WRITE_INT(lsr, 1, 3, "units");
		break;
	case SVG_NUMBER_CM:
		GF_LSR_WRITE_INT(lsr, 2, 3, "units");
		break;
	case SVG_NUMBER_MM:
		GF_LSR_WRITE_INT(lsr, 3, 3, "units");
		break;
	case SVG_NUMBER_PT:
		GF_LSR_WRITE_INT(lsr, 4, 3, "units");
		break;
	case SVG_NUMBER_PC:
		GF_LSR_WRITE_INT(lsr, 5, 3, "units");
		break;
	case SVG_NUMBER_PERCENTAGE:
		GF_LSR_WRITE_INT(lsr, 6, 3, "units");
		break;
	default:
		GF_LSR_WRITE_INT(lsr, 0, 3, "units");
		break;
	}
}


static void lsr_write_clip_time(GF_LASeRCodec *lsr, SVG_Clock *clock, const char *name)
{
	if (!clock || *clock <= 0) {
		GF_LSR_WRITE_INT(lsr, 0, 1, name);
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, name);
		GF_LSR_WRITE_INT(lsr, 0, 1, "isEnum");
		GF_LSR_WRITE_INT(lsr, 0, 1, "sign");
		lsr_write_vluimsbf5(lsr, (u32) (lsr->time_resolution**clock), "val");
	}
}

static void lsr_write_href_anim(GF_LASeRCodec *lsr, XMLRI *href, SVG_Element *parent)
{
	if (!href || (href->target && (href->target==parent))) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_href");
	} else {
		lsr_write_href(lsr, href);
	}
}

static void lsr_write_attribute_type(GF_LASeRCodec *lsr, SVGAllAttributes *atts)
{
	if (!atts->attributeType) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasAttributeType");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasAttributeType");
		GF_LSR_WRITE_INT(lsr, (*atts->attributeType), 2, "attributeType");
	}
}

static void lsr_write_preserve_aspect_ratio(GF_LASeRCodec *lsr, SVG_PreserveAspectRatio *preserveAspectRatio)
{
	GF_LSR_WRITE_INT(lsr, preserveAspectRatio ? 1 : 0, 1, "hasPreserveAspectRatio");
	if (!preserveAspectRatio) return;
	GF_LSR_WRITE_INT(lsr, 0, 1, "choice (meetOrSlice)");
	GF_LSR_WRITE_INT(lsr, preserveAspectRatio->defer ? 1 : 0, 1, "choice (defer)");
	switch (preserveAspectRatio->align) {
	case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
		GF_LSR_WRITE_INT(lsr, 1, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMAXYMID:
		GF_LSR_WRITE_INT(lsr, 2, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
		GF_LSR_WRITE_INT(lsr, 3, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
		GF_LSR_WRITE_INT(lsr, 4, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMIDYMID:
		GF_LSR_WRITE_INT(lsr, 5, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
		GF_LSR_WRITE_INT(lsr, 6, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMINYMAX:
		GF_LSR_WRITE_INT(lsr, 7, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMINYMID:
		GF_LSR_WRITE_INT(lsr, 8, 4, "alignXandY");
		break;
	case SVG_PRESERVEASPECTRATIO_XMINYMIN:
		GF_LSR_WRITE_INT(lsr, 9, 4, "alignXandY");
		break;
	default:
		GF_LSR_WRITE_INT(lsr, 0, 4, "alignXandY");
		break;
	}
}


#define lsr_write_err() GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired")


static void lsr_write_a(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr,  elt, &atts);
	lsr_write_stroke(lsr,  elt, &atts);
	lsr_write_err();
	GF_LSR_WRITE_INT(lsr, (atts.target!=NULL) ? 1 : 0, 1, "hasTarget");
	if (atts.target) lsr_write_byte_align_string(lsr, *(SVG_String*)atts.target, "target");
	lsr_write_href(lsr, atts.xlink_href);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr,  elt, GF_FALSE);
}

static void lsr_write_animate(GF_LASeRCodec *lsr, SVG_Element *elt, SVG_Element *parent)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_animatable(lsr, atts.attributeName, atts.xlink_href, "attributeName");

	lsr_write_accumulate(lsr, atts.accumulate);
	lsr_write_additive(lsr, atts.additive);
	lsr_write_anim_value(lsr, atts.by, "by");
	lsr_write_calc_mode(lsr, atts.calcMode);
	lsr_write_anim_value(lsr, atts.from, "from");
	lsr_write_fraction_12(lsr, atts.keySplines, "keySplines");
	lsr_write_fraction_12(lsr, atts.keyTimes, "keyTimes");
	lsr_write_anim_values(lsr, atts.values, "values");
	lsr_write_attribute_type(lsr, &atts);
	lsr_write_smil_times(lsr, atts.begin, "begin", GF_TRUE);
	lsr_write_duration(lsr, atts.dur, "dur");
	lsr_write_anim_fill(lsr, atts.smil_fill);
	lsr_write_anim_repeat(lsr, atts.repeatCount);
	lsr_write_repeat_duration(lsr, atts.repeatDur);
	lsr_write_anim_restart(lsr, atts.restart);
	lsr_write_anim_value(lsr, atts.to, "to");
	lsr_write_href_anim(lsr, atts.xlink_href, parent);
	GF_LSR_WRITE_INT(lsr, (atts.lsr_enabled && *atts.lsr_enabled) ? 1 : 0, 1, "enabled");

	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}


static void lsr_write_animateMotion(GF_LASeRCodec *lsr, SVG_Element*elt, SVG_Element *parent)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_accumulate(lsr, atts.accumulate);
	lsr_write_additive(lsr, atts.additive);
	lsr_write_anim_value(lsr, atts.by, "by");
	lsr_write_calc_mode(lsr, atts.calcMode);
	lsr_write_anim_value(lsr, atts.from, "from");
	lsr_write_fraction_12(lsr, atts.keySplines, "keySplines");
	lsr_write_fraction_12(lsr, atts.keyTimes, "keyTimes");
	lsr_write_anim_values(lsr, atts.values, "values");
	lsr_write_attribute_type(lsr, &atts);
	lsr_write_smil_times(lsr, atts.begin, "begin", GF_TRUE);
	lsr_write_duration(lsr, atts.dur, "dur");
	lsr_write_anim_fill(lsr, atts.smil_fill);
	lsr_write_anim_repeat(lsr, atts.repeatCount);
	lsr_write_repeat_duration(lsr, atts.repeatDur);
	lsr_write_anim_restart(lsr, atts.restart);
	lsr_write_anim_value(lsr, atts.to, "to");

	lsr_write_float_list(lsr, atts.keyPoints, "keyPoints");
	if (atts.path) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasPath");
		lsr_write_path_type(lsr, atts.path, "path");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasPath");
	}
	lsr_write_rotate_type(lsr, atts.rotate, "rotate");

	lsr_write_href_anim(lsr, atts.xlink_href, parent);
	GF_LSR_WRITE_INT(lsr, (atts.lsr_enabled && *atts.lsr_enabled) ? 1 : 0, 1, "enabled");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_animateTransform(GF_LASeRCodec *lsr, SVG_Element *elt, SVG_Element *parent)
{
	u32 type;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_animatable(lsr, atts.attributeName, atts.xlink_href, "attributeName");

	/*no default value for type or we MUST code it in LASeR ...*/
	type = atts.transform_type ? *atts.transform_type : SVG_TRANSFORM_TRANSLATE;
	/*enumeration rotate{0} scale{1} skewX{2} skewY{3} translate{4}*/
	switch (type) {
	case SVG_TRANSFORM_ROTATE:
		GF_LSR_WRITE_INT(lsr, 0, 3, "rotscatra");
		break;
	case SVG_TRANSFORM_SCALE:
		GF_LSR_WRITE_INT(lsr, 1, 3, "rotscatra");
		break;
	case SVG_TRANSFORM_SKEWX:
		GF_LSR_WRITE_INT(lsr, 2, 3, "rotscatra");
		break;
	case SVG_TRANSFORM_SKEWY:
		GF_LSR_WRITE_INT(lsr, 3, 3, "rotscatra");
		break;
	case SVG_TRANSFORM_TRANSLATE:
		GF_LSR_WRITE_INT(lsr, 4, 3, "rotscatra");
		break;
	}

	lsr_write_accumulate(lsr, atts.accumulate);
	lsr_write_additive(lsr, atts.additive);
	lsr_write_anim_value(lsr, atts.by, "by");
	lsr_write_calc_mode(lsr, atts.calcMode);
	lsr_write_anim_value(lsr, atts.from, "from");
	lsr_write_fraction_12(lsr, atts.keySplines, "keySplines");
	lsr_write_fraction_12(lsr, atts.keyTimes, "keyTimes");
	lsr_write_anim_values(lsr, atts.values, "values");
	lsr_write_attribute_type(lsr, &atts);
	lsr_write_smil_times(lsr, atts.begin, "begin", GF_TRUE);
	lsr_write_duration(lsr, atts.dur, "dur");
	lsr_write_anim_fill(lsr, atts.smil_fill);
	lsr_write_anim_repeat(lsr, atts.repeatCount);
	lsr_write_repeat_duration(lsr, atts.repeatDur);
	lsr_write_anim_restart(lsr, atts.restart);
	lsr_write_anim_value(lsr, atts.to, "to");

	lsr_write_href_anim(lsr, atts.xlink_href, parent);
	GF_LSR_WRITE_INT(lsr, (atts.lsr_enabled && *atts.lsr_enabled) ? 1 : 0, 1, "enabled");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_audio(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_smil_times(lsr, atts.begin, "begin", GF_TRUE);
	lsr_write_duration(lsr, atts.dur, "dur");
	GF_LSR_WRITE_INT(lsr, atts.externalResourcesRequired ? *atts.externalResourcesRequired : 0, 1, "externalResourcesRequired");
	lsr_write_anim_repeat(lsr, atts.repeatCount);
	lsr_write_repeat_duration(lsr, atts.repeatDur);
	lsr_write_anim_restart(lsr, atts.restart);
	lsr_write_sync_behavior(lsr, atts.syncBehavior, "syncBehavior");
	lsr_write_sync_tolerance(lsr, atts.syncTolerance, "syncTolerance");
	lsr_write_content_type(lsr, atts.xlink_type, "type");
	lsr_write_href(lsr, atts.xlink_href);

	lsr_write_clip_time(lsr, atts.clipBegin, "clipBegin");
	lsr_write_clip_time(lsr, atts.clipEnd, "clipEnd");
	GF_LSR_WRITE_INT(lsr, atts.syncReference ? 1 : 0, 1, "hasSyncReference");
	if (atts.syncReference) lsr_write_any_uri(lsr, atts.syncReference, "syncReference");

	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_circle(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	lsr_write_coordinate_ptr(lsr, atts.cx, GF_TRUE, "cx");
	lsr_write_coordinate_ptr(lsr, atts.cy, GF_TRUE, "cy");
	lsr_write_coordinate_ptr(lsr, atts.r, GF_FALSE, "r");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr,  elt, GF_FALSE);
}
static void lsr_write_conditional(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	GF_DOMUpdates *up;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_smil_times(lsr, atts.begin, "begin", 1);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	GF_LSR_WRITE_INT(lsr, (atts.lsr_enabled && *atts.lsr_enabled) ? 1 : 0, 1, "enabled");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	up = elt->children ? (GF_DOMUpdates*)elt->children->node : NULL;
	lsr_write_command_list(lsr, up ? up->updates : NULL, elt, GF_FALSE);
	lsr_write_private_attributes(lsr, elt);
}

static void lsr_write_cursorManager(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
	lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "y");
	lsr_write_href(lsr, atts.xlink_href);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_data(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_defs(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_ellipse(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	lsr_write_coordinate_ptr(lsr, atts.cx, GF_TRUE, "cx");
	lsr_write_coordinate_ptr(lsr, atts.cy, GF_TRUE, "cy");
	lsr_write_coordinate_ptr(lsr, atts.rx, GF_FALSE, "rx");
	lsr_write_coordinate_ptr(lsr, atts.ry, GF_FALSE, "ry");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_foreignObject(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired&&*atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	lsr_write_coordinate_ptr(lsr, atts.height, GF_FALSE, "height");
	lsr_write_coordinate_ptr(lsr, atts.width, GF_FALSE, "width");
	lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
	lsr_write_coordinate_ptr(lsr, atts.y, GF_TRUE, "y");

	lsr_write_any_attribute(lsr, elt, GF_TRUE);
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
}


static void lsr_write_g(GF_LASeRCodec *lsr, SVG_Element *elt, Bool ommit_tag)
{
	Bool is_same = GF_FALSE;
	Bool same_fill;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);


	if (!ommit_tag
	        && lsr_elt_has_same_base(lsr, &atts, lsr->prev_g, &same_fill, NULL, GF_FALSE)
	        && same_fill
	   ) {
		/*samegType*/
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_sameg, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		is_same = GF_TRUE;
	} else {
		/*gType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_g, 6, "ch4");

		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
		lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
		GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_g = elt;
	}
	lsr_write_group_content(lsr, elt, is_same);
}
static void lsr_write_image(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	lsr_write_coordinate_ptr(lsr, atts.height, GF_TRUE, "height");

	if (atts.opacity && (atts.opacity->type == SVG_NUMBER_VALUE)) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "opacity");
		lsr_write_fixed_clamp(lsr, atts.opacity->value, "opacity");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "opacity");
	}
	lsr_write_preserve_aspect_ratio(lsr, atts.preserveAspectRatio);

	lsr_write_content_type(lsr, atts.xlink_type, "type");
	lsr_write_coordinate_ptr(lsr, atts.width, GF_TRUE, "width");
	lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
	lsr_write_coordinate_ptr(lsr, atts.y, GF_TRUE, "y");
	lsr_write_href(lsr, atts.xlink_href);
	lsr_write_transform_behavior(lsr, atts.transformBehavior);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_line(GF_LASeRCodec *lsr, SVG_Element *elt, Bool ommit_tag)
{
	Bool same_fill;
	Bool is_same = GF_FALSE;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, &atts, lsr->prev_line, &same_fill, NULL, GF_FALSE)
	        && same_fill
	   ) {
		/*samelineType*/
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_sameline, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		is_same = GF_TRUE;
	} else {
		/*lineType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_line, 6, "ch4");

		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr,  elt, &atts);
		lsr_write_stroke(lsr,  elt, &atts);
	}

	lsr_write_coordinate_ptr(lsr, atts.x1, GF_TRUE, "x1");
	lsr_write_coordinate_ptr(lsr, atts.x2, GF_FALSE, "x2");
	lsr_write_coordinate_ptr(lsr, atts.y1, GF_TRUE, "y1");
	lsr_write_coordinate_ptr(lsr, atts.y2, GF_FALSE, "y2");
	if (!is_same) {
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_line = elt;
	}
	lsr_write_group_content(lsr, elt, is_same);
}
static void lsr_write_linearGradient(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr,  elt, &atts);
	lsr_write_stroke(lsr,  elt, &atts);
	lsr_write_gradient_units(lsr, atts.gradientUnits);
	lsr_write_coordinate_ptr(lsr, atts.x1, GF_TRUE, "x1");
	lsr_write_coordinate_ptr(lsr, atts.x2, GF_TRUE, "x2");
	lsr_write_coordinate_ptr(lsr, atts.y1, GF_TRUE, "y1");
	lsr_write_coordinate_ptr(lsr, atts.y2, GF_TRUE, "y2");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_mpath(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_href(lsr, atts.xlink_href);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_path(GF_LASeRCodec *lsr, SVG_Element *elt, Bool ommit_tag)
{
	Bool same_fill;
	Bool is_same = GF_FALSE;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, &atts, lsr->prev_path, &same_fill, NULL, GF_FALSE) ) {
		is_same = GF_TRUE;
		if (!same_fill) {
			/*samepathfillType*/
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_samepathfill, 6, "ch4");
			lsr_write_id(lsr, (GF_Node *) elt);
			lsr_write_fill(lsr,  elt, &atts);
		} else {
			/*samepathType*/
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_samepath, 6, "ch4");
			lsr_write_id(lsr, (GF_Node *) elt);
		}
		lsr_write_path_type(lsr, atts.d, "d");
	} else {
		/*pathType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_path, 6, "ch4");

		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr,  elt, &atts);
		lsr_write_stroke(lsr,  elt, &atts);
		lsr_write_path_type(lsr, atts.d, "d");
		if (atts.pathLength) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "hasPathLength");
			lsr_write_fixed_16_8(lsr, atts.pathLength->value, "pathLength");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "hasPathLength");
		}
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_path = elt;
	}
	lsr_write_group_content(lsr, elt, is_same);
}
static void lsr_write_polygon(GF_LASeRCodec *lsr, SVG_Element *elt, Bool is_polyline, Bool ommit_tag)
{
	Bool same_fill, same_stroke;
	u32 same_type = 0;

	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, &atts, lsr->prev_polygon, &same_fill, &same_stroke, GF_TRUE)
	   ) {
		if (same_fill && same_stroke) same_type = 1;
		else if (same_fill) same_type = 3;
		else if (same_stroke) same_type = 2;
	}

	if (same_type) {
		/*samepolylineType / samepolygonType */
		u8 type = is_polyline ? LSR_SCENE_CONTENT_MODEL_samepolyline : LSR_SCENE_CONTENT_MODEL_samepolygon;
		/*samepolylinefillType  / samepolygonfillType */
		if (same_type==2) type = is_polyline ? LSR_SCENE_CONTENT_MODEL_samepolylinefill : LSR_SCENE_CONTENT_MODEL_samepolygonfill;
		/*samepolylinestrokeType  / samepolygonstrokeType*/
		else if (same_type==3) type = is_polyline ? LSR_SCENE_CONTENT_MODEL_samepolylinestroke : LSR_SCENE_CONTENT_MODEL_samepolygonstroke;

		GF_LSR_WRITE_INT(lsr, type, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		if (same_type==2) lsr_write_fill(lsr, elt, &atts);
		else if (same_type==3) lsr_write_stroke(lsr, elt, &atts);
		lsr_write_point_sequence(lsr, atts.points, "points");
	} else {
		/*polyline/polygon*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, is_polyline ? LSR_SCENE_CONTENT_MODEL_polyline : LSR_SCENE_CONTENT_MODEL_polygon, 6, "ch4");

		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr, elt, &atts);
		lsr_write_stroke(lsr, elt, &atts);
		lsr_write_point_sequence(lsr, atts.points, "points");
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_polygon = elt;
	}
	lsr_write_group_content(lsr, elt, same_type);
}
static void lsr_write_radialGradient(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr,  elt, &atts);
	lsr_write_stroke(lsr,  elt, &atts);
	lsr_write_coordinate_ptr(lsr, atts.cx, GF_TRUE, "cx");
	lsr_write_coordinate_ptr(lsr, atts.cy, GF_TRUE, "cy");
	lsr_write_gradient_units(lsr, atts.gradientUnits);
	lsr_write_coordinate_ptr(lsr, atts.r, GF_TRUE, "r");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_rect(GF_LASeRCodec *lsr, SVG_Element *elt, Bool ommit_tag)
{
	u32 same_type = 0;
	Bool same_fill;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, &atts, lsr->prev_rect, &same_fill, NULL, GF_FALSE)
	   ) {
		if (!same_fill) {
			same_type = 2;
			/*samerectfillType*/
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_samerectfill, 6, "ch4");
		} else {
			same_type = 1;
			/*samerectType*/
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_samerect, 6, "ch4");
		}
		lsr_write_id(lsr, (GF_Node *) elt);
		if (same_type==2) lsr_write_fill(lsr,  elt, &atts);
		lsr_write_coordinate_ptr(lsr, atts.height, GF_FALSE, "height");
		lsr_write_coordinate_ptr(lsr, atts.width, GF_FALSE, "width");
		lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
		lsr_write_coordinate_ptr(lsr, atts.y, GF_TRUE, "y");
	} else {
		/*rectType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_rect, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr,  elt, &atts);
		lsr_write_stroke(lsr,  elt, &atts);
		lsr_write_coordinate_ptr(lsr, atts.height, GF_FALSE, "height");
		lsr_write_coordinate_ptr(lsr, atts.rx, GF_TRUE, "rx");
		lsr_write_coordinate_ptr(lsr, atts.ry, GF_TRUE, "ry");
		lsr_write_coordinate_ptr(lsr, atts.width, GF_FALSE, "width");
		lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
		lsr_write_coordinate_ptr(lsr, atts.y, GF_TRUE, "y");
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_rect = elt;
	}
	lsr_write_group_content(lsr, elt, same_type);
}

static void lsr_write_rectClip(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	if (atts.size) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "size");
		lsr_write_coordinate(lsr, atts.size->width, GF_FALSE, "width");
		lsr_write_coordinate(lsr, atts.size->height, GF_FALSE, "height");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "size");
	}
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_script(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	lsr_write_script_type(lsr, atts.type);
	lsr_write_href(lsr, atts.xlink_href);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_selector(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");

	GF_LSR_WRITE_INT(lsr, atts.choice ? 1 : 0, 1, "hasChoice");
	if (atts.choice) {
		if (atts.choice->type==LASeR_CHOICE_N) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
			GF_LSR_WRITE_INT(lsr, atts.choice->choice_index, 8, "value");
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
			GF_LSR_WRITE_INT(lsr, atts.choice->type, 1, "type");
		}
	}
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_set(GF_LASeRCodec *lsr, SVG_Element *elt, SVG_Element *parent)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_animatable(lsr, atts.attributeName, atts.xlink_href, "attributeName");
	lsr_write_attribute_type(lsr, &atts);
	lsr_write_smil_times(lsr, atts.begin, "begin", GF_TRUE);
	lsr_write_duration(lsr, atts.dur, "dur");
	lsr_write_anim_fill(lsr, atts.smil_fill);
	lsr_write_anim_repeat(lsr, atts.repeatCount);
	lsr_write_repeat_duration(lsr, atts.repeatDur);
	lsr_write_anim_restart(lsr, atts.restart);
	lsr_write_anim_value(lsr, atts.to, "to");
	lsr_write_href_anim(lsr, atts.xlink_href, parent);
	GF_LSR_WRITE_INT(lsr, (atts.lsr_enabled && *atts.lsr_enabled) ? 1 : 0, 1, "enabled");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_simpleLayout(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	if (atts.delta) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "delta");
		lsr_write_coordinate(lsr, atts.delta->width, GF_FALSE, "width");
		lsr_write_coordinate(lsr, atts.delta->height, GF_FALSE, "height");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "delta");
	}
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");

	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_stop(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr,  elt, &atts);
	lsr_write_stroke(lsr,  elt, &atts);
	lsr_write_fixed_16_8(lsr, atts.offset ? atts.offset->value : 0, "offset");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}
static void lsr_write_svg(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SMIL_Duration snap;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr,  elt, &atts);
	lsr_write_stroke(lsr,  elt, &atts);
	lsr_write_string_attribute(lsr, atts.baseProfile ? *atts.baseProfile : NULL, "baseProfile");
	lsr_write_string_attribute(lsr, atts.contentScriptType ? *atts.contentScriptType : NULL, "contentScriptType");
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	lsr_write_value_with_units(lsr, atts.height, "height");

	GF_LSR_WRITE_INT(lsr, atts.playbackOrder ? 1 : 0, 1, "hasPlaybackOrder");
	if (atts.playbackOrder) GF_LSR_WRITE_INT(lsr, *atts.playbackOrder, 1, "playbackOrder");
	lsr_write_preserve_aspect_ratio(lsr, atts.preserveAspectRatio);

	snap.clock_value = atts.snapshotTime ? *atts.snapshotTime : 0;
	snap.type = snap.clock_value ? SMIL_DURATION_DEFINED : SMIL_DURATION_INDEFINITE;
	lsr_write_duration(lsr, atts.snapshotTime ? &snap : NULL, "has_snapshotTime");

	GF_LSR_WRITE_INT(lsr, atts.syncBehaviorDefault ? 1 : 0, 1, "hasSyncBehavior");
	if (atts.syncBehaviorDefault) {
		switch (*atts.syncBehaviorDefault) {
		case SMIL_SYNCBEHAVIOR_CANSLIP:
			GF_LSR_WRITE_INT(lsr, 0, 2, "syncBehavior");
			break;
		case SMIL_SYNCBEHAVIOR_INDEPENDENT:
			GF_LSR_WRITE_INT(lsr, 1, 2, "syncBehavior");
			break;
		case SMIL_SYNCBEHAVIOR_LOCKED:
			GF_LSR_WRITE_INT(lsr, 3, 2, "syncBehavior");
			break;
		default:
			GF_LSR_WRITE_INT(lsr, 2, 2, "syncBehavior");
			break;
		}
	}
	GF_LSR_WRITE_INT(lsr, atts.syncToleranceDefault ? 1 : 0, 1, "hasSyncToleranceDefault");
	if (atts.syncToleranceDefault) {
		if (atts.syncToleranceDefault->type==SMIL_SYNCTOLERANCE_VALUE) {
			GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
			lsr_write_vluimsbf5(lsr, (u32) (atts.syncToleranceDefault->value*lsr->time_resolution), "value");
		} else {
			GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		}
	}
	GF_LSR_WRITE_INT(lsr, atts.timelineBegin ? 1 : 0, 1, "hasTimelineBegin");
	if (atts.timelineBegin) GF_LSR_WRITE_INT(lsr, *atts.timelineBegin, 1, "timelineBegin");

	lsr_write_string_attribute(lsr, atts.version ? *atts.version : NULL, "version");

	GF_LSR_WRITE_INT(lsr, atts.viewBox ? 1 : 0, 1, "hasViewBox");
	if (atts.viewBox) {
		lsr_write_fixed_16_8(lsr, atts.viewBox->x, "viewbox.x");
		lsr_write_fixed_16_8(lsr, atts.viewBox->y, "viewbox.y");
		lsr_write_fixed_16_8(lsr, atts.viewBox->width, "viewbox.width");
		lsr_write_fixed_16_8(lsr, atts.viewBox->height, "viewbox.height");
	}
	lsr_write_value_with_units(lsr, atts.width, "width");

	GF_LSR_WRITE_INT(lsr, atts.zoomAndPan ? 1 : 0, 1, "hasZoomAndPan");
	if (atts.zoomAndPan) {
		GF_LSR_WRITE_INT(lsr, (*atts.zoomAndPan==SVG_ZOOMANDPAN_MAGNIFY) ? 1 : 0, 1, "zoomAndPan");
	}
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_switch(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, 0);
}

static void lsr_write_text(GF_LASeRCodec *lsr, SVG_Element *elt, Bool ommit_tag)
{
	Bool same_fill;
	u32 same_type = 0;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, &atts, lsr->prev_text, &same_fill, NULL, 0) ) {
		if (!same_fill) {
			/*sametextfillType*/
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_sametextfill, 6, "ch4");
			lsr_write_id(lsr, (GF_Node *) elt);
			lsr_write_fill(lsr,  elt, &atts);
		} else {
			/*sametextType*/
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_sametext, 6, "ch4");
			lsr_write_id(lsr, (GF_Node *) elt);
		}
		lsr_write_coord_list(lsr, atts.text_x, "x");
		lsr_write_coord_list(lsr, atts.text_y, "y");
		same_type = 1;
	} else {
		/*textType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_text, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
		lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);

		GF_LSR_WRITE_INT(lsr, (atts.editable && *atts.editable) ? 1 : 0, 1, "editable");
		lsr_write_float_list(lsr, atts.text_rotate, "rotate");
		lsr_write_coord_list(lsr, atts.text_x, "x");
		lsr_write_coord_list(lsr, atts.text_y, "y");
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_text = elt;
	}
	lsr_write_group_content(lsr, elt, same_type);
}

static void lsr_write_tspan(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
	lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_use(GF_LASeRCodec *lsr, SVG_Element *elt, Bool ommit_tag)
{
	SVGAllAttributes atts;
	Bool is_same = GF_FALSE;
	gf_svg_flatten_attributes(elt, &atts);

	if (!ommit_tag && lsr_elt_has_same_base(lsr, &atts, lsr->prev_use, NULL, NULL, GF_FALSE)
	        /*TODO check overflow*/
	   ) {
		is_same = GF_TRUE;
		/*sameuseType*/
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_sameuse, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_href(lsr, atts.xlink_href);
	} else {
		/*useType*/
		if (!ommit_tag) GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_use, 6, "ch4");
		lsr_write_id(lsr, (GF_Node *) elt);
		lsr_write_rare(lsr, (GF_Node *) elt);
		lsr_write_fill(lsr, (SVG_Element*)elt, &atts);
		lsr_write_stroke(lsr, (SVG_Element*)elt, &atts);
		GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");

		GF_LSR_WRITE_INT(lsr, atts.overflow ? 1 : 0, 1, "hasOverflow");
		/*one value only??*/
		if (atts.overflow) GF_LSR_WRITE_INT(lsr, 0, 2, "overflow");

		lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
		lsr_write_coordinate_ptr(lsr, atts.y, GF_TRUE, "y");
		lsr_write_href(lsr, atts.xlink_href);
		lsr_write_any_attribute(lsr, elt, GF_TRUE);
		lsr->prev_use = elt;
	}
	lsr_write_group_content(lsr, elt, is_same);
}

static void lsr_write_video(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	u32 fs_value;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	fs_value = 0;
	if (atts.fullscreen) {
		fs_value = *atts.fullscreen + 1;
		atts.fullscreen = NULL;
	}

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);
	lsr_write_smil_times(lsr, atts.begin, "begin", GF_TRUE);
	lsr_write_duration(lsr, atts.dur, "dur");
	GF_LSR_WRITE_INT(lsr, (atts.externalResourcesRequired && *atts.externalResourcesRequired) ? 1 : 0, 1, "externalResourcesRequired");
	lsr_write_coordinate_ptr(lsr, atts.height, GF_TRUE, "height");

	GF_LSR_WRITE_INT(lsr, atts.overlay  ? 1 : 0, 1, "hasOverlay");
	if (atts.overlay) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "choice");
		GF_LSR_WRITE_INT(lsr, *atts.overlay, 1, "overlay");
	}
	lsr_write_preserve_aspect_ratio(lsr, atts.preserveAspectRatio);

	lsr_write_anim_repeat(lsr, atts.repeatCount);
	lsr_write_repeat_duration(lsr, atts.repeatDur);
	lsr_write_anim_restart(lsr, atts.restart);
	lsr_write_sync_behavior(lsr, atts.syncBehavior, "syncBehavior");
	lsr_write_sync_tolerance(lsr, atts.syncTolerance, "syncTolerance");
	lsr_write_transform_behavior(lsr, atts.transformBehavior);
	lsr_write_content_type(lsr, atts.xlink_type, "type");
	lsr_write_coordinate_ptr(lsr, atts.width, GF_TRUE, "width");
	lsr_write_coordinate_ptr(lsr, atts.x, GF_TRUE, "x");
	lsr_write_coordinate_ptr(lsr, atts.y, GF_TRUE, "y");
	lsr_write_href(lsr, atts.xlink_href);

	lsr_write_clip_time(lsr, atts.clipBegin, "clipBegin");
	lsr_write_clip_time(lsr, atts.clipEnd, "clipEnd");

	GF_LSR_WRITE_INT(lsr, fs_value ? 1 : 0, 1, "hasFullscreen");
	if (fs_value) GF_LSR_WRITE_INT(lsr, fs_value - 1, 1, "fullscreen");

	GF_LSR_WRITE_INT(lsr, atts.syncReference ? 1 : 0, 1, "hasSyncReference");
	if (atts.syncReference) lsr_write_any_uri(lsr, atts.syncReference, "syncReference");

	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_listener(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	SVGAllAttributes atts;
	gf_svg_flatten_attributes(elt, &atts);

	lsr_write_id(lsr, (GF_Node *) elt);
	lsr_write_rare(lsr, (GF_Node *) elt);

	GF_LSR_WRITE_INT(lsr, atts.defaultAction ? 1 : 0, 1, "hasDefaultAction");
	if (atts.defaultAction) GF_LSR_WRITE_INT(lsr, *atts.defaultAction ? 1 : 0, 1, "defaultAction");
	if (atts.event) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasEvent");
		lsr_write_event_type(lsr, atts.event->type, atts.event->parameter);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasEvent");
	}
	if (atts.handler && (atts.handler->string || (atts.handler->target && gf_node_get_id((GF_Node *)atts.handler->target) ) )) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasHandler");
		lsr_write_any_uri(lsr, atts.handler, "handler");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasHandler");
	}
	if (atts.observer && atts.observer->target && gf_node_get_id((GF_Node *)atts.observer->target) ) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasObserver");
		lsr_write_codec_IDREF(lsr, atts.observer, "observer");
	} else {
		if (atts.observer) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] listener.observer %s not found in scene - skipping it\n", atts.observer->string ));
		}
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasObserver");
	}

	GF_LSR_WRITE_INT(lsr, atts.phase ? 1 : 0, 1, "hasPhase");
	if (atts.phase) GF_LSR_WRITE_INT(lsr, *atts.phase, 1, "phase");

	GF_LSR_WRITE_INT(lsr, atts.propagate ? 1 : 0, 1, "hasPropagate");
	if (atts.propagate) GF_LSR_WRITE_INT(lsr, *atts.propagate, 1, "propagate");

	if (atts.listener_target && atts.listener_target->target && gf_node_get_id((GF_Node *)atts.listener_target->target) ) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "hasTarget");
		lsr_write_codec_IDREF(lsr, atts.listener_target, "target");
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "hasTarget");
	}
	GF_LSR_WRITE_INT(lsr, (atts.lsr_enabled && *atts.lsr_enabled) ? 1 : 0, 1, "enabled");

	lsr_write_any_attribute(lsr, elt, GF_TRUE);
	lsr_write_group_content(lsr, elt, GF_FALSE);
}

static void lsr_write_scene_content_model(GF_LASeRCodec *lsr, SVG_Element *parent, void *node)
{
	u32 tag = gf_node_get_tag((GF_Node*)node);

	switch(tag) {
	case TAG_SVG_a:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_a, 6, "ch4");
		lsr_write_a(lsr, node);
		break;
	case TAG_SVG_animate:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_animate, 6, "ch4");
		lsr_write_animate(lsr, node, parent);
		break;
	case TAG_SVG_animateColor:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_animateColor, 6, "ch4");
		lsr_write_animate(lsr, node, parent);
		break;
	case TAG_SVG_animateMotion:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_animateMotion, 6, "ch4");
		lsr_write_animateMotion(lsr, node, parent);
		break;
	case TAG_SVG_animateTransform:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_animateTransform, 6, "ch4");
		lsr_write_animateTransform(lsr, node, parent);
		break;
	case TAG_SVG_audio:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_audio, 6, "ch4");
		lsr_write_audio(lsr, node);
		break;
	case TAG_SVG_circle:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_circle, 6, "ch4");
		lsr_write_circle(lsr, node);
		break;
	case TAG_LSR_cursorManager:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_cursorManager, 6, "ch4");
		lsr_write_cursorManager(lsr, node);
		break;
	case TAG_SVG_defs:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_defs, 6, "ch4");
		lsr_write_defs(lsr, node);
		break;
	case TAG_SVG_desc:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_desc, 6, "ch4");
		lsr_write_data(lsr, node);
		break;
	case TAG_SVG_ellipse:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_ellipse, 6, "ch4");
		lsr_write_ellipse(lsr, node);
		break;
	case TAG_SVG_foreignObject:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_foreignObject, 6, "ch4");
		lsr_write_foreignObject(lsr, node);
		break;

	case TAG_SVG_g:
		/*type is written in encoding fct for sameg handling*/
		lsr_write_g(lsr, node, 0);
		break;
	case TAG_SVG_image:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_image, 6, "ch4");
		lsr_write_image(lsr, node);
		break;
	case TAG_SVG_line:
		/*type is written in encoding fct for sameline handling*/
		lsr_write_line(lsr, node, 0);
		break;
	case TAG_SVG_linearGradient:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_linearGradient, 6, "ch4");
		lsr_write_linearGradient(lsr, node);
		break;
	case TAG_SVG_metadata:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_metadata, 6, "ch4");
		lsr_write_data(lsr, node);
		break;
	case TAG_SVG_mpath:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_mpath, 6, "ch4");
		lsr_write_mpath(lsr, node);
		break;
	case TAG_SVG_path:
		/*type is written in encoding fct for samepath handling*/
		lsr_write_path(lsr, node, 0);
		break;
	case TAG_SVG_polygon:
		/*type is written in encoding fct for samepolygon handling*/
		lsr_write_polygon(lsr, node, 0, 0);
		break;
	case TAG_SVG_polyline:
		/*type is written in encoding fct for samepolyline handling*/
		lsr_write_polygon(lsr, node, 1, 0);
		break;
	case TAG_SVG_radialGradient:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_radialGradient, 6, "ch4");
		lsr_write_radialGradient(lsr, node);
		break;
	case TAG_SVG_rect:
		/*type is written in encoding fct for samepolyline handling*/
		lsr_write_rect(lsr, node, 0);
		break;
	case TAG_SVG_script:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_script, 6, "ch4");
		lsr_write_script(lsr, node);
		break;
	case TAG_SVG_set:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_set, 6, "ch4");
		lsr_write_set(lsr, node, parent);
		break;
	case TAG_SVG_stop:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_stop, 6, "ch4");
		lsr_write_stop(lsr, node);
		break;
	case TAG_SVG_switch:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_switch, 6, "ch4");
		lsr_write_switch(lsr, node);
		break;
	case TAG_SVG_text:
		/*type is written in encoding fct for sametext handling*/
		lsr_write_text(lsr, node, 0);
		break;
	case TAG_SVG_title:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_title, 6, "ch4");
		lsr_write_data(lsr, node);
		break;
	case TAG_SVG_tspan:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_tspan, 6, "ch4");
		lsr_write_tspan(lsr, node);
		break;
	case TAG_SVG_use:
		/*type is written in encoding fct for sameuse handling*/
		lsr_write_use(lsr, node, 0);
		break;
	case TAG_SVG_video:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_video, 6, "ch4");
		lsr_write_video(lsr, node);
		break;
	case TAG_SVG_listener:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_listener, 6, "ch4");
		lsr_write_listener(lsr, node);
		break;
	case TAG_LSR_conditional:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_conditional, 6, "ch4");
		lsr_write_conditional(lsr, node);
		break;
	case TAG_LSR_rectClip:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_rectClip, 6, "ch4");
		lsr_write_rectClip(lsr, node);
		break;
	case TAG_LSR_selector:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_selector, 6, "ch4");
		lsr_write_selector(lsr, node);
		break;
	case TAG_LSR_simpleLayout:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_simpleLayout, 6, "ch4");
		lsr_write_simpleLayout(lsr, node);
		break;
#if 0
	/*case privateElement*/
	case TAG_SVG_extendElement:
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_privateContainer, 6, "ch4");
		lsr_write_private_element_container(lsr);
		break;
#endif

	default:
#if 0
		/*case extend*/
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_element_any, 6, "ch4");
		lsr_write_extend_class(lsr, NULL, 0, node);
		break;
#else
		/*hack for encoding - needs cleaning*/
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] node %s not part of LASeR children nodes - skipping\n", gf_node_get_class_name((GF_Node*)node)));
		GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_textContent, 6, "ch4");
		lsr_write_byte_align_string(lsr, "", "textContent");
		break;
#endif
	}
}

static void lsr_write_update_content_model(GF_LASeRCodec *lsr, SVG_Element *parent, void *node)
{
	u32 tag = gf_node_get_tag((GF_Node*)node);

	if (tag==TAG_LSR_conditional) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "ch4");
		GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL2_conditional, 3, "ch61");
		lsr_write_conditional(lsr, node);
	} else if (tag==TAG_LSR_cursorManager) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "ch4");
		GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL2_cursorManager, 3, "ch61");
		lsr_write_cursorManager(lsr, node);
	} else if (tag==TAG_LSR_rectClip) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "ch4");
		GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL2_rectClip, 3, "ch61");
		lsr_write_rectClip(lsr, node);
	} else if (tag==TAG_LSR_selector) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "ch4");
		GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL2_selector, 3, "ch61");
		lsr_write_selector(lsr, node);
	} else if (tag==TAG_LSR_simpleLayout) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "ch4");
		GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL2_simpleLayout, 3, "ch61");
		lsr_write_simpleLayout(lsr, node);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "ch4");
		switch(tag) {
		case TAG_SVG_a:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_a, 6, "ch6");
			lsr_write_a(lsr, node);
			break;
		case TAG_SVG_animate:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_animate, 6, "ch6");
			lsr_write_animate(lsr, node, parent);
			break;
		case TAG_SVG_animateColor:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_animateColor, 6, "ch6");
			lsr_write_animate(lsr, node, parent);
			break;
		case TAG_SVG_animateMotion:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_animateMotion, 6, "ch6");
			lsr_write_animateMotion(lsr, node, parent);
			break;
		case TAG_SVG_animateTransform:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_animateTransform, 6, "ch6");
			lsr_write_animateTransform(lsr, node, parent);
			break;
		case TAG_SVG_audio:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_audio, 6, "ch6");
			lsr_write_audio(lsr, node);
			break;
		case TAG_SVG_circle:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_circle, 6, "ch6");
			lsr_write_circle(lsr, node);
			break;
		case TAG_SVG_defs:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_defs, 6, "ch6");
			lsr_write_defs(lsr, node);
			break;
		case TAG_SVG_desc:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_desc, 6, "ch6");
			lsr_write_data(lsr, node);
			break;
		case TAG_SVG_ellipse:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_ellipse, 6, "ch6");
			lsr_write_ellipse(lsr, node);
			break;
		case TAG_SVG_foreignObject:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_foreignObject, 6, "ch6");
			lsr_write_foreignObject(lsr, node);
			break;
		case TAG_SVG_g:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_g, 6, "ch6");
			lsr_write_g(lsr, node, 1);
			break;
		case TAG_SVG_image:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_image, 6, "ch6");
			lsr_write_image(lsr, node);
			break;
		case TAG_SVG_line:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_line, 6, "ch6");
			lsr_write_line(lsr, node, 1);
			break;
		case TAG_SVG_linearGradient:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_linearGradient, 6, "ch6");
			lsr_write_linearGradient(lsr, node);
			break;
		case TAG_SVG_metadata:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_metadata, 6, "ch6");
			lsr_write_data(lsr, node);
			break;
		case TAG_SVG_mpath:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_mpath, 6, "ch6");
			lsr_write_mpath(lsr, node);
			break;
		case TAG_SVG_path:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_path, 6, "ch6");
			lsr_write_path(lsr, node, 1);
			break;
		case TAG_SVG_polygon:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_polygon, 6, "ch6");
			lsr_write_polygon(lsr, node, 0, 1);
			break;
		case TAG_SVG_polyline:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_polyline, 6, "ch6");
			lsr_write_polygon(lsr, node, 1, 1);
			break;
		case TAG_SVG_radialGradient:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_radialGradient, 6, "ch6");
			lsr_write_radialGradient(lsr, node);
			break;
		case TAG_SVG_rect:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_rect, 6, "ch6");
			lsr_write_rect(lsr, node, 1);
			break;
		case TAG_SVG_script:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_script, 6, "ch6");
			lsr_write_script(lsr, node);
			break;
		case TAG_SVG_set:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_set, 6, "ch6");
			lsr_write_set(lsr, node, parent);
			break;
		case TAG_SVG_stop:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_stop, 6, "ch6");
			lsr_write_stop(lsr, node);
			break;
		case TAG_SVG_svg:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_svg, 6, "ch6");
			lsr_write_svg(lsr, node);
			break;
		case TAG_SVG_switch:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_switch, 6, "ch6");
			lsr_write_switch(lsr, node);
			break;
		case TAG_SVG_text:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_text, 6, "ch6");
			lsr_write_text(lsr, node, 1);
			break;
		case TAG_SVG_title:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_title, 6, "ch6");
			lsr_write_data(lsr, node);
			break;
		case TAG_SVG_tspan:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_tspan, 6, "ch6");
			lsr_write_tspan(lsr, node);
			break;
		case TAG_SVG_use:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_use, 6, "ch6");
			lsr_write_use(lsr, node, 1);
			break;
		case TAG_SVG_video:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_video, 6, "ch6");
			lsr_write_video(lsr, node);
			break;
		case TAG_SVG_listener:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_CONTENT_MODEL_listener, 6, "ch6");
			lsr_write_listener(lsr, node);
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] Cannot encode node %s as command child node\n", gf_node_get_class_name((GF_Node*)node)));
			lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}
	}
}

static void lsr_write_group_content(GF_LASeRCodec *lsr, SVG_Element *elt, Bool skip_object_content)
{
	GF_ChildNodeItem *l;
	u32 count;
	if (!skip_object_content) lsr_write_private_attributes(lsr, elt);

	count = gf_node_list_get_count(elt->children);

	l = elt->children;
	if (!count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
		return;
	}
	GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");
	lsr_write_vluimsbf5(lsr, count, "occ0");

	while (l) {
		if (gf_node_get_tag(l->node)==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)l->node;
			GF_LSR_WRITE_INT(lsr, LSR_SCENE_CONTENT_MODEL_textContent, 6, "ch4");
			lsr_write_byte_align_string(lsr, txt->textContent, "textContent");
		} else {
			lsr_write_scene_content_model(lsr, elt, l->node);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] ############## end %s ###########\n", gf_node_get_class_name((GF_Node*)l->node)));
		}
		l = l->next;
	}
}

static void lsr_write_update_value(GF_LASeRCodec *lsr, SVG_Element *elt, u32 fieldType, u32 att_tag, u32 transformType, void *val, Bool is_indexed)
{
	SVG_Number *n;
	if (is_indexed) {
		assert(gf_list_count(*(GF_List **)val));
		switch (fieldType) {
		case SVG_Points_datatype:/*points*/
		{
			GF_Point2D *pt = (GF_Point2D*)gf_list_get(*(GF_List **)val, 0);
			lsr_write_coordinate(lsr, pt->x, GF_FALSE, "x");
			lsr_write_coordinate(lsr, pt->y, GF_FALSE, "y");
		}
		break;
		case SMIL_Times_datatype:/*smil_times*/
		{
			SMIL_Time *st = (SMIL_Time*)gf_list_get(*(GF_List **)val, 0);
			lsr_write_smil_time(lsr, st);
		}
		break;
		case SMIL_KeyPoints_datatype:/*0to1*/
		{
			SVG_Point *pt = (SVG_Point*)gf_list_get(*(GF_List **)val, 0);
			lsr_write_fixed_clamp(lsr, pt->x, "value_x");
			lsr_write_fixed_clamp(lsr, pt->y, "value_y");
		}
		break;
		case SMIL_KeySplines_datatype:/*float*/
		{
			Fixed *f = (Fixed*)gf_list_get(*(GF_List **)val, 0);
			lsr_write_fixed_16_8(lsr, *f, "v");
		}
		break;
		case SMIL_KeyTimes_datatype:/*keyTime*/
		{
			Fixed *v = (Fixed*)gf_list_get(*(GF_List **)val, 0);
			Fixed f = *v;
			if ((f==0) || (f==FIX_ONE)) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "hasShort");
				GF_LSR_WRITE_INT(lsr, (f==0) ? 0 : 1, 1, "isZero");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "hasShort");
				f *= 4096;
				GF_LSR_WRITE_INT(lsr, (u32) f, 12, "timevalue");
			}
		}
		break;
		case SVG_StrokeDashArray_datatype:/*float*/
		case SVG_ViewBox_datatype:/*float*/
			break;
		case DOM_StringList_datatype:
			if (att_tag==TAG_SVG_ATT_requiredFeatures) {
				/*int*/
			}
			break;
		}
	} else {
		switch (fieldType) {
		case SVG_Boolean_datatype:
			GF_LSR_WRITE_INT(lsr, *(SVG_Boolean*)val ? 1 : 0, 1, "val");
			break;
		case SVG_Paint_datatype:
			lsr_write_paint(lsr, (SVG_Paint *)val, "val");
			break;
		case SVG_Transform_datatype:
			lsr_write_matrix(lsr, (SVG_Transform*)val);
			break;
		case SVG_Transform_Scale_datatype:
			lsr_write_fixed_16_8(lsr, ((SVG_Point *)val)->x, "scale_x");
			lsr_write_fixed_16_8(lsr, ((SVG_Point *)val)->y, "scale_y");
			break;
		case LASeR_Size_datatype:
		case SVG_Transform_Translate_datatype:
			lsr_write_coordinate(lsr, ((SVG_Point *)val)->x, GF_FALSE, "translation_x");
			lsr_write_coordinate(lsr, ((SVG_Point *)val)->y, GF_FALSE, "translation_y");
			break;
		case SVG_Transform_Rotate_datatype:
			GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
			lsr_write_fixed_16_8(lsr, ((SVG_Point_Angle*)val)->angle, "rotate");
			break;
		case SVG_Number_datatype:
		case SVG_FontSize_datatype:
		case SVG_Length_datatype:
			n = (SVG_Number*)val;
			switch (att_tag) {
			/*fractions*/
			case TAG_SVG_ATT_audio_level:
			case TAG_SVG_ATT_fill_opacity:
			case TAG_SVG_ATT_offset:
			case TAG_SVG_ATT_opacity:
			case TAG_SVG_ATT_solid_opacity:
			case TAG_SVG_ATT_stop_opacity:
			case TAG_SVG_ATT_stroke_opacity:
			case TAG_SVG_ATT_viewport_fill_opacity:
				if (n->type==SVG_NUMBER_INHERIT) {
					GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue");
				} else {
					GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
					lsr_write_fixed_clamp(lsr, n->value, "val");
				}
				break;
			case TAG_SVG_ATT_width:
			case TAG_SVG_ATT_height:
				if (elt->sgprivate->tag==TAG_SVG_svg) {
					lsr_write_value_with_units(lsr, n, "val");
				} else {
					lsr_write_coordinate(lsr, n->value, GF_FALSE, "val");
				}
				break;
			default:
				if (n->type==SVG_NUMBER_INHERIT) {
					GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue");
				} else {
					GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
					GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
					lsr_write_fixed_16_8(lsr, n->value, "val");
				}
			}
			break;
		case SVG_Rotate_datatype:
			n = (SVG_Number*)val;
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
		case SVG_Coordinate_datatype:
			n = (SVG_Number*)val;
			lsr_write_coordinate(lsr, n->value, GF_FALSE, "val");
			break;
		//only applies to text x,y and not updatable in laser
#if 0
		case SVG_Coordinates_datatype:
			GF_LSR_WRITE_INT(lsr, 0, 1, "isInherit");
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
			lsr_write_float_list(lsr, (GF_List **)val, "val");
			break;
#endif
		case XMLRI_datatype:
			if ((att_tag==TAG_XLINK_ATT_href) || (att_tag==TAG_SVG_ATT_syncReference)) {
				lsr_write_any_uri(lsr, (XMLRI*)val, "val");
			} else if (((XMLRI*)val)->target) {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefault");
				GF_LSR_WRITE_INT(lsr, 0, 1, "isEscape");
				lsr_write_vluimsbf5(lsr, gf_node_get_id( ((XMLRI*)val)->target) - 1, "ID");
			} else {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isDefault");
			}
			break;
		case SVG_Focus_datatype:
			if (((SVG_Focus*)val)->type == SVG_FOCUS_SELF) {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefault");
				GF_LSR_WRITE_INT(lsr, 1, 1, "isEscape");
				GF_LSR_WRITE_INT(lsr, 1, 2, "escapeEnumVal");
			} else if ( (((SVG_Focus*)val)->type == SVG_FOCUS_AUTO) || !((SVG_Focus*)val)->target.target) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "isDefault");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "isDefault");
				GF_LSR_WRITE_INT(lsr, 0, 1, "isEscape");
				lsr_write_vluimsbf5(lsr, gf_node_get_id( ((SVG_Focus*)val)->target.target) - 1, "ID");
			}
			break;

		case DOM_String_datatype:
		case SVG_ContentType_datatype:
		case SVG_LanguageID_datatype:
			lsr_write_byte_align_string(lsr, val ? *(DOM_String *)val : (char *)"", "val");
			break;
		case SVG_Motion_datatype:
			lsr_write_coordinate(lsr, ((GF_Matrix2D *)val)->m[2], GF_FALSE, "pointValueX");
			lsr_write_coordinate(lsr, ((GF_Matrix2D *)val)->m[5], GF_FALSE, "pointValueY");
			break;
		case SVG_Points_datatype:
			lsr_write_point_sequence(lsr, (GF_List **)val, "val");
			break;
		case SVG_PathData_datatype:
			lsr_write_path_type(lsr, (SVG_PathData*)val, "val");
			break;
		case LASeR_Choice_datatype:
		{
			LASeR_Choice *ch = (LASeR_Choice *)val;
			GF_LSR_WRITE_INT(lsr, (ch->type==LASeR_CHOICE_ALL) ? 1 : 0, 1, "isDefaultValue");
			if (ch->type!=LASeR_CHOICE_ALL) {
				GF_LSR_WRITE_INT(lsr, (ch->type==LASeR_CHOICE_NONE) ? 1 : 0, 1, "escapeFlag");
				if (ch->type==LASeR_CHOICE_NONE) {
					GF_LSR_WRITE_INT(lsr, LASeR_CHOICE_NONE, 2, "escapeEnum");
				} else {
					lsr_write_vluimsbf5(lsr, ((LASeR_Choice *)val)->choice_index, "value");
				}
			}
		}
		break;
		//only applies to svg viewport and not updatable in laser
#if 0
		case SVG_ViewBox_datatype:
		{
			SVG_ViewBox *b = (SVG_ViewBox *)val;
			GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
			GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
			lsr_write_vluimsbf5(lsr, 4, "nb1");
			lsr_write_fixed_16_8(lsr, b->x, "x");
			lsr_write_fixed_16_8(lsr, b->y, "y");
			lsr_write_fixed_16_8(lsr, b->width, "width");
			lsr_write_fixed_16_8(lsr, b->width, "height");
		}
		break;
#endif
		case SVG_FontFamily_datatype:
		{
			SVG_FontFamily *ff = (SVG_FontFamily *)val;
			GF_LSR_WRITE_INT(lsr, (ff->type == SVG_FONTFAMILY_INHERIT) ? 1 : 0, 1, "isDefaultValue");
			if (ff->type != SVG_FONTFAMILY_INHERIT) {
				s32 idx = lsr_get_font_index(lsr, ff);
				if (idx==-1) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] corrupted font table while encoding update value\n"));
					idx=0;
				}
				GF_LSR_WRITE_INT(lsr, 0, 1, "escapeFlag");
				lsr_write_vluimsbf5(lsr, (u32) idx, "nb1");
			}
		}
		break;
		default:
			if ((fieldType>=SVG_FillRule_datatype) && (fieldType<=SVG_LAST_U8_PROPERTY)) {
				u8 v = *(u8 *)val;
				/*TODO fixme, check inherit/default values*/
				if (!v) {
					GF_LSR_WRITE_INT(lsr, 1, 1, "isDefaultValue");
				} else {
					GF_LSR_WRITE_INT(lsr, 0, 1, "isDefaultValue");
					lsr_write_vluimsbf5(lsr, v, "val");
				}
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] update value not implemented for type %d - please fix or report\n", fieldType));
			}
		}
	}
}
/*FIXME - support for scale/translate/rotation*/
static GF_Err lsr_write_add_replace_insert(GF_LASeRCodec *lsr, GF_Command *com)
{
	GF_CommandField *field;
	u8 type = 0;
	Bool is_text_node = GF_FALSE;
	u32 field_type, tr_type = 0;
	if (com->tag==GF_SG_LSR_REPLACE) type = LSR_UPDATE_REPLACE;
	else if (com->tag==GF_SG_LSR_ADD) type = LSR_UPDATE_ADD;
	else if (com->tag==GF_SG_LSR_INSERT) type = LSR_UPDATE_INSERT;
	else return GF_BAD_PARAM;

	GF_LSR_WRITE_INT(lsr, type, 4, "ch4");
	field = (GF_CommandField*)gf_list_get(com->command_fields, 0);
	field_type = 0;
	if (field && ( !field->new_node || (field->fieldIndex==TAG_LSR_ATT_children) ) && !field->node_list) {
		s32 attType = 0;
		field_type = field->fieldType;
		attType = gf_lsr_anim_type_from_attribute(field->fieldIndex);
		if (attType== -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] Attribute %s of element %s is not updatable\n", gf_svg_get_attribute_name(com->node, field->fieldIndex), gf_node_get_class_name(com->node) ));
			return lsr->last_error = GF_BAD_PARAM;
		}
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_attributeName");
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		GF_LSR_WRITE_INT(lsr, attType, 8, "attributeName");
	}
	/*single text */
	else if (field && field->new_node && field->new_node->sgprivate->tag==TAG_DOMText) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_attributeName");
		GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
		GF_LSR_WRITE_INT(lsr, LSR_UPDATE_TYPE_TEXT_CONTENT, 8, "attributeName");
		is_text_node = GF_TRUE;
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
			u8 opAttType;
			opAttType = gf_lsr_anim_type_from_attribute(com->fromFieldIndex);
			GF_LSR_WRITE_INT(lsr, 1, 1, "has_operandAttribute");
			GF_LSR_WRITE_INT(lsr, opAttType, 8, "operandAttribute");
			GF_LSR_WRITE_INT(lsr, 1, 1, "has_operandElementId");
			lsr_write_codec_IDREF_Node(lsr, com->node, "operandElementId");
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "has_operandAttribute");
			GF_LSR_WRITE_INT(lsr, 0, 1, "has_operandElementId");
		}
	}
	lsr_write_codec_IDREF_Node(lsr, com->node, "ref");
	if (field && !field->new_node && !field->node_list && !com->fromNodeID) {
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_value");
		lsr_write_update_value(lsr, (SVG_Element *)com->node, field_type, field->fieldIndex, tr_type, field->field_ptr, (field->pos>=0) ? GF_TRUE : GF_FALSE);
	} else if (is_text_node) {
		GF_DOMText *t = (GF_DOMText *)field->new_node;
		GF_LSR_WRITE_INT(lsr, 1, 1, "has_value");
		lsr_write_update_value(lsr, (SVG_Element *)com->node, DOM_String_datatype, field->fieldIndex, 0, &t->textContent, (field->pos>=0) ? GF_TRUE : GF_FALSE);
	} else {
		GF_LSR_WRITE_INT(lsr, 0, 1, "has_value");
	}
	lsr_write_any_attribute(lsr, NULL, GF_TRUE);
	/*if not add*/
	if (type!=LSR_UPDATE_ADD) {
		if (field && field->node_list && !com->fromNodeID) {
			GF_ChildNodeItem *l = field->node_list;
			u32 count = gf_node_list_get_count(l);
			GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");

			if (type==LSR_UPDATE_REPLACE) lsr_write_vluimsbf5(lsr, count, "count");

			while (l) {
				lsr_write_update_content_model(lsr, (SVG_Element*) com->node, l->node);
				l = l->next;
				if (type==LSR_UPDATE_INSERT) break;
			}
		} else if (field && field->new_node && !is_text_node) {
			GF_LSR_WRITE_INT(lsr, 1, 1, "opt_group");
			if (type==LSR_UPDATE_REPLACE) lsr_write_vluimsbf5(lsr, 1, "count");
			lsr_write_update_content_model(lsr, (SVG_Element*) com->node, field->new_node);
		} else {
			GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
		}
	}
	return GF_OK;
}

static GF_Err lsr_write_command_list(GF_LASeRCodec *lsr, GF_List *com_list, SVG_Element *cond, Bool first_implicit)
{
	GF_CommandField *field;
	u32 i, count;
	u32 detail;
	GF_BitStream *old_bs;

	count = com_list ? gf_list_count(com_list) : 0;

	old_bs = NULL;
	if (cond) {
		/*use temp bitstream for encoding conditional*/
		old_bs = lsr->bs;
		lsr->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}
	assert(count>= (u32) (first_implicit ? 1 : 0) );

	lsr_write_vluimsbf5(lsr, count-first_implicit, "occ0");

	if (!com_list) goto exit;

	count = gf_list_count(com_list);
	for (i=0; i<count; i++) {
		GF_Command *com = (GF_Command *)gf_list_get(com_list, i);
		switch (com->tag) {
		case GF_SG_LSR_NEW_SCENE:
		case GF_SG_LSR_REFRESH_SCENE:
			GF_LSR_WRITE_INT(lsr, (com->tag==GF_SG_LSR_REFRESH_SCENE) ? LSR_UPDATE_REFRESH_SCENE : LSR_UPDATE_NEW_SCENE, 4, "ch4");
			if (com->tag==GF_SG_LSR_REFRESH_SCENE) lsr_write_vluimsbf5(lsr, /*refresh scene time*/0, "time");
			lsr_write_any_attribute(lsr, NULL, 1);
			lsr_write_svg(lsr, (SVG_Element*)com->node);
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
			field = (GF_CommandField*)gf_list_get(com->command_fields, 0);
			if (field && field->fieldType) {
				u8 attType;
				attType = gf_lsr_anim_type_from_attribute(field->fieldIndex);
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_attributeName");
				GF_LSR_WRITE_INT(lsr, 0, 1, "choice");
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
			lsr_write_any_attribute(lsr, NULL, 1);
			break;
		case GF_SG_LSR_RESTORE:
			return GF_NOT_SUPPORTED;
		case GF_SG_LSR_SAVE:
			return GF_NOT_SUPPORTED;
		case GF_SG_LSR_SEND_EVENT:
			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_SEND_EVENT, 4, "ch4");

			detail = com->send_event_integer;
			switch (com->send_event_name) {
			case GF_EVENT_KEYDOWN:
			case GF_EVENT_LONGKEYPRESS:
			case GF_EVENT_REPEAT_KEY:
			case GF_EVENT_SHORT_ACCESSKEY:
				detail = dom_to_lsr_key(detail);
				if (detail==100) detail = 0;
				break;
			}

			/*FIXME in the spec, way too obscur*/
			lsr_write_event_type(lsr, com->send_event_name, com->send_event_integer);

			if (detail && com->send_event_integer) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_intvalue");
				GF_LSR_WRITE_INT(lsr, (com->send_event_integer<0) ? 1 : 0, 1, "sign");
				lsr_write_vluimsbf5(lsr, ABS(com->send_event_integer), "value");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "has_intvalue");
			}
			if (com->send_event_name<=GF_EVENT_MOUSEWHEEL) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_pointvalue");
				lsr_write_coordinate(lsr, INT2FIX(com->send_event_x), 0, "x");
				lsr_write_coordinate(lsr, INT2FIX(com->send_event_y), 0, "y");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "has_pointvalue");
			}
			lsr_write_codec_IDREF_Node(lsr, com->node, "ref");
			if (com->send_event_string) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_stringvalue");
				lsr_write_byte_align_string(lsr, com->send_event_string, "stringvalue");
			} else if (!detail && com->send_event_integer) {
				GF_LSR_WRITE_INT(lsr, 1, 1, "has_stringvalue");
				lsr_write_byte_align_string(lsr, (char *)gf_dom_get_key_name(com->send_event_integer), "stringvalue");
			} else {
				GF_LSR_WRITE_INT(lsr, 0, 1, "has_stringvalue");
			}
			GF_LSR_WRITE_INT(lsr, 0, 1, "has_attr_any");
			break;

		case GF_SG_LSR_ACTIVATE:
		case GF_SG_LSR_DEACTIVATE:
		{
			u32 update_size = lsr_get_IDREF_nb_bits(lsr, com->node);

			GF_LSR_WRITE_INT(lsr, LSR_UPDATE_EXTEND, 4, "ch4");
			GF_LSR_WRITE_INT(lsr, 2, lsr->info->cfg.extensionIDBits, "extensionID");
			lsr_write_vluimsbf5(lsr, 10 + 5 /*occ2*/ + 2 /*reserved*/ + 5 /*occ3*/ + 2 /*ch5*/ + update_size, "len");
			lsr_write_vluimsbf5(lsr, 87, "reserved");
			lsr_write_vluimsbf5(lsr, 1, "occ2");
			GF_LSR_WRITE_INT(lsr, 1, 2, "reserved");
			lsr_write_vluimsbf5(lsr, 1, "occ3");
			GF_LSR_WRITE_INT(lsr, (com->tag==GF_SG_LSR_ACTIVATE) ? 1 : 2, 2, "ch5");
			lsr_write_codec_IDREF_Node(lsr, com->node, "ref");
		}
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
		if (lsr->last_error) break;
	}

exit:
	/*script is aligned*/
	if (cond)	{
		u8 *data;
		u32 data_size;
		gf_bs_get_content(lsr->bs, &data, &data_size);
		gf_bs_del(lsr->bs);
		lsr->bs = old_bs;
		lsr_write_vluimsbf5(lsr, data_size, NULL/*"encoding-length" - don't log to avoid corrupting the log order!!*/);
		/*script is aligned*/
		gf_bs_align(lsr->bs);
		gf_bs_write_data(lsr->bs, data, data_size);
		gf_free(data);
	}
	return lsr->last_error;
}

static void lsr_add_color(GF_LASeRCodec *lsr, SVG_Color *color)
{
	lsr->col_table = (LSRCol*)gf_realloc(lsr->col_table, sizeof(LSRCol)*(lsr->nb_cols+1));
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
			char *ff = (char *)gf_list_get(lsr->font_table, i);
			if (!strcmp(ff, font->value)) {
				found = 1;
				break;
			}
		}
		if (!found) gf_list_add(lsr->font_table, gf_strdup(font->value));
	}
}

static void lsr_check_font_and_color(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	GF_ChildNodeItem *l;
	SVGAttribute *att;
	u32 i, count, tag;
	u32 check_anim_font, check_anim_col;

	tag = gf_node_get_tag((GF_Node*)elt);
	if (tag < GF_NODE_FIRST_DOM_NODE_TAG) goto check_children;

	check_anim_font = check_anim_col = 0;
	att = elt->attributes;
	while (att) {
		switch (att->data_type) {
		case SVG_Paint_datatype:
			lsr_check_col_index(lsr, NULL, att->data);
			break;
		case SVG_FontFamily_datatype:
			lsr_check_font_index(lsr, att->data);
			break;
		case SMIL_AttributeName_datatype:
		{
			SMIL_AttributeName *an = (SMIL_AttributeName*)att->data;
			if (an->name) {
				if (!strcmp(an->name, "fill")) check_anim_col = 1; /*we're sure this is not SMIL fill, it is not animatable*/
				else if (!strcmp(an->name, "stroke")) check_anim_col = 1;
				else if (!strcmp(an->name, "color")) check_anim_col = 1;
				else if (!strcmp(an->name, "solid-color")) check_anim_col = 2;
				else if (!strcmp(an->name, "stop-color")) check_anim_col = 2;
				else if (!strcmp(an->name, "font-family")) check_anim_font = 1;
			}
		}
		break;
		}
		att = att->next;
	}

	if (check_anim_font || check_anim_col) {
		att = elt->attributes;
		while (att) {
			switch (att->data_type) {
			case SMIL_AnimateValue_datatype:
				if (check_anim_font) lsr_check_font_index(lsr, ((SMIL_AnimateValue*)att->data)->value);
				else if (check_anim_col == 1) lsr_check_col_index(lsr, NULL, ((SMIL_AnimateValue*)att->data)->value);
				else if (check_anim_col == 2) lsr_check_col_index(lsr, ((SMIL_AnimateValue*)att->data)->value, NULL);
				break;
			case SMIL_AnimateValues_datatype:
			{
				SMIL_AnimateValues *vals = (SMIL_AnimateValues*)att->data;
				count = gf_list_count(vals->values);
				for (i=0; i<count; i++) {
					if (check_anim_font) lsr_check_font_index(lsr, (SVG_FontFamily*)gf_list_get(vals->values, i));
					else if (check_anim_col == 1) lsr_check_col_index(lsr, NULL, (SVG_Paint*)gf_list_get(vals->values, i) );
					else if (check_anim_col == 2) lsr_check_col_index(lsr, (SVG_Color*)gf_list_get(vals->values, i), NULL);
				}
			}
			break;
			}
			att = att->next;
		}
	}

check_children:
	l = elt->children;
	while (l) {
		if (l->node->sgprivate->tag==TAG_DOMUpdates) {
			GF_DOMUpdates *up = (GF_DOMUpdates *)l->node;
			count = gf_list_count(up->updates);
			for (i=0; i<count; i++) {
				u32 j, c2;
				GF_Command *com = gf_list_get(up->updates, i);
				c2 = gf_list_count(com->command_fields);
				for (j=0; j<c2; j++) {
					GF_CommandField *field = (GF_CommandField *)gf_list_get(com->command_fields, j);
					if (field->new_node) {
						lsr_check_font_and_color(lsr, (SVG_Element*)field->new_node);
					}
					/*replace/insert*/
					else if (field->field_ptr) {
						switch (field->fieldType) {
						case SVG_Paint_datatype:
							lsr_check_col_index(lsr, NULL, (SVG_Paint*)field->field_ptr);
							break;
						case SVG_Color_datatype:
							lsr_check_col_index(lsr, (SVG_Color*)field->field_ptr, NULL);
							break;
						case SVG_FontFamily_datatype:
							lsr_check_font_index(lsr, (SVG_FontFamily*)field->field_ptr);
							break;
						}
					}
				}
			}
		} else {
			lsr_check_font_and_color(lsr, (SVG_Element*)l->node);
		}
		l = l->next;
	}
}

static GF_Err lsr_write_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list, Bool reset_encoding_context)
{
	u32 i, count, prev_col_count, prev_font_count;

	/*
	 *	1 - laser unit header
	 */
	if (!com_list) {
		if (gf_sg_get_root_node(lsr->sg) == NULL) return GF_BAD_PARAM;
		/*RAP generation, force reset encoding context*/
		GF_LSR_WRITE_INT(lsr, 1, 1, "resetEncodingContext");
	} else {
		GF_LSR_WRITE_INT(lsr, reset_encoding_context ? 1 : 0, 1, "resetEncodingContext");
	}
	GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
#if 0
	lsr_write_extension(lsr, NULL, 0, "ext");
#endif

	/*clean all tables*/
	if (reset_encoding_context) {
		lsr->nb_cols = 0;
		if (lsr->col_table) gf_free(lsr->col_table);
		lsr->col_table = NULL;
		while (gf_list_count(lsr->font_table)) {
			char *ft = (char *)gf_list_last(lsr->font_table);
			gf_free(ft);
			gf_list_rem_last(lsr->font_table);
		}
	}

	/*
	 *	2 - codecInitialisations
	 */

	prev_col_count = lsr->nb_cols;
	prev_font_count = gf_list_count(lsr->font_table);
	/*RAP generation, send all fonts and colors*/
	if (!com_list) {
		prev_col_count = prev_font_count = 0;
		lsr_check_font_and_color(lsr, (SVG_Element *)gf_sg_get_root_node(lsr->sg));
	} else {
		/*process all colors and fonts*/
		count = gf_list_count(com_list);
		for (i=0; i<count; i++) {
			GF_Command *com = (GF_Command *)gf_list_get(com_list, i);
			if (gf_list_count(com->command_fields)) {
				GF_CommandField *field = (GF_CommandField *)gf_list_get(com->command_fields, 0);
				if (field->fieldType==SVG_Paint_datatype) lsr_check_col_index(lsr, NULL, (SVG_Paint*)field->field_ptr);
				else if (field->fieldType==SVG_FontFamily_datatype) lsr_check_font_index(lsr, (SVG_FontFamily*)field->field_ptr);
				else if (field->new_node) lsr_check_font_and_color(lsr, (SVG_Element*)field->new_node);
				else if (field->node_list) {
					GF_ChildNodeItem *l = field->node_list;
					while (l) {
						lsr_check_font_and_color(lsr, (SVG_Element*)l->node);
						l = l->next;
					}
				}
			} else if (com->node && (com->tag!=GF_SG_LSR_DELETE) ) {
				lsr_check_font_and_color(lsr, (SVG_Element *)com->node);
			}
		}
	}
	/*
	 * 2.a - condecInitialization.color
	 */
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
	/*
	 * 2.b - condecInitialization.fonts
	 */
	count = gf_list_count(lsr->font_table);
	if (prev_font_count == count) {
		GF_LSR_WRITE_INT(lsr, 0, 1, "fontInitialisation");
	} else {
		GF_LSR_WRITE_INT(lsr, 1, 1, "fontInitialisation");
		lsr_write_vluimsbf5(lsr, count - prev_font_count, "count");
		for (i=prev_font_count; i<count; i++) {
			char *ft = (char *)gf_list_get(lsr->font_table, i);
			lsr_write_byte_align_string(lsr, ft, "font");
		}
	}
	lsr->fontIndexBits = gf_get_bit_size(count);
	/*
	 * 2.c - condecInitialization.private
	 */
	GF_LSR_WRITE_INT(lsr, 0, 1, "privateDataIdentifierInitialisation");
	/*
	 * 2.d - condecInitialization.anyXML
	 */
	GF_LSR_WRITE_INT(lsr, 0, 1, "anyXMLInitialisation");
	/*
	 * 2.e - condecInitialization.extended
	 */
	/*FIXME - node string IDs are currently not used*/
	lsr_write_vluimsbf5(lsr, 0, "countG");
	/*FIXME - global streams are currently not used*/
	GF_LSR_WRITE_INT(lsr, 0, 1, "hasExtension");

	/*RAP generation, encode NewScene with root node*/
	if (!com_list) {
		lsr_write_vluimsbf5(lsr, 0, "occ0");
		GF_LSR_WRITE_INT(lsr, 4, 4, "ch4");
		lsr_write_any_attribute(lsr, NULL, 1);
		lsr_write_svg(lsr, (SVG_Element *)gf_sg_get_root_node(lsr->sg) );
	} else {
		GF_Err e = lsr_write_command_list(lsr, com_list, NULL, 1);
		if (e) return e;
	}
	GF_LSR_WRITE_INT(lsr, 0, 1, "opt_group");
#if 0
	lsr_write_extension(lsr, NULL, 0, "ext");
#endif
	return GF_OK;
}

#endif /*GPAC_DISABLE_LASER*/
