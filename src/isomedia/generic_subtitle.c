/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2012 
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
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

#include <gpac/internal/isomedia_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_ISOM

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_update_generic_subtitle_description( GF_ISOFile                          *movie, 
                                                    u32                                 trackNumber, 
                                                    u32                                 descriptionIndex, 
                                                    GF_GenericSubtitleSampleDescriptor  *desc)
{
	GF_TrackBox *trak;
	GF_Err      e;

	if (!descriptionIndex || !desc) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_SUBM:
		break;
	default:
		return GF_BAD_PARAM;
	}

	//GF_GenericSubtitleSampleEntryBox *gsub;
	//gsub = (GF_GenericSubtitleSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->boxList, descriptionIndex - 1);
	//if (!gsub) return GF_BAD_PARAM;
	//switch (txt->type) {
	//case GF_ISOM_BOX_TYPE_METX:
	//case GF_ISOM_BOX_TYPE_METT:
	//	break;
	//default:
	//	return GF_BAD_PARAM;
	//}

	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	return e;
}

GF_Err gf_isom_new_generic_subtitle_description(GF_ISOFile  *movie, 
                                                u32         trackNumber, 
                                                char        *content_encoding, 
                                                char        *xml_schema_loc, 
                                                char        *mime_type_or_namespace, 
                                                Bool        is_xml, 
                                                char        *URLname,
                                                char        *URNname,
                                                u32         *outDescriptionIndex)
{
	GF_TrackBox                 *trak;
	GF_Err                      e;
	u32                         dataRefIndex;
	GF_MetaDataSampleEntryBox   *metasd;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_SUBM:
		break;
	default:
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	metasd = (GF_MetaDataSampleEntryBox *) gf_isom_box_new((is_xml ? GF_ISOM_BOX_TYPE_METX : GF_ISOM_BOX_TYPE_METT));
	metasd->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, metasd);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);

    metasd->content_encoding = gf_strdup(content_encoding);
    metasd->xml_schema_loc = gf_strdup(xml_schema_loc);
    metasd->mime_type_or_namespace = gf_strdup(mime_type_or_namespace);
	return e;
}


/* blindly adds text to a sample following 3GPP Timed Text style */
GF_Err gf_isom_generic_subtitle_sample_add_text(GF_GenericSubtitleSample *samp, char *text_data, u32 text_len)
{
	if (!samp) return GF_BAD_PARAM;
	if (!text_len) return GF_OK;
	samp->text = (char*)gf_realloc(samp->text, sizeof(char) * (samp->len + text_len) );
	memcpy(samp->text + samp->len, text_data, sizeof(char) * text_len);
	samp->len += text_len;
	return GF_OK;
}

/* 
 * Writing the generic sample structure into a sample buffer 
 * 2 options: 
 * - putting the text or XML in the sample directly
 * - or using a meta box to structure the sample data
*/
GF_ISOSample *gf_isom_generic_subtitle_to_sample(GF_GenericSubtitleSample *samp)
{
	GF_ISOSample *res;
	GF_BitStream *bs;
	if (!samp) return NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	//gf_bs_write_u16(bs, samp->len);

	if (samp->len) gf_bs_write_data(bs, samp->text, samp->len);
    else     gf_bs_write_data(bs, "", 1);

	res = gf_isom_sample_new();
	if (!res) {
		gf_bs_del(bs);
		return NULL;
	}
	gf_bs_get_content(bs, &res->data, &res->dataLength);
	gf_bs_del(bs);
	res->IsRAP = 1;
	return res;
}

//GF_Err gf_isom_generic_subtitle_has_similar_description(GF_ISOFile                          *movie, 
//                                                        u32                                 trackNumber, 
//                                                        GF_GenericSubtitleSampleDescriptor  *desc, 
//                                                        u32                                 *outDescIdx, 
//                                                        Bool                                *same_box)
//{
//	GF_TrackBox             *trak;
//	GF_Err                  e;
//	u32                     i;
//    u32                     j;
//    u32                     count;
//	GF_Tx3gSampleEntryBox   *txt;
//
//	*same_box   = 0;
//	*outDescIdx = 0;
//
//	if (!desc) return GF_BAD_PARAM;
//	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
//	if (e) return GF_BAD_PARAM;
//	
//	trak = gf_isom_get_track_from_file(movie, trackNumber);
//	if (!trak || !trak->Media) return GF_BAD_PARAM;
//
//	switch (trak->Media->handler->handlerType) {
//	case GF_ISOM_MEDIA_SUBT:
//		break;
//	default:
//		return GF_BAD_PARAM;
//	}
//
//	return GF_OK;
//}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_GenericSubtitleSample *gf_isom_new_generic_subtitle_sample()
{
	GF_GenericSubtitleSample *res;
	GF_SAFEALLOC(res, GF_GenericSubtitleSample);
	if (!res) return NULL;
	return res;
}

GF_Err gf_isom_generic_subtitle_reset(GF_GenericSubtitleSample *samp)
{
	if (!samp) return GF_BAD_PARAM;
	if (samp->text) gf_free(samp->text);
	samp->text = NULL;
	samp->len = 0;
	return GF_OK;
}

GF_EXPORT
void gf_isom_delete_generic_subtitle_sample(GF_GenericSubtitleSample * samp)
{
	gf_isom_generic_subtitle_reset(samp);
	gf_free(samp);
}

GF_EXPORT
GF_GenericSubtitleSample *gf_isom_parse_generic_subtitle_sample(GF_BitStream *bs)
{
	GF_GenericSubtitleSample *s = gf_isom_new_generic_subtitle_sample();
	
	/*empty sample*/
	if (!bs || !gf_bs_available(bs)) return s;

	s->len = gf_bs_read_u16(bs);
	if (s->len) {
		/*2 extra bytes for UTF-16 term char just in case (we don't know if a BOM marker is present or 
		not since this may be a sample carried over RTP*/
		s->text = (char *) gf_malloc(sizeof(char)*(s->len+2) );
		s->text[s->len] = 0; 
        s->text[s->len+1] = 0;
		gf_bs_read_data(bs, s->text, s->len);
	}
	return s;
}

GF_GenericSubtitleSample *gf_isom_parse_generic_subtitle_sample_from_data(char *data, u32 dataLength)
{
	GF_GenericSubtitleSample *s;
	GF_BitStream *bs;
	/*empty text sample*/
	if (!data || !dataLength) {
		return gf_isom_new_generic_subtitle_sample();
	}
	
	bs = gf_bs_new(data, dataLength, GF_BITSTREAM_READ);
	s = gf_isom_parse_generic_subtitle_sample(bs);
	gf_bs_del(bs);
	return s;
}


/*out-of-band sample desc (128 and 255 reserved in RFC)*/
#define SAMPLE_INDEX_OFFSET		129

GF_Err gf_isom_rewrite_generic_subtitle_sample(GF_ISOSample *samp, u32 sampleDescriptionIndex, u32 sample_dur)
{
	//GF_BitStream *bs;
	//u32 pay_start, txt_size;
	//Bool is_utf_16 = 0;
	//if (!samp || !samp->data || !samp->dataLength) return GF_OK;

	//bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	//txt_size = gf_bs_read_u16(bs);
	//gf_bs_del(bs);

	///*remove BOM*/
	//pay_start = 2;
	//if (txt_size>2) {
	//	/*seems 3GP only accepts BE UTF-16 (no LE, no UTF32)*/
	//	if (((u8) samp->data[2]==(u8) 0xFE) && ((u8)samp->data[3]==(u8) 0xFF)) {
	//		is_utf_16 = 1;
	//		pay_start = 4;
	//		txt_size -= 2;
	//	}
	//}

	///*rewrite as TTU(1)*/
	//bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	//gf_bs_write_int(bs, is_utf_16, 1);
	//gf_bs_write_int(bs, 0, 4);
	//gf_bs_write_int(bs, 1, 3);
	//gf_bs_write_u16(bs, 8 + samp->dataLength - pay_start);
	//gf_bs_write_u8(bs, sampleDescriptionIndex + SAMPLE_INDEX_OFFSET);
	//gf_bs_write_u24(bs, sample_dur);
	///*write text size*/
	//gf_bs_write_u16(bs, txt_size);
	//if (txt_size) gf_bs_write_data(bs, samp->data + pay_start, samp->dataLength - pay_start);

	//gf_free(samp->data);
	//samp->data = NULL;
	//gf_bs_get_content(bs, &samp->data, &samp->dataLength);
	//gf_bs_del(bs);
	return GF_OK;
}


//GF_Err gf_isom_text_get_encoded_tx3g(GF_ISOFile *file, u32 track, u32 sidx, u32 sidx_offset, char **tx3g, u32 *tx3g_size)
//{
//	GF_BitStream *bs;
//	GF_TrackBox *trak;
//	GF_Tx3gSampleEntryBox *a;
//	
//	trak = gf_isom_get_track_from_file(file, track);
//	if (!trak) return GF_BAD_PARAM;
//
//	a = (GF_Tx3gSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->boxList, sidx-1);
//	if (!a) return GF_BAD_PARAM;
//	if ((a->type != GF_ISOM_BOX_TYPE_TX3G) && (a->type != GF_ISOM_BOX_TYPE_TEXT)) return GF_BAD_PARAM;
//	
//	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
//	gf_isom_write_tx3g(a, bs, sidx, sidx_offset);
//	*tx3g = NULL;
//	*tx3g_size = 0;
//	gf_bs_get_content(bs, tx3g, tx3g_size);
//	gf_bs_del(bs);
//	return GF_OK;
//}

#endif /*GPAC_DISABLE_ISOM*/
