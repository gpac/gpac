/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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

GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values, char list_sep_char)
{
	GF_PropertyValue p;
	char *unit_sep=NULL;
	char unit_char = 0;
	s32 unit = 0;
	memset(&p, 0, sizeof(GF_PropertyValue));
	p.value.data.size=0;
	p.type=type;
	if (!name) name="";

	unit_sep = NULL;
	if (value) {
		u32 len = (u32) strlen(value);
		unit_sep = strrchr("kKgGmM", value[len-1]);
		if (unit_sep) {
			unit_char = unit_sep[0];
			if ((unit_char=='k') || (unit_char=='K')) unit = 1000;
			else if ((unit_char=='m') || (unit_char=='M')) unit = 1000000;
			if ((unit_char=='G') || (unit_char=='g')) unit = 1000000;
		}
	}

	switch (p.type) {
	case GF_PROP_BOOL:
		if (!value || !strcmp(value, "yes") || !strcmp(value, "true") || !strcmp(value, "1")) {
			p.value.boolean = GF_TRUE;
		} else if ( !strcmp(value, "no") || !strcmp(value, "false") ) {
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
		if (value && !strcmp(value, "+I")) p.value.sint = 0xFFFFFFFF;
		else if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else if (enum_values && strchr(enum_values, '|')) {
			u32 a_len = strlen(value);
			u32 val = 0;
			char *str_start = (char *) enum_values;
			while (str_start) {
				u32 len;
				char *sep = strchr(str_start, '|');
				if (sep) {
					len = sep - str_start;
				} else {
					len = strlen(str_start);
				}
				if ((a_len == len) && !strncmp(str_start, value, len))
					break;
				if (!sep) break;
				str_start = sep+1;
				val++;
			}
			if (!str_start) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s enum %s - using 0\n", value, name, enum_values));
				p.value.uint = 0;
			} else {
				p.value.uint = val;
			}
		} else if (!strnicmp(value, "0x", 2)) {
			if (sscanf(value, "0x%x", &p.value.uint)!=1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			} else if (unit) {
				p.value.uint *= unit;
			}
		} else if (sscanf(value, "%d", &p.value.uint)!=1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
		} else if (unit) {
			p.value.uint *= unit;
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
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
			p.value.frac.num = 0;
			p.value.frac.den = 1;
		} else {
			if (sscanf(value, "%d/%u", &p.value.frac.num, &p.value.frac.den) != 2) {
				if (sscanf(value, "%d-%u", &p.value.frac.num, &p.value.frac.den) != 2) {
					p.value.frac.den=1;
					if (sscanf(value, "%d", &p.value.frac.num) != 1) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
						p.value.frac.num = 0;
						p.value.frac.den = 1;
					}
				}
			}
		}
		break;
	case GF_PROP_FRACTION64:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
			p.value.frac.num = 0;
			p.value.frac.den = 1;
		}
		else {
			if (sscanf(value, LLD"/"LLU, &p.value.lfrac.num, &p.value.lfrac.den) != 2) {
				if (sscanf(value, LLD"-"LLU, &p.value.lfrac.num, &p.value.lfrac.den) != 2) {
					p.value.lfrac.den = 1;
					if (sscanf(value, LLD, &p.value.lfrac.num) != 1) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
						p.value.lfrac.num = 0;
						p.value.lfrac.den = 1;
					}
				}
			}
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
		else if (!value || (sscanf(value, "%lg", &p.value.number) != 1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for double arg %s - using 0\n", value, name));
			p.value.number = 0;
		} else if (unit) {
			p.value.number *= unit;
		}
		break;
	case GF_PROP_VEC2I:
		if (!value || (sscanf(value, "%dx%d", &p.value.vec2i.x, &p.value.vec2i.y) != 2)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2i arg %s - using {0,0}\n", value, name));
			p.value.vec2i.x = p.value.vec2i.y = 0;
		}
		break;
	case GF_PROP_VEC2:
		if (!value || (sscanf(value, "%lgx%lg", &p.value.vec2.x, &p.value.vec2.y) != 2)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2 arg %s - using {0,0}\n", value, name));
			p.value.vec2.x = p.value.vec2.y = 0;
		}
		break;
	case GF_PROP_VEC3I:
		if (!value || (sscanf(value, "%dx%dx%d", &p.value.vec3i.x, &p.value.vec3i.y, &p.value.vec3i.z) != 3)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec3i arg %s - using {0,0,0}\n", value, name));
			p.value.vec3i.x = p.value.vec3i.y = p.value.vec3i.z = 0;
		}
		break;
	case GF_PROP_VEC3:
		if (!value || (sscanf(value, "%lgx%lgx%lg", &p.value.vec3.x, &p.value.vec3.y, &p.value.vec3.z) != 3)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec3 arg %s - using {0,0,0}\n", value, name));
			p.value.vec3.x = p.value.vec3.y = p.value.vec3.z = 0;
		}
		break;
	case GF_PROP_VEC4I:
		if (!value || (sscanf(value, "%dx%dx%dx%d", &p.value.vec4i.x, &p.value.vec4i.y, &p.value.vec4i.z, &p.value.vec4i.w) != 4)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec4i arg %s - using {0,0,0}\n", value, name));
			p.value.vec4i.x = p.value.vec4i.y = p.value.vec4i.z = p.value.vec4i.w = 0;
		}
		break;
	case GF_PROP_VEC4:
		if (!value || (sscanf(value, "%lgx%lgx%lgx%lg", &p.value.vec4.x, &p.value.vec4.y, &p.value.vec4.z, &p.value.vec4.w) != 4)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec4 arg %s - using {0,0,0}\n", value, name));
			p.value.vec4.x = p.value.vec4.y = p.value.vec4.z = p.value.vec4.w = 0;
		}
		break;
	case GF_PROP_PIXFMT:
		p.value.uint = gf_pixel_fmt_parse(value);
		break;
	case GF_PROP_PCMFMT:
		p.value.uint = gf_audio_fmt_parse(value);
		break;
	case GF_PROP_NAME:
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
		p.type=GF_PROP_STRING;
		p.value.string = value ? gf_strdup(value) : NULL;
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
	case GF_PROP_DATA_NO_COPY:
		if (!value || (sscanf(value, "%p:%d", &p.value.data.ptr, &p.value.data.size)!=2) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for data arg %s - using 0\n", value, name));
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
		p.value.string_list = gf_list_new();
		char *v = (char *) value;
		if (v[0]=='<') is_xml = GF_TRUE;
		if (!list_sep_char) list_sep_char = ',';
		while (v) {
			u32 len=0;
			char *nv;
			char *sep = strchr(v, list_sep_char);
			if (sep) {
				char *xml_end = strchr(v, '>');
				len = (u32) (sep - v);
				if (xml_end) {
					u32 xml_len = (u32) (xml_end - v);
					if (xml_len > len) {
						sep = strchr(xml_end, list_sep_char);
						if (sep)
							len = (u32) (sep - v);
					}
				}
			}
			if (!sep)
			 	len = (u32) strlen(v);

			nv = gf_malloc(sizeof(char)*(len+1));
			strncpy(nv, v, sizeof(char)*len);
			nv[len] = 0;
			gf_list_add(p.value.string_list, nv);
			if (!sep) break;
			v = sep+1;
		}
	}
		break;
	case GF_PROP_UINT_LIST:
	{
		char *v = (char *) value;
		if (!list_sep_char) list_sep_char = ',';
		while (v) {
			char szV[100];
			u32 val_uint, len=0;
			char *sep = strchr(v, list_sep_char);
			if (sep) {
				len = (u32) (sep - v);
			}
			if (!sep)
			 	len = (u32) strlen(v);

			strncpy(szV, v, sizeof(char)*100);
			sscanf(szV, "%u", &val_uint);
			p.value.uint_list.vals = gf_realloc(p.value.uint_list.vals, (p.value.uint_list.nb_items+1) * sizeof(u32));
			p.value.uint_list.vals[p.value.uint_list.nb_items] = val_uint;
			p.value.uint_list.nb_items++;
			if (!sep) break;
			v = sep+1;
		}
	}
		break;
	case GF_PROP_FORBIDEN:
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Forbidden property type %d for arg %s - ignoring\n", type, name));
		p.type=GF_PROP_FORBIDEN;
		break;
	}
//	if (unit_sep) unit_sep[0] = unit_char;

	return p;
}

Bool gf_props_equal(const GF_PropertyValue *p1, const GF_PropertyValue *p2)
{
	if (p1->type!=p2->type) {
		if ((p1->type == GF_PROP_STRING) && (p2->type == GF_PROP_NAME) ) {
		} else if ((p2->type == GF_PROP_STRING) && (p1->type == GF_PROP_NAME) ) {
		} else {
			return GF_FALSE;
		}
	}

	switch (p1->type) {
	case GF_PROP_SINT: return (p1->value.sint==p2->value.sint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_PIXFMT:
	case GF_PROP_PCMFMT:
	case GF_PROP_UINT:
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
	case GF_PROP_VEC3:
		return ((p1->value.vec3.x==p2->value.vec3.x) && (p1->value.vec3.y==p2->value.vec3.y) && (p1->value.vec3.z==p2->value.vec3.z)) ? GF_TRUE : GF_FALSE;
	case GF_PROP_VEC4I:
		return ((p1->value.vec4i.x==p2->value.vec4i.x) && (p1->value.vec4i.y==p2->value.vec4i.y) && (p1->value.vec4i.z==p2->value.vec4i.z) && (p1->value.vec4i.w==p2->value.vec4i.w)) ? GF_TRUE : GF_FALSE;
	case GF_PROP_VEC4:
		return ((p1->value.vec4.x==p2->value.vec4.x) && (p1->value.vec4.y==p2->value.vec4.y) && (p1->value.vec4.z==p2->value.vec4.z) && (p1->value.vec4.w==p2->value.vec4.w)) ? GF_TRUE : GF_FALSE;


	case GF_PROP_STRING:
	case GF_PROP_NAME:
		if (!p1->value.string) return p2->value.string ? GF_FALSE : GF_TRUE;
		if (!p2->value.string) return GF_FALSE;
		if (!strcmp(p1->value.string, "*")) return GF_TRUE;
		if (!strcmp(p2->value.string, "*")) return GF_TRUE;
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
		return !strcmp(p1->value.string, p2->value.string) ? GF_TRUE : GF_FALSE;

	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		if (!p1->value.data.ptr) return p2->value.data.ptr ? GF_FALSE : GF_TRUE;
		if (!p2->value.data.ptr) return GF_FALSE;
		if (p1->value.data.size != p2->value.data.size) return GF_FALSE;
		return !memcmp(p1->value.data.ptr, p2->value.data.ptr, p1->value.data.size) ? GF_TRUE : GF_FALSE;

	case GF_PROP_STRING_LIST:
	{
		u32 c1, c2, i, j;
		c1 = gf_list_count(p1->value.string_list);
		c2 = gf_list_count(p2->value.string_list);
		if (c1 != c2) return GF_FALSE;
		for (i=0; i<c1; i++) {
			u32 found = 0;
			char *s1 = gf_list_get(p1->value.string_list, i);
			for (j=0; j<c2; j++) {
				char *s2 = gf_list_get(p2->value.string_list, j);
				if (s1 && s2 && !strcmp(s1, s2)) found++;
			}
			if (found!=1) return GF_FALSE;
		}
		return GF_TRUE;
	}
	case GF_PROP_UINT_LIST:
	{
		u32 i;
		if (p1->value.uint_list.nb_items != p2->value.uint_list.nb_items) return GF_FALSE;
		for (i=0; i<p1->value.uint_list.nb_items; i++) {
			if (p1->value.uint_list.vals[i] != p2->value.uint_list.vals[i]) return GF_FALSE;
		}
		return GF_TRUE;
	}

	//user-managed pointer
	case GF_PROP_POINTER: return (p1->value.ptr==p2->value.ptr) ? GF_TRUE : GF_FALSE;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Comparing forbidden property type %d\n", p1->type));
		break;
	}
	return GF_FALSE;
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
	else if (p->type==GF_PROP_UINT_LIST) {
		gf_free(p->value.uint_list.vals);
		p->value.uint_list.vals = NULL;
		p->value.uint_list.nb_items = 0;
	}
}
void gf_props_del_property(GF_PropertyMap *prop, GF_PropertyEntry *it)
{
	assert(it->reference_count);
	if (safe_int_dec(&it->reference_count) == 0 ) {
		if (it->pname && it->name_alloc) gf_free(it->pname);

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
			GF_List *l = it->prop.value.string_list;
			it->prop.value.string_list = NULL;
			while (gf_list_count(l)) {
				char *s = gf_list_pop_back(l);
				gf_free(s);
			}
			gf_list_del(l);
		}
		//string list are destroyed
		else if (it->prop.type==GF_PROP_UINT_LIST) {
			if (it->prop.value.uint_list.vals)
				gf_free(it->prop.value.uint_list.vals);
		}
		it->prop.value.data.size = 0;
		if (it->alloc_size) {
			assert(it->prop.type==GF_PROP_DATA);
			gf_fq_add(it->session->prop_maps_entry_data_alloc_reservoir, it);
		} else {
			gf_fq_add(it->session->prop_maps_entry_reservoir, it);
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
				gf_props_del_property(prop, (GF_PropertyEntry *) gf_list_pop_back(l) );
			}
			prop->hash_table[i] = NULL;
			gf_fq_add(prop->session->prop_maps_list_reservoir, l);
		}
	}
#else
	while (gf_list_count(prop->properties)) {
		gf_props_del_property(prop, (GF_PropertyEntry *) gf_list_pop_back(prop->properties) );
	}
#endif
}

void gf_props_del(GF_PropertyMap *prop)
{
	assert(prop->reference_count == 0);
	gf_props_reset(prop);
	prop->reference_count = 0;
	gf_fq_add(prop->session->prop_maps_reservoir, prop);
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
				gf_props_del_property(map, prop);
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
			gf_props_del_property(map, prop);
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

GF_Err gf_props_insert_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyEntry *prop;
	char *src_ptr;
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

	//remember source pointer
	src_ptr = prop->prop.value.data.ptr;
	//copy prop value
	memcpy(&prop->prop, value, sizeof(GF_PropertyValue));

	if (prop->prop.type == GF_PROP_STRING) {
		prop->prop.value.string = value->value.string ? gf_strdup(value->value.string) : NULL;
	} else if (prop->prop.type == GF_PROP_STRING_NO_COPY) {
		prop->prop.value.string = value->value.string;
		prop->prop.type = GF_PROP_STRING;
	} else if (prop->prop.type == GF_PROP_DATA) {
		//restore source pointer, realloc if needed
		prop->prop.value.data.ptr = src_ptr;
		if (prop->alloc_size < value->value.data.size) {
			prop->alloc_size = value->value.data.size;
			prop->prop.value.data.ptr = gf_realloc(prop->prop.value.data.ptr, sizeof(char) * value->value.data.size);
		}
		memcpy(prop->prop.value.data.ptr, value->value.data.ptr, value->value.data.size);
	} else if (prop->prop.type == GF_PROP_DATA_NO_COPY) {
		prop->prop.type = GF_PROP_DATA;
		prop->alloc_size = value->value.data.size;
	}
	else if (prop->prop.type == GF_PROP_UINT_LIST) {
		prop->prop.value.uint_list.vals = gf_malloc(sizeof(u32) * value->value.uint_list.nb_items);
		memcpy(prop->prop.value.uint_list.vals, value->value.uint_list.vals, sizeof(u32) * value->value.uint_list.nb_items);
		prop->prop.value.uint_list.nb_items = value->value.uint_list.nb_items;
	}

#if GF_PROPS_HASHTABLE_SIZE
	return gf_list_add(map->hash_table[hash], prop);
#else
	return gf_list_add(map->properties, prop);
#endif

}

GF_Err gf_props_set_property(GF_PropertyMap *map, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	u32 hash = gf_props_hash_djb2(p4cc, name ? name : dyn_name);
	gf_props_remove_property(map, hash, p4cc, name ? name : dyn_name);
	if (!value) return GF_OK;
	return gf_props_insert_property(map, hash, p4cc, name, dyn_name, value);
}

const GF_PropertyValue *gf_props_get_property(GF_PropertyMap *map, u32 prop_4cc, const char *name)
{
	u32 i, count, hash;
	const GF_PropertyValue *res=NULL;
	hash = gf_props_hash_djb2(prop_4cc, name);
#if GF_PROPS_HASHTABLE_SIZE
	if (map->hash_table[hash] ) {
		count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *p = gf_list_get(map->hash_table[hash], i);

			if ((prop_4cc && (p->p4cc==prop_4cc)) || (p->pname && name && !strcmp(p->pname, name)) ) {
				res = &p->prop;
				break;
			}
		}
	}
#else
	count = gf_list_count(map->properties);
	for (i=0; i<count; i++) {
		GF_PropertyEntry *p = gf_list_get(map->properties, i);

		if ((prop_4cc && (p->p4cc==prop_4cc)) || (p->pname && name && !strcmp(p->pname, name)) ) {
			res = &p->prop;
			break;
		}
	}
#endif
	return res;
}

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props, gf_filter_prop_filter filter_prop, void *cbk)
{
	GF_Err e;
	u32 i, count;
#if GF_PROPS_HASHTABLE_SIZE
	u32 idx;
#endif
	GF_List *list;
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
	u32 i, count;
#endif
	u32 idx;
	
	const GF_PropertyEntry *pe;
	if (!io_idx) return NULL;

	idx = *io_idx;
	if (idx == 0xFFFFFFFF) return NULL;

#if GF_PROPS_HASHTABLE_SIZE
	for (i=0; i<GF_PROPS_HASHTABLE_SIZE; i++) {
		if (props->hash_table[i]) {
			count = gf_list_count(props->hash_table[i]);
			if (idx >= count) {
				idx -= count;
				continue;
			}
			pe = gf_list_get(props->hash_table[i], idx);
			if (!pe) {
				*io_idx = 0xFFFFFFFF;
				return NULL;
			}
			if (prop_4cc) *prop_4cc = pe->p4cc;
			if (prop_name) *prop_name = pe->pname;
			*io_idx = (*io_idx) + 1;
			return &pe->prop;
		}
	}
	*io_idx = 0xFFFFFFFF;
	return NULL;
#else
	if (idx >= gf_list_count(props->properties)) {
		*io_idx = 0xFFFFFFFF;
		return NULL;
	}
	pe = gf_list_get(props->properties, idx);
	if (!pe) {
		*io_idx = 0xFFFFFFFF;
		return NULL;
	}
	if (prop_4cc) *prop_4cc = pe->p4cc;
	if (prop_name) *prop_name = pe->pname;
	*io_idx = (*io_idx) + 1;
	return &pe->prop;
#endif
}

GF_EXPORT
const char *gf_props_get_type_name(u32 type)
{
	switch (type) {
	case GF_PROP_SINT: return "int";
	case GF_PROP_UINT: return "unsigned int";
	case GF_PROP_LSINT: return "long int";
	case GF_PROP_LUINT: return "unsigned long int";
	case GF_PROP_FRACTION: return "fraction";
	case GF_PROP_FRACTION64: return "64-bit fraction";
	case GF_PROP_BOOL: return "boolean";
	case GF_PROP_FLOAT: return "float";
	case GF_PROP_DOUBLE: return "number";
	case GF_PROP_NAME: return "string";
	case GF_PROP_STRING: return "string";
	case GF_PROP_DATA: return "data";
	case GF_PROP_CONST_DATA: return "const data";
	case GF_PROP_POINTER: return "pointer";
	case GF_PROP_VEC2I: return "vec2d int";
	case GF_PROP_VEC2: return "vec2d float";
	case GF_PROP_VEC3I: return "vec3d int";
	case GF_PROP_VEC3: return "vec3d float";
	case GF_PROP_VEC4I: return "vec4d int";
	case GF_PROP_VEC4: return "vec4d float";
	case GF_PROP_PIXFMT: return "pixel format";
	case GF_PROP_PCMFMT: return "audio format";
	case GF_PROP_STRING_LIST: return "string list";
	case GF_PROP_UINT_LIST: return "uint list";
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unknown property type %d\n", type));
	return "Undefined";
}


struct _gf_prop_typedef {
	u32 type;
	const char *name;
	const char *description;
	u8 prop_type;
} GF_BuiltInProps [] =
{
	{ GF_PROP_PID_ID, "ID", "Stream ID of PID", GF_PROP_UINT},
	{ GF_PROP_PID_ESID, "ESID", "MPEG-4 ESID of PID - mandatory if MPEG-4 Systems used", GF_PROP_UINT},
	{ GF_PROP_PID_ITEM_ID, "ItemID", "ID of image item in HEIF, same value as ID", GF_PROP_UINT},
	{ GF_PROP_PID_SERVICE_ID, "ServiceID", "ID of parent service of this PID", GF_PROP_UINT},
	{ GF_PROP_PID_CLOCK_ID, "ClockID", "ID of clock reference PID for this PID", GF_PROP_UINT},
	{ GF_PROP_PID_DEPENDENCY_ID, "DependencyID", "ID of layer dependended on for this PID", GF_PROP_UINT},
	{ GF_PROP_PID_SUBLAYER, "SubLayer", "Indicates this is a sublayer of the stream depended on rather than an enhancement layer", GF_PROP_BOOL},
	{ GF_PROP_PID_PLAYBACK_MODE, "PlaybackMode", "playback mode supported by PID:\n\t0 is no time control\n\t1 is play/pause/seek,speed=1\n\t2 is play/pause/seek,speed>=0\n\t3 is play/pause/seek, reverse playback", GF_PROP_UINT},
	{ GF_PROP_PID_SCALABLE, "Scalable", "Stream is a scalable stream", GF_PROP_BOOL},
	{ GF_PROP_PID_TILE_BASE, "TileBase", "Stream is a tile base stream", GF_PROP_BOOL},
	{ GF_PROP_PID_LANGUAGE, "Language", "Language name for this PID", GF_PROP_NAME},
	{ GF_PROP_PID_SERVICE_NAME, "ServiceName", "Name of parent service of this PID", GF_PROP_STRING},
	{ GF_PROP_PID_SERVICE_PROVIDER, "ServiceProvider", "Provider of parent service of this PID", GF_PROP_STRING},
	{ GF_PROP_PID_STREAM_TYPE, "StreamType", "media stream type", GF_PROP_UINT},
	{ GF_PROP_PID_SUBTYPE, "StreamSubtype", "media subtype (auxiliary, pic sequence, etc ..)", GF_PROP_UINT},
	{ GF_PROP_PID_ORIG_STREAM_TYPE, "OrigStreamType", "Original stream type before encryption", GF_PROP_UINT},
	{ GF_PROP_PID_CODECID, "CodecID", "codec ID (MPEG-4 OTI or ISOBMFF 4CC)", GF_PROP_UINT},
	{ GF_PROP_PID_IN_IOD, "InitialObjectDescriptor", "indicates if PID is declared in the IOD for MPEG-4", GF_PROP_BOOL},
	{ GF_PROP_PID_UNFRAMED, "Unframed", "indicates the media data is not framed (eg each PACKET is not a complete AU/frame)", GF_PROP_BOOL},
	{ GF_PROP_PID_DURATION, "Duration", "indicates the PID duration", GF_PROP_FRACTION},
	{ GF_PROP_PID_NB_FRAMES, "NumFrames", "indicates the number of frames in the stream", GF_PROP_UINT},
	{ GF_PROP_PID_FRAME_SIZE, "ConstantFrameSize", "indicates size of the frame for constant frame size streams", GF_PROP_UINT},
	{ GF_PROP_PID_TIMESHIFT, "TimeshiftDepth", "indicates the depth of the timeshift buffer", GF_PROP_FRACTION},
	{ GF_PROP_PID_TIMESCALE, "Timescale", "timescale of PID (a timestamp of N is N/timescale seconds)", GF_PROP_UINT},
	{ GF_PROP_PID_PROFILE_LEVEL, "ProfileLevel", "MPEG-4 profile and level of the stream", GF_PROP_UINT},
	{ GF_PROP_PID_DECODER_CONFIG, "DecoderConfig", "data property containing the decoder config", GF_PROP_DATA},
	{ GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, "DecoderConfigEnhancement", "data property containing the decoder config of the enhancement layer", GF_PROP_DATA},
	{ GF_PROP_PID_SAMPLE_RATE, "SampleRate", "audio sample rate", GF_PROP_UINT},
	{ GF_PROP_PID_SAMPLES_PER_FRAME, "SamplesPerFrame", "number of audio sample in one coded frame", GF_PROP_UINT},
	{ GF_PROP_PID_NUM_CHANNELS, "NumChannels", "number of audio channels", GF_PROP_UINT},
	{ GF_PROP_PID_CHANNEL_LAYOUT, "ChannelLayout", "Channel Layout", GF_PROP_UINT},
	{ GF_PROP_PID_AUDIO_FORMAT, "AudioFormat", "audio sample format", GF_PROP_PCMFMT},
	{ GF_PROP_PID_AUDIO_SPEED, "AudioPlaybackSpeed", "audio playback speed, only used for audio output reconfiguration", GF_PROP_DOUBLE},
	{ GF_PROP_PID_MEDIA_SKIP, "MediaSkip", "time to skip at begining of stream to recover initial source timing. Negative value imply holding (delaying) the stream", GF_PROP_SINT},
	{ GF_PROP_PID_WIDTH, "Width", "Visual Width (video / text / graphics)", GF_PROP_UINT},
	{ GF_PROP_PID_HEIGHT, "Height", "Visual Height (video / text / graphics)", GF_PROP_UINT},
	{ GF_PROP_PID_PIXFMT, "PixelFormat", "Pixel format", GF_PROP_PIXFMT},
	{ GF_PROP_PID_STRIDE, "Stride", "image or Y/alpha plane stride", GF_PROP_UINT},
	{ GF_PROP_PID_STRIDE_UV, "StrideUV", "UV plane or U/V planes stride", GF_PROP_UINT},
	{ GF_PROP_PID_BIT_DEPTH_Y, "BitDepthLuma", "Bit depth for luma components", GF_PROP_UINT},
	{ GF_PROP_PID_BIT_DEPTH_UV, "BitDepthChroma", "Bit depth for chroma components", GF_PROP_UINT},
	{ GF_PROP_PID_FPS, "FPS", "Video framerate", GF_PROP_FRACTION},
	{ GF_PROP_PID_INTERLACED, "Interlaced", "Video interlaced flag", GF_PROP_BOOL},
	{ GF_PROP_PID_SAR, "SAR", "Sample (ie pixel) aspect ratio", GF_PROP_FRACTION},
	{ GF_PROP_PID_PAR, "PAR", "Picture aspect ratio", GF_PROP_FRACTION},
	{ GF_PROP_PID_WIDTH_MAX, "MaxWidth", "Max Visual Width (video / text / graphics) of all enhancement layers", GF_PROP_UINT},
	{ GF_PROP_PID_HEIGHT_MAX, "MaxHeight", "Max Visual Height (video / text / graphics) of all enhancement layers", GF_PROP_UINT},
	{ GF_PROP_PID_ZORDER, "ZOrder", "Z-order of the video, from 0 (first) to max int (last)", GF_PROP_UINT},
	{ GF_PROP_PID_TRANS_X, "TransX", "Horizontal translation of the video", GF_PROP_SINT},
	{ GF_PROP_PID_TRANS_Y, "TransY", "Vertical translation of the video", GF_PROP_SINT},
	{ GF_PROP_PID_CROP_POS, "CropOrigin", "Position in source window, X,Y indicates coord in source", GF_PROP_VEC2I},
	{ GF_PROP_PID_ORIG_SIZE, "OriginalSize", "Original resolution of video", GF_PROP_VEC2I},
	{ GF_PROP_PID_BITRATE, "Bitrate", "PID bitrate in bps", GF_PROP_UINT},
	{ GF_PROP_PID_MEDIA_DATA_SIZE, "MediaDat Size", "Size in bytes of media in PID", GF_PROP_LUINT},
	{ GF_PROP_PID_CAN_DATAREF, "DataRef", "Inidcates this PID can use data ref", GF_PROP_BOOL},
	{ GF_PROP_PID_URL, "URL", "URL of source", GF_PROP_STRING},
	{ GF_PROP_PID_REMOTE_URL, "RemoteURL", "Remote URL of source", GF_PROP_STRING},
	{ GF_PROP_PID_FILEPATH, "SourcePath", "Path of source file on file system", GF_PROP_STRING},
	{ GF_PROP_PID_MIME, "MIME Type", "MIME type of source", GF_PROP_STRING},
	{ GF_PROP_PID_FILE_EXT, "Extension", "File extension of source", GF_PROP_STRING},
	{ GF_PROP_PID_FILE_CACHED, "Cached", "indicates the file is completely cached", GF_PROP_BOOL},
	{ GF_PROP_PID_DOWN_RATE, "DownloadRate", "dowload rate of resource in bps", GF_PROP_UINT},
	{ GF_PROP_PID_DOWN_SIZE, "DownloadSize", "size of resource in bytes", GF_PROP_LUINT},
	{ GF_PROP_PID_DOWN_BYTES, "DownBytes", "number of bytes downloaded", GF_PROP_LUINT},
	{ GF_PROP_PID_FILE_RANGE, "ByteRange", "byte range of resource", GF_PROP_FRACTION64},
	{ GF_PROP_PID_DISABLE_PROGRESSIVE, "DisableProgressive", "indicates file cannot be progressivley uploaded because first blocks need patching upon closing", GF_PROP_BOOL},
	{ GF_PROP_SERVICE_WIDTH, "ServiceWidth", "display width of service", GF_PROP_UINT},
	{ GF_PROP_SERVICE_HEIGHT, "ServiceHeight", "display height of service", GF_PROP_UINT},
	{ GF_PROP_PID_CAROUSEL_RATE, "CarouselRate", "repeat rate in ms for systems carousel data", GF_PROP_UINT},

	{ GF_PROP_PID_UTC_TIME, "UTC", "UTC date and time of PID", GF_PROP_LUINT},
	{ GF_PROP_PID_UTC_TIMESTAMP, "UTCTimestamp", "timestamp corresponding to UTC date and time of PID", GF_PROP_LUINT},
	{ GF_PROP_PID_AUDIO_VOLUME, "AudioVolume", "Volume of audio PID", GF_PROP_UINT},
	{ GF_PROP_PID_AUDIO_PAN, "AudioPan", "Balance/Pan of audio PID", GF_PROP_UINT},
	{ GF_PROP_PID_AUDIO_PRIORITY, "AudioPriority", "Audio thread priority", GF_PROP_UINT},
	{ GF_PROP_PID_PROTECTION_SCHEME_TYPE, "ProtectionScheme", "protection scheme type (4CC) used", GF_PROP_UINT},
	{ GF_PROP_PID_PROTECTION_SCHEME_VERSION, "SchemeVersion", "protection scheme version used", GF_PROP_UINT},
	{ GF_PROP_PID_PROTECTION_SCHEME_URI, "SchemeURI", "protection scheme URI", GF_PROP_STRING},
	{ GF_PROP_PID_PROTECTION_KMS_URI, "KMS_URI", "URI for key management system", GF_PROP_STRING},

	{ GF_PROP_PID_ISMA_SELECTIVE_ENC, "SelectiveEncryption", "Indicates if ISAM/OMA selective encryption is used", GF_PROP_BOOL},
	{ GF_PROP_PID_ISMA_IV_LENGTH, "IVLength", "Indicates ISAM IV size", GF_PROP_UINT},
	{ GF_PROP_PID_ISMA_KI_LENGTH, "KILength", "Indicates ISAM KeyIndication size", GF_PROP_UINT},

	{ GF_PROP_PID_OMA_CRYPT_TYPE, "CryptType", "Indicates OMA encryption type", GF_PROP_UINT},
	{ GF_PROP_PID_OMA_CID, "ContentID", "Indicates OMA content ID", GF_PROP_STRING},
	{ GF_PROP_PID_OMA_TXT_HDR, "TextualHeaders", "Indicates OMA textual headers", GF_PROP_STRING},
	{ GF_PROP_PID_OMA_CLEAR_LEN, "PlaintextLen", "Indicates OMA size of plaintext data", GF_PROP_LUINT},

	{ GF_PROP_PCK_SENDER_NTP, "SenderNTP", "Sender NTP time", GF_PROP_LUINT},
	{ GF_PROP_PCK_ISMA_BSO, "ISMA_BSO", "Indicates ISMA BSO of the packet", GF_PROP_LUINT},
	{ GF_PROP_PID_ENCRYPTED, "Encrypted", "Indicates packets for the stream are encrypted - changes are signaled through pid_set_info (no reconfigure)", GF_PROP_BOOL},
	{ GF_PROP_PID_OMA_PREVIEW_RANGE, "OMAPreview", "Indicates OMA Preview range ", GF_PROP_LUINT},
	{ GF_PROP_PID_CENC_PSSH, "CENC_PSSH", "Carries PSSH blob for CENC, formatted as (u32)NbSystems [(bin128)SystemID(u32)KID_count[(bin128)keyID](u32)priv_size(char*priv_size)priv_data]", GF_PROP_DATA},
	{ GF_PROP_PCK_CENC_SAI, "CENC_SAI", "Carries CENC SAI for the sample, formated as (char(IV_Size))IV(u16)NbSubSamples [(u16)ClearBytes(u32)CryptedBytes]", GF_PROP_DATA},
	{ GF_PROP_PID_KID, "KID", "Key ID for packets of the PID - changes are signaled through pid_set_info (no reconfigure)", GF_PROP_DATA},
	{ GF_PROP_PID_CENC_IV_SIZE, "IVSize", "IV size for packets of the PID - changes are signaled through pid_set_info (no reconfigure)", GF_PROP_UINT},
	{ GF_PROP_PID_CENC_IV_CONST, "ConstantIV", "Constant IV for packets of the PID - changes are signaled through pid_set_info (no reconfigure)", GF_PROP_DATA},
	{ GF_PROP_PID_CENC_PATTERN, "CENCPattern", "CENC crypt pattern, CENC pattern, skip as frac.num crypt as frac.den - changes are signaled through pid_set_info (no reconfigure)", GF_PROP_FRACTION},
	{ GF_PROP_PID_CENC_STORE, "CENCStore", "Indicates storage location of SAI data - set as a regular property", GF_PROP_UINT},



	{ GF_PROP_PID_AMR_MODE_SET, "AMRModeSet", "ModeSet for AMR and AMR-WideBand", GF_PROP_UINT},
	{ GF_PROP_PCK_SUBS, "SubSampleInfo", "binary blob describing N subsamples of the sample, formatted as N [(u32)flags(u32)size(u32)reserved(u8)priority(u8) discardable]", GF_PROP_DATA},
	{ GF_PROP_PID_MAX_NALU_SIZE, "NALUMaxSize", "Max size of NAL units in stream - set as info, not property", GF_PROP_UINT},
	{ GF_PROP_PCK_FILENUM, "FileNumber", "Index of file when dumping to files", GF_PROP_UINT},
	{ GF_PROP_PCK_FILENAME, "FileName", "Name of output file when dumping / dashing. Must be set on first packet belonging to new file", GF_PROP_STRING},
	{ GF_PROP_PCK_EODS, "EODS", "End of DASH segment", GF_PROP_BOOL},
	{ GF_PROP_PID_MAX_FRAME_SIZE, "MaxFrameSize", "Max size of frame in stream - set as info, not property", GF_PROP_UINT},

	{ GF_PROP_PID_ISOM_TRACK_TEMPLATE, "TrackTemplate", "ISOBMFF serialized track box for this PID, without any sample info (empty stbl and empty dref) - used by isomuxer to reinject specific boxes of input ISOBMFF", GF_PROP_DATA},
	{ GF_PROP_PID_ISOM_UDTA, "MovieUserData", "ISOBMFF serialized moov UDTA and other moov-level boxes (list) for this PID - used by isomuxer to reinject specific boxes of input ISOBMFF", GF_PROP_DATA},
	{ GF_PROP_PID_PERIOD_ID, "Period", "ID of DASH period", GF_PROP_STRING},
	{ GF_PROP_PID_PERIOD_START, "PStart", "DASH Period start - cf dasher help", GF_PROP_DOUBLE},
	{ GF_PROP_PID_PERIOD_DUR, "PDur", "DASH Period duration - cf dasher help", GF_PROP_DOUBLE},
	{ GF_PROP_PID_REP_ID, "Representation", "ID of DASH representation", GF_PROP_STRING},
	{ GF_PROP_PID_AS_ID, "ASID", "ID of parent DASH AS", GF_PROP_UINT},
	{ GF_PROP_PID_MUX_SRC, "MuxSrc", "Identifies mux source(s)", GF_PROP_STRING},
	{ GF_PROP_PID_DASH_MODE, "DashMode", "Indicates DASH mode to muxer, if any. 0 is no DASH, 1 is regular DASH, 2 is VoD", GF_PROP_UINT},
	{ GF_PROP_PID_DASH_DUR, "DashDur", "Indicates DASH target segment duration in seconds to muxer, if any.", GF_PROP_DOUBLE},
	{ GF_PROP_PID_DASH_MULTI_PID, "__no_def_name__", "Pointer to the GF_List of input pids for multi-stsd entries segments.", GF_PROP_POINTER},
	{ GF_PROP_PID_DASH_MULTI_PID_IDX, "__no_def_name__", "1-based index of PID in the multi PID list", GF_PROP_UINT},
	{ GF_PROP_PID_DASH_MULTI_TRACK, "__no_def_name__", "Pointer to the GF_List of input pids for multi-tracks segments.", GF_PROP_POINTER},

	{ GF_PROP_PID_ROLE, "Role", "list of roles for this PID", GF_PROP_STRING_LIST},
	{ GF_PROP_PID_PERIOD_DESC, "PDesc", "list of descriptors for the DASH period containing this PID", GF_PROP_STRING_LIST},
	{ GF_PROP_PID_AS_COND_DESC, "ASDesc", "list of conditionnal descriptors for the DASH AdaptationSet containing this PID. If a pid with the same property type but different value is found, the PIDs will be in different AdaptationSets", GF_PROP_STRING_LIST},
	{ GF_PROP_PID_AS_ANY_DESC, "ASCDesc", "list of common descriptors for the DASH AdaptationSet containing this PID", GF_PROP_STRING_LIST},
	{ GF_PROP_PID_REP_DESC, "RDesc", "list of descriptors for the DASH Representation containing this PID", GF_PROP_STRING_LIST},
	{ GF_PROP_PID_BASE_URL, "BUrl", "list of base URLs for this PID", GF_PROP_STRING_LIST},
	{ GF_PROP_PID_TEMPLATE, "Template", "template to use for DASH generation", GF_PROP_STRING},
	{ GF_PROP_PID_START_NUMBER, "StartNumber", "specifies the start number to use for this DASH representation", GF_PROP_UINT},
	{ GF_PROP_PID_XLINK, "xlink", "specifies remote period URL for DASH", GF_PROP_STRING},
	{ GF_PROP_PID_CLAMP_DUR, "CDur", "max media duration to process from PID in DASH mode", GF_PROP_DOUBLE},
	{ GF_PROP_PID_HLS_PLAYLIST, "HLSPL", "specifies the name of the variant playlist for this media", GF_PROP_STRING},

};

GF_EXPORT
u32 gf_props_get_id(const char *name)
{
	u32 i, nb_props;
	if (!name) return 0;
	nb_props = sizeof(GF_BuiltInProps) / sizeof(struct _gf_prop_typedef);
	for (i=0; i<nb_props; i++) {
		if (!strcmp(GF_BuiltInProps[i].name, name)) return GF_BuiltInProps[i].type;
	}
	return 0;
}

GF_EXPORT
Bool gf_props_get_description(u32 prop_idx, u32 *type, const char **name, const char **description, u8 *prop_type)
{
	u32 nb_props = sizeof(GF_BuiltInProps) / sizeof(struct _gf_prop_typedef);
	if (prop_idx>=nb_props) return GF_FALSE;
	if (type) *type = GF_BuiltInProps[prop_idx].type;
	if (name) *name = GF_BuiltInProps[prop_idx].name;
	if (description) *description = GF_BuiltInProps[prop_idx].description;
	if (prop_type) *prop_type = GF_BuiltInProps[prop_idx].prop_type;
	return GF_TRUE;
}

GF_EXPORT
const char *gf_props_4cc_get_name(u32 prop_4cc)
{
	u32 i, nb_props = sizeof(GF_BuiltInProps) / sizeof(struct _gf_prop_typedef);
	for (i=0; i<nb_props; i++) {
		if (GF_BuiltInProps[i].type==prop_4cc) return GF_BuiltInProps[i].name;
	}
	return NULL;
}

GF_EXPORT
u32 gf_props_4cc_get_type(u32 prop_4cc)
{
	u32 i, nb_props = sizeof(GF_BuiltInProps) / sizeof(struct _gf_prop_typedef);
	for (i=0; i<nb_props; i++) {
		if (GF_BuiltInProps[i].type==prop_4cc) return GF_BuiltInProps[i].prop_type;
	}
	return GF_PROP_FORBIDEN;
}

Bool gf_props_4cc_check_props()
{
	Bool res = GF_TRUE;
	u32 i, j, nb_props = sizeof(GF_BuiltInProps) / sizeof(struct _gf_prop_typedef);
	for (i=0; i<nb_props; i++) {
		for (j=i+1; j<nb_props; j++) {
			if (GF_BuiltInProps[i].type==GF_BuiltInProps[j].type) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Property %s and %s have the same code type %s\n", GF_BuiltInProps[i].name, GF_BuiltInProps[j].name, gf_4cc_to_str(GF_BuiltInProps[i].type) ));
				res = GF_FALSE;
			}
		}
	}
	return res;
}

GF_EXPORT
const char *gf_prop_dump_val(const GF_PropertyValue *att, char dump[GF_PROP_DUMP_ARG_SIZE], Bool dump_data)
{
	switch (att->type) {
	case GF_PROP_SINT:
		sprintf(dump, "%d", att->value.sint);
		break;
	case GF_PROP_UINT:
		sprintf(dump, "%u", att->value.uint);
		break;
	case GF_PROP_LSINT:
		sprintf(dump, ""LLD, att->value.longsint);
		break;
	case GF_PROP_LUINT:
		sprintf(dump, ""LLU, att->value.longuint);
		break;
	case GF_PROP_FRACTION:
		sprintf(dump, "%d/%u", att->value.frac.num, att->value.frac.den);
		break;
	case GF_PROP_FRACTION64:
		sprintf(dump, LLD"/"LLU, att->value.lfrac.num, att->value.lfrac.den);
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
	case GF_PROP_VEC3:
		sprintf(dump, "%lgx%lgx%lg", att->value.vec3.x, att->value.vec3.y, att->value.vec3.y);
		break;
	case GF_PROP_VEC4I:
		sprintf(dump, "%dx%dx%dx%d", att->value.vec4i.x, att->value.vec4i.y, att->value.vec4i.z, att->value.vec4i.w);
		break;
	case GF_PROP_VEC4:
		sprintf(dump, "%lgx%lgx%lgx%lg", att->value.vec4.x, att->value.vec4.y, att->value.vec4.y, att->value.vec4.w);
		break;
	case GF_PROP_PIXFMT:
		sprintf(dump, "%s", gf_pixel_fmt_name(att->value.uint));
		break;
	case GF_PROP_PCMFMT:
		sprintf(dump, "%s", gf_audio_fmt_name(att->value.uint));
		break;
	case GF_PROP_NAME:
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
		return att->value.string;
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
	case GF_PROP_DATA_NO_COPY:
		if (dump_data && att->value.data.size < 40) {
			u32 i;
			sprintf(dump, "%d bytes 0x", att->value.data.size);
			for (i=0; i<att->value.data.size; i++) {
				sprintf(dump, "%02X", (unsigned char) att->value.data.ptr[i]);
			}
		} else {
			sprintf(dump, "%d bytes CRC32 0x%08X", att->value.data.size, gf_crc_32(att->value.data.ptr, att->value.data.size));
		}
		break;
	case GF_PROP_POINTER:
		sprintf(dump, "%p", att->value.ptr);
		break;
	case GF_PROP_STRING_LIST:
	{
		u32 i, count = gf_list_count(att->value.string_list);
		u32 len = GF_PROP_DUMP_ARG_SIZE-1;
		for (i=0; i<count; i++) {
			char *s = gf_list_get(att->value.string_list, i);
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
	{
		u32 i, count = att->value.uint_list.nb_items;
		u32 len = GF_PROP_DUMP_ARG_SIZE-1;
		for (i=0; i<count; i++) {
			char szItem[1024];
			sprintf(szItem, "%u", att->value.uint_list.vals[i]);
			if (!i) {
				strncpy(dump, szItem, len);
			} else {
				strcat(dump, ",");
				strncat(dump, szItem, len-1);
			}
			len = GF_PROP_DUMP_ARG_SIZE - 1 - (u32) strlen(dump);
			if (len<=1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Uint list is too large to fit in predefined property dump buffer of %d bytes, truncating\n", GF_PROP_DUMP_ARG_SIZE));
				return dump;
			}
		}
		return dump;
	}
	case GF_PROP_FORBIDEN:
		sprintf(dump, "forbiden");
		break;
	}
	return dump;
}

