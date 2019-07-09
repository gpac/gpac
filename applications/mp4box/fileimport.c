/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4box application
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


#include "mp4box.h"

#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/scene_manager.h>
#include <gpac/network.h>
#include <gpac/base_coding.h>

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_SVG)
#include <gpac/scenegraph.h>
#endif


#ifndef GPAC_DISABLE_BIFS
#include <gpac/bifs.h>
#endif
#ifndef GPAC_DISABLE_VRML
#include <gpac/nodes_mpeg4.h>
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE

#include <gpac/xml.h>
#include <gpac/internal/isomedia_dev.h>

typedef struct
{
	const char *root_file;
	const char *dir;
	GF_List *imports;
} WGTEnum;

GF_Err set_file_udta(GF_ISOFile *dest, u32 tracknum, u32 udta_type, char *src, Bool is_box_array)
{
	u8 *data = NULL;
	GF_Err res = GF_OK;
	u32 size;
	bin128 uuid;
	memset(uuid, 0 , 16);

	if (!udta_type && !is_box_array) return GF_BAD_PARAM;

	if (!src || !strlen(src)) {
		return gf_isom_remove_user_data(dest, tracknum, udta_type, uuid);
	}

#ifndef GPAC_DISABLE_CORE_TOOLS
	if (!strnicmp(src, "base64", 6)) {
		src += 7;
		size = (u32) strlen(src);
		data = gf_malloc(sizeof(char) * size);
		size = gf_base64_decode((u8 *)src, size, data, size);
	} else
#endif
	{
		GF_Err e = gf_file_load_data(src, (u8 **) &data, &size);
		if (e) return e;
	}

	if (size && data) {
		if (is_box_array) {
			res = gf_isom_add_user_data_boxes(dest, tracknum, data, size);
		} else {
			res = gf_isom_add_user_data(dest, tracknum, udta_type, uuid, data, size);
		}
		gf_free(data);
	}
	return res;
}

#ifndef GPAC_DISABLE_MEDIA_IMPORT

extern u32 swf_flags;
extern Float swf_flatten_angle;
extern Bool keep_sys_tracks;
extern u32 fs_dump_flags;

void scene_coding_log(void *cbk, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char *fmt, va_list vlist);

void convert_file_info(char *inName, GF_ISOTrackID trackID)
{
	GF_Err e;
	u32 i;
	Bool found;
	GF_MediaImporter import;
	memset(&import, 0, sizeof(GF_MediaImporter));
	import.trackID = trackID;
	import.in_name = inName;
	import.flags = GF_IMPORT_PROBE_ONLY;
	import.print_stats_graph = fs_dump_flags;

	e = gf_media_import(&import);
	if (e) {
		fprintf(stderr, "Error probing file %s: %s\n", inName, gf_error_to_string(e));
		return;
	}
	if (trackID) {
		fprintf(stderr, "Import probing results for track %s#%d:\n", inName, trackID);
	} else {
		fprintf(stderr, "Import probing results for %s:\n", inName);
		if (!import.nb_tracks) {
			fprintf(stderr, "File has no selectable tracks\n");
			return;
		}
		fprintf(stderr, "File has %d tracks\n", import.nb_tracks);
	}
	if (import.probe_duration) {
		fprintf(stderr, "Duration: %g s\n", (Double) (import.probe_duration/1000.0));
	}
	found = 0;
	for (i=0; i<import.nb_tracks; i++) {
		if (trackID && (trackID != import.tk_info[i].track_num)) continue;
		if (!trackID) fprintf(stderr, "\tTrack %d type: ", import.tk_info[i].track_num);
		else fprintf(stderr, "Track type: ");

		switch (import.tk_info[i].stream_type) {
		case GF_STREAM_VISUAL:
			if (import.tk_info[i].media_subtype == GF_ISOM_MEDIA_AUXV)  fprintf(stderr, "Auxiliary Video");
			else if (import.tk_info[i].media_subtype == GF_ISOM_MEDIA_PICT)  fprintf(stderr, "Picture Sequence");
			else fprintf(stderr, "Video");
			if (import.tk_info[i].video_info.temporal_enhancement) fprintf(stderr, " Temporal Enhancement");
			break;
		case GF_STREAM_AUDIO:
			fprintf(stderr, "Audio");
			break;
		case GF_STREAM_TEXT:
			fprintf(stderr, "Text");
			break;
		case GF_STREAM_SCENE:
			fprintf(stderr, "Scene");
			break;
		case GF_STREAM_OD:
			fprintf(stderr, "OD");
			break;
		case GF_STREAM_METADATA:
			fprintf(stderr, "Metadata");
			break;
		default:
			fprintf(stderr, "Other (%s)", gf_4cc_to_str(import.tk_info[i].stream_type));
			break;
		}
		if (import.tk_info[i].codecid) fprintf(stderr, " Codec %s (ID %d)", gf_codecid_name(import.tk_info[i].codecid), import.tk_info[i].codecid);

		if (import.tk_info[i].lang) fprintf(stderr, " lang %s", gf_4cc_to_str(import.tk_info[i].lang));

		if (import.tk_info[i].mpeg4_es_id) fprintf(stderr, " MPEG-4 ESID %d", import.tk_info[i].mpeg4_es_id);

		if (import.tk_info[i].prog_num) {
			if (!import.nb_progs) {
				fprintf(stderr, " Program %d", import.tk_info[i].prog_num);
			} else {
				u32 j;
				for (j=0; j<import.nb_progs; j++) {
					if (import.tk_info[i].prog_num != import.pg_info[j].number) continue;
					fprintf(stderr, " Program %s", import.pg_info[j].name);
					break;
				}
			}
		}
		fprintf(stderr, "\n");

		if (import.tk_info[i].video_info.width && import.tk_info[i].video_info.height
		   ) {
			fprintf(stderr, "\tSize %dx%d", import.tk_info[i].video_info.width, import.tk_info[i].video_info.height);
			if (import.tk_info[i].video_info.FPS) fprintf(stderr, " @ %g FPS", import.tk_info[i].video_info.FPS);
			if (import.tk_info[i].stream_type==GF_STREAM_VISUAL) {
				if (import.tk_info[i].video_info.par) fprintf(stderr, " PAR: %d:%d", import.tk_info[i].video_info.par >> 16, import.tk_info[i].video_info.par & 0xFFFF);
			}

			fprintf(stderr, "\n");
		}
		else if ((import.tk_info[i].stream_type==GF_STREAM_AUDIO) && import.tk_info[i].audio_info.sample_rate) {
			fprintf(stderr, "\tSampleRate %d - %d channels\n", import.tk_info[i].audio_info.sample_rate, import.tk_info[i].audio_info.nb_channels);
		}

		if (trackID) {
			found = 1;
			break;
		}
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "For more details, use `gpac -i %s inspect[:deep][:analyze]`\n", gf_file_basename(inName));
	if (!found && trackID) fprintf(stderr, "Cannot find track %d in file\n", trackID);
}

static void set_chapter_track(GF_ISOFile *file, u32 track, u32 chapter_ref_trak)
{
	u64 ref_duration, chap_duration;
	Double scale;

	gf_isom_set_track_reference(file, chapter_ref_trak, GF_ISOM_REF_CHAP, gf_isom_get_track_id(file, track) );
	gf_isom_set_track_enabled(file, track, 0);

	ref_duration = gf_isom_get_media_duration(file, chapter_ref_trak);
	chap_duration = gf_isom_get_media_duration(file, track);
	scale = (Double) (s64) gf_isom_get_media_timescale(file, track);
	scale /= gf_isom_get_media_timescale(file, chapter_ref_trak);
	ref_duration = (u64) (ref_duration * scale);

	if (chap_duration < ref_duration) {
		chap_duration -= gf_isom_get_sample_duration(file, track, gf_isom_get_sample_count(file, track));
		chap_duration = ref_duration - chap_duration;
		gf_isom_set_last_sample_duration(file, track, (u32) chap_duration);
	}
}

GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample)
{
	u32 track_id, i, j, timescale, track, stype, profile, level, new_timescale, rescale, svc_mode, txt_flags, split_tile_mode, temporal_mode;
	s32 par_d, par_n, prog_id, delay, force_rate, moov_timescale;
	s32 tw, th, tx, ty, txtw, txth, txtx, txty;
	Bool do_audio, do_video, do_auxv,do_pict, do_all, disable, track_layout, text_layout, chap_ref, is_chap, is_chap_file, keep_handler, negative_cts_offset, rap_only, refs_only, force_par;
	u32 group, handler, rvc_predefined, check_track_for_svc, check_track_for_lhvc, check_track_for_hevc;
	const char *szLan;
	GF_Err e;
	u32 tmcd_track = 0;
	Bool keep_audelim = GF_FALSE;
	u32 print_stats_graph=fs_dump_flags;
	GF_MediaImporter import;
	char *ext, szName[1000], *handler_name, *rvc_config, *chapter_name;
	GF_List *kinds;
	GF_TextFlagsMode txt_mode = GF_ISOM_TEXT_FLAGS_OVERWRITE;
	u8 max_layer_id_plus_one, max_temporal_id_plus_one;
	u32 clap_wn, clap_wd, clap_hn, clap_hd, clap_hon, clap_hod, clap_von, clap_vod;
	Bool has_clap=GF_FALSE;
	Bool use_stz2=GF_FALSE;
	Bool has_mx=GF_FALSE;
	s32 mx[9];
	u32 bitdepth=0;
	u32 clr_type=0;
	u32 clr_prim;
	u32 clr_tranf;
	u32 clr_mx;
	u32 clr_full_range=GF_FALSE;
	Bool fmt_ok = GF_TRUE;
	u32 icc_size=0;
	u8 *icc_data = NULL;
	char *ext_start;
	u32 xps_inband=0;
	char *opt_src = NULL;
	char *opt_dst = NULL;
	char *fchain = NULL;

	clap_wn = clap_wd = clap_hn = clap_hd = clap_hon = clap_hod = clap_von = clap_vod = 0;

	rvc_predefined = 0;
	chapter_name = NULL;
	new_timescale = 1;
	moov_timescale = 0;
	rescale = 0;
	text_layout = 0;
	/*0: merge all
	  1: split base and all SVC in two tracks
	  2: split all base and SVC layers in dedicated tracks
	 */
	svc_mode = 0;

	memset(&import, 0, sizeof(GF_MediaImporter));

	strcpy(szName, inName);
#ifdef WIN32
	/*dirty hack for msys&mingw: when we use import options, the ':' separator used prevents msys from translating the path
	we do this for regular cases where the path starts with the drive letter. If the path start with anything else (/home , /opt, ...) we're screwed :( */
	if ( (szName[0]=='/') && (szName[2]=='/')) {
		szName[0] = szName[1];
		szName[1] = ':';
	}
#endif

	is_chap_file = 0;
	handler = 0;
	disable = 0;
	chap_ref = 0;
	is_chap = 0;
	kinds = gf_list_new();
	track_layout = 0;
	szLan = NULL;
	delay = 0;
	group = 0;
	stype = 0;
	profile = level = 0;
	negative_cts_offset = 0;
	split_tile_mode = 0;
	temporal_mode = 0;
	rap_only = 0;
	refs_only = 0;
	txt_flags = 0;
	max_layer_id_plus_one = max_temporal_id_plus_one = 0;
	force_rate = -1;

	tw = th = tx = ty = txtw = txth = txtx = txty = 0;
	par_d = par_n = -2;
	force_par = GF_FALSE;
	/*use ':' as separator, but beware DOS paths...*/
	ext = strchr(szName, ':');
	if (ext && ext[1]=='\\') ext = strchr(szName+2, ':');

	handler_name = NULL;
	rvc_config = NULL;
	while (ext) {
		char *ext2 = strchr(ext+1, ':');

		// if the colon is part of a file path/url we keep it
		if (ext2 && !strncmp(ext2, "://", 3) ) {
			ext2[0] = ':';
			ext2 = strchr(ext2+1, ':');
		}

		// keep windows drive: path, can be after a file://
		if (ext2 && ( !strncmp(ext2, ":\\", 2) || !strncmp(ext2, ":/", 2) ) ) {
			ext2[0] = ':';
			ext2 = strchr(ext2+1, ':');
		}
		if (ext2) ext2[0] = 0;

		/*all extensions for track-based importing*/
		if (!strnicmp(ext+1, "dur=", 4)) import.duration = (u32)( (atof(ext+5) * 1000) + 0.5 );
		else if (!strnicmp(ext+1, "lang=", 5)) {
			/* prevent leak if param is set twice */
			if (szLan)
				gf_free((char*) szLan);

			szLan = gf_strdup(ext+6);
		}
		else if (!strnicmp(ext+1, "delay=", 6)) delay = atoi(ext+7);
		else if (!strnicmp(ext+1, "par=", 4)) {
			if (!stricmp(ext + 5, "none")) {
				par_n = par_d = -1;
			} else if (!stricmp(ext + 5, "force")) {
				force_par = GF_TRUE;
			} else {
				if (ext2) ext2[0] = ':';
				if (ext2) ext2 = strchr(ext2+1, ':');
				if (ext2) ext2[0] = 0;
				sscanf(ext+5, "%d:%d", &par_n, &par_d);
			}
		}
		else if (!strnicmp(ext+1, "clap=", 5)) {
			if (!stricmp(ext+6, "none")) {
				has_clap=GF_TRUE;
			} else {
				if (sscanf(ext+6, "%d:%d:%d:%d:%d:%d:%d:%d", &clap_wn, &clap_wd, &clap_hn, &clap_hd, &clap_hon, &clap_hod, &clap_von, &clap_vod)==8) {
					has_clap=GF_TRUE;
				}
			}
		}
		else if (!strnicmp(ext+1, "mx=", 3)) {
			if (strstr(ext+4, "0x")) {
				if (sscanf(ext+4, "0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d", &mx[0], &mx[1], &mx[2], &mx[3], &mx[4], &mx[5], &mx[6], &mx[7], &mx[8])==9) {
					has_mx=GF_TRUE;
				}
			} else if (sscanf(ext+4, "%d:%d:%d:%d:%d:%d:%d:%d:%d", &mx[0], &mx[1], &mx[2], &mx[3], &mx[4], &mx[5], &mx[6], &mx[7], &mx[8])==9) {
				has_mx=GF_TRUE;
			}
		}
		else if (!strnicmp(ext+1, "name=", 5)) {
			handler_name = gf_strdup(ext+6);
		}
		else if (!strnicmp(ext+1, "ext=", 4)) {
			/*extensions begin with '.'*/
			if (*(ext+5) == '.')
				import.force_ext = gf_strdup(ext+5);
			else {
				import.force_ext = gf_calloc(1+strlen(ext+5)+1, 1);
				import.force_ext[0] = '.';
				strcat(import.force_ext+1, ext+5);
			}
		}
		else if (!strnicmp(ext+1, "hdlr=", 5)) handler = GF_4CC(ext[6], ext[7], ext[8], ext[9]);
		else if (!strnicmp(ext+1, "disable", 7)) disable = 1;
		else if (!strnicmp(ext+1, "group=", 6)) {
			group = atoi(ext+7);
			if (!group) group = gf_isom_get_next_alternate_group_id(dest);
		}
		else if (!strnicmp(ext+1, "fps=", 4)) {
			u32 ticks, dts_inc;
			if (!strcmp(ext+5, "auto")) {
				fprintf(stderr, "Warning, fps=auto option is deprecated\n");
			} else if ((sscanf(ext+5, "%u-%u", &ticks, &dts_inc) == 2) || (sscanf(ext+5, "%u/%u", &ticks, &dts_inc) == 2)) {
				if (!dts_inc) dts_inc=1;
				force_fps.num = ticks;
				force_fps.den = dts_inc;
			} else {
				force_fps.num = (u32) (atof(ext+5));
				force_fps.den = 1000;
			}
		}
		else if (!stricmp(ext+1, "rap")) rap_only = 1;
		else if (!stricmp(ext+1, "refs")) refs_only = 1;
		else if (!stricmp(ext+1, "trailing")) import_flags |= GF_IMPORT_KEEP_TRAILING;
		else if (!strnicmp(ext+1, "agg=", 4)) frames_per_sample = atoi(ext+5);
		else if (!stricmp(ext+1, "dref")) import_flags |= GF_IMPORT_USE_DATAREF;
		else if (!stricmp(ext+1, "keep_refs")) import_flags |= GF_IMPORT_KEEP_REFS;
		else if (!stricmp(ext+1, "nodrop")) import_flags |= GF_IMPORT_NO_FRAME_DROP;
		else if (!stricmp(ext+1, "packed")) import_flags |= GF_IMPORT_FORCE_PACKED;
		else if (!stricmp(ext+1, "sbr")) import_flags |= GF_IMPORT_SBR_IMPLICIT;
		else if (!stricmp(ext+1, "sbrx")) import_flags |= GF_IMPORT_SBR_EXPLICIT;
		else if (!stricmp(ext+1, "ovsbr")) import_flags |= GF_IMPORT_OVSBR;
		else if (!stricmp(ext+1, "ps")) import_flags |= GF_IMPORT_PS_IMPLICIT;
		else if (!stricmp(ext+1, "psx")) import_flags |= GF_IMPORT_PS_EXPLICIT;
		else if (!stricmp(ext+1, "mpeg4")) import_flags |= GF_IMPORT_FORCE_MPEG4;
		else if (!stricmp(ext+1, "nosei")) import_flags |= GF_IMPORT_NO_SEI;
		else if (!stricmp(ext+1, "svc") || !stricmp(ext+1, "lhvc") ) import_flags |= GF_IMPORT_SVC_EXPLICIT;
		else if (!stricmp(ext+1, "nosvc") || !stricmp(ext+1, "nolhvc")) import_flags |= GF_IMPORT_SVC_NONE;

		/*split SVC layers*/
		else if (!strnicmp(ext+1, "svcmode=", 8) || !strnicmp(ext+1, "lhvcmode=", 9)) {
			char *mode = ext+9;
			if (mode[0]=='=') mode = ext+10;

			if (!stricmp(mode, "splitnox"))
				svc_mode = 3;
			else if (!stricmp(mode, "splitnoxib"))
				svc_mode = 4;
			else if (!stricmp(mode, "splitall") || !stricmp(mode, "split"))
				svc_mode = 2;
			else if (!stricmp(mode, "splitbase"))
				svc_mode = 1;
			else if (!stricmp(mode, "merged"))
				svc_mode = 0;
		}
		/*split SHVC temporal sublayers*/
		else if (!strnicmp(ext+1, "temporal=", 9)) {
			char *mode = ext+10;
			if (!stricmp(mode, "split"))
				temporal_mode = 2;
			else if (!stricmp(mode, "splitnox"))
				temporal_mode = 3;
			else if (!stricmp(mode, "splitbase"))
				temporal_mode = 1;
			else {
				fprintf(stderr, "Unrecognized temporal mode %s, ignoring\n", mode);
				temporal_mode = 0;
			}
		}
		else if (!stricmp(ext+1, "subsamples")) import_flags |= GF_IMPORT_SET_SUBSAMPLES;
		else if (!stricmp(ext+1, "deps")) import_flags |= GF_IMPORT_SAMPLE_DEPS;
		else if (!stricmp(ext+1, "ccst")) import_flags |= GF_IMPORT_USE_CCST;
		else if (!stricmp(ext+1, "alpha")) import.is_alpha = GF_TRUE;
		else if (!stricmp(ext+1, "forcesync")) import_flags |= GF_IMPORT_FORCE_SYNC;
		else if (!stricmp(ext+1, "xps_inband")) xps_inband = 1;
		else if (!stricmp(ext+1, "xps_inbandx")) xps_inband = 2;
		else if (!stricmp(ext+1, "au_delim")) keep_audelim = GF_TRUE;
		else if (!strnicmp(ext+1, "max_lid=", 8) || !strnicmp(ext+1, "max_tid=", 8)) {
			s32 val = atoi(ext+9);
			if (val < 0) {
				fprintf(stderr, "Warning: request max layer/temporal id is negative - ignoring\n");
			} else {
				if (!strnicmp(ext+1, "max_lid=", 8))
					max_layer_id_plus_one = 1 + (u8) val;
				else
					max_temporal_id_plus_one = 1 + (u8) val;
			}
		}
		else if (!stricmp(ext+1, "tiles")) split_tile_mode = 2;
		else if (!stricmp(ext+1, "tiles_rle")) split_tile_mode = 3;
		else if (!stricmp(ext+1, "split_tiles")) split_tile_mode = 1;

		/*force all composition offsets to be positive*/
		else if (!strnicmp(ext+1, "negctts", 7)) negative_cts_offset = 1;
		else if (!strnicmp(ext+1, "stype=", 6)) {
			stype = GF_4CC(ext[7], ext[8], ext[9], ext[10]);
		}
		else if (!stricmp(ext+1, "chap")) is_chap = 1;
		else if (!strnicmp(ext+1, "chapter=", 8)) {
			chapter_name = gf_strdup(ext+9);
		}
		else if (!strnicmp(ext+1, "chapfile=", 9)) {
			chapter_name = gf_strdup(ext+10);
			is_chap_file=1;
		}
		else if (!strnicmp(ext+1, "layout=", 7)) {
			if ( sscanf(ext+8, "%dx%dx%dx%d", &tw, &th, &tx, &ty)==4) {
				track_layout = 1;
			} else if ( sscanf(ext+8, "%dx%d", &tw, &th)==2) {
				track_layout = 1;
				tx = ty = 0;
			}
		}
		else if (!strnicmp(ext+1, "rescale=", 8)) {
			rescale = atoi(ext+9);
		}
		else if (!strnicmp(ext+1, "timescale=", 10)) {
			new_timescale = atoi(ext+11);
		}
		else if (!strnicmp(ext+1, "moovts=", 7)) {
			moov_timescale = atoi(ext+8);
		}

		else if (!stricmp(ext+1, "noedit")) import_flags |= GF_IMPORT_NO_EDIT_LIST;


		else if (!strnicmp(ext+1, "rvc=", 4)) {
			if (sscanf(ext+5, "%d", &rvc_predefined) != 1) {
				rvc_config = gf_strdup(ext+5);
			}
		}
		else if (!strnicmp(ext+1, "fmt=", 4)) import.streamFormat = gf_strdup(ext+5);
		else if (!strnicmp(ext+1, "profile=", 8)) profile = atoi(ext+9);
		else if (!strnicmp(ext+1, "level=", 6)) level = atoi(ext+7);
		else if (!strnicmp(ext+1, "novpsext", 8)) import_flags |= GF_IMPORT_NO_VPS_EXTENSIONS;
		else if (!strnicmp(ext+1, "keepav1t", 8)) import_flags |= GF_IMPORT_KEEP_AV1_TEMPORAL_OBU;

		else if (!strnicmp(ext+1, "font=", 5)) import.fontName = gf_strdup(ext+6);
		else if (!strnicmp(ext+1, "size=", 5)) import.fontSize = atoi(ext+6);
		else if (!strnicmp(ext+1, "text_layout=", 12)) {
			if ( sscanf(ext+13, "%dx%dx%dx%d", &txtw, &txth, &txtx, &txty)==4) {
				text_layout = 1;
			} else if ( sscanf(ext+8, "%dx%d", &txtw, &txth)==2) {
				track_layout = 1;
				txtx = txty = 0;
			}
		}

#ifndef GPAC_DISABLE_SWF_IMPORT
		else if (!stricmp(ext+1, "swf-global")) import.swf_flags |= GF_SM_SWF_STATIC_DICT;
		else if (!stricmp(ext+1, "swf-no-ctrl")) import.swf_flags &= ~GF_SM_SWF_SPLIT_TIMELINE;
		else if (!stricmp(ext+1, "swf-no-text")) import.swf_flags |= GF_SM_SWF_NO_TEXT;
		else if (!stricmp(ext+1, "swf-no-font")) import.swf_flags |= GF_SM_SWF_NO_FONT;
		else if (!stricmp(ext+1, "swf-no-line")) import.swf_flags |= GF_SM_SWF_NO_LINE;
		else if (!stricmp(ext+1, "swf-no-grad")) import.swf_flags |= GF_SM_SWF_NO_GRADIENT;
		else if (!stricmp(ext+1, "swf-quad")) import.swf_flags |= GF_SM_SWF_QUAD_CURVE;
		else if (!stricmp(ext+1, "swf-xlp")) import.swf_flags |= GF_SM_SWF_SCALABLE_LINE;
		else if (!stricmp(ext+1, "swf-ic2d")) import.swf_flags |= GF_SM_SWF_USE_IC2D;
		else if (!stricmp(ext+1, "swf-same-app")) import.swf_flags |= GF_SM_SWF_REUSE_APPEARANCE;
		else if (!strnicmp(ext+1, "swf-flatten=", 12)) import.swf_flatten_angle = (Float) atof(ext+13);
#endif

		else if (!strnicmp(ext+1, "kind=", 5)) {
			char *kind_scheme, *kind_value;
			char *kind_data = ext+6;
			char *sep = strchr(kind_data, '=');
			if (sep) {
				*sep = 0;
			}
			kind_scheme = gf_strdup(kind_data);
			if (sep) {
				*sep = '=';
				kind_value = gf_strdup(sep+1);
			} else {
				kind_value = NULL;
			}
			gf_list_add(kinds, kind_scheme);
			gf_list_add(kinds, kind_value);
		}
		else if (!strnicmp(ext+1, "txtflags", 8)) {
			if (!strnicmp(ext+1, "txtflags=", 9)) {
				sscanf(ext+10, "%x", &txt_flags);
			}
			else if (!strnicmp(ext+1, "txtflags+=", 10)) {
				sscanf(ext+11, "%x", &txt_flags);
				txt_mode = GF_ISOM_TEXT_FLAGS_TOGGLE;
			}
			else if (!strnicmp(ext+1, "txtflags-=", 10)) {
				sscanf(ext+11, "%x", &txt_flags);
				txt_mode = GF_ISOM_TEXT_FLAGS_UNTOGGLE;
			}
		}
		else if (!strnicmp(ext+1, "rate=", 5)) {
			force_rate = atoi(ext+6);
		}
		else if (!stricmp(ext+1, "fstat"))
			print_stats_graph |= 1;
		else if (!stricmp(ext+1, "fgraph"))
			print_stats_graph |= 2;
		else if (!strncmp(ext+1, "sopt", 4) || !strncmp(ext+1, "dopt", 4) || !strncmp(ext+1, "@@", 2)) {
			if (ext2) ext2[0] = ':';
			opt_src = strstr(ext, ":sopt:");
			opt_dst = strstr(ext, ":dopt:");
			fchain = strstr(ext, ":@@");
			if (opt_src) opt_src[0] = 0;
			if (opt_dst) opt_dst[0] = 0;
			if (fchain) fchain[0] = 0;

			if (opt_src) import.filter_src_opts = opt_src+6;
			if (opt_dst) import.filter_dst_opts = opt_dst+6;
			if (fchain) import.filter_chain = fchain+3;

			ext = NULL;
			break;
		}

		else if (!strnicmp(ext+1, "asemode=", 8)){
			char *mode = ext+9;
			if (!stricmp(mode, "v0-bs"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_BS;
			else if (!stricmp(mode, "v0-2"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2;
			else if (!stricmp(mode, "v1"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG;
			else if (!stricmp(mode, "v1-qt"))
				import.asemode = GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF;
			else
				fprintf(stderr, "Unrecognized audio sample entry mode %s, ignoring\n", mode);
		}

		else if (!strnicmp(ext+1, "audio_roll=", 11)) {
			import.audio_roll_change = GF_TRUE;
			import.audio_roll = atoi(ext+12);
		}
		else if (!strcmp(ext+1, "stz2")) {
			use_stz2 = GF_TRUE;
		} else if (!strnicmp(ext+1, "bitdepth=", 9)) {
			bitdepth=atoi(ext+10);
		}
		else if (!strnicmp(ext+1, "colr=", 5)) {
			char *cval = ext+6;
			if (strlen(cval)<6) {
				fmt_ok = GF_FALSE;
			} else {
				clr_type = GF_4CC(cval[0],cval[1],cval[2],cval[3]);
				cval+=4;
				if (cval[0] != ',') {
					fmt_ok = GF_FALSE;
				}
				else if (clr_type==GF_ISOM_SUBTYPE_NCLX) {
					if (sscanf(cval+1, "%d,%d,%d,%d", &clr_prim, &clr_tranf, &clr_mx, &clr_full_range) != 4)
						fmt_ok=GF_FALSE;
				}
				else if (clr_type==GF_ISOM_SUBTYPE_NCLC) {
					if (sscanf(cval+1, "%d,%d,%d", &clr_prim, &clr_tranf, &clr_mx) != 3)
						fmt_ok=GF_FALSE;
				}
				else if ((clr_type==GF_ISOM_SUBTYPE_RICC) || (clr_type==GF_ISOM_SUBTYPE_PROF)) {
					FILE *f = gf_fopen(cval+1, "rb");
					if (!f) {
						fprintf(stderr, "Failed to open file %s\n", cval+1);
						fmt_ok = GF_FALSE;
					} else {
						gf_fseek(f, 0, SEEK_END);
						icc_size = (u32) gf_ftell(f);
						icc_data = gf_malloc(sizeof(char)*icc_size);
						gf_fseek(f, 0, SEEK_SET);
						icc_size = (u32) gf_fread(icc_data, 1, icc_size, f);
						gf_fclose(f);
					}
				} else {
					fprintf(stderr, "unrecognized profile %s\n", gf_4cc_to_str(clr_type) );
					fmt_ok = GF_FALSE;
				}
			}
			if (!fmt_ok) {
				fprintf(stderr, "Bad format for clr option, check help\n");
				e = GF_BAD_PARAM;
				goto exit;
			}
		}

		/*unrecognized, assume name has colon in it*/
		else {
			fprintf(stderr, "Unrecognized import option %s, ignoring\n", ext+1);
			ext = ext2;
			continue;
		}

		if (ext2) ext2[0] = ':';

		ext[0] = 0;

		/* restart from where we stopped
		 * if we didn't stop (ext2 null) then the end has been reached
		 * so we can stop the whole thing */
		ext = ext2;
	}

	/*check duration import (old syntax)*/
	ext = strrchr(szName, '%');
	if (ext) {
		import.duration = (u32) (atof(ext+1) * 1000);
		ext[0] = 0;
	}

	/*select switches for av containers import*/
	do_audio = do_video = do_auxv = do_pict = 0;
	track_id = prog_id = 0;
	do_all = 1;

	ext_start = gf_file_ext_start(szName);
	ext = strrchr(ext_start ? ext_start : szName, '#');
	if (ext) ext[0] = 0;

	keep_handler = gf_isom_probe_file(szName);

	import.in_name = szName;
	import.flags = GF_IMPORT_PROBE_ONLY;
	e = gf_media_import(&import);
	if (e) goto exit;
	
	if (ext) {
		ext++;
		char *sep = strchr(ext, ':');
		if (sep) sep[0] = 0;

		if (!strnicmp(ext, "audio", 5)) do_audio = 1;
		else if (!strnicmp(ext, "video", 5)) do_video = 1;
        else if (!strnicmp(ext, "auxv", 4)) do_auxv = 1;
        else if (!strnicmp(ext, "pict", 4)) do_pict = 1;
		else if (!strnicmp(ext, "trackID=", 8)) track_id = atoi(&ext[8]);
		else if (!strnicmp(ext, "PID=", 4)) track_id = atoi(&ext[4]);
		else if (!strnicmp(ext, "program=", 8)) {
			for (i=0; i<import.nb_progs; i++) {
				if (!stricmp(import.pg_info[i].name, ext+8)) {
					prog_id = import.pg_info[i].number;
					do_all = 0;
					break;
				}
			}
		}
		else if (!strnicmp(ext, "prog_id=", 8)) {
			prog_id = atoi(ext+8);
			do_all = 0;
		}
		else track_id = atoi(ext);

		if (sep) sep[0] = ':';
	}
	if (do_audio || do_video || do_auxv || do_pict || track_id) do_all = 0;

	if (track_layout || is_chap) {
		u32 w, h, sw, sh, fw, fh, i;
		w = h = sw = sh = fw = fh = 0;
		chap_ref = 0;
		for (i=0; i<gf_isom_get_track_count(dest); i++) {
			switch (gf_isom_get_media_type(dest, i+1)) {
			case GF_ISOM_MEDIA_SCENE:
			case GF_ISOM_MEDIA_VISUAL:
            case GF_ISOM_MEDIA_AUXV:
            case GF_ISOM_MEDIA_PICT:
				if (!chap_ref && gf_isom_is_track_enabled(dest, i+1) ) chap_ref = i+1;

				gf_isom_get_visual_info(dest, i+1, 1, &sw, &sh);
				gf_isom_get_track_layout_info(dest, i+1, &fw, &fh, NULL, NULL, NULL);
				if (w<sw) w = sw;
				if (w<fw) w = fw;
				if (h<sh) h = sh;
				if (h<fh) h = fh;
				break;
			case GF_ISOM_MEDIA_AUDIO:
				if (!chap_ref && gf_isom_is_track_enabled(dest, i+1) ) chap_ref = i+1;
				break;
			}
		}
		if (track_layout) {
			if (!tw) tw = w;
			if (!th) th = h;
			if (ty==-1) ty = (h>(u32)th) ? h-th : 0;
			import.text_width = tw;
			import.text_height = th;
		}
		if (is_chap && chap_ref) import_flags |= GF_IMPORT_NO_TEXT_FLUSH;
	}
	if (text_layout && txtw && txth) {
		import.text_track_width = import.text_width ? import.text_width : txtw;
		import.text_track_height = import.text_height ? import.text_height : txth;
		import.text_width = txtw;
		import.text_height = txth;
		import.text_x = txtx;
		import.text_y = txty;
	}

	check_track_for_svc = check_track_for_lhvc = check_track_for_hevc = 0;

	import.dest = dest;
	import.video_fps = force_fps;
	import.frames_per_sample = frames_per_sample;
	import.flags = import_flags;
	import.keep_audelim = keep_audelim;
	import.print_stats_graph = print_stats_graph;
	import.xps_inband = xps_inband;

	if (!import.nb_tracks) {
		u32 count, o_count;
		o_count = gf_isom_get_track_count(import.dest);
		e = gf_media_import(&import);
		if (e) return e;
		count = gf_isom_get_track_count(import.dest);

		if (moov_timescale) {
			if (moov_timescale<0) moov_timescale = gf_isom_get_media_timescale(import.dest, o_count+1);
			gf_isom_set_timescale(import.dest, moov_timescale);
			moov_timescale = 0;
		}

		timescale = gf_isom_get_timescale(dest);
		for (i=o_count; i<count; i++) {
			if (szLan) gf_isom_set_media_language(import.dest, i+1, (char *) szLan);
			if (delay) {
				u64 tk_dur;
				gf_isom_remove_edit_segments(import.dest, i+1);
				tk_dur = gf_isom_get_track_duration(import.dest, i+1);
				if (delay>0) {
					gf_isom_append_edit_segment(import.dest, i+1, (timescale*delay)/1000, 0, GF_ISOM_EDIT_EMPTY);
					gf_isom_append_edit_segment(import.dest, i+1, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
				} else if (delay<0) {
					u64 to_skip = (timescale*(-delay))/1000;
					if (to_skip<tk_dur) {
						//u64 seg_dur = (-delay)*gf_isom_get_media_timescale(import.dest, i+1) / 1000;
						gf_isom_append_edit_segment(import.dest, i+1, tk_dur-to_skip, to_skip, GF_ISOM_EDIT_NORMAL);
					} else {
						fprintf(stderr, "Warning: request negative delay longer than track duration - ignoring\n");
					}
				}
			}
			if (((par_n>=0) && (par_d>=0)) || force_par) {
				e = gf_media_change_par(import.dest, i+1, par_n, par_d, force_par);
			}
			if (has_clap) {
				e = gf_isom_set_clean_aperture(import.dest, i+1, 1, clap_wn, clap_wd, clap_hn, clap_hd, clap_hon, clap_hod, clap_von, clap_vod);
			}
			if (use_stz2) {
				e = gf_isom_use_compact_size(import.dest, i+1, 1);
			}
			if (has_mx) {
				e = gf_isom_set_track_matrix(import.dest, i+1, mx);
			}

			if (rap_only || refs_only) {
				e = gf_media_remove_non_rap(import.dest, i+1, refs_only);
			}

			if (handler_name) gf_isom_set_handler_name(import.dest, i+1, handler_name);
			else if (!keep_handler) {
				char szHName[1024];
				const char *fName = gf_url_get_resource_name((const  char *)inName);
				fName = strchr(fName, '.');
				if (fName) fName += 1;
				else fName = "?";

				sprintf(szHName, "*%s@GPAC%s", fName, gf_gpac_version() );
				gf_isom_set_handler_name(import.dest, i+1, szHName);
			}
			if (handler) gf_isom_set_media_type(import.dest, i+1, handler);
			if (disable) gf_isom_set_track_enabled(import.dest, i+1, 0);

			if (group) {
				gf_isom_set_alternate_group_id(import.dest, i+1, group);
			}
			if (track_layout) {
				gf_isom_set_track_layout_info(import.dest, i+1, tw<<16, th<<16, tx<<16, ty<<16, 0);
			}
			if (stype)
				gf_isom_set_media_subtype(import.dest, i+1, 1, stype);

			if (is_chap && chap_ref) {
				set_chapter_track(import.dest, i+1, chap_ref);
			}
			if (bitdepth) {
				gf_isom_set_visual_bit_depth(import.dest, i+1, 1, bitdepth);
			}
			if (clr_type) {
				gf_isom_set_visual_color_info(import.dest, i+1, 1, clr_type, clr_prim, clr_tranf, clr_mx, clr_full_range, icc_data, icc_size);
			}

			for (j = 0; j < gf_list_count(kinds); j+=2) {
				char *kind_scheme = (char *)gf_list_get(kinds, j);
				char *kind_value = (char *)gf_list_get(kinds, j+1);
				gf_isom_add_track_kind(import.dest, i+1, kind_scheme, kind_value);
			}

			if (profile || level)
				gf_media_change_pl(import.dest, i+1, profile, level);

			switch (gf_isom_get_media_subtype(import.dest, i+1, 1)) {
			case GF_ISOM_BOX_TYPE_MP4S:
				keep_sys_tracks = 1;
				break;
			case GF_QT_BOX_TYPE_TMCD:
				tmcd_track = i+1;
				break;
			}

			gf_isom_set_composition_offset_mode(import.dest, i+1, negative_cts_offset);

			if (gf_isom_get_avc_svc_type(import.dest, i+1, 1)>=GF_ISOM_AVCTYPE_AVC_SVC)
				check_track_for_svc = i+1;

			switch (gf_isom_get_hevc_lhvc_type(import.dest, i+1, 1)) {
			case GF_ISOM_HEVCTYPE_HEVC_LHVC:
			case GF_ISOM_HEVCTYPE_LHVC_ONLY:
				check_track_for_lhvc = i+1;
				break;
			case GF_ISOM_HEVCTYPE_HEVC_ONLY:
				check_track_for_hevc=1;
				break;
			}

			if (txt_flags) {
				gf_isom_text_set_display_flags(import.dest, i+1, 0, txt_flags, txt_mode);
			}

			if (force_rate>=0) {
				gf_isom_update_bitrate(import.dest, i+1, 1, force_rate, force_rate, 0);
			}

			if (split_tile_mode) {
				switch (gf_isom_get_media_subtype(import.dest, i+1, 1)) {
				case GF_ISOM_SUBTYPE_HVC1:
				case GF_ISOM_SUBTYPE_HEV1:
				case GF_ISOM_SUBTYPE_HVC2:
				case GF_ISOM_SUBTYPE_HEV2:
					break;
				default:
					split_tile_mode = 0;
					break;
				}
			}
		}
	} else {
		if (do_all)
			import.flags |= GF_IMPORT_KEEP_REFS;

		for (i=0; i<import.nb_tracks; i++) {
			import.trackID = import.tk_info[i].track_num;
			if (prog_id) {
				if (import.tk_info[i].prog_num!=prog_id) continue;
				e = gf_media_import(&import);
			}
			else if (do_all) e = gf_media_import(&import);
			else if (track_id && (track_id==import.trackID)) {
				track_id = 0;
				e = gf_media_import(&import);
			}
			else if (do_audio && (import.tk_info[i].stream_type==GF_STREAM_AUDIO)) {
				do_audio = 0;
				e = gf_media_import(&import);
			}
			else if (do_video && (import.tk_info[i].stream_type==GF_STREAM_VISUAL)) {
				do_video = 0;
				e = gf_media_import(&import);
			}
            else if (do_auxv && (import.tk_info[i].media_subtype==GF_ISOM_MEDIA_AUXV)) {
                do_auxv = 0;
                e = gf_media_import(&import);
            }
            else if (do_pict && (import.tk_info[i].media_subtype==GF_ISOM_MEDIA_PICT)) {
                do_pict = 0;
                e = gf_media_import(&import);
            }
			else continue;
			if (e) goto exit;

			track = gf_isom_get_track_by_id(import.dest, import.final_trackID);

			if (moov_timescale) {
				if (moov_timescale<0) moov_timescale = gf_isom_get_media_timescale(import.dest, track);
				gf_isom_set_timescale(import.dest, moov_timescale);
				moov_timescale = 0;
			}

			timescale = gf_isom_get_timescale(dest);
			if (szLan) gf_isom_set_media_language(import.dest, track, (char *) szLan);
			if (disable) gf_isom_set_track_enabled(import.dest, track, 0);

			if (delay) {
				u64 tk_dur;
				gf_isom_remove_edit_segments(import.dest, track);
				tk_dur = gf_isom_get_track_duration(import.dest, track);
				if (delay>0) {
					gf_isom_append_edit_segment(import.dest, track, (timescale*delay)/1000, 0, GF_ISOM_EDIT_EMPTY);
					gf_isom_append_edit_segment(import.dest, track, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
				} else {
					u64 to_skip = (timescale*(-delay))/1000;
					if (to_skip<tk_dur) {
						u64 media_time = (-delay)*gf_isom_get_media_timescale(import.dest, track) / 1000;
						gf_isom_append_edit_segment(import.dest, track, tk_dur-to_skip, media_time, GF_ISOM_EDIT_NORMAL);
					} else {
						fprintf(stderr, "Warning: request negative delay longer than track duration - ignoring\n");
					}
				}
			}
			if (gf_isom_is_video_subtype(import.tk_info[i].stream_type)) {
				if ((par_n>=-1) && (par_d>=-1)) {
					e = gf_media_change_par(import.dest, track, par_n, par_d, force_par);
				}
				if (has_clap) {
					e = gf_isom_set_clean_aperture(import.dest, track, 1, clap_wn, clap_wd, clap_hn, clap_hd, clap_hon, clap_hod, clap_von, clap_vod);
				}
			}
			if (has_mx) {
				e = gf_isom_set_track_matrix(import.dest, track, mx);
			}
			if (use_stz2) {
				e = gf_isom_use_compact_size(import.dest, track, 1);
			}
			if (gf_isom_get_media_subtype(import.dest, track, 1) == GF_QT_BOX_TYPE_TMCD) {
				tmcd_track = track;
			}
			if (rap_only || refs_only) {
				e = gf_media_remove_non_rap(import.dest, track, refs_only);
			}
			if (handler_name) gf_isom_set_handler_name(import.dest, track, handler_name);
			else if (!keep_handler) {
				char szHName[1024];
				const char *fName = gf_url_get_resource_name((const  char *)inName);
				fName = strchr(fName, '.');
				if (fName) fName += 1;
				else fName = "?";

				sprintf(szHName, "%s@GPAC%s", fName, gf_gpac_version());
				gf_isom_set_handler_name(import.dest, track, szHName);
			}
			if (handler) gf_isom_set_media_type(import.dest, track, handler);

			if (group) {
				gf_isom_set_alternate_group_id(import.dest, track, group);
			}

			if (track_layout) {
				gf_isom_set_track_layout_info(import.dest, track, tw<<16, th<<16, tx<<16, ty<<16, 0);
			}
			if (stype)
				gf_isom_set_media_subtype(import.dest, track, 1, stype);

			if (is_chap && chap_ref) {
				set_chapter_track(import.dest, track, chap_ref);
			}

			if (bitdepth) {
				gf_isom_set_visual_bit_depth(import.dest, track, 1, bitdepth);
			}
			if (clr_type) {
				gf_isom_set_visual_color_info(import.dest, track, 1, clr_type, clr_prim, clr_tranf, clr_mx, clr_full_range, icc_data, icc_size);
			}

			for (j = 0; j < gf_list_count(kinds); j+=2) {
				char *kind_scheme = (char *)gf_list_get(kinds, j);
				char *kind_value = (char *)gf_list_get(kinds, j+1);
				gf_isom_add_track_kind(import.dest, i+1, kind_scheme, kind_value);
			}

			if (profile || level)
				gf_media_change_pl(import.dest, track, profile, level);

			if (gf_isom_get_mpeg4_subtype(import.dest, track, 1))
				keep_sys_tracks = 1;

			if (new_timescale>1) {
				gf_isom_set_media_timescale(import.dest, track, new_timescale, 0);
			}

			if (rescale>1) {
				switch (gf_isom_get_media_type(import.dest, track)) {
				case GF_ISOM_MEDIA_AUDIO:
					fprintf(stderr, "Cannot force media timescale for audio media types - ignoring\n");
					break;
				default:
					gf_isom_set_media_timescale(import.dest, track, rescale, 1);
					break;
				}
			}

			if (rvc_config) {
				u8 *data;
				u32 size;
				e = gf_file_load_data(rvc_config, (u8 **) &data, &size);
				if (e) {
					fprintf(stderr, "Error: failed to load rvc config from file: %s\n", gf_error_to_string(e) );
				} else {
#ifdef GPAC_DISABLE_ZLIB
					fprintf(stderr, "Error: no zlib support - RVC not available\n");
					e = GF_NOT_SUPPORTED;
					gf_free(data);
					goto exit;
#else
					gf_gz_compress_payload(&data, size, &size);
#endif
					gf_isom_set_rvc_config(import.dest, track, 1, 0, "application/rvc-config+xml+gz", data, size);
					gf_free(data);
				}
			} else if (rvc_predefined>0) {
				gf_isom_set_rvc_config(import.dest, track, 1, rvc_predefined, NULL, NULL, 0);
			}

			gf_isom_set_composition_offset_mode(import.dest, track, negative_cts_offset);

			if (gf_isom_get_avc_svc_type(import.dest, track, 1)>=GF_ISOM_AVCTYPE_AVC_SVC)
				check_track_for_svc = track;

			switch (gf_isom_get_hevc_lhvc_type(import.dest, track, 1)) {
			case GF_ISOM_HEVCTYPE_HEVC_LHVC:
			case GF_ISOM_HEVCTYPE_LHVC_ONLY:
				check_track_for_lhvc = i+1;
				break;
			case GF_ISOM_HEVCTYPE_HEVC_ONLY:
				check_track_for_hevc=1;
				break;
			}

			if (txt_flags) {
				gf_isom_text_set_display_flags(import.dest, track, 0, txt_flags, txt_mode);
			}
			if (force_rate>=0) {
				gf_isom_update_bitrate(import.dest, i+1, 1, force_rate, force_rate, 0);
			}

			if (split_tile_mode) {
				switch (gf_isom_get_media_subtype(import.dest, track, 1)) {
				case GF_ISOM_SUBTYPE_HVC1:
				case GF_ISOM_SUBTYPE_HEV1:
				case GF_ISOM_SUBTYPE_HVC2:
				case GF_ISOM_SUBTYPE_HEV2:
					break;
				default:
					split_tile_mode = 0;
					break;
				}
			}
		}
		if (track_id) fprintf(stderr, "WARNING: Track ID %d not found in file\n", track_id);
		else if (do_video) fprintf(stderr, "WARNING: Video track not found\n");
        else if (do_auxv) fprintf(stderr, "WARNING: Auxiliary Video track not found\n");
        else if (do_pict) fprintf(stderr, "WARNING: Picture sequence track not found\n");
		else if (do_audio) fprintf(stderr, "WARNING: Audio track not found\n");
	}

	if (chapter_name) {
		if (is_chap_file) {
			GF_Fraction a_fps = {0,0};
			e = gf_media_import_chapters(import.dest, chapter_name, a_fps);
		} else {
			e = gf_isom_add_chapter(import.dest, 0, 0, chapter_name);
		}
	}

	if (tmcd_track) {
		u32 tmcd_id = gf_isom_get_track_id(import.dest, tmcd_track);
		for (i=0; i < gf_isom_get_track_count(import.dest); i++) {
			switch (gf_isom_get_media_type(import.dest, i+1)) {
			case GF_ISOM_MEDIA_VISUAL:
			case GF_ISOM_MEDIA_AUXV:
			case GF_ISOM_MEDIA_PICT:
				break;
			default:
				continue;
			}
			gf_isom_set_track_reference(import.dest, i+1, GF_ISOM_REF_TMCD, tmcd_id);
		}
	}

	/*force to rewrite all dependencies*/
	for (i = 1; i <= gf_isom_get_track_count(import.dest); i++)
	{
		e = gf_isom_rewrite_track_dependencies(import.dest, i);
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Warning: track ID %d has references to a track not imported\n", gf_isom_get_track_id(import.dest, i) ));
			e = GF_OK;
		}
	}

	if (max_layer_id_plus_one || max_temporal_id_plus_one) {
		for (i = 1; i <= gf_isom_get_track_count(import.dest); i++)
		{
			e = gf_media_filter_hevc(import.dest, i, max_temporal_id_plus_one, max_layer_id_plus_one);
			if (e) {
				fprintf(stderr, "Warning: track ID %d: error while filtering LHVC layers\n", gf_isom_get_track_id(import.dest, i));
				e = GF_OK;
			}
		}
	}

	if (check_track_for_svc) {
		if (svc_mode) {
			e = gf_media_split_svc(import.dest, check_track_for_svc, (svc_mode==2) ? 1 : 0);
			if (e) goto exit;
		} else {
			e = gf_media_merge_svc(import.dest, check_track_for_svc, 1);
			if (e) goto exit;
		}
	}
#ifndef GPAC_DISABLE_HEVC
	if (check_track_for_lhvc) {
		if (svc_mode) {
			GF_LHVCExtractoreMode xmode = GF_LHVC_EXTRACTORS_ON;
			if (svc_mode==3) xmode = GF_LHVC_EXTRACTORS_OFF;
			else if (svc_mode==4) xmode = GF_LHVC_EXTRACTORS_OFF_FORCE_INBAND;
			e = gf_media_split_lhvc(import.dest, check_track_for_lhvc, GF_FALSE, (svc_mode==1) ? 0 : 1, xmode );
			if (e) goto exit;
		} else {
			//TODO - merge, temporal sublayers
		}
	}
	if (check_track_for_hevc) {
		if (split_tile_mode) {
			e = gf_media_split_hevc_tiles(import.dest, split_tile_mode - 1);
			if (e) goto exit;
		}
		if (temporal_mode) {
			GF_LHVCExtractoreMode xmode = (temporal_mode==3) ? GF_LHVC_EXTRACTORS_OFF : GF_LHVC_EXTRACTORS_ON;
			e = gf_media_split_lhvc(import.dest, check_track_for_hevc, GF_TRUE, (temporal_mode==1) ? GF_FALSE : GF_TRUE, xmode );
			if (e) goto exit;
		}
	}

#endif /*GPAC_DISABLE_HEVC*/

exit:
	while (gf_list_count(kinds)) {
		char *kind = (char *)gf_list_get(kinds, 0);
		gf_list_rem(kinds, 0);
		if (kind) gf_free(kind);
	}
	if (opt_src) opt_src[0] = ':';
	if (opt_dst) opt_dst[0] = ':';
	if (fchain) fchain[0] = ':';

	gf_list_del(kinds);
	if (handler_name) gf_free(handler_name);
	if (chapter_name ) gf_free(chapter_name);
	if (import.fontName) gf_free(import.fontName);
	if (import.streamFormat) gf_free(import.streamFormat);
	if (import.force_ext) gf_free(import.force_ext);
	if (rvc_config) gf_free(rvc_config);
	if (szLan) gf_free((char *)szLan);
	if (icc_data) gf_free(icc_data);
	return e;
}

typedef struct
{
	u32 tk;
	Bool has_non_raps;
	u32 last_sample;
	u32 sample_count;
	u32 time_scale;
	u64 firstDTS, lastDTS;
	u32 dst_tk;
	/*set if media can be duplicated at split boundaries - only used for text tracks and provate tracks, this assumes all
	samples are RAP*/
	Bool can_duplicate;
	/*controls import by time rather than by sample (otherwise we would have to remove much more samples video vs audio for example*/
	Bool first_sample_done;
	Bool next_sample_is_rap;
	u32 stop_state;
} TKInfo;

GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u64 split_size_kb, char *inName, Double InterleavingTime, Double chunk_start_time, Bool adjust_split_end, char *outName, const char *tmpdir, Bool force_rap_split)
{
	u32 i, count, nb_tk, needs_rap_sync, cur_file, conv_type, nb_tk_done, nb_samp, nb_done, di;
	Double max_dur, cur_file_time;
	Bool do_add, all_duplicatable, size_exceeded, chunk_extraction, rap_split, split_until_end;
	GF_ISOFile *dest;
	GF_ISOSample *samp;
	GF_Err e;
	TKInfo *tks, *tki;
	char *ext, szName[1000], szFile[1000];
	Double chunk_start = (Double) chunk_start_time;

	chunk_extraction = (chunk_start>=0) ? 1 : 0;
	split_until_end = 0;
	rap_split = 0;
	if (split_size_kb == (u64)-1) rap_split = 1;
	if (split_dur == -1) rap_split = 1;
	else if (split_dur <= -2) {
		split_size_kb = 0;
		split_until_end = 1;
	}
	else if (force_rap_split)
		rap_split = 1;

	if (rap_split) {
		split_size_kb = 0;
		split_dur = (double) GF_MAX_FLOAT;
	}

	//split in same dir as source
	strcpy(szName, inName);
	ext = strrchr(szName, '.');
	if (ext) ext[0] = 0;
	ext = strrchr(inName, '.');

	dest = NULL;

	conv_type = 0;
	switch (gf_isom_guess_specification(mp4)) {
	case GF_ISOM_BRAND_ISMA:
		conv_type = 1;
		break;
	case GF_ISOM_BRAND_3GP4:
	case GF_ISOM_BRAND_3GP5:
	case GF_ISOM_BRAND_3GP6:
	case GF_ISOM_BRAND_3GG6:
	case GF_ISOM_BRAND_3G2A:
		conv_type = 2;
		break;
	}
	if (!stricmp(ext, ".3gp") || !stricmp(ext, ".3g2")) conv_type = 2;

	count = gf_isom_get_track_count(mp4);
	tks = (TKInfo *)gf_malloc(sizeof(TKInfo)*count);
	memset(tks, 0, sizeof(TKInfo)*count);

	e = GF_OK;
	max_dur = 0;
	nb_tk = 0;
	all_duplicatable = 1;
	needs_rap_sync = 0;
	nb_samp = 0;
	for (i=0; i<count; i++) {
		u32 mtype;
		Double dur;
		tks[nb_tk].tk = i+1;
		tks[nb_tk].can_duplicate = 0;

		mtype = gf_isom_get_media_type(mp4, i+1);
		switch (mtype) {
		/*we duplicate text samples at boundaries*/
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_MPEG_SUBT:
			tks[nb_tk].can_duplicate = 1;
		case GF_ISOM_MEDIA_AUDIO:
			break;
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
			if (gf_isom_get_sample_count(mp4, i+1)>1) {
				break;
			}
			continue;
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_OCR:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_OCI:
		case GF_ISOM_MEDIA_IPMP:
		case GF_ISOM_MEDIA_MPEGJ:
		case GF_ISOM_MEDIA_MPEG7:
		case GF_ISOM_MEDIA_FLASH:
			fprintf(stderr, "WARNING: Track ID %d (type %s) not handled by splitter - skipping\n", gf_isom_get_track_id(mp4, i+1), gf_4cc_to_str(mtype));
			continue;
		default:
			/*for all other track types, only split if more than one sample*/
			if (gf_isom_get_sample_count(mp4, i+1)==1) {
				fprintf(stderr, "WARNING: Track ID %d (type %s) not handled by splitter - skipping\n", gf_isom_get_track_id(mp4, i+1), gf_4cc_to_str(mtype));
				continue;
			}
			tks[nb_tk].can_duplicate = 1;
		}

		tks[nb_tk].sample_count = gf_isom_get_sample_count(mp4, i+1);
		nb_samp += tks[nb_tk].sample_count;
		tks[nb_tk].last_sample = 0;
		tks[nb_tk].firstDTS = 0;
		tks[nb_tk].time_scale = gf_isom_get_media_timescale(mp4, i+1);
		tks[nb_tk].has_non_raps = gf_isom_has_sync_points(mp4, i+1);
		/*seen that on some 3gp files from nokia ...*/
		if (mtype==GF_ISOM_MEDIA_AUDIO) tks[nb_tk].has_non_raps = 0;

		dur = (Double) (s64) gf_isom_get_media_duration(mp4, i+1);
		dur /= tks[nb_tk].time_scale;
		if (max_dur<dur) max_dur=dur;

		if (tks[nb_tk].has_non_raps) {
			/*we don't support that*/
			if (needs_rap_sync) {
				fprintf(stderr, "More than one track has non-sync points - cannot split file\n");
				gf_free(tks);
				return GF_NOT_SUPPORTED;
			}
			needs_rap_sync = nb_tk+1;
		}
		if (!tks[nb_tk].can_duplicate) all_duplicatable = 0;
		nb_tk++;
	}
	if (!nb_tk) {
		fprintf(stderr, "No suitable tracks found for splitting file\n");
		gf_free(tks);
		return GF_NOT_SUPPORTED;
	}
	if (chunk_start>=max_dur) {
		fprintf(stderr, "Input file (%f) shorter than requested split start offset (%f)\n", max_dur, chunk_start);
		gf_free(tks);
		return GF_NOT_SUPPORTED;
	}
	if (split_until_end) {
		if (split_dur < -2) {
			split_dur = - (split_dur + 2 - chunk_start);
			if (max_dur < split_dur) {
				fprintf(stderr, "Split duration till end %lf longer than track duration %lf\n", split_dur, max_dur);
				gf_free(tks);
				return GF_NOT_SUPPORTED;
			} else {
				split_dur = max_dur - split_dur;
			}
		} else {
			split_dur = max_dur;
		}
	} else if (!rap_split && (max_dur<=split_dur)) {
		fprintf(stderr, "Input file (%f) shorter than requested split duration (%f)\n", max_dur, split_dur);
		gf_free(tks);
		return GF_NOT_SUPPORTED;
	}
	if (needs_rap_sync) {
		Bool has_enough_sync = GF_FALSE;
		tki = &tks[needs_rap_sync-1];

		if (chunk_start == 0.0f)
			has_enough_sync = GF_TRUE;
		else if (gf_isom_get_sync_point_count(mp4, tki->tk) > 1)
			has_enough_sync = GF_TRUE;
		else if (gf_isom_get_sample_group_info(mp4, tki->tk, 1, GF_ISOM_SAMPLE_GROUP_RAP, NULL, NULL, NULL))
			has_enough_sync = GF_TRUE;
		else if (gf_isom_get_sample_group_info(mp4, tki->tk, 1, GF_ISOM_SAMPLE_GROUP_SYNC, NULL, NULL, NULL))
			has_enough_sync = GF_TRUE;

		if (!has_enough_sync) {
			fprintf(stderr, "Not enough Random Access points in input file - cannot split\n");
			gf_free(tks);
			return GF_NOT_SUPPORTED;
		}
	}
	split_size_kb *= 1024;
	cur_file_time = 0;

	if (chunk_start>0) {
		if (needs_rap_sync) {
			u32 sample_num;
			Double start;
			tki = &tks[needs_rap_sync-1];

			start = (Double) (s64) gf_isom_get_sample_dts(mp4, tki->tk, tki->sample_count);
			start /= tki->time_scale;
			if (start<chunk_start) {
				tki->stop_state = 2;
			} else  {
				samp = NULL;
				e = gf_isom_get_sample_for_media_time(mp4, tki->tk, (u64) (chunk_start*tki->time_scale), &di, GF_ISOM_SEARCH_SYNC_BACKWARD, &samp, &sample_num, NULL);
				if (e!=GF_OK) {
					fprintf(stderr, "Cannot locate RAP in track ID %d for chunk extraction from %02.2f sec\n", gf_isom_get_track_id(mp4, tki->tk), chunk_start);
					gf_free(tks);
					return GF_NOT_SUPPORTED;
				}
				start = (Double) (s64) samp->DTS;
				start /= tki->time_scale;
				gf_isom_sample_del(&samp);
				fprintf(stderr, "Adjusting chunk start time to previous random access at %02.2f sec\n", start);
				split_dur += (chunk_start - start);
				chunk_start = start;
			}
		}
		/*sync all tracks*/
		for (i=0; i<nb_tk; i++) {
			tki = &tks[i];
			while (tki->last_sample<tki->sample_count) {
				Double time;
				u64 dts;
				dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
				time = (Double) (s64) dts;
				time /= tki->time_scale;
				if (time>=chunk_start) {
					/*rewind one sample (text tracks & co)*/
					if (tki->can_duplicate && tki->last_sample) {
						tki->last_sample--;
						tki->firstDTS = (u64) (chunk_start*tki->time_scale);
					} else {
						tki->firstDTS = dts;
					}
					break;
				}
				tki->last_sample++;
			}
		}
		cur_file_time = chunk_start;
	} else {
		chunk_start = 0;
	}

	dest = NULL;
	nb_done = 0;
	nb_tk_done = 0;
	cur_file = 0;
	while (nb_tk_done<nb_tk) {
		Double last_rap_sample_time, max_dts, file_split_dur;
		Bool is_last_rap;
		Bool all_av_done = GF_FALSE;

		if (chunk_extraction) {
			sprintf(szFile, "%s_%d_%d%s", szName, (u32) chunk_start, (u32) (chunk_start+split_dur), ext);
			if (outName) strcpy(szFile, outName);
		} else {
			sprintf(szFile, "%s_%03d%s", szName, cur_file+1, ext);
			if (outName) {
				char *the_file = gf_url_concatenate(outName, szFile);
				if (the_file) {
					strcpy(szFile, the_file);
					gf_free(the_file);
				}
			}
		}
		dest = gf_isom_open(szFile, GF_ISOM_WRITE_EDIT, tmpdir);
		/*clone all tracks*/
		for (i=0; i<nb_tk; i++) {
			tki = &tks[i];
			/*track done - we remove the track from destination, an empty video track could cause pbs to some players*/
			if (tki->stop_state==2) continue;

			e = gf_isom_clone_track(mp4, tki->tk, dest, 0, &tki->dst_tk);
			if (e) {
				fprintf(stderr, "Error cloning track %d\n", tki->tk);
				goto err_exit;
			}
			/*use non-packet CTS offsets (faster add/remove)*/
			if (gf_isom_has_time_offset(mp4, tki->tk)) {
				gf_isom_set_cts_packing(dest, tki->dst_tk, GF_TRUE);
			}
			gf_isom_remove_edit_segments(dest, tki->dst_tk);
		}
		do_add = 1;
		is_last_rap = 0;
		last_rap_sample_time = 0;
		file_split_dur = split_dur;

		size_exceeded = 0;
		max_dts = 0;
		while (do_add) {
			Bool is_rap;
			Double time;
			u32 nb_over, nb_av = 0;
			/*perfom basic de-interleaving to make sure we're not importing too much of a given track*/
			u32 nb_add = 0;
			/*add one sample of each track*/
			for (i=0; i<nb_tk; i++) {
				Double t;
				u64 dts;
				tki = &tks[i];

				if (!tki->can_duplicate) nb_av++;

				if (tki->stop_state)
					continue;
				if (tki->last_sample==tki->sample_count)
					continue;

				/*get sample info, see if we need to check it (basic de-interleaver)*/
				dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);

				/*reinsertion (timed text)*/
				if (dts < tki->firstDTS) {
					samp = gf_isom_get_sample(mp4, tki->tk, tki->last_sample+1, &di);
					samp->DTS = 0;
					e = gf_isom_add_sample(dest, tki->dst_tk, di, samp);
					if (!e) {
						e = gf_isom_copy_sample_info(dest, tki->dst_tk, mp4, tki->tk, tki->last_sample+1);
					}

					gf_isom_sample_del(&samp);
					tki->last_sample += 1;
					dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
				}
				dts -= tki->firstDTS;


				t = (Double) (s64) dts;
				t /= tki->time_scale;
				if (tki->first_sample_done) {
					if (!all_av_done && (t>max_dts)) continue;
				} else {
					/*here's the trick: only take care of a/v media for splitting, and add other media
					only if thir dts is less than the max AV dts found. Otherwise with some text streams we will end up importing
					too much video and corrupting the last sync point indication*/
					if (!tki->can_duplicate && (t>max_dts)) max_dts = t;
					tki->first_sample_done = 1;
				}
				samp = gf_isom_get_sample(mp4, tki->tk, tki->last_sample+1, &di);
				samp->DTS -= tki->firstDTS;

				nb_add += 1;

				is_rap = GF_FALSE;
				if (samp->IsRAP) {
					is_rap = GF_TRUE;
				} else {
					Bool has_roll;
					gf_isom_get_sample_rap_roll_info(mp4, tki->tk, tki->last_sample+1, &is_rap, &has_roll, NULL);
				}


				if (tki->has_non_raps && is_rap) {
					GF_ISOSample *next_rap=NULL;
					u32 next_rap_num, sdi;
					last_rap_sample_time = (Double) (s64) samp->DTS;
					last_rap_sample_time /= tki->time_scale;
					e = gf_isom_get_sample_for_media_time(mp4, tki->tk, samp->DTS+tki->firstDTS+2, &sdi, GF_ISOM_SEARCH_SYNC_FORWARD, &next_rap, &next_rap_num, NULL);
					if (e==GF_EOS)
						is_last_rap = 1;
					if (next_rap) {
						if (!next_rap->IsRAP)
							is_last_rap = 1;
						gf_isom_sample_del(&next_rap);
					}
				}
				tki->lastDTS = samp->DTS;
				e = gf_isom_add_sample(dest, tki->dst_tk, di, samp);
				gf_isom_sample_del(&samp);

				if (!e) {
					e = gf_isom_copy_sample_info(dest, tki->dst_tk, mp4, tki->tk, tki->last_sample+1);
				}
				tki->last_sample += 1;
				gf_set_progress("Splitting", nb_done, nb_samp);
				nb_done++;
				if (e) {
					fprintf(stderr, "Error cloning track %d sample %d\n", tki->tk, tki->last_sample);
					goto err_exit;
				}

				tki->next_sample_is_rap = 0;
				if (rap_split && tki->has_non_raps) {
					if ( gf_isom_get_sample_sync(mp4, tki->tk, tki->last_sample+1))
						tki->next_sample_is_rap = 1;
				}
			}

			/*test by size/duration*/
			nb_over = 0;

			/*test by file size: same as duration test, only dynamically increment import duration*/
			if (split_size_kb) {
				u64 est_size = gf_isom_estimate_size(dest);
				/*while below desired size keep importing*/
				if (est_size<split_size_kb)
					file_split_dur = (Double) GF_MAX_FLOAT;
				else {
					size_exceeded = 1;
				}
			}

			for (i=0; i<nb_tk; i++) {
				tki = &tks[i];
				if (tki->stop_state) {
					nb_over++;
					if (!tki->can_duplicate && (tki->last_sample==tki->sample_count) )
						nb_av--;
					continue;
				}
				time = (Double) (s64) tki->lastDTS;
				time /= tki->time_scale;
				if (size_exceeded
				        || (tki->last_sample==tki->sample_count)
				        || (!tki->can_duplicate && (time>file_split_dur))
				        || (rap_split && tki->has_non_raps && tki->next_sample_is_rap)
				   ) {
					nb_over++;
					tki->stop_state = 1;
					if (tki->last_sample<tki->sample_count)
						is_last_rap = 0;
					else if (tki->first_sample_done)
						is_last_rap = 0;

					if (rap_split && tki->next_sample_is_rap) {
						file_split_dur = (Double) ( gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1) - tki->firstDTS);
						file_split_dur /= tki->time_scale;
					}
				}
				/*special tracks (not audio, not video)*/
				else if (tki->can_duplicate) {
					u64 dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
					time = (Double) (s64) (dts - tki->firstDTS);
					time /= tki->time_scale;
					if (time>file_split_dur) {
						nb_over++;
						tki->stop_state = 1;
					}
				}
				if (!nb_add && (!max_dts || (tki->lastDTS <= 1 + (u64) (tki->time_scale*max_dts) )))
					tki->first_sample_done = 0;
			}
			if (nb_over==nb_tk) do_add = 0;

			if (!nb_av)
				all_av_done = GF_TRUE;
		}

		/*remove samples - first figure out smallest duration*/
		file_split_dur = (Double) GF_MAX_FLOAT;
		for (i=0; i<nb_tk; i++) {
			Double time;
			tki = &tks[i];
			/*track done*/
			if ((tki->stop_state==2) || (!is_last_rap && (tki->sample_count == tki->last_sample)) ) {
				if (tki->has_non_raps) last_rap_sample_time = 0;
				time = (Double) (s64) ( gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1) - tki->firstDTS);
				time /= tki->time_scale;
				if (file_split_dur==(Double)GF_MAX_FLOAT || file_split_dur<time) file_split_dur = time;
				continue;
			}

			//if (tki->lastDTS)
			{
				//time = (Double) (s64) tki->lastDTS;
				time = (Double) (s64) ( gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1) - tki->firstDTS);
				time /= tki->time_scale;
				if ((!tki->can_duplicate || all_duplicatable) && time<file_split_dur) file_split_dur = time;
				else if (rap_split && tki->next_sample_is_rap) file_split_dur = time;
			}
		}
		if (file_split_dur == (Double) GF_MAX_FLOAT) {
			fprintf(stderr, "Cannot split file (duration too small or size too small)\n");
			goto err_exit;
		}
		if (chunk_extraction) {
			if (adjust_split_end) {
				fprintf(stderr, "Adjusting chunk end time to previous random access at %02.2f sec\n", chunk_start + last_rap_sample_time);
				file_split_dur = last_rap_sample_time;
				if (outName) strcpy(szFile, outName);
				else sprintf(szFile, "%s_%d_%d%s", szName, (u32) chunk_start, (u32) (chunk_start+file_split_dur), ext);
				gf_isom_set_final_name(dest, szFile);
			}
			else file_split_dur = split_dur;
		}

		/*don't split if eq to copy...*/
		if (is_last_rap && !cur_file && !chunk_start) {
			fprintf(stderr, "Cannot split file (Not enough sync samples, duration too large or size too big)\n");
			goto err_exit;
		}


		/*if not last chunk and longer duration adjust to previous RAP point*/
		if ( (size_exceeded || !split_size_kb) && (file_split_dur>split_dur) && !chunk_start) {
			/*if larger than last RAP, rewind till it*/
			if (last_rap_sample_time && (last_rap_sample_time<file_split_dur) ) {
				file_split_dur = last_rap_sample_time;
				is_last_rap = 0;
			}
		}

		nb_tk_done = 0;
		if (!is_last_rap || chunk_extraction) {
			for (i=0; i<nb_tk; i++) {
				Double time = 0;
				u32 last_samp;
				tki = &tks[i];
				while (1) {
					last_samp = gf_isom_get_sample_count(dest, tki->dst_tk);

					time = (Double) (s64) gf_isom_get_media_duration(dest, tki->dst_tk);
					//time could get slightly higher than requests dur due to rounding precision. We use 1/4 of the last sample dur as safety marge
					time -= (Double) (s64) gf_isom_get_sample_duration(dest, tki->dst_tk, tki->last_sample) / 4;
					time /= tki->time_scale;

					if (last_samp<=1) break;

					/*done*/
					if (tki->last_sample==tki->sample_count) {
						if (!chunk_extraction && !tki->can_duplicate) {
							tki->stop_state=2;
							break;
						}
					}

					if (time <= file_split_dur) break;

					gf_isom_remove_sample(dest, tki->dst_tk, last_samp);
					tki->last_sample--;
					assert(tki->last_sample);
					nb_done--;
					gf_set_progress("Splitting", nb_done, nb_samp);
				}
				if (tki->last_sample<tki->sample_count) {
					u64 dts;
					tki->stop_state = 0;
					dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
					time = (Double) (s64) (dts - tki->firstDTS);
					time /= tki->time_scale;
					/*re-insert prev sample*/
					if (tki->can_duplicate && (time>file_split_dur) ) {
						Bool was_insert = GF_FALSE;
						tki->last_sample--;
						dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
						if (dts < tki->firstDTS) was_insert = GF_TRUE;
						tki->firstDTS += (u64) (file_split_dur*tki->time_scale);
						//the original, last sample added starts before the first sample in the file: we have re-inserted
						//a single sample, use split duration as target duration
						if (was_insert) {
							gf_isom_set_last_sample_duration(dest, tki->dst_tk, (u32) (file_split_dur*tki->time_scale));
						} else {
							gf_isom_set_last_sample_duration(dest, tki->dst_tk, (u32) (tki->firstDTS - dts) );
						}
					} else {
						tki->firstDTS = dts;
					}
					tki->first_sample_done = 0;
				} else {
					nb_tk_done++;
				}

			}
		}

		if (chunk_extraction) {
			fprintf(stderr, "Extracting chunk %s - duration %02.2fs (%02.2fs->%02.2fs)\n", szFile, file_split_dur, chunk_start, (chunk_start+split_dur));
		} else {
			fprintf(stderr, "Storing split-file %s - duration %02.2f seconds\n", szFile, file_split_dur);
		}

		/*repack CTSs*/
		for (i=0; i<nb_tk; i++) {
			u32 j;
			u64 new_track_dur;
			tki = &tks[i];
			if (tki->stop_state == 2) continue;
			if (!gf_isom_get_sample_count(dest, tki->dst_tk)) {
				gf_isom_remove_track(dest, tki->dst_tk);
				continue;
			}
			if (gf_isom_has_time_offset(mp4, tki->tk)) {
				gf_isom_set_cts_packing(dest, tki->dst_tk, GF_FALSE);
			}
			if (is_last_rap && tki->can_duplicate) {
				gf_isom_set_last_sample_duration(dest, tki->dst_tk, gf_isom_get_sample_duration(mp4, tki->tk, tki->sample_count));
			}

			/*rewrite edit list*/
			new_track_dur = gf_isom_get_track_duration(dest, tki->dst_tk);
			count = gf_isom_get_edit_segment_count(mp4, tki->tk);
			if (count>2) {
				fprintf(stderr, "Warning: %d edit segments - not supported while splitting (max 2) - ignoring extra\n", count);
				count=2;
			}
			for (j=0; j<count; j++) {
				u64 editTime, segDur, MediaTime;
				u8 mode;

				gf_isom_get_edit_segment(mp4, tki->tk, j+1, &editTime, &segDur, &MediaTime, &mode);
				if (!j && (mode!=GF_ISOM_EDIT_EMPTY) ) {
					fprintf(stderr, "Warning: Edit list doesn't look like a track delay scheme - ignoring\n");
					break;
				}
				if (mode==GF_ISOM_EDIT_NORMAL) {
					segDur = new_track_dur;
				}
				gf_isom_set_edit_segment(dest, tki->dst_tk, editTime, segDur, MediaTime, mode);
			}
		}
		/*check chapters*/
		do_add = 1;
		for (i=0; i<gf_isom_get_chapter_count(mp4, 0); i++) {
			char *name;
			u64 chap_time;
			gf_isom_get_chapter(mp4, 0, i+1, &chap_time, (const char **) &name);
			max_dts = (Double) (s64) chap_time;
			max_dts /= 1000;
			if (max_dts<cur_file_time) continue;
			if (max_dts>cur_file_time+file_split_dur) break;
			max_dts-=cur_file_time;
			chap_time = (u64) (max_dts*1000);
			gf_isom_add_chapter(dest, 0, chap_time, name);
			/*add prev*/
			if (do_add && i) {
				gf_isom_get_chapter(mp4, 0, i, &chap_time, (const char **) &name);
				gf_isom_add_chapter(dest, 0, 0, name);
				do_add = 0;
			}
		}
		cur_file_time += file_split_dur;

		if (conv_type==1) gf_media_make_isma(dest, 1, 0, 0);
		else if (conv_type==2) gf_media_make_3gpp(dest);
		if (InterleavingTime) {
			gf_isom_make_interleave(dest, InterleavingTime);
		} else {
			gf_isom_set_storage_mode(dest, GF_ISOM_STORE_STREAMABLE);
		}

		gf_isom_clone_pl_indications(mp4, dest);
		e = gf_isom_close(dest);
		dest = NULL;
		if (e) fprintf(stderr, "Error storing file %s\n", gf_error_to_string(e));
		if (is_last_rap || chunk_extraction) break;
		cur_file++;
	}
	gf_set_progress("Splitting", nb_samp, nb_samp);
err_exit:
	if (dest) gf_isom_delete(dest);
	gf_free(tks);
	return e;
}

GF_Err cat_multiple_files(GF_ISOFile *dest, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);

static Bool merge_parameter_set(GF_List *src, GF_List *dst, const char *name)
{
	u32 j, k;
	for (j=0; j<gf_list_count(src); j++) {
		Bool found = 0;
		GF_AVCConfigSlot *slc = gf_list_get(src, j);
		for (k=0; k<gf_list_count(dst); k++) {
			GF_AVCConfigSlot *slc_dst = gf_list_get(dst, k);
			if ( (slc->size==slc_dst->size) && !memcmp(slc->data, slc_dst->data, slc->size) ) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return GF_FALSE;
		}
	}
	return GF_TRUE;
}

static u32 merge_avc_config(GF_ISOFile *dest, u32 tk_id, GF_ISOFile *orig, u32 src_track, Bool force_cat)
{
	GF_AVCConfig *avc_src, *avc_dst;
	u32 dst_tk = gf_isom_get_track_by_id(dest, tk_id);

	avc_src = gf_isom_avc_config_get(orig, src_track, 1);
	avc_dst = gf_isom_avc_config_get(dest, dst_tk, 1);

	if (avc_src->AVCLevelIndication!=avc_dst->AVCLevelIndication) {
		dst_tk = 0;
	} else if (avc_src->AVCProfileIndication!=avc_dst->AVCProfileIndication) {
		dst_tk = 0;
	}
	else {
		/*rewrite all samples if using different NALU size*/
		if (avc_src->nal_unit_size > avc_dst->nal_unit_size) {
			gf_media_nal_rewrite_samples(dest, dst_tk, 8*avc_src->nal_unit_size);
			avc_dst->nal_unit_size = avc_src->nal_unit_size;
		} else if (avc_src->nal_unit_size < avc_dst->nal_unit_size) {
			gf_media_nal_rewrite_samples(orig, src_track, 8*avc_dst->nal_unit_size);
		}

		/*merge PS*/
		if (!merge_parameter_set(avc_src->sequenceParameterSets, avc_dst->sequenceParameterSets, "SPS"))
			dst_tk = 0;
		if (!merge_parameter_set(avc_src->pictureParameterSets, avc_dst->pictureParameterSets, "PPS"))
			dst_tk = 0;

		gf_isom_avc_config_update(dest, dst_tk, 1, avc_dst);
	}

	gf_odf_avc_cfg_del(avc_src);
	gf_odf_avc_cfg_del(avc_dst);

	if (!dst_tk) {
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);
		gf_isom_set_nalu_extract_mode(orig, src_track, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		if (!force_cat) {
			gf_isom_avc_set_inband_config(dest, dst_tk, 1, GF_FALSE);
		} else {
			fprintf(stderr, "WARNING: Concatenating track ID %d even though sample descriptions do not match\n", tk_id);
		}
	}
	return dst_tk;
}

#ifndef GPAC_DISABLE_HEVC
static u32 merge_hevc_config(GF_ISOFile *dest, u32 tk_id, GF_ISOFile *orig, u32 src_track, Bool force_cat)
{
	u32 i;
	GF_HEVCConfig *hevc_src, *hevc_dst;
	u32 dst_tk = gf_isom_get_track_by_id(dest, tk_id);

	hevc_src = gf_isom_hevc_config_get(orig, src_track, 1);
	hevc_dst = gf_isom_hevc_config_get(dest, dst_tk, 1);

	if (hevc_src->profile_idc != hevc_dst->profile_idc) dst_tk = 0;
	else if (hevc_src->level_idc != hevc_dst->level_idc) dst_tk = 0;
	else if (hevc_src->general_profile_compatibility_flags != hevc_dst->general_profile_compatibility_flags ) dst_tk = 0;
	else {
		/*rewrite all samples if using different NALU size*/
		if (hevc_src->nal_unit_size > hevc_dst->nal_unit_size) {
			gf_media_nal_rewrite_samples(dest, dst_tk, 8*hevc_src->nal_unit_size);
			hevc_dst->nal_unit_size = hevc_src->nal_unit_size;
		} else if (hevc_src->nal_unit_size < hevc_dst->nal_unit_size) {
			gf_media_nal_rewrite_samples(orig, src_track, 8*hevc_dst->nal_unit_size);
		}

		/*merge PS*/
		for (i=0; i<gf_list_count(hevc_src->param_array); i++) {
			u32 k;
			GF_HEVCParamArray *src_ar = gf_list_get(hevc_src->param_array, i);
			for (k=0; k<gf_list_count(hevc_dst->param_array); k++) {
				GF_HEVCParamArray *dst_ar = gf_list_get(hevc_dst->param_array, k);
				if (dst_ar->type==src_ar->type) {
					if (!merge_parameter_set(src_ar->nalus, dst_ar->nalus, "SPS"))
						dst_tk = 0;
					break;
				}
			}
		}

		gf_isom_hevc_config_update(dest, dst_tk, 1, hevc_dst);
	}

	gf_odf_hevc_cfg_del(hevc_src);
	gf_odf_hevc_cfg_del(hevc_dst);

	if (!dst_tk) {
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);
		gf_isom_set_nalu_extract_mode(orig, src_track, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG);
		if (!force_cat) {
			gf_isom_hevc_set_inband_config(dest, dst_tk, 1, GF_FALSE);
		} else {
			fprintf(stderr, "WARNING: Concatenating track ID %d even though sample descriptions do not match\n", tk_id);
		}
	}
	return dst_tk;
}
#endif /*GPAC_DISABLE_HEVC */

GF_Err cat_playlist(GF_ISOFile *dest, char *playlistName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);

GF_Err cat_isomedia_file(GF_ISOFile *dest, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command, Bool is_pl)
{
	u32 i, j, count, nb_tracks, nb_samp, nb_done;
	GF_ISOFile *orig;
	GF_Err e;
	char *opts, *multi_cat;
	Double ts_scale;
	Double dest_orig_dur;
	u32 dst_tk, tk_id, mtype;
	u64 insert_dts;
	Bool is_isom;
	GF_ISOSample *samp;
	GF_Fraction64 aligned_to_DTS_frac;

	if (is_pl) return cat_playlist(dest, fileName, import_flags, force_fps, frames_per_sample, tmp_dir, force_cat, align_timelines, allow_add_in_command);

	if (strchr(fileName, '*') || (strchr(fileName, '@')) )
		return cat_multiple_files(dest, fileName, import_flags, force_fps, frames_per_sample, tmp_dir, force_cat, align_timelines, allow_add_in_command);

	multi_cat = allow_add_in_command ? strchr(fileName, '+') : NULL;
	if (multi_cat) {
		multi_cat[0] = 0;
		multi_cat = &multi_cat[1];
	}
	opts = strchr(fileName, ':');
	if (opts && (opts[1]=='\\'))
		opts = strchr(fileName, ':');

	e = GF_OK;

	/*if options are specified, reimport the file*/
	is_isom = opts ? 0 : gf_isom_probe_file(fileName);

	if (!is_isom || opts) {
		orig = gf_isom_open("temp", GF_ISOM_WRITE_EDIT, tmp_dir);
		e = import_file(orig, fileName, import_flags, force_fps, frames_per_sample);
		if (e) return e;
	} else {
		/*we open the original file in edit mode since we may have to rewrite AVC samples*/
		orig = gf_isom_open(fileName, GF_ISOM_OPEN_EDIT, tmp_dir);
	}

	while (multi_cat) {
		char *sep = strchr(multi_cat, '+');
		if (sep) sep[0] = 0;

		e = import_file(orig, multi_cat, import_flags, force_fps, frames_per_sample);
		if (e) {
			gf_isom_delete(orig);
			return e;
		}
		if (!sep) break;
		sep[0]=':';
		multi_cat = sep+1;
	}

	nb_samp = 0;
	nb_tracks = gf_isom_get_track_count(orig);
	for (i=0; i<nb_tracks; i++) {
		u32 mtype = gf_isom_get_media_type(orig, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_FLASH:
			fprintf(stderr, "WARNING: Track ID %d (type %s) not handled by concatenation - removing from destination\n", gf_isom_get_track_id(orig, i+1), gf_4cc_to_str(mtype));
			continue;
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_MPEG_SUBT:
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_OCR:
		case GF_ISOM_MEDIA_OCI:
		case GF_ISOM_MEDIA_IPMP:
		case GF_ISOM_MEDIA_MPEGJ:
		case GF_ISOM_MEDIA_MPEG7:
		default:
			/*only cat self-contained files*/
			if (gf_isom_is_self_contained(orig, i+1, 1)) {
				nb_samp+= gf_isom_get_sample_count(orig, i+1);
				break;
			}
			break;
		}
	}
	if (!nb_samp) {
		fprintf(stderr, "No suitable media tracks to cat in %s - skipping\n", fileName);
		goto err_exit;
	}

	dest_orig_dur = (Double) (s64) gf_isom_get_duration(dest);
	if (!gf_isom_get_timescale(dest)) {
		gf_isom_set_timescale(dest, gf_isom_get_timescale(orig));
	}
	dest_orig_dur /= gf_isom_get_timescale(dest);

	aligned_to_DTS_frac.num = 0;
	aligned_to_DTS_frac.den = 1;
	for (i=0; i<gf_isom_get_track_count(dest); i++) {
		u64 track_dur = gf_isom_get_media_duration(dest, i+1);
		u32 track_ts = gf_isom_get_media_timescale(dest, i+1);
		if ((u64)aligned_to_DTS_frac.num * track_ts < track_dur * aligned_to_DTS_frac.den) {
			aligned_to_DTS_frac.num = track_dur;
			aligned_to_DTS_frac.den = track_ts;
		}
	}

	fprintf(stderr, "Appending file %s\n", fileName);
	nb_done = 0;
	for (i=0; i<nb_tracks; i++) {
		u64 last_DTS, dest_track_dur_before_cat;
		u32 nb_edits = 0;
		Bool skip_lang_test = 1;
		Bool use_ts_dur = 1;
		Bool merge_edits = 0;
		Bool new_track = 0;
		mtype = gf_isom_get_media_type(orig, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_FLASH:
			continue;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_MPEG_SUBT:
		case GF_ISOM_MEDIA_SCENE:
			use_ts_dur = 0;
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_OCR:
		case GF_ISOM_MEDIA_OCI:
		case GF_ISOM_MEDIA_IPMP:
		case GF_ISOM_MEDIA_MPEGJ:
		case GF_ISOM_MEDIA_MPEG7:
		default:
			if (!gf_isom_is_self_contained(orig, i+1, 1)) continue;
			break;
		}

		dst_tk = 0;
		/*if we had a temporary import of the file, check if the original track ID matches the dst one. If so, skip all language detection code*/
		tk_id = gf_isom_get_track_original_id(orig, i+1);
		if (!tk_id) {
			tk_id = gf_isom_get_track_id(orig, i+1);
			skip_lang_test = 0;
		}
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);


		if (dst_tk) {
			if (mtype != gf_isom_get_media_type(dest, dst_tk))
				dst_tk = 0;
			else {
				u32 subtype_dst = gf_isom_get_media_subtype(dest, dst_tk, 1);
				u32 subtype_src = gf_isom_get_media_subtype(orig, i+1, 1);
				if (subtype_dst==GF_ISOM_SUBTYPE_AVC3_H264)
					subtype_dst=GF_ISOM_SUBTYPE_AVC_H264 ;
				if (subtype_src==GF_ISOM_SUBTYPE_AVC3_H264)
					subtype_src=GF_ISOM_SUBTYPE_AVC_H264;

				if (subtype_dst==GF_ISOM_SUBTYPE_HEV1)
					subtype_dst=GF_ISOM_SUBTYPE_HVC1;
				if (subtype_src==GF_ISOM_SUBTYPE_HEV1)
					subtype_src=GF_ISOM_SUBTYPE_HVC1;

				if (subtype_dst != subtype_src) {
					dst_tk = 0;
				}
			}
		}

		if (!dst_tk) {
			for (j=0; j<gf_isom_get_track_count(dest); j++) {
				if (mtype != gf_isom_get_media_type(dest, j+1)) continue;
				if (gf_isom_is_same_sample_description(orig, i+1, 0, dest, j+1, 0)) {
					if (gf_isom_is_video_subtype(mtype) ) {
						u32 w, h, ow, oh;
						gf_isom_get_visual_info(orig, i+1, 1, &ow, &oh);
						gf_isom_get_visual_info(dest, j+1, 1, &w, &h);
						if ((ow==w) && (oh==h)) {
							dst_tk = j+1;
							break;
						}
					}
					/*check language code*/
					else if (!skip_lang_test && (mtype==GF_ISOM_MEDIA_AUDIO)) {
						u32 lang_src, lang_dst;
						char *lang = NULL;
						gf_isom_get_media_language(orig, i+1, &lang);
						if (lang) {
							lang_src = GF_4CC(lang[0], lang[1], lang[2], lang[3]);
							gf_free(lang);
						} else {
							lang_src = 0;
						}
						gf_isom_get_media_language(dest, j+1, &lang);
						if (lang) {
							lang_dst = GF_4CC(lang[0], lang[1], lang[2], lang[3]);
							gf_free(lang);
						} else {
							lang_dst = 0;
						}
						if (lang_dst==lang_src) {
							dst_tk = j+1;
							break;
						}
					} else {
						dst_tk = j+1;
						break;
					}
				}
			}
		}

		if (dst_tk) {
			u32 found_dst_tk = dst_tk;
			u32 stype = gf_isom_get_media_subtype(dest, dst_tk, 1);
			/*we MUST have the same codec*/
			if (gf_isom_get_media_subtype(orig, i+1, 1) != stype) dst_tk = 0;
			/*we only support cat with the same number of sample descriptions*/
			if (gf_isom_get_sample_description_count(orig, i+1) != gf_isom_get_sample_description_count(dest, dst_tk)) dst_tk = 0;
			/*if not forcing cat, check the media codec config is the same*/
			if (!gf_isom_is_same_sample_description(orig, i+1, 0, dest, dst_tk, 0)) {
				dst_tk = 0;
			}
			/*we force the same visual resolution*/
			else if (gf_isom_is_video_subtype(mtype) ) {
				u32 w, h, ow, oh;
				gf_isom_get_visual_info(orig, i+1, 1, &ow, &oh);
				gf_isom_get_visual_info(dest, dst_tk, 1, &w, &h);
				if ((ow!=w) || (oh!=h)) {
					dst_tk = 0;
				}
			}

			if (!dst_tk) {
				/*merge AVC config if possible*/
				if ((stype == GF_ISOM_SUBTYPE_AVC_H264)
				        || (stype == GF_ISOM_SUBTYPE_AVC2_H264)
				        || (stype == GF_ISOM_SUBTYPE_AVC3_H264)
				        || (stype == GF_ISOM_SUBTYPE_AVC4_H264) ) {
					dst_tk = merge_avc_config(dest, tk_id, orig, i+1, force_cat);
				}
#ifndef GPAC_DISABLE_HEVC
				/*merge HEVC config if possible*/
				else if ((stype == GF_ISOM_SUBTYPE_HVC1)
				         || (stype == GF_ISOM_SUBTYPE_HEV1)
				         || (stype == GF_ISOM_SUBTYPE_HVC2)
				         || (stype == GF_ISOM_SUBTYPE_HEV2)) {
					dst_tk = merge_hevc_config(dest, tk_id, orig, i+1, force_cat);
				}
#endif /*GPAC_DISABLE_HEVC*/
				else if (force_cat) {
					dst_tk = found_dst_tk;
				}
			}
		}

		/*looks like a new track*/
		if (!dst_tk) {
			fprintf(stderr, "No suitable destination track found - creating new one (type %s)\n", gf_4cc_to_str(mtype));
			e = gf_isom_clone_track(orig, i+1, dest, 0, &dst_tk);
			if (e) goto err_exit;
			gf_isom_clone_pl_indications(orig, dest);
			new_track = 1;

			if (align_timelines) {
				u32 max_timescale = 0;
//				u32 dst_timescale = 0;
				u32 idx;
				for (idx=0; idx<nb_tracks; idx++) {
					if (max_timescale < gf_isom_get_media_timescale(orig, idx+1))
						max_timescale = gf_isom_get_media_timescale(orig, idx+1);
				}
#if 0
				if (dst_timescale < max_timescale) {
					dst_timescale = gf_isom_get_media_timescale(dest, dst_tk);
					idx = max_timescale / dst_timescale;
					if (dst_timescale * idx < max_timescale) idx ++;
					dst_timescale *= idx;

					gf_isom_set_media_timescale(dest, dst_tk, max_timescale, 0);
				}
#else
				gf_isom_set_media_timescale(dest, dst_tk, max_timescale, 0);
#endif
			}

			/*remove cloned edit list, as it will be rewritten after import*/
			gf_isom_remove_edit_segments(dest, dst_tk);
		} else {
			nb_edits = gf_isom_get_edit_segment_count(orig, i+1);
		}

		dest_track_dur_before_cat = gf_isom_get_media_duration(dest, dst_tk);
		count = gf_isom_get_sample_count(dest, dst_tk);

		if (align_timelines) {
			insert_dts = (u64) (aligned_to_DTS_frac.num * gf_isom_get_media_timescale(dest, dst_tk));
			insert_dts /= aligned_to_DTS_frac.den;
		} else if (use_ts_dur && (count>1)) {
			insert_dts = 2*gf_isom_get_sample_dts(dest, dst_tk, count) - gf_isom_get_sample_dts(dest, dst_tk, count-1);
		} else {
			insert_dts = dest_track_dur_before_cat;
			if (!count) insert_dts = 0;
		}

		ts_scale = gf_isom_get_media_timescale(dest, dst_tk);
		ts_scale /= gf_isom_get_media_timescale(orig, i+1);

		/*if not a new track, see if we can merge the edit list - this is a crude test that only checks
		we have the same edit types*/
		if (nb_edits && (nb_edits == gf_isom_get_edit_segment_count(dest, dst_tk)) ) {
			u64 editTime, segmentDuration, mediaTime, dst_editTime, dst_segmentDuration, dst_mediaTime;
			u8 dst_editMode, editMode;
			u32 j;
			merge_edits = 1;
			for (j=0; j<nb_edits; j++) {
				gf_isom_get_edit_segment(orig, i+1, j+1, &editTime, &segmentDuration, &mediaTime, &editMode);
				gf_isom_get_edit_segment(dest, dst_tk, j+1, &dst_editTime, &dst_segmentDuration, &dst_mediaTime, &dst_editMode);

				if (dst_editMode!=editMode) {
					merge_edits=0;
					break;
				}
			}
		}

		gf_isom_enable_raw_pack(orig, i+1, 2048);

		last_DTS = 0;
		count = gf_isom_get_sample_count(orig, i+1);
		for (j=0; j<count; j++) {
			u32 di;
			samp = gf_isom_get_sample(orig, i+1, j+1, &di);
			last_DTS = samp->DTS;
			samp->DTS =  (u64) (ts_scale * samp->DTS + (new_track ? 0 : insert_dts));
			samp->CTS_Offset =  (u32) (samp->CTS_Offset * ts_scale);

			if (gf_isom_is_self_contained(orig, i+1, di)) {
				e = gf_isom_add_sample(dest, dst_tk, di, samp);
			} else {
				u64 offset;
				GF_ISOSample *s = gf_isom_get_sample_info(orig, i+1, j+1, &di, &offset);
				e = gf_isom_add_sample_reference(dest, dst_tk, di, samp, offset);
				gf_isom_sample_del(&s);
			}
			if (samp->nb_pack)
				j+= samp->nb_pack-1;

			gf_isom_sample_del(&samp);
			if (e) goto err_exit;

			e = gf_isom_copy_sample_info(dest, dst_tk, orig, i+1, j+1);
			if (e) goto err_exit;

			gf_set_progress("Appending", nb_done, nb_samp);
			nb_done++;
		}
		/*scene description and text: compute last sample duration based on original media duration*/
		if (!use_ts_dur) {
			u64 extend_dur = gf_isom_get_media_duration(orig, i+1) - last_DTS;
			//extend the duration of the last sample, but don't insert any edit list entry
			gf_isom_set_last_sample_duration(dest, dst_tk, (u32) (ts_scale*extend_dur) );
		}

		if (new_track && insert_dts) {
			u64 media_dur = gf_isom_get_media_duration(orig, i+1);
			/*convert from media time to track time*/
			Double rescale = (Float) gf_isom_get_timescale(dest);
			rescale /= (Float) gf_isom_get_media_timescale(dest, dst_tk);
			/*convert from orig to dst time scale*/
			rescale *= ts_scale;

			gf_isom_set_edit_segment(dest, dst_tk, 0, (u64) (s64) (insert_dts*rescale), 0, GF_ISOM_EDIT_EMPTY);
			gf_isom_set_edit_segment(dest, dst_tk, (u64) (s64) (insert_dts*rescale), (u64) (s64) (media_dur*rescale), 0, GF_ISOM_EDIT_NORMAL);
		} else if (merge_edits) {
			/*convert from media time to track time*/
			u32 movts_dst = gf_isom_get_timescale(dest);
			u32 trackts_dst = gf_isom_get_media_timescale(dest, dst_tk);
			/*convert from orig to dst time scale*/
			movts_dst = (u32) (movts_dst * ts_scale);

			/*get the first edit normal mode and add the new track dur*/
			for (j=nb_edits; j>0; j--) {
				u64 editTime, segmentDuration, mediaTime;
				u8 editMode;
				gf_isom_get_edit_segment(dest, dst_tk, j, &editTime, &segmentDuration, &mediaTime, &editMode);

				if (editMode==GF_ISOM_EDIT_NORMAL) {
					Double prev_dur = (Double) (s64) dest_track_dur_before_cat;
					Double dur = (Double) (s64) gf_isom_get_media_duration(orig, i+1);

					dur *= movts_dst;
					dur /= trackts_dst;
					prev_dur *= movts_dst;
					prev_dur /= trackts_dst;

					/*safety test: some files have broken edit lists. If no more than 2 entries, check that the segment duration
					is less or equal to the movie duration*/
					if (prev_dur < segmentDuration) {
						fprintf(stderr, "Warning: suspicious edit list entry found: duration %g sec but longest track duration before cat is %g - fixing it\n", (Double) (s64) segmentDuration/1000.0, prev_dur/1000);
						segmentDuration = (dest_track_dur_before_cat - mediaTime) * movts_dst;
						segmentDuration /= trackts_dst;
					}

					segmentDuration += (u64) (s64) dur;
					gf_isom_modify_edit_segment(dest, dst_tk, j, segmentDuration, mediaTime, editMode);
					break;
				}
			}
		} else {
			u64 editTime, segmentDuration, mediaTime, edit_offset;
			Double t;
			u8 editMode;
			u32 j, count;

			count = gf_isom_get_edit_segment_count(dest, dst_tk);
			if (count) {
				e = gf_isom_get_edit_segment(dest, dst_tk, count, &editTime, &segmentDuration, &mediaTime, &editMode);
				if (e) {
					fprintf(stderr, "Error: edit segment error on destination track %u could not be retrieved.\n", dst_tk);
					goto err_exit;
				}
			} else if (gf_isom_get_edit_segment_count(orig, i+1)) {
				/*fake empty edit segment*/
				/*convert from media time to track time*/
				Double rescale = (Float) gf_isom_get_timescale(dest);
				rescale /= (Float) gf_isom_get_media_timescale(dest, dst_tk);
				segmentDuration = (u64) (dest_track_dur_before_cat * rescale);
				editTime = 0;
				mediaTime = 0;
				gf_isom_set_edit_segment(dest, dst_tk, editTime, segmentDuration, mediaTime, GF_ISOM_EDIT_NORMAL);
			} else {
				editTime = 0;
				segmentDuration = 0;
			}

			/*convert to dst time scale*/
			ts_scale = (Float) gf_isom_get_timescale(dest);
			ts_scale /= (Float) gf_isom_get_timescale(orig);

			edit_offset = editTime + segmentDuration;
			count = gf_isom_get_edit_segment_count(orig, i+1);
			for (j=0; j<count; j++) {
				gf_isom_get_edit_segment(orig, i+1, j+1, &editTime, &segmentDuration, &mediaTime, &editMode);
				t = (Double) (s64) editTime;
				t *= ts_scale;
				t += (s64) edit_offset;
				editTime = (s64) t;
				t = (Double) (s64) segmentDuration;
				t *= ts_scale;
				segmentDuration = (s64) t;
				t = (Double) (s64) mediaTime;
				t *= ts_scale;
				t+= (s64) dest_track_dur_before_cat;
				mediaTime = (s64) t;
				if ((editMode == GF_ISOM_EDIT_EMPTY) && (mediaTime > 0)) {
					editMode = GF_ISOM_EDIT_NORMAL;
				}
				gf_isom_set_edit_segment(dest, dst_tk, editTime, segmentDuration, mediaTime, editMode);
			}
		}
		gf_media_update_bitrate(dest, dst_tk);

	}
	gf_set_progress("Appending", nb_samp, nb_samp);

	/*check brands*/
	gf_isom_get_brand_info(orig, NULL, NULL, &j);
	for (i=0; i<j; i++) {
		u32 brand;
		gf_isom_get_alternate_brand(orig, i+1, &brand);
		gf_isom_modify_alternate_brand(dest, brand, 1);
	}
	/*check chapters*/
	for (i=0; i<gf_isom_get_chapter_count(orig, 0); i++) {
		char *name;
		Double c_time;
		u64 chap_time;
		gf_isom_get_chapter(orig, 0, i+1, &chap_time, (const char **) &name);
		c_time = (Double) (s64) chap_time;
		c_time /= 1000;
		c_time += dest_orig_dur;

		/*check last file chapter*/
		if (!i && gf_isom_get_chapter_count(dest, 0)) {
			const char *last_name;
			u64 last_chap_time;
			gf_isom_get_chapter(dest, 0, gf_isom_get_chapter_count(dest, 0), &last_chap_time, &last_name);
			/*last and first chapters are the same, don't duplicate*/
			if (last_name && name && !stricmp(last_name, name)) continue;
		}

		chap_time = (u64) (c_time*1000);
		gf_isom_add_chapter(dest, 0, chap_time, name);
	}


err_exit:
	gf_isom_delete(orig);
	return e;
}

typedef struct
{
	char szPath[GF_MAX_PATH];
	char szRad1[1024], szRad2[1024], szOpt[200];
	GF_ISOFile *dest;
	u32 import_flags;
	GF_Fraction force_fps;
	u32 frames_per_sample;
	char *tmp_dir;
	Bool force_cat, align_timelines, allow_add_in_command;
} CATEnum;

Bool cat_enumerate(void *cbk, char *szName, char *szPath, GF_FileEnumInfo *file_info)
{
	GF_Err e;
	u32 len_rad1;
	char szFileName[GF_MAX_PATH];
	CATEnum *cat_enum = (CATEnum *)cbk;
	len_rad1 = (u32) strlen(cat_enum->szRad1);
	if (strnicmp(szName, cat_enum->szRad1, len_rad1)) return 0;
	if (strlen(cat_enum->szRad2) && !strstr(szName + len_rad1, cat_enum->szRad2) ) return 0;

	strcpy(szFileName, szPath);
	strcat(szFileName, cat_enum->szOpt);

	e = cat_isomedia_file(cat_enum->dest, szFileName, cat_enum->import_flags, cat_enum->force_fps, cat_enum->frames_per_sample, cat_enum->tmp_dir, cat_enum->force_cat, cat_enum->align_timelines, cat_enum->allow_add_in_command, GF_FALSE);
	if (e) return 1;
	return 0;
}

GF_Err cat_multiple_files(GF_ISOFile *dest, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command)
{
	CATEnum cat_enum;
	char *sep;

	cat_enum.dest = dest;
	cat_enum.import_flags = import_flags;
	cat_enum.force_fps = force_fps;
	cat_enum.frames_per_sample = frames_per_sample;
	cat_enum.tmp_dir = tmp_dir;
	cat_enum.force_cat = force_cat;
	cat_enum.align_timelines = align_timelines;
	cat_enum.allow_add_in_command = allow_add_in_command;

	if (strlen(fileName) >= sizeof(cat_enum.szPath)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", fileName));
		return GF_NOT_SUPPORTED;
	}
	strcpy(cat_enum.szPath, fileName);
	sep = strrchr(cat_enum.szPath, GF_PATH_SEPARATOR);
	if (!sep) sep = strrchr(cat_enum.szPath, '/');
	if (!sep) {
		strcpy(cat_enum.szPath, ".");
		if (strlen(fileName) >= sizeof(cat_enum.szRad1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", fileName));
			return GF_NOT_SUPPORTED;
		}
		strcpy(cat_enum.szRad1, fileName);
	} else {
		if (strlen(sep + 1) >= sizeof(cat_enum.szRad1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", (sep + 1)));
			return GF_NOT_SUPPORTED;
		}
		strcpy(cat_enum.szRad1, sep+1);
		sep[0] = 0;
	}
	sep = strchr(cat_enum.szRad1, '*');
	if (!sep) sep = strchr(cat_enum.szRad1, '@');
	if (strlen(sep + 1) >= sizeof(cat_enum.szRad2)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("File name %s is too long.\n", (sep + 1)));
		return GF_NOT_SUPPORTED;
	}
	strcpy(cat_enum.szRad2, sep+1);
	sep[0] = 0;
	sep = strchr(cat_enum.szRad2, '%');
	if (!sep) sep = strchr(cat_enum.szRad2, '#');
	if (!sep) sep = strchr(cat_enum.szRad2, ':');
	strcpy(cat_enum.szOpt, "");
	if (sep) {
		if (strlen(sep) >= sizeof(cat_enum.szOpt)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Invalid option: %s.\n", sep));
			return GF_NOT_SUPPORTED;
		}
		strcpy(cat_enum.szOpt, sep);
		sep[0] = 0;
	}
	return gf_enum_directory(cat_enum.szPath, 0, cat_enumerate, &cat_enum, NULL);
}

GF_Err cat_playlist(GF_ISOFile *dest, char *playlistName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command)
{
	GF_Err e;
	FILE *pl = gf_fopen(playlistName, "r");
	if (!pl) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Failed to open playlist file %s\n", playlistName));
		return GF_URL_ERROR;
	}

	e = GF_OK;
	while (!feof(pl)) {
		char szLine[10000];
		char *url;
		u32 len;
		szLine[0] = 0;
		if (fgets(szLine, 10000, pl) == NULL) break;
		if (szLine[0]=='#') continue;
		len = (u32) strlen(szLine);
		while (len && strchr("\r\n \t", szLine[len-1])) {
			szLine[len-1] = 0;
			len--;
		}
		if (!len) continue;

		url = gf_url_concatenate(playlistName, szLine);
		if (!url) url = gf_strdup(szLine);

		e = cat_isomedia_file(dest, url, import_flags, force_fps, frames_per_sample, tmp_dir, force_cat, align_timelines, allow_add_in_command, GF_FALSE);


		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Failed to concatenate file %s\n", url));
			gf_free(url);
			break;
		}
		gf_free(url);
	}
	gf_fclose(pl);
	return e;
}

#ifndef GPAC_DISABLE_SCENE_ENCODER
/*
		MPEG-4 encoding
*/

GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs)
{
#ifdef GPAC_DISABLE_SMGR
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	GF_SceneLoader load;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
#ifndef GPAC_DISABLE_SCENE_STATS
	GF_StatManager *statsman = NULL;
#endif

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = in;
	load.ctx = ctx;
	load.swf_import_flags = swf_flags;
	load.swf_flatten_limit = swf_flatten_angle;
	/*since we're encoding we must get MPEG4 nodes only*/
	load.flags = GF_SM_LOAD_MPEG4_STRICT;
	e = gf_sm_load_init(&load);
	if (e<0) {
		gf_sm_load_done(&load);
		fprintf(stderr, "Cannot load context %s - %s\n", in, gf_error_to_string(e));
		goto err_exit;
	}
	e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);

#ifndef GPAC_DISABLE_SCENE_STATS
	if (opts->auto_quant) {
		fprintf(stderr, "Analysing Scene for Automatic Quantization\n");
		statsman = gf_sm_stats_new();
		e = gf_sm_stats_for_scene(statsman, ctx);
		if (!e) {
			GF_SceneStatistics *stats = gf_sm_stats_get(statsman);
			/*LASeR*/
			if (opts->auto_quant==1) {
				if (opts->resolution > (s32)stats->frac_res_2d) {
					fprintf(stderr, " Given resolution %d is (unnecessarily) too high, using %d instead.\n", opts->resolution, stats->frac_res_2d);
					opts->resolution = stats->frac_res_2d;
				} else if (stats->int_res_2d + opts->resolution <= 0) {
					fprintf(stderr, " Given resolution %d is too low, using %d instead.\n", opts->resolution, stats->int_res_2d - 1);
					opts->resolution = 1 - stats->int_res_2d;
				}
				opts->coord_bits = stats->int_res_2d + opts->resolution;
				fprintf(stderr, " Coordinates & Lengths encoded using ");
				if (opts->resolution < 0) fprintf(stderr, "only the %d most significant bits (of %d).\n", opts->coord_bits, stats->int_res_2d);
				else fprintf(stderr, "a %d.%d representation\n", stats->int_res_2d, opts->resolution);

				fprintf(stderr, " Matrix Scale & Skew Coefficients ");
				if (opts->coord_bits - 8 < stats->scale_int_res_2d) {
					opts->scale_bits = stats->scale_int_res_2d - opts->coord_bits + 8;
					fprintf(stderr, "encoded using a %d.8 representation\n", stats->scale_int_res_2d);
				} else  {
					opts->scale_bits = 0;
					fprintf(stderr, "encoded using a %d.8 representation\n", opts->coord_bits - 8);
				}
			}
#ifndef GPAC_DISABLE_VRML
			/*BIFS*/
			else if (stats->base_layer) {
				GF_AUContext *au;
				GF_CommandField *inf;
				M_QuantizationParameter *qp;
				GF_Command *com = gf_sg_command_new(ctx->scene_graph, GF_SG_GLOBAL_QUANTIZER);
				qp = (M_QuantizationParameter *) gf_node_new(ctx->scene_graph, TAG_MPEG4_QuantizationParameter);

				inf = gf_sg_command_field_new(com);
				inf->new_node = (GF_Node *)qp;
				inf->field_ptr = &inf->new_node;
				inf->fieldType = GF_SG_VRML_SFNODE;
				gf_node_register(inf->new_node, NULL);
				au = gf_list_get(stats->base_layer->AUs, 0);
				gf_list_insert(au->commands, com, 0);
				qp->useEfficientCoding = 1;
				qp->textureCoordinateQuant = 0;
				if ((stats->count_2f+stats->count_2d) && opts->resolution) {
					qp->position2DMin = stats->min_2d;
					qp->position2DMax = stats->max_2d;
					qp->position2DNbBits = opts->resolution;
					qp->position2DQuant = 1;
				}
				if ((stats->count_3f+stats->count_3d) &&  opts->resolution) {
					qp->position3DMin = stats->min_3d;
					qp->position3DMax = stats->max_3d;
					qp->position3DQuant = opts->resolution;
					qp->position3DQuant = 1;
					qp->textureCoordinateQuant = 1;
				}
				//float quantif is disabled since 2008, check if we want to re-enable it
#if 0
				if (stats->count_float && opts->resolution) {
					qp->scaleMin = stats->min_fixed;
					qp->scaleMax = stats->max_fixed;
					qp->scaleNbBits = 2*opts->resolution;
					qp->scaleQuant = 1;
				}
#endif
			}
#endif
		}
	}
#endif /*GPAC_DISABLE_SCENE_STATS*/

	if (e<0) {
		fprintf(stderr, "Error loading file %s\n", gf_error_to_string(e));
		goto err_exit;
	} else {
		gf_log_cbk prev_logs = NULL;
		if (logs) {
			gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_DEBUG);
			prev_logs = gf_log_set_callback(logs, scene_coding_log);
		}
		opts->src_url = in;
		e = gf_sm_encode_to_file(ctx, mp4, opts);
		if (logs) {
			gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_ERROR);
			gf_log_set_callback(NULL, prev_logs);
		}
	}

	gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_MP42, 1);
	gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_ISOM, 1);

err_exit:
#ifndef GPAC_DISABLE_SCENE_STATS
	if (statsman) gf_sm_stats_del(statsman);
#endif
	gf_sm_del(ctx);
	gf_sg_del(sg);
	return e;

#endif /*GPAC_DISABLE_SMGR*/
}
#endif /*GPAC_DISABLE_SCENE_ENCODER*/


#ifndef GPAC_DISABLE_BIFS_ENC
/*
		MPEG-4 chunk encoding
*/

static u32 GetNbBits(u32 MaxVal)
{
	u32 k=0;
	while ((s32) MaxVal > ((1<<k)-1) ) k+=1;
	return k;
}

#ifndef GPAC_DISABLE_SMGR
GF_Err EncodeBIFSChunk(GF_SceneManager *ctx, char *bifsOutputFile, GF_Err (*AUCallback)(GF_ISOSample *))
{
	GF_Err			e;
	u8 *data;
	u32 data_len;
	GF_BifsEncoder *bifsenc;
	GF_InitialObjectDescriptor *iod;
	u32 i, j, count;
	Bool encode_names, delete_bcfg;
	GF_BIFSConfig *bcfg;
	GF_AUContext		*au;
	char szRad[GF_MAX_PATH], *ext;
	char szName[1024];
	FILE *f;

	strcpy(szRad, bifsOutputFile);
	ext = strrchr(szRad, '.');
	if (ext) ext[0] = 0;


	/* step3: encoding all AUs in ctx->streams starting at AU index 1 (0 is SceneReplace from previous context) */
	bifsenc = gf_bifs_encoder_new(ctx->scene_graph);
	e = GF_OK;

	iod = (GF_InitialObjectDescriptor *) ctx->root_od;
	/*if no iod check we only have one bifs*/
	if (!iod) {
		count = 0;
		for (i=0; i<gf_list_count(ctx->streams); i++) {
			GF_StreamContext *sc = gf_list_get(ctx->streams, i);
			if (sc->streamType == GF_STREAM_OD) count++;
		}
		if (!iod && count>1) return GF_NOT_SUPPORTED;
	}

	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		u32 nbb;
		Bool delete_esd = GF_FALSE;
		GF_ESD *esd = NULL;
		GF_StreamContext *sc = gf_list_get(ctx->streams, i);

		if (sc->streamType != GF_STREAM_SCENE) continue;

		if (iod) {
			for (j=0; j<gf_list_count(iod->ESDescriptors); j++) {
				esd = gf_list_get(iod->ESDescriptors, j);
				if (esd->decoderConfig && esd->decoderConfig->streamType == GF_STREAM_SCENE) {
					if (!sc->ESID) sc->ESID = esd->ESID;
					if (sc->ESID == esd->ESID) {
						break;
					}
				}
				/*special BIFS direct import from NHNT*/
				else if (gf_list_count(iod->ESDescriptors)==1) {
					sc->ESID = esd->ESID;
					break;
				}
				esd = NULL;
			}
		}

		if (!esd) {
			esd = gf_odf_desc_esd_new(2);
			if (!esd) return GF_OUT_OF_MEM;
			gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = NULL;
			esd->ESID = sc->ESID;
			esd->decoderConfig->streamType = GF_STREAM_SCENE;
			delete_esd = GF_TRUE;
		}
		if (!esd->decoderConfig) return GF_OUT_OF_MEM;

		/*should NOT happen (means inputctx is not properly setup)*/
		if (!esd->decoderConfig->decoderSpecificInfo) {
			bcfg = (GF_BIFSConfig*)gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
			delete_bcfg = 1;
		}
		/*regular retrieve from ctx*/
		else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_BIFS_CFG_TAG) {
			bcfg = (GF_BIFSConfig *)esd->decoderConfig->decoderSpecificInfo;
			delete_bcfg = 0;
		}
		/*should not happen either (unless loading from MP4 in which case BIFSc is not decoded)*/
		else {
			bcfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
			delete_bcfg = 1;
		}
		/*NO CHANGE TO BIFSC otherwise the generated update will not match the input context*/
		nbb = GetNbBits(ctx->max_node_id);
		if (!bcfg->nodeIDbits) bcfg->nodeIDbits=nbb;
		if (bcfg->nodeIDbits<nbb) fprintf(stderr, "Warning: BIFSConfig.NodeIDBits TOO SMALL\n");

		nbb = GetNbBits(ctx->max_route_id);
		if (!bcfg->routeIDbits) bcfg->routeIDbits = nbb;
		if (bcfg->routeIDbits<nbb) fprintf(stderr, "Warning: BIFSConfig.RouteIDBits TOO SMALL\n");

		nbb = GetNbBits(ctx->max_proto_id);
		if (!bcfg->protoIDbits) bcfg->protoIDbits=nbb;
		if (bcfg->protoIDbits<nbb) fprintf(stderr, "Warning: BIFSConfig.ProtoIDBits TOO SMALL\n");

		/*this is the real pb, not stored in cfg or file level, set at EACH replaceScene*/
		encode_names = 0;

		/* The BIFS Config that is passed here should be the BIFSConfig from the IOD */
		gf_bifs_encoder_new_stream(bifsenc, sc->ESID, bcfg, encode_names, 0);
		if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);

		if (!esd->slConfig) esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		if (sc->timeScale) esd->slConfig->timestampResolution = sc->timeScale;
		if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = 1000;
		esd->ESID = sc->ESID;
		gf_bifs_encoder_get_config(bifsenc, sc->ESID, &data, &data_len);

		if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
		esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		esd->decoderConfig->decoderSpecificInfo->data = data;
		esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
		esd->decoderConfig->objectTypeIndication = gf_bifs_encoder_get_version(bifsenc, sc->ESID);

		for (j=1; j<gf_list_count(sc->AUs); j++) {
			u8 *data;
			u32 data_len;
			au = gf_list_get(sc->AUs, j);
			e = gf_bifs_encode_au(bifsenc, sc->ESID, au->commands, &data, &data_len);
			if (data) {
				sprintf(szName, "%s-%02d-%02d.bifs", szRad, sc->ESID, j);
				f = gf_fopen(szName, "wb");
				gf_fwrite(data, data_len, 1, f);
				gf_fclose(f);
				gf_free(data);
			}
		}
		if (delete_esd) gf_odf_desc_del((GF_Descriptor*)esd);
	}
	gf_bifs_encoder_del(bifsenc);
	return e;
}
#endif /*GPAC_DISABLE_SMGR*/


#endif /*GPAC_DISABLE_BIFS_ENC*/

/**
 * \param chunkFile BT chunk to be encoded
 * \param bifs output file name for the BIFS data
 * \param inputContext initial BT upon which the chunk is based (shall not be NULL)
 * \param outputContext: file name to dump the context after applying the new chunk to the input context
                   can be NULL, without .bt
 * \param tmpdir can be NULL
 */
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext, const char *tmpdir)
{
#if defined(GPAC_DISABLE_SMGR) || defined(GPAC_DISABLE_BIFS_ENC) || defined(GPAC_DISABLE_SCENE_ENCODER) || defined (GPAC_DISABLE_SCENE_DUMP)
	fprintf(stderr, "BIFS encoding is not supported in this build of GPAC\n");
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	GF_SceneGraph *sg;
	GF_SceneManager	*ctx;
	GF_SceneLoader load;

	/*Step 1: create context and load input*/
	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = inputContext;
	load.ctx = ctx;
	/*since we're encoding we must get MPEG4 nodes only*/
	load.flags = GF_SM_LOAD_MPEG4_STRICT;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e) {
		fprintf(stderr, "Cannot load context %s: %s\n", inputContext, gf_error_to_string(e));
		goto exit;
	}

	/* Step 2: make sure we have only ONE RAP for each stream*/
	e = gf_sm_aggregate(ctx, 0);
	if (e) goto exit;

	/*Step 3: loading the chunk into the context*/
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = chunkFile;
	load.ctx = ctx;
	load.flags = GF_SM_LOAD_MPEG4_STRICT | GF_SM_LOAD_CONTEXT_READY;
	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (e) {
		fprintf(stderr, "Cannot load scene commands chunk %s: %s\n", chunkFile, gf_error_to_string(e));
		goto exit;
	}
	fprintf(stderr, "Context and chunks loaded\n");

	/* Assumes that the first AU contains only one command a SceneReplace and
	   that is not part of the current chunk */
	/* Last argument is a callback to pass the encoded AUs: not needed here
	   Saving is not handled correctly */
	e = EncodeBIFSChunk(ctx, bifs, NULL);
	if (e) goto exit;


	if (outputContext) {
		u32 d_mode, do_enc;
		char szF[GF_MAX_PATH], *ext;

		/*make random access for storage*/
		e = gf_sm_aggregate(ctx, 0);
		if (e) goto exit;

		/*check if we dump to BT, XMT or encode to MP4*/
		strcpy(szF, outputContext);
		ext = strrchr(szF, '.');
		d_mode = GF_SM_DUMP_BT;
		do_enc = 0;
		if (ext) {
			if (!stricmp(ext, ".xmt") || !stricmp(ext, ".xmta")) d_mode = GF_SM_DUMP_XMTA;
			else if (!stricmp(ext, ".mp4")) do_enc = 1;
			ext[0] = 0;
		}

		if (do_enc) {
			GF_ISOFile *mp4;
			strcat(szF, ".mp4");
			mp4 = gf_isom_open(szF, GF_ISOM_WRITE_EDIT, tmpdir);
			e = gf_sm_encode_to_file(ctx, mp4, NULL);
			if (e) gf_isom_delete(mp4);
			else gf_isom_close(mp4);
		}
		else e = gf_sm_dump(ctx, szF, GF_FALSE, d_mode);
	}

exit:
	if (ctx) {
		sg = ctx->scene_graph;
		gf_sm_del(ctx);
		gf_sg_del(sg);
	}

	return e;

#endif /*defined(GPAC_DISABLE_BIFS_ENC) || defined(GPAC_DISABLE_SCENE_ENCODER) || defined (GPAC_DISABLE_SCENE_DUMP)*/

}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


#ifndef GPAC_DISABLE_CORE_TOOLS
void sax_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	char szCheck[100];
	GF_List *imports = sax_cbck;
	GF_XMLAttribute *att;
	u32 i=0;

	/*do not process hyperlinks*/
	if (!strcmp(node_name, "a") || !strcmp(node_name, "Anchor")) return;

	for (i=0; i<nb_attributes; i++) {
		att = (GF_XMLAttribute *) &attributes[i];
		if (stricmp(att->name, "xlink:href") && stricmp(att->name, "url")) continue;
		if (att->value[0]=='#') continue;
		if (!strnicmp(att->value, "od:", 3)) continue;
		sprintf(szCheck, "%d", atoi(att->value));
		if (!strcmp(szCheck, att->value)) continue;
		gf_list_add(imports, gf_strdup(att->value) );
	}
}

static Bool wgt_enum_files(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	WGTEnum *wgt = (WGTEnum *)cbck;

	if (!strcmp(wgt->root_file, file_path)) return 0;
	//remove hidden files
	if (file_path[0] == '.') return 0;
	gf_list_add(wgt->imports, gf_strdup(file_path) );
	return 0;
}
static Bool wgt_enum_dir(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	if (!stricmp(file_name, "cvs") || !stricmp(file_name, ".svn") || !stricmp(file_name, ".git")) return 0;
	gf_enum_directory(file_path, 0, wgt_enum_files, cbck, NULL);
	return gf_enum_directory(file_path, 1, wgt_enum_dir, cbck, NULL);
}

GF_ISOFile *package_file(char *file_name, char *fcc, const char *tmpdir, Bool make_wgt)
{
	GF_ISOFile *file = NULL;
	GF_Err e;
	GF_SAXParser *sax;
	GF_List *imports;
	Bool ascii;
	char root_dir[GF_MAX_PATH];
	char *isom_src = NULL;
	u32 i, count, mtype, skip_chars;
	char *type;

	type = gf_xml_get_root_type(file_name, &e);
	if (!type) {
		fprintf(stderr, "Cannot process XML file %s: %s\n", file_name, gf_error_to_string(e) );
		return NULL;
	}
	if (make_wgt) {
		if (strcmp(type, "widget")) {
			fprintf(stderr, "XML Root type %s differs from \"widget\" \n", type);
			gf_free(type);
			return NULL;
		}
		gf_free(type);
		type = gf_strdup("application/mw-manifest+xml");
		fcc = "mwgt";
	}
	imports = gf_list_new();


	root_dir[0] = 0;
	if (make_wgt) {
		WGTEnum wgt;
		char *sep = strrchr(file_name, '\\');
		if (!sep) sep = strrchr(file_name, '/');
		if (sep) {
			char c = sep[1];
			sep[1]=0;
			strcpy(root_dir, file_name);
			sep[1] = c;
		} else {
			strcpy(root_dir, "./");
		}
		wgt.dir = root_dir;
		wgt.root_file = file_name;
		wgt.imports = imports;
		gf_enum_directory(wgt.dir, 0, wgt_enum_files, &wgt, NULL);
		gf_enum_directory(wgt.dir, 1, wgt_enum_dir, &wgt, NULL);
		ascii = 1;
	} else {
		sax = gf_xml_sax_new(sax_node_start, NULL, NULL, imports);
		e = gf_xml_sax_parse_file(sax, file_name, NULL);
		ascii = !gf_xml_sax_binary_file(sax);
		gf_xml_sax_del(sax);
		if (e<0) goto exit;
		e = GF_OK;
	}

	if (fcc) {
		mtype = GF_4CC(fcc[0],fcc[1],fcc[2],fcc[3]);
	} else {
		mtype = 0;
		if (!stricmp(type, "svg")) mtype = ascii ? GF_META_TYPE_SVG : GF_META_TYPE_SVGZ;
		else if (!stricmp(type, "smil")) mtype = ascii ? GF_META_TYPE_SMIL : GF_META_TYPE_SMLZ;
		else if (!stricmp(type, "x3d")) mtype = ascii ? GF_META_TYPE_X3D  : GF_META_TYPE_X3DZ  ;
		else if (!stricmp(type, "xmt-a")) mtype = ascii ? GF_META_TYPE_XMTA : GF_META_TYPE_XMTZ;
	}
	if (!mtype) {
		fprintf(stderr, "Missing 4CC code for meta name - please use ABCD:fileName\n");
		e = GF_BAD_PARAM;
		goto exit;
	}


	if (!make_wgt) {
		count = gf_list_count(imports);
		for (i=0; i<count; i++) {
			char *item = gf_list_get(imports, i);

			FILE *test = gf_fopen(item, "rb");
			if (!test) {
				char *resurl = gf_url_concatenate(file_name, item);
				test = gf_fopen(resurl, "rb");
				gf_free(resurl);
			}

			if (!test) {
				gf_list_rem(imports, i);
				i--;
				count--;
				gf_free(item);
				continue;
			}
			gf_fclose(test);
			if (gf_isom_probe_file(item)) {
				if (isom_src) {
					fprintf(stderr, "Cannot package several IsoMedia files together\n");
					e = GF_NOT_SUPPORTED;
					goto exit;
				}
				gf_list_rem(imports, i);
				i--;
				count--;
				isom_src = item;
				continue;
			}
		}
	}

	if (isom_src) {
		file = gf_isom_open(isom_src, GF_ISOM_OPEN_EDIT, tmpdir);
	} else {
		file = gf_isom_open("package", GF_ISOM_WRITE_EDIT, tmpdir);
	}

	e = gf_isom_set_meta_type(file, 1, 0, mtype);
	if (e) goto exit;
	/*add self ref*/
	if (isom_src) {
		e = gf_isom_add_meta_item(file, 1, 0, 1, NULL, isom_src, 0, 0, NULL, NULL, NULL,  NULL, NULL);
		if (e) goto exit;
	}
	e = gf_isom_set_meta_xml(file, 1, 0, file_name, !ascii);
	if (e) goto exit;

	skip_chars = (u32) strlen(root_dir);
	count = gf_list_count(imports);
	for (i=0; i<count; i++) {
		char *ext, *mime, *encoding, *name = NULL, *itemurl;
		char *item = gf_list_get(imports, i);

		name = gf_strdup(item + skip_chars);

		if (make_wgt) {
			char *sep;
			while (1) {
				sep = strchr(name, '\\');
				if (!sep) break;
				sep[0] = '/';
			}
		}


		mime = encoding = NULL;
		ext = strrchr(item, '.');
		if (!stricmp(ext, ".gz")) ext = strrchr(ext-1, '.');

		if (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg")) mime = "image/jpeg";
		else if (!stricmp(ext, ".png")) mime = "image/png";
		else if (!stricmp(ext, ".svg")) mime = "image/svg+xml";
		else if (!stricmp(ext, ".x3d")) mime = "model/x3d+xml";
		else if (!stricmp(ext, ".xmt")) mime = "application/x-xmt";
		else if (!stricmp(ext, ".js")) {
			mime = "application/javascript";
		}
		else if (!stricmp(ext, ".svgz") || !stricmp(ext, ".svg.gz")) {
			mime = "image/svg+xml";
			encoding = "binary-gzip";
		}
		else if (!stricmp(ext, ".x3dz") || !stricmp(ext, ".x3d.gz")) {
			mime = "model/x3d+xml";
			encoding = "binary-gzip";
		}
		else if (!stricmp(ext, ".xmtz") || !stricmp(ext, ".xmt.gz")) {
			mime = "application/x-xmt";
			encoding = "binary-gzip";
		}

		itemurl = gf_url_concatenate(file_name, item);

		e = gf_isom_add_meta_item(file, 1, 0, 0, itemurl ? itemurl : item, name, 0, GF_META_ITEM_TYPE_MIME, mime, encoding, NULL,  NULL, NULL);
		gf_free(name);
		gf_free(itemurl);
		if (e) goto exit;
	}

exit:
	while (gf_list_count(imports)) {
		char *item = gf_list_last(imports);
		gf_list_rem_last(imports);
		gf_free(item);
	}
	gf_list_del(imports);
	if (isom_src) gf_free(isom_src);
	if (type) gf_free(type);
	if (e) {
		if (file) gf_isom_delete(file);
		return NULL;
	}
	return file;
}

GF_Err parse_high_dynamc_range_xml_desc(GF_ISOFile *movie, char *file_name)
{
	GF_DOMParser *parser;
	GF_XMLNode *root, *stream;
	GF_Err e;
	u32 i;
	GF_MasteringDisplayColourVolumeBox mdcv;
	GF_ContentLightLevelBox clli;

	memset(&mdcv, 0, sizeof(GF_MasteringDisplayColourVolumeBox));
	memset(&clli, 0, sizeof(GF_ContentLightLevelBox));

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, file_name, NULL, NULL);
	if (e) {
		fprintf(stderr, "Error parsing HDR XML file: Line %d - %s. Abort.\n", gf_xml_dom_get_line(parser), gf_xml_dom_get_error(parser));
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);
	if (!root) {
		fprintf(stderr, "Error parsing HDR XML file: no \"root\" found. Abort.\n");
		gf_xml_dom_del(parser);
		return e;
	}
	if (strcmp(root->name, "HDR")) {
		fprintf(stderr, "Error parsing HDR XML file: root name is \"%s\", expecting \"HDR\"\n", root->name);
		gf_xml_dom_del(parser);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	i = 0;
	while ((stream = gf_list_enum(root->content, &i))) {
		u32 id = 0, j;
		GF_XMLAttribute* att = NULL;
		GF_XMLNode *box = NULL;

		if (stream->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(stream->name, "Track")) continue;

		j = 0;
		while ((att = gf_list_enum(stream->attributes, &j))) {
			if (!strcmp(att->name, "id")) id = atoi(att->value);
			else fprintf(stderr, "HDR XML: ignoring track attribute \"%s\"\n", att->name);
		}

		j = 0;
		while ((box = gf_list_enum(stream->content, &j))) {
			u32 k;

			if (box->type != GF_XML_NODE_TYPE) continue;

			if (!strcmp(box->name, "mdcv")) {
				k = 0;
				while ((att = gf_list_enum(box->attributes, &k))) {
					if (!strcmp(att->name, "display_primaries_0_x")) mdcv.display_primaries[0].x = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_0_y")) mdcv.display_primaries[0].y = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_1_x")) mdcv.display_primaries[1].x = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_1_y")) mdcv.display_primaries[1].y = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_2_x")) mdcv.display_primaries[2].x = atoi(att->value);
					else if (!strcmp(att->name, "display_primaries_2_y")) mdcv.display_primaries[2].y = atoi(att->value);
					else if (!strcmp(att->name, "white_point_x")) mdcv.white_point_x = atoi(att->value);
					else if (!strcmp(att->name, "white_point_y")) mdcv.white_point_y = atoi(att->value);
					else if (!strcmp(att->name, "max_display_mastering_luminance")) mdcv.max_display_mastering_luminance = atoi(att->value);
					else if (!strcmp(att->name, "min_display_mastering_luminance")) mdcv.min_display_mastering_luminance = atoi(att->value);
					else fprintf(stderr, "HDR XML: ignoring box \"%s\" attribute \"%s\"\n", box->name, att->name);
				}
			} else if (!strcmp(box->name, "clli")) {
				k = 0;
				while ((att = gf_list_enum(box->attributes, &k))) {
					if (!strcmp(att->name, "max_content_light_level")) clli.max_content_light_level = atoi(att->value);
					else if (!strcmp(att->name, "max_pic_average_light_level")) clli.max_pic_average_light_level = atoi(att->value);
					else fprintf(stderr, "HDR XML: ignoring box \"%s\" attribute \"%s\"\n", box->name, att->name);
				}
			} else {
				fprintf(stderr, "HDR XML: ignoring box element \"%s\"\n", box->name);
				continue;
			}
		}

		e = gf_isom_set_high_dynamic_range_info(movie, id, 1, &mdcv, &clli);
		if (e) {
			fprintf(stderr, "HDR XML: error in gf_isom_set_high_dynamic_range_info()\n");
			break;
		}
	}

	gf_xml_dom_del(parser);
	return e;
}

#else
GF_ISOFile *package_file(char *file_name, char *fcc, const char *tmpdir, Bool make_wgt)
{
	fprintf(stderr, "XML Not supported in this build of GPAC - cannot package file\n");
	return NULL;
}

GF_Err parse_high_dynamc_range_xml_desc(GF_ISOFile* movie, char* file_name)
{
	fprintf(stderr, "XML Not supported in this build of GPAC - cannot process HDR parameter file\n");
	return GF_OK;
}
#endif //#ifndef GPAC_DISABLE_CORE_TOOLS


#endif /*GPAC_DISABLE_ISOM_WRITE*/

