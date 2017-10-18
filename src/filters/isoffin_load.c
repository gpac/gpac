/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
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
	GF_ESD *esd;
	ISOMChannel *ch;
	const char *tag;
	u32 i, count, ocr_es_id, tlen, base_track, j, track_id;
	Bool highest_stream;
	Bool single_media_found = GF_FALSE;
	Bool use_iod = GF_FALSE;
	GF_Descriptor *od = gf_isom_get_root_od(read->mov);
	if (od && gf_list_count(((GF_ObjectDescriptor*)od)->ESDescriptors)) {
		use_iod = GF_TRUE;
	}
	if (od) gf_odf_desc_del(od);

	/*TODO check for alternate tracks*/
	count = gf_isom_get_track_count(read->mov);
	for (i=0; i<count; i++) {
		u32 w, h, sr, nb_ch;
		GF_ESD *base_esd=NULL;
		Bool external_base=GF_FALSE;
		Double track_dur=0;
		GF_FilterPid *pid;
		u32 mtype, m_subtype;
		if (!gf_isom_is_track_enabled(read->mov, i+1))
			continue;

		mtype = gf_isom_get_media_type(read->mov, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_SUBPIC:
			break;
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_OD:
			break;
		default:
			continue;
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
		esd = gf_media_map_esd(read->mov, i+1);
		if (!esd) continue;

		if (esd->decoderConfig->streamType==GF_STREAM_INTERACT) {
			gf_odf_desc_del((GF_Descriptor *)esd);
			continue;
		}

		ocr_es_id = 0;
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
				base_esd = gf_media_map_esd(read->mov, base_track);
				if (base_esd) esd->dependsOnESID = base_esd->ESID;
				esd->has_scalable_layers = GF_TRUE;
			} else {
				esd->dependsOnESID=0;
				switch (gf_isom_get_hevc_lhvc_type(read->mov, i+1, 1)) {
				case GF_ISOM_HEVCTYPE_HEVC_LHVC:
				case GF_ISOM_HEVCTYPE_LHVC_ONLY:
					esd->has_scalable_layers = GF_TRUE;
					break;
				//this is likely temporal sublayer of base
				case GF_ISOM_HEVCTYPE_HEVC_ONLY:
					esd->has_scalable_layers = GF_FALSE;
					if (gf_isom_get_reference_count(read->mov, i+1, GF_ISOM_REF_SCAL)<=0) {
						base_esd = gf_media_map_esd(read->mov, base_track);
						if (base_esd) esd->dependsOnESID = base_esd->ESID;
					}
					break;
				}
			}
		} else {
			switch (gf_isom_get_hevc_lhvc_type(read->mov, i+1, 1)) {
			case GF_ISOM_HEVCTYPE_HEVC_LHVC:
			case GF_ISOM_HEVCTYPE_LHVC_ONLY:
				esd->has_scalable_layers = GF_TRUE;
				break;
			}
		}

		if (!esd->langDesc) {
			esd->langDesc = (GF_Language *) gf_odf_desc_new(GF_ODF_LANG_TAG);
			gf_isom_get_media_language(read->mov, i+1, &esd->langDesc->full_lang_code);
		}

		if (base_esd) {
			if (!ocr_es_id) ocr_es_id = base_esd->OCRESID;
			base_esd->OCRESID = ocr_es_id;
		}
		if (!ocr_es_id) ocr_es_id = esd->OCRESID ? esd->OCRESID : esd->ESID;
		esd->OCRESID = ocr_es_id;

		//OK declare PID
		pid = gf_filter_pid_new(read->filter);
		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(esd->ESID));
		gf_filter_pid_set_property(pid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(esd->OCRESID));
		gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(esd->dependsOnESID));

		//MPEG-4 systems present
		if (use_iod)
			gf_filter_pid_set_property(pid, GF_PROP_PID_ESID, &PROP_UINT(esd->ESID));

		if (gf_isom_is_track_in_root_od(read->mov, i+1)) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE));
		}

		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(esd->decoderConfig->streamType));
		gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, &PROP_UINT(esd->decoderConfig->objectTypeIndication));
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(esd->slConfig->timestampResolution));
		if (esd->decoderConfig->decoderSpecificInfo) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength));

			esd->decoderConfig->decoderSpecificInfo->data=NULL;
			esd->decoderConfig->decoderSpecificInfo->dataLength=0;
		}
		if (esd->langDesc) {
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
		}
		if (esd->decoderConfig->avgBitrate) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, &PROP_UINT(esd->decoderConfig->avgBitrate));
		}

		gf_isom_get_visual_info(read->mov, i+1, 1, &w, &h);
		if (w && h) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(w));
			gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(h));
		}
		gf_isom_get_audio_info(read->mov, i+1, 1, &sr, &nb_ch, NULL);
		if (sr && nb_ch) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
			gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
		}
		if (esd->has_scalable_layers)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SCALABLE, &PROP_BOOL(GF_TRUE));

		ch = ISOR_CreateChannel(read, pid, i+1, 0);

		ch->duration = gf_isom_get_track_duration(read->mov, ch->track);
		if (!ch->duration) {
			ch->duration = gf_isom_get_duration(read->mov);
		}
		gf_filter_pid_set_info(pid, GF_PROP_PID_DURATION, &PROP_FRAC_INT(ch->duration, read->time_scale));

		track_dur = (Double) (s64) ch->duration;
		track_dur /= read->time_scale;
		//move channel diuration in media timescale
		ch->duration = (u32) (track_dur * ch->time_scale);

		gf_filter_pid_set_property_str(pid, "BufferLength", &PROP_UINT(500000));
		gf_filter_pid_set_property_str(pid, "RebufferLength", &PROP_UINT(0));
		gf_filter_pid_set_property_str(pid, "BufferMaxOccupancy", &PROP_UINT(500000));


		//todo: map other ESD params if needed

		gf_odf_desc_del((GF_Descriptor *)esd);
	}
	/*declare image items*/
	count = gf_isom_get_meta_item_count(read->mov, GF_TRUE, 0);
	for (i=0; count && i<1; i++) {
		GF_ImageItemProperties props;
		GF_FilterPid *pid;
		u32 item_id = gf_isom_get_meta_primary_item_id(read->mov, GF_TRUE, 0);
		if (!item_id) continue;

		gf_isom_get_meta_image_props(read->mov, GF_TRUE, 0, item_id, &props);
		if (props.hidden) continue;

		esd = gf_media_map_item_esd(read->mov, item_id);
		if (!esd) continue;

		//OK declare PID
		pid = gf_filter_pid_new(read->filter);
		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(esd->ESID));
		//TODO: no support for LHEVC images
		//gf_filter_pid_set_property(pid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(esd->dependsOnESID));

		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(esd->decoderConfig->streamType));
		gf_filter_pid_set_property(pid, GF_PROP_PID_OTI, &PROP_UINT(esd->decoderConfig->objectTypeIndication));
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

		ch = ISOR_CreateChannel(read, pid, 0, item_id);
		gf_odf_desc_del((GF_Descriptor *)esd);
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

#endif /*GPAC_DISABLE_ISOM*/



