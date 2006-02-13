/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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
#include <gpac/internal/media_dev.h>

#ifndef GPAC_READ_ONLY

enum
{
	GF_TEXT_IMPORT_NONE = 0,
	GF_TEXT_IMPORT_SRT,
	GF_TEXT_IMPORT_SUB,
	GF_TEXT_IMPORT_TTXT,
	GF_TEXT_IMPORT_TEXML,
};

#define REM_TRAIL_MARKS(__str, __sep) while (1) {	\
		u32 _len = strlen(__str);		\
		if (!_len) break;	\
		_len--;				\
		if (strchr(__sep, __str[_len])) __str[_len] = 0;	\
		else break;	\
	}	\


static s32 gf_text_get_utf_type(FILE *in_src)
{
	unsigned char BOM[5];
	fread(BOM, 5, 1, in_src);

	if ((BOM[0]==0xFF) && (BOM[1]==0xFE)) {
		/*UTF32 not supported*/
		if (!BOM[2] && !BOM[3]) return -1;
		fseek(in_src, 2, SEEK_SET);
		return 3;
	} 
	if ((BOM[0]==0xFE) && (BOM[1]==0xFF)) {
		/*UTF32 not supported*/
		if (!BOM[2] && !BOM[3]) return -1;
		fseek(in_src, 2, SEEK_SET);
		return 2;
	} else if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) {
		fseek(in_src, 3, SEEK_SET);
		return 1;
	}
	if (BOM[0]<0x80) {
		fseek(in_src, 0, SEEK_SET);
		return 0;
	}
	return -1;
}

static GF_Err gf_text_guess_format(char *filename, u32 *fmt)
{
	char szLine[2048], szTest[10];
	u32 val;
	s32 uni_type;
	FILE *test = fopen(filename, "rt");
	if (!test) return GF_URL_ERROR;
	uni_type = gf_text_get_utf_type(test);

	while (fgets(szLine, 2048, test) != NULL) {
		REM_TRAIL_MARKS(szLine, "\r\n\t ")

		if (strlen(szLine)) break;
	}
	*fmt = GF_TEXT_IMPORT_NONE;
	if ((szLine[0]=='{') && strstr(szLine, "}{")) *fmt = GF_TEXT_IMPORT_SUB;
	else if (sscanf(szLine, "%d", &val)==1) {
		sprintf(szTest, "%d", val);
		if (!strcmp(szTest, szLine)) *fmt = GF_TEXT_IMPORT_SRT;
	}
	else if (!strnicmp(szLine, "<?xml ", 6)) {
		char *ext = strrchr(filename, '.');
		if (!strnicmp(ext, ".ttxt", 5)) *fmt = GF_TEXT_IMPORT_TTXT;
		ext = strstr(szLine, "?>");
		if (ext) ext += 2;
		if (!ext[0]) fgets(szLine, 2048, test);
		if (strstr(szLine, "x-quicktime-tx3g")) *fmt = GF_TEXT_IMPORT_TEXML;
	}
	fclose(test);
	return GF_OK;
}

#define TTXT_DEFAULT_WIDTH	400
#define TTXT_DEFAULT_HEIGHT	60
#define TTXT_DEFAULT_FONT_SIZE	18

static void gf_text_get_video_size(GF_ISOFile *dest, u32 *width, u32 *height)
{
	u32 w, h, f_w, f_h, i;

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


static void gf_text_import_set_language(GF_MediaImporter *import, u32 track)
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


static char *gf_text_get_utf8_line(char *szLine, u32 lineSize, FILE *txt_in, s32 unicode_type)
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
		len = strlen(szLine);
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
					szLineConv[j] = szLine[i]; i++; j++;
				} 
				/*UTF8 3 bytes char*/
				else if ( (szLine[i] & 0xf0) == 0xe0) {
					szLineConv[j] = szLine[i]; i++; j++;
					szLineConv[j] = szLine[i]; i++; j++; 
				} 
				/*UTF8 4 bytes char*/
				else if ( (szLine[i] & 0xf8) == 0xf0) {
					szLineConv[j] = szLine[i]; i++; j++;
					szLineConv[j] = szLine[i]; i++; j++; 
					szLineConv[j] = szLine[i]; i++; j++; 
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
	sptr = (short *)szLine;
	i = gf_utf8_wcstombs(szLineConv, 1024, (const unsigned short **) &sptr);
	szLineConv[i] = 0;
	strcpy(szLine, szLineConv);
	/*this is ugly indeed: since input is UTF16-LE, there are many chances the fgets never reads the \0 after a \n*/
	if (unicode_type==3) fgetc(txt_in); 
	return sOK;
}

static GF_Err gf_text_import_srt(GF_MediaImporter *import)
{
	FILE *srt_in;
	Double scale;
	u32 track, timescale, i, count;
	GF_TextConfig*cfg;
	GF_Err e;
	GF_StyleRecord rec;
	GF_TextSample * samp;
	GF_ISOSample *s;
	u32 sh, sm, ss, sms, eh, em, es, ems, txt_line, char_len, char_line, nb_samp, j, duration, file_size, rem_styles;
	Bool set_start_char, set_end_char, first_samp;
	u64 start, end, prev_end;
	u32 state, curLine, line, len, ID, OCR_ES_ID;
	s32 unicode_type;
	char szLine[2048], szText[2048], *ptr;
	unsigned short uniLine[5000], uniText[5000], *sptr;

	srt_in = fopen(import->in_name, "rt");
	fseek(srt_in, 0, SEEK_END);
	file_size = ftell(srt_in);
	fseek(srt_in, 0, SEEK_SET);

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
	if (import->esd && !import->esd->ESID) import->esd->ESID = gf_isom_get_track_id(import->dest, track);

	if (OCR_ES_ID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, OCR_ES_ID);

	/*setup track*/
	if (cfg) {
		char *firstFont = NULL;
		/*set track info*/
		gf_isom_set_track_layout_info(import->dest, track, cfg->text_width<<16, cfg->text_height<<16, 0, 0, cfg->layer);

		/*and set sample descriptions*/
		count = gf_list_count(cfg->sample_descriptions);
		for (i=0; i<count; i++) {
			GF_TextSampleDescriptor *sd= gf_list_get(cfg->sample_descriptions, i);
			if (!sd->font_count) {
				sd->fonts = malloc(sizeof(GF_FontRecord));
				sd->font_count = 1;
				sd->fonts[0].fontID = 1;
				sd->fonts[0].fontName = strdup("Serif");
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
		gf_text_get_video_size(import->dest, &w, &h);

		/*have to work with default - use max size (if only one video, this means the text region is the
		entire display, and with bottom alignment things should be fine...*/
		gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, 0, 0, 0);
		sd = (GF_TextSampleDescriptor*)gf_odf_desc_new(GF_ODF_TX3G_TAG);
		sd->fonts = malloc(sizeof(GF_FontRecord));
		sd->font_count = 1;
		sd->fonts[0].fontID = 1;
		sd->fonts[0].fontName = strdup("Serif");
		sd->back_color = 0x00000000;	/*transparent*/
		sd->default_style.fontID = 1;
		sd->default_style.font_size = TTXT_DEFAULT_FONT_SIZE;
		sd->default_style.text_color = 0xFFFFFFFF;	/*white*/
		sd->default_style.style_flags = 0;
		sd->horiz_justif = 1; /*center of scene*/
		sd->vert_justif = -1;	/*bottom of scene*/

		if (import->flags & GF_IMPORT_SKIT_TXT_BOX) {
			sd->default_pos.top = sd->default_pos.left = sd->default_pos.right = sd->default_pos.bottom = 0;
		} else {
			if ((sd->default_pos.bottom==sd->default_pos.top) || (sd->default_pos.right==sd->default_pos.left)) {
				sd->default_pos.top = sd->default_pos.left = 0;
				sd->default_pos.right = w;
				sd->default_pos.bottom = h;
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

	e = GF_OK;
	state = 0;
	end = prev_end = 0;
	curLine = 0;
	txt_line = 0;
	set_start_char = set_end_char = 0;
	char_len = 0;
	start = 0;
	nb_samp = 0;
	samp = gf_isom_new_text_sample();

	scale = timescale;
	scale /= 1000;
	first_samp = 1;
	while (1) {
		char *sOK = gf_text_get_utf8_line(szLine, 2048, srt_in, unicode_type);

		if (sOK) REM_TRAIL_MARKS(szLine, "\r\n\t ")
		if (!sOK || !strlen(szLine)) {
			state = 0;
			rec.style_flags = 0;
			rec.startCharOffset = rec.endCharOffset = 0;
			if (txt_line) {
				if (prev_end && (start != prev_end)) {
					GF_TextSample * empty_samp = gf_isom_new_text_sample();
					s = gf_isom_text_to_sample(empty_samp);
					gf_isom_delete_text_sample(empty_samp);
					s->DTS = (u64) (scale*(s64)prev_end);
					s->IsRAP = 1;
					gf_isom_add_sample(import->dest, track, 1, s);
					gf_isom_sample_del(&s);
					nb_samp++;
				}

				s = gf_isom_text_to_sample(samp);
				s->DTS = (u64) (scale*(s64) start);
				s->IsRAP = 1;
				gf_isom_add_sample(import->dest, track, 1, s);
				gf_isom_sample_del(&s);
				nb_samp++;
				prev_end = end;
				txt_line = 0;
				char_len = 0;
				set_start_char = set_end_char = 0;
				rec.startCharOffset = rec.endCharOffset = 0;
				gf_isom_text_reset(samp);

				//gf_import_progress(import, nb_samp, nb_samp+1);
				gf_import_progress(import, ftell(srt_in), file_size);
				if (duration && (end >= duration)) break;
			}
			if (!sOK) break;
			continue;
		}

		switch (state) {
		case 0:
			if (sscanf(szLine, "%d", &line) != 1) {
				e = gf_import_message(import, GF_CORRUPTED_DATA, "Bad SRT formatting - expecting number got \"%s\"", szLine);
				goto exit;
			}
			if (line != curLine + 1) gf_import_message(import, GF_OK, "WARNING: corrupted SRT frame %d after frame %d", line, curLine);
			curLine = line;
			state = 1;
			break;
		case 1:
			if (sscanf(szLine, "%d:%d:%d,%d --> %d:%d:%d,%d", &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) != 8) {
				e = gf_import_message(import, GF_CORRUPTED_DATA, "Error scanning SRT frame %d timing", curLine);
				goto exit;
			}
			start = (3600*sh + 60*sm + ss)*1000 + sms;
			if (start<end) {
				gf_import_message(import, GF_OK, "WARNING: corrupted SRT frame %d - starts (at %d ms) before end of previous one (%d ms) - adjusting time stamps", curLine, start, end);
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
			break;

		default:
			/*reset only when text is present*/
			first_samp = 0;

			/*go to line*/
			if (txt_line) {
				gf_isom_text_add_text(samp, "\n", 1);
				char_len += 1;
			}

			ptr = (char *) szLine;
			len = gf_utf8_mbstowcs(uniLine, 5000, (const char **) &ptr);
			if (len == (u32) -1) {
				e = gf_import_message(import, GF_CORRUPTED_DATA, "Invalid UTF data (line %d)", curLine);
				goto exit;
			}
			char_line = 0;
			i=j=0;
			rem_styles = 0;
			while (i<len) {
				/*start of new style*/
				if ( (uniLine[i]=='<') && (uniLine[i+2]=='>')) {
					/*store prev style*/
					if (set_end_char) {
						assert(set_start_char);
						gf_isom_text_add_style(samp, &rec);
						set_end_char = set_start_char = 0;
						rec.style_flags &= ~rem_styles;
						rem_styles = 0;
					}
					if (set_start_char && (rec.startCharOffset != j)) {
						rec.endCharOffset = char_len + j;
						if (rec.style_flags) gf_isom_text_add_style(samp, &rec);
					}
					switch (uniLine[i+1]) {
					case 'b': case 'B': 
						rec.style_flags |= GF_TXT_STYLE_BOLD; 
						set_start_char = 1;
						rec.startCharOffset = char_len + j;
						break;
					case 'i': case 'I': 
						rec.style_flags |= GF_TXT_STYLE_ITALIC; 
						set_start_char = 1;
						rec.startCharOffset = char_len + j;
						break;
					case 'u': case 'U': 
						rec.style_flags |= GF_TXT_STYLE_UNDERLINED; 
						set_start_char = 1;
						rec.startCharOffset = char_len + j;
						break;
					}
					i+=3;
					continue;
				}

				/*end of prev style*/
				if ( (uniLine[i]=='<') && (uniLine[i+1]=='/') && (uniLine[i+3]=='>')) {
					switch (uniLine[i+2]) {
					case 'b': case 'B': 
						rem_styles |= GF_TXT_STYLE_BOLD; 
						set_end_char = 1;
						rec.endCharOffset = char_len + j;
						break;
					case 'i': case 'I': 
						rem_styles |= GF_TXT_STYLE_ITALIC; 
						set_end_char = 1;
						rec.endCharOffset = char_len + j;
						break;
					case 'u': case 'U': 
						rem_styles |= GF_TXT_STYLE_UNDERLINED; 
						set_end_char = 1;
						rec.endCharOffset = char_len + j;
						break;
					}
					i+=4;
					continue;
				}
				/*store style*/
				if (set_end_char) {
					gf_isom_text_add_style(samp, &rec);
					set_end_char = 0;
					set_start_char = 1;
					rec.startCharOffset = char_len + j;
					rec.style_flags &= ~rem_styles;
					rem_styles = 0;
				}

				uniText[j] = uniLine[i];
				j++;
				i++;
			}
			/*store last style*/
			if (set_end_char) {
				gf_isom_text_add_style(samp, &rec);
				set_end_char = 0;
				set_start_char = 1;
				rec.startCharOffset = char_len + j;
				rec.style_flags &= ~rem_styles;
				rem_styles = 0;
			}

			char_line = j;
			uniText[j] = 0;

			sptr = (u16 *) uniText;
			len = gf_utf8_wcstombs(szText, 5000, (const u16 **) &sptr);

			gf_isom_text_add_text(samp, szText, len);
			char_len += char_line;
			txt_line ++;
			break;
		}
		if (duration && (start >= duration)) break;
	}

	/*final flush*/	
	if (end) {
		gf_isom_text_reset(samp);
		s = gf_isom_text_to_sample(samp);
		s->DTS = (u64) (scale*(s64)end);
		s->IsRAP = 1;
		gf_isom_add_sample(import->dest, track, 1, s);
		gf_isom_sample_del(&s);
		nb_samp++;
	}
	gf_isom_delete_text_sample(samp);
	gf_isom_set_last_sample_duration(import->dest, track, 0);
	gf_import_progress(import, nb_samp, nb_samp);

exit:
	if (e) gf_isom_remove_track(import->dest, track);
	fclose(srt_in);
	return e;
}


static GF_Err gf_text_import_sub(GF_MediaImporter *import)
{
	FILE *sub_in;
	u32 track, ID, timescale, i, j, desc_idx, start, end, prev_end, nb_samp, duration, file_size, len, line;
	GF_TextConfig*cfg;
	GF_Err e;
	Double FPS;
	GF_TextSample * samp;
	Bool first_samp;
	s32 unicode_type;
	char szLine[2048], szTime[20], szText[2048];
	GF_ISOSample *s;

	sub_in = fopen(import->in_name, "rt");
	fseek(sub_in, 0, SEEK_END);
	file_size = ftell(sub_in);
	fseek(sub_in, 0, SEEK_SET);
	
	unicode_type = gf_text_get_utf_type(sub_in);
	if (unicode_type<0) {
		fclose(sub_in);
		return gf_import_message(import, GF_NOT_SUPPORTED, "Unsupported SUB UTF encoding");
	}

	FPS = 25.0;
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
			GF_TextSampleDescriptor *sd= gf_list_get(cfg->sample_descriptions, i);
			if (!sd->font_count) {
				sd->fonts = malloc(sizeof(GF_FontRecord));
				sd->font_count = 1;
				sd->fonts[0].fontID = 1;
				sd->fonts[0].fontName = strdup("Serif");
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
		gf_text_get_video_size(import->dest, &w, &h);

		/*have to work with default - use max size (if only one video, this means the text region is the
		entire display, and with bottom alignment things should be fine...*/
		gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, 0, 0, 0);
		sd = (GF_TextSampleDescriptor*)gf_odf_desc_new(GF_ODF_TX3G_TAG);
		sd->fonts = malloc(sizeof(GF_FontRecord));
		sd->font_count = 1;
		sd->fonts[0].fontID = 1;
		sd->fonts[0].fontName = strdup("Serif");
		sd->back_color = 0x00000000;	/*transparent*/
		sd->default_style.fontID = 1;
		sd->default_style.font_size = TTXT_DEFAULT_FONT_SIZE;
		sd->default_style.text_color = 0xFFFFFFFF;	/*white*/
		sd->default_style.style_flags = 0;
		sd->horiz_justif = 1; /*center of scene*/
		sd->vert_justif = -1;	/*bottom of scene*/

		if (import->flags & GF_IMPORT_SKIT_TXT_BOX) {
			sd->default_pos.top = sd->default_pos.left = sd->default_pos.right = sd->default_pos.bottom = 0;
		} else {
			if ((sd->default_pos.bottom==sd->default_pos.top) || (sd->default_pos.right==sd->default_pos.left)) {
				sd->default_pos.top = sd->default_pos.left = 0;
				sd->default_pos.right = w;
				sd->default_pos.bottom = h;
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
	first_samp = 1;
	while (1) {
		char *sOK = gf_text_get_utf8_line(szLine, 2048, sub_in, unicode_type);
		if (!sOK) break;

		REM_TRAIL_MARKS(szLine, "\r\n\t ")

		line++;
		len = strlen(szLine); 
		if (!len) continue;

		i=0;
		if (szLine[i] != '{') {
			e = gf_import_message(import, GF_NON_COMPLIANT_BITSTREAM, "Bad SUB file (line %d): expecting \"{\" got \"%c\"", line, szLine[i]);
			goto exit;
		}
		while (szLine[i+1] && szLine[i+1]!='}') { szTime[i] = szLine[i+1]; i++; }
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
		while (szLine[i+1+j] && szLine[i+1+j]!='}') { szTime[i] = szLine[i+1+j]; i++; }
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
			s->IsRAP = 1;
			gf_isom_add_sample(import->dest, track, 1, s);
			gf_isom_sample_del(&s);
			first_samp = 0;
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
		gf_isom_text_add_text(samp, szText, strlen(szText) );

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
		gf_import_progress(import, ftell(sub_in), file_size);
		if (duration && (end >= duration)) break;
	}
	/*final flush*/
	if (end) {
		gf_isom_text_reset(samp);
		s = gf_isom_text_to_sample(samp);
		s->DTS = (u64)(FPS*(s64)end);
		gf_isom_add_sample(import->dest, track, 1, s);
		gf_isom_sample_del(&s);
		nb_samp++;
	}
	gf_isom_delete_text_sample(samp);
	
	gf_isom_set_last_sample_duration(import->dest, track, 0);
	gf_import_progress(import, nb_samp, nb_samp);

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


u32 ttxt_get_color(GF_MediaImporter *import, XMLParser *parser)
{
	u32 r, g, b, a, res;
	r = g = b = a = 0;
	if (sscanf(parser->value_buffer, "%x %x %x %x", &r, &g, &b, &a) != 4) {
		gf_import_message(import, GF_OK, "Warning (line %d): color badly formatted", parser->line);
	}
	res = (a&0xFF); res<<=8;
	res |= (r&0xFF); res<<=8;
	res |= (g&0xFF); res<<=8;
	res |= (b&0xFF);
	return res;
}

void ttxt_parse_text_box(GF_MediaImporter *import, XMLParser *parser, GF_BoxRecord *box)
{
	memset(box, 0, sizeof(GF_BoxRecord));
	while (xml_has_attributes(parser)) {
		char *str = xml_get_attribute(parser);
		if (!stricmp(str, "top")) box->top = atoi(parser->value_buffer);
		else if (!stricmp(str, "bottom")) box->bottom = atoi(parser->value_buffer);
		else if (!stricmp(str, "left")) box->left = atoi(parser->value_buffer);
		else if (!stricmp(str, "right")) box->right = atoi(parser->value_buffer);
	}
	xml_skip_element(parser, "TextBox");
}

void ttxt_parse_text_style(GF_MediaImporter *import, XMLParser *parser, GF_StyleRecord *style)
{
	memset(style, 0, sizeof(GF_StyleRecord));
	style->fontID = 1;
	style->font_size = TTXT_DEFAULT_FONT_SIZE;
	style->text_color = 0xFFFFFFFF;

	while (xml_has_attributes(parser)) {
		char *str = xml_get_attribute(parser);
		if (!stricmp(str, "fromChar")) style->startCharOffset = atoi(parser->value_buffer);
		else if (!stricmp(str, "toChar")) style->endCharOffset = atoi(parser->value_buffer);
		else if (!stricmp(str, "fontID")) style->fontID = atoi(parser->value_buffer);
		else if (!stricmp(str, "fontSize")) style->font_size = atoi(parser->value_buffer);
		else if (!stricmp(str, "color")) style->text_color = ttxt_get_color(import, parser);
		else if (!stricmp(str, "styles")) {
			if (strstr(parser->value_buffer, "Bold")) style->style_flags |= GF_TXT_STYLE_BOLD;
			if (strstr(parser->value_buffer, "Italic")) style->style_flags |= GF_TXT_STYLE_ITALIC;
			if (strstr(parser->value_buffer, "Underlined")) style->style_flags |= GF_TXT_STYLE_UNDERLINED;
		}
	}
	xml_skip_element(parser, "Style");
}

char *ttxt_parse_string(GF_MediaImporter *import, XMLParser *parser)
{
	char value[XML_LINE_SIZE];
	u32 i=0;
	u32 k=0;
	char *str = parser->value_buffer;

	strcpy(value, "");
	if (str[0]=='\'') {
		while (strchr(str, '\'')) {
			while (str[0] != '\'')  str += 1;
			str+=1;
			while (str[i] && (str[i] != '\'')) {
				/*handle UTF-8 - WARNING: if parser is in unicode string is already utf8 multibyte chars*/
				if (!parser->unicode_type && (str[i] & 0x80)) {
					/*non UTF8 (likely some win-CP)*/
					if ( (str[i+1] & 0xc0) != 0x80) {
						value[k] = 0xc0 | ( (str[i] >> 6) & 0x3 );
						k++;
						str[i] &= 0xbf;
					}
					/*we only handle UTF8 chars on 2 bytes (eg first byte is 0b110xxxxx)*/
					else if ( (str[i] & 0xe0) == 0xc0) {
						value[k] = str[i];
						i++;
						k++;
					}
				}
				value[k] = str[i];
				i++;
				k++;
			}
			if (!str[i+1]) break;
			str = str + i + 1;
			i=0;
			value[k] = '\n';
			k++;
		} 
	} else {
		while (str[i]) {
			/*handle UTF-8 - WARNING: if parser is in unicode string is already utf8 multibyte chars*/
			if (!parser->unicode_type && (str[i] & 0x80) ) {
				/*non UTF8 (likely some win-CP)*/
				if ( (str[i+1] & 0xc0) != 0x80) {
					value[k] = 0xc0 | ( (str[i] >> 6) & 0x3 );
					k++;
					str[i] &= 0xbf;
				}
				/*we only handle UTF8 chars on 2 bytes (eg first byte is 0b110xxxxx)*/
				else if ( (str[i] & 0xe0) == 0xc0) {
					value[k] = str[i];
					i++;
					k++;
				}
			}
			value[k] = str[i];
			i++;
			k++;
		}
	}
	value[k] = 0;
	if (!strlen(value)) return strdup("");
	return xml_translate_xml_string(value);
}

/*for GCC warnings*/
static void xml_import_progress(void *import, u32 cur_samp, u32 count)
{
	gf_import_progress((GF_MediaImporter *)import, cur_samp, count);
}

static GF_Err gf_text_import_ttxt(GF_MediaImporter *import)
{
	GF_Err e;
	Bool last_sample_empty;
	u32 track, ID, nb_samples, nb_descs;
	u64 last_sample_duration;
	XMLParser parser;
	char *str;

	if (import->flags==GF_IMPORT_PROBE_ONLY) return GF_OK;

	e = xml_init_parser(&parser, import->in_name);
	if (e) return gf_import_message(import, e, "Cannot open file %s", import->in_name);

	str = xml_get_element(&parser);
	if (!str) { e = GF_IO_ERR; goto exit; }
	if (strcmp(str, "TextStream")) {
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid Timed Text file - expecting %s got %s", "TextStream", str);
		goto exit;
	}
	xml_skip_attributes(&parser);

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
		import->esd->decoderConfig->objectTypeIndication = 0x08;
		if (import->esd->OCRESID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, import->esd->OCRESID);
	}
	gf_text_import_set_language(import, track);

	gf_import_message(import, GF_OK, "Timed Text (GPAC TTXT) Import");
	parser.OnProgress = xml_import_progress;
	parser.cbk = import;

	last_sample_empty = 0;
	last_sample_duration = 0;
	nb_descs = 0;
	nb_samples = 0;
	while (!xml_element_done(&parser, "TextStream") && !parser.done) {
		str = xml_get_element(&parser);
		CHECK_STR(str)

		if (!strcmp(str, "TextStreamHeader")) {
			s32 w, h, tx, ty, layer;
			w = TTXT_DEFAULT_WIDTH;
			h = TTXT_DEFAULT_HEIGHT;
			tx = ty = layer = 0;

			while (xml_has_attributes(&parser)) {
				str = xml_get_attribute(&parser);
				if (!strcmp(str, "width")) w = atoi(parser.value_buffer);
				else if (!strcmp(str, "height")) h = atoi(parser.value_buffer);
				else if (!strcmp(str, "layer")) layer = atoi(parser.value_buffer);
				else if (!strcmp(str, "translation_x")) tx = atoi(parser.value_buffer);
				else if (!strcmp(str, "translation_y")) ty = atoi(parser.value_buffer);
			}
			gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, tx<<16, ty<<16, (s16) layer);

			while (!xml_element_done(&parser, "TextStreamHeader")) {
				str = xml_get_element(&parser);
				CHECK_STR(str)

				if (!strcmp(str, "TextSampleDescription")) {
					GF_TextSampleDescriptor td;
					u32 idx, i;
					memset(&td, 0, sizeof(GF_TextSampleDescriptor));
					td.tag = GF_ODF_TEXT_CFG_TAG;
					td.vert_justif = -1;
					td.default_style.fontID = 1;
					td.default_style.font_size = TTXT_DEFAULT_FONT_SIZE;

					while (xml_has_attributes(&parser)) {
						str = xml_get_attribute(&parser);
						if (!strcmp(str, "horizontalJustification")) {
							if (!stricmp(parser.value_buffer, "center")) td.horiz_justif = 1;
							else if (!stricmp(parser.value_buffer, "right")) td.horiz_justif = -1;
							else if (!stricmp(parser.value_buffer, "left")) td.horiz_justif = 0;
						}
						else if (!strcmp(str, "verticalJustification")) {
							if (!stricmp(parser.value_buffer, "center")) td.vert_justif = 1;
							else if (!stricmp(parser.value_buffer, "bottom")) td.vert_justif = -1;
							else if (!stricmp(parser.value_buffer, "top")) td.vert_justif = 0;
						}
						else if (!strcmp(str, "backColor")) td.back_color = ttxt_get_color(import, &parser);
						else if (!strcmp(str, "verticalText") && !stricmp(parser.value_buffer, "yes") ) td.displayFlags |= GF_TXT_VERTICAL;
						else if (!strcmp(str, "fillTextRegion") && !stricmp(parser.value_buffer, "yes") ) td.displayFlags |= GF_TXT_FILL_REGION;
						else if (!strcmp(str, "continuousKaraoke") && !stricmp(parser.value_buffer, "yes") ) td.displayFlags |= GF_TXT_KARAOKE;
						else if (!strcmp(str, "scroll")) {
							if (!stricmp(parser.value_buffer, "inout")) td.displayFlags |= GF_TXT_SCROLL_IN | GF_TXT_SCROLL_OUT;
							else if (!stricmp(parser.value_buffer, "in")) td.displayFlags |= GF_TXT_SCROLL_IN;
							else if (!stricmp(parser.value_buffer, "out")) td.displayFlags |= GF_TXT_SCROLL_OUT;
						}
						else if (!strcmp(str, "scrollMode")) {
							u32 scroll_mode = GF_TXT_SCROLL_CREDITS;
							if (!stricmp(parser.value_buffer, "Credits")) scroll_mode = GF_TXT_SCROLL_CREDITS;
							else if (!stricmp(parser.value_buffer, "Marquee")) scroll_mode = GF_TXT_SCROLL_MARQUEE;
							else if (!stricmp(parser.value_buffer, "Right")) scroll_mode = GF_TXT_SCROLL_RIGHT;
							else if (!stricmp(parser.value_buffer, "Down")) scroll_mode = GF_TXT_SCROLL_DOWN;
							td.displayFlags |= ((scroll_mode<<7) & GF_TXT_SCROLL_DIRECTION);
						}
					}
					while (!xml_element_done(&parser, "TextSampleDescription")) {
						str = xml_get_element(&parser);
						if (!strcmp(str, "TextBox")) ttxt_parse_text_box(import, &parser, &td.default_pos);
						else if (!strcmp(str, "Style")) ttxt_parse_text_style(import, &parser, &td.default_style);
						else if (!strcmp(str, "FontTable")) {
							xml_skip_attributes(&parser);
							while (!xml_element_done(&parser, "FontTable")) {
								str = xml_get_element(&parser);
								if (!strcmp(str, "FontTableEntry")) {
									td.font_count += 1;
									td.fonts = realloc(td.fonts, sizeof(GF_FontRecord)*td.font_count);
									while (xml_has_attributes(&parser)) {
										str = xml_get_attribute(&parser);
										if (!stricmp(str, "fontID")) td.fonts[td.font_count-1].fontID = atoi(parser.value_buffer);
										else if (!stricmp(str, "fontName")) td.fonts[td.font_count-1].fontName = strdup(parser.value_buffer);
									}
									xml_skip_element(&parser, "FontTableEntry");
								}
								else xml_skip_element(&parser, str);
							}
						}
						else {
							xml_skip_element(&parser, str);
						}

					}
					if (import->flags & GF_IMPORT_SKIT_TXT_BOX) {
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
						td.fonts = malloc(sizeof(GF_FontRecord));
						td.fonts[0].fontID = 1;
						td.fonts[0].fontName = strdup("Serif");
					}
					gf_isom_new_text_description(import->dest, track, &td, NULL, NULL, &idx);
					for (i=0; i<td.font_count; i++) free(td.fonts[i].fontName);
					free(td.fonts);
					nb_descs ++;
				}
			}
		} else if (!strcmp(str, "TextSample")) {
			GF_ISOSample *s;
			GF_TextSample * samp;
			u32 ts, descIndex;
			
			if (!nb_descs) {
				e = gf_import_message(import, GF_BAD_PARAM, "Invalid Timed Text file - text stream header not found or empty");
				goto exit;
			}
			samp = gf_isom_new_text_sample();
			ts = 0;
			descIndex = 1;
			last_sample_empty = 0;

			while (xml_has_attributes(&parser)) {
				str = xml_get_attribute(&parser);
				if (!strcmp(str, "sampleTime")) {
					u32 h, m, s, ms;
					if (sscanf(parser.value_buffer, "%d:%d:%d.%d", &h, &m, &s, &ms) == 4) {
						ts = (h*3600 + m*60 + s)*1000 + ms;
					} else {
						ts = (u32) (atof(parser.value_buffer) * 1000);
					}
				}
				else if (!strcmp(str, "sampleDescriptionIndex")) descIndex = atoi(parser.value_buffer);
				else if (!strcmp(str, "text")) {
					u32 len;
					char *str = ttxt_parse_string(import, &parser);
					len = strlen(str);
					gf_isom_text_add_text(samp, str, len);
					free(str);
					last_sample_empty = len ? 0 : 1;
				}
				else if (!strcmp(str, "scrollDelay")) gf_isom_text_set_scroll_delay(samp, (u32) (1000*atoi(parser.value_buffer)));
				else if (!strcmp(str, "highlightColor")) gf_isom_text_set_highlight_color_argb(samp, ttxt_get_color(import, &parser));
				else if (!strcmp(str, "wrap") && !strcmp(parser.value_buffer, "Automatic")) gf_isom_text_set_wrap(samp, 0x01);
			}
			/*get all modifiers*/
			while (!xml_element_done(&parser, "TextSample")) {
				str = xml_get_element(&parser);
				if (!stricmp(str, "Style")) {
					GF_StyleRecord r;
					ttxt_parse_text_style(import, &parser, &r);
					gf_isom_text_add_style(samp, &r);
				}
				else if (!stricmp(str, "TextBox")) {
					GF_BoxRecord r;
					ttxt_parse_text_box(import, &parser, &r);
					gf_isom_text_set_box(samp, r.top, r.left, r.bottom, r.right);
				}
				else if (!stricmp(str, "Highlight")) {
					u16 start, end;
					start = end = 0;
					while (xml_has_attributes(&parser)) {
						str = xml_get_attribute(&parser);
						if (!strcmp(str, "fromChar")) start = atoi(parser.value_buffer);
						else if (!strcmp(str, "toChar")) end = atoi(parser.value_buffer);
					}
					xml_skip_element(&parser, "Highlight");
					gf_isom_text_add_highlight(samp, start, end);
				}
				else if (!stricmp(str, "Blinking")) {
					u16 start, end;
					start = end = 0;
					while (xml_has_attributes(&parser)) {
						str = xml_get_attribute(&parser);
						if (!strcmp(str, "fromChar")) start = atoi(parser.value_buffer);
						else if (!strcmp(str, "toChar")) end = atoi(parser.value_buffer);
					}
					xml_skip_element(&parser, "Blinking");
					gf_isom_text_add_blink(samp, start, end);
				}
				else if (!stricmp(str, "HyperLink")) {
					u16 start, end;
					char *url, *url_tt;
					start = end = 0;
					url = url_tt = NULL;
					while (xml_has_attributes(&parser)) {
						str = xml_get_attribute(&parser);
						if (!strcmp(str, "fromChar")) start = atoi(parser.value_buffer);
						else if (!strcmp(str, "toChar")) end = atoi(parser.value_buffer);
						else if (!strcmp(str, "URL")) url = strdup(parser.value_buffer);
						else if (!strcmp(str, "URLToolTip")) url_tt = strdup(parser.value_buffer);
					}
					xml_skip_element(&parser, "HyperLink");
					gf_isom_text_add_hyperlink(samp, url, url_tt, start, end);
					if (url) free(url);
					if (url_tt) free(url_tt);
				}
				else if (!stricmp(str, "Karaoke")) {
					u32 startTime;
					startTime = 0;
					while (xml_has_attributes(&parser)) {
						str = xml_get_attribute(&parser);
						if (!strcmp(str, "startTime")) startTime = (u32) (1000*atof(parser.value_buffer));
					}
					gf_isom_text_add_karaoke(samp, startTime);
					while (!xml_element_done(&parser, "Karaoke")) {
						str = xml_get_element(&parser);
						if (!strcmp(str, "KaraokeRange")) {
							u16 start, end;
							u32 endTime;
							start = end = 0;
							endTime = 0;
							while (xml_has_attributes(&parser)) {
								str = xml_get_attribute(&parser);
								if (!strcmp(str, "fromChar")) start = atoi(parser.value_buffer);
								else if (!strcmp(str, "toChar")) end = atoi(parser.value_buffer);
								else if (!strcmp(str, "endTime")) endTime = (u32) (1000*atof(parser.value_buffer));
							}
							xml_skip_element(&parser, "KaraokeRange");
							gf_isom_text_set_karaoke_segment(samp, endTime, start, end);
						} else {
							xml_skip_element(&parser, str);
						}
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

			gf_isom_add_sample(import->dest, track, descIndex, s);
			gf_isom_sample_del(&s);
			nb_samples++;

			gf_import_progress(import, parser.file_pos, parser.file_size);
			if (import->duration && (ts>import->duration)) break;
		} else {
			gf_import_message(import, GF_OK, "Unknown element %s - skipping", str);
			xml_skip_element(&parser, str);
		}
	}
	if (last_sample_empty) {
		gf_isom_remove_sample(import->dest, track, nb_samples);
		gf_isom_set_last_sample_duration(import->dest, track, (u32) last_sample_duration);
	}
	gf_import_progress(import, nb_samples, nb_samples);

exit:
	xml_reset_parser(&parser);
	return e;
}


u32 tx3g_get_color(GF_MediaImporter *import, XMLParser *parser)
{
	u32 r, g, b, a;
	u32 res, v;
	r = g = b = a = 0;
	if (sscanf(parser->value_buffer, "%d%%, %d%%, %d%%, %d%%", &r, &g, &b, &a) != 4) {
		gf_import_message(import, GF_OK, "Warning (line %d): color badly formatted", parser->line);
	}
	v = (u32) (a*255/100);
	res = (v&0xFF); res<<=8;
	v = (u32) (r*255/100);
	res |= (v&0xFF); res<<=8;
	v = (u32) (g*255/100);
	res |= (v&0xFF); res<<=8;
	v = (u32) (b*255/100);
	res |= (v&0xFF);
	return res;
}

void tx3g_parse_text_box(GF_MediaImporter *import, XMLParser *parser, GF_BoxRecord *box)
{
	memset(box, 0, sizeof(GF_BoxRecord));
	while (xml_has_attributes(parser)) {
		char *str = xml_get_attribute(parser);
		if (!stricmp(str, "x")) box->left = atoi(parser->value_buffer);
		else if (!stricmp(str, "y")) box->top = atoi(parser->value_buffer);
		else if (!stricmp(str, "height")) box->bottom = atoi(parser->value_buffer);
		else if (!stricmp(str, "width")) box->right = atoi(parser->value_buffer);
	}
	xml_skip_element(parser, "defaultTextBox");
}

typedef struct
{
	u32 id;
	u32 pos;
} Marker;

#define GET_MARKER_POS(_val, __isend) \
	{	\
		u32 i, __m = atoi(parser.value_buffer);	\
		_val = 0;	\
		for (i=0; i<nb_marks; i++) { if (__m==marks[i].id) { _val = marks[i].pos; /*if (__isend) _val--; */break; } }	 \
	}	


static GF_Err gf_text_import_texml(GF_MediaImporter *import)
{
	GF_Err e;
	u32 track, ID, nb_samples, nb_descs, timescale, w, h;
	u64 DTS;
	s32 tx, ty, layer;
	GF_StyleRecord styles[50];
	Marker marks[50];

	XMLParser parser;
	char *str;

	if (import->flags==GF_IMPORT_PROBE_ONLY) return GF_OK;

	e = xml_init_parser(&parser, import->in_name);
	if (e) return gf_import_message(import, e, "Cannot open file %s", import->in_name);

	str = xml_get_element(&parser);
	if (!str) { e = GF_IO_ERR; goto exit; }
	if (strcmp(str, "?quicktime")) {
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid QT TeXML file - expecting \"?quicktime\" header got %s", str);
		goto exit;
	}
	xml_skip_attributes(&parser);

	str = xml_get_element(&parser);
	if (strcmp(str, "text3GTrack")) {
		e = gf_import_message(import, GF_BAD_PARAM, "Invalid QT TeXML file - expecting %s got %s", "text3GTrack", str);
		goto exit;
	}
	w = TTXT_DEFAULT_WIDTH;
	h = TTXT_DEFAULT_HEIGHT;
	tx = ty = 0;
	layer = 0;
	timescale = 1000;
	while (xml_has_attributes(&parser)) {
		str = xml_get_attribute(&parser);
		if (!strcmp(str, "trackWidth")) w = atoi(parser.value_buffer);
		else if (!strcmp(str, "trackHeight")) h = atoi(parser.value_buffer);
		else if (!strcmp(str, "layer")) layer = atoi(parser.value_buffer);
		else if (!strcmp(str, "timescale")) timescale = atoi(parser.value_buffer);
		else if (!strcmp(str, "transform")) {
			Float fx, fy;
			sscanf(parser.value_buffer, "translate(%f,%f)", &fx, &fy);
			tx = (u32) fx; ty = (u32) fy;
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
		import->esd->decoderConfig->objectTypeIndication = 0x08;
		if (import->esd->OCRESID) gf_isom_set_track_reference(import->dest, track, GF_ISOM_REF_OCR, import->esd->OCRESID);
	}
	DTS = 0;
	gf_isom_set_track_layout_info(import->dest, track, w<<16, h<<16, tx<<16, ty<<16, (s16) layer);

	gf_text_import_set_language(import, track);

	gf_import_message(import, GF_OK, "Timed Text (QT TeXML) Import - Track Size %d x %d", w, h);
	parser.OnProgress = xml_import_progress;
	parser.cbk = import;


	nb_descs = 0;
	nb_samples = 0;
	while (!xml_element_done(&parser, "text3GTrack") && !parser.done) {
		GF_TextSampleDescriptor td;
		GF_TextSample * samp = NULL;
		GF_ISOSample *s;
		u32 duration, descIndex, nb_styles, nb_marks;
		Bool isRAP, same_style, same_box;
		str = xml_get_element(&parser);
		CHECK_STR(str)
		if (strcmp(str, "sample")) continue;

		isRAP = 0;
		duration = 1000;
		while (xml_has_attributes(&parser)) {
			str = xml_get_attribute(&parser);
			if (!strcmp(str, "duration")) duration = atoi(parser.value_buffer);
			else if (!strcmp(str, "keyframe")) isRAP = !stricmp(parser.value_buffer, "true");
		}

		nb_styles = 0;
		nb_marks = 0;
		same_style = same_box = 0;
		descIndex = 1;
		while (!xml_element_done(&parser, "sample") && !parser.done) {
			str = xml_get_element(&parser);
			CHECK_STR(str)
			if (!strcmp(str, "description")) {
				u32 i;
				memset(&td, 0, sizeof(GF_TextSampleDescriptor));
				td.tag = GF_ODF_TEXT_CFG_TAG;
				td.vert_justif = -1;
				td.default_style.fontID = 1;
				td.default_style.font_size = TTXT_DEFAULT_FONT_SIZE;

				while (xml_has_attributes(&parser)) {
					str = xml_get_attribute(&parser);
					if (!strcmp(str, "horizontalJustification")) {
						if (!stricmp(parser.value_buffer, "center")) td.horiz_justif = 1;
						else if (!stricmp(parser.value_buffer, "right")) td.horiz_justif = -1;
						else if (!stricmp(parser.value_buffer, "left")) td.horiz_justif = 0;
					}
					else if (!strcmp(str, "verticalJustification")) {
						if (!stricmp(parser.value_buffer, "center")) td.vert_justif = 1;
						else if (!stricmp(parser.value_buffer, "bottom")) td.vert_justif = -1;
						else if (!stricmp(parser.value_buffer, "top")) td.vert_justif = 0;
					}
					else if (!strcmp(str, "backgroundColor")) td.back_color = tx3g_get_color(import, &parser);
					else if (!strcmp(str, "displayFlags")) {
						Bool rev_scroll = 0;
						if (strstr(parser.value_buffer, "scroll")) {
							u32 scroll_mode = 0;
							if (strstr(parser.value_buffer, "scrollIn")) td.displayFlags |= GF_TXT_SCROLL_IN;
							if (strstr(parser.value_buffer, "scrollOut")) td.displayFlags |= GF_TXT_SCROLL_OUT;
							if (strstr(parser.value_buffer, "reverse")) rev_scroll = 1;
							if (strstr(parser.value_buffer, "horizontal")) scroll_mode = rev_scroll ? GF_TXT_SCROLL_RIGHT : GF_TXT_SCROLL_MARQUEE;
							else scroll_mode = (rev_scroll ? GF_TXT_SCROLL_DOWN : GF_TXT_SCROLL_CREDITS);
							td.displayFlags |= (scroll_mode<<7) & GF_TXT_SCROLL_DIRECTION;
						}
						if (strstr(parser.value_buffer, "writeTextVertically")) td.displayFlags |= GF_TXT_VERTICAL;
						if (!strcmp(str, "continuousKaraoke")) td.displayFlags |= GF_TXT_KARAOKE;
					}
				}
				while (!xml_element_done(&parser, "description")) {
					str = xml_get_element(&parser);
					if (!strcmp(str, "defaultTextBox")) tx3g_parse_text_box(import, &parser, &td.default_pos);
					else if (!strcmp(str, "fontTable")) {
						xml_skip_attributes(&parser);
						while (!xml_element_done(&parser, "FontTable")) {
							str = xml_get_element(&parser);
							if (!strcmp(str, "font")) {
								td.font_count += 1;
								td.fonts = realloc(td.fonts, sizeof(GF_FontRecord)*td.font_count);
								while (xml_has_attributes(&parser)) {
									str = xml_get_attribute(&parser);
									if (!stricmp(str, "id")) td.fonts[td.font_count-1].fontID = atoi(parser.value_buffer);
									else if (!stricmp(str, "name")) td.fonts[td.font_count-1].fontName = strdup(parser.value_buffer);
								}
								xml_skip_element(&parser, "font");
							}
							else xml_skip_element(&parser, str);
						}
					}
					else if (!strcmp(str, "sharedStyles")) {
						u32 idx = 0;
						xml_skip_attributes(&parser);
						while (!xml_element_done(&parser, "SharedStyles")) {
							str = xml_get_element(&parser);
							if (!strcmp(str, "style")) {
								memset(&styles[nb_styles], 0, sizeof(GF_StyleRecord));
								while (xml_has_attributes(&parser)) {
									str = xml_get_attribute(&parser);
									if (!strcmp(str, "id")) styles[nb_styles].startCharOffset = atoi(parser.value_buffer);
								}
								while (!xml_element_done(&parser, "style")) {
									str = xml_get_css(&parser);
									if (!strcmp(str, "font-table")) {
										styles[nb_styles].fontID = atoi(parser.value_buffer);
										for (i=0; i<td.font_count; i++) { if (td.fonts[i].fontID == styles[nb_styles].fontID) { idx = i; break; } }
									}
									else if (!strcmp(str, "font-size")) styles[nb_styles].font_size = atoi(parser.value_buffer);
									else if (!strcmp(str, "font-style") && !strcmp(parser.value_buffer, "italic")) styles[nb_styles].style_flags |= GF_TXT_STYLE_ITALIC;
									else if (!strcmp(str, "font-weight") && !strcmp(parser.value_buffer, "bold")) styles[nb_styles].style_flags |= GF_TXT_STYLE_BOLD;
									else if (!strcmp(str, "text-decoration") && !strcmp(parser.value_buffer, "underline")) styles[nb_styles].style_flags |= GF_TXT_STYLE_UNDERLINED;
									else if (!strcmp(str, "color")) styles[nb_styles].text_color = tx3g_get_color(import, &parser);
								}
								if (!nb_styles) td.default_style = styles[0];
								nb_styles++;
							}
							else xml_skip_element(&parser, str);
						}
					}
					else {
						xml_skip_element(&parser, str);
					}

				}
				if ((td.default_pos.bottom==td.default_pos.top) || (td.default_pos.right==td.default_pos.left)) {
					td.default_pos.top = td.default_pos.left = 0;
					td.default_pos.right = w;
					td.default_pos.bottom = h;
				}
				if (!td.fonts) {
					td.font_count = 1;
					td.fonts = malloc(sizeof(GF_FontRecord));
					td.fonts[0].fontID = 1;
					td.fonts[0].fontName = strdup("Serif");
				}
				gf_isom_text_has_similar_description(import->dest, track, &td, &descIndex, &same_box, &same_style);
				if (!descIndex) {
					gf_isom_new_text_description(import->dest, track, &td, NULL, NULL, &descIndex);
					same_style = same_box = 1;
				}

				for (i=0; i<td.font_count; i++) free(td.fonts[i].fontName);
				free(td.fonts);
				nb_descs ++;
			} else if (!strcmp(str, "sampleData")) {
				Bool is_utf16 = 0;
				u16 start, end;
				u32 styleID, i;
				u32 nb_chars, txt_len;
				txt_len = nb_chars = 0;

				samp = gf_isom_new_text_sample();

				while (xml_has_attributes(&parser)) {
					str = xml_get_attribute(&parser);
					if (!strcmp(str, "targetEncoding") && !strcmp(parser.value_buffer, "utf16")) is_utf16 = 1;
					else if (!strcmp(str, "scrollDelay")) gf_isom_text_set_scroll_delay(samp, atoi(parser.value_buffer) );
					else if (!strcmp(str, "highlightColor")) gf_isom_text_set_highlight_color_argb(samp, tx3g_get_color(import, &parser));
				}
				start = end = 0;
				while (!xml_element_done(&parser, "sampleData")) {
					str = xml_get_element(&parser);
					if (!strcmp(str, "text")) {
						styleID = 0;
						while (xml_has_attributes(&parser)) {
							str = xml_get_attribute(&parser);
							if (!strcmp(str, "styleID")) styleID = atoi(parser.value_buffer);
						}
						xml_set_text_mode(&parser, 1);
						txt_len = 0;
						while (!xml_element_done(&parser, "text")) {
							str = xml_get_element(&parser);
							if (str) {
								if (!strcmp(str, "marker")) {
									memset(&marks[nb_marks], 0, sizeof(Marker));
									marks[nb_marks].pos = nb_chars+txt_len;
									while (xml_has_attributes(&parser)) {
										str = xml_get_attribute(&parser);
										if (!strcmp(str, "id")) marks[nb_marks].id = atoi(parser.value_buffer);
									}
									xml_skip_element(&parser, "marker");
									nb_marks++;
								}
								else xml_skip_element(&parser, str);
							} else {
								while (xml_load_text(&parser)) {
									char *str = ttxt_parse_string(import, &parser);
									txt_len += strlen(str);
									gf_isom_text_add_text(samp, str, strlen(str));
									free(str);
								}
							}
						}
						xml_set_text_mode(&parser, 0);
						if (styleID && (!same_style || (td.default_style.startCharOffset != styleID))) {
							GF_StyleRecord st = td.default_style;
							for (i=0; i<nb_styles; i++) { if (styles[i].startCharOffset==styleID) { st = styles[i]; break; } }
							st.startCharOffset = nb_chars;
							st.endCharOffset = nb_chars + txt_len;
							gf_isom_text_add_style(samp, &st);
						}
						nb_chars += txt_len;
					}
					else if (!stricmp(str, "highlight")) {
						while (xml_has_attributes(&parser)) {
							str = xml_get_attribute(&parser);
							if (!strcmp(str, "startMarker")) GET_MARKER_POS(start, 0)
							else if (!strcmp(str, "endMarker")) GET_MARKER_POS(end, 1)
						}
						xml_skip_element(&parser, "highlight");
						gf_isom_text_add_highlight(samp, start, end);
					}
					else if (!stricmp(str, "blink")) {
						while (xml_has_attributes(&parser)) {
							str = xml_get_attribute(&parser);
							if (!strcmp(str, "startMarker")) GET_MARKER_POS(start, 0)
							else if (!strcmp(str, "endMarker")) GET_MARKER_POS(end, 1)
						}
						xml_skip_element(&parser, "blink");
						gf_isom_text_add_blink(samp, start, end);
					}
					else if (!stricmp(str, "link")) {
						char *url, *url_tt;
						url = url_tt = NULL;
						while (xml_has_attributes(&parser)) {
							str = xml_get_attribute(&parser);
							if (!strcmp(str, "startMarker")) GET_MARKER_POS(start, 0)
							else if (!strcmp(str, "endMarker")) GET_MARKER_POS(end, 1)
							else if (!strcmp(str, "URL") || !strcmp(str, "href")) url = strdup(parser.value_buffer);
							else if (!strcmp(str, "URLToolTip") || !strcmp(str, "altString")) url_tt = strdup(parser.value_buffer);
						}
						xml_skip_element(&parser, "link");
						gf_isom_text_add_hyperlink(samp, url, url_tt, start, end);
						if (url) free(url);
						if (url_tt) free(url_tt);
					}
					else if (!stricmp(str, "karaoke")) {
						u32 time = 0;
						while (xml_has_attributes(&parser)) {
							str = xml_get_attribute(&parser);
							if (!strcmp(str, "startTime")) time = atoi(parser.value_buffer);
						}
						gf_isom_text_add_karaoke(samp, time);
						while (!xml_element_done(&parser, "karaoke")) {
							str = xml_get_element(&parser);
							if (!strcmp(str, "run")) {
								start = end = 0;
								while (xml_has_attributes(&parser)) {
									str = xml_get_attribute(&parser);
									if (!strcmp(str, "startMarker")) GET_MARKER_POS(start, 0)
									else if (!strcmp(str, "endMarker")) GET_MARKER_POS(end, 1)
									else if (!strcmp(str, "duration")) time += atoi(parser.value_buffer);
								}
								xml_skip_element(&parser, "run");
								gf_isom_text_set_karaoke_segment(samp, time, start, end);
							} else {
								xml_skip_element(&parser, str);
							}
						}
					}
					else xml_skip_element(&parser, str);
				}
			}
		}
		/*OK, let's add the sample*/
		if (samp) {
			if (!same_box) gf_isom_text_set_box(samp, td.default_pos.top, td.default_pos.left, td.default_pos.bottom, td.default_pos.right);
//			if (!same_style) gf_isom_text_add_style(samp, &td.default_style);

			s = gf_isom_text_to_sample(samp);
			gf_isom_delete_text_sample(samp);
			s->IsRAP = isRAP;
			s->DTS = DTS;
			gf_isom_add_sample(import->dest, track, descIndex, s);
			gf_isom_sample_del(&s);
			nb_samples++;
			DTS += duration;
			if (import->duration && (DTS*1000> timescale*import->duration)) break;
		}
	}
	gf_isom_set_last_sample_duration(import->dest, track, 0);
	gf_import_progress(import, nb_samples, nb_samples);

exit:
	xml_reset_parser(&parser);
	return e;
}


GF_Err gf_import_timed_text(GF_MediaImporter *import)
{
	GF_Err e;
	u32 fmt;
	e = gf_text_guess_format(import->in_name, &fmt);
	if (e) return e;
	if (!fmt) return GF_NOT_SUPPORTED;
	if (import->flags & GF_IMPORT_PROBE_ONLY) {
		if (fmt==GF_TEXT_IMPORT_SUB) import->flags |= GF_IMPORT_OVERRIDE_FPS;
		return GF_OK;
	}
	switch (fmt) {
	case GF_TEXT_IMPORT_SRT: return gf_text_import_srt(import);
	case GF_TEXT_IMPORT_SUB: return gf_text_import_sub(import);
	case GF_TEXT_IMPORT_TTXT: return gf_text_import_ttxt(import);
	case GF_TEXT_IMPORT_TEXML: return gf_text_import_texml(import);
	default: return GF_BAD_PARAM;
	}
}

#endif

