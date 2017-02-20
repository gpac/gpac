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

#ifndef _GF_FILTERS_H_
#define _GF_FILTERS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>
#include <gpac/events.h>
//for offsetof()
#include <stddef.h>

typedef struct __gf_media_session GF_FilterSession;

typedef struct __gf_filter GF_Filter;
typedef GF_Err (*filter_callback)(GF_Filter *filter);

typedef struct __gf_filter_pid GF_FilterPid;

typedef struct __gf_filter_pck GF_FilterPacket;
typedef void (*packet_destructor)(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck);


typedef enum
{
	//the scheduler does not use locks for packet and property queues
	GF_FS_SCHEDULER_LOCK_FREE=0,
	//the scheduler uses locks for packet and property queues. Defaults to lock-free if no threads are used
	GF_FS_SCHEDULER_LOCK,
	//the scheduler uses locks for packet and property queues even if single-threaded (test mode)
	GF_FS_SCHEDULER_FORCE_LOCK,
	//the scheduler uses direct dispatch and no threads, trying to nest task calls within task calls
	GF_FS_SCHEDULER_DIRECT,
} GF_FilterSchedulerType;

GF_FilterSession *gf_fs_new(u32 nb_threads, GF_FilterSchedulerType type);
void gf_fs_del(GF_FilterSession *ms);
GF_Filter *gf_fs_load_filter(GF_FilterSession *ms, const char *name);
GF_Err gf_fs_run(GF_FilterSession *ms);
void gf_fs_print_stats(GF_FilterSession *ms);

typedef enum
{
	GF_PROP_FORBIDEN=0,
	GF_PROP_SINT,
	GF_PROP_UINT,
	GF_PROP_LONGSINT,
	GF_PROP_LONGUINT,
	GF_PROP_BOOL,
	GF_PROP_FRACTION,
	GF_PROP_FLOAT,
	GF_PROP_DOUBLE,
	//string property, memory is duplicated when setting the property and managed internally
	GF_PROP_STRING,
	//data property, memory is duplicated when setting the property and managed internally
	GF_PROP_DATA,
	//const string property, memory is NOT duplicated when setting the property, stays user-managed
	GF_PROP_NAME,
	//const data property, memory is NOT duplicated when setting the property, stays user-managed
	GF_PROP_CONST_DATA,
	//user-managed pointer
	GF_PROP_POINTER,
} GF_PropType;

typedef struct
{
	GF_PropType type;
	union {
		u64 longuint;
		s64 longsint;
		u32 sint;
		s32 uint;
		Bool boolean;
		GF_Fraction frac;
		Fixed fnumber;
		Double number;
		//alloc/freed by filter
		char *data;
		//alloc/freed by filter if type is GF_PROP_STRING, otherwise const char *
		char *string;
		void *ptr;
	} value;

	u32 data_len;
} GF_PropertyValue;

const char *gf_props_get_type_name(u32 type);
GF_PropertyValue gf_props_parse_value(u32 type, const char *name, const char *value, const char *enum_values);

typedef struct
{
	const char *arg_name;
	//offset of the argument in the structure
	s32 offset_in_private;
	const char *arg_desc;
	u8 arg_type;
	const char *arg_default_val;
	const char *min_max_enum;
	Bool updatable;
} GF_FilterArgs;

typedef enum
{
	GF_PID_CONFIG_CONNECT,
	GF_PID_CONFIG_UPDATE,
	GF_PID_CONFIG_DISCONNECT
} GF_PID_ConfigState;

typedef struct
{
	//mandatory - name of the filter as used when setting up filters
	const char *name;
	//optional - author of the filter
	const char *author;
	//mandatory - description of the filter
	const char *description;
	//optional - size of private stack structure. The structure is allocated by the framework and arguments are setup before calling this
	u32 private_size;

	//optional - filter arguments if any
	const GF_FilterArgs *args;

	//mandatory - callback for filter processing
	GF_Err (*process)(GF_Filter *filter);

	//optional for sources, mandatory for filters and sinks - callback for PID update
	//may be called several times on the same pid if pid config is changed
	GF_Err (*configure_pid)(GF_Filter *filter, GF_FilterPid *pid, GF_PID_ConfigState state);

	//optional - callback for filter init -  private stack of filter is allocated by framework)
	GF_Err (*initialize)(GF_Filter *filter);

	//optional - callback for filter desctruction -  private stack of filter is freed by framework
	void (*finalize)(GF_Filter *filter);

	//optional - callback for arguments update. If GF_OK is returned, the stack is updated accordingly
	//if function is NULL, all updatable arguments will be changed in the stack without the filter being notified
	GF_Err (*update_arg)(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val);
} GF_FilterRegister;

u32 gf_fs_filters_registry_count(GF_FilterSession *fsess);
const GF_FilterRegister *gf_fs_get_filter_registry(GF_FilterSession *fsess, u32 idx);


void *gf_filter_get_udta(GF_Filter *filter);
void gf_filter_set_name(GF_Filter *filter, const char *name);
const char *gf_filter_get_name(GF_Filter *filter);

GF_FilterPid *gf_filter_pid_new(GF_Filter *filter);

//set a new property to the pid. previous properties (ones set before last packet dispatch)
//will still be valid. You need to remove them one by one using set_property with NULL property, or reset the
//properties with gf_filter_pid_reset_properties
GF_Err gf_filter_pid_set_property(GF_FilterPid *pid, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pid_set_property_str(GF_FilterPid *pid, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pid_set_property_dyn(GF_FilterPid *pid, char *name, const GF_PropertyValue *value);

void gf_filter_pid_set_udta(GF_FilterPid *pid, void *udta);
void *gf_filter_pid_get_udta(GF_FilterPid *pid);

//resets current properties of the pid
GF_Err gf_filter_pid_reset_properties(GF_FilterPid *pid);
//push a new set of properties for dst_pid using all properties from src_pid
GF_Err gf_filter_pid_copy_properties(GF_FilterPid *dst_pid, GF_FilterPid *src_pid);

const GF_PropertyValue *gf_filter_pid_get_property(GF_FilterPid *pid, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pid_get_property_str(GF_FilterPid *pid, const char *prop_name);

GF_Err gf_filter_pid_set_framing_mode(GF_FilterPid *pid, Bool requires_full_blocks);

GF_FilterPacket * gf_filter_pid_get_packet(GF_FilterPid *pid);
void gf_filter_pid_drop_packet(GF_FilterPid *pid);

void gf_filter_send_update(GF_Filter *filter, const char *filter_id, const char *arg_name, const char *arg_val);

void gf_filter_pck_ref(GF_FilterPacket *pck);
void gf_filter_pck_unref(GF_FilterPacket *pck);

GF_FilterPacket *gf_filter_pck_new_alloc(GF_FilterPid *pid, u32 data_size, char **data);
GF_FilterPacket *gf_filter_pck_new_shared(GF_FilterPid *pid, const char *data, u32 data_size, packet_destructor destruct);
GF_FilterPacket *gf_filter_pck_new_ref(GF_FilterPid *pid, const char *data, u32 data_size, GF_FilterPacket *reference);
GF_Err gf_filter_pck_send(GF_FilterPacket *pck);

const char *gf_filter_pck_get_data(GF_FilterPacket *pck, u32 *size);


GF_Err gf_filter_pck_set_property(GF_FilterPacket *pck, u32 prop_4cc, const GF_PropertyValue *value);
GF_Err gf_filter_pck_set_property_str(GF_FilterPacket *pck, const char *name, const GF_PropertyValue *value);
GF_Err gf_filter_pck_set_property_dyn(GF_FilterPacket *pck, char *name, const GF_PropertyValue *value);

//by defaults packet do not share properties with each-other, this functions enable passing the properties of
//one packet to another
GF_Err gf_filter_pck_merge_properties(GF_FilterPacket *pck_src, GF_FilterPacket *pck_dst);
const GF_PropertyValue *gf_filter_pck_get_property(GF_FilterPacket *pck, u32 prop_4cc);
const GF_PropertyValue *gf_filter_pck_get_property_str(GF_FilterPacket *pck, const char *prop_name);

const GF_PropertyValue *gf_filter_pck_enum_properties(GF_FilterPacket *pck, u32 *idx, u32 *prop_4cc, const char **prop_name);


GF_Err gf_filter_pck_set_framing(GF_FilterPacket *pck, Bool is_start, Bool is_end);
GF_Err gf_filter_pck_get_framing(GF_FilterPacket *pck, Bool *is_start, Bool *is_end);



enum
{
	//property indicating PID name, as used when resolving connections
	GF_FILTER_PID_NAME = GF_4CC('g','f','p','N')
};

#ifdef __cplusplus
}
#endif

#endif	//_GF_FILTERS_H_

