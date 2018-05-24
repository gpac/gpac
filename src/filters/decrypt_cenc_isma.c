/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR decoder module
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
#include <gpac/ismacryp.h>
#include <gpac/crypt.h>
#include <gpac/base_coding.h>
#include <gpac/download.h>

#ifndef GPAC_DISABLE_MCRYPT


enum
{
	ISMAEA_STATE_ERROR,
	ISMAEA_STATE_SETUP,
	ISMAEA_STATE_PLAY,
};

typedef struct
{
	GF_Crypt *crypt;
	char key[16], salt[8];
	u64 last_IV;
	u32 state;
	u32 nb_allow_play;
	Bool is_oma;
	u32 preview_range;
	/*for Common Encryption*/
	Bool is_cenc;
	Bool is_cbc;
	Bool first_crypted_samp;
	u32 KID_count;
	bin128 *KIDs;
	bin128 *keys;

	char *buffer;
	u32 buffer_alloc_size;

	GF_BitStream *plaintext_bs, *cyphertext_bs, *sai_bs;
	GF_CENCSampleAuxInfo sai;

	GF_DownloadManager *dm;
} GF_CENCDecCtx;


static void decenc_kms_netio(void *cbck, GF_NETIO_Parameter *par)
{
}

static GF_Err decenc_get_gpac_kms(GF_CENCDecCtx *ctx, GF_FilterPid *pid, const char *kms_url)
{
	const GF_PropertyValue *prop;
	GF_Err e;
	FILE *t;
	u32 id = 0;
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
		return gf_ismacryp_gpac_get_info(id, (char *)kms_url, ctx->key, ctx->salt);
	}
	/*note that gpac doesn't have TLS support -> not really usefull. As a general remark, ISMACryp
	is supported as a proof of concept, crypto and IPMP being the last priority on gpac...*/
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[CENC/ISMA] Fetching ISMACryp key for channel %d\n", id) );

	sess = gf_dm_sess_new(ctx->dm, kms_url, 0, decenc_kms_netio, ctx, NULL);
	if (!sess) return GF_IO_ERR;
	/*start our download (threaded)*/
	gf_dm_sess_process(sess);

	while (1) {
		e = gf_dm_sess_get_stats(sess, NULL, NULL, NULL, NULL, NULL, NULL);
		if (e) break;
	}
	if (e==GF_EOS) {
		e = gf_ismacryp_gpac_get_info(id, (char *) gf_dm_sess_get_cache_name(sess), ctx->key, ctx->salt);
	}
	gf_dm_sess_del(sess);
	return e;
}


static GF_Err decenc_setup_isma(GF_CENCDecCtx *ctx, GF_FilterPid *pid, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	GF_Err e;

	ctx->state = ISMAEA_STATE_ERROR;

	if (scheme_type != GF_ISOM_ISMACRYP_SCHEME) return GF_NOT_SUPPORTED;
	if (scheme_version != 1) return GF_NOT_SUPPORTED;
	if (!kms_uri) return GF_NON_COMPLIANT_BITSTREAM;

	/*try to fetch the keys*/
	/*base64 inband encoding*/
	if (!strnicmp(kms_uri, "(key)", 5)) {
		char data[100];
		gf_base64_decode((char*) kms_uri+5, (u32)strlen(kms_uri)-5, data, 100);
		memcpy(ctx->key, data, sizeof(char)*16);
		memcpy(ctx->salt, data+16, sizeof(char)*8);
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
			ctx->key[i] = v;
		}

		k = (char *)kms_uri + 10 + 32;
		for (i=0; i<8; i++) {
			szT[0] = k[2*i];
			szT[1] = k[2*i + 1];
			sscanf(szT, "%X", &v);
			ctx->salt[i] = v;
		}
	}
	/*MPEG4-IP KMS*/
	else if (!stricmp(kms_uri, "AudioKey") || !stricmp(kms_uri, "VideoKey")) {
		if (!gf_ismacryp_mpeg4ip_get_info((char *) kms_uri, ctx->key, ctx->salt)) {
			return GF_BAD_PARAM;
		}
	}
	/*gpac default scheme is used, fetch file from KMS and load keys*/
	else if (scheme_uri && !stricmp(scheme_uri, "urn:gpac:isma:encryption_scheme")) {
		e = decenc_get_gpac_kms(ctx, pid, kms_uri);
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
		memcpy(ctx->salt, mysalt, sizeof(char)*8);
		memcpy(ctx->key, mykey, sizeof(char)*16);
	}
	ctx->state = ISMAEA_STATE_SETUP;
	//ctx->nb_allow_play = 1;
	return GF_OK;
}

static GF_Err decenc_access_isma(GF_CENCDecCtx *ctx, Bool is_play)
{
	GF_Err e;
	char IV[16];

	if (is_play) {
		if (ctx->state != ISMAEA_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!ctx->crypt);

		//if (!ctx->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//ctx->nb_allow_play--;

		/*init decrypter*/
		ctx->crypt = gf_crypt_open(GF_AES_128, GF_CTR);
		if (!ctx->crypt) return GF_IO_ERR;

		memset(IV, 0, sizeof(char)*16);
		memcpy(IV, ctx->salt, sizeof(char)*8);
		e = gf_crypt_init(ctx->crypt, ctx->key, IV);
		if (e) return e;

		ctx->state = ISMAEA_STATE_PLAY;
		return GF_OK;
	} else {
		if (ctx->state != ISMAEA_STATE_PLAY) return GF_SERVICE_ERROR;
		if (ctx->crypt) gf_crypt_close(ctx->crypt);
		ctx->crypt = NULL;
		ctx->state = ISMAEA_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err decenc_process_isma(GF_CENCDecCtx *ctx, GF_FilterPid *ipid, GF_FilterPid *opid)
{
	u32 data_size;
	const char *in_data;
	char *out_data;
	u64 isma_BSO = 0;
	GF_FilterPacket *in_pck, *out_pck;
	const GF_PropertyValue *prop;
	if (!ctx->crypt) return GF_SERVICE_ERROR;

	in_pck = gf_filter_pid_get_packet(ipid);
	if (!in_pck) return GF_OK;

	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_ENCRYPTED);
	if (prop && !prop->value.boolean) {
		gf_filter_pck_forward(in_pck, opid);
		gf_filter_pid_drop_packet(ipid);
		return GF_OK;
	}
	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_ISMA_BSO);
	if (prop) isma_BSO = prop->value.longuint;

	/*resync IV*/
	if (!ctx->last_IV || (ctx->last_IV != isma_BSO)) {
		char IV[17];
		u64 count;
		u32 remain;
		GF_BitStream *bs;
		count = isma_BSO / 16;
		remain = (u32) (isma_BSO % 16);

		/*format IV to begin of counter*/
		bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, 0);	/*begin of counter*/
		gf_bs_write_data(bs, ctx->salt, 8);
		gf_bs_write_u64(bs, (s64) count);
		gf_bs_del(bs);
		gf_crypt_set_IV(ctx->crypt, IV, 17);

		/*decrypt remain bytes*/
		if (remain) {
			char dummy[20];
			gf_crypt_decrypt(ctx->crypt, dummy, remain);
		}
		ctx->last_IV = isma_BSO;
	}

	in_data = gf_filter_pck_get_data(in_pck, &data_size);
	out_pck = gf_filter_pck_new_alloc(opid, data_size, &out_data);

	memcpy(out_data, in_data, data_size);
	/*decrypt*/
	gf_crypt_decrypt(ctx->crypt, out_data, data_size);
	ctx->last_IV += data_size;

	gf_filter_pck_merge_properties(in_pck, out_pck);
	gf_filter_pid_drop_packet(ipid);
	gf_filter_pck_send(out_pck);

	return GF_OK;
}

static GF_Err decenc_setup_oma(GF_CENCDecCtx *ctx, GF_FilterPid *pid, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	const GF_PropertyValue *prop;
	ctx->state = ISMAEA_STATE_ERROR;
	if (scheme_type != GF_ISOM_OMADRM_SCHEME) return GF_NOT_SUPPORTED;
	if (scheme_version != 0x00000200) return GF_NOT_SUPPORTED;

	ctx->is_oma = GF_TRUE;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OMA_PREVIEW_RANGE);
	if (prop) ctx->preview_range = (u32) prop->value.longuint;

	/*TODO: call DRM agent, fetch keys*/
	if (!kms_uri) return GF_NON_COMPLIANT_BITSTREAM;
	ctx->state = ISMAEA_STATE_SETUP;

	/*we have preview*/
	if (ctx->preview_range) return GF_OK;

	return GF_NOT_SUPPORTED;
}

static GF_Err decenc_setup_cenc(GF_CENCDecCtx *ctx, GF_FilterPid *pid, u32 scheme_type, u32 scheme_version, const char *scheme_uri, const char *kms_uri)
{
	u32 i, nb_pssh;
	GF_BitStream *bs=NULL;
	const GF_PropertyValue *prop;
	char *pssh_data;
	Bool is_playing = (ctx->state == ISMAEA_STATE_PLAY) ? GF_TRUE : GF_FALSE;

	ctx->state = ISMAEA_STATE_ERROR;

	if ((scheme_type != GF_ISOM_CENC_SCHEME) && (scheme_type != GF_ISOM_CBC_SCHEME) && (scheme_type != GF_ISOM_CENS_SCHEME) && (scheme_type != GF_ISOM_CBCS_SCHEME))
		return GF_NOT_SUPPORTED;
	if (scheme_version != 0x00010000) return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_PSSH);
	if (prop) {
		pssh_data = (char *) prop->value.data.ptr;
		bs = gf_bs_new(prop->value.data.ptr, prop->value.data.size, GF_BITSTREAM_READ);
	}

	if ((scheme_type == GF_ISOM_CENC_SCHEME) || (scheme_type == GF_ISOM_CENS_SCHEME))
		ctx->is_cenc = GF_TRUE;
	else
		ctx->is_cbc = GF_TRUE;

	ctx->state = is_playing ? ISMAEA_STATE_PLAY : ISMAEA_STATE_SETUP;
	//ctx->nb_allow_play = 1;

	if (!bs) return GF_OK;
	nb_pssh = gf_bs_read_u32(bs);
	for (i = 0; i < nb_pssh; i++) {
		char szSystemID[33];
		bin128 sysID;
		u32 j, kid_count;

		gf_bs_read_data(bs, sysID, 16);
		kid_count = gf_bs_read_u32(bs);

		memset(szSystemID, 0, 33);
		for (j=0; j<16; j++) {
			sprintf(szSystemID+j*2, "%02X", (unsigned char) sysID[j]);
		}

		/*SystemID for GPAC Player: 67706163-6365-6E63-6472-6D746F6F6C31*/
		if (strcmp(szSystemID, "6770616363656E6364726D746F6F6C31")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] System ID %s not supported\n", szSystemID));
			gf_bs_skip_bytes(bs, kid_count*16);
			j=gf_bs_read_u32(bs);
			gf_bs_skip_bytes(bs, j);
			continue;
		}
		else {
			u8 cypherOffset;
			bin128 cypherKey, cypherIV;
			GF_Crypt *mc;
			u32 pos, priv_len;

			/*store key IDs*/
			ctx->KID_count = kid_count;
			pos = (u32) gf_bs_get_position(bs);
			ctx->KIDs = (bin128 *)gf_realloc(ctx->KIDs, ctx->KID_count*sizeof(bin128));
			ctx->keys = (bin128 *)gf_realloc(ctx->keys, ctx->KID_count*sizeof(bin128));

			memmove(ctx->KIDs, pssh_data + pos, ctx->KID_count*sizeof(bin128));
			gf_bs_skip_bytes(bs, ctx->KID_count*sizeof(bin128));
			priv_len = gf_bs_read_u32(bs);
			pos = (u32) gf_bs_get_position(bs);

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
			gf_crypt_init(mc, cypherKey, cypherIV);
			gf_crypt_decrypt(mc, pssh_data + pos + cypherOffset, priv_len - cypherOffset);
			gf_crypt_close(mc);

			memmove(ctx->keys, pssh_data + pos + cypherOffset, ctx->KID_count*sizeof(bin128));
			gf_bs_skip_bytes(bs, ctx->KID_count*sizeof(bin128));
		}
	}
	gf_bs_del(bs);
	return GF_OK;
}

static GF_Err decenc_access_cenc(GF_CENCDecCtx *ctx, Bool is_play)
{
	if (is_play) {
		if (ctx->state != ISMAEA_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!ctx->crypt);

		//if (!ctx->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//ctx->nb_allow_play--;

		/*open decrypter - we do NOT initialize decrypter; it wil be done when we decrypt the first crypted sample*/
		if (ctx->is_cenc)
			ctx->crypt = gf_crypt_open(GF_AES_128, GF_CTR);
		else
			ctx->crypt = gf_crypt_open(GF_AES_128, GF_CBC);
		if (!ctx->crypt) return GF_IO_ERR;

		ctx->first_crypted_samp = GF_TRUE;

		ctx->state = ISMAEA_STATE_PLAY;
		return GF_OK;
	} else {
		if (ctx->state != ISMAEA_STATE_PLAY) return GF_SERVICE_ERROR;
		if (ctx->crypt) gf_crypt_close(ctx->crypt);
		ctx->crypt = NULL;
		ctx->state = ISMAEA_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static Bool cenc_prop_filter(void *cbk, u32 prop_4cc, const char *prop_name, const GF_PropertyValue *src_prop)
{
	switch (prop_4cc) {
	case GF_PROP_PCK_ENCRYPTED:
	case GF_PROP_PCK_CENC_SAI:
	case GF_PROP_PID_PCK_CENC_IV_SIZE:
	case GF_PROP_PID_PCK_CENC_IV_CONST:
	case GF_PROP_PID_PCK_CENC_PATTERN:
		return GF_FALSE;
	default:
		return GF_TRUE;
	}
}

static GF_Err decenc_process_cenc(GF_CENCDecCtx *ctx, GF_FilterPid *ipid, GF_FilterPid *opid)
{
	GF_Err e;
	char IV[17];
	bin128 KID;
	u32 i, subsample_idx, subsample_count, max_size;
	u32 data_size;
	const char *in_data;
	char *out_data;
	const char *sai_payload=NULL;
	u32 saiz=0;
	GF_FilterPacket *in_pck, *out_pck;
	const GF_PropertyValue *prop, *const_IV=NULL, *cbc_pattern=NULL;

	if (!ctx->crypt) return GF_SERVICE_ERROR;

	in_pck = gf_filter_pid_get_packet(ipid);
	if (!in_pck) return GF_OK;

	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_ENCRYPTED);
	if (prop && !prop->value.boolean) {
		gf_filter_pck_forward(in_pck, opid);
		gf_filter_pid_drop_packet(ipid);
		return GF_OK;
	}
	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PCK_CENC_SAI);
	if (!prop) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Packet encrypted but no SAI info\n" ) );
		return GF_SERVICE_ERROR;
	}
	sai_payload = prop->value.data.ptr;
	saiz = prop->value.data.size;

	ctx->sai.IV_size = 0;
	memset(&ctx->sai.IV, 0, sizeof(bin128));

	in_data = gf_filter_pck_get_data(in_pck, &data_size);
	out_pck = gf_filter_pck_new_alloc(opid, data_size, &out_data);

	if (!ctx->cyphertext_bs)
		ctx->cyphertext_bs = gf_bs_new(in_data, data_size, GF_BITSTREAM_READ);
	else
		gf_bs_reassign_buffer(ctx->cyphertext_bs, in_data, data_size);

	if (!ctx->sai_bs)
		ctx->sai_bs = gf_bs_new(sai_payload, saiz, GF_BITSTREAM_READ);
	else
		gf_bs_reassign_buffer(ctx->sai_bs, sai_payload, saiz);

	if (!ctx->plaintext_bs)
		ctx->plaintext_bs = gf_bs_new(out_data, data_size, GF_BITSTREAM_WRITE);
	else
		gf_bs_reassign_buffer(ctx->plaintext_bs, out_data, data_size);

	if (!ctx->buffer) {
		ctx->buffer_alloc_size = 4096;
		ctx->buffer = (char*)gf_malloc(sizeof(char) * 4096);
	}

	ctx->sai.IV_size = 8;
	prop = gf_filter_pck_get_property(in_pck, GF_PROP_PID_PCK_CENC_IV_SIZE);
	if (!prop) {
		const_IV = gf_filter_pid_get_property(ipid, GF_PROP_PID_PCK_CENC_IV_CONST);
	} else {
		ctx->sai.IV_size = prop->value.uint;
	}
	
	cbc_pattern = gf_filter_pid_get_property(ipid, GF_PROP_PID_PCK_CENC_PATTERN);


	/*read sample auxiliary information from bitstream*/
	gf_bs_read_data(ctx->sai_bs,  (char *)KID, 16);
	gf_bs_read_data(ctx->sai_bs, (char *)ctx->sai.IV, ctx->sai.IV_size);
	subsample_count = gf_bs_read_u16(ctx->sai_bs);
	if (ctx->sai.subsample_count < subsample_count) {
		ctx->sai.subsamples = (GF_CENCSubSampleEntry *)gf_realloc(ctx->sai.subsamples, subsample_count*sizeof(GF_CENCSubSampleEntry));
		ctx->sai.subsample_count = subsample_count;
	}
	max_size = 0;
	for (i = 0; i < subsample_count; i++) {
		ctx->sai.subsamples[i].bytes_clear_data = gf_bs_read_u16(ctx->sai_bs);
		ctx->sai.subsamples[i].bytes_encrypted_data = gf_bs_read_u32(ctx->sai_bs);


		if (ctx->sai.subsamples[i].bytes_clear_data > max_size)	max_size = ctx->sai.subsamples[i].bytes_clear_data;
		if (ctx->sai.subsamples[i].bytes_encrypted_data > max_size) max_size = ctx->sai.subsamples[i].bytes_encrypted_data;
	}
	if (!subsample_count) max_size = data_size;

	if (max_size >  ctx->buffer_alloc_size) {
		ctx->buffer = (char*)gf_realloc(ctx->buffer, sizeof(char)*max_size);
		ctx->buffer_alloc_size = max_size;
	}

	for (i = 0; i < ctx->KID_count; i++) {
		if (!strncmp((const char *)KID, (const char *)ctx->KIDs[i], 16)) {
			memmove(ctx->key, ctx->keys[i], 16);
			break;
		}
	}

	if (ctx->first_crypted_samp) {
		if (!const_IV) {
			memmove(IV, ctx->sai.IV, ctx->sai.IV_size);
			if (ctx->sai.IV_size == 8)
				memset(IV+8, 0, sizeof(char)*8);
		} else {
			memmove(IV, const_IV->value.data.ptr, const_IV->value.data.size);
			if (const_IV->value.data.size == 8)
				memset(IV+8, 0, sizeof(char)*8);
		}
		e = gf_crypt_init(ctx->crypt, ctx->key, IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot initialize AES-128 AES-128 %s (%s)\n", ctx->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			e = GF_IO_ERR;
			goto exit;
		}
		ctx->first_crypted_samp = GF_FALSE;
	} else {
		if (ctx->is_cenc) {
			memset(IV, 0, sizeof(char)*17);

			IV[0] = 0;	/*begin of counter*/
			memcpy(&IV[1], (char *) ctx->sai.IV, sizeof(char)*ctx->sai.IV_size);
//			if (ctx->sai.IV_size == 8)	/*0-padded if IV_size == 8*/
			gf_crypt_set_IV(ctx->crypt, IV, 17);
		}
		e = gf_crypt_set_key(ctx->crypt, ctx->key);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot set key AES-128 %s (%s)\n", ctx->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			e = GF_IO_ERR;
			goto exit;
		}
	}

	//sub-sample encryption
	if (subsample_count) {
		subsample_idx = 0;
		while (gf_bs_available(ctx->cyphertext_bs)) {
			if (subsample_idx >= subsample_count)
				break;
			if (const_IV) {
				memmove(IV, const_IV->value.data.ptr, const_IV->value.data.size);
				if (const_IV->value.data.size == 8)
					memset(IV+8, 0, sizeof(char)*8);
				gf_crypt_set_IV(ctx->crypt, IV, 16);
			}

			/*read clear data and write it to plaintext_bs bitstream*/
			gf_bs_read_data(ctx->cyphertext_bs, ctx->buffer, ctx->sai.subsamples[subsample_idx].bytes_clear_data);
			gf_bs_write_data(ctx->plaintext_bs, ctx->buffer, ctx->sai.subsamples[subsample_idx].bytes_clear_data);

			/*now read encrypted data, decrypted it and write to plaintext_bs bitstream*/
			gf_bs_read_data(ctx->cyphertext_bs, ctx->buffer, ctx->sai.subsamples[subsample_idx].bytes_encrypted_data);
			//pattern decryption
			if (cbc_pattern && cbc_pattern->value.frac.den && cbc_pattern->value.frac.num) {
				u32 pos = 0;
				u32 res = ctx->sai.subsamples[subsample_idx].bytes_encrypted_data;
				u32 skip_byte_block = cbc_pattern->value.frac.num;
				u32 crypt_byte_block = cbc_pattern->value.frac.den;
				while (res) {
					gf_crypt_decrypt(ctx->crypt, ctx->buffer+pos, res >= (u32) (16* crypt_byte_block) ? 16* crypt_byte_block : res);
					if (res >= (u32) (16 * (crypt_byte_block + skip_byte_block))) {
						pos += 16 * (crypt_byte_block + skip_byte_block);
						res -= 16 * (crypt_byte_block + skip_byte_block);
					} else {
						res = 0;
					}
			}
			} else {
				gf_crypt_decrypt(ctx->crypt, ctx->buffer, ctx->sai.subsamples[subsample_idx].bytes_encrypted_data);
			}
			gf_bs_write_data(ctx->plaintext_bs, ctx->buffer, ctx->sai.subsamples[subsample_idx].bytes_encrypted_data);

			subsample_idx++;
		}
	}
	//full sample encryption
	else {
		gf_bs_read_data(ctx->cyphertext_bs, ctx->buffer, data_size);
		if (ctx->is_cenc) {
			gf_crypt_decrypt(ctx->crypt, ctx->buffer, data_size);
		} else {
			u32 ret = data_size % 16;
			if (data_size >= 16) {
				gf_crypt_decrypt(ctx->crypt, ctx->buffer, data_size-ret);
			}
		}
	}

	gf_filter_pck_merge_properties_filter(in_pck, out_pck, cenc_prop_filter, NULL);
	gf_filter_pck_send(out_pck);

exit:

	if (e && out_pck) {
		gf_filter_pck_discard(out_pck);
	}
	gf_filter_pid_drop_packet(ipid);
	return e;
}


static GF_Err decenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_Err e = GF_OK;
	u32 scheme_type = 0;
	u32 scheme_version = 0;
	const char *scheme_uri = NULL;
	const char *kms_uri = NULL;
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
	if (prop) scheme_type = prop->value.uint;

	GF_FilterPid *opid;
	if (is_remove) {
		opid = gf_filter_pid_get_udta(pid);
		assert(opid);
		gf_filter_pid_remove(opid);
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	opid = gf_filter_pid_get_udta(pid);
	if (!opid) {
		opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(pid, opid);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(opid, pid);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_VERSION);
	if (prop) scheme_version = prop->value.uint;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_URI);
	if (prop) scheme_uri = prop->value.string;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_KMS_URI);
	if (prop) kms_uri = prop->value.string;

	switch (scheme_type) {
	case GF_ISOM_ISMA_SCHEME:
		e = decenc_setup_isma(ctx, pid, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_ODRM_SCHEME:
		e = decenc_setup_oma(ctx, pid, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	case GF_ISOM_CENC_SCHEME:
	case GF_ISOM_CBC_SCHEME:
	case GF_ISOM_CENS_SCHEME:
	case GF_ISOM_CBCS_SCHEME:
		e = decenc_setup_cenc(ctx, pid, scheme_type, scheme_version, scheme_uri, kms_uri);
		break;
	default:
		e = GF_SERVICE_ERROR;
		break;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
	if (prop) {
		gf_filter_pid_set_property(opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(prop->value.uint) );
		gf_filter_pid_set_property(opid, GF_PROP_PID_ORIG_STREAM_TYPE, NULL);
	}
	//remove all cenc properties on output
	gf_filter_pid_set_property(opid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, NULL);
	gf_filter_pid_set_property(opid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, NULL);
	gf_filter_pid_set_property(opid, GF_PROP_PID_PROTECTION_SCHEME_URI, NULL);
	gf_filter_pid_set_property(opid, GF_PROP_PID_PROTECTION_KMS_URI, NULL);
	gf_filter_pid_set_property(opid, GF_PROP_PID_OMA_PREVIEW_RANGE, NULL);
	gf_filter_pid_set_property(opid, GF_PROP_PID_CENC_PSSH, NULL);

	return e;
}


static Bool decenc_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterPid *ipid;
	Bool is_play = GF_FALSE;
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);

	ipid = evt->base.on_pid;
	if (!ipid) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		is_play = GF_TRUE;
	case GF_FEVT_STOP:
		if (ctx->is_cenc || ctx->is_cbc) {
			decenc_access_cenc(ctx, is_play);
		} else if (ctx->is_oma) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[CENC/ISMA] OMA not supported, canceling filter event\n") );
			return GF_TRUE;
		} else {
			decenc_access_isma(ctx, is_play);
		}
		break;
	default:
		break;
	}
	return GF_FALSE;
}
static GF_Err decenc_process(GF_Filter *filter)
{
	u32 i, count = gf_filter_get_ipid_count(filter);
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);

	for (i=0; i<count; i++) {
		GF_FilterPid *opid;
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		if (!ipid) continue;
		opid = gf_filter_pid_get_udta(ipid);

		if (ctx->is_cenc || ctx->is_cbc) {
			return decenc_process_cenc(ctx, ipid, opid);
		} else if (ctx->is_oma) {
			return GF_NOT_SUPPORTED;
		}
		return decenc_process_isma(ctx, ipid, opid);
	}
	return GF_OK;
}

static void decenc_finalize(GF_Filter *filter)
{
	GF_CENCDecCtx *ctx = (GF_CENCDecCtx *)gf_filter_get_udta(filter);
	/*in case something went wrong*/
	if (ctx->crypt) gf_crypt_close(ctx->crypt);
	if (ctx->KIDs) gf_free(ctx->KIDs);
	if (ctx->keys) gf_free(ctx->keys);

	if (ctx->buffer) gf_free(ctx->buffer);
	if (ctx->plaintext_bs) gf_bs_del(ctx->plaintext_bs);
	if (ctx->sai_bs) gf_bs_del(ctx->sai_bs);
	if (ctx->cyphertext_bs) gf_bs_del(ctx->cyphertext_bs);
	if (ctx->sai.subsamples) gf_free(ctx->sai.subsamples);
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

	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_ENCRYPTED),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_FALSE),
};

GF_FilterRegister CENCDecRegister = {
	.name = "cencdecrypt",
	.description = "CENC and ISMA decryptor",
	.private_size = sizeof(GF_CENCDecCtx),
	SETCAPS(CENCDecCaps),
	.configure_pid = decenc_configure_pid,
	.finalize = decenc_finalize,
	.process = decenc_process,
	.process_event = decenc_process_event
	//for now only one PID per CENC decryptor instance, could be further optimized
};

#endif /*GPAC_DISABLE_MCRYPT*/

const GF_FilterRegister *decrypt_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_MCRYPT
	return &CENCDecRegister;
#else
	return NULL;
#endif
}
