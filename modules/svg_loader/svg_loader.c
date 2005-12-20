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

#include <zlib.h>

/*************************************************************************
 *																	 	 *
 * Functions implementing the Basic Decoder and Scene Decoder interfaces *
 *																		 *
 *************************************************************************/

static GF_Err LSR_ProcessDocument(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e;
	SVGParser *parser = plug->privateStack;

	parser->stream_time = stream_time;
	e = SVGParser_ParseLASeR(parser);
	if (!e && parser->needs_attachement) {
		parser->needs_attachement = 0;
		gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
		gf_is_attach_to_renderer(parser->inline_scene);
	}
	return e;
}

/* Only in case of reading from file (cached or not) of an XML file (i.e. not AU framed)
   The buffer is empty but the filename has been given in a previous step: SVG_AttachStream */
static GF_Err SVG_ProcessProgressiveDocument(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	SVGParser *parser = plug->privateStack;

	if (parser->loader_status == 4) return GF_BAD_PARAM;

	if (parser->loader_status == 0) {
		e = SVGParser_InitProgressiveFileChunk(parser);
		if (e) {
			parser->loader_status = 4;
			return e;
		} 
		parser->loader_status = 1;
	}

	e = SVGParser_ParseProgressiveFileChunk(parser);
	if (parser->loader_status == 2) 
	{ /* the SVG root element has been encountered */
		gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
		/*attach graph to renderer*/
		gf_is_attach_to_renderer(parser->inline_scene);
		parser->loader_status = 3;
	}
	return e;
}

/* Only in case of reading from file (cached or not) of an XML file (i.e. not AU framed)
   The buffer is empty but the filename has been given in a previous step: SVG_AttachStream */
static GF_Err SVG_ProcessFullDocument(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_EOS;
	SVGParser *parser = plug->privateStack;

	if (parser->loader_status == 0) {
		/*needs full doc for DOM parsing*/
		if (!SVG_CheckDownload(parser)) return GF_OK;

		parser->loader_status = 1;
		e = SVGParser_ParseFullDoc(parser);

		if (!e) {
			gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
			/*attach graph to renderer*/
			gf_is_attach_to_renderer(parser->inline_scene);
		} else {
			parser->loader_status = 0;
		}
	} 
	return e;
}

/* Only in case of streaming or reading from MP4 file or framed container
   The buffer contains the actual piece of SVG to read */
static GF_Err SVG_ProcessAU(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	SVGParser *parser = plug->privateStack;

	if (parser->loader_status == 0) {
		parser->loader_status = 1;
		e = SVGParser_ParseMemoryFirstChunk(parser, inBuffer, inBufferLength);
		
		if (!e) {
			gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
			/*attach graph to renderer*/
			gf_is_attach_to_renderer(parser->inline_scene);
		} else {
			parser->loader_status = 0;
		}
	} else {
		e = SVGParser_ParseMemoryNextChunk(parser, inBuffer, inBufferLength);
		if (e == GF_EOS) return GF_OK;
	}
	return e;
}


static GF_Err SVG_ProcessData(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	SVGParser *parser = plug->privateStack;
	switch (parser->oti) {
	case SVGLOADER_OTI_SVG:
		/*DOM parsing*/
		if (parser->load_type==SVG_LOAD_DOM) {
			return SVG_ProcessFullDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
		}
		/*SAX parsing*/
		else {
			return SVG_ProcessProgressiveDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
		}
	case SVGLOADER_OTI_STREAMING_SVG:
		return SVG_ProcessAU(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	case SVGLOADER_OTI_STREAMING_SVG_GZ:
	{
		char svg_data[2049];
		GF_Err e = GF_OK;
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
				e = SVG_ProcessAU(plug, svg_data, d_stream.total_out - done, ES_ID, stream_time, mmlevel);
				if (e || (err== Z_STREAM_END)) break;
				done = d_stream.total_out;
				d_stream.avail_out = 2048;
				d_stream.next_out = svg_data;
			}
			inflateEnd(&d_stream);
		}
		return e;
	}
	case SVGLOADER_OTI_LASERML:
		return LSR_ProcessDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	default:
		return GF_BAD_PARAM;
	}
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

Bool SVG_CheckDownload(SVGParser *parser)
{
	u32 size;
	FILE *f = fopen(parser->file_name, "rt");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	if (size==parser->file_size) return 1;
	return 0;
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
	SVGParser *parser = plug->privateStack;
	if (Upstream) return GF_NOT_SUPPORTED;

	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	switch (objectTypeIndication) {
	case SVGLOADER_OTI_STREAMING_SVG:
	case SVGLOADER_OTI_STREAMING_SVG_GZ:
		/*no decSpecInfo defined for streaming svg yet*/
		break;
	default:
		if (!decSpecInfo) return GF_NON_COMPLIANT_BITSTREAM;
		bs = gf_bs_new(decSpecInfo, decSpecInfoSize, GF_BITSTREAM_READ);
		parser->file_size = gf_bs_read_u32(bs);
		gf_bs_del(bs);
		GF_SAFEALLOC(parser->file_name, sizeof(char)*(1 + decSpecInfoSize - sizeof(u32)) );
		memcpy(parser->file_name, decSpecInfo + sizeof(u32), decSpecInfoSize - sizeof(u32) );
		break;
	}
	parser->oti = objectTypeIndication;

	parser->sax_max_duration = 30;
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "SVGLoader", "SAXMaxDuration");
	if (sOpt) parser->sax_max_duration = atoi(sOpt);

	parser->load_type = SVG_LOAD_DOM;
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "SVGLoader", "LoadType");
	if (sOpt && !strcmp(sOpt, "DOM") ) parser->load_type = SVG_LOAD_DOM;
	else if (sOpt && !strcmp(sOpt, "SAX") ) parser->load_type = SVG_LOAD_SAX;
	else if (sOpt && !strcmp(sOpt, "SAX Progressive") ) parser->load_type = SVG_LOAD_SAX_PROGRESSIVE;

	/*if DOM parsing is requested and file size is not available, the server didn't send
	a proper content length, force SAX parsing*/
	if (!parser->file_size && (parser->load_type==SVG_LOAD_DOM)) {
		parser->load_type = SVG_LOAD_SAX;
	}
	return GF_OK;
}

static GF_Err SVG_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	return GF_OK;
}

const char *SVG_GetName(struct _basedecoder *plug)
{
	SVGParser *parser = plug->privateStack;
	if (parser->oti==SVGLOADER_OTI_SVG) return (parser->load_type==SVG_LOAD_DOM) ? "GPAC SVG DOM Parser" : (parser->load_type==SVG_LOAD_SAX) ? "GPAC SVG SAX Parser" : "GPAC SVG Progressive Parser";
	if (parser->oti==SVGLOADER_OTI_STREAMING_SVG) return "GPAC Streaming SVG + libXML Parser";
	if (parser->oti==SVGLOADER_OTI_STREAMING_SVG_GZ) return "GPAC Streaming SVGZ + libXML Parser";
	if (parser->oti==SVGLOADER_OTI_LASERML) return "GPAC LASeRML Parser";
	return "INTERNAL ERROR";
}

Bool SVG_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, unsigned char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if (StreamType==GF_STREAM_PRIVATE_SCENE) {
		if (ObjectType==SVGLOADER_OTI_SVG) return 1;
		//if (ObjectType==SVGLOADER_OTI_LASERML) return 1;
		return 0;
	} else if (StreamType==GF_STREAM_SCENE) {
		if (ObjectType==SVGLOADER_OTI_STREAMING_SVG) return 1;
		if (ObjectType==SVGLOADER_OTI_STREAMING_SVG_GZ) return 1;
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
	SVGParser *parser;
	GF_SceneDecoder *sdec;
	if (InterfaceType != GF_SCENE_DECODER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sdec, sizeof(GF_SceneDecoder))
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC SVG Parser", "gpac distribution");

	parser = NewSVGParser();
	sdec->privateStack = parser;
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

	SVGParser_Terminate(parser);
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
