//This software module was originally developed by TelecomParisTech in the
//course of the development of MPEG-U Widgets (ISO/IEC 23007-1) standard.
//
//This software module is an implementation of a part of one or
//more MPEG-U Widgets (ISO/IEC 23007-1) tools as specified by the MPEG-U Widgets
//(ISO/IEC 23007-1) standard. ISO/IEC gives users of the MPEG-U Widgets
//(ISO/IEC 23007-1) free license to this software module or modifications
//thereof for use in hardware or software products claiming conformance to
//the MPEG-U Widgets (ISO/IEC 23007-1). Those intending to use this software
//module in hardware or software products are advised that its use may
//infringe existing patents.
//The original developer of this software module and his/her company, the
//subsequent editors and their companies, and ISO/IEC have no liability
//for use of this software module or modifications thereof in an implementation.
//Copyright is not released for non MPEG-U Widgets (ISO/IEC 23007-1) conforming
//products.
//Telecom ParisTech retains full right to use the code for his/her own purpose,
//assign or donate the code to a third party and to inhibit third parties from
//using the code for non MPEG-U Widgets (ISO/IEC 23007-1) conforming products.
//
//This copyright notice must be included in all copies or derivative works.
//
//Copyright (c) 2009 Telecom ParisTech.
//
// Alternatively, this software module may be redistributed and/or modified
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2, or (at your option)
//  any later version.
//
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:
//					Cyril Concolato, Telecom ParisTech
//					Jean Le Feuvre, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////


#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/constants.h>

#if defined(GPAC_HAS_SPIDERMONKEY) && !defined(GPAC_DISABLE_SVG)

typedef struct
{
	GF_Scene *scene;
	u8 oti;
	char *file_name;
	u32 file_size;
	Bool loaded;
} WgtLoad;

static GF_Err WGT_ProcessData(GF_SceneDecoder *plug, const char *inBuffer, u32 inBufferLength,
                              u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	WgtLoad *wgtload = (WgtLoad *)plug->privateStack;

	if (stream_time==(u32)-1) {
		gf_sg_reset(wgtload->scene->graph);
		return GF_OK;
	}

	switch (wgtload->oti) {
	case GPAC_OTI_PRIVATE_SCENE_WGT:
		if (wgtload->file_name && !wgtload->loaded) {
			const char *path, *wmpath;
			char *tmp;
			GF_Node *n, *root;
			GF_FieldInfo info;
			FILE *jsfile;
			GF_ChildNodeItem *last;

			wgtload->loaded = GF_TRUE;

			gf_sg_add_namespace(wgtload->scene->graph, "http://www.w3.org/2000/svg", NULL);
			gf_sg_add_namespace(wgtload->scene->graph, "http://www.w3.org/1999/xlink", "xlink");
			gf_sg_add_namespace(wgtload->scene->graph, "http://www.w3.org/2001/xml-events", "ev");
			gf_sg_set_scene_size_info(wgtload->scene->graph, 800, 600, GF_TRUE);

			/* modify the scene with an Inline/Animation pointing to the widget start file URL */
			n = root = gf_node_new(wgtload->scene->graph, TAG_SVG_svg);
			gf_node_register(root, NULL);
			gf_sg_set_root_node(wgtload->scene->graph, root);
			gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_viewBox, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute(n, &info, "0 0 320 240", 0);
			gf_node_get_attribute_by_name(n, "xmlns", 0, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute(n, &info, "http://www.w3.org/2000/svg", 0);
			/*
						gf_sg_set_scene_size_info(wgtload->scene->graph, 800, 600, 1);
						gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_width, 1, 0, &info);
						gf_svg_parse_attribute(n, &info, "800", 0);
						gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_height, 1, 0, &info);
						gf_svg_parse_attribute(n, &info, "600", 0);
			*/
			gf_node_init(n);

			n = gf_node_new(wgtload->scene->graph, TAG_SVG_animation);
			gf_node_set_id(n, 1, "w_anim");
			gf_node_register(n, root);
			gf_node_list_add_child_last(&((GF_ParentNode *)root)->children, n, &last);
			gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_width, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute(n, &info, "320", 0);
			gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_height, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute(n, &info, "240", 0);
			gf_node_init(n);

			tmp = wgtload->file_name;
			while ((tmp = strchr(tmp, '\\'))) {
				tmp[0] = '/';
				tmp++;
			}

			n = gf_node_new(wgtload->scene->graph, TAG_SVG_script);
			gf_node_register(n, root);
			gf_node_list_add_child_last(&((GF_ParentNode *)root)->children, n, &last);
			path = gf_opts_get_key("Widgets", "WidgetLoadScript");
			jsfile = path ? gf_fopen(path, "rt") : NULL;
			if (jsfile) {
				gf_fclose(jsfile);
				gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info);
				gf_svg_parse_attribute(n, &info, (char *) path, 0);
			} else {
				const char *load_fun = "function load_widget(wid_url) {\n"
				                       "	var wid = WidgetManager.load(wid_url);\n"
				                       "	var anim = document.getElementById('w_anim');\n"
				                       "	if (wid != null) {\n"
				                       "		wid.activate(anim);"
				                       "		anim.setAttributeNS('http://www.w3.org/1999/xlink', 'href', wid.main);\n"
				                       "	} else {\n"
				                       "		alert('Widget ' + wid_url + ' is not valid');\n"
				                       "	}\n"
				                       "}\n";

				gf_dom_add_text_node(n, gf_strdup(load_fun) );
			}
			gf_node_init(n);


			wmpath = gf_opts_get_key("Widgets", "WidgetManagerScript");
			jsfile = wmpath ? gf_fopen(wmpath, "rt") : NULL;
			if (jsfile) {
				gf_fclose(jsfile);
				n = gf_node_new(wgtload->scene->graph, TAG_SVG_script);
				gf_node_register(n, root);
				gf_node_list_add_child_last(&((GF_ParentNode *)root)->children, n, &last);
				gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info);
				gf_svg_parse_attribute(n, &info, (char *) wmpath, 0);
				gf_node_init(n);

				n = gf_node_new(wgtload->scene->graph, TAG_SVG_script);
				gf_node_register(n, root);
				gf_node_list_add_child_last(&((GF_ParentNode *)root)->children, n, &last);
				gf_dom_add_text_node(n, gf_strdup("widget_manager_init();") );
				gf_node_init(n);
			}

			tmp = (char*)gf_malloc(sizeof(char) * (strlen(wgtload->file_name)+50) );
			sprintf(tmp, "load_widget(\"%s\");\n", wgtload->file_name);

			n = gf_node_new(wgtload->scene->graph, TAG_SVG_script);
			gf_node_register(n, root);
			gf_node_list_add_child_last(&((GF_ParentNode *)root)->children, n, &last);
			gf_dom_add_text_node(n, gf_strdup(tmp) );
			gf_free(tmp);

			gf_node_init(n);

			if ((wgtload->scene->graph_attached!=1) && (gf_sg_get_root_node(wgtload->scene->graph)!=NULL) ) {
				gf_scene_attach_to_compositor(wgtload->scene);
				e = GF_EOS;
			}
		}
		break;

	default:
		return GF_BAD_PARAM;
	}
	return e;
}

static GF_Err WGT_AttachScene(GF_SceneDecoder *plug, GF_Scene *scene, Bool is_scene_decoder)
{
	WgtLoad *wgtload = (WgtLoad *)plug->privateStack;
	wgtload->scene = scene;
	return GF_OK;
}

static GF_Err WGT_ReleaseScene(GF_SceneDecoder *plug)
{
	WgtLoad *wgtload = (WgtLoad *)plug->privateStack;
	wgtload->scene = NULL;
	return GF_OK;
}

static GF_Err WGT_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	GF_BitStream *bs;
	WgtLoad *wgtload = (WgtLoad *)plug->privateStack;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;

	/* decSpecInfo is not null only when reading from an WGT file (local or distant, cached or not) */
	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_PRIVATE_SCENE_WGT:
	default:
		if (!esd->decoderConfig->decoderSpecificInfo) return GF_NON_COMPLIANT_BITSTREAM;
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		wgtload->file_size = gf_bs_read_u32(bs);
		gf_bs_del(bs);
		wgtload->file_name =  (char *) gf_malloc(sizeof(char)*(1 + esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32)) );
		memcpy(wgtload->file_name, esd->decoderConfig->decoderSpecificInfo->data + sizeof(u32), esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32) );
		wgtload->file_name[esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32) ] = 0;
		break;
	}
	wgtload->oti = esd->decoderConfig->objectTypeIndication;
	return GF_OK;
}

static GF_Err WGT_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	WgtLoad *wgtload = (WgtLoad *)plug->privateStack;
	if (wgtload->file_name) gf_free(wgtload->file_name);
	wgtload->file_name = NULL;
	return GF_OK;
}

const char *WGT_GetName(struct _basedecoder *plug)
{
	return "GPAC W3C Widget Loader";
}

static u32 WGT_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, GF_ESD *esd, u8 PL)
{
	/*don't reply to media type query*/
	if (!esd) return GF_CODEC_NOT_SUPPORTED;

	if (StreamType==GF_STREAM_PRIVATE_SCENE) {
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_PRIVATE_SCENE_WGT) return GF_CODEC_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static GF_Err WGT_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	if (cap->CapCode==GF_CODEC_PADDING_BYTES) {
		/* Adding one byte of padding for \r\n problems*/
		cap->cap.valueInt = 1;
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static GF_Err WGT_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

/*interface create*/
GF_BaseInterface *LoadWidgetReader()
{
	WgtLoad *wgtload;
	GF_SceneDecoder *sdec;

	GF_SAFEALLOC(sdec, GF_SceneDecoder)
	if (!sdec) return NULL;
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC W3C Widget Loader", "gpac distribution");

	GF_SAFEALLOC(wgtload, WgtLoad);
	if (!wgtload) {
		gf_free(sdec);
		return NULL;
	}
	
	sdec->privateStack = wgtload;
	sdec->AttachStream = WGT_AttachStream;
	sdec->CanHandleStream = WGT_CanHandleStream;
	sdec->DetachStream = WGT_DetachStream;
	sdec->AttachScene = WGT_AttachScene;
	sdec->ReleaseScene = WGT_ReleaseScene;
	sdec->ProcessData = WGT_ProcessData;
	sdec->GetName = WGT_GetName;
	sdec->SetCapabilities = WGT_SetCapabilities;
	sdec->GetCapabilities = WGT_GetCapabilities;
	return (GF_BaseInterface *)sdec;
}


/*interface destroy*/
void ShutdownWidgetReader(GF_BaseInterface *ifce)
{
	GF_SceneDecoder *sdec = (GF_SceneDecoder *)ifce;
	WgtLoad *wgtload;
	if (!ifce)
		return;
	wgtload = (WgtLoad *) sdec->privateStack;
	if (wgtload)
		gf_free(wgtload);
	sdec->privateStack = NULL;
	gf_free(sdec);
}

#endif	/* defined(GPAC_HAS_SPIDERMONKEY) && !defined(GPAC_DISABLE_SVG) */
