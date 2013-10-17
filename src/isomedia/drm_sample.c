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

GF_Err gf_isom_remove_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
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


GF_Err gf_isom_set_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, 
						   u32 scheme_version, char *scheme_uri, char *kms_URI,
						   Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	u32 original_format;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionInfoBox *sinf;
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
	case GF_ISOM_BOX_TYPE_AC3:
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
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3: 
	case GF_ISOM_BOX_TYPE_AVC4: 
	case GF_ISOM_BOX_TYPE_SVC1:
		original_format = GF_4CC('2','6','4','b');
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
		original_format = GF_4CC('2','6','5','b');
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
	case GF_ISOM_BOX_TYPE_LSR1:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCS;
		break;
	case GF_ISOM_BOX_TYPE_STSE:
	case GF_ISOM_BOX_TYPE_WVTT:
		original_format = sea->type;
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
	if (scheme_uri) {
		sinf->scheme_type->flags |= 0x000001;
		sinf->scheme_type->URI = gf_strdup(scheme_uri);
	}
	sinf->original_format = (GF_OriginalFormatBox *)frma_New();
	sinf->original_format->data_format = original_format;
	sinf->info = (GF_SchemeInformationBox *)schi_New();

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
	u32 original_format;
	GF_ProtectionInfoBox *sinf;
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
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3: 
	case GF_ISOM_BOX_TYPE_AVC4: 
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_D263:
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
	case GF_ISOM_BOX_TYPE_LSR1:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCS;
		break;
	case GF_ISOM_BOX_TYPE_STSE:
	case GF_ISOM_BOX_TYPE_WVTT:
		original_format = sea->type;
		sea->type = GF_ISOM_BOX_TYPE_ENCT;
		break;
	default:
		return GF_BAD_PARAM;
	}
	
	sinf = (GF_ProtectionInfoBox *)sinf_New();
	gf_list_add(sea->protections, sinf);
	sinf->scheme_type = (GF_SchemeTypeBox *)schm_New();
	sinf->scheme_type->scheme_type = GF_4CC('o','d','k','m');
	sinf->scheme_type->scheme_version = 0x00000200;

	sinf->original_format = (GF_OriginalFormatBox *)frma_New();
	sinf->original_format->data_format = original_format;
	sinf->info = (GF_SchemeInformationBox *)schi_New();

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS


void gf_isom_cenc_sample_del(GF_CENCSampleInfo *samp)
{
	if (samp->subsamples) gf_free(samp->subsamples);
	gf_free(samp);
}

static GF_Err gf_isom_cenc_parse_sample(GF_CENCSampleInfo *cenc_sample, GF_BitStream *bs, u32 sample_number, Bool use_subsample)
{
	u32 i=0, k;
	if ((cenc_sample->IV_size!=0) && (cenc_sample->IV_size!=8) && (cenc_sample->IV_size!=16) ) return GF_ISOM_INVALID_FILE;

	while (gf_bs_available(bs)) {
		i++;
		if (i == sample_number) {
			gf_bs_read_data(bs, (char *) cenc_sample->IV, cenc_sample->IV_size);

			if (use_subsample) {
				cenc_sample->subsample_count = gf_bs_read_u16(bs);
				cenc_sample->subsamples = gf_malloc(sizeof(GF_CENCSubSampleEntry)* cenc_sample->subsample_count);
				for (k=0; k<cenc_sample->subsample_count; k++) {
					cenc_sample->subsamples[k].bytes_clear_data = gf_bs_read_u16(bs);
					cenc_sample->subsamples[k].bytes_encrypted_data = gf_bs_read_u32(bs);
				}
			}
			return GF_OK;
		} else {
			gf_bs_skip_bytes(bs, cenc_sample->IV_size);
			if (use_subsample) {
				u32 subcount = gf_bs_read_u16(bs);
				gf_bs_skip_bytes(bs, subcount * 6);
			}
		}
	}
	return GF_ISOM_INVALID_FILE;
}

GF_CENCSampleInfo *gf_isom_cenc_get_sample(GF_TrackBox *trak, GF_TrackFragmentBox *traf, u32 sample_number)
{
	GF_Err e;
	Bool use_subsample = 0;
	GF_CENCSampleInfo *cenc_sample;
	GF_ProtectionInfoBox *sinf;
	GF_SampleEntryBox *sentry;
	u32 sample_desc_idx;
	GF_BitStream *bs;

	if (!sample_number) return NULL;

	sample_desc_idx = traf->tfhd->sample_desc_index;
	if (!sample_desc_idx) sample_desc_idx = traf->trex->def_sample_desc_index;

	sentry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, sample_desc_idx - 1);
	sinf = sentry ? gf_list_get(sentry->protections, 0) : NULL;

	/*PIFF*/
	if (sinf && sinf->scheme_type && sinf->info->piff_tenc) {
		/*TODO !!*/
		if (!traf->piff_sample_encryption) return NULL;

		GF_SAFEALLOC(cenc_sample, GF_CENCSampleInfo);
		memcpy(cenc_sample->keyID, sinf->info->piff_tenc->KID, 16);
		cenc_sample->IV_size = sinf->info->piff_tenc->IV_size;
		cenc_sample->algo_id = sinf->info->piff_tenc->AlgorithmID;
		
		if (traf->piff_sample_encryption->flags & 1) {
			memcpy(cenc_sample->keyID, traf->piff_sample_encryption->KID, 16);
			cenc_sample->IV_size = traf->piff_sample_encryption->IV_size;
			cenc_sample->algo_id = traf->piff_sample_encryption->AlgorithmID;
			cenc_sample->is_alt_info = 1;
		}

		use_subsample = (traf->piff_sample_encryption->flags & 2) ? 1 : 0;

		bs = gf_bs_new(traf->piff_sample_encryption->cenc_data, traf->piff_sample_encryption->cenc_data_size, GF_BITSTREAM_READ);
		e = gf_isom_cenc_parse_sample(cenc_sample, bs, sample_number, use_subsample);
		gf_bs_del(bs);
		if (e) {
			gf_isom_cenc_sample_del(cenc_sample);
			return NULL;
		}
		return cenc_sample;
	}
	return NULL;
}

#endif //	GPAC_DISABLE_ISOM_FRAGMENTS


#endif /*GPAC_DISABLE_ISOM*/
