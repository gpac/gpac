/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato / Jean Le Feuvre 2005
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

GF_ISMASample *gf_isom_ismacryp_new_sample()
{
	GF_ISMASample *tmp = (GF_ISMASample *) malloc(sizeof(GF_ISMASample));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_ISMASample));
	return tmp;
}
GF_EXPORT
void gf_isom_ismacryp_delete_sample(GF_ISMASample *samp)
{
	if (!samp) return;
	if (samp->data && samp->dataLength) free(samp->data);
	if (samp->key_indicator) free(samp->key_indicator);
	free(samp);
}


GF_ISMASample *gf_isom_ismacryp_sample_from_data(char *data, u32 dataLength, Bool use_selective_encryption, u8 KI_length, u8 IV_length)
{
	GF_ISMASample *s;
	GF_BitStream *bs;
	/*empty text sample*/
	if (!data || !dataLength) {
		return gf_isom_ismacryp_new_sample();
	}
	
	s = gf_isom_ismacryp_new_sample();
		
	/*empty sample*/
	if (!data || !dataLength) return s;

	bs = gf_bs_new(data, dataLength, GF_BITSTREAM_READ);

	s->dataLength = dataLength;
	s->IV_length = IV_length;
	s->KI_length = KI_length;

	if (use_selective_encryption) {
		s->flags = GF_ISOM_ISMA_USE_SEL_ENC;
		if (s->dataLength < 1) goto exit;
		if (gf_bs_read_int(bs, 1)) s->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		gf_bs_read_int(bs, 7);
		s->dataLength -= 1;
	} else {
		s->flags = GF_ISOM_ISMA_IS_ENCRYPTED;
	}
	if (s->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
		if (IV_length != 0) {
			if (s->dataLength < IV_length) goto exit;
			s->IV = gf_bs_read_long_int(bs, 8*IV_length);
			s->dataLength -= IV_length;
		}
		if (KI_length) {
			if (s->dataLength < KI_length) goto exit;
			s->key_indicator = (u8 *)malloc(KI_length);
			gf_bs_read_data(bs, (char*)s->key_indicator, KI_length);
			s->dataLength -= KI_length;
		}
	}
	s->data = (char*)malloc(sizeof(char)*s->dataLength);
	gf_bs_read_data(bs, s->data, s->dataLength);
	gf_bs_del(bs);
	return s;

exit:
	gf_isom_ismacryp_delete_sample(s);
	return NULL;
}

GF_Err gf_isom_ismacryp_sample_to_sample(GF_ISMASample *s, GF_ISOSample *dest)
{
	GF_BitStream *bs;
	if (!s || !dest) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (s->flags & GF_ISOM_ISMA_USE_SEL_ENC) {
		gf_bs_write_int(bs, (s->flags & GF_ISOM_ISMA_IS_ENCRYPTED) ? 1 : 0, 1);
		gf_bs_write_int(bs, 0, 7);
	} 
	if (s->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
		if (s->IV_length) gf_bs_write_long_int(bs, (s64) s->IV, 8*s->IV_length);
		if (s->KI_length) gf_bs_write_data(bs, (char*)s->key_indicator, s->KI_length);
	}
	gf_bs_write_data(bs, s->data, s->dataLength);
	if (dest->data) free(dest->data);
	dest->data = NULL;
	dest->dataLength = 0;
	gf_bs_get_content(bs, &dest->data, &dest->dataLength);
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_ISMASample *gf_isom_get_ismacryp_sample(GF_ISOFile *the_file, u32 trackNumber, GF_ISOSample *samp, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ISMASampleFormatBox *fmt;
	GF_SampleEntryBox *sea;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea || !sea->protection_info 
		|| !sea->protection_info->scheme_type 
		|| !sea->protection_info->info
		) {
		return NULL;
	}
	/*ISMA*/
	if (sea->protection_info->scheme_type->scheme_type == GF_ISOM_ISMACRYP_SCHEME) {
		fmt = sea->protection_info->info->isfm;
		if (!fmt) return NULL;
		return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, sea->protection_info->info->isfm->selective_encryption, sea->protection_info->info->isfm->key_indicator_length, sea->protection_info->info->isfm->IV_length);
	}
	/*OMA*/
	else if (sea->protection_info->scheme_type->scheme_type == GF_4CC('o','d','k','m') ) {
		if (!sea->protection_info->info->okms) return NULL;
		fmt = sea->protection_info->info->okms->fmt;

		if (fmt) {
			return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, fmt->selective_encryption, fmt->key_indicator_length, fmt->IV_length);
		}
		/*OMA default: no selective encryption, one key, 128 bit IV*/
		return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, 0, 0, 128);
	}
	return NULL;
}


GF_EXPORT
u32 gf_isom_is_media_encrypted(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *sea;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea || !sea->protection_info || !sea->protection_info->scheme_type) return 0;
	return sea->protection_info->scheme_type->scheme_type;
}

GF_EXPORT
Bool gf_isom_is_ismacryp_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *sea;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea 
		|| !sea->protection_info 
		|| !sea->protection_info->scheme_type 
		|| (sea->protection_info->scheme_type->scheme_type != GF_ISOM_ISMACRYP_SCHEME)
		|| !sea->protection_info->info
		|| !sea->protection_info->info->ikms
		|| !sea->protection_info->info->isfm
		) 
		return 0;

	return 1;
}

GF_EXPORT
Bool gf_isom_is_omadrm_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *sea;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea 
		|| !sea->protection_info 
		|| !sea->protection_info->scheme_type 
		|| (sea->protection_info->scheme_type->scheme_type != GF_4CC('o','d','k','m') )
		|| !sea->protection_info->info
		|| !sea->protection_info->info->okms
		|| !sea->protection_info->info->okms->hdr
		) 
		return 0;

	return 1;
}

/*retrieves ISMACryp info for the given track & SDI*/
GF_EXPORT
GF_Err gf_isom_get_ismacryp_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outSchemeURI, const char **outKMS_URI, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *sea;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea || !sea->protection_info) return GF_BAD_PARAM;
	if (!sea->protection_info->scheme_type || !sea->protection_info->original_format) return GF_NON_COMPLIANT_BITSTREAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sea->protection_info->original_format->data_format;
		if (IsMP4Description(sea->protection_info->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sea->protection_info->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sea->protection_info->scheme_type->scheme_version;
	if (outSchemeURI) *outSchemeURI = sea->protection_info->scheme_type->URI;

	if (sea->protection_info->info && sea->protection_info->info->ikms) {
		if (outKMS_URI) *outKMS_URI = sea->protection_info->info->ikms->URI;
	} else {
		if (outKMS_URI) *outKMS_URI = NULL;
	}
	if (sea->protection_info->info && sea->protection_info->info->isfm) {
		if (outSelectiveEncryption) *outSelectiveEncryption = sea->protection_info->info->isfm->selective_encryption;
		if (outIVLength) *outIVLength = sea->protection_info->info->isfm->IV_length;
		if (outKeyIndicationLength) *outKeyIndicationLength = sea->protection_info->info->isfm->key_indicator_length;
	} else {
		if (outSelectiveEncryption) *outSelectiveEncryption = 0;
		if (outIVLength) *outIVLength = 0;
		if (outKeyIndicationLength) *outKeyIndicationLength = 0;
	}
	return GF_OK;
}


/*retrieves ISMACryp info for the given track & SDI*/
GF_EXPORT
GF_Err gf_isom_get_omadrm_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat,
							   u32 *outSchemeType, u32 *outSchemeVersion,
							   const char **outContentID, const char **outRightsIssuerURL, const char **outTextualHeaders, u32 *outTextualHeadersLen, u64 *outPlaintextLength, u32 *outEncryptionType, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *sea;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea || !sea->protection_info) return GF_BAD_PARAM;
	if (!sea->protection_info->scheme_type || !sea->protection_info->original_format) return GF_NON_COMPLIANT_BITSTREAM;
	if (!sea->protection_info->info || !sea->protection_info->info->okms || !sea->protection_info->info->okms->hdr) return GF_NON_COMPLIANT_BITSTREAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sea->protection_info->original_format->data_format;
		if (IsMP4Description(sea->protection_info->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sea->protection_info->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sea->protection_info->scheme_type->scheme_version;
	if (outContentID) *outContentID = sea->protection_info->info->okms->hdr->ContentID;
	if (outRightsIssuerURL) *outRightsIssuerURL = sea->protection_info->info->okms->hdr->RightsIssuerURL;
	if (outTextualHeaders) {
		*outTextualHeaders = sea->protection_info->info->okms->hdr->TextualHeaders;
		if (outTextualHeadersLen) *outTextualHeadersLen = sea->protection_info->info->okms->hdr->TextualHeadersLen;
	}
	if (outPlaintextLength) *outPlaintextLength = sea->protection_info->info->okms->hdr->PlaintextLength;
	if (outEncryptionType) *outEncryptionType = sea->protection_info->info->okms->hdr->EncryptionMethod;

	if (sea->protection_info->info && sea->protection_info->info->okms  && sea->protection_info->info->okms->fmt) {
		if (outSelectiveEncryption) *outSelectiveEncryption = sea->protection_info->info->okms->fmt->selective_encryption;
		if (outIVLength) *outIVLength = sea->protection_info->info->okms->fmt->IV_length;
		if (outKeyIndicationLength) *outKeyIndicationLength = sea->protection_info->info->okms->fmt->key_indicator_length;
	} else {
		if (outSelectiveEncryption) *outSelectiveEncryption = 0;
		if (outIVLength) *outIVLength = 0;
		if (outKeyIndicationLength) *outKeyIndicationLength = 0;
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY

GF_Err gf_isom_remove_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *sea;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !StreamDescriptionIndex) return GF_BAD_PARAM;

	Media_GetSampleDesc(trak->Media, StreamDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea || !sea->protection_info) return GF_BAD_PARAM;
	if (!sea->protection_info->scheme_type || !sea->protection_info->original_format) return GF_NON_COMPLIANT_BITSTREAM;

	sea->type = sea->protection_info->original_format->data_format;
	gf_isom_box_del((GF_Box *)sea->protection_info);
	sea->protection_info = NULL;
	if (sea->type == GF_4CC('2','6','4','b')) sea->type = GF_ISOM_BOX_TYPE_AVC1;
	return GF_OK;
}

GF_Err gf_isom_change_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, char *scheme_uri, char *kms_uri)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *sea;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !StreamDescriptionIndex) return GF_BAD_PARAM;

	Media_GetSampleDesc(trak->Media, StreamDescriptionIndex, &sea, NULL);
	/*non-encrypted or non-ISMA*/
	if (!sea || !sea->protection_info) return GF_BAD_PARAM;
	if (!sea->protection_info->scheme_type || !sea->protection_info->original_format) return GF_NON_COMPLIANT_BITSTREAM;

	if (scheme_uri) {
		free(sea->protection_info->scheme_type->URI);
		sea->protection_info->scheme_type->URI = strdup(scheme_uri);
	}
	if (kms_uri) {
		free(sea->protection_info->info->ikms->URI);
		sea->protection_info->info->ikms->URI = strdup(kms_uri);
	}
	return GF_OK;
}


GF_Err gf_isom_set_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, 
						   u32 scheme_version, char *scheme_uri, char *kms_URI,
						   Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	u32 original_format;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, desc_index, &sea, NULL);
	if (e) return e;

	/* Replacing the Media Type */
	switch (sea->type) {
	case GF_ISOM_BOX_TYPE_MP4A:
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCA;
		break;
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_BOX_TYPE_D263:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	/*special case for AVC1*/
	case GF_ISOM_BOX_TYPE_AVC1:
		original_format = GF_4CC('2','6','4','b');
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCS;
		break;
	default:
		return GF_BAD_PARAM;
	}
	
	sea->protection_info = (GF_ProtectionInfoBox *)sinf_New();
	sea->protection_info->scheme_type = (GF_SchemeTypeBox *)schm_New();
	sea->protection_info->scheme_type->scheme_type = scheme_type;
	sea->protection_info->scheme_type->scheme_version = scheme_version;
	if (scheme_uri) {
		sea->protection_info->scheme_type->flags |= 0x000001;
		sea->protection_info->scheme_type->URI = strdup(scheme_uri);
	}
	sea->protection_info->original_format = (GF_OriginalFormatBox *)frma_New();
	sea->protection_info->original_format->data_format = original_format;
	sea->protection_info->info = (GF_SchemeInformationBox *)schi_New();

	sea->protection_info->info->ikms = (GF_ISMAKMSBox *)iKMS_New();
	sea->protection_info->info->ikms->URI = strdup(kms_URI);

	sea->protection_info->info->isfm = (GF_ISMASampleFormatBox *)iSFM_New();
	sea->protection_info->info->isfm->selective_encryption = selective_encryption;
	sea->protection_info->info->isfm->key_indicator_length = KI_length;
	sea->protection_info->info->isfm->IV_length = IV_length;
	return GF_OK;
}

GF_Err gf_isom_set_oma_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index,
						   char *contentID, char *kms_URI, u32 encryption_type, u64 plainTextLength, char *textual_headers, u32 textual_headers_len,
						   Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	u32 original_format;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, desc_index, &sea, NULL);
	if (e) return e;

	/* Replacing the Media Type */
	switch (sea->type) {
	case GF_ISOM_BOX_TYPE_MP4A:
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCA;
		break;
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_D263:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCS;
		break;
	default:
		return GF_BAD_PARAM;
	}
	
	sea->protection_info = (GF_ProtectionInfoBox *)sinf_New();
	sea->protection_info->scheme_type = (GF_SchemeTypeBox *)schm_New();
	sea->protection_info->scheme_type->scheme_type = GF_4CC('o','d','k','m');
	sea->protection_info->scheme_type->scheme_version = 0x00000200;

	sea->protection_info->original_format = (GF_OriginalFormatBox *)frma_New();
	sea->protection_info->original_format->data_format = original_format;
	sea->protection_info->info = (GF_SchemeInformationBox *)schi_New();

	sea->protection_info->info->okms = (GF_OMADRMKMSBox *)odkm_New();
	sea->protection_info->info->okms->fmt = (GF_OMADRMAUFormatBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_ODAF);
	sea->protection_info->info->okms->fmt->selective_encryption = selective_encryption;
	sea->protection_info->info->okms->fmt->key_indicator_length = KI_length;
	sea->protection_info->info->okms->fmt->IV_length = IV_length;

	sea->protection_info->info->okms->hdr = (GF_OMADRMCommonHeaderBox*)ohdr_New();
	sea->protection_info->info->okms->hdr->EncryptionMethod = encryption_type;
	sea->protection_info->info->okms->hdr->PaddingScheme = (encryption_type==0x01) ? 1 : 0;
	sea->protection_info->info->okms->hdr->PlaintextLength = plainTextLength;
	if (contentID) sea->protection_info->info->okms->hdr->ContentID = strdup(contentID);
	if (kms_URI) sea->protection_info->info->okms->hdr->RightsIssuerURL = strdup(kms_URI);
	if (textual_headers) {
		sea->protection_info->info->okms->hdr->TextualHeaders = malloc(sizeof(char)*textual_headers_len);
		memcpy(sea->protection_info->info->okms->hdr->TextualHeaders, textual_headers, sizeof(char)*textual_headers_len);
		sea->protection_info->info->okms->hdr->TextualHeadersLen = textual_headers_len;
	}
	return GF_OK;
}

#endif
