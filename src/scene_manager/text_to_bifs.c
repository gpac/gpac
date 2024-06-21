/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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



#include <gpac/constants.h>
#include <gpac/utf.h>
#include <gpac/xml.h>
#include <gpac/internal/media_dev.h>
#include <gpac/scene_manager.h>

enum
{
	GF_TEXT_IMPORT_NONE = 0,
	GF_TEXT_IMPORT_SRT,
	GF_TEXT_IMPORT_SUB,
	GF_TEXT_IMPORT_TTXT,
	GF_TEXT_IMPORT_TEXML,
};

#define REM_TRAIL_MARKS(__str, __sep) while (1) {	\
		u32 _len = (u32) strlen(__str);		\
		if (!_len) break;	\
		_len--;				\
		if (strchr(__sep, __str[_len])) __str[_len] = 0;	\
		else break;	\
	}	\
 

static GF_Err gf_text_guess_format(char *filename, u32 *fmt)
{
	char szLine[2048], szTest[10];
	u32 val;
	FILE *test = gf_fopen(filename, "rt");
	if (!test) return GF_URL_ERROR;

	while (gf_fgets(szLine, 2048, test) != NULL) {
		REM_TRAIL_MARKS(szLine, "\r\n\t ")

		if (strlen(szLine)) break;
	}
	*fmt = GF_TEXT_IMPORT_NONE;
	if ((szLine[0]=='{') && strstr(szLine, "}{")) *fmt = GF_TEXT_IMPORT_SUB;
	else if (sscanf(szLine, "%u", &val)==1) {
		sprintf(szTest, "%u", val);
		if (!strcmp(szTest, szLine)) *fmt = GF_TEXT_IMPORT_SRT;
	}
	else if (!strnicmp(szLine, "<?xml ", 6)) {
		char *ext = gf_file_ext_start(filename);
		if (!strnicmp(ext, ".ttxt", 5)) *fmt = GF_TEXT_IMPORT_TTXT;
		ext = strstr(szLine, "?>");
		if (ext) ext += 2;
		if (ext && !ext[0]) {
			if (!gf_fgets(szLine, 2048, test))
				szLine[0] = '\0';
		}
		if (strstr(szLine, "x-quicktime-tx3g")) *fmt = GF_TEXT_IMPORT_TEXML;
	}
	gf_fclose(test);
	return GF_OK;
}

#ifndef GPAC_DISABLE_VRML

static GF_Err gf_text_import_srt_bifs(GF_SceneManager *ctx, GF_ESD *src, GF_MuxInfo *mux)
{
	GF_Err e;
	GF_Node *text, *font;
	GF_StreamContext *srt;
	FILE *srt_in;
	GF_FieldInfo string, style;
	u32 sh, sm, ss, sms, eh, em, es, ems, start, end;
	GF_AUContext *au;
	GF_Command *com;
	SFString *sfstr;
	GF_CommandField *inf;
	Bool italic, underlined, bold;
	u32 state, curLine, line, i, len;
	char szLine[2048], szText[2048], *ptr;
	GF_StreamContext *sc = NULL;

	if (!ctx->scene_graph) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] base scene not assigned\n"));
		return GF_BAD_PARAM;
	}
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		if (sc->streamType==GF_STREAM_SCENE) break;
		sc = NULL;
	}

	if (!sc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] cannot locate base scene\n"));
		return GF_BAD_PARAM;
	}
	if (!mux->textNode) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] Target text node unspecified\n"));
		return GF_BAD_PARAM;
	}
	text = gf_sg_find_node_by_name(ctx->scene_graph, mux->textNode);
	if (!text) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] cannot find target text node %s\n", mux->textNode));
		return GF_BAD_PARAM;
	}
	if (gf_node_get_field_by_name(text, "string", &string) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] Target text node %s doesn't look like text\n", mux->textNode));
		return GF_BAD_PARAM;
	}

	font = NULL;
	if (mux->fontNode) {
		font = gf_sg_find_node_by_name(ctx->scene_graph, mux->fontNode);
		if (!font) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] cannot find target font node %s\n", mux->fontNode));
			return GF_BAD_PARAM;
		}
		if (gf_node_get_field_by_name(font, "style", &style) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] Target font node %s doesn't look like font\n", mux->fontNode));
			return GF_BAD_PARAM;
		}
	}

	srt_in = gf_fopen(mux->file_name, "rt");
	if (!srt_in) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] cannot open input file %s\n", mux->file_name));
		return GF_URL_ERROR;
	}

	srt = gf_sm_stream_new(ctx, src->ESID, GF_STREAM_SCENE, GF_CODECID_BIFS);
	if (!srt) return GF_OUT_OF_MEM;

	if (!src->slConfig) src->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	src->slConfig->timestampResolution = 1000;
	if (!src->decoderConfig) src->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	src->decoderConfig->streamType = GF_STREAM_SCENE;
	src->decoderConfig->objectTypeIndication = GF_CODECID_BIFS;

	e = GF_OK;
	state = end = 0;
	curLine = 0;
	au = NULL;
	com = NULL;
	italic = underlined = bold = 0;
	inf = NULL;

	while (1) {
		char *sOK = gf_fgets(szLine, 2048, srt_in);

		if (sOK) REM_TRAIL_MARKS(szLine, "\r\n\t ")

			if (!sOK || !strlen(szLine)) {
				state = 0;
				if (au) {
					/*if italic or underscore do it*/
					if (font && (italic || underlined || bold)) {
						com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
						com->node = font;
						gf_node_register(font, NULL);
						inf = gf_sg_command_field_new(com);
						inf->fieldIndex = style.fieldIndex;
						inf->fieldType = style.fieldType;
						inf->field_ptr = gf_sg_vrml_field_pointer_new(style.fieldType);
						sfstr = (SFString *)inf->field_ptr;
						if (bold && italic && underlined) sfstr->buffer = gf_strdup("BOLDITALIC UNDERLINED");
						else if (italic && underlined) sfstr->buffer = gf_strdup("ITALIC UNDERLINED");
						else if (bold && underlined) sfstr->buffer = gf_strdup("BOLD UNDERLINED");
						else if (underlined) sfstr->buffer = gf_strdup("UNDERLINED");
						else if (bold && italic) sfstr->buffer = gf_strdup("BOLDITALIC");
						else if (bold) sfstr->buffer = gf_strdup("BOLD");
						else sfstr->buffer = gf_strdup("ITALIC");
						gf_list_add(au->commands, com);
					}

					au = gf_sm_stream_au_new(srt, end, 0, 1);
					com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
					com->node = text;
					gf_node_register(text, NULL);
					inf = gf_sg_command_field_new(com);
					inf->fieldIndex = string.fieldIndex;
					inf->fieldType = string.fieldType;
					inf->field_ptr = gf_sg_vrml_field_pointer_new(string.fieldType);
					gf_list_add(au->commands, com);
					/*reset font styles so that all AUs are true random access*/
					if (font) {
						com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
						com->node = font;
						gf_node_register(font, NULL);
						inf = gf_sg_command_field_new(com);
						inf->fieldIndex = style.fieldIndex;
						inf->fieldType = style.fieldType;
						inf->field_ptr = gf_sg_vrml_field_pointer_new(style.fieldType);
						gf_list_add(au->commands, com);
					}
					au = NULL;
				}
				inf = NULL;
				if (!sOK) break;
				continue;
			}

		switch (state) {
		case 0:
			if (sscanf(szLine, "%u", &line) != 1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] bad frame format (src: %s)\n", szLine));
				e = GF_CORRUPTED_DATA;
				goto exit;
			}
			if (line != curLine + 1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] bad frame: previous %d - current %d (src: %s)\n", curLine, line, szLine));
				e = GF_CORRUPTED_DATA;
				goto exit;
			}
			curLine = line;
			state = 1;
			break;
		case 1:
			if (sscanf(szLine, "%u:%u:%u,%u --> %u:%u:%u,%u", &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) != 8) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[srt->bifs] bad frame %u (src: %s)\n", curLine, szLine));
				e = GF_CORRUPTED_DATA;
				goto exit;
			}
			start = (3600*sh + 60*sm + ss)*1000 + sms;
			if (start<end) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[srt->bifs] corrupted frame starts before end of previous one (SRT Frame %d) - adjusting time stamps\n", curLine));
				start = end;
			}
			end = (3600*eh + 60*em + es)*1000 + ems;
			/*make stream start at 0 by inserting a fake AU*/
			if ((curLine==1) && start>0) {
				au = gf_sm_stream_au_new(srt, 0, 0, 1);
				com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
				com->node = text;
				gf_node_register(text, NULL);
				inf = gf_sg_command_field_new(com);
				inf->fieldIndex = string.fieldIndex;
				inf->fieldType = string.fieldType;
				inf->field_ptr = gf_sg_vrml_field_pointer_new(string.fieldType);
				gf_list_add(au->commands, com);
			}

			au = gf_sm_stream_au_new(srt, start, 0, 1);
			com = NULL;
			state = 2;
			italic = underlined = bold = 0;
			break;

		default:
			ptr = szLine;
			/*FIXME - other styles posssibles ??*/
			while (1) {
				if (!strnicmp(ptr, "<i>", 3)) {
					italic = 1;
					ptr += 3;
				}
				else if (!strnicmp(ptr, "<u>", 3)) {
					underlined = 1;
					ptr += 3;
				}
				else if (!strnicmp(ptr, "<b>", 3)) {
					bold = 1;
					ptr += 3;
				}
				else
					break;
			}
			/*if style remove end markers*/
			while ((strlen(ptr)>4) && (ptr[strlen(ptr) - 4] == '<') && (ptr[strlen(ptr) - 1] == '>')) {
				ptr[strlen(ptr) - 4] = 0;
			}

			if (!com) {
				com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
				com->node = text;
				gf_node_register(text, NULL);
				inf = gf_sg_command_field_new(com);
				inf->fieldIndex = string.fieldIndex;
				inf->fieldType = string.fieldType;
				inf->field_ptr = gf_sg_vrml_field_pointer_new(string.fieldType);
				gf_list_add(au->commands, com);
			}
			if (!inf) {
				gf_assert(0);
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			gf_sg_vrml_mf_append(inf->field_ptr, GF_SG_VRML_MFSTRING, (void **) &sfstr);
			len = 0;
			for (i=0; i<strlen(ptr); i++) {
				/*FIXME - UTF16 support !!*/
				if (ptr[i] & 0x80) {
					/*non UTF8 (likely some win-CP)*/
					if ((ptr[i+1] & 0xc0) != 0x80) {
						szText[len] = 0xc0 | ( (ptr[i] >> 6) & 0x3 );
						len++;
						ptr[i] &= 0xbf;
					}
					/*we only handle UTF8 chars on 2 bytes (eg first byte is 0b110xxxxx)*/
					else if ((ptr[i] & 0xe0) == 0xc0) {
						szText[len] = ptr[i];
						len++;
						i++;
					}
				}
				szText[len] = ptr[i];
				len++;
			}
			szText[len] = 0;
			sfstr->buffer = gf_strdup(szText);
			break;
		}
	}

exit:
	gf_fclose(srt_in);
	return e;
}
#endif /*GPAC_DISABLE_VRML*/


#ifndef GPAC_DISABLE_VRML

static GF_Err gf_text_import_sub_bifs(GF_SceneManager *ctx, GF_ESD *src, GF_MuxInfo *mux)
{
	GF_Err e;
	GF_Node *text, *font;
	GF_StreamContext *srt;
	FILE *sub_in;
	GF_FieldInfo string, style;
	u32 start, end, line, i, j, k, len;
	GF_AUContext *au;
	GF_Command *com;
	SFString *sfstr;
	GF_CommandField *inf;
	Bool first_samp;
	char szLine[2048], szTime[30], szText[2048];
	GF_StreamContext *sc = NULL;

	if (!ctx->scene_graph) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] base scene not assigned\n"));
		return GF_BAD_PARAM;
	}
	i=0;
	while ((sc = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		if (sc->streamType==GF_STREAM_SCENE) break;
		sc = NULL;
	}

	if (!sc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] cannot locate base scene\n"));
		return GF_BAD_PARAM;
	}
	if (!mux->textNode) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] target text node unspecified\n"));
		return GF_BAD_PARAM;
	}
	text = gf_sg_find_node_by_name(ctx->scene_graph, mux->textNode);
	if (!text) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] cannot find target text node %s\n", mux->textNode));
		return GF_BAD_PARAM;
	}
	if (gf_node_get_field_by_name(text, "string", &string) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] target text node %s doesn't look like text\n", mux->textNode));
		return GF_BAD_PARAM;
	}

	font = NULL;
	if (mux->fontNode) {
		font = gf_sg_find_node_by_name(ctx->scene_graph, mux->fontNode);
		if (!font) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] cannot find target font node %s\n", mux->fontNode));
			return GF_BAD_PARAM;
		}
		if (gf_node_get_field_by_name(font, "style", &style) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] target font node %s doesn't look like font\n", mux->fontNode));
			return GF_BAD_PARAM;
		}
	}

	sub_in = gf_fopen(mux->file_name, "rt");
	if (!sub_in) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] cannot open input file %s\n", mux->file_name));
		return GF_URL_ERROR;
	}

	srt = gf_sm_stream_new(ctx, src->ESID, GF_STREAM_SCENE, GF_CODECID_BIFS);
	if (!srt) return GF_OUT_OF_MEM;

	if (!src->slConfig) src->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
	src->slConfig->timestampResolution = 1000;
	if (!src->decoderConfig) src->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	src->decoderConfig->streamType = GF_STREAM_SCENE;
	src->decoderConfig->objectTypeIndication = GF_CODECID_BIFS;

	e = GF_OK;
	end = 0;
	au = NULL;
	com = NULL;
	inf = NULL;

	line = 0;
	first_samp = 1;
	while (1) {
		char *sOK = gf_fgets(szLine, 2048, sub_in);
		if (!sOK) break;
		REM_TRAIL_MARKS(szLine, "\r\n\t ")

		line++;
		len = (u32) strlen(szLine);
		if (!len) continue;

		i=0;
		if (szLine[i] != '{') {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] Bad frame (line %d): expecting \"{\" got \"%c\"\n", line, szLine[i]));
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}
		while (szLine[i+1] && szLine[i+1]!='}') {
			szTime[i] = szLine[i+1];
			i++;
		}
		szTime[i] = 0;
		start = atoi(szTime);
		if (start<end) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[sub->bifs] corrupted SUB frame (line %d) - starts (at %d ms) before end of previous one (%d ms) - adjusting time stamps\n", line, start, end));
			start = end;
		}
		j=i+2;
		i=0;
		if (szLine[i+j] != '{') {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[sub->bifs] Bad frame - expecting \"{\" got \"%c\"\n", szLine[i]));
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}
		while (szLine[i+1+j] && szLine[i+1+j]!='}') {
			szTime[i] = szLine[i+1+j];
			i++;
		}
		szTime[i] = 0;
		end = atoi(szTime);
		j+=i+2;

		if (start>end) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[sub->bifs] corrupted frame (line %d) - ends (at %d ms) before start of current frame (%d ms) - skipping\n", line, end, start));
			continue;
		}

		if (start && first_samp) {
			au = gf_sm_stream_au_new(srt, 0, 0, 1);
			com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
			com->node = text;
			gf_node_register(text, NULL);
			inf = gf_sg_command_field_new(com);
			inf->fieldIndex = string.fieldIndex;
			inf->fieldType = string.fieldType;
			inf->field_ptr = gf_sg_vrml_field_pointer_new(string.fieldType);
			gf_list_add(au->commands, com);
		}

		k=0;
		for (i=j; i<len; i++) {
			if (szLine[i]=='|') {
				szText[k] = '\n';
			} else {
				if (szLine[i] & 0x80) {
					/*non UTF8 (likely some win-CP)*/
					if ( (szLine[i+1] & 0xc0) != 0x80) {
						szText[k] = 0xc0 | ( (szLine[i] >> 6) & 0x3 );
						k++;
						szLine[i] &= 0xbf;
					}
					/*we only handle UTF8 chars on 2 bytes (eg first byte is 0b110xxxxx)*/
					else if ( (szLine[i] & 0xe0) == 0xc0) {
						szText[k] = szLine[i];
						i++;
						k++;
					}
				}
				szText[k] = szLine[i];
			}
			k++;
		}
		szText[i-j] = 0;

		if (au) {
			com = gf_sg_command_new(ctx->scene_graph, GF_SG_FIELD_REPLACE);
			com->node = text;
			gf_node_register(text, NULL);
			inf = gf_sg_command_field_new(com);
			inf->fieldIndex = string.fieldIndex;
			inf->fieldType = string.fieldType;
			inf->field_ptr = gf_sg_vrml_field_pointer_new(string.fieldType);
			gf_list_add(au->commands, com);

			gf_sg_vrml_mf_append(inf->field_ptr, GF_SG_VRML_MFSTRING, (void **) &sfstr);
			sfstr->buffer = gf_strdup(szText);
		}
	}
	gf_fclose(sub_in);
	return e;
}
#endif


GF_EXPORT
GF_Err gf_sm_import_bifs_subtitle(GF_SceneManager *ctx, GF_ESD *src, GF_MuxInfo *mux)
{
#ifndef GPAC_DISABLE_VRML
	GF_Err e;
	u32 fmt;
	e = gf_text_guess_format(mux->file_name, &fmt);
	if (e) return e;
	if (!fmt || (fmt>=3)) return GF_NOT_SUPPORTED;

	if (fmt==1) return gf_text_import_srt_bifs(ctx, src, mux);
	else return gf_text_import_sub_bifs(ctx, src, mux);
#else
	return GF_NOT_SUPPORTED;
#endif
}

