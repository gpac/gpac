/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
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

#ifndef GPAC_DISABLE_CRYPTO


enum
{
	DECRYPT_STATE_ERROR,
	DECRYPT_STATE_SETUP,
	DECRYPT_STATE_PLAY,
};

typedef struct
{
	GF_FilterPid *ipid, *opid;

	GF_Crypt *crypt;
	bin128 key, kid;
	u32 state;
	u32 pssh_crc;
	u32 scheme_type, scheme_version;

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
	bin128 *KIDs;
	bin128 *keys;

	/*adobe*/
	Bool crypt_init;
} GF_CENCDecStream;

typedef struct
{
	const char *cfile;

	GF_CryptInfo *cinfo;
	
	GF_List *streams;
	GF_BitStream *bs_r;

	GF_DownloadManager *dm;
} GF_CENCDecCtx;


static void cenc_dec_kms_netio(void *cbck, GF_NETIO_Parameter *par)
{
}

static GF_Err gf_ismacryp_gpac_get_info(u32 stream_id, char *drm_file, char *key, char *salt)
{
	GF_Err e;
	u32 i, count;
	GF_CryptInfo *info;
	GF_TrackCryptInfo *tci;

	e = GF_OK;
	info = gf_crypt_info_load(drm_file);
	if (!info) return GF_NOT_SUPPORTED;
	count = gf_list_count(info->tcis);
	for (i=0; i<count; i++) {
		tci = (GF_TrackCryptInfo *) gf_list_get(info->tcis, i);
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
	GF_Err e;
	FILE *t;
	u32 id = 0;
	GF_FilterPid *pid = cstr->ipid;
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
	/*note that gpac doesn't have TLS support -> not really usefull. As a general remark, ISMACryp
	is supported as a proof of concept, crypto and IPMP being the last priority on gpac...*/
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[CENC/ISMA] Fetching ISMACryp key for channel %d\n", id) );

	sess = gf_dm_sess_new(ctx->dm, kms_url, 0, cenc_dec_kms_netio, ctx, NULL);
	if (!sess) return GF_IO_ERR;
	/*start our download (threaded)*/
	gf_dm_sess_process(sess);

	while (1) {
		e = gf_dm_sess_get_stats(sess, NULL, NULL, NULL, NULL, NULL, NULL);
		if (e) break;
	}
	if (e==GF_EOS) {
		e = gf_ismacryp_gpac_get_info(id, (char *) gf_dm_sess_get_cache_name(sess), cstr->key, cstr->salt);
	}
	gf_dm_sess_del(sess);
	return e;
}

static Bool gf_ismacryp_mpeg4ip_get_info(char *kms_uri, char *key, char *salt)
{
	char szPath[1024], catKey[24];
	u32 i, x;
	Bool got_it;
	FILE *kms;
	strcpy(szPath, getenv("HOME"));
	strcat(szPath , "/.kms_data");
	got_it = 0;
	kms = gf_fopen(szPath, "rt");
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


static GF_Err cenc_dec_setup_isma(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	GF_Err e;
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
		memcpy(cstr->key, data, sizeof(char)*16);
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
			cstr->key[i] = v;
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
		if (!gf_ismacryp_mpeg4ip_get_info((char *) kms_uri, cstr->key, cstr->salt)) {
			return GF_BAD_PARAM;
		}
	}
	/*gpac default scheme is used, fetch file from KMS and load keys*/
	else if (scheme_uri && !stricmp(scheme_uri, "urn:gpac:isma:encryption_scheme")) {
		e = cenc_dec_get_gpac_kms(ctx, cstr, kms_uri);
		if (e) return e;
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
		memcpy(cstr->key, mykey, sizeof(char)*16);
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
	char IV[16];

	if (is_play) {
		if (cstr->state != DECRYPT_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!cstr->crypt);

		//if (!ctx->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//ctx->nb_allow_play--;

		/*init decrypter*/
		cstr->crypt = gf_crypt_open(GF_AES_128, GF_CTR);
		if (!cstr->crypt) return GF_IO_ERR;

		memset(IV, 0, sizeof(char)*16);
		memcpy(IV, cstr->salt, sizeof(char)*8);
		e = gf_crypt_init(cstr->crypt, cstr->key, IV);
		if (e) return e;

		cstr->state = DECRYPT_STATE_PLAY;
		return GF_OK;
	} else {
		if (cstr->state != DECRYPT_STATE_PLAY) return GF_SERVICE_ERROR;
		if (cstr->crypt) gf_crypt_close(cstr->crypt);
		cstr->crypt = NULL;
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
	if (!cstr->crypt) return GF_SERVICE_ERROR;

	if (! gf_filter_pck_get_crypt_flags(in_pck)) {
		out_pck = gf_filter_pck_new_ref(cstr->opid, NULL, 0, in_pck);
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
		gf_crypt_set_IV(cstr->crypt, IV, 17);

		/*decrypt remain bytes*/
		if (remain) {
			char dummy[20];
			gf_crypt_decrypt(cstr->crypt, dummy, remain);
		}
		cstr->last_IV = isma_BSO;
	}
	in_data += offset;
	data_size -= offset;

	out_pck = gf_filter_pck_new_alloc(cstr->opid, data_size, &out_data);

	memcpy(out_data, in_data, data_size);
	/*decrypt*/
	gf_crypt_decrypt(cstr->crypt, out_data, data_size);
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

static GF_Err cenc_dec_setup_cenc(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	GF_CryptInfo *cinfo=NULL;
	u32 i, nb_pssh, pssh_crc=0;
	const GF_PropertyValue *prop, *cinfo_prop;
	char *pssh_data=NULL;
	GF_FilterPid *pid = cstr->ipid;
	Bool is_playing = (cstr->state == DECRYPT_STATE_PLAY) ? GF_TRUE : GF_FALSE;

	cstr->state = DECRYPT_STATE_ERROR;

	if ((scheme_type != GF_ISOM_CENC_SCHEME) && (scheme_type != GF_ISOM_CBC_SCHEME) && (scheme_type != GF_ISOM_CENS_SCHEME) && (scheme_type != GF_ISOM_CBCS_SCHEME))
		return GF_NOT_SUPPORTED;
	if (scheme_version != 0x00010000) return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_PSSH);
	if (prop) {
		pssh_data = (char *) prop->value.data.ptr;
		pssh_crc = gf_crc_32(prop->value.data.ptr, prop->value.data.size);
	}
	if ((cstr->scheme_type==scheme_type) && (cstr->scheme_version==scheme_version) && (cstr->pssh_crc==pssh_crc) )
		return GF_OK;

	cstr->scheme_version = scheme_version;
	cstr->scheme_type = scheme_type;
	cstr->pssh_crc = pssh_crc;

	if ((scheme_type == GF_ISOM_CENC_SCHEME) || (scheme_type == GF_ISOM_CENS_SCHEME))
		cstr->is_cenc = GF_TRUE;
	else
		cstr->is_cbc = GF_TRUE;

	cstr->state = is_playing ? DECRYPT_STATE_PLAY : DECRYPT_STATE_SETUP;
	//ctx->nb_allow_play = 1;

	cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_DECRYPT_INFO);
	if (!cinfo_prop) cinfo_prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CRYPT_INFO);

	if (pssh_data) gf_bs_reassign_buffer(ctx->bs_r, prop->value.data.ptr, prop->value.data.size);
	nb_pssh = pssh_data ? gf_bs_read_u32(ctx->bs_r) : 0;
	for (i = 0; i < nb_pssh; i++) {
		u8 cypherOffset;
		char *enc_data;
		bin128 cypherKey, cypherIV;
		GF_Crypt *mc;
		u32 pos, priv_len;
		char szSystemID[33];
		bin128 sysID;
		u32 j, kid_count;

		gf_bs_read_data(ctx->bs_r, sysID, 16);
		/*version =*/ gf_bs_read_u32(ctx->bs_r);
		kid_count = gf_bs_read_u32(ctx->bs_r);

		memset(szSystemID, 0, 33);
		for (j=0; j<16; j++) {
			sprintf(szSystemID+j*2, "%02X", (unsigned char) sysID[j]);
		}

		/*SystemID for GPAC Player: 67706163-6365-6E63-6472-6D746F6F6C31*/
		if (strcmp(szSystemID, "6770616363656E6364726D746F6F6C31")) {
			if (!cinfo_prop && !ctx->cfile) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[CENC/ISMA] System ID %s not supported\n", szSystemID));
			}
			gf_bs_skip_bytes(ctx->bs_r, kid_count*16);
			j=gf_bs_read_u32(ctx->bs_r);
			gf_bs_skip_bytes(ctx->bs_r, j);
			continue;
		}

		/*store key IDs*/
		cstr->KID_count = kid_count;
		pos = (u32) gf_bs_get_position(ctx->bs_r);
		cstr->KIDs = (bin128 *)gf_realloc(cstr->KIDs, cstr->KID_count*sizeof(bin128));
		cstr->keys = (bin128 *)gf_realloc(cstr->keys, cstr->KID_count*sizeof(bin128));

		memmove(cstr->KIDs, pssh_data + pos, cstr->KID_count*sizeof(bin128));
		gf_bs_skip_bytes(ctx->bs_r, cstr->KID_count*sizeof(bin128));
		priv_len = gf_bs_read_u32(ctx->bs_r);
		pos = (u32) gf_bs_get_position(ctx->bs_r);

		/*GPAC DRM TEST system info, used to validate cypher offset in CENC packager
			keyIDs as usual (before private data)
			URL len on 8 bits
			URL
			keys, cyphered with our magic key :)
		*/
		cypherOffset = pssh_data[pos] + 1;
		gf_bin128_parse("0x6770616363656E6364726D746F6F6C31", cypherKey);
		gf_bin128_parse("0x00000000000000000000000000000001", cypherIV);

		mc = gf_crypt_open(GF_AES_128, GF_CTR);
		if (!mc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open AES-128 CTR\n"));
			return GF_IO_ERR;
		}
		enc_data = gf_malloc(priv_len - cypherOffset);
		memcpy(enc_data, pssh_data + pos + cypherOffset, priv_len - cypherOffset);
		gf_crypt_init(mc, cypherKey, cypherIV);
		gf_crypt_decrypt(mc, enc_data, priv_len - cypherOffset);
		gf_crypt_close(mc);

		memmove(cstr->keys, enc_data, cstr->KID_count*sizeof(bin128));
		gf_bs_skip_bytes(ctx->bs_r, cstr->KID_count*sizeof(bin128));
		gf_free(enc_data);
		return GF_OK;
	}

	if (cinfo_prop) {
		cinfo = gf_crypt_info_load(prop->value.string);
		if (!cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Failed to open crypt info file %s\n", prop->value.string));
			return GF_BAD_PARAM;
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
			cstr->KID_count = tci->KID_count;
			cstr->KIDs = (bin128 *)gf_realloc(cstr->KIDs, tci->KID_count*sizeof(bin128));
			cstr->keys = (bin128 *)gf_realloc(cstr->keys, tci->KID_count*sizeof(bin128));
			for (i=0; i<tci->KID_count; i++) {
				memcpy(cstr->KIDs[i], tci->KIDs[i], sizeof(bin128));
				memcpy(cstr->keys[i], tci->keys[i], sizeof(bin128));
			}
			if (cinfo != ctx->cinfo)
				gf_crypt_info_del(cinfo);
			return GF_OK;
		}
		if (cinfo != ctx->cinfo)
			gf_crypt_info_del(cinfo);
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] No supported system ID, no key found\n"));
	return GF_NOT_SUPPORTED;
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
		cinfo = gf_crypt_info_load(prop->value.string);
		if (!cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Failed to open crypt info file %s\n", prop->value.string));
			return GF_BAD_PARAM;
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
			cstr->KID_count = tci->KID_count;
			cstr->KIDs = (bin128 *)gf_realloc(cstr->KIDs, tci->KID_count*sizeof(bin128));
			cstr->keys = (bin128 *)gf_realloc(cstr->keys, tci->KID_count*sizeof(bin128));
			for (i=0; i<tci->KID_count; i++) {
				memcpy(cstr->KIDs[i], tci->KIDs[i], sizeof(bin128));
				memcpy(cstr->keys[i], tci->keys[i], sizeof(bin128));
			}
			if (cinfo != ctx->cinfo)
				gf_crypt_info_del(cinfo);
			return GF_OK;
		}
		if (cinfo != ctx->cinfo)
			gf_crypt_info_del(cinfo);
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] No supported system ID, no key found\n"));
	return GF_NOT_SUPPORTED;
}

static GF_Err cenc_dec_access_cenc(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, Bool is_play)
{
	if (is_play) {
		if (cstr->state != DECRYPT_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!cstr->crypt);

		//if (!ctx->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//ctx->nb_allow_play--;

		/*open decrypter - we do NOT initialize decrypter; it wil be done when we decrypt the first crypted sample*/
		if (cstr->is_cenc)
			cstr->crypt = gf_crypt_open(GF_AES_128, GF_CTR);
		else
			cstr->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!cstr->crypt) return GF_IO_ERR;

		cstr->state = DECRYPT_STATE_PLAY;
		return GF_OK;
	} else {
		if (cstr->state != DECRYPT_STATE_PLAY) return GF_SERVICE_ERROR;
		if (cstr->crypt) gf_crypt_close(cstr->crypt);
		cstr->crypt = NULL;
		cstr->state = DECRYPT_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err cenc_dec_access_adobe(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, Bool is_play)
{
	if (is_play) {
		if (cstr->state != DECRYPT_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!cstr->crypt);

		cstr->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!cstr->crypt) return GF_IO_ERR;

		cstr->state = DECRYPT_STATE_PLAY;
		return GF_OK;
	} else {
		if (cstr->state != DECRYPT_STATE_PLAY) return GF_SERVICE_ERROR;
		if (cstr->crypt) gf_crypt_close(cstr->crypt);
		cstr->crypt = NULL;
		cstr->state = DECRYPT_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}


static GF_Err cenc_dec_process_cenc(GF_CENCDecCtx *ctx, GF_CENCDecStream *cstr, GF_FilterPacket *in_pck)
{
	GF_Err e;
	char IV[17];
	bin128 KID;
	u32 i, subsample_count;
	u32 data_size;
	u8 *out_data;
	const char *sai_payload=NULL;
	u32 saiz=0;
	u32 IV_size;
	Bool crypt_reinit = GF_FALSE;
	GF_FilterPacket *out_pck;
	const GF_PropertyValue *prop, *const_IV=NULL, *cbc_pattern=NULL;

	if (!cstr->crypt) return GF_SERVICE_ERROR;

	if (! gf_filter_pck_get_crypt_flags(in_pck)) {
		out_pck = gf_filter_pck_new_ref(cstr->opid, NULL, 0, in_pck);
		gf_filter_pck_merge_properties(in_pck, out_pck);
		gf_filter_pck_set_property(out_pck, GF_PROP_PCK_CENC_SAI, NULL);
		gf_filter_pck_set_crypt_flags(out_pck, 0);
		gf_filter_pck_send(out_pck);
		return GF_OK;
	}
	prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_KID);
	if (!prop) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Packet encrypted but no KID info\n" ) );
		return GF_SERVICE_ERROR;
	}
	memcpy(KID, prop->value.data.ptr, 16);

	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_CENC_SAI);
	if (prop) {
		sai_payload = prop->value.data.ptr;
		saiz = prop->value.data.size;
		gf_bs_reassign_buffer(ctx->bs_r, sai_payload, saiz);
	}

	gf_filter_pck_get_data(in_pck, &data_size);
	//CENC can use inplace processing for decryption
	out_pck = gf_filter_pck_new_clone(cstr->opid, in_pck, &out_data);
	if (!out_pck) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Failed to allocated/clone packet for decrypting payload\n" ) );
		gf_filter_pid_drop_packet(cstr->ipid);
		return GF_SERVICE_ERROR;
	}

	IV_size = 8;
	//memset to 0 in case we use <16 byte key
	memset(IV, 0, sizeof(char)*17);
	prop = NULL;
	if (sai_payload)
		prop = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CENC_IV_SIZE);

	if (!prop) {
		const_IV = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CENC_IV_CONST);
		IV_size = 0;
	} else {
		IV_size = prop->value.uint;
	}
	if (!sai_payload && !const_IV) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Packet encrypted but no SAI info nor constant IV\n" ) );
		return GF_SERVICE_ERROR;

	}
	
	cbc_pattern = gf_filter_pid_get_property(cstr->ipid, GF_PROP_PID_CENC_PATTERN);

	/*read sample auxiliary information from bitstream*/
	subsample_count = 0;
	if (sai_payload) {
		gf_bs_read_data(ctx->bs_r, (char *) IV, IV_size);

		if (gf_bs_available(ctx->bs_r))
			subsample_count = gf_bs_read_u16(ctx->bs_r);
	}

	if (const_IV) IV_size = const_IV->value.data.size;

	if (strncmp(cstr->kid, KID, 16)) {
		Bool found = GF_FALSE;
		memcpy(cstr->kid, KID, 16);
		for (i = 0; i < cstr->KID_count; i++) {
			if (!strncmp((const char *)KID, (const char *)cstr->KIDs[i], 16)) {
				memcpy(cstr->key, cstr->keys[i], 16);
				found = GF_TRUE;
				break;
			}
		}
		if (!found) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot locate key with given KID !\n" ));
			e = GF_SERVICE_ERROR;
			goto exit;
		}
		crypt_reinit = GF_TRUE;
	}

	if (crypt_reinit) {
		if (const_IV) {
			memcpy(IV, const_IV->value.data.ptr, const_IV->value.data.size);
		}
		e = gf_crypt_init(cstr->crypt, cstr->key, IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot initialize AES-128 AES-128 %s (%s)\n", cstr->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			e = GF_IO_ERR;
			goto exit;
		}
	} else {
		e = GF_OK;
		//always restore IV at begining of sample regardless of the mode (const IV or IV CBC or CTR
		if (!cstr->is_cbc) {
			memmove(&IV[1], &IV[0], sizeof(char) * IV_size);
			IV[0] = 0;	/*begin of counter*/
			e = gf_crypt_set_IV(cstr->crypt, IV, 17);
		} else {
		 	if (const_IV) {
				memcpy(IV, const_IV->value.data.ptr, const_IV->value.data.size);
			}
			e = gf_crypt_set_IV(cstr->crypt, IV, 16);
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot set key AES-128 %s (%s)\n", cstr->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			e = GF_IO_ERR;
			goto exit;
		}
	}

	//sub-sample encryption
	if (subsample_count) {
		u32 cur_pos = 0;

		while (cur_pos < data_size) {
			u32 bytes_clear_data, bytes_encrypted_data;
			if (subsample_count==0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Corrupted CENC sai, not enough subsamples described\n" ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			if (gf_bs_available(ctx->bs_r) < 6) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Corrupted CENC sai, not enough bytes in subsample info\n" ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			bytes_clear_data = gf_bs_read_u16(ctx->bs_r);
			bytes_encrypted_data = gf_bs_read_u32(ctx->bs_r);
			subsample_count--;

			if (const_IV) {
				memmove(IV, const_IV->value.data.ptr, const_IV->value.data.size);
				if (const_IV->value.data.size == 8)
					memset(IV+8, 0, sizeof(char)*8);
				gf_crypt_set_IV(cstr->crypt, IV, 16);
			}
			if (cur_pos + bytes_clear_data + bytes_encrypted_data > data_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Corrupted CENC sai, subsample info describe more bytes (%d) than in packet (%d)\n", cur_pos + bytes_clear_data + bytes_encrypted_data , data_size ));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			/*skip clear data*/
			cur_pos += bytes_clear_data;

			//pattern decryption
			if (cbc_pattern && cbc_pattern->value.frac.den && cbc_pattern->value.frac.num) {
				u32 pos = cur_pos;
				u32 res = bytes_encrypted_data;
				u32 skip_byte_block = cbc_pattern->value.frac.num;
				u32 crypt_byte_block = cbc_pattern->value.frac.den;

				if (cstr->is_cbc) {
					u32 clear_trailing = res % 16;
					res -= clear_trailing;
				}

				while (res) {
					gf_crypt_decrypt(cstr->crypt, out_data + pos, res >= (u32) (16* crypt_byte_block) ? 16* crypt_byte_block : res);
					if (res >= (u32) (16 * (crypt_byte_block + skip_byte_block))) {
						pos += 16 * (crypt_byte_block + skip_byte_block);
						res -= 16 * (crypt_byte_block + skip_byte_block);
					} else {
						res = 0;
					}
				}
			}
			//full subsample decryption
			else {
				gf_crypt_decrypt(cstr->crypt, out_data+cur_pos, bytes_encrypted_data);
			}
			cur_pos += bytes_encrypted_data;
		}
	}
	//full sample encryption
	else {
		if (cstr->is_cenc) {
			gf_crypt_decrypt(cstr->crypt, out_data, data_size);
		} else {
			u32 ret = data_size % 16;
			if (data_size >= 16) {
				gf_crypt_decrypt(cstr->crypt, out_data, data_size-ret);
			}
		}
	}

	gf_filter_pck_merge_properties(in_pck, out_pck);
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
	char IV[17];
	GF_Err e;
	u32 trim_bytes = 0;
	u32 offset, size;
	Bool encrypted_au;

	if (!cstr->crypt) return GF_SERVICE_ERROR;

	in_data = gf_filter_pck_get_data(in_pck, &data_size);
	out_pck = gf_filter_pck_new_alloc(cstr->opid, data_size, &out_data);

	memcpy(out_data, in_data, data_size);

	trim_bytes = 0;
	offset=0;
	size = data_size;
	encrypted_au = out_data[0] ? GF_TRUE : GF_FALSE;
	if (encrypted_au) {
		if (size<17) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Error in sample size, %d bytes remain but at least 17 are required\n", size ) );
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		memmove(IV, out_data+1, 16);
		if (!cstr->crypt_init) {
			e = gf_crypt_init(cstr->crypt, cstr->keys[0], IV);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot initialize AES-128 CBC (%s)\n", gf_error_to_string(e)) );
				return GF_IO_ERR;
			}
			cstr->crypt_init = GF_TRUE;
		} else {
			e = gf_crypt_set_IV(cstr->crypt, IV, GF_AES_128_KEYSIZE);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ADOBE] Cannot set state AES-128 CBC (%s)\n", gf_error_to_string(e)) );
				return GF_IO_ERR;
			}
		}
		offset += 17;
		size -= 17;

		gf_crypt_decrypt(cstr->crypt, out_data+offset, size);
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
	if (cstr->crypt) gf_crypt_close(cstr->crypt);
	if (cstr->KIDs) gf_free(cstr->KIDs);
	if (cstr->keys) gf_free(cstr->keys);


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
	GF_CENCDecStream *cstr;
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
	if (prop) scheme_type = prop->value.uint;

	if (is_remove) {
		cstr = gf_filter_pid_get_udta(pid);
		if (cstr->opid) gf_filter_pid_remove(cstr->opid);
		gf_list_del_item(ctx->streams, cstr);
		cenc_dec_stream_del(cstr);
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	cstr = gf_filter_pid_get_udta(pid);
	if (!cstr) {
		GF_SAFEALLOC(cstr, GF_CENCDecStream);
		cstr->opid = gf_filter_pid_new(filter);
		cstr->ipid = pid;
		gf_list_add(ctx->streams, cstr);
		gf_filter_pid_set_udta(pid, cstr);
		gf_filter_pid_set_udta(cstr->opid, cstr);
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_VERSION);
	if (prop) scheme_version = prop->value.uint;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_URI);
	if (prop) scheme_uri = prop->value.string;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_KMS_URI);
	if (prop) kms_uri = prop->value.string;

	switch (scheme_type) {
	case GF_ISOM_ISMACRYP_SCHEME:
		e = cenc_dec_setup_isma(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_OMADRM_SCHEME:
		e = cenc_dec_setup_oma(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_CENC_SCHEME:
	case GF_ISOM_CBC_SCHEME:
	case GF_ISOM_CENS_SCHEME:
	case GF_ISOM_CBCS_SCHEME:
		e = cenc_dec_setup_cenc(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_ADOBE_SCHEME:
		e = cenc_dec_setup_adobe(ctx, cstr, scheme_type, scheme_version, scheme_uri, kms_uri);
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
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_IV_SIZE, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_IV_CONST, NULL);
	gf_filter_pid_set_property(cstr->opid, GF_PROP_PID_CENC_PATTERN, NULL);

	cstr->is_nalu = GF_FALSE;;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (prop) {
		switch (prop->value.uint) {
		case GF_CODECID_AVC:
		case GF_CODECID_SVC:
		case GF_CODECID_MVC:
		case GF_CODECID_HEVC:
		case GF_CODECID_HEVC_TILES:
		case GF_CODECID_LHVC:
			cstr->is_nalu = GF_TRUE;;
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

		if (cstr->is_cenc || cstr->is_cbc) {
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
	ctx->streams = gf_list_new();
	if (!ctx->streams) return GF_OUT_OF_MEM;

	if (ctx->cfile) {
		ctx->cinfo = gf_crypt_info_load(ctx->cfile);
		if (!ctx->cinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENCCrypt] Cannot load config file %s\n", ctx->cfile ));
			return GF_BAD_PARAM;
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
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_ISMACRYP_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_OMADRM_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CENC_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CENS_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CBC_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_CBCS_SCHEME),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_PROTECTION_SCHEME_TYPE, GF_ISOM_ADOBE_SCHEME),

	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	//CAP_UINT(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_FALSE),
};


#define OFFS(_n)	#_n, offsetof(GF_CENCDecCtx, _n)
static const GF_FilterArgs GF_CENCDecArgs[] =
{
	{ OFFS(cfile), "crypt file location - see filter help", GF_PROP_STRING, NULL, NULL, 0},
	{0}
};

GF_FilterRegister CENCDecRegister = {
	.name = "cdcrypt",
	GF_FS_SET_DESCRIPTION("CENC decryptor")
	GF_FS_SET_HELP("The CENC decryptor supports decrypting CENC, ISMA and Adobe streams. It uses a configuration file for retrieving keys.\n"
	"The syntax is available at https://github.com/gpac/gpac/wiki/Common-Encryption\n"
	"The file can be set per PID using the property DecryptFile (highest priority), CryptFile (lower priority) "
	"or set at the filter level using [-cfile]() (lowest priority).\n"
	"When the file is set per PID, the first `CrypTrack` with the same ID is used, otherwise the first `CrypTrack` is used.")
	.private_size = sizeof(GF_CENCDecCtx),
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

const GF_FilterRegister *cenc_decrypt_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_CRYPTO
	return &CENCDecRegister;
#else
	return NULL;
#endif
}
