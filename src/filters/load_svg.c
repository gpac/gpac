/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG loader filter
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


#include <gpac/internal/compositor_dev.h>
#include <gpac/scene_manager.h>
#include <gpac/constants.h>

#if !defined(GPAC_DISABLE_SVG) && !defined(GPAC_DISABLE_ZLIB)

#include <zlib.h>

typedef struct
{
	//opts
	u32 sax_dur;

	//internal
	GF_FilterPid *in_pid, *out_pid;
	GF_SceneLoader loader;
	GF_Scene *scene;
	u32 codecid;
	const char *file_name;
	u32 file_size;
	u16 base_es_id;
	u32 file_pos;
	gzFile src;
	Bool is_playing;
} SVGIn;


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
	const u8 *data;
	u32 size;
	GF_Err e = GF_OK;
	GF_FilterPacket *pck;
	SVGIn *svgin = (SVGIn *) gf_filter_get_udta(filter);

	//no scene yet attached
	if (!svgin->scene) {
		if (svgin->is_playing) {
			gf_filter_pid_set_eos(svgin->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}

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

	switch (svgin->codecid) {
	/*! streaming SVG*/
	case GF_CODECID_SVG:
		pck = gf_filter_pid_get_packet(svgin->in_pid);
		if (!pck) return GF_OK;
		data = gf_filter_pck_get_data(pck, &size);
		e = gf_sm_load_string(&svgin->loader, data, GF_FALSE);
		gf_filter_pid_drop_packet(svgin->in_pid);
		break;

	/*! streaming SVG + gz*/
	case GF_CODECID_SVG_GZ:
		pck = gf_filter_pid_get_packet(svgin->in_pid);
		if (!pck) return GF_OK;
		data = gf_filter_pck_get_data(pck, &size);
		e = svgin_deflate(svgin, data, size);
		gf_filter_pid_drop_packet(svgin->in_pid);
		break;

	/*! DIMS (dsi = 3GPP DIMS configuration)*/
	case GF_CODECID_DIMS:
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
		bs = gf_bs_new((u8 *)data, size, GF_BITSTREAM_READ);
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
		if ((svgin->sax_dur == 0) && svgin->file_name) {
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
				if (diff > svgin->sax_dur) {
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
		gf_filter_pid_set_eos(svgin->out_pid);
		e = GF_EOS;
	}
	return e;
}

static GF_Err svgin_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	SVGIn *svgin = (SVGIn *) gf_filter_get_udta(filter);
	const GF_PropertyValue *prop;

	if (is_remove) {
		svgin->in_pid = NULL;
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (prop && prop->value.uint) svgin->codecid = prop->value.uint;

	//we must have a file path or codecid
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (prop && prop->value.string) svgin->file_name = prop->value.string;
	if (!svgin->codecid && !svgin->file_name)
		return GF_NOT_SUPPORTED;

	svgin->loader.type = GF_SM_LOAD_SVG;
	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	switch (svgin->codecid) {
	case GF_CODECID_SVG:
	case GF_CODECID_SVG_GZ:
		svgin->loader.flags |= GF_SM_LOAD_CONTEXT_STREAMING;
		/*no decSpecInfo defined for streaming svg yet*/
		break;
	case GF_CODECID_DIMS:
		svgin->loader.type = GF_SM_LOAD_DIMS;
		svgin->loader.flags |= GF_SM_LOAD_CONTEXT_STREAMING;
		/*decSpecInfo not yet supported for DIMS svg - we need properties at the scene level to store the
		various indications*/
		break;
	default:
		break;
	}

	if (!svgin->in_pid) {
		GF_FilterEvent fevt;
		svgin->in_pid = pid;

		//we work with full file only, send a play event on source to indicate that
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, pid);
		fevt.play.start_range = 0;
		fevt.base.on_pid = svgin->in_pid;
		fevt.play.full_file_only = GF_TRUE;
		gf_filter_pid_send_event(svgin->in_pid, &fevt);

	} else {
		if (pid != svgin->in_pid) {
			return GF_REQUIRES_NEW_INSTANCE;
		}
		//check for filename change
		return GF_OK;
	}

	//declare a new output PID of type scene, codecid RAW
	svgin->out_pid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(svgin->out_pid, pid);
	gf_filter_pid_set_property(svgin->out_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_SCENE) );
	gf_filter_pid_set_property(svgin->out_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(svgin->out_pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_udta(pid, svgin->out_pid);


	if (svgin->file_name) {
		gf_filter_set_name(filter,  ((svgin->sax_dur==0) && svgin->file_size) ? "SVGLoad" : "SVGLoad:Progressive");
	} else if (svgin->codecid==GF_CODECID_SVG) {
		gf_filter_set_name(filter,  "SVG:Streaming");
	} else if (svgin->codecid==GF_CODECID_SVG_GZ) {
		gf_filter_set_name(filter,  "SVG:Streaming:GZ");
	} else if (svgin->codecid==GF_CODECID_DIMS) {
		gf_filter_set_name(filter,  "SVG:DIMS");
	}

	return GF_OK;
}

static Bool svgin_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	u32 count, i;
	SVGIn *svgin = gf_filter_get_udta(filter);
	//check for scene attach
	switch (com->base.type) {
	case GF_FEVT_PLAY:
		//cancel play event, we work with full file
		svgin->is_playing = GF_TRUE;
		return GF_TRUE;
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_RESET_SCENE:
		gf_sm_load_done(&svgin->loader);
		svgin->scene = NULL;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (!com->attach_scene.on_pid) return GF_TRUE;

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
				svgin->loader.localPath = gf_get_default_cache_directory();
				
				/*Warning: svgin->loader.type may be overriden in attach stream */
				svgin->loader.type = GF_SM_LOAD_SVG;
				svgin->loader.flags = GF_SM_LOAD_FOR_PLAYBACK;

				if (! svgin->file_name)
					gf_sm_load_init(&svgin->loader);

				if (svgin->scene->root_od->ck && !svgin->scene->root_od->ck->clock_init)
					gf_clock_set_time(svgin->scene->root_od->ck, 0);

				//init clocks
				gf_odm_check_buffering(svgin->scene->root_od, svgin->in_pid);
			}
			return GF_TRUE;
		}
	}

	return GF_FALSE;
}

static const GF_FilterCapability SVGInCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "image/svg+xml"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "svg|svgz|svg.gz"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SVG),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SVG_GZ),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_DIMS),
};


#define OFFS(_n)	#_n, offsetof(SVGIn, _n)

static const GF_FilterArgs SVGInArgs[] =
{
	{ OFFS(sax_dur), "loading duration for SAX parsing, 0 disables SAX parsing", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};
GF_FilterRegister SVGInRegister = {
	.name = "svgplay",
	GF_FS_SET_DESCRIPTION("SVG loader")
	GF_FS_SET_HELP("This filter parses SVG files directly into the scene graph of the compositor. It cannot be used to dump content.")
	.private_size = sizeof(SVGIn),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = SVGInArgs,
	SETCAPS(SVGInCaps),
	.process = svgin_process,
	.configure_pid = svgin_configure_pid,
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
