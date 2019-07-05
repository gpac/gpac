/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / ISOBMFF reader filter
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

#include "isoffin.h"
#include <gpac/iso639.h>
#include <gpac/base_coding.h>
#include <gpac/media_tools.h>

#ifndef GPAC_DISABLE_ISOM

#if 0	//deprecated - we need to rework chapter information to deal with static chapters and chapter tracks
void isor_emulate_chapters(GF_ISOFile *file, GF_InitialObjectDescriptor *iod)
{
	GF_Segment *prev_seg;
	u64 prev_start;
	u64 start;
	u32 i, count;
	if (!iod || gf_list_count(iod->OCIDescriptors)) return;
	count = gf_isom_get_chapter_count(file, 0);
	if (!count) return;

	prev_seg = NULL;
	start = prev_start = 0;
	for (i=0; i<count; i++) {
		const char *name;
		GF_Segment *seg;
		gf_isom_get_chapter(file, 0, i+1, &start, &name);
		seg = (GF_Segment *) gf_odf_desc_new(GF_ODF_SEGMENT_TAG);
		seg->startTime = (Double) (s64) start;
		seg->startTime /= 1000;
		seg->SegmentName = gf_strdup(name);
		gf_list_add(iod->OCIDescriptors, seg);
		if (prev_seg) {
			prev_seg->Duration = (Double) (s64) (start - prev_start);
			prev_seg->Duration /= 1000;
		} else if (start) {
			prev_seg = (GF_Segment *) gf_odf_desc_new(GF_ODF_SEGMENT_TAG);
			prev_seg->startTime = 0;
			prev_seg->Duration = (Double) (s64) (start);
			prev_seg->Duration /= 1000;
			gf_list_insert(iod->OCIDescriptors, prev_seg, 0);
		}
		prev_seg = seg;
		prev_start = start;
	}
	if (prev_seg) {
		start = 1000*gf_isom_get_duration(file);
		start /= gf_isom_get_timescale(file);
		if (start>prev_start) {
			prev_seg->Duration = (Double) (s64) (start - prev_start);
			prev_seg->Duration /= 1000;
		}
	}
}
#endif

static void isor_declare_track(ISOMReader *read, ISOMChannel *ch, u32 track, u32 stsd_idx, u32 streamtype, Bool use_iod)
{
	u32 w, h, sr, nb_ch, nb_bps, codec_id, depends_on_id, esid, avg_rate, max_rate, buffer_size, sample_count, max_size, nb_refs, exp_refs, base_track, audio_fmt;
	GF_ESD *an_esd;
	const char *mime, *encoding, *stxtcfg, *namespace, *schemaloc;
	u8 *tk_template;
	u32 tk_template_size;
	GF_Language *lang_desc = NULL;
	Bool external_base=GF_FALSE;
	Bool has_scalable_layers = GF_FALSE;
	u8 *dsi = NULL, *enh_dsi = NULL;
	u32 dsi_size = 0, enh_dsi_size = 0;
	Double track_dur=0;
	GF_FilterPid *pid;
	u32 srd_id=0, srd_indep=0, srd_x=0, srd_y=0, srd_w=0, srd_h=0;
	u32 base_tile_track=0;
	Bool srd_full_frame=GF_FALSE;
	u32 mtype, m_subtype;
	GF_GenericSampleDescription *udesc = NULL;
	GF_Err e;
	Bool first_config = GF_FALSE;


	esid = depends_on_id = avg_rate = max_rate = buffer_size = 0;
	mime = encoding = stxtcfg = namespace = schemaloc = NULL;


	m_subtype = gf_isom_get_media_subtype(read->mov, track, stsd_idx);

	u32 ocr_es_id;

	audio_fmt = 0;
	ocr_es_id = 0;
	an_esd = gf_media_map_esd(read->mov, track, stsd_idx);
	if (an_esd) {
		if (an_esd->decoderConfig->streamType==GF_STREAM_INTERACT) {
			gf_odf_desc_del((GF_Descriptor *)an_esd);
			return;
		}
		streamtype = an_esd->decoderConfig->streamType;
		if (an_esd->decoderConfig->objectTypeIndication < GF_CODECID_LAST_MPEG4_MAPPING) {
			codec_id = gf_codecid_from_oti(streamtype, an_esd->decoderConfig->objectTypeIndication);
		} else {
			codec_id = an_esd->decoderConfig->objectTypeIndication;
		}
		ocr_es_id = an_esd->OCRESID;
		depends_on_id = an_esd->dependsOnESID;
		lang_desc = an_esd->langDesc;
		an_esd->langDesc = NULL;
		esid = an_esd->ESID;

		if (an_esd->decoderConfig->decoderSpecificInfo && an_esd->decoderConfig->decoderSpecificInfo->data) {
			dsi = an_esd->decoderConfig->decoderSpecificInfo->data;
			dsi_size = an_esd->decoderConfig->decoderSpecificInfo->dataLength;
			an_esd->decoderConfig->decoderSpecificInfo->data = NULL;
			an_esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
		}

		gf_odf_desc_del((GF_Descriptor *)an_esd);
	} else {
		Bool load_default = GF_FALSE;
		lang_desc = (GF_Language *) gf_odf_desc_new(GF_ODF_LANG_TAG);
		gf_isom_get_media_language(read->mov, track, &lang_desc->full_lang_code);
		esid = gf_isom_get_track_id(read->mov, track);

		if (!streamtype) streamtype = gf_codecid_type(m_subtype);
		codec_id = 0;

		switch (m_subtype) {
		case GF_ISOM_SUBTYPE_STXT:
		case GF_ISOM_SUBTYPE_METT:
		case GF_ISOM_SUBTYPE_SBTT:
		case GF_ISOM_MEDIA_SUBT:

			codec_id = GF_CODECID_SIMPLE_TEXT;
			gf_isom_stxt_get_description(read->mov, track, stsd_idx, &mime, &encoding, &stxtcfg);
			break;
		case GF_ISOM_SUBTYPE_STPP:
			codec_id = GF_CODECID_SUBS_XML;
			gf_isom_xml_subtitle_get_description(read->mov, track, stsd_idx, &namespace, &schemaloc, &mime);
			break;
		case GF_ISOM_SUBTYPE_METX:
			codec_id = GF_CODECID_META_XML;
			gf_isom_xml_subtitle_get_description(read->mov, track, stsd_idx, &namespace, &schemaloc, &mime);
			break;
		case GF_ISOM_SUBTYPE_WVTT:
			codec_id = GF_CODECID_WEBVTT;
			stxtcfg = gf_isom_get_webvtt_config(read->mov, track, stsd_idx);
			break;
		case GF_ISOM_SUBTYPE_MJP2:
			codec_id = GF_CODECID_J2K;
			gf_isom_get_jp2_config(read->mov, track, stsd_idx, &dsi, &dsi_size);
			break;
		case GF_ISOM_SUBTYPE_HVT1:
			codec_id = GF_CODECID_HEVC_TILES;
			gf_isom_get_reference(read->mov, track, GF_ISOM_REF_TBAS, 1, &base_tile_track);
			if (base_tile_track) {
				depends_on_id = gf_isom_get_track_id(read->mov, base_tile_track);
			}
			gf_isom_get_tile_info(read->mov, track, 1, NULL, &srd_id, &srd_indep, &srd_full_frame, &srd_x, &srd_y, &srd_w, &srd_h);
			break;
		case GF_ISOM_SUBTYPE_TEXT:
		case GF_ISOM_SUBTYPE_TX3G:
		{
			GF_TextSampleDescriptor *txtcfg = NULL;
			codec_id = GF_CODECID_TX3G;
			e = gf_isom_get_text_description(read->mov, track, stsd_idx, &txtcfg);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d unable to fetch TX3G config\n", track));
			}
			if (txtcfg) {
				gf_odf_tx3g_write(txtcfg, &dsi, &dsi_size);
				gf_odf_desc_del((GF_Descriptor *) txtcfg);
			}
		}
			break;

		case GF_ISOM_SUBTYPE_FLAC:
			codec_id = GF_CODECID_FLAC;
			gf_isom_flac_config_get(read->mov, track, stsd_idx, &dsi, &dsi_size);
			break;

		case GF_QT_SUBTYPE_TWOS:
		case GF_QT_SUBTYPE_SOWT:
		case GF_QT_SUBTYPE_FL32:
		case GF_QT_SUBTYPE_FL64:
		case GF_QT_SUBTYPE_IN24:
		case GF_QT_SUBTYPE_IN32:
			codec_id = GF_CODECID_RAW;
			audio_fmt = gf_audio_fmt_from_isobmf(m_subtype);
			break;
		default:
			codec_id = gf_codec_id_from_isobmf(m_subtype);
			if (!codec_id)
				load_default = GF_TRUE;
			break;
		}

		if (load_default) {
			if (!codec_id) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d type %s not natively handled\n", track, gf_4cc_to_str(m_subtype) ));

				codec_id = m_subtype;
			}
			udesc = gf_isom_get_generic_sample_description(read->mov, track, stsd_idx);
			if (udesc) {
				dsi = udesc->extension_buf;
				dsi_size = udesc->extension_buf_size;
				udesc->extension_buf = NULL;
				udesc->extension_buf_size = 0;
			}
		}
	}
	if (!streamtype || !codec_id) {
		if (udesc) gf_free(udesc);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Failed to %s pid for track %d, couldnot extract codec/streamtype info\n", ch ? "update" : "create", track));
		if (lang_desc) gf_odf_desc_del((GF_Descriptor *)lang_desc);
		if (dsi) gf_free(dsi);
		return;
	}

	//first setup, creation of PID and channel
	if (!ch) {
		first_config = GF_TRUE;

		gf_isom_get_reference(read->mov, track, GF_ISOM_REF_BASE, 1, &base_track);

		if (base_track) {
			u32 base_subtype=0;
			if (read->smode==MP4DMX_SINGLE)
				depends_on_id = 0;

			switch (m_subtype) {
			case GF_ISOM_SUBTYPE_LHV1:
			case GF_ISOM_SUBTYPE_LHE1:
				base_subtype = gf_isom_get_media_subtype(read->mov, base_track, stsd_idx);
				switch (base_subtype) {
				case GF_ISOM_SUBTYPE_HVC1:
				case GF_ISOM_SUBTYPE_HEV1:
				case GF_ISOM_SUBTYPE_HVC2:
				case GF_ISOM_SUBTYPE_HEV2:
					break;
				default:
					external_base=GF_TRUE;
					break;
				}
			}
			if (external_base) {
				depends_on_id = gf_isom_get_track_id(read->mov, base_track);
				has_scalable_layers = GF_TRUE;
			} else {
				switch (gf_isom_get_hevc_lhvc_type(read->mov, track, stsd_idx)) {
				case GF_ISOM_HEVCTYPE_HEVC_LHVC:
				case GF_ISOM_HEVCTYPE_LHVC_ONLY:
					has_scalable_layers = GF_TRUE;
					break;
				//this is likely temporal sublayer of base
				case GF_ISOM_HEVCTYPE_HEVC_ONLY:
					has_scalable_layers = GF_FALSE;
					if (gf_isom_get_reference_count(read->mov, track, GF_ISOM_REF_SCAL)<=0) {
						depends_on_id = gf_isom_get_track_id(read->mov, base_track);
					}
					break;
				}
			}
		} else {
			switch (gf_isom_get_hevc_lhvc_type(read->mov, track, stsd_idx)) {
			case GF_ISOM_HEVCTYPE_HEVC_LHVC:
			case GF_ISOM_HEVCTYPE_LHVC_ONLY:
				has_scalable_layers = GF_TRUE;
				break;
			}
		}

		if (base_track && !ocr_es_id) {
			ocr_es_id = gf_isom_get_track_id(read->mov, base_track);
		}
		if (!ocr_es_id) ocr_es_id = esid;

		//OK declare PID
		pid = gf_filter_pid_new(read->filter);
		if (read->pid)
			gf_filter_pid_copy_properties(pid, read->pid);

		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(esid));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(ocr_es_id));
		if (depends_on_id && (depends_on_id != esid))
			gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(depends_on_id));

		//MPEG-4 systems present
		if (use_iod)
			gf_filter_pid_set_property(pid, GF_PROP_PID_ESID, &PROP_UINT(esid));

		if (gf_isom_is_track_in_root_od(read->mov, track)) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE));
		}

		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(streamtype));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT( gf_isom_get_media_timescale(read->mov, track) ) );

		//create our channel
		ch = isor_create_channel(read, pid, track, 0, (codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);

		if (lang_desc) {
			char *lang=NULL;
			gf_isom_get_media_language(read->mov, track, &lang);
			//s32 idx = gf_lang_find(lang);
			gf_filter_pid_set_property(pid, GF_PROP_PID_LANGUAGE, &PROP_STRING( lang ));
			if (lang) gf_free(lang);
			gf_odf_desc_del((GF_Descriptor *)lang_desc);
		}


		if (has_scalable_layers)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SCALABLE, &PROP_BOOL(GF_TRUE));

		if (gf_isom_get_reference_count(read->mov, track, GF_ISOM_REF_SABT)>0) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TILE_BASE, &PROP_BOOL(GF_TRUE));
		}

		if (srd_w && srd_h) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(srd_x, srd_y) );
			if (base_tile_track) {
				gf_isom_get_visual_info(read->mov, base_tile_track, stsd_idx, &w, &h);
				if (w && h) {
					gf_filter_pid_set_property(pid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(w, h) );
				}
			}
		}

		for (exp_refs=0; exp_refs<3; exp_refs++) {
			u32 rtype = (exp_refs==2) ? GF_ISOM_REF_TBAS : exp_refs ? GF_ISOM_REF_SABT : GF_ISOM_REF_SCAL;
			const char *rname = (exp_refs==2) ? "isom:tbas" : exp_refs ? "isom:sabt" : "isom:scal";
			if (!exp_refs && (codec_id==GF_CODECID_LHVC))
				continue;

			nb_refs = gf_isom_get_reference_count(read->mov, track, rtype);
			if (nb_refs) {
				u32 j;
				GF_PropertyValue prop;
				prop.type = GF_PROP_UINT_LIST;
				prop.value.uint_list.nb_items = nb_refs;
				prop.value.uint_list.vals = gf_malloc(sizeof(u32)*nb_refs);
				for (j=0; j<nb_refs; j++) {
					gf_isom_get_reference(read->mov, track, rtype, j+1, &prop.value.uint_list.vals[j] );
				}
				gf_filter_pid_set_property_str(pid, rname, &prop);
				gf_free(prop.value.uint_list.vals);
			}
		}

		ch->duration = gf_isom_get_track_duration(read->mov, ch->track);
		if (!ch->duration) {
			ch->duration = gf_isom_get_duration(read->mov);
		}
		if (!ch->has_edit_list) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC_INT((s32) gf_isom_get_media_duration(read->mov, ch->track) - (s32) ch->ts_offset, ch->time_scale));
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC_INT((s32) ch->duration, read->time_scale));
		}

		sample_count = gf_isom_get_sample_count(read->mov, ch->track);
		gf_filter_pid_set_property(pid, GF_PROP_PID_NB_FRAMES, &PROP_UINT(sample_count));

		if (sample_count && (streamtype==GF_STREAM_VISUAL)) {
			u64 mdur = gf_isom_get_media_duration(read->mov, track);
			mdur /= sample_count;
			gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT(ch->time_scale, (u32) mdur));
		}

		track_dur = (Double) (s64) ch->duration;
		track_dur /= read->time_scale;
		//move channel duration in media timescale
		ch->duration = (u32) (track_dur * ch->time_scale);


		//set stream subtype
		mtype = gf_isom_get_media_type(read->mov, track);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SUBTYPE, &PROP_UINT(mtype) );

		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MEDIA_DATA_SIZE, &PROP_LONGUINT(gf_isom_get_media_data_size(read->mov, track) ) );

		w = gf_isom_get_constant_sample_size(read->mov, track);
		if (w)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_FRAME_SIZE, &PROP_UINT(w));

		gf_filter_pid_set_property(pid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_REWIND) );


		GF_PropertyValue brands;
		brands.type = GF_PROP_UINT_LIST;
		u32 major_brand=0;
		gf_isom_get_brand_info(read->mov, &major_brand, NULL, &brands.value.uint_list.nb_items);
		brands.value.uint_list.vals = (u32 *) gf_isom_get_brands(read->mov);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_BRANDS, &brands);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_MBRAND, &PROP_UINT(major_brand) );

		max_size = gf_isom_get_max_sample_size(read->mov, ch->track);
		if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_MAX_FRAME_SIZE, &PROP_UINT(max_size) );

		max_size = gf_isom_get_avg_sample_size(read->mov, ch->track);
		if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_AVG_FRAME_SIZE, &PROP_UINT(max_size) );

		max_size = gf_isom_get_max_sample_delta(read->mov, ch->track);
		if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_MAX_TS_DELTA, &PROP_UINT(max_size) );

		max_size = gf_isom_get_max_sample_cts_offset(read->mov, ch->track);
		if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_MAX_CTS_OFFSET, &PROP_UINT(max_size) );

		max_size = gf_isom_get_sample_const_duration(read->mov, ch->track);
		if (max_size) gf_filter_pid_set_property(pid, GF_PROP_PID_CONSTANT_DURATION, &PROP_UINT(max_size) );

		u32 media_pl=0;
		if (streamtype==GF_STREAM_VISUAL) {
			media_pl = gf_isom_get_pl_indication(read->mov, GF_ISOM_PL_VISUAL);
		} else if (streamtype==GF_STREAM_AUDIO) {
			media_pl = gf_isom_get_pl_indication(read->mov, GF_ISOM_PL_AUDIO);
		}
		if (media_pl && (media_pl!=0xFF) ) gf_filter_pid_set_property(pid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(media_pl) );

#if !defined(GPAC_DISABLE_ISOM_WRITE)
		e = gf_isom_get_track_template(read->mov, ch->track, &tk_template, &tk_template_size);
		if (e == GF_OK) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize track box: %s\n", gf_error_to_string(e) ));
		}

		e = gf_isom_get_trex_template(read->mov, ch->track, &tk_template, &tk_template_size);
		if (e == GF_OK) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TREX_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		}

		e = gf_isom_get_raw_user_data(read->mov, &tk_template, &tk_template_size);
		if (e==GF_OK) {
			if (tk_template_size)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_UDTA, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize moov UDTA box: %s\n", gf_error_to_string(e) ));
		}
#endif

		u32 w, h, i;
		s32 tx, ty;
		s16 l;
		gf_isom_get_track_layout_info(read->mov, ch->track, &w, &h, &tx, &ty, &l);
		if (w && h) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_TRANS_X, &PROP_SINT(tx) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_TRANS_Y, &PROP_SINT(ty) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ZORDER, &PROP_SINT(l) );
		}
		if (codec_id==GF_CODECID_TX3G) {
			u32 m_w = w;
			u32 m_h = h;
			for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
				switch (gf_isom_get_media_type(read->mov, i+1)) {
				case GF_ISOM_MEDIA_SCENE:
				case GF_ISOM_MEDIA_VISUAL:
				case GF_ISOM_MEDIA_AUXV:
				case GF_ISOM_MEDIA_PICT:
					gf_isom_get_track_layout_info(read->mov, i+1, &w, &h, &tx, &ty, &l);
					if (w>m_w) m_w = w;
					if (h>m_h) m_h = h;
					break;
				default:
					break;
				}
			}
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_WIDTH_MAX, &PROP_UINT(m_w) );
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HEIGHT_MAX, &PROP_UINT(m_h) );
			char *tx3g_config_sdp = NULL;
			for (i=0; i<gf_isom_get_sample_description_count(read->mov, ch->track); i++) {
				u8 *tx3g;
				char buffer[2000];
				u32 l1;
				u32 tx3g_len, len;
				gf_isom_text_get_encoded_tx3g(read->mov, ch->track, i+1, GF_RTP_TX3G_SIDX_OFFSET, &tx3g, &tx3g_len);
				len = gf_base64_encode(tx3g, tx3g_len, buffer, 2000);
				gf_free(tx3g);
				buffer[len] = 0;

				l1 = tx3g_config_sdp ? (u32) strlen(tx3g_config_sdp) : 0;
				tx3g_config_sdp = gf_realloc(tx3g_config_sdp, len+3+l1);
				tx3g_config_sdp[l1] = 0;
				if (i) strcat(tx3g_config_sdp, ", ");
				strcat(tx3g_config_sdp, buffer);
			}
			if (tx3g_config_sdp) {
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_STRING_NO_COPY(tx3g_config_sdp) );
			}
		}


		u32 tag_len;
		const u8 *tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_NAME, &tag, &tag_len)==GF_OK)
			gf_filter_pid_set_info_str(ch->pid, "info:name", &PROP_STRING(tag) );

		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_ARTIST, &tag, &tag_len)==GF_OK)
			gf_filter_pid_set_info_str(ch->pid, "info:artist", &PROP_STRING(tag) );

		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_ALBUM, &tag, &tag_len)==GF_OK)
			gf_filter_pid_set_info_str(ch->pid, "info:album", &PROP_STRING(tag) );

		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COMMENT, &tag, &tag_len)==GF_OK)
			gf_filter_pid_set_info_str(ch->pid, "info:comment", &PROP_STRING(tag) );

		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) {
			u16 tk_n = (tag[2]<<8)|tag[3];
			u16 tk_all = (tag[4]<<8)|tag[5];
			gf_filter_pid_set_info_str(ch->pid, "info:track", &PROP_FRAC_INT(tk_n, tk_all) );
		}
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COMPOSER, &tag, &tag_len)==GF_OK)
			gf_filter_pid_set_info_str(ch->pid, "info:composer", &PROP_STRING(tag) );
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_WRITER, &tag, &tag_len)==GF_OK)
			gf_filter_pid_set_info_str(ch->pid, "info:writer", &PROP_STRING(tag) );

		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_GENRE, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
			} else {
				/*com->info.genre = (tag[0]<<8) | tag[1];*/
			}
		}

		if (!gf_sys_is_test_mode()) {
			const char *hdlr = NULL;
			gf_isom_get_handler_name(read->mov, ch->track, &hdlr);
			if (hdlr)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_HANDLER, &PROP_STRING(hdlr));

			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_FLAGS, &PROP_UINT( gf_isom_get_track_flags(read->mov, ch->track) ));

			if (streamtype==GF_STREAM_VISUAL) {
				GF_PropertyValue p;
				u32 vals[9];
				memset(vals, 0, sizeof(u32)*9);
				memset(&p, 0, sizeof(GF_PropertyValue));
				p.type = GF_PROP_UINT_LIST;
				p.value.uint_list.nb_items = 9;
				p.value.uint_list.vals = vals;
				gf_isom_get_track_matrix(read->mov, ch->track, vals);
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_MATRIX, &p);
			}
		}
	}

	//update decoder configs
	ch->check_avc_ps = ch->check_hevc_ps = GF_FALSE;
	if (ch->avcc) gf_odf_avc_cfg_del(ch->avcc);
	ch->avcc = NULL;
	if (ch->hvcc) gf_odf_hevc_cfg_del(ch->hvcc);
	ch->hvcc = NULL;

	if (read->smode != MP4DMX_SINGLE) {
		if ((codec_id==GF_CODECID_LHVC) || (codec_id==GF_CODECID_HEVC)) {
			Bool signal_lhv = (read->smode==MP4DMX_SPLIT) ? GF_TRUE : GF_FALSE;
			GF_HEVCConfig *hvcc = gf_isom_hevc_config_get(read->mov, track, stsd_idx);
			GF_HEVCConfig *lhcc = gf_isom_lhvc_config_get(read->mov, track, stsd_idx);

			if (hvcc || lhcc) {
				if (dsi) gf_free(dsi);
				dsi = NULL;
				//no base layer config
				if (!hvcc) signal_lhv = GF_TRUE;

				if (signal_lhv && lhcc) {
					if (hvcc) {
						hvcc->is_lhvc = GF_FALSE;
						gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
					}
					lhcc->is_lhvc = GF_TRUE;
					gf_odf_hevc_cfg_write(lhcc, &enh_dsi, &enh_dsi_size);
					codec_id = GF_CODECID_LHVC;
				} else {
					if (hvcc) {
						hvcc->is_lhvc = GF_FALSE;
						gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
					}
					if (lhcc) {
						lhcc->is_lhvc = GF_TRUE;
						gf_odf_hevc_cfg_write(lhcc, &enh_dsi, &enh_dsi_size);
					}
					codec_id = GF_CODECID_HEVC;
				}
			}
			if (hvcc) gf_odf_hevc_cfg_del(hvcc);
			if (lhcc) gf_odf_hevc_cfg_del(lhcc);

		}
		if ((codec_id==GF_CODECID_AVC) || (codec_id==GF_CODECID_SVC) || (codec_id==GF_CODECID_MVC)) {
			Bool is_mvc = GF_FALSE;
			Bool signal_svc = (read->smode==MP4DMX_SPLIT) ? GF_TRUE : GF_FALSE;
			GF_AVCConfig *avcc = gf_isom_avc_config_get(read->mov, track, stsd_idx);
			GF_AVCConfig *svcc = gf_isom_svc_config_get(read->mov, track, stsd_idx);
			if (!svcc) {
				svcc = gf_isom_mvc_config_get(read->mov, track, stsd_idx);
				is_mvc = GF_TRUE;
			}

			if (avcc || svcc) {
				if (dsi) gf_free(dsi);
				dsi = NULL;
				//no base layer config
				if (!avcc) signal_svc = GF_TRUE;

				if (signal_svc && svcc) {
					if (avcc) {
						gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
					}
					gf_odf_avc_cfg_write(svcc, &enh_dsi, &enh_dsi_size);
					codec_id = is_mvc ? GF_CODECID_MVC : GF_CODECID_SVC;
				} else {
					if (avcc) {
						gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
					}
					if (svcc) {
						gf_odf_avc_cfg_write(svcc, &enh_dsi, &enh_dsi_size);
					}
					codec_id = GF_CODECID_AVC;
				}
			}
			if (avcc) gf_odf_avc_cfg_del(avcc);
			if (svcc) gf_odf_avc_cfg_del(svcc);
		}
	}

	//all stsd specific init/reconfig
	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CODECID, &PROP_UINT(codec_id));
	if (dsi) {
		ch->dsi_crc = gf_crc_32(dsi, dsi_size);
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size));
	}
	if (enh_dsi) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, &PROP_DATA_NO_COPY(enh_dsi, enh_dsi_size));
	}
	if (audio_fmt) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(audio_fmt));
		if (codec_id == GF_CODECID_RAW) {
			gf_isom_enable_raw_pack(read->mov, track, read->frame_size);
		}
	}

	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_CONFIG_IDX, &PROP_UINT(stsd_idx) );

	w = h = 0;
	gf_isom_get_visual_info(read->mov, track, stsd_idx, &w, &h);
	if (w && h) {
		u32 hspace, vspace;
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_WIDTH, &PROP_UINT(w));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HEIGHT, &PROP_UINT(h));

		gf_isom_get_pixel_aspect_ratio(read->mov, track, stsd_idx, &hspace, &vspace);
		if (hspace != vspace)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAR, &PROP_FRAC_INT(hspace, vspace) );
	}
	sr = nb_ch = nb_bps = 0;
	gf_isom_get_audio_info(read->mov,track, stsd_idx, &sr, &nb_ch, &nb_bps);
	if (sr && nb_ch) {
		u32 d1, d2;
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));

		//to remove once we deprecate master
		if (!gf_sys_is_test_mode())
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_BPS, &PROP_UINT(nb_bps));

		if (first_config ) {
			d1 = gf_isom_get_sample_duration(read->mov, ch->track, 1);
			d2 = gf_isom_get_sample_duration(read->mov, ch->track, 2);
			if (d1 && d2 && (d1==d2)) {
				d1 *= sr;
				d1 /= ch->time_scale;
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT(d1));
			}
		}
	}

	gf_isom_get_bitrate(read->mov, ch->track, stsd_idx, &avg_rate, &max_rate, &buffer_size);

	if (!avg_rate) {
		if (first_config && ch->duration) {
			u64 avg_rate = 8 * gf_isom_get_media_data_size(read->mov, ch->track);
			avg_rate = (u64) (avg_rate / track_dur);
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BITRATE, &PROP_UINT((u32) avg_rate));
		}
	} else {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BITRATE, &PROP_UINT((u32) avg_rate));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MAXRATE, &PROP_UINT((u32) max_rate));
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DBSIZE, &PROP_UINT((u32) buffer_size));
	}

	if (mime) gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(mime) );
	if (encoding) gf_filter_pid_set_property_str(ch->pid, "meta:encoding", &PROP_STRING(encoding) );
	if (namespace) gf_filter_pid_set_property_str(ch->pid, "meta:xmlns", &PROP_STRING(namespace) );
	if (schemaloc) gf_filter_pid_set_property_str(ch->pid, "meta:schemaloc", &PROP_STRING(schemaloc) );

	gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_SUBTYPE, &PROP_UINT(m_subtype) );
	if (stxtcfg) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char *)stxtcfg, (u32) strlen(stxtcfg) ));


#if !defined(GPAC_DISABLE_ISOM_WRITE)
	tk_template=NULL;
	tk_template_size=0;
	e = gf_isom_get_stsd_template(read->mov, ch->track, stsd_idx, &tk_template, &tk_template_size);
	if (e == GF_OK) {
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_STSD_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize stsd box: %s\n", gf_error_to_string(e) ));
	}
#endif

	if (codec_id == GF_CODECID_DIMS) {
		GF_DIMSDescription dims;
		memset(&dims, 0, sizeof(GF_DIMSDescription));

		gf_isom_get_dims_description(read->mov, ch->track, stsd_idx, &dims);
		gf_filter_pid_set_property_str(ch->pid, "dims:profile", &PROP_UINT(dims.profile));
		gf_filter_pid_set_property_str(ch->pid, "dims:level", &PROP_UINT(dims.level));
		gf_filter_pid_set_property_str(ch->pid, "dims:pathComponents", &PROP_UINT(dims.pathComponents));
		gf_filter_pid_set_property_str(ch->pid, "dims:fullRequestHost", &PROP_BOOL(dims.fullRequestHost));
		gf_filter_pid_set_property_str(ch->pid, "dims:streamType", &PROP_BOOL(dims.streamType));
		gf_filter_pid_set_property_str(ch->pid, "dims:redundant", &PROP_BOOL(dims.containsRedundant));
		if (dims.content_script_types)
			gf_filter_pid_set_property_str(ch->pid, "dims:scriptTypes", &PROP_STRING(dims.content_script_types));
		if (dims.textEncoding)
			gf_filter_pid_set_property_str(ch->pid, "meta:encoding", &PROP_STRING(dims.textEncoding));
		if (dims.contentEncoding)
			gf_filter_pid_set_property_str(ch->pid, "meta:content_encoding", &PROP_STRING(dims.contentEncoding));
		if (dims.xml_schema_loc)
			gf_filter_pid_set_property_str(ch->pid, "meta:schemaloc", &PROP_STRING(dims.xml_schema_loc));
		if (dims.mime_type)
			gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(dims.mime_type));
	}
	else if (codec_id==GF_CODECID_AVC)
		ch->check_avc_ps = GF_TRUE;
	else if (codec_id==GF_CODECID_HEVC)
		ch->check_hevc_ps = GF_TRUE;

	if (udesc) {
		gf_filter_pid_set_property_str(ch->pid, "codec_vendor", &PROP_UINT(udesc->vendor_code));
		gf_filter_pid_set_property_str(ch->pid, "codec_version", &PROP_UINT(udesc->version));
		gf_filter_pid_set_property_str(ch->pid, "codec_revision", &PROP_UINT(udesc->revision));
		gf_filter_pid_set_property_str(ch->pid, "compressor_name", &PROP_STRING(udesc->compressor_name));
		gf_filter_pid_set_property_str(ch->pid, "temporal_quality", &PROP_UINT(udesc->temporal_quality));
		gf_filter_pid_set_property_str(ch->pid, "spatial_quality", &PROP_UINT(udesc->spatial_quality));
		if (udesc->h_res) {
			gf_filter_pid_set_property_str(ch->pid, "hres", &PROP_UINT(udesc->h_res));
			gf_filter_pid_set_property_str(ch->pid, "vres", &PROP_UINT(udesc->v_res));
		} else if (udesc->nb_channels) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(udesc->nb_channels));
			switch (udesc->bits_per_sample) {
			case 8:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_U8));
				break;
			case 16:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
				break;
			case 24:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S24));
				break;
			case 32:
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S32));
				break;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d unsupported audio bit depth %d\n", track, udesc->bits_per_sample ));
				break;
			}
		}
		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BIT_DEPTH_Y, &PROP_UINT(udesc->depth));

		gf_free(udesc);
	}


	if (ch->check_avc_ps) {
		ch->avcc = gf_isom_avc_config_get(ch->owner->mov, ch->track, ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
	}
	else if (ch->check_hevc_ps) {
		ch->hvcc = gf_isom_hevc_config_get(ch->owner->mov, ch->track, ch->last_sample_desc_index ? ch->last_sample_desc_index : 1);
	}
}

void isor_update_channel_config(ISOMChannel *ch)
{
	isor_declare_track(ch->owner, ch, ch->track, ch->last_sample_desc_index, GF_STREAM_UNKNOWN, GF_FALSE);

}

GF_Err isor_declare_objects(ISOMReader *read)
{
	const u8 *tag;
	u32 tlen;
	u32 i, count, j, track_id;
	Bool highest_stream;
	Bool single_media_found = GF_FALSE;
	Bool use_iod = GF_FALSE;
	GF_Err e;
	Bool isom_contains_video = GF_FALSE;
	GF_Descriptor *od = gf_isom_get_root_od(read->mov);
	if (od && gf_list_count(((GF_ObjectDescriptor*)od)->ESDescriptors)) {
		use_iod = GF_TRUE;
	}
	if (od) gf_odf_desc_del(od);

	/*TODO
	 check for alternate tracks
    */
	count = gf_isom_get_track_count(read->mov);
	for (i=0; i<count; i++) {
		u32 mtype, m_subtype, streamtype, stsd_idx;

		mtype = gf_isom_get_media_type(read->mov, i+1);

		if (read->tkid) {
			u32 for_id=0;
			if (sscanf(read->tkid, "%d", &for_id)) {
				u32 id = gf_isom_get_track_id(read->mov, i+1);
				if (id != for_id) continue;
			} else if (!strcmp(read->tkid, "audio")) {
				if (mtype!=GF_ISOM_MEDIA_AUDIO) continue;
			} else if (!strcmp(read->tkid, "video")) {
				if (mtype!=GF_ISOM_MEDIA_VISUAL) continue;
			} else if (strlen(read->tkid)==4) {
				u32 t = GF_4CC(read->tkid[0], read->tkid[1], read->tkid[2], read->tkid[3]);
				if (mtype!=t) continue;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[IsoMedia] Bad format for tkid option %s, no match\n", read->tkid));
				return GF_BAD_PARAM;
			}
		}

		switch (mtype) {
		case GF_ISOM_MEDIA_AUDIO:
			streamtype = GF_STREAM_AUDIO;
			break;
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_QTVR:
			streamtype = GF_STREAM_VISUAL;
			isom_contains_video = GF_TRUE;
			break;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_SUBPIC:
		case GF_ISOM_MEDIA_MPEG_SUBT:
		case GF_ISOM_MEDIA_CLOSED_CAPTION:
			streamtype = GF_STREAM_TEXT;
			break;
		case GF_ISOM_MEDIA_FLASH:
		case GF_ISOM_MEDIA_DIMS:
		case GF_ISOM_MEDIA_SCENE:
			streamtype = GF_STREAM_SCENE;
			break;
		case GF_ISOM_MEDIA_OD:
			streamtype = GF_STREAM_OD;
			break;
		case GF_ISOM_MEDIA_META:
		case GF_ISOM_MEDIA_TIMECODE:
			streamtype = GF_STREAM_METADATA;
			break;
		/*hint tracks are never exported*/
		case GF_ISOM_MEDIA_HINT:
			continue;
		default:
			if (!read->allt) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d type %s not supported, ignoring track - you may retry by specifying allt option\n", i+1, gf_4cc_to_str(mtype) ));
				continue;
			}
			streamtype = GF_STREAM_UNKNOWN;
			break;
		}

		if (!read->alltk && !read->tkid && !gf_isom_is_track_enabled(read->mov, i+1)) {
			if (count>1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d is disabled, ignoring track - you may retry by specifying allt option\n", i+1));
				continue;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d is disabled but single track in file, considering it enabled\n", i+1 ));
			}
		}

		stsd_idx = read->stsd ? read->stsd : 1;
		//some subtypes are not declared as readable objects
		m_subtype = gf_isom_get_media_subtype(read->mov, i+1, stsd_idx);
		switch (m_subtype) {
		case GF_ISOM_SUBTYPE_HVT1:
			if (read->smode == MP4DMX_SINGLE)
				continue;

			break;
		default:
			break;
		}

		/*we declare only the highest video track (i.e the track we play)*/
		highest_stream = GF_TRUE;
		track_id = gf_isom_get_track_id(read->mov, i+1);
		if (read->play_only_track_id && (read->play_only_track_id != track_id)) continue;

		if (read->play_only_first_media) {
			if (read->play_only_first_media != mtype) continue;
			if (single_media_found) continue;
			single_media_found = GF_TRUE;
		}

		for (j = 0; j < count; j++) {
			if (gf_isom_has_track_reference(read->mov, j+1, GF_ISOM_REF_SCAL, track_id) > 0) {
				highest_stream = GF_FALSE;
				break;
			}
			if (gf_isom_has_track_reference(read->mov, j+1, GF_ISOM_REF_BASE, track_id) > 0) {
				highest_stream = GF_FALSE;
				break;
			}
		}

		if ((read->smode==MP4DMX_SINGLE) && (gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_VISUAL) && !highest_stream)
			continue;


		isor_declare_track(read, NULL, i+1, stsd_idx, streamtype, use_iod);

		if (read->tkid)
			break;
	}

	if (!read->tkid) {
		/*declare image items*/
		count = gf_isom_get_meta_item_count(read->mov, GF_TRUE, 0);
		for (i=0; i<count; i++) {
			if (! isor_declare_item_properties(read, NULL, i+1))
				continue;

			if (read->itt) break;
		}
	}
	
	/*if cover art, declare a video pid*/
	if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COVER_ART, &tag, &tlen)==GF_OK) {

		/*write cover data*/
		assert(!(tlen & 0x80000000));
		tlen &= 0x7FFFFFFF;

		if (read->expart && !isom_contains_video) {
			GF_FilterPid *cover_pid=NULL;
			gf_filter_pid_set_property(cover_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );

			e = gf_filter_pid_raw_new(read->filter, NULL, NULL, NULL, NULL, (char *) tag, tlen, GF_FALSE, &cover_pid);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[MP3Dmx] error setting up video pid for cover art: %s\n", gf_error_to_string(e) ));
			}
			if (cover_pid) {
				u8 *out_buffer;
				GF_FilterPacket *dst_pck;
				gf_filter_pid_set_name(cover_pid, "CoverArt");
				dst_pck = gf_filter_pck_new_alloc(cover_pid, tlen, &out_buffer);
				gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
				memcpy(out_buffer, tag, tlen);
				gf_filter_pck_send(dst_pck);
				gf_filter_pid_set_eos(cover_pid);
			}
		}
	}
	return GF_OK;
}

Bool isor_declare_item_properties(ISOMReader *read, ISOMChannel *ch, u32 item_idx)
{
	GF_ImageItemProperties props;
	GF_FilterPid *pid;
	GF_ESD *esd;
	u32 item_id=0;
	const char *item_name, *item_mime_type, *item_encoding;

	if (item_idx>gf_isom_get_meta_item_count(read->mov, GF_TRUE, 0))
		return GF_FALSE;

	item_name = item_mime_type = item_encoding = NULL;
	gf_isom_get_meta_item_info(read->mov, GF_TRUE, 0, item_idx, &item_id, NULL, NULL, NULL, NULL, NULL, &item_name, &item_mime_type, &item_encoding);

	if (!item_id) return GF_FALSE;

	gf_isom_get_meta_image_props(read->mov, GF_TRUE, 0, item_id, &props);

	esd = gf_media_map_item_esd(read->mov, item_id);
	if (!esd) return GF_FALSE;

	//OK declare PID
	if (!ch)
		pid = gf_filter_pid_new(read->filter);
	else
		pid = ch->pid;

	if (read->pid)
		gf_filter_pid_copy_properties(pid, read->pid);

	gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(esd->ESID));
	if (read->itemid)
		gf_filter_pid_set_property(pid, GF_PROP_PID_ITEM_ID, &PROP_UINT(item_id));
		
	//TODO: no support for LHEVC images
	//gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(esd->dependsOnESID));

	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(esd->decoderConfig->streamType));
	gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(esd->decoderConfig->objectTypeIndication));
	gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000));
	if (esd->decoderConfig->decoderSpecificInfo) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength));

		esd->decoderConfig->decoderSpecificInfo->data=NULL;
		esd->decoderConfig->decoderSpecificInfo->dataLength=0;
	}

	if (props.width && props.height) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(props.width));
		gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(props.height));
	}
	if (props.hidden) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_HIDDEN, &PROP_BOOL(props.hidden));
	}
	gf_odf_desc_del((GF_Descriptor *)esd);

	if (gf_isom_get_meta_primary_item_id(read->mov, GF_TRUE, 0) == item_id) {
		gf_filter_pid_set_property(pid, GF_PROP_PID_PRIMARY_ITEM, &PROP_BOOL(GF_TRUE));
	} else {
		gf_filter_pid_set_property(pid, GF_PROP_PID_PRIMARY_ITEM, &PROP_BOOL(GF_FALSE));
	}

	gf_filter_pid_set_property_str(pid, "meta:mime", item_mime_type ? &PROP_STRING(item_mime_type) : NULL );
	gf_filter_pid_set_property_str(pid, "meta:name", item_name ? &PROP_STRING(item_name) : NULL );
	gf_filter_pid_set_property_str(pid, "meta:encoding", item_encoding ? &PROP_STRING(item_encoding) : NULL );

	if (!ch) {
		/*ch = */isor_create_channel(read, pid, 0, item_id, GF_FALSE);
	}
	return GF_TRUE;
}

#endif /*GPAC_DISABLE_ISOM*/



