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
#include <gpac/media_tools.h>
/*RTP packetizer flags*/
#include <gpac/ietf.h>
#include <gpac/ismacryp.h>

#define BUFFSIZE	8192

/*in fileimport.c*/
#ifndef GPAC_READ_ONLY
void convert_file_info(char *inName, u32 trackID);
GF_Err import_file(GF_ISOFile *dest, char *inName, u32 import_flags, Double force_fps, u32 frames_per_sample);
GF_Err split_isomedia_file(GF_ISOFile *mp4, Double split_dur, u32 split_size_kb, char *inName, Double InterleavingTime, Double chunk_start, const char *tmpdir, char *outfile);
GF_Err cat_isomedia_file(GF_ISOFile *mp4, char *fileName, u32 import_flags, Double force_fps, u32 frames_per_sample, char *tmp_dir, Bool force_cat);

GF_Err EncodeFile(char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, FILE *logs);
GF_Err EncodeFileChunk(char *chunkFile, char *bifs, char *inputContext, char *outputContext, const char *tmpdir);

GF_ISOFile *package_file(char *file_name, char *fcc, const char *tmpdir);
GF_Err dump_cover_art(GF_ISOFile *file, char *inName);
u32 id3_get_genre_tag(const char *name);
#endif

/*in filedump.c*/
#ifndef GPAC_READ_ONLY
GF_Err dump_file_text(char *file, char *inName, u32 dump_mode, Bool do_log);
void dump_scene_stats(char *file, char *inName, u32 stat_level);
#endif
void PrintNode(const char *name, u32 graph_type);
void PrintBuiltInNodes(u32 graph_type);
void dump_file_mp4(GF_ISOFile *file, char *inName);
void dump_file_rtp(GF_ISOFile *file, char *inName);
void dump_file_ts(GF_ISOFile *file, char *inName);
void dump_file_ismacryp(GF_ISOFile *file, char *inName);
void dump_timed_text_track(GF_ISOFile *file, u32 trackID, char *inName, Bool is_convert, u32 dump_type);
void DumpSDP(GF_ISOFile *file, char *inName);
void DumpTrackInfo(GF_ISOFile *file, u32 trackID, Bool full_dump);
void DumpMovieInfo(GF_ISOFile *file);
void PrintLanguages();
const char *GetLanguageCode(char *lang);
void dump_mpeg2_ts(char *mpeg2ts_in, char *pes_out_name);


Bool quiet = 0;

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
	{GF_ISOM_ITUNE_CREATED, "created", ""},
	{GF_ISOM_ITUNE_DISK, "disk", "usage: disk=x/N"},
	{GF_ISOM_ITUNE_TOOL, "tool", "usage: tool=name"},
	{GF_ISOM_ITUNE_GENRE, "genre", "usage: genre=name"},
	{GF_ISOM_ITUNE_NAME, "name", "usage: name=name"},
	{GF_ISOM_ITUNE_TEMPO, "tempo", "usage: tempo=integer"},
	{GF_ISOM_ITUNE_WRITER, "writer", "usage: writer=name"},
	{GF_ISOM_ITUNE_GROUP, "group", "usage: group=name"},
	{GF_ISOM_ITUNE_COVER_ART, "cover", "usage: covber=file.jpg,file.png"},
	{GF_ISOM_ITUNE_ENCODER, "encoder", "usage: encoder=name"},
	{GF_ISOM_ITUNE_GAPELESS, "gapeless", "usage: artist=yes,no"},
};

u32 nb_itunes_tags = sizeof(itags) / sizeof(itunes_tag);


void PrintVersion()
{
	fprintf(stdout, "MP4Box - GPAC version " GPAC_FULL_VERSION "\n"
#ifdef GPAC_FIXED_POINT
				"GPAC compiled in fixed-point version\n"
#endif
#ifdef GPAC_READ_ONLY
				"GPAC compiled in read-only version\n"
#endif
		"GPAC Copyright: (c) Jean Le Feuvre 2000-2005\n\t\t(c) ENST 2005-200X\n");
}

void PrintGeneralUsage()
{
	fprintf(stdout, "General Options:\n"
			" -inter time_in_ms    interleaves file data (track chunks of time_in_ms)\n"
			"                       * Note 1: Interleaving is 0.5s by default\n"
			"                       * Note 2: Performs drift checking accross tracks\n"
			"                       * Note 3: a value of 0 disables interleaving\n"
			" -old-inter time      same as -inter but doesn't perform drift checking\n"
			" -tight:              performs tight interleaving (sample based) of the file\n"
			"                       * Note: reduces disk seek but increases file size\n"
			" -flat                stores file with all media data first, non-interleaved\n"
			" -frag time_in_ms     fragments file (track fragments of time_in_ms)\n"
			"                       * Note: Always disables interleaving\n"
			" -out filename        specifies output file name\n"
			"                       * Note: By default input (MP4,3GP) file is overwritten\n"
			" -tmp dirname         specifies directory for temporary file creation\n"
			"                       * Note: Default temp dir is OS-dependent\n"
			" -no-sys              removes all MPEG-4 Systems info except IOD (profiles)\n"
			"                       * Note: Set by default whith '-add' and '-cat'\n"
			" -no-iod              removes InitialObjectDescriptor from file\n"
			" -isma                rewrites the file as an ISMA 1.0 AV file\n"
			" -ismax               same as \'-isma\' and removes all clock references\n"
			" -3gp                 rewrites as 3GPP(2) file (no more MPEG-4 Systems Info)\n"
			"                       * Note 1: some tracks may be removed in the process\n"
			"                       * Note 2: always on for *.3gp *.3g2 *.3gpp\n"
			" -ipod                rewrites the file for iPod\n"
			" -brand ABCD[:v]      sets major brand of file, with optional version\n"
			" -ab ABCD             adds given brand to file's alternate brand list\n"
			" -rb ABCD             removes given brand from file's alternate brand list\n"
			" -cprt string         adds copyright string to movie\n"
			" -chap file           adds chapter information contained in file\n"
			" -rem trackID:        removes track from file\n"
			" -enable trackID:     enables track\n"
			" -disable trackID:    disables track\n"
			" -new:                forces creation of a new destination file\n"
			" -rem trackID:        removes track from file\n"
			" -lang [tkID=]LAN:    sets track language. LAN is the ISO 639-2 code (eng, und)\n"
			" -delay tkID=TIME:    sets track start delay in ms.\n"
			" -par tkID=PAR:       sets visual track pixel aspect ratio (PAR=N:D or \"none\")\n"
			" -name tkID=NAME:     sets track handler name\n"
			"                       * NAME can indicate a UTF-8 file (\"file://file name\"\n"
			" -itags tag1[:tag2]:  sets iTunes tags to file - more info: MP4Box -tag-list.\n"
			" -split time_sec      splits in files of time_sec max duration\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -split-size size     splits in files of max filesize kB.\n"
			"                       * Note: this removes all MPEG-4 Systems media\n"
			" -split-chunk S:E     extracts a new file from Start to End (in seconds)\n"
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
			" -ref id:XXXX:refID   adds a reference of type 4CC from track ID to track refID\n"
			"\n");
}

void PrintFormats()
{
	fprintf(stdout, "Suppported raw formats and file extensions:\n"
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
			" MacroMedia Flash     .swf (very limitted import support only)\n"
			"\n"
		);
}

void PrintImportUsage()
{
	fprintf(stdout, "Importing Options\n"
			"\nFile importing syntax:\n"
			" \"#video\" \"#audio\":   base import for most AV files\n"
			" \"#trackID=ID\":       track import for IsoMedia and other files\n"
			" \"#pid=ID\":           stream import from MPEG-2 TS\n"
			" \":dur=D\":            imports only the first D seconds\n"
			" \":lang=LAN\":         sets imported media language code\n"
			" \":delay=delay_ms\":   sets imported media initial delay in ms\n"
			" \":par=PAR\":          sets visual pixel aspect ratio (PAR=Num:Den)\n"
			" \":name=NAME\":        sets track handler name\n"
			" \":disable\":          imported track(s) will be disabled\n"
			" \":group=G\":          adds the track as part of the G alternate group.\n"
			"                        * If G is 0, the first available GroupID will be picked.\n"
			" \":fps=VAL\":          same as -fps option\n"
			" \":agg=VAL\":          same as -agg option\n"
			" \":par=VAL\":          same as -par option\n"
			" \":dref\":             same as -dref option\n"
			" \":nodrop\":           same as -nodrop option\n"
			" \":packed\":           same as -packed option\n"
			" \":sbr\":              same as -sbr option\n"
			" \":sbrx\":             same as -sbrx option\n"
			" \":mpeg4\":            same as -mpeg4 option\n"
			" \":font=name\":        specifies font name for text import (default \"Serif\")\n"
			" \":size=s\":           specifies font size for text import (default 18)\n"
			"\n"
			" -add file:           add file tracks to (new) output file\n"
			" -cat file:           concatenates file samples to (new) output file\n"
			"                       * Note: creates tracks if needed\n"
			" -force-cat:          skips media configuration check when concatenating file\n"
			"                       !!! THIS MAY BREAK THE CONCATENATED TRACK(S) !!!\n"
			" -keep-sys:           keeps all MPEG-4 Systems info when using '-add' / 'cat'\n"
			" -keep-all:           keeps all existing tracks when using '-add'\n"
			"                       * Note: only used when adding IsoMedia files\n"
			"\n"
			"All the following options can be specified as default or for each track.\n"
			"When specified by track the syntax is \":opt\" or \":opt=val\".\n\n"
			" -dref:               keeps media data in original file\n"
			" -no-drop:             forces constant FPS when importing AVI video\n"
			" -packed:             * forces packed bitstream when importing raw ASP\n"
			" -sbr:                backward compatible signaling of AAC-SBR\n"
			" -sbrx:               non-backward compatible signaling of AAC-SBR\n"
			"                       * Note: SBR AAC cannot be detected at import time\n"
			" -fps FPS:            forces frame rate for video and SUB subtitles import\n"
			"                       * For raw H263 import, default FPS is 15\n"
			"                       * For all other imports, default FPS is 25\n"
			"                      -- THIS IS IGNORED FOR IsoMedia IMPORT --\n"
			" -mpeg4:              forces MPEG-4 sample descriptions when possible (3GPP2)\n"
			"                       For AAC, forces MPEG-4 AAC signaling even if MPEG-2\n"
			" -agg N:              aggregates N audio frames in 1 sample (3GP media only)\n"
			"                       * Note: Maximum value is 15 - Disabled by default\n"
			"\n"
			);
}

void PrintEncodeUsage()
{
	fprintf(stdout, "MPEG-4 Scene Encoding Options\n"
			" -mp4:                specify input file is for encoding.\n"
			" -def:                encode DEF names\n"
			" -sync time_in_ms:    forces BIFS sync sample generation every time_in_ms\n"
			"                       * Note: cannot be used with -shadow\n"
			" -shadow time_ms:     forces BIFS sync shadow sample generation every time_ms.\n"
			"                       * Note: cannot be used with -sync\n"
			" -log:                generates scene codec log file if available\n"
			" -ms file:            specifies file for track importing\n"
			"\nChunk Processing\n"
			" -ctx-in file:        specifies initial context (MP4/BT/XMT)\n"
			"                       * Note: input file must be a commands-only file\n"
			" -ctx-out file:       specifies storage of updated context (MP4/BT/XMT)\n"
			"\n"
			"LASeR Encoding options\n"
			" -resolution res:     resolution factor (-8 to 7, default 0)\n"
			"                       all coords are multiplied by 2^res before truncation\n"
			" -coord-bits bits:    bits used for encoding truncated coordinates\n"
			"                       (0 to 31, default 12)\n"
			" -scale-bits bits:    extra bits used for encoding truncated scales\n"
			"                       (0 to 4, default 0)\n"
			" -auto-quant res:     resolution is given as if using -resolution\n"
			"                       but coord-bits and scale-bits are infered\n"
			);
}

void PrintEncryptUsage()
{
	fprintf(stdout, "ISMA Encryption/Decryption Options\n"
			" -crypt drm_file:     crypts a specific track using ISMA AES CTR 128\n"
			" -decrypt [drm_file]  decrypts a specific track using ISMA AES CTR 128\n"
			"                       * Note: drm_file can be omitted if keys are in file\n"
			" -set-kms kms_uri     changes KMS location for all tracks or a given one.\n"
			"                       * to adress a track, use \'tkID=kms_uri\'\n"
			"\n"
			"DRM file syntax for GPAC ISMACryp:\n"
			"                      File is XML and shall start with xml header\n"
			"                      File root is an \"ISMACryp\" element\n"
			"                      File is a list of \"ISMACrypTrack\" elements\n"
			"\n"
			"ISMACrypTrack attributes are\n"
			" TrackID:             ID of track to en/decrypt\n"
			" key:                 AES-128 key formatted (hex string \'0x\'+32 chars)\n"
			" salt:                CTR IV salt key (64 bits) (hex string \'0x\'+16 chars)\n"
			"\nEncryption only attributes:\n"
			" Scheme_URI:          URI of scheme used\n"
			" KMS_URI:             URI of key management system\n"
			"                       * Note: \'self\' writes key and salt in the file\n"
			" selectiveType:       selective encryption type - understood values are:\n"
			"   \"None\":            all samples encrypted (default)\n"
			"   \"RAP\":             only encrypts random access units\n"
			"   \"Non-RAP\":         only encrypts non-random access units\n"
			"   \"Rand\":            random selection is performed\n"
			"   \"X\":               Encrypts every first sample out of X (uint)\n"
			"   \"RandX\":           Encrypts one random sample out of X (uint)\n"
			"\n"
			" ipmpType:            IPMP Signaling Type: None, IPMP, IPMPX\n"
			" ipmpDescriptorID:    IPMP_Descriptor ID to use if IPMP(X) is used\n"
			"                       * If not set MP4Box will generate one for you\n"
			"\n"
		);
}

void PrintHintUsage()
{
	fprintf(stdout, "Hinting Options\n"
			" -hint:               hints the file for RTP/RTSP\n"
			" -mtu size:           specifies RTP MTU (max size) in bytes. Default size is 1450\n"
			"                       * Note: this includes the RTP header (12 bytes)\n"
			" -copy:               copies media data to hint track rather than reference\n"
			"                       * Note: speeds up server but takes much more space\n"
			" -multi [maxptime]:   enables frame concatenation in RTP packets if possible\n"
			"        maxptime:     max packet duration in ms (optional, default 100ms)\n"
			" -rate ck_rate:       specifies rtp rate in Hz when no default for payload\n"
			"                       * Note: default value is 90000 (MPEG rtp rates)\n"
			" -mpeg4:              forces MPEG-4 generic payload whenever possible\n"
			" -latm:               forces MPG4-LATM transport for AAC streams\n"
			" -static:             enables static RTP payload IDs whenever possible\n"
			"                       * By default, dynamic payloads are always used\n"
			"\n"
			"MPEG-4 Generic Payload Options\n"
			" -ocr:                forces all streams to be synchronized\n"
			"                       * Most RTSP servers only support synchronized streams\n"
			" -rap:                signals random access points in RTP packets\n"
			" -ts:                 signals AU Time Stamps in RTP packets\n"
			" -size:               signals AU size in RTP packets\n"
			" -idx:                signals AU sequence numbers in RTP packets\n"
			" -iod:                prevents systems tracks embedding in IOD\n"
			"                       * Note: shouldn't be used with -isma option\n"
			"\n"
			" -add-sdp string:     adds sdp string to (hint) track (\"-add-sdp tkID:string\")\n"
			"                      or movie. This will take care of SDP lines ordering\n"
			" -unhint:             removes all hinting information.\n"
			"\n");
}
void PrintExtractUsage()
{
	fprintf(stdout, "Extracting Options\n"
			" -raw TrackID:        extracts track in raw format when supported\n" 
			" -raws TrackID:       extract each track sample to a file\n" 
			"                       * Note: \"TrackID:N\" extracts Nth sample\n"
			" -nhnt TrackID:       extracts track in nhnt format\n" 
			" -nhml TrackID:       extracts track in nhml format (XML nhnt).\n" 
			"                       * Note: \"-nhml +TrackID\" for full dump\n"
			" -single TrackID:     extracts track to a new mp4 file\n"
			" -avi TrackID:        extracts visual track to an avi file\n"
			" -qcp TrackID:        same as \'-raw\' but defaults to QCP file for EVRC/SMV\n" 
			" -aviraw TK:          extracts AVI track in raw format\n"
			"                      $TK can be one of \"video\" \"audio\" \"audioN\"\n" 
			" -saf:                remux file to SAF multiplex\n"
			"                       * Note: can be used when encoding scene descriptions\n"
			"\n");
}
void PrintDumpUsage()
{
	fprintf(stdout, "Dumping Options\n"
			" -std:                dumps to stdout instead of file\n"
			" -info [trackID]      prints movie info / track info if trackID specified\n"
			"                       * Note: for non IsoMedia files, gets import options\n" 
			" -bt:                 scene to bt format - removes unknown MPEG4 nodes\n" 
			" -xmt:                scene to XMT-A format - removes unknown MPEG4 nodes\n" 
			" -wrl:                scene VRML format - removes unknown VRML nodes\n" 
			" -x3d:                scene to X3D/XML format - removes unknown X3D nodes\n" 
			" -x3dv:               scene to X3D/VRML format - removes unknown X3D nodes\n" 
			" -lsr:                scene to LASeR format\n" 
			" -diso:               scene IsoMedia file boxes in XML output\n"
			" -drtp:               rtp hint samples structure to XML output\n"
			" -dts:                prints sample timing to text output\n"
			" -sdp:                dumps SDP description of hinted file\n"
			" -dcr:                ISMACryp samples structure to XML output\n"
			"\n"
#ifndef GPAC_READ_ONLY
			" -ttxt:               Converts input subtitle to GPAC TTXT format\n"
#endif
			" -ttxt TrackID:       Dumps Text track to GPAC TTXT format\n"
#ifndef GPAC_READ_ONLY
			" -srt:                Converts input subtitle to SRT format\n"
#endif
			" -srt TrackID:        Dumps Text track to SRT format\n"
			"\n"
			" -stat:               generates node/field statistics for scene\n"
			" -stats:              generates node/field statistics per MPEG-4 Access Unit\n"
			" -statx:              generates node/field statistics for scene after each AU\n"
			"\n"
			" -hash:               generates SHA-1 Hash of the input file\n"
			"\n");
}

void PrintMetaUsage()
{
	fprintf(stdout, "Meta handling Options\n"
			" -set-meta args:      sets given meta type - syntax: \"ABCD[:tk=ID]\"\n"
			"                       * ABCD: four char meta type (NULL or 0 to remove meta)\n"
			"                       * [:tk=ID]: if not set use root (file) meta\n"
			"                                if ID is 0 use moov meta\n"
			"                                if ID is not 0 use track meta\n"
			" -add-item args:      adds resource to meta\n"
			"                       * syntax: file_path + options (\':\' separated):\n"
			"                        tk=ID: meta adressing (file, moov, track)\n"
			"                        name=str: item name\n"
			"                        mime=mtype: item mime type\n"
			"                        encoding=enctype: item content-encoding type\n"
			"                       * file_path \"this\" or \"self\": item is the file itself\n"
			" -rem-item args:      removes resource from meta - syntax: item_ID[:tk=ID]\n"
			" -set-primary args:   sets item as primary for meta - syntax: item_ID[:tk=ID]\n"
			" -set-xml args:       sets meta XML data\n"
			"                       * syntax: xml_file_path[:tk=ID][:binary]\n"
			" -rem-xml [tk=ID]:    removes meta XML data\n"
			" -dump-xml args:      dumps meta XML to file - syntax file_path[:tk=ID]\n"
			" -dump-item args:     dumps item to file - syntax item_ID[:tk=ID][:path=fileName]\n"
			" -package:            packages input XML file into an ISO container\n"
			"                       * all media referenced except hyperlinks are added to file\n"
			"\n");
}

void PrintSWFUsage()
{
	fprintf(stdout, 
			"SWF Importer Options\n"
			"\n"
			"MP4Box can import simple Macromedia Flash files (\".SWF\")\n"
			"You can specify a SWF input file with \'-bt\', \'xmt\' and \'-mp4\' options\n"
			"\n"
			" -global:             all SWF defines are placed in first scene replace\n"
			"                       * Note: By default SWF defines are sent when needed\n"
			" -no-ctrl:            uses a single stream for movie control and dictionary\n"
			"                       * Note: this will disable ActionScript\n"
			" -no-text:            removes all SWF text\n"
			" -no-font:            removes all embedded SWF Fonts (terminal fonts used)\n"
			" -no-line:            removes all lines from SWF shapes\n"
			" -no-grad:            removes all gradients from swf shapes\n"
			" -quad:               uses quadratic bezier curves instead of cubic ones\n"
			" -xlp:                support for lines transparency and scalability\n"
			" -flatten ang:        complementary angle below which 2 lines are merged\n"
			"                       * Note: angle \'0\' means no flattening\n"
			"\n"
		);
}

void PrintUsage()
{
	fprintf (stdout, "MP4Box [option] input [option]\n"
#ifndef GPAC_READ_ONLY
			" -h general:          general options help\n"
			" -h hint:             hinting options help\n"
			" -h import:           import options help\n"
			" -h encode:           encode options help\n"
			" -h meta:             meta handling options help\n"
#else
			"READ-ONLY VERSION\n"
#endif
			" -h extract:          extraction options help\n"
			" -h dump:             dump options help\n"
			" -h swf:              Flash (SWF) options help\n"
			" -h crypt:            ISMA E&A options help\n"
			" -h format:           supported formats help\n"
			"\n"
			" -nodes:              lists supported MPEG4 nodes\n"
			" -node NodeName:      gets MPEG4 node syntax and QP info\n"
			" -xnodes:             lists supported X3D nodes\n"
			" -xnode NodeName:     gets X3D node syntax\n"
			" -snodes:             lists supported SVG nodes\n"
			" -snode NodeName:     gets SVG node syntax\n"
			" -languages:              lists supported ISO 639 languages\n"
			"\n"
			"-quiet:               quiet mode\n"
			" -v:                  verbose mode\n"
			" -version:            gets build version\n"
			);
}


void scene_coding_log(void *cbk, u32 log_level, u32 log_tool, const char *fmt, va_list vlist)
{
	FILE *logs = cbk;
	if (log_tool != GF_LOG_CODING) return;
    vfprintf(logs, fmt, vlist);
	fflush(logs);
}

#ifndef GPAC_READ_ONLY

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

GF_Err HintFile(GF_ISOFile *file, u32 MTUSize, u32 max_ptime, u32 rtp_rate, u32 base_flags, Bool copy_data, Bool interleave, Bool regular_iod)
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
	single_av = gf_isom_is_single_av(file);

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
			fprintf(stdout, "Track ID %d disabled - skipping hint\n", gf_isom_get_track_id(file, i+1) );
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
				fprintf(stdout, "Cannot create hinter (%s)\n", gf_error_to_string(e));
				if (!nb_done) return e;
			}
			continue;
		} 
		bw = gf_hinter_track_get_bandwidth(hinter);
		tot_bw += bw;
		flags = gf_hinter_track_get_flags(hinter);
		gf_hinter_track_get_payload_name(hinter, szPayload);
		fprintf(stdout, "Hinting track ID %d - Type \"%s:%s\" (%s) - BW %d kbps\n", gf_isom_get_track_id(file, i+1), gf_4cc_to_str(mtype), gf_4cc_to_str(mtype), szPayload, bw);
		if (flags & GP_RTP_PCK_AUTO_CAROUSEL) fprintf(stdout, "\tMPEG-4 Systems stream carousel enabled\n");
/*
		if (flags & GP_RTP_PCK_FORCE_MPEG4) fprintf(stdout, "\tMPEG4 transport forced\n");
		if (flags & GP_RTP_PCK_USE_MULTI) fprintf(stdout, "\tRTP aggregation enabled\n");
*/
		e = gf_hinter_track_process(hinter);

		if (!e) e = gf_hinter_track_finalize(hinter, has_iod);
		gf_hinter_track_del(hinter);
		
		if (e) {
			fprintf(stdout, "Error while hinting (%s)\n", gf_error_to_string(e));
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
		fprintf(stdout, "Warning: at least 2 timelines found in the file\nThis may not be supported by servers/players\n\n");

	return GF_OK;
}



static void check_media_profile(GF_ISOFile *file, u32 track)
{
	u8 PL;
	GF_M4ADecSpecInfo dsi;
	GF_ESD *esd = gf_isom_get_esd(file, track, 1);
	if (!esd) return;

	switch (esd->decoderConfig->streamType) {
	case 0x04:
		PL = gf_isom_get_pl_indication(file, GF_ISOM_PL_VISUAL);
		if (esd->decoderConfig->objectTypeIndication==0x20) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			if (dsi.VideoPL > PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, dsi.VideoPL);
		} else if (esd->decoderConfig->objectTypeIndication==0x21) {
			gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0x15);
		} else if (!PL) {
			gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, 0xFE);
		}
		break;
	case 0x05:
		PL = gf_isom_get_pl_indication(file, GF_ISOM_PL_AUDIO);
		switch (esd->decoderConfig->objectTypeIndication) {
		case 0x66: case 0x67: case 0x68: case 0x40:
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

#endif

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
	char *__ext = strrchr(inName, '.');
	if (__ext) {
		char ext[20];
		if (!strcmp(__ext, ".gz")) __ext = strrchr(__ext-1, '.');
		strcpy(ext, __ext+1);
		__ext = strchr(ext, '.');
		if (__ext) __ext[0] = 0;

		if (!stricmp(ext, "mp4") || !stricmp(ext, "3gp") || !stricmp(ext, "mov") || !stricmp(ext, "3g2")) type = 1;
		else if (!stricmp(ext, "bt") || !stricmp(ext, "wrl") || !stricmp(ext, "x3dv")) type = 2;
		else if (!stricmp(ext, "xmt") || !stricmp(ext, "x3d")) type = 3;
		else if (!stricmp(ext, "lsr") || !stricmp(ext, "saf")) type = 6;
		else if (!stricmp(ext, "svg")) type = 4;
		else if (!stricmp(ext, "xsr")) type = 4;
		else if (!stricmp(ext, "xml")) type = 4;
		else if (!stricmp(ext, "swf")) type = 5;
		else if (!stricmp(ext, "jp2")) return 0;
		else type = 0;
	}

	/*try open file in read mode*/
	if (!type && gf_isom_probe_file(inName)) type = 1;
	return type;
}

#ifndef GPAC_READ_ONLY
static Bool can_convert_to_isma(GF_ISOFile *file)
{
	u32 spec = gf_isom_guess_specification(file);
	if (spec==GF_4CC('I','S','M','A')) return 1;
	return 0;
}
#endif

static void progress_quiet(void *cbck, char *title, u32 done, u32 total) { }


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

/*for SDP_EX, AddTrack and RemTrack*/
#define MAX_CUMUL_OPS	20

#ifndef GPAC_READ_ONLY
static Bool parse_meta_args(MetaAction *meta, char *opts)
{
	Bool ret = 0;
	char szSlot[1024], *next;

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
			sscanf(szSlot, "tk=%d", &meta->trackID);
			meta->root_meta = 0;
			ret = 1;
		}
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

static Bool parse_tsel_args(TSELAction *tsel_list, char *opts, u32 *nb_tsel_act)
{
	u32 act;
	u32 refTrackID = 0;
	Bool has_switch_id;
	u32 switch_id = 0;
	u32 criteria[30];
	u32 nb_criteria = 0;
	TSELAction *tsel_act;
	char szSlot[1024], *next;

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
			if (*nb_tsel_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track selection operations allowed\n", MAX_CUMUL_OPS); 
				return 0; 
			}
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


#define CHECK_NEXT_ARG	if (i+1==(u32)argc) { fprintf(stdout, "Missing arg - please check usage\n"); return 1; }

#define CHECK_META_OPS	CHECK_NEXT_ARG if (nb_meta_act>=MAX_CUMUL_OPS) { fprintf(stdout, "Sorry - no more than %d meta operations allowed\n", MAX_CUMUL_OPS); return 1; }


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
	*/
	u32 act_type;
	/*track ID*/
	u32 trackID;
	char lang[4];
	s32 delay_ms;
	const char *kms;
	const char *hdl_name;
	s32 par_num, par_den;
} TrackAction;

enum
{
	GF_ISOM_CONV_TYPE_ISMA = 1,
	GF_ISOM_CONV_TYPE_ISMA_EX,
	GF_ISOM_CONV_TYPE_3GPP,
	GF_ISOM_CONV_TYPE_IPOD,
	GF_ISOM_CONV_TYPE_PSP
};

int main(int argc, char **argv)
{
	char outfile[5000];
	GF_Err e;
	GF_SMEncodeOptions opts;
	Double InterleavingTime, split_duration, split_start, import_fps;
	SDPLine sdp_lines[MAX_CUMUL_OPS];
	MetaAction metas[MAX_CUMUL_OPS];
	char *szFilesToCat[MAX_CUMUL_OPS];
	char *szTracksToAdd[MAX_CUMUL_OPS];
	TrackAction tracks[MAX_CUMUL_OPS];
	TSELAction tsel_acts[MAX_CUMUL_OPS];
	u32 brand_add[MAX_CUMUL_OPS], brand_rem[MAX_CUMUL_OPS];
	u32 i, MTUSize, stat_level, hint_flags, info_track_id, import_flags, nb_add, nb_cat, ismaCrypt, agg_samples, nb_sdp_ex, max_ptime, raw_sample_num, split_size, nb_meta_act, nb_track_act, rtp_rate, major_brand, nb_alt_brand_add, nb_alt_brand_rem, old_interleave, car_dur, minor_version, conv_type, nb_tsel_acts;
	Bool HintIt, needSave, FullInter, Frag, HintInter, dump_std, dump_rtp, dump_mode, regular_iod, trackID, HintCopy, remove_sys_tracks, remove_hint, force_new, keep_sys_tracks, remove_root_od, import_subtitle;
	Bool print_sdp, print_info, open_edit, track_dump_type, dump_isom, dump_cr, force_ocr, encode, do_log, do_flat, dump_srt, dump_ttxt, x3d_info, chunk_mode, dump_ts, do_saf, dump_m2ts, dump_cart, do_hash, verbose, force_cat;
	char *inName, *outName, *arg, *mediaSource, *tmpdir, *input_ctx, *output_ctx, *drm_file, *avi2raw, *cprt, *chap_file, *pes_dump, *itunes_tags, *pack_file, *raw_cat;
	GF_ISOFile *file;


	if (argc < 2) {
		PrintUsage();
		return 1;
	}

	nb_tsel_acts = nb_add = nb_cat = nb_track_act = nb_sdp_ex = max_ptime = raw_sample_num = nb_meta_act = rtp_rate = major_brand = nb_alt_brand_add = nb_alt_brand_rem = car_dur = minor_version = 0;
	e = GF_OK;
	split_duration = 0.0;
	split_start = -1.0;
	InterleavingTime = 0.5;
	import_fps = 0;
	import_flags = 0;
	split_size = 0;
	MTUSize = 1450;
	HintCopy = FullInter = HintInter = encode = do_log = old_interleave = do_saf = do_hash = verbose = 0;
	chunk_mode = dump_mode = Frag = force_ocr = remove_sys_tracks = agg_samples = remove_hint = keep_sys_tracks = remove_root_od = 0;
	x3d_info = conv_type = HintIt = needSave = print_sdp = print_info = regular_iod = dump_std = open_edit = dump_isom = dump_rtp = dump_cr = dump_srt = dump_ttxt = force_new = dump_ts = dump_m2ts = dump_cart = import_subtitle = force_cat = 0;
	track_dump_type = 0;
	ismaCrypt = 0;
	file = NULL;
	itunes_tags = pes_dump = NULL;
	memset(&opts, 0, sizeof(opts));
	
	trackID = stat_level = hint_flags = 0;
	info_track_id = 0;
	do_flat = 0;
	inName = outName = mediaSource = input_ctx = output_ctx = drm_file = avi2raw = cprt = chap_file = pack_file = raw_cat = NULL;

	swf_flags = GF_SM_SWF_SPLIT_TIMELINE;
	swf_flatten_angle = 0.0f;
	tmpdir = NULL;
	
	/*parse our args*/
	for (i = 1; i < (u32) argc ; i++) {
		arg = argv[i];
		/*main file*/
//		if (isalnum(arg[0]) || (arg[0]=='/') || (arg[0]=='.') || (arg[0]=='\\') ) {
		if (arg[0] != '-') {
			if (inName) { fprintf(stdout, "Error - 2 input names specified, please check usage\n"); return 1; }
			inName = arg;
		}
		else if (!stricmp(arg, "-?")) { PrintUsage(); return 0; }
		else if (!stricmp(arg, "-version")) { PrintVersion(); return 0; }
		else if (!stricmp(arg, "-sdp")) print_sdp = 1;
		else if (!stricmp(arg, "-quiet")) quiet = 1;
		else if (!stricmp(arg, "-info")) {
			print_info = 1;
			if ((i+1<(u32) argc) && (sscanf(argv[i+1], "%d", &info_track_id)==1)) {
				char szTk[20];
				sprintf(szTk, "%d", info_track_id);
				if (!strcmp(szTk, argv[i+1])) i++;
				else info_track_id=0;
			} else {
				info_track_id=0;
			}
		}
		else if (!stricmp(arg, "-raw")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_NATIVE;
			trackID = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-qcp")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_NATIVE | GF_EXPORT_USE_QCP;
			trackID = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-aviraw")) {
			CHECK_NEXT_ARG
			if (argv[i+1] && !stricmp(argv[i+1], "video")) trackID = 1;
			else if (argv[i+1] && !stricmp(argv[i+1], "audio")) {
				if (strlen(argv[i+1])==5) trackID = 2;
				else trackID = 1 + atoi(argv[i+1] + 5);
			}
			else { fprintf(stdout, "Usage: \"-aviraw video\" or \"-aviraw audio\"\n"); return 1; }
			track_dump_type = GF_EXPORT_AVI_NATIVE;
			i++;
		}
		else if (!stricmp(arg, "-raws")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_RAW_SAMPLES;
			if (strchr(argv[i+1], ':')) {
				sscanf(argv[i+1], "%d:%d", &trackID, &raw_sample_num);
			} else {
				trackID = atoi(argv[i+1]);
			}
			i++;
		}
		else if (!stricmp(arg, "-nhnt")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_NHNT;
			trackID = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-nhml")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_NHML;
			if (argv[i+1][0]=='+') {
				track_dump_type |= GF_EXPORT_NHML_FULL;
				trackID = atoi(argv[i+1] + 1);
			} else {
				trackID = atoi(argv[i+1]);
			}
			i++;
		}
		else if (!stricmp(arg, "-avi")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_AVI;
			trackID = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-node")) { CHECK_NEXT_ARG PrintNode(argv[i+1], 0); return (0); }
		else if (!stricmp(arg, "-xnode")) { CHECK_NEXT_ARG PrintNode(argv[i+1], 1); return (0); }
		else if (!stricmp(arg, "-snode")) { CHECK_NEXT_ARG PrintNode(argv[i+1], 2); return (0); }
		else if (!stricmp(arg, "-nodes")) { PrintBuiltInNodes(0); return (0); }
		else if (!stricmp(arg, "-xnodes")) { PrintBuiltInNodes(1); return (0); } 
		else if (!stricmp(arg, "-snodes")) { PrintBuiltInNodes(2); return (0); } 
		else if (!stricmp(arg, "-std")) dump_std = 1;
		else if (!stricmp(arg, "-bt")) dump_mode = 1 + GF_SM_DUMP_BT;
		else if (!stricmp(arg, "-xmt")) dump_mode = 1 + GF_SM_DUMP_XMTA;
		else if (!stricmp(arg, "-wrl")) dump_mode = 1 + GF_SM_DUMP_VRML;
		else if (!stricmp(arg, "-x3dv")) dump_mode = 1 + GF_SM_DUMP_X3D_VRML;
		else if (!stricmp(arg, "-x3d")) dump_mode = 1 + GF_SM_DUMP_X3D_XML;
		else if (!stricmp(arg, "-lsr")) dump_mode = 1 + GF_SM_DUMP_LASER;
		else if (!stricmp(arg, "-svg")) dump_mode = 1 + GF_SM_DUMP_SVG;
		else if (!stricmp(arg, "-stat")) stat_level = 1;
		else if (!stricmp(arg, "-stats")) stat_level = 2;
		else if (!stricmp(arg, "-statx")) stat_level = 3;
		else if (!stricmp(arg, "-diso")) dump_isom = 1;
		else if (!stricmp(arg, "-dump-cover")) dump_cart = 1;
		else if (!stricmp(arg, "-hash")) do_hash = 1;

		else if (!stricmp(arg, "-dmp4")) {
			dump_isom = 1;
			fprintf(stdout, "WARNING: \"-dmp4\" is deprecated - use \"-diso\" option\n");
		}
		else if (!stricmp(arg, "-drtp")) dump_rtp = 1;
		else if (!stricmp(arg, "-dts")) dump_ts = 1;
		else if (!stricmp(arg, "-dcr")) dump_cr = 1;
		else if (!stricmp(arg, "-ttxt") || !stricmp(arg, "-srt")) {
			if ((i+1<(u32) argc) && (sscanf(argv[i+1], "%d", &trackID)==1)) {
				char szTk[20];
				sprintf(szTk, "%d", trackID);
				if (!strcmp(szTk, argv[i+1])) i++;
				else trackID=0;
			} else {
				trackID = 0;
			}
#ifdef GPAC_READ_ONLY
			if (trackID) { fprintf(stdout, "Error: Read-Only version - subtitle conversion not available\n"); return 1; }
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

#ifndef GPAC_READ_ONLY
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
		else if (!stricmp(arg, "-isma")) { conv_type = GF_ISOM_CONV_TYPE_ISMA; open_edit = 1; }
		else if (!stricmp(arg, "-3gp")) { conv_type = GF_ISOM_CONV_TYPE_3GPP; open_edit = 1; }
		else if (!stricmp(arg, "-ipod")) { conv_type = GF_ISOM_CONV_TYPE_IPOD; open_edit = 1; }
		else if (!stricmp(arg, "-ismax")) { conv_type = GF_ISOM_CONV_TYPE_ISMA_EX; open_edit = 1; }

		else if (!stricmp(arg, "-no-sys") || !stricmp(arg, "-nosys")) { remove_sys_tracks = 1; open_edit = 1; }
		else if (!stricmp(arg, "-no-iod")) { remove_root_od = 1; open_edit = 1; }
		else if (!stricmp(arg, "-out")) { CHECK_NEXT_ARG outName = argv[i+1]; i++; }
		else if (!stricmp(arg, "-tmp")) {
			CHECK_NEXT_ARG tmpdir = argv[i+1]; i++; 
		}
		else if (!stricmp(arg, "-cprt")) { CHECK_NEXT_ARG cprt = argv[i+1]; i++; open_edit = 1; }
		else if (!stricmp(arg, "-chap")) { CHECK_NEXT_ARG chap_file = argv[i+1]; i++; open_edit = 1; }
		else if (!stricmp(arg, "-inter") || !stricmp(arg, "-old-inter")) {
			CHECK_NEXT_ARG
			InterleavingTime = atof(argv[i+1]) / 1000;
			open_edit = 1;
			needSave = 1;
			if (!stricmp(arg, "-old-inter")) old_interleave = 1;
			i++;
		} else if (!stricmp(arg, "-frag")) {
			CHECK_NEXT_ARG
			InterleavingTime = atof(argv[i+1]) / 1000;
			open_edit = 1;
			needSave = 1;
			i++;
			Frag = 1;
		} 
		else if (!stricmp(arg, "-itags")) { CHECK_NEXT_ARG itunes_tags = argv[i+1]; i++; open_edit = 1; }
		else if (!stricmp(arg, "-hint")) { open_edit = 1; HintIt = 1; }
		else if (!stricmp(arg, "-unhint")) { open_edit = 1; remove_hint = 1; }
		else if (!stricmp(arg, "-copy")) HintCopy = 1;
		else if (!stricmp(arg, "-tight")) {
			FullInter = 1;
			open_edit = 1;
			needSave = 1;
		} else if (!stricmp(arg, "-ocr")) force_ocr = 1;
		else if (!stricmp(arg, "-latm")) hint_flags |= GP_RTP_PCK_USE_LATM_AAC;
		else if (!stricmp(arg, "-rap")) hint_flags |= GP_RTP_PCK_SIGNAL_RAP;
		else if (!stricmp(arg, "-ts")) hint_flags |= GP_RTP_PCK_SIGNAL_TS;
		else if (!stricmp(arg, "-size")) hint_flags |= GP_RTP_PCK_SIGNAL_SIZE;
		else if (!stricmp(arg, "-idx")) hint_flags |= GP_RTP_PCK_SIGNAL_AU_IDX;
		else if (!stricmp(arg, "-static")) hint_flags |= GP_RTP_PCK_USE_STATIC_ID;
		else if (!stricmp(arg, "-multi")) {
			hint_flags |= GP_RTP_PCK_USE_MULTI;
			if ((i+1<(u32) argc) && (sscanf(argv[i+1], "%d", &max_ptime)==1)) {
				char szPt[20];
				sprintf(szPt, "%d", max_ptime);
				if (!strcmp(szPt, argv[i+1])) i++;
				else max_ptime=0;
			}
		}
		else if (!stricmp(arg, "-mpeg4")) {
			hint_flags |= GP_RTP_PCK_FORCE_MPEG4;
			import_flags |= GF_IMPORT_FORCE_MPEG4;
		}
		else if (!stricmp(arg, "-mtu")) { CHECK_NEXT_ARG MTUSize = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-cardur")) { CHECK_NEXT_ARG car_dur = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-rate")) { CHECK_NEXT_ARG rtp_rate = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-add-sdp") || !stricmp(arg, "-sdp_ex")) {
			char *id;
			CHECK_NEXT_ARG
			if (nb_sdp_ex>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d extra SDP lines allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			id = strchr(argv[i+1], ':');
			if (id) {
				id[0] = 0;
				if (sscanf(argv[i+1], "%d", &sdp_lines[0].trackID)==1) {
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
		else if (!stricmp(arg, "-single")) {
			CHECK_NEXT_ARG
			track_dump_type = GF_EXPORT_MP4;
			trackID = atoi(argv[i+1]);
			i++;
		}
		else if (!stricmp(arg, "-iod")) regular_iod = 1;
		else if (!stricmp(arg, "-flat")) do_flat = 1;
		else if (!stricmp(arg, "-new")) force_new = 1;
		else if (!stricmp(arg, "-add") || !stricmp(arg, "-import") || !stricmp(arg, "-convert")) {
			CHECK_NEXT_ARG
			if (!stricmp(arg, "-import")) fprintf(stdout, "\tWARNING: \"-import\" is deprecated - use \"-add\"\n");
			else if (!stricmp(arg, "-convert")) fprintf(stdout, "\tWARNING: \"-convert\" is deprecated - use \"-add\"\n");
			if (nb_add>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d add operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			szTracksToAdd[nb_add] = argv[i+1];
			nb_add++;
			i++;
		}
		else if (!stricmp(arg, "-cat")) {
			CHECK_NEXT_ARG
			if (nb_cat>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d cat operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			szFilesToCat[nb_cat] = argv[i+1];
			nb_cat++;
			i++;
		}
		else if (!stricmp(arg, "-force-cat")) force_cat = 1;
		else if (!stricmp(arg, "-raw-cat")) {
			CHECK_NEXT_ARG
			raw_cat = argv[i+1];
			i++;
		}
		else if (!stricmp(arg, "-rem") || !stricmp(arg, "-disable") || !stricmp(arg, "-enable")) {
			CHECK_NEXT_ARG
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
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
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			tracks[nb_track_act].act_type = 4;
			strcpy(szTK, argv[i+1]);
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stdout, "Bad format for track par - expecting ID=PAR_NUM:PAR_DEN got %s\n", argv[i+1]);
				return 1;
			}
			if (!stricmp(ext+1, "none")) {
				tracks[nb_track_act].par_num = tracks[nb_track_act].par_den = -1;
			} else {
				sscanf(ext+1, "%d:%d", &tracks[nb_track_act].par_num, &tracks[nb_track_act].par_den);
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
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
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
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			strcpy(szTK, argv[i+1]);
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stdout, "Bad format for track delay - expecting ID=DLAY got %s\n", argv[i+1]);
				return 1;
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
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			szTK = argv[i+1];
			ext = strchr(szTK, ':');
			if (!ext) {
				fprintf(stdout, "Bad format for track reference - expecting ID:XXXX:refID got %s\n", argv[i+1]);
				return 1;
			}
			tracks[nb_track_act].act_type = 8;
			ext[0] = 0; tracks[nb_track_act].trackID = atoi(szTK); ext[0] = ':'; szTK = ext+1;
			ext = strchr(szTK, ':');
			if (!ext) {
				fprintf(stdout, "Bad format for track reference - expecting ID:XXXX:refID got %s\n", argv[i+1]);
				return 1;
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
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			strcpy(szTK, argv[i+1]);
			ext = strchr(szTK, '=');
			if (!ext) {
				fprintf(stdout, "Bad format for track delay - expecting ID=DLAY got %s\n", argv[i+1]);
				return 1;
			}
			tracks[nb_track_act].act_type = 5;
			tracks[nb_track_act].hdl_name = ext+1;
			ext[0] = 0;
			tracks[nb_track_act].trackID = atoi(szTK);
			ext[0] = '=';
			open_edit = 1;
			nb_track_act++;
			i++;
		}
		else if (!stricmp(arg, "-dref")) import_flags |= GF_IMPORT_USE_DATAREF;
		else if (!stricmp(arg, "-no-drop") || !stricmp(arg, "-nodrop")) import_flags |= GF_IMPORT_NO_FRAME_DROP;
		else if (!stricmp(arg, "-packed")) import_flags |= GF_IMPORT_FORCE_PACKED;
		else if (!stricmp(arg, "-sbr")) import_flags |= GF_IMPORT_SBR_IMPLICIT;
		else if (!stricmp(arg, "-sbrx")) import_flags |= GF_IMPORT_SBR_EXPLICIT;
		else if (!stricmp(arg, "-fps")) { 
			CHECK_NEXT_ARG 
			if (!strcmp(argv[i+1], "auto")) import_fps = 10000.0;
			else import_fps = atof(argv[i+1]);
			i++; 
		}
		else if (!stricmp(arg, "-agg")) { CHECK_NEXT_ARG agg_samples = atoi(argv[i+1]); i++; }
		else if (!stricmp(arg, "-keep-sys") || !stricmp(arg, "-keepsys")) keep_sys_tracks = 1;
		else if (!stricmp(arg, "-keep-all") || !stricmp(arg, "-keepall")) import_flags |= GF_IMPORT_KEEP_ALL_TRACKS;
		else if (!stricmp(arg, "-ms")) { CHECK_NEXT_ARG mediaSource = argv[i+1]; i++; }
		else if (!stricmp(arg, "-mp4")) { encode = 1; open_edit = 1; }
		else if (!stricmp(arg, "-saf")) { do_saf = 1; }
		else if (!stricmp(arg, "-log")) do_log = 1; 
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
		else if (!stricmp(arg, "-auto-quant")) {
			CHECK_NEXT_ARG
			opts.resolution = atoi(argv[i+1]);
			opts.auto_quant = 1;
			i++;
		}
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
		else if (!strcmp(arg, "-crypt")) {
			CHECK_NEXT_ARG
			ismaCrypt = 1;
			drm_file = argv[i+1];
			open_edit = 1;
			i += 1;
		}
		else if (!strcmp(arg, "-decrypt")) {
			CHECK_NEXT_ARG
			ismaCrypt = 2;
			if (get_file_type_by_ext(argv[i+1])!=1) {
				drm_file = argv[i+1];
				i += 1;
			}
			open_edit = 1;
		}
		else if (!stricmp(arg, "-set-kms")) {
			char szTK[20], *ext;
			CHECK_NEXT_ARG
			if (nb_track_act>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d track operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
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
		else if (!stricmp(arg, "-split")) { CHECK_NEXT_ARG split_duration = atof(argv[i+1]); i++; split_size = 0; }
		else if (!stricmp(arg, "-split-size") || !stricmp(arg, "-splits")) { CHECK_NEXT_ARG split_size = atoi(argv[i+1]); i++; split_duration = 0; }
		else if (!stricmp(arg, "-split-chunk") || !stricmp(arg, "-splitx")) { 
			CHECK_NEXT_ARG 
			if (!strstr(argv[i+1], ":")) {
				fprintf(stdout, "Chunk extraction usage: \"-splitx start->end\" expressed in seconds\n");
				return 1;
			}
			sscanf(argv[i+1], "%lf:%lf", &split_start, &split_duration);
			split_duration -= split_start; 
			split_size = 0;
			i++;
		}
		/*meta*/
		else if (!stricmp(arg, "-set-meta")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 0;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-add-item")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 1;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-rem-item")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 2;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-set-primary")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 3;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-set-xml")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 4;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-rem-xml")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 6;
			if (parse_meta_args(&metas[nb_meta_act], argv[i+1])) i++;
			nb_meta_act++;
			open_edit = 1;
		}
		else if (!stricmp(arg, "-dump-xml")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 7;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			i++;
		}
		else if (!stricmp(arg, "-dump-item")) { 
			CHECK_META_OPS
			metas[nb_meta_act].act_type = 8;
			parse_meta_args(&metas[nb_meta_act], argv[i+1]);
			nb_meta_act++;
			i++;
		}
		else if (!stricmp(arg, "-group-add") || !stricmp(arg, "-group-rem-track") || !stricmp(arg, "-group-rem")) { 
			tsel_acts[nb_tsel_acts].act_type = !stricmp(arg, "-group-rem") ? 2 : ( !stricmp(arg, "-group-rem-track") ? 1 : 0 );
			if (parse_tsel_args(tsel_acts, argv[i+1], &nb_tsel_acts)==0) {
				fprintf(stdout, "Invalid group syntax - check usage\n");
				return 1;
			}
			open_edit=1;
			i++;
		}
		else if (!stricmp(arg, "-group-clean")) { 
			tsel_acts[nb_tsel_acts].act_type = 3;
			nb_tsel_acts++;
			open_edit=1;
		}

		else if (!stricmp(arg, "-package")) {
			CHECK_NEXT_ARG 
			pack_file  = argv[i+1];
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
			if (nb_alt_brand_add>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d brand remove operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			brand_add[nb_alt_brand_add] = GF_4CC(b[0], b[1], b[2], b[3]);
			nb_alt_brand_add++;
			open_edit = 1;
			i++;
		}
		else if (!stricmp(arg, "-rb")) { 
			char *b = argv[i+1];
			CHECK_NEXT_ARG 
			if (nb_alt_brand_rem>=MAX_CUMUL_OPS) {
				fprintf(stdout, "Sorry - no more than %d brand remove operations allowed\n", MAX_CUMUL_OPS);
				return 1;
			}
			brand_rem[nb_alt_brand_rem] = GF_4CC(b[0], b[1], b[2], b[3]);
			nb_alt_brand_rem++;
			open_edit = 1;
			i++;
		}
#endif
		else if (!stricmp(arg, "-languages")) { 
			PrintLanguages();
			return 0;
		}
		else if (!stricmp(arg, "-h")) {
			if (i+1== (u32) argc) PrintUsage();
			else if (!strcmp(argv[i+1], "extract")) PrintExtractUsage();
			else if (!strcmp(argv[i+1], "dump")) PrintDumpUsage();
			else if (!strcmp(argv[i+1], "swf")) PrintSWFUsage();
#ifndef GPAC_READ_ONLY
			else if (!strcmp(argv[i+1], "general")) PrintGeneralUsage();
			else if (!strcmp(argv[i+1], "hint")) PrintHintUsage();
			else if (!strcmp(argv[i+1], "import")) PrintImportUsage();
			else if (!strcmp(argv[i+1], "format")) PrintFormats();
			else if (!strcmp(argv[i+1], "encode")) PrintEncodeUsage();
			else if (!strcmp(argv[i+1], "crypt")) PrintEncryptUsage();
			else if (!strcmp(argv[i+1], "meta")) PrintMetaUsage();
#endif
			else if (!strcmp(argv[i+1], "all")) {
#ifndef GPAC_READ_ONLY
				PrintGeneralUsage();
				PrintHintUsage();
				PrintImportUsage();
				PrintFormats();
				PrintEncodeUsage();
				PrintEncryptUsage();
				PrintMetaUsage();
#endif
				PrintExtractUsage();
				PrintDumpUsage();
				PrintSWFUsage();
			}
			else PrintUsage();
			return 0;
		}
		else if (!stricmp(arg, "-v")) verbose++;
		else if (!stricmp(arg, "-tag-list")) {
			fprintf(stdout, "Supported iTunes tag modifiers:\n");
			for (i=0; i<nb_itunes_tags; i++) {
				fprintf(stdout, "\t%s\t%s\n", itags[i].name, itags[i].comment);
			}
			return 0;
		} else {
			fprintf(stdout, "Option %s unknown. Please check usage\n", arg);
			return 1;
		}
	}
	if (!inName) {
		PrintUsage();
		return 1;
	}

	if (raw_cat) {
		char chunk[4096];
		FILE *fin, *fout;
		s64 to_copy, done;
		fin = gf_f64_open(raw_cat, "rb");
		if (!fin) return 1;
		fout = gf_f64_open(inName, "a+b");
		if (!fout) {
			fclose(fin);
			return 1;
		}
		gf_f64_seek(fin, 0, SEEK_END);
		to_copy = gf_f64_tell(fin);
		gf_f64_seek(fin, 0, SEEK_SET);
		done = 0;
		while (1) {
			u32 nb_bytes = fread(chunk, 1, 4096, fin);
			fwrite(chunk, 1, nb_bytes, fout);
			done += nb_bytes;
			fprintf(stdout, "Appending file %s - %02.2f done\r", raw_cat, 100.0*done/to_copy);
			if (done >= to_copy) break;
		}
		fclose(fin);
		fclose(fout);
		return 0;
	}

//	gf_log_set_level((verbose>1) ? GF_LOG_DEBUG : (verbose ? GF_LOG_INFO : GF_LOG_WARNING) );
	gf_log_set_level(verbose ? GF_LOG_DEBUG : GF_LOG_INFO);
	gf_log_set_tools(GF_LOG_CONTAINER|GF_LOG_SCENE|GF_LOG_PARSER|GF_LOG_AUTHOR|GF_LOG_CODING);
	if (quiet) {
		gf_log_set_level(0);
		gf_set_progress_callback(NULL, progress_quiet);

	}
	/*init libgpac*/
	gf_sys_init();


	if (do_saf && !encode) {
		switch (get_file_type_by_ext(inName)) {
		case 2: case 3: case 4: 
			encode = 1;
			break;
		}
	}

	if (dump_mode == 1 + GF_SM_DUMP_SVG) {
		if (strstr(inName, ".srt") || strstr(inName, ".ttxt")) import_subtitle = 2;
	}

	if (import_subtitle && !trackID) {
#ifndef GPAC_READ_ONLY
		GF_MediaImporter import;
		file = gf_isom_open("ttxt_convert", GF_ISOM_OPEN_WRITE, NULL);
		memset(&import, 0, sizeof(GF_MediaImporter));
		import.dest = file;
		import.in_name = inName;
		e = gf_media_import(&import);
		if (e) {
			fprintf(stdout, "Error importing %s: %s\n", inName, gf_error_to_string(e));
			gf_isom_delete(file);
			gf_delete_file("ttxt_convert");
			return 1;
		}
		strcpy(outfile, outName ? outName : inName);
		if (strchr(outfile, '.')) {
			while (outfile[strlen(outfile)-1] != '.') outfile[strlen(outfile)-1] = 0;
			outfile[strlen(outfile)-1] = 0;
		}
		dump_timed_text_track(file, gf_isom_get_track_id(file, 1), dump_std ? NULL : outfile, 1, (import_subtitle==2) ? 2 : dump_srt);
		gf_isom_delete(file);
		gf_delete_file("ttxt_convert");
		if (e) {
			fprintf(stdout, "Error converting %s: %s\n", inName, gf_error_to_string(e));
			return 1;
		}
		return 0;
#else
		fprintf(stdout, "Error: read-only version\n");
		return 1;
#endif
	}
#ifndef GPAC_READ_ONLY
	if (nb_add) {
		u8 open_mode = GF_ISOM_OPEN_EDIT;
		if (force_new) {
			open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
		} else {
			FILE *test = fopen(inName, "rb");
			if (!test) {
				open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
				if (!outName) outName = inName;
			}
			else fclose(test);
		}

		open_edit = 1;
		file = gf_isom_open(inName, open_mode, tmpdir);
		if (!file) {
			fprintf(stdout, "Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) );
			return 1;
		}
		for (i=0; i<nb_add; i++) {
			e = import_file(file, szTracksToAdd[i], import_flags, import_fps, agg_samples);
			if (e) {
				fprintf(stdout, "Error importing %s: %s\n", szTracksToAdd[i], gf_error_to_string(e));
				gf_isom_delete(file);
				return 1;
			}
		}
		/*unless explicitly asked, remove all systems tracks*/
		if (!keep_sys_tracks) remove_systems_tracks(file);
		needSave = 1;
		if (!conv_type && can_convert_to_isma(file)) conv_type = GF_ISOM_CONV_TYPE_ISMA;
	}

	if (nb_cat) {
		if (!file) {
			u8 open_mode = GF_ISOM_OPEN_EDIT;
			if (force_new) {
				open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
			} else {
				FILE *test = fopen(inName, "rb");
				if (!test) {
					open_mode = (do_flat) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
					if (!outName) outName = inName;
				}
				else fclose(test);
			}

			open_edit = 1;
			file = gf_isom_open(inName, open_mode, tmpdir);
			if (!file) {
				fprintf(stdout, "Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) );
				return 1;
			}
		}
		for (i=0; i<nb_cat; i++) {
			e = cat_isomedia_file(file, szFilesToCat[i], import_flags, import_fps, agg_samples, tmpdir, force_cat);
			if (e) {
				fprintf(stdout, "Error appending %s: %s\n", szFilesToCat[i], gf_error_to_string(e));
				gf_isom_delete(file);
				return 1;
			}
		}
		/*unless explicitly asked, remove all systems tracks*/
		if (!keep_sys_tracks) remove_systems_tracks(file);

		needSave = 1;
		if (conv_type && can_convert_to_isma(file)) conv_type = GF_ISOM_CONV_TYPE_ISMA;
	}
#endif
#ifndef GPAC_READ_ONLY
	else if (chunk_mode) {
		if (!inName) {
			fprintf(stdout, "chunk encoding syntax: [-outctx outDump] -inctx inScene auFile\n");
			return 1;
		}
		e = EncodeFileChunk(inName, outName ? outName : inName, input_ctx, output_ctx, tmpdir);
		if (e) fprintf(stdout, "Error encoding chunk file %s\n", gf_error_to_string(e));
		return e ? 1 : 0;
	}
#endif
	else if (encode) {
#ifndef GPAC_READ_ONLY
		FILE *logs = NULL;
		if (do_log) {
			char logfile[5000];
			strcpy(logfile, inName);
			if (strchr(logfile, '.')) {
				while (logfile[strlen(logfile)-1] != '.') logfile[strlen(logfile)-1] = 0;
				logfile[strlen(logfile)-1] = 0;
			}
			strcat(logfile, "_enc.logs");
			logs = fopen(logfile, "wt");
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
#else
		fprintf(stdout, "Cannot encode file - read-only release\n");
#endif
		needSave = 1;
		if (do_saf) {
			needSave = 0;
			open_edit = 0;
		}
	} 
#ifndef GPAC_READ_ONLY
	else if (pack_file) {
		char *fileName = strchr(pack_file, ':');
		if (fileName && ((fileName - pack_file)==4)) {
			fileName[0] = 0;
			file = package_file(fileName + 1, pack_file, tmpdir);
			fileName[0] = ':';
		} else {
			file = package_file(pack_file, NULL, tmpdir);
		}
		if (!outName) outName = inName;
		needSave = 1;
		open_edit = 1;
	} 
#endif
	else if (!file && !(track_dump_type & GF_EXPORT_AVI_NATIVE)) {
		FILE *st = fopen(inName, "rb");
		Bool file_exists = 0;
		if (st) {
			file_exists = 1;
			fclose(st);
		}
		switch (get_file_type_by_ext(inName)) {
		case 1:
			file = gf_isom_open(inName, (u8) (open_edit ? GF_ISOM_OPEN_EDIT : ( (dump_isom>0) ? GF_ISOM_OPEN_READ_DUMP : GF_ISOM_OPEN_READ) ), tmpdir);
			if (!file) {
				if (open_edit && nb_meta_act) {
					file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, tmpdir);
					if (!outName && file) outName = inName;
				}

				if (!file) {
					fprintf(stdout, "Error opening file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)));
					return 1;
				}
			}
			break;
		/*allowed for bt<->xmt*/
		case 2:
		case 3:
		/*allowed for svg->lsr**/
		case 4:
		/*allowed for swf->bt, swf->xmt*/
		case 5:
			break;
		/*used for .saf / .lsr dump*/
		case 6:
			if ((dump_mode==1+GF_SM_DUMP_LASER) || (dump_mode==1+GF_SM_DUMP_SVG)) {
				break;
			}

		default:
			if (!open_edit && file_exists && !gf_isom_probe_file(inName) && track_dump_type) {
			} 
#ifndef GPAC_READ_ONLY
			else if (!open_edit && file_exists /* && !gf_isom_probe_file(inName) */ && !dump_mode) {
				if (dump_m2ts) {
					dump_mpeg2_ts(inName, pes_dump);
				} else {
					convert_file_info(inName, info_track_id);
				}
				return 0;
			}
#endif
			else if (open_edit) {
				file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, tmpdir);
				if (!outName && file) outName = inName;
			} else if (!file_exists) {
				fprintf(stdout, "Error creating file %s: %s\n", inName, gf_error_to_string(GF_URL_ERROR));
				return 1;
			} else {
				fprintf(stdout, "Cannot open %s - extension not supported\n", inName);
				return 1;
			}
		}
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

#ifndef GPAC_READ_ONLY
	if (track_dump_type & GF_EXPORT_AVI_NATIVE) {
		char szFile[1024];
		GF_MediaExporter mdump;
		memset(&mdump, 0, sizeof(mdump));
		mdump.in_name = inName;
		mdump.flags = GF_EXPORT_AVI_NATIVE;
		mdump.trackID = trackID;
		if (trackID>2) {
			sprintf(szFile, "%s_audio%d", outfile, trackID-1);
		} else {
			sprintf(szFile, "%s_%s", outfile, (trackID==1) ? "video" : "audio");
		}
		mdump.out_name = szFile;
		e = gf_media_export(&mdump);
		if (e) goto err_exit;
		return 0;
	}
	if (!open_edit && !gf_isom_probe_file(inName) && track_dump_type) {
		GF_MediaExporter mdump;
		memset(&mdump, 0, sizeof(mdump));
		mdump.in_name = inName;
		mdump.flags = track_dump_type;
		mdump.trackID = trackID;
		mdump.out_name = outfile;
		e = gf_media_export(&mdump);
		if (e) goto err_exit;
		return 0;
	} 

	if (dump_mode) {
		e = dump_file_text(inName, dump_std ? NULL : outfile, dump_mode-1, do_log);
		if (e) goto err_exit;
	}
	if (stat_level) dump_scene_stats(inName, dump_std ? NULL : outfile, stat_level);
#endif
	if (!HintIt && print_sdp) DumpSDP(file, dump_std ? NULL : outfile);
	if (print_info) {
		if (info_track_id) DumpTrackInfo(file, info_track_id, 1);
		else DumpMovieInfo(file);
	}
	if (dump_isom) dump_file_mp4(file, dump_std ? NULL : outfile);
	if (dump_rtp) dump_file_rtp(file, dump_std ? NULL : outfile);
	if (dump_ts) dump_file_ts(file, dump_std ? NULL : outfile);
	if (dump_cr) dump_file_ismacryp(file, dump_std ? NULL : outfile);
	if ((dump_ttxt || dump_srt) && trackID) dump_timed_text_track(file, trackID, dump_std ? NULL : outfile, 0, dump_srt);
	if (do_hash) {
		u8 hash[20];
		e = gf_media_get_file_hash(inName, hash);
		if (e) goto err_exit;
		fprintf(stdout, "File %s hash (SHA-1): ", inName);
		for (i=0; i<20; i++) fprintf(stdout, "%02X", hash[i]);
		fprintf(stdout, "\n");
	}
	if (dump_cart) dump_cover_art(file, outfile);

#ifndef GPAC_READ_ONLY
	if (split_duration || split_size) {
		split_isomedia_file(file, split_duration, split_size, inName, InterleavingTime, split_start, tmpdir, outName);
		/*never save file when splitting is desired*/
		open_edit = 0;
	}
	if (do_saf || track_dump_type) {
		char szFile[1024];
		GF_MediaExporter mdump;
		memset(&mdump, 0, sizeof(mdump));
		mdump.file = file;
		mdump.flags = do_saf ? GF_EXPORT_SAF : track_dump_type;
		if (!do_saf) {
			mdump.trackID = trackID;
			mdump.sample_num = raw_sample_num;
			if (outName) {
				mdump.out_name = outName;
				mdump.flags |= GF_EXPORT_MERGE;
			} else {
				sprintf(szFile, "%s_track%d", outfile, trackID);
				mdump.out_name = szFile;
			}
		} else {
			mdump.out_name = outfile;
		}
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
#ifndef GPAC_READ_ONLY
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
			e = gf_isom_remove_meta_xml(file, meta->root_meta, tk);
			needSave = 1;
			break;
		case 8:
			e = gf_isom_extract_meta_item(file, meta->root_meta, tk, meta->item_id, strlen(meta->szPath) ? meta->szPath : NULL);
			break;
#endif
		case 7:
			e = gf_isom_extract_meta_xml(file, meta->root_meta, tk, meta->szPath, NULL);
			break;
		}
		if (e) goto err_exit;
	}
	if (!open_edit) {
		if (file) gf_isom_delete(file);
		gf_sys_close();
		return 0;
	}

#ifndef GPAC_READ_ONLY

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
		remove_systems_tracks(file);
		needSave = 1;
		if (conv_type < GF_ISOM_CONV_TYPE_ISMA_EX) conv_type = 0;
	}
	if (remove_root_od) {
		gf_isom_remove_root_od(file);
		needSave = 1;
	}
	if (remove_hint) {
		for (i=0; i<gf_isom_get_track_count(file); i++) {
			if (gf_isom_get_media_type(file, i+1) == GF_ISOM_MEDIA_HINT) {
				fprintf(stdout, "Removing hint track ID %d\n", gf_isom_get_track_id(file, i+1));
				gf_isom_remove_track(file, i+1);
				i--;
			}
		}
		gf_isom_sdp_clean(file);
		needSave = 1;
	}


	if (!encode) {
		if (!file) {
			fprintf(stdout, "Nothing to do - exiting\n");
			gf_sys_close();
			return 0;
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
		if ((conv_type == GF_ISOM_CONV_TYPE_ISMA) || (conv_type == GF_ISOM_CONV_TYPE_ISMA_EX)) {
			fprintf(stdout, "Converting to ISMA Audio-Video MP4 file...\n");
			/*keep ESIDs when doing ISMACryp*/
			e = gf_media_make_isma(file, ismaCrypt ? 1 : 0, 0, (conv_type==GF_ISOM_CONV_TYPE_ISMA_EX) ? 1 : 0);
			if (e) goto err_exit;
			needSave = 1;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_3GPP) {
			fprintf(stdout, "Converting to 3GP file...\n");
			e = gf_media_make_3gpp(file);
			if (e) goto err_exit;
			needSave = 1;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_PSP) {
			fprintf(stdout, "Converting to PSP file...\n");
			e = gf_media_make_psp(file);
			if (e) goto err_exit;
			needSave = 1;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_IPOD) {
			u32 major_brand = 0;

			fprintf(stdout, "Setting up iTunes/iPod file...\n");

			for (i=0; i<gf_isom_get_track_count(file); i++) {
				u32 mType = gf_isom_get_media_type(file, i+1);
				switch (mType) {
				case GF_ISOM_MEDIA_VISUAL:
					major_brand = GF_4CC('M','4','V',' ');
					gf_isom_set_ipod_compatible(file, i+1);
					if (gf_isom_get_media_subtype(file, i+1, 1) == GF_ISOM_SUBTYPE_AVC_H264) {
						fprintf(stdout, "Forcing AVC/H264 SAR to 1:1...\n");
						gf_media_change_par(file, i+1, 1, 1);
					}
					break;
				case GF_ISOM_MEDIA_AUDIO:
					if (!major_brand) major_brand = GF_4CC('M','4','A',' ');
					else gf_isom_modify_alternate_brand(file, GF_4CC('M','4','A',' '), 1);
					break;
				}
			}
			gf_isom_set_brand_info(file, major_brand, 0);
			gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_MP42, 1);
			needSave = 1;
		}
		if (ismaCrypt) {
			if (ismaCrypt == 1) {
				if (!drm_file) {
					fprintf(stdout, "Missing DRM file location - usage '-%s drm_file input_file\n", (ismaCrypt==1) ? "crypt" : "decrypt");
					e = GF_BAD_PARAM;
					goto err_exit;
				}
				e = gf_ismacryp_crypt_file(file, drm_file);
			} else if (ismaCrypt ==2) {
				e = gf_ismacryp_decrypt_file(file, drm_file);
			}
			if (e) goto err_exit;
			needSave = 1;
		}
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
				fprintf(stdout, "Error Removing track ID %d: %s\n", tka->trackID, gf_error_to_string(e));
			} else {
				fprintf(stdout, "Removing track ID %d\n", tka->trackID);
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
				if (tka->delay_ms>0) {
					gf_isom_append_edit_segment(file, track, (timescale*tka->delay_ms)/1000, 0, GF_ISOM_EDIT_EMPTY);
					gf_isom_append_edit_segment(file, track, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
					needSave = 1;
				} else {
					u64 to_skip = (timescale*(-tka->delay_ms))/1000;
					if (to_skip<tk_dur) {
						u64 seg_dur = (-tka->delay_ms)*gf_isom_get_media_timescale(file, track) / 1000;
						gf_isom_append_edit_segment(file, track, tk_dur-to_skip, seg_dur, GF_ISOM_EDIT_NORMAL);
					} else {
						fprintf(stdout, "Warning: request negative delay longer than track duration - ignoring\n");
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
				fprintf(stdout, "Invalid iTune tag format \"%s\" - ignoring\n", tags);
				tags = NULL;
				continue;
			}
			itag = itags[itag].code;

			val = strchr(tags, '=');
			if (!val) {
				fprintf(stdout, "Invalid iTune tag format \"%s\" (expecting '=') - ignoring\n", tags);
				tags = NULL;
				continue;
			}
			if (val) {
				val ++;
				if ((val[0]==':') || !val[0] || !stricmp(val, "NULL") ) val = NULL;
			}

			tlen = val ? strlen(val) : 0;
			switch (itag) {
			case GF_ISOM_ITUNE_COVER_ART:
			{
				char *d, *ext;
				FILE *t = fopen(val, "rb");
				fseek(t, 0, SEEK_END);
				tlen = ftell(t);
				fseek(t, 0, SEEK_SET);
				d = malloc(sizeof(char) * tlen);
				fread(d, tlen, 1, t);
				fclose(t);
				
				ext = strrchr(val, '.');
				if (!stricmp(ext, ".png")) tlen |= 0x80000000;
				e = gf_isom_apple_set_tag(file, GF_ISOM_ITUNE_COVER_ART, d, tlen);
				free(d);
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
					gf_isom_apple_set_tag(file, itag, val, strlen(val) );
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
				if (sscanf(val, "%d/%d", &n, &t) == 2) { _t[3]=n; _t[5]=t;}
				else if (sscanf(val, "%d", &n) == 1) { _t[3]=n;}
				else tlen = 0;
				if (tlen) gf_isom_apple_set_tag(file, itag, _t, tlen);
			}
				break;
			case GF_ISOM_ITUNE_GAPELESS:
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

	if (Frag) {
		if (!InterleavingTime) InterleavingTime = 0.5;
		if (HintIt) fprintf(stdout, "Warning: cannot hint and fragment - ignoring hint\n");
		fprintf(stdout, "Fragmenting file (%.3f seconds fragments)\n", InterleavingTime);
		e = gf_media_fragment_file(file, outfile, InterleavingTime);
		if (e) fprintf(stdout, "Error while fragmenting file: %s\n", gf_error_to_string(e));
		gf_isom_delete(file);
		if (!e && !outName && !force_new) {
			if (remove(inName)) fprintf(stdout, "Error removing file %s\n", inName);
			else if (rename(outfile, inName)) fprintf(stdout, "Error renaming file %s to %s\n", outfile, inName);
		}
		gf_sys_close();
		return (e!=GF_OK) ? 1 : 0;
	}
	
	if (HintIt) {
		if (force_ocr) SetupClockReferences(file);
		fprintf(stdout, "Hinting file with Path-MTU %d Bytes\n", MTUSize);
		MTUSize -= 12;		
		e = HintFile(file, MTUSize, max_ptime, rtp_rate, hint_flags, HintCopy, HintInter, regular_iod);
		if (e) goto err_exit;
		needSave = 1;
		if (print_sdp) DumpSDP(file, dump_std ? NULL : outfile);
	}

	/*full interleave (sample-based) if just hinted*/
	if (FullInter) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_TIGHT);
	} else if (!InterleavingTime) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_STREAMABLE);
		needSave = 1;
	} else if (do_flat) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_FLAT);
		needSave = 1;
	} else {
		e = gf_isom_make_interleave(file, InterleavingTime);
		if (!e && !old_interleave) e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_DRIFT_INTERLEAVED);
	}
	if (e) goto err_exit;

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

	if (cprt) {
		e = gf_isom_set_copyright(file, "und", cprt);
		needSave = 1;
		if (e) goto err_exit;
	}
	if (chap_file) {
		e = gf_media_import_chapters(file, chap_file, import_fps);
		needSave = 1;
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
		gf_isom_modify_alternate_brand(file, brand_add[i], 0);
		needSave = 1;
	}

	if (!encode && !force_new) gf_isom_set_final_name(file, outfile);
	if (needSave) {
		if (outName) {
			fprintf(stdout, "Saving to %s: ", outfile);
			gf_isom_set_final_name(file, outfile);
		} else if (encode || pack_file) {
			fprintf(stdout, "Saving to %s: ", gf_isom_get_filename(file) );
		} else {
			fprintf(stdout, "Saving %s: ", inName);
		}
		if (HintIt && FullInter) fprintf(stdout, "Hinted file - Full Interleaving\n");
		else if (FullInter) fprintf(stdout, "Full Interleaving\n");
		else if (do_flat || !InterleavingTime) fprintf(stdout, "Flat storage\n");
		else fprintf(stdout, "%.3f secs Interleaving%s\n", InterleavingTime, old_interleave ? " - no drift control" : "");

		e = gf_isom_close(file);

		if (!e && !outName && !encode && !force_new && !pack_file) {
			if (remove(inName)) fprintf(stdout, "Error removing file %s\n", inName);
			else if (rename(outfile, inName)) fprintf(stdout, "Error renaming file %s to %s\n", outfile, inName);
		}
	} else {
		gf_isom_delete(file);
	}
	/*close libgpac*/
	gf_sys_close();

	if (e) fprintf(stdout, "Error: %s\n", gf_error_to_string(e));
	return (e!=GF_OK) ? 1 : 0;
#else
	/*close libgpac*/
	gf_sys_close();
	gf_isom_delete(file);
	fprintf(stdout, "Error: Read-only version of MP4Box.\n");
	return 1;
#endif
err_exit:
	/*close libgpac*/
	gf_sys_close();
	if (file) gf_isom_delete(file);
	fprintf(stdout, "\n\tError: %s\n", gf_error_to_string(e));
	return 1;
}
