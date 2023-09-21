/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terfsess of the GNU Lesser General Public License as published by
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

#include "filter_session.h"
#include <gpac/constants.h>
//for binxml parsing
#include <gpac/xml.h>
//for base64 decode
#include <gpac/base_coding.h>

typedef u32(*cst_parse_proto)(const char *val);
typedef const char *(*cst_name_proto)(u32 val);

static struct {
	u32 type;
	u32 (*cst_parse)(const char *val);
	const char *(*cst_name)(u32 val);
	const char *(*cst_all_names)();
} EnumProperties[] = {
	{GF_PROP_PIXFMT, (cst_parse_proto) gf_pixel_fmt_parse, (cst_name_proto) gf_pixel_fmt_name, gf_pixel_fmt_all_names},
	{GF_PROP_PCMFMT, (cst_parse_proto) gf_audio_fmt_parse, (cst_name_proto) gf_audio_fmt_name, gf_audio_fmt_all_names},
	{GF_PROP_CICP_COL_PRIM, gf_cicp_parse_color_primaries, gf_cicp_color_primaries_name, gf_cicp_color_primaries_all_names},
	{GF_PROP_CICP_COL_TFC, gf_cicp_parse_color_transfer, gf_cicp_color_transfer_name, gf_cicp_color_transfer_all_names},
	{GF_PROP_CICP_COL_MX, gf_cicp_parse_color_matrix, gf_cicp_color_matrix_name, gf_cicp_color_matrix_all_names}
};

GF_EXPORT
u32 gf_props_parse_enum(u32 type, const char *value)
{
	u32 i, count = GF_ARRAY_LENGTH(EnumProperties);
	for (i=0; i<count; i++) {
		if (EnumProperties[i].type==type)
			return EnumProperties[i].cst_parse(value);
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unrecognized constant type %d\n", type));
	return 0;
}

GF_EXPORT
const char *gf_props_enum_name(u32 type, u32 value)
{
	u32 i, count = GF_ARRAY_LENGTH(EnumProperties);
	for (i=0; i<count; i++) {
		if (EnumProperties[i].type==type)
			return EnumProperties[i].cst_name(value);
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unrecognized constant type %d\n", type));
	return NULL;
}

GF_EXPORT
const char *gf_props_enum_all_names(u32 type)
{
	u32 i, count = GF_ARRAY_LENGTH(EnumProperties);
	for (i=0; i<count; i++) {
		if (EnumProperties[i].type==type)
			return EnumProperties[i].cst_all_names();
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unrecognized constant type %d\n", type));
	return NULL;
}
GF_EXPORT
Bool gf_props_type_is_enum(GF_PropType type)
{
	if ((type>=GF_PROP_FIRST_ENUM) && (type < GF_PROP_LAST_DEFINED)) return GF_TRUE;
	return GF_FALSE;
}

GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values, char list_sep_char)
{
	GF_PropertyValue p;
	char *unit_sep=NULL;
	s32 unit = 0;
	memset(&p, 0, sizeof(GF_PropertyValue));
	p.value.data.size=0;
	p.type=type;
	if (!name) name="";

	unit_sep = NULL;
	if (value) {
		u32 len = (u32) strlen(value);
		unit_sep = len ? strrchr("kKgGmMsS", value[len-1]) : NULL;
		if (unit_sep) {
			u8 unit_char = unit_sep[0];
			if ((unit_char=='k') || (unit_char=='K')) unit = 1000;
			else if ((unit_char=='m') || (unit_char=='M')) unit = 1000000;
			else if ((unit_char=='G') || (unit_char=='g')) unit = 1000000000;
			else if ((unit_char=='s') || (unit_char=='S')) unit = 1000;
		}
		else if (type==GF_PROP_UINT) {
			unit_sep = strstr(value, "sec");
			if (unit_sep && strlen(unit_sep)==3)
				unit = 1000;
			else {
				unit_sep = strstr(value, "min");
				if (unit_sep && strlen(unit_sep)==3)
					unit = 60000;
			}
		}
	}

	switch (p.type) {
	case GF_PROP_BOOL:
		if (!value || !strcmp(value, "yes") || !strcmp(value, "true") || !strcmp(value, "1")) {
			p.value.boolean = GF_TRUE;
		} else if ( !strcmp(value, "no") || !strcmp(value, "false") || !strcmp(value, "0")) {
			p.value.boolean = GF_FALSE;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for boolean arg %s - using false\n", value, name));
			p.value.boolean = GF_FALSE;
		}
		break;
	case GF_PROP_SINT:
		if (value && !strcmp(value, "+I")) p.value.sint = GF_INT_MAX;
		else if (value && !strcmp(value, "-I")) p.value.sint = GF_INT_MIN;
		else if (!value || (sscanf(value, "%d", &p.value.sint)!=1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for int arg %s - using 0\n", value, name));
			p.value.sint = 0;
		} else if (unit) {
			p.value.sint *= unit;
		}
		break;
	case GF_PROP_UINT:
		if (value && !strcmp(value, "+I")) {
			p.value.sint = 0xFFFFFFFF;
			break;
		}

		if (!value && !enum_values) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
			break;
		}

		if (enum_values && strchr(enum_values, '|')) {
			u32 a_len = value ? (u32) strlen(value) : 0;
			u32 val = 0;
			char *str_start = (char *) enum_values;
			while (str_start) {
				u32 len;
				char *sep = strchr(str_start, '|');
				if (sep) {
					len = (u32) (sep - str_start);
				} else {
					len = (u32) strlen(str_start);
				}
				if (!a_len && len) {
					char szVal[50];
					gf_strlcpy(szVal, str_start, MIN(len+1, 50) );
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s enum %s - using `%s`\n", value, name, enum_values, szVal));
					break;
				}
				if ((a_len == len) && value && !strncmp(str_start, value, len))
					break;
				if (!sep) {
					str_start = NULL;
					break;
				}
				str_start = sep+1;
				val++;
			}
			if (!str_start) {
				//special case for enums with default set to -1
				if (value && !strcmp(value, "-1")) {
					p.value.uint = (u32) -1;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s enum %s - using 0\n", value, name, enum_values));
					p.value.uint = 0;
				}
			} else {
				p.value.uint = val;
			}
		} else if (value && !strnicmp(value, "0x", 2)) {
			if (sscanf(value, "0x%x", &p.value.uint)!=1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			} else if (unit) {
				p.value.uint *= unit;
			}
		} else if (value) {
			if (sscanf(value, "%d", &p.value.uint)!=1) {
				if (strlen(value)==4) {
					p.value.uint = GF_4CC(value[0],value[1],value[2],value[3]);
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
				}
			} else if (unit) {
				p.value.uint *= unit;
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Missing argument value for unsigned int arg %s - using 0\n", name));
		}
		break;

	case GF_PROP_4CC:
		if (value && (strlen(value)==4) ) {
			p.value.sint = GF_4CC(value[0], value[1], value[2], value[3]);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for 4CC arg %s - using 0\n", value, name));
		}
		break;

	case GF_PROP_LSINT:
		if (value && !strcmp(value, "+I")) p.value.longsint = 0x7FFFFFFFFFFFFFFFUL;
		else if (value && !strcmp(value, "-I")) p.value.longsint = 0x8000000000000000UL;
		else if (!value || (sscanf(value, ""LLD, &p.value.longsint)!=1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for long int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else if (unit) {
			p.value.longsint *= unit;
		}
		break;
	case GF_PROP_LUINT:
		if (value && !strcmp(value, "+I")) p.value.longuint = 0xFFFFFFFFFFFFFFFFUL;
		else if (!value || (sscanf(value, ""LLU, &p.value.longuint)!=1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for long unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else if (unit) {
			p.value.longuint *= unit;
		}
		break;
	case GF_PROP_FRACTION:
		if (gf_parse_frac(value, &p.value.frac)==GF_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
			p.value.frac.num = 0;
			p.value.frac.den = 1;
		}
		break;
	case GF_PROP_FRACTION64:
		if (gf_parse_lfrac(value, &p.value.lfrac)==GF_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
			p.value.lfrac.num = 0;
			p.value.lfrac.den = 1;
		}
		break;
	case GF_PROP_FLOAT:
		if (value && !strcmp(value, "+I")) p.value.fnumber = FIX_MAX;
		else if (value && !strcmp(value, "-I")) p.value.fnumber = FIX_MIN;
		else if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for float arg %s - using 0\n", value, name));
			p.value.fnumber = 0;
		} else {
			Float f;
			if (sscanf(value, "%f", &f) != 1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for float arg %s - using 0\n", value, name));
				p.value.fnumber = 0;
			} else {
				if (unit) f *= unit;
				p.value.fnumber = FLT2FIX(f);
			}
		}
		break;
	case GF_PROP_DOUBLE:
		if (value && !strcmp(value, "+I")) p.value.number = GF_MAX_DOUBLE;
		else if (value && !strcmp(value, "-I")) p.value.number = GF_MIN_DOUBLE;
		else if (value && (value[0]=='T')) {
			u32 h=0, m=0, s=0, ms=0;
			if (sscanf(value+1, "%u:%u:%u.%u", &h, &m, &s, &ms)==4) {
				p.value.number = h*3600 + m*60 + s;
				p.value.number += ((Double)ms)/1000;
			}
			else if (sscanf(value+1, "%u:%u.%u", &m, &s, &ms)==3) {
				p.value.number = m*60 + s;
				p.value.number += ((Double)ms)/1000;
			}
			else if (sscanf(value+1, "%u.%u", &s, &ms)==2) {
				p.value.number = s;
				p.value.number += ((Double)ms)/1000;
			}
			else if (sscanf(value+1, "%u:%u", &m, &s)==2) {
				if (unit_sep && ( (unit_sep[0]== 'm') || !strcmp(unit_sep, "min") ) )
					p.value.number = m*3600 + s*60;
				else
					p.value.number = m*60 + s;
			} else if (sscanf(value+1, "%u", &m)==1) {
				if (unit_sep && ( (unit_sep[0]== 'm') || !strcmp(unit_sep, "min") ) )
					p.value.number = m*60;
				else if (unit_sep && !strcmp(unit_sep, "sec"))
					p.value.number = m*3600;
				else if (unit_sep && !strcmp(unit_sep, "ms"))
					p.value.number = ((Double)m)/1000;
				else
					p.value.number = m*3600;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong time argument value %s for double arg %s - using 0\n", value, name));
				p.value.number = 0;
			}
		}
		else if (!value || (sscanf(value, "%lg", &p.value.number) != 1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for double arg %s - using 0\n", value, name));
			p.value.number = 0;
		} else if (unit) {
			p.value.number *= unit;
		}
		break;
	case GF_PROP_VEC2I:
		if (!value || (sscanf(value, "%dx%d", &p.value.vec2i.x, &p.value.vec2i.y) != 2)) {
			//allow following keywords for video resolution
			if (value && !strcmp(value, "720")) {
				p.value.vec2i.x = 1280;
				p.value.vec2i.y = 720;
			} else if (value && (!strcmp(value, "1080") || !stricmp(value, "hd")) ) {
				p.value.vec2i.x = 1920;
				p.value.vec2i.y = 1080;
			} else if (value && !strcmp(value, "360") ) {
				p.value.vec2i.x = 640;
				p.value.vec2i.y = 360;
			} else if (value && !strcmp(value, "480") ) {
				p.value.vec2i.x = 640;
				p.value.vec2i.y = 480;
			} else if (value && !strcmp(value, "576") ) {
				p.value.vec2i.x = 720;
				p.value.vec2i.y = 576;
			} else if (value && !stricmp(value, "2k") ) {
				p.value.vec2i.x = 2048;
				p.value.vec2i.y = 1080;
			} else if (value && (!strcmp(value, "2160") || !stricmp(value, "4k")) ) {
				p.value.vec2i.x = 3840;
				p.value.vec2i.y = 2160;
			} else if (value && (!strcmp(value, "4320") || !stricmp(value, "8k")) ) {
				p.value.vec2i.x = 7680;
				p.value.vec2i.y = 4320;
			} else {
				if (value && strcmp(value, "0x0") && strcmp(value, "0")) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2i arg %s - using {0,0}\n", value, name));
				}
				p.value.vec2i.x = p.value.vec2i.y = 0;
			}
		}
		break;
	case GF_PROP_VEC2:
		if (!value || (sscanf(value, "%lgx%lg", &p.value.vec2.x, &p.value.vec2.y) != 2)) {
			if (value && strcmp(value, "0x0") && strcmp(value, "0")) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2 arg %s - using {0,0}\n", value, name));
			}
			p.value.vec2.x = p.value.vec2.y = 0;
		}
		break;
	case GF_PROP_VEC3I:
		if (!value || (sscanf(value, "%dx%dx%d", &p.value.vec3i.x, &p.value.vec3i.y, &p.value.vec3i.z) != 3)) {
			if (value && strcmp(value, "0x0x0") && strcmp(value, "0")) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec3i arg %s - using {0,0,0}\n", value, name));
			}
			p.value.vec3i.x = p.value.vec3i.y = p.value.vec3i.z = 0;
		}
		break;
	case GF_PROP_VEC4I:
		if (!value || (sscanf(value, "%dx%dx%dx%d", &p.value.vec4i.x, &p.value.vec4i.y, &p.value.vec4i.z, &p.value.vec4i.w) != 4)) {
			if (value && strcmp(value, "0x0x0x0") && strcmp(value, "0")) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec4i arg %s - using {0,0,0}\n", value, name));
			}
			p.value.vec4i.x = p.value.vec4i.y = p.value.vec4i.z = p.value.vec4i.w = 0;
		}
		break;
	case GF_PROP_NAME:
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
		p.type=GF_PROP_STRING;
		if (value && !strnicmp(value, "file@", 5) ) {
			u8 *data;
			u32 len;
			GF_Err e = gf_file_load_data(value+5, (u8 **) &data, &len);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot load data from file %s\n", value+5));
			} else {
				p.value.string = data;
			}
		} else if (value && !strnicmp(value, "bxml@", 5) ) {
			GF_Err e;
			GF_DOMParser *dom = gf_xml_dom_new();
			e = gf_xml_dom_parse(dom, value+5, NULL, NULL);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot parse XML from file %s\n", value+5));
			} else {
				GF_XMLNode *root = gf_xml_dom_get_root_idx(dom, 0);
				//if no root, assume NULL
				if (root) {
					e = gf_xml_parse_bit_sequence(root, value+5, &p.value.data.ptr, &p.value.data.size);
				}
				if (e<0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to binarize XML file %s: %s\n", value+5, gf_error_to_string(e) ));
				} else {
					p.type=GF_PROP_DATA;
				}
			}
			gf_xml_dom_del(dom);
		} else {
			p.value.string = value ? gf_strdup(value) : NULL;
		}
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
	case GF_PROP_DATA_NO_COPY:
		if (!value) {
			p.value.data.ptr=NULL;
			p.value.data.size=0;
		} else if (sscanf(value, "%d@%p", &p.value.data.size,&p.value.data.ptr)==2) {
			p.type = GF_PROP_CONST_DATA;
		} else if (!strnicmp(value, "0x", 2) ) {
			u32 i;
			value += 2;
			p.value.data.size = (u32) strlen(value) / 2;
			p.value.data.ptr = gf_malloc(sizeof(char)*p.value.data.size);
			for (i=0; i<p.value.data.size; i++) {
				char szV[3];
				u32 res;
				szV[0] = value[2*i];
				szV[1] = value[2*i + 1];
				szV[2] = 0;
				sscanf(szV, "%x", &res);
				p.value.data.ptr[i] = res;
			}
		} else if (!strnicmp(value, "bxml@", 5) ) {
			GF_Err e;
			GF_DOMParser *dom = gf_xml_dom_new();
			e = gf_xml_dom_parse(dom, value+5, NULL, NULL);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot parse XML from file %s\n", value+5));
			} else {
				GF_XMLNode *root = gf_xml_dom_get_root_idx(dom, 0);
				//if no root, assume NULL
				if (root) {
					e = gf_xml_parse_bit_sequence(root, value+5, &p.value.data.ptr, &p.value.data.size);
				}
				if (e<0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to binarize XML file %s: %s\n", value+5, gf_error_to_string(e) ));
				}
			}
			gf_xml_dom_del(dom);
		} else if (!strnicmp(value, "file@", 5) ) {
			GF_Err e = gf_file_load_data(value+5, (u8 **) &p.value.data.ptr, &p.value.data.size);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot load data from file %s\n", value+5));
				p.value.data.ptr=NULL;
				p.value.data.size=0;
			}
		} else if (!strnicmp(value, "b64@", 4) ) {
			u8 *b64 = (u8 *)value + 5;
			u32 size = (u32) strlen(b64);
			p.value.data.ptr = gf_malloc(sizeof(char) * size);
			if (p.value.data.ptr) {
				p.value.data.size = gf_base64_decode((u8 *)b64, size, p.value.data.ptr, size);
				if (!p.value.data.size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to decode base64 value %s\n", value, name));
					p.type=GF_PROP_FORBIDEN;
				}
				p.value.data.ptr[p.value.data.size] = 0;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to allocate memory for decoding base64 value %s\n", value, name));
				p.value.data.size = 0;
				p.type=GF_PROP_FORBIDEN;
			}
		} else {
			p.value.data.size = (u32) strlen(value);
			if (p.value.data.size)
				p.value.data.ptr = gf_strdup(value);
			else
				p.value.data.ptr = NULL;
			p.type = GF_PROP_DATA;
		}
		break;
	case GF_PROP_POINTER:
		if (!value || (sscanf(value, "%p", &p.value.ptr) != 1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for pointer arg %s - using 0\n", value, name));
		}
		break;
	case GF_PROP_STRING_LIST:
	{
		Bool is_xml = GF_FALSE;
		char *v = (char *) value;
		if (v && v[0]=='<') is_xml = GF_TRUE;
		if (!list_sep_char) list_sep_char = ',';
		while (v) {
			u32 len=0;
			char *nv;
			char *sep = strchr(v, list_sep_char);
			if (sep && is_xml) {
				char *xml_end = strchr(v, '>');
				len = (u32) (sep - v);
				if (xml_end) {
					u32 xml_len = (u32) (xml_end - v);
					if (xml_len > len) {
						sep = strchr(xml_end, list_sep_char);
						//assigned below
//						if (sep)
//							len = (u32) (sep - v);
					}
				}
			}
			if (!sep)
			 	len = (u32) strlen(v);
			else
				len = (u32) (sep - v);

			nv = gf_malloc(sizeof(char)*(len+1));
			strncpy(nv, v, sizeof(char)*len);
			nv[len] = 0;
			if (!strnicmp(nv, "file@", 5) ) {
				u8 *data;
				u32 flen;
				GF_Err e = gf_file_load_data(nv+5, (u8 **) &data, &flen);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Cannot load data from file %s\n", nv+5));
				} else {
					p.value.string_list.vals = gf_realloc(p.value.string_list.vals, sizeof(char *) * (p.value.string_list.nb_items+1));
					p.value.string_list.vals[p.value.string_list.nb_items] = data;
					p.value.string_list.nb_items++;
				}
				gf_free(nv);
			} else {
				p.value.string_list.vals = gf_realloc(p.value.string_list.vals, sizeof(char *) * (p.value.string_list.nb_items+1));
				p.value.string_list.vals[p.value.string_list.nb_items] = nv;
				p.value.string_list.nb_items++;
			}
			if (!sep) break;
			v = sep+1;
		}
	}
		break;
	case GF_PROP_UINT_LIST:
	case GF_PROP_4CC_LIST:
	case GF_PROP_SINT_LIST:
	case GF_PROP_VEC2I_LIST:
	{
		char *v = (char *) value;
		if (!list_sep_char) list_sep_char = ',';
		while (v && v[0]) {
			char szV[100];
			u32 len=0;
			char *sep = strchr(v, list_sep_char);
			if (sep) {
				len = (u32) (sep - v);
			}
			if (!sep)
			 	len = (u32) strlen(v);
			if (len>=99) len=99;
			strncpy(szV, v, len);
			szV[len] = 0;
			if (p.type == GF_PROP_UINT_LIST) {
				u32 val_uint;
				if (sscanf(szV, "%u", &val_uint) != 1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for uint arg %s[%d] - using 0\n", value, name, p.value.uint_list.nb_items));
					val_uint = 0;
				}
				p.value.uint_list.vals = gf_realloc(p.value.uint_list.vals, (p.value.uint_list.nb_items+1) * sizeof(u32));
				p.value.uint_list.vals[p.value.uint_list.nb_items] = val_uint;
			} else if (p.type == GF_PROP_4CC_LIST) {
				u32 val_uint = gf_4cc_parse(szV);
				if (!val_uint) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for 4CC arg %s[%d] - using 0\n", value, name, p.value.uint_list.nb_items));
					val_uint = 0;
				}
				p.value.uint_list.vals = gf_realloc(p.value.uint_list.vals, (p.value.uint_list.nb_items+1) * sizeof(u32));
				p.value.uint_list.vals[p.value.uint_list.nb_items] = val_uint;
			} else if (p.type == GF_PROP_SINT_LIST) {
				s32 val_int;
				sscanf(szV, "%d", &val_int);
				p.value.sint_list.vals = gf_realloc(p.value.sint_list.vals, (p.value.sint_list.nb_items+1) * sizeof(u32));
				p.value.sint_list.vals[p.value.sint_list.nb_items] = val_int;
			} else {
				s32 v1=0, v2=0;
				if (sscanf(szV, "%dx%d", &v1, &v2) != 2) {
					if (strcmp(value, "0x0") && strcmp(value, "0")) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2i arg %s[%d] - using {0,0}\n", value, name, p.value.v2i_list.nb_items));
					}
					v1 = v2 = 0;
				}
				p.value.v2i_list.vals = gf_realloc(p.value.v2i_list.vals, (p.value.v2i_list.nb_items+1) * sizeof(GF_PropVec2i));
				p.value.v2i_list.vals[p.value.v2i_list.nb_items].x = v1;
				p.value.v2i_list.vals[p.value.v2i_list.nb_items].y = v2;
			}
			p.value.uint_list.nb_items++;
			if (!sep) break;
			v = sep+1;
		}
	}
		break;
	case GF_PROP_FORBIDEN:
	default:
		if (gf_props_type_is_enum(type)) {
			p.type = type;
			p.value.uint = gf_props_parse_enum(type, value);
			break;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Forbidden property type %d for arg %s - ignoring\n", type, name));
		p.type=GF_PROP_FORBIDEN;
		break;
	}
//	if (unit_sep) unit_sep[0] = unit_char;

	return p;
}

u32 gf_props_get_base_type(u32 type)
{
	switch (type) {
	case GF_PROP_STRING:
	case GF_PROP_NAME:
	case GF_PROP_STRING_NO_COPY:
		return GF_PROP_STRING;
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		return GF_PROP_DATA;
	case GF_PROP_UINT:
	case GF_PROP_4CC:
		return GF_PROP_UINT;
	case GF_PROP_4CC_LIST:
	case GF_PROP_UINT_LIST:
		return GF_PROP_UINT_LIST;
	default:
		//we declare const as UINT in caps
		if (gf_props_type_is_enum(type))
			return GF_PROP_UINT;
		return type;
	}
}

Bool gf_props_equal_internal(const GF_PropertyValue *p1, const GF_PropertyValue *p2, Bool strict_compare)
{
	if (p1->type!=p2->type) {
		u32 p1_base = gf_props_get_base_type(p1->type);
		u32 p2_base = gf_props_get_base_type(p2->type);

		if (p1_base != p2_base)
			return GF_FALSE;
	}

	switch (p1->type) {
	case GF_PROP_SINT: return (p1->value.sint==p2->value.sint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_UINT:
	case GF_PROP_4CC:
	 	return (p1->value.uint==p2->value.uint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_LSINT: return (p1->value.longsint==p2->value.longsint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_LUINT: return (p1->value.longuint==p2->value.longuint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_BOOL: return (p1->value.boolean==p2->value.boolean) ? GF_TRUE : GF_FALSE;
	case GF_PROP_FRACTION:
		return ((s64)p1->value.frac.num * (s64)p2->value.frac.den == (s64)p2->value.frac.num * (s64)p1->value.frac.den) ? GF_TRUE : GF_FALSE;
	case GF_PROP_FRACTION64:
		return ((s64)p1->value.lfrac.num * (s64)p2->value.lfrac.den == (s64)p2->value.lfrac.num * (s64)p1->value.lfrac.den) ? GF_TRUE : GF_FALSE;

	case GF_PROP_FLOAT: return (p1->value.fnumber==p2->value.fnumber) ? GF_TRUE : GF_FALSE;
	case GF_PROP_DOUBLE: return (p1->value.number==p2->value.number) ? GF_TRUE : GF_FALSE;
	case GF_PROP_VEC2I:
		return ((p1->value.vec2i.x==p2->value.vec2i.x) && (p1->value.vec2i.y==p2->value.vec2i.y)) ? GF_TRUE : GF_FALSE;
	case GF_PROP_VEC2:
		return ((p1->value.vec2.x==p2->value.vec2.x) && (p1->value.vec2.y==p2->value.vec2.y)) ? GF_TRUE : GF_FALSE;
	case GF_PROP_VEC3I:
		return ((p1->value.vec3i.x==p2->value.vec3i.x) && (p1->value.vec3i.y==p2->value.vec3i.y) && (p1->value.vec3i.z==p2->value.vec3i.z)) ? GF_TRUE : GF_FALSE;
	case GF_PROP_VEC4I:
		return ((p1->value.vec4i.x==p2->value.vec4i.x) && (p1->value.vec4i.y==p2->value.vec4i.y) && (p1->value.vec4i.z==p2->value.vec4i.z) && (p1->value.vec4i.w==p2->value.vec4i.w)) ? GF_TRUE : GF_FALSE;

	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
	case GF_PROP_NAME:
		if (!p1->value.string) return p2->value.string ? GF_FALSE : GF_TRUE;
		if (!p2->value.string) return GF_FALSE;
		if (!strict_compare) {
			if (!strcmp(p1->value.string, "*")) return GF_TRUE;
			if (!strcmp(p2->value.string, "*")) return GF_TRUE;
		}
		if (strchr(p2->value.string, '|')) {
			u32 len = (u32) strlen(p1->value.string);
			char *cur = p2->value.string;

			while (cur) {
				if (!strncmp(p1->value.string, cur, len) && (cur[len]=='|' || !cur[len]))
					return GF_TRUE;
				cur = strchr(cur, '|');
				if (cur) cur++;
			}
			return GF_FALSE;
		}
		if (strchr(p1->value.string, '|')) {
			u32 len = (u32) strlen(p2->value.string);
			char *cur = p1->value.string;

			while (cur) {
				if (!strncmp(p2->value.string, cur, len) && (cur[len]=='|' || !cur[len]))
					return GF_TRUE;
				cur = strchr(cur, '|');
				if (cur) cur++;
			}
			return GF_FALSE;
		}
		assert(strchr(p1->value.string, '|')==NULL);
		return !strcmp(p1->value.string, p2->value.string) ? GF_TRUE : GF_FALSE;

	case GF_PROP_DATA:
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_CONST_DATA:
		if (!p1->value.data.ptr) return p2->value.data.ptr ? GF_FALSE : GF_TRUE;
		if (!p2->value.data.ptr) return GF_FALSE;
		if (p1->value.data.size != p2->value.data.size) return GF_FALSE;
		return !memcmp(p1->value.data.ptr, p2->value.data.ptr, p1->value.data.size) ? GF_TRUE : GF_FALSE;

	case GF_PROP_STRING_LIST:
	{
		u32 i, j, c1, c2;
		c1 = p1->value.string_list.nb_items;
		c2 = p2->value.string_list.nb_items;
		if (c1 != c2) return GF_FALSE;
		for (i=0; i<c1; i++) {
			u32 found = 0;
			char *s1 = p1->value.string_list.vals[i];
			for (j=0; j<c2; j++) {
				char *s2 = p2->value.string_list.vals[j];
				if (s1 && s2 && !strcmp(s1, s2)) found++;
			}
			if (found!=1) return GF_FALSE;
		}
		return GF_TRUE;
	}
	case GF_PROP_UINT_LIST:
	case GF_PROP_4CC_LIST:
	case GF_PROP_SINT_LIST:
	case GF_PROP_VEC2I_LIST:
	{
		size_t it_size = sizeof(u32);
		if (p1->type==GF_PROP_UINT_LIST) it_size = sizeof(u32);
		else if (p1->type==GF_PROP_4CC_LIST) it_size = sizeof(u32);
		else if (p1->type==GF_PROP_SINT_LIST) it_size = sizeof(s32);
		else if (p1->type==GF_PROP_VEC2I_LIST) it_size = sizeof(GF_PropVec2i);
		//use uint list for checking
		if (p1->value.uint_list.nb_items != p2->value.uint_list.nb_items) return GF_FALSE;
		if (p1->value.uint_list.nb_items && memcmp(p1->value.uint_list.vals, p2->value.uint_list.vals, it_size * p1->value.uint_list.nb_items))
			return GF_FALSE;
		return GF_TRUE;
	}

	//user-managed pointer
	case GF_PROP_POINTER:
		return (p1->value.ptr==p2->value.ptr) ? GF_TRUE : GF_FALSE;
	default:
		if (gf_props_type_is_enum(p1->type)) {
			return (p1->value.uint==p2->value.uint) ? GF_TRUE : GF_FALSE;
		}

		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Comparing forbidden property type %d\n", p1->type));
		break;
	}
	return GF_FALSE;
}
Bool gf_props_equal(const GF_PropertyValue *p1, const GF_PropertyValue *p2)
{
	return gf_props_equal_internal(p1, p2, GF_FALSE);
}
Bool gf_props_equal_strict(const GF_PropertyValue *p1, const GF_PropertyValue *p2)
{
	return gf_props_equal_internal(p1, p2, GF_TRUE);
}

#if GF_PROPS_HASHTABLE_SIZE
GFINLINE u32 gf_props_hash_djb2(u32 p4cc, const char *str)
{
	u32 hash = 5381;

	if (p4cc) {
		hash = ((hash << 5) + hash) + ((p4cc>>24)&0xFF);
		hash = ((hash << 5) + hash) + ((p4cc>>16)&0xFF);
		hash = ((hash << 5) + hash) + ((p4cc>>8)&0xFF);
		hash = ((hash << 5) + hash) + (p4cc&0xFF);
	} else {
		int c;
		while ( (c = *str++) )
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	}
	return (hash % GF_PROPS_HASHTABLE_SIZE);
}
#endif

GF_PropertyMap * gf_props_new(GF_Filter *filter)
{
	GF_PropertyMap *map;

	map = gf_fq_pop(filter->session->prop_maps_reservoir);

	if (!map) {
		GF_SAFEALLOC(map, GF_PropertyMap);
		if (!map) return NULL;
		
		map->session = filter->session;
#if GF_PROPS_HASHTABLE_SIZE
#else
		map->properties = gf_list_new();
#endif
	}
	assert(!map->reference_count);
	map->reference_count = 1;
	return map;
}

GF_EXPORT
void gf_props_reset_single(GF_PropertyValue *p)
{
	if (p->type==GF_PROP_STRING) {
		gf_free(p->value.string);
		p->value.string = NULL;
	}
	else if (p->type==GF_PROP_DATA) {
		gf_free(p->value.data.ptr);
		p->value.data.ptr = NULL;
		p->value.data.size = 0;
	}
	//use uint_list as base type for list
	else if ((p->type==GF_PROP_UINT_LIST) || (p->type==GF_PROP_4CC_LIST) || (p->type==GF_PROP_SINT_LIST) || (p->type==GF_PROP_VEC2I_LIST)) {
		gf_free(p->value.uint_list.vals);
		p->value.uint_list.vals = NULL;
		p->value.uint_list.nb_items = 0;
	}
	else if (p->type==GF_PROP_STRING_LIST) {
		u32 i;
		for (i=0; i<p->value.string_list.nb_items; i++) {
			char *str = p->value.string_list.vals[i];
			gf_free(str);
		}
		gf_free(p->value.string_list.vals);
		p->value.string_list.vals = NULL;
		p->value.string_list.nb_items = 0;
	}
}
void gf_props_del_property(GF_PropertyEntry *it)
{
	assert(it->reference_count);
	if (safe_int_dec(&it->reference_count) == 0 ) {
		if (it->pname && it->name_alloc)
			gf_free(it->pname);

		it->name_alloc = GF_FALSE;

		if (it->prop.type==GF_PROP_STRING) {
			gf_free(it->prop.value.string);
			it->prop.value.string = NULL;
		}
		else if (it->prop.type==GF_PROP_DATA) {
			assert(it->alloc_size);
			//DATA props are collected at session level for future reuse
		}
		//string list are destroyed
		else if (it->prop.type==GF_PROP_STRING_LIST) {
			u32 i;
			for (i=0; i<it->prop.value.string_list.nb_items; i++) {
				char *s = it->prop.value.string_list.vals[i];
				gf_free(s);
			}
			gf_free(it->prop.value.string_list.vals);
			it->prop.value.string_list.vals = NULL;
			it->prop.value.string_list.nb_items = 0;
		}
		//use uint_list as base type for list
		else if ((it->prop.type==GF_PROP_UINT_LIST) || (it->prop.type==GF_PROP_4CC_LIST) || (it->prop.type==GF_PROP_SINT_LIST) || (it->prop.type==GF_PROP_VEC2I_LIST)) {
			if (it->prop.value.uint_list.vals)
				gf_free(it->prop.value.uint_list.vals);
			it->prop.value.uint_list.nb_items = 0;
			it->prop.value.uint_list.vals = NULL;
		}
		it->prop.value.data.size = 0;
		if (it->alloc_size) {
			assert(it->prop.type==GF_PROP_DATA);
			if (gf_fq_res_add(it->session->prop_maps_entry_data_alloc_reservoir, it)) {
				if (it->prop.value.data.ptr) gf_free(it->prop.value.data.ptr);
				gf_free(it);
			}
		} else {
			if (gf_fq_res_add(it->session->prop_maps_entry_reservoir, it)) {
				gf_free(it);
			}
		}
	}
}

void gf_propmap_del(void *pmap)
{
#if GF_PROPS_HASHTABLE_SIZE
	gf_free(pamp);
#else
	GF_PropertyMap *map = pmap;
	gf_list_del(map->properties);
	gf_free(map);
#endif

}
void gf_props_reset(GF_PropertyMap *prop)
{
#if GF_PROPS_HASHTABLE_SIZE
	u32 i;
	for (i=0; i<GF_PROPS_HASHTABLE_SIZE; i++) {
		if (prop->hash_table[i]) {
			GF_List *l = prop->hash_table[i];
			while (gf_list_count(l)) {
				gf_props_del_property((GF_PropertyEntry *) gf_list_pop_back(l) );
			}
			prop->hash_table[i] = NULL;
			if (gf_fq_res_add(prop->session->prop_maps_list_reservoir, l)) {
				gf_list_del(l);
			}
		}
	}
#else
	while (gf_list_count(prop->properties)) {
		gf_props_del_property( (GF_PropertyEntry *) gf_list_pop_back(prop->properties) );
	}
#endif
}

void gf_props_del(GF_PropertyMap *map)
{
	assert(!map->pckrefs_reference_count || !map->reference_count);
	//we still have a ref
	if (map->pckrefs_reference_count || map->reference_count) return;

	gf_props_reset(map);
	map->reference_count = 0;
	map->timescale = 0;
	if (!map->session || gf_fq_res_add(map->session->prop_maps_reservoir, map)) {
		gf_list_del(map->properties);
		gf_free(map);
	}
}



//purge existing property of same name
void gf_props_remove_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name)
{
#if GF_PROPS_HASHTABLE_SIZE
	if (map->hash_table[hash]) {
		u32 i, count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *prop = gf_list_get(map->hash_table[hash], i);
			if ((p4cc && (p4cc==prop->p4cc)) || (name && prop->pname && !strcmp(prop->pname, name)) ) {
				gf_list_rem(map->hash_table[hash], i);
				gf_props_del_property(prop);
				break;
			}
		}
	}
#else
	u32 i, count = gf_list_count(map->properties);
	for (i=0; i<count; i++) {
		GF_PropertyEntry *prop = gf_list_get(map->properties, i);
		if ((p4cc && (p4cc==prop->p4cc)) || (name && prop->pname && !strcmp(prop->pname, name)) ) {
			gf_list_rem(map->properties, i);
			gf_props_del_property(prop);
			break;
		}
	}
#endif
}


#if GF_PROPS_HASHTABLE_SIZE
GF_List *gf_props_get_list(GF_PropertyMap *map)
{
	GF_List *l;

	l = gf_fq_pop(map->session->prop_maps_list_reservoir);

	if (!l) l = gf_list_new();
	return l;
}
#endif

static GF_Err gf_props_assign_value(GF_PropertyEntry *prop, const GF_PropertyValue *value, Bool is_old_prop)
{
	char *src_ptr;
	//remember source pointer
	src_ptr = prop->prop.value.data.ptr;

	if (is_old_prop) {
		gf_props_reset_single(&prop->prop);
	}

	//copy prop value
	memcpy(&prop->prop, value, sizeof(GF_PropertyValue));

	if (prop->prop.type == GF_PROP_STRING) {
		prop->prop.value.string = value->value.string ? gf_strdup(value->value.string) : NULL;
		if (value->value.string && !prop->prop.value.string) return GF_OUT_OF_MEM;
	} else if (prop->prop.type == GF_PROP_STRING_NO_COPY) {
		prop->prop.value.string = value->value.string;
		prop->prop.type = GF_PROP_STRING;
	} else if (prop->prop.type == GF_PROP_DATA) {
		//restore source pointer, realloc if needed
		prop->prop.value.data.ptr = src_ptr;
		if (prop->alloc_size < value->value.data.size) {
			prop->alloc_size = value->value.data.size;
			prop->prop.value.data.ptr = gf_realloc(prop->prop.value.data.ptr, sizeof(char) * value->value.data.size);
			if (!prop->prop.value.data.ptr) {
				prop->alloc_size = 0;
				return GF_OUT_OF_MEM;
			}
			assert(prop->alloc_size);
		}
		memcpy(prop->prop.value.data.ptr, value->value.data.ptr, value->value.data.size);
	} else if (prop->prop.type == GF_PROP_DATA_NO_COPY) {
		prop->prop.type = GF_PROP_DATA;
		prop->alloc_size = value->value.data.size;
		assert(prop->alloc_size);
	}
	//use uint_list as base type for list
	else if ((prop->prop.type == GF_PROP_UINT_LIST) || (prop->prop.type == GF_PROP_4CC_LIST) || (prop->prop.type == GF_PROP_SINT_LIST) || (prop->prop.type == GF_PROP_VEC2I_LIST)) {
		size_t it_size = sizeof(u32);
		if (prop->prop.type == GF_PROP_UINT_LIST) it_size = sizeof(u32);
		else if (prop->prop.type == GF_PROP_4CC_LIST) it_size = sizeof(u32);
		else if (prop->prop.type == GF_PROP_SINT_LIST) it_size = sizeof(s32);
		else if (prop->prop.type == GF_PROP_VEC2I_LIST) it_size = sizeof(GF_PropVec2i);
		prop->prop.value.uint_list.vals = gf_malloc(it_size * value->value.uint_list.nb_items);
		if (!value->value.uint_list.nb_items) return GF_OUT_OF_MEM;
		memcpy(prop->prop.value.uint_list.vals, value->value.uint_list.vals, it_size * value->value.uint_list.nb_items);
		prop->prop.value.uint_list.nb_items = value->value.uint_list.nb_items;
	}
	else if (prop->prop.type == GF_PROP_STRING_LIST_COPY) {
		u32 i;
		prop->prop.type = GF_PROP_STRING_LIST;
		prop->prop.value.string_list.nb_items = value->value.string_list.nb_items;
		prop->prop.value.string_list.vals = gf_malloc(sizeof(char*)*value->value.string_list.nb_items);
		if (!prop->prop.value.string_list.vals) return GF_OUT_OF_MEM;
		memset(prop->prop.value.string_list.vals, 0, sizeof(char*)*value->value.string_list.nb_items);
		for (i=0; i<prop->prop.value.string_list.nb_items; i++) {
			if (value->value.string_list.vals[i]) {
				prop->prop.value.string_list.vals[i] = gf_strdup(value->value.string_list.vals[i]);
				if (!prop->prop.value.string_list.vals[i]) return GF_OUT_OF_MEM;
			}
		}
	}
	return GF_OK;
}

GF_Err gf_props_insert_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyEntry *prop;
	GF_Err e;
#if GF_PROPS_HASHTABLE_SIZE
	u32 i, count;
#endif
	if ((value->type == GF_PROP_DATA) || (value->type == GF_PROP_DATA_NO_COPY)) {
		if (!value->value.data.ptr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt at defining data property %s with NULL pointer, not allowed\n", p4cc ? gf_4cc_to_str(p4cc) : name ? name : dyn_name ));
			return GF_BAD_PARAM;
		}
	}
#if GF_PROPS_HASHTABLE_SIZE
	if (! map->hash_table[hash] ) {
		map->hash_table[hash] = gf_props_get_list(map);
		if (!map->hash_table[hash]) return GF_OUT_OF_MEM;
	} else {
		count = gf_list_count(map->hash_table[hash]);
		if (count) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("PropertyMap hash collision for %s - %d entries before insertion:\n", p4cc ? gf_4cc_to_str(p4cc) : name ? name : dyn_name, gf_list_count(map->hash_table[hash]) ));
			for (i=0; i<count; i++) {
				GF_PropertyEntry *prop_c = gf_list_get(map->hash_table[hash], i);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("\t%s\n\n", prop_c->pname ? prop_c->pname : gf_4cc_to_str(prop_c->p4cc)  ));
				assert(!prop_c->p4cc || (prop_c->p4cc != p4cc));
			}
		}
	}
#endif
	if ((value->type == GF_PROP_DATA) && value->value.data.ptr) {
		prop = gf_fq_pop(map->session->prop_maps_entry_data_alloc_reservoir);
	} else {
		prop = gf_fq_pop(map->session->prop_maps_entry_reservoir);
	}

	if (!prop) {
		GF_SAFEALLOC(prop, GF_PropertyEntry);
		if (!prop) return GF_OUT_OF_MEM;
		prop->session = map->session;
	}

	prop->reference_count = 1;
	prop->p4cc = p4cc;
	prop->pname = (char *) name;
	if (dyn_name) {
		prop->pname = gf_strdup(dyn_name);
		prop->name_alloc=GF_TRUE;
	}

	e = gf_props_assign_value(prop, value, GF_FALSE);
	if (e) {
		gf_props_del_property(prop);
		return e;
	}

#if GF_PROPS_HASHTABLE_SIZE
	return gf_list_add(map->hash_table[hash], prop);
#else
	return gf_list_add(map->properties, prop);
#endif

}

GF_Err gf_props_set_property(GF_PropertyMap *map, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_Err e;
	u32 hash = gf_props_hash_djb2(p4cc, name ? name : dyn_name);
	gf_mx_p(map->session->info_mx);
	gf_props_remove_property(map, hash, p4cc, name ? name : dyn_name);
	if (!value)
		e = GF_OK;
	else
		e = gf_props_insert_property(map, hash, p4cc, name, dyn_name, value);
	gf_mx_v(map->session->info_mx);
	return e;
}

const GF_PropertyEntry *gf_props_get_property_entry(GF_PropertyMap *map, u32 prop_4cc, const char *name)
{
	u32 i, count, len;
	const GF_PropertyEntry *res=NULL;
#if GF_PROPS_HASHTABLE_SIZE
	u32 hash = gf_props_hash_djb2(prop_4cc, name);
	if (map->hash_table[hash] ) {
		count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *p = gf_list_get(map->hash_table[hash], i);

			if ((prop_4cc && (p->p4cc==prop_4cc)) || (p->pname && name && !strcmp(p->pname, name)) ) {
				res = p;
				break;
			}
		}
	}
#else
	count = gf_list_count(map->properties);
	if (name) {
		len = (u32) strlen(name);
	} else {
		if (!prop_4cc) return NULL;
		len = 0;
	}
	for (i=0; i<count; i++) {
		GF_PropertyEntry *p = gf_list_get(map->properties, i);
		if (!p) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Concurrent read/write access to property map, cannot query property now\n"));
			return NULL;
		}

		if (prop_4cc) {
			if (p->p4cc==prop_4cc) {
				res = p;
				break;
			}
		} else if (p->pname) {
			u32 j;
			for (j=0; j<=len; j++) {
				char c = p->pname[j];
				if (!c)
					break;
				if (c != name[j])
					break;
			}
			if (j==len) {
				res = p;
				break;
			}
		}
	}
#endif
	return res;
}

const GF_PropertyValue *gf_props_get_property(GF_PropertyMap *map, u32 prop_4cc, const char *name)
{
	const GF_PropertyEntry *ent = gf_props_get_property_entry(map, prop_4cc, name);
	if (ent) return &ent->prop;
	return NULL;
}

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props, gf_filter_prop_filter filter_prop, void *cbk)
{
	GF_Err e;
	u32 i, count;
#if GF_PROPS_HASHTABLE_SIZE
	u32 idx;
#endif
	GF_List *list;
	if (src_props->timescale)
		dst_props->timescale = src_props->timescale;

#if GF_PROPS_HASHTABLE_SIZE
	for (idx=0; idx<GF_PROPS_HASHTABLE_SIZE; idx++) {
		if (src_props->hash_table[idx]) {
			list = src_props->hash_table[idx];
#else
			list = src_props->properties;
#endif
			count = gf_list_count(list);
			for (i=0; i<count; i++) {
				GF_PropertyEntry *prop = gf_list_get(list, i);
				assert(prop->reference_count);
				if (!filter_prop || filter_prop(cbk, prop->p4cc, prop->pname, &prop->prop)) {
					safe_int_inc(&prop->reference_count);

#if GF_PROPS_HASHTABLE_SIZE
					if (!dst_props->hash_table[idx]) {
						dst_props->hash_table[idx] = gf_props_get_list(dst_props);
						if (!dst_props->hash_table[idx]) return GF_OUT_OF_MEM;
					}
					e = gf_list_add(dst_props->hash_table[idx], prop);
					if (e) return e;
#else
					e = gf_list_add(dst_props->properties, prop);
					if (e) return e;
#endif
				}
			}
#if GF_PROPS_HASHTABLE_SIZE
		}
	}
#endif
	return GF_OK;
}

const GF_PropertyValue *gf_props_enum_property(GF_PropertyMap *props, u32 *io_idx, u32 *prop_4cc, const char **prop_name)
{
#if GF_PROPS_HASHTABLE_SIZE
	u32 i, nb_items = 0;
#endif
	u32 idx, count;
	
	const GF_PropertyEntry *pe;
	if (!io_idx) return NULL;

	idx = *io_idx;
	if (idx == 0xFFFFFFFF) return NULL;

#if GF_PROPS_HASHTABLE_SIZE
	for (i=0; i<GF_PROPS_HASHTABLE_SIZE; i++) {
		if (props->hash_table[i]) {
			count = gf_list_count(props->hash_table[i]);
			nb_items+=count;
			if (idx >= count) {
				idx -= count;
				continue;
			}
			pe = gf_list_get(props->hash_table[i], idx);
			if (!pe) {
				*io_idx = nb_items;
				return NULL;
			}
			if (prop_4cc) *prop_4cc = pe->p4cc;
			if (prop_name) *prop_name = pe->pname;
			*io_idx = (*io_idx) + 1;
			return &pe->prop;
		}
	}
	*io_idx = nb_items;
	return NULL;
#else
	count = gf_list_count(props->properties);
	if (idx >= count) {
		*io_idx = count;
		return NULL;
	}
	pe = gf_list_get(props->properties, idx);
	if (!pe) {
		*io_idx = count;
		return NULL;
	}
	if (prop_4cc) *prop_4cc = pe->p4cc;
	if (prop_name) *prop_name = pe->pname;
	*io_idx = (*io_idx) + 1;
	return &pe->prop;
#endif
}

typedef struct
{
	GF_PropType type;
	const char *name;
	const char *desc;
} GF_PropTypeDef;

GF_PropTypeDef PropTypes[] =
{
	{GF_PROP_SINT, "sint", "signed 32 bit integer"},
	{GF_PROP_UINT, "uint", "unsigned 32 bit integer"},
	{GF_PROP_4CC, "4cc", "Four character code"},
	{GF_PROP_LSINT, "lsint", "signed 64 bit integer"},
	{GF_PROP_LUINT, "luint", "unsigned 32 bit integer"},
	{GF_PROP_FRACTION, "frac", "32/32 bit fraction"},
	{GF_PROP_FRACTION64, "lfrac", "64/64 bit fraction"},
	{GF_PROP_BOOL, "bool", "boolean"},
	{GF_PROP_FLOAT, "flt", "32 bit float number"},
	{GF_PROP_DOUBLE, "dbl", "64 bit float number"},
	{GF_PROP_NAME, "cstr", "const UTF-8 string"},
	{GF_PROP_STRING, "str", "UTF-8 string"},
	{GF_PROP_STRING_NO_COPY, "str", "UTF-8 string"},
	{GF_PROP_DATA, "mem", "data buffer"},
	{GF_PROP_DATA_NO_COPY, "mem", "data buffer"},
	{GF_PROP_CONST_DATA, "cmem", "const data buffer"},
	{GF_PROP_POINTER, "ptr", "32 or 64 bit pointer"},
	{GF_PROP_VEC2I, "v2di", "2D 32-bit integer vector"},
	{GF_PROP_VEC2, "v2d", "2D 64-bit float vector"},
	{GF_PROP_VEC3I, "v3di", "3D 32-bit integer vector"},
	{GF_PROP_VEC4I, "v4di", "4D 32-bit integer vector"},
	{GF_PROP_STRING_LIST, "strl", "UTF-8 string list"},
	{GF_PROP_UINT_LIST, "uintl", "unsigned 32 bit integer list"},
	{GF_PROP_4CC_LIST, "4ccl", "four-character codes list"},
	{GF_PROP_SINT_LIST, "sintl", "signed 32 bit integer list"},
	{GF_PROP_VEC2I_LIST, "v2il", "2D 32-bit integer vector list"},
	{GF_PROP_PIXFMT, "pfmt", "raw pixel format"},
	{GF_PROP_PCMFMT, "afmt", "raw audio format"},
	{GF_PROP_CICP_COL_PRIM, "cprm", "color primaries, string or int value from ISO/IEC 23091-2"},
	{GF_PROP_CICP_COL_TFC, "ctfc", "color transfer characteristics, string or int value from ISO/IEC 23091-2"},
	{GF_PROP_CICP_COL_MX, "cmxc", "color matrix coefficients, string or int value from ISO/IEC 23091-2"}
};

GF_EXPORT
const char *gf_props_get_type_name(GF_PropType type)
{
	u32 i, nb_props = sizeof(PropTypes) / sizeof(GF_PropTypeDef);
	for (i=0; i<nb_props; i++) {
		if (PropTypes[i].type == type) return PropTypes[i].name;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unknown property type %d\n", type));
	return "Undefined";
}

GF_EXPORT
const char *gf_props_get_type_desc(GF_PropType type)
{
	u32 i, nb_props = sizeof(PropTypes) / sizeof(GF_PropTypeDef);
	for (i=0; i<nb_props; i++) {
		if (PropTypes[i].type == type) return PropTypes[i].desc;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unknown property type %d\n", type));
	return "Undefined";
}

GF_EXPORT
GF_PropType gf_props_parse_type(const char *name)
{
	u32 i, nb_props = sizeof(PropTypes) / sizeof(GF_PropTypeDef);
	for (i=0; i<nb_props; i++) {
		if (!strcmp(PropTypes[i].name, name)) return PropTypes[i].type;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unknown property type %s\n", name));
	return GF_PROP_FORBIDEN;
}

#ifndef GPAC_DISABLE_DOC
#define DEC_PROP(_a, _b, _c, _d) { _a, _b, _c, _d}
#define DEC_PROP_F(_a, _b, _c, _d, _e) { _a, _b, _c, _d, _e}
#else
#define DEC_PROP(_a, _b, _c, _d) { _a, _b, _d}
#define DEC_PROP_F(_a, _b, _c, _d, _e) { _a, _b, _d, _e}
#endif


GF_BuiltInProperty GF_BuiltInProps [] =
{
	DEC_PROP( GF_PROP_PID_ID, "ID", "Stream ID", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_ESID, "ESID", "MPEG-4 ESID of PID", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_ITEM_ID, "ItemID", "ID of image item in HEIF, same value as ID", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_ITEM_NUM, "ItemNumber", "Number (1-based) of image item in HEIF, in order of declaration in file", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_TRACK_NUM, "TrackNumber", "Number (1-based) of track in order of declaration in file", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_SERVICE_ID, "ServiceID", "ID of parent service", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CLOCK_ID, "ClockID", "ID of clock reference PID", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_DEPENDENCY_ID, "DependencyID", "ID of layer depended on", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_SUBLAYER, "SubLayer", "PID is a sublayer of the stream depended on rather than an enhancement layer", GF_PROP_BOOL),
	DEC_PROP_F( GF_PROP_PID_PLAYBACK_MODE, "PlaybackMode", "Playback mode supported:\n- 0: no time control\n- 1: play/pause/seek,speed=1\n- 2: play/pause/seek,speed>=0\n- 3: play/pause/seek, reverse playback", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_SCALABLE, "Scalable", "Scalable stream", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_TILE_BASE, "TileBase", "Tile base stream", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_TILE_ID, "TileID", "ID of the tile for hvt1/hvt2 PIDs", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_LANGUAGE, "Language", "Language code: ISO639 2/3 character code or RFC 4646", GF_PROP_NAME),
	DEC_PROP_F( GF_PROP_PID_SERVICE_NAME, "ServiceName", "Name of parent service", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_SERVICE_PROVIDER, "ServiceProvider", "Provider of parent service", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_STREAM_TYPE, "StreamType", "Media stream type", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_SUBTYPE, "StreamSubtype", "Media subtype 4CC (auxiliary, pic sequence, etc ..), matches ISOM handler type", GF_PROP_4CC, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_SUBTYPE, "ISOMSubtype", "ISOM media subtype 4CC (avc1 avc2...)", GF_PROP_4CC, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_ORIG_STREAM_TYPE, "OrigStreamType", "Original stream type before encryption", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_CODECID, "CodecID", "Codec ID (MPEG-4 OTI or ISOBMFF 4CC)", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_IN_IOD, "InitialObjectDescriptor", "PID is declared in the IOD for MPEG-4", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_UNFRAMED, "Unframed", "The media data is not framed, i.e. each packet is not a complete AU/frame or is not in internal format (e.g. annexB for avc/hevc, adts for aac)", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_UNFRAMED_FULL_AU, "UnframedAU", "The unframed media still has correct AU boundaries: one packet is one full AU, but the packet format might not be the internal one (e.g. annexB for avc/hevc, adts for aac)", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_UNFRAMED_LATM, "LATM", "Media is unframed AAC in LATM format", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_DURATION, "Duration", "Media duration (a negative value means an estimated duration based on rate)", GF_PROP_FRACTION64),
	DEC_PROP_F( GF_PROP_PID_NB_FRAMES, "NumFrames", "Number of frames in the stream", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FRAME_OFFSET, "FrameOffset", "Index of first frame in the stream (used for reporting)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_FRAME_SIZE, "ConstantFrameSize", "Size of the frames for constant frame size streams", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_TIMESHIFT_DEPTH, "TimeshiftDepth", "Depth of the timeshift buffer", GF_PROP_FRACTION, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_TIMESHIFT_TIME, "TimeshiftTime", "Time in the timeshift buffer in seconds - changes are signaled through PID info (no reconfigure)", GF_PROP_DOUBLE, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_TIMESHIFT_STATE, "TimeshiftState", "State of timeshift buffer: 0 is OK, 1 is underflow, 2 is overflow - changes are signaled through PID info (no reconfigure)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_TIMESCALE, "Timescale", "Media timescale (a timestamp delta of N is N/timescale seconds)", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_PROFILE_LEVEL, "ProfileLevel", "MPEG-4 profile and level", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_DECODER_CONFIG, "DecoderConfig", "Decoder configuration data", GF_PROP_DATA),
	DEC_PROP( GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, "DecoderConfigEnhancement", "Decoder configuration data of the enhancement layer(s). Also used by 3GPP/Apple text streams to give the full sample description table used in SDP.", GF_PROP_DATA),
	DEC_PROP( GF_PROP_PID_DSI_SUPERSET, "DSISuperset", "Decoder config is a superset of previous decoder config", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_CONFIG_IDX, "DecoderConfigIndex", "1-based index of decoder config for ISO base media files", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_SAMPLE_RATE, "SampleRate", "Audio sample rate", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_SAMPLES_PER_FRAME, "SamplesPerFrame", "Number of audio sample in one coded frame", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_NUM_CHANNELS, "NumChannels", "Number of audio channels", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_AUDIO_BPS, "BPS", "Number of bits per sample in compressed source", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_CHANNEL_LAYOUT, "ChannelLayout", "Channel Layout mask", GF_PROP_LUINT),
	DEC_PROP( GF_PROP_PID_AUDIO_FORMAT, "AudioFormat", "Audio sample format", GF_PROP_PCMFMT),
	DEC_PROP_F( GF_PROP_PID_AUDIO_SPEED, "AudioPlaybackSpeed", "Audio playback speed, only used for audio output reconfiguration", GF_PROP_DOUBLE, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_DELAY, "Delay", "Delay of presentation compared to composition timestamps, in media timescale. Positive value imply holding (delaying) the stream. Negative value imply skipping the beginning of stream", GF_PROP_LSINT),
	DEC_PROP( GF_PROP_PID_CTS_SHIFT, "CTSShift", "CTS offset to apply in case of negative ctts", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_NO_PRIMING, "SkipPriming", "Audio priming shall not to be removed when initializing decoding", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_WIDTH, "Width", "Visual Width (video / text / graphics)", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_HEIGHT, "Height", "Visual Height (video / text / graphics)", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_PIXFMT, "PixelFormat", "Pixel format", GF_PROP_PIXFMT),
	DEC_PROP( GF_PROP_PID_PIXFMT_WRAPPED, "PixelFormatWrapped", "Underlying pixel format of video stream if pixel format is external GL texture", GF_PROP_PIXFMT),
	DEC_PROP( GF_PROP_PID_STRIDE, "Stride", "Image or Y/alpha plane stride", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_STRIDE_UV, "StrideUV", "UV plane or U/V planes stride", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_BIT_DEPTH_Y, "BitDepthLuma", "Bit depth for luma components", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_BIT_DEPTH_UV, "BitDepthChroma", "Bit depth for chroma components", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_FPS, "FPS", "Video framerate", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_INTERLACED, "Interlaced", "Video is interlaced", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_SAR, "SAR", "Sample (i.e. pixel) aspect ratio", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_WIDTH_MAX, "MaxWidth", "Maximum width (video / text / graphics) of all enhancement layers", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_HEIGHT_MAX, "MaxHeight", "Maximum height (video / text / graphics) of all enhancement layers", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_ZORDER, "ZOrder", "Z-order of the video, from 0 (first) to max int (last)", GF_PROP_SINT),
	DEC_PROP( GF_PROP_PID_TRANS_X, "TransX", "Horizontal translation of the video (positive towards right)", GF_PROP_SINT),
	DEC_PROP( GF_PROP_PID_TRANS_Y, "TransY", "Vertical translation of the video (positive towards up)", GF_PROP_SINT),
	DEC_PROP( GF_PROP_PID_TRANS_X_INV, "TransXRight", "Horizontal offset of the video from right (positive towards right), for cases where reference width is unknown", GF_PROP_SINT),
	DEC_PROP( GF_PROP_PID_TRANS_Y_INV, "TransYTop", "Vertical translation of the video (0 is top, positive towards down), for cases where reference height is unknown", GF_PROP_SINT),
	DEC_PROP( GF_PROP_PID_HIDDEN, "Hidden", "PID is hidden in visual/audio rendering", GF_PROP_BOOL),

	DEC_PROP( GF_PROP_PID_CROP_POS, "CropOrigin", "Position in source window, X,Y indicate coordinates in source (0,0 for top-left)", GF_PROP_VEC2I),
	DEC_PROP( GF_PROP_PID_ORIG_SIZE, "OriginalSize", "Original resolution of video", GF_PROP_VEC2I),
	DEC_PROP( GF_PROP_PID_SRD, "SRD", "Position and size of the video in the referential given by SRDRef", GF_PROP_VEC4I),
	DEC_PROP( GF_PROP_PID_SRD_REF, "SRDRef", "Width and Height of the SRD referential", GF_PROP_VEC2I),
	DEC_PROP( GF_PROP_PID_SRD_MAP, "SRDMap", "Mapping of input videos in reconstructed video, expressed as {Ox,Oy,Ow,Oh,Dx,Dy,Dw,Dh} per input, with:\n"
	"- Ox,Oy,Ow,Oh: position and size of the input video (usually matching its `SRD` property), expressed in the output referential given by `SRDRef`\n"
	"- Dx,Dy,Dw,Dh: Position and Size of the input video in the reconstructed output, expressed in the output referential given by `SRDRef`", GF_PROP_UINT_LIST),

	DEC_PROP( GF_PROP_PID_ALPHA, "Alpha", "Video in this PID is an alpha map", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_MIRROR, "Mirror", "Mirror mode (as bit mask with flags 0: no mirror, 1: along Y-axis, 2: along X-axis)", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_ROTATE, "Rotate", "Video rotation as value*90 degree anti-clockwise", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_CLAP_W, "ClapW", "Width of clean aperture in luma pixels", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_CLAP_H, "ClapH", "Height of clean aperture in luma pixels", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_CLAP_X, "ClapX", "Horizontal offset of clean aperture center in luma pixels, 0 at image center", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_CLAP_Y, "ClapY", "Vertical offset of clean aperture center in luma pixels, 0 at image center", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_NUM_VIEWS, "NumViews", "Number of views packed in a frame (top-to-bottom only)", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_BITRATE, "Bitrate", "Bitrate in bps", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_MAXRATE, "Maxrate", "Max bitrate in bps", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_TARGET_RATE, "TargetRate", "Target bitrate in bps, used to setup encoders", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_DBSIZE, "DBSize", "Decode buffer size in bytes", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_MEDIA_DATA_SIZE, "MediaDataSize", "Size in bytes of media data", GF_PROP_LUINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CAN_DATAREF, "DataRef", "Data referencing is possible (each compressed frame is a continuous set of bytes in source, with no transformation)", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_URL, "URL", "URL of source", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_REMOTE_URL, "RemoteURL", "Remote URL of source - used for MPEG-4 systems", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_REDIRECT_URL, "RedirectURL", "Redirection URL of source", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FILEPATH, "SourcePath", "Path of source file on file system", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MIME, "MIMEType", "MIME type of source", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FILE_EXT, "Extension", "File extension of source", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FILE_CACHED, "Cached", "File is completely cached", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DOWN_RATE, "DownloadRate", "Download rate of resource in bits per second - changes are signaled through PID info (no reconfigure)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DOWN_SIZE, "DownloadSize", "Size of resource in bytes", GF_PROP_LUINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DOWN_BYTES, "DownBytes", "Number of bytes downloaded - changes are signaled through PID info (no reconfigure)", GF_PROP_LUINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FILE_RANGE, "ByteRange", "Byte range of resource", GF_PROP_FRACTION64, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_DISABLE_PROGRESSIVE, "DisableProgressive", "Some blocks in file need patching (replace or insertion) upon closing, potentially disabling progressive upload", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_ISOM_BRANDS, "IsoAltBrands", "ISOBMFF brands associated with PID/file", GF_PROP_4CC_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_MBRAND, "IsoBrand", "ISOBMFF major brand associated with PID/file", GF_PROP_4CC, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_MOVIE_TIME, "MovieTime", "ISOBMFF movie header duration and timescale", GF_PROP_FRACTION64, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_HAS_SYNC, "HasSync", "PID has sync points", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_SERVICE_WIDTH, "ServiceWidth", "Display width of service", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_SERVICE_HEIGHT, "ServiceHeight", "Display height of service", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_IS_DEFAULT, "IsDefault", "Default PID for this stream type", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CAROUSEL_RATE, "CarouselRate", "Repeat rate in ms for systems carousel data", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AUDIO_VOLUME, "AudioVolume", "Volume of audio", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AUDIO_PAN, "AudioPan", "Balance/Pan of audio", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AUDIO_PRIORITY, "AudioPriority", "Audio thread priority", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_PROTECTION_SCHEME_TYPE, "ProtectionScheme", "Protection scheme type (4CC) used", GF_PROP_4CC),
	DEC_PROP( GF_PROP_PID_PROTECTION_SCHEME_VERSION, "SchemeVersion", "Protection scheme version used", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_PROTECTION_SCHEME_URI, "SchemeURI", "Protection scheme URI", GF_PROP_STRING),
	DEC_PROP( GF_PROP_PID_PROTECTION_KMS_URI, "KMS_URI", "URI for key management system", GF_PROP_STRING),
	DEC_PROP( GF_PROP_PID_ISMA_SELECTIVE_ENC, "SelectiveEncryption", "ISMA/OMA selective encryption is used", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_ISMA_IV_LENGTH, "IVLength", "ISMA IV size", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_ISMA_KI_LENGTH, "KILength", "ISMA KeyIndication size", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_OMA_CRYPT_TYPE, "CryptType", "OMA encryption type", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_OMA_CID, "ContentID", "OMA Content ID", GF_PROP_STRING),
	DEC_PROP( GF_PROP_PID_OMA_TXT_HDR, "TextualHeaders", "OMA textual headers", GF_PROP_STRING),
	DEC_PROP( GF_PROP_PID_OMA_CLEAR_LEN, "PlaintextLen", "OMA size of plaintext data", GF_PROP_LUINT),
	DEC_PROP_F( GF_PROP_PID_CRYPT_INFO, "CryptInfo", "URL (local file only) of crypt info file for this PID, use `clear` to force passthrough", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DECRYPT_INFO, "DecryptInfo", "URL (local file only) of crypt info file for this PID - see decrypter help", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_SENDER_NTP, "SenderNTP", "NTP 64 bits timestamp at sender side or grabber side", GF_PROP_LUINT, GF_PROP_FLAG_PCK | GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_RECEIVER_NTP, "ReceiverNTP", "Receiver NTP (64 bits timestamp) usually associated with the sender NTP property", GF_PROP_LUINT, GF_PROP_FLAG_PCK | GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_UTC_TIME, "UTC", "UTC timestamp (in milliseconds) of parent packet", GF_PROP_LUINT, GF_PROP_FLAG_PCK | GF_PROP_FLAG_GSF_REM),

	DEC_PROP( GF_PROP_PID_ENCRYPTED, "Encrypted", "Packets for the stream are by default encrypted (however the encryption state is carried in packet crypt flags) - changes are signaled through PID info change (no reconfigure)", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_OMA_PREVIEW_RANGE, "OMAPreview", "OMA Preview range ", GF_PROP_LUINT),
	DEC_PROP( GF_PROP_PID_CENC_PSSH, "CENC_PSSH", "PSSH blob for CENC, formatted as (u32)NbSystems [ (bin128)SystemID(u32)version(u32)KID_count[ (bin128)keyID ] (u32)priv_size(char*priv_size)priv_data]", GF_PROP_DATA),
	DEC_PROP_F( GF_PROP_PCK_CENC_SAI, "CENC_SAI", "CENC SAI for the packet, formatted as (char(IV_Size))IV(u16)NbSubSamples [(u16)ClearBytes(u32)CryptedBytes]", GF_PROP_DATA, GF_PROP_FLAG_PCK),
	DEC_PROP( GF_PROP_PID_CENC_KEY_INFO, "KeyInfo", "Multi key info formatted as:\n `is_mkey(u8);\nnb_keys(u16);\n[\n\tIV_size(u8);\n\tKID(bin128);\n\tif (!IV_size) {;\n\t\tconst_IV_size(u8);\n\t\tconstIV(const_IV_size);\n}\n]\n`", GF_PROP_DATA),
	DEC_PROP( GF_PROP_PID_CENC_PATTERN, "CENCPattern", "CENC crypt pattern, CENC pattern, skip as frac.num crypt as frac.den", GF_PROP_FRACTION),
	DEC_PROP( GF_PROP_PID_CENC_STORE, "CENCStore", "Storage location 4CC of SAI data", GF_PROP_4CC),
	DEC_PROP( GF_PROP_PID_CENC_STSD_MODE, "CENCstsdMode", "Mode for CENC sample description when using clear samples:\n"
	"- 0: single sample description is used\n"
	"- 1: a clear clone of the sample description is created, inserted before the CENC sample description\n"
	"- 2: a clear clone of the sample description is created, inserted after the CENC sample description", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PID_AMR_MODE_SET, "AMRModeSet", "ModeSet for AMR and AMR-WideBand", GF_PROP_UINT),
	DEC_PROP( GF_PROP_PCK_SUBS, "SubSampleInfo", "Binary blob describing N subsamples of the sample, formatted as N [(u32)flags(u32)size(u32)codec_param(u8)priority(u8) discardable]. Subsamples for a given flag MUST appear in order, however flags can be interleaved", GF_PROP_DATA),
	DEC_PROP( GF_PROP_PID_MAX_NALU_SIZE, "NALUMaxSize", "Max size of NAL units in stream - changes are signaled through PID info change (no reconfigure)", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PCK_FILENUM, "FileNumber", "Index of file when dumping to files", GF_PROP_UINT, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_FILENAME, "FileName", "Name of output file when dumping / dashing. Must be set on first packet belonging to new file", GF_PROP_STRING, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_IDXFILENAME, "IDXName", "Name of index file when dashing MPEG-2 TS. Must be set on first packet belonging to new file", GF_PROP_STRING, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_FILESUF, "FileSuffix", "File suffix name, replacement for $FS$ in tile templates", GF_PROP_STRING, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_EODS, "EODS", "End of DASH segment", GF_PROP_BOOL, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_CUE_START, "CueStart", "Set on packets marking the beginning of a DASH/HLS segment for cue-driven segmentation - see dasher help", GF_PROP_BOOL, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_MEDIA_TIME, "MediaTime", "Corresponding media time of the parent packet (0 being the origin)", GF_PROP_DOUBLE, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MAX_FRAME_SIZE, "MaxFrameSize", "Max size of frame in stream - changes are signaled through PID info change (no reconfigure)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AVG_FRAME_SIZE, "AvgFrameSize", "Average size of frame in stream (ISOBMFF only, static property)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MAX_TS_DELTA, "MaxTSDelta", "Maximum DTS delta between frames (ISOBMFF only, static property)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MAX_CTS_OFFSET, "MaxCTSOffset", "Maximum absolute CTS offset (ISOBMFF only, static property)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CONSTANT_DURATION, "ConstantDuration", "Constant duration of samples, 0 means variable duration (ISOBMFF only, static property)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_TRACK_TEMPLATE, "TrackTemplate", "ISOBMFF serialized track box for this PID, without any sample info (empty stbl and empty dref)", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_TREX_TEMPLATE, "TrexTemplate", "ISOBMFF serialized trex box for this PID", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_STSD_TEMPLATE, "STSDTemplate", "ISOBMFF serialized sample description box (stsd entry) for this PID", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_ISOM_UDTA, "MovieUserData", "ISOBMFF serialized moov UDTA and other moov-level boxes (list) for this PID", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_HANDLER, "HandlerName", "ISOBMFF track handler name", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_TRACK_FLAGS, "TrackFlags", "ISOBMFF track header flags", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_TRACK_MATRIX, "TrackMatrix", "ISOBMFF track header matrix", GF_PROP_SINT_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_ALT_GROUP, "AltGroup", "ISOBMFF alt group ID", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ISOM_FORCE_NEGCTTS, "ForceNCTTS", "ISOBMFF force negative CTS offsets", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DISABLED, "Disable", "ISOBMFF disable flag", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_PERIOD_ID, "Period", "ID of DASH period", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_PERIOD_START, "PStart", "DASH Period start - cf dasher help", GF_PROP_FRACTION64, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_PERIOD_DUR, "PDur", "DASH Period duration - cf dasher help", GF_PROP_FRACTION64, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_REP_ID, "Representation", "ID of DASH representation", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AS_ID, "ASID", "ID of parent DASH AS", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MUX_SRC, "MuxSrc", "Name of mux source(s), set by dasher to direct its outputs", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_MODE, "DashMode", "DASH mode to be used by multiplexer if any, set by dasher. 0 is no DASH, 1 is regular DASH, 2 is VoD", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FORCE_SEG_SYNC, "SegSync", "Indicate segment must be completely flushed before sending segment/fragment size events", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_DUR, "DashDur", "DASH target segment duration in seconds", GF_PROP_FRACTION, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_MULTI_PID, NULL, "Pointer to the GF_List of input PIDs for multi-stsd entries segments, set by dasher", GF_PROP_POINTER, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_MULTI_PID_IDX, NULL, "1-based index of PID in the multi PID list, set by dasher", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_MULTI_TRACK, NULL, "Pointer to the GF_List of input PIDs for multi-tracks segments, set by dasher", GF_PROP_POINTER, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ROLE, "Role", "List of roles for this PID, where each role string can be a DASH role, a `URN:role-value` or any other string (this will throw a warning and use a custom URI for the role)", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_PERIOD_DESC, "PDesc", "List of descriptors for the DASH period containing this PID", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AS_COND_DESC, "ASDesc", "List of conditional descriptors for the DASH AdaptationSet containing this PID. If a PID with the same property type but different value is found, the PIDs will be in different AdaptationSets", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_AS_ANY_DESC, "ASCDesc", "List of common descriptors for the DASH AdaptationSet containing this PID", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_REP_DESC, "RDesc", "List of descriptors for the DASH Representation containing this PID", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_BASE_URL, "BUrl", "List of base URLs for this PID", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_TEMPLATE, "Template", "Template to use for DASH generation for this PID", GF_PROP_STRING),
	DEC_PROP( GF_PROP_PID_START_NUMBER, "StartNumber", "Start number to use for this PID - cf dasher help", GF_PROP_UINT),
	DEC_PROP_F( GF_PROP_PID_XLINK, "xlink", "Remote period URL for DASH - cf dasher help", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CLAMP_DUR, "ClampDur", "Max media duration to process from PID in DASH mode", GF_PROP_FRACTION64, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_HLS_PLAYLIST, "HLSPL", "Name of the HLS variant playlist for this media", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_HLS_GROUPID, "HLSGroup", "Name of HLS Group of a stream", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_HLS_EXT_MASTER, "HLSMExt", "List of extensions to add to the master playlist for this PID", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_HLS_EXT_VARIANT, "HLSVExt", "List of extensions to add to the variant playlist for this PID", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_CUE, "DCue", "Name of a cue list file for this PID - see dasher help", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_SEGMENTS, "DSegs", "Number of DASH segments defined by the DASH cue info", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CODEC, "Codec", "codec parameter string to force. If starting with '.', appended to ISOBMFF code point; otherwise replace the codec string", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_SINGLE_SCALE, "SingleScale", "Movie header should use the media timescale of the first track added", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_UDP, "RequireReorder", "PID packets come from source with losses and reordering happening (UDP)", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_PRIMARY_ITEM, "Primary", "Primary item in ISOBMFF", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_FWD, "DFMode", "DASH forward mode is used for this PID. If 2, the manifest is also carried in packet propery", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_DASH_MANIFEST, "DFManifest", "Value of manifest in forward mode", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_HLS_VARIANT, "DFVariant", "Value of variant playlist in forward mode", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_HLS_VARIANT_NAME, "DFVariantName", "Value of variant playlist name in forward mode", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_DASH_PERIOD_START, "DFPStart", "Value of active period start time in forward mode", GF_PROP_LUINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_PID_HLS_KMS, "HLSKey", "URI, KEYFORMAT and KEYFORMATVERSIONS for HLS full segment encryption creation, Key URI otherwise ( decoding and sample-AES)", GF_PROP_STRING),
	DEC_PROP( GF_PROP_PID_HLS_IV, "HLSIV", "Init Vector for HLS decode", GF_PROP_DATA),
	DEC_PROP( GF_PROP_PID_CLEARKEY_URI, "CKUrl", "URL for ClearKey licence server", GF_PROP_STRING),

	DEC_PROP_F( GF_PROP_PID_COLR_PRIMARIES, "ColorPrimaries", "Color primaries", GF_PROP_CICP_COL_PRIM, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_COLR_TRANSFER, "ColorTransfer", "Color transfer characteristics", GF_PROP_CICP_COL_TFC, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_COLR_MX, "ColorMatrix", "Color matrix coefficient", GF_PROP_CICP_COL_MX, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_COLR_RANGE, "FullRange", "Color full range flag", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_COLR_CHROMAFMT, "Chroma", "Chroma format (see ISO/IEC 23001-8 / 23091-2)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_COLR_CHROMALOC, "ChromaLoc", "Chroma location (see ISO/IEC 23001-8 / 23091-2)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CONTENT_LIGHT_LEVEL, "ContentLightLevel", "Content light level, payload of clli box (see ISO/IEC 14496-12), can be set as a list of 2 integers in fragment declaration (e.g. \"=max_cll,max_pic_avg_ll\")", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MASTER_DISPLAY_COLOUR, "MasterDisplayColour", "Master display colour info, payload of mdcv box (see ISO/IEC 14496-12), can be set as a list of 10 integers in fragment declaration (e.g. \"=dpx0,dpy0,dpx1,dpy1,dpx2,dpy2,wpx,wpy,max,min\")", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ICC_PROFILE, "ICC", "ICC profile (see ISO 15076-1 or ICC.1)", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_SRC_MAGIC, "SrcMagic", "Magic number to store in the track, only used by importers", GF_PROP_LUINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MUX_INDEX, "MuxIndex", "Target track index in destination file, stored by lowest value first (not set by demultiplexers)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP( GF_PROP_NO_TS_LOOP, "NoTSLoop", "Timestamps on this PID are adjusted in case of loops (used by TS multiplexer output)", GF_PROP_BOOL),
	DEC_PROP_F( GF_PROP_PID_MHA_COMPATIBLE_PROFILES, "MHAProfiles", "List of compatible profiles for this MPEG-H Audio object", GF_PROP_UINT_LIST, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PCK_FRAG_START, "FragStart", "Packet is a fragment start (value 1) or a segment start (value 2)", GF_PROP_UINT, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_FRAG_RANGE, "FragRange", "Start and end position in bytes of fragment if packet is a fragment or segment start", GF_PROP_FRACTION64, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_FRAG_TFDT, "FragTFDT", "Decode time of first packet in fragmentt", GF_PROP_LUINT, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_SIDX_RANGE, "SIDXRange", "Start and end position in bytes of sidx if packet is a fragment or segment start", GF_PROP_FRACTION64, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PCK_MOOF_TEMPLATE, "MoofTemplate", "Serialized moof box corresponding to the start of a movie fragment or segment (with styp and optionally sidx)", GF_PROP_DATA, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PCK_INIT, "InitSeg", "Set to true if packet is a complete DASH init segment file", GF_PROP_BOOL, GF_PROP_FLAG_PCK),

	DEC_PROP_F( GF_PROP_PID_RAWGRAB, "RawGrab", "PID is a raw media grabber (webcam, microphone, etc...). Value 2 is used for front camera", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_KEEP_AFTER_EOS, "KeepAfterEOS", "PID must be kept alive after EOS (LASeR and BIFS)", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_COVER_ART, "CoverArt", "PID cover art image data. If associated data is NULL, the data is carried in the PID", GF_PROP_DATA, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_PLAY_BUFFER, "BufferLength", "Playout buffer in ms", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_MAX_BUFFER, "MaxBuffer", "Maximum buffer occupancy in ms", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_RE_BUFFER, "ReBuffer", "Rebuffer threshold in ms, 0 disable rebuffering", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_VIEW_IDX, "ViewIdx", "View index for multiview (1 being left)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ORIG_FRAG_URL, "FragURL", "Fragment URL (without '#') of original URL (used by some filters to set the property on media PIDs)", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_ROUTE_IP, "ROUTEIP", "ROUTE session IP address", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ROUTE_PORT, "ROUTEPort", "ROUTE session port number", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ROUTE_NAME, "ROUTEName", "Name (location) of raw file to advertise in ROUTE session", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ROUTE_CAROUSEL, "ROUTECarousel", "Carousel period in seconds of raw file in ROUTE session", GF_PROP_FRACTION, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_ROUTE_SENDTIME, "ROUTEUpload", "Upload time in seconds of raw file in ROUTE session", GF_PROP_FRACTION, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_STEREO_TYPE, "Stereo", "Stereo type of video", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_PROJECTION_TYPE, "Projection", "Projection type of video", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_VR_POSE, "InitalPose", "Initial pose for 360 video, in degrees expressed as 16.16 bits (x is yaw, y is pitch, z is roll)", GF_PROP_VEC3I, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_CUBE_MAP_PAD, "CMPad", "Number of pixels to pad from edge of each face in cube map", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_EQR_CLAMP, "EQRClamp", "Clamping of frame for EQR as 0.32 fixed point (x is top, y is bottom, z is left and w is right)", GF_PROP_VEC4I, GF_PROP_FLAG_GSF_REM),

	DEC_PROP( GF_PROP_PID_SCENE_NODE, "SceneNode", "PID is a scene node decoder (AFX BitWrapper in BIFS)", GF_PROP_BOOL),
	DEC_PROP( GF_PROP_PID_ORIG_CRYPT_SCHEME, "OrigCryptoScheme", "Original crypto scheme on a decrypted PID", GF_PROP_4CC),
	DEC_PROP_F( GF_PROP_PID_TIMESHIFT_SEGS, "TSBSegs", "Time shift in number of segments for HAS streams, only set by dashin and dasher filters", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_IS_MANIFEST, "IsManifest", "PID is a HAS manifest\n"
	"- 0: not a manifest\n"
	"- 1: DASH manifest\n"
	"- 2: HLS manifest\n"
	"- 3: GHI(X) manifest", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_SPARSE, "Sparse", "PID has potentially empty times between packets", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CHARSET, "CharSet", "Character set for input text PID", GF_PROP_STRING, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_FORCED_SUB, "ForcedSub", "PID or Packet is forced sub\n"
	"0: not forced\n"
	"1: forced frame\n"
	"2: all frames are forced (PID only)", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PID_CHAP_TIMES, "ChapTimes", "Chapter start times", GF_PROP_UINT_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_CHAP_NAMES, "ChapNames", "Chapter names", GF_PROP_STRING_LIST, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_IS_CHAP, "IsChap", "Subtitle PID is chapter (for QT-like chapters)", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),

	DEC_PROP_F( GF_PROP_PCK_SKIP_BEGIN, "SkipBegin", "Amount of media to skip from beginning of packet in PID timescale", GF_PROP_UINT, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PCK_SKIP_PRES, "SkipPres", "Packet and any following with CTS greater than this packet shall not be presented (used by reframer to create edit lists)", GF_PROP_BOOL, GF_PROP_FLAG_PCK),

	DEC_PROP_F( GF_PROP_PCK_HLS_REF, "HLSRef", "HLS playlist reference, gives a unique ID identifying media mux, and indicated in packets carrying child playlists", GF_PROP_LUINT, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_LLHLS, "LLHLS", "HLS low latency mode", GF_PROP_UINT, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_HLS_FRAG_NUM, "LLHLSFragNum", "LLHLS fragment number", GF_PROP_UINT, GF_PROP_FLAG_PCK),
	DEC_PROP_F( GF_PROP_PID_DOWNLOAD_SESSION, "DownloadSession", "Pointer to download session", GF_PROP_POINTER, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PID_HAS_TEMI, "HasTemi", "TEMI present flag", GF_PROP_BOOL, GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_XPS_MASK, "XPSMask", "Parameter set mask", GF_PROP_UINT, GF_PROP_FLAG_PCK|GF_PROP_FLAG_GSF_REM),
	DEC_PROP_F( GF_PROP_PCK_END_RANGE, "RangeEnd", "Signal packet is the last in the desired play range", GF_PROP_BOOL, GF_PROP_FLAG_PCK),
};

static u32 gf_num_props = sizeof(GF_BuiltInProps) / sizeof(GF_BuiltInProperty);

GF_EXPORT
u32 gf_props_get_id(const char *name)
{
	u32 i, len, prop_id=0;
	if (!name) return 0;
	len = (u32) strlen(name);
	if (len==4) {
		prop_id = GF_4CC(name[0], name[1], name[2], name[3]);
	} else if ((len==3) && !strcmp(name, "PID")) {
		return GF_PROP_PID_ID;
	}

	for (i=0; i<gf_num_props; i++) {
		GF_BuiltInProperty *prop = &GF_BuiltInProps[i];
		if (prop_id && (prop->type==prop_id))
			return prop->type;

		if (prop->name) {
			u32 j;
			for (j=0; j<=len; j++) {
				char c = prop->name[j];
				if (!c) break;
				if (c != name[j])
					break;
			}
			if ((j==len) && !prop->name[j])
				return prop->type;
		}
	}
	return 0;
}

GF_EXPORT
const GF_BuiltInProperty *gf_props_get_description(u32 prop_idx)
{
	if (prop_idx>=gf_num_props) return NULL;
	return &GF_BuiltInProps[prop_idx];
}

GF_EXPORT
const char *gf_props_4cc_get_name(u32 prop_4cc)
{
	u32 i;
	for (i=0; i<gf_num_props; i++) {
		if (GF_BuiltInProps[i].type==prop_4cc) return GF_BuiltInProps[i].name;
	}
	return NULL;
}

GF_EXPORT
u8 gf_props_4cc_get_flags(u32 prop_4cc)
{
	u32 i;
	for (i=0; i<gf_num_props; i++) {
		if (GF_BuiltInProps[i].type==prop_4cc) return GF_BuiltInProps[i].flags;
	}
	return 0;
}

GF_EXPORT
u32 gf_props_4cc_get_type(u32 prop_4cc)
{
	u32 i;
	for (i=0; i<gf_num_props; i++) {
		if (GF_BuiltInProps[i].type==prop_4cc) return GF_BuiltInProps[i].data_type;
	}
	return GF_PROP_FORBIDEN;
}

Bool gf_props_4cc_check_props()
{
	Bool res = GF_TRUE;
	u32 i, j;
	for (i=0; i<gf_num_props; i++) {
		for (j=i+1; j<gf_num_props; j++) {
			if (GF_BuiltInProps[i].type==GF_BuiltInProps[j].type) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Property %s and %s have the same code type %s\n", GF_BuiltInProps[i].name, GF_BuiltInProps[j].name, gf_4cc_to_str(GF_BuiltInProps[i].type) ));
				res = GF_FALSE;
			}
		}
	}
	return res;
}

GF_EXPORT
const char *gf_props_dump_val(const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], GF_PropDumpDataMode dump_data_flags, const char *min_max_enum)
{
	switch (att->type) {
	case GF_PROP_NAME:
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
		break;
	default:
		if (!dump) return NULL;
		break;
	}
	Bool no_reduce = dump_data_flags & GF_PROP_DUMP_NO_REDUCE;
	u32 dump_data_type = dump_data_flags&0xFF;

	dump[0] = 0;
	switch (att->type) {
	case GF_PROP_SINT:
		sprintf(dump, "%d", att->value.sint);
		break;
	case GF_PROP_UINT:
		if (min_max_enum && strchr(min_max_enum, '|') ) {
			u32 enum_val = 0;
			char *str_start = (char *) min_max_enum;
			while (str_start) {
				u32 len;
				char *sep = strchr(str_start, '|');
				if (sep) {
					len = (u32) (sep - str_start);
				} else {
					len = (u32) strlen(str_start);
				}
				if (att->value.uint == enum_val) {
					strncpy(dump, str_start, len);
					dump[len]=0;
					break;
				}
				if (!sep) break;
				str_start = sep+1;
				enum_val++;
			}
			if (!str_start) {
				sprintf(dump, "%u", att->value.uint);
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %d not found in enums %s - using integer dump\n", att->value.uint_list, min_max_enum));
			}
		} else {
			sprintf(dump, "%u", att->value.uint);
		}
		break;
	case GF_PROP_4CC:
		sprintf(dump, "%s", gf_4cc_to_str(att->value.uint) );
		break;
	case GF_PROP_LSINT:
		sprintf(dump, ""LLD, att->value.longsint);
		break;
	case GF_PROP_LUINT:
		sprintf(dump, ""LLU, att->value.longuint);
		break;
	case GF_PROP_FRACTION:
		//reduce fraction
		if (!no_reduce && att->value.frac.den && ((att->value.frac.num/att->value.frac.den) * att->value.frac.den == att->value.frac.num)) {
			sprintf(dump, "%d", att->value.frac.num / att->value.frac.den);
		} else {
			sprintf(dump, "%d/%u", att->value.frac.num, att->value.frac.den);
		}
		break;
	case GF_PROP_FRACTION64:
		//reduce fraction
		if (!no_reduce && att->value.lfrac.den && ((att->value.lfrac.num/att->value.lfrac.den) * att->value.lfrac.den == att->value.lfrac.num)) {
			sprintf(dump, LLD, att->value.lfrac.num / att->value.lfrac.den);
		} else {
			sprintf(dump, LLD"/"LLU, att->value.lfrac.num, att->value.lfrac.den);
		}
		break;
	case GF_PROP_BOOL:
		sprintf(dump, "%s", att->value.boolean ? "true" : "false");
		break;
	case GF_PROP_FLOAT:
		sprintf(dump, "%f", FIX2FLT(att->value.fnumber) );
		break;
	case GF_PROP_DOUBLE:
		sprintf(dump, "%g", att->value.number);
		break;
	case GF_PROP_VEC2I:
		sprintf(dump, "%dx%d", att->value.vec2i.x, att->value.vec2i.y);
		break;
	case GF_PROP_VEC2:
		sprintf(dump, "%lgx%lg", att->value.vec2.x, att->value.vec2.y);
		break;
	case GF_PROP_VEC3I:
		sprintf(dump, "%dx%dx%d", att->value.vec3i.x, att->value.vec3i.y, att->value.vec3i.z);
		break;
	case GF_PROP_VEC4I:
		sprintf(dump, "%dx%dx%dx%d", att->value.vec4i.x, att->value.vec4i.y, att->value.vec4i.z, att->value.vec4i.w);
		break;
	case GF_PROP_NAME:
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
		return att->value.string;

	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
	case GF_PROP_DATA_NO_COPY:
		if (dump_data_type==2) {
			sprintf(dump, "%d@%p", att->value.data.size, att->value.data.ptr);
		} else if (dump_data_type && att->value.data.size < 40) {
			u32 i;
			sprintf(dump, "%d bytes 0x", att->value.data.size);
			for (i=0; i<att->value.data.size; i++) {
				sprintf(dump, "%02X", (unsigned char) att->value.data.ptr[i]);
			}
		} else {
			sprintf(dump, "%d bytes (CRC32 0x%08X)", att->value.data.size, gf_crc_32(att->value.data.ptr, att->value.data.size));
		}
		break;
	case GF_PROP_POINTER:
		sprintf(dump, "%p", att->value.ptr);
		break;
	case GF_PROP_STRING_LIST:
	{
		u32 i, count = att->value.string_list.nb_items;
		u32 len = GF_PROP_DUMP_ARG_SIZE-1;
		dump[len]=0;
		for (i=0; i<count; i++) {
			char *s = att->value.string_list.vals[i];
			if (!i) {
				strncpy(dump, s, len);
			} else {
				strcat(dump, ",");
				strncat(dump, s, len-1);
			}
			len = GF_PROP_DUMP_ARG_SIZE - 1 - (u32) strlen(dump);
			if (len<=1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("String list is too large to fit in predefined property dump buffer of %d bytes, truncating\n", GF_PROP_DUMP_ARG_SIZE));
				return dump;
			}
		}
		return dump;
	}
	case GF_PROP_UINT_LIST:
	case GF_PROP_4CC_LIST:
	case GF_PROP_SINT_LIST:
	case GF_PROP_VEC2I_LIST:
	{
		u32 i, count = att->value.uint_list.nb_items;
		u32 len = GF_PROP_DUMP_ARG_SIZE-1;
		dump[len]=0;
		for (i=0; i<count; i++) {
			char szItem[1024];
			if (att->type==GF_PROP_UINT_LIST) {
				sprintf(szItem, "%u", att->value.uint_list.vals[i]);
			} else if (att->type==GF_PROP_4CC_LIST) {
				sprintf(szItem, "%s", gf_4cc_to_str(att->value.uint_list.vals[i]) );
			} else if (att->type==GF_PROP_SINT_LIST) {
				sprintf(szItem, "%d", att->value.sint_list.vals[i]);
			} else {
				sprintf(szItem, "%dx%d", att->value.v2i_list.vals[i].x, att->value.v2i_list.vals[i].x);
			}
			if (!i) {
				strncpy(dump, szItem, len);
			} else {
				strcat(dump, ",");
				strncat(dump, szItem, len-1);
			}
			len = GF_PROP_DUMP_ARG_SIZE - 1 - (u32) strlen(dump);
			if (len<=1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("List is too large to fit in predefined property dump buffer of %d bytes, truncating\n", GF_PROP_DUMP_ARG_SIZE));
				return dump;
			}
		}
		return dump;
	}
	case GF_PROP_FORBIDEN:
		sprintf(dump, "forbiden");
		break;
	case GF_PROP_LAST_DEFINED:
		sprintf(dump, "lastDefined");
		break;
	default:
		if (gf_props_type_is_enum(att->type)) {
			return gf_props_enum_name(att->type, att->value.uint);
		}
	}
	return dump;
}


GF_EXPORT
const char *gf_props_dump(u32 p4cc, const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], GF_PropDumpDataMode dump_data_mode)
{
	switch (p4cc) {
	case GF_PROP_PID_STREAM_TYPE:
	case GF_PROP_PID_ORIG_STREAM_TYPE:
		return gf_stream_type_name(att->value.uint);
	case GF_PROP_PID_CODECID:
		return gf_codecid_name(att->value.uint);

	case GF_PROP_PID_PLAYBACK_MODE:
		if (att->value.uint == GF_PLAYBACK_MODE_SEEK) return "seek";
		else if (att->value.uint == GF_PLAYBACK_MODE_REWIND) return "rewind";
		else if (att->value.uint == GF_PLAYBACK_MODE_FASTFORWARD) return "forward";
		else return "none";


	case GF_PROP_PCK_SENDER_NTP:
	case GF_PROP_PCK_RECEIVER_NTP:
	case GF_PROP_PCK_UTC_TIME:
	{
		u64 time = (p4cc==GF_PROP_PCK_UTC_TIME) ? att->value.longuint : gf_net_ntp_to_utc(att->value.longuint);
		time_t gtime;
		struct tm *t;
		u32 sec;
		u32 ms;
		gtime = time / 1000;
		sec = (u32)(time / 1000);
		ms = (u32)(time - ((u64)sec) * 1000);

		t = gf_gmtime(&gtime);
		sec = t->tm_sec;
		//see issue #859, no clue how this happened...
		if (sec > 60)
			sec = 60;
		snprintf(dump, GF_PROP_DUMP_ARG_SIZE-1, "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, sec, ms);
		dump[GF_PROP_DUMP_ARG_SIZE-1]=0;
	}
		return dump;

	default:
		if (att->type==GF_PROP_UINT) {
			u32 type = gf_props_4cc_get_type(p4cc);
			if (gf_props_type_is_enum(type))
				return gf_props_enum_name(type, att->value.uint);
		}
		return gf_props_dump_val(att, dump, dump_data_mode, NULL);
	}
	return "";
}


GF_Err gf_prop_matrix_decompose(const GF_PropertyValue *p, u32 *flip_mode, u32 *rot_mode)
{
	GF_Point2D scale, translate;
	Fixed rotate;
	GF_Matrix2D mx;
	if (!p || (p->type!=GF_PROP_SINT_LIST) || (p->value.sint_list.nb_items!=9))
		return GF_BAD_PARAM;

	mx.m[0] = INT2FIX(p->value.sint_list.vals[0])/65536;
	mx.m[1] = INT2FIX(p->value.sint_list.vals[1])/65536;
	mx.m[2] = INT2FIX(p->value.sint_list.vals[2])/65536;
	mx.m[3] = INT2FIX(p->value.sint_list.vals[3])/65536;
	mx.m[4] = INT2FIX(p->value.sint_list.vals[4])/65536;
	mx.m[5] = INT2FIX(p->value.sint_list.vals[5])/65536;
	gf_mx2d_decompose(&mx, &scale, &rotate, &translate);

	if (flip_mode) {
		*flip_mode = 0;
		if (ABSDIFF(scale.x, -1) < 0.05) {
			*flip_mode = (ABSDIFF(scale.y, -1) < 0.05) ? 3 : 2;
		}
		else if (ABSDIFF(scale.x, -1) < 0.05) {
			*flip_mode = 1;
		}
	}
	if (rot_mode) {
		*rot_mode = 0;
		if (ABSDIFF(rotate, GF_PI2)<0.05) {
			*rot_mode = 1;
		}
		else if (ABSDIFF(rotate, GF_PI/2)<0.05) {
			*rot_mode = 2;
		}
		if (ABSDIFF(-rotate, GF_PI2)<0.05) {
			*rot_mode = 3;
		}
	}
	return GF_OK;
}

