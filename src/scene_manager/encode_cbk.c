/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
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

#include <gpac/bifsengine.h>

#ifndef GPAC_DISABLE_BENG

#include <gpac/scene_manager.h>
#ifndef GPAC_DISABLE_BIFS_ENC
#include <gpac/bifs.h>
#endif
#ifndef GPAC_DISABLE_LASER
#include <gpac/laser.h>
#endif

#include <gpac/constants.h>

struct __tag_bifs_engine
{
	GF_SceneGraph *sg;
	GF_SceneManager	*ctx;
	GF_SceneLoader load;
	void *calling_object;
	Bool owns_context;	
#ifndef GPAC_DISABLE_BIFS_ENC
	GF_BifsEncoder *bifsenc;
#endif
#ifndef GPAC_DISABLE_LASER
	GF_LASeRCodec *lsrenc;
#endif

};

#ifndef GPAC_DISABLE_BIFS_ENC

static GF_Err gf_sm_setup_bifsenc(GF_BifsEngine *codec, GF_StreamContext *sc, GF_ESD *esd)
{
	GF_Err e;
	char *data;
	u32 data_len;
	u32	nbb;
	Bool encode_names, delete_bcfg;
	GF_BIFSConfig *bcfg;

	if (!esd->decoderConfig || (esd->decoderConfig->streamType != GF_STREAM_SCENE)) return GF_BAD_PARAM;

	e = GF_OK;
	if (!codec->bifsenc)
		codec->bifsenc = gf_bifs_encoder_new(codec->ctx->scene_graph);

	delete_bcfg = 0;
	/*inputctx is not properly setup, do it*/
	if (!esd->decoderConfig->decoderSpecificInfo) {
		bcfg = (GF_BIFSConfig*)gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
		bcfg->pixelMetrics = codec->ctx->is_pixel_metrics;
		bcfg->pixelWidth = codec->ctx->scene_width;
		bcfg->pixelHeight = codec->ctx->scene_height;
		delete_bcfg = 1;
	}
	/*regular case*/
	else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_BIFS_CFG_TAG) {
		bcfg = (GF_BIFSConfig *)esd->decoderConfig->decoderSpecificInfo;
	}
	/*happens when loading from MP4 in which case BIFSc is not decoded*/
	else {
		bcfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
		delete_bcfg = 1;
	}

	/*NO CHANGE TO BIFSC otherwise the generated update will not match the input context
	The only case we modify the bifs config is when XXXBits is not specified*/
	nbb = gf_get_bit_size(codec->ctx->max_node_id);
	if (!bcfg->nodeIDbits) bcfg->nodeIDbits = nbb;
	else if (bcfg->nodeIDbits<nbb) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] BIFSConfig.NodeIDBits too small (%d bits vs %d nodes)\n", bcfg->nodeIDbits, codec->ctx->max_node_id));
	}
	nbb = gf_get_bit_size(codec->ctx->max_route_id);
	if (!bcfg->routeIDbits) bcfg->routeIDbits = nbb;
	else if (bcfg->routeIDbits<nbb) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] BIFSConfig.RouteIDBits too small (%d bits vs %d routes)\n", bcfg->routeIDbits, codec->ctx->max_route_id));
	}
	nbb = gf_get_bit_size(codec->ctx->max_proto_id);
	if (!bcfg->protoIDbits) bcfg->protoIDbits=nbb;
	else if (bcfg->protoIDbits<nbb) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] BIFSConfig.ProtoIDBits too small (%d bits vs %d protos)\n", bcfg->protoIDbits, codec->ctx->max_proto_id));
	}

	/*this is the real pb, not stored in cfg or file level, set at EACH replaceScene*/
	encode_names = 0;

	/* The BIFS Config that is passed here should be the BIFSConfig from the IOD */
	gf_bifs_encoder_new_stream(codec->bifsenc, esd->ESID, bcfg, encode_names, 0);
	if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);

	gf_bifs_encoder_get_config(codec->bifsenc, esd->ESID, &data, &data_len);

	if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->decoderConfig->decoderSpecificInfo->data = data;
	esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
		
	sc->dec_cfg = malloc(sizeof(char)*data_len);
	memcpy(sc->dec_cfg, data, data_len);
	sc->dec_cfg_len = data_len;

	esd->decoderConfig->objectTypeIndication = gf_bifs_encoder_get_version(codec->bifsenc, esd->ESID);		

	return GF_OK;
}
#endif /*GPAC_DISABLE_BIFS_ENC*/


#ifndef GPAC_DISABLE_LASER

static GF_Err gf_sm_setup_lsrenc(GF_BifsEngine *codec, GF_StreamContext *sc, GF_ESD *esd)
{
	GF_Err e;
	char *data;
	u32 data_len;
	GF_LASERConfig lsr_cfg;

	if (!esd->decoderConfig || (esd->decoderConfig->streamType != GF_STREAM_SCENE)) return GF_BAD_PARAM;

	e = GF_OK;
	codec->lsrenc = gf_laser_encoder_new(codec->ctx->scene_graph);


	/*inputctx is not properly setup, do it*/
	if (!esd->decoderConfig->decoderSpecificInfo) {
		memset(&lsr_cfg, 0, sizeof(GF_LASERConfig));
	}
	/*regular case*/
	else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_LASER_CFG_TAG) {
		memcpy(&lsr_cfg, (GF_LASERConfig *)esd->decoderConfig->decoderSpecificInfo, sizeof(GF_LASERConfig) );
	}
	/*happens when loading from MP4 in which case BIFSc is not decoded*/
	else {
		gf_odf_get_laser_config(esd->decoderConfig->decoderSpecificInfo, &lsr_cfg);
	}

	gf_laser_encoder_new_stream(codec->lsrenc, esd->ESID , &lsr_cfg);

	gf_laser_encoder_get_config(codec->lsrenc, esd->ESID, &data, &data_len);

	if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->decoderConfig->decoderSpecificInfo->data = data;
	esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
		
	sc->dec_cfg = malloc(sizeof(char)*data_len);
	memcpy(sc->dec_cfg, data, data_len);
	sc->dec_cfg_len = data_len;
	return GF_OK;
}
#endif /*GPAC_DISABLE_LASER*/

static GF_Err gf_sm_live_setup(GF_BifsEngine *codec)
{
	GF_Err e;
	GF_StreamContext *sc;
	GF_InitialObjectDescriptor *iod;
	GF_ESD *esd;
	u32	i, j, count;

	e = GF_OK;

	iod = (GF_InitialObjectDescriptor *) codec->ctx->root_od;

	/*build an IOD*/
	if (!iod) {
		codec->ctx->root_od = (GF_ObjectDescriptor*) gf_odf_desc_new(GF_ODF_IOD_TAG);
		iod = (GF_InitialObjectDescriptor *) codec->ctx->root_od;

		i=0;
		while ((sc = gf_list_enum(codec->ctx->streams, &i))) {
			if (sc->streamType != GF_STREAM_SCENE) continue;

			esd = gf_odf_desc_esd_new(2);
			gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
			esd->decoderConfig->decoderSpecificInfo = NULL;
			esd->ESID = sc->ESID;
			esd->decoderConfig->streamType = GF_STREAM_SCENE;
			esd->decoderConfig->objectTypeIndication = sc->objectType;
			gf_list_add(iod->ESDescriptors, esd);
		
			if (!sc->timeScale) sc->timeScale = 1000;
			esd->slConfig->timestampResolution = sc->timeScale;
		}
	}

	count = gf_list_count(codec->ctx->streams);
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(codec->ctx->streams, &i))) {
		if (!sc->ESID) sc->ESID = 1;

		j=0;
		while ((esd = gf_list_enum(codec->ctx->root_od->ESDescriptors, &j))) {
			if (sc->ESID==esd->ESID) {
				break;
			}
		}
		if (!esd) continue;

		if (!esd->slConfig) esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = 1000;
		if (!sc->timeScale) sc->timeScale = esd->slConfig->timestampResolution;


		if (sc->streamType == GF_STREAM_SCENE) {	
			switch (sc->objectType) {
#ifndef GPAC_DISABLE_BIFS_ENC
			case GPAC_OTI_SCENE_BIFS:
			case GPAC_OTI_SCENE_BIFS_V2:
				e = gf_sm_setup_bifsenc(codec, sc, esd);
				break;
#endif

#ifndef GPAC_DISABLE_LASER
			case GPAC_OTI_SCENE_LASER:
				e = gf_sm_setup_lsrenc(codec, sc, esd);
				break;
#endif
			case GPAC_OTI_SCENE_DIMS:
				/* TODO */
				e = GF_NOT_SUPPORTED;
				break;
			default:
				e = GF_NOT_SUPPORTED;
				break;
			}	
			if (e) return e;
		}
	}
	return e;
}

static GF_Err gf_sm_live_encode_scene_au(GF_BifsEngine *codec, gf_beng_callback callback, Bool from_start)
{
	GF_Err e;
	u32	i, j, size, count, nb_streams;
	char *data;
	GF_AUContext *au;

	if (!callback) return GF_BAD_PARAM;

	e = GF_OK;

	nb_streams = gf_list_count(codec->ctx->streams);
	for (i=0; i<nb_streams;i++) {
		GF_StreamContext *sc = gf_list_get(codec->ctx->streams, i);
		if (sc->streamType != GF_STREAM_SCENE) continue;

		count = gf_list_count(sc->AUs);
		j = from_start ? 0 : sc->current_au_count;
		for (; j<count; j++) {
			au = (GF_AUContext *)gf_list_get(sc->AUs, j);
			data = NULL;
			size = 0;
			/*in case using XMT*/
			if (au->timing_sec) au->timing = (u64) (au->timing_sec * sc->timeScale);
			switch(sc->objectType) {
#ifndef GPAC_DISABLE_BIFS_ENC
			case GPAC_OTI_SCENE_BIFS:
			case GPAC_OTI_SCENE_BIFS_V2:
				e = gf_bifs_encode_au(codec->bifsenc, sc->ESID, au->commands, &data, &size);
				break;
#endif

#ifndef GPAC_DISABLE_BIFS_ENC
			case GPAC_OTI_SCENE_LASER:
				e = gf_laser_encode_au(codec->lsrenc, sc->ESID, au->commands, 0, &data, &size);
				break;
#endif
			case GPAC_OTI_SCENE_DIMS:
				break;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("Cannot encode AU for Scene OTI %x\n", sc->objectType));
				break;
			}
			callback(codec->calling_object, sc->ESID, data, size, au->timing);
			free(data);
			data = NULL;
			if (e) break;
		}
	}
	return e;
}

GF_EXPORT
GF_Err gf_beng_aggregate_context(GF_BifsEngine *codec)
{
	GF_Err	e;

	/*make random access for storage*/
	e = gf_sm_make_random_access(codec->ctx);
	if (e) return GF_BAD_PARAM;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_beng_save_context(GF_BifsEngine *codec, char *  ctxFileName)
{
	u32	d_mode, do_enc;
	char szF[GF_MAX_PATH], *ext;
	GF_Err	e;

	/*check if we dump to BT, XMT or encode to MP4*/
	strcpy(szF, ctxFileName);
	ext = strrchr(szF, '.');
	d_mode = GF_SM_DUMP_BT;
	do_enc = 0;
	if (ext) {
		if (!stricmp(ext, ".xmt") || !stricmp(ext, ".xmta")) d_mode = GF_SM_DUMP_XMTA;
		else if (!stricmp(ext, ".mp4")) do_enc = 1;
		ext[0] = 0;
	}

	if (do_enc) {
#ifndef GPAC_DISABLE_SCENE_ENCODER
		GF_ISOFile *mp4;
		strcat(szF, ".mp4");
		mp4 = gf_isom_open(szF, GF_ISOM_OPEN_WRITE, NULL);
		e = gf_sm_encode_to_file(codec->ctx, mp4, NULL);
		if (e) gf_isom_delete(mp4);
		else gf_isom_close(mp4);
#else
		return GF_NOT_SUPPORTED;
#endif
	} else {
		e = gf_sm_dump(codec->ctx, szF, d_mode);
	}
	return e;
}

GF_EXPORT
GF_Err gf_beng_encode_from_string(GF_BifsEngine *codec, char *auString, gf_beng_callback callback)
{
	GF_StreamContext *sc;
	u32 i, count, load_type;
	GF_Err e;

	load_type = codec->load.type;
	memset(&(codec->load), 0, sizeof(GF_SceneLoader));
	codec->load.ctx = codec->ctx;
	
	count = gf_list_count(codec->ctx->streams);
	i = 0;
	while ((sc = (GF_StreamContext*)gf_list_enum(codec->ctx->streams, &i))) {
		sc->current_au_count = gf_list_count(sc->AUs);
	}
	codec->load.flags = GF_SM_LOAD_MPEG4_STRICT | GF_SM_LOAD_CONTEXT_READY;
	codec->load.type = load_type;

	e = gf_sm_load_string(&codec->load, auString, 0);
	if (e) goto exit;

	e = gf_sm_live_encode_scene_au(codec, callback, 0); 
exit:
	return e;
}

GF_EXPORT
GF_Err gf_beng_encode_from_file(GF_BifsEngine *codec, char *auFile, gf_beng_callback callback)
{
	GF_Err e;
	GF_StreamContext *sc;
	u32 i, count;

	memset(&(codec->load), 0, sizeof(GF_SceneLoader));
	codec->load.fileName = auFile;
	codec->load.ctx = codec->ctx;

	/* Assumes there is only one BIFS stream in the context
	   TODO: check how to do it when several BIFS streams are encoded at the same time */
	sc = NULL;
	count = gf_list_count(codec->ctx->streams);
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(codec->ctx->streams, &i))) {
		sc->current_au_count = gf_list_count(sc->AUs);
	}

	codec->load.flags = GF_SM_LOAD_MPEG4_STRICT | GF_SM_LOAD_CONTEXT_READY;
	e = gf_sm_load_init(&codec->load);
	if (!e) e = gf_sm_load_run(&codec->load);
	gf_sm_load_done(&codec->load);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] cannot load AU File %s (error %s)\n", auFile, gf_error_to_string(e)));
		goto exit;
	}

	e = gf_sm_live_encode_scene_au(codec, callback, 0); 
	if (e) goto exit;
exit:
	return e;
}

GF_EXPORT
GF_Err gf_beng_encode_context(GF_BifsEngine *codec, gf_beng_callback callback)
{
	if (!codec) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] Cannot encode context. No codec provided\n"));
		return GF_BAD_PARAM;
	}
	return gf_sm_live_encode_scene_au(codec, callback, 1);
} 

GF_EXPORT
void gf_beng_terminate(GF_BifsEngine *codec)
{
#ifndef GPAC_DISABLE_BIFS_ENC
	if (codec->bifsenc) gf_bifs_encoder_del(codec->bifsenc);
#endif

#ifndef GPAC_DISABLE_BIFS_ENC
	if (codec->lsrenc) gf_laser_encoder_del(codec->lsrenc);
#endif

	if (codec->owns_context) {
		if (codec->ctx) gf_sm_del(codec->ctx);
		if (codec->sg) gf_sg_del(codec->sg);
	}
	free(codec);
}

GF_EXPORT
GF_Err gf_beng_get_stream_config(GF_BifsEngine *beng, u32 idx, u16 *ESID, const char **config, u32 *config_len)
{
	GF_StreamContext *sc = gf_list_get(beng->ctx->streams, idx);
	if (!sc || !ESID || !config || !config_len) return GF_BAD_PARAM;
	*ESID = sc->ESID;
	*config = sc->dec_cfg;
	*config_len = sc->dec_cfg_len;
	return GF_OK;
}


GF_EXPORT
GF_BifsEngine *gf_beng_init(void *calling_object, char * inputContext)
{
	GF_BifsEngine *codec;
	GF_Err e = GF_OK;

	if (!inputContext) return NULL;

	GF_SAFEALLOC(codec, GF_BifsEngine)
	if (!codec) return NULL;

	codec->calling_object = calling_object;

	/*Step 1: create context and load input*/
	codec->sg = gf_sg_new();
	codec->ctx = gf_sm_new(codec->sg);
	codec->owns_context = 1;
	memset(&(codec->load), 0, sizeof(GF_SceneLoader));
	codec->load.ctx = codec->ctx;
	/*since we're encoding in BIFS we must get MPEG-4 nodes only*/
	codec->load.flags = GF_SM_LOAD_MPEG4_STRICT;

	codec->load.fileName = inputContext;
	e = gf_sm_load_init(&(codec->load));
	if (!e) e = gf_sm_load_run(&(codec->load));
	gf_sm_load_done(&(codec->load));

	if (e<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] Cannot load context from %s (error %s)\n", inputContext, gf_error_to_string(e)));
		goto exit;
	}
	e = gf_sm_live_setup(codec);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] cannot init scene encoder for context (error %s)\n", gf_error_to_string(e)));
		goto exit;
	}
	return codec;

exit:
	gf_beng_terminate(codec);
	return NULL;
}

GF_EXPORT
GF_BifsEngine *gf_beng_init_from_context(void *calling_object, GF_SceneManager *ctx)
{
	GF_BifsEngine *codec;
	GF_Err e = GF_OK;

	if (!ctx) return NULL;

	GF_SAFEALLOC(codec, GF_BifsEngine)
	if (!codec) return NULL;

	codec->calling_object = calling_object;

	/*Step 1: create context and load input*/
	codec->sg = ctx->scene_graph;
	codec->ctx = ctx;
	codec->owns_context = 0;

	e = gf_sm_live_setup(codec);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] cannot init scene encoder for context (error %s)\n", gf_error_to_string(e)));
		goto exit;
	}
	return codec;

exit:
	gf_beng_terminate(codec);
	return NULL;
}

GF_EXPORT
GF_BifsEngine *gf_beng_init_from_string(void *calling_object, char * inputContext, u32 width, u32 height, Bool usePixelMetrics)
{
	GF_BifsEngine *codec;
	GF_Err e = GF_OK;

	if (!inputContext) return NULL;

	GF_SAFEALLOC(codec, GF_BifsEngine)
	if (!codec) return NULL;

	codec->calling_object = calling_object;

	/*Step 1: create context and load input*/
	codec->sg = gf_sg_new();
	codec->ctx = gf_sm_new(codec->sg);
	codec->owns_context = 1;
	memset(&(codec->load), 0, sizeof(GF_SceneLoader));
	codec->load.ctx = codec->ctx;
	/*since we're encoding in BIFS we must get MPEG-4 nodes only*/
	codec->load.flags = GF_SM_LOAD_MPEG4_STRICT;

	if (inputContext[0] == '<') {
		if (strstr(inputContext, "<svg ")) codec->load.type = GF_SM_LOAD_SVG_DA;
		else if (strstr(inputContext, "<saf ")) codec->load.type = GF_SM_LOAD_XSR;
		else if (strstr(inputContext, "XMT-A") || strstr(inputContext, "X3D")) codec->load.type = GF_SM_LOAD_XMTA;
	} else {
		codec->load.type = GF_SM_LOAD_BT;
	}
	e = gf_sm_load_string(&codec->load, inputContext, 0);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] cannot load context from %s (error %s)\n", inputContext, gf_error_to_string(e)));
		goto exit;
	}
	if (!codec->ctx->root_od) {
		codec->ctx->is_pixel_metrics = usePixelMetrics;
		codec->ctx->scene_width = width;
		codec->ctx->scene_height = height;
	}

	e = gf_sm_live_setup(codec);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] cannot init scene encoder for context (error %s)\n", gf_error_to_string(e)));
		goto exit;
	}
	return codec;

exit:
	gf_beng_terminate(codec);
	return NULL;
}

GF_EXPORT
u32 gf_beng_get_stream_count(GF_BifsEngine *beng)
{
	return gf_list_count(beng->ctx->streams);
}


#endif /*GPAC_DISABLE_BENG*/

