/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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
#include <gpac/utf.h>
#include <gpac/constants.h>
#include <gpac/network.h>
#include <gpac/internal/bifs_dev.h>
#include <gpac/internal/scenegraph_dev.h>

#include <gpac/nodes_x3d.h>
/*for key codes...*/
#include <gpac/user.h>
#include <gpac/color.h>


#if !defined(GPAC_DISABLE_LOADER_BT) && !defined(GPAC_DISABLE_ZLIB)

#include <gpac/mpeg4_odf.h>

/*since 0.2.2, we use zlib for bt reading to handle wrl.gz files*/
#include <zlib.h>

void gf_sm_update_bitwrapper_buffer(GF_Node *node, const char *fileName);

void load_bt_done(GF_SceneLoader *load);

#define BT_LINE_SIZE	4000

typedef struct
{
	char *name;
	char *value;
} BTDefSymbol;

typedef struct
{
	GF_SceneLoader *load;
	Bool initialized;
	gzFile gz_in;
	u32 file_size, file_pos;

	/*create from string only*/
	GF_List *top_nodes;

	GF_Err last_error;
	u32 line;

	Bool done, in_com;
	u32 is_wrl;
	/*0: no unicode, 1: UTF-16BE, 2: UTF-16LE*/
	u32 unicode_type;

	GF_List *def_symbols;

	/*routes are not created in the graph when parsing, so we need to track insert and delete/replace*/
	GF_List *unresolved_routes, *inserted_routes, *peeked_nodes;
	GF_List *undef_nodes, *def_nodes;

	char *line_buffer;
	char cur_buffer[500];
	s32 line_size, line_pos, line_start_pos;

	u32 block_comment;

	/*set when parsing proto*/
	GF_Proto *parsing_proto;
	Bool is_extern_proto_field;

	/*current stream ID, AU time and RAP flag*/
	u32 stream_id;
	u32 au_time;
	Bool au_is_rap;

	/*current BIFS stream & AU*/
	GF_StreamContext *bifs_es;
	GF_AUContext *bifs_au;
	u32 base_bifs_id;
	GF_Command *cur_com;

	/*current OD stream & AU*/
	GF_StreamContext *od_es;
	GF_AUContext *od_au;
	u32 base_od_id;

	GF_List *scripts;

	u32 def_w, def_h;

} GF_BTParser;

GF_Err gf_bt_parse_bifs_command(GF_BTParser *parser, char *name, GF_List *cmdList);
GF_Route *gf_bt_parse_route(GF_BTParser *parser, Bool skip_def, Bool is_insert, GF_Command *com);
void gf_bt_resolve_routes(GF_BTParser *parser, Bool clean);

GF_Node *gf_bt_peek_node(GF_BTParser *parser, char *defID);

static GF_Err gf_bt_report(GF_BTParser *parser, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (format && gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(szMsg, 2048, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[BT/WRL Parsing] %s (line %d)\n", szMsg, parser->line));
	}
#endif
	if (e) parser->last_error = e;
	return e;
}


void gf_bt_check_line(GF_BTParser *parser)
{
	while (1) {
		switch (parser->line_buffer[parser->line_pos]) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			parser->line_pos++;
			continue;
		default:
			break;
		}
		break;
	}

	if (parser->line_buffer[parser->line_pos]=='#') {
		parser->line_size = parser->line_pos;
	}
	else if ((parser->line_buffer[parser->line_pos]=='/') && (parser->line_buffer[parser->line_pos+1]=='/') ) parser->line_size = parser->line_pos;

	if (parser->line_size == parser->line_pos) {
		/*string based input - done*/
		if (!parser->gz_in) {
			parser->done = 1;
			return;
		}

next_line:
		parser->line_start_pos = (s32) gf_gztell(parser->gz_in);
		parser->line_buffer[0] = 0;
		if (parser->unicode_type) {
			u8 c1, c2;
			unsigned short wchar;
			unsigned short l[BT_LINE_SIZE];
			unsigned short *dst = l;
			Bool is_ret = 0;
			u32 last_space_pos, last_space_pos_stream;
			u32 go = BT_LINE_SIZE - 1;
			last_space_pos = last_space_pos_stream = 0;
			while (go && !gf_gzeof(parser->gz_in) ) {
				c1 = gf_gzgetc(parser->gz_in);
				c2 = gf_gzgetc(parser->gz_in);
				/*Little-endian order*/
				if (parser->unicode_type==2) {
					if (c2) {
						wchar = c2;
						wchar <<=8;
						wchar |= c1;
					}
					else wchar = c1;
				} else {
					wchar = c1;
					if (c2) {
						wchar <<= 8;
						wchar |= c2;
					}
				}
				*dst = wchar;
				if (wchar=='\r') is_ret = 1;
				else if (wchar=='\n') {
					dst++;
					break;
				}
				else if (is_ret && wchar!='\n') {
					u32 fpos = (u32) gf_gztell(parser->gz_in);
					gf_gzseek(parser->gz_in, fpos-2, SEEK_SET);
					break;
				}
				if (wchar==' ') {
					//last_space_pos_stream = (u32) gf_gztell(parser->gz_in);
					last_space_pos = (u32) (dst - l);
				}
				dst++;
				go--;

			}
			*dst = 0;
			/*long line, rewind stream to last space*/
			if (!go) {
				u32 rew_pos = (u32)  (gf_gztell(parser->gz_in) - 2*(dst - &l[last_space_pos]) );
				gf_gzseek(parser->gz_in, rew_pos, SEEK_SET);
				l[last_space_pos+1] = 0;
			}
			/*check eof*/
			if (l[0]==0xFFFF) {
				parser->done = 1;
				return;
			}
			/*convert to mbc string*/
			dst = l;
			gf_utf8_wcstombs(parser->line_buffer, BT_LINE_SIZE, (const unsigned short **) &dst);

			if (!strlen(parser->line_buffer) && gf_gzeof(parser->gz_in)) {
				parser->done = 1;
				return;
			}
		} else {
			if ((gf_gzgets(parser->gz_in, parser->line_buffer, BT_LINE_SIZE) == NULL)
			        || (!strlen(parser->line_buffer) && gf_gzeof(parser->gz_in))) {
				parser->done = 1;
				return;
			}
			/*watchout for long lines*/
			if (1 + strlen(parser->line_buffer) == BT_LINE_SIZE) {
				u32 rew, pos, go;
				rew = 0;
				go = 1;
				while (go) {
					switch (parser->line_buffer[strlen(parser->line_buffer)-1]) {
					case ' ':
					case ',':
					case '[':
					case ']':
						go = 0;
						break;
					default:
						parser->line_buffer[strlen(parser->line_buffer)-1] = 0;
						rew++;
						break;
					}
				}
				pos = (u32) gf_gztell(parser->gz_in);
				gf_gzseek(parser->gz_in, pos-rew, SEEK_SET);
			}
		}


		while (1) {
			char c;
			u32 len = (u32) strlen(parser->line_buffer);
			if (!len) break;
			c = parser->line_buffer[len-1];
			if (!strchr("\n\r\t", c)) break;
			parser->line_buffer[len-1] = 0;
		}


		parser->line_size = (u32) strlen(parser->line_buffer);
		parser->line_pos = 0;
		parser->line++;

		{
			u32 pos = (u32) gf_gztell(parser->gz_in);
			if (pos>=parser->file_pos) {
				parser->file_pos = pos;
				if (parser->line>1) gf_set_progress("BT Parsing", pos, parser->file_size);
			}
		}

		while ((parser->line_buffer[parser->line_pos]==' ') || (parser->line_buffer[parser->line_pos]=='\t'))
			parser->line_pos++;
		if ( (parser->line_buffer[parser->line_pos]=='#')
		        || ( (parser->line_buffer[parser->line_pos]=='/') && (parser->line_buffer[parser->line_pos+1]=='/')) ) {

			if (parser->line==1) {
				if (strstr(parser->line_buffer, "VRML")) {
					if (strstr(parser->line_buffer, "VRML V2.0")) parser->is_wrl = 1;
					/*although not std, many files use this*/
					else if (strstr(parser->line_buffer, "VRML2.0")) parser->is_wrl = 1;
					else {
						gf_bt_report(parser, GF_NOT_SUPPORTED, "%s: VRML Version Not Supported", parser->line_buffer);
						return;
					}
				}
				else if (strstr(parser->line_buffer, "X3D")) {
					if (strstr(parser->line_buffer, "X3D V3.0")) parser->is_wrl = 2;
					else {
						gf_bt_report(parser, GF_NOT_SUPPORTED, "%s: X3D Version Not Supported", parser->line_buffer);
						return;
					}
				}
			}
			if (!strnicmp(parser->line_buffer+parser->line_pos, "#define ", 8) && !parser->block_comment) {
				char *buf, *sep;
				parser->line_pos+=8;
				buf = parser->line_buffer+parser->line_pos;
				sep = strchr(buf, ' ');
				if (sep && (sep[1]!='\n') ) {
					BTDefSymbol *def;
					GF_SAFEALLOC(def, BTDefSymbol);
					if (!def) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("Fail to allocate DEF node\n"));
						return;
					}
					sep[0] = 0;
					def->name = gf_strdup(buf);
					sep[0] = ' ';
					buf = sep+1;
					while (strchr(" \t", buf[0])) buf++;
					def->value = gf_strdup(buf);
					gf_list_add(parser->def_symbols, def);
				}
			}
			else if (!strnicmp(parser->line_buffer+parser->line_pos, "#if ", 4)) {
				u32 len = 0;
				parser->line_pos+=4;
				while (1) {
					if (parser->line_pos+(s32)len==parser->line_size) break;
					if (strchr(" \n\t", parser->line_buffer[parser->line_pos+len]))
						break;
					len++;
				}
				if (len) {
					if (len==1) {
						if (!strnicmp(parser->line_buffer+parser->line_pos, "0", len)) {
							parser->block_comment++;
						}
					} else {
						u32 i, count;
						char *keyWord = NULL;
						count = gf_list_count(parser->def_symbols);
						for (i=0; i<count; i++) {
							BTDefSymbol *def = (BTDefSymbol *)gf_list_get(parser->def_symbols, i);
							if (!strnicmp(parser->line_buffer+parser->line_pos, def->name, len)) {
								keyWord = def->value;
								break;
							}
						}
						if (keyWord && !strcmp(keyWord, "0")) {
							parser->block_comment++;
						}
					}
				}
			}
			else if (!strnicmp(parser->line_buffer+parser->line_pos, "#endif", 6)) {
				if (parser->block_comment) parser->block_comment--;
			}
			else if (!strnicmp(parser->line_buffer+parser->line_pos, "#else", 5)) {
				if (parser->block_comment)
					parser->block_comment--;
				else
					parser->block_comment++;
			}
			else if (!strnicmp(parser->line_buffer+parser->line_pos, "#size", 5)) {
				char *buf;
				parser->line_pos+=6;
				buf = parser->line_buffer+parser->line_pos;
				while (strchr(" \t", buf[0]))
					buf++;
				sscanf(buf, "%dx%d", &parser->def_w, &parser->def_h);
			}
			goto next_line;
		}

		if (parser->block_comment)
			goto next_line;

		/*brute-force replacement of defined symbols (!!FIXME - no mem checking done !!)*/
		if (parser->line_pos < parser->line_size) {
			u32 i, count;
			count = gf_list_count(parser->def_symbols);
			while (1) {
				Bool found = 0;
				for (i=0; i<count; i++) {
					u32 symb_len, val_len, copy_len;
					BTDefSymbol *def = (BTDefSymbol *)gf_list_get(parser->def_symbols, i);
					char *start = strstr(parser->line_buffer, def->name);
					if (!start) continue;
					symb_len = (u32) strlen(def->name);
					if (!strchr(" \n\r\t,[]{}\'\"", start[symb_len])) continue;
					val_len = (u32) strlen(def->value);
					copy_len = (u32) strlen(start + symb_len) + 1;
					memmove(start + val_len, start + symb_len, sizeof(char)*copy_len);
					memcpy(start, def->value, sizeof(char)*val_len);
					parser->line_size = (u32) strlen(parser->line_buffer);
					found = 1;
				}
				if (!found) break;
			}
		}
	}
	if (!parser->line_size) {
		if (!gf_gzeof(parser->gz_in)) gf_bt_check_line(parser);
		else parser->done = 1;
	}
	else if (!parser->done && (parser->line_size == parser->line_pos)) gf_bt_check_line(parser);
}

void gf_bt_force_line(GF_BTParser *parser)
{
	parser->line_pos = parser->line_size;
}

Bool gf_bt_check_code(GF_BTParser *parser, char code)
{
	gf_bt_check_line(parser);
	if (parser->line_buffer[parser->line_pos]==code) {
		parser->line_pos++;
		return 1;
	}
	return 0;
}

char *gf_bt_get_next(GF_BTParser *parser, Bool point_break)
{
	u32 has_quote;
	Bool go = 1;
	s32 i;
	gf_bt_check_line(parser);
	i=0;
	has_quote = 0;
	while (go) {
		if (parser->line_buffer[parser->line_pos + i] == '\"') {
			if (!has_quote) has_quote = 1;
			else has_quote = 0;
			parser->line_pos += 1;

			if (parser->line_pos+i==parser->line_size) break;
			continue;
		}
		if (!has_quote) {
			switch (parser->line_buffer[parser->line_pos + i]) {
			case 0:
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			case '{':
			case '}':
			case ']':
			case '[':
			case ',':
				go = 0;
				break;
			case '.':
				if (point_break) go = 0;
				break;
			}
			if (!go) break;
		}
		parser->cur_buffer[i] = parser->line_buffer[parser->line_pos + i];
		i++;
		if (parser->line_pos+i==parser->line_size) break;
	}
	parser->cur_buffer[i] = 0;
	parser->line_pos += i;
	return parser->cur_buffer;
}

char *gf_bt_get_string(GF_BTParser *parser, u8 string_delim)
{
	char *res;
	s32 i, size;

#define	BT_STR_CHECK_ALLOC	\
		if (i==size) {		\
			res = (char*)gf_realloc(res, sizeof(char) * (size+500+1));	\
			size += 500;	\
		}	\

	res = (char*)gf_malloc(sizeof(char) * 500);
	size = 500;
	while (parser->line_buffer[parser->line_pos]==' ') parser->line_pos++;

	if (parser->line_pos==parser->line_size) {
		if (gf_gzeof(parser->gz_in)) return NULL;
		gf_bt_check_line(parser);
	}
	if (!string_delim) string_delim = '"';

	i=0;
	while (1) {
		if (parser->line_buffer[parser->line_pos] == string_delim)
			if ( !parser->line_pos || (parser->line_buffer[parser->line_pos-1] != '\\') ) break;

		BT_STR_CHECK_ALLOC

		if ((parser->line_buffer[parser->line_pos]=='/') && (parser->line_buffer[parser->line_pos+1]=='/') && (parser->line_buffer[parser->line_pos-1]!=':') ) {
			/*this looks like a comment*/
			if (!strchr(&parser->line_buffer[parser->line_pos], string_delim)) {
				gf_bt_check_line(parser);
				continue;
			}
		}
		if ((parser->line_buffer[parser->line_pos] != '\\') || (parser->line_buffer[parser->line_pos+1] != string_delim)) {
			/*handle UTF-8 - WARNING: if parser is in unicode string is already utf8 multibyte chars*/
			if (!parser->unicode_type && parser->line_buffer[parser->line_pos] & 0x80) {
				char c = parser->line_buffer[parser->line_pos];
				/*non UTF8 (likely some win-CP)*/
				if ( (parser->line_buffer[parser->line_pos+1] & 0xc0) != 0x80) {
					res[i] = 0xc0 | ( (parser->line_buffer[parser->line_pos] >> 6) & 0x3 );
					i++;
					BT_STR_CHECK_ALLOC
					parser->line_buffer[parser->line_pos] &= 0xbf;
				}
				/*UTF8 2 bytes char*/
				else if ( (c & 0xe0) == 0xc0) {
					res[i] = parser->line_buffer[parser->line_pos];
					parser->line_pos++;
					i++;
					BT_STR_CHECK_ALLOC
				}
				/*UTF8 3 bytes char*/
				else if ( (c & 0xf0) == 0xe0) {
					res[i] = parser->line_buffer[parser->line_pos];
					parser->line_pos++;
					i++;
					BT_STR_CHECK_ALLOC
					res[i] = parser->line_buffer[parser->line_pos];
					parser->line_pos++;
					i++;
					BT_STR_CHECK_ALLOC
				}
				/*UTF8 4 bytes char*/
				else if ( (c & 0xf8) == 0xf0) {
					res[i] = parser->line_buffer[parser->line_pos];
					parser->line_pos++;
					i++;
					BT_STR_CHECK_ALLOC
					res[i] = parser->line_buffer[parser->line_pos];
					parser->line_pos++;
					i++;
					BT_STR_CHECK_ALLOC
					res[i] = parser->line_buffer[parser->line_pos];
					parser->line_pos++;
					i++;
					BT_STR_CHECK_ALLOC
				}
			}

			res[i] = parser->line_buffer[parser->line_pos];
			i++;
		}
		parser->line_pos++;
		if (parser->line_pos==parser->line_size) {
			gf_bt_check_line(parser);
		}

	}

#undef	BT_STR_CHECK_ALLOC

	res[i] = 0;
	parser->line_pos += 1;
	return res;
}

Bool gf_bt_check_externproto_field(GF_BTParser *parser, char *str)
{
	if (!parser->is_extern_proto_field) return 0;
	if (!strcmp(str, "") || !strcmp(str, "field") || !strcmp(str, "eventIn") || !strcmp(str, "eventOut") || !strcmp(str, "exposedField")) {
		parser->last_error = GF_EOS;
		return 1;
	}
	return 0;
}

static Bool check_keyword(GF_BTParser *parser, char *str, s32 *val)
{
	s32 res;
	char *sep;
	sep = strchr(str, '$');
	if (!sep) return 0;
	sep++;
	if (!strcmp(sep, "F1")) res = GF_KEY_F1;
	else if (!strcmp(sep, "F2")) res = GF_KEY_F2;
	else if (!strcmp(sep, "F3")) res = GF_KEY_F3;
	else if (!strcmp(sep, "F4")) res = GF_KEY_F4;
	else if (!strcmp(sep, "F5")) res = GF_KEY_F5;
	else if (!strcmp(sep, "F6")) res = GF_KEY_F6;
	else if (!strcmp(sep, "F7")) res = GF_KEY_F7;
	else if (!strcmp(sep, "F8")) res = GF_KEY_F8;
	else if (!strcmp(sep, "F9")) res = GF_KEY_F9;
	else if (!strcmp(sep, "F10")) res = GF_KEY_F10;
	else if (!strcmp(sep, "F11")) res = GF_KEY_F11;
	else if (!strcmp(sep, "F12")) res = GF_KEY_F12;
	else if (!strcmp(sep, "HOME")) res = GF_KEY_HOME;
	else if (!strcmp(sep, "END")) res = GF_KEY_END;
	else if (!strcmp(sep, "PREV")) res = GF_KEY_PAGEUP;
	else if (!strcmp(sep, "NEXT")) res = GF_KEY_PAGEDOWN;
	else if (!strcmp(sep, "UP")) res = GF_KEY_UP;
	else if (!strcmp(sep, "DOWN")) res = GF_KEY_DOWN;
	else if (!strcmp(sep, "LEFT")) res = GF_KEY_LEFT;
	else if (!strcmp(sep, "RIGHT")) res = GF_KEY_RIGHT;
	else if (!strcmp(sep, "RETURN")) res = GF_KEY_ENTER;
	else if (!strcmp(sep, "BACK")) res = GF_KEY_BACKSPACE;
	else if (!strcmp(sep, "TAB")) res = GF_KEY_TAB;
	else if (strlen(sep)==1) {
		char c;
		sscanf(sep, "%c", &c);
		res = c;
	} else {
		gf_bt_report(parser, GF_OK, "unrecognized keyword %s - skipping", str);
		res = 0;
	}
	if (strchr(str, '-')) *val = -res;
	else *val = res;
	return 1;
}

GF_Err gf_bt_parse_float(GF_BTParser *parser, const char *name, Fixed *val)
{
	s32 var;
	Float f;
	char *str = gf_bt_get_next(parser, 0);
	if (!str) return parser->last_error = GF_IO_ERR;
	if (gf_bt_check_externproto_field(parser, str)) return GF_OK;

	if (check_keyword(parser, str, &var)) {
		*val = INT2FIX(var);
		return GF_OK;
	}
	if (sscanf(str, "%g", &f) != 1) {
		return gf_bt_report(parser, GF_BAD_PARAM, "%s: Number expected", name);
	}
	*val = FLT2FIX(f);
	return GF_OK;
}
GF_Err gf_bt_parse_double(GF_BTParser *parser, const char *name, SFDouble *val)
{
	char *str = gf_bt_get_next(parser, 0);
	if (!str) return parser->last_error = GF_IO_ERR;
	if (gf_bt_check_externproto_field(parser, str)) return GF_OK;
	if (sscanf(str, "%lf", val) != 1) {
		return gf_bt_report(parser, GF_BAD_PARAM, "%s: Number expected", name);
	}
	return GF_OK;
}
GF_Err gf_bt_parse_int(GF_BTParser *parser, const char *name, SFInt32 *val)
{
	char *str = gf_bt_get_next(parser, 0);
	if (!str) return parser->last_error = GF_IO_ERR;
	if (gf_bt_check_externproto_field(parser, str)) return GF_OK;

	if (check_keyword(parser, str, val)) return GF_OK;
	/*URL ODID*/
	if (!strnicmp(str, "od:", 3)) str += 3;
	if (sscanf(str, "%d", val) != 1) {
		return gf_bt_report(parser, GF_BAD_PARAM, "%s: Number expected", name);
	}
	return GF_OK;
}
GF_Err gf_bt_parse_bool(GF_BTParser *parser, const char *name, SFBool *val)
{
	char *str = gf_bt_get_next(parser, 0);
	if (!str) return parser->last_error = GF_IO_ERR;
	if (gf_bt_check_externproto_field(parser, str)) return GF_OK;

	if (!stricmp(str, "true") || !strcmp(str, "1") ) {
		*val = 1;
	}
	else if (!stricmp(str, "false") || !strcmp(str, "0") ) {
		*val = 0;
	} else {
		return gf_bt_report(parser, GF_BAD_PARAM, "%s: Boolean expected", name);
	}
	return GF_OK;
}

GF_Err gf_bt_parse_color(GF_BTParser *parser, const char *name, SFColor *col)
{
	Float f;
	u32 val;
	char *str = gf_bt_get_next(parser, 0);
	if (!str) return parser->last_error = GF_IO_ERR;
	if (gf_bt_check_externproto_field(parser, str)) return GF_OK;

	if (sscanf(str, "%f", &f) == 1) {
		col->red = FLT2FIX(f);
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, name, & col->green);
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, name, & col->blue);
		return parser->last_error;
	}
	val = gf_color_parse(str);
	if (!val) {
		return gf_bt_report(parser, GF_BAD_PARAM, "%s: Number or name expected", name);
	}
	col->red = INT2FIX((val>>16) & 0xFF) / 255;
	col->green = INT2FIX((val>>8) & 0xFF) / 255;
	col->blue = INT2FIX(val & 0xFF) / 255;
	return parser->last_error;
}

GF_Err gf_bt_parse_colorRGBA(GF_BTParser *parser, const char *name, SFColorRGBA *col)
{
	Float f;
	char *str = gf_bt_get_next(parser, 0);
	if (!str) return parser->last_error = GF_IO_ERR;
	if (gf_bt_check_externproto_field(parser, str)) return GF_OK;

	/*HTML code*/
	if (str[0]=='$') {
		u32 val;
		sscanf(str, "%x", &val);
		col->red = INT2FIX((val>>24) & 0xFF) / 255;
		col->green = INT2FIX((val>>16) & 0xFF) / 255;
		col->blue = INT2FIX((val>>8) & 0xFF) / 255;
		col->alpha = INT2FIX(val & 0xFF) / 255;
		return parser->last_error;
	}
	if (sscanf(str, "%f", &f) != 1) {
		return gf_bt_report(parser, GF_BAD_PARAM, "%s: Number expected", name);
	}
	col->red = FLT2FIX(f);
	gf_bt_check_code(parser, ',');
	gf_bt_parse_float(parser, name, & col->green);
	gf_bt_check_code(parser, ',');
	gf_bt_parse_float(parser, name, & col->blue);
	gf_bt_check_code(parser, ',');
	gf_bt_parse_float(parser, name, & col->alpha);
	return parser->last_error;
}

static void gf_bt_offset_time(GF_BTParser *parser, Double *time)
{
	if (!parser->is_wrl) {
		Double res;
		res = parser->au_time;
		res /= parser->bifs_es->timeScale;
		*time += res;
	}
}

static void gf_bt_check_time_offset(GF_BTParser *parser, GF_Node *n, GF_FieldInfo *info)
{
	if (!n || !(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;
	if (gf_node_get_tag(n) != TAG_ProtoNode) {
		if (!stricmp(info->name, "startTime") || !stricmp(info->name, "stopTime"))
			gf_bt_offset_time(parser, (Double *)info->far_ptr);
	} else if (gf_sg_proto_field_is_sftime_offset(n, info)) {
		gf_bt_offset_time(parser, (Double *)info->far_ptr);
	}
}
static void gf_bt_update_timenode(GF_BTParser *parser, GF_Node *node)
{
	if (!node || !(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_AnimationStream:
		gf_bt_offset_time(parser, & ((M_AnimationStream*)node)->startTime);
		gf_bt_offset_time(parser, & ((M_AnimationStream*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioBuffer:
		gf_bt_offset_time(parser, & ((M_AudioBuffer*)node)->startTime);
		gf_bt_offset_time(parser, & ((M_AudioBuffer*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioClip:
		gf_bt_offset_time(parser, & ((M_AudioClip*)node)->startTime);
		gf_bt_offset_time(parser, & ((M_AudioClip*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioSource:
		gf_bt_offset_time(parser, & ((M_AudioSource*)node)->startTime);
		gf_bt_offset_time(parser, & ((M_AudioSource*)node)->stopTime);
		break;
	case TAG_MPEG4_MovieTexture:
		gf_bt_offset_time(parser, & ((M_MovieTexture*)node)->startTime);
		gf_bt_offset_time(parser, & ((M_MovieTexture*)node)->stopTime);
		break;
	case TAG_MPEG4_TimeSensor:
		gf_bt_offset_time(parser, & ((M_TimeSensor*)node)->startTime);
		gf_bt_offset_time(parser, & ((M_TimeSensor*)node)->stopTime);
		break;
	case TAG_ProtoNode:
	{
		u32 i, nbFields;
		GF_FieldInfo inf;
		nbFields = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
		for (i=0; i<nbFields; i++) {
			gf_node_get_field(node, i, &inf);
			if (inf.fieldType != GF_SG_VRML_SFTIME) continue;
			gf_bt_check_time_offset(parser, node, &inf);
		}
	}
	break;
	}
}


void gf_bt_sffield(GF_BTParser *parser, GF_FieldInfo *info, GF_Node *n)
{
	switch (info->fieldType) {
	case GF_SG_VRML_SFINT32:
		gf_bt_parse_int(parser, info->name, (SFInt32 *)info->far_ptr);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFBOOL:
		gf_bt_parse_bool(parser, info->name, (SFBool *)info->far_ptr);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFFLOAT:
		gf_bt_parse_float(parser, info->name, (SFFloat *)info->far_ptr);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFDOUBLE:
		gf_bt_parse_double(parser, info->name, (SFDouble *)info->far_ptr);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFTIME:
		gf_bt_parse_double(parser, info->name, (SFDouble *)info->far_ptr);
		if (parser->last_error) return;
		gf_bt_check_time_offset(parser, n, info);
		break;
	case GF_SG_VRML_SFCOLOR:
		gf_bt_parse_color(parser, info->name, (SFColor *)info->far_ptr);
		break;
	case GF_SG_VRML_SFCOLORRGBA:
		gf_bt_parse_colorRGBA(parser, info->name, (SFColorRGBA *)info->far_ptr);
		break;
	case GF_SG_VRML_SFVEC2F:
		gf_bt_parse_float(parser, info->name, & ((SFVec2f *)info->far_ptr)->x);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, info->name, & ((SFVec2f *)info->far_ptr)->y);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFVEC2D:
		gf_bt_parse_double(parser, info->name, & ((SFVec2d *)info->far_ptr)->x);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_double(parser, info->name, & ((SFVec2d *)info->far_ptr)->y);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFVEC3F:
		gf_bt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->x);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->y);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, info->name, & ((SFVec3f *)info->far_ptr)->z);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFVEC3D:
		gf_bt_parse_double(parser, info->name, & ((SFVec3d *)info->far_ptr)->x);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_double(parser, info->name, & ((SFVec3d *)info->far_ptr)->y);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_double(parser, info->name, & ((SFVec3d *)info->far_ptr)->z);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFVEC4F:
		gf_bt_parse_float(parser, info->name, & ((SFVec4f *)info->far_ptr)->x);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, info->name, & ((SFVec4f *)info->far_ptr)->y);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, info->name, & ((SFVec4f *)info->far_ptr)->z);
		if (parser->last_error) return;
		/*many VRML files use ',' separator*/
		gf_bt_check_code(parser, ',');
		gf_bt_parse_float(parser, info->name, & ((SFVec4f *)info->far_ptr)->q);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFROTATION:
		gf_bt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->x);
		if (parser->last_error) return;
		gf_bt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->y);
		if (parser->last_error) return;
		gf_bt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->z);
		if (parser->last_error) return;
		gf_bt_parse_float(parser, info->name, & ((SFRotation *)info->far_ptr)->q);
		if (parser->last_error) return;
		break;
	case GF_SG_VRML_SFSTRING:
	{
		u8 delim = 0;
		if (gf_bt_check_code(parser, '\"')) delim = '\"';
		else if (gf_bt_check_code(parser, '\'')) delim = '\'';
		if (delim) {
			char *str = gf_bt_get_string(parser, delim);
			if (!str)
				goto err;
			if (((SFString *)info->far_ptr)->buffer) gf_free(((SFString *)info->far_ptr)->buffer);
			((SFString *)info->far_ptr)->buffer = NULL;
			if (strlen(str))
				((SFString *)info->far_ptr)->buffer = str;
			else
				gf_free(str);

			if (n && (n->sgprivate->tag==TAG_MPEG4_BitWrapper)) {
				gf_sm_update_bitwrapper_buffer(n, parser->load->fileName);
			}
		} else {
			goto err;
		}
	}
	break;
	case GF_SG_VRML_SFURL:
	{
		u8 delim = 0;
		if (gf_bt_check_code(parser, '\"')) delim = '\"';
		else if (gf_bt_check_code(parser, '\'')) delim = '\'';
		if (delim) {
			SFURL *url = (SFURL *)info->far_ptr;
			char *str = gf_bt_get_string(parser, delim);
			if (!str) goto err;
			if (url->url) gf_free(url->url);
			url->url = NULL;
			url->OD_ID = 0;
			if (strchr(str, '#')) {
				url->url = str;
			} else {
				u32 id = 0;
				char *odstr = str;
				if (!strnicmp(str, "od://", 5)) odstr += 5;
				else if (!strnicmp(str, "od:", 3)) odstr += 3;
				/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
				if (sscanf(odstr, "%u", &id) == 1) {
					char szURL[20];
					sprintf(szURL, "%u", id);
					if (strcmp(szURL, odstr)) id=0;
				}
				if (id) {
					url->OD_ID = id;
					gf_free(str);
				} else {
					url->url = str;
				}
			}
		} else {
			s32 val;
			gf_bt_parse_int(parser, info->name, & val );
			if (parser->last_error) return;
			((SFURL *)info->far_ptr)->OD_ID = val;
		}
	}
	break;
	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		SFCommandBuffer *cb = (SFCommandBuffer *)info->far_ptr;
		if (gf_bt_check_code(parser, '{')) {
			GF_Command *prev_com = parser->cur_com;
			while (!parser->last_error) {
				if (gf_bt_check_code(parser, '}')) break;
				parser->last_error = gf_bt_parse_bifs_command(parser, NULL, cb->commandList);
			}
			parser->cur_com = prev_com;
		}
	}
	break;
	case GF_SG_VRML_SFIMAGE:
	{
		u32 i, size, v;
		SFImage *img = (SFImage *)info->far_ptr;
		gf_bt_parse_int(parser, "width", (SFInt32 *)&img->width);
		if (parser->last_error) return;
		gf_bt_parse_int(parser, "height", (SFInt32 *)&img->height);
		if (parser->last_error) return;
		gf_bt_parse_int(parser, "nbComp", (SFInt32 *)&v);
		if (parser->last_error) return;
		img->numComponents = v;
		size = img->width * img->height * img->numComponents;
		if (img->pixels) gf_free(img->pixels);
		img->pixels = (unsigned char*)gf_malloc(sizeof(char) * size);
		for (i=0; i<size; i++) {
			char *str = gf_bt_get_next(parser, 0);
			if (strstr(str, "0x")) sscanf(str, "%x", &v);
			else sscanf(str, "%u", &v);
			switch (img->numComponents) {
			case 1:
				img->pixels[i] = (char) v;
				break;
			case 2:
				img->pixels[i] = (char) (v>>8)&0xFF;
				img->pixels[i+1] = (char) (v)&0xFF;
				i++;
				break;
			case 3:
				img->pixels[i] = (char) (v>>16)&0xFF;
				img->pixels[i+1] = (char) (v>>8)&0xFF;
				img->pixels[i+2] = (char) (v)&0xFF;
				i+=2;
				break;
			case 4:
				img->pixels[i] = (char) (v>>24)&0xFF;
				img->pixels[i+1] = (char) (v>>16)&0xFF;
				img->pixels[i+2] = (char) (v>>8)&0xFF;
				img->pixels[i+3] = (char) (v)&0xFF;
				i+=3;
				break;
			}
		}
	}
	break;
	case GF_SG_VRML_SFSCRIPT:
	{
		SFScript *sc = (SFScript *) info->far_ptr;
		if (!gf_bt_check_code(parser, '\"')) {
			gf_bt_report(parser, GF_BAD_PARAM, "\" expected in Script");
		}
		sc->script_text = (char*)gf_bt_get_string(parser, '\"');
	}
	break;
	case GF_SG_VRML_SFATTRREF:
	{
		SFAttrRef *ar = (SFAttrRef*) info->far_ptr;
		char *str = gf_bt_get_next(parser, 1);
		if (!gf_bt_check_code(parser, '.')) {
			gf_bt_report(parser, GF_BAD_PARAM, "'.' expected in SFAttrRef");
		} else {
			GF_FieldInfo pinfo;
			ar->node = gf_bt_peek_node(parser, str);
			str = gf_bt_get_next(parser, 0);
			if (gf_node_get_field_by_name(ar->node, str, &pinfo) != GF_OK) {
				gf_bt_report(parser, GF_BAD_PARAM, "field %s is not a member of node %s", str, gf_node_get_class_name(ar->node) );
			} else {
				ar->fieldIndex = pinfo.fieldIndex;
			}
		}

	}
	break;
	default:
		parser->last_error = GF_NOT_SUPPORTED;
		break;

	}
	gf_bt_check_code(parser, ',');
	return;
err:
	gf_bt_report(parser, GF_BAD_PARAM, "%s: Invalid field syntax", info->name);
}

void gf_bt_mffield(GF_BTParser *parser, GF_FieldInfo *info, GF_Node *n)
{
	GF_FieldInfo sfInfo;
	Bool force_single = 0;

	if (!gf_bt_check_code(parser, '[')) {
		if (parser->is_extern_proto_field) return;
		force_single = 1;
	}

	sfInfo.fieldType = gf_sg_vrml_get_sf_type(info->fieldType);
	sfInfo.name = info->name;
	gf_sg_vrml_mf_reset(info->far_ptr, info->fieldType);

	while (!gf_bt_check_code(parser, ']')) {
		gf_sg_vrml_mf_append(info->far_ptr, info->fieldType, &sfInfo.far_ptr);
		gf_bt_sffield(parser, &sfInfo, n);
		if (parser->last_error) return;

		gf_bt_check_code(parser, ',');
		if (force_single) break;
	}
}

Bool gf_bt_check_ndt(GF_BTParser *parser, GF_FieldInfo *info, GF_Node *node, GF_Node *parent)
{
	if (!node) return 1;
	if (parent->sgprivate->tag == TAG_MPEG4_Script) return 1;
#ifndef GPAC_DISABLE_X3D
	if (parent->sgprivate->tag == TAG_X3D_Script) return 1;
#endif
	if (node->sgprivate->tag == TAG_UndefinedNode) return 1;

	/*this handles undefined nodes*/
	if (gf_node_in_table(node, info->NDTtype)) return 1;
	/*not found*/
	gf_bt_report(parser, GF_BAD_PARAM, "node %s not valid in field %s\n", gf_node_get_class_name(node), info->name);
	gf_node_unregister(node, parent);
	return 0;
}

u32 gf_bt_get_next_node_id(GF_BTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);
	ID = gf_sg_get_next_available_node_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_node_id))
		parser->load->ctx->max_node_id = ID;
	return ID;
}
u32 gf_bt_get_next_route_id(GF_BTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sg = parser->load->scene_graph;
	if (parser->parsing_proto) sg = gf_sg_proto_get_graph(parser->parsing_proto);

	ID = gf_sg_get_next_available_route_id(sg);
	if (parser->load->ctx && (ID>parser->load->ctx->max_route_id))
		parser->load->ctx->max_route_id = ID;
	return ID;
}
u32 gf_bt_get_next_proto_id(GF_BTParser *parser)
{
	u32 ID;
	GF_SceneGraph *sc = parser->load->scene_graph;
	if (parser->parsing_proto) sc = gf_sg_proto_get_graph(parser->parsing_proto);
	ID = gf_sg_get_next_available_proto_id(sc);
	if (parser->load->ctx && (ID>parser->load->ctx->max_node_id))
		parser->load->ctx->max_proto_id = ID;
	return ID;
}

u32 gf_bt_get_def_id(GF_BTParser *parser, char *defName)
{
	GF_Node *n=NULL;
	u32 ID=0;
	if (sscanf(defName, "N%u", &ID) == 1) {
		u32 k=1;
		while (defName[k]) {
			if (strchr("0123456789", defName[k])==0) {
				ID = 0;
				break;
			}
			k++;
		}
		if (ID) {
			ID ++;
			n = gf_sg_find_node(parser->load->scene_graph, ID);
			if (!n) {
				if (parser->load->ctx && (parser->load->ctx->max_node_id<ID)) parser->load->ctx->max_node_id=ID;
				return ID;
			}
		}
	}

	ID = gf_bt_get_next_node_id(parser);
	if (n) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[BT Parsing] (line %d) Binary ID %d already assigned to %s - keeping internal ID %d", parser->line, gf_node_get_name(n), ID));
	}
	return ID;
}

Bool gf_bt_set_field_is(GF_BTParser *parser, GF_FieldInfo *info, GF_Node *n)
{
	GF_Err e;
	u32 i;
	GF_ProtoFieldInterface *pfield;
	GF_FieldInfo pinfo;
	char *str;
	gf_bt_check_line(parser);
	i=0;
	while ((parser->line_buffer[parser->line_pos + i] == ' ') || (parser->line_buffer[parser->line_pos + i] == '\t')) i++;
	if (strnicmp(&parser->line_buffer[parser->line_pos + i] , "IS", 2)) return 0;

	gf_bt_get_next(parser, 0);
	str = gf_bt_get_next(parser, 0);

	/*that's an ISed field*/
	pfield = gf_sg_proto_field_find_by_name(parser->parsing_proto, str);
	if (!pfield) {
		gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown proto field", str);
		return 1;
	}
	gf_sg_proto_field_get_field(pfield, &pinfo);
	e = gf_sg_proto_field_set_ised(parser->parsing_proto, pinfo.fieldIndex, n, info->fieldIndex);
	if (e) gf_bt_report(parser, GF_BAD_PARAM, "IS: Invalid field type for field %s", info->name);
	return 1;
}

void gf_bt_check_unresolved_nodes(GF_BTParser *parser)
{
	u32 i, count;
	count = gf_list_count(parser->undef_nodes);
	if (!count) return;
	for (i=0; i<count; i++) {
		GF_Node *n = (GF_Node *)gf_list_get(parser->undef_nodes, i);
		gf_bt_report(parser, GF_BAD_PARAM, "Cannot find node %s\n", gf_node_get_name(n) );
		gf_node_unregister(n, NULL);
	}
	parser->last_error = GF_BAD_PARAM;
}

Bool gf_bt_has_been_def(GF_BTParser *parser, char *node_name)
{
	u32 i, count;
	count = gf_list_count(parser->def_nodes);
	for (i=0; i<count; i++) {
		GF_Node *n = (GF_Node *) gf_list_get(parser->def_nodes, i);
		if (!strcmp(gf_node_get_name(n), node_name)) return 1;
	}
	return 0;
}

u32 gf_bt_get_node_tag(GF_BTParser *parser, char *node_name)
{
	u32 tag;
	/*if VRML and allowing non MPEG4 nodes, use X3D*/
	if (parser->is_wrl && !(parser->load->flags & GF_SM_LOAD_MPEG4_STRICT)) {
#ifndef GPAC_DISABLE_X3D
		tag = gf_node_x3d_type_by_class_name(node_name);
		if (!tag)
#endif
			tag = gf_node_mpeg4_type_by_class_name(node_name);
		if (tag) return tag;
#ifndef GPAC_DISABLE_X3D
		if (!strcmp(node_name, "Rectangle")) return TAG_X3D_Rectangle2D;
		if (!strcmp(node_name, "Circle")) return TAG_X3D_Circle2D;
#endif
	} else {
		tag = gf_node_mpeg4_type_by_class_name(node_name);
		if (!tag) {
			if (!strcmp(node_name, "Rectangle2D")) return TAG_MPEG4_Rectangle;
			if (!strcmp(node_name, "Circle2D")) return TAG_MPEG4_Circle;
#ifndef GPAC_DISABLE_X3D
			if (!(parser->load->flags & GF_SM_LOAD_MPEG4_STRICT)) return gf_node_x3d_type_by_class_name(node_name);
#endif
		}
	}
	return tag;
}

GF_Node *gf_bt_sf_node(GF_BTParser *parser, char *node_name, GF_Node *parent, char *szDEFName)
{
	u32 tag, ID;
	Bool is_script, replace_prev, register_def;
	GF_Proto *proto;
	GF_Node *node, *newnode, *undef_node;
	GF_FieldInfo info;
	Bool init_node;
	char *name;
	char * str;

	init_node = 0;

	if (node_name) {
		str = node_name;
	} else {
		str = gf_bt_get_next(parser, 0);
	}
	name = NULL;
	if (!strcmp(str, "NULL")) return NULL;

	ID = 0;
	register_def = 0;
	replace_prev = 0;
	undef_node = NULL;
	if (!strcmp(str, "DEF")) {
		register_def = 1;
		str = gf_bt_get_next(parser, 0);
		name = gf_strdup(str);
		str = gf_bt_get_next(parser, 0);
	} else if (szDEFName) {
		name = gf_strdup(szDEFName);
		register_def = 1;
	}
	if (name) {
		undef_node = gf_sg_find_node_by_name(parser->load->scene_graph, name);
		if (undef_node) {
			gf_list_del_item(parser->peeked_nodes, undef_node);
			ID = gf_node_get_id(undef_node);
			/*if we see twice a DEF N1 then force creation of a new node*/
			if (gf_bt_has_been_def(parser, name)) {
				undef_node = NULL;
				ID = gf_bt_get_def_id(parser, name);
				gf_bt_report(parser, GF_OK, "Node %s has been DEFed several times, IDs may get corrupted", name);
			}
		} else {
			ID = gf_bt_get_def_id(parser, name);
		}
	}
	else if (!strcmp(str, "USE")) {
		str = gf_bt_get_next(parser, 0);
		node = gf_sg_find_node_by_name(parser->load->scene_graph, str);
		if (!node) {
			/*create a temp node (undefined)*/
			node = gf_node_new(parser->load->scene_graph, TAG_UndefinedNode);
			ID = gf_bt_get_def_id(parser, str);
			gf_node_set_id(node, ID, str);
			gf_node_register(node, NULL);
			gf_list_add(parser->undef_nodes, node);
		}
		gf_node_register(node, parent);
		return node;
	}
	proto = NULL;
	tag = gf_bt_get_node_tag(parser, str);
	if (!tag) {
		GF_SceneGraph *sg = parser->load->scene_graph;
		while (1) {
			proto = gf_sg_find_proto(sg, 0, str);
			if (proto) break;
			sg = sg->parent_scene;
			if (!sg) break;
		}
		if (!proto) {
			/*locate proto*/
			gf_bt_report(parser, GF_BAD_PARAM, "%s: not a valid/supported node", str);
			return NULL;
		}
		tag = TAG_ProtoNode;
	}
	if (undef_node && (undef_node->sgprivate->tag == tag)) {
		node = undef_node;
	} else {
		if (undef_node) replace_prev = 1;
		if (proto) {
			node = gf_sg_proto_create_instance(parser->load->scene_graph, proto);
		} else {
			node = gf_node_new(parser->load->scene_graph, tag);
		}
		if (!parser->parsing_proto) init_node = 1;
	}
	is_script = 0;
	if ((tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
	        || (tag==TAG_X3D_Script)
#endif
	   )
		is_script = 1;

	if (!node) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	if (register_def) gf_list_add(parser->def_nodes, node);

	gf_node_register(node, parent);

	/*VRML: "The transformation hierarchy shall be a directed acyclic graph; results are undefined if a node
	in the transformation hierarchy is its own ancestor"
	that's good, because the scene graph can't handle cyclic graphs (destroy will never be called).
	However we still have to register the node before parsing it, to update node registry and get correct IDs*/
	if (name) {
		if (!undef_node || replace_prev) {
			gf_node_set_id(node, ID, name);
		}
		gf_free(name);
		name = NULL;
	}
	if (!parser->parsing_proto) gf_bt_update_timenode(parser, node);

	if (gf_bt_check_code(parser, '{')) {

		while (1) {
			if (gf_bt_check_code(parser, '}'))
				break;

			str = gf_bt_get_next(parser, 0);
			if (!str) {
				gf_bt_report(parser, GF_BAD_PARAM, "Invalid node syntax");
				goto err;
			}
			/*VRML/X3D specific */
			if (parser->is_wrl) {
				/*we ignore bboxCenter and bboxSize*/
				if (!strcmp(str, "bboxCenter") || !strcmp(str, "bboxSize")) {
					Fixed f;
					gf_bt_parse_float(parser, "x", &f);
					gf_bt_parse_float(parser, "y", &f);
					gf_bt_parse_float(parser, "z", &f);
					continue;
				}
				/*some VRML files declare routes almost anywhere*/
				if (!strcmp(str, "ROUTE")) {
					gf_bt_parse_route(parser, 1, 0, NULL);
					continue;
				}
			}

			parser->last_error = gf_node_get_field_by_name(node, str, &info);

			/*check common VRML fields removed in MPEG4*/
			if (parser->last_error) {
				if (!parser->is_wrl) {
					/*we ignore 'solid' for MPEG4 box/cone/etc*/
					if (!strcmp(str, "solid")) {
						SFBool b;
						gf_bt_parse_bool(parser, "solid", &b);
						parser->last_error = GF_OK;
						continue;
					}
					/*we ignore 'description' for MPEG4 sensors*/
					else if (!strcmp(str, "description")) {
						char *tmpstr = gf_bt_get_string(parser, 0);
						gf_free(tmpstr);
						parser->last_error = GF_OK;
						continue;
					}
					/*remaps X3D to old VRML/MPEG4*/
					else if ((tag==TAG_MPEG4_LOD) && !strcmp(str, "children")) {
						str = "level";
						parser->last_error = gf_node_get_field_by_name(node, str, &info);
					}
					else if ((tag==TAG_MPEG4_Switch) && !strcmp(str, "children")) {
						str = "choice";
						parser->last_error = gf_node_get_field_by_name(node, str, &info);
					}
					else if (!strcmp(str, "enabled")) {
						SFBool b;
						gf_bt_parse_bool(parser, "collide", &b);
						parser->last_error = GF_OK;
						continue;
					}
				} else {
					/*remaps old VRML/MPEG4 to X3D if possible*/
#ifndef GPAC_DISABLE_X3D
					if ((tag==TAG_X3D_LOD) && !strcmp(str, "level")) {
						str = "children";
						parser->last_error = gf_node_get_field_by_name(node, str, &info);
					}
					else if ((tag==TAG_X3D_Switch) && !strcmp(str, "choice")) {
						str = "children";
						parser->last_error = gf_node_get_field_by_name(node, str, &info);
					}
					else
#endif
						if (!strcmp(str, "collide")) {
							SFBool b;
							gf_bt_parse_bool(parser, "enabled", &b);
							parser->last_error = GF_OK;
							continue;
						}
				}
			}

			if (is_script && parser->last_error) {
				u32 eType, fType;

				if (!strcmp(str, "eventIn") || !strcmp(str, "inputOnly")) eType = GF_SG_SCRIPT_TYPE_EVENT_IN;
				else if (!strcmp(str, "eventOut") || !strcmp(str, "outputOnly")) eType = GF_SG_SCRIPT_TYPE_EVENT_OUT;
				else if (!strcmp(str, "field") || !strcmp(str, "initializeOnly")) eType = GF_SG_SCRIPT_TYPE_FIELD;
				else {
					gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown script event type", str);
					goto err;
				}
				str = gf_bt_get_next(parser, 0);
				fType = gf_sg_field_type_by_name(str);
				if (fType==GF_SG_VRML_UNKNOWN) {
					gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown script field type", str);
					goto err;
				}
				parser->last_error = GF_OK;
				str = gf_bt_get_next(parser, 0);
				gf_sg_script_field_new(node, eType, fType, str);
				parser->last_error = gf_node_get_field_by_name(node, str, &info);

				if (parser->parsing_proto && gf_bt_set_field_is(parser, &info, node)) continue;
				if ((eType == GF_SG_SCRIPT_TYPE_EVENT_IN) || (eType == GF_SG_SCRIPT_TYPE_EVENT_OUT)) continue;
			}

			if (parser->last_error) {
				gf_bt_report(parser, parser->last_error, "%s: Unknown field", str);
				goto err;
			}

			if (proto) gf_sg_proto_mark_field_loaded(node, &info);
			if (parser->parsing_proto && gf_bt_set_field_is(parser, &info, node)) continue;

			switch (info.fieldType) {
			case GF_SG_VRML_SFNODE:
				/*if redefining node reset it - this happens with CreateVrmlFromString*/
				if (* ((GF_Node **)info.far_ptr) ) {
					gf_node_unregister(* ((GF_Node **)info.far_ptr), node);
					* ((GF_Node **)info.far_ptr) = NULL;
				}

				newnode = gf_bt_sf_node(parser, NULL, node, NULL);
				if (!newnode && parser->last_error) goto err;
				if (newnode) {
					if (!gf_bt_check_ndt(parser, &info, newnode, node)) goto err;

					* ((GF_Node **)info.far_ptr) = newnode;
				}
				break;
			case GF_SG_VRML_MFNODE:
			{
				GF_ChildNodeItem *last = NULL;
				Bool single_child = 0;
				if (!gf_bt_check_code(parser, '[')) {
					if (parser->is_wrl) single_child = 1;
					else break;
				}

				/*if redefining node reset it - this happens with CreateVrmlFromString*/
				if (undef_node==node) {
					gf_node_unregister_children(node, *(GF_ChildNodeItem **)info.far_ptr);
					*(GF_ChildNodeItem **)info.far_ptr = NULL;
				}

				while (single_child || !gf_bt_check_code(parser, ']')) {
					/*VRML seems to allow that*/
					gf_bt_check_code(parser, ',');
					newnode = gf_bt_sf_node(parser, NULL, node, NULL);
					if (!newnode && parser->last_error) goto err;
					if (newnode) {
						if (!gf_bt_check_ndt(parser, &info, newnode, node)) goto err;
						gf_node_list_add_child_last( (GF_ChildNodeItem **)info.far_ptr, newnode, &last);
					}
					if (single_child) break;
				}
			}
			break;
			default:
				if (gf_sg_vrml_is_sf_field(info.fieldType)) {
					gf_bt_sffield(parser, &info, node);
				} else {
					gf_bt_mffield(parser, &info, node);
				}
				if (parser->last_error) goto err;
				break;
			}
			/*VRML seems to allow that*/
			gf_bt_check_code(parser, ',');
		}
	}
	/*VRML seems to allow that*/
	gf_bt_check_code(parser, ',');

	/*we must init the node once ID is set in case we're creating rendering stacks*/
	if (init_node && (gf_node_get_tag(node)!=TAG_ProtoNode) ) gf_node_init(node);

	/*remove temp node*/
	if (replace_prev) {
		gf_node_replace(undef_node, node, 0);
		gf_node_unregister(undef_node, NULL);
		gf_list_del_item(parser->undef_nodes, undef_node);
	}

	if (!parser->parsing_proto && is_script && (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) ) {
		if (parser->cur_com) {
			if (!parser->cur_com->scripts_to_load) parser->cur_com->scripts_to_load = gf_list_new();
			gf_list_add(parser->cur_com->scripts_to_load, node);
		} else {
			/*postpone script init since it may use routes/nodes not yet defined ...*/
			gf_list_add(parser->scripts, node);
		}
	}
	/*For Ivica: load proto as soon as found when in playback mode*/
	if ( (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) && proto && !parser->parsing_proto) {
		parser->last_error = gf_sg_proto_load_code(node);
	}
	return node;

err:
	gf_node_unregister(node, parent);
	if (name) gf_free(name);
	return NULL;
}
/*
	locate node, if not defined yet parse ahead in current AU
	optimization: we actually peek ALL DEF NODES till end of AU
*/
GF_Node *gf_bt_peek_node(GF_BTParser *parser, char *defID)
{
	GF_Node *n, *the_node;
	u32 tag, ID;
	Bool prev_is_insert = 0;
	char *ret;
	char nName[1000];
	u32 pos, line, line_pos, i, count;

	n = gf_sg_find_node_by_name(parser->load->scene_graph, defID);
	if (n) return n;

	count = gf_list_count(parser->peeked_nodes);
	for (i=0; i<count; i++) {
		n = (GF_Node *)gf_list_get(parser->peeked_nodes, i);
		if (!strcmp(gf_node_get_name(n), defID)) return n;
	}

	the_node = NULL;
	pos = parser->line_start_pos;
	line_pos = parser->line_pos;
	line = parser->line;
	strcpy(nName, defID);

	n = NULL;
	while (!parser->done && !the_node) {
		char *str = gf_bt_get_next(parser, 0);
		gf_bt_check_code(parser, '[');
		gf_bt_check_code(parser, ']');
		gf_bt_check_code(parser, '{');
		gf_bt_check_code(parser, '}');
		gf_bt_check_code(parser, ',');
		gf_bt_check_code(parser, '.');

		if ( (!prev_is_insert && !strcmp(str, "AT")) || !strcmp(str, "PROTO") ) {
			/*only check in current command (but be aware of conditionals..)*/
			if (!the_node && gf_list_find(parser->bifs_au->commands, parser->cur_com)) {
				break;
			}
			continue;
		}
		if (!strcmp(str, "INSERT")) prev_is_insert = 1;
		else prev_is_insert = 0;

		if (strcmp(str, "DEF")) continue;
		str = gf_bt_get_next(parser, 0);
		ret = gf_strdup(str);
		str = gf_bt_get_next(parser, 0);
		if (!strcmp(str, "ROUTE")) {
			gf_free(ret);
			continue;
		}

		tag = gf_bt_get_node_tag(parser, str);
		if (!tag) {
			GF_Proto *p;
			GF_SceneGraph *sg = parser->load->scene_graph;
			while (1) {
				p = gf_sg_find_proto(sg, 0, str);
				if (p) break;
				sg = sg->parent_scene;
				if (!sg) break;
			}
			if (!p) {
				/*locate proto*/
				gf_bt_report(parser, GF_BAD_PARAM, "%s: not a valid/supported node", str);
				gf_free(ret);
				return NULL;
			}
			n = gf_sg_proto_create_instance(parser->load->scene_graph, p);
		} else {
			n = gf_node_new(parser->load->scene_graph, tag);
		}
		ID = gf_bt_get_def_id(parser, ret);
		if (n) {
			gf_node_set_id(n, ID, ret);
			gf_list_add(parser->peeked_nodes, n);
			if (!parser->parsing_proto) gf_node_init(n);
			if (!strcmp(ret, nName)) the_node = n;
		}
		gf_free(ret);

		/*NO REGISTER on peek (both scene graph or DEF list) because peek is only used to get node type
		and fields, never to insert in the graph*/

		/*go on till end of AU*/
	}
	/*restore context*/
	parser->done = 0;
	gf_gzrewind(parser->gz_in);
	gf_gzseek(parser->gz_in, pos, SEEK_SET);
	parser->line_pos = parser->line_size;
	gf_bt_check_line(parser);
	parser->line = line;
	parser->line_pos = line_pos;

	return the_node;
}

u32 gf_bt_get_route(GF_BTParser *parser, char *name)
{
	u32 i;
	GF_Command *com;
	GF_Route *r = gf_sg_route_find_by_name(parser->load->scene_graph, name);
	if (r) return r->ID;
	i=0;
	while ((com = (GF_Command *)gf_list_enum(parser->inserted_routes, &i))) {
		if (com->def_name && !strcmp(com->def_name, name)) return com->RouteID;
	}
	return 0;
}

Bool gf_bt_route_id_used(GF_BTParser *parser, u32 ID)
{
	u32 i;
	GF_Command *com;
	GF_Route *r = gf_sg_route_find(parser->load->scene_graph, ID);
	if (r) return 1;
	i=0;
	while ((com = (GF_Command *)gf_list_enum(parser->inserted_routes, &i))) {
		if (com->RouteID == ID) return 1;
	}
	return 0;
}

static u32 get_evt_type(char *eventName)
{
	if (!strcmp(eventName, "eventIn") || !strcmp(eventName, "inputOnly")) return GF_SG_EVENT_IN;
	else if (!strcmp(eventName, "eventOut") || !strcmp(eventName, "outputOnly")) return GF_SG_EVENT_OUT;
	else if (!strcmp(eventName, "field") || !strcmp(eventName, "initializeOnly")) return GF_SG_EVENT_FIELD;
	else if (!strcmp(eventName, "exposedField") || !strcmp(eventName, "inputOutput")) return GF_SG_EVENT_EXPOSED_FIELD;
	else return GF_SG_EVENT_UNKNOWN;
}

GF_Err gf_bt_parse_proto(GF_BTParser *parser, char *proto_code, GF_List *proto_list)
{
	GF_FieldInfo info;
	u32 fType, eType, QPType=0, pID;
	Bool externProto;
	GF_Proto *proto, *prevproto;
	GF_ProtoFieldInterface *pfield;
	GF_SceneGraph *sg;
	char *str, *name;
	char szDefName[1024];
	Bool isDEF;

	if (proto_code)
		str = proto_code;
	else
		str = gf_bt_get_next(parser, 0);

	externProto = !strcmp(str, "EXTERNPROTO") ? 1 : 0;
	str = gf_bt_get_next(parser, 0);
	name = gf_strdup(str);
	if (!gf_bt_check_code(parser, '[')) {
		return gf_bt_report(parser, GF_BAD_PARAM, "[ expected in proto declare");
	}
	pID = gf_bt_get_next_proto_id(parser);
	/*if redefinition remove it - WRL only, may be used by loadVRMLFormString*/
	if (!proto_list && parser->is_wrl) {
		proto = gf_sg_find_proto(parser->load->scene_graph, pID, name);
		if (proto) gf_sg_proto_del(proto);
	}
	proto = gf_sg_proto_new(parser->load->scene_graph, pID, name, proto_list ? 1 : 0);
	if (proto_list) gf_list_add(proto_list, proto);
	if (parser->load->ctx && (parser->load->ctx->max_proto_id<pID)) parser->load->ctx->max_proto_id = pID;

	/*hack for VRML, where externProto default field values are not mandatory*/
	parser->is_extern_proto_field = externProto;

	gf_free(name);
	/*get all fields*/
	while (!parser->last_error && !gf_bt_check_code(parser, ']')) {
		str = gf_bt_get_next(parser, 0);

next_field:
		if (gf_bt_check_code(parser, ']')) break;

		eType = get_evt_type(str);
		if (eType==GF_SG_EVENT_UNKNOWN) {
			gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown event type", str);
			goto err;
		}
		str = gf_bt_get_next(parser, 0);
		fType = gf_sg_field_type_by_name(str);
		if (fType==GF_SG_VRML_UNKNOWN) {
			gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown field type", str);
			goto err;
		}
		str = gf_bt_get_next(parser, 0);
		pfield = gf_sg_proto_field_new(proto, fType, eType, str);
		if ((eType==GF_SG_EVENT_IN) || (eType==GF_SG_EVENT_OUT)) continue;

		gf_sg_proto_field_get_field(pfield, &info);
		if (fType==GF_SG_VRML_SFNODE) {
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "NULL")) {
				if ( (!strlen(str) || (get_evt_type(str)!=GF_SG_EVENT_UNKNOWN)) && parser->is_extern_proto_field) goto next_field;
				pfield->def_sfnode_value = gf_bt_sf_node(parser, str, NULL, NULL);
			}
		} else if (fType==GF_SG_VRML_MFNODE) {
			GF_ChildNodeItem *last = NULL;
			if (gf_bt_check_code(parser, '[')) {
				while (1) {
					GF_Node *pf_node;
					if (gf_bt_check_code(parser, ']')) break;
					pf_node = gf_bt_sf_node(parser, NULL, NULL, NULL);
					if (pf_node) gf_node_list_add_child_last( &pfield->def_mfnode_value, pf_node, &last);
				}
			}
		} else if (gf_sg_vrml_is_sf_field(fType)) {
			gf_bt_sffield(parser, &info, NULL);
			/*value not specified for externproto*/
			if (parser->last_error==GF_EOS) {
				parser->last_error=GF_OK;
				goto next_field;
			}
		} else {
			gf_bt_mffield(parser, &info, NULL);
		}
		/*check QP info*/
		if (!gf_bt_check_code(parser, '{')) continue;
		if (gf_bt_check_code(parser, '}')) continue;
		str = gf_bt_get_next(parser, 0);
		if (!strcmp(str, "QP")) {
			u32 nbBits, hasMin;
			Fixed ftMin, ftMax;
			gf_bt_parse_int(parser, "QPType", (SFInt32*)&QPType);

			nbBits = 0;
			str = gf_bt_get_next(parser, 0);
			if (!strcmp(str, "nbBits")) {
				gf_bt_parse_int(parser, "nbBits", (SFInt32*)&nbBits);
				str = gf_bt_get_next(parser, 0);
			}
			hasMin = 0;
			eType = 0;
			if (!strcmp(str, "b")) {
				hasMin = 1;
				if (!gf_bt_check_code(parser, '{')) {
					gf_bt_report(parser, GF_BAD_PARAM, "%s: Invalid proto coding parameter declare", str);
					goto err;
				}
				gf_bt_parse_float(parser, "min", &ftMin);
				gf_bt_parse_float(parser, "max", &ftMax);
				if (!gf_bt_check_code(parser, '}')) {
					gf_bt_report(parser, GF_BAD_PARAM, "Invalid proto coding parameter declare");
					goto err;
				}
				if (gf_sg_vrml_get_sf_type(fType) == GF_SG_VRML_SFINT32) {
					eType = GF_SG_VRML_SFINT32;
				} else {
					eType = GF_SG_VRML_SFFLOAT;
				}
			}
			gf_bifs_proto_field_set_aq_info(pfield, QPType, hasMin, eType, &ftMin, &ftMax, nbBits);
			if (!gf_bt_check_code(parser, '}')) {
				gf_bt_report(parser, GF_BAD_PARAM, "Invalid proto coding parameter declare");
				goto err;
			}
		}
	}
	parser->is_extern_proto_field = 0;

	if (externProto) {
		SFURL *url;
		Bool has_urls = 0;
		if (gf_bt_check_code(parser, '[')) has_urls = 1;

		gf_sg_vrml_mf_reset(&proto->ExternProto, GF_SG_VRML_MFURL);
		do {
			str = gf_bt_get_next(parser, 0);
			gf_sg_vrml_mf_append(&proto->ExternProto, GF_SG_VRML_MFURL, (void **) &url);
			if (!strnicmp(str, "od:", 3)) {
				sscanf(str, "od:%u", &url->OD_ID);
			} else {
				if (!sscanf(str, "%u", &url->OD_ID)) {
					url->url = gf_strdup(str);
				} else {
					char szURL[20];
					sprintf(szURL, "%d", url->OD_ID);
					if (strcmp(szURL, str)) {
						url->OD_ID = 0;
						url->url = gf_strdup(str);
					}
				}
			}
			if (has_urls) {
				gf_bt_check_code(parser, ',');
				if (gf_bt_check_code(parser, ']')) has_urls = 0;
			}
		} while (has_urls);
		return GF_OK;
	}

	/*parse proto code */
	if (!gf_bt_check_code(parser, '{')) {
		gf_bt_report(parser, GF_OK, "empty proto body");
		return GF_OK;
	}

	prevproto = parser->parsing_proto;
	sg = parser->load->scene_graph;
	parser->parsing_proto = proto;
	parser->load->scene_graph = gf_sg_proto_get_graph(proto);

	isDEF = 0;
	while (!gf_bt_check_code(parser, '}')) {
		str = gf_bt_get_next(parser, 0);
		if (!strcmp(str, "PROTO") || !strcmp(str, "EXTERNPROTO")) {
			gf_bt_parse_proto(parser, str, NULL);
		} else if (!strcmp(str, "DEF")) {
			isDEF = 1;
			str = gf_bt_get_next(parser, 0);
			strcpy(szDefName, str);
		} else if (!strcmp(str, "ROUTE")) {
			GF_Route *r = gf_bt_parse_route(parser, 1, 0, NULL);
			if (isDEF) {
				u32 rID = gf_bt_get_route(parser, szDefName);
				if (!rID) rID = gf_bt_get_next_route_id(parser);
				parser->last_error = gf_sg_route_set_id(r, rID);
				gf_sg_route_set_name(r, szDefName);
				isDEF = 0;
			}
		} else {
			GF_Node *n = gf_bt_sf_node(parser, str, NULL, isDEF ? szDefName : NULL);
			isDEF = 0;
			if (!n) goto err;
			if ((0) && isDEF) {
				u32 ID = gf_bt_get_def_id(parser, szDefName);
				isDEF = 0;
				gf_node_set_id(n, ID, szDefName);
			}
			gf_sg_proto_add_node_code(proto, n);
		}
	}
	gf_bt_resolve_routes(parser, 1);
	gf_bt_check_unresolved_nodes(parser);
	parser->load->scene_graph = sg;
	parser->parsing_proto = prevproto;
	return parser->last_error;

err:
	if (proto_list) gf_list_del_item(proto_list, proto);
	gf_sg_proto_del(proto);
	return parser->last_error;
}


GF_Route *gf_bt_parse_route(GF_BTParser *parser, Bool skip_def, Bool is_insert, GF_Command *com)
{
	GF_Route *r;
	char *str, nstr[1000], rName[1000];
	u32 rID;
	GF_Node *orig, *dest;
	GF_FieldInfo orig_field, dest_field;
	GF_Err e;

	rID = 0;
	strcpy(nstr, gf_bt_get_next(parser, 1));
	if (!skip_def && !strcmp(nstr, "DEF")) {
		str = gf_bt_get_next(parser, 0);
		strcpy(rName, str);
		rID = gf_bt_get_route(parser, rName);
		if (!rID && (str[0]=='R') ) {
			rID = atoi(&str[1]);
			if (rID) {
				rID++;
				if (gf_bt_route_id_used(parser, rID)) rID = 0;
			}
		}
		if (!rID) rID = gf_bt_get_next_route_id(parser);
		strcpy(nstr, gf_bt_get_next(parser, 1));
	}
	orig = gf_bt_peek_node(parser, nstr);
	if (!orig) {
		gf_bt_report(parser, GF_BAD_PARAM, "cannot find node %s", nstr);
		return NULL;
	}
	if (!gf_bt_check_code(parser, '.')) {
		gf_bt_report(parser, GF_BAD_PARAM, ". expected in route decl");
		return NULL;
	}
	str = gf_bt_get_next(parser, 0);
	e = gf_node_get_field_by_name(orig, str, &orig_field);
	/*VRML loosy syntax*/
	if ((e != GF_OK) && parser->is_wrl && !strnicmp(str, "set_", 4))
		e = gf_node_get_field_by_name(orig, &str[4], &orig_field);

	if ((e != GF_OK) && parser->is_wrl && strstr(str, "_changed")) {
		char *s = strstr(str, "_changed");
		s[0] = 0;
		e = gf_node_get_field_by_name(orig, str, &orig_field);
	}

	if (e != GF_OK) {
		gf_bt_report(parser, GF_BAD_PARAM, "%s not a field of node %s (%s)", str, gf_node_get_name(orig), gf_node_get_class_name(orig));
		return NULL;
	}
	str = gf_bt_get_next(parser, 0);
	if (strcmp(str, "TO")) {
		gf_bt_report(parser, GF_BAD_PARAM, "TO expected in route declaration - got \"%s\"", str);
		return NULL;
	}

	strcpy(nstr, gf_bt_get_next(parser, 1));
	dest = gf_bt_peek_node(parser, nstr);
	if (!dest) {
		gf_bt_report(parser, GF_BAD_PARAM, "cannot find node %s", nstr);
		return NULL;
	}
	if (!gf_bt_check_code(parser, '.')) {
		gf_bt_report(parser, GF_BAD_PARAM, ". expected in route decl");
		return NULL;
	}
	str = gf_bt_get_next(parser, 0);
	e = gf_node_get_field_by_name(dest, str, &dest_field);
	/*VRML loosy syntax*/
	if ((e != GF_OK) && parser->is_wrl && !strnicmp(str, "set_", 4))
		e = gf_node_get_field_by_name(dest, &str[4], &dest_field);

	if ((e != GF_OK) && parser->is_wrl && strstr(str, "_changed")) {
		char *s = strstr(str, "_changed");
		s[0] = 0;
		e = gf_node_get_field_by_name(dest, str, &dest_field);
	}

	if (e != GF_OK) {
		gf_bt_report(parser, GF_BAD_PARAM, "%s not a field of node %s (%s)", str, gf_node_get_name(dest), gf_node_get_class_name(dest));
		return NULL;
	}
	if (com) {
		com->fromNodeID = gf_node_get_id(orig);
		com->fromFieldIndex = orig_field.fieldIndex;
		com->toNodeID = gf_node_get_id(dest);
		com->toFieldIndex = dest_field.fieldIndex;
		if (rID) {
			com->RouteID = rID;
			com->def_name = gf_strdup(rName);
			/*whenever inserting routes, keep track of max defined ID*/
			if (is_insert) {
				gf_sg_set_max_defined_route_id(parser->load->scene_graph, rID);
				if (parser->load->ctx && (rID>parser->load->ctx->max_route_id))
					parser->load->ctx->max_route_id = rID;
			}
		}
		return NULL;
	}
	r = gf_sg_route_new(parser->load->scene_graph, orig, orig_field.fieldIndex, dest, dest_field.fieldIndex);
	if (r && rID) {
		gf_sg_route_set_id(r, rID);
		gf_sg_route_set_name(r, rName);
	}
	return r;
}

void gf_bt_resolve_routes(GF_BTParser *parser, Bool clean)
{
	/*resolve all commands*/
	while(gf_list_count(parser->unresolved_routes) ) {
		GF_Command *com = (GF_Command *)gf_list_get(parser->unresolved_routes, 0);
		gf_list_rem(parser->unresolved_routes, 0);
		switch (com->tag) {
		case GF_SG_ROUTE_DELETE:
		case GF_SG_ROUTE_REPLACE:
			com->RouteID = gf_bt_get_route(parser, com->unres_name);
			if (!com->RouteID) gf_bt_report(parser, GF_BAD_PARAM, "Cannot resolve Route %s", com->unres_name);
			gf_free(com->unres_name);
			com->unres_name = NULL;
			com->unresolved = 0;
			break;
		}
	}

	if (!clean) return;
	while (gf_list_count(parser->inserted_routes)) gf_list_rem(parser->inserted_routes, 0);
}


static void bd_set_com_node(GF_Command *com, GF_Node *node)
{
	com->node = node;
	gf_node_register(com->node, NULL);
}

GF_Err gf_bt_parse_bifs_command(GF_BTParser *parser, char *name, GF_List *cmdList)
{
	s32 pos;
	GF_Route *r;
	GF_Node *n, *newnode;
	GF_Command *com;
	GF_CommandField *inf;
	GF_FieldInfo info;
	char *str, field[1000];
	if (!name) {
		str = gf_bt_get_next(parser, 0);
	} else {
		str = name;
	}
	com = NULL;
	pos = -2;
	/*REPLACE commands*/
	if (!strcmp(str, "REPLACE")) {
		str = gf_bt_get_next(parser, 1);
		if (!strcmp(str, "ROUTE")) {
			str = gf_bt_get_next(parser, 0);
			r = gf_sg_route_find_by_name(parser->load->scene_graph, str);
			if (!r) strcpy(field, str);
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "BY")) {
				return gf_bt_report(parser, GF_BAD_PARAM, "BY expected got %s", str);
			}
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_ROUTE_REPLACE);
			if (r) {
				com->RouteID = r->ID;
			} else {
				com->unres_name = gf_strdup(field);
				com->unresolved = 1;
				gf_list_add(parser->unresolved_routes, com);
			}
			gf_bt_parse_route(parser, 1, 0, com);
			gf_list_add(cmdList, com);
			return parser->last_error;
		}
		/*scene replace*/
		if (!strcmp(str, "SCENE")) {
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "BY")) {
				return gf_bt_report(parser, GF_BAD_PARAM, "BY expected got %s", str);
			}
			gf_bt_resolve_routes(parser, 1);
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_SCENE_REPLACE);
			while (gf_list_count(parser->def_nodes)) gf_list_rem(parser->def_nodes, 0);

			while (1) {
				str = gf_bt_get_next(parser, 0);
				if (!strcmp(str, "PROTO") || !strcmp(str, "EXTERNPROTO")) {
					gf_bt_parse_proto(parser, str, com->new_proto_list);
					if (parser->last_error) goto err;
				} else {
					break;
				}
			}
			n = gf_bt_sf_node(parser, str, NULL, NULL);
			com->node = n;

			if (parser->last_error) goto err;
			gf_list_add(cmdList, com);
			parser->cur_com = com;
			return GF_OK;
		}
		if (!strcmp(str, "LAST")) pos = -1;
		else if (!strcmp(str, "BEGIN")) pos = 0;

		gf_bt_check_code(parser, '.');
		strcpy(field, str);
		n = gf_bt_peek_node(parser, str);
		if (!n) return gf_bt_report(parser, GF_BAD_PARAM, "%s: unknown node", field);

		str = gf_bt_get_next(parser, 0);
		strcpy(field, str);
		if (gf_bt_check_code(parser, '[')) {
			if ( (parser->last_error = gf_bt_parse_int(parser, "index", &pos)) ) return parser->last_error;
			if (!gf_bt_check_code(parser, ']'))
				return gf_bt_report(parser, GF_BAD_PARAM, "] expected");
		}
		/*node replace*/
		if (!strcmp(field, "BY")) {
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_NODE_REPLACE);
			bd_set_com_node(com, n);
			inf = gf_sg_command_field_new(com);
			inf->new_node = gf_bt_sf_node(parser, NULL, NULL, NULL);
			inf->fieldType = GF_SG_VRML_SFNODE;
			inf->field_ptr = &inf->new_node;
			gf_list_add(cmdList, com);
			parser->cur_com = com;
			return parser->last_error;
		}
		str = gf_bt_get_next(parser, 0);
		if (strcmp(str, "BY"))
			return gf_bt_report(parser, GF_BAD_PARAM, "BY expected got %s", str);

		parser->last_error = gf_node_get_field_by_name(n, field, &info);
		if (parser->last_error)
			return gf_bt_report(parser, parser->last_error, "%s: Unknown node field", field);

		/*field replace*/
		if (pos==-2) {
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_FIELD_REPLACE);
			bd_set_com_node(com, n);

			inf = gf_sg_command_field_new(com);
			inf->fieldIndex = info.fieldIndex;
			inf->fieldType = info.fieldType;

			switch (info.fieldType) {
			case GF_SG_VRML_SFNODE:
				newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
				if (!gf_bt_check_ndt(parser, &info, newnode, n)) goto err;
				inf->new_node = newnode;
				inf->field_ptr = &inf->new_node;
				break;
			case GF_SG_VRML_MFNODE:
			{
				GF_ChildNodeItem *last = NULL;
				if (!gf_bt_check_code(parser, '[')) break;
				inf->field_ptr = &inf->node_list;
				while (!gf_bt_check_code(parser, ']')) {
					newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
					if (!newnode) goto err;
					if (parser->last_error!=GF_OK) goto err;
					if (!gf_bt_check_ndt(parser, &info, newnode, n)) goto err;
					gf_node_list_add_child_last(& inf->node_list, newnode, &last);
				}
			}
			break;
			default:
				inf->field_ptr = gf_sg_vrml_field_pointer_new(info.fieldType);
				info.far_ptr = inf->field_ptr;
				if (gf_sg_vrml_is_sf_field(info.fieldType)) {
					gf_bt_sffield(parser, &info, n);
				} else {
					gf_bt_mffield(parser, &info, n);
				}
				if (parser->last_error) goto err;
				break;
			}

			gf_list_add(cmdList, com);
			parser->cur_com = com;
			return parser->last_error;
		}
		/*indexed field replace*/
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_INDEXED_REPLACE);
		bd_set_com_node(com, n);
		inf = gf_sg_command_field_new(com);
		inf->pos = pos;
		inf->fieldIndex = info.fieldIndex;
		if (gf_sg_vrml_is_sf_field(info.fieldType)) {
			gf_bt_report(parser, GF_BAD_PARAM, "%s: MF type field expected", info.name);
			goto err;
		}
		inf->fieldType = gf_sg_vrml_get_sf_type(info.fieldType);
		switch (info.fieldType) {
		case GF_SG_VRML_MFNODE:
			newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
			if (!gf_bt_check_ndt(parser, &info, newnode, n)) goto err;
			inf->new_node = newnode;
			inf->field_ptr = &inf->new_node;
			break;
		default:
			info.fieldType = inf->fieldType;
			info.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			gf_bt_sffield(parser, &info, n);
			break;
		}
		if (parser->last_error) goto err;
		gf_list_add(cmdList, com);
		parser->cur_com = com;
		return parser->last_error;
	}
	/*XREPLACE commands*/
	if (!strcmp(str, "XREPLACE")) {
		u32 j;
		Bool force_sf=0;
		char csep;
		GF_Node *targetNode, *idxNode, *childNode, *fromNode;
		GF_FieldInfo targetField, idxField, childField, fromField;

		targetNode = idxNode = childNode = fromNode = NULL;
		str = gf_bt_get_next(parser, 1);
		/*get source node*/
		strcpy(field, str);
		targetNode = gf_bt_peek_node(parser, str);
		if (!targetNode) return gf_bt_report(parser, GF_BAD_PARAM, "%s: unknown node", field);
		if (!gf_bt_check_code(parser, '.')) {
			return gf_bt_report(parser, GF_BAD_PARAM, "XREPLACE: '.' expected");
		}
		/*get source field*/
		str = gf_bt_get_next(parser, 0);
		strcpy(field, str);
		parser->last_error = gf_node_get_field_by_name(targetNode, field, &targetField);
		if (parser->last_error)
			return gf_bt_report(parser, parser->last_error, "%s: Unknown node field", field);

		if (gf_bt_check_code(parser, '[')) {
			pos = -2;
			str = gf_bt_get_next(parser, 1);
			force_sf = 1;
			if (sscanf(str, "%d", &pos) != 1) {
				pos = -2;
				if (!strcmp(str, "LAST")) pos = -1;
				else if (!strcmp(str, "first")) pos = 0;
				else {
					strcpy(field, str);
					/*get idx node*/
					idxNode = gf_bt_peek_node(parser, str);
					if (!idxNode) return gf_bt_report(parser, GF_BAD_PARAM, "%s: unknown node", field);
					if (!gf_bt_check_code(parser, '.'))
						return gf_bt_report(parser, GF_BAD_PARAM, "XREPLACE: '.' expected");

					/*get idx field*/
					str = gf_bt_get_next(parser, 0);
					strcpy(field, str);
					parser->last_error = gf_node_get_field_by_name(idxNode, field, &idxField);
					if (parser->last_error)
						return gf_bt_report(parser, parser->last_error, "%s: Unknown node field", field);
				}
			}
			gf_bt_check_code(parser, ']');

			/*check if we have a child node*/
			if (gf_bt_check_code(parser, '.')) {
				s32 apos = pos;
				force_sf = 0;
				if (idxNode) {
					apos = 0;
					switch (idxField.fieldType) {
					case GF_SG_VRML_SFBOOL:
						if (*(SFBool*)idxField.far_ptr) apos = 1;
						break;
					case GF_SG_VRML_SFINT32:
						if (*(SFInt32*)idxField.far_ptr >=0) apos = *(SFInt32*)idxField.far_ptr;
						break;
					case GF_SG_VRML_SFFLOAT:
						if ( (*(SFFloat *)idxField.far_ptr) >=0) apos = (s32) floor( FIX2FLT(*(SFFloat*)idxField.far_ptr) );
						break;
					case GF_SG_VRML_SFTIME:
						if ( (*(SFTime *)idxField.far_ptr) >=0) apos = (s32) floor( (*(SFTime *)idxField.far_ptr) );
						break;
					}
				}
				childNode = gf_node_list_get_child(*(GF_ChildNodeItem **)targetField.far_ptr, apos);
				if (!childNode)
					return gf_bt_report(parser, GF_BAD_PARAM, "Cannot find child node at specified index");

				str = gf_bt_get_next(parser, 0);
				strcpy(field, str);
				parser->last_error = gf_node_get_field_by_name(childNode, field, &childField);
				if (parser->last_error)
					return gf_bt_report(parser, parser->last_error, "%s: Unknown node field", field);
			}
		}

		str = gf_bt_get_next(parser, 0);
		if (strcmp(str, "BY"))
			return gf_bt_report(parser, GF_BAD_PARAM, "BY expected got %s", str);

		/*peek the next word*/
		j = 0;
		while (strchr(" \n\t\0", parser->line_buffer[parser->line_pos + j])) j++;
		str = parser->line_buffer + parser->line_pos + j;
		j = 0;
		while (!strchr(" .\0", str[j])) j++;
		csep = str[j];
		str[j]=0;
		strcpy(field, str);
		str[j] = csep;
		fromNode = gf_bt_peek_node(parser, field);
		if (fromNode) {
			gf_bt_get_next(parser, 1);

			if (!gf_bt_check_code(parser, '.')) {
				return gf_bt_report(parser, GF_BAD_PARAM, "XREPLACE: '.' expected");
			}
			/*get source field*/
			str = gf_bt_get_next(parser, 0);
			strcpy(field, str);
			parser->last_error = gf_node_get_field_by_name(fromNode, field, &fromField);
			if (parser->last_error)
				return gf_bt_report(parser, parser->last_error, "%s: Unknown node field", field);

		} else {
			/*regular parsing*/
		}

		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_XREPLACE);
		bd_set_com_node(com, targetNode);
		if (fromNode) {
			com->fromNodeID = gf_node_get_id(fromNode);
			com->fromFieldIndex = fromField.fieldIndex;
		}
		if (idxNode) {
			com->toNodeID = gf_node_get_id(idxNode);
			com->toFieldIndex = idxField.fieldIndex;
		}
		if (childNode) {
			com->ChildNodeTag = gf_node_get_tag(childNode);
			if (com->ChildNodeTag==1) {
				com->ChildNodeTag = ((GF_ProtoInstance*)childNode)->proto_interface->ID;
				com->ChildNodeTag = -com->ChildNodeTag ;
			}
			com->child_field = childField.fieldIndex;
		}
		inf = gf_sg_command_field_new(com);
		inf->fieldIndex = targetField.fieldIndex;
		inf->pos = pos;
		if (force_sf) {
			inf->fieldType = gf_sg_vrml_get_sf_type(targetField.fieldType);
		} else if (childNode) {
			inf->fieldType = childField.fieldType;
		} else {
			inf->fieldType = targetField.fieldType;
		}
		if (!fromNode) {
			switch (inf->fieldType) {
			case GF_SG_VRML_SFNODE:
				inf->new_node = gf_bt_sf_node(parser, NULL, NULL, NULL);
				inf->field_ptr = &inf->new_node;
				if (childNode) {
					if (!gf_bt_check_ndt(parser, &childField, inf->new_node, childNode)) goto err;
				} else {
					if (!gf_bt_check_ndt(parser, &targetField, inf->new_node, targetNode)) goto err;
				}
				break;
			case GF_SG_VRML_MFNODE:
			{
				GF_ChildNodeItem *last = NULL;
				if (!gf_bt_check_code(parser, '[')) break;
				inf->field_ptr = &inf->node_list;
				while (!gf_bt_check_code(parser, ']')) {
					newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
					if (!newnode) goto err;
					if (parser->last_error!=GF_OK) goto err;

					if (childNode) {
						if (!gf_bt_check_ndt(parser, &childField, inf->new_node, childNode)) goto err;
					} else {
						if (!gf_bt_check_ndt(parser, &targetField, inf->new_node, targetNode)) goto err;
					}

					gf_node_list_add_child_last(& inf->node_list, newnode, &last);
				}
			}
			break;
			default:
				inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
				info.far_ptr = inf->field_ptr;
				info.fieldType = inf->fieldType;

				if (gf_sg_vrml_is_sf_field(inf->fieldType)) {
					gf_bt_sffield(parser, &info, childNode ? childNode : targetNode);
				} else {
					gf_bt_mffield(parser, &info, childNode ? childNode : targetNode);
				}
				if (parser->last_error) goto err;
				break;
			}
		}

		if (parser->last_error) goto err;
		gf_list_add(cmdList, com);
		parser->cur_com = com;
		return parser->last_error;
	}


	/*INSERT commands*/
	if (!strcmp(str, "INSERT") || !strcmp(str, "APPEND")) {
		Bool is_append = !strcmp(str, "APPEND") ? 1 : 0;
		str = gf_bt_get_next(parser, 0);
		if (!strcmp(str, "ROUTE")) {
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_ROUTE_INSERT);
			gf_bt_parse_route(parser, 0, 1, com);
			if (parser->last_error) goto err;
			gf_list_add(cmdList, com);
			gf_list_add(parser->inserted_routes, com);
			parser->cur_com = com;
			return GF_OK;
		}
		if (strcmp(str, "AT") && strcmp(str, "TO")) {
			return gf_bt_report(parser, GF_BAD_PARAM, (char*) (is_append ? "TO expected got %s" : "AT expected got %s"), str);
		}
		str = gf_bt_get_next(parser, 1);
		strcpy(field, str);
		n = gf_bt_peek_node(parser, str);
		if (!n) {
			return gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown node", field);
		}
		if (!gf_bt_check_code(parser, '.')) {
			return gf_bt_report(parser, GF_BAD_PARAM, ". expected");
		}
		str = gf_bt_get_next(parser, 1);
		strcpy(field, str);
		if (!is_append) {
			if (!gf_bt_check_code(parser, '[')) {
				return gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
			}
			gf_bt_parse_int(parser, "index", &pos);
			if (!gf_bt_check_code(parser, ']')) {
				return gf_bt_report(parser, GF_BAD_PARAM, "] expected");
			}
		} else {
			if (gf_bt_check_code(parser, '[')) {
				return gf_bt_report(parser, GF_BAD_PARAM, "[ unexpected in Append command");
			}
			pos = -1;
		}
		gf_node_get_field_by_name(n, field, &info);
		if (!strcmp(field, "children")) {
			newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
			if (parser->last_error) goto err;

			if (!gf_bt_check_ndt(parser, &info, newnode, n)) goto err;
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_NODE_INSERT);
			bd_set_com_node(com, n);
			inf = gf_sg_command_field_new(com);
			inf->pos = pos;
			inf->new_node = newnode;
			inf->fieldType = GF_SG_VRML_SFNODE;
			inf->field_ptr = &inf->new_node;
			if (parser->last_error) goto err;
			parser->cur_com = com;
			return gf_list_add(cmdList, com);
		}
		if (gf_sg_vrml_is_sf_field(info.fieldType)) {
			gf_bt_report(parser, GF_BAD_PARAM, "%s: MF type field expected", info.name);
			goto err;
		}
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_INDEXED_INSERT);
		bd_set_com_node(com, n);
		inf = gf_sg_command_field_new(com);
		inf->pos = pos;
		inf->fieldIndex = info.fieldIndex;
		inf->fieldType = gf_sg_vrml_get_sf_type(info.fieldType);
		switch (info.fieldType) {
		case GF_SG_VRML_MFNODE:
			inf->new_node = gf_bt_sf_node(parser, NULL, NULL, NULL);
			inf->field_ptr = &inf->new_node;
			break;
		default:
			info.fieldType = inf->fieldType;
			inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			info.far_ptr = inf->field_ptr;
			gf_bt_sffield(parser, &info, n);
			break;
		}
		if (parser->last_error) goto err;
		gf_list_add(cmdList, com);
		parser->cur_com = com;
		return parser->last_error;
	}
	/*DELETE commands*/
	if (!strcmp(str, "DELETE")) {
		str = gf_bt_get_next(parser, 1);
		if (!strcmp(str, "ROUTE")) {
			str = gf_bt_get_next(parser, 0);
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_ROUTE_DELETE);
			com->RouteID = gf_bt_get_route(parser, str);
			if (!com->RouteID) {
				com->unres_name = gf_strdup(str);
				com->unresolved = 1;
				gf_list_add(parser->unresolved_routes, com);
			}
			/*for bt<->xmt conversions*/
			com->def_name = gf_strdup(str);
			return gf_list_add(cmdList, com);
		}
		strcpy(field, str);
		n = gf_bt_peek_node(parser, str);
		if (!n) {
			return gf_bt_report(parser, GF_BAD_PARAM, "DELETE %s: Unknown Node", field);
		}
		if (!gf_bt_check_code(parser, '.')) {
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_NODE_DELETE);
			bd_set_com_node(com, n);
			return gf_list_add(cmdList, com);
		}
		str = gf_bt_get_next(parser, 0);
		if (gf_node_get_field_by_name(n, str, &info) != GF_OK) {
			return gf_bt_report(parser, GF_BAD_PARAM, "%s not a field of node %s", str, gf_node_get_class_name(n) );
		}
		if (gf_bt_check_code(parser, '[')) {
			gf_bt_parse_int(parser, "index", &pos);
			if (!gf_bt_check_code(parser, ']'))
				return gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
		}
		if (gf_sg_vrml_is_sf_field(info.fieldType)) {
			if (info.fieldType == GF_SG_VRML_SFNODE) {
				com = gf_sg_command_new(parser->load->scene_graph, GF_SG_FIELD_REPLACE);
				bd_set_com_node(com, n);
				inf = gf_sg_command_field_new(com);
				inf->fieldIndex = info.fieldIndex;
				inf->fieldType = info.fieldType;
				inf->new_node = NULL;
				inf->field_ptr = &inf->new_node;
				return gf_list_add(cmdList, com);
			}
			return gf_bt_report(parser, GF_BAD_PARAM, "%s is an SFField - cannot indexed delete", info.name);
		}

		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_INDEXED_DELETE);
		bd_set_com_node(com, n);
		inf = gf_sg_command_field_new(com);
		inf->fieldIndex = info.fieldIndex;
		inf->fieldType = info.fieldType;
		inf->pos = pos;
		return gf_list_add(cmdList, com);
	}
	/*Extended BIFS commands*/

	/*GlobalQP commands*/
	if (!strcmp(str, "GLOBALQP")) {
		newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
		if (newnode && (newnode->sgprivate->tag != TAG_MPEG4_QuantizationParameter)) {
			gf_bt_report(parser, GF_BAD_PARAM, "Only QuantizationParameter node allowed in GLOBALQP");
			gf_node_unregister(newnode, NULL);
			return parser->last_error;
		}
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_GLOBAL_QUANTIZER);
		com->node = NULL;
		inf = gf_sg_command_field_new(com);
		inf->new_node = newnode;
		inf->field_ptr = &inf->new_node;
		inf->fieldType = GF_SG_VRML_SFNODE;
		return gf_list_add(cmdList, com);
	}

	/*MultipleReplace commands*/
	if (!strcmp(str, "MULTIPLEREPLACE")) {
		str = gf_bt_get_next(parser, 0);
		strcpy(field, str);
		n = gf_bt_peek_node(parser, str);
		if (!n) {
			return gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown node", field);
		}
		if (!gf_bt_check_code(parser, '{')) {
			return gf_bt_report(parser, GF_BAD_PARAM, "{ expected");
		}
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_MULTIPLE_REPLACE);
		bd_set_com_node(com, n);

		while (!gf_bt_check_code(parser, '}')) {
			str = gf_bt_get_next(parser, 0);
			parser->last_error = gf_node_get_field_by_name(n, str, &info);
			if (parser->last_error) {
				gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown node field", str);
				goto err;
			}
			inf = gf_sg_command_field_new(com);
			inf->fieldIndex = info.fieldIndex;
			inf->fieldType = info.fieldType;
			inf->pos = -1;

			switch (info.fieldType) {
			case GF_SG_VRML_SFNODE:
				inf->new_node = gf_bt_sf_node(parser, NULL, NULL, NULL);
				if (parser->last_error) goto err;
				if (!gf_bt_check_ndt(parser, &info, inf->new_node, n)) goto err;
				inf->field_ptr = &inf->new_node;
				break;
			case GF_SG_VRML_MFNODE:
			{
				GF_ChildNodeItem *last = NULL;
				if (!gf_bt_check_code(parser, '[')) {
					gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
					goto err;
				}
				info.far_ptr = inf->field_ptr = &inf->node_list;
				while (!gf_bt_check_code(parser, ']')) {
					newnode = gf_bt_sf_node(parser, NULL, NULL, NULL);
					if (parser->last_error!=GF_OK) goto err;
					if (!gf_bt_check_ndt(parser, &info, newnode, n)) goto err;
					gf_node_list_add_child_last( & inf->node_list, newnode, &last);
				}
			}
			break;
			default:
				info.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
				if (gf_sg_vrml_is_sf_field(info.fieldType)) {
					gf_bt_sffield(parser, &info, n);
				} else {
					gf_bt_mffield(parser, &info, n);
				}
				if (parser->last_error) goto err;
				break;
			}
		}
		parser->cur_com = com;
		return gf_list_add(cmdList, com);
	}

	/*MultipleIndexReplace commands*/
	if (!strcmp(str, "MULTIPLEINDREPLACE")) {
		str = gf_bt_get_next(parser, 1);
		strcpy(field, str);
		n = gf_bt_peek_node(parser, str);
		if (!n) {
			return gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown node", field);
		}
		if (!gf_bt_check_code(parser, '.')) {
			return gf_bt_report(parser, GF_BAD_PARAM, ". expected");
		}
		str = gf_bt_get_next(parser, 0);
		parser->last_error = gf_node_get_field_by_name(n, str, &info);
		if (parser->last_error) {
			return gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown field", info.name);
		}
		if (gf_sg_vrml_is_sf_field(info.fieldType)) {
			return gf_bt_report(parser, GF_BAD_PARAM, "Only MF field allowed");
		}
		if (!gf_bt_check_code(parser, '[')) {
			return gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
		}

		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_MULTIPLE_INDEXED_REPLACE);
		bd_set_com_node(com, n);
		info.fieldType = gf_sg_vrml_get_sf_type(info.fieldType);

		while (!gf_bt_check_code(parser, ']')) {
			pos=0;
			if (gf_bt_parse_int(parser, "position", (SFInt32 *)&pos)) goto err;
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "BY")) {
				gf_bt_report(parser, GF_BAD_PARAM, "BY expected");
				goto err;
			}
			inf = gf_sg_command_field_new(com);
			inf->fieldIndex = info.fieldIndex;
			inf->fieldType = info.fieldType;
			inf->pos = pos;
			if (inf->fieldType==GF_SG_VRML_SFNODE) {
				info.far_ptr = inf->field_ptr = &inf->new_node;
				inf->new_node = gf_bt_sf_node(parser, NULL, NULL, NULL);
				if (parser->last_error) goto err;
				if (!gf_bt_check_ndt(parser, &info, inf->new_node, n)) goto err;
				inf->field_ptr = &inf->new_node;
			} else {
				info.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
				gf_bt_sffield(parser, &info, n);
				if (parser->last_error) goto err;
			}
		}
		parser->cur_com = com;
		return gf_list_add(cmdList, com);
	}

	if (!strcmp(str, "XDELETE")) {
		str = gf_bt_get_next(parser, 1);
		strcpy(field, str);
		n = gf_bt_peek_node(parser, str);
		if (!n) {
			return gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown Node", field);
		}
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_NODE_DELETE_EX);
		bd_set_com_node(com, n);
		return gf_list_add(cmdList, com);
	}

	if (!strcmp(str, "INSERTPROTO")) {
		if (!gf_bt_check_code(parser, '[')) {
			return gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
		}
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_PROTO_INSERT);
		while (!gf_bt_check_code(parser, ']')) {
			parser->last_error = gf_bt_parse_proto(parser, NULL, com->new_proto_list);
			if (parser->last_error) goto err;
		}
		gf_list_add(cmdList, com);
		return GF_OK;
	}
	if (!strcmp(str, "DELETEPROTO")) {
		if (!gf_bt_check_code(parser, '[')) {
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_PROTO_DELETE_ALL);
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "ALL")) {
				gf_bt_report(parser, GF_BAD_PARAM, "ALL expected");
				goto err;
			}
			return gf_list_add(cmdList, com);
		}
		com = gf_sg_command_new(parser->load->scene_graph, GF_SG_PROTO_DELETE);
		while (!gf_bt_check_code(parser, ']')) {
			GF_Proto *proto;
			str = gf_bt_get_next(parser, 0);
			proto = gf_sg_find_proto(parser->load->scene_graph, 0, str);
			if (!proto) {
				gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown proto", str);
				goto err;
			}
			com->del_proto_list = (u32*)gf_realloc(com->del_proto_list, sizeof(u32)*(com->del_proto_list_size+1));
			com->del_proto_list[com->del_proto_list_size] = proto->ID;
			com->del_proto_list_size++;
		}
		gf_list_add(cmdList, com);
		return GF_OK;
	}
	return gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown command syntax, str");

err:
	if (com) gf_sg_command_del(com);
	return parser->last_error;
}

GF_Descriptor *gf_bt_parse_descriptor(GF_BTParser *parser, char *name);

#ifndef GPAC_MINIMAL_ODF
GF_IPMPX_Data *gf_bt_parse_ipmpx(GF_BTParser *parser, char *name)
{
	char *str, field[500];
	GF_IPMPX_Data *desc, *subdesc;
	GF_Descriptor *oddesc;
	GF_Err e;
	u32 type;
	u8 tag;
	if (name) {
		str = name;
	} else {
		str = gf_bt_get_next(parser, 0);
	}
	tag = gf_ipmpx_get_tag(str);
	if (!tag) {
		gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown IPMPX Data", str);
		return NULL;
	}
	desc = gf_ipmpx_data_new(tag);

	if (!desc) return NULL;
	if (!gf_bt_check_code(parser, '{')) return desc;

	while (1) {
		/*done*/
		if (gf_bt_check_code(parser, '}')) break;
		str = gf_bt_get_next(parser, 0);
		strcpy(field, str);
		type = gf_ipmpx_get_field_type(desc, str);
		switch (type) {
		/*single descriptor*/
		case GF_ODF_FT_OD:
			assert(desc->tag==GF_IPMPX_CONNECT_TOOL_TAG);
			str = gf_bt_get_next(parser, 0);
			oddesc = gf_bt_parse_descriptor(parser, str);
			if (!oddesc) {
				gf_bt_report(parser, GF_BAD_PARAM, "Unknown desc %s in field %s", str, field);
				gf_ipmpx_data_del(desc);
				return NULL;
			}
			assert(oddesc->tag==GF_ODF_IPMP_TAG);
			((GF_IPMPX_ConnectTool *)desc)->toolDescriptor = (GF_IPMP_Descriptor *)oddesc;
			break;
		/*descriptor list*/
		case GF_ODF_FT_OD_LIST:
			assert(desc->tag==GF_IPMPX_GET_TOOLS_RESPONSE_TAG);
			if (gf_bt_check_code(parser, '[')) {
				while (!gf_bt_check_code(parser, ']')) {
					GF_Descriptor *ipmp_t = gf_bt_parse_descriptor(parser, NULL);
					if (!ipmp_t) {
						gf_ipmpx_data_del(desc);
						parser->last_error = GF_BAD_PARAM;
						return NULL;
					}
					assert(ipmp_t->tag==GF_ODF_IPMP_TOOL_TAG);
					gf_list_add( ((GF_IPMPX_GetToolsResponse *)desc)->ipmp_tools, ipmp_t);
				}
			}
			break;

		/*IPMPX ByteArray list*/
		case GF_ODF_FT_IPMPX_BA_LIST:
			if (gf_bt_check_code(parser, '[')) {
				while (!gf_bt_check_code(parser, ']')) {
					str = gf_bt_get_next(parser, 0);
					if (!str) continue;
					if (gf_ipmpx_set_byte_array(desc, field, str) != GF_OK) {
						gf_bt_report(parser, GF_OK, "Invalid ipmpx %s in field %s - skipping", str, field);
					}
					gf_bt_check_code(parser, ',');
				}
			}
			break;
		/*IPMPX ByteArray: check if declared as sub-data or not*/
		case GF_ODF_FT_IPMPX_BA:
			str = NULL;
			if (gf_bt_check_code(parser, '{')) {
				str = gf_bt_get_next(parser, 0);
				if (stricmp(str, "array")) {
					gf_bt_report(parser, GF_BAD_PARAM, "IPMP ByteArray syntax is %s { array \"...\" } or %s \"....\"\n", field, field);
					gf_ipmpx_data_del(desc);
					return NULL;
				}
				str = gf_bt_get_next(parser, 0);
				gf_bt_check_code(parser, '}');
			} else {
				str = gf_bt_get_next(parser, 0);
			}
			e = gf_ipmpx_set_byte_array(desc, field, str);
			if (e) {
				gf_bt_report(parser, e, "Error assigning IPMP ByteArray %s\n", field);
				gf_ipmpx_data_del(desc);
				return NULL;
			}
			break;
		/*IPMPX Data list*/
		case GF_ODF_FT_IPMPX_LIST:
			if (gf_bt_check_code(parser, '[')) {
				while (!gf_bt_check_code(parser, ']')) {
					subdesc = gf_bt_parse_ipmpx(parser, NULL);
					if (!subdesc) {
						gf_ipmpx_data_del(desc);
						parser->last_error = GF_BAD_PARAM;
						return NULL;
					}
					if (gf_ipmpx_set_sub_data(desc, field, subdesc) != GF_OK) {
						gf_bt_report(parser, GF_BAD_PARAM, "Invalid ipmpx %s in field %s - skipping", str, field);
						gf_ipmpx_data_del(subdesc);
					}
				}
			}
			break;
		/*regular IPMPX Data*/
		case GF_ODF_FT_IPMPX:
			str = gf_bt_get_next(parser, 0);
			subdesc = gf_bt_parse_ipmpx(parser, str);
			if (!subdesc) {
				gf_bt_report(parser, GF_BAD_PARAM, "Unknown ipmpx %s in field %s", str, field);
				gf_ipmpx_data_del(desc);
				return NULL;
			}
			if (gf_ipmpx_set_sub_data(desc, field, subdesc) != GF_OK) {
				gf_bt_report(parser, GF_BAD_PARAM, "Invalid ipmpx in field %s - skipping", field);
				gf_ipmpx_data_del(subdesc);
			}
			break;
		default:
			str = gf_bt_get_next(parser, 0);
			parser->last_error = gf_ipmpx_set_field(desc, field, str);

			if (parser->last_error) {
				gf_bt_report(parser, GF_BAD_PARAM, "Invalid value %s in field %s", str, field);
				gf_ipmpx_data_del(desc);
				return NULL;
			}
			break;
		}
	}
	return desc;
}
#endif

static void gf_bt_add_desc(GF_BTParser *parser, GF_Descriptor *par, GF_Descriptor *child, char *fieldName)
{
	GF_Err e = gf_odf_desc_add_desc(par, child);
	if (e) {
		gf_bt_report(parser, GF_OK, "Invalid child descriptor in field %s - skipping", fieldName);
		gf_odf_desc_del(child);
	}
}

GF_Descriptor *gf_bt_parse_descriptor(GF_BTParser *parser, char *name)
{
	char *str, field[500];
	GF_Descriptor *desc, *subdesc;
	u32 type;
	u8 tag;
	if (name) {
		str = name;
	} else {
		str = gf_bt_get_next(parser, 0);
	}
	tag = gf_odf_get_tag_by_name(str);
	if (!tag) {
		gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown descriptor", str);
		return NULL;
	}
	desc = gf_odf_desc_new(tag);

	if (!desc) return NULL;
	if (!gf_bt_check_code(parser, '{')) return desc;

	while (1) {
		Bool is_anim_mask = 0;
		/*done*/
		if (gf_bt_check_code(parser, '}')) break;
		str = gf_bt_get_next(parser, 0);
		strcpy(field, str);

		if ((tag==GF_ODF_BIFS_CFG_TAG) && !strcmp(field, "animationMask")) {
			gf_bt_get_next(parser, 0);
			if (gf_bt_check_code(parser, '{')) is_anim_mask = 1;
			str = gf_bt_get_next(parser, 0);
			strcpy(field, str);
		}

		type = gf_odf_get_field_type(desc, str);
		switch (type) {
#ifndef GPAC_MINIMAL_ODF
		/*IPMPX list*/
		case GF_ODF_FT_IPMPX_LIST:
			if(desc->tag!=GF_ODF_IPMP_TAG) {
				gf_bt_report(parser, GF_BAD_PARAM, "IPMPX_Data list only allowed in GF_IPMP_Descriptor");
				gf_odf_desc_del(desc);
				return NULL;
			}
			if (gf_bt_check_code(parser, '[')) {
				while (!gf_bt_check_code(parser, ']')) {
					GF_IPMPX_Data *ipmpx = gf_bt_parse_ipmpx(parser, NULL);
					if (!ipmpx) {
						gf_odf_desc_del(desc);
						parser->last_error = GF_BAD_PARAM;
						return NULL;
					}
					gf_list_add( ((GF_IPMP_Descriptor *)desc)->ipmpx_data, ipmpx);
				}
			}
			break;
		/*IPMPX*/
		case GF_ODF_FT_IPMPX:
			if(desc->tag!=GF_ODF_IPMP_TOOL_TAG) {
				gf_bt_report(parser, GF_BAD_PARAM, "IPMPX_Data only allowed in GF_IPMP_Tool");
				gf_odf_desc_del(desc);
				return NULL;
			}
			if (gf_bt_check_code(parser, '[')) {
				while (!gf_bt_check_code(parser, ']')) {
					GF_IPMPX_Data *ipmpx = gf_bt_parse_ipmpx(parser, NULL);
					if (!ipmpx) {
						gf_odf_desc_del(desc);
						parser->last_error = GF_BAD_PARAM;
						return NULL;
					}
					if (ipmpx->tag==GF_IPMPX_PARAMETRIC_DESCRIPTION_TAG) {
						GF_IPMP_Tool *it = (GF_IPMP_Tool *)desc;
						if (it->toolParamDesc) gf_ipmpx_data_del((GF_IPMPX_Data *)it->toolParamDesc);
						it->toolParamDesc = (GF_IPMPX_ParametricDescription*)ipmpx;
					} else {
						gf_bt_report(parser, GF_OK, "Only ToolParametricDescription allowed in GF_IPMP_Tool - skipping");
						gf_ipmpx_data_del(ipmpx);
					}
				}
			}
			break;
#endif

		/*descriptor list*/
		case GF_ODF_FT_OD_LIST:
			if (gf_bt_check_code(parser, '[')) {
				while (!gf_bt_check_code(parser, ']')) {
					subdesc = gf_bt_parse_descriptor(parser, NULL);
					if (!subdesc) {
						gf_odf_desc_del(desc);
						parser->last_error = GF_BAD_PARAM;
						return NULL;
					}
					gf_bt_add_desc(parser, desc, subdesc, field);
				}
			}
			if (is_anim_mask)
				gf_bt_check_code(parser, '}');
			break;
		/*single descriptor*/
		case GF_ODF_FT_OD:
			str = gf_bt_get_next(parser, 0);
			subdesc = gf_bt_parse_descriptor(parser, str);
			if (!subdesc) {
				gf_bt_report(parser, GF_BAD_PARAM, "Unknown desc %s in field %s", str, field);
				gf_odf_desc_del(desc);
				return NULL;
			}
			gf_bt_add_desc(parser, desc, subdesc, field);
			break;
		/*regular field*/
		default:
			str = gf_bt_get_next(parser, 0);
			parser->last_error = gf_odf_set_field(desc, field, str);

			if (parser->last_error) {
				gf_bt_report(parser, GF_BAD_PARAM, "Invalid value %s in field %s", str, field);
				gf_odf_desc_del(desc);
				return NULL;
			}
			break;
		}
	}
	if (desc->tag == GF_ODF_BIFS_CFG_TAG) {
		GF_BIFSConfig *bcfg = (GF_BIFSConfig *)desc;
		if (!parser->load->ctx->scene_width) {
			parser->load->ctx->scene_width = bcfg->pixelWidth;
			parser->load->ctx->scene_height = bcfg->pixelHeight;
			parser->load->ctx->is_pixel_metrics = bcfg->pixelMetrics;
		}

		/*for bt->xmt*/
		if (!bcfg->version) bcfg->version = 1;
	}
	else if (desc->tag==GF_ODF_ESD_TAG) {
		GF_ESD *esd  =(GF_ESD*)desc;
		if (esd->decoderConfig) {
			GF_StreamContext *sc=NULL;
			GF_MuxInfo *mux;
			/*watchout for default BIFS stream*/
			if (parser->bifs_es && !parser->base_bifs_id && (esd->decoderConfig->streamType==GF_STREAM_SCENE)) {
				parser->bifs_es->ESID = parser->base_bifs_id = esd->ESID;
				parser->bifs_es->timeScale = (esd->slConfig && esd->slConfig->timestampResolution) ? esd->slConfig->timestampResolution : 1000;
				sc = parser->bifs_es;
			} else {
				sc = gf_sm_stream_new(parser->load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
				/*set default timescale for systems tracks (ignored for other)*/
				if (sc) sc->timeScale = (esd->slConfig && esd->slConfig->timestampResolution) ? esd->slConfig->timestampResolution : 1000;
				/*assign base OD*/
				if (!parser->base_od_id && (esd->decoderConfig->streamType==GF_STREAM_OD)) parser->base_od_id = esd->ESID;
			}
			/*assign broadcast parameter tools*/
			mux = gf_sm_get_mux_info(esd);
			if (sc && mux) {
				sc->aggregate_on_esid = mux->aggregate_on_esid;
				if (!mux->carousel_period_plus_one) sc->carousel_period  = (u32) -1;
				else sc->carousel_period = mux->carousel_period_plus_one - 1;
			}
		}
	} else if (desc->tag==GF_ODF_MUXINFO_TAG) {
		GF_MuxInfo *mi = (GF_MuxInfo *)desc;
		if (! mi->src_url) {
			mi->src_url = gf_strdup(parser->load->src_url ? parser->load->src_url : parser->load->fileName);
		}
	}
	return desc;
}

void gf_bt_parse_od_command(GF_BTParser *parser, char *name)
{
	u32 val=0;
	char *str;

	if (!strcmp(name, "UPDATE")) {
		str = gf_bt_get_next(parser, 0);
		/*OD update*/
		if (!strcmp(str, "OD")) {
			GF_ODUpdate *odU;
			if (!gf_bt_check_code(parser, '[')) {
				gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
				return;
			}
			odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
			gf_list_add(parser->od_au->commands, odU);
			while (!parser->done) {
				GF_Descriptor *desc;
				str = gf_bt_get_next(parser, 0);
				if (gf_bt_check_code(parser, ']')) break;
				if (strcmp(str, "ObjectDescriptor") && strcmp(str, "InitialObjectDescriptor")) {
					gf_bt_report(parser, GF_BAD_PARAM, "Object Descriptor expected got %s", str);
					break;
				}
				desc = gf_bt_parse_descriptor(parser, str);
				if (!desc) break;
				gf_list_add(odU->objectDescriptors, desc);
			}
			return;
		}
		/*ESD update*/
		if (!strcmp(str, "ESD")) {
			GF_ESDUpdate *esdU;
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "IN")) {
				gf_bt_report(parser, GF_BAD_PARAM, "IN expected got %s", str);
				return;
			}
			esdU = (GF_ESDUpdate *) gf_odf_com_new(GF_ODF_ESD_UPDATE_TAG);
			parser->last_error = gf_bt_parse_int(parser, "OD_ID", (SFInt32*)&val);
			if (parser->last_error) return;
			esdU->ODID = val;
			gf_list_add(parser->od_au->commands, esdU);

			if (!gf_bt_check_code(parser, '[')) {
				str = gf_bt_get_next(parser, 0);
				if (strcmp(str, "esDescr")) {
					gf_bt_report(parser, GF_BAD_PARAM, "esDescr expected got %s", str);
					return;
				}
				if (!gf_bt_check_code(parser, '[')) {
					gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
					return;
				}
			}

			while (!parser->done) {
				GF_Descriptor *desc;
				str = gf_bt_get_next(parser, 0);
				if (gf_bt_check_code(parser, ']')) break;
				if (strcmp(str, "ES_Descriptor")) {
					gf_bt_report(parser, GF_BAD_PARAM, "ES_Descriptor expected got %s", str);
					break;
				}
				desc = gf_bt_parse_descriptor(parser, str);
				if (!desc) break;
				gf_list_add(esdU->ESDescriptors, desc);
			}
			return;
		}
		/*IPMP descriptor update*/
		if (!strcmp(str, "IPMPD") || !strcmp(str, "IPMPDX")) {
			GF_IPMPUpdate *ipU;
			if (!gf_bt_check_code(parser, '[')) {
				gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
				return;
			}
			ipU = (GF_IPMPUpdate *) gf_odf_com_new(GF_ODF_IPMP_UPDATE_TAG);
			gf_list_add(parser->od_au->commands, ipU);
			while (!parser->done) {
				GF_Descriptor *desc;
				str = gf_bt_get_next(parser, 0);
				if (gf_bt_check_code(parser, ']')) break;
				if (strcmp(str, "IPMP_Descriptor")) {
					gf_bt_report(parser, GF_BAD_PARAM, "IPMP_Descriptor expected got %s", str);
					break;
				}
				desc = gf_bt_parse_descriptor(parser, str);
				if (!desc) break;
				gf_list_add(ipU->IPMPDescList, desc);
			}
			return;
		}
		gf_bt_report(parser, GF_BAD_PARAM, "unknown OD command", str);
		return;
	}
	if (!strcmp(name, "REMOVE")) {
		str = gf_bt_get_next(parser, 0);
		/*OD remove*/
		if (!strcmp(str, "OD")) {
			GF_ODRemove *odR;
			if (!gf_bt_check_code(parser, '[')) {
				gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
				return;
			}
			odR = (GF_ODRemove *) gf_odf_com_new(GF_ODF_OD_REMOVE_TAG);
			gf_list_add(parser->od_au->commands, odR);
			while (!parser->done) {
				u32 id;
				if (gf_bt_check_code(parser, ']')) break;
				gf_bt_parse_int(parser, "ODID", (SFInt32*)&id);
				if (parser->last_error) return;
				odR->OD_ID = (u16*)gf_realloc(odR->OD_ID, sizeof(u16) * (odR->NbODs+1));
				odR->OD_ID[odR->NbODs] = id;
				odR->NbODs++;
			}
			return;
		}
		/*ESD remove*/
		if (!strcmp(str, "ESD")) {
			u32 odid;
			GF_ESDRemove *esdR;
			str = gf_bt_get_next(parser, 0);
			if (strcmp(str, "FROM")) {
				gf_bt_report(parser, GF_BAD_PARAM, "FROM expected got %s", str);
				return;
			}
			gf_bt_parse_int(parser, "ODID", (SFInt32*)&odid);
			if (parser->last_error) return;

			if (!gf_bt_check_code(parser, '[')) {
				gf_bt_report(parser, GF_BAD_PARAM, "[ expected");
				return;
			}
			esdR = (GF_ESDRemove *) gf_odf_com_new(GF_ODF_ESD_REMOVE_TAG);
			esdR->ODID = odid;
			gf_list_add(parser->od_au->commands, esdR);
			while (!parser->done) {
				u32 id;
				if (gf_bt_check_code(parser, ']')) break;
				gf_bt_parse_int(parser, "ES_ID", (SFInt32*)&id);
				if (parser->last_error) return;
				esdR->ES_ID = (u16*)gf_realloc(esdR->ES_ID, sizeof(u16) * (esdR->NbESDs+1));
				esdR->ES_ID[esdR->NbESDs] = id;
				esdR->NbESDs++;
			}
			return;
		}
		gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown OD command", str);
		return;
	}
}



GF_Err gf_bt_loader_run_intern(GF_BTParser *parser, GF_Command *init_com, Bool initial_run)
{
	char *str;
	GF_Node *node, *vrml_root_node;
	Bool force_new_com;
	GF_Route *r;
	Bool has_id;
	char szDEFName[1000];

	vrml_root_node = NULL;
	has_id = 0;

	if (init_com)
		parser->in_com = 0 ;

	parser->cur_com = init_com;

	force_new_com = (parser->load->flags & GF_SM_LOAD_CONTEXT_READY) ? 1 : 0;


	/*create a default root node for all VRML nodes*/
	if ((parser->is_wrl && !parser->top_nodes) && !vrml_root_node) {
		if (initial_run ) {
#ifndef GPAC_DISABLE_X3D
			vrml_root_node = gf_node_new(parser->load->scene_graph, (parser->load->flags & GF_SM_LOAD_MPEG4_STRICT) ? TAG_MPEG4_Group : TAG_X3D_Group);
#else
			vrml_root_node = gf_node_new(parser->load->scene_graph, TAG_MPEG4_Group);
#endif
			gf_node_register(vrml_root_node, NULL);
			gf_node_init(vrml_root_node);
			gf_sg_set_root_node(parser->load->scene_graph, vrml_root_node);
		} else {
			vrml_root_node = gf_sg_get_root_node(parser->load->scene_graph);
		}
	}

	if (!parser->in_com)
		parser->stream_id = parser->load->force_es_id;

	/*parse all top-level items*/
	while (!parser->last_error) {
		str = gf_bt_get_next(parser, 0);
		if (parser->done) break;

		/*X3D specific things (ignored for now)*/
		if (!strcmp(str, "PROFILE")) gf_bt_force_line(parser);
		else if (!strcmp(str, "COMPONENT")) gf_bt_force_line(parser);
		else if (!strcmp(str, "META")) gf_bt_force_line(parser);
		else if (!strcmp(str, "IMPORT") || !strcmp(str, "EXPORT")) {
			gf_bt_report(parser, GF_NOT_SUPPORTED, "X3D IMPORT/EXPORT not implemented");
			break;
		}

		/*IOD*/
		else if (!strcmp(str, "InitialObjectDescriptor") || !strcmp(str, "ObjectDescriptor")) {
			parser->load->ctx->root_od = (GF_ObjectDescriptor *) gf_bt_parse_descriptor(parser, str);
		}
		/*explicit command*/
		else if (!strcmp(str, "AT") || !strcmp(str, "RAP")) {
			parser->au_is_rap = 0;
			if (!strcmp(str, "RAP")) {
				parser->au_is_rap = 1;
				str = gf_bt_get_next(parser, 0);
				if (strcmp(str, "AT")) {
					gf_bt_report(parser, GF_BAD_PARAM, "AT expected got %s", str);
					parser->last_error = GF_BAD_PARAM;
					break;
				}
			}
			force_new_com = 0;
			str = gf_bt_get_next(parser, 0);
			if (str[0] == 'D') {
				parser->au_time += atoi(&str[1]);
			} else {
				if (sscanf(str, "%u", &parser->au_time) != 1) {
					gf_bt_report(parser, GF_BAD_PARAM, "Number expected got %s", str);
					break;
				}
			}
			if (parser->last_error) break;
			/*reset all contexts*/
			if (parser->od_au && (parser->od_au->timing != parser->au_time)) parser->od_au = NULL;
			if (parser->bifs_au && (parser->bifs_au->timing != parser->au_time)) {
				gf_bt_check_unresolved_nodes(parser);
				parser->bifs_au = NULL;
			}

			parser->stream_id = 0;
			/*fix for mp4tool bt which doesn't support RAP signaling: assume the first AU
			is always RAP*/
			if (!parser->au_time) parser->au_is_rap = 1;

			parser->in_com = 1;

			if (!gf_bt_check_code(parser, '{')) {
				str = gf_bt_get_next(parser, 0);
				if (!strcmp(str, "IN")) {
					gf_bt_parse_int(parser, "IN", (SFInt32*)&parser->stream_id);
					if (parser->last_error) break;
				}
				if (!gf_bt_check_code(parser, '{')) {
					gf_bt_report(parser, GF_BAD_PARAM, "{ expected");
				}
			}
			/*done loading init frame*/
			if (init_com && parser->au_time) break;
		}
		else if (!strcmp(str, "PROTO") || !strcmp(str, "EXTERNPROTO")) {
			gf_bt_parse_proto(parser, str, init_com ? init_com->new_proto_list : NULL);
		}
		/*compatibility for old bt (mp4tool) in ProtoLibs*/
		else if (!strcmp(str, "NULL")) {
		}
		else if (!strcmp(str, "DEF")) {
			str = gf_bt_get_next(parser, 0);
			strcpy(szDEFName, str);
			has_id = 1;
		}
		else if (!strcmp(str, "ROUTE")) {
			GF_Command *com = NULL;
			if (!parser->top_nodes && parser->bifs_au && !parser->is_wrl) {
				/*if doing a scene replace, we need route insert stuff*/
				com = gf_sg_command_new(parser->load->scene_graph, GF_SG_ROUTE_INSERT);
				gf_list_add(parser->bifs_au->commands, com);
				gf_list_add(parser->inserted_routes, com);
			}

			r = gf_bt_parse_route(parser, 1, 0, com);
			if (has_id) {
				u32 rID = gf_bt_get_route(parser, szDEFName);
				if (!rID) rID = gf_bt_get_next_route_id(parser);
				if (com) {
					com->RouteID = rID;
					com->def_name = gf_strdup(szDEFName);
					gf_sg_set_max_defined_route_id(parser->load->scene_graph, rID);
				} else if (r) {
					gf_sg_route_set_id(r, rID);
					gf_sg_route_set_name(r, szDEFName);
				}
				has_id = 0;
			}
		}
		/*OD commands*/
		else if (!strcmp(str, "UPDATE") || !strcmp(str, "REMOVE")) {
			Bool is_base_stream = parser->stream_id ? 0 : 1;
			if (!parser->stream_id || parser->stream_id==parser->bifs_es->ESID) parser->stream_id = parser->base_od_id;

			if (parser->od_es && (parser->od_es->ESID != parser->stream_id)) {
				GF_StreamContext *prev = parser->od_es;
				parser->od_es = gf_sm_stream_new(parser->load->ctx, (u16) parser->stream_id, GF_STREAM_OD, GF_CODECID_OD_V1);
				/*force new AU if stream changed*/
				if (parser->od_es != prev) {
					parser->bifs_au = NULL;
					parser->od_au = NULL;
				}
			}
			if (!parser->od_es) parser->od_es = gf_sm_stream_new(parser->load->ctx, (u16) parser->stream_id, GF_STREAM_OD, GF_CODECID_OD_V1);
			if (!parser->od_au) parser->od_au = gf_sm_stream_au_new(parser->od_es, parser->au_time, 0, parser->au_is_rap);
			gf_bt_parse_od_command(parser, str);
			if (is_base_stream) parser->stream_id= 0;
		}
		/*BIFS commands*/
		else if (!strcmp(str, "REPLACE") || !strcmp(str, "INSERT") || !strcmp(str, "APPEND") || !strcmp(str, "DELETE")
		         /*BIFS extended commands*/
		         || !strcmp(str, "GLOBALQP") || !strcmp(str, "MULTIPLEREPLACE") || !strcmp(str, "MULTIPLEINDREPLACE") || !strcmp(str, "XDELETE") || !strcmp(str, "DELETEPROTO") || !strcmp(str, "INSERTPROTO")
		         || !strcmp(str, "XREPLACE")
		        ) {
			Bool is_base_stream = parser->stream_id ? 0 : 1;

			if (!parser->stream_id) parser->stream_id = parser->base_bifs_id;
			if (!parser->stream_id || (parser->od_es && (parser->stream_id==parser->od_es->ESID)) ) parser->stream_id = parser->base_bifs_id;

			if (parser->bifs_es->ESID != parser->stream_id) {
				GF_StreamContext *prev = parser->bifs_es;
				parser->bifs_es = gf_sm_stream_new(parser->load->ctx, (u16) parser->stream_id, GF_STREAM_SCENE, GF_CODECID_BIFS);
				/*force new AU if stream changed*/
				if (parser->bifs_es != prev) {
					gf_bt_check_unresolved_nodes(parser);
					parser->bifs_au = NULL;
				}
			}
			if (force_new_com) {
				force_new_com = 0;
				parser->bifs_au = gf_list_last(parser->bifs_es->AUs);
				parser->au_time = (u32) (parser->bifs_au ? parser->bifs_au->timing : 0) + 1;
				parser->bifs_au = NULL;
			}

			if (!parser->bifs_au) parser->bifs_au = gf_sm_stream_au_new(parser->bifs_es, parser->au_time, 0, parser->au_is_rap);
			gf_bt_parse_bifs_command(parser, str, parser->bifs_au->commands);
			if (is_base_stream) parser->stream_id= 0;
		}
		/*implicit BIFS command on SFTopNodes only*/
		else if (!strcmp(str, "OrderedGroup")
		         || !strcmp(str, "Group")
		         || !strcmp(str, "Layer2D")
		         || !strcmp(str, "Layer3D")
		         /* VRML parsing: all nodes are allowed*/
		         || parser->is_wrl
		        )
		{

			node = gf_bt_sf_node(parser, str, vrml_root_node, has_id ? szDEFName : NULL);
			has_id = 0;
			if (!node) break;
			if (parser->top_nodes) {
				gf_list_add(parser->top_nodes, node);
			} else if (!vrml_root_node) {
				if (init_com) init_com->node = node;
				else if (parser->load->flags & GF_SM_LOAD_CONTEXT_READY) {
					GF_Command *com = gf_sg_command_new(parser->load->scene_graph, GF_SG_SCENE_REPLACE);
					assert(!parser->bifs_au);
					assert(parser->bifs_es);
					parser->bifs_au = gf_sm_stream_au_new(parser->bifs_es, 0, 0, 1);
					gf_list_add(parser->bifs_au->commands, com);
					com->node = node;
				}
			} else {
				gf_node_insert_child(vrml_root_node, node, -1);
			}

			/*
			if (!gf_sg_get_root_node(parser->load->scene_graph)) {
				gf_node_register(node, NULL);
				gf_sg_set_root_node(parser->load->scene_graph, node);
			}
			*/
		}

		/*if in command, check command end*/
		else {
			/*check command end*/
			if (/*in_com && */gf_bt_check_code(parser, '}')) parser->in_com = 0;
			else if (strlen(str)) {
				gf_bt_report(parser, GF_BAD_PARAM, "%s: Unknown top-level element", str);
			}
			parser->au_is_rap = 0;
		}
	}
	gf_bt_resolve_routes(parser, 0);
	gf_bt_check_unresolved_nodes(parser);

	/*load scripts*/
	while (gf_list_count(parser->scripts)) {
		GF_Node *n = (GF_Node *)gf_list_get(parser->scripts, 0);
		gf_list_rem(parser->scripts, 0);
		gf_sg_script_load(n);
	}
	return parser->last_error;
}

static GF_Err gf_sm_load_bt_initialize(GF_SceneLoader *load, const char *str, Bool input_only)
{
	u32 size;
	gzFile gzInput;
	GF_Err e;
	unsigned char BOM[5];
	GF_BTParser *parser = load->loader_priv;

	parser->last_error = GF_OK;

	if (load->fileName) {
		FILE *test = gf_fopen(load->fileName, "rb");
		if (!test) return GF_URL_ERROR;

		size = (u32) gf_fsize(test);
		gf_fclose(test);

		gzInput = gf_gzopen(load->fileName, "rb");
		if (!gzInput) return GF_IO_ERR;

		parser->line_buffer = (char *) gf_malloc(sizeof(char)*BT_LINE_SIZE);
		memset(parser->line_buffer, 0, sizeof(char)*BT_LINE_SIZE);
		parser->file_size = size;

		parser->line_pos = parser->line_size = 0;
		gf_gzgets(gzInput, (char*) BOM, 5);
		gf_gzseek(gzInput, 0, SEEK_SET);
		parser->gz_in = gzInput;

	} else {
		if (!str || (strlen(str)<5) ) {
			/*wait for first string data to be fed to the parser (for load from string)*/
			parser->initialized = 0;
			return GF_OK;
		}
		strncpy((char *) BOM, str, 5);
	}

	/*0: no unicode, 1: UTF-16BE, 2: UTF-16LE*/
	if ((BOM[0]==0xFF) && (BOM[1]==0xFE)) {
		if (!BOM[2] && !BOM[3]) {
			gf_bt_report(parser, GF_NOT_SUPPORTED, "UTF-32 Text Files not supported");
			return GF_NOT_SUPPORTED;
		} else {
			parser->unicode_type = 2;
			if (parser->gz_in) gf_gzseek(parser->gz_in, 2, SEEK_CUR);
		}
	} else if ((BOM[0]==0xFE) && (BOM[1]==0xFF)) {
		if (!BOM[2] && !BOM[3]) {
			gf_bt_report(parser, GF_NOT_SUPPORTED, "UTF-32 Text Files not supported");
			return GF_NOT_SUPPORTED;
		} else {
			parser->unicode_type = 1;
			if (parser->gz_in) gf_gzseek(parser->gz_in, 2, SEEK_CUR);
		}
	} else if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) {
		/*we handle UTF8 as asci*/
		parser->unicode_type = 0;
		if (parser->gz_in) gf_gzseek(parser->gz_in, 3, SEEK_CUR);
	}
	parser->initialized = 1;

	if ( load->fileName )
	{
		char *sep = gf_file_ext_start(load->fileName);
		if (sep && !strnicmp(sep, ".wrl", 4)) parser->is_wrl = 1;
	}

	if (input_only) return GF_OK;

	/*initalize default streams in the context*/

	/*chunk parsing*/
	if (load->flags & GF_SM_LOAD_CONTEXT_READY) {
		u32 i;
		GF_StreamContext *sc;
		if (!load->ctx) return GF_BAD_PARAM;

		/*restore context - note that base layer are ALWAYS declared BEFORE enhancement layers with gpac parsers*/
		i=0;
		while ((sc = (GF_StreamContext*)gf_list_enum(load->ctx->streams, &i))) {
			switch (sc->streamType) {
			case GF_STREAM_SCENE:
				if (!parser->bifs_es) parser->bifs_es = sc;
				break;
			case GF_STREAM_OD:
				if (!parser->od_es) parser->od_es = sc;
				break;
			default:
				break;
			}
		}
		/*need at least one scene stream*/
		if (!parser->bifs_es) {
			parser->bifs_es = gf_sm_stream_new(load->ctx, 0, GF_STREAM_SCENE, GF_CODECID_BIFS);
			parser->load->ctx->scene_width = 0;
			parser->load->ctx->scene_height = 0;
			parser->load->ctx->is_pixel_metrics = 1;
		}
		else parser->base_bifs_id = parser->bifs_es->ESID;
		if (parser->od_es) parser->base_od_id = parser->od_es->ESID;

		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("BT: MPEG-4 (BT) Scene Chunk Parsing"));
	}
	/*context is not initialized - check for VRML*/
	else {
		GF_Command *com;


		parser->load = NULL;
		gf_bt_check_line(parser);
		parser->load = load;
		if (parser->def_w && parser->def_h) {
			load->ctx->scene_width = parser->def_w;
			load->ctx->scene_height = parser->def_h;
		}

		/*create at least one empty BIFS stream*/
		if (!parser->is_wrl) {
			parser->bifs_es = gf_sm_stream_new(load->ctx, 0, GF_STREAM_SCENE, GF_CODECID_BIFS);
			parser->bifs_au = gf_sm_stream_au_new(parser->bifs_es, 0, 0, 1);
			parser->load->ctx->is_pixel_metrics = 1;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ( ((parser->is_wrl==2) ? "BT: X3D (WRL) Scene Parsing\n" : (parser->is_wrl ? "BT: VRML Scene Parsing\n" : "BT: MPEG-4 Scene Parsing\n")) ));

		/*default scene replace - we create it no matter what since it is used to store BIFS config when parsing IOD.*/
		com = NULL;
		if (!parser->is_wrl) {
			com = gf_sg_command_new(parser->load->scene_graph, GF_SG_SCENE_REPLACE);
			gf_list_add(parser->bifs_au->commands, com);
		}

		/*and perform initial load*/
		e = gf_bt_loader_run_intern(parser, com, 1);
		if (e) return e;
	}
	return GF_OK;
}

void load_bt_done(GF_SceneLoader *load)
{
	GF_BTParser *parser = (GF_BTParser *)load->loader_priv;
	if (!parser) return;
	gf_list_del(parser->unresolved_routes);
	gf_list_del(parser->inserted_routes);
	gf_list_del(parser->undef_nodes);
	gf_list_del(parser->def_nodes);
	gf_list_del(parser->peeked_nodes);
	while (gf_list_count(parser->def_symbols)) {
		BTDefSymbol *d = (BTDefSymbol *)gf_list_get(parser->def_symbols, 0);
		gf_list_rem(parser->def_symbols, 0);
		gf_free(d->name);
		gf_free(d->value);
		gf_free(d);
	}
	gf_list_del(parser->def_symbols);
	gf_list_del(parser->scripts);

	if (parser->gz_in) gf_gzclose(parser->gz_in);
	if (parser->line_buffer) gf_free(parser->line_buffer);
	gf_free(parser);
	load->loader_priv = NULL;
}

GF_Err load_bt_run(GF_SceneLoader *load)
{
	GF_Err e;
	GF_BTParser *parser = (GF_BTParser *)load->loader_priv;
	if (!parser) return GF_BAD_PARAM;

	if (!parser->initialized) {
		e = gf_sm_load_bt_initialize(load, NULL, 1);
		if (e) return e;
	}

	e = gf_bt_loader_run_intern(parser, NULL, 0);

	if ((e<0) || parser->done) {
		parser->done = 0;
		parser->initialized = 0;
		if (parser->gz_in) {
			gf_gzclose(parser->gz_in);
			parser->gz_in = NULL;
		}

		if (parser->line_buffer) {
			gf_free(parser->line_buffer);
			parser->line_buffer = NULL;
		}
		parser->file_size = 0;
		parser->line_pos = parser->line_size = 0;
		load->fileName = NULL;
	}
	return e;
}


GF_Err load_bt_parse_string(GF_SceneLoader *load, const char *str)
{
	GF_Err e;
	char *dup_str;
	GF_BTParser *parser = (GF_BTParser *)load->loader_priv;
	if (!parser) return GF_BAD_PARAM;

	if (parser->done) {
		parser->done = 0;
		parser->initialized = 0;
		parser->file_size = 0;
		parser->line_pos = 0;
	}
	parser->line_buffer = dup_str = gf_strdup(str);
	parser->line_size = (s32)strlen(str);

	if (!parser->initialized) {
		e = gf_sm_load_bt_initialize(load, str, 0);
		if (e) {
			gf_free(dup_str);
			return e;
		}
	}
	e = gf_bt_loader_run_intern(parser, NULL, 0);
	parser->line_buffer = NULL;
	parser->line_size = 0;
	gf_free(dup_str);
	return e;
}

GF_Err load_bt_suspend(GF_SceneLoader *load, Bool suspend)
{
	return GF_OK;
}

GF_Err gf_sm_load_init_bt(GF_SceneLoader *load)
{
	GF_Err e;
	GF_BTParser *parser;

	if (!load || (!load->ctx && !load->scene_graph) ) return GF_BAD_PARAM;
	if (!load->scene_graph) load->scene_graph = load->ctx->scene_graph;

	GF_SAFEALLOC(parser, GF_BTParser);
	if (!parser) return GF_OUT_OF_MEM;
	parser->load = load;
	load->loader_priv = parser;
	parser->def_symbols = gf_list_new();
	parser->unresolved_routes = gf_list_new();
	parser->inserted_routes = gf_list_new();
	parser->undef_nodes = gf_list_new();
	parser->def_nodes = gf_list_new();
	parser->peeked_nodes = gf_list_new();
	parser->scripts = gf_list_new();

	load->process = load_bt_run;
	load->done = load_bt_done;
	load->suspend = load_bt_suspend;
	load->parse_string = load_bt_parse_string;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		gf_bt_report(parser, GF_OK, NULL);
	}
#endif

	e = gf_sm_load_bt_initialize(load, NULL, 0);
	if (e) {
		load_bt_done(load);
		return e;
	}
	return GF_OK;
}


GF_EXPORT
GF_List *gf_sm_load_bt_from_string(GF_SceneGraph *in_scene, char *node_str, Bool force_wrl)
{
	GF_SceneLoader ctx;
	GF_BTParser parser;
	memset(&ctx, 0, sizeof(GF_SceneLoader));
	ctx.scene_graph = in_scene;
	memset(&parser, 0, sizeof(GF_BTParser));
	parser.line_buffer = node_str;
	parser.line_size = (u32) strlen(node_str);
	parser.load = &ctx;
	parser.top_nodes = gf_list_new();
	parser.undef_nodes = gf_list_new();
	parser.def_nodes = gf_list_new();
	parser.peeked_nodes = gf_list_new();
	parser.is_wrl = force_wrl;
	gf_bt_loader_run_intern(&parser, NULL, 1);
	gf_list_del(parser.undef_nodes);
	gf_list_del(parser.def_nodes);
	gf_list_del(parser.peeked_nodes);
	while (gf_list_count(parser.def_symbols)) {
		BTDefSymbol *d = (BTDefSymbol *)gf_list_get(parser.def_symbols, 0);
		gf_list_rem(parser.def_symbols, 0);
		gf_free(d->name);
		gf_free(d->value);
		gf_free(d);
	}
	gf_list_del(parser.def_symbols);
	gf_list_del(parser.scripts);

	return parser.top_nodes;
}

#endif /*GPAC_DISABLE_LOADER_BT*/
