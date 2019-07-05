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

#ifndef GPAC_DISABLE_ISOM



GF_Box *gppc_box_new()
{
	//default type is amr but overwritten by box constructor
	ISOM_DECL_BOX_ALLOC(GF_3GPPConfigBox, GF_ISOM_BOX_TYPE_DAMR);
	return (GF_Box *)tmp;
}

void gppc_box_del(GF_Box *s)
{
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err gppc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	memset(&ptr->cfg, 0, sizeof(GF_3GPConfig));

	ptr->cfg.vendor = gf_bs_read_u32(bs);
	ptr->cfg.decoder_version = gf_bs_read_u8(bs);

	switch (ptr->type) {
	case GF_ISOM_BOX_TYPE_D263:
		ptr->cfg.H263_level = gf_bs_read_u8(bs);
		ptr->cfg.H263_profile = gf_bs_read_u8(bs);
		break;
	case GF_ISOM_BOX_TYPE_DAMR:
		ptr->cfg.AMR_mode_set = gf_bs_read_u16(bs);
		ptr->cfg.AMR_mode_change_period = gf_bs_read_u8(bs);
		ptr->cfg.frames_per_sample = gf_bs_read_u8(bs);
		break;
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
		ptr->cfg.frames_per_sample = gf_bs_read_u8(bs);
		break;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gppc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->cfg.vendor);
	gf_bs_write_u8(bs, ptr->cfg.decoder_version);
	switch (ptr->cfg.type) {
	case GF_ISOM_SUBTYPE_3GP_H263:
		gf_bs_write_u8(bs, ptr->cfg.H263_level);
		gf_bs_write_u8(bs, ptr->cfg.H263_profile);
		break;
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		gf_bs_write_u16(bs, ptr->cfg.AMR_mode_set);
		gf_bs_write_u8(bs, ptr->cfg.AMR_mode_change_period);
		gf_bs_write_u8(bs, ptr->cfg.frames_per_sample);
		break;
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		gf_bs_write_u8(bs, ptr->cfg.frames_per_sample);
		break;
	}
	return GF_OK;
}

GF_Err gppc_box_size(GF_Box *s)
{
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;

	s->size += 5;
	switch (ptr->cfg.type) {
	case GF_ISOM_SUBTYPE_3GP_H263:
		s->size += 2;
		break;
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		s->size += 4;
		break;
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		s->size += 1;
		break;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *ftab_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_FontTableBox, GF_ISOM_BOX_TYPE_FTAB);
	return (GF_Box *) tmp;
}
void ftab_box_del(GF_Box *s)
{
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	if (ptr->fonts) {
		u32 i;
		for (i=0; i<ptr->entry_count; i++)
			if (ptr->fonts[i].fontName) gf_free(ptr->fonts[i].fontName);
		gf_free(ptr->fonts);
	}
	gf_free(ptr);
}
GF_Err ftab_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	ptr->entry_count = gf_bs_read_u16(bs);
	ISOM_DECREASE_SIZE(ptr, 2);

	if (ptr->size<ptr->entry_count*3) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Corrupted ftap box, skipping\n"));
		ptr->entry_count = 0;
		return GF_OK;
	}
	ptr->fonts = (GF_FontRecord *) gf_malloc(sizeof(GF_FontRecord)*ptr->entry_count);
	memset(ptr->fonts, 0, sizeof(GF_FontRecord)*ptr->entry_count);
	for (i=0; i<ptr->entry_count; i++) {
		u32 len;
		ptr->fonts[i].fontID = gf_bs_read_u16(bs);
		len = gf_bs_read_u8(bs);
		if (len) {
			ptr->fonts[i].fontName = (char *)gf_malloc(sizeof(char)*(len+1));
			gf_bs_read_data(bs, ptr->fonts[i].fontName, len);
			ptr->fonts[i].fontName[len] = 0;
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ftab_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->entry_count);
	for (i=0; i<ptr->entry_count; i++) {
		gf_bs_write_u16(bs, ptr->fonts[i].fontID);
		if (ptr->fonts[i].fontName) {
			u32 len = (u32) strlen(ptr->fonts[i].fontName);
			gf_bs_write_u8(bs, len);
			gf_bs_write_data(bs, ptr->fonts[i].fontName, len);
		} else {
			gf_bs_write_u8(bs, 0);
		}
	}
	return GF_OK;
}
GF_Err ftab_box_size(GF_Box *s)
{
	u32 i;
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;

	s->size += 2;
	for (i=0; i<ptr->entry_count; i++) {
		s->size += 3;
		if (ptr->fonts[i].fontName) s->size += strlen(ptr->fonts[i].fontName);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *text_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextSampleEntryBox, GF_ISOM_BOX_TYPE_TEXT);
	return (GF_Box *) tmp;
}

void text_box_del(GF_Box *s)
{
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->textName)
		gf_free(ptr->textName);
	gf_free(ptr);
}

GF_Box *tx3g_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_Tx3gSampleEntryBox, GF_ISOM_BOX_TYPE_TX3G);
	return (GF_Box *) tmp;
}

void tx3g_box_del(GF_Box *s)
{
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	gf_free(s);
}

u32 gpp_read_rgba(GF_BitStream *bs)
{
	u8 r, g, b, a;
	u32 col;
	r = gf_bs_read_u8(bs);
	g = gf_bs_read_u8(bs);
	b = gf_bs_read_u8(bs);
	a = gf_bs_read_u8(bs);
	col = a;
	col<<=8;
	col |= r;
	col<<=8;
	col |= g;
	col<<=8;
	col |= b;
	return col;
}

#define GPP_BOX_SIZE	8
void gpp_read_box(GF_BitStream *bs, GF_BoxRecord *rec)
{
	rec->top = gf_bs_read_u16(bs);
	rec->left = gf_bs_read_u16(bs);
	rec->bottom = gf_bs_read_u16(bs);
	rec->right = gf_bs_read_u16(bs);
}

#define GPP_STYLE_SIZE	12
void gpp_read_style(GF_BitStream *bs, GF_StyleRecord *rec)
{
	rec->startCharOffset = gf_bs_read_u16(bs);
	rec->endCharOffset = gf_bs_read_u16(bs);
	rec->fontID = gf_bs_read_u16(bs);
	rec->style_flags = gf_bs_read_u8(bs);
	rec->font_size = gf_bs_read_u8(bs);
	rec->text_color = gpp_read_rgba(bs);
}

GF_Err tx3g_on_child_box(GF_Box *s, GF_Box *a)
{
	GF_Tx3gSampleEntryBox *ptr = (GF_Tx3gSampleEntryBox*)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_FTAB:
		if (ptr->font_table) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->font_table = (GF_FontTableBox *)a;
		break;
	default:
		return GF_OK;
	}
	return GF_OK;
}

GF_Err tx3g_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_Tx3gSampleEntryBox *ptr = (GF_Tx3gSampleEntryBox*)s;

	if (ptr->size < 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE) return GF_ISOM_INVALID_FILE;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ptr->displayFlags = gf_bs_read_u32(bs);
	ptr->horizontal_justification = gf_bs_read_u8(bs);
	ptr->vertical_justification = gf_bs_read_u8(bs);
	ptr->back_color = gpp_read_rgba(bs);
	gpp_read_box(bs, &ptr->default_box);
	gpp_read_style(bs, &ptr->default_style);

	ISOM_DECREASE_SIZE(ptr, (18 + GPP_BOX_SIZE + GPP_STYLE_SIZE) );

	return gf_isom_box_array_read(s, bs, tx3g_on_child_box);
}

/*this is a quicktime specific box - see apple documentation*/
GF_Err text_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u16 pSize;
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ptr->displayFlags = gf_bs_read_u32(bs);			/*Display flags*/
	ptr->textJustification = gf_bs_read_u32(bs);	/*Text justification*/
	gf_bs_read_data(bs, ptr->background_color, 6);	/*Background color*/
	gpp_read_box(bs, &ptr->default_box);			/*Default text box*/
	gf_bs_read_data(bs, ptr->reserved1, 8);			/*Reserved*/
	ptr->fontNumber = gf_bs_read_u16(bs);			/*Font number*/
	ptr->fontFace   = gf_bs_read_u16(bs);			/*Font face*/
	ptr->reserved2  = gf_bs_read_u8(bs);			/*Reserved*/
	ptr->reserved3  = gf_bs_read_u16(bs);			/*Reserved*/
	gf_bs_read_data(bs, ptr->foreground_color, 6);	/*Foreground color*/
	ISOM_DECREASE_SIZE(ptr, 51);

	/*ffmpeg compatibility with iPod streams: no pascal string*/
	if (!ptr->size)
		return GF_OK;

	pSize = gf_bs_read_u8(bs); /*a Pascal string begins with its size: get textName size*/
	ISOM_DECREASE_SIZE(ptr, 1);

	if (ptr->size < pSize) {
		u32 s = pSize;
		size_t i = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] text box doesn't use a Pascal string: trying to decode anyway.\n"));
		ptr->textName = (char*)gf_malloc((size_t)ptr->size + 1 + 1);
		do {
			char c = (char)s;
			if (c == '\0') {
				break;
			} else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
				ptr->textName[i] = c;
			} else {
				gf_free(ptr->textName);
				ptr->textName = NULL;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] text box doesn't use a Pascal string and contains non-chars. Abort.\n"));
				return GF_ISOM_INVALID_FILE;
			}
			i++;
			if (!ptr->size)
				break;
			ptr->size--;
			s = gf_bs_read_u8(bs);
		} while (s);

		ptr->textName[i] = '\0';				/*Font name*/
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] text box doesn't use a Pascal string: \"%s\" detected.\n", ptr->textName));
		return GF_OK;
	}
	if (pSize) {
		ptr->textName = (char*) gf_malloc(pSize+1 * sizeof(char));
		if (gf_bs_read_data(bs, ptr->textName, pSize) != pSize) {
			gf_free(ptr->textName);
			ptr->textName = NULL;
			return GF_ISOM_INVALID_FILE;
		}
		ptr->textName[pSize] = '\0';				/*Font name*/
	}
	ISOM_DECREASE_SIZE(ptr, pSize);
	return gf_isom_box_array_read(s, bs, NULL);
}

void gpp_write_rgba(GF_BitStream *bs, u32 col)
{
	gf_bs_write_u8(bs, (col>>16) & 0xFF);
	gf_bs_write_u8(bs, (col>>8) & 0xFF);
	gf_bs_write_u8(bs, (col) & 0xFF);
	gf_bs_write_u8(bs, (col>>24) & 0xFF);
}

void gpp_write_box(GF_BitStream *bs, GF_BoxRecord *rec)
{
	gf_bs_write_u16(bs, rec->top);
	gf_bs_write_u16(bs, rec->left);
	gf_bs_write_u16(bs, rec->bottom);
	gf_bs_write_u16(bs, rec->right);
}

#define GPP_STYLE_SIZE	12
void gpp_write_style(GF_BitStream *bs, GF_StyleRecord *rec)
{
	gf_bs_write_u16(bs, rec->startCharOffset);
	gf_bs_write_u16(bs, rec->endCharOffset);
	gf_bs_write_u16(bs, rec->fontID);
	gf_bs_write_u8(bs, rec->style_flags);
	gf_bs_write_u8(bs, rec->font_size);
	gpp_write_rgba(bs, rec->text_color);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tx3g_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_Tx3gSampleEntryBox *ptr = (GF_Tx3gSampleEntryBox*)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	gf_bs_write_u32(bs, ptr->displayFlags);
	gf_bs_write_u8(bs, ptr->horizontal_justification);
	gf_bs_write_u8(bs, ptr->vertical_justification);
	gpp_write_rgba(bs, ptr->back_color);
	gpp_write_box(bs, &ptr->default_box);
	gpp_write_style(bs, &ptr->default_style);
	return GF_OK;
}

GF_Err text_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u16 pSize;
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	gf_bs_write_u32(bs, ptr->displayFlags);			/*Display flags*/
	gf_bs_write_u32(bs, ptr->textJustification);	/*Text justification*/
	gf_bs_write_data(bs, ptr->background_color, 6);	/*Background color*/
	gpp_write_box(bs, &ptr->default_box);			/*Default text box*/
	gf_bs_write_data(bs, ptr->reserved1, 8);		/*Reserved*/
	gf_bs_write_u16(bs, ptr->fontNumber);			/*Font number*/
	gf_bs_write_u16(bs, ptr->fontFace);				/*Font face*/
	gf_bs_write_u8(bs, ptr->reserved2);				/*Reserved*/
	gf_bs_write_u16(bs, ptr->reserved3);			/*Reserved*/
	gf_bs_write_data(bs, ptr->foreground_color, 6);	/*Foreground color*/
	//pSize assignment below is not a mistake
	if (ptr->textName && (pSize = (u16) strlen(ptr->textName))) {
		gf_bs_write_u8(bs, pSize);					/*a Pascal string begins with its size*/
		gf_bs_write_data(bs, ptr->textName, pSize);	/*Font name*/
	} else {
		gf_bs_write_u8(bs, 0);
	}
	return GF_OK;
}

GF_Err tx3g_box_size(GF_Box *s)
{
	/*base + this  + box + style*/
	s->size += 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE;
	return GF_OK;
}

GF_Err text_box_size(GF_Box *s)
{
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;

	/*base + this + string length*/
	s->size += 51 + 1;
	if (ptr->textName)
		s->size += strlen(ptr->textName);
	return GF_OK;
}

#endif

GF_Box *styl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextStyleBox, GF_ISOM_BOX_TYPE_STYL);
	return (GF_Box *) tmp;
}

void styl_box_del(GF_Box *s)
{
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	if (ptr->styles) gf_free(ptr->styles);
	gf_free(ptr);
}

GF_Err styl_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	ptr->entry_count = gf_bs_read_u16(bs);
	if (ptr->entry_count) {
		ptr->styles = (GF_StyleRecord*)gf_malloc(sizeof(GF_StyleRecord)*ptr->entry_count);
		for (i=0; i<ptr->entry_count; i++) {
			gpp_read_style(bs, &ptr->styles[i]);
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err styl_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->entry_count);
	for (i=0; i<ptr->entry_count; i++) gpp_write_style(bs, &ptr->styles[i]);
	return GF_OK;
}

GF_Err styl_box_size(GF_Box *s)
{
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;

	s->size += 2 + ptr->entry_count * GPP_STYLE_SIZE;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hlit_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextHighlightBox, GF_ISOM_BOX_TYPE_HLIT);
	return (GF_Box *) tmp;
}

void hlit_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err hlit_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextHighlightBox *ptr = (GF_TextHighlightBox *)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hlit_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextHighlightBox *ptr = (GF_TextHighlightBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->startcharoffset);
	gf_bs_write_u16(bs, ptr->endcharoffset);
	return GF_OK;
}

GF_Err hlit_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hclr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextHighlightColorBox, GF_ISOM_BOX_TYPE_HCLR);
	return (GF_Box *) tmp;
}

void hclr_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err hclr_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextHighlightColorBox*ptr = (GF_TextHighlightColorBox*)s;
	ptr->hil_color = gpp_read_rgba(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hclr_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextHighlightColorBox*ptr = (GF_TextHighlightColorBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gpp_write_rgba(bs, ptr->hil_color);
	return GF_OK;
}

GF_Err hclr_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *krok_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextKaraokeBox, GF_ISOM_BOX_TYPE_KROK);
	return (GF_Box *) tmp;
}

void krok_box_del(GF_Box *s)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	if (ptr->records) gf_free(ptr->records);
	gf_free(ptr);
}

GF_Err krok_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;

	ptr->highlight_starttime = gf_bs_read_u32(bs);
	ptr->nb_entries = gf_bs_read_u16(bs);
	if (ptr->nb_entries) {
		u32 i;
		ptr->records = (KaraokeRecord*)gf_malloc(sizeof(KaraokeRecord)*ptr->nb_entries);
		for (i=0; i<ptr->nb_entries; i++) {
			ptr->records[i].highlight_endtime = gf_bs_read_u32(bs);
			ptr->records[i].start_charoffset = gf_bs_read_u16(bs);
			ptr->records[i].end_charoffset = gf_bs_read_u16(bs);
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err krok_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->highlight_starttime);
	gf_bs_write_u16(bs, ptr->nb_entries);
	for (i=0; i<ptr->nb_entries; i++) {
		gf_bs_write_u32(bs, ptr->records[i].highlight_endtime);
		gf_bs_write_u16(bs, ptr->records[i].start_charoffset);
		gf_bs_write_u16(bs, ptr->records[i].end_charoffset);
	}
	return GF_OK;
}

GF_Err krok_box_size(GF_Box *s)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	s->size += 6 + 8*ptr->nb_entries;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dlay_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextScrollDelayBox, GF_ISOM_BOX_TYPE_DLAY);
	return (GF_Box *) tmp;
}

void dlay_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err dlay_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextScrollDelayBox*ptr = (GF_TextScrollDelayBox*)s;
	ptr->scroll_delay = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dlay_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextScrollDelayBox*ptr = (GF_TextScrollDelayBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->scroll_delay);
	return GF_OK;
}

GF_Err dlay_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *href_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextHyperTextBox, GF_ISOM_BOX_TYPE_HREF);
	return (GF_Box *) tmp;
}

void href_box_del(GF_Box *s)
{
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	if (ptr->URL) gf_free(ptr->URL);
	if (ptr->URL_hint) gf_free(ptr->URL_hint);
	gf_free(ptr);
}

GF_Err href_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	len = gf_bs_read_u8(bs);
	if (len) {
		ptr->URL = (char *) gf_malloc(sizeof(char) * (len+1));
		gf_bs_read_data(bs, ptr->URL, len);
		ptr->URL[len] = 0;
	}
	len = gf_bs_read_u8(bs);
	if (len) {
		ptr->URL_hint = (char *) gf_malloc(sizeof(char) * (len+1));
		gf_bs_read_data(bs, ptr->URL_hint, len);
		ptr->URL_hint[len]= 0;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err href_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_Err e;
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->startcharoffset);
	gf_bs_write_u16(bs, ptr->endcharoffset);
	if (ptr->URL) {
		len = (u32) strlen(ptr->URL);
		gf_bs_write_u8(bs, len);
		gf_bs_write_data(bs, ptr->URL, len);
	} else {
		gf_bs_write_u8(bs, 0);
	}
	if (ptr->URL_hint) {
		len = (u32) strlen(ptr->URL_hint);
		gf_bs_write_u8(bs, len);
		gf_bs_write_data(bs, ptr->URL_hint, len);
	} else {
		gf_bs_write_u8(bs, 0);
	}
	return GF_OK;
}

GF_Err href_box_size(GF_Box *s)
{
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	s->size += 6;
	if (ptr->URL) s->size += strlen(ptr->URL);
	if (ptr->URL_hint) s->size += strlen(ptr->URL_hint);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *tbox_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextBoxBox, GF_ISOM_BOX_TYPE_TBOX);
	return (GF_Box *) tmp;
}

void tbox_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err tbox_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextBoxBox*ptr = (GF_TextBoxBox*)s;
	gpp_read_box(bs, &ptr->box);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tbox_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextBoxBox*ptr = (GF_TextBoxBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gpp_write_box(bs, &ptr->box);
	return GF_OK;
}

GF_Err tbox_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *blnk_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextBlinkBox, GF_ISOM_BOX_TYPE_BLNK);
	return (GF_Box *) tmp;
}

void blnk_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err blnk_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextBlinkBox*ptr = (GF_TextBlinkBox*)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err blnk_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextBlinkBox*ptr = (GF_TextBlinkBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->startcharoffset);
	gf_bs_write_u16(bs, ptr->endcharoffset);
	return GF_OK;
}

GF_Err blnk_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *twrp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextWrapBox, GF_ISOM_BOX_TYPE_TWRP);
	return (GF_Box *) tmp;
}

void twrp_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err twrp_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextWrapBox*ptr = (GF_TextWrapBox*)s;
	ptr->wrap_flag = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err twrp_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextWrapBox*ptr = (GF_TextWrapBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->wrap_flag);
	return GF_OK;
}
GF_Err twrp_box_size(GF_Box *s)
{
	s->size += 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void tsel_box_del(GF_Box *s)
{
	GF_TrackSelectionBox *ptr;
	ptr = (GF_TrackSelectionBox *) s;
	if (ptr == NULL) return;
	if (ptr->attributeList) gf_free(ptr->attributeList);
	gf_free(ptr);
}

GF_Err tsel_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *) s;

	ptr->switchGroup = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->size % 4) return GF_ISOM_INVALID_FILE;
	ptr->attributeListCount = (u32)ptr->size/4;
	ptr->attributeList = gf_malloc(ptr->attributeListCount*sizeof(u32));
	if (ptr->attributeList == NULL) return GF_OUT_OF_MEM;

	for (i=0; i< ptr->attributeListCount; i++) {
		ptr->attributeList[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *tsel_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackSelectionBox, GF_ISOM_BOX_TYPE_TSEL);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tsel_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs,ptr->switchGroup);

	for (i = 0; i < ptr->attributeListCount; i++ ) {
		gf_bs_write_u32(bs, ptr->attributeList[i]);
	}

	return GF_OK;
}

GF_Err tsel_box_size(GF_Box *s)
{
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *) s;
	ptr->size += 4 + (4*ptr->attributeListCount);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *dimC_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMSSceneConfigBox, GF_ISOM_BOX_TYPE_DIMC);
	return (GF_Box *)tmp;
}
void dimC_box_del(GF_Box *s)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
	if (p->contentEncoding) gf_free(p->contentEncoding);
	if (p->textEncoding) gf_free(p->textEncoding);
	gf_free(p);
}

GF_Err dimC_box_read(GF_Box *s, GF_BitStream *bs)
{
	char str[1024];
	u32 i;
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;

	p->profile = gf_bs_read_u8(bs);
	p->level = gf_bs_read_u8(bs);
	p->pathComponents = gf_bs_read_int(bs, 4);
	p->fullRequestHost = gf_bs_read_int(bs, 1);
	p->streamType = gf_bs_read_int(bs, 1);
	p->containsRedundant = gf_bs_read_int(bs, 2);

	ISOM_DECREASE_SIZE(p, 3);

	i=0;
	str[0]=0;
	while (1) {
		str[i] = gf_bs_read_u8(bs);
		if (!str[i]) break;
		i++;
	}
	ISOM_DECREASE_SIZE(p, i);

	p->textEncoding = gf_strdup(str);

	i=0;
	str[0]=0;
	while (1) {
		str[i] = gf_bs_read_u8(bs);
		if (!str[i]) break;
		i++;
	}
	ISOM_DECREASE_SIZE(p, i);

	p->contentEncoding = gf_strdup(str);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dimC_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, p->profile);
	gf_bs_write_u8(bs, p->level);
	gf_bs_write_int(bs, p->pathComponents, 4);
	gf_bs_write_int(bs, p->fullRequestHost, 1);
	gf_bs_write_int(bs, p->streamType, 1);
	gf_bs_write_int(bs, p->containsRedundant, 2);
    if (p->textEncoding)
        gf_bs_write_data(bs, p->textEncoding, (u32) strlen(p->textEncoding));
    gf_bs_write_u8(bs, 0);
    if (p->contentEncoding)
        gf_bs_write_data(bs, p->contentEncoding, (u32) strlen(p->contentEncoding));
    gf_bs_write_u8(bs, 0);
	return GF_OK;
}
GF_Err dimC_box_size(GF_Box *s)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
    s->size += 3 + 2;
    if (p->textEncoding) s->size += strlen(p->textEncoding);
    if (p->contentEncoding) s->size += strlen(p->contentEncoding);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *diST_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMSScriptTypesBox, GF_ISOM_BOX_TYPE_DIST);
	return (GF_Box *)tmp;
}
void diST_box_del(GF_Box *s)
{
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox *)s;
	if (p->content_script_types) gf_free(p->content_script_types);
	gf_free(p);
}

GF_Err diST_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	char str[1024];
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox *)s;

	i=0;
	str[0]=0;
	while (1) {
		str[i] = gf_bs_read_u8(bs);
		if (!str[i]) break;
		i++;
	}
	ISOM_DECREASE_SIZE(p, i);

	p->content_script_types = gf_strdup(str);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err diST_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (p->content_script_types)
		gf_bs_write_data(bs, p->content_script_types, (u32) strlen(p->content_script_types)+1);
	else
		gf_bs_write_u8(bs, 0);
	return GF_OK;
}
GF_Err diST_box_size(GF_Box *s)
{
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox *)s;
	s->size += p->content_script_types ? (strlen(p->content_script_types)+1) : 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *dims_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMSSampleEntryBox, GF_ISOM_BOX_TYPE_DIMS);
	return (GF_Box*)tmp;
}
void dims_box_del(GF_Box *s)
{
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	gf_free(s);
}

static GF_Err dims_on_child_box(GF_Box *s, GF_Box *a)
{
	GF_DIMSSampleEntryBox *ptr = (GF_DIMSSampleEntryBox  *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_DIMC:
		if (ptr->config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->config = (GF_DIMSSceneConfigBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_DIST:
		if (ptr->scripts) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->scripts = (GF_DIMSScriptTypesBox*)a;
		break;
	}
	return GF_OK;
}
GF_Err dims_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)p, bs);
	if (e) return e;

	ISOM_DECREASE_SIZE(p, 8);
	return gf_isom_box_array_read(s, bs, dims_on_child_box);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dims_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, p->reserved, 6);
	gf_bs_write_u16(bs, p->dataReferenceIndex);
	return GF_OK;
}

GF_Err dims_box_size(GF_Box *s)
{
	u32 pos = 0;
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;
	s->size += 8;
	gf_isom_check_position(s, (GF_Box *) p->config, &pos);
	gf_isom_check_position(s, (GF_Box *) p->scripts, &pos);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
