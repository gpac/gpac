/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#ifndef _MP4BOX_H
#define _MP4BOX_H

#include <gpac/constants.h>
#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/isomedia.h>
#include <gpac/utf.h>
#include <gpac/filters.h>

#ifndef GPAC_DISABLE_SMGR
#include <gpac/scene_manager.h>
#endif

#ifndef GPAC_DISABLE_LOG
#define M4_LOG(_a, _b)	GF_LOG(_a, GF_LOG_APP, _b)
#else
void mp4box_log(const char *fmt, ...);

#define M4_LOG(_a, _b) mp4box_log _b
#endif

u32 parse_u32(char *val, char *log_name);
s32 parse_s32(char *val, char *log_name);

typedef struct
{
	u32 ID_or_num;
	//0: regular trackID, 1: track number, 2: video(N), 3: audio(N), 4: text(N)
	u8 type;
} TrackIdentifier;


typedef enum {
	GF_FILE_TYPE_NOT_SUPPORTED	= 0,
	GF_FILE_TYPE_ISO_MEDIA		= 1,
	GF_FILE_TYPE_BT_WRL_X3DV	= 2,
	GF_FILE_TYPE_XMT_X3D		= 3,
	GF_FILE_TYPE_SVG			= 4,
	GF_FILE_TYPE_SWF			= 5,
	GF_FILE_TYPE_LSR_SAF		= 6,
} GF_FileType;


#ifndef GPAC_DISABLE_MEDIA_IMPORT
GF_Err convert_file_info(char *inName, TrackIdentifier *track_id);
#endif

Bool mp4box_check_isom_fileopt(char *opt);


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err apply_high_dynamc_range_xml_desc(GF_ISOFile* movie, u32 track, char* file_name);

#ifndef GPAC_DISABLE_MEDIA_IMPORT
GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, GF_FilterSession *fsess, char **mux_args_if_first_pass, char **mux_sid_if_first_pass, u32 tk_idx);
#else
static GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, GF_FilterSession *fsess, char **mux_args_if_first_pass, char **mux_sid_if_first_pass, u32 tk_idx) {
	return GF_NOT_SUPPORTED;
}
#endif
GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u64 split_size_kb, char *inName, Double interleaving_time, Double chunk_start, u32 adjust_split_end, char *outName, Bool force_rap_split, const char *split_range_str, u32 fs_dump_flags);
GF_Err cat_isomedia_file(GF_ISOFile *mp4, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command, Bool is_pl);

GF_Err apply_edits(GF_ISOFile *dest, u32 track, char *edits);

#if !defined(GPAC_DISABLE_SCENE_ENCODER)
GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs);
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext);
#endif

GF_ISOFile *package_file(char *file_name, char *fcc, Bool make_wgt);

#endif

GF_Err dump_isom_cover_art(GF_ISOFile *file, char *inName, Bool is_final_name);
GF_Err dump_isom_chapters(GF_ISOFile *file, char *inName, Bool is_final_name, Bool dump_ogg);
GF_Err dump_isom_udta(GF_ISOFile *file, char *inName, Bool is_final_name, u32 dump_udta_type, u32 dump_udta_track);

GF_Err set_file_udta(GF_ISOFile *dest, u32 tracknum, u32 udta_type, char *src, Bool is_box_array, Bool is_string);
u32 id3_get_genre_tag(const char *name);

/*in filedump.c*/
#ifndef GPAC_DISABLE_SCENE_DUMP
GF_Err dump_isom_scene(char *file, char *inName, Bool is_final_name, GF_SceneDumpFormat dump_mode, Bool do_log, Bool no_odf_conv);
//void gf_check_isom_files(char *conf_rules, char *inName);
#endif
#ifndef GPAC_DISABLE_SCENE_STATS
void dump_isom_scene_stats(char *file, char *inName, Bool is_final_name, u32 stat_level);
#endif
u32 PrintNode(const char *name, u32 graph_type);
u32 PrintBuiltInNodes(char *arg_val, u32 dump_type);
u32 PrintBuiltInBoxes(char *arg_val, u32 do_cov);

#ifndef GPAC_DISABLE_ISOM_DUMP
GF_Err dump_isom_xml(GF_ISOFile *file, char *inName, Bool is_final_name, Bool do_track_dump, Bool merge_vtt_cues, Bool skip_init, Bool skip_samples);
#endif


#ifndef GPAC_DISABLE_ISOM_HINTING
#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_isom_rtp(GF_ISOFile *file, char *inName, Bool is_final_name);
#endif
void dump_isom_sdp(GF_ISOFile *file, char *inName, Bool is_final_name);
#endif

void dump_isom_timestamps(GF_ISOFile *file, char *inName, Bool is_final_name, Bool skip_offset);
GF_Err dump_isom_nal(GF_ISOFile *file, GF_ISOTrackID trackID, char *inName, Bool is_final_name, u32 dump_flags);
void dump_isom_saps(GF_ISOFile *file, GF_ISOTrackID trackID, u32 dump_saps_mode, char *inName, Bool is_final_name);

void dump_isom_chunks(GF_ISOFile *file, char *inName, Bool is_final_name);

#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_isom_ismacryp(GF_ISOFile *file, char *inName, Bool is_final_name);
void dump_isom_timed_text(GF_ISOFile *file, GF_ISOTrackID trackID, char *inName, Bool is_final_name, Bool is_convert, GF_TextDumpType dump_type);
#endif /*GPAC_DISABLE_ISOM_DUMP*/


void DumpTrackInfo(GF_ISOFile *file, GF_ISOTrackID trackID, Bool full_dump, Bool is_track_num, Bool dump_m4sys);
void DumpMovieInfo(GF_ISOFile *file, Bool full_dump);
u32 PrintLanguages(char *argv, u32 opt);

#ifndef GPAC_DISABLE_MPEG2TS
void dump_mpeg2_ts(char *mpeg2ts_file, char *pes_out_name, Bool prog_num);
#endif


#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
void PrintStreamerUsage();
int stream_file_rtp(int argc, char **argv);
int live_session(int argc, char **argv);
void PrintLiveUsage();
#endif

#if !defined(GPAC_DISABLE_STREAMING)
u32 grab_live_m2ts(const char *grab_m2ts, const char *outName);
#endif

GF_Err rip_mpd(const char *mpd, const char *dst_file);

GF_Err cat_playlist(GF_ISOFile *dest, char *playlistName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);


u32 parse_track_dump(char *arg, u32 dump_type);
u32 parse_track_action(char *arg, u32 act_type);
u32 parse_sdp_ext(char *arg_val, u32 param);
u32 parse_help(char *arg_val, u32 opt);
u32 parse_gendoc(char *name, u32 opt);
u32 parse_comp_box(char *arg_val, u32 opt);
u32 parse_dnal(char *arg_val, u32 opt);
u32 parse_dsap(char *arg_val, u32 opt);
u32 parse_bs_switch(char *arg_val, u32 opt);
u32 parse_cp_loc(char *arg_val, u32 opt);
u32 parse_pssh(char *arg_val, u32 opt);
u32 parse_sdtp(char *arg_val, u32 opt);
u32 parse_dash_profile(char *arg_val, u32 opt);
u32 parse_rap_ref(char *arg_val, u32 opt);
u32 parse_store_mode(char *arg_val, u32 opt);
u32 parse_base_url(char *arg_val, u32 opt);
u32 parse_multi_rtp(char *arg_val, u32 opt);
u32 parse_senc_param(char *arg_val, u32 opt);
u32 parse_cryp(char *arg_val, u32 opt);
u32 parse_fps(char *arg_val, u32 opt);
u32 parse_split(char *arg_val, u32 opt);
u32 parse_brand(char *b, u32 opt);
u32 parse_mpegu(char *arg_val, u32 opt);
u32 parse_file_info(char *arg_val, u32 opt);
u32 parse_boxpatch(char *arg_val, u32 opt);
u32 parse_aviraw(char *arg_val, u32 opt);
u32 parse_dump_udta(char *code, u32 opt);
u32 parse_dump_ts(char *arg_val, u32 opt);
u32 parse_ttxt(char *arg_val, u32 opt);
u32 parse_dashlive(char *arg, char *arg_val, u32 opt);
u32 parse_compress(char *arg_val, u32 opt);

#endif // _MP4BOX_H

