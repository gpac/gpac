/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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
#include <gpac/constants.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/media_tools.h>
#include <gpac/bifs.h>
#include <gpac/network.h>
#ifndef GPAC_DISABLE_LASER
#include <gpac/laser.h>
#include <gpac/nodes_svg.h>
#endif
#include <gpac/internal/scenegraph_dev.h>


#ifndef GPAC_DISABLE_SCENE_ENCODER

#ifndef GPAC_DISABLE_MEDIA_IMPORT
static void gf_sm_remove_mux_info(GF_ESD *src)
{
	u32 i;
	GF_MuxInfo *mux;
	i=0;
	while ((mux = (GF_MuxInfo *)gf_list_enum(src->extensionDescriptors, &i))) {
		if (mux->tag == GF_ODF_MUXINFO_TAG) {
			gf_odf_desc_del((GF_Descriptor *)mux);
			gf_list_rem(src->extensionDescriptors, i-1);
			return;
		}
	}
}
#endif


static void gf_sm_finalize_mux(GF_ISOFile *mp4, GF_ESD *src, u32 offset_ts)
{
#if !defined (GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)
	u32 track, mts, ts;
	GF_MuxInfo *mux = gf_sm_get_mux_info(src);
	if (!mux && !offset_ts) return;
	track = gf_isom_get_track_by_id(mp4, src->ESID);
	if (!track) return;

	mts = gf_isom_get_media_timescale(mp4, track);
	ts = gf_isom_get_timescale(mp4);
	/*set track time offset*/
	if (mux) offset_ts += mux->startTime * mts / 1000;
	if (offset_ts) {
		u32 off = offset_ts * ts  / mts;
		u64 dur = gf_isom_get_media_duration(mp4, track);
		dur = dur * ts / mts;
		gf_isom_set_edit_segment(mp4, track, 0, off, 0, GF_ISOM_EDIT_EMPTY);
		gf_isom_set_edit_segment(mp4, track, off, dur, 0, GF_ISOM_EDIT_NORMAL);
	}
	/*set track interleaving ID*/
	if (mux) {
		if (mux->GroupID) gf_isom_set_track_interleaving_group(mp4, track, mux->GroupID);
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		if (mux->import_flags & GF_IMPORT_USE_COMPACT_SIZE)
			gf_isom_use_compact_size(mp4, track, 1);
#endif
	}
#endif
}


#ifndef GPAC_DISABLE_MEDIA_IMPORT

static GF_Err gf_sm_import_ui_stream(GF_ISOFile *mp4, GF_ESD *src, Bool rewrite_esd_only)
{
	GF_UIConfig *cfg;
	u32 len, i;
	GF_Err e;
	if (!src->slConfig) src->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	src->slConfig->predefined = 2;
	src->slConfig->timestampResolution = 1000;
	if (!src->decoderConfig || !src->decoderConfig->decoderSpecificInfo) return GF_ODF_INVALID_DESCRIPTOR;
	if (src->decoderConfig->decoderSpecificInfo->tag == GF_ODF_UI_CFG_TAG) {
		cfg = (GF_UIConfig *) src->decoderConfig->decoderSpecificInfo;
		e = gf_odf_encode_ui_config(cfg, &src->decoderConfig->decoderSpecificInfo);
		gf_odf_desc_del((GF_Descriptor *) cfg);
		if (e) return e;
	} else if (src->decoderConfig->decoderSpecificInfo->tag != GF_ODF_DSI_TAG) {
		return GF_ODF_INVALID_DESCRIPTOR;
	}
	if (rewrite_esd_only) return GF_OK;

#ifndef GPAC_DISABLE_ISOM_WRITE
	/*what's the media type for input sensor ??*/
	len = gf_isom_new_track(mp4, src->ESID, GF_ISOM_MEDIA_SCENE, 1000);
	if (!len) return gf_isom_last_error(mp4);
	gf_isom_set_track_enabled(mp4, len, 1);
	if (!src->ESID) src->ESID = gf_isom_get_track_id(mp4, len);
	return gf_isom_new_mpeg4_description(mp4, len, src, NULL, NULL, &i);
#else
	return GF_NOT_SUPPORTED;
#endif
}


static GF_Err gf_sm_import_stream(GF_SceneManager *ctx, GF_ISOFile *mp4, GF_ESD *src, Double imp_time, char *mediaSource, Bool od_sample_rap)
{
	u32 track, di, i;
	GF_Err e;
	Bool isAudio, isVideo;
	char szName[1024];
	char *ext;
	GF_Descriptor *d;
	GF_MediaImporter import;
	GF_MuxInfo *mux = NULL;

	if (od_sample_rap && src->ESID && gf_isom_get_track_by_id(mp4, src->ESID) ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISO Import] Detected several import of the same stream %d in OD RAP sample - skipping duplicated imports\n", src->ESID));
		if (src->decoderConfig->decoderSpecificInfo && (src->decoderConfig->decoderSpecificInfo->tag == GF_ODF_UI_CFG_TAG)) {
			src->decoderConfig->streamType = GF_STREAM_INTERACT;
			return gf_sm_import_ui_stream(mp4, src, 1);
		}
		return GF_OK;
	}

	/*no import if URL string*/
	if (src->URLString) {
		u32 mtype, track;
		if (!src->slConfig) src->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		if (!src->decoderConfig) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] ESD with URL string needs a decoder config with remote stream type to be encoded\n"));
			return GF_BAD_PARAM;
		}
		/*however we still need a track to store the ESD ...*/
		switch (src->decoderConfig->streamType) {
		case GF_STREAM_VISUAL:
			mtype = GF_ISOM_MEDIA_VISUAL;
			break;
		case GF_STREAM_AUDIO:
			mtype = GF_ISOM_MEDIA_AUDIO;
			break;
		case GF_STREAM_MPEG7:
			mtype = GF_ISOM_MEDIA_MPEG7;
			break;
		case GF_STREAM_IPMP:
			mtype = GF_ISOM_MEDIA_IPMP;
			break;
		case GF_STREAM_OCI:
			mtype = GF_ISOM_MEDIA_OCI;
			break;
		case GF_STREAM_MPEGJ:
			mtype = GF_ISOM_MEDIA_MPEGJ;
			break;
		case GF_STREAM_INTERACT:
		case GF_STREAM_SCENE:
			mtype = GF_ISOM_MEDIA_SCENE;
			break;
		case GF_STREAM_TEXT:
			mtype = GF_ISOM_MEDIA_TEXT;
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] Unsupported media type %d for ESD with URL string\n", src->decoderConfig->streamType));
			return GF_BAD_PARAM;
		}
		track = gf_isom_new_track(mp4, src->ESID, mtype, 1000);
		if (!src->ESID) src->ESID = gf_isom_get_track_id(mp4, track);
		return gf_isom_new_mpeg4_description(mp4, track, src, NULL, NULL, &di);
	}

	/*look for muxInfo*/
	mux = gf_sm_get_mux_info(src);

	/*special streams*/
	if (src->decoderConfig) {
		/*InputSensor*/
		if (src->decoderConfig->decoderSpecificInfo && (src->decoderConfig->decoderSpecificInfo->tag == GF_ODF_UI_CFG_TAG))
			src->decoderConfig->streamType = GF_STREAM_INTERACT;
		if (src->decoderConfig->streamType == GF_STREAM_INTERACT) return gf_sm_import_ui_stream(mp4, src, 0);
	}


	/*OCR streams*/
	if (src->decoderConfig && src->decoderConfig->streamType == GF_STREAM_OCR) {
		track = gf_isom_new_track(mp4, src->ESID, GF_ISOM_MEDIA_OCR, 1000);
		if (!track) return gf_isom_last_error(mp4);
		gf_isom_set_track_enabled(mp4, track, 1);
		if (!src->ESID) src->ESID = gf_isom_get_track_id(mp4, track);
		if (!src->slConfig) src->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		src->slConfig->predefined = 2;
		e = gf_isom_new_mpeg4_description(mp4, track, src, NULL, NULL, &di);
		if (e) return e;
		if (mux && mux->duration)
			e = gf_isom_set_edit_segment(mp4, track, 0, mux->duration * gf_isom_get_timescale(mp4) / 1000, 0, GF_ISOM_EDIT_NORMAL);
		return e;
	}

	if (!mux) {
		/*if existing don't import (systems tracks)*/
		track = gf_isom_get_track_by_id(mp4, src->ESID);
		if (track) return GF_OK;
		if (mediaSource) {
			memset(&import, 0, sizeof(GF_MediaImporter));
			import.dest = mp4;
			import.trackID = src->ESID;
			import.orig = gf_isom_open(mediaSource, GF_ISOM_OPEN_READ, NULL);
			if (import.orig) {
				e = gf_media_import(&import);
				gf_isom_delete(import.orig);
				return e;
			}
		}
		return GF_OK;
	}

	if (!mux->file_name) return GF_OK;

	memset(&import, 0, sizeof(GF_MediaImporter));
	if (mux->src_url) {
		ext = gf_url_concatenate(mux->src_url, mux->file_name);
		strcpy(szName, ext ? ext : mux->file_name);
		if (ext) gf_free(ext);
	} else {
		strcpy(szName, mux->file_name);
	}
	ext = strrchr(szName, '.');

	/*get track types for AVI*/
	if (ext && !strnicmp(ext, ".avi", 4)) {
		isAudio = isVideo = 0;
		if (ext && !stricmp(ext, ".avi#video")) isVideo = 1;
		else if (ext && !stricmp(ext, ".avi#audio")) isAudio = 1;
		else if (src->decoderConfig) {
			if (src->decoderConfig->streamType == GF_STREAM_VISUAL) isVideo = 1;
			else if (src->decoderConfig->streamType == GF_STREAM_AUDIO) isAudio = 1;
		}
		if (!isAudio && !isVideo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] missing track specifier for AVI import (file#audio, file#video)\n"));
			return GF_NOT_SUPPORTED;
		}
		if (isVideo) import.trackID = 1;
		else import.trackID = 2;
		ext = strchr(ext, '#');
		if (ext) ext[0] = 0;
	}
	/*get track ID for MP4/others*/
	if (ext) {
		ext = strchr(ext, '#');
		if (ext) {
			import.trackID = atoi(ext+1);
			ext[0] = 0;
		}
	}

	import.streamFormat = mux->streamFormat;
	import.dest = mp4;
	import.esd = src;
	import.duration = mux->duration;
	import.flags = mux->import_flags | GF_IMPORT_FORCE_MPEG4;
	import.video_fps.num = (s32) (1000*mux->frame_rate);
	import.video_fps.den = 1000;
	import.in_name = szName;
	import.initial_time_offset = imp_time;
	e = gf_media_import(&import);
	if (e) return e;

	if (src->OCRESID) {
		gf_isom_set_track_reference(mp4, gf_isom_get_track_by_id(mp4, import.final_trackID), GF_ISOM_REF_OCR, src->OCRESID);
	}

	track = gf_isom_get_track_by_id(mp4, import.final_trackID);
	i=0;
	while ((d = gf_list_enum(src->extensionDescriptors, &i))) {
		Bool do_del = GF_FALSE;
		if (d->tag == GF_ODF_AUX_VIDEO_DATA) {
			gf_isom_add_user_data(mp4, track, GF_ISOM_BOX_TYPE_AUXV, 0, NULL, 0);
			do_del = GF_TRUE;
		}
		else if (d->tag == GF_ODF_GPAC_LANG) {
			gf_isom_add_desc_to_description(mp4, track, 1, d);
			do_del = GF_TRUE;

		}
		if (do_del) {
			gf_list_rem(src->extensionDescriptors, i-1);
			i--;
			gf_odf_desc_del(d);
		}
	}
	/*if desired delete input*/
	if (mux->delete_file) gf_delete_file(mux->file_name);
	return e;
}

static GF_Err gf_sm_import_stream_special(GF_SceneManager *ctx, GF_ESD *esd)
{
	GF_Err e;
	GF_MuxInfo *mux = gf_sm_get_mux_info(esd);
	if (!mux || !mux->file_name) return GF_OK;

	if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo
	        && (esd->decoderConfig->decoderSpecificInfo->tag==GF_ODF_TEXT_CFG_TAG)) return GF_OK;

	e = GF_OK;
	/*SRT/SUB BIFS import if text node unspecified*/
	if (mux->textNode) {
		if (mux->src_url) {
			char *src = gf_url_concatenate(mux->src_url, mux->file_name);
			if (src) {
				gf_free(mux->file_name);
				mux->file_name = src;
			}
		}
		e = gf_sm_import_bifs_subtitle(ctx, esd, mux);
		gf_sm_remove_mux_info(esd);
	}
	return e;
}

static GF_Err gf_sm_import_specials(GF_SceneManager *ctx)
{
	GF_Err e;
	u32 i, j, n, m, k;
	GF_AUContext *au;
	GF_StreamContext *sc;

	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		if (sc->streamType != GF_STREAM_OD) continue;
		j=0;
		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			GF_ODCom *com;
			k=0;
			while ((com = (GF_ODCom *) gf_list_enum(au->commands, &k))) {
				switch (com->tag) {
				case GF_ODF_OD_UPDATE_TAG:
				{
					GF_ObjectDescriptor *od;
					GF_ODUpdate *odU = (GF_ODUpdate *)com;
					n=0;
					while ((od = (GF_ObjectDescriptor *) gf_list_enum(odU->objectDescriptors, &n))) {
						GF_ESD *imp_esd;
						m=0;
						while ((imp_esd = (GF_ESD*)gf_list_enum(od->ESDescriptors, &m))) {
							e = gf_sm_import_stream_special(ctx, imp_esd);
							if (e != GF_OK) return e;
						}
					}
				}
				break;
				case GF_ODF_ESD_UPDATE_TAG:
				{
					GF_ESD *imp_esd;
					GF_ESDUpdate *esdU = (GF_ESDUpdate *)com;
					m=0;
					while ((imp_esd = (GF_ESD*)gf_list_enum(esdU->ESDescriptors, &m))) {
						e = gf_sm_import_stream_special(ctx, imp_esd);
						if (e != GF_OK) return e;
					}
				}
				break;
				}
			}
		}
	}
	return GF_OK;
}


#endif /*GPAC_DISABLE_MEDIA_IMPORT*/

/*locate stream in all OD updates/ESD updates (needed for systems tracks)*/
static GF_ESD *gf_sm_locate_esd(GF_SceneManager *ctx, u16 ES_ID)
{
	u32 i, j, n, m, k;
	GF_AUContext *au;
	GF_StreamContext *sc;
	if (!ES_ID) return NULL;

	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		if (sc->streamType != GF_STREAM_OD) continue;
		j=0;
		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			GF_ODCom *com;
			k=0;
			while ((com = (GF_ODCom *) gf_list_enum(au->commands, &k))) {
				switch (com->tag) {
				case GF_ODF_OD_UPDATE_TAG:
				{
					GF_ObjectDescriptor *od;
					GF_ODUpdate *odU = (GF_ODUpdate *)com;
					n=0;
					while ((od = (GF_ObjectDescriptor *) gf_list_enum(odU->objectDescriptors, &n))) {
						GF_ESD *imp_esd;
						m=0;
						while ((imp_esd = (GF_ESD*)gf_list_enum(od->ESDescriptors, &m))) {
							if (imp_esd->ESID == ES_ID) return imp_esd;
						}
					}
				}
				break;
				case GF_ODF_ESD_UPDATE_TAG:
				{
					GF_ESD *imp_esd;
					GF_ESDUpdate *esdU = (GF_ESDUpdate *)com;
					m=0;
					while ((imp_esd = (GF_ESD*)gf_list_enum(esdU->ESDescriptors, &m))) {
						if (imp_esd->ESID == ES_ID) return imp_esd;
					}
				}
				break;
				}
			}
		}
	}
	return NULL;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

static GF_Err gf_sm_encode_scene(GF_SceneManager *ctx, GF_ISOFile *mp4, GF_SMEncodeOptions *opts, u32 scene_type)
{
	u8 *data;
	Bool is_in_iod, delete_desc;
	u32 i, j, di, rate, init_offset, data_len, count, track, rap_delay, flags, rap_mode;
	u64 last_rap, dur, time_slice, avg_rate, prev_dts;
	GF_Err e;
	GF_InitialObjectDescriptor *iod;
	GF_AUContext *au;
	GF_ISOSample *samp;
	GF_StreamContext *sc;
	GF_ESD *esd;
	GF_MuxInfo *mux;
#ifndef GPAC_DISABLE_BIFS_ENC
	GF_BifsEncoder *bifs_enc;
#endif
#ifndef GPAC_DISABLE_LASER
	GF_LASeRCodec *lsr_enc;
#endif

	rap_mode = 0;
	if (opts && opts->rap_freq) {
		if (opts->flags & GF_SM_ENCODE_RAP_INBAND) rap_mode = 3;
		else if (opts->flags & GF_SM_ENCODE_RAP_SHADOW) rap_mode = 2;
		else rap_mode = 1;
	}

	e = GF_OK;
	iod = (GF_InitialObjectDescriptor *) ctx->root_od;
	/*if no iod check we only have one bifs*/
	if (!iod) {
		count = 0;
		i=0;
		while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
			if (sc->streamType == GF_STREAM_OD) count++;
		}
		if (!iod && count>1) return GF_NOT_SUPPORTED;
	}

	count = gf_list_count(ctx->streams);

	sc = NULL;

	flags = opts ? opts->flags : 0;
	delete_desc = 0;
	esd = NULL;


	/*configure streams*/
	j=0;
	for (i=0; i<count; i++) {
		sc = (GF_StreamContext*)gf_list_get(ctx->streams, i);
		esd = NULL;
		if (sc->streamType != GF_STREAM_SCENE) continue;
		/*NOT BIFS*/
		if (!scene_type && (sc->codec_id > 2) ) continue;
		/*NOT LASeR*/
		if (scene_type && (sc->codec_id != 0x09) ) continue;
		j++;
	}
	if (!j) {
		GF_Node *n = gf_sg_get_root_node(ctx->scene_graph);
		if (!n) return GF_OK;
#ifndef GPAC_DISABLE_LASER
		if ((scene_type==1) && (gf_node_get_tag(n)!=TAG_SVG_svg) ) return GF_OK;
#endif
		if ((scene_type==0) && (gf_node_get_tag(n)>GF_NODE_RANGE_LAST_X3D) ) return GF_OK;
	}

#ifndef GPAC_DISABLE_BIFS_ENC
	bifs_enc = NULL;
#endif
#ifndef GPAC_DISABLE_LASER
	lsr_enc = NULL;
#endif

	if (!scene_type) {
#ifndef GPAC_DISABLE_BIFS_ENC
		bifs_enc = gf_bifs_encoder_new(ctx->scene_graph);
		/*no streams defined, encode a RAP*/
		if (!j) {
			delete_desc = 0;
			esd = NULL;
			is_in_iod = 1;
			goto force_scene_rap;
		}
		gf_bifs_encoder_set_source_url(bifs_enc, opts->src_url);
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	if (scene_type==1) {
#ifndef GPAC_DISABLE_LASER
		lsr_enc = gf_laser_encoder_new(ctx->scene_graph);
		/*no streams defined, encode a RAP*/
		if (!j) {
			delete_desc = 0;
			esd = NULL;
			is_in_iod = 1;
			goto force_scene_rap;
		}
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	/*configure streams*/
	for (i=0; i<count; i++) {
		sc = (GF_StreamContext*)gf_list_get(ctx->streams, i);
		esd = NULL;
		if (sc->streamType != GF_STREAM_SCENE) continue;
		/*NOT BIFS*/
		if (!scene_type && (sc->codec_id > 2) ) continue;
		/*NOT LASeR*/
		if (scene_type && (sc->codec_id != 0x09) ) continue;

		delete_desc = 0;
		esd = NULL;
		is_in_iod = 1;
		if (iod) {
			is_in_iod = 0;
			j=0;
			while ((esd = (GF_ESD*)gf_list_enum(iod->ESDescriptors, &j))) {
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
		if (!esd && sc->ESID) esd = gf_sm_locate_esd(ctx, sc->ESID);

		au = NULL;

#ifndef GPAC_DISABLE_VRML
		/*special BIFS direct import from NHNT*/
		au = (GF_AUContext*)gf_list_get(sc->AUs, 0);
		if (gf_list_count(sc->AUs) == 1) {
			if (gf_list_count(au->commands) == 1) {
				GF_Command *com = (GF_Command *)gf_list_get(au->commands, 0);
				/*no root node, no protos (empty replace) - that's BIFS NHNT import*/
				if ((com->tag == GF_SG_SCENE_REPLACE) && !com->node && !gf_list_count(com->new_proto_list))
					au = NULL;
			}
		}

		/*sanity check: remove first command if it is REPLACE SCENE BY NULL*/
		if (au && !au->timing && !au->timing_sec && (gf_list_count(au->commands) > 1)) {
			GF_Command *com = (GF_Command *)gf_list_get(au->commands, 0);
			if (com->tag==GF_SG_SCENE_REPLACE) {
				if (!com->node && !gf_list_count(com->new_proto_list) ) {
					gf_list_rem(au->commands, 0);
					gf_sg_command_del(com);
				}
			}
		}
#endif

		if (!au && esd && !esd->URLString) {
			/*if not in IOD, the stream will be imported when encoding the OD stream*/
			if (!is_in_iod) continue;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			e = gf_sm_import_stream(ctx, mp4, esd, 0, NULL, 0);
#else
			e = GF_BAD_PARAM;
#endif
			if (e) goto exit;
			gf_sm_finalize_mux(mp4, esd, 0);
			gf_isom_add_track_to_root_od(mp4, gf_isom_get_track_by_id(mp4, esd->ESID));
			continue;
		}

force_scene_rap:
		if (!esd) {
			delete_desc = 1;
			esd = gf_odf_desc_esd_new(2);
			gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = NULL;
			esd->ESID = sc ? sc->ESID : 1;
			esd->decoderConfig->streamType = GF_STREAM_SCENE;
		}

		if (!esd->slConfig) esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		if (sc && sc->timeScale) esd->slConfig->timestampResolution = sc->timeScale;
		if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = 1000;

		if (!esd->decoderConfig) esd->decoderConfig = (GF_DecoderConfig*)gf_odf_desc_new(GF_ODF_DCD_TAG);
		esd->decoderConfig->streamType = GF_STREAM_SCENE;

		/*create track*/
		track = gf_isom_new_track(mp4, sc ? sc->ESID : 1, GF_ISOM_MEDIA_SCENE, esd->slConfig->timestampResolution);
		if (!track) {
			e = gf_isom_last_error(mp4);
			goto exit;
		}
		gf_isom_set_track_enabled(mp4, track, 1);
		if (sc) {
			if (!sc->ESID) sc->ESID = gf_isom_get_track_id(mp4, track);
			esd->ESID = sc->ESID;
		}

		/*BIFS setup*/
		if (!scene_type) {
#ifndef GPAC_DISABLE_BIFS_ENC
			GF_BIFSConfig *bcfg;
			Bool delete_bcfg = 0;

			if (!esd->decoderConfig->decoderSpecificInfo) {
				bcfg = (GF_BIFSConfig*)gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
				delete_bcfg = 1;
			} else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_BIFS_CFG_TAG) {
				bcfg = (GF_BIFSConfig *)esd->decoderConfig->decoderSpecificInfo;
			} else {
				bcfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
				delete_bcfg = 1;
			}
			/*update NodeIDbits and co*/
			/*nodeID bits shall include NULL node*/
			if (!bcfg->nodeIDbits || (bcfg->nodeIDbits<gf_get_bit_size(ctx->max_node_id)) )
				bcfg->nodeIDbits = gf_get_bit_size(ctx->max_node_id);

			if (!bcfg->routeIDbits || (bcfg->routeIDbits != gf_get_bit_size(ctx->max_route_id)) )
				bcfg->routeIDbits = gf_get_bit_size(ctx->max_route_id);

			if (!bcfg->protoIDbits || (bcfg->protoIDbits != gf_get_bit_size(ctx->max_proto_id)) )
				bcfg->protoIDbits = gf_get_bit_size(ctx->max_proto_id);

			if (!bcfg->elementaryMasks) {
				bcfg->pixelMetrics = ctx->is_pixel_metrics;
				bcfg->pixelWidth = ctx->scene_width;
				bcfg->pixelHeight = ctx->scene_height;
			}
			if (bcfg->useNames) flags |= GF_SM_ENCODE_USE_NAMES;

			/*this is for safety, otherwise some players may not understand NULL node*/
			if (!bcfg->nodeIDbits) bcfg->nodeIDbits = 1;
			gf_bifs_encoder_new_stream(bifs_enc, esd->ESID, bcfg, (flags & GF_SM_ENCODE_USE_NAMES) ? 1 : 0, 0);
			if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);
			/*create final BIFS config*/
			if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
			gf_bifs_encoder_get_config(bifs_enc, esd->ESID, &data, &data_len);
			esd->decoderConfig->decoderSpecificInfo->data = data;
			esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
			esd->decoderConfig->objectTypeIndication = gf_bifs_encoder_get_version(bifs_enc, esd->ESID);
#endif
		}
		/*LASeR setup*/
#ifndef GPAC_DISABLE_LASER
		if (scene_type==1) {
			GF_LASERConfig lsrcfg;

			if (!esd->decoderConfig->decoderSpecificInfo) {
				memset(&lsrcfg, 0, sizeof(GF_LASERConfig));
				lsrcfg.tag = GF_ODF_LASER_CFG_TAG;
			} else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_LASER_CFG_TAG) {
				memcpy(&lsrcfg, (GF_LASERConfig *)esd->decoderConfig->decoderSpecificInfo, sizeof(GF_LASERConfig));
			} else {
				gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &lsrcfg);
			}
			/*create final BIFS config*/
			if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);

			/*this is for safety, otherwise some players may not understand NULL node*/
			if (flags & GF_SM_ENCODE_USE_NAMES) lsrcfg.force_string_ids = 1;
			/*override of default*/
			if (opts) {
				if (opts->resolution) lsrcfg.resolution = opts->resolution;
				if (opts->coord_bits) lsrcfg.coord_bits = opts->coord_bits;
				/*by default use 2 extra bits for scale*/
				lsrcfg.scale_bits_minus_coord_bits = opts->scale_bits ? opts->scale_bits : 2;
			}

			gf_laser_encoder_new_stream(lsr_enc, esd->ESID , &lsrcfg);
			/*get final config*/
			gf_laser_encoder_get_config(lsr_enc, esd->ESID, &data, &data_len);

			esd->decoderConfig->decoderSpecificInfo->data = data;
			esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
			esd->decoderConfig->objectTypeIndication = GF_CODECID_LASER;
		}
#endif
		/*create stream description*/
		gf_isom_new_mpeg4_description(mp4, track, esd, NULL, NULL, &di);
		if (is_in_iod) {
			gf_isom_add_track_to_root_od(mp4, track);
			if (ctx->scene_width && ctx->scene_height)
				gf_isom_set_visual_info(mp4, track, di, ctx->scene_width, ctx->scene_height);
		}
		if (esd->URLString) continue;

		if (!sc) {
			samp = gf_isom_sample_new();
			samp->IsRAP = RAP;

#ifndef GPAC_DISABLE_BIFS_ENC
			if (bifs_enc)
				e = gf_bifs_encoder_get_rap(bifs_enc, &samp->data, &samp->dataLength);
#endif

#ifndef GPAC_DISABLE_LASER
			if (lsr_enc)
				e = gf_laser_encoder_get_rap(lsr_enc, &samp->data, &samp->dataLength);
#endif
			if (!e && samp->dataLength) e = gf_isom_add_sample(mp4, track, di, samp);
			gf_isom_sample_del(&samp);
			goto exit;
		}

		dur = 0;
		avg_rate = 0;
		esd->decoderConfig->bufferSizeDB = 0;
		esd->decoderConfig->maxBitrate = 0;
		rate = 0;
		time_slice = 0;
		last_rap = 0;
		rap_delay = 0;
		if (opts) rap_delay = opts->rap_freq * esd->slConfig->timestampResolution / 1000;

		prev_dts = 0;
		init_offset = 0;
		j=0;
		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			u32 samp_size;
			samp = gf_isom_sample_new();
			/*time in sec conversion*/
			if (au->timing_sec) au->timing = (u64) (au->timing_sec * esd->slConfig->timestampResolution + 0.0005);

			if (j==1) init_offset = (u32) au->timing;

			samp->DTS = au->timing - init_offset;
			if ((j>1) && (samp->DTS == prev_dts)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OD-SL] Same sample time %d for Access Unit %d and %d\n", au->timing, j, j-1));
				e = GF_BAD_PARAM;
				goto exit;
			}
			samp->IsRAP = au->flags & GF_SM_AU_RAP;
			if (samp->IsRAP) last_rap = au->timing;


#ifndef GPAC_DISABLE_BIFS_ENC
			if (bifs_enc)
				e = gf_bifs_encode_au(bifs_enc, sc->ESID, au->commands, &samp->data, &samp->dataLength);
#endif
#ifndef GPAC_DISABLE_LASER
			if (lsr_enc)
				e = gf_laser_encode_au(lsr_enc, sc->ESID, au->commands, 0, &samp->data, &samp->dataLength);
#endif

			samp_size = samp->dataLength;

			/*inband RAP */
			if (rap_mode==3) {
				/*current sample before or at the next rep - apply commands*/
				if (samp->DTS <= last_rap + rap_delay) {
					e = gf_sg_command_apply_list(ctx->scene_graph, au->commands, 0);
				}

				/*current sample is after or at next rap, insert rap*/
				while (samp->DTS >= last_rap + rap_delay) {
					GF_ISOSample *rap_sample = gf_isom_sample_new();

#ifndef GPAC_DISABLE_BIFS_ENC
					if (bifs_enc)
						e = gf_bifs_encoder_get_rap(bifs_enc, &rap_sample->data, &rap_sample->dataLength);
#endif

#ifndef GPAC_DISABLE_LASER
					if (lsr_enc)
						e = gf_laser_encoder_get_rap(lsr_enc, &rap_sample->data, &rap_sample->dataLength);
#endif

					rap_sample->DTS = last_rap + rap_delay;
					rap_sample->IsRAP = RAP;
					last_rap = rap_sample->DTS;


					if (!e) e = gf_isom_add_sample(mp4, track, di, rap_sample);
					if (samp_size < rap_sample->dataLength) samp_size = rap_sample->dataLength;

					gf_isom_sample_del(&rap_sample);
					/*same timing, don't add sample*/
					if (last_rap == samp->DTS) {
						if (samp->data) gf_free(samp->data);
						samp->data = NULL;
						samp->dataLength = 0;
					}
				}

				/*apply commands */
				if (samp->DTS > last_rap + rap_delay) {
					e = gf_sg_command_apply_list(ctx->scene_graph, au->commands, 0);
				}
			}

			/*carousel generation*/
			if (!e && (rap_mode == 1)) {
				if (samp->DTS - last_rap > rap_delay) {
					GF_ISOSample *car_samp = gf_isom_sample_new();
					u64 r_dts = samp->DTS;

					/*then get RAP*/
#ifndef GPAC_DISABLE_BIFS_ENC
					if (bifs_enc) {
						e = gf_bifs_encoder_get_rap(bifs_enc, &car_samp->data, &car_samp->dataLength);
						if (e) goto exit;
					}
#endif

#ifndef GPAC_DISABLE_LASER
					if (lsr_enc) {
						e = gf_laser_encoder_get_rap(lsr_enc, &car_samp->data, &car_samp->dataLength);
						if (e) goto exit;
					}
#endif
					car_samp->IsRAP = RAP_REDUNDANT;
					while (1) {
						car_samp->DTS = last_rap+rap_delay;
						if (car_samp->DTS==prev_dts) car_samp->DTS++;
						e = gf_isom_add_sample(mp4, track, di, car_samp);
						if (e) break;
						last_rap+=rap_delay;
						if (last_rap + rap_delay >= r_dts) break;
					}
					gf_isom_sample_del(&car_samp);
					if (e) goto exit;
				}
				if (samp->dataLength) {
					e = gf_isom_add_sample(mp4, track, di, samp);
					if (e) goto exit;
				}
				/*accumulate commmands*/
				e = gf_sg_command_apply_list(ctx->scene_graph, au->commands, 0);
			} else {
				/*if no commands don't add the AU*/
				if (!e && samp->dataLength) e = gf_isom_add_sample(mp4, track, di, samp);
			}

			dur = au->timing;
			avg_rate += samp_size;
			rate += samp_size;
			if (esd->decoderConfig->bufferSizeDB < samp_size) esd->decoderConfig->bufferSizeDB = samp_size;
			if (samp->DTS - time_slice > esd->slConfig->timestampResolution) {
				if (esd->decoderConfig->maxBitrate < rate) esd->decoderConfig->maxBitrate = rate;
				rate = 0;
				time_slice = samp->DTS;
			}

			prev_dts = samp->DTS;
			gf_isom_sample_del(&samp);
			if (e) goto exit;
		}

		if (dur) {
			esd->decoderConfig->avgBitrate = (u32) (avg_rate * esd->slConfig->timestampResolution * 8 / dur);
			esd->decoderConfig->maxBitrate *= 8;
		} else {
			esd->decoderConfig->avgBitrate = 0;
			esd->decoderConfig->maxBitrate = 0;
		}
		gf_isom_change_mpeg4_description(mp4, track, 1, esd);

		/*sync shadow generation*/
		if (rap_mode==2) {
			GF_AUContext *au;
			u32 au_count = gf_list_count(sc->AUs);
			last_rap = 0;
			for (j=0; j<au_count; j++) {
				au = (GF_AUContext *)gf_list_get(sc->AUs, j);
				e = gf_sg_command_apply_list(ctx->scene_graph, au->commands, 0);
				if (!j) continue;
				/*force a RAP shadow on last sample*/
				if ((au->timing - last_rap < rap_delay) && (j+1<au_count) ) continue;

				samp = gf_isom_sample_new();
				last_rap = samp->DTS = au->timing - init_offset;
				samp->IsRAP = RAP;
				/*RAP generation*/
#ifndef GPAC_DISABLE_BIFS_ENC
				if (bifs_enc)
					e = gf_bifs_encoder_get_rap(bifs_enc, &samp->data, &samp->dataLength);
#endif

				if (!e) e = gf_isom_add_sample_shadow(mp4, track, samp);
				gf_isom_sample_del(&samp);
				if (e) goto exit;
			}
		}

		/*if offset add edit list*/
		gf_sm_finalize_mux(mp4, esd, (u32) init_offset);
		gf_isom_set_last_sample_duration(mp4, track, 0);

		mux = gf_sm_get_mux_info(esd);
		if (mux && mux->duration) {
			u64 tot_dur = mux->duration * esd->slConfig->timestampResolution / 1000;
			u64 dur = gf_isom_get_media_duration(mp4, track);
			if (dur <= tot_dur)
				gf_isom_set_last_sample_duration(mp4, track, (u32) (tot_dur - dur));
		}

		if (delete_desc) {
			gf_odf_desc_del((GF_Descriptor *) esd);
			esd = NULL;
		}
	}

	/*to do - proper PL setup according to node used...*/
	gf_isom_set_pl_indication(mp4, GF_ISOM_PL_SCENE, 1);
	gf_isom_set_pl_indication(mp4, GF_ISOM_PL_GRAPHICS, 1);

exit:
#ifndef GPAC_DISABLE_BIFS_ENC
	if (bifs_enc) gf_bifs_encoder_del(bifs_enc);
#endif
#ifndef GPAC_DISABLE_LASER
	if (lsr_enc) gf_laser_encoder_del(lsr_enc);
#endif
	if (esd && delete_desc) gf_odf_desc_del((GF_Descriptor *) esd);
	return e;
}


static GF_Err gf_sm_encode_od(GF_SceneManager *ctx, GF_ISOFile *mp4, char *mediaSource, GF_SMEncodeOptions *opts)
{
	u32 i, j, n, m, rap_delay;
	GF_ESD *esd;
	GF_StreamContext *sc;
	GF_AUContext *au;
	u32 count, track, rate, di;
	u64 dur, time_slice, init_offset, avg_rate, last_rap, last_not_shadow, prev_dts;
	Bool is_in_iod, delete_desc, rap_inband, rap_shadow;
	GF_ISOSample *samp;
	GF_Err e;
	GF_ODCodec *codec, *rap_codec;
	GF_InitialObjectDescriptor *iod;

	gf_isom_set_pl_indication(mp4, GF_ISOM_PL_OD, 0xFE);

	iod = (GF_InitialObjectDescriptor *) ctx->root_od;
	count = 0;
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		if (sc->streamType == GF_STREAM_OD) count++;
	}
	/*no OD stream, nothing to do*/
	if (!count) return GF_OK;
	if (!iod && count>1) return GF_NOT_SUPPORTED;


	rap_inband = rap_shadow = 0;
	if (opts && opts->rap_freq) {
		if (opts->flags & GF_SM_ENCODE_RAP_INBAND) {
			rap_inband = 1;
		} else {
			rap_shadow = 1;
		}
	}

	esd = NULL;
	codec = rap_codec = NULL;
	delete_desc = 0;

	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		if (sc->streamType != GF_STREAM_OD) continue;

		delete_desc = 0;
		esd = NULL;
		is_in_iod = 1;
		if (iod) {
			is_in_iod = 0;
			j=0;
			while ((esd = (GF_ESD*)gf_list_enum(iod->ESDescriptors, &j))) {
				if (esd->decoderConfig->streamType != GF_STREAM_OD) {
					esd = NULL;
					continue;
				}
				if (!sc->ESID) sc->ESID = esd->ESID;
				if (sc->ESID == esd->ESID) {
					is_in_iod = 1;
					break;
				}
			}
		}
		if (!esd) esd = gf_sm_locate_esd(ctx, sc->ESID);
		if (!esd) {
			delete_desc = 1;
			esd = gf_odf_desc_esd_new(2);
			esd->ESID = sc->ESID;
			esd->decoderConfig->objectTypeIndication = GF_CODECID_OD_V1;
			esd->decoderConfig->streamType = GF_STREAM_OD;
		}

		/*create OD track*/
		if (!esd->slConfig) esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		if (sc->timeScale) esd->slConfig->timestampResolution = sc->timeScale;
		if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = 1000;
		track = gf_isom_new_track(mp4, sc->ESID, GF_ISOM_MEDIA_OD, esd->slConfig->timestampResolution);
		if (!sc->ESID) sc->ESID = gf_isom_get_track_id(mp4, track);
		if (!esd->decoderConfig->objectTypeIndication) esd->decoderConfig->objectTypeIndication = GF_CODECID_OD_V1;
		gf_isom_set_track_enabled(mp4, track, 1);
		/*no DSI required*/
		/*create stream description*/
		gf_isom_new_mpeg4_description(mp4, track, esd, NULL, NULL, &di);
		/*add to root OD*/
		if (is_in_iod) gf_isom_add_track_to_root_od(mp4, track);

		codec = gf_odf_codec_new();

		if (rap_inband || rap_shadow) {
			rap_codec = gf_odf_codec_new();
		}


		dur = avg_rate = 0;
		esd->decoderConfig->bufferSizeDB = 0;
		esd->decoderConfig->maxBitrate = 0;
		rate = 0;
		time_slice = 0;
		init_offset = 0;
		last_rap = 0;
		rap_delay = 0;
		last_not_shadow = 0;
		prev_dts = 0;
		if (opts) rap_delay = opts->rap_freq * esd->slConfig->timestampResolution / 1000;


		/*encode all samples and perform import */
		j=0;
		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			GF_ODCom *com;
			u32 k = 0;
			Bool rap_aggregated=0;

			if (au->timing_sec) au->timing = (u64) (au->timing_sec * esd->slConfig->timestampResolution + 0.0005);
			else au->timing_sec = (double) ((s64) (au->timing / esd->slConfig->timestampResolution));

			while ((com = gf_list_enum(au->commands, &k))) {

				/*only updates commandes need to be parsed for import*/
				switch (com->tag) {
				case GF_ODF_OD_UPDATE_TAG:
				{
					GF_ObjectDescriptor *od;
					GF_ODUpdate *odU = (GF_ODUpdate *)com;
					n=0;
					while ((od = (GF_ObjectDescriptor *) gf_list_enum(odU->objectDescriptors, &n))) {
						GF_ESD *imp_esd;
						m=0;
						while ((imp_esd = (GF_ESD*)gf_list_enum(od->ESDescriptors, &m))) {
							/*do not import scene and OD streams*/
							if (imp_esd->decoderConfig) {
								switch (imp_esd->decoderConfig->streamType) {
								case GF_STREAM_SCENE:
									/*import AFX streams, but not others*/
									if (imp_esd->decoderConfig->objectTypeIndication==GF_CODECID_AFX)
										break;
									continue;
								case GF_STREAM_OD:
									continue;
								default:
									break;
								}
							}

							switch (imp_esd->tag) {
							case GF_ODF_ESD_TAG:
#ifndef GPAC_DISABLE_MEDIA_IMPORT
								e = gf_sm_import_stream(ctx, mp4, imp_esd, au->timing_sec, mediaSource, au->flags & GF_SM_AU_RAP);
#else
								e = GF_BAD_PARAM;
#endif
								if (e) {
									GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] cannot import stream %d (error %s)\n", imp_esd->ESID, gf_error_to_string(e)));
									goto err_exit;
								}
								gf_sm_finalize_mux(mp4, imp_esd, 0);
								break;
							case GF_ODF_ESD_REF_TAG:
							case GF_ODF_ESD_INC_TAG:
								break;
							default:
								GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] Invalid descriptor in OD%d.ESDescr\n", od->objectDescriptorID));
								e = GF_BAD_PARAM;
								goto err_exit;
								break;
							}
						}
					}
				}
				break;
				case GF_ODF_ESD_UPDATE_TAG:
				{
					GF_ESD *imp_esd;
					GF_ESDUpdate *esdU = (GF_ESDUpdate *)com;
					m=0;
					while ((imp_esd = (GF_ESD*)gf_list_enum(esdU->ESDescriptors, &m))) {
						switch (imp_esd->tag) {
						case GF_ODF_ESD_TAG:
#ifndef GPAC_DISABLE_MEDIA_IMPORT
							e = gf_sm_import_stream(ctx, mp4, imp_esd, au->timing_sec, mediaSource, au->flags & GF_SM_AU_RAP);
#else
							e = GF_BAD_PARAM;
#endif
							if (e) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] cannot import stream %d (error %s)\n", imp_esd->ESID, gf_error_to_string(e)));
								gf_odf_com_del(&com);
								goto err_exit;
							}
							gf_sm_finalize_mux(mp4, imp_esd, 0);
							break;
						case GF_ODF_ESD_REF_TAG:
						case GF_ODF_ESD_INC_TAG:
							break;
						default:
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISO File Encode] Invalid descriptor in ESDUpdate (OD %d)\n", esdU->ODID));
							e = GF_BAD_PARAM;
							goto err_exit;
							break;
						}
					}
				}
				break;
				}

				/*add to codec*/
				gf_odf_codec_add_com(codec, com);
			}

			if (j==1) init_offset = au->timing;

			samp = gf_isom_sample_new();
			samp->DTS = au->timing - init_offset;
			samp->IsRAP = au->flags & GF_SM_AU_RAP;

			e = gf_odf_codec_encode(codec, 0);
			if (e) goto err_exit;

			if ((j>1) && (samp->DTS == prev_dts)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[OD-SL] Same sample time %d for Access Unit %d and %d\n", au->timing, j, j-1));
				e = GF_BAD_PARAM;
				goto err_exit;
			}
			e = gf_odf_codec_get_au(codec, &samp->data, &samp->dataLength);

			last_not_shadow = samp->DTS;
			if (samp->IsRAP) {
				last_rap = samp->DTS;
			} else if (rap_inband) {
				while (samp->DTS >= rap_delay + last_rap) {
					GF_ISOSample *rap_sample = gf_isom_sample_new();
					rap_sample->DTS = last_rap + rap_delay;
					rap_sample->IsRAP = RAP;

					if (samp->DTS == last_rap + rap_delay) {
						GF_ODCom *com;
						u32 k = 0;
						while ((com = gf_list_enum(au->commands, &k))) {
							e = gf_odf_codec_apply_com(rap_codec, com);
							if (e) goto err_exit;
						}
						rap_aggregated = 1;
					}

					e = gf_odf_codec_encode(rap_codec, 2);
					if (e) goto err_exit;
					e = gf_odf_codec_get_au(rap_codec, &rap_sample->data, &rap_sample->dataLength);

					if (!e) e = gf_isom_add_sample(mp4, track, di, rap_sample);
					last_rap = rap_sample->DTS;

					avg_rate += rap_sample->dataLength;
					rate += rap_sample->dataLength;

					if (rap_sample->DTS==samp->DTS) {
						if (samp->data) gf_free(samp->data);
						samp->data = NULL;
						samp->dataLength = 0;
					}
					gf_isom_sample_del(&rap_sample);
				}
			}

			/*manage carousel/rap generation - we must do it after the RAP has been generated*/
			if (rap_codec && !rap_aggregated) {
				GF_ODCom *com;
				u32 k = 0;
				while ((com = gf_list_enum(au->commands, &k))) {
					e = gf_odf_codec_apply_com(rap_codec, com);
					if (e) goto err_exit;
				}
			}


			if (!e && samp->dataLength) e = gf_isom_add_sample(mp4, track, di, samp);

			dur = au->timing - init_offset;
			avg_rate += samp->dataLength;
			rate += samp->dataLength;
			if (esd->decoderConfig->bufferSizeDB<samp->dataLength)
				esd->decoderConfig->bufferSizeDB = samp->dataLength;
			if (samp->DTS - time_slice > esd->slConfig->timestampResolution) {
				if (esd->decoderConfig->maxBitrate < rate) esd->decoderConfig->maxBitrate = rate;
				rate = 0;
				time_slice = samp->DTS;
			}


			if (rap_shadow && (samp->DTS - last_rap >= rap_delay)) {
				last_rap = samp->DTS;
				e = gf_odf_codec_encode(rap_codec, 2);
				if (e) goto err_exit;
				if (samp->data) gf_free(samp->data);
				samp->data = NULL;
				samp->dataLength = 0;
				e = gf_odf_codec_get_au(rap_codec, &samp->data, &samp->dataLength);
				if (e) goto err_exit;
				samp->IsRAP = RAP;
				e = gf_isom_add_sample_shadow(mp4, track, samp);
				if (e) goto err_exit;

				last_not_shadow = 0;
			}

			prev_dts = samp->DTS;

			gf_isom_sample_del(&samp);
			if (e) goto err_exit;
		}

		if (dur) {
			esd->decoderConfig->avgBitrate = (u32) (avg_rate * esd->slConfig->timestampResolution * 8 / dur);
			esd->decoderConfig->maxBitrate *= 8;
		} else {
			esd->decoderConfig->avgBitrate = 0;
			esd->decoderConfig->maxBitrate = 0;
		}
		gf_isom_change_mpeg4_description(mp4, track, 1, esd);

		gf_sm_finalize_mux(mp4, esd, (u32) init_offset);
		if (delete_desc) {
			gf_odf_desc_del((GF_Descriptor *) esd);
			esd = NULL;
		}
		esd = NULL;
		if (gf_isom_get_sample_count(mp4, track))
			gf_isom_set_last_sample_duration(mp4, track, 0);

		if (rap_codec) {
			if (last_not_shadow && rap_shadow) {
				samp = gf_isom_sample_new();
				samp->DTS = last_not_shadow;
				samp->IsRAP = RAP;
				e = gf_odf_codec_encode(rap_codec, 2);
				if (!e) e = gf_odf_codec_get_au(rap_codec, &samp->data, &samp->dataLength);
				if (!e) e = gf_isom_add_sample_shadow(mp4, track, samp);
				if (e) goto err_exit;
				gf_isom_sample_del(&samp);
			}

			gf_odf_codec_del(rap_codec);
			rap_codec = NULL;
		}
	}
	e = gf_isom_set_pl_indication(mp4, GF_ISOM_PL_OD, 1);

err_exit:
	if (codec) gf_odf_codec_del(codec);
	if (rap_codec) gf_odf_codec_del(rap_codec);
	if (esd && delete_desc) gf_odf_desc_del((GF_Descriptor *) esd);
	return e;
}

GF_EXPORT
GF_Err gf_sm_encode_to_file(GF_SceneManager *ctx, GF_ISOFile *mp4, GF_SMEncodeOptions *opts)
{
	u32 i, count;
	GF_Descriptor *desc;
	GF_Err e;
	if (!ctx->scene_graph) return GF_BAD_PARAM;
	if (ctx->root_od && (ctx->root_od->tag != GF_ODF_IOD_TAG) && (ctx->root_od->tag != GF_ODF_OD_TAG)) return GF_BAD_PARAM;


	/*set MP4 brands*/
	gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_MP42, 1);
	gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_MP41, 1);

	/*import specials, that is input remapping to BIFS*/
#ifndef GPAC_DISABLE_MEDIA_IMPORT
	e = gf_sm_import_specials(ctx);
#else
	e = GF_BAD_PARAM;
#endif
	if (e) return e;


	/*encode BIFS*/
	e = gf_sm_encode_scene(ctx, mp4, opts, 0);
	if (e) return e;
	/*encode LASeR*/
	e = gf_sm_encode_scene(ctx, mp4, opts, 1);
	if (e) return e;
	/*then encode OD to setup all streams*/
	e = gf_sm_encode_od(ctx, mp4, opts ? opts->mediaSource : NULL, opts);
	if (e) return e;

	/*store iod*/
	if (ctx->root_od) {
		gf_isom_set_root_od_id(mp4, ctx->root_od->objectDescriptorID);
		if (ctx->root_od->URLString) gf_isom_set_root_od_url(mp4, ctx->root_od->URLString);
		count = gf_list_count(ctx->root_od->extensionDescriptors);
		for (i=0; i<count; i++) {
			desc = (GF_Descriptor *) gf_list_get(ctx->root_od->extensionDescriptors, i);
			gf_isom_add_desc_to_root_od(mp4, desc);
		}
		count = gf_list_count(ctx->root_od->IPMP_Descriptors);
		for (i=0; i<count; i++) {
			desc = (GF_Descriptor *) gf_list_get(ctx->root_od->IPMP_Descriptors, i);
			gf_isom_add_desc_to_root_od(mp4, desc);
		}
		count = gf_list_count(ctx->root_od->OCIDescriptors);
		for (i=0; i<count; i++) {
			desc = (GF_Descriptor *) gf_list_get(ctx->root_od->OCIDescriptors, i);
			gf_isom_add_desc_to_root_od(mp4, desc);
		}
		if (ctx->root_od->tag==GF_ODF_IOD_TAG) {
			GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor*)ctx->root_od;
			if (iod->IPMPToolList) gf_isom_add_desc_to_root_od(mp4, (GF_Descriptor *) iod->IPMPToolList);
		}
		/*we assume all ESs described in bt/xmt input are used*/
	}

	/*set PLs*/
	if (ctx->root_od && ctx->root_od->tag==GF_ODF_IOD_TAG) {
		GF_InitialObjectDescriptor *iod =  (GF_InitialObjectDescriptor *)ctx->root_od;
		gf_isom_set_pl_indication(mp4, GF_ISOM_PL_SCENE, iod->scene_profileAndLevel);
		gf_isom_set_pl_indication(mp4, GF_ISOM_PL_GRAPHICS, iod->graphics_profileAndLevel);
	} else {
		gf_isom_set_pl_indication(mp4, GF_ISOM_PL_SCENE, 0xFE);
		gf_isom_set_pl_indication(mp4, GF_ISOM_PL_GRAPHICS, 0xFE);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_SCENE_ENCODER*/
