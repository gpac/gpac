/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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


GF_Err gf_isom_get_text_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_TextSampleDescriptor **out_desc)
{
	GF_TrackBox *trak;
	u32 i;
	GF_Tx3gSampleEntryBox *txt;

	if (!descriptionIndex || !out_desc) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	txt = (GF_Tx3gSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex - 1);
	if (!txt) return GF_BAD_PARAM;
	switch (txt->type) {
	case GF_ISOM_BOX_TYPE_TX3G:
	case GF_ISOM_BOX_TYPE_TEXT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	(*out_desc) = (GF_TextSampleDescriptor *) gf_odf_desc_new(GF_ODF_TX3G_TAG);
	if (! (*out_desc) ) return GF_OUT_OF_MEM;
	(*out_desc)->back_color = txt->back_color;
	(*out_desc)->default_pos = txt->default_box;
	(*out_desc)->default_style = txt->default_style;
	(*out_desc)->displayFlags = txt->displayFlags;
	(*out_desc)->vert_justif = txt->vertical_justification;
	(*out_desc)->horiz_justif = txt->horizontal_justification;
	(*out_desc)->font_count = txt->font_table->entry_count;
	(*out_desc)->fonts = (GF_FontRecord *) gf_malloc(sizeof(GF_FontRecord) * txt->font_table->entry_count);
	for (i=0; i<txt->font_table->entry_count; i++) {
		(*out_desc)->fonts[i].fontID = txt->font_table->fonts[i].fontID;
		if (txt->font_table->fonts[i].fontName)
		 	(*out_desc)->fonts[i].fontName = gf_strdup(txt->font_table->fonts[i].fontName);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_update_text_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_TextSampleDescriptor *desc)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 i;
	GF_Tx3gSampleEntryBox *txt;

	if (!descriptionIndex || !desc) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !desc->font_count) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	txt = (GF_Tx3gSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex - 1);
	if (!txt) return GF_BAD_PARAM;
	switch (txt->type) {
	case GF_ISOM_BOX_TYPE_TX3G:
	case GF_ISOM_BOX_TYPE_TEXT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	txt->back_color = desc->back_color;
	txt->default_box = desc->default_pos;
	txt->default_style = desc->default_style;
	txt->displayFlags = desc->displayFlags;
	txt->vertical_justification = desc->vert_justif;
	txt->horizontal_justification = desc->horiz_justif;
	if (txt->font_table) gf_isom_box_del_parent(&txt->child_boxes, (GF_Box*)txt->font_table);

	txt->font_table = (GF_FontTableBox *)gf_isom_box_new_parent(&txt->child_boxes, GF_ISOM_BOX_TYPE_FTAB);
	txt->font_table->entry_count = desc->font_count;
	txt->font_table->fonts = (GF_FontRecord *) gf_malloc(sizeof(GF_FontRecord) * desc->font_count);
	for (i=0; i<desc->font_count; i++) {
		txt->font_table->fonts[i].fontID = desc->fonts[i].fontID;
		if (desc->fonts[i].fontName) txt->font_table->fonts[i].fontName = gf_strdup(desc->fonts[i].fontName);
	}
	return e;
}

GF_EXPORT
GF_Err gf_isom_new_text_description(GF_ISOFile *movie, u32 trackNumber, GF_TextSampleDescriptor *desc, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex, i;
	GF_Tx3gSampleEntryBox *txt;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !desc || !desc->font_count) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	txt = (GF_Tx3gSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TX3G);
	txt->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, txt);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	txt->back_color = desc->back_color;
	txt->default_box = desc->default_pos;
	txt->default_style = desc->default_style;
	txt->displayFlags = desc->displayFlags;
	txt->vertical_justification = desc->vert_justif;
	txt->horizontal_justification = desc->horiz_justif;
	txt->font_table = (GF_FontTableBox *)gf_isom_box_new_parent(&txt->child_boxes, GF_ISOM_BOX_TYPE_FTAB);
	txt->font_table->entry_count = desc->font_count;

	txt->font_table->fonts = (GF_FontRecord *) gf_malloc(sizeof(GF_FontRecord) * desc->font_count);
	for (i=0; i<desc->font_count; i++) {
		txt->font_table->fonts[i].fontID = desc->fonts[i].fontID;
		if (desc->fonts[i].fontName) txt->font_table->fonts[i].fontName = gf_strdup(desc->fonts[i].fontName);
	}
	return e;
}


/*blindly adds text - note we don't rely on terminaison characters to handle utf8 and utf16 data
in the same way. It is the user responsability to signal UTF16*/
GF_EXPORT
GF_Err gf_isom_text_add_text(GF_TextSample *samp, char *text_data, u32 text_len)
{
	if (!samp) return GF_BAD_PARAM;
	if (!text_len) return GF_OK;
	samp->text = (char*)gf_realloc(samp->text, sizeof(char) * (samp->len + text_len) );
	memcpy(samp->text + samp->len, text_data, sizeof(char) * text_len);
	samp->len += text_len;
	return GF_OK;
}

GF_Err gf_isom_text_set_utf16_marker(GF_TextSample *samp)
{
	/*we MUST have an empty sample*/
	if (!samp || samp->text) return GF_BAD_PARAM;
	samp->text = (char*)gf_malloc(sizeof(char) * 2);
	samp->text[0] = (char) 0xFE;
	samp->text[1] = (char) 0xFF;
	samp->len = 2;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_add_style(GF_TextSample *samp, GF_StyleRecord *rec)
{
	if (!samp || !rec) return GF_BAD_PARAM;

	if (!samp->styles) {
		samp->styles = (GF_TextStyleBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STYL);
		if (!samp->styles) return GF_OUT_OF_MEM;
	}
	samp->styles->styles = (GF_StyleRecord*)gf_realloc(samp->styles->styles, sizeof(GF_StyleRecord)*(samp->styles->entry_count+1));
	if (!samp->styles->styles) return GF_OUT_OF_MEM;
	samp->styles->styles[samp->styles->entry_count] = *rec;
	samp->styles->entry_count++;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_add_highlight(GF_TextSample *samp, u16 start_char, u16 end_char)
{
	GF_TextHighlightBox *a;
	if (!samp) return GF_BAD_PARAM;
	if (start_char == end_char) return GF_BAD_PARAM;

	a = (GF_TextHighlightBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HLIT);
	if (!a) return GF_OUT_OF_MEM;
	a->startcharoffset = start_char;
	a->endcharoffset = end_char;
	return gf_list_add(samp->others, a);
}

GF_EXPORT
GF_Err gf_isom_text_set_highlight_color(GF_TextSample *samp, u8 r, u8 g, u8 b, u8 a)
{
	if (!samp) return GF_BAD_PARAM;

	if (!samp->highlight_color) {
		samp->highlight_color = (GF_TextHighlightColorBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HCLR);
		if (!samp->highlight_color) return GF_OUT_OF_MEM;
	}
	samp->highlight_color->hil_color = a;
	samp->highlight_color->hil_color <<= 8;
	samp->highlight_color->hil_color = r;
	samp->highlight_color->hil_color <<= 8;
	samp->highlight_color->hil_color = g;
	samp->highlight_color->hil_color <<= 8;
	samp->highlight_color->hil_color = b;
	return GF_OK;
}

GF_Err gf_isom_text_set_highlight_color_argb(GF_TextSample *samp, u32 argb)
{
	if (!samp) return GF_BAD_PARAM;

	if (!samp->highlight_color) {
		samp->highlight_color = (GF_TextHighlightColorBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HCLR);
		if (!samp->highlight_color) return GF_OUT_OF_MEM;
	}
	samp->highlight_color->hil_color = argb;
	return GF_OK;
}

/*3GPP spec is quite obscur here*/
GF_EXPORT
GF_Err gf_isom_text_add_karaoke(GF_TextSample *samp, u32 start_time)
{
	if (!samp) return GF_BAD_PARAM;
	samp->cur_karaoke = (GF_TextKaraokeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_KROK);
	if (!samp->cur_karaoke) return GF_OUT_OF_MEM;
	samp->cur_karaoke->highlight_starttime = start_time;
	return gf_list_add(samp->others, samp->cur_karaoke);
}

GF_EXPORT
GF_Err gf_isom_text_set_karaoke_segment(GF_TextSample *samp, u32 end_time, u16 start_char, u16 end_char)
{
	if (!samp || !samp->cur_karaoke) return GF_BAD_PARAM;
	samp->cur_karaoke->records = (KaraokeRecord*)gf_realloc(samp->cur_karaoke->records, sizeof(KaraokeRecord)*(samp->cur_karaoke->nb_entries+1));
	if (!samp->cur_karaoke->records) return GF_OUT_OF_MEM;
	samp->cur_karaoke->records[samp->cur_karaoke->nb_entries].end_charoffset = end_char;
	samp->cur_karaoke->records[samp->cur_karaoke->nb_entries].start_charoffset = start_char;
	samp->cur_karaoke->records[samp->cur_karaoke->nb_entries].highlight_endtime = end_time;
	samp->cur_karaoke->nb_entries++;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_set_scroll_delay(GF_TextSample *samp, u32 scroll_delay)
{
	if (!samp) return GF_BAD_PARAM;
	if (!samp->scroll_delay) {
		samp->scroll_delay = (GF_TextScrollDelayBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_DLAY);
		if (!samp->scroll_delay) return GF_OUT_OF_MEM;
	}
	samp->scroll_delay->scroll_delay = scroll_delay;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_add_hyperlink(GF_TextSample *samp, char *URL, char *altString, u16 start_char, u16 end_char)
{
	GF_TextHyperTextBox*a;
	if (!samp) return GF_BAD_PARAM;
	a = (GF_TextHyperTextBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_HREF);
	if (!a) return GF_OUT_OF_MEM;
	a->startcharoffset = start_char;
	a->endcharoffset = end_char;
	a->URL = URL ? gf_strdup(URL) : NULL;
	a->URL_hint = altString ? gf_strdup(altString) : NULL;
	return gf_list_add(samp->others, a);
}

GF_EXPORT
GF_Err gf_isom_text_set_box(GF_TextSample *samp, s16 top, s16 left, s16 bottom, s16 right)
{
	if (!samp) return GF_BAD_PARAM;
	if (!samp->box) {
		samp->box = (GF_TextBoxBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_TBOX);
		if (!samp->box) return GF_OUT_OF_MEM;
	}
	samp->box->box.top = top;
	samp->box->box.left = left;
	samp->box->box.bottom = bottom;
	samp->box->box.right = right;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_add_blink(GF_TextSample *samp, u16 start_char, u16 end_char)
{
	GF_TextBlinkBox *a;
	if (!samp) return GF_BAD_PARAM;
	a = (GF_TextBlinkBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_BLNK);
	if (!a) return GF_OUT_OF_MEM;
	a->startcharoffset = start_char;
	a->endcharoffset = end_char;
	return gf_list_add(samp->others, a);
}

GF_EXPORT
GF_Err gf_isom_text_set_wrap(GF_TextSample *samp, u8 wrap_flags)
{
	if (!samp) return GF_BAD_PARAM;
	if (!samp->wrap) {
		samp->wrap = (GF_TextWrapBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_TWRP);
		if (!samp->wrap) return GF_OUT_OF_MEM;
	}
	samp->wrap->wrap_flag = wrap_flags;
	return GF_OK;
}

static GFINLINE GF_Err gpp_write_modifier(GF_BitStream *bs, GF_Box *a)
{
	GF_Err e;
	if (!a) return GF_OK;
	e = gf_isom_box_size(a);
	if (!e) e = gf_isom_box_write(a, bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_text_sample_write_bs(GF_TextSample *samp, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	if (!samp) return GF_BAD_PARAM;

	gf_bs_write_u16(bs, samp->len);
	if (samp->len) gf_bs_write_data(bs, samp->text, samp->len);

	e = gpp_write_modifier(bs, (GF_Box *)samp->styles);
	if (!e) e = gpp_write_modifier(bs, (GF_Box *)samp->highlight_color);
	if (!e) e = gpp_write_modifier(bs, (GF_Box *)samp->scroll_delay);
	if (!e) e = gpp_write_modifier(bs, (GF_Box *)samp->box);
	if (!e) e = gpp_write_modifier(bs, (GF_Box *)samp->wrap);

	if (!e) {
		GF_Box *a;
		i=0;
		while ((a = (GF_Box*)gf_list_enum(samp->others, &i))) {
			e = gpp_write_modifier(bs, a);
			if (e) break;
		}
	}
	return e;
}

GF_ISOSample *gf_isom_text_to_sample(GF_TextSample *samp)
{
	GF_Err e;
	GF_ISOSample *res;
	GF_BitStream *bs;
	if (!samp) return NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	e = gf_isom_text_sample_write_bs(samp, bs);

	if (e) {
		gf_bs_del(bs);
		return NULL;
	}
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

u32 gf_isom_text_sample_size(GF_TextSample *samp)
{
	GF_Box *a;
	u32 i, size;
	if (!samp) return 0;

	size = 2 + samp->len;
	if (samp->styles) {
		gf_isom_box_size((GF_Box *)samp->styles);
		size += (u32) samp->styles->size;
	}
	if (samp->highlight_color) {
		gf_isom_box_size((GF_Box *)samp->highlight_color);
		size += (u32) samp->highlight_color->size;
	}
	if (samp->scroll_delay) {
		gf_isom_box_size((GF_Box *)samp->scroll_delay);
		size += (u32) samp->scroll_delay->size;
	}
	if (samp->box) {
		gf_isom_box_size((GF_Box *)samp->box);
		size += (u32) samp->box->size;
	}
	if (samp->wrap) {
		gf_isom_box_size((GF_Box *)samp->wrap);
		size += (u32) samp->wrap->size;
	}
	i=0;
	while ((a = (GF_Box*)gf_list_enum(samp->others, &i))) {
		gf_isom_box_size((GF_Box *)a);
		size += (u32) a->size;
	}
	return size;
}

GF_Err gf_isom_text_has_similar_description(GF_ISOFile *movie, u32 trackNumber, GF_TextSampleDescriptor *desc, u32 *outDescIdx, Bool *same_box, Bool *same_styles)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 i, j, count;
	GF_Tx3gSampleEntryBox *txt;

	*same_box = *same_styles = 0;
	*outDescIdx = 0;

	if (!desc) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !desc->font_count) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	count = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	for (i=0; i<count; i++) {
		Bool same_fonts;
		txt = (GF_Tx3gSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
		if (!txt) continue;
		if ((txt->type != GF_ISOM_BOX_TYPE_TX3G) && (txt->type != GF_ISOM_BOX_TYPE_TEXT)) continue;
		if (txt->back_color != desc->back_color) continue;
		if (txt->displayFlags != desc->displayFlags) continue;
		if (txt->vertical_justification != desc->vert_justif) continue;
		if (txt->horizontal_justification != desc->horiz_justif) continue;
		if (txt->font_table->entry_count != desc->font_count) continue;

		same_fonts = 1;
		for (j=0; j<desc->font_count; j++) {
			if (txt->font_table->fonts[j].fontID != desc->fonts[j].fontID) same_fonts = 0;
			else if (strcmp(desc->fonts[j].fontName, txt->font_table->fonts[j].fontName)) same_fonts = 0;
		}
		if (same_fonts) {
			*outDescIdx = i+1;
			if (!memcmp(&txt->default_box, &desc->default_pos, sizeof(GF_BoxRecord))) *same_box = 1;
			if (!memcmp(&txt->default_style, &desc->default_style, sizeof(GF_StyleRecord))) *same_styles = 1;
			return GF_OK;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_TextSample *gf_isom_new_text_sample()
{
	GF_TextSample *res;
	GF_SAFEALLOC(res, GF_TextSample);
	if (!res) return NULL;
	res->others = gf_list_new();
	return res;
}

GF_EXPORT
GF_Err gf_isom_text_reset_styles(GF_TextSample *samp)
{
	if (!samp) return GF_BAD_PARAM;
	if (samp->box) gf_isom_box_del((GF_Box *)samp->box);
	samp->box = NULL;
	if (samp->highlight_color) gf_isom_box_del((GF_Box *)samp->highlight_color);
	samp->highlight_color = NULL;
	if (samp->scroll_delay) gf_isom_box_del((GF_Box *)samp->scroll_delay);
	samp->scroll_delay = NULL;
	if (samp->wrap) gf_isom_box_del((GF_Box *)samp->wrap);
	samp->wrap = NULL;
	if (samp->styles) gf_isom_box_del((GF_Box *)samp->styles);
	samp->styles = NULL;
	samp->cur_karaoke = NULL;
	while (gf_list_count(samp->others)) {
		GF_Box *a = (GF_Box*)gf_list_get(samp->others, 0);
		gf_list_rem(samp->others, 0);
		gf_isom_box_del(a);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_reset(GF_TextSample *samp)
{
	if (!samp) return GF_BAD_PARAM;
	if (samp->text) gf_free(samp->text);
	samp->text = NULL;
	samp->len = 0;
	return gf_isom_text_reset_styles(samp);
}

GF_EXPORT
void gf_isom_delete_text_sample(GF_TextSample * tx_samp)
{
	gf_isom_text_reset(tx_samp);
	gf_list_del(tx_samp->others);
	gf_free(tx_samp);
}

GF_EXPORT
GF_TextSample *gf_isom_parse_texte_sample(GF_BitStream *bs)
{
	GF_TextSample *s = gf_isom_new_text_sample();

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

	while (gf_bs_available(bs)) {
		GF_Box *a;
		GF_Err e = gf_isom_box_parse(&a, bs);
		if (e) break;

		switch (a->type) {
		case GF_ISOM_BOX_TYPE_STYL:
			if (s->styles) {
				GF_TextStyleBox *st2 = (GF_TextStyleBox *)a;
				if (!s->styles->entry_count) {
					gf_isom_box_del((GF_Box*)s->styles);
					s->styles = st2;
				} else {
					s->styles->styles = (GF_StyleRecord*)gf_realloc(s->styles->styles, sizeof(GF_StyleRecord) * (s->styles->entry_count + st2->entry_count));
					memcpy(&s->styles->styles[s->styles->entry_count], st2->styles, sizeof(GF_StyleRecord) * st2->entry_count);
					s->styles->entry_count += st2->entry_count;
					gf_isom_box_del(a);
				}
			} else {
				s->styles = (GF_TextStyleBox*)a;
			}
			break;
		case GF_ISOM_BOX_TYPE_KROK:
			s->cur_karaoke = (GF_TextKaraokeBox*)a;
		case GF_ISOM_BOX_TYPE_HLIT:
		case GF_ISOM_BOX_TYPE_HREF:
		case GF_ISOM_BOX_TYPE_BLNK:
			gf_list_add(s->others, a);
			break;
		case GF_ISOM_BOX_TYPE_HCLR:
			if (s->highlight_color) gf_isom_box_del(a);
			else s->highlight_color = (GF_TextHighlightColorBox *) a;
			break;
		case GF_ISOM_BOX_TYPE_DLAY:
			if (s->scroll_delay) gf_isom_box_del(a);
			else s->scroll_delay= (GF_TextScrollDelayBox*) a;
			break;
		case GF_ISOM_BOX_TYPE_TBOX:
			if (s->box) gf_isom_box_del(a);
			else s->box= (GF_TextBoxBox *) a;
			break;
		case GF_ISOM_BOX_TYPE_TWRP:
			if (s->wrap) gf_isom_box_del(a);
			else s->wrap= (GF_TextWrapBox*) a;
			break;
		default:
			gf_isom_box_del(a);
			break;
		}
	}
	return s;
}

GF_TextSample *gf_isom_parse_texte_sample_from_data(u8 *data, u32 dataLength)
{
	GF_TextSample *s;
	GF_BitStream *bs;
	/*empty text sample*/
	if (!data || !dataLength) {
		return gf_isom_new_text_sample();
	}

	bs = gf_bs_new(data, dataLength, GF_BITSTREAM_READ);
	s = gf_isom_parse_texte_sample(bs);
	gf_bs_del(bs);
	return s;
}


/*out-of-band sample desc (128 and 255 reserved in RFC)*/
#define SAMPLE_INDEX_OFFSET		129


static void gf_isom_write_tx3g(GF_Tx3gSampleEntryBox *a, GF_BitStream *bs, u32 sidx, u32 sidx_offset)
{
	u32 size, j, fount_count;
	void gpp_write_rgba(GF_BitStream *bs, u32 col);
	void gpp_write_box(GF_BitStream *bs, GF_BoxRecord *rec);
	void gpp_write_style(GF_BitStream *bs, GF_StyleRecord *rec);


	if (sidx_offset) gf_bs_write_u8(bs, sidx + sidx_offset);

	/*SINCE WINCE HAS A READONLY VERSION OF MP4 WE MUST DO IT BY HAND*/
	size = 8 + 18 + 8 + 12;
	size += 8 + 2;
	fount_count = 0;
	if (a->font_table) {
		fount_count = a->font_table->entry_count;
		for (j=0; j<fount_count; j++) {
			size += 3;
			if (a->font_table->fonts[j].fontName) size += (u32) strlen(a->font_table->fonts[j].fontName);
		}
	}
	/*write TextSampleEntry box*/
	gf_bs_write_u32(bs, size);
	gf_bs_write_u32(bs, a->type);
	gf_bs_write_data(bs, a->reserved, 6);
	gf_bs_write_u16(bs, a->dataReferenceIndex);
	gf_bs_write_u32(bs, a->displayFlags);
	gf_bs_write_u8(bs, a->horizontal_justification);
	gf_bs_write_u8(bs, a->vertical_justification);
	gpp_write_rgba(bs, a->back_color);
	gpp_write_box(bs, &a->default_box);
	gpp_write_style(bs, &a->default_style);
	/*write font table box*/
	size -= (8 + 18 + 8 + 12);
	gf_bs_write_u32(bs, size);
	gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_FTAB);

	gf_bs_write_u16(bs, fount_count);
	for (j=0; j<fount_count; j++) {
		gf_bs_write_u16(bs, a->font_table->fonts[j].fontID);
		if (a->font_table->fonts[j].fontName) {
			u32 len = (u32) strlen(a->font_table->fonts[j].fontName);
			gf_bs_write_u8(bs, len);
			gf_bs_write_data(bs, a->font_table->fonts[j].fontName, len);
		} else {
			gf_bs_write_u8(bs, 0);
		}
	}
}

GF_Err gf_isom_get_ttxt_esd(GF_MediaBox *mdia, GF_ESD **out_esd)
{
	GF_BitStream *bs;
	u32 count, i;
	Bool has_v_info;
	GF_List *sampleDesc;
	GF_ESD *esd;
	GF_TrackBox *tk;

	*out_esd = NULL;
	sampleDesc = mdia->information->sampleTable->SampleDescription->child_boxes;
	count = gf_list_count(sampleDesc);
	if (!count) return GF_ISOM_INVALID_MEDIA;

	esd = gf_odf_desc_esd_new(2);
	esd->decoderConfig->streamType = GF_STREAM_TEXT;
	esd->decoderConfig->objectTypeIndication = GF_CODECID_TEXT_MPEG4;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);


	/*Base3GPPFormat*/
	gf_bs_write_u8(bs, 0x10);
	/*MPEGExtendedFormat*/
	gf_bs_write_u8(bs, 0x10);
	/*profileLevel*/
	gf_bs_write_u8(bs, 0x10);
	gf_bs_write_u24(bs, mdia->mediaHeader->timeScale);
	gf_bs_write_int(bs, 0, 1);	/*no alt formats*/
	gf_bs_write_int(bs, 2, 2);	/*only out-of-band-band sample desc*/
	gf_bs_write_int(bs, 1, 1);	/*we will write sample desc*/

	/*write v info if any visual track in this movie*/
	has_v_info = 0;
	i=0;
	while ((tk = (GF_TrackBox*)gf_list_enum(mdia->mediaTrack->moov->trackList, &i))) {
		if (tk->Media->handler && (tk->Media->handler->handlerType == GF_ISOM_MEDIA_VISUAL)) {
			has_v_info = 1;
		}
	}
	gf_bs_write_int(bs, has_v_info, 1);

	gf_bs_write_int(bs, 0, 3);	/*reserved, spec doesn't say the values*/
	gf_bs_write_u8(bs, mdia->mediaTrack->Header->layer);
	gf_bs_write_u16(bs, mdia->mediaTrack->Header->width>>16);
	gf_bs_write_u16(bs, mdia->mediaTrack->Header->height>>16);

	/*write desc*/
	gf_bs_write_u8(bs, count);
	for (i=0; i<count; i++) {
		GF_Tx3gSampleEntryBox *a;
		a = (GF_Tx3gSampleEntryBox *) gf_list_get(sampleDesc, i);
		if ((a->type != GF_ISOM_BOX_TYPE_TX3G) && (a->type != GF_ISOM_BOX_TYPE_TEXT) ) continue;
		gf_isom_write_tx3g(a, bs, i+1, SAMPLE_INDEX_OFFSET);
	}
	if (has_v_info) {
		u32 trans;
		/*which video shall we pick for MPEG-4, and how is the associations indicated in 3GP ???*/
		gf_bs_write_u16(bs, 0);
		gf_bs_write_u16(bs, 0);
		trans = mdia->mediaTrack->Header->matrix[6];
		trans >>= 16;
		gf_bs_write_u16(bs, trans);
		trans = mdia->mediaTrack->Header->matrix[7];
		trans >>= 16;
		gf_bs_write_u16(bs, trans);
	}

	gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(bs);
	*out_esd = esd;
	return GF_OK;
}

GF_Err gf_isom_rewrite_text_sample(GF_ISOSample *samp, u32 sampleDescriptionIndex, u32 sample_dur)
{
	GF_BitStream *bs;
	u32 pay_start, txt_size;
	Bool is_utf_16 = 0;
	if (!samp || !samp->data || !samp->dataLength) return GF_OK;

	bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	txt_size = gf_bs_read_u16(bs);
	gf_bs_del(bs);

	/*remove BOM*/
	pay_start = 2;
	if (txt_size>2) {
		/*seems 3GP only accepts BE UTF-16 (no LE, no UTF32)*/
		if (((u8) samp->data[2]==(u8) 0xFE) && ((u8)samp->data[3]==(u8) 0xFF)) {
			is_utf_16 = 1;
			pay_start = 4;
			txt_size -= 2;
		}
	}

	/*rewrite as TTU(1)*/
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bs, is_utf_16, 1);
	gf_bs_write_int(bs, 0, 4);
	gf_bs_write_int(bs, 1, 3);
	gf_bs_write_u16(bs, 8 + samp->dataLength - pay_start);
	gf_bs_write_u8(bs, sampleDescriptionIndex + SAMPLE_INDEX_OFFSET);
	gf_bs_write_u24(bs, sample_dur);
	/*write text size*/
	gf_bs_write_u16(bs, txt_size);
	if (txt_size) gf_bs_write_data(bs, samp->data + pay_start, samp->dataLength - pay_start);

	gf_free(samp->data);
	samp->data = NULL;
	gf_bs_get_content(bs, &samp->data, &samp->dataLength);
	gf_bs_del(bs);
	return GF_OK;
}


GF_Err gf_isom_text_get_encoded_tx3g(GF_ISOFile *file, u32 track, u32 sidx, u32 sidx_offset, u8 **tx3g, u32 *tx3g_size)
{
	GF_BitStream *bs;
	GF_TrackBox *trak;
	GF_Tx3gSampleEntryBox *a;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	a = (GF_Tx3gSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sidx-1);
	if (!a) return GF_BAD_PARAM;
	if ((a->type != GF_ISOM_BOX_TYPE_TX3G) && (a->type != GF_ISOM_BOX_TYPE_TEXT)) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_write_tx3g(a, bs, sidx, sidx_offset);
	*tx3g = NULL;
	*tx3g_size = 0;
	gf_bs_get_content(bs, tx3g, tx3g_size);
	gf_bs_del(bs);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM*/
