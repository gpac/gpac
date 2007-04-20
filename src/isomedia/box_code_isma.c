/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2005
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
 *
 *  GPAC is free software you can redistribute it and/or modify
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

/* ProtectionInfo Box */
GF_Box *sinf_New()
{
	GF_ProtectionInfoBox *tmp = (GF_ProtectionInfoBox *) malloc(sizeof(GF_ProtectionInfoBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ProtectionInfoBox));
	tmp->type = GF_ISOM_BOX_TYPE_SINF;
	return (GF_Box *)tmp;
}

void sinf_del(GF_Box *s)
{
	GF_ProtectionInfoBox *ptr = (GF_ProtectionInfoBox *)s;
	if (ptr == NULL) return;
	if (ptr->original_format) gf_isom_box_del((GF_Box *)ptr->original_format);
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	if (ptr->scheme_type) gf_isom_box_del((GF_Box *)ptr->scheme_type);
	free(ptr);
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
		gf_isom_box_del(a); 
		break;
	}
	return GF_OK;
}

GF_Err sinf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, sinf_AddBox);
}

#ifndef GPAC_READ_ONLY
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
#endif //GPAC_READ_ONLY

/* OriginalFormat Box */
GF_Box *frma_New()
{
	GF_OriginalFormatBox *tmp = (GF_OriginalFormatBox *) malloc(sizeof(GF_OriginalFormatBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_OriginalFormatBox));
	tmp->type = GF_ISOM_BOX_TYPE_FRMA;
	return (GF_Box *)tmp;
}

void frma_del(GF_Box *s)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}

GF_Err frma_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	ptr->data_format = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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
#endif //GPAC_READ_ONLY

/* SchemeType Box */
GF_Box *schm_New()
{
	GF_SchemeTypeBox *tmp = (GF_SchemeTypeBox *) malloc(sizeof(GF_SchemeTypeBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SchemeTypeBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_SCHM;
	return (GF_Box *)tmp;
}

void schm_del(GF_Box *s)
{
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *)s;
	if (ptr == NULL) return;
	if (ptr->URI) free(ptr->URI);
	free(ptr);
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
		ptr->URI = (char*)malloc(sizeof(char)*len);
		if (!ptr->URI) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->URI, len);
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
GF_Err schm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	gf_bs_write_u32(bs, ptr->scheme_type);
	gf_bs_write_u32(bs, ptr->scheme_version);
	if (ptr->flags & 0x000001) gf_bs_write_data(bs, ptr->URI, strlen(ptr->URI)+1);
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
#endif //GPAC_READ_ONLY

/* SchemeInformation Box */
GF_Box *schi_New()
{
	GF_SchemeInformationBox *tmp = (GF_SchemeInformationBox *) malloc(sizeof(GF_SchemeInformationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SchemeInformationBox));
	tmp->type = GF_ISOM_BOX_TYPE_SCHI;
	return (GF_Box *)tmp;
}

void schi_del(GF_Box *s)
{
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;
	if (ptr == NULL) return;
	if (ptr->ikms) gf_isom_box_del((GF_Box *)ptr->ikms);
	if (ptr->isfm) gf_isom_box_del((GF_Box *)ptr->isfm);
	free(ptr);
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
	default:
		gf_isom_box_del(a);
		return GF_OK;
	}
}

GF_Err schi_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, schi_AddBox);
}

#ifndef GPAC_READ_ONLY
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
	return GF_OK;
}

#endif //GPAC_READ_ONLY

/* ISMAKMS Box */
GF_Box *iKMS_New()
{
	GF_ISMAKMSBox *tmp = (GF_ISMAKMSBox *) malloc(sizeof(GF_ISMAKMSBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ISMAKMSBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_IKMS;
	return (GF_Box *)tmp;
}

void iKMS_del(GF_Box *s)
{
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (ptr == NULL) return;
	if (ptr->URI) free(ptr->URI);
	free(ptr);
}

GF_Err iKMS_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 len;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	len = (u32) (ptr->size);
	ptr->URI = (char*) malloc(sizeof(char)*len);
	if (!ptr->URI) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->URI, len);
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
GF_Err iKMS_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->URI, strlen(ptr->URI)+1);
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
#endif //GPAC_READ_ONLY

/* ISMASampleFormat Box */
GF_Box *iSFM_New()
{
	GF_ISMASampleFormatBox *tmp = (GF_ISMASampleFormatBox *) malloc(sizeof(GF_ISMASampleFormatBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ISMASampleFormatBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_ISFM;
	return (GF_Box *)tmp;
}

void iSFM_del(GF_Box *s)
{
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	if (ptr == NULL) return;
	free(ptr);
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

#ifndef GPAC_READ_ONLY
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
#endif //GPAC_READ_ONLY



/* OMADRMCommonHeader Box */
GF_Box *ohdr_New()
{
	GF_OMADRMCommonHeaderBox *tmp;
	GF_SAFEALLOC(tmp, GF_OMADRMCommonHeaderBox);
	if (tmp == NULL) return NULL;
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_OHDR;
	tmp->ExtendedHeaders = gf_list_new();
	return (GF_Box *)tmp;
}

void ohdr_del(GF_Box *s)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox*)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->ExtendedHeaders);
	if (ptr->ContentID) free(ptr->ContentID);
	if (ptr->RightsIssuerURL) free(ptr->RightsIssuerURL);
	if (ptr->TextualHeaders) free(ptr->TextualHeaders);
	free(ptr);
}

GF_Err ohdr_AddBox(GF_Box *s, GF_Box *a)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox*)s;
	return gf_list_add(ptr->ExtendedHeaders, a);
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
		ptr->ContentID = (char *)malloc(sizeof(char)*(cid_len+1));
		gf_bs_read_data(bs, ptr->ContentID, cid_len);
		ptr->ContentID[cid_len]=0;
	}

	if (ri_len) {
		ptr->RightsIssuerURL = (char *)malloc(sizeof(char)*(ri_len+1));
		gf_bs_read_data(bs, ptr->RightsIssuerURL, ri_len);
		ptr->RightsIssuerURL[ri_len]=0;
	}
	
	if (ptr->TextualHeadersLen) {
		ptr->TextualHeaders = (char *)malloc(sizeof(char)*(ptr->TextualHeadersLen+1));
		gf_bs_read_data(bs, ptr->TextualHeaders, ptr->TextualHeadersLen);
		ptr->TextualHeaders[ptr->TextualHeadersLen] = 0;
	}

	ptr->size -= cid_len+ri_len+ptr->TextualHeadersLen;

	return gf_isom_read_box_list(s, bs, ohdr_AddBox);
}

#ifndef GPAC_READ_ONLY
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
	
	cid_len = ptr->ContentID ? strlen(ptr->ContentID) : 0;
	gf_bs_write_u16(bs, cid_len);
	ri_len = ptr->RightsIssuerURL ? strlen(ptr->RightsIssuerURL) : 0;
	gf_bs_write_u16(bs, ri_len);
	gf_bs_write_u16(bs, ptr->TextualHeadersLen);

	if (cid_len) gf_bs_write_data(bs, ptr->ContentID, strlen(ptr->ContentID));
	if (ri_len) gf_bs_write_data(bs, ptr->RightsIssuerURL, strlen(ptr->RightsIssuerURL));
	if (ptr->TextualHeadersLen) gf_bs_write_data(bs, ptr->TextualHeaders, ptr->TextualHeadersLen);
	ptr->size -= cid_len+ri_len+ptr->TextualHeadersLen;
	return gf_isom_box_array_write(s, ptr->ExtendedHeaders, bs);
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
	return gf_isom_box_array_size(s, ptr->ExtendedHeaders);
}
#endif //GPAC_READ_ONLY


/* OMADRMGroupID Box */
GF_Box *grpi_New()
{
	GF_OMADRMGroupIDBox *tmp;
	GF_SAFEALLOC(tmp, GF_OMADRMGroupIDBox);
	if (tmp == NULL) return NULL;
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_GRPI;
	return (GF_Box *)tmp;
}

void grpi_del(GF_Box *s)
{
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	if (ptr == NULL) return;
	if (ptr->GroupID) free(ptr->GroupID);
	if (ptr->GroupKey) free(ptr->GroupKey);
	free(ptr);
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

	ptr->GroupID = malloc(sizeof(char)*(gid_len+1));
	gf_bs_read_data(bs, ptr->GroupID, gid_len);
	ptr->GroupID[gid_len]=0;
	
	ptr->GroupKey = (char *)malloc(sizeof(char)*ptr->GKLength);
	gf_bs_read_data(bs, ptr->GroupKey, ptr->GKLength);
	ptr->size -= gid_len+ptr->GKLength;
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
GF_Err grpi_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u16 gid_len;
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gid_len = ptr->GroupID ? strlen(ptr->GroupID) : 0;
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
#endif //GPAC_READ_ONLY




/* OMADRMMutableInformation Box */
GF_Box *mdri_New()
{
	GF_OMADRMMutableInformationBox *tmp;
	GF_SAFEALLOC(tmp, GF_OMADRMMutableInformationBox);
	if (tmp == NULL) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_MDRI;
	tmp->boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void mdri_del(GF_Box *s)
{
	GF_OMADRMMutableInformationBox*ptr = (GF_OMADRMMutableInformationBox*)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->boxes);
	free(ptr);
}

GF_Err mdri_AddBox(GF_Box *s, GF_Box *a)
{
	GF_OMADRMMutableInformationBox *ptr = (GF_OMADRMMutableInformationBox *)s;
	return gf_list_add(ptr->boxes, a);
}

GF_Err mdri_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, mdri_AddBox);
}

#ifndef GPAC_READ_ONLY
GF_Err mdri_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_OMADRMMutableInformationBox*ptr = (GF_OMADRMMutableInformationBox*)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return gf_isom_box_array_write(s, ptr->boxes, bs);
}

GF_Err mdri_Size(GF_Box *s)
{
	GF_Err e;
	GF_OMADRMMutableInformationBox *ptr = (GF_OMADRMMutableInformationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	return gf_isom_box_array_size(s, ptr->boxes);
}
#endif //GPAC_READ_ONLY


/* OMADRMTransactionTracking Box */
GF_Box *odtt_New()
{
	GF_OMADRMTransactionTrackingBox *tmp;
	GF_SAFEALLOC(tmp, GF_OMADRMTransactionTrackingBox);
	if (tmp == NULL) return NULL;
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_ODTT;
	return (GF_Box *)tmp;
}

void odtt_del(GF_Box *s)
{
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox*)s;
	free(ptr);
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

#ifndef GPAC_READ_ONLY
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
#endif //GPAC_READ_ONLY



/* OMADRMRightsObject Box */
GF_Box *odrb_New()
{
	GF_OMADRMRightsObjectBox *tmp;
	GF_SAFEALLOC(tmp, GF_OMADRMRightsObjectBox);
	if (tmp == NULL) return NULL;
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_ODRB;
	return (GF_Box *)tmp;
}

void odrb_del(GF_Box *s)
{
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox*)s;
	if (ptr->oma_ro) free(ptr->oma_ro);
	free(ptr);
}

GF_Err odrb_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->oma_ro_size = (u32) ptr->size;
	ptr->oma_ro = (char*) malloc(sizeof(char)*ptr->oma_ro_size);
	gf_bs_read_data(bs, ptr->oma_ro, ptr->oma_ro_size);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
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
#endif //GPAC_READ_ONLY




/* OMADRMKMS Box */
GF_Box *odkm_New()
{
	GF_OMADRMKMSBox *tmp;
	GF_SAFEALLOC(tmp, GF_OMADRMKMSBox);
	if (tmp == NULL) return NULL;
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_ODKM;
	return (GF_Box *)tmp;
}

void odkm_del(GF_Box *s)
{
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	if (ptr->hdr) gf_isom_box_del((GF_Box*)ptr->hdr);
	if (ptr->fmt) gf_isom_box_del((GF_Box*)ptr->fmt);
	free(ptr);
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

#ifndef GPAC_READ_ONLY
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
#endif //GPAC_READ_ONLY


