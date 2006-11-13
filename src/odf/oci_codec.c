/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 ObjectDescriptor sub-project
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


#include <gpac/internal/odf_dev.h>


//from OD
GF_Err gf_odf_write_descriptor_list(GF_BitStream *bs, GF_List *descList);
GF_Err gf_odf_size_descriptor_list(GF_List *descList, u32 *outSize);
GF_Err gf_odf_delete_descriptor(GF_Descriptor *desc);
GF_Err gf_odf_parse_descriptor(GF_BitStream *bs, GF_Descriptor **desc, u32 *desc_size);
s32 gf_odf_size_field_size(u32 size_desc);


//max size of an OCI event
#define OCI_MAX_EVENT_SIZE		1<<28 - 1
#define MAX_OCIEVENT_ID		0x7FFF

struct __tag_oci_event
{
	u16 EventID;
	u8 AbsoluteTimeFlag;
	char StartingTime[4];
	char duration[4];
	GF_List *OCIDescriptors;
};

struct __tag_oci_codec 
{
	//events
	GF_List *OCIEvents;
	//version, should always be one
	u8 Version;
	//encoder or decoder
	u8 Mode;
};

GF_EXPORT
OCIEvent *gf_oci_event_new(u16 EventID)
{
	OCIEvent *tmp;
	if (EventID > MAX_OCIEVENT_ID) return NULL;
	tmp = (OCIEvent *)malloc(sizeof(OCIEvent));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(OCIEvent));
	tmp->EventID = EventID;
	tmp->OCIDescriptors = gf_list_new();
	return tmp;
}

GF_EXPORT
void gf_oci_event_del(OCIEvent *event)
{
	GF_Descriptor *desc;
	if (!event) return;

	while (gf_list_count(event->OCIDescriptors)) {
		desc = (GF_Descriptor *)gf_list_get(event->OCIDescriptors, 0);
		gf_list_rem(event->OCIDescriptors, 0);
		gf_odf_delete_descriptor(desc);
	}
	gf_list_del(event->OCIDescriptors);
	free(event);	
}

GF_EXPORT
GF_Err gf_oci_event_set_start_time(OCIEvent *event, u8 Hours, u8 Minutes, u8 Seconds, u8 HundredSeconds, u8 IsAbsoluteTime)
{
	if (!event || (Hours >= 100) || (Minutes >= 100) || (Seconds >= 100) || (HundredSeconds >= 100) )
		return GF_BAD_PARAM;

	event->AbsoluteTimeFlag = IsAbsoluteTime;
	event->StartingTime[0] = Hours;
	event->StartingTime[1] = Minutes;
	event->StartingTime[2] = Seconds;
	event->StartingTime[3] = HundredSeconds;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_oci_event_set_duration(OCIEvent *event, u8 Hours, u8 Minutes, u8 Seconds, u8 HundredSeconds)
{
	if (!event || (Hours >= 100) || (Minutes >= 100) || (Seconds >= 100) || (HundredSeconds >= 100) )
		return GF_BAD_PARAM;

	event->duration[0] = Hours;
	event->duration[1] = Minutes;
	event->duration[2] = Seconds;
	event->duration[3] = HundredSeconds;
	return GF_OK;
}

u8 OCI_IsOCIDesc(GF_Descriptor *oci_desc)
{
	if (oci_desc->tag < GF_ODF_OCI_BEGIN_TAG) return 0;
	if (oci_desc->tag > GF_ODF_OCI_END_TAG) return 0;
	return 1;
}

GF_EXPORT
GF_Err gf_oci_event_add_desc(OCIEvent *event, GF_Descriptor *oci_desc)
{
	if (!event || !oci_desc) return GF_BAD_PARAM;
	if (!OCI_IsOCIDesc(oci_desc)) return GF_ODF_INVALID_DESCRIPTOR;
	
	gf_list_add(event->OCIDescriptors, oci_desc);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_oci_event_get_id(OCIEvent *event, u16 *ID)
{
	if (!event || !ID) return GF_BAD_PARAM;
	*ID = event->EventID;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_oci_event_get_start_time(OCIEvent *event, u8 *Hours, u8 *Minutes, u8 *Seconds, u8 *HundredSeconds, u8 *IsAbsoluteTime)
{
	if (!event || !Hours || !Minutes || !Seconds || !HundredSeconds || !IsAbsoluteTime) 
		return GF_BAD_PARAM;

	*IsAbsoluteTime = event->AbsoluteTimeFlag;
	*Hours = event->StartingTime[0];
	*Minutes = event->StartingTime[1];
	*Seconds = event->StartingTime[2];
	*HundredSeconds = event->StartingTime[3];
	return GF_OK;
}

GF_EXPORT
GF_Err gf_oci_event_get_duration(OCIEvent *event, u8 *Hours, u8 *Minutes, u8 *Seconds, u8 *HundredSeconds)
{
	if (!event || !Hours || !Minutes || !Seconds || !HundredSeconds) 
		return GF_BAD_PARAM;

	*Hours = event->duration[0];
	*Minutes = event->duration[1];
	*Seconds = event->duration[2];
	*HundredSeconds = event->duration[3];
	return GF_OK;
}

GF_EXPORT
u32 gf_oci_event_get_desc_count(OCIEvent *event)
{
	if (!event) return 0;
	return gf_list_count(event->OCIDescriptors);
}

GF_EXPORT
GF_Descriptor *gf_oci_event_get_desc(OCIEvent *event, u32 DescIndex)
{
	if (!event || DescIndex >= gf_list_count(event->OCIDescriptors) ) return NULL;
	return (GF_Descriptor *) gf_list_get(event->OCIDescriptors, DescIndex);
}

GF_EXPORT
GF_Err gf_oci_event_rem_desc(OCIEvent *event, u32 DescIndex)
{
	if (!event || DescIndex >= gf_list_count(event->OCIDescriptors) ) return GF_BAD_PARAM;
	return gf_list_rem(event->OCIDescriptors, DescIndex);
}


//construction / destruction
GF_EXPORT
OCICodec *gf_oci_codec_new(u8 IsEncoder, u8 Version)
{
	OCICodec *tmp;
	if (Version != 0x01) return NULL;
	tmp = (OCICodec *)malloc(sizeof(OCICodec));
	if (!tmp) return NULL;
	tmp->Mode = IsEncoder ? 1 : 0;
	tmp->Version = 0x01;
	tmp->OCIEvents = gf_list_new();
	return tmp;
}

GF_EXPORT
void gf_oci_codec_del(OCICodec *codec)
{
	OCIEvent *ev;
	if (!codec) return;

	while (gf_list_count(codec->OCIEvents)) {
		ev = (OCIEvent *)gf_list_get(codec->OCIEvents, 0);
		gf_oci_event_del(ev);
		gf_list_rem(codec->OCIEvents, 0);
	}
	gf_list_del(codec->OCIEvents);
	free(codec);
}

GF_EXPORT
GF_Err gf_oci_codec_add_event(OCICodec *codec, OCIEvent *event)
{
	if (!codec || !codec->Mode || !event) return GF_BAD_PARAM;

	return gf_list_add(codec->OCIEvents, event);
}

GF_Err WriteSevenBitLength(GF_BitStream *bs, u32 size)
{
	u32 length;
	unsigned char vals[4];

	if (!bs || !size) return GF_BAD_PARAM;
	
	length = size;
	vals[3] = (unsigned char) (length & 0x7f);
	length >>= 7;
	vals[2] = (unsigned char) ((length & 0x7f) | 0x80); 
	length >>= 7;
	vals[1] = (unsigned char) ((length & 0x7f) | 0x80); 
	length >>= 7;
	vals[0] = (unsigned char) ((length & 0x7f) | 0x80);
	
	if (size < 0x00000080) {
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x00004000) {
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x00200000) {
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x10000000) {
		gf_bs_write_int(bs, vals[0], 8);
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else {
		return GF_ODF_INVALID_DESCRIPTOR;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_oci_codec_encode(OCICodec *codec, char **outAU, u32 *au_length)
{
	GF_BitStream *bs;
	u32 i, size, desc_size;
	GF_Err e;
	OCIEvent *ev;
	
	if (!codec || !codec->Mode || *outAU) return GF_BAD_PARAM;

	bs = NULL;
	size = 0;

	//get the size of each event
	i=0;
	while ((ev = (OCIEvent *)gf_list_enum(codec->OCIEvents, &i))) {
		//fixed size header
		size += 10;
		e = gf_odf_size_descriptor_list(codec->OCIEvents, &desc_size);
		if (e) goto err_exit;
		size += desc_size;
	}

	//encode
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	e = WriteSevenBitLength(bs, size);
	if (e) goto err_exit;

	//get one event, write it and delete it
	while (gf_list_count(codec->OCIEvents)) {
		ev = (OCIEvent *)gf_list_get(codec->OCIEvents, 0);
		gf_list_rem(codec->OCIEvents, 0);

		gf_bs_write_int(bs, ev->EventID, 15);
		gf_bs_write_int(bs, ev->AbsoluteTimeFlag, 1);
		gf_bs_write_data(bs, ev->StartingTime, 4);
		gf_bs_write_data(bs, ev->duration, 4);
		
		e = gf_odf_write_descriptor_list(bs, ev->OCIDescriptors);
		gf_oci_event_del(ev);
		if (e) goto err_exit;
		//OCI Event is aligned
		gf_bs_align(bs);
	}
	gf_bs_get_content(bs, outAU, au_length);
	gf_bs_del(bs);
	return GF_OK;


err_exit:
	if (bs) gf_bs_del(bs);
	//delete everything
	while (gf_list_count(codec->OCIEvents)) {
		ev = (OCIEvent *)gf_list_get(codec->OCIEvents, 0);
		gf_list_rem(codec->OCIEvents, 0);
		gf_oci_event_del(ev);
	}
	return e;
}


GF_EXPORT
GF_Err gf_oci_codec_decode(OCICodec *codec, char *au, u32 au_length)
{
	OCIEvent *ev;
	GF_BitStream *bs;
	u32 size, hdrS, desc_size, tot_size, tmp_size, val;
	GF_Descriptor *tmp;	
	GF_Err e;

	//must be decoder
	if (!codec || codec->Mode || !au) return GF_BAD_PARAM;

	bs = gf_bs_new(au, au_length, GF_BITSTREAM_READ);
	ev = 0;
	tot_size = 0;
	while (tot_size < au_length) {
		//create an event
		ev = gf_oci_event_new(0);
		if (!ev) {
			e = GF_OUT_OF_MEM;
			goto err_exit;
		}
		

		//FIX IM1
		gf_bs_read_int(bs, 8);
		size = 0;
		//get its size
		hdrS = 0;
		do {
			val = gf_bs_read_int(bs, 8);
			hdrS += 1;
			size <<= 7;
			size |= val & 0x7F;
		} while ( val & 0x80 );
		
		//parse event vars
		ev->EventID = gf_bs_read_int(bs, 15);
		ev->AbsoluteTimeFlag = gf_bs_read_int(bs, 1);
		gf_bs_read_data(bs, ev->StartingTime, 4);
		gf_bs_read_data(bs, ev->duration, 4);
		desc_size = 0;

		//parse descriptor list
		while (desc_size < size - 10) {
			e = gf_odf_parse_descriptor(bs, &tmp, &tmp_size);
			//RE-FIX IM1
			if (e || !tmp) goto err_exit;
			if (!OCI_IsOCIDesc(tmp)) {
				gf_odf_delete_descriptor(tmp);
				e = GF_ODF_INVALID_DESCRIPTOR;
				goto err_exit;
			}
			gf_list_add(ev->OCIDescriptors, tmp);
			desc_size += tmp_size + gf_odf_size_field_size(tmp_size);
		}
		
		if (desc_size != size - 10) {
			e = GF_CORRUPTED_DATA;
			goto err_exit;
		}

		gf_list_add(codec->OCIEvents, ev);
		//FIX IM1
		size += 1;
		tot_size += size + hdrS;
		ev = NULL;
	}

	if (tot_size != au_length) {
		e = GF_CORRUPTED_DATA;
		goto err_exit;
	}

	gf_bs_del(bs);
	return GF_OK;

err_exit:
	gf_bs_del(bs);
	if (ev) gf_oci_event_del(ev);
	//delete everything
	while (gf_list_count(codec->OCIEvents)) {
		ev = (OCIEvent *)gf_list_get(codec->OCIEvents, 0);
		gf_list_rem(codec->OCIEvents, 0);
		gf_oci_event_del(ev);
	}
	return e;
}


GF_EXPORT
OCIEvent *gf_oci_codec_get_event(OCICodec *codec)
{
	OCIEvent *ev;
	if (!codec ||codec->Mode) return NULL;
	ev = (OCIEvent *)gf_list_get(codec->OCIEvents, 0);
	gf_list_rem(codec->OCIEvents, 0);
	return ev;
}
