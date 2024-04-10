	/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2021-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / NodeJS module
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

#include "gpac_napi.h"

#include <gpac/thread.h>
#include <gpac/dash.h>
#include <gpac/mpd.h>
#include <gpac/network.h>

#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif


typedef struct
{
	Bool is_init;
	u32 str_buf_alloc;
	char *str_buf;

	u32 nb_args;
	char **argv;

	napi_async_context rmt_ctx;
	napi_ref rmt_ref;

	u32 main_thid;
	GF_List *rmt_messages;
	struct _napi_session *fs_rmt_handler;
	Bool rmt_task_scheduled;
} GPAC_NAPI;

#define GF_SETUP_ERROR	0
#define GF_NOTIF_ERROR	1
#define GF_NOTIF_ERROR_AND_DISCONNECT	2

typedef struct _napi_session
{
	GF_FilterSession *fs;
	napi_async_context async_ctx;
	napi_env env;
	napi_ref ref;
	GF_List *defer_filters;
	Bool blocking;
} NAPI_Session;


typedef struct
{
	napi_ref ref;
	GF_FilterPacket *pck;
	u8 is_src, readonly, dangling;
	napi_ref shared_ab_ref, shared_destructor_ref;
	GF_List *pid_refs;
} NAPI_FilterPacket;

typedef struct
{
	GF_Filter *f;
	GF_FilterSession *fs;
	napi_ref ref;
	napi_env env;

	Bool is_custom;
	napi_async_context async_ctx;

	napi_ref binding_ref;
	void *binding_stack;
} NAPI_Filter;

/*for custom filters only*/
typedef struct
{
	GF_Filter *f;
	GF_FilterPid *pid;

	napi_ref ref;

	NAPI_FilterPacket *napi_pck;
	GF_List *pck_refs;
} NAPI_FilterPid;

typedef struct
{
	napi_ref ref;
	napi_env env;
	napi_ref groups;
} NAPI_Dash;


#define NAPI_GPAC \
	GPAC_NAPI *gpac;\
	if ( napi_get_instance_data(env, (void **) &gpac) != napi_ok) { \
		napi_throw_error(env, NULL, "Cannot get GPAC NAPI instance"); \
		return NULL;\
	}

napi_value filter_enum_pid_props(napi_env env, napi_value cbk_fun, GF_FilterPid *pid);
napi_value filter_get_pid_property(napi_env env, char *pname, GF_FilterPid *pid, Bool is_info);
napi_value fs_wrap_filter(napi_env env, GF_FilterSession *fs, GF_Filter *filter);
napi_status prop_from_napi(napi_env env, napi_value prop, u32 p4cc, char *pname, u32 custom_type, GF_PropertyValue *p);
napi_value prop_to_napi(napi_env env, const GF_PropertyValue *p, u32 p4cc);
napi_value frac_to_napi(napi_env env, const GF_Fraction *frac);
napi_value frac64_to_napi(napi_env env, const GF_Fraction64 *frac);

NAPI_FilterPacket *wrap_filter_packet(napi_env env, GF_FilterPacket *pck, Bool is_src);
napi_value wrap_filterevent(napi_env env, GF_FilterEvent *evt, napi_value *for_val);

napi_type_tag filtersession_tag = {0x475041434e4f4445, 0x2053455353494f4e};
napi_type_tag filter_tag = {0x475041434e4f4445, 0x2046494c54455220};
napi_type_tag filterpid_tag = {0x475041434e4f4445, 0x46696c7472506964};
napi_type_tag filterpck_tag = {0x475041434e4f4445, 0x47465041434b4554};
napi_type_tag filterevent_tag = {0x475041434e4f4445, 0x4746464556454e54};
napi_type_tag dashbind_tag = {0x475041434e4f4445, 0x44415348494e4a53};
napi_type_tag fileio_tag = {0x475041434e4f4445,   0x474646494c45494f};
napi_type_tag httpsess_tag = {0x475041434e4f4445, 0x474646494cf0df5c};

napi_value dashin_bind(napi_env env, napi_callback_info info);
void dashin_detach_binding(NAPI_Filter *napi_f);

napi_value httpout_bind(napi_env env, napi_callback_info info);
void httpout_detach_binding(NAPI_Filter *napi_f);


void dummy_finalize(napi_env env, void* finalize_data, void* finalize_hint)
{
}

napi_value gpac_init(napi_env env, napi_callback_info info)
{
	NAPI_GPAC
	NARG_ARGS(2, 0)

	NARG_U32(mem_track, 0, 0);
	NARG_STR(profile, 1, NULL);

	if (!gpac->is_init) {
		//copy profile as we will destroy gpac->str_buf
		if (profile) profile = strdup(profile);
		if (gpac->str_buf) gf_free(gpac->str_buf);
		gpac->str_buf = NULL;
		gpac->str_buf_alloc = 0;
		gf_sys_close();
		gf_sys_init(mem_track, profile);
		gpac->is_init = GF_TRUE;
		if (profile) free(profile);
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[NAPI] libgpac already initialized, ignoring init(%d, %s)\n", mem_track, profile ? profile : "NULL"));
	}
	return NULL;
}

napi_value gpac_e2s(napi_env env, napi_callback_info info)
{
	napi_value val;
	NARG_ARGS(1, 1)
	NARG_S32(e, 0, 0);
	NAPI_CALL( napi_create_string_utf8(env, gf_error_to_string(e), NAPI_AUTO_LENGTH, &val) );
	return val;
}

napi_value gpac_set_logs(napi_env env, napi_callback_info info)
{
	NARG_ARGS(2, 1)
	NARG_STR(logs, 0, NULL);
	NARG_BOOL(reset, 1, GF_FALSE);
	gf_log_set_tools_levels(logs, reset);
	return NULL;
}

napi_value gpac_sys_clock(napi_env env, napi_callback_info info)
{
	napi_status status;
	napi_value val;
	NAPI_CALL( napi_create_uint32(env, gf_sys_clock(), &val) );
	return val;
}

napi_value gpac_sys_clock_high_res(napi_env env, napi_callback_info info)
{
	napi_status status;
	napi_value val;
	NAPI_CALL( napi_create_bigint_uint64(env, gf_sys_clock_high_res(), &val) );
	return val;
}

void gpac_rmt_callback_exec(napi_env env, GPAC_NAPI *gpac)
{
	napi_status status;
	napi_value global, fun_val;

	status = napi_get_global(env, &global);
	if (status == napi_ok)
		status = napi_get_reference_value(env, gpac->rmt_ref, &fun_val);

	if (status == napi_ok) {
		while (gf_list_count(gpac->rmt_messages)) {
			napi_value res, arg;
			char *msg = gf_list_pop_front(gpac->rmt_messages);

			status = napi_create_string_utf8(env, msg, NAPI_AUTO_LENGTH, &arg);
			if (status == napi_ok)
				status = napi_make_callback(env, gpac->rmt_ctx, global, fun_val, 1, &arg, &res);

			gf_free(msg);
		}
	}
}

Bool fs_flush_rmt(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	NAPI_Session *napi_fs = callback;
	GPAC_NAPI *gpac=NULL;
	napi_get_instance_data(napi_fs->env, (void **) &gpac);
	if (!gpac) return GF_FALSE;

	gpac_rmt_callback_exec(napi_fs->env, gpac);
	if (!gf_list_count(gpac->rmt_messages)) {
		gpac->rmt_task_scheduled = GF_FALSE;
		return GF_FALSE;
	}
	*reschedule_ms = 10;
	return GF_TRUE;
}

void gpac_rmt_callback(void *udta, const char *message)
{
	GPAC_NAPI *gpac=NULL;
	napi_get_instance_data((napi_env) udta, (void **) &gpac);
	if (!gpac) return;

	if (!gpac->rmt_messages) gpac->rmt_messages = gf_list_new();
	gf_list_add(gpac->rmt_messages, gf_strdup(message) );

	//session is running in blocking mode, schedule task to call NodeJS from main thread
	if (gpac->fs_rmt_handler && !gpac->rmt_task_scheduled) {
		gpac->rmt_task_scheduled = GF_TRUE;
		gf_fs_post_user_task(gpac->fs_rmt_handler->fs, fs_flush_rmt, gpac->fs_rmt_handler, "RemoteryFlush");
	}
}


napi_value gpac_set_rmt_fun(napi_env env, napi_callback_info info)
{
	NAPI_GPAC
	napi_value global, name;
	NARG_ARGS(1, 0)

	gpac->is_init = GF_TRUE;
	if (!argc) {
		if (gpac->rmt_ctx) {
			uint32_t res;
			napi_async_destroy(env, gpac->rmt_ctx);
			napi_reference_unref(env, gpac->rmt_ref, &res);
			napi_delete_reference(env, gpac->rmt_ref);
			gpac->rmt_ctx = NULL;
		}
		gf_sys_profiler_set_callback(NULL, NULL);
		return NULL;
	}

	NAPI_CALL( napi_get_global(env, &global) );
	NAPI_CALL( napi_create_string_utf8(env, "GPAC_RemoteryCallback", NAPI_AUTO_LENGTH, &name) );
	NAPI_CALL( napi_async_init(env, global, name, &gpac->rmt_ctx) )

	gf_sys_profiler_set_callback(env, gpac_rmt_callback);
	napi_create_reference(env, argv[0], 1, &gpac->rmt_ref);
	return NULL;
}

napi_value gpac_rmt_log(napi_env env, napi_callback_info info)
{
	NARG_ARGS(1, 1)
	NARG_STR(msg, 0, NULL);
	if (msg)
		gf_sys_profiler_log(msg);
	return NULL;
}

napi_value gpac_rmt_send(napi_env env, napi_callback_info info)
{
	NARG_ARGS(1, 1)
	NARG_STR(msg, 0, NULL);
	if (msg)
		gf_sys_profiler_send(msg);
	return NULL;
}

napi_value gpac_rmt_on(napi_env env, napi_callback_info info)
{
	napi_status status;
	napi_value val;
	NAPI_CALL( napi_get_boolean(env, gf_sys_profiler_sampling_enabled(), &val) );
	return val;
}

napi_value gpac_rmt_enable(napi_env env, napi_callback_info info)
{
	NARG_ARGS(1, 1)
	NARG_BOOL(val, 0, GF_FALSE);
	NAPI_GPAC
	gpac->is_init = GF_TRUE;
	gf_sys_profiler_enable_sampling(val);
	return NULL;
}


napi_value gpac_set_args(napi_env env, napi_callback_info info)
{
	NAPI_GPAC
	bool is_array;
	u32 i, nb_items=0;
	napi_value val;
	NARG_ARGS(1, 1)

	gpac->is_init = GF_TRUE;

	NAPI_CALL( napi_is_array(env, argv[0], &is_array) );
	if (!is_array) {
		napi_throw_error(env, NULL, "NAPI call failed");
		return NULL;
	}
	NAPI_CALL( napi_get_array_length(env, argv[0], &nb_items) );

	if (!nb_items) return NULL;
	if (gpac->argv) {
		for (i=0; i<gpac->nb_args; i++) {
			gf_free(gpac->argv[i]);
		}
		gf_free(gpac->argv);
	}
	gpac->argv = gf_malloc(sizeof(char *) * nb_items);
	gpac->nb_args = nb_items;
	for (i=0; i<gpac->nb_args; i++) {
		size_t size;
		NAPI_CALL( napi_get_element(env, argv[0], i, &val) );
		NAPI_CALL( napi_get_value_string_utf8(env, val, NULL, 0, &size) );
		gpac->argv[i] = gf_malloc(sizeof(char) * (size+1) );
		NAPI_CALL( napi_get_value_string_utf8(env, val, gpac->argv[i], size+1, &size) );
		gpac->argv[i][size]=0;
	}
	gf_sys_set_args(gpac->nb_args, (const char **) gpac->argv);
	return NULL;
}


static void gpac_napi_finalize(napi_env env, void* finalize_data, void* finalize_hint)
{
	GPAC_NAPI *inst = finalize_data;

	if (inst->nb_args) {
		u32 i;
		for (i=0; i<inst->nb_args; i++) {
			gf_free(inst->argv[i]);
		}
		gf_free(inst->argv);
	}

	napi_delete_reference(env, inst->rmt_ref);
	if (inst->str_buf) gf_free(inst->str_buf);
	if (inst->rmt_messages) {
		while (gf_list_count(inst->rmt_messages)) {
			char *msg = gf_list_pop_back(inst->rmt_messages);
			gf_free(msg);
		}
		gf_list_del(inst->rmt_messages);
	}
	//close gpac
	gf_sys_close();

#if defined(GPAC_MEMORY_TRACKING)
	if (gf_memory_size() || gf_file_handles_count()) {
		gf_log_set_tools_levels("mem@info", GF_FALSE);
		gf_memory_print();
	}
#endif

	//not allocated using gf_malloc !!
	free(inst);
}

#define FILTERPCK\
	NAPI_FilterPacket *napi_pck=NULL;\
	GF_FilterPacket *pck=NULL;\
	bool _tag;\
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &filterpck_tag, &_tag) );\
	if (!_tag) {\
		napi_throw_error(env, NULL, "Not a FilterPacket object");\
		return NULL;\
	}\
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &napi_pck) );\
	pck = napi_pck->pck;\
	if (!pck) {\
		napi_throw_error(env, NULL, napi_pck->is_src ? "Detached packet, already drop" : "Detached packet, already sent");\
		return NULL;\
	}\


static u32 FS_PROP_PCK_DTS = 0;
static u32 FS_PROP_PCK_CTS = 1;
static u32 FS_PROP_PCK_SAP = 2;
static u32 FS_PROP_PCK_DUR = 3;
static u32 FS_PROP_PCK_SIZE = 4;
static u32 FS_PROP_PCK_DATA = 5;
static u32 FS_PROP_PCK_START = 6;
static u32 FS_PROP_PCK_END = 7;
static u32 FS_PROP_PCK_TIMESCALE = 8;
static u32 FS_PROP_PCK_INTERLACED = 9;
static u32 FS_PROP_PCK_CORRUPTED = 10;
static u32 FS_PROP_PCK_SEEK = 11;
static u32 FS_PROP_PCK_BYTE_OFFSET = 12;
static u32 FS_PROP_PCK_ROLL = 13;
static u32 FS_PROP_PCK_CRYPT = 14;
static u32 FS_PROP_PCK_CLOCK = 15;
static u32 FS_PROP_PCK_CAROUSEL = 16;
static u32 FS_PROP_PCK_SEQNUM = 17;
static u32 FS_PROP_PCK_DEPS = 18;
static u32 FS_PROP_PCK_FRAME_IFCE = 19;
static u32 FS_PROP_PCK_BLOCKING = 20;

napi_value filterpck_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILTERPCK

	if (magic == &FS_PROP_PCK_DTS) {
		u64 dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else {
			NAPI_CALL( napi_create_int64(env, (s64) dts, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PCK_CTS) {
		u64 cts = gf_filter_pck_get_cts(pck);
		if (cts==GF_FILTER_NO_TS) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else {
			NAPI_CALL( napi_create_int64(env, (s64) cts, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PCK_SAP) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_sap(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_DUR) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_duration(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_SIZE) {
		u32 size;
		gf_filter_pck_get_data(pck, &size);
		NAPI_CALL( napi_create_uint32(env, size, &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_DATA) {
		u32 size;
		u8 *data = (u8 *) gf_filter_pck_get_data(pck, &size);
		if (!data) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else {
			//TODO, mark as readonly using napi_pck->readonly !!
			NAPI_CALL(napi_create_external_arraybuffer(env, data, size, dummy_finalize, NULL, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PCK_START) {
		Bool s, e;
		gf_filter_pck_get_framing(pck, &s, &e);
		NAPI_CALL( napi_get_boolean(env, s, &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_END) {
		Bool s, e;
		gf_filter_pck_get_framing(pck, &s, &e);
		NAPI_CALL( napi_get_boolean(env, e, &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_TIMESCALE) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_timescale(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_INTERLACED) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pck_get_interlaced(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_CORRUPTED) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pck_get_corrupted(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_SEEK) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pck_get_seek_flag(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_BYTE_OFFSET) {
		u64 bo = gf_filter_pck_get_byte_offset(pck);
		if (bo==GF_FILTER_NO_BO) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else {
			NAPI_CALL( napi_create_int64(env, (s64) bo, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PCK_ROLL) {
		NAPI_CALL( napi_create_int32(env, gf_filter_pck_get_roll_info(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_CRYPT) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_crypt_flags(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_CLOCK) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_clock_type(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_CAROUSEL) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_carousel_version(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_SEQNUM) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_seq_num(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_DEPS) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pck_get_dependency_flags(pck), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_FRAME_IFCE) {
		NAPI_CALL( napi_get_boolean(env, (gf_filter_pck_get_frame_interface(pck)!=NULL), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PCK_BLOCKING) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pck_is_blocking_ref(pck), &ret) );
		return ret;
	}
	return NULL;
}

napi_value filterpck_setter(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS_MAGIC(1, 1)
	FILTERPCK

	if (magic && napi_pck->is_src) {
		napi_throw_error(env, NULL, "Cannot set property on input packet");
		return NULL;
	}
	if (magic == &FS_PROP_PCK_DTS) {
		u64 dts;
		napi_valuetype rtype;
		napi_typeof(env, argv[0], &rtype);
		if (rtype == napi_null) {
			dts = GF_FILTER_NO_TS;
		} else {
			NAPI_CALL( napi_get_value_int64(env, argv[0], (s64 *) &dts) );
		}
		gf_filter_pck_set_dts(pck, dts);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_CTS) {
		u64 cts;
		napi_valuetype rtype;
		napi_typeof(env, argv[0], &rtype);
		if (rtype == napi_null) {
			cts = GF_FILTER_NO_TS;
		} else {
			NAPI_CALL( napi_get_value_int64(env, argv[0], (s64 *) &cts) );
		}
		gf_filter_pck_set_dts(pck, cts);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_SAP) {
		NARG_U32(sap, 0, 0)
		gf_filter_pck_set_sap(pck, sap);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_DUR) {
		NARG_U32(dur, 0, 0)
		gf_filter_pck_set_duration(pck, dur);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_SIZE) {
		napi_throw_error(env, NULL, "Cannot set size property");
		return NULL;
	}
	if (magic == &FS_PROP_PCK_DATA) {
		napi_throw_error(env, NULL, "Cannot set data property");
		return NULL;
	}
	if (magic == &FS_PROP_PCK_START) {
		NARG_BOOL(start, 0, 0);
		Bool s, e;
		gf_filter_pck_get_framing(pck, &s, &e);
		gf_filter_pck_set_framing(pck, start, e);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_END) {
		NARG_BOOL(end, 0, 0);
		Bool s, e;
		gf_filter_pck_get_framing(pck, &s, &e);
		gf_filter_pck_set_framing(pck, s, end);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_TIMESCALE) {
		napi_throw_error(env, NULL, "Cannot set timescale on output packet");
		return NULL;
	}
	if (magic == &FS_PROP_PCK_INTERLACED) {
		NARG_BOOL(ilced, 0, 0);
		gf_filter_pck_set_interlaced(pck, ilced);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_CORRUPTED) {
		NARG_BOOL(corr, 0, 0);
		gf_filter_pck_set_interlaced(pck, corr);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_SEEK) {
		NARG_BOOL(seek, 0, 0);
		gf_filter_pck_set_interlaced(pck, seek);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_BYTE_OFFSET) {
		u64 bo;
		napi_valuetype rtype;
		napi_typeof(env, argv[0], &rtype);
		if (rtype == napi_null) {
			bo = GF_FILTER_NO_BO;
		} else {
			NAPI_CALL( napi_get_value_int64(env, argv[0], (s64 *) &bo) );
		}
		gf_filter_pck_set_byte_offset(pck, bo);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_ROLL) {
		NARG_U32(roll, 0, 0);
		gf_filter_pck_set_interlaced(pck, roll);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_CRYPT) {
		NARG_U32(crypt, 0, 0);
		gf_filter_pck_set_interlaced(pck, crypt);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_CLOCK) {
		NARG_U32(clock, 0, 0);
		gf_filter_pck_set_clock_type(pck, clock);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_CAROUSEL) {
		NARG_U32(carv, 0, 0);
		gf_filter_pck_set_carousel_version(pck, carv);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_SEQNUM) {
		NARG_U32(seqnum, 0, 0);
		gf_filter_pck_set_seq_num(pck, seqnum);
		return NULL;
	}
	if (magic == &FS_PROP_PCK_DEPS) {
		NARG_U32(deps, 0, 0);
		gf_filter_pck_set_dependency_flags(pck, deps);
		return NULL;
	}
	return NULL;
}


napi_value filterpck_enum_props(napi_env env, napi_callback_info info)
{
	napi_value global;
	NARG_ARGS_THIS(1, 1)
	FILTERPCK

	napi_valuetype rtype;
	napi_typeof(env, argv[0], &rtype);
	if (rtype != napi_function) {
		napi_throw_error(env, NULL, "property enumerator is not a function");
		return NULL;
	}

	NAPI_CALL( napi_get_global(env, &global) );
	u32 prop_idx=0;
	while (1) {
		napi_handle_scope scope_h;
		u32 p4cc=0;
		napi_value fun_args[3], res;
		const char *pname = NULL;
		const GF_PropertyValue *prop = gf_filter_pck_enum_properties(pck, &prop_idx, &p4cc, &pname);
		if (!prop) break;

		NAPI_CALL(napi_open_handle_scope(env, &scope_h) );

		if (p4cc) {
			pname = gf_props_4cc_get_name(p4cc);
			if (!pname) pname = gf_4cc_to_str(p4cc);
		}

		NAPI_CALL( napi_create_string_utf8(env, pname, NAPI_AUTO_LENGTH, &fun_args[0]) );
		fun_args[1] = prop_to_napi(env, prop, p4cc);
		NAPI_CALL( napi_create_int32(env, prop->type, &fun_args[2]) );

		NAPI_CALL(napi_call_function(env, global, argv[0], 3, fun_args, &res) );
		NAPI_CALL(napi_close_handle_scope(env, scope_h) );
	}
	return NULL;
}


napi_value filterpck_get_prop(napi_env env, napi_callback_info info)
{
	const GF_PropertyValue *prop = NULL;
	NARG_ARGS_THIS(1, 1)
	FILTERPCK

	NARG_STR(pname, 0, NULL);

	u32 p4cc = gf_props_get_id(pname);

	if (p4cc)
		prop = gf_filter_pck_get_property(pck, p4cc);
	else
		prop = gf_filter_pck_get_property_str(pck, pname);

	if (!prop) {
		napi_value res;
		NAPI_CALL(napi_get_null(env, &res) );
		return res;
	}
	return prop_to_napi(env, prop, p4cc);
}

napi_value filterpck_ref(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPCK
	//if dangling, no ref
	if (napi_pck->dangling) return NULL;
	napi_pck->pck = gf_filter_pck_ref_ex(napi_pck->pck);
	//add ref
	u32 nbrefs;
	napi_reference_ref(env, napi_pck->ref, &nbrefs);
	return NULL;
}
napi_value filterpck_unref(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPCK
	//if dangling, no ref
	if (napi_pck->dangling) return NULL;
	gf_filter_pck_unref(pck);
	//remove ref
	u32 nbrefs;
	napi_reference_unref(env, napi_pck->ref, &nbrefs);
	if (!nbrefs)
		napi_pck->pck = NULL;
	return NULL;
}
napi_value filterpck_discard(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPCK

	//if dangling, accept discard as a way to free memory
	if (napi_pck->is_src && !napi_pck->dangling) {
		napi_throw_error(env, NULL, "Cannot discard an input packet");
		return NULL;
	}
	gf_filter_pck_discard(pck);

	//detach
	u32 rcount;
	napi_reference_unref(env, napi_pck->ref, &rcount);
	napi_delete_reference(env, napi_pck->ref);
	napi_pck->ref = NULL;
	napi_pck->pck = NULL;
	return NULL;
}

napi_value filterpck_clone(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_ARGS_THIS(1, 0)
	FILTERPCK
	GF_FilterPacket *cached_pck=NULL;
	NAPI_FilterPacket *napi_cached_pck=NULL;
	GF_FilterPacket *clone=NULL;

	if (argc>=1) {
		napi_valuetype rtype;
		napi_typeof(env, argv[0], &rtype);
		if (rtype == napi_object) {
			NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterpck_tag, &_tag) );
			if (!_tag) {
				napi_throw_error(env, NULL, "Cached packet is not a FilterPacket object");
				return NULL;
			}
			NAPI_CALL( napi_unwrap(env, argv[0], (void**) &napi_cached_pck) );
			cached_pck = napi_cached_pck->pck;
		}
	}

	clone = gf_filter_pck_dangling_copy(pck, cached_pck);
	if (!clone) {
		napi_throw_error(env, NULL, "Failed to clone packet");
		return NULL;
	}
	//same packet used, return same object
	if (clone == cached_pck) {
		NAPI_CALL( napi_get_reference_value(env, napi_cached_pck->ref, &ret) );
		return ret;
	}
	//discard previous napi_cached_pck
	if (napi_cached_pck) {
		u32 nbrefs;
		napi_reference_unref(env, napi_cached_pck->ref, &nbrefs);
		napi_delete_reference(env, napi_cached_pck->ref);
		napi_cached_pck->ref = NULL;
		//napi_cached_pck will be freed when GC collects previous cached_pck
	}

	NAPI_FilterPacket *napi_cloned = wrap_filter_packet(env, clone, GF_FALSE);
	if (!napi_cloned) {
		napi_throw_error(env, NULL, "Failed to clone packet");
		return NULL;
	}
	napi_cloned->readonly = 0;
	napi_cloned->dangling = 1;
	NAPI_CALL( napi_get_reference_value(env, napi_cloned->ref, &ret) );
	return ret;
}

napi_value filterpck_readonly(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPCK
	if (napi_pck->is_src) {
		napi_throw_error(env, NULL, "Cannot set readonly mode for input packet");
		return NULL;
	}
	gf_filter_pck_set_readonly(pck);
	return NULL;
}
napi_value filterpck_send(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPCK
	if (napi_pck->is_src) {
		napi_throw_error(env, NULL, "Cannot send an input packet");
		return NULL;
	}
	gf_filter_pck_send(pck);

	//detach
	u32 rcount;
	napi_reference_unref(env, napi_pck->ref, &rcount);
	if (!rcount) {
		napi_delete_reference(env, napi_pck->ref);
		napi_pck->ref = NULL;
	}
	napi_pck->pck = NULL;
	return NULL;
}


napi_value filterpck_copy_props(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTERPCK
	GF_FilterPacket *from_pck=NULL;
	NAPI_FilterPacket *napi_from_pck=NULL;
	if (napi_pck->is_src) {
		napi_throw_error(env, NULL, "Cannot copy properties on input packet");
		return NULL;
	}

	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterpck_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "No packet to copy properties from");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &napi_from_pck) );
	from_pck = napi_from_pck->pck;

	gf_filter_pck_merge_properties(from_pck, pck);
	return NULL;
}


napi_value filterpck_set_prop(napi_env env, napi_callback_info info)
{
	Bool is_null = GF_FALSE;
	u32 p4cc;
	GF_Err e;
	GF_PropertyValue p;
	NARG_ARGS_THIS(3, 1)
	FILTERPCK
	if (napi_pck->is_src) {
		napi_throw_error(env, NULL, "Cannot set property on input packet");
		return NULL;
	}

	NARG_STR(pname, 0, NULL);
	if (!pname) return NULL;

	p4cc = gf_props_get_id(pname);

	if (argc==1) {
		is_null = GF_TRUE;
	} else {
		napi_valuetype rtype;
		napi_typeof(env, argv[1], &rtype);
		if (rtype == napi_null)
			is_null = GF_TRUE;
		else {
			NARG_U32(custom_type,2,0)
			status = prop_from_napi(env, argv[1], p4cc, pname, custom_type, &p);
			if (status != napi_ok) {
				napi_throw_error(env, NULL, "Cannot set packet property, invalid property value");
				return NULL;
			}
		}
	}
	if (p4cc) {
		e = gf_filter_pck_set_property(pck, p4cc, is_null ? NULL : &p);
	} else {
		e = gf_filter_pck_set_property_dyn(pck, pname, is_null ? NULL : &p);
	}
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Cannot set packet property");
	}
	if (!is_null) {
		gf_props_reset_single(&p);
	}
	return NULL;
}

napi_value filterpck_truncate(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTERPCK

	if (napi_pck->is_src) {
		napi_throw_error(env, NULL, "Cannot truncate an input packet");
		return NULL;
	}
	NARG_U32(size, 0, 0)
	gf_filter_pck_truncate(pck, size);
	return NULL;
}


static u32 FS_PROP_PID_NAME = 0;
static u32 FS_PROP_PID_FILTERNAME = 1;
static u32 FS_PROP_PID_EOS = 2;
static u32 FS_PROP_PID_HAS_SEEN_EOS = 3;
static u32 FS_PROP_PID_HAS_RECEIVED = 4;
static u32 FS_PROP_PID_WOULD_BLOCK = 5;
static u32 FS_PROP_PID_MAX_BUFFER = 6;
static u32 FS_PROP_PID_BUFFER = 7;
static u32 FS_PROP_PID_BUFFER_FULL = 8;
static u32 FS_PROP_PID_FIRST_EMPTY = 9;
static u32 FS_PROP_PID_FIRST_CTS = 10;
static u32 FS_PROP_PID_NB_PCK_QUEUED = 11;
static u32 FS_PROP_PID_TIMESCALE = 12;
static u32 FS_PROP_PID_MIN_PCK_DUR = 13;
static u32 FS_PROP_PID_PLAYING = 14;
static u32 FS_PROP_PID_NEXT_TS = 15;
static u32 FS_PROP_PID_SPARSE = 16;
static u32 FS_PROP_PID_HAS_DECODER = 17;

#define FILTERPID\
	GF_FilterPid *pid=NULL;\
	bool _tag;\
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &filterpid_tag, &_tag) );\
	if (!_tag) {\
		napi_throw_error(env, NULL, "Not a FilterPid object");\
		return NULL;\
	}\
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &pid) );\

napi_value filterpid_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILTERPID

	if (magic == &FS_PROP_PID_NAME) {
		const char *fname = gf_filter_pid_get_name(pid);
		if (fname) {
			NAPI_CALL( napi_create_string_utf8(env, fname, NAPI_AUTO_LENGTH, &ret) );
		} else {
			NAPI_CALL( napi_get_null(env, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PID_FILTERNAME) {
		const char *fname = gf_filter_pid_get_filter_name(pid);
		if (fname) {
			NAPI_CALL( napi_create_string_utf8(env, fname, NAPI_AUTO_LENGTH, &ret) );
		} else {
			NAPI_CALL( napi_get_null(env, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PID_EOS) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_is_eos(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_HAS_SEEN_EOS) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_has_seen_eos(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_HAS_RECEIVED) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_eos_received(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_WOULD_BLOCK) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_would_block(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_SPARSE) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_is_sparse(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_MAX_BUFFER) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pid_get_max_buffer(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_BUFFER) {
		NAPI_CALL( napi_create_int64(env, (s64) gf_filter_pid_query_buffer_duration(pid, GF_FALSE), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_BUFFER_FULL) {
		u64 dur = gf_filter_pid_query_buffer_duration(pid, GF_TRUE);
		NAPI_CALL( napi_get_boolean(env, (dur ? 1 : 0), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_FIRST_EMPTY) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_first_packet_is_empty(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_FIRST_CTS) {
		u64 cts;
		if (!gf_filter_pid_get_first_packet_cts(pid, &cts)) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else if (cts == GF_FILTER_NO_TS) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else {
			NAPI_CALL( napi_create_int64(env, (s64) cts, &ret) );
		}
		return ret;
	}
	if (magic == &FS_PROP_PID_NB_PCK_QUEUED) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pid_get_packet_count(pid), &ret) );
        return ret;
	}
	if (magic == &FS_PROP_PID_TIMESCALE) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pid_get_timescale(pid), &ret) );
        return ret;
	}
	if (magic == &FS_PROP_PID_MIN_PCK_DUR) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_pid_get_min_pck_duration(pid), &ret) );
        return ret;
	}
	if (magic == &FS_PROP_PID_PLAYING) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_is_playing(pid), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_PID_NEXT_TS) {
		u64 cts = gf_filter_pid_get_next_ts(pid);
		if (cts == GF_FILTER_NO_TS) {
			NAPI_CALL( napi_get_null(env, &ret) );
		} else {
			NAPI_CALL( napi_create_int64(env, (s64) cts, &ret) );
		}
        return ret;
	}
	if (magic == &FS_PROP_PID_HAS_DECODER) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_pid_has_decoder(pid), &ret) );
        return ret;
	}
	return NULL;
}

napi_value filterpid_setter(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS_MAGIC(1, 1)
	FILTERPID

	if (magic == &FS_PROP_PID_NAME) {
		NARG_STR(name, 0, NULL)
		gf_filter_pid_set_name(pid, name);
		return NULL;
	}
	if (magic == &FS_PROP_PID_EOS) {
		NARG_BOOL(val, 0, GF_FALSE)
		if (val) gf_filter_pid_set_eos(pid);
		return NULL;
	}
	if (magic == &FS_PROP_PID_MAX_BUFFER) {
		NARG_U32(val, 0, 0)
		gf_filter_pid_set_max_buffer(pid, val);
		return NULL;
	}
	if (magic == &FS_PROP_PID_BUFFER) {
		NARG_U32(val, 0, 0)
		gf_filter_pid_set_max_buffer(pid, val);
		return NULL;
	}
	return NULL;
}


napi_value filterpid_remove(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	gf_filter_pid_remove(pid);
	return NULL;
}

napi_value filterpid_enum_props(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	return filter_enum_pid_props(env, argv[0], pid);
}

napi_value filterpid_get_prop(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NARG_STR(pname, 0, NULL)
	return filter_get_pid_property(env, pname, pid, GF_FALSE);
}
napi_value filterpid_get_info(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NARG_STR(pname, 0, NULL)
	return filter_get_pid_property(env, pname, pid, GF_TRUE);
}

napi_value filterpid_copy_props(napi_env env, napi_callback_info info)
{
	GF_Err e;
	NARG_ARGS_THIS(1,1)
	FILTERPID
	GF_FilterPid *ipid=NULL;
	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterpid_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a FilterPid object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &ipid) );

	e = gf_filter_pid_copy_properties(pid, ipid);
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Cannot copy PID properties");
	}
	return NULL;
}

napi_value filterpid_reset_props(napi_env env, napi_callback_info info)
{
	GF_Err e;
	NARG_ARGS_THIS(1,1)
	FILTERPID
	e = gf_filter_pid_reset_properties(pid);
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Cannot reset PID properties");
	}
	return NULL;
}

napi_value filterpid_set_prop_internal(napi_env env, napi_callback_info info, Bool is_info)
{
	GF_PropertyValue p;
	GF_Err e;
	u32 p4cc=0;
	Bool is_null=GF_FALSE;
	NARG_ARGS_THIS(3,1)
	FILTERPID

	NARG_STR(pname, 0, NULL);
	if (!pname) return NULL;

	p4cc = gf_props_get_id(pname);

	if (argc==1) {
		is_null = GF_TRUE;
	} else {
		napi_valuetype rtype;
		napi_typeof(env, argv[1], &rtype);
		if (rtype == napi_null)
			is_null = GF_TRUE;
		else {
			NARG_U32(custom_type,2,0)
			status = prop_from_napi(env, argv[1], p4cc, pname, custom_type, &p);
			if (status != napi_ok) {
				napi_throw_error(env, NULL, "Cannot set PID property, invalid property value");
				return NULL;
			}
		}
	}
	if (p4cc) {
		if (is_info) {
			e = gf_filter_pid_set_info(pid, p4cc, is_null ? NULL : &p);
		} else {
			e = gf_filter_pid_set_property(pid, p4cc, is_null ? NULL : &p);
		}
	} else {
		if (is_info) {
			e = gf_filter_pid_set_info_dyn(pid, pname, is_null ? NULL : &p);
		} else {
			e = gf_filter_pid_set_property_dyn(pid, pname, is_null ? NULL : &p);
		}
	}
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Cannot set PID property");
	}
	if (!is_null) {
		gf_props_reset_single(&p);
	}
	return NULL;
}

napi_value filterpid_set_prop(napi_env env, napi_callback_info info)
{
	return filterpid_set_prop_internal(env, info, GF_FALSE);
}
napi_value filterpid_set_info(napi_env env, napi_callback_info info)
{
	return filterpid_set_prop_internal(env, info, GF_TRUE);
}

napi_value filterpid_clear_eos(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,0)
	FILTERPID
	NARG_BOOL(all_pids, 0, GF_FALSE);
	gf_filter_pid_clear_eos(pid, all_pids);
	return NULL;
}
napi_value filterpid_check_caps(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	gf_filter_pid_check_caps(pid);
	return NULL;
}
napi_value filterpid_discard_block(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	gf_filter_pid_discard_block(pid);
	return NULL;
}
napi_value filterpid_allow_direct_dispatch(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	gf_filter_pid_allow_direct_dispatch(pid);
	return NULL;
}
napi_value filterpid_get_clock_type(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_THIS
	FILTERPID
	NAPI_CALL( napi_create_uint32(env, gf_filter_pid_get_clock_info(pid, NULL, NULL), &ret) );
	return ret;
}

napi_value filterpid_get_clock_timestamp(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	u64 ts;
	u32 scale;
	gf_filter_pid_get_clock_info(pid, &ts, &scale);
	GF_Fraction64 frac = {ts, scale};
	return frac64_to_napi(env, &frac);
}

napi_value filterpid_is_filter_in_parents(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_ARGS_THIS(1,1)
	FILTERPID
	GF_Filter *f=NULL;
	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filter_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a Filter object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &f) );
	NAPI_CALL( napi_get_boolean(env, gf_filter_pid_is_filter_in_parents(pid, f), &ret) );
	return ret;
}


napi_value filterpid_get_buffer_occupancy(napi_env env, napi_callback_info info)
{
	napi_property_descriptor p_desc = {.attributes=napi_enumerable};
	napi_value obj;
	NARG_THIS
	FILTERPID

	u32 max_units = 0, nb_pck = 0, max_dur = 0, dur = 0;
	Bool in_final_flush = gf_filter_pid_get_buffer_occupancy(pid, &max_units, &nb_pck, &max_dur, &dur);
	in_final_flush = ! in_final_flush;

	NAPI_CALL(napi_create_object(env, &obj) );

#define SET_VAL(_val) \
	p_desc.utf8name = #_val; \
	NAPI_CALL(napi_create_uint32(env, _val, &p_desc.value) ); \
	NAPI_CALL(napi_define_properties(env, obj, 1, &p_desc) ); \

	SET_VAL(max_units);
	SET_VAL(nb_pck);
	SET_VAL(max_dur);
	SET_VAL(dur);

	p_desc.utf8name = "is_final_flush";
	NAPI_CALL(napi_get_boolean(env, in_final_flush, &p_desc.value) );
	NAPI_CALL(napi_define_properties(env, obj, 1, &p_desc) );
	return obj;
}

napi_value filterpid_loose_connect(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	gf_filter_pid_set_loose_connect(pid);
	return NULL;
}

napi_value filterpid_set_framing(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,0)
	FILTERPID
	NARG_BOOL(framed, 0, GF_TRUE);
	gf_filter_pid_set_framing_mode(pid, framed);
	return NULL;
}
napi_value filterpid_set_clock_mode(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NARG_U32(ck_mode, 0, 0);
	gf_filter_pid_set_clock_mode(pid, ck_mode);
	return NULL;
}
napi_value filterpid_set_discard(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NARG_BOOL(do_discard, 0, GF_TRUE);
	gf_filter_pid_set_discard(pid, do_discard);
	return NULL;
}
napi_value filterpid_require_source_id(napi_env env, napi_callback_info info)
{
	GF_Err e;
	NARG_THIS
	FILTERPID
	e = gf_filter_pid_require_source_id(pid);
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to require sourceID for PID");
	}
	return NULL;
}
napi_value filterpid_recompute_dts(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NARG_BOOL(do_recompute, 0, GF_TRUE);
	gf_filter_pid_recompute_dts(pid, do_recompute);
	return NULL;
}

napi_value filterpid_resolve_template(napi_env env, napi_callback_info info)
{
	GF_Err e;
	char szTpl[GF_MAX_PATH];
	NARG_ARGS_THIS(3,1)
	napi_value ret;
	FILTERPID

	NARG_U32(file_idx, 1, 0);
	NARG_STR(suffix, 2, NULL);
	if (suffix) suffix = gf_strdup(suffix);

	NARG_STR(template, 0, NULL);
	if (!template) {
		napi_throw_error(env, NULL, "No template provided");
		if (suffix) gf_free(suffix);
		return NULL;
	}

	e = gf_filter_pid_resolve_file_template(pid, szTpl, template, file_idx, suffix);
	if (suffix) gf_free(suffix);

	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to resolve template for PID");
	}
	NAPI_CALL( napi_create_string_utf8(env, szTpl, NAPI_AUTO_LENGTH, &ret) );
	return ret;
}

napi_value filterpid_query_cap(napi_env env, napi_callback_info info)
{
	napi_value ret;
	u32 p4cc;
	const GF_PropertyValue *cap=NULL;
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NARG_STR(pname, 0, NULL);
	if (!pname) return NULL;

	p4cc = gf_props_get_id(pname);
	if (p4cc) {
		cap = gf_filter_pid_caps_query(pid, p4cc);
	} else {
		cap = gf_filter_pid_caps_query_str(pid, pname);
	}
	if (cap) return prop_to_napi(env, cap, p4cc);

	NAPI_CALL(napi_get_null(env, &ret));
	return ret;
}

napi_value filterpid_negotiate_cap(napi_env env, napi_callback_info info)
{
	u32 p4cc;
	GF_Err e;
	GF_PropertyValue p;
	NARG_ARGS_THIS(3,2)
	FILTERPID
	NARG_STR(pname, 0, NULL);
	if (!pname) return NULL;
	p4cc = gf_props_get_id(pname);
	NARG_U32(custom_type, 2, 0);

	NAPI_CALL( prop_from_napi(env, argv[1], p4cc, pname, custom_type, &p) );

	if (p4cc) {
		e = gf_filter_pid_negotiate_property(pid, p4cc, &p);
	} else {
		e = gf_filter_pid_negotiate_property_dyn(pid, pname, &p);
	}
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to negotiate cap for PID");
	}
	gf_props_reset_single(&p);
	return NULL;
}

void napi_pck_del(napi_env env, void *finalize_data, void* finalize_hint)
{
	NAPI_FilterPacket *napi_pck = (NAPI_FilterPacket *)finalize_data;
	//output packet not sent or dangling packet, discard it
	if (napi_pck->pck && (!napi_pck->is_src || napi_pck->dangling) )
		gf_filter_pck_discard(napi_pck->pck);

	//we still have a ref and a packet, unref - this cleans up calls to pck.ref() without pck.unref()
	//not true for dangling copy
	if (!napi_pck->dangling && napi_pck->ref && napi_pck->pck) {
		gf_filter_pck_unref(napi_pck->pck);
	}
	//if pid is still alive, remove ourselves
	//this is needed because the GC for this packet might happen before the GC for the session/custom filter
	//so we need to make sure we won't call the shared destructor callback
	if (napi_pck->pid_refs) {
		gf_list_del_item(napi_pck->pid_refs, napi_pck);
	}
	gf_free(napi_pck);
}

NAPI_FilterPacket *wrap_filter_packet(napi_env env, GF_FilterPacket *pck, Bool is_src)
{
	napi_value pck_obj;
	NAPI_FilterPacket *napi_pck;

	GF_SAFEALLOC(napi_pck, NAPI_FilterPacket);
	if (!napi_pck) {
		return NULL;
	}
	napi_pck->pck = pck;
	napi_pck->is_src = is_src ? 1 : 0;
	//all packets are read-only by default
	napi_pck->readonly = 1;
	napi_create_object(env, &pck_obj);
	napi_create_reference(env, pck_obj, 1, &napi_pck->ref);
	napi_type_tag_object(env, pck_obj, &filterpck_tag);

	//define all props for custom filter
	napi_property_descriptor filterpck_properties[] = {
		{ "dts", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_DTS},
		{ "cts", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_CTS},
		{ "sap", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_SAP},
		{ "dur", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_DUR},
		{ "size", NULL, NULL, filterpck_getter, NULL, NULL, napi_enumerable, &FS_PROP_PCK_SIZE},
		{ "data", NULL, NULL, filterpck_getter, NULL, NULL, napi_enumerable, &FS_PROP_PCK_DATA},
		{ "start", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_START},
		{ "end", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_END},
		{ "timescale", NULL, NULL, filterpck_getter, NULL, NULL, napi_enumerable, &FS_PROP_PCK_TIMESCALE},
		{ "interlaced", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_INTERLACED},
		{ "corrupted", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_CORRUPTED},
		{ "seek", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_SEEK},
		{ "byte_offset", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_BYTE_OFFSET},
		{ "roll", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_ROLL},
		{ "crypt", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_CRYPT},
		{ "clock", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_CLOCK},
		{ "carousel", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_CAROUSEL},
		{ "seqnum", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_SEQNUM},
		{ "deps", NULL, NULL, filterpck_getter, filterpck_setter, NULL, napi_enumerable, &FS_PROP_PCK_DEPS},
		{ "frame_ifce", NULL, NULL, filterpck_getter, NULL, NULL, napi_enumerable, &FS_PROP_PCK_FRAME_IFCE},
		{ "blocking", NULL, NULL, filterpck_getter, NULL, NULL, napi_enumerable, &FS_PROP_PCK_BLOCKING},

		{ "enum_props", 0, filterpck_enum_props, 0, 0, 0, napi_enumerable, 0 },
		{ "get_prop", 0, filterpck_get_prop, 0, 0, 0, napi_enumerable, 0 },
		{ "ref", 0, filterpck_ref, 0, 0, 0, napi_enumerable, 0 },
		{ "unref", 0, filterpck_unref, 0, 0, 0, napi_enumerable, 0 },
		{ "discard", 0, filterpck_discard, 0, 0, 0, napi_enumerable, 0 },
		{ "clone", 0, filterpck_clone, 0, 0, 0, napi_enumerable, 0 },
		{ "readonly", 0, filterpck_readonly, 0, 0, 0, napi_enumerable, 0 },
		{ "send", 0, filterpck_send, 0, 0, 0, napi_enumerable, 0 },
		{ "copy_props", 0, filterpck_copy_props, 0, 0, 0, napi_enumerable, 0 },
		{ "set_prop", 0, filterpck_set_prop, 0, 0, 0, napi_enumerable, 0 },
		{ "truncate", 0, filterpck_truncate, 0, 0, 0, napi_enumerable, 0 },
	};

	napi_define_properties(env, pck_obj, sizeof(filterpck_properties)/sizeof(napi_property_descriptor), filterpck_properties);

	napi_wrap(env, pck_obj, napi_pck, napi_pck_del, NULL, NULL);
	return napi_pck;
}

napi_value filterpid_get_packet(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_THIS
	FILTERPID
	GF_FilterPacket *pck;
	NAPI_FilterPid *napi_pid = gf_filter_pid_get_udta(pid);

	if (napi_pid->napi_pck) {
		NAPI_CALL( napi_get_reference_value(env, napi_pid->napi_pck->ref, &ret) );
		return ret;
	}
	pck = gf_filter_pid_get_packet(pid);
	if (!pck) {
		NAPI_CALL(napi_get_null(env, &ret));
		return ret;
	}
	//create pck object
	napi_pid->napi_pck = wrap_filter_packet(env, pck, GF_TRUE);
	NAPI_CALL( napi_get_reference_value(env, napi_pid->napi_pck->ref, &ret) );
	return ret;
}

napi_value filterpid_drop_packet(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTERPID
	NAPI_FilterPid *napi_pid = gf_filter_pid_get_udta(pid);

	if (!napi_pid->napi_pck) {
		return NULL;
	}
	u32 rcount;
	napi_reference_unref(env, napi_pid->napi_pck->ref, &rcount);
	if (!rcount) {
		napi_delete_reference(env, napi_pid->napi_pck->ref);
		napi_pid->napi_pck->ref = NULL;
		napi_pid->napi_pck->pck = NULL;
	}
	napi_pid->napi_pck = NULL;

	gf_filter_pid_drop_packet(pid);
	return NULL;
}


napi_value filterpid_send_event(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	GF_FilterEvent *evt=NULL;
	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterevent_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a FilterEvent object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &evt) );
	evt->base.on_pid = pid;
	gf_filter_pid_send_event(pid, evt);
	return NULL;
}

napi_value filterpid_forward(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTERPID
	NAPI_FilterPacket *napi_pck=NULL;
	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterpck_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a FilterPacket  object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &napi_pck) );
	if (napi_pck->pck) {
        GF_Err e = gf_filter_pck_forward(napi_pck->pck, pid);
		if (e) {
			napi_throw_error(env, gf_error_to_string(e), "Failed to forward packet");
		}
	}
	return NULL;
}

napi_value filterpid_new_pck(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_ARGS_THIS(1,0)
	FILTERPID

	NARG_U32(size, 0, 0)
	GF_FilterPacket *pck = gf_filter_pck_new_alloc(pid, size, NULL);
	if (!pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	NAPI_FilterPacket *napi_pck = wrap_filter_packet(env, pck, GF_FALSE);
	if (!napi_pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	napi_pck->readonly = 0;
	NAPI_CALL( napi_get_reference_value(env, napi_pck->ref, &ret) );
	return ret;
}

napi_value filterpid_new_pck_ref(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_ARGS_THIS(3,1)
	FILTERPID

	NAPI_FilterPacket *napi_pck=NULL;
	GF_FilterPacket *ipck=NULL;
	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterpck_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a FilterPacket object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &napi_pck) );
	ipck = napi_pck->pck;
	if (!ipck) {
		napi_throw_error(env, NULL, napi_pck->is_src ? "Detached packet, already drop" : "Detached packet, already sent");
		return NULL;
	}

	NARG_U32(size, 1, 0)
	NARG_U32(offset, 2, 0)

	GF_FilterPacket *pck = gf_filter_pck_new_ref(pid, offset, size, ipck);
	if (!pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	napi_pck = wrap_filter_packet(env, pck, GF_FALSE);
	if (!napi_pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	NAPI_CALL( napi_get_reference_value(env, napi_pck->ref, &ret) );
	return ret;
}

void filter_cbk_release_packet(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	NAPI_Filter *napi_f = gf_filter_get_rt_udta(filter);
	NAPI_FilterPacket *napi_pck = NULL;
	NAPI_FilterPid *napi_pid = gf_filter_pid_get_udta(pid);

	napi_pck = gf_list_pop_front(napi_pid->pck_refs);
	if (!napi_pck) return;

	if (napi_pck->shared_destructor_ref) {
		napi_value fun, obj, res;
		napi_get_reference_value(napi_f->env, napi_pck->shared_destructor_ref, &fun);
		napi_get_global(napi_f->env, &obj);
		napi_make_callback(napi_f->env, napi_f->async_ctx, obj, fun, 0, NULL, &res);
	}

	u32 rcount;

	//unref arraybuffer
	napi_reference_unref(napi_f->env, napi_pck->shared_ab_ref, &rcount);
	napi_delete_reference(napi_f->env, napi_pck->shared_ab_ref);

	//unref destructor
	if (napi_pck->shared_destructor_ref) {
		napi_reference_unref(napi_f->env, napi_pck->shared_ab_ref, &rcount);
		napi_delete_reference(napi_f->env, napi_pck->shared_ab_ref);
	}

	//unref packet
	napi_reference_unref(napi_f->env, napi_pck->ref, &rcount);
	if (rcount==0) {
		napi_delete_reference(napi_f->env, napi_pck->ref);
		napi_pck->ref = NULL;
		napi_pck->pck = NULL;
	}

}

napi_value filterpid_new_pck_shared(napi_env env, napi_callback_info info)
{
	Bool has_destructor=GF_FALSE;
	napi_value ret;
	GF_Err e;
	NARG_ARGS_THIS(2,1)
	FILTERPID

	if (argc>1) {
		napi_valuetype rtype;
		napi_typeof(env, argv[1], &rtype);
		if (rtype != napi_function) {
			napi_throw_error(env, NULL, "destructor is not a function");
			return NULL;
		}
		has_destructor = GF_TRUE;
	}

	NAPI_FilterPid *napi_pid = gf_filter_pid_get_udta(pid);

	Bool is_ab=GF_FALSE;
	NAPI_CALL(napi_is_arraybuffer(env, argv[0], (bool*) &is_ab) );
	if (!is_ab) {
		napi_throw_error(env, NULL, "not an array buffer");
		return NULL;
	}

	u8 *data_p=NULL;
	size_t data_len=0;
	NAPI_CALL(napi_get_arraybuffer_info(env, argv[0], (void **)&data_p, &data_len) );

	GF_FilterPacket *pck = gf_filter_pck_new_shared(pid, data_p, data_len, filter_cbk_release_packet);
	if (!pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	NAPI_FilterPacket *napi_pck = wrap_filter_packet(env, pck, GF_FALSE);
	if (!napi_pck) goto err_exit;

	if (!napi_pid->pck_refs) {
		napi_pid->pck_refs = gf_list_new();
		if (!napi_pid->pck_refs) goto err_exit;
	}
	e = gf_list_add(napi_pid->pck_refs, napi_pck);
	if (e) goto err_exit;

	//ref packet to avoid destruction
	u32 nbrefs;
	status = napi_reference_ref(env, napi_pck->ref, &nbrefs);
	if (status!=napi_ok) goto err_exit;
	//ref array buffer
	status = napi_create_reference(env, argv[0], 1, &napi_pck->shared_ab_ref);
	if (status!=napi_ok) goto err_exit;
	if (has_destructor) {
		status = napi_create_reference(env, argv[1], 1, &napi_pck->shared_destructor_ref);
		if (status!=napi_ok) goto err_exit;
	}

	napi_pck->readonly = 0;
	status = napi_get_reference_value(env, napi_pck->ref, &ret);
	if (status == napi_ok) {
		napi_pck->pid_refs = napi_pid->pck_refs;
		return ret;
	}

err_exit:
	if (pck) gf_filter_pck_discard(pck);
	if (napi_pck) gf_list_del_item(napi_pid->pck_refs, napi_pck);

	napi_throw_error(env, NULL, "Failed to create new packet");
	return NULL;

}

napi_value filterpid_new_pck_copy_clone(napi_env env, napi_callback_info info, Bool is_clone)
{
	napi_value ret;
	NARG_ARGS_THIS(1,1)
	FILTERPID

	NAPI_FilterPacket *napi_pck=NULL;
	GF_FilterPacket *ipck=NULL;
	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterpck_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a FilterPacket object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &napi_pck) );
	ipck = napi_pck ? napi_pck->pck : NULL;
	if (!ipck) {
		napi_throw_error(env, NULL, napi_pck->is_src ? "Detached packet, already drop" : "Detached packet, already sent");
		return NULL;
	}

	GF_FilterPacket *pck;
	if (is_clone)
		pck = gf_filter_pck_new_clone(pid, ipck, NULL);
	else
		pck = gf_filter_pck_new_copy(pid, ipck, NULL);

	if (!pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	napi_pck = wrap_filter_packet(env, pck, GF_FALSE);
	if (!napi_pck) {
		napi_throw_error(env, NULL, "Failed to create new packet");
		return NULL;
	}
	napi_pck->readonly = 0;
	NAPI_CALL( napi_get_reference_value(env, napi_pck->ref, &ret) );
	return ret;
}
napi_value filterpid_new_pck_copy(napi_env env, napi_callback_info info)
{
	return filterpid_new_pck_copy_clone(env, info, GF_FALSE);
}
napi_value filterpid_new_pck_clone(napi_env env, napi_callback_info info)
{
	return filterpid_new_pck_copy_clone(env, info, GF_TRUE);
}


static u32 FILTER_PROP_NAME = 0;
static u32 FILTER_PROP_ID = 1;
static u32 FILTER_PROP_IPIDS = 2;
static u32 FILTER_PROP_OPIDS = 3;

#define FILTER\
	GF_Filter *f=NULL;\
	bool _tag;\
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &filter_tag, &_tag) );\
	if (!_tag) {\
		napi_throw_error(env, NULL, "Not a Filter object");\
		return NULL;\
	}\
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &f) );\

#define FILTER_CUST\
	NAPI_Filter *napi_f = (NAPI_Filter *) gf_filter_get_rt_udta(f);\
	if (!napi_f->is_custom) {\
		napi_throw_error(env, NULL, "Not a custom Filter object");\
		return NULL;\
	}\


#define FILTER_ARG(_name, _argidx)\
	GF_Filter *_name=NULL;\
	bool _fa_tag;\
	NAPI_CALL( napi_check_object_type_tag(env, argv[_argidx], &filter_tag, &_fa_tag) );\
	if (!_fa_tag) {\
		napi_throw_error(env, NULL, "Not a Filter object");\
		return NULL;\
	}\
	NAPI_CALL( napi_unwrap(env, argv[_argidx], (void**) &_name) );\

napi_value filter_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILTER

	if (magic == &FILTER_PROP_NAME) {
		const char *fname = gf_filter_get_name(f);
		if (!fname) return NULL;
		NAPI_CALL( napi_create_string_utf8(env, fname, NAPI_AUTO_LENGTH, &ret) );
		return ret;
	}
	if (magic == &FILTER_PROP_ID) {
		const char *fid = gf_filter_get_name(f);
		if (!fid) return NULL;
		NAPI_CALL( napi_create_string_utf8(env, fid, NAPI_AUTO_LENGTH, &ret) );
		return ret;
	}
	if (magic == &FILTER_PROP_IPIDS) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_get_ipid_count(f), &ret) );
		return ret;
	}
	if (magic == &FILTER_PROP_OPIDS) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_get_opid_count(f), &ret) );
		return ret;
	}

	return NULL;
}

napi_value filter_setter(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS_MAGIC(1, 1)
	FILTER

	if (magic == &FILTER_PROP_NAME) {
		NARG_STR(name, 0, NULL);
		gf_filter_set_name(f, name);
		return NULL;
	}
	return NULL;
}

napi_value filter_remove(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTER
	gf_filter_remove(f);
	return NULL;
}
napi_value filter_update(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(3, 2)
	FILTER
	NARG_STR(name, 0, NULL);
	NARG_STR_ALLOC(value, 1, NULL);
	NARG_U32(mask, 2, 0)

	gf_fs_send_update(NULL, NULL, f, name, value, mask);
	if (value) gf_free(value);
	return NULL;
}
napi_value filter_set_source_internal(napi_env env, napi_callback_info info, Bool is_restricted)
{
	GF_Err e;
	NARG_ARGS_THIS(2, 1)
	FILTER
	FILTER_ARG(src_f, 0)

	NARG_STR(link_ext, 1, NULL);

	if (is_restricted)
		e = gf_filter_set_source_restricted(f, src_f, link_ext);
	else
		e = gf_filter_set_source(f, src_f, link_ext);

	if (e)
		napi_throw_error(env, gf_error_to_string(e), "Failed to set source");
	return NULL;
}
napi_value filter_set_source(napi_env env, napi_callback_info info)
{
	return filter_set_source_internal(env, info, GF_FALSE);
}
napi_value filter_set_source_restricted(napi_env env, napi_callback_info info)
{
	return filter_set_source_internal(env, info, GF_TRUE);
}
napi_value filter_insert(napi_env env, napi_callback_info info)
{
	GF_FilterPid *opid=NULL;
	s32 offset=1;
	GF_Err e;
	NARG_ARGS_THIS(3, 1)
	FILTER
	FILTER_ARG(ins_f, 0)

	//check if 2nd param is int, get opid
	if (argc>1) {
		s32 idx = -1;
		if (napi_get_value_int32(env, argv[1], &idx) == napi_ok) {
			offset = 2;
			if (idx>=0) {
				opid = gf_filter_get_opid(f, (u32) idx);
				if (!opid) {
					napi_throw_error(env, NULL, "Invalid output PID index");
					return NULL;
				}
			}
		}
	}
	NARG_STR(link_ext, (u32)offset, NULL);

	e = gf_filter_set_source(f, ins_f, link_ext);
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to set source");
		return NULL;
	}
	e = gf_filter_reconnect_output(f, opid);
	if (e)
		napi_throw_error(env, gf_error_to_string(e), "Failed to reconnect output");
	return NULL;
}

napi_value filter_reconnect(napi_env env, napi_callback_info info)
{
	GF_FilterPid *opid=NULL;
	GF_Err e;
	NARG_ARGS_THIS(1, 0)
	FILTER

	//check if 2nd param is int, get opid
	if (argc) {
		s32 idx = -1;
		if (napi_get_value_int32(env, argv[0], &idx) == napi_ok) {
			if (idx>=0) {
				opid = gf_filter_get_opid(f, (u32) idx);
				if (!opid) {
					napi_throw_error(env, NULL, "Invalid output PID index");
					return NULL;
				}
			}
		}
	}
	e = gf_filter_reconnect_output(f, opid);
	if (e)
		napi_throw_error(env, gf_error_to_string(e), "Failed to reconnect output");
	return NULL;
}

napi_value filter_probe_link(napi_env env, napi_callback_info info)
{
	GF_Err e;
	char *res=NULL;
	s32 idx = -1;
	s32 offset=1;
	NARG_ARGS_THIS(2, 1)
	FILTER

	//check if 2nd param is int, get opid
	if (argc>1) {
		if (napi_get_value_int32(env, argv[0], &idx) == napi_ok) {
			offset=2;
		}
	}
	NARG_STR(fdesc, (u32)offset, NULL);

	e = gf_filter_probe_link(f, idx, fdesc, &res);
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to probe links");
		return NULL;
	}
	napi_value val;
	NAPI_CALL( napi_create_string_utf8(env, res, NAPI_AUTO_LENGTH, &val) );
	gf_free(res);
	return val;
}

napi_value filter_get_destinations(napi_env env, napi_callback_info info)
{
	GF_Err e;
	char *res=NULL;
	s32 idx = -1;
	NARG_ARGS_THIS(1, 0)
	FILTER

	//check if 2nd param is int, get opid
	if (argc && (napi_get_value_int32(env, argv[0], &idx) == napi_ok)) {
	}

	e = gf_filter_get_possible_destinations(f, idx, &res);
	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to get destinations");
		return NULL;
	}
	napi_value val;
	NAPI_CALL( napi_create_string_utf8(env, res, NAPI_AUTO_LENGTH, &val) );
	gf_free(res);
	return val;
}

napi_value filter_require_source_id(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTER
	gf_filter_require_source_id(f);
	return NULL;
}


napi_value filter_bind(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1,1)
	FILTER

	napi_valuetype rtype;
	napi_typeof(env, argv[0], &rtype);
	if (rtype != napi_object) {
		napi_throw_error(env, NULL, "Binding not an object");
		return NULL;
	}

	const char *fname = gf_filter_get_name(f);
	if (fname && !strcmp(fname, "dashin")) {
		return dashin_bind(env, info);
	}
	if (fname && !strcmp(fname, "httpout")) {
		return httpout_bind(env, info);
	}
	napi_throw_error(env, NULL, "Failed to bind to filter, not dashin filter");
	return NULL;
}


napi_value frac_to_napi(napi_env env, const GF_Fraction *frac)
{
	napi_value val, res;
	napi_status status;
	NAPI_CALL(napi_create_object(env, &res) );
	NAPI_CALL(napi_create_int32(env, frac->num, &val) );
	NAPI_CALL(napi_set_named_property(env, res, "num", val) );
	NAPI_CALL(napi_create_uint32(env, frac->den, &val) );
	NAPI_CALL(napi_set_named_property(env, res, "den", val) );
	return res;
}
napi_value frac64_to_napi(napi_env env, const GF_Fraction64 *frac)
{
	napi_value val, res;
	napi_status status;
	NAPI_CALL(napi_create_object(env, &res) );
	NAPI_CALL(napi_create_int64(env, frac->num, &val) );
	NAPI_CALL(napi_set_named_property(env, res, "num", val) );
	NAPI_CALL(napi_create_int64(env, frac->den, &val) );
	NAPI_CALL(napi_set_named_property(env, res, "den", val) );
	return res;
}
napi_status frac_from_napi(napi_env env, napi_value prop, GF_Fraction *frac)
{
	napi_status status;
	Bool has_p;
	napi_value v;
	GET_PROP_FIELD_S32("num", "Fraction", frac->num)
	GET_PROP_FIELD_U32("den", "Fraction", frac->den)
	return napi_ok;
}

napi_status frac64_from_napi(napi_env env, napi_value prop, GF_Fraction64 *frac)
{
	napi_status status;
	napi_value v;
	Bool has_p;
	GET_PROP_FIELD_S64("num", "Fraction64", frac->num)
	GET_PROP_FIELD_U64("den", "Fraction64", frac->den)
	return napi_ok;
}

#define SET_BOOL(_val) \
	NAPI_CALL(napi_get_boolean(env, stats._val ? true : false, &val) ); \
	NAPI_CALL(napi_set_named_property(env, res, #_val, val) );

#define SET_U32(_val) \
	NAPI_CALL(napi_create_uint32(env, stats._val, &val) ); \
	NAPI_CALL(napi_set_named_property(env, res, #_val, val) );

#define SET_U64(_val) \
	NAPI_CALL(napi_create_int64(env, stats._val, &val) ); \
	NAPI_CALL(napi_set_named_property(env, res, #_val, val) );

#define SET_S64(_val) \
	NAPI_CALL(napi_create_int64(env, stats._val, &val) ); \
	NAPI_CALL(napi_set_named_property(env, res, #_val, val) );

#define SET_S32(_val) \
	NAPI_CALL(napi_create_int32(env, stats._val, &val) ); \
	NAPI_CALL(napi_set_named_property(env, res, #_val, val) );

#define SET_STR(_val) \
	if (stats._val) {\
		NAPI_CALL( napi_create_string_utf8(env, stats._val, NAPI_AUTO_LENGTH, &val) );\
	} else {\
		napi_get_null(env, &val);\
	}\
	NAPI_CALL(napi_set_named_property(env, res, #_val, val) );

napi_value filter_get_statistics(napi_env env, napi_callback_info info)
{
	GF_Err e;
	GF_FilterStats stats;
	napi_value res, val;
	NARG_THIS
	FILTER

	e = gf_filter_get_stats(f, &stats);
 	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to get filter stats");
		return NULL;
	}
	NAPI_CALL(napi_create_object(env, &res) );


	NAPI_CALL(napi_get_boolean(env, stats.filter_alias ? true : false, &val) );
	NAPI_CALL(napi_set_named_property(env, res, "filter_alias", val) );
	if (stats.filter_alias) {
		return res;
	}
	SET_U64(nb_tasks_done)
	SET_U64(nb_pck_processed)
	SET_U64(nb_bytes_processed)
	SET_U64(nb_pck_sent)
	SET_U64(nb_hw_pck_sent)
	SET_U32(nb_errors)
	SET_U64(nb_bytes_sent)
	SET_U64(time_process)
	SET_S32(percent)
	SET_STR(status)
	SET_BOOL(report_updated)
	SET_STR(name)
	SET_STR(reg_name)
	SET_STR(filter_id)
	SET_BOOL(done)
	SET_U32(nb_pid_in)
	SET_U64(nb_in_pck)
	SET_U32(nb_pid_out)
	SET_U64(nb_out_pck)
	SET_BOOL(in_eos)
	SET_U32(type)

	NAPI_CALL( napi_create_string_utf8(env, gf_stream_type_name(stats.stream_type), NAPI_AUTO_LENGTH, &val) );
	NAPI_CALL(napi_set_named_property(env, res, "streamtype", val) );

	char *sep = NULL;
	const char *codecid = gf_codecid_file_ext(stats.codecid);
	if (codecid) sep = strchr(codecid, '|');
	if (sep) {
		NAPI_CALL( napi_create_string_utf8(env, codecid, sep-codecid, &val) );
	} else {
		NAPI_CALL( napi_create_string_utf8(env, codecid, NAPI_AUTO_LENGTH, &val) );
	}
	NAPI_CALL(napi_set_named_property(env, res, "codecid", val) );

	NAPI_CALL(napi_set_named_property(env, res, "last_ts_sent", frac64_to_napi(env, &stats.last_ts_sent)) );
	NAPI_CALL(napi_set_named_property(env, res, "last_ts_drop", frac64_to_napi(env, &stats.last_ts_drop)) );
	return res;
}


napi_value prop_to_napi(napi_env env, const GF_PropertyValue *p, u32 p4cc)
{
	napi_value res, val;
	u32 i;
	napi_status status;


	if (gf_props_type_is_enum(p->type)) {
		NAPI_CALL( napi_create_string_utf8(env, gf_props_enum_name(p->type, p->value.uint), NAPI_AUTO_LENGTH, &res) );
		return res;
	}

	switch (p->type) {
	case GF_PROP_BOOL:
		NAPI_CALL(napi_get_boolean(env, p->value.boolean ? true : false, &res) );
		return res;
	case GF_PROP_SINT:
		NAPI_CALL(napi_create_int32(env, p->value.sint, &res) );
		return res;
	case GF_PROP_UINT:
        if (p4cc== GF_PROP_PID_STREAM_TYPE) {
			NAPI_CALL( napi_create_string_utf8(env, gf_stream_type_name(p->value.uint), NAPI_AUTO_LENGTH, &res) );
			return res;
		}
        if (p4cc== GF_PROP_PID_PIXFMT) {
			NAPI_CALL( napi_create_string_utf8(env, gf_pixel_fmt_sname(p->value.uint), NAPI_AUTO_LENGTH, &res) );
			return res;
		}
        if (p4cc== GF_PROP_PID_CODECID) {
			char *sep=NULL;
			const char *codecid = gf_codecid_file_ext(p->value.uint);
			if (codecid) sep = strchr(codecid, '|');
			if (sep) {
				NAPI_CALL( napi_create_string_utf8(env, codecid, sep-codecid, &res) );
			} else {
				NAPI_CALL( napi_create_string_utf8(env, codecid, NAPI_AUTO_LENGTH, &res) );
			}
			return res;
		}
		NAPI_CALL(napi_create_uint32(env, p->value.uint, &res) );
		return res;
	case GF_PROP_LSINT:
		NAPI_CALL(napi_create_int64(env, p->value.longsint, &res) );
		return res;
	case GF_PROP_LUINT:
		NAPI_CALL(napi_create_int64(env, p->value.longuint, &res) );
		return res;
	case GF_PROP_FRACTION:
		return frac_to_napi(env, &p->value.frac);
	case GF_PROP_FRACTION64:
		return frac64_to_napi(env, &p->value.lfrac);
	case GF_PROP_FLOAT:
		NAPI_CALL(napi_create_double(env, (double) FIX2FLT(p->value.fnumber), &res) );
		return res;
	case GF_PROP_DOUBLE:
		NAPI_CALL(napi_create_double(env, p->value.number, &res) );
		return res;
	case GF_PROP_STRING:
	case GF_PROP_NAME:
		if (p->value.string) {
			NAPI_CALL( napi_create_string_utf8(env, p->value.string, NAPI_AUTO_LENGTH, &res) );
		} else {
			napi_get_null(env, &res);
		}
		return res;
	case GF_PROP_4CC:
		NAPI_CALL( napi_create_string_utf8(env, gf_4cc_to_str(p->value.uint), NAPI_AUTO_LENGTH, &res) );
		return res;

	case GF_PROP_VEC2I:
		NAPI_CALL(napi_create_object(env, &res) );
		NAPI_CALL(napi_create_int32(env, p->value.vec2i.x, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "x", val) );
		NAPI_CALL(napi_create_int32(env, p->value.vec2i.y, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "y", val) );
		return res;
	case GF_PROP_VEC2:
		NAPI_CALL(napi_create_object(env, &res) );
		NAPI_CALL(napi_create_double(env, p->value.vec2.x, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "x", val) );
		NAPI_CALL(napi_create_double(env, p->value.vec2.y, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "y", val) );
		return res;
	case GF_PROP_VEC3I:
		NAPI_CALL(napi_create_object(env, &res) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.x, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "x", val) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.y, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "y", val) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.z, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "z", val) );
		return res;
	case GF_PROP_VEC4I:
		NAPI_CALL(napi_create_object(env, &res) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.x, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "x", val) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.y, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "y", val) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.z, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "z", val) );
		NAPI_CALL(napi_create_int32(env, p->value.vec3i.z, &val) );
		NAPI_CALL(napi_set_named_property(env, res, "w", val) );
		return res;
	case GF_PROP_DATA:
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_CONST_DATA:
		if (p->value.data.ptr) {
#if 0
			NAPI_CALL(napi_create_external_arraybuffer(env, p->value.data.ptr, p->value.data.size, dummy_finalize, NULL, &res) );
#else
			u8 *output;
			NAPI_CALL(napi_create_buffer(env, p->value.data.size, (void**) &output, &res) );
			memcpy(output, p->value.data.ptr, p->value.data.size);
#endif
		} else {
			napi_get_null(env, &res);
		}
		return res;
	case GF_PROP_POINTER:
		NAPI_CALL( napi_create_string_utf8(env, "INTERNAL_POINTER", NAPI_AUTO_LENGTH, &res) );
		return res;
	case GF_PROP_UINT_LIST:
		NAPI_CALL(napi_create_array_with_length(env, p->value.uint_list.nb_items, &res) );
		for (i=0; i<p->value.uint_list.nb_items; i++) {
			NAPI_CALL(napi_create_uint32(env, p->value.uint_list.vals[i], &val) );
			NAPI_CALL(napi_set_element(env, res, i, val) );
		}
		return res;
	case GF_PROP_SINT_LIST:
		NAPI_CALL(napi_create_array_with_length(env, p->value.sint_list.nb_items, &res) );
		for (i=0; i<p->value.uint_list.nb_items; i++) {
			NAPI_CALL(napi_create_int32(env, p->value.sint_list.vals[i], &val) );
			NAPI_CALL(napi_set_element(env, res, i, val) );
		}
		return res;
	case GF_PROP_STRING_LIST:
		NAPI_CALL(napi_create_array_with_length(env, p->value.string_list.nb_items, &res) );
		for (i=0; i<p->value.string_list.nb_items; i++) {
			if (p->value.string_list.vals[i]) {
				NAPI_CALL( napi_create_string_utf8(env, p->value.string_list.vals[i], NAPI_AUTO_LENGTH, &val) );
			} else {
				napi_get_null(env, &val);
			}
			NAPI_CALL(napi_set_element(env, res, i, val) );
		}
		return res;
	case GF_PROP_4CC_LIST:
		NAPI_CALL(napi_create_array_with_length(env, p->value.uint_list.nb_items, &res) );
		for (i=0; i<p->value.uint_list.nb_items; i++) {
			NAPI_CALL( napi_create_string_utf8(env, gf_4cc_to_str(p->value.uint_list.vals[i]), NAPI_AUTO_LENGTH, &val) );
			NAPI_CALL(napi_set_element(env, res, i, val) );
		}
		return res;
	case GF_PROP_VEC2I_LIST:
		NAPI_CALL(napi_create_array_with_length(env, p->value.v2i_list.nb_items, &res) );
		for (i=0; i<p->value.v2i_list.nb_items; i++) {
			napi_value item;
			NAPI_CALL(napi_create_object(env, &item) );
			NAPI_CALL(napi_create_int32(env, p->value.v2i_list.vals[i].x, &val) );
			NAPI_CALL(napi_set_named_property(env, item, "x", val) );
			NAPI_CALL(napi_create_int32(env, p->value.v2i_list.vals[i].y, &val) );
			NAPI_CALL(napi_set_named_property(env, item, "y", val) );
			NAPI_CALL(napi_set_element(env, res, i, item) );
		}
		return res;
	default:
		napi_throw_error(env, NULL, "unknown property type");
		return NULL;
	}
}

napi_status napi_get_str(napi_env env, napi_value val, char **ostr)
{
	GPAC_NAPI *gpac;
	size_t s;
	napi_status status;
	status = napi_get_instance_data(env, (void **) &gpac);
	if (status != napi_ok) return status;

	napi_valuetype vtype;
	status = napi_typeof(env, val, &vtype);
	if (status != napi_ok) return status;
	if (vtype == napi_null) {
		*ostr = NULL;
		return napi_ok;
	}

	status = napi_get_value_string_utf8(env, val, NULL, 0, &s);
	if (status != napi_ok) return status;

	if (s+1 > gpac->str_buf_alloc) {
		gpac->str_buf = gf_realloc(gpac->str_buf, sizeof(char) * (s+1));
		if (!gpac->str_buf) {
			napi_throw_error(env, NULL, "Out of memory");
			return napi_generic_failure;
		}
		gpac->str_buf_alloc = s+1;
	}
	status = napi_get_value_string_utf8(env, val, gpac->str_buf, gpac->str_buf_alloc, &s);
	if (status != napi_ok) return status;
	gpac->str_buf[s]=0;
	*ostr = gpac->str_buf;
	return napi_ok;
}

napi_status prop_from_napi(napi_env env, napi_value prop, u32 p4cc, char *pname, u32 custom_type, GF_PropertyValue *p)
{
	char *str;
	napi_status status;
	Bool has_p;
	napi_value v;

	memset(p, 0, sizeof(GF_PropertyValue));
	if (!p4cc) {
		if (!custom_type) {
            p->type = GF_PROP_STRING;
            status = napi_get_str(env, prop, &str);
			if (status != napi_ok) return status;
            p->value.string = str ? gf_strdup(str) : NULL;
            return napi_ok;
		}
        p->type = custom_type;
	} else {
		p->type = gf_props_4cc_get_type(p4cc);
	}

    if (p4cc == GF_PROP_PID_STREAM_TYPE) {
		status = napi_get_str(env, prop, &str);
		if (status != napi_ok) return status;
		if (!str) {
			napi_throw_error(env, NULL, "Invalid StreamType");
			return napi_generic_failure;
		}
        p->value.uint = gf_stream_type_by_name( str );
        return napi_ok;
	}
    if (p4cc == GF_PROP_PID_CODECID) {
		status = napi_get_str(env, prop, &str);
		if (status != napi_ok) return status;
		if (!str) {
			napi_throw_error(env, NULL, "Invalid CodecID");
			return napi_generic_failure;
		}
        p->value.uint = gf_codecid_parse( str );
        return napi_ok;
	}
	if (gf_props_type_is_enum(p->type)) {
		status = napi_get_str(env, prop, &str);
		if (status != napi_ok) return status;
		if (!str) {
			napi_throw_error(env, NULL, "Invalid Enum");
			return napi_generic_failure;
		}
		p->value.uint = gf_props_parse_enum(p->type, str);
        return napi_ok;
	}
	if (p->type==GF_PROP_SINT) {
		return napi_get_value_int32(env, prop, &p->value.sint);
	}
	if (p->type==GF_PROP_UINT) {
		return napi_get_value_uint32(env, prop, &p->value.uint);
	}
	if (p->type==GF_PROP_4CC) {
		status = napi_get_str(env, prop, &str);
		if (status != napi_ok) return status;
		if (!str) {
			napi_throw_error(env, NULL, "Invalid 4CC");
			return napi_generic_failure;
		}
		p->value.uint = gf_4cc_parse(str);
		return napi_ok;
	}
	if (p->type==GF_PROP_LSINT) {
		return napi_get_value_int64(env, prop, &p->value.longsint);
	}
	if (p->type==GF_PROP_LUINT) {
		return napi_get_value_int64(env, prop, (s64 *) &p->value.longuint);
	}
	if (p->type==GF_PROP_BOOL) {
		bool val;
		status = napi_get_value_bool(env, prop, &val);
		p->value.boolean = val ? GF_TRUE : GF_FALSE;
		return status;
	}

	if (p->type==GF_PROP_FRACTION) {
		GET_PROP_FIELD_S32("num", "Fraction", p->value.frac.num)
		GET_PROP_FIELD_U32("den", "Fraction", p->value.frac.den)
		return napi_ok;
	}
	if (p->type==GF_PROP_FRACTION64) {
		GET_PROP_FIELD_S64("num", "Fraction64", p->value.lfrac.num)
		GET_PROP_FIELD_U64("den", "Fraction64", p->value.lfrac.den)
		return napi_ok;
	}
	if (p->type==GF_PROP_FLOAT) {
		Double n;
		status = napi_get_value_double(env, prop, &n);
		p->value.fnumber = FLT2FIX(n);
		return status;
	}
	if (p->type==GF_PROP_DOUBLE) {
		return napi_get_value_double(env, prop, &p->value.number);
	}
	if (p->type==GF_PROP_VEC2I) {
		GET_PROP_FIELD_S32("x", "Vec2i", p->value.vec2i.x)
		GET_PROP_FIELD_S32("y", "Vec2i", p->value.vec2i.y)
		return napi_ok;
	}
	if (p->type==GF_PROP_VEC2) {
		GET_PROP_FIELD_FLT("x", "Vec2", p->value.vec2.x)
		GET_PROP_FIELD_FLT("y", "Vec2", p->value.vec2.y)
		return napi_ok;
	}
	if (p->type==GF_PROP_VEC3I) {
		GET_PROP_FIELD_S32("x", "Vec3i", p->value.vec3i.x)
		GET_PROP_FIELD_S32("y", "Vec3i", p->value.vec3i.y)
		GET_PROP_FIELD_S32("z", "Vec3i", p->value.vec3i.z)
		return napi_ok;
	}
	if (p->type==GF_PROP_VEC4I) {
		GET_PROP_FIELD_S32("x", "Vec4i", p->value.vec4i.x)
		GET_PROP_FIELD_S32("y", "Vec4i", p->value.vec4i.y)
		GET_PROP_FIELD_S32("z", "Vec4i", p->value.vec4i.z)
		GET_PROP_FIELD_S32("w", "Vec4i", p->value.vec4i.w)
		return napi_ok;
	}
	if ((p->type==GF_PROP_STRING) || (p->type==GF_PROP_STRING_NO_COPY) || (p->type==GF_PROP_NAME)) {
		status = napi_get_str(env, prop, &str);
		if (status != napi_ok) return status;
		p->value.string = str ? gf_strdup(p->value.string) : NULL;
		return napi_ok;
	}
	if ((p->type==GF_PROP_DATA) || (p->type==GF_PROP_DATA_NO_COPY) || (p->type==GF_PROP_CONST_DATA)) {
		napi_throw_error(env, NULL, "Setting data property from JS not yet implemented");
		return napi_generic_failure;
	}
	if (p->type==GF_PROP_POINTER) {
		napi_throw_error(env, NULL, "Setting pointer property from JS not supported");
		return napi_generic_failure;
	}

	if ((p->type==GF_PROP_UINT_LIST) || (p->type==GF_PROP_4CC_LIST)
		|| (p->type==GF_PROP_SINT_LIST) || (p->type==GF_PROP_VEC2I_LIST)
 		|| (p->type==GF_PROP_STRING_LIST)
	) {
		Bool is_array;
		u32 i, nb_items;
		size_t size;
		status = napi_is_array(env, prop, (bool *) &is_array);
		if (!is_array) {
			napi_throw_error(env, NULL, "Property value not an array");
			return napi_generic_failure;
		}
		//use uint list for nb_items
		status = napi_get_array_length(env, prop, &nb_items);
		p->value.uint_list.nb_items = nb_items;
		if (!p->value.uint_list.nb_items) {
			p->value.uint_list.vals = NULL;
			return status;
		}
		if (p->type==GF_PROP_STRING_LIST) size = sizeof(char *) * nb_items;
		else if (p->type==GF_PROP_VEC2I_LIST) size = sizeof(GF_PropVec2i) * nb_items;
		else size = sizeof(u32) * nb_items;

		p->value.uint_list.vals = gf_malloc(size);
		if (!p->value.uint_list.vals) {
			napi_throw_error(env, NULL, "Failed to allocate property");
			return napi_generic_failure;
		}
		memset(p->value.uint_list.vals, 0, size);

		for (i=0; i<nb_items; i++) {
			napi_value val;
			status = napi_get_element(env, prop, i, &val);
			if (status != napi_ok) return status;

			if (p->type==GF_PROP_STRING_LIST) {
				napi_valuetype vtype;
				napi_typeof(env, val, &vtype);
				if (vtype == napi_null) {
					p->value.string_list.vals[i] = NULL;
				} else {
					status = napi_get_value_string_utf8(env, val, NULL, 0, &size);
					if (status != napi_ok) return status;
					p->value.string_list.vals[i] = gf_malloc(sizeof(char) * (size+1) );
					if (!p->value.string_list.vals[i]) return napi_generic_failure;
					status = napi_get_value_string_utf8(env, val, p->value.string_list.vals[i], size+1, &size);
					if (status != napi_ok) return status;
					p->value.string_list.vals[i] [size]=0;
				}
			}
			else if (p->type==GF_PROP_VEC2I_LIST) {
				GET_PROP_FIELD_S32("x", "Vec2i", p->value.v2i_list.vals[i].x)
				GET_PROP_FIELD_S32("y", "Vec2i", p->value.v2i_list.vals[i].y)
			} else if (p->type==GF_PROP_SINT_LIST) {
				status = napi_get_value_int32(env, val, &p->value.sint_list.vals[i]);
				if (status != napi_ok) return status;
			} else if (p->type==GF_PROP_UINT_LIST) {
				status = napi_get_value_uint32(env, val, &p->value.uint_list.vals[i]);
				if (status != napi_ok) return status;
			} else if (p->type==GF_PROP_4CC_LIST) {
				status = napi_get_str(env, prop, &str);
				if (status != napi_ok) return status;
				if (str && (strlen(str)==4)) {
					p->value.uint_list.vals[i] = GF_4CC(str[0], str[1], str[2], str[3]);
				} else {
					napi_throw_error(env, NULL, "Invalid 4CC, needs 4 characters");
					return napi_generic_failure;
				}
				if (status != napi_ok) return status;
			}
		}
	}

	napi_throw_error(env, NULL, "Unsupported property type");
	return napi_generic_failure;
}

napi_value filter_enum_pid_props(napi_env env, napi_value cbk_fun, GF_FilterPid *pid)
{
	napi_value res, global;
	napi_status status;
	napi_valuetype rtype;
	napi_typeof(env, cbk_fun, &rtype);
	if (rtype != napi_function) {
		napi_throw_error(env, NULL, "property enumerator is not a function");
		return NULL;
	}

	NAPI_CALL( napi_get_global(env, &global) );
	u32 prop_idx=0;
	while (1) {
		napi_handle_scope scope_h;
		u32 p4cc=0;
		napi_value fun_args[3];
		const char *pname = NULL;
		const GF_PropertyValue *prop = gf_filter_pid_enum_properties(pid, &prop_idx, &p4cc, &pname);
		if (!prop) break;

		NAPI_CALL(napi_open_handle_scope(env, &scope_h) );

		if (p4cc) {
			pname = gf_props_4cc_get_name(p4cc);
			if (!pname) pname = gf_4cc_to_str(p4cc);
		}

		NAPI_CALL( napi_create_string_utf8(env, pname, NAPI_AUTO_LENGTH, &fun_args[0]) );
		fun_args[1] = prop_to_napi(env, prop, p4cc);
		NAPI_CALL( napi_create_int32(env, prop->type, &fun_args[2]) );

		NAPI_CALL(napi_call_function(env, global, cbk_fun, 3, fun_args, &res) );
		NAPI_CALL(napi_close_handle_scope(env, scope_h) );
	}
	return NULL;
}

napi_value filter_iopid_enum_props(napi_env env, napi_callback_info info, Bool is_opid)
{
	GF_FilterPid *pid=NULL;
	NARG_ARGS_THIS(2, 2)
	FILTER
	NARG_U32(idx, 0, 0)

	if (is_opid) {
		pid = gf_filter_get_opid(f, idx);
	} else {
		pid = gf_filter_get_ipid(f, idx);
	}
	if (!pid) {
		napi_throw_error(env, NULL, "Invalid PID index");
		return NULL;
	}
	return filter_enum_pid_props(env, argv[1], pid);
}

napi_value filter_ipid_enum_props(napi_env env, napi_callback_info info)
{
	return filter_iopid_enum_props(env, info, GF_FALSE);
}
napi_value filter_opid_enum_props(napi_env env, napi_callback_info info)
{
	return filter_iopid_enum_props(env, info, GF_TRUE);
}

napi_value filter_get_pid_property(napi_env env, char *pname, GF_FilterPid *pid, Bool is_info)
{
	napi_status status;
	napi_value res;
	GF_PropertyEntry *pe=NULL;
	u32 p4cc = gf_props_get_id(pname);
	const GF_PropertyValue *prop = NULL;

	if (is_info) {
		if (p4cc)
			prop = gf_filter_pid_get_info(pid, p4cc, &pe);
		else
			prop = gf_filter_pid_get_info_str(pid, pname, &pe);
	} else {
		if (p4cc)
			prop = gf_filter_pid_get_property(pid, p4cc);
		else
			prop = gf_filter_pid_get_property_str(pid, pname);
	}
	if (!prop) {
		napi_value res;
		NAPI_CALL(napi_get_null(env, &res) );
		return res;
	}
	res = prop_to_napi(env, prop, p4cc);
	gf_filter_release_property(pe);
	return res;
}

napi_value filter_iopid_prop(napi_env env, napi_callback_info info, Bool is_opid)
{
	GF_FilterPid *pid=NULL;
	NARG_ARGS_THIS(2, 2)
	FILTER
	NARG_U32(idx, 0, 0)
	NARG_STR(name, 1, 0)

	if (is_opid) {
		pid = gf_filter_get_opid(f, idx);
	} else {
		pid = gf_filter_get_ipid(f, idx);
	}
	if (!pid) {
		napi_throw_error(env, NULL, "Invalid PID index");
		return NULL;
	}
	return filter_get_pid_property(env, name, pid, GF_FALSE);
}
napi_value filter_ipid_prop(napi_env env, napi_callback_info info)
{
	return filter_iopid_prop(env, info, GF_FALSE);
}
napi_value filter_opid_prop(napi_env env, napi_callback_info info)
{
	return filter_iopid_prop(env, info, GF_TRUE);
}

napi_value filter_iopid_stats(napi_env env, napi_callback_info info, Bool is_opid)
{
	GF_Err e;
	napi_value res, val;
	GF_FilterPidStatistics stats;
	GF_FilterPid *pid=NULL;
	NARG_ARGS_THIS(2, 1)
	FILTER
	NARG_U32(idx, 0, 0)
	NARG_U32(mode, 1, 0)

	if (is_opid) {
		pid = gf_filter_get_opid(f, idx);
	} else {
		pid = gf_filter_get_ipid(f, idx);
	}
	if (!pid) {
		napi_throw_error(env, NULL, "Invalid PID index");
		return NULL;
	}

	e = gf_filter_pid_get_statistics(pid, &stats, mode);
 	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to get filter stats");
		return NULL;
	}
	NAPI_CALL(napi_create_object(env, &res) );
	SET_BOOL(disconnected)
	SET_U32(average_process_rate)
	SET_U32(max_process_rate)
	SET_U32(average_bitrate)
	SET_U32(max_bitrate)
	SET_U32(nb_processed)
	SET_U32(max_process_time)
	SET_U64(total_process_time)
	SET_U64(first_process_time)
	SET_U64(last_process_time)
	SET_U32(min_frame_dur)
	SET_U32(nb_saps)
	SET_U32(max_sap_process_time)
	SET_U64(total_sap_process_time)
	SET_U64(max_buffer_time)
	SET_U64(max_playout_time)
	SET_U64(min_playout_time)
	SET_U64(buffer_time)
	SET_U32(nb_buffer_units)
	SET_U64(last_rt_report)
	SET_U32(rtt)
	SET_U32(jitter)
	SET_U32(loss_rate)
	NAPI_CALL(napi_set_named_property(env, res, "last_ts_sent", frac64_to_napi(env, &stats.last_ts_sent)) );
	NAPI_CALL(napi_set_named_property(env, res, "last_ts_drop", frac64_to_napi(env, &stats.last_ts_drop)) );
	return res;
}
napi_value filter_ipid_stats(napi_env env, napi_callback_info info)
{
	return filter_iopid_stats(env, info, GF_FALSE);
}
napi_value filter_opid_stats(napi_env env, napi_callback_info info)
{
	return filter_iopid_stats(env, info, GF_TRUE);
}

napi_value filter_ipid_source(napi_env env, napi_callback_info info)
{
	GF_FilterPid *pid=NULL;
	NARG_ARGS_THIS(1, 1)
	FILTER
	NARG_U32(idx, 0, 0)

	pid = gf_filter_get_ipid(f, idx);
	if (!pid) {
		napi_throw_error(env, NULL, "Invalid PID index");
		return NULL;
	}
	GF_Filter *f_src = gf_filter_pid_get_source_filter(pid);

	NAPI_Filter *napi_f = (NAPI_Filter *) gf_filter_get_rt_udta(f);
	return fs_wrap_filter(env, napi_f->fs, f_src);
}

napi_value filter_opid_sinks(napi_env env, napi_callback_info info)
{
	napi_value res;
	u32 i;
	GF_FilterPid *pid=NULL;
	NARG_ARGS_THIS(1, 1)
	FILTER
	NARG_U32(idx, 0, 0)

	pid = gf_filter_get_opid(f, idx);
	if (!pid) {
		napi_throw_error(env, NULL, "Invalid PID index");
		return NULL;
	}
	NAPI_CALL(napi_create_array(env, &res) );

	NAPI_Filter *napi_f = (NAPI_Filter *) gf_filter_get_rt_udta(f);
	i=0;
	while (1) {
		GF_Filter *f_dst = gf_filter_pid_enum_destinations(pid, i);
		if (!f_dst) break;

		NAPI_CALL(napi_set_element(env, res, i, fs_wrap_filter(env, napi_f->fs, f_dst) ) );
		i++;
	}
	return res;
}

napi_value filter_get_info(napi_env env, napi_callback_info info)
{
	napi_value res;
	u32 p4cc;
	NARG_ARGS_THIS(1, 1)
	FILTER
	NARG_STR(prop_name, 0, NULL);

	const GF_PropertyValue *prop;
	GF_PropertyEntry *pe=NULL;
	p4cc =gf_props_get_id(prop_name);
	if (p4cc) {
		prop = gf_filter_get_info(f, p4cc, &pe);
	} else {
		prop = gf_filter_get_info_str(f, prop_name, &pe);
	}
	if (prop) {
		res = prop_to_napi(env, prop, p4cc);
	} else {
		NAPI_CALL( napi_get_null(env, &res) );
	}
	gf_filter_release_property(pe);
	return res;
}

napi_value filter_all_args(napi_env env, napi_callback_info info)
{
	napi_value res;
	u32 a_idx;
	NARG_THIS
	FILTER

	NAPI_CALL(napi_create_array(env, &res) );
	a_idx=0;
	while (1) {
		napi_value n_arg, val;
		const GF_FilterArgs *arg = gf_filter_enumerate_args(f, a_idx);
		if (!arg) break;

		NAPI_CALL(napi_create_object(env, &n_arg) );

		NAPI_CALL( napi_create_string_utf8(env, arg->arg_name, NAPI_AUTO_LENGTH, &val) );
		NAPI_CALL(napi_set_named_property(env, n_arg, "name", val) );
		NAPI_CALL( napi_create_string_utf8(env, arg->arg_desc, NAPI_AUTO_LENGTH, &val) );
		NAPI_CALL(napi_set_named_property(env, n_arg, "desc", val) );
		NAPI_CALL( napi_create_int32(env, arg->arg_type, &val) );
		NAPI_CALL(napi_set_named_property(env, n_arg, "type", val) );

		if (arg->arg_default_val) {
			NAPI_CALL( napi_create_string_utf8(env, arg->arg_default_val, NAPI_AUTO_LENGTH, &val) );
		} else {
			NAPI_CALL( napi_get_null(env, &val) );
		}
		NAPI_CALL(napi_set_named_property(env, n_arg, "default", val) );

		if (arg->min_max_enum) {
			NAPI_CALL( napi_create_string_utf8(env, arg->min_max_enum, NAPI_AUTO_LENGTH, &val) );
		} else {
			NAPI_CALL( napi_get_null(env, &val) );
		}
		NAPI_CALL(napi_set_named_property(env, n_arg, "min_max_enum", val) );

		NAPI_CALL( napi_create_int32(env, arg->flags, &val) );
		NAPI_CALL(napi_set_named_property(env, n_arg, "flags", val) );

		NAPI_CALL(napi_set_element(env, res, a_idx, n_arg) );
		a_idx++;
	}
	return res;
}

/* custom filters*/
static GF_Err napi_filter_cbk_process(GF_Filter *filter)
{
	napi_status status;
	napi_value obj, fun_val, res;
	NAPI_Filter *napi_f = gf_filter_get_rt_udta(filter);
	if (!napi_f) return GF_SERVICE_ERROR;

	napi_get_reference_value(napi_f->env, napi_f->ref, &obj);
	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_f->env, obj, "process", (bool *) &has_fun);
	if (!has_fun) return GF_EOS;

	status = napi_get_named_property(napi_f->env, obj, "process", &fun_val);
	if (status != napi_ok) return GF_BAD_PARAM;

	status = napi_make_callback(napi_f->env, napi_f->async_ctx, obj, fun_val, 0, NULL, &res);

	GF_Err e = GF_OK;
	napi_get_value_int32(napi_f->env, res, &e);
	return e;
}

void filter_inputpid_finalize(napi_env env, void* finalize_data, void* finalize_hint)
{
	//finalize_hint contains napi_pid
	NAPI_FilterPid *napi_pid = (NAPI_FilterPid *)finalize_hint;
	gf_free(napi_pid);
}

static NAPI_FilterPid *wrap_filter_pid(napi_env env, GF_Filter *filter, GF_FilterPid *pid, Bool is_input)
{
	NAPI_FilterPid *napi_pid;
	napi_value pid_obj;

	GF_SAFEALLOC(napi_pid, NAPI_FilterPid);
	if (!napi_pid) {
		return NULL;
	}
	gf_filter_pid_set_udta(pid, napi_pid);
	napi_pid->f = filter;
	napi_pid->pid = pid;

	napi_create_object(env, &pid_obj);
	napi_create_reference(env, pid_obj, 1, &napi_pid->ref);
	napi_type_tag_object(env, pid_obj, &filterpid_tag);

	napi_wrap(env, pid_obj, pid, is_input ? filter_inputpid_finalize : dummy_finalize, napi_pid, NULL);

	//define all props for custom filter
	napi_property_descriptor filterpid_properties[] = {
		{ "name", NULL, NULL, filterpid_getter, filterpid_setter, NULL, napi_enumerable, &FS_PROP_PID_NAME},
		{ "filter_name", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_FILTERNAME},
		{ "eos", NULL, NULL, filterpid_getter, filterpid_setter, NULL, napi_enumerable, &FS_PROP_PID_EOS},
		{ "has_seen_eos", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_HAS_SEEN_EOS},
		{ "eos_received", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_HAS_RECEIVED},
		{ "would_block", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_WOULD_BLOCK},
		{ "sparse", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_SPARSE},
		{ "max_buffer", NULL, NULL, filterpid_getter, filterpid_setter, NULL, napi_enumerable, &FS_PROP_PID_MAX_BUFFER},
		{ "buffer", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_BUFFER},
		{ "buffer_full", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_BUFFER_FULL},
		{ "first_empty", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_FIRST_EMPTY},
		{ "first_cts", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_FIRST_CTS},
		{ "nb_pck_queued", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_NB_PCK_QUEUED},
		{ "timescale", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_TIMESCALE},
		{ "min_pck_dur", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_MIN_PCK_DUR},
		{ "playing", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_PLAYING},
		{ "next_ts", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_NEXT_TS},
		{ "has_decoder", NULL, NULL, filterpid_getter, NULL, NULL, napi_enumerable, &FS_PROP_PID_HAS_DECODER},

		{ "remove", 0, filterpid_remove, 0, 0, 0, napi_enumerable, 0 },
		{ "enum_props", 0, filterpid_enum_props, 0, 0, 0, napi_enumerable, 0 },
		{ "get_prop", 0, filterpid_get_prop, 0, 0, 0, napi_enumerable, 0 },
		{ "get_info", 0, filterpid_get_info, 0, 0, 0, napi_enumerable, 0 },
		{ "copy_props", 0, filterpid_copy_props, 0, 0, 0, napi_enumerable, 0 },
		{ "reset_props", 0, filterpid_reset_props, 0, 0, 0, napi_enumerable, 0 },
		{ "set_prop", 0, filterpid_set_prop, 0, 0, 0, napi_enumerable, 0 },
		{ "set_info", 0, filterpid_set_info, 0, 0, 0, napi_enumerable, 0 },
		{ "clear_eos", 0, filterpid_clear_eos, 0, 0, 0, napi_enumerable, 0 },
		{ "check_caps", 0, filterpid_check_caps, 0, 0, 0, napi_enumerable, 0 },
		{ "discard_block", 0, filterpid_discard_block, 0, 0, 0, napi_enumerable, 0 },
		{ "allow_direct_dispatch", 0, filterpid_allow_direct_dispatch, 0, 0, 0, napi_enumerable, 0 },
		{ "get_clock_type", 0, filterpid_get_clock_type, 0, 0, 0, napi_enumerable, 0 },
		{ "get_clock_timestamp", 0, filterpid_get_clock_timestamp, 0, 0, 0, napi_enumerable, 0 },
		{ "is_filter_in_parents", 0, filterpid_is_filter_in_parents, 0, 0, 0, napi_enumerable, 0 },
		{ "get_buffer_occupancy", 0, filterpid_get_buffer_occupancy, 0, 0, 0, napi_enumerable, 0 },
		{ "loose_connect", 0, filterpid_loose_connect, 0, 0, 0, napi_enumerable, 0 },
		{ "set_framing", 0, filterpid_set_framing, 0, 0, 0, napi_enumerable, 0 },
		{ "set_clock_mode", 0, filterpid_set_clock_mode, 0, 0, 0, napi_enumerable, 0 },
		{ "set_discard", 0, filterpid_set_discard, 0, 0, 0, napi_enumerable, 0 },
		{ "require_source_id", 0, filterpid_require_source_id, 0, 0, 0, napi_enumerable, 0 },
		{ "recompute_dts", 0, filterpid_recompute_dts, 0, 0, 0, napi_enumerable, 0 },
		{ "resolve_template", 0, filterpid_resolve_template, 0, 0, 0, napi_enumerable, 0 },
		{ "query_cap", 0, filterpid_query_cap, 0, 0, 0, napi_enumerable, 0 },
		{ "negotiate_cap", 0, filterpid_negotiate_cap, 0, 0, 0, napi_enumerable, 0 },
		{ "get_packet", 0, filterpid_get_packet, 0, 0, 0, napi_enumerable, 0 },
		{ "drop_packet", 0, filterpid_drop_packet, 0, 0, 0, napi_enumerable, 0 },
		{ "send_event", 0, filterpid_send_event, 0, 0, 0, napi_enumerable, 0 },
		{ "forward", 0, filterpid_forward, 0, 0, 0, napi_enumerable, 0 },
		{ "new_pck", 0, filterpid_new_pck, 0, 0, 0, napi_enumerable, 0 },
		{ "new_pck_ref", 0, filterpid_new_pck_ref, 0, 0, 0, napi_enumerable, 0 },
		{ "new_pck_shared", 0, filterpid_new_pck_shared, 0, 0, 0, napi_enumerable, 0 },
		{ "new_pck_copy", 0, filterpid_new_pck_copy, 0, 0, 0, napi_enumerable, 0 },
		{ "new_pck_clone", 0, filterpid_new_pck_clone, 0, 0, 0, napi_enumerable, 0 },
	};

	napi_define_properties(env, pid_obj, sizeof(filterpid_properties)/sizeof(napi_property_descriptor), filterpid_properties);

	return napi_pid;
}
static GF_Err napi_filter_cbk_configure(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Err e=GF_OK;
	napi_value args[2];
	napi_status status;
	napi_value obj, fun_val, res;
	NAPI_Filter *napi_f = gf_filter_get_rt_udta(filter);
	Bool has_fun = GF_FALSE;
	if (!napi_f) return GF_SERVICE_ERROR;

	napi_get_reference_value(napi_f->env, napi_f->ref, &obj);

	NAPI_FilterPid *napi_pid = gf_filter_pid_get_udta(pid);

	napi_has_named_property(napi_f->env, obj, "configure_pid", (bool *) &has_fun);
	if (has_fun) {
		status = napi_get_named_property(napi_f->env, obj, "configure_pid", &fun_val);
		if (status != napi_ok) has_fun = GF_FALSE;
	}

	if (is_remove) {
		if (napi_pid) {
			if (has_fun) {
				napi_get_reference_value(napi_f->env, napi_pid->ref, &args[0]);
				napi_get_boolean(napi_f->env, (bool) 1, &args[1]);
				status = napi_make_callback(napi_f->env, napi_f->async_ctx, obj, fun_val, 2, args, &res);
				status = napi_get_value_int32(napi_f->env, res, &e);
			}
			u32 rcount;
			napi_reference_unref(napi_f->env, napi_pid->ref, &rcount);
			napi_delete_reference(napi_f->env, napi_pid->ref);
			napi_pid->pid = NULL;
			gf_filter_pid_set_udta(pid, NULL);
		}
		return e;
	}
	if (!napi_pid) {
		napi_pid = wrap_filter_pid(napi_f->env, filter, pid, GF_TRUE);
		if (!napi_pid) return GF_OUT_OF_MEM;
	}

	if (has_fun) {
		napi_get_reference_value(napi_f->env, napi_pid->ref, &args[0]);
		napi_get_boolean(napi_f->env, (bool) 0, &args[1]);
		status = napi_make_callback(napi_f->env, napi_f->async_ctx, obj, fun_val, 2, args, &res);
		status = napi_get_value_int32(napi_f->env, res, &e);
	}

	return e;
}
static GF_Err napi_filter_cbk_reconfigure_output(GF_Filter *filter, GF_FilterPid *pid)
{
	GF_Err e=GF_OK;
	napi_value args[1];
	napi_status status;
	napi_value obj, fun_val, res;
	NAPI_Filter *napi_f = gf_filter_get_rt_udta(filter);
	Bool has_fun = GF_FALSE;
	if (!napi_f) return GF_SERVICE_ERROR;

	napi_get_reference_value(napi_f->env, napi_f->ref, &obj);

	NAPI_FilterPid *napi_pid = gf_filter_pid_get_udta(pid);
	if (!napi_pid) return GF_SERVICE_ERROR;

	napi_has_named_property(napi_f->env, obj, "reconfigure_output", (bool *) &has_fun);
	if (has_fun) {
		status = napi_get_named_property(napi_f->env, obj, "reconfigure_output", &fun_val);
		if (status != napi_ok) has_fun = GF_FALSE;
	}
	//by default return not supported
	e = GF_NOT_SUPPORTED;
	if (has_fun) {
		napi_get_reference_value(napi_f->env, napi_pid->ref, &args[0]);
		status = napi_make_callback(napi_f->env, napi_f->async_ctx, obj, fun_val, 1, args, &res);
		status = napi_get_value_int32(napi_f->env, res, &e);
	}
	return e;
}

static Bool napi_filter_cbk_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	napi_status status;
	napi_value obj, fun_val, arg, res;
	NAPI_Filter *napi_f = gf_filter_get_rt_udta(filter);
	if (!napi_f) return GF_FALSE;

	napi_get_reference_value(napi_f->env, napi_f->ref, &obj);
	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_f->env, obj, "process_event", (bool *) &has_fun);
	if (!has_fun) return GF_FALSE;

	status = napi_get_named_property(napi_f->env, obj, "process_event", &fun_val);
	if (status != napi_ok) return GF_BAD_PARAM;

	arg = wrap_filterevent(napi_f->env, (GF_FilterEvent *)evt, NULL);
	status = napi_make_callback(napi_f->env, napi_f->async_ctx, obj, fun_val, 1, &arg, &res);

	bool ret = GF_FALSE;
	napi_get_value_bool(napi_f->env, res, &ret);
	return ret ? GF_TRUE : GF_FALSE;
}

static u32 FS_PROP_CUST_BLOCK_ENABLED = 0;
static u32 FS_PROP_CUST_OUTPUT_BUFFER = 1;
static u32 FS_PROP_CUST_PLAYOUT_BUFFER = 2;
static u32 FS_PROP_CUST_SINKS_DONE = 3;
static u32 FS_PROP_CUST_EVTS_QUEUED = 4;
static u32 FS_PROP_CUST_CK_HINT_TIME = 5;
static u32 FS_PROP_CUST_CK_HINT_MEDIATIME = 6;
static u32 FS_PROP_CUST_CONNECTIONS_PENDING = 7;

napi_value filter_cust_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILTER
	FILTER_CUST

	if (magic == &FS_PROP_CUST_BLOCK_ENABLED) {
		NAPI_CALL( napi_get_boolean(env, (bool) gf_filter_block_enabled(f), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_CUST_OUTPUT_BUFFER) {
		u32 val;
		gf_filter_get_output_buffer_max(f, &val, NULL);
		NAPI_CALL( napi_create_uint32(env, val, &ret) );
		return ret;
	}
	if (magic == &FS_PROP_CUST_PLAYOUT_BUFFER) {
		u32 val;
		gf_filter_get_output_buffer_max(f, NULL, &val);
		NAPI_CALL( napi_create_uint32(env, val, &ret) );
		return ret;
	}
	if (magic == &FS_PROP_CUST_SINKS_DONE) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_all_sinks_done(f), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_CUST_EVTS_QUEUED) {
		NAPI_CALL( napi_create_uint32(env, gf_filter_get_num_events_queued(f), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_CUST_CK_HINT_TIME) {
        u64 val;
        gf_filter_get_clock_hint(f, &val, NULL);
		NAPI_CALL( napi_create_int64(env, val, &ret) );
		return ret;
	}
	if (magic == &FS_PROP_CUST_CK_HINT_MEDIATIME) {
        GF_Fraction64 val;
        gf_filter_get_clock_hint(f, NULL, &val);
        ret = frac64_to_napi(env, &val);
		return ret;
	}
	if (magic == &FS_PROP_CUST_CONNECTIONS_PENDING) {
		NAPI_CALL( napi_get_boolean(env, gf_filter_connections_pending(f), &ret) );
		return ret;
	}
	return NULL;
}

napi_value filter_push_cap(napi_env env, napi_callback_info info)
{
	GF_Err e;
	char *pname=NULL;
	u32 p4cc=0;
	GF_PropertyValue prop;
	NARG_ARGS_THIS(5, 3)
	FILTER
	FILTER_CUST

	napi_valuetype rtype;
	napi_typeof(env, argv[0], &rtype);
	if (rtype == napi_string) {
		NARG_STR(prop_name, 0, NULL)
		pname = prop_name;
		p4cc = gf_props_get_id(pname);
	} else {
		NARG_U32(pcode, 0, 0)
		p4cc = pcode;
	}
	NARG_U32(flags, 2, 0)
	NARG_U32(priority, 3, 0)
	NARG_U32(custom_type, 4, 0)

	if (p4cc) pname = NULL;

	//clone prop name as prop from napi may reuse the global arg buffer
	if (pname) pname = gf_strdup(pname);
	//convert napi_prop to prop
	status = prop_from_napi(env, argv[1], p4cc, pname, custom_type, &prop);
	if (status == napi_ok) {
		e = gf_filter_push_caps(f, p4cc, &prop, pname, flags, priority);
	} else {
		e = GF_BAD_PARAM;
	}
	if (pname) gf_free(pname);
	//do not reset property, it is now owned by filter

 	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Error assigning cap");
	}
	return NULL;
}

napi_value filter_update_status(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(2, 1)
	FILTER
	FILTER_CUST

	NARG_STR(fstatus, 0, NULL)
	NARG_U32(percent, 1, 0)
	gf_filter_update_status(f, percent, fstatus);
	return NULL;
}

napi_value filter_reschedule(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 0)
	FILTER
	FILTER_CUST
	NARG_U32(when, 0, 0)

	if (when)
		gf_filter_ask_rt_reschedule(f, when);
	else
		gf_filter_post_process_task(f);
	return NULL;
}

napi_value filter_notify_failure(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(2, 1)
	FILTER
	FILTER_CUST
	NARG_S32(err, 0, 0)
	NARG_U32(type, 1, GF_SETUP_ERROR)

	if (type==GF_NOTIF_ERROR)
		gf_filter_notification_failure(f, err, GF_FALSE);
	else if (type==GF_NOTIF_ERROR_AND_DISCONNECT)
		gf_filter_notification_failure(f, err, GF_TRUE);
	else
		gf_filter_setup_failure(f, err);
	return NULL;
}


napi_value filter_make_sticky(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTER
	FILTER_CUST
	gf_filter_make_sticky(f);
	return NULL;
}

napi_value filter_prevent_blocking(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTER
	FILTER_CUST
	NARG_BOOL(enable, 0, 0)
	gf_filter_prevent_blocking(f, enable);
	return NULL;
}
napi_value filter_block_eos(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTER
	FILTER_CUST
	NARG_BOOL(enable, 0, 0)
	gf_filter_block_eos(f, enable);
	return NULL;
}
napi_value filter_set_max_pids(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTER
	FILTER_CUST
	NARG_U32(max_extra_pids, 0, 0)
	gf_filter_set_max_extra_input_pids(f, max_extra_pids);
	return NULL;
}
napi_value filter_hint_clock(napi_env env, napi_callback_info info)
{
	GF_Fraction64 frac;
	NARG_ARGS_THIS(2, 2)
	FILTER
	FILTER_CUST
	NARG_U64(clock_us, 0, 0)
	NAPI_CALL(frac64_from_napi(env, argv[1], &frac) );

	gf_filter_hint_single_clock(f, clock_us, frac);
	return NULL;
}

napi_value filter_new_pid(napi_env env, napi_callback_info info)
{
	napi_value ret;
	NARG_THIS
	FILTER
	FILTER_CUST

	GF_FilterPid *opid = gf_filter_pid_new(f);
	if (!opid) {
		napi_throw_error(env, NULL, "Failed to create new output PID");
		return NULL;
	}

	NAPI_FilterPid *napi_pid = wrap_filter_pid(env, f, opid, GF_FALSE);
	if (!napi_pid) {
		napi_throw_error(env, NULL, "Failed to create new output PID");
		return NULL;
	}
	napi_get_reference_value(env, napi_pid->ref, &ret);
	return ret;
}

napi_value filter_print_connections(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTER
	FILTER_CUST
	gf_filter_print_all_connections(f, NULL);
	return NULL;
}


#define FILTER_SESSION\
	GF_FilterSession *fs;\
	bool _tag;\
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &fs) );\
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &filtersession_tag, &_tag) );\
	if (!_tag) {\
		napi_throw_error(env, NULL, "Not a FilterSession object");\
		return NULL;\
	}\


static u32 FS_PROP_LAST_TASK = 0;
static u32 FS_PROP_NB_FILTERS = 1;
static u32 FS_PROP_HTTP_RATE = 2;
static u32 FS_PROP_MAX_HTTP_RATE = 3;

napi_value fs_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILTER_SESSION

	if (magic == &FS_PROP_LAST_TASK) {
		NAPI_CALL( napi_get_boolean(env, (bool) gf_fs_is_last_task(fs), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_NB_FILTERS) {
		NAPI_CALL( napi_create_uint32(env, gf_fs_get_filters_count(fs), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_HTTP_RATE) {
		NAPI_CALL( napi_create_uint32(env, gf_fs_get_http_rate(fs), &ret) );
		return ret;
	}
	if (magic == &FS_PROP_MAX_HTTP_RATE) {
		NAPI_CALL( napi_create_uint32(env, gf_fs_get_http_max_rate(fs), &ret) );
		return ret;
	}
	return NULL;
}

napi_value fs_setter(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS_MAGIC(1, 1)
	FILTER_SESSION

	if (magic == &FS_PROP_MAX_HTTP_RATE) {
		NARG_U32(new_rate, 0, 0);
		gf_fs_set_http_max_rate(fs, new_rate);
		return NULL;
	}
	return NULL;
}

napi_value fs_run(napi_env env, napi_callback_info info)
{
	GF_Err e;
	NARG_THIS
	FILTER_SESSION
	NAPI_GPAC
	NAPI_Session *napi_fs = gf_fs_get_rt_udta(fs);

	if (napi_fs->blocking) {
		//session is running in blocking mode, schedule task to call NodeJS from main thread
		if (gpac->rmt_ref) {
			gpac->fs_rmt_handler = napi_fs;
			gpac->rmt_task_scheduled = GF_TRUE;
			gf_fs_post_user_task(fs, fs_flush_rmt, napi_fs, "RemoteryFlush");
		}
		e = gf_fs_run(fs);

		gpac->fs_rmt_handler = NULL;
	} else {
		e = gf_fs_run(fs);

		//flush rmt events
		if (gpac->rmt_ref)
			gpac_rmt_callback_exec(env, gpac);
	}

	if (e>=GF_OK) {
		e = gf_fs_get_last_connect_error(fs);
		if (e>=GF_OK)
			e = gf_fs_get_last_process_error(fs);
	}
	if (e<0)
		napi_throw_error(env, gf_error_to_string(e), "Error running session");\


	while (gf_list_count(napi_fs->defer_filters)) {
		GF_Filter *filter = gf_list_pop_front(napi_fs->defer_filters);
		const char *fname = "on_filter_new";
		Bool has_fun = GF_FALSE;
		napi_has_named_property(env, this_val, fname, (bool *) &has_fun);
		if (has_fun) {
			napi_value fun_val;
			status = napi_get_named_property(env, this_val, fname, &fun_val);
			if (status == napi_ok) {
				napi_value res, arg = fs_wrap_filter(env, napi_fs->fs, filter);
				status = napi_make_callback(env, napi_fs->async_ctx, this_val, fun_val, 1, &arg, &res);
			}
		}
	}
	return NULL;
}

napi_value fs_wrap_filter(napi_env env, GF_FilterSession *fs, GF_Filter *filter)
{
	napi_status status;
	napi_value obj;
	NAPI_Filter *napi_f = gf_filter_get_rt_udta(filter);

	if (!filter) {
		NAPI_CALL( napi_get_null(env, &obj) );
		return obj;
	}

	napi_property_descriptor filter_properties[] = {
		{ "name", NULL, NULL, filter_getter, filter_setter, NULL, napi_enumerable, &FILTER_PROP_NAME},
		{ "ID", NULL, NULL, filter_getter, NULL, NULL, napi_enumerable, &FILTER_PROP_ID},
		{ "nb_ipid", NULL, NULL, filter_getter, NULL, NULL, napi_enumerable, &FILTER_PROP_IPIDS},
		{ "nb_opid", NULL, NULL, filter_getter, NULL, NULL, napi_enumerable, &FILTER_PROP_OPIDS},
		{ "remove", 0, filter_remove, 0, 0, 0, napi_enumerable, 0 },
		{ "update", 0, filter_update, 0, 0, 0, napi_enumerable, 0 },
		{ "set_source", 0, filter_set_source, 0, 0, 0, napi_enumerable, 0 },
		{ "set_source_restricted", 0, filter_set_source_restricted, 0, 0, 0, napi_enumerable, 0 },
		{ "insert", 0, filter_insert, 0, 0, 0, napi_enumerable, 0 },
		{ "reconnect", 0, filter_reconnect, 0, 0, 0, napi_enumerable, 0 },
		{ "ipid_prop", 0, filter_ipid_prop, 0, 0, 0, napi_enumerable, 0 },
		{ "ipid_enum_props", 0, filter_ipid_enum_props, 0, 0, 0, napi_enumerable, 0 },
		{ "opid_prop", 0, filter_opid_prop, 0, 0, 0, napi_enumerable, 0 },
		{ "opid_enum_props", 0, filter_opid_enum_props, 0, 0, 0, napi_enumerable, 0 },
		{ "ipid_source", 0, filter_ipid_source, 0, 0, 0, napi_enumerable, 0 },
		{ "opid_sinks", 0, filter_opid_sinks, 0, 0, 0, napi_enumerable, 0 },
		{ "all_args", 0, filter_all_args, 0, 0, 0, napi_enumerable, 0 },
		{ "get_info", 0, filter_get_info, 0, 0, 0, napi_enumerable, 0 },
		{ "get_statistics", 0, filter_get_statistics, 0, 0, 0, napi_enumerable, 0 },
		{ "require_source_id", 0, filter_require_source_id, 0, 0, 0, napi_enumerable, 0 },
		{ "bind", 0, filter_bind, 0, 0, 0, napi_enumerable, 0 },
		{ "ipid_stats", 0, filter_ipid_stats, 0, 0, 0, napi_enumerable, 0 },
		{ "opid_stats", 0, filter_opid_stats, 0, 0, 0, napi_enumerable, 0 },
		{ "probe_link", 0, filter_probe_link, 0, 0, 0, napi_enumerable, 0 },
		{ "get_destinations", 0, filter_get_destinations, 0, 0, 0, napi_enumerable, 0 },
	};

	if (napi_f) {
		NAPI_CALL ( napi_get_reference_value(env, napi_f->ref, &obj) );
		return obj;
	}

	GF_SAFEALLOC(napi_f, NAPI_Filter);
	napi_f->f = filter;
	napi_f->fs = fs;
	napi_f->env = env;

	NAPI_CALL( napi_create_object(env, &obj) );
	NAPI_CALL( napi_define_properties(env, obj, sizeof(filter_properties)/sizeof(napi_property_descriptor), filter_properties) );
	NAPI_CALL( napi_create_reference(env, obj, 1, &napi_f->ref) );
	NAPI_CALL( napi_type_tag_object(env, obj, &filter_tag) );

	NAPI_CALL( napi_wrap(env, obj, filter, dummy_finalize, NULL, NULL) );

	gf_filter_set_rt_udta(filter, napi_f);
	return obj;
}


napi_value fs_load_src_dst(napi_env env, napi_callback_info info, Bool is_source)
{
	GF_Err e;
	NARG_ARGS_THIS(2, 1)
	FILTER_SESSION
	NARG_STR(url, 0, NULL)

	if (!url) {
		napi_throw_error(env, NULL, "Missing URL");
		return NULL;
	}
	NARG_STR_ALLOC(parent_url, 1, NULL);

	GF_Filter *filter;
	if (is_source)
		filter = gf_fs_load_source(fs, url, NULL, parent_url, &e);
	else
		filter = gf_fs_load_destination(fs, url, NULL, parent_url, &e);

	if (parent_url) gf_free(parent_url);

	if (e<0)
		napi_throw_error(env, gf_error_to_string(e), is_source ? "Failed to load source filter" : "Failed to load destination filter");

	return fs_wrap_filter(env, fs, filter);
}

napi_value fs_load_src(napi_env env, napi_callback_info info)
{
	return fs_load_src_dst(env, info, GF_TRUE);
}
napi_value fs_load_dst(napi_env env, napi_callback_info info)
{
	return fs_load_src_dst(env, info, GF_FALSE);
}

napi_value fs_load_filter(napi_env env, napi_callback_info info)
{
	GF_Err e;
	NARG_ARGS_THIS(1, 1)
	FILTER_SESSION
	NARG_STR(url, 0, NULL)

	GF_Filter *filter = gf_fs_load_filter(fs, url, &e);

	if (e<0)
		napi_throw_error(env, gf_error_to_string(e), "Failed to load filter");

	return fs_wrap_filter(env, fs, filter);
}

typedef struct
{
	napi_async_context ctx;
	napi_ref ref;
	napi_env env;
	NAPI_Session *napi_fs;
} NAPI_Task;

Bool fs_task_exec(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	Bool ret = GF_TRUE;
	napi_status status;
	napi_value res, obj_val, fun_val;
	NAPI_Task *task = (NAPI_Task *) callback;
	napi_env env = (napi_env) task->env;

	status = napi_get_reference_value(env, task->ref, &obj_val);
	if (status != napi_ok) goto exit;
	status = napi_get_named_property(env, obj_val, "execute", &fun_val);
	if (status != napi_ok) goto exit;

	status = napi_make_callback(env, task->napi_fs->async_ctx, obj_val, fun_val, 0, NULL, &res);
	if (status != napi_ok) goto exit;

	napi_valuetype rtype;
	napi_typeof(env, res, &rtype);
	if (rtype == napi_boolean) {
		bool val;
		status = napi_get_value_bool(env, res, &val);
		ret = val ? GF_TRUE : GF_FALSE;
	} else if ((rtype==napi_number) || (rtype==napi_bigint)) {
		status = napi_get_value_uint32(env, res, reschedule_ms);
		if (status == napi_ok)
			ret = GF_TRUE;
	} else {
		ret = GF_FALSE;
	}

exit:
	if (status != napi_ok) {
		napi_throw_error(env, NULL, "Failed to execute task");
	}

	if (gf_fs_in_final_flush(fsess))
		ret = GF_FALSE;

	if (!ret) {
		u32 rcount;
		napi_reference_unref(env, task->ref, &rcount);
		napi_delete_reference(env, task->ref);
		gf_free(task);
	}
	return ret;
}

napi_value fs_post_task(napi_env env, napi_callback_info info)
{
	napi_value global, fun_name, fun_val;
	NARG_ARGS_THIS(2, 1)
	FILTER_SESSION
	NARG_STR(log_name, 1, "NapiTask");

	NAPI_CALL( napi_create_string_utf8(env, "execute", NAPI_AUTO_LENGTH, &fun_name) );
	status = napi_get_property(env, argv[0], fun_name, &fun_val);
	if (status != napi_ok) {
		napi_throw_error(env, NULL, "No execute method on object");
		return NULL;
	}
	napi_valuetype rtype;
	napi_typeof(env, fun_val, &rtype);
	if (rtype != napi_function) {
		napi_throw_error(env, NULL, "property execute is not a function");
		return NULL;
	}

	NAPI_Task *task;
	GF_SAFEALLOC(task, NAPI_Task);
	if (!task) {
		napi_throw_error(env, NULL, "Failed to allocate task");
		return NULL;
	}
	task->napi_fs = gf_fs_get_rt_udta(fs);

	NAPI_CALL( napi_get_global(env, &global) );
	NAPI_CALL( napi_create_string_utf8(env, "TaskExecute", NAPI_AUTO_LENGTH, &fun_name) );

	status = napi_create_reference(env, argv[0], 1, &task->ref);
	task->env = env;
	gf_fs_post_user_task(fs, fs_task_exec, task, log_name);

	return NULL;
}

napi_value fs_abort(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 0)
	FILTER_SESSION
	NARG_U32(flush, 0, 0);
	gf_fs_abort(fs, flush);
	return NULL;
}

napi_value fs_lock(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTER_SESSION
	NARG_BOOL(lock, 0, 0);
	gf_fs_lock_filters(fs, lock);
	return NULL;
}
napi_value fs_reporting(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTER_SESSION
	NARG_BOOL(do_report, 0, 0);
	gf_fs_enable_reporting(fs, do_report);
	return NULL;
}

napi_value fs_print_stats(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTER_SESSION
	gf_fs_print_stats(fs);
	return NULL;
}

napi_value fs_print_graph(napi_env env, napi_callback_info info)
{
	NARG_THIS
	FILTER_SESSION
	gf_fs_print_connections(fs);
	return NULL;
}
napi_value fs_get_filter(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS(1, 1)
	FILTER_SESSION
	NARG_U32(fidx, 0, 0);

	GF_Filter *f = gf_fs_get_filter(fs, fidx);
	if (!f) {
		napi_throw_error(env, NULL, "Invalid filter index");
		return NULL;
	}
	return fs_wrap_filter(env, fs, f);
}

napi_value fs_fire_event(napi_env env, napi_callback_info info)
{
	Bool res;
	napi_value ret;
	GF_Filter *f = NULL;
	GF_FilterEvent *evt = NULL;
	NARG_ARGS_THIS(3, 1)
	FILTER_SESSION


	NAPI_CALL( napi_check_object_type_tag(env, argv[0], &filterevent_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a FilterEvent object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, argv[0], (void**) &evt) );

	if (argc>1) {
		NAPI_CALL( napi_check_object_type_tag(env, argv[1], &filter_tag, &_tag) );
		if (!_tag) {
			napi_throw_error(env, NULL, "Not a Filter object");
			return NULL;
		}
		NAPI_CALL( napi_unwrap(env, argv[1], (void**) &f) );
	}

	NARG_BOOL(upstream, 2, GF_FALSE);

	res = gf_fs_fire_event(fs, f, evt, upstream);
	NAPI_CALL(napi_get_boolean(env, res, &ret) );
	return ret;
}


napi_value fs_is_supported_mime(napi_env env, napi_callback_info info)
{
	napi_value val;
	NARG_ARGS_THIS(1, 1)
	FILTER_SESSION
	NARG_STR(mime, 0, NULL);

	NAPI_CALL( napi_get_boolean(env, gf_fs_is_supported_mime(fs, mime), &val) );
	return val;
}

napi_value fs_is_supported_source(napi_env env, napi_callback_info info)
{
	bool is_ok;
	napi_value val;
	NARG_ARGS_THIS(2, 1)
	FILTER_SESSION

	NARG_STR(url, 0, NULL);
	NARG_STR_ALLOC(parent_url, 1, NULL);
	is_ok = gf_fs_is_supported_source(fs, url, parent_url);
	if (parent_url) gf_free(parent_url);

	NAPI_CALL( napi_get_boolean(env, is_ok, &val) );
	return val;
}


napi_value fs_new_filter(napi_env env, napi_callback_info info)
{
	NAPI_Filter *napi_f=NULL;
	GF_Err e=GF_OK;
	napi_value obj, async_name;
	NARG_ARGS_THIS(2, 0)
	FILTER_SESSION
	NARG_STR(fname, 0, "Custom");
	NARG_U32(flags, 1, 0);

	GF_Filter *new_f = gf_fs_new_filter(fs, fname, flags | GF_FS_REG_MAIN_THREAD, &e);
	if (!new_f) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to create custom filter");
		return NULL;
	}
	//inherit from regular filter
	obj = fs_wrap_filter(env, fs, new_f);
	napi_f = gf_filter_get_rt_udta(new_f);

	napi_f->is_custom = GF_TRUE;

	//define all props for custom filter
	napi_property_descriptor curstom_filter_properties[] = {
		{ "block_enabled", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_BLOCK_ENABLED},
		{ "output_buffer", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_OUTPUT_BUFFER},
		{ "playout_buffer", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_PLAYOUT_BUFFER},
		{ "sinks_done", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_SINKS_DONE},
		{ "nb_evts_queued", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_EVTS_QUEUED},
		{ "clock_hint_time", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_CK_HINT_TIME},
		{ "clock_hint_mediatime", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_CK_HINT_MEDIATIME},
		{ "connections_pending", NULL, NULL, filter_cust_getter, NULL, NULL, napi_enumerable, &FS_PROP_CUST_CONNECTIONS_PENDING},

		{ "push_cap", 0, filter_push_cap, 0, 0, 0, napi_enumerable, 0 },
		{ "update_status", 0, filter_update_status, 0, 0, 0, napi_enumerable, 0 },
		{ "reschedule", 0, filter_reschedule, 0, 0, 0, napi_enumerable, 0 },
		{ "notify_failure", 0, filter_notify_failure, 0, 0, 0, napi_enumerable, 0 },
		{ "make_sticky", 0, filter_make_sticky, 0, 0, 0, napi_enumerable, 0 },
		{ "prevent_blocking", 0, filter_prevent_blocking, 0, 0, 0, napi_enumerable, 0 },
		{ "block_eos", 0, filter_block_eos, 0, 0, 0, napi_enumerable, 0 },
		{ "set_max_pids", 0, filter_set_max_pids, 0, 0, 0, napi_enumerable, 0 },
		{ "hint_clock", 0, filter_hint_clock, 0, 0, 0, napi_enumerable, 0 },
		{ "new_pid", 0, filter_new_pid, 0, 0, 0, napi_enumerable, 0 },
		{ "print_connections", 0, filter_print_connections, 0, 0, 0, napi_enumerable, 0 },

	};

	NAPI_CALL(napi_define_properties(env, obj, sizeof(curstom_filter_properties)/sizeof(napi_property_descriptor), curstom_filter_properties) );

	//setup callbacks
	e = gf_filter_set_configure_ckb(new_f, napi_filter_cbk_configure);
	if (!e) e = gf_filter_set_process_ckb(new_f, napi_filter_cbk_process);
	if (!e) e = gf_filter_set_process_event_ckb(new_f, napi_filter_cbk_process_event);
	if (!e) e = gf_filter_set_reconfigure_output_ckb(new_f, napi_filter_cbk_reconfigure_output);

	if (e) {
		napi_throw_error(env, gf_error_to_string(e), "Failed to assign custom filter callbacks");
		return NULL;
	}

	NAPI_CALL( napi_create_string_utf8(env, "AsyncFilterTask", NAPI_AUTO_LENGTH, &async_name) );
	NAPI_CALL( napi_async_init(env, obj, async_name, &napi_f->async_ctx) );
	return obj;
}



void del_fs(napi_env env, void* finalize_data, void* finalize_hint)
{
	GF_FilterSession *fs = (GF_FilterSession *)finalize_data;
	NAPI_Session *napi_fs = gf_fs_get_rt_udta(fs);

	if (napi_fs) {
		u32 rcount;
		napi_reference_unref(env, napi_fs->ref, &rcount);
		napi_delete_reference(env, napi_fs->ref);
		napi_async_destroy(env, napi_fs->async_ctx);
		napi_fs->ref = NULL;
		napi_fs->async_ctx = NULL;
	}

	gf_fs_del(fs);

	if (napi_fs) {
		if (napi_fs->defer_filters) gf_list_del(napi_fs->defer_filters);
		gf_free(napi_fs);
	}
}

void fs_on_filter_creation(void *udta, GF_Filter *filter, Bool is_destroy)
{
	napi_value fs_val, fun_val;
	napi_status status;
	NAPI_Session *napi_fs = (NAPI_Session *)udta;
	NAPI_Filter *napi_f = (NAPI_Filter *) gf_filter_get_rt_udta(filter);
	napi_env env = napi_fs->env;

	//set to NULL means final nodeJS addon shutdown, do not notify
	
	if (napi_fs->async_ctx != NULL) {
		GPAC_NAPI *gpac;
		if ( napi_get_instance_data(env, (void **) &gpac) != napi_ok) {
			return;
		}
		//creation callback are not guaranteed to run in main thread, defer notif to end of fs.run()
		if (!is_destroy && (gpac->main_thid != gf_th_id() ) ) {
			if (!napi_fs->defer_filters) napi_fs->defer_filters = gf_list_new();
			gf_list_add(napi_fs->defer_filters, filter);
			return;
		}
		//destroy callbacks are always scheduled on main thread, but remove filter if in pending list
		if (is_destroy)
			gf_list_del_item(napi_fs->defer_filters, filter);

		status = napi_get_reference_value(env, napi_fs->ref, &fs_val);
		if (status == napi_ok) {
			const char *fname = is_destroy ? "on_filter_del" : "on_filter_new";
			Bool has_fun = GF_FALSE;
			napi_has_named_property(env, fs_val, fname, (bool *) &has_fun);
			if (has_fun) {
				status = napi_get_named_property(env, fs_val, fname, &fun_val);
				if (status == napi_ok) {
					napi_value res, arg = fs_wrap_filter(env, napi_fs->fs, filter);
					status = napi_make_callback(env, napi_fs->async_ctx, fs_val, fun_val, 1, &arg, &res);
				}
			}
		}
	}

	if (is_destroy && napi_f) {
		u32 rcount;
		gf_filter_set_rt_udta(filter, NULL);
		napi_reference_unref(napi_f->env, napi_f->ref, &rcount);
		napi_delete_reference(napi_f->env, napi_f->ref);

		//extenral object was bound, destroy ref
		if (napi_f->binding_ref != NULL) {
			const char *fname = gf_filter_get_name(filter);
			if (fname && !strcmp(fname, "dashin"))
				dashin_detach_binding(napi_f);

			if (fname && !strcmp(fname, "httpour"))
				httpout_detach_binding(napi_f);

			napi_reference_unref(napi_f->env, napi_f->binding_ref, &rcount);
			napi_delete_reference(napi_f->env, napi_f->binding_ref);
		}

		//cleanup our bindings, we don't use finalizer as we don't know the order of destruction
		if (napi_f->is_custom) {
			u32 i, pids = gf_filter_get_opid_count(filter);
			for (i=0; i<pids; i++) {
				GF_FilterPid *pid = gf_filter_get_opid(filter, i);
				NAPI_FilterPid *napi_pid = (NAPI_FilterPid *) gf_filter_pid_get_udta(pid);
				if (napi_pid) {
					if (napi_pid->pck_refs) {
						while (gf_list_count(napi_pid->pck_refs)) {
							NAPI_FilterPacket *napi_pck = gf_list_pop_back(napi_pid->pck_refs);
							napi_pck->pid_refs = NULL;
						}
						gf_list_del(napi_pid->pck_refs);
					}
					gf_free(napi_pid);
				}
			}
		}
		gf_free(napi_f);
	}
}

Bool napi_gpac_event_proc(void *udta, GF_Event *event)
{
	napi_status status;
	bool has_fun;
	GF_FilterEvent fevt;
	napi_value fs_val, fun_val, evt, res;
	GPAC_NAPI *gpac;
	NAPI_Session *napi_fs = (NAPI_Session *) udta;


	status = napi_get_instance_data(napi_fs->env, (void **) &gpac);
	if (status != napi_ok) return GF_FALSE;

	if (gpac->main_thid != gf_th_id()) {
#ifndef GPAC_DISABLE_SVG
		GF_LOG(GF_LOG_WARNING, GF_LOG_SCRIPT, ("[NodeJS] GPAC main event callback from different thread not implemented, ignoring event %s\n", gf_dom_event_get_name(event->type) ));
#else
		GF_LOG(GF_LOG_WARNING, GF_LOG_SCRIPT, ("[NodeJS] GPAC main event callback from different thread not implemented, ignoring event %d\n", event->type));
#endif

		return GF_FALSE;
	}

	status = napi_get_reference_value(napi_fs->env, napi_fs->ref, &fs_val);
	if (status != napi_ok) return GF_FALSE;
	napi_has_named_property(napi_fs->env, fs_val, "on_event", (bool *) &has_fun);
	if (!has_fun) return GF_FALSE;
	status = napi_get_named_property(napi_fs->env, fs_val, "on_event", &fun_val);
	if (status != napi_ok) return GF_FALSE;

	fevt.base.type = GF_FEVT_USER;
	fevt.base.on_pid = NULL;
	fevt.user_event.event = *event;
	evt = wrap_filterevent(napi_fs->env, &fevt, NULL);

	//make event
	status = napi_make_callback(napi_fs->env, napi_fs->async_ctx, fs_val, fun_val, 1, &evt, &res);
	if (status != napi_ok) return GF_FALSE;

	napi_get_value_bool(napi_fs->env, res, &has_fun);
	return has_fun ? GF_TRUE : GF_FALSE;
}

napi_value new_fs(napi_env env, napi_callback_info info)
{
	NAPI_Session *napi_fs;
	napi_value fun_name;
	NARG_ARGS_THIS(1, 0);

	NARG_U32(flags, 0, 0);

	NAPI_GPAC
	gpac->is_init = GF_TRUE;

	GF_SAFEALLOC(napi_fs, NAPI_Session);
	if (!napi_fs) {
		napi_throw_error(env, NULL, "Failed to allocate filter session");
		return NULL;
	}

	GF_FilterSession *fs = gf_fs_new_defaults(flags);
	if (!fs) {
		gf_free(napi_fs);
		napi_throw_error(env, NULL, "Failed to allocate filter session");
		return NULL;
	}
	NAPI_CALL( napi_wrap(env, this_val, fs, del_fs, NULL, NULL) );

	NAPI_CALL( napi_type_tag_object(env, this_val, &filtersession_tag) );

	napi_fs->fs = fs;
	napi_fs->env = env;
	//we force all setup tasks on main thread, async IO with node is not implemented yet here
	gf_fs_set_filter_creation_callback(fs, fs_on_filter_creation, napi_fs, GF_TRUE);

	NAPI_CALL( napi_create_reference(env, this_val, 1, &napi_fs->ref) );

	NAPI_CALL( napi_create_string_utf8(env, "AsyncFilterSessionTask", NAPI_AUTO_LENGTH, &fun_name) );
	NAPI_CALL( napi_async_init(env, this_val, fun_name, &napi_fs->async_ctx) );


	gf_fs_set_ui_callback(fs, napi_gpac_event_proc, napi_fs);

	if (! (flags & GF_FS_FLAG_NON_BLOCKING))
		napi_fs->blocking = GF_TRUE;

	return this_val;
}

napi_status init_fs_class(napi_env env, napi_value exports, GPAC_NAPI *inst)
{
	napi_status status;
	napi_property_descriptor fs_properties[] = {
		{ "last_task", NULL, NULL, fs_getter, NULL, NULL, napi_enumerable, &FS_PROP_LAST_TASK},
		{ "nb_filters", NULL, NULL, fs_getter, NULL, NULL, napi_enumerable, &FS_PROP_NB_FILTERS},
		{ "http_bitrate", NULL, NULL, fs_getter, NULL, NULL, napi_enumerable, &FS_PROP_HTTP_RATE},
		{ "http_max_bitrate", NULL, NULL, fs_getter, fs_setter, NULL, napi_enumerable, &FS_PROP_MAX_HTTP_RATE},

		{ "run", 0, fs_run, 0, 0, 0, napi_enumerable, 0 },
		{ "load_src", 0, fs_load_src, 0, 0, 0, napi_enumerable, 0 },
		{ "load_dst", 0, fs_load_dst, 0, 0, 0, napi_enumerable, 0 },
		{ "load", 0, fs_load_filter, 0, 0, 0, napi_enumerable, 0 },
		{ "post_task", 0, fs_post_task, 0, 0, 0, napi_enumerable, 0 },
		{ "abort", 0, fs_abort, 0, 0, 0, napi_enumerable, 0 },
		{ "lock", 0, fs_lock, 0, 0, 0, napi_enumerable, 0 },
		//TODO move as attrib
		{ "reporting", 0, fs_reporting, 0, 0, 0, napi_enumerable, 0 },
		{ "print_stats", 0, fs_print_stats, 0, 0, 0, napi_enumerable, 0 },
		{ "print_graph", 0, fs_print_graph, 0, 0, 0, napi_enumerable, 0 },
		{ "get_filter", 0, fs_get_filter, 0, 0, 0, napi_enumerable, 0 },
		{ "fire_event", 0, fs_fire_event, 0, 0, 0, napi_enumerable, 0 },
		{ "is_supported_mime", 0, fs_is_supported_mime, 0, 0, 0, napi_enumerable, 0 },
		{ "is_supported_source", 0, fs_is_supported_source, 0, 0, 0, napi_enumerable, 0 },
		{ "new_filter", 0, fs_new_filter, 0, 0, 0, napi_enumerable, 0 },
	};

	napi_value cons;
	status = napi_define_class(env, "FilterSession", NAPI_AUTO_LENGTH , new_fs, NULL, sizeof(fs_properties)/sizeof(napi_property_descriptor), fs_properties, &cons);
	if (status != napi_ok) return status;

	status = napi_set_named_property(env, exports, "FilterSession", cons);
	if (status != napi_ok) return status;

	return napi_ok;
}

static u32 FEVT_PROP_TYPE = 0;
static u32 FEVT_PROP_START_RANGE = 1;
static u32 FEVT_PROP_END_RANGE = 2;
static u32 FEVT_PROP_SPEED = 3;
static u32 FEVT_PROP_FROM_PCK = 4;
static u32 FEVT_PROP_HW_BUFFER_RESET = 5;
static u32 FEVT_PROP_INITIAL_BCAST = 6;
static u32 FEVT_PROP_TS_BASED = 7;
static u32 FEVT_PROP_FULL_FILE_ONLY = 8;
static u32 FEVT_PROP_FORCED_DASH_SWITCH = 9;
static u32 FEVT_PROP_DROP_NON_REF = 10;
static u32 FEVT_PROP_NO_BYTERANGE = 11;
static u32 FEVT_PROP_START_OFFSET = 12;
static u32 FEVT_PROP_END_OFFSET = 13;
static u32 FEVT_PROP_HINT_BLOCK_SIZE = 14;
static u32 FEVT_PROP_SOURCE_SWITCH = 15;
static u32 FEVT_PROP_SWITCH_IS_INIT_SEG = 16;
static u32 FEVT_PROP_SKIP_CACHE_EXP = 17;
static u32 FEVT_PROP_SSIZE_URL = 18;
static u32 FEVT_PROP_SSIZE_MEDIA_START = 19;
static u32 FEVT_PROP_SSIZE_MEDIA_END = 20;
static u32 FEVT_PROP_SSIZE_IDX_START = 21;
static u32 FEVT_PROP_SSIZE_IDX_END = 22;
static u32 FEVT_PROP_SSIZE_IS_INIT = 23;
static u32 FEVT_PROP_SSIZE_IS_SHIFT = 24;
static u32 FEVT_PROP_QS_UP = 25;
static u32 FEVT_PROP_QS_DEP_GROUP_IDX = 26;
static u32 FEVT_PROP_QS_QIDX = 27;
static u32 FEVT_PROP_QS_TILEMODE = 28;
static u32 FEVT_PROP_QS_DEGRADATION = 29;
static u32 FEVT_PROP_MIN_X = 30;
static u32 FEVT_PROP_MIN_Y = 31;
static u32 FEVT_PROP_MAX_X = 32;
static u32 FEVT_PROP_MAX_Y = 33;
static u32 FEVT_PROP_IS_GAZE = 34;
static u32 FEVT_PROP_MAX_BUFFER = 35;
static u32 FEVT_PROP_MAX_PLAYOUT = 36;
static u32 FEVT_PROP_MIN_PLAYOUT = 37;
static u32 FEVT_PROP_PID_ONLY = 38;
static u32 FEVT_PROP_UI_MOUSE_X = 39;
static u32 FEVT_PROP_UI_MOUSE_Y = 40;
static u32 FEVT_PROP_UI_MOUSE_WHEEL = 41;
static u32 FEVT_PROP_UI_MOUSE_BUTTON = 42;
static u32 FEVT_PROP_UI_MT_ROTATE = 43;
static u32 FEVT_PROP_UI_MT_PINCH = 44;
static u32 FEVT_PROP_UI_MT_FINGERS = 45;
static u32 FEVT_PROP_UI_KEY_CODE = 46;
static u32 FEVT_PROP_UI_KEY_NAME = 47;
static u32 FEVT_PROP_UI_KEY_MODS = 48;
static u32 FEVT_PROP_UI_KEY_HW = 49;
static u32 FEVT_PROP_UI_SHOWTYPE = 50;
static u32 FEVT_PROP_UI_CLIPBOARD = 51;
static u32 FEVT_PROP_UI_WIDTH = 52;
static u32 FEVT_PROP_UI_HEIGHT = 53;
static u32 FEVT_PROP_UI_MOVE_X = 54;
static u32 FEVT_PROP_UI_MOVE_Y = 55;
static u32 FEVT_PROP_UI_MOVE_RELATIVE = 56;
static u32 FEVT_PROP_UI_MOVE_ALIGNX = 57;
static u32 FEVT_PROP_UI_MOVE_ALIGNY = 58;
static u32 FEVT_PROP_UI_CAPTION = 59;
static u32 FEVT_PROP_FILE_DEL = 60;
static u32 FEVT_PROP_UI_TYPE = 61;
static u32 FEVT_PROP_NTP_REF = 62;
static u32 FEVT_PROP_NAME = 63;
static u32 FEVT_PROP_UI_NAME = 64;
static u32 FEVT_PROP_PID = 65;
static u32 FEVT_PROP_TO_PCK = 66;
static u32 FEVT_PROP_ORIG_DELAY = 67;
static u32 FEVT_PROP_HINT_FIRST_DTS = 68;
static u32 FEVT_PROP_HINT_START_OFFSET = 69;
static u32 FEVT_PROP_HINT_END_OFFSET = 70;


#define FILTEREVENT\
	GF_FilterEvent *evt=NULL;\
	bool _tag;\
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &filterevent_tag, &_tag) );\
	if (!_tag) {\
		napi_throw_error(env, NULL, "Not a FilterEvent object");\
		return NULL;\
	}\
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &evt) );\


#define ALL_EVENT_FIELDS	\
	EVT_DBL(FEVT_PROP_START_RANGE, evt->play.start_range);\
	EVT_DBL(FEVT_PROP_END_RANGE, evt->play.end_range);\
	EVT_DBL(FEVT_PROP_SPEED, evt->play.speed);\
	EVT_U32(FEVT_PROP_FROM_PCK, evt->play.from_pck);\
	EVT_BOOL(FEVT_PROP_HW_BUFFER_RESET, evt->play.hw_buffer_reset);\
	EVT_BOOL(FEVT_PROP_INITIAL_BCAST, evt->play.initial_broadcast_play);\
	EVT_U32(FEVT_PROP_TS_BASED, evt->play.timestamp_based);\
	EVT_BOOL(FEVT_PROP_FULL_FILE_ONLY, evt->play.full_file_only);\
	EVT_BOOL(FEVT_PROP_FORCED_DASH_SWITCH, evt->play.forced_dash_segment_switch);\
	EVT_BOOL(FEVT_PROP_DROP_NON_REF, evt->play.drop_non_ref);\
	EVT_BOOL(FEVT_PROP_NO_BYTERANGE, evt->play.no_byterange_forward);\
	EVT_U32(FEVT_PROP_TO_PCK, evt->play.to_pck);\
	EVT_U32(FEVT_PROP_ORIG_DELAY, evt->play.orig_delay);\
	EVT_U64(FEVT_PROP_HINT_FIRST_DTS, evt->play.hint_first_dts);\
	EVT_U64(FEVT_PROP_HINT_START_OFFSET, evt->play.hint_start_offset);\
	EVT_U64(FEVT_PROP_HINT_END_OFFSET, evt->play.hint_end_offset);\
	EVT_U64(FEVT_PROP_START_OFFSET, evt->seek.start_offset);\
	EVT_U64(FEVT_PROP_END_OFFSET, evt->seek.end_offset);\
	EVT_U32(FEVT_PROP_HINT_BLOCK_SIZE, evt->seek.hint_block_size);\
	EVT_STR(FEVT_PROP_SOURCE_SWITCH, evt->seek.source_switch);\
	EVT_BOOL(FEVT_PROP_SWITCH_IS_INIT_SEG, evt->seek.is_init_segment);\
	EVT_BOOL(FEVT_PROP_SKIP_CACHE_EXP, evt->seek.skip_cache_expiration);\
	EVT_STR(FEVT_PROP_SSIZE_URL, evt->seg_size.seg_url);\
	EVT_U64(FEVT_PROP_SSIZE_MEDIA_START, evt->seg_size.media_range_start);\
	EVT_U64(FEVT_PROP_SSIZE_MEDIA_END, evt->seg_size.media_range_end);\
	EVT_U64(FEVT_PROP_SSIZE_IDX_START, evt->seg_size.idx_range_start);\
	EVT_U64(FEVT_PROP_SSIZE_IDX_END, evt->seg_size.idx_range_end);\
	EVT_BOOL(FEVT_PROP_SSIZE_IS_INIT, evt->seg_size.is_init);\
	EVT_BOOL(FEVT_PROP_SSIZE_IS_SHIFT, evt->seg_size.is_shift);\
	EVT_BOOL(FEVT_PROP_QS_UP, evt->quality_switch.up);\
	EVT_U32(FEVT_PROP_QS_DEP_GROUP_IDX, evt->quality_switch.dependent_group_index);\
	EVT_S32(FEVT_PROP_QS_QIDX, evt->quality_switch.q_idx);\
	EVT_U32(FEVT_PROP_QS_TILEMODE, evt->quality_switch.set_tile_mode_plus_one);\
	EVT_U32(FEVT_PROP_QS_DEGRADATION, evt->quality_switch.quality_degradation);\
	EVT_U32(FEVT_PROP_MIN_X, evt->visibility_hint.min_x);\
	EVT_U32(FEVT_PROP_MIN_Y, evt->visibility_hint.min_y);\
	EVT_U32(FEVT_PROP_MAX_X, evt->visibility_hint.max_x);\
	EVT_U32(FEVT_PROP_MAX_Y, evt->visibility_hint.max_y);\
	EVT_BOOL(FEVT_PROP_IS_GAZE, evt->visibility_hint.is_gaze);\
	EVT_U32(FEVT_PROP_MAX_BUFFER, evt->buffer_req.max_buffer_us);\
	EVT_U32(FEVT_PROP_MAX_PLAYOUT, evt->buffer_req.max_playout_us);\
	EVT_U32(FEVT_PROP_MIN_PLAYOUT, evt->buffer_req.min_playout_us);\
	EVT_BOOL(FEVT_PROP_PID_ONLY, evt->buffer_req.pid_only);\
	EVT_STR(FEVT_PROP_FILE_DEL, evt->file_del.url);\
	EVT_U64(FEVT_PROP_NTP_REF, evt->ntp.ntp);\
	EVT_U32(FEVT_PROP_UI_TYPE, evt->user_event.event.type);\
	if (evt->user_event.event.type==GF_EVENT_MULTITOUCH) {\
		EVT_FIX(FEVT_PROP_UI_MOUSE_X, evt->user_event.event.mtouch.x);\
		EVT_FIX(FEVT_PROP_UI_MOUSE_Y, evt->user_event.event.mtouch.y);\
	} else {\
		EVT_S32(FEVT_PROP_UI_MOUSE_X, evt->user_event.event.mouse.x);\
		EVT_S32(FEVT_PROP_UI_MOUSE_Y, evt->user_event.event.mouse.y);\
	}\
	EVT_FIX(FEVT_PROP_UI_MOUSE_WHEEL, evt->user_event.event.mouse.wheel_pos);\
	EVT_U32(FEVT_PROP_UI_MOUSE_BUTTON, evt->user_event.event.mouse.button);\
	if (evt->user_event.event.type<=GF_EVENT_MOUSEWHEEL) {\
		EVT_U32(FEVT_PROP_UI_KEY_MODS, evt->user_event.event.mouse.key_states);\
	} else {\
		EVT_U32(FEVT_PROP_UI_KEY_MODS, evt->user_event.event.key.flags);\
	}\
	EVT_FIX(FEVT_PROP_UI_MT_ROTATE, evt->user_event.event.mtouch.rotation);\
	EVT_FIX(FEVT_PROP_UI_MT_PINCH, evt->user_event.event.mtouch.pinch);\
	EVT_U32(FEVT_PROP_UI_MT_FINGERS, evt->user_event.event.mtouch.num_fingers);\
	EVT_U32(FEVT_PROP_UI_KEY_CODE, evt->user_event.event.key.key_code);\
	EVT_U32(FEVT_PROP_UI_KEY_HW, evt->user_event.event.key.hw_code);\
	EVT_STR(FEVT_PROP_UI_CLIPBOARD, evt->user_event.event.clipboard.text);\
	EVT_U32(FEVT_PROP_UI_WIDTH, evt->user_event.event.size.width);\
	EVT_U32(FEVT_PROP_UI_HEIGHT, evt->user_event.event.size.height);\
	EVT_U32(FEVT_PROP_UI_SHOWTYPE, evt->user_event.event.show.show_type);\
	EVT_S32(FEVT_PROP_UI_MOVE_X, evt->user_event.event.move.x);\
	EVT_S32(FEVT_PROP_UI_MOVE_Y, evt->user_event.event.move.y);\
	EVT_U32(FEVT_PROP_UI_MOVE_RELATIVE, evt->user_event.event.move.relative);\
	EVT_U32(FEVT_PROP_UI_MOVE_ALIGNX, evt->user_event.event.move.align_x);\
	EVT_U32(FEVT_PROP_UI_MOVE_ALIGNY, evt->user_event.event.move.align_y);\
	EVT_STR(FEVT_PROP_UI_CAPTION, evt->user_event.event.caption.caption);\

napi_value fevt_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILTEREVENT

#define EVT_DBL(_magic_val, _field) \
	if (magic == &_magic_val) {\
		NAPI_CALL( napi_create_double(env, _field, &ret) );\
		return ret;\
	}

#define EVT_FIX(_magic_val, _field) \
	if (magic == &_magic_val) {\
		NAPI_CALL( napi_create_double(env, FIX2FLT(_field), &ret) );\
		return ret;\
	}

#define EVT_U32(_magic_val, _field) \
	if (magic == &_magic_val) {\
		NAPI_CALL( napi_create_uint32(env, (u32) _field, &ret) );\
		return ret;\
	}

#define EVT_S32(_magic_val, _field) \
	if (magic == &_magic_val) {\
		NAPI_CALL( napi_create_int32(env, (s32) _field, &ret) );\
		return ret;\
	}

#define EVT_STR(_magic_val, _field) \
	if (magic == &_magic_val) {\
		if (_field) {\
			NAPI_CALL( napi_create_string_utf8(env, _field, NAPI_AUTO_LENGTH, &ret) );\
		} else {\
			NAPI_CALL( napi_get_null(env, &ret) );\
		}\
		return ret;\
	}

#define EVT_U64(_magic_val, _field) \
	if (magic == &_magic_val) {\
		NAPI_CALL( napi_create_int64(env, (s64) _field, &ret) );\
		return ret;\
	}

#define EVT_BOOL(_magic_val, _field) \
	if (magic == &_magic_val) {\
		NAPI_CALL( napi_get_boolean(env, (bool) _field, &ret) );\
		return ret;\
	}

	//read-only props
	EVT_U32(FEVT_PROP_TYPE, evt->base.type);
	EVT_STR(FEVT_PROP_NAME, gf_filter_event_name(evt->base.type) );
#ifndef GPAC_DISABLE_SVG
	EVT_STR(FEVT_PROP_UI_NAME, gf_dom_event_get_name(evt->user_event.event.type) );
#endif

	//all read/write props
	ALL_EVENT_FIELDS

	//custom fileds
#ifndef GPAC_DISABLE_SVG
	EVT_STR(FEVT_PROP_UI_KEY_NAME, gf_dom_get_key_name(evt->user_event.event.key.key_code));
#endif

	if (magic == &FEVT_PROP_PID) {
		NAPI_FilterPid *napi_pid = evt->base.on_pid ? gf_filter_pid_get_udta(evt->base.on_pid) : NULL;

		if (napi_pid) {
			napi_get_reference_value(env, napi_pid->ref, &ret);
		} else {
			NAPI_CALL( napi_get_null(env, &ret) );
		}
		return ret;
	}


#undef EVT_DBL
#undef EVT_FIX
#undef EVT_U32
#undef EVT_S32
#undef EVT_STR
#undef EVT_U64
#undef EVT_BOOL

	return NULL;
}

napi_value fevt_setter(napi_env env, napi_callback_info info)
{
	NARG_ARGS_THIS_MAGIC(1, 1)
	FILTEREVENT

#define	EVT_U32(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_U32(val, 0, 0);\
		_field = val;\
		return NULL;\
	}

#define	EVT_S32(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_S32(val, 0, 0);\
		_field = val;\
		return NULL;\
	}

#define	EVT_U64(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_U64(val, 0, 0);\
		_field = val;\
		return NULL;\
	}

#define	EVT_BOOL(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_BOOL(val, 0, 0);\
		_field = val;\
		return NULL;\
	}

#define	EVT_DBL(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_DBL(val, 0, 0);\
		_field = val;\
		return NULL;\
	}

#define	EVT_FIX(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_DBL(val, 0, 0);\
		_field = FLT2FIX(val);\
		return NULL;\
	}

#define	EVT_STR(_magic_val, _field)\
	if (magic == &_magic_val) {\
		NARG_STR(val, 0, 0);\
		_field = val;\
		return NULL;\
	}

	//all read/write props
	ALL_EVENT_FIELDS

	//custom fields
#ifndef GPAC_DISABLE_SVG
	if (magic == &FEVT_PROP_UI_KEY_NAME) {
		NARG_STR(val, 0, 0);
		evt->user_event.event.key.key_code = gf_dom_get_key_type(val);
		return NULL;
	}
#endif
	return NULL;
}


void del_evt(napi_env env, void* finalize_data, void* finalize_hint)
{
	GF_FilterEvent *fevt = (GF_FilterEvent *)finalize_data;
	gf_free(fevt);
}

napi_value wrap_filterevent(napi_env env, GF_FilterEvent *evt, napi_value *for_val)
{
	napi_status status;
	napi_value obj;
	if (for_val) {
		obj = *for_val;
	} else {
		NAPI_CALL(napi_create_object(env, &obj) );
	}
	napi_callback evt_set = for_val ? fevt_setter : NULL;

	napi_property_descriptor fevt_base_properties[] = {
		{ "type", NULL, NULL, fevt_getter, NULL, NULL, napi_enumerable, &FEVT_PROP_TYPE},
		{ "pid", NULL, NULL, fevt_getter, NULL, NULL, napi_enumerable, &FEVT_PROP_PID},
		{ "name", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_NAME},
	};
	NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_base_properties)/sizeof(napi_property_descriptor), fevt_base_properties) );


	//define properties based on event type
	if (evt->base.type == GF_FEVT_PLAY) {
		napi_property_descriptor fevt_properties[] = {
			{ "start_range", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_START_RANGE},
			{ "end_range", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_END_RANGE},
			{ "speed", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SPEED},
			{ "from_pck", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FROM_PCK},
			{ "hw_buffer_reset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_HW_BUFFER_RESET},
			{ "initial_broadcast_play", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_INITIAL_BCAST},
			{ "timestamp_based", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_TS_BASED},
			{ "full_file_only", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FULL_FILE_ONLY},
			{ "forced_dash_segment_switch", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FORCED_DASH_SWITCH},
			{ "drop_non_ref", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_DROP_NON_REF},
			{ "no_byterange_forward", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_NO_BYTERANGE},
			{ "to_pck", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_TO_PCK},
			{ "orig_delay", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_ORIG_DELAY},
			{ "hint_first_dts", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_HINT_FIRST_DTS},
			{ "hint_start_offset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_HINT_START_OFFSET},
			{ "hint_end_offset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_HINT_END_OFFSET},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_SET_SPEED) {
		napi_property_descriptor fevt_properties[] = {
			{ "speed", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SPEED},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if ((evt->base.type == GF_FEVT_STOP) || (evt->base.type == GF_FEVT_PLAY_HINT)) {
		napi_property_descriptor fevt_properties[] = {
			{ "forced_dash_segment_switch", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FORCED_DASH_SWITCH},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_SOURCE_SEEK) {
		napi_property_descriptor fevt_properties[] = {
			{ "start_offset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_START_OFFSET},
			{ "end_offset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_END_OFFSET},
			{ "hint_block_size", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_HINT_BLOCK_SIZE},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_SOURCE_SWITCH) {
		napi_property_descriptor fevt_properties[] = {
			{ "start_offset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_START_OFFSET},
			{ "end_offset", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_END_OFFSET},
			{ "source_switch", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SOURCE_SWITCH},
			{ "is_init_segment", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SWITCH_IS_INIT_SEG},
			{ "skip_cache_expiration", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SKIP_CACHE_EXP},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_SEGMENT_SIZE) {
		napi_property_descriptor fevt_properties[] = {
			{ "seg_url", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_URL},
			{ "media_range_start", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_MEDIA_START},
			{ "media_range_end", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_MEDIA_END},
			{ "idx_range_start", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_IDX_START},
			{ "idx_range_end", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_IDX_END},
			{ "is_init", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_IS_INIT},
			{ "is_shift", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_SSIZE_IS_SHIFT},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_QUALITY_SWITCH) {
		napi_property_descriptor fevt_properties[] = {
			{ "up", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_QS_UP},
			{ "dependent_group_index", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_QS_DEP_GROUP_IDX},
			{ "q_idx", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_QS_QIDX},
			{ "set_tile_mode_plus_one", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_QS_TILEMODE},
			{ "quality_degradation", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_QS_DEGRADATION},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_VISIBILITY_HINT) {
		napi_property_descriptor fevt_properties[] = {
			{ "min_x", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MIN_X},
			{ "max_x", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MIN_Y},
			{ "min_y", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MAX_X},
			{ "max_y", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MAX_Y},
			{ "is_gaze", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_IS_GAZE},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_BUFFER_REQ) {
		napi_property_descriptor fevt_properties[] = {
			{ "max_buffer_us", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MAX_BUFFER},
			{ "max_playout_us", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MAX_PLAYOUT},
			{ "min_playout_us", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_MIN_PLAYOUT},
			{ "pid_only", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_PID_ONLY},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_USER) {
		u32 ui_type = evt->user_event.event.type;

		napi_property_descriptor fevt_ui_properties[] = {
			{ "ui_type", NULL, NULL, fevt_getter, NULL, NULL, napi_enumerable, &FEVT_PROP_UI_TYPE},
#ifndef GPAC_DISABLE_SVG
			{ "ui_name", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_NAME},
#endif
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_ui_properties)/sizeof(napi_property_descriptor), fevt_ui_properties) );

		switch (ui_type) {
		case GF_EVENT_CLICK:
		case GF_EVENT_MOUSEUP:
		case GF_EVENT_MOUSEDOWN:
		case GF_EVENT_MOUSEOVER:
		case GF_EVENT_MOUSEOUT:
		case GF_EVENT_MOUSEMOVE:
		case GF_EVENT_MOUSEWHEEL:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "mouse_x", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOUSE_X},
				{ "mouse_y", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOUSE_Y},
				{ "wheel", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOUSE_WHEEL},
				{ "button", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOUSE_BUTTON},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;
		case GF_EVENT_MULTITOUCH:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "mt_x", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOUSE_X},
				{ "mt_y", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOUSE_Y},
				{ "mt_rotate", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MT_ROTATE},
				{ "mt_pinch", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MT_PINCH},
				{ "mt_fingers", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MT_FINGERS},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;

		case GF_EVENT_KEYUP:
		case GF_EVENT_KEYDOWN:
		case GF_EVENT_LONGKEYPRESS:
		case GF_EVENT_TEXTINPUT:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "keycode", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_KEY_CODE},
#ifndef GPAC_DISABLE_SVG
				{ "keyname", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_KEY_NAME},
#endif
				{ "keymods", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_KEY_MODS},
				{ "hwkey", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_KEY_HW},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;
		case GF_EVENT_PASTE_TEXT:
		case GF_EVENT_COPY_TEXT:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "clipboard", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_CLIPBOARD},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;
		case GF_EVENT_SIZE:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "width", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_WIDTH},
				{ "height", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_HEIGHT},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;
		case GF_EVENT_MOVE:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "move_x", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOVE_X},
				{ "move_y", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOVE_Y},
				{ "showtype", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_SHOWTYPE},
				{ "move_relative", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOVE_RELATIVE},
				{ "move_alignx", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOVE_ALIGNX},
				{ "move_aligny", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_MOVE_ALIGNY},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;
		case GF_EVENT_SET_CAPTION:
		{
			napi_property_descriptor fevt_properties[] = {
				{ "caption", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_UI_CAPTION},
			};
			NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
		}
			break;
		default:
			break;
		}
	}
	else if (evt->base.type == GF_FEVT_PLAY_HINT) {
		napi_property_descriptor fevt_properties[] = {
			{ "full_file_only", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FULL_FILE_ONLY},
			{ "forced_dash_segment_switch", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FORCED_DASH_SWITCH},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_FILE_DELETE) {
		napi_property_descriptor fevt_properties[] = {
			{ "url", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_FILE_DEL},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	else if (evt->base.type == GF_FEVT_NTP_REF) {
		napi_property_descriptor fevt_properties[] = {
			{ "ntp", NULL, NULL, fevt_getter, evt_set, NULL, napi_enumerable, &FEVT_PROP_NTP_REF},
		};
		NAPI_CALL( napi_define_properties(env, obj, sizeof(fevt_properties)/sizeof(napi_property_descriptor), fevt_properties) );
	}
	//other events don't have any attributes

	NAPI_CALL( napi_wrap(env, obj, evt, for_val ? del_evt : NULL, NULL, NULL) );
	NAPI_CALL( napi_type_tag_object(env, obj, &filterevent_tag) );

	return obj;
}
napi_value new_fevt(napi_env env, napi_callback_info info)
{
	GF_FilterEvent *evt;
	NARG_ARGS_THIS(2, 1);
	NARG_U32(type, 0, 0);

	NAPI_GPAC
	gpac->is_init = GF_TRUE;

	if (type==GF_FEVT_USER) {
		if (argc<2) {
			napi_throw_error(env, NULL, "User event type must be specified");
			return NULL;
		}
	}
	GF_SAFEALLOC(evt, GF_FilterEvent)
	if (!evt) {
		napi_throw_error(env, NULL, "Failed to allocate filter event");
		return NULL;
	}
	NAPI_CALL( napi_get_value_uint32(env, argv[0], &evt->base.type) );
	if (type==GF_FEVT_USER) {
		u32 val;
		NAPI_CALL( napi_get_value_uint32(env, argv[1], &val) );
		evt->user_event.event.type = (u8) val;
	}

	return wrap_filterevent(env, evt, &this_val);
}


napi_status init_fevt_class(napi_env env, napi_value exports, GPAC_NAPI *inst)
{
	napi_status status;
	napi_value cons;
	status = napi_define_class(env, "FilterEvent", NAPI_AUTO_LENGTH , new_fevt, NULL, 0, NULL, &cons);
	if (status != napi_ok) return status;
	return napi_set_named_property(env, exports, "FilterEvent", cons);
}


static napi_status InitConstants(napi_env env, napi_value exports);
napi_status init_fio_class(napi_env env, napi_value exports, GPAC_NAPI *inst);

napi_value Init(napi_env env, napi_value exports)
{
	GPAC_NAPI *inst;
	napi_status status;
	napi_property_descriptor p_desc = {.attributes=napi_enumerable};

	//don't use gf_malloc here !!
	inst = malloc(sizeof(GPAC_NAPI));
	if (!inst) {
		napi_throw_error(env, NULL, "Failed to allocate GPAC NAPI context");
		return NULL;
	}
	memset(inst, 0, sizeof(GPAC_NAPI));
	NAPI_CALL( napi_set_instance_data(env, inst, gpac_napi_finalize, NULL) );

	NAPI_CALL(InitConstants(env, exports) );

#define DEF_FUN(_name, _fun) \
	p_desc.utf8name = _name; \
	NAPI_CALL( napi_create_function(env, NULL, 0, _fun, NULL, &p_desc.value) ); \
	NAPI_CALL( napi_define_properties(env, exports, 1, &p_desc) );

	/*function exports*/
	DEF_FUN("init", gpac_init);
	DEF_FUN("e2s", gpac_e2s);
	DEF_FUN("set_logs", gpac_set_logs);
	DEF_FUN("set_args", gpac_set_args);
	DEF_FUN("sys_clock", gpac_sys_clock);
	DEF_FUN("sys_clock_high_res", gpac_sys_clock_high_res);

	DEF_FUN("set_rmt_fun", gpac_set_rmt_fun);
	DEF_FUN("rmt_send", gpac_rmt_send);
	DEF_FUN("rmt_on", gpac_rmt_on);
	DEF_FUN("rmt_enable", gpac_rmt_enable);


	status = init_fs_class(env, exports, inst);
	if (status != napi_ok) return NULL;

	status = init_fevt_class(env, exports, inst);
	if (status != napi_ok) return NULL;

	status = init_fio_class(env, exports, inst);
	if (status != napi_ok) return NULL;

	inst->main_thid = gf_th_id();

	//default init
	gf_sys_init(0, NULL);
	return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)


static napi_status InitConstants(napi_env env, napi_value exports)
{
	napi_status status;
	napi_property_descriptor p_desc = {.attributes=napi_enumerable};

#define DEF_CONST_STR(_name, _fun) \
	p_desc.utf8name = _name; \
	status = napi_create_string_utf8(env, _fun, NAPI_AUTO_LENGTH, &p_desc.value);\
	if (status != napi_ok) return status;\
	status = napi_define_properties(env, exports, 1, &p_desc); \
	if (status != napi_ok) return status;\

#define DEF_CONST_INT(_name, _fun) \
	p_desc.utf8name = _name; \
	status = napi_create_int32(env, _fun, &p_desc.value); \
	if (status != napi_ok) return status;\
	status = napi_define_properties(env, exports, 1, &p_desc);\
	if (status != napi_ok) return status;\

#define DEF_CONST(_const) \
	p_desc.utf8name = #_const; \
	status = napi_create_int32(env, _const, &p_desc.value); \
	if (status != napi_ok) return status;\
	status = napi_define_properties(env, exports, 1, &p_desc);\
	if (status != napi_ok) return status;\


	/*symbol exports*/
	DEF_CONST_STR("version", gf_gpac_version() );
	DEF_CONST_STR("copyright", gf_gpac_copyright() );
	DEF_CONST_STR("copyright_cite", gf_gpac_copyright_cite() );

	DEF_CONST_INT("abi_major", gf_gpac_abi_major() );
	DEF_CONST_INT("abi_minor", gf_gpac_abi_minor() );
	DEF_CONST_INT("abi_micro", gf_gpac_abi_micro() );

	DEF_CONST(GF_LOG_ERROR)
	DEF_CONST(GF_LOG_WARNING)
	DEF_CONST(GF_LOG_INFO)
	DEF_CONST(GF_LOG_DEBUG)

	DEF_CONST(GF_PROP_FORBIDDEN)
	DEF_CONST(GF_PROP_BOOL)
	DEF_CONST(GF_PROP_UINT)
	DEF_CONST(GF_PROP_SINT)
	DEF_CONST(GF_PROP_LUINT)
	DEF_CONST(GF_PROP_LSINT)
	DEF_CONST(GF_PROP_FRACTION)
	DEF_CONST(GF_PROP_FRACTION64)
	DEF_CONST(GF_PROP_FLOAT)
	DEF_CONST(GF_PROP_DOUBLE)
	DEF_CONST(GF_PROP_VEC2I)
	DEF_CONST(GF_PROP_VEC2)
	DEF_CONST(GF_PROP_VEC3I)
	DEF_CONST(GF_PROP_VEC4I)
	DEF_CONST(GF_PROP_STRING)
	DEF_CONST(GF_PROP_STRING_NO_COPY)
	DEF_CONST(GF_PROP_DATA)
	DEF_CONST(GF_PROP_NAME)
	DEF_CONST(GF_PROP_DATA_NO_COPY)
	DEF_CONST(GF_PROP_CONST_DATA)
	DEF_CONST(GF_PROP_POINTER)
	DEF_CONST(GF_PROP_STRING_LIST)
	DEF_CONST(GF_PROP_UINT_LIST)
	DEF_CONST(GF_PROP_SINT_LIST)
	DEF_CONST(GF_PROP_VEC2I_LIST)
	DEF_CONST(GF_PROP_4CC)
	DEF_CONST(GF_PROP_4CC_LIST)

	DEF_CONST(GF_PROP_FIRST_ENUM)
	DEF_CONST(GF_PROP_PIXFMT)
	DEF_CONST(GF_PROP_PCMFMT)
	DEF_CONST(GF_PROP_CICP_COL_PRIM)
	DEF_CONST(GF_PROP_CICP_COL_TFC)
	DEF_CONST(GF_PROP_CICP_COL_MX)
	DEF_CONST(GF_PROP_CICP_LAYOUT)
	DEF_CONST(GF_PROP_FIRST_ENUM)

	DEF_CONST(GF_FEVT_PLAY)
	DEF_CONST(GF_FEVT_SET_SPEED)
	DEF_CONST(GF_FEVT_STOP)
	DEF_CONST(GF_FEVT_PAUSE)
	DEF_CONST(GF_FEVT_RESUME)
	DEF_CONST(GF_FEVT_SOURCE_SEEK)
	DEF_CONST(GF_FEVT_SOURCE_SWITCH)
	DEF_CONST(GF_FEVT_SEGMENT_SIZE)
	DEF_CONST(GF_FEVT_QUALITY_SWITCH)
	DEF_CONST(GF_FEVT_VISIBILITY_HINT)
	DEF_CONST(GF_FEVT_INFO_UPDATE)
	DEF_CONST(GF_FEVT_BUFFER_REQ)
	DEF_CONST(GF_FEVT_CAPS_CHANGE)
	DEF_CONST(GF_FEVT_CONNECT_FAIL)
	DEF_CONST(GF_FEVT_USER)
	DEF_CONST(GF_FEVT_PLAY_HINT)
	DEF_CONST(GF_FEVT_FILE_DELETE)

	DEF_CONST(GF_STATS_LOCAL)
	DEF_CONST(GF_STATS_LOCAL_INPUTS)
	DEF_CONST(GF_STATS_DECODER_SINK)
	DEF_CONST(GF_STATS_DECODER_SOURCE)
	DEF_CONST(GF_STATS_ENCODER_SINK)
	DEF_CONST(GF_STATS_ENCODER_SOURCE)
	DEF_CONST(GF_STATS_SINK)

	DEF_CONST(GF_FILTER_CLOCK_NONE)
	DEF_CONST(GF_FILTER_CLOCK_PCR)
	DEF_CONST(GF_FILTER_CLOCK_PCR_DISC)

	DEF_CONST(GF_FILTER_SAP_NONE)
	DEF_CONST(GF_FILTER_SAP_1)
	DEF_CONST(GF_FILTER_SAP_2)
	DEF_CONST(GF_FILTER_SAP_3)
	DEF_CONST(GF_FILTER_SAP_4)
	DEF_CONST(GF_FILTER_SAP_4_PROL)

	DEF_CONST(GF_EOS);
	DEF_CONST(GF_OK)
	DEF_CONST(GF_BAD_PARAM)
	DEF_CONST(GF_OUT_OF_MEM)
	DEF_CONST(GF_IO_ERR)
	DEF_CONST(GF_NOT_SUPPORTED)
	DEF_CONST(GF_CORRUPTED_DATA)
	DEF_CONST(GF_SG_UNKNOWN_NODE)
	DEF_CONST(GF_SG_INVALID_PROTO)
	DEF_CONST(GF_SCRIPT_ERROR)
	DEF_CONST(GF_BUFFER_TOO_SMALL)
	DEF_CONST(GF_NON_COMPLIANT_BITSTREAM)
	DEF_CONST(GF_FILTER_NOT_FOUND)
	DEF_CONST(GF_URL_ERROR)
	DEF_CONST(GF_SERVICE_ERROR)
	DEF_CONST(GF_REMOTE_SERVICE_ERROR)
	DEF_CONST(GF_STREAM_NOT_FOUND)
	DEF_CONST(GF_ISOM_INVALID_FILE)
	DEF_CONST(GF_ISOM_INCOMPLETE_FILE)
	DEF_CONST(GF_ISOM_INVALID_MEDIA)
	DEF_CONST(GF_ISOM_INVALID_MODE)
	DEF_CONST(GF_ISOM_UNKNOWN_DATA_REF)
	DEF_CONST(GF_ODF_INVALID_DESCRIPTOR)
	DEF_CONST(GF_ODF_FORBIDDEN_DESCRIPTOR)
	DEF_CONST(GF_ODF_INVALID_COMMAND)
	DEF_CONST(GF_BIFS_UNKNOWN_VERSION)
	DEF_CONST(GF_IP_ADDRESS_NOT_FOUND)
	DEF_CONST(GF_IP_CONNECTION_FAILURE)
	DEF_CONST(GF_IP_NETWORK_FAILURE)
	DEF_CONST(GF_IP_CONNECTION_CLOSED)
	DEF_CONST(GF_IP_NETWORK_EMPTY)
	DEF_CONST(GF_IP_UDP_TIMEOUT)
	DEF_CONST(GF_AUTHENTICATION_FAILURE)
	DEF_CONST(GF_NOT_READY)
	DEF_CONST(GF_INVALID_CONFIGURATION)
	DEF_CONST(GF_NOT_FOUND)
	DEF_CONST(GF_PROFILE_NOT_SUPPORTED)
	DEF_CONST(GF_REQUIRES_NEW_INSTANCE)
	DEF_CONST(GF_FILTER_NOT_SUPPORTED)


	DEF_CONST(GF_FILTER_UPDATE_DOWNSTREAM)
	DEF_CONST(GF_FILTER_UPDATE_UPSTREAM)

	DEF_CONST(GF_EVENT_CLICK)
	DEF_CONST(GF_EVENT_MOUSEUP)
	DEF_CONST(GF_EVENT_MOUSEDOWN)
	DEF_CONST(GF_EVENT_MOUSEMOVE)
	DEF_CONST(GF_EVENT_MOUSEWHEEL)
	DEF_CONST(GF_EVENT_DBLCLICK)
	DEF_CONST(GF_EVENT_MULTITOUCH)
	DEF_CONST(GF_EVENT_KEYUP)
	DEF_CONST(GF_EVENT_KEYDOWN)
	DEF_CONST(GF_EVENT_TEXTINPUT)
	DEF_CONST(GF_EVENT_DROPFILE)
	DEF_CONST(GF_EVENT_TIMESHIFT_DEPTH)
	DEF_CONST(GF_EVENT_TIMESHIFT_UPDATE)
	DEF_CONST(GF_EVENT_TIMESHIFT_OVERFLOW)
	DEF_CONST(GF_EVENT_TIMESHIFT_UNDERRUN)
	DEF_CONST(GF_EVENT_PASTE_TEXT)
	DEF_CONST(GF_EVENT_COPY_TEXT)
	DEF_CONST(GF_EVENT_SIZE)
	DEF_CONST(GF_EVENT_SHOWHIDE)
	DEF_CONST(GF_EVENT_MOVE)
	DEF_CONST(GF_EVENT_SET_CAPTION)
	DEF_CONST(GF_EVENT_REFRESH)
	DEF_CONST(GF_EVENT_QUIT)
	DEF_CONST(GF_EVENT_CODEC_SLOW)
	DEF_CONST(GF_EVENT_CODEC_OK)

	DEF_CONST(GF_KEY_UNIDENTIFIED)
	DEF_CONST(GF_KEY_ACCEPT)
	DEF_CONST(GF_KEY_AGAIN)
	DEF_CONST(GF_KEY_ALLCANDIDATES)
	DEF_CONST(GF_KEY_ALPHANUM)
	DEF_CONST(GF_KEY_ALT)
	DEF_CONST(GF_KEY_ALTGRAPH)
	DEF_CONST(GF_KEY_APPS)
	DEF_CONST(GF_KEY_ATTN)
	DEF_CONST(GF_KEY_BROWSERBACK)
	DEF_CONST(GF_KEY_BROWSERFAVORITES)
	DEF_CONST(GF_KEY_BROWSERFORWARD)
	DEF_CONST(GF_KEY_BROWSERHOME)
	DEF_CONST(GF_KEY_BROWSERREFRESH)
	DEF_CONST(GF_KEY_BROWSERSEARCH)
	DEF_CONST(GF_KEY_BROWSERSTOP)
	DEF_CONST(GF_KEY_CAPSLOCK)
	DEF_CONST(GF_KEY_CLEAR)
	DEF_CONST(GF_KEY_CODEINPUT)
	DEF_CONST(GF_KEY_COMPOSE)
	DEF_CONST(GF_KEY_CONTROL)
	DEF_CONST(GF_KEY_CRSEL)
	DEF_CONST(GF_KEY_CONVERT)
	DEF_CONST(GF_KEY_COPY)
	DEF_CONST(GF_KEY_CUT)
	DEF_CONST(GF_KEY_DOWN)
	DEF_CONST(GF_KEY_END)
	DEF_CONST(GF_KEY_ENTER)
	DEF_CONST(GF_KEY_ERASEEOF)
	DEF_CONST(GF_KEY_EXECUTE)
	DEF_CONST(GF_KEY_EXSEL)
	DEF_CONST(GF_KEY_F1)
	DEF_CONST(GF_KEY_F2)
	DEF_CONST(GF_KEY_F3)
	DEF_CONST(GF_KEY_F4)
	DEF_CONST(GF_KEY_F5)
	DEF_CONST(GF_KEY_F6)
	DEF_CONST(GF_KEY_F7)
	DEF_CONST(GF_KEY_F8)
	DEF_CONST(GF_KEY_F9)
	DEF_CONST(GF_KEY_F10)
	DEF_CONST(GF_KEY_F11)
	DEF_CONST(GF_KEY_F12)
	DEF_CONST(GF_KEY_F13)
	DEF_CONST(GF_KEY_F14)
	DEF_CONST(GF_KEY_F15)
	DEF_CONST(GF_KEY_F16)
	DEF_CONST(GF_KEY_F17)
	DEF_CONST(GF_KEY_F18)
	DEF_CONST(GF_KEY_F19)
	DEF_CONST(GF_KEY_F20)
	DEF_CONST(GF_KEY_F21)
	DEF_CONST(GF_KEY_F22)
	DEF_CONST(GF_KEY_F23)
	DEF_CONST(GF_KEY_F24)
	DEF_CONST(GF_KEY_FINALMODE)
	DEF_CONST(GF_KEY_FIND)
	DEF_CONST(GF_KEY_FULLWIDTH)
	DEF_CONST(GF_KEY_HALFWIDTH)
	DEF_CONST(GF_KEY_HANGULMODE)
	DEF_CONST(GF_KEY_HANJAMODE)
	DEF_CONST(GF_KEY_HELP)
	DEF_CONST(GF_KEY_HIRAGANA)
	DEF_CONST(GF_KEY_HOME)
	DEF_CONST(GF_KEY_INSERT)
	DEF_CONST(GF_KEY_JAPANESEHIRAGANA)
	DEF_CONST(GF_KEY_JAPANESEKATAKANA)
	DEF_CONST(GF_KEY_JAPANESEROMAJI)
	DEF_CONST(GF_KEY_JUNJAMODE)
	DEF_CONST(GF_KEY_KANAMODE)
	DEF_CONST(GF_KEY_KANJIMODE)
	DEF_CONST(GF_KEY_KATAKANA)
	DEF_CONST(GF_KEY_LAUNCHAPPLICATION1)
	DEF_CONST(GF_KEY_LAUNCHAPPLICATION2)
	DEF_CONST(GF_KEY_LAUNCHMAIL)
	DEF_CONST(GF_KEY_LEFT)
	DEF_CONST(GF_KEY_META)
	DEF_CONST(GF_KEY_MEDIANEXTTRACK)
	DEF_CONST(GF_KEY_MEDIAPLAYPAUSE)
	DEF_CONST(GF_KEY_MEDIAPREVIOUSTRACK)
	DEF_CONST(GF_KEY_MEDIASTOP)
	DEF_CONST(GF_KEY_MODECHANGE)
	DEF_CONST(GF_KEY_NONCONVERT)
	DEF_CONST(GF_KEY_NUMLOCK)
	DEF_CONST(GF_KEY_PAGEDOWN)
	DEF_CONST(GF_KEY_PAGEUP)
	DEF_CONST(GF_KEY_PASTE)
	DEF_CONST(GF_KEY_PAUSE)
	DEF_CONST(GF_KEY_PLAY)
	DEF_CONST(GF_KEY_PREVIOUSCANDIDATE)
	DEF_CONST(GF_KEY_PRINTSCREEN)
	DEF_CONST(GF_KEY_PROCESS)
	DEF_CONST(GF_KEY_PROPS)
	DEF_CONST(GF_KEY_RIGHT)
	DEF_CONST(GF_KEY_ROMANCHARACTERS)
	DEF_CONST(GF_KEY_SCROLL)
	DEF_CONST(GF_KEY_SELECT)
	DEF_CONST(GF_KEY_SELECTMEDIA)
	DEF_CONST(GF_KEY_SHIFT)
	DEF_CONST(GF_KEY_STOP)
	DEF_CONST(GF_KEY_UP)
	DEF_CONST(GF_KEY_UNDO)
	DEF_CONST(GF_KEY_VOLUMEDOWN)
	DEF_CONST(GF_KEY_VOLUMEMUTE)
	DEF_CONST(GF_KEY_VOLUMEUP)
	DEF_CONST(GF_KEY_WIN)
	DEF_CONST(GF_KEY_ZOOM)
	DEF_CONST(GF_KEY_BACKSPACE)
	DEF_CONST(GF_KEY_TAB)
	DEF_CONST(GF_KEY_CANCEL)
	DEF_CONST(GF_KEY_ESCAPE)
	DEF_CONST(GF_KEY_SPACE)
	DEF_CONST(GF_KEY_EXCLAMATION)
	DEF_CONST(GF_KEY_QUOTATION)
	DEF_CONST(GF_KEY_NUMBER)
	DEF_CONST(GF_KEY_DOLLAR)
	DEF_CONST(GF_KEY_AMPERSAND)
	DEF_CONST(GF_KEY_APOSTROPHE)
	DEF_CONST(GF_KEY_LEFTPARENTHESIS)
	DEF_CONST(GF_KEY_RIGHTPARENTHESIS)
	DEF_CONST(GF_KEY_STAR)
	DEF_CONST(GF_KEY_PLUS)
	DEF_CONST(GF_KEY_COMMA)
	DEF_CONST(GF_KEY_HYPHEN)
	DEF_CONST(GF_KEY_FULLSTOP)
	DEF_CONST(GF_KEY_SLASH)
	DEF_CONST(GF_KEY_0)
	DEF_CONST(GF_KEY_1)
	DEF_CONST(GF_KEY_2)
	DEF_CONST(GF_KEY_3)
	DEF_CONST(GF_KEY_4)
	DEF_CONST(GF_KEY_5)
	DEF_CONST(GF_KEY_6)
	DEF_CONST(GF_KEY_7)
	DEF_CONST(GF_KEY_8)
	DEF_CONST(GF_KEY_9)
	DEF_CONST(GF_KEY_COLON)
	DEF_CONST(GF_KEY_SEMICOLON)
	DEF_CONST(GF_KEY_LESSTHAN)
	DEF_CONST(GF_KEY_EQUALS)
	DEF_CONST(GF_KEY_GREATERTHAN)
	DEF_CONST(GF_KEY_QUESTION)
	DEF_CONST(GF_KEY_AT)
	DEF_CONST(GF_KEY_A)
	DEF_CONST(GF_KEY_B)
	DEF_CONST(GF_KEY_C)
	DEF_CONST(GF_KEY_D)
	DEF_CONST(GF_KEY_E)
	DEF_CONST(GF_KEY_F)
	DEF_CONST(GF_KEY_G)
	DEF_CONST(GF_KEY_H)
	DEF_CONST(GF_KEY_I)
	DEF_CONST(GF_KEY_J)
	DEF_CONST(GF_KEY_K)
	DEF_CONST(GF_KEY_L)
	DEF_CONST(GF_KEY_M)
	DEF_CONST(GF_KEY_N)
	DEF_CONST(GF_KEY_O)
	DEF_CONST(GF_KEY_P)
	DEF_CONST(GF_KEY_Q)
	DEF_CONST(GF_KEY_R)
	DEF_CONST(GF_KEY_S)
	DEF_CONST(GF_KEY_T)
	DEF_CONST(GF_KEY_U)
	DEF_CONST(GF_KEY_V)
	DEF_CONST(GF_KEY_W)
	DEF_CONST(GF_KEY_X)
	DEF_CONST(GF_KEY_Y)
	DEF_CONST(GF_KEY_Z)
	DEF_CONST(GF_KEY_LEFTSQUAREBRACKET)
	DEF_CONST(GF_KEY_BACKSLASH)
	DEF_CONST(GF_KEY_RIGHTSQUAREBRACKET)
	DEF_CONST(GF_KEY_CIRCUM)
	DEF_CONST(GF_KEY_UNDERSCORE)
	DEF_CONST(GF_KEY_GRAVEACCENT)
	DEF_CONST(GF_KEY_LEFTCURLYBRACKET)
	DEF_CONST(GF_KEY_PIPE)
	DEF_CONST(GF_KEY_RIGHTCURLYBRACKET)
	DEF_CONST(GF_KEY_DEL)
	DEF_CONST(GF_KEY_INVERTEXCLAMATION)
	DEF_CONST(GF_KEY_DEADGRAVE)
	DEF_CONST(GF_KEY_DEADEACUTE)
	DEF_CONST(GF_KEY_DEADCIRCUM)
	DEF_CONST(GF_KEY_DEADTILDE)
	DEF_CONST(GF_KEY_DEADMACRON)
	DEF_CONST(GF_KEY_DEADBREVE)
	DEF_CONST(GF_KEY_DEADABOVEDOT)
	DEF_CONST(GF_KEY_DEADDIARESIS)
	DEF_CONST(GF_KEY_DEADRINGABOVE)
	DEF_CONST(GF_KEY_DEADDOUBLEACUTE)
	DEF_CONST(GF_KEY_DEADCARON)
	DEF_CONST(GF_KEY_DEADCEDILLA)
	DEF_CONST(GF_KEY_DEADOGONEK)
	DEF_CONST(GF_KEY_DEADIOTA)
	DEF_CONST(GF_KEY_EURO)
	DEF_CONST(GF_KEY_DEADVOICESOUND)
	DEF_CONST(GF_KEY_DEADSEMIVOICESOUND)
	DEF_CONST(GF_KEY_CHANNELUP)
	DEF_CONST(GF_KEY_CHANNELDOWN)
	DEF_CONST(GF_KEY_TEXT)
	DEF_CONST(GF_KEY_INFO)
	DEF_CONST(GF_KEY_EPG)
	DEF_CONST(GF_KEY_RECORD)
	DEF_CONST(GF_KEY_BEGINPAGE)
	DEF_CONST(GF_KEY_CELL_SOFT1)
	DEF_CONST(GF_KEY_CELL_SOFT2)
	DEF_CONST(GF_KEY_JOYSTICK)

	DEF_CONST(GF_KEY_MOD_SHIFT)
	DEF_CONST(GF_KEY_MOD_CTRL)
	DEF_CONST(GF_KEY_MOD_ALT)
	DEF_CONST(GF_KEY_EXT_NUMPAD)
	DEF_CONST(GF_KEY_EXT_LEFT)
	DEF_CONST(GF_KEY_EXT_RIGHT)

	DEF_CONST(GF_MemTrackerNone)
	DEF_CONST(GF_MemTrackerSimple)
	DEF_CONST(GF_MemTrackerBackTrace)

	DEF_CONST(GF_FS_SCHEDULER_LOCK_FREE)
	DEF_CONST(GF_FS_SCHEDULER_LOCK)
	DEF_CONST(GF_FS_SCHEDULER_LOCK_FREE_X)
	DEF_CONST(GF_FS_SCHEDULER_LOCK_FORCE)
	DEF_CONST(GF_FS_SCHEDULER_DIRECT)

	DEF_CONST(GF_FS_FLAG_LOAD_META)
	DEF_CONST(GF_FS_FLAG_NON_BLOCKING)
	DEF_CONST(GF_FS_FLAG_NO_GRAPH_CACHE)
	DEF_CONST(GF_FS_FLAG_NO_REGULATION)
	DEF_CONST(GF_FS_FLAG_NO_PROBE)
	DEF_CONST(GF_FS_FLAG_NO_REASSIGN)
	DEF_CONST(GF_FS_FLAG_PRINT_CONNECTIONS)
	DEF_CONST(GF_FS_FLAG_NO_ARG_CHECK)
	DEF_CONST(GF_FS_FLAG_NO_RESERVOIR)
	DEF_CONST(GF_FS_FLAG_FULL_LINK)
	DEF_CONST(GF_FS_FLAG_NO_IMPLICIT)
	DEF_CONST(GF_FS_FLAG_REQUIRE_SOURCE_ID)
	DEF_CONST(GF_FS_FLAG_FORCE_DEFER_LINK)
	DEF_CONST(GF_FS_FLAG_PREVENT_PLAY)

	DEF_CONST(GF_FS_ARG_HINT_NORMAL)
	DEF_CONST(GF_FS_ARG_HINT_ADVANCED)
	DEF_CONST(GF_FS_ARG_HINT_EXPERT)
	DEF_CONST(GF_FS_ARG_HINT_HIDE)
	DEF_CONST(GF_FS_ARG_UPDATE)
	DEF_CONST(GF_FS_ARG_META)
	DEF_CONST(GF_FS_ARG_META_ALLOC)
	DEF_CONST(GF_FS_ARG_SINK_ALIAS)

	DEF_CONST(GF_CAPFLAG_IN_BUNDLE)
	DEF_CONST(GF_CAPFLAG_INPUT)
	DEF_CONST(GF_CAPFLAG_OUTPUT)
	DEF_CONST(GF_CAPFLAG_EXCLUDED)
	DEF_CONST(GF_CAPFLAG_LOADED_FILTER)
	DEF_CONST(GF_CAPFLAG_STATIC)
	DEF_CONST(GF_CAPFLAG_OPTIONAL)
	DEF_CONST(GF_CAPS_INPUT)
	DEF_CONST(GF_CAPS_INPUT_OPT)
	DEF_CONST(GF_CAPS_INPUT_STATIC)
	DEF_CONST(GF_CAPS_INPUT_STATIC_OPT)
	DEF_CONST(GF_CAPS_INPUT_EXCLUDED)
	DEF_CONST(GF_CAPS_INPUT_LOADED_FILTER)
	DEF_CONST(GF_CAPS_OUTPUT)
	DEF_CONST(GF_CAPS_OUTPUT_LOADED_FILTER)
	DEF_CONST(GF_CAPS_OUTPUT_EXCLUDED)
	DEF_CONST(GF_CAPS_OUTPUT_STATIC)
	DEF_CONST(GF_CAPS_OUTPUT_STATIC_EXCLUDED)
	DEF_CONST(GF_CAPS_INPUT_OUTPUT)
	DEF_CONST(GF_CAPS_INPUT_OUTPUT_OPT)

	DEF_CONST(GF_FS_FLUSH_NONE)
	DEF_CONST(GF_FS_FLUSH_ALL)
	DEF_CONST(GF_FS_FLUSH_FAST)

	DEF_CONST(GF_SETUP_ERROR)
	DEF_CONST(GF_NOTIF_ERROR)
	DEF_CONST(GF_NOTIF_ERROR_AND_DISCONNECT)

	return napi_ok;
}


void dashbind_finalize(napi_env env, void* finalize_data, void* finalize_hint)
{
	NAPI_Dash *napi_dash = (NAPI_Dash *)finalize_data;
return;
	u32 rcount;
	napi_reference_unref(env, napi_dash->groups, &rcount);
	napi_delete_reference(env, napi_dash->groups);
	napi_dash->groups = NULL;

	napi_reference_unref(env, napi_dash->ref, &rcount);
	napi_delete_reference(env, napi_dash->ref);
	napi_dash->ref = NULL;

	/*destruction not done here*/
}

void dashbind_period_reset(void *udta, u32 type)
{
	napi_status status;
	napi_value obj, fun_val, res;
	NAPI_Dash *napi_dash = (NAPI_Dash *)udta;

	napi_get_reference_value(napi_dash->env, napi_dash->ref, &obj);
	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_dash->env, obj, "on_period_reset", (bool *) &has_fun);
	if (has_fun) {
		status = napi_get_named_property(napi_dash->env, obj, "on_period_reset", &fun_val);
		if (status!=napi_ok) has_fun = GF_FALSE;
	}

	if (!has_fun) return;

	napi_value arg;
	napi_create_uint32(napi_dash->env, type, &arg);
	napi_call_function(napi_dash->env, obj, fun_val, 1, &arg, &res);
}

void dashbind_new_group(void *udta, u32 group_idx, void *_dash)
{
	napi_property_descriptor p_desc = {.attributes=napi_enumerable};
	napi_status status;
	napi_value obj, group, groups, qualities, fun_val, res;
	NAPI_Dash *napi_dash = (NAPI_Dash *)udta;
	napi_env env = napi_dash->env;
	GF_DashClient *dash = (GF_DashClient *) _dash;

	napi_create_object(env, &group);

	napi_get_reference_value(napi_dash->env, napi_dash->groups, &groups);
	napi_set_element(env, groups, group_idx, group);

	p_desc.utf8name = "idx";
	napi_create_uint32(env, group_idx, &p_desc.value);
	napi_define_properties(env, group, 1, &p_desc);

	p_desc.utf8name = "duration";
	napi_create_int64(env, (s64) gf_dash_get_period_duration(dash), &p_desc.value);
	napi_define_properties(env, group, 1, &p_desc);

	p_desc.utf8name = "qualities";
	napi_create_array(env, &p_desc.value);
	napi_define_properties(env, group, 1, &p_desc);
	qualities = p_desc.value;

	u32 i, nb_qualities = gf_dash_group_get_num_qualities(dash, group_idx);
	for (i=0; i<nb_qualities; i++) {
		napi_value q, segs;
		GF_DASHQualityInfo qinfo;
		gf_dash_group_get_quality_info(dash, group_idx, i, &qinfo);

		napi_create_object(env, &q);


#define QSET_U32(_val) \
		p_desc.utf8name = #_val;\
		napi_create_uint32(env, qinfo._val, &p_desc.value); \
		napi_define_properties(env, q, 1, &p_desc);

#define QSET_BOOL(_val) \
		p_desc.utf8name = #_val;\
		napi_get_boolean(env, qinfo._val ? true : false, &p_desc.value); \
		napi_define_properties(env, q, 1, &p_desc);

#define QSET_STR(_val) \
		p_desc.utf8name = #_val;\
		if (qinfo._val) {\
			napi_create_string_utf8(env, qinfo._val, NAPI_AUTO_LENGTH, &p_desc.value);\
		} else {\
			napi_get_null(env, &p_desc.value);\
		}\
		napi_define_properties(env, q, 1, &p_desc);

#define QSET_DOUBLE(_val) \
		p_desc.utf8name = #_val;\
		napi_create_double(env, qinfo._val ? true : false, &p_desc.value); \
		napi_define_properties(env, q, 1, &p_desc);


		QSET_U32(bandwidth)
		QSET_STR(ID)
		QSET_STR(mime)
		QSET_STR(codec)
		QSET_U32(width)
		QSET_U32(height)
		QSET_BOOL(interlaced)

		p_desc.utf8name = "fps";
		GF_Fraction frac = {qinfo.fps_num, qinfo.fps_den};
		p_desc.value = frac_to_napi(env, &frac);
		napi_define_properties(env, q, 1, &p_desc);

		p_desc.utf8name = "sar";
		frac.num = qinfo.par_num;
		frac.den = qinfo.par_den;
		p_desc.value = frac_to_napi(env, &frac);
		napi_define_properties(env, q, 1, &p_desc);

		QSET_U32(sample_rate)
		QSET_U32(nb_channels)
		QSET_BOOL(disabled)
		QSET_BOOL(is_selected)
		QSET_DOUBLE(ast_offset)
		QSET_DOUBLE(average_duration)

		p_desc.utf8name = "sizes";
		napi_create_array(env, &p_desc.value);
		napi_define_properties(env, q, 1, &p_desc);
		segs = p_desc.value;

		/*! list of segmentURLs if known, NULL otherwise. Used for onDemand profile to get segment sizes*/
		if (qinfo.seg_urls) {
			u32 j, nb_segs=gf_list_count(qinfo.seg_urls);
			for (j=0; j<nb_segs; j++) {
				napi_value si;
				GF_MPD_SegmentURL *surl = gf_list_get((GF_List *) qinfo.seg_urls, j);

				napi_create_object(env, &si);
				napi_set_element(env, segs, j, si);
				p_desc.utf8name = "size";
				napi_create_int64(env, (1 + surl->media_range->end_range - surl->media_range->start_range), &p_desc.value);
				napi_define_properties(env, si, 1, &p_desc);

				p_desc.utf8name = "duration";
				napi_create_uint32(env, surl->duration, &p_desc.value);
				napi_define_properties(env, si, 1, &p_desc);
			}
		}
		napi_set_element(env, qualities, i, q);
	}

	//create srd
	p_desc.utf8name = "SRD";
	u32 id, x, y, w, h, fw, fh;
	if (gf_dash_group_get_srd_info(dash, group_idx, &id, &x, &y, &w, &h, &fw, &fh)) {
		napi_value srd, val;
		napi_create_object(env, &srd);
		p_desc.value = srd;

#define SRD_U32(_val) \
		napi_create_uint32(env, _val, &val);\
		napi_set_named_property(env, srd, #_val, val); \

		SRD_U32(id)
		SRD_U32(x)
		SRD_U32(y)
		SRD_U32(w)
		SRD_U32(h)
		SRD_U32(fw)
		SRD_U32(fh)

	} else {
		napi_get_null(env, &p_desc.value);
	}
	napi_define_properties(env, group, 1, &p_desc);

	napi_get_reference_value(napi_dash->env, napi_dash->ref, &obj);
	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_dash->env, obj, "on_new_group", (bool *) &has_fun);
	if (has_fun) {
		status = napi_get_named_property(napi_dash->env, obj, "on_new_group", &fun_val);
		if (status!=napi_ok) has_fun = GF_FALSE;
	}

	if (has_fun) {
		napi_call_function(napi_dash->env, obj, fun_val, 1, &group, &res);
	}
}

s32 dashbind_rate_adaptation(void *udta, u32 group_idx, u32 base_group_idx, Bool force_low_complex, void *_stats)
{
	napi_status status;
	napi_value obj, fun_val, groups, res, st, val;
	GF_DASHCustomAlgoInfo *stats = (GF_DASHCustomAlgoInfo *)_stats;
	NAPI_Dash *napi_dash = (NAPI_Dash *)udta;
	napi_env env = napi_dash->env;

	napi_get_reference_value(napi_dash->env, napi_dash->ref, &obj);
	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_dash->env, obj, "on_rate_adaptation", (bool *) &has_fun);
	if (has_fun) {
		status = napi_get_named_property(napi_dash->env, obj, "on_rate_adaptation", &fun_val);
		if (status!=napi_ok) has_fun = GF_FALSE;
	}

	if (!has_fun) return -1;

	u32 i, nb_groups;
	napi_get_reference_value(env, napi_dash->groups, &groups);
	napi_get_named_property(env, groups, "length", &res);
	napi_get_value_uint32(env, res, &nb_groups);

	napi_value args[4];
	napi_get_null(env, &args[0]);
	napi_get_null(env, &args[1]);
	for (i=0; i<nb_groups;i++) {
		u32 g_idx=0;
		napi_value group;
		napi_get_element(env, groups, i, &group);
		napi_get_named_property(env, group, "idx", &res);
		napi_get_value_uint32(env, res, &g_idx);
		if (g_idx==group_idx) args[0] = group;
		else if (g_idx==base_group_idx) args[1] = group;
	}
	napi_get_boolean(env, force_low_complex ? true : false, &args[2]);

	napi_create_object(env, &st);
	args[3] = st;


#define SSET_U32(_val) \
		napi_create_uint32(env, stats->_val, &val);\
		napi_set_named_property(env, st, #_val, val); \

#define SSET_DOUBLE(_val) \
		napi_create_double(env, stats->_val, &val);\
		napi_set_named_property(env, st, #_val, val); \

	SSET_U32(download_rate);
	SSET_U32(file_size);
	SSET_DOUBLE(speed);
	SSET_DOUBLE(max_available_speed);
	SSET_U32(display_width);
	SSET_U32(display_height);
	SSET_U32(active_quality_idx);
	SSET_U32(buffer_min_ms);
	SSET_U32(buffer_max_ms);
	SSET_U32(buffer_occupancy_ms);
	SSET_U32(quality_degradation_hint);
	SSET_U32(total_rate);
	napi_call_function(napi_dash->env, obj, fun_val, 4, args, &res);

	s32 ret_val=-1;
	napi_get_value_int32(env, res, &ret_val);
	return ret_val;
}

typedef struct
{
	u32 bits_per_sec;
	u64 total_bytes;
	u64 bytes_done;
	u64 us_since_start;
	u32 buffer_dur;
	u32 current_seg_dur;
} GF_DASHDownloadStats;

s32 dashbind_download_monitor(void *udta, u32 group_idx, void *_stats)
{
	napi_status status;
	napi_value obj, fun_val, groups, res, st, val;
	GF_DASHDownloadStats *stats = (GF_DASHDownloadStats *)_stats;
	NAPI_Dash *napi_dash = (NAPI_Dash *)udta;
	napi_env env = napi_dash->env;

	napi_get_reference_value(napi_dash->env, napi_dash->ref, &obj);
	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_dash->env, obj, "on_download_monitor", (bool *) &has_fun);
	if (has_fun) {
		status = napi_get_named_property(napi_dash->env, obj, "on_download_monitor", &fun_val);
		if (status!=napi_ok) has_fun = GF_FALSE;
	}

	if (!has_fun) return -1;

	u32 i, nb_groups;
	napi_get_reference_value(env, napi_dash->groups, &groups);
	napi_get_named_property(env, groups, "length", &res);
	napi_get_value_uint32(env, res, &nb_groups);

	napi_value args[2];
	napi_get_null(env, &args[0]);
	for (i=0; i<nb_groups;i++) {
		u32 g_idx=0;
		napi_value group;
		napi_get_element(env, groups, i, &group);
		napi_get_named_property(env, group, "idx", &res);
		napi_get_value_uint32(env, res, &g_idx);
		if (g_idx==group_idx) {
			args[0] = group;
			break;
		}
	}
	napi_create_object(env, &st);
	args[1] = st;

#define DSSET_U32(_val) \
		napi_create_uint32(env, stats->_val, &val);\
		napi_set_named_property(env, st, #_val, val); \

#define DSSET_U64(_val) \
		napi_create_int64(env, (s64) stats->_val, &val);\
		napi_set_named_property(env, st, #_val, val); \

	DSSET_U32(bits_per_sec);
	DSSET_U64(total_bytes);
	DSSET_U64(bytes_done);
	DSSET_U64(us_since_start);
	DSSET_U32(buffer_dur);
	DSSET_U32(current_seg_dur);

	napi_call_function(napi_dash->env, obj, fun_val, 2, args, &res);

	s32 ret_val=-1;
	napi_get_value_int32(env, res, &ret_val);
	return ret_val;
}
/*currently not exposed by filter.h*/
GF_Err gf_filter_bind_dash_algo_callbacks(GF_Filter *filter, void *udta,
		void (*period_reset)(void *rate_adaptation, u32 type),
		void (*new_group)(void *udta, u32 group_idx, void *dash),
		s32 (*rate_adaptation)(void *udta, u32 group_idx, u32 base_group_idx, Bool force_low_complex, void *stats),
		s32 (*download_monitor)(void *udta, u32 group_idx, void *stats)
	);

napi_value dashin_bind(napi_env env, napi_callback_info info)
{
	GF_Err e;
	napi_value val;
	NAPI_Dash *napi_dash;

	NARG_ARGS_THIS(1,1)
	FILTER

	GF_SAFEALLOC(napi_dash, NAPI_Dash);
	if (!napi_dash) {
		napi_throw_error(env, NULL, "Failed to allocate dashin binding");
		return NULL;
	}


	NAPI_Filter *napi_f = (NAPI_Filter *) gf_filter_get_rt_udta(f);
	napi_create_reference(env, argv[0], 1, &napi_f->binding_ref);
	napi_dash->ref = napi_f->binding_ref;
	napi_dash->env = env;
	napi_f->binding_stack = napi_dash;

	napi_create_array(env, &val);
	napi_create_reference(env, val, 1, &napi_dash->groups);

	napi_type_tag_object(env, argv[0], &dashbind_tag);
	NAPI_CALL( napi_wrap(env, argv[0], napi_dash, dashbind_finalize, NULL, NULL) );

	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_f->env, argv[0], "on_download_monitor", (bool *) &has_fun);
	e = gf_filter_bind_dash_algo_callbacks(f, napi_dash, dashbind_period_reset, dashbind_new_group, dashbind_rate_adaptation, has_fun ? dashbind_download_monitor : NULL);
	if (e) {
		u32 rcount;
		napi_reference_unref(env, napi_f->binding_ref, &rcount);
		napi_delete_reference(env, napi_f->binding_ref);
		napi_f->binding_ref = NULL;
		napi_f->binding_stack = NULL;
		gf_free(napi_dash);

		napi_throw_error(env, gf_error_to_string(e), "Failed to bind dashin callbacks");
		return NULL;
	}
	//tag dasher filter to run on main thread only
	gf_filter_force_main_thread(napi_f->f, GF_TRUE);
	return NULL;
}

void dashin_detach_binding(NAPI_Filter *napi_f)
{
	NAPI_Dash *napi_dash = (NAPI_Dash *) napi_f->binding_stack;
	if (!napi_dash) return;
	gf_free(napi_dash);
}

typedef struct
{
	napi_ref ref;
	napi_env env;
} NAPI_HTTPOut;

void httpoutbind_finalize(napi_env env, void* finalize_data, void* finalize_hint)
{
}

typedef struct
{
	void *httpout_session;
	napi_env env;
	napi_ref req_ref;
} NAPI_HTTPSession;

void napi_http_del(napi_env env, void *finalize_data, void* finalize_hint)
{
	NAPI_HTTPSession *napi_http = (NAPI_HTTPSession *)finalize_data;
	if (napi_http) gf_free(napi_http);
}


static u32 httpout_throttle_cbk(void *udta, u64 done, u64 total)
{
	napi_value req;
	napi_value fun_val, args[2], res;
	u32 retval=0;
	NAPI_HTTPSession *napi_sess = (NAPI_HTTPSession *)udta;

	//call
	napi_get_reference_value(napi_sess->env, napi_sess->req_ref, &req);
	napi_get_named_property(napi_sess->env, req, "throttle", &fun_val);
	napi_create_int64(napi_sess->env, done, &args[0]);
	napi_create_int64(napi_sess->env, total, &args[1]);
	napi_call_function(napi_sess->env, req, fun_val, 2, args, &res);
	napi_get_value_uint32(napi_sess->env, res, &retval);

	return retval;
}
static s32 httpout_read_cbk(void *udta, u8 *buffer, u32 buffer_size)
{
	napi_value req;
	napi_value fun_val, arg, ab, res;
	u32 nb_bytes=0;
	napi_status status;
	NAPI_HTTPSession *napi_sess = (NAPI_HTTPSession *)udta;


	//call
	napi_get_reference_value(napi_sess->env, napi_sess->req_ref, &req);
	napi_get_named_property(napi_sess->env, req, "read", &fun_val);

	//create arg
#if NAPI_VERSION >= 7
	status = napi_create_external_arraybuffer(napi_sess->env, buffer, buffer_size, NULL, NULL, &ab);
#else
	void *buf_data=NULL;
	status = napi_create_arraybuffer(napi_sess->env, buffer_size, &buf_data, &ab);
#endif

	if (status==napi_ok)
		status = napi_create_typedarray(napi_sess->env, napi_uint8_array, buffer_size, ab, 0, &arg);

	//call
	if (status==napi_ok)
		status = napi_call_function(napi_sess->env, req, fun_val, 1, &arg, &res);

	if (status==napi_ok)
		status = napi_get_value_uint32(napi_sess->env, res, &nb_bytes);

#if NAPI_VERSION >= 7
	//detach array buffer
	napi_detach_arraybuffer(napi_sess->env, ab);
#else
	if ((status==napi_ok) && buf_data && nb_bytes)
		memcpy(buffer, buf_data, nb_bytes);
#endif
	return nb_bytes;
}
static u32 httpout_write_cbk(void *udta, const u8 *buffer, u32 buffer_size)
{
	napi_value req;
	napi_value fun_val, arg, ab, res;
	u32 nb_bytes=0;
	napi_status status;
	NAPI_HTTPSession *napi_sess = (NAPI_HTTPSession *)udta;

	//call
	napi_get_reference_value(napi_sess->env, napi_sess->req_ref, &req);
	napi_get_named_property(napi_sess->env, req, "write", &fun_val);

	//create arg
#if NAPI_VERSION >= 7
	status = napi_create_external_arraybuffer(napi_sess->env, (void *)buffer, buffer_size, NULL, NULL, &ab);
#else
	void *buf_data=NULL;
	status = napi_create_arraybuffer(napi_sess->env, buffer_size, &buf_data, &ab);
	if (status == napi_ok) {
		memcpy(buf_data, buffer, buffer_size);
	}
#endif

	if (status==napi_ok)
		status = napi_create_typedarray(napi_sess->env, napi_uint8_array, buffer_size, ab, 0, &arg);

	//call
	if (status==napi_ok)
		status = napi_call_function(napi_sess->env, req, fun_val, 1, &arg, &res);

	if (status==napi_ok)
		status = napi_get_value_uint32(napi_sess->env, res, &nb_bytes);

#if NAPI_VERSION >= 7
	//detach arraybuffer
	napi_detach_arraybuffer(napi_sess->env, ab);
#endif

	if (status!=napi_ok) return 0;
	return nb_bytes;
}
static void httpout_on_close_cbk(void *udta, GF_Err reason)
{
	napi_value req;
	NAPI_HTTPSession *napi_sess = (NAPI_HTTPSession *)udta;

	//call
	napi_get_reference_value(napi_sess->env, napi_sess->req_ref, &req);

	Bool has_p = GF_FALSE;
	napi_has_named_property(napi_sess->env, req, "close", (bool *) &has_p);
	if (has_p) {
		napi_value fun_val, arg, res;
		napi_get_named_property(napi_sess->env, req, "close", &fun_val);
		napi_create_uint32(napi_sess->env, reason, &arg);
		napi_call_function(napi_sess->env, req, fun_val, 1, &arg, &res);
	}
	napi_delete_reference(napi_sess->env, napi_sess->req_ref);
}

GF_Err gf_httpout_send_request(void *sess, void *udta,
	u32 reply,
	char *body_or_file,
	u32 nb_headers,
	const char **headers,
	u32 (*throttle)(void *udta, u64 done, u64 total),
	s32 (*read)(void *udta, u8 *buffer, u32 buffer_size),
	u32 (*write)(void *udta, const u8 *buffer, u32 buffer_size),
	void (*close)(void *udta, GF_Err reason)
);

napi_value httpout_send_cbk(napi_env env, napi_callback_info info)
{
	napi_value val;
	u32 i, reply=0;
	size_t size;
	char *body_or_file=NULL;
	u32 nb_headers=0;
	char **headers=NULL;
	Bool use_throttle=GF_FALSE;
	Bool use_read=GF_FALSE;
	Bool use_write=GF_FALSE;

	NARG_THIS

	NAPI_HTTPSession *napi_hsess;
	bool _tag;
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &httpsess_tag, &_tag) );
	if (!_tag) {
		napi_throw_error(env, NULL, "Not a HTTP session object");
		return NULL;
	}
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &napi_hsess) );

	napi_get_named_property(env, this_val, "reply", &val);
	napi_get_value_uint32(env, val, &reply);

	Bool has_p = GF_FALSE;
	napi_has_named_property(env, this_val, "body", (bool *) &has_p);
	if (has_p) {
		napi_get_named_property(env, this_val, "body", &val);
		napi_get_value_string_utf8(env, val, NULL, 0, &size);
		body_or_file = gf_malloc(sizeof(char) * (size+1) );
		napi_get_value_string_utf8(env, val, body_or_file, size+1, &size);
		body_or_file[size]=0;
	}

	napi_has_named_property(env, this_val, "headers_out", (bool *) &has_p);
	if (has_p) {
		napi_value hdrs;
		u32 nb_items;
		napi_get_named_property(env, this_val, "headers_out", &hdrs);
		napi_get_array_length(env, hdrs, &nb_items);
		for (i=0; i<nb_items; i++) {
			napi_value h;
			napi_get_element(env, hdrs, i, &h);
			napi_has_named_property(env, h, "name", (bool *) &has_p);
			if (!has_p) continue;
			napi_has_named_property(env, h, "value", (bool *) &has_p);
			if (!has_p) continue;

			headers = gf_realloc(headers, sizeof(char *) * (nb_headers+2));

			napi_get_named_property(env, h, "name", &val);
			napi_get_value_string_utf8(env, val, NULL, 0, &size);
			headers[nb_headers] = gf_malloc(sizeof(char) * (size+1) );
			napi_get_value_string_utf8(env, val, headers[nb_headers], size+1, &size);
			headers[nb_headers][size]=0;

			napi_get_named_property(env, h, "value", &val);
			napi_get_value_string_utf8(env, val, NULL, 0, &size);
			headers[nb_headers+1] = gf_malloc(sizeof(char) * (size+1) );
			napi_get_value_string_utf8(env, val, headers[nb_headers+1], size+1, &size);
			headers[nb_headers+1][size]=0;
			nb_headers += 2;
		}
	}

	napi_has_named_property(env, this_val, "throttle", (bool *) &has_p);
	if (has_p) use_throttle = GF_TRUE;
	napi_has_named_property(env, this_val, "read", (bool *) &has_p);
	if (has_p) use_read = GF_TRUE;
	napi_has_named_property(env, this_val, "write", (bool *) &has_p);
	if (has_p) use_write = GF_TRUE;

	gf_httpout_send_request(napi_hsess->httpout_session, napi_hsess, reply, body_or_file, nb_headers, (const char**) headers,
		use_throttle ? httpout_throttle_cbk : NULL,
		use_read ? httpout_read_cbk : NULL,
		use_write ? httpout_write_cbk : NULL,
		httpout_on_close_cbk);

	if (body_or_file) gf_free(body_or_file);
	if (headers) {
		for (i=0; i<nb_headers; i++) {
			gf_free(headers[i]);
		}
		gf_free(headers);
	}
	return NULL;
}

s32 httpout_on_request(void *udta, void *session, const char *method, const char *url, u32 auth_code, u32 nb_hdrs, const char **hdrs)
{
	napi_status status;
	napi_value req_obj;
	u32 i;
	napi_value obj, fun_val, res;
	NAPI_HTTPOut *napi_http = (NAPI_HTTPOut *)udta;
	NAPI_HTTPSession *napi_sess;

	//create request object
	GF_SAFEALLOC(napi_sess, NAPI_HTTPSession);
	if (!napi_sess) return 1;
	napi_sess->httpout_session = session;
	napi_sess->env = napi_http->env;

	napi_create_object(napi_http->env, &req_obj);
	napi_create_reference(napi_http->env, req_obj, 1, &napi_sess->req_ref);
	napi_type_tag_object(napi_http->env, req_obj, &httpsess_tag);
	napi_wrap(napi_http->env, req_obj, napi_sess, napi_http_del, NULL, NULL);

	//set properties
	napi_value val;
	status = napi_create_string_utf8(napi_http->env, method, NAPI_AUTO_LENGTH, &val);
	if (status==napi_ok) status = napi_set_named_property(napi_http->env, req_obj, "method", val);
	if (status!=napi_ok) return 1;

	status = napi_create_string_utf8(napi_http->env, url, NAPI_AUTO_LENGTH, &val);
	if (status==napi_ok) status = napi_set_named_property(napi_http->env, req_obj, "url", val);
	if (status!=napi_ok) return 1;

	status = napi_create_uint32(napi_http->env, auth_code, &val);
	if (status==napi_ok) status = napi_set_named_property(napi_http->env, req_obj, "auth_code", val);
	if (status!=napi_ok) return 1;

	status = napi_create_uint32(napi_http->env, 0, &val);
	if (status==napi_ok) status = napi_set_named_property(napi_http->env, req_obj, "reply", val);
	if (status!=napi_ok) return 1;

	status = napi_create_function(napi_http->env, "send", NAPI_AUTO_LENGTH, httpout_send_cbk, napi_http, &val);
	if (status==napi_ok) status = napi_set_named_property(napi_http->env, req_obj, "send", val);
	if (status!=napi_ok) return 1;

	napi_value in_headers;
	napi_create_array(napi_http->env, &in_headers);
	for (i=0; i<nb_hdrs;i+=2) {
		napi_value h;
		napi_create_object(napi_http->env, &h);
		napi_create_string_utf8(napi_http->env, hdrs[i], NAPI_AUTO_LENGTH, &val);
		napi_set_named_property(napi_http->env, h, "name", val);
		napi_create_string_utf8(napi_http->env, hdrs[i+1], NAPI_AUTO_LENGTH, &val);
		napi_set_named_property(napi_http->env, h, "value", val);
		napi_set_element(napi_http->env, in_headers, i/2, h);
	}
	napi_set_named_property(napi_http->env, req_obj, "headers_in", in_headers);

	napi_value out_headers;
	napi_create_array(napi_http->env, &out_headers);
	napi_set_named_property(napi_http->env, req_obj, "headers_out", out_headers);

	//call
	napi_get_reference_value(napi_http->env, napi_http->ref, &obj);
	status = napi_get_named_property(napi_http->env, obj, "on_request", &fun_val);
	if (status!=napi_ok) return 1;
	napi_call_function(napi_http->env, obj, fun_val, 1, &req_obj, &res);
	return 0;
}

GF_Err gf_filter_bind_httpout_callbacks(GF_Filter *filter, void *udta,
	s32 (*on_request)(void *udta, void *session, const char *method, const char *url, u32 auth_code, u32 nb_hdrs, const char **hdrs)
);

napi_value httpout_bind(napi_env env, napi_callback_info info)
{
	GF_Err e;
	NAPI_HTTPOut *napi_http;

	NARG_ARGS_THIS(1,1)
	FILTER

	NAPI_Filter *napi_f = (NAPI_Filter *) gf_filter_get_rt_udta(f);

	Bool has_fun = GF_FALSE;
	napi_has_named_property(napi_f->env, argv[0], "on_request", (bool *) &has_fun);
	if (!has_fun) {
		napi_throw_error(env, NULL, "Invalid httpout binding object, missing on_request callback");
		return NULL;
	}

	GF_SAFEALLOC(napi_http, NAPI_HTTPOut);
	if (!napi_http) {
		napi_throw_error(env, NULL, "Failed to allocate httpout binding");
		return NULL;
	}

	napi_create_reference(env, argv[0], 1, &napi_f->binding_ref);
	napi_http->ref = napi_f->binding_ref;
	napi_http->env = env;
	napi_f->binding_stack = napi_http;

	napi_type_tag_object(env, argv[0], &dashbind_tag);
	NAPI_CALL( napi_wrap(env, argv[0], napi_http, httpoutbind_finalize, NULL, NULL) );

	e = gf_filter_bind_httpout_callbacks(f, napi_http, httpout_on_request);
	if (e) {
		u32 rcount;
		napi_reference_unref(env, napi_f->binding_ref, &rcount);
		napi_delete_reference(env, napi_f->binding_ref);
		napi_f->binding_ref = NULL;
		napi_f->binding_stack = NULL;
		gf_free(napi_http);

		napi_throw_error(env, gf_error_to_string(e), "Failed to bind httpout callbacks");
		return NULL;
	}
	//tag httpout filter to run on main thread only
	gf_filter_force_main_thread(napi_f->f, GF_TRUE);
	return NULL;
}

void httpout_detach_binding(NAPI_Filter *napi_f)
{
	NAPI_HTTPOut *napi_http = (NAPI_HTTPOut *) napi_f->binding_stack;
	if (!napi_http) return;
	gf_free(napi_http);
}

typedef struct _napi_fio
{
	//points to factory object (passed in FileIO constructor)
	struct _napi_fio *factory;
	GF_FileIO *gfio;
	napi_ref ref;
	u32 nb_refs;

	//for root only
	napi_env env;
	GF_List *pending_urls;
	//GF_List *gc_exclude;
	u32 all_refs;
	napi_ref root_ref;
	napi_async_context async_ctx;
	Bool direct_mem;
} NAPI_FileIO;

static u32 FIO_PROP_URL = 0;

#define FILEIO\
	NAPI_FileIO *napi_fio=NULL;\
	bool _tag;\
	NAPI_CALL( napi_check_object_type_tag(env, this_val, &fileio_tag, &_tag) );\
	if (!_tag) {\
		napi_throw_error(env, NULL, "Not a FileIO object");\
		return NULL;\
	}\
	NAPI_CALL( napi_unwrap(env, this_val, (void**) &napi_fio) );\

napi_value fio_getter(napi_env env, napi_callback_info info)
{
	void *magic;
	napi_status status;
	napi_value this_val, ret;
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, &magic) );
	FILEIO

	if (magic == &FIO_PROP_URL) {
		NAPI_CALL( napi_create_string_utf8(env, gf_fileio_url(napi_fio->gfio), NAPI_AUTO_LENGTH, &ret) );
		return ret;
	}
	return NULL;
}

static GF_Err gfio_seek(GF_FileIO *fileio, u64 offset, s32 whence)
{
	napi_status status;
	s32 rval;
	napi_value args[2], fact_obj, obj, fun_val, res;
	NAPI_FileIO *napi_fio = gf_fileio_get_udta(fileio);
	napi_env env = napi_fio->factory->env;

	//get fun and target this
	napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
	status = napi_get_named_property(env, fact_obj, "seek", &fun_val);
	if (status!=napi_ok) {
		return GF_NOT_SUPPORTED;
	}
	napi_get_reference_value(env, napi_fio->ref, &obj);

	//create args
	status = napi_create_int64(env, (s64) offset, &args[0]);
	if (status != napi_ok) {
		return -1;
	}
	status = napi_create_uint32(env, whence, &args[1]);
	if (status != napi_ok) {
		return -1;
	}

	//call
	status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 2, args, &res);
	if (status!=napi_ok) {
		return -1;
	}
	status = napi_get_value_int32(env, res, &rval);
	if (status!=napi_ok) {
		return -1;
	}
    return rval;
}

/*
	ultimately we would like node to read/write directly to/from our buffer,
	however this currently crashes node
		https://github.com/nodejs/node/pull/33321#issuecomment-630905537

	"If you use napi_create_external_buffer() a second time on the same pointer without waiting for the buffer created from the first time to be released, you’ll get a crash"

	the same applies with napi_create_external_arraybuffer(), and we are usually called
	with the same internal buffer pointer for read or write methods

	until fixed, we create a different buffer and do a memcpy...
*/

static u32 gfio_read(GF_FileIO *fileio, u8 *buffer, u32 bytes)
{
	napi_status status;
	u32 nb_bytes;
	void *buf_data=NULL;

	napi_value arg, ab, fact_obj, obj, fun_val, res;
	NAPI_FileIO *napi_fio = gf_fileio_get_udta(fileio);
	napi_env env = napi_fio->factory->env;

	//get fun and target this
	status = napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
	if (status!=napi_ok) return 0;
	status = napi_get_named_property(env, fact_obj, "read", &fun_val);
	if (status!=napi_ok) return 0;
	status = napi_get_reference_value(env, napi_fio->ref, &obj);
	if (status!=napi_ok) return 0;

	//create arg
#if NAPI_VERSION >= 7
	if (napi_fio->factory->direct_mem)
		status = napi_create_external_arraybuffer(env, buffer, bytes, NULL, NULL, &ab);
	else
#endif
		status = napi_create_arraybuffer(env, bytes, &buf_data, &ab);

	if (status==napi_ok)
		status = napi_create_typedarray(env, napi_uint8_array, bytes, ab, 0, &arg);

	//call
	if (status==napi_ok)
		status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 1, &arg, &res);

	if (status==napi_ok)
		status = napi_get_value_uint32(env, res, &nb_bytes);

	if ((status==napi_ok) && !napi_fio->factory->direct_mem && buf_data && nb_bytes)
		memcpy(buffer, buf_data, nb_bytes);

#if NAPI_VERSION >= 7
	//detach array buffer
	if (napi_fio->factory->direct_mem)
		napi_detach_arraybuffer(env, ab);
#endif

	if (status!=napi_ok) return 0;
    return nb_bytes;
}

static u32 gfio_write(GF_FileIO *fileio, u8 *buffer, u32 bytes)
{
	napi_status status;
	u32 nb_bytes;

	napi_value arg, ab, fact_obj, obj, fun_val, res;
	NAPI_FileIO *napi_fio = gf_fileio_get_udta(fileio);
	napi_env env = napi_fio->factory->env;

	//get fun and target this
	status = napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
	if (status!=napi_ok) return 0;
	status = napi_get_named_property(env, fact_obj, "write", &fun_val);
	if (status!=napi_ok) return 0;
	status = napi_get_reference_value(env, napi_fio->ref, &obj);
	if (status!=napi_ok) return 0;

	//create arg
#if NAPI_VERSION >= 7
	if (napi_fio->factory->direct_mem) {
		status = napi_create_external_arraybuffer(env, buffer, bytes, NULL, NULL, &ab);
	} else
#endif
	{
		void *buf_data=NULL;
		status = napi_create_arraybuffer(env, bytes, &buf_data, &ab);
		if (status == napi_ok) {
			memcpy(buf_data, buffer, bytes);
		}
	}

	if (status==napi_ok)
		status = napi_create_typedarray(env, napi_uint8_array, bytes, ab, 0, &arg);

	//call
	if (status==napi_ok)
		status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 1, &arg, &res);

	if (status==napi_ok)
		status = napi_get_value_uint32(env, res, &nb_bytes);

#if NAPI_VERSION >= 7
	//detach arraybuffer
	if (napi_fio->factory->direct_mem)
		napi_detach_arraybuffer(env, ab);
#endif

	if (status!=napi_ok) return 0;

    return nb_bytes;
}

static s64 gfio_tell(GF_FileIO *fileio)
{
	napi_status status;
	s64 pos;
	napi_value fact_obj, obj, fun_val, res;
	NAPI_FileIO *napi_fio = gf_fileio_get_udta(fileio);
	napi_env env = napi_fio->factory->env;

	//get fun and target this
	napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
	status = napi_get_named_property(env, fact_obj, "tell", &fun_val);
	if (status!=napi_ok) {
		return -1;
	}
	napi_get_reference_value(env, napi_fio->ref, &obj);

	//call
	status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 0, NULL, &res);
	if (status!=napi_ok) {
		return -1;
	}
	status = napi_get_value_int64(env, res, &pos);
	if (status!=napi_ok) {
		return -1;
	}
    return pos;
}

static Bool gfio_eof(GF_FileIO *fileio)
{
	napi_status status;
	bool is_eof;
	napi_value fact_obj, obj, fun_val, res;
	NAPI_FileIO *napi_fio = gf_fileio_get_udta(fileio);
	napi_env env = napi_fio->factory->env;

	//get fun and target this
	napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
	status = napi_get_named_property(env, fact_obj, "eof", &fun_val);
	if (status!=napi_ok) {
		return GF_FALSE;
	}
	napi_get_reference_value(env, napi_fio->ref, &obj);

	//call
	status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 0, NULL, &res);
	if (status!=napi_ok) {
		return GF_FALSE;
	}
	status = napi_get_value_bool(env, res, &is_eof);
	if (status!=napi_ok) {
		return -1;
	}
    return is_eof ? GF_TRUE : GF_FALSE;
}

static GF_FileIO *gfio_open(GF_FileIO *fileio_ref, const char *url, const char *mode, GF_Err *out_error)
{
	napi_status status;
	char *fio_url = NULL;
	Bool do_delete = GF_FALSE;
	napi_value args[3], fact_obj, obj, fun_val, res;
	NAPI_FileIO *napi_fio = gf_fileio_get_udta(fileio_ref);
	napi_env env = napi_fio->factory->env;
	NAPI_GPAC

    *out_error = GF_OK;

	//make sure we are called from main thread
	if (gpac->main_thid != gf_th_id()) {
		*out_error = GF_NOT_SUPPORTED;
		return NULL;
	}

    if (!strcmp(mode, "url")) {
        NAPI_FileIO *new_gfio;
        char *path = gf_url_concatenate(gf_fileio_resource_url(fileio_ref), url);
        GF_SAFEALLOC(new_gfio, NAPI_FileIO);
        if (!new_gfio) {
			*out_error = GF_OUT_OF_MEM;
			return NULL;
		}
		new_gfio->factory = napi_fio->factory;
		new_gfio->gfio = gf_fileio_new(path, new_gfio, gfio_open, gfio_seek, gfio_read, gfio_write, gfio_tell, gfio_eof, NULL);
		gf_free(path);

		if (!new_gfio->gfio) {
			gf_free(new_gfio);
			*out_error = GF_OUT_OF_MEM;
			return NULL;
		}
		gf_fileio_tag_main_thread(new_gfio->gfio);
		gf_list_add(napi_fio->factory->pending_urls, new_gfio);
        //fio_ref.factory.gc_exclude.append(new_gfio)
        napi_fio->factory->all_refs += 1;
        return new_gfio->gfio;
	}
	if (!strcmp(mode, "ref")) {
		napi_fio->nb_refs += 1;
		napi_fio->factory->all_refs += 1;
        return fileio_ref;
	}

	if (!strcmp(mode, "probe")) {
		bool exists;
		//get fun and target this
		napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
		status = napi_get_named_property(env, fact_obj, "exists", &fun_val);
		if (status!=napi_ok) return NULL;

		//create arg
		status = napi_create_string_utf8(env, url, NAPI_AUTO_LENGTH, &args[0]);
		if (status!=napi_ok) return NULL;

		//call
		status = napi_make_callback(env, napi_fio->factory->async_ctx, fact_obj, fun_val, 1, &args[0], &res);
		if (status!=napi_ok) return NULL;

		status = napi_get_value_bool(env, res, &exists);
		if (status!=napi_ok) return NULL;

		if (!exists) *out_error = GF_URL_ERROR;
		return NULL;
	}

	if (!strcmp(mode, "unref")) {
		napi_fio->nb_refs -= 1;
		napi_fio->factory->all_refs -= 1;
		if (napi_fio->nb_refs) return fileio_ref;
		do_delete = GF_TRUE;
	}

	if (!strcmp(mode, "close")) {
		if (napi_fio->ref != NULL) {
			//get fun and target this
			napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
			status = napi_get_named_property(env, fact_obj, "close", &fun_val);
			napi_get_reference_value(env, napi_fio->ref, &obj);
			status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 0, NULL, &res);
            napi_fio->factory->all_refs -= 1;
		}

        if (napi_fio->nb_refs) return NULL;
        do_delete = GF_TRUE;
	}

    if (do_delete) {
		uint32_t res;
		NAPI_FileIO *factory = napi_fio->factory;
        //file no longer used, can now be garbage collected
        //fio_ref.factory.gc_exclude.remove(fio_ref)

        if (napi_fio->factory != napi_fio) {
            gf_fileio_del(napi_fio->gfio);
			napi_get_reference_value(env, napi_fio->ref, &obj);
			napi_reference_unref(env, napi_fio->ref, &res);
			napi_delete_reference(env, napi_fio->ref);
			gf_free(napi_fio);
		}

		//final cleanup
        if (!factory->all_refs && !gf_list_count(factory->pending_urls)) { // and not len(fio_ref.factory.gc_exclude):
            gf_fileio_del(factory->gfio);
			napi_get_reference_value(env, factory->ref, &obj);
			napi_reference_unref(env, factory->ref, &res);
			napi_delete_reference(env, factory->ref);

			napi_get_reference_value(env, factory->root_ref, &obj);
			napi_wrap(env, obj, NULL, NULL, NULL, NULL);
			napi_reference_unref(env, factory->root_ref, &res);
			napi_delete_reference(env, factory->root_ref);
			gf_list_del(factory->pending_urls);
			gf_free(factory);
		}
        return NULL;
	}


	//this is an open() call, check if associated object exists, if not so we can use this instance
	NAPI_FileIO *fio = NULL;
	if (napi_fio->ref == NULL) {
		fio = napi_fio;
		if (gf_list_find(napi_fio->factory->pending_urls, napi_fio)>=0) {
			napi_fio->factory->all_refs -= 1;
			gf_list_del_item(napi_fio->factory->pending_urls, napi_fio);
		}
	}

	//check in pending urls
    if (fio == NULL) {
		u32 i, count = gf_list_count(napi_fio->factory->pending_urls);
		for (i=0; i<count; i++) {
			NAPI_FileIO *afio = gf_list_get(napi_fio->factory->pending_urls, i);
            const char *a_url = gf_fileio_resource_url(afio->gfio);
            if (!strcmp(a_url, url)) {
                napi_fio->factory->all_refs -= 1;
                gf_list_rem(napi_fio->factory->pending_urls, i);
                fio = afio;
                break;
			}
		}
	}
	//we need to create a new FileIO
	Bool created = (fio==NULL) ? GF_TRUE : GF_FALSE;

	//read url, translate
	if (!strncmp(url, "gfio://", 7)) {
		url = gf_fileio_translate_url(url);
	}
	//if we create a new FileIO, we need to resolve url, against parent URL
	else if (created) {
		fio_url = gf_url_concatenate(gf_fileio_resource_url(napi_fio->gfio), url);
		url = fio_url;
	}

	//create new FileIO
	if (fio==NULL) {
        GF_SAFEALLOC(fio, NAPI_FileIO);
        if (!fio) {
			*out_error = GF_OUT_OF_MEM;
			gf_free(fio_url);
			return NULL;
		}
		fio->factory = napi_fio->factory;
		fio->gfio = gf_fileio_new((char *) url, fio, gfio_open, gfio_seek, gfio_read, gfio_write, gfio_tell, gfio_eof, NULL);

		if (!fio->gfio) {
			gf_free(fio);
			gf_free(fio_url);
			*out_error = GF_OUT_OF_MEM;
			return NULL;
		}
		gf_fileio_tag_main_thread(fio->gfio);
	}

    //create object, direct copy of the object passed at construction
	napi_create_object(env, &obj);
	napi_create_reference(env, obj, 1, &fio->ref);


	//get fun and target this
	napi_get_reference_value(env, napi_fio->factory->root_ref, &fact_obj);
	status = napi_get_named_property(env, fact_obj, "open", &fun_val);
	if (status==napi_ok) {
	//create arg
	status = napi_create_string_utf8(env, url, NAPI_AUTO_LENGTH, &args[0]);
	}

	if (status==napi_ok) {
	status = napi_create_string_utf8(env, mode, NAPI_AUTO_LENGTH, &args[1]);
	}

	args[2] = fact_obj;

	//call
	if (status==napi_ok) {
	status = napi_make_callback(env, napi_fio->factory->async_ctx, obj, fun_val, 3, args, &res);
	}

	bool open_res = false;
	if (status==napi_ok) {
	status = napi_get_value_bool(env, res, &open_res);
	}

	if (fio_url) gf_free(fio_url);

	if (open_res) {
		//if not fio in fio.factory.gc_exclude:
		//    fio.factory.gc_exclude.append(fio)
		fio->factory->all_refs += 1;
		return fio->gfio;
	}

    //failure to open a new created object, remove FileIO and let the object be garbage collected
    if (created) {
		gf_fileio_del(fio->gfio);
        fio->gfio = NULL;
	}
	*out_error = GF_URL_ERROR;
	return NULL;
}

void del_fio(napi_env env, void* finalize_data, void* finalize_hint)
{
}

napi_value new_fio(napi_env env, napi_callback_info info)
{
	napi_value fact_obj, fun_val;
	NAPI_FileIO *napi_fio;
	NARG_ARGS_THIS(3, 2);
	NARG_STR(url, 0, NULL);
	NARG_BOOL(direct_mem, 2, GF_TRUE);

	NAPI_GPAC
	gpac->is_init = GF_TRUE;

#if NAPI_VERSION < 7
	direct_mem = GF_FALSE;
#endif

	if (!url) {
		napi_throw_error(env, NULL, "FileIO must be constructed with a non-null URL");
		return NULL;
	}
	napi_valuetype rtype;
	fact_obj = argv[1];
	napi_typeof(env, fact_obj, &rtype);
	if (rtype != napi_object) {
		napi_throw_error(env, NULL, "factory is not an object");
		return NULL;
	}
	status = napi_get_named_property(env, fact_obj, "open", &fun_val);
	if (status != napi_ok) {
		napi_throw_error(env, NULL, "factory has no open function");
		return NULL;
	}
	status = napi_get_named_property(env, fact_obj, "close", &fun_val);
	if (status != napi_ok) {
		napi_throw_error(env, NULL, "factory has no close function");
		return NULL;
	}

	GF_SAFEALLOC(napi_fio, NAPI_FileIO);
	if (!napi_fio) {
		napi_throw_error(env, NULL, "Failed to allocate FileIO");
		return NULL;
	}

	napi_fio->gfio = gf_fileio_new(url, napi_fio, gfio_open, gfio_seek, gfio_read, gfio_write, gfio_tell, gfio_eof, NULL);
	if (!napi_fio->gfio) {
		gf_free(napi_fio);
		napi_throw_error(env, NULL, "Failed to allocate FileIO");
		return NULL;
	}
	gf_fileio_tag_main_thread(napi_fio->gfio);

	napi_create_reference(env, fact_obj, 1, &napi_fio->root_ref);
	napi_fio->env = env;
	napi_fio->factory = napi_fio;
	napi_fio->pending_urls = gf_list_new();
	napi_fio->direct_mem = direct_mem;

	napi_value async_name;
	napi_create_string_utf8(env, "AsyncFileIO", NAPI_AUTO_LENGTH, &async_name);
	napi_async_init(env, this_val, async_name, &napi_fio->async_ctx);


	napi_property_descriptor fio_base_properties[] = {
		{ "url", NULL, NULL, fio_getter, NULL, NULL, napi_enumerable, &FIO_PROP_URL},
	};
	NAPI_CALL( napi_define_properties(env, this_val, sizeof(fio_base_properties)/sizeof(napi_property_descriptor), fio_base_properties) );


	NAPI_CALL( napi_wrap(env, this_val, napi_fio, del_fio, NULL, NULL) );
	NAPI_CALL( napi_type_tag_object(env, this_val, &fileio_tag) );

	return this_val;
}


napi_status init_fio_class(napi_env env, napi_value exports, GPAC_NAPI *inst)
{
	napi_status status;
	napi_value cons;
	status = napi_define_class(env, "FileIO", NAPI_AUTO_LENGTH , new_fio, NULL, 0, NULL, &cons);
	if (status != napi_ok) return status;
	return napi_set_named_property(env, exports, "FileIO", cons);
}
