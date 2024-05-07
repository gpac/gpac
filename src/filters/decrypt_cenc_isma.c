/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / CENC and ISMA decrypt filter
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
#include <gpac/network.h>
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_CRYPTO) && !defined(GPAC_DISABLE_CDCRYPT)

//#define OLD_KEY_FETCHERS

//old key fetchers require sync download
#if defined(GPAC_CONFIG_EMSCRIPTEN) && defined(OLD_KEY_FETCHERS)
#undef OLD_KEY_FETCHERS
#endif

enum
{
	DECRYPT_STATE_ERROR=1,
	DECRYPT_STATE_SETUP,
	DECRYPT_STATE_PLAY,
};

enum
{
	DECRYPT_FULL=0,
	DECRYPT_NOKEY,
	DECRYPT_SKIP,
	DECRYPT_PAD0,
	DECRYPT_PAD1,
	DECRYPT_PADSC,
};

typedef struct
{
	GF_Crypt *crypt;
	bin128 key;
	u32 key_valid;
} CENCDecKey;

typedef struct
{
	const char *cfile;
	u32 decrypt;
	GF_PropUIntList drop_keys;
	GF_PropStringList kids;
	GF_PropStringList keys;
	GF_CryptInfo *cinfo;
	Bool hls_cenc_patch_iv;

	GF_Filter *filter;
	GF_List *streams;
	GF_BitStream *bs_r;

	GF_DownloadManager *dm;
	u32 pending_keys;

} GF_CENCDecCtx;

typedef struct
{
	GF_CENCDecCtx *ctx;
	GF_FilterPid *ipid, *opid;

	u32 state;
	u32 pssh_crc;
	u32 scheme_type, scheme_version;

	GF_Err key_error;

	CENCDecKey *crypts;
	u32 nb_crypts;

	//ISMA & OMA
	char salt[8];
	u64 last_IV;
	u32 nb_allow_play;
	Bool is_oma, is_adobe;
	u32 preview_range;
	Bool is_nalu;
	Bool selective_encryption;
	u32 IV_length, KI_length;

	/*CENC*/
	Bool is_cenc;
	Bool is_cbc;
	u32 KID_count;
	Bool inband_keys;
	bin128 *KIDs;
	bin128 *keys;

	const GF_PropertyValue *cenc_ki;
	u32 multikey;
	const GF_PropertyValue *cenc_pattern;

	/*adobe and CENC*/
	Bool crypt_init;

	Bool is_hls;
	bin128 hls_IV;
	char *hls_key_url;
	Bool is_hls_saes;
	u32 codec_id;

	Bool force_hls_iv;

	Bool gpac_master_leaf;
	bin128 master_key;

	u32 clearkey_crc;
	char *body;
	u32 res_size;
	Bool hdr_done;
	GF_DownloadSession *sess;


	u32 rep_crc, per_crc, as_id;
} GF_CENCDecStream;


#ifdef OLD_KEY_FETCHERS
static void cenc_dec_kms_netio(void *cbck, GF_NETIO_Parameter *par)
{
}

static GF_Err gf_ismacryp_gpac_get_info(u32 stream_id, char *drm_file, char *key, char *salt)
{
	GF_Err e;
	u32 i, count;
	GF_CryptInfo *info;

	e = GF_OK;
	info = gf_crypt_info_load(drm_file, &e);
	if (!info) return e;
	count = gf_list_count(info->tcis);
	for (i=0; i<count; i++) {
		GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *) gf_list_get(info->tcis, i);
		if ((info->has_common_key && !tci->trackID) || (tci->trackID == stream_id) ) {
			if (tci->KID_count)
				memcpy(key, tci->keys[0], sizeof(char)*16);
			memcpy(salt, tci->first_IV, sizeof(char)*8);
			e = GF_OK;
			break;
		}
	}
	gf_crypt_info_del(info);
	return e;
}

static GF_Err cenc_dec_get_gpac_kms(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, const char *kms_url)
{
	const GF_PropertyValue *prop;
	FILE *t;
	u32 id = 0;
	GF_FilterPid *pid = cstr->ipid;
	GF_Err e;
	GF_DownloadSession * sess;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (!prop) prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (prop) id = prop->value.uint;

	if (!strnicmp(kms_url, "(ipmp)", 6)) return GF_NOT_SUPPORTED;
	else if (!strnicmp(kms_url, "(uri)", 5)) kms_url += 5;
	else if (!strnicmp(kms_url, "file://", 7)) kms_url += 7;

	/*try local*/
	t = (strstr(kms_url, "://") == NULL) ? gf_fopen(kms_url, "rb") : NULL;
	if (t) {
		gf_fclose(t);
		return gf_ismacryp_gpac_get_info(id, (char *)kms_url, cstr->key, cstr->salt);
	}
	/*note that gpac doesn't have TLS support -> not really useful. As a general remark, ISMACryp
	is supported as a proof of concept, crypto and IPMP being the last priority on gpac...*/
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[CENC/ISMA] Fetching ISMACryp key for channel %d\n", id) );

	sess = gf_dm_sess_new(ctx->dm, kms_url, GF_NETIO_SESSION_NOT_THREADED, cenc_dec_kms_netio, ctx, NULL);
	if (!sess) return GF_IO_ERR;

	while (1) {
		GF_NetIOStatus status;
		e = gf_dm_sess_process(sess);
		if (e) break;
		gf_dm_sess_get_stats(sess, NULL, NULL, NULL, NULL, NULL, &status);
		if (status>=GF_NETIO_DATA_TRANSFERED) break;
	}
	if (e >= GF_EOS) {
		e = gf_ismacryp_gpac_get_info(id, (char *) gf_dm_sess_get_cache_name(sess), cstr->key, cstr->salt);
	}
	gf_dm_sess_del(sess);
	return e;
}

static Bool gf_ismacryp_mpeg4ip_get_info(char *kms_uri, char *key, char *salt)
{
	char szPath[1024], catKey[24], line[101];
	u32 i, x;
	Bool got_it;
	FILE *kms;
	strcpy(szPath, getenv("HOME"));
	strcat(szPath , "/.kms_data");
	got_it = 0;
	kms = gf_fopen(szPath, "rt");
	while (kms && !gf_feof(kms)) {
		if (!gf_fgets(szPath, 1024, kms)) break;
		szPath[strlen(szPath) - 1] = 0;
		if (stricmp(szPath, kms_uri)) continue;
		gf_fgets(line, 1, 100, kms);
		line[100] = 0;
		for (i=0; i<24; i++) {
			char szV[3];
			szV[0] = line[2*i];
			szV[1] = line[2*i + 1];
			szV[2] = 0;
			if (!sscanf(szV, "%x", &x)) break;
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
#endif


static GF_Err cenc_dec_setup_isma(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	u32 kms_crc=0;
	const GF_PropertyValue *p;
	cstr->state = DECRYPT_STATE_ERROR;

	if (scheme_type != GF_ISOM_ISMACRYP_SCHEME) return GF_NOT_SUPPORTED;
	if (scheme_version != 1) return GF_NOT_SUPPORTED;
	if (!kms_uri) return GF_NON_COMPLIANT_BITSTREAM;

	kms_crc = gf_crc_32(kms_uri, (u32) strlen(kms_uri));
	if (cstr->pssh_crc == kms_crc) return GF_OK;
	cstr->pssh_crc = kms_crc;

	/*try to fetch the keys*/
	/*base64 inband encoding*/
	if (!strnicmp(kms_uri, "(key)", 5)) {
		char data[100];
		gf_base64_decode((char*) kms_uri+5, (u32)strlen(kms_uri)-5, data, 100);
		memcpy(cstr->crypts[0].key, data, sizeof(char)*16);
		memcpy(cstr->salt, data+16, sizeof(char)*8);
	}
	/*hexadecimal inband encoding*/
	else if (!strnicmp(kms_uri, "(key-hexa)", 10)) {
		u32 v;
		char szT[3], *k;
		u32 i;
		szT[2] = 0;
		if (strlen(kms_uri) < 10+32+16) return GF_NON_COMPLIANT_BITSTREAM;

		k = (char *)kms_uri + 10;
		for (i=0; i<16; i++) {
			szT[0] = k[2*i];
			szT[1] = k[2*i + 1];
			sscanf(szT, "%X", &v);
			cstr->crypts[0].key[i] = v;
		}

		k = (char *)kms_uri + 10 + 32;
		for (i=0; i<8; i++) {
			szT[0] = k[2*i];
			szT[1] = k[2*i + 1];
			sscanf(szT, "%X", &v);
			cstr->salt[i] = v;
		}
	}
	/*MPEG4-IP KMS*/
	else if (!stricmp(kms_uri, "AudioKey") || !stricmp(kms_uri, "VideoKey")) {
#ifdef OLD_KEY_FETCHERS
		if (!gf_ismacryp_mpeg4ip_get_info((char *) kms_uri, cstr->key, cstr->salt)) {
			return GF_BAD_PARAM;
		}
#else
		return GF_NOT_SUPPORTED;
#endif
	}
	/*gpac default scheme is used, fetch file from KMS and load keys*/
	else if (scheme_uri && !stricmp(scheme_uri, "urn:gpac:isma:encryption_scheme")) {
#ifdef OLD_KEY_FETCHERS
		e = cenc_dec_get_gpac_kms(ctx, cstr, kms_uri);
		if (e) return e;
#else
		return GF_NOT_SUPPORTED;
#endif
	}
	/*hardcoded keys*/
	else {
		static u8 mysalt[] = { 8,7,6,5,4,3,2,1, 0,0,0,0,0,0,0,0 };
		static u8 mykey[][16]  = {
			{	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
			}
		};
		memcpy(cstr->salt, mysalt, sizeof(char)*8);
		memcpy(cstr->crypts[0].key, mykey, sizeof(char)*16);
	}
	cstr->state = DECRYPT_STATE_SETUP;

	p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_ISMA_SELECTIVE_ENC);
	if (p) cstr->selective_encryption = p->value.boolean;
	p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_ISMA_IV_LENGTH);
	if (p) cstr->IV_length = p->value.uint;
	p = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_ISMA_KI_LENGTH);
	if (p) cstr->KI_length = p->value.uint;


	//ctx->nb_allow_play = 1;
	return GF_OK;
}

static GF_Err cenc_dec_access_isma(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, Bool is_play)
{
	GF_Err e;

	if (is_play) {
		char IV[16];
		if (cstr->state != DECRYPT_STATE_SETUP)
			return GF_SERVICE_ERROR;
		gf_assert(!cstr->crypts[0].crypt);

		//if (!ctx->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//ctx->nb_allow_play--;

		/*init decrypter*/
		cstr->crypts[0].crypt = gf_crypt_open(GF_AES_128, GF_CTR);
		if (!cstr->crypts[0].crypt) return GF_IO_ERR;

		memset(IV, 0, sizeof(char)*16);
		memcpy(IV, cstr->salt, sizeof(char)*8);
		e = gf_crypt_init(cstr->crypts[0].crypt, cstr->crypts[0].key, IV);
		if (e) return e;

		cstr->state = DECRYPT_STATE_PLAY;
		return GF_OK;
	} else {
		if (cstr->state != DECRYPT_STATE_PLAY)
			return GF_SERVICE_ERROR;
		if (cstr->crypts[0].crypt) gf_crypt_close(cstr->crypts[0].crypt);
		cstr->crypts[0].crypt = NULL;
		cstr->state = DECRYPT_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err cenc_dec_process_isma(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, GF_FilterPacket *in_pck)
{
	u32 data_size;
	const char *in_data;
	u8 *out_data;
	u64 isma_BSO = 0;
	u32 offset=0;
	GF_FilterPacket *out_pck;
	Bool is_encrypted=GF_FALSE;
	if (!cstr->crypts[0].crypt)
		return GF_SERVICE_ERROR;

	if (! gf_filter_pck_get_crypt_flags(in_pck)) {
		out_pck = gf_filter_pck_new_ref(cstr->opid, 0, 0, in_pck);
		if (!out_pck) return GF_OUT_OF_MEM;
		gf_filter_pck_merge_properties(in_pck, out_pck);
		gf_filter_pck_set_crypt_flags(out_pck, 0);
		gf_filter_pck_send(out_pck);
		return GF_OK;
	}

	in_data = gf_filter_pck_get_data(in_pck, &data_size);

	gf_bs_reassign_buffer(ctx->bs_r, in_data, data_size);

	if (cstr->selective_encryption) {
		if (gf_bs_read_int(ctx->bs_r, 1)) is_encrypted=GF_TRUE;
		gf_bs_read_int(ctx->bs_r, 7);
		offset = 1;
	} else {
		is_encrypted=GF_TRUE;
	}
	if (is_encrypted) {
		if (cstr->IV_length != 0) {
			isma_BSO = gf_bs_read_long_int(ctx->bs_r, 8*cstr->IV_length);
			offset += cstr->IV_length;
		}
		if (cstr->KI_length) {
			offset += cstr->KI_length;
		}
	}

	/*resync IV*/
	if (!cstr->last_IV || (cstr->last_IV != isma_BSO)) {
		char IV[17];
		u64 count;
		u32 remain;
		GF_BitStream *bs;
		count = isma_BSO / 16;
		remain = (u32) (isma_BSO % 16);

		/*format IV to begin of counter*/
		bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, 0);	/*begin of counter*/
		gf_bs_write_data(bs, cstr->salt, 8);
		gf_bs_write_u64(bs, (s64) count);
		gf_bs_del(bs);
		gf_crypt_set_IV(cstr->crypts[0].crypt, IV, 17);

		/*decrypt remain bytes*/
		if (remain) {
			char dummy[20];
			gf_crypt_decrypt(cstr->crypts[0].crypt, dummy, remain);
		}
		cstr->last_IV = isma_BSO;
	}
	in_data += offset;
	data_size -= offset;

	out_pck = gf_filter_pck_new_alloc(cstr->opid, data_size, &out_data);
	if (!out_pck) return GF_OUT_OF_MEM;

	memcpy(out_data, in_data, data_size);
	/*decrypt*/
	gf_crypt_decrypt(cstr->crypts[0].crypt, out_data, data_size);
	cstr->last_IV += data_size;

	/*replace AVC start codes (0x00000001) by nalu size*/
	if (cstr->is_nalu) {
		u32 nalu_size;
		u32 remain = data_size;
		char *start, *end;
		start = out_data;
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

	gf_filter_pck_merge_properties(in_pck, out_pck);
	gf_filter_pck_set_crypt_flags(out_pck, 0);

	gf_filter_pck_send(out_pck);
	return GF_OK;
}

#ifdef OLD_KEY_FETCHERS

static GF_Err cenc_dec_setup_oma(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	const GF_PropertyValue *prop;
	GF_FilterPid *pid = cstr->ipid;

	cstr->state = DECRYPT_STATE_ERROR;
	if (scheme_type != GF_ISOM_OMADRM_SCHEME) return GF_NOT_SUPPORTED;
	if (scheme_version != 0x00000200) return GF_NOT_SUPPORTED;

	cstr->is_oma = GF_TRUE;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OMA_PREVIEW_RANGE);
	if (prop) cstr->preview_range = (u32) prop->value.longuint;

	/*TODO: call DRM agent, fetch keys*/
	if (!kms_uri) return GF_NON_COMPLIANT_BITSTREAM;
	cstr->state = DECRYPT_STATE_SETUP;

	/*we have preview*/
	if (cstr->preview_range) return GF_OK;

	return GF_NOT_SUPPORTED;
}
#endif


static GF_Err cenc_dec_load_keys(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr)
{
	bin128 blank_KID;
	const u8 *key_info;
	u32 i, j, kpos=3, nb_keys;
	cstr->crypt_init = GF_FALSE;

	//this can be NULL when set per sample and not at init
	if (!cstr->cenc_ki)
		return GF_OK;

	memset(blank_KID, 0, 16);
	key_info = cstr->cenc_ki->value.data.ptr;
	if (key_info[0]) {
		nb_keys = key_info[1];
		nb_keys<<=8;
		nb_keys |= key_info[2];
	} else {
		nb_keys = 1;
	}
	for (i=0; i<nb_keys; i++) {
		Bool found = GF_FALSE;
		u8 iv_size = key_info[kpos];
		const u8 *KID = key_info + kpos+1;
		kpos += 17;
		if (!iv_size) {
			iv_size = key_info[kpos];
			kpos += 1 + iv_size;
		}

		cstr->crypts[i].key_valid = GF_TRUE;
		if (ctx->drop_keys.nb_items) {
			for (j=0; j<ctx->drop_keys.nb_items; j++) {
				if (ctx->drop_keys.vals[j] == i+1) {
					cstr->crypts[i].key_valid = GF_FALSE;
					break;
				}
			}
		}
		if (ctx->kids.nb_items) {
			char szKID[33];
			szKID[0] = 0;
			for (j=0; j<16; j++) {
				char szC[3];
				sprintf(szC, "%02X", KID[j]);
				strcat(szKID, szC);
			}
			for (j=0; j<ctx->kids.nb_items; j++) {
				char *kid_d = ctx->kids.vals[j];
				if (!strncmp(kid_d, "0x", 2)) kid_d+=2;

				if (stricmp(szKID, kid_d)) continue;

				//no global key, disable key
				if (!ctx->keys.nb_items) {
					cstr->crypts[i].key_valid = GF_FALSE;
				}
				//use global keys
				else {
					u32 len;
					bin128 key;
					char *key_str = ctx->keys.vals[j];
					if (!strncmp(key_str, "0x", 2)) key_str+= 2;
					len = (u32) strlen(key_str);
					if (len!=32) {
						cstr->crypts[i].key_valid = GF_FALSE;
						break;
					}
					for (j=0; j<16; j++) {
						u32 val;
						char szV[3];
						szV[0] = key_str[2*j];
						szV[1] = key_str[2*j+1];
						szV[2] = 0;
						sscanf(szV, "%02X", &val);
						key[j] = val;
					}
					memcpy(cstr->crypts[i].key, key, 16);
					found = GF_TRUE;
				}
				break;
			}
		}
		if (!cstr->crypts[i].key_valid) continue;
		if (found) continue;

		for (j=0; j<cstr->KID_count; j++) {
			Bool match = GF_FALSE;
			if (cstr->is_hls) {
				match = GF_TRUE;
			} else if (!memcmp(KID, cstr->KIDs[j], 16) || !memcmp(blank_KID, cstr->KIDs[j], 16) ) {
				match = GF_TRUE;
			}
			if (match) {
				memcpy(cstr->crypts[i].key, cstr->keys[j], 16);
				found = GF_TRUE;
				if (ctx->decrypt>=DECRYPT_SKIP) {
					cstr->crypts[i].key_valid = GF_FALSE;
				} else {
					cstr->crypts[i].key_valid = GF_TRUE;
				}
				break;
			}
		}
		if (!found) {
			char szKID[33];
			//key not yet delivered
			if (cstr->gpac_master_leaf) {
				cstr->crypts[i].key_valid = GF_FALSE;
				continue;
			}

			szKID[0] = 0;
			for (j=0; j<16; j++) {
				char szV[3];
				sprintf(szV, "%02X", KID[j]);
				strcat(szKID, szV);
			}
			if (ctx->decrypt==DECRYPT_FULL) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot locate key #%d for given KID 0x%s, aborting !\n\tUse '--decrypt=nokey' to force decrypting\n", i+1, szKID));
				return cstr->key_error = GF_SERVICE_ERROR;
			}
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Cannot locate key #%d for given KID 0x%s, will leave data encrypted\n", i+1, szKID));
			cstr->crypts[i].key_valid = GF_FALSE;
		}
	}

	cstr->key_error = GF_OK;
	cstr->crypt_init = GF_FALSE;
	return GF_OK;
}

static GF_Err rfmt_dec_b64(u8 *data, u8 *output, u32 osize)
{
	u32 len, i=0;
	while (data[i]) {
		if (data[i] == '-') data[i] = '+';
		else if (data[i] == '_') data[i] = '/';
		i++;
	}
	len = (u32) strlen(data);
	switch (len%4) {
	case 0: break;
	case 2: strcat(data, "=="); break;
	case 3: strcat(data, "="); break;
	default: return GF_NON_COMPLIANT_BITSTREAM;
	}
	len = gf_base64_decode(data, (u32) strlen(data), output, osize);
	if (len != 16) return GF_NON_COMPLIANT_BITSTREAM;
	return GF_OK;
}

#ifdef GPAC_USE_DOWNLOADER
static void ck_http_io(void *usr_cbk, GF_NETIO_Parameter *par)
{
	GF_CENCDecStream *cstr = usr_cbk;
	switch (par->msg_type) {
	case GF_NETIO_GET_METHOD:
		par->name = "POST";
		break;
	case GF_NETIO_GET_HEADER:
		if (!cstr->hdr_done) {
			cstr->hdr_done = GF_TRUE;
			par->name = "Content-Type";
			par->value = "application/json";
		}
		break;
	case GF_NETIO_GET_CONTENT:
		par->data = cstr->body;
		par->size = (u32) strlen(cstr->body);
		cstr->res_size = 0;
		break;
	case GF_NETIO_DATA_EXCHANGE:
		if (!cstr->res_size) {
			if (cstr->body) gf_free(cstr->body);
			cstr->body = NULL;
		}
		cstr->body = gf_realloc(cstr->body, (cstr->res_size + par->size + 1));
		if (!cstr->body) {
			cstr->state = DECRYPT_STATE_ERROR;
			break;
		}
		memcpy(cstr->body + cstr->res_size, par->data, par->size);
		cstr->res_size+=par->size;
		cstr->body[cstr->res_size] = 0;
		break;
	case GF_NETIO_DATA_TRANSFERED:
	{
		//extract kids and keys from json
		GF_Err e = GF_OK;
		u32 key_idx=0;
		char *ptr = cstr->body;
		while (1) {
			char *k = strstr(ptr, "\"k\"");
			char *kid = strstr(ptr, "\"kid\"");
			if (!k || !kid) break;
			k+=3;
			kid+=5;
			while (k[0] && (k[0] != '"')) k++;
			while (kid[0] && (kid[0] != '"')) kid++;
			if ((k[0] != '"') || (kid[0] != '"')) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}

			k++;
			kid++;
			char *s1 = strchr(k, '"');
			char *s2 = strchr(kid, '"');
			if (!s1 || !s2) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
			s1[0] = 0;
			s2[0] = 0;

			u8 key_val[20], kid_val[20];
			e = rfmt_dec_b64(k, key_val, 20);
			if (e) break;
			e = rfmt_dec_b64(kid, kid_val, 20);
			if (e) break;

			cstr->keys = gf_realloc(cstr->keys, sizeof(bin128)*(key_idx+1));
			cstr->KIDs = gf_realloc(cstr->KIDs, sizeof(bin128)*(key_idx+1));
			memcpy(cstr->keys[key_idx], key_val, sizeof(bin128));
			memcpy(cstr->KIDs[key_idx], kid_val, sizeof(bin128));
			key_idx++;
			if (s1 < s2) ptr = s2+1;
			else ptr = s1+1;
		}
		if (!e) {
			cstr->KID_count = key_idx;

			if (!cstr->crypts) {
				cstr->crypts = gf_malloc(sizeof(CENCDecKey));
				memset(cstr->crypts, 0, sizeof(CENCDecKey));
			}
			memcpy(cstr->crypts[0].key, cstr->keys[0], sizeof(bin128));
			cstr->crypts[0].key_valid = 1;
		} else {
			cstr->state = DECRYPT_STATE_ERROR;
		}
		if (cstr->body) gf_free(cstr->body);
		cstr->body = NULL;
		gf_dm_sess_del(cstr->sess);
		cstr->sess = NULL;
		cstr->ctx->pending_keys--;
	}
		break;
	case GF_NETIO_STATE_ERROR:
		if (cstr->sess) {
			gf_dm_sess_del(cstr->sess);
			cstr->sess = NULL;
			cstr->ctx->pending_keys--;
			if (cstr->body) gf_free(cstr->body);
			cstr->body = NULL;
		}
		break;
	default:
		break;
	}
}
#endif


static GF_Err cenc_dec_set_clearkey(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, char *ck_url, u8 *ck_kid)
{
	GF_Err e;
	char data64[32];
	u32 i, cklen, res = gf_base64_encode(ck_kid, 16, data64, 32);
	data64[res]=0;
	for (i=0; i<res; i++) {
		if (data64[i]=='+') data64[i] = '-';
		else if (data64[i]=='/') data64[i] = '_';
	}
	while (data64[res-1]=='=') {
		data64[res-1]=0;
		res--;
		if (!res) break;
	}
	if (cstr->body) gf_free(cstr->body);
	cstr->body = NULL;
	gf_dynstrcat(&cstr->body, "{\"kids\": [\"", NULL);
	gf_dynstrcat(&cstr->body, data64, NULL);
	gf_dynstrcat(&cstr->body, "\"], \"type\":\"temporary\"}", NULL);

	cklen = (u32) strlen(cstr->body);
	u32 crc = gf_crc_32(cstr->body, cklen);
	if (cstr->clearkey_crc == crc) return GF_OK;

#ifdef GPAC_USE_DOWNLOADER
	GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
	if (!dm) return GF_SERVICE_ERROR;
	cstr->sess = gf_dm_sess_new(dm, ck_url, 0, ck_http_io, cstr, &e);
	if (e) return e;
	ctx->pending_keys++;
	return gf_dm_sess_process(cstr->sess);
#else
	return GF_NOT_SUPPORTED;
#endif
}

#ifdef GPAC_USE_DOWNLOADER
static void hls_kms_io(void *usr_cbk, GF_NETIO_Parameter *par)
{
	GF_CENCDecStream *cstr = (GF_CENCDecStream *)usr_cbk;
	if (par->msg_type==GF_NETIO_DATA_EXCHANGE) {
		if (cstr->state == DECRYPT_STATE_ERROR)
			return;

		if (!cstr->res_size) {
			if (cstr->body) gf_free(cstr->body);
			cstr->body = NULL;
		}
		if (cstr->res_size > 32) {
			cstr->state = DECRYPT_STATE_ERROR;
			return;
		}
		cstr->body = gf_realloc(cstr->body, (cstr->res_size + par->size + 1));
		if (!cstr->body) {
			cstr->state = DECRYPT_STATE_ERROR;
			return;
		}
		memcpy(cstr->body + cstr->res_size, par->data, par->size);
		cstr->res_size+=par->size;
		cstr->body[cstr->res_size] = 0;
	} else if (par->msg_type==GF_NETIO_DATA_TRANSFERED) {
		if (cstr->body && (cstr->res_size <= 32)) {
			//first 16 bytes is the key, in some case we have IV repeated after the key (to do ?)
			memcpy(cstr->keys[0], cstr->body, sizeof(bin128));

			//load keys
			if (!cstr->crypts) {
				cstr->crypts = gf_malloc(sizeof(CENCDecKey));
				memset(cstr->crypts, 0, sizeof(CENCDecKey));
			}
			memcpy(cstr->crypts[0].key, cstr->keys[0], sizeof(bin128));
			cstr->crypts[0].key_valid = 1;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CENC/HLS] Invalid key size, greater than 16 bytes\n"))
			cstr->state = DECRYPT_STATE_ERROR;
		}
		if (cstr->body) gf_free(cstr->body);
		cstr->body = NULL;
		gf_dm_sess_del(cstr->sess);
		cstr->sess = NULL;
		cstr->ctx->pending_keys--;
	} else if (par->msg_type==GF_NETIO_STATE_ERROR) {
		if (cstr->sess) {
			gf_dm_sess_del(cstr->sess);
			cstr->sess = NULL;
			cstr->ctx->pending_keys--;
			if (cstr->body) gf_free(cstr->body);
			cstr->body = NULL;
		}
	}
}
#endif

static GF_Err cenc_dec_set_hls_key(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, char *key_url, u8 *key_IV)
{
	cstr->is_hls = GF_TRUE;

	//copy IV
	memcpy(cstr->hls_IV, key_IV, sizeof(bin128));
	//switch key if needed IV
	if (cstr->hls_key_url && key_url && !strcmp(cstr->hls_key_url, key_url)) {
		if (cstr->crypt_init)
			gf_crypt_set_IV(cstr->crypts[0].crypt, cstr->hls_IV, 16);
		return GF_OK;
	}

	if (cstr->hls_key_url) gf_free(cstr->hls_key_url);

	if (!key_url) {
		cstr->hls_key_url = NULL;
		return GF_OK;
	}
	cstr->hls_key_url = gf_strdup(key_url);
	if (!cstr->hls_key_url) {
		return GF_OUT_OF_MEM;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[CENC/HLS] Switching key to %s\n", key_url))

	if (ctx->hls_cenc_patch_iv)
		cstr->force_hls_iv = GF_TRUE;

	cstr->KID_count = 1;
	cstr->keys = (bin128 *)gf_realloc(cstr->keys, cstr->KID_count*sizeof(bin128));

	if (!strncmp(key_url, "urn:gpac:keys:value:", 20)) {
		u32 i;
		u8 *key_data = (u8 *) cstr->keys[0];
		key_url += 20;
		if (!strncmp(key_url, "0x", 2)) key_url += 2;
		i = (u32) strlen(key_url);
		if (i != 32) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CENC/HLS] key %s not found\n", key_url))
			return GF_BAD_PARAM;
		}
		for (i=0; i<16; i++) {
			char szV[3];
			u32 v;
			szV[0] = key_url[2*i];
			szV[1] = key_url[2*i + 1];
			szV[2] = 0;
			sscanf(szV, "%X", &v);
			key_data[i] = v;
		}
	}
	//key is local, activate right away
	else if (gf_url_is_local(key_url)) {
		FILE *fkey = gf_fopen(key_url, "rb");
		if (!fkey) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CENC/HLS] key %s not found\n", key_url))
			return GF_URL_ERROR;
		} else {
			u32 read = (u32) gf_fread(cstr->keys[0], 16, fkey);
			if (read != 16) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CENC/HLS] key %s too short, expecting 16 bytes got %d\n", key_url, read))
				return GF_BAD_PARAM;
			}
			gf_fclose(fkey);
		}
	}
	//load key
	else {
		GF_Err e = GF_SERVICE_ERROR;
#ifdef GPAC_USE_DOWNLOADER
		GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
		if (!dm) return GF_SERVICE_ERROR;
		GF_DownloadSession *sess = gf_dm_sess_new(dm, key_url, GF_NETIO_SESSION_NOT_CACHED, hls_kms_io, cstr, &e);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CENC/HLS] Failed to setup download session for key %s: %s\n", key_url, gf_error_to_string(e)))
			return e;
		}
		ctx->pending_keys++;
		return gf_dm_sess_process(sess);
#else
		e = GF_NOT_SUPPORTED;
#endif
	}

	if (!cstr->crypts) {
		cstr->crypts = gf_malloc(sizeof(CENCDecKey));
		memset(cstr->crypts, 0, sizeof(CENCDecKey));
	}
	memcpy(cstr->crypts[0].key, cstr->keys[0], sizeof(bin128));
	cstr->crypts[0].key_valid = 1;

	return GF_OK;
}

static GF_Err cenc_dec_push_iv(GF_CENCDecStream *cstr, u32 key_idx, u8 *IV, u32 iv_size, u32 const_iv_size, const u8 *const_iv);

static GF_Err cenc_dec_load_pssh(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, const GF_PropertyValue *pssh_prop, Bool is_pck_pssh, const GF_PropertyValue *cinfo_prop)
{
	u32 i, nb_pssh;

	if (pssh_prop) {
		gf_bs_reassign_buffer(ctx->bs_r, pssh_prop->value.data.ptr, pssh_prop->value.data.size);
		nb_pssh = gf_bs_read_u32(ctx->bs_r);
	} else {
		nb_pssh = 0;
	}

	for (i = 0; i < nb_pssh; i++) {
		u32 cypherOffset;
		char *enc_data;
		bin128 cypherKey, cypherIV;
		GF_Crypt *mc;
		u32 pos, priv_len;
		char szSystemID[33];
		bin128 sysID;
		Bool is_leaf_key=GF_FALSE;
		u32 j, kid_count=0, enc_payload_len;
		u8 *pssh_data = (u8 *) pssh_prop->value.data.ptr	;

		gf_bs_read_data(ctx->bs_r, sysID, 16);
		u32 version = gf_bs_read_u32(ctx->bs_r);
		if (version)
			kid_count = gf_bs_read_u32(ctx->bs_r);

		memset(szSystemID, 0, 33);
		for (j=0; j<16; j++) {
			sprintf(szSystemID+j*2, "%02X", (unsigned char) sysID[j]);
		}

		/*SystemID for GPAC Player: 67706163-6365-6E63-6472-6D746F6F6C31*/
		if (!strcmp(szSystemID, "6770616363656E6364726D746F6F6C31")) {
			//no KID signaled (version 0), use inband setup
			if (version==0) {
				cstr->gpac_master_leaf = GF_FALSE;
				cstr->inband_keys = GF_TRUE;
				cstr->KID_count = 0;
				return GF_OK;
			}
			else if (version>1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Unsupported GPAC DRM config (version %d)\n", version));
				continue;
			}
		}
		else {
			if (!cinfo_prop && !ctx->cfile) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[CENC/ISMA] System ID %s not supported\n", szSystemID));
			}
			if (version)
				gf_bs_skip_bytes(ctx->bs_r, kid_count*16);
			j=gf_bs_read_u32(ctx->bs_r);
			gf_bs_skip_bytes(ctx->bs_r, j);
			continue;
		}

		if (kid_count*16 > gf_bs_available(ctx->bs_r)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid KID count %d in GPAC init blob\n", kid_count));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_bin128_parse("0x6770616363656E6364726D746F6F6C31", cypherKey);

		/*store key IDs*/
		cstr->KID_count = kid_count;
		pos = (u32) gf_bs_get_position(ctx->bs_r);
		cstr->KIDs = (bin128 *)gf_realloc(cstr->KIDs, cstr->KID_count*sizeof(bin128));
		cstr->keys = (bin128 *)gf_realloc(cstr->keys, cstr->KID_count*sizeof(bin128));

		memmove(cstr->KIDs, pssh_data + pos, cstr->KID_count*sizeof(bin128));
		gf_bs_skip_bytes(ctx->bs_r, cstr->KID_count*sizeof(bin128));

		//no key announced yet (roll)
		if ((cstr->KID_count==1) && !memcmp(cstr->KIDs[0], cypherKey, sizeof(bin128))) {
			cstr->inband_keys = GF_TRUE;
			cstr->gpac_master_leaf = GF_FALSE;
			cstr->KID_count = 0;
			return GF_OK;
		}

		priv_len = gf_bs_read_u32(ctx->bs_r);
		pos = (u32) gf_bs_get_position(ctx->bs_r);

		if (gf_bs_available(ctx->bs_r) < priv_len) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid private len %d in GPAC init blob\n", priv_len));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (gf_bs_available(ctx->bs_r) % 16 == 0) {
			cypherOffset = 0;
			if (cstr->gpac_master_leaf)
				is_leaf_key = GF_TRUE;
		} else {
			cypherOffset = pssh_data[pos] + 1;
			if (priv_len < cypherOffset) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid cypher offset %d in GPAC init blob\n", cypherOffset));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			if (!strncmp(pssh_data + pos + 1, "master", 6)) {
				cstr->gpac_master_leaf = GF_TRUE;
			}
			else if (!strncmp(pssh_data + pos + 1, "leaf", 4)) {
				is_leaf_key = GF_TRUE;
			} else if (cypherOffset>1) {
				cstr->gpac_master_leaf = GF_FALSE;
				if (!is_pck_pssh) {
					cstr->gpac_master_leaf = GF_FALSE;
					cstr->inband_keys = (version==0) ? GF_TRUE : GF_FALSE;
				}
			} else if (cstr->gpac_master_leaf) {
				is_leaf_key = GF_TRUE;
			}
		}


		if (is_leaf_key) {
			if (gf_bs_available(ctx->bs_r) < 16 + cypherOffset	) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid GPAC init blob for leaf key\n", kid_count));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			memmove(cstr->keys, pssh_data + pos + cypherOffset, cstr->KID_count*sizeof(bin128));
		} else if (priv_len) {
			Bool use_cbc = (!cstr->gpac_master_leaf && cstr->inband_keys) ? GF_TRUE : GF_FALSE;
			/*GPAC DRM TEST system info, used to validate cypher offset in CENC packager
				keyIDs as usual (before private data)
				URL len on 8 bits
				URL
				keys, cyphered with our magic key :)
			*/

			gf_bin128_parse("0x00000000000000000000000000000001", cypherIV);

			enc_payload_len = priv_len - cypherOffset;
			if ((priv_len < cypherOffset) || (enc_payload_len < (u32) (cstr->KID_count*sizeof(bin128)))) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid GPAC init blob\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			if (use_cbc) {
				mc = gf_crypt_open(GF_AES_128, GF_CBC);
			} else {
				mc = gf_crypt_open(GF_AES_128, GF_CTR);
			}
			if (!mc) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Cannot open AES-128 CTR\n"));
				return GF_IO_ERR;
			}
			enc_data = gf_malloc(priv_len - cypherOffset);
			memcpy(enc_data, pssh_data + pos + cypherOffset, priv_len - cypherOffset);

			gf_crypt_init(mc, cypherKey, use_cbc ? NULL : cypherIV);
			gf_crypt_decrypt(mc, enc_data, priv_len - cypherOffset);
			gf_crypt_close(mc);

			memmove(cstr->keys, enc_data, cstr->KID_count*sizeof(bin128));
			gf_bs_skip_bytes(ctx->bs_r, cstr->KID_count*sizeof(bin128));
			gf_free(enc_data);
		}

		if (cstr->gpac_master_leaf && !is_leaf_key) {
			if (!cstr->KID_count) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] No master key found for gpac test DRM\n"));
				return GF_NON_COMPLIANT_BITSTREAM;

			}
			memcpy(cstr->master_key, cstr->keys[0], sizeof(bin128));
			cstr->KID_count = 0;
			return GF_OK;
		}
		if (is_leaf_key) {
			for (j=0; j<cstr->KID_count; j++) {
				GF_Crypt *crypto = gf_crypt_open(GF_AES_128, GF_CBC);
				if (!crypto) return GF_OUT_OF_MEM;
				gf_crypt_init(crypto, cstr->master_key, NULL);
				gf_crypt_decrypt(crypto, cstr->keys[j], 16);
				gf_crypt_close(crypto);
			}
		}
		return cenc_dec_load_keys(ctx, cstr);
	}

	return GF_NOT_FOUND;
}

static GF_Err cenc_dec_setup_cenc(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	GF_Err e;
	GF_CryptInfo *cinfo=NULL;
	u32 i, pssh_crc=0, ki_crc=0;
	const GF_PropertyValue *prop, *cinfo_prop, *pssh_prop;
	GF_FilterPid *pid = cstr->ipid;
	Bool is_valid=GF_TRUE;
	Bool is_playing = (cstr->state == DECRYPT_STATE_PLAY) ? GF_TRUE : GF_FALSE;

	cstr->state = DECRYPT_STATE_ERROR;

	if ((scheme_type != GF_ISOM_CENC_SCHEME)
		&& (scheme_type != GF_ISOM_CBC_SCHEME)
		&& (scheme_type != GF_ISOM_CENS_SCHEME)
		&& (scheme_type != GF_ISOM_CBCS_SCHEME)
		&& (scheme_type != GF_ISOM_PIFF_SCHEME)
		&& (scheme_type != GF_HLS_SAMPLE_AES_SCHEME)
	)
		return GF_NOT_SUPPORTED;

	if (scheme_version != 0x00010000) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] Invalid scheme version %08X for scheme type %s, results might be wrong\n", scheme_version, gf_4cc_to_str(scheme_type) ));
	}

	pssh_prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_PSSH);
	if (pssh_prop) {
		pssh_crc = gf_crc_32(pssh_prop->value.data.ptr, pssh_prop->value.data.size);
	}

	cstr->cenc_pattern = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_PATTERN);
	if (cstr->cenc_pattern && (!cstr->cenc_pattern->value.frac.num || !cstr->cenc_pattern->value.frac.den))
		cstr->cenc_pattern = NULL;

	ki_crc = 0;
	if (cstr->cenc_ki) {
		ki_crc = gf_crc_32(cstr->cenc_ki->value.data.ptr, cstr->cenc_ki->value.data.size);
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_KEY_INFO);
	if (prop) {
		cstr->cenc_ki = prop;
		cstr->multikey = 0;
		if (prop->value.data.ptr[0]) {
			cstr->multikey = prop->value.data.ptr[1];
			cstr->multikey <<= 8;
			cstr->multikey |= prop->value.data.ptr[2];
		}
		u32 new_crc = gf_crc_32(cstr->cenc_ki->value.data.ptr, cstr->cenc_ki->value.data.size);

		//change of key config and same pssh, setup single key
		if ((new_crc != ki_crc) && (pssh_crc==cstr->pssh_crc) && !cstr->inband_keys) {
			e = cenc_dec_load_keys(ctx, cstr);
			if (e) return e;
		}
		//if no pssh or pssh change we setup the key later
		//and if same key/pssh nothing to do for crypto reinit
	} else {
		cstr->cenc_ki = NULL;
	}

	if (cstr->rep_crc) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_REP_ID);
		if (!prop) is_valid = GF_FALSE;
		else {
			u32 crc = gf_crc_32(prop->value.string, (u32) strlen(prop->value.string));
			if (crc!=cstr->rep_crc) is_valid = GF_FALSE;
		}
	}
	if (cstr->per_crc) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PERIOD_ID);
		if (!prop) is_valid = GF_FALSE;
		else {
			u32 crc = gf_crc_32(prop->value.string, (u32) strlen(prop->value.string));
			if (crc!=cstr->per_crc) is_valid = GF_FALSE;
		}
	}
	if (cstr->as_id) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PERIOD_ID);
		if (!prop) is_valid = GF_FALSE;
		else if (prop->value.uint!=cstr->as_id) is_valid = GF_FALSE;
	}

	cstr->state = is_playing ? DECRYPT_STATE_PLAY : DECRYPT_STATE_SETUP;

	if ((cstr->scheme_type==scheme_type) && (cstr->scheme_version==scheme_version) && (cstr->pssh_crc==pssh_crc) && is_valid)
		return GF_OK;

	cstr->scheme_version = scheme_version;
	cstr->scheme_type = scheme_type;
	cstr->pssh_crc = pssh_crc;

	if ((scheme_type == GF_ISOM_CENC_SCHEME) || (scheme_type == GF_ISOM_PIFF_SCHEME) || (scheme_type == GF_ISOM_CENS_SCHEME))
		cstr->is_cenc = GF_TRUE;
	else
		cstr->is_cbc = GF_TRUE;

	cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECRYPT_INFO);
	if (!cinfo_prop) cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CRYPT_INFO);

	if (pssh_prop) {
		e = cenc_dec_load_pssh(ctx, cstr, pssh_prop, GF_FALSE, cinfo_prop);
		if (e==GF_NOT_FOUND) {
			e = GF_OK;
		} else {
			return e;
		}
	}

	if (cinfo_prop) {
		cinfo = gf_crypt_info_load(cinfo_prop->value.string, &e);
		if (!cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Failed to open crypt info file %s\n", cinfo_prop->value.string));
			return e;
		}
	}
	if (!cinfo) cinfo = ctx->cinfo;

	if (cinfo) {
		GF_TrackCryptInfo *any_tci = NULL;
		GF_TrackCryptInfo *tci = NULL;
		u32 count = gf_list_count(cinfo->tcis);
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		for (i=0; i<count; i++) {
			tci = gf_list_get(cinfo->tcis, i);
			if (tci->trackID && prop && (tci->trackID==prop->value.uint)) {
				break;
			}
			if (!any_tci && !tci->trackID) any_tci = tci;
			if ((cinfo != ctx->cinfo) && !any_tci) any_tci = tci;
			tci = NULL;
		}
		if (!tci && any_tci) tci = any_tci;

		if (tci) {
			cstr->rep_crc = 0;
			cstr->per_crc = 0;
			cstr->as_id = 0;
			cstr->KID_count = tci->nb_keys;
			cstr->KIDs = (bin128 *)gf_realloc(cstr->KIDs, tci->nb_keys*sizeof(bin128));
			cstr->keys = (bin128 *)gf_realloc(cstr->keys, tci->nb_keys*sizeof(bin128));
			for (i=0; i<tci->nb_keys; i++) {
				memcpy(cstr->KIDs[i], tci->keys[i].KID, sizeof(bin128));
				memcpy(cstr->keys[i], tci->keys[i].key, sizeof(bin128));
				memcpy(cstr->hls_IV, tci->keys[i].IV, sizeof(bin128));

				//reset KID if not for our period/as/rep
				is_valid = GF_TRUE;
				if (tci->keys[i].repID) {
					prop = gf_filter_pid_get_property(pid, GF_PROP_PID_REP_ID);
					if (prop && prop->value.string && !strcmp(prop->value.string, tci->keys[i].repID)) {
						cstr->rep_crc = gf_crc_32(prop->value.string, (u32) strlen(prop->value.string));
					} else {
						is_valid = GF_FALSE;
					}
				}
				if (tci->keys[i].periodID) {
					prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PERIOD_ID);
					if (prop && prop->value.string && !strcmp(prop->value.string, tci->keys[i].periodID)) {
						cstr->per_crc = gf_crc_32(prop->value.string, (u32) strlen(prop->value.string));
					} else {
						is_valid = GF_FALSE;
					}
				}
				if (tci->keys[i].ASID) {
					prop = gf_filter_pid_get_property(pid, GF_PROP_PID_AS_ID);
					if (prop && (prop->value.uint == tci->keys[i].ASID)) {
						cstr->as_id = tci->keys[i].ASID;
					} else {
						is_valid = GF_FALSE;
					}
				}
				if (!is_valid) {
					memset(cstr->KIDs[i], 0xFF, sizeof(bin128));
					memset(cstr->keys[i], 0xFF, sizeof(bin128));
				}
			}
			if (cinfo != ctx->cinfo)
				gf_crypt_info_del(cinfo);

			return cenc_dec_load_keys(ctx, cstr);
		}
		if (cinfo != ctx->cinfo)
			gf_crypt_info_del(cinfo);
	}

	if (ctx->keys.nb_items) {
		return GF_OK;
	}

	cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_HLS_KMS);
	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_HLS_IV);

	if (cinfo_prop && prop) {
		e = cenc_dec_set_hls_key(ctx, cstr, cinfo_prop->value.string, prop->value.data.ptr);
		if (!e) return GF_OK;
	}

	cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CLEARKEY_URI);
	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CLEARKEY_KID);

	if (cinfo_prop && prop) {
		e = cenc_dec_set_clearkey(ctx, cstr, cinfo_prop->value.string, prop->value.data.ptr);
		if (!e) return GF_OK;
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[CENC] Failed to setup clearkey from download key %s: %s\n", cinfo_prop->value.string, gf_error_to_string(e)))
	}


	if (ctx->decrypt!=DECRYPT_FULL) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] No keys found but playback forced\n"));
		return GF_OK;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] No key found, aborting!\n\tUse '--decrypt=nokey' to force decrypting\n"));
	return GF_FILTER_NOT_SUPPORTED;
}

static GF_Err cenc_dec_hls_saes(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	GF_Err e;
	const GF_PropertyValue *prop, *cinfo_prop;
	GF_FilterPid *pid = cstr->ipid;
	Bool is_playing = (cstr->state == DECRYPT_STATE_PLAY) ? GF_TRUE : GF_FALSE;

	cstr->state = DECRYPT_STATE_ERROR;

	cstr->cenc_pattern = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_PATTERN);
	if (cstr->cenc_pattern && (!cstr->cenc_pattern->value.frac.num || !cstr->cenc_pattern->value.frac.den))
		cstr->cenc_pattern = NULL;

	cstr->state = is_playing ? DECRYPT_STATE_PLAY : DECRYPT_STATE_SETUP;
	cstr->is_cbc = GF_TRUE;
	cstr->is_hls_saes = GF_TRUE;
	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NON_COMPLIANT_BITSTREAM;
	cstr->codec_id = prop->value.uint;

	cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_HLS_KMS);
	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_HLS_IV);

	if (cinfo_prop && prop) {
		e = cenc_dec_set_hls_key(ctx, cstr, cinfo_prop->value.string, prop->value.data.ptr);
		if (!e) return GF_OK;
	}
	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_ISOM_SUBTYPE);
	if (!prop) prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECRYPT_INFO);
	if (prop || ctx->cinfo) {
		e = cenc_dec_setup_cenc(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		if (e) return e;
		if (!cstr->cenc_ki && cstr->nb_crypts && cstr->crypts[0].crypt && !cstr->crypts[0].key_valid) {
			bin128 IV;
			memcpy(cstr->crypts[0].key, cstr->keys[0], 16);
			cenc_dec_push_iv(cstr, 0, IV, 0, 16, cstr->hls_IV);
		}
		return GF_OK;
	}

	if (ctx->decrypt!=DECRYPT_FULL) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] No keys found but playback forced\n"));
		return GF_OK;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] No key found, aborting!\n\tUse '--decrypt=nokey' to force decrypting\n"));
	return GF_FILTER_NOT_SUPPORTED;
}


static GF_Err cenc_dec_setup_adobe(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	GF_CryptInfo *cinfo=NULL;
	const GF_PropertyValue *prop;
	GF_FilterPid *pid = cstr->ipid;
	Bool is_playing = (cstr->state == DECRYPT_STATE_PLAY) ? GF_TRUE : GF_FALSE;

	cstr->state = DECRYPT_STATE_ERROR;

	if (scheme_type != GF_ISOM_ADOBE_SCHEME) return GF_NOT_SUPPORTED;
	if (scheme_version != 1) return GF_NOT_SUPPORTED;


	if (cstr->KID_count) return GF_OK;

	cstr->is_adobe = GF_TRUE;

	cstr->state = is_playing ? DECRYPT_STATE_PLAY : DECRYPT_STATE_SETUP;

	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECRYPT_INFO);
	if (!prop) prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CRYPT_INFO);

	if (prop) {
		GF_Err e;
		cinfo = gf_crypt_info_load(prop->value.string, &e);
		if (!cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Failed to open crypt info file %s\n", prop->value.string));
			return e;
		}
	}
	if (!cinfo) cinfo = ctx->cinfo;

	if (cinfo) {
		GF_TrackCryptInfo *any_tci = NULL;
		GF_TrackCryptInfo *tci = NULL;
		u32 i, count = gf_list_count(cinfo->tcis);
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		for (i=0; i<count; i++) {
			tci = gf_list_get(cinfo->tcis, i);
			if (tci->trackID && prop && (tci->trackID==prop->value.uint)) {
				break;
			}
			if (!any_tci && !tci->trackID) any_tci = tci;
			if ((cinfo != ctx->cinfo) && !any_tci) any_tci = tci;
			tci = NULL;
		}
		if (!tci && any_tci) tci = any_tci;

		if (tci) {
			cstr->KID_count = tci->nb_keys;
			cstr->KIDs = (bin128 *)gf_realloc(cstr->KIDs, tci->nb_keys*sizeof(bin128));
			cstr->keys = (bin128 *)gf_realloc(cstr->keys, tci->nb_keys*sizeof(bin128));
			for (i=0; i<tci->nb_keys; i++) {
				memcpy(cstr->KIDs[i], tci->keys[i].KID, sizeof(bin128));
				memcpy(cstr->keys[i], tci->keys[i].key, sizeof(bin128));
			}
			if (cinfo != ctx->cinfo)
				gf_crypt_info_del(cinfo);
			return GF_OK;
		}
		if (cinfo != ctx->cinfo)
			gf_crypt_info_del(cinfo);
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] No supported system ID, no key found\n"));
	return GF_NOT_SUPPORTED;
}

static GF_Err cenc_dec_access_cenc(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, Bool is_play)
{
	u32 i;
	if (is_play) {
		if (cstr->state != DECRYPT_STATE_SETUP)
			return GF_SERVICE_ERROR;

		//if (!ctx->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//ctx->nb_allow_play--;

		/*open decrypter - we do NOT initialize decrypter; it wil be done when we decrypt the first crypted sample*/
		for (i=0; i<cstr->nb_crypts; i++) {
			gf_assert(!cstr->crypts[i].crypt);
			if (cstr->is_cenc)
				cstr->crypts[i].crypt = gf_crypt_open(GF_AES_128, GF_CTR);
			else
				cstr->crypts[i].crypt = gf_crypt_open(GF_AES_128, GF_CBC);
			if (!cstr->crypts[i].crypt) return GF_IO_ERR;
		}
		cstr->state = DECRYPT_STATE_PLAY;
		return GF_OK;
	} else {
		if (cstr->state != DECRYPT_STATE_PLAY)
			return GF_SERVICE_ERROR;
		for (i=0; i<cstr->nb_crypts; i++) {
			if (cstr->crypts[i].crypt) gf_crypt_close(cstr->crypts[i].crypt);
			cstr->crypts[i].crypt = NULL;
		}
		cstr->state = DECRYPT_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err cenc_dec_access_adobe(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, Bool is_play)
{
	if (is_play) {
		if (cstr->state != DECRYPT_STATE_SETUP)
			return GF_SERVICE_ERROR;
		gf_assert(!cstr->crypts[0].crypt);

		cstr->crypts[0].crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!cstr->crypts[0].crypt) return GF_IO_ERR;

		cstr->state = DECRYPT_STATE_PLAY;
		return GF_OK;
	} else {
		if (cstr->state != DECRYPT_STATE_PLAY)
			return GF_SERVICE_ERROR;
		if (cstr->crypts[0].crypt) gf_crypt_close(cstr->crypts[0].crypt);
		cstr->crypts[0].crypt = NULL;
		cstr->state = DECRYPT_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err cenc_dec_push_iv(GF_CENCDecStream *cstr, u32 key_idx, u8 *IV, u32 iv_size, u32 const_iv_size, const u8 *const_iv)
{
	GF_Err e;

	if (!cstr->crypt_init) {
		if (const_iv) {
			memcpy(IV, const_iv, const_iv_size);
		}
		e = gf_crypt_init(cstr->crypts[key_idx].crypt, cstr->crypts[key_idx].key, IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot initialize AES-128 AES-128 %s (%s)\n", cstr->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			return GF_IO_ERR;
		}
	} else {
		//always restore IV at beginning of sample regardless of the mode (const IV or IV CBC or CTR)
		if (!cstr->is_cbc) {
			if (!iv_size)
				iv_size = const_iv_size;

			memmove(&IV[1], &IV[0], sizeof(char) * iv_size);
			IV[0] = 0;	/*begin of counter*/
			e = gf_crypt_set_IV(cstr->crypts[key_idx].crypt, IV, 17);
		} else {
			if (const_iv) {
				if (cstr->force_hls_iv) {
					memcpy(IV, cstr->hls_IV, sizeof(bin128));
				} else {
					memcpy(IV, const_iv, const_iv_size);
				}
			}
			e = gf_crypt_set_IV(cstr->crypts[key_idx].crypt, IV, 16);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Cannot set key AES-128 %s (%s)\n", cstr->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			return e;
		}
	}
	return GF_OK;
}

u8 key_info_get_iv_size(const u8 *key_info, u32 nb_keys, u32 idx, u8 *const_iv_size, const u8 **const_iv);

static GFINLINE void cenc_decrypt_block(GF_CENCDecCtx *ctx, GF_Crypt *crypt, Bool valid_key, u8 *data, u32 size)
{
	if (!valid_key) {
		if (ctx->decrypt==DECRYPT_PAD1) {
			memset(data, 0xFF, size);
		}
		else if (ctx->decrypt==DECRYPT_PAD0) {
			memset(data, 0x0, size);
		}
		else if (ctx->decrypt==DECRYPT_PADSC) {
			memset(data, 0x0, size-1);
			data[size-1] = 1;
		} else {
			//skip, do nothing
		}
	} else {
		gf_crypt_decrypt(crypt, data, size);
	}
}
static GF_Err cenc_dec_process_cenc(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, GF_FilterPacket *in_pck)
{
	GF_Err e = GF_OK;
	u32 subsample_count;
	u32 data_size;
	u8 *out_data;
	const char *sai_payload=NULL;
	u32 saiz=0;
	u32 min_sai_size_subs = 6;
	GF_FilterPacket *out_pck;
	const GF_PropertyValue *prop;
	u32 skey_const_iv_size = 0;
	Bool has_subsamples = GF_FALSE;
	const u8 *skey_const_iv = NULL;

	//packet has been fetched, we now MUST have a key info
	if (!cstr->cenc_ki) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Packet encrypted but no KID info\n" ) );
		return GF_SERVICE_ERROR;
	}

	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PID_CENC_PSSH);
	if (prop && (prop->type==GF_PROP_DATA) && prop->value.data.ptr) {
		cenc_dec_load_pssh(ctx, cstr, prop, GF_TRUE, NULL);
	}
	gf_filter_pck_get_data(in_pck, &data_size);

	if (!data_size || ! gf_filter_pck_get_crypt_flags(in_pck)) {
		out_pck = gf_filter_pck_new_ref(cstr->opid, 0, 0, in_pck);
		if (!out_pck) return GF_OUT_OF_MEM;
		gf_filter_pck_merge_properties(in_pck, out_pck);
		gf_filter_pck_set_property(out_pck, GF_PROP_PCK_CENC_SAI, NULL);
		gf_filter_pck_set_crypt_flags(out_pck, 0);
		gf_filter_pck_send(out_pck);
		return GF_OK;
	}


	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_CENC_SAI);
	if (prop) {
		sai_payload = prop->value.data.ptr;
		saiz = prop->value.data.size;
		gf_bs_reassign_buffer(ctx->bs_r, sai_payload, saiz);
	}

	//CENC can use inplace processing for decryption
	out_pck = gf_filter_pck_new_clone(cstr->opid, in_pck, &out_data);
	if (!out_pck) return GF_OUT_OF_MEM;

	subsample_count = 0;

	if (cstr->multikey) {
		u8 IV[17];
		u32 k, nb_iv_init;
		min_sai_size_subs = 8;
		if (!sai_payload
			//we need at least 2bytes for IV counts and 4 for nb_subsamples, mandatory in multikey
			|| (gf_bs_available(ctx->bs_r) < 6 )
		) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Multikey with no associated auxiliary info !\n" ) );
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		nb_iv_init = gf_bs_read_u16(ctx->bs_r);
		//init all non-const IV listed
		for (k=0; k<nb_iv_init; k++) {
			u8 IV_size;
			u32 kidx = gf_bs_read_u16(ctx->bs_r);

			if (!kidx || (kidx>cstr->nb_crypts)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Corrupted CENC sai, kidx %d but valid range is [1,%d]\n", kidx, cstr->nb_crypts));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			IV_size = key_info_get_iv_size(cstr->cenc_ki->value.data.ptr, cstr->cenc_ki->value.data.size, kidx, NULL, NULL);
			if (!IV_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] invalid SAI multikey with IV size 0\n" ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			//memset to 0 in case we use <16 byte key
			memset(IV, 0, sizeof(u8)*17);
			gf_bs_read_data(ctx->bs_r, IV, IV_size);

			e = cenc_dec_push_iv(cstr, kidx-1, IV, IV_size, 0, NULL);
			if (e) goto exit;
		}
		if (nb_iv_init < cstr->multikey) {
			//init all const IV listed
			for (k=0; k<cstr->multikey; k++) {
				u8 const_iv_size;
				const u8 *const_iv=NULL;
				u8 IV_size = key_info_get_iv_size(cstr->cenc_ki->value.data.ptr, cstr->cenc_ki->value.data.size, k+1, &const_iv_size, &const_iv);
				if (IV_size) continue;
				memset(IV, 0, sizeof(char)*17);
				e = cenc_dec_push_iv(cstr, k, IV, 0, const_iv_size, const_iv);
				if (e) goto exit;
			}
		}
	} else {
		u8 IV[17];
		u32 iv_size = 0;

		//check single key IV is OK - in multikey we ALWAYS have SAI payload
		if (sai_payload) {
			iv_size = cstr->cenc_ki->value.data.ptr[3];
		}
		if (!iv_size) {
			if (! cstr->cenc_ki->value.data.ptr[3]) {
				skey_const_iv_size = cstr->cenc_ki->value.data.ptr[20];
			}
		}

		if (!sai_payload && !skey_const_iv_size) {
			if (ctx->decrypt >= DECRYPT_SKIP) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Packet encrypted but no SAI info nor constant IV\n" ) );
				e = GF_OK;
				goto send_packet;
			}
			if (gf_filter_pck_get_crypt_flags(in_pck)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Packet encrypted but no SAI info nor constant IV\n" ) );
			}
			return GF_SERVICE_ERROR;
		}

		//memset to 0 in case we use <16 byte key
		memset(IV, 0, sizeof(char)*17);
		if (sai_payload) {
			if (iv_size)
				gf_bs_read_data(ctx->bs_r, IV, iv_size);
		}

		if (skey_const_iv_size)
			skey_const_iv = cstr->cenc_ki->value.data.ptr+21;

		e = cenc_dec_push_iv(cstr, 0, IV, iv_size, skey_const_iv_size, skey_const_iv);
		if (e) goto exit;
	}
	cstr->crypt_init = GF_TRUE;

	if (cstr->key_error) {
		e = cstr->key_error;
		goto exit;
	}

	if (cstr->multikey) {
		subsample_count = gf_bs_read_u32(ctx->bs_r);
		has_subsamples = GF_TRUE;
	} else {
		if (sai_payload && gf_bs_available(ctx->bs_r)) {
			subsample_count = gf_bs_read_u16(ctx->bs_r);
			has_subsamples = GF_TRUE;
		}
	}
	if (has_subsamples && !subsample_count) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC] Subsample field present but no subsamples indicated, assuming clear payload\n"));
		goto send_packet;
	}

	//sub-sample encryption, always on for multikey
	if (subsample_count || cstr->multikey) {
		u32 cur_pos = 0;

		while (cur_pos < data_size) {
			Bool valid_key;
			u8 const_iv_size=0;
			const u8 *const_iv=NULL;
			u32 kidx = 0;
			u32 bytes_clear_data, bytes_encrypted_data;
			if (subsample_count==0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Corrupted CENC sai, not enough subsamples described\n" ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			if (gf_bs_available(ctx->bs_r) < min_sai_size_subs) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Corrupted CENC sai, not enough bytes in subsample info\n" ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			if (cstr->multikey) {
				kidx = gf_bs_read_u16(ctx->bs_r);
				//check index is valid
				if (kidx>cstr->nb_crypts) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Corrupted CENC sai key idx %d, should be in range [1, %d]\n", kidx, cstr->nb_crypts));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
				bytes_clear_data = gf_bs_read_u16(ctx->bs_r);
				bytes_encrypted_data = gf_bs_read_u32(ctx->bs_r);

				if (kidx) {
					key_info_get_iv_size(cstr->cenc_ki->value.data.ptr, cstr->cenc_ki->value.data.size, kidx, &const_iv_size, &const_iv);
					kidx-=1;
				}
				//to clarify in the spec: kidx 0 should be allowed for clear subsamples
				else if (bytes_encrypted_data) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Corrupted CENC sai key idx 0 but encrypted payload\n", cstr->nb_crypts));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
			} else {
				const_iv = skey_const_iv;
				const_iv_size = skey_const_iv_size;
				bytes_clear_data = gf_bs_read_u16(ctx->bs_r);
				bytes_encrypted_data = gf_bs_read_u32(ctx->bs_r);
			}

			subsample_count--;

			//const IV is applied at each subsample
			if (const_iv_size) {
				u8 IV[17];
				memcpy(IV, const_iv, const_iv_size);
				if (const_iv_size == 8)
					memset(IV+8, 0, sizeof(char)*8);

				if (cstr->force_hls_iv)
					memcpy(IV, cstr->hls_IV, sizeof(bin128));

				gf_crypt_set_IV(cstr->crypts[kidx].crypt, IV, 16);
			}
			if (cur_pos + bytes_clear_data + bytes_encrypted_data > data_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC] Corrupted CENC sai, subsample info describe more bytes (%d) than in packet (%d)\n", cur_pos + bytes_clear_data + bytes_encrypted_data , data_size ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			/*skip clear data*/
			cur_pos += bytes_clear_data;
			valid_key = (ctx->decrypt>=DECRYPT_SKIP) ? GF_FALSE : cstr->crypts[kidx].key_valid;
			if (!valid_key && (ctx->decrypt==DECRYPT_SKIP)) {
				cur_pos += bytes_encrypted_data;
				continue;
			}

			//pattern decryption
			if (cstr->cenc_pattern) {
				u32 pos = cur_pos;
				u32 res = bytes_encrypted_data;
				u32 cryp_block = 16 * cstr->cenc_pattern->value.frac.den;
				u32 full_block = 16 * (cstr->cenc_pattern->value.frac.den + cstr->cenc_pattern->value.frac.num);

				if (cstr->is_cbc) {
					u32 clear_trailing = res % 16;
					res -= clear_trailing;
				}

				while (res) {
					u32 c_len = (res >= cryp_block) ? cryp_block : res;
					cenc_decrypt_block(ctx, cstr->crypts[kidx].crypt, valid_key, out_data + pos, c_len);

					if (res >= full_block) {
						pos += full_block;
						gf_assert(res>=full_block);
						res -= full_block;
					} else {
						res = 0;
					}
				}
			}
			//full subsample decryption
			else {
				u32 res = bytes_encrypted_data;
				//trailing will be 0 in cbc1 (as cbc1 mandates bytes_encrypted_data % 16 == 0)
				//but can be non-0 in cbcs NALU-based without pattern (not defined in CENC)
				//in this case, we must only decrypt a multiple of 16-byte blocks
				//note that vpX cbcs mandates bytes_encrypted_data % 16 == 0, as cbc1
				if (cstr->is_cbc) {
					u32 clear_trailing = res % 16;
					res -= clear_trailing;
				}
				cenc_decrypt_block(ctx, cstr->crypts[kidx].crypt, valid_key, out_data+cur_pos, res);
			}
			cur_pos += bytes_encrypted_data;
		}
	}
	//full sample encryption in single key mode
	else {
		u32 cbytes = data_size;
		if (!cstr->is_cenc) {
			u32 ret = data_size % 16;
			if (data_size >= 16) {
				cbytes = data_size-ret;
			} else {
				cbytes = 0;
			}
		}
		if (cbytes) {
			Bool valid_key = (ctx->decrypt>=DECRYPT_SKIP) ? GF_FALSE : cstr->crypts[0].key_valid;
			cenc_decrypt_block(ctx, cstr->crypts[0].crypt, valid_key, out_data, cbytes);
		}
	}

send_packet:
	gf_filter_pck_set_property(out_pck, GF_PROP_PCK_CENC_SAI, NULL);
	gf_filter_pck_set_crypt_flags(out_pck, 0);

	gf_filter_pck_send(out_pck);

exit:
	if (e && out_pck) {
		gf_filter_pck_discard(out_pck);
	}
	return e;
}

static GF_Err cenc_dec_process_hls_saes(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, GF_FilterPacket *in_pck)
{
	GF_Err e = GF_OK;
	u32 data_size;
	u8 *out_data;
	GF_FilterPacket *out_pck;

	gf_filter_pck_get_data(in_pck, &data_size);

	if (!data_size) {
		out_pck = gf_filter_pck_new_ref(cstr->opid, 0, 0, in_pck);
		if (!out_pck) return GF_OUT_OF_MEM;
		gf_filter_pck_merge_properties(in_pck, out_pck);
		gf_filter_pck_set_property(out_pck, GF_PROP_PCK_CENC_SAI, NULL);
		gf_filter_pck_set_crypt_flags(out_pck, 0);
		gf_filter_pck_send(out_pck);
		return GF_OK;
	}

	//we can use inplace processing for decryption
	out_pck = gf_filter_pck_new_clone(cstr->opid, in_pck, &out_data);
	if (!out_pck) return GF_OUT_OF_MEM;

	//packet has been fetched, we now MUST have a key info

	if (!cstr->hls_key_url && !cstr->cenc_ki && !ctx->cinfo) {
		if (ctx->decrypt >= DECRYPT_SKIP) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[HLS_SAES] Packet encrypted but no KEY info\n" ) );
			e = GF_OK;
			goto send_packet;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[HLS_SAES] Packet encrypted but no KEY info\n" ) );
		return GF_SERVICE_ERROR;
	}
	if (cstr->cenc_ki) {
		if (cstr->cenc_ki->value.data.size<37) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[HLS_SAES] Broken KID for HLS SAES\n" ) );
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		e = cenc_dec_push_iv(cstr, 0, cstr->cenc_ki->value.data.ptr + 21, 16, 0, NULL);
		if (e) goto exit;
		memcpy(cstr->hls_IV, cstr->cenc_ki->value.data.ptr + 21, 16);
	} else {
		e = cenc_dec_push_iv(cstr, 0, cstr->hls_IV, 16, 0, NULL);
		if (e) goto exit;
	}
	cstr->crypt_init = GF_TRUE;

	if (cstr->key_error) {
		e = cstr->key_error;
		goto exit;
	}

	if (cstr->codec_id==GF_CODECID_AVC) {
		u32 cur_pos = 0;
		u32 o_data_size = data_size;
		u8 *data = out_data;

		while (cur_pos + 4 < data_size) {
			u32 i;
			u32 nal_size=0;
			u32 bk_idx = 0;
			u8 *nal_start, *nal_hdr_ptr;
			u32 nal_size_o;
			for (i=0; i<4; i++) {
				nal_size <<= 8;
				nal_size |= data[i];
			}
			if (cur_pos+4+nal_size > data_size) {
				break;
			}
			nal_hdr_ptr = data;
			cur_pos += 4;
			data += 4;

			u8 nal_hdr = data[0];
			u8 nal_type = nal_hdr & 0x1F;

			if ((nal_size<=48) || ((nal_type != 1) && (nal_type != 5))) {
				data += nal_size;
				cur_pos += nal_size;
				continue;
			}
			nal_start = data;
			nal_size_o = nal_size;
			//remove EPB
			u32 nb_epb = 0;
#ifndef GPAC_DISABLE_AV_PARSERS
			nb_epb = gf_media_nalu_emulation_bytes_remove_count(data, nal_size);
			if (nb_epb) {
				nal_size = gf_media_nalu_remove_emulation_bytes(data, data, nal_size);
			}
#endif
			//unencrypted header
			data += 32;
			cur_pos += 32;
			nal_size -= 32;

			//const IV is applied at each subsample
			gf_crypt_set_IV(cstr->crypts[0].crypt, cstr->hls_IV, 16);

			while (nal_size) {
				Bool is_crypted = GF_FALSE;
				if (! (bk_idx % 10)) is_crypted = GF_TRUE;

				if (is_crypted && cstr->crypts[0].key_valid) {
					//decrypt
					gf_crypt_decrypt(cstr->crypts[0].crypt, data, 16);
				}

				bk_idx++;
				nal_size -= 16;
				data += 16;
				cur_pos += 16;
				if (nal_size < 16) break;
			}
			//unencrypted trailer
			data += nal_size;
			cur_pos += nal_size;

			if (nb_epb) {
				u32 remain = data_size - (cur_pos+nb_epb);
				//move all remainging bytes
				memmove(nal_start + nal_size_o - nb_epb, nal_start + nal_size_o, remain);
				//rewrite NALU length field
				nal_size_o -= nb_epb;
				nal_hdr_ptr[0] = ((nal_size_o>>24) & 0xFF);
				nal_hdr_ptr[1] = ((nal_size_o>>16) & 0xFF);
				nal_hdr_ptr[2] = ((nal_size_o>>8) & 0xFF);
				nal_hdr_ptr[3] = ((nal_size_o) & 0xFF);

				data_size -= nb_epb;
			}
		}
		if (o_data_size > data_size)
			gf_filter_pck_truncate(out_pck, data_size);
	}
	//otherwise audio, same scheme for all
	else {
		u8 *data = out_data;
		//clear 16 bytes header
		data += 16;
		if (data_size>16) {
			data_size-=16;

			//const IV is applied at each sample
			gf_crypt_set_IV(cstr->crypts[0].crypt, cstr->hls_IV, 16);
		} else {
			data_size=0;
		}

		while (data_size>0) {
			gf_crypt_decrypt(cstr->crypts[0].crypt, data, 16);
			data += 16;
			data_size -= 16;
			if (data_size<16)
				break;
		}
	}


send_packet:
	gf_filter_pck_set_property(out_pck, GF_PROP_PCK_CENC_SAI, NULL);
	gf_filter_pck_set_crypt_flags(out_pck, 0);

	gf_filter_pck_send(out_pck);

exit:
	if (e && out_pck) {
		gf_filter_pck_discard(out_pck);
	}
	return e;
}

static GF_Err cenc_dec_process_adobe(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, GF_FilterPacket *in_pck)
{
	u32 data_size;
	const u8 *in_data;
	u8 *out_data;
	GF_FilterPacket *out_pck;
	GF_Err e;
	u32 offset, size;
	Bool encrypted_au;

	if (!cstr->crypts[0].crypt)
		return GF_SERVICE_ERROR;

	in_data = gf_filter_pck_get_data(in_pck, &data_size);
	out_pck = gf_filter_pck_new_alloc(cstr->opid, data_size, &out_data);
	if (!out_pck) return GF_OUT_OF_MEM;

	memcpy(out_data, in_data, data_size);

	offset=0;
	size = data_size;
	encrypted_au = out_data[0] ? GF_TRUE : GF_FALSE;
	if (encrypted_au) {
		u32 trim_bytes;
		char IV[17];
		if (size<17) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ADOBE] Error in sample size, %d bytes remain but at least 17 are required\n", size ) );
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		memmove(IV, out_data+1, 16);
		if (!cstr->crypt_init) {
			e = gf_crypt_init(cstr->crypts[0].crypt, cstr->keys[0], IV);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ADOBE] Cannot initialize AES-128 CBC (%s)\n", gf_error_to_string(e)) );
				return GF_IO_ERR;
			}
			cstr->crypt_init = GF_TRUE;
		} else {
			e = gf_crypt_set_IV(cstr->crypts[0].crypt, IV, GF_AES_128_KEYSIZE);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ADOBE] Cannot set state AES-128 CBC (%s)\n", gf_error_to_string(e)) );
				return GF_IO_ERR;
			}
		}
		offset += 17;
		size -= 17;

		gf_crypt_decrypt(cstr->crypts[0].crypt, out_data+offset, size);
		trim_bytes = out_data[offset + size - 1];
		size -= trim_bytes;
	} else {
		offset += 1;
		size -= 1;
	}
	memmove(out_data, out_data+offset, size);
	gf_filter_pck_truncate(out_pck, size);
	gf_filter_pck_merge_properties(in_pck, out_pck);

	gf_filter_pck_send(out_pck);
	return GF_OK;
}

static void cenc_dec_stream_del(GF_CENCDecStream *cstr)
{
	if (cstr->crypts) {
		u32 i;
		for (i=0; i<cstr->nb_crypts; i++) {
			if (cstr->crypts[i].crypt) gf_crypt_close(cstr->crypts[i].crypt);
		}
		gf_free(cstr->crypts);
	}
	if (cstr->KIDs) gf_free(cstr->KIDs);
	if (cstr->keys) gf_free(cstr->keys);
	if (cstr->hls_key_url) gf_free(cstr->hls_key_url);

	if (cstr->body) gf_free(cstr->body);
#ifdef GPAC_USE_DOWNLOADER
	if (cstr->sess) gf_dm_sess_del(cstr->sess);
#endif
	gf_free(cstr);
}

static GF_Err cenc_dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_Err e = GF_OK;
	u32 scheme_type = 0;
	u32 scheme_version = 0;
	const char *scheme_uri = NULL;
	const char *kms_uri = NULL;
	u32 nb_keys;
	GF_CENCDecStream *cstr;
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);
	u32 i;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
	if (prop) scheme_type = prop->value.uint;

	if (is_remove) {
		cstr = gf_filter_pid_get_udta(pid);
		if (!cstr) return GF_OK; //configure failure
		if (cstr->opid) {
			gf_filter_pid_remove(cstr->opid);
		}
		gf_list_del_item(ctx->streams, cstr);
		cenc_dec_stream_del(cstr);
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	cstr = gf_filter_pid_get_udta(pid);
	if (!cstr) {
		GF_SAFEALLOC(cstr, GF_CENCDecStream);
		if (!cstr) return GF_OUT_OF_MEM;
		cstr->opid = gf_filter_pid_new(filter);
		cstr->ipid = pid;
		cstr->ctx = ctx;
		gf_list_add(ctx->streams, cstr);
		gf_filter_pid_set_udta(pid, cstr);
		gf_filter_pid_set_udta(cstr->opid, cstr);
		//we need full sample
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_VERSION);
	if (prop) scheme_version = prop->value.uint;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_URI);
	if (prop) scheme_uri = prop->value.string;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_KMS_URI);
	if (prop) kms_uri = prop->value.string;

	if (ctx->cinfo) {
		u32 stream_id=0;
		u32 count = gf_list_count(ctx->cinfo->tcis);
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!prop) prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
		if (prop) stream_id = prop->value.uint;

		for (i=0; i<count; i++) {
			GF_TrackCryptInfo *tci = (GF_TrackCryptInfo *) gf_list_get(ctx->cinfo->tcis, i);
			if ((ctx->cinfo->has_common_key && !tci->trackID) || (tci->trackID == stream_id) ) {
				if (tci->force_type) scheme_type = tci->scheme_type;
				break;
			}
		}
	}

	nb_keys = 1;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_KEY_INFO);
	if (prop && prop->value.data.ptr && prop->value.data.ptr[0]) {
		nb_keys = prop->value.data.ptr[1];
		nb_keys<<=8;
		nb_keys |= prop->value.data.ptr[2];
	}
	if (nb_keys > cstr->nb_crypts) {
		cstr->crypts = gf_realloc(cstr->crypts, sizeof(CENCDecKey) * nb_keys);
		memset(&cstr->crypts[cstr->nb_crypts], 0, sizeof(CENCDecKey) * (nb_keys-cstr->nb_crypts) );

		if (cstr->crypts[0].crypt) {
			for (i=cstr->nb_crypts; i<nb_keys; i++) {
				if (cstr->is_cenc)
					cstr->crypts[i].crypt = gf_crypt_open(GF_AES_128, GF_CTR);
				else
					cstr->crypts[i].crypt = gf_crypt_open(GF_AES_128, GF_CBC);
				if (!cstr->crypts[i].crypt) return GF_IO_ERR;
			}
		}
		cstr->nb_crypts = nb_keys;
	}
	//resetup, check decrypter
	if (cstr->state==DECRYPT_STATE_PLAY) {
		for (i=0; i<cstr->nb_crypts; i++) {
			if (!cstr->crypts[i].crypt) {
				return GF_SERVICE_ERROR;
			}
		}
	}

	switch (scheme_type) {
	case GF_ISOM_ISMACRYP_SCHEME:
		e = cenc_dec_setup_isma(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_OMADRM_SCHEME:
#ifdef OLD_KEY_FETCHERS
		e = cenc_dec_setup_oma(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
#else
		e = GF_NOT_SUPPORTED;
#endif
		break;
	case GF_ISOM_CENC_SCHEME:
	case GF_ISOM_PIFF_SCHEME:
	case GF_ISOM_CBC_SCHEME:
	case GF_ISOM_CENS_SCHEME:
	case GF_ISOM_CBCS_SCHEME:
		e = cenc_dec_setup_cenc(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_ADOBE_SCHEME:
		e = cenc_dec_setup_adobe(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_HLS_SAMPLE_AES_SCHEME:
		e = cenc_dec_hls_saes(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Protection scheme type %s not supported\n", gf_4cc_to_str(scheme_type) ) );
		return GF_SERVICE_ERROR;
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENC/ISMA] Error setting up protection scheme type %s\n", gf_4cc_to_str(scheme_type) ) );
		return e;
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(cstr->opid, pid);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
	if (prop) {
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(prop->value.uint) );
		gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ORIG_STREAM_TYPE, NULL);
	}
	//remove all cenc properties on output
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_SCHEME_URI, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_PROTECTION_KMS_URI, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_OMA_PREVIEW_RANGE, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_PSSH, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ENCRYPTED, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_KEY_INFO, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_PATTERN, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_HLS_KMS, NULL);

	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_ORIG_CRYPT_SCHEME, &PROP_4CC(scheme_type) );


	cstr->is_nalu = GF_FALSE;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (prop) {
		switch (prop->value.uint) {
		case GF_CODECID_AVC:
		case GF_CODECID_SVC:
		case GF_CODECID_MVC:
		case GF_CODECID_HEVC:
		case GF_CODECID_HEVC_TILES:
		case GF_CODECID_LHVC:
		case GF_CODECID_VVC:
		case GF_CODECID_VVC_SUBPIC:
			cstr->is_nalu = GF_TRUE;
			break;
		}
	}
	return e;
}


static Bool cenc_dec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterPid *ipid;
	GF_CENCDecStream *cstr;
	Bool is_play = GF_FALSE;
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);

	ipid = evt->base.on_pid;
	if (!ipid) return GF_FALSE;
	cstr = gf_filter_pid_get_udta(ipid);
	if (!cstr) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		is_play = GF_TRUE;
	case GF_FEVT_STOP:
		if (cstr->is_cenc || cstr->is_cbc) {
			cenc_dec_access_cenc(ctx, cstr, is_play);
		} else if (cstr->is_adobe) {
			cenc_dec_access_adobe(ctx, cstr, is_play);
		} else if (cstr->is_oma) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] OMA not supported, canceling filter event\n") );
			return GF_TRUE;
		} else {
			cenc_dec_access_isma(ctx, cstr, is_play);
		}
		if (!is_play)
			cstr->crypt_init = GF_FALSE;
		break;
	default:
		break;
	}
	return GF_FALSE;
}
static GF_Err cenc_dec_process(GF_Filter *filter)
{
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);
	u32 i, nb_eos, count = gf_list_count(ctx->streams);

	if (ctx->pending_keys)
		return GF_OK;

	nb_eos = 0;
	for (i=0; i<count; i++) {
		GF_Err e;
		GF_CENCDecStream *cstr = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(cstr->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(cstr->ipid)) {
				nb_eos++;
				gf_filter_pid_set_eos(cstr->opid);
			}
			continue;
		}

		if (cstr->state != DECRYPT_STATE_PLAY) {
			gf_filter_pid_drop_packet(cstr->ipid);
			continue;
		}

		if (cstr->is_hls_saes) {
			e = cenc_dec_process_hls_saes(ctx, cstr, pck);
		} else if (cstr->is_cenc || cstr->is_cbc) {
			e = cenc_dec_process_cenc(ctx, cstr, pck);
		} else if (cstr->is_oma) {
			e = GF_NOT_SUPPORTED;
		} else if (cstr->is_adobe) {
			e = cenc_dec_process_adobe(ctx, cstr, pck);
		} else {
			e = cenc_dec_process_isma(ctx, cstr, pck);
		}
		gf_filter_pid_drop_packet(cstr->ipid);
		if (e) return e;
	}
	if (nb_eos && (nb_eos==count)) return GF_EOS;
	return GF_OK;
}

static GF_Err cenc_dec_initialize(GF_Filter *filter)
{
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);
	ctx->filter = filter;

	if (ctx->keys.nb_items) {
		if (ctx->keys.nb_items != ctx->kids.nb_items) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCCrypt] Number of defined keys (%d) must be the same as number of defined KIDs (%d)\n", ctx->keys.nb_items, ctx->kids.nb_items ));
			return GF_BAD_PARAM;
		}
	}
	ctx->streams = gf_list_new();
	if (!ctx->streams) return GF_OUT_OF_MEM;

	if (ctx->cfile) {
		GF_Err e;
		ctx->cinfo = gf_crypt_info_load(ctx->cfile, &e);
		if (!ctx->cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[CENCCrypt] Cannot load config file %s\n", ctx->cfile ));
			return e;
		}
	}
	ctx->bs_r = gf_bs_new((char *) ctx, 1, GF_BITSTREAM_READ);
	return GF_OK;
}

static void cenc_dec_finalize(GF_Filter *filter)
{
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		GF_CENCDecStream *cstr = gf_list_pop_back(ctx->streams);
		cenc_dec_stream_del(cstr);
	}
	gf_list_del(ctx->streams);

	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->cinfo) gf_crypt_info_del(ctx->cinfo);
}


static const GF_FilterCapability CENCDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_ISMACRYP_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_OMADRM_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CENC_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CENS_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CBC_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CBCS_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_ADOBE_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_PIFF_SCHEME),
	CAP_4CC(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_HLS_SAMPLE_AES_SCHEME),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_CENCDecCtx, _n)
static const GF_FilterArgs GF_CENCDecArgs[] =
{
	{ OFFS(cfile), "crypt file location", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(decrypt), "decrypt mode (CENC only)\n"
		"- full: decrypt everything, throwing error if keys are not found\n"
		"- nokey: decrypt everything for which a key is found, skip decryption otherwise\n"
		"- skip: decrypt nothing\n"
		"- pad0: decrypt nothing and replace all crypted bits with 0\n"
		"- pad1: decrypt nothing and replace all crypted bits with 1\n"
		"- padsc: decrypt nothing and replace all crypted bytes with start codes"
		, GF_PROP_UINT, "full", "full|nokey|skip|pad0|pad1|padsc", GF_ARG_HINT_ADVANCED},
	{ OFFS(drop_keys), "consider keys with given 1-based indexes as not available (multi-key debug)", GF_PROP_UINT_LIST, NULL, NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(kids), "define KIDs. If `keys` is empty, consider keys with given KID (as hex string) as not available (debug)", GF_PROP_STRING_LIST, NULL, NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(keys), "define key values for each of the specified KID", GF_PROP_STRING_LIST, NULL, NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(hls_cenc_patch_iv), "ignore IV updates in some broken HLS+CENC streams", GF_PROP_BOOL, "false", NULL, GF_ARG_HINT_EXPERT},
	{0}
};

GF_FilterRegister CENCDecRegister = {
	.name = "cdcrypt",
	GF_FS_SET_DESCRIPTION("CENC decryptor")
	GF_FS_SET_HELP("The CENC decryptor supports decrypting CENC, ISMA, HLS Sample-AES (MPEG2 ts) and Adobe streams.\n"
	"\n"
	"For HLS, key is retrieved according to the key URI in the manifest.\n"
	"Otherwise, the filter uses a configuration file.\n"
	"The syntax is available at https://wiki.gpac.io/xmlformats/Common-Encryption\n"
	"The DRM config file can be set per PID using the property `DecryptInfo` (highest priority), `CryptInfo` (lower priority) "
	"or set at the filter level using [-cfile]() (lowest priority).\n"
	"When the file is set per PID, the first `CryptInfo` with the same ID is used, otherwise the first `CryptInfo` is used."
	"When the file is set globally (not per PID), the first `CrypTrack` in the DRM config file with the same ID is used, otherwise the first `CrypTrack` with ID 0 or not set is used.\n"
	)
	.private_size = sizeof(GF_CENCDecCtx),
	.flags = GF_FS_REG_USE_SYNC_READ,
	.max_extra_pids=-1,
	.args = GF_CENCDecArgs,
	SETCAPS(CENCDecCaps),
	.configure_pid = cenc_dec_configure_pid,
	.initialize = cenc_dec_initialize,
	.finalize = cenc_dec_finalize,
	.process = cenc_dec_process,
	.process_event = cenc_dec_process_event
	//for now only one PID per CENC decryptor instance, could be further optimized
};

#endif /*GPAC_DISABLE_CRYPTO*/

const GF_FilterRegister *cdcrypt_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_CRYPTO) && !defined(GPAC_DISABLE_CDCRYPT)
	return &CENCDecRegister;
#else
	return NULL;
#endif
}
