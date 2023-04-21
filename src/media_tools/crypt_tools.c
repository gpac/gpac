/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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



#include <gpac/crypt_tools.h>
#include <gpac/xml.h>
#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/filters.h>
#include <gpac/network.h>


#if !defined(GPAC_DISABLE_CRYPTO)

static u32 cryptinfo_get_crypt_type(char *cr_type)
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
	else if (!stricmp(cr_type, "OMA"))
		return GF_ISOM_OMADRM_SCHEME;
	else if (!stricmp(cr_type, "HLS SAES"))
		return GF_HLS_SAMPLE_AES_SCHEME;

	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] Unrecognized crypto type %s\n", cr_type));
	return 0;
}

static void cryptinfo_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
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
				info->def_crypt_type = cryptinfo_get_crypt_type(att->value);
			}
		}
		return;
	}


	if (!strcmp(node_name, "CrypTrack")) {
		Bool has_key = GF_FALSE;
		Bool has_common_key = GF_TRUE;
		GF_SAFEALLOC(tkc, GF_TrackCryptInfo);
		if (!tkc) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] Cannnot allocate crypt track, skipping\n"));
			info->last_parse_error = GF_OUT_OF_MEM;
			return;
		}
		//by default track is encrypted
		tkc->IsEncrypted = 1;
		tkc->sai_saved_box_type = GF_ISOM_BOX_TYPE_SENC;
		tkc->scheme_type = info->def_crypt_type;

		//allocate a key to store the default values in single-key mode
		tkc->keys = gf_malloc(sizeof(GF_CryptKeyInfo));
		if (!tkc->keys) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] Cannnot allocate key IDs\n"));
			gf_free(tkc);
			info->last_parse_error = GF_OUT_OF_MEM;
			return;
		}
		memset(tkc->keys, 0, sizeof(GF_CryptKeyInfo));
		gf_list_add(info->tcis, tkc);

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
				tkc->scheme_type = cryptinfo_get_crypt_type(att->value);
			}
			else if (!stricmp(att->name, "forceType")) {
				tkc->force_type = GF_TRUE;
			}
			else if (!stricmp(att->name, "key")) {
				GF_Err e;
				has_key = GF_TRUE;
				e = gf_bin128_parse(att->value, tkc->keys[0].key );
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Cannnot parse key value in CrypTrack\n"));
					info->last_parse_error = GF_BAD_PARAM;
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
					tkc->keys[0].IV[j/2] = v;
					if (j>=30) break;
				}
			}
			else if (!stricmp(att->name, "kms_URI") || !stricmp(att->name, "rightsIssuerURL")) {
				if (tkc->KMS_URI) gf_free(tkc->KMS_URI);
				tkc->KMS_URI = gf_strdup(att->value);
			}
			else if (!stricmp(att->name, "scheme_URI") || !stricmp(att->name, "contentID")) {
				if (tkc->Scheme_URI) gf_free(tkc->Scheme_URI);
				tkc->Scheme_URI = gf_strdup(att->value);
			}
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
				else if (!stricmp(att->value, "None")) {
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Unrecognized selective mode %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "clearStsd")) {
				if (!strcmp(att->value, "none")) tkc->force_clear_stsd_idx = 0;
				else if (!strcmp(att->value, "before")) tkc->force_clear_stsd_idx = 1;
				else if (!strcmp(att->value, "after")) tkc->force_clear_stsd_idx = 2;
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Unrecognized clear stsd type %s, defaulting to no stsd for clear samples\n", att->value));
				}
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
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Unrecognized encryption algo %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "transactionID")) {
				if (strlen(att->value)<=16) strcpy(tkc->TransactionID, att->value);
			}
			else if (!stricmp(att->name, "textualHeaders")) {
			}
			else if (!stricmp(att->name, "IsEncrypted")) {
				if (!stricmp(att->value, "1"))
					tkc->IsEncrypted = 1;
				else
					tkc->IsEncrypted = 0;
			}
			else if (!stricmp(att->name, "IV_size") && (tkc->scheme_type != GF_CRYPT_TYPE_CBCS)) {
				tkc->keys[0].IV_size = atoi(att->value);
			}
			else if (!stricmp(att->name, "first_IV") && (tkc->scheme_type != GF_CRYPT_TYPE_CBCS)) {
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				if ((strlen(sKey) == 16) || (strlen(sKey) == 32)) {
					u32 j;
					for (j=0; j<strlen(sKey); j+=2) {
						u32 v;
						char szV[5];
						sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
						sscanf(szV, "%x", &v);
						tkc->keys[0].IV[j/2] = v;
					}
					if (!tkc->keys[0].IV_size) tkc->keys[0].IV_size = (u8) strlen(sKey) / 2;
				}
			}
			else if (!stricmp(att->name, "saiSavedBox")) {
				if (!stricmp(att->value, "uuid_psec")) tkc->sai_saved_box_type = GF_ISOM_BOX_UUID_PSEC;
				else if (!stricmp(att->value, "senc")) tkc->sai_saved_box_type = GF_ISOM_BOX_TYPE_SENC;
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Unrecognized SAI location %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "keyRoll")) {
				if (!strncmp(att->value, "idx=", 4))
					tkc->defaultKeyIdx = atoi(att->value+4);
				else if (!strncmp(att->value, "roll", 4) || !strncmp(att->value, "samp", 4)) {
					tkc->roll_type = GF_KEYROLL_SAMPLES;
					if (att->value[4]=='=') tkc->keyRoll = atoi(att->value+5);
					if (!tkc->keyRoll) tkc->keyRoll = 1;
				}
				else if (!strncmp(att->value, "seg", 3)) {
					tkc->roll_type = GF_KEYROLL_SEGMENTS;
					if (att->value[3]=='=') tkc->keyRoll = atoi(att->value+4);
					if (!tkc->keyRoll) tkc->keyRoll = 1;
				} else if (!strncmp(att->value, "period", 6)) {
					tkc->roll_type = GF_KEYROLL_PERIODS;
					if (att->value[6]=='=') tkc->keyRoll = atoi(att->value+7);
					if (!tkc->keyRoll) tkc->keyRoll = 1;
				} else if (!strcmp(att->value, "rap")) {
					tkc->roll_type = GF_KEYROLL_SAPS;
					if (att->value[3]=='=') tkc->keyRoll = atoi(att->value+4);
					if (!tkc->keyRoll) tkc->keyRoll = 1;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Unrecognized roll parameter %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "random")) {
				if (!strcmp(att->value, "true") || !strcmp(att->value, "1") || !strcmp(att->value, "yes")) {
					tkc->rand_keys=GF_TRUE;
				}
				else if (!strcmp(att->value, "false") || !strcmp(att->value, "0") || !strcmp(att->value, "no")) {
					tkc->rand_keys=GF_FALSE;
				}
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Unrecognized random parameter %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "metadata")) {
				u32 l = 2 * (u32) strlen(att->value) + 3;
				tkc->metadata = gf_malloc(sizeof(char) * l);
				l = gf_base64_encode(att->value, (u32) strlen(att->value), tkc->metadata, l);
				tkc->metadata[l] = 0;
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
			else if (!stricmp(att->name, "constant_IV_size")
				|| (!stricmp(att->name, "IV_size") && (tkc->scheme_type == GF_CRYPT_TYPE_CBCS))
			) {
				tkc->keys[0].constant_IV_size = atoi(att->value);
				if ((tkc->keys[0].constant_IV_size != 8) && (tkc->keys[0].constant_IV_size != 16)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Constant IV size %d is not 8 or 16\n", att->value));
				}
			}
			else if (!stricmp(att->name, "constant_IV")
				|| (!stricmp(att->name, "first_IV") && (tkc->scheme_type == GF_CRYPT_TYPE_CBCS))
			) {
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				if ((strlen(sKey) == 16) || (strlen(sKey) == 32)) {
					u32 j;
					for (j=0; j<strlen(sKey); j+=2) {
						u32 v;
						char szV[5];
						sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
						sscanf(szV, "%x", &v);
						tkc->keys[0].IV[j/2] = v;
					}
					if (!tkc->keys[0].constant_IV_size) tkc->keys[0].constant_IV_size = (u8) strlen(sKey) / 2;
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
			else if (!stricmp(att->name, "subsamples")) {
				char *val = att->value;
				while (val) {
					char *sep = strchr(val, ';');
					if (sep) sep[0] = 0;
					if (!strncmp(val, "subs=", 5)) {
						if (tkc->subs_crypt) gf_free(tkc->subs_crypt);
						tkc->subs_crypt = gf_strdup(val+4);
					}
					else if (!strncmp(val, "rand", 4)) {
						tkc->subs_rand = 2;
						if (val[4]=='=')
							tkc->subs_rand = atoi(val+5);
					}
					else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] unrecognized attribute value %s for `subsamples`, ignoring\n", val));
					}
					if (!sep) break;
					sep[0] = ';';
					val = sep+1;
				}
			}
			else if (!stricmp(att->name, "multiKey")) {
				if (!strcmp(att->value, "all") || !strcmp(att->value, "on")) tkc->multi_key = GF_TRUE;
				else if (!strcmp(att->value, "no")) tkc->multi_key = GF_FALSE;
				else {
					char *val = att->value;
					tkc->multi_key = GF_TRUE;
					while (val) {
						char *sep = strchr(val, ';');
						if (sep) sep[0] = 0;
						if (!strncmp(val, "roll=", 5)) {
							tkc->mkey_roll_plus_one = 1 + atoi(val+5);
						}
						else if (!strncmp(val, "subs=", 5)) {
							if (tkc->mkey_subs) gf_free(tkc->mkey_subs);
							tkc->mkey_subs = gf_strdup(val+5);
						}
						else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] unrecognized attribute value %s for `multiKey`, ignoring\n", val));
							tkc->multi_key = GF_FALSE;
							if (sep) sep[0] = ';';
							break;
						}
						if (!sep) break;
						sep[0] = ';';
						val = sep+1;
					}
				}
			}
		}
		if (tkc->scheme_type==GF_CRYPT_TYPE_PIFF) {
			tkc->sai_saved_box_type = GF_ISOM_BOX_UUID_PSEC;
		}
		if (has_common_key) info->has_common_key = 1;

		if ((tkc->keys[0].IV_size != 0) && (tkc->keys[0].IV_size != 8) && (tkc->keys[0].IV_size != 16)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] wrong IV size %d for AES-128, using 16\n", (u32) tkc->keys[0].IV_size));
			tkc->keys[0].IV_size = 16;
		}

		if ((tkc->scheme_type == GF_CRYPT_TYPE_CENC) || (tkc->scheme_type == GF_CRYPT_TYPE_CBC1)) {
			if (tkc->crypt_byte_block || tkc->skip_byte_block) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] Using scheme type %s, crypt_byte_block and skip_byte_block shall be 0\n", gf_4cc_to_str(tkc->scheme_type) ));
				tkc->crypt_byte_block = tkc->skip_byte_block = 0;
			}
		}

		if ((tkc->scheme_type == GF_CRYPT_TYPE_CENC) || (tkc->scheme_type == GF_CRYPT_TYPE_CBC1) || (tkc->scheme_type == GF_CRYPT_TYPE_CENS)) {
			if (tkc->keys[0].constant_IV_size) {
				if (!tkc->keys[0].IV_size) {
					tkc->keys[0].IV_size = tkc->keys[0].constant_IV_size;
					GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] Using scheme type %s, constant IV shall not be used, using constant IV as first IV\n", gf_4cc_to_str(tkc->scheme_type)));
					tkc->keys[0].constant_IV_size = 0;
				} else {
					tkc->keys[0].constant_IV_size = 0;
					GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] Using scheme type %s, constant IV shall not be used, ignoring\n", gf_4cc_to_str(tkc->scheme_type)));
				}
			}
		}
		if (tkc->scheme_type == GF_ISOM_OMADRM_SCHEME) {
			/*default to AES 128 in OMA*/
			tkc->encryption = 2;
		}

		if (has_key) tkc->nb_keys = 1;
	}

	if (!strcmp(node_name, "key")) {
		u32 IV_size, const_IV_size;
		Bool kas_civ = GF_FALSE;
		tkc = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		if (!tkc) return;
		//only realloc for 2nd and more
		if (tkc->nb_keys) {
			tkc->keys = (GF_CryptKeyInfo *)gf_realloc(tkc->keys, sizeof(GF_CryptKeyInfo)*(tkc->nb_keys+1));
			memset(&tkc->keys[tkc->nb_keys], 0, sizeof(GF_CryptKeyInfo));
		}
		IV_size = tkc->keys[0].IV_size;
		const_IV_size = tkc->keys[0].constant_IV_size;

		for (i=0; i<nb_attributes; i++) {
			att = (GF_XMLAttribute *) &attributes[i];

			if (!stricmp(att->name, "KID")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->keys[tkc->nb_keys].KID);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Cannnot parse KID\n"));
                    return;
                }
			}
			else if (!stricmp(att->name, "value")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->keys[tkc->nb_keys].key);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Cannnot parse key value\n"));
                    return;
                }
			}
			else if (!stricmp(att->name, "hlsInfo")) {
				if (!strstr(att->value, "URI=\"")) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[CENC] Missing URI in HLS info %s\n", att->value));
                    return;
				}
				if (tkc->keys[tkc->nb_keys].hls_info) gf_free(tkc->keys[tkc->nb_keys].hls_info);
				tkc->keys[tkc->nb_keys].hls_info = gf_strdup(att->value);
			}
			else if (!stricmp(att->name, "IV_size")) {
				IV_size = atoi(att->value);
			}
			else if (!stricmp(att->name, "constant_IV")) {
				char *sKey = att->value;
				if (!strnicmp(sKey, "0x", 2)) sKey += 2;
				if ((strlen(sKey) == 16) || (strlen(sKey) == 32)) {
					u32 j;
					for (j=0; j<strlen(sKey); j+=2) {
						u32 v;
						char szV[5];
						sprintf(szV, "%c%c", sKey[j], sKey[j+1]);
						sscanf(szV, "%x", &v);
						tkc->keys[tkc->nb_keys].IV[j/2] = v;
					}
					const_IV_size = (u8) strlen(sKey) / 2;
					kas_civ = GF_TRUE;
				}
			}
			else if (!stricmp(att->name, "rep")) {
				if (tkc->keys[tkc->nb_keys].repID) gf_free(tkc->keys[tkc->nb_keys].repID);
				tkc->keys[tkc->nb_keys].repID = gf_strdup(att->value);
			}
			else if (!stricmp(att->name, "period")) {
				if (tkc->keys[tkc->nb_keys].periodID) gf_free(tkc->keys[tkc->nb_keys].periodID);
				tkc->keys[tkc->nb_keys].periodID = gf_strdup(att->value);
			}
			else if (!stricmp(att->name, "as")) {
				tkc->keys[tkc->nb_keys].ASID = atoi(att->value);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[CENC] unrecognized attribute %s for `key`, ignoring\n", att->name));
			}
		}
		tkc->keys[tkc->nb_keys].IV_size = IV_size;
		tkc->keys[tkc->nb_keys].constant_IV_size = const_IV_size;
		if (!kas_civ && tkc->nb_keys)
			memcpy(tkc->keys[tkc->nb_keys].IV, tkc->keys[0].IV, 16);
		tkc->nb_keys++;
	}
}

static void cryptinfo_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	GF_CryptInfo *info = (GF_CryptInfo *)sax_cbck;
	if (!strcmp(node_name, "OMATextHeader")) {
		info->in_text_header = 0;
		return;
	}
}

static void cryptinfo_text(void *sax_cbck, const char *text, Bool is_cdata)
{
	u32 len, len2;
	GF_TrackCryptInfo *tkc;
	GF_CryptInfo *info = (GF_CryptInfo *)sax_cbck;

	if (!info->in_text_header) return;

	tkc = (GF_TrackCryptInfo *) gf_list_last(info->tcis);
	len = (u32) strlen(text);
	len2 = tkc->TextualHeaders ? (u32) strlen(tkc->TextualHeaders) : 0;

	tkc->TextualHeaders = gf_realloc(tkc->TextualHeaders, sizeof(char) * (len+len2+1));
	if (!len2) strcpy(tkc->TextualHeaders, "");
	strcat(tkc->TextualHeaders, text);
}

void gf_crypt_info_del(GF_CryptInfo *info)
{
	while (gf_list_count(info->tcis)) {
		u32 i;
		GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		for (i=0; i<tci->nb_keys; i++) {
			if (tci->keys[i].hls_info)
				gf_free(tci->keys[i].hls_info);
			if (tci->keys[i].repID)
				gf_free(tci->keys[i].repID);
			if (tci->keys[i].periodID)
				gf_free(tci->keys[i].periodID);
		}
		if (tci->keys) gf_free(tci->keys);
		if (tci->metadata) gf_free(tci->metadata);
		if (tci->KMS_URI) gf_free(tci->KMS_URI);
		if (tci->Scheme_URI) gf_free(tci->Scheme_URI);
		if (tci->TextualHeaders) gf_free(tci->TextualHeaders);
		if (tci->subs_crypt) gf_free(tci->subs_crypt);
		if (tci->mkey_subs) gf_free(tci->mkey_subs);
		gf_list_rem_last(info->tcis);
		gf_free(tci);
	}
	gf_list_del(info->tcis);
	gf_free(info);
}

GF_CryptInfo *gf_crypt_info_load(const char *file, GF_Err *out_err)
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
	sax = gf_xml_sax_new(cryptinfo_node_start, cryptinfo_node_end, cryptinfo_text, info);
	e = gf_xml_sax_parse_file(sax, file, NULL);
	if (e<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[DRM] Failed to parse DRM config file: %s", gf_xml_sax_get_error(sax) ));
		if (out_err) *out_err = e;
		gf_crypt_info_del(info);
		info = NULL;
	} else if (info->last_parse_error) {
		if (out_err) *out_err = info->last_parse_error;
		gf_crypt_info_del(info);
		info = NULL;
	} else {
		if (out_err) *out_err = GF_OK;
	}
	gf_xml_sax_del(sax);
	return info;
}

#ifndef GPAC_DISABLE_ISOM

extern char gf_prog_lf;

static Bool on_decrypt_event(void *_udta, GF_Event *evt)
{
	Double progress;
	u32 *prev_progress = (u32 *)_udta;
	if (!_udta) return GF_FALSE;
	if (evt->type != GF_EVENT_PROGRESS) return GF_FALSE;
	if (!evt->progress.total) return GF_FALSE;

	progress = (Double) (100*evt->progress.done) / evt->progress.total;
	if ((u32) progress==*prev_progress)
		return GF_FALSE;

	*prev_progress = (u32) progress;
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Decrypting: % 2.2f %%%c", progress, gf_prog_lf));
#else
	fprintf(stderr, "Decrypting: % 2.2f %%%c", progress, gf_prog_lf);
#endif
	return GF_FALSE;
}

static GF_Err gf_decrypt_file_ex(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, Double interleave_time, const char *fragment_name, u32 fs_dump_flags)
{
	char *szArgs = NULL;
	char an_arg[100];
	GF_Filter *src, *dst, *dcrypt;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;
	u32 progress = (u32) -1;

	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Decrypter] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}

	sprintf(an_arg, "mp4dmx:mov=%p", mp4);
	gf_dynstrcat(&szArgs, an_arg, NULL);
	if (fragment_name) {
		gf_dynstrcat(&szArgs, ":sigfrag:catseg=", NULL);
		gf_dynstrcat(&szArgs, fragment_name, NULL);
	}
	src = gf_fs_load_filter(fsess, szArgs, &e);
	gf_free(szArgs);
	szArgs = NULL;

	if (!src) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Decrypter] Cannot load demux filter for source file\n"));
		return e;
	}

	gf_dynstrcat(&szArgs, "cdcrypt:FID=1", NULL);
	if (drm_file) {
		gf_dynstrcat(&szArgs, ":cfile=", NULL);
		gf_dynstrcat(&szArgs, drm_file, NULL);
	}
	dcrypt = gf_fs_load_filter(fsess, szArgs, &e);
	gf_free(szArgs);
	szArgs = NULL;
	if (!dcrypt) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Decrypter] Cannot load decryptor filter\n"));
		return e;
	}

	gf_dynstrcat(&szArgs, "SID=1", NULL);
	if (fragment_name) {
		gf_dynstrcat(&szArgs, ":sseg:noinit:store=frag:refrag:cdur=1000000000", NULL);
	} else {
		if (interleave_time) {
			sprintf(an_arg, ":cdur=%g", interleave_time);
			gf_dynstrcat(&szArgs, an_arg, NULL);
		} else {
			gf_dynstrcat(&szArgs, ":store=flat", NULL);
		}
	}
	gf_dynstrcat(&szArgs, ":xps_inband=auto", NULL);
	
	if (gf_isom_has_keep_utc_times(mp4))
		gf_dynstrcat(&szArgs, ":keep_utc", NULL);

	dst = gf_fs_load_destination(fsess, dst_file, szArgs, NULL, &e);
	gf_free(szArgs);
	szArgs = NULL;

	if (!dst) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Decrypter] Cannot load destination muxer\n"));
		return GF_FILTER_NOT_FOUND;
	}

	if (!gf_sys_is_test_mode()
#ifndef GPAC_DISABLE_LOG
		&& (gf_log_get_tool_level(GF_LOG_APP)!=GF_LOG_QUIET)
#endif
		&& !gf_sys_is_quiet()
	) {
		gf_fs_enable_reporting(fsess, GF_TRUE);
		gf_fs_set_ui_callback(fsess, on_decrypt_event, &progress);
	}
#ifdef GPAC_ENABLE_COVERAGE
	else if (gf_sys_is_cov_mode()) {
		on_decrypt_event(NULL, NULL);
	}
#endif //GPAC_ENABLE_COVERAGE

	e = gf_fs_run(fsess);
	if (e>GF_OK) e = GF_OK;
	if (!e) e = gf_fs_get_last_connect_error(fsess);
	if (!e) e = gf_fs_get_last_process_error(fsess);

	if (!e) gf_fs_print_unused_args(fsess, NULL);
	gf_fs_print_non_connected(fsess);
	if (fs_dump_flags & 1) gf_fs_print_stats(fsess);
	if (fs_dump_flags & 2) gf_fs_print_connections(fsess);

	gf_fs_del(fsess);
	return e;
}

GF_EXPORT
GF_Err gf_decrypt_fragment(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, const char *fragment_name, u32 fs_dump_flags)
{
	return gf_decrypt_file_ex(mp4, drm_file, dst_file, 0, fragment_name, fs_dump_flags);
}
GF_EXPORT
GF_Err gf_decrypt_file(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, Double interleave_time, u32 fs_dump_flags)
{
	return gf_decrypt_file_ex(mp4, drm_file, dst_file, interleave_time, NULL, fs_dump_flags);
}
static Bool on_crypt_event(void *_udta, GF_Event *evt)
{
	Double progress;
	u32 *prev_progress = (u32 *)_udta;
	if (!_udta) return GF_FALSE;
	if (evt->type != GF_EVENT_PROGRESS) return GF_FALSE;
	if (!evt->progress.total) return GF_FALSE;

	progress = (Double) (100*evt->progress.done) / evt->progress.total;
	if ((u32) progress==*prev_progress)
		return GF_FALSE;

	*prev_progress = (u32) progress;
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Encrypting: % 2.2f %%%c", progress, gf_prog_lf));
#else
	fprintf(stderr, "Encrypting: % 2.2f %%%c", progress, gf_prog_lf);
#endif
	return GF_FALSE;
}

static GF_Err gf_crypt_file_ex(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, Double interleave_time, const char *fragment_name, u32 fs_dump_flags)
{
	char *szArgs=NULL;
	char an_arg[100];
	char *arg_dst=NULL;
	u32 progress = (u32) -1;
	GF_Filter *src, *dst, *crypt;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;

	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Encrypter] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}

	sprintf(an_arg, "mp4dmx:mov=%p", mp4);
	gf_dynstrcat(&szArgs, an_arg, NULL);
	if (fragment_name) {
		gf_dynstrcat(&szArgs, ":sigfrag:catseg=", NULL);
		gf_dynstrcat(&szArgs, fragment_name, NULL);
	}
	src = gf_fs_load_filter(fsess, szArgs, &e);

	gf_free(szArgs);
	szArgs = NULL;

	if (!src) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Encrypter] Cannot load demux for source file: %s\n", gf_error_to_string(e)));
		return e;
	}

	gf_dynstrcat(&szArgs, "cecrypt:FID=1:cfile=", NULL);
	gf_dynstrcat(&szArgs, drm_file, NULL);
	crypt = gf_fs_load_filter(fsess, szArgs, &e);

	gf_free(szArgs);
	szArgs = NULL;

	if (!crypt) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Encrypter] Cannot load encryptor: %s\n", gf_error_to_string(e) ));
		return e;
	}

	gf_dynstrcat(&szArgs, "SID=1", NULL);
	if (fragment_name) {
		gf_dynstrcat(&szArgs, ":sseg:noinit:store=frag:refrag:cdur=1000000000", NULL);
	} else {
		if (interleave_time) {
			sprintf(an_arg, ":cdur=%g", interleave_time);
			gf_dynstrcat(&szArgs, an_arg, NULL);
		} else {
			gf_dynstrcat(&szArgs, ":store=flat", NULL);
		}
	}
	gf_dynstrcat(&szArgs, ":xps_inband=auto", NULL);

	if (gf_isom_has_keep_utc_times(mp4))
		gf_dynstrcat(&szArgs, ":keep_utc", NULL);

	arg_dst = gf_url_colon_suffix(dst_file, '=');
	if (arg_dst) {
		gf_dynstrcat(&szArgs, arg_dst, NULL);
		arg_dst[0]=0;
	}
	dst = gf_fs_load_destination(fsess, dst_file, szArgs, NULL, &e);

	gf_free(szArgs);
	szArgs = NULL;
	if (!dst) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Encrypter] Cannot load destination muxer\n"));
		return GF_FILTER_NOT_FOUND;
	}

	if (!gf_sys_is_test_mode()
#ifndef GPAC_DISABLE_LOG
		&& (gf_log_get_tool_level(GF_LOG_APP)!=GF_LOG_QUIET)
#endif
		&& !gf_sys_is_quiet()
	) {
		gf_fs_enable_reporting(fsess, GF_TRUE);
		gf_fs_set_ui_callback(fsess, on_crypt_event, &progress);
	}
#ifdef GPAC_ENABLE_COVERAGE
	else if (gf_sys_is_cov_mode()) {
		on_crypt_event(NULL, NULL);
	}
#endif //GPAC_ENABLE_COVERAGE
	e = gf_fs_run(fsess);
	if (e>GF_OK) e = GF_OK;
	if (!e) e = gf_fs_get_last_connect_error(fsess);
	if (!e) e = gf_fs_get_last_process_error(fsess);

	if (!e) gf_fs_print_unused_args(fsess, NULL);
	gf_fs_print_non_connected(fsess);
	if (fs_dump_flags & 1) gf_fs_print_stats(fsess);
	if (fs_dump_flags & 2) gf_fs_print_connections(fsess);
	gf_fs_del(fsess);
	return e;
}

GF_EXPORT
GF_Err gf_crypt_fragment(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, const char *fragment_name, u32 fs_dump_flags)
{
	return gf_crypt_file_ex(mp4, drm_file, dst_file, 0, fragment_name, fs_dump_flags);
}

GF_EXPORT
GF_Err gf_crypt_file(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, Double interleave_time, u32 fs_dump_flags)
{
	return gf_crypt_file_ex(mp4, drm_file, dst_file, interleave_time, NULL, fs_dump_flags);

}
#endif //GPAC_DISABLE_ISOM


#endif /* !defined(GPAC_DISABLE_CRYPTO)*/

