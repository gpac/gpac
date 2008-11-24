/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
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


#include <gpac/internal/terminal_dev.h>
#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <zlib.h>

#ifndef GPAC_DISABLE_SVG

typedef struct
{
	GF_SceneLoader loader;
	GF_InlineScene *inline_scene;
	u8 oti;
	char *file_name;
	u32 file_size;
	u32 sax_max_duration;
	u16 base_es_id;
	u32 file_pos;
	gzFile src;
} SVGIn;

static Bool svg_check_download(SVGIn *svgin)
{
	u32 size;
	FILE *f = fopen(svgin->file_name, "rb");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	if (size==svgin->file_size) return 1;
	return 0;
}

#define SVG_PROGRESSIVE_BUFFER_SIZE		4096

static GF_Err svgin_deflate(SVGIn *svgin, char *buffer, u32 buffer_len)
{
	GF_Err e;
	char svg_data[2049];
	int err;
	u32 done = 0;
	z_stream d_stream;
	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;
	d_stream.next_in  = (Bytef*)buffer;
	d_stream.avail_in = buffer_len;
	d_stream.next_out = (Bytef*)svg_data;
	d_stream.avail_out = 2048;

	err = inflateInit(&d_stream);
	if (err == Z_OK) {
		while (d_stream.total_in < buffer_len) {
			err = inflate(&d_stream, Z_NO_FLUSH);
			if (err < Z_OK) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
			svg_data[d_stream.total_out - done] = 0;
			e = gf_sm_load_string(&svgin->loader, svg_data, 0);
			if (e || (err== Z_STREAM_END)) break;
			done = d_stream.total_out;
			d_stream.avail_out = 2048;
			d_stream.next_out = (Bytef*)svg_data;
		}
		inflateEnd(&d_stream);
		return GF_OK;
	}
	return GF_NON_COMPLIANT_BITSTREAM;
}

static GF_Err SVG_ProcessData(GF_SceneDecoder *plug, char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	SVGIn *svgin = (SVGIn *)plug->privateStack;

	if (stream_time==(u32)-1) {
		if (svgin->src) gzclose(svgin->src);
		svgin->src = NULL;
		gf_sm_load_done(&svgin->loader);
		svgin->loader.fileName = NULL;
		svgin->file_pos = 0;
		gf_sg_reset(svgin->inline_scene->graph);
		return GF_OK;
	}

	switch (svgin->oti) {
	/*!OTI for SVG dummy stream (dsi = file name) - GPAC internal*/
	case GPAC_OTI_PRIVATE_SCENE_SVG:
		/*full doc parsing*/
		if ((svgin->sax_max_duration==(u32) -1) && svgin->file_size) {
			/*init step*/
			if (!svgin->loader.fileName) {
				/*not done yet*/
				if (!svg_check_download(svgin)) return GF_OK;
				svgin->loader.fileName = svgin->file_name;
				e = gf_sm_load_init(&svgin->loader);
			} else {
				e = gf_sm_load_run(&svgin->loader);
			}
		}
		/*chunk parsing*/
		else {
			u32 entry_time;
			char file_buf[SVG_PROGRESSIVE_BUFFER_SIZE+2];
			/*initial load*/
			if (!svgin->src && !svgin->file_pos) {
				svgin->src = gzopen(svgin->file_name, "rb");
				if (!svgin->src) return GF_URL_ERROR;
				svgin->loader.fileName = svgin->file_name;
			}
			e = GF_OK;
			entry_time = gf_sys_clock();
			
			while (1) {
				u32 diff;
				s32 nb_read;
				nb_read = gzread(svgin->src, file_buf, SVG_PROGRESSIVE_BUFFER_SIZE);
				/*we may have read nothing but we still need to call parse in case the parser got suspended*/
				if (nb_read<=0) {
					nb_read = 0;
					e = gf_sm_load_run(&svgin->loader);
					if ((e==GF_EOS) && gzeof(svgin->src)) {
						gf_set_progress("SVG Parsing", svgin->file_pos, svgin->file_size);
						gzclose(svgin->src);
						svgin->src = NULL;
					}
					goto exit;
				}

				file_buf[nb_read] = file_buf[nb_read+1] = 0;

				e = gf_sm_load_string(&svgin->loader, file_buf, 0);
				svgin->file_pos += nb_read;



				/*handle decompression*/
				if (svgin->file_pos > svgin->file_size) svgin->file_size = svgin->file_pos + 1;
				if (e) break;

				diff = gf_sys_clock() - entry_time;
				gf_set_progress("SVG Parsing", svgin->file_pos, svgin->file_size);
				if (diff > svgin->sax_max_duration) break;
			}
		}
		break;

	/*!OTI for streaming SVG - GPAC internal*/
	case GPAC_OTI_SCENE_SVG:
		e = gf_sm_load_string(&svgin->loader, inBuffer, 0);
		break;

	/*!OTI for streaming SVG + gz - GPAC internal*/
	case GPAC_OTI_SCENE_SVG_GZ:
		e = svgin_deflate(svgin, inBuffer, inBufferLength);
		break;

	/*!OTI for DIMS (dsi = 3GPP DIMS configuration) - GPAC internal*/
	case GPAC_OTI_SCENE_DIMS:
		{
			u8 prev, dims_hdr;
			u32 nb_bytes, pos, size;
			GF_BitStream *bs = gf_bs_new(inBuffer, inBufferLength, GF_BITSTREAM_READ);
//
//			FILE *f = fopen("dump.svg", "wb");
//
			while (gf_bs_available(bs)) {
				pos = (u32) gf_bs_get_position(bs);
				size = gf_bs_read_u16(bs);
				nb_bytes = 2;
				/*GPAC internal hack*/
				if (!size) {
					size = gf_bs_read_u32(bs);
					nb_bytes = 6;
				}
//	            fwrite( inBuffer + pos + nb_bytes + 1, 1, size - 1, f );   

				dims_hdr = gf_bs_read_u8(bs);
				prev = inBuffer[pos + nb_bytes + size];

				inBuffer[pos + nb_bytes + size] = 0;
				if (dims_hdr & GF_DIMS_UNIT_C) {
					e = svgin_deflate(svgin, inBuffer + pos + nb_bytes + 1, size - 1);
				} else {
					e = gf_sm_load_string(&svgin->loader, inBuffer + pos + nb_bytes + 1, 0);
				}
				inBuffer[pos + nb_bytes + size] = prev;
				gf_bs_skip_bytes(bs, size-1);

			}
//          fclose(f);    
			gf_bs_del(bs);
		}
		break;
		
	default: 
		return GF_BAD_PARAM;
	}

exit:
	if ((svgin->inline_scene->graph_attached!=1) && (gf_sg_get_root_node(svgin->loader.scene_graph)!=NULL) ) {
		gf_inline_attach_to_compositor(svgin->inline_scene);
	}
	/*prepare for next playback*/
	if (e==GF_EOS) {
		gf_sm_load_done(&svgin->loader);
		svgin->loader.fileName = NULL;
	}
	return e;
}



static GF_Err SVG_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_scene_decoder)
{
	SVGIn *svgin = (SVGIn *)plug->privateStack;
	memset(&svgin->loader, 0, sizeof(GF_SceneLoader));
	svgin->loader.is = scene;
	svgin->inline_scene = scene;
	svgin->loader.scene_graph = scene->graph;
	svgin->loader.localPath = gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
	/*Warning: svgin->loader.type may be overriden in attach stream */
	svgin->loader.type = GF_SM_LOAD_SVG_DA;
	svgin->loader.flags = GF_SM_LOAD_FOR_PLAYBACK;
	return GF_OK;
}

static GF_Err SVG_ReleaseScene(GF_SceneDecoder *plug)
{
	return GF_OK;
}

static GF_Err SVG_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	const char *sOpt;
	GF_BitStream *bs;
	SVGIn *svgin = (SVGIn *)plug->privateStack;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;

	svgin->loader.type = GF_SM_LOAD_SVG_DA;
	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_SCENE_SVG:
	case GPAC_OTI_SCENE_SVG_GZ:
		svgin->loader.flags |= GF_SM_LOAD_CONTEXT_STREAMING;
		/*no decSpecInfo defined for streaming svg yet*/
		break;
	case GPAC_OTI_SCENE_DIMS:
		svgin->loader.type = GF_SM_LOAD_DIMS;
		svgin->loader.flags |= GF_SM_LOAD_CONTEXT_STREAMING;
		/*decSpecInfo not yet supported for DIMS svg - we need properties at the scene level to store the 
		various indications*/
		break;
	case GPAC_OTI_PRIVATE_SCENE_SVG:
	default:
		if (!esd->decoderConfig->decoderSpecificInfo) return GF_NON_COMPLIANT_BITSTREAM;
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		svgin->file_size = gf_bs_read_u32(bs);
		svgin->file_pos = 0;
		gf_bs_del(bs);
		svgin->file_name =  (char *) malloc(sizeof(char)*(1 + esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32)) );
		memcpy(svgin->file_name, esd->decoderConfig->decoderSpecificInfo->data + sizeof(u32), esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32) );
		svgin->file_name[esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32) ] = 0;
		break;
	}
	svgin->oti = esd->decoderConfig->objectTypeIndication;

	if (!esd->dependsOnESID) svgin->base_es_id = esd->ESID;

	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "SAXLoader", "Progressive");
	if (sOpt && !strcmp(sOpt, "yes")) {
		svgin->sax_max_duration = 30;
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "SAXLoader", "MaxDuration");
		if (sOpt) {
			svgin->sax_max_duration = atoi(sOpt);
		} else {
			svgin->sax_max_duration = 30;
			gf_modules_set_option((GF_BaseInterface *)plug, "SAXLoader", "MaxDuration", "30");
		}
	} else {
		svgin->sax_max_duration = (u32) -1;
	}
	return GF_OK;
}

static GF_Err SVG_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	SVGIn *svgin = (SVGIn *)plug->privateStack;
	if (svgin->file_name) free(svgin->file_name);
	svgin->file_name = NULL;
	gf_sm_load_done(&svgin->loader);
	return GF_OK;
}

const char *SVG_GetName(struct _basedecoder *plug)
{
	SVGIn *svgin = (SVGIn *)plug->privateStack;
	if (svgin->oti==GPAC_OTI_PRIVATE_SCENE_SVG) return ((svgin->sax_max_duration==(u32)-1) && svgin->file_size) ? "GPAC SVG SAX Parser" : "GPAC SVG Progressive Parser";
	if (svgin->oti==GPAC_OTI_SCENE_SVG) return "GPAC Streaming SVG Parser";
	if (svgin->oti==GPAC_OTI_SCENE_SVG_GZ) return "GPAC Streaming SVGZ Parser";
	if (svgin->oti==GPAC_OTI_SCENE_DIMS) return "GPAC DIMS Parser";	
	return "INTERNAL ERROR";
}

Bool SVG_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if (StreamType==GF_STREAM_PRIVATE_SCENE) {
		if (ObjectType==GPAC_OTI_PRIVATE_SCENE_SVG) return 1;
		return 0;
	} else if (StreamType==GF_STREAM_SCENE) {
		if (ObjectType==GPAC_OTI_SCENE_SVG) return 1;
		if (ObjectType==GPAC_OTI_SCENE_SVG_GZ) return 1;
		if (ObjectType==GPAC_OTI_SCENE_DIMS) return 1;	
		return 0;
	}
	return 0;
}

static GF_Err SVG_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	if (cap->CapCode==GF_CODEC_PADDING_BYTES) {
		/* Adding one byte of padding for \r\n problems*/
		cap->cap.valueInt = 1;
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static GF_Err SVG_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

/*interface create*/
GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	SVGIn *svgin;
	GF_SceneDecoder *sdec;

	if (InterfaceType != GF_SCENE_DECODER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sdec, GF_SceneDecoder)
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC SVG Parser", "gpac distribution");

	GF_SAFEALLOC(svgin, SVGIn);
	sdec->privateStack = svgin;
	sdec->AttachStream = SVG_AttachStream;
	sdec->CanHandleStream = SVG_CanHandleStream;
	sdec->DetachStream = SVG_DetachStream;
	sdec->AttachScene = SVG_AttachScene;
	sdec->ReleaseScene = SVG_ReleaseScene;
	sdec->ProcessData = SVG_ProcessData;
	sdec->GetName = SVG_GetName;
	sdec->SetCapabilities = SVG_SetCapabilities;
	sdec->GetCapabilities = SVG_GetCapabilities;
	return (GF_BaseInterface *)sdec;
}


/*interface destroy*/
GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_SceneDecoder *sdec = (GF_SceneDecoder *)ifce;
	SVGIn *svgin = (SVGIn *) sdec->privateStack;
	if (sdec->InterfaceType != GF_SCENE_DECODER_INTERFACE) return;

	free(svgin);
	free(sdec);
}

/*interface query*/
GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_SCENE_DECODER_INTERFACE) return 1;
	return 0;
}
#else


/*interface create*/
GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	return NULL;
}


/*interface destroy*/
GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
}

/*interface query*/
GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	return 0;
}
#endif
