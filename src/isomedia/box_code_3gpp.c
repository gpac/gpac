/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

void gppa_del(GF_Box *s)
{
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);

	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	gf_free(ptr);
}


GF_Err gppa_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	e = gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox*)s, bs);
	if (e) return e;
	e = gf_isom_parse_box((GF_Box **)&ptr->info, bs);
	if (e) return e;
	ptr->info->cfg.type = ptr->type;
	return GF_OK;
}

GF_Box *gppa_New(u32 type)
{
	ISOM_DECL_BOX_ALLOC(GF_3GPPAudioSampleEntryBox, type);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gppa_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);
	return gf_isom_box_write((GF_Box *)ptr->info, bs);
}

GF_Err gppa_Size(GF_Box *s)
{
	GF_Err e;
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox*)s);
	e = gf_isom_box_size((GF_Box *)ptr->info);
	if (e) return e;
	ptr->size += ptr->info->size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *gppv_New(u32 type)
{
	ISOM_DECL_BOX_ALLOC(GF_3GPPVisualSampleEntryBox, type);
	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox *)tmp);
	return (GF_Box *)tmp;
}
void gppv_del(GF_Box *s)
{
	GF_3GPPVisualSampleEntryBox *ptr = (GF_3GPPVisualSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	gf_free(ptr);
}

GF_Err gppv_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_3GPPVisualSampleEntryBox *ptr = (GF_3GPPVisualSampleEntryBox *)s;
	e = gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *)ptr, bs);
	if (e) return e;
	/*FIXME - check for any other boxes...*/
	e = gf_isom_parse_box((GF_Box **)&ptr->info, bs);
	return e;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gppv_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_3GPPVisualSampleEntryBox *ptr = (GF_3GPPVisualSampleEntryBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_video_sample_entry_write((GF_VisualSampleEntryBox *)s, bs);
	e = gf_isom_box_write((GF_Box *)ptr->info, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err gppv_Size(GF_Box *s)
{
	GF_Err e;
	GF_3GPPVisualSampleEntryBox *ptr = (GF_3GPPVisualSampleEntryBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	gf_isom_video_sample_entry_size((GF_VisualSampleEntryBox *)s);
	e = gf_isom_box_size((GF_Box *)ptr->info);
	if (e) return e;
	ptr->size += ptr->info->size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *gppc_New(u32 type)
{
	ISOM_DECL_BOX_ALLOC(GF_3GPPConfigBox, type);
	return (GF_Box *)tmp;
}

void gppc_del(GF_Box *s)
{
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err gppc_Read(GF_Box *s, GF_BitStream *bs)
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

GF_Err gppc_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err gppc_Size(GF_Box *s)
{
	GF_Err e;
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;

	e = gf_isom_box_get_size(s);
	if (e) return e;
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


GF_Box *ftab_New()
{
	ISOM_DECL_BOX_ALLOC(GF_FontTableBox, GF_ISOM_BOX_TYPE_FTAB);
	return (GF_Box *) tmp;
}
void ftab_del(GF_Box *s)
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
GF_Err ftab_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	ptr->entry_count = gf_bs_read_u16(bs);
	ptr->size -= 2;
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
GF_Err ftab_Write(GF_Box *s, GF_BitStream *bs)
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
GF_Err ftab_Size(GF_Box *s)
{
	u32 i;
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 2;
	for (i=0; i<ptr->entry_count; i++) {
		s->size += 3;
		if (ptr->fonts[i].fontName) s->size += strlen(ptr->fonts[i].fontName);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *text_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextSampleEntryBox, GF_ISOM_BOX_TYPE_TEXT);
	return (GF_Box *) tmp;
}

void text_del(GF_Box *s)
{
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->textName)
		gf_free(ptr->textName);
	gf_free(ptr);
}

GF_Box *tx3g_New()
{
	ISOM_DECL_BOX_ALLOC(GF_Tx3gSampleEntryBox, GF_ISOM_BOX_TYPE_TX3G);
	return (GF_Box *) tmp;
}

void tx3g_del(GF_Box *s)
{
	GF_Tx3gSampleEntryBox *ptr = (GF_Tx3gSampleEntryBox*)s;

	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->font_table)
		gf_isom_box_del((GF_Box *)ptr->font_table);
	gf_free(ptr);
}

static u32 gpp_read_rgba(GF_BitStream *bs)
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
static void gpp_read_box(GF_BitStream *bs, GF_BoxRecord *rec)
{
	rec->top = gf_bs_read_u16(bs);
	rec->left = gf_bs_read_u16(bs);
	rec->bottom = gf_bs_read_u16(bs);
	rec->right = gf_bs_read_u16(bs);
}

#define GPP_STYLE_SIZE	12
static void gpp_read_style(GF_BitStream *bs, GF_StyleRecord *rec)
{
	rec->startCharOffset = gf_bs_read_u16(bs);
	rec->endCharOffset = gf_bs_read_u16(bs);
	rec->fontID = gf_bs_read_u16(bs);
	rec->style_flags = gf_bs_read_u8(bs);
	rec->font_size = gf_bs_read_u8(bs);
	rec->text_color = gpp_read_rgba(bs);
}

GF_Err tx3g_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_Box *a;
	GF_Tx3gSampleEntryBox *ptr = (GF_Tx3gSampleEntryBox*)s;

	if (ptr->size < 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE) return GF_ISOM_INVALID_FILE;

	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);
	ptr->displayFlags = gf_bs_read_u32(bs);
	ptr->horizontal_justification = gf_bs_read_u8(bs);
	ptr->vertical_justification = gf_bs_read_u8(bs);
	ptr->back_color = gpp_read_rgba(bs);
	gpp_read_box(bs, &ptr->default_box);
	gpp_read_style(bs, &ptr->default_style);
	ptr->size -= 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE;

	while (ptr->size>=8) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		if (ptr->size<a->size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" larger than remaining bytes in tx3g - ignoring box\n", gf_4cc_to_str(a->type)));
			ptr->size = 0;
			gf_isom_box_del(a);
			return GF_OK;
		}

		ptr->size -= a->size;
		if (a->type==GF_ISOM_BOX_TYPE_FTAB) {
			if (ptr->font_table) gf_isom_box_del((GF_Box *) ptr->font_table);
			ptr->font_table = (GF_FontTableBox *)a;
		} else {
			e = gf_isom_box_add_default(s, a);
			if (e) return e;
		}
	}
	return GF_OK;
}

/*this is a quicktime specific box - see apple documentation*/
GF_Err text_Read(GF_Box *s, GF_BitStream *bs)
{
	u16 pSize;
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;
	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);

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
	if (ptr->size < 51)
		return GF_ISOM_INVALID_FILE;
	ptr->size -= 51;
	if (!ptr->size)
		return GF_OK; /*ffmpeg compatibility with iPod streams: no pascal string*/

	pSize = gf_bs_read_u8(bs); /*a Pascal string begins with its size: get textName size*/
	ptr->size -= 1;
	if (ptr->size < pSize) {
		u32 s = pSize;
		size_t i = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] text box doesn't use a Pascal string: trying to decode anyway.\n"));
		ptr->textName = (char*)gf_malloc((u32) ptr->size + 1 + 1);
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
	ptr->size -= pSize;

	return GF_OK;
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

GF_Err tx3g_Write(GF_Box *s, GF_BitStream *bs)
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
	return gf_isom_box_write((GF_Box *) ptr->font_table, bs);
}

GF_Err text_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err tx3g_Size(GF_Box *s)
{
	GF_Tx3gSampleEntryBox *ptr = (GF_Tx3gSampleEntryBox*)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	/*base + this  + box + style*/
	s->size += 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE;
	if (ptr->font_table) {
		e = gf_isom_box_size((GF_Box *) ptr->font_table);
		if (e) return e;
		s->size += ptr->font_table->size;
	}
	return GF_OK;
}

GF_Err text_Size(GF_Box *s)
{
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	/*base + this + string length*/
	s->size += 51 + 1;
	if (ptr->textName)
		s->size += strlen(ptr->textName);
	return GF_OK;
}

#endif

GF_Box *styl_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextStyleBox, GF_ISOM_BOX_TYPE_STYL);
	return (GF_Box *) tmp;
}

void styl_del(GF_Box *s)
{
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	if (ptr->styles) gf_free(ptr->styles);
	gf_free(ptr);
}

GF_Err styl_Read(GF_Box *s, GF_BitStream *bs)
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
GF_Err styl_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err styl_Size(GF_Box *s)
{
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 2 + ptr->entry_count * GPP_STYLE_SIZE;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hlit_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextHighlightBox, GF_ISOM_BOX_TYPE_HLIT);
	return (GF_Box *) tmp;
}

void hlit_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err hlit_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextHighlightBox *ptr = (GF_TextHighlightBox *)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hlit_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextHighlightBox *ptr = (GF_TextHighlightBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->startcharoffset);
	gf_bs_write_u16(bs, ptr->endcharoffset);
	return GF_OK;
}

GF_Err hlit_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hclr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextHighlightColorBox, GF_ISOM_BOX_TYPE_HCLR);
	return (GF_Box *) tmp;
}

void hclr_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err hclr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextHighlightColorBox*ptr = (GF_TextHighlightColorBox*)s;
	ptr->hil_color = gpp_read_rgba(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hclr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextHighlightColorBox*ptr = (GF_TextHighlightColorBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gpp_write_rgba(bs, ptr->hil_color);
	return GF_OK;
}

GF_Err hclr_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *krok_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextKaraokeBox, GF_ISOM_BOX_TYPE_KROK);
	return (GF_Box *) tmp;
}

void krok_del(GF_Box *s)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	if (ptr->records) gf_free(ptr->records);
	gf_free(ptr);
}

GF_Err krok_Read(GF_Box *s, GF_BitStream *bs)
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
GF_Err krok_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err krok_Size(GF_Box *s)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 6 * 8*ptr->nb_entries;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dlay_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextScrollDelayBox, GF_ISOM_BOX_TYPE_DLAY);
	return (GF_Box *) tmp;
}

void dlay_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err dlay_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextScrollDelayBox*ptr = (GF_TextScrollDelayBox*)s;
	ptr->scroll_delay = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dlay_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextScrollDelayBox*ptr = (GF_TextScrollDelayBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->scroll_delay);
	return GF_OK;
}

GF_Err dlay_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *href_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextHyperTextBox, GF_ISOM_BOX_TYPE_HREF);
	return (GF_Box *) tmp;
}

void href_del(GF_Box *s)
{
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	if (ptr->URL) gf_free(ptr->URL);
	if (ptr->URL_hint) gf_free(ptr->URL_hint);
	gf_free(ptr);
}

GF_Err href_Read(GF_Box *s, GF_BitStream *bs)
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
GF_Err href_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err href_Size(GF_Box *s)
{
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 6;
	if (ptr->URL) s->size += strlen(ptr->URL);
	if (ptr->URL_hint) s->size += strlen(ptr->URL_hint);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *tbox_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextBoxBox, GF_ISOM_BOX_TYPE_TBOX);
	return (GF_Box *) tmp;
}

void tbox_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err tbox_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextBoxBox*ptr = (GF_TextBoxBox*)s;
	gpp_read_box(bs, &ptr->box);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tbox_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextBoxBox*ptr = (GF_TextBoxBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gpp_write_box(bs, &ptr->box);
	return GF_OK;
}

GF_Err tbox_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *blnk_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextBlinkBox, GF_ISOM_BOX_TYPE_BLNK);
	return (GF_Box *) tmp;
}

void blnk_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err blnk_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextBlinkBox*ptr = (GF_TextBlinkBox*)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err blnk_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextBlinkBox*ptr = (GF_TextBlinkBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->startcharoffset);
	gf_bs_write_u16(bs, ptr->endcharoffset);
	return GF_OK;
}

GF_Err blnk_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *twrp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextWrapBox, GF_ISOM_BOX_TYPE_TWRP);
	return (GF_Box *) tmp;
}

void twrp_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err twrp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextWrapBox*ptr = (GF_TextWrapBox*)s;
	ptr->wrap_flag = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err twrp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextWrapBox*ptr = (GF_TextWrapBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->wrap_flag);
	return GF_OK;
}
GF_Err twrp_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void tsel_del(GF_Box *s)
{
	GF_TrackSelectionBox *ptr;
	ptr = (GF_TrackSelectionBox *) s;
	if (ptr == NULL) return;
	if (ptr->attributeList) gf_free(ptr->attributeList);
	gf_free(ptr);
}

GF_Err tsel_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->switchGroup = gf_bs_read_u32(bs);
	ptr->size -= 4;
	if (ptr->size % 4) return GF_ISOM_INVALID_FILE;
	ptr->attributeListCount = (u32)ptr->size/4;
	ptr->attributeList = gf_malloc(ptr->attributeListCount*sizeof(u32));
	if (ptr->attributeList == NULL) return GF_OUT_OF_MEM;

	for (i=0; i< ptr->attributeListCount; i++) {
		ptr->attributeList[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *tsel_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackSelectionBox, GF_ISOM_BOX_TYPE_TSEL);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tsel_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err tsel_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *) s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (4*ptr->attributeListCount);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *dimC_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMSSceneConfigBox, GF_ISOM_BOX_TYPE_DIMC);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}
void dimC_del(GF_Box *s)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
	if (p->contentEncoding) gf_free(p->contentEncoding);
	if (p->textEncoding) gf_free(p->textEncoding);
	gf_free(p);
}

GF_Err dimC_Read(GF_Box *s, GF_BitStream *bs)
{
	char str[1024];
	u32 i;
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
	GF_Err e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	p->profile = gf_bs_read_u8(bs);
	p->level = gf_bs_read_u8(bs);
	p->pathComponents = gf_bs_read_int(bs, 4);
	p->fullRequestHost = gf_bs_read_int(bs, 1);
	p->streamType = gf_bs_read_int(bs, 1);
	p->containsRedundant = gf_bs_read_int(bs, 2);
	s->size -= 3;

	i=0;
	str[0]=0;
	while (1) {
		str[i] = gf_bs_read_u8(bs);
		if (!str[i]) break;
		i++;
	}
	if (s->size < i) return GF_ISOM_INVALID_FILE;
	s->size -= i;
	p->textEncoding = gf_strdup(str);

	i=0;
	str[0]=0;
	while (1) {
		str[i] = gf_bs_read_u8(bs);
		if (!str[i]) break;
		i++;
	}
	if (s->size < i) return GF_ISOM_INVALID_FILE;
	s->size -= i;
	p->contentEncoding = gf_strdup(str);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dimC_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
	GF_Err e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, p->profile);
	gf_bs_write_u8(bs, p->level);
	gf_bs_write_int(bs, p->pathComponents, 4);
	gf_bs_write_int(bs, p->fullRequestHost, 1);
	gf_bs_write_int(bs, p->streamType, 1);
	gf_bs_write_int(bs, p->containsRedundant, 2);
	gf_bs_write_data(bs, p->textEncoding, (u32) strlen(p->textEncoding)+1);
	gf_bs_write_data(bs, p->contentEncoding, (u32) strlen(p->contentEncoding)+1);
	return GF_OK;
}
GF_Err dimC_Size(GF_Box *s)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)s;
	GF_Err e = gf_isom_full_box_get_size(s);
	if (e) return e;
	s->size += 3 + 1 + strlen(p->textEncoding) + 1 + strlen(p->contentEncoding);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *diST_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMSScriptTypesBox, GF_ISOM_BOX_TYPE_DIST);
	return (GF_Box *)tmp;
}
void diST_del(GF_Box *s)
{
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox *)s;
	if (p->content_script_types) gf_free(p->content_script_types);
	gf_free(p);
}

GF_Err diST_Read(GF_Box *s, GF_BitStream *bs)
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
	if (s->size < i) return GF_ISOM_INVALID_FILE;
	s->size -= i;
	p->content_script_types = gf_strdup(str);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err diST_Write(GF_Box *s, GF_BitStream *bs)
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
GF_Err diST_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox *)s;
	if (e) return e;
	s->size += p->content_script_types ? (strlen(p->content_script_types)+1) : 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *dims_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMSSampleEntryBox, GF_ISOM_BOX_TYPE_DIMS);
	return (GF_Box*)tmp;
}
void dims_del(GF_Box *s)
{
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (p->config) gf_isom_box_del((GF_Box *)p->config);
	if (p->scripts) gf_isom_box_del((GF_Box *)p->scripts);
	gf_free(p);
}

static GF_Err dims_AddBox(GF_Box *s, GF_Box *a)
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
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(ptr->protections, a);
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}
GF_Err dims_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;
	gf_bs_read_data(bs, p->reserved, 6);
	p->dataReferenceIndex = gf_bs_read_u16(bs);
	p->size -= 8;
	return gf_isom_read_box_list(s, bs, dims_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dims_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, p->reserved, 6);
	gf_bs_write_u16(bs, p->dataReferenceIndex);
	if (p->config) {
		e = gf_isom_box_write((GF_Box *)p->config, bs);
		if (e) return e;
	}
	if (p->scripts) {
		e = gf_isom_box_write((GF_Box *)p->scripts, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, p->protections, bs);
}

GF_Err dims_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox *)s;
	if (e) return e;
	s->size += 8;

	if (p->config) {
		e = gf_isom_box_size((GF_Box *) p->config);
		if (e) return e;
		p->size += p->config->size;
	}
	if (p->scripts) {
		e = gf_isom_box_size((GF_Box *) p->scripts);
		if (e) return e;
		p->size += p->scripts->size;
	}
	return gf_isom_box_array_size(s, p->protections);
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
