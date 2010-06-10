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

#include <gpac/scene_engine.h>

#ifndef GPAC_DISABLE_SENG

#include <gpac/scene_manager.h>
#ifndef GPAC_DISABLE_BIFS_ENC
#include <gpac/bifs.h>
#endif
#ifndef GPAC_DISABLE_LASER
#include <gpac/laser.h>
#endif

#include <gpac/constants.h>
#include <gpac/base_coding.h>

#include <time.h>

/* for DIMS GZ encoding */
#include <zlib.h>

struct __tag_scene_engine
{
	GF_SceneGraph *sg;
	GF_SceneManager	*ctx;
	GF_SceneLoader loader;
	void *calling_object;
	Bool owns_context;	
#ifndef GPAC_DISABLE_BIFS_ENC
	GF_BifsEncoder *bifsenc;
#endif
#ifndef GPAC_DISABLE_LASER
	GF_LASeRCodec *lsrenc;
#endif

	u32 start_time;

    char *dump_path;

	Bool embed_resources;
};

#ifndef GPAC_DISABLE_BIFS_ENC
static GF_Err gf_sm_setup_bifsenc(GF_SceneEngine *seng, GF_StreamContext *sc, GF_ESD *esd)
{
	GF_Err e;
	char *data;
	u32 data_len;
	u32	nbb;
	Bool encode_names, delete_bcfg;
	GF_BIFSConfig *bcfg;

	if (!esd->decoderConfig || (esd->decoderConfig->streamType != GF_STREAM_SCENE)) return GF_BAD_PARAM;

	e = GF_OK;
	if (!seng->bifsenc)
		seng->bifsenc = gf_bifs_encoder_new(seng->ctx->scene_graph);

	delete_bcfg = 0;
	/*inputctx is not properly setup, do it*/
	if (!esd->decoderConfig->decoderSpecificInfo) {
		bcfg = (GF_BIFSConfig*)gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
		bcfg->pixelMetrics = seng->ctx->is_pixel_metrics;
		bcfg->pixelWidth = seng->ctx->scene_width;
		bcfg->pixelHeight = seng->ctx->scene_height;
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
	nbb = gf_get_bit_size(seng->ctx->max_node_id);
	if (!bcfg->nodeIDbits) bcfg->nodeIDbits = nbb;
	else if (bcfg->nodeIDbits<nbb) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] BIFSConfig.NodeIDBits too small (%d bits vs %d nodes)\n", bcfg->nodeIDbits, seng->ctx->max_node_id));
	}
	nbb = gf_get_bit_size(seng->ctx->max_route_id);
	if (!bcfg->routeIDbits) bcfg->routeIDbits = nbb;
	else if (bcfg->routeIDbits<nbb) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] BIFSConfig.RouteIDBits too small (%d bits vs %d routes)\n", bcfg->routeIDbits, seng->ctx->max_route_id));
	}
	nbb = gf_get_bit_size(seng->ctx->max_proto_id);
	if (!bcfg->protoIDbits) bcfg->protoIDbits=nbb;
	else if (bcfg->protoIDbits<nbb) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] BIFSConfig.ProtoIDBits too small (%d bits vs %d protos)\n", bcfg->protoIDbits, seng->ctx->max_proto_id));
	}

	/*this is the real pb, not stored in cfg or file level, set at EACH replaceScene*/
	encode_names = 0;

	/* The BIFS Config that is passed here should be the BIFSConfig from the IOD */
	gf_bifs_encoder_new_stream(seng->bifsenc, esd->ESID, bcfg, encode_names, 0);
	if (delete_bcfg) gf_odf_desc_del((GF_Descriptor *)bcfg);

	gf_bifs_encoder_get_config(seng->bifsenc, esd->ESID, &data, &data_len);

	if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->decoderConfig->decoderSpecificInfo->data = data;
	esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
		
	sc->dec_cfg = gf_malloc(sizeof(char)*data_len);
	memcpy(sc->dec_cfg, data, data_len);
	sc->dec_cfg_len = data_len;

	esd->decoderConfig->objectTypeIndication = gf_bifs_encoder_get_version(seng->bifsenc, esd->ESID);		

	return GF_OK;
}
#endif /*GPAC_DISABLE_BIFS_ENC*/

#ifndef GPAC_DISABLE_LASER
static GF_Err gf_sm_setup_lsrenc(GF_SceneEngine *seng, GF_StreamContext *sc, GF_ESD *esd)
{
	GF_Err e;
	char *data;
	u32 data_len;
	GF_LASERConfig lsr_cfg;

	if (!esd->decoderConfig || (esd->decoderConfig->streamType != GF_STREAM_SCENE)) return GF_BAD_PARAM;

	e = GF_OK;
	seng->lsrenc = gf_laser_encoder_new(seng->ctx->scene_graph);


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

	gf_laser_encoder_new_stream(seng->lsrenc, esd->ESID , &lsr_cfg);

	gf_laser_encoder_get_config(seng->lsrenc, esd->ESID, &data, &data_len);

	if (esd->decoderConfig->decoderSpecificInfo) gf_odf_desc_del((GF_Descriptor *) esd->decoderConfig->decoderSpecificInfo);
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->decoderConfig->decoderSpecificInfo->data = data;
	esd->decoderConfig->decoderSpecificInfo->dataLength = data_len;
		
	sc->dec_cfg = gf_malloc(sizeof(char)*data_len);
	memcpy(sc->dec_cfg, data, data_len);
	sc->dec_cfg_len = data_len;
	return GF_OK;
}
#endif /*GPAC_DISABLE_LASER*/

static GF_Err gf_sm_live_setup(GF_SceneEngine *seng)
{
	GF_Err e;
	GF_StreamContext *sc;
	GF_InitialObjectDescriptor *iod;
	GF_ESD *esd;
	u32	i, j, count;

	e = GF_OK;

	iod = (GF_InitialObjectDescriptor *) seng->ctx->root_od;

	/*build an IOD*/
	if (!iod) {
		seng->ctx->root_od = (GF_ObjectDescriptor*) gf_odf_desc_new(GF_ODF_IOD_TAG);
		iod = (GF_InitialObjectDescriptor *) seng->ctx->root_od;

		i=0;
		while ((sc = gf_list_enum(seng->ctx->streams, &i))) {
			if (sc->streamType != GF_STREAM_SCENE) continue;

			if (!sc->ESID) sc->ESID = 1;

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

	count = gf_list_count(seng->ctx->streams);
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {

		j=0;
		while ((esd = gf_list_enum(seng->ctx->root_od->ESDescriptors, &j))) {
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
				e = gf_sm_setup_bifsenc(seng, sc, esd);
				break;
#endif

#ifndef GPAC_DISABLE_LASER
			case GPAC_OTI_SCENE_LASER:
				e = gf_sm_setup_lsrenc(seng, sc, esd);
				break;
#endif
			case GPAC_OTI_SCENE_DIMS:
				/* Nothing to be done here */			
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


GF_EXPORT
GF_Err gf_seng_enable_aggregation(GF_SceneEngine *seng, u16 ESID, Bool enable)
{
	GF_Err e = GF_STREAM_NOT_FOUND;
	GF_StreamContext *sc;

	if (ESID) {
		u32 i=0;
		while (sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i)) {
			if (sc->ESID==ESID) break;
		}
	} else {
		sc = (GF_StreamContext*)gf_list_get(seng->ctx->streams, 0);
	}
	if (!sc) return GF_STREAM_NOT_FOUND;

	sc->aggregation_enabled = enable;
	return GF_OK;
}


#define ZLIB_COMPRESS_SAFE	4

static GF_Err compress_dims_payload(char **data, u32 data_len, u32 *max_size, u32 offset)
{
    z_stream stream;
    int err;
    char *dest = (char *)gf_malloc(sizeof(char)*data_len*ZLIB_COMPRESS_SAFE);
    stream.next_in = (Bytef*)(*data) + offset;
    stream.avail_in = (uInt)data_len - offset;
    stream.next_out = ( Bytef*)dest;
    stream.avail_out = (uInt)data_len*ZLIB_COMPRESS_SAFE;
    stream.zalloc = (alloc_func)NULL;
    stream.zfree = (free_func)NULL;
    stream.opaque = (voidpf)NULL;

    err = deflateInit(&stream, 9);
    if (err != Z_OK) {
		gf_free(dest);
		return GF_IO_ERR;
    }

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
		gf_free(dest);
        return GF_IO_ERR;
    }
    if (data_len - offset<stream.total_out) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SceneEngine] compressed data (%d) larger than input (%d)\n", (u32) stream.total_out, (u32) data_len - offset));
    }

    if (*max_size < stream.total_out) {
		*max_size = data_len*ZLIB_COMPRESS_SAFE;
		*data = gf_realloc(*data, *max_size * sizeof(char));
    } 

    memcpy((*data) + offset, dest, sizeof(char)*stream.total_out);
    *max_size = offset + stream.total_out;
    gf_free(dest);

    deflateEnd(&stream);
    return GF_OK;
}

/* Set to 1 if you want every dump with a timed file name */
//#define DUMP_DIMS_LOG_WITH_TIME

static GF_Err gf_seng_encode_dims_au(GF_SceneEngine *seng, u16 ESID, GF_List *commands, char **data, u32 *size)
{
	GF_Err e;
	GF_SceneDumper *dumper = NULL;
	char rad_name[4096];
	char file_name[4096];
	FILE *file = NULL;
	u32 fsize;
	char *buffer = NULL;
	GF_BitStream *bs = NULL;
	u32 offset;
	u8 dims_header;
    Bool compress_dims;
#ifdef DUMP_DIMS_LOG_WITH_TIME
    u32 do_dump_with_time = 1;
#endif
	u32 buffer_len;
    char *cache_dir, *dump_name;

    if (!data) return GF_BAD_PARAM;

	e = GF_OK;

    if (!seng->dump_path) cache_dir = "C:\\Windows\\Temp";
    else cache_dir = seng->dump_path;

    dump_name = "gpac_scene_engine_dump";
	compress_dims = 1;

#ifdef DUMP_DIMS_LOG_WITH_TIME
start:    
#endif
	
	if (commands && gf_list_count(commands)) {
        sprintf(rad_name, "%s%c%s%s", cache_dir, GF_PATH_SEPARATOR, dump_name, "_update");
	} else {
#ifndef DUMP_DIMS_LOG_WITH_TIME
		sprintf(rad_name, "%s%c%s%s", cache_dir, GF_PATH_SEPARATOR, "rap_", dump_name);
#else
        char date_str[100], time_str[100];
        time_t now;
        struct tm *tm_tot;
        now = time(NULL);        
        tm_tot = localtime(&now);
        strftime(date_str, 100, "%Yy%mm%dd", tm_tot);
		strftime(time_str, 100, "%Hh%Mm%Ss", tm_tot);            
        sprintf(rad_name, "%s%c%s-%s-%s%s", cache_dir, GF_PATH_SEPARATOR, date_str, time_str, "rap_", dump_name);
#endif
	}
	dumper = gf_sm_dumper_new(seng->ctx->scene_graph, rad_name, ' ', GF_SM_DUMP_SVG);
	if (!dumper) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[SceneEngine] Cannot create SVG dumper for %s.svg\n", rad_name)); 
		e = GF_IO_ERR;
		goto exit;
	}

	if (commands && gf_list_count(commands)) {
		e = gf_sm_dump_command_list(dumper, commands, 0, 0);
	} else {
		e = gf_sm_dump_graph(dumper, 0, 0);
	}
	gf_sm_dumper_del(dumper);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[SceneEngine] Cannot dump DIMS Commands\n")); 
		goto exit;
	}
#ifdef DUMP_DIMS_LOG_WITH_TIME
    if (do_dump_with_time) {
        do_dump_with_time = 0;
        goto start;        
    }
#endif
	
	sprintf(file_name, "%s.svg", rad_name);
	file = fopen(file_name, "rb");
	if (!file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[SceneEngine] Cannot open SVG dump file %s\n", file_name)); 
		e = GF_IO_ERR;
		goto exit;
	}
	fseek(file, 0, SEEK_END);
	fsize = ftell(file);
	
	if (fsize == 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[SceneEngine] SVG dump %s is empty\n", file_name)); 
		goto exit;
	}

	/* First, read the dump in a buffer */
	buffer = gf_malloc(fsize * sizeof(char));
	fseek(file, 0, SEEK_SET);
	fread(buffer, 1, fsize, file);
	fclose(file);
    file = NULL;

	/* Then, set DIMS unit header - TODO: notify redundant units*/
	dims_header = 0;
    if (commands && gf_list_count(commands)) {
		dims_header = GF_DIMS_UNIT_P; /* streamer->all_non_rap_critical ? 0 : GF_DIMS_UNIT_P;*/
	} else {
		/*redundant RAP with complete scene*/
		dims_header = GF_DIMS_UNIT_M | GF_DIMS_UNIT_S | GF_DIMS_UNIT_I | GF_DIMS_UNIT_P;
	}

	/* Then, if compression is asked, we do it */
	buffer_len = fsize;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[SceneEngine] Sending DIMS data - sizes: raw (%d)", buffer_len)); 
	if (compress_dims) {
		dims_header |= GF_DIMS_UNIT_C;
		e = compress_dims_payload(&buffer, buffer_len, &buffer_len, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("/ compressed (%d)", buffer_len)); 
		if (e) goto exit;
	}
    GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\n")); 

	/* Then,  prepare the DIMS data using a bitstream instead of direct manipulation for endianness
           The new bitstream size should be:
		the size of the (compressed) data 
		+ 1 bytes for the header
		+ 2 bytes for the size
		+ 4 bytes if the size is greater than 65535
	 */
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE); 
	if (buffer_len > 65535) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[SceneEngine] Warning: DIMS Unit size too big !!!\n")); 
		gf_bs_write_u16(bs, 0); /* internal GPAC hack to indicate that the size is larger than 65535 */
		gf_bs_write_u32(bs, buffer_len+1);
		offset = 6;
	} else {
		gf_bs_write_u16(bs, buffer_len+1);
		offset = 2;
	}
	gf_bs_write_u8(bs, dims_header);
	gf_bs_write_data(bs, buffer, buffer_len);

	gf_free(buffer);
    buffer = NULL;

    gf_bs_get_content(bs, data, size);
	gf_bs_del(bs);

exit:
    if (buffer) gf_free(buffer);
    if (file) fclose(file);
	return e;
}

static Bool gf_sm_check_for_modif(GF_AUContext *au)
{
	GF_Command *com;
	Bool modified=0;
	u32 i=0;
	if (au->flags & GF_SM_AU_MODIFIED) {
		au->flags &= ~GF_SM_AU_MODIFIED;
		modified=1;
	}
	while (com = gf_list_enum(au->commands, &i)) {
		if (com->node && gf_node_dirty_get(com->node)) {
			modified=1;
			gf_node_dirty_reset(com->node);
		}
	}
	return modified;
}

static GF_Err gf_sm_live_encode_scene_au(GF_SceneEngine *seng, gf_seng_callback callback, Bool from_start)
{
	GF_Err e;
	u32	i, j, size, count, nb_streams;
	char *data;
	GF_AUContext *au;

	if (!callback) return GF_BAD_PARAM;

	e = GF_OK;

	nb_streams = gf_list_count(seng->ctx->streams);
	for (i=0; i<nb_streams;i++) {
		GF_StreamContext *sc = gf_list_get(seng->ctx->streams, i);
		if (sc->streamType != GF_STREAM_SCENE) continue;

		count = gf_list_count(sc->AUs);
		j = from_start ? 0 : sc->current_au_count;
		for (; j<count; j++) {
			au = (GF_AUContext *)gf_list_get(sc->AUs, j);
			data = NULL;
			size = 0;
			/*in case using XMT*/
			if (au->timing_sec) au->timing = (u64) (au->timing_sec * sc->timeScale);

			if (from_start && !j && !gf_sm_check_for_modif(au)) continue;

			switch (sc->objectType) {
#ifndef GPAC_DISABLE_BIFS_ENC
			case GPAC_OTI_SCENE_BIFS:
			case GPAC_OTI_SCENE_BIFS_V2:
				e = gf_bifs_encode_au(seng->bifsenc, sc->ESID, au->commands, &data, &size);
				break;
#endif

#ifndef GPAC_DISABLE_BIFS_ENC
			case GPAC_OTI_SCENE_LASER:
				e = gf_laser_encode_au(seng->lsrenc, sc->ESID, au->commands, 0, &data, &size);
				break;
#endif
			case GPAC_OTI_SCENE_DIMS:
                e = gf_seng_encode_dims_au(seng, sc->ESID, au->commands, &data, &size);
				break;

			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("Cannot encode AU for Scene OTI %x\n", sc->objectType));
				break;
			}
			callback(seng->calling_object, sc->ESID, data, size, au->timing);
			gf_free(data);
			data = NULL;
			if (e) break;
		}
	}
	return e;
}

GF_EXPORT
GF_Err gf_seng_aggregate_context(GF_SceneEngine *seng, u16 ESID)
{
	return gf_sm_aggregate(seng->ctx, ESID);
}

GF_EXPORT
GF_Err gf_seng_save_context(GF_SceneEngine *seng, char *ctxFileName)
{
	u32	d_mode, do_enc;
	char szF[GF_MAX_PATH], *ext;
	GF_Err	e;

	/*check if we dump to BT, XMT or encode to MP4*/
	ext = NULL;
	if (ctxFileName) {
		strcpy(szF, ctxFileName);
		ext = strrchr(szF, '.');
	}
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
		e = gf_sm_encode_to_file(seng->ctx, mp4, NULL);
		if (e) gf_isom_delete(mp4);
		else gf_isom_close(mp4);
#else
		return GF_NOT_SUPPORTED;
#endif
	} else {
		e = gf_sm_dump(seng->ctx, ctxFileName ? szF : NULL, d_mode);
	}
	return e;
}

static GF_AUContext *gf_seng_create_new_au(GF_StreamContext *sc, u32 time) 
{
    GF_AUContext *new_au, *last_au;
    last_au = gf_list_last(sc->AUs);
    if (last_au && last_au->timing == time) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[SceneEngine] Forcing new AU\n"));
        time++;
    }
    new_au = gf_sm_stream_au_new(sc, time, 0, 0);
    return new_au;
}

GF_EXPORT
GF_Err gf_seng_encode_from_string(GF_SceneEngine *seng, u16 ESID, Bool disable_aggregation, char *auString, gf_seng_callback callback)
{
	GF_StreamContext *sc;
	u32 i, count;
	GF_Err e;
	
	count = gf_list_count(seng->ctx->streams);
	i = 0;
	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {
		sc->current_au_count = gf_list_count(sc->AUs);
		sc->disable_aggregation = disable_aggregation;
	}
	seng->loader.flags |= GF_SM_LOAD_CONTEXT_READY;
	seng->loader.force_es_id = ESID;
    
    /* We need to create an empty AU for the parser to correctly parse a LASeR Command without SceneUnit */
    sc = gf_list_get(seng->ctx->streams, 0);
    if (sc->objectType == GPAC_OTI_SCENE_DIMS) {
        gf_seng_create_new_au(sc, 0);
    }

	e = gf_sm_load_string(&seng->loader, auString, 0);
	if (e) goto exit;

	i = 0;
	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {
		sc->disable_aggregation = 0;
	}
	e = gf_sm_live_encode_scene_au(seng, callback, 0); 
exit:
	return e;
}


GF_EXPORT
GF_Err gf_seng_encode_from_commands(GF_SceneEngine *seng, u16 ESID, Bool disable_aggregation, u32 time, GF_List *commands, gf_seng_callback callback)
{
	GF_Err e;
	u32	size;
	char *data;
    GF_StreamContext *sc;
	u32	i, nb_streams;
    GF_AUContext *new_au;

	if (!callback) return GF_BAD_PARAM;
    if (!commands || !gf_list_count(commands)) return GF_BAD_PARAM;

	e = GF_OK;
    
    /* if the ESID is not provided we try to use the first scene stream */
    sc = NULL;
    nb_streams = gf_list_count(seng->ctx->streams);
    for (i=0; i<nb_streams;i++) {
	    GF_StreamContext *tmp_sc = gf_list_get(seng->ctx->streams, i);
	    if (tmp_sc->streamType != GF_STREAM_SCENE) continue;
        sc = tmp_sc;
        if (!ESID) break;
        else if (sc->ESID == ESID) break;
    }
    if (!sc) return GF_BAD_PARAM;
    /* We need to create an empty AU for the parser to correctly parse a LASeR Command without SceneUnit */
	new_au = gf_seng_create_new_au(sc, time);

	if (disable_aggregation) new_au->flags = GF_SM_AU_NOT_AGGREGATED;


	
	/* Removing the commands from the input list to avoid destruction
       and setting the RAP flag */
    while (gf_list_count(commands)) {
        GF_Command *com = gf_list_get(commands, 0);
        gf_list_rem(commands, 0);
        switch (com->tag) {
            case GF_SG_SCENE_REPLACE:
            case GF_SG_LSR_NEW_SCENE:
				new_au->flags |= GF_SM_AU_RAP;
                break;
        }
        gf_list_add(new_au->commands, com);
    }

    data = NULL;
	size = 0;

    switch(sc->objectType) {
#ifndef GPAC_DISABLE_BIFS_ENC
		case GPAC_OTI_SCENE_BIFS:
		case GPAC_OTI_SCENE_BIFS_V2:
			e = gf_bifs_encode_au(seng->bifsenc, ESID, new_au->commands, &data, &size);
			break;
#endif

#ifndef GPAC_DISABLE_BIFS_ENC
		case GPAC_OTI_SCENE_LASER:
			e = gf_laser_encode_au(seng->lsrenc, ESID, new_au->commands, 0, &data, &size);
			break;
#endif
		case GPAC_OTI_SCENE_DIMS:
            e = gf_seng_encode_dims_au(seng, ESID, new_au->commands, &data, &size);
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("Cannot encode commands for Scene OTI %x\n", sc->objectType));
			break;
    }
    callback(seng->calling_object, ESID, data, size, 0);
	gf_free(data);
	return e;
}

GF_EXPORT
GF_Err gf_seng_encode_from_file(GF_SceneEngine *seng, u16 ESID, Bool disable_aggregation, char *auFile, gf_seng_callback callback)
{
	GF_Err e;
	GF_StreamContext *sc;
	u32 i, count;
	Bool dims = 0;

	seng->loader.fileName = auFile;
	seng->loader.ctx = seng->ctx;
	seng->loader.force_es_id = ESID;

	sc = NULL;
	count = gf_list_count(seng->ctx->streams);
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {
		sc->current_au_count = gf_list_count(sc->AUs);
		sc->disable_aggregation = disable_aggregation;
	}
    /* We need to create an empty AU for the parser to correctly parse a LASeR Command without SceneUnit */
    sc = gf_list_get(seng->ctx->streams, 0);
    if (sc->objectType == GPAC_OTI_SCENE_DIMS) {
		dims = 1;
        gf_seng_create_new_au(sc, 0);
    }
	seng->loader.flags |= GF_SM_LOAD_CONTEXT_READY;

	if (dims) {
		seng->loader.type |= GF_SM_LOAD_DIMS;
	} else {
		seng->loader.flags |= GF_SM_LOAD_MPEG4_STRICT;
	}
	e = gf_sm_load_run(&seng->loader);

	if (e<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] cannot load AU File %s (error %s)\n", auFile, gf_error_to_string(e)));
		goto exit;
	}

	i = 0;
	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {
		sc->disable_aggregation = 0;
	}

	e = gf_sm_live_encode_scene_au(seng, callback, 0); 
	if (e) goto exit;
exit:
	return e;
}

GF_EXPORT
GF_Err gf_seng_encode_context(GF_SceneEngine *seng, gf_seng_callback callback)
{
	if (!seng) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] Cannot encode context. No seng provided\n"));
		return GF_BAD_PARAM;
	}
	return gf_sm_live_encode_scene_au(seng, callback, 1);
} 

GF_EXPORT
void gf_seng_terminate(GF_SceneEngine *seng)
{
#ifndef GPAC_DISABLE_BIFS_ENC
	if (seng->bifsenc) gf_bifs_encoder_del(seng->bifsenc);
#endif

#ifndef GPAC_DISABLE_BIFS_ENC
	if (seng->lsrenc) gf_laser_encoder_del(seng->lsrenc);
#endif

	gf_sm_load_done(&seng->loader);

	if (seng->owns_context) {
		if (seng->ctx) gf_sm_del(seng->ctx);
		if (seng->sg) gf_sg_del(seng->sg);
	}
	gf_free(seng);
}

GF_EXPORT
GF_Err gf_seng_get_stream_config(GF_SceneEngine *seng, u32 idx, u16 *ESID, const char **config, u32 *config_len, u32 *streamType, u32 *objectType, u32 *timeScale)
{
	GF_StreamContext *sc = gf_list_get(seng->ctx->streams, idx);
	if (!sc || !ESID || !config || !config_len) return GF_BAD_PARAM;
	*ESID = sc->ESID;
	*config = sc->dec_cfg;
	*config_len = sc->dec_cfg_len;
	if (streamType) *streamType = sc->streamType;
	if (objectType) *objectType = sc->objectType;
	if (timeScale) *timeScale = sc->timeScale;
	return GF_OK;
}

static void gf_seng_on_node_modified(void *user_priv, u32 type, GF_Node *node, void *ctxdata)
{
	if (type==GF_SG_CALLBACK_MODIFIED) {
		gf_node_dirty_parents(node);
	}
}

GF_EXPORT
GF_SceneEngine *gf_seng_init(void *calling_object, char * inputContext, u32 load_type, char *dump_path, Bool embed_resources)
{
	GF_SceneEngine *seng;
	GF_Err e = GF_OK;

	if (!inputContext) return NULL;

	GF_SAFEALLOC(seng, GF_SceneEngine)
	if (!seng) return NULL;

	seng->calling_object = calling_object;

	/*Step 1: create context and load input*/
	seng->sg = gf_sg_new();
	gf_sg_set_node_callback(seng->sg, gf_seng_on_node_modified);
    seng->dump_path = dump_path;
	seng->ctx = gf_sm_new(seng->sg);
	seng->owns_context = 1;
	memset(&(seng->loader), 0, sizeof(GF_SceneLoader));
	seng->loader.ctx = seng->ctx;
    seng->loader.type = load_type;
	/*since we're encoding in BIFS we must get MPEG-4 nodes only*/
	seng->loader.flags = GF_SM_LOAD_MPEG4_STRICT;
	if (embed_resources) seng->loader.flags |= GF_SM_LOAD_EMBEDS_RES;

	seng->loader.fileName = inputContext;
	e = gf_sm_load_init(&(seng->loader));
	if (!e) e = gf_sm_load_run(&(seng->loader));

	if (e<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] Cannot load context from %s (error %s)\n", inputContext, gf_error_to_string(e)));
		goto exit;
	}
	e = gf_sm_live_setup(seng);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] cannot init scene encoder for context (error %s)\n", gf_error_to_string(e)));
		goto exit;
	}
	return seng;

exit:
	gf_seng_terminate(seng);
	return NULL;
}

GF_EXPORT
GF_SceneEngine *gf_seng_init_from_context(void *calling_object, GF_SceneManager *ctx, char *dump_path)
{
	GF_SceneEngine *seng;
	GF_Err e = GF_OK;

	if (!ctx) return NULL;

	GF_SAFEALLOC(seng, GF_SceneEngine)
	if (!seng) return NULL;

	seng->calling_object = calling_object;
    seng->dump_path = dump_path;
	/*Step 1: create context and load input*/
	seng->sg = ctx->scene_graph;
	seng->ctx = ctx;
	seng->owns_context = 0;

	e = gf_sm_live_setup(seng);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] cannot init scene encoder for context (error %s)\n", gf_error_to_string(e)));
		goto exit;
	}
	return seng;

exit:
	gf_seng_terminate(seng);
	return NULL;
}

GF_EXPORT
GF_SceneEngine *gf_seng_init_from_string(void *calling_object, char * inputContext, u32 load_type, u32 width, u32 height, Bool usePixelMetrics, char *dump_path)
{
	GF_SceneEngine *seng;
	GF_Err e = GF_OK;

	if (!inputContext) return NULL;

	GF_SAFEALLOC(seng, GF_SceneEngine)
	if (!seng) return NULL;

	seng->calling_object = calling_object;
    seng->dump_path = dump_path;
    /*Step 1: create context and load input*/
	seng->sg = gf_sg_new();
	seng->ctx = gf_sm_new(seng->sg);
	seng->owns_context = 1;
	memset(& seng->loader, 0, sizeof(GF_SceneLoader));
	seng->loader.ctx = seng->ctx;
    seng->loader.type = load_type;
	/*since we're encoding in BIFS we must get MPEG-4 nodes only*/
	seng->loader.flags = GF_SM_LOAD_MPEG4_STRICT;

    /* assign a loader type only if it was not requested (e.g. DIMS should not be overriden by SVG) */
    if (!seng->loader.type) {
        if (inputContext[0] == '<') {
		    if (strstr(inputContext, "<svg ")) seng->loader.type = GF_SM_LOAD_SVG;
		    else if (strstr(inputContext, "<saf ")) seng->loader.type = GF_SM_LOAD_XSR;
		    else if (strstr(inputContext, "XMT-A") || strstr(inputContext, "X3D")) seng->loader.type = GF_SM_LOAD_XMTA;
	    } else {
		    seng->loader.type = GF_SM_LOAD_BT;
	    }
    }
	e = gf_sm_load_string(&seng->loader, inputContext, 0);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] cannot load context from %s (error %s)\n", inputContext, gf_error_to_string(e)));
		goto exit;
	}
	if (!seng->ctx->root_od) {
		seng->ctx->is_pixel_metrics = usePixelMetrics;
		seng->ctx->scene_width = width;
		seng->ctx->scene_height = height;
	}

	e = gf_sm_live_setup(seng);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[SceneEngine] cannot init scene encoder for context (error %s)\n", gf_error_to_string(e)));
		goto exit;
	}
	return seng;

exit:
	gf_seng_terminate(seng);
	return NULL;
}

GF_EXPORT
u32 gf_seng_get_stream_count(GF_SceneEngine *seng)
{
	return gf_list_count(seng->ctx->streams);
}

GF_EXPORT
u32 gf_seng_get_stream_info(GF_SceneEngine *seng, u32 i)
{
    GF_StreamContext *sc;
    sc = gf_list_get(seng->ctx->streams, i);
    return sc->streamType;
}

GF_EXPORT
char *gf_seng_get_base64_iod(GF_SceneEngine *seng)
{
	u32 size, size64;
    char *buffer, *buf64;
	u32 i=0;
	GF_StreamContext*sc = NULL;

	if (!seng->ctx->root_od) return NULL;

	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {
		if ((sc->streamType == GF_STREAM_SCENE) && (sc->objectType != GPAC_OTI_SCENE_DIMS))
			break;
	}
	if (!sc) return NULL;

    size = 0;
    gf_odf_desc_write((GF_Descriptor *) seng->ctx->root_od, &buffer, &size);
    buf64 = gf_malloc(size*2);
    size64 = gf_base64_encode( buffer, size, buf64, size*2);
    buf64[size64] = 0;
    gf_free(buffer);
	return buf64;
}

GF_EXPORT
GF_Descriptor *gf_seng_get_iod(GF_SceneEngine *seng)
{
	u32 i=0;
	GF_Descriptor *out_iod = NULL;
	GF_StreamContext*sc = NULL;

	if (!seng->ctx->root_od) return NULL;
	while ((sc = (GF_StreamContext*)gf_list_enum(seng->ctx->streams, &i))) {
		if ((sc->streamType == GF_STREAM_SCENE) && (sc->objectType != GPAC_OTI_SCENE_DIMS))
			break;
	}
	if (!sc) return NULL;
	gf_odf_desc_copy((GF_Descriptor *)seng->ctx->root_od, &out_iod);
	return out_iod;
}


#endif /*GPAC_DISABLE_SENG*/

