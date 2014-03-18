/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MP4 reader module
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

#include "isom_in.h"

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

/*emulate a default IOD for all other files (3GP, weird MP4, QT )*/
GF_Descriptor *isor_emulate_iod(ISOMReader *read)
{
	/*generate an IOD with our private dynamic OD stream*/
	GF_InitialObjectDescriptor *fake_iod = (GF_InitialObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
	isor_emulate_chapters(read->mov, fake_iod);
	read->no_service_desc = GF_TRUE;
	return (GF_Descriptor *)fake_iod;
}

void isor_declare_objects(ISOMReader *read)
{
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	const char *tag;
	u32 i, count, ocr_es_id, tlen, base_track, j, track_id;
	Bool highest_stream;
	char *opt;
	Bool add_ps_lower = GF_TRUE;

	ocr_es_id = 0;
	opt = (char*) gf_modules_get_option((GF_BaseInterface *)read->input, "ISOReader", "DeclareScalableXPS");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)read->input, "ISOReader", "DeclareScalableXPS", "yes");
	} else if (!strcmp(opt, "no")) {
		add_ps_lower = GF_FALSE;
	}

	/*TODO check for alternate tracks*/
	count = gf_isom_get_track_count(read->mov);
	for (i=0; i<count; i++) {
		if (!gf_isom_is_track_enabled(read->mov, i+1)) continue;
	
		switch (gf_isom_get_media_type(read->mov, i+1)) {
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_SUBPIC:
			break;
		default:
			continue;
		}
		
		/*we declare only the highest video track (i.e the track we play)*/
		highest_stream = GF_TRUE;
		track_id = gf_isom_get_track_id(read->mov, i+1);
		for (j = 0; j < count; j++) {
			if (gf_isom_has_track_reference(read->mov, j+1, GF_ISOM_REF_SCAL, track_id) > 0) {
				highest_stream = GF_FALSE;
				break;
			}
		}
		if ((gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_VISUAL) && !highest_stream)
			continue;
		esd = gf_media_map_esd(read->mov, i+1);
		if (esd) {
			gf_isom_get_reference(read->mov, i+1, GF_ISOM_REF_BASE, 1, &base_track);
			esd->has_ref_base = base_track ? GF_TRUE : GF_FALSE;

			od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
			od->service_ifce = read->input;
			od->objectDescriptorID = 0;
			if (!ocr_es_id) ocr_es_id = esd->ESID;
			esd->OCRESID = ocr_es_id;
			gf_list_add(od->ESDescriptors, esd);
			if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
				send_proxy_command(read, GF_FALSE, GF_TRUE, GF_OK, (GF_Descriptor*)od, NULL);
			} else {
				gf_term_add_media(read->service, (GF_Descriptor*)od, GF_TRUE);
			}
		}
	}
	/*if cover art, extract it in cache*/
	if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COVER_ART, &tag, &tlen)==GF_OK) {
		const char *cdir = gf_modules_get_option((GF_BaseInterface *)gf_term_get_service_interface(read->service), "General", "CacheDirectory");
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

			t = gf_f64_open(szName, "wb");

			if (t) {
				Bool isom_contains_video = GF_FALSE;

				/*write cover data*/
				assert(!(tlen & 0x80000000));
				gf_fwrite(tag, tlen & 0x7FFFFFFF, 1, t);
				fclose(t);

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
						gf_term_add_media(read->service, (GF_Descriptor*)od, GF_TRUE);
					}
				}
			}
		}
	}
	if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
		send_proxy_command(read, GF_FALSE, GF_TRUE, GF_OK, NULL, NULL);
	} else {
		gf_term_add_media(read->service, NULL, GF_FALSE);
	} 
}

#endif /*GPAC_DISABLE_ISOM*/


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_ISOM
	GF_NET_CLIENT_INTERFACE,
#endif
#ifndef GPAC_DISABLE_ISOM_WRITE
	GF_STREAMING_MEDIA_CACHE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
#ifndef GPAC_DISABLE_ISOM
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) 
		return (GF_BaseInterface *)isor_client_load();
#endif
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (InterfaceType == GF_STREAMING_MEDIA_CACHE) 
		return (GF_BaseInterface *)isow_load_cache();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_ISOM
	case GF_NET_CLIENT_INTERFACE: isor_client_del(ifce); break;
#endif
#ifndef GPAC_DISABLE_ISOM_WRITE
	case GF_STREAMING_MEDIA_CACHE: isow_delete_cache(ifce); break;
#endif
	}
}

GPAC_MODULE_STATIC_DELARATION( isom )
