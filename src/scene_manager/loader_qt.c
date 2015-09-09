/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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

#include <gpac/scene_manager.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#if !defined(GPAC_DISABLE_QTVR)

#include <gpac/nodes_mpeg4.h>



static GF_Err gf_qt_report(GF_SceneLoader *load, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[1024];
		va_list args;
		va_start(args, format);
		vsprintf(szMsg, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[QT Parsing] %s\n", szMsg) );
	}
#endif
	return e;
}


/*import cubic QTVR to mp4*/
GF_Err gf_sm_load_init_qt(GF_SceneLoader *load)
{
	u32 i, di, w, h, tk, nb_samp;
	Bool has_qtvr;
	GF_ISOSample *samp;
	GF_ISOFile *src;
	GF_StreamContext *st;
	GF_AUContext *au;
	GF_Command *com;
	M_Background *back;
	M_NavigationInfo *ni;
	M_Group *gr;
	GF_ODUpdate *odU;
	GF_SceneGraph *sg;
	GF_ObjectDescriptor *od;
	GF_ESD *esd;

	if (!load->ctx) return GF_NOT_SUPPORTED;

	src = gf_isom_open(load->fileName, GF_ISOM_OPEN_READ, NULL);
	if (!src) return gf_qt_report(load, GF_URL_ERROR, "Opening file %s failed", load->fileName);

	w = h = tk = 0;
	nb_samp = 0;

	has_qtvr = 0;
	for (i=0; i<gf_isom_get_track_count(src); i++) {
		switch (gf_isom_get_media_type(src, i+1)) {
		case GF_ISOM_MEDIA_VISUAL:
			if (gf_isom_get_media_subtype(src, i+1, 1) == GF_4CC('j', 'p', 'e', 'g')) {
				GF_GenericSampleDescription *udesc = gf_isom_get_generic_sample_description(src, i+1, 1);
				if ((udesc->width>w) || (udesc->height>h)) {
					w = udesc->width;
					h = udesc->height;
					tk = i+1;
					nb_samp = gf_isom_get_sample_count(src, i+1);
				}
				if (udesc->extension_buf) gf_free(udesc->extension_buf);
				gf_free(udesc);
			}
			break;
		case GF_4CC('q','t','v','r'):
			has_qtvr = 1;
			break;
		}
	}
	if (!has_qtvr) {
		gf_isom_delete(src);
		return gf_qt_report(load, GF_NOT_SUPPORTED, "QTVR not found - no conversion available for this QuickTime movie");
	}
	if (!tk) {
		gf_isom_delete(src);
		return gf_qt_report(load, GF_NON_COMPLIANT_BITSTREAM, "No associated visual track with QTVR movie");
	}
	if (nb_samp!=6) {
		gf_isom_delete(src);
		return gf_qt_report(load, GF_NOT_SUPPORTED, "Movie %s doesn't look a Cubic QTVR - sorry...", load->fileName);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("QT: Importing Cubic QTVR Movie"));

	/*create scene*/
	sg = load->ctx->scene_graph;
	gr = (M_Group *) gf_node_new(sg, TAG_MPEG4_Group);
	gf_node_register((GF_Node *)gr, NULL);
	st = gf_sm_stream_new(load->ctx, 1, GF_STREAM_SCENE, 1);
	au = gf_sm_stream_au_new(st, 0, 0, 1);
	com = gf_sg_command_new(load->ctx->scene_graph, GF_SG_SCENE_REPLACE);
	gf_list_add(au->commands, com);
	com->node = (GF_Node *)gr;

	back = (M_Background *) gf_node_new(sg, TAG_MPEG4_Background);
	gf_node_list_add_child( &gr->children, (GF_Node*)back);
	gf_node_register((GF_Node *)back, (GF_Node *)gr);

	gf_sg_vrml_mf_alloc(&back->leftUrl, GF_SG_VRML_MFURL, 1);
	back->leftUrl.vals[0].OD_ID = 2;
	gf_sg_vrml_mf_alloc(&back->frontUrl, GF_SG_VRML_MFURL, 1);
	back->frontUrl.vals[0].OD_ID = 3;
	gf_sg_vrml_mf_alloc(&back->rightUrl, GF_SG_VRML_MFURL, 1);
	back->rightUrl.vals[0].OD_ID = 4;
	gf_sg_vrml_mf_alloc(&back->backUrl, GF_SG_VRML_MFURL, 1);
	back->backUrl.vals[0].OD_ID = 5;
	gf_sg_vrml_mf_alloc(&back->topUrl, GF_SG_VRML_MFURL, 1);
	back->topUrl.vals[0].OD_ID = 6;
	gf_sg_vrml_mf_alloc(&back->bottomUrl, GF_SG_VRML_MFURL, 1);
	back->bottomUrl.vals[0].OD_ID = 7;

	ni = (M_NavigationInfo *) gf_node_new(sg, TAG_MPEG4_NavigationInfo);
	gf_node_list_add_child(&gr->children, (GF_Node*)ni);
	gf_node_register((GF_Node *)ni, (GF_Node *)gr);
	gf_sg_vrml_mf_reset(&ni->type, GF_SG_VRML_MFSTRING);
	gf_sg_vrml_mf_alloc(&ni->type, GF_SG_VRML_MFSTRING, 1);
	ni->type.vals[0] = gf_strdup("VR");

	/*create ODs*/
	st = gf_sm_stream_new(load->ctx, 2, GF_STREAM_OD, 1);
	au = gf_sm_stream_au_new(st, 0, 0, 1);
	odU = (GF_ODUpdate*) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
	gf_list_add(au->commands, odU);
	for (i=0; i<6; i++) {
		GF_MuxInfo *mi;
		FILE *img;
		char szName[1024];
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 2+i;
		esd = gf_odf_desc_esd_new(2);
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_IMAGE_JPEG;
		esd->ESID = 3+i;
		/*extract image and remember it*/
		mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
		gf_list_add(esd->extensionDescriptors, mi);
		mi->delete_file = 1;
		sprintf(szName, "%s_img%d.jpg", load->fileName, esd->ESID);
		mi->file_name = gf_strdup(szName);

		gf_list_add(od->ESDescriptors, esd);
		gf_list_add(odU->objectDescriptors, od);

		samp = gf_isom_get_sample(src, tk, i+1, &di);
		img = gf_fopen(mi->file_name, "wb");
		gf_fwrite(samp->data, samp->dataLength, 1, img);
		gf_fclose(img);
		gf_isom_sample_del(&samp);
	}
	gf_isom_delete(src);
	return GF_OK;
}

#endif	/*GPAC_DISABLE_QTVR*/
