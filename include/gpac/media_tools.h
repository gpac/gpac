/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Authoring Tools sub-project
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

#ifndef _GF_MEDIA_H_
#define _GF_MEDIA_H_

/*!
\file <gpac/media_tools.h>
\brief media tools helper for importing, exporting and analysing
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/isomedia.h>
#include <gpac/avparse.h>
#include <gpac/config_file.h>
#include <gpac/filters.h>

/*!
\addtogroup mt_grp ISOBMF Helper tools
\ingroup media_grp
\brief media tools helper for importing, exporting and analysing.

This section documents media tools  functions .

@{
 */


/*!
 *computes file hash.
 If file is ISOBMFF based, computes hash according to OMA (P)DCF (without MutableDRMInformation box).
 Otherwise this is equivalent to \ref gf_sha1_file
 
\param file the source file to hash
\param hash the 20 bytes buffer in which sha128 is performed for this file
\return error if any.
 */
GF_Err gf_media_get_file_hash(const char *file, u8 hash[20]);

#ifndef GPAC_DISABLE_ISOM
/*!
 Creates (if needed) a GF_ESD for the given track - THIS IS RESERVED for local playback
only, since the OTI/codecid used when emulated is not standard...
\param isom_file source file
\param trackNumber track for which the esd is to be emulated
\param sampleDescriptionIndex indicates the default sample description to map. 0 is equivalent to 1, first sample description
\return rebuilt ESD. It is the caller responsibility to delete it.
 */
GF_ESD *gf_media_map_esd(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*!
Creates (if needed) a GF_ESD for the given image item - THIS IS RESERVED for local playback
only, since the OTI/codecid used when emulated is not standard...
\param mp4 source file
\param item_id item for which the esd is to be emulated
\return rebuilt ESD. It is the caller responsibility to delete it.
*/
GF_ESD *gf_media_map_item_esd(GF_ISOFile *mp4, u32 item_id);

/*!
 * Get RFC 6381 description for a given track.
\param isom_file source ISOBMF file
\param trackNumber track to check
 \param sample_desc_index sample description index to check
\param szCodec a pointer to an already allocated string of size RFC6381_CODEC_NAME_SIZE_MAX bytes.
\param force_inband_xps force inband signaling of parameter sets.
\param force_sbr forces using explicit signaling for SBR.
\return error if any.
 */
GF_Err gf_media_get_rfc_6381_codec_name(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_desc_index, char *szCodec, Bool force_inband_xps, Bool force_sbr);
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE
/*!
 *Changes pixel aspect ratio for visual tracks if supported. Negative values remove any PAR info
\param isom_file target ISOBMF file
\param trackNumber target track
\param ar_num aspect ratio numerator. A value of 0 removes PAR info, a value of -1 gets info from video bitstream for BOTH num and den
\param ar_den aspect ratio denominator. A value of 0 removes PAR info, a value of -1 gets info from video bitstream for BOTH num and den
\param force_par aspect ratio is always written even when 1:1, otherwise aspect ratio info is removed if 1:1
\param rewrite_par aspect ratio is modified in bitstream. Ignored if ar_num or ar_den are not strictly positive
\return error if any
 */
GF_Err gf_media_change_par(GF_ISOFile *isom_file, u32 trackNumber, s32 ar_num, s32 ar_den, Bool force_par, Bool rewrite_par);

/*! Changes color property of the media (bitstream rewrite) - only AVC/H264 supported for now. See CICP for value types
Negative values keep source settings for the corresponding flags.
If source stream has no VUI info, create one and set corresponding flags to specified values.
In this case, any other flags are set to preferred values (typically, flag=0 or value=undef).
\param isom_file target ISOBMF file
\param trackNumber target track
\param fullrange fullrange flag
\param video_format video format type
\param color_primaries color primaries
\param transfer transfer characteristics
\param color_matrix olor matrix
\return error if any
*/
GF_Err gf_media_change_color(GF_ISOFile *isom_file, u32 trackNumber, s32 fullrange, s32 video_format, s32 color_primaries, s32 transfer, s32 color_matrix);

/*!
 *Removes all non rap samples (sync and other RAP sample group info) from the track.
\param isom_file target ISOBMF file
\param trackNumber target track
\param non_ref_only if GF_TRUE, removes only non-reference pictures
\return error if any
 */
GF_Err gf_media_remove_non_rap(GF_ISOFile *isom_file, u32 trackNumber, Bool non_ref_only);

/*!
 *updates bitrate info on given track.
\param isom_file target ISOBMF file
\param trackNumber target track
 */
void gf_media_update_bitrate(GF_ISOFile *isom_file, u32 trackNumber);


/*! gets AV1 scalable layer byte offsets of a sample for a1lx box
\param isom_file the target ISO file
\param trackNumber the target track
\param sample_number the target sample to query
\param op_index AV1 operating point index to retrieve sizes for
\param layer_size returned 3 layer sizes (4th is implied, see a1lx spec)
\return error if any
*/
GF_Err gf_media_av1_layer_size_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_number, u8 op_index, u32 layer_size[3]);


/*! sets keys from name and value, as defined in MP4Box -h tags
\param isom_file the target ISO file
\param name the tag name
\param value the tag value
\return error if any
*/
GF_Err gf_media_isom_apply_qt_key(GF_ISOFile *isom_file, const char *name, const char *value);

#endif

/*! @} */


/*!
\addtogroup mimp_grp ISOBMF Importers
\ingroup media_grp
\brief Media importing.

This section documents media tools helper functions for importing, exporting and analysing.

@{
*/

/*!
Default import FPS for video when no VUI/timing information is found
\hideinitializer
*/
#define GF_IMPORT_DEFAULT_FPS	25.0


/*
	All these can import a file into a dedicated track. If esd is NULL the track is blindly added
	otherwise it is added with the requested ESID if non-0, otherwise the new trackID is stored in ESID
	if use_data_ref is set, data is only referenced in the file
	if duration is not 0, only the first duration seconds are imported
	\note If an ESD is specified, its decoderSpecificInfo is also updated

*/

/*!
Track importer flags
\hideinitializer
*/
enum
{
	/*! references data rather than copy, whenever possible*/
	GF_IMPORT_USE_DATAREF = 1,
	/*! for AVI video: imports at constant FPS (eg imports N-Vops due to encoder drops)*/
	GF_IMPORT_NO_FRAME_DROP = 1<<1,
	/*! for CMP ASP only: forces treating the input as packed bitsream and discards all n-vops*/
	GF_IMPORT_FORCE_PACKED = 1<<2,
	/*! for AAC audio: forces SBR mode with implicit signaling (backward compatible)*/
	GF_IMPORT_SBR_IMPLICIT = 1<<3,
	/*! for AAC audio: forces SBR mode with explicit signaling (non-backward compatible).
	Will override GF_IMPORT_SBR_IMPLICIT flag when set*/
	GF_IMPORT_SBR_EXPLICIT = 1<<4,
	/*! forces MPEG-4 import - some 3GP2 streams have both native IsoMedia sample description and MPEG-4 one possible*/
	GF_IMPORT_FORCE_MPEG4 = 1<<5,
	/*! special flag for text import at run time (never set on probe), indicates to leave the text box empty
	so that we dynamically adapt to display size*/
	GF_IMPORT_SKIP_TXT_BOX = 1<<6,

	/*! uses compact size in .MOV/.IsoMedia files*/
	GF_IMPORT_USE_COMPACT_SIZE = 1<<8,
	/*! don't add a final empty sample when importing text tracks from srt*/
	GF_IMPORT_NO_TEXT_FLUSH = 1<<9,
	/*! for SVC or LHVC video: forces explicit SVC / LHVC signaling */
	GF_IMPORT_SVC_EXPLICIT = 1<<10,
	/*! for SVC / LHVC video: removes all SVC / LHVC extensions*/
	GF_IMPORT_SVC_NONE = 1<<11,
	/*! for AAC audio: forces PS mode with implicit signaling (backward compatible)*/
	GF_IMPORT_PS_IMPLICIT = 1<<12,
	/*! for AAC audio: forces PS mode with explicit signaling (non-backward compatible).
	Will override GF_IMPORT_PS_IMPLICIT flag when set*/
	GF_IMPORT_PS_EXPLICIT = 1<<13,
	/*! oversampled SBR */
	GF_IMPORT_OVSBR = 1<<14,
	/*! set subsample information with SVC*/
	GF_IMPORT_SET_SUBSAMPLES = 1<<15,
	/*! force to mark non-IDR frames with sync data (I slices,) to be marked as sync points points
	THE RESULTING FILE IS NOT COMPLIANT*/
	GF_IMPORT_FORCE_SYNC = 1<<16,
	/*! keep trailing 0 bytes in AU payloads when any*/
	GF_IMPORT_KEEP_TRAILING = 1<<17,

	/*! do not compute edit list for B-frames video tracks*/
	GF_IMPORT_NO_EDIT_LIST = 1<<19,
	/*! when set, only updates tracks info and return*/
	GF_IMPORT_PROBE_ONLY	= 1<<20,
	/*! only set when probing, signals several frames per sample possible*/
	GF_IMPORT_3GPP_AGGREGATION = 1<<21,
	/*! only set when probing, signals video FPS overridable*/
	GF_IMPORT_OVERRIDE_FPS = 1<<22,
	/*! only set when probing, signals duration not usable*/
	GF_IMPORT_NO_DURATION = 1<<23,
	/*! when set IP packets found in MPE sections will be sent to the local network */
	GF_IMPORT_MPE_DEMUX = 1<<24,
	/*! when set HEVC VPS is rewritten to remove VPS extensions*/
	GF_IMPORT_NO_VPS_EXTENSIONS = 1<<25,
	/*! when set no SEI messages are imported*/
	GF_IMPORT_NO_SEI = 1<<26,
	/*! keeps track references when importing a single track*/
	GF_IMPORT_KEEP_REFS = 1<<27,
	/*! keeps AV1 temporal delimiter OBU in the samples*/
	GF_IMPORT_KEEP_AV1_TEMPORAL_OBU  = 1<<28,
	/*! imports sample dependencies information*/
	GF_IMPORT_SAMPLE_DEPS  = 1<<29,
	
	//GF_IMPORT_FILTER_STATS = 0x80000000	//(=1<<31)
};

#ifndef GPAC_DISABLE_MEDIA_IMPORT

/*! max supported numbers of tracks in importer*/
#define GF_IMPORT_MAX_TRACKS	100
/*!
Track info for video media
\hideinitializer
 */
struct __track_video_info
{
	/*! video width in coded samples*/
	u32 width;
	/*! video height in coded samples*/
	u32 height;
	/*! pixel aspect ratio expressed as 32 bits, high 16 bits being the numerator and low ones being the denominator*/
	u32 par;
	/*! temporal enhancement flag*/
	Bool temporal_enhancement;
	/*! Video frame rate*/
	Double FPS;
};
/*!
Track info for audio media
\hideinitializer
*/
struct __track_audio_info
{
	/*! audio sample rate*/
	u32 sample_rate;
	/*! number of channels*/
	u32 nb_channels;
	/*! samples per frame*/
	u32 samples_per_frame;
};

/*!
* Track info for any media
*/
struct __track_import_info
{
	/*! ID of the track (PID, TrackID, etc ...)*/
	u32 track_num;
	/*! stream type (one of GF_STREAM_XXXX)*/
	u32 stream_type;
	/*! codec ID ( one of GF_CODECID_XXX*)*/
	u32 codecid;
	/*! GF_ISOM_MEDIA_* : vide, auxv, pict*/
	u32 media_subtype;
	Bool is_chapter;
	/*! video format info*/
	struct __track_video_info video_info;
	/*! audio format info*/
	struct __track_audio_info audio_info;
	/*! codec profile according to 6381*/
	char szCodecProfile[20];
	/*! language of the media, 0/'und ' if not known */
	u32 lang;
	/*! MPEG-4 ES-ID, only used for MPEG4 on MPEG-2 TS*/
	u32 mpeg4_es_id;
	/*! Program number for MPEG2 TS*/
	u16 prog_num;
};

/*!
 * Program info for the source file/stream
 */
struct __program_import_info
{
	/*! program number, as used in MPEG-2 TS*/
	u32 number;
	/*! program name*/
	char name[40];
};

/*!
 * Track importer object
 */
typedef struct __track_import
{
	/*! destination ISOBMFF file where the media is to be imported */
	GF_ISOFile *dest;
	/*! media to import:
		MP4/ISO media: trackID
		AVI files:
			0: first video and first audio,
			1: video track
			2->any: audio track(s)
		MPEG-PS files with N video streams:
			0: first video and first audio
			1->N: video track
			N+1->any: audio track
	TrackNums can be obtain with probing
	*/
	u32 trackID;
	/*! media source - selects importer type based on extension*/
	char *in_name;
	/*! import duration if any*/
	GF_Fraction duration;
	/*! importer flags*/
	u32 flags;
	/*! importer swf flags*/
	u32 swf_flags;
	/*! importer swf flatten angle when converting curves*/
	Float swf_flatten_angle;
	/*! Forced video FPS (CMP, AVI, OGG, H264) - also used by SUB import. Ignored if 0*/
	GF_Fraction video_fps;
	/*! optional ESD to be updated by the importer (used for BT/XMT import)*/
	GF_ESD *esd;
	/*! optional format indication for media source (used in IM1 reference software)*/
	char *streamFormat;
	/*! frame per sample cumulation (3GP media only) - MAX 15, ignored when data ref is used*/
	u32 frames_per_sample;
	/*! track ID of imported media in destination file*/
	u32 final_trackID;
	/*! optional format indication for media source (used in IM1)*/
	char *force_ext;
	/*! for MP4 import only, the source MP4 to be used*/
	GF_ISOFile *orig;

	/*! default font size for text import*/
	u32 fontSize;
	/*! default font name for text import*/
	char *fontName;
	/*! width of the imported text track */
	u32 text_track_width;
	/*! height of the imported text track */
	u32 text_track_height;
	/*! width of the imported text display area (as indicated in text sample description) */
	u32 text_width;
	/*! height of the imported text display area (as indicated in text sample description) */
	u32 text_height;
	/*! horizontal offset of the imported text display area (as indicated in text sample description) */
	s32 text_x;
	/*! vertical offset of the imported text display area (as indicated in text sample description) */
	s32 text_y;

	/*! Initial offset of the first AU to import. Only used for still images and AFX streams*/
	Double initial_time_offset;

	/*! number of tracks after probing - may be set to 0, in which case no track
	selection can be performed. It may also be inaccurate if probing doesn't
	detect all available tracks (cf ogg import)*/
	u32 nb_tracks;
	/*! track info after probing (GF_IMPORT_PROBE_ONLY set).*/
	struct __track_import_info tk_info[GF_IMPORT_MAX_TRACKS];
	/*! duration of the probe for MPEG_2 TS cases.*/
	u64 probe_duration;

	/*! for MPEG-TS and similar: number of program info*/
	u32 nb_progs;
	/*! for MPEG-TS and similar: program info*/
	struct __program_import_info pg_info[GF_IMPORT_MAX_TRACKS];
	/*! last error encountered during import, internal to the importer*/
	GF_Err last_error;
	/*! any filter options to pass to source*/
	const char *filter_src_opts;
	/*! any filter options to pass to sink*/
	const char *filter_dst_opts;
	/*! filter chain to insert before destination, formatted as "f1[:args]@f2[:args]" options to pass to sink*/
	const char *filter_chain;
	Bool is_chain_old_syntax;

	/*! force mode for the created  ISOBMFF sample entry*/
	GF_AudioSampleEntryImportMode asemode;

	/*! indicate to tag the imported media as an alpha channel stream*/
	Bool is_alpha;
	/*! keep AU delimiter in file if allowed by specification*/
	Bool keep_audelim;
	/*! import as NAL-based video using inband parameter sets*/
	u32 xps_inband;
	/*! flag for session stats and graph dumping*/
	u32 print_stats_graph;
	/*! target program ID of source MPEG-2 stream to import*/
	u32 prog_id;

	/*! target timescale to set*/
	s32 moov_timescale;

	/*! value for created track
		0: let importer decide
		0xFFFFFFFF: try to keep source ID
		other value: trackk ID value
	*/
	u32 target_trackID;

	/*magic number for identifying source, will be set to the destination track. Only the low 32 bits are used
	the high 32 bits are updated by the importer as follows:
		1<<33: if bit is set, source was an isobmff file
	*/
	u64 source_magic;
	/*! the session in which to add the importer (for -new-fs option only). If null, the importer runs its own session right away*/
	GF_FilterSession *run_in_session;
	/*! muxer arguments when running multiple importers in one session*/
	char *update_mux_args;
	/*! muxer source ID argument when running multiple importers in one session*/
	char *update_mux_sid;
	/*! index of source importer when running multiple importers in one session*/
	u32 track_index;
	/*! target start time in source*/
	Double start_time;
} GF_MediaImporter;

/*!
 * Imports a media file
\param importer the importer object
\return error if any
 */
GF_Err gf_media_import(GF_MediaImporter *importer);


/*!
 Adds chapter info contained in file
\param isom_file target ISOBMF file
\param chap_file target chapter file
\param import_fps specifies the chapter frame rate (optional, ignored if 0 - defaults to 25). Most formats don't use this feature
\param for_qt use QT signaling for chapter tracks
\return error if any
 */
GF_Err gf_media_import_chapters(GF_ISOFile *isom_file, char *chap_file, GF_Fraction import_fps, Bool for_qt);

/*!
  Make the file ISMA compliant: creates ISMA BIFS / OD tracks if needed, and update audio/video IDs
the file should not contain more than one audio and one video track
\param isom_file the target ISOBMF file
\param keepESIDs if true, ES IDs are not changed.
\param keepImage if true, keeps image tracks
\param no_ocr if true, doesn't write clock references in MPEG-4 system info
\return error if any
*/
GF_Err gf_media_make_isma(GF_ISOFile *isom_file, Bool keepESIDs, Bool keepImage, Bool no_ocr);

/*!
 Make the file 3GP compliant && sets profile
\param isom_file the target ISOBMF file
\return error if any
 */
GF_Err gf_media_make_3gpp(GF_ISOFile *isom_file);

/*!
 make the file playable on a PSP
\param isom_file the target ISOBMF file
\return error if any
*/
GF_Err gf_media_make_psp(GF_ISOFile *isom_file);

/*!
 adjust file params for QT prores
\param qt_file the target QT file
\return error if any
*/
GF_Err gf_media_check_qt_prores(GF_ISOFile *qt_file);

/*! @} */

/*!
\addtogroup mnal_grp AVC and HEVC ISOBMFF tools
\ingroup media_grp
\brief Manipulation AVC and HEVC tracks in ISOBMFF.

This section documents functions for manipulating AVC and HEVC tracks in ISOBMFF.

@{
*/

/*!
 Changes the profile (if not 0) and level (if not 0) indication of the media - only AVC/H264 supported for now
\param isom_file the target ISOBMF file
\param trackNumber the target track
\param profile the new profile to set
\param compat profile compatibility flag for H264
\param level the new level to set
\return error if any
 */
GF_Err gf_media_change_pl(GF_ISOFile *isom_file, u32 trackNumber, u32 profile, u32 compat, u32 level);

#endif

#ifndef GPAC_DISABLE_ISOM

/*!
 Rewrite NAL-based samples (AVC/HEVC/...) samples if nalu size_length has to be changed
\param isom_file the target ISOBMF file
\param trackNumber the target track
\param new_size_in_bits new size in bits of the NALU length field in the track, for all samples description of the track
\return error if any
 */
GF_Err gf_media_nal_rewrite_samples(GF_ISOFile *isom_file, u32 trackNumber, u32 new_size_in_bits);

#endif

#ifndef GPAC_DISABLE_MEDIA_IMPORT
/*!
 Split SVC layers
\param isom_file the target ISOBMF file
\param trackNumber the target track
\param splitAll if set each layers will be in a single track, otherwise all non-base layers will be in the same track
\return error if any
 */
GF_Err gf_media_split_svc(GF_ISOFile *isom_file, u32 trackNumber, Bool splitAll);

/*!
 Merge SVC layers
\param isom_file the target ISOBMF file
\param trackNumber the target track
\param mergeAll if set all layers will be merged a single track, otherwise all non-base layers will be merged in the same track
\return error if any
*/
GF_Err gf_media_merge_svc(GF_ISOFile *isom_file, u32 trackNumber, Bool mergeAll);

/*! LHVC extractor mode*/
typedef enum
{
	/*! use extractors*/
	GF_LHVC_EXTRACTORS_ON,
	/*! don't use extractors and keep base track inband/outofband param set signaling*/
	GF_LHVC_EXTRACTORS_OFF,
	/*! don't use extractors and force inband signaling in enhancement layer*/
	GF_LHVC_EXTRACTORS_OFF_FORCE_INBAND
} GF_LHVCExtractoreMode;

/*! Split L-HEVC layers
\param isom_file the target ISOBMF file
\param trackNumber the target track
\param for_temporal_sublayers if set only temporal sublayers are split, otherwise layers are split
\param splitAll if set each layers will be in a single track, otherwise all non-base layers will be in the same track
\param extractor_mode extractor mode
\return error if any
 */
GF_Err gf_media_split_lhvc(GF_ISOFile *isom_file, u32 trackNumber, Bool for_temporal_sublayers, Bool splitAll, GF_LHVCExtractoreMode extractor_mode);

/*!
 Split HEVC tiles into different tracks
\param isom_file the target ISOBMF file
\param signal_only if set to 1 or 2, inserts tile description and NAL->tile mapping but does not create separate tracks. If 2, NAL->tile mapping uses RLE
\return error if any
 */
GF_Err gf_media_split_hevc_tiles(GF_ISOFile *isom_file, u32 signal_only);


/*!
 Filter HEVC/L-HEVC NALUs by temporal IDs and layer IDs, removing all NALUs above the desired levels.
\param isom_file the target ISOBMF file
\param trackNumber the target track
\param max_temporal_id_plus_one max temporal ID plus 1 of all NALUs to be removed.
\param max_layer_id_plus_one max layer ID plus 1 of all NALUs to be removed.
\return error if any
 */
GF_Err gf_media_filter_hevc(GF_ISOFile *isom_file, u32 trackNumber, u8 max_temporal_id_plus_one, u8 max_layer_id_plus_one);

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


/*! @} */


/*!
\addtogroup dashseg_grp DASH Segmenter
\ingroup media_grp
\brief MPEG-DASH creation.

This section documents media functions for MPEG-DASH creation.

@{
*/

/*!
 * DASH segmenter information per source
 */
typedef struct
{
	/*! source file to be used*/
	char *file_name;
	/*! ID of the representation, may be NULL (assigned by dasher)*/
	char *representationID;
	/*! ID of the period, may be NULL (assigned by dasher)*/
	char *periodID;
	/*! ID of the adaptation set, may be 0 (assigned by dasher)*/
	u32 asID;
	/*! forced media duration.*/
	GF_Fraction64 media_duration;
	/*! number of base URLs in the baseURL structure*/
	u32 nb_baseURL;
	/*! list of baseURL to be used for this representation*/
	char **baseURL;
	/*! xlink of the period if desired, NULL otherwise*/
	char *xlink;
	/*! number of items in roles*/
	u32 nb_roles;
	/*! role of the representation according to MPEG-DASH*/
	char **roles;
	/*! number of items in rep_descs*/
	u32 nb_rep_descs;
	/*! descriptors to be inserted in the representation*/
	char **rep_descs;
	/*! number of items in p_descs*/
	u32 nb_p_descs;
	/*! descriptors to be inserted in the period*/
	char **p_descs;
	/*! number of items in nb_as_descs*/
	u32 nb_as_descs;
	/*! descriptors to be inserted in the adaptation set. Representation with non matching as_descs will be in different adaptation sets*/
	char **as_descs;
	/*! number of items in nb_as_c_descs*/
	u32 nb_as_c_descs;
	/*! descriptors to be inserted in the adaptation set. Ignored when matching Representation to adaptation sets*/
	char **as_c_descs;
	/*! forces bandwidth in bits per seconds of the source media. If 0, computed from file */
	u32 bandwidth;
	/*! forced period duration (used when using empty periods or xlink periods without content)*/
	GF_Fraction period_duration;
	/*! forced dash target duration for this rep*/
	GF_Fraction dash_duration;
	/*! sets default start number for this representation. if not set, assigned automatically */
	u32 startNumber; 	//TODO: start number, template
	/*! overrides template for this input*/
	char *seg_template;
	/*! sets name of HLS playlist*/
	char *hls_pl;
	/*! if true and only one media stream in target segment, the moov will use the media stream timescale*/
	Bool sscale;
	/*! only imports this track from the source*/
	u32 track_id;
	/*! non legacy options passed to dasher for source */
	char *source_opts;
	/*! filter chain to instantiate between this source and the dasher*/
	char *filter_chain;
	/*! period order, internal only*/
	u32 period_order;
} GF_DashSegmenterInput;

/*!
DASH profile constants
\hideinitializer

Matches profile enum of dasher module: auto|live|onDemand|main|full|hbbtv1.5.live|dashavc264.live|dashavc264.onDemand
 */
typedef enum
{
	/*! auto profile, internal use only*/
	GF_DASH_PROFILE_AUTO = 0,
	/*! Live dash profile for: live for ISOFF, SIMPLE for M2TS */
	GF_DASH_PROFILE_LIVE,
	/*! onDemand profile*/
 	GF_DASH_PROFILE_ONDEMAND,
	/*! main profile*/
	GF_DASH_PROFILE_MAIN,
	/*! Full dash (no profile)*/
	GF_DASH_PROFILE_FULL,

	/*! industry profile HbbTV 1.5 ISOBMFF Live */
	GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE,
	/*! industry profile DASH-IF ISOBMFF Live */
	GF_DASH_PROFILE_AVC264_LIVE,
	/*! industry profile DASH-IF ISOBMFF onDemand */
	GF_DASH_PROFILE_AVC264_ONDEMAND,
	/*! industry profile DASH-IF ISOBMFF low latency */
	GF_DASH_PROFILE_DASHIF_LL,
} GF_DashProfile;


/*!
DASH bitstream switching selector
\hideinitializer
 */
typedef enum
{
	/*! inband parameter sets for live profile and none for onDemand*/
	GF_DASH_BSMODE_DEFAULT,
	/*! always out of band parameter sets */
	GF_DASH_BSMODE_NONE,
	/*! always inband parameter sets */
	GF_DASH_BSMODE_INBAND,
	/*! out of band parameter sets except PPS and APS, used for VVC */
	GF_DASH_BSMODE_INBAND_PPS,
	/*! both inband and out of band parameter sets */
	GF_DASH_BSMODE_BOTH, //Romain
	/*! attempts to merge parameter sets in a single sample entry */
	GF_DASH_BSMODE_MERGED,
	/*! parameter sets are in different sample entries */
	GF_DASH_BSMODE_MULTIPLE_ENTRIES,
	/*! forces GF_DASH_BSMODE_INBAND even if only one file is used*/
	GF_DASH_BSMODE_SINGLE,
} GF_DashSwitchingMode;


/*!
DASH media presentation type
\hideinitializer
 */
typedef enum
{
	/*! DASH Presentation is static*/
	GF_DASH_STATIC = 0,
	/*! DASH Presentation is dynamic*/
	GF_DASH_DYNAMIC,
	/*! DASH Presentation is dynamic and this is the last segmenting operation in the period. This can only be used when DASH segmenter context is used, will close the period*/
	GF_DASH_DYNAMIC_LAST,
	/*! same as GF_DASH_DYNAMIC but prevents all segment cleanup */
	GF_DASH_DYNAMIC_DEBUG,
} GF_DashDynamicMode;

/*!
DASH selector for content protection descriptor location
\hideinitializer
 */
typedef enum
{
	/*! content protection descriptor is at the adaptation set level*/
	GF_DASH_CPMODE_ADAPTATION_SET=0,
	/*! content protection descriptor is at the representation level*/
	GF_DASH_CPMODE_REPRESENTATION,
	/*! content protection descriptor is at the adaptation set and representation level*/
	GF_DASH_CPMODE_BOTH,
} GF_DASH_ContentLocationMode;

/*! DASH segmenter*/
typedef struct __gf_dash_segmenter GF_DASHSegmenter;

/*!
 Create a new DASH segmenter
\param mpdName target MPD file name, cannot be changed
\param profile target DASH profile, cannot be changed
\param tmp_dir temp dir for file generation, if NULL uses libgpac default
\param timescale timescale used to specif most of the dash timings. If 0, 1000 is used
\param dasher_context_file config file used to store the context of the DASH segmenter. This allows destroying the segmenter and restarting it later on with the right DASH segquence numbers, MPD and and timing info
\return the DASH segmenter object
*/
GF_DASHSegmenter *gf_dasher_new(const char *mpdName, GF_DashProfile profile, const char *tmp_dir, u32 timescale, const char *dasher_context_file);
/*!
 Deletes a DASH segmenter
\param dasher the DASH segmenter object
*/
void gf_dasher_del(GF_DASHSegmenter *dasher);

/*!
 Removes the DASH inputs. Re-add new ones with gf_dasher_add_input()
\param dasher the DASH segmenter object
*/
void gf_dasher_clean_inputs(GF_DASHSegmenter *dasher);

/*! Sets MPD info
\param dasher the DASH segmenter object
\param title MPD title
\param copyright MPD copyright
\param moreInfoURL MPD "more info" URL
\param sourceInfo MPD source info
\param lang MPD language for title
\return error code if any
*/
GF_Err gf_dasher_set_info(GF_DASHSegmenter *dasher, const char *title, const char *copyright, const char *moreInfoURL, const char *sourceInfo, const char *lang);

/*!
 Sets MPD Location. This is useful to distrubute a dynamic MPD by mail or any non-HTTP mean
\param dasher the DASH segmenter object
\param location the URL where this MPD can be found
\return error code if any
*/
GF_Err gf_dasher_set_location(GF_DASHSegmenter *dasher, const char *location);

/*!
 Adds a base URL to the MPD
\param dasher the DASH segmenter object
\param base_url base url to add
\return error code if any
*/
GF_Err gf_dasher_add_base_url(GF_DASHSegmenter *dasher, const char *base_url);

/*!
 Enable URL template -  - may be overridden by the current profile
\param dasher the DASH segmenter object
\param enable enable usage of URL template
\param default_template template for the segment name
\param default_extension extension for the segment name
\param default_init_extension extension for the initialization segment name
\return error code if any
*/

GF_Err gf_dasher_enable_url_template(GF_DASHSegmenter *dasher, Bool enable, const char *default_template, const char *default_extension, const char *default_init_extension);

/*!
 Enable Segment Timeline template - may be overridden by the current profile
\param dasher the DASH segmenter object
\param enable enable or disable
\return error code if any
*/
GF_Err gf_dasher_enable_segment_timeline(GF_DASHSegmenter *dasher, Bool enable);

/*!
 Enables single segment - may be overridden by the current profile
\param dasher the DASH segmenter object
\param enable enable or disable
\return error code if any
*/

GF_Err gf_dasher_enable_single_segment(GF_DASHSegmenter *dasher, Bool enable);

/*!
 Enable single file (with multiple segments) - may be overridden by the current profile
\param dasher the DASH segmenter object
\param enable enable or disable
\return error code if any
*/
GF_Err gf_dasher_enable_single_file(GF_DASHSegmenter *dasher, Bool enable);

/*!
 Sets bitstream switching mode - may be overridden by the current profile
\param dasher the DASH segmenter object
\param bitstream_switching mode to use for bitstream switching
\return error code if any
*/
GF_Err gf_dasher_set_switch_mode(GF_DASHSegmenter *dasher, GF_DashSwitchingMode bitstream_switching);

/*!
 Sets segment and fragment durations.
\param dasher the DASH segmenter object
\param default_segment_duration the duration of a dash segment
\param default_fragment_duration the duration of a dash fragment - if 0, same as default_segment_duration
\param sub_duration the duration in seconds of media to DASH. If 0, the whole sources will be processed.
\return error code if any
*/
GF_Err gf_dasher_set_durations(GF_DASHSegmenter *dasher, Double default_segment_duration, Double default_fragment_duration, Double sub_duration);

/*!
 Enables splitting at RAP boundaries
\param dasher the DASH segmenter object
\param segments_start_with_rap segments will be split at RAP boundaries
\param fragments_start_with_rap fragments will be split at RAP boundaries
\return error code if any
*/

GF_Err gf_dasher_enable_rap_splitting(GF_DASHSegmenter *dasher, Bool segments_start_with_rap, Bool fragments_start_with_rap);

/*!
 Enables segment marker
\param dasher the DASH segmenter object
\param segment_marker_4cc 4CC code of the segment marker box
\return error code if any
*/
GF_Err gf_dasher_set_segment_marker(GF_DASHSegmenter *dasher, u32 segment_marker_4cc);

/*!
 Enables segment indexes
\param dasher the DASH segmenter object
\param enable_sidx enable or disable
\param subsegs_per_sidx number of subsegments per segment
\param daisy_chain_sidx enable daisy chaining of sidx
\param use_ssix enables ssix generation, level 1 for I-frames, the rest of the segment not mapped
\return error code if any
*/
GF_Err gf_dasher_enable_sidx(GF_DASHSegmenter *dasher, Bool enable_sidx, u32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool use_ssix);

/*!
 Sets mode for the dash segmenter.
\param dasher the DASH segmenter object
\param dash_mode the mode to use. Currently switching from static mode to dynamic mode is not well supported and may produce non-compliant MPDs
\param mpd_update_time time between MPD refresh, in seconds. Used for dynamic mode, may be 0 if \p mpd_live_duration is set
\param time_shift_depth the depth of the time shift buffer in seconds, -1 for infinite time shift.
\param mpd_live_duration total duration of the DASH session in dynamic mode, in seconds. May be set to 0 if \p mpd_update_time is set
\return error code if any
*/
GF_Err gf_dasher_set_dynamic_mode(GF_DASHSegmenter *dasher, GF_DashDynamicMode dash_mode, Double mpd_update_time, s32 time_shift_depth, Double mpd_live_duration);

/*!
 Sets the minimal buffer desired.
\param dasher the DASH segmenter object
\param min_buffer min buffer time in seconds for the DASH session. Currently the minimal buffer is NOT computed from the source material and must be set to an appropriate value.
\return error code if any
*/
GF_Err gf_dasher_set_min_buffer(GF_DASHSegmenter *dasher, Double min_buffer);

/*!
 Sets the availability start time offset.
\param dasher the DASH segmenter object
\param ast_offset ast offset in milliseconds. If >0, the DASH session availabilityStartTime will be earlier than UTC by the amount of seconds specified. If <0, the media representation will have an availabilityTimeOffset of the amount of seconds specified, instructing the client that segments may be accessed earlier.
\return error code if any
*/
GF_Err gf_dasher_set_ast_offset(GF_DASHSegmenter *dasher, s32 ast_offset);

/*!
 Enables memory fragmenting: fragments will be written to disk only once completed
\param dasher the DASH segmenter object
\param enable Enables or disables. Default is disabled.
\return error code if any
*/
GF_Err gf_dasher_enable_memory_fragmenting(GF_DASHSegmenter *dasher, Bool enable);

/*!
 Sets initial values for ISOBMFF sequence number and TFDT in movie fragments.
\param dasher the DASH segmenter object
\param initial_moof_sn sequence number of the first moof to be generated. Default value is 1.
\param initial_tfdt initial tfdt of the first traf to be generated, in DASH segmenter timescale units. Default value is 0.
\return error code if any
*/
GF_Err gf_dasher_set_initial_isobmf(GF_DASHSegmenter *dasher, u32 initial_moof_sn, u64 initial_tfdt);


/*! DASH PSSH storage mode*/
typedef enum
{
	//! PSSH box in moov only
	GF_DASH_PSSH_MOOV = 0,
	//! PSSH box in moof only
	GF_DASH_PSSH_MOOF,
	//! PSSH box in moov and MPD
	GF_DASH_PSSH_MOOV_MPD,
	//! PSSH box in moof and MPD
	GF_DASH_PSSH_MOOF_MPD,
	//! PSSH box in MPD only
	GF_DASH_PSSH_MPD,
	//! Drop PSSH info from mpd and init seg
	GF_DASH_PSSH_NONE,
} GF_DASHPSSHMode;

/*!
 Configure how default values for ISOBMFF are stored
\param dasher the DASH segmenter object
\param no_fragments_defaults if set, fragments default values are repeated in each traf and not set in trex. Default value is GF_FALSE
\param pssh_mode sets the storage mode of PSSH in moov/moof/mpd. 
\param samplegroups_in_traf if set, all sample group definitions are stored in each traf and not set in init segment. Default value is GF_FALSE
\param single_traf_per_moof if set, each moof will contain a single traf, even if source media is multiplexed. Default value is GF_FALSE
\param tfdt_per_traf if set, each traf will contain a tfdt. Only applicable when single_traf_per_moof is GF_TRUE. Default value is GF_FALSE
\param mvex_after_traks if set, the mvex box will be written after all track boxes
\param sdtp_in_traf mode for sdtp storage in traf (smooth compatibility): 0: not allowed, 1: only stdp, no trun flags, 2: both (trun for sync, stdp for the rest)
\return error code if any
*/
GF_Err gf_dasher_configure_isobmf_default(GF_DASHSegmenter *dasher, Bool no_fragments_defaults, GF_DASHPSSHMode pssh_mode, Bool samplegroups_in_traf, Bool single_traf_per_moof, Bool tfdt_per_traf, Bool mvex_after_traks, u32 sdtp_in_traf);

/*!
 Enables insertion of UTC reference in the beginning of segments
\param dasher the DASH segmenter object
\param insert_utc if set, UTC will be inserted. Default value is disabled.
\return error code if any
*/

GF_Err gf_dasher_enable_utc_ref(GF_DASHSegmenter *dasher, Bool insert_utc);

/*!
 Enables real-time generation of media segments.
\param dasher the DASH segmenter object
\param real_time if set, segments are generated in real time. Only supported for single representation (potentially multiplexed) DASH session. Default is disabled.
\return error code if any
*/
GF_Err gf_dasher_enable_real_time(GF_DASHSegmenter *dasher, Bool real_time);

/*!
 Sets where the  ContentProtection element is inserted in an adaptation set.
*	\param dasher the DASH segmenter object
*	\param mode ContentProtection element location mode.
*	\return error code if any
*/
GF_Err gf_dasher_set_content_protection_location_mode(GF_DASHSegmenter *dasher, GF_DASH_ContentLocationMode mode);

/*!
 Sets profile extension as used by DASH-IF and DVB.
\param dasher the DASH segmenter object
\param dash_profile_extension specifies a string of profile extensions, as used by DASH-IF and DVB.
\return error code if any
*/
GF_Err gf_dasher_set_profile_extension(GF_DASHSegmenter *dasher, const char *dash_profile_extension);

/*!
 Enable/Disable cached inputs .
\param dasher the DASH segmenter object
\param no_cache if true, input file will be reopen each time the dasher process function is called .
\return error code if any
*/
GF_Err gf_dasher_enable_cached_inputs(GF_DASHSegmenter *dasher, Bool no_cache);

/*!
 Enable/Disable loop inputs .
\param dasher the DASH segmenter object
\param do_loop if true, input files will be looped at the end of the file in a live simulation. Otherwise a new period will be created.
\return error code if any
*/
GF_Err gf_dasher_enable_loop_inputs(GF_DASHSegmenter *dasher, Bool do_loop);


/*!
DASH selector for segment split mode
\hideinitializer
 */
typedef enum
{
	/*! segment start time is greater than or equal to theoretical segment start (segment_duration*segment_number)*/
	GF_DASH_SPLIT_OUT=0,
	/*! segment start time is as close as possible to theoretical segment start (segment_duration*segment_number), but may be greater*/
	GF_DASH_SPLIT_CLOSEST,
	/*! segment start time is less than or equal to theoretical segment start (segment_duration*segment_number) so that the theoretical start time is always present in the segment*/
	GF_DASH_SPLIT_IN,
} GF_DASH_SplitMode;

/*!
 Enable/Disable split on bound mode.
\param dasher the DASH segmenter object
\param split_mode the desired segmentation mode
\return error code if any
*/
GF_Err gf_dasher_set_split_mode(GF_DASHSegmenter *dasher, GF_DASH_SplitMode split_mode);


/*!
 Enable/Disable last segment merging (disabled by default).
 *	\param dasher the DASH segmenter object
 *	\param merge_last_seg if true, last segment is merged into previous if duration less than half target dur
 *	\return error code if any
*/
GF_Err gf_dasher_set_last_segment_merge(GF_DASHSegmenter *dasher, Bool merge_last_seg);

/*!
 Sets m3u8 file name - if not set, no m3u8 output
\param dasher the DASH segmenter object
\param insert_clock if true UTC clock is inserted in variant playlists in live
\return error code if any
*/
GF_Err gf_dasher_set_hls_clock(GF_DASHSegmenter *dasher, Bool insert_clock);

/*!
 Sets cue file for the session.
\param dasher the DASH segmenter object
\param cues_file name of the cue file. This is an XML document with root 'DASHCues' element, one or multiple 'Stream' elements with attribute ID (trackID)
 and timescale (trackTimescale), and a set of 'cues' elements per Stream with attributes sampleNumber, dts or cts.
\param strict_cues if true will fail if one cue doesn't match a timestamp in the stream or if the split sample is not RAP
\return error code if any
*/
GF_Err gf_dasher_set_cues(GF_DASHSegmenter *dasher, const char *cues_file, Bool strict_cues);

/*!
 Adds a media input to the DASHer
\param dasher the DASH segmenter object
\param input media source to add
\return error code if any
*/
GF_Err gf_dasher_add_input(GF_DASHSegmenter *dasher, const GF_DashSegmenterInput *input);

/*!
 Process the media source and generate segments
\param dasher the DASH segmenter object
\return error code if any
*/
GF_Err gf_dasher_process(GF_DASHSegmenter *dasher);

/*!
 Returns time to wait until end of currently generated segments
\param dasher the DASH segmenter object
\param ms_ins_session if set, retrives the number of ms since the start of the dash session
\return time to wait in milliseconds
*/
u32 gf_dasher_next_update_time(GF_DASHSegmenter *dasher, u64 *ms_ins_session);


/*!
 Sets dasher start date, rather than use current time. Used for debugging purposes, such as simulating long lasting sessions.
\param dasher the DASH segmenter object
\param dash_utc_start_date start date as as xs:date, eg YYYY-MM-DDTHH:MM:SSZ. If 0, current time is used
*/
void gf_dasher_set_start_date(GF_DASHSegmenter *dasher, const char *dash_utc_start_date);


/*!
 Sets print flags for filter session
\param dasher the DASH segmenter object
\param fs_print_flags flags for statistics (1) and graph (2) printing
\return error if any
*/
GF_Err gf_dasher_print_session_info(GF_DASHSegmenter *dasher, u32 fs_print_flags);

/*!
 Keeps UTC creation and modification dates from sources, if any (default is no)
\param dasher the DASH segmenter object
\param keep_utc if GF_TRUE, keeps UTC times
\return error if any
*/
GF_Err gf_dasher_keep_source_utc(GF_DASHSegmenter *dasher, Bool keep_utc);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
/*!
 save file as fragmented movie
\param isom_file the target file to be fragmented
\param output_file name of the output file
\param max_duration_sec max fragment duration in seconds
\param use_mfra insert track fragment movie fragments
\return error if any
 */
GF_Err gf_media_fragment_file(GF_ISOFile *isom_file, const char *output_file, Double max_duration_sec, Bool use_mfra);
#endif

/*! @} */


/*!
\addtogroup mexp_grp Media Exporter
\ingroup media_grp
\brief Media exporting and extraction.

This section documents functions for media exporting and extraction.

@{
 */

/*!
Track dumper formats and flags
\hideinitializer
 */
enum
{
	/*! track dumper types are formatted as flags for conveniency for
	authoring tools, but never used as a OR'ed set*/
	/*native format (JPG, PNG, MP3, etc) if supported*/
	GF_EXPORT_NATIVE = 1,
	/*! raw samples (including hint tracks for rtp)*/
	GF_EXPORT_RAW_SAMPLES = (1<<1),
	/*! NHNT format (any MPEG-4 media)*/
	GF_EXPORT_NHNT = (1<<2),
	/*! full remux of source file - equivalent to `gpac -i in_name:FID=1 reframer:FID=2:SID=1 -o out_name:SID=2` */
	GF_EXPORT_REMUX = (1<<3),
	/*! MP4 (all except OD)*/
	GF_EXPORT_MP4 = (1<<4),
	/*! currently unused*/
	GF_EXPORT_UNUSED = (1<<4),
	/*! NHML format (any media)*/
	GF_EXPORT_NHML = (1<<6),
	/*! SAF format*/
	GF_EXPORT_SAF = (1<<7),
	/*! WebVTT metadata format (any media)*/
	GF_EXPORT_WEBVTT_META = (1<<8),
	/*! WebVTT metadata format: media data will be embedded in webvtt*/
	GF_EXPORT_WEBVTT_META_EMBEDDED = (1<<9),

	/*! following ones are real flags*/
	/*!
	for MP4 extraction, indicates track should be added to dest file if any
	for raw extraction, indicates data shall be appended at the end of output file if present
	*/
	GF_EXPORT_MERGE = (1<<10),
	/*! don't infer file extension */
	GF_EXPORT_NO_FILE_EXT = (1 << 11),
	/*! indicates QCP file format possible as well as native (EVRC and SMV audio only)*/
	GF_EXPORT_USE_QCP = (1<<12),
	/*! indicates full NHML dump*/
	GF_EXPORT_NHML_FULL = (1<<13),
	/*! exports a single svc layer*/
	GF_EXPORT_SVC_LAYER = (1<<14),
	/*! Don't merge identical cues in consecutive samples */
	GF_EXPORT_WEBVTT_NOMERGE = (1<<15),

	/*! Experimental Streaming Instructions */
	GF_EXPORT_SIX = (1<<16),

	/*! only probes extraction format*/
	GF_EXPORT_PROBE_ONLY = (1<<30),
	/*when set by user during export, will abort*/
	GF_EXPORT_DO_ABORT = 0x80000000 //(1<<31)
};

#ifndef GPAC_DISABLE_MEDIA_EXPORT

/*!
  track dumper
 */
typedef struct __track_exporter
{
	/*! source ISOBMF file */
	GF_ISOFile *file;
	/*! ID of track/PID/... to be dumped*/
	u32 trackID;
	/*! sample number to export for GF_EXPORT_RAW_SAMPLES only*/
	u32 sample_num;
	/*! output name, if no extension set the extension will be added based on track type*/
	char *out_name;
	/*! dump type and flags*/
	u32 flags;
	/*! non-ISOBMF source file (AVI, TS)*/
	char *in_name;
	/*! optional FILE for output*/
	FILE *dump_file;
	/*! filter session dump flags*/
	u32 print_stats_graph;
	/*! track type: 0: none specified, 1: video, 2: audio, 3: text*/
	u32 track_type;
} GF_MediaExporter;

/*!
  Dumps a given media track
\param dump the track dumper object
\return  error if any
 */
GF_Err gf_media_export(GF_MediaExporter *dump);

#ifndef GPAC_DISABLE_VTT
/*! dumps a webvtt track to a given file
\param dumper media dumper object
\param trackNumber the target track to dump
\param merge if GF_TRUE, merge vtt cues while dumping them
\param box_dump if GF_TRUE, dumps box structures
\return  error if any
*/
GF_Err gf_webvtt_dump_iso_track(GF_MediaExporter *dumper, u32 trackNumber, Bool merge, Bool box_dump);
#endif

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/

/*! @} */


/*!
\addtogroup mhint_grp Media Hinting
\ingroup media_grp
\brief ISOBMFF file hinting.

 This section documents functions for ISOBMFF file hinting.

@{
 */


#ifndef GPAC_DISABLE_ISOM_HINTING
/*
	RTP IsoMedia file hinting
*/
/*! ISOBMFF RTP hinter object*/
typedef struct __tag_isom_hinter GF_RTPHinter;

/*!
 Creates a new track hinter object
\param isom_file the target ISOBMF file
\param trackNumber the track to hint
\param Path_MTU max RTP packet size (excluding IP/UDP/IP headers)
\param max_ptime max packet duration in RTP timescale, can be set to 0 for auto compute
\param default_rtp_rate RTP rate for the track, can be set to 0 for auto compute
\param hint_flags RTP flags as defined in <gpac/ietf.h>
\param PayloadID RTP payload ID, can be set to 0 for auto compute
\param copy_media if set, media is copied inside the hint samples, otherwise only referenced from the media track
\param InterleaveGroupID sets the group ID of this track for interleaving - same semantics as in gf_isom_set_track_interleaving_group
\param InterleaveGroupPriority sets the group priority of this track for interleaving - same semantics as in gf_isom_set_track_priority_in_group
\param e output error code if any
\return the hinter object
 */
GF_RTPHinter *gf_hinter_track_new(GF_ISOFile *isom_file, u32 trackNumber,
                                  u32 Path_MTU, u32 max_ptime, u32 default_rtp_rate, u32 hint_flags, u8 PayloadID,
                                  Bool copy_media, u32 InterleaveGroupID, u8 InterleaveGroupPriority, GF_Err *e);

/*!
 Delete the track hinter
\param tkHinter track hinter object
*/
void gf_hinter_track_del(GF_RTPHinter *tkHinter);

/*!
 hints all samples in the media track
\param tkHinter track hinter object
\return error if any
 */
GF_Err gf_hinter_track_process(GF_RTPHinter *tkHinter);

/*!
 Gets media bandwidth in kbps
\param tkHinter track hinter object
\return media bandwidth in kbps
*/
u32 gf_hinter_track_get_bandwidth(GF_RTPHinter *tkHinter);

/*!
 Force file to use no random offsets for sequence number and time, if supported by server
\param tkHinter track hinter object
\return error if any
*/
GF_Err gf_hinter_track_force_no_offsets(GF_RTPHinter *tkHinter);

/*!
 Gets track hinter flags
\param tkHinter track hinter object
\return hint flags for this object
 */
u32 gf_hinter_track_get_flags(GF_RTPHinter *tkHinter);

/*!
 Gets rtp payload name
\param tkHinter track hinter object
\param payloadName static buffer for retrieval, minimum 30 bytes
*/
void gf_hinter_track_get_payload_name(GF_RTPHinter *tkHinter, char *payloadName);

/*!
 Finalizes hinting process for the track (setup flags, write SDP for RTP, ...)
\param tkHinter track hinter object
\param AddSystemInfo if true, systems info are duplicated in the SDP (decoder cfg, PL IDs ..)
\return error if any
*/
GF_Err gf_hinter_track_finalize(GF_RTPHinter *tkHinter, Bool AddSystemInfo);

/*!
SDP IOD Profile
\hideinitializer
 */
typedef enum
{
	/*! no IOD included*/
	GF_SDP_IOD_NONE = 0,
	/*! base64 encoding of the regular MPEG-4 IOD*/
	GF_SDP_IOD_REGULAR,
	/*! base64 encoding of IOD containing BIFS and OD tracks (one AU only) - this is used for ISMA 1.0 profiles
	note that the "hinted" file will loose all systems info*/
	GF_SDP_IOD_ISMA,
	/*! same as ISMA but removes all clock references from IOD*/
	GF_SDP_IOD_ISMA_STRICT,
} GF_SDP_IODProfile;

/*!
Finalizes hinting process for the file (setup flags, write SDP for RTP, ...)
\param isom_file target ISOBMF file
\param IOD_Profile the IOD profile to use for SDP
\param bandwidth total bandwidth in kbps of all hinted tracks, 0 means no bandwidth info at session level
\return error if any
*/
GF_Err gf_hinter_finalize(GF_ISOFile *isom_file, GF_SDP_IODProfile IOD_Profile, u32 bandwidth);

/*!
 Check if the given data fits in an ESD url
\param data data to be encoded
\param data_size size of data to be encoded
\param streamType systems stream type needed to signal data mime-type (OD, BIFS or any)
\return GF_TRUE if the encoded data fits in an ESD url
 */
Bool gf_hinter_can_embbed_data(u8 *data, u32 data_size, u32 streamType);

#endif /*GPAC_DISABLE_ISOM_HINTING*/

/*! @} */


/*!
\addtogroup msaf_grp LASeR SAF creation
\ingroup media_grp
\brief LASeR SAF multiplexing.

This section documents functions for LASeR SAF multiplexing.

@{
 */


/*! SAF Multiplexer object. The multiplexer supports concurencial (multi-threaded) access*/
typedef struct __saf_muxer GF_SAFMuxer;
/*!
	Creates a new SAF Multiplexer
\return the SAF multiplexer object
 */
GF_SAFMuxer *gf_saf_mux_new();

/*!
	SAF Multiplexer destructor
\param mux the SAF multiplexer object
 */
void gf_saf_mux_del(GF_SAFMuxer *mux);

/*!
 Adds a new stream in the SAF multiplex
\param mux the SAF multiplexer object
\param stream_id ID of the SAF stream to create
\param ts_res timestamp resolution for AUs in this stream
\param buffersize_db size of decoding buffer in bytes
\param stream_type MPEG-4 systems stream type of this stream
\param object_type MPEG-4 systems object type indication of this stream
\param mime_type MIME type for this stream, NULL if unknown
\param dsi Decoder specific info for this stream
\param dsi_len specific info size for this stream
\param remote_url URL of the SAF stream if not embedded in the multiplex
\return error if any
 */
GF_Err gf_saf_mux_stream_add(GF_SAFMuxer *mux, u32 stream_id, u32 ts_res, u32 buffersize_db, u8 stream_type, u8 object_type, char *mime_type, char *dsi, u32 dsi_len, char *remote_url);


/*!
 adds an AU to the given Warning, AU data will be freed by the multiplexer. AUs are NOT re-sorted by CTS, in order to enable audio interleaving.
\param mux the SAF multiplexer object
\param stream_id ID of the SAF stream to remove
\param CTS composition timestamp of the AU
\param data payload of the AU
\param data_len payload size of the AU
\param is_rap set to GF_TRUE to signal a random access point
\return error if any
*/
GF_Err gf_saf_mux_add_au(GF_SAFMuxer *mux, u32 stream_id, u32 CTS, char *data, u32 data_len, Bool is_rap);

/*!
  Gets the content of the multiplexer for the given time.
\param mux the SAF multiplexer object
\param time_ms target mux time in ms
\param force_end_of_session if set to GF_TRUE, this flushes the SAF Session - no more operations will be allowed on the muxer
\param out_data output SAF data
\param out_size output SAF data size
\return error if any
 */
GF_Err gf_saf_mux_for_time(GF_SAFMuxer *mux, u32 time_ms, Bool force_end_of_session, u8 **out_data, u32 *out_size);

/*!
  Gets timescale and TS increment from double FPS value.
\param fps the target fps
\param timescale output timescale value
\param ts_inc output timestamp increment value
*/
void gf_media_get_video_timing(Double fps, u32 *timescale, u32 *ts_inc);

/*! gets dolby vision level
 \param width width in pixels of video
 \param height height in pixels of video
 \param fps_num framerate numerator
 \param fps_den framerate denominator
 \param codecid GPAC codec ID
 \return dv level
*/
u32 gf_dolby_vision_level(u32 width, u32 height, u64 fps_num, u64 fps_den, u32 codecid);

#ifdef __cplusplus
}
#endif

/*! @} */

#endif	/*_GF_MEDIA_H_*/

