/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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


#if !defined(GPAC_DISABLE_CRYPTO)

static u32 cryptinfo_get_crypt_type(char *cr_type)
{
	if (!stricmp(cr_type, "ISMA") || !stricmp(cr_type, "iAEC"))
		return GF_CRYPT_TYPE_ISMA;
	else if (!stricmp(cr_type, "CENC AES-CTR") || !stricmp(cr_type, "cenc"))
		return GF_CRYPT_TYPE_CENC;
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
			tkc->scheme_type = GF_ISOM_OMADRM_SCHEME;
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
				tkc->scheme_type = cryptinfo_get_crypt_type(att->value);
			}
			else if (!stricmp(att->name, "key")) {
				GF_Err e;
				if (!tkc->keys) {
					tkc->KID_count = 1;
					tkc->keys = gf_malloc(sizeof(bin128));
				}
				e = gf_bin128_parse(att->value, tkc->keys[0] );
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse key value in CrypTrack\n"));
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
					tkc->first_IV[j/2] = v;
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
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Unrecognized selective mode %s, ignoring\n", att->value));
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
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Unrecognized encryption algo %s, ignoring\n", att->value));
				}
			}
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
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Unrecognized SAI location %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "keyRoll")) {
				if (!strncmp(att->value, "idx=", 4))
					tkc->defaultKeyIdx = atoi(att->value+4);
				else if (!strncmp(att->value, "roll=", 5))
					tkc->keyRoll = atoi(att->value+5);
				else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Unrecognized roll parameter %s, ignoring\n", att->value));
				}
			}
			else if (!stricmp(att->name, "metadata")) {
				u32 l = 2 * (u32) strlen(att->value);
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
			else if (!stricmp(att->name, "constant_IV_size")) {
				tkc->constant_IV_size = atoi(att->value);
				assert((tkc->constant_IV_size == 8) || (tkc->constant_IV_size == 16));
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
						tkc->constant_IV[j/2] = v;
					}
				}
				if (!tkc->constant_IV_size) tkc->constant_IV_size = (u8) strlen(sKey) / 2;
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

		if (has_common_key) info->has_common_key = 1;

		if ((tkc->IV_size != 0) && (tkc->IV_size != 8) && (tkc->IV_size != 16)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] wrong IV size %d for AES-128, using 16\n", (u32) tkc->IV_size));
			tkc->IV_size = 16;
		}

		if ((tkc->scheme_type == GF_CRYPT_TYPE_CENC) || (tkc->scheme_type == GF_CRYPT_TYPE_CBC1)) {
			if (tkc->crypt_byte_block || tkc->skip_byte_block) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] Using scheme type %s, crypt_byte_block and skip_byte_block shall be 0\n", gf_4cc_to_str(tkc->scheme_type) ));
				tkc->crypt_byte_block = tkc->skip_byte_block = 0;
			}
		}

		if ((tkc->scheme_type == GF_CRYPT_TYPE_CENC) || (tkc->scheme_type == GF_CRYPT_TYPE_CBC1) || (tkc->scheme_type == GF_CRYPT_TYPE_CENS)) {
			if (tkc->constant_IV_size) {
				if (!tkc->IV_size) {
					tkc->IV_size = tkc->constant_IV_size;
					memcpy(tkc->first_IV, tkc->constant_IV, 16);
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] Using scheme type %s, constant IV shall not be used, using constant IV as first IV\n", gf_4cc_to_str(tkc->scheme_type)));
					tkc->constant_IV_size = 0;
				} else {
					tkc->constant_IV_size = 0;
					memset(tkc->constant_IV, 0, 16);
					GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC] Using scheme type %s, constant IV shall not be used, ignoring\n", gf_4cc_to_str(tkc->scheme_type)));
				}
			}
		}
	}

	if (!strcmp(node_name, "key")) {
		tkc = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		tkc->KIDs = (bin128 *)gf_realloc(tkc->KIDs, sizeof(bin128)*(tkc->KID_count+1));
		tkc->keys = (bin128 *)gf_realloc(tkc->keys, sizeof(bin128)*(tkc->KID_count+1));

		for (i=0; i<nb_attributes; i++) {
			att = (GF_XMLAttribute *) &attributes[i];

			if (!stricmp(att->name, "KID")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->KIDs[tkc->KID_count]);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse KID\n"));
                    return;
                }
			}
			else if (!stricmp(att->name, "value")) {
				GF_Err e = gf_bin128_parse(att->value, tkc->keys[tkc->KID_count]);
                if (e != GF_OK) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannnot parse key value\n"));
                    return;
                }
			}
		}
		tkc->KID_count++;
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
		GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *)gf_list_last(info->tcis);
		if (tci->KIDs) gf_free(tci->KIDs);
		if (tci->keys) gf_free(tci->keys);
		if (tci->metadata) gf_free(tci->metadata);
		if (tci->KMS_URI) gf_free(tci->KMS_URI);
		if (tci->Scheme_URI) gf_free(tci->Scheme_URI);
		if (tci->TextualHeaders) gf_free(tci->TextualHeaders);
		gf_list_rem_last(info->tcis);
		gf_free(tci);
	}
	gf_list_del(info->tcis);
	gf_free(info);
}

GF_CryptInfo *gf_crypt_info_load(const char *file)
{
	GF_Err e;
	GF_CryptInfo *info;
	GF_SAXParser *sax;
	GF_SAFEALLOC(info, GF_CryptInfo);
	if (!info) return NULL;
	info->tcis = gf_list_new();
	sax = gf_xml_sax_new(cryptinfo_node_start, cryptinfo_node_end, cryptinfo_text, info);
	e = gf_xml_sax_parse_file(sax, file, NULL);
	gf_xml_sax_del(sax);
	if (e<0) {
		gf_crypt_info_del(info);
		return NULL;
	}
	return info;
}

static Bool on_decrypt_event(void *_udta, GF_Event *evt)
{
	if (evt->type != GF_EVENT_PROGRESS) return GF_FALSE;
	if (!evt->progress.total) return GF_FALSE;
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Decrypting: % 2.2f %%\r", ((Double)100*evt->progress.done)/evt->progress.total));
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_decrypt_file(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, Double interleave_time, u32 fs_dump_flags)
{
	char szArgs[4096], an_arg[100];
	GF_Filter *src, *dst, *dcrypt;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;

	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Decrypter] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}
	sprintf(szArgs, "mp4dmx:mov=%p", mp4);
	src = gf_fs_load_filter(fsess, szArgs);
	if (!src) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Decrypter] Cannot load demux filter for source file\n"));
		return GF_FILTER_NOT_FOUND;
	}

	sprintf(szArgs, "cdcrypt:FID=1:cfile=%s", drm_file);
	dcrypt = gf_fs_load_filter(fsess, szArgs);
	if (!dcrypt) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Decrypter] Cannot load decryptor filter\n"));
		return GF_FILTER_NOT_FOUND;
	}

	sprintf(szArgs, "SID=1");
	if (interleave_time) {
		sprintf(an_arg, ":cdur=%g", interleave_time);
		strcat(szArgs, an_arg);
	} else {
		strcat(szArgs, ":store=flat");
	}

	dst = gf_fs_load_destination(fsess, dst_file, szArgs, NULL, &e);
	if (!dst) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Decrypter] Cannot load destination muxer\n"));
		return GF_FILTER_NOT_FOUND;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_get_tool_level(GF_LOG_APP)!=GF_LOG_QUIET) {
		gf_fs_enable_reporting(fsess, GF_TRUE);
		gf_fs_set_ui_callback(fsess, on_decrypt_event, fsess);
	}
#endif

	e = gf_fs_run(fsess);
	if (e>GF_OK) e = GF_OK;
	if (!e) e = gf_fs_get_last_connect_error(fsess);
	if (!e) e = gf_fs_get_last_process_error(fsess);

	if (fs_dump_flags & 1) gf_fs_print_stats(fsess);
	if (fs_dump_flags & 2) gf_fs_print_connections(fsess);

	gf_fs_del(fsess);
	return e;
}

static Bool on_crypt_event(void *_udta, GF_Event *evt)
{
	if (evt->type != GF_EVENT_PROGRESS) return GF_FALSE;
	if (!evt->progress.total) return GF_FALSE;
	GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Encrypting: % 2.2f %%\r", ((Double)100*evt->progress.done)/evt->progress.total));
	return GF_FALSE;
}

static GF_Err gf_crypt_file_ex(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, Double interleave_time, const char *fragment_name, u32 fs_dump_flags)
{
	char *szArgs=NULL;
	char an_arg[100];
	char *arg_dst=NULL;

	GF_Filter *src, *dst, *crypt;
	GF_FilterSession *fsess;
	GF_Err e = GF_OK;

	fsess = gf_fs_new_defaults(0);
	if (!fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Encrypter] Failed to create filter session\n"));
		return GF_OUT_OF_MEM;
	}

	sprintf(an_arg, "mp4dmx:mov=%p", mp4);
	gf_dynstrcat(&szArgs, an_arg, NULL);
	if (fragment_name) {
		gf_dynstrcat(&szArgs, ":catseg=", NULL);
		gf_dynstrcat(&szArgs, fragment_name, NULL);
	}
	src = gf_fs_load_filter(fsess, szArgs);

	gf_free(szArgs);
	szArgs = NULL;


	if (!src) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Encrypter] Cannot load demux for source file\n"));
		return GF_FILTER_NOT_FOUND;
	}

	gf_dynstrcat(&szArgs, "cecrypt:FID=1:cfile=", NULL);
	gf_dynstrcat(&szArgs, drm_file, NULL);
	crypt = gf_fs_load_filter(fsess, szArgs);

	gf_free(szArgs);
	szArgs = NULL;

	if (!crypt) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Encrypter] Cannot load encryptor\n"));
		return GF_FILTER_NOT_FOUND;
	}

	gf_dynstrcat(&szArgs, "SID=1", NULL);
	if (fragment_name)
		gf_dynstrcat(&szArgs, ":sseg", NULL);
	else {
		if (interleave_time) {
			sprintf(an_arg, ":cdur=%g", interleave_time);
			gf_dynstrcat(&szArgs, an_arg, NULL);
		} else {
			gf_dynstrcat(&szArgs, ":store=flat", NULL);
		}
	}

	arg_dst = strchr(dst_file, ':');
	if (arg_dst) {
		gf_dynstrcat(&szArgs, arg_dst, NULL);
		arg_dst[0]=0;
	}
	dst = gf_fs_load_destination(fsess, dst_file, szArgs, NULL, &e);

	gf_free(szArgs);
	szArgs = NULL;
	if (!dst) {
		gf_fs_del(fsess);
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[Encrypter] Cannot load destination muxer\n"));
		return GF_FILTER_NOT_FOUND;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_get_tool_level(GF_LOG_APP)!=GF_LOG_QUIET) {
		gf_fs_enable_reporting(fsess, GF_TRUE);
		gf_fs_set_ui_callback(fsess, on_crypt_event, fsess);
	}
#endif
	e = gf_fs_run(fsess);
	if (e>GF_OK) e = GF_OK;
	if (!e) e = gf_fs_get_last_connect_error(fsess);
	if (!e) e = gf_fs_get_last_process_error(fsess);

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
#endif /* !defined(GPAC_DISABLE_ISOM_WRITE)*/

