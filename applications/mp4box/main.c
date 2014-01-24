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


#include <gpac/download.h>
#include <gpac/network.h>

#ifndef GPAC_DISABLE_SMGR
#include <gpac/scene_manager.h>
#endif

#ifdef GPAC_DISABLE_ISOM

#error "Cannot compile MP4Box if GPAC is not built with ISO File Format support"

#else

#if defined(WIN32) && !defined(_WIN32_WCE)
#include <io.h>
#include <fcntl.h>
#endif

#include <gpac/media_tools.h>

/*RTP packetizer flags*/
#ifndef GPAC_DISABLE_STREAMING
#include <gpac/ietf.h>
#endif

#ifndef GPAC_DISABLE_MCRYPT
#include <gpac/ismacryp.h>
#endif

#include <gpac/constants.h>

#include <gpac/internal/mpd.h>

#include <time.h>

#define BUFFSIZE	8192

/*in fileimport.c*/

#ifndef GPAC_DISABLE_MEDIA_IMPORT
void convert_file_info(char *inName, u32 trackID);
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, Double force_fps, u32 frames_per_sample);
GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u32 split_size_kb, char *inName, Double interleaving_time, Double chunk_start, Bool adjust_split_end, char *outName, const char *tmpdir);
GF_Err cat_isomedia_file(GF_ISOFile *mp4, char *fileName, u32 import_flags, Double force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat, Bool align_timelines, Bool allow_add_in_command);

#if !defined(GPAC_DISABLE_SCENE_ENCODER)
GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs);
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext, const char *tmpdir);
#endif

GF_ISOFile *package_file(char *file_name, char *fcc, const char *tmpdir, Bool make_wgt);

#endif

GF_Err dump_cover_art(GF_ISOFile *file, char *inName);
GF_Err dump_chapters(GF_ISOFile *file, char *inName, Bool dump_ogg);
u32 id3_get_genre_tag(const char *name);

/*in filedump.c*/
#ifndef GPAC_DISABLE_SCENE_DUMP
GF_Err dump_file_text(char *file, char *inName, u32 dump_mode, Bool do_log);
#endif
#ifndef GPAC_DISABLE_SCENE_STATS
void dump_scene_stats(char *file, char *inName, u32 stat_level);
#endif
void PrintNode(const char *name, u32 graph_type);
void PrintBuiltInNodes(u32 graph_type);

#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_isom_xml(GF_ISOFile *file, char *inName);
#endif


#ifndef GPAC_DISABLE_ISOM_HINTING
#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_file_rtp(GF_ISOFile *file, char *inName);
#endif
void DumpSDP(GF_ISOFile *file, char *inName);
#endif

void dump_file_timestamps(GF_ISOFile *file, char *inName);
void dump_file_nal(GF_ISOFile *file, u32 trackID, char *inName);

#ifndef GPAC_DISABLE_ISOM_DUMP
void dump_file_ismacryp(GF_ISOFile *file, char *inName);
void dump_timed_text_track(GF_ISOFile *file, u32 trackID, char *inName, Bool is_convert, u32 dump_type);
#endif /*GPAC_DISABLE_ISOM_DUMP*/


void DumpTrackInfo(GF_ISOFile *file, u32 trackID, Bool full_dump);
void DumpMovieInfo(GF_ISOFile *file);
void PrintLanguages();
const char *GetLanguageCode(char *lang);

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

int mp4boxTerminal(int argc, char **argv);

u32 quiet = 0;
Bool dvbhdemux =0;
Bool keep_sys_tracks = 0;


/*some global vars for swf import :(*/
u32 swf_flags = 0;
Float swf_flatten_angle = 0;
s32 laser_resolution = 0;


typedef struct { u32 code; const char *name; const char *comment; } itunes_tag;
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
};

u32 nb_itunes_tags = sizeof(itags) / sizeof(itunes_tag);


void PrintVersion()
{
	fprintf(stderr, "MP4Box - GPAC version " GPAC_FULL_VERSION "\n"
		"GPAC Copyright (c) Telecom ParisTech 2000-2012\n"
		"GPAC Configuration: " GPAC_CONFIGURATION "\n"
		"Features: %s\n", gpac_features());
}

void PrintGeneralUsage()
{
	fprintf(stderr, "General Options:\n"
#ifdef GPAC_MEMORY_TRACKING
			" -mem-track:  enables memory tracker\n"
#endif
			" -strict-error        exits after the first error is reported\n"
			" -inter time_in_ms    interleaves file data (track chunks of time_in_ms)\n"
			"                       * Note 1: Interleaving is 0.5s by default\n"
			"                       * Note 2: Performs drift checking accross tracks\n"
			"                       * Note 3: a value of 0 disables interleaving\n"
			" -old-inter time      same as -inter but doesn't perform drift checking\n"
			" -tight               performs tight interleaving (sample based) of the file\n"
			"                       * Note: reduces disk seek but increases file size\n"
			" -flat                stores file with all media data first, non-interleaved\n"
			" -frag time_in_ms     fragments file (track fragments of time_in_ms)\n"
			"                       * Note: Always disables interleaving\n"
			" -ffspace size        inserts free space before moof in fragmented files\n"
			" -out filename        specifies output file name\n"
			"                       * Note: By default input (MP4,3GP) file is overwritten\n"
			" -tmp dirname         specifies directory for temporary file creation\n"
			"                       * Note: Default temp dir is OS-dependent\n"
			" -write-buffer SIZE   specifies write buffer in bytes for ISOBMF files\n"
			" -no-sys              removes all MPEG-4 Systems info except IOD (profiles)\n"
			"                       * Note: Set by default whith '-add' and '-cat'\n"
			" -no-iod              removes InitialObjectDescriptor from file\n"
			" -isma                rewrites the file as an ISMA 1.0 AV file\n"
			" -ismax               same as \'-isma\' and removes all clock references\n"
			" -3gp                 rewrites as 3GPP(2) file (no more MPEG-4 Systems Info)\n"
			"                       * Note 1: some tracks may be removed in the process\n"
			"                       * Note 2: always on for *.3gp *.3g2 *.3gpp\n"
			" -ipod                rewrites the file for iPod\n"
			" -psp                 rewrites the file for PSP devices\n" 
			" -brand ABCD[:v]      sets major brand of file, with optional version\n"
			" -ab ABCD             adds given brand to file's alternate brand list\n"
			" -rb ABCD             removes given brand from file's alternate brand list\n"
			" -cprt string         adds copyright string to movie\n"
			" -chap file           adds chapter information contained in file\n"
			" -rem trackID         removes track from file\n"
			" -enable trackID      enables track\n"
			" -disable trackID     disables track\n"
			" -new                 forces creation of a new destination file\n"
			" -lang [tkID=]LAN     sets track language. LAN is the ISO 639-2 code (eng, und)\n"
			" -delay tkID=TIME     sets track start delay in ms.\n"
			" -par tkID=PAR        sets visual track pixel aspect ratio (PAR=N:D or \"none\")\n"
			" -name tkID=NAME      sets track handler name\n"
			"                       * NAME can indicate a UTF-8 file (\"file://file name\"\n"
			" -itags tag1[:tag2]   sets iTunes tags to file - more info: MP4Box -tag-list.\n"
			" -split time_sec      splits in files of time_sec max duration\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -split-size size     splits in files of max filesize kB. same as -splits.\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -split-rap           splits in files begining at each RAP. same as -splitr.\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -split-chunk S:E     extracts a new file from Start to End (in seconds). same as -splitx\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -splitz S:E          same as -split-chunk, but adjust the end time to be before the last RAP sample\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -group-add fmt       creates a new grouping information in the file. Format is\n"
			"                      a colon-separated list of following options:\n"
			"                      refTrack=ID: ID of the track used as a group reference.\n"
			"                       If not set, the track will belong to the same group as the previous trackID specified.\n"
			"                       If 0 or no previous track specified, a new alternate group will be created\n"
			"                      switchID=ID: ID of the switch group to create.\n"
			"                       If 0, a new ID will be computed for you\n"
			"                       If <0, disables SwitchGroup\n"
			"                      criteria=string: list of space-separated 4CCs.\n"
			"                      trackID=ID: ID of the track to add to this group.\n"
			"\n"
			"                       *WARNING* Options modify state as they are parsed:\n"
			"                        trackID=1:criteria=lang:trackID=2\n"
			"                       is different from:\n"
			"                        criteria=lang:trackID=1:trackID=2\n"
			"\n"
			" -group-rem-track ID  removes track from its group\n"
			" -group-rem ID        removes the track's group\n"
			" -group-clean         removes all group information from all tracks\n"
 	        " -group-single        puts all tracks in a single group\n"
			" -ref id:XXXX:refID   adds a reference of type 4CC from track ID to track refID\n"
			" -keep-utc            keeps UTC timing in the file after edit\n"
			"\n");
}

void PrintDASHUsage()
{
	fprintf(stderr, "DASH Options:\n"
			" -dash dur            enables DASH-ing of the file(s) with a segment duration of DUR ms\n"
			"                       Note: the duration of a fragment (subsegment) is set\n"
			"						using the -frag switch.\n"
			"                       Note: for onDemand profile, sets duration of a subsegment\n"
			" -frag time_in_ms     Specifies a fragment duration of time_in_ms.\n"
			"                       * Note: By default, this is the DASH duration\n"
			" -out filename        specifies output MPD file name.\n"
			" -tmp dirname         specifies directory for temporary file creation\n"
			"                       * Note: Default temp dir is OS-dependent\n"
			" -profile NAME   specifies the target DASH profile: \"onDemand\", \"live\", \"main\", \"simple\", \"full\"\n"
			"                       * This will set default option values to ensure conformance to the desired profile\n"
			"                       * Default profile is \"full\" in static mode, \"live\" in dynamic mode\n"
			"\n"
			"Input media files to dash can use the following modifiers\n"
			" \"#trackID=N\"       only uses the track ID N from the source file\n"
			" \"#video\"           only uses the first video track from the source file\n"
			" \"#audio\"           only uses the first video track from the source file\n"
			" \":id=NAME\"         sets the representation ID to NAME\n"
			" \":period=NAME\"     sets the representation's period to NAME. Multiple periods may be used\n"
			"                       period appear in the MPD in the same order as specified with this option\n"
			" \":bandwidth=VALUE\" sets the representation's bandwidth to a given value\n"
			" \":role=VALUE\"      sets the role of this representation (cf DASH spec).\n"
			"                       media with different roles belong to different adaptation sets.\n"
			"\n"
			" -rap                 segments begin with random access points\n"
			"                       Note: segment duration may not be exactly what asked by\n"
			"                       \"-dash\" since encoded video data is not modified\n"
			" -frag-rap            All fragments begin with random access points\n"
			"                       Note: fragment duration may not be exactly what is asked by\n"
			"                       \"-frag\" since encoded video data is not modified\n"
			" -segment-name name   sets the segment name for generated segments\n"
			"                       If not set (default), segments are concatenated in output file\n"
			"                        except in \"live\" profile where dash_%%s is used\n"
			" -segment-ext name    sets the segment extension. Default is m4s, \"null\" means no extension\n"
			" -segment-timeline    Uses SegmentTimeline when generating segments. NOT SUPPORTED BY LIVE/CTX MODE YET.\n"
			" -segment-marker MARK Adds a box of type \'MARK\' at the end of each DASH segment. MARK shall be a 4CC identifier\n"
			" -base-url string     sets Base url at MPD level. Can be used several times.\n"
			" -mpd-title string    sets MPD title.\n"
			" -mpd-source string   sets MPD source.\n"
			" -mpd-info-url string sets MPD info url.\n"
			" -cprt string         adds copyright string to MPD\n"
			" -dash-live[=F] dur   generates a live DASH session using dur segment duration, optionnally writing live context to F\n" 
			"                       MP4Box will run the live session until \'q\' is pressed or a fatal error occurs.\n"
			" -ddbg-live[=F] dur   same as -dash-live without time regulation for debug purposes.\n"
			" -dash-ctx FILE       stores/restore DASH timing from FILE.\n"
			" -dynamic             uses dynamic MPD type instead of static.\n"
			" -mpd-refresh TIME    specifies MPD update time in seconds.\n"
			" -time-shift  TIME    specifies MPD time shift buffer depth in seconds (default 0). Specify -1 to keep all files\n"
			" -subdur DUR          specifies maximum duration in ms of the input file to be dashed in LIVE or context mode.\n"
			"                       NOTE: This does not change the segment duration: dashing stops once segments produced exceeded the duration.\n"
			" -min-buffer TIME     specifies MPD min buffer time in milliseconds\n"
			" -ast-offset TIME     specifies MPD AvailabilityStartTime offset in seconds. Default is 1 sec delay\n"
			" -dash-scale SCALE    specifies that timing for -dash and -frag are expressed in SCALE units per seconds\n"
			" -mem-frags           fragments will be produced in memory rather than on disk before flushing to disk\n"
			" -pssh-moof           stores PSSH boxes in first moof of each segments. By default PSSH are stored in movie box.\n"
			
			"\n"
			"Advanced Options, should not be needed when using -dash-profile:\n"
			" -subsegs-per-sidx N  sets the number of subsegments to be written in each SIDX box\n"
			"                       If 0, a single SIDX box is used per segment\n"
			"                       If -1, no SIDX box is used\n"
			" -url-template        uses SegmentTemplate instead of explicit sources in segments.\n"
			"                       Ignored if segments are stored in the output file.\n"
			" -daisy-chain         uses daisy-chain SIDX instead of hierarchical. Ignored if frags/sidx is 0.\n"
			" -single-segment      uses a single segment for the whole file (OnDemand profile). \n"
			" -single-file         uses a single file for the whole file (default). \n"
			" -bs-switching MODE   sets bitstream switching to \"inband\" (default), \"merge\", \"no\" or \"single\" to test with single input.\n" 
			" -moof-sn N           sets sequence number of first moof to N\n"
			" -tfdt N              sets TFDT of first traf to N in SCALE units (cf -dash-scale)\n"
			" -no-frags-default    disables default flags in fragments\n"
			" -dash-ts-prog N      program_number to be considered in case of an MPTS input file.\n"
			"\n");
}

void PrintFormats()
{
	fprintf(stderr, "Suppported raw formats and file extensions:\n"
			" NHNT                 .media .nhnt .info\n"
			" NHML                 .nhml (opt: .media .info)\n"
			" MPEG-1-2 Video       .m1v .m2v\n"
			" MPEG-4 Video         .cmp .m4v\n"
			" H263 Video           .263 .h263\n"
			" AVC/H264 Video       .h264 .h26L .264 .26L\n"
			" JPEG Images          .jpg .jpeg\n"
			" PNG Images           .png\n"
			" MPEG 1-2 Audio       .mp3, .m1a, .m2a\n"
			" ADTS-AAC Audio       .aac\n"
			" AMR(WB) Audio        .amr .awb\n"
			" EVRC Audio           .evc\n"
			" SMV Audio            .smv\n"
			"\n"
			"Supported containers and file extensions:\n"
			" AVI                  .avi\n"
			" MPEG-2 PS            .mpg .mpeg .vob .vcd .svcd\n"
			" MPEG-2 TS            .ts .m2t\n"
			" QCP                  .qcp\n"
			" OGG                  .ogg\n"
			" ISO-Media files      no extension checking\n"
			"\n"
			"Supported text formats:\n"
			" SRT Subtitles        .srt\n"
			" SUB Subtitles        .sub\n"
			" GPAC Timed Text      .ttxt\n"
			" QuickTime TeXML Text .xml  (cf QT documentation)\n"
			"\n"
			"Supported Scene formats:\n"
			" MPEG-4 XMT-A         .xmt .xmta .xmt.gz .xmta.gz\n"
			" MPEG-4 BT            .bt .bt.gz\n"
			" VRML                 .wrl .wrl.gz\n"
			" X3D-XML              .x3d .x3d.gz\n"
			" X3D-VRML             .x3dv .x3dv.gz\n"
			" MacroMedia Flash     .swf (very limited import support only)\n"
			"\n"
		);
}

void PrintImportUsage()
{
	fprintf(stderr, "Importing Options\n"
			"\nFile importing syntax:\n"
			" \"#video\" \"#audio\"  base import for most AV files\n"
			" \"#trackID=ID\"        track import for IsoMedia and other files\n"
			" \"#pid=ID\"            stream import from MPEG-2 TS\n"
			" \":dur=D\"             imports only the first D seconds\n"
			" \":lang=LAN\"          sets imported media language code\n"
			" \":delay=delay_ms\"    sets imported media initial delay in ms\n"
			" \":par=PAR\"           sets visual pixel aspect ratio (PAR=Num:Den)\n"
			" \":name=NAME\"         sets track handler name\n"
			" \":ext=EXT\"           overrides file extension when importing\n"
			" \":hdlr=code\"         sets track handler type to the given code point (4CC)\n"
			" \":disable\"           imported track(s) will be disabled\n"
			" \":group=G\"           adds the track as part of the G alternate group.\n"
			"                         If G is 0, the first available GroupID will be picked.\n"
			" \":fps=VAL\"           same as -fps option\n"
			" \":agg=VAL\"           same as -agg option\n"
			" \":par=VAL\"           same as -par option\n"
			" \":dref\"              same as -dref option\n"
			" \":nodrop\"            same as -nodrop option\n"
			" \":packed\"            same as -packed option\n"
			" \":sbr\"               same as -sbr option\n"
			" \":sbrx\"              same as -sbrx option\n"
			" \":ps\"                same as -ps option\n"
			" \":psx\"               same as -psx option\n"
			" \":ovsbr\"             same as -ovsbr option\n" 
			" \":mpeg4\"             same as -mpeg4 option\n"
			" \":svc\"               import SVC with explicit signaling (no AVC base compatibility)\n"
			" \":nosvc\"             discard SVC data when importing\n"
			" \":svcmode=MODE\"      sets SVC import mode:\n"
			" \"                       split : each AVC/SVC layer is in its own track\n"
			" \"                       merge : all AVC/SVC layers are merged in a single track\n"
			" \"                       splitbase : all SVC layers are merged in a track, and the AVC base in another\n"
			" \":subsamples\"        adds SubSample information for AVC+SVC\n"
			" \":forcesync\"         forces non IDR samples with I slices to be marked as sync points (AVC GDR)\n"
			"       !! RESULTING FILE IS NOT COMPLIANT WITH THE SPEC but will fix seeking in most players\n"
			" \":font=name\"         specifies font name for text import (default \"Serif\")\n"
			" \":size=s\"            specifies font size for text import (default 18)\n"
			" \":stype=4CC\"         forces the sample description type to a different value\n"
			"                         !! THIS MAY BREAK THE FILE WRITING !!\n"
			" \":chap\"              specifies the track is a chapter track\n"
			" \":chapter=NAME\"      adds a single chapter (old nero format) with given name lasting the entire file\n"
			"                         This command can be used in -cat as well\n"
			" \":chapfile=file\"     adds a chapter file (old nero format)\n"
			"                         This command can be used in -cat as well\n"
			" \":layout=WxHxXxY\"    specifies the track layout\n"
			"                         - if W (resp H) = 0, the max width (resp height) of\n"
			"                         the tracks in the file are used.\n"
			"                         - if Y=-1, the layout is moved to the bottom of the\n"
			"                         track area\n"
			"                         - X and Y can be omitted (:layout=WxH)\n"
			" \":rescale=TS\"        forces media timescale to TS !! changes the media duration\n"
			" \":timescale=TS\"      sets import timescale to TS\n"
			" \":swf-global\"        all SWF defines are placed in first scene replace\n"
			"                         * Note: By default SWF defines are sent when needed\n"
			" \":swf-no-ctrl\"       uses a single stream for movie control and dictionary\n"
			"                         * Note: this will disable ActionScript\n"
			" \":swf-no-text\"       removes all SWF text\n"
			" \":swf-no-font\"       removes all embedded SWF Fonts (terminal fonts used)\n"
			" \":swf-no-line\"       removes all lines from SWF shapes\n"
			" \":swf-no-grad\"       removes all gradients from swf shapes\n"
			" \":swf-quad\"          uses quadratic bezier curves instead of cubic ones\n"
			" \":swf-xlp\"           support for lines transparency and scalability\n"
			" \":swf-flatten=ang\"   complementary angle below which 2 lines are merged\n"
			"                         * Note: angle \'0\' means no flattening\n"
			"\n"
			" \":negctts\"           uses negative CTS-DTS offsets (ISO4 brand)\n"
			" -add file              add file tracks to (new) output file\n"
			" -cat file              concatenates file samples to (new) output file\n"
			"                         * Note: creates tracks if needed\n"
			"                         * Note: aligns initial timestamp of the file to be concatenated.\n"
			" -catx file             same as cat but new tracks can be imported before concatenation by specifying '+ADD_COMMAND'\n"
			"                        where ADD_COMMAND is a regular -add syntax\n"
			" -unalign-cat           does not attempt to align timestamps of samples inbetween tracks\n"
			" -force-cat             skips media configuration check when concatenating file\n"
			"                         !!! THIS MAY BREAK THE CONCATENATED TRACK(S) !!!\n"
			" -keep-sys              keeps all MPEG-4 Systems info when using '-add' / 'cat'\n"
			" -keep-all              keeps all existing tracks when using '-add'\n"
			"                         * Note: only used when adding IsoMedia files\n"
			"\n"
			"All the following options can be specified as default or for each track.\n"
			"When specified by track the syntax is \":opt\" or \":opt=val\".\n\n"
			" -dref                  keeps media data in original file\n"
			" -no-drop               forces constant FPS when importing AVI video\n"
			" -packed                forces packed bitstream when importing raw ASP\n"
			" -sbr                   backward compatible signaling of AAC-SBR\n"
			" -sbrx                  non-backward compatible signaling of AAC-SBR\n"
			" -ps                    backward compatible signaling of AAC-PS\n"
			" -psx                   non-backward compatible signaling of AAC-PS\n"
			" -ovsbr                 oversample SBR\n"
			"                         * Note: SBR AAC, PS AAC and oversampled SBR cannot be detected at import time\n"
			" -fps FPS               forces frame rate for video and SUB subtitles import\n"
			"                         FPS is either a number or expressed as timescale-increment\n"
			"                         * For raw H263 import, default FPS is 15\n"
			"                         * For all other imports, default FPS is 25\n"
			"                         !! THIS IS IGNORED FOR IsoMedia IMPORT !!\n"
			" -mpeg4                 forces MPEG-4 sample descriptions when possible (3GPP2)\n"
			"                         For AAC, forces MPEG-4 AAC signaling even if MPEG-2\n"
			" -agg N                 aggregates N audio frames in 1 sample (3GP media only)\n"
			"                         * Note: Maximum value is 15 - Disabled by default\n"
			"\n"
			);
}

void PrintEncodeUsage()
{
	fprintf(stderr, "MPEG-4 Scene Encoding Options\n"
			" -mp4                 specify input file is for encoding.\n"
			" -def                 encode DEF names\n"
			" -sync time_in_ms     forces BIFS sync sample generation every time_in_ms\n"
			"                       * Note: cannot be used with -shadow\n"
			" -shadow time_ms      forces BIFS sync shadow sample generation every time_ms.\n"
			"                       * Note: cannot be used with -sync\n"
			" -log                 generates scene codec log file if available\n"
			" -ms file             specifies file for track importing\n"
			"\nChunk Processing\n"
			" -ctx-in file         specifies initial context (MP4/BT/XMT)\n"
			"                       * Note: input file must be a commands-only file\n"
			" -ctx-out file        specifies storage of updated context (MP4/BT/XMT)\n"
			"\n"
			"LASeR Encoding options\n"
			" -resolution res      resolution factor (-8 to 7, default 0)\n"
			"                       all coords are multiplied by 2^res before truncation\n"
			" -coord-bits bits     bits used for encoding truncated coordinates\n"
			"                       (0 to 31, default 12)\n"
			" -scale-bits bits     extra bits used for encoding truncated scales\n"
			"                       (0 to 4, default 0)\n"
			" -auto-quant res      resolution is given as if using -resolution\n"
			"                       but coord-bits and scale-bits are infered\n"
			);
}

void PrintEncryptUsage()
{
	fprintf(stderr, "ISMA Encryption/Decryption Options\n"
			" -crypt drm_file      crypts a specific track using ISMA AES CTR 128\n"
			" -decrypt [drm_file]  decrypts a specific track using ISMA AES CTR 128\n"
			"                       * Note: drm_file can be omitted if keys are in file\n"
			" -set-kms kms_uri     changes KMS location for all tracks or a given one.\n"
			"                       * to adress a track, use \'tkID=kms_uri\'\n"
			"\n"
			"DRM file syntax for GPAC ISMACryp:\n"
			"                      File is XML and shall start with xml header\n"
			"                      File root is an \"ISMACryp\" element\n"
			"                      File is a list of \"cryptrack\" elements\n"
			"\n"
			"cryptrack attributes are\n"
			" TrackID              ID of track to en/decrypt\n"
			" key                  AES-128 key formatted (hex string \'0x\'+32 chars)\n"
			" salt                 CTR IV salt key (64 bits) (hex string \'0x\'+16 chars)\n"
			"\nEncryption only attributes:\n"
			" Scheme_URI           URI of scheme used\n"
			" KMS_URI              URI of key management system\n"
			"                       * Note: \'self\' writes key and salt in the file\n"
			" selectiveType        selective encryption type - understood values are:\n"
			"   \"None\"             all samples encrypted (default)\n"
			"   \"RAP\"              only encrypts random access units\n"
			"   \"Non-RAP\"          only encrypts non-random access units\n"
			"   \"Rand\"             random selection is performed\n"
			"   \"X\"                Encrypts every first sample out of X (uint)\n"
			"   \"RandX\"            Encrypts one random sample out of X (uint)\n"
			"\n"
			" ipmpType             IPMP Signaling Type: None, IPMP, IPMPX\n"
			" ipmpDescriptorID     IPMP_Descriptor ID to use if IPMP(X) is used\n"
			"                       * If not set MP4Box will generate one for you\n"
			"\n"
		);
}

void PrintHintUsage()
{
	fprintf(stderr, "Hinting Options\n"
			" -hint                hints the file for RTP/RTSP\n"
			" -mtu size            specifies RTP MTU (max size) in bytes. Default size is 1450\n"
			"                       * Note: this includes the RTP header (12 bytes)\n"
			" -copy                copies media data to hint track rather than reference\n"
			"                       * Note: speeds up server but takes much more space\n"
			" -multi [maxptime]    enables frame concatenation in RTP packets if possible\n"
			"        maxptime       max packet duration in ms (optional, default 100ms)\n"
			" -rate ck_rate        specifies rtp rate in Hz when no default for payload\n"
			"                       * Note: default value is 90000 (MPEG rtp rates)\n"
			" -mpeg4               forces MPEG-4 generic payload whenever possible\n"
			" -latm                forces MPG4-LATM transport for AAC streams\n"
			" -static              enables static RTP payload IDs whenever possible\n"
			"                       * By default, dynamic payloads are always used\n"
			"\n"
			"MPEG-4 Generic Payload Options\n"
			" -ocr                 forces all streams to be synchronized\n"
			"                       * Most RTSP servers only support synchronized streams\n"
			" -rap                 signals random access points in RTP packets\n"
			" -ts                  signals AU Time Stamps in RTP packets\n"
			" -size                signals AU size in RTP packets\n"
			" -idx                 signals AU sequence numbers in RTP packets\n"
			" -iod                 prevents systems tracks embedding in IOD\n"
			"                       * Note: shouldn't be used with -isma option\n"
			"\n"
			" -add-sdp string      adds sdp string to (hint) track (\"-add-sdp tkID:string\")\n"
			"                       or movie. This will take care of SDP lines ordering\n"
			" -unhint              removes all hinting information.\n"
			"\n");
}
void PrintExtractUsage()
{
	fprintf(stderr, "Extracting Options:\n"
			" -raw TrackID         extracts track in raw format when supported\n" 
			"                      :output=FileName sets the output filename for this extraction \n" 
			" -raws TrackID        extract each track sample to a file\n" 
			"                       * Note: \"TrackID:N\" extracts Nth sample\n"
			" -nhnt TrackID        extracts track in nhnt format\n" 
			" -nhml TrackID        extracts track in nhml format (XML nhnt).\n" 
			"                       * Note: \"-nhml TrackID:full\" for full dump\n"
			" -webvtt-raw TrackID  extracts raw media track in WebVTT as metadata.\n" 
			"                       * Note: \"-webvtt-raw TrackID:embedded\" to include media data in the WebVTT file\n"
			" -six TrackID		   extracts raw media track in experimental XML streaming instructions.\n" 
			" -single TrackID      extracts track to a new mp4 file\n"
			" -avi TrackID         extracts visual track to an avi file\n"
			" -qcp TrackID         same as \'-raw\' but defaults to QCP file for EVRC/SMV\n" 
			" -aviraw TK           extracts AVI track in raw format\n"
			"			            $TK can be one of \"video\" \"audio\" \"audioN\"\n" 
			" -saf                 remux file to SAF multiplex\n"
			" -dvbhdemux           demux DVB-H file into IP Datagrams\n"
			"                       * Note: can be used when encoding scene descriptions\n"
			" -raw-layer ID        same as -raw but skips SVC/MVC extractors when extracting\n" 
			" -diod                extracts file IOD in raw format when supported\n" 
			"\n"
#if !defined(GPAC_DISABLE_STREAMING)
			" -grab-ts IP:port     grabs TS over UDP or RTP at IP:port location to output TS file\n"
#endif
			"\n");
}
void PrintDumpUsage()
{
	fprintf(stderr, "Dumping Options\n"
			" -stdb                dumps/write to stdout and assumes stdout is opened in binary mode\n"
			" -std                 dumps/write to stdout and try to reopen stdout in binary mode.\n"
			" -info [trackID]      prints movie info / track info if trackID specified\n"
			"                       * Note: for non IsoMedia files, gets import options\n" 
			" -bt                  scene to bt format - removes unknown MPEG4 nodes\n" 
			" -xmt                 scene to XMT-A format - removes unknown MPEG4 nodes\n" 
			" -wrl                 scene VRML format - removes unknown VRML nodes\n" 
			" -x3d                 scene to X3D/XML format - removes unknown X3D nodes\n" 
			" -x3dv                scene to X3D/VRML format - removes unknown X3D nodes\n" 
			" -lsr                 scene to LASeR format\n" 
			" -diso                scene IsoMedia file boxes in XML output\n"
			" -drtp                rtp hint samples structure to XML output\n"
			" -dts                 prints sample timing to text output\n"
			" -dnal trackID        prints NAL sample info of given track\n"
			" -sdp                 dumps SDP description of hinted file\n"
			" -dcr                 ISMACryp samples structure to XML output\n"
			" -dump-cover          Extracts cover art\n"
			" -dump-chap           Extracts chapter file\n"
			"\n"
#ifndef GPAC_DISABLE_ISOM_WRITE
			" -ttxt                Converts input subtitle to GPAC TTXT format\n"
#endif
			" -ttxt TrackID        Dumps Text track to GPAC TTXT format\n"
#ifndef GPAC_DISABLE_ISOM_WRITE
			" -srt                 Converts input subtitle to SRT format\n"
#endif
			" -srt TrackID         Dumps Text track to SRT format\n"
			"\n"
			" -stat                generates node/field statistics for scene\n"
			" -stats               generates node/field statistics per MPEG-4 Access Unit\n"
			" -statx               generates node/field statistics for scene after each AU\n"
			"\n"
			" -hash                generates SHA-1 Hash of the input file\n"
			"\n");
}

void PrintMetaUsage()
{
	fprintf(stderr, "Meta handling Options\n"
			" -set-meta args       sets given meta type - syntax: \"ABCD[:tk=ID]\"\n"
			"                       * ABCD: four char meta type (NULL or 0 to remove meta)\n"
			"                       * [:tk=ID]: if not set use root (file) meta\n"
			"                                if ID is 0 use moov meta\n"
			"                                if ID is not 0 use track meta\n"
			" -add-item args       adds resource to meta\n"
			"                       * syntax: file_path + options (\':\' separated):\n"
			"                        tk=ID: meta adressing (file, moov, track)\n"
			"                        name=str: item name\n"
			"                        mime=mtype: item mime type\n"
			"                        encoding=enctype: item content-encoding type\n"
			"                        id=id: item ID\n"
			"                       * file_path \"this\" or \"self\": item is the file itself\n"
			" -rem-item args       removes resource from meta - syntax: item_ID[:tk=ID]\n"
			" -set-primary args    sets item as primary for meta - syntax: item_ID[:tk=ID]\n"
			" -set-xml args        sets meta XML data\n"
			"                       * syntax: xml_file_path[:tk=ID][:binary]\n"
			" -rem-xml [tk=ID]     removes meta XML data\n"
			" -dump-xml args       dumps meta XML to file - syntax file_path[:tk=ID]\n"
			" -dump-item args      dumps item to file - syntax item_ID[:tk=ID][:path=fileName]\n"
			" -package             packages input XML file into an ISO container\n"
			"                       * all media referenced except hyperlinks are added to file\n"
			" -mgt                 packages input XML file into an MPEG-U widget with ISO container.\n"
			"                       * all files contained in the current folder are added to the widget package\n"
			"\n");
}

void PrintSWFUsage()
{
	fprintf(stderr, 
			"SWF Importer Options\n"
			"\n"
			"MP4Box can import simple Macromedia Flash files (\".SWF\")\n"
			"You can specify a SWF input file with \'-bt\', \'-xmt\' and \'-mp4\' options\n"
			"\n"
			" -global              all SWF defines are placed in first scene replace\n"
			"                       * Note: By default SWF defines are sent when needed\n"
			" -no-ctrl             uses a single stream for movie control and dictionary\n"
			"                       * Note: this will disable ActionScript\n"
			" -no-text             removes all SWF text\n"
			" -no-font             removes all embedded SWF Fonts (terminal fonts used)\n"
			" -no-line             removes all lines from SWF shapes\n"
			" -no-grad             removes all gradients from swf shapes\n"
			" -quad                uses quadratic bezier curves instead of cubic ones\n"
			" -xlp                 support for lines transparency and scalability\n"
			" -flatten ang         complementary angle below which 2 lines are merged\n"
			"                       * Note: angle \'0\' means no flattening\n"
			"\n"
		);
}

void PrintUsage()
{
	fprintf (stderr, "MP4Box [option] input [option]\n"
			" -h general           general options help\n"
			" -h hint              hinting options help\n"
			" -h dash              DASH segmenter help\n"
			" -h import            import options help\n"
			" -h encode            encode options help\n"
			" -h meta              meta handling options help\n"
			" -h extract           extraction options help\n"
			" -h dump              dump options help\n"
			" -h swf               Flash (SWF) options help\n"
			" -h crypt             ISMA E&A options help\n"
			" -h format            supported formats help\n"
			" -h rtp               file streamer help\n"
			" -h live              BIFS streamer help\n"
			" -h all               all options are printed\n"
			"\n"
			" -nodes               lists supported MPEG4 nodes\n"
			" -node NodeName       gets MPEG4 node syntax and QP info\n"
			" -xnodes              lists supported X3D nodes\n"
			" -xnode NodeName      gets X3D node syntax\n"
			" -snodes              lists supported SVG nodes\n"
			" -snode NodeName      gets SVG node syntax\n"
			" -languages           lists supported ISO 639 languages\n"
			"\n"
			" -quiet                quiet mode\n"
			" -noprog               disables progress\n"
			" -v                   verbose mode\n"
			" -logs                set log tools and levels, formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"
			" -version             gets build version\n"
			" -- INPUT             escape option if INPUT starts with - character\n"
			"\n"
			);
}


void scene_coding_log(void *cbk, u32 log_level, u32 log_tool, const char *fmt, va_list vlist)
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
	u32 iod_mode, mtype;
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
			if (spec_type==GF_4CC('I','S','M','A')) continue;
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
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			if (dsi.VideoPL > PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, dsi.VideoPL);
		} else if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SVC)) {
			gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0x15);
		} else if (!PL) {
			gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0xFE);
		}
		break;
	case 0x05:
		PL = gf_isom_get_pl_indication(file, GF_ISOM_PL_AUDIO);
		switch (esd->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_AUDIO_AAC_MPEG2_MP: 
		case GPAC_OTI_AUDIO_AAC_MPEG2_LCP: 
		case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP: 
		case GPAC_OTI_AUDIO_AAC_MPEG4:
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

/*return value:
	0: not supported 
	1: ISO media 
	2: input bt file (.bt, .wrl)
	3: input XML file (.xmt)
	4: input SVG file (.svg)
	5: input SWF file (.swf)
	6: input LASeR file (.lsr or .saf)
*/
u32 get_file_type_by_ext(char *inName)
{
	u32 type = 0;
	char *ext = strrchr(inName, '.');
	if (ext) {
		char *sep;
		if (!strcmp(ext, ".gz")) ext = strrchr(ext-1, '.');
		ext+=1;
		sep = strchr(ext, '.');
		if (sep) sep[0] = 0;

		if (!stricmp(ext, "mp4") || !stricmp(ext, "3gp") || !stricmp(ext, "mov") || !stricmp(ext, "3g2") || !stricmp(ext, "3gs")) type = 1;
		else if (!stricmp(ext, "bt") || !stricmp(ext, "wrl") || !stricmp(ext, "x3dv")) type = 2;
		else if (!stricmp(ext, "xmt") || !stricmp(ext, "x3d")) type = 3;
		else if (!stricmp(ext, "lsr") || !stricmp(ext, "saf")) type = 6;
		else if (!stricmp(ext, "svg")) type = 4;
		else if (!stricmp(ext, "xsr")) type = 4;
		else if (!stricmp(ext, "xml")) type = 4;
		else if (!stricmp(ext, "swf")) type = 5;
		else if (!stricmp(ext, "jp2")) {
			if (sep) sep[0] = '.';
			return 0;
		}
		else type = 0;

		if (sep) sep[0] = '.';
	}


	/*try open file in read mode*/
	if (!type && gf_isom_probe_file(inName)) type = 1;
	return type;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
static Bool can_convert_to_isma(GF_ISOFile *file)
{
	u32 spec = gf_isom_guess_specification(file);
	if (spec==GF_4CC('I','S','M','A')) return 1;
	return 0;
}
#endif

static void progress_quiet(const void *cbck, const char *title, u64 done, u64 total) { }


typedef struct
{
	u32 trackID;
	char *line;
} SDPLine;

typedef struct
{
	/*actions:
		0: set meta type
		1: add item
		2: rem item
		3: set item primary
		4: set XML
		5: set binary XML
		6: rem XML
		7: dump item
		8: dump XML
	*/
	u32 act_type;
	Bool root_meta, use_dref;
	u32 trackID;
	u32 meta_4cc;
	char szPath[GF_MAX_PATH];
	char szName[1024], mime_type[1024], enc_type[1024];
	u32 item_id;
} MetaAction;

#ifndef GPAC_DISABLE_ISOM_WRITE
static Bool parse_meta_args(MetaAction *meta, u32 act_type, char *opts)
{
	Bool ret = 0;
	char szSlot[1024], *next;

	memset(meta, 0, sizeof(MetaAction));
	meta->act_type = act_type;
	meta->mime_type[0] = 0;
	meta->enc_type[0] = 0;
	meta->szName[0] = 0;
	meta->szPath[0] = 0;
	meta->trackID = 0;
	meta->root_meta = 1;

	if (!opts) return 0;
	while (1) {
		if (!opts || !opts[0]) return ret;
		if (opts[0]==':') opts += 1;
		strcpy(szSlot, opts);
		next = strchr(szSlot, ':');
		/*use ':' as separator, but beware DOS paths...*/
		if (next && next[1]=='\\') next = strchr(szSlot+2, ':');
		if (next) next[0] = 0;
		
		if (!strnicmp(szSlot, "tk=", 3)) {
			sscanf(szSlot, "tk=%u", &meta->trackID);
			meta->root_meta = 0;
			ret = 1;
		}
		else if (!strnicmp(szSlot, "id=", 3)) { meta->item_id = atoi(szSlot+3); ret = 1; }
		else if (!strnicmp(szSlot, "name=", 5)) { strcpy(meta->szName, szSlot+5); ret = 1; }
		else if (!strnicmp(szSlot, "path=", 5)) { strcpy(meta->szPath, szSlot+5); ret = 1; }
		else if (!strnicmp(szSlot, "mime=", 5)) { strcpy(meta->mime_type, szSlot+5); ret = 1; }
		else if (!strnicmp(szSlot, "encoding=", 9)) { strcpy(meta->enc_type, szSlot+9); ret = 1; }
		else if (!strnicmp(szSlot, "dref", 4)) { meta->use_dref = 1; ret = 1; }
		else if (!stricmp(szSlot, "binary")) {
			if (meta->act_type==4) meta->act_type=5;
			ret = 1;
		}
		else if (!strchr(szSlot, '=')) {
			switch (meta->act_type) {
			case 0:
				if (!stricmp(szSlot, "null") || !stricmp(szSlot, "0")) meta->meta_4cc = 0;
				else meta->meta_4cc = GF_4CC(szSlot[0], szSlot[1], szSlot[2], szSlot[3]);
				ret = 1;
				break;
			case 1: 
			case 4: 
			case 7: 
				strcpy(meta->szPath, szSlot);  
				ret = 1;
				break;
			case 2: 
			case 3: 
			case 8: 
				meta->item_id = atoi(szSlot);  
				ret = 1;
				break;
			}
		}
		opts += strlen(szSlot);
	}
	return ret;
}
#endif




typedef struct
{	
	/*0: set tsel param - 1 remove tsel - 2 remove all tsel info in alternate group - 3 remove all tsel info in file*/
	u32 act_type;
	u32 trackID;

	u32 refTrackID;
	u32 criteria[30];
	u32 nb_criteria;
	Bool is_switchGroup;
	u32 switchGroupID;
} TSELAction;

static Bool parse_tsel_args(TSELAction **__tsel_list, char *opts, u32 *nb_tsel_act)
{
	u32 act;
	u32 refTrackID = 0;
	Bool has_switch_id;
	u32 switch_id = 0;
	u32 criteria[30];
	u32 nb_criteria = 0;
	TSELAction *tsel_act;
	char szSlot[1024], *next;
	TSELAction *tsel_list = *__tsel_list;

	has_switch_id = 0;
	act = tsel_list[*nb_tsel_act].act_type;
			

	if (!opts) return 0;
	while (1) {
		if (!opts || !opts[0]) return 1;
		if (opts[0]==':') opts += 1;
		strcpy(szSlot, opts);
		next = strchr(szSlot, ':');
		/*use ':' as separator, but beware DOS paths...*/
		if (next && next[1]=='\\') next = strchr(szSlot+2, ':');
		if (next) next[0] = 0;


		if (!strnicmp(szSlot, "ref=", 4)) refTrackID = atoi(szSlot+4);  
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
			__tsel_list = gf_realloc(__tsel_list, sizeof(TSELAction) * (*nb_tsel_act + 1));
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


#define CHECK_NEXT_ARG	if (i+1==(u32)argc) { fprintf(stderr, "Missing arg - please check usage\n"); MP4BOX_EXIT_WITH_CODE(1); }

typedef struct
{
	/*
	0: rem track
	1: set track language
	2: set track delay
	3: set track KMS URI
	4: set visual track PAR if possible
	5: set track handler name
	6: enables track
	7: disables track
	8: referenceTrack
	9: raw extraction
	*/
	u32 act_type;
	/*track ID*/
	u32 trackID;
	char lang[4];
	s32 delay_ms;
	const char *kms;
	const char *hdl_name;
	s32 par_num, par_den;
	u32 dump_type, sample_num;
	char *out_name;
} TrackAction;

enum
{
	GF_ISOM_CONV_TYPE_ISMA = 1,
	GF_ISOM_CONV_TYPE_ISMA_EX,
	GF_ISOM_CONV_TYPE_3GPP,
	GF_ISOM_CONV_TYPE_IPOD,
	GF_ISOM_CONV_TYPE_PSP
};

#define MP4BOX_EXIT_WITH_CODE(__ret_code)	\
	if (mpd_base_urls) gf_free(mpd_base_urls);	\
	if (sdp_lines) gf_free(sdp_lines); \
	if (metas) gf_free(metas); \
	if (tracks) { \
		for (i=0; i<nb_track_act; i++) { \
			if (tracks[i].out_name) \
				gf_free(tracks[i].out_name); \
		} \
		gf_free(tracks); \
	}\
	if (tsel_acts) gf_free(tsel_acts); \
	if (brand_add) gf_free(brand_add); \
	if (brand_rem) gf_free(brand_rem); \
	if (dash_inputs) gf_free(dash_inputs); \
	gf_sys_close();	\
	return __ret_code; \


GF_DashSegmenterInput *set_dash_input(GF_DashSegmenterInput *dash_inputs, char *name, u32 *nb_dash_inputs)
{
	GF_DashSegmenterInput *di;
	char *sep = strchr(name, ':');
	if (sep && (sep[1]=='\\')) sep = strchr(sep+1, ':');

	dash_inputs = gf_realloc(dash_inputs, sizeof(GF_DashSegmenterInput) * (*nb_dash_inputs + 1) );
	memset(&dash_inputs[*nb_dash_inputs], 0, sizeof(GF_DashSegmenterInput) );
	di = &dash_inputs[*nb_dash_inputs];
	(*nb_dash_inputs)++;

	if (sep) {
		char *opts = sep+1;
		sep[0] = 0;
		while (opts) {
			sep = strchr(opts, ':');
			if (sep) sep[0] = 0;
			if (!strnicmp(opts, "id=", 3)) {
				u32 i;
				strncpy(di->representationID, opts+3, 99);
				/* check to see if this representation Id has already been assigned */
				for (i=0; i<(*nb_dash_inputs)-1;i++) {
					GF_DashSegmenterInput *other_di;
					other_di = &dash_inputs[i];
					if (!strcmp(other_di->representationID, di->representationID)) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error: Duplicate Representation ID \"%s\" in command line\n", di->representationID));
					}
				}
			} else if (!strnicmp(opts, "period=", 7)) strncpy(di->periodID, opts+7, 99);
			else if (!strnicmp(opts, "bandwidth=", 10)) di->bandwidth = atoi(opts+10);
			else if (!strnicmp(opts, "role=", 5)) strncpy(di->role, opts+5, 99);
			
			if (!sep) break;
			sep[0] = ':';
			opts = sep+1;
		}
	}
	di->file_name = name;
	if (!strlen(di->representationID)) sprintf(di->representationID, "%d", *nb_dash_inputs);

	return dash_inputs;
}

static GF_Err parse_track_action_params(char *string, TrackAction *action)
{
	char *param = string;
	while (param) {
		param = strchr(param, ':');
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
	(*actions)[*nb_track_act].act_type = 9;
	(*actions)[*nb_track_act].dump_type = dump_type;
	parse_track_action_params(string, &(*actions)[*nb_track_act]);
	(*nb_track_act)++;
	return dump_type;
}

static void on_gpac_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list)
{
	FILE *logs = cbk;
	vfprintf(logs, fmt, list);
	fflush(logs);
}

int mp4boxMain(int argc, char **argv)
{
	char outfile[5000];
	GF_Err e;
#ifndef GPAC_DISABLE_SCENE_ENCODER
	GF_SMEncodeOptions opts;
#endif
	SDPLine *sdp_lines = NULL;
	Double interleaving_time, split_duration, split_start, import_fps, dash_duration, dash_subduration;
	MetaAction *metas = NULL;
	TrackAction *tracks = NULL;
	TSELAction *tsel_acts = NULL;
	u64 movie_time, initial_tfdt;
	s32 subsegs_per_sidx;
	u32 *brand_add = NULL;
	u32 *brand_rem = NULL;
	GF_DashSwitchingMode bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
	u32 i, stat_level, hint_flags, info_track_id, import_flags, nb_add, nb_cat, crypt, agg_samples, nb_sdp_ex, max_ptime, raw_sample_num, split_size, nb_meta_act, nb_track_act, rtp_rate, major_brand, nb_alt_brand_add, nb_alt_brand_rem, old_interleave, car_dur, minor_version, conv_type, nb_tsel_acts, program_number, dump_nal, time_shift_depth, dash_dynamic, initial_moof_sn, dump_std, dump_mode, import_subtitle;
	Bool HintIt, needSave, FullInter, Frag, HintInter, dump_rtp, regular_iod, remove_sys_tracks, remove_hint, force_new, remove_root_od;
	Bool print_sdp, print_info, open_edit, dump_isom, dump_cr, force_ocr, encode, do_log, do_flat, dump_srt, dump_ttxt, dump_timestamps, do_saf, dump_m2ts, dump_cart, do_hash, verbose, force_cat, align_cat, pack_wgt, single_group, dash_live, no_fragments_defaults;
	char *inName, *outName, *arg, *mediaSource, *tmpdir, *input_ctx, *output_ctx, *drm_file, *avi2raw, *cprt, *chap_file, *pes_dump, *itunes_tags, *pack_file, *raw_cat, *seg_name, *dash_ctx_file;
	u32 track_dump_type;
	u32 trackID;
	Double min_buffer = 1.5;
	u32 ast_shift_sec = 1;
	u32 dump_chap = 0;
	char **mpd_base_urls = NULL;
	u32 nb_mpd_base_urls=0;
	u32 dash_scale = 1000;

#ifndef GPAC_DISABLE_MPD
	Bool do_mpd = 0;
#endif
#ifndef GPAC_DISABLE_SCENE_ENCODER
	Bool chunk_mode=0;
#endif
#ifndef GPAC_DISABLE_ISOM_HINTING
	Bool HintCopy=0;
	u32 MTUSize = 1450;
#endif
	GF_ISOFile *file;
	u32 mpd_update_time = 0;
	Bool stream_rtp=0;
	Bool live_scene=0;
	Bool enable_mem_tracker = 0;
	Bool dump_iod=0;
	Bool pssh_in_moof=0;
	Bool daisy_chain_sidx=0;
	Bool single_segment=0;
	Bool single_file=0;
	Bool segment_timeline=0;
	u32 segment_marker = 0;
	GF_DashProfile dash_profile = GF_DASH_PROFILE_UNKNOWN;
	Bool use_url_template=0;
	Bool seg_at_rap=0;
	Bool frag_at_rap=0;
	Bool adjust_split_end = 0;
	Bool memory_frags = 1;
	Bool keep_utc = 0;
	const char *do_wget = NULL;
	GF_DashSegmenterInput *dash_inputs = NULL;
	u32 nb_dash_inputs = 0;
	char *gf_logs = NULL;
	char *seg_ext = NULL;
	const char *dash_title = NULL;
	const char *dash_source = NULL;
	const char *dash_more_info = NULL;
#if !defined(GPAC_DISABLE_STREAMING)
	const char *grab_m2ts = NULL;
#endif
	FILE *logfile = NULL;
	
	nb_tsel_acts = nb_add = nb_cat = nb_track_act = nb_sdp_ex = max_ptime = raw_sample_num = nb_meta_act = rtp_rate = major_brand = nb_alt_brand_add = nb_alt_brand_rem = car_dur = minor_version = 0;
	e = GF_OK;
	split_duration = 0.0;
	split_start = -1.0;
	interleaving_time = 0.0;
	dash_duration = dash_subduration = 0.0;
	import_fps = 0;
	import_flags = 0;
	split_size = 0;
	movie_time = 0;
	dump_nal = 0;
	FullInter = HintInter = encode = do_log = old_interleave = do_saf = do_hash = verbose = 0;
	dump_mode = Frag = force_ocr = remove_sys_tracks = agg_samples = remove_hint = keep_sys_tracks = remove_root_od = single_group = 0;
	conv_type = HintIt = needSave = print_sdp = print_info = regular_iod = dump_std = open_edit = dump_isom = dump_rtp = dump_cr = dump_srt = dump_ttxt = force_new = dump_timestamps = dump_m2ts = dump_cart = import_subtitle = force_cat = pack_wgt = dash_live = 0;
	no_fragments_defaults = 0;
	dash_dynamic = 0;
	/*align cat is the new default behaviour for -cat*/
	align_cat = 1;
	subsegs_per_sidx = 0;
	track_dump_type = 0;
	crypt = 0;
	time_shift_depth = 0;
	file = NULL;
	itunes_tags = pes_dump = NULL;
	seg_name = dash_ctx_file = NULL;
	initial_moof_sn = 0;
	initial_tfdt = 0;

#ifndef GPAC_DISABLE_SCENE_ENCODER
	memset(&opts, 0, sizeof(opts));
#endif

	trackID = stat_level = hint_flags = 0;
	program_number = 0;
	info_track_id = 0;
	do_flat = 0;
	inName = outName = mediaSource = input_ctx = output_ctx = drm_file = avi2raw = cprt = chap_file = pack_file = raw_cat = NULL;

#ifndef GPAC_DISABLE_SWF_IMPORT
	swf_flags = GF_SM_SWF_SPLIT_TIMELINE;
#endif
	swf_flatten_angle = 0.0f;
	tmpdir = NULL;



	for (i = 1; i < (u32) argc ; i++) {
		if (!strcmp(argv[i], "-mem-track")) {
#ifdef GPAC_MEMORY_TRACKING
			enable_mem_tracker = 1;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"-mem-track\"\n"); 
#endif
			break;
		}
	}

	gf_sys_init(enable_mem_tracker);
	if (argc < 2) {
		PrintUsage();
		MP4BOX_EXIT_WITH_CODE(1);
	}


	/*parse our args*/
	for (i = 1; i < (u32) argc ; i++) {
		arg = argv[i];
		/*input file(s)*/
		if ((arg[0] != '-') || !stricmp(arg, "--")) {
			char *arg_val = arg;
			if (!stricmp(arg, "--")) {
				CHECK_NEXT_ARG
				arg_val = argv[i+1];
				i++;
			}
			if (argc < 3) {
				fprintf(stderr, "Error - only one input file found as argument, please check usage\n"); 
				MP4BOX_EXIT_WITH_CODE(1); 
			} else if (inName) { 
				if (dash_duration) {
					if (!nb_dash_inputs) {
						dash_inputs = set_dash_input(dash_inputs, inName, &nb_dash_inputs);
					}
					dash_inputs = set_dash_input(dash_inputs, arg_val, &nb_dash_inputs);
				} else {
					fprintf(stderr, "Error - 2 input names specified, please check usage\n");
					MP4BOX_EXIT_WITH_CODE(1); 
				}
			} else {
				inName = arg_val;
			}
		}
		else if (!stricmp(arg, "-?")) { PrintUsage(); MP4BOX_EXIT_WITH_CODE(0); }
		else if (!stricmp(arg, "-version")) { PrintVersion(); MP4BOX_EXIT_WITH_CODE(0); }
		else if (!stricmp(arg, "-sdp")) print_sdp = 1;
		else if (!stricmp(arg, "-quiet")) quiet = 2;
		else if (!strcmp(argv[i], "-mem-track")) continue;
		
		else if (!stricmp(arg, "-logs")) {
			CHECK_NEXT_ARG
			gf_logs = argv[i+1];
			if (gf_logs)
				gf_log_set_tools_levels(gf_logs);
			i++;
		}
		else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
			logfile = gf_f64_open(argv[i+1], "wt");
			gf_log_set_callback(logfile, on_gpac_log);
			i++;
		} 
		else if (!stricmp(arg, "-noprog")) quiet = 1;
		else if (!stricmp(arg, "-info")) {
			print_info = 1;
			if ((i+1<(u32) argc) && (sscanf(argv[i+1], "%u", &info_track_id)==1)) {
				char szTk[20];
				sprintf(szTk, "%u", info_track_id);
				if (!strcmp(szTk, argv[i+1])) i++;
				else info_track_id=0;
			} else {
				info_track_id=0;
			}
		}
#if !defined(GPAC_DISABLE_STREAMING)
		else if (!stricmp(arg, "-grab-ts")) {
			CHECK_NEXT_ARG
			grab_m2ts = argv[i+1];
			i++;
		}		
#endif
#if !defined(GPAC_DISABLE_CORE_TOOLS)
		else if (!stricmp(arg, "-wget")) {
			CHECK_NEXT_ARG
			do_wget = argv[i+1];
			i++;
		}		
#endif
		/*******************************************************************************/
		else if (!stricmp(arg, "-dvbhdemux")) {
			dvbhdemux = 1;
		}
		/********************************************************************************/
#ifndef GPAC_DISABLE_MEDIA_EXPORT
		else if (!stricmp(arg, "-raw")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_NATIVE);
			i++;
		}
		else if (!stricmp(arg, "-raw-layer")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_NATIVE | GF_EXPORT_SVC_LAYER);
			i++;
		}
		else if (!stricmp(arg, "-qcp")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_NATIVE | GF_EXPORT_USE_QCP);
			i++;
		}
		else if (!stricmp(arg, "-aviraw")) {
			CHECK_NEXT_ARG
			if (argv[i+1] && !stricmp(argv[i+1], "video")) trackID = 1;
			else if (argv[i+1] && !stricmp(argv[i+1], "audio")) {
				if (strlen(argv[i+1])==5) trackID = 2;
				else trackID = 1 + atoi(argv[i+1] + 5);
			}
			else { fprintf(stderr, "Usage: \"-aviraw video\" or \"-aviraw audio\"\n"); MP4BOX_EXIT_WITH_CODE(1); }
			track_dump_type = GF_EXPORT_AVI_NATIVE;
			i++;
		}
		else if (!stricmp(arg, "-raws")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_RAW_SAMPLES);
			i++;
		}
		else if (!stricmp(arg, "-nhnt")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_NHNT);
			i++;
		}
		else if (!stricmp(arg, "-nhml")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_NHML);
			i++;
		}
		else if (!stricmp(arg, "-webvtt-raw")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_WEBVTT_META);
			i++;
		}
		else if (!stricmp(arg, "-six")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_SIX);
			i++;
		}
		else if (!stricmp(arg, "-avi")) {
			CHECK_NEXT_ARG
			track_dump_type = create_new_track_action(argv[i+1], &tracks, &nb_track_act, GF_EXPORT_AVI);
			i++;
		}
#endif /*GPAC_DISABLE_MEDIA_EXPORT*/
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
		else if (!stricmp(arg, "-rtp")) {
			stream_rtp = 1;
		}
		else if (!stricmp(arg, "-live")) {
			live_scene = 1;
		}
#endif
		else if (!stricmp(arg, "-diod")) {
			dump_iod = 1;
		}
#ifndef GPAC_DISABLE_VRML
		else if (!stricmp(arg, "-node")) { CHECK_NEXT_ARG PrintNode(argv[i+1], 0); MP4BOX_EXIT_WITH_CODE(0); }
		else if (!stricmp(arg, "-xnode")) { CHECK_NEXT_ARG PrintNode(argv[i+1], 1); MP4BOX_EXIT_WITH_CODE(0); }
		else if (!stricmp(arg, "-nodes")) { PrintBuiltInNodes(0); MP4BOX_EXIT_WITH_CODE(0); }
		else if (!stricmp(arg, "-xnodes")) { PrintBuiltInNodes(1); MP4BOX_EXIT_WITH_CODE(0); } 
#endif
#ifndef GPAC_DISABLE_SVG
		else if (!stricmp(arg, "-snode")) { CHECK_NEXT_ARG PrintNode(argv[i+1], 2); MP4BOX_EXIT_WITH_CODE(0); }
		else if (!stricmp(arg, "-snodes")) { PrintBuiltInNodes(2); MP4BOX_EXIT_WITH_CODE(0); } 
#endif
		else if (!stricmp(arg, "-std")) dump_std = 2;
		else if (!stricmp(arg, "-stdb")) dump_std = 1;

#if !defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_SCENE_DUMP)
		else if (!stricmp(arg, "-bt")) dump_mode = 1 + GF_SM_DUMP_BT;
		else if (!stricmp(arg, "-xmt")) dump_mode = 1 + GF_SM_DUMP_XMTA;
		else if (!stricmp(arg, "-wrl")) dump_mode = 1 + GF_SM_DUMP_VRML;
		else if (!stricmp(arg, "-x3dv")) dump_mode = 1 + GF_SM_DUMP_X3D_VRML;
		else if (!stricmp(arg, "-x3d")) dump_mode = 1 + GF_SM_DUMP_X3D_XML;
		else if (!stricmp(arg, "-lsr")) dump_mode = 1 + GF_SM_DUMP_LASER;
		else if (!stricmp(arg, "-svg")) dump_mode = 1 + GF_SM_DUMP_SVG;
#endif /*defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_SCENE_DUMP)*/

		else if (!stricmp(arg, "-stat")) stat_level = 1;
		else if (!stricmp(arg, "-stats")) stat_level = 2;
		else if (!stricmp(arg, "-statx")) stat_level = 3;
		else if (!stricmp(arg, "-diso")) dump_isom = 1;
		else if (!stricmp(arg, "-dump-cover")) dump_cart = 1;
		else if (!stricmp(arg, "-dump-chap")) dump_chap = 1;
		else if (!stricmp(arg, "-dump-chap-ogg")) dump_chap = 2;
		else if (!stricmp(arg, "-hash")) do_hash = 1;

		else if (!stricmp(arg, "-dmp4")) {
			dump_isom = 1;
			fprintf(stderr, "WARNING: \"-dmp4\" is deprecated - use \"-diso\" option\n");
		}
		else if (!stricmp(arg, "-drtp")) dump_rtp = 1;
		else if (!stricmp(arg, "-dts")) {
			dump_timestamps = 1;
			if ( ((i+1<(u32) argc) && inName) || (i+2<(u32) argc) ) {
				if (argv[i+1][0] != '-') program_number = atoi(argv[i+1]);
				i++;
			}
		} else if (!stricmp(arg, "-dnal")) {
			CHECK_NEXT_ARG
				dump_nal = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-dcr")) dump_cr = 1;
		else if (!stricmp(arg, "-ttxt") || !stricmp(arg, "-srt")) {
			if ((i+1<(u32) argc) && (sscanf(argv[i+1], "%u", &trackID)==1)) {
				char szTk[20];
				sprintf(szTk, "%d", trackID);
				if (!strcmp(szTk, argv[i+1])) i++;
				else trackID=0;
			} else {
				trackID = 0;
			}
#ifdef GPAC_DISABLE_ISOM_WRITE
			if (trackID) { fprintf(stderr, "Error: Read-Only version - subtitle conversion not available\n"); MP4BOX_EXIT_WITH_CODE(1); }
#endif
			if (!stricmp(arg, "-ttxt")) dump_ttxt = 1;
			else dump_srt = 1;
			import_subtitle = 1;
		} else if (!stricmp(arg, "-dm2ts")) {
			dump_m2ts = 1;
			if ( ((i+1<(u32) argc) && inName) || (i+2<(u32) argc) ) {
				if (argv[i+1][0] != '-') pes_dump = argv[i+1];
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
			swf_flatten_angle = (Float) atof(argv[i+1]);
			i++;
		}
#endif
#ifndef GPAC_DISABLE_ISOM_WRITE
		else if (!stricmp(arg, "-isma")) { conv_type = GF_ISOM_CONV_TYPE_ISMA; open_edit = 1; }
		else if (!stricmp(arg, "-3gp")) { conv_type = GF_ISOM_CONV_TYPE_3GPP; open_edit = 1; }
		else if (!stricmp(arg, "-ipod")) { conv_type = GF_ISOM_CONV_TYPE_IPOD; open_edit = 1; }
		else if (!stricmp(arg, "-psp")) { conv_type = GF_ISOM_CONV_TYPE_PSP; open_edit = 1; }
		else if (!stricmp(arg, "-ismax")) { conv_type = GF_ISOM_CONV_TYPE_ISMA_EX; open_edit = 1; }

		else if (!stricmp(arg, "-no-sys") || !stricmp(arg, "-nosys")) { remove_sys_tracks = 1; open_edit = 1; }
		else if (!stricmp(arg, "-no-iod")) { remove_root_od = 1; open_edit = 1; }
		else if (!stricmp(arg, "-out")) { CHECK_NEXT_ARG outName = argv[i+1]; i++; }
		else if (!stricmp(arg, "-tmp")) {
			CHECK_NEXT_ARG tmpdir = argv[i+1]; i++; 
		}
		else if (!stricmp(arg, "-write-buffer")) {
			CHECK_NEXT_ARG 
			gf_isom_set_output_buffering(NULL, atoi(argv[i+1]));
			i++; 
		}
		else if (!stricmp(arg, "-cprt")) { CHECK_NEXT_ARG cprt = argv[i+1]; i++; if (!dash_duration) open_edit = 1; }
		else if (!stricmp(arg, "-chap")) { CHECK_NEXT_ARG chap_file = argv[i+1]; i++; open_edit = 1; }
		else if (!strcmp(arg, "-strict-error")) {
			gf_log_set_strict_error(1);
		} else if (!stricmp(arg, "-inter") || !stricmp(arg, "-old-inter")) {
			CHECK_NEXT_ARG
			interleaving_time = atof(argv[i+1]) / 1000;
			open_edit = 1;
			needSave = 1;
			if (!stricmp(arg, "-old-inter")) old_interleave = 1;
			i++;
		} else if (!stricmp(arg, "-frag")) {
			CHECK_NEXT_ARG
			interleaving_time = atof(argv[i+1]) / 1000;
			needSave = 1;
			i++;
			Frag = 1;
		} else if (!stricmp(arg, "-dash")) {
			CHECK_NEXT_ARG
			dash_duration = atof(argv[i+1]) / 1000;
			i++;
		} else if (!stricmp(arg, "-subdur")) {
			CHECK_NEXT_ARG
			dash_subduration = atof(argv[i+1]) / 1000;
			i++;
		} else if (!stricmp(arg, "-dash-scale")) {
			CHECK_NEXT_ARG
			dash_scale = atoi(argv[i+1]);
			if (!dash_scale) {
				fprintf(stderr, "\tERROR: \"-dash-scale\": invalid parameter %s\n", argv[i+1]);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			i++;
		} else if (!stricmp(arg, "-dash-ts-prog")) {
			CHECK_NEXT_ARG
			program_number = atoi(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-subsegs-per-sidx") || !stricmp(arg, "-frags-per-sidx")) {
			CHECK_NEXT_ARG
			subsegs_per_sidx = atoi(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-segment-name")) {
			CHECK_NEXT_ARG
			seg_name = argv[i+1];
			i++;
		} else if (!stricmp(arg, "-segment-ext")) {
			CHECK_NEXT_ARG
			seg_ext = argv[i+1];
			i++;
		} else if (!stricmp(arg, "-bs-switching")) {
			CHECK_NEXT_ARG
			if (!stricmp(argv[i+1], "no") || !stricmp(argv[i+1], "off")) bitstream_switching_mode = GF_DASH_BSMODE_NONE;
			else if (!stricmp(argv[i+1], "merge"))  bitstream_switching_mode = GF_DASH_BSMODE_MERGED;
			else if (!stricmp(argv[i+1], "single"))  bitstream_switching_mode = GF_DASH_BSMODE_SINGLE;
			else if (!stricmp(argv[i+1], "inband"))  bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
			else bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
			i++;
		}
		else if (!stricmp(arg, "-dynamic")) { dash_dynamic = 1; }
		else if (!strnicmp(arg, "-dash-live", 10) || !strnicmp(arg, "-ddbg-live", 10)) {
			dash_dynamic = !strnicmp(arg, "-ddbg-live", 10) ? 2 : 1;
			dash_live = 1;
			if (arg[10]=='=') {
				dash_ctx_file = arg+11;
			}
			CHECK_NEXT_ARG
			dash_duration = atof(argv[i+1]) / 1000;
			i++;
		}
		else if (!stricmp(arg, "-mpd-refresh")) { CHECK_NEXT_ARG mpd_update_time = atoi(argv[i+1]); i++;   }
		else if (!stricmp(arg, "-time-shift")) { 
			CHECK_NEXT_ARG 
			time_shift_depth = (u32) atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-min-buffer")) { 
			CHECK_NEXT_ARG 
			min_buffer = atoi(argv[i+1]);
			min_buffer /= 1000;
			i++;
		}
		else if (!stricmp(arg, "-ast-offset")) { 
			CHECK_NEXT_ARG 
			ast_shift_sec = (u32) atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-moof-sn")) { 
			CHECK_NEXT_ARG 
			initial_moof_sn = (u32) atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-tfdt")) { 
			CHECK_NEXT_ARG 
			sscanf(argv[i+1], LLU, &initial_tfdt);
			i++;
		}
		else if (!stricmp(arg, "-no-frags-default")) { no_fragments_defaults = 1; }
		else if (!stricmp(arg, "-mpd-title")) { CHECK_NEXT_ARG dash_title = argv[i+1]; i++; }
		else if (!stricmp(arg, "-mpd-source")) { CHECK_NEXT_ARG dash_source = argv[i+1]; i++; }
		else if (!stricmp(arg, "-mpd-info-url")) { CHECK_NEXT_ARG dash_more_info = argv[i+1]; i++; }
		else if (!stricmp(arg, "-base-url")) { 
			CHECK_NEXT_ARG 
			dash_more_info = argv[i+1]; 
			mpd_base_urls = gf_realloc(mpd_base_urls, (nb_mpd_base_urls+1)*sizeof(char**));
			mpd_base_urls[nb_mpd_base_urls] = argv[i+1];
			nb_mpd_base_urls++;
			i++; 
		}
		
		else if (!stricmp(arg, "-dash-ctx")) {
			CHECK_NEXT_ARG
			dash_ctx_file = argv[i+1];
			i++;
		} else if (!stricmp(arg, "-daisy-chain")) {
			daisy_chain_sidx = 1;
		} else if (!stricmp(arg, "-single-segment")) {
			single_segment = 1;
		} else if (!stricmp(arg, "-single-file")) {
			single_file = 1;
		} else if (!stricmp(arg, "-pssh-moof")) {
			pssh_in_moof = 1;
		} else if (!stricmp(arg, "-dash-profile") || !stricmp(arg, "-profile")) {
			CHECK_NEXT_ARG
			if (!stricmp(argv[i+1], "live") || !stricmp(argv[i+1], "simple")) dash_profile = GF_DASH_PROFILE_LIVE;
			else if (!stricmp(argv[i+1], "onDemand")) dash_profile = GF_DASH_PROFILE_ONDEMAND;
			else if (!stricmp(argv[i+1], "dashavc264:live")) {
				dash_profile = GF_DASH_PROFILE_AVC264_LIVE;
				no_fragments_defaults = 1;
			} else if (!stricmp(argv[i+1], "dashavc264:onDemand")) {
				dash_profile = GF_DASH_PROFILE_AVC264_ONDEMAND;
				no_fragments_defaults = 1;
			} else if (!stricmp(argv[i+1], "main")) dash_profile = GF_DASH_PROFILE_MAIN;
			else dash_profile = GF_DASH_PROFILE_FULL;
			i++;
		} else if (!strnicmp(arg, "-url-template", 13)) {
			use_url_template = 1;
			if ((arg[13]=='=') && arg[14]) {
				if (!strcmp( &arg[14], "simulate")) use_url_template = 2;
			}
		} else if (!stricmp(arg, "-segment-timeline")) {
			segment_timeline = 1;
		} else if (!stricmp(arg, "-mem-frags")) {
			memory_frags = 1;
		} else if (!stricmp(arg, "-segment-marker")) {
			char *m;
			CHECK_NEXT_ARG
			m = argv[i+1];
			segment_marker = GF_4CC(m[0], m[1], m[2], m[3]);
		} else if (!stricmp(arg, "-itags")) { CHECK_NEXT_ARG itunes_tags = argv[i+1]; i++; open_edit = 1; }
#ifndef GPAC_DISABLE_ISOM_HINTING
		else if (!stricmp(arg, "-hint")) { open_edit = 1; HintIt = 1; }
		else if (!stricmp(arg, "-unhint")) { open_edit = 1; remove_hint = 1; }
		else if (!stricmp(arg, "-copy")) HintCopy = 1;
		else if (!stricmp(arg, "-tight")) {
			FullInter = 1;
			open_edit = 1;
			needSave = 1;
		} else if (!stricmp(arg, "-ocr")) force_ocr = 1;
		else if (!stricmp(arg, "-latm")) hint_flags |= GP_RTP_PCK_USE_LATM_AAC;
		else if (!stricmp(arg, "-rap")) {
			hint_flags |= GP_RTP_PCK_SIGNAL_RAP;
			seg_at_rap=1;
		}
		else if (!stricmp(arg, "-frag-rap")) {
			frag_at_rap=1;
		}
		else if (!stricmp(arg, "-ts")) hint_flags |= GP_RTP_PCK_SIGNAL_TS;
		else if (!stricmp(arg, "-size")) hint_flags |= GP_RTP_PCK_SIGNAL_SIZE;
		else if (!stricmp(arg, "-idx")) hint_flags |= GP_RTP_PCK_SIGNAL_AU_IDX;
		else if (!stricmp(arg, "-static")) hint_flags |= GP_RTP_PCK_USE_STATIC_ID;
		else if (!stricmp(arg, "-multi")) {
			hint_flags |= GP_RTP_PCK_USE_MULTI;
			if ((i+1<(u32) argc) && (sscanf(argv[i+1], "%u", &max_ptime)==1)) {
				char szPt[20];
				sprintf(szPt, "%u", max_ptime);
				if (!strcmp(szPt, argv[i+1])) i++;
				else max_ptime=0;
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
		else if (!stricmp(arg, "-mtu")) { CHECK_NEXT_ARG MTUSize = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-cardur")) { CHECK_NEXT_ARG car_dur = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-rate")) { CHECK_NEXT_ARG rtp_rate = atoi(argv[i+1]); i++; }
#ifndef GPAC_DISABLE_SENG
		else if (!stricmp(arg, "-add-sdp") || !stricmp(arg, "-sdp_ex")) {
			char *id;
			CHECK_NEXT_ARG
			sdp_lines = gf_realloc(sdp_lines, sizeof(SDPLine) * (nb_sdp_ex+1) );

			id = strchr(argv[i+1], ':');
			if (id) {
				id[0] = 0;
				if (sscanf(argv[i+1], "%u", &sdp_lines[0].trackID)==1) {
					id[0] = ':';
					sdp_lines[nb_sdp_ex].line = id+1;
				} else {
					id[0] = ':';
					sdp_lines[nb_sdp_ex].line = argv[i+1];
					sdp_lines[nb_sdp_ex].trackID = 0;
				}
			} else {
				sdp_lines[nb_sdp_ex].line = argv[i+1];
				sdp_lines[nb_sdp_ex].trackID = 0;
			}
			open_edit = 1;
			nb_sdp_ex++;
			i++;
		}
#endif /*GPAC_DISABLE_SENG*/
#endif /*GPAC_DISABLE_ISOM_HINTING*/

		else if (!stricmp(arg, "-single")) {
#ifndef GPAC_DISABLE_MEDIA_EXPORT
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_MP4;
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );
			tracks[nb_track_act].act_type = 9;
			tracks[nb_track_act].trackID = atoi(argv[i+1]);
			tracks[nb_track_act].dump_type = GF_EXPORT_MP4;
			nb_track_act++;
			i++;
#endif
		}
		else if (!stricmp(arg, "-iod")) regular_iod = 1;
		else if (!stricmp(arg, "-flat")) do_flat = 1;
		else if (!stricmp(arg, "-keep-utc")) keep_utc = 1;
		else if (!stricmp(arg, "-new")) force_new = 1;
		else if (!stricmp(arg, "-add") || !stricmp(arg, "-import") || !stricmp(arg, "-convert")) {
			CHECK_NEXT_ARG
			if (!stricmp(arg, "-import")) fprintf(stderr, "\tWARNING: \"-import\" is deprecated - use \"-add\"\n");
			else if (!stricmp(arg, "-convert")) fprintf(stderr, "\tWARNING: \"-convert\" is deprecated - use \"-add\"\n");
			nb_add++;
			i++;
		}
		else if (!stricmp(arg, "-cat") || !stricmp(arg, "-catx")) {
			CHECK_NEXT_ARG
			nb_cat++;
			i++;
		}
		else if (!stricmp(arg, "-time")) {
			struct tm time;
			CHECK_NEXT_ARG
			memset(&time, 0, sizeof(struct tm));
			sscanf(argv[i+1], "%d/%d/%d-%d:%d:%d", &time.tm_mday, &time.tm_mon, &time.tm_year, &time.tm_hour, &time.tm_min, &time.tm_sec);
			time.tm_isdst=0;
			time.tm_year -= 1900;
			time.tm_mon -= 1;
			open_edit = 1;
			movie_time = 2082758400;
			movie_time += mktime(&time);
			i++;
		}
		else if (!stricmp(arg, "-force-cat")) force_cat = 1;
		else if (!stricmp(arg, "-align-cat")) align_cat = 1;
		else if (!stricmp(arg, "-unalign-cat")) align_cat = 0;
		else if (!stricmp(arg, "-raw-cat")) {
			CHECK_NEXT_ARG
			raw_cat = argv[i+1];
			i++;
		}
		else if (!stricmp(arg, "-rem") || !stricmp(arg, "-disable") || !stricmp(arg, "-enable")) {
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			if (!stricmp(arg, "-enable")) tracks[nb_track_act].act_type = 6;
			else if (!stricmp(arg, "-disable")) tracks[nb_track_act].act_type = 7;
			else tracks[nb_track_act].act_type = 0;
			tracks[nb_track_act].trackID = atoi(argv[i+1]);
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-par")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			tracks[nb_track_act].act_type = 4;
			assert(strlen(argv[i+1])+1 <= sizeof(szTK));
			strncpy(szTK, argv[i+1], sizeof(szTK));
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track par - expecting ID=PAR_NUM:PAR_DEN got %s\n", argv[i+1]);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			if (!stricmp(ext+1, "none")) {
				tracks[nb_track_act].par_num = tracks[nb_track_act].par_den = -1;
			} else {
				sscanf(ext+1, "%d", &tracks[nb_track_act].par_num);
				ext = strchr(ext+1, ':');
				if (!ext) {
					fprintf(stderr, "Bad format for track par - expecting ID=PAR_NUM:PAR_DEN got %s\n", argv[i+1]);
					MP4BOX_EXIT_WITH_CODE(1);
				}
				sscanf(ext+1, "%d", &tracks[nb_track_act].par_den);
			}
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-lang")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			tracks[nb_track_act].act_type = 1;
			tracks[nb_track_act].lang[3] = 0;
			tracks[nb_track_act].trackID = 0;
			strcpy(szTK, argv[i+1]);
			ext = strchr(szTK, '=');
			if (!strnicmp(argv[i+1], "all=", 4)) {
				strncpy(tracks[nb_track_act].lang, argv[i+1]+1, 3);
			} else if (!ext) {
				strncpy(tracks[nb_track_act].lang, argv[i+1], 3);
			} else {
				strncpy(tracks[nb_track_act].lang, ext+1, 3);
				ext[0] = 0;
				tracks[nb_track_act].trackID = atoi(szTK);
				ext[0] = '=';
			}
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-delay")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			strcpy(szTK, argv[i+1]);
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track delay - expecting ID=DLAY got %s\n", argv[i+1]);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			tracks[nb_track_act].act_type = 2;
			tracks[nb_track_act].delay_ms = atoi(ext+1);
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-ref")) {
			char *szTK, *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			szTK = argv[i+1];
			ext = strchr(szTK, ':');
			if (!ext) {
				fprintf(stderr, "Bad format for track reference - expecting ID:XXXX:refID got %s\n", argv[i+1]);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			tracks[nb_track_act].act_type = 8;
			ext[0] = 0; tracks[nb_track_act].trackID = atoi(szTK); ext[0] = ':'; szTK = ext+1;
			ext = strchr(szTK, ':');
			if (!ext) {
				fprintf(stderr, "Bad format for track reference - expecting ID:XXXX:refID got %s\n", argv[i+1]);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			ext[0] = 0;
			strncpy(tracks[nb_track_act].lang, szTK, 4);
			ext[0] = ':';
			tracks[nb_track_act].delay_ms = (s32) atoi(ext+1);
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-name")) {
			char szTK[GF_MAX_PATH], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			strcpy(szTK, argv[i+1]);
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stderr, "Bad format for track name - expecting ID=name got %s\n", argv[i+1]);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			tracks[nb_track_act].act_type = 5;
			tracks[nb_track_act].hdl_name = strchr(argv[i+1], '=') + 1;
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			ext[0] = '=';
			open_edit = 1;
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
			if (!strcmp(argv[i+1], "auto")) import_fps = GF_IMPORT_AUTO_FPS;
			else if (strchr(argv[i+1], '-')) {
				u32 ticks, dts_inc;
				sscanf(argv[i+1], "%u-%u", &ticks, &dts_inc);
				if (!dts_inc) dts_inc=1;
				import_fps = ticks;
				import_fps /= dts_inc;
			} else import_fps = atof(argv[i+1]);
			i++; 
		}
		else if (!stricmp(arg, "-agg")) { CHECK_NEXT_ARG agg_samples = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-keep-all") || !stricmp(arg, "-keepall")) import_flags |= GF_IMPORT_KEEP_ALL_TRACKS;
#endif /*!defined(GPAC_DISABLE_MEDIA_EXPORT) && !defined(GPAC_DISABLE_MEDIA_IMPORT*/
		else if (!stricmp(arg, "-keep-sys") || !stricmp(arg, "-keepsys")) keep_sys_tracks = 1;
		else if (!stricmp(arg, "-ms")) { CHECK_NEXT_ARG mediaSource = argv[i+1]; i++; }
		else if (!stricmp(arg, "-mp4")) { encode = 1; open_edit = 1; }
		else if (!stricmp(arg, "-saf")) { do_saf = 1; }
		else if (!stricmp(arg, "-log")) { do_log = 1; }
#ifndef GPAC_DISABLE_MPD
		else if (!stricmp(arg, "-mpd")) { 
			do_mpd = 1; 
			CHECK_NEXT_ARG 
			outName = argv[i+1]; 
			i++; 
		}
#endif
		
#ifndef GPAC_DISABLE_SCENE_ENCODER
		else if (!stricmp(arg, "-def")) opts.flags |= GF_SM_ENCODE_USE_NAMES;
		else if (!stricmp(arg, "-sync")) {
			CHECK_NEXT_ARG
			opts.flags |= GF_SM_ENCODE_RAP_INBAND;
			opts.rap_freq = atoi(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-shadow")) {
			CHECK_NEXT_ARG
			opts.flags &= ~GF_SM_ENCODE_RAP_INBAND;
			opts.flags |= GF_SM_ENCODE_RAP_SHADOW;
			opts.rap_freq = atoi(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-carousel")) {
			CHECK_NEXT_ARG
			opts.flags &= ~(GF_SM_ENCODE_RAP_INBAND | GF_SM_ENCODE_RAP_SHADOW);
			opts.rap_freq = atoi(argv[i+1]);
			i++;
		} 
		/*LASeR options*/
		else if (!stricmp(arg, "-resolution")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i+1]);
			i++;
		}
#ifndef GPAC_DISABLE_SCENE_STATS
		else if (!stricmp(arg, "-auto-quant")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i+1]);
			opts.auto_quant = 1;
			i++;
		}
#endif
		else if (!stricmp(arg, "-coord-bits")) {
			CHECK_NEXT_ARG
			opts.coord_bits = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-scale-bits")) {
			CHECK_NEXT_ARG
			opts.scale_bits = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-global-quant")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i+1]);
			opts.auto_quant = 2;
			i++;
		}
		/*chunk encoding*/
		else if (!stricmp(arg, "-ctx-out") || !stricmp(arg, "-outctx")) { CHECK_NEXT_ARG output_ctx = argv[i+1]; i++; }
		else if (!stricmp(arg, "-ctx-in") || !stricmp(arg, "-inctx")) {
			CHECK_NEXT_ARG
			chunk_mode = 1;
			input_ctx = argv[i+1];
			i++;
		}
#endif /*GPAC_DISABLE_SCENE_ENCODER*/
		else if (!strcmp(arg, "-crypt")) {
			CHECK_NEXT_ARG
			crypt = 1;
			drm_file = argv[i+1];
			open_edit = 1;
			i += 1;
		}
		else if (!strcmp(arg, "-decrypt")) {
			CHECK_NEXT_ARG
			crypt = 2;
			if (get_file_type_by_ext(argv[i+1])!=1) {
				drm_file = argv[i+1];
				i += 1;
			}
			open_edit = 1;
		}
		else if (!stricmp(arg, "-set-kms")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			tracks = gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
			memset(&tracks[nb_track_act], 0, sizeof(TrackAction) );

			strncpy(szTK, argv[i+1], 19);
			ext = strchr(szTK, '=');
			tracks[nb_track_act].act_type = 3;
			tracks[nb_track_act].trackID = 0;
			if (!strnicmp(argv[i+1], "all=", 4)) {
				tracks[nb_track_act].kms = argv[i+1] + 4;
			} else if (!ext) {
				tracks[nb_track_act].kms = argv[i+1];
			} else {
				tracks[nb_track_act].kms = ext+1;
				ext[0] = 0;
				tracks[nb_track_act].trackID = atoi(szTK);
				ext[0] = '=';
			}
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-split")) { 
			CHECK_NEXT_ARG 
			split_duration = atof(argv[i+1]);
			if (split_duration<0) split_duration=0;;
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
			split_size = (u32) atoi(argv[i+1]);
			i++; 
			split_duration = 0;
		}
		else if (!stricmp(arg, "-split-chunk") || !stricmp(arg, "-splitx") || !stricmp(arg, "-splitz")) { 
			CHECK_NEXT_ARG 
			if (!strstr(argv[i+1], ":")) {
				fprintf(stderr, "Chunk extraction usage: \"-splitx start:end\" expressed in seconds\n");
				MP4BOX_EXIT_WITH_CODE(1);
			}
			if (strstr(argv[i+1], "end")) {
				sscanf(argv[i+1], "%lf:end", &split_start);
				split_duration = -2; 
			} else {
				sscanf(argv[i+1], "%lf:%lf", &split_start, &split_duration);
				split_duration -= split_start; 
			}
			split_size = 0;
			if (!stricmp(arg, "-splitz")) adjust_split_end = 1;
			i++;
		}
		/*meta*/
		else if (!stricmp(arg, "-set-meta")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 0, argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-add-item")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 1, argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-rem-item")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 2, argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-set-primary")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 3, argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-set-xml")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 4, argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-rem-xml")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			if (parse_meta_args(&metas[nb_meta_act], 6, argv[i+1])) i++;
			nb_meta_act++;
			open_edit = 1;
		}
		else if (!stricmp(arg, "-dump-xml")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 7, argv[i+1]);
			nb_meta_act++;
			i++;
		}
		else if (!stricmp(arg, "-dump-item")) { 
			metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act+1));
			parse_meta_args(&metas[nb_meta_act], 8, argv[i+1]);
			nb_meta_act++;
			i++;
		}
		else if (!stricmp(arg, "-group-add") || !stricmp(arg, "-group-rem-track") || !stricmp(arg, "-group-rem")) { 
			tsel_acts[nb_tsel_acts].act_type = !stricmp(arg, "-group-rem") ? 2 : ( !stricmp(arg, "-group-rem-track") ? 1 : 0 );
			if (parse_tsel_args(&tsel_acts, argv[i+1], &nb_tsel_acts)==0) {
				fprintf(stderr, "Invalid group syntax - check usage\n");
				MP4BOX_EXIT_WITH_CODE(1);
			}
			open_edit=1;
			i++;
		}
		else if (!stricmp(arg, "-group-clean")) { 
			tsel_acts[nb_tsel_acts].act_type = 3;
			nb_tsel_acts++;
			open_edit=1;
		}
 		else if (!stricmp(arg, "-group-single")) {
 		    single_group = 1;
 		}
		else if (!stricmp(arg, "-package")) {
			CHECK_NEXT_ARG 
			pack_file  = argv[i+1];
			i++;
		}
		else if (!stricmp(arg, "-mgt")) {
			CHECK_NEXT_ARG 
			pack_file  = argv[i+1];
			pack_wgt = 1;
			i++;
		}
		
		else if (!stricmp(arg, "-brand")) { 
			char *b = argv[i+1];
			CHECK_NEXT_ARG 
			major_brand = GF_4CC(b[0], b[1], b[2], b[3]);
			open_edit = 1;
			if (b[4]==':') minor_version = atoi(b+5);
			i++;
		}
		else if (!stricmp(arg, "-ab")) { 
			char *b = argv[i+1];
			CHECK_NEXT_ARG 
			brand_add = gf_realloc(brand_add, sizeof(u32) * (nb_alt_brand_add+1));

			brand_add[nb_alt_brand_add] = GF_4CC(b[0], b[1], b[2], b[3]);
			nb_alt_brand_add++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-rb")) { 
			char *b = argv[i+1];
			CHECK_NEXT_ARG 
			brand_rem = gf_realloc(brand_rem, sizeof(u32) * (nb_alt_brand_rem+1));

			brand_rem[nb_alt_brand_rem] = GF_4CC(b[0], b[1], b[2], b[3]);
			nb_alt_brand_rem++;
			open_edit = 1;
			i++;
		}
#endif
		else if (!stricmp(arg, "-languages")) { 
			PrintLanguages();
			MP4BOX_EXIT_WITH_CODE(0);
		}
		else if (!stricmp(arg, "-h")) {
			if (i+1== (u32) argc) PrintUsage();
			else if (!strcmp(argv[i+1], "general")) PrintGeneralUsage();
			else if (!strcmp(argv[i+1], "extract")) PrintExtractUsage();
			else if (!strcmp(argv[i+1], "dash")) PrintDASHUsage();
			else if (!strcmp(argv[i+1], "dump")) PrintDumpUsage();
			else if (!strcmp(argv[i+1], "import")) PrintImportUsage();
			else if (!strcmp(argv[i+1], "format")) PrintFormats();
			else if (!strcmp(argv[i+1], "hint")) PrintHintUsage();
			else if (!strcmp(argv[i+1], "encode")) PrintEncodeUsage();
			else if (!strcmp(argv[i+1], "crypt")) PrintEncryptUsage();
			else if (!strcmp(argv[i+1], "meta")) PrintMetaUsage();
			else if (!strcmp(argv[i+1], "swf")) PrintSWFUsage();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
			else if (!strcmp(argv[i+1], "rtp")) PrintStreamerUsage();
			else if (!strcmp(argv[i+1], "live")) PrintLiveUsage	();
#endif
			else if (!strcmp(argv[i+1], "all")) {
				PrintGeneralUsage();
				PrintExtractUsage();
				PrintDASHUsage();
				PrintDumpUsage();
				PrintImportUsage();
				PrintFormats();
				PrintHintUsage();
				PrintEncodeUsage();
				PrintEncryptUsage();
				PrintMetaUsage();
				PrintSWFUsage();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
				PrintStreamerUsage();
				PrintLiveUsage	();
#endif
			}
			else PrintUsage();
			MP4BOX_EXIT_WITH_CODE(0);
		}
		else if (!stricmp(arg, "-v")) verbose++;
		else if (!stricmp(arg, "-tag-list")) {
			fprintf(stderr, "Supported iTunes tag modifiers:\n");
			for (i=0; i<nb_itunes_tags; i++) {
				fprintf(stderr, "\t%s\t%s\n", itags[i].name, itags[i].comment);
			}
			MP4BOX_EXIT_WITH_CODE(0);
		} else if (!live_scene && !stream_rtp) {
			fprintf(stderr, "Option %s unknown. Please check usage\n", arg);
			MP4BOX_EXIT_WITH_CODE(1);
		}
	}

	if (!inName && dump_std) 
		inName = "std";

	if (!inName) {
		PrintUsage();
		MP4BOX_EXIT_WITH_CODE(1);
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
		else
			interleaving_time = 0.5;
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
			MP4BOX_EXIT_WITH_CODE(1);
		}
	}

#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
	if (live_scene) {
		int ret = live_session(argc, argv);
		MP4BOX_EXIT_WITH_CODE(ret);
	}
	if (stream_rtp) {
		int ret = stream_file_rtp(argc, argv);
		MP4BOX_EXIT_WITH_CODE(ret);
	}
#endif

	if (raw_cat) {
		char chunk[4096];
		FILE *fin, *fout;
		s64 to_copy, done;
		fin = gf_f64_open(raw_cat, "rb");
		if (!fin) MP4BOX_EXIT_WITH_CODE(1);
		fout = gf_f64_open(inName, "a+b");
		if (!fout) {
			fclose(fin);
			MP4BOX_EXIT_WITH_CODE(1);
		}
		gf_f64_seek(fin, 0, SEEK_END);
		to_copy = gf_f64_tell(fin);
		gf_f64_seek(fin, 0, SEEK_SET);
		done = 0;
		while (1) {
			u32 nb_bytes = (u32) fread(chunk, 1, 4096, fin);
			gf_fwrite(chunk, 1, nb_bytes, fout);
			done += nb_bytes;
			fprintf(stderr, "Appending file %s - %02.2f done\r", raw_cat, 100.0*done/to_copy);
			if (done >= to_copy) break;
		}
		fclose(fin);
		fclose(fout);
		MP4BOX_EXIT_WITH_CODE(0);
	}
#if !defined(GPAC_DISABLE_STREAMING) 
	if (grab_m2ts) {
		return grab_live_m2ts(grab_m2ts, inName);
	}
#endif
	/*init libgpac*/
	if (enable_mem_tracker) {
		gf_sys_close();
		gf_sys_init(enable_mem_tracker);
	}

	if (gf_logs) {
		//gf_log_set_tools_levels(gf_logs);
	} else {
		u32 level = verbose ? GF_LOG_DEBUG : GF_LOG_INFO;
		gf_log_set_tool_level(GF_LOG_CONTAINER, level);
		gf_log_set_tool_level(GF_LOG_SCENE, level);
		gf_log_set_tool_level(GF_LOG_PARSER, level);
		gf_log_set_tool_level(GF_LOG_AUTHOR, level);
		gf_log_set_tool_level(GF_LOG_CODING, level);
#ifdef GPAC_MEMORY_TRACKING
		if (enable_mem_tracker)
			gf_log_set_tool_level(GF_LOG_MEMORY, level);
#endif
		if (quiet) {
			if (quiet==2) gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_QUIET);
			gf_set_progress_callback(NULL, progress_quiet);

		}
	}

#if !defined(DISABLE_CORE_TOOLS)
	if (do_wget != NULL) {
		e = gf_dm_wget(do_wget, inName, 0, 0);
		if (e != GF_OK) {
			fprintf(stderr, "Cannot retrieve %s: %s\n", do_wget, gf_error_to_string(e) );
		}
		MP4BOX_EXIT_WITH_CODE(1);
	}
#endif
    
#ifndef GPAC_DISABLE_MPD
	if (do_mpd) {
		Bool remote = 0;
		char *mpd_base_url = gf_strdup(inName);
		if (!strnicmp(inName, "http://", 7)) {
#if !defined(GPAC_DISABLE_CORE_TOOLS)
			e = gf_dm_wget(inName, "tmp_main.m3u8", 0, 0);
			if (e != GF_OK) {
				fprintf(stderr, "Cannot retrieve M3U8 (%s): %s\n", inName, gf_error_to_string(e));
				gf_free(mpd_base_url);
				MP4BOX_EXIT_WITH_CODE(1);
			}
			inName = "tmp_main.m3u8";
			remote = 1;
#else
			fprintf(stderr, "HTTP Downloader disabled in this build\n");
            MP4BOX_EXIT_WITH_CODE(1);
#endif
		}
        e = gf_m3u8_to_mpd(inName, mpd_base_url, (outName ? outName : inName), 0, "video/mp2t", 1, use_url_template, NULL);
		gf_free(mpd_base_url);
        
		if (remote) {
			//gf_delete_file("tmp_main.m3u8");
		}
		if (e != GF_OK) {
			fprintf(stderr, "Error converting M3U8 (%s) to MPD (%s): %s\n", inName, outName, gf_error_to_string(e));
			MP4BOX_EXIT_WITH_CODE(1);
		} else {
			fprintf(stderr, "Done converting M3U8 (%s) to MPD (%s)\n", inName, outName);
			MP4BOX_EXIT_WITH_CODE(0);
		}
	}
#endif
	if (dash_duration && !nb_dash_inputs) {
		dash_inputs = set_dash_input(dash_inputs, inName, &nb_dash_inputs);
	}


	if (do_saf && !encode) {
		switch (get_file_type_by_ext(inName)) {
		case 2: case 3: case 4: 
			encode = 1;
			break;
		}
	}

#ifndef GPAC_DISABLE_SCENE_DUMP
	if (dump_mode == 1 + GF_SM_DUMP_SVG) {
		if (strstr(inName, ".srt") || strstr(inName, ".ttxt")) import_subtitle = 2;
	}
#endif


	if (import_subtitle && !trackID) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		GF_MediaImporter import;
		file = gf_isom_open("ttxt_convert", GF_ISOM_OPEN_WRITE, NULL);
		memset(&import, 0, sizeof(GF_MediaImporter));
		import.dest = file;
		import.in_name = inName;
		e = gf_media_import(&import);
		if (e) {
			fprintf(stderr, "Error importing %s: %s\n", inName, gf_error_to_string(e));
			gf_isom_delete(file);
			gf_delete_file("ttxt_convert");
			MP4BOX_EXIT_WITH_CODE(1);
		}
		strcpy(outfile, outName ? outName : inName);
		if (strchr(outfile, '.')) {
			while (outfile[strlen(outfile)-1] != '.') outfile[strlen(outfile)-1] = 0;
			outfile[strlen(outfile)-1] = 0;
		}
#ifndef GPAC_DISABLE_ISOM_DUMP
		dump_timed_text_track(file, gf_isom_get_track_id(file, 1), dump_std ? NULL : outfile, 1, (import_subtitle==2) ? 2 : dump_srt);
#endif
		gf_isom_delete(file);
		gf_delete_file("ttxt_convert");
		if (e) {
			fprintf(stderr, "Error converting %s: %s\n", inName, gf_error_to_string(e));
			MP4BOX_EXIT_WITH_CODE(1);
		}
		MP4BOX_EXIT_WITH_CODE(0);
#else
		fprintf(stderr, "Feature not supported\n");
		MP4BOX_EXIT_WITH_CODE(1);
#endif
	}

#if !defined(GPAC_DISABLE_MEDIA_IMPORT) && !defined(GPAC_DISABLE_ISOM_WRITE)
	if (nb_add) {
		u8 open_mode = GF_ISOM_OPEN_EDIT;
		if (force_new) {
			open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
		} else {
			FILE *test = gf_f64_open(inName, "rb");
			if (!test) {
				open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
				if (!outName) outName = inName;
			} else {
				fclose(test);
				if (! gf_isom_probe_file(inName) ) {
					open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
					if (!outName) outName = inName;
				}
			}
		}

		open_edit = 1;
		file = gf_isom_open(inName, open_mode, tmpdir);
		if (!file) {
			fprintf(stderr, "Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) );
			MP4BOX_EXIT_WITH_CODE(1);
		}
		for (i=0; i<(u32) argc; i++) {
			if (!strcmp(argv[i], "-add")) {
				char *src = argv[i+1];

				e = import_file(file, src, import_flags, import_fps, agg_samples);
				if (e) {
					while (src) {
						char *sep = strchr(src, '+');
						if (sep) sep[0] = 0;

						e = import_file(file, src, import_flags, import_fps, agg_samples);

						if (sep) {
							sep[0] = '+';
							src = sep+1;
						} else {
							src= NULL;
						}
						if (e)
							break;
					}
					if (e) {
						fprintf(stderr, "Error importing %s: %s\n", argv[i+1], gf_error_to_string(e));
						gf_isom_delete(file);
						MP4BOX_EXIT_WITH_CODE(1);
					}
				}
				i++;
			}
		}

		/*unless explicitly asked, remove all systems tracks*/
		if (!keep_sys_tracks) remove_systems_tracks(file);
		needSave = 1;
		/*JLF commented: if you want ISMA, just ask for it, no more auto-detect*/
//		if (!conv_type && can_convert_to_isma(file)) conv_type = GF_ISOM_CONV_TYPE_ISMA;
	}

	if (nb_cat) {
		if (!file) {
			u8 open_mode = GF_ISOM_OPEN_EDIT;
			if (force_new) {
				open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
			} else {
				FILE *test = gf_f64_open(inName, "rb");
				if (!test) {
					open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
					if (!outName) outName = inName;
				}
				else fclose(test);
			}

			open_edit = 1;
			file = gf_isom_open(inName, open_mode, tmpdir);
			if (!file) {
				fprintf(stderr, "Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) );
				MP4BOX_EXIT_WITH_CODE(1);
			}
		}
		for (i=0; i<(u32)argc; i++) {
			if (!strcmp(argv[i], "-cat") || !strcmp(argv[i], "-catx")) {
				e = cat_isomedia_file(file, argv[i+1], import_flags, import_fps, agg_samples, tmpdir, force_cat, align_cat, !strcmp(argv[i], "-catx") ? GF_TRUE : GF_FALSE);
				if (e) {
					fprintf(stderr, "Error appending %s: %s\n", argv[i+1], gf_error_to_string(e));
					gf_isom_delete(file);
					MP4BOX_EXIT_WITH_CODE(1);
				}
				i++;
			}
		}
		/*unless explicitly asked, remove all systems tracks*/
		if (!keep_sys_tracks) remove_systems_tracks(file);

		needSave = 1;
		if (conv_type && can_convert_to_isma(file)) conv_type = GF_ISOM_CONV_TYPE_ISMA;
	}
#endif

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	else if (chunk_mode) {
		if (!inName) {
			fprintf(stderr, "chunk encoding syntax: [-outctx outDump] -inctx inScene auFile\n");
			MP4BOX_EXIT_WITH_CODE(1);
		}
		e = EncodeFileChunk(inName, outName ? outName : inName, input_ctx, output_ctx, tmpdir);
		if (e) fprintf(stderr, "Error encoding chunk file %s\n", gf_error_to_string(e));
		MP4BOX_EXIT_WITH_CODE( e ? 1 : 0 );
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
			logs = gf_f64_open(logfile, "wt");
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
		if (logs) fclose(logs);
		if (e) goto err_exit;
		needSave = 1;
		if (do_saf) {
			needSave = 0;
			open_edit = 0;
		}
#endif
	} 

#ifndef GPAC_DISABLE_ISOM_WRITE
	else if (pack_file) {
		char *fileName = strchr(pack_file, ':');
		if (fileName && ((fileName - pack_file)==4)) {
			fileName[0] = 0;
			file = package_file(fileName + 1, pack_file, tmpdir, pack_wgt);
			fileName[0] = ':';
		} else {
			file = package_file(pack_file, NULL, tmpdir, pack_wgt);
		}
		if (!outName) outName = inName;
		needSave = 1;
		open_edit = 1;
	} 
#endif

	if (dash_duration) {
		Bool del_file = GF_FALSE;
		char szMPD[GF_MAX_PATH], *sep;
		GF_Config *dash_ctx = NULL;
		u32 do_abort = 0;
		gf_log_set_tool_level(GF_LOG_DASH, GF_LOG_INFO);
		strcpy(outfile, outName ? outName : gf_url_get_resource_name(inName) );
		sep = strrchr(outfile, '.');
		if (sep) sep[0] = 0;
		if (!outName) strcat(outfile, "_dash");
		strcpy(szMPD, outfile);
		strcat(szMPD, ".mpd");

		if (dash_dynamic)
			fprintf(stderr, "Live DASH-ing - press 'q' to quit, 's' to save context and quit\n");

		if (!dash_ctx_file && dash_live) {
			dash_ctx = gf_cfg_new(NULL, NULL);
		} else if (dash_ctx_file) {
			if (force_new) 
				gf_delete_file(dash_ctx_file);

			dash_ctx = gf_cfg_force_new(NULL, dash_ctx_file);
		}

		if (dash_profile==GF_DASH_PROFILE_UNKNOWN)
			dash_profile = dash_dynamic ? GF_DASH_PROFILE_LIVE : GF_DASH_PROFILE_FULL;

		if (!dash_dynamic) {
			time_shift_depth = 0;
			mpd_update_time = 0;
		} else if ((dash_profile>=GF_DASH_PROFILE_MAIN) && !use_url_template && !mpd_update_time) {
			/*use a default MPD update of dash_duration sec*/
			mpd_update_time = (u32)  (dash_subduration ? dash_subduration : dash_duration);
			fprintf(stderr, "Using default MPD refresh of %d seconds\n", mpd_update_time);
		}

		if (file && needSave) {
			gf_isom_close(file);
			file = NULL;
			del_file = GF_TRUE;
		}
		while (!do_abort) {

			e = gf_dasher_segment_files(szMPD, dash_inputs, nb_dash_inputs, dash_profile, dash_title, dash_source, cprt, dash_more_info,
										(const char **) mpd_base_urls, nb_mpd_base_urls, 
									   use_url_template, segment_timeline, single_segment, single_file, bitstream_switching_mode,
									   seg_at_rap, dash_duration, seg_name, seg_ext, segment_marker,
									   interleaving_time, subsegs_per_sidx, daisy_chain_sidx, frag_at_rap, tmpdir,
									   dash_ctx, dash_dynamic, mpd_update_time, time_shift_depth, dash_subduration, min_buffer, 
									   ast_shift_sec, dash_scale, memory_frags, initial_moof_sn, initial_tfdt, no_fragments_defaults, pssh_in_moof );
			if (e) break;

			if (dash_live) {
				u32 sleep_for = gf_dasher_next_update_time(dash_ctx, mpd_update_time);
				fprintf(stderr, "sleep for %d ms\n", sleep_for);
				while (1) {
					if (gf_prompt_has_input()) {
						char c = (char) gf_prompt_get_char(); 
						if (c=='q') { do_abort = 1; break; }
						if (c=='s') { do_abort = 2; break; }
					}

					if (dash_dynamic == 2) {
						break;
					}
					if (sleep_for<100) 
						break;
					gf_sleep(100);

					sleep_for = gf_dasher_next_update_time(dash_ctx, mpd_update_time);
				}
			} else {
				break;
			}
		}
		
		if (dash_ctx) {
			if (do_abort==2) {
				char szName[1024];
				fprintf(stderr, "Enter file name to save dash context:\n");
				if (scanf("%s", szName) == 1) {
					gf_cfg_set_filename(dash_ctx, szName);
					gf_cfg_save(dash_ctx);
				}
			}
			gf_cfg_del(dash_ctx);
		}
		if (e) fprintf(stderr, "Error DASHing file: %s\n", gf_error_to_string(e));
		if (file) gf_isom_delete(file);
		if (del_file) 
			gf_delete_file(inName);
	
		MP4BOX_EXIT_WITH_CODE( (e!=GF_OK) ? 1 : 0 );
	}

	else if (!file
#ifndef GPAC_DISABLE_MEDIA_EXPORT
		&& !(track_dump_type & GF_EXPORT_AVI_NATIVE)
#endif
		) {
		FILE *st = gf_f64_open(inName, "rb");
		Bool file_exists = 0;
		if (st) {
			file_exists = 1;
			fclose(st);
		}
		switch (get_file_type_by_ext(inName)) {
		case 1:
			file = gf_isom_open(inName, (u8) (open_edit ? GF_ISOM_OPEN_EDIT : ( ((dump_isom>0) || print_info) ? GF_ISOM_OPEN_READ_DUMP : GF_ISOM_OPEN_READ) ), tmpdir);
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
					MP4BOX_EXIT_WITH_CODE(1);
				}
			}
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
			if ((dump_mode==1+GF_SM_DUMP_LASER) || (dump_mode==1+GF_SM_DUMP_SVG)) {
				break;
			}
#endif

		default:
			if (!open_edit && file_exists && !gf_isom_probe_file(inName) && track_dump_type) {
			} 
#ifndef GPAC_DISABLE_ISOM_WRITE
			else if (!open_edit && file_exists /* && !gf_isom_probe_file(inName) */ && !dump_mode) {
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
						MP4BOX_EXIT_WITH_CODE(1);
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
#ifndef GPAC_DISABLE_MEDIA_IMPORT
				} else {
					convert_file_info(inName, info_track_id);
#endif
				}
				MP4BOX_EXIT_WITH_CODE(0);
			}
#endif /*GPAC_DISABLE_ISOM_WRITE*/
			else if (open_edit) {
				file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, tmpdir);
				if (!outName && file) outName = inName;
			} else if (!file_exists) {
				fprintf(stderr, "Error creating file %s: %s\n", inName, gf_error_to_string(GF_URL_ERROR));
				MP4BOX_EXIT_WITH_CODE(1);
			} else {
				fprintf(stderr, "Cannot open %s - extension not supported\n", inName);
				MP4BOX_EXIT_WITH_CODE(1);
			}
		}
	}

	if (file && keep_utc && open_edit) {
		gf_isom_keep_utc_times(file, 1);
	}

	strcpy(outfile, outName ? outName : inName);
	if (strrchr(outfile, '.')) {
		char *szExt = strrchr(outfile, '.');

		/*turn on 3GP saving*/
		if (!stricmp(szExt, ".3gp") || !stricmp(szExt, ".3gpp") || !stricmp(szExt, ".3g2")) 
			conv_type = GF_ISOM_CONV_TYPE_3GPP;
		else if (!stricmp(szExt, ".m4a") || !stricmp(szExt, ".m4v"))
			conv_type = GF_ISOM_CONV_TYPE_IPOD;
		else if (!stricmp(szExt, ".psp"))
			conv_type = GF_ISOM_CONV_TYPE_PSP;
		
		while (outfile[strlen(outfile)-1] != '.') outfile[strlen(outfile)-1] = 0;
		outfile[strlen(outfile)-1] = 0;
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

		e = gf_media_export(&mdump);
		if (e) goto err_exit;
		MP4BOX_EXIT_WITH_CODE(0);
	}
	if (!open_edit && !gf_isom_probe_file(inName) && track_dump_type) {
		GF_MediaExporter mdump;
		char szFile[1024];
		for (i=0; i<nb_track_act; i++) {
			TrackAction *tka = &tracks[i];
			if (tka->act_type != 9) continue;
			memset(&mdump, 0, sizeof(mdump));
			mdump.in_name = inName;
			mdump.flags = tka->dump_type;
			mdump.trackID = tka->trackID;
			mdump.sample_num = raw_sample_num;
			if (outName) {
				mdump.out_name = outName;
				mdump.flags |= GF_EXPORT_MERGE;
			} else if (nb_track_act>1) {
				sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
				mdump.out_name = szFile;
			} else {
				mdump.out_name = outfile;
			}
			e = gf_media_export(&mdump);
			if (e) goto err_exit;
		}
		MP4BOX_EXIT_WITH_CODE(0);
	} 

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/

#ifndef GPAC_DISABLE_SCENE_DUMP
	if (dump_mode) {
		e = dump_file_text(inName, dump_std ? NULL : outfile, dump_mode-1, do_log);
		if (e) goto err_exit;
	}
#endif

#ifndef GPAC_DISABLE_SCENE_STATS
	if (stat_level) dump_scene_stats(inName, dump_std ? NULL : outfile, stat_level);
#endif

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (!HintIt && print_sdp) DumpSDP(file, dump_std ? NULL : outfile);
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
	if (dump_isom) dump_isom_xml(file, dump_std ? NULL : outfile);
	if (dump_cr) dump_file_ismacryp(file, dump_std ? NULL : outfile);
	if ((dump_ttxt || dump_srt) && trackID) dump_timed_text_track(file, trackID, dump_std ? NULL : outfile, 0, dump_srt);
#ifndef GPAC_DISABLE_ISOM_HINTING
	if (dump_rtp) dump_file_rtp(file, dump_std ? NULL : outfile);
#endif
#endif

	if (dump_timestamps) dump_file_timestamps(file, dump_std ? NULL : outfile);
	if (dump_nal) dump_file_nal(file, dump_nal, dump_std ? NULL : outfile);
	
	if (do_hash) {
		u8 hash[20];
		e = gf_media_get_file_hash(inName, hash);
		if (e) goto err_exit;
		fprintf(stderr, "File %s hash (SHA-1): ", inName);
		for (i=0; i<20; i++) fprintf(stderr, "%02X", hash[i]);
		fprintf(stderr, "\n");
	}
	if (dump_cart) dump_cover_art(file, outfile);
	if (dump_chap) dump_chapters(file, outfile, (dump_chap==2) ? 1 : 0);

	if (dump_iod) {
		GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *)gf_isom_get_root_od(file);
		if (!iod) {
			fprintf(stderr, "File %s has no IOD", inName);
		} else {
			char szName[GF_MAX_PATH];
			FILE *iodf;
			GF_BitStream *bs = NULL;

			sprintf(szName, "%s.iod", outfile);
			iodf = gf_f64_open(szName, "wb");
			if (!iodf) {
				fprintf(stderr, "Cannot open destination %s\n", szName);
			} else {
				char *desc;
				u32 size;
				bs = gf_bs_from_file(iodf, GF_BITSTREAM_WRITE);
				if (gf_odf_desc_write((GF_Descriptor *)iod, &desc, &size)==GF_OK) {
					gf_fwrite(desc, 1, size, iodf);
					gf_free(desc);
				} else {
					fprintf(stderr, "Error writing IOD %s\n", szName);
				}
				fclose(iodf);
			}
                        gf_free(bs);
		}
	}

#if !(definedGPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	if (split_duration || split_size) {
		split_isomedia_file(file, split_duration, split_size, inName, interleaving_time, split_start, adjust_split_end, outName, tmpdir);
		/*never save file when splitting is desired*/
		open_edit = 0;
		needSave = 0;
	}
#endif

#ifndef GPAC_DISABLE_MEDIA_EXPORT
	if (track_dump_type) {
		char szFile[1024];
		GF_MediaExporter mdump;
		for (i=0; i<nb_track_act; i++) {
			TrackAction *tka = &tracks[i];
			if (tka->act_type != 9) continue;
			memset(&mdump, 0, sizeof(mdump));
			mdump.file = file;
			mdump.flags = tka->dump_type;
			mdump.trackID = tka->trackID;
			mdump.sample_num = raw_sample_num;
			if (tka->out_name) {
				mdump.out_name = tka->out_name;
			} else if (outName) {
				mdump.out_name = outName;
				mdump.flags |= GF_EXPORT_MERGE;
			} else {
				sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
				mdump.out_name = szFile;
			}
			if (tka->trackID==(u32) -1) {
				u32 j;
				for (j=0; j<gf_isom_get_track_count(file); j++) {
					mdump.trackID = gf_isom_get_track_id(file, j+1);
					sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
					mdump.out_name = szFile;
					e = gf_media_export(&mdump);
					if (e) goto err_exit;
				}
			} else {
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
		case 0:
			/*note: we don't handle file brand modification, this is an author stuff and cannot be guessed from meta type*/
			e = gf_isom_set_meta_type(file, meta->root_meta, tk, meta->meta_4cc);
			gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_ISO2, 1);
			needSave = 1;
			break;
		case 1:
			self_ref = !stricmp(meta->szPath, "NULL") || !stricmp(meta->szPath, "this") || !stricmp(meta->szPath, "self");
			e = gf_isom_add_meta_item(file, meta->root_meta, tk, self_ref, self_ref ? NULL : meta->szPath, 
					strlen(meta->szName) ? meta->szName : NULL,  
					meta->item_id,
					strlen(meta->mime_type) ? meta->mime_type : NULL,  
					strlen(meta->enc_type) ? meta->enc_type : NULL,  
					meta->use_dref ? meta->szPath : NULL,  NULL);
			needSave = 1;
			break;
		case 2:
			e = gf_isom_remove_meta_item(file, meta->root_meta, tk, meta->item_id);
			needSave = 1;
			break;
		case 3:
			e = gf_isom_set_meta_primary_item(file, meta->root_meta, tk, meta->item_id);
			needSave = 1;
			break;
		case 4:
		case 5:
			e = gf_isom_set_meta_xml(file, meta->root_meta, tk, meta->szPath, (meta->act_type==5) ? 1 : 0);
			needSave = 1;
			break;
		case 6:
			if (gf_isom_get_meta_item_count(file, meta->root_meta, tk)) {
				e = gf_isom_remove_meta_xml(file, meta->root_meta, tk);
				needSave = 1;
			} else {
				fprintf(stderr, "No meta box in input file\n");
			}
			break;
		case 8:
			if (gf_isom_get_meta_item_count(file, meta->root_meta, tk)) {
				e = gf_isom_extract_meta_item(file, meta->root_meta, tk, meta->item_id, strlen(meta->szPath) ? meta->szPath : NULL);
			} else {
				fprintf(stderr, "No meta box in input file\n");
			}
			break;
#endif
		case 7:
			if (gf_isom_has_meta_xml(file, meta->root_meta, tk)) {
				e = gf_isom_extract_meta_xml(file, meta->root_meta, tk, meta->szPath, NULL);
			} else {
				fprintf(stderr, "No meta box in input file\n");
			}
			break;
		}
		if (e) goto err_exit;
	}
	if (!open_edit && !needSave) {
		if (file) gf_isom_delete(file);
		MP4BOX_EXIT_WITH_CODE(0);
	}

#ifndef GPAC_DISABLE_ISOM_WRITE
	for (i=0; i<nb_tsel_acts; i++) {
		switch (tsel_acts[i].act_type) {
		case 0:
			e = gf_isom_set_track_switch_parameter(file, 
				gf_isom_get_track_by_id(file, tsel_acts[i].trackID),
				tsel_acts[i].refTrackID ? gf_isom_get_track_by_id(file, tsel_acts[i].refTrackID) : 0,
				tsel_acts[i].is_switchGroup ? 1 : 0,
				&tsel_acts[i].switchGroupID,
				tsel_acts[i].criteria, tsel_acts[i].nb_criteria);
			if (e) goto err_exit;
			needSave = 1;
			break;
		case 1:
			e = gf_isom_reset_track_switch_parameter(file, gf_isom_get_track_by_id(file, tsel_acts[i].trackID), 0);
			if (e) goto err_exit;
			needSave = 1;
			break;
		case 2:
			e = gf_isom_reset_track_switch_parameter(file, gf_isom_get_track_by_id(file, tsel_acts[i].trackID), 1);
			if (e) goto err_exit;
			needSave = 1;
			break;
		case 3:
			e = gf_isom_reset_switch_parameters(file);
			if (e) goto err_exit;
			needSave = 1;
			break;
		}
	}

	if (remove_sys_tracks) {
#ifndef GPAC_DISABLE_AV_PARSERS
		remove_systems_tracks(file);
#endif
		needSave = 1;
		if (conv_type < GF_ISOM_CONV_TYPE_ISMA_EX) conv_type = 0;
	}
	if (remove_root_od) {
		gf_isom_remove_root_od(file);
		needSave = 1;
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
		needSave = 1;
	}
#endif


	if (!encode) {
		if (!file) {
			fprintf(stderr, "Nothing to do - exiting\n");
			MP4BOX_EXIT_WITH_CODE(0);
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
			e = gf_media_make_isma(file, crypt ? 1 : 0, 0, (conv_type==GF_ISOM_CONV_TYPE_ISMA_EX) ? 1 : 0);
			if (e) goto err_exit;
			needSave = 1;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_3GPP) {
			fprintf(stderr, "Converting to 3GP file...\n");
			e = gf_media_make_3gpp(file);
			if (e) goto err_exit;
			needSave = 1;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_PSP) {
			fprintf(stderr, "Converting to PSP file...\n");
			e = gf_media_make_psp(file);
			if (e) goto err_exit;
			needSave = 1;
		}
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/
		if (conv_type == GF_ISOM_CONV_TYPE_IPOD) {
			u32 major_brand = 0;

			fprintf(stderr, "Setting up iTunes/iPod file...\n");

			for (i=0; i<gf_isom_get_track_count(file); i++) {
				u32 mType = gf_isom_get_media_type(file, i+1);
				switch (mType) {
				case GF_ISOM_MEDIA_VISUAL:
					major_brand = GF_4CC('M','4','V',' ');
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
					if (!major_brand) major_brand = GF_4CC('M','4','A',' ');
					else gf_isom_modify_alternate_brand(file, GF_4CC('M','4','A',' '), 1);
					break;
				case GF_ISOM_MEDIA_TEXT:
					/*this is a text track track*/
					if (gf_isom_get_media_subtype(file, i+1, 1) == GF_4CC('t','x','3','g')) {
						u32 j;
						Bool is_chap = 0;
						for (j=0; j<gf_isom_get_track_count(file); j++) {
							s32 count = gf_isom_get_reference_count(file, j+1, GF_4CC('c','h','a','p'));
							if (count>0) {
								u32 tk, k;
								for (k=0; k<(u32) count; k++) {
									gf_isom_get_reference(file, j+1, GF_4CC('c','h','a','p'), k+1, &tk);
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
			needSave = 1;
		}

#ifndef GPAC_DISABLE_MCRYPT
		if (crypt) {
			if (!drm_file) {
				fprintf(stderr, "Missing DRM file location - usage '-%s drm_file input_file\n", (crypt==1) ? "crypt" : "decrypt");
				e = GF_BAD_PARAM;
				goto err_exit;
			}
			if (crypt == 1) {
				e = gf_crypt_file(file, drm_file);
			} else if (crypt ==2) {
				e = gf_decrypt_file(file, drm_file);
			}
			if (e) goto err_exit;
			needSave = 1;
		}
#endif /*GPAC_DISABLE_MCRYPT*/
	} else if (outName) {
		strcpy(outfile, outName);
	}


	for (i=0; i<nb_track_act; i++) {
		TrackAction *tka = &tracks[i];
		u32 track = tka->trackID ? gf_isom_get_track_by_id(file, tka->trackID) : 0;
		u32 timescale = gf_isom_get_timescale(file);
		switch (tka->act_type) {
		case 0:
			e = gf_isom_remove_track(file, track);
			if (e) {
				fprintf(stderr, "Error Removing track ID %d: %s\n", tka->trackID, gf_error_to_string(e));
			} else {
				fprintf(stderr, "Removing track ID %d\n", tka->trackID);
			}
			needSave = 1;
			break;
		case 1:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_set_media_language(file, i+1, (char *) GetLanguageCode(tka->lang));
				if (e) goto err_exit;
				needSave = 1;
			}
			needSave = 1;
			break;
		case 2:
			if (tka->delay_ms) {
				u64 tk_dur;

				gf_isom_remove_edit_segments(file, track);
				tk_dur = gf_isom_get_track_duration(file, track);
				if (gf_isom_get_edit_segment_count(file, track))
					needSave = 1;
				if (tka->delay_ms>0) {
					gf_isom_append_edit_segment(file, track, (timescale*tka->delay_ms)/1000, 0, GF_ISOM_EDIT_EMPTY);
					gf_isom_append_edit_segment(file, track, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
					needSave = 1;
				} else {
					u64 to_skip = (timescale*(-tka->delay_ms))/1000;
					if (to_skip<tk_dur) {
						u64 media_time = (-tka->delay_ms)*gf_isom_get_media_timescale(file, track) / 1000;
						gf_isom_append_edit_segment(file, track, tk_dur-to_skip, media_time, GF_ISOM_EDIT_NORMAL);
						needSave = 1;
					} else {
						fprintf(stderr, "Warning: request negative delay longer than track duration - ignoring\n");
					}
				}
			} else if (gf_isom_get_edit_segment_count(file, track)) {
				gf_isom_remove_edit_segments(file, track);
				needSave = 1;
			}
			break;
		case 3:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				if (!gf_isom_is_media_encrypted(file, i+1, 1)) continue;
				if (!gf_isom_is_ismacryp_media(file, i+1, 1)) continue;
				e = gf_isom_change_ismacryp_protection(file, i+1, 1, NULL, (char *) tka->kms);
				if (e) goto err_exit;
				needSave = 1;
			}
			break;
		case 4:
			e = gf_media_change_par(file, track, tka->par_num, tka->par_den);
			needSave = 1;
			break;
		case 5:
			e = gf_isom_set_handler_name(file, track, tka->hdl_name);
			needSave = 1;
			break;
		case 6:
			if (!gf_isom_is_track_enabled(file, track)) {
				e = gf_isom_set_track_enabled(file, track, 1);
				needSave = 1;
			}
			break;
		case 7:
			if (gf_isom_is_track_enabled(file, track)) {
				e = gf_isom_set_track_enabled(file, track, 0);
				needSave = 1;
			}
			break;
		case 8:
			e = gf_isom_set_track_reference(file, track, GF_4CC(tka->lang[0], tka->lang[1], tka->lang[2], tka->lang[3]), (u32) tka->delay_ms);
			needSave = 1;
			break;
		}
		if (e) goto err_exit;
	}

	if (itunes_tags) {
		char *tags = itunes_tags;

		while (tags) {
			char *val;
			char *sep = strchr(tags, ':');
			u32 tlen, itag = 0;
			if (sep) {
				while (sep) {
					for (itag=0; itag<nb_itunes_tags;itag++) {
						if (!strnicmp(sep+1, itags[itag].name, strlen(itags[itag].name))) break;
					}
					if (itag<nb_itunes_tags) {
						break;
					}
					sep = strchr(sep+1, ':');
				}
				if (sep) sep[0] = 0;
			} 
			for (itag=0; itag<nb_itunes_tags;itag++) {
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
				char *d, *ext;
				FILE *t = gf_f64_open(val, "rb");
				gf_f64_seek(t, 0, SEEK_END);
				tlen = (u32) gf_f64_tell(t);
				gf_f64_seek(t, 0, SEEK_SET);
				d = gf_malloc(sizeof(char) * tlen);
				tlen = (u32) fread(d, sizeof(char), tlen, t);
				fclose(t);
				
				ext = strrchr(val, '.');
				if (!stricmp(ext, ".png")) tlen |= 0x80000000;
				e = gf_isom_apple_set_tag(file, GF_ISOM_ITUNE_COVER_ART, d, tlen);
				gf_free(d);
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
					gf_isom_apple_set_tag(file, itag, val, (u32) strlen(val) );
				}
			}
				break;
			case GF_ISOM_ITUNE_DISK:
			case GF_ISOM_ITUNE_TRACKNUMBER:
			{
				u32 n, t;
				char _t[8];
				n = t = 0;
				memset(_t, 0, sizeof(char)*8);
				tlen = (itag==GF_ISOM_ITUNE_DISK) ? 6 : 8;
				if (sscanf(val, "%u/%u", &n, &t) == 2) { _t[3]=n; _t[2]=n>>8; _t[5]=t; _t[4]=t>>8; }
				else if (sscanf(val, "%u", &n) == 1) { _t[3]=n; _t[2]=n>>8;}
				else tlen = 0;
				if (tlen) gf_isom_apple_set_tag(file, itag, _t, tlen);
			}
				break;
			case GF_ISOM_ITUNE_GAPLESS:
			case GF_ISOM_ITUNE_COMPILATION:
			{
				char _t[1];
				if (!stricmp(val, "yes")) _t[0] = 1;
				else  _t[0] = 0;
				gf_isom_apple_set_tag(file, itag, _t, 1);
			}
				break;
			default:
				gf_isom_apple_set_tag(file, itag, val, tlen);
				break;
			}
			needSave = 1;

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
		needSave = 1;
	}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (Frag) {
		if (!interleaving_time) interleaving_time = 0.5;
		if (HintIt) fprintf(stderr, "Warning: cannot hint and fragment - ignoring hint\n");
		fprintf(stderr, "Fragmenting file (%.3f seconds fragments)\n", interleaving_time);
		e = gf_media_fragment_file(file, outfile, interleaving_time);
		if (e) fprintf(stderr, "Error while fragmenting file: %s\n", gf_error_to_string(e));
		gf_isom_delete(file);
		if (!e && !outName && !force_new) {
			if (gf_delete_file(inName)) fprintf(stderr, "Error removing file %s\n", inName);
			else if (gf_move_file(outfile, inName)) fprintf(stderr, "Error renaming file %s to %s\n", outfile, inName);
		}
		MP4BOX_EXIT_WITH_CODE( (e!=GF_OK) ? 1 : 0 );
	}
#endif

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (HintIt) {
		if (force_ocr) SetupClockReferences(file);
		fprintf(stderr, "Hinting file with Path-MTU %d Bytes\n", MTUSize);
		MTUSize -= 12;		
		e = HintFile(file, MTUSize, max_ptime, rtp_rate, hint_flags, HintCopy, HintInter, regular_iod, single_group);
		if (e) goto err_exit;
		needSave = 1;
		if (print_sdp) DumpSDP(file, dump_std ? NULL : outfile);
	}
#endif

	/*full interleave (sample-based) if just hinted*/
	if (FullInter) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_TIGHT);
	} else if (!interleaving_time) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_STREAMABLE);
		needSave = 1;
	} else if (do_flat) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_FLAT);
		needSave = 1;
	} else {
		e = gf_isom_make_interleave(file, interleaving_time);
		if (!e && !old_interleave) e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_DRIFT_INTERLEAVED);
	}
	if (e) goto err_exit;

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
			needSave = 1;
		} else {
			gf_isom_sdp_add_line(file, sdp_lines[i].line);
			needSave = 1;
		}
	}
#endif /*!defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)*/

	if (cprt) {
		e = gf_isom_set_copyright(file, "und", cprt);
		needSave = 1;
		if (e) goto err_exit;
	}
	if (chap_file) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		e = gf_media_import_chapters(file, chap_file, import_fps);
		needSave = 1;
#else
		fprintf(stderr, "Warning: GPAC compiled without Media Import, chapters can't be imported\n");
		e = GF_NOT_SUPPORTED;
#endif
		if (e) goto err_exit;
	}

	if (major_brand) {
		gf_isom_set_brand_info(file, major_brand, minor_version);
		needSave = 1;
	}
	for (i=0; i<nb_alt_brand_add; i++) {
		gf_isom_modify_alternate_brand(file, brand_add[i], 1);
		needSave = 1;
	}
	for (i=0; i<nb_alt_brand_rem; i++) {
		gf_isom_modify_alternate_brand(file, brand_rem[i], 0);
		needSave = 1;
	}

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

		if (!e && !outName && !encode && !force_new && !pack_file) {
			if (gf_delete_file(inName)) fprintf(stderr, "Error removing file %s\n", inName);
			else if (gf_move_file(outfile, inName)) fprintf(stderr, "Error renaming file %s to %s\n", outfile, inName);
		}
	} else {
		gf_isom_delete(file);
	}

	if (e) fprintf(stderr, "Error: %s\n", gf_error_to_string(e));
	MP4BOX_EXIT_WITH_CODE( (e!=GF_OK) ? 1 : 0 );
#else
	/*close libgpac*/
	gf_isom_delete(file);
	fprintf(stderr, "Error: Read-only version of MP4Box.\n");
	MP4BOX_EXIT_WITH_CODE(1);
#endif
err_exit:
	/*close libgpac*/
	if (file) gf_isom_delete(file);		
	fprintf(stderr, "\n\tError: %s\n", gf_error_to_string(e));
	MP4BOX_EXIT_WITH_CODE(1);
}

int main( int argc, char** argv )
{
	return mp4boxMain( argc, argv );
}

#endif /*GPAC_DISABLE_ISOM*/
