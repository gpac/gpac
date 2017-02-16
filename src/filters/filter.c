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


GF_Filter *gf_filter_new(GF_FilterSession *fsess, const GF_FilterRegister *registry)
{
	char szName[200];
	GF_Filter *filter;
	assert(fsess);

	GF_SAFEALLOC(filter, GF_Filter);
	if (!filter) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Failed to alloc filter for %s\n", registry->name));
		return NULL;
	}
	filter->freg = registry;
	filter->session = fsess;
	if (filter->freg->construct) {
		GF_Err e = filter->freg->construct(filter);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Error %s while instantiating filter %s\n", gf_error_to_string(e), registry->name));
			gf_free(filter);
			return NULL;
		}
	}

	if (fsess->use_locks) {
		snprintf(szName, 200, "Filter%sPackets", filter->freg->name);
		filter->pcks_mx = gf_mx_new(szName);
		snprintf(szName, 200, "Filter%sProps", filter->freg->name);
		filter->props_mx = gf_mx_new(szName);
	}
	//for now we always use a lock on the filter task lists
	//TODO: this is our only lock in lock-free mode, we need to find a way to avoid this lock
	//maybe using permanent filter task?
	snprintf(szName, 200, "Filter%sTasks", filter->freg->name);
	filter->tasks_mx = gf_mx_new(szName);


	filter->tasks = gf_fq_new(filter->tasks_mx);

	filter->pcks_shared_reservoir = gf_fq_new(filter->pcks_mx);
	filter->pcks_alloc_reservoir = gf_fq_new(filter->pcks_mx);
	filter->pcks_inst_reservoir = gf_fq_new(filter->pcks_mx);

	filter->prop_maps_list_reservoir = gf_fq_new(filter->props_mx);
	filter->prop_maps_reservoir = gf_fq_new(filter->props_mx);
	filter->prop_maps_entry_reservoir = gf_fq_new(filter->props_mx);

	gf_list_add(fsess->filters, filter);

	gf_filter_set_name(filter, NULL);
	return filter;
}

void gf_filter_del(GF_Filter *filter)
{
	assert(filter);

	//delete output pids the packet reservoir
	while (gf_list_count(filter->output_pids)) {
		GF_FilterPid *pid = gf_list_pop_back(filter->output_pids);
		gf_filter_pid_del(pid);
	}
	gf_list_del(filter->output_pids);

	gf_list_del(filter->input_pids);

	gf_fq_del(filter->tasks, gf_void_del);

	//delete filter params before the props reservoir
	if (filter->params) gf_props_del(filter->params);

	gf_fq_del(filter->pcks_shared_reservoir, gf_void_del);
	gf_fq_del(filter->pcks_inst_reservoir, gf_void_del);
	gf_fq_del(filter->pcks_alloc_reservoir, gf_filterpacket_del);

	gf_fq_del(filter->prop_maps_reservoir, gf_void_del);
	gf_fq_del(filter->prop_maps_list_reservoir, (gf_destruct_fun) gf_list_del);
	gf_fq_del(filter->prop_maps_entry_reservoir, gf_void_del);

	gf_mx_del(filter->pcks_mx);
	gf_mx_del(filter->props_mx);
	gf_mx_del(filter->tasks_mx);

	if (filter->name) gf_free(filter->name);
	if (filter->id) gf_free(filter->id);
	if (filter->source_ids) gf_free(filter->source_ids);
	gf_free(filter);
}



void gf_filter_set_udta(GF_Filter *filter, void *udta)
{
	assert(filter);

	filter->filter_udta = udta;
}

void *gf_filter_get_udta(GF_Filter *filter)
{
	assert(filter);

	return filter->filter_udta;
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
void gf_filter_set_sources(GF_Filter *filter, const char *sources_ID)
{
	assert(filter);

	if (filter->source_ids) gf_free(filter->source_ids);
	filter->source_ids = sources_ID ? gf_strdup(sources_ID) : NULL;
}

static Bool gf_filter_set_args(GF_FSTask *task)
{
	assert(task->filter);
	assert(task->filter->freg);
	assert(task->filter->freg->update_args);

	task->filter->freg->update_args(task->filter);
	return GF_FALSE;
}


void gf_filter_parse_args(GF_Filter *filter, const char *args)
{
	u32 i=0;
	char *szArg;
	u32 alloc_len=1024;
	if (!filter) return;
	if (!args && filter->freg->update_args) {
		gf_fs_post_task(filter->session, gf_filter_set_args, filter, NULL, "set_args");
		return;
	}

	szArg = gf_malloc(sizeof(char)*1024);

	//parse each arg
	while (args) {
		char *value;
		u32 len;
		Bool found=GF_FALSE;
		//look for our arg separator
		char *sep = strchr(args, ':');

		//watchout for "C:\\"
		while (sep && (sep[1]=='\\')) {
			sep = strchr(sep+1, ':');
		}
		if (sep) len = sep-args;
		else len = strlen(args);

		if (len>=alloc_len) {
			alloc_len = len+1;
			szArg = gf_realloc(szArg, sizeof(char)*len);
		}
		strncpy(szArg, args, len);
		szArg[len]=0;

		value = strchr(szArg, '=');
		if (value) {
			value[0] = 0;
			value++;
		}
		i=0;
		while (filter->freg->args) {
			const GF_FilterArgs *a = &filter->freg->args[i];
			i++;
			if (!a || !a->arg_name) break;

			if (!strcmp(a->arg_name, szArg)) {
				GF_PropertyValue argv;
				found=GF_TRUE;

				argv = gf_props_parse_value(a->arg_type, a->arg_name, value);

				if (argv.type != GF_PROP_FORBIDEN) {
					if (!filter->params) filter->params = gf_props_new(filter);
					gf_props_set_property(filter->params, 0, a->arg_name, NULL, &argv);
				}
			}
		}
		if (!found) {
			if (!strcmp("FID", szArg)) {
				gf_filter_set_id(filter, value);
				found = GF_TRUE;
			}
			else if (!strcmp("SID", szArg)) {
				gf_filter_set_sources(filter, value);
				found = GF_TRUE;
			}
		}


		if (!found) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("Argument \"%s\" not found in filter options, ignoring\n", szArg));
		}
		if (sep) {
			args=sep+1;
		} else {
			args=NULL;
		}
	}
	gf_free(szArg);

	//instantiate all others with defauts value
	i=0;
	while (filter->freg->args) {
		GF_PropertyValue argv;
		const GF_FilterArgs *a = &filter->freg->args[i];
		i++;
		if (!a || !a->arg_name) break;
		if (!a->arg_default_val) continue;

		if (gf_filter_get_property(filter, a->arg_name) != NULL) continue;

		argv = gf_props_parse_value(a->arg_type, a->arg_name, a->arg_default_val);

		if (argv.type != GF_PROP_FORBIDEN) {
			if (!filter->params) filter->params = gf_props_new(filter);
			gf_props_set_property(filter->params, 0, a->arg_name, NULL, &argv);
		}
	}


	if (filter->freg->update_args) {
		gf_fs_post_task(filter->session, gf_filter_set_args, filter, NULL, "set_args");
	}

}

Bool gf_filter_process(GF_FSTask *task)
{
	GF_Err e;
	assert(task->filter);
	assert(task->filter->freg);
	assert(task->filter->freg->process);

	e = task->filter->freg->process(task->filter);

	//source filters, flush data if enough space available. If the sink  returns EOS, don't repost the task
	if ( !task->filter->input_pids && task->filter->output_pids && (e!=GF_EOS))
		return GF_TRUE;

	//non-source filter, re-post task if input not empty
	if ( (gf_fq_count(task->filter->tasks)==0) && task->filter->pending_packets)
		return GF_TRUE;

	return GF_FALSE;
}

const GF_PropertyValue *gf_filter_get_property(GF_Filter *filter, const char *prop_name)
{
	if (!filter || !filter->params) return NULL;
	return gf_props_get_property(filter->params, 0, prop_name);
}


