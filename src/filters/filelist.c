/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / file concatenator filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif

typedef struct
{
	GF_FilterPid *ipid;
	GF_FilterPid *opid;
	u32 stream_type;
	//in current timescale
	u64 max_cts, max_dts;
	u32 o_timescale, timescale;
	//in original timescale
	u64 cts_o, dts_o;
	Bool single_frame;
	Bool is_eos;
	u64 dts_sub;
	u64 first_dts_plus_one;
} FileListPid;

typedef struct
{
	//opts
	Bool loop, revert;
	char *in;
	GF_Fraction dur;
	u32 timescale;

	GF_FilterPid *file_pid;
	const char *file_path;
	u32 last_url_crc;
	Bool load_next;

	GF_Filter *filter_src;
	GF_List *io_pids;
	Bool is_eos;

	//in 1000000 Hz
	u64 cts_offset, dts_offset, dts_sub_plus_one;

	u32 nb_repeat;
	Double start, stop;

	GF_List *file_list;
	s32 file_list_idx;

	char szCom[GF_MAX_PATH];
} GF_FileListCtx;

static const GF_FilterCapability FileListCapsSrc[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

static void filelist_start_ipid(GF_FileListCtx *ctx, FileListPid *iopid)
{
	GF_FilterEvent evt;
	iopid->is_eos = GF_FALSE;

	//if we reattached the input, we must send a play request
	gf_filter_init_play_event(iopid->ipid, &evt, ctx->start, 1.0, "FileList");
	gf_filter_pid_send_event(iopid->ipid, &evt);

	//and convert back cts/dts offsets from 1Mhs to OLD timescale (since we dispatch in this timescale)
	iopid->dts_o = ctx->dts_offset;
	iopid->dts_o *= iopid->o_timescale;
	iopid->dts_o /= 1000000;

	iopid->cts_o = ctx->cts_offset;
	iopid->cts_o *= iopid->o_timescale;
	iopid->cts_o /= 1000000;
	iopid->max_cts = iopid->max_dts = 0;

	iopid->first_dts_plus_one = 0;
}

#define FILELIST_SEP_SET	"# \n\r\t,"
void filelist_update_pid_props(GF_FileListCtx *ctx, GF_FilterPid *pid)
{
	char *com = ctx->szCom;
	while (com[0]) {
		char c;
		u32 end=0;
		//strip whitespace and comma
		while (com[end] && strchr(FILELIST_SEP_SET, com[end])) {
			end++;
		}
		if (!com[end]) break;
		com += end;
		//locate next whitespace/comma
		end=0;
		while (com[end] && !strchr(FILELIST_SEP_SET, com[end])) {
			end++;
		}
		c = com[end];
		com[end]=0;

		if (!strncmp(com, "repeat=", 7)) {}
		else if (!strncmp(com, "stop=", 5)) {}
		else if (!strncmp(com, "start=", 6)) {}
		else {
			char *sep = strchr(com, '=');
			if (!sep) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_AUTHOR, ("[FileList] %s doesn't look like a command syntax A=b, skipping\n", com));
			} else {
				u32 p4cc=0;
				u32 prop_type=0;
				sep[0] = 0;
				p4cc = gf_props_get_id(com);
				if (!p4cc && (strlen(com)==4) ) p4cc = GF_4CC(com[0], com[1], com[2], com[3]);
				if (p4cc) prop_type = gf_props_4cc_get_type(p4cc);

				if (prop_type != GF_PROP_FORBIDEN) {
					GF_PropertyValue p = gf_props_parse_value(prop_type, com, sep+1, NULL);
					if (prop_type==GF_PROP_NAME) {
						p.type = GF_PROP_STRING;
						gf_filter_pid_set_property(pid, p4cc, &p);
						p.type = GF_PROP_NAME;
					} else {
						gf_filter_pid_set_property(pid, p4cc, &p);
					}
					gf_props_reset_single(&p);
				} else {
					GF_PropertyValue p;
					memset(&p, 0, sizeof(GF_PropertyValue));
					p.type = GF_PROP_STRING;
					p.value.string = sep+1;
					gf_filter_pid_set_property_dyn(pid, com, &p);
				}
			}
		}
		com[end]=c;
		com += end;
	}
}

GF_Err filelist_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	FileListPid *iopid;
	const GF_PropertyValue *p;
	u32 i, count;
	Bool reassign = GF_FALSE;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (pid==ctx->file_pid)
			ctx->file_pid = NULL;
		return GF_OK;
	}

	if (!ctx->file_pid && !ctx->file_list) {
		if (! gf_filter_pid_check_caps(pid))
			return GF_NOT_SUPPORTED;
		ctx->file_pid = pid;
		//we want the complete file
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

		//we will declare pids later

		//from now on we only accept the above caps
		gf_filter_override_caps(filter, FileListCapsSrc, 2);
	}
	if (ctx->file_pid == pid) return GF_OK;

	iopid = NULL;
	count = gf_list_count(ctx->io_pids);
	for (i=0; i<count; i++) {
		iopid = gf_list_get(ctx->io_pids, i);
		if (iopid->ipid==pid) break;
		//check matching stream types if out pit not connected, and reuse if matching
		if (!iopid->ipid) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
			assert(p);
			if (p->value.uint == iopid->stream_type) {
				iopid->ipid = pid;
				iopid->timescale = gf_filter_pid_get_timescale(pid);
				reassign = GF_TRUE;
				break;
			}
		}
		iopid = NULL;
	}

	if (!iopid) {
		GF_SAFEALLOC(iopid, FileListPid);
		iopid->ipid = pid;
		if (ctx->timescale) iopid->o_timescale = ctx->timescale;
		else {
			iopid->o_timescale = gf_filter_pid_get_timescale(pid);
			if (!iopid->o_timescale) iopid->o_timescale = 1000;
		}
		gf_list_add(ctx->io_pids, iopid);
	}
	if (!iopid->opid) {
		iopid->opid = gf_filter_pid_new(filter);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		assert(p);
		iopid->stream_type = p->value.uint;
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(iopid->opid, iopid->ipid);
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_MEDIA_SKIP, NULL);
	//we could further optimize by querying the duration of all sources in the list
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_NONE) );
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(iopid->o_timescale) );
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_NB_FRAMES, NULL );

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
	iopid->single_frame = (p && (p->value.uint==1)) ? GF_TRUE : GF_FALSE ;
	iopid->timescale = gf_filter_pid_get_timescale(pid);
	if (!iopid->timescale) iopid->timescale = 1000;

	filelist_update_pid_props(ctx, iopid->opid);
	//if we reattached the input, we must send a play request
	if (reassign) {
		filelist_start_ipid(ctx, iopid);
	}

	return GF_OK;
}

static Bool filelist_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i, count;
	FileListPid *iopid;
	GF_FilterEvent fevt;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);

	//manually forward event to all input, except our file in pid
	memcpy(&fevt, evt, sizeof(GF_FilterEvent));
	count = gf_list_count(ctx->io_pids);
	for (i=0; i<count; i++) {
		iopid = gf_list_get(ctx->io_pids, i);
		if (!iopid->ipid) continue;

		if (evt->base.type==GF_FEVT_PLAY) {
			gf_filter_init_play_event(iopid->ipid, &fevt, ctx->start, 1.0, "FileList");
		} else {
			fevt.base.on_pid = iopid->ipid;
		}
		gf_filter_pid_send_event(iopid->ipid, &fevt);
	}
	//and cancel
	return GF_TRUE;
}

void filelist_parse_arg(char *com, char *name, Bool is_float, u32 *int_val, Double *float_val)
{
	char *val = strstr(com, name);
	if (val) {
		char c= 0;
		char *sep = strchr(val, ' ');
		if (!sep) sep = strchr(val, ',');
		if (!sep) sep = strchr(val, '\n');
		if (!sep) sep = strchr(val, '\r');
		if (sep) {
			c = sep[0];
			sep[0] = 0;
		}
		val += strlen(name);
		if (is_float) (*float_val) = atof(val);
		else (*int_val) = atoi(val);
		if (sep) sep[0] = c;
	}
}

Bool filelist_next_url(GF_FileListCtx *ctx, char szURL[GF_MAX_PATH])
{
	u32 len;
	Bool last_found = GF_FALSE;
	FILE *f;
	u32 nb_repeat=0;
	Double start=0, stop=0;

	if (ctx->file_list) {
		char *url;
		if (ctx->revert) {
			if (!ctx->file_list_idx) {
				if (!ctx->loop) return GF_FALSE;
				ctx->file_list_idx = gf_list_count(ctx->file_list);
			}
			ctx->file_list_idx --;
		} else {
			ctx->file_list_idx ++;
			if (ctx->file_list_idx >= (s32) gf_list_count(ctx->file_list)) {
				if (!ctx->loop) return GF_FALSE;
				ctx->file_list_idx = 0;
			}
		}
		url = gf_list_get(ctx->file_list, ctx->file_list_idx);
		strncpy(szURL, url, sizeof(char)*(GF_MAX_PATH-1) );
		return GF_TRUE;
	}


	f = gf_fopen(ctx->file_path, "rt");
	while (f) {
		u32 crc;
		fgets(szURL, GF_MAX_PATH, f);
		if (feof(f)) {
			if (ctx->loop) {
				gf_fseek(f, 0, SEEK_SET);
				//load first line
				last_found = GF_TRUE;
				continue;
			}
			gf_fclose(f);
			return GF_FALSE;
		}
		//comment
		if (szURL[0] == '#') {
			nb_repeat=0;
			start=stop=0;
			filelist_parse_arg(szURL, "repeat=", GF_FALSE, &nb_repeat, NULL);
			filelist_parse_arg(szURL, "start=", GF_TRUE, NULL, &start);
			filelist_parse_arg(szURL, "stop=", GF_TRUE, NULL, &stop);
			strcpy(ctx->szCom, szURL);
			continue;
		}
		crc = gf_crc_32(szURL, (u32) strlen(szURL) );
		if (!ctx->last_url_crc) {
			ctx->last_url_crc = crc;
			break;
		}
		if (ctx->last_url_crc == crc) {
			last_found = GF_TRUE;
			nb_repeat=0;
			start=stop=0;
			ctx->szCom[0] = 0;
			continue;
		}
		if (last_found) {
			ctx->last_url_crc = crc;
			break;
		}
		nb_repeat=0;
		start=stop=0;
		ctx->szCom[0] = 0;
	}
	gf_fclose(f);

	len = (u32) strlen(szURL);
	while (len && strchr("\r\n", szURL[len-1])) {
		szURL[len-1] = 0;
		len--;
	}
	ctx->nb_repeat = nb_repeat;
	ctx->start = start;
	ctx->stop = stop;
	return GF_TRUE;
}

GF_Err filelist_process(GF_Filter *filter)
{
	Bool start, end;
	GF_Err e;
	const GF_PropertyValue *p;
	u32 i, count, nb_done, nb_inactive;
	FileListPid *iopid;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;

	if (ctx->is_eos)
		return GF_EOS;

	if (!ctx->file_path && !ctx->file_list) {
		pck = gf_filter_pid_get_packet(ctx->file_pid);
		if (!pck) return GF_OK;
		gf_filter_pck_get_framing(pck, &start, &end);
		gf_filter_pid_drop_packet(ctx->file_pid);
		if (end) {
			FILE *f=NULL;
			p = gf_filter_pid_get_property(ctx->file_pid, GF_PROP_PID_FILEPATH);
			if (p) {
				ctx->file_path = p->value.string;
				f = gf_fopen(p->value.string, "rt");
			}
			if (!f) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Unable to open file %s\n", ctx->file_path ? ctx->file_path : "no source path"));
				return GF_SERVICE_ERROR;
			} else {
				gf_fclose(f);
				ctx->load_next = GF_TRUE;
			}
		}
	}

	count = gf_list_count(ctx->io_pids);

	if (ctx->load_next) {
		char szURL[GF_MAX_PATH];

		if (ctx->filter_src) gf_filter_remove(ctx->filter_src, filter);
		ctx->filter_src = NULL;
		ctx->load_next = GF_FALSE;

		if (! filelist_next_url(ctx, szURL)) {
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				gf_filter_pid_set_eos(iopid->opid);
				iopid->ipid = NULL;
			}
			ctx->is_eos = GF_TRUE;
			return GF_EOS;
		}
		ctx->filter_src = gf_filter_connect_source(filter, szURL, ctx->file_path, &e);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Failed to open file %s: %s\n", szURL, gf_error_to_string(e) ));
			return GF_SERVICE_ERROR;
		}
		//wait for PIDs to connect
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] Switching to file %s\n", szURL));
	}

	//init first timestamp
	if (!ctx->dts_sub_plus_one) {
		for (i=0; i<count; i++) {
			GF_FilterPacket *pck;
			u64 dts;
			iopid = gf_list_get(ctx->io_pids, i);
			if (!iopid->ipid) return GF_OK;
			pck = gf_filter_pid_get_packet(iopid->ipid);
			if (!pck) return GF_OK;

			dts = gf_filter_pck_get_dts(pck);
			if (dts==GF_FILTER_NO_TS) dts=0;

			dts *= 1000000;
			dts /= iopid->timescale;
			if (!ctx->dts_sub_plus_one) ctx->dts_sub_plus_one = dts + 1;
			else if (dts < ctx->dts_sub_plus_one - 1) ctx->dts_sub_plus_one = dts + 1;
	 	}
		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			iopid->dts_sub = ctx->dts_sub_plus_one - 1;
			iopid->dts_sub *= iopid->o_timescale;
			iopid->dts_sub /= 1000000;
		}
	}

	nb_done = nb_inactive = 0;
	for (i=0; i<count; i++) {
		iopid = gf_list_get(ctx->io_pids, i);
		if (!iopid->ipid) {
			nb_inactive++;
			continue;
		}
		if (iopid->is_eos) {
			nb_done++;
			continue;
		}
		if (gf_filter_pid_would_block(iopid->opid))
			continue;

		while (1) {
			GF_FilterPacket *dst_pck;
			u64 cts, dts;
			u32 dur;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(iopid->ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(iopid->ipid))
					iopid->is_eos = GF_TRUE;

				if (iopid->is_eos)
					nb_done++;
				break;
			}
			dst_pck = gf_filter_pck_new_ref(iopid->opid, NULL, 0, pck);
			gf_filter_pck_merge_properties(pck, dst_pck);
			dts = gf_filter_pck_get_dts(pck);
			if (dts==GF_FILTER_NO_TS) dts=0;

			cts = gf_filter_pck_get_cts(pck);
			if (cts==GF_FILTER_NO_TS) cts=0;

			if (iopid->single_frame && ctx->dur.num && ctx->dur.den) {
				dur = ctx->dur.num;
				dur *= iopid->timescale;
				dur /= ctx->dur.den;
			} else {
				dur = gf_filter_pck_get_duration(pck);
			}

			if (iopid->timescale == iopid->o_timescale) {
				gf_filter_pck_set_dts(dst_pck, iopid->dts_o + dts - iopid->dts_sub);
				gf_filter_pck_set_cts(dst_pck, iopid->cts_o + cts - iopid->dts_sub);
			} else {
				u64 ts = dts;
				ts *= iopid->o_timescale;
				ts /= iopid->timescale;
				gf_filter_pck_set_dts(dst_pck, iopid->dts_o + ts - iopid->dts_sub);
				ts = cts;
				ts *= iopid->o_timescale;
				ts /= iopid->timescale;
				gf_filter_pck_set_cts(dst_pck, iopid->cts_o + ts - iopid->dts_sub);

				ts = dur;
				ts *= iopid->o_timescale;
				ts /= iopid->timescale;
				gf_filter_pck_set_duration(dst_pck, (u32) ts);
			}
			dts += dur;
			cts += dur;
			if (dts > iopid->max_dts) iopid->max_dts = dts;
			if (cts > iopid->max_cts) iopid->max_cts = cts;

			//remember our first DTS
			if (!iopid->first_dts_plus_one) {
				iopid->first_dts_plus_one = dts + 1;
			}
			gf_filter_pck_send(dst_pck);
			gf_filter_pid_drop_packet(iopid->ipid);
			//if we have an end range, compute max_dts (includes dur) - firrst_dts
			if (ctx->stop>ctx->start) {
				if ( (ctx->stop-ctx->start) * iopid->timescale <= (iopid->max_dts - iopid->first_dts_plus_one + 1)) {
					iopid->is_eos = GF_TRUE;
					nb_done++;
					break;
				}
			}
		}
	}
	if ((nb_inactive!=count) && (nb_done+nb_inactive==count)) {
		//compute max cts and dts in 1Mhz timescale
		u64 max_cts = 0, max_dts = 0;
		ctx->dts_sub_plus_one = 0;
		for (i=0; i<count; i++) {
			iopid = gf_list_get(ctx->io_pids, i);
			if (iopid->ipid) {
				u64 ts;
				ts = iopid->max_cts - iopid->dts_sub;
				ts *= 1000000;
				ts /= iopid->timescale;
				if (max_cts < ts) max_cts = ts;

				ts = iopid->max_dts - iopid->dts_sub;
				ts *= 1000000;
				ts /= iopid->timescale;
				if (max_dts < ts) max_dts = ts;
			}
		}
		ctx->cts_offset += max_cts;
		ctx->dts_offset += max_dts;

		if (ctx->nb_repeat) {
			ctx->nb_repeat--;
			for (i=0; i<count; i++) {
				GF_FilterEvent evt;
				iopid = gf_list_get(ctx->io_pids, i);
				if (!iopid->ipid) continue;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, iopid->ipid);
				gf_filter_pid_send_event(iopid->ipid, &evt);

				iopid->is_eos = GF_FALSE;
				filelist_start_ipid(ctx, iopid);
			}
		} else {
			//detach all input pids and
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				iopid->ipid = NULL;
			}
			//force load
			ctx->load_next = GF_TRUE;
		}
	}
	return GF_OK;
}

Bool filelist_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	GF_FileListCtx *ctx = cbck;
	if (file_info->hidden) return GF_FALSE;
	if (file_info->directory) return GF_FALSE;
	if (file_info->drive) return GF_FALSE;
	if (file_info->system) return GF_FALSE;

	gf_list_add(ctx->file_list, gf_strdup(item_path));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUTHOR, ("[FileList] Adding file %s to list\n", item_path));
	return GF_FALSE;
}

GF_Err filelist_initialize(GF_Filter *filter)
{
	char *sep_dir, *sep, c=0, *dir, *pattern, *list;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);
	ctx->io_pids = gf_list_new();

	if (!ctx->in) return GF_OK;
	ctx->file_list = gf_list_new();

	list = ctx->in;
	while (list) {
		sep = strrchr(list, ',');
		if (sep) sep[0] = 0;

		if (strchr(list, '*') ) {
			sep_dir = strrchr(list, '/');
			if (!sep_dir) sep_dir = strrchr(list, '\\');
			if (sep_dir) {
				c = sep_dir[0];
				sep_dir[0] = 0;
				dir = list;
				pattern =  sep_dir+1;
			} else {
				dir = ".";
				pattern = list;
			}
			gf_enum_directory(dir, GF_FALSE, filelist_enum, ctx, pattern);
			if (sep_dir) sep_dir[0] = c;
		} else {
			if (gf_file_exists(list)) {
				gf_list_add(ctx->file_list, gf_strdup(list));
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[FileList] File %s not found, ignoring\n", list));
			}
		}
		if (!sep) break;
		sep[0] = ',';
		list = sep+1;
	}
	if (!gf_list_count(ctx->file_list)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] No files found in list %s\n", ctx->in));
		return GF_BAD_PARAM;
	}
	ctx->file_list_idx = ctx->revert ? gf_list_count(ctx->file_list) : -1;
	ctx->load_next = GF_TRUE;
	//from now on we only accept the above caps
	gf_filter_override_caps(filter, FileListCapsSrc, 2);
	//and we act as a source, request processing
	gf_filter_post_process_task(filter);
	//prevent deconnection of filter when no input
	gf_filter_make_sticky(filter);
	return GF_OK;
}

void filelist_finalize(GF_Filter *filter)
{
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);
	while (gf_list_count(ctx->io_pids)) {
		FileListPid *iopid = gf_list_pop_back(ctx->io_pids);
		gf_free(iopid);
	}
	if (ctx->file_list) {
		while (gf_list_count(ctx->file_list)) {
			gf_free(gf_list_pop_back(ctx->file_list));
		}
		gf_list_del(ctx->file_list);
	}
	gf_list_del(ctx->io_pids);
}


#define OFFS(_n)	#_n, offsetof(GF_FileListCtx, _n)
static const GF_FilterArgs GF_FileListArgs[] =
{
	{ OFFS(loop), "continuously loop playlist/list of files - see filter help", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(in), "list of files to play - see filter help", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(dur), "for source files with a single frame, sets frame duration. 0/NaN fraction means reuse source timing which is usually not set!", GF_PROP_FRACTION, "1/25", NULL, GF_FALSE},
	{ OFFS(revert), "revert list of files (not playlist)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(timescale), "forces output timescal on all pids. 0 uses the timescale of the first pid found", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{0}
};


static const GF_FilterCapability FileListCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "txt|m3u"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};


GF_FilterRegister FileListRegister = {
	.name = "flist",
	.description = "sources concatenator",
	.comment = "This filter can be used to play playlist files (extension txt or m3u) or a list of sources using flist:l=\"f1[,f2]\", where f1 can be a file or a directory to enum.\n"\
		"Syntax for directory is:\n"\
		"\tdir/*: enumerates everything in dir\n"\
		"\tfoo/*.png: enumerates all files with extension png in foo\n"\
		"\tfoo/*.png;*.jpg: enumerates all files with extension png or jpg in foo\n"\
		"\n"\
		"The filter loads any source supported by GPAC, files (remote or local) or other.\n"\
		"The filter forces input demultiplex (no streamtype FILE) and recomputes the input timestamps into a continuous timeline.\n"\
		"At each new source, the filter tries to remap input PIDs to already declared output PIDs of the same type, if any, or declares new output PIDs otherwise. If no input PID matches the type of an output, no packets are send for that PID.\n"\
		"\n"\
		"When using a playlist, directives can be given in a comment line (starting with '#' before the file name)\n"\
		"The following directives (separated with space or comma) are supported:\n"\
		"\trepeat=N: repeats N times the content (hence played N+1)\n"\
		"\tstart=T: tries to play the file from start time T seconds (double format only)\n"\
		"\t\t!! This may not work with some files/formats not supporting seeking\n"
		"\tstop=T: stops source playback after T seconds (double format only)\n"\
		"\t\tThis works on any source (implemented independetly from seek support)\n"
		"\tName=Val: sets output PIDs property (4cc, built-in name or any name) to the given value.\n"\
		"\t\tIf a non built-in property is used, the value will be delared as string\n"
		"\t\t!! Properties are not filtered and override the source props, be carefull not to break\n"
		"\t\tthe session by overriding core properties such as width/height/samplerate/... !!\n"
		"\t\tAdded properties are valid only for the current source, and are reseted at next source\n"
		"\n"\
		"The playlist file is refreshed whenever the next source has to be reloaded in order to allow for dynamic pushing of sources in the playlist\n"\
		"If the last URL played cannot be found in the playlist, the first URL in the playlist file will be loaded\n"\
	,
	.private_size = sizeof(GF_FileListCtx),
	.max_extra_pids = -1,
	.args = GF_FileListArgs,
	.initialize = filelist_initialize,
	.finalize = filelist_finalize,
	SETCAPS(FileListCaps),
	.configure_pid = filelist_configure_pid,
	.process = filelist_process,
	.process_event = filelist_process_event
};

const GF_FilterRegister *filelist_register(GF_FilterSession *session)
{
	return &FileListRegister;
}

