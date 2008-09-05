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

#ifndef GPAC_READ_ONLY

#include <gpac/scene_manager.h>
#include <gpac/bifs.h>
#include <gpac/constants.h>

struct __tag_bifs_engine
{
	GF_SceneGraph *sg;
	GF_SceneManager	*ctx;
	GF_SceneLoader load;
	void *calling_object;
	GF_StreamContext *sc;
	Bool owns_context;	
	GF_BifsEncoder *bifsenc;
	u32 stream_ts_res;
	/* TODO: maybe the currentAUCount should be a GF_List of u32 
	to capture the number of AU per input BIFS stream */
	u32 currentAUCount;

	char encoded_bifs_config[20];
	u32 encoded_bifs_config_size;
};

static GF_Err gf_sm_setup_bifsenc(GF_BifsEngine *codec, GF_InitialObjectDescriptor *iod)
{
	GF_Err e;
	char *data;
	u32 data_len;
	u32	j, nbb;
	GF_ESD *esd;
	Bool is_in_iod, delete_desc, encode_names, delete_bcfg;
	GF_BIFSConfig *bcfg;

	e = GF_OK;
	codec->bifsenc = gf_bifs_encoder_new(codec->ctx->scene_graph);

	delete_desc = 0;
	esd = NULL;
	is_in_iod = 1;
	if (iod) {
		is_in_iod = 0;
		j=0;
		while ((esd = (GF_ESD*)gf_list_enum(iod->ESDescriptors, &j))) {
			if (esd->decoderConfig && esd->decoderConfig->streamType == GF_STREAM_SCENE) {
				if (!codec->sc->ESID) codec->sc->ESID = esd->ESID;
				if (codec->sc->ESID == esd->ESID) {
					is_in_iod = 1;
					break;
				}
			}
			/*special BIFS direct import from NHNT*/
			else if (gf_list_count(iod->ESDescriptors)==1) {
				codec->sc->ESID = esd->ESID;
				is_in_iod = 1;
				break;
			}
			esd = NULL;
		}
	}

	if (!esd) {
		delete_desc = 1;
		esd = gf_odf_desc_esd_new(2);
		gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
		esd->decoderConfig->decoderSpecificInfo = NULL;
		esd->ESID = codec->sc->ESID;
		esd->decoderConfig->streamType = GF_STREAM_SCENE;
	}

	delete_bcfg = 0;

	/*should NOT happen (means inputctx is not properly setup)*/
	if (!esd->decoderConfig->decoderSpecificInfo) {
		bcfg = (GF_BIFSConfig*)gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
		bcfg->pixelMetrics = codec->ctx->is_pixel_metrics;
		bcfg->pixelWidth = codec->ctx->scene_width;
		bcfg->pixelHeight = codec->ctx->scene_height;
		delete_bcfg = 1;
	}
	/*regular retrieve from ctx*/
	else if (esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_BIFS_CFG_TAG) {
		bcfg = (GF_BIFSConfig *)esd->decoderConfig->decoderSpecificInfo;
	}
	/*should not happen either (unless loading from MP4 in which case BIFSc is not decoded)*/
	else {
		bcfg = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
		delete_bcfg = 1;
	}
	/*NO CHANGE TO BIFSC otherwise the generated update will not match the input context, UNLESS NO NbBits
	were specified*/
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
	gf_bifs_encoder_new_stream(codec->bifsenc, codec->sc->ESID, bcfg, encode_names, 0);
	if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);

	if (!esd->slConfig) esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	if (codec->sc->timeScale) esd->slConfig->timestampResolution = codec->sc->timeScale;
	if (!esd->slConfig->timestampResolution) esd->slConfig->timestampResolution = 1000;

	esd->ESID = codec->sc->ESID;
	gf_bifs_encoder_get_config(codec->bifsenc, codec->sc->ESID, &data, &data_len);

	if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->decoderConfig->decoderSpecificInfo->data = data;
	esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
	
	codec->stream_ts_res = esd->slConfig->timestampResolution;
	memcpy(codec->encoded_bifs_config, data, data_len);
	codec->encoded_bifs_config_size = data_len;

	esd->decoderConfig->objectTypeIndication = gf_bifs_encoder_get_version(codec->bifsenc, codec->sc->ESID);		
	return GF_OK;
}

static GF_Err gf_sm_live_setup(GF_BifsEngine *codec)
{
	GF_Err e;
	GF_InitialObjectDescriptor *iod;
	u32	i, count;

	e = GF_OK;

	iod = (GF_InitialObjectDescriptor *) codec->ctx->root_od;
	/*if no iod check we only have one bifs*/
	if (!iod) {
		count = 0;
		i=0;
		while ((codec->sc = (GF_StreamContext*)gf_list_enum(codec->ctx->streams, &i))) {
			if (codec->sc->streamType == GF_STREAM_OD) count++;
			codec->sc = NULL;
		}
		if (!iod && count>1) return GF_NOT_SUPPORTED;
	}

	codec->sc = NULL;
	count = gf_list_count(codec->ctx->streams);
	i=0;
	while ((codec->sc = (GF_StreamContext*)gf_list_enum(codec->ctx->streams, &i))) {
		if (codec->sc->streamType == GF_STREAM_SCENE) break;
	}
	if (!codec->sc) return GF_NOT_SUPPORTED;
	if (!codec->sc->ESID) codec->sc->ESID = 1;

	if (codec->sc->objectType <= GPAC_OTI_SCENE_BIFS_V2) {
		return gf_sm_setup_bifsenc(codec, iod);
	} else if (codec->sc->objectType == GPAC_OTI_SCENE_DIMS) {
		/* TODO */
		return GF_NOT_SUPPORTED;
	}
	return e;
}

static GF_Err gf_sm_live_encode_scene_au(GF_BifsEngine *codec, u32 currentAUCount, 
						  GF_Err (*AUCallback)(void *, char *, u32 , u64)
						  )
{
	GF_Err e;
	u32	j, size, count;
	char *data;
	GF_AUContext *au;

	if (!AUCallback) return GF_BAD_PARAM;

	e = GF_OK;
	count = gf_list_count(codec->sc->AUs);
	for (j=currentAUCount; j<count; j++) {
		au = (GF_AUContext *)gf_list_get(codec->sc->AUs, j);
		/*in case using XMT*/
		if (au->timing_sec) au->timing = (u64) (au->timing_sec * codec->stream_ts_res);
		switch(codec->sc->objectType) {
		case GPAC_OTI_SCENE_BIFS:
		case GPAC_OTI_SCENE_BIFS_V2:
			e = gf_bifs_encode_au(codec->bifsenc, codec->sc->ESID, au->commands, &data, &size);
			break;
		case GPAC_OTI_SCENE_DIMS:
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("Cannot encode AU for Scene OTI %x\n", codec->sc->objectType));
			break;
		}
		AUCallback(codec->calling_object, data, size, au->timing);
		free(data);
		data = NULL;
		if (e) break;
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
		GF_ISOFile *mp4;
		strcat(szF, ".mp4");
		mp4 = gf_isom_open(szF, GF_ISOM_OPEN_WRITE, NULL);
		e = gf_sm_encode_to_file(codec->ctx, mp4, NULL);
		if (e) gf_isom_delete(mp4);
		else gf_isom_close(mp4);
	} else {
		e = gf_sm_dump(codec->ctx, szF, d_mode);
	}
	return e;
}

GF_EXPORT
GF_Err gf_beng_encode_from_string(GF_BifsEngine *codec, char *auString, GF_Err (*AUCallback)(void *, char *, u32 , u64 ))
{
	GF_StreamContext *sc;
	u32 i, count;
	GF_Err e;

	memset(&(codec->load), 0, sizeof(GF_SceneLoader));
	codec->load.ctx = codec->ctx;
	
	/* Assumes there is only one BIFS stream in the context
	   TODO: check how to do it when several BIFS streams are encoded at the same time */
	sc = NULL;
	count = gf_list_count(codec->ctx->streams);
	i = 0;
	while ((sc = (GF_StreamContext*)gf_list_enum(codec->ctx->streams, &i))) {
		if (sc->streamType == GF_STREAM_SCENE) break;
		sc = NULL;
	}
	if (!sc) return GF_BAD_PARAM;
	codec->currentAUCount = gf_list_count(sc->AUs);

	codec->load.flags = GF_SM_LOAD_MPEG4_STRICT | GF_SM_LOAD_CONTEXT_READY;
	codec->load.type = GF_SM_LOAD_BT;

	e = gf_sm_load_string(&codec->load, auString, 0);
	if (e) goto exit;

	e = gf_sm_live_encode_scene_au(codec, codec->currentAUCount, AUCallback); 
exit:
	return e;
}

GF_EXPORT
GF_Err gf_beng_encode_from_file(GF_BifsEngine *codec, char *auFile, GF_Err (*AUCallback)(void *, char *, u32 , u64 ))
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
		if (sc->streamType == GF_STREAM_SCENE) break;
		sc = NULL;
	}
	if (!sc) return GF_BAD_PARAM;
	codec->currentAUCount = gf_list_count(sc->AUs);

	codec->load.flags = GF_SM_LOAD_MPEG4_STRICT | GF_SM_LOAD_CONTEXT_READY;
	e = gf_sm_load_init(&codec->load);
	if (!e) e = gf_sm_load_run(&codec->load);
	gf_sm_load_done(&codec->load);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[BENG] cannot load AU File %s (error %s)\n", auFile, gf_error_to_string(e)));
		goto exit;
	}

	e = gf_sm_live_encode_scene_au(codec, codec->currentAUCount, AUCallback); 
	if (e) goto exit;
exit:
	return e;
}

GF_EXPORT
GF_Err gf_beng_encode_context(GF_BifsEngine *codec, GF_Err (*AUCallback)(void *, char *, u32 , u64 ))
{
	return gf_sm_live_encode_scene_au(codec, 0, AUCallback);
} 

GF_EXPORT
void gf_beng_terminate(GF_BifsEngine *codec)
{
	if (codec->bifsenc) gf_bifs_encoder_del(codec->bifsenc);

	if (codec->owns_context) {
		if (codec->ctx) gf_sm_del(codec->ctx);
		if (codec->sg) gf_sg_del(codec->sg);
	}
	free(codec);
}

GF_EXPORT
void gf_beng_get_stream_config(GF_BifsEngine *codec, char **config, u32 *config_len)
{
	*config = codec->encoded_bifs_config;
	*config_len = codec->encoded_bifs_config_size;
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

	if (e) {
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


#endif

