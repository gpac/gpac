/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / NHML stream to file filter
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
#include <gpac/constants.h>
#include <gpac/bitstream.h>
#include <gpac/base_coding.h>

#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ZLIB
#include <zlib.h>
#endif

typedef struct
{
	//opts
	const char *name;
	Bool exporter, dims, full, nhmlonly, chksum;
	FILE *filep;


	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid_nhml, *opid_mdia, *opid_info;

	u32 codecid;
	u32 streamtype;
	u32 oti;
	u32 chan, sr, bps, w, h;
	const char *dcfg;
	u32 dcfg_size;
	char *media_file, *info_file;
	const char *szRootName;

	GF_Fraction duration;
	Bool first;
	Bool uncompress, is_dims, is_stpp;

	GF_BitStream *bs_w, *bs_r;
	u8 *nhml_buffer;
	u32 nhml_buffer_size;
	char *b64_buffer;
	u32 b64_buffer_size;
	u64 mdia_pos;
	u32 pck_num;
} GF_NHMLDumpCtx;

GF_Err nhmldump_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cid;
	char *mime=NULL, *name, *ext;
	char fileName[1024];
	const GF_PropertyValue *p;
	GF_NHMLDumpCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid_nhml);
		gf_filter_pid_remove(ctx->opid_mdia);
		if (ctx->opid_info) gf_filter_pid_remove(ctx->opid_info);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	cid = p->value.uint;

	if (ctx->codecid == cid) {
		return GF_OK;
	}
	ctx->codecid = cid;

	if (ctx->codecid<GF_CODECID_LAST_MPEG4_MAPPING) {
		ctx->oti = ctx->codecid;
	} else {
		ctx->oti = gf_codecid_oti(ctx->codecid);
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	ctx->streamtype = p ? p->value.uint : GF_STREAM_UNKNOWN;

	if (!ctx->opid_nhml && !ctx->filep)
		ctx->opid_nhml = gf_filter_pid_new(filter);

	ctx->is_dims = GF_FALSE;
	if ((ctx->codecid == GF_CODECID_DIMS) && ctx->dims) {
		gf_filter_pid_remove(ctx->opid_mdia);
		ctx->opid_mdia = NULL;
		ctx->is_dims = GF_TRUE;
	} else if (!ctx->opid_mdia && !ctx->nhmlonly)
		ctx->opid_mdia = gf_filter_pid_new(filter);

	//file pointer set, we act as a sink, send play
	if (!ctx->ipid && ctx->filep) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &evt);
	}
	ctx->ipid = pid;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	ctx->sr = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	ctx->chan = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
	ctx->bps = p ? gf_audio_fmt_bit_depth(p->value.uint) : 16;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	ctx->w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	ctx->h = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		ctx->dcfg = p->value.data.ptr;
		ctx->dcfg_size = p->value.data.size;

		if (!ctx->opid_info && !ctx->nhmlonly) {
			ctx->opid_info = gf_filter_pid_new(filter);
		}

	} else if (ctx->opid_info) {
		if (ctx->opid_info) gf_filter_pid_remove(ctx->opid_info);
	}
	if (ctx->info_file) gf_free(ctx->info_file);
	ctx->info_file = NULL;

	name = (char*) gf_codecid_name(ctx->codecid);
	if (ctx->exporter) {
		if (ctx->w && ctx->h) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - Size %dx%d\n", name, ctx->w, ctx->h));
		} else if (ctx->sr && ctx->chan) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - SampleRate %d %d channels %d bits per sample\n", name, ctx->sr, ctx->chan, ctx->bps));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s\n", name));
		}
	}

	if (ctx->opid_nhml) {
		mime = ctx->dims ? "application/dims" : "application/x-nhml";
		ext = ctx->dims ? "dims" : "nhml";

		strncpy(fileName, ctx->name, 1024);
		name = gf_file_ext_start(fileName);
		if (name) {
			name[0] = 0;
			gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_OUTPATH, &PROP_STRING(ctx->name) );
		} else {
			strcat(fileName, ".nhml");
			gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_OUTPATH, &PROP_STRING(fileName) );
			name = strrchr(fileName, '.');
		}

		gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_MIME, &PROP_STRING(mime) );
		gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_FILE_EXT, &PROP_STRING(ext) );
	}

	if (ctx->opid_mdia) {
		gf_filter_pid_set_property(ctx->opid_mdia, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid_mdia, GF_PROP_PID_MIME, &PROP_STRING(mime) );
		name[0] = 0;
		strcat(fileName, ".media");
		gf_filter_pid_set_property(ctx->opid_mdia, GF_PROP_PID_OUTPATH, &PROP_STRING(fileName) );
		if (ctx->media_file) gf_free(ctx->media_file);
		ctx->media_file = gf_strdup(fileName);
		gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_FILE_EXT, &PROP_STRING("media") );
	}

	if (ctx->opid_info) {
		gf_filter_pid_set_property(ctx->opid_info, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid_info, GF_PROP_PID_MIME, &PROP_STRING(mime) );
		name[0] = 0;
		strcat(fileName, ".info");
		gf_filter_pid_set_property(ctx->opid_info, GF_PROP_PID_OUTPATH, &PROP_STRING(fileName) );
		ctx->info_file = gf_strdup(fileName);
		gf_filter_pid_set_property(ctx->opid_nhml, GF_PROP_PID_FILE_EXT, &PROP_STRING("info") );
	}

	ctx->first = GF_TRUE;
	ctx->is_stpp = (cid==GF_CODECID_SUBS_XML) ? GF_TRUE : GF_FALSE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) ctx->duration = p->value.frac;


	if (ctx->opid_nhml)
		gf_filter_pid_set_name(ctx->opid_nhml, "nhml");
	if (ctx->opid_mdia)
		gf_filter_pid_set_name(ctx->opid_mdia, "media");
	if (ctx->opid_info)
		gf_filter_pid_set_name(ctx->opid_info, "info");

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	return GF_OK;
}

#define NHML_PRINT_4CC(_code, _pname, _name) \
		if (_code) p = gf_filter_pid_get_property(ctx->ipid, _code); \
		else p = gf_filter_pid_get_property_str(ctx->ipid, _pname); \
		if (p) { \
			sprintf(nhml, "%s=\"%s\" ", _name, gf_4cc_to_str(p->value.uint)); \
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml)); \
		}

#define NHML_PRINT_UINT(_code, _pname, _name) \
		if (_code) p = gf_filter_pid_get_property(ctx->ipid, _code); \
		else p = gf_filter_pid_get_property_str(ctx->ipid, _pname); \
		if (p) { \
			sprintf(nhml, "%s=\"%d\" ", _name, p->value.uint); \
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml)); \
		}

#define NHML_PRINT_STRING(_code, _pname, _name) \
		if (_code) p = gf_filter_pid_get_property(ctx->ipid, _code); \
		else p = gf_filter_pid_get_property_str(ctx->ipid, _pname); \
		if (p) { \
			sprintf(nhml, "%s=\"%s\" ", _name, p->value.string); \
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml)); \
		}



static void nhmldump_send_header(GF_NHMLDumpCtx *ctx)
{
	GF_FilterPacket *dst_pck;
	char nhml[1024];
	u32 size;
	u8 *output;
	const GF_PropertyValue *p;

	ctx->szRootName = "NHNTStream";
	if (ctx->dims) {
		ctx->szRootName = "DIMSStream";
	} else if (!ctx->filep) {
		sprintf(nhml, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	}

	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

	/*write header*/
	sprintf(nhml, "<%s version=\"1.0\" ", ctx->szRootName);
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));


	NHML_PRINT_UINT(GF_PROP_PID_ID, NULL, "trackID")
	NHML_PRINT_UINT(GF_PROP_PID_TIMESCALE, NULL, "timeScale")

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_IN_IOD);
	if (p && p->value.boolean) {
		sprintf(nhml, "inRootOD=\"yes\" ");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	}

	if (ctx->oti && (ctx->oti<GF_CODECID_LAST_MPEG4_MAPPING)) {
		sprintf(nhml, "streamType=\"%d\" objectTypeIndication=\"%d\" ", ctx->streamtype, ctx->oti);
		gf_bs_write_data(ctx->bs_w, nhml, (u32)strlen(nhml));
	} else {
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_SUBTYPE);
		if (p) {
			sprintf(nhml, "%s=\"%s\" ", "mediaType", gf_4cc_to_str(p->value.uint));
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

			NHML_PRINT_4CC(GF_PROP_PID_ISOM_SUBTYPE, "mediaSubType", "mediaSubType")
		} else {
			NHML_PRINT_4CC(GF_PROP_PID_CODECID, NULL, "codecID")
		}
	}

	if (ctx->w && ctx->h) {
		//compatibility with old arch, we might want to remove this
		switch (ctx->streamtype) {
		case GF_STREAM_VISUAL:
		case GF_STREAM_SCENE:
			sprintf(nhml, "width=\"%d\" height=\"%d\" ", ctx->w, ctx->h);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
			break;
		default:
			break;
		}
	}
	else if (ctx->sr && ctx->chan) {
		sprintf(nhml, "sampleRate=\"%d\" numChannels=\"%d\" ", ctx->sr, ctx->chan);
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		sprintf(nhml, "sampleRate=\"%d\" numChannels=\"%d\" ", ctx->sr, ctx->chan);
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_AUDIO_FORMAT);
		sprintf(nhml, "bitsPerSample=\"%d\" ", gf_audio_fmt_bit_depth(p->value.uint));
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	}

	NHML_PRINT_4CC(0, "codec_vendor", "codecVendor")
	NHML_PRINT_UINT(0, "codec_version", "codecVersion")
	NHML_PRINT_UINT(0, "codec_revision", "codecRevision")
	NHML_PRINT_STRING(0, "compressor_name", "compressorName")
	NHML_PRINT_UINT(0, "temporal_quality", "temporalQuality")
	NHML_PRINT_UINT(0, "spatial_quality", "spatialQuality")
	NHML_PRINT_UINT(0, "hres", "horizontalResolution")
	NHML_PRINT_UINT(0, "vres", "verticalResolution")
	NHML_PRINT_UINT(GF_PROP_PID_BIT_DEPTH_Y, NULL, "bitDepth")

	NHML_PRINT_STRING(0, "meta:xmlns", "xml_namespace")
	NHML_PRINT_STRING(0, "meta:schemaloc", "xml_schema_location")
	NHML_PRINT_STRING(0, "meta:mime", "mime_type")

	NHML_PRINT_STRING(0, "meta:config", "config")
	NHML_PRINT_STRING(0, "meta:aux_mimes", "aux_mime_type")

	if (ctx->codecid == GF_CODECID_DIMS) {
		if (gf_filter_pid_get_property_str(ctx->ipid, "meta:xmlns")==NULL) {
			sprintf(nhml, "xmlns=\"http://www.3gpp.org/richmedia\" ");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}

		NHML_PRINT_UINT(0, "dims:profile", "profile")
		NHML_PRINT_UINT(0, "dims:level", "level")
		NHML_PRINT_UINT(0, "dims:pathComponents", "pathComponents")

		p = gf_filter_pid_get_property_str(ctx->ipid, "dims:fullRequestHost");
		if (p) {
			sprintf(nhml, "useFullRequestHost=\"%s\" ", p->value.boolean ? "yes" : "no");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		p = gf_filter_pid_get_property_str(ctx->ipid, "dims:streamType");
		if (p) {
			sprintf(nhml, "stream_type=\"%s\" ", p->value.boolean ? "primary" : "secondary");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		p = gf_filter_pid_get_property_str(ctx->ipid, "dims:redundant");
		if (p) {
			sprintf(nhml, "contains_redundant=\"%s\" ", (p->value.uint==1) ? "main" : ((p->value.uint==1) ? "redundant" : "main+redundant") );
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		NHML_PRINT_UINT(0, "dims:scriptTypes", "scriptTypes")
	}

	//send DCD
	if (ctx->opid_info) {
		sprintf(nhml, "specificInfoFile=\"%s\" ", gf_file_basename(ctx->info_file) );
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

		dst_pck = gf_filter_pck_new_shared(ctx->opid_info, ctx->dcfg, ctx->dcfg_size, NULL);
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
		gf_filter_pck_set_readonly(dst_pck);
		gf_filter_pck_send(dst_pck);
	}

	NHML_PRINT_STRING(0, "meta:encoding", "encoding")
	NHML_PRINT_STRING(0, "meta:contentEncoding", "content_encoding")
	ctx->uncompress = GF_FALSE;
	if (p) {
		if (!strcmp(p->value.string, "deflate")) ctx->uncompress = GF_TRUE;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[NHMLMx] content_encoding %s not supported\n", p->value.string ));
		}
	}

	if (ctx->opid_mdia) {
		sprintf(nhml, "baseMediaFile=\"%s\" ", gf_file_basename(ctx->media_file) );
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	}
	sprintf(nhml, ">\n");
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

	gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->nhml_buffer, &size, &ctx->nhml_buffer_size);

	if (ctx->filep) {
		fwrite(ctx->nhml_buffer, 1, size, ctx->filep);
		return;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid_nhml, size, &output);
	memcpy(output, ctx->nhml_buffer, size);
	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
	gf_filter_pck_send(dst_pck);
}

static void nhmldump_send_dims(GF_NHMLDumpCtx *ctx, char *data, u32 data_size, GF_FilterPacket *pck)
{
	char nhml[1024];
	u32 size;
	u8 *output;
	GF_FilterPacket *dst_pck;
	u64 dts = gf_filter_pck_get_dts(pck);
	u64 cts = gf_filter_pck_get_cts(pck);

	if (dts==GF_FILTER_NO_TS) dts = cts;
	if (cts==GF_FILTER_NO_TS) cts = dts;

	if (!ctx->bs_r) ctx->bs_r = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs_r, data, data_size);

	while (gf_bs_available(ctx->bs_r)) {
		u64 pos = gf_bs_get_position(ctx->bs_r);
		u16 size = gf_bs_read_u16(ctx->bs_r);
		u8 flags = gf_bs_read_u8(ctx->bs_r);
		u8 prev;

		if (pos+size+2 > data_size)
			break;

		prev = data[pos+2+size];
		data[pos+2+size] = 0;


		sprintf(nhml, "<DIMSUnit time=\""LLU"\"", cts);
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

		/*DIMS flags*/
		if (flags & GF_DIMS_UNIT_S) {
			sprintf(nhml, " is-Scene=\"yes\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		if (flags & GF_DIMS_UNIT_M) {
			sprintf(nhml, " is-RAP=\"yes\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		if (flags & GF_DIMS_UNIT_I) {
			sprintf(nhml, " is-redundant=\"yes\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		if (flags & GF_DIMS_UNIT_D) {
			sprintf(nhml, " redundant-exit=\"yes\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		if (flags & GF_DIMS_UNIT_P) {
			sprintf(nhml, " priority=\"high\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		if (flags & GF_DIMS_UNIT_C) {
			sprintf(nhml, " compressed=\"yes\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		sprintf(nhml, ">");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		if (ctx->uncompress && (flags & GF_DIMS_UNIT_C)) {
#ifndef GPAC_DISABLE_ZLIB
			char svg_data[2049];
			int err;
			u32 done = 0;
			z_stream d_stream;
			d_stream.zalloc = (alloc_func)0;
			d_stream.zfree = (free_func)0;
			d_stream.opaque = (voidpf)0;
			d_stream.next_in  = (Bytef*) data+pos+3;
			d_stream.avail_in = size-1;
			d_stream.next_out = (Bytef*)svg_data;
			d_stream.avail_out = 2048;

			err = inflateInit(&d_stream);
			if (err == Z_OK) {
				while ((s32) d_stream.total_in < size-1) {
					err = inflate(&d_stream, Z_NO_FLUSH);
					if (err < Z_OK) break;
					svg_data[d_stream.total_out - done] = 0;
					gf_bs_write_data(ctx->bs_w, svg_data, (u32) strlen(svg_data));

					if (err== Z_STREAM_END) break;
					done = (u32) d_stream.total_out;
					d_stream.avail_out = 2048;
					d_stream.next_out = (Bytef*)svg_data;
				}
				inflateEnd(&d_stream);
			}
#else
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Error: your version of GPAC was compiled with no libz support."));
			gf_bs_del(ctx->bs_r);
			return;
#endif
		} else {
			gf_bs_write_data(ctx->bs_w, data+pos+3, size-1);
		}
		sprintf(nhml, "</DIMSUnit>\n");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

		data[pos+2+size] = prev;
		gf_bs_skip_bytes(ctx->bs_r, size-1);
	}

	gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->nhml_buffer, &size, &ctx->nhml_buffer_size);

	if (ctx->filep) {
		fwrite(ctx->nhml_buffer, 1, size, ctx->filep);
		return;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid_nhml, size, &output);
	memcpy(output, ctx->nhml_buffer, size);
	gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
	gf_filter_pck_send(dst_pck);
}


static void nhmldump_pck_property(GF_NHMLDumpCtx *ctx, u32 p4cc, const char *pname, const GF_PropertyValue *att)
{
	u32 i;
	char nhml[1024];
	char pval[GF_PROP_DUMP_ARG_SIZE];
	if (!pname) pname = gf_props_4cc_get_name(p4cc);

	sprintf(nhml, "%s=\"", pname ? pname : gf_4cc_to_str(p4cc));
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));



	switch (att->type) {
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
	case GF_PROP_DATA_NO_COPY:
		sprintf(nhml, "0x");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		for (i=0; i<att->value.data.size; i++) {
			sprintf(nhml, "%02X", (unsigned char) att->value.data.ptr[i]);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		nhml[0] = 0;
		break;
	default:
		sprintf(nhml, "%s", gf_prop_dump_val(att, pval, GF_FALSE, NULL) );
		break;
	}
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	sprintf(nhml, "\"");
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
}

static void nhmldump_send_frame(GF_NHMLDumpCtx *ctx, char *data, u32 data_size, GF_FilterPacket *pck)
{
	GF_FilterPacket *dst_pck;
	char nhml[1024];
	const GF_PropertyValue *p;
	u32 size;
	u8 *output;
	GF_FilterSAPType sap = gf_filter_pck_get_sap(pck);
	u64 dts = gf_filter_pck_get_dts(pck);
	u64 cts = gf_filter_pck_get_cts(pck);

	if (dts==GF_FILTER_NO_TS) dts = cts;
	if (cts==GF_FILTER_NO_TS) cts = dts;

	ctx->pck_num++;
	sprintf(nhml, "<NHNTSample number=\"%d\" DTS=\""LLU"\" dataLength=\"%d\" ", ctx->pck_num, dts, data_size);
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	if (ctx->full || (cts != dts) ) {
		sprintf(nhml, "CTSOffset=\"%d\" ", (s32) ((s64)cts - (s64)dts));
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	}
	if (sap==GF_FILTER_SAP_1) {
		sprintf(nhml, "isRAP=\"yes\" ");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	} else if (sap) {
		sprintf(nhml, "SAPType=\"%d\" ", sap);
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	} else if (ctx->full) {
		sprintf(nhml, "isRAP=\"no\" ");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		if (sap==GF_FILTER_SAP_4) {
			s32 roll = gf_filter_pck_get_roll_info(pck);
			sprintf(nhml, "SAPType=\"4\" roll=\"%d\" ", roll);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
	}

	if (ctx->full) {
		u64 bo;
		u32 duration, idx;
		sprintf(nhml, "mediaOffset=\""LLU"\" ", ctx->mdia_pos);
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

		bo = gf_filter_pck_get_byte_offset(pck);
		if (bo!=GF_FILTER_NO_BO) {
			sprintf(nhml, "sourceByteOffset=\""LLU"\" ", bo);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		duration = gf_filter_pck_get_duration(pck);
		if (duration) {
			sprintf(nhml, "duration=\"%d\" ", duration);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		idx = gf_filter_pck_get_carousel_version(pck);
		if (idx) {
			sprintf(nhml, "carouselVersion=\"%d\" ", idx);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
		idx = 0;
		while (1) {
			u32 prop_4cc;
			const char *prop_name;
			const GF_PropertyValue * p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			if (prop_4cc == GF_PROP_PCK_SUBS) continue;
			nhmldump_pck_property(ctx, prop_4cc, prop_name, p);
		}
	}

	if (ctx->chksum) {
		if (ctx->chksum==1) {
			u32 crc = gf_crc_32(data, data_size);
			sprintf(nhml, "crc=\"%08X\" ", crc);
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		} else {
			u32 j;
			u8 hash[GF_SHA1_DIGEST_SIZE];
			gf_sha1_csum(data, data_size, hash);
			sprintf(nhml, "sha1=\"");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
			for (j=0; j<20; j++) {
				sprintf(nhml, "%02X", hash[j]);
				gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
			}
			sprintf(nhml, "\" ");
			gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		}
	}

	sprintf(nhml, ">\n");
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

	p = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);
	if (p) {
		u32 offset_in_sample = 0;
		Bool first_subs = GF_TRUE;
		if (!ctx->bs_r) ctx->bs_r = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
		else gf_bs_reassign_buffer(ctx->bs_r, p->value.data.ptr, p->value.data.size);

		//(data) binary blob containing N [(u32)flags(u32)size(u32)reserved(u8)priority(u8) discardable]
		while (gf_bs_available(ctx->bs_r)) {
			u32 s_flags = gf_bs_read_u32(ctx->bs_r);
			u32 s_size = gf_bs_read_u32(ctx->bs_r);
			u32 s_res = gf_bs_read_u32(ctx->bs_r);
			u8 s_prio = gf_bs_read_u8(ctx->bs_r);
			u8 s_discard = gf_bs_read_u8(ctx->bs_r);


			if (offset_in_sample + s_size > data_size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("Wrong subsample info: sample size %d vs subsample offset+size %dn", data_size, offset_in_sample + s_size));
				break;
			}

			if (ctx->is_stpp && ctx->nhmlonly) {
				if (first_subs) {
					sprintf(nhml, "<NHNTSubSample>\n");
					gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

					gf_bs_write_data(ctx->bs_w, data, s_size);

					sprintf(nhml, "</NHNTSubSample>\n");
					gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
				} else {
					u32 d_size;
					if (ctx->b64_buffer_size<2*s_size) {
						ctx->b64_buffer_size = 2 * s_size;
						ctx->b64_buffer = gf_realloc(ctx->b64_buffer, ctx->b64_buffer_size);
					}
					d_size = gf_base64_encode(data + offset_in_sample, s_size, ctx->b64_buffer, ctx->b64_buffer_size);
					ctx->b64_buffer[d_size] = 0;
					sprintf(nhml, "<NHNTSubSample data=\"data:application/octet-string;base64,");
					gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
					gf_bs_write_data(ctx->bs_w, ctx->b64_buffer, d_size);
					sprintf(nhml, "\">\n");
					gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
				}
			} else {
				sprintf(nhml, "<NHNTSubSample size=\"%d\" flags=\"%d\" reserved=\"%d\" priority=\"%d\" discard=\"%d\" />\n", s_size, s_flags, s_res, s_prio, s_discard);
				gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
			}
			first_subs = GF_FALSE;
		}
	} else if (ctx->is_stpp && ctx->nhmlonly) {
		sprintf(nhml, "<NHNTSubSample><![CDATA[\n");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
		gf_bs_write_data(ctx->bs_w, data, data_size);
		sprintf(nhml, "]]></NHNTSubSample>\n");
		gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));
	}
	sprintf(nhml, "</NHNTSample>\n");
	gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

	gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->nhml_buffer, &size, &ctx->nhml_buffer_size);

	if (ctx->filep) {
		fwrite(ctx->nhml_buffer, 1, size, ctx->filep);
		return;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid_nhml, size, &output);
	memcpy(output, ctx->nhml_buffer, size);
	gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
	gf_filter_pck_send(dst_pck);

	ctx->mdia_pos += data_size;

	if (ctx->opid_mdia) {
		//send data packet
		dst_pck = gf_filter_pck_new_ref(ctx->opid_mdia, data, data_size, pck);
		gf_filter_pck_merge_properties(pck, dst_pck);
		//keep byte offset ?
//		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		gf_filter_pck_send(dst_pck);
	}
}


GF_Err nhmldump_process(GF_Filter *filter)
{
	GF_NHMLDumpCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	char *data;
	u32 pck_size;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (ctx->bs_w && ctx->szRootName) {
				char nhml[1024];
				u32 size;
				u8 *output;
				GF_FilterPacket *dst_pck;
				gf_bs_reassign_buffer(ctx->bs_w, ctx->nhml_buffer, ctx->nhml_buffer_size);
				sprintf(nhml, "</%s>\n", ctx->szRootName);
				gf_bs_write_data(ctx->bs_w, nhml, (u32) strlen(nhml));

				gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->nhml_buffer, &size, &ctx->nhml_buffer_size);

				if (ctx->filep) {
					fwrite(ctx->nhml_buffer, 1, size, ctx->filep);
				} else {
					dst_pck = gf_filter_pck_new_alloc(ctx->opid_nhml, size, &output);
					memcpy(output, ctx->nhml_buffer, size);
					gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_TRUE);
					gf_filter_pck_send(dst_pck);
				}
				ctx->szRootName = NULL;
			}
			if (ctx->opid_nhml) gf_filter_pid_set_eos(ctx->opid_nhml);
			if (ctx->opid_mdia) gf_filter_pid_set_eos(ctx->opid_mdia);
			if (ctx->opid_info) gf_filter_pid_set_eos(ctx->opid_info);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, ctx->nhml_buffer, ctx->nhml_buffer_size);

	if (ctx->first) {
		nhmldump_send_header(ctx);
		gf_bs_reassign_buffer(ctx->bs_w, ctx->nhml_buffer, ctx->nhml_buffer_size);
	}

	//get media data
	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	//send nhnt data
	if (ctx->is_dims) {
		nhmldump_send_dims(ctx, data, pck_size, pck);
	} else {
		nhmldump_send_frame(ctx, data, pck_size, pck);
	}
	ctx->first = GF_FALSE;


	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_cts(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num*timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static GF_Err nhmldump_initialize(GF_Filter *filter)
{
	GF_NHMLDumpCtx *ctx = gf_filter_get_udta(filter);
	if (!ctx->name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[NHMLMx] output name missing, cannot generate NHML\n"));
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static void nhmldump_finalize(GF_Filter *filter)
{
	GF_NHMLDumpCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->nhml_buffer) gf_free(ctx->nhml_buffer);
	if (ctx->b64_buffer) gf_free(ctx->b64_buffer);
	if (ctx->info_file) gf_free(ctx->info_file);
	if (ctx->media_file) gf_free(ctx->media_file);
}

static const GF_FilterCapability NHMLDumpCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "nhml|dims|dml"),
};

#define OFFS(_n)	#_n, offsetof(GF_NHMLDumpCtx, _n)
static const GF_FilterArgs NHMLDumpArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dims), "use DIMS mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(name), "set output name of files produced (needed media/info files refered to from XML", GF_PROP_STRING, "dump.nhml", NULL, 0},
	{ OFFS(nhmlonly), "only dump NHML info, not media", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(full), "full NHML dump", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(chksum), "insert frame checksum\n"
	"- none: no checksum\n"
	"- crc: CRC32 checksum\n"
	"- sha1: SHA1 checksum"
	"", GF_PROP_UINT, "none", "none|crc|sha1", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(filep), "dump directly to the given FILE pointer (used by MP4Box)", GF_PROP_POINTER, NULL, NULL, GF_FS_ARG_HINT_HIDE},
	{0}
};


GF_FilterRegister NHMLDumpRegister = {
	.name = "nhmlw",
	GF_FS_SET_DESCRIPTION("NHML writer")
	GF_FS_SET_HELP("This filter converts a single stream to an NHML output file.\n"
	"NHML documentation is available at https://github.com/gpac/gpac/wiki/NHML-Format\n")
	.private_size = sizeof(GF_NHMLDumpCtx),
	.args = NHMLDumpArgs,
	.initialize = nhmldump_initialize,
	.finalize = nhmldump_finalize,
	SETCAPS(NHMLDumpCaps),
	.configure_pid = nhmldump_configure_pid,
	.process = nhmldump_process
};

const GF_FilterRegister *nhmldump_register(GF_FilterSession *session)
{
	return &NHMLDumpRegister;
}

