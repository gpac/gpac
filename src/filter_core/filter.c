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

//helper functions
void gf_void_del(void *p)
{
	gf_free(p);
}
void gf_filterpacket_del(void *p)
{
	GF_FilterPacket *pck=(GF_FilterPacket *)p;
	if (pck->data) gf_free(pck->data);
	gf_free(p);
}

static void gf_filter_parse_args(GF_Filter *filter, const char *args, GF_FilterArgType arg_type);

const char *gf_fs_path_escape_colon(GF_FilterSession *sess, const char *path)
{
	if (!path) return NULL;
	if (sess->sep_args != ':')
		return strchr(path, sess->sep_args);

	return gf_url_colon_suffix(path);
}

static const char *gf_filter_get_args_stripped(GF_FilterSession *fsess, const char *in_args, Bool is_dst)
{
	char szEscape[7];
	char *args_striped = NULL;
	if (in_args) {
		char szDst[5];
		if (is_dst) {
			sprintf(szDst, "dst%c", fsess->sep_name);
		} else {
			sprintf(szDst, "src%c", fsess->sep_name);
		}
		args_striped = strstr(in_args, szDst);
		if (args_striped) {
			args_striped = (char *)gf_fs_path_escape_colon(fsess, args_striped+4);
			if (args_striped) args_striped ++;
		} else {
			args_striped = (char *)in_args;
		}
	}
	sprintf(szEscape, "gpac%c", fsess->sep_args);
	if (args_striped && !strncmp(args_striped, szEscape, 5))
		return args_striped + 5;

	return args_striped;
}

const char *gf_filter_get_dst_args(GF_Filter *filter)
{
	return gf_filter_get_args_stripped(filter->session, filter->dst_args, GF_TRUE);
}

char *gf_filter_get_dst_name(GF_Filter *filter)
{
	char szDst[5];
	char *dst, *arg_sep, *res, *dst_args;
	sprintf(szDst, "dst%c", filter->session->sep_name);

	dst_args = filter->dst_args;
	if (!dst_args) {
		GF_FilterPid *outpid = gf_list_get(filter->output_pids, 0);
		if (outpid) dst_args = outpid->filter->dst_args;

		if (!dst_args) {
			GF_Filter *outf = gf_list_get(filter->destination_links, 0);
			if (!outf || !outf->dst_args)
				outf = gf_list_get(filter->destination_filters, 0);
			if (outf)
				dst_args = filter->dst_args;
		}
	}
	dst = dst_args ? strstr(dst_args, szDst) : NULL;
	if (!dst) return NULL;

	arg_sep = (char*) gf_fs_path_escape_colon(filter->session, dst+4);

	if (arg_sep) {
		arg_sep[0] = 0;
		res = gf_strdup(dst+4);
		arg_sep[0] = filter->session->sep_args;
	} else {
		res = gf_strdup(dst+4);
	}
	return res;
}

GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *freg, const char *src_args, const char *dst_args, GF_FilterArgType arg_type, GF_Err *err)
{
	char szName[200];
	const char *dst_striped = NULL;
	const char *src_striped = NULL;
	GF_Filter *filter;
	GF_Err e;
	u32 i;
	assert(fsess);

	GF_SAFEALLOC(filter, GF_Filter);
	if (!filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc filter for %s\n", freg->name));
		return NULL;
	}
	filter->freg = freg;
	filter->session = fsess;
	filter->max_extra_pids = freg->max_extra_pids;

	if (fsess->use_locks) {
		snprintf(szName, 200, "Filter%sPackets", filter->freg->name);
		filter->pcks_mx = gf_mx_new(szName);
	}

	//for now we always use a lock on the filter task lists
	//this mutex protects the task list and the number of process virtual tasks
	//we cannot remove it in non-threaded mode since we have no garantee that a filter won't use threading on its own
	snprintf(szName, 200, "Filter%sTasks", filter->freg->name);
	filter->tasks_mx = gf_mx_new(szName);

	filter->tasks = gf_fq_new(filter->tasks_mx);

	if (! (filter->session->flags & GF_FS_FLAG_NO_RESERVOIR)) {
		filter->pcks_shared_reservoir = gf_fq_new(filter->pcks_mx);
		filter->pcks_alloc_reservoir = gf_fq_new(filter->pcks_mx);
		filter->pcks_inst_reservoir = gf_fq_new(filter->pcks_mx);
	}

	filter->pending_pids = gf_fq_new(NULL);

	filter->blacklisted = gf_list_new();
	filter->destination_filters = gf_list_new();
	filter->destination_links = gf_list_new();

	filter->bundle_idx_at_resolution = -1;
	filter->cap_idx_at_resolution = -1;

	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);
	gf_list_add(fsess->filters, filter);
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);

	src_striped = src_args;
	dst_striped = NULL;
	switch (arg_type) {
	case GF_FILTER_ARG_EXPLICIT_SOURCE_NO_DST_INHERIT:
		filter->arg_type = arg_type = GF_FILTER_ARG_EXPLICIT_SOURCE;
		filter->no_dst_arg_inherit = GF_TRUE;
		break;
	case GF_FILTER_ARG_INHERIT_SOURCE_ONLY:
		filter->arg_type = arg_type = GF_FILTER_ARG_INHERIT;
		filter->no_dst_arg_inherit = GF_TRUE;
		break;
	default:
		filter->arg_type = arg_type;
		dst_striped = gf_filter_get_args_stripped(fsess, dst_args, GF_TRUE);
		break;
	}

	if ((arg_type!=GF_FILTER_ARG_EXPLICIT_SOURCE) && (arg_type!=GF_FILTER_ARG_EXPLICIT)) {
		src_striped = gf_filter_get_args_stripped(fsess, src_args, GF_FALSE);
	}

	//if we already concatenated our dst args to this source filter (eg this is an intermediate dynamically loaded one)
	//don't reappend the args
	if (dst_striped && src_striped && strstr(src_striped, dst_striped) != NULL) {
		dst_striped = NULL;
	}

	if (src_striped && dst_striped) {
		char *all_args;
		const char *dbsep;
		char *localarg_marker;
		u32 nb_db_sep=0;
		char szDBSep[3];
		Bool insert_escape = GF_FALSE;
		u32 len = 2 + (u32) strlen(src_striped) + (u32) strlen(dst_striped);
		//source has a URL (locat or not), escape it to make sure we don't pass dst args as params to the URL
		if ((strstr(src_striped, "src=") || strstr(src_striped, "dst=")) && strstr(src_striped, "://")){
			char szEscape[10];
			sprintf(szEscape, "%cgpac", fsess->sep_args);
			if (strstr(src_striped, szEscape)==NULL) {
				insert_escape = GF_TRUE;
				len += 5;
			}
		}

		szDBSep[0] = szDBSep[1] = filter->session->sep_args;
		szDBSep[2] = 0;
		dbsep = src_striped;
		while (dbsep) {
			char *next_dbsep = strstr(dbsep, szDBSep);
			if (!next_dbsep) break;
			nb_db_sep++;
			dbsep = next_dbsep+2;
		}
		if (nb_db_sep % 2) nb_db_sep=1;
		else nb_db_sep=0;

		all_args = gf_malloc(sizeof(char)*(len+nb_db_sep));
		if (!nb_db_sep) {
			szDBSep[1] = 0;
		}

		if (insert_escape) {
			sprintf(all_args, "%s%sgpac%c%s", src_striped, szDBSep, fsess->sep_args, dst_striped);
		} else {
			sprintf(all_args, "%s%s%s", src_striped, szDBSep, dst_striped);
		}
		localarg_marker = strstr(all_args, "locarg");
		if (localarg_marker) {
			localarg_marker[0]=0;
			if (strlen(all_args) && (localarg_marker[-1]==fsess->sep_args))
				localarg_marker[-1]=0;
		}
		e = gf_filter_new_finalize(filter, all_args, arg_type);
		filter->orig_args = all_args;
		src_striped = NULL;
	} else if (dst_striped) {
		e = gf_filter_new_finalize(filter, dst_striped, arg_type);
		filter->orig_args = gf_strdup(dst_striped);
		src_striped = NULL;
	} else {
		e = gf_filter_new_finalize(filter, src_striped, arg_type);
	}
	filter->dst_args = dst_args ? gf_strdup(dst_args) : NULL;

	if (e) {
		if (!filter->setup_notified) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Error %s while instantiating filter %s\n", gf_error_to_string(e),freg->name));
			gf_filter_setup_failure(filter, e);
		}
		if (err) *err = e;
		return NULL;
	}
	if (filter && src_striped) filter->orig_args = gf_strdup(src_striped);

	for (i=0; i<freg->nb_caps; i++) {
		if (freg->caps[i].flags & GF_CAPFLAG_OUTPUT) {
			filter->has_out_caps = GF_TRUE;
			break;
		}
	}
	if (filter) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Created filter register %s args %s\n", freg->name, filter->orig_args ? filter->orig_args : "none"));
	}
	return filter;
}


GF_Err gf_filter_new_finalize(GF_Filter *filter, const char *args, GF_FilterArgType arg_type)
{
	gf_filter_set_name(filter, NULL);

	gf_filter_parse_args(filter, args, arg_type);

	if (filter->freg->initialize) {
		GF_Err e;
		FSESS_CHECK_THREAD(filter)
		e = filter->freg->initialize(filter);
		if (e) return e;
	}
	//flush all pending pid init requests
	if (filter->has_pending_pids) {
		filter->has_pending_pids=GF_FALSE;
		while (gf_fq_count(filter->pending_pids)) {
			GF_FilterPid *pid=gf_fq_pop(filter->pending_pids);
			gf_filter_pid_post_init_task(filter, pid);
		}
	}
	return GF_OK;
}

void gf_filter_reset_pending_packets(GF_Filter *filter)
{
	//may happen when a filter is removed from the chain
	if (filter->postponed_packets) {
		while (gf_list_count(filter->postponed_packets)) {
			GF_FilterPacket *pck = gf_list_pop_front(filter->postponed_packets);
			gf_filter_packet_destroy(pck);
		}
		gf_list_del(filter->postponed_packets);
		filter->postponed_packets = NULL;
	}
}

static void reset_filter_args(GF_Filter *filter);

//when destroying the filter queue we have to skip tasks marked as notified, since they are also present in the
//session task list
void task_del(void *_task)
{
	GF_FSTask *task = _task;
	if (!task->notified) gf_free(task);
}

void gf_filter_del(GF_Filter *filter)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s destruction\n", filter->name));
	assert(filter);
//	assert(!filter->detached_pid_inst);
	assert(!filter->detach_pid_tasks_pending);
//	assert(!filter->swap_pidinst_dst);
	assert(!filter->swap_pidinst_src);

#ifndef GPAC_DISABLE_3D
	gf_list_del_item(filter->session->gl_providers, filter);
	gf_fs_check_gl_provider(filter->session);
#endif

#ifdef GPAC_MEMORY_TRACKING
	if (filter->session->check_allocs) {
		if (filter->max_nb_process>10 && (filter->max_nb_consecutive_process * 10 < filter->max_nb_process)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("\nFilter %s extensively uses memory alloc/free in process(): \n", filter->name));
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("\tmax stats of over %d calls (%d consecutive calls with no alloc/free):\n", filter->max_nb_process, filter->max_nb_consecutive_process));
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("\t\t%d allocs %d callocs %d reallocs %d free\n", filter->max_stats_nb_alloc, filter->max_stats_nb_calloc, filter->max_stats_nb_realloc, filter->max_stats_nb_free));
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("\tPlease consider rewriting the code\n"));
		}
	}
#endif

	//may happen when a filter is removed from the chain
	if (filter->postponed_packets) {
		while (gf_list_count(filter->postponed_packets)) {
			GF_FilterPacket *pck = gf_list_pop_front(filter->postponed_packets);
			gf_filter_packet_destroy(pck);
		}
		gf_list_del(filter->postponed_packets);
		filter->postponed_packets = NULL;
	}

	//delete output pids before the packet reservoir
	while (gf_list_count(filter->output_pids)) {
		gf_filter_pid_del(gf_list_pop_back(filter->output_pids));
	}
	gf_list_del(filter->output_pids);

	gf_list_del(filter->blacklisted);
	gf_list_del(filter->destination_filters);
	gf_list_del(filter->destination_links);
	gf_list_del(filter->source_filters);
	gf_list_del(filter->input_pids);
	gf_fq_del(filter->tasks, task_del);
	gf_fq_del(filter->pending_pids, NULL);

	reset_filter_args(filter);
	if (filter->src_args) gf_free(filter->src_args);

	if (filter->pcks_shared_reservoir)
		gf_fq_del(filter->pcks_shared_reservoir, gf_void_del);
	if (filter->pcks_inst_reservoir)
		gf_fq_del(filter->pcks_inst_reservoir, gf_void_del);
	if (filter->pcks_alloc_reservoir)
		gf_fq_del(filter->pcks_alloc_reservoir, gf_filterpacket_del);

	gf_mx_del(filter->pcks_mx);
	if (filter->tasks_mx)
		gf_mx_del(filter->tasks_mx);

	if (filter->id) gf_free(filter->id);
	if (filter->source_ids) gf_free(filter->source_ids);
	if (filter->dynamic_source_ids) gf_free(filter->dynamic_source_ids);
	if (filter->filter_udta) gf_free(filter->filter_udta);
	if (filter->orig_args) gf_free(filter->orig_args);
	if (filter->dst_args) gf_free(filter->dst_args);
	if (filter->name) gf_free(filter->name);
	if (filter->status_str) gf_free(filter->status_str);


	if (!filter->session->in_final_flush && !filter->session->run_status) {
		u32 i, count;
		gf_mx_p(filter->session->filters_mx);
		count = gf_list_count(filter->session->filters);
		for (i=0; i<count; i++) {
			GF_Filter *a_filter = gf_list_get(filter->session->filters, i);
			gf_list_del_item(a_filter->destination_filters, filter);
			gf_list_del_item(a_filter->destination_links, filter);
			gf_list_del_item(a_filter->source_filters, filter);
			if (a_filter->cap_dst_filter==filter)
				a_filter->cap_dst_filter = NULL;
			if (a_filter->cloned_from == filter)
				a_filter->cloned_from = NULL;
			if (a_filter->on_setup_error_filter == filter)
				a_filter->on_setup_error_filter = NULL;
			if (a_filter->target_filter == filter)
				a_filter->target_filter = NULL;
			if (a_filter->dst_filter == filter)
				a_filter->dst_filter = NULL;
		}
		gf_mx_v(filter->session->filters_mx);
	}
	gf_free(filter);
}

GF_EXPORT
void *gf_filter_get_udta(GF_Filter *filter)
{
	assert(filter);

	return filter->filter_udta;
}

GF_EXPORT
const char * gf_filter_get_name(GF_Filter *filter)
{
	assert(filter);
	return (const char *)filter->freg->name;
}

GF_EXPORT
void gf_filter_set_name(GF_Filter *filter, const char *name)
{
	assert(filter);

	if (filter->name) gf_free(filter->name);
	filter->name = gf_strdup(name ? name : filter->freg->name);
}

static void gf_filter_set_id(GF_Filter *filter, const char *ID)
{
	assert(filter);

	if (filter->id) gf_free(filter->id);
	filter->id = ID ? gf_strdup(ID) : NULL;
}

GF_EXPORT
void gf_filter_set_sources(GF_Filter *filter, const char *sources_ID)
{
	u32 old_len, len;
	char szS[2];
	assert(filter);
	if (!sources_ID) {
		if (filter->source_ids) gf_free(filter->source_ids);
		filter->source_ids = NULL;
		return;
	}
	if (!filter->source_ids) {
		filter->source_ids = gf_strdup(sources_ID);
		return;
	}
	old_len = (u32) strlen(filter->source_ids);
	len = old_len + (u32) strlen(sources_ID) + 2;
	filter->source_ids = gf_realloc(filter->source_ids, len);
	filter->source_ids[old_len] = 0;
	szS[0] = filter->session->sep_list;
	szS[1] = 0;
	strcat(filter->source_ids, szS);
	strcat(filter->source_ids, sources_ID);
}

static void gf_filter_set_arg(GF_Filter *filter, const GF_FilterArgs *a, GF_PropertyValue *argv)
{
#ifdef WIN32
	void *ptr = (void *) (((char*) filter->filter_udta) + a->offset_in_private);
#else
	void *ptr = filter->filter_udta + a->offset_in_private;
#endif
	Bool res = GF_FALSE;
	if (a->offset_in_private<0) return;

	switch (argv->type) {
	case GF_PROP_BOOL:
		if (a->offset_in_private + sizeof(Bool) <= filter->freg->private_size) {
			*(Bool *)ptr = argv->value.boolean;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_SINT:
		if (a->offset_in_private + sizeof(s32) <= filter->freg->private_size) {
			*(s32 *)ptr = argv->value.sint;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_UINT:
	case GF_PROP_PIXFMT:
	case GF_PROP_PCMFMT:
		if (a->offset_in_private + sizeof(u32) <= filter->freg->private_size) {
			*(u32 *)ptr = argv->value.uint;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_LSINT:
		if (a->offset_in_private + sizeof(s64) <= filter->freg->private_size) {
			*(s64 *)ptr = argv->value.longsint;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_LUINT:
		if (a->offset_in_private + sizeof(u64) <= filter->freg->private_size) {
			*(u64 *)ptr = argv->value.longuint;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_FLOAT:
		if (a->offset_in_private + sizeof(Fixed) <= filter->freg->private_size) {
			*(Fixed *)ptr = argv->value.fnumber;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_DOUBLE:
		if (a->offset_in_private + sizeof(Double) <= filter->freg->private_size) {
			*(Double *)ptr = argv->value.number;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_FRACTION:
		if (a->offset_in_private + sizeof(GF_Fraction) <= filter->freg->private_size) {
			*(GF_Fraction *)ptr = argv->value.frac;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_FRACTION64:
		if (a->offset_in_private + sizeof(GF_Fraction64) <= filter->freg->private_size) {
			*(GF_Fraction64 *)ptr = argv->value.lfrac;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_VEC2I:
		if (a->offset_in_private + sizeof(GF_PropVec2i) <= filter->freg->private_size) {
			*(GF_PropVec2i *)ptr = argv->value.vec2i;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_VEC2:
		if (a->offset_in_private + sizeof(GF_PropVec2) <= filter->freg->private_size) {
			*(GF_PropVec2 *)ptr = argv->value.vec2;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_VEC3I:
		if (a->offset_in_private + sizeof(GF_PropVec3i) <= filter->freg->private_size) {
			*(GF_PropVec3i *)ptr = argv->value.vec3i;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_VEC3:
		if (a->offset_in_private + sizeof(GF_PropVec3) <= filter->freg->private_size) {
			*(GF_PropVec3 *)ptr = argv->value.vec3;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_VEC4I:
		if (a->offset_in_private + sizeof(GF_PropVec4i) <= filter->freg->private_size) {
			*(GF_PropVec4i *)ptr = argv->value.vec4i;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_VEC4:
		if (a->offset_in_private + sizeof(GF_PropVec4) <= filter->freg->private_size) {
			*(GF_PropVec4 *)ptr = argv->value.vec4;
			res = GF_TRUE;
		}
		break;

	case GF_PROP_NAME:
	case GF_PROP_STRING:
		if (a->offset_in_private + sizeof(char *) <= filter->freg->private_size) {
			if (*(char **)ptr) gf_free( * (char **)ptr);
			//we don't strdup since we don't free the string at the caller site
			*(char **)ptr = argv->value.string;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_DATA:
	case GF_PROP_DATA_NO_COPY:
	case GF_PROP_CONST_DATA:
		if (a->offset_in_private + sizeof(GF_PropData) <= filter->freg->private_size) {
			GF_PropData *pd = (GF_PropData *) ptr;
			if ((argv->type!=GF_PROP_CONST_DATA) && pd->ptr) gf_free(pd->ptr);
			//we don't free/alloc  since we don't free the string at the caller site
			pd->size = argv->value.data.size;
			pd->ptr = argv->value.data.ptr;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_POINTER:
		if (a->offset_in_private + sizeof(void *) <= filter->freg->private_size) {
			*(void **)ptr = argv->value.ptr;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_STRING_LIST:
		if (a->offset_in_private + sizeof(void *) <= filter->freg->private_size) {
			GF_List *l = *(GF_List **)ptr;
			if (l) {
				while (gf_list_count(l)) {
					char *s = gf_list_pop_back(l);
					gf_free(s);
				}
				gf_list_del(l);
			}
			//we don't clone since we don't free the string at the caller site
			*(GF_List **)ptr = argv->value.string_list;
			res = GF_TRUE;
		}
		break;
	case GF_PROP_UINT_LIST:
		if (a->offset_in_private + sizeof(void *) <= filter->freg->private_size) {
			GF_PropUIntList *l = (GF_PropUIntList *)ptr;
			if (l) gf_free(l->vals);
			*l = argv->value.uint_list;
			res = GF_TRUE;
		}
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Property type %s not supported for filter argument\n", gf_props_get_type_name(argv->type) ));
		return;
		break;
	}
	if (!res) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to set argument %s: memory offset %d overwrite structure size %f\n", a->arg_name, a->offset_in_private, filter->freg->private_size));
	}
}

static GF_PropertyValue gf_filter_parse_prop_solve_env_var(GF_Filter *filter, u32 type, const char *name, const char *value, const char *enum_values)
{
	char szPath[GF_MAX_PATH];
	GF_PropertyValue argv;

	if (!value) return gf_props_parse_value(type, name, NULL, enum_values, filter->session->sep_list);


	if (!strnicmp(value, "$GPAC_SHARED", 12)) {
		if (gf_get_default_shared_directory(szPath)) {
			strcat(szPath, value+12);
			value = szPath;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to query GPAC shared resource directory location\n"));
		}
	}
	else if (!strnicmp(value, "$GPAC_LANG", 15)) {
		value = gf_opts_get_key("core", "lang");
		if (!value) value = "en";
	}
	else if (!strnicmp(value, "$GPAC_UA", 15)) {
		value = gf_opts_get_key("core", "user-agent");
		if (!value) value = "GPAC " GPAC_VERSION;
	}
	argv = gf_props_parse_value(type, name, value, enum_values, filter->session->sep_list);
	return argv;
}

void gf_filter_update_arg_task(GF_FSTask *task)
{
	u32 i=0;
	Bool found = GF_FALSE;
	GF_FilterUpdate *arg=task->udta;
	//find arg
	i=0;
	while (task->filter->freg->args) {
		GF_PropertyValue argv;
		const GF_FilterArgs *a = &task->filter->freg->args[i];
		i++;
		if (!a || !a->arg_name) break;

		if (strcmp(a->arg_name, arg->name))
			continue;

		found = GF_TRUE;
		if (! (a->flags & GF_FS_ARG_UPDATE) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Argument %s of filter %s is not updatable - ignoring\n", a->arg_name, task->filter->name));
			break;
		}

		argv = gf_filter_parse_prop_solve_env_var(task->filter, a->arg_type, a->arg_name, arg->val, a->min_max_enum);

		if (argv.type != GF_PROP_FORBIDEN) {
			GF_Err e = GF_OK;
			FSESS_CHECK_THREAD(task->filter)
			//if no update function consider the arg OK
			if (task->filter->freg->update_arg) {
				e = task->filter->freg->update_arg(task->filter, arg->name, &argv);
			}
			if (e==GF_OK) {
				gf_filter_set_arg(task->filter, a, &argv);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s did not accept update of arg %s to value %s: %s\n", task->filter->name, arg->name, arg->val, gf_error_to_string(e) ));
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to parse argument %s value %s\n", a->arg_name, a->arg_default_val));
		}
		break;
	}

	if (!found) {
		if (arg->recursive) {
			u32 i;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Failed to locate argument %s in filter %s, propagating %s the filter chain\n", arg->name, task->filter->freg->name,
				arg->recursive & (GF_FILTER_UPDATE_UPSTREAM|GF_FILTER_UPDATE_DOWNSTREAM) ? "up and down" : ((arg->recursive & GF_FILTER_UPDATE_UPSTREAM) ? "up" : "down") ));

			GF_List *flist = gf_list_new();
			if (arg->recursive & GF_FILTER_UPDATE_UPSTREAM) {
				for (i=0; i<task->filter->num_output_pids; i++) {
					GF_FilterPid *pid = gf_list_get(task->filter->output_pids, i);
					if (gf_list_find(flist, pid->filter)<0) gf_list_add(flist, pid->filter);
				}
				for (i=0; i<gf_list_count(flist); i++) {
					GF_Filter *a_f = gf_list_get(flist, i);
					//only allow upstream propagation
					gf_fs_send_update(task->filter->session, NULL, a_f, arg->name, arg->val, GF_FILTER_UPDATE_UPSTREAM);
				}
				gf_list_reset(flist);

			}
			if (arg->recursive & GF_FILTER_UPDATE_DOWNSTREAM) {
				for (i=0; i<task->filter->num_input_pids; i++) {
					GF_FilterPidInst *pidi = gf_list_get(task->filter->input_pids, i);
					if (gf_list_find(flist, pidi->pid->filter)<0) gf_list_add(flist, pidi->pid->filter);
				}

				for (i=0; i<gf_list_count(flist); i++) {
					GF_Filter *a_f = gf_list_get(flist, i);
					//only allow downstream propagation
					gf_fs_send_update(task->filter->session, NULL, a_f, arg->name, arg->val, GF_FILTER_UPDATE_DOWNSTREAM);
				}
			}

			gf_list_del(flist);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Failed to locate argument %s in filter %s\n", arg->name, task->filter->freg->name));
		}
	}
	gf_free(arg->name);
	gf_free(arg->val);
	gf_free(arg);
}

static const char *gf_filter_load_arg_config(GF_FilterSession *session, const char *sec_name, const char *arg_name, const char *arg_val)
{
	Bool gf_sys_has_filter_global_args();
	const char *opt;

	//look in global args
	if (gf_sys_has_filter_global_args()) {
		u32 alen = (u32) strlen(arg_name);
		u32 i, nb_args = gf_sys_get_argc();
		for (i=0; i<nb_args; i++) {
			const char *arg = gf_sys_get_arg(i);
			if (arg[0]!='-') continue;
			if (arg[1]!='-') continue;
			if (!strncmp(arg+2, arg_name, alen)) {
				char *sep = strchr(arg, '=');
				gf_fs_push_arg(session, arg_name, GF_TRUE, 0);

				if (sep) return sep+1;
				//no arg value means boolean true
				else return "true";
			}
		}
	}

	//look in config file
	opt = gf_opts_get_key(sec_name, arg_name);
	if (opt)
		return opt;

	//ifce (used by socket and keep MP4Client behaviour: some options are set in MP4Client main apply them
	if (!strcmp(arg_name, "ifce")) {
		opt = gf_opts_get_key("core", "ifce");
		if (opt)
			return opt;
	}

	return arg_val;
}

static void gf_filter_load_meta_args_config(const char *sec_name, GF_Filter *filter)
{
	GF_PropertyValue argv;
	Bool gf_sys_has_filter_global_meta_args();
	u32 i, key_count = gf_opts_get_key_count(sec_name);

	FSESS_CHECK_THREAD(filter)

	for (i=0; i<key_count; i++) {
		Bool arg_found = GF_FALSE;
		u32 k=0;
		const char *arg_val, *arg_name = gf_opts_get_key_name(sec_name, i);
		//check if this is a regular arg, if so don't process it
		while (filter->freg->args) {
			const GF_FilterArgs *a = &filter->freg->args[k];
			if (!a || !a->arg_name) break;
			k++;
			if (!strcmp(a->arg_name, arg_name)) {
				arg_found = GF_TRUE;
				break;
			}
		}
		if (arg_found) continue;

		arg_val = gf_opts_get_key(sec_name, arg_name);
		if (!arg_val) continue;

		memset(&argv, 0, sizeof(GF_PropertyValue));
		argv.type = GF_PROP_STRING;
		argv.value.string = (char *) arg_val;
		filter->freg->update_arg(filter, arg_name, &argv);
	}
	if (!gf_sys_has_filter_global_meta_args()) return;

	key_count = gf_sys_get_argc();
	for (i=0; i<key_count; i++) {
		char szArg[100];
		GF_Err e;
		const char *sep, *arg = gf_sys_get_arg(i);
		if (arg[0] != '-') continue;
		if (arg[1] != '+') continue;
		arg+=2;
		sep = strchr(arg, '=');
		memset(&argv, 0, sizeof(GF_PropertyValue));
		argv.type = GF_PROP_STRING;
		if (sep) {
			strncpy(szArg, arg, sizeof(char)* (sep - arg) );
			argv.value.string = (char *) sep+1;
		} else {
			strncpy(szArg, arg, sizeof(char)* strlen(arg) );
		}
		e = filter->freg->update_arg(filter, szArg, &argv);
		gf_fs_push_arg(filter->session, szArg, (e==GF_OK) ? GF_TRUE : GF_FALSE, 2);
	}
}

static void gf_filter_parse_args(GF_Filter *filter, const char *args, GF_FilterArgType arg_type)
{
	u32 i=0;
	char szSecName[200];
	char szEscape[7];
	char szSrc[5], szDst[5];
	Bool has_meta_args = GF_FALSE;
	char *szArg=NULL;
	u32 alloc_len=1024;
	if (!filter) return;

	if (!filter->freg->private_size) {
		if (filter->freg->args && filter->freg->args[0].arg_name) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter with arguments but no private stack size, no arg passing\n"));
		}
	} else {
		filter->filter_udta = gf_malloc(filter->freg->private_size);
		if (!filter->filter_udta) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to allocate private data stack\n"));
			return;
		}
		memset(filter->filter_udta, 0, filter->freg->private_size);
	}

	sprintf(szEscape, "%cgpac%c", filter->session->sep_args, filter->session->sep_args);
	sprintf(szSrc, "src%c", filter->session->sep_name);
	sprintf(szDst, "dst%c", filter->session->sep_name);

	snprintf(szSecName, 200, "filter@%s", filter->freg->name);

	//instantiate all args with defauts value
	i=0;
	while (filter->freg->args) {
		GF_PropertyValue argv;
		const char *def_val;
		const GF_FilterArgs *a = &filter->freg->args[i];
		if (!a || !a->arg_name) break;
		i++;
		if (a->flags & GF_FS_ARG_META) {
			has_meta_args = GF_TRUE;
			continue;
		}
		def_val = gf_filter_load_arg_config(filter->session, szSecName, a->arg_name, a->arg_default_val);

		if (!def_val) continue;

		argv = gf_filter_parse_prop_solve_env_var(filter, a->arg_type, a->arg_name, def_val, a->min_max_enum);

		if (argv.type != GF_PROP_FORBIDEN) {
			if (a->offset_in_private>=0) {
				gf_filter_set_arg(filter, a, &argv);
			} else if (filter->freg->update_arg) {
				FSESS_CHECK_THREAD(filter)
				filter->freg->update_arg(filter, a->arg_name, &argv);
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to parse argument %s value %s\n", a->arg_name, a->arg_default_val));
		}
	}
	//handle meta filter options, not exposed in registry
	if (has_meta_args && filter->freg->update_arg)
		gf_filter_load_meta_args_config(szSecName, filter);


	if (args)
		szArg = gf_malloc(sizeof(char)*1024);

	//parse each arg
	while (args) {
		char *value;
		u32 len;
		Bool found=GF_FALSE;
		char *escaped = NULL;
		Bool opaque_arg = GF_FALSE;
		Bool absolute_url = GF_FALSE;
		Bool internal_url = GF_FALSE;
		Bool internal_arg = GF_FALSE;
		char *sep = NULL;

		//look for our arg separator - if arg[0] is also a separator, consider the entire string until next double sep as the parameter
		if (args[0] != filter->session->sep_args)
			sep = strchr(args, filter->session->sep_args);
		else {
			while (args[0] == filter->session->sep_args) {
				args++;
			}
			sep = (char *) args + 1;
			while (1) {
				sep = strchr(sep, filter->session->sep_args);
				if (!sep) break;
				if (sep && (sep[1]==filter->session->sep_args)) {
					break;
				}
				sep = sep+1;
			}
			opaque_arg = GF_TRUE;
		}

		if (!opaque_arg) {
			//we don't use gf_fs_path_escape_colon here because we also analyse whether the URL is internal or not, and we don't want to do that on each arg

			if (filter->session->sep_args == ':') {
				while (sep && !strncmp(sep, "://", 3)) {
					absolute_url = GF_TRUE;
					//filter internal url schemes
					if ((!strncmp(args, szSrc, 4) || !strncmp(args, szDst, 4) ) &&
						(!strncmp(args+4, "video://", 8)
						|| !strncmp(args+4, "audio://", 8)
						|| !strncmp(args+4, "av://", 5)
						|| !strncmp(args+4, "gmem://", 7)
						|| !strncmp(args+4, "gpac://", 7)
						|| !strncmp(args+4, "pipe://", 7)
						|| !strncmp(args+4, "tcp://", 6)
						|| !strncmp(args+4, "udp://", 6)
						|| !strncmp(args+4, "tcpu://", 7)
						|| !strncmp(args+4, "udpu://", 7)
						|| !strncmp(args+4, "atsc://", 7)
						)
					) {
						internal_url = GF_TRUE;
						sep = strchr(sep+3, ':');
						if (!strncmp(args+4, "tcp://", 6)
							|| !strncmp(args+4, "udp://", 6)
							|| !strncmp(args+4, "tcpu://", 7)
							|| !strncmp(args+4, "udpu://", 7)
						) {
							char *sep2 = sep ? strchr(sep+1, ':') : NULL;
							if (sep2) {
								u32 port = 0;;
								sep2[0] = 0;
								if (sscanf(sep+1, "%d", &port)!=1) {
									port = 0;
								}
								sep2[0] = ':';
								if (port) sep = sep2;
							}
						}

					} else {
						//get root /
						sep = strchr(sep+3, '/');
						//get first : after root
						if (sep) sep = strchr(sep+1, ':');
					}
				}

				//watchout for "C:\\" or "C:/"
				while (sep && (sep[1]=='\\' || sep[1]=='/')) {
					sep = strchr(sep+1, ':');
				}
				if (sep) {
					char *prev = sep-3;
					if (prev[0]=='T') {}
					else if (prev[1]=='T') { prev ++; }
					else { prev = NULL; }

					if (prev) {
						//assume date Th:m:s or Th:mm:s or Thh:m:s or Thh:mm:s
						if ((prev[2] == ':') && (prev[4] == ':')) sep = strchr(prev+5, ':');
						else if ((prev[2] == ':') && (prev[5] == ':')) sep = strchr(prev+6, ':');
						else if ((prev[3] == ':') && (prev[5] == ':')) sep = strchr(prev+6, ':');
						else if ((prev[3] == ':') && (prev[6] == ':')) sep = strchr(prev+7, ':');
					}
				}
			}
			if (sep) {
				escaped = strstr(sep, szEscape);
				if (escaped) sep = escaped;
			}

			if (sep && !strncmp(args, szSrc, 4) && !escaped && absolute_url && !internal_url) {
				Bool file_exists;
				sep[0]=0;
				if (!strcmp(args+4, "null")) file_exists = GF_TRUE;
				else if (!strncmp(args+4, "tcp://", 6)) file_exists = GF_TRUE;
				else if (!strncmp(args+4, "udp://", 6)) file_exists = GF_TRUE;
				else file_exists = gf_file_exists(args+4);

				if (!file_exists) {
					char *fsep = strchr(args+4, filter->session->sep_frag);
					if (fsep) {
						fsep[0] = 0;
						file_exists = gf_file_exists(args+4);
						fsep[0] = filter->session->sep_frag;
					}
				}
				sep[0]= filter->session->sep_args;
				if (!file_exists) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Non-escaped argument pattern \"%s\" in src %s, assuming arguments are part of source URL. Use src=PATH:gpac:ARGS to differentiate, or change separators\n", sep, args));
					sep = NULL;
				}
			}
		}


		//escape some XML inputs
		if (sep) {
			char *xml_start = strchr(args, '<');
			len = (u32) (sep-args);
			if (xml_start) {
				u32 xlen = (u32) (xml_start-args);
				if ((xlen < len) && (args[len-1] != '>')) {
					while (1) {
						sep = strchr(sep+1, filter->session->sep_args);
						if (!sep) {
							len = (u32) strlen(args);
							break;
						}
						len = (u32) (sep-args);
						if (args[len-1] != '>') break;
					}
				}

			}
		}
		else len = (u32) strlen(args);

		if (len>=alloc_len) {
			alloc_len = len+1;
			szArg = gf_realloc(szArg, sizeof(char)*len);
		}
		strncpy(szArg, args, len);
		szArg[len]=0;

		value = strchr(szArg, filter->session->sep_name);
		if (value) {
			value[0] = 0;
			value++;
		}

		if (szArg[0] == filter->session->sep_frag) {
			filter->user_pid_props = GF_TRUE;
			goto skip_arg;
		}

		if ((arg_type == GF_FILTER_ARG_INHERIT) && (!strcmp(szArg, "src") || !strcmp(szArg, "dst")) )
			goto skip_arg;

		i=0;
		while (filter->filter_udta && filter->freg->args) {
			Bool is_my_arg = GF_FALSE;
			Bool reverse_bool = GF_FALSE;
			const char *restricted=NULL;
			const GF_FilterArgs *a = &filter->freg->args[i];
			i++;
			if (!a || !a->arg_name) break;

			if (!strcmp(a->arg_name, szArg))
				is_my_arg = GF_TRUE;
			else if ( (szArg[0]==filter->session->sep_neg) && !strcmp(a->arg_name, szArg+1)) {
				is_my_arg = GF_TRUE;
				reverse_bool = GF_TRUE;
			}
			//little optim here, if no value check for enums
			else if (!value && a->min_max_enum && strchr(a->min_max_enum, '|') ) {
				char *arg_found = strstr(a->min_max_enum, szArg);
				if (arg_found) {
					char c = arg_found[strlen(szArg)];
					if (!c || (c=='|')) {
						is_my_arg = GF_TRUE;
						value = szArg;
					}
				}
			}

			if (is_my_arg) restricted = gf_opts_get_key_restricted(szSecName, a->arg_name);
			if (restricted) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Argument %s of filter %s is restricted to %s by system-wide configuration, ignoring\n", szArg, filter->freg->name, restricted));
				found=GF_TRUE;
				break;
			}

			if (is_my_arg) {
				GF_PropertyValue argv;
				found=GF_TRUE;

				argv = gf_filter_parse_prop_solve_env_var(filter, (a->flags & GF_FS_ARG_META) ? GF_PROP_STRING : a->arg_type, a->arg_name, value, a->min_max_enum);

				if (reverse_bool && (argv.type==GF_PROP_BOOL))
					argv.value.boolean = !argv.value.boolean;

				if (argv.type != GF_PROP_FORBIDEN) {
					if (a->offset_in_private>=0) {
						gf_filter_set_arg(filter, a, &argv);
					} else if (filter->freg->update_arg) {
						FSESS_CHECK_THREAD(filter)
						filter->freg->update_arg(filter, a->arg_name, &argv);

						if ((argv.type==GF_PROP_STRING) && argv.value.string)
							gf_free(argv.value.string);
					}
				}
				break;
			}
		}
		if (!strlen(szArg)) {
			found = GF_TRUE;
		} else if (!found) {
			if (!strcmp("FID", szArg)) {
				if (arg_type != GF_FILTER_ARG_INHERIT)
					gf_filter_set_id(filter, value);
				found = GF_TRUE;
				internal_arg = GF_TRUE;
			}
			else if (!strcmp("SID", szArg)) {
				if (arg_type!=GF_FILTER_ARG_INHERIT)
					gf_filter_set_sources(filter, value);
				found = GF_TRUE;
				internal_arg = GF_TRUE;
			} else if (!strcmp("clone", szArg)) {
				if ((arg_type==GF_FILTER_ARG_EXPLICIT_SINK) || (arg_type==GF_FILTER_ARG_EXPLICIT))
					filter->clonable=GF_TRUE;
				found = GF_TRUE;
				internal_arg = GF_TRUE;
			}
			//codec for generic enc load
			else if (!strcmp("c", szArg)) {
				found = GF_TRUE;
				internal_arg = GF_TRUE;
			} else if (!strcmp("N", szArg)) {
				gf_filter_set_name(filter, value);
				found = GF_TRUE;
				internal_arg = GF_TRUE;
			}
			//codec for generic enc load
			else if (!strcmp("gfreg", szArg)) {
				found = GF_TRUE;
				internal_arg = GF_TRUE;
			}
			else if (has_meta_args && filter->freg->update_arg) {
				GF_PropertyValue argv = gf_props_parse_value(GF_PROP_STRING, szArg, value, NULL, filter->session->sep_list);
				FSESS_CHECK_THREAD(filter)
				filter->freg->update_arg(filter, szArg, &argv);
				if (argv.value.string) gf_free(argv.value.string);
				found = GF_TRUE;
			}
		}

		if (!internal_arg && !opaque_arg)
			gf_fs_push_arg(filter->session, szArg, found, 0);

skip_arg:
		if (escaped) {
			args=sep+6;
		} else if (sep) {
			args=sep+1;
			if (opaque_arg)
				args += 1;
		} else {
			args=NULL;
		}
	}
	if (szArg) gf_free(szArg);
}

static void reset_filter_args(GF_Filter *filter)
{
	u32 i=0;
	//removed or no stack
	if (!filter->filter_udta) return;

	//instantiate all args with defauts value
	while (filter->freg->args) {
		GF_PropertyValue argv;
		const GF_FilterArgs *a = &filter->freg->args[i];
		i++;
		if (!a || !a->arg_name) break;

		if (a->arg_type != GF_PROP_FORBIDEN) {
			memset(&argv, 0, sizeof(GF_PropertyValue));
			argv.type = a->arg_type;
			gf_filter_set_arg(filter, a, &argv);
		}
	}
}

void gf_filter_check_output_reconfig(GF_Filter *filter)
{
	u32 i, j;
	//not needed
	if (!filter->reconfigure_outputs) return;
	filter->reconfigure_outputs = GF_FALSE;
	//check destinations of all output pids
	for (i=0; i<filter->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		for (j=0; j<pid->num_destinations; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
			//PID was reconfigured, update props
			if (pidi->reconfig_pid_props) {
				assert(pidi->props);
				if (pidi->props != pidi->reconfig_pid_props) {
					//unassign old property list and set the new one
					assert(pidi->props->reference_count);
					if (safe_int_dec(& pidi->props->reference_count) == 0) {
						//see \ref gf_filter_pid_merge_properties_internal for mutex
						gf_mx_p(pidi->pid->filter->tasks_mx);
						gf_list_del_item(pidi->pid->properties, pidi->props);
						gf_mx_v(pidi->pid->filter->tasks_mx);
						gf_props_del(pidi->props);
					}
					pidi->props = pidi->reconfig_pid_props;
					safe_int_inc( & pidi->props->reference_count );
				}
				pidi->reconfig_pid_props = NULL;
				gf_fs_post_task(filter->session, gf_filter_pid_reconfigure_task, pidi->filter, pid, "pidinst_reconfigure", NULL);
			}
		}
	}
}

static GF_FilterPidInst *filter_relink_get_upper_pid(GF_FilterPidInst *cur_pidinst, Bool *needs_flush)
{
	GF_FilterPidInst *pidinst = cur_pidinst;
	*needs_flush = GF_FALSE;
	//locate the true destination
	while (1) {
		GF_FilterPid *opid;
		if (pidinst->filter->num_input_pids != 1) break;
		if (pidinst->filter->num_output_pids != 1) break;
		//filter was explicitly loaded, cannot go beyond
		if (! pidinst->filter->dynamic_filter) break;
		opid = gf_list_get(pidinst->filter->output_pids, 0);
		assert(opid);
		//we have a fan-out, we cannot replace the filter graph after that point
		//this would affect the other branches of the upper graph
		if (opid->num_destinations != 1) break;
		pidinst = gf_list_get(opid->destinations, 0);

		if (gf_fq_count(pidinst->packets))
			(*needs_flush) = GF_TRUE;
	}
	return pidinst;
}

void gf_filter_relink_task(GF_FSTask *task)
{
	Bool needs_flush;
	GF_FilterPidInst *cur_pidinst = task->udta;
	/*GF_FilterPidInst *pidinst = */filter_relink_get_upper_pid(cur_pidinst, &needs_flush);
	if (needs_flush) {
		task->requeue_request = GF_TRUE;
		return;
	}
	//good do go, unprotect pid
	assert(cur_pidinst->detach_pending);
	safe_int_dec(&cur_pidinst->detach_pending);
	task->filter->removed = GF_FALSE;

 	gf_filter_relink_dst(cur_pidinst);
}

void gf_filter_relink_dst(GF_FilterPidInst *from_pidinst)
{
	GF_Filter *filter_dst;
	GF_FilterPid *link_from_pid = NULL;
	u32 min_chain_len = 0;
	Bool needs_flush = GF_FALSE;
	GF_FilterPidInst *src_pidinst = from_pidinst;
	GF_FilterPidInst *dst_pidinst;
	GF_Filter *cur_filter = from_pidinst->filter;

	//locate the true destination
	dst_pidinst = filter_relink_get_upper_pid(src_pidinst, &needs_flush);
	assert(dst_pidinst);

	//make sure we flush the end of the pipeline  !
	if (needs_flush) {
		cur_filter->removed = GF_TRUE;
		//prevent any fetch from pid
		safe_int_inc(&src_pidinst->detach_pending);
		gf_fs_post_task(cur_filter->session, gf_filter_relink_task, cur_filter, NULL, "relink_dst", src_pidinst);
		return;
	}
	filter_dst = dst_pidinst->filter;

	//walk down the filter chain and find the shortest path to our destination
	//stop when the current filter is not a one-to-one filter
	while (1) {
		u32 fchain_len;
		GF_FilterPidInst *an_inpid = NULL;
		if (cur_filter->num_input_pids>1) break;
		if (cur_filter->num_output_pids>1) break;

		an_inpid = gf_list_get(cur_filter->input_pids, 0);
		if (!an_inpid)
			break;

		if (gf_filter_pid_caps_match((GF_FilterPid *)an_inpid, filter_dst->freg, filter_dst, NULL, NULL, NULL, -1)) {
			link_from_pid = an_inpid->pid;
			break;
		}
		fchain_len = gf_filter_pid_resolve_link_length(an_inpid->pid, filter_dst);
		if (fchain_len && (!min_chain_len || (min_chain_len > fchain_len))) {
			min_chain_len = fchain_len;
			link_from_pid = an_inpid->pid;
		}
		cur_filter = an_inpid->pid->filter;
	}
	if (!link_from_pid) return;

	//detach the pidinst, and relink from the new input pid
	gf_filter_renegociate_output_dst(link_from_pid, link_from_pid->filter, filter_dst, dst_pidinst, src_pidinst);
}

void gf_filter_renegociate_output_dst(GF_FilterPid *pid, GF_Filter *filter, GF_Filter *filter_dst, GF_FilterPidInst *dst_pidi, GF_FilterPidInst *src_pidi)
{
	Bool is_new_chain = GF_TRUE;
	GF_Filter *new_f;
	Bool reconfig_only = src_pidi ? GF_FALSE: GF_TRUE;

	if (!filter_dst) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Internal error, lost destination for pid %s in filter %s while negotiating caps !!\n", pid->name, filter->name));
		return;
	}

	//try to load filters to reconnect output pid
	//we pass the source pid if given, so that we are sure the active property set is used to match the caps
	if (!reconfig_only && gf_filter_pid_caps_match(src_pidi ? (GF_FilterPid *)src_pidi : pid, filter_dst->freg, filter_dst, NULL, NULL, NULL, -1)) {
		GF_FilterPidInst *a_dst_pidi;
		new_f = pid->filter;
		assert(pid->num_destinations==1);
		a_dst_pidi = gf_list_get(pid->destinations, 0);
		//we are replacing the chain, remove filters until dest, keeping the final PID connected since we will detach
		// and reattach it
		if (!filter_dst->sticky) filter_dst->sticky = 2;
		gf_filter_remove_internal(a_dst_pidi->filter, filter_dst, GF_TRUE);
		is_new_chain = GF_FALSE;
		
		//we will reassign packets from that pid instance to the new connection
		filter_dst->swap_pidinst_dst = a_dst_pidi;

	}
	//we are inserting a new chain
	else if (reconfig_only) {
		new_f = gf_filter_pid_resolve_link_for_caps(pid, filter_dst);
	} else {
		Bool reassigned;
		new_f = gf_filter_pid_resolve_link(pid, filter_dst, &reassigned);
	}
	if (! new_f) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("No suitable filter to adapt caps between pid %s in filter %s to filter %s, disconnecting pid!\n", pid->name, filter->name, filter_dst->name));
		filter->session->last_connect_error = GF_FILTER_NOT_FOUND;

		if (pid->adapters_blacklist) {
			gf_list_del(pid->adapters_blacklist);
			pid->adapters_blacklist = NULL;
		}
		gf_fs_post_task(filter->session, gf_filter_pid_disconnect_task, dst_pidi->filter, dst_pidi->pid, "pidinst_disconnect", NULL);
		return;
	}
	//detach pid instance from its source pid
	if (dst_pidi) {
		//signal as detached, this will prevent any further packet access
		safe_int_inc(&dst_pidi->detach_pending);

		//we need to first reconnect the pid and then detach the output
		if (is_new_chain) {
			//signal a stream reset is pending to prevent filter entering endless loop
			safe_int_inc(&dst_pidi->filter->stream_reset_pending);
			//keep track of the pidinst being detached in the target filter
			new_f->swap_pidinst_dst = dst_pidi;
			//keep track of the pidinst being detached from the source filter
			new_f->swap_pidinst_src = src_pidi;
			new_f->swap_needs_init = GF_TRUE;
		}
		//we directly detach the pid
		else {
			safe_int_inc(&dst_pidi->pid->filter->detach_pid_tasks_pending);
			safe_int_inc(&filter_dst->detach_pid_tasks_pending);
			gf_fs_post_task(filter->session, gf_filter_pid_detach_task, filter_dst, dst_pidi->pid, "pidinst_detach", filter_dst);
		}
	}

	if (reconfig_only) {
		assert(pid->caps_negociate);
		new_f->caps_negociate = pid->caps_negociate;
		safe_int_inc(&new_f->caps_negociate->reference_count);
	}

	if (is_new_chain) {
		//mark this filter has having pid connection pending to prevent packet dispatch until the connection is done
		safe_int_inc(&pid->filter->out_pid_connection_pending);
		gf_filter_pid_post_connect_task(new_f, pid);
	} else {
		gf_fs_post_task(filter->session, gf_filter_pid_reconfigure_task, filter_dst, pid, "pidinst_reconfigure", NULL);
	}
}

Bool gf_filter_reconf_output(GF_Filter *filter, GF_FilterPid *pid)
{
	GF_Err e;
	GF_FilterPidInst *src_pidi = gf_list_get(filter->input_pids, 0);
	GF_FilterPid *src_pid = src_pidi->pid;
	if (filter->is_pid_adaptation_filter) {
		//do not remove from destination_filters, needed for end of pid_init task
		if (!filter->dst_filter) filter->dst_filter = gf_list_get(filter->destination_filters, 0);
		assert(filter->dst_filter);
		assert(filter->num_input_pids==1);
	}
	assert(filter->freg->reconfigure_output);
	//swap to pid
	pid->caps_negociate = filter->caps_negociate;
	filter->caps_negociate = NULL;
	e = filter->freg->reconfigure_output(filter, pid);


	if (e!=GF_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("PID Adaptation Filter %s output reconfiguration error %s, discarding filter and reloading new adaptation chain\n", filter->name, gf_error_to_string(e)));
		gf_filter_pid_retry_caps_negotiate(src_pid, pid, filter->dst_filter);
		return GF_FALSE;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("PID Adaptation Filter %s output reconfiguration OK (between filters %s and %s)\n", filter->name, src_pid->filter->name, filter->dst_filter->name));

	gf_filter_check_output_reconfig(filter);

	//success !
	if (src_pid->adapters_blacklist) {
		gf_list_del(pid->adapters_blacklist);
		src_pid->adapters_blacklist = NULL;
	}
	assert(pid->caps_negociate->reference_count);
	if (safe_int_dec(&pid->caps_negociate->reference_count) == 0) {
		gf_props_del(pid->caps_negociate);
	}
	pid->caps_negociate = NULL;
	if (filter->is_pid_adaptation_filter) {
		filter->dst_filter = NULL;
	}
	return GF_TRUE;
}

void gf_filter_reconfigure_output_task(GF_FSTask *task)
{
	GF_Filter *filter = task->filter;
	GF_FilterPid *pid;
	assert(filter->num_output_pids==1);
	pid = gf_list_get(filter->output_pids, 0);
	gf_filter_reconf_output(filter, pid);
}


static void gf_filter_renegociate_output(GF_Filter *filter)
{
	u32 i, j;
	assert(filter->nb_caps_renegociate );
	safe_int_dec(& filter->nb_caps_renegociate );
	for (i=0; i<filter->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		if (pid->caps_negociate) {
			Bool is_ok = GF_FALSE;

			//the property map is create with ref count 1

			//we cannot reconfigure output if more than one destination
			if ((pid->num_destinations<=1) && filter->freg->reconfigure_output) {
				GF_Err e = filter->freg->reconfigure_output(filter, pid);
				if (e) {
					if (filter->is_pid_adaptation_filter) {
						GF_FilterPidInst *src_pidi = gf_list_get(filter->input_pids, 0);
						GF_FilterPidInst *pidi = gf_list_get(pid->destinations, 0);

						GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("PID Adaptation Filter %s output reconfiguration error %s, discarding filter and reloading new adaptation chain\n", filter->name, gf_error_to_string(e)));

						assert(filter->num_input_pids==1);

						gf_filter_pid_retry_caps_negotiate(src_pidi->pid, pid, pidi->filter);

						continue;
					}
					GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s output reconfiguration error %s, loading filter chain for renegociation\n", filter->name, gf_error_to_string(e)));
				} else {
					is_ok = GF_TRUE;
					gf_filter_check_output_reconfig(filter);
				}
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s cannot reconfigure output pids, loading filter chain for renegociation\n", filter->name));
			}

			if (!is_ok) {
				GF_Filter *filter_dst;
				//we are currently connected to output
				if (pid->num_destinations) {
					for (j=0; j<pid->num_destinations; j++) {
						GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
						if (pid->caps_negociate_pidi != pidi) continue;
						filter_dst = pidi->filter;

						if (filter_dst->freg->reconfigure_output) {
							assert(!filter_dst->caps_negociate);
							filter_dst->caps_negociate = pid->caps_negociate;
							safe_int_inc(&filter_dst->caps_negociate->reference_count);

							gf_fs_post_task(filter->session, gf_filter_reconfigure_output_task, filter_dst, NULL, "filter renegociate", NULL);
						} else {
							//disconnect pid, but prevent filter from unloading
							if (!filter_dst->sticky) filter_dst->sticky = 2;
							gf_filter_renegociate_output_dst(pid, filter, filter_dst, pidi, NULL);
						}
					}
				}
				//we are deconnected (unload of a previous adaptation filter)
				else {
					filter_dst = pid->caps_dst_filter;
					assert(pid->num_destinations==0);
					pid->caps_dst_filter = NULL;
					gf_filter_renegociate_output_dst(pid, filter, filter_dst, NULL, NULL);
				}
			}
			assert(pid->caps_negociate->reference_count);
			if (safe_int_dec(&pid->caps_negociate->reference_count) == 0) {
				gf_props_del(pid->caps_negociate);
			}
			pid->caps_negociate = NULL;
			pid->caps_negociate_pidi = NULL;
		}
	}
}

static void gf_filter_check_pending_tasks(GF_Filter *filter, GF_FSTask *task)
{
	if (filter->session->run_status!=GF_OK) {
		return;
	}

	//lock task mx to take the decision whether to requeue a new task or not (cf gf_filter_post_process_task)
	//TODO: find a way to bypass this mutex ?
	gf_mx_p(filter->tasks_mx);

	assert(filter->scheduled_for_next_task);
	assert(filter->process_task_queued);
	if (safe_int_dec(&filter->process_task_queued) == 0) {
		//we have pending packets, auto-post and requeue
		if (filter->pending_packets) {
			safe_int_inc(&filter->process_task_queued);
			task->requeue_request = GF_TRUE;
		}
		//special case here: no more pending packets, filter has detected eos on one of its input but is still generating packet,
		//we reschedule (typically flush of decoder)
		else if (filter->eos_probe_state == 2) {
			safe_int_inc(&filter->process_task_queued);
			task->requeue_request = GF_TRUE;
			filter->eos_probe_state = 0;
		}
		//we are done for now
		else {
			task->requeue_request = GF_FALSE;
		}
	} else {
		//we have some more process() requests queued, requeue
		task->requeue_request = GF_TRUE;
	}
	if (task->requeue_request) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("[Filter] %s kept in scheduler blocking %d\n", filter->name, filter->would_block));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("[Filter] %s removed from scheduler - blocking %d\n", filter->name, filter->would_block));
	}
	gf_mx_v(filter->tasks_mx);
}

#ifdef GPAC_MEMORY_TRACKING
static GF_Err gf_filter_process_check_alloc(GF_Filter *filter)
{
	GF_Err e;
	u32 nb_allocs=0, nb_callocs=0, nb_reallocs=0, nb_free=0;
	u32 prev_nb_allocs=0, prev_nb_callocs=0, prev_nb_reallocs=0, prev_nb_free=0;

	//reset alloc/realloc stats of filter
	filter->session->nb_alloc_pck = 0;
	filter->session->nb_realloc_pck = 0;
	//get current alloc state
	gf_mem_get_stats(&prev_nb_allocs, &prev_nb_callocs, &prev_nb_reallocs, &prev_nb_free);
	e = filter->freg->process(filter);

	//get new alloc state
	gf_mem_get_stats(&nb_allocs, &nb_callocs, &nb_reallocs, &nb_free);
	//remove prev alloc stats
	nb_allocs -= prev_nb_allocs;
	nb_callocs -= prev_nb_callocs;
	nb_reallocs -= prev_nb_reallocs;
	nb_free -= prev_nb_free;

	//remove internal allocs/reallocs due to filter lib
	if (nb_allocs>filter->session->nb_alloc_pck)
		nb_allocs -= filter->session->nb_alloc_pck;
	else
		nb_allocs = 0;

	if (nb_reallocs>filter->session->nb_realloc_pck)
		nb_reallocs -= filter->session->nb_realloc_pck;
	else
		nb_reallocs = 0;

	//we now have nomber of allocs/realloc used by the filter internally during its process
	if (nb_allocs || nb_callocs || nb_reallocs /* || nb_free */) {
		filter->stats_nb_alloc += nb_allocs;
		filter->stats_nb_calloc += nb_callocs;
		filter->stats_nb_realloc += nb_reallocs;
		filter->stats_nb_free += nb_free;
	} else {
		filter->nb_consecutive_process ++;
	}
	filter->nb_process_since_reset++;
	return e;

}
#endif

static GFINLINE void check_filter_error(GF_Filter *filter, GF_Err e, Bool for_reconnection)
{
	Bool kill_filter = GF_FALSE;
	if (e>GF_OK) e = GF_OK;
	else if (e==GF_IP_NETWORK_EMPTY) e = GF_OK;

	if (e) {
		u64 diff;
		filter->session->last_process_error = e;

		filter->nb_errors ++;
		if (!filter->nb_consecutive_errors) filter->time_at_first_error = gf_sys_clock_high_res();

		filter->nb_consecutive_errors ++;
		if (filter->nb_pck_io) filter->nb_consecutive_errors = 0;
		//give it at most one second
		diff = gf_sys_clock_high_res() - filter->time_at_first_error;
		if (diff >= 1000000) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[Filter] %s in error / not responding properly: %d consecutive errors in "LLU" us with no packet discarded or sent\n\tdiscarding all inputs and notifying end of stream on all outputs\n", filter->name, filter->nb_consecutive_errors, diff));
			kill_filter = GF_TRUE;
		}
	} else {
		if ((!filter->nb_pck_io && filter->pending_packets && (filter->nb_pids_playing>0) ) || for_reconnection) {
			filter->nb_consecutive_errors++;

			if (filter->nb_consecutive_errors >= 100000) {
				if (for_reconnection) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[Filter] %s not responding properly: %d consecutive attempts at reconfiguring\n\tdiscarding all inputs and notifying end of stream on all outputs\n", filter->name, filter->nb_consecutive_errors));
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[Filter] %s not responding properly: %d consecutive process with no packet discarded or sent, but %d packets pending\n\tdiscarding all inputs and notifying end of stream on all outputs\n", filter->name, filter->nb_consecutive_errors, filter->pending_packets));
				}
				kill_filter = GF_TRUE;
			}
		} else {
			filter->nb_consecutive_errors = 0;
			filter->nb_pck_io = 0;
		}
	}

	if (kill_filter) {
		u32 i;
		for (i=0; i<filter->num_input_pids; i++) {
			GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, i);
			gf_filter_pid_set_discard((GF_FilterPid *)pidi, GF_TRUE);
		}
		for (i=0; i<filter->num_output_pids; i++) {
			GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
			gf_filter_pid_set_eos(pid);
		}
		filter->session->last_process_error = GF_SERVICE_ERROR;
		filter->disabled = GF_TRUE;
	}
}

static void gf_filter_process_task(GF_FSTask *task)
{
	GF_Err e;
	GF_Filter *filter = task->filter;
	Bool force_block_state_check=GF_FALSE;
	assert(task->filter);
	assert(filter->freg);
	assert(filter->freg->process);
	task->can_swap = GF_TRUE;

	assert(filter->process_task_queued);
	filter->schedule_next_time = 0;

	if (filter->disabled) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s is disabled, cancelling process\n", filter->name));
		gf_mx_p(task->filter->tasks_mx);
		task->filter->process_task_queued = 0;
		gf_mx_v(task->filter->tasks_mx);
		return;
	}

	if (filter->out_pid_connection_pending || filter->detached_pid_inst || filter->caps_negociate) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has %s pending, requeuing process\n", filter->name, filter->out_pid_connection_pending ? "connections" : filter->caps_negociate ? "caps negociation" : "input pid reassignments"));
		//do not cancel the process task since it might have been triggered by the filter itself,
		//we would not longer call it
		task->requeue_request = GF_TRUE;
		task->schedule_next_time = gf_sys_clock_high_res() + 10000;
		assert(filter->process_task_queued);
		check_filter_error(filter, GF_OK, GF_TRUE);
		return;
	}
	if (filter->removed || filter->finalized) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has been %s, skiping process\n", filter->name, filter->finalized ? "finalized" : "removed"));
		return;
	}

	//blocking filter: remove filter process task - task will be reinserted upon unblock()
	if (filter->would_block && (filter->would_block + filter->num_out_pids_not_connected == filter->num_output_pids ) ) {
		gf_mx_p(task->filter->tasks_mx);
		//it may happen that by the time we get the lock, the filter has been unblocked by another thread. If so, don't skip task
		if (filter->would_block) {
			filter->nb_tasks_done--;
			task->filter->process_task_queued = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s blocked, skiping process\n", filter->name));
			gf_mx_v(task->filter->tasks_mx);
			return;
		}
		gf_mx_v(task->filter->tasks_mx);
	}
	if (filter->stream_reset_pending) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has stream reset pending, postponing process\n", filter->name));
		filter->nb_tasks_done--;
		task->requeue_request = GF_TRUE;
		assert(filter->process_task_queued);
		return;
	}

	//the following breaks demuxers where PIDs are not all known from start: if we filter some pids due to user request,
	//we may end up with the following test true but not all PIDs yet declared, hence no more processing
	//we could add a filter cap for that, but for now we simply rely on the blocking mode algo only
#if 0
	if (filter->num_output_pids && (filter->num_out_pids_not_connected==filter->num_output_pids)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has no valid connected outputs, skiping process\n", filter->name));
		return;
	}
#endif

	//we have postponed packets on filter, flush them
	if (task->filter->postponed_packets) {
		while (gf_list_count(task->filter->postponed_packets)) {
			GF_FilterPacket *pck = gf_list_pop_front(task->filter->postponed_packets);
			GF_Err e = gf_filter_pck_send_internal(pck, GF_FALSE);
			if (e==GF_PENDING_PACKET) {
				//packet is pending so was added at the end of our postponed queue - remove from queue and reinsert in front
				gf_list_del_item(task->filter->postponed_packets, pck);
				gf_list_insert(task->filter->postponed_packets, pck, 0);
				task->requeue_request = GF_TRUE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s still has postponed packets, postponing process\n", filter->name));
				return;
			}
		}
		gf_list_del(task->filter->postponed_packets);
		task->filter->postponed_packets = NULL;
	}
	FSESS_CHECK_THREAD(filter)

	filter->nb_pck_io = 0;

	if (task->filter->nb_caps_renegociate) {
		gf_filter_renegociate_output(task->filter);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s process\n", filter->name));
	gf_rmt_begin_hash(filter->name, GF_RMT_AGGREGATE, &filter->rmt_hash);

#ifdef GPAC_MEMORY_TRACKING
	if (filter->session->check_allocs)
		e = gf_filter_process_check_alloc(filter);
	else
#endif
		e = filter->freg->process(filter);

	gf_rmt_end();

	//flush all pending pid init requests following the call to init
	if (filter->has_pending_pids) {
		filter->has_pending_pids=GF_FALSE;
		while (gf_fq_count(filter->pending_pids)) {
			GF_FilterPid *pid=gf_fq_pop(filter->pending_pids);
			gf_filter_pid_post_init_task(filter, pid);
		}
	}
	//no requeue if end of session
	if (filter->session->run_status != GF_OK) {
		return;
	}
	//if eos but we still have pending packets or proocess tasts queued, move to GF_OK so that
	//we evaluate the blocking state
	if (e==GF_EOS) {
		if (filter->postponed_packets) {
		 	e = GF_OK;
		} else if (filter->process_task_queued) {
			e = GF_OK;
			force_block_state_check = GF_TRUE;
		}
	}

	if ((e==GF_EOS) || filter->removed || filter->finalized) {
		gf_mx_p(filter->tasks_mx);
		filter->process_task_queued = 0;
		gf_mx_v(filter->tasks_mx);
		return;
	}
	check_filter_error(filter, e, GF_FALSE);

	//source filters, flush data if enough space available.
	if ( (!filter->num_output_pids || (filter->would_block + filter->num_out_pids_not_connected < filter->num_output_pids) )
		&& !filter->input_pids
		&& (e!=GF_EOS)
		&& !force_block_state_check
	) {
		if (filter->schedule_next_time)
			task->schedule_next_time = filter->schedule_next_time;
		task->requeue_request = GF_TRUE;
		assert(filter->process_task_queued);
	}
	//filter requested a requeue
	else if (filter->schedule_next_time) {
		if (!filter->session->in_final_flush) {
			task->schedule_next_time = filter->schedule_next_time;
			task->requeue_request = GF_TRUE;
			assert(filter->process_task_queued);
		}
	}
	//last task for filter but pending packets and not blocking, requeue in main scheduler
	else if ((filter->would_block < filter->num_output_pids)
			&& filter->pending_packets
			&& (gf_fq_count(filter->tasks)<=1)
	) {
		task->requeue_request = GF_TRUE;
		assert(filter->process_task_queued);
	}
	else {
		assert (!filter->schedule_next_time);
		gf_filter_check_pending_tasks(filter, task);
		if (task->requeue_request) {
			assert(filter->process_task_queued);
		}
	}
}

void gf_filter_process_inline(GF_Filter *filter)
{
	GF_Err e;
	if (filter->out_pid_connection_pending || filter->removed || filter->stream_reset_pending) {
		return;
	}
	if (filter->would_block && (filter->would_block == filter->num_output_pids) ) {
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s inline process\n", filter->name));

	if (filter->postponed_packets) {
		while (gf_list_count(filter->postponed_packets)) {
			GF_FilterPacket *pck = gf_list_pop_front(filter->postponed_packets);
			gf_filter_pck_send(pck);
		}
		gf_list_del(filter->postponed_packets);
		filter->postponed_packets = NULL;
		if (filter->process_task_queued==1) {
			gf_mx_p(filter->tasks_mx);
			filter->process_task_queued = 0;
			gf_mx_v(filter->tasks_mx);
			return;
		}
	}
	FSESS_CHECK_THREAD(filter)

#ifdef GPAC_MEMORY_TRACKING
	if (filter->session->check_allocs)
		e = gf_filter_process_check_alloc(filter);
	else
#endif
		e = filter->freg->process(filter);

	//flush all pending pid init requests following the call to init
	if (filter->has_pending_pids) {
		filter->has_pending_pids=GF_FALSE;
		while (gf_fq_count(filter->pending_pids)) {
			GF_FilterPid *pid=gf_fq_pop(filter->pending_pids);
			gf_filter_pid_post_init_task(filter, pid);
		}
	}
	//no requeue if end of session
	if (filter->session->run_status != GF_OK) {
		return;
	}
	if ((e==GF_EOS) || filter->removed || filter->finalized) {
		gf_mx_p(filter->tasks_mx);
		filter->process_task_queued = 0;
		gf_mx_v(filter->tasks_mx);
		return;
	}
	check_filter_error(filter, e, GF_FALSE);
}

GF_EXPORT
void gf_filter_send_update(GF_Filter *filter, const char *fid, const char *name, const char *val, u32 propagate_mask)
{
	if (filter) gf_fs_send_update(filter->session, fid, fid ? NULL : filter, name, val, propagate_mask);
}

GF_Filter *gf_filter_clone(GF_Filter *filter)
{
	GF_Filter *new_filter = gf_filter_new(filter->session, filter->freg, filter->orig_args, NULL, filter->arg_type, NULL);
	if (!new_filter) return NULL;
	new_filter->cloned_from = filter;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter cloned (register %s, args %s)\n", filter->freg->name, filter->orig_args ? filter->orig_args : "none"));

	return new_filter;
}

GF_EXPORT
u32 gf_filter_get_ipid_count(GF_Filter *filter)
{
	return filter->num_input_pids;
}

GF_EXPORT
GF_FilterPid *gf_filter_get_ipid(GF_Filter *filter, u32 idx)
{
	return gf_list_get(filter->input_pids, idx);
}

GF_EXPORT
u32 gf_filter_get_opid_count(GF_Filter *filter)
{
	return filter->num_output_pids;
}

GF_EXPORT
GF_FilterPid *gf_filter_get_opid(GF_Filter *filter, u32 idx)
{
	return gf_list_get(filter->output_pids, idx);
}

void gf_filter_post_process_task_internal(GF_Filter *filter, Bool use_direct_dispatch)
{
	if (filter->finalized || filter->removed)
		return;
	//lock task mx to take the decision whether to post a new task or not (cf gf_filter_check_pending_tasks)
	gf_mx_p(filter->tasks_mx);
	assert((s32)filter->process_task_queued>=0);

	if (use_direct_dispatch) {
		safe_int_inc(&filter->process_task_queued);
		gf_fs_post_task_ex(filter->session, gf_filter_process_task, filter, NULL, "process", NULL, GF_FALSE, GF_TRUE);
	} else if (safe_int_inc(&filter->process_task_queued) <= 1) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s added to scheduler\n", filter->freg->name));
		gf_fs_post_task(filter->session, gf_filter_process_task, filter, NULL, "process", NULL);
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s skip post process task\n", filter->freg->name));
		assert(filter->session->run_status
		 		|| filter->session->in_final_flush
		 		|| filter->disabled
				|| filter->scheduled_for_next_task
		 		|| gf_fq_count(filter->tasks)
		);
	}
	if (!filter->session->direct_mode && !use_direct_dispatch) {
		assert(filter->process_task_queued);
	}
	gf_mx_v(filter->tasks_mx);
}

GF_EXPORT
void gf_filter_post_process_task(GF_Filter *filter)
{
	gf_filter_post_process_task_internal(filter, GF_FALSE);
}
GF_EXPORT
void gf_filter_ask_rt_reschedule(GF_Filter *filter, u32 us_until_next)
{
	if (!filter->in_process) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s request for real-time reschedule but filter is not in process\n", filter->name));
		return;
	}
	if (!us_until_next)
		return;
	filter->schedule_next_time = 1+us_until_next + gf_sys_clock_high_res();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Filter %s real-time reschedule in %d us (at "LLU" sys clock)\n", filter->name, us_until_next, filter->schedule_next_time));
}

GF_EXPORT
void gf_filter_set_setup_failure_callback(GF_Filter *filter, GF_Filter *source_filter, void (*on_setup_error)(GF_Filter *f, void *on_setup_error_udta, GF_Err e), void *udta)
{
	if (!filter) return;
	if (!source_filter) return;
	source_filter->on_setup_error = on_setup_error;
	source_filter->on_setup_error_filter = filter;
	source_filter->on_setup_error_udta = udta;
}

struct _gf_filter_setup_failure
{
	GF_Err e;
	GF_Filter *filter;
	GF_Filter *notify_filter;
	Bool do_disconnect;
} filter_setup_failure;

static void gf_filter_setup_failure_task(GF_FSTask *task)
{
	s32 res;
	GF_Err e;
	GF_Filter *f = ((struct _gf_filter_setup_failure *)task->udta)->filter;
	if (task->udta) {
		e = ((struct _gf_filter_setup_failure *)task->udta)->e;
		gf_free(task->udta);

		f->session->last_connect_error = e;
	}

	if (!f->finalized && f->freg->finalize) {
		FSESS_CHECK_THREAD(f)
		f->freg->finalize(f);
	}
	if (f->session->filters_mx) gf_mx_p(f->session->filters_mx);

	res = gf_list_del_item(f->session->filters, f);
	if (res < 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s task failure callback on already removed filter!\n", f->name));
	}

	if (f->session->filters_mx) gf_mx_v(f->session->filters_mx);

	gf_filter_del(f);
}

static void gf_filter_setup_failure_notify_task(GF_FSTask *task)
{
	struct _gf_filter_setup_failure *st = (struct _gf_filter_setup_failure *)task->udta;
	if (st->notify_filter && st->filter->on_setup_error)
		st->filter->on_setup_error(st->filter, st->filter->on_setup_error_udta, st->e);

	if (st->do_disconnect) {
		gf_fs_post_task(st->filter->session, gf_filter_setup_failure_task, NULL, NULL, "setup_failure", st);
	} else {
		gf_free(st);
	}
}

GF_EXPORT
void gf_filter_notification_failure(GF_Filter *filter, GF_Err reason, Bool force_disconnect)
{
	struct _gf_filter_setup_failure *stack;
	if (!filter->on_setup_error_filter && !force_disconnect) return;

	stack = gf_malloc(sizeof(struct _gf_filter_setup_failure));
	stack->e = reason;
	stack->notify_filter = filter->on_setup_error_filter;
	stack->filter = filter;
	stack->do_disconnect = force_disconnect;
	if (force_disconnect) {
		filter->removed = GF_TRUE;
	}
	if (filter->on_setup_error_filter) {
		gf_fs_post_task(filter->session, gf_filter_setup_failure_notify_task, filter->on_setup_error_filter, NULL, "setup_failure_notify", stack);
	} else if (force_disconnect) {
		gf_fs_post_task(filter->session, gf_filter_setup_failure_task, NULL, NULL, "setup_failure", stack);
	}
}

GF_EXPORT
void gf_filter_setup_failure(GF_Filter *filter, GF_Err reason)
{

	if (filter->in_connect_err) {
		filter->in_connect_err = reason;
		return;
	}

	//filter was already connected, trigger removal of all pid instances
	if (filter->num_input_pids) {
		gf_filter_reset_pending_packets(filter);
		filter->removed = GF_TRUE;
		while (filter->num_input_pids) {
			GF_FilterPidInst *pidinst = gf_list_get(filter->input_pids, 0);
			GF_Filter *sfilter = pidinst->pid->filter;				
			gf_list_del_item(filter->input_pids, pidinst);
			pidinst->filter = NULL;
			filter->num_input_pids = gf_list_count(filter->input_pids);

			//post a pid_delete task to also trigger removal of the filter if needed
			gf_fs_post_task(filter->session, gf_filter_pid_inst_delete_task, sfilter, pidinst->pid, "pid_inst_delete", pidinst);
		}
		return;
	}

	//don't accept twice a notif
	if (filter->setup_notified) return;
	filter->setup_notified = GF_TRUE;

	gf_filter_notification_failure(filter, reason, GF_TRUE);
}

void gf_filter_remove_task(GF_FSTask *task)
{
	s32 res;
	GF_Filter *f = task->filter;
	u32 count = gf_fq_count(f->tasks);

	if (f->out_pid_connection_pending || f->detach_pid_tasks_pending) {
		task->requeue_request = GF_TRUE;
		return;
	}

	assert(f->finalized);

	if (count!=1) {
		task->requeue_request = GF_TRUE;
		task->can_swap = GF_TRUE;
		count = gf_fq_count(f->tasks);
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s destroy postponed, tasks remaining in filer %d:\n", f->name, count));
		for (u32 i=0; i<count; i++) {
			GF_FSTask *at = gf_fq_get(f->tasks, i);
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\ttask #%d %s\n", i+1, at->log_name));
		}
		return;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Filter %s destruction task\n", f->name));

	//avoid destruction of the task
	gf_fq_pop(f->tasks);
	
	if (f->freg->finalize) {
		FSESS_CHECK_THREAD(f)
		f->freg->finalize(f);
	}

	if (f->session->filters_mx) gf_mx_p(f->session->filters_mx);

	res = gf_list_del_item(f->session->filters, f);
	if (res<0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s destruction task on already removed filter\n", f->name));
	}

	if (f->session->filters_mx) gf_mx_v(f->session->filters_mx);

	//detach all input pids
	while (gf_list_count(f->input_pids)) {
		GF_FilterPidInst *pidinst = gf_list_pop_back(f->input_pids);
		pidinst->filter = NULL;
	}

	gf_filter_del(f);
	task->filter = NULL;
	task->requeue_request = GF_FALSE;
}

void gf_filter_post_remove(GF_Filter *filter)
{
	assert(!filter->swap_pidinst_dst);
	assert(!filter->swap_pidinst_src);
	assert(!filter->finalized);
	filter->finalized = GF_TRUE;
	gf_fs_post_task(filter->session, gf_filter_remove_task, filter, NULL, "filter_destroy", NULL);
}

static void gf_filter_tag_remove(GF_Filter *filter, GF_Filter *source_filter, GF_Filter *until_filter)
{
	u32 i, count, j, nb_inst;
	u32 nb_rem_inst=0;
	if (filter==until_filter) return;

	count = gf_list_count(filter->input_pids);
	for (i=0; i<count; i++) {
		GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, i);
		if (pidi->pid->filter==source_filter) nb_rem_inst++;
	}
	if (nb_rem_inst != count) return;
	//already removed
	if (filter->removed) return;
	
	//filter will be removed, propagate on all output pids
	filter->removed = GF_TRUE;

	count = gf_list_count(filter->output_pids);
	for (i=0; i<count; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		nb_inst = pid->num_destinations;
		for (j=0; j<nb_inst; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
			gf_filter_tag_remove(pidi->filter, filter, until_filter);
			gf_fs_post_task(filter->session, gf_filter_pid_disconnect_task, pidi->filter, pid, "pidinst_disconnect", NULL);
		}
	}
}

void gf_filter_remove_internal(GF_Filter *filter, GF_Filter *until_filter, Bool keep_end_connections)
{
	u32 i, j, count;

	if (!filter) return;

	if (filter->removed) 
		return;

	if (filter==until_filter)
		return;

	if (until_filter) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Disconnecting filter %s up to %s\n", filter->name, until_filter->name));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Disconnecting filter %s from session\n", filter->name));
	}
	//get all dest pids, post disconnect and mark filters as removed
	assert(!filter->removed);
	filter->removed = GF_TRUE;
	for (i=0; i<filter->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		count = pid->num_destinations;
		for (j=0; j<count; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);

			if (until_filter) {
				gf_filter_tag_remove(pidi->filter, filter, until_filter);
			}

			if (keep_end_connections && (pidi->filter == until_filter)) {

			} else {
				gf_fs_post_task(filter->session, gf_filter_pid_disconnect_task, pidi->filter, pid, "pidinst_disconnect", NULL);
			}
		}
	}
	if (keep_end_connections) return;

	//check all pids connected to this filter, ensure their owner is only connected to this filter
	for (i=0; i<filter->num_input_pids; i++) {
		GF_FilterPid *pid;
		GF_FilterPidInst *pidi = gf_list_get(filter->input_pids, i);
		Bool can_remove = GF_TRUE;
		//check all output pids of the filter owning this pid are connected to ourselves
		pid = pidi->pid;
		count = pid->num_destinations;
		for (j=0; j<count; j++) {
			GF_FilterPidInst *pidi_o = gf_list_get(pid->destinations, j);
			if (pidi_o->filter != filter) {
				can_remove = GF_FALSE;
				break;
			}
		}
		if (can_remove && !pid->filter->removed) {
			gf_filter_remove_internal(pid->filter, NULL, GF_FALSE);
		}
	}
}

GF_EXPORT
void gf_filter_remove_src(GF_Filter *filter, GF_Filter *src_filter)
{
	gf_filter_remove_internal(src_filter, filter, GF_FALSE);
}

#if 0
GF_EXPORT
void gf_filter_remove_dst(GF_Filter *filter, GF_Filter *dst_filter)
{
	u32 i, j;
	Bool removed;
	if (!filter) return;
	removed = filter->removed;
	filter->removed = GF_TRUE;
	gf_filter_remove_internal(dst_filter, filter, GF_FALSE);
	filter->removed = removed;

	for (i=0; i<filter->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		for (j=0; j<pid->num_destinations; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
			if (pidi->filter->removed) {
				gf_fs_post_task(pidi->filter->session, gf_filter_pid_disconnect_task, pidi->filter, pidi->pid, "pidinst_disconnect", NULL);
			}
		}
	}
}
#endif

Bool gf_filter_swap_source_register(GF_Filter *filter)
{
	u32 i;
	char *src_url=NULL;
	GF_Filter *target_filter=NULL;
	GF_Err e;
	const GF_FilterArgs *src_arg=NULL;

	while (gf_list_count(filter->postponed_packets)) {
		GF_FilterPacket *pck = gf_list_pop_front(filter->postponed_packets);
		gf_filter_packet_destroy(pck);
	}

	while (gf_list_count(filter->output_pids)) {
		GF_FilterPid *pid = gf_list_pop_back(filter->output_pids);
		pid->destroyed = GF_TRUE;
		gf_fs_post_task(filter->session, gf_filter_pid_del_task, filter, pid, "pid_delete", NULL);
	}
	filter->num_output_pids = 0;

	if (filter->freg->finalize) {
		FSESS_CHECK_THREAD(filter)
		filter->freg->finalize(filter);
		filter->finalized = GF_TRUE;
	}
	gf_list_add(filter->blacklisted, (void *)filter->freg);

	i=0;
	while (filter->freg->args) {
		src_arg = &filter->freg->args[i];
		if (!src_arg || !src_arg->arg_name) {
			src_arg=NULL;
			break;
		}
		i++;
		if (strcmp(src_arg->arg_name, "src")) continue;
		//found it, get the url
		if (src_arg->offset_in_private<0) continue;

#ifdef WIN32
		src_url = *(char **)( ((char *)filter->filter_udta) + src_arg->offset_in_private);
		*(char **)(((char *)filter->filter_udta) + src_arg->offset_in_private) = NULL;
#else
		src_url = *(char **) (filter->filter_udta + src_arg->offset_in_private);
		*(char **)(filter->filter_udta + src_arg->offset_in_private) = NULL;
#endif
		 break;
	}

	gf_free(filter->filter_udta);
	filter->filter_udta = NULL;
	if (!src_url) return GF_FALSE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Swaping source filter for URL %s\n", src_url));

	target_filter = filter->target_filter;
	filter->finalized = GF_FALSE;
	gf_fs_load_source_dest_internal(filter->session, src_url, NULL, NULL, &e, filter, filter->target_filter ? filter->target_filter : filter->dst_filter, GF_TRUE, filter->no_dst_arg_inherit, NULL);
	//we manage to reassign an input registry
	if (e==GF_OK) {
		gf_free(src_url);
		if (target_filter) filter->dst_filter = NULL;
		return GF_TRUE;
	}
	if (!filter->finalized) {
		gf_free(src_url);
		return gf_filter_swap_source_register(filter);
	}

	for (i=0; i<gf_list_count(filter->destination_links); i++) {
		GF_Filter *af = gf_list_get(filter->destination_links, i);
		if (af->num_input_pids) {
			u32 j;
			for (j=0; j<af->num_input_pids; j++) {
				GF_FilterPidInst *pidi = gf_list_get(af->input_pids, j);
				pidi->is_end_of_stream = GF_TRUE;
			}
		}
		if (af->sticky) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to find any filter for URL %s\n", src_url));
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to find any filter for URL %s, disabling destination filter %s\n", src_url, af->name));
			af->removed = GF_TRUE;
		}
	}
	//nope ...
	gf_filter_setup_failure(filter, e);
	gf_free(src_url);
	return GF_FALSE;
}

void gf_filter_forward_clock(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	u32 i;
	u64 clock_val;
	if (!filter->next_clock_dispatch_type) return;
	if (!filter->num_output_pids) return;
	
	for (i=0; i<filter->num_output_pids; i++) {
		Bool req_props_map, info_modified;
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		GF_PropertyMap *map;

		//see \ref gf_filter_pid_merge_properties_internal for mutex
		gf_mx_p(pid->filter->tasks_mx);
		map = gf_list_last(pid->properties);
		gf_mx_v(pid->filter->tasks_mx);

		clock_val = filter->next_clock_dispatch;
		if (map->timescale != filter->next_clock_dispatch_timescale) {
			clock_val *= map->timescale;
			clock_val /= filter->next_clock_dispatch_timescale;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s PID %s internal forward of clock reference\n", pid->filter->name, pid->name));
		pck = gf_filter_pck_new_shared((GF_FilterPid *)pid, NULL, 0, NULL);
		gf_filter_pck_set_cts(pck, clock_val);
		gf_filter_pck_set_clock_type(pck, filter->next_clock_dispatch_type);

		//do not let the clock packet carry the props/info change flags since it is an internal
		//packet discarded before processing these flags
		req_props_map = pid->request_property_map;
		pid->request_property_map = GF_TRUE;
		info_modified = pid->pid_info_changed;
		pid->pid_info_changed = GF_FALSE;

		gf_filter_pck_send(pck);
		pid->request_property_map = req_props_map;
		pid->pid_info_changed = info_modified;
	}
	filter->next_clock_dispatch_type = 0;
}

GF_EXPORT
Bool gf_filter_is_supported_source(GF_Filter *filter, const char *url, const char *parent_url)
{
	GF_Err e;
	Bool is_supported = GF_FALSE;
	gf_fs_load_source_dest_internal(filter->session, url, NULL, parent_url, &e, NULL, filter, GF_TRUE, GF_TRUE, &is_supported);
	return is_supported;
}

GF_EXPORT
GF_Filter *gf_filter_connect_source(GF_Filter *filter, const char *url, const char *parent_url, GF_Err *err)
{
	GF_Filter *filter_src = gf_fs_load_source_dest_internal(filter->session, url, NULL, parent_url, err, NULL, filter, GF_TRUE, GF_TRUE, NULL);
	if (!filter_src) return NULL;

	if (!filter->source_filters)
		filter->source_filters = gf_list_new();
	gf_list_add(filter->source_filters, filter_src);
	return filter_src;
}

GF_EXPORT
GF_Filter *gf_filter_connect_destination(GF_Filter *filter, const char *url, GF_Err *err)
{
	return gf_fs_load_source_dest_internal(filter->session, url, NULL, NULL, err, NULL, filter, GF_FALSE, GF_FALSE, NULL);
}


GF_EXPORT
void gf_filter_get_buffer_max(GF_Filter *filter, u32 *max_buf, u32 *max_playout_buf)
{
	u32 i;
	u32 buf_max = 0;
	u32 buf_play_max = 0;
	for (i=0; i<filter->num_output_pids; i++) {
		u32 j;
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		if (buf_max < pid->user_max_buffer_time) buf_max = (u32) pid->user_max_buffer_time;
		if (buf_max < pid->max_buffer_time) buf_max = (u32) pid->max_buffer_time;

		if (buf_play_max < pid->user_max_playout_time) buf_play_max = pid->user_max_playout_time;
		if (buf_play_max < pid->max_buffer_time) buf_play_max = (u32) pid->max_buffer_time;

		for (j=0; j<pid->num_destinations; j++) {
			u32 mb, pb;
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
			gf_filter_get_buffer_max(pidi->filter, &mb, &pb);
			if (buf_max < mb) buf_max = mb;
			if (buf_play_max < pb) buf_play_max = pb;
		}
	}
	*max_buf = buf_max;
	*max_playout_buf = buf_play_max;
	return;
}

GF_EXPORT
void gf_filter_make_sticky(GF_Filter *filter)
{
	if (filter) filter->sticky = 1;
}

static u32 gf_filter_get_num_events_queued_internal(GF_Filter *filter)
{
	u32 i;
	u32 nb_events = 0;
	if (!filter) return 0;
	nb_events = filter->num_events_queued;

	for (i=0; i<filter->num_output_pids; i++) {
		u32 k;
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		for (k=0; k<pid->num_destinations; k++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, k);
			nb_events += gf_filter_get_num_events_queued(pidi->filter);
		}
	}
	return nb_events;
}
GF_EXPORT
u32 gf_filter_get_num_events_queued(GF_Filter *filter)
{
	u32 res;
	GF_FilterSession *fsess;
	if (!filter) return 0;
	fsess = filter->session;

	if (fsess->filters_mx)
		gf_mx_p(fsess->filters_mx);

	res = gf_filter_get_num_events_queued_internal(filter);
	if (fsess->filters_mx)
		gf_mx_v(fsess->filters_mx);

	return res;
}

GF_EXPORT
void gf_filter_hint_single_clock(GF_Filter *filter, u64 time_in_us, Double media_timestamp)
{
	//for now only one clock hint possible ...
	filter->session->hint_clock_us = time_in_us;
	filter->session->hint_timestamp = media_timestamp;
}

GF_EXPORT
void gf_filter_get_clock_hint(GF_Filter *filter, u64 *time_in_us, Double *media_timestamp)
{
	//for now only one clock hint possible ...
	if (time_in_us) *time_in_us = filter->session->hint_clock_us;
	if (media_timestamp) *media_timestamp = filter->session->hint_timestamp;
}

GF_EXPORT
GF_Err gf_filter_assign_id(GF_Filter *filter, const char *id)
{
	if (!filter || filter->id || !id) return GF_BAD_PARAM;
	filter->id = gf_strdup(id);
	return GF_OK;
}

GF_EXPORT
const char *gf_filter_get_id(GF_Filter *filter)
{
	if (filter) return filter->id;
	return NULL;
}

GF_EXPORT
GF_Err gf_filter_set_source(GF_Filter *filter, GF_Filter *link_from, const char *link_ext)
{
	char szID[1024];
	if (!filter || !link_from) return GF_BAD_PARAM;
	if (filter == link_from) return GF_BAD_PARAM;
	if (filter->num_input_pids) return GF_BAD_PARAM;
	//link from may have input pids declared but pending if source filter
	if (filter->num_output_pids) return GF_BAD_PARAM;
	//don't allow loops
	if (filter_in_parent_chain(filter, link_from)) return GF_BAD_PARAM;

	if (!link_from->id) {
		sprintf(szID, "_%p_", link_from);
		gf_filter_set_id(link_from, szID);
	}
	if (link_ext) {
		sprintf(szID, "%s%c%s", link_from->id, link_from->session->sep_frag, link_ext);
		gf_filter_set_sources(filter, szID);
	} else {
		gf_filter_set_sources(filter, link_from->id);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_filter_override_caps(GF_Filter *filter, const GF_FilterCapability *caps, u32 nb_caps )
{
	if (!filter) return GF_BAD_PARAM;
	if (filter->num_output_pids || filter->num_output_pids) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Attempts at setting input caps on filter %s during execution of filter, not supported\n", filter->name));
		return GF_NOT_SUPPORTED;
	}
	filter->forced_caps = nb_caps ? caps : NULL;
	filter->nb_forced_caps = nb_caps;
	return GF_OK;
}

GF_EXPORT
void gf_filter_pid_init_play_event(GF_FilterPid *pid, GF_FilterEvent *evt, Double start, Double speed, const char *log_name)
{
	u32 pmode = GF_PLAYBACK_MODE_NONE;
	const GF_PropertyValue *p;
	Bool was_end = GF_FALSE;
	memset(evt, 0, sizeof(GF_FilterEvent));
	evt->base.type = GF_FEVT_PLAY;
	evt->base.on_pid = pid;

	evt->play.speed = 1.0;

	if ((speed<0) && !start) start = -1.0;

	p = gf_filter_pid_get_property_first(pid, GF_PROP_PID_PLAYBACK_MODE);
	if (p) pmode = p->value.uint;

	evt->play.start_range = start;
	if (start<0) {
		was_end = GF_TRUE;
		p = gf_filter_pid_get_property_first(pid, GF_PROP_PID_DURATION);
		if (p && p->value.frac.den) {
			evt->play.start_range *= -100;
			evt->play.start_range *= p->value.frac.num;
			evt->play.start_range /= 100 * p->value.frac.den;
		}
	}
	switch (pmode) {
	case GF_PLAYBACK_MODE_NONE:
		evt->play.start_range = 0;
		if (start) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[%s] Media PID does not support seek, ignoring start directive\n", log_name));
		}
		break;
	case GF_PLAYBACK_MODE_SEEK:
		if (speed != 1.0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[%s] Media PID does not support speed, ignoring speed directive\n", log_name));
		}
		break;
	case GF_PLAYBACK_MODE_FASTFORWARD:
		if (speed<0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[%s] Media PID does not support negative speed, ignoring speed directive\n", log_name));
			if (was_end) evt->play.start_range = 0;
		} else {
			evt->play.speed = speed;
		}
		break;
	default:
		evt->play.speed = speed;
		break;
	}
}

void gf_filter_sep_max_extra_input_pids(GF_Filter *filter, u32 max_extra_pids)
{
	if (filter) filter->max_extra_pids = max_extra_pids;
}

Bool gf_filter_block_enabled(GF_Filter *filter)
{
	if (!filter) return GF_FALSE;
	return filter->session->disable_blocking ? GF_FALSE : GF_TRUE;
}

GF_EXPORT
GF_Err gf_filter_pid_raw_new(GF_Filter *filter, const char *url, const char *local_file, const char *mime_type, const char *fext, u8 *probe_data, u32 probe_size, Bool trust_mime, GF_FilterPid **out_pid)
{
	char *sep;
	char tmp_ext[50];
	Bool ext_not_trusted, is_new_pid = GF_FALSE;
	GF_FilterPid *pid = *out_pid;
	if (!pid) {
		pid = gf_filter_pid_new(filter);
		if (!pid) {
			return GF_OUT_OF_MEM;
		}
		pid->max_buffer_unit = 1;
		is_new_pid = GF_TRUE;
		*out_pid = pid;
	}

	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );

	if (local_file)
		gf_filter_pid_set_property(pid, GF_PROP_PID_FILEPATH, &PROP_STRING(local_file));


	if (url) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_URL, &PROP_STRING(url));

		if (!strnicmp(url, "isobmff://", 10)) {
			gf_filter_pid_set_name(pid, "isobmff://");
			fext = "mp4";
			mime_type = "video/mp4";
			if (is_new_pid)
				gf_filter_pid_set_eos(pid);
		} else {
			sep = gf_file_basename(url);

			gf_filter_pid_set_name(pid, sep);
		}

		if (fext) {
			strncpy(tmp_ext, fext, 20);
			strlwr(tmp_ext);
			gf_filter_pid_set_property(pid, GF_PROP_PID_FILE_EXT, &PROP_STRING(tmp_ext));
		} else {
			char *ext = strrchr(url, '.');
			if (ext && !stricmp(ext, ".gz")) {
				char *anext;
				ext[0] = 0;
				anext = strrchr(url, '.');
				ext[0] = '.';
				ext = anext;
			}
			if (ext) ext++;

			if (ext) {
				char *s = strchr(ext, '#');
				if (s) s[0] = 0;

				strncpy(tmp_ext, ext, 20);
				strlwr(tmp_ext);
				gf_filter_pid_set_property(pid, GF_PROP_PID_FILE_EXT, &PROP_STRING(tmp_ext));
				if (s) s[0] = '#';
			}
		}
	}
	
	ext_not_trusted = GF_FALSE;
	//probe data
	if ((!mime_type || !trust_mime) && !filter->no_probe && is_new_pid && probe_data && probe_size && !(filter->session->flags & GF_FS_FLAG_NO_PROBE)) {
		u32 i, count;
		GF_FilterProbeScore score, max_score = GF_FPROBE_NOT_SUPPORTED;
		const char *probe_mime = NULL;
		gf_mx_p(filter->session->filters_mx);
		count = gf_list_count(filter->session->registry);
		for (i=0; i<count; i++) {
			const char *a_mime;
			const GF_FilterRegister *freg = gf_list_get(filter->session->registry, i);
			if (!freg || !freg->probe_data) continue;
			score = GF_FPROBE_NOT_SUPPORTED;
			a_mime = freg->probe_data(probe_data, probe_size, &score);
			if (score==GF_FPROBE_NOT_SUPPORTED) {
				u32 k;
				for (k=0;k<freg->nb_caps && !ext_not_trusted; k++) {
					const GF_FilterCapability *cap = &freg->caps[k];
					if (!(cap->flags & GF_CAPFLAG_IN_BUNDLE)) continue;
					if (!(cap->flags & GF_CAPFLAG_INPUT)) continue;
					if (cap->code != GF_PROP_PID_FILE_EXT) continue;

					if (strstr(cap->val.value.string, tmp_ext))
						ext_not_trusted = GF_TRUE;
				}
			} else if (score==GF_FPROBE_EXT_MATCH) {
				if (a_mime && strstr(a_mime, tmp_ext)) {
					ext_not_trusted = GF_FALSE;
					probe_mime = NULL;
					break;
				}
			} else {
				if (a_mime) {
					GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("Data Prober (filter %s) detected format is%s mime %s\n", freg->name, (score==GF_FPROBE_MAYBE_SUPPORTED) ? " maybe" : "", a_mime));
				}
				if (a_mime && (score > max_score)) {
					probe_mime = a_mime;
					max_score = score;
				}
			}
		}
		gf_mx_v(filter->session->filters_mx);

		pid->ext_not_trusted = ext_not_trusted;

		if (probe_mime) {
			mime_type = probe_mime;
		}
	}
	if (mime_type) {
		strncpy(tmp_ext, mime_type, 50);
		strlwr(tmp_ext);
		gf_filter_pid_set_property(pid, GF_PROP_PID_MIME, &PROP_STRING( tmp_ext ));
		//we have a mime, disable extension checking
		pid->ext_not_trusted = GF_TRUE;
	}

	return GF_OK;
}

GF_EXPORT
const char *gf_filter_get_arg(GF_Filter *filter, const char *arg_name, char dump[GF_PROP_DUMP_ARG_SIZE])
{
	u32 i=0;
	if (!filter || !arg_name || !dump) return NULL;

	while (1) {
		GF_PropertyValue p;
		const GF_FilterArgs *arg = &filter->freg->args[i];
		if (!arg || !arg->arg_name) break;
		i++;
		
		if (strcmp(arg->arg_name, arg_name)) continue;
		if (arg->offset_in_private < 0) continue;

		p.type = arg->arg_type;
		switch (arg->arg_type) {
		case GF_PROP_BOOL:
			p.value.boolean = * (Bool *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_UINT:
			p.value.uint = * (u32 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_SINT:
			p.value.sint = * (s32 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_LUINT:
			p.value.longuint = * (u64 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_LSINT:
			p.value.longsint = * (s64 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_FLOAT:
			p.value.fnumber = * (Fixed *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_DOUBLE:
			p.value.number = * (Double *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_VEC2I:
			p.value.vec2i = * (GF_PropVec2i *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_VEC2:
			p.value.vec2 = * (GF_PropVec2 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_VEC3I:
			p.value.vec3i = * (GF_PropVec3i *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_VEC3:
			p.value.vec3 = * (GF_PropVec3 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_VEC4I:
			p.value.vec4i = * (GF_PropVec4i *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_VEC4:
			p.value.vec4 = * (GF_PropVec4 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_FRACTION:
			p.value.frac = * (GF_Fraction *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_FRACTION64:
			p.value.lfrac = * (GF_Fraction64 *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_DATA:
		case GF_PROP_DATA_NO_COPY:
		case GF_PROP_CONST_DATA:
			p.value.data = * (GF_PropData *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_POINTER:
			p.value.ptr = * (void **) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_STRING_NO_COPY:
		case GF_PROP_STRING:
		case GF_PROP_NAME:
			p.value.ptr = * (char **) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_STRING_LIST:
			p.value.string_list = * (GF_List **) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_UINT_LIST:
			p.value.uint_list = * (GF_PropUIntList *) ((char *)filter->filter_udta + arg->offset_in_private);
			break;
		case GF_PROP_PIXFMT:
			p.value.uint = * (u32 *) ((char *)filter->filter_udta + arg->offset_in_private);
			return gf_pixel_fmt_name(p.value.uint);
			break;
		case GF_PROP_PCMFMT:
			p.value.uint = * (u32 *) ((char *)filter->filter_udta + arg->offset_in_private);
			return gf_audio_fmt_name(p.value.uint);
			break;
		default:
			return NULL;
		}
		return gf_prop_dump_val(&p, dump, GF_FALSE, arg->min_max_enum);
	}
	return NULL;
}

GF_EXPORT
Bool gf_filter_is_supported_mime(GF_Filter *filter, const char *mime)
{
	return gf_fs_mime_supported(filter->session, mime);
}

GF_EXPORT
Bool gf_filter_ui_event(GF_Filter *filter, GF_Event *uievt)
{
	return gf_fs_ui_event(filter->session, uievt);
}

GF_EXPORT
Bool gf_filter_all_sinks_done(GF_Filter *filter)
{
	u32 i, count;
	Bool res = GF_TRUE;
	if (!filter || filter->session->in_final_flush || (filter->session->run_status==GF_EOS) )
		return GF_TRUE;
		
	gf_mx_p(filter->session->filters_mx);
	count = gf_list_count(filter->session->filters);
	for (i=0; i<count; i++) {
		u32 j;
		GF_Filter *f = gf_list_get(filter->session->filters, i);
		if (f->num_output_pids) continue;
		for (j=0; j<f->num_input_pids; j++) {
			GF_FilterPidInst *pidi = gf_list_get(f->input_pids, j);
			if (pidi->pid->is_playing && !pidi->is_end_of_stream) {
				res = GF_FALSE;
				goto exit;
			}
		}
	}
exit:
	gf_mx_v(filter->session->filters_mx);
	return res;
}


GF_EXPORT
void gf_filter_register_opengl_provider(GF_Filter *filter, Bool do_register)
{
#ifndef GPAC_DISABLE_3D
	GF_Err e;
	if (filter->removed || filter->finalized) return;

	if (do_register) {
		if (gf_list_find(filter->session->gl_providers, filter)<0)
			gf_list_add(filter->session->gl_providers, filter);
		return;
	}
	gf_list_del_item(filter->session->gl_providers, filter);
	e = gf_fs_check_gl_provider(filter->session);
	if (e && filter->session->nb_gl_filters) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to reload an OpenGL provider and some filters require openGL, aborting\n"));
		gf_fs_abort(filter->session, GF_FALSE);
	}
#endif

}

GF_EXPORT
GF_Err gf_filter_request_opengl(GF_Filter *filter)
{
#ifndef GPAC_DISABLE_3D
	GF_Err e;
	if (filter->finalized || filter->removed) return GF_OK;

	filter->session->nb_gl_filters++;
	e = gf_fs_check_gl_provider(filter->session);
	if (e) {
		filter->session->nb_gl_filters--;
		filter->main_thread_forced = GF_FALSE;
		return e;
	}
	filter->main_thread_forced = GF_TRUE;
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_EXPORT
u32 gf_filter_count_source_by_protocol(GF_Filter *filter, const char *protocol_scheme, Bool expand_proto, GF_FilterPid * (*enum_pids)(void *udta, u32 *idx), void *udta)
{
	u32 i, count, len, res=0;
	if (!filter || !protocol_scheme) return 0;
	gf_mx_p(filter->session->filters_mx);
	len = (u32) strlen(protocol_scheme);
	count = gf_list_count(filter->session->filters);
	for (i=0; i<count; i++) {
		const char *args;
		GF_Filter *src = gf_list_get(filter->session->filters, i);
		//check only sinks
		if (src->freg->configure_pid) continue;
		args = src->src_args;
		if (!args) args = src->orig_args;
		if (!args || (strlen(args)<5) ) continue;

		if (strncmp(args + 4, protocol_scheme, len)) continue;
		if (!expand_proto && (src->src_args[len] != ':')) continue;

		if (! filter_in_parent_chain(filter, src)) {
			u32 j=0;
			Bool found=GF_FALSE;
			if (!enum_pids) continue;
			while (1) {
				GF_FilterPid *pid = enum_pids(udta, &j);
				if (!pid) break;
				j++;
				if ( filter_in_parent_chain(pid->pid->filter, src)) {
					found=GF_TRUE;
					break;
				}
			}
			if (!found) continue;
		}

		res++;
	}

	gf_mx_v(filter->session->filters_mx);
	return res;
}

GF_EXPORT
void gf_filter_disable_probe(GF_Filter *filter)
{
	if (filter)
	 	filter->no_probe = GF_TRUE;
}

GF_EXPORT
void gf_filter_disable_inputs(GF_Filter *filter)
{
	if (filter)
	 	filter->no_inputs = GF_TRUE;
}

static Bool gf_filter_has_pid_connection_pending_internal(GF_Filter *filter, GF_Filter *stop_at_filter)
{
	u32 i, j;
	if (filter == stop_at_filter) return GF_FALSE;

	if (filter->has_pending_pids) return GF_TRUE;

	for (i=0; i<filter->num_output_pids; i++) {
		GF_FilterPid *pid = gf_list_get(filter->output_pids, i);
		if (pid->init_task_pending) return GF_TRUE;
		for (j=0; j<pid->num_destinations; j++) {
			GF_FilterPidInst *pidi = gf_list_get(pid->destinations, j);
			Bool res = gf_filter_has_pid_connection_pending_internal(pidi->filter, stop_at_filter);
			if (res) return GF_TRUE;
		}
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_filter_has_pid_connection_pending(GF_Filter *filter, GF_Filter *stop_at_filter)
{
	GF_FilterSession *fsess;
	Bool res;
	if (!filter) return GF_FALSE;
	//lock session, this is an unsafe call
	fsess = filter->session;
	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);
	res = gf_filter_has_pid_connection_pending_internal(filter, stop_at_filter);
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);
	return res;
}

GF_EXPORT
Bool gf_filter_reporting_enabled(GF_Filter *filter)
{
	if (filter) return filter->session->reporting_on;
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_filter_update_status(GF_Filter *filter, u32 percent, char *szStatus)
{
	GF_Event evt;
	u32 len;
	if (!filter) return GF_BAD_PARAM;
	if (!filter->session->reporting_on)
		return GF_OK;
		
	if (!szStatus) {
		if (filter->status_str) filter->status_str[0] = 0;
		return GF_OK;
	}
	len = (u32) strlen(szStatus);
	if (len>=filter->status_str_alloc) {
		filter->status_str_alloc = len+1;
		filter->status_str = gf_realloc(filter->status_str, filter->status_str_alloc);
		if (!filter->status_str) {
			filter->status_str_alloc = 0;
			return GF_OUT_OF_MEM;
		}
	}
	memcpy(filter->status_str, szStatus, len+1);
	filter->status_percent = percent;

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_PROGRESS;
	evt.progress.progress_type = 3;
	evt.progress.done = percent;
	evt.progress.total = ((s32)percent>0) ? 10000 : 0;
	evt.progress.filter_idx = gf_list_find(filter->session->filters, filter);
	gf_fs_ui_event(filter->session, &evt);
	return GF_OK;
}

void gf_filter_report_unused_meta_option(GF_Filter *filter, const char *arg)
{
	if (!filter->session || filter->removed || filter->finalized) return;
	gf_mx_p(filter->session->filters_mx);
	gf_fs_push_arg(filter->session, arg, GF_FALSE, 2);
	gf_mx_v(filter->session->filters_mx);
}
