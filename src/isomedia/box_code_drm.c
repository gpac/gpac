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
	if (e) return e;
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
	if (ptr->adkm) gf_isom_box_del((GF_Box *)ptr->adkm);
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
	case GF_ISOM_BOX_TYPE_ADKM:
		if (ptr->adkm) return GF_ISOM_INVALID_FILE;
		ptr->adkm = (GF_AdobeDRMKeyManagementSystemBox *)a;
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
	if (ptr->adkm) {
		e = gf_isom_box_write((GF_Box *) ptr->adkm, bs);
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
	if (ptr->adkm) {
		e = gf_isom_box_size((GF_Box *) ptr->adkm);
		if (e) return e;
		ptr->size += ptr->adkm->size;
	}
	if (ptr->piff_tenc) {
		e = gf_isom_box_size((GF_Box *) ptr->piff_tenc);
		if (e) return e;
		ptr->size += ptr->piff_tenc->size;
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

	cid_len = ptr->ContentID ? (u16) strlen(ptr->ContentID) : 0;
	gf_bs_write_u16(bs, cid_len);
	ri_len = ptr->RightsIssuerURL ? (u16) strlen(ptr->RightsIssuerURL) : 0;
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
	gid_len = ptr->GroupID ? (u16) strlen(ptr->GroupID) : 0;
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

	gf_bs_read_u8(bs); //reserved
	if (!ptr->version) {
		gf_bs_read_u8(bs); //reserved
	} else {
		ptr->crypt_byte_block = gf_bs_read_int(bs, 4);
		ptr->skip_byte_block = gf_bs_read_int(bs, 4);
	}
	ptr->isProtected = gf_bs_read_u8(bs);
	ptr->Per_Sample_IV_Size = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, (char *) ptr->KID, 16);
	ptr->size -= 20;
	if ((ptr->isProtected == 1) && !ptr->Per_Sample_IV_Size) {
		ptr->constant_IV_size = gf_bs_read_u8(bs);
		gf_bs_read_data(bs, (char *) ptr->constant_IV, ptr->constant_IV_size);
		ptr->size -= 1 + ptr->constant_IV_size;
	}
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

	gf_bs_write_u8(bs, 0x0); //reserved
	if (!ptr->version) {
		gf_bs_write_u8(bs, 0x0); //reserved
	} else {
		gf_bs_write_int(bs, ptr->crypt_byte_block, 4);
		gf_bs_write_int(bs, ptr->skip_byte_block, 4);
	}
	gf_bs_write_u8(bs, ptr->isProtected);
	gf_bs_write_u8(bs, ptr->Per_Sample_IV_Size);
	gf_bs_write_data(bs, (char *) ptr->KID, 16);
	if ((ptr->isProtected == 1) && !ptr->Per_Sample_IV_Size) {
		gf_bs_write_u8(bs, ptr->constant_IV_size);
		gf_bs_write_data(bs,(char *) ptr->constant_IV, ptr->constant_IV_size);
	}
	return GF_OK;
}

GF_Err tenc_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 20;
	if ((ptr->isProtected == 1) && !ptr->Per_Sample_IV_Size) {
		ptr->size += 1 + ptr->constant_IV_size;
	}
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
		if (sai) gf_isom_cenc_samp_aux_info_del(sai);
		gf_list_rem(ptr->samp_aux_info, 0);
	}
	if (ptr->samp_aux_info) gf_list_del(ptr->samp_aux_info);
	gf_free(s);
}


GF_Err piff_psec_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 sample_count, i, j;
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
	if (ptr->IV_size == 0)
		ptr->IV_size = 8; //default to 8

	sample_count = gf_bs_read_u32(bs);
	ptr->size -= 4;
	if (ptr->IV_size != 8 && ptr->IV_size != 16) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] PIFF PSEC box incorrect IV size: %u - shall be 8 or 16\n", ptr->IV_size));
		return GF_BAD_PARAM;
	}

	ptr->samp_aux_info = gf_list_new();
	for (i=0; i<sample_count; ++i) {
		GF_CENCSampleAuxInfo *sai;
		GF_SAFEALLOC(sai, GF_CENCSampleAuxInfo);
		if (!sai) return GF_OUT_OF_MEM;

		sai->IV_size = ptr->IV_size;
		gf_bs_read_data(bs, (char *) sai->IV, ptr->IV_size);
		ptr->size -= ptr->IV_size;
		if (ptr->flags & 2) {
			sai->subsample_count = gf_bs_read_u16(bs);
			sai->subsamples = gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
			for (j = 0; j < sai->subsample_count; ++j) {
				sai->subsamples[j].bytes_clear_data = gf_bs_read_u16(bs);
				sai->subsamples[j].bytes_encrypted_data = gf_bs_read_u32(bs);
			}
			ptr->size -= 2+sai->subsample_count*6;
		}
		gf_list_add(ptr->samp_aux_info, sai);
	}

	ptr->bs_offset = gf_bs_get_position(bs);
	assert(ptr->size == 0);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err store_senc_info(GF_SampleEncryptionBox *ptr, GF_BitStream *bs)
{
	GF_Err e;
	u64 pos;
	if (!ptr->cenc_saio) return GF_OK;

	pos = gf_bs_get_position(bs);
	if (pos>0xFFFFFFFFULL) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] \"senc\" offset larger than 32-bits , \"saio\" box version must be 1 .\n"));
	}
	e = gf_bs_seek(bs, ptr->cenc_saio->offset_first_offset_field);
	if (e) return e;
	//force using version 1 for saio box i.e offset has 64 bits
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->traf) {
		gf_bs_write_u64(bs, pos - ptr->traf->moof_start_in_bs );
	} else
#endif
	{
		gf_bs_write_u64(bs, pos);
	}
	return gf_bs_seek(bs, pos);
}

GF_Err piff_psec_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 sample_count;
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
	sample_count = gf_list_count(ptr->samp_aux_info);
	gf_bs_write_u32(bs, sample_count);
	if (sample_count) {
		u32 i, j;
		e = store_senc_info((GF_SampleEncryptionBox *)ptr, bs);
		if (e) return e;

		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			if (! sai->IV_size) continue;
			gf_bs_write_data(bs, (char *)sai->IV, sai->IV_size);
			gf_bs_write_u16(bs, sai->subsample_count);
			for (j = 0; j < sai->subsample_count; j++) {
				gf_bs_write_u16(bs, sai->subsamples[j].bytes_clear_data);
				gf_bs_write_u32(bs, sai->subsamples[j].bytes_encrypted_data);
			}
		}
	}
	return GF_OK;
}

GF_Err piff_psec_Size(GF_Box *s)
{
	u32 i, sample_count;
	GF_Err e;
	GF_PIFFSampleEncryptionBox *ptr = (GF_PIFFSampleEncryptionBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	if (ptr->flags & 1) {
		ptr->size += 20;
	}
	ptr->size += 4;
	sample_count = gf_list_count(ptr->samp_aux_info);
	if (sample_count) {
		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			if (! sai->IV_size) continue;
			ptr->size += 18 + 6*sai->subsample_count;
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
	GF_Err e;
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

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
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox *) s;
	GF_Err e = gf_isom_full_box_write(s, bs);
	if (e) return e;

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
		if (sai) gf_isom_cenc_samp_aux_info_del(sai);
		gf_list_rem(ptr->samp_aux_info, 0);
	}
	if (ptr->samp_aux_info) gf_list_del(ptr->samp_aux_info);
	gf_free(s);
}

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err senc_Parse(GF_BitStream *bs, GF_TrackBox *trak, GF_TrackFragmentBox *traf, GF_SampleEncryptionBox *ptr)
#else
GF_Err senc_Parse(GF_BitStream *bs, GF_TrackBox *trak, void *traf, GF_SampleEncryptionBox *ptr)
#endif
{
	GF_Err e;
	u32 i, j, count;
	u64 pos = gf_bs_get_position(bs);

#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (!traf)
		return GF_BAD_PARAM;
#endif

	gf_bs_seek(bs, ptr->bs_offset);

	count = gf_bs_read_u32(bs);
	if (!ptr->samp_aux_info) ptr->samp_aux_info = gf_list_new();
	for (i=0; i<count; i++) {
		u32 is_encrypted;
		u32 samp_count;
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
		memset(sai, 0, sizeof(GF_CENCSampleAuxInfo));

		samp_count = i+1;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		if (trak) samp_count += trak->sample_count_at_seg_start;
#endif

		e = gf_isom_get_sample_cenc_info_ex(trak, traf, samp_count, &is_encrypted, &sai->IV_size, NULL, NULL, NULL, NULL, NULL);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isobmf] could not get cenc info for sample %d: %s\n", samp_count, gf_error_to_string(e) ));
			return e;
		}
		if (is_encrypted) {
			gf_bs_read_data(bs, (char *)sai->IV, sai->IV_size);
			if (ptr->flags & 0x00000002) {
				sai->subsample_count = gf_bs_read_u16(bs);
				sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
				for (j = 0; j < sai->subsample_count; j++) {
					sai->subsamples[j].bytes_clear_data = gf_bs_read_u16(bs);
					sai->subsamples[j].bytes_encrypted_data = gf_bs_read_u32(bs);
				}
			}
		}
		gf_list_add(ptr->samp_aux_info, sai);
	}
	gf_bs_seek(bs, pos);
	return GF_OK;
}


GF_Err senc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *)s;
	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ptr->size -= 4;

	ptr->bs_offset = gf_bs_get_position(bs);
	gf_bs_skip_bytes(bs, ptr->size);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err senc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 sample_count;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *) s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	sample_count = gf_list_count(ptr->samp_aux_info);
	gf_bs_write_u32(bs, sample_count);
	if (sample_count) {
		u32 i, j;

		e = store_senc_info(ptr, bs);
		if (e) return e;

		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			//for cbcs scheme, IV_size is 0, constant IV shall be used. It is written in tenc box rather than in sai
			if (sai->IV_size)
				gf_bs_write_data(bs, (char *)sai->IV, sai->IV_size);
			if (ptr->flags & 0x00000002) {
				gf_bs_write_u16(bs, sai->subsample_count);
				for (j = 0; j < sai->subsample_count; j++) {
					gf_bs_write_u16(bs, sai->subsamples[j].bytes_clear_data);
					gf_bs_write_u32(bs, sai->subsamples[j].bytes_encrypted_data);
				}
			}
		}
	}
	return GF_OK;
}

GF_Err senc_Size(GF_Box *s)
{
	GF_Err e;
	u32 sample_count;
	u32 i;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox*)s;
	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 4;

	ptr->size += 4;
	sample_count = gf_list_count(ptr->samp_aux_info);
	if (sample_count) {
		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			//if (! sai->IV_size) continue;
			ptr->size += sai->IV_size;
			if (ptr->flags & 0x00000002)
				ptr->size += 2 + 6*sai->subsample_count;
		}
	}
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *adkm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeDRMKeyManagementSystemBox, GF_ISOM_BOX_TYPE_ADKM);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void adkm_del(GF_Box *s)
{
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	if (!ptr) return;
	if (ptr->header) gf_isom_box_del((GF_Box *)ptr->header);
	if (ptr->au_format) gf_isom_box_del((GF_Box *)ptr->au_format);
	gf_free(s);
}

GF_Err adkm_AddBox(GF_Box *s, GF_Box *a)
{
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_AHDR:
		if (ptr->header) return GF_ISOM_INVALID_FILE;
		ptr->header = (GF_AdobeDRMHeaderBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_ADAF:
		if (ptr->au_format) return GF_ISOM_INVALID_FILE;
		ptr->au_format = (GF_AdobeDRMAUFormatBox *)a;
		break;

	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err adkm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return gf_isom_read_box_list(s, bs, adkm_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err adkm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//ahdr
	e = gf_isom_box_write((GF_Box *) ptr->header, bs);
	if (e) return e;
	//adaf
	e = gf_isom_box_write((GF_Box *) ptr->au_format, bs);
	if (e) return e;

	return GF_OK;
}

GF_Err adkm_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	e = gf_isom_box_size((GF_Box *) ptr->header);
	if (e) return e;
	ptr->size += ptr->header->size;
	e = gf_isom_box_size((GF_Box *) ptr->au_format);
	if (e) return e;
	ptr->size += ptr->au_format->size;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *ahdr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeDRMHeaderBox, GF_ISOM_BOX_TYPE_AHDR);
	tmp->version = 2;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void ahdr_del(GF_Box *s)
{
	GF_AdobeDRMHeaderBox *ptr = (GF_AdobeDRMHeaderBox *)s;
	if (!ptr) return;
	if (ptr->std_enc_params) gf_isom_box_del((GF_Box *)ptr->std_enc_params);
	gf_free(s);
}


GF_Err ahdr_AddBox(GF_Box *s, GF_Box *a)
{
	GF_AdobeDRMHeaderBox *ptr = (GF_AdobeDRMHeaderBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_APRM:
		if (ptr->std_enc_params) return GF_ISOM_INVALID_FILE;
		ptr->std_enc_params = (GF_AdobeStdEncryptionParamsBox *)a;
		break;

	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err ahdr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return gf_isom_read_box_list(s, bs, ahdr_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ahdr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeDRMHeaderBox *ptr = (GF_AdobeDRMHeaderBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	e = gf_isom_box_write((GF_Box *) ptr->std_enc_params, bs);
	if (e) return e;

	return GF_OK;
}

GF_Err ahdr_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeDRMHeaderBox *ptr = (GF_AdobeDRMHeaderBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	e = gf_isom_box_size((GF_Box *) ptr->std_enc_params);
	if (e) return e;
	ptr->size += ptr->std_enc_params->size;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *aprm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeStdEncryptionParamsBox, GF_ISOM_BOX_TYPE_APRM);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void aprm_del(GF_Box *s)
{
	GF_AdobeStdEncryptionParamsBox *ptr = (GF_AdobeStdEncryptionParamsBox *)s;
	if (!ptr) return;
	if (ptr->enc_info) gf_isom_box_del((GF_Box *)ptr->enc_info);
	if (ptr->key_info) gf_isom_box_del((GF_Box *)ptr->key_info);
	gf_free(s);
}

GF_Err aprm_AddBox(GF_Box *s, GF_Box *a)
{
	GF_AdobeStdEncryptionParamsBox *ptr = (GF_AdobeStdEncryptionParamsBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_AEIB:
		if (ptr->enc_info) return GF_ISOM_INVALID_FILE;
		ptr->enc_info = (GF_AdobeEncryptionInfoBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_AKEY:
		if (ptr->key_info) return GF_ISOM_INVALID_FILE;
		ptr->key_info = (GF_AdobeKeyInfoBox *)a;
		break;

	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err aprm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return gf_isom_read_box_list(s, bs, aprm_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err aprm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeStdEncryptionParamsBox *ptr = (GF_AdobeStdEncryptionParamsBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//aeib
	e = gf_isom_box_write((GF_Box *) ptr->enc_info, bs);
	if (e) return e;
	//akey
	e = gf_isom_box_write((GF_Box *) ptr->key_info, bs);
	if (e) return e;

	return GF_OK;
}

GF_Err aprm_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeStdEncryptionParamsBox *ptr = (GF_AdobeStdEncryptionParamsBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	e = gf_isom_box_size((GF_Box *) ptr->enc_info);
	if (e) return e;
	ptr->size += ptr->enc_info->size;
	e = gf_isom_box_size((GF_Box *) ptr->key_info);
	if (e) return e;
	ptr->size += ptr->key_info->size;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *aeib_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeEncryptionInfoBox, GF_ISOM_BOX_TYPE_AEIB);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void aeib_del(GF_Box *s)
{
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox*)s;
	if (!ptr) return;
	if (ptr->enc_algo) gf_free(ptr->enc_algo);
	gf_free(ptr);
}

GF_Err aeib_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox*)s;
	u32 len;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	len = (u32) ptr->size - 1;
	if (len) {
		if (ptr->enc_algo) return GF_ISOM_INVALID_FILE;
		ptr->enc_algo = (char *)gf_malloc(len*sizeof(char));
		gf_bs_read_data(bs, ptr->enc_algo, len);
	}
	ptr->key_length = gf_bs_read_u8(bs);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err aeib_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->enc_algo) {
		gf_bs_write_data(bs, (char *) ptr->enc_algo, (u32) strlen(ptr->enc_algo));
		gf_bs_write_u8(bs, 0); //string end
	}
	gf_bs_write_u8(bs, ptr->key_length);
	return GF_OK;
}

GF_Err aeib_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (ptr->enc_algo)
		ptr->size += strlen(ptr->enc_algo) + 1;
	ptr->size += 1; //KeyLength
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *akey_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeKeyInfoBox, GF_ISOM_BOX_TYPE_AKEY);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void akey_del(GF_Box *s)
{
	GF_AdobeKeyInfoBox *ptr = (GF_AdobeKeyInfoBox *)s;
	if (!ptr) return;
	if (ptr->params) gf_isom_box_del((GF_Box *)ptr->params);
	gf_free(s);
}

GF_Err akey_AddBox(GF_Box *s, GF_Box *a)
{
	GF_AdobeKeyInfoBox *ptr = (GF_AdobeKeyInfoBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_FLXS:
		if (ptr->params) return GF_ISOM_INVALID_FILE;
		ptr->params = (GF_AdobeFlashAccessParamsBox *)a;
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err akey_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	return gf_isom_read_box_list(s, bs, akey_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err akey_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeKeyInfoBox *ptr = (GF_AdobeKeyInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	e = gf_isom_box_write((GF_Box *) ptr->params, bs);
	if (e) return e;

	return GF_OK;
}

GF_Err akey_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeKeyInfoBox *ptr = (GF_AdobeKeyInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	e = gf_isom_box_size((GF_Box *) ptr->params);
	if (e) return e;
	ptr->size += ptr->params->size;
	e = gf_isom_box_size((GF_Box *) ptr->params);
	return e;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *flxs_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeFlashAccessParamsBox, GF_ISOM_BOX_TYPE_FLXS);
	return (GF_Box *)tmp;
}

void flxs_del(GF_Box *s)
{
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox*)s;
	if (!ptr) return;
	if (ptr->metadata)
		gf_free(ptr->metadata);
	gf_free(ptr);
}

GF_Err flxs_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox*)s;
	u32 len;

	len = (u32) ptr->size;
	if (len) {
		if (ptr->metadata) return GF_ISOM_INVALID_FILE;
		ptr->metadata = (char *)gf_malloc(len*sizeof(char));
		gf_bs_read_data(bs, ptr->metadata, len);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err flxs_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->metadata) {
		gf_bs_write_data(bs, ptr->metadata, (u32) strlen(ptr->metadata));
		gf_bs_write_u8(bs, 0); //string end
	}
	return GF_OK;
}

GF_Err flxs_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (ptr->metadata)
		ptr->size += strlen(ptr->metadata) + 1;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *adaf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeDRMAUFormatBox, GF_ISOM_BOX_TYPE_ADAF);
	return (GF_Box *)tmp;
}

void adaf_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err adaf_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeDRMAUFormatBox *ptr = (GF_AdobeDRMAUFormatBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->selective_enc = gf_bs_read_u8(bs);
	gf_bs_read_u8(bs);//resersed
	ptr->IV_length = gf_bs_read_u8(bs);
	ptr->size -= 3;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err adaf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AdobeDRMAUFormatBox *ptr = (GF_AdobeDRMAUFormatBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->selective_enc);
	gf_bs_write_u8(bs, 0x0);
	gf_bs_write_u8(bs, ptr->IV_length);
	return GF_OK;
}

GF_Err adaf_Size(GF_Box *s)
{
	GF_Err e;
	GF_AdobeDRMAUFormatBox *ptr = (GF_AdobeDRMAUFormatBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 3;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


#endif /*GPAC_DISABLE_ISOM*/
