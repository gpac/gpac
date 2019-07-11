/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

#ifdef GPAC_DISABLE_ISOM

#error "Cannot compile MP4Box if GPAC is not built with ISO File Format support"

#else

#if defined(WIN32) && !defined(_WIN32_WCE)
#include <io.h>
#include <fcntl.h>
#endif

#include <gpac/media_tools.h>
#include <gpac/main.h>

/*RTP packetizer flags*/
#ifndef GPAC_DISABLE_STREAMING
#include <gpac/ietf.h>
#endif

#ifndef GPAC_DISABLE_CRYPTO
#include <gpac/crypt_tools.h>
#endif

#include <gpac/constants.h>

#include <gpac/internal/mpd.h>

#define BUFFSIZE	8192
#define DEFAULT_INTERLEAVING_IN_SEC 0.5


int mp4boxTerminal(int argc, char **argv);

Bool dvbhdemux = GF_FALSE;
Bool keep_sys_tracks = GF_FALSE;


/*some global vars for swf import :(*/
u32 swf_flags = 0;
Float swf_flatten_angle = 0;
s32 laser_resolution = 0;
static FILE *helpout = NULL;
static u32 help_flags = 0;

typedef struct {
	u32 code;
	const char *name;
	const char *comment;
} itunes_tag;
static const itunes_tag itags[] = {
	{GF_ISOM_ITUNE_ALBUM_ARTIST, "album_artist", "usage: album_artist=album artist"},
	{GF_ISOM_ITUNE_ALBUM, "album", "usage: album=name" },
	{GF_ISOM_ITUNE_TRACKNUMBER, "tracknum", "usage: track=x/N"},
	{GF_ISOM_ITUNE_TRACK, "track", "usage: track=name"},
	{GF_ISOM_ITUNE_ARTIST, "artist", "usage: artist=name"},
	{GF_ISOM_ITUNE_COMMENT, "comment", "usage: comment=any comment"},
	{GF_ISOM_ITUNE_COMPILATION, "compilation", "usage: compilation=yes,no"},
	{GF_ISOM_ITUNE_COMPOSER, "composer", "usage: composer=name"},
	{GF_ISOM_ITUNE_CREATED, "created", "usage: created=time"},
	{GF_ISOM_ITUNE_DISK, "disk", "usage: disk=x/N"},
	{GF_ISOM_ITUNE_TOOL, "tool", "usage: tool=name"},
	{GF_ISOM_ITUNE_GENRE, "genre", "usage: genre=name"},
	{GF_ISOM_ITUNE_NAME, "name", "usage: name=name"},
	{GF_ISOM_ITUNE_TEMPO, "tempo", "usage: tempo=integer"},
	{GF_ISOM_ITUNE_WRITER, "writer", "usage: writer=name"},
	{GF_ISOM_ITUNE_GROUP, "group", "usage: group=name"},
	{GF_ISOM_ITUNE_COVER_ART, "cover", "usage: cover=file.jpg,file.png"},
	{GF_ISOM_ITUNE_ENCODER, "encoder", "usage: encoder=name"},
	{GF_ISOM_ITUNE_GAPLESS, "gapless", "usage: gapless=yes,no"},
	{GF_ISOM_ITUNE_ALL, "all", "usage: all=NULL"},
};

u32 nb_itunes_tags = sizeof(itags) / sizeof(itunes_tag);


void PrintVersion()
{
	fprintf(stderr, "MP4Box - GPAC version %s\n"
	        "%s\n"
	        "GPAC Configuration: " GPAC_CONFIGURATION "\n"
	        "Features: %s %s\n", gf_gpac_version(), gf_gpac_copyright(), gf_enabled_features(), gf_disabled_features());
}

GF_GPACArg m4b_gen_args[] =
{
#ifdef GPAC_MEMORY_TRACKING
 	GF_DEF_ARG("mem-track", NULL, "enable memory tracker", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("mem-track-stack", NULL, "enable memory tracker with stack dumping", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#endif
 	GF_DEF_ARG("inter", NULL, "interleave file, producing track chunks with given duration in ms. A value of 0 disables interleaving ", "0.5", NULL, GF_ARG_DOUBLE, 0),
 	GF_DEF_ARG("old-inter", NULL, "same as [-inter]() but wihout drift correction", NULL, NULL, GF_ARG_DOUBLE, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("tight", NULL, "tight interleaving (sample based) of the file. This reduces disk seek operations but increases file size", NULL, NULL, GF_ARG_DOUBLE, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("flat", NULL, "store file with all media data first, non-interleaved. This speeds up writing time when creating new files", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("frag", NULL, "fragment file, producing track fragments of given duration in ms. This disables interleaving", NULL, NULL, GF_ARG_DOUBLE, 0),
 	GF_DEF_ARG("out", NULL, "specify output file name. By default input file is overwritten", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("tmp", NULL, "specify directory for temporary file creation", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("co64", NULL, "force usage of 64-bit chunk offsets for ISOBMF files", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("write-buffer", NULL, "specify write buffer in bytes for ISOBMF files to reduce disk IO", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("new", NULL, "force creation of a new destination file", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("no-sys", NULL, "remove all MPEG-4 Systems info except IOD, kept for profiles. This is the default when creating regular AV content", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("no-sys", NULL, "remove MPEG-4 InitialObjectDescriptor from file", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("mfra", NULL, "insert movie fragment random offset when fragmenting file (ignored in dash mode)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("isma", NULL, "rewrite the file as an ISMA 1.0 file", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("ismax", NULL, "same as [-isma]() and remove all clock references", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("3gp", NULL, "rewrite as 3GPP(2) file (no more MPEG-4 Systems Info), always enabled if destination file extension is `.3gp`, `.3g2` or `.3gpp`. Some tracks may be removed in the process", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("ipod", NULL, "rewrite the file for iPod/old iTunes", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("psp", NULL, "rewrite the file for PSP devices", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("brand", NULL, "set major brand of file (`ABCD`) or brand with optionnal version (`ABCD:v`)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("ab", NULL, "add given brand to file's alternate brand list", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("rb", NULL, "remove given brand to file's alternate brand list", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("cprt", NULL, "add copyright string to file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("chap", NULL, "add chapter information from given file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("set-track-id `id1:id2`", NULL, "change id of track with id1 to id2", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("swap-track-id `id1:id2`", NULL, "swap the id between tracks with id1 to id2", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("rem", NULL, "remove given track from file", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("rap", NULL, "remove all non-RAP samples from given track", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("refonly", NULL, "remove all non-reference pictures from given track", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("enable", NULL, "enable given track", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("disable", NULL, "disable given track", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("timescale", NULL, "set movie timescale to given value (ticks per second)", "600", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("lang `[ID=]LAN`", NULL, "set language. LAN is the BCP-47 code (eng, en-UK, ...). If no track ID is given, sets language to all tracks", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("delay `ID=TIME`", NULL, "set track start delay in ms", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("par `ID=PAR`", NULL, "set visual track pixel aspect ratio. PAR is `N:D` or `none` or `force` to write anyway", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("clap `ID=CLAP`", NULL, "set visual track clean aperture. CLAP is `Wn:Wd:Hn:Hd:HOn:HOd:VOn:VOd` or `none`\n"
 			"- n, d: numerator, denominator\n"
	        "- W, H, HO, VO: clap width, clap height, clap horizontal offset, clap vertical offset\n"
 			, NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("mx `ID=MX`", NULL, "set track matrix, with MX is M1:M2:M3:M4:M5:M6:M7:M8:M9 in 16.16 fixed point intergers or hexa"
 			, NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("name `ID=NAME`", NULL, "set track handler name to NAME (UTF-8 string)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("itags `tag1[:tag2]`", NULL, "set iTunes tags to file, see [-tag-list]()", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("tag-list", NULL, "print the set of supported iTunes tags", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("split", NULL, "split in files of given max duration. Set [-rap] to start each file at RAP", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("split-size", "splits", "split in files of given max size (in kb)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("split-rap", "splitr", "split in files at each new RAP", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("split-chunk `S:E`", "splitx", "extract a new file from `S` (number of seconds) to `E` with `E` a number (in seconds), `end` or `end-N`, N being the desired number of seconds before the end", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("splitz `S:E`", NULL, "same as -split-chunk, but adjust the end time to be before the last RAP sample", NULL, NULL, GF_ARG_STRING, 0),

 	GF_DEF_ARG("group-add", NULL, "create a new grouping information in the file. Format is a colon-separated list of following options:\n"
	        "- refTrack=ID: ID of the track used as a group reference. If not set, the track will belong to the same group as the "
	        "previous trackID specified. If 0 or no previous track specified, a new alternate group will be created\n"
	        "- switchID=ID: ID of the switch group to create. If 0, a new ID will be computed for you. If <0, disables SwitchGroup\n"
	        "- criteria=string: list of space-separated 4CCs.\n"
	        "- trackID=ID: ID of the track to add to this group.\n"
	        "  \n"
	        "Warning: Options modify state as they are parsed, `trackID=1:criteria=lang:trackID=2` is different from `criteria=lang:trackID=1:trackID=2`"
	        "\n", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),

	GF_DEF_ARG("group-rem-track", NULL, "remove given track from its group", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("group-rem", NULL, "remove the track's group\n", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("group-clean", NULL, "remove all group information from all tracks\n", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("ref `id:XXXX:refID`", NULL, "add a reference of type 4CC from track ID to track refID\n", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("keep-utc", NULL, "keep UTC timing in the file after edit\n", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("udta ID:[OPTS]", NULL, "set udta for given track or movie if ID is 0. OPTS is a colon separated list of:\n"
	        "- type=CODE: 4CC code of the UDTA (not needed for `box=` option)\n"
	        "- box=FILE: location of the udta data, formatted as serialized boxes\n"
	        "- box=base64,DATA: base64 encoded udta data, formatted as serialized boxes\n"
	        "- src=FILE: location of the udta data (will be stored in a single box of type CODE)\n"
	        "- src=base64,DATA: base64 encoded udta data (will be stored in a single box of type CODE)\n"
	        "Note: If no source is set, UDTA of type CODE will be removed\n", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("patch", NULL, "apply box patch in the given file\n", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("bo", NULL, "freeze the order of boxes in input file\n", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	{0}
};

void PrintGeneralUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# General Options\n"
		"MP4Box is a multimedia packager, with a vast number of functionalities: conversion, splitting, hinting, dumping, DASH-ing, encryption and others.\n"
		"MP4Box provides a large set of options, classified by categories (see [-h]()). These options do not follow any particular ordering.\n"
		"MP4Box performs in-place rewrite of IsoMedia files (the input file is overwritten). You can change this behaviour by using the [-out]() option.\n"
		"MP4Box stores by default the file with 0.5 second interleaving and meta-data (`moov`...) at the beginning, making it suitable for HTTP streaming. This may however takes longer to store the file, use [-flat]() to change this behaviour.\n"
		"MP4Box usually generates a temporary file when creating a new IsoMedia file. The location of this temporary file is OS-dependent, and it may happen that the drive/partition the temporary file is created on has not enough space or no write access. In such a case, you can specify a temporary file location with [-tmp]().\n"
		"Note: Track operations identify tracks through their ID, not their order\n"
		"  \n"
		"# File Splitting and Concatenation\n"
		"MP4Box can split IsoMedia files by size, duration or extract a given part of the file to new IsoMedia file(s). This process requires that at most one track in the input file has non random-access points (typically one video track at most). This process will also ignore all MPEG-4 Systems tracks and hint tracks, but will try to split private media tracks.\n"
		"Note: The input file must have enough random access points in order to be split. This may not be the case with some video files where only the very first sample of the video track is a key frame (many 3GP files with H263 video are recorded that way). In order to split such files you will have to use a real video editor and re-encode the content.\n"
		"Note: You can add media to a file and split it in the same pass. In this case, the destination file (the one which would be obtained without spliting) will not be stored.\n"
		"  \n"
		"Options:\n"
	);

	while (m4b_gen_args[i].name) {
		GF_GPACArg *arg = &m4b_gen_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-gen");
	}

}


GF_GPACArg m4b_dash_args[] =
{
 	GF_DEF_ARG("mpd", NULL, "convert given HLS or smooth manifest (local or remote http) to MPD.\nWarning: This is not compatible with other DASH options and does not convert associated segments", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("dash", "-dash-strict", "create DASH from input files with given segment (subsegment for onDemand profile) duration in ms", NULL, NULL, GF_ARG_DOUBLE, 0),
 	GF_DEF_ARG("dash-live", NULL, "generate a live DASH session using the given segment duration in ms; using `-dash-live=F`will also write the live context to `F`. MP4Box will run the live session until `q` is pressed or a fatal error occurs", NULL, NULL, GF_ARG_DOUBLE, 0),
 	GF_DEF_ARG("ddbg-live", NULL, "same as [-dash-live]() without time regulation for debug purposes", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("frag", NULL, "specify the fragment duration in ms. If not set, this is the DASH duration (one fragment per segment)", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("out", NULL, "specify the output MPD file name", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("tmp", NULL, "specify directory for temporary file creation", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("profile", NULL, "specify the target DASH profile, and set default options to ensure conformance to the desired profile. Default profile is `full` in static mode, `live` in dynamic mode", NULL, "onDemand|live|main|simple|full|hbbtv1.5:live|dashavc264:live|dashavc264:onDemand", GF_ARG_STRING, 0),
	GF_DEF_ARG("profile-ext", NULL, "specify a list of profile extensions, as used by DASH-IF and DVB. The string will be colon-concatenated with the profile used", NULL, NULL, GF_ARG_STRING, 0),

	GF_DEF_ARG("rap", NULL, "ensure that segments begin with random access points, segment durations might vary depending on the source encoding", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("frag-rap", NULL, "ensure that all fragments begin with random access points (duration might vary depending on the source encoding)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("segment-name", NULL, "set the segment name for generated segments. If not set (default), segments are concatenated in output file except in `live` profile where `dash_%%s`. Supported replacement strings are:\n"
	        "- $Number[%%0Nd]$ is replaced by the segment number, possibly prefixed with 0.\n"
	        "- $RepresentationID$ is replaced by representation name.\n"
	        "- $Time$ is replaced by segment start time.\n"
	        "- $Bandwidth$ is replaced by representation bandwidth.\n"
	        "- $Init=NAME$ is replaced by NAME for init segment, ignored otherwise. May occur multiple times.\n"
	        "- $Index=NAME$ is replaced by NAME for index segments, ignored otherwise. May occur multiple times.\n"
	        "- $Path=PATH$ is replaced by PATH when creating segments, ignored otherwise. May occur multiple times.\n"
	        "- $Segment=NAME$ is replaced by NAME for media segments, ignored for init segments. May occur multiple times", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("segment-ext", NULL, "set the segment extension, `null` means no extension", "m4s", NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("init-segment-ext", NULL, "set the segment extension for init, index and bitstream switching segments, `null` means no extension\n", "mp4", NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("segment-timeline", NULL, "use `SegmentTimeline` when generating segments", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("segment-marker `MARK`", NULL, "add a box of type `MARK` (4CC) at the end of each DASH segment", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("insert-utc", NULL, "insert UTC clock at the beginning of each ISOBMF segment", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("base-url", NULL, "set Base url at MPD level. Can be used several times.\nWarning: this does not  modify generated files location", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("mpd-title", NULL, "set MPD title", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("mpd-source", NULL, "set MPD source", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("mpd-info-url", NULL, "set MPD info url", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("cprt", NULL, "add copyright string to MPD", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("dash-ctx", NULL, "store/restore DASH timing from indicated file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("dynamic", NULL, "use dynamic MPD type instead of static", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("last-dynamic", NULL, "same as [-dynamic]() but close the period (insert lmsg brand if needed and update duration)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("mpd-duration", NULL, "set the duration in second of a live session (if `0`, you must use [-mpd-refresh]())", "0", NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("mpd-refresh", NULL, "specify MPD update time in seconds", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("time-shift", NULL, "specify MPD time shift buffer depth in seconds, `-1` to keep all files)", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("subdur", NULL, "specify maximum duration in ms of the input file to be dashed in LIVE or context mode. This does not change the segment duration, but stops dashing once segments produced exceeded the duration. If there is not enough samples to finish a segment, data is looped unless [-no-loop]() is used which triggers a period end", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("run-for", NULL, "run for given ms  the dash-live session then exits", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("min-buffer", NULL, "specify MPD min buffer time in ms", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("ast-offset", NULL, "specify MPD AvailabilityStartTime offset in ms if positive, or availabilityTimeOffset of each representation if negative", "0", NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("dash-scale", NULL, "specify that timing for [-dash]() and [-frag]() are expressed in given timexale (units per seconds)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("mem-frags", NULL, "fragmentation happens in memory rather than on disk before flushing to disk", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("pssh", NULL, "set pssh store mode.\n"
	"- v: initial movie\n"
	"- f: movie fragments\n"
	"- m: MPD\n"
	"- mv, vm: in initial movie and MPD\n"
	"- mf, fm: in movie fragments and MPD", NULL, "v|f|m|mv|vm|mf|fm", GF_ARG_INT, 0),
	GF_DEF_ARG("sample-groups-traf", NULL, "store sample group descriptions in traf (duplicated for each traf). If not set, sample group descriptions are stored in the initial movie", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("mvex-after-traks", NULL, "store `mvex` box after `trak` boxes within the moov box. If not set, `mvex` is before", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("no-cache", NULL, "disable file cache for dash inputs", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("no-loop", NULL, "disable looping content in live mode and uses period switch instead", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("hlsc", NULL, "insert UTC in variant playlists for live HLS", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("bound", NULL, "enable video segmentation with same method as audio (i.e.: always try to split before or at the segment boundary - not after)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("closest", NULL, "enable video segmentation closest to the segment boundary (before or after)", NULL, NULL, GF_ARG_BOOL, 0),

	GF_DEF_ARG("subsegs-per-sidx", NULL, "set the number of subsegments to be written in each SIDX box\n"
	"- 0: a single SIDX box is used per segment\n"
	"- -1: no SIDX box is used", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("ssix", NULL, "enable SubsegmentIndexBox describing 2 ranges, first one from moof to end of first I-frame, second one unmapped. This does not work with daisy chaining mode enabled", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("url-template", NULL, "use SegmentTemplate instead of explicit sources in segments. Ignored if segments are stored in the output file", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("daisy-chain", NULL, "use daisy-chain SIDX instead of hierarchical. Ignored if frags/sidx is 0", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("single-segment", NULL, "use a single segment for the whole file (OnDemand profile)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("single-file", NULL, "use a single file for the whole file (default)", "yes", NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("bs-switching", NULL, "set bitstream switching mode\n"
	"- inband: use inband param set and a single init segment\n"
	"- merge: try to merge param sets in a single sample description, fallback to `no`\n"
	"- multi: use several sample description, one per quality\n"
	"- no: use one init segment per quality\n"
	"- single: to test with single input", "inband", "inband|merge|multi|no|single", GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("moof-sn", NULL, "set sequence number of first moof to given value", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("tfdt", NULL, "set TFDT of first traf to given value in SCALE units (cf -dash-scale)", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("no-frags-default", NULL, "disable default fragments flags in trex (required by some dash-if profiles and CMAF/smooth streaming compatibility)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("single-traf", NULL, "use a single track fragment per moof (smooth streaming and derived specs may require this)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("tfdt-traf", NULL, "use a tfdt per track fragment (when -single-traf is used)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("dash-ts-prog", NULL, "program_number to be considered in case of an MPTS input file", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("frag-rt", NULL, "when using fragments in live mode, flush fragments according to their timing", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("cp-location", NULL, "set ContentProtection element location\n"
	        "- as: sets ContentProtection in AdaptationSet element\n"
	        "- rep: sets ContentProtection in Representation element\n"
	        "- both: sets ContentProtection in both elements", NULL, "as|rep\both", GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("start-date", NULL, "for live mode, set start date (as xs:date, eg YYYY-MM-DDTHH:MM:SSZ). Default is current UTC\n"
	"Warning: Do not use with multiple periods, nor when DASH duration is not a multiple of GOP size", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),

	GF_DEF_ARG("cues", NULL, "ignore dash duration and segment according to cue times in given XML file (tests/media/dash_cues for examples)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("strict-cues", NULL, "throw error if something is wrong while parsing cues or applying cue-based segmentation", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	{0}
};

void PrintDASHUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# DASH Options\n"
		"Also see:\n"
		"- the [dasher `gpac -h dash`](dasher) filter documentation\n"
		"- [[online DASH Intro doc|DASH Introduction]].\n"
		"- [[online MP4Box DASH support doc|DASH Support in MP4Box]].\n"
		"\n"
		"# Specifying input files\n"
		"Input media files to dash can use the following modifiers\n"
		"- #trackID=N: only use the track ID N from the source file\n"
		"- #N: only use the track ID N from the source file (mapped to [-tkid](mp4dmx))\n"
		"- #video: only use the first video track from the source file\n"
		"- #audio: only use the first audio track from the source file\n"
		"- :id=NAME: set the representation ID to NAME\n"
		"- :dur=VALUE: process VALUE seconds from the media. If VALUE is longer than media duration, last sample duration is extended.\n"
		"- :period=NAME: set the representation's period to NAME. Multiple periods may be used period appear in the MPD in the same order as specified with this option\n"
		"- :BaseURL=NAME: set the BaseURL. Set multiple times for multiple BaseURLs\nWarning: This does not modify generated files location (see segment template).\n"
		"- :bandwidth=VALUE: set the representation's bandwidth to a given value\n"
		"- :period_duration=VALUE: increase the duration of this period by the given duration in seconds. This is only used when no input media is specified (remote period insertion), eg `:period=X:xlink=Z:duration=Y`\n"
		"- :duration=VALUE: override target DASH segment duration for this input\n"
		"- :xlink=VALUE: set the xlink value for the period containing this element. Only the xlink declared on the first rep of a period will be used\n"
		"- :role=VALUE: set the role of this representation (cf DASH spec). Media with different roles belong to different adaptation sets.\n"
		"- :desc_p=VALUE: add a descriptor at the Period level. Value must be a properly formatted XML element.\n"
		"- :desc_as=VALUE: add a descriptor at the AdaptationSet level. Value must be a properly formatted XML element. Two input files with different values will be in different AdaptationSet elements.\n"
		"- :desc_as_c=VALUE: add a descriptor at the AdaptationSet level. Value must be a properly formatted XML element. Value is ignored while creating AdaptationSet elements.\n"
		"- :desc_rep=VALUE: add a descriptor at the Representation level. Value must be a properly formatted XML element. Value is ignored while creating AdaptationSet elements.\n"
		"- :sscale: force movie timescale to match media timescale of the first track in the segment.\n"
		"- :trackID=N: only use the track ID N from the source file\n"
		"- @@f1[:args][@@fN:args]: set a filter chain to insert between the source and the dasher. Each filter in the chain is formatted as a regular filter, see [filter doc `gpac -h doc`](filters_general). If several filters are set, they will be chained in the given order.\n"
		"\n"
		"Note: `@@f` must be placed after all other options.\n"
		"\n"
		"# Options\n"
		);


	while (m4b_dash_args[i].name) {
		GF_GPACArg *arg = &m4b_dash_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-dash");
	}
}


GF_GPACArg m4b_imp_args[] =
{
 	GF_DEF_ARG("add", NULL, "add given file tracks to file. Multiple inputs can be specified using `+`(eg `-add url1+url2)", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("cat", NULL, "concatenate given file samples to file, creating tracks if needed. Multiple inputs can be specified using `+`(eg `-cat url1+url2).\nNote: Note: This aligns initial timestamp of the file to be concatenated", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("catx", NULL, "same as [-cat]() but new tracks can be imported before concatenation by specifying `+ADD_COMMAND` where `ADD_COMMAND` is a regular [-add]() syntax", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("catpl", NULL, "concatenate files listed in the given playlist file (one file per line, lines starting with # are comments).\nNote: Each listed file is concatenated as if called with -cat", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("unalign-cat", NULL, "do not attempt to align timestamps of samples inbetween tracks", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("force-cat", NULL, "skip media configuration check when concatenating file.\nWarning: THIS MAY BREAK THE CONCATENATED TRACK(S)", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("keep-sys", NULL, "keep all MPEG-4 Systems info when using [-add]() and [-cat]() (only used when adding IsoMedia files)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dref", NULL, "keep media data in original file using `data referencing`. The resulting file only contains the meta-data of the presentation (frame sizes, timing, etc...) and references media data in the original file. This is extremely useful when developping content, since importing and storage of the MP4 file is much faster and the resulting file much smaller.\nNote: Data referencing may fail on some files because it requires the framed data (eg an IsoMedia sample) to be continuous in the original file, which is not always the case depending on the original interleaving or bitstream format (__AVC__ or __HEVC__ cannot use this option)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-drop", NULL, "force constant FPS when importing AVI video", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("packed", NULL, "force packed bitstream when importing raw MPEG-4 part 2 Advanced Simple Profile", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("sbr", NULL, "backward compatible signaling of AAC-SBR", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("sbrx", NULL, "non-backward compatible signaling of AAC-SBR", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ps", NULL, "backward compatible signaling of AAC-PS", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("psx", NULL, "non-backward compatible signaling of AAC-PS", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ovsbr", NULL, "oversample SBR import (SBR AAC, PS AAC and oversampled SBR cannot be detected at import time)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("fps", NULL, "force frame rate for video and SUB subtitles import to the given value, expressed as a number or as `timescale-increment`.\nNote: For raw H263 import, default FPS is `15`, otherwise `25`\nWarning: This is ignored for ISOBMFF import, use `:rescale` option for that", "25", NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("mpeg4", NULL, "force MPEG-4 sample descriptions when possible. For AAC, forces MPEG-4 AAC signaling even if MPEG-2", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("agg", NULL, "aggregate N audio frames in 1 sample (3GP media only).\nNote: Maximum value is 15", NULL, NULL, GF_ARG_INT, 0),
	{0}
};

void PrintImportUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Importing Options\n"
		"# File importing\n"
		"Syntax is [-add]() / [-cat]() `filename[#FRAGMENT][:opt1...:optN=val]`\n"
		"This process will create the destination file if not existing, and add the track(s) to it. If you wish to always create a new destination file, add [-new](MP4B_GEN).\n"
		"The supported input media types depend on your installation, check [filters documentation](Filters) for more info.\n"
		"  \n"
		"To select a desired media track, the following syntax is used:\n"
		"- `-add inputFile#video`: adds the first video track in `inputFile`. DOES NOT WORK for IsoMedia nor MPEG-2 TS files.\n"
		"- `-add inputFile#audio`: adds the first audio track in `inputFile`. DOES NOT WORK for IsoMedia nor MPEG-2 TS files.\n"
		"- `-add inputFile#trackID=ID` or `-add inputFile#ID`: adds the specified track. For IsoMedia files, ID is the track ID. For other media files, ID is the value indicated by `MP4Box -info inputFile`.\n"
		"  \n"
		"MP4Box can import a desired amount of the input file rather than the whole file. To do this, use the syntax `-add inputFileN`, where N is the number of seconds you wish to import from input. MP4Box cannot start importing from a random point in the input, it always import from the begining.\n"
		"Note: When importing SRT or SUB files, MP4Box will choose default layout options to make the subtitle appear at the bottom of the video. You SHOULD NOT import such files before any video track is added to the destination file, otherwise the results will likelly not be useful (default SRT/SUB importing uses default serif font, fontSize 18 and display size 400x60). Check [TTXT doc](Subtitling-with-GPAC) for more details.\n"
		"  \n"
		"When importing several tracks/sources in one pass, all options will be applied if relevant to each source. These options are set for all imported streams. If you need to specify these options par stream, set per-file options using the syntax `-add stream[:opt1:...:optN]`.\n"
		"  \n"
		""
		"Allowed per-file options:\n"
		"- #video, #audio: base import for most AV files\n"
		"- #trackID=ID: track import for IsoMedia and other files\n"
		"- #pid=ID: stream import from MPEG-2 TS\n"
		"- :dur=D: import only the first D seconds\n"
		"- :lang=LAN: set imported media language code\n"
		"- :delay=delay_ms: set imported media initial delay in ms\n"
		"- :par=PAR: set visual pixel aspect ratio (see [-par](MP4B_GEN) )\n"
		"- :clap=CLAP: set visual clean aperture (see [-clap](MP4B_GEN) )\n"
		"- :mx=MX: sets track matrix (see [-mx](MP4B_GEN) )\n"
		"- :name=NAME: set track handler name\n"
		"- :ext=EXT: override file extension when importing\n"
		"- :hdlr=code: set track handler type to the given code point (4CC)\n"
		"- :disable: imported track(s) will be disabled\n"
		"- :group=G: add the track as part of the G alternate group. If G is 0, the first available GroupID will be picked.\n"
		"- :fps=VAL: same as [-fps]()\n"
		"- :rap: import only RAP samples\n"
		"- :refs: import only reference pictures\n"
		"- :trailing: keep trailing 0-bytes in AVC/HEVC samples\n"
		"- :agg=VAL: same as [-agg]()\n"
		"- :dref: same as [-dref]()\n"
		"- :keep_refs: keeps track reference when importing a single track\n"
		"- :nodrop: same as [-nodrop]()\n"
		"- :packed: same as [-packed]()\n"
		"- :sbr: same as [-sbr]()\n"
		"- :sbrx: same as [-sbrx]()\n"
		"- :ovsbr: same as [-ovsbr]()\n"
		"- :ps: same as [-ps]()\n"
		"- :psx: same as [-psx]()\n"
		"- :asemode=MODE: set the mode to create the AudioSampleEntry"
		" - v0-bs: use MPEG AudioSampleEntry v0 and the channel count from the bitstream (even if greater than 2) - default\n"
		" - v0-2: use MPEG AudioSampleEntry v0 and the channel count is forced to 2\n"
		" - v1: use MPEG AudioSampleEntry v1 and the channel count from the bitstream\n"
		" - v1-qt: use QuickTime Sound Sample Description Version 1 and the channel count from the bitstream (even if greater than 2)\n"
		"- :audio_roll=N: add a roll sample group with roll_distance `N`\n"
		"- :mpeg4: same as [-mpeg4]() option\n"
		"- :nosei: discard all SEI messages during import\n"
		"- :svc: import SVC/LHVC with explicit signaling (no AVC base compatibility)\n"
		"- :nosvc: discard SVC/LHVC data when importing\n"
		"- :svcmode=MODE: set SVC/LHVC import mode\n"
		" - split: each layer is in its own track\n"
		" - merge: all layers are merged in a single track\n"
		" - splitbase: all layers are merged in a track, and the AVC base in another\n"
		" - splitnox: each layer is in its own track, and no extractors are written\n"
		" - splitnoxib: each layer is in its own track, no extractors are written, using inband param set signaling\n"
		"- :temporal: set HEVC/LHVC temporal sublayer import mode\n"
		" - split: each sublayer is in its own track\n"
		" - splitbase: all sublayers are merged in a track, and the HEVC base in another\n"
		" - splitnox: each layer is in its own track, and no extractors are written\n"
		"- :subsamples: add SubSample information for AVC+SVC\n"
		"- :deps: import sample dependency information for AVC and HEVC\n"
		"- :ccst: add default HEIF ccst box to visual sample entry\n"
		"- :forcesync: force non IDR samples with I slices to be marked as sync points (AVC GDR)\n"
		"Warning: RESULTING FILE IS NOT COMPLIANT WITH THE SPEC but will fix seeking in most players\n"
		"- :xps_inband: set xPS inband for AVC/H264 and HEVC (for reverse operation, re-import from raw media)\n"
		"- :xps_inbandx: same as xps_inband and also keep first xPS in sample desciption\n"
		"- :au_delim: keep AU delimiter NAL units in the imported file\n"
		"- :max_lid=N: set HEVC max layer ID to be imported to `N` (by default imports all layers).\n"
		"- :max_tid=N: set HEVC max temporal ID to be imported to `N` (by default imports all temporal sublayers)\n"
		"- :tiles: add HEVC tiles signaling and NALU maps without splitting the tiles into different tile tracks.\n"
		"- :split_tiles: split HEVC tiles into different tile tracks, one tile (or all tiles of one slice) per track.\n"
		"- :negctts: use negative CTS-DTS offsets (ISO4 brand)\n"
		"- :chap: specify the track is a chapter track\n"
		"- :chapter=NAME: add a single chapter (old nero format) with given name lasting the entire file"
		"- :chapfile=file: adds a chapter file (old nero format)"
		"- :layout=WxHxXxY: specify the track layout\n"
		" - if W (resp H) = 0, the max width (resp height) of the tracks in the file are used.\n"
		" - if Y=-1, the layout is moved to the bottom of the track area\n"
		" - X and Y can be omitted (:layout=WxH)\n"
		"- :rescale=TS: force media timescale to TS !! changes the media duration\n"
		"- :timescale=TS: set imported media timescale to TS.\n"
		"- :moovts=TS: set movie timescale to TS. A negative value picks the media timescale of the first track imported.\n"
		"- :noedit: do not set edit list when importing B-frames video tracks\n"
		"- :rvc=FILENAME: set RVC configuration for the media\n"
		"- :fmt=FORMAT: override format detection with given format (cf BT/XMTA doc)\n"
		"- :profile=INT: override AVC profile\n"
		"- :level=INT: override AVC level\n"
		"- :novpsext: remove VPS extensions from HEVC VPS\n"
		"- :keepav1t: keep AV1 temporal delimiter OBU in samples, might help if source file had losses\n"
		"- :font=name: specify font name for text import (default `Serif`)\n"
		"- :size=s: specify font size for text import (default `18`)\n"
		"- :text_layout=WxHxXxY: specify the track text layout\n"
		" - if W (resp H) = 0, the max width (resp height) of the tracks in the file are used.\n"
		" - if Y=-1, the layout is moved to the bottom of the track area\n"
		" - X and Y can be omitted (:layout=WxH)\n"
		"- :swf-global: all SWF defines are placed in first scene replace rather than when needed\n"
		"- :swf-no-ctrl: use a single stream for movie control and dictionary (this will disable ActionScript)\n"
		"- :swf-no-text: remove all SWF text\n"
		"- :swf-no-font: remove all embedded SWF Fonts (local playback host fonts used)\n"
		"- :swf-no-line: remove all lines from SWF shapes\n"
		"- :swf-no-grad: remove all gradients from SWF shapes\n"
		"- :swf-quad: use quadratic bezier curves instead of cubic ones\n"
		"- :swf-xlp: support for lines transparency and scalability\n"
		"- :swf-ic2d: use indexed curve 2D hardcoded proto\n"
		"- :swf-same-app: appearance nodes are reused\n"
		"- :swf-flatten=ang: complementary angle below which 2 lines are merged, `0` means no flattening\n"
		"- :kind=schemeURI=value: set kind for the track\n"
		"- :txtflags=flags: set display flags (hexa number) of text track\n"
		"- :txtflags+=flags: add display flags (hexa number) to text track\n"
		"- :txtflags-=flags: remove display flags (hexa number) from text track\n"
		"- :rate=VAL: force average rate and max rate to VAL (in bps) in btrt box. If 0, removes btrt box\n"
		"- :stz2: use compact size table (for low-bitrates)\n"
		"- :bitdepth=VAL: set bit depth to VAL for imported video content (default is 24)\n"
		"- :colr=OPT: set color profile for imported video content (see ISO/IEC 23001-8). `OPT` is formatted as:\n"
		" - nclc,p,t,m: with p colour primary, t transfer characteristics and m matrix coef\n"
		" - nclx,p,t,m,r: same as `nclx` with r full range flag\n"
		" - prof,path: with path indicating the file containing the ICC color profile\n"
		" - rICC,path: with path indicating the file containing the restricted ICC color profile\n"
		"- :fstat: print filter session stats after import\n"
		"- :fgraph: print filter session graph after import\n"
		"- :sopt:[OPTS]: set `OPTS` as additionnal arguments to source filter. `OPTS` can be any usual filter argument, see [filter doc `gpac -h doc`](Filters)\n"
		"- :dopt:[OPTS]: set `OPTS` as additionnal arguments to [destination filter](mp4mx). OPTS can be any usual filter argument, see [filter doc `gpac -h doc`](Filters)\n"
		"- @@f1[:args][@@fN:args]: set a filter chain to insert before the muxer. Each filter in the chain is formatted as a regular filter, see [filter doc `gpac -h doc`](Filters). If several filters are set, they will be chained in the given order. The last filter shall not have any Filter ID specified\n"
		"\n"
		"Note: `sopt`, `dopt` and `@@f` must be placed after all other options.\n"
		"# Global import options\n"
	);
	while (m4b_imp_args[i].name) {
		GF_GPACArg *arg = &m4b_imp_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-import");
	}
}

GF_GPACArg m4b_senc_args[] =
{
 	GF_DEF_ARG("mp4", NULL, "specify input file is for encoding", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("def", NULL, "encode DEF names in BIFS", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("sync", NULL, "force BIFS sync sample generation every given time in ms.\nNote: cannot be used with [-shadow]()", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("shadow", NULL, "force BIFS sync shadow sample generation every given time in ms.\nNote: cannot be used with [-sync]()", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("log", NULL, "generate scene codec log file if available", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ms", NULL, "import tracks from the given file", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("ctx-in", NULL, "specify initial context (MP4/BT/XMT) file for chunk processing. Input file must be a commands-only file", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("ctx-out", NULL, "specify storage of updated context (MP4/BT/XMT) file for chunk processing, optionnal", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("resolution", NULL, "resolution factor (-8 to 7, default 0) for LASeR encoding, and all coords are multiplied by `2^res` before truncation (LASeR encoding)", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("coord-bits", NULL, "number of bits used for encoding truncated coordinates (0 to 31, default 12) (LASeR encoding)", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("scale-bits", NULL, "extra bits used for encoding truncated scales (0 to 4, default 0) (LASeR encoding)", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("auto-quant", NULL, "resolution is given as if using -resolution but coord-bits and scale-bits are infered (LASeR encoding)", NULL, NULL, GF_ARG_INT, 0),
 	{0}
};


void PrintEncodeUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# MPEG-4 Scene Encoding Options\n"
		"## General considerations\n"
		"MP4Box supports encoding and decoding of of BT, XMT, VRML and (partially) X3D formats int MPEG-4 BIFS, and encoding and decoding of XSR and SVG into MPEG-4 LASeR\n"
		"Any media track specified through a `MuxInfo` element will be imported in the resulting MP4 file.\n"
		"See https://github.com/gpac/gpac/wiki/MPEG-4-BIFS-Textual-Format and related pages.\n"
		"## Scene Random Access\n"
		"MP4Box can encode BIFS or LASeR streams and insert random access points at a given frequency. This is useful when packaging content for broadcast, where users will not turn in the scene at the same time. In MPEG-4 terminology, this is called the __scene carousel__."
		"## BIFS Chunk Processing\n"
		"The BIFS chunk encoding mode alows encoding single BIFS access units from an initial context and a set of commands.\n"
		"The generated AUs are raw BIFS (not SL-packetized), in files called FILE-ESID-AUIDX.bifs, with FILE the basename of the input file.\n"
		"Commands with a timing of 0 in the input will modify the carousel version only (i.e. output context).\n"
		"Commands with a timing different from 0 in the input will generate new AUs.\n"
		"  \n"
		"Options:\n"
	);

	while (m4b_senc_args[i].name) {
		GF_GPACArg *arg = &m4b_senc_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-senc");
	}
}

GF_GPACArg m4b_crypt_args[] =
{
 	GF_DEF_ARG("crypt", NULL, "encrypt the input file using the given CryptFile", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("decrypt `[drm_file]`", NULL, "decrypt the input file, potentially using the given CryptFile. If not given, will fail if the key management system is not supported", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("set-kms", NULL, "change ISMA/OMA KMS location for all tracks, or for a given one if `ID=kms_uri`is used", NULL, NULL, GF_ARG_STRING, 0),
 	{0}
};

void PrintEncryptUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Encryption/Decryption Options\n"
	"MP4Box supports encryption and decryption of ISMA, OMA and CENC content, see [encryption filter `gpac -h cecrypt`](cecrypt).\n"
	"It requires a specific XML file called `CryptFile`, whose syntax is available at https://github.com/gpac/gpac/wiki/Common-Encryption\n"
	"  \n"
	"Options:\n"
	);
	while (m4b_crypt_args[i].name) {
		GF_GPACArg *arg = &m4b_crypt_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-crypt");
	}
}

GF_GPACArg m4b_hint_args[] =
{
 	GF_DEF_ARG("hint", NULL, "hint the file for RTP/RTSP", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("hint", NULL, "specify RTP MTU (max size) in bytes (this includes 12 bytes RTP header)", "1450", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("copy", NULL, "copy media data to hint track rather than reference (speeds up server but takes much more space)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("multi `[maxptime]`", NULL, "enable frame concatenation in RTP packets if possible (with max duration 100 ms or `maxptime` ms if given)", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("rate", NULL, "specify rtp rate in Hz when no default for payload", "90000", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("mpeg4", NULL, "force MPEG-4 generic payload whenever possible", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("latm", NULL, "force MPG4-LATM transport for AAC streams", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("static", NULL, "enable static RTP payload IDs whenever possible (by default, dynamic payloads are always used)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("add-sdp", NULL, "add given SDP string to hint track (`ID:string`) or movie (`string`)", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("unhint", NULL, "remove all hinting information from file", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("group-single", NULL, "put all tracks in a single hint group", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ocr", NULL, "force all MPEG-4 streams to be synchronized (MPEG-4 Systems only)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("rap", NULL, "signal random access points in RTP packets (MPEG-4 Systems)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ts", NULL, "signal AU Time Stamps in RTP packets (MPEG-4 Systems)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("size", NULL, "signal AU size in RTP packets (MPEG-4 Systems)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("idx", NULL, "signal AU sequence numbers in RTP packets (MPEG-4 Systems)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("iod", NULL, "prevent systems tracks embedding in IOD (MPEG-4 Systems), not compatible with [-isma]()", NULL, NULL, GF_ARG_BOOL, 0),
 	{0}
};

void PrintHintUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Hinting Options\n"
		"IsoMedia hinting consists in creating special tracks in the file that contain transport protocol specific information and optionally multiplexing information. These tracks are then used by the server to create the actual packets being sent over the network, in other words they provide the server with hints on how to build packets, hence their names `hint tracks`.\n"
		"MP4Box supports creation of hint tracks for RTSP servers supporting these such as QuickTime Streaming Server, DarwinStreaming Server or 3GPP-compliant RTSP servers.\n"
		"Note: GPAC streaming tools [rtp output](rtpout) and [rtsp server](rtspout) do not use hint tracks, they use on-the-fly packetization "
		"from any media sources, not just MP4\n"
		"  \n"
		"Options:\n"
	);
	while (m4b_hint_args[i].name) {
		GF_GPACArg *arg = &m4b_hint_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-hint");
	}
}


GF_GPACArg m4b_extr_args[] =
{
 	GF_DEF_ARG("raw", NULL, "extract given track in raw format when supported. Use `ID:output=FileName` to set output file name", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("raws", NULL, "extract each sample of the given track to a file. Use `ID:N`to extract the Nth sample", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("nhnt", NULL, "extract given track to [NHNT](nhntr) format", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("nhml", NULL, "extract given track to [NHML](nhmlr) format. Use `ID:full` for full NHML dump", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("webvtt-raw", NULL, "extract given track as raw media in WebVTT as metadata. Use `ID:embedded` to include media data in the WebVTT file", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("single", NULL, "extract given track to a new mp4 file", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("six", NULL, "extract given track as raw media in **experimental** XML streaming instructions", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("avi", NULL, "extract given track to an avi file", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("avi", NULL, "same as [-raw]() but defaults to QCP file for EVRC/SMV", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("aviraw", NULL, "extract AVI track in raw format; parameter can be `video`, `audio`or `audioN`", NULL, "video|audio", GF_ARG_STRING, 0),
 	GF_DEF_ARG("saf", NULL, "remux file to SAF multiplex", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dvbhdemux", NULL, "demux DVB-H file into IP Datagrams sent on the network", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("raw-layer", NULL, "same as [-raw]() but skips SVC/MVC/LHVC extractors when extracting", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("diod", NULL, "extract file IOD in raw format", NULL, NULL, GF_ARG_BOOL, 0),
 	{0}
};

void PrintExtractUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Extracting Options\n"
	"MP4Box can be used to extract media tracks from MP4 files. If you need to convert these tracks however, please check the [filters doc](Filters).\n"
	"  \n"
	"Options:\n"
	);
	while (m4b_extr_args[i].name) {
		GF_GPACArg *arg = &m4b_extr_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

GF_GPACArg m4b_dump_args[] =
{
 	GF_DEF_ARG("stdb", NULL, "dump/write to stdout and assume stdout is opened in binary mode", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("stdb", NULL, "dump/write to stdout  and try to reopen stdout in binary mode", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("info", NULL, "print movie info (no parameter) or track info with specified ID", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("diso", NULL, "dump IsoMedia file boxes in XML output", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dxml", NULL, "dump IsoMedia file boxes and known track samples in XML output", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("bt", NULL, "dump scene to BT format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("xmt", NULL, "dump scene to XMT format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("wrl", NULL, "dump scene to VRML format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("x3d", NULL, "dump scene to X3D XML format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("x3dc", NULL, "dump scene to X3D VRML format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("lsr", NULL, "dump scene to LASeR XML (XSR) format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("drtp", NULL, "dump rtp hint samples structure to XML output", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dts", NULL, "print sample timing, size and position in file to text output", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dtsx", NULL, "same as [-dts]() but does not print offset", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dtsc", NULL, "same as [-dts]() but analyse each sample for duplicated dts/cts (__slow !__)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dtsxc", NULL, "same as [-dtsc]() but does not print offset (__slow !__)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dnal", NULL, "print NAL sample info of given track", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("dnalc", NULL, "print NAL sample info of given track, adding CRC for each nal", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("sdp", NULL, "dump SDP description of hinted file", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dsap", NULL, "dump DASH SAP cues (see -cues) for a given track", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("dsaps", NULL, "same as [-dsap]() but only print sample number", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("dsapc", NULL, "same as [-dsap]() but only print CTS", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("dsapd", NULL, "same as [-dsap]() but only print DTS, `-dsapp` to only print presentation time", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("dsapp", NULL, "same as [-dsap]() but only print presentation time", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("dcr", NULL, "dump ISMACryp samples structure to XML output", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dump-cover", NULL, "extract cover art", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dump-chap", NULL, "extract chapter file", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dump-chap-ogg", NULL, "extract chapter file as OGG format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("dump-udta `[ID:]4cc`", NULL, "extract udta for the given 4CC. If `ID` is given, dumps from UDTA of the given track ID, otherwise moov is used", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("mergevtt", NULL, "merge vtt cues while dumping", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ttxt", NULL, "convert input subtitle to GPAC TTXT format if no parameter. Otherwise, dump given text track to GPAC TTXT format", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("srt", NULL, "convert input subtitle to SRT format if no parameter. Otherwise, dump given text track to SRT format", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("rip-mpd", NULL, "download manifest and segments of an MPD. Does not work with live sessions", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("stat", NULL, "generate node/field statistics for scene", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("stats", NULL, "generate node/field statistics per Access Unit", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("statx", NULL, "generate node/field statistics for scene after each AU", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("hash", NULL, "generate SHA-1 Hash of the input file", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("comp", NULL, "replace with compressed version all top level box types given as parameter, formated as `orig_4cc_1=comp_4cc_1[,orig_4cc_2=comp_4cc_2]`", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("bin", NULL, "convert input XML file using NHML bitstream syntax to binary", NULL, NULL, GF_ARG_BOOL, 0),
 	{0}
};

void PrintDumpUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# File Dumping\n"
	"  \n"
	"MP4Box has many dump functionalities, from simple track listing to more complete reporting of special tracks.\n"
	"  \n"
	"Options:\n"
	);
	while (m4b_dump_args[i].name) {
		GF_GPACArg *arg = &m4b_dump_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

GF_GPACArg m4b_meta_args[] =
{
 	GF_DEF_ARG("set-meta `ABCD[:tk=ID]`", NULL, "set meta box type, with `ABCD` the four char meta type (NULL or 0 to remove meta)\n"
		"- tk not set: use root (file) meta\n"
		"- ID == 0: use moov meta\n"
		"- ID != 0: use meta of given track", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("add-items", NULL, "add resource to meta, with parameter syntax `file_path[:opt1:optN]`\n"
		"- file_path `this` or `self`: item is the file itself\n"
		"- tk=ID: meta location (file, moov, track)\n"
		"- name=str: item name\n"
		"- type=itype: item 4cc type (not needed if mime is provided)\n"
		"- mime=mtype: item mime type\n"
		"- encoding=enctype: item content-encoding type\n"
		"- id=ID: item ID\n"
		"- ref=4cc,id: reference of type 4cc to an other item", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("add-image", NULL, "add the given file (with parameters) as HEIF image item. Same syntax as [-add-item]()\n"
		"- name=str: see [-add-item]()\n"
		"- id=id: see [-add-item]()\n"
		"- ref=4cc, id: see [-add-item]()\n"
		"- primary: indicate that this item should be the primary item\n"
		"- time=t: use the next sync sample after time t (float, in sec, default 0). A negative time imports ALL frames as items\n"
		"- split_tiles: for an HEVC tiled image, each tile is stored as a separate item\n"
		"- rotation=a: set the rotation angle for this image to 90*a degrees anti-clockwise\n"
		"- hidden: indicate that this image item should be hidden\n"
		"- icc_path: path to icc to add as colr\n"
		"- alpha: indicate that the image is an alpha image (should use ref=auxl also)\n"
		"- any other option will be passed as options to the media importer, see [-add]()", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("rem-item `item_ID[:tk=ID]`", NULL, "remove resource from meta", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("set-primary `item_ID[:tk=ID]`", NULL, "set item as primary for meta", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("set-xml `xml_file_path[:tk=ID][:binary]`", NULL, "set meta XML data", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("rem-xml `[tk=ID]`", NULL, "remove meta XML data", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("dump-xml `file_path[:tk=ID]`", NULL, "dump meta XML to file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("dump-item `item_ID[:tk=ID][:path=fileName]`", NULL, "dump item to file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("package", NULL, "package input XML file into an ISO container, all media referenced except hyperlinks are added to file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("package", NULL, "package input XML file into an MPEG-U widget with ISO container, all files contained in the current folder are added to the widget package", NULL, NULL, GF_ARG_STRING, 0),
	{0}
};

void PrintMetaUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Meta and HEIF Options\n"
	"IsoMedia files can be used as generic meta-data containers, for examples storing XML information and sample images for a movie. The resulting file may not always contain a movie as is the case with some HEIF files or MPEG-21 files.\n"
	"  \n"
	"These information can be stored at the file root level, as is the case for HEIF/IFF and MPEG-21 file formats, or at the moovie or track level for a regular movie."
	"  \n  \n");
	while (m4b_meta_args[i].name) {
		GF_GPACArg *arg = &m4b_meta_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

GF_GPACArg m4b_swf_args[] =
{
 	GF_DEF_ARG("global", NULL, "all SWF defines are placed in first scene replace rather than when needed", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-ctrl", NULL, "use a single stream for movie control and dictionary (this will disable ActionScript)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-text", NULL, "remove all SWF text", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-font", NULL, "remove all embedded SWF Fonts (local playback host fonts used)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-line", NULL, "remove all lines from SWF shapes", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-grad", NULL, "remove all gradients from swf shapes", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("quad", NULL, "use quadratic bezier curves instead of cubic ones", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("xlp", NULL, "support for lines transparency and scalability", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("flatten", NULL, "complementary angle below which 2 lines are merged, value `0`means no flattening", NULL, NULL, GF_ARG_DOUBLE, 0),
	{0}
};

void PrintSWFUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# SWF Importer Options\n"
	        "\n"
	        "MP4Box can import simple Macromedia Flash files (\".SWF\")\n"
	        "You can specify a SWF input file with \'-bt\', \'-xmt\' and \'-mp4\' options\n"
	        "  \n"
	        "Options:\n"
	);
	while (m4b_swf_args[i].name) {
		GF_GPACArg *arg = &m4b_swf_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

GF_GPACArg m4b_atsc_args[] =
{
 	GF_DEF_ARG("atsc", NULL, "enable ATSC 3.0 reader", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("ifce", NULL, "IP address of network interface to use", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("dir", NULL, "local filesystem path to which the files are written. If not set, nothing is written to disk", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("service", NULL, "ID of the service to grab\n"
 	"- not set or -1: all services are dumped\n"
 	"- 0: no services are dumped\n"
 	"- -2: the first service found is used\n"
 	"- positive: tunes to given service ID", NULL, NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("nb-segs", NULL, "set max segments to keep on disk per stream, `-1` keeps all", "-1", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("atsc-stats", NULL, "print stats every N seconds", NULL, NULL, GF_ARG_INT, 0),
 	{0}
};

void PrintATSCUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# ATSC 3.0 Grabber Options\n"
	        "MP4Box can be used to grab files from an ATSC 3.0 ROUTE session and records them to disk.\n"
	        "  \n"
	        "Note: On OSX with VM packet replay you will need to force mcast routing\n"
	        "EX route add -net 239.255.1.4/32 -interface vboxnet0\n"
	        "  \n"
	        "Options\n"
	);
	while (m4b_atsc_args[i].name) {
		GF_GPACArg *arg = &m4b_atsc_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

GF_GPACArg m4b_liveenc_args[] =
{
 	GF_DEF_ARG("dst", NULL, "destination IP", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("port", NULL, "destination port", "7000", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("mtu", NULL, "path MTU for RTP packets", "1450", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("ifce", NULL, "IP address of the physical interface to use", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("ttl", NULL, "time to live for multicast packets", "1", NULL, GF_ARG_INT, 0),
 	GF_DEF_ARG("sdp", NULL, "output SDP file", "session.sdp", NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("dims", NULL, "turn on DIMS mode for SVG input", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("no-rap", NULL, "disable RAP sending and carousel generation", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("src", NULL, "source of scene updates", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("rap", NULL, "duration in ms of base carousel; you can specify the RAP period of a single ESID (not in DIMS) using `ESID=X:time`", NULL, NULL, GF_ARG_INT, 0),
 	{0}
};

void PrintLiveUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Live Scene Encoder Options\n"
	        "The options shall be specified as òpt_name=opt_val.\n"
	        "Options:\n"
	        "\n"
	);
	while (m4b_liveenc_args[i].name) {
		GF_GPACArg *arg = &m4b_liveenc_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}

	gf_sys_format_help(helpout, help_flags, "  \n"
		"Runtime options:\n"
		"- q: quits\n"
		"- u: inputs some commands to be sent\n"
		"- U: same as u but signals the updates as critical\n"
		"- e: inputs some commands to be sent without being aggregated\n"
		"- E: same as e but signals the updates as critical\n"
		"- f: forces RAP sending\n"
		"- F: forces RAP regeneration and sending\n"
		"- p: dumps current scene\n"
	);
}

void PrintCoreUsage()
{
	gf_sys_format_help(helpout, help_flags, "# libgpac core options\n");
	gf_sys_print_core_help(NULL, 0, GF_ARGMODE_ALL, 0);
}

GF_GPACArg m4b_usage_args[] =
{
 	GF_DEF_ARG("h", NULL, "print help\n"
 		"- general: general options help\n"
		"- hint: hinting options help\n"
		"- dash: DASH segmenter help\n"
		"- import: import options help\n"
		"- encode: encode options help\n"
		"- meta: meta handling options help\n"
		"- extract: extraction options help\n"
		"- dump: dump options help\n"
		"- swf: Flash (SWF) options help\n"
		"- crypt: ISMA E&A options help\n"
		"- format: supported formats help\n"
		"- live: BIFS streamer help\n"
		"- atsc: ATSC3 reader help\n"
		"- core: libgpac core options\n"
		"- all: all options are printed\n", NULL, NULL, GF_ARG_STRING, 0),

 	GF_DEF_ARG("nodes", NULL, "list supported MPEG4 nodes", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("node", NULL, "get given MPEG4 node syntax and QP infolist", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("xnodes", NULL, "list supported X3D nodes", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("xnode", NULL, "get given X3D node syntax", NULL, NULL, GF_ARG_STRING, 0),
 	GF_DEF_ARG("snodes", NULL, "list supported SVG nodes", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("languages", NULL, "list supported ISO 639 languages", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("boxes", NULL, "list all supported ISOBMF boxes and their syntax", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("fstat", NULL, "print filter session statistics (import/export/encrypt/decrypt/dashing)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("fgraph", NULL, "print filter session graph (import/export/encrypt/decrypt/dashing)", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("v", NULL, "verbose mode", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("version", NULL, "get build version", NULL, NULL, GF_ARG_BOOL, 0),
 	GF_DEF_ARG("-- INPUT", NULL, "escape option if INPUT starts with `-` character", NULL, NULL, GF_ARG_BOOL, 0),
 	{0}
};

void PrintUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "MP4Box [option] input [option]\n"
		"  \n"
		"# General Options:\n"
	);
	while (m4b_usage_args[i].name) {
		GF_GPACArg *arg = &m4b_usage_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-general");
	}
}


void scene_coding_log(void *cbk, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char *fmt, va_list vlist)
{
	FILE *logs = cbk;
	if (log_tool != GF_LOG_CODING) return;
	vfprintf(logs, fmt, vlist);
	fflush(logs);
}

#ifndef GPAC_DISABLE_ISOM_HINTING

/*
		MP4 File Hinting
*/

void SetupClockReferences(GF_ISOFile *file)
{
	u32 i, count, ocr_id;
	count = gf_isom_get_track_count(file);
	if (count==1) return;
	ocr_id = 0;
	for (i=0; i<count; i++) {
		if (!gf_isom_is_track_in_root_od(file, i+1)) continue;
		ocr_id = gf_isom_get_track_id(file, i+1);
		break;
	}
	/*doesn't look like MP4*/
	if (!ocr_id) return;
	for (i=0; i<count; i++) {
		GF_ESD *esd = gf_isom_get_esd(file, i+1, 1);
		if (esd) {
			esd->OCRESID = ocr_id;
			gf_isom_change_mpeg4_description(file, i+1, 1, esd);
			gf_odf_desc_del((GF_Descriptor *) esd);
		}
	}
}

/*base RTP payload type used (you can specify your own types if needed)*/
#define BASE_PAYT		96

GF_Err HintFile(GF_ISOFile *file, u32 MTUSize, u32 max_ptime, u32 rtp_rate, u32 base_flags, Bool copy_data, Bool interleave, Bool regular_iod, Bool single_group)
{
	GF_ESD *esd;
	GF_InitialObjectDescriptor *iod;
	u32 i, val, res, streamType;
	u32 sl_mode, prev_ocr, single_ocr, nb_done, tot_bw, bw, flags, spec_type;
	GF_Err e;
	char szPayload[30];
	GF_RTPHinter *hinter;
	Bool copy, has_iod, single_av;
	u8 init_payt = BASE_PAYT;
	u32 mtype;
	GF_SDP_IODProfile iod_mode = GF_SDP_IOD_NONE;
	u32 media_group = 0;
	u8 media_prio = 0;

	tot_bw = 0;
	prev_ocr = 0;
	single_ocr = 1;

	has_iod = 1;
	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(file);
	if (!iod) has_iod = 0;
	else {
		if (!gf_list_count(iod->ESDescriptors)) has_iod = 0;
		gf_odf_desc_del((GF_Descriptor *) iod);
	}

	spec_type = gf_isom_guess_specification(file);
	single_av = single_group ? 1 : gf_isom_is_single_av(file);

	/*first make sure we use a systems track as base OCR*/
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		res = gf_isom_get_media_type(file, i+1);
		if ((res==GF_ISOM_MEDIA_SCENE) || (res==GF_ISOM_MEDIA_OD)) {
			if (gf_isom_is_track_in_root_od(file, i+1)) {
				gf_isom_set_default_sync_track(file, i+1);
				break;
			}
		}
	}

	nb_done = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		sl_mode = base_flags;
		copy = copy_data;
		/*skip emty tracks (mainly MPEG-4 interaction streams...*/
		if (!gf_isom_get_sample_count(file, i+1)) continue;
		if (!gf_isom_is_track_enabled(file, i+1)) {
			fprintf(stderr, "Track ID %d disabled - skipping hint\n", gf_isom_get_track_id(file, i+1) );
			continue;
		}

		mtype = gf_isom_get_media_type(file, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_VISUAL:
			if (single_av) {
				media_group = 2;
				media_prio = 2;
			}
			break;
        case GF_ISOM_MEDIA_AUXV:
            if (single_av) {
                media_group = 2;
                media_prio = 3;
            }
            break;
        case GF_ISOM_MEDIA_PICT:
            if (single_av) {
                media_group = 2;
                media_prio = 4;
            }
            break;
		case GF_ISOM_MEDIA_AUDIO:
			if (single_av) {
				media_group = 2;
				media_prio = 1;
			}
			break;
		case GF_ISOM_MEDIA_HINT:
			continue;
		default:
			/*no hinting of systems track on isma*/
			if (spec_type==GF_ISOM_BRAND_ISMA) continue;
		}
		mtype = gf_isom_get_media_subtype(file, i+1, 1);
		if ((mtype==GF_ISOM_SUBTYPE_MPEG4) || (mtype==GF_ISOM_SUBTYPE_MPEG4_CRYP) ) mtype = gf_isom_get_mpeg4_subtype(file, i+1, 1);

		if (!single_av) {
			/*one media per group only (we should prompt user for group selection)*/
			media_group ++;
			media_prio = 1;
		}

		streamType = 0;
		esd = gf_isom_get_esd(file, i+1, 1);
		if (esd) {
			streamType = esd->decoderConfig->streamType;
			if (!prev_ocr) {
				prev_ocr = esd->OCRESID;
				if (!esd->OCRESID) prev_ocr = esd->ESID;
			} else if (esd->OCRESID && prev_ocr != esd->OCRESID) {
				single_ocr = 0;
			}
			/*OD MUST BE WITHOUT REFERENCES*/
			if (streamType==1) copy = 1;
		}
		gf_odf_desc_del((GF_Descriptor *) esd);

		if (!regular_iod && gf_isom_is_track_in_root_od(file, i+1)) {
			/*single AU - check if base64 would fit in ESD (consider 33% overhead of base64), otherwise stream*/
			if (gf_isom_get_sample_count(file, i+1)==1) {
				GF_ISOSample *samp = gf_isom_get_sample(file, i+1, 1, &val);
				if (streamType) {
					res = gf_hinter_can_embbed_data(samp->data, samp->dataLength, streamType);
				} else {
					/*not a system track, we shall hint it*/
					res = 0;
				}
				if (samp) gf_isom_sample_del(&samp);
				if (res) continue;
			}
		}
		if (interleave) sl_mode |= GP_RTP_PCK_USE_INTERLEAVING;

		hinter = gf_hinter_track_new(file, i+1, MTUSize, max_ptime, rtp_rate, sl_mode, init_payt, copy, media_group, media_prio, &e);

		if (!hinter) {
			if (e) {
				fprintf(stderr, "Cannot create hinter (%s)\n", gf_error_to_string(e));
				if (!nb_done) return e;
			}
			continue;
		}
		bw = gf_hinter_track_get_bandwidth(hinter);
		tot_bw += bw;
		flags = gf_hinter_track_get_flags(hinter);

		//set extraction mode for AVC/SVC
		gf_isom_set_nalu_extract_mode(file, i+1, GF_ISOM_NALU_EXTRACT_LAYER_ONLY);

		gf_hinter_track_get_payload_name(hinter, szPayload);
		fprintf(stderr, "Hinting track ID %d - Type \"%s:%s\" (%s) - BW %d kbps\n", gf_isom_get_track_id(file, i+1), gf_4cc_to_str(mtype), gf_4cc_to_str(mtype), szPayload, bw);
		if (flags & GP_RTP_PCK_SYSTEMS_CAROUSEL) fprintf(stderr, "\tMPEG-4 Systems stream carousel enabled\n");
		/*
				if (flags & GP_RTP_PCK_FORCE_MPEG4) fprintf(stderr, "\tMPEG4 transport forced\n");
				if (flags & GP_RTP_PCK_USE_MULTI) fprintf(stderr, "\tRTP aggregation enabled\n");
		*/
		e = gf_hinter_track_process(hinter);

		if (!e) e = gf_hinter_track_finalize(hinter, has_iod);
		gf_hinter_track_del(hinter);

		if (e) {
			fprintf(stderr, "Error while hinting (%s)\n", gf_error_to_string(e));
			if (!nb_done) return e;
		}
		init_payt++;
		nb_done ++;
	}

	if (has_iod) {
		iod_mode = GF_SDP_IOD_ISMA;
		if (regular_iod) iod_mode = GF_SDP_IOD_REGULAR;
	} else {
		iod_mode = GF_SDP_IOD_NONE;
	}
	gf_hinter_finalize(file, iod_mode, tot_bw);

	if (!single_ocr)
		fprintf(stderr, "Warning: at least 2 timelines found in the file\nThis may not be supported by servers/players\n\n");

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_HINTING*/

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_AV_PARSERS)

static void check_media_profile(GF_ISOFile *file, u32 track)
{
	u8 PL;
	GF_M4ADecSpecInfo dsi;
	GF_ESD *esd = gf_isom_get_esd(file, track, 1);
	if (!esd) return;

	switch (esd->decoderConfig->streamType) {
	case 0x04:
		PL = gf_isom_get_pl_indication(file, GF_ISOM_PL_VISUAL);
		if (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			if (dsi.VideoPL > PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, dsi.VideoPL);
		} else if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_AVC) || (esd->decoderConfig->objectTypeIndication==GF_CODECID_SVC)) {
			gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0x15);
		} else if (!PL) {
			gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0xFE);
		}
		break;
	case 0x05:
		PL = gf_isom_get_pl_indication(file, GF_ISOM_PL_AUDIO);
		switch (esd->decoderConfig->objectTypeIndication) {
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
		case GF_CODECID_AAC_MPEG4:
			gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			if (dsi.audioPL > PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_AUDIO, dsi.audioPL);
			break;
		default:
			if (!PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_AUDIO, 0xFE);
		}
		break;
	}
	gf_odf_desc_del((GF_Descriptor *) esd);
}
void remove_systems_tracks(GF_ISOFile *file)
{
	u32 i, count;

	count = gf_isom_get_track_count(file);
	if (count==1) return;

	/*force PL rewrite*/
	gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0);
	gf_isom_set_pl_indication(file, GF_ISOM_PL_AUDIO, 0);
	gf_isom_set_pl_indication(file, GF_ISOM_PL_OD, 1);	/*the lib always remove IOD when no profiles are specified..*/

	for (i=0; i<gf_isom_get_track_count(file); i++) {
		switch (gf_isom_get_media_type(file, i+1)) {
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
			gf_isom_remove_track_from_root_od(file, i+1);
			check_media_profile(file, i+1);
			break;
		/*only remove real systems tracks (eg, delaing with scene description & presentation)
		but keep meta & all unknown tracks*/
		case GF_ISOM_MEDIA_SCENE:
			switch (gf_isom_get_media_subtype(file, i+1, 1)) {
			case GF_ISOM_MEDIA_DIMS:
				gf_isom_remove_track_from_root_od(file, i+1);
				continue;
			default:
				break;
			}
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_OCR:
		case GF_ISOM_MEDIA_MPEGJ:
			gf_isom_remove_track(file, i+1);
			i--;
			break;
		default:
			break;
		}
	}
	/*none required*/
	if (!gf_isom_get_pl_indication(file, GF_ISOM_PL_AUDIO)) gf_isom_set_pl_indication(file, GF_ISOM_PL_AUDIO, 0xFF);
	if (!gf_isom_get_pl_indication(file, GF_ISOM_PL_VISUAL)) gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0xFF);

	gf_isom_set_pl_indication(file, GF_ISOM_PL_OD, 0xFF);
	gf_isom_set_pl_indication(file, GF_ISOM_PL_SCENE, 0xFF);
	gf_isom_set_pl_indication(file, GF_ISOM_PL_GRAPHICS, 0xFF);
	gf_isom_set_pl_indication(file, GF_ISOM_PL_INLINE, 0);
}

#endif /*!defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_AV_PARSERS)*/

GF_FileType get_file_type_by_ext(char *inName)
{
	GF_FileType type = GF_FILE_TYPE_NOT_SUPPORTED;
	char *ext = strrchr(inName, '.');
	if (ext) {
		char *sep;
		if (!strcmp(ext, ".gz")) ext = strrchr(ext-1, '.');
		ext+=1;
		sep = strchr(ext, '.');
		if (sep) sep[0] = 0;

		if (!stricmp(ext, "mp4") || !stricmp(ext, "3gp") || !stricmp(ext, "mov") || !stricmp(ext, "3g2") || !stricmp(ext, "3gs")) {
			type = GF_FILE_TYPE_ISO_MEDIA;
		} else if (!stricmp(ext, "bt") || !stricmp(ext, "wrl") || !stricmp(ext, "x3dv")) {
			type = GF_FILE_TYPE_BT_WRL_X3DV;
		} else if (!stricmp(ext, "xmt") || !stricmp(ext, "x3d")) {
			type = GF_FILE_TYPE_XMT_X3D;
		} else if (!stricmp(ext, "lsr") || !stricmp(ext, "saf")) {
			type = GF_FILE_TYPE_LSR_SAF;
		} else if (!stricmp(ext, "svg") || !stricmp(ext, "xsr") || !stricmp(ext, "xml")) {
			type = GF_FILE_TYPE_SVG;
		} else if (!stricmp(ext, "swf")) {
			type = GF_FILE_TYPE_SWF;
		} else if (!stricmp(ext, "jp2")) {
			if (sep) sep[0] = '.';
			return GF_FILE_TYPE_NOT_SUPPORTED;
		}
		else type = GF_FILE_TYPE_NOT_SUPPORTED;

		if (sep) sep[0] = '.';
	}


	/*try open file in read mode*/
	if (!type && gf_isom_probe_file(inName)) type = GF_FILE_TYPE_ISO_MEDIA;
	return type;
}

typedef struct
{
	GF_ISOTrackID trackID;
	char *line;
} SDPLine;

typedef enum {
	META_ACTION_SET_TYPE			= 0,
	META_ACTION_ADD_ITEM			= 1,
	META_ACTION_REM_ITEM			= 2,
	META_ACTION_SET_PRIMARY_ITEM	= 3,
	META_ACTION_SET_XML				= 4,
	META_ACTION_SET_BINARY_XML		= 5,
	META_ACTION_REM_XML				= 6,
	META_ACTION_DUMP_ITEM			= 7,
	META_ACTION_DUMP_XML			= 8,
	META_ACTION_ADD_IMAGE_ITEM		= 9,
} MetaActionType;

typedef struct
{
	MetaActionType act_type;
	Bool root_meta, use_dref;
	GF_ISOTrackID trackID;
	u32 meta_4cc;
	char *szPath, *szName, *mime_type, *enc_type;
	u32 item_id;
	Bool primary;
	u32 item_type;
	u32 ref_item_id;
	u32 ref_type;
	GF_ImageItemProperties *image_props;
} MetaAction;

#ifndef GPAC_DISABLE_ISOM_WRITE
static Bool parse_meta_args(MetaAction *meta, MetaActionType act_type, char *opts)
{
	Bool ret = 0;
	char *next;

	memset(meta, 0, sizeof(MetaAction));
	meta->act_type = act_type;
	meta->trackID = 0;
	meta->root_meta = 1;

	if (!opts) return 0;
	while (1) {
		char *szSlot;
		if (!opts || !opts[0]) return ret;
		if (opts[0]==':') opts += 1;

		szSlot = opts;
		next = gf_url_colon_suffix(opts);
		if (next) next[0] = 0;

		if (!strnicmp(szSlot, "tk=", 3)) {
			sscanf(szSlot, "tk=%u", &meta->trackID);
			meta->root_meta = 0;
			ret = 1;
		}
		else if (!strnicmp(szSlot, "id=", 3)) {
			meta->item_id = atoi(szSlot+3);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "type=", 5)) {
			meta->item_type = GF_4CC(szSlot[5], szSlot[6], szSlot[7], szSlot[8]);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "ref=", 4)) {
			char type[10];
			sscanf(szSlot, "ref=%s,%u", type, &meta->ref_item_id);
			meta->ref_type = GF_4CC(type[0], type[1], type[2], type[3]);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "name=", 5)) {
			meta->szName = gf_strdup(szSlot+5);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "path=", 5)) {
			meta->szPath = gf_strdup(szSlot+5);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "mime=", 5)) {
			meta->item_type = GF_META_ITEM_TYPE_MIME;
			meta->mime_type = gf_strdup(szSlot+5);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "encoding=", 9)) {
			meta->enc_type = gf_strdup(szSlot+9);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "image-size=", 11)) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			sscanf(szSlot+11, "%dx%d", &meta->image_props->width, &meta->image_props->height);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "image-pasp=", 11)) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			sscanf(szSlot+11, "%dx%d", &meta->image_props->hSpacing, &meta->image_props->vSpacing);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "image-rloc=", 11)) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			sscanf(szSlot+11, "%dx%d", &meta->image_props->hOffset, &meta->image_props->vOffset);
			ret = 1;
		}
		else if (!strnicmp(szSlot, "rotation=", 9)) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			meta->image_props->angle = atoi(szSlot+9);
			ret = 1;
		}
		else if (!stricmp(szSlot, "hidden")) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			meta->image_props->hidden = GF_TRUE;
			ret = 1;
		}
		else if (!stricmp(szSlot, "alpha")) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			meta->image_props->alpha = GF_TRUE;
			ret = 1;
		}
		else if (!strnicmp(szSlot, "time=", 5)) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			meta->image_props->time = atof(szSlot+5);
			ret = 1;
		}
		else if (!stricmp(szSlot, "split_tiles")) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			meta->image_props->tile_mode = TILE_ITEM_ALL_BASE;
			ret = 1;
		}
		else if (!stricmp(szSlot, "dref")) {
			meta->use_dref = 1;
			ret = 1;
		}
		else if (!stricmp(szSlot, "primary")) {
			meta->primary = 1;
			ret = 1;
		}
		else if (!stricmp(szSlot, "binary")) {
			if (meta->act_type==META_ACTION_SET_XML) meta->act_type=META_ACTION_SET_BINARY_XML;
			ret = 1;
		}
		else if (!strnicmp(szSlot, "icc_path=", 9)) {
			if (!meta->image_props) {
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);
			}
			strcpy(meta->image_props->iccPath, szSlot+9);
			ret = 1;
		}
		else if (!strchr(szSlot, '=')) {
			switch (meta->act_type) {
			case META_ACTION_SET_TYPE:
				if (!stricmp(szSlot, "null") || !stricmp(szSlot, "0")) meta->meta_4cc = 0;
				else meta->meta_4cc = GF_4CC(szSlot[0], szSlot[1], szSlot[2], szSlot[3]);
				ret = 1;
				break;
			case META_ACTION_ADD_ITEM:
			case META_ACTION_ADD_IMAGE_ITEM:
			case META_ACTION_SET_XML:
			case META_ACTION_DUMP_XML:
				if (!strncmp(szSlot, "dopt", 4) || !strncmp(szSlot, "sopt", 4) || !strncmp(szSlot, "@@", 2)) {
					if (next) next[0]=':';
					next=NULL;
				}
				//cat as -add arg
				gf_dynstrcat(&meta->szPath, szSlot, ":");
				ret = 1;
				break;
			case META_ACTION_REM_ITEM:
			case META_ACTION_SET_PRIMARY_ITEM:
			case META_ACTION_DUMP_ITEM:
				meta->item_id = atoi(szSlot);
				ret = 1;
				break;
			default:
				break;
			}
		}
		if (!next) break;
		opts += strlen(szSlot);
		next[0] = ':';
	}
	return ret;
}
#endif


typedef enum {
	TSEL_ACTION_SET_PARAM = 0,
	TSEL_ACTION_REMOVE_TSEL = 1,
	TSEL_ACTION_REMOVE_ALL_TSEL_IN_GROUP = 2,
} TSELActionType;

typedef struct
{
	TSELActionType act_type;
	GF_ISOTrackID trackID;

	GF_ISOTrackID refTrackID;
	u32 criteria[30];
	u32 nb_criteria;
	Bool is_switchGroup;
	u32 switchGroupID;
} TSELAction;

static Bool parse_tsel_args(TSELAction **__tsel_list, char *opts, u32 *nb_tsel_act, TSELActionType act)
{
	GF_ISOTrackID refTrackID = 0;
	Bool has_switch_id;
	u32 switch_id = 0;
	u32 criteria[30];
	u32 nb_criteria = 0;
	TSELAction *tsel_act;
	char szSlot[1024], *next;
	TSELAction *tsel_list;

	has_switch_id = 0;


	if (!opts) return 0;
	while (1) {
		if (!opts || !opts[0]) return 1;
		if (opts[0]==':') opts += 1;
		strcpy(szSlot, opts);
		next = gf_url_colon_suffix(szSlot);
		if (next) next[0] = 0;


		if (!strnicmp(szSlot, "refTrack=", 9)) refTrackID = atoi(szSlot+9);
		else if (!strnicmp(szSlot, "switchID=", 9)) {
			if (atoi(szSlot+9)<0) {
				switch_id = 0;
				has_switch_id = 0;
			} else {
				switch_id = atoi(szSlot+9);
				has_switch_id = 1;
			}
		}
		else if (!strnicmp(szSlot, "switchID", 8)) {
			switch_id = 0;
			has_switch_id = 1;
		}
		else if (!strnicmp(szSlot, "criteria=", 9)) {
			u32 j=9;
			nb_criteria = 0;
			while (j+3<strlen(szSlot)) {
				criteria[nb_criteria] = GF_4CC(szSlot[j], szSlot[j+1], szSlot[j+2], szSlot[j+3]);
				j+=5;
				nb_criteria++;
			}
		}
		else if (!strnicmp(szSlot, "trackID=", 8) || !strchr(szSlot, '=') ) {
			*__tsel_list = gf_realloc(*__tsel_list, sizeof(TSELAction) * (*nb_tsel_act + 1));
			tsel_list = *__tsel_list;

			tsel_act = &tsel_list[*nb_tsel_act];
			memset(tsel_act, 0, sizeof(TSELAction));
			tsel_act->act_type = act;
			tsel_act->trackID = strchr(szSlot, '=') ? atoi(szSlot+8) : atoi(szSlot);
			tsel_act->refTrackID = refTrackID;
			tsel_act->switchGroupID = switch_id;
			tsel_act->is_switchGroup = has_switch_id;
			tsel_act->nb_criteria = nb_criteria;
			memcpy(tsel_act->criteria, criteria, sizeof(u32)*nb_criteria);

			if (!refTrackID)
				refTrackID = tsel_act->trackID;

			(*nb_tsel_act) ++;
		}
		opts += strlen(szSlot);
	}
	return 1;
}


#define CHECK_NEXT_ARG	if (i+1==(u32)argc) { fprintf(stderr, "Missing arg - please check usage\n"); return mp4box_cleanup(1); }

typedef enum {
	TRAC_ACTION_REM_TRACK		= 0,
	TRAC_ACTION_SET_LANGUAGE	= 1,
	TRAC_ACTION_SET_DELAY		= 2,
	TRAC_ACTION_SET_KMS_URI		= 3,
	TRAC_ACTION_SET_PAR			= 4,
	TRAC_ACTION_SET_HANDLER_NAME= 5,
	TRAC_ACTION_ENABLE			= 6,
	TRAC_ACTION_DISABLE			= 7,
	TRAC_ACTION_REFERENCE		= 8,
	TRAC_ACTION_RAW_EXTRACT		= 9,
	TRAC_ACTION_REM_NON_RAP		= 10,
	TRAC_ACTION_SET_KIND		= 11,
	TRAC_ACTION_REM_KIND		= 12,
	TRAC_ACTION_SET_ID			= 13,
	TRAC_ACTION_SET_UDTA		= 14,
	TRAC_ACTION_SWAP_ID			= 15,
	TRAC_ACTION_REM_NON_REFS	= 16,
	TRAC_ACTION_SET_CLAP		= 17,
	TRAC_ACTION_SET_MX		= 18,
} TrackActionType;

typedef struct
{
	TrackActionType act_type;
	GF_ISOTrackID trackID;
	char lang[10];
	s32 delay_ms;
	const char *kms;
	const char *hdl_name;
	s32 par_num, par_den;
	u32 dump_type, sample_num;
	char *out_name;
	char *src_name;
	u32 udta_type;
	char *kind_scheme, *kind_value;
	u32 newTrackID;
	s32 clap_wnum, clap_wden, clap_hnum, clap_hden, clap_honum, clap_hoden, clap_vonum, clap_voden;
	s32 mx[9];
} TrackAction;

enum
{
	GF_ISOM_CONV_TYPE_ISMA = 1,
	GF_ISOM_CONV_TYPE_ISMA_EX,
	GF_ISOM_CONV_TYPE_3GPP,
	GF_ISOM_CONV_TYPE_IPOD,
	GF_ISOM_CONV_TYPE_PSP,
	GF_ISOM_CONV_TYPE_MOV
};



GF_DashSegmenterInput *set_dash_input(GF_DashSegmenterInput *dash_inputs, char *name, u32 *nb_dash_inputs)
{
	GF_DashSegmenterInput *di;
	char *sep;
	char *other_opts = NULL;
#if 0
	// skip ./ and ../, and look for first . to figure out extension
	if ((name[1]=='/') || (name[2]=='/') || (name[1]=='\\') || (name[2]=='\\') ) sep = strchr(name+3, '.');
	else {
		char *s2 = strchr(name, ':');
		sep = strchr(name, '.');
		if (sep && s2 && (s2 - sep) < 0) {
			sep = name;
		}
	}
#else
	sep = gf_file_ext_start(name);
#endif

	//then look for our opt separator :
	sep = gf_url_colon_suffix(sep ? sep : name);

	dash_inputs = gf_realloc(dash_inputs, sizeof(GF_DashSegmenterInput) * (*nb_dash_inputs + 1) );
	memset(&dash_inputs[*nb_dash_inputs], 0, sizeof(GF_DashSegmenterInput) );
	di = &dash_inputs[*nb_dash_inputs];
	(*nb_dash_inputs)++;

	if (sep) {
		char *opts, *first_opt;
		opts = first_opt = sep;
		while (opts) {
			sep = gf_url_colon_suffix(opts);
			while (sep) {
				/* this is a real separator if it is followed by a keyword we are looking for */
				if (!strnicmp(sep, ":id=", 4) ||
				        !strnicmp(sep, ":dur=", 5) ||
				        !strnicmp(sep, ":period=", 8) ||
				        !strnicmp(sep, ":BaseURL=", 9) ||
				        !strnicmp(sep, ":bandwidth=", 11) ||
				        !strnicmp(sep, ":role=", 6) ||
				        !strnicmp(sep, ":desc", 5) ||
				        !strnicmp(sep, ":sscale", 7) ||
				        !strnicmp(sep, ":duration=", 10) || /*legacy*/!strnicmp(sep, ":period_duration=", 10) ||
				        !strnicmp(sep, ":xlink=", 7) ||
				        !strnicmp(sep, ":asID=", 6) ||
				        !strnicmp(sep, ":sn=", 4) ||
				        !strnicmp(sep, ":tpl=", 5) ||
				        !strnicmp(sep, ":hls=", 5) ||
				        !strnicmp(sep, ":trackID=", 9) ||
				        !strnicmp(sep, ":@@", 3)
				        ) {
					break;
				} else {
					char *nsep = gf_url_colon_suffix(sep+1);
					if (nsep) nsep[0] = 0;

					gf_dynstrcat(&other_opts, sep, ":");

					if (nsep) nsep[0] = ':';

					sep = strchr(sep+1, ':');
				}
			}
			if (sep && !strncmp(sep, "://", 3) && strnicmp(sep, ":@@", 3)) sep = gf_url_colon_suffix(sep+3);
			if (sep) sep[0] = 0;

			if (!strnicmp(opts, "id=", 3)) {
				u32 i;
				di->representationID = gf_strdup(opts+3);
				/* check to see if this representation Id has already been assigned */
				for (i=0; i<(*nb_dash_inputs)-1; i++) {
					GF_DashSegmenterInput *other_di;
					other_di = &dash_inputs[i];
					if (!strcmp(other_di->representationID, di->representationID)) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error: Duplicate Representation ID \"%s\" in command line\n", di->representationID));
					}
				}
			} else if (!strnicmp(opts, "dur=", 4)) di->media_duration = (Double)atof(opts+4);
			else if (!strnicmp(opts, "period=", 7)) di->periodID = gf_strdup(opts+7);
			else if (!strnicmp(opts, "BaseURL=", 8)) {
				di->baseURL = (char **)gf_realloc(di->baseURL, (di->nb_baseURL+1)*sizeof(char *));
				di->baseURL[di->nb_baseURL] = gf_strdup(opts+8);
				di->nb_baseURL++;
			} else if (!strnicmp(opts, "bandwidth=", 10)) di->bandwidth = atoi(opts+10);
			else if (!strnicmp(opts, "role=", 5)) {
				di->roles = gf_realloc(di->roles, sizeof (char *) * (di->nb_roles+1));
				di->roles[di->nb_roles] = gf_strdup(opts+5);
				di->nb_roles++;
			} else if (!strnicmp(opts, "desc", 4)) {
				u32 *nb_descs=NULL;
				char ***descs=NULL;
				u32 opt_offset=0;
				u32 len;
				if (!strnicmp(opts, "desc_p=", 7)) {
					nb_descs = &di->nb_p_descs;
					descs = &di->p_descs;
					opt_offset = 7;
				} else if (!strnicmp(opts, "desc_as=", 8)) {
					nb_descs = &di->nb_as_descs;
					descs = &di->as_descs;
					opt_offset = 8;
				} else if (!strnicmp(opts, "desc_as_c=", 8)) {
					nb_descs = &di->nb_as_c_descs;
					descs = &di->as_c_descs;
					opt_offset = 10;
				} else if (!strnicmp(opts, "desc_rep=", 8)) {
					nb_descs = &di->nb_rep_descs;
					descs = &di->rep_descs;
					opt_offset = 9;
				}
				if (opt_offset) {
					(*nb_descs)++;
					opts += opt_offset;
					len = (u32) strlen(opts);
					(*descs) = (char **)gf_realloc((*descs), (*nb_descs)*sizeof(char *));
					(*descs)[(*nb_descs)-1] = (char *)gf_malloc((len+1)*sizeof(char));
					strncpy((*descs)[(*nb_descs)-1], opts, len);
					(*descs)[(*nb_descs)-1][len] = 0;
				}

			}
			else if (!strnicmp(opts, "xlink=", 6)) di->xlink = gf_strdup(opts+6);
			else if (!strnicmp(opts, "sscale", 6)) di->sscale = GF_TRUE;
			else if (!strnicmp(opts, "period_duration=", 16)) {
				di->period_duration = (Double) atof(opts+16);
			}	else if (!strnicmp(opts, "duration=", 9)) {
				di->period_duration = (Double) atof(opts+9); /*legacy: use period_duration instead*/
			}
			else if (!strnicmp(opts, "asID=", 5)) di->asID = atoi(opts+5);
			else if (!strnicmp(opts, "sn=", 3)) di->startNumber = atoi(opts+3);
			else if (!strnicmp(opts, "tpl=", 4)) di->seg_template = gf_strdup(opts+4);
			else if (!strnicmp(opts, "hls=", 4)) di->hls_pl = gf_strdup(opts+4);
			else if (!strnicmp(opts, "trackID=", 8)) di->track_id = atoi(opts+8);
			else if (!strnicmp(opts, "@@", 2)) {
				di->filter_chain = gf_strdup(opts+2);
				if (sep) sep[0] = ':';
				sep = NULL;
			}

			if (!sep) break;
			sep[0] = ':';
			opts = sep+1;
		}
		first_opt[0] = '\0';
	}
	di->file_name = name;
	di->source_opts = other_opts;

	if (!di->representationID) {
		char szRep[100];
		sprintf(szRep, "%d", *nb_dash_inputs);
		di->representationID = gf_strdup(szRep);
	}

	return dash_inputs;
}

static GF_Err parse_track_action_params(char *string, TrackAction *action)
{
	char *param = string;
	if (!action || !string) return GF_BAD_PARAM;

	while (param) {
		param = gf_url_colon_suffix(param);
		if (param) {
			*param = 0;
			param++;
#ifndef GPAC_DISABLE_MEDIA_EXPORT
			if (!strncmp("vttnomerge", param, 10)) {
				action->dump_type |= GF_EXPORT_WEBVTT_NOMERGE;
			} else if (!strncmp("layer", param, 5)) {
				action->dump_type |= GF_EXPORT_SVC_LAYER;
			} else if (!strncmp("full", param, 4)) {
				action->dump_type |= GF_EXPORT_NHML_FULL;
			} else if (!strncmp("embedded", param, 8)) {
				action->dump_type |= GF_EXPORT_WEBVTT_META_EMBEDDED;
			} else if (!strncmp("output=", param, 7)) {
				action->out_name = gf_strdup(param+7);
			} else if (!strncmp("src=", param, 4)) {
				action->src_name = gf_strdup(param+4);
			} else if (!strncmp("box=", param, 4)) {
				action->src_name = gf_strdup(param+4);
				action->sample_num = 1;
			} else if (!strncmp("type=", param, 4)) {
				action->udta_type = GF_4CC(param[5], param[6], param[7], param[8]);
			} else if (action->dump_type == GF_EXPORT_RAW_SAMPLES) {
				action->sample_num = atoi(param);
			}
#endif
		}
	}
	if (!strcmp(string, "*")) {
		action->trackID = (u32) -1;
	} else {
		action->trackID = atoi(string);
	}
	return GF_OK;
}

static u32 create_new_track_action(char *string, TrackAction **actions, u32 *nb_track_act, u32 dump_type)
{
	*actions = (TrackAction *)gf_realloc(*actions, sizeof(TrackAction) * (*nb_track_act+1));
	memset(&(*actions)[*nb_track_act], 0, sizeof(TrackAction) );
	(*actions)[*nb_track_act].act_type = TRAC_ACTION_RAW_EXTRACT;
	(*actions)[*nb_track_act].dump_type = dump_type;
	parse_track_action_params(string, &(*actions)[*nb_track_act]);
	(*nb_track_act)++;
	return dump_type;
}

#ifndef GPAC_DISABLE_CORE_TOOLS
static GF_Err xml_bs_to_bin(char *inName, char *outName, u32 dump_std)
{
	GF_Err e;
	GF_XMLNode *root;
	u8 *data = NULL;
	u32 data_size;

	GF_DOMParser *dom = gf_xml_dom_new();
	e = gf_xml_dom_parse(dom, inName, NULL, NULL);
	if (e) {
		gf_xml_dom_del(dom);
		fprintf(stderr, "Failed to parse XML file: %s\n", gf_error_to_string(e));
		return e;
	}
	root = gf_xml_dom_get_root_idx(dom, 0);
	if (!root) {
		gf_xml_dom_del(dom);
		return GF_OK;
	}

	e = gf_xml_parse_bit_sequence(root, inName, &data, &data_size);
	gf_xml_dom_del(dom);

	if (e) {
		fprintf(stderr, "Failed to parse binary sequence: %s\n", gf_error_to_string(e));
		return e;
	}

	if (dump_std) {
		fwrite(data, 1, data_size, stdout);
	} else {
		FILE *t;
		char szFile[GF_MAX_PATH];
		if (outName) {
			strcpy(szFile, outName);
		} else {
			strcpy(szFile, inName);
			strcat(szFile, ".bin");
		}
		t = gf_fopen(szFile, "wb");
		if (!t) {
			fprintf(stderr, "Failed to open file %s\n", szFile);
			e = GF_IO_ERR;
		} else {
			if (fwrite(data, 1, data_size, t) != data_size) {
				fprintf(stderr, "Failed to write output to file %s\n", szFile);
				e = GF_IO_ERR;
			}
			gf_fclose(t);
		}
	}
	gf_free(data);
	return e;
}
#endif /*GPAC_DISABLE_CORE_TOOLS*/

static GF_Err do_compress_top_boxes(char *inName, char *outName, char *compress_top_boxes, u32 comp_top_box_version, Bool use_lzma)
{
	FILE *in, *out;
	u8 *buf;
	u32 buf_alloc, comp_size, start_offset;
	s32 bytes_comp=0;
	s32 bytes_uncomp=0;
	GF_Err e = GF_OK;
	u64 source_size, dst_size;
	u32 orig_box_overhead;
	u32 final_box_overhead;
	u32 gzip_code = use_lzma ? GF_4CC('l','z','m','a') : GF_4CC('g','z','i','p') ;
	u32 nb_added_box_bytes=0;
	Bool has_mov = GF_FALSE;
	u32 range_idx, nb_ranges=0;
	Bool replace_all = !strcmp(compress_top_boxes, "*");
	Bool requires_byte_ranges=GF_FALSE;
	GF_BitStream *bs_in, *bs_out;
	u32 idx_size=0, nb_moof;
	struct _ranges {
		u32 size, csize;
	} *ranges=NULL;

	if (!outName) {
		fprintf(stderr, "Missing output file name\n");
		return GF_BAD_PARAM;
	}

	in = gf_fopen(inName, "rb");
	if (!in) return GF_URL_ERROR;
	out = gf_fopen(outName, "wb");
	if (!out) return GF_IO_ERR;

	buf_alloc = 4096;
	buf = gf_malloc(buf_alloc);

	bs_in = gf_bs_from_file(in, GF_BITSTREAM_READ);
	source_size = gf_bs_get_size(bs_in);

	bs_out = gf_bs_from_file(out, GF_BITSTREAM_WRITE);

	start_offset = 0;
	nb_moof = 0;
	if (comp_top_box_version==2) {
		u32 i;
		while (gf_bs_available(bs_in)) {
			u32 size = gf_bs_read_u32(bs_in);
			u32 type = gf_bs_read_u32(bs_in);
			const char *b4cc = gf_4cc_to_str(type);
			const char *replace = strstr(compress_top_boxes, b4cc);

			if (start_offset) {
				Bool compress = (replace || replace_all) ? 1 : 0;
				ranges = gf_realloc(ranges, sizeof(struct _ranges)*(nb_ranges+1));
				ranges[nb_ranges].csize = compress;
				ranges[nb_ranges].size = size-8;
				nb_ranges++;
			}
			if (!strcmp(b4cc, "ftyp") || !strcmp(b4cc, "styp")) {
				if (!start_offset) start_offset = (u32) gf_bs_get_position(bs_in) + size-8;
			}
			if (!strcmp(b4cc, "sidx") || !strcmp(b4cc, "ssix")) {
				requires_byte_ranges = GF_TRUE;
			}
			if (!strcmp(b4cc, "moof"))
				nb_moof++;

			gf_bs_skip_bytes(bs_in, size-8);
		}

		gf_bs_seek(bs_in, 0);
		if (buf_alloc<start_offset) {
			buf_alloc = start_offset;
			buf = gf_realloc(buf, buf_alloc);
		}
		gf_bs_read_data(bs_in, buf, start_offset);
		gf_bs_write_data(bs_out, buf, start_offset);

		if (!requires_byte_ranges) {
			nb_ranges = 0;
			gf_free(ranges);
			ranges = NULL;
		}
		idx_size = 8 + 4 + 4;
		for (i=0; i<nb_ranges; i++) {
			idx_size += 1;
			if (ranges[i].size<0xFFFF) {
				idx_size += 2;
				if (ranges[i].csize) idx_size += 2;
			} else {
				idx_size += 4;
				if (ranges[i].csize) idx_size += 4;
			}
			ranges[i].csize = 0;
		}
		i=idx_size;
		while (i) {
			gf_bs_write_u8(bs_out, 0);
			i--;
		}
	}

	range_idx = 0;
	orig_box_overhead = 0;
	final_box_overhead = 0;
	while (gf_bs_available(bs_in)) {
		u32 size = gf_bs_read_u32(bs_in);
		u32 type = gf_bs_read_u32(bs_in);
		const char *b4cc = gf_4cc_to_str(type);
		const u8 *replace = (const u8 *) strstr(compress_top_boxes, b4cc);
		if (!strcmp(b4cc, "moov")) has_mov = GF_TRUE;

		if (!replace && !replace_all) {
			if (ranges) {
				assert(! ranges[range_idx].csize);
				range_idx++;
			}
			gf_bs_write_u32(bs_out, size);
			gf_bs_write_u32(bs_out, type);

			size-=8;
			while (size) {
				u32 nbytes = size;
				if (nbytes>buf_alloc) nbytes=buf_alloc;
				gf_bs_read_data(bs_in, buf, nbytes);
				gf_bs_write_data(bs_out, buf, nbytes);
				size-=nbytes;
			}
			continue;
		}
		orig_box_overhead += size;

		if (comp_top_box_version != 1)
			size-=8;

		if (size>buf_alloc) {
			buf_alloc = size;
			buf = gf_realloc(buf, buf_alloc);
		}
		gf_bs_read_data(bs_in, buf, size);

		if (comp_top_box_version != 1)
			replace+=5;

		comp_size = buf_alloc;

		if (use_lzma) {
			e = gf_lz_compress_payload(&buf, size, &comp_size);
		} else {
			e = gf_gz_compress_payload(&buf, size, &comp_size);
		}
		if (e) break;

		if (comp_size>buf_alloc) {
			buf_alloc = comp_size;
		}
		bytes_uncomp += size;
		bytes_comp += comp_size;
		if (comp_top_box_version==1)
			nb_added_box_bytes +=8;

		//write size
		gf_bs_write_u32(bs_out, comp_size+8);
		//write type
		if (comp_top_box_version==1)
			gf_bs_write_u32(bs_out, gzip_code);
		else
			gf_bs_write_data(bs_out, replace, 4);
		//write data
		gf_bs_write_data(bs_out, buf, comp_size);

		final_box_overhead += 8+comp_size;

		if (ranges) {
			assert(ranges[range_idx].size == size);
			ranges[range_idx].csize = comp_size;
			range_idx++;
		}
	}
	dst_size = gf_bs_get_position(bs_out);

	if (comp_top_box_version==2) {
		u32 i;
		gf_bs_seek(bs_out, start_offset);
		gf_bs_write_u32(bs_out, idx_size);
		gf_bs_write_u32(bs_out, GF_4CC('c','m','a','p'));
		gf_bs_write_u32(bs_out, gzip_code);
		gf_bs_write_u32(bs_out, nb_ranges);
		for (i=0; i<nb_ranges; i++) {
			u32 large_size = ranges[i].size>0xFFFF ? 1 : 0;
			gf_bs_write_int(bs_out, ranges[i].csize ? 1 : 0, 1);
			gf_bs_write_int(bs_out, large_size ? 1 : 0, 1);
			gf_bs_write_int(bs_out, 0, 6);
			large_size = large_size ? 32 : 16;

			gf_bs_write_int(bs_out, ranges[i].size, large_size);
			if (ranges[i].csize)
				gf_bs_write_int(bs_out, ranges[i].csize, large_size);
		}
		final_box_overhead += idx_size;
		nb_added_box_bytes += idx_size;

	}
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_fclose(in);
	gf_fclose(out);
	if (e) {
		fprintf(stderr, "Error compressing: %s\n", gf_error_to_string(e));
		return e;
	}

	if (has_mov) {
		u32 i, nb_tracks, nb_samples;
		GF_ISOFile *mov;
		Double rate, new_rate, duration;

		mov = gf_isom_open(inName, GF_ISOM_OPEN_READ, NULL);
		duration = (Double) gf_isom_get_duration(mov);
		duration /= gf_isom_get_timescale(mov);

		nb_samples = 0;
		nb_tracks = gf_isom_get_track_count(mov);
		for (i=0; i<nb_tracks; i++) {
			nb_samples += gf_isom_get_sample_count(mov, i+1);
		}
		gf_isom_close(mov);

		rate = (Double) source_size;
		rate /= duration;
		rate /= 1000;

		new_rate = (Double) dst_size;
		new_rate /= duration;
		new_rate /= 1000;


		fprintf(stderr, "Log format:\nname\torig\tcomp\tgain\tadded_bytes\torate\tcrate\tsamples\tobbps\tcbbps\n");
		fprintf(stdout, "%s\t%d\t%d\t%g\t%d\t%g\t%g\t%d\t%g\t%g\n", inName, bytes_uncomp, bytes_comp, ((Double)(bytes_uncomp-bytes_comp)*100)/bytes_uncomp, nb_added_box_bytes, rate, new_rate, nb_samples,  ((Double)orig_box_overhead)/nb_samples, ((Double)final_box_overhead)/nb_samples );

		fprintf(stderr, "%s Compressing top-level boxes saved %d bytes out of %d (reduced by %g %%) additionnal bytes %d original rate %g kbps new rate %g kbps, orig %g box bytes/sample final %g bytes/sample\n", inName, bytes_uncomp-bytes_comp, bytes_uncomp, ((Double)(bytes_uncomp-bytes_comp)*100)/bytes_uncomp, nb_added_box_bytes, rate, new_rate, ((Double)orig_box_overhead)/nb_samples, ((Double)final_box_overhead)/nb_samples );

	} else {
		fprintf(stderr, "Log format:\nname\torig\tcomp\tgain\tadded_bytes\n");
		fprintf(stdout, "%s\t%d\t%d\t%g\t%d\n", inName, bytes_uncomp, bytes_comp, ((Double) (bytes_uncomp - bytes_comp)*100)/(bytes_uncomp), nb_added_box_bytes);

		fprintf(stderr, "%s Compressing top-level boxes saved %d bytes out of %d (reduced by %g %%) additionnal bytes %d\n", inName, bytes_uncomp-bytes_comp, bytes_uncomp, ((Double)(bytes_uncomp-bytes_comp)*100)/bytes_uncomp, nb_added_box_bytes);

	}
	return GF_OK;
}

static GF_Err hash_file(char *name, u32 dump_std)
{
	u32 i;
	u8 hash[20];
	GF_Err e = gf_media_get_file_hash(name, hash);
	if (e) return e;
	if (dump_std==2) {
		fwrite(hash, 1, 20, stdout);
	} else if (dump_std==1) {
		for (i=0; i<20; i++) fprintf(stdout, "%02X", hash[i]);
	}
	fprintf(stderr, "File hash (SHA-1): ");
	for (i=0; i<20; i++) fprintf(stderr, "%02X", hash[i]);
	fprintf(stderr, "\n");

	return GF_OK;
}


char outfile[5000];
GF_Err e;
#ifndef GPAC_DISABLE_SCENE_ENCODER
GF_SMEncodeOptions opts;
#endif
SDPLine *sdp_lines = NULL;
Double interleaving_time, split_duration, split_start, dash_duration, dash_subduration;
GF_Fraction import_fps;
Bool dash_duration_strict;
MetaAction *metas = NULL;
TrackAction *tracks = NULL;
TSELAction *tsel_acts = NULL;
u64 movie_time, initial_tfdt;
s32 subsegs_per_sidx;
u32 *brand_add = NULL;
u32 *brand_rem = NULL;
GF_DashSwitchingMode bitstream_switching_mode = GF_DASH_BSMODE_DEFAULT;
u32 i, j, stat_level, hint_flags, info_track_id, import_flags, nb_add, nb_cat, crypt, agg_samples, nb_sdp_ex, max_ptime, split_size, nb_meta_act, nb_track_act, rtp_rate, major_brand, nb_alt_brand_add, nb_alt_brand_rem, old_interleave, car_dur, minor_version, conv_type, nb_tsel_acts, program_number, dump_nal, time_shift_depth, initial_moof_sn, dump_std, import_subtitle, dump_saps, dump_saps_mode;
GF_DashDynamicMode dash_mode=GF_DASH_STATIC;
#ifndef GPAC_DISABLE_SCENE_DUMP
GF_SceneDumpFormat dump_mode;
#endif
Double mpd_live_duration = 0;
Bool HintIt, needSave, FullInter, Frag, HintInter, dump_rtp, regular_iod, remove_sys_tracks, remove_hint, force_new, remove_root_od;
Bool print_sdp, print_info, open_edit, dump_cr, force_ocr, encode, do_log, dump_srt, dump_ttxt, do_saf, dump_m2ts, dump_cart, do_hash, verbose, force_cat, align_cat, pack_wgt, single_group, clean_groups, dash_live, no_fragments_defaults, single_traf_per_moof, tfdt_per_traf, dump_nal_crc, hls_clock, do_mpd_rip, merge_vtt_cues;
char *inName, *outName, *arg, *mediaSource, *tmpdir, *input_ctx, *output_ctx, *drm_file, *avi2raw, *cprt, *chap_file, *pes_dump, *itunes_tags, *pack_file, *raw_cat, *seg_name, *dash_ctx_file, *compress_top_boxes, *high_dynamc_range_filename, *use_init_seg, *box_patch_filename;
u32 track_dump_type, dump_isom, dump_timestamps;
GF_ISOTrackID trackID;
u32 do_flat, box_patch_trackID=0;
Bool comp_lzma=GF_FALSE;
Bool freeze_box_order=GF_FALSE;
Double min_buffer = 1.5;
u32 comp_top_box_version = 0;
s32 ast_offset_ms = 0;
u32 fs_dump_flags = 0;
u32 dump_chap = 0;
u32 dump_udta_type = 0;
u32 dump_udta_track = 0;
char **mpd_base_urls = NULL;
u32 nb_mpd_base_urls = 0;
u32 dash_scale = 1000;
Bool insert_utc = GF_FALSE;
const char *udp_dest = NULL;

#ifndef GPAC_DISABLE_MPD
Bool do_mpd = GF_FALSE;
#endif
#ifndef GPAC_DISABLE_SCENE_ENCODER
Bool chunk_mode = GF_FALSE;
#endif
#ifndef GPAC_DISABLE_ISOM_HINTING
Bool HintCopy = 0;
u32 MTUSize = 1450;
#endif
#ifndef GPAC_DISABLE_CORE_TOOLS
Bool do_bin_xml = GF_FALSE;
#endif
GF_ISOFile *file;
Bool frag_real_time = GF_FALSE;
const char *dash_start_date=NULL;
GF_DASH_ContentLocationMode cp_location_mode = GF_DASH_CPMODE_ADAPTATION_SET;
Double mpd_update_time = GF_FALSE;
Bool force_co64 = GF_FALSE;
Bool live_scene = GF_FALSE;
Bool use_mfra = GF_FALSE;
GF_MemTrackerType mem_track = GF_MemTrackerNone;

Bool dump_iod = GF_FALSE;
GF_DASHPSSHMode pssh_mode = 0;
Bool samplegroups_in_traf = GF_FALSE;
Bool mvex_after_traks = GF_FALSE;
Bool daisy_chain_sidx = GF_FALSE;
Bool use_ssix = GF_FALSE;
Bool single_segment = GF_FALSE;
Bool single_file = GF_FALSE;
Bool segment_timeline = GF_FALSE;
u32 segment_marker = GF_FALSE;
GF_DashProfile dash_profile = GF_DASH_PROFILE_AUTO;
const char *dash_profile_extension = NULL;
const char *dash_cues = NULL;
Bool strict_cues = GF_FALSE;
Bool use_url_template = GF_FALSE;
Bool seg_at_rap = GF_FALSE;
Bool frag_at_rap = GF_FALSE;
Bool adjust_split_end = GF_FALSE;
Bool memory_frags = GF_TRUE;
Bool keep_utc = GF_FALSE;
#ifndef GPAC_DISABLE_ATSC
Bool grab_atsc = GF_FALSE;
s32 atsc_max_segs = -1;
u32 atsc_stats_rate = 0;
u32 atsc_debug_tsi = 0;
const char *atsc_output_dir = NULL;
s32 atsc_service = -1;
#endif
u32 timescale = 0;
const char *do_wget = NULL;
GF_DashSegmenterInput *dash_inputs = NULL;
u32 nb_dash_inputs = 0;
char *gf_logs = NULL;
char *seg_ext = NULL;
char *init_seg_ext = NULL;
const char *dash_title = NULL;
const char *dash_source = NULL;
const char *dash_more_info = NULL;

FILE *logfile = NULL;
static u32 run_for=0;
static u32 dash_cumulated_time,dash_prev_time,dash_now_time;
static Bool no_cache=GF_FALSE;
static Bool no_loop=GF_FALSE;
static Bool split_on_bound=GF_FALSE;
static Bool split_on_closest=GF_FALSE;

u32 mp4box_cleanup(u32 ret_code) {
	if (mpd_base_urls) {
		gf_free(mpd_base_urls);
		mpd_base_urls = NULL;
	}
	if (sdp_lines) {
		gf_free(sdp_lines);
		sdp_lines = NULL;
	}
	if (metas) {
		u32 i;
		for (i=0; i<nb_meta_act; i++) {
			if (metas[i].enc_type) gf_free(metas[i].enc_type);
			if (metas[i].mime_type) gf_free(metas[i].mime_type);
			if (metas[i].szName) gf_free(metas[i].szName);
			if (metas[i].szPath) gf_free(metas[i].szPath);
		}
		gf_free(metas);
		metas = NULL;
	}
	if (tracks) {
		for (i = 0; i<nb_track_act; i++) {
			if (tracks[i].out_name)
				gf_free(tracks[i].out_name);
			if (tracks[i].src_name)
				gf_free(tracks[i].src_name);
			if (tracks[i].kind_scheme)
				gf_free(tracks[i].kind_scheme);
			if (tracks[i].kind_value)
				gf_free(tracks[i].kind_value);
		}
		gf_free(tracks);
		tracks = NULL;
	}
	if (tsel_acts) {
		gf_free(tsel_acts);
		tsel_acts = NULL;
	}
	if (brand_add) {
		gf_free(brand_add);
		brand_add = NULL;
	}
	if (brand_rem) {
		gf_free(brand_rem);
		brand_rem = NULL;
	}
	if (dash_inputs) {
		u32 i, j;
		for (i = 0; i<nb_dash_inputs; i++) {
			GF_DashSegmenterInput *di = &dash_inputs[i];
			if (di->nb_baseURL) {
				for (j = 0; j<di->nb_baseURL; j++) {
					gf_free(di->baseURL[j]);
				}
				gf_free(di->baseURL);
			}
			if (di->rep_descs) {
				for (j = 0; j<di->nb_rep_descs; j++) {
					gf_free(di->rep_descs[j]);
				}
				gf_free(di->rep_descs);
			}
			if (di->as_descs) {
				for (j = 0; j<di->nb_as_descs; j++) {
					gf_free(di->as_descs[j]);
				}
				gf_free(di->as_descs);
			}
			if (di->as_c_descs) {
				for (j = 0; j<di->nb_as_c_descs; j++) {
					gf_free(di->as_c_descs[j]);
				}
				gf_free(di->as_c_descs);
			}
			if (di->p_descs) {
				for (j = 0; j<di->nb_p_descs; j++) {
					gf_free(di->p_descs[j]);
				}
				gf_free(di->p_descs);
			}
			if (di->representationID) gf_free(di->representationID);
			if (di->periodID) gf_free(di->periodID);
			if (di->xlink) gf_free(di->xlink);
			if (di->seg_template) gf_free(di->seg_template);
			if (di->hls_pl) gf_free(di->hls_pl);
			if (di->source_opts) gf_free(di->source_opts);
			if (di->filter_chain) gf_free(di->filter_chain);

			if (di->roles) {
				for (j = 0; j<di->nb_roles; j++) {
					gf_free(di->roles[j]);
				}
				gf_free(di->roles);
			}
		}
		gf_free(dash_inputs);
		dash_inputs = NULL;
	}
	if (logfile) gf_fclose(logfile);
	gf_sys_close();
	return ret_code;
}

u32 mp4box_parse_args_continue(int argc, char **argv, u32 *current_index)
{
	u32 i = *current_index;
	/*parse our args*/
	{
		arg = argv[i];
		if (!stricmp(arg, "-itags")) {
			CHECK_NEXT_ARG
			itunes_tags = argv[i + 1];
			i++;
			open_edit = GF_TRUE;
		}
#ifndef GPAC_DISABLE_ISOM_HINTING
		else if (!stricmp(arg, "-hint")) {
			open_edit = GF_TRUE;
			HintIt = 1;
		}
		else if (!stricmp(arg, "-unhint")) {
			open_edit = GF_TRUE;
			remove_hint = 1;
		}
		else if (!stricmp(arg, "-copy")) HintCopy = 1;
		else if (!stricmp(arg, "-tight")) {
			FullInter = 1;
			open_edit = GF_TRUE;
			needSave = GF_TRUE;
		}
		else if (!stricmp(arg, "-ocr")) force_ocr = 1;
		else if (!stricmp(arg, "-latm")) hint_flags |= GP_RTP_PCK_USE_LATM_AAC;
		else if (!stricmp(arg, "-rap") || !stricmp(arg, "-refonly")) {
			if ((i + 1 < (u32)argc) && (argv[i + 1][0] != '-')) {
				if (sscanf(argv[i + 1], "%d", &trackID) == 1) {
					tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
					memset(&tracks[nb_track_act], 0, sizeof(TrackAction));
					tracks[nb_track_act].act_type = !stricmp(arg, "-rap") ? TRAC_ACTION_REM_NON_RAP : TRAC_ACTION_REM_NON_REFS;
					tracks[nb_track_act].trackID = trackID;
					nb_track_act++;
					i++;
					open_edit = GF_TRUE;
				}
			}
			hint_flags |= GP_RTP_PCK_SIGNAL_RAP;
			seg_at_rap = 1;
		}
		else if (!stricmp(arg, "-frag-rap")) {
			frag_at_rap = 1;
		}
		else if (!stricmp(arg, "-mfra")) {
			use_mfra = GF_TRUE;
		}
		else if (!stricmp(arg, "-ts")) hint_flags |= GP_RTP_PCK_SIGNAL_TS;
		else if (!stricmp(arg, "-size")) hint_flags |= GP_RTP_PCK_SIGNAL_SIZE;
		else if (!stricmp(arg, "-idx")) hint_flags |= GP_RTP_PCK_SIGNAL_AU_IDX;
		else if (!stricmp(arg, "-static")) hint_flags |= GP_RTP_PCK_USE_STATIC_ID;
		else if (!stricmp(arg, "-multi")) {
			hint_flags |= GP_RTP_PCK_USE_MULTI;
			if ((i + 1 < (u32)argc) && (sscanf(argv[i + 1], "%u", &max_ptime) == 1)) {
				char szPt[20];
				sprintf(szPt, "%u", max_ptime);
				if (!strcmp(szPt, argv[i + 1])) i++;
				else max_ptime = 0;
			}
		}
#endif
		else if (!stricmp(arg, "-mpeg4")) {
#ifndef GPAC_DISABLE_ISOM_HINTING
			hint_flags |= GP_RTP_PCK_FORCE_MPEG4;
#endif
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			import_flags |= GF_IMPORT_FORCE_MPEG4;
#endif
		}
#ifndef GPAC_DISABLE_ISOM_HINTING
		else if (!stricmp(arg, "-mtu")) {
			CHECK_NEXT_ARG
			MTUSize = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-cardur")) {
			CHECK_NEXT_ARG
			car_dur = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-rate")) {
			CHECK_NEXT_ARG
			rtp_rate = atoi(argv[i + 1]);
			i++;
		}
#ifndef GPAC_DISABLE_SENG
		else if (!stricmp(arg, "-add-sdp") || !stricmp(arg, "-sdp_ex")) {
			char *id;
			CHECK_NEXT_ARG
			sdp_lines = gf_realloc(sdp_lines, sizeof(SDPLine) * (nb_sdp_ex + 1));

			id = strchr(argv[i + 1], ':');
			if (id) {
				id[0] = 0;
				if (sscanf(argv[i + 1], "%u", &sdp_lines[0].trackID) == 1) {
					id[0] = ':';
					sdp_lines[nb_sdp_ex].line = id + 1;
				}
				else {
					id[0] = ':';
					sdp_lines[nb_sdp_ex].line = argv[i + 1];
					sdp_lines[nb_sdp_ex].trackID = 0;
				}
			}
			else {
				sdp_lines[nb_sdp_ex].line = argv[i + 1];
				sdp_lines[nb_sdp_ex].trackID = 0;
			}
			open_edit = GF_TRUE;
			nb_sdp_ex++;
			i++;
		}
#endif /*GPAC_DISABLE_SENG*/
#endif /*GPAC_DISABLE_ISOM_HINTING*/

		else if (!stricmp(arg, "-single")) {
#ifndef GPAC_DISABLE_MEDIA_EXPORT
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_MP4;
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));
			tracks[nb_track_act].act_type = TRAC_ACTION_RAW_EXTRACT;
			tracks[nb_track_act].trackID = atoi(argv[i + 1]);
			tracks[nb_track_act].dump_type = GF_EXPORT_MP4;
			nb_track_act++;
			i++;
#endif
		}
		else if (!stricmp(arg, "-iod")) regular_iod = 1;
		else if (!stricmp(arg, "-flat")) {
			open_edit = GF_TRUE;
			do_flat = 1;
		}
		else if (!stricmp(arg, "-keep-utc")) keep_utc = GF_TRUE;
		else if (!stricmp(arg, "-new")) force_new = GF_TRUE;
		else if (!stricmp(arg, "-timescale")) {
			CHECK_NEXT_ARG
			timescale = atoi(argv[i + 1]);
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-udta")) {
			CHECK_NEXT_ARG
			create_new_track_action(argv[i + 1], &tracks, &nb_track_act, 0);
			tracks[nb_track_act - 1].act_type = TRAC_ACTION_SET_UDTA;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-add") || !stricmp(arg, "-import") || !stricmp(arg, "-convert")) {
			CHECK_NEXT_ARG
			if (!stricmp(arg, "-import")) fprintf(stderr, "\tWARNING: \"-import\" is deprecated - use \"-add\"\n");
			else if (!stricmp(arg, "-convert")) fprintf(stderr, "\tWARNING: \"-convert\" is deprecated - use \"-add\"\n");
			nb_add++;
			i++;
		}
		else if (!stricmp(arg, "-cat") || !stricmp(arg, "-catx") || !stricmp(arg, "-catpl")) {
			CHECK_NEXT_ARG
			nb_cat++;
			i++;
		}
		else if (!stricmp(arg, "-time")) {
			struct tm time;
			CHECK_NEXT_ARG
			memset(&time, 0, sizeof(struct tm));
			sscanf(argv[i + 1], "%d/%d/%d-%d:%d:%d", &time.tm_mday, &time.tm_mon, &time.tm_year, &time.tm_hour, &time.tm_min, &time.tm_sec);
			time.tm_isdst = 0;
			time.tm_year -= 1900;
			time.tm_mon -= 1;
			open_edit = GF_TRUE;
			movie_time = 2082758400;
			movie_time += mktime(&time);
			i++;
		}
		else if (!stricmp(arg, "-force-cat")) force_cat = 1;
		else if (!stricmp(arg, "-align-cat")) align_cat = 1;
		else if (!stricmp(arg, "-unalign-cat")) align_cat = 0;
		else if (!stricmp(arg, "-raw-cat")) {
			CHECK_NEXT_ARG
			raw_cat = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-rem") || !stricmp(arg, "-disable") || !stricmp(arg, "-enable")) {
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));
			if (!stricmp(arg, "-enable")) tracks[nb_track_act].act_type = TRAC_ACTION_ENABLE;
			else if (!stricmp(arg, "-disable")) tracks[nb_track_act].act_type = TRAC_ACTION_DISABLE;
			else tracks[nb_track_act].act_type = TRAC_ACTION_REM_TRACK;
			tracks[nb_track_act].trackID = atoi(argv[i + 1]);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-set-track-id") || !stricmp(arg, "-swap-track-id")) {
			char *sep;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));
			tracks[nb_track_act].act_type = !stricmp(arg, "-set-track-id") ? TRAC_ACTION_SET_ID : TRAC_ACTION_SWAP_ID;
			sep = strchr(argv[i + 1], ':');
			if (!sep) {
				fprintf(stderr, "Bad format for -set-track-id - expecting \"id1:id2\" got \"%s\"\n", argv[i + 1]);
				return 2;
			}
			*sep = 0;
			tracks[nb_track_act].trackID = atoi(argv[i + 1]);
			*sep = ':';
			sep++;
			tracks[nb_track_act].newTrackID = atoi(sep);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-par")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			tracks[nb_track_act].act_type = TRAC_ACTION_SET_PAR;
			assert(strlen(argv[i + 1]) + 1 <= sizeof(szTK));
			strncpy(szTK, argv[i + 1], sizeof(szTK));
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track par - expecting ID=none or ID=PAR_NUM:PAR_DEN got %s\n", argv[i + 1]);
				return 2;
			}
			if (!stricmp(ext + 1, "none")) {
				tracks[nb_track_act].par_num = tracks[nb_track_act].par_den = -1;
			}
			else {
				sscanf(ext + 1, "%d", &tracks[nb_track_act].par_num);
				ext = strchr(ext + 1, ':');
				if (!ext) {
					fprintf(stderr, "Bad format for track par - expecting ID=PAR_NUM:PAR_DEN got %s\n", argv[i + 1]);
					return 2;
				}
				sscanf(ext + 1, "%d", &tracks[nb_track_act].par_den);
			}
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-clap")) {
			char szTK[200], *ext;
			TrackAction *tka;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			tracks[nb_track_act].act_type = TRAC_ACTION_SET_CLAP;
			assert(strlen(argv[i + 1]) + 1 <= sizeof(szTK));
			strncpy(szTK, argv[i + 1], sizeof(szTK));
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track clap - expecting ID=none or ID=WN:WD:HN:HD:HON:HOD:VON:VOD got %s\n", argv[i + 1]);
				return 2;
			}
			tka = &tracks[nb_track_act];
			if (!stricmp(ext + 1, "none")) {
				tka->clap_wnum= tka->clap_wden = tka->clap_hnum = tka->clap_hden = tka->clap_honum = tka->clap_hoden = tka->clap_vonum = tka->clap_voden = 0;
			} else {
				if (sscanf(ext + 1, "%d:%d:%d:%d:%d:%d:%d:%d", &tka->clap_wnum, &tka->clap_wden, &tka->clap_hnum, &tka->clap_hden, &tka->clap_honum, &tka->clap_hoden, &tka->clap_vonum, &tka->clap_voden) != 8) {

					fprintf(stderr, "Bad format for track clap - expecting ID=none or ID=WN:WD:HN:HD:HON:HOD:VON:VOD got %s\n", argv[i + 1]);
					return 2;
				}
			}
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-mx")) {
			char szTK[200], *ext;
			TrackAction *tka;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			tracks[nb_track_act].act_type = TRAC_ACTION_SET_MX;
			assert(strlen(argv[i + 1]) + 1 <= sizeof(szTK));
			strncpy(szTK, argv[i + 1], sizeof(szTK));
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track matrix - expecting ID=none or ID=M1:M2:M3:M4:M5:M6:M7:M8:M9 got %s\n", argv[i + 1]);
				return 2;
			}
			tka = &tracks[nb_track_act];
			if (!stricmp(ext + 1, "none")) {
				memset(tka->mx, 0, sizeof(s32)*9);
			} else {
				s32 res;
				if (strstr(ext+1, "0x")) {
					res = sscanf(ext + 1, "0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d", &tka->mx[0], &tka->mx[1], &tka->mx[2], &tka->mx[3], &tka->mx[4], &tka->mx[5], &tka->mx[6], &tka->mx[7], &tka->mx[8]);
				} else {
					res = sscanf(ext + 1, "%d:%d:%d:%d:%d:%d:%d:%d:%d", &tka->mx[0], &tka->mx[1], &tka->mx[2], &tka->mx[3], &tka->mx[4], &tka->mx[5], &tka->mx[6], &tka->mx[7], &tka->mx[8]);
				}
				if (res != 9) {
					fprintf(stderr, "Bad format for track matrix - expecting ID=none or ID=M1:M2:M3:M4:M5:M6:M7:M8:M9 got %s\n", argv[i + 1]);
					return 2;
				}
			}
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-hdr")) {
			CHECK_NEXT_ARG
			high_dynamc_range_filename = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-bo")) {
			freeze_box_order = GF_TRUE;
		}
		else if (!stricmp(arg, "-patch")) {
			CHECK_NEXT_ARG
			box_patch_filename = argv[i + 1];
			char *sep = strchr(box_patch_filename, '=');
			if (sep) {
				sep[0] = 0;
				box_patch_trackID = atoi(box_patch_filename);
				sep[0] = '=';
				box_patch_filename = sep+1;
			}
 			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-lang")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			tracks[nb_track_act].act_type = TRAC_ACTION_SET_LANGUAGE;
			tracks[nb_track_act].trackID = 0;
			strncpy(szTK, argv[i + 1], sizeof(szTK)-1);
			szTK[sizeof(szTK)-1] = 0;
			ext = strchr(szTK, '=');
			if (!strnicmp(argv[i + 1], "all=", 4)) {
				strncpy(tracks[nb_track_act].lang, argv[i + 1] + 4, 10);
			}
			else if (!ext) {
				strncpy(tracks[nb_track_act].lang, argv[i + 1], 10);
			}
			else {
				strncpy(tracks[nb_track_act].lang, ext + 1, 10);
				ext[0] = 0;
				tracks[nb_track_act].trackID = atoi(szTK);
				ext[0] = '=';
			}
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-kind") || !stricmp(arg, "-kind-rem")) {
			char szTK[200], *ext;
			char *scheme_start = NULL;
			Bool has_track_id = GF_FALSE;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			if (!stricmp(arg, "-kind")) {
				tracks[nb_track_act].act_type = TRAC_ACTION_SET_KIND;
			}
			else {
				tracks[nb_track_act].act_type = TRAC_ACTION_REM_KIND;
			}
			tracks[nb_track_act].trackID = 0;
			if (!strnicmp(argv[i + 1], "all=", 4)) {
				scheme_start = argv[i + 1] + 4;
				has_track_id = GF_TRUE;
			}
			if (!scheme_start) {
				if (strlen(argv[i + 1]) > 200) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_ALL, ("Warning: track kind parameter is too long!"));
				}
				strncpy(szTK, argv[i + 1], 200);
				ext = strchr(szTK, '=');
				if (ext && !has_track_id) {
					ext[0] = 0;
					has_track_id = (sscanf(szTK, "%d", &tracks[nb_track_act].trackID) == 1 ? GF_TRUE : GF_FALSE);
					if (has_track_id) {
						scheme_start = ext + 1;
					}
					else {
						scheme_start = szTK;
					}
					ext[0] = '=';
				}
				else {
					scheme_start = szTK;
				}
			}
			ext = strchr(scheme_start, '=');
			if (!ext) {
				tracks[nb_track_act].kind_scheme = gf_strdup(scheme_start);
			}
			else {
				ext[0] = 0;
				tracks[nb_track_act].kind_scheme = gf_strdup(scheme_start);
				ext[0] = '=';
				tracks[nb_track_act].kind_value = gf_strdup(ext + 1);
			}
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-delay")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			strncpy(szTK, argv[i + 1], sizeof(szTK)-1);
			szTK[sizeof(szTK)-1] = 0;
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track delay - expecting ID=DLAY got %s\n", argv[i + 1]);
				return 2;
			}
			tracks[nb_track_act].act_type = TRAC_ACTION_SET_DELAY;
			tracks[nb_track_act].delay_ms = atoi(ext + 1);
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-ref")) {
			char *szTK, *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			szTK = argv[i + 1];
			ext = strchr(szTK, ':');
			if (!ext) {
				fprintf(stderr, "Bad format for track reference - expecting ID:XXXX:refID got %s\n", argv[i + 1]);
				return 2;
			}
			tracks[nb_track_act].act_type = TRAC_ACTION_REFERENCE;
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			ext[0] = ':';
			szTK = ext + 1;
			ext = strchr(szTK, ':');
			if (!ext) {
				fprintf(stderr, "Bad format for track reference - expecting ID:XXXX:refID got %s\n", argv[i + 1]);
				return 2;
			}
			ext[0] = 0;
			strncpy(tracks[nb_track_act].lang, szTK, 10);
			ext[0] = ':';
			tracks[nb_track_act].delay_ms = (s32)atoi(ext + 1);
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-name")) {
			char szTK[GF_MAX_PATH], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			strncpy(szTK, argv[i + 1], sizeof(szTK)-1);
			szTK[sizeof(szTK)-1] = 0;
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track name - expecting ID=name got %s\n", argv[i + 1]);
				return 2;
			}
			tracks[nb_track_act].act_type = TRAC_ACTION_SET_HANDLER_NAME;
			tracks[nb_track_act].hdl_name = strchr(argv[i + 1], '=') + 1;
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			ext[0] = '=';
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
#if !defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
		else if (!stricmp(arg, "-dref")) import_flags |= GF_IMPORT_USE_DATAREF;
		else if (!stricmp(arg, "-no-drop") || !stricmp(arg, "-nodrop")) import_flags |= GF_IMPORT_NO_FRAME_DROP;
		else if (!stricmp(arg, "-packed")) import_flags |= GF_IMPORT_FORCE_PACKED;
		else if (!stricmp(arg, "-sbr")) import_flags |= GF_IMPORT_SBR_IMPLICIT;
		else if (!stricmp(arg, "-sbrx")) import_flags |= GF_IMPORT_SBR_EXPLICIT;
		else if (!stricmp(arg, "-ps")) import_flags |= GF_IMPORT_PS_IMPLICIT;
		else if (!stricmp(arg, "-psx")) import_flags |= GF_IMPORT_PS_EXPLICIT;
		else if (!stricmp(arg, "-ovsbr")) import_flags |= GF_IMPORT_OVSBR;
		else if (!stricmp(arg, "-fps")) {
			CHECK_NEXT_ARG
			if (!strcmp(argv[i + 1], "auto")) { fprintf(stderr, "Warning, fps=auto option is deprecated\n"); }
			else if (strchr(argv[i + 1], '-')) {
				u32 ticks, dts_inc;
				sscanf(argv[i + 1], "%u-%u", &ticks, &dts_inc);
				if (!dts_inc) dts_inc = 1;
				import_fps.num = ticks;
				import_fps.den = dts_inc;
			} else {
				import_fps.num = (s32) (1000 * atof(argv[i + 1]));
				import_fps.den = 0;
			}
			i++;
		}
		else if (!stricmp(arg, "-agg")) {
			CHECK_NEXT_ARG agg_samples = atoi(argv[i + 1]);
			i++;
		}
#endif /*!defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_MEDIA_IMPORT*/
		else if (!stricmp(arg, "-keep-sys") || !stricmp(arg, "-keepsys")) keep_sys_tracks = 1;
		else if (!stricmp(arg, "-ms")) {
			CHECK_NEXT_ARG mediaSource = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-mp4")) {
			encode = GF_TRUE;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-saf")) {
			do_saf = GF_TRUE;
		}
		else if (!stricmp(arg, "-log")) {
			do_log = GF_TRUE;
		}
#ifndef GPAC_DISABLE_MPD
		else if (!stricmp(arg, "-mpd")) {
			do_mpd = GF_TRUE;
			CHECK_NEXT_ARG
			inName = argv[i + 1];
			i++;
		}
#endif

#ifndef GPAC_DISABLE_SCENE_ENCODER
		else if (!stricmp(arg, "-def")) opts.flags |= GF_SM_ENCODE_USE_NAMES;
		else if (!stricmp(arg, "-sync")) {
			CHECK_NEXT_ARG
			opts.flags |= GF_SM_ENCODE_RAP_INBAND;
			opts.rap_freq = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-shadow")) {
			CHECK_NEXT_ARG
			opts.flags &= ~GF_SM_ENCODE_RAP_INBAND;
			opts.flags |= GF_SM_ENCODE_RAP_SHADOW;
			opts.rap_freq = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-carousel")) {
			CHECK_NEXT_ARG
			opts.flags &= ~(GF_SM_ENCODE_RAP_INBAND | GF_SM_ENCODE_RAP_SHADOW);
			opts.rap_freq = atoi(argv[i + 1]);
			i++;
		}
		/*LASeR options*/
		else if (!stricmp(arg, "-resolution")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i + 1]);
			i++;
		}
#ifndef GPAC_DISABLE_SCENE_STATS
		else if (!stricmp(arg, "-auto-quant")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i + 1]);
			opts.auto_quant = 1;
			i++;
		}
#endif
		else if (!stricmp(arg, "-coord-bits")) {
			CHECK_NEXT_ARG
			opts.coord_bits = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-scale-bits")) {
			CHECK_NEXT_ARG
			opts.scale_bits = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-global-quant")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i + 1]);
			opts.auto_quant = 2;
			i++;
		}
		/*chunk encoding*/
		else if (!stricmp(arg, "-ctx-out") || !stricmp(arg, "-outctx")) {
			CHECK_NEXT_ARG
			output_ctx = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-ctx-in") || !stricmp(arg, "-inctx")) {
			CHECK_NEXT_ARG
			chunk_mode = GF_TRUE;
			input_ctx = argv[i + 1];
			i++;
		}
#endif /*GPAC_DISABLE_SCENE_ENCODER*/
		else if (!strcmp(arg, "-crypt")) {
			CHECK_NEXT_ARG
			crypt = 1;
			drm_file = argv[i + 1];
			open_edit = GF_TRUE;
			i += 1;
		}
		else if (!strcmp(arg, "-decrypt")) {
			CHECK_NEXT_ARG
			crypt = 2;
			if (get_file_type_by_ext(argv[i + 1]) != 1) {
				drm_file = argv[i + 1];
				i += 1;
			}
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-set-kms")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act + 1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction));

			strncpy(szTK, argv[i + 1], 19);
			ext = strchr(szTK, '=');
			tracks[nb_track_act].act_type = TRAC_ACTION_SET_KMS_URI;
			tracks[nb_track_act].trackID = 0;
			if (!strnicmp(argv[i + 1], "all=", 4)) {
				tracks[nb_track_act].kms = argv[i + 1] + 4;
			}
			else if (!ext) {
				tracks[nb_track_act].kms = argv[i + 1];
			}
			else {
				tracks[nb_track_act].kms = ext + 1;
				ext[0] = 0;
				tracks[nb_track_act].trackID = atoi(szTK);
				ext[0] = '=';
			}
			open_edit = GF_TRUE;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-split")) {
			CHECK_NEXT_ARG
			split_duration = atof(argv[i + 1]);
			if (split_duration < 0) split_duration = 0;
			i++;
			split_size = 0;
		}
		else if (!stricmp(arg, "-split-rap") || !stricmp(arg, "-splitr")) {
			CHECK_NEXT_ARG
			split_duration = -1;
			split_size = -1;
		}
		else if (!stricmp(arg, "-split-size") || !stricmp(arg, "-splits")) {
			CHECK_NEXT_ARG
			split_size = (u32)atoi(argv[i + 1]);
			i++;
			split_duration = 0;
		}
		else if (!stricmp(arg, "-split-chunk") || !stricmp(arg, "-splitx") || !stricmp(arg, "-splitz")) {
			CHECK_NEXT_ARG
			if (!strstr(argv[i + 1], ":")) {
				fprintf(stderr, "Chunk extraction usage: \"-splitx start:end\" expressed in seconds\n");
				return 2;
			}
			if (strstr(argv[i + 1], "end")) {
				if (strstr(argv[i + 1], "end-")) {
					Double dur_end=0;
					sscanf(argv[i + 1], "%lf:end-%lf", &split_start, &dur_end);
					split_duration = -2 - dur_end;
				} else {
					sscanf(argv[i + 1], "%lf:end", &split_start);
					split_duration = -2;
				}
			}
			else {
				sscanf(argv[i + 1], "%lf:%lf", &split_start, &split_duration);
				split_duration -= split_start;
			}
			split_size = 0;
			if (!stricmp(arg, "-splitz")) adjust_split_end = 1;
			i++;
		}
		/*meta*/
		else if (!stricmp(arg, "-set-meta")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_SET_TYPE, argv[i + 1]);
			nb_meta_act++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-add-item")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_ADD_ITEM, argv[i + 1]);
			nb_meta_act++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-add-image")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_ADD_IMAGE_ITEM, argv[i + 1]);
			nb_meta_act++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-rem-item")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_REM_ITEM, argv[i + 1]);
			nb_meta_act++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-set-primary")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_SET_PRIMARY_ITEM, argv[i + 1]);
			nb_meta_act++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-set-xml")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_SET_XML, argv[i + 1]);
			nb_meta_act++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-rem-xml")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			if (parse_meta_args(&metas[nb_meta_act], META_ACTION_REM_XML, argv[i + 1])) i++;
			nb_meta_act++;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-dump-xml")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_DUMP_XML, argv[i + 1]);
			nb_meta_act++;
			i++;
		}
		else if (!stricmp(arg, "-dump-item")) {
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
			parse_meta_args(&metas[nb_meta_act], META_ACTION_DUMP_ITEM, argv[i + 1]);
			nb_meta_act++;
			i++;
		}
		else if (!stricmp(arg, "-group-add") || !stricmp(arg, "-group-rem-track") || !stricmp(arg, "-group-rem") ) {
			TSELActionType act_type;
			if (!stricmp(arg, "-group-rem")) {
				act_type = TSEL_ACTION_REMOVE_ALL_TSEL_IN_GROUP;
			}
			else if (!stricmp(arg, "-group-rem-track")) {
				act_type = TSEL_ACTION_REMOVE_TSEL;
			}
			else {
				act_type = TSEL_ACTION_SET_PARAM;
			}
			if (parse_tsel_args(&tsel_acts, argv[i + 1], &nb_tsel_acts, act_type) == 0) {
				fprintf(stderr, "Invalid group syntax - check usage\n");
				return 2;
			}
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-group-clean")) {
			clean_groups = 1;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-group-single")) {
			single_group = 1;
		}
		else if (!stricmp(arg, "-package")) {
			CHECK_NEXT_ARG
			pack_file = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-mgt")) {
			CHECK_NEXT_ARG
			pack_file = argv[i + 1];
			pack_wgt = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-brand")) {
			char *b = argv[i + 1];
			CHECK_NEXT_ARG
			major_brand = GF_4CC(b[0], b[1], b[2], b[3]);
			open_edit = GF_TRUE;
			if (b[4] == ':') minor_version = atoi(b + 5);
			i++;
		}
		else if (!stricmp(arg, "-ab")) {
			char *b = argv[i + 1];
			CHECK_NEXT_ARG
			brand_add = (u32*)gf_realloc(brand_add, sizeof(u32) * (nb_alt_brand_add + 1));
			brand_add[nb_alt_brand_add] = GF_4CC(b[0], b[1], b[2], b[3]);
			nb_alt_brand_add++;
			open_edit = GF_TRUE;
			i++;
		}
		else if (!stricmp(arg, "-rb")) {
			char *b = argv[i + 1];
			CHECK_NEXT_ARG
			brand_rem = (u32*)gf_realloc(brand_rem, sizeof(u32) * (nb_alt_brand_rem + 1));
			brand_rem[nb_alt_brand_rem] = GF_4CC(b[0], b[1], b[2], b[3]);
			nb_alt_brand_rem++;
			open_edit = GF_TRUE;
			i++;
		}
#endif
		else if (!stricmp(arg, "-languages")) {
			PrintLanguages();
			return 1;
		}
		else if (!stricmp(arg, "-h")) {
			if (i + 1 == (u32)argc) PrintUsage();
			else if (!strcmp(argv[i + 1], "general")) PrintGeneralUsage();
			else if (!strcmp(argv[i + 1], "extract")) PrintExtractUsage();
			else if (!strcmp(argv[i + 1], "dash")) PrintDASHUsage();
			else if (!strcmp(argv[i + 1], "dump")) PrintDumpUsage();
			else if (!strcmp(argv[i + 1], "import")) PrintImportUsage();
			else if (!strcmp(argv[i + 1], "format")) fprintf(stderr, "deprectaed, see [filters documentation](Filters)\n");
			else if (!strcmp(argv[i + 1], "hint")) PrintHintUsage();
			else if (!strcmp(argv[i + 1], "encode")) PrintEncodeUsage();
			else if (!strcmp(argv[i + 1], "crypt")) PrintEncryptUsage();
			else if (!strcmp(argv[i + 1], "meta")) PrintMetaUsage();
			else if (!strcmp(argv[i + 1], "swf")) PrintSWFUsage();
#ifndef GPAC_DISABLE_ATSC
			else if (!strcmp(argv[i + 1], "atsc")) PrintATSCUsage();
#endif
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
			else if (!strcmp(argv[i + 1], "rtp")) fprintf(stderr, "RTP streaming deprecated in MP4Box, use gpac applications\n");
			else if (!strcmp(argv[i + 1], "live")) PrintLiveUsage();
#endif
			else if (!strcmp(argv[i + 1], "core")) PrintCoreUsage();
			else if (!strcmp(argv[i + 1], "all")) {
				PrintGeneralUsage();
				PrintExtractUsage();
				PrintDASHUsage();
				PrintDumpUsage();
				PrintImportUsage();
				PrintHintUsage();
				PrintEncodeUsage();
				PrintEncryptUsage();
				PrintMetaUsage();
				PrintSWFUsage();
#ifndef GPAC_DISABLE_ATSC
				PrintATSCUsage();
#endif
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
				PrintLiveUsage();
#endif
				PrintCoreUsage();
			} else {
				PrintUsage();
			}
			return 1;
		}
		else if (!strcmp(arg, "-genmd")) {
			help_flags = GF_PRINTARG_MD | GF_PRINTARG_IS_APP;
			helpout = gf_fopen("mp4box-gen-opts.md", "w");

	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » General");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			fprintf(helpout, "# Syntax\n");
			gf_sys_format_help(helpout, help_flags, "MP4Box [option] input [option] [other_dash_inputs]\n"
				"  \n"
			);
			PrintGeneralUsage();
			PrintEncryptUsage();
			fprintf(helpout, "# Help Options\n");
			while (m4b_usage_args[i].name) {
				GF_GPACArg *arg = &m4b_usage_args[i];
				i++;
				gf_sys_print_arg(helpout, help_flags, arg, "mp4box-general");
			}

			gf_fclose(helpout);

			helpout = gf_fopen("mp4box-import-opts.md", "w");
	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » Media Import");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			PrintImportUsage();
			gf_fclose(helpout);

			helpout = gf_fopen("mp4box-dash-opts.md", "w");
	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » Media DASH");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			PrintDASHUsage();
			gf_fclose(helpout);

			helpout = gf_fopen("mp4box-dump-opts.md", "w");
	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » Media Dump and Export");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			PrintExtractUsage();
			PrintDumpUsage();
			gf_fclose(helpout);

			helpout = gf_fopen("mp4box-meta-opts.md", "w");
	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » Meta and HEIF/IFF");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			PrintMetaUsage();
			gf_fclose(helpout);


			helpout = gf_fopen("mp4box-scene-opts.md", "w");
	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » Scene Description");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			PrintEncodeUsage();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
			PrintLiveUsage();
#endif
			PrintSWFUsage();
			gf_fclose(helpout);

			helpout = gf_fopen("mp4box-other-opts.md", "w");
	 		fprintf(helpout, "[**HOME**](Home) » [**MP4Box**](MP4Box) » Other Features");
	 		fprintf(helpout, "<!-- automatically generated - do not edit, patch gpac/applications/mp4box/main.c -->\n");
			PrintHintUsage();
#ifndef GPAC_DISABLE_ATSC
			PrintATSCUsage();
#endif
			gf_fclose(helpout);

			gf_sys_close();
			return 1;
		} else if (!strcmp(arg, "-genman")) {
			help_flags = GF_PRINTARG_MAN;
			helpout = gf_fopen("mp4box.1", "w");


	 		fprintf(helpout, ".TH MP4Box 1 2019 MP4Box GPAC\n");
			fprintf(helpout, ".\n.SH NAME\n.LP\nMP4Box \\- GPAC command-line media packager\n.SH SYNOPSIS\n.LP\n.B MP4Box\n.RI [options] \\ [file] \\ [options]\n.br\n.\n");

			PrintGeneralUsage();
			PrintExtractUsage();
			PrintDASHUsage();
			PrintDumpUsage();
			PrintImportUsage();
			PrintHintUsage();
			PrintEncodeUsage();
			PrintEncryptUsage();
			PrintMetaUsage();
			PrintSWFUsage();
#ifndef GPAC_DISABLE_ATSC
			PrintATSCUsage();
#endif
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
			PrintLiveUsage();
#endif

			fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://github.com/gpac/gpac/wiki/MP4Box-Introduction\n");
			fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
			".br\nFor bug reports, feature requests, more information and source code, visit http://github.com/gpac/gpac\n"
			".br\nbuild: %s\n"
			".br\nCopyright: %s\n.br\n"
			".SH SEE ALSO\n"
			".LP\ngpac(1), MP4Client(1)\n", gf_gpac_version(), gf_gpac_copyright());

			gf_fclose(helpout);
			gf_sys_close();
			return 1;
		}

		else if (!stricmp(arg, "-v")) verbose++;
		else if (!stricmp(arg, "-tag-list")) {
			fprintf(stderr, "Supported iTunes tag modifiers:\n");
			for (i = 0; i < nb_itunes_tags; i++) {
				fprintf(stderr, "\t%s\t%s\n", itags[i].name, itags[i].comment);
			}
			return 1;
		}
		else if (!live_scene) {
			u32 res = gf_sys_is_gpac_arg(arg);
			if (res==0) {
				fprintf(stderr, "Option %s unknown. Please check usage\n", arg);
				return 2;
			} else if (res==2) {
				i++;
			}
		}
	}
	*current_index = i;
	return 0;
}

Bool mp4box_parse_args(int argc, char **argv)
{
	u32 i;
	/*parse our args*/
	for (i = 1; i < (u32)argc; i++) {
		arg = argv[i];
		/*input file(s)*/
		if ((arg[0] != '-') || !stricmp(arg, "--")) {
			char *arg_val = arg;
			if (!stricmp(arg, "--")) {
				CHECK_NEXT_ARG
				arg_val = argv[i + 1];
				i++;
			}
			if (argc < 3) {
				fprintf(stderr, "Error - only one input file found as argument, please check usage\n");
				return 2;
			}
			else if (inName) {
				if (dash_duration) {
					if (!nb_dash_inputs) {
						dash_inputs = set_dash_input(dash_inputs, inName, &nb_dash_inputs);
					}
					dash_inputs = set_dash_input(dash_inputs, arg_val, &nb_dash_inputs);
				}
				else {
					fprintf(stderr, "Error - 2 input names specified, please check usage\n");
					return 2;
				}
			}
			else {
				inName = arg_val;
			}
		}
		else if (!stricmp(arg, "-?")) {
			PrintUsage();
			return 1;
		}
		else if (!stricmp(arg, "-version")) {
			PrintVersion();
			return 1;
		}
		else if (!stricmp(arg, "-sdp")) print_sdp = 1;
        else if (!strcmp(argv[i], "-mem-track")) continue;
        else if (!strcmp(argv[i], "-mem-track-stack")) continue;

		else if (!stricmp(arg, "-logs") || !strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			i++;
		}
		else if (!stricmp(arg, "-info")) {
			print_info = 1;
			if ((i + 1<(u32)argc) && (sscanf(argv[i + 1], "%u", &info_track_id) == 1)) {
				char szTk[20];
				sprintf(szTk, "%u", info_track_id);
				if (!strcmp(szTk, argv[i + 1])) i++;
				else info_track_id = 0;
			}
			else {
				info_track_id = 0;
			}
		}
#if !defined(GPAC_DISABLE_STREAMING)
		else if (!stricmp(arg, "-grab-ts")) {
			fprintf(stderr, "Deprecated option - use gpac application\n");
			return mp4box_cleanup(2);
		}
#endif
#ifndef GPAC_DISABLE_ATSC
		else if (!stricmp(arg, "-atsc")) {
			grab_atsc = GF_TRUE;
		}
		else if (!stricmp(arg, "-dir")) {
			CHECK_NEXT_ARG
			atsc_output_dir = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-service")) {
			CHECK_NEXT_ARG
			atsc_service = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-nb-segs")) {
			CHECK_NEXT_ARG
			atsc_max_segs = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-atsc-stats")) {
			CHECK_NEXT_ARG
			atsc_stats_rate = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-tsi")) {
			CHECK_NEXT_ARG
			atsc_debug_tsi = atoi(argv[i + 1]);
			i++;
		}
#endif
#if !defined(GPAC_DISABLE_CORE_TOOLS)
		else if (!stricmp(arg, "-wget")) {
			CHECK_NEXT_ARG
			do_wget = argv[i + 1];
			i++;
		}
#endif
		/*******************************************************************************/
		else if (!stricmp(arg, "-dvbhdemux")) {
			dvbhdemux = GF_TRUE;
		}
		/********************************************************************************/
#ifndef GPAC_DISABLE_MEDIA_EXPORT
		else if (!stricmp(arg, "-raw")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_NATIVE);
			i++;
		}
		else if (!stricmp(arg, "-raw-layer")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_NATIVE | GF_EXPORT_SVC_LAYER);
			i++;
		}
		else if (!stricmp(arg, "-qcp")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_NATIVE | GF_EXPORT_USE_QCP);
			i++;
		}
		else if (!stricmp(arg, "-aviraw")) {
			CHECK_NEXT_ARG
			if (argv[i + 1] && !stricmp(argv[i + 1], "video")) trackID = 1;
			else if (argv[i + 1] && !stricmp(argv[i + 1], "audio")) {
				if (strlen(argv[i + 1]) == 5) trackID = 2;
				else trackID = 1 + atoi(argv[i + 1] + 5);
			} else {
				fprintf(stderr, "Usage: \"-aviraw video\" or \"-aviraw audio\"\n");
				return 2;
			}
			track_dump_type = GF_EXPORT_AVI_NATIVE;
			i++;
		}
		else if (!stricmp(arg, "-raws")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_RAW_SAMPLES);
			i++;
		}
		else if (!stricmp(arg, "-nhnt")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_NHNT);
			i++;
		}
		else if (!stricmp(arg, "-nhml")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_NHML);
			i++;
		}
		else if (!stricmp(arg, "-webvtt-raw")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_WEBVTT_META);
			i++;
		}
		else if (!stricmp(arg, "-six")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_SIX);
			i++;
		}
		else if (!stricmp(arg, "-avi")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i + 1], &tracks, &nb_track_act, GF_EXPORT_AVI);
			if (tracks[nb_track_act-1].trackID)
				i++;
		}
#endif /*GPAC_DISABLE_MEDIA_EXPORT*/
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
		else if (!stricmp(arg, "-rtp")) {
			fprintf(stderr, "Deprecated option - use gpac application\n");
			return mp4box_cleanup(2);
		}
		else if (!stricmp(arg, "-live")) {
			live_scene = GF_TRUE;
		}
#endif
		else if (!stricmp(arg, "-diod")) {
			dump_iod = GF_TRUE;
		}
#ifndef GPAC_DISABLE_VRML
		else if (!stricmp(arg, "-node")) {
			CHECK_NEXT_ARG
			PrintNode(argv[i + 1], 0);
			return 1;
		}
		else if (!stricmp(arg, "-xnode")) {
			CHECK_NEXT_ARG
			PrintNode(argv[i + 1], 1);
			return 1;
		}
		else if (!stricmp(arg, "-nodes") || !stricmp(arg, "-nodex")) {
			PrintBuiltInNodes(0, !stricmp(arg, "-nodex") ? GF_TRUE : GF_FALSE);
			return 1;
		}
		else if (!stricmp(arg, "-xnodes") || !stricmp(arg, "-xnodex")) {
			PrintBuiltInNodes(1, !stricmp(arg, "-xnodex") ? GF_TRUE : GF_FALSE);
			return 1;
		}
#endif
#ifndef GPAC_DISABLE_SVG
		else if (!stricmp(arg, "-snodes")) {
			PrintBuiltInNodes(2, GF_FALSE);
			return 1;
		}
#endif
		else if (!stricmp(arg, "-boxcov")) {
			gf_sys_set_args(argc, (const char **) argv);
            PrintBuiltInBoxes(GF_TRUE);
			return 1;
		} else if (!stricmp(arg, "-boxes")) {
			PrintBuiltInBoxes(GF_FALSE);
			return 1;
		}
		else if (!stricmp(arg, "-std")) dump_std = 2;
		else if (!stricmp(arg, "-stdb")) dump_std = 1;
		else if (!stricmp(arg, "-fstat")) fs_dump_flags |= 1;
		else if (!stricmp(arg, "-fgraph")) fs_dump_flags |= 1<<1;

#if !defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_SCENE_DUMP)
		else if (!stricmp(arg, "-bt")) dump_mode = GF_SM_DUMP_BT;
		else if (!stricmp(arg, "-xmt")) dump_mode = GF_SM_DUMP_XMTA;
		else if (!stricmp(arg, "-wrl")) dump_mode = GF_SM_DUMP_VRML;
		else if (!stricmp(arg, "-x3dv")) dump_mode = GF_SM_DUMP_X3D_VRML;
		else if (!stricmp(arg, "-x3d")) dump_mode = GF_SM_DUMP_X3D_XML;
		else if (!stricmp(arg, "-lsr")) dump_mode = GF_SM_DUMP_LASER;
		else if (!stricmp(arg, "-svg")) dump_mode = GF_SM_DUMP_SVG;
#endif /*defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_SCENE_DUMP)*/

		else if (!stricmp(arg, "-stat")) stat_level = 1;
		else if (!stricmp(arg, "-stats")) stat_level = 2;
		else if (!stricmp(arg, "-statx")) stat_level = 3;
		else if (!stricmp(arg, "-diso")) dump_isom = 1;
		else if (!stricmp(arg, "-dxml")) dump_isom = 2;
		else if (!stricmp(arg, "-mergevtt")) merge_vtt_cues = GF_TRUE;
		else if (!stricmp(arg, "-dump-cover")) dump_cart = 1;
		else if (!stricmp(arg, "-dump-chap")) dump_chap = 1;
		else if (!stricmp(arg, "-dump-chap-ogg")) dump_chap = 2;
		else if (!stricmp(arg, "-hash")) do_hash = GF_TRUE;
		else if (!strnicmp(arg, "-comp", 5)) {
			CHECK_NEXT_ARG

			if (strchr(arg, 'x')) comp_top_box_version = 1;
			else if (strchr(arg, 'f')) comp_top_box_version = 2;

			if (strchr(arg, 'l')) comp_lzma = GF_TRUE;

			compress_top_boxes = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-mpd-rip")) do_mpd_rip = GF_TRUE;
		else if (!strcmp(arg, "-init-seg")) {
			CHECK_NEXT_ARG
			use_init_seg = argv[i + 1];
			i += 1;
		}

#ifndef GPAC_DISABLE_CORE_TOOLS
		else if (!stricmp(arg, "-bin")) do_bin_xml = GF_TRUE;
#endif
		else if (!stricmp(arg, "-dump-udta")) {
			char *sep, *code;
			CHECK_NEXT_ARG
			sep = strchr(argv[i + 1], ':');
			if (sep) {
				sep[0] = 0;
				dump_udta_track = atoi(argv[i + 1]);
				sep[0] = ':';
				code = &sep[1];
			}
			else {
				code = argv[i + 1];
			}
			dump_udta_type = GF_4CC(code[0], code[1], code[2], code[3]);
			i++;
		}
		else if (!stricmp(arg, "-dmp4")) {
			dump_isom = 1;
			fprintf(stderr, "WARNING: \"-dmp4\" is deprecated - use \"-diso\" option\n");
		}
		else if (!stricmp(arg, "-drtp")) dump_rtp = 1;
		else if (!stricmp(arg, "-dts")) {
			dump_timestamps = 1;
			if (((i + 1<(u32)argc) && inName) || (i + 2<(u32)argc)) {
				if (isdigit(argv[i + 1][0])) {
					program_number = atoi(argv[i + 1]);
					i++;
				}
			}
		}
		else if (!stricmp(arg, "-dtsx")) {
			dump_timestamps = 2;
		}
		else if (!stricmp(arg, "-dtsc")) {
			dump_timestamps = 3;
		}
		else if (!stricmp(arg, "-dtsxc")) {
			dump_timestamps = 4;
		}
		else if (!stricmp(arg, "-dnal")) {
			CHECK_NEXT_ARG
			dump_nal = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-dnalc")) {
			CHECK_NEXT_ARG
			dump_nal = atoi(argv[i + 1]);
			dump_nal_crc = GF_TRUE;
			i++;
		}
		else if (!strnicmp(arg, "-dsap", 5)) {
			CHECK_NEXT_ARG
			dump_saps = atoi(argv[i + 1]);
			if (!stricmp(arg, "-dsaps")) dump_saps_mode = 1;
			else if (!stricmp(arg, "-dsapc")) dump_saps_mode = 2;
			else if (!stricmp(arg, "-dsapd")) dump_saps_mode = 3;
			else if (!stricmp(arg, "-dsapp")) dump_saps_mode = 4;
			else dump_saps_mode = 0;
			i++;
		}
		else if (!stricmp(arg, "-dcr")) dump_cr = 1;
		else if (!stricmp(arg, "-ttxt") || !stricmp(arg, "-srt")) {
			if ((i + 1<(u32)argc) && (sscanf(argv[i + 1], "%u", &trackID) == 1)) {
				char szTk[20];
				sprintf(szTk, "%d", trackID);
				if (!strcmp(szTk, argv[i + 1])) i++;
				else trackID = 0;
			}
			else if ((i + 1<(u32)argc) && !strcmp(argv[i + 1], "*")) {
				trackID = (u32)-1;
				i++;
			}
			else {
				trackID = 0;
			}
#ifdef GPAC_DISABLE_ISOM_WRITE
			if (trackID) {
				fprintf(stderr, "Error: Read-Only version - subtitle conversion not available\n");
				return 2;
			}
#endif
			if (!stricmp(arg, "-ttxt")) dump_ttxt = GF_TRUE;
			else dump_srt = GF_TRUE;
			import_subtitle = 1;
		}
		else if (!stricmp(arg, "-dm2ts")) {
			dump_m2ts = 1;
			if (((i + 1<(u32)argc) && inName) || (i + 2<(u32)argc)) {
				if (argv[i + 1][0] != '-') pes_dump = argv[i + 1];
				i++;
			}
		}

#ifndef GPAC_DISABLE_SWF_IMPORT
		/*SWF importer options*/
		else if (!stricmp(arg, "-global")) swf_flags |= GF_SM_SWF_STATIC_DICT;
		else if (!stricmp(arg, "-no-ctrl")) swf_flags &= ~GF_SM_SWF_SPLIT_TIMELINE;
		else if (!stricmp(arg, "-no-text")) swf_flags |= GF_SM_SWF_NO_TEXT;
		else if (!stricmp(arg, "-no-font")) swf_flags |= GF_SM_SWF_NO_FONT;
		else if (!stricmp(arg, "-no-line")) swf_flags |= GF_SM_SWF_NO_LINE;
		else if (!stricmp(arg, "-no-grad")) swf_flags |= GF_SM_SWF_NO_GRADIENT;
		else if (!stricmp(arg, "-quad")) swf_flags |= GF_SM_SWF_QUAD_CURVE;
		else if (!stricmp(arg, "-xlp")) swf_flags |= GF_SM_SWF_SCALABLE_LINE;
		else if (!stricmp(arg, "-ic2d")) swf_flags |= GF_SM_SWF_USE_IC2D;
		else if (!stricmp(arg, "-same-app")) swf_flags |= GF_SM_SWF_REUSE_APPEARANCE;
		else if (!stricmp(arg, "-flatten")) {
			CHECK_NEXT_ARG
			swf_flatten_angle = (Float)atof(argv[i + 1]);
			i++;
		}
#endif
#ifndef GPAC_DISABLE_ISOM_WRITE
		else if (!stricmp(arg, "-isma")) {
			conv_type = GF_ISOM_CONV_TYPE_ISMA;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-3gp")) {
			conv_type = GF_ISOM_CONV_TYPE_3GPP;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-ipod")) {
			conv_type = GF_ISOM_CONV_TYPE_IPOD;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-psp")) {
			conv_type = GF_ISOM_CONV_TYPE_PSP;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-ismax")) {
			conv_type = GF_ISOM_CONV_TYPE_ISMA_EX;
			open_edit = GF_TRUE;
		}

		else if (!stricmp(arg, "-no-sys") || !stricmp(arg, "-nosys")) {
			remove_sys_tracks = 1;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-no-iod")) {
			remove_root_od = 1;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-out")) {
			CHECK_NEXT_ARG outName = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-tmp")) {
			CHECK_NEXT_ARG tmpdir = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-co64")) {
			force_co64 = GF_TRUE;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-write-buffer")) {
			CHECK_NEXT_ARG
			gf_isom_set_output_buffering(NULL, atoi(argv[i + 1]));
			i++;
		}
		else if (!stricmp(arg, "-cprt")) {
			CHECK_NEXT_ARG cprt = argv[i + 1];
			i++;
			if (!dash_duration) open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-chap")) {
			CHECK_NEXT_ARG chap_file = argv[i + 1];
			i++;
			open_edit = GF_TRUE;
		}
		else if (!stricmp(arg, "-inter") || !stricmp(arg, "-old-inter")) {
			CHECK_NEXT_ARG
			interleaving_time = atof(argv[i + 1]) / 1000;
			if (!interleaving_time) do_flat = 2;
			open_edit = GF_TRUE;
			needSave = GF_TRUE;
			if (!stricmp(arg, "-old-inter")) old_interleave = 1;
			i++;
		}
		else if (!stricmp(arg, "-frag")) {
			CHECK_NEXT_ARG
			interleaving_time = atof(argv[i + 1]) / 1000;
			needSave = GF_TRUE;
			i++;
			Frag = GF_TRUE;
		}
		else if (!stricmp(arg, "-dash")) {
			CHECK_NEXT_ARG
			dash_duration = atof(argv[i + 1]) / 1000;
			if (dash_duration == 0.0) {
				fprintf(stderr, "\tERROR: \"-dash-dash_duration\": invalid parameter %s\n", argv[i + 1]);
				return 2;
			}
			i++;
		}
		else if (!stricmp(arg, "-dash-strict")) {
			CHECK_NEXT_ARG
			dash_duration = atof(argv[i + 1]) / 1000;
			if (dash_duration == 0.0) {
				fprintf(stderr, "\tERROR: \"-dash-dash_duration\": invalid parameter %s\n", argv[i + 1]);
				return 2;
			}
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] -dash-strict is deprecated, will behave like -dash\n"));
			i++;
		}
		else if (!stricmp(arg, "-subdur")) {
			CHECK_NEXT_ARG
			dash_subduration = atof(argv[i + 1]) / 1000;
			i++;
		}
		else if (!stricmp(arg, "-dash-scale")) {
			CHECK_NEXT_ARG
			dash_scale = atoi(argv[i + 1]);
			if (!dash_scale) {
				fprintf(stderr, "\tERROR: \"-dash-scale\": invalid parameter %s\n", argv[i + 1]);
				return 2;
			}
			i++;
		}
		else if (!stricmp(arg, "-dash-ts-prog")) {
			CHECK_NEXT_ARG
			program_number = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-subsegs-per-sidx") || !stricmp(arg, "-frags-per-sidx")) {
			CHECK_NEXT_ARG
			subsegs_per_sidx = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-segment-name")) {
			CHECK_NEXT_ARG
			seg_name = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-run-for")) {
			CHECK_NEXT_ARG
			run_for = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-no-cache")) {
			no_cache = GF_TRUE;
		}
		else if (!stricmp(arg, "-no-loop")) {
			no_loop = GF_TRUE;
		}
		else if (!stricmp(arg, "-hlsc")) {
			hls_clock = GF_TRUE;
		}
		else if (!stricmp(arg, "-bound")) {
			split_on_bound = GF_TRUE;
		}
		else if (!stricmp(arg, "-closest")) {
			split_on_closest = GF_TRUE;
		}
		else if (!stricmp(arg, "-segment-ext")) {
			CHECK_NEXT_ARG
			seg_ext = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-init-segment-ext")) {
			CHECK_NEXT_ARG
			init_seg_ext = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-bs-switching")) {
			CHECK_NEXT_ARG
			if (!stricmp(argv[i + 1], "no") || !stricmp(argv[i + 1], "off")) bitstream_switching_mode = GF_DASH_BSMODE_NONE;
			else if (!stricmp(argv[i + 1], "merge"))  bitstream_switching_mode = GF_DASH_BSMODE_MERGED;
			else if (!stricmp(argv[i + 1], "multi"))  bitstream_switching_mode = GF_DASH_BSMODE_MULTIPLE_ENTRIES;
			else if (!stricmp(argv[i + 1], "single"))  bitstream_switching_mode = GF_DASH_BSMODE_SINGLE;
			else if (!stricmp(argv[i + 1], "inband"))  bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
			else {
				fprintf(stderr, "\tWARNING: Unrecognized bitstream switchin mode \"%s\" - please check usage\n", argv[i + 1]);
				return 2;
			}
			i++;
		}
		else if (!stricmp(arg, "-dynamic")) {
			dash_mode = GF_DASH_DYNAMIC;
		}
		else if (!stricmp(arg, "-last-dynamic")) {
			dash_mode = GF_DASH_DYNAMIC_LAST;
		}
		else if (!stricmp(arg, "-frag-rt")) {
			frag_real_time = GF_TRUE;
		}
		else if (!stricmp(arg, "-start-date")) {
			dash_start_date = argv[i+1];
			i++;
		}
		else if (!strnicmp(arg, "-cp-location=", 13)) {
			if (strcmp(arg+13, "both")) cp_location_mode = GF_DASH_CPMODE_BOTH;
			else if (strcmp(arg+13, "as")) cp_location_mode = GF_DASH_CPMODE_ADAPTATION_SET;
			else if (strcmp(arg+13, "rep")) cp_location_mode = GF_DASH_CPMODE_REPRESENTATION;
			else {
				fprintf(stderr, "\tWARNING: Unrecognized ContentProtection loction mode \"%s\" - please check usage\n", argv[i + 13]);
				return 2;
			}
		}
		else if (!strnicmp(arg, "-dash-live", 10) || !strnicmp(arg, "-ddbg-live", 10)) {
			dash_mode = !strnicmp(arg, "-ddbg-live", 10) ? GF_DASH_DYNAMIC_DEBUG : GF_DASH_DYNAMIC;
			dash_live = 1;
			if (arg[10] == '=') {
				dash_ctx_file = arg + 11;
			}
			CHECK_NEXT_ARG
			dash_duration = atof(argv[i + 1]) / 1000;
			i++;
		}
		else if (!stricmp(arg, "-mpd-duration")) {
			CHECK_NEXT_ARG mpd_live_duration = atof(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-mpd-refresh")) {
			CHECK_NEXT_ARG mpd_update_time = atof(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-time-shift")) {
			CHECK_NEXT_ARG
			time_shift_depth = (u32)atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-min-buffer")) {
			CHECK_NEXT_ARG
			min_buffer = atoi(argv[i + 1]);
			min_buffer /= 1000;
			i++;
		}
		else if (!stricmp(arg, "-ast-offset")) {
			CHECK_NEXT_ARG
			ast_offset_ms = atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-moof-sn")) {
			CHECK_NEXT_ARG
			initial_moof_sn = (u32)atoi(argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-tfdt")) {
			CHECK_NEXT_ARG
			sscanf(argv[i + 1], LLU, &initial_tfdt);
			i++;
		}
		else if (!stricmp(arg, "-no-frags-default")) {
			no_fragments_defaults = 1;
		}
		else if (!stricmp(arg, "-single-traf")) {
			single_traf_per_moof = 1;
		}
        else if (!stricmp(arg, "-tfdt-traf")) {
            tfdt_per_traf = 1;
        }
		else if (!stricmp(arg, "-mpd-title")) {
			CHECK_NEXT_ARG dash_title = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-mpd-source")) {
			CHECK_NEXT_ARG dash_source = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-mpd-info-url")) {
			CHECK_NEXT_ARG dash_more_info = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-base-url")) {
			CHECK_NEXT_ARG
			dash_more_info = argv[i + 1];
			mpd_base_urls = gf_realloc(mpd_base_urls, (nb_mpd_base_urls + 1)*sizeof(char**));
			mpd_base_urls[nb_mpd_base_urls] = argv[i + 1];
			nb_mpd_base_urls++;
			i++;
		}
		else if (!stricmp(arg, "-dash-ctx")) {
			CHECK_NEXT_ARG
			dash_ctx_file = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-ssix")) {
			use_ssix = 1;
		}
		else if (!stricmp(arg, "-daisy-chain")) {
			daisy_chain_sidx = 1;
		}
		else if (!stricmp(arg, "-single-segment")) {
			single_segment = 1;
		}
		else if (!stricmp(arg, "-single-file")) {
			single_file = 1;
		}
		else if (!stricmp(arg, "-pssh-moof")) {
			pssh_mode = GF_DASH_PSSH_MOOF;
		}
		else if (!strnicmp(arg, "-pssh=", 6)) {
			if (!strcmp(arg+6, "f")) pssh_mode = GF_DASH_PSSH_MOOF;
			else if (!strcmp(arg+6, "v")) pssh_mode = GF_DASH_PSSH_MOOV;
			else if (!strcmp(arg+6, "m")) pssh_mode = GF_DASH_PSSH_MPD;
			else if (!strcmp(arg+6, "mf") || !strcmp(arg+6, "fm")) pssh_mode = GF_DASH_PSSH_MOOF_MPD;
			else if (!strcmp(arg+6, "mv") || !strcmp(arg+6, "vm")) pssh_mode = GF_DASH_PSSH_MOOV_MPD;
			else pssh_mode = GF_DASH_PSSH_MOOV;
		}
		else if (!stricmp(arg, "-sample-groups-traf")) {
			samplegroups_in_traf = 1;
		}
		else if (!stricmp(arg, "-mvex-after-traks")) {
			mvex_after_traks = GF_TRUE;
		}
		else if (!stricmp(arg, "-dash-profile") || !stricmp(arg, "-profile")) {
			CHECK_NEXT_ARG
			if (!stricmp(argv[i + 1], "live") || !stricmp(argv[i + 1], "simple")) dash_profile = GF_DASH_PROFILE_LIVE;
			else if (!stricmp(argv[i + 1], "onDemand")) dash_profile = GF_DASH_PROFILE_ONDEMAND;
			else if (!stricmp(argv[i + 1], "hbbtv1.5:live")) {
				dash_profile = GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE;
			}
			else if (!stricmp(argv[i + 1], "dashavc264:live")) {
				dash_profile = GF_DASH_PROFILE_AVC264_LIVE;
			}
			else if (!stricmp(argv[i + 1], "dashavc264:onDemand")) {
				dash_profile = GF_DASH_PROFILE_AVC264_ONDEMAND;
			}
			else if (!stricmp(argv[i + 1], "main")) dash_profile = GF_DASH_PROFILE_MAIN;
			else dash_profile = GF_DASH_PROFILE_FULL;
			i++;
		}
		else if (!stricmp(arg, "-profile-ext")) {
			CHECK_NEXT_ARG
			dash_profile_extension = argv[i + 1];
			i++;
		}
		else if (!strnicmp(arg, "-url-template", 13)) {
			use_url_template = 1;
			if ((arg[13] == '=') && arg[14]) {
				if (!strcmp(&arg[14], "simulate")) use_url_template = 2;
			}
		}
		else if (!stricmp(arg, "-segment-timeline")) {
			segment_timeline = 1;
		}
		else if (!stricmp(arg, "-mem-frags")) {
			memory_frags = 1;
		}
		else if (!stricmp(arg, "-segment-marker")) {
			char *m;
			CHECK_NEXT_ARG
			m = argv[i + 1];
			segment_marker = GF_4CC(m[0], m[1], m[2], m[3]);
			i++;
		}
		else if (!stricmp(arg, "-cues")) {
			CHECK_NEXT_ARG
			dash_cues = argv[i + 1];
			i++;
		}
		else if (!stricmp(arg, "-strict-cues")) {
			strict_cues = GF_TRUE;
		}
		else if (!stricmp(arg, "-insert-utc")) {
			insert_utc = GF_TRUE;
		}
		else if (!stricmp(arg, "-udp-write")) {
			udp_dest = argv[i+1];
			i++;
		}
		else {
			u32 ret = mp4box_parse_args_continue(argc, argv, &i);
			if (ret) return ret;
		}
	}
	return 0;
}

int mp4boxMain(int argc, char **argv)
{
	nb_tsel_acts = nb_add = nb_cat = nb_track_act = nb_sdp_ex = max_ptime = nb_meta_act = rtp_rate = major_brand = nb_alt_brand_add = nb_alt_brand_rem = car_dur = minor_version = 0;
	e = GF_OK;
	split_duration = 0.0;
	split_start = -1.0;
	interleaving_time = 0;
	dash_duration = dash_subduration = 0.0;
	import_fps.num = import_fps.den = 0;
	import_flags = 0;
	split_size = 0;
	movie_time = 0;
	dump_nal = dump_saps = dump_saps_mode = 0;
	FullInter = HintInter = encode = do_log = old_interleave = do_saf = do_hash = verbose = do_mpd_rip = merge_vtt_cues = GF_FALSE;
#ifndef GPAC_DISABLE_SCENE_DUMP
	dump_mode = GF_SM_DUMP_NONE;
#endif
	Frag = force_ocr = remove_sys_tracks = agg_samples = remove_hint = keep_sys_tracks = remove_root_od = single_group = clean_groups = GF_FALSE;
	conv_type = HintIt = needSave = print_sdp = print_info = regular_iod = dump_std = open_edit = dump_rtp = dump_cr = dump_srt = dump_ttxt = force_new = dump_m2ts = dump_cart = import_subtitle = force_cat = pack_wgt = dash_live = GF_FALSE;
	no_fragments_defaults = GF_FALSE;
	single_traf_per_moof = hls_clock = GF_FALSE;
    tfdt_per_traf = GF_FALSE;
	dump_nal_crc = GF_FALSE;
	dump_isom = 0;
	/*align cat is the new default behaviour for -cat*/
	align_cat = GF_TRUE;
	subsegs_per_sidx = 0;
	track_dump_type = 0;
	crypt = 0;
	time_shift_depth = 0;
	file = NULL;
	itunes_tags = pes_dump = NULL;
	seg_name = dash_ctx_file = NULL;
	compress_top_boxes = NULL;
	initial_moof_sn = 0;
	initial_tfdt = 0;

#ifndef GPAC_DISABLE_SCENE_ENCODER
	memset(&opts, 0, sizeof(opts));
#endif

	trackID = stat_level = hint_flags = 0;
	program_number = 0;
	info_track_id = 0;
	do_flat = 0;
	inName = outName = mediaSource = input_ctx = output_ctx = drm_file = avi2raw = cprt = chap_file = pack_file = raw_cat = high_dynamc_range_filename = use_init_seg = box_patch_filename = NULL;

#ifndef GPAC_DISABLE_SWF_IMPORT
	swf_flags = GF_SM_SWF_SPLIT_TIMELINE;
#endif
	swf_flatten_angle = 0.0f;
	tmpdir = NULL;

	for (i = 1; i < (u32) argc ; i++) {
		if (!strcmp(argv[i], "-mem-track") || !strcmp(argv[i], "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(argv[i], "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", argv[i]);
#endif
			break;
		}
	}

#ifdef _TWO_DIGIT_EXPONENT
	_set_output_format(_TWO_DIGIT_EXPONENT);
#endif

	/*init libgpac*/
	gf_sys_init(mem_track, NULL);
	if (argc < 2) {
		fprintf(stderr, "Not enough arguments - check usage with -h\n"
			"MP4Box - GPAC version %s\n"
	        "%s\n", gf_gpac_version(), gf_gpac_copyright());
		gf_sys_close();
		return 0;
	}

	helpout = stdout;

	i = mp4box_parse_args(argc, argv);
	if (i) {
		return mp4box_cleanup(i - 1);
	}

	if (!inName && dump_std)
		inName = "std";

#ifndef GPAC_DISABLE_ATSC
	if (grab_atsc) {
		if (!gf_logs) {
			gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
			gf_log_set_tool_level(GF_LOG_CONTAINER, GF_LOG_INFO);
		}
		return grab_atsc3_session(atsc_output_dir, atsc_service, atsc_max_segs, atsc_stats_rate, atsc_debug_tsi);
	}
#endif

	if (!inName) {
		PrintUsage();
		return mp4box_cleanup(1);
	}
	if (!strcmp(inName, "std")) dump_std = 2;
	if (!strcmp(inName, "stdb")) {
		inName = "std";
		dump_std = 1;
	}

	if (!interleaving_time) {
		/*by default use single fragment per dash segment*/
		if (dash_duration)
			interleaving_time = dash_duration;
		else if (!do_flat) {
			interleaving_time = DEFAULT_INTERLEAVING_IN_SEC;
		}
	}

	if (dump_std)
		outName = "std";

	if (dump_std==2) {
#ifdef WIN32
		if ( _setmode(_fileno(stdout), _O_BINARY) == -1 )
#else
		if ( freopen(NULL, "wb", stdout) == NULL)
#endif
		{
			fprintf(stderr, "Fatal error: cannot reopen stdout in binary mode.\n");
			return mp4box_cleanup(1);
		}
	}

#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
	if (live_scene) {
		int ret = live_session(argc, argv);
		return mp4box_cleanup(ret);
	}
#endif

	if (gf_logs) {
	} else {
		GF_LOG_Level level = verbose ? GF_LOG_DEBUG : GF_LOG_INFO;
		gf_log_set_tool_level(GF_LOG_CONTAINER, level);
		gf_log_set_tool_level(GF_LOG_SCENE, level);
		gf_log_set_tool_level(GF_LOG_PARSER, level);
		gf_log_set_tool_level(GF_LOG_AUTHOR, level);
		gf_log_set_tool_level(GF_LOG_CODING, level);
		gf_log_set_tool_level(GF_LOG_DASH, level);
#ifdef GPAC_MEMORY_TRACKING
		if (mem_track)
			gf_log_set_tool_level(GF_LOG_MEMORY, level);
#endif
	}
	e = gf_sys_set_args(argc, (const char **) argv);
	if (e) {
		fprintf(stderr, "Error assigning libgpac arguments: %s\n", gf_error_to_string(e) );
		return mp4box_cleanup(1);
	}

	if (raw_cat) {
		char chunk[4096];
		FILE *fin, *fout;
		s64 to_copy, done;
		fin = gf_fopen(raw_cat, "rb");
		if (!fin) return mp4box_cleanup(1);

		fout = gf_fopen(inName, "a+b");
		if (!fout) {
			gf_fclose(fin);
			return mp4box_cleanup(1);
		}
		gf_fseek(fin, 0, SEEK_END);
		to_copy = gf_ftell(fin);
		gf_fseek(fin, 0, SEEK_SET);
		done = 0;
		while (1) {
			u32 nb_bytes = (u32) fread(chunk, 1, 4096, fin);
			gf_fwrite(chunk, 1, nb_bytes, fout);
			done += nb_bytes;
			fprintf(stderr, "Appending file %s - %02.2f done\r", raw_cat, 100.0*done/to_copy);
			if (done >= to_copy) break;
		}
		gf_fclose(fin);
		gf_fclose(fout);
		return mp4box_cleanup(0);
	}
	if (compress_top_boxes) {
		e = do_compress_top_boxes(inName, outName, compress_top_boxes, comp_top_box_version, comp_lzma);
		return mp4box_cleanup(e ? 1 : 0);
	}

	if (do_mpd_rip) {
		e = rip_mpd(inName, outName);
		return mp4box_cleanup(e ? 1 : 0);
	}

#ifndef GPAC_DISABLE_CORE_TOOLS
	if (do_wget != NULL) {
		e = gf_dm_wget(do_wget, inName, 0, 0, NULL);
		if (e != GF_OK) {
			fprintf(stderr, "Cannot retrieve %s: %s\n", do_wget, gf_error_to_string(e) );
			return mp4box_cleanup(1);
		}
		return mp4box_cleanup(0);
	}
#endif

	if (udp_dest) {
		GF_Socket *sock = gf_sk_new(GF_SOCK_TYPE_UDP);
		u16 port = 2345;
		char *sep = strrchr(udp_dest, ':');
		if (sep) {
			sep[0] = 0;
			port = atoi(sep+1);
		}
		e = gf_sk_bind( sock, "127.0.0.1", 0, udp_dest, port, 0);
		if (sep) sep[0] = ':';
		if (e) fprintf(stderr, "Failed to bind socket to %s: %s\n", udp_dest, gf_error_to_string(e) );
		else {
			e = gf_sk_send(sock, (u8 *) inName, (u32)strlen(inName));
			if (e) fprintf(stderr, "Failed to send datagram: %s\n", gf_error_to_string(e) );
		}
		gf_sk_del(sock);
		return 0;
	}

#ifndef GPAC_DISABLE_MPD
	if (do_mpd) {
		Bool remote = GF_FALSE;
		GF_MPD *mpd;
		char *mpd_base_url = NULL;
		if (!strnicmp(inName, "http://", 7) || !strnicmp(inName, "https://", 8)) {
#if !defined(GPAC_DISABLE_CORE_TOOLS)
			e = gf_dm_wget(inName, "tmp_main.m3u8", 0, 0, &mpd_base_url);
			if (e != GF_OK) {
				fprintf(stderr, "Cannot retrieve M3U8 (%s): %s\n", inName, gf_error_to_string(e));
				if (mpd_base_url) gf_free(mpd_base_url);
				return mp4box_cleanup(1);
			}
			remote = GF_TRUE;
#else
			gf_free(mpd_base_url);
			fprintf(stderr, "HTTP Downloader disabled in this build\n");
			return mp4box_cleanup(1);
#endif

			if (outName)
				strcpy(outfile, outName);
			else {
				const char *sep = gf_file_basename(inName);
				char *ext = gf_file_ext_start(sep);
				if (ext) ext[0] = 0;
				sprintf(outfile, "%s.mpd", sep);
				if (ext) ext[0] = '.';
			}
		} else {
			if (outName)
				strcpy(outfile, outName);
			else {
				char *dst = strdup(inName);
				char *ext = strstr(dst, ".m3u8");
				if (ext) ext[0] = 0;
				sprintf(outfile, "%s.mpd", dst);
				gf_free(dst);
			}
		}

		mpd = gf_mpd_new();
		if (!mpd) {
			e = GF_OUT_OF_MEM;
			fprintf(stderr, "[DASH] Error: MPD creation problem %s\n", gf_error_to_string(e));
			mp4box_cleanup(1);
		}
		FILE *f = gf_fopen(remote ? "tmp_main.m3u8" : inName, "r");
		u32 manif_type = 0;
		if (f) {
			char szDATA[1000];
			s32 read;
			szDATA[999]=0;
			read = (s32) fread(szDATA, 1, 999, f);
			if (read<0) read = 0;
			szDATA[read]=0;
			gf_fclose(f);
			if (strstr(szDATA, "SmoothStreamingMedia"))
				manif_type = 2;
			else if (strstr(szDATA, "#EXTM3U"))
				manif_type = 1;
		}

		if (manif_type==1) {
			e = gf_m3u8_to_mpd(remote ? "tmp_main.m3u8" : inName, mpd_base_url ? mpd_base_url : inName, outfile, 0, "video/mp2t", GF_TRUE, use_url_template, segment_timeline, NULL, mpd, GF_TRUE, GF_TRUE);
		} else if (manif_type==2) {
			e = gf_mpd_smooth_to_mpd(remote ? "tmp_main.m3u8" : inName, mpd, mpd_base_url ? mpd_base_url : inName);
		} else {
			e = GF_NOT_SUPPORTED;
		}
		if (!e)
			gf_mpd_write_file(mpd, outfile);

		if (mpd)
			gf_mpd_del(mpd);
		if (mpd_base_url)
			gf_free(mpd_base_url);

		if (remote) {
			gf_delete_file("tmp_main.m3u8");
		}
		if (e != GF_OK) {
			fprintf(stderr, "Error converting %s (%s) to MPD (%s): %s\n", (manif_type==1) ? "HLS" : "Smooth",  inName, outfile, gf_error_to_string(e));
			return mp4box_cleanup(1);
		} else {
			fprintf(stderr, "Done converting %s (%s) to MPD (%s)\n", (manif_type==1) ? "HLS" : "Smooth",  inName, outfile);
			return mp4box_cleanup(0);
		}
	}
#endif
	if (dash_duration && !nb_dash_inputs) {
		dash_inputs = set_dash_input(dash_inputs, inName, &nb_dash_inputs);
	}


	if (do_saf && !encode) {
		switch (get_file_type_by_ext(inName)) {
		case GF_FILE_TYPE_BT_WRL_X3DV:
		case GF_FILE_TYPE_XMT_X3D:
		case GF_FILE_TYPE_SVG:
			encode = GF_TRUE;
			break;
		case GF_FILE_TYPE_NOT_SUPPORTED:
		case GF_FILE_TYPE_ISO_MEDIA:
		case GF_FILE_TYPE_SWF:
		case GF_FILE_TYPE_LSR_SAF:
			break;
		}
	}

#ifndef GPAC_DISABLE_SCENE_DUMP
	if (dump_mode == GF_SM_DUMP_SVG) {
		if (strstr(inName, ".srt") || strstr(inName, ".ttxt")) import_subtitle = 2;
	}
#endif


	if (import_subtitle && !trackID) {
		/* We import the subtitle file,
		   i.e. we parse it and store the content as samples of a 3GPP Timed Text track in an ISO file,
		   possibly for later export (e.g. when converting SRT to TTXT, ...) */
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		GF_MediaImporter import;
		/* Prepare the importer */
		file = gf_isom_open("ttxt_convert", GF_ISOM_OPEN_WRITE, NULL);
		if (timescale && file) gf_isom_set_timescale(file, timescale);

		memset(&import, 0, sizeof(GF_MediaImporter));
		import.dest = file;
		import.in_name = inName;
		/* Start the import */
		e = gf_media_import(&import);
		if (e) {
			fprintf(stderr, "Error importing %s: %s\n", inName, gf_error_to_string(e));
			gf_isom_delete(file);
			gf_delete_file("ttxt_convert");
			return mp4box_cleanup(1);
		}
		/* Prepare the export */
		strcpy(outfile, inName);
		if (strchr(outfile, '.')) {
			while (outfile[strlen(outfile)-1] != '.') outfile[strlen(outfile)-1] = 0;
			outfile[strlen(outfile)-1] = 0;
		}
#ifndef GPAC_DISABLE_ISOM_DUMP
		/* Start the export of the track #1, in the appropriate dump type, indicating it's a conversion */
		dump_isom_timed_text(file, gf_isom_get_track_id(file, 1),
							  dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE,
		                      GF_TRUE,
		                      (import_subtitle==2) ? GF_TEXTDUMPTYPE_SVG : (dump_srt ? GF_TEXTDUMPTYPE_SRT : GF_TEXTDUMPTYPE_TTXT));
#endif
		/* Clean the importer */
		gf_isom_delete(file);
		gf_delete_file("ttxt_convert");
		if (e) {
			fprintf(stderr, "Error converting %s: %s\n", inName, gf_error_to_string(e));
			return mp4box_cleanup(1);
		}
		return mp4box_cleanup(0);
#else
		fprintf(stderr, "Feature not supported\n");
		return mp4box_cleanup(1);
#endif
	}

#if !defined(GPAC_DISABLE_MEDIA_IMPORT) && !defined(GPAC_DISABLE_ISOM_WRITE)
	if (nb_add) {
		u8 open_mode = GF_ISOM_OPEN_EDIT;
		if (force_new) {
			open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
		} else {
			FILE *test = gf_fopen(inName, "rb");
			if (!test) {
				open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
				if (!outName) outName = inName;
			} else {
				gf_fclose(test);
				if (! gf_isom_probe_file(inName) ) {
					open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
					if (!outName) outName = inName;
				}
			}
		}

		open_edit = GF_TRUE;
		file = gf_isom_open(inName, open_mode, tmpdir);
		if (!file) {
			fprintf(stderr, "Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) );
			return mp4box_cleanup(1);
		}
		if (freeze_box_order)
			gf_isom_freeze_order(file);

		for (i=0; i<(u32) argc; i++) {
			if (!strcmp(argv[i], "-add")) {
				char *src = argv[i+1];

				while (src) {
					char *sep = strchr(src, '+');
					if (sep) {
						sep[0] = 0;
					}

					e = import_file(file, src, import_flags, import_fps, agg_samples);

					if (sep) {
						sep[0] = '+';
						src = sep+1;
					} else {
						break;
					}
					if (e)
						break;

					if (e) {
						fprintf(stderr, "Error importing %s: %s\n", argv[i+1], gf_error_to_string(e));
						gf_isom_delete(file);
						return mp4box_cleanup(1);
					}
				}
				i++;
			}
		}

		/*unless explicitly asked, remove all systems tracks*/
		if (!keep_sys_tracks) remove_systems_tracks(file);
		needSave = GF_TRUE;
	}

	if (nb_cat) {
		if (!file) {
			u8 open_mode = GF_ISOM_OPEN_EDIT;
			if (force_new) {
				open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
			} else {
				FILE *test = gf_fopen(inName, "rb");
				if (!test) {
					open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
					if (!outName) outName = inName;
				}
				else gf_fclose(test);
			}

			open_edit = GF_TRUE;
			file = gf_isom_open(inName, open_mode, tmpdir);
			if (!file) {
				fprintf(stderr, "Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) );
				return mp4box_cleanup(1);
			}
			if (freeze_box_order)
				gf_isom_freeze_order(file);
		}
		for (i=0; i<(u32)argc; i++) {
			if (!strcmp(argv[i], "-cat") || !strcmp(argv[i], "-catx") || !strcmp(argv[i], "-catpl")) {
				e = cat_isomedia_file(file, argv[i+1], import_flags, import_fps, agg_samples, tmpdir, force_cat, align_cat, !strcmp(argv[i], "-catx") ? GF_TRUE : GF_FALSE, !strcmp(argv[i], "-catpl") ? GF_TRUE : GF_FALSE);
				if (e) {
					fprintf(stderr, "Error appending %s: %s\n", argv[i+1], gf_error_to_string(e));
					gf_isom_delete(file);
					return mp4box_cleanup(1);
				}
				i++;
			}
		}
		/*unless explicitly asked, remove all systems tracks*/
		if (!keep_sys_tracks) remove_systems_tracks(file);

		needSave = GF_TRUE;
	}
#endif /*!GPAC_DISABLE_MEDIA_IMPORT && !GPAC_DISABLE_ISOM_WRITE*/

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	else if (chunk_mode) {
		if (!inName) {
			fprintf(stderr, "chunk encoding syntax: [-outctx outDump] -inctx inScene auFile\n");
			return mp4box_cleanup(1);
		}
		e = EncodeFileChunk(inName, outName ? outName : inName, input_ctx, output_ctx, tmpdir);
		if (e) {
			fprintf(stderr, "Error encoding chunk file %s\n", gf_error_to_string(e));
			return mp4box_cleanup(1);
		}
		goto exit;
	}
#endif
	else if (encode) {
#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
		FILE *logs = NULL;
		if (do_log) {
			char logfile[5000];
			strcpy(logfile, inName);
			if (strchr(logfile, '.')) {
				while (logfile[strlen(logfile)-1] != '.') logfile[strlen(logfile)-1] = 0;
				logfile[strlen(logfile)-1] = 0;
			}
			strcat(logfile, "_enc.logs");
			logs = gf_fopen(logfile, "wt");
		}
		strcpy(outfile, outName ? outName : inName);
		if (strchr(outfile, '.')) {
			while (outfile[strlen(outfile)-1] != '.') outfile[strlen(outfile)-1] = 0;
			outfile[strlen(outfile)-1] = 0;
		}
		strcat(outfile, ".mp4");
		file = gf_isom_open(outfile, GF_ISOM_WRITE_EDIT, tmpdir);
		opts.mediaSource = mediaSource ? mediaSource : outfile;
		e = EncodeFile(inName, file, &opts, logs);
		if (logs) gf_fclose(logs);
		if (e) goto err_exit;
		needSave = GF_TRUE;
		if (do_saf) {
			needSave = GF_FALSE;
			open_edit = GF_FALSE;
		}
#endif
	}

#ifndef GPAC_DISABLE_ISOM_WRITE
	else if (pack_file) {
		char *fileName = gf_url_colon_suffix(pack_file);
		if (fileName && ((fileName - pack_file)==4)) {
			fileName[0] = 0;
			file = package_file(fileName + 1, pack_file, tmpdir, pack_wgt);
			fileName[0] = ':';
		} else {
			file = package_file(pack_file, NULL, tmpdir, pack_wgt);
			if (!file) {
				fprintf(stderr, "Failed to package file\n");
				return mp4box_cleanup(1);
			}
		}
		if (!outName) outName = inName;
		needSave = GF_TRUE;
		open_edit = GF_TRUE;
	}
#endif

	if (dash_duration) {
		Bool del_file = GF_FALSE;
		char szMPD[GF_MAX_PATH], *sep;
		char szStateFile[GF_MAX_PATH];
		Bool dyn_state_file = GF_FALSE;
		u32 do_abort = 0;
		GF_DASHSegmenter *dasher=NULL;

		if (crypt) {
			fprintf(stderr, "MP4Box cannot crypt and DASH on the same pass. Please encrypt your content first.\n");
			return mp4box_cleanup(1);
		}

		strcpy(outfile, outName ? outName : gf_url_get_resource_name(inName) );
		sep = strrchr(outfile, '.');
		if (sep) sep[0] = 0;
		if (!outName) strcat(outfile, "_dash");
		strcpy(szMPD, outfile);
		if (sep) {
			sep[0] = '.';
			strcat(szMPD, sep);
		} else {
			strcat(szMPD, ".mpd");
		}
		
		if ((dash_subduration>0) && (dash_duration > dash_subduration)) {
			fprintf(stderr, "Warning: -subdur parameter (%g s) should be greater than segment duration (%g s), using segment duration instead\n", dash_subduration, dash_duration);
			dash_subduration = dash_duration;
		}

		if (dash_mode && dash_live)
			fprintf(stderr, "Live DASH-ing - press 'q' to quit, 's' to save context and quit\n");

		if (!dash_ctx_file && dash_live) {
			u32 r1;
			u64 add = (u64) (intptr_t) &dasher;
			add ^= gf_net_get_utc();
			r1 = (u32) add ^ (u32) (add/0xFFFFFFFF);
			r1 ^= gf_rand();
 			sprintf(szStateFile, "%s/dasher_%X.xml", gf_get_default_cache_directory(), r1 );
			dash_ctx_file = szStateFile;
			dyn_state_file = GF_TRUE;
		} else if (dash_ctx_file) {
			if (force_new)
				gf_delete_file(dash_ctx_file);
		}

		if (dash_profile==GF_DASH_PROFILE_AUTO)
			dash_profile = dash_mode ? GF_DASH_PROFILE_LIVE : GF_DASH_PROFILE_FULL;

		if (!dash_mode) {
			time_shift_depth = 0;
			mpd_update_time = 0;
		} else if ((dash_profile>=GF_DASH_PROFILE_MAIN) && !use_url_template && !mpd_update_time) {
			/*use a default MPD update of dash_duration sec*/
			mpd_update_time = (Double) (dash_subduration ? dash_subduration : dash_duration);
			fprintf(stderr, "Using default MPD refresh of %g seconds\n", mpd_update_time);
		}

		if (file && needSave) {
			gf_isom_close(file);
			file = NULL;
			del_file = GF_TRUE;
		}

		/*setup dash*/
		dasher = gf_dasher_new(szMPD, dash_profile, tmpdir, dash_scale, dash_ctx_file);
		if (!dasher) {
			return mp4box_cleanup(1);
			return GF_OUT_OF_MEM;
		}
		e = gf_dasher_set_info(dasher, dash_title, cprt, dash_more_info, dash_source, NULL);
		if (e) {
			fprintf(stderr, "DASH Error: %s\n", gf_error_to_string(e));
			return mp4box_cleanup(1);
		}

		gf_dasher_set_start_date(dasher, dash_start_date);
		gf_dasher_set_location(dasher, dash_source);
		for (i=0; i < nb_mpd_base_urls; i++) {
			e = gf_dasher_add_base_url(dasher, mpd_base_urls[i]);
			if (e) {
				fprintf(stderr, "DASH Error: %s\n", gf_error_to_string(e));
				return mp4box_cleanup(1);
			}
		}

		if (segment_timeline && !use_url_template) {
			fprintf(stderr, "DASH Warning: using -segment-timeline with no -url-template. Forcing URL template.\n");
			use_url_template = GF_TRUE;
		}

		e = gf_dasher_enable_url_template(dasher, (Bool) use_url_template, seg_name, seg_ext, init_seg_ext);
		if (!e) e = gf_dasher_enable_segment_timeline(dasher, segment_timeline);
		if (!e) e = gf_dasher_enable_single_segment(dasher, single_segment);
		if (!e) e = gf_dasher_enable_single_file(dasher, single_file);
		if (!e) e = gf_dasher_set_switch_mode(dasher, bitstream_switching_mode);
		if (!e) e = gf_dasher_set_durations(dasher, dash_duration, interleaving_time, dash_subduration);
		if (!e) e = gf_dasher_enable_rap_splitting(dasher, seg_at_rap, frag_at_rap);
		if (!e) e = gf_dasher_set_segment_marker(dasher, segment_marker);
		if (!e) e = gf_dasher_enable_sidx(dasher, (subsegs_per_sidx>=0) ? 1 : 0, (u32) subsegs_per_sidx, daisy_chain_sidx, use_ssix);
		if (!e) e = gf_dasher_set_dynamic_mode(dasher, dash_mode, mpd_update_time, time_shift_depth, mpd_live_duration);
		if (!e) e = gf_dasher_set_min_buffer(dasher, min_buffer);
		if (!e) e = gf_dasher_set_ast_offset(dasher, ast_offset_ms);
		if (!e) e = gf_dasher_enable_memory_fragmenting(dasher, memory_frags);
		if (!e) e = gf_dasher_set_initial_isobmf(dasher, initial_moof_sn, initial_tfdt);
		if (!e) e = gf_dasher_configure_isobmf_default(dasher, no_fragments_defaults, pssh_mode, samplegroups_in_traf, single_traf_per_moof, tfdt_per_traf, mvex_after_traks);
		if (!e) e = gf_dasher_enable_utc_ref(dasher, insert_utc);
		if (!e) e = gf_dasher_enable_real_time(dasher, frag_real_time);
		if (!e) e = gf_dasher_set_content_protection_location_mode(dasher, cp_location_mode);
		if (!e) e = gf_dasher_set_profile_extension(dasher, dash_profile_extension);
		if (!e) e = gf_dasher_enable_cached_inputs(dasher, no_cache);
		if (!e) e = gf_dasher_enable_loop_inputs(dasher, ! no_loop);
		if (!e) e = gf_dasher_set_split_on_bound(dasher, split_on_bound);
		if (!e) e = gf_dasher_set_split_on_closest(dasher, split_on_closest);
		if (!e) e = gf_dasher_set_hls_clock(dasher, hls_clock);
		if (!e && dash_cues) e = gf_dasher_set_cues(dasher, dash_cues, strict_cues);
		if (!e && fs_dump_flags) e = gf_dasher_print_session_info(dasher, fs_dump_flags);

		for (i=0; i < nb_dash_inputs; i++) {
			if (!e) e = gf_dasher_add_input(dasher, &dash_inputs[i]);
		}
		if (e) {
			fprintf(stderr, "DASH Setup Error: %s\n", gf_error_to_string(e));
			return mp4box_cleanup(1);
		}

		dash_cumulated_time=0;

		while (1) {
			if (run_for && (dash_cumulated_time >= run_for)) {
				fprintf(stderr, "Done running, computing static MPD\n");
				do_abort = 3;
			}

			dash_prev_time=gf_sys_clock();
			if (do_abort>=2) {
				e = gf_dasher_set_dynamic_mode(dasher, GF_DASH_DYNAMIC_LAST, 0, time_shift_depth, mpd_live_duration);
			}

			if (!e) e = gf_dasher_process(dasher);
			if (!dash_live && (e==GF_EOS) ) {
				fprintf(stderr, "Nothing to dash, too early ...\n");
				e = GF_OK;
			}

			if (do_abort)
				break;

			//this happens when reading file while writing them (local playback of the live session ...)
			if (dash_live && (e==GF_IO_ERR) ) {
				fprintf(stderr, "Error dashing file (%s) but continuing ...\n", gf_error_to_string(e) );
				e = GF_OK;
			}

			if (e) break;

			if (dash_live) {
				u64 ms_in_session=0;
				u32 slept = gf_sys_clock();
				u32 sleep_for = gf_dasher_next_update_time(dasher, &ms_in_session);
				fprintf(stderr, "Next generation scheduled in %u ms (DASH time "LLU" ms)\n", sleep_for, ms_in_session);
				if (ms_in_session>=run_for) {
					dash_cumulated_time = 1+run_for;
					continue;
				}

				while (1) {
					if (gf_prompt_has_input()) {
						char c = (char) gf_prompt_get_char();
						if (c=='X') {
							do_abort = 1;
							break;
						}
						if (c=='q') {
							do_abort = 2;
							break;
						}
						if (c=='s') {
							do_abort = 3;
							break;
						}
					}

					if (dash_mode == GF_DASH_DYNAMIC_DEBUG) {
						break;
					}
					if (!sleep_for) break;

					gf_sleep(sleep_for/10);
					sleep_for = gf_dasher_next_update_time(dasher, NULL);
					if (sleep_for<=1) {
						dash_now_time=gf_sys_clock();
						dash_cumulated_time+=(dash_now_time-dash_prev_time);
						fprintf(stderr, "Slept for %d ms before generation, dash cumulated time %d\n", dash_now_time - slept, dash_cumulated_time);
						break;
					}
				}
			} else {
				break;
			}
		}

		gf_dasher_del(dasher);

		if (!run_for && dash_ctx_file && (do_abort==3) && (dyn_state_file) && !gf_sys_is_test_mode() ) {
			char szName[1024];
			fprintf(stderr, "Enter file name to save dash context:\n");
			if (scanf("%s", szName) == 1) {
				gf_move_file(dash_ctx_file, szName);
			}
		}
		if (e) fprintf(stderr, "Error DASHing file: %s\n", gf_error_to_string(e));
		if (file) gf_isom_delete(file);
		if (del_file)
			gf_delete_file(inName);

		if (e) return mp4box_cleanup(1);
		goto exit;
	}

	else if (!file && !do_hash
#ifndef GPAC_DISABLE_MEDIA_EXPORT
	         && !(track_dump_type & GF_EXPORT_AVI_NATIVE)
#endif
	        ) {
		FILE *st = gf_fopen(inName, "rb");
		Bool file_exists = 0;
		u8 omode;
		if (st) {
			file_exists = 1;
			gf_fclose(st);
		}
		switch (get_file_type_by_ext(inName)) {
		case 1:
			omode =  (u8) (force_new ? GF_ISOM_WRITE_EDIT : (open_edit ? GF_ISOM_OPEN_EDIT : ( ((dump_isom>0) || print_info) ? GF_ISOM_OPEN_READ_DUMP : GF_ISOM_OPEN_READ) ) );

			if (crypt) {
				omode = GF_ISOM_OPEN_CAT_FRAGMENTS;
				if (use_init_seg)
					file = gf_isom_open(use_init_seg, GF_ISOM_OPEN_READ, tmpdir);
			}
			if (!crypt && use_init_seg) {
				file = gf_isom_open(use_init_seg, GF_ISOM_OPEN_READ_DUMP, tmpdir);
				if (file) {
					e = gf_isom_open_segment(file, inName, 0, 0, 0);
					if (e) {
						fprintf(stderr, "Error opening segment %s: %s\n", inName, gf_error_to_string(e) );
						gf_isom_delete(file);
						file = NULL;
					}
				}
			}
			if (!file)
				file = gf_isom_open(inName, omode, tmpdir);

			if (!file && (gf_isom_last_error(NULL) == GF_ISOM_INCOMPLETE_FILE) && !open_edit) {
				u64 missing_bytes;
				e = gf_isom_open_progressive(inName, 0, 0, &file, &missing_bytes);
				fprintf(stderr, "Truncated file - missing "LLD" bytes\n", missing_bytes);
			}

			if (!file) {
				if (open_edit && nb_meta_act) {
					file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, tmpdir);
					if (!outName && file) outName = inName;
				}

				if (!file) {
					fprintf(stderr, "Error opening file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)));
					return mp4box_cleanup(1);
				}
			}
			if (freeze_box_order)
					gf_isom_freeze_order(file);
			break;
		/*allowed for bt<->xmt*/
		case 2:
		case 3:
		/*allowed for svg->lsr**/
		case 4:
		/*allowed for swf->bt, swf->xmt, swf->svg*/
		case 5:
			break;
		/*used for .saf / .lsr dump*/
		case 6:
#ifndef GPAC_DISABLE_SCENE_DUMP
			if ((dump_mode==GF_SM_DUMP_LASER) || (dump_mode==GF_SM_DUMP_SVG)) {
				break;
			}
#endif

		default:
			if (!open_edit && file_exists && !gf_isom_probe_file(inName) && track_dump_type) {
			}
#ifndef GPAC_DISABLE_ISOM_WRITE
			else if (!open_edit && file_exists /* && !gf_isom_probe_file(inName) */
#ifndef GPAC_DISABLE_SCENE_DUMP
			         && dump_mode == GF_SM_DUMP_NONE
#endif
			        ) {
				/*************************************************************************************************/
#ifndef GPAC_DISABLE_MEDIA_IMPORT
				if(dvbhdemux)
				{
					GF_MediaImporter import;
					file = gf_isom_open("ttxt_convert", GF_ISOM_OPEN_WRITE, NULL);
					memset(&import, 0, sizeof(GF_MediaImporter));
					import.dest = file;
					import.in_name = inName;
					import.flags = GF_IMPORT_MPE_DEMUX;
					e = gf_media_import(&import);
					if (e) {
						fprintf(stderr, "Error importing %s: %s\n", inName, gf_error_to_string(e));
						gf_isom_delete(file);
						gf_delete_file("ttxt_convert");
						return mp4box_cleanup(1);
					}
				}
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/

				if (dump_m2ts) {
#ifndef GPAC_DISABLE_MPEG2TS
					dump_mpeg2_ts(inName, pes_dump, program_number);
#endif
				} else if (dump_timestamps) {
#ifndef GPAC_DISABLE_MPEG2TS
					dump_mpeg2_ts(inName, pes_dump, program_number);
#endif
#ifndef GPAC_DISABLE_CORE_TOOLS
				} else if (do_bin_xml) {
					xml_bs_to_bin(inName, outName, dump_std);
#endif
				} else if (do_hash) {
					hash_file(inName, dump_std);
				} else {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
					convert_file_info(inName, info_track_id);
#endif
				}
				goto exit;
			}
#endif /*GPAC_DISABLE_ISOM_WRITE*/
			else if (open_edit) {
				file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, tmpdir);
				if (!outName && file) outName = inName;
			} else if (!file_exists) {
				fprintf(stderr, "Error creating file %s: %s\n", inName, gf_error_to_string(GF_URL_ERROR));
				return mp4box_cleanup(1);
			} else {
				fprintf(stderr, "Cannot open %s - extension not supported\n", inName);
				return mp4box_cleanup(1);
			}
		}
	}

	if (high_dynamc_range_filename) {
		e = parse_high_dynamc_range_xml_desc(file, high_dynamc_range_filename);
		if (e) goto err_exit;
	}

	if (file && keep_utc && open_edit) {
		gf_isom_keep_utc_times(file, 1);
	}

	strcpy(outfile, outName ? outName : inName);
	{

		char *szExt = gf_file_ext_start(outfile);

		if (szExt)
		{
			/*turn on 3GP saving*/
			if (!stricmp(szExt, ".3gp") || !stricmp(szExt, ".3gpp") || !stricmp(szExt, ".3g2"))
				conv_type = GF_ISOM_CONV_TYPE_3GPP;
			else if (!stricmp(szExt, ".m4a") || !stricmp(szExt, ".m4v"))
				conv_type = GF_ISOM_CONV_TYPE_IPOD;
			else if (!stricmp(szExt, ".psp"))
				conv_type = GF_ISOM_CONV_TYPE_PSP;
			else if (!stricmp(szExt, ".mov"))
				conv_type = GF_ISOM_CONV_TYPE_MOV;

			//remove extension from outfile
			*szExt = 0;
		}
	}

#ifndef GPAC_DISABLE_MEDIA_EXPORT
	if (track_dump_type & GF_EXPORT_AVI_NATIVE) {
		char szFile[1024];
		GF_MediaExporter mdump;
		memset(&mdump, 0, sizeof(mdump));
		mdump.in_name = inName;
		mdump.flags = GF_EXPORT_AVI_NATIVE;
		mdump.trackID = trackID;
		if (dump_std) {
			mdump.out_name = "std";
		} else if (outName) {
			mdump.out_name = outName;
		} else if (trackID>2) {
			sprintf(szFile, "%s_audio%d", outfile, trackID-1);
			mdump.out_name = szFile;
		} else {
			sprintf(szFile, "%s_%s", outfile, (trackID==1) ? "video" : "audio");
			mdump.out_name = szFile;
		}

		mdump.print_stats_graph = fs_dump_flags;
		e = gf_media_export(&mdump);
		if (e) goto err_exit;
		goto exit;
	}
	if (!open_edit && !gf_isom_probe_file(inName) && track_dump_type) {
		GF_MediaExporter mdump;
		char szFile[1024];
		for (i=0; i<nb_track_act; i++) {
			TrackAction *tka = &tracks[i];
			if (tka->act_type != TRAC_ACTION_RAW_EXTRACT) continue;
			memset(&mdump, 0, sizeof(mdump));
			mdump.in_name = inName;
			mdump.flags = tka->dump_type;
			mdump.trackID = tka->trackID;
			mdump.sample_num = tka->sample_num;
			if (outName) {
				mdump.out_name = outName;
				mdump.flags |= GF_EXPORT_MERGE;
			} else if (nb_track_act>1) {
				sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
				mdump.out_name = szFile;
			} else {
				mdump.out_name = outfile;
			}
			mdump.print_stats_graph = fs_dump_flags;
			e = gf_media_export(&mdump);
			if (e) goto err_exit;
		}
		goto exit;
	}

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/

#ifndef GPAC_DISABLE_SCENE_DUMP
	if (dump_mode != GF_SM_DUMP_NONE) {
		e = dump_isom_scene(inName, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_mode, do_log);
		if (e) goto err_exit;
	}
#endif

#ifndef GPAC_DISABLE_SCENE_STATS
	if (stat_level) dump_isom_scene_stats(inName, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, stat_level);
#endif

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (!HintIt && print_sdp) dump_isom_sdp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
#endif
	if (print_info) {
		if (!file) {
			fprintf(stderr, "Cannot print info on a non ISOM file (%s)\n", inName);
		} else {
			if (info_track_id) DumpTrackInfo(file, info_track_id, 1);
			else DumpMovieInfo(file);
		}
	}
#ifndef GPAC_DISABLE_ISOM_DUMP
	if (dump_isom) {
		e = dump_isom_xml(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, (dump_isom==2) ? GF_TRUE : GF_FALSE, merge_vtt_cues, use_init_seg ? GF_TRUE : GF_FALSE);
		if (e) goto err_exit;
	}
	if (dump_cr) dump_isom_ismacryp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
	if ((dump_ttxt || dump_srt) && trackID) {

		if (trackID == (u32)-1) {

			u32 j;
			for (j=0; j<gf_isom_get_track_count(file); j++) {
				trackID = gf_isom_get_track_id(file, j+1);
				dump_isom_timed_text(file, trackID, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE,
									GF_FALSE, dump_srt ? GF_TEXTDUMPTYPE_SRT : GF_TEXTDUMPTYPE_TTXT);
			}

		}
		else {
			dump_isom_timed_text(file, trackID, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE,
								GF_FALSE, dump_srt ? GF_TEXTDUMPTYPE_SRT : GF_TEXTDUMPTYPE_TTXT);
		}
	}

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (dump_rtp) dump_isom_rtp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
#endif

#endif

	if (dump_timestamps) dump_isom_timestamps(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_timestamps);
	if (dump_nal) dump_isom_nal(file, dump_nal, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_nal_crc);
	if (dump_saps) dump_isom_saps(file, dump_saps, dump_saps_mode, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);

	if (do_hash) {
		e = hash_file(inName, dump_std);
		if (e) goto err_exit;
	}
#ifndef GPAC_DISABLE_CORE_TOOLS
	if (do_bin_xml) {
		e = xml_bs_to_bin(inName, outName, dump_std);
		if (e) goto err_exit;
	}
#endif

	if (dump_cart) dump_isom_cover_art(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
	if (dump_chap) dump_isom_chapters(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE,(dump_chap==2) ? 1 : 0);
	if (dump_udta_type) dump_isom_udta(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_udta_type, dump_udta_track);

	if (dump_iod) {
		GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *)gf_isom_get_root_od(file);
		if (!iod) {
			fprintf(stderr, "File %s has no IOD", inName);
		} else {
			char szName[GF_MAX_PATH];
			FILE *iodf;
			GF_BitStream *bs = NULL;

			sprintf(szName, "%s.iod", outfile);
			iodf = gf_fopen(szName, "wb");
			if (!iodf) {
				fprintf(stderr, "Cannot open destination %s\n", szName);
			} else {
				u8 *desc;
				u32 size;
				bs = gf_bs_from_file(iodf, GF_BITSTREAM_WRITE);
				if (gf_odf_desc_write((GF_Descriptor *)iod, &desc, &size)==GF_OK) {
					gf_fwrite(desc, 1, size, iodf);
					gf_free(desc);
				} else {
					fprintf(stderr, "Error writing IOD %s\n", szName);
				}
				gf_fclose(iodf);
			}
			gf_free(bs);
			gf_odf_desc_del((GF_Descriptor*)iod);
		}
	}

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	if (split_duration || split_size) {
		split_isomedia_file(file, split_duration, split_size, inName, interleaving_time, split_start, adjust_split_end, outName, tmpdir, seg_at_rap);
		/*never save file when splitting is desired*/
		open_edit = GF_FALSE;
		needSave = GF_FALSE;
	}
#endif

#ifndef GPAC_DISABLE_MEDIA_EXPORT
	if (track_dump_type) {
		char szFile[1024];
		GF_MediaExporter mdump;
		for (i=0; i<nb_track_act; i++) {
			TrackAction *tka = &tracks[i];
			if (tka->act_type != TRAC_ACTION_RAW_EXTRACT) continue;
			memset(&mdump, 0, sizeof(mdump));
			mdump.file = file;
			mdump.flags = tka->dump_type;
			mdump.trackID = tka->trackID;
			mdump.sample_num = tka->sample_num;
			if (tka->out_name) {
				mdump.out_name = tka->out_name;
			} else if (outName) {
				mdump.out_name = outName;
				mdump.flags |= GF_EXPORT_MERGE;
			} else if (mdump.trackID) {
				sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
				mdump.out_name = szFile;
			} else {
				sprintf(szFile, "%s_export", outfile);
				mdump.out_name = szFile;
			}
			if (tka->trackID==(u32) -1) {
				u32 j;
				for (j=0; j<gf_isom_get_track_count(file); j++) {
					mdump.trackID = gf_isom_get_track_id(file, j+1);
					sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
					mdump.out_name = szFile;
					mdump.print_stats_graph = fs_dump_flags;
					e = gf_media_export(&mdump);
					if (e) goto err_exit;
				}
			} else {
				mdump.print_stats_graph = fs_dump_flags;
				e = gf_media_export(&mdump);
				if (e) goto err_exit;
			}
		}
	} else if (do_saf) {
		GF_MediaExporter mdump;
		memset(&mdump, 0, sizeof(mdump));
		mdump.file = file;
		mdump.flags = GF_EXPORT_SAF;
		mdump.out_name = outfile;
		mdump.print_stats_graph = fs_dump_flags;
		e = gf_media_export(&mdump);
		if (e) goto err_exit;
	}
#endif

	for (i=0; i<nb_meta_act; i++) {
		u32 tk = 0;
		Bool self_ref;
		MetaAction *meta = &metas[i];

		if (meta->trackID) tk = gf_isom_get_track_by_id(file, meta->trackID);

		switch (meta->act_type) {
#ifndef GPAC_DISABLE_ISOM_WRITE
		case META_ACTION_SET_TYPE:
			/*note: we don't handle file brand modification, this is an author stuff and cannot be guessed from meta type*/
			e = gf_isom_set_meta_type(file, meta->root_meta, tk, meta->meta_4cc);
			gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_ISO2, 1);
			needSave = GF_TRUE;
			break;
		case META_ACTION_ADD_ITEM:
			self_ref = !stricmp(meta->szPath, "NULL") || !stricmp(meta->szPath, "this") || !stricmp(meta->szPath, "self");
			e = gf_isom_add_meta_item(file, meta->root_meta, tk, self_ref, self_ref ? NULL : meta->szPath,
			                          meta->szName,
			                          meta->item_id,
									  meta->item_type,
			                          meta->mime_type,
			                          meta->enc_type,
			                          meta->use_dref ? meta->szPath : NULL,  NULL,
			                          meta->image_props);
			if (meta->ref_type) {
				e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, meta->ref_item_id, meta->ref_type, NULL);
			}
			needSave = GF_TRUE;
			break;
		case META_ACTION_ADD_IMAGE_ITEM:
		{
			u32 old_tk_count = gf_isom_get_track_count(file);
			GF_Fraction _frac = {0,0};
			e = import_file(file, meta->szPath, 0, _frac, 0);
			if (e == GF_OK) {
				u32 meta_type = gf_isom_get_meta_type(file, meta->root_meta, tk);
				if (!meta_type) {
					e = gf_isom_set_meta_type(file, meta->root_meta, tk, GF_META_ITEM_TYPE_PICT);
				} else {
					if (meta_type != GF_META_ITEM_TYPE_PICT) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Warning: file already has a root 'meta' box of type %s\n", gf_4cc_to_str(meta_type)));
						e = GF_BAD_PARAM;
					}
				}
				if (e == GF_OK) {
					if (!meta->item_id) {
						e = gf_isom_meta_get_next_item_id(file, meta->root_meta, tk, &meta->item_id);
					}
					if (e == GF_OK) {
						e = gf_isom_iff_create_image_item_from_track(file, meta->root_meta, tk, 1,
								meta->szName,
								meta->item_id,
								meta->image_props, NULL);
						if (e == GF_OK && meta->primary) {
							e = gf_isom_set_meta_primary_item(file, meta->root_meta, tk, meta->item_id);
						}
						if (e == GF_OK && meta->ref_type) {
							e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, meta->ref_item_id, meta->ref_type, NULL);
						}
					}
				}
			}
			gf_isom_remove_track(file, old_tk_count+1);
			needSave = GF_TRUE;
		}
			break;
		case META_ACTION_REM_ITEM:
			e = gf_isom_remove_meta_item(file, meta->root_meta, tk, meta->item_id);
			needSave = GF_TRUE;
			break;
		case META_ACTION_SET_PRIMARY_ITEM:
			e = gf_isom_set_meta_primary_item(file, meta->root_meta, tk, meta->item_id);
			needSave = GF_TRUE;
			break;
		case META_ACTION_SET_XML:
		case META_ACTION_SET_BINARY_XML:
			e = gf_isom_set_meta_xml(file, meta->root_meta, tk, meta->szPath, (meta->act_type==META_ACTION_SET_BINARY_XML) ? 1 : 0);
			needSave = GF_TRUE;
			break;
		case META_ACTION_REM_XML:
			if (gf_isom_get_meta_item_count(file, meta->root_meta, tk)) {
				e = gf_isom_remove_meta_xml(file, meta->root_meta, tk);
				needSave = GF_TRUE;
			} else {
				fprintf(stderr, "No meta box in input file\n");
			}
			break;
		case META_ACTION_DUMP_ITEM:
			if (gf_isom_get_meta_item_count(file, meta->root_meta, tk)) {
				e = gf_isom_extract_meta_item(file, meta->root_meta, tk, meta->item_id, strlen(meta->szPath) ? meta->szPath : NULL);
			} else {
				fprintf(stderr, "No meta box in input file\n");
			}
			break;
#endif
		case META_ACTION_DUMP_XML:
			if (gf_isom_has_meta_xml(file, meta->root_meta, tk)) {
				e = gf_isom_extract_meta_xml(file, meta->root_meta, tk, meta->szPath, NULL);
			} else {
				fprintf(stderr, "No meta box in input file\n");
			}
			break;
		default:
			break;
		}
		if (meta->image_props) {
			gf_free(meta->image_props);
			meta->image_props = NULL;
		}
		if (e) goto err_exit;
	}
	if (!open_edit && !needSave) {
		if (file) gf_isom_delete(file);
		goto exit;
	}


#ifndef GPAC_DISABLE_ISOM_WRITE
	if (clean_groups) {
		e = gf_isom_reset_switch_parameters(file);
		if (e) goto err_exit;
		needSave = GF_TRUE;
	}

	for (i=0; i<nb_tsel_acts; i++) {
		switch (tsel_acts[i].act_type) {
		case TSEL_ACTION_SET_PARAM:
			e = gf_isom_set_track_switch_parameter(file,
			                                       gf_isom_get_track_by_id(file, tsel_acts[i].trackID),
			                                       tsel_acts[i].refTrackID ? gf_isom_get_track_by_id(file, tsel_acts[i].refTrackID) : 0,
			                                       tsel_acts[i].is_switchGroup ? 1 : 0,
			                                       &tsel_acts[i].switchGroupID,
			                                       tsel_acts[i].criteria, tsel_acts[i].nb_criteria);
			if (e == GF_BAD_PARAM) {
				u32 alternateGroupID, nb_groups;
				gf_isom_get_track_switch_group_count(file, gf_isom_get_track_by_id(file, tsel_acts[i].trackID), &alternateGroupID, &nb_groups);
				if (alternateGroupID)
					fprintf(stderr, "Hint: for adding more tracks to group, using: -group-add -refTrack=ID1:[criteria:]trackID=ID2\n");
				else
					fprintf(stderr, "Hint: for creates a new grouping information, using -group-add -trackID=ID1:[criteria:]trackID=ID2\n");
			}
			if (e) goto err_exit;
			needSave = GF_TRUE;
			break;
		case TSEL_ACTION_REMOVE_TSEL:
			e = gf_isom_reset_track_switch_parameter(file, gf_isom_get_track_by_id(file, tsel_acts[i].trackID), 0);
			if (e) goto err_exit;
			needSave = GF_TRUE;
			break;
		case TSEL_ACTION_REMOVE_ALL_TSEL_IN_GROUP:
			e = gf_isom_reset_track_switch_parameter(file, gf_isom_get_track_by_id(file, tsel_acts[i].trackID), 1);
			if (e) goto err_exit;
			needSave = GF_TRUE;
			break;
		default:
			break;
		}
	}

	if (remove_sys_tracks) {
#ifndef GPAC_DISABLE_AV_PARSERS
		remove_systems_tracks(file);
#endif
		needSave = GF_TRUE;
		if (conv_type < GF_ISOM_CONV_TYPE_ISMA_EX) conv_type = 0;
	}
	if (remove_root_od) {
		gf_isom_remove_root_od(file);
		needSave = GF_TRUE;
	}
#ifndef GPAC_DISABLE_ISOM_HINTING
	if (remove_hint) {
		for (i=0; i<gf_isom_get_track_count(file); i++) {
			if (gf_isom_get_media_type(file, i+1) == GF_ISOM_MEDIA_HINT) {
				fprintf(stderr, "Removing hint track ID %d\n", gf_isom_get_track_id(file, i+1));
				gf_isom_remove_track(file, i+1);
				i--;
			}
		}
		gf_isom_sdp_clean(file);
		needSave = GF_TRUE;
	}
#endif

	if (timescale && (timescale != gf_isom_get_timescale(file))) {
		gf_isom_set_timescale(file, timescale);
		needSave = GF_TRUE;
	}

	if (!encode) {
		if (!file) {
			fprintf(stderr, "Nothing to do - exiting\n");
			goto exit;
		}
		if (outName) {
			strcpy(outfile, outName);
		} else {
			char *rel_name = strrchr(inName, GF_PATH_SEPARATOR);
			if (!rel_name) rel_name = strrchr(inName, '/');

			strcpy(outfile, "");
			if (tmpdir) {
				strcpy(outfile, tmpdir);
				if (!strchr("\\/", tmpdir[strlen(tmpdir)-1])) strcat(outfile, "/");
			}
			if (!pack_file) strcat(outfile, "out_");
			strcat(outfile, rel_name ? rel_name + 1 : inName);

			if (pack_file) {
				strcpy(outfile, rel_name ? rel_name + 1 : inName);
				rel_name = strrchr(outfile, '.');
				if (rel_name) rel_name[0] = 0;
				strcat(outfile, ".m21");
			}
		}
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		if ((conv_type == GF_ISOM_CONV_TYPE_ISMA) || (conv_type == GF_ISOM_CONV_TYPE_ISMA_EX)) {
			fprintf(stderr, "Converting to ISMA Audio-Video MP4 file...\n");
			/*keep ESIDs when doing ISMACryp*/
			e = gf_media_make_isma(file, crypt ? 1 : 0, GF_FALSE, (conv_type==GF_ISOM_CONV_TYPE_ISMA_EX) ? 1 : 0);
			if (e) goto err_exit;
			needSave = GF_TRUE;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_3GPP) {
			fprintf(stderr, "Converting to 3GP file...\n");
			e = gf_media_make_3gpp(file);
			if (e) goto err_exit;
			needSave = GF_TRUE;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_PSP) {
			fprintf(stderr, "Converting to PSP file...\n");
			e = gf_media_make_psp(file);
			if (e) goto err_exit;
			needSave = GF_TRUE;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_MOV) {
			e = gf_media_check_qt_prores(file);
			if (e) goto err_exit;
			needSave = GF_TRUE;
			if (interleaving_time) interleaving_time = 0.5;
		}
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/
		if (conv_type == GF_ISOM_CONV_TYPE_IPOD) {
			u32 major_brand = 0;

			fprintf(stderr, "Setting up iTunes/iPod file...\n");

			for (i=0; i<gf_isom_get_track_count(file); i++) {
				u32 mType = gf_isom_get_media_type(file, i+1);
				switch (mType) {
				case GF_ISOM_MEDIA_VISUAL:
                case GF_ISOM_MEDIA_AUXV:
                case GF_ISOM_MEDIA_PICT:
					major_brand = GF_ISOM_BRAND_M4V;
					gf_isom_set_ipod_compatible(file, i+1);
#if 0
					switch (gf_isom_get_media_subtype(file, i+1, 1)) {
					case GF_ISOM_SUBTYPE_AVC_H264:
					case GF_ISOM_SUBTYPE_AVC2_H264:
					case GF_ISOM_SUBTYPE_AVC3_H264:
					case GF_ISOM_SUBTYPE_AVC4_H264:
						fprintf(stderr, "Forcing AVC/H264 SAR to 1:1...\n");
						gf_media_change_par(file, i+1, 1, 1);
						break;
					}
#endif
					break;
				case GF_ISOM_MEDIA_AUDIO:
					if (!major_brand) major_brand = GF_ISOM_BRAND_M4A;
					else gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_M4A, 1);
					break;
				case GF_ISOM_MEDIA_TEXT:
					/*this is a text track track*/
					if (gf_isom_get_media_subtype(file, i+1, 1) == GF_ISOM_SUBTYPE_TX3G) {
						u32 j;
						Bool is_chap = 0;
						for (j=0; j<gf_isom_get_track_count(file); j++) {
							s32 count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_CHAP);
							if (count>0) {
								u32 tk, k;
								for (k=0; k<(u32) count; k++) {
									gf_isom_get_reference(file, j+1, GF_ISOM_REF_CHAP, k+1, &tk);
									if (tk==i+1) {
										is_chap = 1;
										break;
									}
								}
								if (is_chap) break;
							}
							if (is_chap) break;
						}
						/*this is a subtitle track*/
						if (!is_chap)
							gf_isom_set_media_type(file, i+1, GF_ISOM_MEDIA_SUBT);
					}
					break;
				}
			}
			gf_isom_set_brand_info(file, major_brand, 1);
			gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_MP42, 1);
			needSave = GF_TRUE;
		}

	} else if (outName) {
		strcpy(outfile, outName);
	}

	for (j=0; j<nb_track_act; j++) {
		TrackAction *tka = &tracks[j];
		u32 track = tka->trackID ? gf_isom_get_track_by_id(file, tka->trackID) : 0;
		u32 timescale = gf_isom_get_timescale(file);
		switch (tka->act_type) {
		case TRAC_ACTION_REM_TRACK:
			e = gf_isom_remove_track(file, track);
			if (e) {
				fprintf(stderr, "Error Removing track ID %d: %s\n", tka->trackID, gf_error_to_string(e));
			} else {
				fprintf(stderr, "Removing track ID %d\n", tka->trackID);
			}
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_LANGUAGE:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_set_media_language(file, i+1, tka->lang);
				if (e) goto err_exit;
				needSave = GF_TRUE;
			}
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_KIND:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_add_track_kind(file, i+1, tka->kind_scheme, tka->kind_value);
				if (e) goto err_exit;
				needSave = GF_TRUE;
			}
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_REM_KIND:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_remove_track_kind(file, i+1, tka->kind_scheme, tka->kind_value);
				if (e) goto err_exit;
				needSave = GF_TRUE;
			}
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_DELAY:
			if (tka->delay_ms) {
				u64 tk_dur;

				gf_isom_remove_edit_segments(file, track);
				tk_dur = gf_isom_get_track_duration(file, track);
				if (gf_isom_get_edit_segment_count(file, track))
					needSave = GF_TRUE;
				if (tka->delay_ms>0) {
					gf_isom_append_edit_segment(file, track, (timescale*tka->delay_ms)/1000, 0, GF_ISOM_EDIT_EMPTY);
					gf_isom_append_edit_segment(file, track, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
					needSave = GF_TRUE;
				} else {
					u64 to_skip = (timescale*(-tka->delay_ms))/1000;
					if (to_skip<tk_dur) {
						u64 media_time = (-tka->delay_ms)*gf_isom_get_media_timescale(file, track) / 1000;
						gf_isom_append_edit_segment(file, track, tk_dur-to_skip, media_time, GF_ISOM_EDIT_NORMAL);
						needSave = GF_TRUE;
					} else {
						fprintf(stderr, "Warning: request negative delay longer than track duration - ignoring\n");
					}
				}
			} else if (gf_isom_get_edit_segment_count(file, track)) {
				gf_isom_remove_edit_segments(file, track);
				needSave = GF_TRUE;
			}
			break;
		case TRAC_ACTION_SET_KMS_URI:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				if (!gf_isom_is_media_encrypted(file, i+1, 1)) continue;
				if (!gf_isom_is_ismacryp_media(file, i+1, 1)) continue;
				e = gf_isom_change_ismacryp_protection(file, i+1, 1, NULL, (char *) tka->kms);
				if (e) goto err_exit;
				needSave = GF_TRUE;
			}
			break;
		case TRAC_ACTION_SET_ID:
			if (!tka->trackID && (gf_isom_get_track_count(file) == 1)) {
				fprintf(stderr, "Warning: track id is not specified, but file has only one track - assume that you want to change id for this track\n");
				track = 1;
			}
			if (track) {
				u32 newTrack;
				newTrack = gf_isom_get_track_by_id(file, tka->newTrackID);
				if (newTrack != 0) {
					fprintf(stderr, "Error: Cannot set track id with value %d because a track already exists - ignoring", tka->newTrackID);
				} else {
					e = gf_isom_set_track_id(file, track, tka->newTrackID);
					needSave = GF_TRUE;
				}
			} else {
				fprintf(stderr, "Error: Cannot change id for track %d because it does not exist - ignoring", tka->trackID);
			}
			break;
		case TRAC_ACTION_SWAP_ID:
			if (track) {
				u32 tk1, tk2;
				tk1 = gf_isom_get_track_by_id(file, tka->trackID);
				tk2 = gf_isom_get_track_by_id(file, tka->newTrackID);
				if (!tk1 || !tk2) {
					fprintf(stderr, "Error: Cannot swap track IDs because not existing - ignoring");
				} else {
					e = gf_isom_set_track_id(file, tk2, 0);
					if (!e) e = gf_isom_set_track_id(file, tk1, tka->newTrackID);
					if (!e) e = gf_isom_set_track_id(file, tk2, tka->trackID);
					needSave = GF_TRUE;
				}
			} else {
				fprintf(stderr, "Error: Cannot change id for track %d because it does not exist - ignoring", tka->trackID);
			}
			break;
		case TRAC_ACTION_SET_PAR:
			e = gf_media_change_par(file, track, tka->par_num, tka->par_den, GF_FALSE);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_CLAP:
			e = gf_isom_set_clean_aperture(file, track, 1, tka->clap_wnum, tka->clap_wden, tka->clap_hnum, tka->clap_hden, tka->clap_honum, tka->clap_hoden, tka->clap_vonum, tka->clap_voden);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_MX:
			e = gf_isom_set_track_matrix(file, track, tka->mx);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_HANDLER_NAME:
			e = gf_isom_set_handler_name(file, track, tka->hdl_name);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_ENABLE:
			if (!gf_isom_is_track_enabled(file, track)) {
				e = gf_isom_set_track_enabled(file, track, 1);
				needSave = GF_TRUE;
			}
			break;
		case TRAC_ACTION_DISABLE:
			if (gf_isom_is_track_enabled(file, track)) {
				e = gf_isom_set_track_enabled(file, track, 0);
				needSave = GF_TRUE;
			}
			break;
		case TRAC_ACTION_REFERENCE:
			e = gf_isom_set_track_reference(file, track, GF_4CC(tka->lang[0], tka->lang[1], tka->lang[2], tka->lang[3]), (u32) tka->delay_ms);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_REM_NON_RAP:
			fprintf(stderr, "Removing non-rap samples from track %d\n", tka->trackID);
			e = gf_media_remove_non_rap(file, track, GF_FALSE);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_REM_NON_REFS:
			fprintf(stderr, "Removing non-reference samples from track %d\n", tka->trackID);
			e = gf_media_remove_non_rap(file, track, GF_TRUE);
			needSave = GF_TRUE;
			break;
		case TRAC_ACTION_SET_UDTA:
			fprintf(stderr, "Assigning udta box\n");
			e = set_file_udta(file, track, tka->udta_type, tka->src_name, tka->sample_num ? GF_TRUE : GF_FALSE);
			if (e) goto err_exit;
			needSave = GF_TRUE;
			break;
		default:
			break;
		}
		if (e) goto err_exit;
	}

	if (itunes_tags) {
		char *tags = itunes_tags;

		while (tags) {
			char *val;
			char *sep = gf_url_colon_suffix(tags);
			u32 tlen, itag = 0;
			if (sep) {
				while (sep) {
					for (itag=0; itag<nb_itunes_tags; itag++) {
						if (!strnicmp(sep+1, itags[itag].name, strlen(itags[itag].name))) break;
					}
					if (itag<nb_itunes_tags) {
						break;
					}
					sep = gf_url_colon_suffix(sep);
				}
				if (sep) sep[0] = 0;
			}
			for (itag=0; itag<nb_itunes_tags; itag++) {
				if (!strnicmp(tags, itags[itag].name, strlen(itags[itag].name))) {
					break;
				}
			}
			if (itag==nb_itunes_tags) {
				fprintf(stderr, "Invalid iTune tag format \"%s\" - ignoring\n", tags);
				tags = NULL;
				continue;
			}
			itag = itags[itag].code;

			val = strchr(tags, '=');
			if (!val) {
				fprintf(stderr, "Invalid iTune tag format \"%s\" (expecting '=') - ignoring\n", tags);
				tags = NULL;
				continue;
			}
			if (val) {
				val ++;
				if ((val[0]==':') || !val[0] || !stricmp(val, "NULL") ) val = NULL;
			}

			tlen = val ? (u32) strlen(val) : 0;
			switch (itag) {
			case GF_ISOM_ITUNE_COVER_ART:
			{
				u8 *d=NULL;
				char *ext;

				if (val) {
					gf_file_load_data(val, (u8 **) &d, &tlen);

					ext = strrchr(val, '.');
					if (!stricmp(ext, ".png")) tlen |= 0x80000000;
				}
				e = gf_isom_apple_set_tag(file, GF_ISOM_ITUNE_COVER_ART, d, tlen);
				if (d) gf_free(d);
			}
			break;
			case GF_ISOM_ITUNE_TEMPO:
				gf_isom_apple_set_tag(file, itag, NULL, atoi(val) );
				break;
			case GF_ISOM_ITUNE_GENRE:
			{
				u8 _v = id3_get_genre_tag(val);
				if (_v) {
					gf_isom_apple_set_tag(file, itag, NULL, _v);
				} else {
					if (!val) val="";
					gf_isom_apple_set_tag(file, itag, (u8 *) val, (u32) strlen(val) );
				}
			}
			break;
			case GF_ISOM_ITUNE_DISK:
			case GF_ISOM_ITUNE_TRACKNUMBER:
			{
				u32 n, t;
				char _t[8];
				n = t = 0;
				if (val) {
					memset(_t, 0, sizeof(char) * 8);
					tlen = (itag == GF_ISOM_ITUNE_DISK) ? 6 : 8;
					if (sscanf(val, "%u/%u", &n, &t) == 2) {
						_t[3] = n;
						_t[2] = n >> 8;
						_t[5] = t;
						_t[4] = t >> 8;
					}
					else if (sscanf(val, "%u", &n) == 1) {
						_t[3] = n;
						_t[2] = n >> 8;
					}
					else tlen = 0;
				}
				if (!val || tlen) gf_isom_apple_set_tag(file, itag, val ? (u8 *)_t : NULL, tlen);
			}
			break;
			case GF_ISOM_ITUNE_GAPLESS:
			case GF_ISOM_ITUNE_COMPILATION:
			{
				u8 _t[1];
				if (val && !stricmp(val, "yes")) _t[0] = 1;
				else  _t[0] = 0;
				gf_isom_apple_set_tag(file, itag, _t, 1);
			}
			break;
			default:
				gf_isom_apple_set_tag(file, itag, (u8 *)val, tlen);
				break;
			}
			needSave = GF_TRUE;

			if (sep) {
				sep[0] = ':';
				tags = sep+1;
			} else {
				tags = NULL;
			}
		}
	}

	if (movie_time) {
		gf_isom_set_creation_time(file, movie_time);
		for (i=0; i<gf_isom_get_track_count(file); i++) {
			gf_isom_set_track_creation_time(file, i+1, movie_time);
		}
		needSave = GF_TRUE;
	}

	if (cprt) {
		e = gf_isom_set_copyright(file, "und", cprt);
		needSave = GF_TRUE;
		if (e) goto err_exit;
	}
	if (chap_file) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		e = gf_media_import_chapters(file, chap_file, import_fps);
		needSave = GF_TRUE;
#else
		fprintf(stderr, "Warning: GPAC compiled without Media Import, chapters can't be imported\n");
		e = GF_NOT_SUPPORTED;
#endif
		if (e) goto err_exit;
	}

	if (major_brand) {
		gf_isom_set_brand_info(file, major_brand, minor_version);
		needSave = GF_TRUE;
	}
	for (i=0; i<nb_alt_brand_add; i++) {
		gf_isom_modify_alternate_brand(file, brand_add[i], 1);
		needSave = GF_TRUE;
	}
	for (i=0; i<nb_alt_brand_rem; i++) {
		gf_isom_modify_alternate_brand(file, brand_rem[i], 0);
		needSave = GF_TRUE;
	}
	if (box_patch_filename) {
		e = gf_isom_apply_box_patch(file, box_patch_trackID, box_patch_filename);
		if (e) {
			fprintf(stderr, "Failed to apply box patch %s: %s\n", box_patch_filename, gf_error_to_string(e) );
			goto err_exit;
		}
		needSave = GF_TRUE;
	}

#ifndef GPAC_DISABLE_CRYPTO
	if (crypt) {
		if (!drm_file) {
			fprintf(stderr, "Missing DRM file location - usage '-%s drm_file input_file\n", (crypt==1) ? "crypt" : "decrypt");
			e = GF_BAD_PARAM;
			goto err_exit;
		}
		if (get_file_type_by_ext(inName) != GF_FILE_TYPE_ISO_MEDIA) {
			fprintf(stderr, "MP4Box can crypt only ISOMedia File\n");
			e = GF_BAD_PARAM;
			goto err_exit;
		}
		if (crypt == 1) {
			if (use_init_seg) {
				e = gf_crypt_fragment(file, drm_file, outfile, inName, fs_dump_flags);
			} else {
				e = gf_crypt_file(file, drm_file, outfile, interleaving_time, fs_dump_flags);
			}
		} else if (crypt ==2) {
			e = gf_decrypt_file(file, drm_file, outfile, interleaving_time, fs_dump_flags);
		}
		if (e) goto err_exit;
		needSave = outName ? GF_FALSE : GF_TRUE;

		if (!Frag && !HintIt && !FullInter && !force_co64) {
			char szName[GF_MAX_PATH];
			strcpy(szName, gf_isom_get_filename(file) );
			e = GF_OK;
			gf_isom_delete(file);
			file = NULL;
			if (!outName) {
				e = gf_move_file(outfile, szName);
				if (e) goto err_exit;
			}
			goto exit;
		}
	}
#endif /*GPAC_DISABLE_CRYPTO*/

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (Frag) {
		if (!interleaving_time) interleaving_time = DEFAULT_INTERLEAVING_IN_SEC;
		if (HintIt) fprintf(stderr, "Warning: cannot hint and fragment - ignoring hint\n");
		fprintf(stderr, "Fragmenting file (%.3f seconds fragments)\n", interleaving_time);
		e = gf_media_fragment_file(file, outfile, interleaving_time, use_mfra);
		if (e) fprintf(stderr, "Error while fragmenting file: %s\n", gf_error_to_string(e));
		if (!e && !outName) {
			if (gf_file_exists(inName) && gf_delete_file(inName)) fprintf(stderr, "Error removing file %s\n", inName);
			else if (gf_move_file(outfile, inName)) fprintf(stderr, "Error renaming file %s to %s\n", outfile, inName);
		}
		if (e) goto err_exit;
		gf_isom_delete(file);
		goto exit;
	}
#endif

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (HintIt) {
		if (force_ocr) SetupClockReferences(file);
		fprintf(stderr, "Hinting file with Path-MTU %d Bytes\n", MTUSize);
		MTUSize -= 12;
		e = HintFile(file, MTUSize, max_ptime, rtp_rate, hint_flags, HintCopy, HintInter, regular_iod, single_group);
		if (e) goto err_exit;
		needSave = GF_TRUE;
		if (print_sdp) dump_isom_sdp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
	}
#endif

#if !defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)
	for (i=0; i<nb_sdp_ex; i++) {
		if (sdp_lines[i].trackID) {
			u32 track = gf_isom_get_track_by_id(file, sdp_lines[i].trackID);
			if (gf_isom_get_media_type(file, track)!=GF_ISOM_MEDIA_HINT) {
				s32 ref_count;
				u32 j, k, count = gf_isom_get_track_count(file);
				for (j=0; j<count; j++) {
					if (gf_isom_get_media_type(file, j+1)!=GF_ISOM_MEDIA_HINT) continue;
					ref_count = gf_isom_get_reference_count(file, j+1, GF_ISOM_REF_HINT);
					if (ref_count<0) continue;
					for (k=0; k<(u32) ref_count; k++) {
						u32 refTk;
						if (gf_isom_get_reference(file, j+1, GF_ISOM_REF_HINT, k+1, &refTk)) continue;
						if (refTk==track) {
							track = j+1;
							j=count;
							break;
						}
					}
				}
			}
			gf_isom_sdp_add_track_line(file, track, sdp_lines[i].line);
			needSave = GF_TRUE;
		} else {
			gf_isom_sdp_add_line(file, sdp_lines[i].line);
			needSave = GF_TRUE;
		}
	}
#endif /*!defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)*/

	/*full interleave (sample-based) if just hinted*/
	if (FullInter) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_TIGHT);
	} else if (do_flat) {
		e = gf_isom_set_storage_mode(file, (do_flat==1) ? GF_ISOM_STORE_FLAT : GF_ISOM_STORE_STREAMABLE);
		needSave = GF_TRUE;
	} else {
		e = gf_isom_make_interleave(file, interleaving_time);
		if (!e && old_interleave) e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_INTERLEAVED);
	}
	if (force_co64)
		gf_isom_force_64bit_chunk_offset(file, GF_TRUE);

	if (e) goto err_exit;

	if (!encode && !force_new) gf_isom_set_final_name(file, outfile);
	if (needSave) {
		if (outName) {
			fprintf(stderr, "Saving to %s: ", outfile);
			gf_isom_set_final_name(file, outfile);
		} else if (encode || pack_file) {
			fprintf(stderr, "Saving to %s: ", gf_isom_get_filename(file) );
		} else {
			fprintf(stderr, "Saving %s: ", inName);
		}
		if (HintIt && FullInter) fprintf(stderr, "Hinted file - Full Interleaving\n");
		else if (FullInter) fprintf(stderr, "Full Interleaving\n");
		else if (do_flat || !interleaving_time) fprintf(stderr, "Flat storage\n");
		else fprintf(stderr, "%.3f secs Interleaving%s\n", interleaving_time, old_interleave ? " - no drift control" : "");

		e = gf_isom_close(file);
		file = NULL;

		if (!e && !outName && !encode && !force_new && !pack_file) {
			e = gf_delete_file(inName);
			if (e) {
				fprintf(stderr, "Error removing file %s\n", inName);
			} else {
				e = gf_move_file(outfile, inName);
				if (e) {
					fprintf(stderr, "Error renaming file %s to %s\n", outfile, inName);
				}
			}
		}
	} else {
		gf_isom_delete(file);
	}

	if (e) {
		fprintf(stderr, "Error: %s\n", gf_error_to_string(e));
		goto err_exit;
	}
	goto exit;

#else
	/*close libgpac*/
	gf_isom_delete(file);
	fprintf(stderr, "Error: Read-only version of MP4Box.\n");
	return mp4box_cleanup(1);
#endif

err_exit:
	/*close libgpac*/
	if (file) gf_isom_delete(file);
	fprintf(stderr, "\n\tError: %s\n", gf_error_to_string(e));
	return mp4box_cleanup(1);

exit:
	mp4box_cleanup(0);

#ifdef GPAC_MEMORY_TRACKING
	if (mem_track && (gf_memory_size() || gf_file_handles_count() )) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif
	return 0;
}


GF_MAIN_FUNC(mp4boxMain)


#endif /*GPAC_DISABLE_ISOM*/
