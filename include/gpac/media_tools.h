/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/isomedia.h>
#include <gpac/avparse.h>
#include <gpac/config_file.h>

/*computes file hash. If file is ISO-based, computre hash according to OMA (P)DCF (without MutableDRMInformation box)*/
GF_Err gf_media_get_file_hash(const char *file, u8 hash[20]);

#ifndef GPAC_DISABLE_ISOM
/*creates (if needed) a GF_ESD for the given track - THIS IS RESERVED for local playback
only, since the OTI used when emulated is not standard...*/
GF_ESD *gf_media_map_esd(GF_ISOFile *mp4, u32 track);
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE
/*changes pixel aspect ratio for visual tracks if supported. Negative values remove any PAR info*/
GF_Err gf_media_change_par(GF_ISOFile *file, u32 track, s32 ar_num, s32 ar_den);
GF_Err gf_media_remove_non_rap(GF_ISOFile *file, u32 track);
#endif


#define GF_IMPORT_DEFAULT_FPS	25.0

#ifndef GPAC_DISABLE_MEDIA_IMPORT

/*			
			track importers

	All these can import a file into a dedicated track. If esd is NULL the track is blindly added 
	otherwise it is added with the requested ESID if non-0, otherwise the new trackID is stored in ESID
	if use_data_ref is set, data is only referenced in the file
	if duration is not 0, only the first duration seconds are imported
	NOTE: if an ESD is specified, its decoderSpecificInfo is also updated
*/
/*track importer flags*/
enum
{
	/*references data rather than copy, whenever possible*/
	GF_IMPORT_USE_DATAREF = 1,
	/*for AVI video: imports at constant FPS (eg imports N-Vops due to encoder drops)*/
	GF_IMPORT_NO_FRAME_DROP = 1<<1,
	/*for CMP ASP only: forces treating the input as packed bitsream and discards all n-vops*/
	GF_IMPORT_FORCE_PACKED = 1<<2,
	/*for AAC audio: forces SBR mode with implicit signaling (backward compatible)*/
	GF_IMPORT_SBR_IMPLICIT = 1<<3,
	/*for AAC audio: forces SBR mode with explicit signaling (non-backward compatible). 
	Will override GF_IMPORT_SBR_IMPLICIT flag when set*/
	GF_IMPORT_SBR_EXPLICIT = 1<<4,
	/*forces MPEG-4 import - some 3GP2 streams have both native IsoMedia sample description and MPEG-4 one possible*/
	GF_IMPORT_FORCE_MPEG4 = 1<<5,
	/*special flag for text import at run time (never set on probe), indicates to leave the text box empty
	so that we dynamically adapt to display size*/
	GF_IMPORT_SKIP_TXT_BOX = 1<<6,
	/*indicates to keep unknown tracks from .MOV/.IsoMedia files*/
	GF_IMPORT_KEEP_ALL_TRACKS = 1<<7,
	/*uses compact size in .MOV/.IsoMedia files*/
	GF_IMPORT_USE_COMPACT_SIZE = 1<<8,
	/*don't add a final empty sample when importing text tracks from srt*/
	GF_IMPORT_NO_TEXT_FLUSH = 1<<9,
	/*for SVC or SHVC video: forces explicit SVC / SHVC signaling */
	GF_IMPORT_SVC_EXPLICIT = 1<<10,
	/*for SVC / SHVC video: removes all SVC / SHVC extensions*/
	GF_IMPORT_SVC_NONE = 1<<11,

	/*for AAC audio: forces PS mode with implicit signaling (backward compatible)*/
	GF_IMPORT_PS_IMPLICIT = 1<<12,
	/*for AAC audio: forces PS mode with explicit signaling (non-backward compatible). 
	Will override GF_IMPORT_PS_IMPLICIT flag when set*/
	GF_IMPORT_PS_EXPLICIT = 1<<13,
	
	/* oversampled SBR */
	GF_IMPORT_OVSBR = 1<<14,

	/* set subsample information with SVC*/
	GF_IMPORT_SET_SUBSAMPLES = 1<<15,

	/* force to mark non-IDR frmaes with sync data (I slices,) to be marked as sync points points 
	THE RESULTING FILE IS NOT COMPLIANT*/
	GF_IMPORT_FORCE_SYNC = 1<<16,

	/*when set, only updates tracks info and return*/
	GF_IMPORT_PROBE_ONLY	= 1<<20,
	/*only set when probing, signals several frames per sample possible*/
	GF_IMPORT_3GPP_AGGREGATION = 1<<21,
	/*only set when probing, signals video FPS overridable*/
	GF_IMPORT_OVERRIDE_FPS = 1<<22,
	/*only set when probing, signals duration not usable*/
	GF_IMPORT_NO_DURATION = 1<<23,
	/*when set IP packets found in MPE sections will be sent to the local network */
	GF_IMPORT_MPE_DEMUX = 1<<24,
	/*when set by user during import, will abort*/
	GF_IMPORT_DO_ABORT = 1<<31
};



#define GF_IMPORT_AUTO_FPS		10000.0

#define GF_IMPORT_MAX_TRACKS	100
struct __track_video_info
{
	u32 width, height, par;
	Double FPS;
};
struct __track_audio_info
{
	u32 sample_rate, nb_channels;
};

struct __track_import_info
{
	u32 track_num;
	/*track type (GF_ISOM_MEDIA_****)*/
	u32 type;
	/*media type ('MPG1', 'MPG2', ISO 4CC, AVI 4CC)*/
	u32 media_type;
	/*possible import flags*/
	u32 flags;
	/*media format info*/
	struct __track_video_info video_info;
	struct __track_audio_info audio_info;

	char szCodecProfile[20];

	u32 lang;
	/*for MPEG4 on MPEG-2 TS: mpeg4 ES-ID*/
	u32 mpeg4_es_id;
	/*for MPEG2 TS: program number*/
	u16 prog_num;
};

struct __program_import_info
{
	u32 number;
	char name[40];
};

/*track dumper*/
typedef struct __track_import
{
	GF_ISOFile *dest;
	/*media to import:
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
	/*media source - selects importer type based on extension*/
	char *in_name;
	/*import duration if any*/
	u32 duration;
	/*importer flags*/
	u32 flags;
	/*importer swf flags*/
	u32 swf_flags;
	Float swf_flatten_angle;
	/*if non 0, force video FPS (CMP, AVI, OGG, H264) - also used by SUB import*/
	Double video_fps;
	/*optional ESD*/
	GF_ESD *esd;
	/*optional format indication for media source (used in IM1)*/
	char *streamFormat;
	/*frame per sample cumulation (3GP media only) - MAX 15, ignored in data ref*/
	u32 frames_per_sample;
	/*track ID of imported media in destination file*/
	u32 final_trackID;
	/*optional format indication for media source (used in IM1)*/
	char *force_ext;
	
	/*for MP4 import only*/
	GF_ISOFile *orig;

	/*for text import*/
	u32 fontSize;
	char *fontName;
	u32 text_track_width, text_track_height, text_width, text_height;
	s32 text_x, text_y;

	/*Initial offset of the first AU to import*/
	Double initial_time_offset;

	/*number of tracks after probing - may be set to 0, in which case no track 
	selection can be performed. It may also be inaccurate if probing doesn't
	detect all available tracks (cf ogg import)*/
	u32 nb_tracks;
	/*track info after probing (GF_IMPORT_PROBE_ONLY set).*/
	struct __track_import_info tk_info[GF_IMPORT_MAX_TRACKS];
	u64 probe_duration;

	/*for MPEG-TS and similar: program names*/
	u32 nb_progs;
	struct __program_import_info pg_info[GF_IMPORT_MAX_TRACKS];

	GF_Err last_error;
} GF_MediaImporter;

GF_Err gf_media_import(GF_MediaImporter *importer);


/*adds chapter info contained in file - import_fps is optional (most formats don't use it), defaults to 25*/
GF_Err gf_media_import_chapters(GF_ISOFile *file, char *chap_file, Double import_fps);

/*make the file ISMA compliant: creates ISMA BIFS / OD tracks if needed, and update audio/video IDs
the file should not contain more than one audio and one video track
@keepImage: if set, generates system info if image is found - only used for image imports
*/
GF_Err gf_media_make_isma(GF_ISOFile *mp4file, Bool keepESIDs, Bool keepImage, Bool no_ocr);

/*make the file 3GP compliant && sets profile
*/
GF_Err gf_media_make_3gpp(GF_ISOFile *mp4file);

/*make the file playable on a PSP
*/
GF_Err gf_media_make_psp(GF_ISOFile *mp4file);

/*changes the profile (if not 0) and level (if not 0) indication of the media - only AVC/H264 supported for now*/
GF_Err gf_media_change_pl(GF_ISOFile *file, u32 track, u32 profile, u32 level);

/*rewrite AVC samples if nalu size_length has to be changed*/
GF_Err gf_media_avc_rewrite_samples(GF_ISOFile *file, u32 track, u32 prev_size_in_bits, u32 new_size_in_bits);

/* Split SVC layers */
GF_Err gf_media_split_svc(GF_ISOFile *file, u32 track, Bool splitAll);

/* Merge SVC layers*/
GF_Err gf_media_merge_svc(GF_ISOFile *file, u32 track, Bool mergeAll);

/* Split SHVC layers */
GF_Err gf_media_split_shvc(GF_ISOFile *file, u32 track, Bool splitAll, Bool use_extractors);

GF_Err gf_media_split_hevc_tiles(GF_ISOFile *file);

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


typedef struct 
{
	char *file_name;
	char representationID[100];
	char periodID[100];
	char xlink[100];
	char role[100];
	u32 bandwidth;
} GF_DashSegmenterInput;

typedef enum
{
	GF_DASH_PROFILE_FULL = 0,
	GF_DASH_PROFILE_LIVE, /*live for ISOFF, SIMPLE for M2TS*/
	GF_DASH_PROFILE_ONDEMAND,
	GF_DASH_PROFILE_MAIN,

	/* DASH-AVC/264 profiles */
	GF_DASH_PROFILE_AVC264_LIVE,
	GF_DASH_PROFILE_AVC264_ONDEMAND,

	/*internal use only*/
	GF_DASH_PROFILE_UNKNOWN
} GF_DashProfile;


typedef enum
{
	GF_DASH_BSMODE_NONE = 0,
	GF_DASH_BSMODE_INBAND,
	GF_DASH_BSMODE_MERGED,
	GF_DASH_BSMODE_SINGLE
} GF_DashSwitchingMode;

GF_Err gf_dasher_segment_files(const char *mpd_name, GF_DashSegmenterInput *inputs, u32 nb_inputs, GF_DashProfile profile, 
							   const char *mpd_title, const char *mpd_source, const char *mpd_copyright,
							   const char *mpd_moreInfoURL, const char **mpd_base_urls, u32 nb_mpd_base_urls, 
							   u32 use_url_template, Bool use_segment_timeline,  Bool single_segment, Bool single_file, GF_DashSwitchingMode bitstream_switching_mode,
							   Bool segments_start_with_rap, Double dash_duration_sec, char *seg_rad_name, char *seg_ext, u32 segment_marker_4cc,
							   Double frag_duration_sec, s32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool fragments_start_with_rap, const char *tmp_dir,  
							   GF_Config *dash_ctx, u32 dash_dynamic, u32 mpd_update_time, u32 time_shift_depth, Double subduration, Double min_buffer, 
							   u32 ast_shift_sec, u32 dash_scale, Bool fragments_in_memory, u32 initial_moof_sn, u64 initial_tfdt, Bool no_fragments_defaults, Bool pssh_moof, Bool samplegroups_in_traf);

/*returns time to wait until end of currently generated segments*/
u32 gf_dasher_next_update_time(GF_Config *dash_ctx, u32 mpd_update_time);

#ifndef GPAC_DISABLE_ISOM_WRITE

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
/*save file as fragmented movie*/
GF_Err gf_media_fragment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec);


#endif

#endif


#ifndef GPAC_DISABLE_MEDIA_EXPORT

enum
{
	/*track dumper types are formatted as flags for conveniency for 
	authoring tools, but never used as a OR'ed set*/
	/*native format (JPG, PNG, MP3, etc) if supported*/
	GF_EXPORT_NATIVE = GF_TRUE,
	/*raw samples (including hint tracks for rtp)*/
	GF_EXPORT_RAW_SAMPLES = (1<<1),
	/*NHNT format (any MPEG-4 media)*/
	GF_EXPORT_NHNT = (1<<2),
	/*AVI (MPEG4 video and AVC tracks only)*/
	GF_EXPORT_AVI = (1<<3),
	/*MP4 (all except OD)*/
	GF_EXPORT_MP4 = (1<<4),
	/*AVI->RAW to dump video (trackID=1) or audio (trackID>=2)*/
	GF_EXPORT_AVI_NATIVE = (1<<5),
	/*NHML format (any media)*/
	GF_EXPORT_NHML = (1<<6),
	/*SAF format*/
	GF_EXPORT_SAF = (1<<7),
	/*WebVTT metadata format (any media)*/
	GF_EXPORT_WEBVTT_META = (1<<8),
	/*WebVTT metadata format: media data will be embedded in webvtt*/
	GF_EXPORT_WEBVTT_META_EMBEDDED = (1<<9),
	/* Experimental Streaming Instructions */
	GF_EXPORT_SIX = (1<<14),

	/*following ones are real flags*/
	/*
	for MP4 extraction, indicates track should be added to dest file if any
	for raw extraction, indicates data shall be appended at the end of output file if present
	*/
	GF_EXPORT_MERGE = (1<<10),
	/*indicates QCP file format possible as well as native (EVRC and SMV audio only)*/
	GF_EXPORT_USE_QCP = (1<<11),
	/*indicates full NHML dump*/
	GF_EXPORT_NHML_FULL = (1<<11),
	/**/
	GF_EXPORT_SVC_LAYER = (1<<12),
	/* Don't merge identical cues in consecutive samples */
	GF_EXPORT_WEBVTT_NOMERGE = (1<<13),
	/*ony probes extraction format*/
	GF_EXPORT_PROBE_ONLY = (1<<30),
	/*when set by user during export, will abort*/
	GF_EXPORT_DO_ABORT = (1<<31),
};

/*track dumper*/
typedef struct __track_exporter
{
	GF_ISOFile *file;
	u32 trackID;
	/*sample number to export for GF_EXPORT_RAW_SAMPLES only*/
	u32 sample_num;
	/*out name WITHOUT extension*/
	char *out_name;
	/*dump type*/
	u32 flags;
	/*non-IsoMedia file (AVI)*/
	char *in_name;
} GF_MediaExporter;

/*if error returns same value as error signaled in message*/
GF_Err gf_media_export(GF_MediaExporter *dump);

#endif /*GPAC_DISABLE_MEDIA_EXPORT*/


#ifndef GPAC_DISABLE_ISOM_HINTING
/*
	RTP IsoMedia file hinting
*/
typedef struct __tag_isom_hinter GF_RTPHinter;

GF_RTPHinter *gf_hinter_track_new(GF_ISOFile *file, u32 TrackNum, 
							u32 Path_MTU, u32 max_ptime, u32 default_rtp_rate, u32 hint_flags, u8 PayloadID, 
							Bool copy_media, u32 InterleaveGroupID, u8 InterleaveGroupPriority, GF_Err *e);
/*delete the track hinter*/
void gf_hinter_track_del(GF_RTPHinter *tkHinter);
/*hints all samples in the media track*/
GF_Err gf_hinter_track_process(GF_RTPHinter *tkHint);
/*returns media bandwidth in kbps*/
u32 gf_hinter_track_get_bandwidth(GF_RTPHinter *tkHinter);
/*retrieves hinter flags*/
u32 gf_hinter_track_get_flags(GF_RTPHinter *tkHinter);
/*retrieves rtp payload name 
	@payloadName: static buffer for retrieval, minimum 30 bytes
*/
void gf_hinter_track_get_payload_name(GF_RTPHinter *tkHint, char *payloadName);

/*finalizes hinting process for the track (setup flags, write SDP for RTP, ...)
	@AddSystemInfo: if non-0, systems info are duplicated in the SDP (decoder cfg, PL IDs ..)
*/
GF_Err gf_hinter_track_finalize(GF_RTPHinter *tkHint, Bool AddSystemInfo);

/*SDP IOD flag*/
enum
{
	/*no IOD included*/
	GF_SDP_IOD_NONE = 0,
	/*base64 encoding of the regular MPEG-4 IOD*/
	GF_SDP_IOD_REGULAR,
	/*base64 encoding of IOD containing BIFS and OD tracks (one AU only) - this is used for ISMA 1.0 profiles
	note that the "hinted" file will loose all systems info*/
	GF_SDP_IOD_ISMA,
	/*same as ISMA but removes all clock references from IOD*/
	GF_SDP_IOD_ISMA_STRICT,
};

/*finalizes hinting process for the file (setup flags, write SDP for RTP, ...)
	@IOD_Profile: see above
	@bandwidth: total bandwidth in kbps of all hinted tracks, 0 means no bandwidth info at session level
*/
GF_Err gf_hinter_finalize(GF_ISOFile *file, u32 IOD_Profile, u32 bandwidth);


/*returns TRUE if the encoded data fits in an ESD url - streamType is the systems stream type needed to
signal data mime-type (OD, BIFS or any) */
Bool gf_hinter_can_embbed_data(char *data, u32 data_size, u32 streamType);


#endif /*GPAC_DISABLE_ISOM_HINTING*/


/*SAF Multiplexer object. The multiplexer supports concurencial (multi-threaded) access*/
typedef struct __saf_muxer GF_SAFMuxer;
/*SAF Multiplexer constructor*/
GF_SAFMuxer *gf_saf_mux_new();
/*SAF Multiplexer destructor*/
void gf_saf_mux_del(GF_SAFMuxer *mux);
/*adds a new stream in the SAF multiplex*/
GF_Err gf_saf_mux_stream_add(GF_SAFMuxer *mux, u32 stream_id, u32 ts_res, u32 buffersize_db, u8 stream_type, u8 object_type, char *mime_type, char *dsi, u32 dsi_len, char *remote_url);
/*removes a stream from the SAF multiplex*/
GF_Err gf_saf_mux_stream_rem(GF_SAFMuxer *mux, u32 stream_id);
/*adds an AU to the given stream. !!AU data will be freed by the multiplexer!! 
AUs are not reinterleaved based on their CTS, in order to enable audio interleaving
*/
GF_Err gf_saf_mux_add_au(GF_SAFMuxer *mux, u32 stream_id, u32 CTS, char *data, u32 data_len, Bool is_rap);
/*gets the content of the multiplexer for the given time.
if force_end_of_session is set, this flushes the SAF Session - no more operations will be allowed on the muxer*/
GF_Err gf_saf_mux_for_time(GF_SAFMuxer *mux, u32 time_ms, Bool force_end_of_session, char **out_data, u32 *out_size);

/*reduces input width/height to common aspect ration num/denum values*/
void gf_media_reduce_aspect_ratio(u32 *width, u32 *height);
/*reduces input FPS to a more compact value (eg 25000/1000 -> 25/1)*/
void gf_media_get_reduced_frame_rate(u32 *timescale, u32 *sample_dur);

#ifdef __cplusplus
}
#endif


#endif	/*_GF_MEDIA_H_*/

