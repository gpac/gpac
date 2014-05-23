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

#include <gpac/ismacryp.h>
#include <gpac/crypt.h>
#include <gpac/base_coding.h>
#include <gpac/download.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/modules/ipmp.h>

#ifndef GPAC_DISABLE_MCRYPT

#define OMA_DRM_MP4MC


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
	/*for Common Enxryption*/
	Bool is_cenc;
	Bool is_cbc;
	Bool first_crypted_samp;
	u32 KID_count;
	bin128 *KIDs;
	bin128 *keys;
} ISMAEAPriv;


static void ISMA_KMS_NetIO(void *cbck, GF_NETIO_Parameter *par)
{
}

static GF_Err ISMA_GetGPAC_KMS(ISMAEAPriv *priv, GF_Channel *ch, const char *kms_url)
{
	GF_Err e;
	FILE *t;
	GF_DownloadSession * sess;
	if (!strnicmp(kms_url, "(ipmp)", 6)) return GF_NOT_SUPPORTED;
	else if (!strnicmp(kms_url, "(uri)", 5)) kms_url += 5;
	else if (!strnicmp(kms_url, "file://", 7)) kms_url += 7;

	e = GF_OK;
	/*try local*/
	t = (strstr(kms_url, "://") == NULL) ? gf_f64_open(kms_url, "r") : NULL;
	if (t) {
		fclose(t);
		return gf_ismacryp_gpac_get_info(ch->esd->ESID, (char *)kms_url, priv->key, priv->salt);
	}
	/*note that gpac doesn't have TLS support -> not really usefull. As a general remark, ISMACryp
	is supported as a proof of concept, crypto and IPMP being the last priority on gpac...*/
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[CENC/ISMA] Fetching ISMACryp key for channel %d\n", ch->esd->ESID) );

	sess = gf_term_download_new(ch->service, kms_url, 0, ISMA_KMS_NetIO, ch);
	if (!sess) return GF_IO_ERR;
	/*start our download (threaded)*/
	gf_dm_sess_process(sess);

	while (1) {
		e = gf_dm_sess_get_stats(sess, NULL, NULL, NULL, NULL, NULL, NULL);
		if (e) break;
	}
	if (e==GF_EOS) {
		e = gf_ismacryp_gpac_get_info(ch->esd->ESID, (char *) gf_dm_sess_get_cache_name(sess), priv->key, priv->salt);
	}
	gf_term_download_del(sess);
	return e;
}


static GF_Err ISMA_Setup(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	GF_Err e;
	GF_ISMACrypConfig *cfg = (GF_ISMACrypConfig*)evt->config_data;

	priv->state = ISMAEA_STATE_ERROR;

	if (cfg->scheme_type != GF_4CC('i','A','E','C')) return GF_NOT_SUPPORTED;
	if (cfg->scheme_version != 1) return GF_NOT_SUPPORTED;

	if (!cfg->kms_uri) return GF_NON_COMPLIANT_BITSTREAM;

	/*try to fetch the keys*/
	/*base64 inband encoding*/
	if (!strnicmp(cfg->kms_uri, "(key)", 5)) {
		char data[100];
		gf_base64_decode((char*)cfg->kms_uri+5, (u32)strlen(cfg->kms_uri)-5, data, 100);
		memcpy(priv->key, data, sizeof(char)*16);
		memcpy(priv->salt, data+16, sizeof(char)*8);
	}
	/*hexadecimal inband encoding*/
	else if (!strnicmp(cfg->kms_uri, "(key-hexa)", 10)) {
		u32 v;
		char szT[3], *k;
		u32 i;
		szT[2] = 0;
		if (strlen(cfg->kms_uri) < 10+32+16) return GF_NON_COMPLIANT_BITSTREAM;

		k = (char *)cfg->kms_uri + 10;
		for (i=0; i<16; i++) {
			szT[0] = k[2*i];
			szT[1] = k[2*i + 1];
			sscanf(szT, "%X", &v);
			priv->key[i] = v;
		}

		k = (char *)cfg->kms_uri + 10 + 32;
		for (i=0; i<8; i++) {
			szT[0] = k[2*i];
			szT[1] = k[2*i + 1];
			sscanf(szT, "%X", &v);
			priv->salt[i] = v;
		}
	}
	/*MPEG4-IP KMS*/
	else if (!stricmp(cfg->kms_uri, "AudioKey") || !stricmp(cfg->kms_uri, "VideoKey")) {
		if (!gf_ismacryp_mpeg4ip_get_info((char *) cfg->kms_uri, priv->key, priv->salt)) {
			return GF_BAD_PARAM;
		}
	}
	/*gpac default scheme is used, fetch file from KMS and load keys*/
	else if (cfg->scheme_uri && !stricmp(cfg->scheme_uri, "urn:gpac:isma:encryption_scheme")) {
		e = ISMA_GetGPAC_KMS(priv, evt->channel, cfg->kms_uri);
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
		memcpy(priv->salt, mysalt, sizeof(char)*8);
		memcpy(priv->key, mykey, sizeof(char)*16);
	}
	priv->state = ISMAEA_STATE_SETUP;
	//priv->nb_allow_play = 1;
	return GF_OK;
}

static GF_Err ISMA_Access(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	GF_Err e;
	char IV[16];

	if (evt->event_type==GF_IPMP_TOOL_GRANT_ACCESS) {
		if (priv->state != ISMAEA_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!priv->crypt);

		//if (!priv->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//priv->nb_allow_play--;

		/*init decrypter*/
		priv->crypt = gf_crypt_open("AES-128", "CTR");
		if (!priv->crypt) return GF_IO_ERR;

		memset(IV, 0, sizeof(char)*16);
		memcpy(IV, priv->salt, sizeof(char)*8);
		e = gf_crypt_init(priv->crypt, priv->key, 16, IV);
		if (e) return e;

		priv->state = ISMAEA_STATE_PLAY;
		return GF_OK;
	}
	if (evt->event_type==GF_IPMP_TOOL_RELEASE_ACCESS) {
		if (priv->state != ISMAEA_STATE_PLAY) return GF_SERVICE_ERROR;
		if (priv->crypt) gf_crypt_close(priv->crypt);
		priv->crypt = NULL;
		priv->state = ISMAEA_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err ISMA_ProcessData(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	if (!priv->crypt) return GF_SERVICE_ERROR;

	if (!evt->is_encrypted) return GF_OK;

	/*resync IV*/
	if (!priv->last_IV || (priv->last_IV != evt->isma_BSO)) {
		char IV[17];
		u64 count;
		u32 remain;
		GF_BitStream *bs;
		count = evt->isma_BSO / 16;
		remain = (u32) (evt->isma_BSO % 16);

		/*format IV to begin of counter*/
		bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, 0);	/*begin of counter*/
		gf_bs_write_data(bs, priv->salt, 8);
		gf_bs_write_u64(bs, (s64) count);
		gf_bs_del(bs);
		gf_crypt_set_state(priv->crypt, IV, 17);

		/*decrypt remain bytes*/
		if (remain) {
			char dummy[20];
			gf_crypt_decrypt(priv->crypt, dummy, remain);
		}
		priv->last_IV = evt->isma_BSO;
	}
	/*decrypt*/
	gf_crypt_decrypt(priv->crypt, evt->data, evt->data_size);
	priv->last_IV += evt->data_size;
	return GF_OK;
}


#ifdef OMA_DRM_MP4MC
static GF_Err OMA_DRM_Setup(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	u32 hdr_pos;
	GF_OMADRM2Config *cfg = (GF_OMADRM2Config*)evt->config_data;

	priv->state = ISMAEA_STATE_ERROR;

	if (cfg->scheme_type != GF_4CC('o','d','k','m')) return GF_NOT_SUPPORTED;
	if (cfg->scheme_version != 0x00000200) return GF_NOT_SUPPORTED;

	hdr_pos = 0;
	while (hdr_pos<cfg->oma_drm_textual_headers_len) {
		u32 len;
		char *sep;
		if (!strncmp(cfg->oma_drm_textual_headers + hdr_pos, "PreviewRange", 12)) {
			sep = strchr(cfg->oma_drm_textual_headers + hdr_pos, ':');
			if (sep) priv->preview_range = atoi(sep+1);
		}
		len = (u32) strlen(cfg->oma_drm_textual_headers + hdr_pos);
		hdr_pos += len+1;
	}
	priv->is_oma = GF_TRUE;

	/*TODO: call DRM agent, fetch keys*/
	if (!cfg->kms_uri) return GF_NON_COMPLIANT_BITSTREAM;
	priv->state = ISMAEA_STATE_SETUP;
	//priv->nb_allow_play = 1;

	/*we have preview*/
	if (priv->preview_range) return GF_OK;
	return GF_NOT_SUPPORTED;
}
#endif

static GF_Err CENC_Setup(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	GF_CENCConfig *cfg = (GF_CENCConfig*)evt->config_data;
	u32 i;

	priv->state = ISMAEA_STATE_ERROR;

	if ((cfg->scheme_type != GF_4CC('c', 'e', 'n', 'c')) && (cfg->scheme_type != GF_4CC('c','b','c','1'))) return GF_NOT_SUPPORTED;
	if (cfg->scheme_version != 0x00010000) return GF_NOT_SUPPORTED;

	for (i = 0; i < cfg->PSSH_count; i++) {
		GF_NetComDRMConfigPSSH *pssh = &cfg->PSSHs[i];
		char szSystemID[33];
		u32 j;

		memset(szSystemID, 0, 33);
		for (j=0; j<16; j++) {
			sprintf(szSystemID+j*2, "%02X", (unsigned char) pssh->SystemID[j]);
		}

		/*SystemID for GPAC Player: 67706163-6365-6E63-6472-6D746F6F6C31*/
		if (strcmp(szSystemID, "6770616363656E6364726D746F6F6C31")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[CENC/ISMA] System ID %s not supported\n", szSystemID));
			continue;
		}
		else {
			u8 cypherOffset;
			bin128 cypherKey, cypherIV;
			GF_Crypt *mc;

			/*GPAC DRM TEST system info, used to validate cypher offset in CENC packager
				keyIDs as usual (before private data)
				URL len on 8 bits
				URL
				keys, cyphered with oyur magic key :)
			*/
			cypherOffset = pssh->private_data[0] + 1;
			gf_bin128_parse("0x6770616363656E6364726D746F6F6C31", cypherKey);
			gf_bin128_parse("0x00000000000000000000000000000001", cypherIV);

			mc = gf_crypt_open("AES-128", "CTR");
			if (!mc) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC/ISMA] Cannot open AES-128 CTR\n"));
				return GF_IO_ERR;
			}
			gf_crypt_init(mc, cypherKey, 16, cypherIV);
			gf_crypt_decrypt(mc, pssh->private_data+cypherOffset, pssh->private_data_size-cypherOffset);
			gf_crypt_close(mc);

			/*now we search a key*/
			priv->KID_count = pssh->KID_count;
			if (priv->KIDs) {
				gf_free(priv->KIDs);
				priv->KIDs = NULL;
			}
			priv->KIDs = (bin128 *)gf_malloc(pssh->KID_count*sizeof(bin128));
			if (priv->keys) {
				gf_free(priv->keys);
				priv->keys = NULL;
			}
			priv->keys = (bin128 *)gf_malloc(pssh->KID_count*sizeof(bin128));

			memmove(priv->KIDs, pssh->KIDs, pssh->KID_count*sizeof(bin128));
			memmove(priv->keys, pssh->private_data + cypherOffset, pssh->KID_count*sizeof(bin128));
		}
	}

	if (cfg->scheme_type == GF_4CC('c', 'e', 'n', 'c'))
		priv->is_cenc = GF_TRUE;
	else
		priv->is_cbc = GF_TRUE;

	priv->state = ISMAEA_STATE_SETUP;
	//priv->nb_allow_play = 1;
	return GF_OK;
}

static GF_Err CENC_Access(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	if (evt->event_type==GF_IPMP_TOOL_GRANT_ACCESS) {
		if (priv->state != ISMAEA_STATE_SETUP) return GF_SERVICE_ERROR;
		assert(!priv->crypt);

		//if (!priv->nb_allow_play) return GF_AUTHENTICATION_FAILURE;
		//priv->nb_allow_play--;

		/*open decrypter - we do NOT initialize decrypter; it wil be done when we decrypt the first crypted sample*/
		if (priv->is_cenc)
			priv->crypt = gf_crypt_open("AES-128", "CTR");
		else
			priv->crypt = gf_crypt_open("AES-128", "CBC");
		if (!priv->crypt) return GF_IO_ERR;

		priv->first_crypted_samp = GF_TRUE;

		priv->state = ISMAEA_STATE_PLAY;
		return GF_OK;
	}
	if (evt->event_type==GF_IPMP_TOOL_RELEASE_ACCESS) {
		if (priv->state != ISMAEA_STATE_PLAY) return GF_SERVICE_ERROR;
		if (priv->crypt) gf_crypt_close(priv->crypt);
		priv->crypt = NULL;
		priv->state = ISMAEA_STATE_SETUP;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

static GF_Err CENC_ProcessData(ISMAEAPriv *priv, GF_IPMPEvent *evt)
{
	GF_Err e;
	GF_BitStream *pleintext_bs, *cyphertext_bs, *sai_bs;
	char IV[17];
	bin128 KID;
	char *buffer;
	u32 max_size, i, subsample_count;
	u64 BSO;
	GF_CENCSampleAuxInfo *sai;

	e = GF_OK;
	pleintext_bs = cyphertext_bs = sai_bs = NULL;
	buffer = NULL;
	max_size = 4096;

	if (!priv->crypt) return GF_SERVICE_ERROR;

	if (!evt->is_encrypted || !evt->IV_size || !evt->saiz) return GF_OK;

	cyphertext_bs = gf_bs_new(evt->data, evt->data_size, GF_BITSTREAM_READ);
	sai_bs = gf_bs_new(evt->sai, evt->saiz, GF_BITSTREAM_READ);
	pleintext_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	buffer = (char*)gf_malloc(sizeof(char) * max_size);

	sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
	if (!sai) {
		e = GF_IO_ERR;
		goto exit;
	}
	memset(sai, 0, sizeof(GF_CENCSampleAuxInfo));
	sai->IV_size = evt->IV_size;
	/*read sample auxiliary information from bitstream*/
	gf_bs_read_data(sai_bs,  (char *)KID, 16);
	gf_bs_read_data(sai_bs, (char *)sai->IV, sai->IV_size);
	sai->subsample_count = gf_bs_read_u16(sai_bs);
	if (sai->subsample_count) {
		sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
		for (i = 0; i < sai->subsample_count; i++) {
			sai->subsamples[i].bytes_clear_data = gf_bs_read_u16(sai_bs);
			sai->subsamples[i].bytes_encrypted_data = gf_bs_read_u32(sai_bs);
		}
	}


	for (i = 0; i < priv->KID_count; i++) {
		if (!strncmp((const char *)KID, (const char *)priv->KIDs[i], 16)) {
			memmove(priv->key, priv->keys[i], 16);
			break;
		}
	}

	if (priv->first_crypted_samp) {
		memmove(IV, sai->IV, sai->IV_size);
		if (sai->IV_size == 8)
			memset(IV+8, 0, sizeof(char)*8);
		e = gf_crypt_init(priv->crypt, priv->key, 16, IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot initialize AES-128 AES-128 %s (%s)\n", priv->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			e = GF_IO_ERR;
			goto exit;
		}
		priv->first_crypted_samp = GF_FALSE;
	} else {
		if (priv->is_cenc) {
			GF_BitStream *bs;
			bs = gf_bs_new(IV, 17, GF_BITSTREAM_WRITE);
			gf_bs_write_u8(bs, 0);	/*begin of counter*/
			gf_bs_write_data(bs,(char *)sai->IV, sai->IV_size);
			if (sai->IV_size == 8)
				gf_bs_write_u64(bs, 0); /*0-padded if IV_size == 8*/
			gf_bs_del(bs);
			gf_crypt_set_state(priv->crypt, IV, 17);
		}
		e = gf_crypt_set_key(priv->crypt, priv->key, 16, IV);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[CENC] Cannot set key AES-128 %s (%s)\n", priv->is_cenc ? "CTR" : "CBC", gf_error_to_string(e)) );
			e = GF_IO_ERR;
			goto exit;
		}
	}

	subsample_count = 0;
	BSO = 0;
	while (gf_bs_available(cyphertext_bs)) {
		if (subsample_count >= sai->subsample_count)
			break;

		/*read clear data and write it to pleintext bitstream*/
		if (max_size < sai->subsamples[subsample_count].bytes_clear_data) {
			buffer = (char*)gf_realloc(buffer, sizeof(char)*sai->subsamples[subsample_count].bytes_clear_data);
			max_size = sai->subsamples[subsample_count].bytes_clear_data;
		}
		gf_bs_read_data(cyphertext_bs, buffer, sai->subsamples[subsample_count].bytes_clear_data);
		gf_bs_write_data(pleintext_bs, buffer, sai->subsamples[subsample_count].bytes_clear_data);

		/*now read encrypted data, decrypted it and write to pleintext bitstream*/
		if (max_size < sai->subsamples[subsample_count].bytes_encrypted_data) {
			buffer = (char*)gf_realloc(buffer, sizeof(char)*sai->subsamples[subsample_count].bytes_encrypted_data);
			max_size = sai->subsamples[subsample_count].bytes_encrypted_data;
		}
		gf_bs_read_data(cyphertext_bs, buffer, sai->subsamples[subsample_count].bytes_encrypted_data);
		gf_crypt_decrypt(priv->crypt, buffer, sai->subsamples[subsample_count].bytes_encrypted_data);
		gf_bs_write_data(pleintext_bs, buffer, sai->subsamples[subsample_count].bytes_encrypted_data);

		/*update IV for next subsample*/
		if (priv->is_cenc) {
			BSO += sai->subsamples[subsample_count].bytes_encrypted_data;
			if (gf_bs_available(cyphertext_bs)) {
				char next_IV[17];
				u64 prev_block_count, salt_portion, block_count_portion;
				u32 remain;
				GF_BitStream *bs, *tmp;

				prev_block_count = BSO / 16;
				remain = BSO % 16;
				tmp = gf_bs_new((const char *)sai->IV, 16, GF_BITSTREAM_READ);
				bs = gf_bs_new(next_IV, 17, GF_BITSTREAM_WRITE);
				gf_bs_write_u8(bs, 0);	/*begin of counter*/

				salt_portion = gf_bs_read_u64(tmp);
				block_count_portion = gf_bs_read_u64(tmp);
				/*reset the block counter to zero without affecting the other 64 bits of the IV*/
				if (prev_block_count > 0xFFFFFFFFFFFFFFFFULL - block_count_portion)
					block_count_portion = prev_block_count - (0xFFFFFFFFFFFFFFFFULL - block_count_portion) - 1;
				else
					block_count_portion +=  prev_block_count;
				gf_bs_write_u64(bs, salt_portion);
				gf_bs_write_u64(bs, block_count_portion);

				gf_crypt_set_state(priv->crypt, next_IV, 17);
				/*decrypt remain bytes*/
				if (remain) {
					char dummy[20];
					gf_crypt_decrypt(priv->crypt, dummy, remain);
				}

				gf_bs_del(bs);
				gf_bs_del(tmp);
			}
		}

		subsample_count++;
	}

	if (buffer) gf_free(buffer);
	gf_bs_get_content(pleintext_bs, &buffer, &evt->data_size);
	memmove(evt->data, buffer, evt->data_size);

exit:
	if (pleintext_bs) gf_bs_del(pleintext_bs);
	if (sai_bs) gf_bs_del(sai_bs);
	if (cyphertext_bs) gf_bs_del(cyphertext_bs);
	if (buffer) gf_free(buffer);
	if (sai && sai->subsamples) gf_free(sai->subsamples);
	if (sai) gf_free(sai);
	return e;
}

static GF_Err IPMP_Process(GF_IPMPTool *plug, GF_IPMPEvent *evt)
{
	ISMAEAPriv *priv = (ISMAEAPriv *)plug->udta;

	switch (evt->event_type) {
	case GF_IPMP_TOOL_SETUP:
		if (evt->config_data_code == GF_4CC('i','s','m','a')) return ISMA_Setup(priv, evt);
#ifdef OMA_DRM_MP4MC
		if (evt->config_data_code == GF_4CC('o','d','r','m')) return OMA_DRM_Setup(priv, evt);
#endif
		if((evt->config_data_code != GF_4CC('c', 'e', 'n', 'c')) || (evt->config_data_code != GF_4CC('c','b','c','1'))) return CENC_Setup(priv, evt);
		return GF_NOT_SUPPORTED;

	case GF_IPMP_TOOL_GRANT_ACCESS:
	case GF_IPMP_TOOL_RELEASE_ACCESS:
		if (priv->is_cenc || priv->is_cbc) {
			return CENC_Access(priv, evt);
		} else if (priv->is_oma) {
		} else {
			return ISMA_Access(priv, evt);
		}
		break;
	case GF_IPMP_TOOL_PROCESS_DATA:
		if (priv->is_cenc || priv->is_cbc) {
			return CENC_ProcessData(priv, evt);
		} else if (priv->is_oma) {
			if (evt->is_encrypted) {
				evt->restart_requested = GF_TRUE;
				return GF_EOS;
			}
			return GF_OK;
		}
		return ISMA_ProcessData(priv, evt);
	}
	return GF_OK;
}

void DeleteIPMPTool(GF_IPMPTool *plug)
{
	ISMAEAPriv *priv = (ISMAEAPriv *)plug->udta;
	/*in case something went wrong*/
	if (priv->crypt) gf_crypt_close(priv->crypt);
	if (priv->KIDs) gf_free(priv->KIDs);
	if (priv->keys) gf_free(priv->keys);
	gf_free(priv);
	gf_free(plug);
}

GF_IPMPTool *NewIPMPTool()
{
	ISMAEAPriv *priv;
	GF_IPMPTool *tmp;

	GF_SAFEALLOC(tmp, GF_IPMPTool);
	if (!tmp) return NULL;
	GF_SAFEALLOC(priv, ISMAEAPriv);
	tmp->udta = priv;
	tmp->process = IPMP_Process;
	GF_REGISTER_MODULE_INTERFACE(tmp, GF_IPMP_TOOL_INTERFACE, "GPAC ISMACryp tool", "gpac distribution")
	return (GF_IPMPTool *) tmp;
}

#endif /*GPAC_DISABLE_MCRYPT*/

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_MCRYPT
		GF_IPMP_TOOL_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
#ifndef GPAC_DISABLE_MCRYPT
	case GF_IPMP_TOOL_INTERFACE:
		return (GF_BaseInterface *)NewIPMPTool();
#endif
	default:
		return NULL;
	}
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_MCRYPT
	case GF_IPMP_TOOL_INTERFACE:
		DeleteIPMPTool((GF_IPMPTool *)ifce);
		break;
#endif
	}
}

GPAC_MODULE_STATIC_DELARATION( isma_ea )

