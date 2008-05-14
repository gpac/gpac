/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre - 2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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



#include <gpac/ismacryp.h>
#include <gpac/xml.h>
#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/crypt.h>


typedef struct 
{
	GF_List *tcis;
	Bool has_common_key;
	Bool in_text_header;
} ISMACrypInfo; 

void isma_ea_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_XMLAttribute *att;
	GF_TrackCryptInfo *tkc;
	u32 i;
	ISMACrypInfo *info = (ISMACrypInfo *)sax_cbck;
	
	if (!strcmp(node_name, "OMATextHeader")) {
		info->in_text_header = 1;
		return;
	}
	if (!strcmp(node_name, "ISMACrypTrack") || !strcmp(node_name, "OMATrack")) {
		GF_SAFEALLOC(tkc, GF_TrackCryptInfo);
		gf_list_add(info->tcis, tkc);

		if (!strcmp(node_name, "OMATrack")) {
			tkc->enc_type = 1;
			/*default to AES 128 in OMA*/
			tkc->encryption = 2;
		}

		for (i=0; i<nb_attributes; i++) {
			att = (GF_XMLAttribute *) &attributes[i];
			if (!stricmp(att->name, "trackID") || !stricmp(att->name, "ID")) {
				if (!strcmp(att->value, "*")) info->has_common_key = 1;
				else tkc->trackID = atoi(att->value);
			}
			else if (!stricmp(att->name, "key")) {
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				if (strlen(sKey) == 32) {
					u32 j;
					for (j=0; j<32; j+=2) {
						u32 v;
						char szV[5];
						sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
						sscanf(szV, "%x", &v);
						tkc->key[j/2] = v;
					}
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISMA E&A] Key is not 16-bytes long - skipping\n"));
				}
			}
			else if (!stricmp(att->name, "salt")) {
				u32 len, j;
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				len = strlen(sKey);
				for (j=0; j<len; j+=2) {
					char szV[5];
					u32 v;
					sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
					sscanf(szV, "%x", &v);
					tkc->salt[j/2] = v;
				}
			}
			else if (!stricmp(att->name, "kms_URI")) strcpy(tkc->KMS_URI, att->value);
			else if (!stricmp(att->name, "rightsIssuerURL")) strcpy(tkc->KMS_URI, att->value);
			else if (!stricmp(att->name, "scheme_URI")) strcpy(tkc->Scheme_URI, att->value);
			else if (!stricmp(att->name, "selectiveType")) {
				if (!stricmp(att->value, "Rap")) tkc->sel_enc_type = GF_ISMACRYP_SELENC_RAP;
				else if (!stricmp(att->value, "Non-Rap")) tkc->sel_enc_type = GF_ISMACRYP_SELENC_NON_RAP;
				else if (!stricmp(att->value, "Rand")) tkc->sel_enc_type = GF_ISMACRYP_SELENC_RAND;
				else if (!strnicmp(att->value, "Rand", 4)) {
					tkc->sel_enc_type = GF_ISMACRYP_SELENC_RAND_RANGE;
					tkc->sel_enc_range = atoi(&att->value[4]);
				}
				else if (sscanf(att->value, "%d", &tkc->sel_enc_range)==1) {
					if (tkc->sel_enc_range==1) tkc->sel_enc_range = 0;
					else tkc->sel_enc_type = GF_ISMACRYP_SELENC_RANGE;
				}
				else if (!strnicmp(att->value, "Preview", 7)) {
					tkc->sel_enc_type = GF_ISMACRYP_SELENC_PREVIEW;
				}
			}
			else if (!stricmp(att->name, "Preview")) {
				tkc->sel_enc_type = GF_ISMACRYP_SELENC_PREVIEW;
				sscanf(att->value, "%d", &tkc->sel_enc_range);
			}
			else if (!stricmp(att->name, "ipmpType")) {
				if (!stricmp(att->value, "None")) tkc->ipmp_type = 0;
				else if (!stricmp(att->value, "IPMP")) tkc->sel_enc_type = 1;
				else if (!stricmp(att->value, "IPMPX")) tkc->sel_enc_type = 2;
			}
			else if (!stricmp(att->name, "ipmpDescriptorID")) tkc->ipmp_desc_id = atoi(att->value);
			else if (!stricmp(att->name, "encryptionMethod")) {
				if (!strcmp(att->value, "AES_128_CBC")) tkc->encryption = 1;
				else if (!strcmp(att->value, "None")) tkc->encryption = 0;
				else if (!strcmp(att->value, "AES_128_CTR") || !strcmp(att->value, "default")) tkc->encryption = 2;
			}
			else if (!stricmp(att->name, "contentID")) strcpy(tkc->Scheme_URI, att->value);
			else if (!stricmp(att->name, "rightsIssuerURL")) strcpy(tkc->KMS_URI, att->value);
			else if (!stricmp(att->name, "transactionID")) {
				if (strlen(att->value)<=16) strcpy(tkc->TransactionID, att->value);
			}
			else if (!stricmp(att->name, "textualHeaders")) {
			}
		}
	}
}

void isma_ea_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	ISMACrypInfo *info = (ISMACrypInfo *)sax_cbck;
	if (!strcmp(node_name, "OMATextHeader")) {
		info->in_text_header = 0;
		return;
	}
}

void isma_ea_text(void *sax_cbck, const char *text, Bool is_cdata)
{
	u32 len;
	GF_TrackCryptInfo *tkc;
	ISMACrypInfo *info = (ISMACrypInfo *)sax_cbck;

	if (!info->in_text_header) return;

	tkc = (GF_TrackCryptInfo *) gf_list_last(info->tcis);
	len = strlen(text);
	if (len+tkc->TextualHeadersLen > 5000) return;

	if (tkc->TextualHeadersLen) {
		tkc->TextualHeadersLen ++;
		tkc->TextualHeaders[tkc->TextualHeadersLen] = 0;
	}

	memcpy(tkc->TextualHeaders + tkc->TextualHeadersLen, text, sizeof(char)*len);
	tkc->TextualHeadersLen += len;
	tkc->TextualHeaders[tkc->TextualHeadersLen] = 0;
}

static void del_crypt_info(ISMACrypInfo *info)
{
	while (gf_list_count(info->tcis)) {
		GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		gf_list_rem_last(info->tcis);
		free(tci);
	}
	gf_list_del(info->tcis);
	free(info);
}

static ISMACrypInfo *load_crypt_file(const char *file)
{
	GF_Err e;
	ISMACrypInfo *info;
	GF_SAXParser *sax;
	GF_SAFEALLOC(info, ISMACrypInfo);
	info->tcis = gf_list_new();
	sax = gf_xml_sax_new(isma_ea_node_start, isma_ea_node_end, isma_ea_text, info);
	e = gf_xml_sax_parse_file(sax, file, NULL);
	gf_xml_sax_del(sax);
	if (e<0) {
		del_crypt_info(info);
		return NULL;
	}
	return info;
}


GF_EXPORT
GF_Err gf_ismacryp_gpac_get_info(u32 stream_id, char *drm_file, char *key, char *salt)
{
	GF_Err e;
	u32 i, count;
	ISMACrypInfo *info;
	GF_TrackCryptInfo *tci;

	e = GF_OK;
	info = load_crypt_file(drm_file);
	if (!info) return GF_NOT_SUPPORTED;
	count = gf_list_count(info->tcis);
	for (i=0; i<count; i++) {
		tci = (GF_TrackCryptInfo *) gf_list_get(info->tcis, i);
		if ((info->has_common_key && !tci->trackID) || (tci->trackID == stream_id) ) {
			memcpy(key, tci->key, sizeof(char)*16);
			memcpy(salt, tci->salt, sizeof(char)*8);
			e = GF_OK;
			break;
		}
	}
	del_crypt_info(info);
	return e;
}

GF_EXPORT
Bool gf_ismacryp_mpeg4ip_get_info(char *kms_uri, char *key, char *salt)
{
	char szPath[1024], catKey[24];
	u32 i, x;
	Bool got_it;
	FILE *kms;
	strcpy(szPath, getenv("HOME"));
	strcat(szPath , "/.kms_data"); 
	got_it = 0;
	kms = fopen(szPath, "r");
	while (kms && !feof(kms)) {
		if (!fgets(szPath, 1024, kms)) break;
		szPath[strlen(szPath) - 1] = 0;
		if (stricmp(szPath, kms_uri)) continue;
		for (i=0; i<24; i++) {
			if (!fscanf(kms, "%x", &x)) break;
			catKey[i] = x;
		}
		if (i==24) got_it = 1;
		break;
	}
	if (kms) fclose(kms);
	if (got_it) {
		/*watchout, MPEG4IP stores SALT|KEY, NOT KEY|SALT*/
		memcpy(key, catKey+8, sizeof(char)*16);
		memcpy(salt, catKey, sizeof(char)*8);
		return 1;
	}
	return 0;
}

#ifndef GPAC_READ_ONLY

static GFINLINE void resync_IV(GF_Crypt *mc, u64 BSO, char *salt)
{
	char IV[17];
	u64 count;
	u32 remain;
	GF_BitStream *bs;
	count = BSO / 16;
	remain = (u32) (BSO % 16);

	/*format IV to begin of counter*/
	bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
	gf_bs_write_u8(bs, 0);	/*begin of counter*/
	gf_bs_write_data(bs, salt, 8);
	gf_bs_write_u64(bs, (s64) count);
	gf_bs_del(bs);
	gf_crypt_set_state(mc, IV, 17);

	/*decrypt remain bytes*/
	if (remain) {
		char dummy[20];
		gf_crypt_decrypt(mc, dummy, remain);
	}
}

GF_EXPORT
GF_Err gf_ismacryp_decrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u32 done, u32 total), void *cbk)
{
	GF_Err e;
	Bool use_sel_enc;
	u32 track, count, i, j, si, is_avc;
	GF_ISOSample *samp;
	GF_ISMASample *ismasamp;
	GF_Crypt *mc;
	unsigned char IV[17];
	u32 IV_size;
	Bool prev_sample_encrypted;
	GF_ESD *esd;

	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	e = gf_isom_get_ismacryp_info(mp4, track, 1, &is_avc, NULL, NULL, NULL, NULL, &use_sel_enc, &IV_size, NULL);
	is_avc = (is_avc==GF_4CC('2','6','4','b')) ? 1 : 0;
		

	mc = gf_crypt_open("AES-128", "CTR");
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot open AES-128 CTR cryptography\n", tci->trackID));
		return GF_IO_ERR;
	}

	memset(IV, 0, sizeof(char)*16);
	memcpy(IV, tci->salt, sizeof(char)*8);
	e = gf_crypt_init(mc, tci->key, 16, IV);
	if (e) {
		gf_crypt_close(mc);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] cannot initialize AES-128 CTR (%s)\n", gf_error_to_string(e)));
		return GF_IO_ERR;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA E&A] Decrypting track ID %d - KMS: %s%s\n", tci->trackID, tci->KMS_URI, use_sel_enc ? " - Selective Decryption" : ""));

	/*start as initialized*/
	prev_sample_encrypted = 1;
	/* decrypt each sample */
	count = gf_isom_get_sample_count(mp4, track);
	for (i = 0; i < count; i++) {
		samp = gf_isom_get_sample(mp4, track, i+1, &si); 
		ismasamp = gf_isom_get_ismacryp_sample(mp4, track, samp, si);

		free(samp->data);
		samp->data = ismasamp->data;
		samp->dataLength = ismasamp->dataLength;
		ismasamp->data = NULL;
		ismasamp->dataLength = 0;

		/* Decrypt payload */
		if (ismasamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
			/*restore IV*/
			if (!prev_sample_encrypted) resync_IV(mc, ismasamp->IV, tci->salt);
			gf_crypt_decrypt(mc, samp->data, samp->dataLength);
		}
		prev_sample_encrypted = (ismasamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED);
		gf_isom_ismacryp_delete_sample(ismasamp);

		/*replace AVC start codes (0x00000001) by nalu size*/
		if (is_avc) {
			u32 nalu_size;
			u32 remain = samp->dataLength;
			char *start, *end;
			start = samp->data;
			end = start + 4;
			while (remain>4) {
				if (!end[0] && !end[1] && !end[2] && (end[3]==0x01)) {
					nalu_size = end - start - 4;
					start[0] = (nalu_size>>24)&0xFF;
					start[1] = (nalu_size>>16)&0xFF;
					start[2] = (nalu_size>>8)&0xFF;
					start[3] = (nalu_size)&0xFF;
					start = end;
					end = start+4;
					continue;
				}
				end++;
				remain--;
			}
			nalu_size = end - start - 4;
			start[0] = (nalu_size>>24)&0xFF;
			start[1] = (nalu_size>>16)&0xFF;
			start[2] = (nalu_size>>8)&0xFF;
			start[3] = (nalu_size)&0xFF;
		}

		gf_isom_update_sample(mp4, track, i+1, samp, 1);
		gf_isom_sample_del(&samp);
		gf_set_progress("ISMA Decrypt", i+1, count);
	}

	gf_crypt_close(mc);
	/*and remove protection info*/
	e = gf_isom_remove_ismacryp_protection(mp4, track, 1);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Error ISMACryp signature from trackID %d: %s\n", tci->trackID, gf_error_to_string(e)));
	}

	/*remove all IPMP ptrs*/
	esd = gf_isom_get_esd(mp4, track, 1);
	if (esd) {
		while (gf_list_count(esd->IPMPDescriptorPointers)) {
			GF_Descriptor *d = (GF_Descriptor *)gf_list_get(esd->IPMPDescriptorPointers, 0);
			gf_list_rem(esd->IPMPDescriptorPointers, 0);
			gf_odf_desc_del(d);
		}
		gf_isom_change_mpeg4_description(mp4, track, 1, esd);
		gf_odf_desc_del((GF_Descriptor *)esd);
	}

	/*update OD track if any*/
	track = 0;
	for (i=0; i<gf_isom_get_track_count(mp4); i++) {
		GF_ODCodec *cod;
		if (gf_isom_get_media_type(mp4, i+1) != GF_ISOM_MEDIA_OD) continue;

		/*remove all IPMPUpdate commads...*/
		samp = gf_isom_get_sample(mp4, i+1, 1, &si);
		cod = gf_odf_codec_new();
		gf_odf_codec_set_au(cod, samp->data, samp->dataLength);
		gf_odf_codec_decode(cod);
		for (j=0; j<gf_list_count(cod->CommandList); j++) {
			GF_IPMPUpdate *com = (GF_IPMPUpdate *)gf_list_get(cod->CommandList, j);
			if (com->tag != GF_ODF_IPMP_UPDATE_TAG) continue;
			gf_list_rem(cod->CommandList, j);
			j--;
			gf_odf_com_del((GF_ODCom **)&com);
		}
		free(samp->data);
		samp->data = NULL;
		samp->dataLength = 0;
		gf_odf_codec_encode(cod, 1);
		gf_odf_codec_get_au(cod, &samp->data, &samp->dataLength);
		gf_odf_codec_del(cod);
		gf_isom_update_sample(mp4, i+1, 1, samp, 1);
		gf_isom_sample_del(&samp);

		/*remove IPMPToolList if any*/	
		if (mp4->moov->iods && (mp4->moov->iods->descriptor->tag == GF_ODF_ISOM_IOD_TAG) ) {
			GF_IsomInitialObjectDescriptor *iod = (GF_IsomInitialObjectDescriptor *)mp4->moov->iods->descriptor;
			if (iod->IPMPToolList) gf_odf_desc_del((GF_Descriptor*) iod->IPMPToolList);
			iod->IPMPToolList = NULL;
		}
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_ismacryp_decrypt_file(GF_ISOFile *mp4, const char *drm_file)
{
	GF_Err e;
	u32 i, idx, count, common_idx, nb_tracks, scheme_type, cur_tk;
	const char *scheme_URI, *KMS_URI;
	ISMACrypInfo *info;
	Bool is_oma;
	GF_TrackCryptInfo *a_tci, tci;

	is_oma = 0;
	count = 0;
	info = NULL;
	if (drm_file) {
		info = load_crypt_file(drm_file);
		if (!info) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot open or validate xml file %s\n", drm_file));
			return GF_NOT_SUPPORTED;
		}
		count = gf_list_count(info->tcis);
	}

	common_idx=0;
	if (info && info->has_common_key) {
		for (common_idx=0; common_idx<count; common_idx++) {
			a_tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, common_idx);
			if (!a_tci->trackID) break;
		}
	}
	
	nb_tracks = gf_isom_get_track_count(mp4);
	cur_tk = 1;
	e = GF_OK;
	for (i=0; i<nb_tracks; i++) {
		u32 trackID = gf_isom_get_track_id(mp4, i+1);
		scheme_type = gf_isom_is_media_encrypted(mp4, i+1, 1);
		if (!scheme_type) continue;

		for (idx=0; idx<count; idx++) {
			a_tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
			if (a_tci->trackID == trackID) break;
		}
		if (idx==count) {
			if (!drm_file || info->has_common_key) idx = common_idx;
			/*no available KMS info for this track*/
			else continue;
		}
		if (count) {
			a_tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
			memcpy(&tci, a_tci, sizeof(GF_TrackCryptInfo));
		} else {
			memset(&tci, 0, sizeof(GF_TrackCryptInfo));
			tci.trackID = trackID;
		}

		if (gf_isom_is_ismacryp_media(mp4, i+1, 1)) {
			e = gf_isom_get_ismacryp_info(mp4, i+1, 1, NULL, &scheme_type, NULL, &scheme_URI, &KMS_URI, NULL, NULL, NULL);
		} else if (gf_isom_is_omadrm_media(mp4, i+1, 1)) {
			if (!drm_file) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot decrypt OMA (P)DCF file without GPAC's DRM file & keys\n"));
				continue;
			}
			KMS_URI = "OMA DRM";
			is_oma = 1;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISMA E&A] TrackID %d encrypted with unknown scheme %s - skipping\n", trackID, gf_4cc_to_str(scheme_type) ));
			continue;
		}
		/*get key and salt from KMS*/
		/*GPAC*/
		if (!strnicmp(KMS_URI, "(key)", 5)) {
			char data[100];
			gf_base64_decode((char*)KMS_URI+5, strlen(KMS_URI)-5, data, 100);
			memcpy(tci.key, data, sizeof(char)*16);
			memcpy(tci.salt, data+16, sizeof(char)*8);
		}
		/*MPEG4IP*/
		else if (!stricmp(KMS_URI, "AudioKey") || !stricmp(KMS_URI, "VideoKey")) {
			if (!gf_ismacryp_mpeg4ip_get_info((char *) KMS_URI, tci.key, tci.salt)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Couldn't load MPEG4IP ISMACryp keys for TrackID %d\n", trackID));
				continue;
			}
		} else if (!drm_file) {
			FILE *test = NULL;
			if (!stricmp(scheme_URI, "urn:gpac:isma:encryption_scheme")) test = fopen(KMS_URI, "rt");

			if (!test) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA E&A] TrackID %d does not contain decryption keys - skipping\n", trackID));
				continue;
			}
			fclose(test);
			if (gf_ismacryp_gpac_get_info(tci.trackID, (char *) KMS_URI, tci.key, tci.salt) != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Couldn't load TrackID %d keys in GPAC DRM file %s\n", tci.trackID, KMS_URI));
				continue;
			}
		}

		if (KMS_URI && strlen(tci.KMS_URI) && strcmp(KMS_URI, tci.KMS_URI) )
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISMA E&A] KMS URI for trackID %d Mismatch\n", trackID));

		if (drm_file || (KMS_URI && strncmp(KMS_URI, "(key)", 5)) ) {
			strcpy(tci.KMS_URI, KMS_URI ? KMS_URI : "");
		} else {
			strcpy(tci.KMS_URI, "self-contained");
		}
		e = gf_ismacryp_decrypt_track(mp4, &tci, NULL, NULL);
		if (e) break;
	}
	if (is_oma) {
		e = gf_isom_set_brand_info(mp4, GF_4CC('i','s','o','2'), 0x00000001);
		if (!e) e = gf_isom_modify_alternate_brand(mp4, GF_4CC('o','d','c','f'), 0);
	}
	if (info) del_crypt_info(info);
	return e;
}

GF_EXPORT
GF_Err gf_ismacryp_encrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u32 done, u32 total), void *cbk)
{
	char IV[16];
	GF_ISOSample *samp;
	GF_ISMASample *isamp;
	GF_Crypt *mc;
	u32 i, count, di, track, IV_size, rand, avc_size_length;
	u64 BSO, range_end;
	GF_ESD *esd;
	GF_IPMPPtr *ipmpdp;
	GF_IPMP_Descriptor *ipmpd;
	GF_IPMPUpdate *ipmpdU;
	GF_IPMPX_ISMACryp *ismac;
	GF_Err e;
	Bool prev_sample_encryped, has_crypted_samp;

	avc_size_length = 0;
	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot find TrackID %d in input file - skipping\n", tci->trackID));
		return GF_OK;
	}
	esd = gf_isom_get_esd(mp4, track, 1);
	if (esd && (esd->decoderConfig->streamType==GF_STREAM_OD)) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot encrypt OD tracks - skipping"));
		return GF_NOT_SUPPORTED;
	}
	if (esd) {
		if (esd->decoderConfig->objectTypeIndication==0x21) avc_size_length = 1;
		gf_odf_desc_del((GF_Descriptor*) esd);
	}
	if (avc_size_length) {
		GF_AVCConfig *avccfg = gf_isom_avc_config_get(mp4, track, 1);
		avc_size_length = avccfg->nal_unit_size;
		gf_odf_avc_cfg_del(avccfg);
		if (avc_size_length != 4) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot encrypt AVC/H264 track with %d size_length field - onmy 4 supported\n", avc_size_length));
			return GF_NOT_SUPPORTED;
		}
	}

	if (!tci->enc_type && !strlen(tci->Scheme_URI)) strcpy(tci->Scheme_URI, "urn:gpac:isma:encryption_scheme");

	if (!gf_isom_has_sync_points(mp4, track) &&
	((tci->sel_enc_type==GF_ISMACRYP_SELENC_RAP) || (tci->sel_enc_type==GF_ISMACRYP_SELENC_NON_RAP)) ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISMA E&A] All samples in trackID %d are random access - disabling selective encryption\n", tci->trackID));
		tci->sel_enc_type = GF_ISMACRYP_SELENC_NONE;
	}
	else if ((tci->sel_enc_type==GF_ISMACRYP_SELENC_RAND) || (tci->sel_enc_type==GF_ISMACRYP_SELENC_RAND_RANGE)) {
		gf_rand_init(1);
	}

	BSO = gf_isom_get_media_data_size(mp4, track);
	if (tci->enc_type==0) {
		if (BSO<0xFFFF) IV_size = 2;
		else if (BSO<0xFFFFFFFF) IV_size = 4;
		else IV_size = 8;
	} else {
		/*128 bit IV in OMA*/
		IV_size = 16;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA E&A] Encrypting track ID %d - KMS: %s%s\n", tci->trackID, tci->KMS_URI, tci->sel_enc_type ? " - Selective Encryption" : ""));

	/*init crypto*/
	memset(IV, 0, sizeof(char)*16);
	memcpy(IV, tci->salt, sizeof(char)*8);
	mc = gf_crypt_open("AES-128", "CTR");
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot open AES-128 CTR\n"));
		return GF_IO_ERR;
	}
	e = gf_crypt_init(mc, tci->key, 16, IV);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot initialize AES-128 CTR (%s)\n", gf_error_to_string(e)) );
		gf_crypt_close(mc);
		return GF_IO_ERR;
	}
	if (!stricmp(tci->KMS_URI, "self")) {
		char Data[100], d64[100];
		u32 s64;
		memcpy(Data, tci->key, sizeof(char)*16);
		memcpy(Data+16, tci->salt, sizeof(char)*8);
		s64 = gf_base64_encode(Data, 24, d64, 100);
		d64[s64] = 0;
		strcpy(tci->KMS_URI, "(key)");
		strcat(tci->KMS_URI, d64);
	}

	/*create ISMA protection*/
	if (tci->enc_type==0) {
		e = gf_isom_set_ismacryp_protection(mp4, track, 1, GF_ISOM_ISMACRYP_SCHEME, 1, 
			tci->Scheme_URI, tci->KMS_URI, (tci->sel_enc_type!=0) ? 1 : 0, 0, IV_size);	 
	} else {
		if ((tci->sel_enc_type==GF_ISMACRYP_SELENC_PREVIEW) && tci->sel_enc_range) {
			char *szPreview = tci->TextualHeaders + tci->TextualHeadersLen;
			sprintf(szPreview, "PreviewRange:%d", tci->sel_enc_range);
			tci->TextualHeadersLen += strlen(szPreview)+1;
		}
		e = gf_isom_set_oma_protection(mp4, track, 1, 
			strlen(tci->Scheme_URI) ? tci->Scheme_URI : NULL,
			tci->KMS_URI, 
			tci->encryption, BSO, 
			tci->TextualHeadersLen ? tci->TextualHeaders : NULL, 
			tci->TextualHeadersLen,
			(tci->sel_enc_type!=0) ? 1 : 0, 0, IV_size);
	}
	if (e) return e;

	has_crypted_samp = 0;
	BSO = 0;
	prev_sample_encryped = 1;
	range_end = 0;
	if (tci->sel_enc_type==GF_ISMACRYP_SELENC_PREVIEW) {
		range_end = gf_isom_get_media_timescale(mp4, track) * tci->sel_enc_range;
	}

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, 1);

	count = gf_isom_get_sample_count(mp4, track);
	for (i = 0; i < count; i++) {
		samp = gf_isom_get_sample(mp4, track, i+1, &di); 

		isamp = gf_isom_ismacryp_new_sample();
		isamp->IV_length = IV_size;
		isamp->KI_length = 0;

		switch (tci->sel_enc_type) {
		case GF_ISMACRYP_SELENC_RAP:
			if (samp->IsRAP) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		case GF_ISMACRYP_SELENC_NON_RAP:
			if (!samp->IsRAP) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		/*random*/
		case GF_ISMACRYP_SELENC_RAND:
			rand = gf_rand();
			if (rand%2) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		/*random every sel_freq samples*/
		case GF_ISMACRYP_SELENC_RAND_RANGE:
			if (!(i%tci->sel_enc_range)) has_crypted_samp = 0;
			if (!has_crypted_samp) {
				rand = gf_rand();
				if (!(rand%tci->sel_enc_range)) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;

				if (!(isamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) && !( (1+i)%tci->sel_enc_range)) {
					isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
				}
				has_crypted_samp = (isamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED);
			}
			break;
		/*every sel_freq samples*/
		case GF_ISMACRYP_SELENC_RANGE:
			if (!(i%tci->sel_enc_type)) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		case GF_ISMACRYP_SELENC_PREVIEW:
			if (samp->DTS + samp->CTS_Offset >= range_end) 
				isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		case 0:
			isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		default:
			break;
		}
		if (tci->sel_enc_type) isamp->flags |= GF_ISOM_ISMA_USE_SEL_ENC;

		/*isma e&a stores AVC1 in AVC/H264 annex B bitstream fashion, with 0x00000001 start codes*/
		if (avc_size_length) {
			u32 done = 0;
			u8 *d = samp->data;
			while (done < samp->dataLength) {
				u32 nal_size = GF_4CC(d[0], d[1], d[2], d[3]);
				d[0] = d[1] = d[2] = 0; d[3] = 1;
				d += 4 + nal_size;
				done += 4 + nal_size;
			}
		}

		if (isamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
			/*resync IV*/
			if (!prev_sample_encryped) resync_IV(mc, BSO, tci->salt);
			gf_crypt_encrypt(mc, samp->data, samp->dataLength);
			prev_sample_encryped = 1;
		} else {
			prev_sample_encryped = 0;
		}


		isamp->IV = BSO;
		BSO += samp->dataLength;
		isamp->data = samp->data;
		isamp->dataLength = samp->dataLength;
		samp->data = NULL;
		samp->dataLength = 0;

		gf_isom_ismacryp_sample_to_sample(isamp, samp);
		gf_isom_ismacryp_delete_sample(isamp);
		gf_isom_update_sample(mp4, track, i+1, samp, 1);
		gf_isom_sample_del(&samp);
		gf_set_progress("ISMA Encrypt", i+1, count);
	}
	gf_isom_set_cts_packing(mp4, track, 0);
	gf_crypt_close(mc);


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
		ipmpd->IPMP_DescriptorID = 0xFF;
		ipmpd->IPMP_DescriptorIDEx = tci->ipmp_desc_id;
		ipmpd->IPMPS_Type = 0xFFFF;
		ipmpd->IPMP_ToolID[14] = 0x49; ipmpd->IPMP_ToolID[15] = 0x53;
		ipmpd->control_point = 1;
		ipmpd->cp_sequence_code = 0x80;
		/*format IPMPXData*/
		ismac = (GF_IPMPX_ISMACryp *) gf_ipmpx_data_new(GF_IPMPX_ISMACRYP_TAG);
		ismac->cryptoSuite = 1;	/*default ISMA AESCTR128*/
		ismac->IV_length = IV_size;
		ismac->key_indicator_length = 0;
		ismac->use_selective_encryption = (tci->sel_enc_type!=0)? 1 : 0;
		gf_list_add(ipmpd->ipmpx_data, ismac);
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
		free(samp->data);
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
			ipmpt->IPMP_ToolID[14] = 0x49; ipmpt->IPMP_ToolID[15] = 0x53;
			gf_isom_add_desc_to_root_od(mp4, (GF_Descriptor *)ipmptl);
			gf_odf_desc_del((GF_Descriptor *)ipmptl);
		}
		break;
	}
	return e;
}

GF_EXPORT
GF_Err gf_ismacryp_crypt_file(GF_ISOFile *mp4, const char *drm_file)
{
	GF_Err e;
	u32 i, count, nb_tracks, common_idx, idx;
	ISMACrypInfo *info;
	Bool is_oma;
	GF_TrackCryptInfo *tci;

	is_oma = 0;

	info = load_crypt_file(drm_file);
	if (!info) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA E&A] Cannot open or validate xml file %s\n", drm_file));
		return GF_NOT_SUPPORTED;
	}
	e = GF_OK;
	count = gf_list_count(info->tcis);

	common_idx=0;
	if (info && info->has_common_key) {
		for (common_idx=0; common_idx<count; common_idx++) {
			tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, common_idx);
			if (!tci->trackID) break;
		}
	}
	nb_tracks = gf_isom_get_track_count(mp4);
	for (i=0; i<nb_tracks; i++) {
		u32 trackID = gf_isom_get_track_id(mp4, i+1);
		for (idx=0; idx<count; idx++) {
			tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
			if (tci->trackID==trackID) break;
		}
		if (idx==count) {
			if (!info->has_common_key) continue;
			idx = common_idx;
		}
		tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);

		/*default to FILE uri*/
		if (!strlen(tci->KMS_URI)) strcpy(tci->KMS_URI, drm_file);

		e = gf_ismacryp_encrypt_track(mp4, tci, NULL, NULL);
		if (e) break;

		if (tci->enc_type==1) is_oma = 1;
	}

	if (is_oma) {
#if 0
		/*set as OMA V2*/
		e = gf_isom_set_brand_info(mp4, GF_4CC('o','d','c','f'), 0x00000002);
		gf_isom_reset_alt_brands(mp4);
#else
		e = gf_isom_modify_alternate_brand(mp4, GF_4CC('o','p','f','2'), 1);
#endif
	}

	del_crypt_info(info);
	return e;
}

#endif

GF_EXPORT
GF_Err gf_media_get_file_hash(const char *file, u8 hash[20]) 
{
	u8 block[1024];
	u32 read;
	u64 size, tot;
	FILE *in;
	GF_BitStream *bs = NULL;
	GF_SHA1Context ctx;
	Bool is_isom = gf_isom_probe_file(file);

	in = fopen(file, "rb");
	gf_f64_seek(in, 0, SEEK_END);
	size = gf_f64_tell(in);
	gf_f64_seek(in, 0, SEEK_SET);

	gf_sha1_starts(&ctx);
	tot = 0;
	if (is_isom) bs = gf_bs_from_file(in, GF_BITSTREAM_READ);

	while (tot<size) {
		if (is_isom) {
			u64 box_size = gf_bs_peek_bits(bs, 32, 0);
			u32 box_type = gf_bs_peek_bits(bs, 32, 4);

			/*till end of file*/
			if (!box_size) box_size = size-tot;
			/*64-bit size*/
			else if (box_size==1) box_size = gf_bs_peek_bits(bs, 64, 8);

			/*skip all MutableDRMInformation*/
			if (box_type==GF_4CC('m','d','r','i')) {
				gf_bs_skip_bytes(bs, box_size);
				tot += box_size;
			} else {
				u32 bsize = 0;
				while (bsize<box_size) {
					u32 to_read = (u32) ((box_size-bsize<1024) ? (box_size-bsize) : 1024);
					gf_bs_read_data(bs, block, to_read);
					gf_sha1_update(&ctx, block, to_read);
					bsize += to_read;
				}
				tot += box_size;
			}
		} else {
			read = fread(block, 1, 1024, in);
			gf_sha1_update(&ctx, block, read);
			tot += read;
		}
	}
	gf_sha1_finish(&ctx, hash);
	if (bs) gf_bs_del(bs);
	fclose(in);
	return GF_OK;
}

