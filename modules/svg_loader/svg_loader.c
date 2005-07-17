/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
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
#include "svg_parser.h"

#ifndef GPAC_DISABLE_SVG

/*************************************************************************
 *																	 	 *
 * Functions implementing the Basic Decoder and Scene Decoder interfaces *
 *																		 *
 *************************************************************************/
static GF_Err SVG_ProcessDocument(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	SVGParser *parser = plug->privateStack;

	if (parser->status == 0) {
		parser->status = 1;
		e = SVGParser_Parse(parser);
		if (!e) {
			gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
			/*attach graph to renderer*/
			gf_is_attach_to_renderer(parser->inline_scene);
		} else {
			parser->status = 0;
			return e;
		}
	}
	return GF_EOS;
}


static GF_Err SVG_ProcessFragment(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	SVGParser *parser;
	char szFile[GF_MAX_PATH];

	GF_Err e = GF_OK;

	parser = plug->privateStack;
	
	assert(parser->fileName);
	if (parser->status==2) return GF_EOS;

	/*need to init (RAP-like)*/
	if (!parser->status) {
		char *sSep;
		parser->seg_idx = 0;
		parser->status = 1;
		if (parser->szOriginalRad) free(parser->szOriginalRad);

		parser->szOriginalRad = strdup(parser->fileName);
		sSep = strrchr(parser->szOriginalRad, '.');
		if (sSep) sSep[0] = 0;
		while (1) {
			u32 len = strlen(parser->szOriginalRad);
			if (!len) break;
			if (!isdigit(parser->szOriginalRad[len-1])) break;
			parser->szOriginalRad[len-1] = 0;
		}

		/*first file is assumed to be named XXX0.svgm*/
		parser->seg_idx = 1;

		/*first segment of the file*/
		e = SVGParser_Parse(parser);
		if (e >= GF_OK) {
			gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
			/*attach graph to renderer*/
			gf_is_attach_to_renderer(parser->inline_scene);
			return e;
		} else {
			parser->status = 0;
			return e;
		}
	} else {
		sprintf(szFile, "%s%d.svgm", parser->szOriginalRad, parser->seg_idx);
		parser->seg_idx++;

		free(parser->fileName);
		parser->fileName = strdup(szFile);

		fprintf(stdout, "parser file name:%s\n", parser->fileName);

		//parsing the segment
		e = SVGParser_Parse(parser);
		if (e == GF_OK) return GF_OK;
		else if (e==GF_EOS) {
			parser->status = 2;
			return GF_EOS;
		} else {
			parser->status = 0;
			return e;
		}
	}
}


static GF_Err SVG_ProcessData(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	SVGParser *parser = plug->privateStack;
	if (parser->oti==2) return SVG_ProcessDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	if (parser->oti==3) return SVG_ProcessFragment(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	return GF_BAD_PARAM;
}

static GF_Err SVG_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_scene_decoder)
{
	SVGParser *parser = plug->privateStack;
	parser->inline_scene = scene;
	parser->graph = scene->graph;
	parser->temp_dir = (char *) gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
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
	SVGParser *parser = plug->privateStack;
	if (Upstream) return GF_NOT_SUPPORTED;

	/*main dummy stream we need a dsi*/
	if (!decSpecInfo) return GF_NON_COMPLIANT_BITSTREAM;

	parser->fileName = strdup(decSpecInfo);
	parser->oti = objectTypeIndication;

	return GF_OK;
}

static GF_Err SVG_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	return GF_OK;
}

const char *SVG_GetName(struct _basedecoder *plug)
{
	SVGParser *parser = plug->privateStack;
	if (parser->oti==2) return "GPAC SVG Parser";
	if (parser->oti==3) return "GPAC FragmentedSVG Parser";
	return "INTERNAL ERROR";
}

Bool SVG_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, unsigned char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if (StreamType!=GF_STREAM_PRIVATE_SCENE) return 0;
	if (ObjectType==2) return 1;
	if (ObjectType==3) return 1;
	return 0;
}

static GF_Err SVG_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	return GF_NOT_SUPPORTED;
}

static GF_Err SVG_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	SVGParser *parser;
	GF_SceneDecoder *sdec;
	if (InterfaceType != GF_SCENE_DECODER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sdec, sizeof(GF_SceneDecoder))
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC SVG Parser", "gpac distribution");

	parser = NewSVGParser();

	sdec->privateStack = parser;
#ifdef USE_GPAC_CACHE_MECHANISM
	parser->sdec = sdec;
#endif
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
	SVGParser *parser = (SVGParser *) sdec->privateStack;
	if (sdec->InterfaceType != GF_SCENE_DECODER_INTERFACE) return;

	gf_list_del(parser->ided_nodes);
	if (parser->fileName) free(parser->fileName);
	if (parser->szOriginalRad) free(parser->szOriginalRad);
	free(parser);
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
