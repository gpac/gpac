/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / NHML demuxer filter
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
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>
#include <gpac/xml.h>
#include <gpac/network.h>
#include <gpac/isomedia.h>
#include <gpac/base_coding.h>

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif

typedef struct
{
	//opts
	Bool reframe;
	Double index;

	GF_FilterPid *ipid;
	GF_FilterPid *opid;

	Bool is_dims;

	Double start_range;
	u64 first_dts;

	Bool is_playing;
	GF_Fraction64 duration;
	Bool in_seek;

	u32 timescale;
	u32 sample_num;

	FILE *mdia;
	char *media_file;

	GF_DOMParser *parser;
	GF_XMLNode *root;
	//0: not initialized, 1: OK, samples can be sent, 2: EOS, 3: error
	u32 parsing_state;
	u32 current_child_idx;
	Bool has_sap;
	u32 compress_type;
	const char *src_url;
	u64 last_dts;
	u32 dts_inc;

	u8 *samp_buffer;
	u32 samp_buffer_alloc, samp_buffer_size;
	char *zlib_buffer;
	u32 zlib_buffer_alloc, zlib_buffer_size;
#ifndef GPAC_DISABLE_ZLIB
	Bool use_dict;
	char *dictionary;
#endif

	u64 media_done;
	Bool is_img;
	u32 header_end;

	GF_BitStream *bs_w;
	GF_BitStream *bs_r;
} GF_NHMLDmxCtx;


GF_Err nhmldmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_NHMLDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
	if (p && p->value.string && strstr(p->value.string, "dims")) ctx->is_dims = GF_TRUE;
	else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
		if (p && p->value.string && strstr(p->value.string, "dims")) ctx->is_dims = GF_TRUE;
	}

	//check multithreaded FileIO restrictions
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (p && p->value.string && gf_fileio_is_main_thread(p->value.string)) {
		gf_filter_force_main_thread(filter, GF_TRUE);
	}
	return GF_OK;
}

static Bool nhmldmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i=0;
	u64 cur_dts = 0;
	u64 byte_offset = 0;
	u32 sample_num = 0;
	GF_XMLNode *node;
	GF_NHMLDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->is_playing && (ctx->start_range ==  evt->play.start_range)) {
			return GF_TRUE;
		}

		ctx->start_range = evt->play.start_range;
		ctx->current_child_idx = 0;
		ctx->media_done = ctx->header_end;
		ctx->is_playing = GF_TRUE;
		//post a seek
		ctx->in_seek = GF_TRUE;

		//lcoate previous RAP sample
		while ((node = (GF_XMLNode *) gf_list_enum(ctx->root->content, &i))) {
			u32 j=0;
			u64 dts=0;
			u32 datalen=0;
			Bool is_rap = ctx->has_sap ? GF_FALSE : GF_TRUE;
			s32 cts_offset=0;
			u64 sample_duration = 0;
			GF_XMLAttribute *att;
			if (node->type) continue;
			if (stricmp(node->name, ctx->is_dims ? "DIMSUnit" : "NHNTSample") ) continue;

			while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
				if (!stricmp(att->name, "DTS") || !stricmp(att->name, "time")) {
					u32 h, m, s, ms;
					u64 dst_val;
					if (strchr(att->value, ':') && sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
						dts = (u64) ( (Double) ( ((h*3600.0 + m*60.0 + s)*1000 + ms) / 1000.0) * ctx->timescale );
					} else if (sscanf(att->value, ""LLU, &dst_val)==1) {
						dts = dst_val;
					}
				}
				else if (!stricmp(att->name, "CTSOffset")) cts_offset = atoi(att->value);
				else if (!stricmp(att->name, "duration") ) sscanf(att->value, ""LLU, &sample_duration);
				else if (!stricmp(att->name, "isRAP") ) {
					is_rap = (!stricmp(att->value, "yes")) ? GF_TRUE : GF_FALSE;
				}
				else if (!stricmp(att->name, "mediaOffset"))
					byte_offset = (s64) atof(att->value) ;
				else if (!stricmp(att->name, "dataLength"))
					datalen = atoi(att->value);
			}

			dts += cts_offset;
			if ((s64) dts < 0) dts = 0;

			if (dts) cur_dts = dts;
			if (sample_duration) cur_dts += sample_duration;
			else if (ctx->dts_inc) cur_dts += ctx->dts_inc;

			if (cur_dts >= ctx->timescale * evt->play.start_range) {
				break;
			}
			if (is_rap) {
				ctx->current_child_idx = i-1;
				ctx->media_done = byte_offset;
				ctx->sample_num = sample_num;
			}
			byte_offset += datalen;
			sample_num++;
		}

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		//don't cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static GF_XMLNode *nhmldmx_get_props(GF_NHMLDmxCtx *ctx, char *ID)
{
	u32 i=0;
	GF_XMLNode *node;
	while ((node = (GF_XMLNode *) gf_list_enum(ctx->root->content, &i))) {
		GF_XMLAttribute *att;
		GF_XMLNode *childnode;
		u32 j;
		if (node->type) continue;
		if (!stricmp(node->name, "Properties")) {
			j=0;
			while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
				if (!strcmp(att->name, "id") && !strcmp(att->value, ID)) return node;
			}
			continue;
		}
		if (stricmp(node->name, "NHNTSample")) continue;
		j=0;
		while ((childnode = (GF_XMLNode *) gf_list_enum(node->content, &j))) {
			u32 k=0;
			if (childnode->type) continue;
			if (stricmp(childnode->name, "Properties")) continue;
			while ( (att = (GF_XMLAttribute *)gf_list_enum(childnode->attributes, &k))) {
				if (!strcmp(att->name, "id") && !strcmp(att->value, ID)) return childnode;
			}
		}
	}
	return NULL;
}
static void nhmldmx_set_props(GF_NHMLDmxCtx *ctx, GF_XMLNode *props, GF_FilterPacket *pck)
{
	GF_XMLNode *childnode;
	u32 i=0;
	GF_XMLAttribute *att;

	i=0;
	while ( (att = (GF_XMLAttribute *)gf_list_enum(props->attributes, &i))) {
		if (strcmp(att->name, "ref")) continue;;
		GF_XMLNode *ref_props = nhmldmx_get_props(ctx, att->value);
		if (ref_props) {
			props = ref_props;
			break;
		}
	}

	i=0;
	while ((childnode = (GF_XMLNode *) gf_list_enum(props->content, &i))) {
		u32 j;
		GF_XMLNode *bs_child;
		Bool has_bs = GF_FALSE;
		if (childnode->type) continue;
		if (stricmp(childnode->name, "P")) continue;

		char *ptype=NULL;
		char *pname=NULL;
		char *pval=NULL;
		j=0;
		while ( (att = (GF_XMLAttribute *)gf_list_enum(childnode->attributes, &j))) {
			if (!strcmp(att->name, "type")) ptype = att->value;
			else if (!strcmp(att->name, "name")) pname = att->value;
			else if (!strcmp(att->name, "value")) pval = att->value;
			else if (!pname) {
				pname = att->name;
				pval = att->value;
			}
		}

		j=0;
		while ( (bs_child = (GF_XMLNode *)gf_list_enum(childnode->content, &j))) {
			if (bs_child->type) continue;
			if (!stricmp(bs_child->name, "BS")) has_bs = GF_TRUE;
		}


		Bool reset=GF_TRUE;
		u32 prop_type = 0;
		u32 p4cc = gf_props_get_id(pname);
		if (p4cc) {
			prop_type = gf_props_4cc_get_type(p4cc);
			pname = NULL;
			if (prop_type!=GF_PROP_DATA)
				has_bs = GF_FALSE;
		} else if (ptype) {
			prop_type = gf_props_parse_type(ptype);
		} else {
			prop_type = GF_PROP_STRING;
		}
		GF_PropertyValue prop_val;
		if (has_bs) {
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			prop_val.type = GF_PROP_DATA_NO_COPY;
			gf_xml_parse_bit_sequence_bs(childnode, ctx->src_url, NULL, bs);
			gf_bs_get_content(bs, &prop_val.value.data.ptr, &prop_val.value.data.size);
			gf_bs_del(bs);
			reset = GF_FALSE;
		} else if (pval) {
			prop_val = gf_props_parse_value(prop_type, pname, pval, NULL, ',');
		} else {
			continue;
		}

		if (p4cc) {
			if (pck)
				gf_filter_pck_set_property(pck, p4cc, &prop_val );
			else
				gf_filter_pid_set_property(ctx->opid, p4cc, &prop_val );
		} else {
			if (pck)
				gf_filter_pck_set_property_dyn(pck, pname, &prop_val );
			else
				gf_filter_pid_set_property_dyn(ctx->opid, pname, &prop_val );
		}
		if (reset)
			gf_props_reset_single(&prop_val);
	}
}

typedef struct
{
	Bool from_is_start, from_is_end, to_is_start, to_is_end;
	u64 from_pos, to_pos;
	char *from_id, *to_id;
	GF_List *id_stack;
	GF_SAXParser *sax;
} XMLBreaker;


static void nhml_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	XMLBreaker *breaker = (XMLBreaker *)sax_cbck;
	char *node_id;
	u32 i;
	node_id = NULL;
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
		if (stricmp(att->name, "DEF") && stricmp(att->name, "id")) continue;
		node_id = gf_strdup(att->value);
		break;
	}
	if (!node_id) {
		node_id = gf_strdup("__nhml__none");
		gf_list_add(breaker->id_stack, node_id);
		return;
	}
	gf_list_add(breaker->id_stack, node_id);

	if (breaker->from_is_start && breaker->from_id && !strcmp(breaker->from_id, node_id)) {
		breaker->from_pos = gf_xml_sax_get_node_start_pos(breaker->sax);
		breaker->from_is_start = GF_FALSE;
	}
	if (breaker->to_is_start && breaker->to_id && !strcmp(breaker->to_id, node_id)) {
		breaker->to_pos = gf_xml_sax_get_node_start_pos(breaker->sax);
		breaker->to_is_start = GF_FALSE;
	}
	if (!breaker->to_is_start && !breaker->from_is_start && !breaker->to_is_end && !breaker->from_is_end) {
		gf_xml_sax_suspend(breaker->sax, GF_TRUE);
	}

}

static void nhml_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	XMLBreaker *breaker = (XMLBreaker *)sax_cbck;
	char *node_id = (char *)gf_list_last(breaker->id_stack);
	gf_list_rem_last(breaker->id_stack);
	if (breaker->from_is_end && breaker->from_id && !strcmp(breaker->from_id, node_id)) {
		breaker->from_pos = gf_xml_sax_get_node_end_pos(breaker->sax);
		breaker->from_is_end = GF_FALSE;
	}
	if (breaker->to_is_end && breaker->to_id && !strcmp(breaker->to_id, node_id)) {
		breaker->to_pos = gf_xml_sax_get_node_end_pos(breaker->sax);
		breaker->to_is_end = GF_FALSE;
	}
	gf_free(node_id);
	if (!breaker->to_is_start && !breaker->from_is_start && !breaker->to_is_end && !breaker->from_is_end) {
		gf_xml_sax_suspend(breaker->sax, GF_TRUE);
	}
}


static GF_Err nhml_sample_from_xml(GF_NHMLDmxCtx *ctx, char *xml_file, char *xmlFrom, char *xmlTo)
{
	GF_Err e = GF_OK;
	u32 read;
	XMLBreaker breaker;
	char *tmp;
	FILE *xml;
	u8 szBOM[3];
	if (!xml_file || !xmlFrom || !xmlTo) return GF_BAD_PARAM;

	memset(&breaker, 0, sizeof(XMLBreaker));

	xml = gf_fopen(xml_file, "rb");
	if (!xml) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] import failure: file %s not found\n", xml_file ));
		goto exit;
	}
	//we cannot use files with BOM since the XML position we get from the parser are offsets in the UTF-8 version of the XML.
	//TODO: to support files with BOM we would need to serialize on the fly the callback from the sax parser
	read = (u32) gf_fread(szBOM, 3, xml);
	if (read==3) {
		gf_fseek(xml, 0, SEEK_SET);
		if ((szBOM[0]==0xFF) || (szBOM[0]==0xFE) || (szBOM[0]==0xEF)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] import failure: XML file %s uses unsupported BOM, please convert to plain UTF-8 or ANSI first\n", xml_file));
			goto exit;
		}
	}


	memset(&breaker, 0, sizeof(XMLBreaker));
	breaker.id_stack = gf_list_new();

	if (strstr(xmlFrom, ".start")) breaker.from_is_start = GF_TRUE;
	else breaker.from_is_end = GF_TRUE;
	tmp = strchr(xmlFrom, '.');
	*tmp = 0;
	if (stricmp(xmlFrom, "doc")) breaker.from_id = gf_strdup(xmlFrom);
	/*doc start pos is 0, no need to look for it*/
	else if (breaker.from_is_start) breaker.from_is_start = GF_FALSE;
	*tmp = '.';

	if (strstr(xmlTo, ".start")) breaker.to_is_start = GF_TRUE;
	else breaker.to_is_end = GF_TRUE;
	tmp = strchr(xmlTo, '.');
	*tmp = 0;
	if (stricmp(xmlTo, "doc")) breaker.to_id = gf_strdup(xmlTo);
	/*doc end pos is file size, no need to look for it*/
	else if (breaker.to_is_end) breaker.to_is_end = GF_FALSE;
	*tmp = '.';

	breaker.sax = gf_xml_sax_new(nhml_node_start, nhml_node_end, NULL, &breaker);
	e = gf_xml_sax_parse_file(breaker.sax, xml_file, NULL);
	gf_xml_sax_del(breaker.sax);
	if (e<0) goto exit;

	if (!breaker.to_id) {
		breaker.to_pos = gf_fsize(xml);
	}
	if (breaker.to_pos < breaker.from_pos) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] import failure: xmlFrom %s is located after xmlTo %s\n", xmlFrom, xmlTo));
		goto exit;
	}

	assert(breaker.to_pos > breaker.from_pos);


	ctx->samp_buffer_size = (u32) (breaker.to_pos - breaker.from_pos);
	if (ctx->samp_buffer_alloc < ctx->samp_buffer_size) {
		ctx->samp_buffer_alloc = ctx->samp_buffer_size;
		ctx->samp_buffer = (char*)gf_realloc(ctx->samp_buffer, sizeof(char)*ctx->samp_buffer_alloc);
	}
	gf_fseek(xml, breaker.from_pos, SEEK_SET);
	if (0 == gf_fread(ctx->samp_buffer, ctx->samp_buffer_size, xml)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] Failed to read samp->dataLength\n"));
	}
	e = GF_OK;
	
exit:
	if (xml) gf_fclose(xml);
	while (gf_list_count(breaker.id_stack)) {
		char *id = (char *)gf_list_last(breaker.id_stack);
		gf_list_rem_last(breaker.id_stack);
		gf_free(id);
	}
	gf_list_del(breaker.id_stack);
	if (breaker.from_id) gf_free(breaker.from_id);
	if (breaker.to_id) gf_free(breaker.to_id);
	return e;
}


#ifndef GPAC_DISABLE_ZLIB

/*since 0.2.2, we use zlib for xmt/x3d reading to handle gz files*/
#include <zlib.h>

#define ZLIB_COMPRESS_SAFE	4

static GF_Err compress_sample_data(GF_NHMLDmxCtx *ctx, u32 compress_type, char **dict, u32 offset)
{
	z_stream stream;
	int err;
	u32 size;

	if (!ctx) return GF_OK;

	size = ctx->samp_buffer_size*ZLIB_COMPRESS_SAFE;
	if (ctx->zlib_buffer_alloc < size) {
		ctx->zlib_buffer_alloc = size;
		ctx->zlib_buffer = gf_realloc(ctx->zlib_buffer, sizeof(char)*size);
	}

	stream.next_in = (Bytef*) ctx->samp_buffer + offset;
	stream.avail_in = (uInt)ctx->samp_buffer_size - offset;
	stream.next_out = ( Bytef*)ctx->zlib_buffer;
	stream.avail_out = (uInt)size;
	stream.zalloc = (alloc_func)NULL;
	stream.zfree = (free_func)NULL;
	stream.opaque = (voidpf)NULL;

	if (compress_type==1) {
		err = deflateInit(&stream, 9);
	} else {
		err = deflateInit2(&stream, 9, Z_DEFLATED, 16+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
	}
	if (err != Z_OK) {
		return GF_IO_ERR;
	}
	if (dict && *dict) {
		err = deflateSetDictionary(&stream, (Bytef *)*dict, (u32) strlen(*dict));
		if (err != Z_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Error assigning dictionary\n"));
			deflateEnd(&stream);
			return GF_IO_ERR;
		}
	}
	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return GF_IO_ERR;
	}
	if (ctx->samp_buffer_size - offset < stream.total_out) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] compressed data (%d) bigger than input data (%d)\n", (u32) stream.total_out, (u32) ctx->samp_buffer_size - offset));
	}
	if (dict) {
		if (*dict) gf_free(*dict);
		*dict = (char*)gf_malloc(sizeof(char) * ctx->samp_buffer_size);
		memcpy(*dict, ctx->samp_buffer, ctx->samp_buffer_size);
	}
	if (ctx->samp_buffer_alloc < stream.total_out) {
		ctx->samp_buffer_alloc = (u32) (stream.total_out*2);
		ctx->samp_buffer = (char*)gf_realloc(ctx->samp_buffer, ctx->samp_buffer_alloc * sizeof(char));
	}

	memcpy(ctx->samp_buffer + offset, ctx->zlib_buffer, sizeof(char)*stream.total_out);
	ctx->samp_buffer_size = (u32) (offset + stream.total_out);

	deflateEnd(&stream);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ZLIB*/

#define NHML_SCAN_INT(_fmt, _value)	\
	{\
	if (strstr(att->value, "0x")) { u32 __i; sscanf(att->value+2, "%x", &__i); _value = __i; }\
	else if (strstr(att->value, "0X")) { u32 __i; sscanf(att->value+2, "%X", &__i); _value = __i; }\
	else sscanf(att->value, _fmt, &_value); \
	}\


static GF_Err nhmldmx_config_output(GF_Filter *filter, GF_NHMLDmxCtx *ctx, GF_XMLNode *root)
{
	GF_Err e;
	Bool inRootOD;
	u32 i, tkID, mtype, streamType, codecid, specInfoSize, par_den, par_num;
	GF_XMLAttribute *att;
	u32 width, height, codec_tag, sample_rate, nb_channels, version, revision, vendor_code, temporal_quality, spatial_quality, h_res, v_res, bit_depth, bits_per_sample;

	u32 dims_profile, dims_level, dims_pathComponents, dims_fullRequestHost, dims_streamType, dims_containsRedundant;
	char *textEncoding, *contentEncoding, *dims_content_script_types, *mime_type, *xml_schema_loc, *xmlns;
	char *auxiliary_mime_types = NULL;
	char*init_name=NULL, szXmlFrom[1000], szXmlHeaderEnd[1000];
	u8 *specInfo;
	char compressor_name[100];
	GF_XMLNode *node;
	FILE *finfo;
	u64 media_size;
	char *szImpName;

	szImpName = ctx->is_dims ? "DIMS" : "NHML";

	ctx->dts_inc = 0;
	inRootOD = GF_FALSE;
	ctx->compress_type = 0;
	specInfo = NULL;

	gf_dynstrcat(&init_name, ctx->media_file, NULL);
	char *ext = gf_file_ext_start(init_name);
	if (ext) ext[0] = 0;
	gf_dynstrcat(&init_name, ".info", NULL);


#ifndef GPAC_DISABLE_ZLIB
	ctx->use_dict = GF_FALSE;
#endif

	tkID = mtype = streamType = codecid = par_den = par_num = 0;
	ctx->timescale = 1000;
	i=0;
	strcpy(szXmlHeaderEnd, "");
	ctx->header_end = 0;

	width = height = codec_tag = sample_rate = nb_channels = version = revision = vendor_code = temporal_quality = spatial_quality = h_res = v_res = bit_depth = bits_per_sample = 0;

	dims_pathComponents = dims_fullRequestHost = 0;
	textEncoding = contentEncoding = dims_content_script_types = mime_type = xml_schema_loc = xmlns = NULL;
	dims_profile = dims_level = 255;
	dims_streamType = GF_TRUE;
	dims_containsRedundant = 1;

	while ((att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		if (!stricmp(att->name, "streamType")) {
			NHML_SCAN_INT("%u", streamType)
			if (!streamType)
				streamType = gf_stream_type_by_name(att->value);
		} else if (!stricmp(att->name, "mediaType") && (strlen(att->value)==4)) {
			mtype = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
		} else if (!stricmp(att->name, "mediaSubType") && (strlen(att->value)==4)) {
			codec_tag = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
		} else if (!stricmp(att->name, "objectTypeIndication")) {
			NHML_SCAN_INT("%u", codecid)
		} else if (!stricmp(att->name, "codecID")) {
			if (strlen(att->value)==4) {
				codecid = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
			} else {
				codecid = gf_codecid_parse(att->value);
			}
		} else if (!stricmp(att->name, "timeScale")) {
			u32 old_timescale = ctx->timescale;
			NHML_SCAN_INT("%u", ctx->timescale)
			if (old_timescale && ctx->timescale && (root != ctx->root)) {
				if (ctx->dts_inc)
					ctx->dts_inc = (u32) gf_timestamp_rescale(ctx->dts_inc, old_timescale, ctx->timescale);
				if (ctx->last_dts != GF_FILTER_NO_TS)
					ctx->last_dts = gf_timestamp_rescale(ctx->last_dts, old_timescale, ctx->timescale);
			}
		} else if (!stricmp(att->name, "width")) {
			NHML_SCAN_INT("%u", width)
		} else if (!stricmp(att->name, "height")) {
			NHML_SCAN_INT("%u", height)
		} else if (!stricmp(att->name, "parNum")) {
			NHML_SCAN_INT("%u", par_num)
		} else if (!stricmp(att->name, "parDen")) {
			NHML_SCAN_INT("%u", par_den)
		} else if (!stricmp(att->name, "sampleRate")) {
			NHML_SCAN_INT("%u", sample_rate)
		} else if (!stricmp(att->name, "numChannels")) {
			NHML_SCAN_INT("%u", nb_channels)
		} else if (!stricmp(att->name, "baseMediaFile")) {
			char *url = gf_url_concatenate(ctx->src_url, att->value);
			if (ctx->media_file) gf_free(ctx->media_file);
			ctx->media_file = url;
		} else if (!stricmp(att->name, "specificInfoFile")) {
			char *url = gf_url_concatenate(ctx->src_url, att->value);
			if (init_name) gf_free(init_name);
			init_name = url;
		} else if (!stricmp(att->name, "headerEnd")) {
			NHML_SCAN_INT("%u", ctx->header_end)
		} else if (!stricmp(att->name, "trackID")) {
			NHML_SCAN_INT("%u", tkID)
		} else if (!stricmp(att->name, "inRootOD")) {
			inRootOD = (!stricmp(att->value, "yes") );
		} else if (!stricmp(att->name, "DTS_increment")) {
			NHML_SCAN_INT("%u", ctx->dts_inc)
		} else if (!stricmp(att->name, "gzipSamples")) {
			if (!stricmp(att->value, "yes") || !stricmp(att->value, "gzip"))
				ctx->compress_type = 2;
			else if (!stricmp(att->value, "deflate"))
				ctx->compress_type = 1;
		} else if (!stricmp(att->name, "auxiliaryMimeTypes")) {
			auxiliary_mime_types = gf_strdup(att->name);
		}
#ifndef GPAC_DISABLE_ZLIB
		else if (!stricmp(att->name, "gzipDictionary")) {
			u32 d_size;
			if (stricmp(att->value, "self")) {
				char *url = gf_url_concatenate(ctx->src_url, att->value);

				e = gf_file_load_data(url ? url : att->value, (u8 **) &ctx->dictionary, &d_size);

				if (url) gf_free(url);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Cannot open dictionary file %s: %s\n", att->value, gf_error_to_string(e) ));
					continue;
				}
			}
			ctx->use_dict = GF_TRUE;
		}
#endif
		/*unknown desc related*/
		else if (!stricmp(att->name, "compressorName")) {
			strncpy(compressor_name, att->value, 99);
			compressor_name[99]=0;
		} else if (!stricmp(att->name, "codecVersion")) {
			NHML_SCAN_INT("%u", version)
		} else if (!stricmp(att->name, "codecRevision")) {
			NHML_SCAN_INT("%u", revision)
		} else if (!stricmp(att->name, "codecVendor") && (strlen(att->value)==4)) {
			vendor_code = GF_4CC(att->value[0], att->value[1], att->value[2], att->value[3]);
		} else if (!stricmp(att->name, "temporalQuality")) {
			NHML_SCAN_INT("%u", temporal_quality)
		} else if (!stricmp(att->name, "spatialQuality")) {
			NHML_SCAN_INT("%u", spatial_quality)
		} else if (!stricmp(att->name, "horizontalResolution")) {
			NHML_SCAN_INT("%u", h_res)
		} else if (!stricmp(att->name, "verticalResolution")) {
			NHML_SCAN_INT("%u", v_res)
		} else if (!stricmp(att->name, "bitDepth")) {
			NHML_SCAN_INT("%u", bit_depth)
		} else if (!stricmp(att->name, "bitsPerSample")) {
			NHML_SCAN_INT("%u", bits_per_sample)
		}
		/*DIMS stuff*/
		else if (!stricmp(att->name, "profile")) {
			NHML_SCAN_INT("%u", dims_profile)
		} else if (!stricmp(att->name, "level")) {
			NHML_SCAN_INT("%u", dims_level)
		} else if (!stricmp(att->name, "pathComponents")) {
			NHML_SCAN_INT("%u", dims_pathComponents)
		} else if (!stricmp(att->name, "useFullRequestHost") && !stricmp(att->value, "yes")) {
			dims_fullRequestHost = GF_TRUE;
		} else if (!stricmp(att->name, "stream_type") && !stricmp(att->value, "secondary")) {
			dims_streamType = GF_FALSE;
		} else if (!stricmp(att->name, "contains_redundant")) {
			if (!stricmp(att->value, "main")) {
				dims_containsRedundant = 1;
			} else if (!stricmp(att->value, "redundant")) {
				dims_containsRedundant = 2;
			} else if (!stricmp(att->value, "main+redundant")) {
				dims_containsRedundant = 3;
			}
		} else if (!stricmp(att->name, "text_encoding") || !stricmp(att->name, "encoding")) {
			textEncoding = att->value;
		} else if (!stricmp(att->name, "content_encoding")) {
			if (!strcmp(att->value, "deflate")) {
				contentEncoding = att->value;
				ctx->compress_type = 1;
			}
			else if (!strcmp(att->value, "gzip")) {
				contentEncoding = att->value;
				ctx->compress_type = 2;
			}
		} else if (!stricmp(att->name, "content_script_types")) {
			dims_content_script_types = att->value;
		} else if (!stricmp(att->name, "mime_type")) {
			mime_type = att->value;
		} else if (!stricmp(att->name, "media_namespace")) {
			xmlns = att->value;
		} else if (!stricmp(att->name, "media_schema_location")) {
			xml_schema_loc = att->value;
		} else if (!stricmp(att->name, "xml_namespace")) {
			xmlns = att->value;
		} else if (!stricmp(att->name, "xml_schema_location")) {
			xml_schema_loc = att->value;
		} else if (!stricmp(att->name, "xmlHeaderEnd")) {
			strcpy(szXmlHeaderEnd, att->value);
		}
	}
	if (sample_rate && !ctx->timescale) {
		ctx->timescale = sample_rate;
	}
	if (!bits_per_sample) {
		bits_per_sample = 16;
	}

	if (ctx->is_dims || (codec_tag==GF_ISOM_SUBTYPE_3GP_DIMS)) {
		mtype = GF_ISOM_MEDIA_DIMS;
		codec_tag=GF_ISOM_SUBTYPE_3GP_DIMS;
		codecid = GF_CODECID_DIMS;
		streamType = GF_STREAM_SCENE;
	}
	if (gf_file_exists_ex(ctx->media_file, ctx->src_url))
		ctx->mdia = gf_fopen_ex(ctx->media_file, ctx->src_url, "rb", GF_FALSE);

	specInfoSize = 0;
	if (!streamType && !mtype && !codec_tag) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] parsing %s file - StreamType or MediaType not specified\n", szImpName));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	finfo = NULL;
	if (gf_file_exists_ex(init_name, ctx->src_url))
		finfo = gf_fopen_ex(init_name, ctx->src_url, "rb", GF_FALSE);

	if (finfo) {
		e = gf_file_load_data_filep(finfo, (u8 **)&specInfo, &specInfoSize);
		gf_fclose(finfo);
		if (e) {
			if (init_name) gf_free(init_name);
			return e;
		}
	} else if (ctx->header_end) {
		/* for text based streams, the decoder specific info can be at the beginning of the file */
		specInfoSize = ctx->header_end;
		specInfo = (char*)gf_malloc(sizeof(char) * (specInfoSize+1));
		specInfoSize = (u32) gf_fread(specInfo, specInfoSize, ctx->mdia);
		specInfo[specInfoSize] = 0;
		ctx->header_end = specInfoSize;
	} else if (strlen(szXmlHeaderEnd)) {
		/* for XML based streams, the decoder specific info can be up to some element in the file */
		strcpy(szXmlFrom, "doc.start");
		ctx->samp_buffer_size = 0;
		e = nhml_sample_from_xml(ctx, ctx->media_file, szXmlFrom, szXmlHeaderEnd);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] failed to load XML header: %s\n", gf_error_to_string(e) ));
			if (init_name) gf_free(init_name);
			return e;
		}

		specInfo = (char*)gf_malloc(sizeof(char) * (ctx->samp_buffer_size +1));
		memcpy(specInfo, ctx->samp_buffer, ctx->samp_buffer_size);
		specInfoSize = ctx->samp_buffer_size;
		specInfo[specInfoSize] = 0;
	}

	i = 0;
	GF_XMLNode *par = (root != ctx->root) ? root : ctx->root;
	while ((node = (GF_XMLNode *) gf_list_enum(par->content, &i))) {
		if (node->type) continue;
		if (!stricmp(node->name, ctx->is_dims ? "DIMSUnit" : "NHNTSample") ) break;
		if (stricmp(node->name, "DecoderSpecificInfo") ) continue;

		e = gf_xml_parse_bit_sequence(node, ctx->src_url, &specInfo, &specInfoSize);
		if (e) {
			if (specInfo) gf_free(specInfo);
			if (init_name) gf_free(init_name);
			return e;
		}
		break;
	}

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(streamType) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(codecid) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	if (ctx->reframe)
	   gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

#ifndef GPAC_DISABLE_AV_PARSERS
	if (!width && !height && specInfo && (codecid==GF_CODECID_MPEG4_PART2)) {
		GF_M4VDecSpecInfo dsi;
		e = gf_m4v_get_config(specInfo, specInfoSize, &dsi);
		if (!e) {
			width = dsi.width;
			height = dsi.height;
			par_num = dsi.par_num;
			par_den = dsi.par_den;
		}
	}
#endif

	if (tkID) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ESID, &PROP_UINT(tkID) );
	if (width) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(width) );
	if (height) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(height) );

	if (par_den) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC_INT(par_num, par_den) );
	switch (bits_per_sample) {
	case 8:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_U8) );
		break;
	case 16:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );
		break;
	case 24:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S24) );
		break;
	case 32:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S32) );
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] Unsupported audio bit depth %d\n", bits_per_sample));
		break;
	}

	if (sample_rate) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sample_rate) );
	if (nb_channels) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_channels) );
	if (bit_depth) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BIT_DEPTH_Y, &PROP_UINT(bit_depth) );

	if (ctx->is_dims) {
		if (dims_profile) gf_filter_pid_set_property_str(ctx->opid, "dims:profile", &PROP_UINT(dims_profile) );
		if (dims_level) gf_filter_pid_set_property_str(ctx->opid, "dims:level", &PROP_UINT(dims_level) );
		if (dims_pathComponents) gf_filter_pid_set_property_str(ctx->opid, "dims:pathComponents", &PROP_UINT(dims_pathComponents) );
		if (dims_fullRequestHost) gf_filter_pid_set_property_str(ctx->opid, "dims:fullRequestHost", &PROP_UINT(dims_fullRequestHost) );
		if (dims_streamType) gf_filter_pid_set_property_str(ctx->opid, "dims:streamType", &PROP_BOOL(dims_streamType) );
		if (dims_containsRedundant) gf_filter_pid_set_property_str(ctx->opid, "dims:redundant", &PROP_UINT(dims_containsRedundant) );
		if (textEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:encoding", &PROP_STRING(textEncoding) );
		if (contentEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:content_encoding", &PROP_STRING(contentEncoding) );
		if (dims_content_script_types) gf_filter_pid_set_property_str(ctx->opid, "dims:scriptTypes", &PROP_STRING(dims_content_script_types) );
		if (mime_type) gf_filter_pid_set_property_str(ctx->opid, "meta:mime", &PROP_STRING(mime_type) );
		if (xml_schema_loc) gf_filter_pid_set_property_str(ctx->opid, "meta:schemaloc", &PROP_STRING(xml_schema_loc) );

	} else if (mtype == GF_ISOM_MEDIA_MPEG_SUBT || mtype == GF_ISOM_MEDIA_SUBT || mtype == GF_ISOM_MEDIA_TEXT) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(mtype) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(codec_tag) );

		if (codec_tag == GF_ISOM_SUBTYPE_STPP) {
			if (xmlns) gf_filter_pid_set_property_str(ctx->opid, "meta:xmlns", &PROP_STRING(xmlns) );
			if (xml_schema_loc) gf_filter_pid_set_property_str(ctx->opid, "meta:schemaloc", &PROP_STRING(xml_schema_loc) );
			if (auxiliary_mime_types) gf_filter_pid_set_property_str(ctx->opid, "meta:aux_mimes", &PROP_STRING(auxiliary_mime_types) );

		} else if (codec_tag == GF_ISOM_SUBTYPE_SBTT) {
			if (mime_type) gf_filter_pid_set_property_str(ctx->opid, "meta:mime", &PROP_STRING(mime_type) );
			if (textEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:encoding", &PROP_STRING(textEncoding) );
		} else if (codec_tag == GF_ISOM_SUBTYPE_STXT) {
			if (mime_type) gf_filter_pid_set_property_str(ctx->opid, "meta:mime", &PROP_STRING(mime_type) );
			if (textEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:encoding", &PROP_STRING(textEncoding) );
			if (contentEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:content_encoding", &PROP_STRING(contentEncoding) );
		} else {
			e = GF_NOT_SUPPORTED;
		}
	} else if (mtype == GF_ISOM_MEDIA_META) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(mtype) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(codec_tag) );

		if (codec_tag == GF_ISOM_SUBTYPE_METX) {
			if (xmlns) gf_filter_pid_set_property_str(ctx->opid, "meta:xmlns", &PROP_STRING(xmlns) );
			if (xml_schema_loc) gf_filter_pid_set_property_str(ctx->opid, "meta:schemaloc", &PROP_STRING(xml_schema_loc) );
			if (textEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:encoding", &PROP_STRING(textEncoding) );
		} else if (codec_tag == GF_ISOM_SUBTYPE_METT) {
			if (mime_type) gf_filter_pid_set_property_str(ctx->opid, "meta:mime", &PROP_STRING(mime_type) );
			if (textEncoding) gf_filter_pid_set_property_str(ctx->opid, "meta:encoding", &PROP_STRING(textEncoding) );
		} else {
			e = GF_NOT_SUPPORTED;
		}
	} else if (!streamType) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(mtype) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(codec_tag) );

		if (version) gf_filter_pid_set_property_str(ctx->opid, "gene:version", &PROP_UINT(version) );
		if (revision) gf_filter_pid_set_property_str(ctx->opid, "gene:revision", &PROP_UINT(revision) );
		if (vendor_code) gf_filter_pid_set_property_str(ctx->opid, "gene:vendor", &PROP_UINT(vendor_code) );
		if (temporal_quality) gf_filter_pid_set_property_str(ctx->opid, "gene:temporal_quality", &PROP_UINT(temporal_quality) );
		if (spatial_quality) gf_filter_pid_set_property_str(ctx->opid, "gene:spatial_quality", &PROP_UINT(spatial_quality) );
		if (h_res) gf_filter_pid_set_property_str(ctx->opid, "gene:horizontal_res", &PROP_UINT(h_res) );
		if (v_res) gf_filter_pid_set_property_str(ctx->opid, "gene:vertical_res", &PROP_UINT(v_res) );
	}


	if (specInfo) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(specInfo, specInfoSize) );
		specInfo = NULL;
		specInfoSize = 0;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILEPATH, & PROP_STRING(ctx->media_file));

	if (ctx->mdia) {
		media_size = gf_fsize(ctx->mdia);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, & PROP_LONGUINT(media_size) );
	}

	if (specInfo) gf_free(specInfo);
	if (auxiliary_mime_types) gf_free(auxiliary_mime_types);
	if (init_name) gf_free(init_name);

	if (inRootOD) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE) );


	i = 0;
	par = (root != ctx->root) ? root : ctx->root;
	while ((node = (GF_XMLNode *) gf_list_enum(par->content, &i))) {
		if (node->type) continue;
		if (!stricmp(node->name, ctx->is_dims ? "DIMSUnit" : "NHNTSample") ) break;

		if (stricmp(node->name, "Properties")) continue;

		nhmldmx_set_props(ctx, node, NULL);
		break;
	}

	return GF_OK;
}

static GF_Err nhmldmx_init_parsing(GF_Filter *filter, GF_NHMLDmxCtx *ctx)
{
	GF_Err e;
	u32 i;
	GF_XMLAttribute *att;
	FILE *nhml;
	const GF_PropertyValue *p;
	char *ext;
	GF_XMLNode *node;
	u64 last_dts;
	char *szRootName, *szSampleName, *szImpName;

	szRootName = ctx->is_dims ? "DIMSStream" : "NHNTStream";
	szSampleName = ctx->is_dims ? "DIMSUnit" : "NHNTSample";
	szImpName = ctx->is_dims ? "DIMS" : "NHML";

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}
	ctx->src_url = p->value.string;
	nhml = gf_fopen(ctx->src_url, "rt");
	if (!nhml) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Cannot find %s file %s\n", szImpName, ctx->src_url));
		return GF_URL_ERROR;
	}

	if (ctx->media_file) gf_free(ctx->media_file);
	ctx->media_file = NULL;

	if (!strncmp(ctx->src_url, "gfio://", 7)) {
		char *base = gf_file_basename( gf_fileio_translate_url(ctx->src_url) );
		if (base) gf_dynstrcat(&ctx->media_file, base, NULL);
	} else {
		gf_dynstrcat(&ctx->media_file, ctx->src_url, NULL);
	}
	if (!ctx->media_file) return GF_OUT_OF_MEM;
	ext = gf_file_ext_start(ctx->media_file);
	if (ext) ext[0] = 0;
	gf_dynstrcat(&ctx->media_file, ".media", NULL);

	ctx->parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(ctx->parser, p->value.string, NULL, NULL);
	if (e) {
		gf_fclose(nhml);
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Error parsing %s file: Line %d - %s\n", szImpName, gf_xml_dom_get_line(ctx->parser), gf_xml_dom_get_error(ctx->parser) ));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	gf_fclose(nhml);

	ctx->root = gf_xml_dom_get_root(ctx->parser);
	if (!ctx->root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Error parsing %s file - no root node found\n", szImpName ));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (stricmp(ctx->root->name, szRootName)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Error parsing %s file - \"%s\" root expected, got \"%s\"", szImpName, szRootName, ctx->root->name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	nhmldmx_config_output(filter, ctx, ctx->root);

	ctx->media_done = 0;
	ctx->current_child_idx = 0;
	ctx->last_dts = GF_FILTER_NO_TS;

	//compute duration
	ctx->duration.den = ctx->timescale;
	ctx->duration.num = 0;
	last_dts = 0;
	i=0;
	ctx->has_sap = GF_FALSE;
	while ((node = (GF_XMLNode *) gf_list_enum(ctx->root->content, &i))) {
		u32 j=0;
		u64 dts=0;
		s32 cts_offset=0;
		u64 sample_duration = 0;
		if (node->type) continue;
		if (!stricmp(node->name, szSampleName) ) {
		} else if (!stricmp(node->name, "NHNTReconfig") ) {
			break;
		} else {
			continue;
		}

		while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!stricmp(att->name, "DTS") || !stricmp(att->name, "time")) {
				u32 h, m, s, ms;
				u64 dst_val;
				if (strchr(att->value, ':') && sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					dts = (u64) ( (Double) ( ((h*3600.0 + m*60.0 + s)*1000 + ms) / 1000.0) * ctx->timescale );
				} else if (sscanf(att->value, ""LLU, &dst_val)==1) {
					dts = dst_val;
				}
			}
			else if (!stricmp(att->name, "CTSOffset")) cts_offset = atoi(att->value);
			else if (!stricmp(att->name, "duration") ) sscanf(att->value, ""LLU, &sample_duration);
			else if (!stricmp(att->name, "isRAP") ) ctx->has_sap = GF_TRUE;
		}

		last_dts = ctx->duration.num;
		if (dts) ctx->duration.num = (u32) (dts + cts_offset);
		if (sample_duration) {
			last_dts = 0;
			ctx->duration.num += (u32) sample_duration;
		} else if (ctx->dts_inc) {
			last_dts = 0;
			ctx->duration.num += ctx->dts_inc;
		}
	}
	if (last_dts) {
		ctx->duration.num += (u32) (ctx->duration.num - last_dts);
	}

	p = gf_filter_pid_get_property(ctx->opid, GF_PROP_PID_STREAM_TYPE);
	//assume image, one sec (default for old arch)
	if (p && (p->value.uint==4) && !ctx->duration.num) {
		ctx->is_img = GF_TRUE;
		ctx->duration.num =ctx->duration.den;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD ) );
	return e;
}



void nhml_get_bs(GF_BitStream **bs, char *data, u32 size, u32 mode)
{
	if (*bs) gf_bs_reassign_buffer(*bs, data, size);
	else  (*bs) = gf_bs_new(data, size, mode);
}

static void nhmldmx_set_subs(GF_NHMLDmxCtx *ctx, GF_XMLNode *node, GF_FilterPacket *pck)
{
	GF_XMLNode *childnode, *subs;
	u32 i=0;
	while ((childnode = (GF_XMLNode *) gf_list_enum(node->content, &i))) {
		u32 j;
		GF_XMLAttribute *att;
		GF_BitStream *bs;

		if (childnode->type) continue;
		if (stricmp(childnode->name, "SubSamples")) continue;

		u32 flags=0;
		j=0;
		while ( (att = (GF_XMLAttribute *)gf_list_enum(childnode->attributes, &j))) {
			if (!strcmp(att->name, "flags")) flags = atoi(att->value);
		}

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		j=0;
		while ((subs = (GF_XMLNode *) gf_list_enum(childnode->content, &j))) {
			if (subs->type) continue;
			if (stricmp(subs->name, "SubSample")) continue;

			Bool discardable=0;
			u32 priority=0;
			u32 size=0;
			u32 codec_info=0;
			char *fsize = NULL;
			Bool ftext=GF_FALSE;

			u32 k = 0;
			while ((att = (GF_XMLAttribute *)gf_list_enum(subs->attributes, &k))) {
				if (!stricmp(att->name, "size")) size = atoi(att->value);
				else if (!stricmp(att->name, "fsize")) fsize = att->value;
				else if (!stricmp(att->name, "textmode")) {
					if (!stricmp(att->value, "yes") || !stricmp(att->value, "true") || !stricmp(att->value, "1")) {
						ftext = GF_TRUE;
					}
				}
				else if (!stricmp(att->name, "discardable")) {
					if (!stricmp(att->value, "yes") || !stricmp(att->value, "true") || !stricmp(att->value, "1")) {
						discardable = GF_TRUE;
					}
				}
				else if (!stricmp(att->name, "priority")) priority = atoi(att->value);
				else if (!stricmp(att->name, "codec_info")) {
					if (!strnicmp(att->value, "0x", 2)) sscanf(att->value+2, "%x", &codec_info);
					else sscanf(att->value, "%u", &codec_info);
				}
			}

			if (fsize) {
				char *sub_file_url = gf_url_concatenate(ctx->src_url, fsize);
				if (sub_file_url) {
					FILE *f = gf_fopen(sub_file_url, ftext ? "rt" : "r");
					if (f) {
						size += (u32) gf_fsize(f);
						gf_fclose(f);
					}
					gf_free(sub_file_url);
				}
			}

			gf_bs_write_u32(bs, flags); //flags
			gf_bs_write_u32(bs, size);
			gf_bs_write_u32(bs, codec_info); //codec_specific_parameters
			gf_bs_write_u8(bs, priority); //priority
			gf_bs_write_u8(bs, discardable); //discardable
		}

		u8 *subs_data;
		u32 subs_size;
		gf_bs_get_content(bs, &subs_data, &subs_size);
		gf_bs_del(bs);
		gf_filter_pck_set_property(pck, GF_PROP_PCK_SUBS, &PROP_DATA_NO_COPY(subs_data, subs_size) );
		break;
	}
}

static GF_XMLNode *nhmldmx_find_sai(GF_NHMLDmxCtx *ctx, char *id)
{
	GF_XMLNode *node;
	u32 idx=0;
	while ((node = (GF_XMLNode *) gf_list_enum(ctx->root->content, &idx))) {
		GF_XMLNode *child;
		u32 j=0;
		if (node->type) continue;
		if (stricmp(node->name, ctx->is_dims ? "DIMSUnit" : "NHNTSample") ) continue;

		while ((child = (GF_XMLNode *) gf_list_enum(node->content, &j))) {
			u32 k=0;
			GF_XMLAttribute *att;
			if (child->type) continue;
			if (stricmp(child->name, "SAI") ) continue;
			while ((att = (GF_XMLAttribute *) gf_list_enum(child->attributes, &k))) {
				if (!strcmp(att->name, "id") && !strcmp(att->value, id))
					return child;
			}
		}
	}
	return NULL;
}

static void nhmldmx_set_sai(GF_NHMLDmxCtx *ctx, GF_XMLNode *node, GF_FilterPacket *pck)
{
	GF_XMLNode *childnode;
	u32 i=0;
	while ((childnode = (GF_XMLNode *) gf_list_enum(node->content, &i))) {
		u32 j;
		char szPName[30];
		GF_XMLAttribute *att;
		GF_BitStream *bs;

		if (childnode->type) continue;
		if (stricmp(childnode->name, "SAI")) continue;

		Bool is_ref=GF_FALSE;
		u32 type, aux_info;
		Bool is_sample_group;
		Bool valid = GF_TRUE;

retry_sai:
		type=0;
		aux_info=0;
		is_sample_group = GF_FALSE;
		j=0;
		while ( (att = (GF_XMLAttribute *)gf_list_enum(childnode->attributes, &j))) {
			if (!strcmp(att->name, "type")) type = gf_4cc_parse(att->value);
			else if (!strcmp(att->name, "ref")) {
				if (!is_ref) {
					GF_XMLNode *ref_sai = nhmldmx_find_sai(ctx, att->value);
					if (ref_sai) {
						is_ref = GF_TRUE;
						childnode = ref_sai;
						goto retry_sai;
					} else {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Cannot locate SAI with id %s, ignoring SAI\n", att->value));
						valid=GF_FALSE;
					}
				}
			}
			else if (!strcmp(att->name, "aux_info")) aux_info = atoi(att->value);
			else if (!strcmp(att->name, "group")) {
				if (!strcmp(att->value, "yes") || !strcmp(att->value, "true") || !strcmp(att->value, "1")) is_sample_group = GF_TRUE;
			}
		}
		if (!valid) continue;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_xml_parse_bit_sequence_bs(childnode, ctx->src_url, NULL, bs);

		u8 *sai_data;
		u32 sai_size;
		gf_bs_get_content(bs, &sai_data, &sai_size);
		gf_bs_del(bs);

		char *prefix = is_sample_group ? "grp" : "sai";
		if (aux_info) {
			sprintf(szPName, "%s_%s_%d", prefix, gf_4cc_to_str(type), aux_info);
		} else {
			sprintf(szPName, "%s_%s", prefix, gf_4cc_to_str(type));
		}

		gf_filter_pck_set_property_dyn(pck, szPName, &PROP_DATA_NO_COPY(sai_data, sai_size) );
	}
}


static GF_Err nhmldmx_send_sample(GF_Filter *filter, GF_NHMLDmxCtx *ctx)
{
	GF_XMLNode *node, *childnode;
	u64 sample_duration = 0;
	char szMediaTemp[GF_MAX_PATH], szXmlFrom[1000], szXmlTo[1000];

	while ((node = (GF_XMLNode *) gf_list_enum(ctx->root->content, &ctx->current_child_idx))) {
		u8 *data;
		GF_FilterPacket *pck;
		u32 j, dims_flags;
		GF_FilterSAPType sap_type;
		GF_XMLAttribute *att;
		u64 dts=0;
		GF_Err e=GF_OK;
		s32 cts_offset;
		u64 offset=0, byte_offset = GF_FILTER_NO_BO;
		Bool redundant_rap, append, has_subbs;
		u32 compress_type;
		char *base_media_file = NULL;
		char *base_data = NULL;
		if (node->type) continue;
		if (stricmp(node->name, ctx->is_dims ? "DIMSUnit" : "NHNTSample") ) {
			if (!strcmp(node->name, "NHNTReconfig")) {
				nhmldmx_config_output(filter, ctx, node);
			}
			continue;
		}

		strcpy(szMediaTemp, "");
		strcpy(szXmlFrom, "");
		strcpy(szXmlTo, "");

		/*by default handle all samples as contiguous*/
		ctx->samp_buffer_size = 0;
		dims_flags = 0;
		append = GF_FALSE;
		compress_type = ctx->compress_type;
		sample_duration = 0;
		redundant_rap = 0;
		sap_type = ctx->has_sap ? GF_FILTER_SAP_NONE : GF_FILTER_SAP_1;

		cts_offset = 0;
		if (ctx->last_dts != GF_FILTER_NO_TS)
			dts = ctx->last_dts;
		else
			dts = 0;

		ctx->sample_num++;


		j=0;
		while ( (att = (GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!stricmp(att->name, "DTS") || !stricmp(att->name, "time")) {
				u32 h, m, s, ms;
				u64 dst_val;
				if (strchr(att->value, ':') && sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					dts = (u64) ( (Double) ( ((h*3600.0 + m*60.0 + s)*1000 + ms) / 1000.0) * ctx->timescale );
				} else if (sscanf(att->value, ""LLU, &dst_val)==1) {
					dts = dst_val;
				}
			}
			else if (!stricmp(att->name, "CTSOffset")) cts_offset = atoi(att->value);
			else if (!stricmp(att->name, "isRAP") ) {
				if ((att->value[0]>='0') && (att->value[0]<='9'))
					sap_type = atoi(att->value);
				else
					sap_type = (!stricmp(att->value, "yes")) ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE;
			}
			else if (!stricmp(att->name, "isSyncShadow")) redundant_rap = !stricmp(att->value, "yes") ? 1 : 0;
			else if (!stricmp(att->name, "SAPType") ) sap_type = atoi(att->value);
			else if (!stricmp(att->name, "mediaOffset")) offset = (s64) atof(att->value) ;
			else if (!stricmp(att->name, "dataLength")) ctx->samp_buffer_size = atoi(att->value);
			else if (!stricmp(att->name, "mediaFile")) {
				if (!strncmp(att->value, "data:", 5)) {
					char *base = strstr(att->value, "base64,");
					if (!base) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] Data encoding scheme not recognized in sample %d - skipping\n", ctx->sample_num));
					} else {
						base_data = att->value;
					}
				} else if (!strnicmp(att->value, "gmem://", 7)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] Invalid url %s for NHML import\n", att->value));
				} else {
					base_media_file = att->value;
					char *url = gf_url_concatenate(ctx->src_url, att->value);
					if (!url) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] Failed to get full url for %s\n", att->value));
					} else {
						strncpy(szMediaTemp, url, GF_MAX_PATH-1);
						szMediaTemp[GF_MAX_PATH-1] = 0;
						gf_free(url);
					}
				}
			}
			else if (!stricmp(att->name, "xmlFrom")) {
				strncpy(szXmlFrom, att->value, 999);
				szXmlFrom[999]=0;
			}
			else if (!stricmp(att->name, "xmlTo")) {
				strncpy(szXmlTo, att->value, 999);
				szXmlTo[999]=0;
			}
			/*DIMS flags*/
			else if (!stricmp(att->name, "is-Scene") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_S;
			else if (!stricmp(att->name, "is-RAP") && !stricmp(att->value, "yes")) {
				dims_flags |= GF_DIMS_UNIT_M;
				sap_type = GF_FILTER_SAP_1;
			}
			else if (!stricmp(att->name, "is-redundant") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_I;
			else if (!stricmp(att->name, "redundant-exit") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_D;
			else if (!stricmp(att->name, "priority") && !stricmp(att->value, "high"))
				dims_flags |= GF_DIMS_UNIT_P;
			else if (!stricmp(att->name, "compress") && !stricmp(att->value, "yes"))
				dims_flags |= GF_DIMS_UNIT_C;
			else if (!stricmp(att->name, "duration") )
				sscanf(att->value, ""LLU, &sample_duration);
		}
		if (sap_type==GF_FILTER_SAP_1)
			dims_flags |= GF_DIMS_UNIT_M;

		if (ctx->is_dims)
			compress_type = (dims_flags & GF_DIMS_UNIT_C) ? 2 : 0 ;

		if (ctx->is_img) sample_duration = ctx->duration.den;

		GF_XMLNode *sample_child=NULL;
		Bool has_subs_child = GF_FALSE;
		Bool has_sai_child = GF_FALSE;
		has_subbs = GF_FALSE;
		GF_XMLNode *props = NULL;
		j=0;
		while ((childnode = (GF_XMLNode *) gf_list_enum(node->content, &j))) {
			if (childnode->type) continue;
			if (!stricmp(childnode->name, "Media")) {
				sample_child = childnode;
				has_subbs = GF_TRUE;
			}
			if (!stricmp(childnode->name, "SubSamples")) {
				has_subs_child = GF_TRUE;
			}
			if (!stricmp(childnode->name, "SAI")) {
				has_sai_child = GF_TRUE;
			}
			if (!stricmp(childnode->name, "BS")) {
				has_subbs = GF_TRUE;
			}
			if (!stricmp(childnode->name, "Properties")) {
				props = childnode;
			}
		}

		if (strlen(szXmlFrom) && strlen(szXmlTo)) {
			char *xml_file;
			if (strlen(szMediaTemp)) xml_file = szMediaTemp;
			else xml_file = ctx->media_file;
			ctx->samp_buffer_size = 0;
			e = nhml_sample_from_xml(ctx, xml_file, szXmlFrom, szXmlTo);
		} else if (ctx->is_dims && !strlen(szMediaTemp)) {

			char *content = gf_xml_dom_serialize(node, GF_TRUE, GF_FALSE);

			ctx->samp_buffer_size = 3 + (u32) strlen(content);
			if (ctx->samp_buffer_alloc < ctx->samp_buffer_size) {
				ctx->samp_buffer_alloc = ctx->samp_buffer_size;
				ctx->samp_buffer = gf_realloc(ctx->samp_buffer, ctx->samp_buffer_size);
			}
			nhml_get_bs(&ctx->bs_w, ctx->samp_buffer, ctx->samp_buffer_size, GF_BITSTREAM_WRITE);
			gf_bs_write_u16(ctx->bs_w, ctx->samp_buffer_size - 2);
			gf_bs_write_u8(ctx->bs_w, (u8) dims_flags);
			gf_bs_write_data(ctx->bs_w, content, (ctx->samp_buffer_size - 3));
			gf_free(content);

			/*same DIMS unit*/
			if (ctx->last_dts == dts)
				append = GF_TRUE;
		} else if (base_data) {
			char *start = strchr(base_data, ',');
			if (start) {
				u32 len = (u32)strlen(start+1);
				if (len > ctx->samp_buffer_alloc) {
					ctx->samp_buffer_alloc = len;
					ctx->samp_buffer = gf_realloc(ctx->samp_buffer, sizeof(char)*ctx->samp_buffer_alloc);
				}
				ctx->samp_buffer_size = gf_base64_decode(start, len, ctx->samp_buffer, ctx->samp_buffer_alloc);
			}
		} else {
			Bool close = GF_FALSE;
			FILE *f = ctx->mdia;

			if (strlen(szMediaTemp)) {
				f = gf_fopen(szMediaTemp, "rb");
				if (!f) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] import failure in sample %d: file %s not found\n", ctx->sample_num, close ? szMediaTemp : ctx->media_file));
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				close = GF_TRUE;
				if (offset) gf_fseek(f, offset, SEEK_SET);
				//when using dedicated source files per samples, we don't allow for data reference yet
			} else {
				if (!offset) offset = ctx->media_done;
				byte_offset = offset;
			}

			//use full file only if no subbs
			if (!ctx->samp_buffer_size && !has_subbs && f) {
				u64 ssize = gf_fsize(f);
				assert(ssize < 0x80000000);
				ctx->samp_buffer_size = (u32) ssize;
			}

			if (f && ctx->samp_buffer_size) {
				gf_fseek(f, offset, SEEK_SET);

				if (ctx->is_dims) {
					u32 read;
					if (ctx->samp_buffer_size + 3 > ctx->samp_buffer_alloc) {
						ctx->samp_buffer_alloc = ctx->samp_buffer_size + 3;
						ctx->samp_buffer = (char*)gf_realloc(ctx->samp_buffer, sizeof(char) * ctx->samp_buffer_alloc);
					}
					nhml_get_bs(&ctx->bs_w, ctx->samp_buffer, ctx->samp_buffer_alloc, GF_BITSTREAM_WRITE);
					gf_bs_write_u16(ctx->bs_w, ctx->samp_buffer_size+1);
					gf_bs_write_u8(ctx->bs_w, (u8) dims_flags);
					read = (u32) gf_fread( ctx->samp_buffer + 3, ctx->samp_buffer_size, f);
					if (ctx->samp_buffer_size != read) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Failed to fully read sample %d: dataLength %d read %d\n", ctx->sample_num, ctx->samp_buffer_size, read));
					}
					ctx->samp_buffer_size += 3;

					/*same DIMS unit*/
					if (ctx->last_dts == dts)
						append = GF_TRUE;
				} else {
					u32 read;
					if (ctx->samp_buffer_alloc < ctx->samp_buffer_size) {
						ctx->samp_buffer_alloc = ctx->samp_buffer_size;
						ctx->samp_buffer = (char*)gf_realloc(ctx->samp_buffer, sizeof(char) * ctx->samp_buffer_alloc);
					}
					read = (u32) gf_fread(ctx->samp_buffer, ctx->samp_buffer_size, f);
					if (ctx->samp_buffer_size != read) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Failed to fully read sample %d: dataLength %d read %d\n", ctx->sample_num, ctx->samp_buffer_size, read));
					}
				}
			} else {
				e = GF_OK;
			}
			if (f && close) gf_fclose(f);
		}

		if (e) return e;

		//child BS present, parse them
		if (has_subbs) {
			u8 *output;
			//we must create a tmp bitstream, we don't know how much we will write in the XML bit sequence
			GF_BitStream *bs_tmp = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (ctx->samp_buffer_size) {
				gf_bs_write_data(bs_tmp, ctx->samp_buffer, ctx->samp_buffer_size);
			}
			if (!base_media_file) base_media_file = ctx->media_file;
			gf_xml_parse_bit_sequence_bs(sample_child ? sample_child : node, ctx->src_url, base_media_file, bs_tmp);
			gf_bs_get_content(bs_tmp, &output, &ctx->samp_buffer_size);
			gf_bs_del(bs_tmp);

			if (ctx->samp_buffer_size > ctx->samp_buffer_alloc) {
				ctx->samp_buffer_alloc = ctx->samp_buffer_size;
				ctx->samp_buffer = gf_realloc(ctx->samp_buffer, ctx->samp_buffer_size);;
			}
			memcpy(ctx->samp_buffer, output, ctx->samp_buffer_size);
			gf_free(output);
		}
		if (!ctx->samp_buffer_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[NHMLDmx] No media file associated with sample %d!\n", ctx->sample_num));
		}

		//override DIMS flags
		if (ctx->is_dims) {
			if (strstr(ctx->samp_buffer + 3, "svg ")) dims_flags |= GF_DIMS_UNIT_S;
			if (dims_flags & GF_DIMS_UNIT_S) dims_flags |= GF_DIMS_UNIT_P;
			ctx->samp_buffer[2] = dims_flags;
		}

		if (compress_type) {
#ifndef GPAC_DISABLE_ZLIB
			e = compress_sample_data(ctx, compress_type, ctx->use_dict ? &ctx->dictionary : NULL, ctx->is_dims ? 3 : 0);
			if (e) return e;

			if (ctx->is_dims) {
				nhml_get_bs(&ctx->bs_w, ctx->samp_buffer, ctx->samp_buffer_size, GF_BITSTREAM_WRITE);
				gf_bs_write_u16(ctx->bs_w, ctx->samp_buffer_size-2);
			}
#else
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] Error: your version of GPAC was compiled with no libz support, aborting\n"));
			return GF_NOT_SUPPORTED;
#endif
		}

		if (ctx->is_dims && (ctx->samp_buffer_size > 0xFFFF)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[NHMLDmx] DIMS import failure: sample %d data is too long - maximum size allowed: 65532 bytes\n", ctx->sample_num));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (ctx->samp_buffer_size) {
			pck = gf_filter_pck_new_alloc(ctx->opid, ctx->samp_buffer_size, &data);
			if (!pck) return GF_OUT_OF_MEM;

			memcpy(data, ctx->samp_buffer, ctx->samp_buffer_size);
			gf_filter_pck_set_framing(pck, append ? GF_FALSE : GF_TRUE, GF_FALSE);
			if (!append) {
				gf_filter_pck_set_sap(pck, sap_type);
				gf_filter_pck_set_dts(pck, dts);
				gf_filter_pck_set_cts(pck, dts+cts_offset);
				if (redundant_rap && !ctx->is_dims) gf_filter_pck_set_dependency_flags(pck, 0x1);

				if (sample_duration || ctx->dts_inc)
					gf_filter_pck_set_duration(pck, sample_duration ? (u32) sample_duration : ctx->dts_inc);

				if (byte_offset != GF_FILTER_NO_BO)
					gf_filter_pck_set_byte_offset(pck, byte_offset);

				if (ctx->in_seek) {
					if (dts+cts_offset >= ctx->start_range * ctx->timescale)
						ctx->in_seek = GF_FALSE;
					else
						gf_filter_pck_set_seek_flag(pck, GF_TRUE);
				}
			}
			if (has_subs_child) {
				nhmldmx_set_subs(ctx, node, pck);
			}
			if (has_sai_child) {
				nhmldmx_set_sai(ctx, node, pck);
			}
			if (props) {
				nhmldmx_set_props(ctx, props, pck);
			}
			gf_filter_pck_send(pck);
		}

		ctx->last_dts = dts;

		if (sample_duration)
			ctx->last_dts += sample_duration;
		else
			ctx->last_dts += ctx->dts_inc;
		ctx->media_done += ctx->samp_buffer_size;

		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
	}
	ctx->parsing_state = 2;
	return GF_OK;
}

GF_Err nhmldmx_process(GF_Filter *filter)
{
	GF_NHMLDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_Err e;
	Bool start, end;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (pck) {
		gf_filter_pck_get_framing(pck, &start, &end);
		//for now we only work with complete files
		assert(end);
	}


	//need init ?
	switch (ctx->parsing_state) {
	case 0:
		e = nhmldmx_init_parsing(filter, ctx);
		if (e) {
			ctx->parsing_state = 3;
			return e;
		}
		ctx->parsing_state = 1;
		//fall-through
	case 1:
		if (!ctx->is_playing) return GF_OK;

		e = nhmldmx_send_sample(filter, ctx);
		if (e) return e;
		break;
	case 2:
	default:
		if (pck) gf_filter_pid_drop_packet(ctx->ipid);
		if (ctx->opid) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		break;
	}
	return GF_OK;
}

GF_Err nhmldmx_initialize(GF_Filter *filter)
{
//	GF_NHMLDmxCtx *ctx = gf_filter_get_udta(filter);
	return GF_OK;
}

void nhmldmx_finalize(GF_Filter *filter)
{
	GF_NHMLDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->mdia) gf_fclose(ctx->mdia);
	if (ctx->parser)
		gf_xml_dom_del(ctx->parser);

#ifndef GPAC_DISABLE_ZLIB
	if (ctx->dictionary) gf_free(ctx->dictionary);
#endif
	if (ctx->bs_r) gf_bs_del(ctx->bs_r);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->samp_buffer) gf_free(ctx->samp_buffer);
	if (ctx->zlib_buffer) gf_free(ctx->zlib_buffer);
	if (ctx->media_file) gf_free(ctx->media_file);
}


#define OFFS(_n)	#_n, offsetof(GF_NHMLDmxCtx, _n)
static const GF_FilterArgs GF_NHMLDmxArgs[] =
{
	{ OFFS(reframe), "force re-parsing of referenced content", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


static const GF_FilterCapability NHMLDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "nhml|dims|dml"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-nhml|application/dims"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE)
};

GF_FilterRegister NHMLDmxRegister = {
	.name = "nhmlr",
	GF_FS_SET_DESCRIPTION("NHML reader")
	GF_FS_SET_HELP("This filter reads NHML files/data to produce a media PID and frames.\n"
	"NHML documentation is available at https://wiki.gpac.io/NHML-Format\n")
	.private_size = sizeof(GF_NHMLDmxCtx),
	.args = GF_NHMLDmxArgs,
	.initialize = nhmldmx_initialize,
	.finalize = nhmldmx_finalize,
	SETCAPS(NHMLDmxCaps),
	.configure_pid = nhmldmx_configure_pid,
	.process = nhmldmx_process,
	.process_event = nhmldmx_process_event
};

const GF_FilterRegister *nhmldmx_register(GF_FilterSession *session)
{
	return &NHMLDmxRegister;
}

