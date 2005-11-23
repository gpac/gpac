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

#define GF_LSR_READ_INT(_codec, _val, _nbBits, _str)	{\
	_val = gf_bs_read_int(_codec->bs, _nbBits);	\
	}\


static void lsr_read_group_content(GF_LASeRCodec *lsr, SVGElement *elt);
static GF_Err lsr_read_command_list(GF_LASeRCodec *lsr, GF_List *comList, SVGscriptElement *script);
static GF_Err lsr_decode_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list);

GF_LASeRCodec *gf_laser_decoder_new(GF_SceneGraph *graph)
{
	GF_LASeRCodec *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_LASeRCodec));
	if (!tmp) return NULL;
	tmp->streamInfo = gf_list_new();
	tmp->font_table = gf_list_new();
	tmp->sg = graph;
	return tmp;
}

void gf_laser_decoder_del(GF_LASeRCodec *codec)
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


void gf_laser_decoder_set_trace(GF_LASeRCodec *codec, FILE *trace)
{
	codec->trace = trace;
	if (trace) fprintf(codec->trace, "Name\t\tNbBits\t\tValue\t\t//comment\n\n");
}

static LASeRStreamInfo *lsr_get_stream(GF_LASeRCodec *codec, u16 ESID)
{
	u32 i;
	for (i=0;i<gf_list_count(codec->streamInfo);i++) {
		LASeRStreamInfo *ptr = gf_list_get(codec->streamInfo, i);
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}


GF_Err gf_laser_decoder_configure_stream(GF_LASeRCodec *codec, u16 ESID, char *dsi, u32 dsi_len)
{
	LASeRStreamInfo *info;
	GF_BitStream *bs;
	if (lsr_get_stream(codec, ESID) != NULL) return GF_BAD_PARAM;
	GF_SAFEALLOC(info, sizeof(LASeRStreamInfo));
	info->ESID = ESID;
	bs = gf_bs_new(dsi, dsi_len, GF_BITSTREAM_READ);

	info->cfg.profile = gf_bs_read_int(bs, 8);
	info->cfg.level = gf_bs_read_int(bs, 8);
	info->cfg.encoding = gf_bs_read_int(bs, 2);
	info->cfg.pointsCodec = gf_bs_read_int(bs, 2);
	info->cfg.pathComponents = gf_bs_read_int(bs, 8);
	info->cfg.fullRequestHost = gf_bs_read_int(bs, 1);
	if (gf_bs_read_int(bs, 1)) {
		info->cfg.time_resolution = gf_bs_read_int(bs, 16);
	} else {
		info->cfg.time_resolution = 1000;
	}
	info->cfg.colorComponentBits = gf_bs_read_int(bs, 4);
	info->cfg.colorComponentBits += 1;
	info->cfg.resolution = gf_bs_read_int(bs, 4);
	info->cfg.scale_bits = gf_bs_read_int(bs, 4);
	info->cfg.coord_bits = gf_bs_read_int(bs, 5);
	info->cfg.append = gf_bs_read_int(bs, 1);
	info->cfg.has_string_ids = gf_bs_read_int(bs, 1);
	info->cfg.has_private_data = gf_bs_read_int(bs, 1);
	info->cfg.hasExtendedAttributes = gf_bs_read_int(bs, 1);
	info->cfg.extensionIDBits = gf_bs_read_int(bs, 4);
	gf_list_add(codec->streamInfo, info);
	return GF_OK;
}

GF_Err gf_laser_decoder_remove_stream(GF_LASeRCodec *codec, u16 ESID)
{
	u32 i;
	for (i=0;i<gf_list_count(codec->streamInfo);i++) {
		LASeRStreamInfo *ptr = (LASeRStreamInfo *) gf_list_get(codec->streamInfo, i);
		if (ptr->ESID==ESID) {
			free(ptr);
			gf_list_rem(codec->streamInfo, i);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}



GF_Err gf_laser_decode_au(GF_LASeRCodec *codec, u16 ESID, char *data, u32 data_len)
{
	GF_Err e;
	if (!codec || !data || !data_len) return GF_BAD_PARAM;

	codec->info = lsr_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits + codec->coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution>=0)
		codec->res_factor = INT2FIX(1<<codec->info->cfg.resolution);
	else 
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1 << (-codec->info->cfg.resolution)) );


	codec->bs = gf_bs_new(data, data_len, GF_BITSTREAM_READ);
	e = lsr_decode_laser_unit(codec, NULL);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}

GF_Err gf_laser_decode_command_list(GF_LASeRCodec *codec, u16 ESID, char *data, u32 data_len, GF_List *com_list)
{
	GF_Err e;
	if (!codec || !data || !data_len) return GF_BAD_PARAM;

	codec->info = lsr_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits + codec->coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution>=0)
		codec->res_factor = INT2FIX(1<<codec->info->cfg.resolution);
	else 
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1 << (-codec->info->cfg.resolution)) );

	codec->bs = gf_bs_new(data, data_len, GF_BITSTREAM_READ);
	e = lsr_decode_laser_unit(codec, com_list);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}

void gf_laser_decoder_set_clock(GF_LASeRCodec *codec, Double (*GetSceneTime)(void *st_cbk), void *st_cbk )
{
	codec->GetSceneTime = GetSceneTime;
	codec->cbk = st_cbk;
}


static void lsr_dec_log_bits(GF_LASeRCodec *lsr, u32 val, u32 nb_bits, const char *name)
{
}

static u32 lsr_read_vluimsbf5(GF_LASeRCodec *lsr, const char *name)
{
	u32 nb_words = 0;
	u32 nb_tot, nb_bits, val;

	while (gf_bs_read_int(lsr->bs, 1)) nb_words++;
	nb_words++;
	nb_tot = nb_words;
	nb_bits = nb_words*4;
	nb_tot += nb_bits;
	val = gf_bs_read_int(lsr->bs, nb_bits);
	lsr_dec_log_bits(lsr, val, nb_tot, name);
	return val;
}

static void lsr_read_extension_class(GF_LASeRCodec *lsr, char **out_data, u32 *out_len, const char *name)
{
	u32 len = lsr_read_vluimsbf5(lsr, name);
	*out_data = malloc(sizeof(char)*len);
	gf_bs_read_data(lsr->bs, *out_data, len);
	*out_len = len;
}

static void lsr_read_extend_class(GF_LASeRCodec *lsr, char **out_data, u32 *out_len, const char *name)
{
	u32 len;
	GF_LSR_READ_INT(lsr, len, lsr->info->cfg.extensionIDBits, "reserved");
	len = lsr_read_vluimsbf5(lsr, "byteLength");
	*out_data = malloc(sizeof(char)*len);
	gf_bs_read_data(lsr->bs, *out_data, len);
	*out_len = len;
}

static void lsr_read_codec_IDREF(GF_LASeRCodec *lsr, SVG_IRI *href, const char *name)
{
	GF_Node *n;
	u32 nID = lsr_read_vluimsbf5(lsr, name);
	n = gf_sg_find_node(lsr->sg, nID);
	if (!n) {
		fprintf(stdout, "ERROR: undefined node\n");
		return;
	}
	href->target = (SVGElement *)n;
}
static void lsr_read_codec_IDREF_URI(GF_LASeRCodec *lsr, unsigned char **out_uri, const char *name)
{
	char szN[100];
	GF_Node *n;
	u32 nID = lsr_read_vluimsbf5(lsr, name);
	n = gf_sg_find_node(lsr->sg, nID);
	if (!n) {
		fprintf(stdout, "ERROR: undefined node\n");
		return;
	}
	if (lsr->info->cfg.has_string_ids) {
		sprintf(szN, "#%s", gf_node_get_name(n));
	} else {
		sprintf(szN, "#%d", nID);
	}
	if (*out_uri) free(*out_uri);
	*out_uri = strdup(szN);
}

static void lsr_read_vl5string(GF_LASeRCodec *lsr, char **out_string, const char *name)
{
	u32 len = lsr_read_vluimsbf5(lsr, name);
	if (*out_string) free(*out_string);
	*out_string = malloc(sizeof(char)*(len+1));
	gf_bs_read_data(lsr->bs, *out_string, len);
	*out_string[len] = 0;
}

static Fixed lsr_read_fixed_16_8(GF_LASeRCodec *lsr, const char *name)
{
	s32 val;
	GF_LSR_READ_INT(lsr, val, 24, name);
#ifdef GPAC_FIXED_POINT
	return val << 8;
#else
	return ((Float) val) / (1<<8);
#endif
}

static const char *lsr_get_font(GF_LASeRCodec *lsr, u32 idx)
{
	return gf_list_get(lsr->font_table, idx);
}

static void lsr_get_color(GF_LASeRCodec *lsr, u32 idx, SVG_Color *color)
{
	LSRCol *c;
	if (idx>=lsr->nb_cols) return;

	c = &lsr->col_table[idx];
	color->red = INT2FIX(c->r) / lsr->color_scale;
	color->green = INT2FIX(c->g) / lsr->color_scale;
	color->blue = INT2FIX(c->b) / lsr->color_scale;
	color->type = SVG_COLOR_RGBCOLOR;
}

static void lsr_read_color(GF_LASeRCodec *lsr, SVG_Color *color, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "hasIndex");
	if (val) {
		GF_LSR_READ_INT(lsr, val, lsr->colorIndexBits, name);
		lsr_get_color(lsr, val, color);
	} else {
		GF_LSR_READ_INT(lsr, val, 1, "isEnum");
		if (val) {
			GF_LSR_READ_INT(lsr, val, 2, "type");
			if (val==2) color->type = /*SVG_COLOR_NONE*/SVG_COLOR_INHERIT;
			else if (val==1) color->type = SVG_COLOR_CURRENTCOLOR;
			else color->type = SVG_COLOR_INHERIT;
		} else {
			/*TODO*/
			char *unsup;
			lsr_read_vl5string(lsr, &unsup, "colorExType0");
			if (unsup) free(unsup);
		}
	}
}

static void lsr_read_color_class(GF_LASeRCodec *lsr, SVG_Paint *paint, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 2, "choice");
	if (val==3) {
		GF_LSR_READ_INT(lsr, val, 2, "type");
		if (val==0) { paint->type = SVG_PAINT_COLOR; paint->color->type = SVG_COLOR_CURRENTCOLOR; }
		else if (val==2) paint->type = SVG_PAINT_NONE;
		else paint->type = SVG_PAINT_INHERIT;
	}
	else if (val==0) {
		paint->type=SVG_PAINT_COLOR;
		lsr_read_color(lsr, paint->color, name);
	} else if (val==2) {
		paint->type=SVG_PAINT_URI;
		lsr_read_codec_IDREF_URI(lsr, &paint->uri, "uri");
	} else {
		char *unsup;
		lsr_read_vl5string(lsr, &unsup, "colorExType0");
		if (unsup) free(unsup);
	}
}

static void lsr_read_line_increment_type(GF_LASeRCodec *lsr, SVG_LineIncrement *li, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val==1) {
		GF_LSR_READ_INT(lsr, val, 1, "type");
		if (val==1) li->type=SVG_NUMBER_INHERIT;
		else li->type=SVG_NUMBER_AUTO;
	} else {
		li->value = lsr_read_fixed_16_8(lsr, "line-increment-value");
	}
}

static void lsr_read_byte_align_string(GF_LASeRCodec *lsr, unsigned char **str, const char *name)
{
	u32 len;
	gf_bs_align(lsr->bs);
	len = gf_bs_read_u8(lsr->bs);
	if (*str) free(*str);
	*str = malloc(sizeof(char)*(len+1));
	gf_bs_read_data(lsr->bs, *str, len);
	(*str) [len] = 0;
}
static void lsr_read_byte_align_string_list(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	unsigned char *text, *sep, *sep2, *cur;
	while (gf_list_count(l)) {
		char *str = gf_list_last(l);
		gf_list_rem_last(l);
		free(str);
	}
	lsr_read_byte_align_string(lsr, &text, name);
	cur = text;
	while (1) {
		sep = strchr(cur, '\'');
		if (!sep) {
			gf_list_add(l, strdup(cur));
			break;
		}
		sep2 = strchr(sep + 1, '\'');
		if (!sep2) {
			gf_list_add(l, strdup(cur));
			break;
		}
		sep2[0] = 0;
		gf_list_add(l, strdup(sep+1));
		sep2[0] = '\'';
		cur = sep2 + 1;
	}
	free(text);
}

static void lsr_read_any_uri(GF_LASeRCodec *lsr, SVG_IRI *iri, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "hasUri");
	if (val) {
		unsigned char *s;
		iri->type=SVG_IRI_IRI;
		lsr_read_byte_align_string(lsr, &s, "uri");
		GF_LSR_READ_INT(lsr, val, 1, "hasData");
		if (!val) {
			iri->iri = s;
		} else {
			unsigned char *data;
			u32 len;
			lsr_read_byte_align_string(lsr, &data, "uri");
			len = strlen(data) + 1 + strlen(s) + 1;
			iri->iri = malloc(sizeof(char) * len);
			strcpy(iri->iri, s);
			strcat(iri->iri, ",");
			strcat(iri->iri, data);
			iri->iri[len] = 0;
			free(s);
			free(data);
		}
    }
	GF_LSR_READ_INT(lsr, val, 1, "hasID");
	if (val) {
		iri->type=SVG_IRI_ELEMENTID;
		lsr_read_codec_IDREF(lsr, iri, "idref");
	}
	GF_LSR_READ_INT(lsr, val, 1, "hasStreamID");
	if (val) {
		u32 streamID = lsr_read_vluimsbf5(lsr, name);
	}
}

static void lsr_read_private_att_class(GF_LASeRCodec *lsr)
{
	/*NO PRIVATE DATA ON ENCODING YET*/
	assert(0);
}

static void lsr_read_private_attr_container(GF_LASeRCodec *lsr, u32 index, const char *name)
{
	assert(0);
}

static void lsr_read_any_attribute(GF_LASeRCodec *lsr, GF_Node *node)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "has_attrs");
	if (val) {
		do {
			GF_LSR_READ_INT(lsr, val, lsr->info->cfg.extensionIDBits, "reserved");
		    val = lsr_read_vluimsbf5(lsr, "len");//len in BITS
			GF_LSR_READ_INT(lsr, val, val, "reserved_val");
			GF_LSR_READ_INT(lsr, val, 1, "hasNextExtension");
		} while (val);
	}
}

static void lsr_read_object_content(GF_LASeRCodec *lsr, SVGElement *elt)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "opt_group");
	if (val) lsr_read_private_att_class(lsr);
}
static void lsr_read_string_attribute(GF_LASeRCodec *lsr, unsigned char **class_attr, char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (val) lsr_read_byte_align_string(lsr, class_attr, name);
}
static void lsr_read_id(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 val, id;
	unsigned char *name;
	GF_LSR_READ_INT(lsr, val, 1, "has__0_id");
	if (!val) return;
	
	name = NULL;
    id = lsr_read_vluimsbf5(lsr, "ID");
    if (lsr->info->cfg.has_string_ids) lsr_read_byte_align_string(lsr, &name, "stringId");
	gf_node_set_id(n, id, name);

	GF_LSR_READ_INT(lsr, val, 1, "reserved");
#if TODO_LASER_EXTENSIONS	
	if (val) {
		u32 len = lsr_read_vluimsbf5(lsr, "len");
		GF_LSR_READ_INT(lsr, val, len, "reserved");
	}
#endif
}

/*start of RARE properties*/
#define RARE_AUDIO_LEVEL		1
#define RARE_COLOR				2
#define RARE_COLOR_RENDERING	3
#define RARE_DISPLAY			4
#define RARE_DISPLAY_ALIGN		5
#define RARE_FILL_OPACITY		6
#define RARE_FILL_RULE			7
#define RARE_IMAGE_RENDERING	8
#define RARE_POINTER_EVENTS		10
#define RARE_SHAPE_RENDERING	11
#define RARE_SOLID_COLOR		12
#define RARE_SOLID_OPACITY		13
#define RARE_STOP_COLOR			14
#define RARE_STOP_OPACITY		15
#define RARE_STROKE_DASHARRAY	16
#define RARE_STROKE_DASHOFFSET	17
#define RARE_STROKE_LINECAP		18
#define RARE_STROKE_LINEJOIN	19
#define RARE_STROKE_MITERLIMIT	20
#define RARE_STROKE_OPACITY		21
#define RARE_STROKE_WIDTH		22
#define RARE_TEXT_ANCHOR		23
#define RARE_TEXT_RENDERING		24
#define RARE_VIEWPORT_FILL		25
#define RARE_VIEWPORT_FILL_OPACITY	26
#define RARE_VECTOR_EFFECT		27
#define RARE_VISIBILITY			28
#define RARE_FONT_FAMILY		51
#define RARE_FONT_SIZE			52
#define RARE_FONT_STYLE			53
#define RARE_FONT_WEIGHT		54
/*end of RARE properties*/
/*conditional processing*/
#define RARE_REQUIREDEXTENSIONS	29
#define RARE_REQUIREDFEATURES	30
#define RARE_REQUIREDFORMATS	31
#define RARE_SYSTEMLANGUAGE		32
/*XML*/
#define RARE_XML_BASE			33
#define RARE_XML_LANG			34
#define RARE_XML_SPACE			35
/*focus*/
#define RARE_FOCUSNEXT			36
#define RARE_FOCUSNORTH			37
#define RARE_FOCUSNORTHEAST		38
#define RARE_FOCUSNORTHWEST		39
#define RARE_FOCUSPREV			40
#define RARE_FOCUSSOUTH			41
#define RARE_FOCUSSOUTHEAST		42
#define RARE_FOCUSSOUTHWEST		43
#define RARE_FOCUSWEST			44
#define RARE_FOCUSABLE			45
#define RARE_FOCUSEAST			46
/*href*/
#define RARE_HREF_TITLE			55
#define RARE_HREF_TYPE			56
#define RARE_HREF_ROLE			57
#define RARE_HREF_ARCROLE		58
#define RARE_HREF_ACTUATE		59
#define RARE_HREF_SHOW			60
/*timing*/
#define RARE_END				61
#define RARE_MAX				62
#define RARE_MIN				63

#define RARE_TRANSFORM			47
#define RARE_LSR_ROTATION		48
#define RARE_LSR_SCALE			49
#define RARE_LSR_TRANSLATION	50


static u32 lsr_get_rare_props_idx(SVGElement *n, GF_FieldInfo *info)
{
	if (n->properties.audio_level == info->far_ptr) return RARE_AUDIO_LEVEL;
	else if (n->properties.color == info->far_ptr) return RARE_COLOR;
	else if (n->properties.color_rendering == info->far_ptr) return RARE_COLOR_RENDERING;
	else if (n->properties.display == info->far_ptr) return RARE_DISPLAY;
	else if (n->properties.display_align == info->far_ptr) return RARE_DISPLAY_ALIGN;
	else if (n->properties.fill_opacity == info->far_ptr) return RARE_FILL_OPACITY;
	else if (n->properties.fill_rule == info->far_ptr) return RARE_FILL_RULE;
	else if (n->properties.image_rendering == info->far_ptr) return RARE_IMAGE_RENDERING;
	else if (n->properties.pointer_events == info->far_ptr) return RARE_POINTER_EVENTS;
	else if (n->properties.shape_rendering == info->far_ptr) return RARE_SHAPE_RENDERING;
	else if (n->properties.solid_color == info->far_ptr) return RARE_SOLID_COLOR;
	else if (n->properties.solid_opacity == info->far_ptr) return RARE_SOLID_OPACITY;
	else if (n->properties.stop_color == info->far_ptr) return RARE_STOP_COLOR;
	else if (n->properties.stop_opacity == info->far_ptr) return RARE_STOP_OPACITY;
	else if (n->properties.stroke_dasharray == info->far_ptr) return RARE_STROKE_DASHARRAY;
	else if (n->properties.stroke_dashoffset == info->far_ptr) return RARE_STROKE_DASHOFFSET;
	else if (n->properties.stroke_linecap == info->far_ptr) return RARE_STROKE_LINECAP;
	else if (n->properties.stroke_linejoin == info->far_ptr) return RARE_STROKE_LINEJOIN;
	else if (n->properties.stroke_miterlimit == info->far_ptr) return RARE_STROKE_MITERLIMIT;
	else if (n->properties.stroke_opacity == info->far_ptr) return RARE_STROKE_OPACITY;
	else if (n->properties.stroke_width == info->far_ptr) return RARE_STROKE_WIDTH;
	else if (n->properties.text_anchor == info->far_ptr) return RARE_TEXT_ANCHOR;
	else if (n->properties.text_rendering == info->far_ptr) return RARE_TEXT_RENDERING;
	else if (n->properties.viewport_fill == info->far_ptr) return RARE_VIEWPORT_FILL;
	else if (n->properties.viewport_fill_opacity == info->far_ptr) return RARE_VIEWPORT_FILL_OPACITY;
	else if (n->properties.vector_effect == info->far_ptr) return RARE_VECTOR_EFFECT;
	else if (n->properties.visibility == info->far_ptr) return RARE_VISIBILITY;
	else if (n->properties.font_family == info->far_ptr) return RARE_FONT_FAMILY;
	else if (n->properties.font_size == info->far_ptr) return RARE_FONT_SIZE;
	else if (n->properties.font_style == info->far_ptr) return RARE_FONT_STYLE;
	else if (n->properties.font_weight == info->far_ptr) return RARE_FONT_WEIGHT;

	return 0;
}

static Fixed lsr_read_fixed_clamp(GF_LASeRCodec *lsr, const char *name)
{
	s32 val;
	GF_LSR_READ_INT(lsr, val, 8, name);
	return INT2FIX(val) / 255;
}

static void lsr_read_focus(GF_LASeRCodec *lsr, void *foc, const char *name)
{
	fprintf(stdout, "ERROR: FOCUS NOT IMPLEMENTED\n");
}

static void lsr_read_rare(GF_LASeRCodec *lsr, SVGElement *n)
{
	u32 i, nb_rare, field_rare;
	GF_LSR_READ_INT(lsr, nb_rare, 1, "has__0_rare");
	if (!nb_rare) return;
	GF_LSR_READ_INT(lsr, nb_rare, 6, "nbOfAttributes");

	for (i=0; i<nb_rare; i++) {
		GF_LSR_READ_INT(lsr, field_rare, 6, "attributeRARE");
		switch (field_rare) {
		/*properties*/

		/*TODO !!!! what about inherit types??*/
		case RARE_AUDIO_LEVEL: n->properties.audio_level->value = lsr_read_fixed_clamp(lsr, "audio-level"); break;
		case RARE_FILL_OPACITY: n->properties.fill_opacity->value = lsr_read_fixed_clamp(lsr, "fill-opacity"); break;
		case RARE_SOLID_OPACITY: n->properties.solid_opacity->value = lsr_read_fixed_clamp(lsr, "solid-opacity"); break;
		case RARE_STOP_OPACITY: n->properties.stop_opacity->value = lsr_read_fixed_clamp(lsr, "stop-opacity"); break;
		case RARE_STROKE_OPACITY: n->properties.stroke_opacity->value = lsr_read_fixed_clamp(lsr, "stroke-opacity"); break;
		case RARE_VIEWPORT_FILL_OPACITY: n->properties.viewport_fill_opacity->value = lsr_read_fixed_clamp(lsr, "viewport-fill-opacity"); break;

	    case RARE_COLOR: lsr_read_color(lsr, n->properties.color, "color"); break;
	    case RARE_SOLID_COLOR: lsr_read_color_class(lsr, n->properties.solid_color, "solid-color"); break;
	    case RARE_STOP_COLOR: lsr_read_color_class(lsr, n->properties.stop_color, "stop-color"); break;
	    case RARE_VIEWPORT_FILL: lsr_read_color_class(lsr, n->properties.viewport_fill, "viewport-fill"); break;

		case RARE_DISPLAY: GF_LSR_READ_INT(lsr, *n->properties.display, 5, "display"); break;
	    case RARE_DISPLAY_ALIGN: GF_LSR_READ_INT(lsr, *n->properties.display_align, 2, "display-align"); break;
	    case RARE_FILL_RULE: GF_LSR_READ_INT(lsr, *n->properties.fill_rule, 2, "fill-rule"); break;

		case RARE_COLOR_RENDERING: GF_LSR_READ_INT(lsr, *n->properties.color_rendering, 2, "color-rendering"); break;
		case RARE_IMAGE_RENDERING: GF_LSR_READ_INT(lsr, *n->properties.image_rendering, 2, "image-rendering"); break;
		case RARE_SHAPE_RENDERING: GF_LSR_READ_INT(lsr, *n->properties.shape_rendering, 3, "shape-rendering"); break;
		case RARE_TEXT_RENDERING: GF_LSR_READ_INT(lsr, *n->properties.text_rendering, 2, "text-rendering"); break;
		/*TODO FIXME - SVG values are not in sync with LASeR*/
		case RARE_STROKE_LINECAP: GF_LSR_READ_INT(lsr, *n->properties.stroke_linecap, 2, "stroke-linecap"); break;
		case RARE_STROKE_LINEJOIN: GF_LSR_READ_INT(lsr, *n->properties.stroke_linejoin, 2, "stroke-linejoin"); break;
		case RARE_TEXT_ANCHOR: GF_LSR_READ_INT(lsr, *n->properties.text_anchor, 2, "text-achor"); break;
		case RARE_VECTOR_EFFECT: GF_LSR_READ_INT(lsr, *n->properties.vector_effect, 4, "vector-effect"); break;
	    case RARE_POINTER_EVENTS: GF_LSR_READ_INT(lsr, *n->properties.pointer_events, 4, "pointer-events"); break;
	    case RARE_VISIBILITY: GF_LSR_READ_INT(lsr, *n->properties.visibility, 2, "visibility"); break;

		case RARE_FONT_FAMILY:
		{
			u32 flag;
			GF_LSR_READ_INT(lsr, flag, 1, "isInherit");
			if (n->properties.font_family->value) free(n->properties.font_family->value);
			n->properties.font_family->value = NULL;
			if (flag) {
				n->properties.font_family->type = SVG_FONTFAMILY_INHERIT;
			} else {
				char *ft;
				GF_LSR_READ_INT(lsr, flag, lsr->fontIndexBits, "fontIndex");
				ft = gf_list_get(lsr->font_table, flag);
				if (ft) n->properties.font_family->value = strdup(ft);
			}
		}
			break;
		case RARE_FONT_SIZE: n->properties.font_size->value = lsr_read_fixed_16_8(lsr, "fontSize"); break;
		/*TODO not specified in spec !!*/
		case RARE_FONT_STYLE: GF_LSR_READ_INT(lsr, *n->properties.font_style, 5, "fontStyle"); break;
		/*TODO not specified in spec !!*/
		case RARE_FONT_WEIGHT: GF_LSR_READ_INT(lsr, *n->properties.font_weight, 4, "fontWeight"); break;

		/*other stuff */
#if 0
		case RARE_REQUIREDEXTENSIONS: lsr_read_byte_align_string_list(lsr, , "requiredExtensions");
	    case RARE_REQUIREDFORMATS: lsr_read_byte_align_string_list(lsr, *(GF_List **)fi->far_ptr, "requiredFormats");
	    case RARE_REQUIREDFEATURES: lsr_read_byte_align_string_list(lsr, *(GF_List **)fi->far_ptr, "requiredFeatures");
	    case RARE_SYSTEMLANGUAGE: lsr_read_byte_align_string_list(lsr, *(GF_List **)fi->far_ptr, "systemLanguage");
	    case RARE_XML_BASE: lsr_read_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xml:base"); break;
	    case RARE_XML_LANG: lsr_read_byte_align_string(lsr, *(SVG_String *)fi->far_ptr, "xml:lang"); break;
	    case RARE_XML_SPACE: GF_LSR_READ_INT(lsr, *(XML_Space *)fi->far_ptr, 1, "xml:space"); break;
		/*focusable*/
		case RARE_FOCUSNEXT: lsr_read_focus(lsr, fi->far_ptr, "focusNext"); break;
		case RARE_FOCUSNORTH: lsr_read_focus(lsr, fi->far_ptr, "focusNorth"); break;
		case RARE_FOCUSNORTHEAST: lsr_read_focus(lsr, fi->far_ptr, "focusNorthEast"); break;
		case RARE_FOCUSNORTHWEST: lsr_read_focus(lsr, fi->far_ptr, "focusNorthWest"); break;
		case RARE_FOCUSPREV: lsr_read_focus(lsr, fi->far_ptr, "focusPrev"); break;
		case RARE_FOCUSSOUTH: lsr_read_focus(lsr, fi->far_ptr, "focusSouth"); break;
		case RARE_FOCUSSOUTHEAST: lsr_read_focus(lsr, fi->far_ptr, "focusSouthEast"); break;
		case RARE_FOCUSSOUTHWEST: lsr_read_focus(lsr, fi->far_ptr, "focusSouthWest"); break;
		case RARE_FOCUSWEST: lsr_read_focus(lsr, fi->far_ptr, "focusWest"); break;
		case RARE_FOCUSEAST: lsr_read_focus(lsr, fi->far_ptr, "focusEast"); break;
#endif
		}
	}
}

static void lsr_read_fill(GF_LASeRCodec *lsr, SVGElement *n)
{
	Bool has_fill;
	GF_LSR_READ_INT(lsr, has_fill, 1, "has__1_fill");
	if (has_fill) lsr_read_color_class(lsr, n->properties.fill, "_1_fill");
}

static void lsr_read_line_increment(GF_LASeRCodec *lsr, SVGElement *n)
{
	Bool has_line_inc;
	GF_LSR_READ_INT(lsr, has_line_inc, 1, "has__1_line-increment");
	if (has_line_inc) lsr_read_line_increment_type(lsr, n->properties.line_increment, "_1_line-increment");
}

static void lsr_read_stroke(GF_LASeRCodec *lsr, SVGElement *n)
{
	Bool has_stroke;
	GF_LSR_READ_INT(lsr, has_stroke, 1, "has__1_stroke");
	if (has_stroke) lsr_read_color_class(lsr, n->properties.stroke, "_1_stroke");
}
static void lsr_read_href(GF_LASeRCodec *lsr, SVG_IRI *iri)
{
	Bool has_href;
	GF_LSR_READ_INT(lsr, has_href, 1, "has_href");
	if (has_href) lsr_read_any_uri(lsr, iri, "href");
}

static void lsr_read_accumulate(GF_LASeRCodec *lsr, u8 *accum_type)
{
	Bool v;
	GF_LSR_READ_INT(lsr, v, 1, "has__4_accumulate");
	if (v) {
		GF_LSR_READ_INT(lsr, *accum_type, 1, "_4_accumulate");
	}
	else *accum_type = 0;
}
static void lsr_read_additive(GF_LASeRCodec *lsr, u8 *add_type)
{
	Bool v;
	GF_LSR_READ_INT(lsr, v, 1, "has__4_additive");
	if (v) {
		GF_LSR_READ_INT(lsr, *add_type, 1, "_4_additive");
	}
	else *add_type = 0;
}
static void lsr_read_calc_mode(GF_LASeRCodec *lsr, u8 *calc_mode)
{
	u32 v;
	/*SMIL_CALCMODE_LINEAR is default and 0 in our code*/
	GF_LSR_READ_INT(lsr, v, 1, "has__4_calcMode");
	if (v) {
		GF_LSR_READ_INT(lsr, v, 2, name);
		/*SMIL_CALCMODE_DISCRETE is 0 in LASeR, 1 in our code*/
		if (!v) *calc_mode = 1;
		else *calc_mode = v;
	} else {
		*calc_mode = 0;
	}
}

static void lsr_read_animatable(GF_LASeRCodec *lsr, SMIL_AttributeName *anim_type, SVGElement *elt, const char *name)
{
/*enumeration 
	audio-level{0} choice{1} color{2} color-rendering{3} cx{4} cy{5} d{6} display{7} display-align{8} editable{9} 
	fill{10} fill-opacity{11} fill-rule{12} 
	focusEast{13} focusNorth{14} focusNorthEast{15} focusNorthWest{16} focusSouth{17} focusSouthEast{18} 
	focusSouthWest{19} focusWest{20} focusable{21} font-family{22} font-size{23} font-style{24} font-weight{25} 
	gradientUnits{26} height{27} image-rendering{28} line-increment{29} opacity{30} pathLength{31} pointer-events{32} 
	points{33} preserveAspectRatio{34} r{35} rotate{36} rx{37} ry{38} shape-rendering{39} size{40} solid-color{41} 
	solid-opacity{42} stop-color{43} stop-opacity{44} stroke{45} stroke-dasharray{46} stroke-dashoffset{47} 
	stroke-linecap{48} stroke-linejoin{49} stroke-miterlimit{50} stroke-opacity{51} stroke-width{52} 
	target{53} text-anchor{54} transform{55} type{56} vector-effect{57} viewBox{58} viewport-fill{59} 
	viewport-fill-opacity{60} visibility{61} width{62} x{63} x1{64} x2{65} xlink:href{66} y{67} y1{68} y2{69} 
*/
	u32 val;
	GF_LSR_READ_INT(lsr, val, 8, "attributeType");
}
static void lsr_read_time(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	SMIL_Time *v;
	u32 val;
	while (gf_list_count(l)) {
		v = gf_list_last(l);
		gf_list_rem_last(l);
		if (v->element_id) free(v->element_id);
		free(v);
	}
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (!val) return;
	GF_LSR_READ_INT(lsr, val, 1, "choice");
	GF_SAFEALLOC(v, sizeof(SMIL_Time));
	if (val) {
		GF_LSR_READ_INT(lsr, val, 1, "time");
		v->type = val ? SMIL_TIME_UNSPECIFIED : SMIL_TIME_INDEFINITE;
	} else {
		Bool sign;
		u32 now;
		GF_LSR_READ_INT(lsr, sign, 1, "sign");
		now = lsr_read_vluimsbf5(lsr, "value");
		v->type = SMIL_TIME_CLOCK;
		v->clock = now;
		v->clock /= lsr->time_resolution;
		if (sign) v->clock *= -1; 
	}
	gf_list_add(l, v);
}

static void lsr_read_single_time(GF_LASeRCodec *lsr, SMIL_Time *v, const char *name)
{
	u32 val;
	v->type = SMIL_TIME_INDEFINITE;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (!val) return;

	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val) {
		GF_LSR_READ_INT(lsr, val, 1, "time");
		v->type = val ? SMIL_TIME_UNSPECIFIED : SMIL_TIME_INDEFINITE;
	} else {
		Bool sign;
		u32 now;
		GF_LSR_READ_INT(lsr, sign, 1, "sign");
		now = lsr_read_vluimsbf5(lsr, "value");
		v->type = SMIL_TIME_CLOCK;
		v->clock = now;
		v->clock /= lsr->time_resolution;
		if (sign) v->clock *= -1; 
	}
}

static void lsr_read_duration(GF_LASeRCodec *lsr, SMIL_Duration *smil, const char *name)
{
	u32 val;
	smil->type = 0;
	GF_LSR_READ_INT(lsr, val, 1, "has__5_dur");
	if (!val) return;

	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val) {
		GF_LSR_READ_INT(lsr, val, 1, "time");
		smil->type = val ? SMIL_DURATION_MEDIA : SMIL_DURATION_INDEFINITE;
	} else {
		u32 now;
		Bool sign;
		GF_LSR_READ_INT(lsr, sign, 1, "sign");
		now = lsr_read_vluimsbf5(lsr, "value");
		smil->clock_value = now;
		smil->clock_value /= lsr->time_resolution;
		if (sign) smil->clock_value *= -1; 
		smil->type = SMIL_DURATION_DEFINED;
	}
}
static void lsr_read_anim_fill(GF_LASeRCodec *lsr, u8 *animFreeze, const char *name)
{
	u32 val;

	GF_LSR_READ_INT(lsr, val, 1, name);
	if (val) {
		/*enumeration freeze{0} remove{1}*/
		GF_LSR_READ_INT(lsr, val, 1, name);
		*animFreeze = SMIL_FILL_REMOVE;
	} else {
		*animFreeze = SMIL_FILL_FREEZE;
	}
}
static void lsr_read_anim_repeat(GF_LASeRCodec *lsr, SMIL_RepeatCount *repeat, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (!val) {
		repeat->type = SMIL_REPEATCOUNT_UNSPECIFIED;
	} else {
		GF_LSR_READ_INT(lsr, val, 1, name);
		if (val) repeat->type = SMIL_REPEATCOUNT_INDEFINITE;
		else {
			repeat->type = SMIL_REPEATCOUNT_DEFINED;
			/*TODO spec is wrong here*/
			repeat->count = INT2FIX( lsr_read_vluimsbf5(lsr, name) );
		}
	}
}
static void lsr_read_anim_restart(GF_LASeRCodec *lsr, u8 *animRestart, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (!val) {
		*animRestart = SMIL_RESTART_ALWAYS;
	} else {
		/*enumeration always{0} never{1} whenNotActive{2}*/
		GF_LSR_READ_INT(lsr, *animRestart, 2, name);
	}
}

static void lsr_read_an_anim_value(GF_LASeRCodec *lsr, SMIL_AnimateValue *val, u32 type, const char *name)
{
	u32 flag;
	/*TODO spec is wrong here*/
	GF_LSR_READ_INT(lsr, flag, 1, "escapeFlag");
    if (flag) GF_LSR_READ_INT(lsr, flag, 2, "escapeEnum");

    switch(type) {
    case 0: lsr_read_byte_align_string(lsr, val->value, name); break;
    case 1: *(Fixed *) val->value = lsr_read_fixed_16_8(lsr, name); break;
    case 12: lsr_read_any_uri(lsr, val->value, name); break;
#if TODO_LASER_EXTENSIONS	
    case 2: lsr_read_path(lsr, val->value, name); break;
    case 3: lsr_read_point_sequence(lsr, val->value, name); break;
#endif
    case 4: *(Fixed *) val->value = lsr_read_fixed_clamp(lsr, name); break;
    case 5: lsr_read_color_class(lsr, (SVG_Paint *)val->value, name); break;
    case 6: *(u8 *) val->value = lsr_read_vluimsbf5(lsr, name); break;
    case 10: *(u32 *) val->value = lsr_read_vluimsbf5(lsr, name); break;
#if TODO_LASER_EXTENSIONS	
    case 11: // font
        vluimsbf5 j;
        value = fontTable[j];
        break;
#endif
    case 7: // ints
	{
		GF_List *l = *(GF_List **)val->value;
		u32 i, count = gf_list_count(l);
		while (gf_list_count(l)) {
			u8 *v = gf_list_last(l);
			gf_list_rem_last(l);
			free(v);
		}
		count = lsr_read_vluimsbf5(lsr, "count");
        for (i=0; i<count; i++) {
			u8 *v = malloc(sizeof(u8));
			*v = lsr_read_vluimsbf5(lsr, "val");
			gf_list_add(l, v);
        }
	}
        break;
    case 8: // floats
	{
		GF_List *l = *(GF_List **)val->value;
		u32 i, count = gf_list_count(l);
		while (gf_list_count(l)) {
			Fixed *v = gf_list_last(l);
			gf_list_rem_last(l);
			free(v);
		}
		count = lsr_read_vluimsbf5(lsr, "count");
        for (i=0; i<count; i++) {
			Fixed *v = malloc(sizeof(Fixed));
			*v = lsr_read_fixed_16_8(lsr, "val");
			gf_list_add(l, v);
        }
	}
        break;
    case 9: // point - TODO why is this fixed_16_8 and not a coordinate ??
		((SVG_Point *)val->value)->x = lsr_read_fixed_16_8(lsr, "val");
		((SVG_Point *)val->value)->y = lsr_read_fixed_16_8(lsr, "val");
        break;
    default:
		lsr_read_extension_class(lsr, NULL, 0, name);
        break;
    }
}

static void lsr_read_anim_value(GF_LASeRCodec *lsr, SMIL_AnimateValue *anim, const char *name)
{
	u32 val, type;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (!val) {
		anim->type = 0;
	} else {
		GF_LSR_READ_INT(lsr, type, 4, "type");
		lsr_read_an_anim_value(lsr, anim, type, name);
	}
}

static void lsr_read_anim_values(GF_LASeRCodec *lsr, SMIL_AnimateValues *anims, const char *name)
{
	u32 flag, i, count = 0;
	u32 type;
	
	GF_LSR_READ_INT(lsr, flag, 1, name);
	if (!flag) return;

	while (gf_list_count(anims->values)) {
		SMIL_AnimateValue *att = gf_list_last(anims->values);
		gf_list_rem_last(anims->values);
		/*TODO THIS IS WRONG FIXME*/
		free(att);
	}

	GF_LSR_READ_INT(lsr, type, 4, "type");
	count = lsr_read_vluimsbf5(lsr, "count");
	for (i=0; i<count; i++) {
		SMIL_AnimateValue *att;
		GF_SAFEALLOC(att, sizeof(SMIL_AnimateValue));
		gf_list_add(anims->values, att);
		lsr_read_an_anim_value(lsr, att, type, name);
	}
}

static void lsr_read_fraction_12(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	u32 i, count, flag;
	GF_LSR_READ_INT(lsr, count, 1, name);
	if (!count) return;
	while (gf_list_count(l)) {
		Fixed *f = gf_list_last(l);
		gf_list_rem_last(l);
		free(f);

	}
	count = lsr_read_vluimsbf5(lsr, "name");
	for (i=0; i<count; i++) {
		Fixed *f;
		GF_SAFEALLOC(f, sizeof(Fixed));
		GF_LSR_READ_INT(lsr, flag, 1, "hasShort");
		if (flag) {
			GF_LSR_READ_INT(lsr, flag, 1, "isZero");
			if (flag) *f = 0;
			else *f = FIX_ONE;
		} else {
			u32 v;
			GF_LSR_READ_INT(lsr, v, 12, "val");
			*f = INT2FIX(v) / 4096/*(1<<12)*/;
		}
	}
}
static void lsr_read_float_list(GF_LASeRCodec *lsr, GF_List *l, const char *name)
{
	u32 i, count = gf_list_count(l);
	GF_LSR_READ_INT(lsr, count, 1, name);
	if (!count) return;
	while (gf_list_count(l)) {
		Fixed *v = gf_list_last(l);
		gf_list_rem_last(l);
		free(v);
	}
	count = lsr_read_vluimsbf5(lsr, "count");
	for (i=0;i<count;i++) {
		Fixed *v = malloc(sizeof(Fixed));
		*v = lsr_read_fixed_16_8(lsr, "val");
		gf_list_add(l, v);
	}
}

static Fixed lsr_translate_coords(GF_LASeRCodec *lsr, u32 val)
{
	u32 mask;
	Bool sign = 0;
	Fixed res;
	mask = 1<<(lsr->coord_bits-1);
	if (val & mask) {
		sign = 1;
		val &= ~mask;
	}
	res = gf_divfix(INT2FIX(val), lsr->res_factor);
	if (sign) res *= -1;
	return res;
}


static void lsr_read_point_sequence(GF_LASeRCodec *lsr, GF_List *pts, const char *name)
{
	u32 flag, i, count;
	
	while (gf_list_count(pts)) {
		SVG_Point *v = gf_list_last(pts);
		gf_list_rem_last(pts);
		free(v);
	}
	count = lsr_read_vluimsbf5(lsr, "nbPoints");
	if (!count) return;
	/*TODO laser point encoding*/
	GF_LSR_READ_INT(lsr, flag, 1, "flag");
    if (!flag) {
        if (count < 3) {
			u32 nb_bits, v;
			GF_LSR_READ_INT(lsr, nb_bits, 5, "bits");
            for (i=0; i<count; i++) {
				SVG_Point *pt = malloc(sizeof(SVG_Point));
				gf_list_add(pts, pt);
				GF_LSR_READ_INT(lsr, v, nb_bits, "x");
				pt->x = lsr_translate_coords(lsr, v);
				GF_LSR_READ_INT(lsr, v, nb_bits, "y");
				pt->y = lsr_translate_coords(lsr, v);
            }
        } else {
			u32 nb_dx, nb_dy, x, y, dx, dy;
			SVG_Point *pt = malloc(sizeof(SVG_Point));
			gf_list_add(pts, pt);
			
			GF_LSR_READ_INT(lsr, nb_dx, 5, "bits");
			GF_LSR_READ_INT(lsr, x, nb_dx, "x");
			pt->x = lsr_translate_coords(lsr, x);
			GF_LSR_READ_INT(lsr, y, nb_dx, "y");
			pt->y = lsr_translate_coords(lsr, y);

			GF_LSR_READ_INT(lsr, nb_dx, 5, "bitsx");
			GF_LSR_READ_INT(lsr, nb_dy, 5, "bitsy");
			for (i=1; i<count; i++) {
				SVG_Point *pt = malloc(sizeof(SVG_Point));
				gf_list_add(pts, pt);
				GF_LSR_READ_INT(lsr, dx, nb_dx, "dx"); x += dx; pt->x = lsr_translate_coords(lsr, x);
				GF_LSR_READ_INT(lsr, dy, nb_dy, "dy"); x += dy; pt->y = lsr_translate_coords(lsr, y);
			}
        }
	}
	/*TODO golomb coding*/
}
static void lsr_read_path_type(GF_LASeRCodec *lsr, SVG_PathData *path, const char *name)
{
	u32 i, count;
	lsr_read_point_sequence(lsr, path->points, "seq");
	while (gf_list_count(path->commands)) {
		u8 *v = gf_list_last(path->commands);
		gf_list_rem_last(path->commands);
		free(v);
	}
    count = lsr_read_vluimsbf5(lsr, "nbOfTypes");
    for (i=0; i<count; i++) {
        u8 *type = malloc(sizeof(u8));
		gf_list_add(path->commands, type);
		GF_LSR_READ_INT(lsr, *type, 8, name);
    }
}

static void lsr_read_rotate_type(GF_LASeRCodec *lsr, SVG_String *rotate, const char *name)
{
	u32 flag;
	if (*rotate) free(*rotate);
	GF_LSR_READ_INT(lsr, flag, 1, name);
	*rotate = NULL;
	assert(0);
}
static void lsr_read_sync_behavior(GF_LASeRCodec *lsr, u8 *sync, const char *name)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, name);
	if (!flag) *sync = SMIL_SYNCBEHAVIOR_INHERIT;
	else {
		GF_LSR_READ_INT(lsr, flag, 2, name);
		*sync = flag + 1;
	}
}
static void lsr_read_sync_tolerance(GF_LASeRCodec *lsr, SMIL_SyncTolerance *sync, const char *name)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, name);
	if (!flag) sync->type = SMIL_SYNCTOLERANCE_INHERIT;
	else {
		GF_LSR_READ_INT(lsr, flag, 1, name);
		if (flag) sync->type = SMIL_SYNCTOLERANCE_DEFAULT;
		else {
			u32 v = lsr_read_vluimsbf5(lsr, "value");
			sync->value = INT2FIX(v);
			sync->value /= lsr->time_resolution;
		}
	}
}
static void lsr_read_coordinate(GF_LASeRCodec *lsr, Fixed *val, Bool skipable, const char *name)
{
	u32 flag;
	if (skipable) {
		GF_LSR_READ_INT(lsr, flag, 1, name);
		if (!flag) {
			*val = 0;
			return;
		}
	} 
	GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, name);
	*val = lsr_translate_coords(lsr, flag);
}
static void lsr_read_transform_behavior(GF_LASeRCodec *lsr, u8 *tr_type, const char *name)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, name);
	if (!flag) {
		*tr_type = 0;
	} else {
		GF_LSR_READ_INT(lsr, flag, 4, name);
		assert(0);
	}
}

static void lsr_read_content_type(GF_LASeRCodec *lsr, unsigned char **type, const char *name)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "hasType");
	if (flag) lsr_read_byte_align_string(lsr, type, "type");
}
static void lsr_read_value_with_units(GF_LASeRCodec *lsr, SVG_Number *n, const char *name)
{
	s32 val;
	GF_LSR_READ_INT(lsr, val, 32, name);
#ifdef GPAC_FIXED_POINT
	n->value = val << 8;
#else
	n->value = INT2FIX(val) / (1<<8);
#endif
	GF_LSR_READ_INT(lsr, val, 3, "units");
    switch (val) {
	case 1: n->type = SVG_NUMBER_IN; break;
	case 2: n->type = SVG_NUMBER_CM; break;
	case 3: n->type = SVG_NUMBER_MM; break;
	case 4: n->type = SVG_NUMBER_PT; break;
	case 5: n->type = SVG_NUMBER_PC; break;
	case 6: n->type = SVG_NUMBER_PERCENTAGE; break;
	default:  n->type = 0; break;
	}
}

static void lsr_read_event_type(GF_LASeRCodec *lsr, u32 *evtType, unsigned char **evtName)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "choice");
	if (!flag) {
		lsr_read_byte_align_string(lsr, evtName, "evtString");
	} else {
		GF_LSR_READ_INT(lsr, flag, 6, "event");
		/*enumeration abort{0} activate{1} begin{2} click{3} end{4} error{5} focusin{6} focusout{7} 
		keydown{8} keypress{9} keyup{10} load{11} longkeypress{12} mousedown{13} mouseout{14} 
		mouseover{15} mouseup{16} repeat{17} resize{18} scroll{19} textinput{20} unload{21} zoom{22} */
		switch (flag) {
		case 0: *evtType = SVG_DOM_EVT_ABORT; break;
		case 1: *evtType = SVG_DOM_EVT_ACTIVATE; break;
		case 2: *evtType = SVG_DOM_EVT_BEGIN; break;
		case 3: *evtType = SVG_DOM_EVT_CLICK; break;
		case 4: *evtType = SVG_DOM_EVT_END; break;
		case 5: *evtType = SVG_DOM_EVT_ERROR; break;
		case 6: *evtType = SVG_DOM_EVT_FOCUSIN; break;
		case 7: *evtType = SVG_DOM_EVT_FOCUSOUT; break;
		case 8: *evtType = SVG_DOM_EVT_KEYDOWN; break;
		case 9: *evtType = SVG_DOM_EVT_KEYPRESS; break;
		case 10: *evtType = SVG_DOM_EVT_KEYUP; break;
		case 11: *evtType = SVG_DOM_EVT_LOAD; break;
		case 12: *evtType = SVG_DOM_EVT_LONGKEYPRESS; break;
		case 13: *evtType = SVG_DOM_EVT_MOUSEDOWN; break;
		case 14: *evtType = SVG_DOM_EVT_MOUSEOUT; break;
		case 15: *evtType = SVG_DOM_EVT_MOUSEOVER; break;
		case 16: *evtType = SVG_DOM_EVT_MOUSEUP; break;
		case 17: *evtType = SVG_DOM_EVT_REPEAT; break;
		case 18: *evtType = SVG_DOM_EVT_RESIZE; break;
		case 19: *evtType = SVG_DOM_EVT_SCROLL; break;
		case 20: *evtType = SVG_DOM_EVT_TEXTINPUT; break;
		case 21: *evtType = SVG_DOM_EVT_UNLOAD; break;
		case 22: *evtType = SVG_DOM_EVT_ZOOM; break;
		default:
			fprintf(stdout, "Unsupported LASER event\n");
			break;
		}
	}
}


static GF_Node *lsr_read_a(GF_LASeRCodec *lsr)
{
	Bool flag;
	SVGaElement *a = (SVGaElement *) gf_node_new(lsr->sg, TAG_SVG_a);
	lsr_read_string_attribute(lsr, &a->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) a);
	lsr_read_rare(lsr, (SVGElement *) a);
	lsr_read_fill(lsr, (SVGElement *) a);
	lsr_read_line_increment(lsr, (SVGElement *) a);
	lsr_read_stroke(lsr, (SVGElement *) a);
	GF_LSR_READ_INT(lsr, a->externalResourcesRequired, 1, "externalResourcesRequired");
	GF_LSR_READ_INT(lsr, flag, 1, "hasTarget");
	if (flag) lsr_read_byte_align_string(lsr, &a->target, "target");
	lsr_read_href(lsr, & a->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) a);
	lsr_read_group_content(lsr, (SVGElement *) a);
	return (GF_Node *)a;
}

static GF_Node *lsr_read_animate(GF_LASeRCodec *lsr)
{
	Bool flag;
	SVGanimateElement *elt = (SVGanimateElement *) gf_node_new(lsr->sg, TAG_SVG_animate);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_accumulate(lsr, &elt->accumulate);
	lsr_read_additive(lsr, &elt->additive);
	lsr_read_anim_value(lsr, &elt->by, "_4_by");
	lsr_read_calc_mode(lsr, &elt->calcMode);
	lsr_read_anim_value(lsr, &elt->from, "_4_from");
	lsr_read_fraction_12(lsr, elt->keySplines, "_4_keySplines");
	lsr_read_fraction_12(lsr, elt->keyTimes, "_4_keyTimes");
	lsr_read_anim_values(lsr, &elt->values, "_4_values");
	lsr_read_animatable(lsr, &elt->attributeName, (SVGElement *)elt, "_5_attributeName");
	lsr_read_time(lsr, elt->begin, "_5_begin");
	lsr_read_duration(lsr, &elt->dur, "_5_dur");
	/*TODO lsr:enabled not present in our code*/
	GF_LSR_READ_INT(lsr, flag/*animate->enabled*/, 1, "enabled");
	lsr_read_anim_fill(lsr, &elt->fill, "_5_fill");
	lsr_read_anim_repeat(lsr, &elt->repeatCount, "repeatCount");
	lsr_read_duration(lsr, &elt->repeatDur, "_5_repeatDur");
	lsr_read_anim_restart(lsr, &elt->restart, "_5_restart");
	lsr_read_anim_value(lsr, &elt->to, "_5_to");
	lsr_read_href(lsr, & elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}


static GF_Node *lsr_read_animateMotion(GF_LASeRCodec *lsr)
{
	Bool flag;
	SVGanimateMotionElement *elt = (SVGanimateMotionElement *) gf_node_new(lsr->sg, TAG_SVG_animateMotion);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_accumulate(lsr, &elt->accumulate);
	lsr_read_additive(lsr, &elt->additive);
	lsr_read_anim_value(lsr, &elt->by, "_4_by");
	lsr_read_calc_mode(lsr, &elt->calcMode);
	lsr_read_anim_value(lsr, &elt->from, "_4_from");
	lsr_read_fraction_12(lsr, elt->keySplines, "_4_keySplines");
	lsr_read_fraction_12(lsr, elt->keyTimes, "_4_keyTimes");
	lsr_read_anim_values(lsr, &elt->values, "_4_values");
//	lsr_read_animatable(lsr, &elt->attributeName, (SVGElement *)elt, "_5_attributeName");
	lsr_read_time(lsr, elt->begin, "_5_begin");
	lsr_read_duration(lsr, &elt->dur, "_5_dur");
	/*TODO lsr:enabled not present in our code*/
	GF_LSR_READ_INT(lsr, flag/*elt->enabled*/, 1, "enabled");
	lsr_read_anim_fill(lsr, &elt->fill, "_5_fill");
	lsr_read_anim_repeat(lsr, &elt->repeatCount, "_5_repeatCount");
	lsr_read_duration(lsr, &elt->repeatDur, "_5_repeatDur");
	lsr_read_anim_restart(lsr, &elt->restart, "_5_restart");
	lsr_read_anim_value(lsr, &elt->to, "_5_to");
	lsr_read_float_list(lsr, elt->keyPoints, "keyPoints");
	lsr_read_path_type(lsr, &elt->path, "path");
	lsr_read_rotate_type(lsr, &elt->rotate, "rotate");
	lsr_read_href(lsr, &elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}

static GF_Node *lsr_read_animateTransform(GF_LASeRCodec *lsr)
{
	Bool flag;
	SVGanimateTransformElement *elt= (SVGanimateTransformElement *) gf_node_new(lsr->sg, TAG_SVG_animateTransform);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_accumulate(lsr, &elt->accumulate);
	lsr_read_additive(lsr, &elt->additive);
	lsr_read_anim_value(lsr, &elt->by, "_4_by");
	lsr_read_calc_mode(lsr, &elt->calcMode);
	lsr_read_anim_value(lsr, &elt->from, "_4_from");
	lsr_read_fraction_12(lsr, elt->keySplines, "_4_keySplines");
	lsr_read_fraction_12(lsr, elt->keyTimes, "_4_keyTimes");
	lsr_read_anim_values(lsr, &elt->values, "_4_values");
	lsr_read_animatable(lsr, &elt->attributeName, (SVGElement *)elt, "_5_attributeName");
	lsr_read_time(lsr, elt->begin, "_5_begin");
	lsr_read_duration(lsr, &elt->dur, "_5_dur");
	/*TODO lsr:enabled not present in our code*/
	GF_LSR_READ_INT(lsr, flag/*elt->enabled*/, 1, "enabled");
	lsr_read_anim_fill(lsr, &elt->fill, "_5_fill");
	lsr_read_anim_repeat(lsr, &elt->repeatCount, "_5_repeatCount");
	lsr_read_duration(lsr, &elt->repeatDur, "_5_repeatDur");
	lsr_read_anim_restart(lsr, &elt->restart, "_5_restart");
	lsr_read_anim_value(lsr, &elt->to, "_5_to");

	/*enumeration rotate{0} scale{1} skewX{2} skewY{3} translate{4}*/
	GF_LSR_READ_INT(lsr, flag, 3, "rotscatra");
	switch (flag) {
	case 0: elt->type = SVG_TRANSFORM_ROTATE; break;
	case 1: elt->type = SVG_TRANSFORM_SCALE; break;
	case 2: elt->type = SVG_TRANSFORM_SKEWX; break;
	case 3: elt->type = SVG_TRANSFORM_SKEWY; break;
	case 4: elt->type = SVG_TRANSFORM_TRANSLATE; break;
	}

	lsr_read_href(lsr, & elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}

static GF_Node *lsr_read_audio(GF_LASeRCodec *lsr)
{
	Bool flag;
	SVGaudioElement *elt= (SVGaudioElement *) gf_node_new(lsr->sg, TAG_SVG_audio);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_time(lsr, elt->begin, "_5_begin");
	lsr_read_duration(lsr, &elt->dur, "_5_dur");
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_anim_repeat(lsr, &elt->repeatCount, "_5_repeatCount");
	lsr_read_duration(lsr, &elt->repeatDur, "_5_repeatDur");
	lsr_read_sync_behavior(lsr, &elt->syncBehavior, "syncBehavior");
	lsr_read_sync_tolerance(lsr, &elt->syncTolerance, "syncBehavior");
	lsr_read_content_type(lsr, &elt->type, "type");
	lsr_read_href(lsr, & elt->xlink_href);

//	lsr_read_time(lsr, elt->clipBegin, "clipBegin");
	GF_LSR_READ_INT(lsr, flag, 1, "clipBegin");
//	lsr_read_time(lsr, elt->clipEnd, "clipEnd");
	GF_LSR_READ_INT(lsr, flag, 1, "clipEnd");
//	lsr_read_href(lsr, & elt->syncReference);
	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncReference");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}
static GF_Node *lsr_read_circle(GF_LASeRCodec *lsr)
{
	SVGcircleElement *elt= (SVGcircleElement *) gf_node_new(lsr->sg, TAG_SVG_circle);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	lsr_read_coordinate(lsr, &elt->cx.value, 1, "cx");
	lsr_read_coordinate(lsr, &elt->cy.value, 1, "cy");
	lsr_read_coordinate(lsr, &elt->r.value, 0, "r");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}
static GF_Node *lsr_read_cursor(GF_LASeRCodec *lsr)
{
	/*TODO FIX CODE GENERATION*/
	SVGimageElement *elt = (SVGimageElement*) gf_node_new(lsr->sg, TAG_SVG_image);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_coordinate(lsr, &elt->x.value, 1, "x");
	lsr_read_coordinate(lsr, &elt->y.value, 1, "y");
	lsr_read_href(lsr, &elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}

static GF_Node *lsr_read_data(GF_LASeRCodec *lsr, u32 node_tag)
{
	SVGdescElement *elt = (SVGdescElement*) gf_node_new(lsr->sg, node_tag);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}

static GF_Node *lsr_read_defs(GF_LASeRCodec *lsr)
{
	SVGdefsElement *elt = (SVGdefsElement*) gf_node_new(lsr->sg, TAG_SVG_defs);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}
static GF_Node *lsr_read_ellipse(GF_LASeRCodec *lsr)
{
	SVGellipseElement *elt = (SVGellipseElement*) gf_node_new(lsr->sg, TAG_SVG_ellipse);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	lsr_read_coordinate(lsr, &elt->cx.value, 1, "cx");
	lsr_read_coordinate(lsr, &elt->cy.value, 1, "cy");
	lsr_read_coordinate(lsr, &elt->rx.value, 0, "rx");
	lsr_read_coordinate(lsr, &elt->ry.value, 0, "ry");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}
static GF_Node *lsr_read_foreignObject(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGforeignObjectElement *elt = (SVGforeignObjectElement *) gf_node_new(lsr->sg, TAG_SVG_foreignObject);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_coordinate(lsr, &elt->height.value, 0, "height");
	lsr_read_coordinate(lsr, &elt->width.value, 0, "width");
	lsr_read_coordinate(lsr, &elt->x.value, 1, "x");
	lsr_read_coordinate(lsr, &elt->y.value, 1, "y");

/*	TODO
	bit(1) opt_group;
	if(opt_group) {
		vluimsbf5 occ1;
		for(int t=0;t<occ1;t++) {
			privateElementContainer child0[[t]];
		}
	}
*/
	GF_LSR_READ_INT(lsr, flag, 1, "opt_group");
	return (GF_Node *) elt;
}
static GF_Node *lsr_read_g(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGgElement *elt = (SVGgElement*) gf_node_new(lsr->sg, TAG_SVG_g);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	/*TODO choice only ibn LASeR, not in SVG*/
	GF_LSR_READ_INT(lsr, flag, 1, "choice");
/*
class choiceClass {
		bit(1) choice;
		switch(choice) {
			case 0:
				bit(8) value;
				break;
			case 1:
					enumeration all{0} clip{1} delta{2} none{3} 								
					bit(2) choice;
				break;
			default:
				break;
			}
}

	*/
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	/*TODO size only in LASeR, not in SVG*/
	GF_LSR_READ_INT(lsr, flag, 1, "size");

	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *) elt;
}
static GF_Node *lsr_read_image(GF_LASeRCodec *lsr)
{
	u32 flag;
	u8 fixme;
	SVGimageElement *elt = (SVGimageElement*) gf_node_new(lsr->sg, TAG_SVG_image);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_coordinate(lsr, &elt->height.value, 1, "height");
	GF_LSR_READ_INT(lsr, flag, 1, "opacity");
	if (flag) {
		elt->opacity.type = SVG_NUMBER_VALUE;
		elt->opacity.value = lsr_read_fixed_clamp(lsr, "opacity");
	}
	lsr_read_transform_behavior(lsr, &fixme, "transformBehavior");
	lsr_read_content_type(lsr, &elt->type, "type");
	lsr_read_coordinate(lsr, &elt->width.value, 1, "width");
	lsr_read_coordinate(lsr, &elt->x.value, 1, "x");
	lsr_read_coordinate(lsr, &elt->y.value, 1, "y");
	lsr_read_href(lsr, &elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_line(GF_LASeRCodec *lsr)
{
	SVGlineElement *elt = (SVGlineElement*) gf_node_new(lsr->sg, TAG_SVG_line);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	lsr_read_coordinate(lsr, &elt->x1.value, 1, "x1");
	lsr_read_coordinate(lsr, &elt->x2.value, 0, "x2");
	lsr_read_coordinate(lsr, &elt->y1.value, 1, "y1");
	lsr_read_coordinate(lsr, &elt->y2.value, 0, "y2");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_linearGradient(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGlinearGradientElement*elt = (SVGlinearGradientElement*) gf_node_new(lsr->sg, TAG_SVG_linearGradient);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	GF_LSR_READ_INT(lsr, flag, 1, "hasGradientUnits");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "hasGradientUnits");
		/*enumeration objectBoundingBox{0} userSpaceOnUse{1}*/
		if (flag) elt->gradientUnits = SVG_GRADIENTUNITS_USER;
	}
	lsr_read_coordinate(lsr, &elt->x1.value, 1, "x1");
	lsr_read_coordinate(lsr, &elt->x2.value, 1, "x2");
	lsr_read_coordinate(lsr, &elt->y1.value, 1, "y1");
	lsr_read_coordinate(lsr, &elt->y2.value, 1, "y2");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_mpath(GF_LASeRCodec *lsr)
{
	SVGmpathElement *elt = (SVGmpathElement*) gf_node_new(lsr->sg, TAG_SVG_mpath);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_href(lsr, &elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_path(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGpathElement *elt = (SVGpathElement*) gf_node_new(lsr->sg, TAG_SVG_path);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	lsr_read_path_type(lsr, &elt->d, "d");
	GF_LSR_READ_INT(lsr, flag, 1, "hasPathLength");
	if (flag) elt->pathLength.value = lsr_read_fixed_16_8(lsr, "pathLength");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_polygon(GF_LASeRCodec *lsr)
{
	SVGpolygonElement *elt = (SVGpolygonElement*) gf_node_new(lsr->sg, TAG_SVG_polygon);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	lsr_read_point_sequence(lsr, elt->points, "points");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_radialGradient(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGradialGradientElement *elt = (SVGradialGradientElement*) gf_node_new(lsr->sg, TAG_SVG_radialGradient);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	lsr_read_coordinate(lsr, &elt->cx.value, 1, "cx");
	lsr_read_coordinate(lsr, &elt->cy.value, 1, "cy");

	/*enumeration objectBoundingBox{0} userSpaceOnUse{1}*/
	GF_LSR_READ_INT(lsr, flag, 1, "hasGradientUnits");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "radientUnits");
		if (flag) elt->gradientUnits = SVG_GRADIENTUNITS_USER;
	}
	lsr_read_coordinate(lsr, &elt->r.value, 1, "r");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_rect(GF_LASeRCodec *lsr)
{
	SVGrectElement*elt = (SVGrectElement*) gf_node_new(lsr->sg, TAG_SVG_rect);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	lsr_read_coordinate(lsr, &elt->height.value, 0, "height");
	lsr_read_coordinate(lsr, &elt->rx.value, 1, "rx");
	lsr_read_coordinate(lsr, &elt->ry.value, 1, "ry");
	lsr_read_coordinate(lsr, &elt->width.value, 0, "width");
	lsr_read_coordinate(lsr, &elt->x.value, 1, "x");
	lsr_read_coordinate(lsr, &elt->y.value, 1, "y");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_script(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGscriptElement*elt = (SVGscriptElement*) gf_node_new(lsr->sg, TAG_SVG_script);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	/*TODO fix code generator*/
	lsr_read_single_time(lsr, NULL/*&elt->begin*/, "begin");
	GF_LSR_READ_INT(lsr, flag/*elt->enabled*/, 1, "enabled");
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_content_type(lsr, &elt->type, "type");
	/*TODO fix code generator*/
	lsr_read_href(lsr, NULL /*&elt->xlink_href*/);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_command_list(lsr, NULL, elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_set(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGsetElement*elt = (SVGsetElement*) gf_node_new(lsr->sg, TAG_SVG_set);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_animatable(lsr, &elt->attributeName, (SVGElement *)elt, "_5_attributeName");
	lsr_read_time(lsr, elt->begin, "_5_begin");
	lsr_read_duration(lsr, &elt->dur, "_5_dur");
	/*TODO lsr:enabled not present in our code*/
	GF_LSR_READ_INT(lsr, flag/*elt->enabled*/, 1, "enabled");
	lsr_read_anim_fill(lsr, &elt->fill, "_5_fill");
	lsr_read_anim_repeat(lsr, &elt->repeatCount, "_5_repeatCount");
	lsr_read_duration(lsr, &elt->repeatDur, "_5_repeatDur");
	lsr_read_anim_restart(lsr, &elt->restart, "_5_restart");
	lsr_read_anim_value(lsr, &elt->to, "_5_to");
	lsr_read_href(lsr, &elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_stop(GF_LASeRCodec *lsr)
{
	SVGstopElement *elt = (SVGstopElement *) gf_node_new(lsr->sg, TAG_SVG_stop);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	elt->offset.value = lsr_read_fixed_16_8(lsr, "offset");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}
static GF_Node *lsr_read_svg(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGsvgElement*elt = (SVGsvgElement*) gf_node_new(lsr->sg, TAG_SVG_svg);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement *) elt);
	lsr_read_line_increment(lsr, (SVGElement *) elt);
	lsr_read_stroke(lsr, (SVGElement *) elt);
	lsr_read_string_attribute(lsr, &elt->baseProfile, "baseProfile");
	lsr_read_string_attribute(lsr, &elt->contentScriptType, "contentScriptType");
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_value_with_units(lsr, &elt->height, "height");
	GF_LSR_READ_INT(lsr, flag, 1, "hasPlaybackOrder");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "playbackOrder");
		if (flag) elt->playbackOrder=SVG_PLAYBACKORDER_FORWARDONLY;
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasPreserveAR");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 5, "preserveAR");
		if (flag % 2) {
			elt->preserveAspectRatio.meetOrSlice = SVG_MEETORSLICE_MEET;
			flag --;
		}
		switch (flag) {
		case 1: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMAXYMAX; break;
		case 3: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMAXYMID; break;
		case 5: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMAXYMIN; break;
		case 7: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMIDYMAX; break;
		case 9: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMIDYMID; break;
		case 11: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMIDYMIN; break;
		case 13: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMINYMAX; break;
		case 15: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMINYMID; break;
		case 17: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_XMINYMIN; break;
		default: elt->preserveAspectRatio.align = SVG_PRESERVEASPECTRATIO_NONE; break;
		}
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncBehavior");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 2, "syncBehavior");
		switch (flag) {
		case 0: elt->syncBehaviorDefault = SMIL_SYNCBEHAVIOR_CANSLIP; break;
		case 1: elt->syncBehaviorDefault = SMIL_SYNCBEHAVIOR_INDEPENDENT; break;
		case 3: elt->syncBehaviorDefault = SMIL_SYNCBEHAVIOR_LOCKED; break;
		default: elt->syncBehaviorDefault = 0; break;
		}
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncTolerance");
	if (flag) {
		elt->syncToleranceDefault.type = SMIL_SYNCTOLERANCE_VALUE;
		GF_LSR_READ_INT(lsr, flag, 1, "choice");
	    elt->syncToleranceDefault.value = lsr_read_vluimsbf5(lsr, "value");
		elt->syncToleranceDefault.value /= lsr->time_resolution;
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasTimelineBegin");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "timelineBegin");
		if (flag) elt->timelineBegin = SVG_TIMELINEBEGIN_ONLOAD;
	}
	lsr_read_string_attribute(lsr, &elt->version, "version");

	GF_LSR_READ_INT(lsr, flag, 1, "hasViewBox");
	if (flag) {
		elt->viewBox.x = lsr_read_fixed_16_8(lsr, "viewbox.x");
		elt->viewBox.y = lsr_read_fixed_16_8(lsr, "viewbox.y");
		elt->viewBox.width = lsr_read_fixed_16_8(lsr, "viewbox.width");
		elt->viewBox.height = lsr_read_fixed_16_8(lsr, "viewbox.height");
	}
	lsr_read_value_with_units(lsr, &elt->width, "width");
	/*zoom and pan must be encoded in our code...*/
	GF_LSR_READ_INT(lsr, flag, 1, "hasZoomAndPan");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "hasZoomAndPan");
		elt->zoomAndPan = flag ? SVG_ZOOMANDPAN_MAGNIFY : SVG_ZOOMANDPAN_DISABLE;
	}
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_switch(GF_LASeRCodec *lsr)
{
	SVGswitchElement*elt = (SVGswitchElement*) gf_node_new(lsr->sg, TAG_SVG_switch);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_text(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGtextElement*elt = (SVGtextElement*) gf_node_new(lsr->sg, TAG_SVG_text);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);

	GF_LSR_READ_INT(lsr, elt->editable, 1, "editable");
	lsr_read_float_list(lsr, elt->rotate, "rotate");
	/*FIXME*/
//	lsr_read_coordinate(lsr, elt->x.value, 1, "x");
	GF_LSR_READ_INT(lsr, flag, 1, "hasX");
//	lsr_read_coordinate(lsr, elt->y.value, 1, "y");
	GF_LSR_READ_INT(lsr, flag, 1, "hasY");
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	/*TODO this won't work with our current implementation of text in SVG, we MUST move to tspan*/
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_tspan(GF_LASeRCodec *lsr)
{
	SVGtspanElement*elt = (SVGtspanElement*) gf_node_new(lsr->sg, TAG_SVG_tspan);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_use(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGuseElement*elt = (SVGuseElement*) gf_node_new(lsr->sg, TAG_SVG_use);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_fill(lsr, (SVGElement*)elt);
	lsr_read_line_increment(lsr, (SVGElement*)elt);
	lsr_read_stroke(lsr, (SVGElement*)elt);
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	/*TODO */
	GF_LSR_READ_INT(lsr, flag, 1, "hasOverflow");
	lsr_read_coordinate(lsr, &elt->x.value, 1, "x");
	lsr_read_coordinate(lsr, &elt->y.value, 1, "y");
	lsr_read_href(lsr, &elt->xlink_href);
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_video(GF_LASeRCodec *lsr)
{
	u32 flag;
	u8 fixme;
	SVGvideoElement*elt = (SVGvideoElement*) gf_node_new(lsr->sg, TAG_SVG_video);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement*) elt);
	lsr_read_time(lsr, elt->begin, "_5_begin");
	//lsr_read_duration(lsr, &elt->dur, "_5_dur");
	GF_LSR_READ_INT(lsr, elt->externalResourcesRequired, 1, "externalResourcesRequired");
	lsr_read_coordinate(lsr, &elt->height.value, 1, "height");
	/*TODO*/
	GF_LSR_READ_INT(lsr, flag, 1, "hasOverlay");
	lsr_read_anim_repeat(lsr, &elt->repeatCount, "_5_repeatCount");
	lsr_read_duration(lsr, &elt->repeatDur, "_5_repeatDur");
	lsr_read_sync_behavior(lsr, &elt->syncBehavior, "syncBehavior");
	lsr_read_sync_tolerance(lsr, &elt->syncTolerance, "syncBehavior");
	lsr_read_transform_behavior(lsr, &fixme, "transformBehavior");
	lsr_read_content_type(lsr, &elt->type, "type");
	lsr_read_coordinate(lsr, &elt->width.value, 1, "width");
	lsr_read_coordinate(lsr, &elt->x.value, 1, "x");
	lsr_read_coordinate(lsr, &elt->y.value, 1, "y");
	lsr_read_href(lsr, &elt->xlink_href);
//	lsr_read_time(lsr, elt->clipBegin, "clipBegin");
	GF_LSR_READ_INT(lsr, flag, 1, "clipBegin");
//	lsr_read_time(lsr, elt->clipEnd, "clipEnd");
	GF_LSR_READ_INT(lsr, flag, 1, "clipEnd");
//	lsr_read_href(lsr, & elt->syncReference);
	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncReference");
	
	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_listener(GF_LASeRCodec *lsr)
{
	u32 flag;
	SVGlistenerElement*elt = (SVGlistenerElement*) gf_node_new(lsr->sg, TAG_SVG_listener);
	lsr_read_string_attribute(lsr, &elt->class_attribute, "class");
	lsr_read_id(lsr, (GF_Node *) elt);
	lsr_read_rare(lsr, (SVGElement *) elt);
	GF_LSR_READ_INT(lsr, elt->defaultAction, 1, "hasDefaultAction");
	if (elt->defaultAction) GF_LSR_READ_INT(lsr, elt->defaultAction, 1, "defaultAction");
	GF_LSR_READ_INT(lsr, flag/*elt->endabled*/, 1, "enabled");
	GF_LSR_READ_INT(lsr, flag, 1, "hasEvent");
	if (flag) lsr_read_event_type(lsr, &elt->event, NULL);
	GF_LSR_READ_INT(lsr, flag, 1, "hasHandler");
	if (flag) lsr_read_any_uri(lsr, &elt->handler, "handler");
	GF_LSR_READ_INT(lsr, flag, 1, "hasObserver");
	/*TODO double check spec here*/
	if (flag) lsr_read_codec_IDREF(lsr, &elt->observer, "observer");
	GF_LSR_READ_INT(lsr, flag, 1, "hasPropagate");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag/*elt->prop*/, 1, "propagate");
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasTarget");
	/*TODO double check spec here*/
	if (flag) lsr_read_codec_IDREF(lsr, &elt->target, "target");
	/*TODO modify our SVG tree for LASeR*/
	GF_LSR_READ_INT(lsr, flag, 1, "hasDelay");
	/*TODO modify our SVG tree for LASeR*/
	GF_LSR_READ_INT(lsr, flag, 1, "hasTimeAttribute");

	lsr_read_any_attribute(lsr, (GF_Node *) elt);
	lsr_read_group_content(lsr, (SVGElement *) elt);
	return (GF_Node *)elt;
}

static GF_Node *lsr_read_content_model_2(GF_LASeRCodec *lsr)
{
	GF_Node *n;
	u32 ntype;
	GF_LSR_READ_INT(lsr, ntype, 6, "ch4"); 
	switch (ntype) {
	case 0: n = lsr_read_a(lsr); break;
	case 1: n = lsr_read_animate(lsr); break;
	case 2: n = lsr_read_animate(lsr); break;
	case 3: n = lsr_read_animateMotion(lsr); break;
	case 4: n = lsr_read_animateTransform(lsr); break;
	case 5: n = lsr_read_audio(lsr); break;
	case 6: n = lsr_read_circle(lsr); break;
#if TODO_LASER_EXTENSIONS	
	case 7: n = lsr_read_cursor(lsr); break;
#endif
	case 8: n = lsr_read_defs(lsr); break;
	case 9: n = lsr_read_data(lsr, TAG_SVG_desc); break;
	case 10: n = lsr_read_ellipse(lsr); break;
	case 11: n = lsr_read_foreignObject(lsr); break;
	case 12: n = lsr_read_g(lsr); break;
	case 13: n = lsr_read_image(lsr); break;
	case 14: n = lsr_read_line(lsr); break;
	case 15: n = lsr_read_linearGradient(lsr); break;
	case 16: n = lsr_read_data(lsr, TAG_SVG_metadata); break;
	case 17: n = lsr_read_mpath(lsr); break;
	case 18: n = lsr_read_path(lsr); break;
	case 19: n = lsr_read_polygon(lsr); break;
	case 20: n = lsr_read_polygon(lsr); break;
	case 21: n = lsr_read_radialGradient(lsr); break;
	case 22: n = lsr_read_rect(lsr); break;
/*	case 23: samegType
	case 24: samelineType sameline;
	case 25: samepathType samepath;
	case 26: samepathfillType samepathfill;
	case 27: samepolygonType samepolygon;
	case 28: samepolygonfillType samepolygonfill;
	case 29: samepolygonstrokeType samepolygonstroke;
	case 30: samepolygonType samepolyline;
	case 31: samepolygonfillType samepolylinefill;
	case 32: samepolygonstrokeType samepolylinestroke;
	case 33: samerectType samerect;
	case 34: samerectfillType samerectfill;
	case 35: sametextType sametext;
	case 36: sametextfillType sametextfill;
	case 37: sameuseType sameuse;
*/
	case 38: n = lsr_read_script(lsr); break;
	case 39: n = lsr_read_set(lsr); break;
	case 40: n = lsr_read_stop(lsr); break;
	case 41: n = lsr_read_switch(lsr); break;
	case 42: n = lsr_read_text(lsr); break;
	case 43: n = lsr_read_data(lsr, TAG_SVG_title); break;
	case 44: n = lsr_read_tspan(lsr); break;
	case 45: n = lsr_read_use(lsr); break;
	case 46: n = lsr_read_video(lsr); break;
	case 47: n = lsr_read_listener(lsr); break;
#if TODO_LASER_EXTENSIONS	
	case 48: 
		lsr_read_privateElement(lsr); 
		break;
	case 49:
		lsr_read_byte_align_string(lsr);
		break;
	case 50: 
		lsr_read_extend_class(lsr, NULL, 0, "node"); 
		break;
#endif
	default:
		break;
	}
	return n;
}

static void lsr_read_group_content(GF_LASeRCodec *lsr, SVGElement *elt)
{
	u32 i, count;
	lsr_read_object_content(lsr, elt);
	/*node attributes are all parsed*/
	gf_node_init((GF_Node *)elt);

	GF_LSR_READ_INT(lsr, count, 1, "opt_group");
	if (count) {
		count = lsr_read_vluimsbf5(lsr, "occ0");
		for (i=0; i<count; i++) {
			GF_Node *n = lsr_read_content_model_2(lsr);
			if (n) {
				gf_node_register(n, (GF_Node *)elt);
				gf_list_add(elt->children, n);
			}
		}
	}
	/*fire OnLoad event*/
}

static GF_Err lsr_read_command_list(GF_LASeRCodec *lsr, GF_List *com_list, SVGscriptElement *script)
{
	GF_Node *n;
	GF_Command *com;
	u32 i, type, count = 0;
	if (com_list) count += gf_list_count(com_list);
	if (script && script->textContent) count += 1;

	if (script) {
		lsr_read_object_content(lsr, (SVGElement *) script);
		GF_LSR_READ_INT(lsr, count, 1, "opt_group");
		if (!count) {
			gf_node_init((GF_Node *)script);
			return GF_OK;
		}
	}
	count = lsr_read_vluimsbf5(lsr, "occ0");
	for (i=0; i<count; i++) {
		GF_LSR_READ_INT(lsr, type, 4, "ch4");
		switch (type) {
		case 4: /*NewScene*/
			lsr_read_any_attribute(lsr, NULL);
			if (com_list) {
				n = lsr_read_svg(lsr);
				if (!n) return GF_NON_COMPLIANT_BITSTREAM;
				gf_node_register(n, NULL);
				com = gf_sg_command_new(lsr->sg, GF_SG_LSR_NEW_SCENE);
				com->node = n;
				gf_list_add(com_list, com);
			} else {
				gf_sg_reset(lsr->sg);
				n = lsr_read_svg(lsr);
				if (!n) return GF_NON_COMPLIANT_BITSTREAM;
				gf_node_register(n, NULL);
				gf_sg_set_root_node(lsr->sg, n);
				gf_sg_set_scene_size_info(lsr->sg, 0, 0, 1);
			}
			break;
		case 10:	/*script*/
			if (script) lsr_read_byte_align_string(lsr, &script->textContent, "textContent");
			break;
		default:
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	return GF_OK;
}

static GF_Err lsr_decode_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list)
{
	Bool reset_encoding_context;
	u32 flag, i, count;
	/*laser unit header*/
	GF_LSR_READ_INT(lsr, reset_encoding_context, 1, "resetEncodingContext");
	GF_LSR_READ_INT(lsr, flag, 1, "opt_group");
	/*if (flag)  codec_extension ext; */

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

	/*codec initialization*/
	GF_LSR_READ_INT(lsr, flag, 1, "anyXMLInitialisation");

	GF_LSR_READ_INT(lsr, flag, 1, "colorInitialisation");
	if (flag) {
		count = lsr_read_vluimsbf5(lsr, "count");
		lsr->col_table = realloc(lsr->col_table, sizeof(LSRCol)*(lsr->nb_cols+count));
		for (i=0; i<count; i++) {
			LSRCol c;
			GF_LSR_READ_INT(lsr, c.r, lsr->info->cfg.colorComponentBits, "red");
			GF_LSR_READ_INT(lsr, c.g, lsr->info->cfg.colorComponentBits, "green");
			GF_LSR_READ_INT(lsr, c.b, lsr->info->cfg.colorComponentBits, "blue");
			lsr->col_table[lsr->nb_cols+i] = c;
		}
		lsr->nb_cols += count;
	}
	lsr->colorIndexBits = gf_get_bit_size(lsr->nb_cols);
	GF_LSR_READ_INT(lsr, flag, 1, "extendedInitialisation");
	GF_LSR_READ_INT(lsr, flag, 1, "fontInitialisation");
	if (flag) {
		count = lsr_read_vluimsbf5(lsr, "count");
		for (i=0; i<count; i++) {
			unsigned char *ft = NULL;
			lsr_read_byte_align_string(lsr, &ft, "font");
			gf_list_add(lsr->font_table, ft);
		}
	}
	lsr->fontIndexBits = gf_get_bit_size(count);
	GF_LSR_READ_INT(lsr, flag, 1, "privateDataIdentifierInitialisation");

	return lsr_read_command_list(lsr, com_list, NULL);
}

