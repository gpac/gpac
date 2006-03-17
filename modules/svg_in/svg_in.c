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

enum {
	/*defined by dummy_in plugin*/
	SVG_IN_OTI_SVG = 2,
	/*defined by dummy_in plugin*/
	SVG_IN_OTI_LASERML = 3,
	/*defined by ourselves - streamType 3 (scene description) for SVG streaming*/
	SVG_IN_OTI_STREAMING_SVG	  = 10,
	/*defined by ourselves - streamType 3 (scene description) for SVG streaming*/
	SVG_IN_OTI_STREAMING_SVG_GZ	  = 11
};

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
	gzFile *src;
	Bool attached;
} SVGIn;

static Bool svg_check_download(SVGIn *svgin)
{
	u32 size;
	FILE *f = fopen(svgin->file_name, "rb");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	if (size==svgin->file_size) return 1;
	fprintf(stdout, "dnld not complete %d / %d\n", size, svgin->file_size);
	return 0;
}

static void SVG_OnMessage(void *cbk, char *szMsg, GF_Err e)
{
	SVGIn *svgin = (SVGIn*)cbk;
	gf_term_message(svgin->inline_scene->root_od->term, svgin->inline_scene->root_od->net_service->url, szMsg, e);
}

static void SVG_OnProgress(void *cbk, u32 done, u32 tot)
{
	GF_Event evt;
	SVGIn *svgin = (SVGIn*)cbk;
	evt.type = GF_EVT_PROGRESS;
	evt.progress.progress_type = 2;
	evt.progress.done = done;
	evt.progress.total = tot;
	evt.progress.service = svgin->inline_scene->root_od->net_service->url;
	GF_USER_SENDEVENT(svgin->inline_scene->root_od->term->user, &evt);
}

#define SVG_PROGRESSIVE_BUFFER_SIZE		4096

static GF_Err SVG_ProcessData(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	SVGIn *svgin = plug->privateStack;
	switch (svgin->oti) {
	case SVG_IN_OTI_SVG:
		/*full doc parsing*/
		if ((svgin->sax_max_duration==(u32) -1) && svgin->file_size) {
			/*init step*/
			if (!svgin->loader.fileName) {
				svgin->loader.fileName = svgin->file_name;
				/*not done yet*/
				if (!svg_check_download(svgin)) return GF_OK;
				e = gf_sm_load_init(&svgin->loader);
			} else {
				/*should not be needed since SVG parser loads the entire file for now*/
				e = /*gf_sm_load_run(&svgin->loader)*/GF_EOS;
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
				u32 diff, nb_read;
				nb_read = gzread(svgin->src, file_buf, SVG_PROGRESSIVE_BUFFER_SIZE);
				file_buf[nb_read] = file_buf[nb_read+1] = 0;
				if (!nb_read) {
					if (gzeof(svgin->src)) {
						SVG_OnProgress(svgin, svgin->file_pos, svgin->file_size);
						gzclose(svgin->src);
						svgin->src = NULL;
						e = GF_EOS;
						goto exit;
					}
					break;
				}

				e = gf_sm_load_string(&svgin->loader, file_buf);
				svgin->file_pos += nb_read;
				/*handle decompression*/
				if (svgin->file_pos > svgin->file_size) svgin->file_size = svgin->file_pos + 1;
				if (e) break;

				diff = gf_sys_clock() - entry_time;
				SVG_OnProgress(svgin, svgin->file_pos, svgin->file_size);
				if (diff > svgin->sax_max_duration) break;
			}
		}
		break;
	case SVG_IN_OTI_STREAMING_SVG:
		e = gf_sm_load_string(&svgin->loader, inBuffer);
		break;
	case SVG_IN_OTI_STREAMING_SVG_GZ:
	{
		char svg_data[2049];
		int err;
		u32 done = 0;
		z_stream d_stream;
		d_stream.zalloc = (alloc_func)0;
		d_stream.zfree = (free_func)0;
		d_stream.opaque = (voidpf)0;
		d_stream.next_in  = inBuffer;
		d_stream.avail_in = inBufferLength;
		d_stream.next_out = svg_data;
		d_stream.avail_out = 2048;

		err = inflateInit(&d_stream);
		if (err == Z_OK) {
			while (d_stream.total_in < inBufferLength) {
				err = inflate(&d_stream, Z_NO_FLUSH);
				if (err < Z_OK) {
					e = GF_NON_COMPLIANT_BITSTREAM;
					break;
				}
				svg_data[d_stream.total_out - done] = 0;
				e = gf_sm_load_string(&svgin->loader, svg_data);
				if (e || (err== Z_STREAM_END)) break;
				done = d_stream.total_out;
				d_stream.avail_out = 2048;
				d_stream.next_out = svg_data;
			}
			inflateEnd(&d_stream);
		}
	}
		break;
	case SVG_IN_OTI_LASERML: return GF_NOT_SUPPORTED;
	default: return GF_BAD_PARAM;
	}

exit:
	if (!svgin->attached && (gf_sg_get_root_node(svgin->loader.scene_graph)!=NULL) ) {
		gf_is_attach_to_renderer(svgin->inline_scene);
		svgin->attached = 1;
	}
	return e;
}



static GF_Err SVG_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_scene_decoder)
{
	SVGIn *svgin = plug->privateStack;
	memset(&svgin->loader, 0, sizeof(GF_SceneLoader));
	svgin->inline_scene = scene;
	svgin->loader.scene_graph = scene->graph;
	svgin->loader.localPath = gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
	svgin->loader.type = GF_SM_LOAD_SVG;
	svgin->loader.OnMessage = SVG_OnMessage;
	svgin->loader.OnProgress = SVG_OnProgress;
	svgin->loader.cbk = svgin;
	svgin->loader.flags = GF_SM_LOAD_FOR_PLAYBACK;
	return GF_OK;
}

static GF_Err SVG_ReleaseScene(GF_SceneDecoder *plug)
{
	return GF_OK;
}

static GF_Err SVG_AttachStream(GF_BaseDecoder *plug, 
									 u16 ES_ID, 
									 unsigned char *decSpecInfo, 
									 u32 decSpecInfoSize, 
									 u16 DependsOnES_ID,
									 u32 objectTypeIndication, 
									 Bool Upstream)
{
	const char *sOpt;
	GF_BitStream *bs;
	SVGIn *svgin = plug->privateStack;
	if (Upstream) return GF_NOT_SUPPORTED;

	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	switch (objectTypeIndication) {
	case SVG_IN_OTI_STREAMING_SVG:
	case SVG_IN_OTI_STREAMING_SVG_GZ:
		/*no decSpecInfo defined for streaming svg yet*/
		break;
	default:
		if (!decSpecInfo) return GF_NON_COMPLIANT_BITSTREAM;
		bs = gf_bs_new(decSpecInfo, decSpecInfoSize, GF_BITSTREAM_READ);
		svgin->file_size = gf_bs_read_u32(bs);
		svgin->file_pos = 0;
		gf_bs_del(bs);
		GF_SAFEALLOC(svgin->file_name, sizeof(char)*(1 + decSpecInfoSize - sizeof(u32)) );
		memcpy(svgin->file_name, decSpecInfo + sizeof(u32), decSpecInfoSize - sizeof(u32) );
		break;
	}
	svgin->oti = objectTypeIndication;
	if (!DependsOnES_ID) svgin->base_es_id = ES_ID;

	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "SAXLoader", "Progressive");
	if (sOpt && !strcmp(sOpt, "yes")) {
		svgin->sax_max_duration = 30;
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "SAXLoader", "MaxDuration");
		if (sOpt) svgin->sax_max_duration = atoi(sOpt);
	} else {
		svgin->sax_max_duration = (u32) -1;
	}
	return GF_OK;
}

static GF_Err SVG_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	SVGIn *svgin = plug->privateStack;
	if (svgin->file_name) free(svgin->file_name);
	svgin->file_name = NULL;
	gf_sm_load_done(&svgin->loader);
	return GF_OK;
}

const char *SVG_GetName(struct _basedecoder *plug)
{
	SVGIn *svgin = plug->privateStack;
	if (svgin->oti==SVG_IN_OTI_SVG) return ((svgin->sax_max_duration==(u32)-1) && svgin->file_size) ? "GPAC SVG SAX Parser" : "GPAC SVG Progressive Parser";
	if (svgin->oti==SVG_IN_OTI_STREAMING_SVG) return "GPAC Streaming SVG Parser";
	if (svgin->oti==SVG_IN_OTI_STREAMING_SVG_GZ) return "GPAC Streaming SVGZ Parser";
	if (svgin->oti==SVG_IN_OTI_LASERML) return "GPAC LASeRML Parser";
	return "INTERNAL ERROR";
}

Bool SVG_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, unsigned char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if (StreamType==GF_STREAM_PRIVATE_SCENE) {
		if (ObjectType==SVG_IN_OTI_SVG) return 1;
		//if (ObjectType==SVG_IN_OTI_LASERML) return 1;
		return 0;
	} else if (StreamType==GF_STREAM_SCENE) {
		if (ObjectType==SVG_IN_OTI_STREAMING_SVG) return 1;
		if (ObjectType==SVG_IN_OTI_STREAMING_SVG_GZ	) return 1;
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
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	SVGIn *svgin;
	GF_SceneDecoder *sdec;

	if (InterfaceType != GF_SCENE_DECODER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sdec, sizeof(GF_SceneDecoder))
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC SVG Parser", "gpac distribution");

	GF_SAFEALLOC(svgin, sizeof(SVGIn));
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
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_SceneDecoder *sdec = (GF_SceneDecoder *)ifce;
	SVGIn *svgin = (SVGIn *) sdec->privateStack;
	if (sdec->InterfaceType != GF_SCENE_DECODER_INTERFACE) return;

	free(svgin);
	free(sdec);
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_SCENE_DECODER_INTERFACE) return 1;
	return 0;
}
#else


/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	return NULL;
}


/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	return 0;
}
#endif
