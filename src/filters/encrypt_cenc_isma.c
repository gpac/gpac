/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / CENC and ISMA encrypt module
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

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/crypt_tools.h>
#include <gpac/crypt.h>
#include <gpac/base_coding.h>
#include <gpac/download.h>
#include <gpac/xml.h>
#include <gpac/internal/isomedia_dev.h>

#include <gpac/internal/media_dev.h>

#ifndef GPAC_DISABLE_CRYPTO


enum
{
	ISMAEA_STATE_ERROR,
	ISMAEA_STATE_SETUP,
	ISMAEA_STATE_PLAY,
};


typedef enum {
	CENC_FULL_SAMPLE=1,

	/*below types may have several ranges (clear/encrypted) per sample*/
	CENC_AVC, /*AVC, nalu-based*/
	CENC_HEVC, /*HEVC, nalu-based*/
	CENC_AV1,  /*AV1, OBU-based*/
	CENC_VPX,  /*VPX, custom, see https://www.webmproject.org/vp9/mp4/ */
	CENC_VVC, /*VVC, nalu-based*/
} CENCCodecMode;

typedef struct
{
	GF_Crypt *crypt;
	char IV[16];
	bin128 key;
	u32 IV_size;
} CENC_MKey;

typedef struct
{
	Bool passthrough;

	GF_CryptInfo *cinfo;

	GF_FilterPid *ipid;
	GF_FilterPid *opid;
	GF_TrackCryptInfo *tci;

	//active keys (for multikey support)
	u32 nb_keys;
	CENC_MKey *keys;
	Bool multi_key;
	u32 codec_id;

	u32 nb_pck;

	//ISMA/OMA var
	u32 nalu_size_length;
	u32 dsi_crc;
	Bool isma_oma;
	u64 BSO;
	u64 range_end;
	Bool prev_pck_encrypted;
	u32 KI_length;
	u32 isma_IV_size;
	Bool has_crypted_pck;

	Bool is_adobe;

	CENCCodecMode cenc_codec;

	u32 bytes_in_nal_hdr;
	Bool use_subsamples;
	Bool cenc_init;
	u32 nb_pck_encrypted, kidx, nb_saps, nb_segments, nb_periods;

	//true if using AES-CTR mode, false if using AES-CBC mode
	Bool ctr_mode;
	Bool is_saes;

	Bool rap_roll;

#ifndef GPAC_DISABLE_AV_PARSERS
	AVCState *avc_state;
#ifndef GPAC_DISABLE_HEVC
	HEVCState *hevc_state;
#endif
	AV1State *av1_state;
	GF_VPConfig *vp9_cfg;

	VVCState *vvc_state;
#endif
	Bool slice_header_clear;

	GF_PropUIntList mkey_indices;

	//0: no generation, >0: use v-1 as key idx
	u32 pssh_template_plus_one;
	GF_List *pssh_templates;
} GF_CENCStream;

typedef struct
{
	//options
	const char *cfile;
	Bool allc;
	
	//internal
	GF_CryptInfo *cinfo;

	GF_List *streams;
	GF_BitStream *bs_w, *bs_r;
} GF_CENCEncCtx;


static GF_Err isma_enc_configure(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, Bool is_isma, const char *scheme_uri, const char *kms_uri)
{
	GF_Err e;
	bin128 IV;
	const GF_PropertyValue *p;
	p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	switch (cstr->codec_id) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		if (p) {
			GF_AVCConfig *avcc = gf_odf_avc_cfg_read(p->value.data.ptr, p->value.data.size);
			cstr->nalu_size_length = avcc ? avcc->nal_unit_size : 0;
			if (avcc) gf_odf_avc_cfg_del(avcc);
		}
		if (!cstr->nalu_size_length) {
			cstr->nalu_size_length = 4;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Missing NALU length size, assuming 4\n") );
		}
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		if (p) {
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, (cstr->codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);
			cstr->nalu_size_length = hvcc ? hvcc->nal_unit_size : 0;
			if (hvcc) gf_odf_hevc_cfg_del(hvcc);
		}
		if (!cstr->nalu_size_length) {
			cstr->nalu_size_length = 4;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Missing NALU length size, assuming 4\n") );
		}
		break;
	case GF_CODECID_VVC:
		if (p) {
			GF_VVCConfig *vvcc = gf_odf_vvc_cfg_read(p->value.data.ptr, p->value.data.size);
			cstr->nalu_size_length = vvcc ? vvcc->nal_unit_size : 0;
			if (vvcc) gf_odf_vvc_cfg_del(vvcc);
		}
		if (!cstr->nalu_size_length) {
			cstr->nalu_size_length = 4;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Missing NALU length size, assuming 4\n") );
		}
		break;
	}

	if (!scheme_uri || !strlen(scheme_uri)) scheme_uri = "urn:gpac:isma:encryption_scheme";

	if (cstr->tci && ((cstr->tci->sel_enc_type==GF_CRYPT_SELENC_RAND) || (cstr->tci->sel_enc_type==GF_CRYPT_SELENC_RAND_RANGE)) ) {
		gf_rand_init(GF_TRUE);
	}

	cstr->isma_IV_size = 0;
	if (!is_isma) {
		/*128 bit IV in OMA*/
		cstr->isma_IV_size = 16;
	} else {
		p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_ISMA_IV_LENGTH);
		if (p) {
			cstr->isma_IV_size = p->value.uint;
		} else {
			p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_MEDIA_DATA_SIZE);
			if (p) {
				u64 BSO = p->value.longuint;

				if (BSO<0xFFFF) cstr->isma_IV_size = 2;
				else if (BSO<0xFFFFFFFF) cstr->isma_IV_size = 4;
				else cstr->isma_IV_size = 8;
			}
		}
		if (!cstr->isma_IV_size || (cstr->isma_IV_size > 8)) cstr->isma_IV_size = 8;
	}

	if (!cstr->tci || !cstr->tci->nb_keys) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ISMACrypt] No keys specified for ISMA\n"));
		return GF_BAD_PARAM;
	}

	/*init crypto*/
	memset(IV, 0, sizeof(char)*16);
	memcpy(IV, cstr->tci->keys[0].IV, sizeof(char)*8);
	if (cstr->keys[0].crypt) gf_crypt_close(cstr->keys[0].crypt);
	cstr->keys[0].crypt = gf_crypt_open(GF_AES_128, GF_CTR);
	if (!cstr->keys[0].crypt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ISMACrypt] Cannot open AES-128 CTR\n"));
		return GF_IO_ERR;
	}

	e = gf_crypt_init(cstr->keys[0].crypt, cstr->tci->keys[0].key, IV);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ISMACrypt] Cannot initialize AES-128 CTR (%s)\n", gf_error_to_string(e)) );
		return GF_IO_ERR;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ISMACrypt] Encrypting stream %s - KMS: %s%s\n", gf_filter_pid_get_name(cstr->ipid), cstr->tci->KMS_URI, cstr->tci->sel_enc_type ? " - Selective Encryption" : ""));

	if (!stricmp(kms_uri, "self")) {
		char Data[100], d64[100];
		u32 size_b64;
		memcpy(Data, cstr->tci->keys[0].key, sizeof(char)*16);
		memcpy(Data+16, cstr->tci->keys[0].IV, sizeof(char)*8);
		size_b64 = gf_base64_encode(Data, 24, d64, 100);
		d64[size_b64] = 0;
		cstr->tci->KMS_URI = gf_realloc(cstr->tci->KMS_URI, size_b64+6);
		strcpy(cstr->tci->KMS_URI, "(key)");
		strcat(cstr->tci->KMS_URI, d64);
		kms_uri = cstr->tci->KMS_URI;
	}

	/*create ISMA protection*/
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, &PROP_UINT(1) );
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_KMS_URI, kms_uri ? &PROP_STRING(kms_uri) : NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ISMA_SELECTIVE_ENC, &PROP_BOOL( (cstr->tci->sel_enc_type!=0) ? GF_TRUE : GF_FALSE) );
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ISMA_IV_LENGTH, &PROP_UINT(cstr->isma_IV_size) );

	if (is_isma) {
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_URI, scheme_uri ? &PROP_STRING(scheme_uri) : NULL );
	} else {
		if ((cstr->tci->sel_enc_type==GF_CRYPT_SELENC_PREVIEW) && cstr->tci->sel_enc_range) {
			char szSTR[100];
			u32 len;
			char *szPreview;

			sprintf(szSTR, "PreviewRange:%d", cstr->tci->sel_enc_range);
			len = (u32) strlen(szSTR) + ( cstr->tci->TextualHeaders ? (u32) strlen(cstr->tci->TextualHeaders) : 0 ) + 1;
			szPreview = gf_malloc(sizeof(char) * len);
			strcpy(szPreview, cstr->tci->TextualHeaders ? cstr->tci->TextualHeaders : "");
			strcat(szPreview, szSTR);

			gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_OMA_TXT_HDR, &PROP_STRING_NO_COPY(szPreview));
		} else {
			gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_OMA_TXT_HDR, cstr->tci->TextualHeaders ? &PROP_STRING(cstr->tci->TextualHeaders) : NULL);
		}

		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_OMA_CID, scheme_uri ? &PROP_STRING(scheme_uri) : NULL );

		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_OMA_CRYPT_TYPE, &PROP_UINT(cstr->tci->encryption));
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_OMA_CLEAR_LEN, &PROP_LONGUINT(cstr->tci->sel_enc_range) );
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ISMA_KI_LENGTH, &PROP_UINT(0) );
	}

	cstr->range_end = 0;
	if (cstr->tci->sel_enc_type==GF_CRYPT_SELENC_PREVIEW) {
		p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_TIMESCALE);
		if (p)
			cstr->range_end = p->value.uint *cstr->tci->sel_enc_range;
	}

	cstr->isma_oma = GF_TRUE;
	cstr->prev_pck_encrypted = GF_TRUE;
	cstr->has_crypted_pck = GF_FALSE;

	//we drop IPMPX support for now
#if GPAC_DEPRECTAED
	/*format as IPMP(X) - note that the ISMACryp spec is broken since it always uses IPMPPointers to a
	single desc which would assume the same protection (eg key & salt) for all streams using it...*/
	if (!tci->ipmp_type) return GF_OK;

	ipmpdp = (GF_IPMPPtr*)gf_odf_desc_new(GF_ODF_IPMP_PTR_TAG);
	if (!tci->ipmp_desc_id) tci->ipmp_desc_id = track;
	if (tci->ipmp_type==2) {
		ipmpdp->IPMP_DescriptorID = 0xFF;
		ipmpdp->IPMP_DescriptorIDEx = tci->ipmp_desc_id;
	} else {
		ipmpdp->IPMP_DescriptorID = tci->ipmp_desc_id;
	}
	gf_isom_add_desc_to_description(mp4, track, 1, (GF_Descriptor *)ipmpdp);
	gf_odf_desc_del((GF_Descriptor*)ipmpdp);

	ipmpdU = (GF_IPMPUpdate*)gf_odf_com_new(GF_ODF_IPMP_UPDATE_TAG);
	/*format IPMPD*/
	ipmpd = (GF_IPMP_Descriptor*)gf_odf_desc_new(GF_ODF_IPMP_TAG);
	if (tci->ipmp_type==2) {
#ifndef GPAC_MINIMAL_ODF
		ipmpd->IPMP_DescriptorID = 0xFF;
		ipmpd->IPMP_DescriptorIDEx = tci->ipmp_desc_id;
		ipmpd->IPMPS_Type = 0xFFFF;
		ipmpd->IPMP_ToolID[14] = 0x49;
		ipmpd->IPMP_ToolID[15] = 0x53;
		ipmpd->control_point = 1;
		ipmpd->cp_sequence_code = 0x80;
		/*format IPMPXData*/
		ismac = (GF_IPMPX_ISMACryp *) gf_ipmpx_data_new(GF_IPMPX_ISMACRYP_TAG);
		ismac->cryptoSuite = 1;	/*default ISMA AESCTR128*/
		ismac->IV_length = IV_size;
		ismac->key_indicator_length = 0;
		ismac->use_selective_encryption = (tci->sel_enc_type!=0)? 1 : 0;
		gf_list_add(ipmpd->ipmpx_data, ismac);
#endif
	} else {
		ipmpd->IPMP_DescriptorID = tci->ipmp_desc_id;
	}
	gf_list_add(ipmpdU->IPMPDescList, ipmpd);

	for (i=0; i<gf_isom_get_track_count(mp4); i++) {
		GF_ODCodec *cod;
		if (gf_isom_get_media_type(mp4, i+1) != GF_ISOM_MEDIA_OD) continue;

		/*add com*/
		samp = gf_isom_get_sample(mp4, i+1, 1, &di);
		cod = gf_odf_codec_new();
		gf_odf_codec_set_au(cod, samp->data, samp->dataLength);
		gf_odf_codec_decode(cod);
		gf_odf_codec_add_com(cod, (GF_ODCom *) ipmpdU);
		gf_free(samp->data);
		samp->data = NULL;
		samp->dataLength = 0;
		gf_odf_codec_encode(cod, 1);
		gf_odf_codec_get_au(cod, &samp->data, &samp->dataLength);
		ipmpdU = NULL;
		gf_odf_codec_del(cod);
		gf_isom_update_sample(mp4, i+1, 1, samp, 1);
		gf_isom_sample_del(&samp);

		if (tci->ipmp_type==2) {
			GF_IPMP_ToolList*ipmptl = (GF_IPMP_ToolList*)gf_odf_desc_new(GF_ODF_IPMP_TL_TAG);
			GF_IPMP_Tool *ipmpt = (GF_IPMP_Tool*)gf_odf_desc_new(GF_ODF_IPMP_TOOL_TAG);
			gf_list_add(ipmptl->ipmp_tools, ipmpt);
			ipmpt->IPMP_ToolID[14] = 0x49;
			ipmpt->IPMP_ToolID[15] = 0x53;
			gf_isom_add_desc_to_root_od(mp4, (GF_Descriptor *)ipmptl);
			gf_odf_desc_del((GF_Descriptor *)ipmptl);
		}
		break;
	}
#endif
	return GF_OK;
}

static GF_Err adobe_enc_configure(GF_CENCEncCtx *ctx, GF_CENCStream *cstr)
{
	if (cstr->keys[0].crypt) gf_crypt_close(cstr->keys[0].crypt);
	cstr->keys[0].crypt = gf_crypt_open(GF_AES_128, GF_CBC);
	if (!cstr->keys[0].crypt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Adobe] Cannot open AES-128 CBC \n"));
		return GF_IO_ERR;
	}

	/*Adobe's protection scheme does not support selective key*/
	memcpy(cstr->keys[0].key, cstr->tci->keys[0].key, 16);

	if (cstr->tci->metadata)
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ADOBE_CRYPT_META, &PROP_DATA(cstr->tci->metadata, (u32) strlen(cstr->tci->metadata) ) );

	cstr->is_adobe = GF_TRUE;

	return GF_OK;
}

static GF_Err cenc_parse_pssh(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, const char *cfile_name)
{
	GF_DOMParser *parser;
	GF_XMLNode *root, *node;
	u32 i;
	GF_Err e;
	u32 nb_pssh=0;
	GF_BitStream *pssh_bs=NULL;

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, cfile_name, NULL, NULL);
	if (e) {
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);
	if (!root) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Cannot open or validate xml file %s\n", ctx->cfile));
		return GF_NOT_SUPPORTED;
	}
	pssh_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(pssh_bs, 0);

	if (cstr->pssh_templates) {
		while (gf_list_count(cstr->pssh_templates)) {
			GF_XMLNode *n = gf_list_pop_back(cstr->pssh_templates);
			gf_xml_dom_node_del(n);
		}
		gf_list_del(cstr->pssh_templates);
	}
	cstr->pssh_templates = NULL;

	i=0;
	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		Bool is_pssh;
		u32 version, cypherMode, specInfoSize, len, KID_count, j;
		bin128 cypherKey, cypherIV, systemID;
		GF_XMLAttribute *att;
		u8 *data, *specInfo;
		GF_BitStream *bs;
		bin128 *KIDs;
		s32 cypherOffset = -1;
		Bool has_key = GF_FALSE, has_IV = GF_FALSE;

		if (!strcmp(node->name, "DRMInfoTemplate")) {
			if (!cstr->pssh_templates) cstr->pssh_templates = gf_list_new();
			if (gf_list_add(cstr->pssh_templates, node) == GF_OK) {
				i--;
				gf_list_rem(root->content, i);
			}
			continue;
		}
		if (strcmp(node->name, "DRMInfo")) continue;

		j = 0;
		is_pssh = GF_FALSE;
		version = cypherMode = 0;
		data = specInfo = NULL;
		bs = NULL;

		while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!strcmp(att->name, "type")) {
				if (!strcmp(att->value, "pssh"))
					is_pssh = GF_TRUE;
			} else if (!strcmp(att->name, "version")) {
				version = atoi(att->value);
			} else if (!strcmp(att->name, "cypherMode")) {
				/*cypher-mode: 0: ctr (default mode) -  1: cbc - 2: clear*/
				if (!strcmp(att->value, "yes"))
					cypherMode = 0;
				else if (!strcmp(att->value, "no"))
					cypherMode = 1;
			} else if (!strcmp(att->name, "cypherKey")) {
				e = gf_bin128_parse(att->value, cypherKey);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannnot parse cypherKey\n"));
                    break;
                }
				has_key = GF_TRUE;
			} else if (!strcmp(att->name, "cypherIV")) {
				e = gf_bin128_parse(att->value, cypherIV);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannnot parse cypherIV\n"));
                    break;
                }
				has_IV = GF_TRUE;
			} else if (!strcmp(att->name, "cypherOffset")) {
				cypherOffset = atoi(att->value);
			}
		}

		if (!is_pssh) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] Not a Protection System Specific Header Box - skipping\n"));
			continue;
		}

		e = gf_xml_parse_bit_sequence(node, cfile_name, &specInfo, &specInfoSize);
		if (e) {
			if (specInfo) gf_free(specInfo);
			break;
		}

		bs = gf_bs_new(specInfo, specInfoSize, GF_BITSTREAM_READ);
		u32 bsize = gf_bs_read_u32(bs);
		u32 btype = gf_bs_read_u32(bs);
		if ((bsize==specInfoSize) && (btype == GF_ISOM_BOX_TYPE_PSSH)) {
			version = gf_bs_read_u8(bs);
			/*flags*/ gf_bs_read_int(bs, 24);
		} else {
			gf_bs_seek(bs, 0);
			btype = 0;
		}

		gf_bs_read_data(bs, (char *)systemID, 16);
		if (version) {
			KID_count = gf_bs_read_u32(bs);
			if (KID_count*16 > gf_bs_available(bs)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid PSSH blob, KID count %d but only %d bytes available\n", KID_count, gf_bs_available(bs)));
				if (specInfo) gf_free(specInfo);
				gf_bs_del(bs);
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
			KIDs = (bin128 *)gf_malloc(KID_count*sizeof(bin128));
			if (!KIDs) {
				if (specInfo) gf_free(specInfo);
				gf_bs_del(bs);
				e = GF_OUT_OF_MEM;
				break;
			}
			for (j = 0; j < KID_count; j++) {
				gf_bs_read_data(bs, (char *)KIDs[j], 16);
			}
		}
		else {
			KID_count = 0;
			KIDs = NULL;
		}

		if (specInfoSize < 16 + (version ? 4 + 16*KID_count : 0)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid PSSH blob in version %d: size %d key count %d - ignoring PSSH\n", version, specInfoSize, KID_count));

			if (specInfo) gf_free(specInfo);
			gf_bs_del(bs);
			if (KIDs) gf_free(KIDs);
			continue;
		}
		if (btype)
			len = gf_bs_read_u32(bs);
		else
			len = specInfoSize - 16 - (version ? 4 + 16*KID_count : 0);

		data = (char *)gf_malloc(len*sizeof(char));
		if (!data) {
			e = GF_OUT_OF_MEM;
			if (specInfo) gf_free(specInfo);
			gf_bs_del(bs);
			if (KIDs) gf_free(KIDs);
			break;
		}
		gf_bs_read_data(bs, data, len);

		if (has_key && has_IV && (cypherOffset >= 0) && (cypherMode != 1)) {
			GF_Crypt *gc = gf_crypt_open(GF_AES_128, GF_CTR);
			if (!gc) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Cannot open AES-128 CTR\n"));
				if (specInfo) gf_free(specInfo);
				gf_bs_del(bs);
				if (KIDs) gf_free(KIDs);
				if (data) gf_free(data);
				e = GF_IO_ERR;
				break;
			}
			e = gf_crypt_init(gc, cypherKey, cypherIV);
			gf_crypt_encrypt(gc, data+cypherOffset, len-cypherOffset);
			gf_crypt_close(gc);
		}
		if (!e) {
			nb_pssh++;
			gf_bs_write_data(pssh_bs, systemID, 16);
			gf_bs_write_u32(pssh_bs, version);
			if (version) {
				gf_bs_write_u32(pssh_bs, KID_count);
				for (j=0; j<KID_count; j++) {
					gf_bs_write_data(pssh_bs, KIDs[j], 16);
				}
			}
			gf_bs_write_u32(pssh_bs, len);
			gf_bs_write_data(pssh_bs, data, len);
		}

		if (specInfo) gf_free(specInfo);
		if (data) gf_free(data);
		if (KIDs) gf_free(KIDs);
		if (bs) gf_bs_del(bs);
		if (e) break;
	}

	gf_xml_dom_del(parser);

	if (!e && nb_pssh) {
		u8 *pssh=NULL;
		u32 pssh_size=0;
		u32 pos = (u32) gf_bs_get_position(pssh_bs);
		gf_bs_seek(pssh_bs, 0);
		gf_bs_write_u32(pssh_bs, nb_pssh);
		gf_bs_seek(pssh_bs, pos);
		gf_bs_get_content(pssh_bs, &pssh, &pssh_size);
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_PSSH, &PROP_DATA_NO_COPY(pssh, pssh_size));
	}
	gf_bs_del(pssh_bs);

	return e;
}

static void cenc_pid_reset_codec_states(GF_CENCStream *cstr)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	if (cstr->av1_state) {
		if (cstr->av1_state->config) gf_odf_av1_cfg_del(cstr->av1_state->config);
		gf_free(cstr->av1_state);
		cstr->av1_state = NULL;
	}
	if (cstr->avc_state) {
		gf_free(cstr->avc_state);
		cstr->avc_state = NULL;
	}
	if (cstr->hevc_state) {
		gf_free(cstr->hevc_state);
		cstr->hevc_state = NULL;
	}
	if (cstr->vvc_state) {
		gf_free(cstr->vvc_state);
		cstr->vvc_state = NULL;
	}
#endif
	if (cstr->vp9_cfg) {
		gf_odf_vp_cfg_del(cstr->vp9_cfg);
		cstr->vp9_cfg = NULL;
	}
}

static GF_Err cenc_enc_configure(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, const char *cfile_name)
{
#if !defined(GPAC_DISABLE_AV_PARSERS)
	u32 i;
#endif
	u32 dsi_crc=0;
	Bool is_reinit=GF_FALSE;
	GF_AVCConfig *avccfg;
	GF_HEVCConfig *hevccfg;
	GF_VVCConfig *vvccfg;
	const GF_PropertyValue *p;

	p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	if (p) {
		dsi_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
	}
	if (dsi_crc != cstr->dsi_crc)
		is_reinit = GF_TRUE;
	else if (!dsi_crc && !cstr->keys[0].crypt)
		is_reinit = GF_TRUE;

	cstr->dsi_crc = dsi_crc;
	if (cstr->is_saes) {
		cstr->tci->crypt_byte_block = 1;
		cstr->tci->skip_byte_block = 9;
	}


	if (is_reinit) {
		const GF_PropertyValue *p2;
		Bool allow_saes=GF_FALSE;
		u32 cenc_codec = CENC_FULL_SAMPLE;
		cstr->nalu_size_length = 0;

		//get CENC media type
		switch (cstr->codec_id) {
		case GF_CODECID_AVC:
			allow_saes=GF_TRUE;
		case GF_CODECID_SVC:
		case GF_CODECID_MVC:
			cenc_codec = CENC_AVC;
			break;
		case GF_CODECID_HEVC:
		case GF_CODECID_LHVC:
			cenc_codec = CENC_HEVC;
			break;
		case GF_CODECID_AV1:
			cenc_codec = CENC_AV1;
			break;
		case GF_CODECID_VP8:
		case GF_CODECID_VP9:
			cenc_codec = CENC_VPX;
			break;
		case GF_CODECID_VVC:
			cenc_codec = CENC_VVC;
			break;
		case GF_CODECID_AAC_MPEG4:
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
			allow_saes=GF_TRUE;
			break;
		}
		if (cstr->is_saes && !allow_saes) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCCrypt] HLS Sample-AES not supported for codec %s\n", gf_codecid_name(cstr->codec_id) ));
			return GF_NOT_SUPPORTED;
		}
		//if change in type, free/realloc parsing contexts
		if (cstr->cenc_codec != cenc_codec) {
			cenc_pid_reset_codec_states(cstr);
			cstr->cenc_codec = cenc_codec;
			switch (cenc_codec) {
			case CENC_AVC:
				GF_SAFEALLOC(cstr->avc_state, AVCState);
				if (!cstr->avc_state) return GF_OUT_OF_MEM;
				break;
			case CENC_HEVC:
				GF_SAFEALLOC(cstr->hevc_state, HEVCState);
				if (!cstr->hevc_state) return GF_OUT_OF_MEM;
				break;
			case CENC_VVC:
				GF_SAFEALLOC(cstr->vvc_state, VVCState);
				if (!cstr->vvc_state) return GF_OUT_OF_MEM;
				break;
			case CENC_AV1:
				GF_SAFEALLOC(cstr->av1_state, AV1State);
				if (!cstr->av1_state) return GF_OUT_OF_MEM;
				break;
			}
		} else {
			is_reinit = GF_FALSE;
		}


		switch (cstr->cenc_codec) {
		case CENC_AVC:
			if (!p)
				return GF_OK;

			avccfg = gf_odf_avc_cfg_read(p->value.data.ptr, p->value.data.size);
			if (avccfg) cstr->nalu_size_length = avccfg->nal_unit_size;

			p2 = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_ISOM_SUBTYPE);
			if (p2 && (p2->value.uint==GF_ISOM_BOX_TYPE_AVC1)) {
				if (!cstr->tci->allow_encrypted_slice_header) {
					cstr->slice_header_clear = GF_TRUE;
				} else if (cstr->tci->scheme_type==GF_CRYPT_TYPE_CBCS) {
					cstr->slice_header_clear = GF_TRUE;
				}
			} else {
				cstr->slice_header_clear = GF_TRUE;
			}
#ifndef GPAC_DISABLE_AV_PARSERS
			if (avccfg) {
				for (i=0; i<gf_list_count(avccfg->sequenceParameterSets); i++) {
					GF_NALUFFParam *slc = gf_list_get(avccfg->sequenceParameterSets, i);
					gf_avc_read_sps(slc->data, slc->size, cstr->avc_state, 0, NULL);
				}
				for (i=0; i<gf_list_count(avccfg->pictureParameterSets); i++) {
					GF_NALUFFParam *slc = gf_list_get(avccfg->pictureParameterSets, i);
					gf_avc_read_pps(slc->data, slc->size, cstr->avc_state);
				}

				gf_odf_avc_cfg_del(avccfg);
			}
#else
			if (avccfg) gf_odf_avc_cfg_del(avccfg);
#endif
			cstr->bytes_in_nal_hdr = 1;

			if (!cstr->slice_header_clear && cstr->tci->clear_bytes)
				cstr->bytes_in_nal_hdr = cstr->tci->clear_bytes;

			if (!cstr->nalu_size_length) {
				cstr->nalu_size_length = 4;
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENCCrypt] Missing NALU length size, assuming 4\n") );
			}
			break;
		case CENC_HEVC:
			if (!p)
				return GF_OK;

			hevccfg = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, (cstr->codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);
			if (hevccfg) cstr->nalu_size_length = hevccfg->nal_unit_size;

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
			gf_hevc_parse_ps(hevccfg, cstr->hevc_state, GF_HEVC_NALU_VID_PARAM);
			gf_hevc_parse_ps(hevccfg, cstr->hevc_state, GF_HEVC_NALU_SEQ_PARAM);
			gf_hevc_parse_ps(hevccfg, cstr->hevc_state, GF_HEVC_NALU_PIC_PARAM);
#endif

			//mandatory for HEVC
			cstr->slice_header_clear = GF_TRUE;

			if (hevccfg) gf_odf_hevc_cfg_del(hevccfg);
			cstr->bytes_in_nal_hdr = 2;

			if (!cstr->nalu_size_length) {
				cstr->nalu_size_length = 4;
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENCCrypt] Missing NALU length size, assuming 4\n") );
			}
			break;
		case CENC_AV1:
			if (!p)
				return GF_OK;

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1)
			cstr->av1_state->config = gf_odf_av1_cfg_read(p->value.data.ptr, p->value.data.size);
			cstr->bytes_in_nal_hdr = 2;
#endif
			//mandatory for AV1
			cstr->slice_header_clear = GF_TRUE;
			break;
		case CENC_VPX:
			if (p) {
				cstr->bytes_in_nal_hdr = 2;
				cstr->vp9_cfg = gf_odf_vp_cfg_new();
			}
			break;
		case CENC_VVC:
			if (!p)
				return GF_OK;

			vvccfg = gf_odf_vvc_cfg_read(p->value.data.ptr, p->value.data.size);
			if (vvccfg) cstr->nalu_size_length = vvccfg->nal_unit_size;

#if !defined(GPAC_DISABLE_AV_PARSERS)
			gf_vvc_parse_ps(vvccfg, cstr->vvc_state, GF_VVC_NALU_VID_PARAM);
			gf_vvc_parse_ps(vvccfg, cstr->vvc_state, GF_VVC_NALU_SEQ_PARAM);
			gf_vvc_parse_ps(vvccfg, cstr->vvc_state, GF_VVC_NALU_PIC_PARAM);
#endif

			//mandatory for VVC
			cstr->slice_header_clear = GF_TRUE;

			if (vvccfg) gf_odf_vvc_cfg_del(vvccfg);
			cstr->bytes_in_nal_hdr = 2;

			if (!cstr->nalu_size_length) {
				cstr->nalu_size_length = 4;
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENCCrypt] Missing NALU length size, assuming 4\n") );
			}
			break;

		default:
			break;
		}
	}

	if (((cstr->tci->scheme_type == GF_CRYPT_TYPE_CENS) || (cstr->tci->scheme_type == GF_CRYPT_TYPE_CBCS) ) && (cstr->cenc_codec>CENC_FULL_SAMPLE) && (cstr->cenc_codec<=CENC_AV1)
	)  {
		if (!cstr->tci->crypt_byte_block || !cstr->tci->skip_byte_block) {
			if (cstr->tci->crypt_byte_block || cstr->tci->skip_byte_block) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Using pattern mode, crypt_byte_block and skip_byte_block shall be 0 only for track other than video, using 1 crypt + 9 skip\n"));
			}
			cstr->tci->crypt_byte_block = 1;
			cstr->tci->skip_byte_block = 9;
		}
	}

	if ( (cstr->cenc_codec==CENC_VPX) && (cstr->tci->scheme_type != GF_CRYPT_TYPE_CENC) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Encryption mode %s is not supported with VP8/VP9, only cenc is\n", gf_4cc_to_str(cstr->tci->scheme_type) ));
		return GF_NOT_SUPPORTED;
	}


	cstr->use_subsamples = GF_FALSE;
	if (cstr->cenc_codec != CENC_FULL_SAMPLE)
		cstr->use_subsamples = GF_TRUE;
	//CBCS mode with skip byte block may be used for any track, in which case we need subsamples
	else if (cstr->tci->scheme_type == GF_CRYPT_TYPE_CBCS) {
		const GF_PropertyValue *prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_STREAM_TYPE);
		if (prop && prop->value.uint != GF_STREAM_VISUAL) {
			if (cstr->tci->skip_byte_block) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("\n[CENC] Using cbcs pattern mode on-video track is disabled in GPAC, using whole-block full encryption\n"));
				cstr->tci->skip_byte_block = 0;
			}
		}
		if (cstr->tci->skip_byte_block) {
			cstr->use_subsamples = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("\n[CENC] Using cbcs pattern mode on non NAL video track, this may not be supported by most devices; consider setting skip_byte_block to 0\n\n"));
			//cbcs allows bytes of clear data
			cstr->bytes_in_nal_hdr = cstr->tci->clear_bytes;
		}
		//This is not clear in the spec, setting skip and crypt to 0 means no pattern, in which case tenc version shall be 0
		//but cbcs asks for 1 - needs further clarification
#if 0
		//setup defaults
		else if (!cstr->crypt_byte_block) {
			cstr->tci->crypt_byte_block = 1;
		}
#else
		else {
			cstr->tci->crypt_byte_block = 0;
		}
#endif
	}
	else if (cstr->tci->scheme_type == GF_CRYPT_TYPE_CENS) {
		if (cstr->tci->skip_byte_block) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Using cens pattern mode on non NAL video track not allowed, forcing skip_byte_block to 0\n"));
			cstr->tci->skip_byte_block = 0;
			if (!cstr->tci->crypt_byte_block) {
				cstr->tci->crypt_byte_block = 1;
			}
		}
	}

	//setup crypto context, disabling multikey if the stream doesn't use subsample
	if (!cstr->keys) {
		if (!cstr->use_subsamples)
			cstr->multi_key = GF_FALSE;

		if (cstr->multi_key) {
			cstr->nb_keys = cstr->tci->nb_keys;
		} else {
			cstr->nb_keys = 1;
		}
		cstr->keys = gf_malloc(sizeof(CENC_MKey) * cstr->nb_keys);
		if (!cstr->keys) return GF_OUT_OF_MEM;
		memset(cstr->keys, 0, sizeof(CENC_MKey) * cstr->nb_keys);
	}
	//init rand
	if ((cstr->tci->sel_enc_type==GF_CRYPT_SELENC_RAND)
		|| (cstr->tci->sel_enc_type==GF_CRYPT_SELENC_RAND_RANGE)
		|| cstr->tci->subs_rand
	) {
		gf_rand_init(GF_TRUE);
	}

	//configure roll rap
	if (cstr->tci->roll_type==GF_KEYROLL_SAPS) {
		cstr->rap_roll = GF_TRUE;
		//if no subsample, only use key roll "rap" if codec has sync point
		if (!cstr->use_subsamples) {
			p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_HAS_SYNC);
			if (p && p->value.uint) {
				cstr->rap_roll = GF_TRUE;
			} else {
				switch (cstr->codec_id) {
				case GF_CODECID_USAC:
					cstr->rap_roll = GF_TRUE;
					break;
				}
			}
		} else {
			cstr->rap_roll = GF_TRUE;
		}
	}
	if (cstr->multi_key && !cstr->mkey_indices.nb_items && cstr->tci->mkey_subs) {
		GF_PropertyValue pval = gf_props_parse_value(GF_PROP_UINT_LIST, "subs", cstr->tci->mkey_subs, NULL, ',');
		cstr->mkey_indices = pval.value.uint_list;
	}

	if (is_reinit) {
		u32 nb_keys;

		/*select key*/
		if (!cstr->tci->keys) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] No key specified\n"));
			return GF_BAD_PARAM;
		}

		nb_keys = cstr->multi_key ? cstr->tci->nb_keys : 1;
		for (i=0; i<nb_keys; i++) {

			if (cstr->keys[i].crypt) gf_crypt_close(cstr->keys[i].crypt);
			if (cstr->ctr_mode) {
				cstr->keys[i].crypt = gf_crypt_open(GF_AES_128, GF_CTR);
			}
			else {
				cstr->keys[i].crypt = gf_crypt_open(GF_AES_128, GF_CBC);
			}
			if (!cstr->keys[i].crypt) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot open AES-128 %s\n", cstr->ctr_mode ? "CTR" : "CBC"));
				return GF_IO_ERR;
			}

			if (nb_keys>1) {
				memcpy(cstr->keys[i].key, cstr->tci->keys[i].key, 16);
				cstr->kidx = 0;
			} else {
				if (cstr->tci->defaultKeyIdx && (cstr->tci->defaultKeyIdx < cstr->tci->nb_keys)) {
					cstr->kidx = cstr->tci->defaultKeyIdx;
				} else {
					cstr->kidx = 0;
				}
				memcpy(cstr->keys[0].key, cstr->tci->keys[cstr->kidx].key, 16);
			}
			GF_Err e = gf_crypt_init(cstr->keys[i].crypt, cstr->keys[i].key, cstr->tci->keys[i].IV);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot initialize AES-128 %s\n", cstr->ctr_mode ? "CTR" : "CBC"));
				return e;
			}
		}

		cstr->prev_pck_encrypted = cstr->tci->IsEncrypted;
	}

	/*set CENC protection properties*/

	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, &PROP_UINT(0x00010000) );
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ENCRYPTED, &PROP_BOOL(cstr->tci->IsEncrypted ? GF_TRUE : GF_FALSE) );

	u8 *data;
	u32 size;
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (cstr->multi_key) {
		gf_bs_write_u8(bs, 1);
		gf_bs_write_u16(bs, cstr->tci->nb_keys);

		for (i=0; i<cstr->tci->nb_keys; i++) {
			GF_CryptKeyInfo *ki = &cstr->tci->keys[i];
			gf_bs_write_u8(bs, ki->constant_IV_size ? 0 : ki->IV_size);
			gf_bs_write_data(bs, ki->KID, 16);
			if (ki->constant_IV_size) {
				gf_bs_write_u8(bs, ki->constant_IV_size);
				gf_bs_write_data(bs, ki->IV, ki->constant_IV_size);
			}
		}
	} else {
		GF_CryptKeyInfo *ki = &cstr->tci->keys[cstr->kidx];
		gf_bs_write_u8(bs, 0);
		gf_bs_write_u16(bs, 0);
		gf_bs_write_u8(bs, ki->constant_IV_size ? 0 : ki->IV_size);
		gf_bs_write_data(bs, ki->KID, 16);
		if (ki->constant_IV_size) {
			gf_bs_write_u8(bs, ki->constant_IV_size);
			gf_bs_write_data(bs, ki->IV, ki->constant_IV_size);
		}
	}
	gf_bs_get_content(bs, &data, &size);
	gf_bs_del(bs);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_KEY_INFO, &PROP_DATA_NO_COPY(data, size) );

	if (!cstr->multi_key) {
		char *hls_info = cstr->tci->keys[0].hls_info;
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_HLS_KMS, hls_info ? &PROP_STRING(hls_info) : NULL);
	}

	if (cstr->tci->skip_byte_block || cstr->tci->crypt_byte_block) {
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_PATTERN, &PROP_FRAC_INT(cstr->tci->skip_byte_block, cstr->tci->crypt_byte_block ) );
	} else {
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_PATTERN, NULL);
	}

	//if constantIV and not using CENC subsample, no CENC auxiliary info
	if (!cstr->tci->keys[0].constant_IV_size || cstr->use_subsamples) {
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_STORE, &PROP_4CC(cstr->tci->sai_saved_box_type) );
	}

	//parse pssh even if reinit since we need to reassign pssh property
	return cenc_parse_pssh(ctx, cstr, cfile_name);
}


static void cenc_free_pid_context(GF_CENCStream *cstr)
{
	u32 i;
	for (i=0; i<cstr->nb_keys; i++) {
		if (cstr->keys[i].crypt) gf_crypt_close(cstr->keys[i].crypt);
	}
	gf_free(cstr->keys);
	if (cstr->cinfo) gf_crypt_info_del(cstr->cinfo);

	if (cstr->mkey_indices.vals) gf_free(cstr->mkey_indices.vals);
	cenc_pid_reset_codec_states(cstr);
	if (cstr->pssh_templates) {
		while (gf_list_count(cstr->pssh_templates)) {
			GF_XMLNode *n = gf_list_pop_back(cstr->pssh_templates);
			gf_xml_dom_node_del(n);
		}
		gf_list_del(cstr->pssh_templates);
	}
	gf_free(cstr);
}

void cenc_gen_bin128(bin128 data)
{
	u64 *low = (u64 *) &data[0];
	u64 *high = (u64 *) &data[8];

	u64 val = gf_rand();
	val <<= 32;
	val |= gf_rand();
	*low = val;

	val = gf_rand();
	val <<= 32;
	val |= gf_rand();
	*high = val;
}

static GF_Err cenc_enc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_Err e = GF_OK;
	u32 scheme_type = 0;
	GF_TrackCryptInfo *tci = NULL;
	const char *scheme_uri = NULL;
	const char *kms_uri = NULL;
	GF_CryptInfo *cinfo = NULL;
	GF_CENCStream *cstr;
	GF_CENCEncCtx *ctx = (GF_CENCEncCtx *)gf_filter_get_udta(filter);
	const char *cfile_name = ctx->cfile;
	GF_TrackCryptInfo *tci_any = NULL;
	u32 i, count;
	Bool force_clear = GF_FALSE;

	if (is_remove) {
		cstr = gf_filter_pid_get_udta(pid);
		if (cstr) {
			gf_list_del_item(ctx->streams, cstr);
			if (cstr->opid)
				gf_filter_pid_remove(cstr->opid);

			cenc_free_pid_context(cstr);
		}
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CRYPT_INFO);
	if (prop) {
		if (!stricmp(prop->value.string, "clear")) {
			force_clear = GF_TRUE;
		} else {
			cinfo = gf_crypt_info_load(prop->value.string, &e);
			if (!cinfo) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCrypt] failed to load crypt info file %s for pid %s\n", prop->value.string, gf_filter_pid_get_name(pid) ) );
				return e;
			}
			cfile_name = prop->value.string;
		}
	}
	if (!cinfo && !force_clear)
		cinfo = ctx->cinfo;


	if (cinfo) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		count = gf_list_count(cinfo->tcis);
		for (i=0; i<count; i++) {
			tci = gf_list_get(cinfo->tcis, i);
			if (prop && tci->trackID && (tci->trackID==prop->value.uint)) break;
			if (!tci_any && !tci->trackID) tci_any = tci;
			if ((cinfo != ctx->cinfo) && !tci_any) tci_any = tci;
			tci = NULL;
		}
		if (!tci) tci = tci_any;
	} else if (ctx->allc && !force_clear) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCrypt] Missing DRM config file\n") );
		return GF_NOT_SUPPORTED;
	}

	if (!tci) {
		if (cinfo) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENCrypt] Missing track crypt info in DRM config file, PID will not be crypted\n") );
		}
	} else {
		scheme_type = tci->scheme_type;

		switch (scheme_type) {
		case GF_CRYPT_TYPE_ISMA:
		case GF_CRYPT_TYPE_OMA:
		case GF_CRYPT_TYPE_ADOBE:
		case GF_CRYPT_TYPE_CENC:
		case GF_CRYPT_TYPE_CBC1:
		case GF_CRYPT_TYPE_CENS:
		case GF_CRYPT_TYPE_CBCS:
		case GF_CRYPT_TYPE_PIFF:
		case GF_CRYPT_TYPE_SAES:
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCrypt] Unsupported scheme type %s\n", gf_4cc_to_str(scheme_type) ));
			if (cinfo != ctx->cinfo) gf_crypt_info_del(cinfo);
			return GF_NOT_SUPPORTED;
		}
	}

	cstr = gf_filter_pid_get_udta(pid);
	if (!cstr) {
		GF_SAFEALLOC(cstr, GF_CENCStream);
		if (!cstr) return GF_OUT_OF_MEM;
		cstr->ipid = pid;
		cstr->opid = gf_filter_pid_new(filter);
		cstr->tci = tci;
		gf_list_add(ctx->streams, cstr);
		gf_filter_pid_set_udta(pid, cstr);
		//we need full sample
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}
	if (cstr->cinfo) gf_crypt_info_del(cstr->cinfo);
	cstr->cinfo = (cinfo != ctx->cinfo) ? cinfo : NULL;
	cstr->tci = tci;
	cstr->passthrough = tci ? GF_FALSE : GF_TRUE;

	if (tci && tci->rand_keys) {
		if (!tci->nb_keys) {
			tci->keys = gf_realloc(tci->keys, sizeof(GF_CryptKeyInfo));
			memset(&tci->keys[0], 0, sizeof(GF_CryptKeyInfo));
		}
		tci->nb_keys = 1;
		if (tci->scheme_type == GF_CRYPT_TYPE_CBCS) {
			tci->keys[0].IV_size = 0;
			tci->keys[0].constant_IV_size = 16;
		} else {
			if (!tci->keys[0].IV_size) tci->keys[0].IV_size = 16;
			tci->keys[0].constant_IV_size = 0;
		}
		tci->nb_keys = 1;
		gf_rand_init(GF_TRUE);
		cenc_gen_bin128(tci->keys[0].key);
		cenc_gen_bin128(tci->keys[0].KID);
		cenc_gen_bin128(tci->keys[0].IV);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(cstr->opid, pid);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (prop) cstr->codec_id = prop->value.uint;

	if (cstr->tci) {
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, &PROP_4CC(scheme_type) );


		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		if (prop) {
			switch (prop->value.uint) {
			case GF_STREAM_VISUAL:
			case GF_STREAM_AUDIO:
			case GF_STREAM_SCENE:
			case GF_STREAM_FONT:
			case GF_STREAM_TEXT:
				gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ORIG_STREAM_TYPE, & PROP_UINT(prop->value.uint) );
				break;
			default:
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[CENCrypt] Stream type %s cannot be encrypted, using passthrough\n", gf_stream_type_name(prop->value.uint) ));
				cstr->passthrough = GF_TRUE;
				break;
			}
		}
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_STSD_MODE, &PROP_UINT(tci->force_clear_stsd_idx) );
	}
	
	if (cstr->passthrough) return GF_OK;

	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_ENCRYPTED) );

	scheme_uri = cstr->tci->Scheme_URI;
	kms_uri = cstr->tci->KMS_URI;

	/*default to FILE uri*/
	if (!kms_uri || !strlen(kms_uri) ) {
		kms_uri = cfile_name;
	}

	if (!cstr->keys) {
		cstr->multi_key = cstr->tci->multi_key;
		if (cstr->multi_key) {
			switch (scheme_type) {
			case GF_CRYPT_TYPE_CENC:
			case GF_CRYPT_TYPE_CENS:
			case GF_CRYPT_TYPE_PIFF:
			case GF_CRYPT_TYPE_CBC1:
			case GF_CRYPT_TYPE_CBCS:
				break;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Multi-key not possible with scheme %s, ignoring\n", gf_4cc_to_str(scheme_type) ));
				cstr->multi_key = 0;
				break;
			}
		}
		if (!cstr->multi_key) {
			cstr->nb_keys = 1;
			cstr->keys = gf_malloc(sizeof(CENC_MKey) * cstr->nb_keys);
			if (!cstr->keys) return GF_OUT_OF_MEM;
			memset(cstr->keys, 0, sizeof(CENC_MKey) * cstr->nb_keys);
		}
	}

	cstr->ctr_mode = GF_FALSE;

	switch (scheme_type) {
	case GF_CRYPT_TYPE_ISMA:
		cstr->ctr_mode = GF_TRUE;
		return isma_enc_configure(ctx, cstr, GF_TRUE, scheme_uri, kms_uri);
	case GF_CRYPT_TYPE_OMA:
		return isma_enc_configure(ctx, cstr, GF_FALSE, scheme_uri, kms_uri);
	case GF_CRYPT_TYPE_ADOBE:
		return adobe_enc_configure(ctx, cstr);
	case GF_CRYPT_TYPE_CENC:
	case GF_CRYPT_TYPE_CENS:
	case GF_CRYPT_TYPE_PIFF:
		cstr->ctr_mode = GF_TRUE;
	case GF_CRYPT_TYPE_CBC1:
	case GF_CRYPT_TYPE_CBCS:
		return cenc_enc_configure(ctx, cstr, cfile_name);
	case GF_CRYPT_TYPE_SAES:
		cstr->is_saes = GF_TRUE;
		return cenc_enc_configure(ctx, cstr, cfile_name);
	default:
		return GF_NOT_SUPPORTED;
	}

	return e;
}


static GF_Err isma_process(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, GF_FilterPacket *pck)
{
	u32 isma_hdr_size=0;
	GF_FilterPacket *dst_pck;
	const u8 *data;
	u8 *output;
	u32 size, rand, flags;
	u64 cts = gf_filter_pck_get_cts(pck);
	u8 sap = gf_filter_pck_get_sap(pck);


	flags = 0;

	switch (cstr->tci->sel_enc_type) {
	case GF_CRYPT_SELENC_RAP:
		if (sap) flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		break;
	case GF_CRYPT_SELENC_NON_RAP:
		if (!sap) flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		break;
	/*random*/
	case GF_CRYPT_SELENC_RAND:
		rand = gf_rand();
		if (rand%2) flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		break;
	/*random every sel_freq samples*/
	case GF_CRYPT_SELENC_RAND_RANGE:
		if (! (cstr->nb_pck % cstr->tci->sel_enc_range) ) cstr->has_crypted_pck = 0;
		if (! cstr->has_crypted_pck) {
			rand = gf_rand();
			if (!(rand % cstr->tci->sel_enc_range)) flags |= GF_ISOM_ISMA_IS_ENCRYPTED;

			if (!(flags & GF_ISOM_ISMA_IS_ENCRYPTED) && !( (1+cstr->nb_pck) % cstr->tci->sel_enc_range)) {
				flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			}
			cstr->has_crypted_pck = (flags & GF_ISOM_ISMA_IS_ENCRYPTED);
		}
		break;
	/*every sel_freq samples*/
	case GF_CRYPT_SELENC_RANGE:
		if (!(cstr->nb_pck % cstr->tci->sel_enc_type)) flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		break;
	case GF_CRYPT_SELENC_PREVIEW:
		if (cts >= cstr->range_end)
			flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		break;
	case 0:
		flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
		break;
	default:
		break;
	}

	if (cstr->tci->sel_enc_type) flags |= GF_ISOM_ISMA_USE_SEL_ENC;

	if (flags & GF_ISOM_ISMA_USE_SEL_ENC) isma_hdr_size = 1;
	if (flags & GF_ISOM_ISMA_IS_ENCRYPTED) isma_hdr_size += cstr->isma_IV_size + cstr->KI_length;

	data = gf_filter_pck_get_data(pck, &size);
	if (!data) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] No data associated with packet\n" ));
		return GF_OK;
	}
	dst_pck = gf_filter_pck_new_alloc(cstr->opid, size+isma_hdr_size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;

	memcpy(output+isma_hdr_size, data, sizeof(char)*size);
	gf_filter_pck_merge_properties(pck, dst_pck);

	
	/*isma e&a stores AVC1 in AVC/H264 annex B bitstream fashion, with 0x00000001 start codes*/
	if (cstr->nalu_size_length) {
		u32 done = 0;
		u8 *d = (u8*) output+isma_hdr_size;
		while (done < size) {
			u32 nal_size = GF_4CC(d[0], d[1], d[2], d[3]);
			d[0] = d[1] = d[2] = 0;
			d[3] = 1;
			d += 4 + nal_size;
			done += 4 + nal_size;
		}
	}

	//encrypt
	if (flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
		/*resync IV*/
		if (!cstr->prev_pck_encrypted) {
			char IV[17];
			u64 count;
			u32 remain;
			GF_BitStream *bs;
			count = cstr->BSO / 16;
			remain = (u32) (cstr->BSO % 16);

			/*format IV to begin of counter*/
			bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
			gf_bs_write_u8(bs, 0);	/*begin of counter*/
			gf_bs_write_data(bs, cstr->tci->keys[0].IV, 8);
			gf_bs_write_u64(bs, (s64) count);
			gf_bs_del(bs);
			gf_crypt_set_IV(cstr->keys[0].crypt, IV, GF_AES_128_KEYSIZE+1);

			/*decrypt remain bytes*/
			if (remain) {
				char dummy[20];
				gf_crypt_decrypt(cstr->keys[0].crypt, dummy, remain);
			}
		}
		gf_crypt_encrypt(cstr->keys[0].crypt, output+isma_hdr_size, size);
		cstr->prev_pck_encrypted = GF_TRUE;
	} else {
		cstr->prev_pck_encrypted = GF_FALSE;
	}

	//rewrite ISMA header
	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, isma_hdr_size, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, output, isma_hdr_size);

	if (flags & GF_ISOM_ISMA_USE_SEL_ENC) {
		gf_bs_write_int(ctx->bs_w, (flags & GF_ISOM_ISMA_IS_ENCRYPTED) ? 1 : 0, 1);
		gf_bs_write_int(ctx->bs_w, 0, 7);
	}
	if (flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
		if (cstr->isma_IV_size) gf_bs_write_long_int(ctx->bs_w, (s64) cstr->BSO, 8*cstr->isma_IV_size);
		//not yet implemented
//		if (cstr->KI_length) gf_bs_write_data(ctx->bs_w, (char*) key_indicator, cstr->KI_length);
	}

	cstr->BSO += size;

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}

static GF_Err adobe_process(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, GF_FilterPacket *pck)
{
	Bool is_encrypted = GF_TRUE;
	GF_FilterPacket *dst_pck;
	const u8 *data;
	u8 *output;
	GF_Err e;
	bin128 IV;
	u32 size, adobe_hdr_size;
	u32 len, padding_bytes;
	u8 sap = gf_filter_pck_get_sap(pck);

	switch (cstr->tci->sel_enc_type) {
	case GF_CRYPT_SELENC_RAP:
		if (!sap) is_encrypted = GF_FALSE;
		break;
	case GF_CRYPT_SELENC_NON_RAP:
		if (sap) is_encrypted = GF_FALSE;
		break;
	default:
		break;
	}

	adobe_hdr_size = 1;
	if (is_encrypted) adobe_hdr_size += 16;

	data = gf_filter_pck_get_data(pck, &size);
	if (!data) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] No data associated with packet\n" ));
		return GF_OK;
	}
	padding_bytes = 0;
	len = size;
	if (is_encrypted) {
		padding_bytes = 16 - len % 16;
		len += padding_bytes;
	}
	dst_pck = gf_filter_pck_new_alloc(cstr->opid, len + adobe_hdr_size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;
	memcpy(output+adobe_hdr_size, data, sizeof(char)*size);
	gf_filter_pck_merge_properties(pck, dst_pck);


	if (is_encrypted) {
		if (!cstr->prev_pck_encrypted) {
			memcpy(IV, cstr->tci->keys[0].IV, sizeof(char)*16);
			e = gf_crypt_init(cstr->keys[0].crypt, cstr->keys[0].key, IV);
			cstr->prev_pck_encrypted = GF_TRUE;
		} else {
			cstr->isma_IV_size = 16;
			e = gf_crypt_get_IV(cstr->keys[0].crypt, IV, &cstr->isma_IV_size);
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ADOBE] Cannot initialize AES-128 CBC (%s)\n",  gf_error_to_string(e)) );
			gf_filter_pck_discard(dst_pck);
			return GF_IO_ERR;
		}

		memset(output+adobe_hdr_size+size, padding_bytes, sizeof(char)*padding_bytes);

		gf_crypt_encrypt(cstr->keys[0].crypt, output+adobe_hdr_size, len);

		/*write encrypted AU header*/
		output[0] = 0x10;
		memcpy(output+1, (char *) IV, sizeof(char) * 16);
	} else {
		/*write encrypted AU header*/
		output[0] = 0x0;
	}

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}


/*Common Encryption*/
static void increase_counter(char *x, int x_size) {
	register int i;

	for (i=x_size-1; i>=0; i--) {
		register int y=0;
		if ((u8) x[i] == 0xFF) {
			x[i] = 0;
			y = 1;
		} else x[i]++;

		if (y==0) break;
	}

	return;
}

static void cenc_resync_IV(GF_Crypt *mc, char IV[16], u8 IV_size)
{
	char next_IV[17];
	u32 size = 17;

	gf_crypt_get_IV(mc, (u8 *) next_IV, &size);
	/*
		NOTE 1: the next_IV returned by get_state has 17 bytes, the first byte being the current counter position in the following 16 bytes.
		If this index is 0, this means that we are at the beginning of a new block and we can use it as IV for next sample,
		otherwise we must discard unused bytes in the counter (next sample shall begin with counter at 0)
		if less than 16 blocks were cyphered, we must force increasing the next IV for next sample, not doing so would produce the same IV for the next bytes cyphered,
		which is forbidden by CENC (unique IV per sample). In GPAC, we ALWAYS force counter increase

		NOTE 2: in case where IV_size is 8, because the cypher block is treated as 16 bytes while processing,
		we need to increment manually the 8-bytes IV (bytes 0 to 7) for the next sample, otherwise we would likely use the same IV (eg unless we had cyphered 16 * 2^64 - 1
		bytes in the last sample , quite unlikely !)

		NOTE 3: Bytes 8 to 15 are set to 0 when forcing a new IV for 8-bytes IVs.

		NOTE 4: Since CENC forces declaration of a unique, potentially random, IV per sample, we could increase the IV counter at each sample start
		but this is currently not done
	*/
	if (IV_size == 8) {
		/*cf note 2*/
		increase_counter(&next_IV[1], IV_size);
		next_IV[0] = 0;
		/*cf note 3*/
		memset(&next_IV[9], 0, 8*sizeof(char));
	} else if (next_IV[0]) {
		/*cf note 1*/
		increase_counter(&next_IV[1], IV_size);
		next_IV[0] = 0;
	}

	gf_crypt_set_IV(mc, next_IV, size);

	memset(IV, 0, 16*sizeof(char));
	memcpy(IV, next_IV+1, 16*sizeof(char));
}

#ifndef GPAC_DISABLE_AV_PARSERS
//parses slice header and returns its size
static u32 cenc_get_clear_bytes(GF_CENCStream *cstr, GF_BitStream *plaintext_bs, char *samp_data, u32 nal_size, u32 bytes_in_nalhr)
{
	u32 clear_bytes = 0;

	if (cstr->slice_header_clear) {
		u32 nal_start = (u32) gf_bs_get_position(plaintext_bs);
		if (cstr->cenc_codec==CENC_AVC) {
			u32 ntype;
			gf_avc_parse_nalu(plaintext_bs, cstr->avc_state);
			ntype = cstr->avc_state->last_nal_type_parsed;
			switch (ntype) {
			case GF_AVC_NALU_NON_IDR_SLICE:
			case GF_AVC_NALU_DP_A_SLICE:
			case GF_AVC_NALU_DP_B_SLICE:
			case GF_AVC_NALU_DP_C_SLICE:
			case GF_AVC_NALU_IDR_SLICE:
			case GF_AVC_NALU_SLICE_AUX:
			case GF_AVC_NALU_SVC_SLICE:
				gf_bs_align(plaintext_bs);
				clear_bytes = (u32) gf_bs_get_position(plaintext_bs) - nal_start;
				break;
			default:
				clear_bytes = nal_size;
				break;
			}
			if (cstr->is_saes) {
				if ((ntype == GF_AVC_NALU_NON_IDR_SLICE) || (ntype == GF_AVC_NALU_IDR_SLICE)) {
					clear_bytes = 32;
				} else {
					clear_bytes = nal_size;
				}
			}

		} else if (cstr->cenc_codec==CENC_HEVC) {
#if !defined(GPAC_DISABLE_HEVC)
			u8 ntype, ntid, nlid;
			cstr->hevc_state->full_slice_header_parse = GF_TRUE;
//			gf_hevc_parse_nalu(samp_data + nal_start, nal_size, cstr->hevc_state, &ntype, &ntid, &nlid);
			gf_hevc_parse_nalu_bs(plaintext_bs, cstr->hevc_state, &ntype, &ntid, &nlid);
			if (ntype<=GF_HEVC_NALU_SLICE_CRA) {
				clear_bytes = cstr->hevc_state->s_info.payload_start_offset - nal_start;
			} else {
				clear_bytes = nal_size;
			}
#endif
		} else if (cstr->cenc_codec==CENC_VVC) {
			u8 ntype, ntid, nlid;
			cstr->vvc_state->parse_mode = 1;
//			gf_vvc_parse_nalu(samp_data + nal_start, nal_size, cstr->vvc_state, &ntype, &ntid, &nlid);
			gf_vvc_parse_nalu_bs(plaintext_bs, cstr->vvc_state, &ntype, &ntid, &nlid);
			if (ntype <= GF_VVC_NALU_SLICE_GDR) {
				clear_bytes = cstr->vvc_state->s_info.payload_start_offset - nal_start;
			} else {
				clear_bytes = nal_size;
			}
		}
		//reset EPB removal and seek to start of nal
		gf_bs_enable_emulation_byte_removal(plaintext_bs, GF_FALSE);
		gf_bs_seek(plaintext_bs, nal_start);
	} else {
		clear_bytes = bytes_in_nalhr;
	}
	gf_bs_enable_emulation_byte_removal(plaintext_bs, GF_FALSE);
	return clear_bytes;
}
#endif

static GF_Err cenc_encrypt_packet(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, GF_FilterPacket *pck)
{
	GF_BitStream *sai_bs;
	u32 prev_entry_bytes_clear=0;
	u32 prev_entry_bytes_crypt=0;
	u32 prev_entry_mkey_idx=0;
	u32 pck_size;
	GF_FilterPacket *dst_pck;
	const u8 *data;
	u8 *output;
	u32 sai_size;
	u32 i, nb_iv_init=0, nb_keys, nb_subsamples_bits, sai_size_sub, nb_subsamples=0;
	u32 key_idx = 0;
	u32 subsample_idx = 0;
	u32 nb_subs_crypted = 0;
	u32 nb_sub_offset;
	Bool multi_key;

	if (cstr->multi_key) {
		nb_keys = cstr->tci->nb_keys;
		multi_key = GF_TRUE;
		nb_subsamples_bits = 32;
		sai_size_sub = 8;
	} else {
		nb_keys = 1;
		multi_key = GF_FALSE;
		nb_subsamples_bits = 16;
		sai_size_sub = 6;
	}
	for (i=0; i<nb_keys; i++) {
		if (cstr->tci->keys[i].IV_size) {
			//in cbcs scheme, if Per_Sample_IV_size is not 0 (no constant IV), fetch current IV
			if (!cstr->ctr_mode) {
				u32 IV_size = 16;
				gf_crypt_get_IV(cstr->keys[i].crypt, cstr->keys[i].IV, &IV_size);
			}
			nb_iv_init++;
		}
	}

	data = gf_filter_pck_get_data(pck, &pck_size);

	//CENC can use inplace processing for decryption, SAES cannot
	if (!cstr->is_saes) {
		dst_pck = gf_filter_pck_new_clone(cstr->opid, pck, &output);
	} else {
		dst_pck = gf_filter_pck_new_copy(cstr->opid, pck, &output);
	}
	if (!dst_pck) return GF_OUT_OF_MEM;

	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_crypt_flags(dst_pck, GF_FILTER_PCK_CRYPT);

	if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, pck_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

	sai_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (multi_key) {
		gf_bs_write_u16(sai_bs, nb_iv_init);
		for (i=0; i<nb_keys; i++) {
			//for now we use all our keys in one sample
			if (cstr->tci->keys[i].IV_size) {
				gf_bs_write_u16(sai_bs, i+1);
				gf_bs_write_data(sai_bs, cstr->keys[i].IV, cstr->tci->keys[i].IV_size);
			}
		}
		nb_subsamples_bits = 32;
	} else {
		gf_bs_write_data(sai_bs, cstr->keys[0].IV, cstr->tci->keys[0].IV_size);
	}
	sai_size = nb_sub_offset = (u32) gf_bs_get_position(sai_bs);

	while (gf_bs_available(ctx->bs_r)) {
		GF_Err e=GF_OK;

		if (cstr->use_subsamples) {
#ifndef GPAC_DISABLE_AV_PARSERS
			ObuType obut = 0;
			u32 num_frames_in_superframe = 0, superframe_index_size = 0;
			u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME];
			struct {
				int clear, encrypted;
			} ranges[AV1_MAX_TILE_ROWS * AV1_MAX_TILE_COLS];
			u64 obu_size = 0;
			u32 hdr_size = 0;
#else
			struct {
				int clear, encrypted;
			} ranges[1];
#endif
			u64 pos;
			u32 clear_bytes_at_end = 0;
			u32 clear_bytes = 0;
			u32 nb_ranges = 1;
			u32 range_idx = 0;
			u32 nalu_size = 0;

#ifndef GPAC_DISABLE_AV_PARSERS
			switch (cstr->cenc_codec) {
			case CENC_AVC:
			case CENC_HEVC:
			case CENC_VVC:
				nalu_size = gf_bs_read_int(ctx->bs_r, 8*cstr->nalu_size_length);
				if (nalu_size == 0) {
					continue;
				}
				clear_bytes = cenc_get_clear_bytes(cstr, ctx->bs_r, (char *) data, nalu_size, cstr->bytes_in_nal_hdr);
				break;

			case CENC_AV1:
				pos = gf_bs_get_position(ctx->bs_r);
				e = gf_av1_parse_obu(ctx->bs_r, &obut, &obu_size, &hdr_size, cstr->av1_state);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Failed to parse OBU\n" ));
					return e;
				}
				gf_bs_seek(ctx->bs_r, pos);

				nalu_size = (u32)obu_size;
				switch (obut) {
				//we only encrypt frame and tile group
				case OBU_FRAME:
				case OBU_TILE_GROUP:
					if (!cstr->av1_state->frame_state.nb_tiles_in_obu) {
						clear_bytes = (u32) obu_size;
					} else {
						nb_ranges = cstr->av1_state->frame_state.nb_tiles_in_obu;

						ranges[0].clear = cstr->av1_state->frame_state.tiles[0].obu_start_offset;
						ranges[0].encrypted = cstr->av1_state->frame_state.tiles[0].size;
						for (i = 1; i < nb_ranges; ++i) {
							ranges[i].clear = cstr->av1_state->frame_state.tiles[i].obu_start_offset - (cstr->av1_state->frame_state.tiles[i - 1].obu_start_offset + cstr->av1_state->frame_state.tiles[i - 1].size);
							ranges[i].encrypted = cstr->av1_state->frame_state.tiles[i].size;
						}
						clear_bytes = ranges[0].clear;
						nalu_size = clear_bytes + ranges[0].encrypted;

						/* A subsample SHALL be created for each tile even if less than 16 bytes
							see https://github.com/AOMediaCodec/av1-isobmff/pull/116#discussion_r340176740

						If previous range had encrypted bytes, create a new one, otherwise merge in prev
						*/
						if (prev_entry_bytes_crypt) {
							if (!nb_subsamples) gf_bs_write_int(sai_bs, 0, nb_subsamples_bits);
							nb_subsamples++;
							if (multi_key)
								gf_bs_write_u16(sai_bs, prev_entry_bytes_crypt ? (prev_entry_mkey_idx+1) : 0);
							gf_bs_write_u16(sai_bs, prev_entry_bytes_clear);
							gf_bs_write_u32(sai_bs, prev_entry_bytes_crypt);
							sai_size += sai_size_sub;

							prev_entry_bytes_crypt = 0;
							prev_entry_bytes_clear = 0;
							prev_entry_mkey_idx = 0;
						}
					}
					break;
				default:
					clear_bytes = (u32) obu_size;
					break;
				}
				break;
			case CENC_VPX:
				if (cstr->tci->block_align != 2) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] VP9 mandates that blockAlign=\"always\". Forcing value.\n"));
					cstr->tci->block_align = 2;
				}

				pos = gf_bs_get_position(ctx->bs_r);
				e = gf_vp9_parse_superframe(ctx->bs_r, pck_size, &num_frames_in_superframe, frame_sizes, &superframe_index_size);
				if (e) return e;
				gf_bs_seek(ctx->bs_r, pos);

				nb_ranges = num_frames_in_superframe;

				for (i = 0; i < num_frames_in_superframe; ++i) {
					Bool key_frame;
					u32 width = 0, height = 0, renderWidth = 0, renderHeight = 0;
					u64 pos2 = gf_bs_get_position(ctx->bs_r);
					e = gf_vp9_parse_sample(ctx->bs_r, cstr->vp9_cfg, &key_frame, &width, &height, &renderWidth, &renderHeight);
					if (e) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Error parsing VP9 frame at DTS "LLU"\n", gf_filter_pck_get_dts(pck) ));
						return e;
					}

					ranges[i].clear = (int)(gf_bs_get_position(ctx->bs_r) - pos2);
					ranges[i].encrypted = frame_sizes[i] - ranges[i].clear;

					gf_bs_seek(ctx->bs_r, pos2 + frame_sizes[i]);
				}
				if (gf_bs_get_position(ctx->bs_r) + superframe_index_size != pos + pck_size) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Inconsistent VP9 size %u (parsed "LLU") at DTS "LLU". Re-import raw VP9/IVF for more details.\n",
						pck_size, gf_bs_get_position(ctx->bs_r) + superframe_index_size - pos, gf_filter_pck_get_dts(pck)));
				}
				gf_bs_seek(ctx->bs_r, pos);

				clear_bytes = ranges[0].clear;
				assert(frame_sizes[0] == ranges[0].clear + ranges[0].encrypted);
				nalu_size = frame_sizes[0];

				//final superframe index must be in clear
				if (superframe_index_size > 0) {
					ranges[nb_ranges].clear = superframe_index_size;
					ranges[nb_ranges].encrypted = 0;
					nb_ranges++;
				}

				//not clearly defined in the spec (so we do the same as in AV1 which is more clearly defined):
				if (frame_sizes[0] - clear_bytes >= 16) {
					//A subsample SHALL be created for each tile >= 16 bytes. If previous range had encrypted bytes, create a new one, otherwise merge in prev
					if (prev_entry_bytes_crypt) {
						if (!nb_subsamples) gf_bs_write_int(sai_bs, 0, nb_subsamples_bits);
						nb_subsamples++;
						if (multi_key)
							gf_bs_write_u16(sai_bs, prev_entry_bytes_crypt ? (prev_entry_mkey_idx+1) : 0);
						gf_bs_write_u16(sai_bs, prev_entry_bytes_clear);
						gf_bs_write_u32(sai_bs, prev_entry_bytes_crypt);
						sai_size += sai_size_sub;

						prev_entry_bytes_crypt = 0;
						prev_entry_bytes_clear = 0;
						prev_entry_mkey_idx = 0;
					}
				} else {
					clear_bytes = nalu_size;
				}
				break;
			default:
				//used by cbcs
				clear_bytes = 0;
				break;
			}
#else
			clear_bytes = nalu_size;
#endif

			//VCL subsample, check out settings to decide if we leave it in the clear
			if ((nalu_size > clear_bytes) &&
				(cstr->tci->subs_crypt || cstr->tci->subs_rand || cstr->multi_key)
			) {
				Bool do_crypt = GF_TRUE;

				if (cstr->mkey_indices.nb_items) {
					if (subsample_idx>=cstr->mkey_indices.nb_items)
						do_crypt = GF_FALSE;
					else {
						key_idx = cstr->mkey_indices.vals[subsample_idx];
						if (!key_idx || (key_idx>cstr->nb_keys))
							do_crypt = GF_FALSE;
						else
							key_idx-=1;
					}
				}
				subsample_idx++;
				if (do_crypt && cstr->tci->subs_crypt) {
					char szSub[20], *sep;
					sprintf(szSub, "%d", subsample_idx);
					sep = strstr(cstr->tci->subs_crypt, szSub);
					if (sep) {
						u32 len = (u32) strlen(szSub);
						if ((sep[len]!=',') && sep[len])
							do_crypt = GF_FALSE;
					} else {
						do_crypt = GF_FALSE;
					}
				}
				if (do_crypt && cstr->tci->subs_rand) {
					u32 rndm = gf_rand() % cstr->tci->subs_rand;
					if (rndm)
						do_crypt = GF_FALSE;
				}

				if (!do_crypt) {
					clear_bytes = nalu_size;
				} else {
					nb_subs_crypted++;
				}
			}
			
			while (nb_ranges) {
				if (cstr->ctr_mode) {

					// adjust so that encrypted bytes are a multiple of 16 bytes: cenc SHOULD, cens SHALL, we always do it
					if (nalu_size > clear_bytes) {
						u32 ret = (nalu_size - clear_bytes) % 16;
						//in AV1 always enforced
						if (cstr->cenc_codec==CENC_AV1) {
							clear_bytes += ret;
						}
						//for CENC (should),
						else if ((cstr->tci->scheme_type == GF_CRYPT_TYPE_CENC) || (cstr->tci->scheme_type == GF_CRYPT_TYPE_PIFF))  {
							//do it if not disabled by user
							if (cstr->tci->block_align != 1) {
								//always align even if sample is not encrypted in the end
								if (cstr->tci->block_align==2) {
									clear_bytes += ret;
								}
								//or if we don't end up with sample in the clear
								else if (nalu_size > clear_bytes + ret) {
									clear_bytes += ret;
								}
							}
						} else {
							clear_bytes += ret;
						}
					}
				} else {
					//in cbcs, we don't adjust bytes_encrypted_data to be a multiple of 16 bytes and leave the last block unencrypted
					//except in VPX, where BytesOfProtectedData SHALL end on the last byte of the decode_tile structure
					if ((cstr->cenc_codec != CENC_VPX)
						&& ((cstr->tci->scheme_type == GF_CRYPT_TYPE_CBCS) || cstr->is_saes)
					) {
						u32 ret = (nalu_size - clear_bytes) % 16;
						clear_bytes_at_end = ret;
					}
					//in cbc1 or cbcs+VPX, we adjust bytes_encrypted_data to be a multiple of 16 bytes
					else {
						u32 ret = (nalu_size - clear_bytes) % 16;
						clear_bytes += ret;
						clear_bytes_at_end = 0;
					}

				}

				/*skip bytes of clear data*/
				gf_bs_skip_bytes(ctx->bs_r, clear_bytes);

				//read data to encrypt
				if (nalu_size > clear_bytes) {
					/*get encrypted data start*/
					u32 cur_pos = (u32) gf_bs_get_position(ctx->bs_r);

					/*skip bytes of encrypted data*/
					gf_bs_skip_bytes(ctx->bs_r, nalu_size - clear_bytes);

					//cbcs scheme with constant IV, reinit at each sub sample,
					if (!cstr->ctr_mode && !cstr->tci->keys[key_idx].IV_size)
						gf_crypt_set_IV(cstr->keys[key_idx].crypt, cstr->keys[key_idx].IV, 16);

					//pattern encryption
					if (cstr->tci->crypt_byte_block && cstr->tci->skip_byte_block) {
						u32 res = nalu_size - clear_bytes - clear_bytes_at_end;
						pos = cur_pos;
						assert((res % 16) == 0);

						while (res) {
							e = gf_crypt_encrypt(cstr->keys[key_idx].crypt, output+pos, res >= (u32) (16*cstr->tci->crypt_byte_block) ? 16*cstr->tci->crypt_byte_block : res);
							if (res >= (u32) (16 * (cstr->tci->crypt_byte_block + cstr->tci->skip_byte_block))) {
								pos += 16 * (cstr->tci->crypt_byte_block + cstr->tci->skip_byte_block);
								res -= 16 * (cstr->tci->crypt_byte_block + cstr->tci->skip_byte_block);
							} else {
								res = 0;
							}
						}
					}
					//full subsample encryption
					else {
						//clear_bytes_at_end is 0 unless NALU-based cbcs without pattern (not defined in CENC)
						//in this case, we must only encrypt a multiple of 16-byte blocks
						e = gf_crypt_encrypt(cstr->keys[key_idx].crypt, output+cur_pos, nalu_size - clear_bytes - clear_bytes_at_end);
					}
				}


				//prev entry is not a VCL, append this NAL
				if (!prev_entry_bytes_crypt) {
					prev_entry_bytes_clear += cstr->nalu_size_length + clear_bytes;
					prev_entry_bytes_crypt += nalu_size - clear_bytes;
					if (prev_entry_bytes_crypt)
						prev_entry_mkey_idx = key_idx;
				} else {
					//store current
					if (!nb_subsamples) gf_bs_write_int(sai_bs, 0, nb_subsamples_bits);
					nb_subsamples++;
					if (multi_key)
						gf_bs_write_u16(sai_bs, prev_entry_bytes_crypt ? (prev_entry_mkey_idx+1) : 0);
					gf_bs_write_u16(sai_bs, prev_entry_bytes_clear);
					gf_bs_write_u32(sai_bs, prev_entry_bytes_crypt);
					sai_size += sai_size_sub;

					prev_entry_bytes_clear = cstr->nalu_size_length + clear_bytes;
					prev_entry_bytes_crypt = nalu_size - clear_bytes;
					prev_entry_mkey_idx = key_idx;
				}

				//subsample was a possible encryption target, apply key roll
				if (multi_key && prev_entry_bytes_crypt && !cstr->mkey_indices.nb_items) {
					Bool change_key = GF_FALSE;
					if (!cstr->tci->mkey_roll_plus_one) {
						change_key = GF_TRUE;
					} else if (cstr->tci->mkey_roll_plus_one>1) {
						if (nb_subs_crypted + 1 == cstr->tci->mkey_roll_plus_one) {
							change_key = GF_TRUE;
							nb_subs_crypted = 0;
						}
					}
					if (change_key) {
						key_idx++;
						if (key_idx==cstr->nb_keys)
							key_idx = 0;
					}
				}

				//check bytes of clear is not larger than 16bits
				while (prev_entry_bytes_clear > 0xFFFF) {
					//store current
					if (!nb_subsamples) gf_bs_write_int(sai_bs, 0, nb_subsamples_bits);
					nb_subsamples++;
					if (multi_key)
						gf_bs_write_u16(sai_bs, 0);
					gf_bs_write_u16(sai_bs, 0xFFFF);
					gf_bs_write_u32(sai_bs, 0);
					sai_size += sai_size_sub;

					prev_entry_bytes_clear -= 0xFFFF;
				}


				nb_ranges--;
				if (!nb_ranges) break;

				range_idx++;
				if (range_idx >= AV1_MAX_TILE_ROWS * AV1_MAX_TILE_COLS) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] More ranges than tiles allowed spec, bitstream error ?\n"));
					return GF_BAD_PARAM;
				}
				switch (cstr->cenc_codec) {
				case CENC_AV1:
					clear_bytes = ranges[range_idx].clear;
					nalu_size = clear_bytes + ranges[range_idx].encrypted;
					break;
				case CENC_VPX:
					if (nb_ranges > 1) {
						clear_bytes = ranges[range_idx].clear;
						nalu_size = clear_bytes + ranges[range_idx].encrypted;
					} else { /*last*/
						nalu_size = clear_bytes = ranges[range_idx].clear;
						assert(ranges[range_idx].encrypted == 0);
					}
					break;
				default:
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Unexpected subrange for sample format, only allowed for VPX and AV1\n"));
					return GF_BAD_PARAM;
				}
			}
		}
		//the other cases are NOT multi-key
		//CTR full sample
		else if (cstr->ctr_mode) {
			gf_bs_skip_bytes(ctx->bs_r, pck_size);
			e = gf_crypt_encrypt(cstr->keys[0].crypt, output, pck_size);
		}
		//CBC full sample with padding
		else {
			u32 clear_trailing, clear_header=0;
			//clear SAES header is 16 bytes for all audio codecs (AAC and AC3)
			if (cstr->is_saes)
				clear_header = 16;

			clear_trailing = (pck_size-clear_header) % 16;

			//cbcs scheme with constant IV, reinit at each sample,
			if (!cstr->tci->keys[0].IV_size)
				gf_crypt_set_IV(cstr->keys[0].crypt, cstr->keys[0].IV, 16);

			if (pck_size >= 16) {
				gf_crypt_encrypt(cstr->keys[0].crypt, output+clear_header, pck_size - clear_header - clear_trailing);
			}
			gf_bs_skip_bytes(ctx->bs_r, pck_size);
		}

		if (e) {
			gf_filter_pck_discard(dst_pck);
			return e;
		}
	}
	
	if (prev_entry_bytes_clear || prev_entry_bytes_crypt) {
		if (!nb_subsamples) gf_bs_write_int(sai_bs, 0, nb_subsamples_bits);
		nb_subsamples++;
		if (multi_key)
			gf_bs_write_u16(sai_bs, prev_entry_bytes_crypt ? (prev_entry_mkey_idx+1) : 0);
		gf_bs_write_u16(sai_bs, prev_entry_bytes_clear);
		gf_bs_write_u32(sai_bs, prev_entry_bytes_crypt);
		sai_size += sai_size_sub;
	}
	if (cstr->ctr_mode) {
		for (i=0; i<nb_keys; i++) {
			cenc_resync_IV(cstr->keys[i].crypt, cstr->keys[i].IV, cstr->tci->keys[i].IV_size);
		}
	}

	if (sai_size) {
		u8 *sai=NULL;
		if (cstr->use_subsamples) {
			sai_size = (u32) gf_bs_get_position(sai_bs);
			gf_bs_seek(sai_bs, nb_sub_offset);
			gf_bs_write_int(sai_bs, nb_subsamples, nb_subsamples_bits);
			gf_bs_seek(sai_bs, sai_size);
		}
		sai_size = 0;
		gf_bs_get_content(sai_bs, &sai, &sai_size);

		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CENC_SAI, &PROP_DATA_NO_COPY(sai, sai_size) );
	}
	gf_bs_del(sai_bs);

	if (cstr->is_saes)
		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CENC_SAI, NULL);

	if (cstr->is_saes && (cstr->cenc_codec==CENC_AVC)) {
		u8 *nal;
		u32 epb_add_count = 0;

		gf_bs_reassign_buffer(ctx->bs_r, output, pck_size);
		nal = output;
		while (gf_bs_available(ctx->bs_r)) {
			u32 nalu_size = gf_bs_read_int(ctx->bs_r, 8*cstr->nalu_size_length);
			nal += cstr->nalu_size_length;
			epb_add_count += gf_media_nalu_emulation_bytes_add_count(nal, nalu_size);
			nal += nalu_size;
			gf_bs_skip_bytes(ctx->bs_r, nalu_size);
		}

		if (epb_add_count) {
			u32 w_pos = 0;
			u8 *dst_nal;
			u8 *old_output = gf_malloc(sizeof(u8) * pck_size);
			memcpy(old_output, output, sizeof(u8) * pck_size);
			gf_filter_pck_expand(dst_pck, epb_add_count, &output, NULL, NULL);
			gf_bs_reassign_buffer(ctx->bs_r, old_output, pck_size);
			gf_bs_reassign_buffer(ctx->bs_w, output, pck_size+epb_add_count);
			dst_nal = output;
			nal = old_output;

			while (gf_bs_available(ctx->bs_r)) {
				u32 nalu_size = gf_bs_read_int(ctx->bs_r, 8*cstr->nalu_size_length);
				epb_add_count = gf_media_nalu_emulation_bytes_add_count(nal, nalu_size);
				gf_bs_write_int(ctx->bs_w, nalu_size+epb_add_count, 8*cstr->nalu_size_length);
				nal += cstr->nalu_size_length;
				dst_nal += cstr->nalu_size_length;
				w_pos += 4;

				gf_media_nalu_add_emulation_bytes(nal, dst_nal, nalu_size);
				nal += nalu_size;
				dst_nal += nalu_size+epb_add_count;
				gf_bs_skip_bytes(ctx->bs_r, nalu_size);
				w_pos += nalu_size+epb_add_count;
				gf_bs_seek(ctx->bs_w, w_pos);
			}
			gf_free(old_output);
		}
	}

	if (cstr->pssh_template_plus_one) {
		u32 key_idx = cstr->pssh_template_plus_one-1;
		cstr->pssh_template_plus_one = 0;

		if (cstr->pssh_templates) {
			GF_XMLNode *pssh_tpl=NULL;
			char szKID[33];
			const char *cfile_name = ctx->cfile;
			const GF_PropertyValue *prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CRYPT_INFO);
			if (prop)  cfile_name = prop->value.string;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			u32 nb_systems=0;
			//nb systems
			gf_bs_write_u32(bs, 0);

			szKID[0]=0;
			for (i=0; i<16; i++) {
				char szTmp[3];
				sprintf(szTmp, "%02x", cstr->tci->keys[key_idx].KID[i]);
				strcat(szKID, szTmp);
			}
			i=0;
			while ((pssh_tpl = gf_list_enum(cstr->pssh_templates, &i))) {
				u32 j=0;
				GF_XMLNode *bs_node;
				char szCryptKey[33];
				u32 pssh_version=1;
				u32 crypt_mode=GF_CBC;
				char *key_att_backup = NULL;
				char *kid_att_backup = NULL;
				Bool valid=GF_TRUE;
				GF_XMLAttribute *att, *sys_att=NULL, *key_val = NULL, *key_att = NULL, *kid_att = NULL, *iv_val=NULL;
				while ((att = gf_list_enum(pssh_tpl->attributes, &j))) {
					if (!stricmp(att->name, "system")) {
						sys_att = att;
					}
					else if (!stricmp(att->name, "key")) {
						key_val = att;
					}
					else if (!stricmp(att->name, "mode")) {
						if (strcmp(att->value, "cbc")) crypt_mode = GF_CBC;
						else if (strcmp(att->value, "ctr")) crypt_mode = GF_CTR;
						else if (strcmp(att->value, "ecb")) crypt_mode = GF_ECB;
						else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Unsupported mode %s, ignoring DRMInfoTemplate\n", att->value));
							valid=GF_FALSE;
						}
					}
					else if (!stricmp(att->name, "IV")) {
						iv_val = att;
					}
					else if (!stricmp(att->name, "version")) {
						pssh_version = atoi(att->value);
					}
				}
				if (!valid) continue;

				if (!sys_att) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Missing systemID, ignoring DRMInfoTemplate\n"));
					continue;
				}
				if (!key_val) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Missing keyval, ignoring DRMInfoTemplate\n"));
					continue;
				}

				j=0;
				while ((bs_node = gf_list_enum(pssh_tpl->content, &j))) {
					u32 k=0;
					if (strcmp(bs_node->name, "BS")) continue;
					while ((att = gf_list_enum(bs_node->attributes, &k))) {
						if (stricmp(att->name, "ID128")) continue;
						if (!stricmp(att->value, "KEY"))
							key_att = att;
						else if (!stricmp(att->value, "KID"))
							kid_att = att;
					}
				}

				if (kid_att) {
					kid_att_backup = kid_att->value;
					kid_att->value = szKID;
				}

				if (key_val && key_att) {
					bin128 master_key, leaf_key;
					key_att_backup = key_att->value;
					gf_bin128_parse(key_val->value, master_key);
					GF_Crypt *crypto = gf_crypt_open(GF_AES_128, crypt_mode);
					if (iv_val) {
						bin128 IV;
						gf_bin128_parse(iv_val->value, IV);
						gf_crypt_init(crypto, master_key, IV);
					} else {
						gf_crypt_init(crypto, master_key, NULL);
					}
					memcpy(leaf_key, cstr->tci->keys[key_idx].key, 16);
					gf_crypt_encrypt(crypto, leaf_key, 16);
					gf_crypt_close(crypto);

					szCryptKey[0]=0;
					for (j=0; j<16; j++) {
						char szTmp[3];
						sprintf(szTmp, "%02x", leaf_key[j]);
						strcat(szCryptKey, szTmp);
					}
					key_att->value = szCryptKey;
				}

				u8 *pssh_kid=NULL;
				u32 pssh_kid_size;
				GF_Err e = gf_xml_parse_bit_sequence(pssh_tpl, cfile_name, &pssh_kid, &pssh_kid_size);
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] Failed to get binary XML for PSSH template: %s\n", gf_error_to_string(e) ));
				} else {
					bin128 system_ID;
					gf_bin128_parse(sys_att->value, system_ID);
					gf_bs_write_data(bs, system_ID, 16); //systemID
					gf_bs_write_u32(bs, pssh_version); //version
					if (pssh_version) {
						gf_bs_write_u32(bs, 1); //nb KIDs
						gf_bs_write_data(bs, (const u8 *) cstr->tci->keys[key_idx].KID, 16); //kid
					}
					gf_bs_write_u32(bs, pssh_kid_size);
					gf_bs_write_data(bs, pssh_kid, pssh_kid_size);
					nb_systems++;
				}
				if (pssh_kid) gf_free(pssh_kid);
				if (key_att_backup) key_att->value = key_att_backup;
				if (kid_att_backup) kid_att->value = kid_att_backup;
			}
			u32 pssh_pos = (u32) gf_bs_get_position(bs);
			gf_bs_seek(bs, 0);
			gf_bs_write_u32(bs, nb_systems);
			gf_bs_seek(bs, pssh_pos);
			u8 *pssh;
			u32 pssh_len;
			gf_bs_get_content(bs, &pssh, &pssh_len);
			gf_bs_del(bs);
			gf_filter_pck_set_property(dst_pck, GF_PROP_PID_CENC_PSSH, &PROP_DATA_NO_COPY(pssh, pssh_len) );
		}
	}

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}

static GF_Err cenc_process(GF_CENCEncCtx *ctx, GF_CENCStream *cstr, GF_FilterPacket *pck)
{
	Bool is_encrypted = GF_TRUE;
	const u8 *data;
	GF_Err e;
	Bool all_rap=GF_FALSE;
	u32 pck_size;
	Bool force_clear = GF_FALSE;
	u8 sap = gf_filter_pck_get_sap(pck);

	data = gf_filter_pck_get_data(pck, &pck_size);
	if (!data) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ISMACrypt] No data associated with packet\n" ));
		return GF_OK;
	}

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new((char *) &sap, 1, GF_BITSTREAM_WRITE);

	switch (cstr->tci->sel_enc_type) {
	case GF_CRYPT_SELENC_RAP:
		if (!sap && !all_rap) {
			is_encrypted = GF_FALSE;
		}
		break;
	case GF_CRYPT_SELENC_NON_RAP:
		if (sap || all_rap) {
			is_encrypted = GF_FALSE;
		}
		break;
	case GF_CRYPT_SELENC_CLEAR:
		is_encrypted = GF_FALSE;
		break;
	case GF_CRYPT_SELENC_CLEAR_FORCED:
		is_encrypted = GF_FALSE;
		if (!cstr->multi_key) {
			force_clear = GF_TRUE;
			if (cstr->tci->sel_enc_range && (cstr->nb_pck+1 >= cstr->tci->sel_enc_range)) {
				cstr->tci->sel_enc_type = GF_CRYPT_SELENC_NONE;
			}
		}
		break;
	default:
		break;
	}

	if (!is_encrypted) {
		u8 *sai=NULL;
		u32 i, sai_size = 0;
		Bool signal_sai = GF_FALSE;
		GF_FilterPacket *dst_pck;
		dst_pck = gf_filter_pck_new_ref(cstr->opid, 0, 0, pck);
		if (!dst_pck) return GF_OUT_OF_MEM;
		
		gf_filter_pck_merge_properties(pck, dst_pck);

		if (force_clear && !cstr->tci->force_clear_stsd_idx)
			signal_sai = GF_TRUE;
		//format NULL bitstream only for forced clear mode
		if (cstr->use_subsamples && signal_sai) {
			GF_BitStream *bs;
			u32 subsample_count = 1;
			u32 olen = pck_size;
			while (olen>0xFFFF) {
				olen -= 0xFFFF;
				subsample_count ++;
			}
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (force_clear) {
				bin128 NULL_IV;
				memset(NULL_IV, 0, 16);
				memcpy(NULL_IV, (char *) &cstr->nb_pck, 4);
				gf_bs_write_data(bs, NULL_IV, cstr->tci->keys[0].IV_size);
			}
			gf_bs_write_u16(bs, subsample_count);
			olen = pck_size;
			for (i = 0; i < subsample_count; i++) {
				if (olen<0xFFFF) {
					gf_bs_write_u16(bs, olen);
				} else {
					gf_bs_write_u16(bs, 0xFFFF);
					olen -= 0xFFFF;
				}
				gf_bs_write_u32(bs, 0);
			}
			gf_bs_get_content(bs, &sai, &sai_size);
			gf_bs_del(bs);
		}
		if (sai)
			gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_CENC_SAI, &PROP_DATA_NO_COPY(sai, sai_size) );

		gf_filter_pck_set_crypt_flags(dst_pck, signal_sai ? GF_FILTER_PCK_CRYPT : 0);
		gf_filter_pck_send(dst_pck);
		return GF_OK;
	}

	cstr->pssh_template_plus_one = 0;

	/*load initialization vector for the first sample in track ... */
	if (!cstr->cenc_init) {
		u32 i, nb_keys = cstr->multi_key ? cstr->tci->nb_keys : 1;

		for (i=0; i<nb_keys; i++) {
			//memset IV to 0, and copy from start (0-padding at the end)
			memset(cstr->keys[i].IV, 0, sizeof(char)*16);
			//IV
			if ((cstr->tci->keys[i].IV_size == 8) || (cstr->tci->keys[i].IV_size == 16)) {
				memcpy(cstr->keys[i].IV, cstr->tci->keys[i].IV, sizeof(char) * cstr->tci->keys[i].IV_size);
			}
			//const IV
			else if (!cstr->tci->keys[i].IV_size) {
				if ((cstr->tci->keys[i].constant_IV_size == 8) || (cstr->tci->keys[i].constant_IV_size == 16)) {
					memcpy(cstr->keys[i].IV, cstr->tci->keys[i].IV, sizeof(char) * cstr->tci->keys[i].constant_IV_size);
				} else {
					return GF_NOT_SUPPORTED;
				}
			} else {
				return GF_NOT_SUPPORTED;
			}

			e = gf_crypt_init(cstr->keys[i].crypt, cstr->keys[i].key, cstr->keys[i].IV);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot initialize AES-128 %s (%s)\n", cstr->ctr_mode ? "CTR" : "CBC", gf_error_to_string(e)) );
				return GF_IO_ERR;
			}

			cstr->pssh_template_plus_one = 1;
		}
		cstr->cenc_init = GF_TRUE;
	} else if (!cstr->multi_key) {
		u32 new_idx = cstr->kidx;
		u32 nb_keys = cstr->tci->rand_keys ? 2 : cstr->tci->nb_keys;
		Bool key_changed = GF_FALSE;

		if (cstr->tci->keyRoll) {
			if (cstr->tci->roll_type == GF_KEYROLL_SAMPLES) {
				new_idx = (cstr->nb_pck_encrypted / cstr->tci->keyRoll) % nb_keys;
			} else if (cstr->tci->roll_type == GF_KEYROLL_SEGMENTS) {
				const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
				if (!p) {
					p = gf_filter_pck_get_property(pck, GF_PROP_PCK_CUE_START);
					if (p && !p->value.boolean) p=NULL;
				}
				if (p) {
					cstr->nb_segments++;
					new_idx = (cstr->nb_segments / cstr->tci->keyRoll) % nb_keys;
				}
			} else if (cstr->tci->roll_type == GF_KEYROLL_PERIODS) {
				const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PID_DASH_PERIOD_START);
				if (p) {
					cstr->nb_periods++;
					new_idx = (cstr->nb_periods / cstr->tci->keyRoll) % nb_keys;
				}
			} else if (cstr->rap_roll) {
				if ((sap==GF_FILTER_SAP_1) || (sap==GF_FILTER_SAP_2)) {
					cstr->nb_saps++;
					new_idx = (cstr->nb_saps / cstr->tci->keyRoll) % nb_keys;
				}
			}
		}
		if (cstr->kidx != new_idx) {
			if (cstr->tci->rand_keys) {
				new_idx = 0;
				cenc_gen_bin128(cstr->tci->keys[0].KID);
				cenc_gen_bin128(cstr->tci->keys[0].key);
			}
			cstr->kidx = new_idx;
			memcpy(cstr->keys[0].key, cstr->tci->keys[cstr->kidx].key, 16);
			key_changed = GF_TRUE;
			cstr->pssh_template_plus_one = new_idx+1;
			e = gf_crypt_set_key(cstr->keys[0].crypt, cstr->keys[0].key);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot set key AES-128 %s (%s)\n", cstr->ctr_mode ? "CTR" : "CBC", gf_error_to_string(e)) );
				return e;
			}
		}

		if (key_changed) {
			char *hls_info;
			GF_CryptKeyInfo *ki = &cstr->tci->keys[cstr->kidx];
			u8 key_info[40];
			u32 key_info_size = 20;
			key_info[0] = 0;
			key_info[1] = 0;
			key_info[2] = 0;
			key_info[3] = ki->IV_size;
			memcpy(key_info+4, ki->KID, 16);
			if (!ki->IV_size) {
				key_info[20] = ki->constant_IV_size;
				memcpy(key_info+21, ki->IV, ki->constant_IV_size);
				key_info_size += ki->constant_IV_size + 1;
			}

			gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_KEY_INFO, &PROP_DATA( key_info, key_info_size ) );

			hls_info = cstr->tci->keys[cstr->kidx].hls_info;
			gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_HLS_KMS, hls_info ? &PROP_STRING(hls_info) : NULL);
		}
	}

	e = cenc_encrypt_packet(ctx, cstr, pck);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Error encrypting packet %d in PID %s: %s\n", cstr->nb_pck, gf_filter_pid_get_name(cstr->ipid), gf_error_to_string(e)) );
		return e;
	}

	cstr->nb_pck_encrypted++;
	return GF_OK;
}

static GF_Err cenc_enc_process(GF_Filter *filter)
{
	GF_CENCEncCtx *ctx = (GF_CENCEncCtx *)gf_filter_get_udta(filter);
	u32 i, nb_eos, count = gf_list_count(ctx->streams);

	nb_eos = 0;
	for (i=0; i<count; i++) {
		GF_Err e = GF_OK;;
		GF_CENCStream *cstr = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(cstr->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(cstr->ipid)) {
				gf_filter_pid_set_eos(cstr->opid);
				nb_eos++;
			}
			continue;
		}

		if (cstr->passthrough) {
			gf_filter_pck_forward(pck, cstr->opid);
		}
		else if (cstr->isma_oma) {
			e = isma_process(ctx, cstr, pck);
		} else if (cstr->is_adobe) {
	 		e = adobe_process(ctx, cstr, pck);
		} else {
			e = cenc_process(ctx, cstr, pck);
		}
		gf_filter_pid_drop_packet(cstr->ipid);
		cstr->nb_pck++;

		if (e) return e;
	}
	if (nb_eos==count) return GF_EOS;

	return GF_OK;
}

static GF_Err cenc_enc_initialize(GF_Filter *filter)
{
	GF_CENCEncCtx *ctx = (GF_CENCEncCtx *)gf_filter_get_udta(filter);

	if (ctx->cfile) {
		GF_Err e;
		ctx->cinfo = gf_crypt_info_load(ctx->cfile, &e);
		if (!ctx->cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCCrypt] Cannot load config file %s\n", ctx->cfile ));
			return GF_BAD_PARAM;
		}
	}

	ctx->streams = gf_list_new();
	return GF_OK;
}

static void cenc_enc_finalize(GF_Filter *filter)
{
	GF_CENCEncCtx *ctx = (GF_CENCEncCtx *)gf_filter_get_udta(filter);
	if (ctx->cinfo) gf_crypt_info_del(ctx->cinfo);
	while (gf_list_count(ctx->streams)) {
		GF_CENCStream *s = gf_list_pop_back(ctx->streams);
		cenc_free_pid_context(s);
	}
	gf_list_del(ctx->streams);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
}


static const GF_FilterCapability CENCEncCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_CENCEncCtx, _n)
static const GF_FilterArgs GF_CENCEncArgs[] =
{
	{ OFFS(cfile), "crypt file location", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(allc), "throw error if no DRM config file is found for a PID", GF_PROP_BOOL, NULL, NULL, 0},
	{0}
};

GF_FilterRegister CENCEncRegister = {
	.name = "cecrypt",
	GF_FS_SET_DESCRIPTION("CENC  encryptor")
	GF_FS_SET_HELP("The CENC encryptor supports CENC, ISMA and Adobe encryption. It uses a DRM config file for declaring keys.\n"
	"The syntax is available at https://wiki.gpac.io/Common-Encryption\n"
	"The DRM config file can be set per PID using the property `CryptInfo`, or set at the filter level using [-cfile]().\n"
	"When the DRM config file is set per PID, the first `CrypTrack` in the DRM config file with the same ID is used, otherwise the first `CrypTrack` is used (regardless of the `CrypTrack` ID).\n"
	"When the DRM config file is set globally (not per PID), the first `CrypTrack` in the DRM config file with the same ID is used, otherwise the first `CrypTrack` with ID 0 or not set is used.\n"
	"If no DRM config file is defined for a given PID, this PID will not be encrypted, or an error will be thrown if [-allc]() is specified.\n"
	)
	.private_size = sizeof(GF_CENCEncCtx),
	.max_extra_pids=-1,
	//encryptor shall be explicitly loaded
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.args = GF_CENCEncArgs,
	SETCAPS(CENCEncCaps),
	.configure_pid = cenc_enc_configure_pid,
	.initialize = cenc_enc_initialize,
	.finalize = cenc_enc_finalize,
	.process = cenc_enc_process

};

#endif /*GPAC_DISABLE_CRYPTO*/

const GF_FilterRegister *cenc_encrypt_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_CRYPTO
	return &CENCEncRegister;
#else
	return NULL;
#endif
}
