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
#include <gpac/media_tools.h>

#ifndef GPAC_DISABLE_ISOM

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



void isor_declare_objects(ISOMReader *read)
{
	ISOMChannel *ch;
#ifdef FILTER_FIXME
	const char *tag;
	u32 tlen;
#endif
	u32 i, count, ocr_es_id, base_track, j, track_id;
	Bool highest_stream;
	Bool single_media_found = GF_FALSE;
	Bool use_iod = GF_FALSE;
	GF_Err e;
	GF_Descriptor *od = gf_isom_get_root_od(read->mov);
	if (od && gf_list_count(((GF_ObjectDescriptor*)od)->ESDescriptors)) {
		use_iod = GF_TRUE;
	}
	if (od) gf_odf_desc_del(od);

	/*TODO
	 add support for multiple stream descriptions
	 check for alternate tracks
    */
	count = gf_isom_get_track_count(read->mov);
	for (i=0; i<count; i++) {
		u32 w, h, sr, nb_ch, streamtype, codec_id, depends_on_id, esid, avg_rate, sample_count, max_size;
		GF_ESD *an_esd;
		const char *mime, *encoding, *stxtcfg, *namespace, *schemaloc;
		char *tk_template;
		u32 tk_template_size;
		GF_Language *lang_desc = NULL;
		Bool external_base=GF_FALSE;
		Bool has_scalable_layers = GF_FALSE;
		char *dsi = NULL;
		u32 dsi_size = 0;
		Double track_dur=0;
		GF_FilterPid *pid;
		u32 mtype, m_subtype;
		GF_GenericSampleDescription *udesc = NULL;

		if (!gf_isom_is_track_enabled(read->mov, i+1))
			continue;

		esid = depends_on_id = avg_rate = 0;
		mime = encoding = stxtcfg = namespace = schemaloc = NULL;

		mtype = gf_isom_get_media_type(read->mov, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_AUDIO:
			streamtype = GF_STREAM_AUDIO;
			break;
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_QTVR:
			streamtype = GF_STREAM_VISUAL;
			break;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_SUBPIC:
		case GF_ISOM_MEDIA_MPEG_SUBT:
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
			streamtype = GF_STREAM_METADATA;
			break;
		case GF_ISOM_MEDIA_HINT:
			continue;
		default:
			if (!read->alltracks) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d type %s not supported, ignoring track - you may retry by specifying alltracks options\n", i+1, gf_4cc_to_str(mtype) ));
				continue;
			}
			streamtype = GF_STREAM_UNKNOWN;
			break;
		}
		//some subtypes are not declared as readable objects
		m_subtype = gf_isom_get_media_subtype(read->mov, i+1, 1);
		switch (m_subtype) {
		case GF_ISOM_SUBTYPE_HVT1:
			continue;
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

		if ((gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_VISUAL) && !highest_stream)
			continue;

		ocr_es_id = 0;
		an_esd = gf_media_map_esd(read->mov, i+1);
		if (an_esd) {
			if (an_esd->decoderConfig->streamType==GF_STREAM_INTERACT) {
				gf_odf_desc_del((GF_Descriptor *)an_esd);
				continue;
			}
			streamtype = an_esd->decoderConfig->streamType;
			codec_id = an_esd->decoderConfig->objectTypeIndication;
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
			gf_isom_get_media_language(read->mov, i+1, &lang_desc->full_lang_code);

			if (!streamtype) streamtype = gf_codecid_type(m_subtype);
			codec_id = 0;

			switch (m_subtype) {
			case GF_ISOM_SUBTYPE_3GP_AMR:
				codec_id = GF_CODECID_AMR;
				break;
			case GF_ISOM_SUBTYPE_3GP_AMR_WB:
				codec_id = GF_CODECID_AMR_WB;
				break;
			case GF_ISOM_SUBTYPE_3GP_H263:
			case GF_ISOM_SUBTYPE_H263:
				codec_id = GF_CODECID_H263;
				break;
			case GF_ISOM_SUBTYPE_XDVB:
				codec_id = GF_CODECID_MPEG2_MAIN;
				break;
			case GF_ISOM_MEDIA_FLASH:
				codec_id = GF_CODECID_FLASH;
				break;
			case GF_ISOM_SUBTYPE_AC3:
				codec_id = GF_CODECID_AC3;
				break;
			case GF_ISOM_SUBTYPE_EC3:
				codec_id = GF_CODECID_EAC3;
				break;
			case GF_ISOM_SUBTYPE_MP3:
				codec_id = GF_CODECID_MPEG_AUDIO;
				break;
			case GF_ISOM_SUBTYPE_JPEG:
				codec_id = GF_CODECID_JPEG;
				break;
			case GF_ISOM_SUBTYPE_PNG:
				codec_id = GF_CODECID_PNG;
				break;
			case GF_ISOM_SUBTYPE_JP2K:
				codec_id = GF_CODECID_J2K;
				break;
			case GF_ISOM_SUBTYPE_STXT:
				codec_id = GF_CODECID_SIMPLE_TEXT;
				gf_isom_stxt_get_description(read->mov, i+1, 1, &mime, &encoding, &stxtcfg);
				break;
			case GF_ISOM_SUBTYPE_METT:
				gf_isom_stxt_get_description(read->mov, i+1, 1, &mime, &encoding, &stxtcfg);
				codec_id = GF_CODECID_META_TEXT;
				break;
			case GF_ISOM_SUBTYPE_METX:
				codec_id = GF_CODECID_META_XML;
				break;
			case GF_ISOM_SUBTYPE_SBTT:
				codec_id = GF_CODECID_SUBS_TEXT;
				break;
			case GF_ISOM_SUBTYPE_STPP:
				codec_id = GF_CODECID_SUBS_XML;
				gf_isom_xml_subtitle_get_description(read->mov, i+1, 1, &namespace, &schemaloc, &mime);
				break;
			case GF_ISOM_SUBTYPE_WVTT:
				codec_id = GF_CODECID_WEBVTT;
				stxtcfg = gf_isom_get_webvtt_config(read->mov, i+1, 1);
				break;
			case GF_ISOM_SUBTYPE_3GP_DIMS:
				codec_id = GF_CODECID_DIMS;
				break;
			case GF_ISOM_SUBTYPE_TEXT:
			case GF_ISOM_SUBTYPE_TX3G:
			{
				GF_TextSampleDescriptor *txtcfg = NULL;
				codec_id = GF_CODECID_TX3G;
				e = gf_isom_get_text_description(read->mov, i+1, 1, &txtcfg);
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d unable to fetch TX3G config\n", i+1));
				}
				if (txtcfg) {
					gf_odf_tx3g_write(txtcfg, &dsi, &dsi_size);
					gf_odf_desc_del((GF_Descriptor *) txtcfg);
				}
			}
				break;
			default:
				load_default = GF_TRUE;
				break;
			}
			
			if (load_default) {
				if (!codec_id) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d type %s not natively handled\n", i+1, gf_4cc_to_str(m_subtype) ));

					codec_id = m_subtype;
				}
				udesc = gf_isom_get_generic_sample_description(read->mov, i+1, 1);
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
			continue;
		}

		gf_isom_get_reference(read->mov, i+1, GF_ISOM_REF_BASE, 1, &base_track);
			
		if (base_track) {
			u32 base_subtype=0;
			switch (m_subtype) {
			case GF_ISOM_SUBTYPE_LHV1:
			case GF_ISOM_SUBTYPE_LHE1:
				base_subtype = gf_isom_get_media_subtype(read->mov, base_track, 1);
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
				switch (gf_isom_get_hevc_lhvc_type(read->mov, i+1, 1)) {
				case GF_ISOM_HEVCTYPE_HEVC_LHVC:
				case GF_ISOM_HEVCTYPE_LHVC_ONLY:
					has_scalable_layers = GF_TRUE;
					break;
				//this is likely temporal sublayer of base
				case GF_ISOM_HEVCTYPE_HEVC_ONLY:
					has_scalable_layers = GF_FALSE;
					if (gf_isom_get_reference_count(read->mov, i+1, GF_ISOM_REF_SCAL)<=0) {
						depends_on_id = gf_isom_get_track_id(read->mov, base_track);
					}
					break;
				}
			}
		} else {
			switch (gf_isom_get_hevc_lhvc_type(read->mov, i+1, 1)) {
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

		if (gf_isom_is_track_in_root_od(read->mov, i+1)) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE));
		}

		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(streamtype));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(codec_id));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT( gf_isom_get_media_timescale(read->mov, i+1) ) );
		if (dsi) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size));
		}
		if (lang_desc) {
			char *lang=NULL;
			s32 idx;
			gf_isom_get_media_language(read->mov, i+1, &lang);
			idx = gf_lang_find(lang);
			if (idx>=0) {
				gf_filter_pid_set_property(pid, GF_PROP_PID_LANGUAGE, &PROP_NAME( (char *) gf_lang_get_name(idx) ));
				gf_free(lang);
			} else {
				gf_filter_pid_set_property(pid, GF_PROP_PID_LANGUAGE, &PROP_STRING( lang ));
			}
			gf_odf_desc_del((GF_Descriptor *)lang_desc);
		}
		if (avg_rate) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, &PROP_UINT(avg_rate));
		}

		w = h = 0;
		gf_isom_get_visual_info(read->mov, i+1, 1, &w, &h);
		if (w && h) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(w));
			gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(h));
		}
		sr = nb_ch = 0;
		gf_isom_get_audio_info(read->mov, i+1, 1, &sr, &nb_ch, NULL);
		if (sr && nb_ch) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
			gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
		}
		if (has_scalable_layers)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SCALABLE, &PROP_BOOL(GF_TRUE));

		ch = isor_create_channel(read, pid, i+1, 0);

		ch->duration = gf_isom_get_track_duration(read->mov, ch->track);
		if (!ch->duration) {
			ch->duration = gf_isom_get_duration(read->mov);
		}
		gf_filter_pid_set_info(pid, GF_PROP_PID_DURATION, &PROP_FRAC_INT((s32) ch->duration, read->time_scale));
		sample_count = gf_isom_get_sample_count(read->mov, ch->track);
		gf_filter_pid_set_info(pid, GF_PROP_PID_NB_FRAMES, &PROP_UINT(sample_count));

		track_dur = (Double) (s64) ch->duration;
		track_dur /= read->time_scale;
		//move channel duration in media timescale
		ch->duration = (u32) (track_dur * ch->time_scale);
		if (ch->duration) {
			u64 avg_rate = 8 * gf_isom_get_media_data_size(read->mov, ch->track);
			avg_rate = (u64) (avg_rate / track_dur);
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, &PROP_UINT((u32) avg_rate));
		}

		gf_filter_pid_set_property_str(pid, "BufferLength", &PROP_UINT(500000));
		gf_filter_pid_set_property_str(pid, "RebufferLength", &PROP_UINT(0));
		gf_filter_pid_set_property_str(pid, "BufferMaxOccupancy", &PROP_UINT(500000));

		if (mime) gf_filter_pid_set_property_str(ch->pid, "meta:mime", &PROP_STRING(mime) );
		if (encoding) gf_filter_pid_set_property_str(ch->pid, "meta:encoding", &PROP_STRING(encoding) );
		if (namespace) gf_filter_pid_set_property_str(ch->pid, "meta:xmlns", &PROP_STRING(namespace) );
		if (schemaloc) gf_filter_pid_set_property_str(ch->pid, "meta:schemaloc", &PROP_STRING(schemaloc) );

		w = gf_isom_get_sample_duration(read->mov, ch->track, 1);
		h = gf_isom_get_sample_duration(read->mov, ch->track, 2);
		if (w && h && (w==h)) {
			w *= sr;
			w /= ch->time_scale;
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT(w));
		}

		gf_filter_pid_set_property(ch->pid, GF_PROP_PID_MEDIA_DATA_SIZE, &PROP_LONGUINT(gf_isom_get_media_data_size(read->mov, i+1) ) );

		if (stxtcfg) gf_filter_pid_set_property(ch->pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char *)stxtcfg, (u32) strlen(stxtcfg) ));

		w = gf_isom_get_constant_sample_size(read->mov, i+1);
		if (w)
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_FRAME_SIZE, &PROP_UINT(w));

		gf_filter_pid_set_info(pid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_REWIND) );

		max_size = 0;
		for (j=0; j<sample_count; j++) {
			u32 s = gf_isom_get_sample_size(read->mov, ch->track, j+1);
			if (s > max_size) max_size = s;
		}
		gf_filter_pid_set_info(pid, GF_PROP_PID_MAX_FRAME_SIZE, &PROP_UINT(max_size) );

#if !defined(GPAC_DISABLE_ISOM_WRITE)
		e = gf_isom_get_track_template(read->mov, ch->track, &tk_template, &tk_template_size);
		if (e == GF_OK) {
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_TRACK_TEMPLATE, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize track box: %s\n", gf_error_to_string(e) ));
		}

		e = gf_isom_get_raw_user_data(read->mov, &tk_template, &tk_template_size);
		if (e==GF_OK) {
			if (tk_template_size)
				gf_filter_pid_set_property(ch->pid, GF_PROP_PID_ISOM_UDTA, &PROP_DATA_NO_COPY(tk_template, tk_template_size) );
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Failed to serialize moov UDTA box: %s\n", gf_error_to_string(e) ));
		}
#endif

		if (codec_id == GF_CODECID_DIMS) {
			GF_DIMSDescription dims;
			memset(&dims, 0, sizeof(GF_DIMSDescription));

			gf_isom_get_dims_description(read->mov, ch->track, 1, &dims);
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
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Track %d unsupported audio bit depth %d\n", i+1, udesc->bits_per_sample ));
					break;
				}
			}
			gf_filter_pid_set_property(ch->pid, GF_PROP_PID_BIT_DEPTH_Y, &PROP_UINT(udesc->depth));

			gf_free(udesc);
		}
	}
	/*declare image items*/
	count = gf_isom_get_meta_item_count(read->mov, GF_TRUE, 0);
	for (i=0; i<count; i++) {
		if (! isor_declare_item_properties(read, NULL, i+1))
			continue;

		if (read->itt) break;
	}

#ifdef FILTER_FIXME
	/*if cover art, extract it in cache*/
	if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COVER_ART, &tag, &tlen)==GF_OK) {
		const char *cdir = gf_modules_get_option((GF_BaseInterface *)gf_service_get_interface(read->service), "General", "CacheDirectory");
		if (cdir) {
			char szName[GF_MAX_PATH];
			const char *sep;
			FILE *t;
			sep = strrchr(gf_isom_get_filename(read->mov), '\\');
			if (!sep) sep = strrchr(gf_isom_get_filename(read->mov), '/');
			if (!sep) sep = gf_isom_get_filename(read->mov);

			if ((cdir[strlen(cdir)-1] != '\\') && (cdir[strlen(cdir)-1] != '/')) {
				sprintf(szName, "%s/%s_cover.%s", cdir, sep, (tlen & 0x80000000) ? "png" : "jpg");
			} else {
				sprintf(szName, "%s%s_cover.%s", cdir, sep, (tlen & 0x80000000) ? "png" : "jpg");
			}

			t = gf_fopen(szName, "wb");

			if (t) {
				Bool isom_contains_video = GF_FALSE;

				/*write cover data*/
				assert(!(tlen & 0x80000000));
				gf_fwrite(tag, tlen & 0x7FFFFFFF, 1, t);
				gf_fclose(t);

				/*don't display cover art when video is present*/
				for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
					if (!gf_isom_is_track_enabled(read->mov, i+1))
						continue;
					if (gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_VISUAL) {
						isom_contains_video = GF_TRUE;
						break;
					}
				}

				if (!isom_contains_video) {
					od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
					od->service_ifce = read->input;
					od->objectDescriptorID = GF_MEDIA_EXTERNAL_ID;
					od->URLString = gf_strdup(szName);
					if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
						send_proxy_command(read, GF_FALSE, GF_TRUE, GF_OK, (GF_Descriptor*)od, NULL);
					} else {
						gf_service_declare_media(read->service, (GF_Descriptor*)od, GF_TRUE);
					}
				}
			}
		}
	}
#endif

}

Bool isor_declare_item_properties(ISOMReader *read, ISOMChannel *ch, u32 item_idx)
{
	GF_ImageItemProperties props;
	GF_FilterPid *pid;
	GF_ESD *esd;
	u32 item_id;
	gf_isom_get_meta_item_info(read->mov, GF_TRUE, 0, item_idx, &item_id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	if (!item_id) return GF_FALSE;

	gf_isom_get_meta_image_props(read->mov, GF_TRUE, 0, item_id, &props);
	if (props.hidden) return GF_FALSE;

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
	gf_odf_desc_del((GF_Descriptor *)esd);

	if (!ch) {
		/*ch = */isor_create_channel(read, pid, 0, item_id);
	}
	return GF_TRUE;
}

#endif /*GPAC_DISABLE_ISOM*/



