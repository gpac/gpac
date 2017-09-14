/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

#if !defined(GPAC_DISABLE_SVG) && !defined(GPAC_DISABLE_ZLIB)

#include <zlib.h>

typedef struct
{
	GF_FilterPid *in_pid, *out_pid;
	GF_SceneLoader loader;
	GF_Scene *scene;
	u32 oti;
	const char *file_name;
	u32 file_size;
	u32 sax_max_duration;
	u16 base_es_id;
	u32 file_pos;
	gzFile src;
} SVGIn;

static Bool svg_check_download(SVGIn *svgin)
{
	u64 size;
	FILE *f = gf_fopen(svgin->file_name, "rb");
	if (!f) return GF_FALSE;
	gf_fseek(f, 0, SEEK_END);
	size = gf_ftell(f);
	gf_fclose(f);
	if (size==svgin->file_size) return GF_TRUE;
	return GF_FALSE;
}

#define SVG_PROGRESSIVE_BUFFER_SIZE		4096

static GF_Err svgin_deflate(SVGIn *svgin, const char *buffer, u32 buffer_len)
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
		e = GF_OK;
		while (d_stream.total_in < buffer_len) {
			err = inflate(&d_stream, Z_NO_FLUSH);
			if (err < Z_OK) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
			svg_data[d_stream.total_out - done] = 0;
			e = gf_sm_load_string(&svgin->loader, svg_data, GF_FALSE);
			if (e || (err== Z_STREAM_END)) break;
			done = (u32) d_stream.total_out;
			d_stream.avail_out = 2048;
			d_stream.next_out = (Bytef*)svg_data;
		}
		inflateEnd(&d_stream);
		return e;
	}
	return GF_NON_COMPLIANT_BITSTREAM;
}

static GF_Err svgin_process(GF_Filter *filter)
{
	Bool start, end;
	const char *data;
	u32 size;
	GF_Err e = GF_OK;
	GF_FilterPacket *pck;
	SVGIn *svgin = (SVGIn *) gf_filter_get_udta(filter);

	//no scene yet attached
	if (!svgin->scene) return GF_OK;

#ifdef FILTER_FIXME
	if (stream_time==(u32)-1) {
		if (svgin->src) gzclose(svgin->src);
		svgin->src = NULL;
		gf_sm_load_done(&svgin->loader);
		svgin->loader.fileName = NULL;
		svgin->file_pos = 0;
		gf_sg_reset(svgin->scene->graph);
		return GF_OK;
	}
#endif

	switch (svgin->oti) {
	/*!OTI for streaming SVG - GPAC internal*/
	case GPAC_OTI_SCENE_SVG:
		pck = gf_filter_pid_get_packet(svgin->in_pid);
		if (!pck) return GF_OK;
		data = gf_filter_pck_get_data(pck, &size);
		e = gf_sm_load_string(&svgin->loader, data, GF_FALSE);
		gf_filter_pid_drop_packet(svgin->in_pid);
		break;

	/*!OTI for streaming SVG + gz - GPAC internal*/
	case GPAC_OTI_SCENE_SVG_GZ:
		pck = gf_filter_pid_get_packet(svgin->in_pid);
		if (!pck) return GF_OK;
		data = gf_filter_pck_get_data(pck, &size);
		e = svgin_deflate(svgin, data, size);
		gf_filter_pid_drop_packet(svgin->in_pid);
		break;

	/*!OTI for DIMS (dsi = 3GPP DIMS configuration) - GPAC internal*/
	case GPAC_OTI_SCENE_DIMS:
	{
		u8 prev, dims_hdr;
		u32 nb_bytes, size;
		u64 pos;
		char * buf2 ;
		GF_BitStream *bs;

		pck = gf_filter_pid_get_packet(svgin->in_pid);
		if (!pck) return GF_OK;
		data = gf_filter_pck_get_data(pck, &size);

		buf2 = gf_malloc(size);
		bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
		memcpy(buf2, data, size);

		gf_filter_pid_drop_packet(svgin->in_pid);
		
		while (gf_bs_available(bs)) {
			pos = gf_bs_get_position(bs);
			size = gf_bs_read_u16(bs);
			nb_bytes = 2;
			/*GPAC internal hack*/
			if (!size) {
				size = gf_bs_read_u32(bs);
				nb_bytes = 6;
			}

			dims_hdr = gf_bs_read_u8(bs);
			prev = buf2[pos + nb_bytes + size];

			buf2[pos + nb_bytes + size] = 0;
			if (dims_hdr & GF_DIMS_UNIT_C) {
				e = svgin_deflate(svgin, buf2 + pos + nb_bytes + 1, size - 1);
			} else {
				e = gf_sm_load_string(&svgin->loader, buf2 + pos + nb_bytes + 1, GF_FALSE);
			}
			buf2[pos + nb_bytes + size] = prev;
			gf_bs_skip_bytes(bs, size-1);

		}
		gf_bs_del(bs);
	}
	break;

	default:
		if (!svgin->file_name) return GF_SERVICE_ERROR;

		pck = gf_filter_pid_get_packet(svgin->in_pid);
		if (!pck) return GF_OK;
		gf_filter_pck_get_framing(pck, &start, &end);
		gf_filter_pid_drop_packet( svgin->in_pid);

		/*full doc parsing*/
		if ((svgin->sax_max_duration==(u32) -1) && svgin->file_name) {
			if (end) {
				svgin->loader.fileName = svgin->file_name;
				e = gf_sm_load_init(&svgin->loader);
				if (!e) e = gf_sm_load_run(&svgin->loader);
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
				gf_sm_load_init(&svgin->loader);
			}
			e = GF_OK;
			entry_time = gf_sys_clock();

			while (1) {
				u32 diff;
				s32 nb_read;
				nb_read = gzread(svgin->src, file_buf, SVG_PROGRESSIVE_BUFFER_SIZE);
				/*we may have read nothing but we still need to call parse in case the parser got suspended*/
				if (nb_read<=0) {
					if ((e==GF_EOS) && gzeof(svgin->src)) {
						gf_set_progress("SVG Parsing", svgin->file_pos, svgin->file_size);
						gzclose(svgin->src);
						svgin->src = NULL;
						gf_sm_load_done(&svgin->loader);
					}
					goto exit;
				}

				file_buf[nb_read] = file_buf[nb_read+1] = 0;

				e = gf_sm_load_string(&svgin->loader, file_buf, GF_FALSE);
				svgin->file_pos += nb_read;



				/*handle decompression*/
				if (svgin->file_pos > svgin->file_size) svgin->file_size = svgin->file_pos + 1;
				if (e) break;

				gf_set_progress("SVG Parsing", svgin->file_pos, svgin->file_size);
				diff = gf_sys_clock() - entry_time;
				if (diff > svgin->sax_max_duration) {
					break;
				}
			}
		}
		break;
	}

exit:
	if ((e>=GF_OK) && (svgin->scene->graph_attached!=1) && (gf_sg_get_root_node(svgin->loader.scene_graph)!=NULL) ) {
		gf_scene_attach_to_compositor(svgin->scene);
	}
	/*prepare for next playback*/
	if (e) {
		gf_sm_load_done(&svgin->loader);
		svgin->loader.fileName = NULL;
		e = GF_EOS;
	}
	return e;
}

static GF_Err svgin_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const char *sOpt;
	SVGIn *svgin = (SVGIn *) gf_filter_get_udta(filter);
	const GF_PropertyValue *prop;

	if (is_remove) {
		svgin->in_pid = NULL;
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (prop && prop->value.uint) svgin->oti = prop->value.uint;

	//we must have a file path or OTI
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (prop && prop->value.string) svgin->file_name = prop->value.string;
	if (!svgin->oti && !svgin->file_name)
		return GF_NOT_SUPPORTED;

	svgin->loader.type = GF_SM_LOAD_SVG;
	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	switch (svgin->oti) {
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
		break;
	}

	if (!svgin->in_pid) {
		svgin->in_pid = pid;
		if (svgin->file_name) {
			GF_FilterEvent evt;
			evt.base.type = GF_FEVT_FILE_NO_PCK;
			gf_filter_pid_send_event(pid, &evt);
		}
	} else {
		if (pid != svgin->in_pid) {
			return GF_REQUIRES_NEW_INSTANCE;
		}
		//check for filename change
		return GF_OK;
	}

	//declare a new output PID of type STREAM, OTI RAW
	svgin->out_pid = gf_filter_pid_new(filter);

	gf_filter_pid_reset_properties(svgin->out_pid);
	gf_filter_pid_copy_properties(svgin->out_pid, pid);
	gf_filter_pid_set_property(svgin->out_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_SCENE) );
	gf_filter_pid_set_property(svgin->out_pid, GF_PROP_PID_OTI, &PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM) );
	gf_filter_pid_set_property(svgin->out_pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_udta(pid, svgin->out_pid);


	svgin->sax_max_duration = (u32) -1;

#ifdef FILTER_FIXME
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
#endif

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

static Bool svgin_process_event(GF_Filter *filter, GF_FilterEvent *com)
{
	u32 count, i;
	SVGIn *svgin = gf_filter_get_udta(filter);
	GF_FilterPid *ipid;
	//check for scene attach
	switch (com->base.type) {
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_RESET_SCENE:
		gf_sm_load_done(&svgin->loader);
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (!com->attach_scene.on_pid) return GF_TRUE;

	ipid = gf_filter_pid_get_udta(com->attach_scene.on_pid);
	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(ipid);
		//we found our pid, set it up
		if (opid == com->attach_scene.on_pid) {
			if (!svgin->scene) {
				GF_ObjectManager *odm = com->attach_scene.object_manager;
				svgin->scene = odm->subscene ? odm->subscene : odm->parentscene;

				memset(&svgin->loader, 0, sizeof(GF_SceneLoader));
				svgin->loader.is = svgin->scene;
				svgin->scene = svgin->scene;
				svgin->loader.scene_graph = svgin->scene->graph;
#ifdef FILTER_FIXME
				svgin->loader.localPath = gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
#endif
				/*Warning: svgin->loader.type may be overriden in attach stream */
				svgin->loader.type = GF_SM_LOAD_SVG;
				svgin->loader.flags = GF_SM_LOAD_FOR_PLAYBACK;

				if (svgin->oti!= GPAC_OTI_PRIVATE_SCENE_SVG)
					gf_sm_load_init(&svgin->loader);

//				gf_sg_set_node_callback(svgin->scene->graph, CTXLoad_NodeCallback);
//				priv->service_url = odm->scene_ns->url;
//				if (!priv->ctx)	CTXLoad_Setup(filter, priv);

			}
			return GF_TRUE;
		}
	}

	return GF_FALSE;
}


static const GF_FilterCapability SVGInInputs[] =
{
	{.code=GF_PROP_PID_MIME, PROP_STRING("image/svg+xml"), .start=GF_TRUE},
	{.code=GF_PROP_PID_FILE_EXT, PROP_STRING("svg|svgz|svg.gz"), .start=GF_TRUE},

	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_SCENE_SVG), .start=GF_TRUE},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_SCENE_SVG_GZ), .start=GF_TRUE},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_SCENE_DIMS), .start=GF_TRUE},

	{}
};

static const GF_FilterCapability SVGInOutputs[] =
{
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_SCENE)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM )},
	{}
};

GF_FilterRegister SVGInRegister = {
	.name = "svg_in",
	.description = "SVG playback",
	.private_size = sizeof(SVGIn),
	.requires_main_thread = GF_TRUE,
	.args = NULL,
	.input_caps = SVGInInputs,
	.output_caps = SVGInOutputs,
	.process = svgin_process,
	.configure_pid = svgin_configure_pid,
	.update_arg = NULL,
	.process_event = svgin_process_event,
};

#endif


const GF_FilterRegister *svgin_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_SVG) && !defined(GPAC_DISABLE_ZLIB)
	return &SVGInRegister;
#else
	return NULL;
#endif
}
