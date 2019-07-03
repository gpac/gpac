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
#include <gpac/events.h>

#ifndef GPAC_DISABLE_LASER


#define GF_LSR_READ_INT(_codec, _val, _nbBits, _str)	{\
	(_val) = gf_bs_read_int(_codec->bs, _nbBits);	\
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", _str, _nbBits, _val)); \
	}\


static void lsr_read_group_content(GF_LASeRCodec *lsr, GF_Node *elt, Bool skip_object_content);
static void lsr_read_group_content_post_init(GF_LASeRCodec *lsr, SVG_Element *elt, Bool skip_init);
static GF_Err lsr_read_command_list(GF_LASeRCodec *lsr, GF_List *comList, SVG_Element *cond, Bool first_imp);
static GF_Err lsr_decode_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list);
static void lsr_read_path_type(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, SVG_PathData *path, const char *name);
static void lsr_read_point_sequence(GF_LASeRCodec *lsr, GF_List *pts, const char *name);
static Bool lsr_setup_smil_anim(GF_LASeRCodec *lsr, SVG_Element *anim, SVG_Element *anim_parent);

GF_EXPORT
GF_LASeRCodec *gf_laser_decoder_new(GF_SceneGraph *graph)
{
	GF_LASeRCodec *tmp;
	GF_SAFEALLOC(tmp, GF_LASeRCodec);
	if (!tmp) return NULL;
	tmp->streamInfo = gf_list_new();
	tmp->font_table = gf_list_new();
	tmp->defered_hrefs = gf_list_new();
	tmp->defered_listeners = gf_list_new();
	tmp->defered_anims = gf_list_new();
	tmp->unresolved_commands = gf_list_new();
	tmp->sg = graph;
	return tmp;
}

GF_EXPORT
void gf_laser_decoder_del(GF_LASeRCodec *codec)
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
#if 0
	while (gf_list_count(codec->defered_hrefs)) {
		XMLRI *iri = (XMLRI *)gf_list_last(codec->defered_hrefs);
		gf_list_rem_last(codec->defered_hrefs);
		if (iri->string) gf_free(iri->string);
		iri->string = NULL;
	}
#endif
	gf_list_del(codec->defered_hrefs);
	gf_list_del(codec->defered_anims);
	gf_list_del(codec->defered_listeners);
	gf_list_del(codec->unresolved_commands);
	gf_free(codec);
}

static LASeRStreamInfo *lsr_get_stream(GF_LASeRCodec *codec, u16 ESID)
{
	u32 i=0;
	LASeRStreamInfo *ptr;
	while ((ptr = (LASeRStreamInfo *)gf_list_enum(codec->streamInfo, &i))) {
		if (!ESID || (ptr->ESID==ESID)) return ptr;
	}
	return NULL;
}


GF_EXPORT
GF_Err gf_laser_decoder_configure_stream(GF_LASeRCodec *codec, u16 ESID, u8 *dsi, u32 dsi_len)
{
	LASeRStreamInfo *info;
	GF_BitStream *bs;
	if (lsr_get_stream(codec, ESID) != NULL) return GF_BAD_PARAM;
	GF_SAFEALLOC(info, LASeRStreamInfo);
	if (!info) return GF_OUT_OF_MEM;
	info->ESID = ESID;
	bs = gf_bs_new(dsi, dsi_len, GF_BITSTREAM_READ);

	info->cfg.profile = gf_bs_read_int(bs, 8);
	info->cfg.level = gf_bs_read_int(bs, 8);
	/*info->cfg.reserved = */ gf_bs_read_int(bs, 3);
	info->cfg.pointsCodec = gf_bs_read_int(bs, 2);
	info->cfg.pathComponents = gf_bs_read_int(bs, 4);
	info->cfg.fullRequestHost = gf_bs_read_int(bs, 1);
	if (gf_bs_read_int(bs, 1)) {
		info->cfg.time_resolution = gf_bs_read_int(bs, 16);
	} else {
		info->cfg.time_resolution = 1000;
	}
	info->cfg.colorComponentBits = gf_bs_read_int(bs, 4);
	info->cfg.colorComponentBits += 1;
	info->cfg.resolution = gf_bs_read_int(bs, 4);
	if (info->cfg.resolution>7) info->cfg.resolution -= 16;
	info->cfg.coord_bits = gf_bs_read_int(bs, 5);
	info->cfg.scale_bits_minus_coord_bits = gf_bs_read_int(bs, 4);
	info->cfg.newSceneIndicator = gf_bs_read_int(bs, 1);
	/*reserved*/ gf_bs_read_int(bs, 3);
	info->cfg.extensionIDBits = gf_bs_read_int(bs, 4);
	/*we ignore the rest*/
	gf_list_add(codec->streamInfo, info);
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_laser_decoder_remove_stream(GF_LASeRCodec *codec, u16 ESID)
{
	u32 i, count;
	count = gf_list_count(codec->streamInfo);
	for (i=0; i<count; i++) {
		LASeRStreamInfo *ptr = (LASeRStreamInfo *) gf_list_get(codec->streamInfo, i);
		if (ptr->ESID==ESID) {
			gf_free(ptr);
			gf_list_rem(codec->streamInfo, i);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}


void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);

void lsr_end_of_stream(void *co)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] memory overread - corrupted decoding\n"));
	((GF_LASeRCodec *) co)->last_error = GF_NON_COMPLIANT_BITSTREAM;
}

GF_EXPORT
GF_Err gf_laser_decode_au(GF_LASeRCodec *codec, u16 ESID, const u8 *data, u32 data_len)
{
	GF_Err e;
	if (!codec || !data || !data_len) return GF_BAD_PARAM;

	codec->info = lsr_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits_minus_coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution >= 0)
		codec->res_factor = INT2FIX(1<<codec->info->cfg.resolution);
	else
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1 << (-codec->info->cfg.resolution)) );

	codec->bs = gf_bs_new(data, data_len, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(codec->bs, lsr_end_of_stream, codec);
	codec->memory_dec = GF_FALSE;
	e = lsr_decode_laser_unit(codec, NULL);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return e;
}

GF_EXPORT
GF_Err gf_laser_decode_command_list(GF_LASeRCodec *codec, u16 ESID, u8 *data, u32 data_len, GF_List *com_list)
{
	GF_Err e;
	u32 i;
	if (!codec || !data || !data_len) return GF_BAD_PARAM;

	codec->info = lsr_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits_minus_coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution >= 0)
		codec->res_factor = INT2FIX(1<<codec->info->cfg.resolution);
	else
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1 << (-codec->info->cfg.resolution)) );

	codec->bs = gf_bs_new(data, data_len, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(codec->bs, lsr_end_of_stream, codec);
	codec->memory_dec = GF_TRUE;
	e = lsr_decode_laser_unit(codec, com_list);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	if (e) return e;

	for (i=0; i<gf_list_count(codec->unresolved_commands); i++) {
		GF_Command *com = (GF_Command *)gf_list_get(codec->unresolved_commands, i);
		assert(!com->node);
		com->node = gf_sg_find_node(codec->sg, com->RouteID);
		if (com->node) {
			gf_node_register(com->node, NULL);
			com->RouteID = 0;
			gf_list_rem(codec->unresolved_commands, i);
			i--;
		}
	}
	return GF_OK;
}

GF_EXPORT
void gf_laser_decoder_set_clock(GF_LASeRCodec *codec, Double (*GetSceneTime)(void *st_cbk), void *st_cbk )
{
	codec->GetSceneTime = GetSceneTime;
	codec->cbk = st_cbk;
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
	if (name) GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", name, nb_tot, val));
	return val;
}
static u32 lsr_read_vluimsbf8(GF_LASeRCodec *lsr, const char *name)
{
	u32 nb_words = 0;
	u32 nb_tot, nb_bits, val;

	while (gf_bs_read_int(lsr->bs, 1)) nb_words++;
	nb_words++;
	nb_tot = nb_words;
	nb_bits = nb_words*7;
	nb_tot += nb_bits;
	val = gf_bs_read_int(lsr->bs, nb_bits);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%d\n", name, nb_tot, val));
	return val;
}

static void lsr_read_extension(GF_LASeRCodec *lsr, const char *name)
{
	u32 len = lsr_read_vluimsbf5(lsr, name);
#if 0
	*out_data = gf_malloc(sizeof(char)*len);
	gf_bs_read_data(lsr->bs, *out_data, len);
	*out_len = len;
#else
	while (len && gf_bs_available(lsr->bs) ) {
		gf_bs_read_int(lsr->bs, 8);
		len--;
	}
	if (len) lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;
#endif
}

static void lsr_read_extend_class(GF_LASeRCodec *lsr, char **out_data, u32 *out_len, const char *name)
{
	u32 len;
	GF_LSR_READ_INT(lsr, len, lsr->info->cfg.extensionIDBits, "reserved");
	len = lsr_read_vluimsbf5(lsr, "len");
//	while (len) gf_bs_read_int(lsr->bs, 1);
	gf_bs_read_long_int(lsr->bs, len);
	if (out_data) *out_data = NULL;
	if (out_len) *out_len = 0;
}

static void lsr_read_private_element_container(GF_LASeRCodec *lsr)
{
	u32 val, len;
	GF_LSR_READ_INT(lsr, val, 4, "ch4");
	switch (val) {
	/*privateAnyXMLElement*/
	case 0:
		len = lsr_read_vluimsbf5(lsr, "len");
		gf_bs_skip_bytes(lsr->bs, len);
		break;
	/*privateOpaqueElement*/
	case 1:
		len = lsr_read_vluimsbf5(lsr, "len");
		gf_bs_skip_bytes(lsr->bs, len);
		break;
	/*element_any*/
	case 2:
		lsr_read_extend_class(lsr, NULL, 0, "reserved");
		break;
	/*attr_custom_extension*/
	default:
		len = lsr_read_vluimsbf5(lsr, "len");
		gf_bs_skip_bytes(lsr->bs, len);
		break;
	}
}

static void lsr_read_private_attribute_container(GF_LASeRCodec *lsr)
{
	u32 val;
	do {
		u32 skip_len;
		GF_LSR_READ_INT(lsr, val, 2, "privateDataType");
		skip_len = lsr_read_vluimsbf5(lsr, "skipLen");
		gf_bs_align(lsr->bs);
		/*just skip data*/
#if 1
		if (skip_len>gf_bs_available(lsr->bs)) {
			lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;
			return;
		}
		gf_bs_skip_bytes(lsr->bs, skip_len);
#else
		switch (val) {
		/*private data of type "anyXML"*/
		case 0:
			count = lsr_read_vluimsbf5(lsr, "count");
			for (i=0; i<count; i++) {
				privateAttribute(0) attr[i];
			}
			break;
		case 1:
			/*TODO FIXME - nameSpaceIndexBits is not defined in the spec*/
			uint(nameSpaceIndexBits) nameSpaceIndex;
			gf_bs_align(lsr->bs);
			byte[skipLen - ((nameSpaceIndexBits+7)%8)] data;
			break;
		default:
			/*TODO - spec is wrong here (typo, "len" instead of "skipLen" )*/
			gf_bs_skip_bytes(skipLen);
			break;
		}
#endif
		gf_bs_align(lsr->bs);
		GF_LSR_READ_INT(lsr, val, 1, "hasMorePrivateData");
	} while (val);
}

static void lsr_read_any_attribute(GF_LASeRCodec *lsr, GF_Node *node, Bool skippable)
{
	u32 val = 1;
	if (skippable) GF_LSR_READ_INT(lsr, val, 1, "has_attrs");
	if (val) {
		do {
			GF_LSR_READ_INT(lsr, val, lsr->info->cfg.extensionIDBits, "reserved");
			val = lsr_read_vluimsbf5(lsr, "len");//len in BITS
			GF_LSR_READ_INT(lsr, val, val, "reserved_val");
			GF_LSR_READ_INT(lsr, val, 1, "hasNextExtension");
		} while (val);
	}
}

static void lsr_read_object_content(GF_LASeRCodec *lsr, SVG_Element *elt)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "has_private_attr");
	if (val) lsr_read_private_attribute_container(lsr);
}

static void lsr_read_codec_IDREF(GF_LASeRCodec *lsr, XMLRI *href, const char *name)
{
	GF_Node *n;
	u32 flag;
	u32 nID = 1+lsr_read_vluimsbf5(lsr, name);

	GF_LSR_READ_INT(lsr, flag, 1, "reserved");
	if (flag) {
		u32 len = lsr_read_vluimsbf5(lsr, "len");
		GF_LSR_READ_INT(lsr, flag, len, "reserved");
	}

	n = gf_sg_find_node(lsr->sg, nID);
	if (!n) {
		char NodeID[1024];
		sprintf(NodeID, "N%d", nID-1);
		href->string = gf_strdup(NodeID);
		if (href->type!=0xFF)
			gf_list_add(lsr->defered_hrefs, href);
		href->type = XMLRI_ELEMENTID;
		return;
	}
	href->target = (SVG_Element *)n;
	href->type = XMLRI_ELEMENTID;
	gf_node_register_iri(lsr->sg, href);
}

static u32 lsr_read_codec_IDREF_command(GF_LASeRCodec *lsr, const char *name)
{
	u32 flag;
	u32 nID = 1+lsr_read_vluimsbf5(lsr, name);

	GF_LSR_READ_INT(lsr, flag, 1, "reserved");
	if (flag) {
		u32 len = lsr_read_vluimsbf5(lsr, "len");
		GF_LSR_READ_INT(lsr, flag, len, "reserved");
	}
	return nID;
}

static Fixed lsr_read_fixed_16_8(GF_LASeRCodec *lsr, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 24, name);
	if (val & (1<<23)) {
		s32 res = val - (1<<24);
#ifdef GPAC_FIXED_POINT
		return res*256;
#else
		return INT2FIX(res) / 256;
#endif
	} else {
#ifdef GPAC_FIXED_POINT
		return val*256;
#else
		return INT2FIX(val) / 256;
#endif
	}
}

static void lsr_read_fixed_16_8i(GF_LASeRCodec *lsr, SVG_Number *n, const char *name)
{
	s32 val;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (val) {
		n->type=SVG_NUMBER_INHERIT;
	} else {
		n->type=SVG_NUMBER_VALUE;
		n->value = lsr_read_fixed_16_8(lsr, name);
	}
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


static void lsr_read_line_increment_type(GF_LASeRCodec *lsr, SVG_Number *li, const char *name)
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

static void lsr_read_byte_align_string(GF_LASeRCodec *lsr, char **str, const char *name)
{
	u32 len;
	gf_bs_align(lsr->bs);
	len = lsr_read_vluimsbf8(lsr, "len");
	if (str) {
		if (*str) gf_free(*str);
		*str = NULL;
		if (len) {
			if (len > gf_bs_available(lsr->bs) ) {
				lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;
				return;
			}
			*str = (char*)gf_malloc(sizeof(char)*(len+1));
			gf_bs_read_data(lsr->bs, *str, len);
			(*str) [len] = 0;
		}
	} else {
		while (len) {
			gf_bs_read_int(lsr->bs, 8);
			len--;
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] %s\t\t%d\t\t%s\n", name, 8*len, str ? *str : ""));
}

static void lsr_read_text_content(GF_LASeRCodec *lsr, GF_Node *elt)
{
	char *str = NULL;
	lsr_read_byte_align_string(lsr, &str, "textContent");
	if (!str) return;
	gf_dom_add_text_node(elt, str);
}

static void lsr_read_byte_align_string_list(GF_LASeRCodec *lsr, GF_List *l, const char *name, Bool is_iri, Bool is_font)
{
	XMLRI *iri;
	char *text, *sep, *sep2, *cur;
	while (gf_list_count(l)) {
		char *str = (char *)gf_list_last(l);
		gf_list_rem_last(l);
		gf_free(str);
	}
	text = NULL;
	lsr_read_byte_align_string(lsr, &text, name);
	cur = text;
	while (cur) {
		sep = strchr(cur, '\'');
		if (!sep && is_font) {
			sep = strchr(cur, ',');
			if (!sep) sep = strchr(cur, ';');
		}
		if (!sep) {
			if (is_iri) {
				GF_SAFEALLOC(iri, XMLRI);
				iri->string = gf_strdup(cur);
				iri->type = XMLRI_STRING;
				gf_list_add(l, iri);
			} else {
				gf_list_add(l, gf_strdup(cur));
			}
			break;
		}
		sep2 = strchr(sep + 1, '\'');
		if (!sep2 && !is_font) {
			if (is_iri) {
				GF_SAFEALLOC(iri, XMLRI);
				iri->string = gf_strdup(cur);
				iri->type = XMLRI_STRING;
				gf_list_add(l, iri);
			} else {
				gf_list_add(l, gf_strdup(cur));
			}
			break;
		}
		if (sep2)
			sep2[0] = 0;
		else
			sep[0] = 0;
		if (is_iri) {
			GF_SAFEALLOC(iri, XMLRI);
			iri->string = gf_strdup(sep+1);
			iri->type = XMLRI_STRING;
			gf_list_add(l, iri);
		} else {
			gf_list_add(l, gf_strdup(sep+1));
		}
		if (sep2) {
			sep2[0] = '\'';
			cur = sep2 + 1;
		} else {
			sep[0] = ';';
			cur = sep + 1;
		}
	}
	gf_free(text);
}

static void lsr_read_any_uri(GF_LASeRCodec *lsr, XMLRI *iri, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "hasUri");
	if (val) {
		char *s = NULL;
		iri->type=XMLRI_STRING;
		if (iri->string) {
			gf_free(iri->string);
			iri->string = NULL;
		}
		lsr_read_byte_align_string(lsr, &s, "uri");
		GF_LSR_READ_INT(lsr, val, 1, "hasData");
		if (!val) {
			iri->string = s;
		} else {
			u32 len_rad, len;
			len = lsr_read_vluimsbf5(lsr, "len");
			len_rad = s ? (u32) strlen(s) : 0;
			iri->string = (char*)gf_malloc(sizeof(char)*(len_rad+1+len+1));
			iri->string[0] = 0;
			if (s) {
				strcpy(iri->string, s);
				gf_free(s);
			}
			strcat(iri->string, ",");
			gf_bs_read_data(lsr->bs, iri->string + len_rad + 1, len);
			iri->string[len_rad + 1 + len] = 0;
		}
	}
	GF_LSR_READ_INT(lsr, val, 1, "hasID");
	if (val) lsr_read_codec_IDREF(lsr, iri, "idref");

	GF_LSR_READ_INT(lsr, val, 1, "hasStreamID");
	if (val) {
		iri->type = XMLRI_STREAMID;
		iri->lsr_stream_id = lsr_read_vluimsbf5(lsr, name);
		GF_LSR_READ_INT(lsr, val, 1, "reserved");
		if (val) {
			u32 len = lsr_read_vluimsbf5(lsr, "len");
			GF_LSR_READ_INT(lsr, val, len, "reserved");
		}
	}
}

static void lsr_read_paint(GF_LASeRCodec *lsr, SVG_Paint *paint, const char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "hasIndex");
	if (val) {
		GF_LSR_READ_INT(lsr, val, lsr->colorIndexBits, name);
		lsr_get_color(lsr, val, &paint->color);
		paint->type = SVG_PAINT_COLOR;
		paint->color.type = 0;
	} else {
		GF_LSR_READ_INT(lsr, val, 2, "enum");
		switch (val) {
		case 0:
			GF_LSR_READ_INT(lsr, val, 2, "choice");
			switch (val) {
			case 0:
				paint->type = SVG_PAINT_INHERIT;
				break;
			case 1:
				paint->type = SVG_PAINT_COLOR;
				paint->color.type = SVG_COLOR_CURRENTCOLOR;
				break;
			default:
				paint->type = SVG_PAINT_NONE;
				break;
			}
			break;
		case 1:
		{
			XMLRI iri;
			memset(&iri, 0, sizeof(XMLRI));
			iri.type = 0xFF;
			lsr_read_any_uri(lsr, &iri, name);
			gf_node_unregister_iri(lsr->sg, &iri);
			paint->type = SVG_PAINT_URI;
			if (iri.string) {
				paint->type = SVG_PAINT_URI;
				paint->iri.type = XMLRI_STRING;
				paint->iri.string = iri.string;
			} else if (iri.target) {
				paint->iri.type = XMLRI_ELEMENTID;
				paint->iri.target = iri.target;
			}
		}
		break;
		case 2:
		{
			char *sysPaint=NULL;
			lsr_read_byte_align_string(lsr, &sysPaint, "systemsPaint");
			if (sysPaint) {
				paint->type = SVG_PAINT_COLOR;
				paint->color.type = gf_svg_get_system_paint_server_type(sysPaint);
				gf_free(sysPaint);
			}
		}
		break;
		case 3:
			lsr_read_extension(lsr, name);
			break;
		}
	}
}

static void lsr_read_string_attribute(GF_LASeRCodec *lsr, GF_Node *elt, u32 tag, char *name)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (val) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, tag, GF_TRUE, GF_FALSE, &info);
		lsr_read_byte_align_string(lsr, info.far_ptr, name);
	}
}
static void lsr_read_id(GF_LASeRCodec *lsr, GF_Node *n)
{
	GF_FieldInfo info;
	u32 val, id, i, count;
	char *name;
	GF_LSR_READ_INT(lsr, val, 1, "has_id");
	if (!val) return;

	name = NULL;
	id = 1+lsr_read_vluimsbf5(lsr, "ID");
	gf_node_set_id(n, id, name);

	GF_LSR_READ_INT(lsr, val, 1, "reserved");
	/*currently not used*/
	if (val) {
		u32 len = lsr_read_vluimsbf5(lsr, "len");
		GF_LSR_READ_INT(lsr, val, len, "reserved");
	}

	/*update all pending HREFs*/
	count = gf_list_count(lsr->defered_hrefs);
	for (i=0; i<count; i++) {
		XMLRI *href = (XMLRI *)gf_list_get(lsr->defered_hrefs, i);
		char *str_id = href->string;
		if (str_id[0] == '#') str_id++;
		/*skip 'N'*/
		str_id++;
		if (id == (1 + (u32) atoi(str_id))) {
			href->target = (SVG_Element*) n;
			gf_free(href->string);
			href->string = NULL;
			gf_list_rem(lsr->defered_hrefs, i);
			i--;
			count--;
		}
	}

	/*update unresolved listeners*/
	count = gf_list_count(lsr->defered_listeners);
	for (i=0; i<count; i++) {
		GF_Node *par;
		XMLRI *observer = NULL;
		GF_Node *listener = (GF_Node *)gf_list_get(lsr->defered_listeners, i);

		par = NULL;
		if (gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_observer, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			observer = (XMLRI*)info.far_ptr;
			if (observer->type == XMLRI_ELEMENTID) {
				if (!observer->target) continue;
				else par = (GF_Node*)observer->target;
			}
		}
		if (gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_target, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			if (((XMLRI*)info.far_ptr)->type == XMLRI_ELEMENTID) {
				if (!((XMLRI*)info.far_ptr)->target) continue;
				else if (!par) par = (GF_Node*)((XMLRI*)info.far_ptr)->target;
			}
		}
		/*FIXME - double check with XML events*/
		if (!par && !observer) {
			if (gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_event, GF_FALSE, GF_FALSE, &info) == GF_OK) {
				XMLEV_Event *ev = (XMLEV_Event *)info.far_ptr;
				/*all non-UI get attched to root*/
				if (ev && (ev->type>GF_EVENT_MOUSEWHEEL)) {
					par = (GF_Node*) lsr->current_root;
				}
			}
		}

		assert(par);
		gf_node_dom_listener_add(par, listener);
		gf_list_rem(lsr->defered_listeners, i);
		i--;
		count--;
	}

	/*update all pending animations*/
	count = gf_list_count(lsr->defered_anims);
	for (i=0; i<count; i++) {
		SVG_Element *elt = (SVG_Element *)gf_list_get(lsr->defered_anims, i);
		if (lsr_setup_smil_anim(lsr, elt, NULL)) {
			gf_list_rem(lsr->defered_anims, i);
			i--;
			count--;
			gf_node_init((GF_Node*)elt);
		}
	}
}

static Fixed lsr_translate_coords(GF_LASeRCodec *lsr, u32 val, u32 nb_bits)
{
#ifdef GPAC_FIXED_POINT
	if (val >> (nb_bits-1) ) {
		s32 neg = (s32) val - (1<<nb_bits);
		if (neg < -FIX_ONE / 2)
			return 2 * gf_divfix(INT2FIX(neg/2), lsr->res_factor);
		return gf_divfix(INT2FIX(neg), lsr->res_factor);
	} else {
		if (val > FIX_ONE / 2)
			return 2 * gf_divfix(INT2FIX(val/2), lsr->res_factor);
		return gf_divfix(INT2FIX(val), lsr->res_factor);
	}
#else
	if (val >> (nb_bits-1) ) {
		s32 neg = (s32) val - (1<<nb_bits);
		return gf_divfix(INT2FIX(neg), lsr->res_factor);
	} else {
		return gf_divfix(INT2FIX(val), lsr->res_factor);
	}
#endif
}

static Fixed lsr_translate_scale(GF_LASeRCodec *lsr, u32 val)
{
	if (val >> (lsr->coord_bits-1) ) {
		s32 v = val - (1<<lsr->coord_bits);
		return INT2FIX(v) / 256 ;
	} else {
		return INT2FIX(val) / 256;
	}
}
static void lsr_read_matrix(GF_LASeRCodec *lsr, SVG_Transform *mx)
{
	u32 flag;
	gf_mx2d_init(mx->mat);
	mx->is_ref = 0;
	GF_LSR_READ_INT(lsr, flag, 1, "isNotMatrix");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "isRef");
		if (flag) {
			GF_LSR_READ_INT(lsr, flag, 1, "hasXY");
			if (flag) {
				mx->mat.m[2] = lsr_read_fixed_16_8(lsr, "valueX");
				mx->mat.m[5] = lsr_read_fixed_16_8(lsr, "valueY");
			}
		} else {
			lsr_read_extension(lsr, "ext");
		}
	} else {
		lsr->coord_bits += lsr->scale_bits;
		GF_LSR_READ_INT(lsr, flag, 1, "xx_yy_present");
		if (flag) {
			GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "xx");
			mx->mat.m[0] = lsr_translate_scale(lsr, flag);
			GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "yy");
			mx->mat.m[4] = lsr_translate_scale(lsr, flag);
		} else {
			mx->mat.m[0] = mx->mat.m[4] = FIX_ONE;
		}
		GF_LSR_READ_INT(lsr, flag, 1, "xy_yx_present");
		if (flag) {
			GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "xy");
			mx->mat.m[1] = lsr_translate_scale(lsr, flag);
			GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "yx");
			mx->mat.m[3] = lsr_translate_scale(lsr, flag);
		}

		GF_LSR_READ_INT(lsr, flag, 1, "xz_yz_present");
		if (flag) {
			GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "xz");
			mx->mat.m[2] = lsr_translate_coords(lsr, flag, lsr->coord_bits);
			GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "yz");
			mx->mat.m[5] = lsr_translate_coords(lsr, flag, lsr->coord_bits);
		}
		lsr->coord_bits -= lsr->scale_bits;
	}
}

static Fixed lsr_read_fixed_clamp(GF_LASeRCodec *lsr, const char *name)
{
	s32 val;
	GF_LSR_READ_INT(lsr, val, 8, name);
	return INT2FIX(val) / 255;
}

static void lsr_read_focus(GF_LASeRCodec *lsr, SVG_Focus *foc, const char *name)
{
	u32 flag;

	if (foc->target.string) {
		gf_free(foc->target.string);
		foc->target.string = NULL;
	}
	if (foc->target.target) foc->target.target = NULL;
	gf_node_unregister_iri(lsr->sg, &foc->target);

	GF_LSR_READ_INT(lsr, flag, 1, "isEnum");
	if (flag) {
		GF_LSR_READ_INT(lsr, foc->type, 1, "enum");
	} else {
		foc->type = SVG_FOCUS_IRI;
		lsr_read_codec_IDREF(lsr, &foc->target, "id");
	}
}

static void lsr_restore_base(GF_LASeRCodec *lsr, SVG_Element *elt, SVG_Element *base, Bool reset_fill, Bool reset_stroke)
{
	GF_Err e;
	GF_FieldInfo f_base, f_clone;
	SVGAttribute *att;

	/*clone all propertie from base*/
	att = base->attributes;
	while (att) {
		Bool is_fill, is_stroke;
		is_fill = is_stroke = GF_FALSE;
		switch (att->tag) {
		/*for all properties*/
		case TAG_SVG_ATT_fill:
			is_fill = GF_TRUE;
			break;
		case TAG_SVG_ATT_stroke:
			is_stroke = GF_TRUE;
			break;
		case TAG_SVG_ATT_audio_level:
		case TAG_SVG_ATT_color:
		case TAG_SVG_ATT_color_rendering:
		case TAG_SVG_ATT_display:
		case TAG_SVG_ATT_display_align:
		case TAG_SVG_ATT_fill_opacity:
		case TAG_SVG_ATT_fill_rule:
		case TAG_SVG_ATT_font_family:
		case TAG_SVG_ATT_font_size:
		case TAG_SVG_ATT_font_style:
		case TAG_SVG_ATT_font_variant:
		case TAG_SVG_ATT_font_weight:
		case TAG_SVG_ATT_image_rendering:
		case TAG_SVG_ATT_line_increment:
		case TAG_SVG_ATT_opacity:
		case TAG_SVG_ATT_pointer_events:
		case TAG_SVG_ATT_shape_rendering:
		case TAG_SVG_ATT_solid_color:
		case TAG_SVG_ATT_solid_opacity:
		case TAG_SVG_ATT_stop_color:
		case TAG_SVG_ATT_stop_opacity:
		case TAG_SVG_ATT_stroke_dasharray:
		case TAG_SVG_ATT_stroke_dashoffset:
		case TAG_SVG_ATT_stroke_linecap:
		case TAG_SVG_ATT_stroke_linejoin:
		case TAG_SVG_ATT_stroke_miterlimit:
		case TAG_SVG_ATT_stroke_opacity:
		case TAG_SVG_ATT_stroke_width:
		case TAG_SVG_ATT_text_align:
		case TAG_SVG_ATT_text_anchor:
		case TAG_SVG_ATT_text_rendering:
		case TAG_SVG_ATT_vector_effect:
		case TAG_SVG_ATT_viewport_fill:
		case TAG_SVG_ATT_viewport_fill_opacity:
		case TAG_SVG_ATT_visibility:
		/*and xml:_class*/
		case TAG_SVG_ATT__class:
		case TAG_SVG_ATT_externalResourcesRequired:
			break;

		/*pathLength for path*/
		case TAG_SVG_ATT_pathLength:
			break;
		/*rx & ry for rect*/
		case TAG_SVG_ATT_rx:
		case TAG_SVG_ATT_ry:
			if (base->sgprivate->tag!=TAG_SVG_rect) {
				att = att->next;
				continue;
			}
			break;
		/*x & y for use*/
		case TAG_SVG_ATT_x:
		case TAG_SVG_ATT_y:
			if (base->sgprivate->tag!=TAG_SVG_use) {
				att = att->next;
				continue;
			}
			break;
		/*editable & rotate for text*/
		case TAG_SVG_ATT_editable:
		case TAG_SVG_ATT_rotate:
			if (base->sgprivate->tag!=TAG_SVG_text) {
				att = att->next;
				continue;
			}
			break;
		case TAG_SVG_ATT_transform:
			break;
		default:
			att = att->next;
			continue;
		}
		/*clone field*/
		e = gf_node_get_attribute_by_tag((GF_Node*)elt, att->tag, GF_TRUE, GF_FALSE, &f_clone);
		if (e) goto err_exit;
		f_base.fieldIndex = att->tag;
		f_base.fieldType = att->data_type;
		f_base.far_ptr = att->data;
		e = gf_svg_attributes_copy(&f_clone, &f_base, GF_FALSE);
		if (e) goto err_exit;

		if (is_fill && reset_fill) {
			SVG_Paint*p = (SVG_Paint*)f_clone.far_ptr;
			if (p->iri.string) gf_free(p->iri.string);
			memset(p, 0, sizeof(SVG_Paint));
		}
		if (is_stroke && reset_stroke) {
			SVG_Paint*p = (SVG_Paint*)f_clone.far_ptr;
			if (p->iri.string) gf_free(p->iri.string);
			memset(p, 0, sizeof(SVG_Paint));
		}
		att = att->next;
	}
	return;

err_exit:
	lsr->last_error = e;
}


static u32 lsr_to_dom_key(u32 lsr_k)
{
	switch (lsr_k) {
	case 0:
		return GF_KEY_STAR;
	case 1:
		return GF_KEY_0;
	case 2:
		return GF_KEY_1;
	case 3:
		return GF_KEY_2;
	case 4:
		return GF_KEY_3;
	case 5:
		return GF_KEY_4;
	case 6:
		return GF_KEY_5;
	case 7:
		return GF_KEY_6;
	case 8:
		return GF_KEY_7;
	case 9:
		return GF_KEY_8;
	case 10:
		return GF_KEY_9;
	case 12:
		return GF_KEY_DOWN;
	case 14:
		return GF_KEY_LEFT;
	case 16:
		return GF_KEY_RIGHT;
	case 20:
		return GF_KEY_UP;
	/*WHAT IS ANY_KEY (11) ??*/
	case 13:
		return GF_KEY_ENTER;
	case 15:
		return GF_KEY_ESCAPE;
	case 17:
		return GF_KEY_NUMBER;
	case 18:
		return GF_KEY_CELL_SOFT1;
	case 19:
		return GF_KEY_CELL_SOFT2;
	default:
		/*use '*' by default ... */
		return 0;
	}
}

static void lsr_read_event_type(GF_LASeRCodec *lsr, XMLEV_Event *evtType)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "choice");
	if (!flag) {
		char *evtName, *sep;
		evtName = NULL;
		lsr_read_byte_align_string(lsr, &evtName, "evtString");
		evtType->type = evtType->parameter = 0;
		if (evtName) {
			sep = strchr(evtName, '(');
			if (sep) {
				char *param;
				sep[0] = 0;
				evtType->type = gf_dom_event_type_by_name(evtName);
				sep[0] = '(';
				param = sep+1;
				sep = strchr(evtName, ')');
				if (sep) sep[0]=0;
				if (evtType->type==GF_EVENT_REPEAT) {
					evtType->parameter = atoi(param);
				} else {
					evtType->parameter = gf_dom_get_key_type(param);
				}
			} else {
				evtType->type = gf_dom_event_type_by_name(evtName);
			}
			gf_free(evtName);
		}
	} else {
		evtType->parameter = 0;
		GF_LSR_READ_INT(lsr, flag, 6, "event");
		switch (flag) {
		case LSR_EVT_abort:
			evtType->type = GF_EVENT_ABORT;
			break;
		case LSR_EVT_accessKey:
			evtType->type = GF_EVENT_KEYDOWN;
			break;
		case LSR_EVT_activate:
			evtType->type = GF_EVENT_ACTIVATE;
			break;
		case LSR_EVT_activatedEvent:
			evtType->type = GF_EVENT_ACTIVATED;
			break;
		case LSR_EVT_beginEvent:
			evtType->type = GF_EVENT_BEGIN_EVENT;
			break;
		case LSR_EVT_click:
			evtType->type = GF_EVENT_CLICK;
			break;
		case LSR_EVT_deactivatedEvent:
			evtType->type = GF_EVENT_DEACTIVATED;
			break;
		case LSR_EVT_endEvent:
			evtType->type = GF_EVENT_END_EVENT;
			break;
		case LSR_EVT_error:
			evtType->type = GF_EVENT_ERROR;
			break;
		case LSR_EVT_executionTime:
			evtType->type = GF_EVENT_EXECUTION_TIME;
			break;
		case LSR_EVT_focusin:
			evtType->type = GF_EVENT_FOCUSIN;
			break;
		case LSR_EVT_focusout:
			evtType->type = GF_EVENT_FOCUSOUT;
			break;
		case LSR_EVT_keydown:
			evtType->type = GF_EVENT_KEYDOWN;
			break;
		case LSR_EVT_keyup:
			evtType->type = GF_EVENT_KEYUP;
			break;
		case LSR_EVT_load:
			evtType->type = GF_EVENT_LOAD;
			break;
		case LSR_EVT_longAccessKey:
			evtType->type = GF_EVENT_LONGKEYPRESS;
			break;
		case LSR_EVT_mousedown:
			evtType->type = GF_EVENT_MOUSEDOWN;
			break;
		case LSR_EVT_mousemove:
			evtType->type = GF_EVENT_MOUSEMOVE;
			break;
		case LSR_EVT_mouseout:
			evtType->type = GF_EVENT_MOUSEOUT;
			break;
		case LSR_EVT_mouseover:
			evtType->type = GF_EVENT_MOUSEOVER;
			break;
		case LSR_EVT_mouseup:
			evtType->type = GF_EVENT_MOUSEUP;
			break;
		case LSR_EVT_pause:
			evtType->type = GF_EVENT_PAUSE;
			break;
		case LSR_EVT_pausedEvent:
			evtType->type = GF_EVENT_PAUSED_EVENT;
			break;
		case LSR_EVT_play:
			evtType->type = GF_EVENT_PLAY;
			break;
		case LSR_EVT_repeatEvent:
			evtType->type = GF_EVENT_REPEAT_EVENT;
			break;
		case LSR_EVT_repeatKey:
			evtType->type = GF_EVENT_REPEAT_KEY;
			break;
		case LSR_EVT_resize:
			evtType->type = GF_EVENT_RESIZE;
			break;
		case LSR_EVT_resumedEvent:
			evtType->type = GF_EVENT_RESUME_EVENT;
			break;
		case LSR_EVT_scroll:
			evtType->type = GF_EVENT_SCROLL;
			break;
		case LSR_EVT_shortAccessKey:
			evtType->type = GF_EVENT_SHORT_ACCESSKEY;
			break;
		case LSR_EVT_textinput:
			evtType->type = GF_EVENT_TEXTINPUT;
			break;
		case LSR_EVT_unload:
			evtType->type = GF_EVENT_UNLOAD;
			break;
		case LSR_EVT_zoom:
			evtType->type = GF_EVENT_ZOOM;
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] Undefined LASeR event %d\n", flag));
			break;
		}
		switch (flag) {
		case LSR_EVT_accessKey:
		case LSR_EVT_longAccessKey:
		case LSR_EVT_repeatKey:
		case LSR_EVT_shortAccessKey:
			evtType->parameter = lsr_read_vluimsbf5(lsr, "keyCode");
			evtType->parameter  = lsr_to_dom_key(evtType->parameter);
			break;
		}
	}
}

static SMIL_Time *lsr_read_smil_time(GF_LASeRCodec *lsr, GF_Node *n)
{
	SMIL_Time *t;
	u32 val;

	GF_SAFEALLOC(t, SMIL_Time);
	if (!t) return NULL;
	t->type = GF_SMIL_TIME_CLOCK;

	GF_LSR_READ_INT(lsr, val, 1, "hasEvent");
	if (val) {
		t->type = GF_SMIL_TIME_EVENT;
		GF_LSR_READ_INT(lsr, val, 1, "hasIdentifier");
		if (val) {
			XMLRI iri;
			iri.type = 0xFF;
			iri.string = NULL;
			lsr_read_codec_IDREF(lsr, &iri, "idref");
			gf_node_unregister_iri(lsr->sg, &iri);
			if (iri.string) {
				t->element_id = iri.string;
			} else {
				t->element = (GF_Node *)iri.target;
			}
		}
		lsr_read_event_type(lsr, &t->event);
		if (t->event.type==GF_EVENT_EXECUTION_TIME) {
			t->type = GF_SMIL_TIME_CLOCK;
			t->clock = gf_node_get_scene_time(n);
		}
	}
	GF_LSR_READ_INT(lsr, val, 1, "hasClock");
	if (val) {
		u32 now;
		GF_LSR_READ_INT(lsr, val, 1, "sign");
		now = lsr_read_vluimsbf5(lsr, "value");
		t->clock = now;
		t->clock /= lsr->time_resolution;
		if (val) t->clock *= -1;
	}
	return t;
}

static void lsr_read_smil_times(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, SMIL_Times *times, const char *name, Bool skipable)
{
	GF_FieldInfo info;
	SMIL_Time *v;
	u32 val, i, count;

	if (skipable) {
		GF_LSR_READ_INT(lsr, val, 1, name);
		if (!val) return;
	}
	if (!times) {
		lsr->last_error = gf_node_get_attribute_by_tag(n, tag, GF_TRUE, GF_FALSE, &info);
		times = (SMIL_Times*)info.far_ptr;
	}

	while (gf_list_count(*times)) {
		v = (SMIL_Time *)gf_list_last(*times);
		gf_list_rem_last(*times);
		if (v->element_id) gf_free(v->element_id);
		gf_free(v);
	}

	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val) {
		GF_SAFEALLOC(v, SMIL_Time);
		if (v) {
			v->type = GF_SMIL_TIME_INDEFINITE;
			gf_list_add(*times, v);
		}
		return;
	}
	count = lsr_read_vluimsbf5(lsr, "count");
	for (i=0; i<count; i++) {
		v = lsr_read_smil_time(lsr, n);
		gf_list_add(*times, v);
	}
}

static void lsr_read_duration_ex(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, SMIL_Duration *smil, const char *name, Bool skipable)
{
	GF_FieldInfo info;
	u32 val = 1;

	if (skipable) {
		GF_LSR_READ_INT(lsr, val, 1, name);
		if (!val) return;
	}
	if (!smil) {
		lsr->last_error = gf_node_get_attribute_by_tag(n, tag, GF_TRUE, GF_FALSE, &info);
		if (lsr->last_error) return;
		smil = (SMIL_Duration *)info.far_ptr;
	}
	smil->type = 0;
	smil->clock_value=0;

	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val) {
		GF_LSR_READ_INT(lsr, smil->type, 2, "time");
	} else {
		Bool sign;
		u32 now;
		GF_LSR_READ_INT(lsr, sign, 1, "sign");
		now = lsr_read_vluimsbf5(lsr, "value");
		smil->clock_value = now;
		smil->clock_value /= lsr->time_resolution;
		if (sign) smil->clock_value *= -1;
		smil->type = SMIL_DURATION_DEFINED;
	}
}
static void lsr_read_duration(GF_LASeRCodec *lsr, GF_Node *n)
{
	lsr_read_duration_ex(lsr, n, TAG_SVG_ATT_dur, NULL, "dur", GF_TRUE);
}
/*TODO Add decent error checking...*/
static void lsr_read_rare_full(GF_LASeRCodec *lsr, GF_Node *n)
{
	GF_FieldInfo info;
	u32 i, nb_rare, field_rare;
	s32 field_tag;

	GF_LSR_READ_INT(lsr, nb_rare, 1, "has_rare");
	if (!nb_rare) return;
	GF_LSR_READ_INT(lsr, nb_rare, 6, "nbOfAttributes");

	for (i=0; i<nb_rare; i++) {
		GF_LSR_READ_INT(lsr, field_rare, 6, "attributeRARE");

		/*lsr extend*/
		if (field_rare==49) {
			u32 extID, len, j;
			while (1) {
				GF_LSR_READ_INT(lsr, extID, lsr->info->cfg.extensionIDBits, "extensionID");
				len = lsr_read_vluimsbf5(lsr, "len");
				if (extID==2) {
					GF_LSR_READ_INT(lsr, len, 2, "nbOfAttributes");
					for (j=0; j<len; j++) {
						GF_LSR_READ_INT(lsr, extID, 3, "attributeRARE");
						switch (extID) {
						case 0:
							lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_syncMaster, GF_TRUE, GF_FALSE, &info);
							GF_LSR_READ_INT(lsr, *(SVG_Boolean *)info.far_ptr, 1, "syncMaster");
							break;
						case 1:
							lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_focusHighlight, GF_TRUE, GF_FALSE, &info);
							GF_LSR_READ_INT(lsr, *(SVG_FocusHighlight *)info.far_ptr, 2, "focusHighlight");
							break;
						case 2:
							lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_initialVisibility, GF_TRUE, GF_FALSE, &info);
							GF_LSR_READ_INT(lsr, *(SVG_InitialVisibility *)info.far_ptr, 2, "initialVisibility");
							break;
						case 3:
							lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_fullscreen, GF_TRUE, GF_FALSE, &info);
							GF_LSR_READ_INT(lsr, *(SVG_Boolean *)info.far_ptr, 1, "fullscreen");
							break;
						case 4:
							lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_requiredFonts, GF_TRUE, GF_FALSE, &info);
							lsr_read_byte_align_string_list(lsr, *(GF_List **)info.far_ptr, "requiredFonts", GF_FALSE, GF_TRUE);
							break;
						}
					}
				} else {
					gf_bs_read_int(lsr->bs, len);
				}
				GF_LSR_READ_INT(lsr, extID, 1, "hasNextExtension");
				if (!extID) break;
			}
			continue;
		}
		field_tag = gf_lsr_rare_type_to_attribute(field_rare);
		if (field_tag==-1) {
			return;
		}
		lsr->last_error = gf_node_get_attribute_by_tag(n, field_tag, GF_TRUE, GF_FALSE, &info);
		if (!info.far_ptr) lsr->last_error = GF_NOT_SUPPORTED;
		if (lsr->last_error) return;

		switch (field_tag) {
		case TAG_SVG_ATT__class:
			lsr_read_byte_align_string(lsr, info.far_ptr, "class");
			break;
		/*properties*/
		case TAG_SVG_ATT_audio_level:
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "audio-level");
			break;
		case TAG_SVG_ATT_color:
			lsr_read_paint(lsr, (SVG_Paint *)info.far_ptr, "color");
			break;
		case TAG_SVG_ATT_color_rendering:
			GF_LSR_READ_INT(lsr, *(SVG_RenderingHint*)info.far_ptr, 2, "color-rendering");
			break;
		case TAG_SVG_ATT_display:
			GF_LSR_READ_INT(lsr, *(SVG_Display*)info.far_ptr, 5, "display");
			break;
		case TAG_SVG_ATT_display_align:
			GF_LSR_READ_INT(lsr, *(SVG_DisplayAlign*)info.far_ptr, 3, "display-align");
			break;
		case TAG_SVG_ATT_fill_opacity:
			((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "fill-opacity");
			break;
		case TAG_SVG_ATT_fill_rule:
			GF_LSR_READ_INT(lsr, *(SVG_FillRule*)info.far_ptr, 2, "fill-rule");
			break;
		case TAG_SVG_ATT_image_rendering:
			GF_LSR_READ_INT(lsr, *(SVG_RenderingHint*)info.far_ptr, 2, "image-rendering");
			break;
		case TAG_SVG_ATT_line_increment:
			lsr_read_line_increment_type(lsr, info.far_ptr, "line-increment");
			break;
		case TAG_SVG_ATT_pointer_events:
			GF_LSR_READ_INT(lsr, *(SVG_PointerEvents*)info.far_ptr, 4, "pointer-events");
			break;
		case TAG_SVG_ATT_shape_rendering:
			GF_LSR_READ_INT(lsr, *(SVG_RenderingHint*)info.far_ptr, 3, "shape-rendering");
			break;
		case TAG_SVG_ATT_solid_color:
			lsr_read_paint(lsr, info.far_ptr, "solid-color");
			break;
		case TAG_SVG_ATT_solid_opacity:
			((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "solid-opacity");
			break;
		case TAG_SVG_ATT_stop_color:
			lsr_read_paint(lsr, info.far_ptr, "stop-color");
			break;
		case TAG_SVG_ATT_stop_opacity:
			((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "stop-opacity");
			break;
		case TAG_SVG_ATT_stroke_dasharray:
		{
			u32 j, flag;
			SVG_StrokeDashArray *da = (SVG_StrokeDashArray *)info.far_ptr;
			GF_LSR_READ_INT(lsr, flag, 1, "dashArray");
			if (flag) {
				da->type=SVG_STROKEDASHARRAY_INHERIT;
			} else {
				da->type=SVG_STROKEDASHARRAY_ARRAY;
				da->array.count = lsr_read_vluimsbf5(lsr, "len");
				da->array.vals = (Fixed*)gf_malloc(sizeof(Fixed)*da->array.count);
				da->array.units = (u8*)gf_malloc(sizeof(u8)*da->array.count);
				for (j=0; j<da->array.count; j++) {
					da->array.vals[j] = lsr_read_fixed_16_8(lsr, "dash");
					da->array.units[j] = 0;
				}
			}
		}
		break;
		case TAG_SVG_ATT_stroke_dashoffset:
			lsr_read_fixed_16_8i(lsr, info.far_ptr, "dashOffset");
			break;

		case TAG_SVG_ATT_stroke_linecap:
			GF_LSR_READ_INT(lsr, *(SVG_StrokeLineCap*)info.far_ptr, 2, "stroke-linecap");
			break;
		case TAG_SVG_ATT_stroke_linejoin:
			GF_LSR_READ_INT(lsr, *(SVG_StrokeLineJoin*)info.far_ptr, 2, "stroke-linejoin");
			break;
		case TAG_SVG_ATT_stroke_miterlimit:
			lsr_read_fixed_16_8i(lsr, info.far_ptr, "miterLimit");
			break;
		case TAG_SVG_ATT_stroke_opacity:
			((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "stroke-opacity");
			break;
		case TAG_SVG_ATT_stroke_width:
			lsr_read_fixed_16_8i(lsr, info.far_ptr, "strokeWidth");
			break;
		case TAG_SVG_ATT_text_anchor:
			GF_LSR_READ_INT(lsr, *(SVG_TextAnchor*)info.far_ptr, 2, "text-achor");
			break;
		case TAG_SVG_ATT_text_rendering:
			GF_LSR_READ_INT(lsr, *(SVG_RenderingHint*)info.far_ptr, 3, "text-rendering");
			break;
		case TAG_SVG_ATT_viewport_fill:
			lsr_read_paint(lsr, info.far_ptr, "viewport-fill");
			break;
		case TAG_SVG_ATT_viewport_fill_opacity:
			((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "viewport-fill-opacity");
			break;
		case TAG_SVG_ATT_vector_effect:
			GF_LSR_READ_INT(lsr, *(SVG_VectorEffect*)info.far_ptr, 4, "vector-effect");
			break;
		case TAG_SVG_ATT_visibility:
			GF_LSR_READ_INT(lsr, *(SVG_Visibility*)info.far_ptr, 2, "visibility");
			break;
		case TAG_SVG_ATT_requiredExtensions:
			lsr_read_byte_align_string_list(lsr, *(GF_List**)info.far_ptr, "requiredExtensions", GF_TRUE, GF_FALSE);
			break;
		case TAG_SVG_ATT_requiredFormats:
			lsr_read_byte_align_string_list(lsr, *(GF_List**)info.far_ptr, "requiredFormats", GF_FALSE, GF_FALSE);
			break;
		case TAG_SVG_ATT_requiredFeatures:
		{
			u32 j, fcount = lsr_read_vluimsbf5(lsr, "count");
			for (j=0; j<fcount; j++) {
				u32 fval;
				GF_LSR_READ_INT(lsr, fval, 6, "feature");
			}
		}
		break;
		case TAG_SVG_ATT_systemLanguage:
			lsr_read_byte_align_string_list(lsr, *(GF_List**)info.far_ptr, "systemLanguage", GF_FALSE, GF_FALSE);
			break;
		case TAG_XML_ATT_base:
			lsr_read_byte_align_string(lsr, &((XMLRI*)info.far_ptr)->string, "xml:base");
			((XMLRI*)info.far_ptr)->type = XMLRI_STRING;
			break;
		case TAG_XML_ATT_lang:
			lsr_read_byte_align_string(lsr, info.far_ptr, "xml:lang");
			break;
		case TAG_XML_ATT_space:
			GF_LSR_READ_INT(lsr, *(XML_Space*)info.far_ptr, 1, "xml:space");
			break;
		/*focusable*/
		case TAG_SVG_ATT_nav_next:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusNext");
			break;
		case TAG_SVG_ATT_nav_up:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusNorth");
			break;
		case TAG_SVG_ATT_nav_up_left:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusNorthEast");
			break;
		case TAG_SVG_ATT_nav_up_right:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusNorthWest");
			break;
		case TAG_SVG_ATT_nav_prev:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusPrev");
			break;
		case TAG_SVG_ATT_nav_down:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusSouth");
			break;
		case TAG_SVG_ATT_nav_down_left:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusSouthEast");
			break;
		case TAG_SVG_ATT_nav_down_right:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusSouthWest");
			break;
		case TAG_SVG_ATT_nav_left:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusEast");
			break;
		case TAG_SVG_ATT_focusable:
			GF_LSR_READ_INT(lsr, *(SVG_Focusable*)info.far_ptr, 2, "focusable");
			break;
		case TAG_SVG_ATT_nav_right:
			lsr_read_focus(lsr, (SVG_Focus*)info.far_ptr, "focusWest");
			break;
		case TAG_SVG_ATT_transform:
			lsr_read_matrix(lsr, info.far_ptr);
			break;
		case TAG_SVG_ATT_text_decoration:
			/*FIXME ASAP*/
			assert(0);
			break;

		case TAG_SVG_ATT_font_variant:
			GF_LSR_READ_INT(lsr, *(SVG_FontVariant*)info.far_ptr, 2, "font-variant");
			break;
		case TAG_SVG_ATT_font_family:
		{
			u32 flag;
			GF_LSR_READ_INT(lsr, flag, 1, "isInherit");
			if (flag) {
				((SVG_FontFamily*)info.far_ptr)->type = SVG_FONTFAMILY_INHERIT;
			} else {
				char *ft;
				((SVG_FontFamily*)info.far_ptr)->type = SVG_FONTFAMILY_VALUE;
				GF_LSR_READ_INT(lsr, flag, lsr->fontIndexBits, "fontIndex");
				ft = (char*)gf_list_get(lsr->font_table, flag);
				if (ft) ((SVG_FontFamily*)info.far_ptr)->value = gf_strdup(ft);
			}
		}
		break;
		case TAG_SVG_ATT_font_size:
			lsr_read_fixed_16_8i(lsr, info.far_ptr, "fontSize");
			break;
		case TAG_SVG_ATT_font_style:
			GF_LSR_READ_INT(lsr, *(SVG_FontStyle*)info.far_ptr, 3, "fontStyle");
			break;
		case TAG_SVG_ATT_font_weight:
			GF_LSR_READ_INT(lsr, *(SVG_FontWeight*)info.far_ptr, 4, "fontWeight");
			break;
		case TAG_XLINK_ATT_title:
			lsr_read_byte_align_string(lsr, info.far_ptr, "xlink:title");
			break;
		case TAG_XLINK_ATT_type:
			/*TODO FIXME*/
			GF_LSR_READ_INT(lsr, field_rare, 3, "xlink:type");
			break;
		case TAG_XLINK_ATT_role:
			lsr_read_any_uri(lsr, info.far_ptr, "xlink:role");
			break;
		case TAG_XLINK_ATT_arcrole:
			lsr_read_any_uri(lsr, info.far_ptr, "xlink:arcrole");
			break;
		case TAG_XLINK_ATT_actuate:
			/*TODO FIXME*/
			GF_LSR_READ_INT(lsr, field_rare, 2, "xlink:actuate");
			break;
		case TAG_XLINK_ATT_show:
			/*TODO FIXME*/
			GF_LSR_READ_INT(lsr, field_rare, 3, "xlink:show");
			break;
		case TAG_SVG_ATT_end:
			lsr_read_smil_times(lsr, NULL, 0, info.far_ptr, "end", 0);
			break;
		case TAG_SVG_ATT_max:
			lsr_read_duration_ex(lsr, NULL, 0, info.far_ptr, "min", 0);
			break;
		case TAG_SVG_ATT_min:
			lsr_read_duration_ex(lsr, NULL, 0, info.far_ptr, "min", 0);
			break;
		}
		if (lsr->last_error) break;
	}
}

#define lsr_read_rare(_a, _b) lsr_read_rare_full(_a, _b)

static void lsr_read_fill(GF_LASeRCodec *lsr, GF_Node *n)
{
	Bool has_fill;
	GF_LSR_READ_INT(lsr, has_fill, 1, "fill");
	if (has_fill) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_fill, GF_TRUE, GF_FALSE, &info);
		lsr_read_paint(lsr, info.far_ptr, "fill");
	}
}

static void lsr_read_stroke(GF_LASeRCodec *lsr, GF_Node *n)
{
	Bool has_stroke;
	GF_LSR_READ_INT(lsr, has_stroke, 1, "has_stroke");
	if (has_stroke) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_stroke, GF_TRUE, GF_FALSE, &info);
		lsr_read_paint(lsr, info.far_ptr, "stroke");
	}
}
static void lsr_read_href(GF_LASeRCodec *lsr, GF_Node *n)
{
	Bool has_href;
	GF_LSR_READ_INT(lsr, has_href, 1, "has_href");
	if (has_href) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info);
		lsr_read_any_uri(lsr, info.far_ptr, "href");
	}
}

static void lsr_read_accumulate(GF_LASeRCodec *lsr, GF_Node *n)
{
	Bool v;
	GF_LSR_READ_INT(lsr, v, 1, "has_accumulate");
	if (v) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_accumulate, GF_TRUE, GF_FALSE, &info);
		GF_LSR_READ_INT(lsr, *(SMIL_Accumulate*)info.far_ptr, 1, "accumulate");
	}
}
static void lsr_read_additive(GF_LASeRCodec *lsr, GF_Node *n)
{
	Bool v;
	GF_LSR_READ_INT(lsr, v, 1, "has_additive");
	if (v) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_additive, GF_TRUE, GF_FALSE, &info);
		GF_LSR_READ_INT(lsr, *(SMIL_Additive*)info.far_ptr, 1, "additive");
	}
}
static void lsr_read_calc_mode(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 v;
	/*SMIL_CALCMODE_LINEAR is default and 0 in our code*/
	GF_LSR_READ_INT(lsr, v, 1, "has_calcMode");
	if (v) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_calcMode, GF_TRUE, GF_FALSE, &info);
		GF_LSR_READ_INT(lsr, *(SMIL_CalcMode*)info.far_ptr, 2, "calcMode");
	}
}

static void lsr_read_attribute_name_ex(GF_LASeRCodec *lsr, GF_Node *n, Bool skippable)
{
	u32 val = 1;
	if (skippable) {
		GF_LSR_READ_INT(lsr, val, 1, "hasAttributeName");
		if (!val) return;
	}

	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val) {
		lsr_read_vluimsbf5(lsr, "item[i]");
		lsr_read_vluimsbf5(lsr, "item[i]");
		return;
	} else {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_attributeName, GF_TRUE, GF_FALSE, &info);
		GF_LSR_READ_INT(lsr, val, 8, "attributeType");

		/*translate type to attribute tag*/
		((SMIL_AttributeName*)info.far_ptr)->type = gf_lsr_anim_type_to_attribute(val);
	}
}
static void lsr_read_attribute_name(GF_LASeRCodec *lsr, GF_Node *n)
{
	lsr_read_attribute_name_ex(lsr, n, GF_TRUE);
}

static void lsr_translate_anim_value(SMIL_AnimateValue *val, u32 coded_type)
{
	switch (val->type) {
	case SVG_StrokeDashArray_datatype:
	{
		SVG_StrokeDashArray *da;
		GF_List *l = (GF_List *)val->value;
		u32 i;
		GF_SAFEALLOC(da, SVG_StrokeDashArray);
		if (!da) return;
		da->array.count = gf_list_count(l);
		if (!da->array.count) {
			da->type = SVG_STROKEDASHARRAY_INHERIT;
		} else {
			da->type = SVG_STROKEDASHARRAY_ARRAY;
			da->array.vals = (Fixed *) gf_malloc(sizeof(Fixed)*da->array.count);
			da->array.units = (u8 *) gf_malloc(sizeof(u8)*da->array.count);
			for (i=0; i<da->array.count; i++) {
				Fixed *v = (Fixed *)gf_list_get(l, i);
				da->array.vals[i] = *v;
				da->array.units[i] = 0;
				gf_free(v);
			}
			gf_list_del(l);
			val->value = da;
		}
	}
	break;
	case SVG_ViewBox_datatype:
	{
		SVG_ViewBox *vb;
		GF_List *l = (GF_List *)val->value;
		GF_SAFEALLOC(vb, SVG_ViewBox);
		if (!vb) return;
		if (gf_list_count(l)==4) {
			vb->x = * ((Fixed *)gf_list_get(l, 0));
			vb->y = * ((Fixed *)gf_list_get(l, 1));
			vb->width = * ((Fixed *)gf_list_get(l, 2));
			vb->height = * ((Fixed *)gf_list_get(l, 3));
		}
		while (gf_list_count(l)) {
			Fixed *v = (Fixed *)gf_list_last(l);
			gf_free(v);
			gf_list_rem_last(l);
		}
		gf_list_del(l);
		val->value = vb;
	}
	break;
	case SVG_Coordinates_datatype:
	{
		SVG_Coordinates *coords;
		if (coded_type==1) {
			GF_List *l = gf_list_new();
			/*allocated value is already an SVG number*/
			gf_list_add(l, val->value);
			coords = (SVG_Coordinates*)gf_malloc(sizeof(SVG_Coordinates));
			*coords = l;
			val->value = coords;
		} else if (coded_type==8) {
			GF_List *l = (GF_List *)val->value;
			u32 i, count = gf_list_count(l);
			for (i=0; i<count; i++) {
				SVG_Coordinate *c;
				Fixed *v = (Fixed *)gf_list_get(l, i);
				c = (SVG_Coordinate*)gf_malloc(sizeof(SVG_Coordinate));
				c->type = SVG_NUMBER_VALUE;
				c->value = *v;
				gf_free(v);
				gf_list_rem(l, i);
				gf_list_insert(l, c, i);
			}
			coords = (SVG_Coordinates*)gf_malloc(sizeof(SVG_Coordinates));
			*coords = (GF_List *) val->value;
			val->value = coords;
		}
	}
	break;
	case SVG_Motion_datatype:
		if (coded_type==9) {
			GF_Matrix2D *mat;
			SVG_Point *pt = (SVG_Point *)val->value;
			GF_SAFEALLOC(mat, GF_Matrix2D);
			gf_mx2d_init(*mat);
			mat->m[2] = pt->x;
			mat->m[5] = pt->y;
			gf_free(pt);
			val->value = mat;
		}
		break;
	}
}
static void lsr_translate_anim_values(SMIL_AnimateValues *val, u32 coded_type)
{
	u32 i, count;
	GF_List *list, *new_list;

	list = val->values;
	switch (val->type) {
	case SVG_StrokeDashArray_datatype:
		break;
	case SVG_ViewBox_datatype:
		break;
	case SVG_Coordinates_datatype:
		break;
	case SVG_Motion_datatype:
		if (coded_type!=9) return;
		break;
	default:
		return;
	}
	val->values = new_list = gf_list_new();
	count = gf_list_count(list);
	for (i=0; i<count; i++) {
		switch (val->type) {
		case SVG_StrokeDashArray_datatype:
		{
			SVG_StrokeDashArray *da;
			GF_List *l = (GF_List *)gf_list_get(list, i);
			u32 j;
			GF_SAFEALLOC(da, SVG_StrokeDashArray);
			if (!da) return;
			da->array.count = gf_list_count(l);
			if (!da->array.count) {
				da->type = SVG_STROKEDASHARRAY_INHERIT;
			} else {
				da->type = SVG_STROKEDASHARRAY_ARRAY;
				da->array.vals = (Fixed *)gf_malloc(sizeof(Fixed)*da->array.count);
				da->array.units = (u8 *) gf_malloc(sizeof(u8)*da->array.count);
				for (j=0; j<da->array.count; j++) {
					Fixed *v = (Fixed *)gf_list_get(l, j);
					da->array.vals[j] = *v;
					da->array.units[j] = 0;
					gf_free(v);
				}
			}
			gf_list_del(l);
			gf_list_add(new_list, da);
		}
		break;
		case SVG_ViewBox_datatype:
		{
			SVG_ViewBox *vb;
			GF_List *l = (GF_List *)gf_list_get(list, i);
			GF_SAFEALLOC(vb, SVG_ViewBox);
			if (!vb) return;
			if (gf_list_count(l)==4) {
				vb->x = * ((Fixed *)gf_list_get(l, 0));
				vb->y = * ((Fixed *)gf_list_get(l, 1));
				vb->width = * ((Fixed *)gf_list_get(l, 2));
				vb->height = * ((Fixed *)gf_list_get(l, 3));
			}
			while (gf_list_count(l)) {
				Fixed *v=(Fixed *)gf_list_last(l);
				gf_free(v);
				gf_list_rem_last(l);
			}
			gf_list_del(l);
			gf_list_add(new_list, vb);
		}
		break;
		case SVG_Coordinates_datatype:
		{
			SVG_Coordinates *coords;
			GF_List *l = (GF_List *)gf_list_get(list, i);
			u32 j, count2;
			count2 = gf_list_count(l);
			for (j=0; j<count2; j++) {
				SVG_Coordinate *c = (SVG_Coordinate *)gf_malloc(sizeof(SVG_Coordinate));
				Fixed *v = (Fixed *)gf_list_get(l, j);
				c->type = SVG_NUMBER_VALUE;
				c->value = *v;
				gf_list_rem(l, j);
				gf_list_insert(l, c, j);
				gf_free(v);
			}

			coords = (SVG_Coordinates*)gf_malloc(sizeof(SVG_Coordinates));
			*coords = l;
			gf_list_add(new_list, coords);
		}
		break;

		case SVG_Motion_datatype:
 		{
			GF_Matrix2D *m = (GF_Matrix2D *)gf_malloc(sizeof(GF_Matrix2D ));
			GF_Point2D *pt = (GF_Point2D *)gf_list_get(list, i);
			gf_mx2d_init(*m);
			m->m[2] = pt->x;
			m->m[5] = pt->y;
			gf_free(pt);
			gf_list_add(new_list, m);
		}
			break;
		}
	}
	gf_list_del(list);
}

static Bool lsr_init_smil_times(GF_LASeRCodec *lsr, SVG_Element *anim, GF_List *times, SVG_Element *parent)
{
	u32 i, count;
	count = gf_list_count(times);
	for (i=0; i<count; i++) {
		SMIL_Time *t = (SMIL_Time *)gf_list_get(times, i);
		if (t->type==GF_SMIL_TIME_EVENT) {
			if (t->element_id) {
				if (t->element_id[0]=='N') {
					t->element = gf_sg_find_node(lsr->sg, atoi(t->element_id+1) + 1);
				} else {
					t->element = gf_sg_find_node_by_name(lsr->sg, t->element_id);
				}
				if (!t->element) return GF_FALSE;
				gf_free(t->element_id);
				t->element_id = NULL;
			}
			else if (!t->element) {
				if (t->event.parameter && (t->event.type==GF_EVENT_KEYDOWN) ) {
					t->element = lsr->sg->RootNode ? lsr->sg->RootNode : lsr->current_root;
				} else {
					t->element = (GF_Node*)parent;
				}
			}
		}
	}
	return GF_TRUE;
}

static Bool lsr_setup_smil_anim(GF_LASeRCodec *lsr, SVG_Element *anim, SVG_Element *anim_parent)
{
	GF_FieldInfo info;
	u32 coded_type, not_res;
	GF_Node *target;
	Bool is_animateMotion, is_animateTransform;
	XMLRI *xlink;
	SMIL_AttributeName *name = NULL;
	SMIL_AnimateValue *value;

	/*setup smil events*/
	not_res = 0;
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_begin, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if (!lsr_init_smil_times(lsr, anim, *(GF_List**)info.far_ptr, anim_parent)) not_res++;
	}

	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_end, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if (!lsr_init_smil_times(lsr, anim, *(GF_List**)info.far_ptr, anim_parent)) not_res++;
	}


	/*get xlink*/
	xlink = NULL;
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_XLINK_ATT_href, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		xlink = info.far_ptr;
	}

	/*setup target node*/
	if (!xlink || !xlink->target) {
		/*target not received*/
		if (xlink && (xlink->type == XMLRI_ELEMENTID)) return GF_FALSE;

		if (!xlink) {
			/*target is parent, initialize xlink (needed by anim module)*/
			if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info)==GF_OK) {
				xlink = info.far_ptr;
			} else {
				return GF_FALSE;
			}
		}

		xlink->type = XMLRI_ELEMENTID;
		xlink->target = anim_parent;
		gf_node_register_iri(lsr->sg, xlink);
		target = (GF_Node *)anim_parent;
	} else {
		target = (GF_Node *)xlink->target;
	}
	if (!target || not_res) return GF_FALSE;

	is_animateTransform = is_animateMotion = GF_FALSE;
	if (anim->sgprivate->tag==TAG_SVG_animateMotion) is_animateMotion = GF_TRUE;
	else if (anim->sgprivate->tag==TAG_SVG_animateTransform) {
		is_animateTransform = GF_TRUE;
	}
	if (is_animateMotion) goto translate_vals;

	/*for all except animateMotion, get attributeName*/
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_attributeName, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		name = info.far_ptr;
	}
	if (!name) {
		return GF_FALSE;
	}

	if (!name->field_ptr) {
		if (gf_node_get_attribute_by_tag((GF_Node *)target, name->type, GF_TRUE, GF_FALSE, &info)!=GF_OK) return GF_FALSE;
		name->field_ptr = info.far_ptr;
		name->type = info.fieldType;
		name->tag = info.fieldIndex;
	}


	/*browse all anim types and retranslate anim values. This must be done in 2 steps since we may not have received
	the target node when parsing the animation node*/
translate_vals:

	/*and setup anim values*/
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_from, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if (is_animateTransform) {
			name->type = ((SMIL_AnimateValue*)info.far_ptr)->type;
		} else {
			value = info.far_ptr;
			coded_type = value->type;
			value->type = is_animateMotion ? SVG_Motion_datatype : name->type;
			lsr_translate_anim_value(value, coded_type);
		}
	}
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_by, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if (is_animateTransform) {
			name->type = ((SMIL_AnimateValue*)info.far_ptr)->type;
		} else {
			value = info.far_ptr;
			coded_type = value->type;
			value->type = is_animateMotion ? SVG_Motion_datatype : name->type;
			lsr_translate_anim_value(value, coded_type);
		}
	}
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_to, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if (is_animateTransform) {
			name->type = ((SMIL_AnimateValue*)info.far_ptr)->type;
		} else {
			value = info.far_ptr;
			coded_type = value->type;
			value->type = is_animateMotion ? SVG_Motion_datatype : name->type;
			lsr_translate_anim_value(value, coded_type);
		}
	}
	if (gf_node_get_attribute_by_tag((GF_Node *)anim, TAG_SVG_ATT_values, GF_FALSE, GF_FALSE, &info)==GF_OK) {
		if (is_animateTransform) {
			name->type = ((SMIL_AnimateValues*)info.far_ptr)->type;
		} else {
			SMIL_AnimateValues *values = info.far_ptr;
			coded_type = values->type;
			values->type = is_animateMotion ? SVG_Motion_datatype : name->type;
			values->laser_strings = 0;
			lsr_translate_anim_values(values, coded_type);
		}
	}

	return GF_TRUE;
}

static void lsr_read_anim_fill(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 val;

	GF_LSR_READ_INT(lsr, val, 1, "has_smil_fill");
	if (val) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_smil_fill, GF_TRUE, 0, &info);
		/*enumeration freeze{0} remove{1}*/
		GF_LSR_READ_INT(lsr, val, 1, "smil_fill");
		*(SMIL_Fill*)info.far_ptr = val ? SMIL_FILL_REMOVE : SMIL_FILL_FREEZE;
	}
}
static void lsr_read_anim_repeatCount(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "has_repeatCount");
	if (val) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_repeatCount, GF_TRUE, 0, &info);
		GF_LSR_READ_INT(lsr, val, 1, "repeatCount");
		if (val) ((SMIL_RepeatCount*)info.far_ptr)->type = SMIL_REPEATCOUNT_INDEFINITE;
		else {
			((SMIL_RepeatCount*)info.far_ptr)->type = SMIL_REPEATCOUNT_DEFINED;
			((SMIL_RepeatCount*)info.far_ptr)->count = lsr_read_fixed_16_8(lsr, "repeatCount");
		}
	}
}
static void lsr_read_repeat_duration(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "has_repeatDur");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_repeatDur, GF_TRUE, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "choice");

		if (flag) {
			((SMIL_Duration *)info.far_ptr)->type = SMIL_DURATION_INDEFINITE;
		} else {
			((SMIL_Duration *)info.far_ptr)->clock_value = (Double) lsr_read_vluimsbf5(lsr, "value");
			((SMIL_Duration *)info.far_ptr)->clock_value /= lsr->time_resolution;
			((SMIL_Duration *)info.far_ptr)->type = SMIL_DURATION_DEFINED;
		}
	}
}
static void lsr_read_anim_restart(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 val;
	GF_LSR_READ_INT(lsr, val, 1, "has_restart");
	if (val) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_restart, GF_TRUE, 0, &info);
		/*enumeration always{0} never{1} whenNotActive{2}*/
		GF_LSR_READ_INT(lsr, *(SMIL_Restart*)info.far_ptr, 2, "restart");
	}
}

static void *lsr_read_an_anim_value(GF_LASeRCodec *lsr, u32 coded_type, const char *name)
{
	u32 flag;
	u32 escapeFlag, escape_val = 0;
	u8 *enum_val;
	u32 *id_val;
	char *string;
	SVG_String *svg_string;
	SVG_Number *num;
	XMLRI *iri;
	SVG_Point *pt;
	SVG_Paint *paint;

	GF_LSR_READ_INT(lsr, escapeFlag, 1, "escapeFlag");
	if (escapeFlag) GF_LSR_READ_INT(lsr, escape_val, 2, "escapeEnum");

	switch (coded_type) {
	case 0:
		string = NULL;
		lsr_read_byte_align_string(lsr, &string, name);
		GF_SAFEALLOC(svg_string, SVG_String);
		if (!svg_string) return NULL;
		*svg_string = string;
		return svg_string;
	case 1:
		num = (SVG_Number*)gf_malloc(sizeof(SVG_Number));
		if (escapeFlag) {
			num->type = (escape_val==1) ? SVG_NUMBER_INHERIT : SVG_NUMBER_VALUE;
		} else {
			num->type = SVG_NUMBER_VALUE;
			num->value = lsr_read_fixed_16_8(lsr, name);
		}
		return num;
	case 2:
	{
		SVG_PathData *pd = (SVG_PathData *)gf_svg_create_attribute_value(SVG_PathData_datatype);
		lsr_read_path_type(lsr, NULL, 0, pd, name);
		return pd;
	}
	case 3:
	{
		SVG_Points *pts = (SVG_Points *)gf_svg_create_attribute_value(SVG_Points_datatype);
		lsr_read_point_sequence(lsr, *pts, name);
		return pts;
	}
	case 4:
		num = (SVG_Number*)gf_malloc(sizeof(SVG_Number));
		if (escapeFlag) {
			num->type = (escape_val==1) ? SVG_NUMBER_INHERIT : SVG_NUMBER_VALUE;
		} else {
			num->type = SVG_NUMBER_VALUE;
			num->value = lsr_read_fixed_clamp(lsr, name);
		}
		return num;
	case 5:
		GF_SAFEALLOC(paint, SVG_Paint);
		if (!paint) return NULL;
		if (escapeFlag) {
			paint->type = SVG_PAINT_INHERIT;
		} else {
			lsr_read_paint(lsr, paint, name);
		}
		return paint;
	case 6:
		enum_val = (u8*)gf_malloc(sizeof(u8));
		*enum_val = lsr_read_vluimsbf5(lsr, name);
		return enum_val;
	/*TODO check this is correct*/
	case 7:
	{
		GF_List *l = gf_list_new();
		u32 i, count;
		count = lsr_read_vluimsbf5(lsr, "count");
		for (i=0; i<count; i++) {
			u8 *v = (u8 *)gf_malloc(sizeof(u8));
			*v = lsr_read_vluimsbf5(lsr, "val");
			gf_list_add(l, v);
		}
		return l;
	}
	/*TODO check this is correct*/
	case 8: // floats
	{
		GF_List *l = gf_list_new();
		u32 i, count;
		count = lsr_read_vluimsbf5(lsr, "count");
		for (i=0; i<count; i++) {
			Fixed *v = (Fixed *)gf_malloc(sizeof(Fixed));
			*v = lsr_read_fixed_16_8(lsr, "val");
			gf_list_add(l, v);
		}
		return l;
	}

	/*point */
	case 9:
		pt = (SVG_Point*)gf_malloc(sizeof(SVG_Point));
		GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "valX");
		pt->x = lsr_translate_coords(lsr, flag, lsr->coord_bits);
		GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, "valY");
		pt->y = lsr_translate_coords(lsr, flag, lsr->coord_bits);
		return pt;
	case 10:
		id_val = (u32*)gf_malloc(sizeof(u32));
		*id_val = lsr_read_vluimsbf5(lsr, name);
		return id_val;
	case 11:
	{
		SVG_FontFamily *ft;
		u32 idx;
		GF_SAFEALLOC(ft, SVG_FontFamily);
		if (!ft) return NULL;
		if (escapeFlag) {
			ft->type = SVG_FONTFAMILY_INHERIT;
		} else {
			idx = lsr_read_vluimsbf5(lsr, name);
			ft->type = SVG_FONTFAMILY_VALUE;
			ft->value = (char*)gf_list_get(lsr->font_table, idx);
			if (ft->value) ft->value = gf_strdup(ft->value);
		}
		return ft;
	}
	case 12:
		GF_SAFEALLOC(iri, XMLRI);
		lsr_read_any_uri(lsr, iri, name);
		return iri;
	default:
		lsr_read_extension(lsr, name);
		break;
	}
	return NULL;
}

static void lsr_translate_anim_trans_value(SMIL_AnimateValue *val, u32 transform_type)
{
	SVG_Point_Angle *p;
	Fixed *f;
	u32 coded_type = val->type;

	switch(transform_type) {
	case SVG_TRANSFORM_TRANSLATE:
		val->type = SVG_Transform_Translate_datatype;
		break;
	case SVG_TRANSFORM_SCALE:
		val->type = SVG_Transform_Scale_datatype;
		break;
	case SVG_TRANSFORM_ROTATE:
		val->type = SVG_Transform_Rotate_datatype;
		break;
	case SVG_TRANSFORM_SKEWX:
		val->type = SVG_Transform_SkewX_datatype;
		break;
	case SVG_TRANSFORM_SKEWY:
		val->type = SVG_Transform_SkewY_datatype;
		break;
	case SVG_TRANSFORM_MATRIX:
		val->type = SVG_Transform_datatype;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[LSR Parsing] unknown datatype for animate transform.\n"));
		return;
	}
	if (!val->value) return;
	switch (transform_type) {
	case SVG_TRANSFORM_ROTATE:
		p = (SVG_Point_Angle*)gf_malloc(sizeof(SVG_Point_Angle));
		p->x = p->y = 0;
		if (coded_type==8) {
			GF_List *l = (GF_List *)val->value;
			f = (Fixed*)gf_list_get(l, 0);
			if (f) {
				p->angle = *f;
				gf_free(f);
			}
			f = (Fixed*)gf_list_get(l, 1);
			if (f) {
				p->x = *f;
				gf_free(f);
			}
			f = (Fixed*)gf_list_get(l, 2);
			if (f) {
				p->y = *f;
				gf_free(f);
			}
			gf_list_del(l);
		} else {
			p->angle = ((SVG_Number *)val->value)->value;
			gf_free(val->value);
		}
		p->angle = gf_muldiv(p->angle, GF_PI, INT2FIX(180) );
		val->value = p;
		break;
	case SVG_TRANSFORM_SCALE:
		if (coded_type==8) {
			SVG_Point *pt;
			GF_List *l = (GF_List *)val->value;
			GF_SAFEALLOC(pt , SVG_Point);
			if (!pt) return;
			f = (Fixed*)gf_list_get(l, 0);
			if (f) {
				pt->x = *f;
				gf_free(f);
			}
			f = (Fixed*)gf_list_get(l, 1);
			if (f) {
				pt->y = *f;
				gf_free(f);
			}
			else pt->y = pt->x;
			gf_list_del(l);
			val->value = pt;
		}
		break;
	case SVG_TRANSFORM_SKEWX:
	case SVG_TRANSFORM_SKEWY:
		f = (Fixed*)gf_malloc(sizeof(Fixed));
		*f = ((SVG_Number *)val->value)->value;
		gf_free(val->value);
		val->value = f;
		break;
	}
}

static void lsr_translate_anim_trans_values(SMIL_AnimateValues *val, u32 transform_type)
{
	u32 count, i, coded_type;
	SVG_Point_Angle *p;
	SVG_Point *pt;
	Fixed *f;
	GF_List *l;

	coded_type = val->type;
	switch(transform_type) {
	case SVG_TRANSFORM_TRANSLATE:
		val->type = SVG_Transform_Translate_datatype;
		break;
	case SVG_TRANSFORM_SCALE:
		val->type = SVG_Transform_Scale_datatype;
		break;
	case SVG_TRANSFORM_ROTATE:
		val->type = SVG_Transform_Rotate_datatype;
		break;
	case SVG_TRANSFORM_SKEWX:
		val->type = SVG_Transform_SkewX_datatype;
		break;
	case SVG_TRANSFORM_SKEWY:
		val->type = SVG_Transform_SkewY_datatype;
		break;
	case SVG_TRANSFORM_MATRIX:
		val->type = SVG_Transform_datatype;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] unknown datatype for animate transform.\n"));
		return;
	}
	count = gf_list_count(val->values);
	if (!count) return;

	if (transform_type==SVG_TRANSFORM_TRANSLATE)
		return;

	for (i=0; i<count; i++) {
		void *a_val = gf_list_get(val->values, i);
		switch (transform_type) {
		case SVG_TRANSFORM_ROTATE:
			GF_SAFEALLOC(p, SVG_Point_Angle);
			if (!p) return;

			if (coded_type==8) {
				l = (GF_List*)a_val;
				f = (Fixed*)gf_list_get(l, 0);
				p->angle = *f;
				f = (Fixed*)gf_list_get(l, 1);
				if (f) p->x = *f;
				f = (Fixed*)gf_list_get(l, 2);
				if (f) p->y = *f;
				while (gf_list_count(l)) {
					f = (Fixed*)gf_list_last(l);
					gf_list_rem_last(l);
					gf_free(f);
				}
				gf_list_del(l);
			} else if (coded_type==1) {
				p->angle = ((SVG_Number *)a_val)->value;
				gf_free(a_val);
			}
			p->angle = gf_muldiv(p->angle, GF_PI, INT2FIX(180) );
			gf_list_rem(val->values, i);
			gf_list_insert(val->values, p, i);
			break;
		case SVG_TRANSFORM_SKEWX:
		case SVG_TRANSFORM_SKEWY:
			f = (Fixed*)gf_malloc(sizeof(Fixed));
			*f = ((SVG_Number *)a_val)->value;
			gf_free(a_val);
			gf_list_rem(val->values, i);
			gf_list_insert(val->values, f, i);
			break;
		case SVG_TRANSFORM_SCALE:
			pt = (SVG_Point*)gf_malloc(sizeof(SVG_Point));
			l = (GF_List*)a_val;
			f = (Fixed*)gf_list_get(l, 0);
			if (f) pt->x = *f;
			f = (Fixed*)gf_list_get(l, 1);
			if (f) pt->y = *f;
			else pt->y = pt->x;
			while (gf_list_count(l)) {
				f = (Fixed*)gf_list_last(l);
				gf_list_rem_last(l);
				gf_free(f);
			}
			gf_list_del(l);
			gf_list_rem(val->values, i);
			gf_list_insert(val->values, pt, i);
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[LASeR] unknown transform type %d\n", transform_type));
			break;
		}
	}
}

static void lsr_read_anim_value_ex(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, const char *name, u32 *tr_type)
{
	u32 val, coded_type;
	GF_LSR_READ_INT(lsr, val, 1, name);
	if (val) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, tag, GF_TRUE, 0, &info);

		GF_LSR_READ_INT(lsr, coded_type, 4, "type");
		((SMIL_AnimateValue*)info.far_ptr)->value = lsr_read_an_anim_value(lsr, coded_type, name);
		((SMIL_AnimateValue*)info.far_ptr)->type = coded_type;

		if (tr_type) {
			lsr_translate_anim_trans_value(info.far_ptr, *tr_type);
		}
	}
}

static void lsr_read_anim_values_ex(GF_LASeRCodec *lsr, GF_Node *n, u32 *tr_type)
{
	u32 flag, i, count = 0;
	u32 coded_type;
	GF_FieldInfo info;
	SMIL_AnimateValues *values;

	GF_LSR_READ_INT(lsr, flag, 1, "values");
	if (!flag) return;

	lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_values, GF_TRUE, 0, &info);
	values = (SMIL_AnimateValues *)info.far_ptr;

	GF_LSR_READ_INT(lsr, coded_type, 4, "type");
	values->type = coded_type;
	values->laser_strings = 1;

	count = lsr_read_vluimsbf5(lsr, "count");
	for (i=0; i<count; i++) {
		void *att = lsr_read_an_anim_value(lsr, coded_type, "a_value");
		if (att) gf_list_add(values->values, att);
	}
	if (tr_type) {
		lsr_translate_anim_trans_values(info.far_ptr, *tr_type);
	}
}
#define lsr_read_anim_value(_a, _b, _c, _d) lsr_read_anim_value_ex(_a, _b, _c, _d, NULL)
#define lsr_read_anim_values(_a, _b) lsr_read_anim_values_ex(_a, _b, NULL)

static Fixed *lsr_read_fraction_12_item(GF_LASeRCodec *lsr)
{
	u32 flag;
	Fixed *f;
	GF_SAFEALLOC(f, Fixed);
	if (!f) {
		lsr->last_error = GF_OUT_OF_MEM;
		return NULL;
	}
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
	return f;
}

static void lsr_read_fraction_12(GF_LASeRCodec *lsr, GF_Node *elt, u32 tag, const char *name)
{
	GF_FieldInfo info;
	u32 i, count;
	GF_LSR_READ_INT(lsr, count, 1, name);
	if (!count) return;

	lsr->last_error = gf_node_get_attribute_by_tag(elt, tag, GF_TRUE, 0, &info);

	count = lsr_read_vluimsbf5(lsr, "name");
	for (i=0; i<count; i++) {
		Fixed *f = lsr_read_fraction_12_item(lsr);
		gf_list_add( *((SMIL_KeyTimes*)info.far_ptr), f);
	}
}
static void lsr_read_float_list(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, SVG_Coordinates*coords, const char *name)
{
	u32 i, count;
	GF_LSR_READ_INT(lsr, count, 1, name);
	if (!count) return;

	if (!coords) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, tag, GF_TRUE, 0, &info);
		coords = (SVG_Coordinates*)info.far_ptr;
	} else {
		while (gf_list_count(*coords)) {
			Fixed *v = (Fixed *)gf_list_last(*coords);
			gf_list_rem_last(*coords);
			gf_free(v);
		}
	}
	count = lsr_read_vluimsbf5(lsr, "count");
	if (tag == TAG_SVG_ATT_text_rotate) {
		for (i=0; i<count; i++) {
			SVG_Number *n = (SVG_Number *)gf_malloc(sizeof(SVG_Number));
			n->type = SVG_NUMBER_VALUE;
			n->value = lsr_read_fixed_16_8(lsr, "val");
			gf_list_add(*coords, n);
		}
	}
	else {
		for (i=0; i<count; i++) {
			Fixed *n = (Fixed *)gf_malloc(sizeof(Fixed));
			*n = lsr_read_fixed_16_8(lsr, "val");
			gf_list_add(*coords, n);
		}
	}
}

static void lsr_read_point_sequence(GF_LASeRCodec *lsr, GF_List *pts, const char *name)
{
	u32 flag, i, count;

	while (gf_list_count(pts)) {
		SVG_Point *v = (SVG_Point *)gf_list_last(pts);
		gf_list_rem_last(pts);
		gf_free(v);
	}
	count = lsr_read_vluimsbf5(lsr, "nbPoints");
	if (!count) return;
	/*TODO golomb coding*/
	GF_LSR_READ_INT(lsr, flag, 1, "flag");
	if (!flag) {
		if (count < 3) {
			u32 nb_bits, v;
			GF_LSR_READ_INT(lsr, nb_bits, 5, "bits");
			for (i=0; i<count; i++) {
				SVG_Point *pt = (SVG_Point *)gf_malloc(sizeof(SVG_Point));
				gf_list_add(pts, pt);
				GF_LSR_READ_INT(lsr, v, nb_bits, "x");
				pt->x = lsr_translate_coords(lsr, v, nb_bits);
				GF_LSR_READ_INT(lsr, v, nb_bits, "y");
				pt->y = lsr_translate_coords(lsr, v, nb_bits);
			}
		} else {
			u32 nb_dx, nb_dy, k;
			Fixed x, y;
			SVG_Point *pt = (SVG_Point *)gf_malloc(sizeof(SVG_Point));
			gf_list_add(pts, pt);

			GF_LSR_READ_INT(lsr, nb_dx, 5, "bits");
			GF_LSR_READ_INT(lsr, k, nb_dx, "x");
			x = pt->x = lsr_translate_coords(lsr, k, nb_dx);
			GF_LSR_READ_INT(lsr, k, nb_dx, "y");
			y = pt->y = lsr_translate_coords(lsr, k, nb_dx);

			GF_LSR_READ_INT(lsr, nb_dx, 5, "bitsx");
			GF_LSR_READ_INT(lsr, nb_dy, 5, "bitsy");
			for (i=1; i<count; i++) {
				SVG_Point *pt = (SVG_Point *)gf_malloc(sizeof(SVG_Point));
				gf_list_add(pts, pt);
				GF_LSR_READ_INT(lsr, k, nb_dx, "dx");
				pt->x = x + lsr_translate_coords(lsr, k, nb_dx);
				x = pt->x;
				GF_LSR_READ_INT(lsr, k, nb_dy, "dy");
				pt->y = y + lsr_translate_coords(lsr, k, nb_dy);
				y = pt->y;
			}
		}
	}
}
static void lsr_read_path_type(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, SVG_PathData *path, const char *name)
{
#if USE_GF_PATH
	GF_Point2D *pt, *ct1, *ct2, *end;
	GF_Point2D orig, ct_orig;
	u32 i, count, cur_pt, type;
	GF_List *pts = gf_list_new();
	lsr_read_point_sequence(lsr, pts, "seq");

	if (!path) {
		GF_FieldInfo info;
		gf_node_get_attribute_by_tag(n, tag, GF_TRUE, 0, &info);
		path = (SVG_PathData*)info.far_ptr;
	} else {
		gf_path_reset(path);
	}
	/*first MoveTo is skipped in LASeR*/
	pt = (GF_Point2D*)gf_list_get(pts, 0);
	if (pt) {
		ct_orig = orig = *pt;
		gf_path_add_move_to_vec(path, pt);
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] Empty path found.\n"));
	}
	cur_pt = 1;
	count = lsr_read_vluimsbf5(lsr, "nbOfTypes");
	for (i=0; i<count; i++) {
		GF_LSR_READ_INT(lsr, type, 5, name);
		switch (type) {
		case LSR_PATH_COM_h:
		case LSR_PATH_COM_l:
		case LSR_PATH_COM_v:
		case LSR_PATH_COM_H:
		case LSR_PATH_COM_V:
		case LSR_PATH_COM_L:
			pt = (GF_Point2D*)gf_list_get(pts, cur_pt);
			if (!pt) goto err_exit;
			gf_path_add_line_to_vec(path, pt);
			cur_pt++;
			break;
		case LSR_PATH_COM_m:
		case LSR_PATH_COM_M:
			pt = (GF_Point2D*)gf_list_get(pts, cur_pt);
			if (!pt) goto err_exit;
			gf_path_add_move_to_vec(path, pt);
			cur_pt++;
			break;
		case LSR_PATH_COM_q:
		case LSR_PATH_COM_Q:
			ct1 = (GF_Point2D*)gf_list_get(pts, cur_pt);
			end = (GF_Point2D*)gf_list_get(pts, cur_pt+1);
			if (!ct1 || !end) goto err_exit;
			gf_path_add_quadratic_to_vec(path, ct1, end);
			orig = *end;
			cur_pt+=2;
			break;
		case LSR_PATH_COM_c:
		case LSR_PATH_COM_C:
			ct1 = (GF_Point2D*)gf_list_get(pts, cur_pt);
			ct2 = (GF_Point2D*)gf_list_get(pts, cur_pt+1);
			end = (GF_Point2D*)gf_list_get(pts, cur_pt+2);
			if (!ct1 || !ct2 || !end) goto err_exit;
			gf_path_add_cubic_to_vec(path, ct1, ct2, end);
			cur_pt+=3;
			ct_orig = *ct2;
			orig = *end;
			break;
		case LSR_PATH_COM_s:
		case LSR_PATH_COM_S:
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			ct2 = (GF_Point2D*)gf_list_get(pts, cur_pt);
			end = (GF_Point2D*)gf_list_get(pts, cur_pt+1);
			if (!ct2 || !end) goto err_exit;
			gf_path_add_cubic_to_vec(path, &ct_orig, ct2, end);
			ct_orig = *ct2;
			orig = *end;
			cur_pt+=2;
			break;
		case LSR_PATH_COM_t:
		case LSR_PATH_COM_T:
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			end = gf_list_get(pts, cur_pt);
			if (!end) goto err_exit;
			gf_path_add_quadratic_to_vec(path, &ct_orig, end);
			orig = *end;
			break;
		case LSR_PATH_COM_z:
		case LSR_PATH_COM_Z:
			gf_path_close(path);
			break;
		}
	}
	goto exit;

err_exit:
	lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;

exit:
	while (gf_list_count(pts)) {
		end = (GF_Point2D*)gf_list_get(pts, 0);
		gf_list_rem(pts, 0);
		gf_free(end);
	}
	gf_list_del(pts);

#else
	u32 i, count, c;
	u8 *type;
	lsr_read_point_sequence(lsr, path->points, "seq");
	while (gf_list_count(path->commands)) {
		u8 *v = (u8 *)gf_list_last(path->commands);
		gf_list_rem_last(path->commands);
		gf_free(v);
	}

	count = lsr_read_vluimsbf5(lsr, "nbOfTypes");
	for (i=0; i<count; i++) {
		type = (u8 *)gf_malloc(sizeof(u8));
		GF_LSR_READ_INT(lsr, c, 5, name);

		switch (c) {
		case LSR_PATH_COM_h:
		case LSR_PATH_COM_l:
		case LSR_PATH_COM_v:
		case LSR_PATH_COM_H:
		case LSR_PATH_COM_V:
		case LSR_PATH_COM_L:
			*type=SVG_PATHCOMMAND_L;
			break;
		case LSR_PATH_COM_m:
		case LSR_PATH_COM_M:
			*type=SVG_PATHCOMMAND_M;
			break;
		case LSR_PATH_COM_q:
		case LSR_PATH_COM_Q:
			*type=SVG_PATHCOMMAND_Q;
			break;
		case LSR_PATH_COM_c:
		case LSR_PATH_COM_C:
			*type=SVG_PATHCOMMAND_C;
			break;
		case LSR_PATH_COM_s:
		case LSR_PATH_COM_S:
			*type=SVG_PATHCOMMAND_S;
			break;
		case LSR_PATH_COM_t:
		case LSR_PATH_COM_T:
			*type=SVG_PATHCOMMAND_T;
			break;
		case LSR_PATH_COM_z:
		case LSR_PATH_COM_Z:
			*type=SVG_PATHCOMMAND_Z;
			break;
		}
		gf_list_add(path->commands, type);
	}
#endif
}

static void lsr_read_rotate_type(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "rotate");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_rotate, GF_TRUE, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "choice");

		if (flag) {
			GF_LSR_READ_INT(lsr, flag, 1, "rotate");
			((SVG_Number *)info.far_ptr)->type = flag ? SVG_NUMBER_AUTO_REVERSE : SVG_NUMBER_AUTO;
		} else {
			((SVG_Number *)info.far_ptr)->value = lsr_read_fixed_16_8(lsr, "rotate");
			((SVG_Number *)info.far_ptr)->type = SVG_NUMBER_VALUE;
		}
	}
}
static void lsr_read_sync_behavior(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "syncBehavior");
	if (flag) {
		GF_FieldInfo info;
		GF_LSR_READ_INT(lsr, flag, 2, "syncBehavior");
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_syncBehavior, GF_TRUE, 0, &info);
		*(SMIL_SyncBehavior*)info.far_ptr = flag + 1;
	}
}
static void lsr_read_sync_tolerance(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "syncTolerance");
	if (flag) {
		GF_FieldInfo info;
		GF_LSR_READ_INT(lsr, flag, 1, "syncTolerance");
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_syncTolerance, GF_TRUE, 0, &info);
		if (flag) ((SMIL_SyncTolerance *)info.far_ptr)->type = SMIL_SYNCTOLERANCE_DEFAULT;
		else {
			u32 v = lsr_read_vluimsbf5(lsr, "value");
			((SMIL_SyncTolerance *)info.far_ptr)->value = INT2FIX(v);
			((SMIL_SyncTolerance *)info.far_ptr)->value /= lsr->time_resolution;
		}
	}
}
static void lsr_read_sync_reference(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncReference");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_syncReference, GF_TRUE, 0, &info);
		lsr_read_any_uri(lsr, info.far_ptr, "syncReference");
	}
}

static void lsr_read_coordinate(GF_LASeRCodec *lsr, SVG_Number *coord, Bool skipable, const char *name)
{
	u32 flag;
	if (skipable) {
		GF_LSR_READ_INT(lsr, flag, 1, name);
		if (!flag) {
			//coord->type = SVG_NUMBER_UNKNOWN;
			//coord->value = 0;
			return;
		}
	}
	coord->type = SVG_NUMBER_VALUE;
	GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, name);
	coord->value = lsr_translate_coords(lsr, flag, lsr->coord_bits);
}
static void lsr_read_coordinate_ptr(GF_LASeRCodec *lsr, GF_Node *n, u32 tag, Bool skipable, const char *name)
{
	u32 flag;
	GF_FieldInfo info;
	if (skipable) {
		GF_LSR_READ_INT(lsr, flag, 1, name);
		if (!flag) return;
	}
	lsr->last_error = gf_node_get_attribute_by_tag(n, tag, GF_TRUE, 0, &info);

	((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
	GF_LSR_READ_INT(lsr, flag, lsr->coord_bits, name);
	((SVG_Number*)info.far_ptr)->value = lsr_translate_coords(lsr, flag, lsr->coord_bits);
}

static void lsr_read_coord_list(GF_LASeRCodec *lsr, GF_Node *elt, u32 tag, const char *name)
{
	GF_FieldInfo info;
	u32 i, count;
	GF_LSR_READ_INT(lsr, count, 1, name);
	if (!count) return;
	count = lsr_read_vluimsbf5(lsr, "nb_coords");
	if (!count) return;
	if (count>1000000) {
		lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;
		return;
	}

	lsr->last_error = gf_node_get_attribute_by_tag(elt, tag, GF_TRUE, 0, &info);

	for (i=0; i<count; i++) {
		u32 res;
		SVG_Coordinate *f;
		GF_SAFEALLOC(f, SVG_Coordinate );
		if (!f) {
			lsr->last_error = GF_OUT_OF_MEM;
			return;
		}
		GF_LSR_READ_INT(lsr, res, lsr->coord_bits, name);
		f->value = lsr_translate_coords(lsr, res, lsr->coord_bits);
		gf_list_add(*(SVG_Coordinates*)info.far_ptr, f);
	}
}

static void lsr_read_transform_behavior(GF_LASeRCodec *lsr, GF_Node *n)
{
	GF_FieldInfo info;
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "hasTransformBehavior");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_transformBehavior, GF_TRUE, 0, &info);
		GF_LSR_READ_INT(lsr, *(SVG_TransformBehavior*)info.far_ptr, 4, "transformBehavior");
	}
}

static void lsr_read_content_type(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "hasType");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_type, GF_TRUE, 0, &info);
		lsr_read_byte_align_string(lsr, info.far_ptr, "type");
	}
}
static void lsr_read_script_type(GF_LASeRCodec *lsr, GF_Node *n)
{
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "hasType");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_type, GF_TRUE, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "choice");
		if (flag) {
			GF_LSR_READ_INT(lsr, flag, 1, "script");
			switch (flag) {
			case 0:
				*(SVG_String*)info.far_ptr = gf_strdup("application/ecmascript");
				break;
			case 1:
				*(SVG_String*)info.far_ptr = gf_strdup("application/jar-archive");
				break;
			default:
				break;
			}
		} else {
			lsr_read_byte_align_string(lsr, info.far_ptr, "type");
		}
	}
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
	case 1:
		n->type = SVG_NUMBER_IN;
		break;
	case 2:
		n->type = SVG_NUMBER_CM;
		break;
	case 3:
		n->type = SVG_NUMBER_MM;
		break;
	case 4:
		n->type = SVG_NUMBER_PT;
		break;
	case 5:
		n->type = SVG_NUMBER_PC;
		break;
	case 6:
		n->type = SVG_NUMBER_PERCENTAGE;
		break;
	default:
		n->type = SVG_NUMBER_VALUE;
		break;
	}
}


static void lsr_read_clip_time(GF_LASeRCodec *lsr, GF_Node *elt, u32 tag, const char *name)
{
	GF_FieldInfo info;
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, name);
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, tag, 1, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "isEnum");
		if (!flag) {
			GF_LSR_READ_INT(lsr, flag, 1, "sign");
			flag = lsr_read_vluimsbf5(lsr, "val");
			*((SVG_Clock *)info.far_ptr) = flag;
			*((SVG_Clock *)info.far_ptr) /= lsr->time_resolution;
		}
	}
}

static void lsr_read_attribute_type(GF_LASeRCodec *lsr, GF_Node *elt)
{
	GF_FieldInfo info;
	u32 flag;
	GF_LSR_READ_INT(lsr, flag, 1, "hasAttributeType");
	if (!flag) return;
	lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_attributeType, 1, 0, &info);
	GF_LSR_READ_INT(lsr, *(SMIL_AttributeType*)info.far_ptr, 2, "attributeType");
}

static void lsr_read_preserve_aspect_ratio(GF_LASeRCodec *lsr, GF_Node *n)
{
	GF_FieldInfo info;
	u32 flag;
	SVG_PreserveAspectRatio *par;

	GF_LSR_READ_INT(lsr, flag, 1, "hasPreserveAspectRatio");
	if (!flag) return;

	lsr->last_error = gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_preserveAspectRatio, 1, 0, &info);
	par = (SVG_PreserveAspectRatio *)info.far_ptr;

	GF_LSR_READ_INT(lsr, flag, 1, "choice (meetOrSlice)");
	GF_LSR_READ_INT(lsr, par->defer, 1, "choice (defer)");
	GF_LSR_READ_INT(lsr, flag, 4, "alignXandY");
	switch (flag) {
	case 1:
		par->align = SVG_PRESERVEASPECTRATIO_XMAXYMAX;
		break;
	case 2:
		par->align = SVG_PRESERVEASPECTRATIO_XMAXYMID;
		break;
	case 3:
		par->align = SVG_PRESERVEASPECTRATIO_XMAXYMIN;
		break;
	case 4:
		par->align = SVG_PRESERVEASPECTRATIO_XMIDYMAX;
		break;
	case 5:
		par->align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
		break;
	case 6:
		par->align = SVG_PRESERVEASPECTRATIO_XMIDYMIN;
		break;
	case 7:
		par->align = SVG_PRESERVEASPECTRATIO_XMINYMAX;
		break;
	case 8:
		par->align = SVG_PRESERVEASPECTRATIO_XMINYMID;
		break;
	case 9:
		par->align = SVG_PRESERVEASPECTRATIO_XMINYMIN;
		break;
	default:
		par->align = SVG_PRESERVEASPECTRATIO_NONE;
		break;
	}
}

static void lsr_read_eRR(GF_LASeRCodec *lsr, GF_Node *elt)
{
	u32 err;
	GF_LSR_READ_INT(lsr, err, 1, "externalResourcesRequired");
	if (err) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_externalResourcesRequired, 1, 0, &info);
		*(SVG_Boolean*)info.far_ptr = 1;
	}
}

static void lsr_read_lsr_enabled(GF_LASeRCodec *lsr, GF_Node *elt)
{
	u32 err;
	GF_LSR_READ_INT(lsr, err, 1, "enabled");
	if (err) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_LSR_ATT_enabled, 1, 0, &info);
		*(SVG_Boolean*)info.far_ptr = 1;
	}
}

static GF_Node *lsr_read_a(GF_LASeRCodec *lsr)
{
	Bool flag;
	GF_Node *elt = (GF_Node*) gf_node_new(lsr->sg, TAG_SVG_a);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_eRR(lsr, elt);
	GF_LSR_READ_INT(lsr, flag, 1, "hasTarget");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_target, 1, 0, &info);
		lsr_read_byte_align_string(lsr, info.far_ptr, "target");
	}
	lsr_read_href(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}



static GF_Node *lsr_read_animate(GF_LASeRCodec *lsr, SVG_Element *parent, Bool is_animateColor)
{
	GF_Node *elt = gf_node_new(lsr->sg, is_animateColor ? TAG_SVG_animateColor : TAG_SVG_animate);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_attribute_name(lsr, elt);

	lsr_read_accumulate(lsr, elt);
	lsr_read_additive(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_by, "by");
	lsr_read_calc_mode(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_from, "from");
	lsr_read_fraction_12(lsr, elt, TAG_SVG_ATT_keySplines, "keySplines");
	lsr_read_fraction_12(lsr, elt, TAG_SVG_ATT_keyTimes, "keyTimes");
	lsr_read_anim_values(lsr, elt);
	lsr_read_attribute_type(lsr, elt);
	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_duration(lsr, elt);
	lsr_read_anim_fill(lsr, elt);
	lsr_read_anim_repeatCount(lsr, elt);
	lsr_read_repeat_duration(lsr, elt);
	lsr_read_anim_restart(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_to, "to");
	lsr_read_href(lsr, elt);
	lsr_read_lsr_enabled(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);

	if (!lsr_setup_smil_anim(lsr, (SVG_Element*)elt, parent)) {
		gf_list_add(lsr->defered_anims, elt);
		lsr_read_group_content_post_init(lsr, (SVG_Element*)elt, 1);
	} else {
		lsr_read_group_content(lsr, elt, 0);
	}
	return elt;
}


static GF_Node *lsr_read_animateMotion(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	Bool flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_animateMotion);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_accumulate(lsr, elt);
	lsr_read_additive(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_by, "by");
	lsr_read_calc_mode(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_from, "from");
	lsr_read_fraction_12(lsr, elt, TAG_SVG_ATT_keySplines, "keySplines");
	lsr_read_fraction_12(lsr, elt, TAG_SVG_ATT_keyTimes, "keyTimes");
	lsr_read_anim_values(lsr, elt);
	lsr_read_attribute_type(lsr, elt);
	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_duration(lsr, elt);
	lsr_read_anim_fill(lsr, elt);
	lsr_read_anim_repeatCount(lsr, elt);
	lsr_read_repeat_duration(lsr, elt);
	lsr_read_anim_restart(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_to, "to");
	lsr_read_float_list(lsr, elt, TAG_SVG_ATT_keyPoints, NULL, "keyPoints");
	GF_LSR_READ_INT(lsr, flag, 1, "hasPath");
	if (flag) lsr_read_path_type(lsr, elt, TAG_SVG_ATT_path, NULL, "path");

	lsr_read_rotate_type(lsr, elt);
	lsr_read_href(lsr, elt);
	lsr_read_lsr_enabled(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);

	if (!lsr_setup_smil_anim(lsr, (SVG_Element*)elt, parent)) {
		gf_list_add(lsr->defered_anims, elt);
		lsr_read_group_content_post_init(lsr, (SVG_Element*)elt, 1);
	} else {
		lsr_read_group_content_post_init(lsr, (SVG_Element*)elt, 0);
	}
	return elt;
}


static GF_Node *lsr_read_animateTransform(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	u32 type;
	u32 flag;
	GF_FieldInfo info;
	GF_Node *elt= gf_node_new(lsr->sg, TAG_SVG_animateTransform);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_attribute_name(lsr, elt);

	/*enumeration rotate{0} scale{1} skewX{2} skewY{3} translate{4}*/
	GF_LSR_READ_INT(lsr, flag, 3, "rotscatra");
	switch (flag) {
	case 0:
		type = SVG_TRANSFORM_ROTATE;
		break;
	case 1:
		type = SVG_TRANSFORM_SCALE;
		break;
	case 2:
		type = SVG_TRANSFORM_SKEWX;
		break;
	case 3:
		type = SVG_TRANSFORM_SKEWY;
		break;
	case 4:
		type = SVG_TRANSFORM_TRANSLATE;
		break;
	default:
		type = SVG_TRANSFORM_ROTATE;
		break;
	}
	if (gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_transform_type, 1, 0, &info)==GF_OK) {
		*(SVG_TransformType *)info.far_ptr = type;
	}

	lsr_read_accumulate(lsr, elt);
	lsr_read_additive(lsr, elt);
	lsr_read_anim_value_ex(lsr, elt, TAG_SVG_ATT_by, "by", &type);
	lsr_read_calc_mode(lsr, elt);
	lsr_read_anim_value_ex(lsr, elt, TAG_SVG_ATT_from, "from", &type);
	lsr_read_fraction_12(lsr, elt, TAG_SVG_ATT_keySplines, "keySplines");
	lsr_read_fraction_12(lsr, elt, TAG_SVG_ATT_keyTimes, "keyTimes");
	lsr_read_anim_values_ex(lsr, elt, &type);
	lsr_read_attribute_type(lsr, elt);

	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_duration(lsr, elt);
	lsr_read_anim_fill(lsr, elt);
	lsr_read_anim_repeatCount(lsr, elt);
	lsr_read_repeat_duration(lsr, elt);
	lsr_read_anim_restart(lsr, elt);
	lsr_read_anim_value_ex(lsr, elt, TAG_SVG_ATT_to, "to", &type);

	lsr_read_href(lsr, elt);
	lsr_read_lsr_enabled(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);

	if (!lsr_setup_smil_anim(lsr, (SVG_Element*)elt, parent)) {
		gf_list_add(lsr->defered_anims, elt);
		lsr_read_group_content_post_init(lsr, (SVG_Element*)elt, 1);
	} else {
		lsr_read_group_content(lsr, elt, 0);
	}
	return elt;
}

static GF_Node *lsr_read_audio(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	GF_Node *elt= gf_node_new(lsr->sg, TAG_SVG_audio);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_duration(lsr, elt);
	lsr_read_eRR(lsr, elt);
	lsr_read_anim_repeatCount(lsr, elt);
	lsr_read_repeat_duration(lsr, elt);
	lsr_read_anim_restart(lsr, elt);
	lsr_read_sync_behavior(lsr, elt);
	lsr_read_sync_tolerance(lsr, elt);
	lsr_read_content_type(lsr, elt);
	lsr_read_href(lsr, elt);

	lsr_read_clip_time(lsr, elt, TAG_SVG_ATT_clipBegin, "clipBegin");
	lsr_read_clip_time(lsr, elt, TAG_SVG_ATT_clipEnd, "clipEnd");
	lsr_read_sync_reference(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_circle(GF_LASeRCodec *lsr)
{
	GF_Node *elt= gf_node_new(lsr->sg, TAG_SVG_circle);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_cx, 1, "cx");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_cy, 1, "cy");
	lsr_read_coordinate_ptr(lsr, elt,TAG_SVG_ATT_r, 0, "r");
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_conditional(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_LSR_conditional);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_eRR(lsr, elt);
	lsr_read_lsr_enabled(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_command_list(lsr, NULL, (SVG_Element*)elt, 0);

	gf_node_init(elt);
	return elt;
}
static GF_Node *lsr_read_cursorManager(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_LSR_cursorManager);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt,TAG_SVG_ATT_x, 1, "x");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");
	lsr_read_href(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_data(GF_LASeRCodec *lsr, u32 node_tag)
{
	GF_Node *elt = gf_node_new(lsr->sg, node_tag);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_defs(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_defs);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_ellipse(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_ellipse);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_cx, 1, "cx");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_cy, 1, "cy");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_rx, 0, "rx");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_ry, 0, "ry");
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_foreignObject(GF_LASeRCodec *lsr)
{
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_foreignObject);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_eRR(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_height, 0, "height");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_width, 0, "width");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x, 1, "x");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");

	lsr_read_any_attribute(lsr, elt, 1);
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
	return elt;
}


static GF_Node *lsr_read_g(GF_LASeRCodec *lsr, Bool is_same)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_g);
	if (is_same) {
		if (lsr->prev_g) {
			lsr_restore_base(lsr, (SVG_Element*) elt, lsr->prev_g, 0, 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] sameg coded in bitstream but no g defined !\n"));
		}
		lsr_read_id(lsr, elt);
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);
		lsr_read_eRR(lsr, elt);
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_g = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, is_same);
	return elt;
}

static void lsr_read_opacity(GF_LASeRCodec *lsr, GF_Node *elt)
{
	u32 flag;
	GF_FieldInfo info;
	GF_LSR_READ_INT(lsr, flag, 1, "opacity");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_opacity, 1, 0, &info);
		((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
		((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_clamp(lsr, "opacity");
	}

}
static GF_Node *lsr_read_image(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_image);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_eRR(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_height, 1, "height");
	lsr_read_opacity(lsr, elt);

	lsr_read_preserve_aspect_ratio(lsr, elt);
	lsr_read_content_type(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_width, 1, "width");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x, 1, "x");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");
	lsr_read_href(lsr, elt);
	lsr_read_transform_behavior(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_line(GF_LASeRCodec *lsr, Bool is_same)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_line);

	if (is_same) {
		if (lsr->prev_line) {
			lsr_restore_base(lsr, (SVG_Element*) elt, (SVG_Element *)lsr->prev_line, 0, 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] sameline coded in bitstream but no line defined !\n"));
		}
		lsr_read_id(lsr, elt);
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);
	}
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x1, 1, "x1");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x2, 0, "x2");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y1, 1, "y1");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y2, 0, "y2");
	if (!is_same) {
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_line = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, is_same);
	return elt;
}

static void lsr_read_gradient_units(GF_LASeRCodec *lsr, GF_Node *elt)
{
	u32 flag;
	GF_FieldInfo info;
	GF_LSR_READ_INT(lsr, flag, 1, "hasGradientUnits");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_gradientUnits, 1, 0, &info);
		GF_LSR_READ_INT(lsr, *(SVG_GradientUnit*)info.far_ptr, 1, "gradientUnits");
	}
}
static GF_Node *lsr_read_linearGradient(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_linearGradient);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_gradient_units(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x1, 1, "x1");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x2, 1, "x2");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y1, 1, "y1");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y2, 1, "y2");
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_mpath(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_mpath);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_href(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_path(GF_LASeRCodec *lsr, u32 same_type)
{
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_path);

	if (same_type) {
		if (lsr->prev_path) {
			lsr_restore_base(lsr, (SVG_Element*)elt, (SVG_Element *)lsr->prev_path, (same_type==2) ? 1 : 0, 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] samepath coded in bitstream but no path defined !\n"));
		}
		lsr_read_id(lsr, elt);
		if (same_type==2) lsr_read_fill(lsr, elt);
		lsr_read_path_type(lsr, elt, TAG_SVG_ATT_d, NULL, "d");
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);
		lsr_read_path_type(lsr, elt, TAG_SVG_ATT_d, NULL, "d");
		GF_LSR_READ_INT(lsr, flag, 1, "hasPathLength");
		if (flag) {
			GF_FieldInfo info;
			lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_pathLength, 1, 0, &info);
			((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_16_8(lsr, "pathLength");
		}
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_path = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, same_type);
	return elt;
}
static GF_Node *lsr_read_polygon(GF_LASeRCodec *lsr, Bool is_polyline, u32 same_type)
{
	GF_FieldInfo info;
	GF_Node *elt = gf_node_new(lsr->sg, is_polyline ? TAG_SVG_polyline : TAG_SVG_polygon);

	lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_points, 1, 0, &info);

	if (same_type) {
		if (lsr->prev_polygon) {
			lsr_restore_base(lsr, (SVG_Element*)elt, (SVG_Element *)lsr->prev_polygon, (same_type==2) ? 0 : 0, (same_type==3) ? 0 : 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] samepolyXXX coded in bitstream but no polyXXX defined !\n"));
		}
		lsr_read_id(lsr, elt);
		if (same_type==2) lsr_read_fill(lsr, elt);
		else if (same_type==3) lsr_read_stroke(lsr, elt);
		lsr_read_point_sequence(lsr, *(GF_List**)info.far_ptr, "points");
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);
		lsr_read_point_sequence(lsr, *(GF_List**)info.far_ptr, "points");
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_polygon = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, same_type);
	return elt;
}
static GF_Node *lsr_read_radialGradient(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_radialGradient);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_cx, 1, "cx");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_cy, 1, "cy");
	lsr_read_gradient_units(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_r, 1, "r");
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_rect(GF_LASeRCodec *lsr, u32 same_type)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_rect);

	if (same_type) {
		if (lsr->prev_rect) {
			lsr_restore_base(lsr, (SVG_Element*)elt, (SVG_Element *)lsr->prev_rect, (same_type==2) ? 1 : 0, 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] samerect coded in bitstream but no rect defined !\n"));
		}
		lsr_read_id(lsr, elt);
		if (same_type==2) lsr_read_fill(lsr, elt);
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_height, 0, "height");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_width, 0, "width");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x, 1, "x");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_height, 0, "height");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_rx, 1, "rx");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_ry, 1, "ry");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_width, 0, "width");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x, 1, "x");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_rect = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, same_type);
	return elt;
}

static GF_Node *lsr_read_rectClip(GF_LASeRCodec *lsr)
{
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_LSR_rectClip);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_eRR(lsr, elt);
	GF_LSR_READ_INT(lsr, flag, 1, "has_size");
	if (flag) {
		SVG_Number num;
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_size, 1, 0, &info);
		lsr_read_coordinate(lsr, & num, 0, "width");
		((LASeR_Size*)info.far_ptr)->width = num.value;
		lsr_read_coordinate(lsr, & num, 0, "height");
		((LASeR_Size*)info.far_ptr)->height = num.value;
	}
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_script(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_script);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_eRR(lsr, elt);
	lsr_read_script_type(lsr, elt);
	lsr_read_href(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_selector(GF_LASeRCodec *lsr)
{
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_LSR_selector);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_eRR(lsr, elt);
	GF_LSR_READ_INT(lsr, flag, 1, "hasChoice");
	if (flag) {
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_choice, 1, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "choice");
		if (flag) {
			GF_LSR_READ_INT(lsr, ((LASeR_Choice*)info.far_ptr)->type, 1, "type");
		} else {
			GF_LSR_READ_INT(lsr, ((LASeR_Choice*)info.far_ptr)->choice_index, 8, "value");
			((LASeR_Choice*)info.far_ptr)->type = LASeR_CHOICE_N;
		}
	}
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_set(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_set);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_attribute_name(lsr, elt);
	lsr_read_attribute_type(lsr, elt);

	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_duration(lsr, elt);
	lsr_read_anim_fill(lsr, elt);
	lsr_read_anim_repeatCount(lsr, elt);
	lsr_read_repeat_duration(lsr, elt);
	lsr_read_anim_restart(lsr, elt);
	lsr_read_anim_value(lsr, elt, TAG_SVG_ATT_to, "to");
	lsr_read_href(lsr, elt);
	lsr_read_lsr_enabled(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);

	if (!lsr_setup_smil_anim(lsr, (SVG_Element*)elt, parent)) {
		gf_list_add(lsr->defered_anims, elt);
		lsr_read_group_content_post_init(lsr, (SVG_Element*)elt, 1);
	} else {
		lsr_read_group_content(lsr, elt, 0);
	}
	return elt;
}

static GF_Node *lsr_read_simpleLayout(GF_LASeRCodec *lsr)
{
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_LSR_simpleLayout);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	GF_LSR_READ_INT(lsr, flag, 1, "has_delta");
	if (flag) {
		SVG_Number num;
		GF_FieldInfo info;
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_delta, 1, 0, &info);
		lsr_read_coordinate(lsr, & num, 0, "width");
		((LASeR_Size*)info.far_ptr)->width = num.value;
		lsr_read_coordinate(lsr, & num, 0, "height");
		((LASeR_Size*)info.far_ptr)->height = num.value;
	}
	lsr_read_eRR(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_stop(GF_LASeRCodec *lsr)
{
	GF_FieldInfo info;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_stop);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);

	lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_offset, 1, 0, &info);
	((SVG_Number*)info.far_ptr)->value = lsr_read_fixed_16_8(lsr, "offset");
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}
static GF_Node *lsr_read_svg(GF_LASeRCodec *lsr, Bool init_node)
{
	GF_FieldInfo info;
	SMIL_Duration snap;
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_svg);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_string_attribute(lsr, elt, TAG_SVG_ATT_baseProfile, "baseProfile");
	lsr_read_string_attribute(lsr, elt, TAG_SVG_ATT_contentScriptType, "contentScriptType");
	lsr_read_eRR(lsr, elt);
	lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_height, 1, 0, &info);
	lsr_read_value_with_units(lsr, info.far_ptr, "height");
	GF_LSR_READ_INT(lsr, flag, 1, "hasPlaybackOrder");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_playbackOrder, 1, 1, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "playbackOrder");
		if (flag) *(SVG_PlaybackOrder*)info.far_ptr = SVG_PLAYBACKORDER_FORWARDONLY;
	}

	lsr_read_preserve_aspect_ratio(lsr, elt);


	GF_LSR_READ_INT(lsr, flag, 1, "has_snapshotTime");
	if (flag) {
		lsr_read_duration_ex(lsr, NULL, 0, &snap, "snapshotTime", 0);
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_snapshotTime, 1, 1, &info);
		if (snap.type==SMIL_DURATION_DEFINED) *((SVG_Clock *)info.far_ptr) = snap.clock_value;
	}

	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncBehavior");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_syncBehaviorDefault, 1, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 2, "syncBehaviorDefault");
		switch (flag) {
		case 0:
			*((SMIL_SyncBehavior*)info.far_ptr) = SMIL_SYNCBEHAVIOR_CANSLIP;
			break;
		case 1:
			*((SMIL_SyncBehavior*)info.far_ptr) = SMIL_SYNCBEHAVIOR_INDEPENDENT;
			break;
		case 3:
			*((SMIL_SyncBehavior*)info.far_ptr) = SMIL_SYNCBEHAVIOR_LOCKED;
			break;
		default:
			*((SMIL_SyncBehavior*)info.far_ptr) = SMIL_SYNCBEHAVIOR_INHERIT;
			break;
		}
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasSyncToleranceDefault");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_syncToleranceDefault, 1, 0, &info);
		((SMIL_SyncTolerance*)info.far_ptr)->type = SMIL_SYNCTOLERANCE_VALUE;
		GF_LSR_READ_INT(lsr, flag, 1, "choice");
		((SMIL_SyncTolerance*)info.far_ptr)->value = lsr_read_vluimsbf5(lsr, "value");
		((SMIL_SyncTolerance*)info.far_ptr)->value /= lsr->time_resolution;
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasTimelineBegin");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_timelineBegin, 1, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "timelineBegin");
		if (flag) *(SVG_TimelineBegin*)info.far_ptr = SVG_TIMELINEBEGIN_ONLOAD;
	}
	lsr_read_string_attribute(lsr, elt, TAG_SVG_ATT_version, "version");

	GF_LSR_READ_INT(lsr, flag, 1, "hasViewBox");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_viewBox, 1, 0, &info);
		((SVG_ViewBox*)info.far_ptr)->x = lsr_read_fixed_16_8(lsr, "viewbox.x");
		((SVG_ViewBox*)info.far_ptr)->y = lsr_read_fixed_16_8(lsr, "viewbox.y");
		((SVG_ViewBox*)info.far_ptr)->width = lsr_read_fixed_16_8(lsr, "viewbox.width");
		((SVG_ViewBox*)info.far_ptr)->height = lsr_read_fixed_16_8(lsr, "viewbox.height");
		((SVG_ViewBox*)info.far_ptr)->is_set = 1;
	}

	lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_width, 1, 0, &info);
	lsr_read_value_with_units(lsr, info.far_ptr, "width");

	GF_LSR_READ_INT(lsr, flag, 1, "hasZoomAndPan");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_zoomAndPan, 1, 0, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "zoomAndPan");
		*((SVG_ZoomAndPan*)info.far_ptr) = flag ? SVG_ZOOMANDPAN_MAGNIFY : SVG_ZOOMANDPAN_DISABLE;
	}
	lsr_read_any_attribute(lsr, elt, 1);
	/*store current root for listeners with no focus target*/
	lsr->current_root = elt;

	if (init_node) {
		gf_node_register(elt, NULL);
		gf_sg_set_root_node(lsr->sg, elt);
	}

	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_switch(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_switch);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_eRR(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}


static GF_Node *lsr_read_text(GF_LASeRCodec *lsr, u32 same_type)
{
	u32 flag;
	GF_FieldInfo info;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_text);
	if (same_type) {
		if (lsr->prev_text) {
			lsr_restore_base(lsr, (SVG_Element *)elt, (SVG_Element *)lsr->prev_text, (same_type==2) ? 1 : 0, 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] sametext coded in bitstream but no text defined !\n"));
		}
		lsr_read_id(lsr, elt);
		if (same_type==2) lsr_read_fill(lsr, elt);
		lsr_read_coord_list(lsr, elt, TAG_SVG_ATT_text_x, "x");
		lsr_read_coord_list(lsr, elt, TAG_SVG_ATT_text_y, "y");
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);

		GF_LSR_READ_INT(lsr, flag, 1, "editable");
		if (flag) {
			lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_editable, 1, 0, &info);
			*(SVG_Boolean*)info.far_ptr = flag;
		}
		lsr_read_float_list(lsr, elt, TAG_SVG_ATT_text_rotate, NULL, "rotate");
		lsr_read_coord_list(lsr, elt, TAG_SVG_ATT_text_x, "x");
		lsr_read_coord_list(lsr, elt, TAG_SVG_ATT_text_y, "y");
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_text = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, same_type);
	return elt;
}

static GF_Node *lsr_read_tspan(GF_LASeRCodec *lsr)
{
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_tspan);
	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	lsr_read_fill(lsr, elt);
	lsr_read_stroke(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_use(GF_LASeRCodec *lsr, Bool is_same)
{
	GF_FieldInfo info;
	u32 flag;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_use);
	if (is_same) {
		if (lsr->prev_use) {
			lsr_restore_base(lsr, (SVG_Element *)elt, lsr->prev_use, 0, 0);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] sameuse coded in bitstream but no use defined !\n"));
		}
		lsr_read_id(lsr, elt);
		lsr_read_href(lsr, elt);
	} else {
		lsr_read_id(lsr, elt);
		lsr_read_rare_full(lsr, elt);
		lsr_read_fill(lsr, elt);
		lsr_read_stroke(lsr, elt);
		lsr_read_eRR(lsr, elt);

		GF_LSR_READ_INT(lsr, flag, 1, "hasOverflow");
		if (flag) {
			lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_overflow, 1, 0, &info);
			GF_LSR_READ_INT(lsr, *(SVG_Overflow*)info.far_ptr, 2, "overflow");
		}
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x, 1, "x");
		lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");
		lsr_read_href(lsr, elt);
		lsr_read_any_attribute(lsr, elt, 1);
		lsr->prev_use = (SVG_Element*)elt;
	}
	lsr_read_group_content(lsr, elt, is_same);
	return elt;
}

static GF_Node *lsr_read_video(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	GF_FieldInfo info;
	u32 flag;
	GF_Node*elt = gf_node_new(lsr->sg, TAG_SVG_video);
	lsr_read_id(lsr, elt);
	lsr_read_rare_full(lsr, elt);
	lsr_read_smil_times(lsr, elt, TAG_SVG_ATT_begin, NULL, "begin", 1);
	lsr_read_duration(lsr, elt);
	lsr_read_eRR(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_height, 1, "height");
	GF_LSR_READ_INT(lsr, flag, 1, "hasOverlay");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_overlay, 1, 1, &info);
		GF_LSR_READ_INT(lsr, flag, 1, "choice");
		if (flag) {
			GF_LSR_READ_INT(lsr, *(SVG_Overlay*)info.far_ptr, 1, "choice");
		} else {
			char *str = NULL;
			lsr_read_byte_align_string(lsr, & str, "overlayExt");
			if (str) gf_free(str);
		}
	}
	lsr_read_preserve_aspect_ratio(lsr, elt);
	lsr_read_anim_repeatCount(lsr, elt);
	lsr_read_repeat_duration(lsr, elt);
	lsr_read_anim_restart(lsr, elt);
	lsr_read_sync_behavior(lsr, elt);
	lsr_read_sync_tolerance(lsr, elt);
	lsr_read_transform_behavior(lsr, elt);
	lsr_read_content_type(lsr, elt);
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_width, 1, "width");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_x, 1, "x");
	lsr_read_coordinate_ptr(lsr, elt, TAG_SVG_ATT_y, 1, "y");
	lsr_read_href(lsr, elt);

	lsr_read_clip_time(lsr, elt, TAG_SVG_ATT_clipBegin, "clipBegin");
	lsr_read_clip_time(lsr, elt, TAG_SVG_ATT_clipEnd, "clipEnd");

	GF_LSR_READ_INT(lsr, flag, 1, "hasFullscreen");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_SVG_ATT_fullscreen, 1, 0, &info);
		GF_LSR_READ_INT(lsr, *(SVG_Boolean *)info.far_ptr, 1, "fullscreen");
	}

	lsr_read_sync_reference(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);
	return elt;
}

static GF_Node *lsr_read_listener(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	u32 flag;
	GF_FieldInfo info;
	XMLEV_Event *ev = NULL;
	XMLRI *observer, *target, *handler;
	GF_Node *elt = gf_node_new(lsr->sg, TAG_SVG_listener);

	observer = target = handler = NULL;

	lsr_read_id(lsr, elt);
	lsr_read_rare(lsr, elt);
	GF_LSR_READ_INT(lsr, flag, 1, "hasDefaultAction");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_defaultAction, 1, 0, &info);
		GF_LSR_READ_INT(lsr, *(XMLEV_DefaultAction*)info.far_ptr, 1, "defaultAction");
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasEvent");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_event, 1, 0, &info);
		lsr_read_event_type(lsr, info.far_ptr);
		ev = info.far_ptr;
	}
	/*create default handler but UNINITIALIZED*/
	lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_handler, 1, 0, &info);
	handler = info.far_ptr;
	GF_LSR_READ_INT(lsr, flag, 1, "hasHandler");
	if (flag) {
		lsr_read_any_uri(lsr, info.far_ptr, "handler");
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasObserver");
	/*TODO double check spec here*/
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_observer, 1, 0, &info);
		lsr_read_codec_IDREF(lsr, info.far_ptr, "observer");
		observer = info.far_ptr;
	}

	GF_LSR_READ_INT(lsr, flag, 1, "hasPhase");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_phase, 1, 0, &info);
		GF_LSR_READ_INT(lsr, *(XMLEV_Phase*)info.far_ptr, 1, "phase");
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasPropagate");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_propagate, 1, 0, &info);
		GF_LSR_READ_INT(lsr, *(XMLEV_Propagate*)info.far_ptr, 1, "propagate");
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasTarget");
	if (flag) {
		lsr->last_error = gf_node_get_attribute_by_tag(elt, TAG_XMLEV_ATT_target, 1, 0, &info);
		lsr_read_codec_IDREF(lsr, info.far_ptr, "target");
		target = info.far_ptr;
	}

	lsr_read_lsr_enabled(lsr, elt);
	lsr_read_any_attribute(lsr, elt, 1);
	lsr_read_group_content(lsr, elt, 0);

	/*register listener element*/
	{
		Bool post_pone = 0;
		SVG_Element *par = NULL;
		if (observer && observer->type == XMLRI_ELEMENTID) {
			if (observer->target) par = observer->target;
		}
		if (!par && target && (target->type == XMLRI_ELEMENTID)) {
			if (!target->target) post_pone = 1;
			else par = target->target;
		}
		if (!handler->target && !handler->string) {
			handler->type = XMLRI_ELEMENTID;
			handler->target = parent;
		}
		/*FIXME - double check with XML events*/
		if (!par && !observer) {
			/*all non-UI get attched to root*/
			if (ev && (ev->type>GF_EVENT_MOUSEWHEEL)) {
				par = (SVG_Element*) lsr->current_root;
			}
			else if (parent) par = parent;
			else par = (SVG_Element*) lsr->current_root;
		}
		if (!par) post_pone = 1;

		if (post_pone) {
			gf_list_add(lsr->defered_listeners, elt);
		} else {
			if (!par) par = parent;
			gf_node_dom_listener_add((GF_Node *)par, elt);
		}
	}
	return elt;
}

static GF_Node *lsr_read_scene_content_model(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	GF_Node *n;
	u32 ntype;
	GF_LSR_READ_INT(lsr, ntype, 6, "ch4");
	n = NULL;
	switch (ntype) {
	case LSR_SCENE_CONTENT_MODEL_a:
		n = lsr_read_a(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_animate:
		n = lsr_read_animate(lsr, parent, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_animateColor:
		n = lsr_read_animate(lsr, parent, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_animateMotion:
		n = lsr_read_animateMotion(lsr, parent);
		break;
	case LSR_SCENE_CONTENT_MODEL_animateTransform:
		n = lsr_read_animateTransform(lsr, parent);
		break;
	case LSR_SCENE_CONTENT_MODEL_audio:
		n = lsr_read_audio(lsr, parent);
		break;
	case LSR_SCENE_CONTENT_MODEL_circle:
		n = lsr_read_circle(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_conditional:
		n = lsr_read_conditional(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_cursorManager:
		n = lsr_read_cursorManager(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_defs:
		n = lsr_read_defs(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_desc:
		n = lsr_read_data(lsr, TAG_SVG_desc);
		break;
	case LSR_SCENE_CONTENT_MODEL_ellipse:
		n = lsr_read_ellipse(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_foreignObject:
		n = lsr_read_foreignObject(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_g:
		n = lsr_read_g(lsr, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_image:
		n = lsr_read_image(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_line:
		n = lsr_read_line(lsr, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_linearGradient:
		n = lsr_read_linearGradient(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_metadata:
		n = lsr_read_data(lsr, TAG_SVG_metadata);
		break;
	case LSR_SCENE_CONTENT_MODEL_mpath:
		n = lsr_read_mpath(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_path:
		n = lsr_read_path(lsr, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_polygon:
		n = lsr_read_polygon(lsr, 0, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_polyline:
		n = lsr_read_polygon(lsr, 1, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_radialGradient:
		n = lsr_read_radialGradient(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_rect:
		n = lsr_read_rect(lsr, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_rectClip:
		n = lsr_read_rectClip(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_sameg:
		n = lsr_read_g(lsr, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_sameline:
		n = lsr_read_line(lsr, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepath:
		n = lsr_read_path(lsr, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepathfill:
		n = lsr_read_path(lsr, 2);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepolygon:
		n = lsr_read_polygon(lsr, 0, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepolygonfill:
		n = lsr_read_polygon(lsr, 0, 2);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepolygonstroke:
		n = lsr_read_polygon(lsr, 0, 3);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepolyline:
		n = lsr_read_polygon(lsr, 1, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepolylinefill:
		n = lsr_read_polygon(lsr, 1, 2);
		break;
	case LSR_SCENE_CONTENT_MODEL_samepolylinestroke:
		n = lsr_read_polygon(lsr, 1, 3);
		break;
	case LSR_SCENE_CONTENT_MODEL_samerect:
		n = lsr_read_rect(lsr, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_samerectfill:
		n = lsr_read_rect(lsr, 2);
		break;
	case LSR_SCENE_CONTENT_MODEL_sametext:
		n = lsr_read_text(lsr, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_sametextfill:
		n = lsr_read_text(lsr, 2);
		break;
	case LSR_SCENE_CONTENT_MODEL_sameuse:
		n = lsr_read_use(lsr, 1);
		break;
	case LSR_SCENE_CONTENT_MODEL_script:
		n = lsr_read_script(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_selector:
		n = lsr_read_selector(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_set:
		n = lsr_read_set(lsr, parent);
		break;
	case LSR_SCENE_CONTENT_MODEL_simpleLayout:
		n = lsr_read_simpleLayout(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_stop:
		n = lsr_read_stop(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_switch:
		n = lsr_read_switch(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_text:
		n = lsr_read_text(lsr, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_title:
		n = lsr_read_data(lsr, TAG_SVG_title);
		break;
	case LSR_SCENE_CONTENT_MODEL_tspan:
		n = lsr_read_tspan(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_use:
		n = lsr_read_use(lsr, 0);
		break;
	case LSR_SCENE_CONTENT_MODEL_video:
		n = lsr_read_video(lsr, parent);
		break;
	case LSR_SCENE_CONTENT_MODEL_listener:
		n = lsr_read_listener(lsr, parent);
		break;
	case LSR_SCENE_CONTENT_MODEL_element_any:
		lsr_read_extend_class(lsr, NULL, 0, "node");
		break;
	case LSR_SCENE_CONTENT_MODEL_privateContainer:
		lsr_read_private_element_container(lsr);
		break;
	case LSR_SCENE_CONTENT_MODEL_textContent:
		lsr_read_text_content(lsr, (GF_Node*)parent);
		break;
	default:
		break;
	}
	if (n && n->sgprivate->interact && n->sgprivate->interact->dom_evt) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = GF_EVENT_LOAD;
		gf_dom_event_fire(n, &evt);
	}
	return n;
}

static GF_Node *lsr_read_update_content_model(GF_LASeRCodec *lsr, SVG_Element *parent)
{
	u32 flag;
	GF_Node *n=NULL;
	GF_LSR_READ_INT(lsr, flag, 1, "ch4");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 3, "ch61");
		switch (flag) {
		case LSR_UPDATE_CONTENT_MODEL2_conditional:
			n = lsr_read_conditional(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL2_cursorManager:
			n = lsr_read_cursorManager(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL2_extend:
			lsr_read_extend_class(lsr, NULL, 0, "extend");
			return NULL;
		case LSR_UPDATE_CONTENT_MODEL2_private:
			lsr_read_private_element_container(lsr);
			return NULL;
		case LSR_UPDATE_CONTENT_MODEL2_rectClip:
			n = lsr_read_rectClip(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL2_simpleLayout:
			n = lsr_read_simpleLayout(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL2_selector:
			n = lsr_read_selector(lsr);
			break;
		}
	} else {
		GF_LSR_READ_INT(lsr, flag, 6, "ch6");
		switch(flag) {
		case LSR_UPDATE_CONTENT_MODEL_a:
			n = lsr_read_a(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_animate:
			n = lsr_read_animate(lsr, parent, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_animateColor:
			n = lsr_read_animate(lsr, parent, 1);
			break;
		case LSR_UPDATE_CONTENT_MODEL_animateMotion:
			n = lsr_read_animateMotion(lsr, parent);
			break;
		case LSR_UPDATE_CONTENT_MODEL_animateTransform:
			n = lsr_read_animateTransform(lsr, parent);
			break;
		case LSR_UPDATE_CONTENT_MODEL_audio:
			n = lsr_read_audio(lsr, parent);
			break;
		case LSR_UPDATE_CONTENT_MODEL_circle:
			n = lsr_read_circle(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_defs:
			n = lsr_read_defs(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_desc:
			n = lsr_read_data(lsr, TAG_SVG_desc);
			break;
		case LSR_UPDATE_CONTENT_MODEL_ellipse:
			n = lsr_read_ellipse(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_foreignObject:
			n = lsr_read_foreignObject(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_g:
			n = lsr_read_g(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_image:
			n = lsr_read_image(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_line:
			n = lsr_read_line(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_linearGradient:
			n = lsr_read_linearGradient(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_metadata:
			n = lsr_read_data(lsr, TAG_SVG_metadata);
			break;
		case LSR_UPDATE_CONTENT_MODEL_mpath:
			n = lsr_read_mpath(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_path:
			n = lsr_read_path(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_polygon:
			n = lsr_read_polygon(lsr, 0, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_polyline:
			n = lsr_read_polygon(lsr, 1, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_radialGradient:
			n = lsr_read_radialGradient(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_rect:
			n = lsr_read_rect(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_script:
			n = lsr_read_script(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_set:
			n = lsr_read_set(lsr, parent);
			break;
		case LSR_UPDATE_CONTENT_MODEL_stop:
			n = lsr_read_stop(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_svg:
			n = lsr_read_svg(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_switch:
			n = lsr_read_switch(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_text:
			n = lsr_read_text(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_title:
			n = lsr_read_data(lsr, TAG_SVG_title);
			break;
		case LSR_UPDATE_CONTENT_MODEL_tspan:
			n = lsr_read_tspan(lsr);
			break;
		case LSR_UPDATE_CONTENT_MODEL_use:
			n = lsr_read_use(lsr, 0);
			break;
		case LSR_UPDATE_CONTENT_MODEL_video:
			n = lsr_read_video(lsr, parent);
			break;
		case LSR_UPDATE_CONTENT_MODEL_listener:
			n = lsr_read_listener(lsr, parent);
			break;
		}
	}
	if (n && n->sgprivate->interact && n->sgprivate->interact->dom_evt) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = GF_EVENT_LOAD;
		gf_dom_event_fire(n, &evt);
	}
	return n;
}

static void lsr_read_group_content(GF_LASeRCodec *lsr, GF_Node *elt, Bool skip_object_content)
{
	u32 i, count;
	if (lsr->last_error) return;

	if (!skip_object_content) lsr_read_object_content(lsr, (SVG_Element*)elt);


	/*node attributes are all parsed*/
	if (elt->sgprivate->tag!=TAG_SVG_script)
		gf_node_init(elt);

	GF_LSR_READ_INT(lsr, count, 1, "opt_group");
	if (count) {
		GF_ChildNodeItem *last = NULL;
		count = lsr_read_vluimsbf5(lsr, "occ0");
		for (i=0; i<count; i++) {
			GF_Node *n;
			if (lsr->last_error) break;
			n = lsr_read_scene_content_model(lsr, (SVG_Element*)elt);
			if (n) {
				gf_node_register(n, elt);
				gf_node_list_add_child_last(& ((SVG_Element*)elt)->children, n, &last);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] ############## end %s ###########\n", gf_node_get_class_name(n)));
			} else {
				/*either error or text content*/
			}
		}
	}

	if (elt->sgprivate->tag==TAG_SVG_script)
		gf_node_init(elt);
}

static void lsr_read_group_content_post_init(GF_LASeRCodec *lsr, SVG_Element *elt, Bool skip_init)
{
	u32 i, count;
	if (lsr->last_error) return;
	lsr_read_object_content(lsr, elt);

	GF_LSR_READ_INT(lsr, count, 1, "opt_group");
	if (count) {
		GF_ChildNodeItem *last = NULL;
		count = lsr_read_vluimsbf5(lsr, "occ0");
		for (i=0; i<count; i++) {
			GF_Node *n;
			if (lsr->last_error) return;
			n = lsr_read_scene_content_model(lsr, elt);
			if (n) {
				gf_node_register(n, (GF_Node*)elt);
				gf_node_list_add_child_last(&elt->children, n, &last);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[LASeR] ############## end %s ###########\n", gf_node_get_class_name(n)));
			} else {
				/*either error or text content*/
			}
		}
	}
	if (!skip_init) gf_node_init((GF_Node*)elt);
}

static void *lsr_read_update_value_indexed(GF_LASeRCodec *lsr, GF_Node*node, u32 fieldType, void *rep_val, u32 idx, Bool is_insert, Bool is_com, u32 *single_field_type)
{
	Fixed *f_val;
	SVG_Point *pt;
	SVG_Number num;

	switch (fieldType) {
	case SVG_Points_datatype/*ITYPE_point*/:
	{
		ListOfXXX *res;
		GF_SAFEALLOC(res, ListOfXXX);
		if (!res) return NULL;
		*res = gf_list_new();
		pt = (SVG_Point*)gf_malloc(sizeof(SVG_Point));
		if (pt) {
			lsr_read_coordinate(lsr, &num, 0, "coordX");
			pt->x = num.value;
			lsr_read_coordinate(lsr, &num, 0, "coordY");
			pt->y = num.value;
			gf_list_add(*res, pt);
		}
		return res;
	}
	case SMIL_KeySplines_datatype/*ITYPE_float*/:
	{
		ListOfXXX *res;
		GF_SAFEALLOC(res, ListOfXXX);
		if (!res) return NULL;
		*res = gf_list_new();
		f_val = (Fixed*)gf_malloc(sizeof(Fixed));
		if (f_val) {
			*f_val = lsr_read_fixed_16_8(lsr, "floatValue");
			gf_list_add(*res, f_val);
		}
		return res;
	}
	case SVG_StrokeDashArray_datatype:
	case SVG_ViewBox_datatype:
		f_val = (Fixed*)gf_malloc(sizeof(Fixed));
		if (!f_val) {
			lsr->last_error = GF_OUT_OF_MEM;
			return NULL;
		}
		*f_val = lsr_read_fixed_16_8(lsr, "floatValue");
		return f_val;
	case SMIL_KeyTimes_datatype/*ITYPE_keyTime*/:
	{
		ListOfXXX *res;
		GF_SAFEALLOC(res, ListOfXXX);
		if (!res) return NULL;
		*res = gf_list_new();
		f_val = lsr_read_fraction_12_item(lsr);
		if (f_val) gf_list_add(*res, f_val);
		return res;
	}
	case SMIL_KeyPoints_datatype/*ITYPE_0to1 - keyPoints*/:
	{
		ListOfXXX *res;
		GF_SAFEALLOC(res, ListOfXXX);
		if (!res) return NULL;
		*res = gf_list_new();
		pt = (SVG_Point*)gf_malloc(sizeof(SVG_Point));
		if (pt) {
			pt->x = lsr_read_fixed_clamp(lsr, "valueX");
			pt->y = lsr_read_fixed_clamp(lsr, "valueY");
			gf_list_add(*res, pt);
		}
		return res;
	}
	case SMIL_Times_datatype/*ITYPE_smil_time*/:
	{
		ListOfXXX *res;
		GF_SAFEALLOC(res, ListOfXXX);
		if (!res) return NULL;
		*res = gf_list_new();
		gf_list_add(*res, lsr_read_smil_time(lsr, node) );
		return res;
	}
	default:
		lsr_read_extension(lsr, "privateData");
		break;
	}
	return NULL;
}

static void lsr_read_update_value(GF_LASeRCodec *lsr, GF_Node *node, u32 att_tag, u32 fieldType, void *val, u32 node_tag)
{
	u32 is_default, has_escape, escape_val = 0;
	SVG_Number num, *n;

	switch (fieldType) {
	case SVG_Boolean_datatype:
		GF_LSR_READ_INT(lsr, *(SVG_Boolean*)val, 1, "val");
		break;
	case SVG_Paint_datatype:
		lsr_read_paint(lsr, (SVG_Paint*)val, "val");
		break;
	/*
		case SVG_AudioLevel_datatype:
			n = val;
			GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
			if (is_default) n->type=SVG_NUMBER_INHERIT;
			else {
				n->type = SVG_NUMBER_VALUE;
				n->value = lsr_read_fixed_clamp(lsr, "val");
			}
			break;
	*/
	case SVG_Transform_Scale_datatype:
		((SVG_Point *)val)->x = lsr_read_fixed_16_8(lsr, "scale_x");
		((SVG_Point *)val)->y = lsr_read_fixed_16_8(lsr, "scale_y");
		break;
	case LASeR_Size_datatype:
	case SVG_Transform_Translate_datatype:
		lsr_read_coordinate(lsr, &num, 0, "translation_x");
		((SVG_Point *)val)->x = num.value;
		lsr_read_coordinate(lsr, &num, 0, "translation_y");
		((SVG_Point *)val)->y = num.value;
		break;
	case SVG_Transform_Rotate_datatype:
		GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
		if (is_default) ((SVG_Point_Angle*)val)->angle = 0;
		else {
			GF_LSR_READ_INT(lsr, has_escape, 1, "escapeFlag");
			if (has_escape) {
				GF_LSR_READ_INT(lsr, escape_val, 2, "escapeEnum");
				((SVG_Point_Angle*)val)->angle = 0;
			}
			else {
				((SVG_Point_Angle*)val)->angle = lsr_read_fixed_16_8(lsr, "rotate");
			}
		}
		break;
	case SVG_Transform_datatype:
		lsr_read_matrix(lsr, val);
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
			GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
			if (is_default) n->type=SVG_NUMBER_INHERIT;
			else {
				n->type = SVG_NUMBER_VALUE;
				n->value = lsr_read_fixed_clamp(lsr, "val");
			}
			break;
		case TAG_SVG_ATT_width:
		case TAG_SVG_ATT_height:
			if (node_tag==TAG_SVG_svg) {
				lsr_read_value_with_units(lsr, n, "val");
			} else {
				lsr_read_coordinate(lsr, n, 0, "val");
			}
			break;
		default:
			GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
			if (is_default) n->type=SVG_NUMBER_INHERIT;
			else {
				GF_LSR_READ_INT(lsr, has_escape, 1, "escapeFlag");
				if (has_escape) {
					GF_LSR_READ_INT(lsr, escape_val, 2, "escapeEnum");
					n->type = SVG_NUMBER_AUTO;//only lineIncrement
				} else {
					n->type = SVG_NUMBER_VALUE;
					n->value = lsr_read_fixed_16_8(lsr, "val");
				}
			}
			break;
		}
		break;
	case SVG_Coordinate_datatype:
		n = (SVG_Number*)val;
		n->type = SVG_NUMBER_VALUE;
		lsr_read_coordinate(lsr, n, 0, "val");
		break;

	case SVG_Rotate_datatype:
		n = (SVG_Number*)val;
		GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
		if (is_default) n->type=SVG_NUMBER_INHERIT;
		else {
			GF_LSR_READ_INT(lsr, has_escape, 1, "escapeFlag");
			if (has_escape) {
				GF_LSR_READ_INT(lsr, escape_val, 2, "escapeEnum");
				n->type = escape_val ? SVG_NUMBER_AUTO_REVERSE : SVG_NUMBER_AUTO;
			} else {
				n->type = SVG_NUMBER_VALUE;
				n->value = lsr_read_fixed_16_8(lsr, "rotate");
			}
		}
		break;
	case SVG_Coordinates_datatype:
		lsr_read_float_list(lsr, NULL, 0, val, "val");
		break;
	case SVG_ViewBox_datatype:
	{
		u32 count;
		SVG_ViewBox *vb = (SVG_ViewBox *)val;
		GF_LSR_READ_INT(lsr, count, 1, "isDefault");
		if (count) {
			vb->is_set = 0;
		} else {
			vb->is_set = 1;
			GF_LSR_READ_INT(lsr, count, 1, "escapeFlag");
			count = lsr_read_vluimsbf5(lsr, "count");
			if (count) {
				vb->x = lsr_read_fixed_16_8(lsr, "val");
				count--;
			}
			if (count) {
				vb->y = lsr_read_fixed_16_8(lsr, "val");
				count--;
			}
			if (count) {
				vb->width = lsr_read_fixed_16_8(lsr, "val");
				count--;
			}
			if (count) {
				vb->height = lsr_read_fixed_16_8(lsr, "val");
			}
		}
	}
	break;
	case XMLRI_datatype:
	case SVG_Focus_datatype:
		if ((att_tag==TAG_XLINK_ATT_href) || (att_tag==TAG_SVG_ATT_syncReference)) {
			lsr_read_any_uri(lsr, (XMLRI*)val, "val");
		} else {
			Bool is_default, is_escape;
			u32 escape_val, ID;
			escape_val = ID = 0;
			is_escape = 0;
			GF_LSR_READ_INT(lsr, is_default, 1, "isDefault");
			if (!is_default) {
				GF_LSR_READ_INT(lsr, is_escape, 1, "isEscape");
				if (is_escape) {
					GF_LSR_READ_INT(lsr, escape_val, 2, "escapeEnumVal");
				} else {
					ID = lsr_read_vluimsbf5(lsr, "ID");
				}
			}
			if (att_tag==SVG_Focus_datatype) {
				if (is_default) ((SVG_Focus*)val)->type = SVG_FOCUS_AUTO;
				else if (is_escape) ((SVG_Focus*)val)->type = escape_val;
				else {
					((SVG_Focus*)val)->type = SVG_FOCUS_IRI;
					((SVG_Focus*)val)->target.type = XMLRI_ELEMENTID;
					((SVG_Focus*)val)->target.node_id = ID;
				}
			} else {
				if (is_default) ((XMLRI*)val)->type = XMLRI_STRING;
				else {
					((XMLRI *)val)->type = XMLRI_ELEMENTID;
					((XMLRI *)val)->node_id = ID;
				}
			}
		}
		break;

	case DOM_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
		lsr_read_byte_align_string(lsr, (char**)val, "val");
		break;
	case SVG_Motion_datatype:
		lsr_read_coordinate(lsr, &num, 0, "pointValueX");
		((GF_Matrix2D*)val)->m[2] = num.value;
		lsr_read_coordinate(lsr, &num, 0, "pointValueY");
		((GF_Matrix2D*)val)->m[5] = num.value;
		break;
	case SVG_Points_datatype:
		lsr_read_point_sequence(lsr, *(GF_List **)val, "val");
		break;
	case SVG_PathData_datatype:
		lsr_read_path_type(lsr, NULL, 0, (SVG_PathData*)val, "val");
		break;
	case SVG_FontFamily_datatype:
	{
		u32 idx;
		SVG_FontFamily *ff = (SVG_FontFamily *)val;
		GF_LSR_READ_INT(lsr, idx, 1, "isDefault");
		ff->type = SVG_FONTFAMILY_INHERIT;
		if (!idx) {
			char *ft;
			GF_LSR_READ_INT(lsr, idx, 1, "escapeFlag");
			idx = lsr_read_vluimsbf5(lsr, "index");
			if (ff->value) gf_free(ff->value);
			ff->value = NULL;
			ft = (char*)gf_list_get(lsr->font_table, idx);
			if (ft) {
				ff->value = gf_strdup(ft);
				ff->type = SVG_FONTFAMILY_VALUE;
			}
		}
	}
	break;
	break;
	case LASeR_Choice_datatype:
		GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
		if (is_default) ((LASeR_Choice *)val)->type = LASeR_CHOICE_ALL;
		else {
			GF_LSR_READ_INT(lsr, has_escape, 1, "escapeFlag");
			if (has_escape) {
				GF_LSR_READ_INT(lsr, escape_val, 2, "escapeEnum");
				((LASeR_Choice *)val)->type = escape_val ? LASeR_CHOICE_NONE : LASeR_CHOICE_ALL;
			} else {
				((LASeR_Choice *)val)->type = LASeR_CHOICE_N;
				((LASeR_Choice *)val)->choice_index = lsr_read_vluimsbf5(lsr, "value");
			}
		}
		break;
	default:
		if ((fieldType>=SVG_FillRule_datatype) && (fieldType<=SVG_LAST_U8_PROPERTY)) {
			/*TODO fixme, check inherit values*/
			GF_LSR_READ_INT(lsr, is_default, 1, "isDefaultValue");
			if (is_default) *(u8 *)val = 0;
			else *(u8 *)val = lsr_read_vluimsbf5(lsr, "val");
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[LASeR] Warning: update value not supported: fieldType %d - attribute tag %d\n", fieldType, att_tag));
		}
	}
	if (node) {
		//gf_node_dirty_set(node, 0, 0);
		gf_node_changed(node, NULL);
	}
}

static u32 lsr_get_attribute_name(GF_LASeRCodec *lsr)
{
	u32 val = 1;
	GF_LSR_READ_INT(lsr, val, 1, "has_attributeName");
	if (!val) return -1;

	GF_LSR_READ_INT(lsr, val, 1, "choice");
	if (val) {
		lsr_read_vluimsbf5(lsr, "item[i]");
		lsr_read_vluimsbf5(lsr, "item[i]");
		return -1;
	} else {
		GF_LSR_READ_INT(lsr, val, 8, "attributeName");
		return val;
	}
}

static GF_Err lsr_read_add_replace_insert(GF_LASeRCodec *lsr, GF_List *com_list, u32 com_type)
{
	GF_FieldInfo info;
	GF_Node *n, *operandNode, *new_node;
	GF_Command *com;
	GF_CommandField *field;
	s32 idx, att_type, op_att_type;
	u32 type, idref, op_idref = 0;

	operandNode = NULL;
	op_att_type = -1;

	att_type = lsr_get_attribute_name(lsr);

	idx = -1;
	if (com_type) {
		GF_LSR_READ_INT(lsr, type, 1, "has_index");
		if (type) idx = lsr_read_vluimsbf5(lsr, "index");
	}
	if (com_type!=3) {
		GF_LSR_READ_INT(lsr, type, 1, "has_operandAttribute");
		if (type) GF_LSR_READ_INT(lsr, op_att_type, 8, "attributeName");
		GF_LSR_READ_INT(lsr, type, 1, "has_operandElementId");
		if (type) {
			op_idref = lsr_read_codec_IDREF_command(lsr, "operandElementId");
			operandNode = gf_sg_find_node(lsr->sg, op_idref);
			if (!operandNode)
				return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	idref = lsr_read_codec_IDREF_command(lsr, "ref");

	n = gf_sg_find_node(lsr->sg, idref);
	if (!n) {
		if (!com_list) {
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	GF_LSR_READ_INT(lsr, type, 1, "has_value");
	if (type) {
		/*node or node-list replacement*/
		if (att_type==-2) {
			lsr_read_byte_align_string(lsr, NULL, "anyXML");
		}
		else if (att_type<0) {
			GF_Node *new_node;
			if (!com_type)
				return GF_NON_COMPLIANT_BITSTREAM;
			GF_LSR_READ_INT(lsr, type, 1, "isInherit");
			if (type)
				return GF_NON_COMPLIANT_BITSTREAM;
			if (idx==-1) {
				GF_LSR_READ_INT(lsr, type, 1, "escapeFlag");
				if (type)
					return GF_NON_COMPLIANT_BITSTREAM;
			}

			new_node = lsr_read_update_content_model(lsr, (idx==-1) ? NULL : (SVG_Element *)n);
			if (com_list) {
				com = gf_sg_command_new(lsr->sg, (com_type==3) ? GF_SG_LSR_INSERT : GF_SG_LSR_REPLACE);
				gf_list_add(com_list, com);
				if (n) {
					com->node = n;
					gf_node_register(com->node, NULL);
				} else {
					com->RouteID = idref;
					gf_list_add(lsr->unresolved_commands, com);
				}
				field = gf_sg_command_field_new(com);
				field->pos = idx;
				field->new_node = new_node;
				if (new_node) gf_node_register(new_node, NULL);
			} else if (com_type==3) {
				gf_node_list_insert_child(& ((SVG_Element *)n)->children, new_node, idx);
				if (new_node) gf_node_register(new_node, n);
			} else {
				/*child replacement*/
				if (idx!=-1) {
					GF_Node *old = gf_node_list_get_child( ((SVG_Element *)n)->children, idx);
					if (old)
						gf_node_replace(old, new_node, 0);
					else {
						gf_node_list_add_child( & ((SVG_Element *)n)->children, new_node);
						if (new_node) gf_node_register(new_node, n);
					}
				} else {
					/*node replacement*/
					gf_node_replace(n, new_node, 0);
				}
			}
		}
		/*value replace/add*/
		else if (com_list) {
			u32 field_type;
			Bool text_content = 0;
			com = gf_sg_command_new(lsr->sg, (com_type==0) ? GF_SG_LSR_ADD : (com_type==3) ? GF_SG_LSR_INSERT : GF_SG_LSR_REPLACE);
			field = gf_sg_command_field_new(com);
			field->pos = idx;
			field_type = 0;
			switch (att_type) {
			/*text*/
			case LSR_UPDATE_TYPE_TEXT_CONTENT:
				text_content = 1;
				break;
			/*matrix.translation, scale or rotate*/
			case LSR_UPDATE_TYPE_SCALE:
				field->fieldType = field_type = SVG_Transform_Scale_datatype;
				field->fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
				break;
			case LSR_UPDATE_TYPE_ROTATE:
				field->fieldType = field_type = SVG_Transform_Rotate_datatype;
				field->fieldIndex = TAG_SVG_ATT_transform;
				break;
			case LSR_UPDATE_TYPE_TRANSLATION:
				field->fieldType = field_type = SVG_Transform_Translate_datatype;
				field->fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
				break;
			case LSR_UPDATE_TYPE_SVG_HEIGHT:
				field->fieldIndex = TAG_SVG_ATT_height;
				field_type = field->fieldType = SVG_Length_datatype;
				break;
			case LSR_UPDATE_TYPE_SVG_WIDTH:
				field->fieldIndex = TAG_SVG_ATT_width;
				field_type = field->fieldType = SVG_Length_datatype;
				break;
			default:
				field->fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
				if (field->fieldIndex == (u32)-1) {
					lsr->last_error = GF_NON_COMPLIANT_BITSTREAM;
					gf_sg_command_del(com);
					return lsr->last_error;
				}
				field_type = field->fieldType = gf_xml_get_attribute_type(field->fieldIndex);
				break;
			}
			gf_list_add(com_list, com);
			if (n) {
				com->node = n;
				gf_node_register(com->node, NULL);
			} else {
				com->RouteID = idref;
				gf_list_add(lsr->unresolved_commands, com);
			}
			if (idx==-1) {
				if (text_content) {
					GF_DOMText *text = gf_dom_new_text_node(lsr->sg);
					gf_node_register((GF_Node *)text, NULL);
					lsr_read_byte_align_string(lsr, &text->textContent, "val");
					field->new_node = (GF_Node*)text;
				} else {
					field->field_ptr = gf_svg_create_attribute_value(field_type);
					lsr_read_update_value(lsr, NULL, field->fieldIndex, field->fieldType, field->field_ptr, n ? n->sgprivate->tag : 0);
				}
			} else {
				field->field_ptr = lsr_read_update_value_indexed(lsr, (GF_Node*)n, field_type, NULL, idx, com_type==LSR_UPDATE_INSERT, 1, &field->fieldType);
			}
		} else {
			GF_Point2D matrix_tmp;
			SVG_Point_Angle matrix_tmp_rot;
			u32 fieldIndex = 0;
			u32 field_type = 0;
			Bool text_content = 0;
			Bool is_lsr_transform = 0;
			switch (att_type) {
			/*text*/
			case LSR_UPDATE_TYPE_TEXT_CONTENT:
				text_content = 1;
				break;
			/*matrix.translation, scale or rotate*/
			case LSR_UPDATE_TYPE_SCALE:
				info.far_ptr = (void *)&matrix_tmp;
				field_type = SVG_Transform_Scale_datatype;
				is_lsr_transform = 1;
				break;
			case LSR_UPDATE_TYPE_ROTATE:
				info.far_ptr = (void *)&matrix_tmp_rot;
				field_type = SVG_Transform_Rotate_datatype;
				is_lsr_transform = 1;
				break;
			case LSR_UPDATE_TYPE_TRANSLATION:
				info.far_ptr = (void *)&matrix_tmp;
				field_type = SVG_Transform_Translate_datatype;
				is_lsr_transform = 1;
				break;
			default:
				fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
				gf_node_get_attribute_by_tag(n, fieldIndex, 1, 0, &info);
				field_type = info.fieldType;
				break;
			}
			info.fieldType = field_type;
			if (is_lsr_transform) {
				SVG_Transform *dest;
				if (idx==-1) {
					lsr_read_update_value(lsr, NULL, fieldIndex, field_type, info.far_ptr, 0);
				} else {
					assert(0);
				}


//				fieldIndex = gf_node_get_attribute_by_tag((GF_Node*)n, TAG_SVG_ATT_transform, 1, 1, &info);
				dest = (SVG_Transform *)info.far_ptr;
				if (com_type) {
					GF_Point2D scale, translate;
					SVG_Point_Angle rotate;
					if (gf_mx2d_decompose(&dest->mat, &scale, &rotate.angle, &translate)) {
						gf_mx2d_init(dest->mat);
						if (att_type==LSR_UPDATE_TYPE_SCALE) scale = matrix_tmp;
						else if (att_type==LSR_UPDATE_TYPE_TRANSLATION) translate = matrix_tmp;
						else if (att_type==LSR_UPDATE_TYPE_ROTATE) rotate = matrix_tmp_rot;

						gf_mx2d_add_scale(&dest->mat, scale.x, scale.y);
						gf_mx2d_add_rotation(&dest->mat, 0, 0, rotate.angle);
						gf_mx2d_add_translation(&dest->mat, translate.x, translate.y);
					}
				}
				else if (att_type==LSR_UPDATE_TYPE_SCALE) gf_mx2d_add_scale(&dest->mat, matrix_tmp.x, matrix_tmp.y);
				else if (att_type==LSR_UPDATE_TYPE_TRANSLATION) gf_mx2d_add_translation(&dest->mat, matrix_tmp.x, matrix_tmp.y);
				else if (att_type==LSR_UPDATE_TYPE_ROTATE) gf_mx2d_add_rotation(&dest->mat, 0, 0, matrix_tmp_rot.angle);

				gf_node_changed((GF_Node*)n, &info);
			}
			else if (com_type) {
				if (text_content) {
					GF_DOMText *t = NULL;
					if (idx>=0) {
						if (com_type==LSR_UPDATE_INSERT) {
							t = gf_dom_new_text_node(n->sgprivate->scenegraph);
							gf_node_register((GF_Node *)t, n);
							gf_node_list_insert_child(&((GF_ParentNode *)n)->children, (GF_Node*)t, idx);
						} else {
							t = (GF_DOMText *) gf_node_list_get_child(((SVG_Element*)n)->children, idx);
							if (t->sgprivate->tag!=TAG_DOMText) t = NULL;
						}
					} else {
						/*this is a replace, reset ALL node content*/
						gf_sg_parent_reset(n);
						t = gf_dom_add_text_node(n, NULL);
					}
					lsr_read_byte_align_string(lsr, t ? &t->textContent : NULL, "textContent");
					gf_node_changed(n, NULL);
				} else if (idx==-1) {
					lsr_read_update_value(lsr, (GF_Node*)n, fieldIndex, field_type, info.far_ptr, n->sgprivate->tag);
				} else {
					SVG_Point *pt;
					Fixed *v1, *v2;
					//SMIL_Time *t;
					void *prev, *new_item;
					void *tmp = lsr_read_update_value_indexed(lsr, (GF_Node*)n, field_type, info.far_ptr, idx, com_type==LSR_UPDATE_INSERT, 0, NULL);
					switch (field_type) {
					/*generic GF_List containers, no type translation needed*/
					case SMIL_KeyTimes_datatype/*ITYPE_keyTime*/:
					case SMIL_KeySplines_datatype/*ITYPE_float*/:
					case SVG_Points_datatype/*ITYPE_point*/:
					case SMIL_Times_datatype/*ITYPE_smil_time*/:
						new_item = gf_list_pop_front(*(GF_List **)tmp);
						if (com_type==LSR_UPDATE_INSERT) {
							gf_list_insert(*(SVG_Coordinates*)info.far_ptr, new_item, idx);
						} else {
							prev = gf_list_get(*(SVG_Coordinates*)info.far_ptr, idx);
							gf_free(prev);
							gf_list_rem(*(SVG_Coordinates*)info.far_ptr, idx);
							gf_list_insert(*(SVG_Coordinates*)info.far_ptr, new_item, idx);
						}
						gf_node_changed((GF_Node*)n, NULL);
						gf_list_del(*(GF_List **)tmp);
						gf_free(tmp);
						break;
					/*list of floats - to check when implementing it...*/
					case SMIL_KeyPoints_datatype/*ITYPE_0to1 - keyPoints*/:
						pt = (SVG_Point*) gf_list_pop_front(*(GF_List **)tmp);
						v1 = (Fixed *) gf_malloc(sizeof(Fixed));
						*v1 = pt->x;
						v2 = (Fixed *) gf_malloc(sizeof(Fixed));
						*v2 = pt->y;
						gf_free(pt);
						gf_list_del(*(GF_List **)tmp);
						gf_free(tmp);

						if (com_type==LSR_UPDATE_INSERT) {
							gf_list_insert(*(SVG_Coordinates*)info.far_ptr, v1, idx);
							gf_list_insert(*(SVG_Coordinates*)info.far_ptr, v2, idx+1);
						} else {
							prev = gf_list_get(*(SVG_Coordinates*)info.far_ptr, idx);
							gf_free(prev);
							gf_list_rem(*(SVG_Coordinates*)info.far_ptr, idx);
							prev = gf_list_get(*(SVG_Coordinates*)info.far_ptr, idx);
							gf_free(prev);
							gf_list_rem(*(SVG_Coordinates*)info.far_ptr, idx);
							gf_list_insert(*(SVG_Coordinates*)info.far_ptr, v1, idx);
							gf_list_insert(*(SVG_Coordinates*)info.far_ptr, v2, idx);
						}
						gf_node_changed((GF_Node*)n, NULL);
						break;
					case SVG_ViewBox_datatype:
						v1 = (Fixed*)tmp;
						switch (idx) {
						case 0:
							((SVG_ViewBox*)info.far_ptr)->x = *v1;
							break;
						case 1:
							((SVG_ViewBox*)info.far_ptr)->y = *v1;
							break;
						case 2:
							((SVG_ViewBox*)info.far_ptr)->width = *v1;
							break;
						case 3:
							((SVG_ViewBox*)info.far_ptr)->height = *v1;
							break;
						}
						gf_free(tmp);
						gf_node_changed((GF_Node*)n, NULL);
						break;
					case SVG_StrokeDashArray_datatype:
						v1 = (Fixed*)tmp;
						if (com_type==LSR_UPDATE_INSERT) {
							SVG_StrokeDashArray*da = (SVG_StrokeDashArray*)info.far_ptr;
							/*use MFFloat for insert*/
							if (gf_sg_vrml_mf_insert(&da->array, GF_SG_VRML_MFFLOAT, (void*) &v2, idx)==GF_OK)
								*v2 = *v1;
						} else {
							SVG_StrokeDashArray*da = (SVG_StrokeDashArray*)info.far_ptr;
							if (idx<(s32)da->array.count) da->array.vals[idx] = *v1;
						}
						gf_free(tmp);
						gf_node_changed((GF_Node*)n, NULL);
						break;
					default:
						gf_free(tmp);
						break;
					}
				}
			} else {
				GF_FieldInfo tmp;
				tmp = info;
				if (idx==-1) {
					tmp.far_ptr = gf_svg_create_attribute_value(info.fieldType);
					lsr_read_update_value(lsr, n, fieldIndex, field_type, tmp.far_ptr, n->sgprivate->tag);
				} else {
					tmp.far_ptr = lsr_read_update_value_indexed(lsr, n, field_type, NULL, idx, 0, 1, NULL);
				}
				gf_svg_attributes_add(&info, &tmp, &info, 0);
				gf_svg_delete_attribute_value(info.fieldType, tmp.far_ptr, gf_node_get_graph(n));
			}
		}
	}
	/*copy from node*/
	else if (operandNode && (op_att_type>=0)) {
		u32 opFieldIndex = gf_lsr_anim_type_to_attribute(op_att_type);
		if (com_list) {
			com = gf_sg_command_new(lsr->sg, com_type ? GF_SG_LSR_REPLACE : GF_SG_LSR_ADD);
			gf_list_add(com_list, com);
			com->node = n;
			gf_node_register(com->node, NULL);
			com->fromNodeID = op_idref;
			com->fromFieldIndex = opFieldIndex;
			field = gf_sg_command_field_new(com);
			field->pos = idx;
			field->fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
		} else {
			u32 fieldIndex;
			GF_FieldInfo op_info;
			fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
			gf_node_get_field(n, fieldIndex, &info);
			gf_node_get_field(operandNode, opFieldIndex, &op_info);
			if (com_type) {
				gf_svg_attributes_copy(&info, &op_info, 0);
			} else {
				gf_svg_attributes_add(&info, &op_info, &info, 0);
			}
		}
	}

	lsr_read_any_attribute(lsr, NULL, 1);

	/*if not add*/
	if (!com_type) return GF_OK;

	/*list replacement*/
	GF_LSR_READ_INT(lsr, type, 1, "opt_group");
	if (type) {
		if (!com_type /*|| (idx!=-1) */)
			return GF_NON_COMPLIANT_BITSTREAM;

		if (com_list) {
			u32 count;
			GF_ChildNodeItem *last = NULL;

			if (com_type==LSR_UPDATE_INSERT) count = 1;
			else count = lsr_read_vluimsbf5(lsr, "count");

			com = gf_sg_command_new(lsr->sg, (com_type==LSR_UPDATE_REPLACE) ? GF_SG_LSR_REPLACE : GF_SG_LSR_INSERT);
			gf_list_add(com_list, com);
			com->node = n;
			gf_node_register(com->node, NULL);
			field = gf_sg_command_field_new(com);
			field->pos = idx;

			if (!count && (att_type==LSR_UPDATE_TYPE_TEXT_CONTENT)) {
				field->fieldIndex = -1;
			} else if (count==1) {
				field->new_node = lsr_read_update_content_model(lsr, (SVG_Element *) n);
				gf_node_register(field->new_node, NULL);
				if (att_type>=0) field->fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
			} else {
				field->field_ptr = &field->node_list;
				while (count) {
					GF_Node *new_node = lsr_read_update_content_model(lsr, (SVG_Element *) n);
					gf_node_register(new_node, NULL);
					gf_node_list_add_child_last(& field->node_list, new_node, &last);
					count--;
				}
			}
		} else {
			SVG_Element*elt = (SVG_Element*)n;
			GF_ChildNodeItem *last = NULL;
			u32 count;
			if (com_type==LSR_UPDATE_INSERT) count = 1;
			else count = lsr_read_vluimsbf5(lsr, "count");

			if (com_type==LSR_UPDATE_REPLACE) {
				if (idx>=0) {
					new_node = gf_node_list_del_child_idx(&elt->children, idx);
					if (new_node) gf_node_unregister(new_node, n);
				} else {
					gf_node_unregister_children(n, elt->children);
					elt->children = NULL;
				}
			}
			if ((com_type==LSR_UPDATE_INSERT) || (gf_lsr_anim_type_to_attribute(att_type) == TAG_LSR_ATT_children)) {
				while (count) {
					new_node = lsr_read_update_content_model(lsr, elt);
					if (new_node) {
						if (idx>=0) {
							gf_node_list_insert_child(&elt->children, new_node, idx);
						} else {
							gf_node_list_add_child_last(&elt->children, new_node, &last);
						}
						gf_node_register(new_node, n);
					}
					count--;
				}
				gf_node_changed(n, NULL);
			}
			/*node replacement*/
			else if ((att_type==-1) && (count==1)) {
				new_node = lsr_read_update_content_model(lsr, elt);
				gf_node_replace((GF_Node*)elt, new_node, 0);
			}
		}
	}
	return GF_OK;
}

static GF_Err lsr_read_delete(GF_LASeRCodec *lsr, GF_List *com_list)
{
	GF_FieldInfo info;
	s32 idx, att_type;
	u32 type, idref;

	att_type = lsr_get_attribute_name(lsr);

	idx = -2;
	GF_LSR_READ_INT(lsr, type, 1, "has_index");
	if (type) idx = (s32) lsr_read_vluimsbf5(lsr, "index");
	idref = lsr_read_codec_IDREF_command(lsr, "ref");

	lsr_read_any_attribute(lsr, NULL, 1);
	if (com_list) {
		GF_Command *com;
		GF_CommandField *field;
		com = gf_sg_command_new(lsr->sg, GF_SG_LSR_DELETE);
		gf_list_add(com_list, com);
		com->node = gf_sg_find_node(lsr->sg, idref);
		if (!com->node) {
			com->RouteID = idref;
			gf_list_add(lsr->unresolved_commands, com);
		} else {
			gf_node_register(com->node, NULL);
		}

		if ((idx>=0) || (att_type>=0)) {
			field = gf_sg_command_field_new(com);
			field->pos = idx;
			if (att_type>=0) {
				field->fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
				gf_node_get_field(com->node, field->fieldIndex, &info);
				field->fieldType = info.fieldType;
			}
		}
	} else {
		s32 fieldIndex = -1;
		SVG_Element *elt = (SVG_Element *) gf_sg_find_node(lsr->sg, idref);
		if (!elt)
			return GF_NON_COMPLIANT_BITSTREAM;

		if (att_type>=0) {
			fieldIndex = gf_lsr_anim_type_to_attribute(att_type);
			gf_node_get_field((GF_Node*)elt, fieldIndex, &info);
			/*TODO implement*/
		}
		/*node deletion*/
		else if (idx>=0) {
			GF_Node *c = (GF_Node *)gf_node_list_get_child(elt->children, idx);
			if (c) {
				if (!gf_node_list_del_child( & elt->children, c))
					return GF_IO_ERR;
				gf_node_unregister(c, (GF_Node*)elt);
			}
		} else {
			gf_node_replace((GF_Node*)elt, NULL, 0);
		}
	}
	return GF_OK;
}

static GF_Err lsr_read_send_event(GF_LASeRCodec *lsr, GF_List *com_list)
{
	u32 flag, idref;
	s32 detail;
	SVG_Number x, y;
	XMLEV_Event event;
	lsr_read_event_type(lsr, &event);

	detail = 0;
	GF_LSR_READ_INT(lsr, flag, 1, "has_intvalue");
	if (flag) {
		GF_LSR_READ_INT(lsr, flag, 1, "sign");
		detail = lsr_read_vluimsbf5(lsr, "value");
		if (flag) detail = -detail;

		switch (event.type) {
		case GF_EVENT_KEYDOWN:
		case GF_EVENT_LONGKEYPRESS:
		case GF_EVENT_REPEAT_KEY:
		case GF_EVENT_SHORT_ACCESSKEY:
			detail = lsr_to_dom_key(detail);
			break;
		default:
			break;
		}
	}
	x.value = y.value = 0;
	GF_LSR_READ_INT(lsr, flag, 1, "has_pointvalue");
	if (flag) {
		lsr_read_coordinate(lsr, &x, 0, "x");
		lsr_read_coordinate(lsr, &y, 0, "y");
	}
	idref = lsr_read_codec_IDREF_command(lsr, "idref");

	GF_LSR_READ_INT(lsr, flag, 1, "has_pointvalue");
	if (flag) {
		lsr_read_byte_align_string(lsr, NULL, "string");
	}
	lsr_read_any_attribute(lsr, NULL, 1);

	if (!com_list) {
		GF_DOM_Event evt;
		GF_Node *target = gf_sg_find_node(lsr->sg, idref);
		if (!target) return GF_OK;

		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = event.type;
		evt.detail = detail ? detail : (s32) event.parameter;
		evt.clientX = FIX2INT(x.value);
		evt.clientY = FIX2INT(y.value);
		gf_dom_event_fire(target, &evt);

	} else {
		GF_Command *com = gf_sg_command_new(lsr->sg, GF_SG_LSR_SEND_EVENT);
		gf_list_add(com_list, com);
		com->node = gf_sg_find_node(lsr->sg, idref);
		if (!com->node) {
			com->RouteID = idref;
			gf_list_add(lsr->unresolved_commands, com);
		} else {
			gf_node_register(com->node, NULL);
		}
		com->send_event_integer = detail ? detail : (s32) event.parameter;
		com->send_event_name = event.type;
		com->send_event_x = FIX2INT(x.value);
		com->send_event_y = FIX2INT(y.value);
	}
	return GF_OK;
}

static GF_Err lsr_read_save(GF_LASeRCodec *lsr, GF_List *com_list)
{
	u32 i, count;
	count = lsr_read_vluimsbf5(lsr, "nbIds");
	for (i=0; i<count; i++) {
		u32 flag;
		lsr_read_vluimsbf5(lsr, "ref[[i]]");
		GF_LSR_READ_INT(lsr, flag, 1, "reserved");

		lsr_get_attribute_name(lsr);
	}
	lsr_read_byte_align_string(lsr, NULL, "groupID");
	lsr_read_any_attribute(lsr, NULL, 1);
	return GF_OK;
}

static GF_Err lsr_read_restore(GF_LASeRCodec *lsr, GF_List *com_list)
{
	lsr_read_byte_align_string(lsr, NULL, "groupID");
	lsr_read_any_attribute(lsr, NULL, 1);
	return GF_OK;
}

void lsr_exec_command_list(GF_Node *node, void *par, Bool is_destroy)
{
	GF_DOMUpdates *up = (GF_DOMUpdates *)node;
	GF_LASeRCodec *codec = (GF_LASeRCodec *)gf_node_get_private((GF_Node*)node);

	if (is_destroy || !up || (up->sgprivate->tag!=TAG_DOMUpdates)) return;
	assert(!codec->bs);

	codec->info = lsr_get_stream(codec, 0);
	if (!codec->info) return;
	codec->coord_bits = codec->info->cfg.coord_bits;
	codec->scale_bits = codec->info->cfg.scale_bits_minus_coord_bits;
	codec->time_resolution = codec->info->cfg.time_resolution;
	codec->color_scale = (1<<codec->info->cfg.colorComponentBits) - 1;
	if (codec->info->cfg.resolution >= 0)
		codec->res_factor = INT2FIX(1<<codec->info->cfg.resolution);
	else
		codec->res_factor = gf_divfix(FIX_ONE, INT2FIX(1 << (-codec->info->cfg.resolution)) );

	codec->bs = gf_bs_new(up->data, up->data_size, GF_BITSTREAM_READ);
	codec->memory_dec = 0;
	lsr_read_command_list(codec, NULL, NULL, 0);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
}

static GF_Err lsr_read_command_list(GF_LASeRCodec *lsr, GF_List *com_list, SVG_Element *cond, Bool first_imp)
{
	GF_Node *n;
	GF_Command *com;
	u32 i, type, count;

	if (cond) {
		u32 s_len;
		GF_DOMUpdates *up = gf_dom_add_updates_node((GF_Node*)cond);
		gf_node_set_callback_function((GF_Node*)up, lsr_exec_command_list);
		gf_node_set_private((GF_Node *) up, lsr);

		s_len = lsr_read_vluimsbf5(lsr, NULL/*"encoding-length" - don't log to avoid corrupting the log order!!*/);
		gf_bs_align(lsr->bs);
		/*not in memory mode, direct decode*/
		if (!lsr->memory_dec) {
			com_list = NULL;
			up->data_size = s_len;
			up->data = (char*)gf_malloc(sizeof(char)*s_len);
			gf_bs_read_data(lsr->bs, up->data, s_len);
			goto exit;
		}
		/*memory mode, decode commands*/
		else {
			com_list = up->updates;
		}
	}
	count = lsr_read_vluimsbf5(lsr, "occ0");
	if (first_imp) count++;

	for (i=0; i<count; i++) {
		GF_LSR_READ_INT(lsr, type, 4, "ch4");
		switch (type) {
		case LSR_UPDATE_ADD:
		case LSR_UPDATE_INSERT:
		case LSR_UPDATE_REPLACE:
			lsr_read_add_replace_insert(lsr, com_list, type);
			break;
		case LSR_UPDATE_DELETE:
			lsr_read_delete(lsr, com_list);
			break;
		case LSR_UPDATE_NEW_SCENE:
		case LSR_UPDATE_REFRESH_SCENE: /*TODO FIXME, depends on decoder state*/
			if (type==5) lsr_read_vluimsbf5(lsr, "time");
			lsr_read_any_attribute(lsr, NULL, 1);
			if (com_list) {
				n = lsr_read_svg(lsr, 0);
				if (!n)
					return (lsr->last_error = GF_NON_COMPLIANT_BITSTREAM);
				gf_node_register(n, NULL);
				com = gf_sg_command_new(lsr->sg, (type==5) ? GF_SG_LSR_REFRESH_SCENE : GF_SG_LSR_NEW_SCENE);
				com->node = n;
				gf_list_add(com_list, com);
			} else {
				gf_sg_reset(lsr->sg);
				gf_sg_set_scene_size_info(lsr->sg, 0, 0, 1);
				n = lsr_read_svg(lsr, 1);
				if (!n)
					return (lsr->last_error = GF_NON_COMPLIANT_BITSTREAM);
			}
			break;
		case LSR_UPDATE_TEXT_CONTENT:
			lsr_read_byte_align_string(lsr, NULL, "textContent");
			break;
		case LSR_UPDATE_SEND_EVENT:
			lsr_read_send_event(lsr, com_list);
			break;
		case LSR_UPDATE_RESTORE:
			lsr_read_restore(lsr, com_list);
			break;
		case LSR_UPDATE_SAVE:
			lsr_read_save(lsr, com_list);
			break;
		case LSR_UPDATE_EXTEND:
		{
			u32 extID;
			GF_Node *n;
			GF_LSR_READ_INT(lsr, extID, lsr->info->cfg.extensionIDBits, "extensionID");
			/*len = */lsr_read_vluimsbf5(lsr, "len");
			if (extID==2) {
				u32 j, sub_count;
				lsr_read_vluimsbf5(lsr, "reserved");
				sub_count = lsr_read_vluimsbf5(lsr, "occ2");
				for (j=0; j<sub_count; j++) {
					u32 k, occ3;
					GF_LSR_READ_INT(lsr, k, 2, "reserved");
					occ3 = lsr_read_vluimsbf5(lsr, "occ3");
					for (k=0; k<occ3; k++) {
						u32 sub_type, idref;
						GF_LSR_READ_INT(lsr, sub_type, 2, "ch5");
						switch (sub_type) {
						case 1:
						case 2:
							idref = lsr_read_codec_IDREF_command(lsr, "ref");

							n = gf_sg_find_node(lsr->sg, idref);
							if (com_list) {
								com = gf_sg_command_new(lsr->sg, (sub_type==1) ? GF_SG_LSR_ACTIVATE : GF_SG_LSR_DEACTIVATE);
								if (n) {
									com->node = n;
									gf_node_register(n, NULL);
								} else {
									com->RouteID = idref;
								}
								gf_list_add(com_list, com);
							} else if (sub_type==1) {
								if (!n) return GF_NON_COMPLIANT_BITSTREAM;
								gf_node_activate(n);
							} else {
								if (!n) return GF_NON_COMPLIANT_BITSTREAM;
								gf_node_deactivate(n);
							}
							break;
						default:
							lsr_read_private_element_container(lsr);
							break;
						}
					}
				}
			}
		}
		break;
		default:
			return (lsr->last_error = GF_NON_COMPLIANT_BITSTREAM);
		}
		/*same-coding scope is command-based (to check in the spec)*/
		if (cond) {
			lsr->prev_g = NULL;
			lsr->prev_line = NULL;
			lsr->prev_path = NULL;
			lsr->prev_polygon = NULL;
			lsr->prev_rect = NULL;
			lsr->prev_text = NULL;
			lsr->prev_use = NULL;
		}

		if (lsr->last_error)
			return lsr->last_error;
	}
exit:
	/*script is align*/
	if (cond) {
		gf_bs_align(lsr->bs);
		lsr_read_object_content(lsr, (SVG_Element*)cond);
		lsr->prev_g = NULL;
		lsr->prev_line = NULL;
		lsr->prev_path = NULL;
		lsr->prev_polygon = NULL;
		lsr->prev_rect = NULL;
		lsr->prev_text = NULL;
		lsr->prev_use = NULL;
	}
	return GF_OK;
}

static GF_Err lsr_decode_laser_unit(GF_LASeRCodec *lsr, GF_List *com_list)
{
	GF_Err e;
	Bool reset_encoding_context;
	u32 flag, i, count = 0, privateDataIdentifierIndexBits;

	lsr->last_error = GF_OK;

	/*
	 *	1 - laser unit header
	 */
	GF_LSR_READ_INT(lsr, reset_encoding_context, 1, "resetEncodingContext");
	GF_LSR_READ_INT(lsr, flag, 1, "opt_group");
	if (flag) lsr_read_extension(lsr, "ext");

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
		lsr->privateData_id_index = lsr->privateTag_index = 0;
	}

	/*
	 *	2 - codecInitialisations
	 */

	/*
	 * 2.a - condecInitialization.color
	 */
	GF_LSR_READ_INT(lsr, flag, 1, "colorInitialisation");

	if (flag) {
		count = lsr_read_vluimsbf5(lsr, "count");
		lsr->col_table = (LSRCol*)gf_realloc(lsr->col_table, sizeof(LSRCol)*(lsr->nb_cols+count));
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
	/*
	 * 2.b - condecInitialization.fonts
	 */
	GF_LSR_READ_INT(lsr, flag, 1, "fontInitialisation");
	count = 0;
	if (flag) {
		count = lsr_read_vluimsbf5(lsr, "count");
		for (i=0; i<count; i++) {
			char *ft = NULL;
			lsr_read_byte_align_string(lsr, &ft, "font");
			gf_list_add(lsr->font_table, ft);
		}
	}
	lsr->fontIndexBits = gf_get_bit_size(count);
	/*
	 * 2.c - condecInitialization.private
	 */
	GF_LSR_READ_INT(lsr, flag, 1, "privateDataIdentifierInitialisation");

	if (flag) {
		count = lsr_read_vluimsbf5(lsr, "nbPrivateDataIdentifiers");
		for (i=0; i<count; i++) {
			lsr->privateData_id_index++;
			lsr_read_byte_align_string(lsr, NULL, "privateDataIdentifier");
		}
	}
	/*
	 * 2.d - condecInitialization.anyXML
	 */
	GF_LSR_READ_INT(lsr, flag, 1, "anyXMLInitialisation");

	if (flag) {
		privateDataIdentifierIndexBits = gf_get_bit_size(lsr->privateData_id_index);
		count = lsr_read_vluimsbf5(lsr, "nbTags");
		for (i=0; i<count; i++) {
			lsr->privateTag_index++;
			if (i) {
				/* uint(privateDataIdentifierIndexBits) = */
				GF_LSR_READ_INT(lsr, flag, privateDataIdentifierIndexBits, "privateDataIdentifierIndex");
				lsr_read_byte_align_string(lsr, NULL, "tag");
			}
			GF_LSR_READ_INT(lsr, flag, 1, "hasAttrs");
			if (flag) {
				u32 k, c2 = lsr_read_vluimsbf5(lsr, "nbAttrNames");
				for (k=0; k<c2; k++) {
					if (!i) {
						/* uint(privateDataIdentifierIndexBits) = */
						GF_LSR_READ_INT(lsr, flag, privateDataIdentifierIndexBits, "privateDataIdentifierIndex");
					}
					lsr_read_byte_align_string(lsr, NULL, "tag");
				}
			}
		}
	}
	/*
	 * 2.e - condecInitialization.extension
	 */
	count = lsr_read_vluimsbf5(lsr, "countG");
	for (i=0; i<count; i++) {
		/*u32 locID = */lsr_read_vluimsbf5(lsr, "binaryIdForThisStringID");
		lsr_read_byte_align_string(lsr, NULL, "stringID");
	}
	GF_LSR_READ_INT(lsr, flag, 1, "hasExtension");
	if (flag) {
		u32 len = lsr_read_vluimsbf5(lsr, "len");
		u32 pos = gf_bs_get_bit_offset(lsr->bs);

		count = lsr_read_vluimsbf5(lsr, "len");
		for (i=0; i<count; i++) {
			/*u32 locID = */lsr_read_vluimsbf5(lsr, "localStreamIdForThisGlobal");
			lsr_read_byte_align_string(lsr, NULL, "globalName");
		}
		pos = gf_bs_get_bit_offset(lsr->bs) - pos;
		if (len<pos)
			return GF_NON_COMPLIANT_BITSTREAM;

		GF_LSR_READ_INT(lsr, flag, pos, "remainingData");
	}

	e = lsr_read_command_list(lsr, com_list, NULL, 1);
	GF_LSR_READ_INT(lsr, flag, 1, "opt_group");
	if (flag) lsr_read_extension(lsr, "ext");
	return e;
}

#endif /*GPAC_DISABLE_LASER*/
