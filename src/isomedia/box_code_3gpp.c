/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

void gppa_del(GF_Box *s)
{
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	if (ptr->protection_info) gf_isom_box_del((GF_Box *)ptr->protection_info);
	free(ptr);
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
	GF_3GPPAudioSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_3GPPAudioSampleEntryBox));
	if (tmp == NULL) return NULL;
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	tmp->type = type;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

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

#endif //GPAC_READ_ONLY


GF_Box *gppv_New(u32 type)
{
	GF_3GPPVisualSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_3GPPVisualSampleEntryBox));
	if (tmp == NULL) return NULL;
	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox *)tmp);
	tmp->type = type;
	return (GF_Box *)tmp;
}
void gppv_del(GF_Box *s)
{
	GF_3GPPVisualSampleEntryBox *ptr = (GF_3GPPVisualSampleEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	if (ptr->protection_info) gf_isom_box_del((GF_Box *)ptr->protection_info);
	free(ptr);
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

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

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

#endif //GPAC_READ_ONLY


GF_Box *gppc_New(u32 type)
{
	GF_3GPPConfigBox *tmp = (GF_3GPPConfigBox *) malloc(sizeof(GF_3GPPConfigBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_3GPPConfigBox));
	tmp->type = type;
	return (GF_Box *)tmp;
}

void gppc_del(GF_Box *s)
{
	GF_3GPPConfigBox *ptr = (GF_3GPPConfigBox *)s;
	if (ptr == NULL) return;
	free(ptr);
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

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

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

#endif //GPAC_READ_ONLY


GF_Box *ftab_New()
{
	GF_FontTableBox *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_FontTableBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_FTAB;
	return (GF_Box *) tmp;
}
void ftab_del(GF_Box *s)
{
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	if (ptr->fonts) {
		u32 i;
		for (i=0; i<ptr->entry_count; i++) 
			if (ptr->fonts[i].fontName) free(ptr->fonts[i].fontName);
		free(ptr->fonts);
	}
	free(ptr);
}
GF_Err ftab_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_FontTableBox *ptr = (GF_FontTableBox *)s;
	ptr->entry_count = gf_bs_read_u16(bs);
	GF_SAFEALLOC(ptr->fonts, sizeof(GF_FontRecord)*ptr->entry_count);
	for (i=0; i<ptr->entry_count; i++) {
		u32 len;
		ptr->fonts[i].fontID = gf_bs_read_u16(bs);
		len = gf_bs_read_u8(bs);
		if (len) {
			GF_SAFEALLOC(ptr->fonts[i].fontName, sizeof(char)*(len+1));
			gf_bs_read_data(bs, ptr->fonts[i].fontName, len);
		}
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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
			u32 len = strlen(ptr->fonts[i].fontName);
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

#endif




GF_Box *tx3g_New()
{
	GF_TextSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextSampleEntryBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TX3G;
	return (GF_Box *) tmp;
}

void tx3g_del(GF_Box *s)
{
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;
	if (ptr->font_table) gf_isom_box_del((GF_Box *)ptr->font_table);
	free(ptr);
}

static u32 gpp_read_rgba(GF_BitStream *bs)
{
	u8 r, g, b, a;
	u32 col;
	r = gf_bs_read_u8(bs);
	g = gf_bs_read_u8(bs);
	b = gf_bs_read_u8(bs);
	a = gf_bs_read_u8(bs);
	col = a; col<<=8; 
	col |= r; col<<=8; 
	col |= g; col<<=8; 
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
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;

	if (ptr->size < 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE) return GF_ISOM_INVALID_FILE;

	gf_bs_read_data(bs, (unsigned char*)ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);
	ptr->displayFlags = gf_bs_read_u32(bs);
	ptr->horizontal_justification = gf_bs_read_u8(bs);
	ptr->vertical_justification = gf_bs_read_u8(bs);
	ptr->back_color = gpp_read_rgba(bs);
	gpp_read_box(bs, &ptr->default_box);
	gpp_read_style(bs, &ptr->default_style);
	ptr->size -= 18 + GPP_BOX_SIZE + GPP_STYLE_SIZE;

	while (ptr->size) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;
		ptr->size -= a->size;
		if (a->type==GF_ISOM_BOX_TYPE_FTAB) {
			if (ptr->font_table) gf_isom_box_del((GF_Box *) ptr->font_table);
			ptr->font_table = (GF_FontTableBox *)a;
		} else {
			gf_isom_box_del(a);
		}
	}
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

#ifndef GPAC_READ_ONLY

GF_Err tx3g_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, (unsigned char*)ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	gf_bs_write_u32(bs, ptr->displayFlags);
	gf_bs_write_u8(bs, ptr->horizontal_justification);
	gf_bs_write_u8(bs, ptr->vertical_justification);
	gpp_write_rgba(bs, ptr->back_color);
	gpp_write_box(bs, &ptr->default_box);
	gpp_write_style(bs, &ptr->default_style);
	return gf_isom_box_write((GF_Box *) ptr->font_table, bs);
}

GF_Err tx3g_Size(GF_Box *s)
{
	GF_TextSampleEntryBox *ptr = (GF_TextSampleEntryBox*)s;
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

#endif

GF_Box *styl_New()
{
	GF_TextStyleBox *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextStyleBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_STYL;
	return (GF_Box *) tmp;
}

void styl_del(GF_Box *s)
{
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	if (ptr->styles) free(ptr->styles);
	free(ptr);
}

GF_Err styl_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	ptr->entry_count = gf_bs_read_u16(bs);
	if (ptr->entry_count) {
		GF_SAFEALLOC(ptr->styles, sizeof(GF_StyleRecord)*ptr->entry_count);
		for (i=0; i<ptr->entry_count; i++) {
			gpp_read_style(bs, &ptr->styles[i]);
		}
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
GF_Err styl_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TextStyleBox*ptr = (GF_TextStyleBox*)s;
	e = gf_isom_box_write_header(s, bs);

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

#endif

GF_Box *hlit_New()
{
	GF_TextHighlightBox *tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextHighlightBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_HLIT;
	return (GF_Box *) tmp;
}

void hlit_del(GF_Box *s)
{
	free(s);
}

GF_Err hlit_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextHighlightBox *ptr = (GF_TextHighlightBox *)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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

#endif

GF_Box *hclr_New()
{
	GF_TextHighlightColorBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextHighlightColorBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_HCLR;
	return (GF_Box *) tmp;
}

void hclr_del(GF_Box *s)
{
	free(s);
}

GF_Err hclr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextHighlightColorBox*ptr = (GF_TextHighlightColorBox*)s;
	ptr->hil_color = gpp_read_rgba(bs);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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

#endif

GF_Box *krok_New()
{
	GF_TextKaraokeBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextKaraokeBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_KROK;
	return (GF_Box *) tmp;
}

void krok_del(GF_Box *s)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	if (ptr->records) free(ptr->records);
	free(ptr);
}

GF_Err krok_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;

	ptr->highlight_starttime = gf_bs_read_u32(bs);
	ptr->entrycount = gf_bs_read_u16(bs);
	if (ptr->entrycount) {
		u32 i;
		GF_SAFEALLOC(ptr->records, sizeof(KaraokeRecord)*ptr->entrycount);
		for (i=0; i<ptr->entrycount; i++) {
			ptr->records[i].highlight_endtime = gf_bs_read_u32(bs);
			ptr->records[i].start_charoffset = gf_bs_read_u16(bs);
			ptr->records[i].end_charoffset = gf_bs_read_u16(bs);
		}
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
GF_Err krok_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TextKaraokeBox*ptr = (GF_TextKaraokeBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->highlight_starttime);
	gf_bs_write_u16(bs, ptr->entrycount);
	for (i=0; i<ptr->entrycount; i++) {
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
	s->size += 6 * 8*ptr->entrycount;
	return GF_OK;
}

#endif

GF_Box *dlay_New()
{
	GF_TextScrollDelayBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextScrollDelayBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_DLAY;
	return (GF_Box *) tmp;
}

void dlay_del(GF_Box *s)
{
	free(s);
}

GF_Err dlay_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextScrollDelayBox*ptr = (GF_TextScrollDelayBox*)s;
	ptr->scroll_delay = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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

#endif

GF_Box *href_New()
{
	GF_TextHyperTextBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextHyperTextBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_HREF;
	return (GF_Box *) tmp;
}

void href_del(GF_Box *s)
{
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	if (ptr->URL) free(ptr->URL);
	if (ptr->URL_hint) free(ptr->URL_hint);
	free(ptr);
}

GF_Err href_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_TextHyperTextBox*ptr = (GF_TextHyperTextBox*)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	len = gf_bs_read_u8(bs);
	if (len) {
		GF_SAFEALLOC(ptr->URL, sizeof(char) * (len+1));
		gf_bs_read_data(bs, ptr->URL, len);
	}
	len = gf_bs_read_u8(bs);
	if (len) {
		GF_SAFEALLOC(ptr->URL_hint, sizeof(char) * (len+1));
		gf_bs_read_data(bs, ptr->URL_hint, len);
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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
		len = strlen(ptr->URL);
		gf_bs_write_u8(bs, len);
		gf_bs_write_data(bs, ptr->URL, len);
	} else {
		gf_bs_write_u8(bs, 0);
	}
	if (ptr->URL_hint) {
		len = strlen(ptr->URL_hint);
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

#endif


GF_Box *tbox_New()
{
	GF_TextBoxBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextBoxBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TBOX;
	return (GF_Box *) tmp;
}

void tbox_del(GF_Box *s)
{
	free(s);
}

GF_Err tbox_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextBoxBox*ptr = (GF_TextBoxBox*)s;
	gpp_read_box(bs, &ptr->box);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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

#endif


GF_Box *blnk_New()
{
	GF_TextBlinkBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextBlinkBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_BLNK;
	return (GF_Box *) tmp;
}

void blnk_del(GF_Box *s)
{
	free(s);
}

GF_Err blnk_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextBlinkBox*ptr = (GF_TextBlinkBox*)s;
	ptr->startcharoffset = gf_bs_read_u16(bs);
	ptr->endcharoffset = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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

#endif

GF_Box *twrp_New()
{
	GF_TextWrapBox*tmp;
	GF_SAFEALLOC(tmp, sizeof(GF_TextWrapBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TWRP;
	return (GF_Box *) tmp;
}

void twrp_del(GF_Box *s)
{
	free(s);
}

GF_Err twrp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextWrapBox*ptr = (GF_TextWrapBox*)s;
	ptr->wrap_flag = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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

#endif

