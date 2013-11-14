/*
 *			GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato / Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

GF_ISMASample *gf_isom_ismacryp_new_sample()
{
	GF_ISMASample *tmp = (GF_ISMASample *) gf_malloc(sizeof(GF_ISMASample));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_ISMASample));
	return tmp;
}
GF_EXPORT
void gf_isom_ismacryp_delete_sample(GF_ISMASample *samp)
{
	if (!samp) return;
	if (samp->data && samp->dataLength) gf_free(samp->data);
	if (samp->key_indicator) gf_free(samp->key_indicator);
	gf_free(samp);
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
			s->key_indicator = (u8 *)gf_malloc(KI_length);
			gf_bs_read_data(bs, (char*)s->key_indicator, KI_length);
			s->dataLength -= KI_length;
		}
	}
	s->data = (char*)gf_malloc(sizeof(char)*s->dataLength);
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
	if (dest->data) gf_free(dest->data);
	dest->data = NULL;
	dest->dataLength = 0;
	gf_bs_get_content(bs, &dest->data, &dest->dataLength);
	gf_bs_del(bs);
	return GF_OK;
}

static GF_ProtectionInfoBox *gf_isom_get_sinf_entry(GF_TrackBox *trak, u32 sampleDescriptionIndex, u32 scheme_type, GF_SampleEntryBox **out_sea)
{
	u32 i=0;
	GF_SampleEntryBox *sea;
	GF_ProtectionInfoBox *sinf;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	if (!sea) return NULL;

	i = 0;
	while ((sinf = gf_list_enum(sea->protections, &i))) {
		if (sinf->original_format && sinf->scheme_type && sinf->info) {
			if (!scheme_type || (sinf->scheme_type->scheme_type == scheme_type)) {
				if (out_sea)
					*out_sea = sea;
				return sinf;
			}
		}
	}
	return NULL;
}

GF_EXPORT
GF_ISMASample *gf_isom_get_ismacryp_sample(GF_ISOFile *the_file, u32 trackNumber, GF_ISOSample *samp, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ISMASampleFormatBox *fmt;
	GF_ProtectionInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, 0, NULL);
	if (!sinf) return NULL;

	/*ISMA*/
	if (sinf->scheme_type->scheme_type == GF_ISOM_ISMACRYP_SCHEME) {
		fmt = sinf->info->isfm;
		if (!fmt) return NULL;
		return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, sinf->info->isfm->selective_encryption, sinf->info->isfm->key_indicator_length, sinf->info->isfm->IV_length);
	}
	/*OMA*/
	else if (sinf->scheme_type->scheme_type == GF_4CC('o','d','k','m') ) {
		if (!sinf->info->okms) return NULL;
		fmt = sinf->info->okms->fmt;

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
	GF_ProtectionInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, 0, NULL);
	if (!sinf) return 0;

	/*non-encrypted or non-ISMA*/
	if (!sinf || !sinf->scheme_type) return 0;
	return sinf->scheme_type->scheme_type;
}

GF_EXPORT
Bool gf_isom_is_ismacryp_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, NULL);
	if (!sinf) return 0;

	/*non-encrypted or non-ISMA*/
	if (!sinf->info || !sinf->info->ikms || !sinf->info->isfm ) 
		return 0;

	return 1;
}

GF_EXPORT
Bool gf_isom_is_omadrm_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_OMADRM_SCHEME, NULL);
	if (!sinf) return 0;

	/*non-encrypted or non-OMA*/
	if (!sinf->info || !sinf->info->okms || !sinf->info->okms->hdr) 
		return 0;

	return 1;
}

/*retrieves ISMACryp info for the given track & SDI*/
GF_EXPORT
GF_Err gf_isom_get_ismacryp_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outSchemeURI, const char **outKMS_URI, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength)
{
	GF_TrackBox *trak;
	GF_ProtectionInfoBox *sinf;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, NULL);
	if (!sinf) return 0;

	if (outOriginalFormat) {
		*outOriginalFormat = sinf->original_format->data_format;
		if (IsMP4Description(sinf->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sinf->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sinf->scheme_type->scheme_version;
	if (outSchemeURI) *outSchemeURI = sinf->scheme_type->URI;

	if (sinf->info && sinf->info->ikms) {
		if (outKMS_URI) *outKMS_URI = sinf->info->ikms->URI;
	} else {
		if (outKMS_URI) *outKMS_URI = NULL;
	}
	if (sinf->info && sinf->info->isfm) {
		if (outSelectiveEncryption) *outSelectiveEncryption = sinf->info->isfm->selective_encryption;
		if (outIVLength) *outIVLength = sinf->info->isfm->IV_length;
		if (outKeyIndicationLength) *outKeyIndicationLength = sinf->info->isfm->key_indicator_length;
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
	GF_ProtectionInfoBox *sinf;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_OMADRM_SCHEME, NULL);
	if (!sinf) return 0;

	if (!sinf->info || !sinf->info->okms || !sinf->info->okms->hdr) return GF_NON_COMPLIANT_BITSTREAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sinf->original_format->data_format;
		if (IsMP4Description(sinf->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sinf->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sinf->scheme_type->scheme_version;
	if (outContentID) *outContentID = sinf->info->okms->hdr->ContentID;
	if (outRightsIssuerURL) *outRightsIssuerURL = sinf->info->okms->hdr->RightsIssuerURL;
	if (outTextualHeaders) {
		*outTextualHeaders = sinf->info->okms->hdr->TextualHeaders;
		if (outTextualHeadersLen) *outTextualHeadersLen = sinf->info->okms->hdr->TextualHeadersLen;
	}
	if (outPlaintextLength) *outPlaintextLength = sinf->info->okms->hdr->PlaintextLength;
	if (outEncryptionType) *outEncryptionType = sinf->info->okms->hdr->EncryptionMethod;

	if (sinf->info && sinf->info->okms && sinf->info->okms->fmt) {
		if (outSelectiveEncryption) *outSelectiveEncryption = sinf->info->okms->fmt->selective_encryption;
		if (outIVLength) *outIVLength = sinf->info->okms->fmt->IV_length;
		if (outKeyIndicationLength) *outKeyIndicationLength = sinf->info->okms->fmt->key_indicator_length;
	} else {
		if (outSelectiveEncryption) *outSelectiveEncryption = 0;
		if (outIVLength) *outIVLength = 0;
		if (outKeyIndicationLength) *outKeyIndicationLength = 0;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_remove_track_protection(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionInfoBox *sinf;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !sampleDescriptionIndex) return GF_BAD_PARAM;

	sea = NULL;
	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, &sea);
	if (!sinf) sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, &sea);
	if (!sinf) sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, &sea);
	if (!sinf) sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_OMADRM_SCHEME, &sea);
	if (!sinf) return GF_OK;

	sea->type = sinf->original_format->data_format;
	gf_isom_box_array_del(sea->protections);
	sea->protections = gf_list_new();
	if (sea->type == GF_4CC('2','6','4','b')) sea->type = GF_ISOM_BOX_TYPE_AVC1;
	if (sea->type == GF_4CC('2','6','5','b')) sea->type = GF_ISOM_BOX_TYPE_HVC1;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_change_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, char *scheme_uri, char *kms_uri)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionInfoBox *sinf;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !sampleDescriptionIndex) return GF_BAD_PARAM;

	sea = NULL;
	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, &sea);
	if (!sinf) return 0;

	if (scheme_uri) {
		gf_free(sinf->scheme_type->URI);
		sinf->scheme_type->URI = gf_strdup(scheme_uri);
	}
	if (kms_uri) {
		gf_free(sinf->info->ikms->URI);
		sinf->info->ikms->URI = gf_strdup(kms_uri);
	}
	return GF_OK;
}


static GF_Err gf_isom_set_protected_entry(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, u32 scheme_version, Bool is_isma, GF_ProtectionInfoBox **out_sinf)
{
	u32 original_format;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionInfoBox *sinf;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, desc_index, &sea, NULL);
	if (e) return e;

	original_format = sea->type;

	/* Replacing the Media Type */
	switch (sea->type) {
	case GF_ISOM_BOX_TYPE_MP4A:
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
	case GF_ISOM_BOX_TYPE_AC3:
		sea->type = GF_ISOM_BOX_TYPE_ENCA;
		break;
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_BOX_TYPE_D263:
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	/*special case for AVC1*/
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3: 
	case GF_ISOM_BOX_TYPE_AVC4: 
	case GF_ISOM_BOX_TYPE_SVC1:
		if (is_isma) 
			original_format = GF_4CC('2','6','4','b');
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_SHC1:
	case GF_ISOM_BOX_TYPE_SHV1:
		if (is_isma) 
			original_format = GF_4CC('2','6','5','b');
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
	case GF_ISOM_BOX_TYPE_LSR1:
		sea->type = GF_ISOM_BOX_TYPE_ENCS;
		break;
	case GF_ISOM_BOX_TYPE_STSE:
	case GF_ISOM_BOX_TYPE_WVTT:
		sea->type = GF_ISOM_BOX_TYPE_ENCT;
		break;
	default:
		return GF_BAD_PARAM;
	}
	
	sinf = (GF_ProtectionInfoBox *)sinf_New();
	gf_list_add(sea->protections, sinf);

	sinf->scheme_type = (GF_SchemeTypeBox *)schm_New();
	sinf->scheme_type->scheme_type = scheme_type;
	sinf->scheme_type->scheme_version = scheme_version;

	sinf->original_format = (GF_OriginalFormatBox *)frma_New();
	sinf->original_format->data_format = original_format;
	
	//common to isma, cenc and oma
	sinf->info = (GF_SchemeInformationBox *)schi_New();

	*out_sinf = sinf;
	return GF_OK;
}

GF_Err gf_isom_set_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, 
						   u32 scheme_version, char *scheme_uri, char *kms_URI,
						   Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	GF_Err e;
	GF_ProtectionInfoBox *sinf;

	//setup generic protection
	e = gf_isom_set_protected_entry(the_file, trackNumber, desc_index, scheme_type, scheme_version, GF_TRUE, &sinf);
	if (e) return e;

	if (scheme_uri) {
		sinf->scheme_type->flags |= 0x000001;
		sinf->scheme_type->URI = gf_strdup(scheme_uri);
	}

	sinf->info->ikms = (GF_ISMAKMSBox *)iKMS_New();
	sinf->info->ikms->URI = gf_strdup(kms_URI);

	sinf->info->isfm = (GF_ISMASampleFormatBox *)iSFM_New();
	sinf->info->isfm->selective_encryption = selective_encryption;
	sinf->info->isfm->key_indicator_length = KI_length;
	sinf->info->isfm->IV_length = IV_length;
	return GF_OK;
}

GF_Err gf_isom_set_oma_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index,
						   char *contentID, char *kms_URI, u32 encryption_type, u64 plainTextLength, char *textual_headers, u32 textual_headers_len,
						   Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	GF_ProtectionInfoBox *sinf;
	GF_Err e;

	//setup generic protection
	e = gf_isom_set_protected_entry(the_file, trackNumber, desc_index, GF_ISOM_OMADRM_SCHEME, 0x00000200, GF_FALSE, &sinf);
	if (e) return e;

	sinf->info->okms = (GF_OMADRMKMSBox *)odkm_New();
	sinf->info->okms->fmt = (GF_OMADRMAUFormatBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_ODAF);
	sinf->info->okms->fmt->selective_encryption = selective_encryption;
	sinf->info->okms->fmt->key_indicator_length = KI_length;
	sinf->info->okms->fmt->IV_length = IV_length;

	sinf->info->okms->hdr = (GF_OMADRMCommonHeaderBox*)ohdr_New();
	sinf->info->okms->hdr->EncryptionMethod = encryption_type;
	sinf->info->okms->hdr->PaddingScheme = (encryption_type==0x01) ? 1 : 0;
	sinf->info->okms->hdr->PlaintextLength = plainTextLength;
	if (contentID) sinf->info->okms->hdr->ContentID = gf_strdup(contentID);
	if (kms_URI) sinf->info->okms->hdr->RightsIssuerURL = gf_strdup(kms_URI);
	if (textual_headers) {
		sinf->info->okms->hdr->TextualHeaders = gf_malloc(sizeof(char)*textual_headers_len);
		memcpy(sinf->info->okms->hdr->TextualHeaders, textual_headers, sizeof(char)*textual_headers_len);
		sinf->info->okms->hdr->TextualHeadersLen = textual_headers_len;
	}
	return GF_OK;
}

/* Common Encryption*/
GF_EXPORT
Bool gf_isom_is_cenc_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, NULL);
	if (!sinf) sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, NULL);

	if (!sinf) return GF_FALSE;

	/*non-encrypted or non-CENC*/
	if (!sinf->info || !sinf->info->tenc) 
		return GF_FALSE;

	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_isom_get_cenc_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, u32 *outIVLength)
{
	GF_TrackBox *trak;
	GF_ProtectionInfoBox *sinf;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, NULL);
	if (!sinf) sinf = gf_isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, NULL);

	if (!sinf) return GF_BAD_PARAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sinf->original_format->data_format;
		if (IsMP4Description(sinf->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sinf->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sinf->scheme_type->scheme_version;

	if (outIVLength) {
		if (sinf->info && sinf->info->tenc)
			*outIVLength = sinf->info->tenc->IV_size;
		else
			*outIVLength = 0;
	}

	return GF_OK;
}

GF_Err gf_isom_set_cenc_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, 
						   u32 scheme_version, u32 default_IsEncrypted, u8 default_IV_size,	bin128 default_KID)
{
	GF_Err e;
	GF_ProtectionInfoBox *sinf;

	//setup generic protection
	e = gf_isom_set_protected_entry(the_file, trackNumber, desc_index, scheme_type, scheme_version, GF_FALSE, &sinf);
	if (e) return e;

	sinf->info->tenc = (GF_TrackEncryptionBox *)tenc_New();
	sinf->info->tenc->IsEncrypted = default_IsEncrypted;
	sinf->info->tenc->IV_size = default_IV_size;
	memcpy(sinf->info->tenc->KID, default_KID, 16*sizeof(char));

	return GF_OK;
}

#if 0

GF_Err gf_isom_set_cenc_saio(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_SampleTableBox *stbl;
	GF_SampleAuxiliaryInfoOffsetBox *saio;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return GF_BAD_PARAM;

	saio = (GF_SampleAuxiliaryInfoOffsetBox *)saio_New();
	saio->aux_info_type = GF_4CC('c', 'e', 'n', 'c');
	saio->aux_info_type_parameter = 0;

	if (!stbl->sai_offsets) stbl->sai_offsets = gf_list_new();
	gf_list_add(stbl->sai_offsets, saio);

	return GF_OK;
}
#endif

GF_Err gf_isom_remove_cenc_saiz(GF_ISOFile *the_file, u32 trackNumber)
{
	u32 i;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return GF_BAD_PARAM;

	for (i = 0; i < gf_list_count(stbl->sai_sizes); i++) {
		GF_SampleAuxiliaryInfoSizeBox *saiz = (GF_SampleAuxiliaryInfoSizeBox *)gf_list_get(stbl->sai_sizes, i);
		if (saiz->aux_info_type != GF_4CC('c', 'e', 'n', 'c'))
			continue;
		saiz_del((GF_Box *)saiz);
		gf_list_rem(stbl->sai_sizes, i);
		i--;
	}

	if (!gf_list_count(stbl->sai_sizes)) {
		gf_list_del(stbl->sai_sizes);
		stbl->sai_sizes = NULL;
	}

	return GF_OK;
}

GF_Err gf_isom_remove_cenc_saio(GF_ISOFile *the_file, u32 trackNumber)
{
	u32 i;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return GF_BAD_PARAM;

	for (i = 0; i < gf_list_count(stbl->sai_offsets); i++) {
		GF_SampleAuxiliaryInfoOffsetBox *saio = (GF_SampleAuxiliaryInfoOffsetBox *)gf_list_get(stbl->sai_offsets, i);
		if (saio->aux_info_type != GF_4CC('c', 'e', 'n', 'c'))
			continue;
		saiz_del((GF_Box *)saio);
		gf_list_rem(stbl->sai_offsets, i);
		i--;
	}

	if (!gf_list_count(stbl->sai_offsets)) {
		gf_list_del(stbl->sai_offsets);
		stbl->sai_offsets = NULL;
	}

	return GF_OK;
}

GF_Err gf_cenc_set_pssh(GF_ISOFile *mp4, bin128 systemID, u32 version, u32 KID_count, bin128 *KIDs, char *data, u32 len) {
	GF_ProtectionSystemHeaderBox *pssh;

	pssh = (GF_ProtectionSystemHeaderBox *)pssh_New();
	if (!pssh)
		return GF_IO_ERR;
	memmove((char *)pssh->SystemID, systemID, 16);
	pssh->version = version;
	if (version) {
		pssh->KID_count = KID_count;
		if (KID_count) {
			if (!pssh->KIDs) pssh->KIDs = (bin128 *)gf_malloc(pssh->KID_count*sizeof(bin128));
			memmove(pssh->KIDs, KIDs, pssh->KID_count*sizeof(bin128));
		}
	}
	pssh->private_data_size = len;
	if (!pssh->private_data)
		pssh->private_data = (u8 *)gf_malloc(pssh->private_data_size*sizeof(char));
	memmove((char *)pssh->private_data, data, pssh->private_data_size);

	if (!mp4->moov->other_boxes) mp4->moov->other_boxes = gf_list_new();
	gf_list_add(mp4->moov->other_boxes, pssh);

	return GF_OK;
}



GF_Err gf_isom_remove_samp_enc_box(GF_ISOFile *the_file, u32 trackNumber)
{
	u32 i;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return GF_BAD_PARAM;

	for (i = 0; i < gf_list_count(stbl->other_boxes); i++) {
		GF_Box *a = (GF_Box *)gf_list_get(stbl->other_boxes, i);
		if ((a->type ==GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
			piff_psec_del(a);
			gf_list_rem(stbl->other_boxes, i);
			i--;
		}
		else if (a->type == GF_ISOM_BOX_TYPE_SENC){
			senc_del(a);
			gf_list_rem(stbl->other_boxes, i);
			i--;
		}
	}

	if (!gf_list_count(stbl->other_boxes)) {
		gf_list_del(stbl->other_boxes);
		stbl->other_boxes = NULL;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_PIFFSampleEncryptionBox * gf_isom_create_piff_psec_box(u8 version, u32 flags, u32 AlgorithmID, u8 IV_size, bin128 KID) 
{
	GF_PIFFSampleEncryptionBox *psec;

	psec = (GF_PIFFSampleEncryptionBox *) piff_psec_New();
	if (!psec)
		return NULL;
	psec->version = version;
	psec->flags = flags;
	if (psec->flags & 0x1) {
		psec->AlgorithmID = AlgorithmID;
		psec->IV_size = IV_size;
		strcpy((char *)psec->KID, (const char *)KID);
	}
	psec->samp_aux_info = gf_list_new();

	return psec;
}

GF_SampleEncryptionBox * gf_isom_create_samp_enc_box(u8 version, u32 flags) 
{
	GF_SampleEncryptionBox *senc;

	senc = (GF_SampleEncryptionBox *) senc_New();
	if (!senc)
		return NULL;
	senc->version = version;
	senc->flags = flags;
	senc->samp_aux_info = gf_list_new();

	return senc;
}

GF_Err gf_isom_cenc_allocate_storage(GF_ISOFile *the_file, u32 trackNumber, u32 container_type, u32 AlgorithmID, u8 IV_size, bin128 KID) 
{
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl) return GF_BAD_PARAM;
	if (!stbl) return GF_BAD_PARAM;

	switch (container_type) {
	case GF_ISOM_BOX_UUID_PSEC: 
		if (stbl->piff_psec) return GF_OK;
		stbl->piff_psec = (GF_Box *)gf_isom_create_piff_psec_box(1, 0, AlgorithmID, IV_size, KID);
		//senc will be written and destroyed with the other boxes
		return gf_isom_box_add_default((GF_Box *) stbl, stbl->piff_psec);

	case GF_ISOM_BOX_TYPE_SENC: 
		if (stbl->senc) return GF_OK;
		stbl->senc = (GF_Box *)gf_isom_create_samp_enc_box(0, 0);
		//senc will be written and destroyed with the other boxes
		return gf_isom_box_add_default((GF_Box *) stbl, stbl->senc);
	}
	return GF_NOT_SUPPORTED;
}


void gf_isom_cenc_set_saiz_saio(GF_SampleEncryptionBox *senc, GF_SampleTableBox *stbl, GF_TrackFragmentBox  *traf, u32 len)
{
	u32  i;
	if (!senc->cenc_saiz) {
		senc->cenc_saiz = (GF_SampleAuxiliaryInfoSizeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SAIZ);
		senc->cenc_saiz->aux_info_type = GF_4CC('c', 'e', 'n', 'c');
		senc->cenc_saiz->aux_info_type_parameter = 0;
		if (stbl) 
			stbl_AddBox(stbl, (GF_Box *)senc->cenc_saiz);
		else
			traf_AddBox((GF_Box*)traf, (GF_Box *)senc->cenc_saiz);
	}
	if (!senc->cenc_saio) {
		senc->cenc_saio = (GF_SampleAuxiliaryInfoOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SAIO);
		senc->cenc_saio->aux_info_type = GF_4CC('c', 'e', 'n', 'c');
		senc->cenc_saio->aux_info_type_parameter = 0;
		senc->cenc_saio->entry_count = 1;
		if (stbl) 
			stbl_AddBox(stbl, (GF_Box *)senc->cenc_saio);
		else
			traf_AddBox((GF_Box*)traf, (GF_Box *)senc->cenc_saio);
	}

	if (!senc->cenc_saiz->sample_count || (senc->cenc_saiz->default_sample_info_size==len)) {
		senc->cenc_saiz->sample_count ++;
		senc->cenc_saiz->default_sample_info_size = len;
	} else {
		senc->cenc_saiz->sample_info_size = gf_realloc(senc->cenc_saiz->sample_info_size, sizeof(u8)*(senc->cenc_saiz->sample_count+1));

		if (senc->cenc_saiz->default_sample_info_size) {
			for (i=0; i<senc->cenc_saiz->sample_count; i++)
				senc->cenc_saiz->sample_info_size[i] = senc->cenc_saiz->default_sample_info_size;
			senc->cenc_saiz->default_sample_info_size = 0;
		}
		senc->cenc_saiz->sample_info_size[senc->cenc_saiz->sample_count] = len;
		senc->cenc_saiz->sample_count++;
	}
}
GF_Err gf_isom_track_cenc_add_sample_info(GF_ISOFile *the_file, u32 trackNumber, u32 container_type, char *buf, u32 len)
{
	u32 i;
	GF_BitStream *bs;
	GF_SampleEncryptionBox *senc;
	GF_CENCSampleAuxInfo *sai;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl) return GF_BAD_PARAM;

	switch (container_type) {
	case GF_ISOM_BOX_UUID_PSEC:
		senc = (GF_SampleEncryptionBox *) stbl->piff_psec;
		break;
	case GF_ISOM_BOX_TYPE_SENC:
		senc = (GF_SampleEncryptionBox *)stbl->senc;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}


	sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
	if (!sai) return GF_OUT_OF_MEM;
	memset(sai, 0, sizeof(GF_CENCSampleAuxInfo));
	bs = gf_bs_new(buf, len, GF_BITSTREAM_READ);
	gf_bs_read_data(bs, (char *)sai->IV, 16);
	sai->subsample_count = gf_bs_read_u16(bs);
	if (sai->subsample_count) senc->flags = 0x00000002;
	sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
	for (i = 0; i < sai->subsample_count; i++) {
		sai->subsamples[i].bytes_clear_data = gf_bs_read_u32(bs);
		sai->subsamples[i].bytes_encrypted_data = gf_bs_read_u32(bs);
	}
	gf_bs_del(bs);

	gf_list_add(senc->samp_aux_info, sai);

	gf_isom_cenc_set_saiz_saio(senc, stbl, NULL, len);

	return GF_OK;
}


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void gf_isom_cenc_samp_aux_info_del(GF_CENCSampleAuxInfo *samp)
{
	//if (samp->IV) gf_free(samp->IV);
	if (samp->subsamples) gf_free(samp->subsamples);
	gf_free(samp);
}

GF_EXPORT
GF_CENCSampleAuxInfo * gf_isom_cenc_get_sample_aux_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *container_type)
{
	GF_CENCSampleAuxInfo *sai;
	GF_SampleTableBox *stbl;
	GF_Box *a_box;
	u32 i, type;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;
	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return NULL;

	type = 0;
	for (i = 0; i < gf_list_count(stbl->other_boxes); i++) {
		a_box = (GF_Box *)gf_list_get(stbl->other_boxes, i++);
		if ((a_box->type == GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)a_box)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
			type = GF_ISOM_BOX_UUID_PSEC;
			break;
		}
		else if (a_box->type == GF_ISOM_BOX_TYPE_SENC) {
			type = GF_ISOM_BOX_TYPE_SENC;
			break;
		}
	}

	sai = NULL;
	switch (type) {
		case GF_ISOM_BOX_UUID_PSEC:
			sai = (GF_CENCSampleAuxInfo *)gf_list_get(((GF_PIFFSampleEncryptionBox *)a_box)->samp_aux_info, sampleNumber-1);
			break;
		case GF_ISOM_BOX_TYPE_SENC:
			sai = (GF_CENCSampleAuxInfo *)gf_list_get(((GF_SampleEncryptionBox *)a_box)->samp_aux_info, sampleNumber-1);
			break;
	}

	if (container_type) *container_type = type;

	return sai;
}

GF_EXPORT
void gf_isom_cenc_get_KID(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, bin128 *outKID) {
	GF_TrackBox *trak;
	GF_ProtectionInfoBox *sinf;

	memset(*outKID, 0, 16);

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return;

	sinf = gf_isom_get_sinf_entry(trak, 1, GF_ISOM_CENC_SCHEME, NULL);
	if (!sinf) sinf = gf_isom_get_sinf_entry(trak, 1, GF_ISOM_CBC_SCHEME, NULL);

	if (sinf && sinf->info && sinf->info->tenc)
		memmove(*outKID, sinf->info->tenc->KID, 16);
	
}

#endif //	GPAC_DISABLE_ISOM_FRAGMENTS


void gf_isom_ipmpx_remove_tool_list(GF_ISOFile *the_file)
{
		/*remove IPMPToolList if any*/	
	if (the_file && the_file->moov && the_file->moov->iods && (the_file ->moov->iods->descriptor->tag == GF_ODF_ISOM_IOD_TAG) ) {
		GF_IsomInitialObjectDescriptor *iod = (GF_IsomInitialObjectDescriptor *)the_file ->moov->iods->descriptor;
		if (iod->IPMPToolList) gf_odf_desc_del((GF_Descriptor*) iod->IPMPToolList);
		iod->IPMPToolList = NULL;
	}
}

#endif /*GPAC_DISABLE_ISOM*/
