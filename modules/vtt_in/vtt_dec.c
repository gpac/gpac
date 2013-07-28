/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato 
 *			Copyright (c) Telecom ParisTech 2013-
 *					All rights reserved
 *
 *  This file is part of GPAC / VTT Loader/Decoder module
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


#include <gpac/constants.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/webvtt.h>

#ifndef GPAC_DISABLE_VTT

typedef struct {
	GF_BaseInterface *module;

	/* Object Type Indication to distinguish between WebVTT content passsed to the decoder as:
	   - a single file (GPAC_OTI_PRIVATE_SCENE_VTT)
	   - character strings (GPAC_OTI_SCENE_VTT)
	   - ISO sample (GPAC_OTI_SCENE_VTT_MP4)
	*/
	u32 oti;

	/* Boolean indicating if there is already a WebVTT stream attached to this decoder */
	Bool is_stream_attached;

	/* ID of the Elementary Stream on which this stream depends (if any)
	   0 if no dependency */
	u32 base_es_id;

	/* file_name of the VTT file when parsing a input text file 
	   not used when parsing from MP4 or from a stream */
	char *file_name;
	u64 file_size;
	u64 file_pos;

	/* config of the VTT file when parsing an MP4 file 
	   not used when parsing from text file or from a stream */
	char *config;

	/* Boolean indicating if a progressive download/parsing is required */
	Bool use_progressive;

	/* Scene to which this WebVTT stream is linked */
	GF_Scene *scene;

	/* Terminal to which this WebVTT stream is linked */
	GF_Terminal *terminal;

	GF_List *cues;

	/* Scene graph for the subtitle content */
	GF_SceneGraph *sg;
	Bool has_rendering_script;
} VTTDec;


#define VTT_BUFFER_SIZE 4096

/* Checks that the file is fully downloaded
   requires that the file_size is given in the DecoderSpecificInfo */
static Bool vtt_check_download(VTTDec *vttdec)
{
	u64 size;
	FILE *f = gf_f64_open(vttdec->file_name, "rt");
	if (!f) return GF_FALSE;
	gf_f64_seek(f, 0, SEEK_END);
	size = gf_f64_tell(f);
	fclose(f);
	if (size==vttdec->file_size) return GF_TRUE;
	return GF_FALSE;
}


static GF_Err VTT_ProcessData(GF_SceneDecoder *plug, const char *inBuffer, u32 inBufferLength,
							  u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	VTTDec *vttdec = (VTTDec *)plug->privateStack;
	if (!vttdec->has_rendering_script) return GF_BAD_PARAM;

	if (stream_time==(u32)-1) {
		/* TODO */
		return GF_OK;
	}

	switch (vttdec->oti) {
	case GPAC_OTI_PRIVATE_SCENE_VTT:
		/*full parsing*/
		if (vttdec->file_size) {
			/*init step*/
			if (!vttdec->use_progressive) {
				/*not done yet*/
				if (!vtt_check_download(vttdec)) return GF_OK;
				/* TODO: parse */
			} else {
				/* TODO: parse what you can */
			}
		}
		break;

	case GPAC_OTI_SCENE_VTT:
		/* TODO: try to parse a cue from the given string */
		break;

	case GPAC_OTI_SCENE_VTT_MP4:
		{ 
			char start[100], end[100];
			GF_List *cues;
			cues = gf_webvtt_parse_cues_from_data(inBuffer, inBufferLength, 0);
			gf_webvtt_js_removeCues(vttdec->sg->RootNode);
			if (gf_list_count(cues)) {
				while (gf_list_count(cues)) {
					GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, 0);
					gf_list_rem(cues, 0);
					sprintf(start, "%02d:%02d:%02d.%03d", cue->start.hour, cue->start.min, cue->start.sec, cue->start.ms);
					sprintf(end, "%02d:%02d:%02d.%03d", cue->end.hour, cue->end.min, cue->end.sec, cue->end.ms);
					gf_webvtt_js_addCue(vttdec->sg->RootNode, cue->id, start, end, cue->settings, cue->text);
				}
			} 
			gf_list_del(cues);
		}
		break;

	default:
		return GF_BAD_PARAM;
	}

	return e;
}

void VTT_load_script(VTTDec *vttdec, GF_SceneGraph *graph)
{
	GF_Node *n, *root;
	GF_FieldInfo info;
	const char *path;
	FILE *jsfile;

	if (!graph) return;
	gf_sg_add_namespace(graph, "http://www.w3.org/2000/svg", NULL);
	gf_sg_add_namespace(graph, "http://www.w3.org/1999/xlink", "xlink");
	gf_sg_add_namespace(graph, "http://www.w3.org/2001/xml-events", "ev");
	gf_sg_set_scene_size_info(graph, 800, 600, GF_TRUE);

	/* modify the scene with an Inline/Animation pointing to the VTT Renderer */
	n = root = gf_node_new(graph, TAG_SVG_svg);
	gf_node_register(root, NULL);
	gf_sg_set_root_node(graph, root);
	gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_viewBox, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(n, &info, "0 0 320 240", 0);
	gf_node_get_attribute_by_name(n, "xmlns", 0, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(n, &info, "http://www.w3.org/2000/svg", 0);
	gf_node_init(n);

	n = gf_node_new(graph, TAG_SVG_script);
	gf_node_register(n, root);
	gf_node_list_add_child(&((GF_ParentNode *)root)->children, n);
	path = gf_modules_get_option((GF_BaseInterface *)vttdec->module, "WebVTT", "RenderingScript");
	if (!path) {
		/* try to find the JS renderer in the default GPAC installation folder */
		const char *startuppath = gf_modules_get_option((GF_BaseInterface *)vttdec->module, "General", "StartupFile");
		path = gf_url_concatenate(startuppath, "webvtt-renderer.js");
		jsfile = gf_f64_open(path, "rt");
		if (jsfile) {
			gf_modules_set_option((GF_BaseInterface *)vttdec->module, "WebVTT", "RenderingScript", path);
			fclose(jsfile);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebVTT] Cannot find Rendering Script - check config file\n"));
			return;
		}
	}
	jsfile = gf_f64_open(path, "rt");
	if (jsfile) {
		fclose(jsfile);
		gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info);
		gf_svg_parse_attribute(n, &info, (char *) path, 0);
		vttdec->has_rendering_script = GF_TRUE;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[WebVTT] Cannot open Rendering Script - %s\n", path));
		return;
	}
	gf_node_init(n);

}

static GF_Err VTT_AttachScene(GF_SceneDecoder *plug, GF_Scene *scene, Bool is_scene_decoder)
{
	VTTDec *vttdec = (VTTDec *)plug->privateStack;
	/*WebVTT cannot be a root scene object*/
	//if (is_scene_decoder) return GF_BAD_PARAM;
	vttdec->scene = scene;
	vttdec->terminal = scene->root_od->term;

	vttdec->sg = gf_sg_new_subscene(vttdec->scene->graph);
	VTT_load_script(vttdec, vttdec->sg);
	//vtt_create_text_node(vttdec, 2000, GF_TRUE);
	return GF_OK;
}

static void VTT_CleanExtraScene(VTTDec *vttdec)
{
	/* Cleaning scene graph related data */
	if (vttdec->sg) {
		gf_scene_register_extra_graph(vttdec->scene, vttdec->sg, GF_TRUE);
		gf_sg_del(vttdec->sg);
		vttdec->sg = NULL;
	}
}

static GF_Err VTT_ReleaseScene(GF_SceneDecoder *plug)
{
	VTTDec *vttdec = (VTTDec *)plug->privateStack;

	VTT_CleanExtraScene(vttdec);
	vttdec->scene = NULL;
	vttdec->terminal = NULL;

	return GF_OK;
}

static void VTT_ReadFileNameFromDSI(VTTDec *vttdec, GF_DefaultDescriptor *dsi)
{
	GF_BitStream *bs;
	bs = gf_bs_new(dsi->data, dsi->dataLength, GF_BITSTREAM_READ);
	vttdec->file_size = gf_bs_read_u32(bs);
	vttdec->file_pos = 0;
	gf_bs_del(bs);
	vttdec->file_name =  (char *) gf_malloc(sizeof(char)*(1 + dsi->dataLength - sizeof(u32)) );
	memcpy(vttdec->file_name, dsi->data + sizeof(u32), dsi->dataLength - sizeof(u32) );
	vttdec->file_name[dsi->dataLength - sizeof(u32) ] = 0;
}

static void VTT_ReadConfigFromDSI(VTTDec *vttdec, GF_DefaultDescriptor *dsi)
{
	GF_BitStream *bs;
	u32 entry_type;

	bs = gf_bs_new(dsi->data, dsi->dataLength, GF_BITSTREAM_READ);
	entry_type = gf_bs_read_u32(bs);
	if (entry_type == GF_ISOM_BOX_TYPE_WVTT) {
		GF_Box *b;
		gf_isom_parse_box(&b, bs);
		vttdec->config = ((GF_StringBox *)b)->string;
		((GF_StringBox *)b)->string = NULL;
		gf_isom_box_del(b);
	} 
	gf_bs_del(bs);
}

static GF_Err VTT_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	VTTDec *vttdec = (VTTDec *)plug->privateStack;
	/* This is a down stream */
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	/* Only process one stream at a time*/
	if (vttdec->is_stream_attached) return GF_NOT_SUPPORTED;

	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_SCENE_VTT:
		/*no decSpecInfo defined for streaming VTT*/
		break;
	case GPAC_OTI_SCENE_VTT_MP4:
		/* decSpecInfo for VTT in MP4 contains WVTT config box */
		if (!esd->decoderConfig->decoderSpecificInfo) return GF_NON_COMPLIANT_BITSTREAM;
		VTT_ReadConfigFromDSI(vttdec, esd->decoderConfig->decoderSpecificInfo);
		break;
	case GPAC_OTI_PRIVATE_SCENE_VTT:
	default:
		if (!esd->decoderConfig->decoderSpecificInfo) return GF_NON_COMPLIANT_BITSTREAM;
		VTT_ReadFileNameFromDSI(vttdec, esd->decoderConfig->decoderSpecificInfo);
		break;
	}
	vttdec->oti = esd->decoderConfig->objectTypeIndication;
	vttdec->is_stream_attached = GF_TRUE;

	if (!esd->dependsOnESID) vttdec->base_es_id = esd->ESID;

	return GF_OK;
}

static GF_Err VTT_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	VTTDec *vttdec = (VTTDec *)plug->privateStack;
	if (vttdec->file_name) gf_free(vttdec->file_name);
	VTT_CleanExtraScene(vttdec);
	vttdec->file_name = NULL;
	vttdec->is_stream_attached = GF_FALSE;

	return GF_OK;
}

const char *VTT_GetName(struct _basedecoder *plug)
{
	VTTDec *vttdec = (VTTDec *)plug->privateStack;
	if (vttdec->oti==GPAC_OTI_PRIVATE_SCENE_VTT) return "GPAC WebVTT Parser";
	if (vttdec->oti==GPAC_OTI_SCENE_VTT) return "GPAC WebVTT Streaming Parser";
	if (vttdec->oti==GPAC_OTI_SCENE_VTT_MP4) return "GPAC WebVTT/MP4 Parser";
	return "INTERNAL ERROR";
}

static u32 VTT_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType==GF_STREAM_TEXT) {
		/*media type query*/
		if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		switch (esd->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_SCENE_VTT:
		case GPAC_OTI_SCENE_VTT_MP4:
			return GF_CODEC_SUPPORTED;
		default:	
			return GF_CODEC_NOT_SUPPORTED;
		}
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static GF_Err VTT_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	/* TODO */
	return GF_NOT_SUPPORTED;
}

static GF_Err VTT_SetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability capability)
{
	VTTDec *vttdec = (VTTDec *)plug->privateStack;
	if (capability.CapCode==GF_CODEC_ABORT) {
		/* TODO */
	} else if (capability.CapCode==GF_CODEC_SHOW_SCENE) {
		if (capability.cap.valueInt) {
			gf_scene_register_extra_graph(vttdec->scene, vttdec->sg, GF_FALSE);
		} else {
			gf_scene_register_extra_graph(vttdec->scene, vttdec->sg, GF_TRUE);
		}
	}
	return GF_OK;
}

GF_BaseInterface *NewVTTDec()
{
	VTTDec *vttdec;
	GF_SceneDecoder *sdec;

	GF_SAFEALLOC(sdec, GF_SceneDecoder)
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC WebVTT Parser", "gpac distribution");

	GF_SAFEALLOC(vttdec, VTTDec);
	vttdec->cues = gf_list_new();
	vttdec->module = (GF_BaseInterface *)sdec;

	sdec->privateStack = vttdec;
	sdec->CanHandleStream	= VTT_CanHandleStream;
	sdec->AttachStream		= VTT_AttachStream;
	sdec->DetachStream		= VTT_DetachStream;
	sdec->AttachScene		= VTT_AttachScene;
	sdec->ReleaseScene		= VTT_ReleaseScene;
	sdec->ProcessData		= VTT_ProcessData;
	sdec->GetName			= VTT_GetName;
	sdec->SetCapabilities	= VTT_SetCapabilities;
	sdec->GetCapabilities	= VTT_GetCapabilities;
	return (GF_BaseInterface *)sdec;
}

void DeleteVTTDec(GF_BaseDecoder *plug)
{
	VTTDec *dec= (VTTDec *)plug->privateStack;
	gf_list_del(dec->cues);
	gf_free(dec);
	gf_free(plug);
}

#endif
