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

static const char *gf_filter_get_dst_args_stripped(GF_FilterSession *fsess, const char *dst_args)
{
	char *dst_striped = NULL;
	if (dst_args) {
		char szDst[5];
		sprintf(szDst, "dst%c", fsess->sep_name);
		dst_striped = strstr(dst_args, szDst);
		if (dst_striped) {
			dst_striped = strchr(dst_striped+5, fsess->sep_args);
			if (dst_striped) dst_striped ++;
		} else {
			dst_striped = (char *)dst_args;
		}
	}
	return dst_striped;
}

const char *gf_filter_get_dst_args(GF_Filter *filter)
{
	return gf_filter_get_dst_args_stripped(filter->session, filter->dst_args);
}

GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *registry, const char *args, const char *dst_args, GF_FilterArgType arg_type, GF_Err *err)
{
	char szName[200];
	const char *dst_striped = NULL;
	GF_Filter *filter;
	GF_Err e;
	u32 i;
	assert(fsess);

	GF_SAFEALLOC(filter, GF_Filter);
	if (!filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc filter for %s\n", registry->name));
		return NULL;
	}
	filter->freg = registry;
	filter->session = fsess;
	filter->max_extra_pids = registry->max_extra_pids;

	if (fsess->use_locks) {
		snprintf(szName, 200, "Filter%sPackets", filter->freg->name);
		filter->pcks_mx = gf_mx_new(szName);
	}
	//for now we always use a lock on the filter task lists
	//TODO: this is our only lock in lock-free mode, we need to find a way to avoid this lock
	snprintf(szName, 200, "Filter%sTasks", filter->freg->name);
	filter->tasks_mx = gf_mx_new(szName);


	filter->tasks = gf_fq_new(filter->tasks_mx);

	filter->pcks_shared_reservoir = gf_fq_new(filter->pcks_mx);
	filter->pcks_alloc_reservoir = gf_fq_new(filter->pcks_mx);
	filter->pcks_inst_reservoir = gf_fq_new(filter->pcks_mx);

	filter->pending_pids = gf_fq_new(NULL);

	filter->blacklisted = gf_list_new();

	if (fsess->filters_mx) gf_mx_p(fsess->filters_mx);
	gf_list_add(fsess->filters, filter);
	if (fsess->filters_mx) gf_mx_v(fsess->filters_mx);

	filter->arg_type = arg_type;
	dst_striped = gf_filter_get_dst_args_stripped(fsess, dst_args);

	if (args && dst_striped) {
		char *all_args;
		u32 len = 2 + (u32) strlen(args) + (u32) strlen(dst_striped);
		all_args = gf_malloc(sizeof(char)*len);
		sprintf(all_args, "%s%c%s", args, fsess->sep_args, dst_striped);
		e = gf_filter_new_finalize(filter, all_args, arg_type);
		filter->orig_args = all_args;
		args = NULL;
	} else if (dst_striped) {
		e = gf_filter_new_finalize(filter, dst_striped, arg_type);
		filter->orig_args = gf_strdup(dst_striped);
		args = NULL;
	} else {
		e = gf_filter_new_finalize(filter, args, arg_type);
	}
	filter->dst_args = dst_args;

	if (e) {
		if (!filter->setup_notified) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Error %s while instantiating filter %s\n", gf_error_to_string(e),registry->name));
			gf_filter_setup_failure(filter, e);
		}
		if (err) *err = e;
		return NULL;
	}
	if (filter && args) filter->orig_args = gf_strdup(args);

	for (i=0; i<registry->nb_caps; i++) {
		if (registry->caps[i].flags & GF_FILTER_CAPS_OUTPUT) {
			filter->has_out_caps = GF_TRUE;
			break;
		}
	}
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Created filter registry %s args %s\n", registry->name, args ? args : "none"));
	return filter;
}


GF_Err gf_filter_new_finalize(GF_Filter *filter, const char *args, GF_FilterArgType arg_type)
{
	gf_filter_set_name(filter, NULL);

	gf_filter_parse_args(filter, args, arg_type);

	filter->skip_process_trigger_on_tasks = GF_FALSE;
	if (!strcmp(filter->freg->name, "compositor"))
		filter->skip_process_trigger_on_tasks = GF_TRUE;

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
	assert(!filter->detached_pid_inst);

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
		GF_FilterPid *pid = gf_list_pop_back(filter->output_pids);
		gf_filter_pid_del(pid);
	}
	gf_list_del(filter->output_pids);

	gf_list_del(filter->blacklisted);
	gf_list_del(filter->input_pids);

	gf_fq_del(filter->tasks, task_del);
	gf_fq_del(filter->pending_pids, NULL);

	reset_filter_args(filter);
	if (filter->src_args) gf_free(filter->src_args);

	gf_fq_del(filter->pcks_shared_reservoir, gf_void_del);
	gf_fq_del(filter->pcks_inst_reservoir, gf_void_del);
	gf_fq_del(filter->pcks_alloc_reservoir, gf_filterpacket_del);

	gf_mx_del(filter->pcks_mx);
	gf_mx_del(filter->tasks_mx);

	if (filter->name) gf_free(filter->name);
	if (filter->id) gf_free(filter->id);
	if (filter->source_ids) gf_free(filter->source_ids);
	if (filter->filter_udta) gf_free(filter->filter_udta);
	if (filter->orig_args) gf_free(filter->orig_args);
	gf_free(filter);
}

void *gf_filter_get_udta(GF_Filter *filter)
{
	assert(filter);

	return filter->filter_udta;
}

const char * gf_filter_get_name(GF_Filter *filter)
{
	assert(filter);
	return (const char *)filter->name;
}

void gf_filter_set_name(GF_Filter *filter, const char *name)
{
	assert(filter);

	if (filter->name) gf_free(filter->name);
	filter->name = gf_strdup(name ? name : filter->freg->name);
}

void gf_filter_set_id(GF_Filter *filter, const char *ID)
{
	assert(filter);

	if (filter->id) gf_free(filter->id);
	filter->id = ID ? gf_strdup(ID) : NULL;
}
const char *gf_filter_sft_id(GF_Filter *filter)
{
	assert(filter);
	return filter->id;
}

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

void gf_filter_set_arg(GF_Filter *filter, const GF_FilterArgs *a, GF_PropertyValue *argv)
{
#ifdef WIN32
	void *ptr = (void *) (((char*) filter->filter_udta) + a->offset_in_private);
#else
	void *ptr = filter->filter_udta + a->offset_in_private;
#endif
	Bool res = GF_FALSE;

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

	if (!value) return gf_props_parse_value(type, name, NULL, enum_values);


	if (!strnicmp(value, "$GPAC_SHARED", 12)) {
		if (gf_get_default_shared_directory(szPath)) {
			strcat(szPath, value+12);
			value = szPath;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to query GPAC shared resource directory location\n"));
		}
	}
	else if (!strnicmp(value, "$GPAC_LANGUAGES", 15)) {
		value = "English";
	}
	argv = gf_props_parse_value(type, name, value, enum_values);
	return argv;
}

void gf_filter_update_arg_task(GF_FSTask *task)
{
	u32 i=0;
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

		if (!a->updatable) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Argument %s of filter %s is not updatable - ignoring\n", a->arg_name, task->filter->name));
			break;
		}

		argv = gf_filter_parse_prop_solve_env_var(task->filter, a->arg_type, a->arg_name, arg->val, a->min_max_enum);

		if (argv.type != GF_PROP_FORBIDEN) {
			GF_Err e;
			FSESS_CHECK_THREAD(task->filter)
			e = task->filter->freg->update_arg(task->filter, arg->name, &argv);
			if (e==GF_OK) {
				gf_filter_set_arg(task->filter, a, &argv);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Filter %s did not accept opdate of arg %s to value %s: %s\n", task->filter->name, arg->name, arg->val, gf_error_to_string(e) ));
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to parse argument %s value %s\n", a->arg_name, a->arg_default_val));
		}
		break;
	}
	gf_free(arg->name);
	gf_free(arg->val);
	gf_free(arg);
}

static void gf_filter_parse_args(GF_Filter *filter, const char *args, GF_FilterArgType arg_type)
{
	u32 i=0;
	char szEscape[7];
	char szSrc[5];
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

	//instantiate all others with defauts value
	i=0;
	while (filter->freg->args) {
		GF_PropertyValue argv;
		const GF_FilterArgs *a = &filter->freg->args[i];
		i++;
		if (!a || !a->arg_name) break;
		if (a->meta_arg) {
			has_meta_args = GF_TRUE;
			continue;
		}
		if (!a->arg_default_val) continue;

		argv = gf_filter_parse_prop_solve_env_var(filter, a->arg_type, a->arg_name, a->arg_default_val, a->min_max_enum);

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

	if (args)
		szArg = gf_malloc(sizeof(char)*1024);

	//parse each arg
	while (args) {
		char *value;
		u32 len;
		Bool found=GF_FALSE;
		char *escaped = NULL;
		Bool internal_url = GF_FALSE;
		//look for our arg separator
		char *sep = strchr(args, filter->session->sep_args);

		if (filter->session->sep_args == ':') {
			while (sep && !strncmp(sep, "://", 3)) {
				//filter internal url schemes
				if (!strncmp(args, szSrc, 4) &&
					(!strncmp(args+4, "video://", 8)
					|| !strncmp(args+4, "audio://", 8)
					|| !strncmp(args+4, "av://", 5)
					|| !strncmp(args+4, "gmem://", 7)
					|| !strncmp(args+4, "gpac://", 7)
					)
				) {
					internal_url = GF_TRUE;
					sep = strchr(sep+3, ':');

				} else {
					//get root /
					sep = strchr(sep+3, '/');
					//get first : after root
					if (sep) sep = strchr(sep+1, ':');
				}
			}

			//watchout for "C:\\"
			while (sep && (sep[1]=='\\')) {
				sep = strchr(sep+1, ':');
			}
		}
		if (sep) {
			escaped = strstr(sep, szEscape);
			if (escaped) sep = escaped;
		}

		if (sep && !strncmp(args, szSrc, 4) && !escaped && !internal_url) {
			Bool file_exists;
			sep[0]=0;
			file_exists = gf_file_exists(args+4);
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

		if ((arg_type == GF_FILTER_ARG_GLOBAL) && !strcmp(szArg, "src"))
			goto skip_arg;

		i=0;
		while (filter->filter_udta && filter->freg->args) {
			Bool is_my_arg = GF_FALSE;
			Bool reverse_bool = GF_FALSE;
			const GF_FilterArgs *a = &filter->freg->args[i];
			i++;
			if (!a || !a->arg_name) break;

			if (!strcmp(a->arg_name, szArg)) is_my_arg = GF_TRUE;
			else if ( (szArg[0]==filter->session->sep_neg) && !strcmp(a->arg_name, szArg+1)) {
				is_my_arg = GF_TRUE;
				reverse_bool = GF_TRUE;
			}
			//little optim here, if no value check for enums
			else if (!value && a->min_max_enum && strchr(a->min_max_enum, '|') ) {
				char *found = strstr(a->min_max_enum, szArg);
				if (found) {
					char c = found[strlen(szArg)];
					if (!c || (c=='|')) {
						is_my_arg = GF_TRUE;
						value = szArg;
					}
				}
			}

			if (is_my_arg) {
				GF_PropertyValue argv;
				found=GF_TRUE;

				argv = gf_filter_parse_prop_solve_env_var(filter, a->meta_arg ? GF_PROP_STRING : a->arg_type, a->arg_name, value, a->min_max_enum);

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
				if (arg_type != GF_FILTER_ARG_GLOBAL)
					gf_filter_set_id(filter, value);
				found = GF_TRUE;
			}
			else if (!strcmp("SID", szArg)) {
				if (arg_type==GF_FILTER_ARG_LOCAL)
					gf_filter_set_sources(filter, value);
				found = GF_TRUE;
			} else if (!strcmp("clone", szArg)) {
				if (arg_type==GF_FILTER_ARG_GLOBAL_SINK)
					filter->clonable=GF_TRUE;
				found = GF_TRUE;
			}
			else if (has_meta_args && filter->freg->update_arg) {
				GF_PropertyValue argv = gf_props_parse_value(GF_PROP_STRING, szArg, value, NULL);
				FSESS_CHECK_THREAD(filter)
				filter->freg->update_arg(filter, szArg, &argv);
				if (argv.value.string) gf_free(argv.value.string);
				found = GF_TRUE;
			}
		}

		if (!found && (arg_type==GF_FILTER_ARG_LOCAL) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Argument \"%s\" not found in filter %s options, ignoring\n", szArg, filter->freg->name));
		}
skip_arg:
		if (escaped) {
			args=sep+6;
		} else if (sep) {
			args=sep+1;
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
				assert (pidi->props != pidi->reconfig_pid_props);
				//unassign old property list and set the new one
				if (safe_int_dec(& pidi->props->reference_count) == 0) {
					gf_list_del_item(pidi->pid->properties, pidi->props);
					gf_props_del(pidi->props);
				}
				pidi->props = pidi->reconfig_pid_props;
				safe_int_inc( & pidi->props->reference_count );
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
		if (opid->num_destinations>1) break;
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
	safe_int_dec(&cur_pidinst->detach_pending);
	task->filter->removed = GF_FALSE;

 	gf_filter_relink_dst(cur_pidinst);
}

void gf_filter_relink_dst(GF_FilterPidInst *pidinst)
{
	GF_Filter *filter_dst;
	GF_FilterPid *link_from_pid = NULL;
	u32 min_chain_len = 0;
	Bool needs_flush = GF_FALSE;
	GF_FilterPidInst *cur_pidinst = pidinst;
	GF_Filter *cur_filter = pidinst->filter;

	//locate the true destination
	pidinst = filter_relink_get_upper_pid(cur_pidinst, &needs_flush);
	assert(pidinst);

	//make sure we flush the end of the pipeline  !
	if (needs_flush) {
		cur_filter->removed = GF_TRUE;
		//prevent any fetch from pid
		safe_int_inc(&cur_pidinst->detach_pending);
		gf_fs_post_task(cur_filter->session, gf_filter_relink_task, cur_filter, NULL, "relink_dst", cur_pidinst);
		return;
	}
	filter_dst = pidinst->filter;

	//walk down the filter chain and find the shortest path to our destination
	//stop when the current filter is not a one-to-one filter
	while (1) {
		u32 fchain_len;
		GF_FilterPidInst *an_inpid = NULL;
		if (cur_filter->num_input_pids>1) break;
		if (cur_filter->num_output_pids>1) break;

		an_inpid = gf_list_get(cur_filter->input_pids, 0);
		
		if (gf_filter_pid_caps_match(an_inpid->pid, filter_dst->freg, filter_dst, NULL, NULL, NULL, -1)) {
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
	gf_filter_renegociate_output_dst(link_from_pid, link_from_pid->filter, filter_dst, pidinst, GF_FALSE);
}

void gf_filter_renegociate_output_dst(GF_FilterPid *pid, GF_Filter *filter, GF_Filter *filter_dst, GF_FilterPidInst *pidi, Bool reconfig_only)
{
	Bool is_new_chain = GF_TRUE;
	GF_Filter *new_f;

	if (!filter_dst) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Internal error, lost destination for pid %s in filter %s while negotiating caps !!\n", pid->name, filter->name));
		return;
	}

	//try to load filters to reconnect output pid
	if (!reconfig_only && gf_filter_pid_caps_match(pid, filter_dst->freg, filter_dst, NULL, NULL, NULL, -1)) {
		GF_FilterPidInst *dst_pidi;
		new_f = pid->filter;
		assert(pid->num_destinations==1);
		dst_pidi = gf_list_get(pid->destinations, 0);
		//we are replacing the chain, remove filters until dest, keeping the final PID connected since we will detach
		// and reattach it
		if (!filter_dst->sticky) filter_dst->sticky = 2;
		gf_filter_remove_internal(dst_pidi->filter, filter_dst, GF_TRUE);
		is_new_chain = GF_FALSE;
		//we will reassign packets from that pid instance to the new connection
		filter_dst->swap_pidinst = dst_pidi;

	}
	//we are inserting a new chain
	else if (reconfig_only) {
		new_f = gf_filter_pid_resolve_link_for_caps(pid, filter_dst);
	} else {
		Bool reassigned;
		new_f = gf_filter_pid_resolve_link(pid, filter_dst, &reassigned);
	}
	if (! new_f) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("No suitable filter to adapat caps between pid %s in filter %s to filter %s - NOT CONNECTED!\n", pid->name, filter->name, filter_dst->name));

		if (pid->adapters_blacklist) {
			gf_list_del(pid->adapters_blacklist);
			pid->adapters_blacklist = NULL;
		}
		return;
	}
	//detach pid instance from its source pid
	if (pidi) {
		//signal as detached, this will prevent any further packet access
		safe_int_inc(&pidi->detach_pending);

		//we need to first reconnect the pid and then detach the output
		if (is_new_chain) {
			//signal a stream reset is pending to prevent filter entering endless loop
			safe_int_inc(&pidi->filter->stream_reset_pending);
			//keep track of the pidinst being detached in the new filter
			new_f->swap_pidinst = pidi;
			new_f->swap_needs_init = GF_TRUE;
		}
		//we directly detach the pid
		else {
			gf_fs_post_task(filter->session, gf_filter_pid_detach_task, filter_dst, pidi->pid, "pidinst_detach", filter_dst);
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
	if (safe_int_dec(&pid->caps_negociate->reference_count)==0) {
		gf_props_del(pid->caps_negociate);
	}
	pid->caps_negociate = NULL;
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
							gf_filter_renegociate_output_dst(pid, filter, filter_dst, pidi, GF_TRUE);
						}
					}
				}
				//we are deconnected (unload of a previous adaptation filter)
				else {
					filter_dst = pid->caps_dst_filter;
					assert(pid->num_destinations==0);
					pid->caps_dst_filter = NULL;
					gf_filter_renegociate_output_dst(pid, filter, filter_dst, NULL, GF_TRUE);
				}
			}
			if (safe_int_dec(&pid->caps_negociate->reference_count)==0) {
				gf_props_del(pid->caps_negociate);
			}
			pid->caps_negociate = NULL;
			pid->caps_negociate_pidi = NULL;
		}
	}
}

static void gf_filter_check_pending_tasks(GF_Filter *filter, GF_FSTask *task)
{
	safe_int_dec(&filter->process_task_queued);
	if (filter->session->run_status!=GF_OK) return;

	if (filter->process_task_queued) {
		task->requeue_request = GF_TRUE;
	} else if (filter->pending_packets) {
		safe_int_inc(&filter->process_task_queued);
		task->requeue_request = GF_TRUE;
	} else {
		task->requeue_request = GF_FALSE;
	}
}

#ifdef GPAC_MEMORY_TRACKING
static GF_Err gf_filter_process_check_alloc(GF_Filter *filter)
{
	GF_Err e;
	size_t gf_mem_get_stats(unsigned int *nb_allocs, unsigned int *nb_callocs, unsigned int *nb_reallocs, unsigned int *nb_free);
	u32 nb_allocs=0, nb_callocs=0, nb_reallocs=0, nb_free=0;
	u32 prev_nb_allocs=0, prev_nb_callocs=0, prev_nb_reallocs=0, prev_nb_free=0;

	//reset alloc/realloc stats of filter
	filter->nb_alloc_pck = 0;
	filter->nb_realloc_pck = 0;
	//get current alloc state
	gf_mem_get_stats(&prev_nb_allocs, &prev_nb_callocs, &prev_nb_reallocs, &prev_nb_free);
	e = filter->freg->process(filter);

	//get new alloc state
	gf_mem_get_stats(&nb_allocs, &nb_callocs, &nb_reallocs, &nb_free);
	//remove internal allocs/reallocs due to filter lib
	assert (nb_allocs>=filter->nb_alloc_pck);
	assert (nb_reallocs>=filter->nb_realloc_pck);
	nb_allocs -= filter->nb_alloc_pck;
	nb_reallocs -= filter->nb_realloc_pck;

	//remove prev alloc stats
	nb_allocs -= prev_nb_allocs;
	nb_callocs -= prev_nb_callocs;
	nb_reallocs -= prev_nb_reallocs;
	nb_free -= prev_nb_free;

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


static void gf_filter_process_task(GF_FSTask *task)
{
	GF_Err e;
	GF_Filter *filter = task->filter;
	assert(task->filter);
	assert(filter->freg);
	assert(filter->freg->process);
	task->is_filter_process = GF_TRUE;

	filter->schedule_next_time = 0;
	if (filter->out_pid_connection_pending || filter->detached_pid_inst) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has %s pending, requeuing process\n", filter->name, filter->out_pid_connection_pending ? "connections" : "input pid reassignments"));
		//do not cancel the process task since it might have been triggered by the filter itself,
		//we would not longer call it
		task->requeue_request = GF_TRUE;
		return;
	}
	if (filter->removed || filter->finalized) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s has been removed, skiping process\n", filter->name));
		return;
	}
	if (filter->would_block && (filter->would_block == filter->num_output_pids) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s blocked, skiping process\n", filter->name));
		filter->nb_tasks_done--;
		gf_filter_check_pending_tasks(filter, task);
		return;
	}
	if (filter->stream_reset_pending) {
		filter->nb_tasks_done--;
		task->requeue_request = GF_TRUE;
		return;
	}
#if 0
	//empty input for this filter, don't call process
	else if (filter->num_input_pids==1 && !filter->pending_packets && !filter->skip_process_trigger_on_tasks) {
		filter->nb_tasks_done--;
		gf_filter_check_pending_tasks(filter, task);
		return;
	}
#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter %s process\n", filter->name));

	if (task->filter->postponed_packets) {
		while (gf_list_count(task->filter->postponed_packets)) {
			GF_FilterPacket *pck = gf_list_pop_front(task->filter->postponed_packets);
			gf_filter_pck_send(pck);
		}
		gf_list_del(task->filter->postponed_packets);
		task->filter->postponed_packets = NULL;
		if (task->filter->process_task_queued==1) {
			task->filter->process_task_queued = 0;
			return;
		}
	}
	FSESS_CHECK_THREAD(filter)

	if (task->filter->nb_caps_renegociate) {
		gf_filter_renegociate_output(task->filter);
	}

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
		filter->process_task_queued = 0;
		return;
	}
	if (e) filter->session->last_process_error = e;
	
	//source filters, flush data if enough space available. If the sink  returns EOS, don't repost the task
	if (!filter->would_block && !filter->input_pids && (e!=GF_EOS)) {
		if (filter->schedule_next_time)
			task->schedule_next_time = filter->schedule_next_time;
		task->requeue_request = GF_TRUE;
	}
	//last task for filter but pending packets and not blocking, requeue in main scheduler
	else if ((filter->would_block < filter->num_output_pids)
			&& filter->pending_packets
			&& (gf_fq_count(filter->tasks)<=1)
			&& !filter->skip_process_trigger_on_tasks)
		task->requeue_request = GF_TRUE;

	//filter requested a requeue
	else if (filter->schedule_next_time) {
		task->schedule_next_time = filter->schedule_next_time;
		task->requeue_request = GF_TRUE;
	}
	else {
		assert (!filter->schedule_next_time);
		gf_filter_check_pending_tasks(filter, task);
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
			filter->process_task_queued = 0;
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
		filter->process_task_queued = 0;
		return;
	}
	if (e) filter->session->last_process_error = e;
}

void gf_filter_send_update(GF_Filter *filter, const char *fid, const char *name, const char *val)
{
	gf_fs_send_update(filter->session, fid, name, val);
}

GF_Filter *gf_filter_clone(GF_Filter *filter)
{
	GF_Filter *new_filter = gf_filter_new(filter->session, filter->freg, filter->orig_args, NULL, filter->arg_type, NULL);
	if (!new_filter) return NULL;
	new_filter->cloned_from = filter;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_FILTER, ("Filter cloned (registry %s, args %s)\n", filter->freg->name, filter->orig_args ? filter->orig_args : "none"));

	return new_filter;
}

u32 gf_filter_get_ipid_count(GF_Filter *filter)
{
	return filter->num_input_pids;
}

GF_FilterPid *gf_filter_get_ipid(GF_Filter *filter, u32 idx)
{
	return gf_list_get(filter->input_pids, idx);
}

u32 gf_filter_get_opid_count(GF_Filter *filter)
{
	return filter->num_output_pids;
}

GF_FilterPid *gf_filter_get_opid(GF_Filter *filter, u32 idx)
{
	return gf_list_get(filter->output_pids, idx);
}

GF_FilterSession *gf_filter_get_session(GF_Filter *filter)
{
	if (filter) return filter->session;
	return NULL;
}

void gf_filter_post_process_task(GF_Filter *filter)
{
	if (filter->finalized || filter->removed) return;
	
	safe_int_inc(&filter->process_task_queued);
	if (filter->process_task_queued<=1) {
		assert(!filter->finalized);
		gf_fs_post_task(filter->session, gf_filter_process_task, filter, NULL, "process", NULL);
	}
}

void gf_filter_ask_rt_reschedule(GF_Filter *filter, u32 us_until_next)
{
	if (!filter->in_process) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Filter %s request for real-time reschedule but filter is not in process\n", filter->name));
		return;
	}
	if (!us_until_next) return;
	filter->schedule_next_time = 1+us_until_next + gf_sys_clock_high_res();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCHEDULER, ("Filter %s real-time reschedule in %d us (at "LLU" sys clock)\n", filter->name, us_until_next, filter->schedule_next_time));
}

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
	GF_Filter *f = ((struct _gf_filter_setup_failure *)task->udta)->filter;
	gf_free(task->udta);

	if (!f->finalized && f->freg->finalize) {
		FSESS_CHECK_THREAD(f)
		f->freg->finalize(f);
	}
	if (f->session->filters_mx) gf_mx_p(f->session->filters_mx);

	res = gf_list_del_item(f->session->filters, f);
	assert (res >=0 );

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

void gf_filter_setup_failure(GF_Filter *filter, GF_Err reason)
{
	//don't accept twice a notif
	if (filter->setup_notified) return;
	filter->setup_notified = GF_TRUE;

	gf_filter_notification_failure(filter, reason, GF_TRUE);
}
void gf_filter_post_task(GF_Filter *filter, gf_fs_task_callback task_fun, void *udta, const char *task_name)
{
	gf_fs_post_task(filter->session, task_fun, filter, NULL, task_name, udta);
}

void gf_filter_remove_task(GF_FSTask *task)
{
	s32 res;
	GF_Filter *f = task->filter;
	u32 count = gf_fq_count(f->tasks);

	if (f->out_pid_connection_pending) {
		task->requeue_request = GF_TRUE;
		return;
	}

	assert(f->finalized);
	if (task->in_main_task_list_only) count++;
	if (count!=1) {
		task->requeue_request = GF_TRUE;
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
	if (!task->in_main_task_list_only) {
		gf_fq_pop(f->tasks);
	}

	if (f->freg->finalize) {
		FSESS_CHECK_THREAD(f)
		f->freg->finalize(f);
	}

	if (f->session->filters_mx) gf_mx_p(f->session->filters_mx);

	res = gf_list_del_item(f->session->filters, f);
	assert (res >=0 );

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

void gf_filter_remove(GF_Filter *filter, GF_Filter *until_filter)
{
	gf_filter_remove_internal(filter, until_filter, GF_FALSE);
}


Bool gf_filter_swap_source_registry(GF_Filter *filter)
{
	u32 i;
	char *src_url=NULL;
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

	filter->finalized = GF_FALSE;
	gf_fs_load_source_dest_internal(filter->session, src_url, NULL, NULL, &e, filter, filter->dst_filter, GF_TRUE);
	//we manage to reassign an input registry
	if (e==GF_OK) {
		return GF_TRUE;
	}
	//nope ...
	gf_filter_setup_failure(filter, e);
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
		GF_PropertyMap *map = gf_list_last(pid->properties);
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

GF_Filter *gf_filter_connect_source(GF_Filter *filter, const char *url, const char *parent_url, GF_Err *err)
{
	return gf_fs_load_source_dest_internal(filter->session, url, NULL, parent_url, err, NULL, filter, GF_TRUE);
}

GF_Filter *gf_filter_connect_destination(GF_Filter *filter, const char *url, GF_Err *err)
{
	return gf_fs_load_source_dest_internal(filter->session, url, NULL, NULL, err, NULL, filter, GF_FALSE);
}


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

void gf_filter_make_sticky(GF_Filter *filter)
{
	if (filter) filter->sticky = 1;
}

//recursievely go up the filter chain and count queued events
//THIS FUNCTION IS NOT THREAD SAFE
u32 gf_filter_get_num_events_queued(GF_Filter *filter)
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

void gf_filter_hint_single_clock(GF_Filter *filter, u64 time_in_us, Double media_timestamp)
{
	//for now only one clock hint possible ...
	filter->session->hint_clock_us = time_in_us;
	filter->session->hint_timestamp = media_timestamp;
}

void gf_filter_get_clock_hint(GF_Filter *filter, u64 *time_in_us, Double *media_timestamp)
{
	//for now only one clock hint possible ...
	if (time_in_us) *time_in_us = filter->session->hint_clock_us;
	if (media_timestamp) *media_timestamp = filter->session->hint_timestamp;
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
		sprintf(szID, "__gpac__%p__", link_from);
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

void gf_filter_init_play_event(GF_FilterPid *pid, GF_FilterEvent *evt, Double start, Double speed, const char *log_name)
{
	u32 pmode = GF_PLAYBACK_MODE_NONE;
	const GF_PropertyValue *p;
	memset(evt, 0, sizeof(GF_FilterEvent));
	evt->base.type = GF_FEVT_PLAY;
	evt->base.on_pid = pid;

	evt->play.speed = 1.0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
	if (p) pmode = p->value.uint;

	evt->play.start_range = start;
	if (start<0) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
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

