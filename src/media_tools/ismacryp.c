/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <math.h>


#if !defined(GPAC_DISABLE_MCRYPT)

typedef struct
{
	GF_List *tcis;
	Bool has_common_key;
	Bool in_text_header;
	//global for all tracks unless overriden
	u32 def_crypt_type;

	GF_Err parse_error;
} GF_CryptInfo;

static u32 get_crypt_type(char *cr_type)
{
	if (!stricmp(cr_type, "ISMA") || !stricmp(cr_type, "iAEC"))
		return GF_CRYPT_TYPE_ISMA;
	else if (!stricmp(cr_type, "CENC AES-CTR") || !stricmp(cr_type, "cenc"))
		return GF_CRYPT_TYPE_CENC;
	else if (!stricmp(cr_type, "piff"))
		return GF_CRYPT_TYPE_PIFF;
	else if (!stricmp(cr_type, "CENC AES-CBC") || !stricmp(cr_type, "cbc1"))
		return GF_CRYPT_TYPE_CBC1;
	else if (!stricmp(cr_type, "ADOBE") || !stricmp(cr_type, "adkm"))
		return GF_CRYPT_TYPE_ADOBE;
	else if (!stricmp(cr_type, "CENC AES-CTR Pattern") || !stricmp(cr_type, "cens"))
		return GF_CRYPT_TYPE_CENS;
	else if (!stricmp(cr_type, "CENC AES-CBC Pattern") || !stricmp(cr_type, "cbcs"))
		return GF_CRYPT_TYPE_CBCS;

	GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] Unrecognized crypto type %s\n", cr_type));
	return 0;
}

void isma_ea_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_XMLAttribute *att;
	GF_TrackCryptInfo *tkc;
	u32 i;
	GF_CryptInfo *info = (GF_CryptInfo *)sax_cbck;

	if (!strcmp(node_name, "OMATextHeader")) {
		info->in_text_header = 1;
		return;
	}
	if (!strcmp(node_name, "GPACDRM")) {
		for (i=0; i<nb_attributes; i++) {
			att = (GF_XMLAttribute *) &attributes[i];
			if (!stricmp(att->name, "type")) {
				info->def_crypt_type = get_crypt_type(att->value);
			}
		}
		return;
	}
	if (!strcmp(node_name, "CrypTrack")) {
		Bool has_common_key = GF_TRUE;
		GF_SAFEALLOC(tkc, GF_TrackCryptInfo);
		if (!tkc) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] Cannnot allocate crypt track, skipping\n"));
			return;
		}
		//by default track is encrypted
		tkc->IsEncrypted = 1;
		tkc->sai_saved_box_type = GF_ISOM_BOX_TYPE_SENC;
		tkc->scheme_type = info->def_crypt_type;
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
				else {
					tkc->trackID = atoi(att->value);
					has_common_key = GF_FALSE;
				}
			}
			else if (!stricmp(att->name, "type")) {
				tkc->scheme_type = get_crypt_type(att->value);
			}
			else if (!stricmp(att->name, "key")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->key);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse key value in CrypTrack\n"));
                    info->parse_error = e;
                    return;
                }
			}
			else if (!stricmp(att->name, "salt")) {
				u32 len, j;
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				len = (u32) strlen(sKey);
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
				if (!stricmp(att->value, "Rap")) tkc->sel_enc_type = GF_CRYPT_SELENC_RAP;
				else if (!stricmp(att->value, "Non-Rap")) tkc->sel_enc_type = GF_CRYPT_SELENC_NON_RAP;
				else if (!stricmp(att->value, "Rand")) tkc->sel_enc_type = GF_CRYPT_SELENC_RAND;
				else if (!strnicmp(att->value, "Rand", 4)) {
					tkc->sel_enc_type = GF_CRYPT_SELENC_RAND_RANGE;
					tkc->sel_enc_range = atoi(&att->value[4]);
				}
				else if (sscanf(att->value, "%u", &tkc->sel_enc_range)==1) {
					if (tkc->sel_enc_range==1) tkc->sel_enc_range = 0;
					else tkc->sel_enc_type = GF_CRYPT_SELENC_RANGE;
				}
				else if (!strnicmp(att->value, "Preview", 7)) {
					tkc->sel_enc_type = GF_CRYPT_SELENC_PREVIEW;
				}
				else if (!strnicmp(att->value, "Clear", 5)) {
					tkc->sel_enc_type = GF_CRYPT_SELENC_CLEAR;
				}
				else if (!strnicmp(att->value, "ForceClear", 10)) {
					char *sep = strchr(att->value, '=');
					if (sep) tkc->sel_enc_range = atoi(sep+1);
					tkc->sel_enc_type = GF_CRYPT_SELENC_CLEAR_FORCED;
				}
			}
			else if (!stricmp(att->name, "clearStsd")) {
				if (!strcmp(att->value, "none")) tkc->force_clear_stsd_idx = 0;
				else if (!strcmp(att->value, "before")) tkc->force_clear_stsd_idx = 1;
				else if (!strcmp(att->value, "after")) tkc->force_clear_stsd_idx = 2;
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Unrecognized clear stsd type %s, defaulting to no stsd for clear samples\n", att->value));
				}
			}
			else if (!stricmp(att->name, "forceType")) {
				tkc->force_type = GF_TRUE;
			}
			else if (!stricmp(att->name, "Preview")) {
				tkc->sel_enc_type = GF_CRYPT_SELENC_PREVIEW;
				sscanf(att->value, "%u", &tkc->sel_enc_range);
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
			/*CENC extensions*/
			else if (!stricmp(att->name, "IsEncrypted")) {
				if (!stricmp(att->value, "1"))
					tkc->IsEncrypted = 1;
				else
					tkc->IsEncrypted = 0;
			}
			else if (!stricmp(att->name, "IV_size")) {
				tkc->IV_size = atoi(att->value);
			}
			else if (!stricmp(att->name, "first_IV")) {
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				if ((strlen(sKey) == 16) || (strlen(sKey) == 32)) {
					u32 j;
					for (j=0; j<strlen(sKey); j+=2) {
						u32 v;
						char szV[5];
						sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
						sscanf(szV, "%x", &v);
						tkc->first_IV[j/2] = v;
					}
					if (!tkc->IV_size) tkc->IV_size = (u8) strlen(sKey) / 2;
				}
			}
			else if (!stricmp(att->name, "saiSavedBox")) {
				if (!stricmp(att->value, "uuid_psec")) tkc->sai_saved_box_type = GF_ISOM_BOX_UUID_PSEC;
				else if (!stricmp(att->value, "senc")) tkc->sai_saved_box_type = GF_ISOM_BOX_TYPE_SENC;
			}
			else if (!stricmp(att->name, "keyRoll")) {
				if (!strncmp(att->value, "idx=", 4))
					tkc->defaultKeyIdx = atoi(att->value+4);
				else if (!strncmp(att->value, "roll=", 5))
					tkc->keyRoll = atoi(att->value+5);
			}
			else if (!stricmp(att->name, "metadata")) {
				tkc->metadata_len = gf_base64_encode(att->value, (u32) strlen(att->value), tkc->metadata, 5000);
				tkc->metadata[tkc->metadata_len] = 0;
			}
			else if (!stricmp(att->name, "crypt_byte_block")) {
				tkc->crypt_byte_block = atoi(att->value);
			}
			else if (!stricmp(att->name, "skip_byte_block")) {
				tkc->skip_byte_block = atoi(att->value);
			}
			else if (!stricmp(att->name, "clear_bytes")) {
				tkc->clear_bytes = atoi(att->value);
			}
			else if (!stricmp(att->name, "constant_IV_size")) {
				tkc->constant_IV_size = atoi(att->value);
				if (tkc->constant_IV_size != 8 && tkc->constant_IV_size != 16) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] wrong given constant IV size %d, should be 8 or 16\n", tkc->constant_IV_size));
				}
			}
			else if (!stricmp(att->name, "constant_IV")) {
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				if (!tkc->constant_IV_size) {
					tkc->constant_IV_size = (u8) strlen(sKey) / 2;
				} else {
					u8 expected_size = (u8) strlen(sKey) / 2;
					if (tkc->constant_IV_size != expected_size) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Mismatch between given constant_IV_size %d and constant_IV length %d\n", tkc->constant_IV_size, expected_size));
					}
				}
				if ((strlen(sKey) == 16) || (strlen(sKey) == 32)) {
					u32 j;
					for (j=0; j<strlen(sKey); j+=2) {
						u32 v;
						char szV[5];
						sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
						sscanf(szV, "%x", &v);
						tkc->constant_IV[j/2] = v;
					}
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] wrong constant IV value size %d, should be 16 or 32\n", strlen(sKey)));
				}
			}
			else if (!stricmp(att->name, "encryptSliceHeader")) {
				tkc->allow_encrypted_slice_header = !strcmp(att->value, "yes") ? GF_TRUE : GF_FALSE;
			}
			else if (!stricmp(att->name, "blockAlign")) {
				if (!strcmp(att->value, "disable")) tkc->block_align = 1;
				else if (!strcmp(att->value, "always")) tkc->block_align = 2;
				else tkc->block_align = 0;
			}
		}
		if (tkc->scheme_type == GF_CRYPT_TYPE_PIFF)
			tkc->sai_saved_box_type = GF_ISOM_BOX_UUID_PSEC;

		if (has_common_key) info->has_common_key = 1;

		if ((tkc->IV_size != 0) && (tkc->IV_size != 8) && (tkc->IV_size != 16)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] wrong IV size %d for AES-128, using 16\n", (u32) tkc->IV_size));
			tkc->IV_size = 16;
		}

		if ((tkc->scheme_type == GF_CRYPT_TYPE_CENC) || (tkc->scheme_type == GF_CRYPT_TYPE_PIFF) || (tkc->scheme_type == GF_CRYPT_TYPE_CBC1)) {
			if (tkc->crypt_byte_block || tkc->skip_byte_block) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Using scheme type %s, crypt_byte_block and skip_byte_block shall be 0\n", gf_4cc_to_str(tkc->scheme_type) ));
				tkc->crypt_byte_block = tkc->skip_byte_block = 0;
			}
		}

		if ((tkc->scheme_type == GF_CRYPT_TYPE_CENC) || (tkc->scheme_type == GF_CRYPT_TYPE_PIFF) || (tkc->scheme_type == GF_CRYPT_TYPE_CBC1) || (tkc->scheme_type == GF_CRYPT_TYPE_CENS)) {
			if (tkc->constant_IV_size) {
				if (!tkc->IV_size) {
					tkc->IV_size = tkc->constant_IV_size;
					memcpy(tkc->first_IV, tkc->constant_IV, 16);
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Using scheme type %s, constant IV shall not be used, using constant IV as first IV\n", gf_4cc_to_str(tkc->scheme_type)));
					tkc->constant_IV_size = 0;
				} else {
					tkc->constant_IV_size = 0;
					memset(tkc->constant_IV, 0, 16);
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Using scheme type %s, constant IV shall not be used, ignoring\n", gf_4cc_to_str(tkc->scheme_type)));
				}
			}
		}
	}

	if (!strcmp(node_name, "key")) {
		tkc = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		if (!tkc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Missing track encryption info in XML\n"));
			info->parse_error = GF_BAD_PARAM;
			return;
		}
		tkc->KIDs = (bin128 *)gf_realloc(tkc->KIDs, sizeof(bin128)*(tkc->KID_count+1));
		tkc->keys = (bin128 *)gf_realloc(tkc->keys, sizeof(bin128)*(tkc->KID_count+1));

		for (i=0; i<nb_attributes; i++) {
			att = (GF_XMLAttribute *) &attributes[i];

			if (!stricmp(att->name, "KID")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->KIDs[tkc->KID_count]);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse KID\n"));
                    info->parse_error = e;
                    return;
                }
			}
			else if (!stricmp(att->name, "value")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->keys[tkc->KID_count]);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse key value\n"));
                    info->parse_error = e;
                    return;
                }
			}
		}
		tkc->KID_count++;
	}
}

void isma_ea_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	GF_CryptInfo *info = (GF_CryptInfo *)sax_cbck;
	if (!strcmp(node_name, "OMATextHeader")) {
		info->in_text_header = 0;
		return;
	}
}

void isma_ea_text(void *sax_cbck, const char *text, Bool is_cdata)
{
	u32 len;
	GF_TrackCryptInfo *tkc;
	GF_CryptInfo *info = (GF_CryptInfo *)sax_cbck;

	if (!info->in_text_header) return;

	tkc = (GF_TrackCryptInfo *) gf_list_last(info->tcis);
	if (!tkc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Missing track encryption info in XML\n"));
		return;
	}
	len = (u32) strlen(text);
	if (len+tkc->TextualHeadersLen > 5000) return;

	if (tkc->TextualHeadersLen) {
		tkc->TextualHeadersLen ++;
		tkc->TextualHeaders[tkc->TextualHeadersLen] = 0;
	}

	memcpy(tkc->TextualHeaders + tkc->TextualHeadersLen, text, sizeof(char)*len);
	tkc->TextualHeadersLen += len;
	tkc->TextualHeaders[tkc->TextualHeadersLen] = 0;
}

static void del_crypt_info(GF_CryptInfo *info)
{
	while (gf_list_count(info->tcis)) {
		GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		if (tci->KIDs) gf_free(tci->KIDs);
		if (tci->keys) gf_free(tci->keys);
		gf_list_rem_last(info->tcis);
		gf_free(tci);
	}
	gf_list_del(info->tcis);
	gf_free(info);
}

static GF_CryptInfo *load_crypt_file(const char *file, GF_Err *out_err)
{
	GF_Err e;
	GF_CryptInfo *info;
	GF_SAXParser *sax;
	GF_SAFEALLOC(info, GF_CryptInfo);
	if (!info) {
		if (out_err) *out_err = GF_OUT_OF_MEM;
		return NULL;
	}
	info->tcis = gf_list_new();
	sax = gf_xml_sax_new(isma_ea_node_start, isma_ea_node_end, isma_ea_text, info);
	e = gf_xml_sax_parse_file(sax, file, NULL);

	if (e>=0)
		e = info->parse_error;

	if (e<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Error parsing XML file: %s\n", gf_xml_sax_get_error(sax) ));
		if (out_err) *out_err = e;
		del_crypt_info(info);
		info = NULL;
	} else {
		if (out_err) *out_err = GF_OK;
	}

	gf_xml_sax_del(sax);
	return info;
}


GF_EXPORT
GF_Err gf_ismacryp_gpac_get_info(u32 stream_id, char *drm_file, char *key, char *salt)
{
	GF_Err e;
	u32 i, count;
	GF_CryptInfo *info;
	GF_TrackCryptInfo *tci;

	e = GF_OK;
	info = load_crypt_file(drm_file, &e);
	if (!info) return e;
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
	kms = gf_fopen(szPath, "r");
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
	if (kms) gf_fclose(kms);
	if (got_it) {
		/*watchout, MPEG4IP stores SALT|KEY, NOT KEY|SALT*/
		memcpy(key, catKey+8, sizeof(char)*16);
		memcpy(salt, catKey, sizeof(char)*8);
		return 1;
	}
	return 0;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

/*ISMACrypt*/

static GFINLINE void isma_resync_IV(GF_Crypt *mc, u64 BSO, char *salt)
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
	gf_crypt_set_IV(mc, IV, GF_AES_128_KEYSIZE+1);

	/*decrypt remain bytes*/
	if (remain) {
		char dummy[20];
		gf_crypt_decrypt(mc, dummy, remain);
	}
}

GF_EXPORT
GF_Err gf_ismacryp_decrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk)
{
	GF_Err e;
	Bool use_sel_enc;
	u32 track, count, i, j, si, otype, is_nalu;
	GF_ISOSample *samp;
	GF_ISMASample *ismasamp;
	GF_Crypt *mc;
	unsigned char IV[17];
	u32 IV_size;
	Bool prev_sample_encrypted;
	GF_ESD *esd;

	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	e = gf_isom_get_ismacryp_info(mp4, track, 1, &otype, NULL, NULL, NULL, NULL, &use_sel_enc, &IV_size, NULL);
	if (e) return e;
	is_nalu = ((otype==GF_ISOM_BOX_TYPE_264B) || (otype==GF_ISOM_BOX_TYPE_265B)) ? 1 : 0;


	mc = gf_crypt_open(GF_AES_128, GF_CTR);
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open AES-128 CTR cryptography\n", tci->trackID));
		return GF_IO_ERR;
	}

	memset(IV, 0, sizeof(char)*16);
	memcpy(IV, tci->salt, sizeof(char)*8);
	e = gf_crypt_init(mc, tci->key, IV);
	if (e) {
		gf_crypt_close(mc);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] cannot initialize AES-128 CTR (%s)\n", gf_error_to_string(e)));
		return GF_IO_ERR;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[CENC/ISMA] Decrypting track ID %d - KMS: %s%s\n", tci->trackID, tci->KMS_URI, use_sel_enc ? " - Selective Decryption" : ""));

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, GF_TRUE);

	/*start as initialized*/
	prev_sample_encrypted = 1;
	/* decrypt each sample */
	count = gf_isom_get_sample_count(mp4, track);
	gf_isom_set_nalu_extract_mode(mp4, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 0; i < count; i++) {
		samp = gf_isom_get_sample(mp4, track, i+1, &si);
		ismasamp = gf_isom_get_ismacryp_sample(mp4, track, samp, si);

		gf_free(samp->data);
		samp->data = (char *)gf_malloc(ismasamp->dataLength);
		memmove(samp->data, ismasamp->data, ismasamp->dataLength);
		samp->dataLength = ismasamp->dataLength;

		/* Decrypt payload */
		if (ismasamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
			/*restore IV*/
			if (!prev_sample_encrypted) isma_resync_IV(mc, ismasamp->IV, (char *) tci->salt);
			gf_crypt_decrypt(mc, samp->data, samp->dataLength);
		}
		prev_sample_encrypted = (ismasamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED);
		gf_isom_ismacryp_delete_sample(ismasamp);

		/*replace AVC start codes (0x00000001) by nalu size*/
		if (is_nalu) {
			u32 nalu_size;
			u32 remain = samp->dataLength;
			char *start, *end;
			start = samp->data;
			end = start + 4;
			while (remain>4) {
				if (!end[0] && !end[1] && !end[2] && (end[3]==0x01)) {
					nalu_size = (u32) (end - start - 4);
					start[0] = (nalu_size>>24)&0xFF;
					start[1] = (nalu_size>>16)&0xFF;
					start[2] = (nalu_size>>8)&0xFF;
					start[3] = (nalu_size)&0xFF;
					start = end;
					end = start+4;
					remain -= 4;
					continue;
				}
				end++;
				remain--;
			}
			nalu_size = (u32) (end - start - 4);
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
	e = gf_isom_remove_track_protection(mp4, track, 1);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Error ISMACryp signature from trackID %d: %s\n", tci->trackID, gf_error_to_string(e)));
	}
	gf_isom_set_cts_packing(mp4, track, GF_FALSE);

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

	gf_media_update_bitrate(mp4, track);

	/*update OD track if any*/
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
		gf_free(samp->data);
		samp->data = NULL;
		samp->dataLength = 0;
		gf_odf_codec_encode(cod, 1);
		gf_odf_codec_get_au(cod, &samp->data, &samp->dataLength);
		gf_odf_codec_del(cod);
		gf_isom_update_sample(mp4, i+1, 1, samp, 1);
		gf_isom_sample_del(&samp);

		/*remove IPMPToolList if any*/
		gf_isom_ipmpx_remove_tool_list(mp4);
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_ismacryp_encrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk)
{
	char IV[16];
	GF_ISOSample *samp;
	GF_ISMASample *isamp;
	GF_Crypt *mc;
	u32 i, count, di, track, IV_size, rand, avc_size_length, hevc_size_length;
	u64 BSO, range_end;
	GF_ESD *esd;
	GF_IPMPPtr *ipmpdp;
	GF_IPMP_Descriptor *ipmpd;
	GF_IPMPUpdate *ipmpdU;
#ifndef GPAC_MINIMAL_ODF
	GF_IPMPX_ISMACryp *ismac;
#endif
	GF_Err e;
	Bool prev_sample_encryped, has_crypted_samp;

	avc_size_length = hevc_size_length = 0;
	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot find TrackID %d in input file - skipping\n", tci->trackID));
		return GF_OK;
	}
	esd = gf_isom_get_esd(mp4, track, 1);
	if (esd && (esd->decoderConfig->streamType==GF_STREAM_OD)) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot encrypt OD tracks - skipping"));
		return GF_NOT_SUPPORTED;
	}
	if (esd) {
		if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SVC)) avc_size_length = 1;
		else if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_HEVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_LHVC)) hevc_size_length = 1;
		gf_odf_desc_del((GF_Descriptor*) esd);
	}
	if (avc_size_length) {
		GF_AVCConfig *avccfg = gf_isom_avc_config_get(mp4, track, 1);
		avc_size_length = avccfg->nal_unit_size;
		gf_odf_avc_cfg_del(avccfg);
		if (avc_size_length != 4) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot encrypt AVC/H264 track with %d size_length field - only 4 supported\n", avc_size_length));
			return GF_NOT_SUPPORTED;
		}
	}
	if (hevc_size_length) {
		GF_HEVCConfig *hvccfg = gf_isom_hevc_config_get(mp4, track, 1);
		avc_size_length = hvccfg->nal_unit_size;
		gf_odf_hevc_cfg_del(hvccfg);
		if (avc_size_length != 4) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot encrypt HEVC/H265 track with %d size_length field - only 4 supported\n", avc_size_length));
			return GF_NOT_SUPPORTED;
		}
	}

	if (!tci->enc_type && !strlen(tci->Scheme_URI)) strcpy(tci->Scheme_URI, "urn:gpac:isma:encryption_scheme");

	if (!gf_isom_has_sync_points(mp4, track) &&
	        ((tci->sel_enc_type==GF_CRYPT_SELENC_RAP) || (tci->sel_enc_type==GF_CRYPT_SELENC_NON_RAP)) ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] All samples in trackID %d are random access - disabling selective encryption\n", tci->trackID));
		tci->sel_enc_type = GF_CRYPT_SELENC_NONE;
	}
	else if ((tci->sel_enc_type==GF_CRYPT_SELENC_RAND) || (tci->sel_enc_type==GF_CRYPT_SELENC_RAND_RANGE)) {
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

	GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[CENC/ISMA] Encrypting track ID %d - KMS: %s%s\n", tci->trackID, tci->KMS_URI, tci->sel_enc_type ? " - Selective Encryption" : ""));

	/*init crypto*/
	memset(IV, 0, sizeof(char)*16);
	memcpy(IV, tci->salt, sizeof(char)*8);
	mc = gf_crypt_open(GF_AES_128, GF_CTR);
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open AES-128 CTR\n"));
		return GF_IO_ERR;
	}
	e = gf_crypt_init(mc, tci->key, IV);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot initialize AES-128 CTR (%s)\n", gf_error_to_string(e)) );
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
		if ((tci->sel_enc_type==GF_CRYPT_SELENC_PREVIEW) && tci->sel_enc_range) {
			char *szPreview = tci->TextualHeaders + tci->TextualHeadersLen;
			sprintf(szPreview, "PreviewRange:%d", tci->sel_enc_range);
			tci->TextualHeadersLen += (u32) strlen(szPreview)+1;
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
	if (tci->sel_enc_type==GF_CRYPT_SELENC_PREVIEW) {
		range_end = gf_isom_get_media_timescale(mp4, track) * tci->sel_enc_range;
	}

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, GF_TRUE);

	count = gf_isom_get_sample_count(mp4, track);
	gf_isom_set_nalu_extract_mode(mp4, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 0; i < count; i++) {
		samp = gf_isom_get_sample(mp4, track, i+1, &di);

		isamp = gf_isom_ismacryp_new_sample();
		isamp->IV_length = IV_size;
		isamp->KI_length = 0;

		switch (tci->sel_enc_type) {
		case GF_CRYPT_SELENC_RAP:
			if (!samp->IsRAP)
				gf_isom_get_sample_rap_roll_info(mp4, track, i+1, (Bool *) &samp->IsRAP, NULL, NULL);
			if (samp->IsRAP) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		case GF_CRYPT_SELENC_NON_RAP:
			if (!samp->IsRAP) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		/*random*/
		case GF_CRYPT_SELENC_RAND:
			rand = gf_rand();
			if (rand%2) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		/*random every sel_freq samples*/
		case GF_CRYPT_SELENC_RAND_RANGE:
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
		case GF_CRYPT_SELENC_RANGE:
			if (!(i%tci->sel_enc_type)) isamp->flags |= GF_ISOM_ISMA_IS_ENCRYPTED;
			break;
		case GF_CRYPT_SELENC_PREVIEW:
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
			u8 *d = (u8*)samp->data;
			while (done < samp->dataLength) {
				u32 nal_size = GF_4CC(d[0], d[1], d[2], d[3]);
				d[0] = d[1] = d[2] = 0;
				d[3] = 1;
				d += 4 + nal_size;
				done += 4 + nal_size;
			}
		}

		if (isamp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
			/*resync IV*/
			if (!prev_sample_encryped) isma_resync_IV(mc, BSO, (char *) tci->salt);
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
	gf_crypt_close(mc);

	gf_isom_set_cts_packing(mp4, track, GF_FALSE);
	gf_media_update_bitrate(mp4, track);


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
	return e;
}

/*Common Encryption*/
static void increase_counter(char *x, int x_size) {
	register int i, y=0;

	for (i=x_size-1; i>=0; i--) {
		y = 0;
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
		otherwise we must discard unsued bytes in the counter (next sample shall begin with counter at 0)
		if less than 16 blocks were cyphered, we must force increasing the next IV for next sample, not doing so would produce the same IV for the next bytes cyphered,
		which is forbidden by CENC (unique IV per sample). In GPAC, we ALWAYS force counter increase

		NOTE 2: in case where IV_size is 8, because the cypher block is treated as 16 bytes while processing,
		we need to increment manually the 8-bytes IV (bytes 0 to 7) for the next sample, otherwise we would likely the same IV (eg unless we had cyphered 16 * 2^64 - 1
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

//parses slice header and returns its size
static u32 gf_cenc_get_clear_bytes(GF_TrackCryptInfo *tci, GF_BitStream *plaintext_bs, char *samp_data, u32 nal_size, u32 bytes_in_nalhr)
{
	u32 clear_bytes = 0;
	if (tci->slice_header_clear) {
#ifndef GPAC_DISABLE_AV_PARSERS
		u32 nal_start = (u32) gf_bs_get_position(plaintext_bs);
		if (tci->is_avc) {
			s32 ret;
			u32 ntype, nhdr, nb_epb_count;
			GF_BitStream *bs = NULL;
			char *epb_rem_bytes;

			//check if we have EP bytes in the payload (they could be in slice header)
			nb_epb_count = gf_media_nalu_emulation_bytes_remove_count(samp_data + nal_start, nal_size);
			if (nb_epb_count) {
				epb_rem_bytes = gf_malloc(sizeof(char) * nal_size);
				nb_epb_count = gf_media_nalu_remove_emulation_bytes(samp_data + nal_start, epb_rem_bytes, nal_size);
				bs = gf_bs_new(epb_rem_bytes, nb_epb_count, GF_BITSTREAM_READ);
			}
			nhdr = gf_bs_read_u8(bs ? bs : plaintext_bs);

			ntype = nhdr & 0x1F;
			switch (ntype) {
			case GF_AVC_NALU_NON_IDR_SLICE:
			case GF_AVC_NALU_DP_A_SLICE:
			case GF_AVC_NALU_DP_B_SLICE:
			case GF_AVC_NALU_DP_C_SLICE:
			case GF_AVC_NALU_IDR_SLICE:
			case GF_AVC_NALU_SLICE_AUX:
			case GF_AVC_NALU_SVC_SLICE:
				ret = gf_media_avc_parse_nalu(bs ? bs : plaintext_bs, nhdr, &tci->avc);
				if (ret<0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Error parsing slice header, cannot get slice header size. Assuming 8 bytes is enough, but resulting file will not be compliant\n"));
					clear_bytes = 8;
				} else {
					//skip bits till alignment
					gf_bs_align(bs ? bs : plaintext_bs);
					if (bs) {
						clear_bytes = (u32) gf_bs_get_position(bs);
					} else {
						clear_bytes = (u32) gf_bs_get_position(plaintext_bs) - nal_start;
					}
				}
				break;
			case GF_AVC_NALU_SEQ_PARAM:
				gf_media_avc_read_sps(samp_data + nal_start, nal_size, &tci->avc, 0, NULL);
				clear_bytes = nal_size;
				break;
			case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
				gf_media_avc_read_sps(samp_data + nal_start, nal_size, &tci->avc, 1, NULL);
				clear_bytes = nal_size;
				break;
			case GF_AVC_NALU_PIC_PARAM:
				gf_media_avc_read_pps(samp_data + nal_start, nal_size, &tci->avc);
				clear_bytes = nal_size;
				break;

			default:
				clear_bytes = nal_size;
				break;
			}
			if (bs) {
				gf_bs_del(bs);
				gf_free(epb_rem_bytes);

				//check if we have EP bytes in slice header
				if (clear_bytes < nal_size) {
					nb_epb_count = gf_media_nalu_emulation_bytes_remove_count(samp_data + nal_start, clear_bytes);
					if (nb_epb_count)
						clear_bytes += nb_epb_count;
				}
			}
		} else {
#if !defined(GPAC_DISABLE_HEVC)
			u8 ntype, ntid, nlid;
			u32 nal_start = (u32) gf_bs_get_position(plaintext_bs);
			tci->hevc.full_slice_header_parse = GF_TRUE;
			gf_media_hevc_parse_nalu (samp_data + nal_start, nal_size, &tci->hevc, &ntype, &ntid, &nlid);
			if (ntype<=GF_HEVC_NALU_SLICE_CRA) {
				clear_bytes = tci->hevc.s_info.payload_start_offset;
			} else {
				clear_bytes = nal_size;
			}
		}
		gf_bs_seek(plaintext_bs, nal_start);
#endif

#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] AV parsers disabled, cannot get slice header size. Assuming 8 bytes is enough, but resulting file will not be compliant\n"));
		clear_bytes = 8;
#endif //GPAC_DISABLE_AV_PARSERS

	} else {
		clear_bytes = bytes_in_nalhr;
	}
	return clear_bytes;
}

typedef enum {
	ENC_FULL_SAMPLE,

	/*below types may have several ranges (clear/encrypted) per sample*/
	ENC_NALU, /*NALU-based*/
	ENC_OBU,  /*OBU-based*/
	ENC_VP9,  /*custom, see https://www.webmproject.org/vp9/mp4/ */
} GF_Enc_BsFmt;

static GF_Err gf_cenc_encrypt_sample_ctr(GF_Crypt *mc, GF_TrackCryptInfo *tci, GF_ISOSample *samp, GF_Enc_BsFmt bs_type, u32 nalu_size_length_in_bytes, char IV[16], u32 IV_size, char **sai, u32 *saiz,
										 u32 bytes_in_nalhr, u8 crypt_byte_block, u8 skip_byte_block)
{
	GF_BitStream *plaintext_bs = NULL, *cyphertext_bs, *sai_bs = NULL;
	GF_CENCSubSampleEntry *prev_entry = NULL;
	char *buffer;
	u32 max_size_in_bytes, unit_size = 0;
	GF_Err e = GF_OK;
	GF_List *subsamples = NULL;

	plaintext_bs = cyphertext_bs = sai_bs = NULL;
	max_size_in_bytes = 4096;
	buffer = (char*)gf_malloc(sizeof(char) * max_size_in_bytes);
	memset(buffer, 0, max_size_in_bytes);
	plaintext_bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	cyphertext_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	sai_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_data(sai_bs, IV, IV_size);
	subsamples = gf_list_new();
	if (!subsamples) {
		e = GF_IO_ERR;
		goto exit;
	}
	if ((bs_type == ENC_OBU) || (bs_type == ENC_VP9))
		nalu_size_length_in_bytes = 0;

	while (gf_bs_available(plaintext_bs)) {
		if (bs_type != ENC_FULL_SAMPLE) {
			u32 clear_bytes = 0;
			u32 range_idx = 0;
			struct {
				int clear, encrypted;
			} ranges[AV1_MAX_TILE_ROWS * AV1_MAX_TILE_COLS];
			u32 nb_ranges=1;

			switch(bs_type) {
			case ENC_NALU:
				unit_size = gf_bs_read_int(plaintext_bs, 8*nalu_size_length_in_bytes);
				if (unit_size == 0) {
					continue;
				}
				if (unit_size > gf_bs_available(plaintext_bs) ) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] NAL size %u larger than bytes remaining in sample\n", unit_size ));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}

	 			clear_bytes = gf_cenc_get_clear_bytes(tci, plaintext_bs, samp->data, unit_size, bytes_in_nalhr);
				break;
			case ENC_OBU: {
				ObuType obut;
				u64 pos, obu_size;
				u32 hdr_size, i;

				pos = gf_bs_get_position(plaintext_bs);
				e = gf_media_aom_av1_parse_obu(plaintext_bs, &obut, &obu_size, &hdr_size, &tci->av1);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC-CTR] Failed to parse OBU\n"));
					goto exit;
				}
				gf_bs_seek(plaintext_bs, pos);

				unit_size = (u32)obu_size;
				switch (obut) { //we only encrypt frame and tile group
				case OBU_FRAME:
				case OBU_TILE_GROUP:
					assert(tci->av1.frame_state.nb_tiles_in_obu > 0);
					nb_ranges = tci->av1.frame_state.nb_tiles_in_obu;

					ranges[0].clear = tci->av1.frame_state.tiles[0].obu_start_offset;
					ranges[0].encrypted = tci->av1.frame_state.tiles[0].size;
					for (i = 1; i < nb_ranges; ++i) {
						ranges[i].clear = tci->av1.frame_state.tiles[i].obu_start_offset - (tci->av1.frame_state.tiles[i - 1].obu_start_offset + tci->av1.frame_state.tiles[i - 1].size);
						ranges[i].encrypted = tci->av1.frame_state.tiles[i].size;
					}
					clear_bytes = ranges[0].clear;
					unit_size = clear_bytes + ranges[0].encrypted;
					if (ranges[0].encrypted >= 16) {
						//A subsample SHALL be created for each tile >= 16 bytes. If previous range had encrypted bytes, create a new one, otherwise merge in prev
						if (prev_entry && prev_entry->bytes_encrypted_data)
							prev_entry = NULL;
					} else {
						clear_bytes = unit_size;
					}
					break;
				default:
					clear_bytes = (u32)obu_size;
					break;
				}
			}
				break;
			case ENC_VP9: {
				u64 pos = 0;
				int num_frames_in_superframe = 0, superframe_index_size = 0, i = 0;
				u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME];

				if (tci->block_align != 2) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] VP9 mandates that blockAlign=\"always\". Forcing value.\n"));
					tci->block_align = 2;
				}

				pos = gf_bs_get_position(plaintext_bs);
				e = vp9_parse_superframe(plaintext_bs, samp->dataLength, &num_frames_in_superframe, frame_sizes, &superframe_index_size);
				if (e) goto exit;
				gf_bs_seek(plaintext_bs, pos);

				nb_ranges = num_frames_in_superframe;

				for (i = 0; i < num_frames_in_superframe; ++i) {
					Bool key_frame;
					int width = 0, height = 0, renderWidth = 0, renderHeight = 0;
					GF_VPConfig *vp9_cfg = gf_odf_vp_cfg_new();
					u64 pos2 = gf_bs_get_position(plaintext_bs);
					e = vp9_parse_sample(plaintext_bs, vp9_cfg, &key_frame, &width, &height, &renderWidth, &renderHeight);
					gf_odf_vp_cfg_del(vp9_cfg);
					if (e) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[CENC-CTR][VP9] Error parsing sample at DTS="LLU"\n", samp->DTS));
						goto exit;
					}

					ranges[i].clear = (int)(gf_bs_get_position(plaintext_bs) - pos2);
					ranges[i].encrypted = frame_sizes[i] - ranges[i].clear;

					gf_bs_seek(plaintext_bs, pos2 + frame_sizes[i]);
				}
				if (gf_bs_get_position(plaintext_bs) + superframe_index_size != pos + samp->dataLength) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[CENC-CTR][VP9] Inconsistent sample size %u (parsed "LLU") at DTS="LLU". Re-import raw VP9/IVF for more details.\n",
						samp->dataLength, gf_bs_get_position(plaintext_bs) + superframe_index_size - pos, samp->DTS));
				}
				gf_bs_seek(plaintext_bs, pos);

				clear_bytes = ranges[0].clear;
				assert(frame_sizes[0] == ranges[0].clear + ranges[0].encrypted);
				unit_size = frame_sizes[0];

				//final superframe index must be in clear
				if (superframe_index_size > 0) {
					ranges[nb_ranges].clear = superframe_index_size;
					ranges[nb_ranges].encrypted = 0;
					nb_ranges++;
				}

				//not clearly defined in the spec (so we do the same as in AV1 which is more clearly defined):
				if (frame_sizes[0] - clear_bytes >= 16) {
					//A subsample SHALL be created for each tile >= 16 bytes. If previous range had encrypted bytes, create a new one, otherwise merge in prev
					if (prev_entry && prev_entry->bytes_encrypted_data)
						prev_entry = NULL;
				} else {
					clear_bytes = unit_size;
				}
			}
				break;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC-CTR] Unsupported bitstream format (%s).\n", bs_type));
				e = GF_NOT_SUPPORTED;
				goto exit;
			}


			while (nb_ranges) {
				if (unit_size > max_size_in_bytes) {
					buffer = (char*)gf_realloc(buffer, sizeof(char)*unit_size);
					max_size_in_bytes = unit_size;
				}

				// adjust so that encrypted bytes are a multiple of 16 bytes: cenc SHOULD, cens SHALL, we always do it
				if (unit_size > clear_bytes) {
					u32 ret = (unit_size - clear_bytes) % 16;
					//in OBU (AV1) always enforced
					if (bs_type == ENC_OBU) {
						clear_bytes += ret;
					}
					//for CENC (should),
					else if ((tci->scheme_type == GF_CRYPT_TYPE_CENC) || (tci->scheme_type == GF_CRYPT_TYPE_PIFF)) {
						//do it if not disabled by user
						if (tci->block_align != 1) {
							//always align even if sample is not encrypted in the end
							if (tci->block_align == 2) {
								clear_bytes += ret;
							}
							//or if we don't end up with sample in the clear
							else if (unit_size > clear_bytes + ret) {
								clear_bytes += ret;
							}
						}
					} else {
						clear_bytes += ret;
					}
				}

				/*read clear data and transfer to output*/
				gf_bs_read_data(plaintext_bs, buffer, clear_bytes);
				if (bs_type == ENC_NALU)
					gf_bs_write_int(cyphertext_bs, unit_size, 8*nalu_size_length_in_bytes);

				gf_bs_write_data(cyphertext_bs, buffer, clear_bytes);

				//read data to encrypt
				if (unit_size > clear_bytes) {
					gf_bs_read_data(plaintext_bs, buffer, unit_size - clear_bytes);

					//pattern encryption
					if (crypt_byte_block && skip_byte_block) {
						u32 pos = 0;
						u32 res = unit_size - clear_bytes;
						while (res) {
							gf_crypt_encrypt(mc, buffer+pos, res >= (u32) (16*crypt_byte_block) ? 16*crypt_byte_block : res);
							if (res >= (u32) (16 * (crypt_byte_block + skip_byte_block))) {
								pos += 16 * (crypt_byte_block + skip_byte_block);
								res -= 16 * (crypt_byte_block + skip_byte_block);
							} else {
								res = 0;
							}
						}
					} else {
						gf_crypt_encrypt(mc, buffer, unit_size - clear_bytes);
					}

					/*write encrypted data to bitstream*/
					gf_bs_write_data(cyphertext_bs, buffer, unit_size - clear_bytes);
				}
				//prev entry is not a VCL, append this NAL
				if (prev_entry && !prev_entry->bytes_encrypted_data) {
					prev_entry->bytes_clear_data += nalu_size_length_in_bytes + clear_bytes;
					prev_entry->bytes_encrypted_data += unit_size - clear_bytes;
				} else {
					prev_entry = (GF_CENCSubSampleEntry *)gf_malloc(sizeof(GF_CENCSubSampleEntry));
					prev_entry->bytes_clear_data = nalu_size_length_in_bytes + clear_bytes;
					prev_entry->bytes_encrypted_data = unit_size - clear_bytes;
					gf_list_add(subsamples, prev_entry);
				}
				//check bytes of clear is not larger than 16bits
				while (prev_entry->bytes_clear_data > 0xFFFF) {
					GF_CENCSubSampleEntry *split_entry = (GF_CENCSubSampleEntry *)gf_malloc(sizeof(GF_CENCSubSampleEntry));
					split_entry->bytes_clear_data = prev_entry->bytes_clear_data - 0xFFFF;
					split_entry->bytes_encrypted_data = prev_entry->bytes_encrypted_data;
					prev_entry->bytes_clear_data = 0xFFFF;
					prev_entry->bytes_encrypted_data = 0;
					gf_list_add(subsamples, split_entry);
					prev_entry = split_entry;
				}


				nb_ranges--;
				if (!nb_ranges) break;

				range_idx++;
				switch (bs_type) {
				case ENC_OBU:
					clear_bytes = ranges[range_idx].clear;
					unit_size = clear_bytes + ranges[range_idx].encrypted;
					prev_entry = NULL; //a subsample SHALL be created for each tile.
					break;
				case ENC_VP9:
					if (nb_ranges > 1) {
						clear_bytes = ranges[range_idx].clear;
						unit_size = clear_bytes + ranges[range_idx].encrypted;
					} else { /*last*/
						unit_size = clear_bytes = ranges[range_idx].clear;
						assert(ranges[range_idx].encrypted == 0);
					}
					//a subsample SHALL be created for each encrypted tile.
					if (prev_entry->bytes_encrypted_data)
						prev_entry = NULL;
					break;
				default:
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Detected unexpected subrange in bitstream format %d\n", bs_type));
					assert(0);
					break;
				}
			}
		} else {
			assert(bs_type == ENC_FULL_SAMPLE);
			if (samp->dataLength > max_size_in_bytes) {
				buffer = (char*)gf_realloc(buffer, sizeof(char)*samp->dataLength);
				max_size_in_bytes = samp->dataLength;
			}

			gf_bs_read_data(plaintext_bs, buffer, samp->dataLength);
			gf_crypt_encrypt(mc, buffer, samp->dataLength);
			gf_bs_write_data(cyphertext_bs, buffer, samp->dataLength);
		}
	}


	if (samp->data) {
		gf_free(samp->data);
		samp->data = NULL;
		samp->dataLength = 0;
	}
	gf_bs_get_content(cyphertext_bs, &samp->data, &samp->dataLength);
	if (gf_list_count(subsamples)) {
		gf_bs_write_u16(sai_bs, gf_list_count(subsamples));
		while (gf_list_count(subsamples)) {
			GF_CENCSubSampleEntry *ptr = (GF_CENCSubSampleEntry *)gf_list_get(subsamples, 0);
			gf_list_rem(subsamples, 0);
			gf_bs_write_u16(sai_bs, ptr->bytes_clear_data);
			gf_bs_write_u32(sai_bs, ptr->bytes_encrypted_data);
			gf_free(ptr);
		}
	}
	gf_list_del(subsamples);
	gf_bs_get_content(sai_bs, sai, saiz);
	cenc_resync_IV(mc, IV, IV_size);

exit:
	if (buffer) gf_free(buffer);
	if (plaintext_bs) gf_bs_del(plaintext_bs);
	if (cyphertext_bs) gf_bs_del(cyphertext_bs);
	if (sai_bs) gf_bs_del(sai_bs);
	return e;
}


static GF_Err gf_cenc_encrypt_sample_cbc(GF_Crypt *mc, GF_TrackCryptInfo *tci, GF_ISOSample *samp, GF_Enc_BsFmt bs_type, u32 nalu_size_length_in_bytes, char IV[16], u32 IV_size, char **sai, u32 *saiz,
										u32 bytes_in_nalhr, u8 crypt_byte_block, u8 skip_byte_block) {
	GF_BitStream *plaintext_bs = NULL, *cyphertext_bs = NULL, *sai_bs = NULL;
	GF_CENCSubSampleEntry *prev_entry = NULL;
	char *buffer = NULL;
	u32 max_size, unit_size;
	GF_Err e = GF_OK;
	GF_List *subsamples;

	plaintext_bs = cyphertext_bs = sai_bs = NULL;
	max_size = 4096;
	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	memset(buffer, 0, max_size);
	plaintext_bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	cyphertext_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	sai_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_data(sai_bs, IV, IV_size);
	subsamples = gf_list_new();
	if (!subsamples) {
		e = GF_IO_ERR;
		goto exit;
	}
	if ((bs_type == ENC_OBU) || (bs_type == ENC_VP9))
		nalu_size_length_in_bytes = 0;

	while (gf_bs_available(plaintext_bs)) {
		if (skip_byte_block || (bs_type == ENC_NALU) || (bs_type == ENC_OBU)) {
			u32 clear_bytes = 0;
			u32 clear_bytes_at_end = 0;
			u32 av1_tile_idx = 0;
			struct {
				int clear, encrypted;
			} ranges[AV1_MAX_TILE_ROWS * AV1_MAX_TILE_COLS];
			u32 nb_ranges=1;

			if (bs_type == ENC_NALU) {
				unit_size = gf_bs_read_int(plaintext_bs, 8*nalu_size_length_in_bytes);

				gf_bs_write_int(cyphertext_bs, unit_size, 8*nalu_size_length_in_bytes);

				clear_bytes = gf_cenc_get_clear_bytes(tci, plaintext_bs, samp->data, unit_size, bytes_in_nalhr);
			} else if (bs_type == ENC_OBU) {
				ObuType obut;
				u64 pos, obu_size;
				u32 hdr_size, i;
				pos = gf_bs_get_position(plaintext_bs);
				e = gf_media_aom_av1_parse_obu(plaintext_bs, &obut, &obu_size, &hdr_size, &tci->av1);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC-CBC] Failed to parse OBU\n"));
					goto exit;
				}
				gf_bs_seek(plaintext_bs, pos);

				unit_size = (u32) obu_size;
				switch (obut) {
				//we only encrypt frame and tile group
				case OBU_FRAME:
				case OBU_TILE_GROUP:
					assert(tci->av1.frame_state.nb_tiles_in_obu > 0);
					nb_ranges = tci->av1.frame_state.nb_tiles_in_obu;
					ranges[0].clear = tci->av1.frame_state.tiles[0].obu_start_offset;
					ranges[0].encrypted = tci->av1.frame_state.tiles[0].size;
					for (i = 1; i < nb_ranges; ++i) {
						ranges[i].clear = tci->av1.frame_state.tiles[i].obu_start_offset - (tci->av1.frame_state.tiles[i - 1].obu_start_offset + tci->av1.frame_state.tiles[i - 1].size);
						ranges[i].encrypted = tci->av1.frame_state.tiles[i].size;
					}

					clear_bytes = ranges[0].clear;
					unit_size = clear_bytes + ranges[0].encrypted;
					// A subsample SHALL be created for each tile even if less than 16 bytes. (see https://github.com/AOMediaCodec/av1-isobmff/pull/116#discussion_r340176740)
					// If previous range had encrypted bytes, create a new one, otherwise merge in prev
					if (prev_entry && prev_entry->bytes_encrypted_data)
						prev_entry = NULL;
					break;
				default:
					clear_bytes = (u32) obu_size;
					break;
				}
			} else {
				unit_size = (u32) gf_bs_available(plaintext_bs);
				clear_bytes = bytes_in_nalhr;
				if (unit_size<clear_bytes) {
					if (tci->block_align==2) {
						clear_bytes = unit_size;
					} else {
						clear_bytes = 0;
					}
				}
			}

			while (nb_ranges) {
				if (unit_size+1 > max_size) {
					buffer = (char*)gf_realloc(buffer, sizeof(char)*(unit_size+1));
					memset(buffer, 0, sizeof(char)*(unit_size+1));
					max_size = unit_size + 1;
				}

				//in cbcs, we don't adjust bytes_encrypted_data to be a multiple of 16 bytes and leave the last block unencrypted
				//except in VP9, where BytesOfProtectedData SHALL end on the last byte of the decode_tile structure
				if ( ( (bs_type != ENC_VP9) ) && (tci->scheme_type == GF_CRYPT_TYPE_CBCS) ) {
					u32 ret = (unit_size-clear_bytes) % 16;
					clear_bytes_at_end = ret;
				}
				//in cbc1 or cbcs+VP9, we adjust bytes_encrypted_data to be a multiple of 16 bytes
				else {
					u32 ret = (unit_size-clear_bytes) % 16;
					clear_bytes += ret;
					clear_bytes_at_end = 0;
				}
				//copy over clear bytes
				if (clear_bytes) {
					assert(gf_bs_available(plaintext_bs) >= clear_bytes);

					gf_bs_read_data(plaintext_bs, buffer, clear_bytes);
					gf_bs_write_data(cyphertext_bs, buffer, clear_bytes);
				}

				if (unit_size - clear_bytes) {
					//read the bytes to be encrypted
					assert(gf_bs_available(plaintext_bs) >= unit_size - clear_bytes);
					gf_bs_read_data(plaintext_bs, buffer, unit_size - clear_bytes);

					//cbcs scheme (constant IV), reinit at each sub sample,
					if (!IV_size)
						gf_crypt_set_IV(mc, IV, 16);
					//pattern encryption
					if (crypt_byte_block && skip_byte_block) {
						u32 pos = 0;
						u32 res = unit_size - clear_bytes - clear_bytes_at_end;
						assert((res % 16) == 0);

						while (res) {
							gf_crypt_encrypt(mc, buffer + pos, res >= (u32) (16*crypt_byte_block) ? 16*crypt_byte_block : res);
							if (res >= (u32) (16 * (crypt_byte_block + skip_byte_block))) {
								pos += 16 * (crypt_byte_block + skip_byte_block);
								res -= 16 * (crypt_byte_block + skip_byte_block);
							} else {
								res = 0;
							}
						}
					} else {
						gf_crypt_encrypt(mc, buffer, unit_size - clear_bytes - clear_bytes_at_end);
					}
					//write the cyphered data, including the non encrypted bytes at the end of the block
					gf_bs_write_data(cyphertext_bs, buffer, unit_size - clear_bytes);
				}

				//for NALU-based, subsamples. Otherwise, if bytes in clear at the beginning, subsample
				//the spec is not clear here: can we ommit subsamples if we always cypher everything ?
				//for now commented out for max compatibility
				//if (is_nalu_video || bytes_in_nalhr)
				{
					//note that we write encrypted data to cover the complete byte range after the slice header
					//but the last incomplete block might be unencrypted, but is still signaled in bytes_encrypted, as per the spec

					//prev entry is not a VCL, append this NAL
					if (prev_entry && !prev_entry->bytes_encrypted_data) {
						prev_entry->bytes_clear_data += nalu_size_length_in_bytes + clear_bytes;
						prev_entry->bytes_encrypted_data += unit_size - clear_bytes;
					} else {
						prev_entry = (GF_CENCSubSampleEntry *)gf_malloc(sizeof(GF_CENCSubSampleEntry));
						prev_entry->bytes_clear_data = nalu_size_length_in_bytes + clear_bytes;
						prev_entry->bytes_encrypted_data = unit_size - clear_bytes;
						gf_list_add(subsamples, prev_entry);
					}

					//check bytes of clear is not larger than 16bits
					while (prev_entry->bytes_clear_data > 0xFFFF) {
						GF_CENCSubSampleEntry *split_entry = (GF_CENCSubSampleEntry *)gf_malloc(sizeof(GF_CENCSubSampleEntry));
						split_entry->bytes_clear_data = prev_entry->bytes_clear_data - 0xFFFF;
						split_entry->bytes_encrypted_data = prev_entry->bytes_encrypted_data;
						prev_entry->bytes_clear_data = 0xFFFF;
						prev_entry->bytes_encrypted_data = 0;
						gf_list_add(subsamples, split_entry);
						prev_entry = split_entry;
					}
					assert((s32)prev_entry->bytes_encrypted_data >= 0);
					assert(prev_entry->bytes_encrypted_data <= samp->dataLength);
				}
				nb_ranges--;
				if (!nb_ranges) break;

				av1_tile_idx++;
				clear_bytes = ranges[av1_tile_idx].clear;
				unit_size = clear_bytes + ranges[av1_tile_idx].encrypted;
				//A subsample SHALL be created for each tile.
				prev_entry = NULL;
			}
		} else {
			u32 clear_trailing;

			if (samp->dataLength > max_size) {
				buffer = (char*)gf_realloc(buffer, sizeof(char)*samp->dataLength);
				max_size = samp->dataLength;
			}

			gf_bs_read_data(plaintext_bs, buffer, samp->dataLength);
			clear_trailing = samp->dataLength % 16;

			//cbcs scheme with constant IV, reinit at each sample,
			if (!IV_size)
				gf_crypt_set_IV(mc, IV, 16);

			if (samp->dataLength >= 16) {
				gf_crypt_encrypt(mc, buffer, samp->dataLength - clear_trailing);
				gf_bs_write_data(cyphertext_bs, buffer, samp->dataLength - clear_trailing);
			}
			if (clear_trailing) {
				gf_bs_write_data(cyphertext_bs, buffer+samp->dataLength - clear_trailing, clear_trailing);
			}
		}
	}

	if (samp->data) {
		gf_free(samp->data);
		samp->data = NULL;
		samp->dataLength = 0;
	}
	gf_bs_get_content(cyphertext_bs, &samp->data, &samp->dataLength);
	if (gf_list_count(subsamples)) {
		gf_bs_write_u16(sai_bs, gf_list_count(subsamples));
		while (gf_list_count(subsamples)) {
			GF_CENCSubSampleEntry *ptr = (GF_CENCSubSampleEntry *)gf_list_get(subsamples, 0);
			gf_list_rem(subsamples, 0);
			gf_bs_write_u16(sai_bs, ptr->bytes_clear_data);
			gf_bs_write_u32(sai_bs, ptr->bytes_encrypted_data);
			gf_free(ptr);
		}
	}
	gf_list_del(subsamples);
	gf_bs_get_content(sai_bs, sai, saiz);

exit:
	if (buffer) gf_free(buffer);
	if (plaintext_bs) gf_bs_del(plaintext_bs);
	if (cyphertext_bs) gf_bs_del(cyphertext_bs);
	if (sai_bs) gf_bs_del(sai_bs);
	return e;
}

/*encrypts track - logs, progress: info callbacks, NULL for default*/
GF_Err gf_cenc_encrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk)
{
	GF_Err e;
	char IV[16];
	GF_ISOSample *samp = NULL;
	GF_Crypt *mc;
	Bool all_rap = GF_FALSE;
	u32 i, count, stsd_idx, track, saiz_len, nb_samp_encrypted, nalu_size_length, idx, bytes_in_nalhr;
	GF_ESD *esd;
	Bool has_crypted_samp;
	GF_Enc_BsFmt bs_type = ENC_FULL_SAMPLE;
	Bool use_subsamples = GF_FALSE;
	char *saiz_buf;
	Bool use_seig = GF_FALSE;
	Bool has_seig = GF_FALSE;
	u32 clear_stsd_idx = 1;
	u32 crypt_stsd_idx = 1;
	GF_BitStream *bs;

	nalu_size_length = 0;
	mc = NULL;
	saiz_buf = NULL;
	bs = NULL;
	bytes_in_nalhr = 0;

	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot find TrackID %d in input file - skipping\n", tci->trackID));
		return GF_OK;
	}

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, GF_TRUE);

	esd = gf_isom_get_esd(mp4, track, 1);
	if (esd && (esd->decoderConfig->streamType == GF_STREAM_OD)) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot encrypt OD tracks - skipping"));
		return GF_NOT_SUPPORTED;
	}
	if (esd) {
		if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SVC)) {
			GF_AVCConfig *avccfg = gf_isom_avc_config_get(mp4, track, 1);
			GF_AVCConfig *svccfg = gf_isom_svc_config_get(mp4, track, 1);

			bytes_in_nalhr = 1;

			if (avccfg)
				nalu_size_length = avccfg->nal_unit_size;
			else if (svccfg)
				nalu_size_length = svccfg->nal_unit_size;

			switch (gf_isom_get_media_subtype(mp4, track, 1)) {
			case GF_ISOM_BOX_TYPE_AVC1:
				if (!tci->allow_encrypted_slice_header) {
					tci->slice_header_clear = GF_TRUE;
				} else if (tci->scheme_type==GF_CRYPT_TYPE_CBCS) {
					tci->slice_header_clear = GF_TRUE;
				}
				break;
			default:
				tci->slice_header_clear = GF_TRUE;
				break;
			}
			tci->is_avc = GF_TRUE;
			if (!tci->slice_header_clear && tci->clear_bytes)
				bytes_in_nalhr = tci->clear_bytes;

#if !defined(GPAC_DISABLE_AV_PARSERS)
			for (i=0; i<gf_list_count(avccfg->sequenceParameterSets); i++) {
				GF_AVCConfigSlot *slc = gf_list_get(avccfg->sequenceParameterSets, i);
				gf_media_avc_read_sps(slc->data, slc->size, &tci->avc, 0, NULL);
			}
			for (i=0; i<gf_list_count(avccfg->pictureParameterSets); i++) {
				GF_AVCConfigSlot *slc = gf_list_get(avccfg->pictureParameterSets, i);
				gf_media_avc_read_pps(slc->data, slc->size, &tci->avc);
			}
#endif
			if (avccfg) gf_odf_avc_cfg_del(avccfg);
			if (svccfg) gf_odf_avc_cfg_del(svccfg);
			bs_type = ENC_NALU;
		} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_HEVC) {
			GF_HEVCConfig *hevccfg = gf_isom_hevc_config_get(mp4, track, 1);
			if (hevccfg)
				nalu_size_length = hevccfg->nal_unit_size;

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_HEVC)
			gf_media_hevc_parse_ps(hevccfg, &tci->hevc, GF_HEVC_NALU_VID_PARAM);
			gf_media_hevc_parse_ps(hevccfg, &tci->hevc, GF_HEVC_NALU_SEQ_PARAM);
			gf_media_hevc_parse_ps(hevccfg, &tci->hevc, GF_HEVC_NALU_PIC_PARAM);
#endif
			//mandatory for HEVC
			tci->slice_header_clear = GF_TRUE;
			tci->is_avc = GF_FALSE;

			if (hevccfg) gf_odf_hevc_cfg_del(hevccfg);
			bs_type = ENC_NALU;
			bytes_in_nalhr = 2;
		} else if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_AV1) {
			tci->av1.config = gf_odf_av1_cfg_new();
			bs_type = ENC_OBU;
			bytes_in_nalhr = 2;
		} else if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_VP9) {
			bs_type = ENC_VP9;
			bytes_in_nalhr = 2;
		}
		gf_odf_desc_del((GF_Descriptor*) esd);
	}

	if ( (tci->scheme_type == GF_CRYPT_TYPE_CENS) || (tci->scheme_type == GF_CRYPT_TYPE_CBCS) ) {
		if ( (bs_type == ENC_NALU) || (bs_type == ENC_OBU) ) {
			if (!tci->crypt_byte_block || !tci->skip_byte_block) {
				if (tci->crypt_byte_block || tci->skip_byte_block) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Using pattern mode, crypt_byte_block and skip_byte_block shall be 0 only for track other than video, using 1 crypt + 9 skip\n"));
				}
				tci->crypt_byte_block = 1;
				tci->skip_byte_block = 9;
			}
		} else if (bs_type == ENC_VP9) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Using pattern mode, CBCS or CENS, is not supported with VP9.\n"));
			e = GF_NOT_SUPPORTED;
			goto exit;
		}
	}


	if (bs_type != ENC_FULL_SAMPLE) {
		use_subsamples = GF_TRUE;
	}
	//CBCS mode with skip byte block may be used for any track, in which case we need subsamples
	else if (tci->scheme_type == GF_CRYPT_TYPE_CBCS) {
		if (tci->skip_byte_block) {
			use_subsamples = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("\n[CENC] Using cbcs pattern mode on non NAL video track, this may not be supported by most devices; consider setting skip_byte_block to 0\n\n"));
			//cbcs allows bytes of clear data
			bytes_in_nalhr = tci->clear_bytes;
		}
		//This is not clear in the spec, setting skip and crypt to 0 means no pattern, in which case tenc version shall be 0
		//but cbcs asks for 1 - needs further clarification
#if 0
		//setup defaults
		else if (!tci->crypt_byte_block) {
			tci->crypt_byte_block = 1;
		}
#else
		else {
			tci->crypt_byte_block = 0;
		}
#endif
	}
	else if ((tci->scheme_type == GF_CRYPT_TYPE_CENS) && tci->skip_byte_block) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] Using cens pattern mode on non NAL video track not allowed, forcing skip_byte_block to 0\n"));
		tci->skip_byte_block = 0;
		if (!tci->crypt_byte_block) {
			tci->crypt_byte_block = 1;
		}
	}
	samp = NULL;

	if (tci->ctr_mode) {
		mc = gf_crypt_open(GF_AES_128, GF_CTR);
	} else {
		mc = gf_crypt_open(GF_AES_128, GF_CBC);
	}
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot open AES-128 %s\n", tci->ctr_mode ? "CTR" : "CBC"));
		e = GF_IO_ERR;
		goto exit;
	}

	if ((tci->sel_enc_type==GF_CRYPT_SELENC_CLEAR_FORCED) && tci->force_clear_stsd_idx) {
		e = gf_isom_clone_sample_description(mp4, track, mp4, track, 1, NULL, NULL, &clear_stsd_idx);
		if (e)
			goto exit;

		if (tci->force_clear_stsd_idx==1) {
			crypt_stsd_idx = 2;
			clear_stsd_idx = 1;
		} else {
			clear_stsd_idx = 2;
			crypt_stsd_idx = 1;
		}
	}

	/*select key*/
	if (!tci->keys) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] No key specified\n"));
		e = GF_BAD_PARAM;
		goto exit;
	}
	if (tci->defaultKeyIdx && (tci->defaultKeyIdx < tci->KID_count)) {
		memcpy(tci->key, tci->keys[tci->defaultKeyIdx], 16);
		memcpy(tci->default_KID, tci->KIDs[tci->defaultKeyIdx], 16);
		idx = tci->defaultKeyIdx;
	} else {
		memcpy(tci->key, tci->keys[0], 16);
		memcpy(tci->default_KID, tci->KIDs[0], 16);
		idx = 0;
		tci->defaultKeyIdx = 0;
	}

	/*create CENC protection*/
	e = gf_isom_set_cenc_protection(mp4, track, crypt_stsd_idx, tci->scheme_type, 0x00010000, tci->IsEncrypted, tci->IV_size, tci->default_KID,
		tci->crypt_byte_block, tci->skip_byte_block, tci->constant_IV_size, tci->constant_IV);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot create CENC protection: %s\n", gf_error_to_string(e)));
		goto exit;
	}


	count = gf_isom_get_sample_count(mp4, track);

	has_crypted_samp = GF_FALSE;
	nb_samp_encrypted = 0;

	//if constantIV and not using CENC subsample, no CENC auxiliary info
	if (!tci->constant_IV_size || use_subsamples) {
		/*Sample Encryption Box*/
		e = gf_isom_cenc_allocate_storage(mp4, track, tci->sai_saved_box_type, 0, 0, NULL);
		if (e) goto exit;
	}

	if (! gf_isom_has_sync_points(mp4, track))
		all_rap = GF_TRUE;

	if (tci->keyRoll) {
		use_seig = GF_TRUE;
	} else if (tci->sel_enc_type == GF_CRYPT_SELENC_RAP) {
		if (gf_isom_has_sync_points(mp4, track))
			use_seig = GF_TRUE;
	} else if (tci->sel_enc_type > GF_CRYPT_SELENC_RAP) {
		use_seig = GF_TRUE;
	}

	gf_isom_set_nalu_extract_mode(mp4, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 0; i < count; i++) {
		bin128 NULL_IV;
		Bool forced_clear = GF_FALSE;
		saiz_len=0;
		samp = gf_isom_get_sample(mp4, track, i+1, &stsd_idx);
		if (!samp) {
			e = GF_IO_ERR;
			goto exit;
		}

		switch (tci->sel_enc_type) {
		case GF_CRYPT_SELENC_RAP:
			if (!samp->IsRAP)
				gf_isom_get_sample_rap_roll_info(mp4, track, i+1, (Bool *) &samp->IsRAP, NULL, NULL);

			if (!samp->IsRAP && !all_rap) {
				//sample is not encrypted, put an empty SAI (size 0)
				e = gf_isom_track_cenc_add_sample_info(mp4, track, tci->sai_saved_box_type, 0, NULL, 0, GF_FALSE, NULL);
				if (e)
					goto exit;
				saiz_buf = NULL;

				memset(NULL_IV, 0, 16);
				e = gf_isom_set_sample_cenc_group(mp4, track, i+1, 0, 0, NULL_IV, 0, 0, 0, NULL);
				if (e) goto exit;

				if (crypt_stsd_idx != stsd_idx) {
					gf_isom_change_sample_desc_index(mp4, track, i+1, crypt_stsd_idx);
				}
				gf_isom_sample_del(&samp);
				continue;
			}
			break;
		case GF_CRYPT_SELENC_NON_RAP:
			if (samp->IsRAP || all_rap) {
				//sample is not encrypted, put an empty SAI (size 0)
				e = gf_isom_track_cenc_add_sample_info(mp4, track, tci->sai_saved_box_type, 0, NULL, 0, GF_FALSE, NULL);
				if (e)
					goto exit;
				saiz_buf = NULL;

				memset(NULL_IV, 0, 16);
				e = gf_isom_set_sample_cenc_group(mp4, track, i+1, 0, 0, NULL_IV, 0, 0, 0, NULL);
				if (e) goto exit;

				if (crypt_stsd_idx != stsd_idx) {
					gf_isom_change_sample_desc_index(mp4, track, i+1, crypt_stsd_idx);
				}
				gf_isom_sample_del(&samp);
				continue;
			}
			break;
		case GF_CRYPT_SELENC_CLEAR_FORCED:
			forced_clear = GF_TRUE;
		case GF_CRYPT_SELENC_CLEAR:
			if (!forced_clear || !tci->force_clear_stsd_idx) {
				memset(NULL_IV, 0, 16);

				//sample is not encrypted, put an empty SAI (size 0) except in force_clear mode where we generate
				//an SAI with a clear byte range covering the entire sample
				if (forced_clear) {
					memcpy(NULL_IV, (char *) &i, 4);
					e = gf_isom_track_cenc_add_sample_info(mp4, track, tci->sai_saved_box_type, tci->IV_size, NULL, samp->dataLength, use_subsamples, (char *)NULL_IV);
				} else {
					e = gf_isom_track_cenc_add_sample_info(mp4, track, tci->sai_saved_box_type, 0, NULL, 0, GF_FALSE, NULL);
				}
				if (e)
					goto exit;
				saiz_buf = NULL;

				if (!forced_clear) {
					e = gf_isom_set_sample_cenc_group(mp4, track, i + 1, 0, 0, NULL_IV, 0, 0, 0, NULL);
					if (e) goto exit;
				}
			} else {
				//add an empty SAI, this will populate the saiz table with an entry of size 0
				e = gf_isom_track_cenc_add_sample_info(mp4, track, tci->sai_saved_box_type, 0, NULL, 0, GF_FALSE, NULL);
				if (e)
					goto exit;
			}

			if (forced_clear && tci->force_clear_stsd_idx) {
				if (clear_stsd_idx != stsd_idx) {
					gf_isom_change_sample_desc_index(mp4, track, i+1, clear_stsd_idx);
				}
			} else {
				if (crypt_stsd_idx != stsd_idx) {
					gf_isom_change_sample_desc_index(mp4, track, i+1, crypt_stsd_idx);
				}
			}

			gf_isom_sample_del(&samp);

			//we have a range, go back to regulare encryption once done
			if (forced_clear && tci->sel_enc_range && (i+1>=tci->sel_enc_range)) {
				tci->sel_enc_type = GF_CRYPT_SELENC_NONE;
			}
			continue;

		default:
			break;
		}

		/*generate initialization vector for the first sample in track ... */
		if (!has_crypted_samp) {
			memset(IV, 0, sizeof(char)*16);
			if (tci->IV_size == 8) {
				memcpy(IV, tci->first_IV, sizeof(char)*8);
				memset(IV+8, 0, sizeof(char)*8);
			}
			else if (tci->IV_size == 16) {
				memcpy(IV, tci->first_IV, sizeof(char)*16);
			}
			else if (!tci->IV_size) {
				if (tci->constant_IV_size == 8) {
					memcpy(IV, tci->constant_IV, sizeof(char)*8);
					memset(IV+8, 0, sizeof(char)*8);
				}
				else if (tci->constant_IV_size == 16) {
					memcpy(IV, tci->constant_IV, sizeof(char)*16);
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] No IV set and invalid constant IV size %d crypt info file\n", tci->constant_IV_size));
					return GF_BAD_PARAM;
				}
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Invalid IV size %d in crypt info file\n", tci->IV_size));
				return GF_NOT_SUPPORTED;
			}

			e = gf_crypt_init(mc, tci->key, IV);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot initialize AES-128 %s (%s)\n", tci->ctr_mode ? "CTR" : "CBC", gf_error_to_string(e)) );
				gf_crypt_close(mc);
				mc = NULL;
				e = GF_IO_ERR;
				goto exit;
			}
			has_crypted_samp = GF_TRUE;
		}
		else {
			if (tci->keyRoll) {
				idx = (nb_samp_encrypted / tci->keyRoll) % tci->KID_count;
				memcpy(tci->key, tci->keys[idx], 16);
				e = gf_crypt_set_key(mc, tci->key);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot set key AES-128 %s (%s)\n", tci->ctr_mode ? "CTR" : "CBC", gf_error_to_string(e)) );
					gf_crypt_close(mc);
					mc = NULL;
					e = GF_IO_ERR;
					goto exit;
				}
			}
		}

		if (use_seig && (tci->defaultKeyIdx != idx) ) {
			/*add this sample to sample encryption group*/
			e = gf_isom_set_sample_cenc_group(mp4, track, i+1, 1, tci->IV_size, tci->KIDs[idx], tci->crypt_byte_block, tci->skip_byte_block, tci->constant_IV_size, tci->constant_IV);
			if (e) goto exit;
			has_seig = GF_TRUE;
		} else if (has_seig) {
			e = gf_isom_set_sample_cenc_default(mp4, track, i+1);
			if (e) goto exit;
		}

		if (tci->ctr_mode) {
			e = gf_cenc_encrypt_sample_ctr(mc, tci, samp, bs_type, nalu_size_length, IV, tci->IV_size, &saiz_buf, &saiz_len, bytes_in_nalhr, tci->crypt_byte_block, tci->skip_byte_block);
			if (e) goto exit;
		} else {
			//in cbcs scheme, if Per_Sample_IV_size is not 0 (no constant IV), fetch current IV
			if (tci->IV_size) {
				u32 IV_size = 16;
				gf_crypt_get_IV(mc, IV, &IV_size);
			}
			e = gf_cenc_encrypt_sample_cbc(mc, tci, samp, bs_type, nalu_size_length, IV, tci->IV_size, &saiz_buf, &saiz_len, bytes_in_nalhr, tci->crypt_byte_block, tci->skip_byte_block);
			if (e) goto exit;
		}

		gf_isom_update_sample(mp4, track, i+1, samp, 1);

		if (crypt_stsd_idx != stsd_idx) {
			gf_isom_change_sample_desc_index(mp4, track, i+1, crypt_stsd_idx);
		}
		gf_isom_sample_del(&samp);
		samp = NULL;

		if (saiz_len) {
			e = gf_isom_track_cenc_add_sample_info(mp4, track, tci->sai_saved_box_type, tci->IV_size, saiz_buf, saiz_len, use_subsamples, NULL);
			if (e)
				goto exit;
		}
		gf_free(saiz_buf);
		saiz_buf = NULL;

		nb_samp_encrypted++;
		gf_set_progress("CENC Encrypt", i+1, count);
	}

	gf_isom_set_cts_packing(mp4, track, GF_FALSE);
	//not strictly needed but we call it in case bitrate info in source is wrong
	gf_media_update_bitrate(mp4, track);

exit:
	if (samp) gf_isom_sample_del(&samp);
	if (mc) gf_crypt_close(mc);
	if (saiz_buf) gf_free(saiz_buf);
	if (bs) gf_bs_del(bs);
	if (tci->av1.config) gf_odf_av1_cfg_del(tci->av1.config);
	return e;
}

/*decrypts track - logs, progress: info callbacks, NULL for default*/
GF_Err gf_cenc_decrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk)
{
	GF_Err e;
	u32 track, count, i, j, si, max_size, subsample_count, nb_samp_decrypted;
	GF_ISOSample *samp = NULL;
	GF_Crypt *mc;
	char IV[17];
	bin128 blank_key;
	Bool prev_sample_encrypted;
	GF_BitStream *plaintext_bs, *cyphertext_bs;
	GF_CENCSampleAuxInfo *sai;
	char *buffer;
	u32 scheme_type;
	Bool is_ctr_mode = GF_FALSE;

	plaintext_bs = cyphertext_bs = NULL;
	mc = NULL;
	buffer = NULL;
	max_size = 4096;
	nb_samp_decrypted = 0;
	sai = NULL;

	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot find TrackID %d in input file - skipping\n", tci->trackID));
		return GF_OK;
	}

	scheme_type = gf_isom_is_media_encrypted(mp4, track, 0);
	if ((scheme_type==GF_CRYPT_TYPE_CENC) || (scheme_type==GF_CRYPT_TYPE_PIFF) || (scheme_type==GF_CRYPT_TYPE_CENS))
		is_ctr_mode = GF_TRUE;

	if (is_ctr_mode)
		mc = gf_crypt_open(GF_AES_128, GF_CTR);
	else
		mc = gf_crypt_open(GF_AES_128, GF_CBC);
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot open AES-128 %s\n", is_ctr_mode ? "CTR" : "CBC"));
		e = GF_IO_ERR;
		goto exit;
	}

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, GF_TRUE);

	memset(blank_key, 0, 16);
	/* decrypt each sample */
	count = gf_isom_get_sample_count(mp4, track);
	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	prev_sample_encrypted = GF_FALSE;
	gf_isom_set_nalu_extract_mode(mp4, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 0; i < count; i++) {
		u32 Is_Encrypted;
		u8 IV_size, constant_IV_size, skip_byte_block, crypt_byte_block;
		bin128 KID, constant_IV;

		constant_IV_size = skip_byte_block = crypt_byte_block = 0;
		e = gf_isom_get_sample_cenc_info(mp4, track, i+1, &Is_Encrypted, &IV_size, &KID, &crypt_byte_block, &skip_byte_block, &constant_IV_size, &constant_IV);
		if (e) goto exit;

		if (!Is_Encrypted)
			continue;

		/*select key*/
		for (j = 0; j < tci->KID_count; j++) {
			if (!strncmp((const char *)tci->KIDs[j], (const char *)KID, 16)
				|| !strncmp((const char *)tci->KIDs[j], (const char *)blank_key, 16)
			) {
				memcpy(tci->key, tci->keys[j], 16);
				break;
			}
		}
		if (j == tci->KID_count) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot locate key for given KID\n") );
			e = GF_SERVICE_ERROR;
			goto exit;
		}

		memset(IV, 0, 17);
		memset(buffer, 0, max_size);

		samp = gf_isom_get_sample(mp4, track, i+1, &si);
		if (!samp)
		{
			e = GF_IO_ERR;
			goto exit;
		}

		e = gf_isom_cenc_get_sample_aux_info(mp4, track, i+1, si, &sai, NULL);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot fetch senc data for sample %d\n", i+1) );
			goto exit;
		}

		cyphertext_bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
		plaintext_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		if (sai)
			sai->IV_size = IV_size;


		if (!prev_sample_encrypted) {
			if (sai && sai->IV_size) {
				memmove(IV, sai->IV, sai->IV_size);
				if (sai->IV_size == 8)
					memset(IV+8, 0, sizeof(char)*8);
			} else {
				//cbcs scheme mode, use constant IV
				memmove(IV, constant_IV, constant_IV_size);
				if (constant_IV_size == 8)
					memset(IV+8, 0, sizeof(char)*8);
			}

			e = gf_crypt_init(mc, tci->key, IV);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot initialize AES-128 %s (%s)\n", is_ctr_mode ? "CTR" : "CBC", gf_error_to_string(e)) );
				gf_crypt_close(mc);
				mc = NULL;
				e = GF_IO_ERR;
				goto exit;
			}
			prev_sample_encrypted = GF_TRUE;
		}
		else {
			e = gf_crypt_set_key(mc, tci->key);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot set key AES-128 %s (%s)\n", is_ctr_mode ? "CTR" : "CBC", gf_error_to_string(e)) );
				gf_crypt_close(mc);
				mc = NULL;
				e = GF_IO_ERR;
				goto exit;
			}
			if (sai && sai->IV_size) {
				if (is_ctr_mode) {
					GF_BitStream *bs;
					bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
					gf_bs_write_u8(bs, 0);	/*begin of counter*/
					gf_bs_write_data(bs, (char *)sai->IV, sai->IV_size);
					if (sai->IV_size == 8)
						gf_bs_write_u64(bs, 0);
					gf_bs_del(bs);
					gf_crypt_set_IV(mc, IV, GF_AES_128_KEYSIZE+1);
				}
				else {
					memmove(IV, sai->IV, 16);
					gf_crypt_set_IV(mc, IV, GF_AES_128_KEYSIZE);
				}
			} else {
				//cbcs scheme mode, use constant IV
				memmove(IV, constant_IV, constant_IV_size);
				if (constant_IV_size == 8)
					memset(IV+8, 0, sizeof(char)*8);
				gf_crypt_set_IV(mc, IV, GF_AES_128_KEYSIZE);
			}
		}

		//sub-sample encryption
		if (sai && sai->subsample_count) {
			u32 nb_done = 0;
			subsample_count = 0;
			while (gf_bs_available(cyphertext_bs)) {
				GF_CENCSubSampleEntry *sai_e;
				assert(subsample_count < sai->subsample_count);
				if (!sai->IV_size) {
					//cbcs scheme mode, use constant IV
					memmove(IV, constant_IV, constant_IV_size);
					if (constant_IV_size == 8)
						memset(IV+8, 0, sizeof(char)*8);
					gf_crypt_set_IV(mc, IV, GF_AES_128_KEYSIZE);
				}
				sai_e = &sai->subsamples[subsample_count];
				if (nb_done + sai_e->bytes_clear_data + sai_e->bytes_encrypted_data > samp->dataLength) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Error in sample %d subsample info: %d bytes in samples but more bytes signaled in subsample data (%d bytes at subsample %d)\n", i+1, samp->dataLength, nb_done + sai_e->bytes_clear_data + sai_e->bytes_encrypted_data, subsample_count+1));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
				nb_done += sai_e->bytes_clear_data + sai_e->bytes_encrypted_data;

				/*read clear data and write it to plaintext bitstream*/
				if (max_size < sai_e->bytes_clear_data) {
					buffer = (char*)gf_realloc(buffer, sizeof(char)*sai_e->bytes_clear_data);
					max_size = sai_e->bytes_clear_data;
				}
				gf_bs_read_data(cyphertext_bs, buffer, sai_e->bytes_clear_data);
				gf_bs_write_data(plaintext_bs, buffer, sai_e->bytes_clear_data);

				/*now read encrypted data, decrypted it and write to pleintext bitstream*/
				if (max_size < sai_e->bytes_encrypted_data) {
					buffer = (char*)gf_realloc(buffer, sizeof(char)*sai_e->bytes_encrypted_data);
					max_size = sai_e->bytes_encrypted_data;
				}
				gf_bs_read_data(cyphertext_bs, buffer, sai_e->bytes_encrypted_data);
				//pattern decryption
				if (crypt_byte_block && skip_byte_block) {
					u32 pos = 0;
					u32 res = sai_e->bytes_encrypted_data;

					if (!is_ctr_mode) {
						u32 clear_trailing = res % 16;
						res -= clear_trailing;
					}

					while (res) {
						gf_crypt_decrypt(mc, buffer+pos, res >= (u32) (16*crypt_byte_block) ? 16*crypt_byte_block : res);
						if (res >= (u32) (16 * (crypt_byte_block + skip_byte_block))) {
							pos += 16 * (crypt_byte_block + skip_byte_block);
							res -= 16 * (crypt_byte_block + skip_byte_block);
						} else {
							res = 0;
						}
					}
				} else {
					gf_crypt_decrypt(mc, buffer, sai_e->bytes_encrypted_data);
				}
				gf_bs_write_data(plaintext_bs, buffer, sai_e->bytes_encrypted_data);

				subsample_count++;
			}
		}
		//full sample encryption
		else {
			u32 clear_trailing = 0;
			if (max_size < samp->dataLength) {
				buffer = (char*)gf_realloc(buffer, sizeof(char)*samp->dataLength);
				max_size = samp->dataLength;
			}
			gf_bs_read_data(cyphertext_bs, buffer,samp->dataLength);

			if (!is_ctr_mode) {
				clear_trailing = samp->dataLength % 16;
			}
			if (skip_byte_block && crypt_byte_block) {
				u32 pos = 0;
				u32 res = samp->dataLength - clear_trailing;
				while (res) {
					gf_crypt_decrypt(mc, buffer+pos, res >= (u32) (16*crypt_byte_block) ? 16*crypt_byte_block : res);
					if (res >= (u32) (16 * (crypt_byte_block + skip_byte_block))) {
						pos += 16 * (crypt_byte_block + skip_byte_block);
						res -= 16 * (crypt_byte_block + skip_byte_block);
					} else {
						res = 0;
					}
				}
			} else {
				gf_crypt_decrypt(mc, buffer, samp->dataLength - clear_trailing);
			}
			gf_bs_write_data(plaintext_bs, buffer, samp->dataLength - clear_trailing);

			if (clear_trailing) {
				gf_bs_write_data(plaintext_bs, buffer + samp->dataLength - clear_trailing, clear_trailing);
			}
		}

		if (sai) {
			gf_isom_cenc_samp_aux_info_del(sai);
			sai = NULL;
		}

		gf_bs_del(cyphertext_bs);
		cyphertext_bs = NULL;
		if (samp->data) {
			gf_free(samp->data);
			samp->data = NULL;
			samp->dataLength = 0;
		}
		gf_bs_get_content(plaintext_bs, &samp->data, &samp->dataLength);
		gf_bs_del(plaintext_bs);
		plaintext_bs = NULL;
		gf_isom_update_sample(mp4, track, i+1, samp, 1);
		gf_isom_sample_del(&samp);
		samp = NULL;
		nb_samp_decrypted++;

		gf_set_progress("CENC Decrypt", i+1, count);
	}

	/*remove protection info*/
	count = gf_isom_get_sample_description_count(mp4, track);
	for (i=0; i<count; i++) {
		e = gf_isom_remove_track_protection(mp4, track, i+1);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Error CENC signature from trackID %d: %s\n", tci->trackID, gf_error_to_string(e)));
		}
	}

	gf_isom_remove_cenc_saiz(mp4, track);
	gf_isom_remove_cenc_saio(mp4, track);
	gf_isom_remove_samp_enc_box(mp4, track);
	gf_isom_remove_samp_group_box(mp4, track);

	count = gf_isom_get_sample_description_count(mp4, track);
	if (count>1) {
		u32 nb_same_sdesc=1;
		for (i=1; i<count; i++) {
			if (gf_isom_is_same_sample_description(mp4, track, 1, mp4, track, i+1)) {
				nb_same_sdesc++;
			}
		}
		if (nb_same_sdesc==count) {
			while (count>1) {
				gf_isom_remove_sample_description(mp4, track, count);
				count--;
			}
			count = gf_isom_get_sample_count(mp4, track);
			for (i=0; i<count; i++) {
				gf_isom_change_sample_desc_index(mp4, track, i+1, 1);
			}
		}
	}

	gf_isom_set_cts_packing(mp4, track, GF_FALSE);

exit:
	if (mc) gf_crypt_close(mc);
	if (plaintext_bs) gf_bs_del(plaintext_bs);
	if (cyphertext_bs) gf_bs_del(cyphertext_bs);
	if (samp) gf_isom_sample_del(&samp);
	if (buffer) gf_free(buffer);
	if (sai) gf_isom_cenc_samp_aux_info_del(sai);
	return e;
}

GF_Err gf_adobe_encrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk)
{
	GF_Err e;
	char IV[16];
	GF_ISOSample *samp;
	GF_Crypt *mc;
	Bool all_rap = GF_FALSE;
	u32 i, count, di, track, len;
	Bool has_crypted_samp;
	char *buf;
	GF_BitStream *bs;
	u32 IV_size;

	samp = NULL;
	mc = NULL;
	buf = NULL;
	bs = NULL;

	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Adobe] Cannot find TrackID %d in input file - skipping\n", tci->trackID));
		return GF_OK;
	}

	mc = gf_crypt_open(GF_AES_128, GF_CBC);
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Adobe] Cannot open AES-128 CBC \n"));
		e = GF_IO_ERR;
		goto exit;
	}

	/*Adobe's protection scheme does not support selective key*/
	memcpy(tci->key, tci->keys[0], 16);

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, GF_TRUE);

	e = gf_isom_set_adobe_protection(mp4, track, 1, GF_ISOM_ADOBE_SCHEME, 1, GF_TRUE, tci->metadata, tci->metadata_len);
	if (e) goto  exit;

	count = gf_isom_get_sample_count(mp4, track);
	has_crypted_samp = GF_FALSE;
	if (! gf_isom_has_sync_points(mp4, track))
		all_rap = GF_TRUE;

	gf_isom_set_nalu_extract_mode(mp4, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 0; i < count; i++) {
		Bool is_encrypted_au = GF_TRUE;
		samp = gf_isom_get_sample(mp4, track, i+1, &di);
		if (!samp)
		{
			e = GF_IO_ERR;
			goto exit;
		}

		len = samp->dataLength;
		buf = (char *) gf_malloc(len*sizeof(char));
		memmove(buf, samp->data, len);
		gf_free(samp->data);
		samp->dataLength = 0;

		switch (tci->sel_enc_type) {
		case GF_CRYPT_SELENC_RAP:
			if (!samp->IsRAP)
				gf_isom_get_sample_rap_roll_info(mp4, track, i+1, (Bool *) &samp->IsRAP, NULL, NULL);
			if (!samp->IsRAP && !all_rap) {
				is_encrypted_au = GF_FALSE;
			}
			break;
		case GF_CRYPT_SELENC_NON_RAP:
			if (samp->IsRAP || all_rap) {
				is_encrypted_au = GF_FALSE;
			}
			break;
		default:
			break;
		}

		if (is_encrypted_au) {
			u32 padding_bytes;
			if (!has_crypted_samp) {
				memset(IV, 0, sizeof(char)*16);
				memcpy(IV, tci->first_IV, sizeof(char)*16);
				e = gf_crypt_init(mc, tci->key, IV);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot initialize AES-128 CBC (%s)\n",  gf_error_to_string(e)) );
					gf_crypt_close(mc);
					mc = NULL;
					e = GF_IO_ERR;
					goto exit;
				}
				has_crypted_samp = GF_TRUE;
			}
			else {
				IV_size = 16;
				e = gf_crypt_get_IV(mc, IV, &IV_size);
			}

			padding_bytes = 16 - len % 16;
			len += padding_bytes;
			buf = (char *)gf_realloc(buf, len);
			memset(buf+len-padding_bytes, padding_bytes, padding_bytes);

			gf_crypt_encrypt(mc, buf, len);
		}

		/*rewrite sample with AU header*/
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		if (is_encrypted_au) {
			gf_bs_write_u8(bs, 0x10);
			gf_bs_write_data(bs, (char *) IV, 16);
		}
		else {
			gf_bs_write_u8(bs, 0x0);
		}
		gf_bs_write_data(bs, buf, len);
		gf_bs_get_content(bs, &samp->data, &samp->dataLength);
		gf_bs_del(bs);
		bs = NULL;
		gf_isom_update_sample(mp4, track, i+1, samp, 1);
		gf_isom_sample_del(&samp);
		samp = NULL;
		gf_free(buf);
		buf = NULL;

		gf_set_progress("Adobe's protection scheme Encrypt", i+1, count);
	}
	gf_isom_set_cts_packing(mp4, track, GF_FALSE);
	gf_media_update_bitrate(mp4, track);

exit:
	if (samp) gf_isom_sample_del(&samp);
	if (mc) gf_crypt_close(mc);
	if (buf) gf_free(buf);
	if (bs) gf_bs_del(bs);
	return e;
}

GF_Err gf_adobe_decrypt_track(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk)
{
	GF_Err e;
	u32 track, count, len, i, prev_sample_decrypted, si;
	u8 encrypted_au;
	GF_Crypt *mc;
	GF_ISOSample *samp;
	char IV[17];
	char *ptr;
	GF_BitStream *bs;

	mc = NULL;
	samp = NULL;
	bs = NULL;
	prev_sample_decrypted = GF_FALSE;

	track = gf_isom_get_track_by_id(mp4, tci->trackID);
	if (!track) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot find TrackID %d in input file - skipping\n", tci->trackID));
		return GF_OK;
	}

	mc = gf_crypt_open(GF_AES_128, GF_CBC);
	if (!mc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot open AES-128 CBC\n"));
		e = GF_IO_ERR;
		goto exit;
	}

	memcpy(tci->key, tci->keys[0], 16);

	if (gf_isom_has_time_offset(mp4, track)) gf_isom_set_cts_packing(mp4, track, GF_TRUE);

	count = gf_isom_get_sample_count(mp4, track);
	gf_isom_set_nalu_extract_mode(mp4, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 0; i < count; i++) {
		u32 trim_bytes = 0;
		samp = gf_isom_get_sample(mp4, track, i+1, &si);
		if (!samp)
		{
			e = GF_IO_ERR;
			goto exit;
		}

		ptr = samp->data;
		len = samp->dataLength;

		encrypted_au = ptr[0];
		if (encrypted_au) {
			memmove(IV, ptr+1, 16);
			if (!prev_sample_decrypted) {
				e = gf_crypt_init(mc, tci->key, IV);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot initialize AES-128 CBC (%s)\n", gf_error_to_string(e)) );
					gf_crypt_close(mc);
					mc = NULL;
					e = GF_IO_ERR;
					goto exit;
				}
				prev_sample_decrypted = GF_TRUE;
			}
			else {
				e = gf_crypt_set_IV(mc, IV, GF_AES_128_KEYSIZE);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot set state AES-128 CBC (%s)\n", gf_error_to_string(e)) );
					gf_crypt_close(mc);
					mc = NULL;
					e = GF_IO_ERR;
					goto exit;
				}
			}

			ptr += 17;
			len -= 17;

			gf_crypt_decrypt(mc, ptr, len);
			trim_bytes = ptr[len-1];
		}
		else {
			ptr += 1;
			len -= 1;
		}

		//rewrite decrypted sample
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs, ptr, len - trim_bytes);
		gf_free(samp->data);
		samp->dataLength = 0;
		gf_bs_get_content(bs, &samp->data, &samp->dataLength);
		gf_isom_update_sample(mp4, track, i+1, samp, 1);
		gf_bs_del(bs);
		bs = NULL;
		gf_isom_sample_del(&samp);
		samp = NULL;
		gf_set_progress("Adobe's protection scheme Decrypt", i+1, count);
	}

	/*remove protection info*/
	e = gf_isom_remove_track_protection(mp4, track, 1);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Error Adobe's protection scheme signature from trackID %d: %s\n", tci->trackID, gf_error_to_string(e)));
	}
	gf_isom_set_cts_packing(mp4, track, GF_FALSE);
	gf_media_update_bitrate(mp4, track);

exit:
	if (mc) gf_crypt_close(mc);
	if (samp) gf_isom_sample_del(&samp);
	if (bs) gf_bs_del(bs);
	return e;
}


GF_EXPORT
GF_Err gf_decrypt_file(GF_ISOFile *mp4, const char *drm_file)
{
	GF_Err e;
	u32 i, idx, count, common_idx, nb_tracks, scheme_type;
	const char *scheme_URI="", *KMS_URI="";
	GF_CryptInfo *info;
	Bool is_oma, is_cenc;
	GF_TrackCryptInfo *a_tci, tci;

	is_oma = is_cenc = GF_FALSE;
	count = 0;
	info = NULL;
	if (drm_file) {
		info = load_crypt_file(drm_file, &e);
		if (!info) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open or validate xml file %s\n", drm_file));
			return e;
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
	e = GF_OK;
	for (i=0; i<nb_tracks; i++) {
		Bool is_isma = GF_FALSE;
		Bool is_adobe = GF_FALSE;
		u32 j, nb_stsd = gf_isom_get_sample_description_count(mp4, i+1);

		GF_Err (*gf_decrypt_track)(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk);

		u32 trackID = gf_isom_get_track_id(mp4, i+1);
		for (j=0; j<nb_stsd; j++) {
			scheme_type = gf_isom_is_media_encrypted(mp4, i+1, j+1);
			if (scheme_type) break;
		}
		if (!scheme_type) continue;

		for (idx=0; idx<count; idx++) {
			a_tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
			if (a_tci->trackID == trackID) break;
		}
		if (idx==count) {
			if (!drm_file || info->has_common_key) idx = common_idx;
			/*no available KMS info for this track*/
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] No crypt info found in %s for track %d, keeping track encrypted\n", drm_file, trackID));
				continue;
			}
		}
		if (count) {
			a_tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
			memcpy(&tci, a_tci, sizeof(GF_TrackCryptInfo));
		} else {
			memset(&tci, 0, sizeof(GF_TrackCryptInfo));
			tci.trackID = trackID;
			a_tci = NULL;
		}

		if (a_tci && a_tci->force_type) {
			scheme_type = tci.scheme_type = a_tci->scheme_type;
		}
		if (!tci.trackID)
			tci.trackID = trackID;

		switch (scheme_type) {
		case GF_CRYPT_TYPE_ISMA:
			gf_decrypt_track = gf_ismacryp_decrypt_track;
			is_isma = GF_TRUE;
			break;
		case GF_CRYPT_TYPE_CENC:
		case GF_CRYPT_TYPE_CENS:
		case GF_CRYPT_TYPE_PIFF:
			tci.ctr_mode = GF_TRUE;
		case GF_CRYPT_TYPE_CBC1:
		case GF_CRYPT_TYPE_CBCS:
			is_cenc = GF_TRUE;
			gf_decrypt_track = gf_cenc_decrypt_track;
			break;
		case GF_CRYPT_TYPE_ADOBE:
			is_adobe = GF_TRUE;
			gf_decrypt_track = gf_adobe_decrypt_track;
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Encryption type not supported\n"));
			return GF_NOT_SUPPORTED;
		}

		if (is_isma) {
			e = gf_isom_get_ismacryp_info(mp4, i+1, 1, NULL, &scheme_type, NULL, &scheme_URI, &KMS_URI, NULL, NULL, NULL);
		} else if (gf_isom_is_omadrm_media(mp4, i+1, 1)) {
			if (!drm_file) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot decrypt OMA (P)DCF file without GPAC's DRM file & keys\n"));
				continue;
			}
			KMS_URI = "OMA DRM";
			is_oma = 1;
		} else if (!is_cenc && !is_adobe) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] TrackID %d encrypted with unknown scheme %s - skipping\n", trackID, gf_4cc_to_str(scheme_type) ));
			continue;
		}

		if (scheme_type == GF_CRYPT_TYPE_ISMA) {
			/*get key and salt from KMS*/
			/*GPAC*/
			if (!strnicmp(KMS_URI, "(key)", 5)) {
				char data[100];
				gf_base64_decode((char*)KMS_URI+5, (u32) strlen(KMS_URI)-5, data, 100);
				memcpy(tci.key, data, sizeof(char)*16);
				memcpy(tci.salt, data+16, sizeof(char)*8);
			}
			/*MPEG4IP*/
			else if (!stricmp(KMS_URI, "AudioKey") || !stricmp(KMS_URI, "VideoKey")) {
				if (!gf_ismacryp_mpeg4ip_get_info((char *) KMS_URI, (char *) tci.key, (char *) tci.salt)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Couldn't load MPEG4IP ISMACryp keys for TrackID %d\n", trackID));
					continue;
				}
			} else if (!drm_file) {
				FILE *test = NULL;
				if (!stricmp(scheme_URI, "urn:gpac:isma:encryption_scheme")) test = gf_fopen(KMS_URI, "rt");

				if (!test) {
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[CENC/ISMA] TrackID %d does not contain decryption keys - skipping\n", trackID));
					continue;
				}
				gf_fclose(test);
				if (gf_ismacryp_gpac_get_info(tci.trackID, (char *) KMS_URI, (char *) tci.key, (char *) tci.salt) != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Couldn't load TrackID %d keys in GPAC DRM file %s\n", tci.trackID, KMS_URI));
					continue;
				}
			}

			if (KMS_URI && strlen(tci.KMS_URI) && strcmp(KMS_URI, tci.KMS_URI) )
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] KMS URI for TrackID %d Mismatch: \"%s\" in file vs \"%s\" in licence\n", trackID, KMS_URI, tci.KMS_URI));

			if (drm_file || (KMS_URI && strncmp(KMS_URI, "(key)", 5)) ) {
				strcpy(tci.KMS_URI, KMS_URI ? KMS_URI : "");
			} else {
				strcpy(tci.KMS_URI, "self-contained");
			}
		}
		e = gf_decrypt_track(mp4, &tci, NULL, NULL);
		if (e) break;
	}
	if (is_oma) {
		e = gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_ISO2, 0x00000001);
		if (!e) e = gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_ODCF, 0);
	}

	if (is_cenc && !e)
		e = gf_isom_remove_pssh_box(mp4);
	if (info) del_crypt_info(info);
	return e;
}

static GF_Err gf_cenc_parse_drm_system_info(GF_ISOFile *mp4, const char *drm_file, u32 crypt_type) {
	GF_DOMParser *parser;
	GF_XMLNode *root, *node;
	u32 i;
	GF_Err e = GF_OK;

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, drm_file, NULL, NULL);
	if (e) {
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);
	if (!root) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open or validate xml file %s\n", drm_file));
		return GF_NOT_SUPPORTED;
	}
	i=0;
	while ((node = (GF_XMLNode *) gf_list_enum(root->content, &i))) {
		Bool is_pssh;
		u32 version, cypherMode, specInfoSize, len, KID_count, j;
		bin128 cypherKey, cypherIV, systemID;
		GF_XMLAttribute *att;
		char *data, *specInfo;
		GF_BitStream *bs;
		bin128 *KIDs;
		s32 cypherOffset = -1;
		Bool has_key = GF_FALSE, has_IV = GF_FALSE;

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
			} else if (!strcmp(att->name, "cypher-mode")) {
				/*cypher-mode: 0: data (default mode) -  1: all - 2: clear*/
				if (!strcmp(att->value, "data"))
					cypherMode = 0;
				else if (!strcmp(att->value, "all"))
					cypherMode = 1;
				else if (!strcmp(att->value, "clear"))
					cypherMode = 2;
			} else if (!strcmp(att->name, "cypherKey")) {
				GF_Err e = gf_bin128_parse(att->value, cypherKey);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse cypherKey\n"));
                    return e;
                }
				has_key = GF_TRUE;
			} else if (!strcmp(att->name, "cypherIV")) {
				GF_Err e = gf_bin128_parse(att->value, cypherIV);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse cypherIV\n"));
                    return e;
                }
				has_IV = GF_TRUE;
			} else if (!strcmp(att->name, "cypherOffset")) {
				cypherOffset = atoi(att->value);
			}
		}

		if (!is_pssh) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] Not a Protection System Specific Header Box - skipping\n"));
			continue;
		}



		e = gf_xml_parse_bit_sequence(node, &specInfo, &specInfoSize);
		if (e) {
			if (specInfo) gf_free(specInfo);
			gf_xml_dom_del(parser);
			return e;
		}

		bs = gf_bs_new(specInfo, specInfoSize, GF_BITSTREAM_READ);
		gf_bs_read_data(bs, (char *)systemID, 16);
		if (version) {
			KID_count = gf_bs_read_u32(bs);
			KIDs = (bin128 *)gf_malloc(KID_count*sizeof(bin128));
			for (j = 0; j < KID_count; j++) {
				gf_bs_read_data(bs, (char *)KIDs[j], 16);
			}
		}
		else {
			KID_count = 0;
			KIDs = NULL;
		}
		if (specInfoSize < 16 + (version ? 4 + 16*KID_count : 0)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] Invalid PSSH blob in version %d: size %d key count %d - ignoring PSSH\n", version, specInfoSize, KID_count));
			if (specInfo) gf_free(specInfo);
			if (KIDs) gf_free(KIDs);
			if (bs) gf_bs_del(bs);
			continue;
		}
		len = specInfoSize - 16 - (version ? 4 + 16*KID_count : 0);
		data = (char *)gf_malloc(len*sizeof(char));
		gf_bs_read_data(bs, data, len);

		if (has_key && has_IV && (cypherOffset >= 0) && (cypherMode != 2)) {
			GF_Crypt *gc = gf_crypt_open(GF_AES_128, GF_CTR);
			if (!gc) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open AES-128 CTR\n"));
				return GF_IO_ERR;
			}
			e = gf_crypt_init(gc, cypherKey, cypherIV);
			gf_crypt_encrypt(gc, data+cypherOffset, len-cypherOffset);
			gf_crypt_close(gc);
		}
		if (!e) {
			e = gf_cenc_set_pssh(mp4, systemID, version, KID_count, KIDs, data, len, (crypt_type==GF_CRYPT_TYPE_PIFF) ? GF_TRUE : GF_FALSE);
		}
		if (specInfo) gf_free(specInfo);
		if (data) gf_free(data);
		if (KIDs) gf_free(KIDs);
		if (bs) gf_bs_del(bs);
		if (e) {
			gf_xml_dom_del(parser);
			return e;
		}
	}

	gf_xml_dom_del(parser);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_crypt_file(GF_ISOFile *mp4, const char *drm_file)
{
	GF_Err e;
	u32 i, count, nb_tracks, common_idx, idx;
	GF_CryptInfo *info;
	Bool is_oma, is_encrypted=GF_FALSE;
	GF_TrackCryptInfo *tci;
	Bool check_pssh = GF_FALSE;
	is_oma = 0;

	info = load_crypt_file(drm_file, &e);
	if (!info) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open or validate xml file %s\n", drm_file));
		return e;
	}

	e = GF_OK;
	count = gf_list_count(info->tcis);
	for (i=0; i<count; i++) {
		tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, i);
		/*write pssh box in case of CENC*/
		if ((tci->scheme_type == GF_CRYPT_TYPE_CENC)
			|| (tci->scheme_type == GF_CRYPT_TYPE_CBC1)
			|| (tci->scheme_type == GF_CRYPT_TYPE_CENS)
			|| (tci->scheme_type == GF_CRYPT_TYPE_CBCS)
			|| (tci->scheme_type == GF_CRYPT_TYPE_PIFF)
		) {
			check_pssh = GF_TRUE;
		}
	}
	if (check_pssh) {
		e = gf_cenc_parse_drm_system_info(mp4, drm_file, info->def_crypt_type);
		if (e) return e;
	}

	common_idx=0;
	if (info && info->has_common_key) {
		for (common_idx=0; common_idx<count; common_idx++) {
			tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, common_idx);
			if (!tci->trackID) break;
		}
	}
	nb_tracks = gf_isom_get_track_count(mp4);
	for (i=0; i<nb_tracks; i++) {
		GF_Err (*gf_encrypt_track)(GF_ISOFile *mp4, GF_TrackCryptInfo *tci, void (*progress)(void *cbk, u64 done, u64 total), void *cbk);
		u32 trackID = gf_isom_get_track_id(mp4, i+1);

		if (gf_isom_is_track_encrypted(mp4, i+1)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] Track %d is already encrypted\n", trackID));
			continue;
		}
		for (idx=0; idx<count; idx++) {
			tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
			if (tci->trackID==trackID) break;
		}
		if (idx==count) {
			if (!info->has_common_key) continue;
			idx = common_idx;
		}
		tci = (GF_TrackCryptInfo *)gf_list_get(info->tcis, idx);
		switch (tci->scheme_type) {
		case GF_CRYPT_TYPE_ISMA:
			gf_encrypt_track = gf_ismacryp_encrypt_track;
			break;
		case GF_CRYPT_TYPE_CENC:
		case GF_CRYPT_TYPE_CENS:
		case GF_CRYPT_TYPE_PIFF:
			tci->ctr_mode = GF_TRUE;
		case GF_CRYPT_TYPE_CBC1:
		case GF_CRYPT_TYPE_CBCS:
			gf_encrypt_track = gf_cenc_encrypt_track;
			break;
		case GF_CRYPT_TYPE_ADOBE:
			gf_encrypt_track = gf_adobe_encrypt_track;
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Encryption type not supported\n"));
			return GF_NOT_SUPPORTED;
		}

		/*default to FILE uri*/
		if (!strlen(tci->KMS_URI)) strcpy(tci->KMS_URI, drm_file);

		if (tci->IsEncrypted > 0) {
			GF_TrackCryptInfo bck;
			memcpy(&bck, tci, sizeof(GF_TrackCryptInfo));
			if (!tci->trackID) tci->trackID = trackID;

 			e = gf_encrypt_track(mp4, tci, NULL, NULL);
			memcpy(tci, &bck, sizeof(GF_TrackCryptInfo));
			if (e) break;

			is_encrypted = GF_TRUE;
			if (tci->enc_type == 1) is_oma = 1;
		}
	}

	if (is_oma) {
#if 0
		/*set as OMA V2*/
		e = gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_ODCF, 0x00000002);
		gf_isom_reset_alt_brands(mp4);
#else
		e = gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_OPF2, 1);
#endif
	}

	if (!e && (is_encrypted == GF_FALSE)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] Warning: no track was encrypted (but PSSH was written).\n"));
	}

	del_crypt_info(info);
	return e;
}

#endif /* !defined(GPAC_DISABLE_ISOM_WRITE)*/
#endif /* !defined(GPAC_DISABLE_MCRYPT)*/

