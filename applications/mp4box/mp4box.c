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
#include <gpac/filters.h>

#include <gpac/mpd.h>

#define BUFFSIZE	8192
#define DEFAULT_INTERLEAVING_IN_SEC 0.5

//undefine to check validity of defined args' syntax
//#define TEST_ARGS


static u32 mp4box_cleanup(u32 ret_code);

/*
 * 		START OF ARGUMENT VALUES DECLARATION
 */



typedef struct
{
	TrackIdentifier track_id;
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
	META_ACTION_ADD_IMAGE_DERIVED	= 10,
} MetaActionType;

typedef struct {
	u32 ref_item_id;
	u32 ref_type;
} MetaRef;

typedef struct
{
	MetaActionType act_type;
	Bool root_meta, use_dref;
	TrackIdentifier track_id;
	u32 meta_4cc;
	char *szPath, *szName, *mime_type, *enc_type, *keep_props;
	u32 item_id;
	Bool primary;
	Bool replace;
	u32 item_type;
	u32 ref_item_id;
	GF_List *item_refs;
	u32 group_id;
	u32 group_type;
	GF_ImageItemProperties *image_props;
} MetaAction;


typedef enum {
	TRACK_ACTION_REM_TRACK= 0,
	TRACK_ACTION_SET_LANGUAGE,
	TRACK_ACTION_SET_DELAY,
	TRACK_ACTION_SET_KMS_URI,
	TRACK_ACTION_SET_PAR,
	TRACK_ACTION_SET_HANDLER_NAME,
	TRACK_ACTION_ENABLE,
	TRACK_ACTION_DISABLE,
	TRACK_ACTION_REFERENCE,
	TRACK_ACTION_RAW_EXTRACT,
	TRACK_ACTION_REM_NON_RAP,
	TRACK_ACTION_SET_KIND,
	TRACK_ACTION_REM_KIND,
	TRACK_ACTION_SET_ID,
	TRACK_ACTION_SET_UDTA,
	TRACK_ACTION_SWAP_ID,
	TRACK_ACTION_REM_NON_REFS,
	TRACK_ACTION_SET_CLAP,
	TRACK_ACTION_SET_MX,
	TRACK_ACTION_SET_EDITS,
	TRACK_ACTION_SET_TIME,
	TRACK_ACTION_SET_MEDIA_TIME,
} TrackActionType;

#define LANG_SIZE	50
typedef struct
{
	TrackActionType act_type;
	TrackIdentifier target_track;
	char lang[LANG_SIZE];
	GF_Fraction delay;
	const char *kms;
	const char *hdl_name;
	s32 par_num, par_den;
	u8 force_par, rewrite_bs;
	u32 dump_type, sample_num;
	char *out_name;
	char *src_name;
	char *string;
	u32 udta_type;
	char *kind_scheme, *kind_value;
	TrackIdentifier newTrackID;
	s32 clap_wnum, clap_wden, clap_hnum, clap_hden, clap_honum, clap_hoden, clap_vonum, clap_voden;
	s32 mx[9];
	u64 time;
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

typedef enum {
	TSEL_ACTION_SET_PARAM = 0,
	TSEL_ACTION_REMOVE_TSEL = 1,
	TSEL_ACTION_REMOVE_ALL_TSEL_IN_GROUP = 2,
} TSELActionType;

typedef struct
{
	TSELActionType act_type;
	TrackIdentifier target_track;
	TrackIdentifier reference_track;

	u32 criteria[30];
	u32 nb_criteria;
	Bool is_switchGroup;
	u32 switchGroupID;
} TSELAction;

GF_FileType get_file_type_by_ext(char *inName);

//All static variables - DO NOT set init value here, use \ref init_global_vars
char outfile[GF_MAX_PATH];

#ifndef GPAC_DISABLE_SCENE_ENCODER
GF_SMEncodeOptions smenc_opts;
#endif
#ifndef GPAC_DISABLE_SWF_IMPORT
u32 swf_flags;
#endif
GF_Fraction import_fps;

#if defined(GPAC_CONFIG_ANDROID) || defined(GPAC_CONFIG_EMSCRIPTEN)
u32 mp4b_help_flags;
FILE *mp4b_helpout;

#define help_flags mp4b_help_flags
#define helpout mp4b_helpout

#else
FILE *helpout;
u32 help_flags;
#endif

Double interleaving_time, split_duration, split_start, dash_duration, dash_subduration, swf_flatten_angle, mpd_live_duration, min_buffer, mpd_update_time;

Bool arg_parse_res, dash_duration_strict, dvbhdemux, keep_sys_tracks, align_cat;
Bool do_hint, do_save, full_interleave, do_frag, hint_interleave, dump_rtp, regular_iod, remove_sys_tracks, remove_hint, remove_root_od;
Bool print_sdp, open_edit, dump_cr, force_ocr, encode, do_scene_log, dump_srt, dump_ttxt, do_saf, dump_m2ts, dump_cart, dump_chunk, dump_check_xml, fuzz_chk;
Bool do_hash, verbose, force_cat, pack_wgt, single_group, clean_groups, dash_live, no_fragments_defaults, single_traf_per_moof, tfdt_per_traf;
Bool hls_clock, do_mpd_rip, merge_vtt_cues, get_nb_tracks, no_inplace, merge_last_seg, freeze_box_order, no_odf_conf;
Bool insert_utc, chunk_mode, HintCopy, hint_no_offset, do_bin_xml, frag_real_time, force_co64, live_scene, use_mfra, dump_iod, samplegroups_in_traf;
Bool mvex_after_traks, daisy_chain_sidx, use_ssix, single_segment, single_file, segment_timeline, has_add_image;
Bool strict_cues, use_url_template, seg_at_rap, frag_at_rap, memory_frags, keep_utc, has_next_arg, no_cache, no_loop;
Bool conv_type_from_ext;

u32 stat_level, hint_flags, import_flags, nb_add, nb_cat, crypt, agg_samples, nb_sdp_ex, max_ptime, split_size, nb_meta_act;
u32 nb_track_act, rtp_rate, major_brand, nb_alt_brand_add, nb_alt_brand_rem, old_interleave, minor_version, conv_type, nb_tsel_acts;
u32 program_number, time_shift_depth, initial_moof_sn, dump_std, import_subtitle, dump_saps_mode, force_new, compress_moov;
u32 track_dump_type, dump_isom, dump_timestamps, dump_nal_type, do_flat, print_info;
u32 size_top_box, fs_dump_flags, dump_chap, dump_udta_type, moov_pading, sdtp_in_traf, segment_marker, timescale, dash_scale;
u32 MTUSize, run_for, dash_cumulated_time, dash_prev_time, dash_now_time, adjust_split_end, nb_mpd_base_urls, nb_dash_inputs;

u64 initial_tfdt;

s32 subsegs_per_sidx, laser_resolution, ast_offset_ms;

char *inName, *outName, *mediaSource, *input_ctx, *output_ctx, *drm_file, *avi2raw, *cprt, *chap_file, *chap_file_qt, *itunes_tags, *pack_file;
char *raw_cat, *seg_name, *dash_ctx_file, *compress_top_boxes, *high_dynamc_range_filename, *use_init_seg, *box_patch_filename, *udp_dest;
char *do_mpd_conv, *dash_start_date, *dash_profile_extension, *dash_cues, *do_wget, *mux_name, *seg_ext, *init_seg_ext, *dash_title, *dash_source;
char *dash_more_info, *split_range_str;

GF_DASH_ContentLocationMode cp_location_mode;
GF_MemTrackerType mem_track;
GF_DASHPSSHMode pssh_mode;
GF_DashProfile dash_profile;
GF_DASH_SplitMode dash_split_mode;
GF_DashSwitchingMode bitstream_switching_mode;
GF_DashDynamicMode dash_mode;
#ifndef GPAC_DISABLE_SCENE_DUMP
GF_SceneDumpFormat dump_mode;
#endif

//things to free upon cleanup
MetaAction *metas;
TrackAction *tracks;
TSELAction *tsel_acts;
SDPLine *sdp_lines;
u32 *brand_add;
u32 *brand_rem;
char **mpd_base_urls;
GF_DashSegmenterInput *dash_inputs;
FILE *logfile;
GF_ISOFile *file;

TrackIdentifier info_track_id;
TrackIdentifier ttxt_track_id;
TrackIdentifier dump_nal_track;
TrackIdentifier dump_saps_track;
TrackIdentifier box_patch_track;
TrackIdentifier dump_udta_track;


static void init_global_vars()
{
	//Bool
	dash_duration_strict = dvbhdemux = keep_sys_tracks = do_hint = do_save = full_interleave = do_frag = hint_interleave = GF_FALSE;
	dump_rtp = regular_iod = remove_sys_tracks = remove_hint = remove_root_od = print_sdp = open_edit = GF_FALSE;
	dump_cr = force_ocr = encode = do_scene_log = dump_srt = dump_ttxt = do_saf = dump_m2ts = dump_cart = dump_chunk = GF_FALSE;
	dump_check_xml = do_hash = verbose = force_cat = pack_wgt = single_group = clean_groups = dash_live = no_fragments_defaults = fuzz_chk = GF_FALSE;
	single_traf_per_moof = tfdt_per_traf = hls_clock = do_mpd_rip = merge_vtt_cues = get_nb_tracks = GF_FALSE;
	no_inplace = merge_last_seg = freeze_box_order = no_odf_conf = GF_FALSE;
	insert_utc = chunk_mode = HintCopy = hint_no_offset = do_bin_xml = frag_real_time = force_co64 = live_scene = GF_FALSE;
	use_mfra = dump_iod = samplegroups_in_traf = mvex_after_traks = daisy_chain_sidx = use_ssix = single_segment = single_file = GF_FALSE;
	segment_timeline = has_add_image = strict_cues = use_url_template = seg_at_rap = frag_at_rap = memory_frags = keep_utc = GF_FALSE;
	has_next_arg = no_cache = no_loop = GF_FALSE;
	conv_type_from_ext = GF_FALSE;

	/*align cat is the new default behavior for -cat*/
	align_cat=GF_TRUE;

	//u32
	arg_parse_res = nb_mpd_base_urls = nb_dash_inputs = help_flags = 0;
	stat_level = hint_flags = import_flags = nb_add = nb_cat = crypt = 0;
	agg_samples = nb_sdp_ex = max_ptime = split_size = nb_meta_act = nb_track_act = 0;
	rtp_rate = major_brand = nb_alt_brand_add = nb_alt_brand_rem = old_interleave = minor_version = 0;
	conv_type = nb_tsel_acts = program_number = time_shift_depth = initial_moof_sn = dump_std = 0;
	import_subtitle = dump_saps_mode = force_new = compress_moov = 0;
	track_dump_type = dump_isom = dump_timestamps = dump_nal_type = do_flat = print_info = 0;
	size_top_box = fs_dump_flags = dump_chap = dump_udta_type = moov_pading = sdtp_in_traf = 0;
	segment_marker = timescale = adjust_split_end = run_for = dash_cumulated_time = dash_prev_time = dash_now_time = 0;
	dash_scale = 1000;
	MTUSize = 1450;

	//s32
	subsegs_per_sidx = laser_resolution = ast_offset_ms = 0;

	//Double
	interleaving_time = split_duration = dash_duration = dash_subduration = swf_flatten_angle = mpd_live_duration = mpd_update_time = 0.0;
	split_start = -1.0;
	min_buffer = 1.5;

	//u64
	initial_tfdt = 0;

	//char
	outfile[0] = 0;

	//char *
	split_range_str = inName = outName = mediaSource = input_ctx = output_ctx = drm_file = avi2raw = cprt = NULL;
	chap_file = chap_file_qt = itunes_tags = pack_file = raw_cat = seg_name = dash_ctx_file = compress_top_boxes = NULL;
	high_dynamc_range_filename = use_init_seg = box_patch_filename = udp_dest = do_mpd_conv = dash_start_date = NULL;
	dash_profile_extension = dash_cues = do_wget = mux_name = seg_ext =  init_seg_ext = dash_title = dash_source = dash_more_info = NULL;

#ifndef GPAC_DISABLE_SCENE_ENCODER
	memset(&smenc_opts, 0, sizeof(GF_SMEncodeOptions));
#endif
#ifndef GPAC_DISABLE_SWF_IMPORT
	swf_flags = GF_SM_SWF_SPLIT_TIMELINE;
#endif
	import_fps.num = 0;
	import_fps.den = 0;

	memset(&info_track_id, 0, sizeof(TrackIdentifier));
	memset(&ttxt_track_id, 0, sizeof(TrackIdentifier));
	memset(&dump_nal_track, 0, sizeof(TrackIdentifier));
	memset(&dump_saps_track, 0, sizeof(TrackIdentifier));
	memset(&box_patch_track, 0, sizeof(TrackIdentifier));
	memset(&dump_udta_track, 0, sizeof(TrackIdentifier));

	//allocated
	metas = NULL;
	tracks = NULL;
	tsel_acts = NULL;
	sdp_lines = NULL;
	brand_add = NULL;
	brand_rem = NULL;
	mpd_base_urls = NULL;
	dash_inputs = NULL;

	//constants
#ifndef GPAC_DISABLE_SCENE_DUMP
	dump_mode=GF_SM_DUMP_NONE;
#endif
	bitstream_switching_mode = GF_DASH_BSMODE_DEFAULT;
	dash_mode = GF_DASH_STATIC;
	cp_location_mode = GF_DASH_CPMODE_ADAPTATION_SET;
	pssh_mode = GF_DASH_PSSH_MOOV;
	dash_split_mode = GF_DASH_SPLIT_OUT;
	dash_profile = GF_DASH_PROFILE_AUTO;
	mem_track = GF_MemTrackerNone;

	logfile = NULL;
	helpout = NULL;
	file = NULL;
}


typedef u32 (*parse_arg_fun)(char *arg_val, u32 param);
typedef u32 (*parse_arg_fun2)(char *arg_name, char *arg_val, u32 param);

//other custom option parsing functions definitions are in mp4box.h
static u32 parse_meta_args(char *opts, MetaActionType act_type);
static Bool parse_tsel_args(char *opts, TSELActionType act);

u32 parse_u32(char *val, char *log_name)
{
	u32 res;
	if (sscanf(val, "%u", &res)==1) return res;
	M4_LOG(GF_LOG_ERROR, ("%s must be an unsigned integer (got %s), using 0\n", log_name, val));
	return 0;
}

s32 parse_s32(char *val, char *log_name)
{
	s32 res;
	if (sscanf(val, "%d", &res)==1) return res;
	M4_LOG(GF_LOG_ERROR, ("%s must be a signed integer (got %s), using 0\n", log_name, val));
	return 0;
}

static Bool parse_track_id(TrackIdentifier *tkid, char *arg_val, Bool allow_all)
{
	tkid->ID_or_num = 0;
	tkid->type = 0;

	if (!strcmp(arg_val, "*")) {
		if (allow_all) {
			tkid->ID_or_num = (u32) -1;
			return GF_TRUE;
		}
		return GF_FALSE;
	}
	if (!strncmp(arg_val, "video", 5)) {
		arg_val += 5;
		tkid->type = 2;
	}
	else if (!strncmp(arg_val, "audio", 5)) {
		arg_val += 5;
		tkid->type = 3;
	}
	else if (!strncmp(arg_val, "text", 4)) {
		arg_val += 4;
		tkid->type = 4;
	}
	else if (!strnicmp(arg_val, "n", 1)) {
		arg_val += 1;
		tkid->type = 1;
	}

	if (!arg_val[0]) return (tkid->type>1) ? GF_TRUE : GF_FALSE;
	u32 len = (u32) strlen(arg_val);
	while (len) {
		if ((arg_val[len-1]<'0') || (arg_val[len-1]>'9')) {
			tkid->ID_or_num = 0;
			tkid->type = 0;
			return GF_FALSE;
		}
		len--;
	}
	//don't use parse_u32 we don't want a warning
	if (sscanf(arg_val, "%u", &tkid->ID_or_num) != 1)
		tkid->ID_or_num = 0;
	if (!tkid->ID_or_num) {
		tkid->type = 0;
		return GF_FALSE;
	}
	return GF_TRUE;
}

/*
 * 		START OF ARGS PARSING AND HELP
 */


Bool print_version(char *arg_val, u32 param)
{
	fprintf(stderr, "MP4Box - GPAC version %s\n"
	        "%s\n"
	        "GPAC Configuration: " GPAC_CONFIGURATION "\n"
	        "Features: %s %s\n", gf_gpac_version(), gf_gpac_copyright_cite(), gf_sys_features(GF_FALSE), gf_sys_features(GF_TRUE));
	return GF_TRUE;
}

//arg will toggle open_edit
#define ARG_OPEN_EDIT		1
//arg will toggle do_save
#define ARG_NEED_SAVE		1<<1
#define ARG_NO_INPLACE		1<<2
#define ARG_BIT_MASK		1<<3
#define ARG_BIT_MASK_REM	1<<4
#define ARG_HAS_VALUE		1<<5
#define ARG_DIV_1000		1<<6
#define ARG_NON_ZERO		1<<7
#define ARG_64BITS			1<<8
#define ARG_IS_4CC			1<<9
#define ARG_BOOL_REV		1<<10
#define ARG_INT_INC			1<<11
#define ARG_IS_FUN			1<<12
#define ARG_EMPTY			1<<13
#define ARG_PUSH_SYSARGS	1<<14
#define ARG_IS_FUN2			1<<15



typedef struct
{
	GF_GPAC_ARG_BASE

	void *arg_ptr;
	u32 argv_val;
	u16 parse_flags;
} MP4BoxArg;

#define MP4BOX_ARG(_a, _c, _f, _g, _h, _i, _j) {_a, NULL, _c, NULL, NULL, _f, _g, _h, _i, _j}
#define MP4BOX_ARG_ALT(_a, _b, _c, _f, _g, _h, _i, _j) {_a, _b, _c, NULL, NULL, _f, _g, _h, _i, _j}
#define MP4BOX_ARG_S(_a, _s, _c, _g, _h, _i, _j) {_a, NULL, _c, _s, NULL, GF_ARG_CUSTOM, _g, _h, _i, _j}
#define MP4BOX_ARG_S_ALT(_a, _b, _s, _c, _g, _h, _i, _j) {_a, _b, _c, _s, NULL, GF_ARG_CUSTOM, _g, _h, _i, _j}


MP4BoxArg m4b_gen_args[] =
{
#ifdef GPAC_MEMORY_TRACKING
 	MP4BOX_ARG("mem-track", "enable memory tracker", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, NULL, 0, 0),
 	MP4BOX_ARG("mem-track-stack", "enable memory tracker with stack dumping", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, NULL, 0, 0),
#endif
 	MP4BOX_ARG("p", "use indicated profile for the global GPAC config. If not found, config file is created. If a file path is indicated, this will load profile from that file. Otherwise, this will create a directory of the specified name and store new config there. Reserved name `0` means a new profile, not stored to disk. Works using -p=NAME or -p NAME", GF_ARG_STRING, GF_ARG_HINT_EXPERT, NULL, 0, 0),
 	{"inter", NULL, "interleave file, producing track chunks with given duration in ms. A value of 0 disables interleaving ", "0.5", NULL, GF_ARG_DOUBLE, 0, parse_store_mode, 0, ARG_IS_FUN},
 	MP4BOX_ARG("old-inter", "same as [-inter]() but without drift correction", GF_ARG_DOUBLE, GF_ARG_HINT_EXPERT, parse_store_mode, 1, ARG_IS_FUN),
 	MP4BOX_ARG("tight", "tight interleaving (sample based) of the file. This reduces disk seek operations but increases file size", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &full_interleave, 0, ARG_OPEN_EDIT|ARG_NEED_SAVE),
 	MP4BOX_ARG("flat", "store file with all media data first, non-interleaved. This speeds up writing time when creating new files", GF_ARG_BOOL, 0, &do_flat, 0, ARG_NO_INPLACE),
 	MP4BOX_ARG("frag", "fragment file, producing track fragments of given duration in ms. This disables interleaving", GF_ARG_DOUBLE, 0, parse_store_mode, 2, ARG_IS_FUN),
 	MP4BOX_ARG("out", "specify ISOBMFF output file name. By default input file is overwritten", GF_ARG_STRING, 0, &outName, 0, 0),
 	MP4BOX_ARG("co64","force usage of 64-bit chunk offsets for ISOBMF files", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &force_co64, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("new", "force creation of a new destination file", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &force_new, 0, 0),
 	MP4BOX_ARG("newfs", "force creation of a new destination file without temp file but interleaving support", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, parse_store_mode, 3, ARG_IS_FUN),
 	MP4BOX_ARG_ALT("no-sys", "nosys", "remove all MPEG-4 Systems info except IOD, kept for profiles. This is the default when creating regular AV content", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &remove_sys_tracks, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("no-iod", "remove MPEG-4 InitialObjectDescriptor from file", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &remove_root_od, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("mfra", "insert movie fragment random offset when fragmenting file (ignored in dash mode)", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &use_mfra, 0, 0),
 	MP4BOX_ARG("isma", "rewrite the file as an ISMA 1.0 file", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &conv_type, GF_ISOM_CONV_TYPE_ISMA, ARG_OPEN_EDIT),
 	MP4BOX_ARG("ismax", "same as [-isma]() and remove all clock references", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &conv_type, GF_ISOM_CONV_TYPE_ISMA_EX, ARG_OPEN_EDIT),
 	MP4BOX_ARG("3gp", "rewrite as 3GPP(2) file (no more MPEG-4 Systems Info), always enabled if destination file extension is `.3gp`, `.3g2` or `.3gpp`. Some tracks may be removed in the process", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &conv_type, GF_ISOM_CONV_TYPE_3GPP, ARG_OPEN_EDIT),
 	MP4BOX_ARG("ipod", "rewrite the file for iPod/old iTunes", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &conv_type, GF_ISOM_CONV_TYPE_IPOD, ARG_OPEN_EDIT),
 	MP4BOX_ARG("psp", "rewrite the file for PSP devices", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &conv_type, GF_ISOM_CONV_TYPE_PSP, ARG_OPEN_EDIT),
 	MP4BOX_ARG("brand", "set major brand of file (`ABCD`) or brand with optional version (`ABCD:v`)", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, parse_brand, 0, ARG_IS_FUN),
 	MP4BOX_ARG("ab", "add given brand to file's alternate brand list", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, parse_brand, 1, ARG_IS_FUN),
 	MP4BOX_ARG("rb", "remove given brand to file's alternate brand list", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, parse_brand, 2, ARG_IS_FUN),
 	MP4BOX_ARG("cprt", "add copyright string to file", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, &cprt, 0, 0),
 	MP4BOX_ARG("chap", "set chapter information from given file. The following formats are supported (but cannot be mixed) in the chapter text file:\n"
		"  - ZoomPlayer: `AddChapter(nb_frames,chapter name)`, `AddChapterBySeconds(nb_sec,chapter name)` and `AddChapterByTime(h,m,s,chapter name)` with 1 chapter per line\n"
		"  - Time codes: `h:m:s chapter_name`, `h:m:s:ms chapter_name` and `h:m:s.ms chapter_name` with 1 chapter per line\n"
		"  - SMPTE codes: `h:m:s;nb_f/fps chapter_name` and `h:m:s;nb_f chapter_name` with `nb_f` the number of frames and `fps` the framerate with 1 chapter per line\n"
		"  - Common syntax: `CHAPTERX=h:m:s[:ms or .ms]` on first line and `CHAPTERXNAME=name` on next line (reverse order accepted)", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, &chap_file, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("chapqt", "set chapter information from given file, using QT signaling for text tracks", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, &chap_file_qt, 0, ARG_OPEN_EDIT),
	MP4BOX_ARG_S("set-track-id", "tkID:id2", "change id of track to id2", 0, parse_track_action, TRACK_ACTION_SET_ID, ARG_IS_FUN),
	MP4BOX_ARG_S("swap-track-id", "tkID1:tkID1", "swap the id between tracks with id1 to id2", 0, parse_track_action, TRACK_ACTION_SWAP_ID, ARG_IS_FUN),
 	MP4BOX_ARG("rem", "remove given track from file", GF_ARG_INT, 0, parse_track_action, TRACK_ACTION_REM_TRACK, ARG_IS_FUN),
 	MP4BOX_ARG("rap", "remove all non-RAP samples from given track", GF_ARG_INT, GF_ARG_HINT_ADVANCED, parse_rap_ref, 0, ARG_IS_FUN | ARG_EMPTY),
 	MP4BOX_ARG("refonly", "remove all non-reference pictures from given track", GF_ARG_INT, GF_ARG_HINT_ADVANCED, parse_rap_ref, 1, ARG_IS_FUN | ARG_EMPTY),
 	MP4BOX_ARG("enable", "enable given track", GF_ARG_INT, 0, parse_track_action, TRACK_ACTION_ENABLE, ARG_IS_FUN),
 	MP4BOX_ARG("disable", "disable given track", GF_ARG_INT, 0, parse_track_action, TRACK_ACTION_DISABLE, ARG_IS_FUN),
 	{"timescale", NULL, "set movie timescale to given value (ticks per second)", "600", NULL, GF_ARG_INT, 0, &timescale, 0, ARG_OPEN_EDIT},
 	MP4BOX_ARG_S("lang", "[tkID=]LAN", "set language. LAN is the BCP-47 code (eng, en-UK, ...). If no track ID is given, sets language to all tracks", 0, parse_track_action, TRACK_ACTION_SET_LANGUAGE, ARG_IS_FUN),
 	MP4BOX_ARG_S("delay", "tkID=TIME", "set track start delay (>0) or initial skip (<0) in ms or in fractional seconds (`N/D`)", 0, parse_track_action, TRACK_ACTION_SET_DELAY, ARG_IS_FUN),
 	MP4BOX_ARG_S("par", "tkID=PAR", "set visual track pixel aspect ratio. PAR is:\n"
					"  - N:D: set PAR to N:D in track, do not modify the bitstream\n"
					"  - wN:D: set PAR to N:D in track and try to modify the bitstream\n"
					"  - none: remove PAR info from track, do not modify the bitstream\n"
					"  - auto: retrieve PAR info from bitstream and set it in track\n"
					"  - force: force 1:1 PAR in track, do not modify the bitstream", GF_ARG_HINT_ADVANCED, parse_track_action, TRACK_ACTION_SET_PAR, ARG_IS_FUN
					),
 	MP4BOX_ARG_S("clap", "tkID=CLAP", "set visual track clean aperture. CLAP is `Wn,Wd,Hn,Hd,HOn,HOd,VOn,VOd` or `none`\n"
 			"- n, d: numerator, denominator\n"
	        "- W, H, HO, VO: clap width, clap height, clap horizontal offset, clap vertical offset\n"
 			, GF_ARG_HINT_ADVANCED, parse_track_action, TRACK_ACTION_SET_CLAP, ARG_IS_FUN),
 	MP4BOX_ARG_S("mx", "tkID=MX", "set track matrix, with MX is M1:M2:M3:M4:M5:M6:M7:M8:M9 in 16.16 fixed point integers or hexa"
 			, GF_ARG_HINT_ADVANCED, parse_track_action, TRACK_ACTION_SET_MX, ARG_IS_FUN),
	MP4BOX_ARG_S("kind", "tkID=schemeURI=value", "set kind for the track or for all tracks using `all=schemeURI=value`", 0, parse_track_action, TRACK_ACTION_SET_KIND, ARG_IS_FUN),
	MP4BOX_ARG_S("kind-rem", "tkID=schemeURI=value", "remove kind if given schemeID for the track or for all tracks with `all=schemeURI=value`", 0, parse_track_action, TRACK_ACTION_REM_KIND, ARG_IS_FUN),
 	MP4BOX_ARG_S("name", "tkID=NAME", "set track handler name to NAME (UTF-8 string)", GF_ARG_HINT_ADVANCED, parse_track_action, TRACK_ACTION_SET_HANDLER_NAME, ARG_IS_FUN),
	MP4BOX_ARG_ALT("tags", "itags", "set iTunes tags to file, see `-h tags`", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, &itunes_tags, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("group-add", "create a new grouping information in the file. Format is a colon-separated list of following options:\n"
	        "- refTrack=ID: track used as a group reference. If not set, the track will belong to the same group as the "
	        "previous trackID specified. If 0 or no previous track specified, a new alternate group will be created\n"
	        "- switchID=ID: ID of the switch group to create. If 0, a new ID will be computed for you. If <0, disables SwitchGroup\n"
	        "- criteria=string: list of space-separated 4CCs\n"
	        "- trackID=ID: track to add to this group\n"
	        "  \n"
	        "Warning: Options modify state as they are parsed, `trackID=1:criteria=lang:trackID=2` is different from `criteria=lang:trackID=1:trackID=2`"
	        "\n", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, parse_tsel_args, TSEL_ACTION_SET_PARAM, ARG_IS_FUN),

	MP4BOX_ARG("group-rem-track", "remove given track from its group", GF_ARG_INT, GF_ARG_HINT_ADVANCED, parse_tsel_args, TSEL_ACTION_REMOVE_TSEL, ARG_IS_FUN),
	MP4BOX_ARG("group-rem", "remove the track's group", GF_ARG_INT, GF_ARG_HINT_ADVANCED, parse_tsel_args, TSEL_ACTION_REMOVE_ALL_TSEL_IN_GROUP, ARG_IS_FUN),
	MP4BOX_ARG("group-clean", "remove all group information from all tracks", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &clean_groups, 0, ARG_OPEN_EDIT),
	MP4BOX_ARG_S("ref", "tkID:R4CC:refID", "add a reference of type R4CC from track ID to track refID (remove track reference if refID is 0)", GF_ARG_HINT_ADVANCED, parse_track_action, TRACK_ACTION_REFERENCE, ARG_IS_FUN),
	MP4BOX_ARG("keep-utc", "keep UTC timing in the file after edit", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &keep_utc, 0, 0),
	MP4BOX_ARG_S("udta", "tkID:[OPTS]", "set udta for given track or movie if tkID is 0. OPTS is a colon separated list of:\n"
	        "- type=CODE: 4CC code of the UDTA (not needed for `box=` option)\n"
	        "- box=FILE: location of the udta data, formatted as serialized boxes\n"
	        "- box=base64,DATA: base64 encoded udta data, formatted as serialized boxes\n"
	        "- src=FILE: location of the udta data (will be stored in a single box of type CODE)\n"
	        "- src=base64,DATA: base64 encoded udta data (will be stored in a single box of type CODE)\n"
	        "- str=STRING: use the given string as payload for the udta box\n"
	        "Note: If no source is set, UDTA of type CODE will be removed\n", GF_ARG_HINT_ADVANCED, parse_track_action, TRACK_ACTION_SET_UDTA, ARG_IS_FUN|ARG_OPEN_EDIT),
	MP4BOX_ARG_S("patch", "[tkID=]FILE", "apply box patch described in FILE, for given trackID if set", GF_ARG_HINT_ADVANCED, parse_boxpatch, 0, ARG_IS_FUN),
	MP4BOX_ARG("bo", "freeze the order of boxes in input file", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, &freeze_box_order, 0, 0),
	MP4BOX_ARG("init-seg", "use the given file as an init segment for dumping or for encryption", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, &use_init_seg, 0, 0),
	MP4BOX_ARG("zmov", "compress movie box according to ISOBMFF box compression or QT if mov extension", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, parse_compress, 0, ARG_IS_FUN),
	MP4BOX_ARG("xmov", "same as zmov and wraps ftyp in otyp", GF_ARG_BOOL, GF_ARG_HINT_ADVANCED, parse_compress, 1, ARG_IS_FUN),
 	MP4BOX_ARG_S("edits", "tkID=EDITS", "set edit list. The following syntax is used (no separators between entries):\n"
			" - `r`: removes all edits\n"
			" - `eSTART`: add empty edit with given start time. START can be\n"
			"   - `VAL`: start time in seconds (int, double, fraction), media duration used as edit duration\n"
			"   - `VAL-DUR`: start time and duration in seconds (int, double, fraction)\n"
			" - `eSTART,MEDIA[,RATE]`: add regular edit with given start, media start time in seconds (int, double, fraction) and rate (fraction or INT)\n"
			" - Examples: \n"
			"   - `re0-5e5-3,4`: remove edits, add empty edit at 0s for 5s, then add regular edit at 5s for 3s starting at 4s in media track\n"
			"   - `re0-4,0,0.5`: remove edits, add single edit at 0s for 4s starting at 0s in media track and playing at speed 0.5\n"
				, 0, parse_track_action, TRACK_ACTION_SET_EDITS, ARG_IS_FUN),
 	MP4BOX_ARG("moovpad", "specify amount of padding to keep after moov box for later inplace editing - if 0, moov padding is disabled", GF_ARG_INT, GF_ARG_HINT_EXPERT, &moov_pading, 0, ARG_NEED_SAVE),
 	MP4BOX_ARG("no-inplace", "disable inplace rewrite", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &no_inplace, 0, 0),
 	MP4BOX_ARG("hdr", "update HDR information based on given XML, 'none' removes HDR info", GF_ARG_STRING, GF_ARG_HINT_EXPERT, &high_dynamc_range_filename, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG_S("time", "[tkID=]DAY/MONTH/YEAR-H:M:S", "set movie or track creation time", GF_ARG_HINT_EXPERT, parse_track_action, TRACK_ACTION_SET_TIME, ARG_IS_FUN),
 	MP4BOX_ARG_S("mtime", "tkID=DAY/MONTH/YEAR-H:M:S", "set media creation time", GF_ARG_HINT_EXPERT, parse_track_action, TRACK_ACTION_SET_MEDIA_TIME, ARG_IS_FUN),
	{0}
};

void PrintGeneralUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# General Options\n"
		"MP4Box is a multimedia packager, with a vast number of functionalities: conversion, splitting, hinting, dumping, DASH-ing, encryption, transcoding and others.\n"
		"MP4Box provides a large set of options, classified by categories (see [-h]()). These options do not follow any particular ordering.\n"
		"  \n"
		"By default, MP4Box rewrites the input file. You can change this behavior by using the [-out]() option.\n"
		"MP4Box stores by default the file with 0.5 second interleaving and meta-data (`moov` ...) at the beginning, making it suitable for HTTP download-and-play. This may however takes longer to store the file, use [-flat]() to change this behavior.\n"
		"  \n"
		"MP4Box usually generates a temporary file when creating a new IsoMedia file. The location of this temporary file is OS-dependent, and it may happen that the drive/partition the temporary file is created on has not enough space or no write access. In such a case, you can specify a temporary file location with [-tmp]().\n"
		"  \n"
		"Track identifier for track-based operations (usually referred to as `tkID` in the help) use the following syntax:\n"
		"- INT: target is track with ID `INT`\n"
		"- n`INT`: target is track number `INT`\n"
		"- `audio`, `video`, `text`: target is first `audio`, `video` or `text` track\n"
		"- `audioN`, `videoN`, `textN`: target is the `N`th `audio`, `video` or `text` track, with `N=1` being the first track of desired type\n"
		"  \n"
		"Option values:\n"
		"Unless specified otherwise, a track operation option of type `integer` expects a track identifier value following it.\n"
		"An option of type `boolean` expects no following value.\n"
		"  \n"
	);


	while (m4b_gen_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_gen_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-gen");
	}
}


MP4BoxArg m4b_split_args[] =
{
 	MP4BOX_ARG("split", "split in files of given max duration (float number) in seconds. A trailing unit can be specified:\n"
	"- `M`, `m`: duration is in minutes\n"
	"- `H`, `h`: size is in hours", GF_ARG_STRING, 0, parse_split, 0, ARG_IS_FUN),
	MP4BOX_ARG_ALT("split-rap", "splitr", "split in files at each new RAP", GF_ARG_STRING, 0, parse_split, 1, ARG_IS_FUN),
	MP4BOX_ARG_ALT("split-size", "splits", "split in files of given max size (integer number) in kilobytes. A trailing unit can be specified:\n"
	"- `M`, `m`: size is in megabytes\n"
	"- `G`, `g`: size is in gigabytes", GF_ARG_STRING, 0, parse_split, 2, ARG_IS_FUN),
	MP4BOX_ARG_ALT("split-chunk", "splitx", "extract the specified time range as follows:\n"
	"- the start time is moved to the RAP sample closest to the specified start time\n"
	"- the end time is kept as requested"
		, GF_ARG_STRING, 0, parse_split, 3, ARG_IS_FUN),
	MP4BOX_ARG("splitz", "extract the specified time range so that ranges `A:B` and `B:C` share exactly the same boundary `B`:\n"
	"- the start time is moved to the RAP sample at or after the specified start time\n"
	"- the end time is moved to the frame preceding the RAP sample at or following the specified end time"
		, GF_ARG_STRING, 0, parse_split, 4, ARG_IS_FUN),
	MP4BOX_ARG("splitg", "extract the specified time range as follows:\n"
	"- the start time is moved to the RAP sample at or before the specified start time\n"
	"- the end time is moved to the frame preceding the RAP sample at or following the specified end time"
		, GF_ARG_STRING, 0, parse_split, 5, ARG_IS_FUN),
	MP4BOX_ARG("splitf", "extract the specified time range and insert edits such that the extracted output is exactly the specified range\n", GF_ARG_STRING, 0, parse_split, 6, ARG_IS_FUN),
	{0}
};


static void PrintSplitUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "  \n"
		"# File splitting\n"
		"MP4Box can split input files by size, duration or extract a given part of the file to new IsoMedia file(s).\n"
		"This requires that at most one track in the input file has non random-access points (typically one video track at most).\n"
		"Splitting will ignore all MPEG-4 Systems tracks and hint tracks, but will try to split private media tracks.\n"
		"The input file must have enough random access points in order to be split. If this is not the case, you will have to re-encode the content.\n"
		"You can add media to a file and split it in the same pass. In this case, the destination file (the one which would be obtained without splitting) will not be stored.\n"
		"  \n"
		"Time ranges are specified as follows:\n"
		"- `S-E`: `S` start and `E` end times, formatted as `HH:MM:SS.ms`, `MM:SS.ms` or time in seconds (int, double, fraction)\n"
		"- `S:E`: `S` start time and `E` end times in seconds (int, double, fraction). If `E` is prefixed with `D`, this sets `E = S + time`\n"
		"- `S:end` or `S:end-N`: `S` start time in seconds (int, double), `N` number of seconds (int, double) before the end\n"
		"  \n"
		"MP4Box splitting runs a filter session using the `reframer` filter as follows:\n"
		"- `splitrange` option of the reframer is always set\n"
		"- source is demultiplexed with `alltk` option set\n"
		"- start and end ranges are passed to `xs` and `xe` options of the reframer\n"
		"- for `-splitz`, options `xadjust` and `xround=after` are enforced\n"
		"- for `-splitg`, options `xadjust` and `xround=before` are enforced\n"
		"- for `-splitf`, option `xround=seek` is enforced and `propbe_ref`set if not specified at prompt\n"
		"- for `-splitx`, option `xround=closest` and `propbe_ref` are enforced if not specified at prompt\n"
		"  \n"
		"The default output storage mode is to full interleave and will require a temp file for each output. This behavior can be modified using `-flat`, `-newfs`, `-inter` and `-frag`.\n"
		"The output file name(s) can be specified using `-out` and templates (e.g. `-out split$num%%04d$.mp4` produces split0001.mp4, split0002.mp4, ...).\n"
		"Multiple time ranges can be specified as a comma-separated list for `-splitx`, `-splitz` and `-splitg`.\n"
		"  \n"
	);

	i=0;
	while (m4b_split_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_split_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-split");
	}

}


MP4BoxArg m4b_dash_args[] =
{
 	MP4BOX_ARG ("dash", "create DASH from input files with given segment (subsegment for onDemand profile) duration in ms", GF_ARG_DOUBLE, 0, &dash_duration, 0, ARG_NON_ZERO),
 	MP4BOX_ARG("dash-live", "generate a live DASH session using the given segment duration in ms; using `-dash-live=F` will also write the live context to `F`. MP4Box will run the live session until `q` is pressed or a fatal error occurs", GF_ARG_DOUBLE, 0, parse_dashlive, 0, ARG_IS_FUN2),
 	MP4BOX_ARG("ddbg-live", "same as [-dash-live]() without time regulation for debug purposes", GF_ARG_DOUBLE, 0, parse_dashlive, 1, ARG_IS_FUN2),
	MP4BOX_ARG("frag", "specify the fragment duration in ms. If not set, this is the DASH duration (one fragment per segment)", GF_ARG_DOUBLE, 0, parse_store_mode, 2, ARG_IS_FUN),
	MP4BOX_ARG("out", "specify the output MPD file name", GF_ARG_STRING, 0, &outName, 0, 0),
	MP4BOX_ARG_ALT("profile", "dash-profile", "specify the target DASH profile, and set default options to ensure conformance to the desired profile. Default profile is `full` in static mode, `live` in dynamic mode (old syntax using `:live` instead of `.live` as separator still possible). Defined values are onDemand, live, main, simple, full, hbbtv1.5.live, dashavc264.live, dashavc264.onDemand, dashif.ll", GF_ARG_STRING, 0, parse_dash_profile, 0, ARG_IS_FUN),
	MP4BOX_ARG("profile-ext", "specify a list of profile extensions, as used by DASH-IF and DVB. The string will be colon-concatenated with the profile used", GF_ARG_STRING, 0, &dash_profile_extension, 0, 0),
	MP4BOX_ARG("rap", "ensure that segments begin with random access points, segment durations might vary depending on the source encoding", GF_ARG_BOOL, 0, parse_rap_ref, 0, ARG_IS_FUN | ARG_EMPTY),
	MP4BOX_ARG("frag-rap", "ensure that all fragments begin with random access points (duration might vary depending on the source encoding)", GF_ARG_BOOL, 0, &frag_at_rap, 0, 0),
	MP4BOX_ARG("segment-name", "set the segment name for generated segments. If not set (default), segments are concatenated in output file except in `live` profile where `dash_%%s`. Supported replacement strings are:\n"
	        "- $Number[%%0Nd]$ is replaced by the segment number, possibly prefixed with 0\n"
	        "- $RepresentationID$ is replaced by representation name\n"
	        "- $Time$ is replaced by segment start time\n"
	        "- $Bandwidth$ is replaced by representation bandwidth\n"
	        "- $Init=NAME$ is replaced by NAME for init segment, ignored otherwise\n"
	        "- $Index=NAME$ is replaced by NAME for index segments, ignored otherwise\n"
	        "- $Path=PATH$ is replaced by PATH when creating segments, ignored otherwise\n"
	        "- $Segment=NAME$ is replaced by NAME for media segments, ignored for init segments", GF_ARG_STRING, 0, &seg_name, 0, 0),
	{"segment-ext", NULL, "set the segment extension, `null` means no extension", "m4s", NULL, GF_ARG_STRING, 0, &seg_ext, 0, 0},
	{"init-segment-ext", NULL, "set the segment extension for init, index and bitstream switching segments, `null` means no extension\n", "mp4", NULL, GF_ARG_STRING, 0, &init_seg_ext, 0, 0},
	MP4BOX_ARG("segment-timeline", "use `SegmentTimeline` when generating segments", GF_ARG_BOOL, 0, &segment_timeline, 0, 0),
	MP4BOX_ARG("segment-marker", "add a box of given type (4CC) at the end of each DASH segment", GF_ARG_STRING, 0, &segment_marker, 0, ARG_IS_4CC),
	MP4BOX_ARG("insert-utc", "insert UTC clock at the beginning of each ISOBMF segment", GF_ARG_BOOL, 0, &insert_utc, 0, 0),
	MP4BOX_ARG("base-url", "set Base url at MPD level. Can be used several times.  \nWarning: this does not  modify generated files location", GF_ARG_STRING, 0, parse_base_url, 0, ARG_IS_FUN),
	MP4BOX_ARG("mpd-title", "set MPD title", GF_ARG_STRING, 0, &dash_title, 0, 0),
	MP4BOX_ARG("mpd-source", "set MPD source", GF_ARG_STRING, 0, &dash_source, 0, 0),
	MP4BOX_ARG("mpd-info-url", "set MPD info url", GF_ARG_STRING, 0, &dash_more_info, 0, 0),
 	MP4BOX_ARG("cprt", "add copyright string to MPD", GF_ARG_STRING, GF_ARG_HINT_ADVANCED, &cprt, 0, 0),
	MP4BOX_ARG("dash-ctx", "store/restore DASH timing from indicated file", GF_ARG_STRING, 0, &dash_ctx_file, 0, 0),
	MP4BOX_ARG("dynamic", "use dynamic MPD type instead of static", GF_ARG_BOOL, 0, &dash_mode, GF_DASH_DYNAMIC, 0),
	MP4BOX_ARG("last-dynamic", "same as [-dynamic]() but close the period (insert lmsg brand if needed and update duration)", GF_ARG_BOOL, 0, &dash_mode, GF_DASH_DYNAMIC_LAST, 0),
	MP4BOX_ARG("mpd-duration", "set the duration in second of a live session (if `0`, you must use [-mpd-refresh]())", GF_ARG_DOUBLE, 0, &mpd_live_duration, 0, 0),
	MP4BOX_ARG("mpd-refresh", "specify MPD update time in seconds", GF_ARG_DOUBLE, 0, &mpd_update_time, 0, 0),
	MP4BOX_ARG("time-shift", "specify MPD time shift buffer depth in seconds, `-1` to keep all files)", GF_ARG_INT, 0, &time_shift_depth, 0, 0),
	MP4BOX_ARG("subdur", "specify maximum duration in ms of the input file to be dashed in LIVE or context mode. This does not change the segment duration, but stops dashing once segments produced exceeded the duration. If there is not enough samples to finish a segment, data is looped unless [-no-loop]() is used which triggers a period end", GF_ARG_DOUBLE, 0, &dash_subduration, 0, 0),
	MP4BOX_ARG("run-for", "run for given ms  the dash-live session then exits", GF_ARG_INT, 0, &run_for, 0, 0),
	MP4BOX_ARG("min-buffer", "specify MPD min buffer time in ms", GF_ARG_INT, 0, &min_buffer, 0, ARG_DIV_1000),
	MP4BOX_ARG("ast-offset", "specify MPD AvailabilityStartTime offset in ms if positive, or availabilityTimeOffset of each representation if negative", GF_ARG_INT, 0, &ast_offset_ms, 0, 0),
	MP4BOX_ARG("dash-scale", "specify that timing for [-dash](),  [-dash-live](), [-subdur]() and [-do_frag]() are expressed in given timescale (units per seconds) rather than ms", GF_ARG_INT, 0, &dash_scale, 0, ARG_NON_ZERO),
	MP4BOX_ARG("mem-frags", "fragmentation happens in memory rather than on disk before flushing to disk", GF_ARG_BOOL, 0, &memory_frags, 0, 0),
	MP4BOX_ARG("pssh", "set pssh store mode\n"
	"- v: initial movie\n"
	"- f: movie fragments\n"
	"- m: MPD\n"
	"- mv, vm: in initial movie and MPD\n"
	"- mf, fm: in movie fragments and MPD\n"
	"- n: remove PSSH from MPD, initial movie and movie fragments", GF_ARG_INT, 0, parse_pssh, 0, ARG_IS_FUN),
	MP4BOX_ARG("sample-groups-traf", "store sample group descriptions in traf (duplicated for each traf). If not set, sample group descriptions are stored in the initial movie", GF_ARG_BOOL, 0, &samplegroups_in_traf, 0, 0),
	MP4BOX_ARG("mvex-after-traks", "store `mvex` box after `trak` boxes within the moov box. If not set, `mvex` is before", GF_ARG_BOOL, 0, &mvex_after_traks, 0, 0),
	MP4BOX_ARG("sdtp-traf", "use `sdtp` box in `traf` (Smooth-like)\n"
	"- no: do not use sdtp\n"
	"- sdtp: use sdtp box to indicate sample dependencies and do not write info in trun sample flags\n"
	"- both: use sdtp box to indicate sample dependencies and also write info in trun sample flags\n", GF_ARG_INT, 0, parse_sdtp, 0, ARG_IS_FUN),
	MP4BOX_ARG("no-cache", "disable file cache for dash inputs", GF_ARG_BOOL, 0, &no_cache, 0, 0),
	MP4BOX_ARG("no-loop", "disable looping content in live mode and uses period switch instead", GF_ARG_BOOL, 0, &no_loop, 0, 0),
	MP4BOX_ARG("hlsc", "insert UTC in variant playlists for live HLS", GF_ARG_BOOL, 0, &hls_clock, 0, 0),
	MP4BOX_ARG("bound", "segmentation will always try to split before or at, but never after, the segment boundary", GF_ARG_BOOL, 0, &dash_split_mode, GF_DASH_SPLIT_IN, 0),
	MP4BOX_ARG("closest", "segmentation will use the closest frame to the segment boundary (before or after)", GF_ARG_BOOL, 0, &dash_split_mode, GF_DASH_SPLIT_CLOSEST, 0),
	MP4BOX_ARG_ALT("subsegs-per-sidx", "frags-per-sidx", "set the number of subsegments to be written in each SIDX box\n"
	"- 0: a single SIDX box is used per segment\n"
	"- -1: no SIDX box is used", GF_ARG_INT, GF_ARG_HINT_EXPERT, &subsegs_per_sidx, 0, 0),
	MP4BOX_ARG("ssix", "enable SubsegmentIndexBox describing 2 ranges, first one from moof to end of first I-frame, second one unmapped. This does not work with daisy chaining mode enabled", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &use_ssix, 0, 0),
	MP4BOX_ARG("url-template", "use SegmentTemplate instead of explicit sources in segments. Ignored if segments are stored in the output file", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &use_url_template, 1, 0),
	MP4BOX_ARG("url-template-sim", "use SegmentTemplate simulation while converting HLS to MPD", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &use_url_template, 2, 0),
	MP4BOX_ARG("daisy-chain", "use daisy-chain SIDX instead of hierarchical. Ignored if frags/sidx is 0", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &daisy_chain_sidx, 0, 0),
	MP4BOX_ARG("single-segment", "use a single segment for the whole file (OnDemand profile)", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &single_segment, 0, 0),
	{"single-file", NULL, "use a single file for the whole file (default)", "yes", NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &single_file, 0, 0},
	{"bs-switching", NULL, "set bitstream switching mode\n"
	"- inband: use inband param set and a single init segment\n"
	"- merge: try to merge param sets in a single sample description, fallback to `no`\n"
	"- multi: use several sample description, one per quality\n"
	"- no: use one init segment per quality\n"
	"- pps: use out of band VPS,SPS,DCI, inband for PPS,APS and a single init segment\n"
	"- single: to test with single input", "inband", "inband|merge|multi|no|single", GF_ARG_STRING, GF_ARG_HINT_EXPERT, parse_bs_switch, 0, ARG_IS_FUN},
	MP4BOX_ARG("moof-sn", "set sequence number of first moof to given value", GF_ARG_INT, GF_ARG_HINT_EXPERT, &initial_moof_sn, 0, 0),
	MP4BOX_ARG("tfdt", "set TFDT of first traf to given value in SCALE units (cf -dash-scale)", GF_ARG_INT, GF_ARG_HINT_EXPERT, &initial_tfdt, 0, ARG_64BITS),
	MP4BOX_ARG("no-frags-default", "disable default fragments flags in trex (required by some dash-if profiles and CMAF/smooth streaming compatibility)", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &no_fragments_defaults, 0, 0),
	MP4BOX_ARG("single-traf", "use a single track fragment per moof (smooth streaming and derived specs may require this)", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &single_traf_per_moof, 0, 0),
	MP4BOX_ARG("tfdt-traf", "use a tfdt per track fragment (when -single-traf is used)", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &tfdt_per_traf, 0, 0),
	MP4BOX_ARG("dash-ts-prog", "program_number to be considered in case of an MPTS input file", GF_ARG_INT, GF_ARG_HINT_EXPERT, &program_number, 0, 0),
	MP4BOX_ARG("frag-rt", "when using fragments in live mode, flush fragments according to their timing", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &frag_real_time, 0, 0),
	MP4BOX_ARG("cp-location", "set ContentProtection element location\n"
	        "- as: sets ContentProtection in AdaptationSet element\n"
	        "- rep: sets ContentProtection in Representation element\n"
	        "- both: sets ContentProtection in both elements", GF_ARG_STRING, GF_ARG_HINT_EXPERT, parse_cp_loc, 0, ARG_IS_FUN),
	MP4BOX_ARG("start-date", "for live mode, set start date (as xs:date, e.g. YYYY-MM-DDTHH:MM:SSZ). Default is current UTC\n"
	"Warning: Do not use with multiple periods, nor when DASH duration is not a multiple of GOP size", GF_ARG_STRING, GF_ARG_HINT_EXPERT, &dash_start_date, 0, 0),
	MP4BOX_ARG("cues", "ignore dash duration and segment according to cue times in given XML file (tests/media/dash_cues for examples)", GF_ARG_STRING, GF_ARG_HINT_EXPERT, &dash_cues, 0, 0),
	MP4BOX_ARG("strict-cues", "throw error if something is wrong while parsing cues or applying cue-based segmentation", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &strict_cues, 0, 0),
	MP4BOX_ARG("merge-last-seg", "merge last segment if shorter than half the target duration", GF_ARG_BOOL, GF_ARG_HINT_EXPERT, &merge_last_seg, 0, 0),
	{0}
};

void PrintDASHUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# DASH Options\n"
		"Also see:\n"
		"- the [dasher `gpac -h dash`](dasher) filter documentation\n"
		"- [[DASH wiki|DASH-intro]].\n"
		"\n"
		"# Specifying input files\n"
		"Input media files to dash can use the following modifiers\n"
		"- #trackID=N: only use the track ID N from the source file\n"
		"- #N: only use the track ID N from the source file (mapped to [-tkid](mp4dmx))\n"
		"- #video: only use the first video track from the source file\n"
		"- #audio: only use the first audio track from the source file\n"
		"- :id=NAME: set the representation ID to NAME. Reserved value `NULL` disables representation ID for multiplexed inputs. If not set, a default value is computed and all selected tracks from the source will be in the same output multiplex.\n"
		"- :dur=VALUE: process VALUE seconds (fraction) from the media. If VALUE is longer than media duration, last sample duration is extended.\n"
		"- :period=NAME: set the representation's period to NAME. Multiple periods may be used. Periods appear in the MPD in the same order as specified with this option\n"
		"- :BaseURL=NAME: set the BaseURL. Set multiple times for multiple BaseURLs\nWarning: This does not modify generated files location (see segment template).\n"
		"- :bandwidth=VALUE: set the representation's bandwidth to a given value\n"
		"- :pdur=VALUE: sets the duration of the associated period to VALUE seconds (fraction) (alias for period_duration:VALUE). This is only used when no input media is specified (remote period insertion), e.g. `:period=X:xlink=Z:pdur=Y`\n"
		"- :ddur=VALUE: override target DASH segment duration to VALUE seconds (fraction) for this input (alias for duration:VALUE)\n"
		"- :xlink=VALUE: set the xlink value for the period containing this element. Only the xlink declared on the first rep of a period will be used\n"
		"- :asID=VALUE: set the AdaptationSet ID to VALUE (unsigned int)\n"
		"- :role=VALUE: set the role of this representation (cf DASH spec). Media with different roles belong to different adaptation sets.\n"
		"- :desc_p=VALUE: add a descriptor at the Period level.\n"
		"- :desc_as=VALUE: add a descriptor at the AdaptationSet level. Two input files with different values will be in different AdaptationSet elements.\n"
		"- :desc_as_c=VALUE: add a descriptor at the AdaptationSet level. Value is ignored while creating AdaptationSet elements.\n"
		"- :desc_rep=VALUE: add a descriptor at the Representation level. Value is ignored while creating AdaptationSet elements.\n"
		"- :sscale: force movie timescale to match media timescale of the first track in the segment.\n"
		"- :trackID=N: only use the track ID N from the source file\n"
		"- @f1[:args][@fN:args][@@fK:args]: set a filter chain to insert between the source and the dasher. Each filter in the chain is formatted as a regular filter, see [filter doc `gpac -h doc`](filters_general). If several filters are set:\n"
		"  - they will be chained in the given order if separated by a single `@`\n"
		"  - a new filter chain will be created if separated by a double `@@`. In this case, no representation ID is assigned to the source.\n"
		"EX source.mp4:@c=avc:b=1M@@c=avc:b=500k\n"
		"This will load a filter chain with two encoders connected to the source and to the dasher.\n"
		"EX source.mp4:@c=avc:b=1M@c=avc:b=500k\n"
		"This will load a filter chain with the second encoder connected to the output of the first (!!).\n"
		"\n"
		"Note: `@f` must be placed after all other options.\n"
		"\n"
		"Note: Descriptors value must be a properly formatted XML element(s), value is not checked. Syntax can use `file@FILENAME` to load content from file.\n"
		"# Options\n"
		);


	while (m4b_dash_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_dash_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-dash");
	}
}


MP4BoxArg m4b_imp_args[] =
{
	MP4BOX_ARG("add", "add given file tracks to file. Multiple inputs can be specified using `+`, e.g. `-add url1+url2`", GF_ARG_STRING, 0, &nb_add, 0, ARG_INT_INC),
	MP4BOX_ARG("cat", "concatenate given file samples to file, creating tracks if needed. Multiple inputs can be specified using `+`, e.g/ `-cat url1+url2`.  \nNote: This aligns initial timestamp of the file to be concatenated", GF_ARG_STRING, 0, &nb_cat, 0, ARG_INT_INC),
 	MP4BOX_ARG("catx", "same as [-cat]() but new tracks can be imported before concatenation by specifying `+ADD_COMMAND` where `ADD_COMMAND` is a regular [-add]() syntax", GF_ARG_STRING, 0, &nb_cat, 0, ARG_INT_INC),
 	MP4BOX_ARG("catpl", "concatenate files listed in the given playlist file (one file per line, lines starting with # are comments).  \nNote: Each listed file is concatenated as if called with -cat", GF_ARG_STRING, 0, &nb_cat, 0, ARG_INT_INC),
 	MP4BOX_ARG("unalign-cat", "do not attempt to align timestamps of samples in-between tracks", GF_ARG_BOOL, 0, &align_cat, 0, ARG_BOOL_REV),
 	MP4BOX_ARG("force-cat", "skip media configuration check when concatenating file.  \nWarning: THIS MAY BREAK THE CONCATENATED TRACK(S)", GF_ARG_BOOL, 0, &force_cat, 0, 0),
 	MP4BOX_ARG("keep-sys", "keep all MPEG-4 Systems info when using [-add]() and [-cat]() (only used when adding IsoMedia files)", GF_ARG_BOOL, 0, &keep_sys_tracks, 0, 0),
	MP4BOX_ARG("dref", "keep media data in original file using `data referencing`. The resulting file only contains the meta-data of the presentation (frame sizes, timing, etc...) and references media data in the original file. This is extremely useful when developing content, since importing and storage of the MP4 file is much faster and the resulting file much smaller.  \nNote: Data referencing may fail on some files because it requires the framed data (e.g. an IsoMedia sample) to be continuous in the original file, which is not always the case depending on the original interleaving or bitstream format (__AVC__ or __HEVC__ cannot use this option)", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_USE_DATAREF, ARG_BIT_MASK),
 	MP4BOX_ARG_ALT("no-drop", "nodrop", "force constant FPS when importing AVI video", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_NO_FRAME_DROP, ARG_BIT_MASK),
 	MP4BOX_ARG("packed", "force packed bitstream when importing raw MPEG-4 part 2 Advanced Simple Profile", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_FORCE_PACKED, ARG_BIT_MASK),
 	MP4BOX_ARG("sbr", "backward compatible signaling of AAC-SBR", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_SBR_IMPLICIT, ARG_BIT_MASK),
 	MP4BOX_ARG("sbrx", "non-backward compatible signaling of AAC-SBR", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_SBR_EXPLICIT, ARG_BIT_MASK),
 	MP4BOX_ARG("ps", "backward compatible signaling of AAC-PS", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_PS_IMPLICIT, ARG_BIT_MASK),
 	MP4BOX_ARG("psx", "non-backward compatible signaling of AAC-PS", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_PS_EXPLICIT, ARG_BIT_MASK),
 	MP4BOX_ARG("ovsbr", "oversample SBR import (SBR AAC, PS AAC and oversampled SBR cannot be detected at import time)", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_OVSBR, ARG_BIT_MASK),
 	MP4BOX_ARG("fps", "force frame rate for video and SUB subtitles import to the given value, expressed as a number, as `TS-inc` or `TS/inc`.  \nNote: For raw H263 import, default FPS is `15`, otherwise `25`. This is accepted for ISOBMFF import but `:rescale` option should be preferred", GF_ARG_STRING, 0, parse_fps, 0, ARG_IS_FUN),
 	MP4BOX_ARG("mpeg4", "force MPEG-4 sample descriptions when possible. For AAC, forces MPEG-4 AAC signaling even if MPEG-2", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_FORCE_MPEG4, ARG_BIT_MASK),
 	MP4BOX_ARG("agg", "aggregate N audio frames in 1 sample (3GP media only, maximum value is 15)", GF_ARG_INT, 0, &agg_samples, 0, 0),
	{0}
};


static MP4BoxArg m4b_imp_fileopt_args [] = {
	GF_DEF_ARG("dur", NULL, "`XC` import only the specified duration from the media. Value can be:\n"
		"  - positive float: specifies duration in seconds\n"
		"  - fraction: specifies duration as NUM/DEN fraction\n"
		"  - negative integer: specifies duration in number of coded frames", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("start", NULL, "`C` target start time in source media, may not be supported depending on the source", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("lang", NULL, "`S` set imported media language code", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("delay", NULL, "`S` set imported media initial delay (>0) or initial skip (<0) in ms or as fractional seconds (`N/D`)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("par", NULL, "`S` set visual pixel aspect ratio (see [-par](MP4B_GEN) )", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("clap", NULL, "`S` set visual clean aperture (see [-clap](MP4B_GEN) )", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("mx", NULL, "`S` set track matrix (see [-mx](MP4B_GEN) )", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("name", NULL, "`S` set track handler name", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("ext", NULL, "override file extension when importing", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("hdlr", NULL, "`S` set track handler type to the given code point (4CC)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("stype", NULL, "`S` force sample description type to given code point (4CC), may likely break the file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("tkhd", NULL, "`S` set track header flags has hex integer or as comma-separated list of `enable`, `movie`, `preview`, `size_ar` keywords (use `tkhd+=FLAGS` to add and `tkhd-=FLAGS` to remove)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("disable", NULL, "`S` disable imported track(s), use `disable=no` to force enabling a disabled track", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("group", NULL, "`S` add the track as part of the G alternate group. If G is 0, the first available GroupID will be picked", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("fps", NULL, "`S` same as [-fps]()", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("rap", NULL, "`DS` import only RAP samples", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("refs", NULL, "`DS` import only reference pictures", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("trailing", NULL, "keep trailing 0-bytes in AVC/HEVC samples", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("agg", NULL, "`X` same as [-agg]()", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("dref", NULL, "`XC` same as [-dref]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("keep_refs", NULL, "`C` keep track reference when importing a single track", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("nodrop", NULL, "same as [-nodrop]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("packed", NULL, "`X` same as [-packed]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("sbr", NULL, "same as [-sbr]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("sbrx", NULL, "same as [-sbrx]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("ovsbr", NULL, "same as [-ovsbr]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("ps", NULL, "same as [-ps]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("psx", NULL, "same as [-psx]()", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("asemode", NULL, "`XS` set the mode to create the AudioSampleEntry. Value can be:\n"
		"  - v0-bs: use MPEG AudioSampleEntry v0 and the channel count from the bitstream (even if greater than 2) - default\n"
		"  - v0-2: use MPEG AudioSampleEntry v0 and the channel count is forced to 2\n"
		"  - v1: use MPEG AudioSampleEntry v1 and the channel count from the bitstream\n"
		"  - v1-qt: use QuickTime Sound Sample Description Version 1 and the channel count from the bitstream (even if greater than 2). This will also trigger using alis data references instead of url, even for non-audio tracks", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("audio_roll", NULL, "`S` add a roll sample group with roll_distance `N` for audio tracks", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("roll", NULL, "`S` add a roll sample group with roll_distance `N`", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("proll", NULL, "`S` add a preroll sample group with roll_distance `N`", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("mpeg4", NULL, "`X` same as [-mpeg4]() option", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("nosei", NULL, "discard all SEI messages during import", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("svc", NULL, "import SVC/LHVC with explicit signaling (no AVC base compatibility)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("nosvc", NULL, "discard SVC/LHVC data when importing", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("svcmode", NULL, "`DS` set SVC/LHVC import mode. Value can be:\n"
		"  - split: each layer is in its own track\n"
		"  - merge: all layers are merged in a single track\n"
		"  - splitbase: all layers are merged in a track, and the AVC base in another\n"
		"  - splitnox: each layer is in its own track, and no extractors are written\n"
		"  - splitnoxib: each layer is in its own track, no extractors are written, using inband param set signaling", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("temporal", NULL, "`DS` set HEVC/LHVC temporal sublayer import mode. Value can be:\n"
		"  - split: each sublayer is in its own track\n"
		"  - splitbase: all sublayers are merged in a track, and the HEVC base in another\n"
		"  - splitnox: each layer is in its own track, and no extractors are written", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("subsamples", NULL, "add SubSample information for AVC+SVC", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("deps", NULL, "import sample dependency information for AVC and HEVC", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("ccst", NULL, "`S` add default HEIF ccst box to visual sample entry", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("forcesync", NULL, "force non IDR samples with I slices (OpenGOP or GDR) to be marked as sync points\n"
		"Warning: RESULTING FILE IS NOT COMPLIANT WITH THE SPEC but will fix seeking in most players", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("xps_inband", NULL, "`XC` set xPS inband for AVC/H264 and HEVC (for reverse operation, re-import from raw media)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("xps_inbandx", NULL, "`XC` same as xps_inband and also keep first xPS in sample description", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("au_delim", NULL, "keep AU delimiter NAL units in the imported file", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("max_lid", NULL, "set HEVC max layer ID to be imported to `N` (by default imports all layers)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("max_tid", NULL, "set HEVC max temporal ID to be imported to `N` (by default imports all temporal sublayers)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("tiles", NULL, "`S` add HEVC tiles signaling and NALU maps without splitting the tiles into different tile tracks", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("split_tiles", NULL, "`DS` split HEVC tiles into different tile tracks, one tile (or all tiles of one slice) per track", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("negctts", NULL, "`S` use negative CTS-DTS offsets (ISO4 brand). Use `negctts=no` to force using positive offset on existing track", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("chap", NULL, "`S` specify the track is a chapter track", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("chapter", NULL, "`S` add a single chapter (old nero format) with given name lasting the entire file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("chapfile", NULL, "`S` add a chapter file (old nero format)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("layout", NULL, "`S` specify the track layout as `WxH[xXxY][xLAYER]`. If `W` (resp `H`) is 0, the max width (resp height) of the tracks in the file are used", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("rescale", NULL, "`S` force media timescale to TS  (int or fraction) and change the media duration", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("sampdur", NULL, "`S` force all samples duration (`D`) or sample durations and media timescale (`D/TS`), used to patch CFR files with broken timings", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("timescale", NULL, "`S` set imported media timescale to TS", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("moovts", NULL, "`S` set movie timescale to TS. A negative value picks the media timescale of the first track imported", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("noedit", NULL, "`XS` do not set edit list when importing B-frames video tracks", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("rvc", NULL, "`S` set RVC configuration for the media", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("fmt", NULL, "override format detection with given format - disable data probing and force `ext` option on source", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("profile", NULL, "`S` override AVC profile. Integer value, or `high444`, `high`, `extended`, `main`, `baseline`", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("level", NULL, "`S` override AVC level, if value < 6, interpreted as decimal expression", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("compat", NULL, "`S` force the profile compatibility flags for the H.264 content", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("novpsext", NULL, "remove VPS extensions from HEVC VPS", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("keepav1t", NULL, "keep AV1 temporal delimiter OBU in samples, might help if source file had losses", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("dlba", NULL, "`S` force DolbyAtmos mode for EAC3. Value can be\n"
		"- no: disable Atmos signaling\n"
		"- auto: use Atmos signaling from first sample\n"
		"- N: force Atmos signaling using compatibility type index N", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("font", NULL, "specify font name for text import (default `Serif`)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("size", NULL, "specify font size for text import (default `18`)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("text_layout", NULL, "specify the track text layout as WxHxXxY\n"
		"  - if W (resp H) = 0: the max width (resp height) of the tracks in the file are used\n"
		"  - if Y=-1: the layout is moved to the bottom of the track area\n"
		"  - X and Y can be omitted: `:layout=WxH`", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("swf-global", NULL, "all SWF defines are placed in first scene replace rather than when needed", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-no-ctrl", NULL, "use a single stream for movie control and dictionary (this will disable ActionScript)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-no-text", NULL, "remove all SWF text", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-no-font", NULL, "remove all embedded SWF Fonts (local playback host fonts used)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-no-line", NULL, "remove all lines from SWF shapes", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-no-grad", NULL, "remove all gradients from SWF shapes", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-quad", NULL, "use quadratic bezier curves instead of cubic ones", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-xlp", NULL, "support for lines transparency and scalability", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-ic2d", NULL, "use indexed curve 2D hardcoded proto", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-same-app", NULL, "appearance nodes are reused", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("swf-flatten", NULL, "complementary angle below which 2 lines are merged, `0` means no flattening", NULL, NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("kind", NULL, "`S` set kind for the track as `schemeURI=value`", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("txtflags", NULL, "set display flags (hexa number) of text track. Use `txtflags+=FLAGS` to add flags and `txtflags-=FLAGS` to remove flags", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("rate", NULL, "force average rate and max rate to VAL (in bps) in btrt box. If 0, removes btrt box", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("stz2", NULL, "`S` use compact size table (for low-bitrates)", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("bitdepth", NULL, "set bit depth to VAL for imported video content (default is 24)", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("colr", NULL, "`S` set color profile for imported video content. Value is formatted as:\n"
		"  - nclc,p,t,m: with `p` colour primary (int or string), `t` transfer characteristics (int or string) and `m` matrix coef (int or string), cf `-h cicp`\n"
		"  - nclx,p,t,m,r: same as `nclx` with r full range flag (`yes`, `on` or `no`, `off`)\n"
		"  - prof,path: with path indicating the file containing the ICC color profile\n"
		"  - rICC,path: with path indicating the file containing the restricted ICC color profile\n"
		"  - 'none': removes color info", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("hdr", NULL, "`S` set HDR info on track (see [-hdr](MP4B_GEN) ), 'none' removes HDR info", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("dvp", "dv-profile", "`S` set the Dolby Vision profile on imported track\n"
	"- Profile is an integer, or `none` to remove DV signaling\n"
	"- Profile can be suffixed with compatibility ID, e.g. `5.hdr10`\n"
	"- Allowed compatibility ID are `none`, `hdr10`, `bt709`, `hlg709`, `hlg2100`, `bt2020`, `brd`, or integer value as per DV spec\n"
	"- Profile can be prefixed with 'f' to force DV codec type signaling, e.g. `f8.2`", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("fullrange", NULL, "`S` force the video fullrange type in VUI for the AVC|H264 content (value `yes`, `on` or `no`, `off`)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("videofmt", NULL, "`S` force the video format in VUI for AVC|H264 and HEVC content, value can be `component`, `pal`, `ntsc`, `secam`, `mac`, `undef`", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("colorprim", NULL, "`S` force the colour primaries in VUI for AVC|H264 and HEVC (int or string, cf `-h cicp`)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("colortfc", NULL, "`S` force transfer characteristics in VUI for AVC|H264 and HEVC (int or string, cf `-h cicp`)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("colormx", NULL, "`S` force the matrix coefficients in VUI for the AVC|H264 and HEVC content (int or string, cf `-h cicp`)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("tc", NULL, "`S` inject a single QT timecode. Value is formatted as:\n"
		"  - [d]FPS[/FPS_den],h,m,s,f[,framespertick]: optional drop flag, framerate (integer or fractional), hours, minutes, seconds and frame number\n"
		"  - : `d` is an optional flag used to indicate that the counter is in drop-frame format\n"
		"  - : the `framespertick` is optional and defaults to round(framerate); it indicates the number of frames per counter tick", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("edits", NULL, "`S` override edit list, same syntax as [-edits]()", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("lastsampdur", NULL, "`S` set duration of the last sample. Value is formatted as:\n"
		"  - no value: use the previous sample duration\n"
		"  - integer: indicate the duration in milliseconds\n"
		"  - N/D: indicate the duration as fractional second", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("ID", NULL, "`S` set target ID\n"
		"  - a value of 0 (default) will try to keep source track ID\n"
		"  - a value of -1 will ignore source track ID\n"
		"  - other value will try to set track ID to this value if no other track with same ID is present"
		"", NULL, NULL, GF_ARG_INT, 0),
	GF_DEF_ARG("tkgp", NULL, "`S` assign track group to track. Value is formatted as `TYPE,N` with TYPE the track group type (4CC) and N the track group ID. A negative ID removes from track group ID -N", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("tkidx", NULL, "`S` set track position in track list, 1 being first track in file", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("stats", "fstat", "`C` print filter session stats after import", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("graph", "fgraph", "`C` print filter session graph after import", NULL, NULL, GF_ARG_BOOL, 0),
	{"sopt:[OPTS]", NULL, "set `OPTS` as additional arguments to source filter. `OPTS` can be any usual filter argument, see [filter doc `gpac -h doc`](Filters)"},
	{"dopt:[OPTS]", NULL, "`X` set `OPTS` as additional arguments to [destination filter](mp4mx). OPTS can be any usual filter argument, see [filter doc `gpac -h doc`](Filters)"},
	{"@f1[:args][@fN:args]", NULL, "set a filter chain to insert before the multiplexer. Each filter in the chain is formatted as a regular filter, see [filter doc `gpac -h doc`](Filters). A `@@` separator starts a new chain (see DASH help). The last filter in each chain shall not have any ID specified"},
	{0}
};

void PrintImportUsage()
{
	u32 i;

	gf_sys_format_help(helpout, help_flags, "# Importing Options\n"
		"# File importing\n"
		"Syntax is [-add]() / [-cat]() `URL[#FRAGMENT][:opt1...:optN=val]`\n"
		"This process will create the destination file if not existing, and add the track(s) to it. If you wish to always create a new destination file, add [-new](MP4B_GEN).\n"
		"The supported input media types depend on your installation, check [filters documentation](Filters) for more info.\n"
		"  \n"
		"To select a desired media track from a source, a fragment identifier '#' can be specified, before any other options. The following syntax is used:\n"
		"- `#video`: adds the first video track found in source\n"
		"- `#audio`: adds the first audio track found in source\n"
		"- `#auxv`: adds the first auxiliary video track found in source\n"
		"- `#pict`: adds the first picture track found in source\n"
		"- `#trackID=ID` or `#ID`: adds the specified track. For IsoMedia files, ID is the track ID. For other media files, ID is the value indicated by `MP4Box -info inputFile`\n"
		"- `#pid=ID`: number of desired PID for MPEG-2 TS sources\n"
		"- `#prog_id=ID`: number of desired program for MPEG-2 TS sources\n"
		"- `#program=NAME`: name of desired program for MPEG-2 TS sources\n"
		"  \n"
		"By default all imports are performed sequentially, and final interleaving is done at the end; this however requires a temporary file holding original ISOBMF file (if any) and added files before creating the final output. Since this can become quite large, it is possible to add media to a new file without temporary storage, using [-flat](MP4B_GEN) option, but this disables media interleaving.\n"
		"  \n"
		"If you wish to create an interleaved new file with no temporary storage, use the [-newfs](MP4B_GEN) option. The interleaving might not be as precise as when using [-new]() since it is dependent on multiplexer input scheduling (each execution might lead to a slightly different result). Additionally in this mode: \n"
		" - Some multiplexing options (marked with `X` below) will be activated for all inputs (e.g. it is not possible to import one AVC track with `xps_inband` and another without).\n"
		" - Some multiplexing options (marked as `D` below) cannot be used as they require temporary storage for file edition.\n"
		" - Usage of [-cat]() is possible, but concatenated sources will not be interleaved in the output. If you wish to perform more complex cat/add operations without temp file, use a [playlist](flist).\n"
		"  \n"
		"Source URL can be any URL supported by GPAC, not limited to local files.\n"
		"  \n"
		"Note: When importing SRT or SUB files, MP4Box will choose default layout options to make the subtitle appear at the bottom of the video. You SHOULD NOT import such files before any video track is added to the destination file, otherwise the results will likely not be useful (default SRT/SUB importing uses default serif font, fontSize 18 and display size 400x60). For more details, check [TTXT doc](Subtitling-with-GPAC).\n"
		"  \n"
		"When importing several tracks/sources in one pass, all options will be applied if relevant to each source. These options are set for all imported streams. If you need to specify these options per stream, set per-file options using the syntax `-add stream[:opt1:...:optN]`.\n"
		"  \n"
		"The import file name may be set to empty or `self`, indicating that the import options should be applied to the destination file track(s).\n"
		"EX -add self:moovts=-1:noedit src.mp4\n"
		"This will apply `moovts` and `noedit` option to all tracks in src.mp4\n"
		"EX -add self#2:moovts=-1:noedit src.mp4\n"
		"This will apply `moovts` and `noedit` option to track with `ID=2` in src.mp4\n"
		"Only per-file options marked with a `S` are possible in this mode.\n"
		"  \n"
		"When importing an ISOBMFF/QT file, only options marked as `C` or `S` can be used.\n"
		"  \n"
		"Allowed per-file options:\n\n"
	);

	i=0;
	while (m4b_imp_fileopt_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_imp_fileopt_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags | GF_PRINTARG_NO_DASH, arg, "mp4box-import");
	}

	gf_sys_format_help(helpout, help_flags, "\n"
		"Note: `sopt`, `dopt` and `@f` must be placed after all other options.\n"
		"# Global import options\n"
	);

	i=0;
	while (m4b_imp_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_imp_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-import");
	}
}

Bool mp4box_check_isom_fileopt(char *opt)
{
	GF_GPACArg *arg = NULL;
	u32 i=0;

	while (m4b_imp_fileopt_args[i].name) {
		arg = (GF_GPACArg *) &m4b_imp_fileopt_args[i];
		i++;
		if (!stricmp(arg->name, opt)) break;
		arg = NULL;
	}
	if (!arg) {
		fprintf(stderr, "Option %s not described in doc, please report to GPAC devs!\n", opt);
		return GF_FALSE;
	}
	if (arg->description[0] != '`')
		return GF_FALSE;
	const char *d = arg->description+1;
	while (d[0] != '`') {
		if (d[0]=='S') return GF_TRUE;
		if (d[0]=='C') return GF_TRUE;
		d++;
	}
	return GF_FALSE;
}


MP4BoxArg m4b_senc_args[] =
{
#ifndef GPAC_DISABLE_SCENE_ENCODER
 	MP4BOX_ARG("mp4", "specify input file is for BIFS/LASeR encoding", GF_ARG_BOOL, 0, &encode, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("def", "encode DEF names in BIFS", GF_ARG_BOOL, 0, &smenc_opts.flags, GF_SM_ENCODE_USE_NAMES, ARG_BIT_MASK),
 	MP4BOX_ARG("sync", "force BIFS sync sample generation every given time in ms (cannot be used with [-shadow]() or [-carousel]() )", GF_ARG_INT, 0, parse_senc_param, 0, ARG_IS_FUN),
 	MP4BOX_ARG("shadow", "force BIFS sync shadow sample generation every given time in ms (cannot be used with [-sync]() or [-carousel]() )", GF_ARG_INT, 0, parse_senc_param, 1, ARG_IS_FUN),
 	MP4BOX_ARG("carousel", "use BIFS carousel (cannot be used with [-sync]() or [-shadow]() )", GF_ARG_INT, 0, parse_senc_param, 2, ARG_IS_FUN),

 	MP4BOX_ARG("sclog", "generate scene codec log file if available", GF_ARG_BOOL, 0, &do_scene_log, 0, 0),
 	MP4BOX_ARG("ms", "import tracks from the given file", GF_ARG_STRING, 0, &mediaSource, 0, 0),
 	MP4BOX_ARG("ctx-in", "specify initial context (MP4/BT/XMT) file for chunk processing. Input file must be a commands-only file", GF_ARG_STRING, 0, parse_senc_param, 5, ARG_IS_FUN),
 	MP4BOX_ARG("ctx-out", "specify storage of updated context (MP4/BT/XMT) file for chunk processing, optional", GF_ARG_STRING, 0, &output_ctx, 0, 0),
 	MP4BOX_ARG("resolution", "resolution factor (-8 to 7, default 0) for LASeR encoding, and all coordinates are multiplied by `2^res` before truncation (LASeR encoding)", GF_ARG_INT, 0, &smenc_opts.resolution, 0, 0),
 	MP4BOX_ARG("coord-bits", "number of bits used for encoding truncated coordinates (0 to 31, default 12) (LASeR encoding)", GF_ARG_INT, 0, &smenc_opts.coord_bits, 0, 0),
 	MP4BOX_ARG("scale-bits", "extra bits used for encoding truncated scales (0 to 4, default 0) (LASeR encoding)", GF_ARG_INT, 0, &smenc_opts.scale_bits, 0, 0),
 	MP4BOX_ARG("auto-quant", "resolution is given as if using [-resolution]() but coord-bits and scale-bits are inferred (LASeR encoding)", GF_ARG_INT, 0, parse_senc_param, 3, ARG_IS_FUN),
 	MP4BOX_ARG("global-quant", "resolution is given as if using [-resolution]() but the res is inferred (BIFS encoding)", GF_ARG_INT, 0, parse_senc_param, 4, ARG_IS_FUN),
#endif
 	{0}
};


void PrintEncodeUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# MPEG-4 Scene Encoding Options\n"
		"## General considerations\n"
		"MP4Box supports encoding and decoding of of BT, XMT, VRML and (partially) X3D formats int MPEG-4 BIFS, and encoding and decoding of XSR and SVG into MPEG-4 LASeR\n"
		"Any media track specified through a `MuxInfo` element will be imported in the resulting MP4 file.\n"
		"See https://wiki.gpac.io/MPEG-4-BIFS-Textual-Format and related pages.\n"
		"## Scene Random Access\n"
		"MP4Box can encode BIFS or LASeR streams and insert random access points at a given frequency. This is useful when packaging content for broadcast, where users will not turn in the scene at the same time. In MPEG-4 terminology, this is called the __scene carousel__."
		"## BIFS Chunk Processing\n"
		"The BIFS chunk encoding mode allows encoding single BIFS access units from an initial context and a set of commands.\n"
		"The generated AUs are raw BIFS (not SL-packetized), in files called FILE-ESID-AUIDX.bifs, with FILE the basename of the input file.\n"
		"Commands with a timing of 0 in the input will modify the carousel version only (i.e. output context).\n"
		"Commands with a timing different from 0 in the input will generate new AUs.\n"
		"  \n"
		"Options:\n"
	);

	while (m4b_senc_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_senc_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-senc");
	}
}

MP4BoxArg m4b_crypt_args[] =
{
 	MP4BOX_ARG("crypt", "encrypt the input file using the given `CryptFile`", GF_ARG_STRING, 0, parse_cryp, 0, ARG_IS_FUN),
 	MP4BOX_ARG("decrypt", "decrypt the input file, potentially using the given `CryptFile`. If `CryptFile` is not given, will fail if the key management system is not supported", GF_ARG_STRING, 0, parse_cryp, 1, ARG_IS_FUN | ARG_EMPTY),
 	MP4BOX_ARG_S("set-kms", "tkID=kms_uri", "change ISMA/OMA KMS location for a given track or for all tracks if `all=` is used", 0, parse_track_action, TRACK_ACTION_SET_KMS_URI, ARG_IS_FUN),
 	{0}
};

void PrintEncryptUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Encryption/Decryption Options\n"
	"MP4Box supports encryption and decryption of ISMA, OMA and CENC content, see [encryption filter `gpac -h cecrypt`](cecrypt).\n"
	"It requires a specific XML file called `CryptFile`, whose syntax is available at https://wiki.gpac.io/Common-Encryption\n"
	"Image files (HEIF) can also be crypted / decrypted, using CENC only.\n"
	"  \n"
	"Options:\n"
	);
	while (m4b_crypt_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_crypt_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-crypt");
	}
}

MP4BoxArg m4b_hint_args[] =
{
#ifndef GPAC_DISABLE_STREAMING
 	MP4BOX_ARG("hint", "hint the file for RTP/RTSP", GF_ARG_BOOL, 0, &do_hint, 0, ARG_OPEN_EDIT),
 	{"mtu", NULL, "specify RTP MTU (max size) in bytes (this includes 12 bytes RTP header)", "1450", NULL, GF_ARG_INT, 0, &MTUSize, 0, 0},
 	MP4BOX_ARG("copy", "copy media data to hint track rather than reference (speeds up server but takes much more space)", GF_ARG_BOOL, 0, &HintCopy, 0, 0),
 	MP4BOX_ARG_S("multi", "[maxptime]", "enable frame concatenation in RTP packets if possible (with max duration 100 ms or `maxptime` ms if given)", 0, parse_multi_rtp, 0, ARG_IS_FUN),
 	{"rate", NULL, "specify rtp rate in Hz when no default for payload", "90000", NULL, GF_ARG_INT, 0, &rtp_rate, 0, 0},
 	MP4BOX_ARG("mpeg4", "force MPEG-4 generic payload whenever possible", GF_ARG_BOOL, 0, &import_flags, GF_IMPORT_FORCE_MPEG4, ARG_BIT_MASK),
 	MP4BOX_ARG("latm", "force MPG4-LATM transport for AAC streams", GF_ARG_BOOL, 0, &hint_flags, GP_RTP_PCK_USE_LATM_AAC, ARG_BIT_MASK),
 	MP4BOX_ARG("static", "enable static RTP payload IDs whenever possible (by default, dynamic payloads are always used)", GF_ARG_BOOL, 0, &hint_flags, GP_RTP_PCK_USE_STATIC_ID, ARG_BIT_MASK),
 	MP4BOX_ARG("add-sdp", "add given SDP string to movie (`string`) or track (`tkID:string`), `tkID` being the track ID or the hint track ID", GF_ARG_STRING, 0, parse_sdp_ext, 0, ARG_IS_FUN),
 	MP4BOX_ARG("no-offset", "signal no random offset for sequence number and timestamp (support will depend on server)", GF_ARG_BOOL, 0, &hint_no_offset, 0, 0),
 	MP4BOX_ARG("unhint", "remove all hinting information from file", GF_ARG_BOOL, 0, &remove_hint, 0, ARG_OPEN_EDIT),
 	MP4BOX_ARG("group-single", "put all tracks in a single hint group", GF_ARG_BOOL, 0, &single_group, 0, 0),
 	MP4BOX_ARG("ocr", "force all MPEG-4 streams to be synchronized (MPEG-4 Systems only)", GF_ARG_BOOL, 0, &force_ocr, 0, 0),
 	MP4BOX_ARG("rap", "signal random access points in RTP packets (MPEG-4 Systems)", GF_ARG_BOOL, 0, parse_rap_ref, 0, ARG_IS_FUN | ARG_EMPTY),
 	MP4BOX_ARG("ts", "signal AU Time Stamps in RTP packets (MPEG-4 Systems)", GF_ARG_BOOL, 0, &hint_flags, GP_RTP_PCK_SIGNAL_TS, ARG_BIT_MASK),
	MP4BOX_ARG("size", "signal AU size in RTP packets (MPEG-4 Systems)", GF_ARG_BOOL, 0, &hint_flags, GP_RTP_PCK_SIGNAL_SIZE, ARG_BIT_MASK),
 	MP4BOX_ARG("idx", "signal AU sequence numbers in RTP packets (MPEG-4 Systems)", GF_ARG_BOOL, 0, &hint_flags, GP_RTP_PCK_SIGNAL_AU_IDX, ARG_BIT_MASK),
 	MP4BOX_ARG("iod", "prevent systems tracks embedding in IOD (MPEG-4 Systems), not compatible with [-isma]()", GF_ARG_BOOL, 0, &regular_iod, 0, 0),
#endif
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
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_hint_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-hint");
	}
}


MP4BoxArg m4b_extr_args[] =
{
 	MP4BOX_ARG("raw", "extract given track in raw format when supported. Use `tkID:output=FileName` to set output file name", GF_ARG_STRING, 0, parse_track_dump, GF_EXPORT_NATIVE, ARG_IS_FUN),
 	MP4BOX_ARG("raws", "extract each sample of the given track to a file. Use `tkID:N` to extract the Nth sample", GF_ARG_STRING, 0, parse_track_dump, GF_EXPORT_RAW_SAMPLES, ARG_IS_FUN),
 	MP4BOX_ARG("nhnt", "extract given track to [NHNT](nhntr) format", GF_ARG_INT, 0, parse_track_dump, GF_EXPORT_NHNT, ARG_IS_FUN),
 	MP4BOX_ARG("nhml", "extract given track to [NHML](nhmlr) format. Use `tkID:full` for full NHML dump with all packet properties", GF_ARG_STRING, 0, parse_track_dump, GF_EXPORT_NHML, ARG_IS_FUN),
 	MP4BOX_ARG("webvtt-raw", "extract given track as raw media in WebVTT as metadata. Use `tkID:embedded` to include media data in the WebVTT file", GF_ARG_STRING, 0, parse_track_dump, GF_EXPORT_WEBVTT_META, ARG_IS_FUN),
 	MP4BOX_ARG("single", "extract given track to a new mp4 file", GF_ARG_INT, 0, parse_track_dump, GF_EXPORT_MP4, ARG_IS_FUN),
 	MP4BOX_ARG("six", "extract given track as raw media in **experimental** XML streaming instructions", GF_ARG_INT, 0, parse_track_dump, GF_EXPORT_SIX, ARG_IS_FUN),
 	MP4BOX_ARG("mux", "multiplex input file to given destination", GF_ARG_STRING, 0, &mux_name, 0, 0),
 	MP4BOX_ARG("qcp", "same as [-raw]() but defaults to QCP file for EVRC/SMV", GF_ARG_INT, 0, parse_track_dump, GF_EXPORT_NATIVE | GF_EXPORT_USE_QCP, ARG_IS_FUN),
 	MP4BOX_ARG("saf", "multiplex input file to SAF multiplex", GF_ARG_BOOL, 0, &do_saf, 0, 0),
 	MP4BOX_ARG("dvbhdemux", "demultiplex DVB-H file into IP Datagrams sent on the network", GF_ARG_BOOL, 0, &dvbhdemux, 0, 0),
 	MP4BOX_ARG("raw-layer", "same as [-raw]() but skips SVC/MVC/LHVC extractors when extracting", GF_ARG_INT, 0, parse_track_dump, GF_EXPORT_NATIVE | GF_EXPORT_SVC_LAYER, ARG_IS_FUN),
 	MP4BOX_ARG("diod", "extract file IOD in raw format", GF_ARG_BOOL, 0, &dump_iod, 0, 0),
 	MP4BOX_ARG("mpd", "convert given HLS or smooth manifest (local or remote http) to MPD - options `-url-template` and `-segment-timeline`can be used in this mode.  \nNote: This only provides basic conversion, for more advanced conversions, see `gpac -h dasher`\n  \nWarning: This is not compatible with other DASH options and does not convert associated segments", GF_ARG_STRING, 0, &do_mpd_conv, 0, 0),
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
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_extr_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

MP4BoxArg m4b_dump_args[] =
{
 	MP4BOX_ARG("std", "dump/write to stdout and assume stdout is opened in binary mode", GF_ARG_BOOL, 0, &dump_std, 2, 0),
 	MP4BOX_ARG("stdb", "dump/write to stdout and try to reopen stdout in binary mode", GF_ARG_BOOL, 0, &dump_std, 1, 0),
 	MP4BOX_ARG("tracks", "print the number of tracks on stdout", GF_ARG_BOOL, 0, &get_nb_tracks, 0, 0),
 	MP4BOX_ARG("info", "print movie info (no parameter) or track extended info with specified ID", GF_ARG_STRING, 0, parse_file_info, 0, ARG_IS_FUN|ARG_EMPTY),
 	MP4BOX_ARG("infon", "print track info for given track number, 1 being the first track in the file", GF_ARG_STRING, 0, parse_file_info, 1, ARG_IS_FUN|ARG_EMPTY),
 	MP4BOX_ARG("infox", "print movie and track extended info (same as -info N` for each track)", GF_ARG_BOOL, 0, parse_file_info, 2, ARG_IS_FUN|ARG_EMPTY),
 	MP4BOX_ARG_ALT("diso", "dmp4", "dump IsoMedia file boxes in XML output", GF_ARG_BOOL, 0, &dump_isom, 1, 0),
 	MP4BOX_ARG("dxml", "dump IsoMedia file boxes and known track samples in XML output", GF_ARG_BOOL, 0, &dump_isom, 2, 0),
 	MP4BOX_ARG("disox", "dump IsoMedia file boxes except sample tables in XML output", GF_ARG_BOOL, 0, &dump_isom, 3, 0),
 	MP4BOX_ARG("keep-ods", "do not translate ISOM ODs and ESDs tags (debug purpose only)", GF_ARG_BOOL, 0, &no_odf_conf, 0, 0),
#ifndef GPAC_DISABLE_SCENE_DUMP
 	MP4BOX_ARG("bt", "dump scene to BT format", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_BT, ARG_HAS_VALUE),
 	MP4BOX_ARG("xmt", "dump scene to XMT format", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_XMTA, 0),
 	MP4BOX_ARG("wrl", "dump scene to VRML format", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_VRML, 0),
 	MP4BOX_ARG("x3d", "dump scene to X3D XML format", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_X3D_XML, 0),
 	MP4BOX_ARG("x3dv", "dump scene to X3D VRML format", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_X3D_VRML, 0),
 	MP4BOX_ARG("lsr", "dump scene to LASeR XML (XSR) format", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_LASER, 0),
 	MP4BOX_ARG("svg", "dump scene to SVG", GF_ARG_BOOL, 0, &dump_mode, GF_SM_DUMP_SVG, 0),
#endif
 	MP4BOX_ARG("drtp", "dump rtp hint samples structure to XML output", GF_ARG_BOOL, 0, &dump_rtp, 0, 0),
 	MP4BOX_ARG("dts", "print sample timing, size and position in file to text output", GF_ARG_BOOL, 0, parse_dump_ts, 0, ARG_IS_FUN),
 	MP4BOX_ARG("dtsx", "same as [-dts]() but does not print offset", GF_ARG_BOOL, 0, &dump_timestamps, 2, 0),
 	MP4BOX_ARG("dtsc", "same as [-dts]() but analyses each sample for duplicated dts/cts (__slow !__)", GF_ARG_BOOL, 0, &dump_timestamps, 3, 0),
 	MP4BOX_ARG("dtsxc", "same as [-dtsc]() but does not print offset (__slow !__)", GF_ARG_BOOL, 0, &dump_timestamps, 4, 0),
 	MP4BOX_ARG("dnal", "print NAL sample info of given track", GF_ARG_INT, 0, parse_dnal, 0, ARG_IS_FUN),
 	MP4BOX_ARG("dnalc", "print NAL sample info of given track, adding CRC for each nal", GF_ARG_INT, 0, parse_dnal, 1, ARG_IS_FUN),
 	MP4BOX_ARG("dnald", "print NAL sample info of given track without DTS and CTS info", GF_ARG_INT, 0, parse_dnal, 2, ARG_IS_FUN),
 	MP4BOX_ARG("dnalx", "print NAL sample info of given track without DTS and CTS info and adding CRC for each nal", GF_ARG_INT, 0, parse_dnal, 2|1, ARG_IS_FUN),
 	MP4BOX_ARG("sdp", "dump SDP description of hinted file", GF_ARG_BOOL, 0, &print_sdp, 0, 0),
 	MP4BOX_ARG("dsap", "dump DASH SAP cues (see -cues) for a given track", GF_ARG_INT, 0, parse_dsap, 0, ARG_IS_FUN),
 	MP4BOX_ARG("dsaps", "same as [-dsap]() but only print sample number", GF_ARG_INT, 0, parse_dsap, 1, ARG_IS_FUN),
 	MP4BOX_ARG("dsapc", "same as [-dsap]() but only print CTS", GF_ARG_INT, 0, parse_dsap, 2, ARG_IS_FUN),
 	MP4BOX_ARG("dsapd", "same as [-dsap]() but only print DTS", GF_ARG_INT, 0, parse_dsap, 3, ARG_IS_FUN),
 	MP4BOX_ARG("dsapp", "same as [-dsap]() but only print presentation time", GF_ARG_INT, 4, parse_dsap, 4, ARG_IS_FUN),
 	MP4BOX_ARG("dcr", "dump ISMACryp samples structure to XML output", GF_ARG_BOOL, 0, &dump_cr, 0, 0),
 	MP4BOX_ARG("dchunk", "dump chunk info", GF_ARG_BOOL, 0, &dump_chunk, 0, 0),
 	MP4BOX_ARG("dump-cover", "extract cover art", GF_ARG_BOOL, 0, &dump_cart, 0, 0),
 	MP4BOX_ARG("dump-chap", "extract chapter file as TTXT format", GF_ARG_BOOL, 0, &dump_chap, 1, 0),
 	MP4BOX_ARG("dump-chap-ogg", "extract chapter file as OGG format", GF_ARG_BOOL, 0, &dump_chap, 2, 0),
 	MP4BOX_ARG("dump-chap-zoom", "extract chapter file as zoom format", GF_ARG_BOOL, 0, &dump_chap, 3, 0),
 	MP4BOX_ARG_S("dump-udta", "[tkID:]4cc", "extract user data for the given 4CC. If `tkID` is given, dumps from UDTA of the given track ID, otherwise moov is used", 0, parse_dump_udta, 0, ARG_IS_FUN),
 	MP4BOX_ARG("mergevtt", "merge vtt cues while dumping", GF_ARG_BOOL, 0, &merge_vtt_cues, 0, 0),
 	MP4BOX_ARG("ttxt", "convert input subtitle to GPAC TTXT format if no parameter. Otherwise, dump given text track to GPAC TTXT format", GF_ARG_INT, 0, parse_ttxt, 0, ARG_IS_FUN),
 	MP4BOX_ARG("srt", "convert input subtitle to SRT format if no parameter. Otherwise, dump given text track to SRT format", GF_ARG_INT, 0, parse_ttxt, 1, ARG_IS_FUN),
 	MP4BOX_ARG("nstat", "generate node/field statistics for scene", GF_ARG_BOOL, 0, &stat_level, 1, 0),
 	MP4BOX_ARG("nstats", "generate node/field statistics per Access Unit", GF_ARG_BOOL, 0, &stat_level, 2, 0),
 	MP4BOX_ARG("nstatx", "generate node/field statistics for scene after each AU", GF_ARG_BOOL, 0, &stat_level, 3, 0),
 	MP4BOX_ARG("hash", "generate SHA-1 Hash of the input file", GF_ARG_BOOL, 0, &do_hash, 0, 0),
 	MP4BOX_ARG("comp", "replace with compressed version all top level box types given as parameter, formatted as `orig_4cc_1=comp_4cc_1[,orig_4cc_2=comp_4cc_2]`", GF_ARG_STRING, 0, parse_comp_box, 0, ARG_IS_FUN),
 	MP4BOX_ARG("topcount", "print to stdout the number of top-level boxes matching box types given as parameter, formatted as `4cc_1,4cc_2N`", GF_ARG_STRING, 0, parse_comp_box, 2, ARG_IS_FUN),
 	MP4BOX_ARG("topsize", "print to stdout the number of bytes of top-level boxes matching types given as parameter, formatted as `4cc_1,4cc_2N` or `all` for all boxes", GF_ARG_STRING, 0, parse_comp_box, 1, ARG_IS_FUN),
 	MP4BOX_ARG("bin", "convert input XML file using NHML bitstream syntax to binary", GF_ARG_BOOL, 0, &do_bin_xml, 0, 0),
 	MP4BOX_ARG("mpd-rip", "fetch MPD and segment to disk", GF_ARG_BOOL, 0, &do_mpd_rip, 0, 0),
 	//MP4BOX_ARG_S("udp-write", "IP[:port]", "write input name to UDP (default port 2345)", GF_FS_ARG_HINT_EXPERT, &udp_dest, 0, GF_ARG_STRING),
	{"udp-write", NULL, "write input name to UDP (default port 2345)", "IP[:port]", NULL, GF_ARG_STRING, GF_FS_ARG_HINT_EXPERT, &udp_dest, 0, 0},
 	MP4BOX_ARG("raw-cat", "raw concatenation of given file with input file", GF_ARG_STRING, GF_FS_ARG_HINT_EXPERT, &raw_cat, 0, 0),
 	MP4BOX_ARG("wget", "fetch resource from http(s) URL", GF_ARG_STRING, GF_FS_ARG_HINT_EXPERT, &do_wget, 0, 0),
 	MP4BOX_ARG("dm2ts", "dump timing of an input MPEG-2 TS stream sample timing", GF_ARG_BOOL, 0, &dump_m2ts, 0, 0),
 	MP4BOX_ARG("check-xml", "check XML output format for -dnal*, -diso* and -dxml options", GF_ARG_BOOL, 0, &dump_check_xml, 0, 0),
 	MP4BOX_ARG("fuzz-chk", "open file without probing and exit (for fuzz tests)", GF_ARG_BOOL, GF_FS_ARG_HINT_EXPERT, &fuzz_chk, 0, 0),
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
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_dump_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

MP4BoxArg m4b_meta_args[] =
{
 	MP4BOX_ARG_S("set-meta", "ABCD[:tk=tkID]", "set meta box type, with `ABCD` the four char meta type (NULL or 0 to remove meta)\n"
		"- tk not set: use root (file) meta\n"
		"- tkID == 0: use moov meta\n"
		"- tkID != 0: use meta of given track", 0, parse_meta_args, META_ACTION_SET_TYPE, ARG_IS_FUN),
 	MP4BOX_ARG("add-item", "add resource to meta, with parameter syntax `file_path[:opt1:optN]`\n"
		"- file_path `this` or `self`: item is the file itself\n"
		"- tk=tkID: meta location (file, moov, track)\n"
		"- name=str: item name, none if not set\n"
		"- type=itype: item 4cc type (not needed if mime is provided)\n"
		"- mime=mtype: item mime type, none if not set\n"
		"- encoding=enctype: item content-encoding type, none if not set\n"
		"- id=ID: item ID\n"
		"- ref=4cc,id: reference of type 4cc to an other item (can be set multiple times)\n"
		"- group=id,type: indicate the id and type of an alternate group for this item\n"
		"- replace: replace existing item by new item"
		, GF_ARG_STRING, 0, parse_meta_args, META_ACTION_ADD_ITEM, ARG_IS_FUN),
	MP4BOX_ARG("add-image", "add the given file as HEIF image item, with parameter syntax `file_path[:opt1:optN]`. If `filepath` is omitted, source is the input MP4 file\n"
		"- name, id, ref: see [-add-item]()\n"
		"- primary: indicate that this item should be the primary item\n"
		"- time=t[-e][/i]: use the next sync sample after time t (float, in sec, default 0). A negative time imports ALL intra frames as items\n"
		" - If `e` is set (float, in sec), import all sync samples between `t` and `e`\n"
		" - If `i` is set (float, in sec), sets time increment between samples to import\n"
		"- split_tiles: for an HEVC tiled image, each tile is stored as a separate item\n"
		"- image-size=wxh: force setting the image size and ignoring the bitstream info, used for grid, overlay and identity derived images also\n"
		"- rotation=a: set the rotation angle for this image to 90*a degrees anti-clockwise\n"
		"- mirror-axis=axis: set the mirror axis: vertical, horizontal\n"
		"- clap=Wn,Wd,Hn,Hd,HOn,HOd,VOn,VOd: see track clap\n"
 		"- image-pasp=axb: force the aspect ratio of the image\n"
 		"- image-pixi=(a|a,b,c): force the bit depth (1 or 3 channels)\n"
		"- hidden: indicate that this image item should be hidden\n"
		"- icc_path: path to icc data to add as color info\n"
		"- alpha: indicate that the image is an alpha image (should use ref=auxl also)\n"
		"- depth: indicate that the image is a depth image (should use ref=auxl also)\n"
		"- it=ID: indicate the item ID of the source item to import\n"
		"- itp=ID: same as `it=` but copy over all properties of the source item\n"
		"- tk=tkID: indicate the track ID of the source sample. If 0, uses the first video track in the file\n"
		"- samp=N: indicate the sample number of the source sample\n"
		"- ref: do not copy the data but refer to the final sample/item location, ignored if `filepath` is set\n"
		"- agrid[=AR]: creates an automatic grid from the image items present in the file, in their declaration order. The grid will **try to** have `AR` aspect ratio if specified (float), or the aspect ratio of the source otherwise. The grid will be the primary item and all other images will be hidden\n"
		"- av1_op_index: select the AV1 operating point to use via a1op box\n"
		"- replace: replace existing image by new image, keeping props listed in `keep_props`\n"
		"- keep_props=4CCs: coma-separated list of properties types to keep when replacing the image, e.g. `keep_props=auxC`\n"
		"- auxt=URN: mark image as auxiliary using given `URN`\n"
		"- auxd=FILE: use data from `FILE` as auxiliary extensions (cf `auxC` box)\n"
		"- any other options will be passed as options to the media importer, see [-add]()"
		, GF_ARG_STRING, 0, parse_meta_args, META_ACTION_ADD_IMAGE_ITEM, ARG_IS_FUN),
	MP4BOX_ARG("add-derived-image", "create an image grid, overlay or identity item, with parameter syntax `:type=(grid|iovl|iden)[:opt1:optN]`\n"
		"- image-grid-size=rxc: set the number of rows and columns of the grid\n"
 		"- image-overlay-offsets=h,v[,h,v]*: set the horizontal and vertical offsets of the images in the overlay\n"
 		"- image-overlay-color=r,g,b,a: set the canvas color of the overlay [0-65535]\n"
	    "- any other options from [-add-image]() can be used\n", GF_ARG_STRING, 0, parse_meta_args, META_ACTION_ADD_IMAGE_DERIVED, ARG_IS_FUN),
	MP4BOX_ARG_S_ALT("rem-item", "rem-image", "item_ID[:tk=tkID]", "remove resource from meta", 0, parse_meta_args, META_ACTION_REM_ITEM, ARG_IS_FUN),
	MP4BOX_ARG_S("set-primary", "item_ID[:tk=tkID]", "set item as primary for meta", 0, parse_meta_args, META_ACTION_SET_PRIMARY_ITEM, ARG_IS_FUN),
	MP4BOX_ARG_S("set-xml", "xml_file_path[:tk=tkID][:binary]", "set meta XML data", 0, parse_meta_args, META_ACTION_SET_XML, ARG_IS_FUN),
	MP4BOX_ARG_S("rem-xml", "[tk=tkID]", "remove meta XML data", 0, parse_meta_args, META_ACTION_REM_XML, ARG_IS_FUN),
	MP4BOX_ARG_S("dump-xml", "file_path[:tk=tkID]", "dump meta XML to file", 0, parse_meta_args, META_ACTION_DUMP_XML, ARG_IS_FUN),
	MP4BOX_ARG_S("dump-item", "item_ID[:tk=tkID][:path=fileName]", "dump item to file", 0, parse_meta_args, META_ACTION_DUMP_ITEM, ARG_IS_FUN),
	MP4BOX_ARG("package", "package input XML file into an ISO container, all media referenced except hyperlinks are added to file", GF_ARG_STRING, 0, &pack_file, 0, 0),
	MP4BOX_ARG("mgt", "package input XML file into an MPEG-U widget with ISO container, all files contained in the current folder are added to the widget package", GF_ARG_STRING, 0, parse_mpegu, 0, ARG_IS_FUN),
	{0}
};

void PrintMetaUsage()
{
	u32 i=0;
	gf_sys_format_help(helpout, help_flags, "# Meta and HEIF Options\n"
	"IsoMedia files can be used as generic meta-data containers, for examples storing XML information and sample images for a movie. The resulting file may not always contain a movie as is the case with some HEIF files or MPEG-21 files.\n"
	"  \n"
	"These information can be stored at the file root level, as is the case for HEIF/IFF and MPEG-21 file formats, or at the movie or track level for a regular movie."
	"  \n  \n");
	while (m4b_meta_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_meta_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

MP4BoxArg m4b_swf_args[] =
{
#ifndef GPAC_DISABLE_SWF_IMPORT
 	MP4BOX_ARG("global", "all SWF defines are placed in first scene replace rather than when needed", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_STATIC_DICT, ARG_BIT_MASK),
 	MP4BOX_ARG("no-ctrl", "use a single stream for movie control and dictionary (this will disable ActionScript)", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_SPLIT_TIMELINE, ARG_BIT_MASK_REM),
 	MP4BOX_ARG("no-text", "remove all SWF text", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_NO_TEXT, ARG_BIT_MASK),
 	MP4BOX_ARG("no-font", "remove all embedded SWF Fonts (local playback host fonts used)", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_NO_FONT, ARG_BIT_MASK),
 	MP4BOX_ARG("no-line", "remove all lines from SWF shapes", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_NO_LINE, ARG_BIT_MASK),
 	MP4BOX_ARG("no-grad", "remove all gradients from swf shapes", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_NO_GRADIENT, ARG_BIT_MASK),
 	MP4BOX_ARG("quad", "use quadratic bezier curves instead of cubic ones", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_QUAD_CURVE, ARG_BIT_MASK),
 	MP4BOX_ARG("xlp", "support for lines transparency and scalability", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_SCALABLE_LINE, ARG_BIT_MASK),
	MP4BOX_ARG("ic2d", "use indexed curve 2D hardcoded proto", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_USE_IC2D, ARG_BIT_MASK),
	MP4BOX_ARG("same-app", "appearance nodes are reused", GF_ARG_BOOL, 0, &swf_flags, GF_SM_SWF_REUSE_APPEARANCE, ARG_BIT_MASK),
 	MP4BOX_ARG("flatten", "complementary angle below which 2 lines are merged, value `0` means no flattening", GF_ARG_DOUBLE, 0, &swf_flatten_angle, 0, 0),
#endif
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
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_swf_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-extract");
	}
}

MP4BoxArg m4b_liveenc_args[] =
{
 	MP4BOX_ARG("live", "enable live BIFS/LASeR encoder", GF_ARG_BOOL, 0, &live_scene, 0, 0),
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
	        "The options shall be specified as `opt_name=opt_val.\n"
	        "Options:\n"
	        "\n"
	);
	while (m4b_liveenc_args[i].name) {
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_liveenc_args[i];
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
	gf_sys_print_core_help(helpout, 0, GF_ARGMODE_ALL, 0);
}

void PrintTags()
{
	u32 i = 0;

	gf_sys_format_help(helpout, help_flags, "# Tagging support\n"
	"Tags are specified as a colon-separated list `tag_name=tag_value[:tag2=val2]`\n"
	"Setting a tag with no value or value `NULL` removes the tag.\n"
	"Special tag value `clear` (or `reset`) removes all tags.\n"
	"Unsupported tags can be added using their four character code as a tag name, and string value will be assumed.\n"
	"If the tag name length is 3, the prefix 0xA9 is used to create the four character code.\n"
	"  \n"
	"Tags can also be loaded from a text file using `-itags filename`. The file must be in UTF8 with:\n"
	"- lines starting with `tag_name=value` specify the start of a tag\n"
	"- other lines specify the remainder of the last declared tag\n"
	"  \n"
	"If tag name starts with `WM/`, the tag is added to `Xtra` box (WMA tag, string only).\n"
	"  \n"
	"## QT metadata key\n"
	"The tag is added as a QT metadata key if:\n"
	"- `tag_name` starts with `QT/`\n"
	"- or `tag_name` is not recognized and longer than 4 characters\n"
	"  \n"
	"The `tag_name` can optionally be prefixed with `HDLR@`, indicating the tag namespace 4CC, the default namespace being `mdta`.\n"
	"The `tag_value` can be prefixed with:\n"
	"- S: force string encoding (must be placed first) instead of parsing the tag value\n"
	"- b: use 8-bit encoding for signed or unsigned int\n"
	"- s: use 16-bit encoding for signed or unsigned int\n"
	"- l: use 32-bit encoding for signed or unsigned int\n"
	"- L: use 64-bit encoding for signed or unsigned int\n"
	"- f: force float encoding for numbers\n"
	"Numbers are converted by default and stored in variable-size mode.\n"
	"To force a positive integer to use signed storage, add `+` in front of the number.\n"
	"EX -tags io.gpac.some_tag=s+32\n"
	"This will force storing value `32` in signed 16 bit format.\n"
	"The `tag_value` can also be formatted as:\n"
	"- XxY@WxH: a rectangle type\n"
	"- XxY: a point type\n"
	"- W@H: a size type\n"
	"- A,B,C,D,E,F,G,H,I: a 3x3 matrix\n"
	"- FNAME: data is loaded from `FNAME`, type set to jpeg or png if needed\n"
	"  \n"
	"## Supported tag names (name, value, type, aliases)\n"
	);

	while (1) {
		s32 type = gf_itags_get_type(i);
		if (type<0) break;
		const char *name = gf_itags_get_name(i);
		u32 itag = gf_itags_get_itag(i);
		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST , "%s", name);
		gf_sys_format_help(helpout, help_flags, " (%s) ", gf_4cc_to_str(itag) );
		switch (type) {
		case GF_ITAG_STR:
			gf_sys_format_help(helpout, help_flags, "string"); break;
		case GF_ITAG_INT8:
		case GF_ITAG_INT16:
		case GF_ITAG_INT32:
		case GF_ITAG_INT64:
			gf_sys_format_help(helpout, help_flags, "integer"); break;
		case GF_ITAG_FRAC6:
		case GF_ITAG_FRAC8:
			gf_sys_format_help(helpout, help_flags, "fraction (syntax: `A/B` or `A`, B will be 0)"); break;
		case GF_ITAG_BOOL:
			gf_sys_format_help(helpout, help_flags, "bool (`yes` or `no`)"); break;
		case GF_ITAG_ID3_GENRE:
			gf_sys_format_help(helpout, help_flags, "string (ID3 genre tag)"); break;
		case GF_ITAG_FILE:
			gf_sys_format_help(helpout, help_flags, "file path"); break;
		}
		name = gf_itags_get_alt_name(i);
		if (name) {
			gf_sys_format_help(helpout, help_flags, " (`alias` %s)", name);
		}

		gf_sys_format_help(helpout, help_flags, "\n");
		i++;
	}
}

void PrintCICP()
{
	u32 i;
	gf_sys_format_help(helpout, help_flags, "# Video CICP (ISO/IEC 23091-2) Constants\n");
	gf_sys_format_help(helpout, help_flags, "CICP Color Primaries:\n");
	for (i=0; i<GF_CICP_PRIM_LAST; i++) {
		const char *name = gf_cicp_color_primaries_name(i);
		if (!name || !strcmp(name, "unknown")) continue;
		gf_sys_format_help(helpout, help_flags, " - `%s` (value %d)\n", name, i);
	}
	gf_sys_format_help(helpout, help_flags, "  \nCICP Color Transfer Characteristics:\n");
	for (i=0; i<GF_CICP_TRANSFER_LAST; i++) {
		const char *name = gf_cicp_color_transfer_name(i);
		if (!name) continue;
		gf_sys_format_help(helpout, help_flags, " - `%s` (value %d)\n", name, i);
	}
	gf_sys_format_help(helpout, help_flags, "  \nCICP Color Matrix Coefficients:\n");
	for (i=0; i<GF_CICP_MX_LAST; i++) {
		const char *name = gf_cicp_color_matrix_name(i);
		if (!name) continue;
		gf_sys_format_help(helpout, help_flags, " - `%s` (value %d)\n", name, i);
	}
}

MP4BoxArg m4b_usage_args[] =
{
 	MP4BOX_ARG("h", "print help\n"
 		"- general: general options help\n"
		"- hint: hinting options help\n"
		"- dash: DASH segmenter help\n"
		"- split: split options help\n"
		"- import: import options help\n"
		"- encode: scene description encoding options help\n"
		"- meta: meta (HEIF, MPEG-21) handling options help\n"
		"- extract: extraction options help\n"
		"- dump: dump options help\n"
		"- swf: Flash (SWF) options help\n"
		"- crypt: ISMA E&A options help\n"
		"- format: supported formats help\n"
		"- live: BIFS streamer help\n"
		"- core: libgpac core options\n"
		"- all: print all the above help screens\n"
		"- opts: print all options\n"
		"- tags: print supported iTunes tags\n"
		"- cicp: print various CICP code points\n"
		"- VAL: search for option named `VAL` (without `-` or `--`) in MP4Box, libgpac core and all filters\n"
		, GF_ARG_STRING, 0, parse_help, 0, ARG_IS_FUN | ARG_EMPTY | ARG_PUSH_SYSARGS),
 	MP4BOX_ARG("hx", "look for given string in name and descriptions of all MP4Box and filters options"
		, GF_ARG_STRING, 0, parse_help, 1, ARG_IS_FUN),
 	MP4BOX_ARG("nodes", "list supported MPEG4 nodes", GF_ARG_BOOL, 0, PrintBuiltInNodes, 0, ARG_IS_FUN),
 	MP4BOX_ARG("nodex", "list supported MPEG4 nodes and print nodes", GF_ARG_BOOL, 0, PrintBuiltInNodes, 1, ARG_IS_FUN),
 	MP4BOX_ARG("node", "get given MPEG4 node syntax and QP infolist", GF_ARG_STRING, 0, PrintNode, 0, ARG_IS_FUN),
 	MP4BOX_ARG("xnodes", "list supported X3D nodes", GF_ARG_BOOL, 0, PrintBuiltInNodes, 2, ARG_IS_FUN),
 	MP4BOX_ARG("xnodex", "list supported X3D nodes and print nodes", GF_ARG_BOOL, 0, PrintBuiltInNodes, 3, ARG_IS_FUN),
 	MP4BOX_ARG("xnode", "get given X3D node syntax", GF_ARG_STRING, 0, PrintNode, 1, ARG_IS_FUN),
 	MP4BOX_ARG("snodes", "list supported SVG nodes", GF_ARG_BOOL, 0, PrintBuiltInNodes, 4, ARG_IS_FUN),
 	MP4BOX_ARG("languages", "list supported ISO 639 languages", GF_ARG_BOOL, 0, PrintLanguages, 0, ARG_IS_FUN),
 	MP4BOX_ARG("boxes", "list all supported ISOBMF boxes and their syntax", GF_ARG_BOOL, 0, PrintBuiltInBoxes, 0, ARG_IS_FUN),
 	MP4BOX_ARG("boxcov", "perform coverage of box IO coode", GF_ARG_BOOL, GF_ARG_HINT_HIDE, PrintBuiltInBoxes, 1, ARG_IS_FUN|ARG_PUSH_SYSARGS),
 	MP4BOX_ARG_ALT("stats", "fstat", "print filter session statistics (import/export/encrypt/decrypt/dashing)", GF_ARG_BOOL, 0, &fs_dump_flags, 1, ARG_BIT_MASK),
 	MP4BOX_ARG_ALT("graph", "fgraph", "print filter session graph (import/export/encrypt/decrypt/dashing)", GF_ARG_BOOL, 0, &fs_dump_flags, 2, ARG_BIT_MASK),
 	MP4BOX_ARG("v", "verbose mode", GF_ARG_BOOL, 0, &verbose, 0, ARG_INT_INC),
 	MP4BOX_ARG("version", "get build version", GF_ARG_BOOL, 0, print_version, 0, ARG_IS_FUN),
 	MP4BOX_ARG("genmd", "generate MD doc", GF_ARG_BOOL, GF_ARG_HINT_HIDE, parse_gendoc, 0, ARG_IS_FUN),
 	MP4BOX_ARG("genman", "generate man doc", GF_ARG_BOOL, GF_ARG_HINT_HIDE, parse_gendoc, 1, ARG_IS_FUN),
 	MP4BOX_ARG_S("-", "INPUT", "escape option if INPUT starts with `-` character", 0, NULL, 0, 0),
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
		GF_GPACArg *arg = (GF_GPACArg *) &m4b_usage_args[i];
		i++;
		gf_sys_print_arg(helpout, help_flags, arg, "mp4box-general");
	}
	gf_sys_format_help(helpout, help_flags, "\nReturn codes are 0 for no error, 1 for error"
#ifdef GPAC_MEMORY_TRACKING
		" and 2 for memory leak detection when -mem-track is used"
#endif
		"\n");
}

/*
 * 		END OF ARGS PARSING AND HELP
 */


enum
{
	SEARCH_ARG_EXACT,
	SEARCH_ARG_CLOSE,
	SEARCH_DESC,
};

static u32 PrintHelpForArgs(char *arg_name, MP4BoxArg *args, GF_GPACArg *_args, u32 search_type, char *class_name)
{
	char szDesc[100];
	u32 res=0;
	u32 i=0;
	u32 alen = (u32) strlen(arg_name);

	sprintf(szDesc, "see MP4Box -h %s", class_name);

	while (1) {
		u32 flags=0;
		GF_GPACArg *arg;
		GF_GPACArg an_arg;
		Bool do_match = GF_FALSE;
		if (args) {
			if (!args[i].name)
				break;
			arg = (GF_GPACArg *) &args[i];
		} else {
			if (!_args[i].name)
				break;
			arg = &_args[i];
		}

		if (args == m4b_imp_fileopt_args) {
			flags = GF_PRINTARG_COLON;
			if (!strncmp(arg_name, arg->name, alen) && ((arg->name[alen]==0) || (arg->name[alen]=='=')))
				do_match = GF_TRUE;
		}
		else if (!strcmp(arg_name, arg->name))
			do_match = GF_TRUE;
		else if ((alen < (u32) strlen(arg->name)) && (arg->name[alen]==' ') && !strncmp(arg_name, arg->name, alen))
			do_match = GF_TRUE;

		if (arg_name[0] == '@')
			do_match = GF_TRUE;

		if ((search_type==SEARCH_ARG_EXACT) && !do_match) {
			i++;
			continue;
		}
		if ((search_type==SEARCH_ARG_CLOSE) && !gf_sys_word_match(arg_name, arg->name)) {
			i++;
			continue;
		}
		if ((search_type==SEARCH_DESC) && strcmp(arg->name, arg_name) && !gf_strnistr(arg->description, arg_name, alen)) {
			i++;
			continue;
		}

		an_arg = *arg;
		if (search_type!=SEARCH_ARG_EXACT) {
			if (search_type == SEARCH_DESC)
				an_arg.description = szDesc;
			else
				an_arg.description = NULL;
			an_arg.type = GF_ARG_BOOL;
			gf_sys_print_arg(helpout, flags, (GF_GPACArg *) &an_arg, "");

		} else {
			gf_sys_print_arg(helpout, flags, (GF_GPACArg *) &an_arg, "");
		}
		res++;
		i++;
	}
	return res;
}
static Bool PrintHelpArg(char *arg_name, u32 search_type, GF_FilterSession *fs)
{
	char szDesc[100];
	Bool first=GF_TRUE;
	GF_GPACArg an_arg;
	u32 i, count;
	u32 res = 0;
	u32 alen = (u32) strlen(arg_name);
	res += PrintHelpForArgs(arg_name, m4b_gen_args, NULL, search_type, "general");
	res += PrintHelpForArgs(arg_name, m4b_split_args, NULL, search_type, "split");
	res += PrintHelpForArgs(arg_name, m4b_dash_args, NULL, search_type, "dash");
	res += PrintHelpForArgs(arg_name, m4b_imp_args, NULL, search_type, "import");
	res += PrintHelpForArgs(arg_name, m4b_imp_fileopt_args, NULL, search_type, "import (per-file option)");
	res += PrintHelpForArgs(arg_name, m4b_senc_args, NULL, search_type, "encode");
	res += PrintHelpForArgs(arg_name, m4b_crypt_args, NULL, search_type, "crypt");
	res += PrintHelpForArgs(arg_name, m4b_hint_args, NULL, search_type, "hint");
	res += PrintHelpForArgs(arg_name, m4b_extr_args, NULL, search_type, "extract");
	res += PrintHelpForArgs(arg_name, m4b_dump_args, NULL, search_type, "dump");
	res += PrintHelpForArgs(arg_name, m4b_meta_args, NULL, search_type, "meta");
	res += PrintHelpForArgs(arg_name, m4b_swf_args, NULL, search_type, "swf");
	res += PrintHelpForArgs(arg_name, m4b_liveenc_args, NULL, search_type, "live");
	res += PrintHelpForArgs(arg_name, m4b_usage_args, NULL, search_type, "");
	res += PrintHelpForArgs(arg_name, NULL, (GF_GPACArg *) gf_sys_get_options(), search_type, "core");

	if (!fs) return res;

	memset(&an_arg, 0, sizeof(GF_GPACArg));
	count = gf_fs_filters_registers_count(fs);
	for (i=0; i<count; i++) {
		u32 j=0;
		const GF_FilterRegister *reg = gf_fs_get_filter_register(fs, i);

		while (reg->args) {
			u32 len;
			const GF_FilterArgs *arg = &reg->args[j];
			if (!arg || !arg->arg_name) break;
			j++;
			if ((search_type==SEARCH_ARG_EXACT) && strcmp(arg->arg_name, arg_name)) continue;

			if ((search_type==SEARCH_ARG_CLOSE) && !gf_sys_word_match(arg->arg_name, arg_name)) continue;

			if (search_type==SEARCH_DESC) {
				if (stricmp(arg->arg_name, arg_name) && !gf_strnistr(arg->arg_desc, arg_name, alen)) continue;
			}

			an_arg.name = arg->arg_name;
			if (search_type==SEARCH_ARG_EXACT) {
				an_arg.description = arg->arg_desc;
				switch (arg->arg_type) {
				case GF_PROP_BOOL:
					an_arg.type = GF_ARG_BOOL;
					break;
				case GF_PROP_UINT:
				case GF_PROP_SINT:
					an_arg.type = GF_ARG_INT;
					break;
				case GF_PROP_DOUBLE:
					an_arg.type = GF_ARG_DOUBLE;
					break;
				case GF_PROP_STRING_LIST:
				case GF_PROP_UINT_LIST:
				case GF_PROP_SINT_LIST:
				case GF_PROP_VEC2I_LIST:
					an_arg.type = GF_ARG_STRINGS;
					break;
				case GF_PROP_4CC:
					an_arg.type = GF_ARG_4CC;
					break;
				case GF_PROP_4CC_LIST:
					an_arg.type = GF_ARG_4CCS;
					break;
				default:
					an_arg.type = GF_ARG_STRING;
					break;
				}
				if (first) {
					first = GF_FALSE;
					gf_sys_format_help(helpout, 0, "\nGlobal filter session arguments matching %s:\n", arg_name);
					//. Syntax is `--arg` or `--arg=VAL`. `[F]` indicates filter name. See `gpac -h` and `gpac -h F` for more info.\n");
				}
				fprintf(helpout, "[%s]", reg->name);
				len = (u32)strlen(reg->name);
				while (len<10) {
					len++;
					fprintf(helpout, " ");
				}
				fprintf(helpout, " ");
			} else if (search_type==SEARCH_DESC) {
				sprintf(szDesc, "see filter %s", reg->name);
				an_arg.description = szDesc;
			}

			gf_sys_print_arg(helpout, GF_PRINTARG_ADD_DASH, &an_arg, "TEST");
			res++;
		}
	}
	if (res) return GF_TRUE;
	return GF_FALSE;
}

static void PrintHelp(char *arg_name, Bool search_desc, Bool no_match)
{
	GF_FilterSession *fs;
	Bool res;

	fs = gf_fs_new_defaults(0);

	if (arg_name[0]=='-')
		arg_name++;

	if (search_desc) {
		char *_arg_name = gf_strdup(arg_name);
		strlwr(_arg_name);
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Possible options mentioning `%s`:\n", arg_name));
		PrintHelpArg(_arg_name, SEARCH_DESC, fs);
		gf_free(_arg_name);
	} else {
		res = no_match ? GF_FALSE : PrintHelpArg(arg_name, SEARCH_ARG_EXACT, fs);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Option -%s unknown, please check usage.\n", arg_name));
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Possible options are:\n"));

			PrintHelpArg(arg_name, SEARCH_ARG_CLOSE, fs);
		}
	}
	if (fs)
		gf_fs_del(fs);
}


u32 parse_sdp_ext(char *arg_val, u32 param)
{
	char *id;
	sdp_lines = gf_realloc(sdp_lines, sizeof(SDPLine) * (nb_sdp_ex + 1));
	if (!sdp_lines) return 2;
	id = strchr(arg_val, ':');
	if (id) {
		id[0] = 0;
		if (parse_track_id(&sdp_lines[nb_sdp_ex].track_id, arg_val, GF_FALSE)) {
			id[0] = ':';
			sdp_lines[nb_sdp_ex].line = id + 1;
		}
		else {
			id[0] = ':';
			sdp_lines[nb_sdp_ex].line = arg_val;
		}
	}
	else {
		sdp_lines[nb_sdp_ex].line = arg_val;
		sdp_lines[nb_sdp_ex].track_id.type = 0;
		sdp_lines[nb_sdp_ex].track_id.ID_or_num = 0;
	}
	open_edit = GF_TRUE;
	nb_sdp_ex++;
	return GF_FALSE;
}


static u32 parse_meta_args(char *opts, MetaActionType act_type)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	MetaAction *meta;

	metas = gf_realloc(metas, sizeof(MetaAction) * (nb_meta_act + 1));
	if (!metas) return 2;
	meta = &metas[nb_meta_act];
	nb_meta_act ++;

	memset(meta, 0, sizeof(MetaAction));
	meta->act_type = act_type;
	meta->root_meta = 1;
	open_edit = GF_TRUE;

	if (!opts) return 2;

	if (act_type == META_ACTION_ADD_IMAGE_ITEM)
		has_add_image = GF_TRUE;

#define CHECK_IMGPROP\
			if (!meta->image_props) {\
				GF_SAFEALLOC(meta->image_props, GF_ImageItemProperties);\
				if (!meta->image_props) return 2;\
			}\

	while (1) {
		char *next;
		char *szSlot;
		if (!opts || !opts[0]) return 0;
		if (opts[0]==':') opts += 1;

		szSlot = opts;
		next = gf_url_colon_suffix(opts, '=');
		if (next) next[0] = 0;
		if (next && !strncmp(szSlot, "auxt", 4)) {
			next[0] = ':';
			char *sep = strchr(next, '=');
			if (sep) {
				next = sep;
				while (next[0] != ':')
					next--;

				next[0] = 0;
			}
		}

		if (!strnicmp(szSlot, "tk=", 3)) {
			parse_track_id(&meta->track_id, szSlot+3, GF_FALSE);
			if (act_type == META_ACTION_ADD_ITEM)
				meta->root_meta = 0;
		}
		else if (!strnicmp(szSlot, "id=", 3)) {
			meta->item_id = parse_u32(szSlot+3, "id");
		}
		else if (!strnicmp(szSlot, "type=", 5)) {
			meta->item_type = GF_4CC(szSlot[5], szSlot[6], szSlot[7], szSlot[8]);
		}
		//"ref" (without '=') is for data reference, "ref=" is for item references
		else if (!strnicmp(szSlot, "ref=", 4)) {
			char type[5];
			MetaRef	*ref;
			if (!meta->item_refs) {
				meta->item_refs = gf_list_new();
				if (!meta->item_refs) return 2;
			}
			GF_SAFEALLOC(ref, MetaRef);
			if (!ref) return 2;
			sscanf(szSlot, "ref=%4s,%u", type, &(ref->ref_item_id));
			ref->ref_type = GF_4CC(type[0], type[1], type[2], type[3]);
			gf_list_add(meta->item_refs, ref);
		}
		else if (!strnicmp(szSlot, "name=", 5)) {
			meta->szName = gf_strdup(szSlot+5);
		}
		else if (!strnicmp(szSlot, "path=", 5)) {
			meta->szPath = gf_strdup(szSlot+5);
		}
		else if (!strnicmp(szSlot, "mime=", 5)) {
			meta->item_type = GF_META_ITEM_TYPE_MIME;
			meta->mime_type = gf_strdup(szSlot+5);
		}
		else if (!strnicmp(szSlot, "encoding=", 9)) {
			meta->enc_type = gf_strdup(szSlot+9);
		}
		else if (!strnicmp(szSlot, "image-size=", 11)) {
			CHECK_IMGPROP
			sscanf(szSlot+11, "%dx%d", &meta->image_props->width, &meta->image_props->height);
		}
		else if (!strnicmp(szSlot, "image-grid-size=", 16)) {
			CHECK_IMGPROP
			sscanf(szSlot+16, "%dx%d", &meta->image_props->num_grid_rows, &meta->image_props->num_grid_columns);
		}
		else if (!strnicmp(szSlot, "image-overlay-color=", 20)) {
			CHECK_IMGPROP
			sscanf(szSlot+20, "%d,%d,%d,%d", &meta->image_props->overlay_canvas_fill_value_r,&meta->image_props->overlay_canvas_fill_value_g,&meta->image_props->overlay_canvas_fill_value_b,&meta->image_props->overlay_canvas_fill_value_a);
		}
		else if (!strnicmp(szSlot, "image-overlay-offsets=", 22)) {
			CHECK_IMGPROP
			u32 position = 22;
			u32 comma_count = 0;
			u32 offset_index = 0;
			char *prev = szSlot+position;
			char *sub_next = strchr(szSlot+position, ',');
			while (sub_next != NULL) {
				comma_count++;
				sub_next++;
				sub_next = strchr(sub_next, ',');
			}
			meta->image_props->overlay_count = comma_count/2+1;
			meta->image_props->overlay_offsets = (GF_ImageItemOverlayOffset *)gf_malloc(meta->image_props->overlay_count*sizeof(GF_ImageItemOverlayOffset));
			if (!meta->image_props->overlay_offsets) {
				return 0;
			}
			sub_next = strchr(szSlot+position, ',');
			while (sub_next != NULL) {
				*sub_next = 0;
				meta->image_props->overlay_offsets[offset_index].horizontal = parse_u32(prev, "image-overlay-offsets[].horizontal");
				*sub_next = ',';
				sub_next++;
				prev = sub_next;
				if (sub_next) {
					sub_next = strchr(sub_next, ',');
					if (sub_next) *sub_next = 0;
					meta->image_props->overlay_offsets[offset_index].vertical = parse_u32(prev, "image-overlay-offsets[].horizontal");
					if (sub_next) {
						*sub_next = ',';
						sub_next++;
						prev = sub_next;
						sub_next = strchr(sub_next, ',');
					}
				} else {
					meta->image_props->overlay_offsets[offset_index].vertical = 0;
				}
				offset_index++;
			}
		}
		else if (!strnicmp(szSlot, "image-pasp=", 11)) {
			CHECK_IMGPROP
			sscanf(szSlot+11, "%dx%d", &meta->image_props->hSpacing, &meta->image_props->vSpacing);
		}
		else if (!strnicmp(szSlot, "image-pixi=", 11)) {
			CHECK_IMGPROP
			if (strchr(szSlot+11, ',') == NULL) {
				meta->image_props->num_channels = 1;
				meta->image_props->bits_per_channel[0] = parse_u32(szSlot+11, "image-pixi");
			} else {
				meta->image_props->num_channels = 3;
				sscanf(szSlot+11, "%u,%u,%u", &(meta->image_props->bits_per_channel[0]), &(meta->image_props->bits_per_channel[1]), &(meta->image_props->bits_per_channel[2]));
			}
		}
		else if (!strnicmp(szSlot, "image-rloc=", 11)) {
			CHECK_IMGPROP
			sscanf(szSlot+11, "%dx%d", &meta->image_props->hOffset, &meta->image_props->vOffset);
		}
		else if (!strnicmp(szSlot, "rotation=", 9)) {
			CHECK_IMGPROP
			meta->image_props->angle = parse_u32(szSlot+9, "rotation");
		}
		else if (!strnicmp(szSlot, "mirror-axis=", 12)) {
			CHECK_IMGPROP
			meta->image_props->mirror = (!strnicmp(szSlot+12, "vertical", 8) ? 1 : 2);
		}
		else if (!strnicmp(szSlot, "clap=", 5)) {
			CHECK_IMGPROP
			sscanf(szSlot + 5, "%d,%d,%d,%d,%d,%d,%d,%d", &meta->image_props->clap_wnum, &meta->image_props->clap_wden,
					   &meta->image_props->clap_hnum, &meta->image_props->clap_hden,
					   &meta->image_props->clap_honum, &meta->image_props->clap_hoden,
					   &meta->image_props->clap_vonum, &meta->image_props->clap_voden);
		}
		else if (!stricmp(szSlot, "hidden")) {
			CHECK_IMGPROP
			meta->image_props->hidden = GF_TRUE;
		}
		else if (!stricmp(szSlot, "alpha")) {
			CHECK_IMGPROP
			meta->image_props->alpha = GF_TRUE;
		}
		else if (!stricmp(szSlot, "depth")) {
			CHECK_IMGPROP
			meta->image_props->depth = GF_TRUE;
 		}
		//"ref" (without '=') is for data reference, "ref=" is for item references
		else if (!stricmp(szSlot, "ref")) {
			CHECK_IMGPROP
			meta->image_props->use_reference = GF_TRUE;
		}
		else if (!strnicmp(szSlot, "it=", 3)) {
			CHECK_IMGPROP
			meta->image_props->item_ref_id = parse_u32(szSlot+3, "it");
		}
		else if (!strnicmp(szSlot, "itp=", 4)) {
			CHECK_IMGPROP
			meta->image_props->item_ref_id = parse_u32(szSlot+4, "itp");
			meta->image_props->copy_props = 1;
		}
		else if (!strnicmp(szSlot, "time=", 5)) {
			Float s=0, e=0, step=0;
			CHECK_IMGPROP
			if (sscanf(szSlot+5, "%f-%f/%f", &s, &e, &step)==3) {
				meta->image_props->time = s;
				meta->image_props->end_time = e;
				meta->image_props->step_time = step;
			} else if (sscanf(szSlot+5, "%f-%f", &s, &e)==2) {
				meta->image_props->time = s;
				meta->image_props->end_time = e;
			} else if (sscanf(szSlot+5, "%f/%f", &s, &step)==2) {
				meta->image_props->time = s;
				meta->image_props->step_time = step;
			} else if (sscanf(szSlot+5, "%f", &s)==1) {
				meta->image_props->time = s;
			}
		}
		else if (!strnicmp(szSlot, "samp=", 5)) {
			CHECK_IMGPROP
			meta->image_props->sample_num = parse_u32(szSlot+5, "samp");
			meta->root_meta = 1;
		}
		else if (!strnicmp(szSlot, "group=", 6)) {
			char type[5];
			sscanf(szSlot, "group=%4s,%u", type, &meta->group_id);
			meta->group_type = GF_4CC(type[0], type[1], type[2], type[3]);
		}
		else if (!stricmp(szSlot, "split_tiles")) {
			CHECK_IMGPROP
			meta->image_props->tile_mode = TILE_ITEM_ALL_BASE;
		}
		else if (!stricmp(szSlot, "dref")) {
			meta->use_dref = 1;
		}
		else if (!stricmp(szSlot, "primary")) {
			meta->primary = 1;
		}
		else if (!stricmp(szSlot, "binary")) {
			if (meta->act_type==META_ACTION_SET_XML) meta->act_type=META_ACTION_SET_BINARY_XML;
		}
		else if (!strnicmp(szSlot, "icc_path=", 9)) {
			CHECK_IMGPROP
			strcpy(meta->image_props->iccPath, szSlot+9);
		}
		else if (!stricmp(szSlot, "agrid") || !strnicmp(szSlot, "agrid=", 6)) {
			CHECK_IMGPROP
			meta->image_props->auto_grid = GF_TRUE;
			if (!strnicmp(szSlot, "agrid=", 6))
				meta->image_props->auto_grid_ratio = atof(szSlot+6);
		}
		else if (!strnicmp(szSlot, "av1_op_index=", 13)) {
			CHECK_IMGPROP
			meta->image_props->av1_op_index = parse_u32(szSlot+13, "av1_op_index");
		}
		else if (!stricmp(szSlot, "replace")) {
			meta->replace = GF_TRUE;
		}
		else if (!strnicmp(szSlot, "keep_props=", 11)) {
			CHECK_IMGPROP
			meta->keep_props = gf_strdup(szSlot+11);
		}
		else if (!strnicmp(szSlot, "auxt=", 5)) {
			CHECK_IMGPROP
			meta->image_props->aux_urn = gf_strdup(szSlot+5);
		}
		else if (!strnicmp(szSlot, "auxd=", 5)) {
			CHECK_IMGPROP
			GF_Err e = gf_file_load_data(szSlot+5, (u8 **) &meta->image_props->aux_data, &meta->image_props->aux_data_len);
			if (e) {
				M4_LOG(GF_LOG_ERROR, ("Failed to load auxiliary data file %s: %s\n", szSlot+5, gf_error_to_string(e) ));
				return 1;
			}
		}
		else if (!strchr(szSlot, '=')) {
			switch (meta->act_type) {
			case META_ACTION_SET_TYPE:
				if (!stricmp(szSlot, "null") || !stricmp(szSlot, "0")) meta->meta_4cc = 0;
				else meta->meta_4cc = GF_4CC(szSlot[0], szSlot[1], szSlot[2], szSlot[3]);
				break;
			case META_ACTION_ADD_ITEM:
			case META_ACTION_ADD_IMAGE_ITEM:
			case META_ACTION_SET_XML:
			case META_ACTION_DUMP_XML:
				if (!strncmp(szSlot, "dopt", 4) || !strncmp(szSlot, "sopt", 4) || !strncmp(szSlot, "@", 1)) {
					if (next) next[0]=':';
					next=NULL;
				}
				//cat as -add arg
				gf_dynstrcat(&meta->szPath, szSlot, ":");
				if (!meta->szPath) return 2;
				break;
			case META_ACTION_REM_ITEM:
			case META_ACTION_SET_PRIMARY_ITEM:
			case META_ACTION_DUMP_ITEM:
				meta->item_id = parse_u32(szSlot, "Item ID");
				break;
			default:
				break;
			}
		}
		if (!next) break;
		opts += strlen(szSlot);
		next[0] = ':';
	}
#endif //GPAC_DISABLE_ISOM_WRITE
	return 0;
}


static Bool parse_tsel_args(char *opts, TSELActionType act)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	TrackIdentifier refTrackID = {0, 0};
	Bool has_switch_id;
	u32 switch_id = 0;
	u32 criteria[30];
	u32 nb_criteria = 0;
	TSELAction *tsel_act;
	char szSlot[1024];

	has_switch_id = 0;

	if (!opts) return 0;
	while (1) {
		char *next;
		if (!opts || !opts[0]) return 0;
		if (opts[0]==':') opts += 1;
		strcpy(szSlot, opts);
		next = gf_url_colon_suffix(szSlot, '=');
		if (next) next[0] = 0;


		if (!strnicmp(szSlot, "refTrack=", 9)) {
			parse_track_id(&refTrackID, szSlot+9, GF_FALSE);
		} else if (!strnicmp(szSlot, "switchID=", 9)) {
			if (parse_s32(szSlot+9, "switchID")<0) {
				switch_id = 0;
				has_switch_id = 0;
			} else {
				switch_id = parse_u32(szSlot+9, "switchID");
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
			tsel_acts = gf_realloc(tsel_acts, sizeof(TSELAction) * (nb_tsel_acts + 1));
			if (!tsel_acts) return 2;

			tsel_act = &tsel_acts[nb_tsel_acts];
			nb_tsel_acts++;

			memset(tsel_act, 0, sizeof(TSELAction));
			tsel_act->act_type = act;
			char *tk_id = strchr(szSlot, '=');
			if (!tk_id) tk_id = szSlot;
			else tk_id++;

			parse_track_id(&tsel_act->target_track, tk_id, GF_FALSE);

			tsel_act->reference_track = refTrackID;
			tsel_act->switchGroupID = switch_id;
			tsel_act->is_switchGroup = has_switch_id;
			tsel_act->nb_criteria = nb_criteria;
			memcpy(tsel_act->criteria, criteria, sizeof(u32)*nb_criteria);

			if (!refTrackID.ID_or_num)
				refTrackID = tsel_act->target_track;

			open_edit = GF_TRUE;
		}
		opts += strlen(szSlot);
	}
#endif // GPAC_DISABLE_ISOM_WRITE
	return 0;
}


GF_DashSegmenterInput *set_dash_input(GF_DashSegmenterInput *dash_inputs, char *name, u32 *nb_dash_inputs)
{
	GF_DashSegmenterInput *di;
	Bool skip_rep_id = GF_FALSE;
	char *other_opts = NULL;
	char *sep = gf_url_colon_suffix(name, '=');

	dash_inputs = gf_realloc(dash_inputs, sizeof(GF_DashSegmenterInput) * (*nb_dash_inputs + 1) );
	memset(&dash_inputs[*nb_dash_inputs], 0, sizeof(GF_DashSegmenterInput) );
	di = &dash_inputs[*nb_dash_inputs];
	(*nb_dash_inputs)++;

	if (sep) {
		char *opts, *first_opt;
		first_opt = sep;
		opts = sep+1;
		while (opts) {
			char *xml_start = strchr(opts, '<');
			//none of our options use filenames, so don't check for '='
			sep = gf_url_colon_suffix(opts, 0);
			//escape XML
			if (xml_start && (xml_start<sep)) {
				char *xml_end = strstr(opts, ">:");
				if (xml_end) sep = xml_end+1;
				else sep = NULL;
			}

			if (sep && !strncmp(sep, "://", 3) && strncmp(sep, ":@", 2)) sep = gf_url_colon_suffix(sep+3, 0);
			if (sep) sep[0] = 0;

			if (!strnicmp(opts, "id=", 3)) {
				if (!stricmp(opts+3, "NULL"))
					skip_rep_id = GF_TRUE;
				else
					di->representationID = gf_strdup(opts+3);
				/*we allow the same repID to be set to force muxed representations*/
			}
			else if (!strnicmp(opts, "dur=", 4)) gf_parse_lfrac(opts+4, &di->media_duration);
			else if (!strnicmp(opts, "period=", 7)) di->periodID = gf_strdup(opts+7);
			else if (!strnicmp(opts, "BaseURL=", 8)) {
				di->baseURL = (char **)gf_realloc(di->baseURL, (di->nb_baseURL+1)*sizeof(char *));
				di->baseURL[di->nb_baseURL] = gf_strdup(opts+8);
				di->nb_baseURL++;
			} else if (!strnicmp(opts, "bandwidth=", 10)) di->bandwidth = parse_u32(opts+10, "bandwidth");
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
				} else if (!strnicmp(opts, "desc_as_c=", 10)) {
					nb_descs = &di->nb_as_c_descs;
					descs = &di->as_c_descs;
					opt_offset = 10;
				} else if (!strnicmp(opts, "desc_rep=", 9)) {
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
					memcpy((*descs)[(*nb_descs)-1], opts, len);
					(*descs)[(*nb_descs)-1][len] = 0;
				}

			}
			else if (!strnicmp(opts, "xlink=", 6)) di->xlink = gf_strdup(opts+6);
			else if (!strnicmp(opts, "sscale", 6)) di->sscale = GF_TRUE;
			else if (!strnicmp(opts, "pdur=", 5)) gf_parse_frac(opts+5, &di->period_duration);
			else if (!strnicmp(opts, "period_duration=", 16)) gf_parse_frac(opts+16, &di->period_duration);
			else if (!strnicmp(opts, "ddur=", 5)) gf_parse_frac(opts+5, &di->dash_duration);
			else if (!strnicmp(opts, "duration=", 9)) gf_parse_frac(opts+9, &di->dash_duration);
			else if (!strnicmp(opts, "asID=", 5)) di->asID = parse_u32(opts+5, "asID");
			else if (!strnicmp(opts, "sn=", 3)) di->startNumber = parse_u32(opts+3, "sn");
			else if (!strnicmp(opts, "tpl=", 4)) di->seg_template = gf_strdup(opts+4);
			else if (!strnicmp(opts, "hls=", 4)) di->hls_pl = gf_strdup(opts+4);
			else if (!strnicmp(opts, "trackID=", 8)) di->track_id = parse_u32(opts+8, "trackID");
			else if (!strnicmp(opts, "@", 1)) {
				Bool old_syntax = (opts[1]=='@') ? GF_TRUE : GF_FALSE;
				if (sep) sep[0] = ':';
				di->filter_chain = gf_strdup(opts + (old_syntax ? 2 : 1) );
				sep = NULL;
				if (!old_syntax && (strstr(di->filter_chain, "@@")!=NULL)) {
					skip_rep_id = GF_TRUE;
				}
			} else {
				gf_dynstrcat(&other_opts, opts, ":");
			}

			if (!sep) break;
			sep[0] = ':';
			opts = sep+1;
		}
		first_opt[0] = '\0';
	}
	di->file_name = name;
	di->source_opts = other_opts;

	if (!skip_rep_id && !di->representationID) {
		char szRep[100];
		sprintf(szRep, "%d", *nb_dash_inputs);
		di->representationID = gf_strdup(szRep);
	}

	return dash_inputs;
}

static Bool create_new_track_action(char *arg_val, u32 act_type, u32 dump_type)
{
	TrackAction *tka;
	char *param = arg_val;
	tracks = (TrackAction *)gf_realloc(tracks, sizeof(TrackAction) * (nb_track_act+1));
	if (!tracks) return GF_FALSE;

	tka = & tracks[nb_track_act];
	nb_track_act++;

	memset(tka, 0, sizeof(TrackAction) );
	tka->act_type = act_type;
	tka->dump_type = dump_type;
	if (act_type != TRACK_ACTION_RAW_EXTRACT) {
		open_edit = GF_TRUE;
		do_save = GF_TRUE;
	}

	if ((act_type==TRACK_ACTION_SET_ID) || (act_type==TRACK_ACTION_SWAP_ID)) {
		char *sep = strchr(param, ':');
		if (!sep) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for -set-track-id - expecting \"id1:id2\" got \"%s\"\n", param));
			return GF_FALSE;
		}
		sep[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		sep[0] = ':';
		if (act_type==TRACK_ACTION_SWAP_ID)
			parse_track_id(&tka->newTrackID, sep+1, GF_FALSE);
		else
			tka->newTrackID.ID_or_num = parse_u32(sep+1, "ID");
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_PAR) {
		char *ext;
		ext = strchr(param, '=');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track par - expecting tkID=none or tkID=PAR_NUM:PAR_DEN got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';

		if (!stricmp(ext+1, "none"))
			tka->par_num = tka->par_den = 0;
		else if (!stricmp(ext+1, "auto")) {
			tka->par_num = tka->par_den = -1;
			tka->force_par = 1;
		}
		else if (!stricmp(ext+1, "force")) {
			tka->par_num = tka->par_den = 1;
			tka->force_par = 1;
		}
		else {
			if (ext[1]=='w') {
				tka->rewrite_bs = 1;
				ext++;
			}
			if (sscanf(ext+1, "%d:%d", &tka->par_num, &tka->par_den) != 2) {
				M4_LOG(GF_LOG_ERROR, ("Bad format for track par - expecting tkID=PAR_NUM:PAR_DEN got %s\n", param));
				return GF_FALSE;
			}
		}
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_CLAP) {
		char *ext = strchr(param, '=');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track clap - expecting tkID=none or tkID=Wn,Wd,Hn,Hd,HOn,HOd,VOn,VOd got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';
		if (stricmp(ext + 1, "none")) {
			if (sscanf(ext + 1, "%d,%d,%d,%d,%d,%d,%d,%d", &tka->clap_wnum, &tka->clap_wden, &tka->clap_hnum, &tka->clap_hden, &tka->clap_honum, &tka->clap_hoden, &tka->clap_vonum, &tka->clap_voden) != 8) {

				M4_LOG(GF_LOG_ERROR, ("Bad format for track clap - expecting tkID=none or tkID=Wn,Wd,Hn,Hd,HOn,HOd,VOn,VOd got %s\n", param));
				return GF_FALSE;
			}
		}
		return GF_TRUE;
	}

	if (act_type==TRACK_ACTION_SET_MX) {
		char *ext = strchr(param, '=');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track matrix - expecting ID=none or ID=M1:M2:M3:M4:M5:M6:M7:M8:M9 got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';
		if (!stricmp(ext + 1, "none")) {
			memset(tka->mx, 0, sizeof(s32)*9);
			tka->mx[0] = tka->mx[4] = 0x00010000;
			tka->mx[8] = 0x40000000;
		} else {
			s32 res;
			if (strstr(ext+1, "0x")) {
				res = sscanf(ext + 1, "0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d:0x%d", &tka->mx[0], &tka->mx[1], &tka->mx[2], &tka->mx[3], &tka->mx[4], &tka->mx[5], &tka->mx[6], &tka->mx[7], &tka->mx[8]);
			} else {
				res = sscanf(ext + 1, "%d:%d:%d:%d:%d:%d:%d:%d:%d", &tka->mx[0], &tka->mx[1], &tka->mx[2], &tka->mx[3], &tka->mx[4], &tka->mx[5], &tka->mx[6], &tka->mx[7], &tka->mx[8]);
			}
			if (res != 9) {
				M4_LOG(GF_LOG_ERROR, ("Bad format for track matrix - expecting ID=none or ID=M1:M2:M3:M4:M5:M6:M7:M8:M9 got %s\n", param));
				return GF_FALSE;
			}
		}
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_EDITS) {
		char *ext = strchr(param, '=');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track edits - expecting ID=EDITS got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';
		tka->string = gf_strdup(ext+1);
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_LANGUAGE) {
		char *ext = strchr(param, '=');
		if (!strnicmp(param, "all=", 4)) {
			strncpy(tka->lang, param + 4, LANG_SIZE-1);
		}
		else if (!ext) {
			strncpy(tka->lang, param, LANG_SIZE-1);
		} else {
			strncpy(tka->lang, ext + 1, LANG_SIZE-1);
			ext[0] = 0;
			parse_track_id(&tka->target_track, param, GF_FALSE);
			ext[0] = '=';
		}
		return GF_TRUE;
	}
	if ((act_type==TRACK_ACTION_SET_KIND) || (act_type==TRACK_ACTION_REM_KIND)) {
		char *ext;
		char *scheme_start = NULL;

		//extract trackID
		if (!strnicmp(param, "all=", 4)) {
			scheme_start = param + 4;
		} else {
			ext = strchr(param, '=');
			if (ext) {
				ext[0] = 0;
				parse_track_id(&tka->target_track, param, GF_FALSE);
				if (tka->target_track.ID_or_num) {
					scheme_start = ext + 1;
				} else {
					scheme_start = param;
				}
				ext[0] = '=';
			} else {
				scheme_start = param;
			}
		}

		//extract scheme and value - if not, remove kind
		if (!scheme_start || !scheme_start[0]) {
			M4_LOG(GF_LOG_ERROR, ("Missing kind scheme - expecting ID=schemeURI=value got %s\n", param));
			return GF_FALSE;
		} else {
			ext = strchr(scheme_start, '=');
			if (!ext) {
				tka->kind_scheme = gf_strdup(scheme_start);
			} else {
				ext[0] = 0;
				tka->kind_scheme = gf_strdup(scheme_start);
				ext[0] = '=';
				tka->kind_value = gf_strdup(ext + 1);
			}
		}
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_DELAY) {
		char *ext = strchr(param, '=');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track delay - expecting tkID=DLAY got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';
		if (sscanf(ext+1, "%d/%u", &tka->delay.num, &tka->delay.den) != 2) {
			tka->delay.num = parse_s32(ext + 1, "delay");
			tka->delay.den = 1000;
		}
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_REFERENCE) {
		char *ext = strchr(param, '=');
		if (!ext) ext = strchr(param, ':');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track reference - expecting tkID:XXXX:refID got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';

		char *ext2 = strchr(ext, ':');
		if (!ext2) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track reference - expecting tkID:XXXX:refID got %s\n", param));
			return GF_FALSE;
		}
		ext2[0] = 0;
		strncpy(tka->lang, ext+1, LANG_SIZE-1);
		ext2[0] = ':';
		parse_track_id(&tka->newTrackID, ext2 + 1, GF_FALSE);
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_HANDLER_NAME) {
		char *ext = strchr(param, '=');
		if (!ext) {
			M4_LOG(GF_LOG_ERROR, ("Bad format for track name - expecting tkID=name got %s\n", param));
			return GF_FALSE;
		}
		ext[0] = 0;
		parse_track_id(&tka->target_track, param, GF_FALSE);
		ext[0] = '=';
		tka->hdl_name = ext + 1;
		return GF_TRUE;
	}
	if (act_type==TRACK_ACTION_SET_KMS_URI) {
		char *ext = strchr(param, '=');

		if (!strnicmp(param, "all=", 4)) {
			tka->kms = param + 4;
		} else if (!ext) {
			tka->kms = param;
		} else {
			tka->kms = ext + 1;
			ext[0] = 0;
			parse_track_id(&tka->target_track, param, GF_FALSE);
			ext[0] = '=';
		}
		return GF_TRUE;
	}
	if ((act_type==TRACK_ACTION_SET_TIME) || (act_type==TRACK_ACTION_SET_MEDIA_TIME)) {
		struct tm time;
		char *ext = strchr(arg_val, '=');
		if (ext) {
			ext[0] = 0;
			parse_track_id(&tka->target_track, arg_val, GF_FALSE);
			ext[0] = '=';
			arg_val = ext+1;
		}
		memset(&time, 0, sizeof(struct tm));
		sscanf(arg_val, "%d/%d/%d-%d:%d:%d", &time.tm_mday, &time.tm_mon, &time.tm_year, &time.tm_hour, &time.tm_min, &time.tm_sec);
		time.tm_isdst = 0;
		time.tm_year -= 1900;
		time.tm_mon -= 1;
		tka->time = GF_ISOM_MAC_TIME_OFFSET;
		tka->time += mktime(&time);
		return GF_TRUE;
	}

	while (param) {
		param = gf_url_colon_suffix(param, '=');
		if (param) {
			*param = 0;
			param++;
#ifndef GPAC_DISABLE_MEDIA_EXPORT
			if (!strncmp("vttnomerge", param, 10)) {
				tka->dump_type |= GF_EXPORT_WEBVTT_NOMERGE;
			} else if (!strncmp("layer", param, 5)) {
				tka->dump_type |= GF_EXPORT_SVC_LAYER;
			} else if (!strncmp("full", param, 4)) {
				tka->dump_type |= GF_EXPORT_NHML_FULL;
			} else if (!strncmp("embedded", param, 8)) {
				tka->dump_type |= GF_EXPORT_WEBVTT_META_EMBEDDED;
			} else if (!strncmp("output=", param, 7)) {
				tka->out_name = gf_strdup(param+7);
			} else if (!strncmp("src=", param, 4)) {
				tka->src_name = gf_strdup(param+4);
			} else if (!strncmp("str=", param, 4)) {
				tka->string = gf_strdup(param+4);
			} else if (!strncmp("box=", param, 4)) {
				tka->src_name = gf_strdup(param+4);
				tka->sample_num = 1;
			} else if (!strncmp("type=", param, 4)) {
				tka->udta_type = GF_4CC(param[5], param[6], param[7], param[8]);
			} else if (tka->dump_type == GF_EXPORT_RAW_SAMPLES) {
				tka->sample_num = parse_u32(param, "Sample number");
			}
#endif
		}
	}
	if (arg_val) {
		parse_track_id(&tka->target_track, arg_val, GF_TRUE);
	}
	return GF_TRUE;
}

u32 parse_track_dump(char *arg, u32 dump_type)
{
	if (!create_new_track_action(arg, TRACK_ACTION_RAW_EXTRACT, dump_type))
		return 2;
	track_dump_type = dump_type;
	return 0;
}
u32 parse_track_action(char *arg, u32 act_type)
{
	if (!create_new_track_action(arg, act_type, 0)) {
		return 2;
	}
	return 0;
}

u32 parse_comp_box(char *arg_val, u32 opt)
{
	compress_top_boxes = arg_val;
	size_top_box = opt;
	return 0;
}
u32 parse_dnal(char *arg_val, u32 opt)
{
	parse_track_id(&dump_nal_track, arg_val, GF_FALSE);
	dump_nal_type = opt;
	return 0;
}
u32 parse_dsap(char *arg_val, u32 opt)
{
	parse_track_id(&dump_saps_track, arg_val, GF_FALSE);
	dump_saps_mode = opt;
	return 0;
}

u32 parse_bs_switch(char *arg_val, u32 opt)
{
	if (!stricmp(arg_val, "no") || !stricmp(arg_val, "off")) bitstream_switching_mode = GF_DASH_BSMODE_NONE;
	else if (!stricmp(arg_val, "merge"))  bitstream_switching_mode = GF_DASH_BSMODE_MERGED;
	else if (!stricmp(arg_val, "multi"))  bitstream_switching_mode = GF_DASH_BSMODE_MULTIPLE_ENTRIES;
	else if (!stricmp(arg_val, "single"))  bitstream_switching_mode = GF_DASH_BSMODE_SINGLE;
	else if (!stricmp(arg_val, "inband"))  bitstream_switching_mode = GF_DASH_BSMODE_INBAND;
	else if (!stricmp(arg_val, "pps"))  bitstream_switching_mode = GF_DASH_BSMODE_INBAND_PPS;
	else {
		M4_LOG(GF_LOG_ERROR, ("Unrecognized bitstream switching mode \"%s\" - please check usage\n", arg_val));
		return 2;
	}
	return 0;
}

u32 parse_cp_loc(char *arg_val, u32 opt)
{
	if (!strcmp(arg_val, "both")) cp_location_mode = GF_DASH_CPMODE_BOTH;
	else if (!strcmp(arg_val, "as")) cp_location_mode = GF_DASH_CPMODE_ADAPTATION_SET;
	else if (!strcmp(arg_val, "rep")) cp_location_mode = GF_DASH_CPMODE_REPRESENTATION;
	else {
		M4_LOG(GF_LOG_ERROR, ("Unrecognized ContentProtection loction mode \"%s\" - please check usage\n", arg_val));
		return 2;
	}
	return 0;
}

u32 parse_pssh(char *arg_val, u32 opt)
{
	if (!strcmp(arg_val, "f")) pssh_mode = GF_DASH_PSSH_MOOF;
	else if (!strcmp(arg_val, "v")) pssh_mode = GF_DASH_PSSH_MOOV;
	else if (!strcmp(arg_val, "m")) pssh_mode = GF_DASH_PSSH_MPD;
	else if (!strcmp(arg_val, "mf") || !strcmp(arg_val, "fm")) pssh_mode = GF_DASH_PSSH_MOOF_MPD;
	else if (!strcmp(arg_val, "mv") || !strcmp(arg_val, "vm")) pssh_mode = GF_DASH_PSSH_MOOV_MPD;
	else if (!strcmp(arg_val, "n")) pssh_mode = GF_DASH_PSSH_NONE;
	else pssh_mode = GF_DASH_PSSH_MOOV;
	return 0;
}
u32 parse_sdtp(char *arg_val, u32 opt)
{
	if (!stricmp(arg_val, "both")) sdtp_in_traf = 2;
	else if (!stricmp(arg_val, "sdtp")) sdtp_in_traf = 1;
	else sdtp_in_traf = 0;
	return 0;
}

u32 parse_rap_ref(char *arg_val, u32 opt)
{
	if (arg_val) {
		TrackIdentifier tkid;
		if (parse_track_id(&tkid, arg_val, GF_FALSE)) {
			parse_track_action(arg_val, opt ? TRACK_ACTION_REM_NON_REFS : TRACK_ACTION_REM_NON_RAP);
		}
	}
#ifndef GPAC_DISABLE_STREAMING
	hint_flags |= GP_RTP_PCK_SIGNAL_RAP;
#endif
	seg_at_rap = 1;
	return 0;
}
u32 parse_store_mode(char *arg_val, u32 opt)
{
	do_save = GF_TRUE;
	if ((opt == 0) || (opt == 1)) {
		interleaving_time = atof(arg_val) / 1000;
		if (!interleaving_time) do_flat = 2;
		open_edit = GF_TRUE;
		no_inplace = GF_TRUE;
		if (opt==1) old_interleave = 1;
	} else if (opt==2) {
		interleaving_time = atof(arg_val);
		do_frag = GF_TRUE;
	} else {
		force_new = 2;
		interleaving_time = 0.5;
		do_flat = 1;
	}
	return 0;
}
u32 parse_base_url(char *arg_val, u32 opt)
{
	mpd_base_urls = gf_realloc(mpd_base_urls, (nb_mpd_base_urls + 1)*sizeof(char**));
	if (!mpd_base_urls) return 2;
	mpd_base_urls[nb_mpd_base_urls] = arg_val;
	nb_mpd_base_urls++;
	return 0;
}
u32 parse_multi_rtp(char *arg_val, u32 opt)
{
#ifndef GPAC_DISABLE_STREAMING
	hint_flags |= GP_RTP_PCK_USE_MULTI;
#endif
	if (arg_val)
		max_ptime = parse_u32(arg_val, "maxptime");
	return 0;
}

u32 parse_senc_param(char *arg_val, u32 opt)
{
#ifndef GPAC_DISABLE_SCENE_ENCODER
	switch (opt) {
	case 0: //-sync
		smenc_opts.flags |= GF_SM_ENCODE_RAP_INBAND;
		smenc_opts.rap_freq = parse_u32(arg_val, "sync");
		break;
	case 1: //-shadow
		smenc_opts.flags &= ~GF_SM_ENCODE_RAP_INBAND;
		smenc_opts.flags |= GF_SM_ENCODE_RAP_SHADOW;
		smenc_opts.rap_freq = parse_u32(arg_val, "shadow");
		break;
	case 2: //-carousel
		smenc_opts.flags &= ~(GF_SM_ENCODE_RAP_INBAND | GF_SM_ENCODE_RAP_SHADOW);
		smenc_opts.rap_freq = parse_u32(arg_val, "carousel");
		break;
	case 3: //-auto-quant
		smenc_opts.resolution = parse_s32(arg_val, "auto-quant");
		smenc_opts.auto_quant = 1;
		break;
	case 4: //-global-quant
		smenc_opts.resolution = parse_s32(arg_val, "global-quant");
		smenc_opts.auto_quant = 2;
		break;
	case 5: //-ctx-in or -inctx
		chunk_mode = GF_TRUE;
		input_ctx = arg_val;
	}
#endif
	return 0;
}
u32 parse_cryp(char *arg_val, u32 opt)
{
	open_edit = GF_TRUE;
	if (!opt) {
		crypt = 1;
		drm_file = arg_val;
		open_edit = GF_TRUE;
		return 0;
	}
	crypt = 2;
	if (arg_val && get_file_type_by_ext(arg_val) != 1) {
		drm_file = arg_val;
		return 0;
	}
	return 3;
}

u32 parse_dash_profile(char *arg_val, u32 opt)
{
	if (!stricmp(arg_val, "live") || !stricmp(arg_val, "simple")) dash_profile = GF_DASH_PROFILE_LIVE;
	else if (!stricmp(arg_val, "onDemand")) dash_profile = GF_DASH_PROFILE_ONDEMAND;
	else if (!stricmp(arg_val, "hbbtv1.5:live") || !stricmp(arg_val, "hbbtv1.5.live"))
		dash_profile = GF_DASH_PROFILE_HBBTV_1_5_ISOBMF_LIVE;
	else if (!stricmp(arg_val, "dashavc264:live") || !stricmp(arg_val, "dashavc264.live"))
		dash_profile = GF_DASH_PROFILE_AVC264_LIVE;
	else if (!stricmp(arg_val, "dashavc264:onDemand") || !stricmp(arg_val, "dashavc264.onDemand"))
		dash_profile = GF_DASH_PROFILE_AVC264_ONDEMAND;
	else if (!stricmp(arg_val, "dashif.ll")) dash_profile = GF_DASH_PROFILE_DASHIF_LL;
	else if (!stricmp(arg_val, "main")) dash_profile = GF_DASH_PROFILE_MAIN;
	else if (!stricmp(arg_val, "full")) dash_profile = GF_DASH_PROFILE_FULL;
	else {
		M4_LOG(GF_LOG_ERROR, ("Unrecognized DASH profile \"%s\" - please check usage\n", arg_val));
		return 2;
	}
	return 0;
}

u32 parse_fps(char *arg_val, u32 opt)
{
	u32 ticks, dts_inc;
	if (!strcmp(arg_val, "auto")) {
		M4_LOG(GF_LOG_WARNING, ("Warning, fps=auto option is deprecated\n"));
	}
	else if ((sscanf(arg_val, "%u-%u", &ticks, &dts_inc)==2) || (sscanf(arg_val, "%u/%u", &ticks, &dts_inc)==2) ) {
		if (!dts_inc) dts_inc = 1;
		import_fps.num = ticks;
		import_fps.den = dts_inc;
	} else {
		gf_parse_frac(arg_val, &import_fps);
	}
	return 0;
}

u32 parse_split(char *arg_val, u32 opt)
{
	u32 scale=1;
	char *unit;
	switch (opt) {
	case 0://-split
		unit = arg_val + (u32) (strlen(arg_val) - 1);
		if (strchr("Mm", unit[0])) scale = 60;
		else if (strchr("Hh", unit[0])) scale = 3600;
		if (scale > 1) {
			char c = unit[0];
			unit[0] = 0;
			split_duration = scale * atof(arg_val);
			unit[0] = c;
		} else {
			split_duration = atof(arg_val);
		}
		if (split_duration < 0) split_duration = 0;
		split_size = 0;
		break;
	case 1: //-split-rap, -splitr
		split_duration = -1;
		split_size = -1;
		break;
	case 2: //-split-size, -splits
		unit = arg_val + (u32) (strlen(arg_val) - 1);
		if (strchr("Mm", unit[0])) scale = 1000;
		else if (strchr("Gg", unit[0])) scale = 1000000;
		if (scale > 1) {
			char c = unit[0];
			unit[0] = 0;
			split_size = scale * parse_u32(arg_val, "Split size");
			unit[0] = c;
		} else {
			split_size = (u32)parse_u32(arg_val, "Split size");
		}
		split_duration = 0;
		break;
	case 3: //-split-chunk, -splitx
	case 4: //-splitz
	case 5: //-splitg
	case 6: //-splitf
		adjust_split_end = opt-3;
		if (!strstr(arg_val, ":") && !strstr(arg_val, "-")) {
			M4_LOG(GF_LOG_ERROR, ("Chunk extraction usage: \"-split* start:end\" expressed in seconds\n"));
			return 2;
		}
		if (strstr(arg_val, "end")) {
			if (strstr(arg_val, "end-")) {
				Double dur_end=0;
				sscanf(arg_val, "%lf:end-%lf", &split_start, &dur_end);
				split_duration = -2 - dur_end;
			} else {
				sscanf(arg_val, "%lf:end", &split_start);
				split_duration = -2;
			}
		} else {
			split_range_str = arg_val;
		}
		split_size = 0;
		break;
	}
	return 0;
}

u32 parse_brand(char *b, u32 opt)
{
	open_edit = GF_TRUE;
	switch (opt) {
	case 0: //-brand
		major_brand = GF_4CC(b[0], b[1], b[2], b[3]);
		if (b[4] == ':') {
			if (!strncmp(b+5, "0x", 2))
				sscanf(b+5, "0x%x", &minor_version);
			else
				minor_version = parse_u32(b + 5, "version");
		}
		break;
	case 1: //-ab
		brand_add = (u32*)gf_realloc(brand_add, sizeof(u32) * (nb_alt_brand_add + 1));
		if (!brand_add) return 2;
		brand_add[nb_alt_brand_add] = GF_4CC(b[0], b[1], b[2], b[3]);
		nb_alt_brand_add++;
		break;
	case 2: //-rb
		brand_rem = (u32*)gf_realloc(brand_rem, sizeof(u32) * (nb_alt_brand_rem + 1));
		if (!brand_rem) return 2;
		brand_rem[nb_alt_brand_rem] = GF_4CC(b[0], b[1], b[2], b[3]);
		nb_alt_brand_rem++;
		break;
	}
	return 0;
}

u32 parse_mpegu(char *arg_val, u32 opt)
{
	pack_file = arg_val;
	pack_wgt = GF_TRUE;
	return 0;
}

u32 parse_file_info(char *arg_val, u32 opt)
{
	print_info = opt ? 2 : 1;
	if (opt==2) {
		print_info = 2;
		if (arg_val) return 3;
		return 0;
	}
	if (arg_val) {
		if (!parse_track_id(&info_track_id, arg_val, GF_FALSE))
			return 3;
	}
	return 0;
}
u32 parse_boxpatch(char *arg_val, u32 opt)
{
	box_patch_filename = arg_val;
	char *sep = strchr(box_patch_filename, '=');
	if (sep) {
		sep[0] = 0;
		parse_track_id(&box_patch_track, box_patch_filename, GF_FALSE);
		sep[0] = '=';
		box_patch_filename = sep+1;
	}
	open_edit = GF_TRUE;
	return 0;
}
u32 parse_compress(char *arg_val, u32 opt)
{
	compress_moov = opt ? 2 : 1;
	open_edit = GF_TRUE;
	do_save = GF_TRUE;
	return 0;
}


u32 parse_dump_udta(char *code, u32 opt)
{
	char *sep;
	sep = strchr(code, ':');
	if (sep) {
		sep[0] = 0;
		parse_track_id(&dump_udta_track, code, GF_FALSE);
		sep[0] = ':';
		code = sep + 1;
	}

	if (strlen(code) == 4) {
		dump_udta_type = GF_4CC(code[0], code[1], code[2], code[3]);
	} else if (strlen(code) == 8) {
		// hex representation on 8 chars
		u32 hex1, hex2, hex3, hex4;
		if (sscanf(code, "%02x%02x%02x%02x", &hex1, &hex2, &hex3, &hex4) != 4) {
			M4_LOG(GF_LOG_ERROR, ("udta code is either a 4CC or 8 hex chars for non-printable 4CC\n"));
			return 2;
		}
		dump_udta_type = GF_4CC(hex1, hex2, hex3, hex4);
	} else {
		M4_LOG(GF_LOG_ERROR, ("udta code is either a 4CC or 8 hex chars for non-printable 4CC\n"));
		return 2;
	}
	return 0;
}

u32 parse_dump_ts(char *arg_val, u32 opt)
{
	dump_timestamps = 1;
	if (arg_val) {
		if (isdigit(arg_val[0])) {
			program_number = parse_u32(arg_val, "Program");
		} else {
			return 3;
		}
	}
	return 0;
}

u32 parse_ttxt(char *arg_val, u32 opt)
{
	if (opt) //-srt
		dump_srt = GF_TRUE;
	else
		dump_ttxt = GF_TRUE;

	import_subtitle = 1;
	ttxt_track_id.type = 0;
	ttxt_track_id.ID_or_num = 0;

	if (arg_val && (!strcmp(arg_val, "*") || !strcmp(arg_val, "@") || !strcmp(arg_val, "all")) ) {
		ttxt_track_id.ID_or_num = (u32)-1;
	} else if (arg_val) {
		if (parse_track_id(&ttxt_track_id, arg_val, GF_FALSE)==GF_FALSE)
			return 3;
	}
	return 0;
}

u32 parse_dashlive(char *arg, char *arg_val, u32 opt)
{
	dash_mode = opt ? GF_DASH_DYNAMIC_DEBUG : GF_DASH_DYNAMIC;
	dash_live = 1;
	if (arg[10] == '=') {
		dash_ctx_file = arg + 11;
	}
	dash_duration = atof(arg_val);
	return 0;
}

u32 parse_help(char *arg_val, u32 opt)
{
	if (!arg_val) PrintUsage();
	else if (opt) PrintHelp(arg_val, GF_TRUE, GF_FALSE);
	else if (!strcmp(arg_val, "general")) PrintGeneralUsage();
	else if (!strcmp(arg_val, "extract")) PrintExtractUsage();
	else if (!strcmp(arg_val, "split")) PrintSplitUsage();
	else if (!strcmp(arg_val, "dash")) PrintDASHUsage();
	else if (!strcmp(arg_val, "dump")) PrintDumpUsage();
	else if (!strcmp(arg_val, "import")) PrintImportUsage();
	else if (!strcmp(arg_val, "format")) {
		M4_LOG(GF_LOG_WARNING, ("see [filters documentation](Filters), `gpac -h codecs`, `gpac -h formats` and `gpac -h protocols` \n"));
	}
	else if (!strcmp(arg_val, "hint")) PrintHintUsage();
	else if (!strcmp(arg_val, "encode")) PrintEncodeUsage();
	else if (!strcmp(arg_val, "crypt")) PrintEncryptUsage();
	else if (!strcmp(arg_val, "meta")) PrintMetaUsage();
	else if (!strcmp(arg_val, "swf")) PrintSWFUsage();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
	else if (!strcmp(arg_val, "rtp")) {
		M4_LOG(GF_LOG_WARNING, ("RTP streaming deprecated in MP4Box, use gpac application\n"));
	}
	else if (!strcmp(arg_val, "live")) PrintLiveUsage();
#endif
	else if (!strcmp(arg_val, "core")) PrintCoreUsage();
	else if (!strcmp(arg_val, "tags")) PrintTags();
	else if (!strcmp(arg_val, "cicp")) PrintCICP();
	else if (!strcmp(arg_val, "all")) {
		PrintGeneralUsage();
		PrintExtractUsage();
		PrintDASHUsage();
		PrintSplitUsage();
		PrintDumpUsage();
		PrintImportUsage();
		PrintHintUsage();
		PrintEncodeUsage();
		PrintEncryptUsage();
		PrintMetaUsage();
		PrintSWFUsage();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
		PrintLiveUsage();
#endif
		PrintCoreUsage();
		PrintTags();
		PrintCICP();
	} else if (!strcmp(arg_val, "opts")) {
		PrintHelp("@", GF_FALSE, GF_FALSE);
	} else {
		PrintHelp(arg_val, GF_FALSE, GF_FALSE);
	}
	return 1;
}

#define DOC_AUTOGEN_WARNING \
	"<!-- automatically generated - do not edit, patch gpac/applications/mp4box/mp4box.c -->\n"

u32 parse_gendoc(char *name, u32 opt)
{
	u32 i=0;
	//gen MD
	if (!opt) {
		help_flags = GF_PRINTARG_MD | GF_PRINTARG_IS_APP;
		helpout = gf_fopen("mp4box-gen-opts.md", "w");

		fprintf(helpout, DOC_AUTOGEN_WARNING);
		fprintf(helpout, "# Syntax\n");
		gf_sys_format_help(helpout, help_flags, "MP4Box [option] input [option] [other_dash_inputs]\n"
			"  \n"
		);
		PrintGeneralUsage();
		PrintEncryptUsage();
		fprintf(helpout, "# Help Options\n");
		while (m4b_usage_args[i].name) {
			GF_GPACArg *g_arg = (GF_GPACArg *) &m4b_usage_args[i];
			i++;
			gf_sys_print_arg(helpout, help_flags, g_arg, "mp4box-general");
		}

		gf_fclose(helpout);

		helpout = gf_fopen("mp4box-import-opts.md", "w");
		fprintf(helpout, DOC_AUTOGEN_WARNING);
		PrintImportUsage();
		gf_fclose(helpout);

		helpout = gf_fopen("mp4box-dash-opts.md", "w");
		fprintf(helpout, DOC_AUTOGEN_WARNING);
		PrintDASHUsage();
		gf_fclose(helpout);

		helpout = gf_fopen("mp4box-dump-opts.md", "w");
		fprintf(helpout, DOC_AUTOGEN_WARNING);
		PrintExtractUsage();
		PrintDumpUsage();
		PrintSplitUsage();
		gf_fclose(helpout);

		helpout = gf_fopen("mp4box-meta-opts.md", "w");
		fprintf(helpout, DOC_AUTOGEN_WARNING);
		PrintMetaUsage();
		gf_fclose(helpout);


		helpout = gf_fopen("mp4box-scene-opts.md", "w");
		fprintf(helpout, DOC_AUTOGEN_WARNING);
		PrintEncodeUsage();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
		PrintLiveUsage();
#endif
		PrintSWFUsage();
		gf_fclose(helpout);

		helpout = gf_fopen("mp4box-other-opts.md", "w");
		fprintf(helpout, DOC_AUTOGEN_WARNING);
		PrintHintUsage();
		PrintTags();
		gf_fclose(helpout);
	}
	//gen man
	else {
		help_flags = GF_PRINTARG_MAN;
		helpout = gf_fopen("mp4box.1", "w");


		fprintf(helpout, ".TH MP4Box 1 2019 MP4Box GPAC\n");
		fprintf(helpout, ".\n.SH NAME\n.LP\nMP4Box \\- GPAC command-line media packager\n.SH SYNOPSIS\n.LP\n.B MP4Box\n.RI [options] \\ [file] \\ [options]\n.br\n.\n");

		PrintGeneralUsage();
		PrintExtractUsage();
		PrintDASHUsage();
		PrintSplitUsage();
		PrintDumpUsage();
		PrintImportUsage();
		PrintHintUsage();
		PrintEncodeUsage();
		PrintEncryptUsage();
		PrintMetaUsage();
		PrintSWFUsage();
		PrintTags();
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG)
		PrintLiveUsage();
#endif

		fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://wiki.gpac.io/MP4Box\n");
		fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
		".br\nFor bug reports, feature requests, more information and source code, visit https://github.com/gpac/gpac\n"
		".br\nbuild: %s\n"
		".br\nCopyright: %s\n.br\n"
		".SH SEE ALSO\n"
		".LP\ngpac(1)\n", GPAC_VERSION, gf_gpac_copyright());

		gf_fclose(helpout);
	}
	return 1;
}

u32 mp4box_parse_single_arg_class(int argc, char **argv, char *arg, u32 *arg_index, MP4BoxArg *arg_class)
{
	MP4BoxArg *arg_desc = NULL;
	char *arg_val = NULL;
	u32 i=0;
	while (arg_class[i].name) {
		arg_desc = (MP4BoxArg *) &arg_class[i];
		i++;

#ifdef TEST_ARGS
		char *sep = strchr(arg_desc->name, ' ');
		if (sep) {
			M4_LOG(GF_LOG_ERROR, ("Invalid arg %s, space not allowed\n", arg_desc->name));
			exit(1);
		}
#endif
		if (!strcmp(arg_desc->name, arg+1))
			break;
		if (arg_desc->altname && !strcmp(arg_desc->altname, arg+1))
			break;

		if (arg_desc->parse_flags & ARG_IS_FUN2) {
			if (!strncmp(arg_desc->name, arg+1, strlen(arg_desc->name) ))
				break;
		}
		arg_desc = NULL;
	}
	if (!arg_desc)
		return GF_FALSE;

	if (arg_desc->parse_flags & ARG_OPEN_EDIT) open_edit = GF_TRUE;
	if (arg_desc->parse_flags & ARG_NEED_SAVE) do_save = GF_TRUE;
	if (arg_desc->parse_flags & ARG_NO_INPLACE) no_inplace = GF_TRUE;

	if (arg_desc->type != GF_ARG_BOOL) {
		Bool has_next = GF_TRUE;
		if (*arg_index + 1 == (u32) argc)
			has_next = GF_FALSE;
		else if (argv[*arg_index + 1][0] == '-') {
			s32 v;
			if (sscanf(argv[*arg_index + 1], "%d", &v)!=1)
				has_next = GF_FALSE;
		}
		if (!has_next && ! (arg_desc->parse_flags & ARG_EMPTY) ) {
			M4_LOG(GF_LOG_ERROR, ("Missing argument value for %s - please check usage\n", arg));
			arg_parse_res = 2;
			return GF_TRUE;
		}

		if (has_next && (arg_desc->parse_flags & ARG_EMPTY) && (arg_desc->type==GF_ARG_INT)) {
			s32 ival;
			if (sscanf(argv[*arg_index + 1], "%d", &ival) != 1) {
				has_next = GF_FALSE;
				arg_val = NULL;
			}
		}
		if (has_next) {
			has_next_arg = GF_TRUE;
			*arg_index += 1;
			arg_val = argv[*arg_index];
		}
	}
	if (!arg_desc->arg_ptr) return GF_TRUE;

	if (arg_desc->parse_flags & (ARG_IS_FUN|ARG_IS_FUN2) ) {
		u32 res;
		if (arg_desc->parse_flags & ARG_PUSH_SYSARGS)
			gf_sys_set_args(argc, (const char**) argv);

		if (arg_desc->parse_flags & ARG_IS_FUN) {
			parse_arg_fun fun = (parse_arg_fun) arg_desc->arg_ptr;
			res = fun(arg_val, arg_desc->argv_val);
		} else {
			parse_arg_fun2 fun2 = (parse_arg_fun2) arg_desc->arg_ptr;
			res = fun2(arg, arg_val, arg_desc->argv_val);
		}
		//rewind, not our arg
		if ((res==3) && argv) {
			*arg_index -= 1;
			res = 0;
		}
		arg_parse_res = res;
		return GF_TRUE;
	}

	if (arg_desc->parse_flags & ARG_INT_INC) {
		* (u32 *) arg_desc->arg_ptr += 1;
		return GF_TRUE;
	}

	if (arg_desc->type == GF_ARG_BOOL) {
		if (!arg_desc->parse_flags) {
			if (arg_desc->argv_val) {
				* (u32 *) arg_desc->arg_ptr = arg_desc->argv_val;
			} else {
				* (Bool *) arg_desc->arg_ptr = GF_TRUE;
			}
		} else if (arg_desc->parse_flags & ARG_BOOL_REV) {
			* (Bool *) arg_desc->arg_ptr = GF_FALSE;
		} else if (arg_desc->parse_flags & ARG_HAS_VALUE) {
			* (u32 *) arg_desc->arg_ptr = 0;
		} else if (arg_desc->parse_flags & ARG_BIT_MASK) {
			* (u32 *) arg_desc->arg_ptr |= arg_desc->argv_val;
		} else if (arg_desc->parse_flags & ARG_BIT_MASK_REM) {
			* (u32 *) arg_desc->arg_ptr &= ~arg_desc->argv_val;
		} else if (arg_desc->argv_val) {
			* (u32 *) arg_desc->arg_ptr = arg_desc->argv_val;
		} else {
			* (u32 *) arg_desc->arg_ptr = GF_TRUE;
		}
		return GF_TRUE;
	}

	if (arg_desc->type == GF_ARG_STRING) {
		if (arg_desc->parse_flags & ARG_IS_4CC) {
			u32 alen = arg_val ? (u32) strlen(arg_val) : 0;
			if ((alen<3) || (alen>4)) {
				M4_LOG(GF_LOG_ERROR, ("Value for %s must be a 4CC, %s is not - please check usage\n", arg, arg_val));
				arg_parse_res = 2;
				return GF_TRUE;
			}
			* (u32 *) arg_desc->arg_ptr = GF_4CC(arg_val[0], arg_val[1], arg_val[2], arg_val[3]);
			return GF_TRUE;
		}

		* (char **) arg_desc->arg_ptr = arg_val;
		return GF_TRUE;
	}
	if (!arg_val) {
		M4_LOG(GF_LOG_ERROR, ("Missing value for %s - please check usage\n", arg));
		arg_parse_res = 2;
		return GF_TRUE;
	}

	if (arg_desc->type == GF_ARG_DOUBLE) {
		Double v = atof(arg_val);
		if (arg_desc->parse_flags & ARG_DIV_1000) {
			v /= 1000;
		}
		if ((arg_desc->parse_flags & ARG_NON_ZERO) && !v) {
			M4_LOG(GF_LOG_ERROR, ("Value for %s shall not be 0 - please check usage\n", arg));
			arg_parse_res = 2;
			return GF_TRUE;
		}
		* (Double *) arg_desc->arg_ptr = v;
		return GF_TRUE;
	}

	if (arg_desc->type != GF_ARG_INT) {
		M4_LOG(GF_LOG_ERROR, ("Unsupported argument type for %s - please report to gpac devs\n", arg));
		arg_parse_res = 2;
		return GF_TRUE;
	}
	if (arg_desc->parse_flags & ARG_64BITS) {
		u64 v;
		sscanf(arg_val, LLU, &v);
		if (arg_desc->parse_flags & ARG_DIV_1000) {
			v /= 1000;
		}
		if ((arg_desc->parse_flags & ARG_NON_ZERO) && !v) {
			M4_LOG(GF_LOG_ERROR, ("Value for %s shall not be 0 - please check usage\n", arg));
			arg_parse_res = 2;
			return GF_TRUE;
		}
		* (u64 *) arg_desc->arg_ptr = v;
	} else {
		s32 v;
		if (sscanf(arg_val, "%d", &v) != 1) {
			M4_LOG(GF_LOG_ERROR, ("Value for %s shall not be an integer - please check usage\n", arg));
			arg_parse_res = 2;
			return GF_TRUE;
		}
		if (arg_desc->parse_flags & ARG_DIV_1000) {
			v /= 1000;
		}
		if ((arg_desc->parse_flags & ARG_NON_ZERO) && !v) {
			M4_LOG(GF_LOG_ERROR, ("Value for %s shall not be 0 - please check usage\n", arg));
			arg_parse_res = 2;
			return GF_TRUE;
		}
		* (s32 *) arg_desc->arg_ptr = v;
	}
	return GF_TRUE;
}

Bool mp4box_parse_single_arg(int argc, char **argv, char *arg, u32 *arg_index)
{
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_gen_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_split_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_dash_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_imp_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_senc_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_crypt_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_hint_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_extr_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_dump_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_meta_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_swf_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_liveenc_args)) return GF_TRUE;
	if (mp4box_parse_single_arg_class(argc, argv, arg, arg_index, m4b_usage_args)) return GF_TRUE;

	return GF_FALSE;
}


u32 mp4box_parse_args(int argc, char **argv)
{
	u32 i;
	/*parse our args*/
	for (i = 1; i < (u32)argc; i++) {
		char *arg = argv[i];
		/*input file(s)*/
		if ((arg[0] != '-') || !stricmp(arg, "--")) {
			char *arg_val = arg;
			if (!stricmp(arg, "--")) {
				if (i+1==(u32)argc) {
					M4_LOG(GF_LOG_ERROR, ("Missing arg for `--` - please check usage\n"));
					return 2;
				}
				has_next_arg = GF_TRUE;
				arg_val = argv[i + 1];
				i++;
			}
			if (argc < 3) {
				M4_LOG(GF_LOG_ERROR, ("Error - only one input file found as argument, please check usage\n"));
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
					M4_LOG(GF_LOG_ERROR, ("Error - 2 input names specified, please check usage\n"));
					return 2;
				}
			}
			else {
				inName = arg_val;
			}
		}
		//all deprecated options
		else if (!stricmp(arg, "-grab-ts") || !stricmp(arg, "-atsc") || !stricmp(arg, "-rtp")) {
			M4_LOG(GF_LOG_ERROR, ("Deprecated fuctionnality `%s` - use gpac application\n", arg));
			return 2;
		}
		else if (!stricmp(arg, "-write-buffer")) {
			M4_LOG(GF_LOG_WARNING, ("`%s` option deprecated, use `-bs-cache-size`", arg));
			gf_opts_set_key("temp", "bs-cache-size", argv[i + 1]);
			i++;
		}
		else if (!stricmp(arg, "-pssh-moof")) {
			M4_LOG(GF_LOG_ERROR, ("`-pssh-moof` option deprecated , use `-pssh` option\n"));
			return 2;
		}
		else if (!stricmp(arg, "-tag-list")) {
			M4_LOG(GF_LOG_ERROR, ("`-tag-list` option deprecated, use `-h tags`\n"));
			return 2;
		}
		else if (!stricmp(arg, "-aviraw")) {
			M4_LOG(GF_LOG_ERROR, ("`-aviraw` option deprecated, use `-raw`\n"));
			return 2;
		}
		else if (!stricmp(arg, "-avi")) {
			M4_LOG(GF_LOG_ERROR, ("`-avi` option deprecated, use `-mux`\n"));
			return 2;
		}
		else if (!strncmp(arg, "-p=", 3)) {
			continue;
		}
#ifndef GPAC_MEMORY_TRACKING
		else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
			continue;
		}
#endif

		//parse argument
		else if (mp4box_parse_single_arg(argc, argv, arg, &i)) {
			if (arg_parse_res)
				return mp4box_cleanup(arg_parse_res);
		}
		//not a MP4Box arg
		else {
			u32 res = gf_sys_is_gpac_arg(arg);
			if (res==0) {
				PrintHelp(arg, GF_FALSE, GF_TRUE);
				return 2;
			} else if (res==2) {
				i++;
			}
		}
		//live scene encoder does not use the unified parsing and should be moved as a scene encoder filter
		if (live_scene) return 0;
	}
	return 0;
}

/*
	END OF OPTION PARSING CODE
*/



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

GF_Err HintFile(GF_ISOFile *file, u32 MTUSize, u32 max_ptime, u32 rtp_rate, u32 base_flags, Bool copy_data, Bool interleave, Bool regular_iod, Bool single_group, Bool hint_no_offset)
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
			M4_LOG(GF_LOG_INFO, ("Track ID %d disabled - skipping hint\n", gf_isom_get_track_id(file, i+1) ));
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
		if (esd && esd->decoderConfig) {
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
				if (streamType && samp) {
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
				M4_LOG(nb_done ? GF_LOG_WARNING : GF_LOG_ERROR, ("Cannot create hinter (%s)\n", gf_error_to_string(e) ));
				if (!nb_done) return e;
			}
			continue;
		}

		if (hint_no_offset)
			gf_hinter_track_force_no_offsets(hinter);

		bw = gf_hinter_track_get_bandwidth(hinter);
		tot_bw += bw;
		flags = gf_hinter_track_get_flags(hinter);

		//set extraction mode for AVC/SVC
		gf_isom_set_nalu_extract_mode(file, i+1, GF_ISOM_NALU_EXTRACT_LAYER_ONLY);

		gf_hinter_track_get_payload_name(hinter, szPayload);
		M4_LOG(GF_LOG_INFO, ("Hinting track ID %d - Type \"%s:%s\" (%s) - BW %d kbps\n", gf_isom_get_track_id(file, i+1), gf_4cc_to_str(mtype), gf_4cc_to_str(mtype), szPayload, bw));
		if (flags & GP_RTP_PCK_SYSTEMS_CAROUSEL) M4_LOG(GF_LOG_INFO, ("\tMPEG-4 Systems stream carousel enabled\n"));
		e = gf_hinter_track_process(hinter);

		if (!e) e = gf_hinter_track_finalize(hinter, has_iod);
		gf_hinter_track_del(hinter);

		if (e) {
			M4_LOG(GF_LOG_ERROR, ("Error while hinting (%s)\n", gf_error_to_string(e)));
			return e;
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
		M4_LOG(GF_LOG_WARNING, ("Warning: at least 2 timelines found in the file\nThis may not be supported by servers/players\n\n"));

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_HINTING*/

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_AV_PARSERS)

static void check_media_profile(GF_ISOFile *file, u32 track)
{
	u8 PL;
	GF_ESD *esd = gf_isom_get_esd(file, track, 1);
	if (!esd) return;

	switch (esd->decoderConfig->streamType) {
	case 0x04:
		PL = gf_isom_get_pl_indication(file, GF_ISOM_PL_VISUAL);
		if (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2) {
			GF_M4VDecSpecInfo vdsi;
			gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &vdsi);
			if (vdsi.VideoPL > PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_VISUAL, vdsi.VideoPL);
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
		{
			GF_M4ADecSpecInfo adsi;
			gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &adsi);
			if (adsi.audioPL > PL) gf_isom_set_pl_indication(file, GF_ISOM_PL_AUDIO, adsi.audioPL);
		}
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
		M4_LOG(GF_LOG_ERROR, ("Failed to parse XML file: %s\n", gf_error_to_string(e)));
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
		M4_LOG(GF_LOG_ERROR, ("Failed to parse binary sequence: %s\n", gf_error_to_string(e)));
		return e;
	}

	if (dump_std) {
		gf_fwrite(data, data_size, stdout);
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
			M4_LOG(GF_LOG_ERROR, ("Failed to open file %s\n", szFile));
			e = GF_IO_ERR;
		} else {
			if (gf_fwrite(data, data_size, t) != data_size) {
				M4_LOG(GF_LOG_ERROR, ("Failed to write output to file %s\n", szFile));
				e = GF_IO_ERR;
			}
			gf_fclose(t);
		}
	}
	gf_free(data);
	return e;
}

static u64 do_size_top_boxes(char *inName, char *compress_top_boxes, u32 mode)
{
	FILE *in;
	u64 top_size = 0;
	Bool do_all = GF_FALSE;
	GF_BitStream *bs_in;
	if (!compress_top_boxes) return GF_BAD_PARAM;
	if (!strcmp(compress_top_boxes, "all") || !strcmp(compress_top_boxes, "*") || !strcmp(compress_top_boxes, "@"))
		do_all = GF_TRUE;

	in = gf_fopen(inName, "rb");
	if (!in) return GF_URL_ERROR;
	bs_in = gf_bs_from_file(in, GF_BITSTREAM_READ);
	while (gf_bs_available(bs_in)) {
		const char *stype;
		u32 hdr_size = 8;
		u64 lsize = gf_bs_read_u32(bs_in);
		u32 type = gf_bs_read_u32(bs_in);

		if (lsize==1) {
			lsize = gf_bs_read_u64(bs_in);
			hdr_size = 16;
		} else if (lsize==0) {
			lsize = gf_bs_available(bs_in) + 8;
		}
		stype = gf_4cc_to_str(type);
		if (do_all || strstr(compress_top_boxes, stype)) {
			//only count boxes
			if (mode==2) {
				top_size += 1;
			} else {
				top_size += lsize;
			}
		}
		gf_bs_skip_bytes(bs_in, lsize - hdr_size);
	}
	gf_bs_del(bs_in);
	gf_fclose(in);
	return top_size;

}

static GF_Err do_compress_top_boxes(char *inName, char *outName)
{
	FILE *in, *out;
	u8 *buf;
	u32 buf_alloc, comp_size;
	s32 bytes_comp=0;
	s32 bytes_uncomp=0;
	GF_Err e = GF_OK;
	u64 source_size, dst_size;
	u32 orig_box_overhead;
	u32 final_box_overhead;
	u32 nb_added_box_bytes=0;
	Bool has_mov = GF_FALSE;
	Bool replace_all = !strcmp(compress_top_boxes, "*");
	GF_BitStream *bs_in, *bs_out;

	if (!outName) {
		M4_LOG(GF_LOG_ERROR, ("Missing output file name\n"));
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

	orig_box_overhead = 0;
	final_box_overhead = 0;
	while (gf_bs_available(bs_in)) {
		u32 size = gf_bs_read_u32(bs_in);
		u32 type = gf_bs_read_u32(bs_in);
		const char *b4cc = gf_4cc_to_str(type);
		const u8 *replace = (const u8 *) strstr(compress_top_boxes, b4cc);
		if (!strcmp(b4cc, "moov")) has_mov = GF_TRUE;

		if (!replace && !replace_all) {
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

		size-=8;

		if (size>buf_alloc) {
			buf_alloc = size;
			buf = gf_realloc(buf, buf_alloc);
		}
		gf_bs_read_data(bs_in, buf, size);

		replace+=5;

		comp_size = buf_alloc;

		e = gf_gz_compress_payload(&buf, size, &comp_size);
		if (e) break;

		if (comp_size>buf_alloc) {
			buf_alloc = comp_size;
		}
		bytes_uncomp += size;
		bytes_comp += comp_size;

		//write size
		gf_bs_write_u32(bs_out, comp_size+8);
		//write type
		gf_bs_write_data(bs_out, replace, 4);
		//write data
		gf_bs_write_data(bs_out, buf, comp_size);

		final_box_overhead += 8+comp_size;
	}
	dst_size = gf_bs_get_position(bs_out);

	if (buf) gf_free(buf);
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_fclose(in);
	gf_fclose(out);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error compressing: %s\n", gf_error_to_string(e)));
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


		fprintf(stderr, "Log format:\nname\torig\tcomp\tgain\tadded_bytes\torate\tcrate\tsamples\tduration\tobbps\tcbbps\n");
		fprintf(stdout, "%s\t%d\t%d\t%g\t%d\t%g\t%g\t%d\t%g\t%g\t%g\n", inName, bytes_uncomp, bytes_comp, ((Double)(bytes_uncomp-bytes_comp)*100)/bytes_uncomp, nb_added_box_bytes, rate, new_rate, nb_samples, duration, ((Double)orig_box_overhead)/nb_samples, ((Double)final_box_overhead)/nb_samples );

		fprintf(stderr, "%s Compressing top-level boxes saved %d bytes out of %d (reduced by %g %%) additional bytes %d original rate %g kbps new rate %g kbps, orig %g box bytes/sample final %g bytes/sample\n", inName, bytes_uncomp-bytes_comp, bytes_uncomp, ((Double)(bytes_uncomp-bytes_comp)*100)/bytes_uncomp, nb_added_box_bytes, rate, new_rate, ((Double)orig_box_overhead)/nb_samples, ((Double)final_box_overhead)/nb_samples );

	} else {
		fprintf(stderr, "Log format:\nname\torig\tcomp\tgain\tadded_bytes\n");
		fprintf(stdout, "%s\t%d\t%d\t%g\t%d\n", inName, bytes_uncomp, bytes_comp, ((Double) (bytes_uncomp - bytes_comp)*100)/(bytes_uncomp), nb_added_box_bytes);

		fprintf(stderr, "%s Compressing top-level boxes saved %d bytes out of %d (reduced by %g %%) additional bytes %d\n", inName, bytes_uncomp-bytes_comp, bytes_uncomp, ((Double)(bytes_uncomp-bytes_comp)*100)/bytes_uncomp, nb_added_box_bytes);

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
		gf_fwrite(hash, 20, stdout);
	} else if (dump_std==1) {
		for (i=0; i<20; i++) fprintf(stdout, "%02X", hash[i]);
	}
	fprintf(stderr, "File hash (SHA-1): ");
	for (i=0; i<20; i++) fprintf(stderr, "%02X", hash[i]);
	fprintf(stderr, "\n");

	return GF_OK;
}

static u32 do_raw_cat()
{
	char chunk[4096];
	int ret=0;
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
		u32 nb_bytes = (u32) gf_fread(chunk, 4096, fin);
		if (gf_fwrite(chunk, nb_bytes, fout) != nb_bytes) {
			ret = 1;
			fprintf(stderr, "Error appengin file\n");
			break;
		}
		done += nb_bytes;
		fprintf(stderr, "Appending file %s - %02.2f done\r", raw_cat, 100.0*done/to_copy);
		if (done >= to_copy) break;
	}
	gf_fclose(fin);
	gf_fclose(fout);
	return mp4box_cleanup(ret);
}

static u32 do_write_udp()
{
#ifndef GPAC_DISABLE_NETWORK
	GF_Err e;
	u32 res = 0;
	GF_Socket *sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	u16 port = 2345;
	char *sep = strrchr(udp_dest, ':');
	if (sep) {
		sep[0] = 0;
		port = parse_u32(sep+1, "port");
	}
	e = gf_sk_bind( sock, NULL, port, udp_dest, port, 0);
	if (sep) sep[0] = ':';
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Failed to bind socket to %s: %s\n", udp_dest, gf_error_to_string(e) ));
		res = 1;
	} else {
		e = gf_sk_send(sock, (u8 *) inName, (u32)strlen(inName));
		if (e) {
			M4_LOG(GF_LOG_ERROR, ("Failed to send datagram: %s\n", gf_error_to_string(e) ));
			res = 1;
		}
	}
	gf_sk_del(sock);
	return res;
#else
	return 0;
#endif // GPAC_DISABLE_NETWORK
}

#ifndef GPAC_DISABLE_MPD
static u32 convert_mpd()
{
	GF_Err e;
	Bool remote = GF_FALSE;
	GF_MPD *mpd;
	char *mpd_base_url = NULL;
	if (!strnicmp(inName, "http://", 7) || !strnicmp(inName, "https://", 8)) {
#ifndef GPAC_DISABLE_NETWORK
		e = gf_dm_wget(inName, "tmp_main.m3u8", 0, 0, &mpd_base_url);
#else
		e = GF_NOT_SUPPORTED;
#endif
		if (e != GF_OK) {
			M4_LOG(GF_LOG_ERROR, ("Cannot retrieve M3U8 (%s): %s\n", inName, gf_error_to_string(e)));
			if (mpd_base_url) gf_free(mpd_base_url);
			return mp4box_cleanup(1);
		}
		remote = GF_TRUE;

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
		M4_LOG(GF_LOG_ERROR, ("[DASH] Error: MPD creation problem %s\n", gf_error_to_string(e)));
		mp4box_cleanup(1);
	}
	FILE *f = gf_fopen(remote ? "tmp_main.m3u8" : inName, "r");
	u32 manif_type = 0;
	if (f) {
		char szDATA[1000];
		s32 read;
		szDATA[999]=0;
		read = (s32) gf_fread(szDATA, 999, f);
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
		gf_file_delete("tmp_main.m3u8");
	}
	if (e != GF_OK) {
		M4_LOG(GF_LOG_ERROR, ("Error converting %s (%s) to MPD (%s): %s\n", (manif_type==1) ? "HLS" : "Smooth",  inName, outfile, gf_error_to_string(e)));
		return mp4box_cleanup(1);
	} else {
		M4_LOG(GF_LOG_INFO, ("Done converting %s (%s) to MPD (%s)\n", (manif_type==1) ? "HLS" : "Smooth",  inName, outfile));
		return mp4box_cleanup(0);
	}
}
#endif

static u32 do_import_sub()
{
	/* We import the subtitle file,
	   i.e. we parse it and store the content as samples of a 3GPP Timed Text track in an ISO file,
	   possibly for later export (e.g. when converting SRT to TTXT, ...) */
#ifndef GPAC_DISABLE_MEDIA_IMPORT
	GF_Err e;
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
		M4_LOG(GF_LOG_ERROR, ("Error importing %s: %s\n", inName, gf_error_to_string(e)));
		gf_isom_delete(file);
		gf_file_delete("ttxt_convert");
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
	gf_file_delete("ttxt_convert");
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error converting %s: %s\n", inName, gf_error_to_string(e)));
		return mp4box_cleanup(1);
	}
	return mp4box_cleanup(0);
#else
	M4_LOG(GF_LOG_ERROR, ("Feature not supported\n"));
	return mp4box_cleanup(1);
#endif
}

#if !defined(GPAC_DISABLE_MEDIA_IMPORT) && !defined(GPAC_DISABLE_ISOM_WRITE)
static u32 do_add_cat(int argc, char **argv)
{
	GF_Err e;
	u32 i, ipass, nb_pass = 1;
	char *mux_args=NULL;
	char *mux_sid=NULL;
	GF_FilterSession *fs = NULL;
	if (nb_add) {

		GF_ISOOpenMode open_mode = GF_ISOM_OPEN_EDIT;
		if (force_new) {
			open_mode = (do_flat || (force_new==2)) ? GF_ISOM_OPEN_WRITE : GF_ISOM_WRITE_EDIT;
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
		open_edit = do_flat ? GF_FALSE : GF_TRUE;
		file = gf_isom_open(inName, open_mode, NULL);
		if (!file) {
			M4_LOG(GF_LOG_ERROR, ("Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) ));
			return mp4box_cleanup(1);
		}

		if (freeze_box_order)
			gf_isom_freeze_order(file);

		if (do_flat) {
			if (major_brand)
				gf_isom_set_brand_info(file, major_brand, minor_version);
			for (i=0; i<nb_alt_brand_add; i++) {
				gf_isom_modify_alternate_brand(file, brand_add[i], GF_TRUE);
				do_save = GF_TRUE;
			}

			if (!major_brand && !nb_alt_brand_add && has_add_image) {
				gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_MIF1, GF_TRUE);
			}
		}
	}

#ifndef GPAC_DISABLE_ISOM_WRITE
	if (file && keep_utc && open_edit) {
		gf_isom_keep_utc_times(file, 1);
	}
#endif

	if (do_flat && interleaving_time) {
		char szSubArg[100];
		gf_isom_set_storage_mode(file, GF_ISOM_STORE_FASTSTART);
		do_flat = 2;
		nb_pass = 2;
		fs = gf_fs_new_defaults(0);
		if (!fs) {
			M4_LOG(GF_LOG_ERROR, ("Error creating filter session\n"));
			gf_isom_delete(file);
			return mp4box_cleanup(1);
		}

		//mux args
		gf_dynstrcat(&mux_args, "mp4mx:importer:store=fstart", ":");

		sprintf(szSubArg, "file=%p", file);
		gf_dynstrcat(&mux_args, szSubArg, ":");
		sprintf(szSubArg, "cdur=%g", interleaving_time);
		gf_dynstrcat(&mux_args, szSubArg, ":");
	}

	for (ipass=0; ipass<nb_pass; ipass++) {
		for (i=0; i<(u32) argc; i++) {
			char *margs=NULL;
			char *msid = NULL;
			if (!strcmp(argv[i], "-add")) {
				char *src = argv[i+1];

				while (src) {
					char *loc_src = src;
					char *sep = NULL;
					while (1) {
						char *opt_sep, *frag_sep;
						sep = strchr(loc_src, '+');
						if (!sep) break;

						sep[0] = 0;
						if (strstr(src, "://"))
							break;

						opt_sep = gf_url_colon_suffix(sep+1, '=');
						frag_sep = gf_url_colon_suffix(sep+1, '#');
						if (frag_sep && opt_sep && (frag_sep<opt_sep)) {
							opt_sep=NULL;
						}
						if (opt_sep) opt_sep[0] = 0;
						if (frag_sep) frag_sep[0] = 0;
						if (gf_file_exists(sep+1)) {
							if (opt_sep) opt_sep[0] = ':';
							if (frag_sep) frag_sep[0] = '#';
							break;
						}
						if (opt_sep) opt_sep[0] = ':';
						if (frag_sep) frag_sep[0] = '#';

						sep[0] = '+';
						loc_src = sep+1;
					}
					u32 tk_idx = gf_isom_get_track_count(file);
					//if existing tracks in file, set default index to next track
					if (tk_idx) tk_idx++;

					if (fs && (ipass==0)) {
						e = import_file(file, src, import_flags, import_fps, agg_samples, fs, &margs, &msid, tk_idx);
					} else {
						e = import_file(file, src, import_flags, import_fps, agg_samples, fs, NULL, NULL, tk_idx);
					}

					if (margs) {
						gf_dynstrcat(&mux_args, margs, ":");
						gf_free(margs);
					}
					if (msid) {
						gf_dynstrcat(&mux_sid, msid, ",");
						gf_free(msid);
					}

					if (e) {
						M4_LOG(GF_LOG_ERROR, ("Error importing %s: %s\n", argv[i+1], gf_error_to_string(e)));
						gf_isom_delete(file);
						if (fs)
							gf_fs_del(fs);
						return mp4box_cleanup(1);
					}
					if (sep) {
						sep[0] = '+';
						src = sep+1;
					} else {
						break;
					}
				}
				i++;
			} else if (!strcmp(argv[i], "-cat") || !strcmp(argv[i], "-catx") || !strcmp(argv[i], "-catpl")) {
				if (nb_pass == 2) {
					M4_LOG(GF_LOG_ERROR, ("Cannot cat files when using -newfs mode\n"));
					return mp4box_cleanup(1);
				}
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
					file = gf_isom_open(inName, open_mode, NULL);
					if (!file) {
						M4_LOG(GF_LOG_ERROR, ("Cannot open destination file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL)) ));
						return mp4box_cleanup(1);
					}
				}

				e = cat_isomedia_file(file, argv[i+1], import_flags, import_fps, agg_samples, force_cat, align_cat, !strcmp(argv[i], "-catx") ? GF_TRUE : GF_FALSE, !strcmp(argv[i], "-catpl") ? GF_TRUE : GF_FALSE);
				if (e) {
					M4_LOG(GF_LOG_ERROR, ("Error appending %s: %s\n", argv[i+1], gf_error_to_string(e)));
					gf_isom_delete(file);
					return mp4box_cleanup(1);
				}
				i++;
			}
		}
		if ((nb_pass == 2) && !ipass) {
			if (mux_sid) {
				gf_dynstrcat(&mux_args, "SID=", ":");
				gf_dynstrcat(&mux_args, mux_sid, NULL);
			}
			GF_Filter *mux_filter = gf_fs_load_filter(fs, mux_args, NULL);
			gf_free(mux_args);
			if (!mux_filter) {
				M4_LOG(GF_LOG_ERROR, ("Error loadin isobmff mux filter\n"));
				gf_isom_delete(file);
				gf_fs_del(fs);
				return mp4box_cleanup(1);
			}

			e = gf_fs_run(fs);
			if (e==GF_EOS) e = GF_OK;

			if (!e) e = gf_fs_get_last_connect_error(fs);
			if (!e) e = gf_fs_get_last_process_error(fs);

			if (e) {
				M4_LOG(GF_LOG_ERROR, ("Error importing sources: %s\n", gf_error_to_string(e)));
				gf_isom_delete(file);
				gf_fs_del(fs);
				return mp4box_cleanup(1);
			}
		}
	}
	if (fs) {
		gf_fs_print_non_connected(fs);
		if (fs_dump_flags & 1) gf_fs_print_stats(fs);
		if (fs_dump_flags & 2) gf_fs_print_connections(fs);
		gf_fs_del(fs);
	}

	/*unless explicitly asked, remove all systems tracks*/
#ifndef GPAC_DISABLE_AV_PARSERS
	if (!keep_sys_tracks) remove_systems_tracks(file);
#endif
	do_save = GF_TRUE;

	return 0;
}
#endif /*!GPAC_DISABLE_MEDIA_IMPORT && !GPAC_DISABLE_ISOM_WRITE*/


#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
static GF_Err do_scene_encode()
{
	GF_Err e;
	FILE *logs = NULL;
	if (do_scene_log) {
		char alogfile[GF_MAX_PATH];
		strcpy(alogfile, inName);
		if (strchr(alogfile, '.')) {
			while (alogfile[strlen(alogfile)-1] != '.') alogfile[strlen(alogfile)-1] = 0;
			alogfile[strlen(alogfile)-1] = 0;
		}
		strcat(alogfile, "_enc.logs");
		logs = gf_fopen(alogfile, "wt");
	}
	strcpy(outfile, outName ? outName : inName);
	if (strchr(outfile, '.')) {
		while (outfile[strlen(outfile)-1] != '.') outfile[strlen(outfile)-1] = 0;
		outfile[strlen(outfile)-1] = 0;
	}
	strcat(outfile, ".mp4");
	file = gf_isom_open(outfile, GF_ISOM_WRITE_EDIT, NULL);
	smenc_opts.mediaSource = mediaSource ? mediaSource : outfile;
	e = EncodeFile(inName, file, &smenc_opts, logs);
	if (logs) gf_fclose(logs);
	if (e) return e;
	do_save = GF_TRUE;
	if (do_saf) {
		do_save = GF_FALSE;
		open_edit = GF_FALSE;
	}
	return GF_OK;
}
#endif //!defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)


static GF_Err do_dash()
{
	GF_Err e;
	u32 i;
	Bool del_file = GF_FALSE;
	char szMPD[GF_MAX_PATH], *sep;
	char szStateFile[GF_MAX_PATH];
	Bool dyn_state_file = GF_FALSE;
	u32 do_abort = 0;
	GF_DASHSegmenter *dasher=NULL;

	if (crypt) {
		M4_LOG(GF_LOG_ERROR, ("MP4Box cannot use -crypt and -dash in the same pass. Please encrypt your content first, or specify encryption filters on dash sources.\n"));
		return GF_BAD_PARAM;
	}

	strcpy(outfile, outName ? outName : gf_url_get_resource_name(inName) );
	sep = strrchr(outfile, '.');
	if (sep) sep[0] = 0;
	if (!outName) strcat(outfile, "_dash");
	strcpy(szMPD, outfile);
	if (outName && sep) {
		sep[0] = '.';
		strcat(szMPD, sep);
	} else {
		strcat(szMPD, ".mpd");
	}

	if ((dash_subduration>0) && (dash_duration > dash_subduration)) {
		M4_LOG(GF_LOG_WARNING, ("Warning: -subdur parameter (%g s) should be greater than segment duration (%g s), using segment duration instead\n", dash_subduration, dash_duration));
		dash_subduration = dash_duration;
	}

	if (dash_mode && dash_live)
		M4_LOG(GF_LOG_INFO, ("Live DASH-ing - press 'q' to quit, 's' to save context and quit\n"));

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
			gf_file_delete(dash_ctx_file);
	}

	if (dash_profile==GF_DASH_PROFILE_AUTO)
		dash_profile = dash_mode ? GF_DASH_PROFILE_LIVE : GF_DASH_PROFILE_FULL;

	if (!dash_mode) {
		time_shift_depth = 0;
		mpd_update_time = 0;
	} else if ((dash_profile>=GF_DASH_PROFILE_MAIN) && !use_url_template && !mpd_update_time) {
		/*use a default MPD update of dash_duration sec*/
		mpd_update_time = (Double) (dash_subduration ? dash_subduration : dash_duration);
		M4_LOG(GF_LOG_INFO, ("Using default MPD refresh of %g seconds\n", mpd_update_time));
	}

	if (file && do_save) {
		gf_isom_close(file);
		file = NULL;
		del_file = GF_TRUE;
	}

	/*setup dash*/
	dasher = gf_dasher_new(szMPD, dash_profile, NULL, dash_scale, dash_ctx_file);
	if (!dasher) {
		return mp4box_cleanup(1);
	}
	e = gf_dasher_set_info(dasher, dash_title, cprt, dash_more_info, dash_source, NULL);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("DASH Error: %s\n", gf_error_to_string(e)));
		gf_dasher_del(dasher);
		return e;
	}

	gf_dasher_set_start_date(dasher, dash_start_date);
	gf_dasher_set_location(dasher, dash_source);
	for (i=0; i < nb_mpd_base_urls; i++) {
		e = gf_dasher_add_base_url(dasher, mpd_base_urls[i]);
		if (e) {
			M4_LOG(GF_LOG_ERROR, ("DASH Error: %s\n", gf_error_to_string(e)));
			gf_dasher_del(dasher);
			return e;
		}
	}

	if (segment_timeline && !use_url_template) {
		M4_LOG(GF_LOG_WARNING, ("DASH Warning: using -segment-timeline with no -url-template. Forcing URL template.\n"));
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
	if (!e) e = gf_dasher_configure_isobmf_default(dasher, no_fragments_defaults, pssh_mode, samplegroups_in_traf, single_traf_per_moof, tfdt_per_traf, mvex_after_traks, sdtp_in_traf);
	if (!e) e = gf_dasher_enable_utc_ref(dasher, insert_utc);
	if (!e) e = gf_dasher_enable_real_time(dasher, frag_real_time);
	if (!e) e = gf_dasher_set_content_protection_location_mode(dasher, cp_location_mode);
	if (!e) e = gf_dasher_set_profile_extension(dasher, dash_profile_extension);
	if (!e) e = gf_dasher_enable_cached_inputs(dasher, no_cache);
	if (!e) e = gf_dasher_enable_loop_inputs(dasher, ! no_loop);
	if (!e) e = gf_dasher_set_split_mode(dasher, dash_split_mode);
	if (!e) e = gf_dasher_set_last_segment_merge(dasher, merge_last_seg);
	if (!e) e = gf_dasher_set_hls_clock(dasher, hls_clock);
	if (!e && dash_cues) e = gf_dasher_set_cues(dasher, dash_cues, strict_cues);
	if (!e) e = gf_dasher_print_session_info(dasher, fs_dump_flags);
	if (!e)  e = gf_dasher_keep_source_utc(dasher, keep_utc);

	for (i=0; i < nb_dash_inputs; i++) {
		if (!e) e = gf_dasher_add_input(dasher, &dash_inputs[i]);
	}
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("DASH Setup Error: %s\n", gf_error_to_string(e)));
		gf_dasher_del(dasher);
		return e;
	}

	dash_cumulated_time=0;

	while (1) {
		if (run_for && (dash_cumulated_time >= run_for)) {
			M4_LOG(GF_LOG_INFO, ("Done running, computing static MPD\n"));
			do_abort = 3;
		}

		dash_prev_time=gf_sys_clock();
		if (do_abort>=2) {
			e = gf_dasher_set_dynamic_mode(dasher, GF_DASH_DYNAMIC_LAST, 0, time_shift_depth, mpd_live_duration);
		}

		if (!e) e = gf_dasher_process(dasher);
		if (!dash_live && (e==GF_EOS) ) {
			M4_LOG(GF_LOG_INFO, ("Nothing to dash, too early ...\n"));
			e = GF_OK;
		}

		if (do_abort)
			break;

		//this happens when reading file while writing them (local playback of the live session ...)
		if (dash_live && (e==GF_IO_ERR) ) {
			M4_LOG(GF_LOG_WARNING, ("Error dashing file (%s) but continuing ...\n", gf_error_to_string(e) ));
			e = GF_OK;
		}

		if (e) break;

		if (dash_live) {
			u64 ms_in_session=0;
			u32 slept = gf_sys_clock();
			u32 sleep_for = gf_dasher_next_update_time(dasher, &ms_in_session);
			M4_LOG(GF_LOG_INFO, ("Next generation scheduled in %u ms (DASH time "LLU" ms)\r", sleep_for, ms_in_session));
			if (run_for && (ms_in_session>=run_for)) {
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
					M4_LOG(GF_LOG_INFO, ("Slept for %d ms before generation, dash cumulated time %d\n", dash_now_time - slept, dash_cumulated_time));
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
		M4_LOG(GF_LOG_INFO, ("Enter file name to save dash context:\n"));
		if (scanf("%1023s", szName) == 1) {
			gf_file_move(dash_ctx_file, szName);
		}
	}
	if (e) M4_LOG(GF_LOG_ERROR, ("Error DASHing file: %s\n", gf_error_to_string(e)));
	if (file) gf_isom_delete(file);
	if (del_file)
		gf_file_delete(inName);

	return e;
}

static GF_Err do_export_tracks_non_isobmf()
{
#ifndef GPAC_DISABLE_MEDIA_EXPORT
	u32 i;

	GF_MediaExporter mdump;
	char szFile[GF_MAX_PATH+24];
	for (i=0; i<nb_track_act; i++) {
		GF_Err e;
		TrackAction *tka = &tracks[i];
		if (tka->act_type != TRACK_ACTION_RAW_EXTRACT) continue;
		memset(&mdump, 0, sizeof(mdump));
		mdump.in_name = inName;
		mdump.flags = tka->dump_type;
		mdump.trackID = tka->target_track.ID_or_num;
		if (tka->target_track.type>1)
			mdump.track_type = tka->target_track.type-1;
		mdump.sample_num = tka->sample_num;

		if (dump_std) {
			mdump.out_name = "std";
		}
		else if (outName) {
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
		if (e) return e;
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}


static GF_Err do_dump_iod()
{
	GF_Err e = GF_OK;
	GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *)gf_isom_get_root_od(file);
	if (!iod) {
		M4_LOG(GF_LOG_WARNING, ("File %s has no IOD\n", inName));
	} else {
		char szName[GF_MAX_PATH+10];
		FILE *iodf;
		sprintf(szName, "%s.iod", outfile);
		iodf = gf_fopen(szName, "wb");
		if (!iodf) {
			M4_LOG(GF_LOG_ERROR, ("Cannot open destination %s\n", szName));
			e = GF_IO_ERR;
		} else {
			u8 *desc;
			u32 size;
			GF_BitStream *bs = gf_bs_from_file(iodf, GF_BITSTREAM_WRITE);
			e = gf_odf_desc_write((GF_Descriptor *)iod, &desc, &size);
			if (e==GF_OK) {
				if (gf_fwrite(desc, size, iodf)!=size) e = GF_IO_ERR;
				gf_free(desc);
			} else {
				e = GF_IO_ERR;
			}
			if (e) {
				M4_LOG(GF_LOG_ERROR, ("Error writing IOD %s\n", szName));
			}
			gf_bs_del(bs);
			gf_fclose(iodf);
		}
		gf_odf_desc_del((GF_Descriptor*)iod);
	}
	return e;
}

static u32 get_track_id(GF_ISOFile *file, TrackIdentifier *tkid)
{
	u32 cur_tk=0, i, count = gf_isom_get_track_count(file);
	if (tkid->type==1) {
		if (!tkid->ID_or_num || (count < tkid->ID_or_num)) return 0;
		return gf_isom_get_track_id(file, tkid->ID_or_num);
	}
	if (tkid->type==0) {
		return tkid->ID_or_num;
	}
	for (i=0; i<count; i++) {
		u32 mtype = gf_isom_get_media_type(file, i+1);
		if (tkid->type==2) {
			if (!gf_isom_is_video_handler_type(mtype)) continue;
		}
		else if (tkid->type==3) {
			if (mtype != GF_ISOM_MEDIA_AUDIO) continue;
		} else if (tkid->type==4) {
			if ((mtype != GF_ISOM_MEDIA_TEXT) && (mtype != GF_ISOM_MEDIA_SUBT)) continue;
		} else {
			continue;
		}
		cur_tk++;
		if (tkid->ID_or_num && (cur_tk != tkid->ID_or_num)) continue;
		return gf_isom_get_track_id(file, i+1);
	}
	return 0;
}

static GF_Err do_export_tracks()
{
#ifndef GPAC_DISABLE_MEDIA_EXPORT
	GF_Err e;
	u32 i;
	char szFile[GF_MAX_PATH+24];
	GF_MediaExporter mdump;
	for (i=0; i<nb_track_act; i++) {
		u32 j;
		TrackAction *tka = &tracks[i];
		if (tka->act_type != TRACK_ACTION_RAW_EXTRACT) continue;
		memset(&mdump, 0, sizeof(mdump));
		mdump.file = file;
		mdump.flags = tka->dump_type;
		mdump.sample_num = tka->sample_num;
		//this can be 0
		mdump.trackID = get_track_id(file, &tka->target_track);

		if (tka->out_name) {
			mdump.out_name = tka->out_name;
		} else if (outName) {
			mdump.out_name = outName;
			mdump.flags |= GF_EXPORT_MERGE;
			/*don't infer extension on user-given filename*/
			mdump.flags |= GF_EXPORT_NO_FILE_EXT;
		} else if (mdump.trackID) {
			sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
			mdump.out_name = szFile;
		} else {
			sprintf(szFile, "%s_export", outfile);
			mdump.out_name = szFile;
		}
		if (tka->target_track.ID_or_num==(u32) -1) {
			for (j=0; j<gf_isom_get_track_count(file); j++) {
				mdump.trackID = gf_isom_get_track_id(file, j+1);
				sprintf(szFile, "%s_track%d", outfile, mdump.trackID);
				mdump.out_name = szFile;
				mdump.print_stats_graph = fs_dump_flags;
				e = gf_media_export(&mdump);
				if (e) return e;
			}
		} else {
			mdump.print_stats_graph = fs_dump_flags;
			e = gf_media_export(&mdump);
			if (e) return e;
		}
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

static GF_Err do_meta_act()
{
	u32 i;
	for (i=0; i<nb_meta_act; i++) {
		GF_Err e = GF_OK;
		u32 tk = 0;
#ifndef GPAC_DISABLE_ISOM_WRITE
		Bool self_ref;
#endif
		MetaAction *meta = &metas[i];
		u32 tk_id = get_track_id(file, &meta->track_id);

		if (tk_id) tk = gf_isom_get_track_by_id(file, tk_id);

		switch (meta->act_type) {
#ifndef GPAC_DISABLE_ISOM_WRITE
		case META_ACTION_SET_TYPE:
			/*note: we don't handle file brand modification, this is an author stuff and cannot be guessed from meta type*/
			e = gf_isom_set_meta_type(file, meta->root_meta, tk, meta->meta_4cc);
			gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_ISO2, GF_TRUE);
			do_save = GF_TRUE;
			break;
		case META_ACTION_ADD_ITEM:

			if (meta->replace) {
				if (!gf_isom_get_meta_item_by_id(file, meta->root_meta, tk, meta->item_id)) {
					M4_LOG(GF_LOG_WARNING, ("No item with ID %d, ignoring replace\n", meta->item_id));
					meta->item_id = 0;
				} else {
					e = gf_isom_remove_meta_item(file, meta->root_meta, tk, meta->item_id, GF_TRUE, meta->keep_props);
					if (e) break;
				}
			}

			self_ref = !stricmp(meta->szPath, "NULL") || !stricmp(meta->szPath, "this") || !stricmp(meta->szPath, "self");
			e = gf_isom_add_meta_item(file, meta->root_meta, tk, self_ref, self_ref ? NULL : meta->szPath,
			                          meta->szName,
			                          meta->item_id,
									  meta->item_type,
			                          meta->mime_type,
			                          meta->enc_type,
			                          meta->use_dref ? meta->szPath : NULL,  NULL,
			                          meta->image_props);
			if (meta->item_refs && gf_list_count(meta->item_refs)) {
				u32 ref_i;
				for (ref_i = 0; ref_i < gf_list_count(meta->item_refs); ref_i++) {
					MetaRef	*ref_entry = gf_list_get(meta->item_refs, ref_i);
					e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, ref_entry->ref_item_id, ref_entry->ref_type, NULL);
				}
			}
			do_save = GF_TRUE;
			break;
		case META_ACTION_ADD_IMAGE_ITEM:
		{
			u32 old_tk_count = gf_isom_get_track_count(file);
			u32 src_tk_id = 1;
			GF_Fraction _frac = {0,0};
			GF_ISOFile *fsrc = file;
			self_ref = GF_FALSE;

			tk = 0;
			if (meta->image_props && meta->image_props->auto_grid) {
				e = GF_OK;
				self_ref = GF_TRUE;
			} else if (!meta->szPath || (meta->image_props && meta->image_props->sample_num && meta->image_props->use_reference)) {
				e = GF_OK;
				self_ref = GF_TRUE;
				src_tk_id = tk_id;
			} else if (meta->szPath) {
				if (meta->image_props && gf_isom_probe_file(meta->szPath) && !meta->image_props->tile_mode) {
					meta->image_props->src_file = gf_isom_open(meta->szPath, GF_ISOM_OPEN_READ, NULL);
					e = gf_isom_last_error(meta->image_props->src_file);
					fsrc = meta->image_props->src_file;
					if (meta->image_props->item_ref_id)
						src_tk_id = 0;
				} else {
					gf_isom_disable_brand_rewrite(file, GF_TRUE);
					e = import_file(file, meta->szPath, 0, _frac, 0, NULL, NULL, NULL, 0);
					gf_isom_disable_brand_rewrite(file, GF_FALSE);
				}
			} else {
				M4_LOG(GF_LOG_ERROR, ("Missing file name to import\n"));
				e = GF_BAD_PARAM;
			}
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
						if (!src_tk_id && (!meta->image_props || !meta->image_props->item_ref_id) ) {
							u32 j;
							for (j=0; j<gf_isom_get_track_count(fsrc); j++) {
								if (gf_isom_is_video_handler_type (gf_isom_get_media_type(fsrc, j+1))) {
									src_tk_id = gf_isom_get_track_id(fsrc, j+1);
									break;
								}
							}

							if (!src_tk_id) {
								M4_LOG(GF_LOG_ERROR, ("No video track in file, cannot add image from track\n"));
								e = GF_BAD_PARAM;
							}
						}

						if (e == GF_OK && meta->replace) {
							if (!gf_isom_get_meta_item_by_id(file, meta->root_meta, tk, meta->item_id)) {
								M4_LOG(GF_LOG_WARNING, ("No item with ID %d, ignoring replace\n", meta->item_id));
								meta->item_id = 0;
							} else {
								e = gf_isom_remove_meta_item(file, meta->root_meta, tk, meta->item_id, GF_TRUE, meta->keep_props);
							}
						}
						if (e==GF_OK)
							e = gf_isom_iff_create_image_item_from_track(file, meta->root_meta, tk, src_tk_id, meta->szName, meta->item_id, meta->image_props, NULL);

						if (e == GF_OK && meta->primary) {
							e = gf_isom_set_meta_primary_item(file, meta->root_meta, tk, meta->item_id);
						}
						if (e == GF_OK && meta->item_refs && gf_list_count(meta->item_refs)) {
							u32 ref_i;
							for (ref_i = 0; ref_i < gf_list_count(meta->item_refs); ref_i++) {
								MetaRef	*ref_entry = gf_list_get(meta->item_refs, ref_i);
								e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, ref_entry->ref_item_id, ref_entry->ref_type, NULL);
							}
						}
						if (e == GF_OK && meta->group_type) {
							e = gf_isom_meta_add_item_group(file, meta->root_meta, tk, meta->item_id, meta->group_id, meta->group_type);
						}
					}
				}
			}
			if (meta->image_props && meta->image_props->src_file) {
				gf_isom_delete(meta->image_props->src_file);
				meta->image_props->src_file = NULL;
			} else if (!self_ref) {
				gf_isom_remove_track(file, old_tk_count+1);
				if (do_flat) {
					M4_LOG(GF_LOG_ERROR, ("Warning: -flat storage cannot be used when using -add-image on external file\n"));
					e = GF_NOT_SUPPORTED;
				}
			}
			do_save = GF_TRUE;
		}
			break;
		case META_ACTION_ADD_IMAGE_DERIVED:
		{
			u32 meta_type = gf_isom_get_meta_type(file, meta->root_meta, tk);
			e = GF_OK;
			if (!meta_type) {
				e = gf_isom_set_meta_type(file, meta->root_meta, tk, GF_META_ITEM_TYPE_PICT);
			} else {
				if (meta_type != GF_META_ITEM_TYPE_PICT) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Warning: file already has a root 'meta' box of type %s\n", gf_4cc_to_str(meta_type)));
					e = GF_BAD_PARAM;
				}
			}
			if (e) break;
			if (!meta->item_id) {
				e = gf_isom_meta_get_next_item_id(file, meta->root_meta, tk, &meta->item_id);
			}
			if (e == GF_OK && meta->item_type) {
				if (meta->item_type == GF_4CC('g','r','i','d')) {
					e = gf_isom_iff_create_image_grid_item(file, meta->root_meta, tk,
							meta->szName && strlen(meta->szName) ? meta->szName : NULL,
							meta->item_id,
							meta->image_props);
				} else if (meta->item_type == GF_4CC('i','o','v','l')) {
					e = gf_isom_iff_create_image_overlay_item(file, meta->root_meta, tk,
							meta->szName && strlen(meta->szName) ? meta->szName : NULL,
							meta->item_id,
							meta->image_props);
				} else if (meta->item_type == GF_4CC('i','d','e','n')) {
					e = gf_isom_iff_create_image_identity_item(file, meta->root_meta, tk,
							meta->szName && strlen(meta->szName) ? meta->szName : NULL,
							meta->item_id,
							meta->image_props);
				} else {
					e = GF_NOT_SUPPORTED;
				}
				if (e) break;
				if (meta->primary) {
					e = gf_isom_set_meta_primary_item(file, meta->root_meta, tk, meta->item_id);
					if (e) break;
				}
				if (meta->item_refs && gf_list_count(meta->item_refs)) {
					u32 ref_i;
					for (ref_i = 0; ref_i < gf_list_count(meta->item_refs); ref_i++) {
						MetaRef	*ref_entry = gf_list_get(meta->item_refs, ref_i);
						e = gf_isom_meta_add_item_ref(file, meta->root_meta, tk, meta->item_id, ref_entry->ref_item_id, ref_entry->ref_type, NULL);
						if (e) break;
					}
				}
			}
			do_save = GF_TRUE;
		}
			break;
		case META_ACTION_REM_ITEM:
			e = gf_isom_remove_meta_item(file, meta->root_meta, tk, meta->item_id, GF_FALSE, NULL);
			do_save = GF_TRUE;
			break;
		case META_ACTION_SET_PRIMARY_ITEM:
			e = gf_isom_set_meta_primary_item(file, meta->root_meta, tk, meta->item_id);
			do_save = GF_TRUE;
			break;
		case META_ACTION_SET_XML:
		case META_ACTION_SET_BINARY_XML:
			e = gf_isom_set_meta_xml(file, meta->root_meta, tk, meta->szPath, NULL, 0, (meta->act_type==META_ACTION_SET_BINARY_XML) ? 1 : 0);
			do_save = GF_TRUE;
			break;
		case META_ACTION_REM_XML:
			if (gf_isom_get_meta_item_count(file, meta->root_meta, tk)) {
				e = gf_isom_remove_meta_xml(file, meta->root_meta, tk);
				do_save = GF_TRUE;
			} else {
				M4_LOG(GF_LOG_WARNING, ("No meta box in input file\n"));
				e = GF_OK;
			}
			break;
		case META_ACTION_DUMP_ITEM:
			if (gf_isom_get_meta_item_count(file, meta->root_meta, tk)) {
				e = gf_isom_extract_meta_item(file, meta->root_meta, tk, meta->item_id, meta->szPath && strlen(meta->szPath) ? meta->szPath : NULL);
			} else {
				M4_LOG(GF_LOG_WARNING, ("No meta box in input file\n"));
				e = GF_OK;
			}
			break;
#endif // GPAC_DISABLE_ISOM_WRITE

		case META_ACTION_DUMP_XML:
			if (gf_isom_has_meta_xml(file, meta->root_meta, tk)) {
				e = gf_isom_extract_meta_xml(file, meta->root_meta, tk, meta->szPath, NULL);
			} else {
				M4_LOG(GF_LOG_WARNING, ("No meta box in input file\n"));
				e = GF_OK;
			}
			break;
		default:
			break;
		}
		if (meta->item_refs) {
			while (gf_list_count(meta->item_refs)) {
				gf_free(gf_list_pop_back(meta->item_refs));
			}
			gf_list_del(meta->item_refs);
			meta->item_refs = NULL;
		}
		// meta->image_props is freed in mp4box_cleanup
		if (e) return e;
	}
	return GF_OK;
}

static GF_Err do_tsel_act()
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 i;
	GF_Err e;
	for (i=0; i<nb_tsel_acts; i++) {
		u32 refID, trackID = get_track_id(file, &tsel_acts[i].target_track);
		if (!trackID) {
			M4_LOG(GF_LOG_ERROR, ("Track not found\n"));
			e = GF_BAD_PARAM;
			return e;
		}

		switch (tsel_acts[i].act_type) {
		case TSEL_ACTION_SET_PARAM:
			refID = get_track_id(file, &tsel_acts[i].reference_track);
			e = gf_isom_set_track_switch_parameter(file,
			                                       gf_isom_get_track_by_id(file, trackID),
			                                       refID,
			                                       tsel_acts[i].is_switchGroup ? 1 : 0,
			                                       &tsel_acts[i].switchGroupID,
			                                       tsel_acts[i].criteria, tsel_acts[i].nb_criteria);
			if (e == GF_BAD_PARAM) {
				u32 alternateGroupID, nb_groups;
				gf_isom_get_track_switch_group_count(file, gf_isom_get_track_by_id(file, trackID), &alternateGroupID, &nb_groups);
				if (alternateGroupID) {
					M4_LOG(GF_LOG_ERROR, ("Error - for adding more tracks to group, using: -group-add -refTrack=ID1:[criteria:]trackID=ID2\n"));
				} else {
					M4_LOG(GF_LOG_ERROR, ("Error - for creating a new grouping information, using -group-add -trackID=ID1:[criteria:]trackID=ID2\n"));
				}
			}
			if (e) return e;
			do_save = GF_TRUE;
			break;
		case TSEL_ACTION_REMOVE_TSEL:
			e = gf_isom_reset_track_switch_parameter(file, gf_isom_get_track_by_id(file, trackID), 0);
			if (e) return e;
			do_save = GF_TRUE;
			break;
		case TSEL_ACTION_REMOVE_ALL_TSEL_IN_GROUP:
			e = gf_isom_reset_track_switch_parameter(file, gf_isom_get_track_by_id(file, trackID), 1);
			if (e) return e;
			do_save = GF_TRUE;
			break;
		default:
			break;
		}
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

static void do_ipod_conv()
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 i, ipod_major_brand = 0;
	M4_LOG(GF_LOG_INFO, ("Setting up iTunes/iPod file\n"));

	for (i=0; i<gf_isom_get_track_count(file); i++) {
		u32 mType = gf_isom_get_media_type(file, i+1);
		switch (mType) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
			ipod_major_brand = GF_ISOM_BRAND_M4V;
			gf_isom_set_ipod_compatible(file, i+1);
			break;
		case GF_ISOM_MEDIA_AUDIO:
			if (!ipod_major_brand) ipod_major_brand = GF_ISOM_BRAND_M4A;
			else gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_M4A, GF_TRUE);
			break;
		case GF_ISOM_MEDIA_TEXT:
			/*this is a text track track*/
			if (gf_isom_get_media_subtype(file, i+1, 1) == GF_ISOM_SUBTYPE_TX3G) {
				Bool is_chap = 0;
				u32 j;
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
	gf_isom_set_brand_info(file, ipod_major_brand, 1);
	gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_MP42, GF_TRUE);
	do_save = GF_TRUE;
#endif
}

static GF_Err do_track_act()
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 j;
	for (j=0; j<nb_track_act; j++) {
		u32 i;
		GF_Err e = GF_OK;
		TrackAction *tka = &tracks[j];
		u32 trackID = get_track_id(file, &tka->target_track);
		u32 track = trackID ? gf_isom_get_track_by_id(file, trackID) : 0;

		u32 newTrackID = get_track_id(file, &tka->newTrackID);

		timescale = gf_isom_get_timescale(file);
		switch (tka->act_type) {
		case TRACK_ACTION_REM_TRACK:
			e = gf_isom_remove_track(file, track);
			if (e) {
				M4_LOG(GF_LOG_ERROR, ("Error Removing track ID %d: %s\n", trackID, gf_error_to_string(e)));
			} else {
				M4_LOG(GF_LOG_INFO, ("Removing track ID %d\n", trackID));
			}
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_LANGUAGE:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_set_media_language(file, i+1, tka->lang);
				if (e) return e;
				do_save = GF_TRUE;
			}
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_KIND:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_add_track_kind(file, i+1, tka->kind_scheme, tka->kind_value);
				if (e) return e;
				do_save = GF_TRUE;
			}
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_REM_KIND:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_remove_track_kind(file, i+1, tka->kind_scheme, tka->kind_value);
				if (e) return e;
				do_save = GF_TRUE;
			}
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_DELAY:
			if (tka->delay.num && tka->delay.den) {
				u64 tk_dur;

				e = gf_isom_remove_edits(file, track);
				if (e) return e;
				tk_dur = gf_isom_get_track_duration(file, track);
				if (gf_isom_get_edits_count(file, track))
					do_save = GF_TRUE;
				if (tka->delay.num>0) {
					//cast to u64, delay_ms * timescale can be quite big before / 1000
					e = gf_isom_append_edit(file, track, ((u64) tka->delay.num) * timescale / tka->delay.den, 0, GF_ISOM_EDIT_EMPTY);
					if (e) return e;
					e = gf_isom_append_edit(file, track, tk_dur, 0, GF_ISOM_EDIT_NORMAL);
					if (e) return e;
					do_save = GF_TRUE;
				} else {
					//cast to u64, delay_ms * timescale can be quite big before / 1000
					u64 to_skip = ((u64) -tka->delay.num) * timescale / tka->delay.den;
					if (to_skip<tk_dur) {
						//cast to u64, delay_ms * timescale can be quite big before / 1000
						u64 media_time = ((u64) -tka->delay.num) * gf_isom_get_media_timescale(file, track) / tka->delay.den;
						e = gf_isom_append_edit(file, track, tk_dur-to_skip, media_time, GF_ISOM_EDIT_NORMAL);
						if (e) return e;
						do_save = GF_TRUE;
					} else {
						M4_LOG(GF_LOG_WARNING, ("Warning: request negative delay longer than track duration - ignoring\n"));
					}
				}
			} else if (gf_isom_get_edits_count(file, track)) {
				e = gf_isom_remove_edits(file, track);
				if (e) return e;
				do_save = GF_TRUE;
			}
			break;
		case TRACK_ACTION_SET_KMS_URI:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				if (!gf_isom_is_media_encrypted(file, i+1, 1)) continue;
				if (!gf_isom_is_ismacryp_media(file, i+1, 1)) continue;
				e = gf_isom_change_ismacryp_protection(file, i+1, 1, NULL, (char *) tka->kms);
				if (e) return e;
				do_save = GF_TRUE;
			}
			break;
		case TRACK_ACTION_SET_ID:
			if (!trackID && (gf_isom_get_track_count(file) == 1)) {
				M4_LOG(GF_LOG_WARNING, ("Warning: track id is not specified, but file has only one track - assume that you want to change id for this track\n"));
				track = 1;
			}
			if (track) {
				u32 newTrack;
				newTrack = gf_isom_get_track_by_id(file, newTrackID);
				if (newTrack != 0) {
					M4_LOG(GF_LOG_WARNING, ("Cannot set track id with value %d because a track already exists - ignoring", tka->newTrackID));
				} else {
					e = gf_isom_set_track_id(file, track, newTrackID);
					if (e) return e;
					do_save = GF_TRUE;
				}
			} else {
				M4_LOG(GF_LOG_WARNING, ("Error: Cannot change id for track %d because it does not exist - ignoring", trackID));
			}
			break;
		case TRACK_ACTION_SWAP_ID:
			if (track) {
				u32 tk1, tk2;
				tk1 = gf_isom_get_track_by_id(file, trackID);
				tk2 = gf_isom_get_track_by_id(file, newTrackID);
				if (!tk1 || !tk2) {
					M4_LOG(GF_LOG_WARNING, ("Error: Cannot swap track IDs because not existing - ignoring"));
				} else {
					e = gf_isom_set_track_id(file, tk2, 0);
					if (e) return e;
					e = gf_isom_set_track_id(file, tk1, newTrackID);
					if (e) return e;
					e = gf_isom_set_track_id(file, tk2, trackID);
					if (e) return e;
					do_save = GF_TRUE;
				}
			} else {
				M4_LOG(GF_LOG_WARNING, ("Error: Cannot change id for track %d because it does not exist - ignoring", trackID));
			}
			break;
		case TRACK_ACTION_SET_PAR:
			e = gf_media_change_par(file, track, tka->par_num, tka->par_den, tka->force_par, tka->rewrite_bs);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_CLAP:
			e = gf_isom_set_clean_aperture(file, track, 1, tka->clap_wnum, tka->clap_wden, tka->clap_hnum, tka->clap_hden, tka->clap_honum, tka->clap_hoden, tka->clap_vonum, tka->clap_voden);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_MX:
			e = gf_isom_set_track_matrix(file, track, tka->mx);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_HANDLER_NAME:
			e = gf_isom_set_handler_name(file, track, tka->hdl_name);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_ENABLE:
			if (!gf_isom_is_track_enabled(file, track)) {
				e = gf_isom_set_track_enabled(file, track, GF_TRUE);
				do_save = GF_TRUE;
			}
			break;
		case TRACK_ACTION_DISABLE:
			if (gf_isom_is_track_enabled(file, track)) {
				e = gf_isom_set_track_enabled(file, track, GF_FALSE);
				do_save = GF_TRUE;
			}
			break;
		case TRACK_ACTION_REFERENCE:
			if (newTrackID)
				e = gf_isom_set_track_reference(file, track, GF_4CC(tka->lang[0], tka->lang[1], tka->lang[2], tka->lang[3]), newTrackID);
			else
				e = gf_isom_remove_track_reference(file, track, GF_4CC(tka->lang[0], tka->lang[1], tka->lang[2], tka->lang[3]) );
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_REM_NON_RAP:
			e = gf_media_remove_non_rap(file, track, GF_FALSE);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_REM_NON_REFS:
			e = gf_media_remove_non_rap(file, track, GF_TRUE);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_UDTA:
			e = set_file_udta(file, track, tka->udta_type, tka->string ? tka->string : tka->src_name , tka->sample_num ? GF_TRUE : GF_FALSE, tka->string ? GF_TRUE : GF_FALSE);
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_EDITS:
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			e = apply_edits(file, track, tka->string);
#else
			e = GF_NOT_SUPPORTED;
#endif
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_TIME:
			if (!trackID) {
				e = gf_isom_set_creation_time(file, tka->time, tka->time);
				if (e) return e;
				for (i=0; i<gf_isom_get_track_count(file); i++) {
					e = gf_isom_set_track_creation_time(file, i+1, tka->time, tka->time);
					if (e) return e;
				}
			} else {
				e = gf_isom_set_track_creation_time(file, track, tka->time, tka->time);
			}
			do_save = GF_TRUE;
			break;
		case TRACK_ACTION_SET_MEDIA_TIME:
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				if (track && (track != i+1)) continue;
				e = gf_isom_set_media_creation_time(file, i+1, tka->time, tka->time);
				if (e) return e;
			}
			do_save = GF_TRUE;
			break;
		default:
			break;
		}
		if (e) return e;
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}



static Bool do_qt_keys(char *name, char *val)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	GF_Err e = gf_media_isom_apply_qt_key(file, name, val);
	if (e) return GF_FALSE;
	do_save = GF_TRUE;
	return GF_TRUE;
#else
	return GF_FALSE;
#endif
}

static GF_Err do_itunes_tag()
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	GF_Err e;
	char *itunes_data = NULL;
	char *tags = itunes_tags;

	if (gf_file_exists(itunes_tags)) {
		u32 len;
		e = gf_file_load_data(itunes_tags, (u8 **) &itunes_data, &len);
		if (e) return e;
		tags = itunes_data;
	}

	while (tags) {
		char *val;
		Bool clear = GF_FALSE;
		Bool is_wma = GF_FALSE;
		u32 tlen, tagtype=0, itag = 0;
		s32 tag_idx=-1;
		char *sep = itunes_data ? strchr(tags, '\n') : gf_url_colon_suffix(tags, '=');
		while (sep) {
			char *eq = strchr(sep+1, '=');
			if (eq) eq[0] = 0;
			s32 next_tag_idx = gf_itags_find_by_name(sep+1);
			if (next_tag_idx<0) {
				//4CC tag
				if ( strlen(sep+1)==4) {
					next_tag_idx = 0;
				}
				//3CC tag, changed to @tag
				if ( strlen(sep+1)==3) {
					next_tag_idx = 0;
				}
				//unrecognized tag tag
				else if (strlen(sep+1)) {
					M4_LOG(GF_LOG_WARNING, ("Unrecognize tag \"%s\", assuming part of previous tag\n", sep+1));
				}
			}

			if (eq) eq[0] = '=';
			if (next_tag_idx>=0) {
				sep[0] = 0;
				break;
			}
			sep = itunes_data ? strchr(sep+1, '\n') : gf_url_colon_suffix(sep+1, '=');
		}
		val = strchr(tags, '=');
		if (val) val[0] = 0;
		if (!strcmp(tags, "clear") || !strcmp(tags, "reset")) {
			clear = GF_TRUE;
		} else if (!strncmp(tags, "WM/", 3) ) {
			is_wma = GF_TRUE;
		} else if (!strncmp(tags, "QT/", 3) ) {
			tags+=3;
		} else {
			tag_idx = gf_itags_find_by_name(tags);
			if (tag_idx<0) {
				if (strlen(tags)==4) {
					itag = GF_4CC(tags[0], tags[1], tags[2], tags[3]);
					tagtype = GF_ITAG_STR;
				} else if (strlen(tags)==3) {
					itag = GF_4CC(0xA9, tags[0], tags[1], tags[2]);
					tagtype = GF_ITAG_STR;
				}
			}
		}
		if (!itag && !clear && !is_wma) {
			if (tag_idx<0) {
				if (val && !do_qt_keys(tags, val+1)) {
					M4_LOG(GF_LOG_WARNING, ("Invalid iTune tag name \"%s\" - ignoring\n", tags));
				}
				if (val) val[0] = '=';
				goto tag_done;
			}
			itag = gf_itags_get_itag(tag_idx);
			tagtype = gf_itags_get_type(tag_idx);
		}
		if (val) {
			val[0] = '=';
			val++;
		}
		if (!val || (val[0]==':') || !val[0] || !stricmp(val, "NULL") ) val = NULL;

		//if val is NULL, use tlen=1 to force removal of tag
		if (val) {
			tlen = (u32) strlen(val);
			while (tlen && ( (val[tlen-1]=='\n') || (val[tlen-1]=='\r')))
				tlen--;
		} else {
			tlen = 1;
		}

		if (clear) {
			e = gf_isom_apple_set_tag(file, GF_ISOM_ITUNE_RESET, NULL, 0, 0, 0);
			e |= gf_isom_set_qt_key(file, NULL);
		}
		else if (is_wma) {
			if (val) val[-1] = 0;
			e = gf_isom_wma_set_tag(file, tags, val);
			if (val) val[-1] = '=';
		}
		else if (val && (tagtype==GF_ITAG_FILE)) {
			u32 flen = (u32) strlen(val);
			u8 *d=NULL;
			while (flen && val[flen-1]=='\n') flen--;
			val[flen] = 0;
			e = gf_file_load_data(val, (u8 **) &d, &tlen);
			val[flen] = '\n';

			if (!e)
				e = gf_isom_apple_set_tag(file, itag, d, tlen, 0, 0);

			if (d) gf_free(d);
		} else {
			e = gf_isom_apple_set_tag(file, itag, (u8 *) val, tlen, 0, 0);
		}
		if (e) {
			M4_LOG(GF_LOG_ERROR, ("Error assigning tag %s: %s\n", tags, gf_error_to_string(e) ));
		}

		do_save = GF_TRUE;

tag_done:
		if (sep) {
			sep[0] = itunes_data ? '\n' : ':';
			tags = sep+1;
		} else {
			tags = NULL;
		}
	}
	if (itunes_data) gf_free(itunes_data);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

#if !defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)
static void set_sdp_ext()
{
	u32 i, j;
	for (i=0; i<nb_sdp_ex; i++) {
		u32 trackID = get_track_id(file, &sdp_lines[i].track_id);
		if (trackID) {
			u32 track = gf_isom_get_track_by_id(file, trackID);
			if (gf_isom_get_media_type(file, track)!=GF_ISOM_MEDIA_HINT) {
				s32 ref_count;
				u32 k, count = gf_isom_get_track_count(file);
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
			do_save = GF_TRUE;
		} else {
			gf_isom_sdp_add_line(file, sdp_lines[i].line);
			do_save = GF_TRUE;
		}
	}
}
#endif /*!defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)*/

static GF_Err do_remux_file()
{
#ifndef GPAC_DISABLE_MEDIA_EXPORT
	GF_MediaExporter mdump;
	memset(&mdump, 0, sizeof(GF_MediaExporter));
	mdump.in_name = inName;
	mdump.out_name = mux_name;
	mdump.flags = GF_EXPORT_REMUX;
	mdump.print_stats_graph = fs_dump_flags;
	return gf_media_export(&mdump);
#else
	return GF_NOT_SUPPORTED;
#endif
}

static u32 mp4box_cleanup(u32 ret_code) {
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
			if (metas[i].keep_props) gf_free(metas[i].keep_props);
			if (metas[i].image_props) {
				GF_ImageItemProperties *iprops = metas[i].image_props;
				if (iprops->overlay_offsets) gf_free(iprops->overlay_offsets);
				if (iprops->aux_urn) gf_free((char *) iprops->aux_urn);
				if (iprops->aux_data) gf_free((char *) iprops->aux_data);
				gf_free(iprops);
			}
		}
		gf_free(metas);
		metas = NULL;
	}
	if (tracks) {
		u32 i;
		for (i = 0; i<nb_track_act; i++) {
			if (tracks[i].out_name)
				gf_free(tracks[i].out_name);
			if (tracks[i].src_name)
				gf_free(tracks[i].src_name);
			if (tracks[i].string)
				gf_free(tracks[i].string);
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

#ifdef GPAC_MEMORY_TRACKING
	if (mem_track && (gf_memory_size() || gf_file_handles_count() )) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
	}
#endif
	return ret_code;
}



int mp4box_main(int argc, char **argv)
{
	u32 i, j;
	const char *gpac_profile = "0";
	GF_Err e = GF_OK;

	init_global_vars();

#ifdef TEST_ARGS
	i=0;
	mp4box_parse_single_arg(argc, argv, "", &i);
#endif

	for (i = 1; i < (u32) argc ; i++) {
		if (!strcmp(argv[i], "-mem-track") || !strcmp(argv[i], "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(argv[i], "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			M4_LOG(GF_LOG_WARNING, ("WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", argv[i]));
#endif
			break;
		}
		else if (!strcmp(argv[i], "-p")) {
			if (i+1<(u32) argc)
				gpac_profile = argv[i+1];
			else {
				M4_LOG(GF_LOG_ERROR, ("Bad argument for -p, expecting profile name but no more args\n"));
				return 1;
			}
		}
		else if (!strncmp(argv[i], "-p=", 3))
			gpac_profile = argv[i]+3;
	}

#ifdef _TWO_DIGIT_EXPONENT
	_set_output_format(_TWO_DIGIT_EXPONENT);
#endif

	/*init libgpac*/
	gf_sys_init(mem_track, gpac_profile);
#ifdef GPAC_CONFIG_ANDROID
	//prevent destruction of JSRT until we unload the JNI gpac wrapper (see applications/gpac_android/src/main/jni/gpac_jni.cpp)
	gf_opts_set_key("temp", "static-jsrt", "true");
#endif

	if (argc < 2) {
		M4_LOG(GF_LOG_ERROR, ("Not enough arguments - check usage with -h\n"));
		M4_LOG(GF_LOG_INFO, ("MP4Box - GPAC version %s\n"
	        "%s\n", gf_gpac_version(), gf_gpac_copyright_cite() ));
		gf_sys_close();
		return 0;
	}

	helpout = stdout;

	i = mp4box_parse_args(argc, argv);
	if (i) {
		return mp4box_cleanup(i - 1);
	}
#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_SENG) && !defined(GPAC_CONFIG_EMSCRIPTEN)
	if (live_scene) {
		int ret = live_session(argc, argv);
		return mp4box_cleanup(ret);
	}
#endif

	if (!dash_duration && interleaving_time && do_frag)
		interleaving_time /= 1000;

	if (do_mpd_conv) inName = do_mpd_conv;

#ifndef GPAC_DISABLE_STREAMING
	if (import_flags & GF_IMPORT_FORCE_MPEG4)
		hint_flags |= GP_RTP_PCK_FORCE_MPEG4;
#endif

	if (!inName && dump_std)
		inName = "std";

	if (!dash_duration && cprt)
		open_edit = GF_TRUE;

	if (!inName) {
		if (has_next_arg) {
			M4_LOG(GF_LOG_ERROR, ("Broken argument specifier or file name missing - check usage with -h\n"));
		} else {
			PrintUsage();
		}
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
		else if (!do_flat && !(split_duration || split_size || split_range_str)) {
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
			M4_LOG(GF_LOG_ERROR, ("Fatal error: cannot reopen stdout in binary mode.\n"));
			return mp4box_cleanup(1);
		}
	}

	GF_LOG_Level level = verbose ? GF_LOG_DEBUG : GF_LOG_INFO;
	gf_log_set_tool_level(GF_LOG_CONTAINER, level);
	gf_log_set_tool_level(GF_LOG_SCENE, level);
	gf_log_set_tool_level(GF_LOG_PARSER, level);
	gf_log_set_tool_level(GF_LOG_MEDIA, level);
	gf_log_set_tool_level(GF_LOG_CODING, level);
	gf_log_set_tool_level(GF_LOG_DASH, level);
#ifdef GPAC_MEMORY_TRACKING
	if (mem_track)
		gf_log_set_tool_level(GF_LOG_MEMORY, level);
#endif

	if (fuzz_chk) {
		file = gf_isom_open(inName, GF_ISOM_OPEN_READ_DUMP, NULL);
		if (file) gf_isom_close(file);
		file = gf_isom_open(inName, GF_ISOM_OPEN_READ, NULL);
		if (file) gf_isom_close(file);
		file = gf_isom_open(inName, GF_ISOM_OPEN_KEEP_FRAGMENTS, NULL);
		if (file) gf_isom_close(file);
		return 0;
	}

	e = gf_sys_set_args(argc, (const char **) argv);
	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error assigning libgpac arguments: %s\n", gf_error_to_string(e) ));
		return mp4box_cleanup(1);
	}

	if (raw_cat)
		return do_raw_cat();

	if (compress_top_boxes) {
		if (size_top_box) {
			u64 top_size = do_size_top_boxes(inName, compress_top_boxes, size_top_box);
			fprintf(stdout, LLU"\n", top_size);
			return mp4box_cleanup(e ? 1 : 0);
		} else {
			e = do_compress_top_boxes(inName, outName);
			return mp4box_cleanup(e ? 1 : 0);
		}
	}

	if (do_mpd_rip) {
		e = rip_mpd(inName, outName);
		return mp4box_cleanup(e ? 1 : 0);
	}

	if (do_wget != NULL) {
#ifndef GPAC_DISABLE_NETWORK
		e = gf_dm_wget(do_wget, inName, 0, 0, NULL);
#else
		e = GF_NOT_SUPPORTED;
#endif
		if (e != GF_OK) {
			M4_LOG(GF_LOG_ERROR, ("Cannot retrieve %s: %s\n", do_wget, gf_error_to_string(e) ));
			return mp4box_cleanup(1);
		}
		return mp4box_cleanup(0);
	}

	if (udp_dest)
		return do_write_udp();

#ifndef GPAC_DISABLE_MPD
	if (do_mpd_conv)
		return convert_mpd();
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

	if (import_subtitle && !ttxt_track_id.type && !ttxt_track_id.ID_or_num)
		return do_import_sub();


#if !defined(GPAC_DISABLE_MEDIA_IMPORT) && !defined(GPAC_DISABLE_ISOM_WRITE)
	if (nb_add || nb_cat) {
		u32 res = do_add_cat(argc, argv);
		if (res) return res;
	}
#endif /*!GPAC_DISABLE_MEDIA_IMPORT && !GPAC_DISABLE_ISOM_WRITE*/

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	else if (chunk_mode) {
		if (!inName) {
			M4_LOG(GF_LOG_ERROR, ("chunk encoding syntax: [-outctx outDump] -inctx inScene auFile\n"));
			return mp4box_cleanup(1);
		}
		e = EncodeFileChunk(inName, outName ? outName : inName, input_ctx, output_ctx);
		if (e) {
			M4_LOG(GF_LOG_ERROR, ("Error encoding chunk file %s\n", gf_error_to_string(e)));
			return mp4box_cleanup(1);
		}
		goto exit;
	}
#endif // !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)

	else if (encode) {
#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
		e = do_scene_encode();
		if (e) goto err_exit;
#endif //!defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_SCENE_ENCODER) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	}

#ifndef GPAC_DISABLE_ISOM_WRITE
	else if (pack_file) {
		//don't use any check for ':', the first word must be the four CC
		char *fileName = gf_url_colon_suffix(pack_file, 0);
		if (fileName && ((fileName - pack_file)==4)) {
			fileName[0] = 0;
			file = package_file(fileName + 1, pack_file, pack_wgt);
			fileName[0] = ':';
		} else {
			file = package_file(pack_file, NULL, pack_wgt);
		}
		if (!file) {
			M4_LOG(GF_LOG_ERROR, ("Failed to package file\n"));
			return mp4box_cleanup(1);
		}
		if (!outName) outName = inName;
		do_save = GF_TRUE;
		open_edit = GF_TRUE;
	}
#endif //GPAC_DISABLE_ISOM_WRITE

	if (dash_duration) {
		e = do_dash();
		if (e) return mp4box_cleanup(1);
		goto exit;
	}

	if (split_duration || split_size || split_range_str) {
		if (force_new==2) {
			do_flat = 3;
			force_new = 0;
		}
	} else {
		if (do_flat)
			open_edit = GF_TRUE;
	}

	//need to open input
	if (!file && !do_hash) {
		FILE *st = gf_fopen(inName, "rb");
		Bool file_exists = 0;
		GF_ISOOpenMode omode;
		if (st) {
			file_exists = 1;
			gf_fclose(st);
		}
		switch (get_file_type_by_ext(inName)) {
		case 1:
			omode =  (u8) (force_new ? GF_ISOM_WRITE_EDIT : (open_edit ? GF_ISOM_OPEN_EDIT : ( ((dump_isom>0) || print_info) ? GF_ISOM_OPEN_READ_DUMP : GF_ISOM_OPEN_READ) ) );

			if (crypt) {
				//keep fragment signaling in moov
				omode = GF_ISOM_OPEN_READ;
				if (use_init_seg)
					file = gf_isom_open(use_init_seg, GF_ISOM_OPEN_READ, NULL);
			}
			if (!crypt && use_init_seg) {
				file = gf_isom_open(use_init_seg, GF_ISOM_OPEN_READ_DUMP, NULL);
				if (file) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
					e = gf_isom_open_segment(file, inName, 0, 0, 0);
#else
					e = GF_NOT_SUPPORTED;
#endif
					if (e==GF_ISOM_INCOMPLETE_FILE) {
						M4_LOG(GF_LOG_WARNING, ("Segment %s: %s\n", inName, gf_error_to_string(e) ));
					} else if (e) {
						M4_LOG(GF_LOG_ERROR, ("Error opening segment %s: %s\n", inName, gf_error_to_string(e) ));
						gf_isom_delete(file);
						file = NULL;
					}
				}
			}
			if (!file)
				file = gf_isom_open(inName, omode, NULL);

			if (!file && (gf_isom_last_error(NULL) == GF_ISOM_INCOMPLETE_FILE) && !open_edit) {
				u64 missing_bytes;
				gf_isom_open_progressive(inName, 0, 0, GF_FALSE, &file, &missing_bytes);
				M4_LOG(GF_LOG_ERROR, ("Incomplete file - missing "LLD" bytes\n", missing_bytes));
			}

			if (!file) {
				if (open_edit && nb_meta_act) {
					file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, NULL);
					if (!outName && file) outName = inName;
				}

				if (!file) {
					M4_LOG(GF_LOG_ERROR, ("Error opening file %s: %s\n", inName, gf_error_to_string(gf_isom_last_error(NULL))));
					return mp4box_cleanup(1);
				}
			}
#ifndef GPAC_DISABLE_ISOM_WRITE
			if (freeze_box_order)
				gf_isom_freeze_order(file);
#endif
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
#endif //GPAC_DISABLE_SCENE_DUMP
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
						M4_LOG(GF_LOG_ERROR, ("Error importing %s: %s\n", inName, gf_error_to_string(e)));
						gf_isom_delete(file);
						gf_file_delete("ttxt_convert");
						return mp4box_cleanup(1);
					}
				}
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/

				if (dump_m2ts) {
#ifndef GPAC_DISABLE_MPEG2TS
					dump_mpeg2_ts(inName, outName, program_number);
#endif
				} else if (dump_timestamps) {
#ifndef GPAC_DISABLE_MPEG2TS
					dump_mpeg2_ts(inName, outName, program_number);
#endif
				} else if (do_bin_xml) {
					xml_bs_to_bin(inName, outName, dump_std);
				} else if (do_hash) {
					hash_file(inName, dump_std);
				} else if (print_info) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
					e = convert_file_info(inName, &info_track_id);
					if (e) goto err_exit;
#endif
				} else {
					if (mux_name) {
						e = do_remux_file();
						if (e) goto err_exit;
						if (file) gf_isom_delete(file);
						goto exit;
					} else {
						M4_LOG(GF_LOG_ERROR, ("Input %s is not an MP4 file, operation not allowed\n", inName));
						return mp4box_cleanup(1);
					}
				}
				goto exit;
			}
#endif /*GPAC_DISABLE_ISOM_WRITE*/
			else if (open_edit) {
				file = gf_isom_open(inName, GF_ISOM_WRITE_EDIT, NULL);
				if (!outName && file) outName = inName;
			} else if (!file_exists) {
				M4_LOG(GF_LOG_ERROR, ("Error %s file %s: %s\n", force_new ? "creating" : "opening", inName, gf_error_to_string(GF_URL_ERROR)));
				return mp4box_cleanup(1);
			} else {
				M4_LOG(GF_LOG_ERROR, ("Cannot open %s - extension not supported\n", inName));
				return mp4box_cleanup(1);
			}
		}
	}

#ifndef GPAC_DISABLE_ISOM_WRITE
	if (high_dynamc_range_filename) {
		e = apply_high_dynamc_range_xml_desc(file, 0, high_dynamc_range_filename);
		if (e) goto err_exit;
	}

	if (file && keep_utc) {
		gf_isom_keep_utc_times(file, 1);
	}
#endif

	if ( gf_strlcpy(outfile, outName ? outName : inName, sizeof(outfile)) >= sizeof(outfile) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Filename too long (limit is %d)\n", GF_MAX_PATH));
		return mp4box_cleanup(1);
	}

	char *szExt = gf_file_ext_start(outfile);
	if (szExt) {
		/*turn on 3GP saving*/
		if (!stricmp(szExt, ".3gp") || !stricmp(szExt, ".3gpp") || !stricmp(szExt, ".3g2"))
			conv_type = GF_ISOM_CONV_TYPE_3GPP;
		else if (!stricmp(szExt, ".m4a") || !stricmp(szExt, ".m4v"))
			conv_type = GF_ISOM_CONV_TYPE_IPOD;
		else if (!stricmp(szExt, ".psp"))
			conv_type = GF_ISOM_CONV_TYPE_PSP;
		else if (!stricmp(szExt, ".mov") || !stricmp(szExt, ".qt"))
			conv_type = GF_ISOM_CONV_TYPE_MOV;

		if (conv_type)
			conv_type_from_ext = GF_TRUE;
		//if not same ext, force conversion
		char *szExtOrig = gf_file_ext_start(inName);
		if (szExtOrig && strcmp(szExtOrig, szExt))
			conv_type_from_ext = GF_FALSE;
		//remove extension from outfile
		*szExt = 0;
	}

#ifndef GPAC_DISABLE_MEDIA_EXPORT
	if (!open_edit && track_dump_type && !gf_isom_probe_file(inName)) {
		e = do_export_tracks_non_isobmf();
		if (e) goto err_exit;
		goto exit;
	}
	if (mux_name) {
		e = do_remux_file();
		if (e) goto err_exit;
		if (file) gf_isom_delete(file);
		goto exit;
	}



#endif /*GPAC_DISABLE_MEDIA_EXPORT*/

#ifndef GPAC_DISABLE_SCENE_DUMP
	if (dump_mode != GF_SM_DUMP_NONE) {
		e = dump_isom_scene(inName, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_mode, do_scene_log, no_odf_conf);
		if (e) goto err_exit;
	}
#endif

#ifndef GPAC_DISABLE_SCENE_STATS
	if (stat_level) dump_isom_scene_stats(inName, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, stat_level);
#endif

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (!do_hint && print_sdp) dump_isom_sdp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
#endif
	if (get_nb_tracks) {
		fprintf(stdout, "%d\n", gf_isom_get_track_count(file));
	}
	if (print_info) {
		if (!file) {
			M4_LOG(GF_LOG_ERROR, ("Cannot print info on a non ISOM file (%s)\n", inName));
		} else {
			u32 info_tk_id = get_track_id(file, &info_track_id);
			if (info_tk_id) {
				DumpTrackInfo(file, info_tk_id, 1, (print_info==2) ? GF_TRUE : GF_FALSE, GF_FALSE);
			}
			else DumpMovieInfo(file, (print_info==2) ? GF_TRUE : GF_FALSE);
		}
	}
#ifndef GPAC_DISABLE_ISOM_DUMP
	if (dump_isom) {
		e = dump_isom_xml(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, (dump_isom==2) ? GF_TRUE : GF_FALSE, merge_vtt_cues, use_init_seg ? GF_TRUE : GF_FALSE, (dump_isom==3) ? GF_TRUE : GF_FALSE);
		if (e) goto err_exit;
	}
	if (dump_cr) dump_isom_ismacryp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);

	if (dump_ttxt || dump_srt) {
		u32 ttxt_tkid = get_track_id(file, &ttxt_track_id);
		if (ttxt_tkid == (u32)-1) {
			for (j=0; j<gf_isom_get_track_count(file); j++) {
				ttxt_tkid = gf_isom_get_track_id(file, j+1);
				dump_isom_timed_text(file, ttxt_tkid, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE,
									GF_FALSE, dump_srt ? GF_TEXTDUMPTYPE_SRT : GF_TEXTDUMPTYPE_TTXT);
			}

		}
		else if (ttxt_tkid) {
			dump_isom_timed_text(file, ttxt_tkid, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE,
								GF_FALSE, dump_srt ? GF_TEXTDUMPTYPE_SRT : GF_TEXTDUMPTYPE_TTXT);
		}
	}

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (dump_rtp) dump_isom_rtp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
#endif

#endif

	if (dump_timestamps) dump_isom_timestamps(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_timestamps);

	u32 dump_tkid = get_track_id(file, &dump_nal_track);
	if (dump_tkid) {
		e = dump_isom_nal(file, dump_tkid, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_nal_type);
		if (e) goto err_exit;
	}
	dump_tkid = get_track_id(file, &dump_saps_track);
	if (dump_tkid) dump_isom_saps(file, dump_tkid, dump_saps_mode, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);

	if (do_hash) {
		e = hash_file(inName, dump_std);
		if (e) goto err_exit;
	}
	if (do_bin_xml) {
		e = xml_bs_to_bin(inName, outName, dump_std);
		if (e) goto err_exit;
	}

	if (dump_chunk) dump_isom_chunks(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
	if (dump_cart) dump_isom_cover_art(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
	if (dump_chap) dump_isom_chapters(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_chap);
	if (dump_udta_type) {
		dump_tkid = get_track_id(file, &dump_udta_track);
		dump_isom_udta(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE, dump_udta_type, dump_tkid);
	}

	if (dump_iod) {
		e = do_dump_iod();
		if (e) goto err_exit;
	}

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
	if (split_duration || split_size || split_range_str) {
		if (force_new && !outName) outName = inName;
		e = split_isomedia_file(file, split_duration, split_size, inName, interleaving_time, split_start, adjust_split_end, outName, seg_at_rap, split_range_str, fs_dump_flags);
		if (e) goto err_exit;

		/*never save file when splitting is desired*/
		open_edit = GF_FALSE;
		do_save = GF_FALSE;
	}
#endif // !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)

#ifndef GPAC_DISABLE_MEDIA_EXPORT
	if (track_dump_type) {
		e = do_export_tracks();
		if (e) goto err_exit;
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

	e = do_meta_act();
	if (e) goto err_exit;

	if (!open_edit && !do_save) {
		if (file) gf_isom_delete(file);
		goto exit;
	}


#ifndef GPAC_DISABLE_ISOM_WRITE
	if (clean_groups) {
		e = gf_isom_reset_switch_parameters(file);
		if (e) goto err_exit;
		do_save = GF_TRUE;
	}


	e = do_tsel_act();
	if (e) goto err_exit;

	if (remove_sys_tracks) {
#ifndef GPAC_DISABLE_AV_PARSERS
		remove_systems_tracks(file);
#endif
		do_save = GF_TRUE;
		if (conv_type < GF_ISOM_CONV_TYPE_ISMA_EX) conv_type = 0;
	}
	if (remove_root_od) {
		gf_isom_remove_root_od(file);
		do_save = GF_TRUE;
	}
#ifndef GPAC_DISABLE_ISOM_HINTING
	if (remove_hint) {
		for (i=0; i<gf_isom_get_track_count(file); i++) {
			if (gf_isom_get_media_type(file, i+1) == GF_ISOM_MEDIA_HINT) {
				M4_LOG(GF_LOG_INFO, ("Removing hint track ID %d\n", gf_isom_get_track_id(file, i+1)));
				gf_isom_remove_track(file, i+1);
				i--;
			}
		}
		gf_isom_sdp_clean(file);
		do_save = GF_TRUE;
	}
#endif // GPAC_DISABLE_ISOM_HINTING

	if (timescale && (timescale != gf_isom_get_timescale(file))) {
		gf_isom_set_timescale(file, timescale);
		do_save = GF_TRUE;
	}

	if (!encode) {
		if (!file) {
			M4_LOG(GF_LOG_INFO, ("Nothing to do - exiting\n"));
			goto exit;
		}
		if (outName) {
			strcpy(outfile, outName);
		} else {
			const char *tmp_dir = gf_opts_get_key("core", "tmp");
			char *rel_name = strrchr(inName, GF_PATH_SEPARATOR);
			if (!rel_name) rel_name = strrchr(inName, '/');

			strcpy(outfile, "");
			if (tmp_dir) {
				strcpy(outfile, tmp_dir);
				if (!strchr("\\/", tmp_dir[strlen(tmp_dir)-1])) strcat(outfile, "/");
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

		if (conv_type_from_ext) {
			if (!no_inplace && gf_isom_is_inplace_rewrite(file))
				conv_type = 0;
		}

#ifndef GPAC_DISABLE_MEDIA_IMPORT
		if ((conv_type == GF_ISOM_CONV_TYPE_ISMA) || (conv_type == GF_ISOM_CONV_TYPE_ISMA_EX)) {
			M4_LOG(GF_LOG_INFO, ("Converting to ISMA Audio-Video MP4 file\n"));
			/*keep ESIDs when doing ISMACryp*/
			e = gf_media_make_isma(file, crypt ? 1 : 0, GF_FALSE, (conv_type==GF_ISOM_CONV_TYPE_ISMA_EX) ? 1 : 0);
			if (e) goto err_exit;
			do_save = GF_TRUE;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_3GPP) {
			M4_LOG(GF_LOG_INFO, ("Converting to 3GP file\n"));
			e = gf_media_make_3gpp(file);
			if (e) goto err_exit;
			do_save = GF_TRUE;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_PSP) {
			M4_LOG(GF_LOG_INFO, ("Converting to PSP file\n"));
			e = gf_media_make_psp(file);
			if (e) goto err_exit;
			do_save = GF_TRUE;
		}
		if (conv_type == GF_ISOM_CONV_TYPE_MOV) {
			e = gf_media_check_qt_prores(file);
			if (e) goto err_exit;
			do_save = GF_TRUE;
			if (interleaving_time) interleaving_time = 0.5;
		}
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/
		if (conv_type == GF_ISOM_CONV_TYPE_IPOD) {
			do_ipod_conv();
		}

	} else if (outName) {
		strcpy(outfile, outName);
	}

	e = do_track_act();
	if (e) goto err_exit;

	if (itunes_tags) {
		e = do_itunes_tag();
		if (e) goto err_exit;
	}

	if (cprt) {
		e = gf_isom_set_copyright(file, "und", cprt);
		do_save = GF_TRUE;
		if (e) goto err_exit;
	}
	if (chap_file || chap_file_qt) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		Bool chap_qt = GF_FALSE;
		if (chap_file_qt) {
			chap_file = chap_file_qt;
			chap_qt = GF_TRUE;
		}
		e = gf_media_import_chapters(file, chap_file, import_fps, chap_qt);
		do_save = GF_TRUE;
#else
		M4_LOG(GF_LOG_WARNING, ("Warning: GPAC compiled without Media Import, chapters can't be imported\n"));
		e = GF_NOT_SUPPORTED;
#endif
		if (e) goto err_exit;
	}

	if (major_brand) {
		gf_isom_set_brand_info(file, major_brand, minor_version);
		do_save = GF_TRUE;
	}
	for (i=0; i<nb_alt_brand_add; i++) {
		gf_isom_modify_alternate_brand(file, brand_add[i], GF_TRUE);
		do_save = GF_TRUE;
	}
	for (i=0; i<nb_alt_brand_rem; i++) {
		gf_isom_modify_alternate_brand(file, brand_rem[i], GF_FALSE);
		do_save = GF_TRUE;
	}
	if (box_patch_filename) {
		e = gf_isom_apply_box_patch(file, get_track_id(file, &box_patch_track), box_patch_filename, GF_FALSE);
		if (e) {
			M4_LOG(GF_LOG_ERROR, ("Failed to apply box patch %s: %s\n", box_patch_filename, gf_error_to_string(e) ));
			goto err_exit;
		}
		do_save = GF_TRUE;
	}

#ifndef GPAC_DISABLE_CRYPTO
	if (crypt) {
		if (!drm_file && (crypt==1) ) {
			M4_LOG(GF_LOG_ERROR, ("Missing DRM file location - usage '-%s drm_file input_file\n", (crypt==1) ? "crypt" : "decrypt"));
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
			if (use_init_seg) {
				e = gf_decrypt_fragment(file, drm_file, outfile, inName, fs_dump_flags);
			} else {
				e = gf_decrypt_file(file, drm_file, outfile, interleaving_time, fs_dump_flags);
			}
		}
		if (e) goto err_exit;
		do_save = outName ? GF_FALSE : GF_TRUE;

		if (!do_frag && !do_hint && !full_interleave && !force_co64) {
			char szName[GF_MAX_PATH];
			strcpy(szName, gf_isom_get_filename(file) );
			gf_isom_delete(file);
			file = NULL;
			if (!outName) {
				e = gf_file_move(outfile, szName);
				if (e) goto err_exit;
			}
			goto exit;
		}
	}
#endif /*GPAC_DISABLE_CRYPTO*/

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (do_frag) {
		if (!interleaving_time) interleaving_time = DEFAULT_INTERLEAVING_IN_SEC;
		if (do_hint) M4_LOG(GF_LOG_WARNING, ("Warning: cannot hint and fragment - ignoring hint\n"));
		M4_LOG(GF_LOG_INFO, ("Fragmenting file (%.3f seconds fragments)\n", interleaving_time));
		e = gf_media_fragment_file(file, outfile, interleaving_time, use_mfra);
		if (e) M4_LOG(GF_LOG_ERROR, ("Error while fragmenting file: %s\n", gf_error_to_string(e)));
		if (!e && !outName) {
			if (gf_file_exists(inName) && gf_file_delete(inName)) {
				M4_LOG(GF_LOG_INFO, ("Error removing file %s\n", inName));
			}
			else if (gf_file_move(outfile, inName)) {
				M4_LOG(GF_LOG_INFO, ("Error renaming file %s to %s\n", outfile, inName));
			}
		}
		if (e) goto err_exit;
		gf_isom_delete(file);
		goto exit;
	}
#endif

#ifndef GPAC_DISABLE_ISOM_HINTING
	if (do_hint) {
		if (force_ocr) SetupClockReferences(file);
		MTUSize -= 12;
		e = HintFile(file, MTUSize, max_ptime, rtp_rate, hint_flags, HintCopy, hint_interleave, regular_iod, single_group, hint_no_offset);
		if (e) goto err_exit;
		do_save = GF_TRUE;
		if (print_sdp) dump_isom_sdp(file, dump_std ? NULL : (outName ? outName : outfile), outName ? GF_TRUE : GF_FALSE);
	}
#endif

#if !defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)
	set_sdp_ext();
#endif /*!defined(GPAC_DISABLE_ISOM_HINTING) && !defined(GPAC_DISABLE_SENG)*/

	if (force_co64)
		gf_isom_force_64bit_chunk_offset(file, GF_TRUE);

	if (compress_moov)
		gf_isom_enable_compression(file, GF_ISOM_COMP_ALL, (compress_moov==2) ? GF_ISOM_COMP_WRAP_FTYPE : 0);

	if (no_inplace)
		gf_isom_disable_inplace_rewrite(file);

	if (moov_pading)
		gf_isom_set_inplace_padding(file, moov_pading);

	if (outName) {
		gf_isom_set_final_name(file, outfile);
	} else if (!encode && !force_new && !gf_isom_is_inplace_rewrite(file)) {
		gf_isom_set_final_name(file, outfile);
	}

	Bool is_inplace = gf_isom_is_inplace_rewrite(file);


	/*full interleave (sample-based) if just hinted*/
	if (full_interleave) {
		e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_TIGHT);
	} else if (do_flat) {
		e = gf_isom_set_storage_mode(file, (do_flat==1) ? GF_ISOM_STORE_FLAT : GF_ISOM_STORE_STREAMABLE);
		do_save = GF_TRUE;
	}
	//do not set storage mode unless inplace rewrite is disabled , either by user or due to operations on file
	else if (!is_inplace) {
		e = gf_isom_make_interleave(file, interleaving_time);
		if (!e && old_interleave) e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_INTERLEAVED);
	}

	if (e) goto err_exit;


	if (do_save) {

		if (!gf_sys_is_quiet()) {
			if (outName) {
			} else if (encode || pack_file) {
				M4_LOG(GF_LOG_INFO, ("Saving to %s: ", gf_isom_get_filename(file) ));
			} else {
				M4_LOG(GF_LOG_INFO, ("Saving %s: ", inName));
			}
			if (is_inplace) {
				M4_LOG(GF_LOG_INFO, ("In-place rewrite\n"));
			} else if (do_hint && full_interleave) {
				M4_LOG(GF_LOG_INFO, ("Hinted file - Full Interleaving\n"));
			} else if (full_interleave) {
				M4_LOG(GF_LOG_INFO, ("Full Interleaving\n"));
			} else if ((force_new==2) && interleaving_time) {
				M4_LOG(GF_LOG_INFO, ("Fast-start interleaved storage\n"));
			} else if (do_flat || !interleaving_time) {
				M4_LOG(GF_LOG_INFO, ("Flat storage\n"));
			} else {
				M4_LOG(GF_LOG_INFO, ("%.3f secs Interleaving%s\n", interleaving_time, old_interleave ? " - no drift control" : ""));
			}
		}

		e = gf_isom_close(file);
		file = NULL;

		if (!e && !outName && !encode && !force_new && !pack_file && !is_inplace) {
			if (gf_file_exists(inName)) {
				e = gf_file_delete(inName);
				if (e) {
					M4_LOG(GF_LOG_ERROR, ("Error removing file %s\n", inName));
				}
			}

			e = gf_file_move(outfile, inName);
			if (e) {
				M4_LOG(GF_LOG_ERROR, ("Error renaming file %s to %s\n", outfile, inName));
			}
		}
	} else {
		gf_isom_delete(file);
	}

	if (e) {
		M4_LOG(GF_LOG_ERROR, ("Error: %s\n", gf_error_to_string(e)));
		goto err_exit;
	}
	goto exit;

#else
	/*close libgpac*/
	gf_isom_delete(file);
	M4_LOG(GF_LOG_ERROR, ("Error: Read-only version of MP4Box.\n"));
	return mp4box_cleanup(1);
#endif //GPAC_DISABLE_ISOM_WRITE


err_exit:
	/*close libgpac*/
	if (file) gf_isom_delete(file);
	M4_LOG(GF_LOG_ERROR, ("\n\tError: %s\n", gf_error_to_string(e)));
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

#if !defined(GPAC_CONFIG_ANDROID) && !defined(GPAC_CONFIG_EMSCRIPTEN)

GF_MAIN_FUNC(mp4box_main)

#endif //GPAC_CONFIG_ANDROID


#endif /*GPAC_DISABLE_ISOM*/
