/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / text import filter
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
#include <gpac/utf.h>
#include <gpac/xml.h>
#include <gpac/token.h>
#include <gpac/color.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_SWF_IMPORT
/* SWF Importer */
#include <gpac/internal/swf_dev.h>
#endif


typedef struct __txtin_ctx GF_TXTIn;

struct __txtin_ctx
{
	//opts
	u32 width, height, x, y, fontsize;
	s32 zorder;
	const char *fontname, *lang;
	Bool nodefbox, noflush, webvtt;
	u32 timescale;
	GF_Fraction fps;


	GF_FilterPid *ipid, *opid;
	const char *file_name;
	u32 fmt;
	u32 playstate;
	//0: not seeking, 1: seek request pending, 2: seek configured, discarding packets up until start_range
	u32 seek_state;
	Double start_range;

	Bool is_setup;

	GF_Err (*text_process)(GF_Filter *filter, GF_TXTIn *ctx);

	s32 unicode_type;

	FILE *src;

	GF_BitStream *bs_w;
	Bool first_samp;
	Bool hdr_parsed;

	//state vars for srt
	u32 state, default_color;
	GF_TextSample *samp;
	u64 start, end, prev_end;
	u32 curLine;
	GF_StyleRecord style;

	//WebVTT state
	GF_WebVTTParser *vttparser;

	//TTXT state
	GF_DOMParser *parser;
	u32 cur_child_idx, nb_children, last_desc_idx;
	GF_List *text_descs;
	Bool last_sample_empty;
	u64 last_sample_duration;
	//TTML state is the same as ttxt plus the timescale and start (webvtt) for cts compute
	u32 txml_timescale;

	//TTML state
	GF_XMLNode *root_working_copy, *div_node, *sample_list_node;
	GF_DOMParser *parser_working_copy;
	Bool non_compliant_ttml;
	u32 nb_p_found;



#ifndef GPAC_DISABLE_SWF_IMPORT
	//SWF text
	SWFReader *swf_parse;
	Bool do_suspend;
#endif


};


enum
{
	GF_TXTIN_MODE_NONE = 0,
	GF_TXTIN_MODE_SRT,
	GF_TXTIN_MODE_SUB,
	GF_TXTIN_MODE_TTXT,
	GF_TXTIN_MODE_TEXML,
	GF_TXTIN_MODE_WEBVTT,
	GF_TXTIN_MODE_TTML,
	GF_TXTIN_MODE_SWF_SVG,
};

#define REM_TRAIL_MARKS(__str, __sep) while (1) {	\
		u32 _len = (u32) strlen(__str);		\
		if (!_len) break;	\
		_len--;				\
		if (strchr(__sep, __str[_len])) __str[_len] = 0;	\
		else break;	\
	}	\
 

s32 gf_text_get_utf_type(FILE *in_src)
{
	u32 read;
	unsigned char BOM[5];
	read = (u32) fread(BOM, sizeof(char), 5, in_src);
	if ((s32) read < 1)
		return -1;

	if ((BOM[0]==0xFF) && (BOM[1]==0xFE)) {
		/*UTF32 not supported*/
		if (!BOM[2] && !BOM[3]) return -1;
		gf_fseek(in_src, 2, SEEK_SET);
		return 3;
	}
	if ((BOM[0]==0xFE) && (BOM[1]==0xFF)) {
		/*UTF32 not supported*/
		if (!BOM[2] && !BOM[3]) return -1;
		gf_fseek(in_src, 2, SEEK_SET);
		return 2;
	} else if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) {
		gf_fseek(in_src, 3, SEEK_SET);
		return 1;
	}
	if (BOM[0]<0x80) {
		gf_fseek(in_src, 0, SEEK_SET);
		return 0;
	}
	return -1;
}
static void ttxt_dom_progress(void *cbk, u64 cur_samp, u64 count)
{
	GF_TXTIn *ctx = (GF_TXTIn *)cbk;
	ctx->end = count;
}

static GF_Err gf_text_guess_format(const char *filename, u32 *fmt)
{
	char szLine[2048];
	u32 val;
	s32 uni_type;
	FILE *test = gf_fopen(filename, "rb");
	if (!test) return GF_URL_ERROR;
	uni_type = gf_text_get_utf_type(test);

	if (uni_type>1) {
		const u16 *sptr;
		char szUTF[1024];
		u32 read = (u32) fread(szUTF, 1, 1023, test);
		if ((s32) read < 0) {
			gf_fclose(test);
			return GF_IO_ERR;
		}
		szUTF[read]=0;
		sptr = (u16*)szUTF;
		/*read = (u32) */gf_utf8_wcstombs(szLine, read, &sptr);
	} else {
		val = (u32) fread(szLine, 1, 1024, test);
		if ((s32) val<0) return GF_IO_ERR;
		
		szLine[val]=0;
	}
	REM_TRAIL_MARKS(szLine, "\r\n\t ")

	*fmt = GF_TXTIN_MODE_NONE;
	if ((szLine[0]=='{') && strstr(szLine, "}{")) *fmt = GF_TXTIN_MODE_SUB;
	else if (szLine[0] == '<') {
		char *ext = strrchr(filename, '.');
		if (!strnicmp(ext, ".ttxt", 5)) *fmt = GF_TXTIN_MODE_TTXT;
		else if (!strnicmp(ext, ".ttml", 5)) *fmt = GF_TXTIN_MODE_TTML;
		ext = strstr(szLine, "?>");
		if (ext) ext += 2;
		if (ext && !ext[0]) {
			if (!fgets(szLine, 2048, test))
				szLine[0] = '\0';
		}
		if (strstr(szLine, "x-quicktime-tx3g") || strstr(szLine, "text3GTrack")) *fmt = GF_TXTIN_MODE_TEXML;
		else if (strstr(szLine, "TextStream")) *fmt = GF_TXTIN_MODE_TTXT;
		else if (strstr(szLine, "tt")) *fmt = GF_TXTIN_MODE_TTML;
	}
	else if (strstr(szLine, "WEBVTT") )
		*fmt = GF_TXTIN_MODE_WEBVTT;
	else if (strstr(szLine, " --> ") )
		*fmt = GF_TXTIN_MODE_SRT; /* might want to change the default to WebVTT */

	else if (!strncmp(szLine, "FWS", 3) || !strncmp(szLine, "CWS", 3))
		*fmt = GF_TXTIN_MODE_SWF_SVG;

	gf_fclose(test);
	return GF_OK;
}



char *gf_text_get_utf8_line(char *szLine, u32 lineSize, FILE *txt_in, s32 unicode_type)
{
	u32 i, j, len;
	char *sOK;
	char szLineConv[1024];
	unsigned short *sptr;

	memset(szLine, 0, sizeof(char)*lineSize);
	sOK = fgets(szLine, lineSize, txt_in);
	if (!sOK) return NULL;
	if (unicode_type<=1) {
		j=0;
		len = (u32) strlen(szLine);
		for (i=0; i<len; i++) {
			if (!unicode_type && (szLine[i] & 0x80)) {
				/*non UTF8 (likely some win-CP)*/
				if ((szLine[i+1] & 0xc0) != 0x80) {
					szLineConv[j] = 0xc0 | ( (szLine[i] >> 6) & 0x3 );
					j++;
					szLine[i] &= 0xbf;
				}
				/*UTF8 2 bytes char*/
				else if ( (szLine[i] & 0xe0) == 0xc0) {
					szLineConv[j] = szLine[i];
					i++;
					j++;
				}
				/*UTF8 3 bytes char*/
				else if ( (szLine[i] & 0xf0) == 0xe0) {
					szLineConv[j] = szLine[i];
					i++;
					j++;
					szLineConv[j] = szLine[i];
					i++;
					j++;
				}
				/*UTF8 4 bytes char*/
				else if ( (szLine[i] & 0xf8) == 0xf0) {
					szLineConv[j] = szLine[i];
					i++;
					j++;
					szLineConv[j] = szLine[i];
					i++;
					j++;
					szLineConv[j] = szLine[i];
					i++;
					j++;
				} else {
					i+=1;
					continue;
				}
			}
			szLineConv[j] = szLine[i];
			j++;
		}
		szLineConv[j] = 0;
		strcpy(szLine, szLineConv);
		return sOK;
	}

#ifdef GPAC_BIG_ENDIAN
	if (unicode_type==3) {
#else
	if (unicode_type==2) {
#endif
		i=0;
		while (1) {
			char c;
			if (!szLine[i] && !szLine[i+1]) break;
			c = szLine[i+1];
			szLine[i+1] = szLine[i];
			szLine[i] = c;
			i+=2;
		}
	}
	sptr = (u16 *)szLine;
	i = (u32) gf_utf8_wcstombs(szLineConv, 1024, (const unsigned short **) &sptr);
	szLineConv[i] = 0;
	strcpy(szLine, szLineConv);
	/*this is ugly indeed: since input is UTF16-LE, there are many chances the fgets never reads the \0 after a \n*/
	if (unicode_type==3) fgetc(txt_in);
	return sOK;
}


static void txtin_probe_duration(GF_TXTIn *ctx)
{
	GF_Fraction dur;
	dur.num = 0;

	if (ctx->fmt == GF_TXTIN_MODE_SWF_SVG) {
#ifndef GPAC_DISABLE_SWF_IMPORT
		u32 frame_count, frame_rate;
		gf_swf_get_duration(ctx->swf_parse, &frame_rate, &frame_count);
		if (frame_count) {
			GF_Fraction dur;
			dur.num = frame_count;
			dur.den = frame_rate;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC(dur));
		}
#endif
		return;
	}
	if ((ctx->fmt == GF_TXTIN_MODE_SRT) || (ctx->fmt == GF_TXTIN_MODE_WEBVTT)  || (ctx->fmt == GF_TXTIN_MODE_SUB)) {
		u64 pos = gf_ftell(ctx->src);
		gf_fseek(ctx->src, 0, SEEK_SET);
		while (!feof(ctx->src)) {
			u64 end;
			char szLine[2048], szText[2048];
			char *sOK = gf_text_get_utf8_line(szLine, 2048, ctx->src, ctx->unicode_type);
			if (!sOK) break;
			REM_TRAIL_MARKS(szLine, "\r\n\t ")

			if (ctx->fmt == GF_TXTIN_MODE_SUB) {
				u32 sframe, eframe;
				if (sscanf(szLine, "{%d}{%d}%s", &sframe, &eframe, szText) == 3) {
					if (ctx->fps.den)
						end = 1000 * eframe * ctx->fps.num / ctx->fps.den;
					else
						end = 1000 * eframe / 25;
					if (end>dur.num) dur.num = (s32) end;
				}
			} else {
				u32 eh, em, es, ems;
				char *start = strstr(szLine, "-->");
				if (!start) continue;
				while (start[0] && ((start[0] == ' ') || (start[0] == '\t'))) start++;

				if (sscanf(start, "%u:%u:%u,%u", &eh, &em, &es, &ems) != 4) {
					eh = 0;
					if (sscanf(szLine, "%u:%u,%u", &em, &es, &ems) != 3) {
						continue;
					}
				}
				end = (3600*eh + 60*em + es)*1000 + ems;
				if (end>dur.num) dur.num = (s32) end;
			}
		}
		gf_fseek(ctx->src, pos, SEEK_SET);
		if (dur.num) {
			dur.den = 1000;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC(dur));
		}
		return;
	}
	if ((ctx->fmt == GF_TXTIN_MODE_TTXT) || (ctx->fmt == GF_TXTIN_MODE_TEXML)) {
		u32 i=0;
		GF_XMLNode *node, *root = gf_xml_dom_get_root(ctx->parser);
		while ((node = gf_list_enum(root->content, &i))) {
			u32 j;
			u64 duration;
			GF_XMLAttribute *att;
			if (node->type) {
				continue;
			}
			/*sample text*/
			if ((ctx->fmt == GF_TXTIN_MODE_TTXT) && strcmp(node->name, "TextSample")) continue;
			else if ((ctx->fmt == GF_TXTIN_MODE_TEXML) && strcmp(node->name, "sample")) continue;


			j=0;
			while ( (att=(GF_XMLAttribute*)gf_list_enum(node->attributes, &j))) {
				u32 h, m, s, ms;
				u64 ts=0;
				if (ctx->fmt == GF_TXTIN_MODE_TTXT) {
					if (strcmp(att->name, "sampleTime")) continue;

					if (sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
						ts = (h*3600 + m*60 + s)*1000 + ms;
					} else {
						ts = (u32) (atof(att->value) * 1000);
					}
					if (ts > dur.num) dur.num = (s32) ts;
				} else {
					if (strcmp(att->name, "duration")) continue;
					duration = atoi(att->value);
					dur.num += (s32) ( (1000 * duration) / ctx->txml_timescale);
				}
			}
		}
		if (dur.num) {
			dur.den = 1000;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC(dur));
		}
		return;
	}

	if (ctx->fmt == GF_TXTIN_MODE_TTML) {
		u32 i=0;
		GF_XMLNode *node, *p_node;

		while ((node = gf_list_enum(ctx->div_node->content, &i))) {
			GF_XMLAttribute *att;
			u32 h, m, s, ms, p_idx=0;
			u64 ts_end=0;
			h = m = s = ms = 0;
			while ( (att = (GF_XMLAttribute*)gf_list_enum(node->attributes, &p_idx))) {
				if (!att) continue;
				if (strcmp(att->name, "end")) continue;

				if (sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts_end = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(att->value, "%u:%u:%u", &h, &m, &s) == 3) {
					ts_end = (h*3600 + m*60+s)*1000;
				}
			}
			//or under a <span>
			p_idx = 0;
			while ( (p_node = (GF_XMLNode*)gf_list_enum(node->content, &p_idx))) {
				u32 span_idx = 0;
				while ( (att = (GF_XMLAttribute*)gf_list_enum(p_node->attributes, &span_idx))) {
					if (!att) continue;
					if (strcmp(att->name, "end")) continue;
					if (sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
						ts_end = (h*3600 + m*60+s)*1000+ms;
					} else if (sscanf(att->value, "%u:%u:%u", &h, &m, &s) == 3) {
						ts_end = (h*3600 + m*60+s)*1000;
					}
				}
			}
			if (ts_end>dur.num) dur.num = (s32) ts_end;
		}
		if (dur.num) {
			dur.den = 1000;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, &PROP_FRAC(dur));
		}
		return;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Duration probing not supported for format %d\n", ctx->fmt));
}

static GF_Err txtin_setup_srt(GF_Filter *filter, GF_TXTIn *ctx)
{
	u32 ID, OCR_ES_ID, dsi_len, file_size;
	u8 *dsi;
	GF_TextSampleDescriptor *sd;

	ctx->src = gf_fopen(ctx->file_name, "rt");
	if (!ctx->src) return GF_URL_ERROR;

	gf_fseek(ctx->src, 0, SEEK_END);
	file_size = (u32) gf_ftell(ctx->src);
	gf_fseek(ctx->src, 0, SEEK_SET);

	ctx->unicode_type = gf_text_get_utf_type(ctx->src);
	if (ctx->unicode_type<0) {
		gf_fclose(ctx->src);
		ctx->src = NULL;
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Unsupported SRT UTF encoding\n"));
		return GF_NOT_SUPPORTED;
	}

	if (!ctx->timescale) ctx->timescale = 1000;
	OCR_ES_ID = ID = 0;

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_TX3G) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(file_size) );

	if (!ID) ID = 1;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(ID) );
	if (OCR_ES_ID) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(OCR_ES_ID) );
	if (ctx->width) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	if (ctx->height) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	if (ctx->zorder) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ZORDER, &PROP_SINT(ctx->zorder) );
	if (ctx->lang) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_LANGUAGE, &PROP_STRING( ctx->lang) );

	sd = (GF_TextSampleDescriptor*)gf_odf_desc_new(GF_ODF_TX3G_TAG);
	sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
	sd->font_count = 1;
	sd->fonts[0].fontID = 1;
	sd->fonts[0].fontName = gf_strdup(ctx->fontname ? ctx->fontname : "Serif");
	sd->back_color = 0x00000000;	/*transparent*/
	sd->default_style.fontID = 1;
	sd->default_style.font_size = ctx->fontsize;
	sd->default_style.text_color = 0xFFFFFFFF;	/*white*/
	sd->default_style.style_flags = 0;
	sd->horiz_justif = 1; /*center of scene*/
	sd->vert_justif = (s8) -1;	/*bottom of scene*/

	if (ctx->nodefbox) {
		sd->default_pos.top = sd->default_pos.left = sd->default_pos.right = sd->default_pos.bottom = 0;
	} else if ((sd->default_pos.bottom==sd->default_pos.top) || (sd->default_pos.right==sd->default_pos.left)) {
		sd->default_pos.left = ctx->x;
		sd->default_pos.top = ctx->y;
		sd->default_pos.right = ctx->width + sd->default_pos.left;
		sd->default_pos.bottom = ctx->height + sd->default_pos.top;
	}

	/*store attribs*/
	ctx->style = sd->default_style;
	gf_odf_tx3g_write(sd, &dsi, &dsi_len);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_len) );

	gf_odf_desc_del((GF_Descriptor *)sd);

	ctx->default_color = ctx->style.text_color;
	ctx->samp = gf_isom_new_text_sample();
	ctx->state = 0;
	ctx->end = ctx->prev_end = ctx->start = 0;
	ctx->first_samp = GF_TRUE;
	ctx->curLine = 0;

	txtin_probe_duration(ctx);
	return GF_OK;
}

static void txtin_process_send_text_sample(GF_TXTIn *ctx, GF_TextSample *txt_samp, u64 ts, u32 duration, Bool is_rap)
{
	GF_FilterPacket *dst_pck;
	u8 *pck_data;
	u32 size;

	if (ctx->seek_state==2) {
		Double end = (Double) (ts+duration);
		end /= 1000;
		if (end < ctx->start_range) return;
		ctx->seek_state = 0;
	}

	size = gf_isom_text_sample_size(txt_samp);

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &pck_data);
	gf_bs_reassign_buffer(ctx->bs_w, pck_data, size);
	gf_isom_text_sample_write_bs(txt_samp, ctx->bs_w);

	ts *= ctx->timescale;
	ts /= 1000;
	duration *= ctx->timescale;
	duration /= 1000;

	gf_filter_pck_set_sap(dst_pck, is_rap ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
	gf_filter_pck_set_cts(dst_pck, ts);
	gf_filter_pck_set_duration(dst_pck, duration);

	gf_filter_pck_send(dst_pck);
}

static GF_Err txtin_process_srt(GF_Filter *filter, GF_TXTIn *ctx)
{
	u32 i;
	u32 sh, sm, ss, sms, eh, em, es, ems, txt_line, char_len, char_line, j, rem_styles;
	Bool set_start_char, set_end_char, rem_color;
	u32 line, len;
	char szLine[2048], szText[2048], *ptr;
	unsigned short uniLine[5000], uniText[5000], *sptr;

	if (!ctx->is_setup) {
		ctx->is_setup = GF_TRUE;
		return txtin_setup_srt(filter, ctx);
	}
	if (!ctx->opid) return GF_NOT_SUPPORTED;
	if (!ctx->playstate) return GF_OK;
	else if (ctx->playstate==2) return GF_EOS;

	txt_line = 0;
	set_start_char = set_end_char = GF_FALSE;
	char_len = 0;

	if (ctx->seek_state == 1) {
		ctx->seek_state = 2;
		gf_fseek(ctx->src, 0, SEEK_SET);
	}

	while (1) {
		char *sOK = gf_text_get_utf8_line(szLine, 2048, ctx->src, ctx->unicode_type);

		if (sOK) REM_TRAIL_MARKS(szLine, "\r\n\t ")

		if (!sOK || !strlen(szLine)) {
			u32 nb_empty = 1;
			u32 pos = (u32) gf_ftell(ctx->src);
			if (ctx->state) {
				while (!feof(ctx->src)) {
					sOK = gf_text_get_utf8_line(szLine+nb_empty, 2048-nb_empty, ctx->src, ctx->unicode_type);
					if (sOK) REM_TRAIL_MARKS((szLine+nb_empty), "\r\n\t ")

					if (!sOK) {
						gf_fseek(ctx->src, pos, SEEK_SET);
						break;
					} else if (!strlen(szLine+nb_empty)) {
						nb_empty++;
						continue;
					} else if (	sscanf(szLine+nb_empty, "%u", &line) == 1) {
						gf_fseek(ctx->src, pos, SEEK_SET);
						break;
					} else {
						u32 k;
						for (k=0; k<nb_empty; k++) szLine[k] = '\n';
						goto force_line;
					}
				}
			}
			ctx->style.style_flags = 0;
			ctx->style.startCharOffset = ctx->style.endCharOffset = 0;
			if (txt_line) {
				if (ctx->prev_end && (ctx->start != ctx->prev_end) && (ctx->state<=2)) {
					GF_TextSample * empty_samp = gf_isom_new_text_sample();
					txtin_process_send_text_sample(ctx, empty_samp, ctx->prev_end, (u32) (ctx->start - ctx->prev_end), GF_TRUE );
					gf_isom_delete_text_sample(empty_samp);
				}

				if (ctx->state<=2) {
					txtin_process_send_text_sample(ctx, ctx->samp,  ctx->start, (u32) (ctx->end -  ctx->start), GF_TRUE);
					ctx->prev_end = ctx->end;
				}
				txt_line = 0;
				char_len = 0;
				set_start_char = set_end_char = GF_FALSE;
				ctx->style.startCharOffset = ctx->style.endCharOffset = 0;
				gf_isom_text_reset(ctx->samp);

				gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT( gf_ftell(ctx->src )) );
			}
			ctx->state = 0;
			if (!sOK) break;
			continue;
		}

force_line:
		switch (ctx->state) {
		case 0:
			if (sscanf(szLine, "%u", &line) != 1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Bad SRT formatting - expecting number got \"%s\"\n", szLine));
				break;
			}
			if (line != ctx->curLine + 1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Corrupted SRT frame %d after frame %d\n", line, ctx->curLine));
			}
			ctx->curLine = line;
			ctx->state = 1;
			break;
		case 1:
			if (sscanf(szLine, "%u:%u:%u,%u --> %u:%u:%u,%u", &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) != 8) {
				if (sscanf(szLine, "%u:%u:%u.%u --> %u:%u:%u.%u", &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) != 8) {
					sh = eh = 0;
					if (sscanf(szLine, "%u:%u,%u --> %u:%u,%u", &sm, &ss, &sms, &em, &es, &ems) != 6) {
						if (sscanf(szLine, "%u:%u.%u --> %u:%u.%u", &sm, &ss, &sms, &em, &es, &ems) != 6) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Error scanning SRT frame %d timing\n", ctx->curLine));
				    		ctx->state = 0;
							break;
						}
					}
				}
			}
			ctx->start = (3600*sh + 60*sm + ss)*1000 + sms;
			if (ctx->start < ctx->end) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Overlapping SRT frame %d - starts "LLD" ms is before end of previous one "LLD" ms - adjusting time stamps\n", ctx->curLine, ctx->start, ctx->end));
				ctx->start = ctx->end;
			}

			ctx->end = (3600*eh + 60*em + es)*1000 + ems;
			/*make stream start at 0 by inserting a fake AU*/
			if (ctx->first_samp && (ctx->start > 0)) {
				txtin_process_send_text_sample(ctx, ctx->samp, 0, (u32) ctx->start, GF_TRUE);
			}
			ctx->style.style_flags = 0;
			ctx->state = 2;
			if (ctx->end <= ctx->prev_end) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Overlapping SRT frame %d end "LLD" is at or before previous end "LLD" - removing\n", ctx->curLine, ctx->end, ctx->prev_end));
				ctx->start = ctx->end;
				ctx->state = 3;
			}
			break;

		default:
			/*reset only when text is present*/
			ctx->first_samp = GF_FALSE;

			/*go to line*/
			if (txt_line) {
				gf_isom_text_add_text(ctx->samp, "\n", 1);
				char_len += 1;
			}

			ptr = (char *) szLine;
			{
				size_t _len = gf_utf8_mbstowcs(uniLine, 5000, (const char **) &ptr);
				if (_len == (size_t) -1) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Invalid UTF data (line %d)\n", ctx->curLine));
					ctx->state = 0;
				}
				len = (u32) _len;
			}
			i=j=0;
			rem_styles = 0;
			rem_color = 0;
			while (i<len) {
				u32 font_style = 0;
				u32 style_nb_chars = 0;
				u32 style_def_type = 0;

				if ( (uniLine[i]=='<') && (uniLine[i+2]=='>')) {
					style_nb_chars = 3;
					style_def_type = 1;
				}
				else if ( (uniLine[i]=='<') && (uniLine[i+1]=='/') && (uniLine[i+3]=='>')) {
					style_def_type = 2;
					style_nb_chars = 4;
				}
				else if (uniLine[i]=='<')  {
					const unsigned short* src = uniLine + i;
					size_t alen = gf_utf8_wcstombs(szLine, 2048, (const unsigned short**) & src);
					szLine[alen] = 0;
					strlwr(szLine);
					if (!strncmp(szLine, "<font ", 6) ) {
						char *a_sep = strstr(szLine, "color");
						if (a_sep) a_sep = strchr(a_sep, '"');
						if (a_sep) {
							char *e_sep = strchr(a_sep+1, '"');
							if (e_sep) {
								e_sep[0] = 0;
								font_style = gf_color_parse(a_sep+1);
								e_sep[0] = '"';
								e_sep = strchr(e_sep+1, '>');
								if (e_sep) {
									style_nb_chars = (u32) (1 + e_sep - szLine);
									style_def_type = 1;
								}
							}

						}
					}
					else if (!strncmp(szLine, "</font>", 7) ) {
						style_nb_chars = 7;
						style_def_type = 2;
						font_style = 0xFFFFFFFF;
					}
					//skip unknown
					else {
						char *a_sep = strstr(szLine, ">");
						if (a_sep) {
							style_nb_chars = (u32) (a_sep - szLine);
							i += style_nb_chars;
							continue;
						}
					}

				}

				/*start of new style*/
				if (style_def_type==1)  {
					/*store prev style*/
					if (set_end_char) {
						assert(set_start_char);
						gf_isom_text_add_style(ctx->samp, &ctx->style);
						set_end_char = set_start_char = GF_FALSE;
						ctx->style.style_flags &= ~rem_styles;
						rem_styles = 0;
						if (rem_color) {
							ctx->style.text_color = ctx->default_color;
							rem_color = 0;
						}
					}
					if (set_start_char && (ctx->style.startCharOffset != j)) {
						ctx->style.endCharOffset = char_len + j;
						if (ctx->style.style_flags) gf_isom_text_add_style(ctx->samp, &ctx->style);
					}
					switch (uniLine[i+1]) {
					case 'b':
					case 'B':
						ctx->style.style_flags |= GF_TXT_STYLE_BOLD;
						set_start_char = GF_TRUE;
						ctx->style.startCharOffset = char_len + j;
						break;
					case 'i':
					case 'I':
						ctx->style.style_flags |= GF_TXT_STYLE_ITALIC;
						set_start_char = GF_TRUE;
						ctx->style.startCharOffset = char_len + j;
						break;
					case 'u':
					case 'U':
						ctx->style.style_flags |= GF_TXT_STYLE_UNDERLINED;
						set_start_char = GF_TRUE;
						ctx->style.startCharOffset = char_len + j;
						break;
					case 'f':
					case 'F':
						if (font_style) {
							ctx->style.text_color = font_style;
							set_start_char = GF_TRUE;
							ctx->style.startCharOffset = char_len + j;
						}
						break;
					}
					i += style_nb_chars;
					continue;
				}

				/*end of prev style*/
				if (style_def_type==2)  {
					switch (uniLine[i+2]) {
					case 'b':
					case 'B':
						rem_styles |= GF_TXT_STYLE_BOLD;
						set_end_char = GF_TRUE;
						ctx->style.endCharOffset = char_len + j;
						break;
					case 'i':
					case 'I':
						rem_styles |= GF_TXT_STYLE_ITALIC;
						set_end_char = GF_TRUE;
						ctx->style.endCharOffset = char_len + j;
						break;
					case 'u':
					case 'U':
						rem_styles |= GF_TXT_STYLE_UNDERLINED;
						set_end_char = GF_TRUE;
						ctx->style.endCharOffset = char_len + j;
						break;
					case 'f':
					case 'F':
						if (font_style) {
							rem_color = 1;
							set_end_char = GF_TRUE;
							ctx->style.endCharOffset = char_len + j;
						}
					}
					i+=style_nb_chars;
					continue;
				}
				/*store style*/
				if (set_end_char) {
					gf_isom_text_add_style(ctx->samp, &ctx->style);
					set_end_char = GF_FALSE;
					set_start_char = GF_TRUE;
					ctx->style.startCharOffset = char_len + j;
					ctx->style.style_flags &= ~rem_styles;
					rem_styles = 0;
					ctx->style.text_color = ctx->default_color;
					rem_color = 0;
				}

				uniText[j] = uniLine[i];
				j++;
				i++;
			}
			/*store last style*/
			if (set_end_char) {
				gf_isom_text_add_style(ctx->samp, &ctx->style);
				set_end_char = GF_FALSE;
				set_start_char = GF_TRUE;
				ctx->style.startCharOffset = char_len + j;
				ctx->style.style_flags &= ~rem_styles;
			}

			char_line = j;
			uniText[j] = 0;

			sptr = (u16 *) uniText;
			len = (u32) gf_utf8_wcstombs(szText, 5000, (const u16 **) &sptr);

			gf_isom_text_add_text(ctx->samp, szText, len);
			char_len += char_line;
			txt_line ++;
			break;
		}

		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
	}

	/*final flush*/	
	if (ctx->end && ! ctx->noflush) {
		gf_isom_text_reset(ctx->samp);
		txtin_process_send_text_sample(ctx, ctx->samp, ctx->end, 0, GF_TRUE);
		ctx->end = 0;
	}
	gf_isom_text_reset(ctx->samp);

	return GF_EOS;
}

/* Structure used to pass importer and track data to the parsers without exposing the GF_MediaImporter structure
   used by WebVTT and Flash->SVG */
typedef struct {
	GF_TXTIn *ctx;
	u32 timescale;
	u32 track;
	u32 descriptionIndex;
} GF_ISOFlusher;

#ifndef GPAC_DISABLE_VTT

static GF_Err gf_webvtt_import_report(void *user, GF_Err e, char *message, const char *line)
{
	GF_LOG(e ? GF_LOG_WARNING : GF_LOG_INFO, GF_LOG_AUTHOR, ("[TXTIn] WebVTT line %s: %s\n", line, message) );
	return e;
}

static void gf_webvtt_import_header(void *user, const char *config)
{
	GF_TXTIn *ctx = (GF_TXTIn *)user;
	if (!ctx->hdr_parsed) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char *) config, (u32) (1+strlen(config)) ) );
		ctx->hdr_parsed = GF_TRUE;
		gf_webvtt_parser_suspend(ctx->vttparser);
	}
}

static void gf_webvtt_flush_sample(void *user, GF_WebVTTSample *samp)
{
	u64 start, end;
	GF_TXTIn *ctx = (GF_TXTIn *)user;
	GF_ISOSample *s;

	start = gf_webvtt_sample_get_start(samp);
	end = gf_webvtt_sample_get_end(samp);

	if (ctx->seek_state==2) {
		Double tsend = (Double) end;
		tsend /= 1000;
		if (tsend<ctx->start_range) return;
		ctx->seek_state = 0;
	}

	s = gf_isom_webvtt_to_sample(samp);
	if (s) {
		GF_FilterPacket *pck;
		u8 *pck_data;

		pck = gf_filter_pck_new_alloc(ctx->opid, s->dataLength, &pck_data);
		memcpy(pck_data, s->data, s->dataLength);
		gf_filter_pck_set_cts(pck, (u64) (ctx->timescale * start / 1000) );
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);


		if (end && (end>=start) ) {
			gf_filter_pck_set_duration(pck, (u32) (ctx->timescale * (end-start) / 1000) );
		}
		gf_filter_pck_send(pck);

		gf_isom_sample_del(&s);
	}
	gf_webvtt_sample_del(samp);

	gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT( gf_ftell(ctx->src )) );

	if (gf_filter_pid_would_block(ctx->opid))
		gf_webvtt_parser_suspend(ctx->vttparser);

}

static GF_Err txtin_webvtt_setup(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e;
	u32 ID, OCR_ES_ID, file_size, w, h;
	Bool is_srt;
	char *ext;

	ctx->src = gf_fopen(ctx->file_name, "rt");
	if (!ctx->src) return GF_URL_ERROR;

	gf_fseek(ctx->src, 0, SEEK_END);
	file_size = (u32) gf_ftell(ctx->src);
	gf_fseek(ctx->src, 0, SEEK_SET);

	ctx->unicode_type = gf_text_get_utf_type(ctx->src);
	if (ctx->unicode_type<0) {
		gf_fclose(ctx->src);
		ctx->src = NULL;
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Unsupported SRT UTF encoding\n"));
		return GF_NOT_SUPPORTED;
	}
	ext = strrchr(ctx->file_name, '.');
	is_srt = (ext && !strnicmp(ext, ".srt", 4)) ? GF_TRUE : GF_FALSE;


	if (!ctx->timescale) ctx->timescale = 1000;
	OCR_ES_ID = ID = 0;

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_WEBVTT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(file_size) );

	w = ctx->width;
	h = ctx->height;
	if (!ID) ID = 1;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(ID) );
	if (OCR_ES_ID) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(OCR_ES_ID) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
	if (ctx->zorder) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ZORDER, &PROP_SINT(ctx->zorder) );
	if (ctx->lang) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_LANGUAGE, &PROP_STRING( ctx->lang) );

	ctx->vttparser = gf_webvtt_parser_new();

	e = gf_webvtt_parser_init(ctx->vttparser, ctx->src, ctx->unicode_type, is_srt, ctx, gf_webvtt_import_report, gf_webvtt_flush_sample, gf_webvtt_import_header);
	if (e != GF_OK) {
		gf_webvtt_parser_del(ctx->vttparser);
		ctx->vttparser = NULL;
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] WebVTT parser init error %s\n", gf_error_to_string(e) ));
	}
	//get the header
	e = gf_webvtt_parser_parse(ctx->vttparser);

	txtin_probe_duration(ctx);
	return e;
}

static GF_Err txtin_process_webvtt(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e;

	if (!ctx->is_setup) {
		ctx->is_setup = GF_TRUE;
		return txtin_webvtt_setup(filter, ctx);
	}
	if (!ctx->vttparser) return GF_NOT_SUPPORTED;
	if (ctx->seek_state==1) {
		ctx->seek_state = 2;
		gf_webvtt_parser_restart(ctx->vttparser);
	}

	e = gf_webvtt_parser_parse(ctx->vttparser);
	if (e < GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] WebVTT process error %s\n", gf_error_to_string(e) ));
	}
	return e;
}

#endif /*GPAC_DISABLE_VTT*/

static char *ttxt_parse_string(char *str, Bool strip_lines)
{
	u32 i=0;
	u32 k=0;
	u32 len = (u32) strlen(str);
	u32 state = 0;

	if (!strip_lines) {
		for (i=0; i<len; i++) {
			if ((str[i] == '\r') && (str[i+1] == '\n')) {
				i++;
			}
			str[k] = str[i];
			k++;
		}
		str[k]=0;
		return str;
	}

	if (str[0]!='\'') return str;
	for (i=0; i<len; i++) {
		if (str[i] == '\'') {

			if (!state) {
				if (k) {
					str[k]='\n';
					k++;
				}
				state = !state;
			} else if (state) {
				if ( (i+1==len) ||
				        ((str[i+1]==' ') || (str[i+1]=='\n') || (str[i+1]=='\r') || (str[i+1]=='\t') || (str[i+1]=='\''))
				   ) {
					state = !state;
				} else {
					str[k] = str[i];
					k++;
				}
			}
		} else if (state) {
			str[k] = str[i];
			k++;
		}
	}
	str[k]=0;
	return str;
}

static void GF_TXTIN_MODE_ebu_ttd_remove_samples(GF_XMLNode *root, GF_XMLNode **sample_list_node)
{
	u32 idx = 0, body_num = 0;
	GF_XMLNode *node = NULL;
	*sample_list_node = NULL;
	while ( (node = (GF_XMLNode*)gf_list_enum(root->content, &idx))) {
		if (!strcmp(node->name, "body")) {
			GF_XMLNode *body_node;
			u32 body_idx = 0;
			while ( (body_node = (GF_XMLNode*)gf_list_enum(node->content, &body_idx))) {
				if (!strcmp(body_node->name, "div")) {
					*sample_list_node = body_node;
					body_num = gf_list_count(body_node->content);
					while (body_num--) {
						GF_XMLNode *content_node = (GF_XMLNode*)gf_list_get(body_node->content, 0);
						assert(gf_list_find(body_node->content, content_node) == 0);
						gf_list_rem(body_node->content, 0);
						gf_xml_dom_node_del(content_node);
					}
					return;
				}
			}
		}
	}
}

#define TTML_NAMESPACE "http://www.w3.org/ns/ttml"

static GF_Err gf_text_ttml_setup(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e;
	u32 i, nb_children, ID;
	u64 file_size;
	GF_XMLAttribute *att;
	GF_XMLNode *root, *node, *body_node;
	const char *lang = ctx->lang;


	ctx->is_setup = GF_TRUE;
	ctx->parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(ctx->parser, ctx->file_name, ttxt_dom_progress, ctx);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Error parsing TTML file: Line %d - %s. Abort.\n", gf_xml_dom_get_line(ctx->parser), gf_xml_dom_get_error(ctx->parser) ));
		ctx->is_setup = GF_TRUE;
		ctx->non_compliant_ttml = GF_TRUE;
		return e;
	}
	root = gf_xml_dom_get_root(ctx->parser);
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Error parsing TTML file: no root XML element found. Abort.\n"));
		ctx->non_compliant_ttml = GF_TRUE;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*look for TTML*/
	if (gf_xml_get_element_check_namespace(root, "tt", NULL) != GF_OK) {
		if (root->ns) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("TTML file not recognized: root element is \"%s:%s\" (check your namespaces)\n", root->ns, root->name));
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("TTML file not recognized: root element is \"%s\"\n", root->name));
		}
		ctx->non_compliant_ttml = GF_TRUE;
		return GF_NOT_SUPPORTED;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[TXTIn] TTML EBU-TTD detected\n"));

	root = gf_xml_dom_get_root(ctx->parser);


	/*** root (including language) ***/
	i=0;
	while ( (att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[TTML] Found root attribute name %s, value %s\n", att->name, att->value));

		if (!strcmp(att->name, "xmlns")) {
			if (strcmp(att->value, TTML_NAMESPACE)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML] XML Namespace %s not recognized, expecting %s\n", att->name, att->value, TTML_NAMESPACE));
				ctx->non_compliant_ttml = GF_TRUE;
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else if (!strcmp(att->name, "xml:lang")) {
			lang = att->value;
		}
	}

	//locate body
	nb_children = gf_list_count(root->content);
	body_node = NULL;

	i=0;
	while ( (node = (GF_XMLNode*)gf_list_enum(root->content, &i))) {
		if (node->type) {
			nb_children--;
			continue;
		}
		e = gf_xml_get_element_check_namespace(node, "body", root->ns);
		if (e == GF_BAD_PARAM) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] ignored \"%s\" node, check your namespaces\n", node->name));
		} else if (e == GF_OK) {
			if (body_node) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] duplicated \"body\" element. Abort.\n"));
				ctx->non_compliant_ttml = GF_TRUE;
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			body_node = node;
		}
	}
	if (!body_node) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] \"body\" element not found. Abort.\n"));
		ctx->non_compliant_ttml = GF_TRUE;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	i=0;
	while ( (node = (GF_XMLNode*)gf_list_enum(body_node->content, &i))) {
		if (node->type) {
			nb_children--;
			continue;
		}
		e = gf_xml_get_element_check_namespace(node, "div", root->ns);
		if (e == GF_BAD_PARAM) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] ignored \"%s\" node, check your namespaces\n", node->name));
		} else if (e == GF_OK) {
			if (ctx->div_node) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] several \"div\" found in document, only the first one will be imported - not supported but patch is welcome\n"));
			}
			ctx->div_node = node;
		}
	}
	ID = 0;
	file_size = ctx->end;
	if (!ctx->timescale) ctx->timescale = 1000;

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SUBS_XML) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(file_size) );

	if (!ID) ID = 1;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(ID) );
	if (ctx->width) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	if (ctx->height) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	if (ctx->zorder) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ZORDER, &PROP_SINT(ctx->zorder) );
	if (lang) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_LANGUAGE, &PROP_STRING( lang) );
	gf_filter_pid_set_property_str(ctx->opid, "meta:xmlns", &PROP_STRING(TTML_NAMESPACE) );

	/*** body ***/
	ctx->parser_working_copy = gf_xml_dom_new();
	e = gf_xml_dom_parse(ctx->parser_working_copy, ctx->file_name, NULL, NULL);
	assert (e == GF_OK);
	ctx->root_working_copy = gf_xml_dom_get_root(ctx->parser_working_copy);
	assert(ctx->root_working_copy);

	/*remove all the sample entries (instances in body) entries from the working copy, we will add each sample in this clone DOM  to create full XML of each sample*/
	GF_TXTIN_MODE_ebu_ttd_remove_samples(ctx->root_working_copy, &ctx->sample_list_node);

	ctx->nb_children = gf_list_count(ctx->div_node->content);
	ctx->cur_child_idx = 0;

	ctx->last_sample_duration = 0;
	ctx->end = 0;
	ctx->first_samp = GF_TRUE;

	txtin_probe_duration(ctx);

	return GF_OK;
}

static GF_Err gf_text_process_ttml(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e;
	GF_XMLNode *root;
	char *samp_text=NULL;

	if (!ctx->is_setup) return gf_text_ttml_setup(filter, ctx);
	if (ctx->non_compliant_ttml || !ctx->opid) return GF_NOT_SUPPORTED;
	if (!ctx->playstate) return GF_OK;
	else if (ctx->playstate==2) return GF_EOS;

	if (ctx->seek_state==1) {
		ctx->seek_state = 2;
		ctx->cur_child_idx = 0;
	}

	root = gf_xml_dom_get_root(ctx->parser);

	for (; ctx->cur_child_idx < ctx->nb_children; ctx->cur_child_idx++) {
		GF_XMLNode *p_node;
		GF_XMLAttribute *p_att;
		u32 p_idx = 0, h, m, s, ms;
		s64 ts_begin = -1, ts_end = -1;

		GF_XMLNode *div_child = (GF_XMLNode*)gf_list_get(ctx->div_node->content, ctx->cur_child_idx);
		if (div_child->type) {
			continue;
		}
		e = gf_xml_get_element_check_namespace(div_child, "p", root->ns);
		if (e != GF_OK) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] ignored \"%s\" node, check your namespaces\n", div_child->name));
			continue;
		}

		//sample is either in the <p> ...
		while ( (p_att = (GF_XMLAttribute*)gf_list_enum(div_child->attributes, &p_idx))) {
			if (!p_att) continue;

			if (!strcmp(p_att->name, "begin")) {
				if (ts_begin != -1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] duplicated \"begin\" attribute. Abort.\n"));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
				if (sscanf(p_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts_begin = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(p_att->value, "%u:%u:%u", &h, &m, &s) == 3) {
					ts_begin = (h*3600 + m*60+s)*1000;
				}
			} else if (!strcmp(p_att->name, "end")) {
				if (ts_end != -1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] duplicated \"end\" attribute. Abort.\n"));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
				if (sscanf(p_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts_end = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(p_att->value, "%u:%u:%u", &h, &m, &s) == 3) {
					ts_end = (h*3600 + m*60+s)*1000;
				}
			}
			if ((ts_begin != -1) && (ts_end != -1) && !samp_text && ctx->sample_list_node) {
				e = gf_xml_dom_append_child(ctx->sample_list_node, div_child);
				assert(e == GF_OK);
				assert(!samp_text);
				samp_text = gf_xml_dom_serialize((GF_XMLNode*)ctx->root_working_copy, GF_FALSE);
				e = gf_xml_dom_rem_child(ctx->sample_list_node, div_child);
				assert(e == GF_OK);
			}
		}

		//or under a <span>
		p_idx = 0;
		while ( (p_node = (GF_XMLNode*)gf_list_enum(div_child->content, &p_idx))) {
			e = gf_xml_get_element_check_namespace(p_node, "span", root->ns);
			if (e == GF_BAD_PARAM) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] ignored \"%s\" node, check your namespaces\n", div_child->name));
			} else if (e == GF_OK) {
				u32 span_idx = 0;
				GF_XMLAttribute *span_att;
				while ( (span_att = (GF_XMLAttribute*)gf_list_enum(p_node->attributes, &span_idx))) {
					if (!span_att) continue;

					if (!strcmp(span_att->name, "begin")) {
						if (ts_begin != -1) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] duplicated \"begin\" attribute under <span>. Abort.\n"));
							e = GF_NON_COMPLIANT_BITSTREAM;
							goto exit;
						}
						if (sscanf(span_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
							ts_begin = (h*3600 + m*60+s)*1000+ms;
						} else if (sscanf(span_att->value, "%u:%u:%u", &h, &m, &s) == 3) {
							ts_begin = (h*3600 + m*60+s)*1000;
						}
					} else if (!strcmp(span_att->name, "end")) {
						if (ts_end != -1) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] duplicated \"end\" attribute under <span>. Abort.\n"));
							e = GF_NON_COMPLIANT_BITSTREAM;
							goto exit;
						}
						if (sscanf(span_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
							ts_end = (h*3600 + m*60+s)*1000+ms;
						} else if (sscanf(span_att->value, "%u:%u:%u", &h, &m, &s) == 3) {
							ts_end = (h*3600 + m*60+s)*1000;
						}
					}
					if ((ts_begin != -1) && (ts_end != -1) && !samp_text && ctx->sample_list_node) {
						if (samp_text) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] duplicated sample text under <span>. Abort.\n"));
							e = GF_NON_COMPLIANT_BITSTREAM;
							goto exit;
						}

						/*append the sample*/
						e = gf_xml_dom_append_child(ctx->sample_list_node, div_child);
						assert(e == GF_OK);
						assert(!samp_text);
						samp_text = gf_xml_dom_serialize((GF_XMLNode*)ctx->root_working_copy, GF_FALSE);
						e = gf_xml_dom_rem_child(ctx->sample_list_node, div_child);
						assert(e == GF_OK);
					}
				}
			}
		}

		if ((ts_begin != -1) && (ts_end != -1) && samp_text) {
			GF_FilterPacket *pck;
			u8 *pck_data;
			Bool skip_pck = GF_FALSE;
			u32 txt_len;
			char *txt_str;

			if (ts_end < ts_begin) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] invalid timings: \"begin\"="LLD" , \"end\"="LLD". Abort.\n", ts_begin, ts_end));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}

			if (ts_begin < (s64) ctx->end) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TTML EBU-TTD] timing overlapping not supported: \"begin\" is "LLD" , last \"end\" was "LLD". Abort.\n", ts_begin, ctx->end));
				e = GF_NOT_SUPPORTED;
				goto exit;
			}

			txt_str = ttxt_parse_string(samp_text, GF_TRUE);
			if (!txt_str) txt_str = "";
			txt_len = (u32) strlen(txt_str);

			if (ctx->first_samp) {
				ts_begin = 0; /*in MP4 we must start at T=0*/
				ctx->last_sample_duration = ts_end;
				ctx->first_samp = GF_FALSE;
			} else {
				ctx->last_sample_duration = ts_end - ts_begin;
			}

			ctx->end = ts_end;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("ts_begin="LLD", ts_end="LLD", last_sample_duration="LLU" (real duration: "LLU"), last_sample_end="LLU"\n", ts_begin, ts_end, ts_end - ctx->end, ctx->last_sample_duration, ctx->end));

			if (ctx->seek_state==2) {
				Double end = (Double) ts_end;
				end /= ctx->timescale;
				if (end<ctx->start_range) skip_pck = GF_TRUE;
				else ctx->seek_state = 0;
			}

			if (!skip_pck) {
				pck = gf_filter_pck_new_alloc(ctx->opid, txt_len, &pck_data);
				memcpy(pck_data, txt_str, txt_len);
				gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
				gf_filter_pck_set_cts(pck, (ctx->timescale*ts_begin)/1000);
				gf_filter_pck_send(pck);
			}

			gf_free(samp_text);
			samp_text = NULL;
			ctx->nb_p_found++;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] incomplete sample (begin="LLD", end="LLD", text=\"%s\"). Skip.\n", ts_begin, ts_end, samp_text ? samp_text : "NULL"));
		}

		if (gf_filter_pid_would_block(ctx->opid)) {
			ctx->cur_child_idx++;
			return GF_OK;
		}
	}

	if (!ctx->nb_p_found) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTML EBU-TTD] \"%s\" div node has no <p> elements.\n", ctx->div_node->name));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[TTML EBU-TTD] last_sample_duration="LLU", last_sample_end="LLU"\n", ctx->last_sample_duration, ctx->end));

	gf_filter_pid_set_info_str( ctx->opid, "ttxt:last_dur", &PROP_UINT((u32) ctx->last_sample_duration) );

	return GF_EOS;


exit:
	ctx->non_compliant_ttml = GF_TRUE;
	return e;
}

#ifndef GPAC_DISABLE_SWF_IMPORT

static GF_Err swf_svg_add_iso_sample(void *user, const u8 *data, u32 length, u64 timestamp, Bool isRap)
{
	GF_FilterPacket *pck;
	u8 *pck_data;
	GF_TXTIn *ctx = (GF_TXTIn *)user;

	if (ctx->seek_state==2) {
		Double ts = (Double) timestamp;
		ts/=1000;
		if (ts<ctx->start_range) return GF_OK;
		ctx->seek_state = 0;
	}

	pck = gf_filter_pck_new_alloc(ctx->opid, length, &pck_data);
	memcpy(pck_data, data, length);
	gf_filter_pck_set_cts(pck, (u64) (ctx->timescale*timestamp/1000) );
	gf_filter_pck_set_sap(pck, isRap ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
	gf_filter_pck_set_framing(pck, GF_TRUE, GF_FALSE);

	gf_filter_pck_send(pck);

	if (gf_filter_pid_would_block(ctx->opid))
		ctx->do_suspend = GF_TRUE;
	return GF_OK;
}

static GF_Err swf_svg_add_iso_header(void *user, const u8 *data, u32 length, Bool isHeader)
{
	GF_TXTIn *ctx = (GF_TXTIn *)user;

	if (isHeader) {
		if (!ctx->hdr_parsed) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((char *)data, (u32) ( strlen(data)+1 ) )  );
			ctx->hdr_parsed = GF_TRUE;
		}
	} else if (!ctx->seek_state) {
		GF_FilterPacket *pck;
		u8 *pck_data;
		pck = gf_filter_pck_new_alloc(ctx->opid, length, &pck_data);
		memcpy(pck_data, data, length);
		gf_filter_pck_set_framing(pck, GF_FALSE, GF_TRUE);

		gf_filter_pck_send(pck);
	}
	return GF_OK;
}

static GF_Err gf_text_swf_setup(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e = GF_OK;
	u32 ID;

	ctx->swf_parse = gf_swf_reader_new(NULL, ctx->file_name);
	e = gf_swf_read_header(ctx->swf_parse);
	if (e) return e;
	gf_swf_reader_set_user_mode(ctx->swf_parse, ctx, swf_svg_add_iso_sample, swf_svg_add_iso_header);

	if (!ctx->timescale) ctx->timescale = 1000;
	ID = 0;

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_SIMPLE_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
//	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, &PROP_UINT(file_size) );

	//patch for old arch
	ctx->width = FIX2INT(ctx->swf_parse->width);
	ctx->height = FIX2INT(ctx->swf_parse->height);
	if (!ctx->width && !ctx->height) {
		ctx->width = 400;
		ctx->height = 60;
	}
	if (!ID) ID = 1;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(ID) );
	if (ctx->width) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	if (ctx->height) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	if (ctx->zorder) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ZORDER, &PROP_SINT(ctx->zorder) );
	if (ctx->lang) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_LANGUAGE, &PROP_STRING( ctx->lang) );

	gf_filter_pid_set_property_str(ctx->opid, "meta:mime", &PROP_STRING("image/svg+xml") );

#ifndef GPAC_DISABLE_SVG
	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] swf -> svg not fully migrated, using SWF flags 0 and no flatten angle. Patch welcome\n"));
	e = swf_to_svg_init(ctx->swf_parse, 0, 0);
#endif

	//SWF->BIFS is handled in ctx loader, no need to define it here
	txtin_probe_duration(ctx);

	return e;
}

static GF_Err gf_text_process_swf(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e=GF_OK;

	if (!ctx->is_setup) {
		ctx->is_setup = GF_TRUE;
		return gf_text_swf_setup(filter, ctx);
	}
	if (!ctx->opid) return GF_NOT_SUPPORTED;

	if (ctx->seek_state==1) {
		ctx->seek_state = 2;
		gf_swf_reader_del(ctx->swf_parse);
		ctx->swf_parse = gf_swf_reader_new(NULL, ctx->file_name);
		gf_swf_read_header(ctx->swf_parse);
		gf_swf_reader_set_user_mode(ctx->swf_parse, ctx, swf_svg_add_iso_sample, swf_svg_add_iso_header);
	}

	ctx->do_suspend = GF_FALSE;
	/*parse all tags*/
	while (e == GF_OK) {
		e = swf_parse_tag(ctx->swf_parse);
		if (ctx->do_suspend) return GF_OK;
	}
	if (e==GF_EOS) {
		if (ctx->swf_parse->finalize) {
			ctx->swf_parse->finalize(ctx->swf_parse);
			ctx->swf_parse->finalize = NULL;
		}
	}
	return e;
}
/* end of SWF Importer */

#else

static GF_Err gf_text_process_swf(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Warning: GPAC was compiled without SWF import support, can't import file.\n"));
	return GF_NOT_SUPPORTED;
}

#endif /*GPAC_DISABLE_SWF_IMPORT*/

static GF_Err gf_text_process_sub(GF_Filter *filter, GF_TXTIn *ctx)
{
	u32 i, j, len, line;
	GF_TextSample *samp;
	Double ts_scale;
	char szLine[2048], szTime[20], szText[2048];

	//same setup as for srt
	if (!ctx->is_setup) {
		ctx->is_setup = GF_TRUE;
		return txtin_setup_srt(filter, ctx);
	}
	if (!ctx->opid) return GF_NOT_SUPPORTED;
	if (!ctx->playstate) return GF_OK;
	else if (ctx->playstate==2) return GF_EOS;

	if (ctx->seek_state==1) {
		ctx->seek_state = 2;
		gf_fseek(ctx->src, 0, SEEK_SET);
	}

	if (ctx->fps.den && ctx->fps.num) {
		ts_scale = ((Double) ctx->fps.num) / ctx->fps.den;
	} else {
		ts_scale = 25;
	}

	line = 0;

	while (1) {
		char *sOK = gf_text_get_utf8_line(szLine, 2048, ctx->src, ctx->unicode_type);
		if (!sOK) break;

		REM_TRAIL_MARKS(szLine, "\r\n\t ")

		line++;
		len = (u32) strlen(szLine);
		if (!len) continue;

		i=0;
		if (szLine[i] != '{') {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Bad SUB file (line %d): expecting \"{\" got \"%c\"\n", line, szLine[i]));
			continue;
		}
		while (szLine[i+1] && szLine[i+1]!='}') {
			szTime[i] = szLine[i+1];
			i++;
		}
		szTime[i] = 0;
		ctx->start = atoi(szTime);
		if (ctx->start < ctx->end) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] corrupted SUB frame (line %d) - starts (at %d ms) before end of previous one (%d ms) - adjusting time stamps\n", line, ctx->start, ctx->end));
			ctx->start = ctx->end;
		}
		j=i+2;
		i=0;
		if (szLine[i+j] != '{') {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Bad SUB file - expecting \"{\" got \"%c\"\n", szLine[i]));
			continue;
		}
		while (szLine[i+1+j] && szLine[i+1+j]!='}') {
			szTime[i] = szLine[i+1+j];
			i++;
		}
		szTime[i] = 0;
		ctx->end = atoi(szTime);
		j+=i+2;

		if (ctx->start > ctx->end) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] corrupted SUB frame (line %d) - ends (at %d ms) before start of current frame (%d ms) - skipping\n", line, ctx->end, ctx->start));
			continue;
		}

		if (ctx->start && ctx->first_samp) {
			samp = gf_isom_new_text_sample();
			txtin_process_send_text_sample(ctx, samp, 0, (u32) (ts_scale*ctx->start), GF_TRUE);
			ctx->first_samp = GF_FALSE;
			gf_isom_delete_text_sample(samp);
		}

		for (i=j; i<len; i++) {
			if (szLine[i]=='|') {
				szText[i-j] = '\n';
			} else {
				szText[i-j] = szLine[i];
			}
		}
		szText[i-j] = 0;

		if (ctx->prev_end) {
			samp = gf_isom_new_text_sample();
			txtin_process_send_text_sample(ctx, samp, (u64) (ts_scale*(s64)ctx->prev_end), (u32) (ts_scale*(ctx->prev_end - ctx->start)), GF_TRUE);
			gf_isom_delete_text_sample(samp);
		}

		samp = gf_isom_new_text_sample();
		gf_isom_text_add_text(samp, szText, (u32) strlen(szText) );
		txtin_process_send_text_sample(ctx, samp, (u64) (ts_scale*(s64)ctx->start), (u32) (ts_scale*(ctx->end - ctx->start)), GF_TRUE);
		gf_isom_delete_text_sample(samp);

		ctx->prev_end = ctx->end;

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DOWN_BYTES, &PROP_LONGUINT( gf_ftell(ctx->src )) );

		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
	}
	/*final flush*/
	if (ctx->end && !ctx->noflush) {
		samp = gf_isom_new_text_sample();
		txtin_process_send_text_sample(ctx, samp, (u64) (ts_scale*(s64)ctx->end), 0, GF_TRUE);
		gf_isom_delete_text_sample(samp);
	}

	gf_filter_pid_set_info_str( ctx->opid, "ttxt:last_dur", &PROP_UINT(0) );

	return GF_EOS;
}


#define CHECK_STR(__str)	\
	if (!__str) { \
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid XML formatting (line %d)", parser.line);	\
		goto exit;	\
	}	\
 

static u32 ttxt_get_color(char *val)
{
	u32 r, g, b, a, res;
	r = g = b = a = 0;
	if (sscanf(val, "%x %x %x %x", &r, &g, &b, &a) != 4) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TXTIn] Warning: color badly formatted %s\n", val));
	}
	res = (a&0xFF);
	res<<=8;
	res |= (r&0xFF);
	res<<=8;
	res |= (g&0xFF);
	res<<=8;
	res |= (b&0xFF);
	return res;
}

static void ttxt_parse_text_box(GF_XMLNode *n, GF_BoxRecord *box)
{
	u32 i=0;
	GF_XMLAttribute *att;
	memset(box, 0, sizeof(GF_BoxRecord));
	while ( (att=(GF_XMLAttribute *)gf_list_enum(n->attributes, &i))) {
		if (!stricmp(att->name, "top")) box->top = atoi(att->value);
		else if (!stricmp(att->name, "bottom")) box->bottom = atoi(att->value);
		else if (!stricmp(att->name, "left")) box->left = atoi(att->value);
		else if (!stricmp(att->name, "right")) box->right = atoi(att->value);
	}
}

static void ttxt_parse_text_style(GF_TXTIn *ctx, GF_XMLNode *n, GF_StyleRecord *style)
{
	u32 i=0;
	GF_XMLAttribute *att;
	memset(style, 0, sizeof(GF_StyleRecord));
	style->fontID = 1;
	style->font_size = ctx->fontsize ;
	style->text_color = 0xFFFFFFFF;

	while ( (att=(GF_XMLAttribute *)gf_list_enum(n->attributes, &i))) {
		if (!stricmp(att->name, "fromChar")) style->startCharOffset = atoi(att->value);
		else if (!stricmp(att->name, "toChar")) style->endCharOffset = atoi(att->value);
		else if (!stricmp(att->name, "fontID")) style->fontID = atoi(att->value);
		else if (!stricmp(att->name, "fontSize")) style->font_size = atoi(att->value);
		else if (!stricmp(att->name, "color")) style->text_color = ttxt_get_color(att->value);
		else if (!stricmp(att->name, "styles")) {
			if (strstr(att->value, "Bold")) style->style_flags |= GF_TXT_STYLE_BOLD;
			if (strstr(att->value, "Italic")) style->style_flags |= GF_TXT_STYLE_ITALIC;
			if (strstr(att->value, "Underlined")) style->style_flags |= GF_TXT_STYLE_UNDERLINED;
		}
	}
}

static GF_Err txtin_setup_ttxt(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e;
	u32 j, k, ID, OCR_ES_ID;
	u64 file_size;
	GF_XMLAttribute *att;
	GF_XMLNode *root, *node, *ext;
	GF_PropertyValue *dcd;

	ctx->parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(ctx->parser, ctx->file_name, ttxt_dom_progress, ctx);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Error parsing TTXT file: Line %d - %s\n", gf_xml_dom_get_line(ctx->parser), gf_xml_dom_get_error(ctx->parser)));
		return e;
	}
	root = gf_xml_dom_get_root(ctx->parser);

	if (strcmp(root->name, "TextStream")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Invalid Timed Text file - expecting \"TextStream\" got %s", root->name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	file_size = ctx->end;
	ctx->end = 0;

	/*setup track in 3GP format directly (no ES desc)*/
	if (!ctx->timescale) ctx->timescale = 1000;
	OCR_ES_ID = ID = 0;

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_TX3G) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(file_size) );

	if (!ID) ID = 1;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(ID) );
	if (OCR_ES_ID) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(OCR_ES_ID) );

	ctx->nb_children = gf_list_count(root->content);

	ctx->cur_child_idx = 0;
	for (ctx->cur_child_idx=0; ctx->cur_child_idx < ctx->nb_children; ctx->cur_child_idx++) {
		node = (GF_XMLNode*) gf_list_get(root->content, ctx->cur_child_idx);

		if (node->type) {
			continue;
		}

		if (!strcmp(node->name, "TextStreamHeader")) {
			GF_XMLNode *sdesc;
			s32 w, h, tx, ty, layer;
			u32 tref_id;
			w = ctx->width;
			h = ctx->height;
			tx = ctx->x;
			ty = ctx->y;
			layer = ctx->zorder;
			tref_id = 0;

			j=0;
			while ( (att=(GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
				if (!strcmp(att->name, "width")) w = atoi(att->value);
				else if (!strcmp(att->name, "height")) h = atoi(att->value);
				else if (!strcmp(att->name, "layer")) layer = atoi(att->value);
				else if (!strcmp(att->name, "translation_x")) tx = atoi(att->value);
				else if (!strcmp(att->name, "translation_y")) ty = atoi(att->value);
				else if (!strcmp(att->name, "trefID")) tref_id = atoi(att->value);
			}

			if (tref_id) {
				gf_filter_pid_set_property_str(ctx->opid, "tref:chap", &PROP_UINT(tref_id) );
			}

			if (w) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
			if (h) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
			if (tx) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TRANS_X, &PROP_UINT(tx) );
			if (ty) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TRANS_X, &PROP_UINT(ty) );
			if (layer) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ZORDER, &PROP_SINT(ctx->zorder) );
			if (ctx->lang) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_LANGUAGE, &PROP_STRING( ctx->lang) );

			j=0;
			while ( (sdesc=(GF_XMLNode*)gf_list_enum(node->content, &j))) {
				if (sdesc->type) continue;

				if (!strcmp(sdesc->name, "TextSampleDescription")) {
					GF_TextSampleDescriptor td;
					memset(&td, 0, sizeof(GF_TextSampleDescriptor));
					td.tag = GF_ODF_TEXT_CFG_TAG;
					td.vert_justif = (s8) -1;
					td.default_style.fontID = 1;
					td.default_style.font_size = ctx->fontsize;

					k=0;
					while ( (att=(GF_XMLAttribute *)gf_list_enum(sdesc->attributes, &k))) {
						if (!strcmp(att->name, "horizontalJustification")) {
							if (!stricmp(att->value, "center")) td.horiz_justif = 1;
							else if (!stricmp(att->value, "right")) td.horiz_justif = (s8) -1;
							else if (!stricmp(att->value, "left")) td.horiz_justif = 0;
						}
						else if (!strcmp(att->name, "verticalJustification")) {
							if (!stricmp(att->value, "center")) td.vert_justif = 1;
							else if (!stricmp(att->value, "bottom")) td.vert_justif = (s8) -1;
							else if (!stricmp(att->value, "top")) td.vert_justif = 0;
						}
						else if (!strcmp(att->name, "backColor")) td.back_color = ttxt_get_color(att->value);
						else if (!strcmp(att->name, "verticalText") && !stricmp(att->value, "yes") ) td.displayFlags |= GF_TXT_VERTICAL;
						else if (!strcmp(att->name, "fillTextRegion") && !stricmp(att->value, "yes") ) td.displayFlags |= GF_TXT_FILL_REGION;
						else if (!strcmp(att->name, "continuousKaraoke") && !stricmp(att->value, "yes") ) td.displayFlags |= GF_TXT_KARAOKE;
						else if (!strcmp(att->name, "scroll")) {
							if (!stricmp(att->value, "inout")) td.displayFlags |= GF_TXT_SCROLL_IN | GF_TXT_SCROLL_OUT;
							else if (!stricmp(att->value, "in")) td.displayFlags |= GF_TXT_SCROLL_IN;
							else if (!stricmp(att->value, "out")) td.displayFlags |= GF_TXT_SCROLL_OUT;
						}
						else if (!strcmp(att->name, "scrollMode")) {
							u32 scroll_mode = GF_TXT_SCROLL_CREDITS;
							if (!stricmp(att->value, "Credits")) scroll_mode = GF_TXT_SCROLL_CREDITS;
							else if (!stricmp(att->value, "Marquee")) scroll_mode = GF_TXT_SCROLL_MARQUEE;
							else if (!stricmp(att->value, "Right")) scroll_mode = GF_TXT_SCROLL_RIGHT;
							else if (!stricmp(att->value, "Down")) scroll_mode = GF_TXT_SCROLL_DOWN;
							td.displayFlags |= ((scroll_mode<<7) & GF_TXT_SCROLL_DIRECTION);
						}
					}

					k=0;
					while ( (ext=(GF_XMLNode*)gf_list_enum(sdesc->content, &k))) {
						if (ext->type) continue;
						if (!strcmp(ext->name, "TextBox")) ttxt_parse_text_box(ext, &td.default_pos);
						else if (!strcmp(ext->name, "Style")) ttxt_parse_text_style(ctx, ext, &td.default_style);
						else if (!strcmp(ext->name, "FontTable")) {
							GF_XMLNode *ftable;
							u32 z=0;
							while ( (ftable=(GF_XMLNode*)gf_list_enum(ext->content, &z))) {
								u32 m;
								if (ftable->type || strcmp(ftable->name, "FontTableEntry")) continue;
								td.font_count += 1;
								td.fonts = (GF_FontRecord*)gf_realloc(td.fonts, sizeof(GF_FontRecord)*td.font_count);
								m=0;
								while ( (att=(GF_XMLAttribute *)gf_list_enum(ftable->attributes, &m))) {
									if (!stricmp(att->name, "fontID")) td.fonts[td.font_count-1].fontID = atoi(att->value);
									else if (!stricmp(att->name, "fontName")) td.fonts[td.font_count-1].fontName = gf_strdup(att->value);
								}
							}
						}
					}
					if (ctx->nodefbox) {
						td.default_pos.top = td.default_pos.left = td.default_pos.right = td.default_pos.bottom = 0;
					} else {
						if ((td.default_pos.bottom==td.default_pos.top) || (td.default_pos.right==td.default_pos.left)) {
							td.default_pos.top = td.default_pos.left = 0;
							td.default_pos.right = w;
							td.default_pos.bottom = h;
						}
					}
					if (!td.fonts) {
						td.font_count = 1;
						td.fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
						td.fonts[0].fontID = 1;
						td.fonts[0].fontName = gf_strdup("Serif");
					}
					GF_SAFEALLOC(dcd, GF_PropertyValue);
					dcd->type = GF_PROP_DATA;

					gf_odf_tx3g_write(&td, &dcd->value.data.ptr, &dcd->value.data.size);
					if (!ctx->text_descs) ctx->text_descs = gf_list_new();
					gf_list_add(ctx->text_descs, dcd);

					for (k=0; k<td.font_count; k++) gf_free(td.fonts[k].fontName);
					gf_free(td.fonts);
				}
			}
		}
		else {
			break;
		}
	}

	if (!ctx->text_descs) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Invalid Timed Text file - text stream header not found or empty\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	dcd = gf_list_get(ctx->text_descs, 0);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, dcd);
	ctx->last_desc_idx = 1;

	ctx->first_samp = GF_TRUE;
	ctx->last_sample_empty = GF_FALSE;
	ctx->last_sample_duration = 0;

	txtin_probe_duration(ctx);

	return GF_OK;
}

static GF_Err txtin_process_ttxt(GF_Filter *filter, GF_TXTIn *ctx)
{
	u32 j, k;
	GF_XMLAttribute *att;
	GF_XMLNode *root, *node, *ext;

	if (!ctx->is_setup) {
		ctx->is_setup = GF_TRUE;
		return txtin_setup_ttxt(filter, ctx);
	}
	if (!ctx->opid) return GF_NON_COMPLIANT_BITSTREAM;
	if (!ctx->playstate) return GF_OK;
	else if (ctx->playstate==2) return GF_EOS;

	if (ctx->seek_state==1) {
		ctx->seek_state = 2;
		ctx->cur_child_idx = 0;
	}
	root = gf_xml_dom_get_root(ctx->parser);

	for (; ctx->cur_child_idx < ctx->nb_children; ctx->cur_child_idx++) {
		GF_TextSample * samp;
		u32 ts, descIndex;
		Bool has_text = GF_FALSE;

		node = (GF_XMLNode*) gf_list_get(root->content, ctx->cur_child_idx);

		if (node->type) {
			continue;
		}
		/*sample text*/
		else if (strcmp(node->name, "TextSample")) continue;

		samp = gf_isom_new_text_sample();
		ts = 0;
		descIndex = 1;
		ctx->last_sample_empty = GF_TRUE;

		j=0;
		while ( (att=(GF_XMLAttribute*)gf_list_enum(node->attributes, &j))) {
			if (!strcmp(att->name, "sampleTime")) {
				u32 h, m, s, ms;
				if (sscanf(att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60 + s)*1000 + ms;
				} else {
					ts = (u32) (atof(att->value) * 1000);
				}
			}
			else if (!strcmp(att->name, "sampleDescriptionIndex")) descIndex = atoi(att->value);
			else if (!strcmp(att->name, "text")) {
				u32 len;
				char *str = ttxt_parse_string(att->value, GF_TRUE);
				len = (u32) strlen(str);
				gf_isom_text_add_text(samp, str, len);
				ctx->last_sample_empty = len ? GF_FALSE : GF_TRUE;
				has_text = GF_TRUE;
			}
			else if (!strcmp(att->name, "scrollDelay")) gf_isom_text_set_scroll_delay(samp, (u32) (1000*atoi(att->value)));
			else if (!strcmp(att->name, "highlightColor")) gf_isom_text_set_highlight_color_argb(samp, ttxt_get_color(att->value));
			else if (!strcmp(att->name, "wrap") && !strcmp(att->value, "Automatic")) gf_isom_text_set_wrap(samp, 0x01);
		}

		/*get all modifiers*/
		j=0;
		while ( (ext=(GF_XMLNode*)gf_list_enum(node->content, &j))) {
			if (!has_text && (ext->type==GF_XML_TEXT_TYPE)) {
				u32 len;
				char *str = ttxt_parse_string(ext->name, GF_FALSE);
				len = (u32) strlen(str);
				gf_isom_text_add_text(samp, str, len);
				ctx->last_sample_empty = len ? GF_FALSE : GF_TRUE;
				has_text = GF_TRUE;
			}
			if (ext->type) continue;

			if (!stricmp(ext->name, "Style")) {
				GF_StyleRecord r;
				ttxt_parse_text_style(ctx, ext, &r);
				gf_isom_text_add_style(samp, &r);
			}
			else if (!stricmp(ext->name, "TextBox")) {
				GF_BoxRecord r;
				ttxt_parse_text_box(ext, &r);
				gf_isom_text_set_box(samp, r.top, r.left, r.bottom, r.right);
			}
			else if (!stricmp(ext->name, "Highlight")) {
				u16 start, end;
				start = end = 0;
				k=0;
				while ( (att=(GF_XMLAttribute *)gf_list_enum(ext->attributes, &k))) {
					if (!strcmp(att->name, "fromChar")) start = atoi(att->value);
					else if (!strcmp(att->name, "toChar")) end = atoi(att->value);
				}
				gf_isom_text_add_highlight(samp, start, end);
			}
			else if (!stricmp(ext->name, "Blinking")) {
				u16 start, end;
				start = end = 0;
				k=0;
				while ( (att=(GF_XMLAttribute *)gf_list_enum(ext->attributes, &k))) {
					if (!strcmp(att->name, "fromChar")) start = atoi(att->value);
					else if (!strcmp(att->name, "toChar")) end = atoi(att->value);
				}
				gf_isom_text_add_blink(samp, start, end);
			}
			else if (!stricmp(ext->name, "HyperLink")) {
				u16 start, end;
				char *url, *url_tt;
				start = end = 0;
				url = url_tt = NULL;
				k=0;
				while ( (att=(GF_XMLAttribute *)gf_list_enum(ext->attributes, &k))) {
					if (!strcmp(att->name, "fromChar")) start = atoi(att->value);
					else if (!strcmp(att->name, "toChar")) end = atoi(att->value);
					else if (!strcmp(att->name, "URL")) url = gf_strdup(att->value);
					else if (!strcmp(att->name, "URLToolTip")) url_tt = gf_strdup(att->value);
				}
				gf_isom_text_add_hyperlink(samp, url, url_tt, start, end);
				if (url) gf_free(url);
				if (url_tt) gf_free(url_tt);
			}
			else if (!stricmp(ext->name, "Karaoke")) {
				u32 startTime;
				GF_XMLNode *krok;
				startTime = 0;
				k=0;
				while ( (att=(GF_XMLAttribute *)gf_list_enum(ext->attributes, &k))) {
					if (!strcmp(att->name, "startTime")) startTime = (u32) (1000*atof(att->value));
				}
				gf_isom_text_add_karaoke(samp, startTime);
				k=0;
				while ( (krok=(GF_XMLNode*)gf_list_enum(ext->content, &k))) {
					u16 start, end;
					u32 endTime, m;
					if (krok->type) continue;
					if (strcmp(krok->name, "KaraokeRange")) continue;
					start = end = 0;
					endTime = 0;
					m=0;
					while ( (att=(GF_XMLAttribute *)gf_list_enum(krok->attributes, &m))) {
						if (!strcmp(att->name, "fromChar")) start = atoi(att->value);
						else if (!strcmp(att->name, "toChar")) end = atoi(att->value);
						else if (!strcmp(att->name, "endTime")) endTime = (u32) (1000*atof(att->value));
					}
					gf_isom_text_set_karaoke_segment(samp, endTime, start, end);
				}
			}
		}

		if (!descIndex) descIndex = 1;
		if (descIndex != ctx->last_desc_idx) {
			GF_PropertyValue *dcd;
			ctx->last_desc_idx = descIndex;
			dcd = gf_list_get(ctx->text_descs, descIndex-1);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, dcd);
		}

		/*in MP4 we must start at T=0, so add an empty sample*/
		if (ts && ctx->first_samp) {
			GF_TextSample * firstsamp = gf_isom_new_text_sample();
			txtin_process_send_text_sample(ctx, firstsamp, 0, 0, GF_TRUE);
			gf_isom_delete_text_sample(firstsamp);
		}
		ctx->first_samp = GF_FALSE;

		txtin_process_send_text_sample(ctx, samp, ts, 0, GF_TRUE);

		gf_isom_delete_text_sample(samp);

		if (ctx->last_sample_empty) {
			ctx->last_sample_duration = ts - ctx->last_sample_duration;
		} else {
			ctx->last_sample_duration = ts;
		}

		if (gf_filter_pid_would_block(ctx->opid)) {
			ctx->cur_child_idx++;
			return GF_OK;
		}
	}

	if (ctx->last_sample_empty) {
		//this is a bit ugly, in regular streaming mode we don't want to remove empty samples
		//howvere the last one can be removed, adjusting the duration of the previous one.
		//doing this here is problematic if the loader is sent a new ttxt file, we would have a cue termination sample
		//we therefore share that info through pid, and let the final user (muxer& co) decide what to do
		gf_filter_pid_set_info_str( ctx->opid, "ttxt:rem_last", &PROP_BOOL(GF_TRUE) );
		gf_filter_pid_set_info_str( ctx->opid, "ttxt:last_dur", &PROP_UINT((u32) ctx->last_sample_duration) );
	}

	return GF_EOS;
}


static u32 tx3g_get_color(char *value)
{
	u32 r, g, b, a;
	u32 res, v;
	r = g = b = a = 0;
	if (sscanf(value, "%u%%, %u%%, %u%%, %u%%", &r, &g, &b, &a) != 4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("Warning: color badly formatted\n"));
	}
	v = (u32) (a*255/100);
	res = (v&0xFF);
	res<<=8;
	v = (u32) (r*255/100);
	res |= (v&0xFF);
	res<<=8;
	v = (u32) (g*255/100);
	res |= (v&0xFF);
	res<<=8;
	v = (u32) (b*255/100);
	res |= (v&0xFF);
	return res;
}

static void tx3g_parse_text_box(GF_XMLNode *n, GF_BoxRecord *box)
{
	u32 i=0;
	GF_XMLAttribute *att;
	memset(box, 0, sizeof(GF_BoxRecord));
	while ((att=(GF_XMLAttribute *)gf_list_enum(n->attributes, &i))) {
		if (!stricmp(att->name, "x")) box->left = atoi(att->value);
		else if (!stricmp(att->name, "y")) box->top = atoi(att->value);
		else if (!stricmp(att->name, "height")) box->bottom = atoi(att->value);
		else if (!stricmp(att->name, "width")) box->right = atoi(att->value);
	}
}

typedef struct
{
	u32 id;
	u32 pos;
} Marker;

#define GET_MARKER_POS(_val, __isend) \
	{	\
		u32 i, __m = atoi(att->value);	\
		_val = 0;	\
		for (i=0; i<nb_marks; i++) { if (__m==marks[i].id) { _val = marks[i].pos; /*if (__isend) _val--; */break; } }	 \
	}


static GF_Err txtin_texml_setup(GF_Filter *filter, GF_TXTIn *ctx)
{
	GF_Err e;
	u32 ID, OCR_ES_ID, i;
	u64 file_size;
	GF_XMLAttribute *att;
	GF_XMLNode *root;

	ctx->parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(ctx->parser, ctx->file_name, ttxt_dom_progress, ctx);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Error parsing TeXML file: Line %d - %s", gf_xml_dom_get_line(ctx->parser), gf_xml_dom_get_error(ctx->parser) ));
		gf_xml_dom_del(ctx->parser);
		ctx->parser = NULL;
		return e;
	}

	root = gf_xml_dom_get_root(ctx->parser);

	if (strcmp(root->name, "text3GTrack")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTIn] Invalid QT TeXML file - expecting root \"text3GTrack\" got \"%s\"", root->name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	file_size = ctx->end;
	ctx->txml_timescale = 600;

	i=0;
	while ( (att=(GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		if (!strcmp(att->name, "trackWidth")) ctx->width = atoi(att->value);
		else if (!strcmp(att->name, "trackHeight")) ctx->height = atoi(att->value);
		else if (!strcmp(att->name, "layer")) ctx->zorder = atoi(att->value);
		else if (!strcmp(att->name, "timeScale")) ctx->txml_timescale = atoi(att->value);
		else if (!strcmp(att->name, "transform")) {
			Float fx, fy;
			sscanf(att->value, "translate(%f,%f)", &fx, &fy);
			ctx->x = (u32) fx;
			ctx->y = (u32) fy;
		}
	}

	/*setup track in 3GP format directly (no ES desc)*/
	OCR_ES_ID = ID = 0;
	if (!ctx->timescale) ctx->timescale = 1000;

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_ISOM_SUBTYPE_TX3G) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, &PROP_LONGUINT(file_size) );


	if (!ID) ID = 1;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(ID) );
	if (OCR_ES_ID) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(OCR_ES_ID) );
	if (ctx->width) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	if (ctx->height) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	if (ctx->zorder) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ZORDER, &PROP_SINT(ctx->zorder) );
	if (ctx->lang) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_LANGUAGE, &PROP_STRING( ctx->lang) );


	ctx->nb_children = gf_list_count(root->content);
	ctx->cur_child_idx = 0;
	txtin_probe_duration(ctx);

	return GF_OK;
}

static GF_Err txtin_process_texml(GF_Filter *filter, GF_TXTIn *ctx)
{
	u32 j, k;
	GF_StyleRecord styles[50];
	Marker marks[50];
	GF_XMLAttribute *att;
	GF_XMLNode *root;
	Bool probe_first_desc_only = GF_FALSE;

	if (!ctx->is_setup) {
		GF_Err e;

		ctx->is_setup = GF_TRUE;
		e = txtin_texml_setup(filter, ctx);
		if (e) return e;
		probe_first_desc_only = GF_TRUE;
	}
	if (!ctx->opid) return GF_NON_COMPLIANT_BITSTREAM;
	if (!ctx->playstate && !probe_first_desc_only) return GF_OK;
	else if (ctx->playstate==2) return GF_EOS;

	if (ctx->seek_state==1) {
		ctx->seek_state = 2;
		ctx->cur_child_idx = 0;
		ctx->start = 0;
	}

	root = gf_xml_dom_get_root(ctx->parser);

	for (; ctx->cur_child_idx < ctx->nb_children; ctx->cur_child_idx++) {
		GF_XMLNode *node, *desc;
		GF_TextSampleDescriptor td;
		GF_TextSample * samp = NULL;
		u64 duration;
		u32 nb_styles, nb_marks;
		Bool isRAP, same_style, same_box;

		if (probe_first_desc_only && ctx->text_descs && gf_list_count(ctx->text_descs))
			return GF_OK;

		node = (GF_XMLNode*)gf_list_get(root->content, ctx->cur_child_idx);
		if (node->type) continue;
		if (strcmp(node->name, "sample")) continue;

		isRAP = GF_TRUE;
		duration = 1000;
		j=0;
		while ((att=(GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!strcmp(att->name, "duration")) duration = atoi(att->value);
			else if (!strcmp(att->name, "keyframe")) isRAP = (!stricmp(att->value, "true") ? GF_TRUE : GF_FALSE);
		}
		nb_styles = 0;
		nb_marks = 0;
		same_style = same_box = GF_FALSE;
		j=0;
		while ((desc=(GF_XMLNode*)gf_list_enum(node->content, &j))) {
			if (desc->type) continue;

			if (!strcmp(desc->name, "description")) {
				u8 *dsi;
				u32 dsi_len, k, stsd_idx;
				GF_XMLNode *sub;
				memset(&td, 0, sizeof(GF_TextSampleDescriptor));
				td.tag = GF_ODF_TEXT_CFG_TAG;
				td.vert_justif = (s8) -1;
				td.default_style.fontID = 1;
				td.default_style.font_size = ctx->fontsize;

				k=0;
				while ((att=(GF_XMLAttribute *)gf_list_enum(desc->attributes, &k))) {
					if (!strcmp(att->name, "horizontalJustification")) {
						if (!stricmp(att->value, "center")) td.horiz_justif = 1;
						else if (!stricmp(att->value, "right")) td.horiz_justif = (s8) -1;
						else if (!stricmp(att->value, "left")) td.horiz_justif = 0;
					}
					else if (!strcmp(att->name, "verticalJustification")) {
						if (!stricmp(att->value, "center")) td.vert_justif = 1;
						else if (!stricmp(att->value, "bottom")) td.vert_justif = (s8) -1;
						else if (!stricmp(att->value, "top")) td.vert_justif = 0;
					}
					else if (!strcmp(att->name, "backgroundColor")) td.back_color = tx3g_get_color(att->value);
					else if (!strcmp(att->name, "displayFlags")) {
						Bool rev_scroll = GF_FALSE;
						if (strstr(att->value, "scroll")) {
							u32 scroll_mode = 0;
							if (strstr(att->value, "scrollIn")) td.displayFlags |= GF_TXT_SCROLL_IN;
							if (strstr(att->value, "scrollOut")) td.displayFlags |= GF_TXT_SCROLL_OUT;
							if (strstr(att->value, "reverse")) rev_scroll = GF_TRUE;
							if (strstr(att->value, "horizontal")) scroll_mode = rev_scroll ? GF_TXT_SCROLL_RIGHT : GF_TXT_SCROLL_MARQUEE;
							else scroll_mode = (rev_scroll ? GF_TXT_SCROLL_DOWN : GF_TXT_SCROLL_CREDITS);
							td.displayFlags |= (scroll_mode<<7) & GF_TXT_SCROLL_DIRECTION;
						}
						/*TODO FIXME: check in QT doc !!*/
						if (strstr(att->value, "writeTextVertically")) td.displayFlags |= GF_TXT_VERTICAL;
						if (!strcmp(att->name, "continuousKaraoke")) td.displayFlags |= GF_TXT_KARAOKE;
					}
				}

				k=0;
				while ((sub=(GF_XMLNode*)gf_list_enum(desc->content, &k))) {
					if (sub->type) continue;
					if (!strcmp(sub->name, "defaultTextBox")) tx3g_parse_text_box(sub, &td.default_pos);
					else if (!strcmp(sub->name, "fontTable")) {
						GF_XMLNode *ftable;
						u32 m=0;
						while ((ftable=(GF_XMLNode*)gf_list_enum(sub->content, &m))) {
							if (ftable->type) continue;
							if (!strcmp(ftable->name, "font")) {
								u32 n=0;
								td.font_count += 1;
								td.fonts = (GF_FontRecord*)gf_realloc(td.fonts, sizeof(GF_FontRecord)*td.font_count);
								while ((att=(GF_XMLAttribute *)gf_list_enum(ftable->attributes, &n))) {
									if (!stricmp(att->name, "id")) td.fonts[td.font_count-1].fontID = atoi(att->value);
									else if (!stricmp(att->name, "name")) td.fonts[td.font_count-1].fontName = gf_strdup(att->value);
								}
							}
						}
					}
					else if (!strcmp(sub->name, "sharedStyles")) {
						GF_XMLNode *style, *ftable;
						u32 m=0;
						while ((style=(GF_XMLNode*)gf_list_enum(sub->content, &m))) {
							if (style->type) continue;
							if (!strcmp(style->name, "style")) break;
						}
						if (style) {
							char *cur;
							s32 start=0;
							char css_style[1024], css_val[1024];
							memset(&styles[nb_styles], 0, sizeof(GF_StyleRecord));
							m=0;
							while ( (att=(GF_XMLAttribute *)gf_list_enum(style->attributes, &m))) {
								if (!strcmp(att->name, "id")) styles[nb_styles].startCharOffset = atoi(att->value);
							}
							m=0;
							while ( (ftable=(GF_XMLNode*)gf_list_enum(style->content, &m))) {
								if (ftable->type) break;
							}
							cur = ftable->name;
							while (cur) {
								start = gf_token_get_strip(cur, 0, "{:", " ", css_style, 1024);
								if (start <0) break;
								start = gf_token_get_strip(cur, start, ":}", " ", css_val, 1024);
								if (start <0) break;
								cur = strchr(cur+start, '{');

								if (!strcmp(css_style, "font-table")) {
									u32 z;
									styles[nb_styles].fontID = atoi(css_val);
									for (z=0; z<td.font_count; z++) {
										if (td.fonts[z].fontID == styles[nb_styles].fontID)
											break;
									}
								}
								else if (!strcmp(css_style, "font-size")) styles[nb_styles].font_size = atoi(css_val);
								else if (!strcmp(css_style, "font-style") && !strcmp(css_val, "italic")) styles[nb_styles].style_flags |= GF_TXT_STYLE_ITALIC;
								else if (!strcmp(css_style, "font-weight") && !strcmp(css_val, "bold")) styles[nb_styles].style_flags |= GF_TXT_STYLE_BOLD;
								else if (!strcmp(css_style, "text-decoration") && !strcmp(css_val, "underline")) styles[nb_styles].style_flags |= GF_TXT_STYLE_UNDERLINED;
								else if (!strcmp(css_style, "color")) styles[nb_styles].text_color = tx3g_get_color(css_val);
							}
							if (!nb_styles) td.default_style = styles[0];
							nb_styles++;
						}
					}

				}
				if ((td.default_pos.bottom==td.default_pos.top) || (td.default_pos.right==td.default_pos.left)) {
					td.default_pos.top = ctx->y;
					td.default_pos.left = ctx->x;
					td.default_pos.right = ctx->width;
					td.default_pos.bottom = ctx->height;
				}
				if (!td.fonts) {
					td.font_count = 1;
					td.fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
					td.fonts[0].fontID = 1;
					td.fonts[0].fontName = gf_strdup( ctx->fontname ? ctx->fontname : "Serif");
				}

				gf_odf_tx3g_write(&td, &dsi, &dsi_len);
				stsd_idx = 0;
				for (k=0; ctx->text_descs && k<gf_list_count(ctx->text_descs); k++) {
					GF_PropertyValue *d = gf_list_get(ctx->text_descs, k);
					if (d->value.data.size != dsi_len) continue;
					if (! memcmp(d->value.data.ptr, dsi, dsi_len)) {
						stsd_idx = k+1;
						break;
					}
				}
				if (stsd_idx) {
					gf_free(dsi);
				} else {
					GF_PropertyValue *d;
					GF_SAFEALLOC(d, GF_PropertyValue);
					d->type = GF_PROP_DATA;
					d->value.data.ptr = dsi;
					d->value.data.size = dsi_len;
					if (!ctx->text_descs) ctx->text_descs = gf_list_new();
					gf_list_add(ctx->text_descs, d);
					stsd_idx = gf_list_count(ctx->text_descs);
				}
				if (stsd_idx != ctx->last_desc_idx) {
					ctx->last_desc_idx = stsd_idx;
					GF_PropertyValue *d = gf_list_get(ctx->text_descs, stsd_idx-1);
					gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, d);
				}

				for (k=0; k<td.font_count; k++) gf_free(td.fonts[k].fontName);
				gf_free(td.fonts);

				if (probe_first_desc_only)
					return GF_OK;
			}
			else if (!strcmp(desc->name, "sampleData")) {
				GF_XMLNode *sub;
				u16 start, end;
				u32 styleID;
				u32 nb_chars, txt_len, m;
				nb_chars = 0;

				samp = gf_isom_new_text_sample();

				k=0;
				while ((att=(GF_XMLAttribute *)gf_list_enum(desc->attributes, &k))) {
					if (!strcmp(att->name, "targetEncoding") && !strcmp(att->value, "utf16")) ;//is_utf16 = 1;
					else if (!strcmp(att->name, "scrollDelay")) gf_isom_text_set_scroll_delay(samp, atoi(att->value) );
					else if (!strcmp(att->name, "highlightColor")) gf_isom_text_set_highlight_color_argb(samp, tx3g_get_color(att->value));
				}
				start = end = 0;
				k=0;
				while ((sub=(GF_XMLNode*)gf_list_enum(desc->content, &k))) {
					if (sub->type) continue;
					if (!strcmp(sub->name, "text")) {
						GF_XMLNode *text;
						styleID = 0;
						m=0;
						while ((att=(GF_XMLAttribute *)gf_list_enum(sub->attributes, &m))) {
							if (!strcmp(att->name, "styleID")) styleID = atoi(att->value);
						}
						txt_len = 0;

						m=0;
						while ((text=(GF_XMLNode*)gf_list_enum(sub->content, &m))) {
							if (!text->type) {
								if (!strcmp(text->name, "marker")) {
									u32 z;
									memset(&marks[nb_marks], 0, sizeof(Marker));
									marks[nb_marks].pos = nb_chars+txt_len;

									z = 0;
									while ( (att=(GF_XMLAttribute *)gf_list_enum(text->attributes, &z))) {
										if (!strcmp(att->name, "id")) marks[nb_marks].id = atoi(att->value);
									}
									nb_marks++;
								}
							} else if (text->type==GF_XML_TEXT_TYPE) {
								txt_len += (u32) strlen(text->name);
								gf_isom_text_add_text(samp, text->name, (u32) strlen(text->name));
							}
						}
						if (styleID && (!same_style || (td.default_style.startCharOffset != styleID))) {
							GF_StyleRecord st = td.default_style;
							for (m=0; m<nb_styles; m++) {
								if (styles[m].startCharOffset==styleID) {
									st = styles[m];
									break;
								}
							}
							st.startCharOffset = nb_chars;
							st.endCharOffset = nb_chars + txt_len;
							gf_isom_text_add_style(samp, &st);
						}
						nb_chars += txt_len;
					}
					else if (!stricmp(sub->name, "highlight")) {
						m=0;
						while ((att=(GF_XMLAttribute *)gf_list_enum(sub->attributes, &m))) {
							if (!strcmp(att->name, "startMarker")) GET_MARKER_POS(start, 0)
								else if (!strcmp(att->name, "endMarker")) GET_MARKER_POS(end, 1)
								}
						gf_isom_text_add_highlight(samp, start, end);
					}
					else if (!stricmp(sub->name, "blink")) {
						m=0;
						while ((att=(GF_XMLAttribute *)gf_list_enum(sub->attributes, &m))) {
							if (!strcmp(att->name, "startMarker")) GET_MARKER_POS(start, 0)
								else if (!strcmp(att->name, "endMarker")) GET_MARKER_POS(end, 1)
								}
						gf_isom_text_add_blink(samp, start, end);
					}
					else if (!stricmp(sub->name, "link")) {
						char *url, *url_tt;
						url = url_tt = NULL;
						m=0;
						while ((att=(GF_XMLAttribute *)gf_list_enum(sub->attributes, &m))) {
							if (!strcmp(att->name, "startMarker")) GET_MARKER_POS(start, 0)
								else if (!strcmp(att->name, "endMarker")) GET_MARKER_POS(end, 1)
									else if (!strcmp(att->name, "URL") || !strcmp(att->name, "href")) url = gf_strdup(att->value);
									else if (!strcmp(att->name, "URLToolTip") || !strcmp(att->name, "altString")) url_tt = gf_strdup(att->value);
						}
						gf_isom_text_add_hyperlink(samp, url, url_tt, start, end);
						if (url) gf_free(url);
						if (url_tt) gf_free(url_tt);
					}
					else if (!stricmp(sub->name, "karaoke")) {
						u32 time = 0;
						GF_XMLNode *krok;
						m=0;
						while ((att=(GF_XMLAttribute *)gf_list_enum(sub->attributes, &m))) {
							if (!strcmp(att->name, "startTime")) time = atoi(att->value);
						}
						gf_isom_text_add_karaoke(samp, time);
						m=0;
						while ((krok=(GF_XMLNode*)gf_list_enum(sub->content, &m))) {
							u32 u=0;
							if (krok->type) continue;
							if (strcmp(krok->name, "run")) continue;
							start = end = 0;
							while ((att=(GF_XMLAttribute *)gf_list_enum(krok->attributes, &u))) {
								if (!strcmp(att->name, "startMarker")) GET_MARKER_POS(start, 0)
									else if (!strcmp(att->name, "endMarker")) GET_MARKER_POS(end, 1)
										else if (!strcmp(att->name, "duration")) time += atoi(att->value);
							}
							gf_isom_text_set_karaoke_segment(samp, time, start, end);
						}
					}
				}
			}
		}
		/*OK, let's add the sample*/
		if (samp) {
			if (!same_box) gf_isom_text_set_box(samp, td.default_pos.top, td.default_pos.left, td.default_pos.bottom, td.default_pos.right);
//			if (!same_style) gf_isom_text_add_style(samp, &td.default_style);

			txtin_process_send_text_sample(ctx, samp, (ctx->start*ctx->timescale)/ctx->txml_timescale, (u32) (duration*ctx->timescale)/ctx->txml_timescale, isRAP);
			ctx->start += duration;
			gf_isom_delete_text_sample(samp);

		}
		if (gf_filter_pid_would_block(ctx->opid)) {
			ctx->cur_child_idx++;
			return GF_OK;
		}
	}

	return GF_EOS;
}


static GF_Err txtin_process(GF_Filter *filter)
{
	GF_TXTIn *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_Err e;
	Bool start, end;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		return GF_OK;
	}
	gf_filter_pck_get_framing(pck, &start, &end);
	if (!end) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	//file is loaded

	e = ctx->text_process(filter, ctx);


	if (e==GF_EOS) {
		//keep input alive until end of stream, so that we keep getting called
		gf_filter_pid_drop_packet(ctx->ipid);
		if (gf_filter_pid_is_eos(ctx->ipid))
			gf_filter_pid_set_eos(ctx->opid);
	}
	return e;
}

static GF_Err txtin_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Err e;
	const char *src = NULL;
	GF_TXTIn *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *prop;

	if (is_remove) {
		ctx->ipid = NULL;
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	//we must have a file path
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (prop && prop->value.string) src = prop->value.string;
	if (!src)
		return GF_NOT_SUPPORTED;

	if (!ctx->ipid) {
		GF_FilterEvent fevt;
		ctx->ipid = pid;

		//we work with full file only, send a play event on source to indicate that
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, pid);
		fevt.play.start_range = 0;
		fevt.base.on_pid = ctx->ipid;
		fevt.play.full_file_only = GF_TRUE;
		gf_filter_pid_send_event(ctx->ipid, &fevt);
		ctx->file_name = src;
	} else {
		if (pid != ctx->ipid) {
			return GF_REQUIRES_NEW_INSTANCE;
		}
		if (!strcmp(ctx->file_name, src)) return GF_OK;
		//TODO reset context
		ctx->is_setup = GF_FALSE;

		ctx->file_name = src;
	}
	//guess type
	e = gf_text_guess_format(ctx->file_name, &ctx->fmt);
	if (e) return e;
	if (!ctx->fmt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[TXTLoad] Unknown text format for %s\n", ctx->file_name));
		return GF_NOT_SUPPORTED;
	}

	if (ctx->webvtt && (ctx->fmt == GF_TXTIN_MODE_SRT))
		ctx->fmt = GF_TXTIN_MODE_WEBVTT;

	switch (ctx->fmt) {
	case GF_TXTIN_MODE_SRT:
		ctx->text_process = txtin_process_srt;
		break;
#ifndef GPAC_DISABLE_VTT
	case GF_TXTIN_MODE_WEBVTT:
		ctx->text_process = txtin_process_webvtt;
		break;
#endif
	case GF_TXTIN_MODE_TTXT:
		ctx->text_process = txtin_process_ttxt;
		break;
	case GF_TXTIN_MODE_TEXML:
		ctx->text_process = txtin_process_texml;
		break;
	case GF_TXTIN_MODE_SUB:
		ctx->text_process = gf_text_process_sub;
		break;
	case GF_TXTIN_MODE_TTML:
		ctx->text_process = gf_text_process_ttml;
		break;
#ifndef GPAC_DISABLE_SWF_IMPORT
	case GF_TXTIN_MODE_SWF_SVG:
		ctx->text_process = gf_text_process_swf;
		break;
#endif
	default:
		return GF_BAD_PARAM;
	}

	return GF_OK;
}

static Bool txtin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_TXTIn *ctx = gf_filter_get_udta(filter);
	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->playstate==1) return GF_TRUE;
		ctx->playstate = 1;
		if ((ctx->start_range < 0.1) && (evt->play.start_range<0.1)) return GF_TRUE;
		ctx->start_range = evt->play.start_range;
		ctx->seek_state = 1;
		//cancel play event, we work with full file
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->playstate = 2;
		//cancel play event, we work with full file
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
	return GF_FALSE;
}

GF_Err txtin_initialize(GF_Filter *filter)
{
	char data[1];
	GF_TXTIn *ctx = gf_filter_get_udta(filter);
	ctx->bs_w = gf_bs_new(data, 1, GF_BITSTREAM_WRITE);
	return GF_OK;
}
void txtin_finalize(GF_Filter *filter)
{
	GF_TXTIn *ctx = gf_filter_get_udta(filter);

	if (ctx->samp) gf_isom_delete_text_sample(ctx->samp);
	if (ctx->src) gf_fclose(ctx->src);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->vttparser) gf_webvtt_parser_del(ctx->vttparser);
	if (ctx->parser) gf_xml_dom_del(ctx->parser);
	if (ctx->parser_working_copy) gf_xml_dom_del(ctx->parser_working_copy);

	if (ctx->text_descs) {
		while (gf_list_count(ctx->text_descs)) {
			GF_PropertyValue *p = gf_list_pop_back(ctx->text_descs);
			gf_free(p->value.data.ptr);
			gf_free(p);
		}
		gf_list_del(ctx->text_descs);
	}
#ifndef GPAC_DISABLE_SWF_IMPORT
	gf_swf_reader_del(ctx->swf_parse);
#endif
}

static const char *txtin_probe_data(const u8 *data, u32 data_size, GF_FilterProbeScore *score)
{
	char *dst = NULL;
	u8 *res;

	res = gf_utf_get_utf8_string_from_bom((char *)data, data_size, &dst);
	if (res) data = res;

#define PROBE_OK(_score, _mime) \
		*score = _score;\
		if (dst) gf_free(dst);\
		return _mime; \


	if (!strncmp(data, "WEBVTT", 6)) {
		PROBE_OK(GF_FPROBE_SUPPORTED, "subtitle/vtt")
	}
	if (strstr(data, " --> ")) {
		PROBE_OK(GF_FPROBE_MAYBE_SUPPORTED, "subtitle/srt")
	}
	if (!strncmp(data, "FWS", 3) || !strncmp(data, "CWS", 3)) {
		PROBE_OK(GF_FPROBE_MAYBE_SUPPORTED, "application/x-shockwave-flash")
	}

	if ((data[0]=='{') && strstr(data, "}{")) {
		PROBE_OK(GF_FPROBE_MAYBE_SUPPORTED, "subtitle/sub")

	}
	/*XML formats*/
	if (!strstr(data, "?>") ) {
		if (dst) gf_free(dst);
		return NULL;
	}

	if (strstr(data, "<x-quicktime-tx3g") || strstr(data, "<text3GTrack")) {
		PROBE_OK(GF_FPROBE_MAYBE_SUPPORTED, "quicktime/text")
	}
	if (strstr(data, "TextStream")) {
		PROBE_OK(GF_FPROBE_MAYBE_SUPPORTED, "subtitle/ttxt")
	}
	if (strstr(data, "<tt ") || strstr(data, ":tt ")) {
		PROBE_OK(GF_FPROBE_MAYBE_SUPPORTED, "subtitle/ttml")
	}

	if (dst) gf_free(dst);
	return NULL;
}

static const GF_FilterCapability TXTInCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "x-subtitle/srt|subtitle/srt|text/srt"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "x-subtitle/sub|subtitle/sub|text/sub"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "x-subtitle/ttxt|subtitle/ttxt|text/ttxt"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "x-subtitle/vtt|subtitle/vtt|text/vtt"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "x-quicktime/text|quicktime/text"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "subtitle/ttml|text/ttml|application/xml+ttml"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-shockwave-flash"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "srt|ttxt|sub|vtt|txml|ttml|swf"),
};

#define OFFS(_n)	#_n, offsetof(GF_TXTIn, _n)

static const GF_FilterArgs TXTInArgs[] =
{
	{ OFFS(webvtt), "force WebVTT import of SRT files", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nodefbox), "skip default text box", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(noflush), "skip final sample flush for srt", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(fontname), "default font to use", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(fontsize), "default font size", GF_PROP_UINT, "18", NULL, 0},
	{ OFFS(lang), "default language to use", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(width), "default width of text area, set to 0 to resolve against visual PIDs", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(height), "default height of text area, set to 0 to resolve against visual PIDs", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(x), "default horizontal offset of text area: -1 (left), 0 (center) or 1 (right)", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(y), "default vertical offset of text area: -1 (bottom), 0 (center) or 1 (top)", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(zorder), "default z-order of the PID", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timescale), "default timescale of the PID", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister TXTInRegister = {
	.name = "txtin",
	GF_FS_SET_DESCRIPTION("Subtitle loader")
	GF_FS_SET_HELP("This filter reads subtitle data (srt/webvtt/ttxt/sub) to produce media PIDs and frames.\n"
	"The TTXT documentation is available at https://github.com/gpac/gpac/wiki/TTXT-Format-Documentation\n"
	)

	.private_size = sizeof(GF_TXTIn),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = TXTInArgs,
	SETCAPS(TXTInCaps),
	.process = txtin_process,
	.configure_pid = txtin_configure_pid,
	.process_event = txtin_process_event,
	.probe_data = txtin_probe_data,
	.initialize = txtin_initialize,
	.finalize = txtin_finalize
};


const GF_FilterRegister *txtin_register(GF_FilterSession *session)
{
	return &TXTInRegister;
}
