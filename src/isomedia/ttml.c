/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2000-2012
 *                  All rights reserved
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
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_ISOM

#ifndef GPAC_DISABLE_TTML

GF_Box *stpp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_XMLSubtitleSampleEntryBox, GF_ISOM_BOX_TYPE_STPP);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

void stpp_del(GF_Box *s)
{
	GF_XMLSubtitleSampleEntryBox *stpp= (GF_XMLSubtitleSampleEntryBox *)s;
	if (stpp == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	btrt_del((GF_Box*)stpp->bitrate);
	if (stpp->xmlnamespace) gf_free(stpp->xmlnamespace);
	if (stpp->schema_location) gf_free(stpp->schema_location);
	if (stpp->auxiliary_mime_types) gf_free(stpp->auxiliary_mime_types);
	gf_free(s);
}

static GF_Err stpp_Add(GF_Box *s, GF_Box *a)
{
	GF_XMLSubtitleSampleEntryBox *stpp = (GF_XMLSubtitleSampleEntryBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_BTRT:
		if (stpp->bitrate) ERROR_ON_DUPLICATED_BOX(a, s)
			stpp->bitrate = (GF_MPEG4BitRateBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(stpp->protections, a);
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err stpp_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 size, i;
	char *str;
	GF_XMLSubtitleSampleEntryBox *stpp = (GF_XMLSubtitleSampleEntryBox *)s;
	gf_bs_read_data(bs, stpp->reserved, 6);
	stpp->dataReferenceIndex = gf_bs_read_u16(bs);

	size = (u32) stpp->size - 8;
	str = (char *)gf_malloc(sizeof(char)*size);

	i=0;
	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) stpp->xmlnamespace = gf_strdup(str);

	i=0;
	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) stpp->schema_location = gf_strdup(str);

	i=0;
	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) stpp->auxiliary_mime_types = gf_strdup(str);

	stpp->size = size;
	gf_free(str);
	return gf_isom_read_box_list(s, bs, stpp_Add);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err stpp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_XMLSubtitleSampleEntryBox *stpp = (GF_XMLSubtitleSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	gf_bs_write_data(bs, stpp->reserved, 6);
	gf_bs_write_u16(bs, stpp->dataReferenceIndex);

	gf_bs_write_data(bs, stpp->xmlnamespace, (u32) strlen(stpp->xmlnamespace));
	gf_bs_write_u8(bs, 0);

	if (stpp->schema_location)
		gf_bs_write_data(bs, stpp->schema_location, (u32) strlen(stpp->schema_location));
	gf_bs_write_u8(bs, 0);

	if (stpp->auxiliary_mime_types)
		gf_bs_write_data(bs, stpp->auxiliary_mime_types, (u32) strlen(stpp->auxiliary_mime_types));
	gf_bs_write_u8(bs, 0);

	e = gf_isom_box_write((GF_Box *)stpp->bitrate, bs);
	if (e) return e;

	return gf_isom_box_array_write(s, stpp->protections, bs);
}

GF_Err stpp_Size(GF_Box *s)
{
	GF_Err e;
	GF_XMLSubtitleSampleEntryBox *stpp = (GF_XMLSubtitleSampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8; // reserved and dataReferenceIndex

	stpp->size += strlen(stpp->xmlnamespace);
	stpp->size++;

	if (stpp->schema_location)
		stpp->size += strlen(stpp->schema_location);
	stpp->size++;

	if (stpp->auxiliary_mime_types)
		stpp->size += strlen(stpp->auxiliary_mime_types);
	stpp->size++;

	e = gf_isom_box_size((GF_Box *)stpp->bitrate);
	if (e) return e;
	stpp->size += stpp->bitrate->size;

	return gf_isom_box_array_size(s, stpp->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_DUMP
GF_Err DumpBox(GF_Box *a, FILE * trace);
void gf_box_dump_done(char *name, GF_Box *ptr, FILE *trace);

GF_Err stpp_dump(GF_Box *a, FILE * trace)
{
	GF_XMLSubtitleSampleEntryBox *stpp = (GF_XMLSubtitleSampleEntryBox *)a;
	const char *name = "XMLSubtitleSampleEntryBox";

	fprintf(trace, "<%s ", name);
	fprintf(trace, "namespace=\"%s\" ", stpp->xmlnamespace);
	if (stpp->schema_location) fprintf(trace, "schema_location=\"%s\" ", stpp->schema_location);
	if (stpp->auxiliary_mime_types) fprintf(trace, "auxiliary_mime_types=\"%s\" ", stpp->auxiliary_mime_types);
	fprintf(trace, ">\n");
	DumpBox(a, trace);

	gf_box_dump(stpp->bitrate, trace);

	gf_box_dump_done(NULL, a, trace);
	fprintf(trace, "</%s>\n", name);
	return GF_OK;
}

#endif /* GPAC_DISABLE_ISOM_DUMP */

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_update_xml_subtitle_description( GF_ISOFile                          *movie,
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
	case GF_ISOM_MEDIA_MPEG_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	return e;
}

GF_Err gf_isom_new_xml_subtitle_description(GF_ISOFile  *movie,
        u32         trackNumber,
        char        *xmlnamespace,
        char        *xml_schema_loc,
        char        *mimes,
        u32         *outDescriptionIndex)
{
	GF_TrackBox                 *trak;
	GF_Err                      e;
	u32                         dataRefIndex;
	GF_XMLSubtitleSampleEntryBox *stpp;
	char *URLname = NULL;
	char *URNname = NULL;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_MPEG_SUBT:
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
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stpp = (GF_XMLSubtitleSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STPP);
	stpp->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, stpp);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);

	stpp->bitrate = (GF_MPEG4BitRateBox *)btrt_New();
	stpp->xmlnamespace = gf_strdup(xmlnamespace);
	stpp->schema_location = gf_strdup(xml_schema_loc);
	stpp->auxiliary_mime_types = gf_strdup(mimes);
	return e;
}

#endif

/* blindly adds text to a sample following 3GPP Timed Text style */
GF_Err gf_isom_xml_subtitle_sample_add_text(GF_GenericSubtitleSample *samp, char *text_data, u32 text_len)
{
	if (!samp) return GF_BAD_PARAM;
	if (!text_len) return GF_OK;
	samp->text = (char*)gf_realloc(samp->text, sizeof(char) * (samp->len + text_len) );
	memcpy(samp->text + samp->len, text_data, sizeof(char) * text_len);
	samp->len += text_len;
	return GF_OK;
}

/*
 * Writing the xml sample structure into a sample buffer
 * - putting the text or XML in the sample directly
 * - optionally handle secondary resources (TODO)
*/
GF_ISOSample *gf_isom_xml_subtitle_to_sample(GF_GenericSubtitleSample *samp)
{
	GF_ISOSample *res;
	GF_BitStream *bs;
	if (!samp) return NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (samp->len) gf_bs_write_data(bs, samp->text, samp->len);
	else     gf_bs_write_data(bs, "", 1);

	res = gf_isom_sample_new();
	if (!res) {
		gf_bs_del(bs);
		return NULL;
	}
	gf_bs_get_content(bs, &res->data, &res->dataLength);
	gf_bs_del(bs);
	res->IsRAP = RAP;
	return res;
}

GF_GenericSubtitleSample *gf_isom_new_xml_subtitle_sample()
{
	GF_GenericSubtitleSample *res;
	GF_SAFEALLOC(res, GF_GenericSubtitleSample);
	if (!res) return NULL;
	return res;
}

GF_Err gf_isom_xml_subtitle_reset(GF_GenericSubtitleSample *samp)
{
	if (!samp) return GF_BAD_PARAM;
	if (samp->text) gf_free(samp->text);
	samp->text = NULL;
	samp->len = 0;
	return GF_OK;
}

GF_EXPORT
void gf_isom_delete_xml_subtitle_sample(GF_GenericSubtitleSample * samp)
{
	gf_isom_xml_subtitle_reset(samp);
	gf_free(samp);
}

GF_EXPORT
GF_GenericSubtitleSample *gf_isom_parse_xml_subtitle_sample(GF_BitStream *bs)
{
	GF_GenericSubtitleSample *s = gf_isom_new_xml_subtitle_sample();

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

GF_GenericSubtitleSample *gf_isom_parse_xml_subtitle_sample_from_data(char *data, u32 dataLength)
{
	GF_GenericSubtitleSample *s;
	GF_BitStream *bs;
	/*empty text sample*/
	if (!data || !dataLength) {
		return gf_isom_new_xml_subtitle_sample();
	}

	bs = gf_bs_new(data, dataLength, GF_BITSTREAM_READ);
	s = gf_isom_parse_xml_subtitle_sample(bs);
	gf_bs_del(bs);
	return s;
}


#endif /* GPAC_DISABLE_TTML */

#endif /*GPAC_DISABLE_ISOM*/
