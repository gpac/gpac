/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
		seg->SegmentName = strdup(name);
		gf_list_add(iod->OCIDescriptors, seg);
		if (prev_seg) {
			prev_seg->Duration = (u32) (start - prev_start);
			prev_seg->Duration /= 1000;
		} else if (start) {
			prev_seg = (GF_Segment *) gf_odf_desc_new(GF_ODF_SEGMENT_TAG);
			prev_seg->startTime = 0;
			prev_seg->Duration = (u32) (start);
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
			prev_seg->Duration = (u32) (start - prev_start);
			prev_seg->Duration /= 1000;
		}
	}
}

/*emulate a default IOD for all other files (3GP, weird MP4, QT )*/
GF_Descriptor *isor_emulate_iod(ISOMReader *read)
{
	GF_ODCodec *codec;
	GF_ODUpdate *odU;
	u32 i, OD_ID, ID;
	GF_InitialObjectDescriptor *fake_iod;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;

	//gf_term_on_message(read->service, GF_OK, "IOD not found or broken - emulating");
	read->OD_ESID = 0;
	for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
		switch (gf_isom_get_media_type(read->mov, i+1)) {
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_TEXT:
			ID = gf_isom_get_track_id(read->mov, i+1);
			if ((ID<0xFFFF) && (read->OD_ESID < ID)) read->OD_ESID = ID;
			break;
		}
	}
	/*no usable tracks*/
	if (!read->OD_ESID) return NULL;
	read->OD_ESID++;


	OD_ID = 2;

	/*make OD AU*/
	codec = gf_odf_codec_new();	
	odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);

	for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
		switch (gf_isom_get_media_type(read->mov, i+1)) {
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_TEXT:
			esd = gp_media_map_esd(read->mov, i+1);
			if (esd) {
				od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
				od->objectDescriptorID = OD_ID;
				esd->OCRESID = read->OD_ESID;
				gf_list_add(od->ESDescriptors, esd);
				gf_list_add(odU->objectDescriptors, od);
				OD_ID++;
			}
			break;
		default:
			break;
		}
	}

	gf_odf_codec_add_com(codec, (GF_ODCom *)odU);
	gf_odf_codec_encode(codec);
	gf_odf_codec_get_au(codec, &read->od_au, &read->od_au_size);
	gf_odf_codec_del(codec);


	/*generate an IOD with our private dynamic OD stream*/
	fake_iod = (GF_InitialObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
	fake_iod->objectDescriptorID = 1;
	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	esd->slConfig->useRandomAccessPointFlag = 1;
	esd->slConfig->useTimestampsFlag = 1;
	esd->OCRESID = esd->ESID = read->OD_ESID;
	esd->decoderConfig->streamType = GF_STREAM_OD;
	esd->decoderConfig->objectTypeIndication = GPAC_STATIC_OD_OTI;
	gf_list_add(fake_iod->ESDescriptors, esd);
	fake_iod->graphics_profileAndLevel = 1;
	fake_iod->OD_profileAndLevel = 1;
	fake_iod->scene_profileAndLevel = 1;
	fake_iod->audio_profileAndLevel = 0xFE;
	fake_iod->visual_profileAndLevel = 0xFE;
	isor_emulate_chapters(read->mov, fake_iod);
	return (GF_Descriptor *)fake_iod;
}

Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return 1;
#ifndef GPAC_READ_ONLY
	if (InterfaceType == GF_STREAMING_MEDIA_CACHE) return 1;
#endif
	return 0;
}

GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)isor_client_load();
#ifndef GPAC_READ_ONLY
	if (InterfaceType == GF_STREAMING_MEDIA_CACHE) return (GF_BaseInterface *)isow_load_cache();
#endif
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE: isor_client_del(ifce); break;
#ifndef GPAC_READ_ONLY
	case GF_STREAMING_MEDIA_CACHE: isow_delete_cache(ifce); break;
#endif
	}
}
