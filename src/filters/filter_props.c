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

GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values)
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
	case GF_PROP_NAME:
	case GF_PROP_STRING:
		p.type=GF_PROP_STRING;
		p.value.string = value ? gf_strdup(value) : NULL;
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		if (!value || (sscanf(value, "%p:%d", &p.value.ptr, &p.data_len)!=2) ) {
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
	if (p1->type!=p2->type) return GF_FALSE;
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
		return !strcmp(p1->value.string, p2->value.string) ? GF_TRUE : GF_FALSE;

	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		if (!p1->value.data) return p2->value.data ? GF_FALSE : GF_TRUE;
		if (!p2->value.data) return GF_FALSE;
		if (p1->data_len != p2->data_len) return GF_FALSE;
		return !memcmp(p1->value.data, p2->value.data, p1->data_len) ? GF_TRUE : GF_FALSE;

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
	if (safe_int_dec(&it->reference_count) == 0 ) {
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("PropertyMap hash collision for %s - %d entries\n", p4cc ? gf_4cc_to_str(p4cc) : name ? name : dyn_name, 1+gf_list_count(map->hash_table[hash]) ));
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
	u32 hash = gf_props_hash_djb2(p4cc, name ? name : dyn_name);
	gf_props_remove_property(map, hash, p4cc, name ? name : dyn_name);
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
	dst_props->timescale = src_props->timescale;
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
				safe_int_inc(&prop->reference_count);
				e = gf_list_add(dst_props->hash_table[idx], prop);
				if (e) return e;
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
	{ GF_PROP_PID_SERVICE_ID, "ServiceID", "ID of parent service of this PID", GF_PROP_UINT},

	{ GF_PROP_PID_STREAM_TYPE, "StreamType", "media stream type", GF_PROP_UINT},
	{ GF_PROP_PID_OTI, "ObjectTypeIndication", "codec format as register for MPEG-4", GF_PROP_UINT},
	{ GF_PROP_PID_TIMESCALE, "Timescale", "timescale of PID (a timestamp of N is N/timescale seconds)", GF_PROP_UINT},
	{ GF_PROP_PID_DECODER_CONFIG, "DecoderConfig", "data property containing the decoder config", GF_PROP_DATA},
	{ GF_PROP_PID_SAMPLE_RATE, "SampleRate", "audio sample rate", GF_PROP_UINT},
	{ GF_PROP_PID_SAMPLES_PER_FRAME, "SamplesPerFrame", "number of audio sample in one coded frame", GF_PROP_UINT},
	{ GF_PROP_PID_NUM_CHANNELS, "NumChannels", "number of audio channels", GF_PROP_UINT},
	{ GF_PROP_PID_AUDIO_FORMAT, "AudioFormat", "audio sample format (u8|s16|s32|flt|dbl|u8p|s16p|s32p|fltp|dblp)", GF_PROP_UINT},
	{ GF_PROP_PID_CHANNEL_LAYOUT, "ChannelLayout", "Channel Layout", GF_PROP_UINT},
	{ GF_PROP_PID_WIDTH, "Width", "Visual width (video / text / graphics)", GF_PROP_UINT},
	{ GF_PROP_PID_HEIGHT, "Height", "Visualheight (video / text / graphics)", GF_PROP_UINT},
	{ GF_PROP_PID_FPS, "FPS", "Video framerate", GF_PROP_FRACTION},
	{ GF_PROP_PID_SAR, "SAR", "Sample (ie pixel) aspect ratio", GF_PROP_FRACTION},
	{ GF_PROP_PID_PAR, "PAR", "Picture aspect ratio", GF_PROP_FRACTION},

	{ GF_PROP_PID_PIXFMT, "PixelFormat", "Pixel format", GF_PROP_UINT},
	{ GF_PROP_PID_STRIDE, "Stride", "image or Y/alpha plane stride", GF_PROP_UINT},
	{ GF_PROP_PID_STRIDE_UV, "StrideUV", "U/V plane stride", GF_PROP_UINT},

	{ GF_PROP_PID_BITRATE, "Bitrate", "PID bitrate in bps", GF_PROP_UINT},

	{ GF_PROP_PCK_SENDER_NTP, "SenderNTP", "Indicate sender NTP time if known", GF_PROP_LUINT},
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

