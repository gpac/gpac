/*
 *			GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato / Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2019
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


GF_ISMASample *gf_isom_ismacryp_sample_from_data(u8 *data, u32 dataLength, Bool use_selective_encryption, u8 KI_length, u8 IV_length)
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

static GF_ProtectionSchemeInfoBox *isom_get_sinf_entry(GF_TrackBox *trak, u32 sampleDescriptionIndex, u32 scheme_type, GF_SampleEntryBox **out_sea)
{
	u32 i=0;
	GF_SampleEntryBox *sea=NULL;
	GF_ProtectionSchemeInfoBox *sinf;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	if (!sea) return NULL;

	i = 0;
	while ((sinf = (GF_ProtectionSchemeInfoBox*)gf_list_enum(sea->child_boxes, &i))) {
		if (sinf->type != GF_ISOM_BOX_TYPE_SINF) continue;

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
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, 0, NULL);
	if (!sinf) return NULL;

	/*ISMA*/
	if (sinf->scheme_type->scheme_type == GF_ISOM_ISMACRYP_SCHEME) {
		fmt = sinf->info->isfm;
		if (!fmt) return NULL;
		return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, sinf->info->isfm->selective_encryption, sinf->info->isfm->key_indicator_length, sinf->info->isfm->IV_length);
	}
	/*OMA*/
	else if (sinf->scheme_type->scheme_type == GF_ISOM_OMADRM_SCHEME ) {
		if (!sinf->info->odkm) return NULL;
		fmt = sinf->info->odkm->fmt;

		if (fmt) {
			return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, fmt->selective_encryption, fmt->key_indicator_length, fmt->IV_length);
		}
		/*OMA default: no selective encryption, one key, 128 bit IV*/
		return gf_isom_ismacryp_sample_from_data(samp->data, samp->dataLength, GF_FALSE, 0, 128);
	}
	return NULL;
}


GF_EXPORT
u32 gf_isom_is_media_encrypted(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, 0, NULL);
	if (!sinf) return 0;

	/*non-encrypted or non-ISMA*/
	if (!sinf || !sinf->scheme_type) return 0;
	if (sinf->scheme_type->scheme_type == GF_4CC('p','i','f','f')) return GF_4CC('c','e','n','c');
	return sinf->scheme_type->scheme_type;
}

GF_EXPORT
Bool gf_isom_is_ismacryp_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, NULL);
	if (!sinf) return GF_FALSE;

	/*non-encrypted or non-ISMA*/
	if (!sinf->info || !sinf->info->ikms || !sinf->info->isfm )
		return GF_FALSE;

	return GF_TRUE;
}

GF_EXPORT
Bool gf_isom_is_omadrm_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_OMADRM_SCHEME, NULL);
	if (!sinf) return GF_FALSE;

	/*non-encrypted or non-OMA*/
	if (!sinf->info || !sinf->info->odkm || !sinf->info->odkm->hdr)
		return GF_FALSE;

	return GF_TRUE;
}

/*retrieves ISMACryp info for the given track & SDI*/
GF_EXPORT
GF_Err gf_isom_get_ismacryp_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outSchemeURI, const char **outKMS_URI, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, NULL);
	if (!sinf) return GF_OK;

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
		if (outSelectiveEncryption) *outSelectiveEncryption = GF_FALSE;
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
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_OMADRM_SCHEME, NULL);
	if (!sinf) return GF_OK;

	if (!sinf->info || !sinf->info->odkm || !sinf->info->odkm->hdr) return GF_NON_COMPLIANT_BITSTREAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sinf->original_format->data_format;
		if (IsMP4Description(sinf->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sinf->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sinf->scheme_type->scheme_version;
	if (outContentID) *outContentID = sinf->info->odkm->hdr->ContentID;
	if (outRightsIssuerURL) *outRightsIssuerURL = sinf->info->odkm->hdr->RightsIssuerURL;
	if (outTextualHeaders) {
		*outTextualHeaders = sinf->info->odkm->hdr->TextualHeaders;
		if (outTextualHeadersLen) *outTextualHeadersLen = sinf->info->odkm->hdr->TextualHeadersLen;
	}
	if (outPlaintextLength) *outPlaintextLength = sinf->info->odkm->hdr->PlaintextLength;
	if (outEncryptionType) *outEncryptionType = sinf->info->odkm->hdr->EncryptionMethod;

	if (sinf->info && sinf->info->odkm && sinf->info->odkm->fmt) {
		if (outSelectiveEncryption) *outSelectiveEncryption = sinf->info->odkm->fmt->selective_encryption;
		if (outIVLength) *outIVLength = sinf->info->odkm->fmt->IV_length;
		if (outKeyIndicationLength) *outKeyIndicationLength = sinf->info->odkm->fmt->key_indicator_length;
	} else {
		if (outSelectiveEncryption) *outSelectiveEncryption = GF_FALSE;
		if (outIVLength) *outIVLength = 0;
		if (outKeyIndicationLength) *outKeyIndicationLength = 0;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_remove_track_protection(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionSchemeInfoBox *sinf;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !sampleDescriptionIndex) return GF_BAD_PARAM;

	sea = NULL;
	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, &sea);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, &sea);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENS_SCHEME, &sea);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBCS_SCHEME, &sea);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, &sea);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_OMADRM_SCHEME, &sea);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ADOBE_SCHEME, &sea);
	if (!sinf) return GF_OK;

	sea->type = sinf->original_format->data_format;
	while (1) {
		GF_Box *b = gf_isom_box_find_child(sea->child_boxes, GF_ISOM_BOX_TYPE_SINF);
		if (!b) break;
		gf_isom_box_del_parent(&sea->child_boxes, b);
	}
	if (sea->type == GF_ISOM_BOX_TYPE_264B) sea->type = GF_ISOM_BOX_TYPE_AVC1;
	if (sea->type == GF_ISOM_BOX_TYPE_265B) sea->type = GF_ISOM_BOX_TYPE_HVC1;
	if (sea->type == GF_ISOM_BOX_TYPE_AV1B) sea->type = GF_ISOM_BOX_TYPE_AV01;
	if (sea->type == GF_ISOM_BOX_TYPE_VP9B) sea->type = GF_ISOM_BOX_TYPE_VP09;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_change_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, char *scheme_uri, char *kms_uri)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionSchemeInfoBox *sinf;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !sampleDescriptionIndex) return GF_BAD_PARAM;

	sea = NULL;
	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ISMACRYP_SCHEME, &sea);
	if (!sinf) return GF_OK;

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


static GF_Err isom_set_protected_entry(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u8 version, u32 flags,
        u32 scheme_type, u32 scheme_version, char *scheme_uri, Bool is_isma, GF_ProtectionSchemeInfoBox **out_sinf)
{
	u32 original_format;
	GF_Err e;
	GF_SampleEntryBox *sea;
	GF_ProtectionSchemeInfoBox *sinf;
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
	case GF_ISOM_BOX_TYPE_EC3:
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
	case GF_ISOM_BOX_TYPE_MVC1:
		if (is_isma)
			original_format = GF_ISOM_BOX_TYPE_264B;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_LHE1:
	case GF_ISOM_BOX_TYPE_LHV1:
	case GF_ISOM_BOX_TYPE_HVT1:
		if (is_isma)
			original_format = GF_ISOM_BOX_TYPE_265B;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_VP09:
		if (is_isma)
			original_format = GF_ISOM_BOX_TYPE_VP9B;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_AV01:
		if (is_isma)
			original_format = GF_ISOM_BOX_TYPE_AV1B;
		sea->type = GF_ISOM_BOX_TYPE_ENCV;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
	case GF_ISOM_BOX_TYPE_LSR1:
		sea->type = GF_ISOM_BOX_TYPE_ENCS;
		break;
	case GF_ISOM_BOX_TYPE_STXT:
	case GF_ISOM_BOX_TYPE_WVTT:
	case GF_ISOM_BOX_TYPE_STPP:
		sea->type = GF_ISOM_BOX_TYPE_ENCT;
		break;
	case GF_ISOM_BOX_TYPE_ENCA:
	case GF_ISOM_BOX_TYPE_ENCV:
	case GF_ISOM_BOX_TYPE_ENCT:
	case GF_ISOM_BOX_TYPE_ENCM:
	case GF_ISOM_BOX_TYPE_ENCF:
	case GF_ISOM_BOX_TYPE_ENCS:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] cannot set protection entry: file is already encrypted.\n"));
		return GF_BAD_PARAM;
	default:
		return GF_BAD_PARAM;
	}

	sinf = (GF_ProtectionSchemeInfoBox *)gf_isom_box_new_parent(&sea->child_boxes, GF_ISOM_BOX_TYPE_SINF);

	sinf->scheme_type = (GF_SchemeTypeBox *)gf_isom_box_new_parent(&sinf->child_boxes, GF_ISOM_BOX_TYPE_SCHM);
	sinf->scheme_type->version = version;
	sinf->scheme_type->flags = flags;
	sinf->scheme_type->scheme_type = scheme_type;
	sinf->scheme_type->scheme_version = scheme_version;
	if (sinf->scheme_type->flags == 1) {
		sinf->scheme_type->URI = (char *)gf_malloc(sizeof(char)*strlen(scheme_uri));
		memmove(sinf->scheme_type->URI, scheme_uri, strlen(scheme_uri));
	}

	sinf->original_format = (GF_OriginalFormatBox *)gf_isom_box_new_parent(&sinf->child_boxes, GF_ISOM_BOX_TYPE_FRMA);
	sinf->original_format->data_format = original_format;

	//common to isma, cenc and oma
	sinf->info = (GF_SchemeInformationBox *)gf_isom_box_new_parent(&sinf->child_boxes, GF_ISOM_BOX_TYPE_SCHI);

	*out_sinf = sinf;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type,
                                       u32 scheme_version, char *scheme_uri, char *kms_URI,
                                       Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	GF_Err e;
	GF_ProtectionSchemeInfoBox *sinf;

	//setup generic protection
	e = isom_set_protected_entry(the_file, trackNumber, desc_index, 0, 0, scheme_type, scheme_version, NULL, GF_TRUE, &sinf);
	if (e) return e;

	if (scheme_uri) {
		sinf->scheme_type->flags |= 0x000001;
		sinf->scheme_type->URI = gf_strdup(scheme_uri);
	}

	sinf->info->ikms = (GF_ISMAKMSBox *)gf_isom_box_new_parent(&sinf->info->child_boxes, GF_ISOM_BOX_TYPE_IKMS);
	sinf->info->ikms->URI = gf_strdup(kms_URI);

	sinf->info->isfm = (GF_ISMASampleFormatBox *)gf_isom_box_new_parent(&sinf->info->child_boxes, GF_ISOM_BOX_TYPE_ISFM);
	sinf->info->isfm->selective_encryption = selective_encryption;
	sinf->info->isfm->key_indicator_length = KI_length;
	sinf->info->isfm->IV_length = IV_length;
	return GF_OK;
}

GF_Err gf_isom_set_oma_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index,
                                  char *contentID, char *kms_URI, u32 encryption_type, u64 plainTextLength, char *textual_headers, u32 textual_headers_len,
                                  Bool selective_encryption, u32 KI_length, u32 IV_length)
{
	GF_ProtectionSchemeInfoBox *sinf;
	GF_Err e;

	//setup generic protection
	e = isom_set_protected_entry(the_file, trackNumber, desc_index, 0, 0, GF_ISOM_OMADRM_SCHEME, 0x00000200, NULL, GF_FALSE, &sinf);
	if (e) return e;

	sinf->info->odkm = (GF_OMADRMKMSBox *)gf_isom_box_new_parent(&sinf->info->child_boxes, GF_ISOM_BOX_TYPE_ODKM);
	sinf->info->odkm->fmt = (GF_OMADRMAUFormatBox*)gf_isom_box_new_parent(&sinf->info->odkm->child_boxes, GF_ISOM_BOX_TYPE_ODAF);
	sinf->info->odkm->fmt->selective_encryption = selective_encryption;
	sinf->info->odkm->fmt->key_indicator_length = KI_length;
	sinf->info->odkm->fmt->IV_length = IV_length;

	sinf->info->odkm->hdr = (GF_OMADRMCommonHeaderBox*)gf_isom_box_new_parent(&sinf->info->odkm->child_boxes, GF_ISOM_BOX_TYPE_OHDR);
	sinf->info->odkm->hdr->EncryptionMethod = encryption_type;
	sinf->info->odkm->hdr->PaddingScheme = (encryption_type==0x01) ? 1 : 0;
	sinf->info->odkm->hdr->PlaintextLength = plainTextLength;
	if (contentID) sinf->info->odkm->hdr->ContentID = gf_strdup(contentID);
	if (kms_URI) sinf->info->odkm->hdr->RightsIssuerURL = gf_strdup(kms_URI);
	if (textual_headers) {
		sinf->info->odkm->hdr->TextualHeaders = (char*)gf_malloc(sizeof(char)*textual_headers_len);
		memcpy(sinf->info->odkm->hdr->TextualHeaders, textual_headers, sizeof(char)*textual_headers_len);
		sinf->info->odkm->hdr->TextualHeadersLen = textual_headers_len;
	}
	return GF_OK;
}

GF_Err gf_isom_set_generic_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, u32 scheme_version, char *scheme_uri, char *kms_URI)
{
	GF_Err e;
	GF_ProtectionSchemeInfoBox *sinf;

	//setup generic protection
	e = isom_set_protected_entry(the_file, trackNumber, desc_index, 0, 0, scheme_type, scheme_version, NULL, GF_TRUE, &sinf);
	if (e) return e;

	if (scheme_uri) {
		sinf->scheme_type->flags |= 0x000001;
		sinf->scheme_type->URI = gf_strdup(scheme_uri);
	}

	if (kms_URI) {
		sinf->info->ikms = (GF_ISMAKMSBox *)gf_isom_box_new_parent(&sinf->info->child_boxes, GF_ISOM_BOX_TYPE_IKMS);
		sinf->info->ikms->URI = gf_strdup(kms_URI);
	}
	return GF_OK;
}
#endif // GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_get_original_format_type(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *sea;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	Media_GetSampleDesc(trak->Media, sampleDescriptionIndex, &sea, NULL);
	if (!sea) return GF_BAD_PARAM;

	sinf = (GF_ProtectionSchemeInfoBox*) gf_isom_box_find_child(sea->child_boxes, GF_ISOM_BOX_TYPE_SINF);
	if (outOriginalFormat && sinf && sinf->original_format) {
		*outOriginalFormat = sinf->original_format->data_format;
	}
	return GF_OK;
}


/* Common Encryption*/
GF_EXPORT
Bool gf_isom_is_cenc_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

//	if (trak->sample_encryption) return GF_TRUE;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENS_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBCS_SCHEME, NULL);

	if (!sinf) return GF_FALSE;

	/*non-encrypted or non-CENC*/
	if (!sinf->info || !sinf->info->tenc || !sinf->scheme_type)
		return GF_FALSE;

	//TODO: validate the fields in tenc for each scheme type
	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_isom_get_cenc_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, u32 *outIVLength)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;


	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENS_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBCS_SCHEME, NULL);

	if (!sinf) return GF_BAD_PARAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sinf->original_format->data_format;
		if (IsMP4Description(sinf->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sinf->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sinf->scheme_type->scheme_version;

	if (outIVLength) {
		if (sinf->info && sinf->info->tenc)
			*outIVLength = sinf->info->tenc->Per_Sample_IV_Size;
		else
			*outIVLength = 0;
	}

	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_set_cenc_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type,
                                   u32 scheme_version, u32 default_IsEncrypted, u8 default_IV_size,	bin128 default_KID,
								   u8 default_crypt_byte_block, u8 default_skip_byte_block,
								    u8 default_constant_IV_size, bin128 default_constant_IV)
{
	GF_Err e;
	GF_ProtectionSchemeInfoBox *sinf;

	//setup generic protection
	e = isom_set_protected_entry(the_file, trackNumber, desc_index, 0, 0, scheme_type, scheme_version, NULL, GF_FALSE, &sinf);
	if (e) return e;

	sinf->info->tenc = (GF_TrackEncryptionBox *)gf_isom_box_new_parent(&sinf->info->child_boxes, GF_ISOM_BOX_TYPE_TENC);
	sinf->info->tenc->isProtected = default_IsEncrypted;
	sinf->info->tenc->Per_Sample_IV_Size = default_IV_size;
	memcpy(sinf->info->tenc->KID, default_KID, 16*sizeof(char));
	if ((scheme_type == GF_ISOM_CENS_SCHEME) || (scheme_type == GF_ISOM_CBCS_SCHEME)) {
		sinf->info->tenc->version = 1;
		sinf->info->tenc->crypt_byte_block = default_crypt_byte_block;
		sinf->info->tenc->skip_byte_block = default_skip_byte_block;
	}
	if (scheme_type == GF_ISOM_CBCS_SCHEME) {
		sinf->info->tenc->constant_IV_size = default_constant_IV_size;
		memcpy(sinf->info->tenc->constant_IV, default_constant_IV, 16*sizeof(char));
	}
	return GF_OK;
}


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
		switch (saiz->aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
		case 0:
			break;
		default:
			continue;
		}
		gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *)saiz);
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
		switch (saio->aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
		case 0:
			break;
		default:
			continue;
		}
		gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *)saio);
		gf_list_rem(stbl->sai_offsets, i);
		i--;
	}

	if (!gf_list_count(stbl->sai_offsets)) {
		gf_list_del(stbl->sai_offsets);
		stbl->sai_offsets = NULL;
	}

	return GF_OK;
}

GF_Err gf_cenc_set_pssh(GF_ISOFile *file, bin128 systemID, u32 version, u32 KID_count, bin128 *KIDs, u8 *data, u32 len)
{
	GF_ProtectionSystemHeaderBox *pssh = NULL;
	u32 i=0;
	GF_Box *a;
	GF_List **other_boxes = NULL;
	if (file->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) {
		if (!file->moof) return GF_BAD_PARAM;
		if (!file->moof->PSSHs) file->moof->PSSHs = gf_list_new();
		other_boxes = &file->moof->PSSHs;
	} else {
		if (!file->moov->child_boxes) file->moov->child_boxes = gf_list_new();
		other_boxes = &file->moov->child_boxes;
	}

	while ((a = gf_list_enum(*other_boxes, &i))) {
		if (a->type!=GF_ISOM_BOX_TYPE_PSSH) continue;
		pssh = (GF_ProtectionSystemHeaderBox *)a;
		if (!memcmp(pssh->SystemID, systemID, sizeof(bin128))) break;
		pssh = NULL;
	}
	//we had a pssh with same ID but different private data, keep both...
	if (pssh && pssh->private_data && len && memcmp(pssh->private_data, data, sizeof(char)*len) ) {
		pssh = NULL;
	}

	if (!pssh) {
		pssh = (GF_ProtectionSystemHeaderBox *)gf_isom_box_new_parent(other_boxes, GF_ISOM_BOX_TYPE_PSSH);
		if (!pssh) return GF_IO_ERR;

		memcpy((char *)pssh->SystemID, systemID, sizeof(bin128));
		pssh->version = version;
	}

	if (KID_count) {
		u32 j;
		for (j=0; j<KID_count; j++) {
			Bool found = GF_FALSE;
			for (i=0; i<pssh->KID_count; i++) {
				if (!memcmp(pssh->KIDs[i], KIDs[j], sizeof(bin128))) found = GF_TRUE;
			}

			if (!found) {
				pssh->KIDs = gf_realloc(pssh->KIDs, sizeof(bin128) * (pssh->KID_count+1));
				memcpy(pssh->KIDs[pssh->KID_count], KIDs[j], sizeof(bin128));
				pssh->KID_count++;
			}
		}
		if (!pssh->version)
			pssh->version = 1;
	}

	if (!pssh->private_data_size) {
		pssh->private_data_size = len;
		if (!pssh->private_data && len)
			pssh->private_data = (u8 *)gf_malloc(pssh->private_data_size*sizeof(char));
		memcpy((char *)pssh->private_data, data, pssh->private_data_size);
	}

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

	for (i = 0; i < gf_list_count(stbl->child_boxes); i++) {
		GF_Box *a = (GF_Box *)gf_list_get(stbl->child_boxes, i);
		if ((a->type ==GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
			gf_isom_box_del_parent(&stbl->child_boxes, a);
			i--;
		}
		else if (a->type == GF_ISOM_BOX_TYPE_SENC) {
			gf_isom_box_del_parent(&stbl->child_boxes, a);
			i--;
		}
	}

	if (!gf_list_count(stbl->child_boxes)) {
		gf_list_del(stbl->child_boxes);
		stbl->child_boxes = NULL;
	}
	for (i = 0; i < gf_list_count(trak->child_boxes); i++) {
		GF_Box *a = (GF_Box *)gf_list_get(trak->child_boxes, i);
		if ((a->type ==GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
			gf_isom_box_del_parent(&trak->child_boxes, a);
			i--;
		}
		else if (a->type == GF_ISOM_BOX_TYPE_SENC) {
			gf_isom_box_del_parent(&trak->child_boxes, a);
			i--;
		}
	}
	return GF_OK;
}

GF_Err gf_isom_remove_samp_group_box(GF_ISOFile *the_file, u32 trackNumber)
{
	u32 i;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return GF_BAD_PARAM;

	for (i = 0; i < gf_list_count(stbl->sampleGroupsDescription); i++) {
		GF_SampleGroupDescriptionBox *a = (GF_SampleGroupDescriptionBox *)gf_list_get(stbl->sampleGroupsDescription, i);
		if (a->grouping_type == GF_ISOM_SAMPLE_GROUP_SEIG) {
			gf_list_rem(stbl->sampleGroupsDescription, i);
			gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *) a);
			i--;
		}
	}
	if (!gf_list_count(stbl->sampleGroupsDescription)) {
		gf_list_del(stbl->sampleGroupsDescription);
		stbl->sampleGroupsDescription = NULL;
	}

	for (i = 0; i < gf_list_count(stbl->sampleGroups); i++) {
		GF_SampleGroupBox *a = (GF_SampleGroupBox *)gf_list_get(stbl->sampleGroups, i);
		if (a->grouping_type == GF_ISOM_SAMPLE_GROUP_SEIG) {
			gf_list_rem(stbl->sampleGroups, i);
			gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *) a);
			i--;
		}
	}
	if (!gf_list_count(stbl->sampleGroups)) {
		gf_list_del(stbl->sampleGroups);
		stbl->sampleGroups = NULL;
	}

	return GF_OK;
}

GF_Err gf_isom_remove_pssh_box(GF_ISOFile *the_file)
{
	u32 i;
	for (i = 0; i < gf_list_count(the_file->moov->child_boxes); i++) {
		GF_Box *a = (GF_Box *)gf_list_get(the_file->moov->child_boxes, i);
		if ((a->type == GF_ISOM_BOX_UUID_PSSH) || (a->type == GF_ISOM_BOX_TYPE_PSSH)) {
			gf_isom_box_del_parent(&the_file->moov->child_boxes, a);
			i--;
		}
	}

	if (!gf_list_count(the_file->moov->child_boxes)) {
		gf_list_del(the_file->moov->child_boxes);
		the_file->moov->child_boxes = NULL;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_SampleEncryptionBox * gf_isom_create_piff_psec_box(u8 version, u32 flags, u32 AlgorithmID, u8 IV_size, bin128 KID)
{
	GF_SampleEncryptionBox *psec;

	psec = (GF_SampleEncryptionBox *) gf_isom_box_new(GF_ISOM_BOX_UUID_PSEC);
	if (!psec)
		return NULL;
	psec->version = version;
	psec->flags = flags;
	psec->is_piff = GF_TRUE;
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

	senc = (GF_SampleEncryptionBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SENC);
	if (!senc)
		return NULL;
	senc->version = version;
	senc->flags = flags;
	senc->samp_aux_info = gf_list_new();

	return senc;
}

GF_Err gf_isom_cenc_allocate_storage(GF_ISOFile *the_file, u32 trackNumber, u32 container_type, u32 AlgorithmID, u8 IV_size, bin128 KID)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (trak->sample_encryption) return GF_OK;
	switch (container_type) {
	case GF_ISOM_BOX_UUID_PSEC:
		trak->sample_encryption = (GF_SampleEncryptionBox *)gf_isom_create_piff_psec_box(1, 0, AlgorithmID, IV_size, KID);
		break;
	case GF_ISOM_BOX_TYPE_SENC:
		trak->sample_encryption = (GF_SampleEncryptionBox *)gf_isom_create_samp_enc_box(0, 0);
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	//senc will be written and destroyed with the other boxes
	if (!trak->child_boxes) trak->child_boxes = gf_list_new();
	return gf_list_add(trak->child_boxes, trak->sample_encryption);
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
void gf_isom_cenc_set_saiz_saio(GF_SampleEncryptionBox *senc, GF_SampleTableBox *stbl, GF_TrackFragmentBox  *traf, u32 len, Bool saio_32bits)
{
	u32  i;
	GF_List **other_boxes = stbl ? &stbl->child_boxes : &traf->child_boxes;
	if (!senc->cenc_saiz) {
		senc->cenc_saiz = (GF_SampleAuxiliaryInfoSizeBox *) gf_isom_box_new_parent(other_boxes, GF_ISOM_BOX_TYPE_SAIZ);
		//as per 3rd edition of cenc "so content SHOULD be created omitting these optional fields" ...
		senc->cenc_saiz->aux_info_type = 0;
		senc->cenc_saiz->aux_info_type_parameter = 0;
		if (stbl)
			stbl_on_child_box((GF_Box*)stbl, (GF_Box *)senc->cenc_saiz);
		else
			traf_on_child_box((GF_Box*)traf, (GF_Box *)senc->cenc_saiz);
	}
	if (!senc->cenc_saio) {
		senc->cenc_saio = (GF_SampleAuxiliaryInfoOffsetBox *) gf_isom_box_new_parent(other_boxes, GF_ISOM_BOX_TYPE_SAIO);
		//force using version 1 for saio box, it could be redundant when we use 64 bits for offset
		senc->cenc_saio->version = saio_32bits ? 0 : 1;
		//as per 3rd edition of cenc "so content SHOULD be created omitting these optional fields" ...
		senc->cenc_saio->aux_info_type = 0;
		senc->cenc_saio->aux_info_type_parameter = 0;
		senc->cenc_saio->entry_count = 1;
		if (stbl)
			stbl_on_child_box((GF_Box*)stbl, (GF_Box *)senc->cenc_saio);
		else
			traf_on_child_box((GF_Box*)traf, (GF_Box *)senc->cenc_saio);
	}

	if (!senc->cenc_saiz->sample_count || ((senc->cenc_saiz->default_sample_info_size==len) && len) ) {
		senc->cenc_saiz->sample_count ++;
		senc->cenc_saiz->default_sample_info_size = len;
	} else {
		if (senc->cenc_saiz->sample_count + 1 > senc->cenc_saiz->sample_alloc) {
			if (!senc->cenc_saiz->sample_alloc) senc->cenc_saiz->sample_alloc = senc->cenc_saiz->sample_count+1;
			else senc->cenc_saiz->sample_alloc *= 2;

			senc->cenc_saiz->sample_info_size = (u8*)gf_realloc(senc->cenc_saiz->sample_info_size, sizeof(u8)*(senc->cenc_saiz->sample_alloc));
		}

		if (senc->cenc_saiz->default_sample_info_size) {
			for (i=0; i<senc->cenc_saiz->sample_count; i++)
				senc->cenc_saiz->sample_info_size[i] = senc->cenc_saiz->default_sample_info_size;
			senc->cenc_saiz->default_sample_info_size = 0;
		}
		senc->cenc_saiz->sample_info_size[senc->cenc_saiz->sample_count] = len;
		senc->cenc_saiz->sample_count++;
	}
}

void gf_isom_cenc_merge_saiz_saio(GF_SampleEncryptionBox *senc, GF_SampleTableBox *stbl, u64 offset, u32 len)
{
	u32  i;
	assert(stbl);
	if (!senc->cenc_saiz) {
		senc->cenc_saiz = (GF_SampleAuxiliaryInfoSizeBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_SAIZ);
		senc->cenc_saiz->aux_info_type = GF_ISOM_CENC_SCHEME;
		senc->cenc_saiz->aux_info_type_parameter = 0;
		stbl_on_child_box((GF_Box*)stbl, (GF_Box *)senc->cenc_saiz);
	}
	if (!senc->cenc_saio) {
		senc->cenc_saio = (GF_SampleAuxiliaryInfoOffsetBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_SAIO);
		//force using version 1 for saio box, it could be redundant when we use 64 bits for offset
		senc->cenc_saio->version = 1;
		senc->cenc_saio->aux_info_type = GF_ISOM_CENC_SCHEME;
		senc->cenc_saio->aux_info_type_parameter = 0;
		stbl_on_child_box((GF_Box*)stbl, (GF_Box *)senc->cenc_saio);
	}

	if (!senc->cenc_saiz->sample_count || (senc->cenc_saiz->default_sample_info_size==len)) {
		senc->cenc_saiz->sample_count ++;
		senc->cenc_saiz->default_sample_info_size = len;
	} else {
		if (senc->cenc_saiz->sample_count + 1 > senc->cenc_saiz->sample_alloc) {
			if (!senc->cenc_saiz->sample_alloc) senc->cenc_saiz->sample_alloc = senc->cenc_saiz->sample_count + 1;
			else senc->cenc_saiz->sample_alloc *= 2;
			senc->cenc_saiz->sample_info_size = (u8*)gf_realloc(senc->cenc_saiz->sample_info_size, sizeof(u8)*(senc->cenc_saiz->sample_alloc));
		}

		if (senc->cenc_saiz->default_sample_info_size) {
			for (i=0; i<senc->cenc_saiz->sample_count; i++)
				senc->cenc_saiz->sample_info_size[i] = senc->cenc_saiz->default_sample_info_size;
			senc->cenc_saiz->default_sample_info_size = 0;
		}
		senc->cenc_saiz->sample_info_size[senc->cenc_saiz->sample_count] = len;
		senc->cenc_saiz->sample_count++;
	}

	if (!senc->cenc_saio->entry_count) {
		senc->cenc_saio->offsets = (u64 *)gf_malloc(sizeof(u64));
		senc->cenc_saio->offsets[0] = offset;
		senc->cenc_saio->entry_count ++;
	} else {
		senc->cenc_saio->offsets = (u64*)gf_realloc(senc->cenc_saio->offsets, sizeof(u64)*(senc->cenc_saio->entry_count+1));
		senc->cenc_saio->offsets[senc->cenc_saio->entry_count] = offset;
		senc->cenc_saio->entry_count++;
	}
	if (offset > 0xFFFFFFFFUL)
		senc->cenc_saio->version=1;
}
#endif /* GPAC_DISABLE_ISOM_FRAGMENTS */

GF_Err gf_isom_track_cenc_add_sample_info(GF_ISOFile *the_file, u32 trackNumber, u32 container_type, u8 IV_size, u8 *buf, u32 len, Bool use_subsamples, u8 *clear_IV, Bool use_saio_32bit)
{
	u32 i;
	GF_SampleEncryptionBox *senc;
	GF_CENCSampleAuxInfo *sai;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl) return GF_BAD_PARAM;

	switch (container_type) {
	case GF_ISOM_BOX_UUID_PSEC:
	case GF_ISOM_BOX_TYPE_SENC:
		senc = trak->sample_encryption;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
	if (!sai) return GF_OUT_OF_MEM;

	memset(sai, 0, sizeof(GF_CENCSampleAuxInfo));
	if (len && buf) {
		GF_BitStream *bs = gf_bs_new(buf, len, GF_BITSTREAM_READ);
		sai->IV_size = IV_size;
		gf_bs_read_data(bs, (char *)sai->IV, IV_size);
		if (use_subsamples) {
			sai->subsample_count = gf_bs_read_u16(bs);
			if (sai->subsample_count) senc->flags = 0x00000002;
			sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
			for (i = 0; i < sai->subsample_count; i++) {
				sai->subsamples[i].bytes_clear_data = gf_bs_read_u16(bs);
				sai->subsamples[i].bytes_encrypted_data = gf_bs_read_u32(bs);
			}
		}
		gf_bs_del(bs);
	} else if (len) {
		u32 olen = len;
		sai->IV_size = IV_size;
		if (clear_IV) memcpy(sai->IV, clear_IV, sizeof(bin128));
		if (use_subsamples) {
			sai->subsample_count = 1;
			if (sai->subsample_count) senc->flags = 0x00000002;
			while (olen>0xFFFF) {
				olen -= 0xFFFF;
				sai->subsample_count ++;
			}
			sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
			olen = len;
			for (i = 0; i < sai->subsample_count; i++) {
				if (olen<0xFFFF) {
					sai->subsamples[i].bytes_clear_data = olen;
				} else {
					sai->subsamples[i].bytes_clear_data = 0xFFFF;
					olen -= 0xFFFF;
				}
				sai->subsamples[i].bytes_encrypted_data = 0;
			}
		}
		len = IV_size + 2 + 6*sai->subsample_count;
	}

	gf_list_add(senc->samp_aux_info, sai);
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	gf_isom_cenc_set_saiz_saio(senc, stbl, NULL, len, use_saio_32bit);
#endif

	return GF_OK;
}


GF_EXPORT
void gf_isom_cenc_samp_aux_info_del(GF_CENCSampleAuxInfo *samp)
{
	//if (samp->IV) gf_free(samp->IV);
	if (samp->subsamples) gf_free(samp->subsamples);
	gf_free(samp);
}

Bool gf_isom_cenc_has_saiz_saio_full(GF_SampleTableBox *stbl, void *_traf, u32 scheme_type)
{
	u32 i, c1, c2;
	GF_List *sai_sizes, *sai_offsets;
	Bool has_saiz, has_saio;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	GF_TrackFragmentBox *traf=(GF_TrackFragmentBox *)_traf;
#endif
	has_saiz = has_saio = GF_FALSE;

	if (stbl) {
		sai_sizes = stbl->sai_sizes;
		sai_offsets = stbl->sai_offsets;
	}
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	else if (_traf) {
		sai_sizes = traf->sai_sizes;
		sai_offsets = traf->sai_offsets;
	}
#endif
	else
		return GF_FALSE;

	c1 = gf_list_count(sai_sizes);
	c2 = gf_list_count(sai_offsets);
	for (i = 0; i < c1; i++) {
		GF_SampleAuxiliaryInfoSizeBox *saiz = (GF_SampleAuxiliaryInfoSizeBox *)gf_list_get(sai_sizes, i);
		u32 saiz_aux_info_type = saiz->aux_info_type;
		if (!saiz_aux_info_type) saiz_aux_info_type = scheme_type;

		if (!saiz_aux_info_type && (c1==1) && (c2==1)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] saiz box without flags nor aux info type and no default scheme, ignoring\n"));
			continue;
		}

		switch (saiz_aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			has_saiz = GF_TRUE;
			break;
		}
	}

	for (i = 0; i < c2; i++) {
		GF_SampleAuxiliaryInfoOffsetBox *saio = (GF_SampleAuxiliaryInfoOffsetBox *)gf_list_get(sai_offsets, i);
		u32 saio_aux_info_type = saio->aux_info_type;
		if (!saio_aux_info_type) saio_aux_info_type = scheme_type;

		if (!saio_aux_info_type && (c1==1) && (c2==1)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] saio box without flags nor aux info type and no default scheme, ignoring\n"));
			continue;
		}
		switch (saio_aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			has_saio = GF_TRUE;
			break;
		}
	}
	return (has_saiz && has_saio);
}

Bool gf_isom_cenc_has_saiz_saio_track(GF_SampleTableBox *stbl, u32 scheme_type)
{
	return gf_isom_cenc_has_saiz_saio_full(stbl, NULL, scheme_type);
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
Bool gf_isom_cenc_has_saiz_saio_traf(GF_TrackFragmentBox *traf, u32 scheme_type)
{
	return gf_isom_cenc_has_saiz_saio_full(NULL, traf, scheme_type);
}
#endif


static GF_Err isom_cenc_get_sai_by_saiz_saio(GF_MediaBox *mdia, u32 sampleNumber, u32 scheme_type, u8 IV_size, GF_CENCSampleAuxInfo **sai, u8 **out_buffer, u32 *out_size)
{
	u32  prev_sai_size, size, i, j, nb_saio;
	u64 cur_position, offset;
	GF_Err e = GF_OK;
	GF_SampleAuxiliaryInfoOffsetBox *saio_cenc=NULL;
	GF_SampleAuxiliaryInfoSizeBox *saiz_cenc=NULL;
	nb_saio = size = prev_sai_size = 0;
	offset = 0;

	for (i = 0; i < gf_list_count(mdia->information->sampleTable->sai_offsets); i++) {
		GF_SampleAuxiliaryInfoOffsetBox *saio = (GF_SampleAuxiliaryInfoOffsetBox *)gf_list_get(mdia->information->sampleTable->sai_offsets, i);
		u32 aux_info_type = saio->aux_info_type;
		if (!aux_info_type) aux_info_type = scheme_type;

		switch (aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			break;
		default:
			continue;
		}

		if (saio->entry_count == 1)
			offset = saio->offsets[0];
		else
			offset = saio->offsets[sampleNumber-1];
		nb_saio = saio->entry_count;
		saio_cenc = saio;
		break;
	}
	if (!saio_cenc) return GF_ISOM_INVALID_FILE;

	for (i = 0; i < gf_list_count(mdia->information->sampleTable->sai_sizes); i++) {
		GF_SampleAuxiliaryInfoSizeBox *saiz = (GF_SampleAuxiliaryInfoSizeBox *)gf_list_get(mdia->information->sampleTable->sai_sizes, i);
		u32 aux_info_type = saiz->aux_info_type;
		if (!aux_info_type) aux_info_type = scheme_type;

		switch (aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			break;
		default:
			continue;
		}
		if (sampleNumber>saiz->sample_count) {
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if ((nb_saio==1) && !saio_cenc->total_size) {
			for (j = 0; j < saiz->sample_count; j++) {
				saio_cenc->total_size += saiz->default_sample_info_size ? saiz->default_sample_info_size : saiz->sample_info_size[j];
			}
		}
		if (saiz->cached_sample_num+1== sampleNumber) {
			prev_sai_size = saiz->cached_prev_size;
		} else {
			for (j = 0; j < sampleNumber-1; j++)
				prev_sai_size += saiz->default_sample_info_size ? saiz->default_sample_info_size : saiz->sample_info_size[j];
		}
		size = saiz->default_sample_info_size ? saiz->default_sample_info_size : saiz->sample_info_size[sampleNumber-1];
		saiz_cenc=saiz;
		break;
	}
	if (!saiz_cenc) return GF_BAD_PARAM;

	if (saio_cenc->total_size) {
		if (!saio_cenc->cached_data) {
			saio_cenc->cached_data = gf_malloc(sizeof(u8)*saio_cenc->total_size);
			cur_position = gf_bs_get_position(mdia->information->dataHandler->bs);
			gf_bs_seek(mdia->information->dataHandler->bs, offset);
			gf_bs_read_data(mdia->information->dataHandler->bs, saio_cenc->cached_data, saio_cenc->total_size);
			gf_bs_seek(mdia->information->dataHandler->bs, cur_position);
		}
		if ((*out_size) < size) {
			(*out_buffer) = gf_realloc((*out_buffer), sizeof(char)*(size) );
		}
		(*out_size) = size;
		memcpy((*out_buffer), saio_cenc->cached_data + prev_sai_size, size);
		saiz_cenc->cached_sample_num = sampleNumber;
		saiz_cenc->cached_prev_size = prev_sai_size + size;
		return GF_OK;
	}

	offset += (nb_saio == 1) ? prev_sai_size : 0;
	cur_position = gf_bs_get_position(mdia->information->dataHandler->bs);
	gf_bs_seek(mdia->information->dataHandler->bs, offset);

	if (sai) {
		*sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
		memset(*sai, 0, sizeof(GF_CENCSampleAuxInfo));

		if (size) {
			gf_bs_read_data(mdia->information->dataHandler->bs, (char *)(*sai)->IV, IV_size);
			if (size > IV_size) {
				(*sai)->subsample_count = gf_bs_read_u16(mdia->information->dataHandler->bs);
				(*sai)->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sizeof(GF_CENCSubSampleEntry)*(*sai)->subsample_count);
				for (i = 0; i < (*sai)->subsample_count; i++) {
					(*sai)->subsamples[i].bytes_clear_data = gf_bs_read_u16(mdia->information->dataHandler->bs);
					(*sai)->subsamples[i].bytes_encrypted_data = gf_bs_read_u32(mdia->information->dataHandler->bs);
				}
			}
		}
	} else {
		if ((*out_size) < size) {
			(*out_buffer) = gf_realloc((*out_buffer), sizeof(char)*(size) );
		}
		(*out_size) = size;
		gf_bs_read_data(mdia->information->dataHandler->bs, (*out_buffer), size);
	}
	gf_bs_seek(mdia->information->dataHandler->bs, cur_position);

	return e;
}


static GF_Err gf_isom_cenc_get_sample_aux_info_internal(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, GF_CENCSampleAuxInfo **sai, u32 *container_type, u8 **out_buffer, u32 *outSize)
{
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;
	GF_SampleEncryptionBox *senc = NULL;
	u32 type, scheme_type = -1;
	GF_CENCSampleAuxInfo *a_sai;
	u8 IV_size, constant_IV_size;
	bin128 constant_IV;
	Bool is_Protected;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl)
		return GF_BAD_PARAM;

	type = 0;
	senc = trak->sample_encryption;
	//no senc is OK
	if (senc) {
		if ((senc->type == GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)senc)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
			type = GF_ISOM_BOX_UUID_PSEC;
		} else if (senc->type == GF_ISOM_BOX_TYPE_SENC) {
			type = GF_ISOM_BOX_TYPE_SENC;
		}

		if (container_type) *container_type = type;
	}

	if (!sai && !out_buffer) return GF_OK; /*we need only container_type*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	sampleNumber -= trak->sample_count_at_seg_start;
#endif

	if (sai && *sai) {
		gf_isom_cenc_samp_aux_info_del(*sai);
		*sai = NULL;
	}
	gf_isom_get_cenc_info(the_file, trackNumber, 1, NULL, &scheme_type, NULL, NULL);
	gf_isom_get_sample_cenc_info_ex(trak, NULL, senc, sampleNumber, &is_Protected, &IV_size, NULL, NULL, NULL, &constant_IV_size, &constant_IV);

	/*get sample auxiliary information by saiz/saio rather than by parsing senc box*/
	if ((!type || !sai) && gf_isom_cenc_has_saiz_saio_track(stbl, scheme_type)) {
		return isom_cenc_get_sai_by_saiz_saio(trak->Media, sampleNumber, scheme_type, IV_size, sai, out_buffer, outSize);

	}
	if (!senc)
		return GF_OK;

	//senc is not loaded by default, do it now
	if (!gf_list_count(senc->samp_aux_info)) {
		GF_Err e = senc_Parse(trak->Media->information->dataHandler->bs, trak, NULL, senc);
		if (e) return e;
	}

	a_sai = NULL;
	switch (type) {
	case GF_ISOM_BOX_UUID_PSEC:
		if (senc)
			a_sai = (GF_CENCSampleAuxInfo *)gf_list_get(senc->samp_aux_info, sampleNumber-1);
		break;
	case GF_ISOM_BOX_TYPE_SENC:
		if (senc)
			a_sai = (GF_CENCSampleAuxInfo *)gf_list_get(senc->samp_aux_info, sampleNumber-1);
		break;
	}
	if (!a_sai) {
		if (!IV_size && constant_IV_size) return GF_OK;
		return GF_NOT_SUPPORTED;
	}

	if (!sai)
		return GF_BAD_PARAM;

	GF_SAFEALLOC((*sai), GF_CENCSampleAuxInfo);
	if (!(*sai) ) return GF_OUT_OF_MEM;
	if (senc) {
		u8 size = ((*sai)->IV_size != 0) ? (*sai)->IV_size : 8/*default for modern PIFF/CENC with AES-CTR*/;
		memmove((*sai)->IV, a_sai->IV, size);
		(*sai)->subsample_count = a_sai->subsample_count;
		if ((*sai)->subsample_count > 0) {
			(*sai)->subsamples = (GF_CENCSubSampleEntry*)gf_malloc(sizeof(GF_CENCSubSampleEntry)*(*sai)->subsample_count);
			memmove((*sai)->subsamples, a_sai->subsamples, sizeof(GF_CENCSubSampleEntry)*(*sai)->subsample_count);
		}
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_cenc_get_sample_aux_info_buffer(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *container_type, u8 **out_buffer, u32 *outSize)
{
	return gf_isom_cenc_get_sample_aux_info_internal(the_file, trackNumber, sampleNumber, NULL, container_type, out_buffer, outSize);
}

GF_EXPORT
GF_Err gf_isom_cenc_get_sample_aux_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, GF_CENCSampleAuxInfo **sai, u32 *container_type)
{
	return gf_isom_cenc_get_sample_aux_info_internal(the_file, trackNumber, sampleNumber, sai, container_type, NULL, NULL);
}

void gf_isom_cenc_get_default_info_ex(GF_TrackBox *trak, u32 sampleDescriptionIndex, u32 *container_type, Bool *default_IsEncrypted, u8 *default_IV_size, bin128 *default_KID, u8 *constant_IV_size, bin128 *constant_IV, u8 *crypt_byte_block, u8 *skip_byte_block)
{
	GF_ProtectionSchemeInfoBox *sinf;

	if (default_IsEncrypted) *default_IsEncrypted = 0;
	if (default_IV_size) *default_IV_size = 0;
	if (default_KID) memset(*default_KID, 0, 16);
	if (constant_IV_size) *constant_IV_size = 0;
	if (constant_IV) memset(*constant_IV, 0, 16);
	if (crypt_byte_block) *crypt_byte_block = 0;
	if (skip_byte_block) *skip_byte_block = 0;
	if (container_type) *container_type = 0;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENC_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBC_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENS_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBCS_SCHEME, NULL);

	if (sinf && sinf->info && sinf->info->tenc) {
		if (default_IsEncrypted) *default_IsEncrypted = sinf->info->tenc->isProtected;
		if (default_IV_size) *default_IV_size = sinf->info->tenc->Per_Sample_IV_Size;
		if (default_KID) memmove(*default_KID, sinf->info->tenc->KID, 16);
		if (constant_IV_size) *constant_IV_size = sinf->info->tenc->constant_IV_size;
		if (constant_IV) memmove(*constant_IV, sinf->info->tenc->constant_IV, 16);
		if (crypt_byte_block) *crypt_byte_block = sinf->info->tenc->crypt_byte_block;
		if (skip_byte_block) *skip_byte_block = sinf->info->tenc->skip_byte_block;

	} else {
		if (! trak->moov->mov->is_smooth) {
			trak->moov->mov->is_smooth = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] senc box without tenc, assuming MS smooth+piff\n"));
		}
		if (default_IsEncrypted) *default_IsEncrypted = GF_TRUE;
		//leave as 0 to make sure we use the default IV size from PIFF_PSEC if any ...
		if (default_IV_size) *default_IV_size = 0;
	}

	if (container_type && trak->sample_encryption) {
		if (trak->sample_encryption->type == GF_ISOM_BOX_TYPE_SENC) *container_type = GF_ISOM_BOX_TYPE_SENC;
		else if (trak->sample_encryption->type == GF_ISOM_BOX_TYPE_UUID) *container_type = ((GF_UUIDBox*)trak->sample_encryption)->internal_4cc;
	}
}

GF_EXPORT
void gf_isom_cenc_get_default_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *container_type, Bool *default_IsEncrypted, u8 *default_IV_size, bin128 *default_KID, u8 *constant_IV_size, bin128 *constant_IV, u8 *crypt_byte_block, u8 *skip_byte_block)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return;
	gf_isom_cenc_get_default_info_ex(trak, sampleDescriptionIndex, container_type, default_IsEncrypted, default_IV_size, default_KID, constant_IV_size, constant_IV, crypt_byte_block, skip_byte_block);
}

/*
	Adobe'protection scheme
*/
GF_Err gf_isom_set_adobe_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, u32 scheme_version, Bool is_selective_enc, char *metadata, u32 len)
{
	GF_ProtectionSchemeInfoBox *sinf;

	//setup generic protection
#ifndef GPAC_DISABLE_ISOM_WRITE
	GF_Err e;
	e = isom_set_protected_entry(the_file, trackNumber, desc_index, 1, 0, scheme_type, scheme_version, NULL, GF_FALSE, &sinf);
	if (e) return e;
#else
	return GF_NOT_SUPPORTED;
#endif

	sinf->info->adkm = (GF_AdobeDRMKeyManagementSystemBox *)gf_isom_box_new_parent(&sinf->info->child_boxes, GF_ISOM_BOX_TYPE_ADKM);

	sinf->info->adkm->header = (GF_AdobeDRMHeaderBox *)gf_isom_box_new_parent(&sinf->info->adkm->child_boxes, GF_ISOM_BOX_TYPE_AHDR);

	sinf->info->adkm->header->std_enc_params = (GF_AdobeStdEncryptionParamsBox *)gf_isom_box_new_parent(& sinf->info->adkm->header->child_boxes, GF_ISOM_BOX_TYPE_APRM);

	sinf->info->adkm->header->std_enc_params->enc_info = (GF_AdobeEncryptionInfoBox *)gf_isom_box_new_parent(&sinf->info->adkm->header->std_enc_params->child_boxes, GF_ISOM_BOX_TYPE_AEIB);
	if (sinf->info->adkm->header->std_enc_params->enc_info->enc_algo)
		gf_free(sinf->info->adkm->header->std_enc_params->enc_info->enc_algo);
	sinf->info->adkm->header->std_enc_params->enc_info->enc_algo = (char *)gf_malloc(8*sizeof(char));
	strncpy(sinf->info->adkm->header->std_enc_params->enc_info->enc_algo, "AES-CBC", 7);
	sinf->info->adkm->header->std_enc_params->enc_info->enc_algo[7] = 0;
	sinf->info->adkm->header->std_enc_params->enc_info->key_length = 16;

	sinf->info->adkm->header->std_enc_params->key_info = (GF_AdobeKeyInfoBox *)gf_isom_box_new_parent(&sinf->info->adkm->header->std_enc_params->child_boxes, GF_ISOM_BOX_TYPE_AKEY);

	sinf->info->adkm->header->std_enc_params->key_info->params = (GF_AdobeFlashAccessParamsBox *)gf_isom_box_new_parent(&sinf->info->adkm->header->std_enc_params->key_info->child_boxes, GF_ISOM_BOX_TYPE_FLXS);
	if (metadata && len) {
		if (sinf->info->adkm->header->std_enc_params->key_info->params->metadata)
			gf_free(sinf->info->adkm->header->std_enc_params->key_info->params->metadata);
		sinf->info->adkm->header->std_enc_params->key_info->params->metadata = (char *)gf_malloc((len+1)*sizeof(char));
		strncpy(sinf->info->adkm->header->std_enc_params->key_info->params->metadata, metadata, len);
		sinf->info->adkm->header->std_enc_params->key_info->params->metadata[len] = 0;
	}

	sinf->info->adkm->au_format = (GF_AdobeDRMAUFormatBox *)gf_isom_box_new_parent(&sinf->info->adkm->child_boxes, GF_ISOM_BOX_TYPE_ADAF);
	sinf->info->adkm->au_format->selective_enc = is_selective_enc ? 0x10 : 0x00;
	sinf->info->adkm->au_format->IV_length = 16;

	return GF_OK;
}

GF_EXPORT
Bool gf_isom_is_adobe_protection_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ADOBE_SCHEME, NULL);

	if (!sinf) return GF_FALSE;

	/*non-encrypted or non-ADOBE*/
	if (!sinf->info || !sinf->info->adkm)
		return GF_FALSE;

	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_isom_get_adobe_protection_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outMetadata)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_ADOBE_SCHEME, NULL);

	if (!sinf) return GF_BAD_PARAM;

	if (outOriginalFormat) {
		*outOriginalFormat = sinf->original_format->data_format;
		if (IsMP4Description(sinf->original_format->data_format)) *outOriginalFormat = GF_ISOM_SUBTYPE_MPEG4;
	}
	if (outSchemeType) *outSchemeType = sinf->scheme_type->scheme_type;
	if (outSchemeVersion) *outSchemeVersion = sinf->scheme_type->scheme_version;

	if (outMetadata) {
		*outMetadata = NULL;
		if (sinf->info && sinf->info->adkm && sinf->info->adkm->header && sinf->info->adkm->header->std_enc_params && sinf->info->adkm->header->std_enc_params->key_info
			&& sinf->info->adkm->header->std_enc_params->key_info->params && sinf->info->adkm->header->std_enc_params->key_info->params->metadata)
		{
			*outMetadata = sinf->info->adkm->header->std_enc_params->key_info->params->metadata;
		}
	}

	return GF_OK;
}

GF_EXPORT
Bool gf_isom_cenc_is_pattern_mode(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ProtectionSchemeInfoBox *sinf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

	sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CENS_SCHEME, NULL);
	if (!sinf) sinf = isom_get_sinf_entry(trak, sampleDescriptionIndex, GF_ISOM_CBCS_SCHEME, NULL);

	if (!sinf) return GF_FALSE;

	if (!sinf->info || !sinf->info->tenc || !sinf->info->tenc->crypt_byte_block || !sinf->info->tenc->skip_byte_block)
		return GF_FALSE;

	return GF_TRUE;
}


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
