/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
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

#ifndef GPAC_DISABLE_ISOM

/* ProtectionInfo Box */
GF_Box *sinf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ProtectionInfoBox, GF_ISOM_BOX_TYPE_SINF);
	return (GF_Box *)tmp;
}

void sinf_del(GF_Box *s)
{
	GF_ProtectionInfoBox *ptr = (GF_ProtectionInfoBox *)s;
	if (ptr == NULL) return;
	if (ptr->original_format) gf_isom_box_del((GF_Box *)ptr->original_format);
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	if (ptr->scheme_type) gf_isom_box_del((GF_Box *)ptr->scheme_type);
	gf_free(ptr);
}

GF_Err sinf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_ProtectionInfoBox *ptr = (GF_ProtectionInfoBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_FRMA: 
		if (ptr->original_format) return GF_ISOM_INVALID_FILE;
		ptr->original_format = (GF_OriginalFormatBox*)a; 
		break;
	case GF_ISOM_BOX_TYPE_SCHM: 
		if (ptr->scheme_type) return GF_ISOM_INVALID_FILE;
		ptr->scheme_type = (GF_SchemeTypeBox*)a; 
		break;
	case GF_ISOM_BOX_TYPE_SCHI: 
		if (ptr->info) return GF_ISOM_INVALID_FILE;
		ptr->info = (GF_SchemeInformationBox*)a; 
		break;
	default: 
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err sinf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, sinf_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sinf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ProtectionInfoBox *ptr = (GF_ProtectionInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//frma
	e = gf_isom_box_write((GF_Box *) ptr->original_format, bs);
	if (e) return e;
	// schm
	e = gf_isom_box_write((GF_Box *) ptr->scheme_type, bs);
	if (e) return e;
	// schi
	e = gf_isom_box_write((GF_Box *) ptr->info, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err sinf_Size(GF_Box *s)
{
	GF_Err e;
	GF_ProtectionInfoBox *ptr = (GF_ProtectionInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	e = gf_isom_box_size((GF_Box *) ptr->original_format);
	if (e) return e;
	ptr->size += ptr->original_format->size;
	e = gf_isom_box_size((GF_Box *) ptr->scheme_type);
	if (e) return e;
	ptr->size += ptr->scheme_type->size;
	e = gf_isom_box_size((GF_Box *) ptr->info);
	if (e) return e;
	ptr->size += ptr->info->size;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* OriginalFormat Box */
GF_Box *frma_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OriginalFormatBox, GF_ISOM_BOX_TYPE_FRMA);
	return (GF_Box *)tmp;
}

void frma_del(GF_Box *s)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err frma_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	ptr->data_format = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err frma_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->data_format);
	return GF_OK;
}

GF_Err frma_Size(GF_Box *s)
{
	GF_Err e;
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* SchemeType Box */
GF_Box *schm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SchemeTypeBox, GF_ISOM_BOX_TYPE_SCHM);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void schm_del(GF_Box *s)
{
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *)s;
	if (ptr == NULL) return;
	if (ptr->URI) gf_free(ptr->URI);
	gf_free(ptr);
}

GF_Err schm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->scheme_type = gf_bs_read_u32(bs);
	ptr->scheme_version = gf_bs_read_u32(bs);
	ptr->size -= 8;
	if (ptr->size && (ptr->flags & 0x000001)) {
		u32 len = (u32) (ptr->size);
		ptr->URI = (char*)gf_malloc(sizeof(char)*len);
		if (!ptr->URI) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->URI, len);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err schm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	assert(e == GF_OK);
	gf_bs_write_u32(bs, ptr->scheme_type);
	gf_bs_write_u32(bs, ptr->scheme_version);
	if (ptr->flags & 0x000001) gf_bs_write_data(bs, ptr->URI, (u32) strlen(ptr->URI)+1);
	return GF_OK;
}

GF_Err schm_Size(GF_Box *s)
{
	GF_Err e;
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 8;
	if (ptr->flags & 0x000001) ptr->size += strlen(ptr->URI)+1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* SchemeInformation Box */
GF_Box *schi_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SchemeInformationBox, GF_ISOM_BOX_TYPE_SCHI);
	return (GF_Box *)tmp;
}

void schi_del(GF_Box *s)
{
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;
	if (ptr == NULL) return;
	if (ptr->ikms) gf_isom_box_del((GF_Box *)ptr->ikms);
	if (ptr->isfm) gf_isom_box_del((GF_Box *)ptr->isfm);
	if (ptr->okms) gf_isom_box_del((GF_Box *)ptr->okms);
	if (ptr->tenc) gf_isom_box_del((GF_Box *)ptr->tenc);
	if (ptr->piff_tenc) gf_isom_box_del((GF_Box *)ptr->piff_tenc);
	gf_free(ptr);
}

GF_Err schi_AddBox(GF_Box *s, GF_Box *a)
{
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IKMS:
		if (ptr->ikms) return GF_ISOM_INVALID_FILE;
		ptr->ikms = (GF_ISMAKMSBox*)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ISFM:
		if (ptr->isfm) return GF_ISOM_INVALID_FILE;
		ptr->isfm = (GF_ISMASampleFormatBox*)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ODKM:
		if (ptr->okms) return GF_ISOM_INVALID_FILE;
		ptr->okms = (GF_OMADRMKMSBox*)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TENC:
		if (ptr->tenc) return GF_ISOM_INVALID_FILE;
		ptr->tenc = (GF_TrackEncryptionBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UUID:
		if (((GF_UUIDBox*)a)->internal_4cc==GF_ISOM_BOX_UUID_TENC) {
			if (ptr->piff_tenc) return GF_ISOM_INVALID_FILE;
			ptr->piff_tenc = (GF_PIFFTrackEncryptionBox *)a;
			return GF_OK;
		} else {
			return gf_isom_box_add_default(s, a);
		}
	default:
		return gf_isom_box_add_default(s, a);
	}
}

GF_Err schi_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, schi_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err schi_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->ikms) {
		e = gf_isom_box_write((GF_Box *) ptr->ikms, bs);
		if (e) return e;
	}
	if (ptr->isfm) {
		e = gf_isom_box_write((GF_Box *) ptr->isfm, bs);
		if (e) return e;
	}
	if (ptr->okms) {
		e = gf_isom_box_write((GF_Box *) ptr->okms, bs);
		if (e) return e;
	}
	if (ptr->tenc) {
		e = gf_isom_box_write((GF_Box *) ptr->tenc, bs);
		if (e) return e;
	}
	if (ptr->piff_tenc) {
		e = gf_isom_box_write((GF_Box *) ptr->piff_tenc, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err schi_Size(GF_Box *s)
{
	GF_Err e;
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	if (ptr->ikms) {
		e = gf_isom_box_size((GF_Box *) ptr->ikms);
		if (e) return e;
		ptr->size += ptr->ikms->size;
	}
	if (ptr->isfm) {
		e = gf_isom_box_size((GF_Box *) ptr->isfm);
		if (e) return e;
		ptr->size += ptr->isfm->size;
	}
	if (ptr->okms) {
		e = gf_isom_box_size((GF_Box *) ptr->okms);
		if (e) return e;
		ptr->size += ptr->okms->size;
	}
	if (ptr->tenc) {
		e = gf_isom_box_size((GF_Box *) ptr->tenc);
		if (e) return e;
		ptr->size += ptr->tenc->size;
	}
	if (ptr->piff_tenc) {
		e = gf_isom_box_size((GF_Box *) ptr->tenc);
		if (e) return e;
		ptr->size += ptr->tenc->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* ISMAKMS Box */
GF_Box *iKMS_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ISMAKMSBox, GF_ISOM_BOX_TYPE_IKMS);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void iKMS_del(GF_Box *s)
{
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (ptr == NULL) return;
	if (ptr->URI) gf_free(ptr->URI);
	gf_free(ptr);
}

GF_Err iKMS_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 len;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	len = (u32) (ptr->size);
	ptr->URI = (char*) gf_malloc(sizeof(char)*len);
	if (!ptr->URI) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->URI, len);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iKMS_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->URI, (u32) strlen(ptr->URI)+1);
	return GF_OK;
}

GF_Err iKMS_Size(GF_Box *s)
{
	GF_Err e;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += strlen(ptr->URI)+1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* ISMASampleFormat Box */
GF_Box *iSFM_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ISMASampleFormatBox, GF_ISOM_BOX_TYPE_ISFM);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void iSFM_del(GF_Box *s)
{
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err iSFM_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->selective_encryption = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	ptr->key_indicator_length = gf_bs_read_u8(bs);
	ptr->IV_length = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iSFM_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->selective_encryption, 1);
	gf_bs_write_int(bs, 0, 7);
	gf_bs_write_u8(bs, ptr->key_indicator_length);
	gf_bs_write_u8(bs, ptr->IV_length);
	return GF_OK;
}

GF_Err iSFM_Size(GF_Box *s)
{
	GF_Err e;
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 3;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



/* OMADRMCommonHeader Box */
GF_Box *ohdr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMCommonHeaderBox, GF_ISOM_BOX_TYPE_OHDR);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void ohdr_del(GF_Box *s)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox*)s;
	if (ptr == NULL) return;
	if (ptr->ContentID) gf_free(ptr->ContentID);
	if (ptr->RightsIssuerURL) gf_free(ptr->RightsIssuerURL);
	if (ptr->TextualHeaders) gf_free(ptr->TextualHeaders);
	gf_free(ptr);
}

GF_Err ohdr_AddBox(GF_Box *s, GF_Box *a)
{
	return gf_isom_box_add_default(s, a);
}

GF_Err ohdr_Read(GF_Box *s, GF_BitStream *bs)
{
	u16 cid_len, ri_len;
	GF_Err e;
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox*)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->EncryptionMethod = gf_bs_read_u8(bs);
	ptr->PaddingScheme = gf_bs_read_u8(bs);
	ptr->PlaintextLength = gf_bs_read_u64(bs);
	cid_len = gf_bs_read_u16(bs);
	ri_len = gf_bs_read_u16(bs);
	ptr->TextualHeadersLen = gf_bs_read_u16(bs);
	ptr->size -= 1+1+8+2+2+2;
	if (ptr->size<cid_len+ri_len+ptr->TextualHeadersLen) return GF_ISOM_INVALID_FILE;

	if (cid_len) {
		ptr->ContentID = (char *)gf_malloc(sizeof(char)*(cid_len+1));
		gf_bs_read_data(bs, ptr->ContentID, cid_len);
		ptr->ContentID[cid_len]=0;
	}

	if (ri_len) {
		ptr->RightsIssuerURL = (char *)gf_malloc(sizeof(char)*(ri_len+1));
		gf_bs_read_data(bs, ptr->RightsIssuerURL, ri_len);
		ptr->RightsIssuerURL[ri_len]=0;
	}
	
	if (ptr->TextualHeadersLen) {
		ptr->TextualHeaders = (char *)gf_malloc(sizeof(char)*(ptr->TextualHeadersLen+1));
		gf_bs_read_data(bs, ptr->TextualHeaders, ptr->TextualHeadersLen);
		ptr->TextualHeaders[ptr->TextualHeadersLen] = 0;
	}

	ptr->size -= cid_len+ri_len+ptr->TextualHeadersLen;

	return gf_isom_read_box_list(s, bs, ohdr_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ohdr_Write(GF_Box *s, GF_BitStream *bs)
{
	u16 cid_len, ri_len;
	GF_Err e;
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->EncryptionMethod);
	gf_bs_write_u8(bs, ptr->PaddingScheme);
	gf_bs_write_u64(bs, ptr->PlaintextLength);
	
	cid_len = ptr->ContentID ? (u32) strlen(ptr->ContentID) : 0;
	gf_bs_write_u16(bs, cid_len);
	ri_len = ptr->RightsIssuerURL ? (u32) strlen(ptr->RightsIssuerURL) : 0;
	gf_bs_write_u16(bs, ri_len);
	gf_bs_write_u16(bs, ptr->TextualHeadersLen);

	if (cid_len) gf_bs_write_data(bs, ptr->ContentID, (u32) strlen(ptr->ContentID));
	if (ri_len) gf_bs_write_data(bs, ptr->RightsIssuerURL, (u32) strlen(ptr->RightsIssuerURL));
	if (ptr->TextualHeadersLen) gf_bs_write_data(bs, ptr->TextualHeaders, ptr->TextualHeadersLen);
	ptr->size -= cid_len+ri_len+ptr->TextualHeadersLen;
	return GF_OK;
}

GF_Err ohdr_Size(GF_Box *s)
{
	GF_Err e;
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 1+1+8+2+2+2;
	if (ptr->ContentID) ptr->size += strlen(ptr->ContentID);
	if (ptr->RightsIssuerURL) ptr->size += strlen(ptr->RightsIssuerURL);
	if (ptr->TextualHeadersLen) ptr->size += ptr->TextualHeadersLen;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* OMADRMGroupID Box */
GF_Box *grpi_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMGroupIDBox, GF_ISOM_BOX_TYPE_GRPI);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void grpi_del(GF_Box *s)
{
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	if (ptr == NULL) return;
	if (ptr->GroupID) gf_free(ptr->GroupID);
	if (ptr->GroupKey) gf_free(ptr->GroupKey);
	gf_free(ptr);
}

GF_Err grpi_Read(GF_Box *s, GF_BitStream *bs)
{
	u16 gid_len;
	GF_Err e;
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox*)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	gid_len = gf_bs_read_u16(bs);
	ptr->GKEncryptionMethod = gf_bs_read_u8(bs);
	ptr->GKLength = gf_bs_read_u16(bs);

	ptr->size -= 1+2+2;
	if (ptr->size<gid_len+ptr->GKLength) return GF_ISOM_INVALID_FILE;

	ptr->GroupID = gf_malloc(sizeof(char)*(gid_len+1));
	gf_bs_read_data(bs, ptr->GroupID, gid_len);
	ptr->GroupID[gid_len]=0;
	
	ptr->GroupKey = (char *)gf_malloc(sizeof(char)*ptr->GKLength);
	gf_bs_read_data(bs, ptr->GroupKey, ptr->GKLength);
	ptr->size -= gid_len+ptr->GKLength;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err grpi_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u16 gid_len;
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gid_len = ptr->GroupID ? (u32) strlen(ptr->GroupID) : 0;
	gf_bs_write_u16(bs, gid_len);
	gf_bs_write_u8(bs, ptr->GKEncryptionMethod);
	gf_bs_write_u16(bs, ptr->GKLength);
	gf_bs_write_data(bs, ptr->GroupID, gid_len);
	gf_bs_write_data(bs, ptr->GroupKey, ptr->GKLength);
	return GF_OK;
}

GF_Err grpi_Size(GF_Box *s)
{
	GF_Err e;
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 2+2+1 + ptr->GKLength;
	if (ptr->GroupID) ptr->size += strlen(ptr->GroupID);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




/* OMADRMMutableInformation Box */
GF_Box *mdri_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMMutableInformationBox, GF_ISOM_BOX_TYPE_MDRI);
	return (GF_Box *)tmp;
}

void mdri_del(GF_Box *s)
{
	GF_OMADRMMutableInformationBox*ptr = (GF_OMADRMMutableInformationBox*)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err mdri_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, gf_isom_box_add_default);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err mdri_Write(GF_Box *s, GF_BitStream *bs)
{
//	GF_OMADRMMutableInformationBox*ptr = (GF_OMADRMMutableInformationBox*)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err mdri_Size(GF_Box *s)
{
	GF_Err e;
//	GF_OMADRMMutableInformationBox *ptr = (GF_OMADRMMutableInformationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* OMADRMTransactionTracking Box */
GF_Box *odtt_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMTransactionTrackingBox, GF_ISOM_BOX_TYPE_ODTT);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void odtt_del(GF_Box *s)
{
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox*)s;
	gf_free(ptr);
}

GF_Err odtt_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	gf_bs_read_data(bs, ptr->TransactionID, 16);
	ptr->size -= 16;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err odtt_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox*)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->TransactionID, 16);
	return GF_OK;
}

GF_Err odtt_Size(GF_Box *s)
{
	GF_Err e;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	s->size += 16;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



/* OMADRMRightsObject Box */
GF_Box *odrb_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMRightsObjectBox, GF_ISOM_BOX_TYPE_ODRB);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void odrb_del(GF_Box *s)
{
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox*)s;
	if (ptr->oma_ro) gf_free(ptr->oma_ro);
	gf_free(ptr);
}

GF_Err odrb_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->oma_ro_size = (u32) ptr->size;
	ptr->oma_ro = (char*) gf_malloc(sizeof(char)*ptr->oma_ro_size);
	gf_bs_read_data(bs, ptr->oma_ro, ptr->oma_ro_size);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err odrb_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->oma_ro, ptr->oma_ro_size);
	return GF_OK;
}

GF_Err odrb_Size(GF_Box *s)
{
	GF_Err e;
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	s->size += ptr->oma_ro_size;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




/* OMADRMKMS Box */
GF_Box *odkm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMKMSBox, GF_ISOM_BOX_TYPE_ODKM);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void odkm_del(GF_Box *s)
{
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	if (ptr->hdr) gf_isom_box_del((GF_Box*)ptr->hdr);
	if (ptr->fmt) gf_isom_box_del((GF_Box*)ptr->fmt);
	gf_free(ptr);
}

GF_Err odkm_Add(GF_Box *s, GF_Box *a)
{
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_OHDR:
		if (ptr->hdr) gf_isom_box_del((GF_Box*)ptr->hdr);
		ptr->hdr = (GF_OMADRMCommonHeaderBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ODAF:
		if (ptr->fmt) gf_isom_box_del((GF_Box*)ptr->fmt);
		ptr->fmt = (GF_OMADRMAUFormatBox*)a;
		return GF_OK;
	default:
		gf_isom_box_del(a);
		return GF_OK;
	}
}

GF_Err odkm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	if (s == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return gf_isom_read_box_list(s, bs, odkm_Add);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err odkm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->hdr) {
		e = gf_isom_box_write((GF_Box*)ptr->hdr, bs);
		if (e) return e;
	}
	if (ptr->fmt) {
		e = gf_isom_box_write((GF_Box*)ptr->fmt, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err odkm_Size(GF_Box *s)
{
	GF_Err e;
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (ptr->hdr) {
		e = gf_isom_box_size((GF_Box*)ptr->hdr);
		if (e) return e;
		ptr->size += ptr->hdr->size;
	}
	if (ptr->fmt) {
		e = gf_isom_box_size((GF_Box*)ptr->fmt);
		if (e) return e;
		ptr->size += ptr->fmt->size;
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *pssh_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ProtectionSystemHeaderBox, GF_ISOM_BOX_TYPE_PSSH);
	return (GF_Box *)tmp;
}

void pssh_del(GF_Box *s)
{
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox*)s;
	if (ptr == NULL) return;
	if (ptr->private_data) gf_free(ptr->private_data);
	if (ptr->KIDs) gf_free(ptr->KIDs);
	gf_free(ptr);
}

GF_Err pssh_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	gf_bs_read_data(bs, (char *) ptr->SystemID, 16);
	ptr->size -= 16;
	if (ptr->version > 0) {
		ptr->KID_count = gf_bs_read_u32(bs);
		ptr->size -= 4;
		if (ptr->KID_count) {
			u32 i;
			ptr->KIDs = gf_malloc(ptr->KID_count*sizeof(bin128));
			for (i=0; i<ptr->KID_count; i++) {
				gf_bs_read_data(bs, (char *) ptr->KIDs[i], 16);
				ptr->size -= 16;
			}
		}
	}
	ptr->private_data_size = gf_bs_read_u32(bs);
	ptr->size -= 4;
	if (ptr->private_data_size) {
		ptr->private_data = gf_malloc(sizeof(char)*ptr->private_data_size);
		gf_bs_read_data(bs, (char *) ptr->private_data, ptr->private_data_size);
		ptr->size -= ptr->private_data_size;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pssh_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, (char *) ptr->SystemID, 16);
	if (ptr->version > 0) {
		u32 i;
		gf_bs_write_u32(bs, ptr->KID_count);
		for (i=0; i<ptr->KID_count; i++) 
			gf_bs_write_data(bs, (char *) ptr->KIDs[i], 16);
	}
	if (ptr->private_data) {
		gf_bs_write_u32(bs, ptr->private_data_size);
		gf_bs_write_data(bs, (char *) ptr->private_data, ptr->private_data_size);
	} else 
		gf_bs_write_u32(bs, 0);
	return GF_OK;
}

GF_Err pssh_Size(GF_Box *s)
{
	GF_Err e;
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox*)s;

	if (ptr->KID_count && !ptr->version) {
		ptr->version = 1;
	}

	e = gf_isom_full_box_get_size(s);
	if (e) return e;

	ptr->size += 16;
	if (ptr->version) ptr->size += 4 + 16*ptr->KID_count;
	ptr->size += 4 + (ptr->private_data ? ptr->private_data_size : 0);
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


GF_Box *tenc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackEncryptionBox, GF_ISOM_BOX_TYPE_TENC);
	return (GF_Box *)tmp;
}

void tenc_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err tenc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->IsEncrypted = gf_bs_read_int(bs, 24);
	ptr->IV_size = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, (char *) ptr->KID, 16);
	ptr->size -= 20;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tenc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->IsEncrypted, 24);
	gf_bs_write_u8(bs, ptr->IV_size);
	gf_bs_write_data(bs, (char *) ptr->KID, 16);
	return GF_OK;
}

GF_Err tenc_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 20;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *piff_tenc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PIFFTrackEncryptionBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_TENC;
	return (GF_Box *)tmp;
}

void piff_tenc_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err piff_tenc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox*)s;

	if (ptr->size<4) return GF_ISOM_INVALID_FILE;
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ptr->size -= 4;

	ptr->AlgorithmID = gf_bs_read_int(bs, 24);
	ptr->IV_size = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, (char *) ptr->KID, 16);
	ptr->size -= 20;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err piff_tenc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	gf_bs_write_int(bs, ptr->AlgorithmID, 24);
	gf_bs_write_u8(bs, ptr->IV_size);
	gf_bs_write_data(bs, (char *) ptr->KID, 16);
	return GF_OK;
}

GF_Err piff_tenc_Size(GF_Box *s)
{
	GF_Err e;
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 24;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


GF_Box *piff_psec_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PIFFSampleEncryptionBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_PSEC;
	return (GF_Box *)tmp;
}

void piff_psec_del(GF_Box *s)
{
	GF_PIFFSampleEncryptionBox *ptr = (GF_PIFFSampleEncryptionBox *)s;
	while (gf_list_count(ptr->samp_aux_info)) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, 0);
		if (sai) {
			gf_isom_cenc_samp_aux_info_del(sai);
			gf_free(sai);
			sai = NULL;
		}
		gf_list_rem(ptr->samp_aux_info, 0);
	}
	if (ptr->samp_aux_info) gf_list_del(ptr->samp_aux_info);
	gf_free(s);
}

GF_Err piff_psec_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PIFFSampleEncryptionBox *ptr = (GF_PIFFSampleEncryptionBox *)s;
	if (ptr->size<4) return GF_ISOM_INVALID_FILE;
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ptr->size -= 4;

	if (ptr->flags & 1) {
		ptr->AlgorithmID = gf_bs_read_int(bs, 24);
		ptr->IV_size = gf_bs_read_u8(bs);
		gf_bs_read_data(bs, (char *) ptr->KID, 16);
		ptr->size -= 20;
	}
	ptr->sample_count = gf_bs_read_u32(bs);
	ptr->size -= 4;
	if (ptr->sample_count) {
		u32 i, j;

		if (!ptr->samp_aux_info) ptr->samp_aux_info = gf_list_new();
		for (i = 0; i < ptr->sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
			memset(sai, 0, sizeof(GF_CENCSampleAuxInfo));
			gf_bs_read_data(bs, (char *)sai->IV, 16);
			sai->subsample_count = gf_bs_read_u16(bs);
			sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
			for (j = 0; j < sai->subsample_count; j++) {
				sai->subsamples[j].bytes_clear_data = gf_bs_read_u32(bs);
				sai->subsamples[j].bytes_encrypted_data = gf_bs_read_u32(bs);
			}
			gf_list_add(ptr->samp_aux_info, sai);
		}
	}
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err piff_psec_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PIFFSampleEncryptionBox *ptr = (GF_PIFFSampleEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	if (ptr->flags & 1) {
		gf_bs_write_int(bs, ptr->AlgorithmID, 24);
		gf_bs_write_u8(bs, ptr->IV_size);
		gf_bs_write_data(bs, (char *) ptr->KID, 16);
	}
	gf_bs_write_u32(bs, ptr->sample_count);
	if (ptr->sample_count) {
		u32 i, j;
		assert(ptr->sample_count == gf_list_count(ptr->samp_aux_info));
		for (i = 0; i < ptr->sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			gf_bs_write_data(bs, (char *)sai->IV, 16);
			gf_bs_write_u16(bs, sai->subsample_count);
			for (j = 0; j < sai->subsample_count; j++) {
				gf_bs_write_u32(bs, sai->subsamples[j].bytes_clear_data);
				gf_bs_write_u32(bs, sai->subsamples[j].bytes_encrypted_data);
			}
		}
	}
	return GF_OK;
}

GF_Err piff_psec_Size(GF_Box *s)
{
	u32 i;
	GF_Err e;
	GF_PIFFSampleEncryptionBox *ptr = (GF_PIFFSampleEncryptionBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	if (ptr->flags & 1) {
		ptr->size += 20;
	}
	ptr->size += 4;
	if (ptr->sample_count) {
		assert(ptr->sample_count == gf_list_count(ptr->samp_aux_info));
		for (i = 0; i < ptr->sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			ptr->size += 18 + 8*sai->subsample_count;
		}
	}
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


GF_Box *piff_pssh_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PIFFProtectionSystemHeaderBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_PSSH;
	return (GF_Box *)tmp;
}

void piff_pssh_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err piff_pssh_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*)s;

	if (ptr->size<4) return GF_ISOM_INVALID_FILE;
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ptr->size -= 4;

	gf_bs_read_data(bs, (char *) ptr->SystemID, 16);
	ptr->private_data_size = gf_bs_read_u32(bs);
	ptr->size -= 20;
	ptr->private_data = gf_malloc(sizeof(char)*ptr->private_data_size);
	gf_bs_read_data(bs, (char *) ptr->private_data, ptr->private_data_size);
	ptr->size -= ptr->private_data_size;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err piff_pssh_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	gf_bs_write_data(bs, (char *) ptr->SystemID, 16);
	gf_bs_write_u32(bs, ptr->private_data_size);
	gf_bs_write_data(bs, (char *) ptr->private_data, ptr->private_data_size);
	return GF_OK;
}

GF_Err piff_pssh_Size(GF_Box *s)
{
	GF_Err e;
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 24 + ptr->private_data_size;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *senc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleEncryptionBox, GF_ISOM_BOX_TYPE_SENC);
	return (GF_Box *)tmp;
}

void senc_del(GF_Box *s)
{
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *)s;
	while (gf_list_count(ptr->samp_aux_info)) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, 0);
		if (sai) {
			gf_isom_cenc_samp_aux_info_del(sai);
			gf_free(sai);
			sai = NULL;
		}
		gf_list_rem(ptr->samp_aux_info, 0);
	}
	if (ptr->samp_aux_info) gf_list_del(ptr->samp_aux_info);
	gf_free(s);
}

GF_Err senc_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 sample_count;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *)s;
	if (ptr->size<4) return GF_ISOM_INVALID_FILE;
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ptr->size -= 4;

	sample_count = gf_bs_read_u32(bs);
	ptr->size -= 4;
	if (sample_count) {
		u32 i, j;

		if (!ptr->samp_aux_info) ptr->samp_aux_info = gf_list_new();
		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
			memset(sai, 0, sizeof(GF_CENCSampleAuxInfo));
			gf_bs_read_data(bs, (char *)sai->IV, 16);
			sai->subsample_count = gf_bs_read_u16(bs);
			sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
			for (j = 0; j < sai->subsample_count; j++) {
				sai->subsamples[j].bytes_clear_data = gf_bs_read_u32(bs);
				sai->subsamples[j].bytes_encrypted_data = gf_bs_read_u32(bs);
			}
			gf_list_add(ptr->samp_aux_info, sai);
		}
	}
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err senc_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 sample_count;
	GF_Err e;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	sample_count = gf_list_count(ptr->samp_aux_info);
	gf_bs_write_u32(bs, sample_count);
	if (sample_count) {
		u32 i, j;
		assert(sample_count == gf_list_count(ptr->samp_aux_info));
		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			gf_bs_write_data(bs, (char *)sai->IV, 16);
			gf_bs_write_u16(bs, sai->subsample_count);
			for (j = 0; j < sai->subsample_count; j++) {
				gf_bs_write_u32(bs, sai->subsamples[j].bytes_clear_data);
				gf_bs_write_u32(bs, sai->subsamples[j].bytes_encrypted_data);
			}
		}
	}
	return GF_OK;
}

GF_Err senc_Size(GF_Box *s)
{
	u32 sample_count;
	u32 i;
	GF_Err e;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 8;
	sample_count = gf_list_count(ptr->samp_aux_info);
	if (sample_count) {
		assert(sample_count == gf_list_count(ptr->samp_aux_info));
		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			ptr->size += 18 + 8*sai->subsample_count;
		}
	}
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

#endif /*GPAC_DISABLE_ISOM*/
