/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Deniz Ugur
 *			Copyright (c) Motion Spell 2025-
 *					All rights reserved
 *
 *  This file is part of GPAC / SMIL demuxer filter
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

#include <gpac/filters.h>
#include <gpac/xml.h>

#ifndef GPAC_DISABLE_SMIL2PL

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif

typedef struct
{
	GF_FilterPid *ipid, *opid;
	GF_DOMParser *parser;
	GF_XMLNode *root;
	GF_FileIO *fio;
} GF_SMIL2PLCtx;

GF_Err smil2pl_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_SMIL2PLCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove)
	{
		ctx->ipid = NULL;
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid)
	{
		ctx->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_copy_properties(ctx->opid, pid);

	// the output file is no longer cached
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_CACHED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILEPATH, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_URL, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("pl"));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/x-gpac-playlist"));

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	// check multithreaded FileIO restrictions
	const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (p && p->value.string && gf_fileio_is_main_thread(p->value.string))
	{
		gf_filter_force_main_thread(filter, GF_TRUE);
	}

	return GF_OK;
}

GF_Err smil2pl_process(GF_Filter *filter)
{
	GF_SMIL2PLCtx *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *p;
	GF_FilterPacket *pck, *dst;
	u32 pkt_size;
	Bool start, end;
	u8 *data, *output;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (u8 *)gf_filter_pck_get_data(pck, &pkt_size);
	gf_filter_pck_get_framing(pck, &start, &end);
	// for now we only work with complete files
	gf_assert(end);

	// create the DOM parser
	if (!ctx->parser)
	{
		const char *src_url;
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
		if (!p)
		{
			if (ctx->fio)
				gf_fclose((FILE *)ctx->fio);
			ctx->fio = gf_fileio_from_mem(NULL, data, pkt_size);
			src_url = gf_fileio_url(ctx->fio);
		}
		else
		{
			src_url = p->value.string;
		}
		if (!gf_file_exists(src_url))
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Smil2pl] Cannot find SMIL file %s\n", src_url));
			return GF_URL_ERROR;
		}

		ctx->parser = gf_xml_dom_new();
		GF_Err e = gf_xml_dom_parse(ctx->parser, src_url, NULL, NULL);
		if (e)
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Smil2pl] Error parsing SMIL file: Line %d - %s\n", gf_xml_dom_get_line(ctx->parser), gf_xml_dom_get_error(ctx->parser)));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	ctx->root = gf_xml_dom_get_root(ctx->parser);
	if (!ctx->root)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Smil2pl] Error parsing SMIL file - no root node found\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	// create temporary output file
	FILE *dump = gf_file_temp(NULL);
	gf_fprintf(dump, "## begin\n");

	// start conversion
	GF_XMLNode *node;
	u32 i = 0;
	while ((node = (GF_XMLNode *)gf_list_enum(ctx->root->content, &i)))
	{
		if (node->type)
			continue;
		// skip head for now
		if (!stricmp(node->name, "head"))
			continue;
		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[Smil2pl] Unsupported SMIL element: %s\n", node->name));
	}

	gf_fprintf(dump, "/home/deniz/workspace/gpac/gpac_public/testsuite/external_media/counter/counter_30s_GDR_320x180_160kbps.264\n");
	gf_fprintf(dump, "## end\n");

	// send output packet
	if (dump && gf_ftell(dump))
	{
		u32 size = (u32)gf_ftell(dump);
		dst = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		if (!dst)
			return GF_OUT_OF_MEM;
		gf_fseek(dump, 0, SEEK_SET);
		gf_fread(output, size, dump);
		gf_filter_pck_merge_properties(pck, dst);
		gf_filter_pck_set_byte_offset(dst, GF_FILTER_NO_BO);
		gf_filter_pck_set_framing(dst, GF_TRUE, GF_TRUE);

		gf_filter_pck_send(dst);
	}
	if (dump)
		gf_fclose(dump);

	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

GF_Err smil2pl_initialize(GF_Filter *filter)
{
	// GF_SMIL2PLCtx *ctx = gf_filter_get_udta(filter);
	return GF_OK;
}

void smil2pl_finalize(GF_Filter *filter)
{
	GF_SMIL2PLCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->parser)
		gf_xml_dom_del(ctx->parser);
	if (ctx->fio)
		gf_fclose((FILE *)ctx->fio);
}

#define OFFS(_n) #_n, offsetof(GF_SMIL2PLCtx, _n)
static const GF_FilterArgs GF_SMIL2PLArgs[] = {
	{0}
};

static const GF_FilterCapability SMIL2PLCaps[] = {
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "smil"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/smil"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "pl"),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_MIME, "application/x-gpac-playlist"),
};

GF_FilterRegister SMIL2PLRegister = {
	.name = "smil2pl",
	GF_FS_SET_DESCRIPTION("SMIL to GPAC Playlist converter")
	GF_FS_SET_HELP("This filter reads SMIL 2.0 Basic Profile file and converts it to a GPAC playlist format.")
	.private_size = sizeof(GF_SMIL2PLCtx),
	.flags = GF_FS_REG_USE_SYNC_READ,
	.args = GF_SMIL2PLArgs,
	.initialize = smil2pl_initialize,
	.finalize = smil2pl_finalize,
	SETCAPS(SMIL2PLCaps),
	.configure_pid = smil2pl_configure_pid,
	.process = smil2pl_process,
	.hint_class_type = GF_FS_CLASS_TOOL
};

const GF_FilterRegister *smil2pl_register(GF_FilterSession *session)
{
	return &SMIL2PLRegister;
}
#else
const GF_FilterRegister *smil2pl_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_SMIL2PL

// TODO: Implement SMIL 2.0 Basic Profile - https://www.w3.org/TR/2005/REC-SMIL2-20050107/smil-basic.html
// TODO(high): Convert SMIL to GPAC playlist format and use playlist filter to handle it
// FIXME: Load filelist implicitly?