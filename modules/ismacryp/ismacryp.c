/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
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

#include <gpac/modules/ipmp.h>
#include <gpac/crypt.h>
#include <gpac/ismacryp.h>
#include <gpac/base_coding.h>
#include <gpac/download.h>
#include <gpac/internal/terminal_dev.h>


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
	t = (strstr(kms_url, "://") == NULL) ? fopen(kms_url, "r") : NULL;
	if (t) {
		fclose(t);
		return gf_ismacryp_gpac_get_info(ch->esd->ESID, (char *)kms_url, priv->key, priv->salt);
	}
	/*note that gpac doesn't have TLS support -> not really usefull. As a general remark, ISMACryp
	is supported as a proof of concept, crypto and IPMP being the last priority on gpac...*/
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ISMA E&A] Fetching ISMACryp key for channel %d\n", ch->esd->ESID) );

	sess = gf_term_download_new(ch->service, kms_url, 0, ISMA_KMS_NetIO, ch);
	if (!sess) return GF_IO_ERR;

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
		gf_base64_decode((char*)cfg->kms_uri+5, strlen(cfg->kms_uri)-5, data, 100);
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
			szT[0] = k[2*i]; szT[1] = k[2*i + 1];
			sscanf(szT, "%X", &v); 
			priv->key[i] = v;
		}

		k = (char *)cfg->kms_uri + 10 + 32;
		for (i=0; i<8; i++) { 
			szT[0] = k[2*i]; szT[1] = k[2*i + 1];
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
	{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 } };
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
		len = strlen(cfg->oma_drm_textual_headers + hdr_pos);
		hdr_pos += len+1;
	}
	priv->is_oma = 1;

	/*TODO: call DRM agent, fetch keys*/
	if (!cfg->kms_uri) return GF_NON_COMPLIANT_BITSTREAM;
	priv->state = ISMAEA_STATE_SETUP;
	//priv->nb_allow_play = 1;
	
	/*we have preview*/
	if (priv->preview_range) return GF_OK;
	return GF_NOT_SUPPORTED;
}
#endif

static GF_Err ISMA_Process(GF_IPMPTool *plug, GF_IPMPEvent *evt)
{
	ISMAEAPriv *priv = (ISMAEAPriv *)plug->udta;

	switch (evt->event_type) {
	case GF_IPMP_TOOL_SETUP:
		if (evt->config_data_code == GF_4CC('i','s','m','a')) return ISMA_Setup(priv, evt);
#ifdef OMA_DRM_MP4MC
		if (evt->config_data_code == GF_4CC('o','d','r','m')) return OMA_DRM_Setup(priv, evt);
#endif
		return GF_NOT_SUPPORTED;
		
	case GF_IPMP_TOOL_GRANT_ACCESS:
	case GF_IPMP_TOOL_RELEASE_ACCESS:
		if (priv->is_oma) {
		} else {
			return ISMA_Access(priv, evt);
		}
		break;
	case GF_IPMP_TOOL_PROCESS_DATA:
		if (priv->is_oma) {
			if (evt->is_encrypted) {
				evt->restart_requested = 1;
				return GF_EOS;
			}
			return GF_OK;
		}
		return ISMA_ProcessData(priv, evt);
	}
	return GF_OK;
}

void DeleteISMACrypTool(GF_IPMPTool *plug)
{
	ISMAEAPriv *priv = (ISMAEAPriv *)plug->udta;
	/*in case something went wrong*/
	if (priv->crypt) gf_crypt_close(priv->crypt);
	free(priv);
	free(plug);
}

GF_IPMPTool *NewISMACrypTool()
{
	ISMAEAPriv *priv;
	GF_IPMPTool *tmp;
	
	GF_SAFEALLOC(tmp, GF_IPMPTool);
	if (!tmp) return NULL;
	GF_SAFEALLOC(priv, ISMAEAPriv);
	tmp->udta = priv;
	tmp->process = ISMA_Process;
	GF_REGISTER_MODULE_INTERFACE(tmp, GF_IPMP_TOOL_INTERFACE, "GPAC ISMACryp tool", "gpac distribution")
	return (GF_IPMPTool *) tmp;
}

GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_IPMP_TOOL_INTERFACE:
		return 1;
	default:
		return 0;
	}
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_IPMP_TOOL_INTERFACE:
		return (GF_BaseInterface *)NewISMACrypTool();
	default:
		return NULL;
	}
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_IPMP_TOOL_INTERFACE:
		DeleteISMACrypTool((GF_IPMPTool *)ifce);
		break;
	}
}
