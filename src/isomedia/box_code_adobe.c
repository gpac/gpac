/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Author: Romain Bouqueau, Jean Le Feuvre
 *			Copyright (c) Romain Bouqueau 2012- Telecom Paris 2019-
 *				All rights reserved
 *
 *          Note: this development was kindly sponsorized by Vizion'R (http://vizionr.com)
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ISOM_ADOBE

#ifndef GPAC_DISABLE_ISOM

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void abst_box_del(GF_Box *s)
{
	GF_AdobeBootstrapInfoBox *ptr = (GF_AdobeBootstrapInfoBox *)s;
	if (ptr == NULL) return;

	if (ptr->movie_identifier)
		gf_free(ptr->movie_identifier);
	if (ptr->drm_data)
		gf_free(ptr->drm_data);
	if (ptr->meta_data)
		gf_free(ptr->meta_data);

	while (gf_list_count(ptr->server_entry_table)) {
		gf_free(gf_list_get(ptr->server_entry_table, 0));
		gf_list_rem(ptr->server_entry_table, 0);
	}
	gf_list_del(ptr->server_entry_table);

	while (gf_list_count(ptr->quality_entry_table)) {
		gf_free(gf_list_get(ptr->quality_entry_table, 0));
		gf_list_rem(ptr->quality_entry_table, 0);
	}
	gf_list_del(ptr->quality_entry_table);


	while (gf_list_count(ptr->segment_run_table_entries)) {
		gf_isom_box_del((GF_Box *)gf_list_get(ptr->segment_run_table_entries, 0));
		gf_list_rem(ptr->segment_run_table_entries, 0);
	}
	gf_list_del(ptr->segment_run_table_entries);

	while (gf_list_count(ptr->fragment_run_table_entries)) {
		gf_isom_box_del((GF_Box *)gf_list_get(ptr->fragment_run_table_entries, 0));
		gf_list_rem(ptr->fragment_run_table_entries, 0);
	}
	gf_list_del(ptr->fragment_run_table_entries);

	gf_free(ptr);
}

GF_Err abst_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AdobeBootstrapInfoBox *ptr = (GF_AdobeBootstrapInfoBox *)s;
	int i;
	u32 tmp_strsize;
	char *tmp_str;
	GF_Err e;

	ptr->bootstrapinfo_version = gf_bs_read_u32(bs);
	ptr->profile = gf_bs_read_int(bs, 2);
	ptr->live = gf_bs_read_int(bs, 1);
	ptr->update = gf_bs_read_int(bs, 1);
	ptr->reserved = gf_bs_read_int(bs, 4);
	ptr->time_scale = gf_bs_read_u32(bs);
	ptr->current_media_time = gf_bs_read_u64(bs);
	ptr->smpte_time_code_offset = gf_bs_read_u64(bs);

	i=0;
	if (ptr->size<8) return GF_ISOM_INVALID_FILE;
	tmp_strsize=(u32)ptr->size-8;
	tmp_str = gf_malloc(sizeof(char)*tmp_strsize);

	while (tmp_strsize) {
		tmp_str[i] = gf_bs_read_u8(bs);
		tmp_strsize--;
		if (!tmp_str[i])
			break;
		i++;
	}
	if (i)
		ptr->movie_identifier = gf_strdup(tmp_str);

	ptr->server_entry_count = gf_bs_read_u8(bs);
	for (i=0; i<ptr->server_entry_count; i++) {
		int j=0;
		tmp_strsize=(u32)ptr->size-8;
		while (tmp_strsize) {
			tmp_str[j] = gf_bs_read_u8(bs);
			tmp_strsize--;
			if (!tmp_str[j])
				break;
			j++;
		}
		gf_list_insert(ptr->server_entry_table, gf_strdup(tmp_str), i);
	}

	ptr->quality_entry_count = gf_bs_read_u8(bs);
	for (i=0; i<ptr->quality_entry_count; i++) {
		int j=0;
		tmp_strsize=(u32)ptr->size-8;
		while (tmp_strsize) {
			tmp_str[j] = gf_bs_read_u8(bs);
			tmp_strsize--;
			if (!tmp_str[j])
				break;
			j++;
		}
		gf_list_insert(ptr->quality_entry_table, gf_strdup(tmp_str), i);
	}

	i=0;
	tmp_strsize=(u32)ptr->size-8;
	while (tmp_strsize) {
		tmp_str[i] = gf_bs_read_u8(bs);
		tmp_strsize--;
		if (!tmp_str[i])
			break;
		i++;
	}
	if (i)
		ptr->drm_data = gf_strdup(tmp_str);

	i=0;
	tmp_strsize=(u32)ptr->size-8;
	while (tmp_strsize) {
		tmp_str[i] = gf_bs_read_u8(bs);
		tmp_strsize--;
		if (!tmp_str[i])
			break;
		i++;
	}
	if (i)
		ptr->meta_data = gf_strdup(tmp_str);

	ptr->segment_run_table_count = gf_bs_read_u8(bs);
	for (i=0; i<ptr->segment_run_table_count; i++) {
		GF_AdobeSegmentRunTableBox *asrt;
		e = gf_isom_box_parse((GF_Box **)&asrt, bs);
		if (e) return e;
		gf_list_add(ptr->segment_run_table_entries, asrt);
	}

	ptr->fragment_run_table_count = gf_bs_read_u8(bs);
	for (i=0; i<ptr->fragment_run_table_count; i++) {
		GF_AdobeFragmentRunTableBox *afrt;
		e = gf_isom_box_parse((GF_Box **)&afrt, bs);
		if (e) return e;
		gf_list_add(ptr->fragment_run_table_entries, afrt);
	}

	gf_free(tmp_str);

	return GF_OK;
}

GF_Box *abst_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeBootstrapInfoBox, GF_ISOM_BOX_TYPE_ABST);
	tmp->server_entry_table = gf_list_new();
	tmp->quality_entry_table = gf_list_new();
	tmp->segment_run_table_entries = gf_list_new();
	tmp->fragment_run_table_entries = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err abst_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	unsigned int i;
	GF_AdobeBootstrapInfoBox *ptr = (GF_AdobeBootstrapInfoBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->bootstrapinfo_version);
	gf_bs_write_int(bs, ptr->profile, 2);
	gf_bs_write_int(bs, ptr->live, 1);
	gf_bs_write_int(bs, ptr->update, 1);
	gf_bs_write_int(bs, ptr->reserved, 4);
	gf_bs_write_u32(bs, ptr->time_scale);
	gf_bs_write_u64(bs, ptr->current_media_time);
	gf_bs_write_u64(bs, ptr->smpte_time_code_offset);
	if (ptr->movie_identifier)
		gf_bs_write_data(bs, ptr->movie_identifier, (u32)strlen(ptr->movie_identifier) + 1);
	else
		gf_bs_write_u8(bs, 0);

	gf_bs_write_u8(bs, ptr->server_entry_count);
	for (i=0; i<ptr->server_entry_count; i++) {
		char *str = (char*)gf_list_get(ptr->server_entry_table, i);
		gf_bs_write_data(bs, str, (u32)strlen(str) + 1);
	}

	gf_bs_write_u8(bs, ptr->quality_entry_count);
	for (i=0; i<ptr->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(ptr->quality_entry_table, i);
		gf_bs_write_data(bs, str, (u32)strlen(str) + 1);
	}

	if (ptr->drm_data)
		gf_bs_write_data(bs, ptr->drm_data, (u32)strlen(ptr->drm_data) + 1);
	else
		gf_bs_write_u8(bs, 0);

	if (ptr->meta_data)
		gf_bs_write_data(bs, ptr->meta_data, (u32)strlen(ptr->meta_data) + 1);
	else
		gf_bs_write_u8(bs, 0);

	gf_bs_write_u8(bs, ptr->segment_run_table_count);
	for (i=0; i<ptr->segment_run_table_count; i++) {
		e = gf_isom_box_write((GF_Box *)gf_list_get(ptr->segment_run_table_entries, i), bs);
		if (e) return e;
	}

	gf_bs_write_u8(bs, ptr->fragment_run_table_count);
	for (i=0; i<ptr->fragment_run_table_count; i++) {
		e = gf_isom_box_write((GF_Box *)gf_list_get(ptr->fragment_run_table_entries, i), bs);
		if (e) return e;
	}

	return GF_OK;
}

GF_Err abst_box_size(GF_Box *s)
{
	GF_Err e;
	u32 i;
	GF_AdobeBootstrapInfoBox *ptr = (GF_AdobeBootstrapInfoBox *)s;

	s->size += 25
	           + (ptr->movie_identifier ? (strlen(ptr->movie_identifier) + 1) : 1)
	           + 1;

	for (i=0; i<ptr->server_entry_count; i++)
		s->size += strlen(gf_list_get(ptr->server_entry_table, i)) + 1;

	s->size += 1;

	for (i=0; i<ptr->quality_entry_count; i++)
		s->size += strlen(gf_list_get(ptr->quality_entry_table, i)) + 1;

	s->size += (ptr->drm_data ? (strlen(ptr->drm_data) + 1) : 1)
	           + (ptr->meta_data ? (strlen(ptr->meta_data) + 1) : 1)
	           + 1;

	for (i=0; i<ptr->segment_run_table_count; i++) {
		GF_Box *box = (GF_Box *)gf_list_get(ptr->segment_run_table_entries, i);
		e = gf_isom_box_size(box);
		if (e) return e;
		s->size += box->size;
	}

	s->size += 1;
	for (i=0; i<ptr->fragment_run_table_count; i++) {
		GF_Box *box = (GF_Box *)gf_list_get(ptr->fragment_run_table_entries, i);
		e = gf_isom_box_size(box);
		if (e) return e;
		s->size += box->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void afra_box_del(GF_Box *s)
{
	GF_AdobeFragRandomAccessBox *ptr = (GF_AdobeFragRandomAccessBox *)s;
	if (ptr == NULL) return;

	while (gf_list_count(ptr->local_access_entries)) {
		gf_free(gf_list_get(ptr->local_access_entries, 0));
		gf_list_rem(ptr->local_access_entries, 0);
	}
	gf_list_del(ptr->local_access_entries);

	while (gf_list_count(ptr->global_access_entries)) {
		gf_free(gf_list_get(ptr->global_access_entries, 0));
		gf_list_rem(ptr->global_access_entries, 0);
	}
	gf_list_del(ptr->global_access_entries);

	gf_free(ptr);
}

GF_Err afra_box_read(GF_Box *s, GF_BitStream *bs)
{
	unsigned int i;
	GF_AdobeFragRandomAccessBox *ptr = (GF_AdobeFragRandomAccessBox *)s;

	ptr->long_ids = gf_bs_read_int(bs, 1);
	ptr->long_offsets = gf_bs_read_int(bs, 1);
	ptr->global_entries = gf_bs_read_int(bs, 1);
	ptr->reserved = gf_bs_read_int(bs, 5);
	ptr->time_scale = gf_bs_read_u32(bs);

	ptr->entry_count = gf_bs_read_u32(bs);
	for (i=0; i<ptr->entry_count; i++) {
		GF_AfraEntry *ae = gf_malloc(sizeof(GF_AfraEntry));

		ae->time = gf_bs_read_u64(bs);
		if (ptr->long_offsets)
			ae->offset = gf_bs_read_u64(bs);
		else
			ae->offset = gf_bs_read_u32(bs);

		gf_list_insert(ptr->local_access_entries, ae, i);
	}

	if (ptr->global_entries) {
		ptr->global_entry_count = gf_bs_read_u32(bs);
		for (i=0; i<ptr->global_entry_count; i++) {
			GF_GlobalAfraEntry *ae = gf_malloc(sizeof(GF_GlobalAfraEntry));

			ae->time = gf_bs_read_u64(bs);
			if (ptr->long_ids) {
				ae->segment = gf_bs_read_u32(bs);
				ae->fragment = gf_bs_read_u32(bs);
			} else {
				ae->segment = gf_bs_read_u16(bs);
				ae->fragment = gf_bs_read_u16(bs);
			}
			if (ptr->long_offsets) {
				ae->afra_offset = gf_bs_read_u64(bs);
				ae->offset_from_afra = gf_bs_read_u64(bs);
			} else {
				ae->afra_offset = gf_bs_read_u32(bs);
				ae->offset_from_afra = gf_bs_read_u32(bs);
			}

			gf_list_insert(ptr->global_access_entries, ae, i);
		}
	}

	return GF_OK;
}

GF_Box *afra_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeFragRandomAccessBox, GF_ISOM_BOX_TYPE_AFRA);
	tmp->local_access_entries = gf_list_new();
	tmp->global_access_entries = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err afra_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	unsigned int i;
	GF_AdobeFragRandomAccessBox *ptr = (GF_AdobeFragRandomAccessBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->long_ids, 1);
	gf_bs_write_int(bs, ptr->long_offsets, 1);
	gf_bs_write_int(bs, ptr->global_entries, 1);
	gf_bs_write_int(bs, 0, 5);
	gf_bs_write_u32(bs, ptr->time_scale);

	gf_bs_write_u32(bs, ptr->entry_count);
	for (i=0; i<ptr->entry_count; i++) {
		GF_AfraEntry *ae = (GF_AfraEntry *)gf_list_get(ptr->local_access_entries, i);
		gf_bs_write_u64(bs, ae->time);
		if (ptr->long_offsets)
			gf_bs_write_u64(bs, ae->offset);
		else
			gf_bs_write_u32(bs, (u32)ae->offset);
	}

	if (ptr->global_entries) {
		gf_bs_write_u32(bs, ptr->global_entry_count);
		for (i=0; i<ptr->global_entry_count; i++) {
			GF_GlobalAfraEntry *gae = (GF_GlobalAfraEntry *)gf_list_get(ptr->global_access_entries, i);
			gf_bs_write_u64(bs, gae->time);
			if (ptr->long_ids) {
				gf_bs_write_u32(bs, gae->segment);
				gf_bs_write_u32(bs, gae->fragment);
			} else {
				gf_bs_write_u16(bs, (u16)gae->segment);
				gf_bs_write_u16(bs, (u16)gae->fragment);
			}
			if (ptr->long_offsets) {
				gf_bs_write_u64(bs, gae->afra_offset);
				gf_bs_write_u64(bs, gae->offset_from_afra);
			} else {
				gf_bs_write_u32(bs, (u32)gae->afra_offset);
				gf_bs_write_u32(bs, (u32)gae->offset_from_afra);
			}
		}
	}

	return GF_OK;
}


GF_Err afra_box_size(GF_Box *s)
{
	GF_AdobeFragRandomAccessBox *ptr = (GF_AdobeFragRandomAccessBox *)s;

	s->size += 9
	           + ptr->entry_count * (ptr->long_offsets ? 16 : 12)
	           + (ptr->global_entries ? 4 + ptr->global_entry_count * (4 + (ptr->long_offsets ? 16 : 8) + (ptr->long_ids ? 8 : 4)) : 0);

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void asrt_box_del(GF_Box *s)
{
	GF_AdobeSegmentRunTableBox *ptr = (GF_AdobeSegmentRunTableBox *)s;
	if (ptr == NULL) return;

	while (gf_list_count(ptr->quality_segment_url_modifiers)) {
		gf_free(gf_list_get(ptr->quality_segment_url_modifiers, 0));
		gf_list_rem(ptr->quality_segment_url_modifiers, 0);
	}
	gf_list_del(ptr->quality_segment_url_modifiers);

	while (gf_list_count(ptr->segment_run_entry_table)) {
		gf_free(gf_list_get(ptr->segment_run_entry_table, 0));
		gf_list_rem(ptr->segment_run_entry_table, 0);
	}
	gf_list_del(ptr->segment_run_entry_table);

	gf_free(ptr);
}

GF_Err asrt_box_read(GF_Box *s, GF_BitStream *bs)
{
	unsigned int i;
	GF_AdobeSegmentRunTableBox *ptr = (GF_AdobeSegmentRunTableBox *)s;

	ptr->quality_entry_count = gf_bs_read_u8(bs);
	for (i=0; i<ptr->quality_entry_count; i++) {
		int j=0;
		u32 tmp_strsize=(u32)ptr->size-8;
		char *tmp_str = (char*) gf_malloc(tmp_strsize);
		while (tmp_strsize) {
			tmp_str[j] = gf_bs_read_u8(bs);
			tmp_strsize--;
			if (!tmp_str[j])
				break;
			j++;
		}
		gf_list_insert(ptr->quality_segment_url_modifiers, tmp_str, i);
	}

	ptr->segment_run_entry_count = gf_bs_read_u32(bs);
	for (i=0; i<ptr->segment_run_entry_count; i++) {
		GF_AdobeSegmentRunEntry *sre = gf_malloc(sizeof(GF_AdobeSegmentRunEntry));
		sre->first_segment = gf_bs_read_u32(bs);
		sre->fragment_per_segment = gf_bs_read_u32(bs);
		gf_list_insert(ptr->segment_run_entry_table, sre, i);
	}

	return GF_OK;
}

GF_Box *asrt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeSegmentRunTableBox, GF_ISOM_BOX_TYPE_ASRT);
	tmp->quality_segment_url_modifiers = gf_list_new();
	tmp->segment_run_entry_table = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err asrt_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	unsigned int i;
	GF_AdobeSegmentRunTableBox *ptr = (GF_AdobeSegmentRunTableBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->quality_entry_count);
	for (i=0; i<ptr->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(ptr->quality_segment_url_modifiers, i);
		gf_bs_write_data(bs, str, (u32)strlen(str) + 1);
	}

	gf_bs_write_u32(bs, ptr->segment_run_entry_count);
	for (i=0; i<ptr->segment_run_entry_count; i++) {
		GF_AdobeSegmentRunEntry *sre = (GF_AdobeSegmentRunEntry *)gf_list_get(ptr->segment_run_entry_table, i);
		gf_bs_write_u32(bs, sre->first_segment);
		gf_bs_write_u32(bs, sre->fragment_per_segment);
	}

	return GF_OK;
}


GF_Err asrt_box_size(GF_Box *s)
{
	int i;
	GF_AdobeSegmentRunTableBox *ptr = (GF_AdobeSegmentRunTableBox *)s;

	s->size += 5;

	for (i=0; i<ptr->quality_entry_count; i++)
		s->size += strlen(gf_list_get(ptr->quality_segment_url_modifiers, i)) + 1;

	s->size += ptr->segment_run_entry_count * sizeof(GF_AdobeSegmentRunEntry);

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void afrt_box_del(GF_Box *s)
{
	GF_AdobeFragmentRunTableBox *ptr = (GF_AdobeFragmentRunTableBox *)s;
	if (ptr == NULL) return;

	while (gf_list_count(ptr->quality_segment_url_modifiers)) {
		gf_free(gf_list_get(ptr->quality_segment_url_modifiers, 0));
		gf_list_rem(ptr->quality_segment_url_modifiers, 0);
	}
	gf_list_del(ptr->quality_segment_url_modifiers);

	while (gf_list_count(ptr->fragment_run_entry_table)) {
		gf_free(gf_list_get(ptr->fragment_run_entry_table, 0));
		gf_list_rem(ptr->fragment_run_entry_table, 0);
	}
	gf_list_del(ptr->fragment_run_entry_table);

	gf_free(ptr);
}

GF_Err afrt_box_read(GF_Box *s, GF_BitStream *bs)
{
	unsigned int i;
	GF_AdobeFragmentRunTableBox *ptr = (GF_AdobeFragmentRunTableBox *)s;

	ptr->timescale = gf_bs_read_u32(bs);

	ptr->quality_entry_count = gf_bs_read_u8(bs);
	for (i=0; i<ptr->quality_entry_count; i++) {
		int j=0;
		u32 tmp_strsize=(u32)ptr->size-8;
		char *tmp_str = (char*) gf_malloc(tmp_strsize);
		while (tmp_strsize) {
			tmp_str[j] = gf_bs_read_u8(bs);
			tmp_strsize--;
			if (!tmp_str[j])
				break;
			j++;
		}
		gf_list_insert(ptr->quality_segment_url_modifiers, tmp_str, i);
	}

	ptr->fragment_run_entry_count = gf_bs_read_u32(bs);
	for (i=0; i<ptr->fragment_run_entry_count; i++) {
		GF_AdobeFragmentRunEntry *fre = gf_malloc(sizeof(GF_AdobeFragmentRunEntry));
		fre->first_fragment = gf_bs_read_u32(bs);
		fre->first_fragment_timestamp = gf_bs_read_u64(bs);
		fre->fragment_duration = gf_bs_read_u32(bs);
		if (!fre->fragment_duration)
			fre->discontinuity_indicator = gf_bs_read_u8(bs);
		gf_list_insert(ptr->fragment_run_entry_table, fre, i);
	}

	return GF_OK;
}

GF_Box *afrt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeFragmentRunTableBox, GF_ISOM_BOX_TYPE_AFRT);
	tmp->quality_segment_url_modifiers = gf_list_new();
	tmp->fragment_run_entry_table = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err afrt_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	unsigned int i;
	GF_AdobeFragmentRunTableBox *ptr = (GF_AdobeFragmentRunTableBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->timescale);
	gf_bs_write_u8(bs, ptr->quality_entry_count);
	for (i=0; i<ptr->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(ptr->quality_segment_url_modifiers, i);
		gf_bs_write_data(bs, str, (u32)strlen(str) + 1);
	}

	gf_bs_write_u32(bs, ptr->fragment_run_entry_count);
	for (i=0; i<ptr->fragment_run_entry_count; i++) {
		GF_AdobeFragmentRunEntry *fre = (GF_AdobeFragmentRunEntry *)gf_list_get(ptr->fragment_run_entry_table, i);
		gf_bs_write_u32(bs, fre->first_fragment);
		gf_bs_write_u64(bs, fre->first_fragment_timestamp);
		gf_bs_write_u32(bs, fre->fragment_duration);
		if (!fre->fragment_duration)
			gf_bs_write_u8(bs, fre->discontinuity_indicator);
	}

	return GF_OK;
}


GF_Err afrt_box_size(GF_Box *s)
{
	u32 i;
	GF_AdobeFragmentRunTableBox *ptr = (GF_AdobeFragmentRunTableBox *)s;

	s->size += 5;

	for (i=0; i<ptr->quality_entry_count; i++)
		s->size += strlen(gf_list_get(ptr->quality_segment_url_modifiers, i)) + 1;

	s->size += 4;

	for (i=0; i<ptr->fragment_run_entry_count; i++) {
		GF_AdobeFragmentRunEntry *fre = (GF_AdobeFragmentRunEntry *)gf_list_get(ptr->fragment_run_entry_table, i);
		if (fre->fragment_duration)
			s->size += 16;
		else
			s->size += 17;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

#endif /*GPAC_DISABLE_ISOM*/

#endif /*GPAC_DISABLE_ISOM_ADOBE*/
