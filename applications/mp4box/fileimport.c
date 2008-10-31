/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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


#include <gpac/scene_manager.h>
#include <gpac/bifs.h>
#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/scenegraph.h>


#ifndef GPAC_READ_ONLY

extern u32 swf_flags;
extern Float swf_flatten_angle;

const char *GetLanguageCode(char *lang);
void scene_coding_log(void *cbk, u32 log_level, u32 log_tool, const char *fmt, va_list vlist);

void convert_file_info(char *inName, u32 trackID)
{
	GF_Err e;
	u32 i;
	Bool found;
	GF_MediaImporter import;
	memset(&import, 0, sizeof(GF_MediaImporter));
	import.trackID = trackID;
	import.in_name = inName;
	import.flags = GF_IMPORT_PROBE_ONLY;
	e = gf_media_import(&import);
	if (e) {
		fprintf(stdout, "Error probing file %s: %s\n", inName, gf_error_to_string(e));
		return;
	}
	if (trackID) {
		fprintf(stdout, "Import probing results for track %s#%d:\n", inName, trackID);
	} else {
		fprintf(stdout, "Import probing results for %s:\n", inName);
		if (!import.nb_tracks) {
			fprintf(stdout, "File has no selectable tracks\n");
			return;
		}
		fprintf(stdout, "File has %d tracks\n", import.nb_tracks);
	}
	found = 0;
	for (i=0; i<import.nb_tracks; i++) {
		if (trackID && (trackID != import.tk_info[i].track_num)) continue;
		if (!trackID) fprintf(stdout, "\tTrack %d type: ", import.tk_info[i].track_num);
		else fprintf(stdout, "Track type: ");

		switch (import.tk_info[i].type) {
		case GF_ISOM_MEDIA_VISUAL: fprintf(stdout, "Video (%s)", gf_4cc_to_str(import.tk_info[i].media_type)); break;
		case GF_ISOM_MEDIA_AUDIO: fprintf(stdout, "Audio (%s)", gf_4cc_to_str(import.tk_info[i].media_type)); break;
		case GF_ISOM_MEDIA_TEXT: fprintf(stdout, "Text (%s)", gf_4cc_to_str(import.tk_info[i].media_type)); break;
		case GF_ISOM_MEDIA_SCENE: fprintf(stdout, "Scene (%s)", gf_4cc_to_str(import.tk_info[i].media_type)); break;
		case GF_ISOM_MEDIA_OD: fprintf(stdout, "ObjectDescriptor (%s)", gf_4cc_to_str(import.tk_info[i].media_type)); break;
		default: fprintf(stdout, "Other (4CC: %s)", gf_4cc_to_str(import.tk_info[i].type)); break;
		}

		if (import.tk_info[i].lang) fprintf(stdout, " - lang %s", gf_4cc_to_str(import.tk_info[i].lang));

		if (import.tk_info[i].prog_num) {
			if (!import.nb_progs) {
				fprintf(stdout, " - Program %d", import.tk_info[i].prog_num);
			} else {
				u32 j;
				for (j=0; j<import.nb_progs; j++) {
					if (import.tk_info[i].prog_num != import.pg_info[j].number) continue;
					fprintf(stdout, " - Program %s", import.pg_info[j].name);
					break;
				}
			}
		}
		fprintf(stdout, "\n");
		if (!trackID) continue;

		if ((import.tk_info[i].type==GF_ISOM_MEDIA_VISUAL) 
			&& import.tk_info[i].video_info.width 
			&& import.tk_info[i].video_info.height
			) {
			fprintf(stdout, "Source: %s %dx%d", gf_4cc_to_str(import.tk_info[i].media_type), import.tk_info[i].video_info.width, import.tk_info[i].video_info.height);
			if (import.tk_info[i].video_info.FPS) fprintf(stdout, " @ %g FPS", import.tk_info[i].video_info.FPS);
			if (import.tk_info[i].video_info.par) fprintf(stdout, " PAR: %d:%d", import.tk_info[i].video_info.par >> 16, import.tk_info[i].video_info.par & 0xFFFF);
			fprintf(stdout, "\n");
		}
		else if ((import.tk_info[i].type==GF_ISOM_MEDIA_AUDIO) && import.tk_info[i].audio_info.sample_rate) {
			fprintf(stdout, "Source: %s - SampleRate %d - %d channels\n", gf_4cc_to_str(import.tk_info[i].media_type), import.tk_info[i].audio_info.sample_rate, import.tk_info[i].audio_info.nb_channels);
		} else {
			fprintf(stdout, "Source: %s\n", gf_4cc_to_str(import.tk_info[i].media_type));
		}
			

		fprintf(stdout, "\nImport Capabilities:\n");
		if (import.tk_info[i].flags & GF_IMPORT_USE_DATAREF) fprintf(stdout, "\tCan use data referencing\n");
		if (import.tk_info[i].flags & GF_IMPORT_NO_FRAME_DROP) fprintf(stdout, "\tCan use fixed FPS import\n");
		if (import.tk_info[i].flags & GF_IMPORT_FORCE_PACKED) fprintf(stdout, "\tCan force packed bitstream import\n");
		if (import.tk_info[i].flags & GF_IMPORT_OVERRIDE_FPS) fprintf(stdout, "\tCan override source frame rate\n");
		if (import.tk_info[i].flags & (GF_IMPORT_SBR_IMPLICIT|GF_IMPORT_SBR_EXPLICIT)) fprintf(stdout, "\tCan use AAC-SBR signaling\n");
		if (import.tk_info[i].flags & GF_IMPORT_FORCE_MPEG4) fprintf(stdout, "\tCan force MPEG-4 Systems signaling\n");
		if (import.tk_info[i].flags & GF_IMPORT_3GPP_AGGREGATION) fprintf(stdout, "\tCan use 3GPP frame aggregation\n");
		if (import.tk_info[i].flags & GF_IMPORT_NO_DURATION) fprintf(stdout, "\tCannot use duration-based import\n");

		found = 1;
		break;
	}
	fprintf(stdout, "\n");
	if (!found && trackID) fprintf(stdout, "Cannot find track %d in file\n", trackID);
}

GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, Double force_fps, u32 frames_per_sample)
{
	u32 track_id, i, timescale, track;
	s32 par_d, par_n, prog_id, delay;
	Bool do_audio, do_video, do_all, disable;
	u32 group;
	const char *szLan;
	GF_Err e;
	GF_MediaImporter import;
	char *ext, szName[1000], *handler_name;

	memset(&import, 0, sizeof(GF_MediaImporter));

	strcpy(szName, inName);
	ext = strrchr(inName, '.');
	if (!ext) {
		fprintf(stdout, "Unknown input file type\n");
		return GF_BAD_PARAM;
	}

	disable = 0;
	szLan = NULL;
	delay = 0;
	group = 0;
	par_d = par_n = -2;
	/*use ':' as separator, but beware DOS paths...*/
	ext = strchr(szName, ':');
	if (ext && ext[1]=='\\') ext = strchr(szName+2, ':');

	handler_name = NULL;
	while (ext) {
		char *ext2 = strchr(ext+1, ':');
		if (ext2 && !strncmp(ext2, "://", 3)) ext2 = strchr(ext2+1, ':');
		if (ext2 && !strncmp(ext2, ":\\", 2)) ext2 = strchr(ext2+1, ':');
		if (ext2) ext2[0] = 0;

		/*all extensions for track-based importing*/
		if (!strnicmp(ext+1, "lang=", 5)) szLan = GetLanguageCode(ext+6);
		else if (!strnicmp(ext+1, "delay=", 6)) delay = atoi(ext+7);
		else if (!strnicmp(ext+1, "fps=", 4)) {
			if (!strcmp(ext+5, "auto")) force_fps = 10000.0;
			else force_fps = atof(ext+5);
		}
		else if (!stricmp(ext+1, "dref")) import_flags |= GF_IMPORT_USE_DATAREF;
		else if (!stricmp(ext+1, "nodrop")) import_flags |= GF_IMPORT_NO_FRAME_DROP;
		else if (!stricmp(ext+1, "packed")) import_flags |= GF_IMPORT_FORCE_PACKED;
		else if (!stricmp(ext+1, "sbr")) import_flags |= GF_IMPORT_SBR_IMPLICIT;
		else if (!stricmp(ext+1, "sbrx")) import_flags |= GF_IMPORT_SBR_EXPLICIT;
		else if (!stricmp(ext+1, "mpeg4")) import_flags |= GF_IMPORT_FORCE_MPEG4;
		else if (!strnicmp(ext+1, "agg=", 4)) frames_per_sample = atoi(ext+5);
		else if (!strnicmp(ext+1, "dur=", 4)) import.duration = (u32) (atof(ext+5) * 1000);
		else if (!strnicmp(ext+1, "par=", 4)) {
			if (!stricmp(ext+5, "none")) {
				par_n = par_d = -1;
			} else {
				if (ext2) ext2[0] = ':';
				if (ext2) ext2 = strchr(ext2+1, ':');
				if (ext2) ext2[0] = 0;
				sscanf(ext+5, "%d:%d", &par_n, &par_d);
			}
		}
		else if (!strnicmp(ext+1, "name=", 5)) handler_name = strdup(ext+6);
		else if (!strnicmp(ext+1, "font=", 5)) import.fontName = strdup(ext+6);
		else if (!strnicmp(ext+1, "size=", 5)) import.fontSize = atoi(ext+6);
		else if (!strnicmp(ext+1, "fmt=", 4)) import.streamFormat = strdup(ext+5);
		else if (!strnicmp(ext+1, "disable", 7)) disable = 1;
		else if (!strnicmp(ext+1, "group=", 6)) {
			group = atoi(ext+7);
			if (!group) group = gf_isom_get_next_alternate_group_id(dest);
		}
		

		/*unrecognized, assume name has colon in it*/
		else {
		 ext = ext2;
		 continue;
		}

		if (ext2) ext2[0] = ':';
		ext2 = ext+1;
		ext[0] = 0;
		ext = strchr(ext+1, ':');
	}

	/*check duration import (old syntax)*/
	ext = strrchr(szName, '%');
	if (ext) {
		import.duration = (u32) (atof(ext+1) * 1000);
		ext[0] = 0;
	}
	
	/*select switches for av containers import*/
	do_audio = do_video = 0;
	track_id = prog_id = 0;
	do_all = 1;
	ext = strrchr(szName, '#');
	if (ext) ext[0] = 0;

	import.in_name = szName;
	import.flags = GF_IMPORT_PROBE_ONLY;
	e = gf_media_import(&import);
	if (e) goto exit;

	if (ext) {
		ext++;
		if (!strnicmp(ext, "audio", 5)) do_audio = 1;
		else if (!strnicmp(ext, "video", 5)) do_video = 1;
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
		else track_id = atoi(ext);
	}
	if (do_audio || do_video || track_id) do_all = 0;

	import.dest = dest;
	import.video_fps = force_fps;
	import.frames_per_sample = frames_per_sample;
	import.flags = import_flags;

	if (!import.nb_tracks) {
		u32 count, o_count;
		o_count = gf_isom_get_track_count(import.dest);
		e = gf_media_import(&import);
		if (e) return e;
		count = gf_isom_get_track_count(import.dest);
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
				} else {
					u64 to_skip = (timescale*(-delay))/1000;
					if (to_skip<tk_dur) {
						//u64 seg_dur = (-delay)*gf_isom_get_media_timescale(import.dest, i+1) / 1000;
						gf_isom_append_edit_segment(import.dest, i+1, tk_dur-to_skip, to_skip, GF_ISOM_EDIT_NORMAL);
					} else {
						fprintf(stdout, "Warning: request negative delay longer than track duration - ignoring\n");
					}
				}
			}
			if ((par_n>=0) && (par_d>=0)) {
				e = gf_media_change_par(import.dest, i+1, par_n, par_d);
			}
			if (handler_name) gf_isom_set_handler_name(import.dest, i+1, handler_name);
		}
	} else {
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
			else if (do_audio && (import.tk_info[i].type==GF_ISOM_MEDIA_AUDIO)) {
				do_audio = 0;
				e = gf_media_import(&import);
			}
			else if (do_video && (import.tk_info[i].type==GF_ISOM_MEDIA_VISUAL)) {
				do_video = 0;
				e = gf_media_import(&import);
			}
			else continue;
			if (e) goto exit;

			timescale = gf_isom_get_timescale(dest);
			track = gf_isom_get_track_by_id(import.dest, import.final_trackID);
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
						//u64 seg_dur = (-delay)*gf_isom_get_media_timescale(import.dest, i+1) / 1000;
						gf_isom_append_edit_segment(import.dest, i+1, tk_dur-to_skip, to_skip, GF_ISOM_EDIT_NORMAL);
					} else {
						fprintf(stdout, "Warning: request negative delay longer than track duration - ignoring\n");
					}
				}
			}
			if ((import.tk_info[i].type==GF_ISOM_MEDIA_VISUAL) && (par_n>=-1) && (par_d>=-1)) {
				e = gf_media_change_par(import.dest, track, par_n, par_d);
			}
			if (handler_name) gf_isom_set_handler_name(import.dest, track, handler_name);

			if (group) {
				gf_isom_set_alternate_group_id(import.dest, track, group);
			}
		}
		if (track_id) fprintf(stdout, "WARNING: Track ID %d not found in file\n", track_id);
		else if (do_video) fprintf(stdout, "WARNING: Video track not found\n");
		else if (do_audio) fprintf(stdout, "WARNING: Audio track not found\n");
	}

exit:
	if (handler_name) free(handler_name);
	if (import.fontName) free(import.fontName);
	if (import.streamFormat) free(import.streamFormat);
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
	u32 stop_state;
} TKInfo;

GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u32 split_size_kb, char *inName, Double InterleavingTime, Double chunk_start_time, const char *tmpdir, char *outfile)
{
	u32 i, count, nb_tk, needs_rap_sync, cur_file, conv_type, nb_tk_done, nb_samp, nb_done, di;
	Double max_dur, cur_file_time;
	Bool do_add, all_duplicatable, size_exceeded, chunk_extraction;
	GF_ISOFile *dest;
	GF_ISOSample *samp;
	GF_Err e;
	TKInfo *tks, *tki;
	char *ext, szName[1000], szFile[1000];
	Double chunk_start = (Double) chunk_start_time;

	chunk_extraction = (chunk_start>=0) ? 1 : 0;


	strcpy(szName, inName);
	ext = strrchr(szName, '.');
	if (ext) ext[0] = 0;
	ext = strrchr(inName, '.');
	
	dest = NULL;

	conv_type = 0;
	switch (gf_isom_guess_specification(mp4)) {
	case GF_4CC('I','S','M','A'): conv_type = 1; break;
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
	tks = (TKInfo *)malloc(sizeof(TKInfo)*count);
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
			tks[nb_tk].can_duplicate = 1;
		case GF_ISOM_MEDIA_AUDIO:
			break;
		case GF_ISOM_MEDIA_VISUAL:
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
			fprintf(stdout, "WARNING: Track ID %d (type %s) not handled by spliter - skipping\n", gf_isom_get_track_id(mp4, i+1), gf_4cc_to_str(mtype));
			continue;
		default:
			/*for all other track types, only split if more than one sample*/
			if (gf_isom_get_sample_count(mp4, i+1)==1) {
				fprintf(stdout, "WARNING: Track ID %d (type %s) not handled by spliter - skipping\n", gf_isom_get_track_id(mp4, i+1), gf_4cc_to_str(mtype));
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
				fprintf(stdout, "More than one track has non-sync points - cannot split file\n");
				free(tks);
				return GF_NOT_SUPPORTED;
			}
			needs_rap_sync = nb_tk+1;
		}
		if (!tks[nb_tk].can_duplicate) all_duplicatable = 0;
		nb_tk++;
	}
	if (!nb_tk) {
		fprintf(stdout, "No suitable tracks found for spliting file\n");
		free(tks);
		return GF_NOT_SUPPORTED;
	}
	if (chunk_start>=max_dur) {
		fprintf(stdout, "Input file (%f) shorter than requested split start offset (%f)\n", max_dur, chunk_start);
		free(tks);
		return GF_NOT_SUPPORTED;
	}
	if (max_dur<=split_dur) {
		fprintf(stdout, "Input file (%f) shorter than requested split duration (%f)\n", max_dur, split_dur);
		free(tks);
		return GF_NOT_SUPPORTED;
	}
	if (needs_rap_sync) {
		tki = &tks[needs_rap_sync-1];
		if ((gf_isom_get_sync_point_count(mp4, tki->tk)==1) && (chunk_start != 0.0f)) {
			fprintf(stdout, "Not enough Random Access points in input file - cannot split\n");
			free(tks);
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
				needs_rap_sync = 0;
			} else  {
				e = gf_isom_get_sample_for_media_time(mp4, tki->tk, (u64) (chunk_start*tki->time_scale), &di, GF_ISOM_SEARCH_SYNC_BACKWARD, &samp, &sample_num);
				if (e!=GF_OK) {
					fprintf(stdout, "Cannot locate RAP in track ID %d for chunk extraction from %02.2f sec\n", gf_isom_get_track_id(mp4, tki->tk), chunk_start);
					free(tks);
					return GF_NOT_SUPPORTED;
				}
				start = (Double) (s64) samp->DTS;
				start /= tki->time_scale;
				gf_isom_sample_del(&samp);
				fprintf(stdout, "Adjusting chunk start time to previous random access at %02.2f sec\n", start);
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
		Bool is_last;

		if (chunk_extraction) {
			if (outfile) {
				strcpy(szFile, outfile);
			} else {
				sprintf(szFile, "%s_%d_%d%s", szName, (u32) chunk_start, (u32) (chunk_start+split_dur), ext);
			}
		} else {
			sprintf(szFile, "%s_%03d%s", szName, cur_file+1, ext);
		}
		dest = gf_isom_open(szFile, GF_ISOM_WRITE_EDIT, tmpdir);
		/*clone all tracks*/
		for (i=0; i<nb_tk; i++) {
			tki = &tks[i];
			/*track done - we remove the track from destination, an empty video track could cause pbs to some players*/
			if (tki->stop_state==2) continue;

			e = gf_isom_clone_track(mp4, tki->tk, dest, 0, &tki->dst_tk);
			if (e) {
				fprintf(stdout, "Error cloning track %d\n", tki->tk);
				goto err_exit;
			}
			/*use non-packet CTS offsets (faster add/remove)*/
			if (gf_isom_has_time_offset(mp4, tki->tk)) {
				gf_isom_set_cts_packing(dest, tki->dst_tk, 1);
			}
			gf_isom_remove_edit_segments(dest, tki->dst_tk);
		}
		do_add = 1;
		is_last = 0;
		last_rap_sample_time = 0;
		file_split_dur = split_dur;

		size_exceeded = 0;
		max_dts = 0;
		while (do_add) {
			Double time;
			u32 nb_over;
			/*perfom basic de-interleaving to make sure we're not importing too much of a given track*/
			u32 nb_add = 0;
			/*add one sample of each track*/
			for (i=0; i<nb_tk; i++) {
				Double t;
				u64 dts;
				tki = &tks[i];
				
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
					gf_isom_sample_del(&samp);
					tki->last_sample += 1;
					dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
				}
				dts -= tki->firstDTS;


				t = (Double) (s64) dts;
				t /= tki->time_scale;
				if (tki->first_sample_done) {
					if (t>max_dts) continue;
				} else {
					/*here's the trick: only take care of a/v media for deinterleaving, and ad other media
					only if thir dts is less than the max AV dts found. Otherwise with some text streams we will end up importing
					too much video and corrupting the last sync point indication*/
					if (!tki->can_duplicate && (t>max_dts)) max_dts = t;
					tki->first_sample_done = 1;
				}
				samp = gf_isom_get_sample(mp4, tki->tk, tki->last_sample+1, &di);
				samp->DTS -= tki->firstDTS;

				nb_add += 1;

				if (tki->has_non_raps && samp->IsRAP) {
					GF_ISOSample *next_rap;
					u32 next_rap_num, sdi;
					last_rap_sample_time = (Double) (s64) samp->DTS;
					last_rap_sample_time /= tki->time_scale;
					e = gf_isom_get_sample_for_media_time(mp4, tki->tk, samp->DTS+tki->firstDTS+2, &sdi, GF_ISOM_SEARCH_SYNC_FORWARD, &next_rap, &next_rap_num);
					if (e==GF_EOS) is_last = 1;
					if (next_rap) {
						if (!next_rap->IsRAP) 
							is_last = 1;
						gf_isom_sample_del(&next_rap);
					}
				}
				tki->lastDTS = samp->DTS;
				e = gf_isom_add_sample(dest, tki->dst_tk, di, samp);
				gf_isom_sample_del(&samp);
				tki->last_sample += 1;
				gf_set_progress("Splitting", nb_done, nb_samp);
				nb_done++;
				if (e) {
					fprintf(stdout, "Error cloning track %d sample %d\n", tki->tk, tki->last_sample);
					goto err_exit;
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
					continue;
				}
				time = (Double) (s64) tki->lastDTS;
				time /= tki->time_scale;
				if (size_exceeded || (tki->last_sample==tki->sample_count) || (!tki->can_duplicate && (time>file_split_dur)) ) {
					nb_over++;
					tki->stop_state = 1;
					if (tki->last_sample<tki->sample_count) is_last = 0;
					if ((!tki->can_duplicate || all_duplicatable) && (tki->last_sample==tki->sample_count)) is_last = 1;
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
		}

		/*remove samples - first figure out smallest duration*/
		file_split_dur = (Double) GF_MAX_FLOAT;
		for (i=0; i<nb_tk; i++) {
			Double time;
			tki = &tks[i];
			/*track done*/
			if ((tki->stop_state==2) || (!is_last && (tki->sample_count == tki->last_sample)) ) {
				if (tki->has_non_raps) last_rap_sample_time = 0;
				continue;
			}

			if (tki->lastDTS) {
				time = (Double) (s64) tki->lastDTS;
				time /= tki->time_scale;
				if ((!tki->can_duplicate || all_duplicatable) && time<file_split_dur) file_split_dur = time;
			}
		}
		if (chunk_extraction) file_split_dur = split_dur;

		/*don't split if eq to copy...*/
		if (is_last && !cur_file && !chunk_start) {
			fprintf(stdout, "Cannot split file (Not enough sync samples, duration too large or size too big)\n");
			goto err_exit;
		}


		/*if not last chunk and longer duration adjust to previous RAP point*/
		if ( (size_exceeded || !split_size_kb) && (file_split_dur>split_dur) && !chunk_start) {
			/*if larger than last RAP, rewind till it*/
			if (last_rap_sample_time && (last_rap_sample_time<file_split_dur) ) {
				file_split_dur = last_rap_sample_time;
				is_last = 0;
			}
		}

		nb_tk_done = 0;
		if (!is_last || chunk_extraction) {
			for (i=0; i<nb_tk; i++) {
				Double time = 0;
				u32 last_samp;
				tki = &tks[i];
				while (1) {
					u64 dts;
					last_samp = gf_isom_get_sample_count(dest, tki->dst_tk);
					if (!last_samp) break;

					dts = gf_isom_get_sample_dts(dest, tki->dst_tk, last_samp);
					time = (Double) (s64) dts;
					time /= tki->time_scale;
					/*done*/
					if (tki->last_sample==tki->sample_count) {
						if (!chunk_extraction && !tki->can_duplicate) {
							tki->stop_state=2;
							break;
						}
					}
					if (time /*+ (Double) GF_EPSILON_FLOAT*/ < file_split_dur) break;

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
						tki->last_sample--;
						dts = gf_isom_get_sample_dts(mp4, tki->tk, tki->last_sample+1);
						tki->firstDTS += (u64) (file_split_dur*tki->time_scale);
						gf_isom_set_last_sample_duration(dest, tki->dst_tk, (u32) (tki->firstDTS - dts) );
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
			fprintf(stdout, "Extracting chunk %s - duration %02.2f seconds\n", szFile, file_split_dur);
		} else {
			fprintf(stdout, "Storing split-file %s - duration %02.2f seconds\n", szFile, file_split_dur);
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
				gf_isom_set_cts_packing(dest, tki->dst_tk, 0);
			}
			if (is_last && tki->can_duplicate) {
				gf_isom_set_last_sample_duration(dest, tki->dst_tk, gf_isom_get_sample_duration(mp4, tki->tk, tki->sample_count));
			}

			/*rewrite edit list*/
			new_track_dur = gf_isom_get_track_duration(dest, tki->dst_tk);
			count = gf_isom_get_edit_segment_count(mp4, tki->tk);
			if (count>2) {
				fprintf(stdout, "Warning: %d edit segments - not supported while splitting (max 2) - ignoring extra\n", count);
				count=2;
			}
			for (j=0; j<count; j++) {
				u64 editTime, segDur, MediaTime;
				u8 mode;
			
				gf_isom_get_edit_segment(mp4, tki->tk, j+1, &editTime, &segDur, &MediaTime, &mode);
				if (!j && (mode!=GF_ISOM_EDIT_EMPTY) ) {
					fprintf(stdout, "Warning: Edit list doesn't look like a track delay scheme - ignoring\n");
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
		if (e) fprintf(stdout, "Error storing file %s\n", gf_error_to_string(e));
		if (is_last || chunk_extraction) break;
		cur_file++;
	}
	gf_set_progress("Splitting", nb_samp, nb_samp);
err_exit:
	if (dest) gf_isom_delete(dest);
	free(tks);
	return e;
}

GF_Err cat_multiple_files(GF_ISOFile *dest, char *fileName, u32 import_flags, Double force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat);

GF_Err cat_isomedia_file(GF_ISOFile *dest, char *fileName, u32 import_flags, Double force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat)
{
	u32 i, j, count, nb_tracks, nb_samp, nb_done;
	GF_ISOFile *orig;
	GF_Err e;
	Float ts_scale;
	Double dest_orig_dur;
	u32 dst_tk, tk_id, mtype;
	u64 insert_dts;
	GF_ISOSample *samp;

	if (strchr(fileName, '*')) return cat_multiple_files(dest, fileName, import_flags, force_fps, frames_per_sample, tmp_dir, force_cat);

	e = GF_OK;
	if (!gf_isom_probe_file(fileName)) {
		orig = gf_isom_open("temp", GF_ISOM_WRITE_EDIT, tmp_dir);
		e = import_file(orig, fileName, import_flags, force_fps, frames_per_sample);
		if (e) return e;
	} else {
		orig = gf_isom_open(fileName, GF_ISOM_OPEN_READ, NULL);
	}

	nb_samp = 0;
	nb_tracks = gf_isom_get_track_count(orig);
	for (i=0; i<nb_tracks; i++) {
		u32 mtype = gf_isom_get_media_type(orig, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_FLASH:
			fprintf(stdout, "WARNING: Track ID %d (type %s) not handled by concatenation - removing from destination\n", gf_isom_get_track_id(orig, i+1), gf_4cc_to_str(mtype));
			continue;
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_VISUAL:
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
		fprintf(stdout, "No suitable media tracks to cat in %s - skipping\n", fileName);
		goto err_exit;
	}
	
	dest_orig_dur = (Double) (s64) gf_isom_get_duration(dest);
	if (!gf_isom_get_timescale(dest)) {
		gf_isom_set_timescale(dest, gf_isom_get_timescale(orig));
	}
	dest_orig_dur /= gf_isom_get_timescale(dest);

	fprintf(stdout, "Appending file %s\n", fileName);
	nb_done = 0;
	for (i=0; i<nb_tracks; i++) {
		u64 last_DTS, dest_track_dur_before_cat;
		u32 nb_edits = 0;
		Bool new_track = 0;
		Bool use_ts_dur = 1;
		Bool merge_edits = 0;
		mtype = gf_isom_get_media_type(orig, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_HINT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_FLASH:
			continue;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SCENE:
			use_ts_dur = 0;
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
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
		tk_id = gf_isom_get_track_id(orig, i+1);
		dst_tk = gf_isom_get_track_by_id(dest, tk_id);
		if (dst_tk) {
			/*we MUST have the same codec*/
			if (gf_isom_get_media_subtype(orig, i+1, 1) != gf_isom_get_media_subtype(dest, dst_tk, 1)) dst_tk = 0;
			/*we only support cat with the same number of sample descriptions*/
			if (gf_isom_get_sample_description_count(orig, i+1) != gf_isom_get_sample_description_count(dest, dst_tk)) dst_tk = 0;
			/*if not forcing cat, check the media codec config is the same*/
			if (!force_cat && !gf_isom_is_same_sample_description(orig, i+1, dest, dst_tk)) dst_tk = 0;
			/*we force the same visual resolution*/
			else if (mtype==GF_ISOM_MEDIA_VISUAL) {
				u32 w, h, ow, oh;
				gf_isom_get_visual_info(orig, i+1, 1, &ow, &oh);
				gf_isom_get_visual_info(dest, dst_tk, 1, &w, &h);
				if ((ow!=w) || (oh!=h)) dst_tk = 0;
			}
		}

		if (!dst_tk) {
			for (j=0; j<gf_isom_get_track_count(dest); j++) {
				if (mtype != gf_isom_get_media_type(dest, j+1)) continue;
				if (gf_isom_is_same_sample_description(orig, i+1, dest, j+1)) {
					if (mtype==GF_ISOM_MEDIA_VISUAL) {
						u32 w, h, ow, oh;
						gf_isom_get_visual_info(orig, i+1, 1, &ow, &oh);
						gf_isom_get_visual_info(dest, j+1, 1, &w, &h);
						if ((ow==w) && (oh==h)) {
							dst_tk = j+1;
							break;
						}
					}
					/*check language code*/
					else if (mtype==GF_ISOM_MEDIA_AUDIO) {
						u32 lang_src, lang_dst;
						char lang[4];
						lang[3]=0;
						gf_isom_get_media_language(orig, i+1, lang);
						lang_src = GF_4CC(lang[0], lang[1], lang[2], lang[3]);
						gf_isom_get_media_language(dest, j+1, lang);
						lang_dst = GF_4CC(lang[0], lang[1], lang[2], lang[3]);
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
		/*looks like a new file*/
		if (!dst_tk) {
			fprintf(stdout, "No suitable destination track found - creating new one (type %s)\n", gf_4cc_to_str(mtype));
			e = gf_isom_clone_track(orig, i+1, dest, 1, &dst_tk);
			if (e) goto err_exit;
			gf_isom_clone_pl_indications(orig, dest);
			new_track = 1;
		} else {
			nb_edits = gf_isom_get_edit_segment_count(orig, i+1);
		}

		dest_track_dur_before_cat = gf_isom_get_media_duration(dest, dst_tk);

		count = gf_isom_get_sample_count(dest, dst_tk);
		if (use_ts_dur && (count>1)) {
			insert_dts = 2*gf_isom_get_sample_dts(dest, dst_tk, count) - gf_isom_get_sample_dts(dest, dst_tk, count-1);
		} else {
			insert_dts = dest_track_dur_before_cat;
			if (!count) insert_dts = 0;
		}

		ts_scale = (Float) gf_isom_get_media_timescale(dest, dst_tk);
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

		last_DTS = 0;
		count = gf_isom_get_sample_count(orig, i+1);
		for (j=0; j<count; j++) {
			u32 di;
			samp = gf_isom_get_sample(orig, i+1, j+1, &di);
			last_DTS = samp->DTS;
			samp->DTS =  (u64) (ts_scale * (s64)samp->DTS) + insert_dts;
			samp->CTS_Offset =  (u32) (samp->CTS_Offset * ts_scale);

			if (gf_isom_is_self_contained(orig, i+1, di)) {
				e = gf_isom_add_sample(dest, dst_tk, di, samp);
			} else {
				u64 offset;
				GF_ISOSample *s = gf_isom_get_sample_info(orig, i+1, j+1, &di, &offset);
				e = gf_isom_add_sample_reference(dest, dst_tk, di, samp, offset);
				gf_isom_sample_del(&s);
			}
			gf_isom_sample_del(&samp);
			if (e) goto err_exit;
			gf_set_progress("Appending", nb_done, nb_samp);
			nb_done++;
		}
		/*scene description and text: compute last sample duration based on original media duration*/
		if (!use_ts_dur) {
			insert_dts = gf_isom_get_media_duration(orig, i+1) - last_DTS;
			gf_isom_set_last_sample_duration(dest, dst_tk, (u32) insert_dts);
		}

		if (merge_edits) {
			/*get the first edit normal mode and add the new track dur*/
			for (j=nb_edits; j>0; j--) {
				u64 editTime, segmentDuration, mediaTime;
				u8 editMode;
				gf_isom_get_edit_segment(dest, dst_tk, j, &editTime, &segmentDuration, &mediaTime, &editMode);

				if (editMode==GF_ISOM_EDIT_NORMAL) {
					Double dur = (Double) (s64) gf_isom_get_media_duration(orig, i+1);
					/*convert to dst time scale*/
					dur *= ts_scale;

					/*convert to track time scale*/
					ts_scale = (Float) gf_isom_get_timescale(dest);
					ts_scale /= (Float) gf_isom_get_media_timescale(dest, dst_tk);
					dur *= ts_scale;

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
			gf_isom_get_edit_segment(dest, dst_tk, count, &editTime, &segmentDuration, &mediaTime, &editMode);


			/*convert to dst time scale*/
			ts_scale = (Float) gf_isom_get_timescale(dest);
			ts_scale /= (Float) gf_isom_get_timescale(orig);

			edit_offset = editTime + segmentDuration;
			count = gf_isom_get_edit_segment_count(orig, i+1);
			for (j=0; j<count; j++) {
				gf_isom_get_edit_segment(orig, i+1, j+1, &editTime, &segmentDuration, &mediaTime, &editMode);
				t = (Double) (s64) editTime; t *= ts_scale; t += (s64) edit_offset; editTime = (s64) t;
				t = (Double) (s64) segmentDuration; t *= ts_scale; segmentDuration = (s64) t;
				t = (Double) (s64) mediaTime; t *= ts_scale; t+= (s64) dest_track_dur_before_cat; mediaTime = (s64) t;

				gf_isom_set_edit_segment(dest, dst_tk, editTime, segmentDuration, mediaTime, editMode);
			}
		}

	}
	gf_set_progress("Appending", nb_samp, nb_samp);

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
	Double force_fps;
	u32 frames_per_sample;
	char *tmp_dir;
	Bool force_cat;
} CATEnum;

Bool cat_enumerate(void *cbk, char *szName, char *szPath)
{
	GF_Err e;
	u32 len_rad1;
	char szFileName[GF_MAX_PATH];
	CATEnum *cat_enum = (CATEnum *)cbk;
	len_rad1 = strlen(cat_enum->szRad1);
	if (strnicmp(szName, cat_enum->szRad1, len_rad1)) return 0;
	if (strlen(cat_enum->szRad2) && !strstr(szName + len_rad1, cat_enum->szRad2) ) return 0;

	strcpy(szFileName, szName);
	strcat(szFileName, cat_enum->szOpt);

	e = cat_isomedia_file(cat_enum->dest, szFileName, cat_enum->import_flags, cat_enum->force_fps, cat_enum->frames_per_sample, cat_enum->tmp_dir, cat_enum->force_cat);
	if (e) return 1;
	return 0;
}

GF_Err cat_multiple_files(GF_ISOFile *dest, char *fileName, u32 import_flags, Double force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat)
{
	CATEnum cat_enum;
	char *sep;

	cat_enum.dest = dest;
	cat_enum.import_flags = import_flags;
	cat_enum.force_fps = force_fps;
	cat_enum.frames_per_sample = frames_per_sample;
	cat_enum.tmp_dir = tmp_dir;
	cat_enum.force_cat = force_cat;

	strcpy(cat_enum.szPath, fileName);
	sep = strrchr(cat_enum.szPath, GF_PATH_SEPARATOR);
	if (!sep) sep = strrchr(cat_enum.szPath, '/');
	if (!sep) {
		strcpy(cat_enum.szPath, ".");
		strcpy(cat_enum.szRad1, fileName);
	} else {
		strcpy(cat_enum.szRad1, sep+1);
		sep[0] = 0;
	}
	sep = strchr(cat_enum.szRad1, '*');
	strcpy(cat_enum.szRad2, sep+1);
	sep[0] = 0;
	sep = strchr(cat_enum.szRad2, '%');
	if (!sep) sep = strchr(cat_enum.szRad2, '#');
	if (!sep) sep = strchr(cat_enum.szRad2, ':');
	strcpy(cat_enum.szOpt, "");
	if (sep) {
		strcpy(cat_enum.szOpt, sep);
		sep[0] = 0;
	}
	return gf_enum_directory(cat_enum.szPath, 0, cat_enumerate, &cat_enum, NULL);
}


/*
		MPEG-4 encoding
*/

GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs) 
{
	GF_Err e;
	GF_SceneLoader load;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_StatManager *statsman = NULL;
	
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
		fprintf(stdout, "Cannot load context %s - %s\n", in, gf_error_to_string(e));
		goto err_exit;
	}
	e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);

	if (opts->auto_quant) {
		fprintf(stdout, "Analysing Scene for Automatic Quantization\n");
		statsman = gf_sm_stats_new();
		e = gf_sm_stats_for_scene(statsman, ctx);
		if (!e) {
			GF_SceneStatistics *stats = gf_sm_stats_get(statsman);
			/*LASeR*/
			if (opts->auto_quant==1) {
				if (opts->resolution > (s32)stats->frac_res_2d) {
					fprintf(stdout, " Given resolution %d is (unnecessarily) too high, using %d instead.\n", opts->resolution, stats->frac_res_2d);
					opts->resolution = stats->frac_res_2d;
				} else if (stats->int_res_2d + opts->resolution <= 0) {
					fprintf(stdout, " Given resolution %d is too low, using %d instead.\n", opts->resolution, stats->int_res_2d - 1);
					opts->resolution = 1 - stats->int_res_2d;
				}				
				opts->coord_bits = stats->int_res_2d + opts->resolution;
				fprintf(stdout, " Coordinates & Lengths encoded using ");
				if (opts->resolution < 0) fprintf(stdout, "only the %d most significant bits (of %d).\n", opts->coord_bits, stats->int_res_2d);
				else fprintf(stdout, "a %d.%d representation\n", stats->int_res_2d, opts->resolution);

				fprintf(stdout, " Matrix Scale & Skew Coefficients ");
				if (opts->coord_bits - 8 < stats->scale_int_res_2d) {
					opts->scale_bits = stats->scale_int_res_2d - opts->coord_bits + 8;
					fprintf(stdout, "encoded using a %d.8 representation\n", stats->scale_int_res_2d);
				} else  {
					opts->scale_bits = 0;
					fprintf(stdout, "encoded using a %d.8 representation\n", opts->coord_bits - 8);
				}
			} 
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
				if (0 && stats->count_float && opts->resolution) {
					qp->scaleMin = stats->min_fixed;
					qp->scaleMax = stats->max_fixed;
					qp->scaleNbBits = 2*opts->resolution;
					qp->scaleQuant = 1;
				}
			}
		}
	}

	if (e) {
		fprintf(stdout, "Error loading file %s\n", gf_error_to_string(e));
		goto err_exit;
	} else {
		u32 prev_level = gf_log_get_level();
		u32 prev_tools = gf_log_get_tools();
		gf_log_cbk prev_logs = NULL;
		if (logs) {
			gf_log_set_tools(GF_LOG_CODING);
			gf_log_set_level(GF_LOG_DEBUG);
			prev_logs = gf_log_set_callback(logs, scene_coding_log);
		}
		e = gf_sm_encode_to_file(ctx, mp4, opts);
		if (logs) {
			gf_log_set_tools(prev_tools);
			gf_log_set_level(prev_level);
			gf_log_set_callback(NULL, prev_logs);
		}
	}

	gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_MP42, 1);
	gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_ISOM, 1);

err_exit:
	if (statsman) gf_sm_stats_del(statsman);
	gf_sm_del(ctx);
	gf_sg_del(sg);
	return e;
}

/*
		MPEG-4 chunk encoding
*/

static u32 GetNbBits(u32 MaxVal)
{
	u32 k=0;
	while ((s32) MaxVal > ((1<<k)-1) ) k+=1;
	return k;
}

GF_Err EncodeBIFSChunk(GF_SceneManager *ctx, char *bifsOutputFile, GF_Err (*AUCallback)(GF_ISOSample *))
{
	GF_Err			e;
	char *data;
	u32 data_len;
	GF_BifsEncoder *bifsenc;
	GF_InitialObjectDescriptor *iod;
	u32 i, j, count;
	GF_StreamContext *sc;
	GF_ESD *esd;
	Bool is_in_iod, delete_desc, encode_names, delete_bcfg;
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
			sc = gf_list_get(ctx->streams, i);
			if (sc->streamType == GF_STREAM_OD) count++;
		}
		if (!iod && count>1) return GF_NOT_SUPPORTED;
	}

	count = gf_list_count(ctx->streams);

	for (i=0; i<gf_list_count(ctx->streams); i++) {
		u32 nbb;
		GF_StreamContext *sc = gf_list_get(ctx->streams, i);
		esd = NULL;
		if (sc->streamType != GF_STREAM_SCENE) continue;

		delete_desc = 0;
		esd = NULL;
		is_in_iod = 1;
		if (iod) {
			is_in_iod = 0;
			for (j=0; j<gf_list_count(iod->ESDescriptors); j++) {
				esd = gf_list_get(iod->ESDescriptors, j);
				if (esd->decoderConfig && esd->decoderConfig->streamType == GF_STREAM_SCENE) {
					if (!sc->ESID) sc->ESID = esd->ESID;
					if (sc->ESID == esd->ESID) {
						is_in_iod = 1;
						break;
					}
				}
				/*special BIFS direct import from NHNT*/
				else if (gf_list_count(iod->ESDescriptors)==1) {
					sc->ESID = esd->ESID;
					is_in_iod = 1;
					break;
				}
				esd = NULL;
			}
		}

		if (!esd) {
			delete_desc = 1;
			esd = gf_odf_desc_esd_new(2);
			gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = NULL;
			esd->ESID = sc->ESID;
			esd->decoderConfig->streamType = GF_STREAM_SCENE;
		}

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
		if (bcfg->nodeIDbits<nbb) fprintf(stdout, "Warning: BIFSConfig.NodeIDBits TOO SMALL\n");
		nbb = GetNbBits(ctx->max_route_id);
		if (bcfg->routeIDbits<nbb) fprintf(stdout, "Warning: BIFSConfig.RouteIDBits TOO SMALL\n");
		nbb = GetNbBits(ctx->max_proto_id);
		if (bcfg->protoIDbits<nbb) fprintf(stdout, "Warning: BIFSConfig.ProtoIDBits TOO SMALL\n");

		/*this is the real pb, not stored in cfg or file level, set at EACH replaceScene*/
		encode_names = 0;

		/* The BIFS Config that is passed here should be the BIFSConfig from the IOD */
		gf_bifs_encoder_new_stream(bifsenc, sc->ESID, bcfg, encode_names, 0);
		if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);

		/*setup MP4 track*/
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
			char *data;
			u32 data_len;
			au = gf_list_get(sc->AUs, j);
			e = gf_bifs_encode_au(bifsenc, sc->ESID, au->commands, &data, &data_len);
			if (data) {
				sprintf(szName, "%s%02d.bifs", szRad, j);
				f = fopen(szName, "wb");
				fwrite(data, data_len, 1, f);
				fclose(f);
				free(data);
			}
		}
	}
	gf_bifs_encoder_del(bifsenc);
	return e;
}


/**
 * @chunkFile BT chunk to be encoded
 * @bifs output file name for the BIFS data
 * @inputContext initial BT upon which the chunk is based (shall not be NULL)
 * @outputContext: file name to dump the context after applying the new chunk to the input context 
                   can be NULL, without .bt
 * @logFile: can be NULL
 */
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext, const char *tmpdir) 
{
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
		fprintf(stdout, "Cannot load context %s - %s\n", inputContext, gf_error_to_string(e));
		goto exit;
	}

	/* Step 2: make sure we have only ONE RAP for each stream*/
	e = gf_sm_make_random_access(ctx);
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
		fprintf(stdout, "Cannot load chunk context %s - %s\n", chunkFile, gf_error_to_string(e));
		goto exit;
	}
	fprintf(stdout, "Context and chunks loaded\n");

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
		e = gf_sm_make_random_access(ctx);
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
		else e = gf_sm_dump(ctx, szF, d_mode);
	}

exit:
	if (ctx) {
		sg = ctx->scene_graph;
		gf_sm_del(ctx);
		gf_sg_del(sg);
	}
	return e;
}

#include <gpac/xml.h>

void sax_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	char szCheck[100];
	GF_List *imports = sax_cbck;
	GF_XMLAttribute *att;
	u32 i=0;

	/*do not process hyperlinks*/
	if (!strcmp(node_name, "a") || !strcmp(node_name, "Anchor")) return;
	
	for (i=0; i<nb_attributes;i++) {
		att = (GF_XMLAttribute *) &attributes[i];
		if (stricmp(att->name, "xlink:href") && stricmp(att->name, "url")) continue;
		if (att->value[0]=='#') continue;
		if (!strnicmp(att->value, "od:", 3)) continue;
		sprintf(szCheck, "%d", atoi(att->value));
		if (!strcmp(szCheck, att->value)) continue;
		gf_list_add(imports, strdup(att->value) );
	}
}

GF_ISOFile *package_file(char *file_name, char *fcc, const char *tmpdir)
{
	GF_ISOFile *file = NULL;
	GF_Err e;
	GF_SAXParser *sax;
	GF_List *imports;
	Bool ascii;
	char *isom_src = NULL;
	u32 i, count, mtype;
	char *type;

	type = gf_xml_get_root_type(file_name, &e);
	if (!type) {
		fprintf(stdout, "Cannot process XML file %s: %s\n", file_name, gf_error_to_string(e) );
		return NULL;
	}

	imports = gf_list_new();
	sax = gf_xml_sax_new(sax_node_start, NULL, NULL, imports);
	e = gf_xml_sax_parse_file(sax, file_name, NULL);
	ascii = !gf_xml_sax_binary_file(sax);
	gf_xml_sax_del(sax);
	if (e<0) goto exit;
	e = GF_OK;

	if (fcc) {
		mtype = GF_4CC(fcc[0],fcc[1],fcc[2],fcc[3]);
	} else {
		mtype = 0;
		if (!stricmp(type, "svg")) mtype = ascii ? GF_4CC('s','v','g',' ') : GF_4CC('s','v','g','z');
		else if (!stricmp(type, "smil")) mtype = ascii ? GF_4CC('s','m','i','l') : GF_4CC('s','m','l','z');
		else if (!stricmp(type, "x3d")) mtype = ascii ? GF_4CC('x','3','d',' ')  : GF_4CC('x','3','d','z')  ;
		else if (!stricmp(type, "xmt-a")) mtype = ascii ? GF_4CC('x','m','t','a') : GF_4CC('x','m','t','z');
	}
	if (!mtype) {
		fprintf(stdout, "Missing 4CC code for meta name - please use ABCD:fileName\n");
		e = GF_BAD_PARAM;
		goto exit;
	}


	count = gf_list_count(imports);
	for (i=0; i<count; i++) {
		char *item = gf_list_get(imports, i);
		FILE *test = fopen(item, "rb");
		if (!test) {
			gf_list_rem(imports, i);
			i--;
			count--;
			free(item);
			continue;
		}
		fclose(test);
		if (gf_isom_probe_file(item)) {
			if (isom_src) {
				fprintf(stdout, "Cannot package several IsoMedia files together\n");
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
	if (isom_src) {
		file = gf_isom_open(isom_src, GF_ISOM_OPEN_EDIT, tmpdir);
	} else {
		file = gf_isom_open("package", GF_ISOM_WRITE_EDIT, tmpdir);
	}

	e = gf_isom_set_meta_type(file, 1, 0, mtype);
	if (e) goto exit;
	/*add self ref*/
	if (isom_src) {
		e = gf_isom_add_meta_item(file, 1, 0, 1, NULL, isom_src, NULL, NULL, NULL,  NULL);
		if (e) goto exit;
	}
	e = gf_isom_set_meta_xml(file, 1, 0, file_name, !ascii);
	if (e) goto exit;

	count = gf_list_count(imports);
	for (i=0; i<count; i++) {
		char *ext, *mime, *encoding;
		char *item = gf_list_get(imports, i);

		mime = encoding = NULL;
		ext = strrchr(item, '.');
		if (!stricmp(ext, ".gz")) ext = strrchr(ext-1, '.');

		if (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg")) mime = "image/jpeg"; 
		else if (!stricmp(ext, ".png")) mime = "image/png"; 
		else if (!stricmp(ext, ".svg")) mime = "image/svg+xml"; 
		else if (!stricmp(ext, ".x3d")) mime = "model/x3d+xml"; 
		else if (!stricmp(ext, ".xmt")) mime = "application/x-xmt"; 
		else if (!stricmp(ext, ".svgz") || !stricmp(ext, ".svg.gz")) { mime = "image/svg+xml"; encoding = "binary-gzip"; }
		else if (!stricmp(ext, ".x3dz") || !stricmp(ext, ".x3d.gz")) { mime = "model/x3d+xml"; encoding = "binary-gzip"; }
		else if (!stricmp(ext, ".xmtz") || !stricmp(ext, ".xmt.gz")) { mime = "application/x-xmt"; encoding = "binary-gzip"; }

		e = gf_isom_add_meta_item(file, 1, 0, 0, item, item, mime, NULL, NULL,  NULL);
		if (e) goto exit;
	}

exit:
	while (gf_list_count(imports)) {
		char *item = gf_list_last(imports);
		gf_list_rem_last(imports);
		free(item);
	}
	gf_list_del(imports);
	if (isom_src) free(isom_src);
	if (type) free(type);
	if (e) {
		if (file) gf_isom_delete(file);
		return NULL;
	}
	return file;
}

#endif

