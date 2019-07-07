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

#ifndef _MP4BOX_H
#define _MP4BOX_H

#include <gpac/constants.h>
#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/isomedia.h>
#include <gpac/utf.h>

#ifndef GPAC_DISABLE_SMGR
#include <gpac/scene_manager.h>
#endif


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
void convert_file_info(char *inName, GF_ISOTrackID trackID);
#endif

GF_Err parse_high_dynamc_range_xml_desc(GF_ISOFile* movie, char* file_name);

#ifndef GPAC_DISABLE_ISOM_WRITE

#ifndef GPAC_DISABLE_MEDIA_IMPORT
GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample);
#else
GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample) {
	return GF_NOT_SUPPORTED;
}
#endif
GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u64 split_size_kb, char *inName, Double interleaving_time, Double chunk_start, Bool adjust_split_end, char *outName, const char *tmpdir, Bool force_rap_split);
GF_Err cat_isomedia_file(GF_ISOFile *mp4, char *fileName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command, Bool is_pl);

#if !defined(GPAC_DISABLE_SCENE_ENCODER)
GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs);
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext, const char *tmpdir);
#endif

GF_ISOFile *package_file(char *file_name, char *fcc, const char *tmpdir, Bool make_wgt);

#endif

GF_Err dump_isom_cover_art(GF_ISOFile *file, char *inName, Bool is_final_name);
GF_Err dump_isom_chapters(GF_ISOFile *file, char *inName, Bool is_final_name, Bool dump_ogg);
GF_Err dump_isom_udta(GF_ISOFile *file, char *inName, Bool is_final_name, u32 dump_udta_type, u32 dump_udta_track);

GF_Err set_file_udta(GF_ISOFile *dest, u32 tracknum, u32 udta_type, char *src, Bool is_box_array);
u32 id3_get_genre_tag(const char *name);

/*in filedump.c*/
#ifndef GPAC_DISABLE_SCENE_DUMP
GF_Err dump_isom_scene(char *file, char *inName, Bool is_final_name, GF_SceneDumpFormat dump_mode, Bool do_log);
//void gf_check_isom_files(char *conf_rules, char *inName);
#endif
#ifndef GPAC_DISABLE_SCENE_STATS
void dump_isom_scene_stats(char *file, char *inName, Bool is_final_name, u32 stat_level);
#endif
void PrintNode(const char *name, u32 graph_type);
void PrintBuiltInNodes(u32 graph_type, Bool dump_all);
void PrintBuiltInBoxes(Bool do_cov);

#ifndef GPAC_DISABLE_ISOM_DUMP
GF_Err dump_isom_xml(GF_ISOFile *file, char *inName, Bool is_final_name, Bool do_track_dump, Bool merge_vtt_cues, Bool skip_init);
#endif


#ifndef GPAC_DISABLE_ISOM_HINTING
#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_isom_rtp(GF_ISOFile *file, char *inName, Bool is_final_name);
#endif
void dump_isom_sdp(GF_ISOFile *file, char *inName, Bool is_final_name);
#endif

void dump_isom_timestamps(GF_ISOFile *file, char *inName, Bool is_final_name, Bool skip_offset);
void dump_isom_nal(GF_ISOFile *file, GF_ISOTrackID trackID, char *inName, Bool is_final_name, Bool dump_crc);
void dump_isom_saps(GF_ISOFile *file, GF_ISOTrackID trackID, u32 dump_saps_mode, char *inName, Bool is_final_name);

#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_isom_ismacryp(GF_ISOFile *file, char *inName, Bool is_final_name);
void dump_isom_timed_text(GF_ISOFile *file, GF_ISOTrackID trackID, char *inName, Bool is_final_name, Bool is_convert, GF_TextDumpType dump_type);
#endif /*GPAC_DISABLE_ISOM_DUMP*/


void DumpTrackInfo(GF_ISOFile *file, GF_ISOTrackID trackID, Bool full_dump);
void DumpMovieInfo(GF_ISOFile *file);
void PrintLanguages();

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

u32 grab_atsc3_session(const char *dir, s32 serviceID, s32 max_segs, u32 stats_rate, u32 debug_tsi);

GF_Err rip_mpd(const char *mpd, const char *dst_file);

GF_Err cat_playlist(GF_ISOFile *dest, char *playlistName, u32 import_flags, GF_Fraction force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);

#endif // _MP4BOX_H

