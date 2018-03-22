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

GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values)
{
	GF_PropertyValue p;
	memset(&p, 0, sizeof(GF_PropertyValue));
	p.value.data.size=0;
	p.type=type;
	if (!name) name="";

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
		if (!value || (sscanf(value, "%d", &p.value.sint)!=1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for int arg %s - using 0\n", value, name));
			p.value.sint = 0;
		}
		break;
	case GF_PROP_UINT:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else if (enum_values && strchr(enum_values, '|')) {
			char *str_start = strstr(enum_values, value);
			if (!str_start) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s enum %s - using 0\n", value, name, enum_values));
				p.value.uint = 0;
			} else {
				char *pos = (char *)enum_values;
				u32 val=0;
				while (pos != str_start) {
					if (pos[0]=='|') val++;
					pos++;
				}
				p.value.uint = val;
			}
		} else if (!strnicmp(value, "0x", 2)) {
			if (sscanf(value, "0x%x", &p.value.uint)!=1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			}
		} else if (sscanf(value, "%d", &p.value.uint)!=1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
		}
		break;
	case GF_PROP_LSINT:
		if (!value || (sscanf(value, ""LLD, &p.value.longsint)!=1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for long int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		}
		break;
	case GF_PROP_LUINT:
		if (!value || (sscanf(value, ""LLU, &p.value.longuint)!=1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for long unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
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
	case GF_PROP_FLOAT:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for float arg %s - using 0\n", value, name));
			p.value.fnumber = 0;
		} else {
			Float f;
			if (sscanf(value, "%f", &f) != 1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for float arg %s - using 0\n", value, name));
			p.value.fnumber = 0;
			} else {
				p.value.fnumber = FLT2FIX(f);
			}
		}
		break;
	case GF_PROP_DOUBLE:
		if (!value || (sscanf(value, "%lg", &p.value.number) != 1) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for double arg %s - using 0\n", value, name));
			p.value.number = 0;
		}
		break;
	case GF_PROP_VEC2I:
		if (!value || (sscanf(value, "%dx%d", &p.value.vec2i.x, &p.value.vec2i.y) != 2)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2i arg %s - using {0,0}\n", value, name));
			p.value.vec2i.x = p.value.vec2i.x = 0;
		}
		break;
	case GF_PROP_VEC2:
		if (!value || (sscanf(value, "%lgx%lg", &p.value.vec2.x, &p.value.vec2.y) != 2)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec2 arg %s - using {0,0}\n", value, name));
			p.value.vec2.x = p.value.vec2.x = 0;
		}
		break;
	case GF_PROP_VEC3I:
		if (!value || (sscanf(value, "%dx%dx%d", &p.value.vec3i.x, &p.value.vec3i.y, &p.value.vec3i.z) != 3)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec3i arg %s - using {0,0,0}\n", value, name));
			p.value.vec3i.x = p.value.vec3i.x = p.value.vec3i.z = 0;
		}
		break;
	case GF_PROP_VEC3:
		if (!value || (sscanf(value, "%lgx%lgx%lg", &p.value.vec3.x, &p.value.vec3.y, &p.value.vec3.z) != 3)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec3 arg %s - using {0,0,0}\n", value, name));
			p.value.vec3.x = p.value.vec3.x = p.value.vec3.z = 0;
		}
		break;
	case GF_PROP_VEC4I:
		if (!value || (sscanf(value, "%dx%dx%dx%d", &p.value.vec4i.x, &p.value.vec4i.y, &p.value.vec4i.z, &p.value.vec4i.w) != 4)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec4i arg %s - using {0,0,0}\n", value, name));
			p.value.vec4i.x = p.value.vec4i.x = p.value.vec4i.z = p.value.vec4i.w = 0;
		}
		break;
	case GF_PROP_VEC4:
		if (!value || (sscanf(value, "%lgx%lgx%lgx%lg", &p.value.vec4.x, &p.value.vec4.y, &p.value.vec4.z, &p.value.vec4.w) != 4)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for vec4 arg %s - using {0,0,0}\n", value, name));
			p.value.vec4.x = p.value.vec4.x = p.value.vec4.z = p.value.vec4.w = 0;
		}
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
	case GF_PROP_FORBIDEN:
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Forbidden property type %d for arg %s - ignoring\n", type, name));
		p.type=GF_PROP_FORBIDEN;
		break;
	}
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
	case GF_PROP_UINT: return (p1->value.uint==p2->value.uint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_LSINT: return (p1->value.longsint==p2->value.longsint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_LUINT: return (p1->value.longuint==p2->value.longuint) ? GF_TRUE : GF_FALSE;
	case GF_PROP_BOOL: return (p1->value.boolean==p2->value.boolean) ? GF_TRUE : GF_FALSE;
	case GF_PROP_FRACTION:
		return ( (s64) p1->value.frac.num * (s64) p2->value.frac.den == (s64) p2->value.frac.num * (s64) p1->value.frac.den) ? GF_TRUE : GF_FALSE;

	case GF_PROP_FLOAT: return (p1->value.fnumber==p2->value.fnumber) ? GF_TRUE : GF_FALSE;
	case GF_PROP_DOUBLE: return (p1->value.number==p2->value.number) ? GF_TRUE : GF_FALSE;
	case GF_PROP_STRING:
	case GF_PROP_NAME:
		if (!p1->value.string) return p2->value.string ? GF_FALSE : GF_TRUE;
		if (!p2->value.string) return GF_FALSE;
		if (!strcmp(p2->value.string, "*")) return GF_TRUE;
		if (strchr(p2->value.string, '|')) {
			u32 len = strlen(p1->value.string);
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

	//user-managed pointer
	case GF_PROP_POINTER: return (p1->value.ptr==p2->value.ptr) ? GF_TRUE : GF_FALSE;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Comparing forbidden property type %d\n", p1->type));
		break;
	}
	return GF_FALSE;
}

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
	return (hash % HASH_TABLE_SIZE);
}

GF_PropertyMap * gf_props_new(GF_Filter *filter)
{
	GF_PropertyMap *map;

	map = gf_fq_pop(filter->session->prop_maps_reservoir);

	if (!map) {
		GF_SAFEALLOC(map, GF_PropertyMap);
		map->session = filter->session;
	}
	assert(!map->reference_count);
	map->reference_count = 1;
	return map;
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
		it->prop.value.data.size = 0;
		if (it->alloc_size) {
			assert(it->prop.type==GF_PROP_DATA);
			gf_fq_add(it->session->prop_maps_entry_data_alloc_reservoir, it);
		} else {
			gf_fq_add(it->session->prop_maps_entry_reservoir, it);
		}
	}
}

void gf_props_reset(GF_PropertyMap *prop)
{
	u32 i;
	for (i=0; i<HASH_TABLE_SIZE; i++) {
		if (prop->hash_table[i]) {
			GF_List *l = prop->hash_table[i];
			while (gf_list_count(l)) {
				gf_props_del_property(prop, (GF_PropertyEntry *) gf_list_pop_back(l) );
			}
			prop->hash_table[i] = NULL;
			gf_fq_add(prop->session->prop_maps_list_reservoir, l);
		}
	}
}

void gf_props_del(GF_PropertyMap *prop)
{
	gf_props_reset(prop);
	prop->reference_count = 0;
	gf_fq_add(prop->session->prop_maps_reservoir, prop);
}



//purge existing property of same name
void gf_props_remove_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name)
{
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
}

GF_List *gf_props_get_list(GF_PropertyMap *map)
{
	GF_List *l;

	l = gf_fq_pop(map->session->prop_maps_list_reservoir);

	if (!l) l = gf_list_new();
	return l;
}

GF_Err gf_props_insert_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyEntry *prop;
	char *src_ptr;

	if ((value->type == GF_PROP_DATA) || (value->type == GF_PROP_DATA_NO_COPY)) {
		if (!value->value.data.ptr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempt at defining data property %s with NULL pointer, not allowed\n", p4cc ? gf_4cc_to_str(p4cc) : name ? name : dyn_name ));
			return GF_BAD_PARAM;
		}
	}
	if (! map->hash_table[hash] ) {
		map->hash_table[hash] = gf_props_get_list(map);
		if (!map->hash_table[hash]) return GF_OUT_OF_MEM;
	} else {
		u32 i, count = gf_list_count(map->hash_table[hash]);
		if (count) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("PropertyMap hash collision for %s - %d entries before insertion:\n", p4cc ? gf_4cc_to_str(p4cc) : name ? name : dyn_name, gf_list_count(map->hash_table[hash]) ));
			for (i=0; i<count; i++) {
				GF_PropertyEntry *prop_c = gf_list_get(map->hash_table[hash], i);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("\t%s\n\n", prop_c->pname ? prop_c->pname : gf_4cc_to_str(prop_c->p4cc)  ));
				assert(!prop_c->p4cc || (prop_c->p4cc != p4cc));
			}
		}
	}
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
	} else if (prop->prop.type != GF_PROP_CONST_DATA) {
		prop->prop.value.data.size = 0;
	}

	return gf_list_add(map->hash_table[hash], prop);
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
	hash = gf_props_hash_djb2(prop_4cc, name);
	if (map->hash_table[hash] ) {
		count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *p = gf_list_get(map->hash_table[hash], i);

			if ((prop_4cc && (p->p4cc==prop_4cc)) || (p->pname && name && !strcmp(p->pname, name)) )
				return &p->prop;
		}
	}
	return NULL;
}

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props, gf_filter_prop_filter filter_prop, void *cbk)
{
	GF_Err e;
	u32 i, count, idx;
	dst_props->timescale = src_props->timescale;
	for (idx=0; idx<HASH_TABLE_SIZE; idx++) {
		if (src_props->hash_table[idx]) {
			count = gf_list_count(src_props->hash_table[idx] );

			for (i=0; i<count; i++) {
				GF_PropertyEntry *prop = gf_list_get(src_props->hash_table[idx], i);
				assert(prop->reference_count);
				if (!filter_prop || filter_prop(cbk, prop->p4cc, prop->pname, &prop->prop)) {
					safe_int_inc(&prop->reference_count);

					if (!dst_props->hash_table[idx]) {
						dst_props->hash_table[idx] = gf_props_get_list(dst_props);
						if (!dst_props->hash_table[idx]) return GF_OUT_OF_MEM;
					}
					e = gf_list_add(dst_props->hash_table[idx], prop);
					if (e) return e;
				}
			}
		}
	}
	return GF_OK;
}

const GF_PropertyValue *gf_props_enum_property(GF_PropertyMap *props, u32 *io_idx, u32 *prop_4cc, const char **prop_name)
{
	u32 i, count, idx;
	if (!io_idx) return NULL;

	idx = *io_idx;
	if (idx== 0xFFFFFFFF) return NULL;

	for (i=0; i<HASH_TABLE_SIZE; i++) {
		if (props->hash_table[i]) {
			const GF_PropertyEntry *pe;
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
	case GF_PROP_BOOL: return "boolean";
	case GF_PROP_FLOAT: return "float";
	case GF_PROP_DOUBLE: return "number";
	case GF_PROP_NAME: return "string";
	case GF_PROP_STRING: return "string";
	case GF_PROP_DATA: return "data";
	case GF_PROP_CONST_DATA: return "const data";
	case GF_PROP_POINTER: return "pointer";
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unknown property type %d\n", type));
	return "Undefined";
}


struct _gf_prop_typedef {
	u32 type;
	const char *name;
	const char *description;
	u8 prop_type;
} GF_BuiltInProps [] = {

	{ GF_PROP_PID_ID, "ID", "Stream ID of PID", GF_PROP_UINT},
	{ GF_PROP_PID_ESID, "ESID", "MPEG-4 ESID of PID - mandatory if MPEG-4 Systems used", GF_PROP_UINT},
	{ GF_PROP_PID_SERVICE_ID, "ServiceID", "ID of parent service of this PID", GF_PROP_UINT},
	{ GF_PROP_PID_CLOCK_ID, "ClockID", "ID of clock reference PID for this PID", GF_PROP_UINT},
	{ GF_PROP_PID_DEPENDENCY_ID, "DependencyID", "ID of layer dependended on for this PID", GF_PROP_UINT},
	{ GF_PROP_PID_NO_TIME_CTRL, "NoTimeControl", "Indicates time control is not possible on this pid", GF_PROP_BOOL},
	{ GF_PROP_PID_SCALABLE, "Scalable", "Stream is a scalable stream", GF_PROP_BOOL},
	{ GF_PROP_PID_LANGUAGE, "Language", "Language name for this PID", GF_PROP_NAME},
	{ GF_PROP_PID_SERVICE_NAME, "ServiceName", "Name of parent service of this PID", GF_PROP_STRING},
	{ GF_PROP_PID_SERVICE_PROVIDER, "ServiceProvider", "Provider of parent service of this PID", GF_PROP_STRING},
	{ GF_PROP_PID_STREAM_TYPE, "StreamType", "media stream type", GF_PROP_UINT},
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
	{ GF_PROP_PID_AUDIO_FORMAT, "AudioFormat", "audio sample format (u8|s16|s32|flt|dbl|u8p|s16p|s32p|fltp|dblp|s24|s24p)", GF_PROP_UINT},
	{ GF_PROP_PID_WIDTH, "Width", "Visual Width (video / text / graphics)", GF_PROP_UINT},
	{ GF_PROP_PID_HEIGHT, "Height", "Visual Height (video / text / graphics)", GF_PROP_UINT},
	{ GF_PROP_PID_PIXFMT, "PixelFormat", "Pixel format", GF_PROP_UINT},
	{ GF_PROP_PID_STRIDE, "Stride", "image or Y/alpha plane stride", GF_PROP_UINT},
	{ GF_PROP_PID_STRIDE_UV, "StrideUV", "U/V plane stride", GF_PROP_UINT},
	{ GF_PROP_PID_BIT_DEPTH_Y, "BitDepthLuma", "Bit depth for luma components", GF_PROP_UINT},
	{ GF_PROP_PID_BIT_DEPTH_UV, "BitDepthChroma", "Bit depth for chroma components", GF_PROP_UINT},
	{ GF_PROP_PID_FPS, "FPS", "Video framerate", GF_PROP_FRACTION},
	{ GF_PROP_PID_SAR, "SAR", "Sample (ie pixel) aspect ratio", GF_PROP_FRACTION},
	{ GF_PROP_PID_PAR, "PAR", "Picture aspect ratio", GF_PROP_FRACTION},
	{ GF_PROP_PID_WIDTH_MAX, "MaxWidth", "Max Visual Width (video / text / graphics) of all enhancement layers", GF_PROP_UINT},
	{ GF_PROP_PID_HEIGHT_MAX, "MaxHeight", "Max Visual Height (video / text / graphics) of all enhancement layers", GF_PROP_UINT},
	{ GF_PROP_PID_ZORDER, "ZOrder", "Z-order of the video, from 0 (first) to max int (last)", GF_PROP_UINT},
	{ GF_PROP_PID_TRANS_X, "TransX", "Horizontal translation of the video", GF_PROP_SINT},
	{ GF_PROP_PID_TRANS_Y, "TransY", "Vertical translation of the video", GF_PROP_SINT},
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
	{ GF_PROP_PID_DOWN_SIZE, "DownloadSize", "dowload size of resource in bps", GF_PROP_UINT},
	{ GF_PROP_PID_DOWN_BYTES, "DownBytes", "number of bytes downloaded", GF_PROP_UINT},
	{ GF_PROP_PID_FILE_RANGE, "ByteRange", "byte range of resource", GF_PROP_FRACTION},
	{ GF_PROP_SERVICE_WIDTH, "ServiceWidth", "display width of service", GF_PROP_UINT},
	{ GF_PROP_SERVICE_HEIGHT, "ServiceHeight", "display height of service", GF_PROP_UINT},
	{ GF_PROP_PID_UTC_TIME, "UTC", "UTC date and time of PID", GF_PROP_LUINT},
	{ GF_PROP_PID_UTC_TIMESTAMP, "UTCTimestamp", "timestamp corresponding to UTC date and time of PID", GF_PROP_LUINT},
	{ GF_PROP_PID_REVERSE_PLAYBACK, "ReversePlayback", "PID is capable of reverse playback", GF_PROP_BOOL},
	{ GF_PROP_PID_AUDIO_VOLUME, "AudioVolume", "Volume of audio PID", GF_PROP_UINT},
	{ GF_PROP_PID_AUDIO_PAN, "AudioPan", "Balance/Pan of audio PID", GF_PROP_UINT},
	{ GF_PROP_PID_AUDIO_PRIORITY, "AudioPriority", "Audio thread priority", GF_PROP_UINT},
	{ GF_PROP_PID_PROTECTION_SCHEME_TYPE, "ProtectionScheme", "protection scheme type (4CC) used", GF_PROP_UINT},
	{ GF_PROP_PID_PROTECTION_SCHEME_VERSION, "SchemeVersion", "protection scheme version used", GF_PROP_UINT},
	{ GF_PROP_PID_PROTECTION_SCHEME_URI, "SchemeURI", "protection scheme URI", GF_PROP_STRING},
	{ GF_PROP_PID_PROTECTION_KMS_URI, "KMS_URI", "URI for key management system", GF_PROP_STRING},
	{ GF_PROP_PCK_SENDER_NTP, "SenderNTP", "Sender NTP time", GF_PROP_LUINT},
	{ GF_PROP_PCK_ENCRYPTED, "Encrypted", "Indicates the stream is encrypted", GF_PROP_BOOL},
	{ GF_PROP_PCK_ISMA_BSO, "ISMA_BSO", "Indicates ISMA BSO of the packet", GF_PROP_LUINT},
	{ GF_PROP_PID_OMA_PREVIEW_RANGE, "OMAPreview", "Indicates OMA Preview range ", GF_PROP_LUINT},
	{ GF_PROP_PID_CENC_PSSH, "CENC_PSSH", "Carries PSSH blob for CENC, formatted as (u32)NbSystems [(bin128)SystemID(u32)KID_count[(bin128)keyID](u32)priv_size(char*priv_size)priv_data]", GF_PROP_DATA},
	{ GF_PROP_PCK_CENC_SAI, "CENC_SAI", "Carries CENC SAI for the sample, formated as (bin128)KeyID(char(IV_Size))IV(u16)NbSubSamples [(u16)ClearBytes(u32)CryptedBytes]", GF_PROP_DATA},
	{ GF_PROP_PID_PCK_CENC_IV_SIZE, "IVSize", "IV size of the sample", GF_PROP_UINT},
	{ GF_PROP_PID_PCK_CENC_IV_CONST, "ConstantIV", "Constant IV the PID", GF_PROP_DATA},
	{ GF_PROP_PID_PCK_CENC_PATTERN, "CENCPattern", "CENC crypt pattern, CENC pattern, skip as frac.num crypt as frac.den", GF_PROP_FRACTION},
	{ GF_PROP_PID_AMR_MODE_SET, "AMRModeSet", "ModeSet for AMR and AMR-WideBand", GF_PROP_UINT},
	{ GF_PROP_PID_AC3_CFG, "AC3Config", "24 bits of AC3 config as 3GPP", GF_PROP_DATA},
	{ GF_PROP_PCK_SUBS, "SubSampleInfo", "binary blob describing N subsamples of the sample, formatted as N [(u32)flags(u32)size(u32)reserved(u8)priority(u8) discardable]", GF_PROP_DATA},

	{ GF_PROP_PID_MAX_NALU_SIZE, "NALUMaxSize", "Max size of NAL units in stream - set as info, not property", GF_PROP_UINT},
	{ GF_PROP_PCK_FILENUM, "FileNumber", "Index of file when dumping streams made of files", GF_PROP_UINT},

};

const char *gf_props_4cc_get_name(u32 prop_4cc)
{
	u32 i, nb_props = sizeof(GF_BuiltInProps) / sizeof(struct _gf_prop_typedef);
	for (i=0; i<nb_props; i++) {
		if (GF_BuiltInProps[i].type==prop_4cc) return GF_BuiltInProps[i].name;
	}
	return NULL;
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

