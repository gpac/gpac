/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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
#include <gpac/token.h>
#include <gpac/color.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ISOM_WRITE

enum
{
	GF_TEXT_IMPORT_NONE = 0,
	GF_TEXT_IMPORT_SRT,
	GF_TEXT_IMPORT_SUB,
	GF_TEXT_IMPORT_TTXT,
	GF_TEXT_IMPORT_TEXML,
	GF_TEXT_IMPORT_WEBVTT,
	GF_TEXT_IMPORT_TTML,
	GF_TEXT_IMPORT_SWF_SVG,
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
	u32 readen;
	unsigned char BOM[5];
	readen = (u32) fread(BOM, sizeof(char), 5, in_src);
	if (readen < 1)
		return -1;

	if ((BOM[0]==0xFF) && (BOM[1]==0xFE)) {
		/*UTF32 not supported*/
		if (!BOM[2] && !BOM[3]) return -1;
		gf_f64_seek(in_src, 2, SEEK_SET);
		return 3;
	}
	if ((BOM[0]==0xFE) && (BOM[1]==0xFF)) {
		/*UTF32 not supported*/
		if (!BOM[2] && !BOM[3]) return -1;
		gf_f64_seek(in_src, 2, SEEK_SET);
		return 2;
	} else if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) {
		gf_f64_seek(in_src, 3, SEEK_SET);
		return 1;
	}
	if (BOM[0]<0x80) {
		gf_f64_seek(in_src, 0, SEEK_SET);
		return 0;
	}
	return -1;
}

static GF_Err gf_text_guess_format(char *filename, u32 *fmt)
{
	char szLine[2048];
	u32 val;
	s32 uni_type;
	FILE *test = gf_f64_open(filename, "rb");
	if (!test) return GF_URL_ERROR;
	uni_type = gf_text_get_utf_type(test);

	if (uni_type>1) {
		const u16 *sptr;
		char szUTF[1024];
		u32 read = (u32) fread(szUTF, 1, 1023, test);
		szUTF[read]=0;
		sptr = (u16*)szUTF;
		read = (u32) gf_utf8_wcstombs(szLine, read, &sptr);
	} else {
		val = (u32) fread(szLine, 1, 1024, test);
		szLine[val]=0;
	}
	REM_TRAIL_MARKS(szLine, "\r\n\t ")

	*fmt = GF_TEXT_IMPORT_NONE;
	if ((szLine[0]=='{') && strstr(szLine, "}{")) *fmt = GF_TEXT_IMPORT_SUB;
	else if (!strnicmp(szLine, "<?xml ", 6)) {
		char *ext = strrchr(filename, '.');
		if (!strnicmp(ext, ".ttxt", 5)) *fmt = GF_TEXT_IMPORT_TTXT;
		else if (!strnicmp(ext, ".ttml", 5)) *fmt = GF_TEXT_IMPORT_TTML;
		ext = strstr(szLine, "?>");
		if (ext) ext += 2;
		if (!ext[0]) {
			if (!fgets(szLine, 2048, test))
				szLine[0] = '\0';
		}
		if (strstr(szLine, "x-quicktime-tx3g") || strstr(szLine, "text3GTrack")) *fmt = GF_TEXT_IMPORT_TEXML;
		else if (strstr(szLine, "TextStream")) *fmt = GF_TEXT_IMPORT_TTXT;
		else if (strstr(szLine, "tt")) *fmt = GF_TEXT_IMPORT_TTML;
	}
	else if (strstr(szLine, "WEBVTT") )
		*fmt = GF_TEXT_IMPORT_WEBVTT;
	else if (strstr(szLine, " --> ") )
		*fmt = GF_TEXT_IMPORT_SRT; /* might want to change the default to WebVTT */

	fclose(test);
	return GF_OK;
}


#define TTXT_DEFAULT_WIDTH	400
#define TTXT_DEFAULT_HEIGHT	60
#define TTXT_DEFAULT_FONT_SIZE	18

#ifndef GPAC_DISABLE_MEDIA_IMPORT
void gf_text_get_video_size(GF_MediaImporter *import, u32 *width, u32 *height)
{
	u32 w, h, f_w, f_h, i;
	GF_ISOFile *dest = import->dest;

	if (import->text_track_width && import->text_track_height) {
		(*width) = import->text_track_width;
		(*height) = import->text_track_height;
		return;
	}

	f_w = f_h = 0;
	for (i=0; i<gf_isom_get_track_count(dest); i++) {
		switch (gf_isom_get_media_type(dest, i+1)) {
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_VISUAL:
			gf_isom_get_visual_info(dest, i+1, 1, &w, &h);
			if (w > f_w) f_w = w;
			if (h > f_h) f_h = h;
			gf_isom_get_track_layout_info(dest, i+1, &w, &h, NULL, NULL, NULL);
			if (w > f_w) f_w = w;
			if (h > f_h) f_h = h;
			break;
		}
	}
	(*width) = f_w ? f_w : TTXT_DEFAULT_WIDTH;
	(*height) = f_h ? f_h : TTXT_DEFAULT_HEIGHT;
}


void gf_text_import_set_language(GF_MediaImporter *import, u32 track)
{
	if (import->esd && import->esd->langDesc) {
		char lang[4];
		lang[0] = (import->esd->langDesc->langCode>>16) & 0xFF;
		lang[1] = (import->esd->langDesc->langCode>>8) & 0xFF;
		lang[2] = (import->esd->langDesc->langCode) & 0xFF;
		lang[3] = 0;
		gf_isom_set_media_language(import->dest, track, lang);
	}
}
#endif

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


#ifndef GPAC_DISABLE_MEDIA_IMPORT

static GF_Err gf_text_import_srt(GF_MediaImporter *import)
{
	FILE *srt_in;
	u32 track, timescale, i, count;
	GF_TextConfig*cfg;
	GF_Err e;
	GF_StyleRecord rec;
	GF_TextSample * samp;
	GF_ISOSample *s;
	u32 sh, sm, ss, sms, eh, em, es, ems, txt_line, char_len, char_line, nb_samp, j, duration, rem_styles;
	Bool set_start_char, set_end_char, first_samp, rem_color;
	u64 start, end, prev_end, file_size;
	u32 state, curLine, line, len, ID, OCR_ES_ID, default_color;
	s32 unicode_type;
	char szLine[2048], szText[2048], *ptr;
	unsigned short uniLine[5000], uniText[5000], *sptr;

	srt_in = gf_f64_open(import->in_name, "rt");
	gf_f64_seek(srt_in, 0, SEEK_END);
	file_size = gf_f64_tell(srt_in);
	gf_f64_seek(srt_in, 0, SEEK_SET);

	unicode_type = gf_text_get_utf_type(srt_in);
	if (unicode_type<0) {
		fclose(srt_in);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported SRT UTF encoding");
	}

	cfg = NULL;
	if (import->esd) {
		if (!import->esd->slConfig) {
			import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
			import->esd->slConfig->predefined = 2;
			import->esd->slConfig->timestampResolution = 1000;
		}
		timescale = import->esd->slConfig->timestampResolution;
		if (!timescale) timescale = 1000;

		/*explicit text config*/
		if (import->esd->decoderConfig && import->esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_TEXT_CFG_TAG) {
			cfg = (GF_TextConfig *) import->esd->decoderConfig->decoderSpecificInfo;
			import->esd->decoderConfig->decoderSpecificInfo = NULL;
		}
		ID = import->esd->ESID;
		OCR_ES_ID = import->esd->OCRESID;
	} else {
		timescale = 1000;
		OCR_ES_ID = ID = 0;
	}

	if (cfg && cfg->timescale) timescale = cfg->timescale;
	track = gf_isom_new_track(import->dest, ID, GF_ISOM_MEDIA_TEXT, timescale);
	if (!track) {
		fclose(srt_in);
		return gf_import_message(import, gf_isom_last_error(import->dest), "Error creating text track");
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);
	if (import->esd && !import->esd->ESID) import->esd->ESID = import->final_trackID;

	if (OCR_ES_ID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, OCR_ES_ID);

	/*setup track*/
	if (cfg) {
		char *firstFont = NULL;
		/*set track info*/
		gf_isom_set_track_layout_info(import->dest, track, cfg->text_width<<16, cfg->text_height<<16, 0, 0, cfg->layer);

		/*and set sample descriptions*/
		count = gf_list_count(cfg->sample_descriptions);
		for (i=0; i<count; i++) {
			GF_TextSampleDescriptor *sd= (GF_TextSampleDescriptor *)gf_list_get(cfg->sample_descriptions, i);
			if (!sd->font_count) {
				sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
				sd->font_count = 1;
				sd->fonts[0].fontID = 1;
				sd->fonts[0].fontName = gf_strdup("Serif");
			}
			if (!sd->default_style.fontID) sd->default_style.fontID = sd->fonts[0].fontID;
			if (!sd->default_style.font_size) sd->default_style.font_size = 16;
			if (!sd->default_style.text_color) sd->default_style.text_color = 0xFF000000;
			/*store attribs*/
			if (!i) rec = sd->default_style;

			gf_isom_new_text_description(import->dest, track, sd, NULL, NULL, &state);
			if (!firstFont) firstFont = sd->fonts[0].fontName;
		}
		gf_import_message(import, GF_OK, "Timed Text (SRT) import - text track %d x %d, font %s (size %d)", cfg->text_width, cfg->text_height, firstFont, rec.font_size);

		gf_odf_desc_del((GF_Descriptor *)cfg);
	} else {
		u32 w, h;
		GF_TextSampleDescriptor *sd;
		gf_text_get_video_size(import, &w, &h);

		/*have to work with default - use max size (if only one video, this means the text region is the
		entire display, and with bottom alignment things should be fine...*/
		gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, 0, 0, 0);
		sd = (GF_TextSampleDescriptor*)gf_odf_desc_new(GF_ODF_TX3G_TAG);
		sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
		sd->font_count = 1;
		sd->fonts[0].fontID = 1;
		sd->fonts[0].fontName = gf_strdup(import->fontName ? import->fontName : "Serif");
		sd->back_color = 0x00000000;	/*transparent*/
		sd->default_style.fontID = 1;
		sd->default_style.font_size = import->fontSize ? import->fontSize : TTXT_DEFAULT_FONT_SIZE;
		sd->default_style.text_color = 0xFFFFFFFF;	/*white*/
		sd->default_style.style_flags = 0;
		sd->horiz_justif = 1; /*center of scene*/
		sd->vert_justif = (s8) -1;	/*bottom of scene*/

		if (import->flags & GF_IMPORT_SKIP_TXT_BOX) {
			sd->default_pos.top = sd->default_pos.left = sd->default_pos.right = sd->default_pos.bottom = 0;
		} else {
			if ((sd->default_pos.bottom==sd->default_pos.top) || (sd->default_pos.right==sd->default_pos.left)) {
				sd->default_pos.left = import->text_x;
				sd->default_pos.top = import->text_y;
				sd->default_pos.right = (import->text_width ? import->text_width : w) + sd->default_pos.left;
				sd->default_pos.bottom = (import->text_height ? import->text_height : h) + sd->default_pos.top;
			}
		}

		/*store attribs*/
		rec = sd->default_style;
		gf_isom_new_text_description(import->dest, track, sd, NULL, NULL, &state);

		gf_import_message(import, GF_OK, "Timed Text (SRT) import - text track %d x %d, font %s (size %d)", w, h, sd->fonts[0].fontName, rec.font_size);
		gf_odf_desc_del((GF_Descriptor *)sd);
	}
	gf_text_import_set_language(import, track);
	duration = (u32) (((Double) import->duration)*timescale/1000.0);

	default_color = rec.text_color;

	e = GF_OK;
	state = 0;
	end = prev_end = 0;
	curLine = 0;
	txt_line = 0;
	set_start_char = set_end_char = GF_FALSE;
	char_len = 0;
	start = 0;
	nb_samp = 0;
	samp = gf_isom_new_text_sample();

	first_samp = GF_TRUE;
	while (1) {
		char *sOK = gf_text_get_utf8_line(szLine, 2048, srt_in, unicode_type);

		if (sOK) REM_TRAIL_MARKS(szLine, "\r\n\t ")
			if (!sOK || !strlen(szLine)) {
				rec.style_flags = 0;
				rec.startCharOffset = rec.endCharOffset = 0;
				if (txt_line) {
					if (prev_end && (start != prev_end)) {
						GF_TextSample * empty_samp = gf_isom_new_text_sample();
						s = gf_isom_text_to_sample(empty_samp);
						gf_isom_delete_text_sample(empty_samp);
						if (state<=2) {
							s->DTS = (u64) ((timescale*prev_end)/1000);
							s->IsRAP = RAP;
							gf_isom_add_sample(import->dest, track, 1, s);
							nb_samp++;
						}
						gf_isom_sample_del(&s);
					}

					s = gf_isom_text_to_sample(samp);
					if (state<=2) {
						s->DTS = (u64) ((timescale*start)/1000);
						s->IsRAP = RAP;
						gf_isom_add_sample(import->dest, track, 1, s);
						gf_isom_sample_del(&s);
						nb_samp++;
						prev_end = end;
					}
					txt_line = 0;
					char_len = 0;
					set_start_char = set_end_char = GF_FALSE;
					rec.startCharOffset = rec.endCharOffset = 0;
					gf_isom_text_reset(samp);

					//gf_import_progress(import, nb_samp, nb_samp+1);
					gf_set_progress("Importing SRT", gf_f64_tell(srt_in), file_size);
					if (duration && (end >= duration)) break;
				}
				state = 0;
				if (!sOK) break;
				continue;
			}

		switch (state) {
		case 0:
			if (sscanf(szLine, "%u", &line) != 1) {
				e = gf_import_message(import, GF_CORRUPTED_DATA, "Bad SRT formatting - expecting number got \"%s\"", szLine);
				goto exit;
			}
			if (line != curLine + 1) gf_import_message(import, GF_OK, "WARNING: corrupted SRT frame %d after frame %d", line, curLine);
			curLine = line;
			state = 1;
			break;
		case 1:
			if (sscanf(szLine, "%u:%u:%u,%u --> %u:%u:%u,%u", &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) != 8) {
				sh = eh = 0;
				if (sscanf(szLine, "%u:%u,%u --> %u:%u,%u", &sm, &ss, &sms, &em, &es, &ems) != 6) {
					e = gf_import_message(import, GF_CORRUPTED_DATA, "Error scanning SRT frame %d timing", curLine);
					goto exit;
				}
			}
			start = (3600*sh + 60*sm + ss)*1000 + sms;
			if (start<end) {
				gf_import_message(import, GF_OK, "WARNING: overlapping SRT frame %d - starts "LLD" ms is before end of previous one "LLD" ms - adjusting time stamps", curLine, start, end);
				start = end;
			}

			end = (3600*eh + 60*em + es)*1000 + ems;
			/*make stream start at 0 by inserting a fake AU*/
			if (first_samp && (start>0)) {
				s = gf_isom_text_to_sample(samp);
				s->DTS = 0;
				gf_isom_add_sample(import->dest, track, 1, s);
				gf_isom_sample_del(&s);
				nb_samp++;
			}
			rec.style_flags = 0;
			state = 2;
			if (end<=prev_end) {
				gf_import_message(import, GF_OK, "WARNING: overlapping SRT frame %d end "LLD" is at or before previous end "LLD" - removing", curLine, end, prev_end);
				start = end;
				state = 3;
			}
			break;

		default:
			/*reset only when text is present*/
			first_samp = GF_FALSE;

			/*go to line*/
			if (txt_line) {
				gf_isom_text_add_text(samp, "\n", 1);
				char_len += 1;
			}

			ptr = (char *) szLine;
			{
				size_t _len = gf_utf8_mbstowcs(uniLine, 5000, (const char **) &ptr);
				if (_len == (size_t) -1) {
					e = gf_import_message(import, GF_CORRUPTED_DATA, "Invalid UTF data (line %d)", curLine);
					goto exit;
				}
				len = (u32) _len;
			}
			char_line = 0;
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
						gf_isom_text_add_style(samp, &rec);
						set_end_char = set_start_char = GF_FALSE;
						rec.style_flags &= ~rem_styles;
						rem_styles = 0;
						if (rem_color) {
							rec.text_color = default_color;
							rem_color = 0;
						}
					}
					if (set_start_char && (rec.startCharOffset != j)) {
						rec.endCharOffset = char_len + j;
						if (rec.style_flags) gf_isom_text_add_style(samp, &rec);
					}
					switch (uniLine[i+1]) {
					case 'b':
					case 'B':
						rec.style_flags |= GF_TXT_STYLE_BOLD;
						set_start_char = GF_TRUE;
						rec.startCharOffset = char_len + j;
						break;
					case 'i':
					case 'I':
						rec.style_flags |= GF_TXT_STYLE_ITALIC;
						set_start_char = GF_TRUE;
						rec.startCharOffset = char_len + j;
						break;
					case 'u':
					case 'U':
						rec.style_flags |= GF_TXT_STYLE_UNDERLINED;
						set_start_char = GF_TRUE;
						rec.startCharOffset = char_len + j;
						break;
					case 'f':
					case 'F':
						if (font_style) {
							rec.text_color = font_style;
							set_start_char = GF_TRUE;
							rec.startCharOffset = char_len + j;
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
						rec.endCharOffset = char_len + j;
						break;
					case 'i':
					case 'I':
						rem_styles |= GF_TXT_STYLE_ITALIC;
						set_end_char = GF_TRUE;
						rec.endCharOffset = char_len + j;
						break;
					case 'u':
					case 'U':
						rem_styles |= GF_TXT_STYLE_UNDERLINED;
						set_end_char = GF_TRUE;
						rec.endCharOffset = char_len + j;
						break;
					case 'f':
					case 'F':
						if (font_style) {
							rem_color = 1;
							set_end_char = GF_TRUE;
							rec.endCharOffset = char_len + j;
						}
					}
					i+=style_nb_chars;
					continue;
				}
				/*store style*/
				if (set_end_char) {
					gf_isom_text_add_style(samp, &rec);
					set_end_char = GF_FALSE;
					set_start_char = GF_TRUE;
					rec.startCharOffset = char_len + j;
					rec.style_flags &= ~rem_styles;
					rem_styles = 0;
					rec.text_color = default_color;
					rem_color = 0;
				}

				uniText[j] = uniLine[i];
				j++;
				i++;
			}
			/*store last style*/
			if (set_end_char) {
				gf_isom_text_add_style(samp, &rec);
				set_end_char = GF_FALSE;
				set_start_char = GF_TRUE;
				rec.startCharOffset = char_len + j;
				rec.style_flags &= ~rem_styles;
				rem_styles = 0;
			}

			char_line = j;
			uniText[j] = 0;

			sptr = (u16 *) uniText;
			len = (u32) gf_utf8_wcstombs(szText, 5000, (const u16 **) &sptr);

			gf_isom_text_add_text(samp, szText, len);
			char_len += char_line;
			txt_line ++;
			break;
		}
		if (duration && (start >= duration)) break;
	}

	gf_isom_delete_text_sample(samp);
	/*do not add any empty sample at the end since it modifies track duration and is not needed - it is the player job
	to figure out when to stop displaying the last text sample
	However update the last sample duration*/
	gf_isom_set_last_sample_duration(import->dest, track, (u32) (end-start) );
	gf_set_progress("Importing SRT", nb_samp, nb_samp);

exit:
	if (e) gf_isom_remove_track(import->dest, track);
	fclose(srt_in);
	return e;
}

/* Structure used to pass importer and track data to the parsers without exposing the GF_MediaImporter structure
   used by WebVTT and Flash->SVG */
typedef struct {
	GF_MediaImporter *import;
	u32 timescale;
	u32 track;
	u32 descriptionIndex;
} GF_ISOFlusher;

static GF_Err gf_webvtt_import_report(void *user, GF_Err e, char *message, const char *line)
{
	GF_ISOFlusher     *flusher = (GF_ISOFlusher *)user;
	return gf_import_message(flusher->import, e, message, line);
}

static void gf_webvtt_import_header(void *user, const char *config)
{
	GF_ISOFlusher     *flusher = (GF_ISOFlusher *)user;
	gf_isom_update_webvtt_description(flusher->import->dest, flusher->track, flusher->descriptionIndex, config);
}

static void gf_webvtt_flush_sample_to_iso(void *user, GF_WebVTTSample *samp)
{
	GF_ISOSample            *s;
	GF_ISOFlusher     *flusher = (GF_ISOFlusher *)user;
	//gf_webvtt_dump_sample(stdout, samp);
	s = gf_isom_webvtt_to_sample(samp);
	if (s) {
		s->DTS = (u64) (flusher->timescale*gf_webvtt_sample_get_start(samp)/1000);
		s->IsRAP = RAP;
		gf_isom_add_sample(flusher->import->dest, flusher->track, flusher->descriptionIndex, s);
		gf_isom_sample_del(&s);
	}
	gf_webvtt_sample_del(samp);
}

static GF_Err gf_text_import_webvtt(GF_MediaImporter *import)
{
	GF_Err						e;
	u32							track;
	u32							timescale;
	u32							duration;
	u32							descIndex;
	u32							ID;
	u32							OCR_ES_ID;
	GF_GenericSubtitleConfig	*cfg;
	GF_WebVTTParser				*vttparser;
	GF_ISOFlusher				flusher;

	cfg	= NULL;
	if (import->esd) {
		if (!import->esd->slConfig)	{
			import->esd->slConfig =	(GF_SLConfig *)	gf_odf_desc_new(GF_ODF_SLC_TAG);
			import->esd->slConfig->predefined =	2;
			import->esd->slConfig->timestampResolution = 1000;
		}
		timescale =	import->esd->slConfig->timestampResolution;
		if (!timescale)	timescale =	1000;

		/*explicit text	config*/
		if (import->esd->decoderConfig && import->esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_GEN_SUB_CFG_TAG) {
			cfg	= (GF_GenericSubtitleConfig	*) import->esd->decoderConfig->decoderSpecificInfo;
			import->esd->decoderConfig->decoderSpecificInfo	= NULL;
		}
		ID = import->esd->ESID;
		OCR_ES_ID =	import->esd->OCRESID;
	} else {
		timescale =	1000;
		OCR_ES_ID =	ID = 0;
	}

	if (cfg	&& cfg->timescale) timescale = cfg->timescale;
	track =	gf_isom_new_track(import->dest,	ID,	GF_ISOM_MEDIA_TEXT,	timescale);
	if (!track)	{
		return gf_import_message(import, gf_isom_last_error(import->dest), "Error creating WebVTT track");
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	import->final_trackID = gf_isom_get_track_id(import->dest, track);
	if (import->esd && !import->esd->ESID) import->esd->ESID = import->final_trackID;

	if (OCR_ES_ID) gf_isom_set_track_reference(import->dest, track,	GF_ISOM_REF_OCR, OCR_ES_ID);

	/*setup	track*/
	if (cfg) {
		u32	i;
		u32	count;
		/*set track	info*/
		gf_isom_set_track_layout_info(import->dest,	track, cfg->text_width<<16,	cfg->text_height<<16, 0, 0,	cfg->layer);

		/*and set sample descriptions*/
		count =	gf_list_count(cfg->sample_descriptions);
		for	(i=0; i<count; i++)	{
			gf_isom_new_webvtt_description(import->dest, track, NULL, NULL, NULL, &descIndex);
		}
		gf_import_message(import, GF_OK, "WebVTT import	- text track %d	x %d", cfg->text_width,	cfg->text_height);
		gf_odf_desc_del((GF_Descriptor *)cfg);
	} else {
		u32	w;
		u32	h;

		gf_text_get_video_size(import, &w, &h);
		gf_isom_set_track_layout_info(import->dest,	track, w<<16, h<<16, 0,	0, 0);

		gf_isom_new_webvtt_description(import->dest, track,	NULL, NULL,	NULL, &descIndex);

		gf_import_message(import, GF_OK, "WebVTT import");
	}
	gf_text_import_set_language(import, track);
	duration = (u32) (((Double) import->duration)*timescale/1000.0);

	vttparser = gf_webvtt_parser_new();
	flusher.import = import;
	flusher.timescale = timescale;
	flusher.track = track;
	flusher.descriptionIndex = descIndex;
	e = gf_webvtt_parser_init(vttparser, import->in_name, &flusher, gf_webvtt_import_report, gf_webvtt_flush_sample_to_iso, gf_webvtt_import_header);
	if (e != GF_OK) {
		gf_webvtt_parser_del(vttparser);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported WebVTT UTF encoding");
	}
	e = gf_webvtt_parser_parse(vttparser, duration);
	if (e != GF_OK) {
		gf_isom_remove_track(import->dest, track);
	}
	/*do not add any empty sample at the end since it modifies track duration and is not needed - it is the player job
	to figure out when to stop displaying the last text sample
	However update the last sample duration*/
	gf_isom_set_last_sample_duration(import->dest, track, (u32) gf_webvtt_parser_last_duration(vttparser));
	gf_webvtt_parser_del(vttparser);
	return e;
}

static char *ttxt_parse_string(GF_MediaImporter *import, char *str, Bool strip_lines)
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

static void ttml_import_progress(void *cbk, u64 cur_samp, u64 count)
{
	gf_set_progress("TTML Loading", cur_samp, count);
}

static void gf_text_import_ebu_ttd_remove_samples(GF_XMLNode *root, GF_XMLNode **sample_list_node)
{
	u32 idx = 0, body_num = 0;
	GF_XMLNode *node = NULL, *body_node = NULL;
	*sample_list_node = NULL;
	while ( (node = (GF_XMLNode*)gf_list_enum(root->content, &idx))) {
		if (!strcmp(node->name, "body")) {
			*sample_list_node = node;
			body_num = gf_list_count(node->content);
			while (body_num--) {
				body_node = (GF_XMLNode*)gf_list_get(node->content, 0);
				assert(gf_list_find(node->content, body_node) == 0);
				gf_list_rem(node->content, 0);
				gf_xml_dom_node_del(body_node);
			}
			return;
		}
	}
}

static GF_Err gf_text_import_ebu_ttd(GF_MediaImporter *import, GF_DOMParser *parser, GF_XMLNode *root)
{
	GF_Err e;
	u32 i, track, ID, desc_idx, nb_samples, nb_children;
	u64 last_sample_duration, last_sample_end;
	GF_XMLAttribute *att;
	GF_XMLNode *node, *root_working_copy, *sample_list_node;
	GF_DOMParser *parser_working_copy;
	char *samp_text, *xmlns;
	Bool has_body;

	samp_text = NULL;
	xmlns = NULL;
	root_working_copy = NULL;
	parser_working_copy = NULL;
	e = GF_OK;

	/*setup track in 3GP format directly (no ES desc)*/
	ID = (import->esd) ? import->esd->ESID : 0;
	track = gf_isom_new_track(import->dest, ID, GF_ISOM_MEDIA_MPEG_SUBT, 1000);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	/*some MPEG-4 setup*/
	if (import->esd) {
		if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->timestampResolution = 1000;
		import->esd->decoderConfig->streamType = GF_STREAM_TEXT;
		import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_TEXT_MPEG4;
		if (import->esd->OCRESID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, import->esd->OCRESID);
	}

	gf_import_message(import, GF_OK, "TTML Import");

	/*** root (including language) ***/
	i=0;
	while ( (att = (GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("Found root attribute name %s, value %s\n", att->name, att->value));

		if (!strcmp(att->name, "xmlns")) {
			if (strcmp(att->value, "http://www.w3.org/ns/ttml")) {
				e = gf_import_message(import, GF_BAD_PARAM, "Found invalid EBU-TTD root attribute name %s, value %s (shall be \"%s\")\n", att->name, att->value, "http://www.w3.org/ns/ttml");
				goto exit;
			}
			xmlns = att->name;
		} else if (!strcmp(att->name, "xml:lang")) {
			if (import->esd && !import->esd->langDesc) {
				char *lang;
				lang = gf_strdup(att->value);
				import->esd->langDesc = (GF_Language *) gf_odf_desc_new(GF_ODF_LANG_TAG);
				gf_isom_set_media_language(import->dest, track, lang);
			}
		} else if (!strcmp(att->name, "ttp:timeBase") && strcmp(att->value, "media")) {
			e = gf_import_message(import, GF_BAD_PARAM, "Found invalid EBU-TTD root attribute name %s, value %s (shall be \"%s\")\n", att->name, att->value, "media");
			goto exit;
		} else if (!strcmp(att->name, "xmlns:ttp") && strcmp(att->value, "http://www.w3.org/ns/ttml#parameter")) {
			e = gf_import_message(import, GF_BAD_PARAM, "Found invalid EBU-TTD root attribute name %s, value %s (shall be \"%s\")\n", att->name, att->value, "http://www.w3.org/ns/ttml#parameter");
			goto exit;
		} else if (!strcmp(att->name, "xmlns:tts") && strcmp(att->value, "http://www.w3.org/ns/ttml#styling")) {
			e = gf_import_message(import, GF_BAD_PARAM, "Found invalid EBU-TTD root attribute name %s, value %s (shall be \"%s\")\n", att->name, att->value, "http://www.w3.org/ns/ttml#styling");
			goto exit;
		}
	}

	/*** style ***/
#if 0
	Bool has_styling, has_style;
	GF_TextSampleDescriptor *sd;
	has_styling = GF_FALSE;
	has_style = GF_FALSE;
	sd = (GF_TextSampleDescriptor*)gf_odf_desc_new(GF_ODF_TX3G_TAG);
	i=0;
	while ( (node = (GF_XMLNode*)gf_list_enum(root->content, &i))) {
		if (node->type) {
			continue;
		} else if (!strcmp(node->name, "head")) {
			GF_XMLNode *head_node;
			u32 head_idx = 0;
			while ( (head_node = (GF_XMLNode*)gf_list_enum(node->content, &head_idx))) {
				if (!strcmp(head_node->name, "styling")) {
					GF_XMLNode *styling_node;
					u32 styling_idx;
					if (has_styling) {
						e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated \"styling\" element. Abort.\n");
						goto exit;
					}
					has_styling = GF_TRUE;

					styling_idx = 0;
					while ( (styling_node = (GF_XMLNode*)gf_list_enum(head_node->content, &styling_idx))) {
						if (!strcmp(styling_node->name, "style")) {
							GF_XMLAttribute *p_att;
							u32 style_idx = 0;
							while ( (p_att = (GF_XMLAttribute*)gf_list_enum(styling_node->attributes, &style_idx))) {
								if (!strcmp(p_att->name, "tts:direction")) {
								} else if (!strcmp(p_att->name, "tts:fontFamily")) {
									sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
									sd->font_count = 1;
									sd->fonts[0].fontID = 1;
									sd->fonts[0].fontName = gf_strdup(p_att->value);
								} else if (!strcmp(p_att->name, "tts:backgroundColor")) {
									GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("EBU-TTD style attribute \"%s\" ignored.\n", p_att->name));
									//sd->back_color = ;
								} else {
									if ( !strcmp(p_att->name, "tts:fontSize")
									        || !strcmp(p_att->name, "tts:lineHeight")
									        || !strcmp(p_att->name, "tts:textAlign")
									        || !strcmp(p_att->name, "tts:color")
									        || !strcmp(p_att->name, "tts:fontStyle")
									        || !strcmp(p_att->name, "tts:fontWeight")
									        || !strcmp(p_att->name, "tts:textDecoration")
									        || !strcmp(p_att->name, "tts:unicodeBidi")
									        || !strcmp(p_att->name, "tts:wrapOption")
									        || !strcmp(p_att->name, "tts:multiRowAlign")
									        || !strcmp(p_att->name, "tts:linePadding")) {
										GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("EBU-TTD style attribute \"%s\" ignored.\n", p_att->name));
									} else {
										GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("EBU-TTD unknown style attribute: \"%s\". Ignoring.\n", p_att->name));
									}
								}
							}
							break; //TODO: we only take care of the first style
						}
					}
				}
			}
		}
	}
	if (!has_styling) {
		e = gf_import_message(import, GF_BAD_PARAM, "TTML: missing \"styling\" element. Abort.\n");
		goto exit;
	}
	if (!has_style) {
		e = gf_import_message(import, GF_BAD_PARAM, "TTML: missing \"style\" element. Abort.\n");
		goto exit;
	}
	e = gf_isom_new_text_description(import->dest, track, sd, NULL, NULL, &desc_idx);
	gf_odf_desc_del((GF_Descriptor*)sd);
#else
	e = gf_isom_new_xml_subtitle_description(import->dest, track, xmlns, NULL, NULL, &desc_idx);
#endif
	if (e != GF_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("TTML: incorrect sample description. Abort.\n"));
		e = gf_isom_last_error(import->dest);
		goto exit;
	}

	/*** body ***/
	parser_working_copy = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser_working_copy, import->in_name, NULL, NULL);
	assert (e == GF_OK);
	root_working_copy = gf_xml_dom_get_root(parser_working_copy);
	assert(root_working_copy);
	last_sample_duration = 0;
	last_sample_end = 0;
	nb_samples = 0;
	nb_children = gf_list_count(root->content);
	has_body = GF_FALSE;
	i=0;
	while ( (node = (GF_XMLNode*)gf_list_enum(root->content, &i))) {
		if (node->type) {
			nb_children--;
			continue;
		}

		if (!strcmp(node->name, "body")) {
			GF_XMLNode *body_node;
			u32 body_idx = 0;

			if (has_body) {
				e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated \"body\" element. Abort.\n");
				goto exit;
			}
			has_body = GF_TRUE;

			/*remove all the entries from the working copy, we'll add samples one to one to create full XML samples*/
			gf_text_import_ebu_ttd_remove_samples(root_working_copy, &sample_list_node);

			while ( (body_node = (GF_XMLNode*)gf_list_enum(node->content, &body_idx))) {
				if (!strcmp(body_node->name, "div")) {
					GF_XMLNode *div_node;
					u32 div_idx = 0;
					while ( (div_node = (GF_XMLNode*)gf_list_enum(body_node->content, &div_idx))) {
						if (!strcmp(div_node->name, "p")) {
							GF_XMLNode *p_node;
							GF_XMLAttribute *p_att;
							u32 p_idx = 0, h, m, s, ms;
							s64 ts_begin = -1, ts_end = -1;

							//sample is either in the <p> ...
							while ( (p_att = (GF_XMLAttribute*)gf_list_enum(div_node->attributes, &p_idx))) {
								if (!strcmp(p_att->name, "begin")) {
									if (ts_begin != -1) {
										e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated \"begin\" attribute. Abort.\n");
										goto exit;
									}
									if (sscanf(p_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
										ts_begin = (h*3600 + m*60+s)*1000+ms;
									}
								} else if (!strcmp(p_att->name, "end")) {
									if (ts_end != -1) {
										e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated \"end\" attribute. Abort.\n");
										goto exit;
									}
									if (sscanf(p_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
										ts_end = (h*3600 + m*60+s)*1000+ms;
									}
								}
								if ((ts_begin != -1) && (ts_end != -1) && !samp_text && sample_list_node) {
									e = gf_xml_dom_append_child(sample_list_node, div_node);
									assert(e == GF_OK);
									assert(!samp_text);
									samp_text = gf_xml_dom_serialize((GF_XMLNode*)root_working_copy, GF_FALSE);
									e = gf_xml_dom_rem_child(sample_list_node, div_node);
									assert(e == GF_OK);
								}
							}

							//or under a <span>
							p_idx = 0;
							while ( (p_node = (GF_XMLNode*)gf_list_enum(div_node->content, &p_idx))) {
								if (!strcmp(p_node->name, "span")) {
									u32 span_idx = 0;
									GF_XMLAttribute *span_att;
									while ( (span_att = (GF_XMLAttribute*)gf_list_enum(p_node->attributes, &span_idx))) {
										if (!strcmp(span_att->name, "begin")) {
											if (ts_begin != -1) {
												e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated \"begin\" attribute under <span>. Abort.\n");
												goto exit;
											}
											if (sscanf(span_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
												ts_begin = (h*3600 + m*60+s)*1000+ms;
											}
										} else if (!strcmp(span_att->name, "end")) {
											if (ts_end != -1) {
												e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated \"end\" attribute under <span>. Abort.\n");
												goto exit;
											}
											if (sscanf(span_att->value, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
												ts_end = (h*3600 + m*60+s)*1000+ms;
											}
										}
										if ((ts_begin != -1) && (ts_end != -1) && !samp_text && sample_list_node) {
											if (samp_text) {
												e = gf_import_message(import, GF_BAD_PARAM, "TTML: duplicated sample text under <span>. Abort.\n");
												goto exit;
											}

											/*append the sample*/
											e = gf_xml_dom_append_child(sample_list_node, div_node);
											assert(e == GF_OK);
											assert(!samp_text);
											samp_text = gf_xml_dom_serialize((GF_XMLNode*)root_working_copy, GF_FALSE);
											e = gf_xml_dom_rem_child(sample_list_node, div_node);
											assert(e == GF_OK);
										}
									}
								}
							}

							if ((ts_begin != -1) && (ts_end != -1) && samp_text) {
								GF_ISOSample *s;
								GF_GenericSubtitleSample *samp;
								u32 len;
								char *str;

								if (ts_end < ts_begin) {
									e = gf_import_message(import, GF_BAD_PARAM, "TTML: invalid timings: \"begin\"="LLD" , \"end\"="LLD". Abort.\n", ts_begin, ts_end);
									goto exit;
								}

								if (ts_begin < (s64)last_sample_end) {
									e = gf_import_message(import, GF_BAD_PARAM, "TTML: timing overlapping not supported: \"begin\" is "LLD" , last \"end\" was "LLD". Abort.\n", ts_begin, last_sample_end);
									goto exit;
								}

								str = ttxt_parse_string(import, samp_text, GF_TRUE);
								len = (u32) strlen(str);
								samp = gf_isom_new_xml_subtitle_sample();
								/*each sample consists of a full valid XML file*/
								e = gf_isom_xml_subtitle_sample_add_text(samp, str, len);
								if (e) goto exit;
								gf_free(samp_text);
								samp_text = NULL;

								s = gf_isom_xml_subtitle_to_sample(samp);
								gf_isom_delete_xml_subtitle_sample(samp);
								if (!nb_samples) {
									s->DTS = 0; /*in MP4 we must start at T=0*/
								} else {
									s->DTS = ts_begin;
								}
								last_sample_duration = ts_end - ts_begin;
								last_sample_end = ts_end;
								GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("ts_begin="LLD", ts_end="LLD", last_sample_duration="LLU" (real duration: "LLU"), last_sample_end="LLU"\n", ts_begin, ts_end, ts_end - last_sample_end, last_sample_duration, last_sample_end));

								e = gf_isom_add_sample(import->dest, track, desc_idx, s);
								if (e) goto exit;
								gf_isom_sample_del(&s);
								nb_samples++;

								gf_set_progress("Importing TTML", nb_samples, nb_children);
								if (import->duration && (ts_end > import->duration))
									break;
							} else {
								GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("TTML: incomplete sample (begin="LLD", end="LLD", text=\"%s\"). Skip.\n", ts_begin, ts_end, samp_text ? samp_text : "NULL"));
							}
						}
					}
				}
			}
		}
	}
	if (!has_body) {
		e = gf_import_message(import, GF_BAD_PARAM, "TTML: missing \"body\" element. Abort.\n");
		goto exit;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("last_sample_duration="LLU", last_sample_end="LLU"\n", last_sample_duration, last_sample_end));
	gf_isom_set_last_sample_duration(import->dest, track, (u32) last_sample_duration);
	gf_set_progress("Importing TTML", nb_samples, nb_samples);

exit:
	gf_free(samp_text);
	gf_xml_dom_del(parser_working_copy);
	return e;
}

static GF_Err gf_text_import_ttml(GF_MediaImporter *import)
{
	GF_Err e;
	GF_DOMParser *parser;
	GF_XMLNode *root;

	if (import->flags == GF_IMPORT_PROBE_ONLY)
		return GF_OK;

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, import->in_name, ttml_import_progress, import);
	if (e) {
		gf_import_message(import, e, "Error parsing TTML file: Line %d - %s. Abort.", gf_xml_dom_get_line(parser), gf_xml_dom_get_error(parser));
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);
	if (!root) {
		gf_import_message(import, e, "Error parsing TTML file: no \"root\" found. Abort.");
		gf_xml_dom_del(parser);
		return e;
	}

	/*look for EBU-TTD*/
	if (!strcmp(root->name, "tt")) {
		e = gf_text_import_ebu_ttd(import, parser, root);
		if (e == GF_OK) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Note: TTML import - EBU-TTD detected\n"));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("Unsupported TTML file - only EBU-TTD is supported (root shall be \"tt\", got %s)\n", root->name));
			GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("Importing as generic TTML\n"));
			e = GF_OK;
		}
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("TTML file not recognized: found \"%s\" as root, \"%s\" expected\n", root->name, "tt"));
		e = GF_BAD_PARAM;
	}

	gf_xml_dom_del(parser);
	return e;
}

/* SimpleText Text tracks -related functions */
GF_SimpleTextSampleEntryBox *gf_isom_get_simpletext_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex)
{
	GF_SimpleTextSampleEntryBox *stxt;
	GF_TrackBox *trak;
	GF_Err e;

	if (!descriptionIndex) return NULL;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_READ);
	if (e) return NULL;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return NULL;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
		break;
	default:
		return NULL;
	}

	stxt = (GF_SimpleTextSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, descriptionIndex - 1);
	if (!stxt) return NULL;
	return stxt;
}

GF_Box *boxstring_new_with_data(u32 type, const char *string);

GF_Err gf_isom_update_simpletext_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, const char *config)
{
	GF_Err e;
	GF_SimpleTextSampleEntryBox *stxt;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	stxt = (GF_SimpleTextSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, descriptionIndex - 1);
	if (!stxt) {
		return GF_BAD_PARAM;
	} else {
		switch (stxt->type) {
		case GF_ISOM_BOX_TYPE_STXT:
			break;
		default:
			return GF_BAD_PARAM;
		}
		if (!movie->keep_utc)
			trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

		stxt->config = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_STTC, config);
		return GF_OK;
	}
}

GF_Err gf_isom_new_simpletext_description(GF_ISOFile *movie, u32 trackNumber, GF_TextSampleDescriptor *desc, char *URLname, char *URNname,
        const char *content_encoding, const char *mime, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_SimpleTextSampleEntryBox *stxt;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stxt = (GF_SimpleTextSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STXT);
	stxt->dataReferenceIndex = dataRefIndex;
	stxt->mime_type = gf_strdup(mime);
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, stxt);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	return e;
}


#ifndef GPAC_DISABLE_SWF_IMPORT

/* SWF Importer */
#include <gpac/internal/swf_dev.h>

static GF_Err swf_svg_add_iso_sample(void *user, const char *data, u32 length, u64 timestamp, Bool isRap)
{
	GF_Err				e = GF_OK;
	GF_ISOFlusher		*flusher = (GF_ISOFlusher *)user;
	GF_ISOSample		*s;
	GF_BitStream		*bs;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_BAD_PARAM;
	gf_bs_write_data(bs, data, length);
	s = gf_isom_sample_new();
	if (s) {
		gf_bs_get_content(bs, &s->data, &s->dataLength);
		s->DTS = (u64) (flusher->timescale*timestamp/1000);
		s->IsRAP = isRap ? RAP : RAP_NO;
		gf_isom_add_sample(flusher->import->dest, flusher->track, flusher->descriptionIndex, s);
		gf_isom_sample_del(&s);
	} else {
		e = GF_BAD_PARAM;
	}
	gf_bs_del(bs);
	return e;
}

static GF_Err swf_svg_add_iso_header(void *user, const char *data, u32 length, Bool isHeader)
{
	GF_ISOFlusher		*flusher = (GF_ISOFlusher *)user;
	if (!flusher) return GF_BAD_PARAM;
	if (isHeader) {
		return gf_isom_update_simpletext_description(flusher->import->dest, flusher->track, flusher->descriptionIndex , data);
	} else {
		return GF_OK;
	}
}

GF_EXPORT
GF_Err gf_text_import_swf(GF_MediaImporter *import)
{
	GF_Err						e = GF_OK;
	u32							track;
	u32							timescale;
	//u32							duration;
	u32							descIndex;
	u32							ID;
	u32							OCR_ES_ID;
	GF_GenericSubtitleConfig	*cfg;
	SWFReader					*read;
	GF_ISOFlusher				flusher;
	char						*mime;

	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		import->nb_tracks = 1;
		return GF_OK;
	}

	cfg	= NULL;
	if (import->esd) {
		if (!import->esd->slConfig)	{
			import->esd->slConfig =	(GF_SLConfig *)	gf_odf_desc_new(GF_ODF_SLC_TAG);
			import->esd->slConfig->predefined =	2;
			import->esd->slConfig->timestampResolution = 1000;
		}
		timescale =	import->esd->slConfig->timestampResolution;
		if (!timescale)	timescale =	1000;

		/*explicit text	config*/
		if (import->esd->decoderConfig && import->esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_GEN_SUB_CFG_TAG) {
			cfg	= (GF_GenericSubtitleConfig	*) import->esd->decoderConfig->decoderSpecificInfo;
			import->esd->decoderConfig->decoderSpecificInfo	= NULL;
		}
		ID = import->esd->ESID;
		OCR_ES_ID =	import->esd->OCRESID;
	} else {
		timescale =	1000;
		OCR_ES_ID =	ID = 0;
	}

	if (cfg	&& cfg->timescale) timescale = cfg->timescale;
	track =	gf_isom_new_track(import->dest,	ID,	GF_ISOM_MEDIA_TEXT,	timescale);
	if (!track)	{
		return gf_import_message(import, gf_isom_last_error(import->dest), "Error creating text track");
	}
	gf_isom_set_track_enabled(import->dest,	track, 1);
	if (import->esd	&& !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);

	if (OCR_ES_ID) gf_isom_set_track_reference(import->dest, track,	GF_ISOM_REF_OCR, OCR_ES_ID);

	if (!stricmp(import->streamFormat, "SVG")) {
		mime = "image/svg+xml";
	} else {
		mime = "application/octet-stream";
	}
	/*setup	track*/
	if (cfg) {
		u32	i;
		u32	count;
		/*set track	info*/
		gf_isom_set_track_layout_info(import->dest,	track, cfg->text_width<<16,	cfg->text_height<<16, 0, 0,	cfg->layer);

		/*and set sample descriptions*/
		count =	gf_list_count(cfg->sample_descriptions);
		for	(i=0; i<count; i++)	{
			gf_isom_new_simpletext_description(import->dest, track, NULL, NULL, NULL, NULL, mime, &descIndex);
		}
		gf_import_message(import, GF_OK, "SWF import - text track %d	x %d", cfg->text_width,	cfg->text_height);
		gf_odf_desc_del((GF_Descriptor *)cfg);
	} else {
		u32	w;
		u32	h;

		gf_text_get_video_size(import, &w, &h);
		gf_isom_set_track_layout_info(import->dest,	track, w<<16, h<<16, 0,	0, 0);

		gf_isom_new_simpletext_description(import->dest, track,	NULL, NULL,	NULL, NULL, mime, &descIndex);

		gf_import_message(import, GF_OK, "SWF import (as text - type: %s)", import->streamFormat);
	}
	gf_text_import_set_language(import, track);
	//duration = (u32) (((Double) import->duration)*timescale/1000.0);

	read = gf_swf_reader_new(NULL, import->in_name);
	gf_swf_read_header(read);
	flusher.import = import;
	flusher.track = track;
	flusher.timescale = timescale;
	flusher.descriptionIndex = descIndex;
	gf_swf_reader_set_user_mode(read, &flusher, swf_svg_add_iso_sample, swf_svg_add_iso_header);
	if (!import->streamFormat || (import->streamFormat && !stricmp(import->streamFormat, "SVG"))) {
		e = swf_to_svg_init(read, import->swf_flags, import->swf_flatten_angle);
	} else { /*if (import->streamFormat && !strcmp(import->streamFormat, "BIFS"))*/
		e = swf_to_bifs_init(read);
	}
	if (e) {
		goto exit;
	}
	/*parse all tags*/
	while (e == GF_OK) {
		e = swf_parse_tag(read);
	}
	if (e==GF_EOS) e = GF_OK;
exit:
	gf_swf_reader_del(read);
	return e;
}
/* end of SWF Importer */

#else

GF_EXPORT
GF_Err gf_text_import_swf(GF_MediaImporter *import)
{
	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Warning: GPAC was compiled without SWF import support, can't import track.\n"));
	return GF_NOT_SUPPORTED;
}

#endif /*GPAC_DISABLE_SWF_IMPORT*/

static GF_Err gf_text_import_sub(GF_MediaImporter *import)
{
	FILE *sub_in;
	u32 track, ID, timescale, i, j, desc_idx, start, end, prev_end, nb_samp, duration, len, line;
	u64 file_size;
	GF_TextConfig*cfg;
	GF_Err e;
	Double FPS;
	GF_TextSample * samp;
	Bool first_samp;
	s32 unicode_type;
	char szLine[2048], szTime[20], szText[2048];
	GF_ISOSample *s;

	sub_in = gf_f64_open(import->in_name, "rt");
	gf_f64_seek(sub_in, 0, SEEK_END);
	file_size = gf_f64_tell(sub_in);
	gf_f64_seek(sub_in, 0, SEEK_SET);

	unicode_type = gf_text_get_utf_type(sub_in);
	if (unicode_type<0) {
		fclose(sub_in);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported SUB UTF encoding");
	}

	FPS = GF_IMPORT_DEFAULT_FPS;
	if (import->video_fps) FPS = import->video_fps;

	cfg = NULL;
	if (import->esd) {
		if (!import->esd->slConfig) {
			import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
			import->esd->slConfig->predefined = 2;
			import->esd->slConfig->timestampResolution = 1000;
		}
		timescale = import->esd->slConfig->timestampResolution;
		if (!timescale) timescale = 1000;

		/*explicit text config*/
		if (import->esd->decoderConfig && import->esd->decoderConfig->decoderSpecificInfo->tag == GF_ODF_TEXT_CFG_TAG) {
			cfg = (GF_TextConfig *) import->esd->decoderConfig->decoderSpecificInfo;
			import->esd->decoderConfig->decoderSpecificInfo = NULL;
		}
		ID = import->esd->ESID;
	} else {
		timescale = 1000;
		ID = 0;
	}

	if (cfg && cfg->timescale) timescale = cfg->timescale;
	track = gf_isom_new_track(import->dest, ID, GF_ISOM_MEDIA_TEXT, timescale);
	if (!track) {
		fclose(sub_in);
		return gf_import_message(import, gf_isom_last_error(import->dest), "Error creating text track");
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);

	gf_text_import_set_language(import, track);

	file_size = 0;
	/*setup track*/
	if (cfg) {
		u32 count;
		char *firstFont = NULL;
		/*set track info*/
		gf_isom_set_track_layout_info(import->dest, track, cfg->text_width<<16, cfg->text_height<<16, 0, 0, cfg->layer);

		/*and set sample descriptions*/
		count = gf_list_count(cfg->sample_descriptions);
		for (i=0; i<count; i++) {
			GF_TextSampleDescriptor *sd= (GF_TextSampleDescriptor *)gf_list_get(cfg->sample_descriptions, i);
			if (!sd->font_count) {
				sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
				sd->font_count = 1;
				sd->fonts[0].fontID = 1;
				sd->fonts[0].fontName = gf_strdup("Serif");
			}
			if (!sd->default_style.fontID) sd->default_style.fontID = sd->fonts[0].fontID;
			if (!sd->default_style.font_size) sd->default_style.font_size = 16;
			if (!sd->default_style.text_color) sd->default_style.text_color = 0xFF000000;
			file_size = sd->default_style.font_size;
			gf_isom_new_text_description(import->dest, track, sd, NULL, NULL, &desc_idx);
			if (!firstFont) firstFont = sd->fonts[0].fontName;
		}
		gf_import_message(import, GF_OK, "Timed Text (SUB @ %02.2f) import - text track %d x %d, font %s (size %d)", FPS, cfg->text_width, cfg->text_height, firstFont, file_size);

		gf_odf_desc_del((GF_Descriptor *)cfg);
	} else {
		u32 w, h;
		GF_TextSampleDescriptor *sd;
		gf_text_get_video_size(import, &w, &h);

		/*have to work with default - use max size (if only one video, this means the text region is the
		entire display, and with bottom alignment things should be fine...*/
		gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, 0, 0, 0);
		sd = (GF_TextSampleDescriptor*)gf_odf_desc_new(GF_ODF_TX3G_TAG);
		sd->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
		sd->font_count = 1;
		sd->fonts[0].fontID = 1;
		sd->fonts[0].fontName = gf_strdup("Serif");
		sd->back_color = 0x00000000;	/*transparent*/
		sd->default_style.fontID = 1;
		sd->default_style.font_size = TTXT_DEFAULT_FONT_SIZE;
		sd->default_style.text_color = 0xFFFFFFFF;	/*white*/
		sd->default_style.style_flags = 0;
		sd->horiz_justif = 1; /*center of scene*/
		sd->vert_justif = (s8) -1;	/*bottom of scene*/

		if (import->flags & GF_IMPORT_SKIP_TXT_BOX) {
			sd->default_pos.top = sd->default_pos.left = sd->default_pos.right = sd->default_pos.bottom = 0;
		} else {
			if ((sd->default_pos.bottom==sd->default_pos.top) || (sd->default_pos.right==sd->default_pos.left)) {
				sd->default_pos.left = import->text_x;
				sd->default_pos.top = import->text_y;
				sd->default_pos.right = (import->text_width ? import->text_width : w) + sd->default_pos.left;
				sd->default_pos.bottom = (import->text_height ? import->text_height : h) + sd->default_pos.top;
			}
		}

		gf_isom_new_text_description(import->dest, track, sd, NULL, NULL, &desc_idx);
		gf_import_message(import, GF_OK, "Timed Text (SUB @ %02.2f) import - text track %d x %d, font %s (size %d)", FPS, w, h, sd->fonts[0].fontName, TTXT_DEFAULT_FONT_SIZE);
		gf_odf_desc_del((GF_Descriptor *)sd);
	}

	duration = (u32) (((Double) import->duration)*timescale/1000.0);

	e = GF_OK;
	nb_samp = 0;
	samp = gf_isom_new_text_sample();

	FPS = ((Double) timescale ) / FPS;
	start = end = prev_end = 0;

	line = 0;
	first_samp = GF_TRUE;
	while (1) {
		char *sOK = gf_text_get_utf8_line(szLine, 2048, sub_in, unicode_type);
		if (!sOK) break;

		REM_TRAIL_MARKS(szLine, "\r\n\t ")

		line++;
		len = (u32) strlen(szLine);
		if (!len) continue;

		i=0;
		if (szLine[i] != '{') {
			e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Bad SUB file (line %d): expecting \"{\" got \"%c\"", line, szLine[i]);
			goto exit;
		}
		while (szLine[i+1] && szLine[i+1]!='}') {
			szTime[i] = szLine[i+1];
			i++;
		}
		szTime[i] = 0;
		start = atoi(szTime);
		if (start<end) {
			gf_import_message(import, GF_OK, "WARNING: corrupted SUB frame (line %d) - starts (at %d ms) before end of previous one (%d ms) - adjusting time stamps", line, start, end);
			start = end;
		}
		j=i+2;
		i=0;
		if (szLine[i+j] != '{') {
			e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Bad SUB file - expecting \"{\" got \"%c\"", szLine[i]);
			goto exit;
		}
		while (szLine[i+1+j] && szLine[i+1+j]!='}') {
			szTime[i] = szLine[i+1+j];
			i++;
		}
		szTime[i] = 0;
		end = atoi(szTime);
		j+=i+2;

		if (start>end) {
			gf_import_message(import, GF_OK, "WARNING: corrupted SUB frame (line %d) - ends (at %d ms) before start of current frame (%d ms) - skipping", line, end, start);
			continue;
		}

		gf_isom_text_reset(samp);

		if (start && first_samp) {
			s = gf_isom_text_to_sample(samp);
			s->DTS = 0;
			s->IsRAP = RAP;
			gf_isom_add_sample(import->dest, track, 1, s);
			gf_isom_sample_del(&s);
			first_samp = GF_FALSE;
			nb_samp++;
		}

		for (i=j; i<len; i++) {
			if (szLine[i]=='|') {
				szText[i-j] = '\n';
			} else {
				szText[i-j] = szLine[i];
			}
		}
		szText[i-j] = 0;
		gf_isom_text_add_text(samp, szText, (u32) strlen(szText) );

		if (prev_end) {
			GF_TextSample * empty_samp = gf_isom_new_text_sample();
			s = gf_isom_text_to_sample(empty_samp);
			s->DTS = (u64) (FPS*(s64)prev_end);
			gf_isom_add_sample(import->dest, track, 1, s);
			gf_isom_sample_del(&s);
			nb_samp++;
			gf_isom_delete_text_sample(empty_samp);
		}

		s = gf_isom_text_to_sample(samp);
		s->DTS = (u64) (FPS*(s64)start);
		gf_isom_add_sample(import->dest, track, 1, s);
		gf_isom_sample_del(&s);
		nb_samp++;
		gf_isom_text_reset(samp);
		prev_end = end;
		gf_set_progress("Importing SUB", gf_f64_tell(sub_in), file_size);
		if (duration && (end >= duration)) break;
	}
	gf_isom_delete_text_sample(samp);
	/*do not add any empty sample at the end since it modifies track duration and is not needed - it is the player job
	to figure out when to stop displaying the last text sample
		However update the last sample duration*/

	gf_isom_set_last_sample_duration(import->dest, track, (u32) (end-start) );
	gf_set_progress("Importing SUB", nb_samp, nb_samp);

exit:
	if (e) gf_isom_remove_track(import->dest, track);
	fclose(sub_in);
	return e;
}


#define CHECK_STR(__str)	\
	if (!__str) { \
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid XML formatting (line %d)", parser.line);	\
		goto exit;	\
	}	\
 

u32 ttxt_get_color(GF_MediaImporter *import, char *val)
{
	u32 r, g, b, a, res;
	r = g = b = a = 0;
	if (sscanf(val, "%x %x %x %x", &r, &g, &b, &a) != 4) {
		gf_import_message(import, GF_OK, "Warning: color badly formatted");
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

void ttxt_parse_text_box(GF_MediaImporter *import, GF_XMLNode *n, GF_BoxRecord *box)
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

void ttxt_parse_text_style(GF_MediaImporter *import, GF_XMLNode *n, GF_StyleRecord *style)
{
	u32 i=0;
	GF_XMLAttribute *att;
	memset(style, 0, sizeof(GF_StyleRecord));
	style->fontID = 1;
	style->font_size = TTXT_DEFAULT_FONT_SIZE;
	style->text_color = 0xFFFFFFFF;

	while ( (att=(GF_XMLAttribute *)gf_list_enum(n->attributes, &i))) {
		if (!stricmp(att->name, "fromChar")) style->startCharOffset = atoi(att->value);
		else if (!stricmp(att->name, "toChar")) style->endCharOffset = atoi(att->value);
		else if (!stricmp(att->name, "fontID")) style->fontID = atoi(att->value);
		else if (!stricmp(att->name, "fontSize")) style->font_size = atoi(att->value);
		else if (!stricmp(att->name, "color")) style->text_color = ttxt_get_color(import, att->value);
		else if (!stricmp(att->name, "styles")) {
			if (strstr(att->value, "Bold")) style->style_flags |= GF_TXT_STYLE_BOLD;
			if (strstr(att->value, "Italic")) style->style_flags |= GF_TXT_STYLE_ITALIC;
			if (strstr(att->value, "Underlined")) style->style_flags |= GF_TXT_STYLE_UNDERLINED;
		}
	}
}

static void ttxt_import_progress(void *cbk, u64 cur_samp, u64 count)
{
	gf_set_progress("TTXT Loading", cur_samp, count);
}

static GF_Err gf_text_import_ttxt(GF_MediaImporter *import)
{
	GF_Err e;
	Bool last_sample_empty;
	u32 i, j, k, track, ID, nb_samples, nb_descs, nb_children;
	u64 last_sample_duration;
	GF_XMLAttribute *att;
	GF_DOMParser *parser;
	GF_XMLNode *root, *node, *ext;

	if (import->flags==GF_IMPORT_PROBE_ONLY) return GF_OK;

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, import->in_name, ttxt_import_progress, import);
	if (e) {
		gf_import_message(import, e, "Error parsing TTXT file: Line %d - %s", gf_xml_dom_get_line(parser), gf_xml_dom_get_error(parser));
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);

	e = GF_OK;
	if (strcmp(root->name, "TextStream")) {
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid Timed Text file - expecting \"TextStream\" got %s", "TextStream", root->name);
		goto exit;
	}

	/*setup track in 3GP format directly (no ES desc)*/
	ID = (import->esd) ? import->esd->ESID : 0;
	track = gf_isom_new_track(import->dest, ID, GF_ISOM_MEDIA_TEXT, 1000);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	/*some MPEG-4 setup*/
	if (import->esd) {
		if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->timestampResolution = 1000;
		import->esd->decoderConfig->streamType = GF_STREAM_TEXT;
		import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_TEXT_MPEG4;
		if (import->esd->OCRESID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, import->esd->OCRESID);
	}
	gf_text_import_set_language(import, track);

	gf_import_message(import, GF_OK, "Timed Text (GPAC TTXT) Import");

	last_sample_empty = GF_FALSE;
	last_sample_duration = 0;
	nb_descs = 0;
	nb_samples = 0;
	nb_children = gf_list_count(root->content);

	i=0;
	while ( (node = (GF_XMLNode*)gf_list_enum(root->content, &i))) {
		if (node->type) {
			nb_children--;
			continue;
		}

		if (!strcmp(node->name, "TextStreamHeader")) {
			GF_XMLNode *sdesc;
			s32 w, h, tx, ty, layer;
			u32 tref_id;
			w = TTXT_DEFAULT_WIDTH;
			h = TTXT_DEFAULT_HEIGHT;
			tx = ty = layer = 0;
			nb_children--;
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

			if (tref_id)
				gf_isom_set_track_reference(import->dest, track, GF_ISOM_BOX_TYPE_CHAP, tref_id);

			gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, tx<<16, ty<<16, (s16) layer);

			j=0;
			while ( (sdesc=(GF_XMLNode*)gf_list_enum(node->content, &j))) {
				if (sdesc->type) continue;

				if (!strcmp(sdesc->name, "TextSampleDescription")) {
					GF_TextSampleDescriptor td;
					u32 idx;
					memset(&td, 0, sizeof(GF_TextSampleDescriptor));
					td.tag = GF_ODF_TEXT_CFG_TAG;
					td.vert_justif = (s8) -1;
					td.default_style.fontID = 1;
					td.default_style.font_size = TTXT_DEFAULT_FONT_SIZE;

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
						else if (!strcmp(att->name, "backColor")) td.back_color = ttxt_get_color(import, att->value);
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
						if (!strcmp(ext->name, "TextBox")) ttxt_parse_text_box(import, ext, &td.default_pos);
						else if (!strcmp(ext->name, "Style")) ttxt_parse_text_style(import, ext, &td.default_style);
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
					if (import->flags & GF_IMPORT_SKIP_TXT_BOX) {
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
					gf_isom_new_text_description(import->dest, track, &td, NULL, NULL, &idx);
					for (k=0; k<td.font_count; k++) gf_free(td.fonts[k].fontName);
					gf_free(td.fonts);
					nb_descs ++;
				}
			}
		}
		/*sample text*/
		else if (!strcmp(node->name, "TextSample")) {
			GF_ISOSample *s;
			GF_TextSample * samp;
			u32 ts, descIndex;
			Bool has_text = GF_FALSE;
			if (!nb_descs) {
				e = gf_import_message(import, GF_BAD_PARAM, "Invalid Timed Text file - text stream header not found or empty");
				goto exit;
			}
			samp = gf_isom_new_text_sample();
			ts = 0;
			descIndex = 1;
			last_sample_empty = GF_TRUE;

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
					char *str = ttxt_parse_string(import, att->value, GF_TRUE);
					len = (u32) strlen(str);
					gf_isom_text_add_text(samp, str, len);
					last_sample_empty = len ? GF_FALSE : GF_TRUE;
					has_text = GF_TRUE;
				}
				else if (!strcmp(att->name, "scrollDelay")) gf_isom_text_set_scroll_delay(samp, (u32) (1000*atoi(att->value)));
				else if (!strcmp(att->name, "highlightColor")) gf_isom_text_set_highlight_color_argb(samp, ttxt_get_color(import, att->value));
				else if (!strcmp(att->name, "wrap") && !strcmp(att->value, "Automatic")) gf_isom_text_set_wrap(samp, 0x01);
			}

			/*get all modifiers*/
			j=0;
			while ( (ext=(GF_XMLNode*)gf_list_enum(node->content, &j))) {
				if (!has_text && (ext->type==GF_XML_TEXT_TYPE)) {
					u32 len;
					char *str = ttxt_parse_string(import, ext->name, GF_FALSE);
					len = (u32) strlen(str);
					gf_isom_text_add_text(samp, str, len);
					last_sample_empty = len ? GF_FALSE : GF_TRUE;
					has_text = GF_TRUE;
				}
				if (ext->type) continue;

				if (!stricmp(ext->name, "Style")) {
					GF_StyleRecord r;
					ttxt_parse_text_style(import, ext, &r);
					gf_isom_text_add_style(samp, &r);
				}
				else if (!stricmp(ext->name, "TextBox")) {
					GF_BoxRecord r;
					ttxt_parse_text_box(import, ext, &r);
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

			/*in MP4 we must start at T=0, so add an empty sample*/
			if (ts && !nb_samples) {
				GF_TextSample * firstsamp = gf_isom_new_text_sample();
				s = gf_isom_text_to_sample(firstsamp);
				s->DTS = 0;
				gf_isom_add_sample(import->dest, track, 1, s);
				nb_samples++;
				gf_isom_delete_text_sample(firstsamp);
				gf_isom_sample_del(&s);
			}

			s = gf_isom_text_to_sample(samp);
			gf_isom_delete_text_sample(samp);
			s->DTS = ts;
			if (last_sample_empty) {
				last_sample_duration = s->DTS - last_sample_duration;
			} else {
				last_sample_duration = s->DTS;
			}

			e = gf_isom_add_sample(import->dest, track, descIndex, s);
			if (e) goto exit;
			gf_isom_sample_del(&s);
			nb_samples++;

			gf_set_progress("Importing TTXT", nb_samples, nb_children);
			if (import->duration && (ts>import->duration)) break;
		}
	}
	if (last_sample_empty) {
		gf_isom_remove_sample(import->dest, track, nb_samples);
		gf_isom_set_last_sample_duration(import->dest, track, (u32) last_sample_duration);
	}
	gf_set_progress("Importing TTXT", nb_samples, nb_samples);

exit:
	gf_xml_dom_del(parser);
	return e;
}


u32 tx3g_get_color(GF_MediaImporter *import, char *value)
{
	u32 r, g, b, a;
	u32 res, v;
	r = g = b = a = 0;
	if (sscanf(value, "%u%%, %u%%, %u%%, %u%%", &r, &g, &b, &a) != 4) {
		gf_import_message(import, GF_OK, "Warning: color badly formatted");
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

void tx3g_parse_text_box(GF_MediaImporter *import, GF_XMLNode *n, GF_BoxRecord *box)
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


static void texml_import_progress(void *cbk, u64 cur_samp, u64 count)
{
	gf_set_progress("TeXML Loading", cur_samp, count);
}

static GF_Err gf_text_import_texml(GF_MediaImporter *import)
{
	GF_Err e;
	u32 track, ID, nb_samples, nb_children, nb_descs, timescale, w, h, i, j, k;
	u64 DTS;
	s32 tx, ty, layer;
	GF_StyleRecord styles[50];
	Marker marks[50];
	GF_XMLAttribute *att;
	GF_DOMParser *parser;
	GF_XMLNode *root, *node;

	if (import->flags==GF_IMPORT_PROBE_ONLY) return GF_OK;

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, import->in_name, texml_import_progress, import);
	if (e) {
		gf_import_message(import, e, "Error parsing TeXML file: Line %d - %s", gf_xml_dom_get_line(parser), gf_xml_dom_get_error(parser));
		gf_xml_dom_del(parser);
		return e;
	}
	root = gf_xml_dom_get_root(parser);

	if (strcmp(root->name, "text3GTrack")) {
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid QT TeXML file - expecting root \"text3GTrack\" got \"%s\"", root->name);
		goto exit;
	}
	w = TTXT_DEFAULT_WIDTH;
	h = TTXT_DEFAULT_HEIGHT;
	tx = ty = 0;
	layer = 0;
	timescale = 1000;
	i=0;
	while ( (att=(GF_XMLAttribute *)gf_list_enum(root->attributes, &i))) {
		if (!strcmp(att->name, "trackWidth")) w = atoi(att->value);
		else if (!strcmp(att->name, "trackHeight")) h = atoi(att->value);
		else if (!strcmp(att->name, "layer")) layer = atoi(att->value);
		else if (!strcmp(att->name, "timescale")) timescale = atoi(att->value);
		else if (!strcmp(att->name, "transform")) {
			Float fx, fy;
			sscanf(att->value, "translate(%f,%f)", &fx, &fy);
			tx = (u32) fx;
			ty = (u32) fy;
		}
	}

	/*setup track in 3GP format directly (no ES desc)*/
	ID = (import->esd) ? import->esd->ESID : 0;
	track = gf_isom_new_track(import->dest, ID, GF_ISOM_MEDIA_TEXT, timescale);
	if (!track) {
		e = gf_isom_last_error(import->dest);
		goto exit;
	}
	gf_isom_set_track_enabled(import->dest, track, 1);
	/*some MPEG-4 setup*/
	if (import->esd) {
		if (!import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);
		if (!import->esd->decoderConfig) import->esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
		if (!import->esd->slConfig) import->esd->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
		import->esd->slConfig->timestampResolution = timescale;
		import->esd->decoderConfig->streamType = GF_STREAM_TEXT;
		import->esd->decoderConfig->objectTypeIndication = GPAC_OTI_TEXT_MPEG4;
		if (import->esd->OCRESID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, import->esd->OCRESID);
	}
	DTS = 0;
	gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, tx<<16, ty<<16, (s16) layer);

	gf_text_import_set_language(import, track);
	e = GF_OK;

	gf_import_message(import, GF_OK, "Timed Text (QT TeXML) Import - Track Size %d x %d", w, h);

	nb_children = gf_list_count(root->content);
	nb_descs = 0;
	nb_samples = 0;
	i=0;
	while ( (node=(GF_XMLNode*)gf_list_enum(root->content, &i))) {
		GF_XMLNode *desc;
		GF_TextSampleDescriptor td;
		GF_TextSample * samp = NULL;
		GF_ISOSample *s;
		u32 duration, descIndex, nb_styles, nb_marks;
		Bool isRAP, same_style, same_box;

		if (node->type) continue;
		if (strcmp(node->name, "sample")) continue;

		isRAP = GF_FALSE;
		duration = 1000;
		j=0;
		while ((att=(GF_XMLAttribute *)gf_list_enum(node->attributes, &j))) {
			if (!strcmp(att->name, "duration")) duration = atoi(att->value);
			else if (!strcmp(att->name, "keyframe")) isRAP = (!stricmp(att->value, "true") ? GF_TRUE : GF_FALSE);
		}
		nb_styles = 0;
		nb_marks = 0;
		same_style = same_box = GF_FALSE;
		descIndex = 1;
		j=0;
		while ((desc=(GF_XMLNode*)gf_list_enum(node->content, &j))) {
			if (desc->type) continue;

			if (!strcmp(desc->name, "description")) {
				GF_XMLNode *sub;
				memset(&td, 0, sizeof(GF_TextSampleDescriptor));
				td.tag = GF_ODF_TEXT_CFG_TAG;
				td.vert_justif = (s8) -1;
				td.default_style.fontID = 1;
				td.default_style.font_size = TTXT_DEFAULT_FONT_SIZE;

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
					else if (!strcmp(att->name, "backgroundColor")) td.back_color = tx3g_get_color(import, att->value);
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
					if (!strcmp(sub->name, "defaultTextBox")) tx3g_parse_text_box(import, sub, &td.default_pos);
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
								else if (!strcmp(css_style, "color")) styles[nb_styles].text_color = tx3g_get_color(import, css_val);
							}
							if (!nb_styles) td.default_style = styles[0];
							nb_styles++;
						}
					}

				}
				if ((td.default_pos.bottom==td.default_pos.top) || (td.default_pos.right==td.default_pos.left)) {
					td.default_pos.top = td.default_pos.left = 0;
					td.default_pos.right = w;
					td.default_pos.bottom = h;
				}
				if (!td.fonts) {
					td.font_count = 1;
					td.fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord));
					td.fonts[0].fontID = 1;
					td.fonts[0].fontName = gf_strdup("Serif");
				}
				gf_isom_text_has_similar_description(import->dest, track, &td, &descIndex, &same_box, &same_style);
				if (!descIndex) {
					gf_isom_new_text_description(import->dest, track, &td, NULL, NULL, &descIndex);
					same_style = same_box = GF_TRUE;
				}

				for (k=0; k<td.font_count; k++) gf_free(td.fonts[k].fontName);
				gf_free(td.fonts);
				nb_descs ++;
			}
			else if (!strcmp(desc->name, "sampleData")) {
				GF_XMLNode *sub;
				u16 start, end;
				u32 styleID;
				u32 nb_chars, txt_len, m;
				txt_len = nb_chars = 0;

				samp = gf_isom_new_text_sample();

				k=0;
				while ((att=(GF_XMLAttribute *)gf_list_enum(desc->attributes, &k))) {
					if (!strcmp(att->name, "targetEncoding") && !strcmp(att->value, "utf16")) ;//is_utf16 = 1;
					else if (!strcmp(att->name, "scrollDelay")) gf_isom_text_set_scroll_delay(samp, atoi(att->value) );
					else if (!strcmp(att->name, "highlightColor")) gf_isom_text_set_highlight_color_argb(samp, tx3g_get_color(import, att->value));
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

			s = gf_isom_text_to_sample(samp);
			gf_isom_delete_text_sample(samp);
			s->IsRAP = isRAP ? RAP : RAP_NO;
			s->DTS = DTS;
			gf_isom_add_sample(import->dest, track, descIndex, s);
			gf_isom_sample_del(&s);
			nb_samples++;
			DTS += duration;
			gf_set_progress("Importing TeXML", nb_samples, nb_children);
			if (import->duration && (DTS*1000> timescale*import->duration)) break;
		}
	}
	gf_isom_set_last_sample_duration(import->dest, track, 0);
	gf_set_progress("Importing TeXML", nb_samples, nb_samples);

exit:
	gf_xml_dom_del(parser);
	return e;
}


GF_Err gf_import_timed_text(GF_MediaImporter *import)
{
	GF_Err e;
	u32 fmt;
	e = gf_text_guess_format(import->in_name, &fmt);
	if (e) return e;
	if (import->streamFormat) {
		if (!strcmp(import->streamFormat, "VTT")) fmt = GF_TEXT_IMPORT_WEBVTT;
		else if (!strcmp(import->streamFormat, "TTML")) fmt = GF_TEXT_IMPORT_TTML;
	}
	if ((strstr(import->in_name, ".swf") || strstr(import->in_name, ".SWF")) && !stricmp(import->streamFormat, "SVG")) fmt = GF_TEXT_IMPORT_SWF_SVG;
	if (!fmt) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TTXT Import] Input %s does not look like a supported text format - ignoring\n", import->in_name));
		return GF_NOT_SUPPORTED;
	}
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		if (fmt==GF_TEXT_IMPORT_SUB) import->flags |= GF_IMPORT_OVERRIDE_FPS;
		return GF_OK;
	}
	switch (fmt) {
	case GF_TEXT_IMPORT_SRT:
		return gf_text_import_srt(import);
	case GF_TEXT_IMPORT_SUB:
		return gf_text_import_sub(import);
	case GF_TEXT_IMPORT_TTXT:
		return gf_text_import_ttxt(import);
	case GF_TEXT_IMPORT_TEXML:
		return gf_text_import_texml(import);
	case GF_TEXT_IMPORT_WEBVTT:
		return gf_text_import_webvtt(import);
	case GF_TEXT_IMPORT_SWF_SVG:
		return gf_text_import_swf(import);
	case GF_TEXT_IMPORT_TTML:
		return gf_text_import_ttml(import);
	default:
		return GF_BAD_PARAM;
	}
}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/

#endif /*GPAC_DISABLE_ISOM_WRITE*/

