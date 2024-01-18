/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2005-2023
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
GF_Box *sinf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ProtectionSchemeInfoBox, GF_ISOM_BOX_TYPE_SINF);
	return (GF_Box *)tmp;
}

void sinf_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err sinf_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_ProtectionSchemeInfoBox *ptr = (GF_ProtectionSchemeInfoBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_FRMA:
		BOX_FIELD_ASSIGN(original_format, GF_OriginalFormatBox)
		break;
	case GF_ISOM_BOX_TYPE_SCHM:
		BOX_FIELD_ASSIGN(scheme_type, GF_SchemeTypeBox)
		break;
	case GF_ISOM_BOX_TYPE_SCHI:
		BOX_FIELD_ASSIGN(info, GF_SchemeInformationBox)
		break;
	}
	return GF_OK;
}

GF_Err sinf_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sinf_box_write(GF_Box *s, GF_BitStream *bs)
{
	return  gf_isom_box_write_header(s, bs);
}

GF_Err sinf_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_ProtectionSchemeInfoBox *ptr = (GF_ProtectionSchemeInfoBox *)s;
	gf_isom_check_position(s, (GF_Box *)ptr->original_format, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->scheme_type, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->info, &pos);
    return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* OriginalFormat Box */
GF_Box *frma_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OriginalFormatBox, GF_ISOM_BOX_TYPE_FRMA);
	return (GF_Box *)tmp;
}

void frma_box_del(GF_Box *s)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err frma_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->data_format = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err frma_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->gnr_type)
		gf_bs_write_u32(bs, ptr->gnr_type);
	else
		gf_bs_write_u32(bs, ptr->data_format);
	return GF_OK;
}

GF_Err frma_box_size(GF_Box *s)
{
	GF_OriginalFormatBox *ptr = (GF_OriginalFormatBox *)s;
	ptr->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* SchemeType Box */
GF_Box *schm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SchemeTypeBox, GF_ISOM_BOX_TYPE_SCHM);
	return (GF_Box *)tmp;
}

void schm_box_del(GF_Box *s)
{
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *)s;
	if (ptr == NULL) return;
	if (ptr->URI) gf_free(ptr->URI);
	gf_free(ptr);
}

GF_Err schm_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *)s;

	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->scheme_type = gf_bs_read_u32(bs);
	ptr->scheme_version = gf_bs_read_u32(bs);

	if (ptr->size && (ptr->flags & 0x000001)) {
		u32 len = (u32) (ptr->size);
		ptr->URI = (char*)gf_malloc(sizeof(char)*(len+1));
		if (!ptr->URI) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->URI, len);
		ptr->URI[len] = 0;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err schm_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->scheme_type);
	gf_bs_write_u32(bs, ptr->scheme_version);
	if (ptr->flags & 0x000001) {
		if (ptr->URI)
			gf_bs_write_data(bs, ptr->URI, (u32) strlen(ptr->URI)+1);
		else
			gf_bs_write_u8(bs, 0);
	}
	return GF_OK;
}

GF_Err schm_box_size(GF_Box *s)
{
	GF_SchemeTypeBox *ptr = (GF_SchemeTypeBox *) s;
	if (!s) return GF_BAD_PARAM;
	ptr->size += 8;
	if (ptr->flags & 0x000001) ptr->size += 1 + (ptr->URI ? strlen(ptr->URI) : 0);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* SchemeInformation Box */
GF_Box *schi_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SchemeInformationBox, GF_ISOM_BOX_TYPE_SCHI);
	return (GF_Box *)tmp;
}

void schi_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err schi_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IKMS:
		BOX_FIELD_ASSIGN(ikms, GF_ISMAKMSBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ISFM:
		BOX_FIELD_ASSIGN(isfm, GF_ISMASampleFormatBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ISLT:
		BOX_FIELD_ASSIGN(islt, GF_ISMACrypSaltBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ODKM:
		BOX_FIELD_ASSIGN(odkm, GF_OMADRMKMSBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TENC:
		BOX_FIELD_ASSIGN(tenc, GF_TrackEncryptionBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ADKM:
		BOX_FIELD_ASSIGN(adkm, GF_AdobeDRMKeyManagementSystemBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UUID:
		if (((GF_UUIDBox*)a)->internal_4cc==GF_ISOM_BOX_UUID_TENC) {
			BOX_FIELD_ASSIGN(piff_tenc, GF_PIFFTrackEncryptionBox)
			return GF_OK;
		} else {
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err schi_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err schi_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err schi_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_SchemeInformationBox *ptr = (GF_SchemeInformationBox *)s;

	gf_isom_check_position(s, (GF_Box *)ptr->ikms, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->isfm, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->islt, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->odkm, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->tenc, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->adkm, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->piff_tenc, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* ISMAKMS Box */
GF_Box *iKMS_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ISMAKMSBox, GF_ISOM_BOX_TYPE_IKMS);
	return (GF_Box *)tmp;
}

void iKMS_box_del(GF_Box *s)
{
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (ptr == NULL) return;
	if (ptr->URI) gf_free(ptr->URI);
	gf_free(ptr);
}

GF_Err iKMS_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;

	len = (u32) (ptr->size);
	ptr->URI = (char*) gf_malloc(sizeof(char)*len);
	if (!ptr->URI) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->URI, len);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iKMS_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
    if (ptr->URI)
        gf_bs_write_data(bs, ptr->URI, (u32) strlen(ptr->URI));
    gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err iKMS_box_size(GF_Box *s)
{
	GF_ISMAKMSBox *ptr = (GF_ISMAKMSBox *)s;
    ptr->size += (ptr->URI ? strlen(ptr->URI) : 0) + 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* ISMASampleFormat Box */
GF_Box *iSFM_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ISMASampleFormatBox, GF_ISOM_BOX_TYPE_ISFM);
	return (GF_Box *)tmp;
}

void iSFM_box_del(GF_Box *s)
{
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err iSFM_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;

	ISOM_DECREASE_SIZE(ptr, 3);
	ptr->selective_encryption = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 7);
	ptr->key_indicator_length = gf_bs_read_u8(bs);
	ptr->IV_length = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iSFM_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err iSFM_box_size(GF_Box *s)
{
	GF_ISMASampleFormatBox *ptr = (GF_ISMASampleFormatBox *)s;
	ptr->size += 3;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* ISMASampleFormat Box */
GF_Box *iSLT_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ISMACrypSaltBox, GF_ISOM_BOX_TYPE_ISLT);
	return (GF_Box *)tmp;
}

void iSLT_box_del(GF_Box *s)
{
	GF_ISMACrypSaltBox *ptr = (GF_ISMACrypSaltBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err iSLT_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ISMACrypSaltBox *ptr = (GF_ISMACrypSaltBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->salt = gf_bs_read_u64(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iSLT_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ISMACrypSaltBox *ptr = (GF_ISMACrypSaltBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u64(bs, ptr->salt);
	return GF_OK;
}

GF_Err iSLT_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



/* OMADRMCommonHeader Box */
GF_Box *ohdr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMCommonHeaderBox, GF_ISOM_BOX_TYPE_OHDR);
	tmp->child_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void ohdr_box_del(GF_Box *s)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox*)s;
	if (ptr == NULL) return;
	if (ptr->ContentID) gf_free(ptr->ContentID);
	if (ptr->RightsIssuerURL) gf_free(ptr->RightsIssuerURL);
	if (ptr->TextualHeaders) gf_free(ptr->TextualHeaders);
	gf_free(ptr);
}

GF_Err ohdr_box_read(GF_Box *s, GF_BitStream *bs)
{
	u16 cid_len, ri_len;
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox*)s;

	ISOM_DECREASE_SIZE(ptr, (1+1+8+2+2+2) );
	ptr->EncryptionMethod = gf_bs_read_u8(bs);
	ptr->PaddingScheme = gf_bs_read_u8(bs);
	ptr->PlaintextLength = gf_bs_read_u64(bs);
	cid_len = gf_bs_read_u16(bs);
	ri_len = gf_bs_read_u16(bs);
	ptr->TextualHeadersLen = gf_bs_read_u16(bs);

	if (ptr->size<cid_len+ri_len+ptr->TextualHeadersLen) return GF_ISOM_INVALID_FILE;

	if (cid_len) {
		ptr->ContentID = (char *)gf_malloc(sizeof(char)*(cid_len+1));
		if (!ptr->ContentID) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->ContentID, cid_len);
		ptr->ContentID[cid_len]=0;
	}

	if (ri_len) {
		ptr->RightsIssuerURL = (char *)gf_malloc(sizeof(char)*(ri_len+1));
		if (!ptr->RightsIssuerURL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->RightsIssuerURL, ri_len);
		ptr->RightsIssuerURL[ri_len]=0;
	}

	if (ptr->TextualHeadersLen) {
		ptr->TextualHeaders = (char *)gf_malloc(sizeof(char)*(ptr->TextualHeadersLen+1));
		if (!ptr->TextualHeaders) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->TextualHeaders, ptr->TextualHeadersLen);
		ptr->TextualHeaders[ptr->TextualHeadersLen] = 0;
	}

	ISOM_DECREASE_SIZE(ptr, (cid_len+ri_len+ptr->TextualHeadersLen) );

	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ohdr_box_write(GF_Box *s, GF_BitStream *bs)
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

	ISOM_DECREASE_SIZE(ptr, (cid_len+ri_len+ptr->TextualHeadersLen) );
	return GF_OK;
}

GF_Err ohdr_box_size(GF_Box *s)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox *)s;
	ptr->size += 1+1+8+2+2+2;
	if (ptr->ContentID) ptr->size += strlen(ptr->ContentID);
	if (ptr->RightsIssuerURL) ptr->size += strlen(ptr->RightsIssuerURL);
	if (ptr->TextualHeadersLen) ptr->size += ptr->TextualHeadersLen;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* OMADRMGroupID Box */
GF_Box *grpi_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMGroupIDBox, GF_ISOM_BOX_TYPE_GRPI);
	return (GF_Box *)tmp;
}

void grpi_box_del(GF_Box *s)
{
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	if (ptr == NULL) return;
	if (ptr->GroupID) gf_free(ptr->GroupID);
	if (ptr->GroupKey) gf_free(ptr->GroupKey);
	gf_free(ptr);
}

GF_Err grpi_box_read(GF_Box *s, GF_BitStream *bs)
{
	u16 gid_len;
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox*)s;

	ISOM_DECREASE_SIZE(ptr, (1+2+2) );
	gid_len = gf_bs_read_u16(bs);
	ptr->GKEncryptionMethod = gf_bs_read_u8(bs);
	ptr->GKLength = gf_bs_read_u16(bs);

	if (ptr->size<gid_len+ptr->GKLength) return GF_ISOM_INVALID_FILE;

	ptr->GroupID = gf_malloc(sizeof(char)*(gid_len+1));
	if (!ptr->GroupID) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->GroupID, gid_len);
	ptr->GroupID[gid_len]=0;

	ptr->GroupKey = (char *)gf_malloc(sizeof(char)*ptr->GKLength);
	if (!ptr->GroupKey) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->GroupKey, ptr->GKLength);
	ISOM_DECREASE_SIZE(ptr, (gid_len+ptr->GKLength) );
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err grpi_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err grpi_box_size(GF_Box *s)
{
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)s;
	ptr->size += 2+2+1 + ptr->GKLength;
	if (ptr->GroupID) ptr->size += strlen(ptr->GroupID);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




/* OMADRMMutableInformation Box */
GF_Box *mdri_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMMutableInformationBox, GF_ISOM_BOX_TYPE_MDRI);
	return (GF_Box *)tmp;
}

void mdri_box_del(GF_Box *s)
{
	GF_OMADRMMutableInformationBox*ptr = (GF_OMADRMMutableInformationBox*)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err mdri_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err mdri_box_write(GF_Box *s, GF_BitStream *bs)
{
//	GF_OMADRMMutableInformationBox*ptr = (GF_OMADRMMutableInformationBox*)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err mdri_box_size(GF_Box *s)
{
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* OMADRMTransactionTracking Box */
GF_Box *odtt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMTransactionTrackingBox, GF_ISOM_BOX_TYPE_ODTT);
	return (GF_Box *)tmp;
}

void odtt_box_del(GF_Box *s)
{
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox*)s;
	gf_free(ptr);
}

GF_Err odtt_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox *)s;

	gf_bs_read_data(bs, ptr->TransactionID, 16);
	ISOM_DECREASE_SIZE(ptr, 16);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err odtt_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox*)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->TransactionID, 16);
	return GF_OK;
}

GF_Err odtt_box_size(GF_Box *s)
{
	s->size += 16;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



/* OMADRMRightsObject Box */
GF_Box *odrb_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMRightsObjectBox, GF_ISOM_BOX_TYPE_ODRB);
	return (GF_Box *)tmp;
}

void odrb_box_del(GF_Box *s)
{
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox*)s;
	if (ptr->oma_ro) gf_free(ptr->oma_ro);
	gf_free(ptr);
}

GF_Err odrb_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;

	ptr->oma_ro_size = (u32) ptr->size;
	ptr->oma_ro = (char*) gf_malloc(sizeof(char)*ptr->oma_ro_size);
	if (!ptr->oma_ro) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->oma_ro, ptr->oma_ro_size);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err odrb_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->oma_ro, ptr->oma_ro_size);
	return GF_OK;
}

GF_Err odrb_box_size(GF_Box *s)
{
	GF_OMADRMRightsObjectBox *ptr = (GF_OMADRMRightsObjectBox *)s;
	s->size += ptr->oma_ro_size;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




/* OMADRMKMS Box */
GF_Box *odkm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OMADRMKMSBox, GF_ISOM_BOX_TYPE_ODKM);
	return (GF_Box *)tmp;
}

void odkm_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err odkm_Add(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_OHDR:
		BOX_FIELD_ASSIGN(hdr, GF_OMADRMCommonHeaderBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_ODAF:
		BOX_FIELD_ASSIGN(fmt, GF_OMADRMAUFormatBox)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err odkm_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err odkm_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err odkm_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_OMADRMKMSBox *ptr = (GF_OMADRMKMSBox *)s;
	gf_isom_check_position(s, (GF_Box *)ptr->hdr, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->fmt, &pos);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *pssh_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ProtectionSystemHeaderBox, GF_ISOM_BOX_TYPE_PSSH);
	return (GF_Box *)tmp;
}

void pssh_box_del(GF_Box *s)
{
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox*)s;
	if (ptr == NULL) return;
	if (ptr->private_data) gf_free(ptr->private_data);
	if (ptr->KIDs) gf_free(ptr->KIDs);
	gf_free(ptr);
}

GF_Err pssh_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox *)s;

	gf_bs_read_data(bs, (char *) ptr->SystemID, 16);
	ISOM_DECREASE_SIZE(ptr, 16);
	if (ptr->version > 0) {
		ptr->KID_count = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 4);
		if (ptr->KID_count) {
			u32 i;
			if (ptr->size / sizeof(bin128) < ptr->KID_count)
				return GF_ISOM_INVALID_FILE;
			ptr->KIDs = gf_malloc(ptr->KID_count*sizeof(bin128));
			if (!ptr->KIDs)
				return GF_OUT_OF_MEM;
			for (i=0; i<ptr->KID_count; i++) {
				gf_bs_read_data(bs, (char *) ptr->KIDs[i], 16);
				ISOM_DECREASE_SIZE(ptr, 16);
			}
		}
	}
	ptr->private_data_size = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->private_data_size) {
		if (ptr->size < ptr->private_data_size)
			return GF_ISOM_INVALID_FILE;
		ptr->private_data = gf_malloc(sizeof(char)*ptr->private_data_size);
		if (!ptr->private_data)
			return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, (char *) ptr->private_data, ptr->private_data_size);
		ISOM_DECREASE_SIZE(ptr, ptr->private_data_size);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pssh_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err pssh_box_size(GF_Box *s)
{
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox*)s;

	if (ptr->KID_count && !ptr->version) {
		ptr->version = 1;
	}

	ptr->size += 16;
	if (ptr->version) ptr->size += 4 + 16*ptr->KID_count;
	ptr->size += 4 + (ptr->private_data ? ptr->private_data_size : 0);
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


GF_Box *tenc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackEncryptionBox, GF_ISOM_BOX_TYPE_TENC);
	return (GF_Box *)tmp;
}

void tenc_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err tenc_box_read(GF_Box *s, GF_BitStream *bs)
{
	u8 iv_size;
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*)s;

	ISOM_DECREASE_SIZE(ptr, 3);

	gf_bs_read_u8(bs); //reserved

	if (!ptr->version) {
		gf_bs_read_u8(bs); //reserved
	} else {
		ptr->crypt_byte_block = gf_bs_read_int(bs, 4);
		ptr->skip_byte_block = gf_bs_read_int(bs, 4);
	}
	ptr->isProtected = gf_bs_read_u8(bs);


	ISOM_DECREASE_SIZE(ptr, 17);

	ptr->key_info[0] = 0;
	ptr->key_info[1] = 0;
	ptr->key_info[2] = 0;
	ptr->key_info[3] = iv_size = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, ptr->key_info+4, 16);
	if (!iv_size && ptr->isProtected) {
		ISOM_DECREASE_SIZE(ptr, 1);
		iv_size = ptr->key_info[20] = gf_bs_read_u8(bs);
		ISOM_DECREASE_SIZE(ptr, ptr->key_info[20]);
		if ((iv_size!=8) && (iv_size!=16)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid constant IV size %d, must be 8 or 16\n", (u32) iv_size));
			ptr->key_info[20] = 16;
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_bs_read_data(bs, ptr->key_info+21, iv_size);
	}
	else if ((iv_size!=0) && (iv_size!=8) && (iv_size!=16)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid IV size %d, must be 0, 8 or 16\n", (u32) iv_size));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tenc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, 0); //reserved

	if (!ptr->version) {
		gf_bs_write_u8(bs, 0); //reserved
	} else {
		gf_bs_write_int(bs, ptr->crypt_byte_block, 4);
		gf_bs_write_int(bs, ptr->skip_byte_block, 4);
	}
	gf_bs_write_u8(bs, ptr->isProtected);

	gf_bs_write_u8(bs, ptr->key_info[3]);
	gf_bs_write_data(bs, ptr->key_info + 4, 16);
	if ((ptr->isProtected == 1) && !ptr->key_info[3]) {
		gf_bs_write_u8(bs, ptr->key_info[20]);
		gf_bs_write_data(bs, ptr->key_info + 21, ptr->key_info[20]);
	}
	return GF_OK;
}

GF_Err tenc_box_size(GF_Box *s)
{
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*)s;
	ptr->size += 3;

	ptr->size += 17;
	if ((ptr->isProtected == 1) && ! ptr->key_info[3]) {
		ptr->size += 1 + ptr->key_info[20];
	}
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *piff_tenc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PIFFTrackEncryptionBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_TENC;
	return (GF_Box *)tmp;
}

void piff_tenc_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err piff_tenc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox*)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	//PIFF TENC extends UUID and fullbox
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);

	ISOM_DECREASE_SIZE(ptr, 20);
	ptr->AlgorithmID = gf_bs_read_int(bs, 24);
	ptr->key_info[0] = 0;
	ptr->key_info[1] = 0;
	ptr->key_info[2] = 0;
	ptr->key_info[3] = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, ptr->key_info+4, 16);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err piff_tenc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	gf_bs_write_int(bs, ptr->AlgorithmID, 24);
	gf_bs_write_u8(bs, ptr->key_info[3]);
	gf_bs_write_data(bs, ptr->key_info+4, 16);
	return GF_OK;
}

GF_Err piff_tenc_box_size(GF_Box *s)
{
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox*)s;
	ptr->size += 24;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


GF_Box *piff_psec_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleEncryptionBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_PSEC;
	tmp->piff_type = 1;
	return (GF_Box *)tmp;
}

void piff_psec_box_del(GF_Box *s)
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


GF_Err piff_psec_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	//PIFF PSEC extends UUID and fullbox
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);

	if (ptr->flags & 1) {
		ISOM_DECREASE_SIZE(ptr, 20);
		ptr->AlgorithmID = gf_bs_read_int(bs, 24);
		ptr->IV_size = gf_bs_read_u8(bs);
		gf_bs_read_data(bs, (char *) ptr->KID, 16);
	}
	if (ptr->IV_size == 0)
		ptr->IV_size = 8; //default to 8

	ptr->bs_offset = gf_bs_get_position(bs);

	/*u32 sample_count = */gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->IV_size != 8 && ptr->IV_size != 16) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] PIFF PSEC box incorrect IV size: %u - shall be 8 or 16\n", ptr->IV_size));
		return GF_BAD_PARAM;
	}
	//as for senc, we skip parsing of the box until we have all saiz/saio info
	gf_bs_skip_bytes(bs, ptr->size);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err store_senc_info(GF_SampleEncryptionBox *ptr, GF_BitStream *bs)
{
	GF_Err e;
	u64 pos, new_pos;
	if (!ptr->cenc_saio) return GF_OK;

	pos = gf_bs_get_position(bs);
	if (pos>0xFFFFFFFFULL) {
		if (ptr->cenc_saio && !ptr->cenc_saio->version) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] saio offset larger than 32-bits but box version 0 enforced. Retry without \"saio32\" option\n"));
			return GF_BAD_PARAM;
		}
	}
	e = gf_bs_seek(bs, ptr->cenc_saio->offset_first_offset_field);
	if (e) return e;
	//force using version 1 for saio box i.e offset has 64 bits
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->traf) {
		new_pos = pos - ptr->traf->moof_start_in_bs;
	} else
#endif
	{
		new_pos = pos;
	}

	if (ptr->cenc_saio->offsets) {
		u32 i;
		u64 old_offset = ptr->cenc_saio->offsets[0];
		for (i=0; i<ptr->cenc_saio->entry_count; i++) {
			if (ptr->cenc_saio->version) {
				gf_bs_write_u64(bs, new_pos + ptr->cenc_saio->offsets[i] - old_offset);
			} else {
				gf_bs_write_u32(bs, (u32) (new_pos + ptr->cenc_saio->offsets[i] - old_offset));
			}
			ptr->cenc_saio->offsets[i] = new_pos + ptr->cenc_saio->offsets[i] - old_offset;
		}
	} else {
		if (ptr->cenc_saio->version) {
			gf_bs_write_u64(bs, new_pos);
		} else {
			gf_bs_write_u32(bs, (u32) new_pos);
		}
	}

	return gf_bs_seek(bs, pos);
}

GF_Err piff_psec_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 sample_count;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *) s;
	if (!s) return GF_BAD_PARAM;

	sample_count = gf_list_count(ptr->samp_aux_info);
	if (!sample_count) {
		ptr->size = 0;
		return GF_OK;
	}
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
		u32 i;
		e = store_senc_info((GF_SampleEncryptionBox *)ptr, bs);
		if (e) return e;

		for (i = 0; i < sample_count; i++) {
			GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
			if (! sai->cenc_data_size) continue;
			gf_bs_write_data(bs, (char *)sai->cenc_data, sai->cenc_data_size);
		}
	}
	return GF_OK;
}

GF_Err piff_psec_box_size(GF_Box *s)
{
	u32 i, sample_count;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox*)s;

	sample_count = gf_list_count(ptr->samp_aux_info);
	if (!sample_count) {
		ptr->size = 0;
		return GF_OK;
	}

	ptr->size += 4;
	if (ptr->flags & 1) {
		ptr->size += 20;
	}
	ptr->size += 4;

	for (i = 0; i < sample_count; i++) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
		if (! sai->cenc_data_size) continue;
		ptr->size += sai->cenc_data_size;
	}
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


GF_Box *piff_pssh_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PIFFProtectionSystemHeaderBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_PSSH;
	return (GF_Box *)tmp;
}

void piff_pssh_box_del(GF_Box *s)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*)s;
	if (ptr->private_data) gf_free(ptr->private_data);
	gf_free(s);
}

GF_Err piff_pssh_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*)s;

	ISOM_DECREASE_SIZE(ptr, 24);
	//PIFF PSSH extends UUID and fullbox
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	gf_bs_read_data(bs, (char *) ptr->SystemID, 16);
	ptr->private_data_size = gf_bs_read_u32(bs);

	if (ptr->size < ptr->private_data_size)
	    return GF_ISOM_INVALID_FILE;
	ptr->private_data = gf_malloc(sizeof(char)*ptr->private_data_size);
	if (!ptr->private_data)
	    return GF_OUT_OF_MEM;

	ISOM_DECREASE_SIZE(ptr, ptr->private_data_size);
	gf_bs_read_data(bs, (char *) ptr->private_data, ptr->private_data_size);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err piff_pssh_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox *) s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	gf_bs_write_data(bs, (char *) ptr->SystemID, 16);
	gf_bs_write_u32(bs, ptr->private_data_size);
	gf_bs_write_data(bs, (char *) ptr->private_data, ptr->private_data_size);
	return GF_OK;
}

GF_Err piff_pssh_box_size(GF_Box *s)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*)s;

	ptr->size += 24 + ptr->private_data_size;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *senc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleEncryptionBox, GF_ISOM_BOX_TYPE_SENC);
	return (GF_Box *)tmp;
}

void senc_box_del(GF_Box *s)
{
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *)s;
	while (1) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_pop_back(ptr->samp_aux_info);
		if (!sai) break;
		gf_isom_cenc_samp_aux_info_del(sai);
	}
	if (ptr->samp_aux_info) gf_list_del(ptr->samp_aux_info);
	gf_free(s);
}

#endif //ISOM

u8 key_info_get_iv_size(const u8 *key_info, u32 key_info_size, u32 idx, u8 *const_iv_size, const u8 **const_iv)
{
	u32 i=0, kpos=3;
	if (const_iv_size) *const_iv_size = 0;
	if (const_iv) *const_iv = NULL;

	if (!key_info || !key_info_size)
		return 0;

	while (1) {
		u8 civ_size=0;
		const u8 *civ = NULL;
		u8 iv_size = key_info[kpos];
		kpos += 17;

		if (!iv_size) {
			if (kpos>key_info_size)
				break;
			civ_size = key_info[kpos];
			civ = key_info + kpos + 1;
			kpos += 1 + civ_size;
		}

		if (kpos>key_info_size)
			break;

		if (i+1==idx) {
			if (const_iv_size) *const_iv_size = civ_size;
			if (const_iv) *const_iv = civ;
			return iv_size;
		}
		i++;
		if (kpos==key_info_size)
			break;
	}
	return 0;
}

#ifndef GPAC_DISABLE_ISOM

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err senc_Parse(GF_BitStream *bs, GF_TrackBox *trak, GF_TrackFragmentBox *traf, GF_SampleEncryptionBox *senc, u32 max_nb_samples)
#else
GF_Err senc_Parse(GF_BitStream *bs, GF_TrackBox *trak, void *traf, GF_SampleEncryptionBox *senc, u32 max_nb_samples)
#endif
{
	GF_Err e;
	Bool parse_failed = GF_FALSE;
	u32 i, count, sample_number;
	u32 senc_size = (u32) senc->size;
	u32 subs_size = 0, def_IV_size;
	u64 pos = gf_bs_get_position(bs);
	Bool do_warn = GF_TRUE;
	Bool use_multikey = GF_FALSE;
	Bool patch_subsamples_present = GF_FALSE;

#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (!traf)
		return GF_BAD_PARAM;
#endif

	//BOX + version/flags
	if (senc_size<12) return GF_BAD_PARAM;
	senc_size -= 12;

	if (senc->piff_type==1) {
		//UUID
		if (senc_size<16) return GF_BAD_PARAM;
		senc_size -= 16;
	} else if (!senc->piff_type) {
		if (senc->version==1)
			use_multikey = GF_TRUE;
	}
	if (senc->flags & 2) subs_size = 8;

	if (senc_size<4) return GF_BAD_PARAM;
	if (!max_nb_samples) {
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		if (traf) {
			i=0;
			GF_TrackFragmentRunBox *trun;
			while ((trun = gf_list_enum(traf->TrackRuns, &i))) {
				max_nb_samples += trun->nb_samples;
			}
		} else
#endif
		if (trak->Media->information->sampleTable->SampleSize)
			max_nb_samples = trak->Media->information->sampleTable->SampleSize->sampleCount;
	}

	sample_number = 1;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (trak) sample_number += trak->sample_count_at_seg_start;
#endif

	gf_bs_seek(bs, senc->bs_offset);

	count = gf_bs_read_u32(bs);
	senc_size -= 4;

	def_IV_size = 0;
	//check the target size if we have one subsample
	if (senc_size >= count * (16 + subs_size)) {
		def_IV_size = 16;
	}
	else if (senc_size >= count * (8 + subs_size)) {
		def_IV_size = 8;
	}
	else if (senc_size >= count * (subs_size)) {
		def_IV_size = 0;
	}

	if (gf_opts_get_bool("core", "piff-force-subsamples") && !(senc->flags & 0x00000002))
		patch_subsamples_present = GF_TRUE;

	if (!senc->samp_aux_info) senc->samp_aux_info = gf_list_new();
	for (i=0; i<count; i++) {
		const u8 *key_info=NULL;
		u32 key_info_size=0;
		Bool is_encrypted;
		GF_CENCSampleAuxInfo *sai;
		u8 IV_size=0;
		//u32 nb_keys = 0;
		u32 nb_bytes_subsample = 6;
		u32 nb_subs_bits = 16;

		if (!max_nb_samples)
			break;

		GF_SAFEALLOC(sai, GF_CENCSampleAuxInfo);
		if (!sai) {
			gf_bs_seek(bs, pos);
			return GF_OUT_OF_MEM;
		}
		if (trak) {
			e = gf_isom_get_sample_cenc_info_internal(trak, traf, senc, sample_number, &is_encrypted, NULL, NULL, &key_info, &key_info_size);
			if (! key_info) {
				if (!key_info_size && is_encrypted) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isobmf] no key info and no associated IV size !\n" ));
					gf_free(sai);
					return GF_ISOM_INVALID_FILE;
				}
				IV_size = key_info_size; //piff default
				use_multikey = GF_FALSE;
				senc->piff_type = 2;
			} else if (use_multikey) {
				//nb_keys = key_info[1];
				//nb_keys <<= 8;
				//nb_keys |= key_info[2];
				nb_bytes_subsample = 8;
				nb_subs_bits = 32;
			} else {
				IV_size = key_info[3];
			}

			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isobmf] could not get cenc info for sample %d: %s\n", sample_number, gf_error_to_string(e) ));
				gf_isom_cenc_samp_aux_info_del(sai);
				gf_bs_seek(bs, pos);
				if (trak->moov->mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)
					return GF_OK;
				return e;
			}
		}
		//no init movie setup (segment dump/inspaction, assume default encrypted and 16 bytes IV
		else {
			if (do_warn) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[isobmf] no moov found, cannot get cenc default info, assuming isEncrypted, IV size %d (computed from senc size)\n", def_IV_size));
				do_warn = GF_FALSE;
			}
			is_encrypted = GF_TRUE;
			IV_size = def_IV_size;
		}
		if (senc_size < IV_size) {
			parse_failed = GF_TRUE;
			gf_isom_cenc_samp_aux_info_del(sai);
			break;
		}

		sample_number++;

		//subsample info is only signaled for encrypted samples
		if (is_encrypted) {
			u64 sai_start = gf_bs_get_position(bs);
			u32 nb_subs = 0;

			if (use_multikey) {
				u32 j;
				u32 nb_iv_init = gf_bs_read_u16(bs);
				for (j=0; j<nb_iv_init; j++) {
					u32 idx = gf_bs_read_u16(bs);
					IV_size = key_info_get_iv_size(key_info, key_info_size, idx, NULL, NULL);
					if (!IV_size) {
						gf_isom_cenc_samp_aux_info_del(sai);
						GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[isobmf] Failed to parse SENC box, invalid SAI multikey with IV size 0\n" ));
						gf_bs_seek(bs, pos);
						return GF_ISOM_INVALID_FILE;
					}
					gf_bs_skip_bytes(bs, IV_size);
				}

			} else {
				if (IV_size > 16) {
					gf_isom_cenc_samp_aux_info_del(sai);
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[isobmf] Failed to parse SENC box, invalid SAI size\n" ));
					gf_bs_seek(bs, pos);
					return GF_ISOM_INVALID_FILE;
				}
				if (IV_size) {
					gf_bs_skip_bytes(bs, IV_size);
				}
			}
			if (senc->flags & 0x00000002) {
				nb_subs = gf_bs_read_int(bs, nb_subs_bits);
			}

			sai->cenc_data_size = (u32) (gf_bs_get_position(bs) - sai_start);
			sai->cenc_data_size += nb_subs * nb_bytes_subsample;
			gf_bs_seek(bs, sai_start);

			if ((s32) senc_size < sai->cenc_data_size) {
				parse_failed = GF_TRUE;
				gf_isom_cenc_samp_aux_info_del(sai);
				break;
			}

			sai->cenc_data = gf_malloc(sizeof(u8) * sai->cenc_data_size);
			if (!sai->cenc_data) {
				gf_isom_cenc_samp_aux_info_del(sai);
				gf_bs_seek(bs, pos);
				return GF_OUT_OF_MEM;
			}
			gf_bs_read_data(bs, sai->cenc_data, sai->cenc_data_size);
			senc_size -= sai->cenc_data_size;

			if (patch_subsamples_present) {
				gf_bs_read_int(bs, nb_subs_bits);
			}
		} else {
			i--;
			sai->isNotProtected = 1;
		}

		if (senc->internal_4cc == GF_ISOM_BOX_UUID_PSEC) {
			sai->key_info_size = IV_size;
		} else {
			sai->key_info = key_info;
			sai->key_info_size = key_info_size;
		}
		gf_list_add(senc->samp_aux_info, sai);

		max_nb_samples--;
	}
	gf_bs_seek(bs, pos);
	if (parse_failed) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isobmf] cannot parse senc, missing IV/crypto state\n"));
	}
	return GF_OK;
}


GF_Err senc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);

	ptr->bs_offset = gf_bs_get_position(bs);
	gf_bs_skip_bytes(bs, ptr->size);
	ptr->size = 0;
	ptr->load_needed = GF_TRUE;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err senc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	u32 sample_count, nb_crypt_samples;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *) s;

	sample_count = gf_list_count(ptr->samp_aux_info);
	//temp patch until we cleanup the spec...
	nb_crypt_samples = 0;
	for (i = 0; i < sample_count; i++) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
		if (!sai->isNotProtected) nb_crypt_samples++;
	}
	if (!nb_crypt_samples) {
		ptr->size = 0;
		return GF_OK;
	}

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, ptr->flags);

	gf_bs_write_u32(bs, nb_crypt_samples);

	e = store_senc_info(ptr, bs);
	if (e) return e;

	for (i = 0; i < sample_count; i++) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
		if (sai->isNotProtected || !sai->cenc_data_size)
			continue;
		gf_bs_write_data(bs, sai->cenc_data, sai->cenc_data_size);
	}
	return GF_OK;
}

GF_Err senc_box_size(GF_Box *s)
{
	u32 sample_count;
	u32 i, nb_crypt_samples;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox*)s;
	sample_count = gf_list_count(ptr->samp_aux_info);
	//temp patch until we cleanup the spec...
	nb_crypt_samples=0;
	for (i = 0; i < sample_count; i++) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
		if (!sai->isNotProtected) nb_crypt_samples++;
	}

	if (!nb_crypt_samples) {
		ptr->size = 0;
		return GF_OK;
	}

	//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
	ptr->size += 4; //version and flags

	ptr->size += 4; //sample count
	for (i = 0; i < sample_count; i++) {
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
		if (sai->isNotProtected)
			continue;
		ptr->size += sai->cenc_data_size;
	}
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *adkm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeDRMKeyManagementSystemBox, GF_ISOM_BOX_TYPE_ADKM);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void adkm_box_del(GF_Box *s)
{
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	if (!ptr) return;
	gf_free(s);
}

GF_Err adkm_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_AHDR:
		BOX_FIELD_ASSIGN(header, GF_AdobeDRMHeaderBox)
		break;
	case GF_ISOM_BOX_TYPE_ADAF:
		BOX_FIELD_ASSIGN(au_format, GF_AdobeDRMAUFormatBox)
		break;
	}
	return GF_OK;
}

GF_Err adkm_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err adkm_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err adkm_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_AdobeDRMKeyManagementSystemBox *ptr = (GF_AdobeDRMKeyManagementSystemBox *)s;
	gf_isom_check_position(s, (GF_Box *)ptr->header, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->au_format, &pos);
    return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *ahdr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeDRMHeaderBox, GF_ISOM_BOX_TYPE_AHDR);
	tmp->version = 2;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void ahdr_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err ahdr_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_AdobeDRMHeaderBox *ptr = (GF_AdobeDRMHeaderBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_APRM:
		BOX_FIELD_ASSIGN(std_enc_params, GF_AdobeStdEncryptionParamsBox)
		break;
	}
	return GF_OK;
}

GF_Err ahdr_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ahdr_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err ahdr_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_AdobeDRMHeaderBox *ptr = (GF_AdobeDRMHeaderBox *)s;
	gf_isom_check_position(s, (GF_Box *)ptr->std_enc_params, &pos);
    return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *aprm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeStdEncryptionParamsBox, GF_ISOM_BOX_TYPE_APRM);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void aprm_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err aprm_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_AdobeStdEncryptionParamsBox *ptr = (GF_AdobeStdEncryptionParamsBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_AEIB:
		BOX_FIELD_ASSIGN(enc_info, GF_AdobeEncryptionInfoBox)
		break;
	case GF_ISOM_BOX_TYPE_AKEY:
		BOX_FIELD_ASSIGN(key_info, GF_AdobeKeyInfoBox)
		break;
	}
	return GF_OK;
}

GF_Err aprm_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err aprm_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err aprm_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_AdobeStdEncryptionParamsBox *ptr = (GF_AdobeStdEncryptionParamsBox *)s;
	gf_isom_check_position(s, (GF_Box *)ptr->enc_info, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->key_info, &pos);
    return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *aeib_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeEncryptionInfoBox, GF_ISOM_BOX_TYPE_AEIB);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void aeib_box_del(GF_Box *s)
{
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox*)s;
	if (!ptr) return;
	if (ptr->enc_algo) gf_free(ptr->enc_algo);
	gf_free(ptr);
}

GF_Err aeib_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox*)s;
	u32 len;

	if (!ptr->size) {
		//will force error exit
		ISOM_DECREASE_SIZE(ptr, 1);
	}

	len = (u32) ptr->size - 1;
	if (len) {
		ptr->enc_algo = (char *)gf_malloc(len*sizeof(char));
		if (!ptr->enc_algo) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->enc_algo, len);
	}
	ptr->key_length = gf_bs_read_u8(bs);
	ptr->size = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err aeib_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err aeib_box_size(GF_Box *s)
{
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox*)s;
	if (ptr->enc_algo)
		ptr->size += strlen(ptr->enc_algo) + 1;
	ptr->size += 1; //KeyLength
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *akey_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeKeyInfoBox, GF_ISOM_BOX_TYPE_AKEY);
	tmp->version = 1;
	tmp->flags = 0;
	return (GF_Box *)tmp;
}

void akey_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err akey_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_AdobeKeyInfoBox *ptr = (GF_AdobeKeyInfoBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_FLXS:
		BOX_FIELD_ASSIGN(params, GF_AdobeFlashAccessParamsBox)
		break;
	}
	return GF_OK;
}

GF_Err akey_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err akey_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err akey_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_AdobeKeyInfoBox *ptr = (GF_AdobeKeyInfoBox *)s;
	gf_isom_check_position(s, (GF_Box *)ptr->params, &pos);
    return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *flxs_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeFlashAccessParamsBox, GF_ISOM_BOX_TYPE_FLXS);
	return (GF_Box *)tmp;
}

void flxs_box_del(GF_Box *s)
{
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox*)s;
	if (!ptr) return;
	if (ptr->metadata)
		gf_free(ptr->metadata);
	gf_free(ptr);
}

GF_Err flxs_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox*)s;
	u32 len;

	len = (u32) ptr->size;
	if (len) {
		ptr->metadata = (char *)gf_malloc(len*sizeof(char));
		if (!ptr->metadata) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->metadata, len);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err flxs_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err flxs_box_size(GF_Box *s)
{
	GF_AdobeFlashAccessParamsBox *ptr = (GF_AdobeFlashAccessParamsBox*)s;
	if (ptr->metadata)
		ptr->size += strlen(ptr->metadata) + 1;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *adaf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AdobeDRMAUFormatBox, GF_ISOM_BOX_TYPE_ADAF);
	return (GF_Box *)tmp;
}

void adaf_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err adaf_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AdobeDRMAUFormatBox *ptr = (GF_AdobeDRMAUFormatBox*)s;

	ISOM_DECREASE_SIZE(ptr, 3);
	ptr->selective_enc = gf_bs_read_u8(bs);
	gf_bs_read_u8(bs);//resersed
	ptr->IV_length = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err adaf_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err adaf_box_size(GF_Box *s)
{
	GF_AdobeDRMAUFormatBox *ptr = (GF_AdobeDRMAUFormatBox*)s;
	ptr->size += 3;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


#endif /*GPAC_DISABLE_ISOM*/
