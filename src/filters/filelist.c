/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2019
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

	Bool is_playing;
} FileListPid;

typedef struct
{
	char *file_name;
	u64 last_mod_time;
	u64 file_size;
} FileListEntry;

enum
{
	FL_SORT_NONE=0,
	FL_SORT_NAME,
	FL_SORT_SIZE,
	FL_SORT_DATE,
	FL_SORT_DATEX,
};

typedef struct
{
	//opts
	Bool revert;
	s32 floop;
	u32 fsort;
	GF_List *srcs;
	GF_Fraction fdur;
	u32 timescale;

	GF_FilterPid *file_pid;
	char *file_path;
	u32 last_url_crc;
	u32 last_url_lineno;
	Bool load_next;

	GF_List *filter_srcs;
	GF_List *io_pids;
	Bool is_eos;

	//in 1000000 Hz
	u64 cts_offset, dts_offset, dts_sub_plus_one;

	u32 nb_repeat;
	Double start, stop;

	GF_List *file_list;
	s32 file_list_idx;

	u64 current_file_dur;

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
	gf_filter_pid_init_play_event(iopid->ipid, &evt, ctx->start, 1.0, "FileList");
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
		else {
			iopid = gf_filter_pid_get_udta(pid);
			if (iopid) iopid->ipid = NULL;
		}
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
		if (!iopid) return GF_OUT_OF_MEM;
		iopid->ipid = pid;
		if (ctx->timescale) iopid->o_timescale = ctx->timescale;
		else {
			iopid->o_timescale = gf_filter_pid_get_timescale(pid);
			if (!iopid->o_timescale) iopid->o_timescale = 1000;
		}
		gf_list_add(ctx->io_pids, iopid);
	}
	gf_filter_pid_set_udta(pid, iopid);

	if (!iopid->opid) {
		iopid->opid = gf_filter_pid_new(filter);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		assert(p);
		iopid->stream_type = p->value.uint;
	}

	gf_filter_pid_reset_properties(iopid->opid);
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(iopid->opid, iopid->ipid);
	//we could further optimize by querying the duration of all sources in the list
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_NONE) );
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(iopid->o_timescale) );
	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_NB_FRAMES, NULL );

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
	iopid->single_frame = (p && (p->value.uint==1)) ? GF_TRUE : GF_FALSE ;
	iopid->timescale = gf_filter_pid_get_timescale(pid);
	if (!iopid->timescale) iopid->timescale = 1000;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (p) {
		s64 delay = p->value.sint;
		delay *= iopid->timescale;
		delay /= iopid->o_timescale;
	}

	//if we reattached the input, we must send a play request
	if (reassign) {
		filelist_start_ipid(ctx, iopid);
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
	if (p)
	 	gf_filter_pid_set_property(iopid->opid, GF_PROP_PID_URL, p);

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

		fevt.base.on_pid = iopid->ipid;
		if (evt->base.type==GF_FEVT_PLAY) {
			gf_filter_pid_init_play_event(iopid->ipid, &fevt, ctx->start, 1.0, "FileList");
			iopid->is_playing = GF_TRUE;
			iopid->is_eos = GF_FALSE;
		} else if (evt->base.type==GF_FEVT_STOP) {
			iopid->is_playing = GF_FALSE;
			iopid->is_eos = GF_TRUE;
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
	u32 nb_repeat=0, lineno=0;
	Double start=0, stop=0;

	if (ctx->file_list) {
		FileListEntry *fentry, *next;

		if (ctx->revert) {
			if (!ctx->file_list_idx) {
				if (!ctx->floop) return GF_FALSE;
				if (ctx->floop>0) ctx->floop--;
				ctx->file_list_idx = gf_list_count(ctx->file_list);
			}
			ctx->file_list_idx --;
		} else {
			ctx->file_list_idx ++;
			if (ctx->file_list_idx >= (s32) gf_list_count(ctx->file_list)) {
				if (!ctx->floop) return GF_FALSE;
				if (ctx->floop>0) ctx->floop--;
				ctx->file_list_idx = 0;
			}
		}
		fentry = gf_list_get(ctx->file_list, ctx->file_list_idx);
		strncpy(szURL, fentry->file_name, sizeof(char)*(GF_MAX_PATH-1) );
		szURL[GF_MAX_PATH-1] = 0;
		next = gf_list_get(ctx->file_list, ctx->file_list_idx + 1);
		if (next)
			ctx->current_file_dur = next->last_mod_time - fentry->last_mod_time;
		return GF_TRUE;
	}


	f = gf_fopen(ctx->file_path, "rt");
	while (f) {
		u32 crc;
		char *l = gf_fgets(szURL, GF_MAX_PATH, f);
		if (!l || gf_feof(f)) {
			if (ctx->floop != 0) {
				gf_fseek(f, 0, SEEK_SET);
				//load first line
				last_found = GF_TRUE;
				continue;
			}
			gf_fclose(f);
			return GF_FALSE;
		}
		lineno++;

		len = (u32) strlen(szURL);
		while (len && strchr("\n\r\t ", szURL[len-1])) {
			szURL[len-1] = 0;
			len--;
		}
		if (!len) continue;

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
			ctx->last_url_lineno = lineno;
			break;
		}
		if ((ctx->last_url_crc == crc) && (ctx->last_url_lineno == lineno)) {
			last_found = GF_TRUE;
			nb_repeat=0;
			start=stop=0;
			ctx->szCom[0] = 0;
			continue;
		}
		if (last_found) {
			ctx->last_url_crc = crc;
			ctx->last_url_lineno = lineno;
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

	if (ctx->is_eos)
		return GF_EOS;

	if (!ctx->file_list) {
		GF_FilterPacket *pck;
		if (!ctx->file_pid) {
			return GF_EOS;
		}
		pck = gf_filter_pid_get_packet(ctx->file_pid);
		if (pck) {
			gf_filter_pck_get_framing(pck, &start, &end);
			gf_filter_pid_drop_packet(ctx->file_pid);

			if (end) {
				Bool is_first = GF_TRUE;
				FILE *f=NULL;
				p = gf_filter_pid_get_property(ctx->file_pid, GF_PROP_PID_FILEPATH);
				if (p) {
					if (ctx->file_path) {
						gf_free(ctx->file_path);
						is_first = GF_FALSE;
					}
					ctx->file_path = gf_strdup(p->value.string);
					f = gf_fopen(p->value.string, "rt");
				}
				if (!f) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Unable to open file %s\n", ctx->file_path ? ctx->file_path : "no source path"));
					return GF_SERVICE_ERROR;
				} else {
					gf_fclose(f);
					ctx->load_next = is_first;
				}
			}
		}
	}

	count = gf_list_count(ctx->io_pids);

	if (ctx->load_next) {
		GF_Filter *fsrc;
		char *url;
		char szURL[GF_MAX_PATH];

		while (gf_list_count(ctx->filter_srcs)) {
			fsrc = gf_list_pop_back(ctx->filter_srcs);
			gf_filter_remove_src(filter, fsrc);
		}
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
		url = szURL;
		while (url) {
			char *sep = strstr(url, " && ");
			if (sep) sep[0] = 0;

			fsrc = gf_filter_connect_source(filter, url, ctx->file_path, &e);
			if (e) {
				if (sep) sep[0] = ' ';
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] Failed to open file %s: %s\n", szURL, gf_error_to_string(e) ));
				return GF_SERVICE_ERROR;
			}
			gf_list_add(ctx->filter_srcs, fsrc);

			if (!sep) break;
			sep[0] = ' ';
			url = sep+4;
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
		if (!iopid->is_playing) {
			continue;
		}
		if (gf_filter_pid_would_block(iopid->opid))
			continue;

		while (1) {
			GF_FilterPacket *dst_pck;
			u64 cts, dts;
			u32 dur;
			GF_FilterPacket *pck;

			pck = gf_filter_pid_get_packet(iopid->ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(iopid->ipid))
					iopid->is_eos = GF_TRUE;

				if (iopid->is_eos)
					nb_done++;
				break;
			}

			if (gf_filter_pid_would_block(iopid->opid))
				break;

			dst_pck = gf_filter_pck_new_ref(iopid->opid, NULL, 0, pck);
			gf_filter_pck_merge_properties(pck, dst_pck);
			dts = gf_filter_pck_get_dts(pck);
			if (dts==GF_FILTER_NO_TS) dts=0;

			cts = gf_filter_pck_get_cts(pck);
			if (cts==GF_FILTER_NO_TS) cts=0;

			if (iopid->single_frame && (ctx->fsort==FL_SORT_DATEX) ) {
				dur = (u32) ctx->current_file_dur;
				//move from second to input pid timescale
				dur *= iopid->timescale;
			} else if (iopid->single_frame && ctx->fdur.num && ctx->fdur.den) {
				s64 pdur = ctx->fdur.num;
				pdur *= iopid->timescale;
				pdur /= ctx->fdur.den;
				dur = (u32) pdur;
			} else {
				dur = gf_filter_pck_get_duration(pck);
			}

			if (iopid->timescale == iopid->o_timescale) {
				gf_filter_pck_set_dts(dst_pck, iopid->dts_o + dts - iopid->dts_sub);
				gf_filter_pck_set_cts(dst_pck, iopid->cts_o + cts - iopid->dts_sub);
				gf_filter_pck_set_duration(dst_pck, dur);
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

		if (gf_filter_end_of_session(filter)) {
			for (i=0; i<count; i++) {
				iopid = gf_list_get(ctx->io_pids, i);
				gf_filter_pid_set_eos(iopid->opid);
			}
			ctx->is_eos = GF_TRUE;
			return GF_EOS;
		}
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
		ctx->cts_offset += max_dts;
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
			return filelist_process(filter);
		}
	}
	return GF_OK;
}

void filelist_add_entry(GF_FileListCtx *ctx, FileListEntry *fentry)
{
	u32 i, count;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUTHOR, ("[FileList] Adding file %s to list\n", fentry->file_name));
	if (ctx->fsort==FL_SORT_NONE) {
		gf_list_add(ctx->file_list, fentry);
		return;
	}
	count = gf_list_count(ctx->file_list);
	for (i=0; i<count; i++) {
		Bool insert=GF_FALSE;
		FileListEntry *cur = gf_list_get(ctx->file_list, i);
		switch (ctx->fsort) {
		case FL_SORT_SIZE:
			if (cur->file_size>fentry->file_size) insert = GF_TRUE;
			break;
		case FL_SORT_DATE:
		case FL_SORT_DATEX:
			if (cur->last_mod_time>fentry->last_mod_time) insert = GF_TRUE;
			break;
		case FL_SORT_NAME:
			if (strcmp(cur->file_name, fentry->file_name) > 0) insert = GF_TRUE;
			break;
		}
		if (insert) {
			gf_list_insert(ctx->file_list, fentry, i);
			return;
		}
	}
	gf_list_add(ctx->file_list, fentry);
}

Bool filelist_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	FileListEntry *fentry;
	GF_FileListCtx *ctx = cbck;
	if (file_info->hidden) return GF_FALSE;
	if (file_info->directory) return GF_FALSE;
	if (file_info->drive) return GF_FALSE;
	if (file_info->system) return GF_FALSE;

	GF_SAFEALLOC(fentry, FileListEntry);
	if (!fentry) return GF_TRUE;

	fentry->file_name = gf_strdup(item_path);
	fentry->file_size = file_info->size;
	fentry->last_mod_time = file_info->last_modified;
	filelist_add_entry(ctx, fentry);

	return GF_FALSE;
}

GF_Err filelist_initialize(GF_Filter *filter)
{
	u32 i, count;
	char *sep_dir, c=0, *dir, *pattern;
	GF_FileListCtx *ctx = gf_filter_get_udta(filter);
	ctx->io_pids = gf_list_new();

	if (!ctx->srcs || !gf_list_count(ctx->srcs)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[FileList] No inputs\n"));
		//not completely correct, needs further testing
#if 0
		if (!gf_filter_connections_pending(filter)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] No source specified and no input PIDs pending, cannot instantiate\n"));
			return GF_BAD_PARAM;
		}
#endif
		return GF_OK;
	}
	ctx->filter_srcs = gf_list_new();
	ctx->file_list = gf_list_new();
	count = gf_list_count(ctx->srcs);
	for (i=0; i<count; i++) {
		char *list = gf_list_get(ctx->srcs, i);

		if (strchr(list, '*') ) {
			sep_dir = strrchr(list, '/');
			if (!sep_dir) sep_dir = strrchr(list, '\\');
			if (sep_dir) {
				c = sep_dir[0];
				sep_dir[0] = 0;
				dir = list;
				pattern = sep_dir+2;
			} else {
				dir = ".";
				pattern = list;
			}
			gf_enum_directory(dir, GF_FALSE, filelist_enum, ctx, pattern);
			if (c && sep_dir) sep_dir[0] = c;
		} else {
			if (gf_file_exists(list)) {
				FILE *fo;
				FileListEntry *fentry;
				GF_SAFEALLOC(fentry, FileListEntry);
				if (fentry) {
					fentry->file_name = gf_strdup(list);
					fentry->last_mod_time = gf_file_modification_time(list);
					fo = gf_fopen(list, "rb");
					if (fo) {
						fentry->file_size = gf_fsize(fo);
						gf_fclose(fo);
					}
					filelist_add_entry(ctx, fentry);
				}
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[FileList] File %s not found, ignoring\n", list));
			}
		}
	}

	if (!gf_list_count(ctx->file_list)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[FileList] No files found in list %s\n", ctx->srcs));
		return GF_BAD_PARAM;
	}
	if (ctx->fsort==FL_SORT_DATEX) {
		ctx->revert = GF_FALSE;
		ctx->floop = 0;
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
			FileListEntry *fentry = gf_list_pop_back(ctx->file_list);
			gf_free(fentry->file_name);
			gf_free(fentry);
		}
		gf_list_del(ctx->file_list);
	}
	gf_list_del(ctx->io_pids);
	gf_list_del(ctx->filter_srcs);
	if (ctx->file_path) gf_free(ctx->file_path);
}


#define OFFS(_n)	#_n, offsetof(GF_FileListCtx, _n)
static const GF_FilterArgs GF_FileListArgs[] =
{
	{ OFFS(floop), "loop playlist/list of files, `0` for one time, `n` for n+1 times, `-1` for indefinitely", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(srcs), "list of files to play - see filter help", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(fdur), "for source files with a single frame, sets frame duration. 0/NaN fraction means reuse source timing which is usually not set!", GF_PROP_FRACTION, "1/25", NULL, 0},
	{ OFFS(revert), "revert list of files (not playlist)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timescale), "force output timescale on all pids. 0 uses the timescale of the first pid found", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},

	{ OFFS(fsort), "sort list of files\n"
		"- no: no sorting, use default directory enumeration of OS\n"
		"- name: sort by alphabetical name\n"
		"- size: sort by increasing size\n"
		"- date: sort by increasing modification time\n"
		"- datex: sort by increasing modification time - see filter help"
		, GF_PROP_UINT, "no", "no|name|size|date|datex", 0},
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
	GF_FS_SET_DESCRIPTION("Sources concatenator")
	GF_FS_SET_HELP("This filter can be used to play playlist files or a list of sources.\n"
		"\n"
		"The filter loads any source supported by GPAC: remote or local files or streaming sessions (TS, RTP, DASH or other).\n"
		"The filter forces input demultiplex and recomputes the input timestamps into a continuous timeline.\n"
		"At each new source, the filter tries to remap input PIDs to already declared output PIDs of the same type, if any, or declares new output PIDs otherwise. If no input PID matches the type of an output, no packets are send for that PID.\n"
		"\n"
		"# Source list mode\n"
		"The source list mode is activated by using `flist:srcs=f1[,f2]`, where f1 can be a file or a directory to enum.\n"
		"The syntax for directory enum is:\n"
		"- dir/*: enumerates everything in dir\n"
		"- foo/*.png: enumerates all files with extension png in foo\n"
		"- foo/*.png;*.jpg: enumerates all files with extension png or jpg in foo\n"
		"\n"
		"The resulting file list can be sorted using [-fsort]().\n"
		"If the sort mode is `datex` and source files are images or single frame files, the following applies:\n"
		"- options [-floop](), [-revert]() and [-dur]() are ignored\n"
		"- the files are sorted by modification time\n"
		"- the first frame is assigned a timestamp of 0\n"
		"- each frame (coming from each file) is assigned a duration equal to the difference of modification time between the file and the next file\n"
		"- the last frame is assigned the same duration as the previous one\n"
		"\n"
		"# Playlist mode\n"
		"The playlist mode is activated when opening a playlist file (extension txt or m3u).\n"
		"In this mode, directives can be given in a comment line, i.e. a line starting with '#' before the line with the file name.\n"
		"The following directives, separated with space or comma, are supported:\n"
		"- repeat=N: repeats N times the content (hence played N+1)\n"
		"- start=T: tries to play the file from start time T seconds (double format only)\n"
		"Warning: This may not work with some files/formats not supporting seeking\n"
		"- stop=T: stops source playback after T seconds (double format only)\n"
		"This works on any source (implemented independently from seek support).\n"
		"\n"
		"The source lines follow the usual source syntax, see `gpac -h`.\n"
		"Additional pid properties can be added per source (see `gpac -h doc`), but are valid only for the current source, and reset at next source.\n"
		"\n"
		"The URL given can either be a single URL, or a list of URLs separated by \" && \" to load several sources for the active entry.\n"
		"EX audio.mp4 && video.mp4\n"
		"\n"
		"The playlist file is refreshed whenever the next source has to be reloaded in order to allow for dynamic pushing of sources in the playlist.\n"\
		"If the last URL played cannot be found in the playlist, the first URL in the playlist file will be loaded.\n")
	.private_size = sizeof(GF_FileListCtx),
	.max_extra_pids = -1,
	.flags = GF_FS_REG_ACT_AS_SOURCE | GF_FS_REG_REQUIRES_RESOLVER,
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

