/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
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

GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value)
{
	GF_PropertyValue p;
	memset(&p, 0, sizeof(GF_PropertyValue));
	p.data_len=0;
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
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for int arg %s - using 0\n", value, name));
			p.value.sint = 0;
		} else {
			p.value.sint = atoi(value);
		}
		break;
	case GF_PROP_UINT:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else {
			p.value.uint = (u32) atoi(value);
		}
		break;
	case GF_PROP_LONGSINT:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for long int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else {
			sscanf(value, ""LLD, &p.value.longsint);
		}
		break;
	case GF_PROP_LONGUINT:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for long unsigned int arg %s - using 0\n", value, name));
			p.value.uint = 0;
		} else {
			sscanf(value, ""LLU, &p.value.longuint);
		}
		break;
	case GF_PROP_FRACTION:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
			p.value.frac.num = 0;
			p.value.frac.den = 1;
		} else {
			if (sscanf(value, "%d/%d", &p.value.frac.num, &p.value.frac.den) != 2) {
				p.value.frac.den=1;
				if (sscanf(value, "%d", &p.value.frac.num) != 1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for fraction arg %s - using 0/1\n", value, name));
					p.value.frac.num = 0;
					p.value.frac.den = 1;
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
			sscanf(value, "%f", &f);
			p.value.fnumber = FLT2FIX(f);
		}
		break;
	case GF_PROP_DOUBLE:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for double arg %s - using 0\n", value, name));
			p.value.number = 0;
		} else {
			p.value.number = atof(value);
		}
		break;
	case GF_PROP_NAME:
	case GF_PROP_STRING:
		if (!value) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Wrong argument value %s for string arg %s - ignoring\n", value, name));

			p.type=GF_PROP_FORBIDEN;
		} else {
			p.type=GF_PROP_STRING;
			p.value.string = strdup(value);
		}
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("DATA argument not yet supported for arg %s - ignoring\n", name));
		p.type=GF_PROP_FORBIDEN;
		break;
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("POINTER argument not supported for arg %s - ignoring\n", name));
		p.type=GF_PROP_FORBIDEN;
		break;
	case GF_PROP_FORBIDEN:
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Forbidden property type %d for arg %s - ignoring\n", name, type));
		p.type=GF_PROP_FORBIDEN;
		break;
	}
	return p;
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

	map = gf_fq_pop(filter->prop_maps_reservoir);

	if (!map) {
		GF_SAFEALLOC(map, GF_PropertyMap);
		map->filter = filter;
	}
	assert(!map->reference_count);
	map->reference_count = 1;
	return map;
}

void gf_props_del_property(GF_PropertyMap *prop, GF_PropertyEntry *it)
{
	assert(it->reference_count);
	ref_count_dec(&it->reference_count);
	if (!it->reference_count) {
		if (it->pname && it->name_alloc) gf_free(it->pname);

		if (it->prop.type==GF_PROP_STRING) {
			gf_free(it->prop.value.string);
			it->prop.value.string = NULL;
		}
		else if (it->prop.type==GF_PROP_DATA) {
			gf_free(it->prop.value.data);
			it->prop.value.data = NULL;
		}
		it->prop.data_len = 0;

		gf_fq_add(it->filter->prop_maps_entry_reservoir, it);
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
			gf_fq_add(prop->filter->prop_maps_list_reservoir, l);
		}
	}
}

void gf_props_del(GF_PropertyMap *prop)
{
	gf_props_reset(prop);
	prop->reference_count = 0;
	gf_fq_add(prop->filter->prop_maps_reservoir, prop);
}



//purge existing property of same name
void gf_props_remove_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name)
{
	if (map->hash_table[hash]) {
		u32 i, count = gf_list_count(map->hash_table[hash]);
		for (i=0; i<count; i++) {
			GF_PropertyEntry *prop = gf_list_get(map->hash_table[hash], i);
			if ((p4cc && (p4cc==prop->p4cc)) || (name && !strcmp(prop->pname, name)) ) {
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

	l = gf_fq_pop(map->filter->prop_maps_list_reservoir);

	if (!l) l = gf_list_new();
	return l;
}

GF_Err gf_props_insert_property(GF_PropertyMap *map, u32 hash, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	GF_PropertyEntry *prop;

	if (! map->hash_table[hash] ) {
		map->hash_table[hash] = gf_props_get_list(map);
		if (!map->hash_table[hash]) return GF_OUT_OF_MEM;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("PropertyMap hash collision - %d enries\n", 1+gf_list_count(map->hash_table[hash]) ));
	}

	prop = gf_fq_pop(map->filter->prop_maps_entry_reservoir);

	if (!prop) {
		GF_SAFEALLOC(prop, GF_PropertyEntry);
		if (!prop) return GF_OUT_OF_MEM;
		prop->filter = map->filter;
	}

	prop->reference_count = 1;
	prop->p4cc = p4cc;
	prop->pname = (char *) name;
	if (dyn_name) {
		prop->pname = gf_strdup(dyn_name);
		prop->name_alloc=GF_TRUE;
	}

	memcpy(&prop->prop, value, sizeof(GF_PropertyValue));
	if (prop->prop.type == GF_PROP_STRING)
		prop->prop.value.string = value->value.string ? gf_strdup(value->value.string) : NULL;

	if (prop->prop.type == GF_PROP_DATA) {
		prop->prop.value.data = gf_malloc(sizeof(char) * value->data_len);
		memcpy(prop->prop.value.data, value->value.data, value->data_len);
	} else if (prop->prop.type != GF_PROP_CONST_DATA) {
		prop->prop.data_len = 0;
	}

	return gf_list_add(map->hash_table[hash], prop);
}

GF_Err gf_props_set_property(GF_PropertyMap *map, u32 p4cc, const char *name, char *dyn_name, const GF_PropertyValue *value)
{
	u32 hash = gf_props_hash_djb2(p4cc, name);
	gf_props_remove_property(map, hash, p4cc, name);
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

GF_Err gf_props_merge_property(GF_PropertyMap *dst_props, GF_PropertyMap *src_props)
{
	GF_Err e;
	u32 i, count, idx;
	for (idx=0; idx<HASH_TABLE_SIZE; idx++) {
		if (src_props->hash_table[idx]) {
			count = gf_list_count(src_props->hash_table[idx] );
			if (!dst_props->hash_table[idx]) {
				dst_props->hash_table[idx] = gf_props_get_list(dst_props);
				if (!dst_props->hash_table[idx]) return GF_OUT_OF_MEM;
			}

			for (i=0; i<count; i++) {
				GF_PropertyEntry *prop = gf_list_get(src_props->hash_table[idx], i);
				assert(prop->reference_count);
				ref_count_inc(&prop->reference_count);
				e = gf_list_add(dst_props->hash_table[idx], prop);
				if (e) return e;
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
const char *gf_props_get_type_name(u32 type)
{
	switch (type) {
	case GF_PROP_SINT: return "int";
	case GF_PROP_UINT: return "unsigned int";
	case GF_PROP_LONGSINT: return "long int";
	case GF_PROP_LONGUINT: return "unsigned long int";
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


