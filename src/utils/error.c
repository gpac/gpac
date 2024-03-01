/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/tools.h>
#include <gpac/thread.h>


//ugly patch, we have a concurrence issue with gf_4cc_to_str, for now fixed by rolling buffers
#define NB_4CC_BUF	10
static char szTYPE_BUF[NB_4CC_BUF][GF_4CC_MSIZE];
static u32 buf_4cc_idx = NB_4CC_BUF;

GF_EXPORT
const char *gf_4cc_to_str_safe(u32 type, char szType[GF_4CC_MSIZE])
{
	u32 ch, i;
	if (!type) {
		strcpy(szType, "00000000");
		return szType;
	}
	char *name = (char *)szType;
	for (i = 0; i < 4; i++) {
		ch = type >> (8 * (3-i) ) & 0xff;
		if ( ch >= 0x20 && ch <= 0x7E ) {
			*name = ch;
			name++;
		} else {
			sprintf(name, "%02X", ch);
			name += 2;
		}
	}
	*name = 0;
	return szType;
}
#include <gpac/thread.h>

GF_EXPORT
const char *gf_4cc_to_str(u32 type)
{
	if (!type) return "00000000";
	if (safe_int_dec(&buf_4cc_idx)==0)
		buf_4cc_idx=NB_4CC_BUF;

	return gf_4cc_to_str_safe(type, szTYPE_BUF[buf_4cc_idx-1]);
}



GF_EXPORT
u32 gf_4cc_parse(const char *val)
{
	if (val && strlen(val)==4) return GF_4CC(val[0], val[1], val[2], val[3]);
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Value %s is not a properly defined 4CC\n", val));
	return 0;
}

static const char *szProg[] =
{
	"                    ",
	"=                   ",
	"==                  ",
	"===                 ",
	"====                ",
	"=====               ",
	"======              ",
	"=======             ",
	"========            ",
	"=========           ",
	"==========          ",
	"===========         ",
	"============        ",
	"=============       ",
	"==============      ",
	"===============     ",
	"================    ",
	"=================   ",
	"==================  ",
	"=================== ",
	"====================",
};

static u64 prev_pos = (u64) -1;
static u64 prev_pc = (u64) -1;
extern char gf_prog_lf;

static void gf_on_progress_std(const char *_title, u64 done, u64 total)
{
	Double prog;
	u32 pos, pc;
	const char *szT = _title ? (char *)_title : (char *) "";

	if (total) {
		prog = (double) done;
		prog /= total;
	} else {
		prog = 0;
	}

	pos = MIN((u32) (20 * prog), 20);

	if (done != total && pos>prev_pos) {
		prev_pos = 0;
		prev_pc = 0;
	}
	pc = (u32) ( 100 * prog);

	if ((done != total || prev_pos ) && ((pos!=prev_pos) || (pc!=prev_pc))) {
		prev_pos = pos;
		prev_pc = pc;
		fprintf(stderr, "%s: |%s| (%02d/100)%c", szT, szProg[pos], pc, gf_prog_lf);
		fflush(stderr);
	}
	if (done==total) {
		if (prev_pos) {
			u32 len = (u32) strlen(szT) + 40;
			while (len) {
				fprintf(stderr, " ");
				len--;
			};
			fprintf(stderr, "%c", gf_prog_lf);
		}
		prev_pos = 0;
	}
}

static gf_on_progress_cbk prog_cbk = NULL;
static void *user_cbk = NULL;
#if defined(GPAC_CONFIG_IOS) || defined(GPAC_CONFIG_ANDROID)
static Bool gpac_no_color_logs = GF_TRUE;
#else
static Bool gpac_no_color_logs = GF_FALSE;
#endif

GF_EXPORT
void gf_set_progress(const char *title, u64 done, u64 total)
{
	if (done>=total)
		done=total;
	if (prog_cbk || user_cbk) {
		if (prog_cbk)
			prog_cbk(user_cbk, title, done, total);
	}
#ifndef _WIN32_WCE
	else {
		gf_on_progress_std(title, done, total);
	}
#endif
}

GF_EXPORT
void gf_set_progress_callback(void *_user_cbk, gf_on_progress_cbk _prog_cbk)
{
	prog_cbk = _prog_cbk;
	user_cbk = _user_cbk;
}

#ifndef GPAC_DISABLE_LOG

/*ENTRIES MUST BE IN THE SAME ORDER AS LOG_TOOL DECLARATION IN <gpac/tools.h>*/
static struct log_tool_info {
	u32 type;
	const char *name;
	GF_LOG_Level level;
} global_log_tools [] =
{
	{ GF_LOG_CORE, "core", GF_LOG_WARNING },
	{ GF_LOG_CODING, "coding", GF_LOG_WARNING },
	{ GF_LOG_CONTAINER, "container", GF_LOG_WARNING },
	{ GF_LOG_NETWORK, "network", GF_LOG_WARNING },
	{ GF_LOG_HTTP, "http", GF_LOG_WARNING },
	{ GF_LOG_RTP, "rtp", GF_LOG_WARNING },
	{ GF_LOG_CODEC, "codec", GF_LOG_WARNING },
	{ GF_LOG_PARSER, "parser", GF_LOG_WARNING },
	{ GF_LOG_MEDIA, "media", GF_LOG_WARNING },
	{ GF_LOG_SCENE, "scene", GF_LOG_WARNING },
	{ GF_LOG_SCRIPT, "script", GF_LOG_WARNING },
	{ GF_LOG_INTERACT, "interact", GF_LOG_WARNING },
	{ GF_LOG_COMPOSE, "compose", GF_LOG_WARNING },
	{ GF_LOG_COMPTIME, "ctime", GF_LOG_WARNING },
	{ GF_LOG_CACHE, "cache", GF_LOG_WARNING },
	{ GF_LOG_MMIO, "mmio", GF_LOG_WARNING },
	{ GF_LOG_RTI, "rti", GF_LOG_WARNING },
	{ GF_LOG_MEMORY, "mem", GF_LOG_WARNING },
	{ GF_LOG_AUDIO, "audio", GF_LOG_WARNING },
	{ GF_LOG_MODULE, "module", GF_LOG_WARNING },
	{ GF_LOG_MUTEX, "mutex", GF_LOG_WARNING },
	{ GF_LOG_CONDITION, "condition", GF_LOG_WARNING },
	{ GF_LOG_DASH, "dash", GF_LOG_WARNING },
	{ GF_LOG_FILTER, "filter", GF_LOG_WARNING },
	{ GF_LOG_SCHEDULER, "sched", GF_LOG_WARNING },
	{ GF_LOG_ROUTE, "route", GF_LOG_WARNING },
	{ GF_LOG_CONSOLE, "console", GF_LOG_INFO },
	{ GF_LOG_APP, "app", GF_LOG_INFO },
};

#define GF_LOG_TOOL_MAX_NAME_SIZE (GF_LOG_TOOL_MAX*10)

#endif

GF_EXPORT
GF_Err gf_log_modify_tools_levels(const char *val_)
{
#ifndef GPAC_DISABLE_LOG
	char tmp[GF_LOG_TOOL_MAX_NAME_SIZE];
	const char *val = tmp;
	if (!val_) val_ = "";
	if (strlen(val_) >= GF_LOG_TOOL_MAX_NAME_SIZE) return GF_BAD_PARAM;
	strncpy(tmp, val_, GF_LOG_TOOL_MAX_NAME_SIZE - 1);
	tmp[GF_LOG_TOOL_MAX_NAME_SIZE - 1] = 0;

	while (val && strlen(val)) {
		void default_log_callback(void *cbck, GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list vlist);
		u32 level;
		const char *next_val = NULL;
		const char *tools = NULL;
		/*look for log level*/
		char *sep_level = strchr(val, '@');
		if (!sep_level) {
			if (!strcmp(val, "ncl")) {
				gpac_no_color_logs = GF_TRUE;
				gf_log_set_callback(NULL, default_log_callback);
				if (!val[3]) break;
				val += 4;
				continue;
			} else if (!strcmp(val, "cl")) {
				gpac_no_color_logs = GF_FALSE;
				gf_log_set_callback(NULL, default_log_callback);
				if (!val[2]) break;
				val += 3;
				continue;
			} else if (!strcmp(val, "strict")) {
				gf_log_set_strict_error(GF_TRUE);
				if (!val[6]) break;
				val += 7;
				continue;
			} else if (!strcmp(val, "quiet")) {
				u32 i;
				for (i=0; i<GF_LOG_TOOL_MAX; i++)
					global_log_tools[i].level = GF_LOG_QUIET;

				if (!val[5]) break;
				val += 6;
				continue;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unrecognized log format %s - expecting logTool@logLevel\n", val));
				return GF_BAD_PARAM;
			}
		}

		if (!strnicmp(sep_level+1, "error", 5)) {
			level = GF_LOG_ERROR;
			next_val = sep_level+1 + 5;
		}
		else if (!strnicmp(sep_level+1, "warning", 7)) {
			level = GF_LOG_WARNING;
			next_val = sep_level+1 + 7;
		}
		else if (!strnicmp(sep_level+1, "info", 4)) {
			level = GF_LOG_INFO;
			next_val = sep_level+1 + 4;
		}
		else if (!strnicmp(sep_level+1, "debug", 5)) {
			level = GF_LOG_DEBUG;
			next_val = sep_level+1 + 5;
		}
		else if (!strnicmp(sep_level+1, "quiet", 5)) {
			level = GF_LOG_QUIET;
			next_val = sep_level+1 + 5;
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknown log level specified: %s\n", sep_level+1));
			return GF_BAD_PARAM;
		}

		sep_level[0] = 0;
		tools = val;
		while (tools) {
			u32 i;

			char *sep = strchr(tools, ':');
			if (sep) sep[0] = 0;

			if (!stricmp(tools, "all")) {
				for (i=0; i<GF_LOG_TOOL_MAX; i++)
					global_log_tools[i].level = level;
			}
			else if (!strcmp(val, "ncl")) {
				gpac_no_color_logs = GF_TRUE;
				gf_log_set_callback(NULL, default_log_callback);
			}
			else if (!strcmp(val, "cl")) {
				gpac_no_color_logs = GF_FALSE;
				gf_log_set_callback(NULL, default_log_callback);
			}
			else {
				Bool found = GF_FALSE;
				for (i=0; i<GF_LOG_TOOL_MAX; i++) {
					if (!strcmp(global_log_tools[i].name, tools)) {
						global_log_tools[i].level = level;
						found = GF_TRUE;
						break;
					}
				}
				if (!found) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknown log tool specified: %s\n", tools));
					sep_level[0] = '@';
					if (sep) sep[0] = ':';
					return GF_BAD_PARAM;
				}
			}

			if (!sep) break;
			sep[0] = ':';
			tools = sep+1;
		}

		sep_level[0] = '@';
		if (!next_val[0]) break;
		val = next_val+1;
	}
#endif
	return GF_OK;
}

GF_EXPORT
GF_Err gf_log_set_tools_levels(const char *val, Bool reset_all)
{
#ifndef GPAC_DISABLE_LOG
	u32 i;
	if (reset_all) {
		for (i=0; i<GF_LOG_TOOL_MAX; i++)
			global_log_tools[i].level = GF_LOG_WARNING;
	}
	return gf_log_modify_tools_levels(val);
#else
	return GF_OK;
#endif
}

GF_EXPORT
char *gf_log_get_tools_levels()
{
#ifndef GPAC_DISABLE_LOG
	u32 i, level, len;
	char szLogs[GF_MAX_PATH];
	char szLogTools[GF_MAX_PATH];
	strcpy(szLogTools, "");

	level = GF_LOG_QUIET;
	while (level <= GF_LOG_DEBUG) {
		u32 nb_tools = 0;
		strcpy(szLogs, "");
		for (i=0; i<GF_LOG_TOOL_MAX; i++) {
			if (global_log_tools[i].level == level) {
				strcat(szLogs, global_log_tools[i].name);
				strcat(szLogs, ":");
				nb_tools++;
			}
		}
		if (nb_tools) {
			char *levelstr = "@warning";
			if (level==GF_LOG_QUIET) levelstr = "@quiet";
			else if (level==GF_LOG_ERROR) levelstr = "@error";
			else if (level==GF_LOG_WARNING) levelstr = "@warning";
			else if (level==GF_LOG_INFO) levelstr = "@info";
			else if (level==GF_LOG_DEBUG) levelstr = "@debug";

			if (nb_tools>GF_LOG_TOOL_MAX/2) {
				strcpy(szLogs, szLogTools);
				strcpy(szLogTools, "all");
				strcat(szLogTools, levelstr);
				if (strlen(szLogs)) {
					strcat(szLogTools, ":");
					strcat(szLogTools, szLogs);
				}
			} else {
				if (strlen(szLogTools)) {
					strcat(szLogTools, ":");
				}
				/*remove last ':' from tool*/
				szLogs[ strlen(szLogs) - 1 ] = 0;
				strcat(szLogTools, szLogs);
				strcat(szLogTools, levelstr);
			}
		}
		level++;
	}
	len = (u32) strlen(szLogTools);
	if (len) {
		/*remove last ':' from level*/
		if (szLogTools[ len-1 ] == ':') szLogTools[ len-1 ] = 0;
		return gf_strdup(szLogTools);
	}
#endif
	return gf_strdup("all@quiet");
}


#if defined(GPAC_CONFIG_WIN32) || defined(WIN32)
#include <windows.h>
#include <wincon.h>
static HANDLE console = NULL;
static WORD console_attr_ori = 0;
static CONSOLE_SCREEN_BUFFER_INFO cbck_console;
static CONSOLE_CURSOR_INFO cbck_cursor;
static PCHAR_INFO cbck_buffer=NULL;
static Bool cbck_stored=GF_FALSE;
static Bool is_mintty = GF_FALSE;
#endif




GF_EXPORT
void gf_sys_set_console_code(FILE *std, GF_ConsoleCodes code)
{
	u32 color_code;

	if (gf_sys_is_test_mode() || gpac_no_color_logs)
		return;
	color_code = code & 0xFFFF;
#if defined(GPAC_CONFIG_WIN32) || defined(WIN32)
	WORD attribs=0;
	if (!is_mintty && (console == NULL)) {
		CONSOLE_SCREEN_BUFFER_INFO console_info;
		const char *shellv = getenv("SHELL");
		if (shellv) {
			is_mintty = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Console] Detected MSys/MinGW/Cygwin TTY, will use VT for coloring\n"))
		} else {
			console = GetStdHandle(STD_ERROR_HANDLE);
			if (console != INVALID_HANDLE_VALUE) {
				GetConsoleScreenBufferInfo(console, &console_info);
				console_attr_ori = console_info.wAttributes;
				GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Console] Detected Windows shell, will use SetConsoleTextAttribute for coloring\n"))
			}
		}
	}

	if (is_mintty) goto win32_ismtty;

	switch(color_code) {
	case GF_CONSOLE_RED:
		attribs = FOREGROUND_INTENSITY | FOREGROUND_RED;
		break;
	case GF_CONSOLE_BLUE:
		attribs = FOREGROUND_INTENSITY | FOREGROUND_BLUE;
		break;
	case GF_CONSOLE_GREEN:
		attribs = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
		break;
	case GF_CONSOLE_YELLOW:
		attribs = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
		break;
	case GF_CONSOLE_MAGENTA:
		attribs = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE;
		break;
	case GF_CONSOLE_CYAN:
		attribs = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE;
		break;
	case GF_CONSOLE_WHITE:
		attribs = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE;
		break;

	case GF_CONSOLE_SAVE:
		if (!cbck_stored) {
			u32 size, line, line_incr;
			COORD pos;
			BOOL res;
			SMALL_RECT region;

			res = GetConsoleScreenBufferInfo(console, &cbck_console);
			if (res) res = GetConsoleCursorInfo(console, &cbck_cursor);

			if (!res) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to save win32 console info\n"));
				return;
			}
			size = cbck_console.dwSize.X * cbck_console.dwSize.Y;
			cbck_buffer = (PCHAR_INFO) gf_malloc(size * sizeof(CHAR_INFO));
			pos.X = 0;
			region.Left = 0;
			region.Right = cbck_console.dwSize.X - 1;
			line_incr = 12000 / cbck_console.dwSize.X;
			for (line=0; line < (u32) cbck_console.dwSize.Y; line += line_incr) {
				pos.Y = line;
				region.Top = line;
				region.Bottom = line + line_incr - 1;
				if (!ReadConsoleOutput(console, cbck_buffer, cbck_console.dwSize, pos, &region)) {
					gf_free(cbck_buffer);
					cbck_buffer = NULL;
					GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to save win32 console info\n"));
					return;
				}
			}
			cbck_stored = GF_TRUE;
		}
		return;

	case GF_CONSOLE_RESTORE:
		if (cbck_stored) {
			COORD pos;
			SMALL_RECT region;
			BOOL res;
			pos.X = pos.Y = 0;
			region.Top = region.Left = 0;
			region.Bottom = cbck_console.dwSize.Y - 1;
			region.Right = cbck_console.dwSize.X - 1;
			res = SetConsoleScreenBufferSize(console, cbck_console.dwSize);
			if (res) res = SetConsoleWindowInfo(console, TRUE, &cbck_console.srWindow);
			if (res) res = SetConsoleCursorPosition(console, cbck_console.dwCursorPosition);
			if (res) res = SetConsoleTextAttribute(console, cbck_console.wAttributes);

			if (!res) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to restore win32 console info\n"));
				return;
			}
			WriteConsoleOutput(console, cbck_buffer, cbck_console.dwSize,
				pos, &region);
			gf_free(cbck_buffer);
			cbck_stored = GF_FALSE;
		}
		return;

	case GF_CONSOLE_CLEAR:
	{
		COORD coords;
		DWORD written;
		DWORD consolesize;
		CONSOLE_SCREEN_BUFFER_INFO console_info;

		coords.X = coords.Y = 0;
		GetConsoleScreenBufferInfo(console, &console_info);
		consolesize = console_info.dwSize.X * console_info.dwSize.Y;

		/* fill space & attribute */
		FillConsoleOutputCharacter(console, ' ', consolesize, coords, &written);
		FillConsoleOutputAttribute(console, console_info.wAttributes, consolesize, coords, &written);
		SetConsoleCursorPosition(console, coords);
	}
		return;
	case GF_CONSOLE_RESET:
	default:
		attribs = console_attr_ori;
	}

	if ((code & 0xFFFF0000) && !color_code) {
		attribs |= COMMON_LVB_UNDERSCORE;
	}

	SetConsoleTextAttribute(console, attribs);
	return;

win32_ismtty:
#endif


#if !defined(_WIN32_WCE)

	switch(color_code) {
	case GF_CONSOLE_RED: fprintf(std, "\x1b[31m"); break;
	case GF_CONSOLE_BLUE:
		fprintf(std, "\x1b[34m");
		break;
	case GF_CONSOLE_GREEN:
		fprintf(std, "\x1b[32m");
		break;
	case GF_CONSOLE_YELLOW:
		fprintf(std, "\x1b[33m");
		break;
	case GF_CONSOLE_MAGENTA:
		fprintf(std, "\x1b[35m");
		break;
	case GF_CONSOLE_CYAN:
		fprintf(std, "\x1b[36m");
		break;
	case GF_CONSOLE_WHITE:
		fprintf(std, "\x1b[37m");
		break;

	case GF_CONSOLE_CLEAR:
		fprintf(std, "\x1b[2J\x1b[0;0H");
		return;

	case GF_CONSOLE_SAVE:
		fprintf(std, "\033[?47h");
		return;
	case GF_CONSOLE_RESTORE:
		fprintf(std, "\033[?47l");
		fprintf(std, "\x1b[J");
		return;

	case GF_CONSOLE_RESET:
	default:
		if (!code)
			fprintf(std, "\x1b[0m");
	}
	if (code) {
		if (code & GF_CONSOLE_BOLD) fprintf(std, "\x1b[1m");
		if (code & GF_CONSOLE_ITALIC) fprintf(std, "\x1b[3m");
		if (code & GF_CONSOLE_UNDERLINED) fprintf(std, "\x1b[4m");
		if (code & GF_CONSOLE_STRIKE) fprintf(std, "\x1b[9m");
	}
#endif
}

#ifndef GPAC_DISABLE_LOG
u32 call_lev = 0;
u32 call_tool = 0;

GF_EXPORT
Bool gf_log_tool_level_on(GF_LOG_Tool log_tool, GF_LOG_Level log_level)
{
	if (log_tool==GF_LOG_TOOL_MAX) return GF_TRUE;
	if (log_tool>GF_LOG_TOOL_MAX) return GF_FALSE;
	if (global_log_tools[log_tool].level >= log_level) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
const char *gf_log_tool_name(GF_LOG_Tool log_tool)
{
	if (log_tool>=GF_LOG_TOOL_MAX) return "unknown";
	return global_log_tools[log_tool].name;
}
const char *gf_log_level_name(GF_LOG_Level log_level)
{
	switch (log_level) {
	case GF_LOG_DEBUG: return "debug";
	case GF_LOG_INFO: return "info";
	case GF_LOG_WARNING: return "warning";
	case GF_LOG_ERROR: return "error";
	default: return "unknown";
	}
}

GF_EXPORT
u32 gf_log_get_tool_level(GF_LOG_Tool log_tool)
{
	if (log_tool>=GF_LOG_TOOL_MAX) return GF_LOG_ERROR;
	return global_log_tools[log_tool].level;
}

FILE *gpac_log_file = NULL;
Bool gpac_log_time_start = GF_FALSE;
Bool gpac_log_utc_time = GF_FALSE;
Bool gpac_log_dual = GF_FALSE;
Bool last_log_is_lf = GF_TRUE;
static u64 gpac_last_log_time=0;

static void do_log_time(FILE *logs, const char *fmt)
{
	if (!gpac_log_time_start && !gpac_log_utc_time) return;

	if (last_log_is_lf) {
		if (gpac_log_time_start) {
			u64 now = gf_sys_clock_high_res();
			gf_fprintf(logs, "At "LLD" (diff %d) - ", now, (u32) (now - gpac_last_log_time) );
			gpac_last_log_time = now;
		}
		if (gpac_log_utc_time) {
			u64 utc_clock = gf_net_get_utc() ;
			time_t secs = utc_clock/1000;
			struct tm t;
			t = *gf_gmtime(&secs);
			gf_fprintf(logs, "UTC %d-%02d-%02dT%02d:%02d:%02dZ (TS "LLU") - ", 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, utc_clock);
		}
	}
	u32 flen = (u32) strlen(fmt);
	if (flen && fmt[flen-1] == '\n') last_log_is_lf = GF_TRUE;
	else last_log_is_lf = GF_FALSE;
}

int gf_fileio_printf(GF_FileIO *gfio, const char *format, va_list args);

void default_log_callback(void *cbck, GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list vlist)
{
	FILE *logs = gpac_log_file ? gpac_log_file : stderr;
	if (tool != GF_LOG_APP)
		do_log_time(logs, fmt);

	if (gf_fileio_check(logs)) {
		gf_fileio_printf((GF_FileIO *)logs, fmt, vlist);
	} else {
		if (gpac_log_dual && gpac_no_color_logs) {
			va_list vlist_c;
			va_copy(vlist_c, vlist);
			vfprintf(stderr, fmt, vlist_c);
			va_end(vlist_c);
		}
		vfprintf(logs, fmt, vlist);
	}
	gf_fflush(logs);
}

void default_log_callback_color(void *cbck, GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list vlist)
{
	if (gpac_log_file) {
		if (!gpac_log_dual) {
			default_log_callback(cbck, level, tool, fmt, vlist);
			return;
		}
		va_list vlist_c;
		va_copy(vlist_c, vlist);
		default_log_callback(cbck, level, tool, fmt, vlist_c);
		va_end(vlist_c);
	}
	switch(level) {
	case GF_LOG_ERROR:
		gf_sys_set_console_code(stderr, GF_CONSOLE_RED);
		break;
	case GF_LOG_WARNING:
		gf_sys_set_console_code(stderr, GF_CONSOLE_YELLOW);
		break;
	case GF_LOG_INFO:
		gf_sys_set_console_code(stderr, (tool==GF_LOG_APP) ? GF_CONSOLE_WHITE : GF_CONSOLE_GREEN);
		break;
	case GF_LOG_DEBUG:
		gf_sys_set_console_code(stderr, GF_CONSOLE_CYAN);
		break;
	default:
		gf_sys_set_console_code(stderr, GF_CONSOLE_WHITE);
		break;
	}
	if (tool != GF_LOG_APP)
		do_log_time(stderr, fmt);

	vfprintf(stderr, fmt, vlist);
	gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
	gf_fflush(stderr);
}



static void *user_log_cbk = NULL;
gf_log_cbk log_cbk = default_log_callback_color;
static Bool log_exit_on_error = GF_FALSE;
extern GF_Mutex *logs_mx;

GF_EXPORT
Bool gf_log_use_color()
{
	return (log_cbk == default_log_callback_color) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
void gf_log(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	gf_mx_p(logs_mx);
	log_cbk(user_log_cbk, call_lev, call_tool, fmt, vl);
	gf_mx_v(logs_mx);
	va_end(vl);
	if (log_exit_on_error && (call_lev==GF_LOG_ERROR) && (call_tool != GF_LOG_MEMORY)) {
		exit(1);
	}
}

GF_EXPORT
void gf_log_va_list(GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list vl)
{
	log_cbk(user_log_cbk, call_lev, call_tool, fmt, vl);
	if (log_exit_on_error && (call_lev==GF_LOG_ERROR) && (call_tool != GF_LOG_MEMORY)) {
		exit(1);
	}
}

GF_EXPORT
Bool gf_log_set_strict_error(Bool strict)
{
	Bool old = log_exit_on_error;
	log_exit_on_error = strict;
	return old;
}

GF_EXPORT
void gf_log_set_tool_level(GF_LOG_Tool tool, GF_LOG_Level level)
{
	if (tool>GF_LOG_TOOL_MAX) return;
	if (tool==GF_LOG_ALL) {
		u32 i;
		for (i=0; i<GF_LOG_TOOL_MAX; i++) {
			global_log_tools[i].level = level;
		}
	} else {
		global_log_tools[tool].level = level;
	}
}

GF_EXPORT
void gf_log_lt(GF_LOG_Level ll, GF_LOG_Tool lt)
{
	call_lev = ll;
	call_tool = lt;
}

GF_EXPORT
gf_log_cbk gf_log_set_callback(void *usr_cbk, gf_log_cbk cbk)
{
	gf_log_cbk prev_cbk = log_cbk;
	log_cbk = cbk;
	if (!log_cbk) log_cbk = default_log_callback;
	if (usr_cbk) user_log_cbk = usr_cbk;
	return prev_cbk;
}

#else
GF_EXPORT
void gf_log(const char *fmt, ...)
{
}
GF_EXPORT
void gf_log_lt(GF_LOG_Level ll, GF_LOG_Tool lt)
{
}

Bool log_exit_on_error=GF_FALSE;
GF_EXPORT
Bool gf_log_set_strict_error(Bool strict)
{
	Bool old = log_exit_on_error;
	log_exit_on_error = strict;
	return old;
}

GF_EXPORT
void gf_log_check_error(GF_LOG_Level loglev, GF_LOG_Tool logtool)
{
	if (log_exit_on_error && (loglev==GF_LOG_ERROR) && (logtool != GF_LOG_MEMORY)) {
		exit(1);
	}
}

GF_EXPORT
gf_log_cbk gf_log_set_callback(void *usr_cbk, gf_log_cbk cbk)
{
	return NULL;
}

GF_EXPORT
void gf_log_set_tool_level(GF_LOG_Tool tool, GF_LOG_Level level)
{
}

GF_EXPORT
u32 gf_log_get_tool_level(GF_LOG_Tool log_tool)
{
	return 0;
}

GF_EXPORT
Bool gf_log_tool_level_on(GF_LOG_Tool log_tool, GF_LOG_Level log_level)
{
	return GF_FALSE;
}

#endif

static char szErrMsg[50];

GF_EXPORT
const char *gf_error_to_string(GF_Err e)
{
	switch (e) {
	case GF_EOS:
		return "End Of Stream / File";
	case GF_OK:
		return "No Error";

	/*General errors */
	case GF_BAD_PARAM:
		return "Bad Parameter";
	case GF_OUT_OF_MEM:
		return "Out Of Memory";
	case GF_IO_ERR:
		return "I/O Error";
	case GF_NOT_SUPPORTED:
		return "Feature Not Supported";
	case GF_CORRUPTED_DATA:
		return "Corrupted Data in file/stream";

	/*File Format Errors */
	case GF_ISOM_INVALID_FILE:
		return "Invalid IsoMedia File";
	case GF_ISOM_INCOMPLETE_FILE:
		return "IsoMedia File is truncated";
	case GF_ISOM_INVALID_MEDIA:
		return "Invalid IsoMedia Media";
	case GF_ISOM_INVALID_MODE:
		return "Invalid Mode while accessing the file";
	case GF_ISOM_UNKNOWN_DATA_REF:
		return "Media Data Reference not found";

	/*Object Descriptor Errors */
	case GF_ODF_INVALID_DESCRIPTOR:
		return "Invalid MPEG-4 Descriptor";
	case GF_ODF_FORBIDDEN_DESCRIPTOR:
		return "MPEG-4 Descriptor Not Allowed";
	case GF_ODF_INVALID_COMMAND:
		return "Read OD Command Failed";

	/*BIFS Errors */
	case GF_SG_UNKNOWN_NODE:
		return "Unknown BIFS Node";
	case GF_SG_INVALID_PROTO:
		return "Invalid Proto Interface";
	case GF_BIFS_UNKNOWN_VERSION:
		return "Invalid BIFS version";
	case GF_SCRIPT_ERROR:
		return "Invalid Script";

	case GF_BUFFER_TOO_SMALL:
		return "Bad Buffer size (too small)";
	case GF_NON_COMPLIANT_BITSTREAM:
		return "BitStream Not Compliant";
	case GF_FILTER_NOT_FOUND:
		return "Filter not found for the desired type";

	case GF_URL_ERROR:
		return "Requested URL is not valid or cannot be found";
	case GF_URL_REMOVED:
		return "Requested URL is no longer available";

	case GF_SERVICE_ERROR:
		return "Internal Service Error";
	case GF_REMOTE_SERVICE_ERROR:
		return "Dialog Failure with remote peer";

	case GF_STREAM_NOT_FOUND:
		return "Media Channel couldn't be found";

	case GF_IP_ADDRESS_NOT_FOUND:
		return "IP Address Not Found";
	case GF_IP_CONNECTION_FAILURE:
		return "IP Connection Failed";
	case GF_IP_NETWORK_FAILURE:
		return "Network Unreachable";

	case GF_IP_NETWORK_EMPTY:
		return "Network Empty";
	case GF_IP_CONNECTION_CLOSED:
		return "Connection to server closed";
	case GF_IP_UDP_TIMEOUT:
		return "UDP traffic timeout";
	case GF_AUTHENTICATION_FAILURE:
		return "Authentication failure";
	case GF_NOT_READY:
		return "Not ready, retry later";
	case GF_INVALID_CONFIGURATION:
		return "Bad configuration for the current context";
	case GF_NOT_FOUND:
		return "At least one required element has not been found";
	case GF_PROFILE_NOT_SUPPORTED:
		return "Unsupported codec profile";
	case GF_REQUIRES_NEW_INSTANCE:
		return "Requires a new instance of the filter to be supported";
	case GF_FILTER_NOT_SUPPORTED:
		return "Not supported by any filter chain";
	default:
		sprintf(szErrMsg, "Unknown Error (%d)", e);
		return szErrMsg;
	}
}

static const u32 gf_crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

GF_EXPORT
u32 gf_crc_32(const u8 *data, u32 len)
{
	register u32 i;
	u32 crc = 0xffffffff;
	if (!data) return 0;
	for (i=0; i<len; i++)
		crc = (crc << 8) ^ gf_crc_table[((crc >> 24) ^ *data++) & 0xff];

	return crc;
}


#define CHECK_MAC(_a) "#_a :" ? (_a) ? "yes":"no"

static const char *gf_enabled_features()
{
	const char *features = ""
#ifdef GPAC_CONFIG_WIN32
	                       "GPAC_CONFIG_WIN32 "
#endif
#ifdef GPAC_CONFIG_DARWIN
	                       "GPAC_CONFIG_DARWIN "
#endif
#ifdef GPAC_CONFIG_LINUX
	                       "GPAC_CONFIG_LINUX "
#endif
#ifdef GPAC_CONFIG_ANDROID
	                       "GPAC_CONFIG_ANDROID "
#endif
#ifdef GPAC_CONFIG_IOS
	                       "GPAC_CONFIG_IOS "
#endif
#ifdef GPAC_ENABLE_DEBUG
	                       "GPAC_ENABLE_DEBUG "
#endif
#ifdef GPAC_ASSERT_FATAL
	                       "GPAC_ASSERT_FATAL "
#endif
#ifdef GPAC_CONFIG_EMSCRIPTEN
	                       "GPAC_CONFIG_EMSCRIPTEN "
#endif
#ifdef GPAC_64_BITS
	                       "GPAC_64_BITS "
#endif
#ifdef GPAC_FIXED_POINT
	                       "GPAC_FIXED_POINT "
#endif
#ifdef GPAC_MEMORY_TRACKING
	                       "GPAC_MEMORY_TRACKING "
#endif
#ifdef GPAC_BIG_ENDIAN
	                       "GPAC_BIG_ENDIAN "
#endif
#ifdef GPAC_HAS_IPV6
	                       "GPAC_HAS_IPV6 "
#endif
#ifdef GPAC_HAS_SSL
	                       "GPAC_HAS_SSL "
#endif
#ifdef GPAC_HAS_SOCK_UN
	                       "GPAC_HAS_SOCK_UN "
#endif
#ifdef GPAC_ENABLE_COVERAGE
	                       "GPAC_ENABLE_COVERAGE "
#endif
#ifdef GPAC_MINIMAL_ODF
	                       "GPAC_MINIMAL_ODF "
#endif
#ifdef GPAC_HAS_QJS
	                       "GPAC_HAS_QJS "
#endif
#ifdef GPAC_HAS_FREETYPE
	                       "GPAC_HAS_FREETYPE "
#endif
#ifdef GPAC_HAS_FAAD
	                       "GPAC_HAS_FAAD "
#endif
#ifdef GPAC_HAS_MAD
	                       "GPAC_HAS_MAD "
#endif
#ifdef GPAC_HAS_LIBA52
	                       "GPAC_HAS_LIBA52 "
#endif
#ifdef GPAC_HAS_JPEG
	                       "GPAC_HAS_JPEG "
#endif
#ifdef GPAC_HAS_PNG
	                       "GPAC_HAS_PNG "
#endif
#ifdef GPAC_HAS_FFMPEG
	                       "GPAC_HAS_FFMPEG "
#endif
#ifdef GPAC_HAS_OPENSVC
	                       "GPAC_HAS_OPENSVC "
#endif
#ifdef GPAC_HAS_JP2
	                       "GPAC_HAS_JP2 "
#endif
#ifdef GPAC_HAS_OPENHEVC
	                       "GPAC_HAS_OPENHEVC "
#endif
#ifdef GPAC_HAS_THEORA
	                       "GPAC_HAS_THEORA "
#endif
#ifdef GPAC_HAS_VORBIS
	                       "GPAC_HAS_VORBIS "
#endif
#ifdef GPAC_HAS_XVID
	                       "GPAC_HAS_XVID "
#endif
#ifdef GPAC_HAS_LINUX_DVB
	                       "GPAC_HAS_LINUX_DVB "
#endif
#ifdef GPAC_HAS_GLU
	                       "GPAC_HAS_GLU "
#endif
#ifdef GPAC_USE_TINYGL
	                       "GPAC_USE_TINYGL "
#endif
#ifdef GPAC_USE_GLES1X
	                       "GPAC_USE_GLES1X "
#endif
#ifdef GPAC_USE_GLES2
	                       "GPAC_USE_GLES2 "
#endif
#ifdef GPAC_HAS_HTTP2
	                       "GPAC_HAS_HTTP2 "
#endif

#if defined(_WIN32_WCE)
#ifdef GPAC_USE_IGPP
	                       "GPAC_USE_IGPP "
#endif
#ifdef GPAC_USE_IGPP_HP
	                       "GPAC_USE_IGPP_HP "
#endif
#endif
	                       ;
	return features;
}

static const char *gf_disabled_features()
{
	const char *features = ""
#ifdef GPAC_DISABLE_3D
	                       "GPAC_DISABLE_3D "
#endif
#ifdef GPAC_DISABLE_DOC
	                       "GPAC_DISABLE_DOC "
#endif
#ifdef GPAC_DISABLE_ZLIB
	                       "GPAC_DISABLE_ZLIB "
#endif
#ifdef GPAC_DISABLE_THREADS
	                       "GPAC_DISABLE_THREADS "
#endif
#ifdef GPAC_DISABLE_NETWORK
	                       "GPAC_DISABLE_NETWORK "
#endif
#ifdef GPAC_DISABLE_STREAMING
	                       "GPAC_DISABLE_STREAMING "
#endif
#ifdef GPAC_DISABLE_ROUTE
	                       "GPAC_DISABLE_ROUTE "
#endif
#ifdef GPAC_DISABLE_EVG
	                       "GPAC_DISABLE_EVG "
#endif
#ifdef GPAC_DISABLE_AVILIB
	                       "GPAC_DISABLE_AVILIB "
#endif
#ifdef GPAC_DISABLE_OGG
	                       "GPAC_DISABLE_OGG "
#endif
#ifdef GPAC_DISABLE_AV_PARSERS
	                       "GPAC_DISABLE_AV_PARSERS "
#endif
#ifdef GPAC_DISABLE_MPEG2PS
	                       "GPAC_DISABLE_MPEG2PS "
#endif
#ifdef GPAC_DISABLE_MPEG2PS
	                       "GPAC_DISABLE_MPEG2TS "
#endif
#ifdef GPAC_DISABLE_MEDIA_IMPORT
	                       "GPAC_DISABLE_MEDIA_IMPORT "
#endif
#ifdef GPAC_DISABLE_MEDIA_EXPORT
	                       "GPAC_DISABLE_MEDIA_EXPORT "
#endif
#ifdef GPAC_DISABLE_VRML
	                       "GPAC_DISABLE_VRML "
#endif
#ifdef GPAC_DISABLE_SVG
	                       "GPAC_DISABLE_SVG "
#endif
#ifdef GPAC_DISABLE_QTVR
	                       "GPAC_DISABLE_QTVR "
#endif
#ifdef GPAC_DISABLE_SWF_IMPORT
	                       "GPAC_DISABLE_SWF_IMPORT "
#endif
#ifdef GPAC_DISABLE_SCENE_STATS
	                       "GPAC_DISABLE_SCENE_STATS "
#endif
#ifdef GPAC_DISABLE_SCENE_DUMP
	                       "GPAC_DISABLE_SCENE_DUMP "
#endif
#ifdef GPAC_DISABLE_BIFS
	                       "GPAC_DISABLE_BIFS "
#endif
#ifdef GPAC_DISABLE_LASER
	                       "GPAC_DISABLE_LASER "
#endif
#ifdef GPAC_DISABLE_SENG
	                       "GPAC_DISABLE_SENG "
#endif
#ifdef GPAC_DISABLE_SCENE_ENCODER
	                       "GPAC_DISABLE_SCENE_ENCODER "
#endif
#ifdef GPAC_DISABLE_LOADER_ISOM
	                       "GPAC_DISABLE_LOADER_ISOM "
#endif
#ifdef GPAC_DISABLE_OD_DUMP
	                       "GPAC_DISABLE_OD_DUMP "
#endif
#ifdef GPAC_DISABLE_CRYPTO
	                       "GPAC_DISABLE_CRYPTO "
#endif
#ifdef GPAC_DISABLE_ISOM
	                       "GPAC_DISABLE_CRYPTO "
#endif
#ifdef GPAC_DISABLE_ISOM_HINTING
	                       "GPAC_DISABLE_ISOM_HINTING "
#endif
#ifdef GPAC_DISABLE_ISOM_WRITE
	                       "GPAC_DISABLE_ISOM_WRITE "
#endif
#ifdef GPAC_DISABLE_ISOM_FRAGMENTS
	                       "GPAC_DISABLE_ISOM_FRAGMENTS "
#endif
#ifdef GPAC_DISABLE_ISOM_DUMP
	                       "GPAC_DISABLE_ISOM_DUMP "
#endif
#ifdef GPAC_DISABLE_NETCAP
	                       "GPAC_DISABLE_NETCAP "
#endif

#ifdef GPAC_DISABLE_COMPOSITOR
	                       "GPAC_DISABLE_COMPOSITOR "
#endif
#ifdef GPAC_DISABLE_FONTS
	                       "GPAC_DISABLE_FONTS "
#endif
#ifdef GPAC_DISABLE_VOUT
	                       "GPAC_DISABLE_VOUT "
#endif
#ifdef GPAC_DISABLE_AOUT
	                       "GPAC_DISABLE_AOUT "
#endif
#ifdef GPAC_DISABLE_BSAGG
	                       "GPAC_DISABLE_BSAGG "
#endif
#ifdef GPAC_DISABLE_BSSPLIT
	                       "GPAC_DISABLE_BSSPLIT "
#endif
#ifdef GPAC_DISABLE_BSRW
	                       "GPAC_DISABLE_BSRW "
#endif
#ifdef GPAC_DISABLE_DASHER
	                       "GPAC_DISABLE_DASHER "
#endif
#ifdef GPAC_DISABLE_DASHIN
	                       "GPAC_DISABLE_DASHIN "
#endif
#ifdef GPAC_DISABLE_IMGDEC
	                       "GPAC_DISABLE_IMGDEC "
#endif
#ifdef GPAC_DISABLE_UNCVDEC
	                       "GPAC_DISABLE_UNCVDEC "
#endif
#ifdef GPAC_DISABLE_CDCRYPT
	                       "GPAC_DISABLE_CDCRYPT "
#endif
#ifdef GPAC_DISABLE_GHIDMX
	                       "GPAC_DISABLE_GHIDMX "
#endif
#ifdef GPAC_DISABLE_GSFDMX
	                       "GPAC_DISABLE_GSFDMX "
#endif
#ifdef GPAC_DISABLE_NHMLR
	                       "GPAC_DISABLE_NHMLR "
#endif
#ifdef GPAC_DISABLE_NHNTR
	                       "GPAC_DISABLE_NHNTR "
#endif
#ifdef GPAC_DISABLE_CECRYPT
	                       "GPAC_DISABLE_CECRYPT "
#endif
#ifdef GPAC_DISABLE_FLIST
	                       "GPAC_DISABLE_FLIST "
#endif
#ifdef GPAC_DISABLE_HEVCMERGE
	                       "GPAC_DISABLE_HEVCMERGE "
#endif
#ifdef GPAC_DISABLE_HEVCSPLIT
	                       "GPAC_DISABLE_HEVCSPLIT "
#endif
#ifdef GPAC_DISABLE_FIN
	                       "GPAC_DISABLE_FIN "
#endif
#ifdef GPAC_DISABLE_PIN
	                       "GPAC_DISABLE_PIN "
#endif
#ifdef GPAC_DISABLE_INSPECT
	                       "GPAC_DISABLE_INSPECT "
#endif
#ifdef GPAC_DISABLE_CRYPTFILE
	                       "GPAC_DISABLE_CRYPTFILE "
#endif
#ifdef GPAC_DISABLE_MP4DMX
	                       "GPAC_DISABLE_MP4DMX "
#endif
#ifdef GPAC_DISABLE_TXTIN
	                       "GPAC_DISABLE_TXTIN "
#endif
#ifdef GPAC_DISABLE_GSFMX
	                       "GPAC_DISABLE_GSFMX "
#endif
#ifdef GPAC_DISABLE_MP4MX
	                       "GPAC_DISABLE_MP4MX "
#endif
#ifdef GPAC_DISABLE_FOUT
	                       "GPAC_DISABLE_FOUT "
#endif
#ifdef GPAC_DISABLE_POUT
	                       "GPAC_DISABLE_POUT "
#endif
#ifdef GPAC_DISABLE_RFAC3
	                       "GPAC_DISABLE_RFAC3 "
#endif
#ifdef GPAC_DISABLE_RFADTS
	                       "GPAC_DISABLE_RFADTS "
#endif
#ifdef GPAC_DISABLE_RFAMR
	                       "GPAC_DISABLE_RFAMR "
#endif
#ifdef GPAC_DISABLE_RFAV1
	                       "GPAC_DISABLE_RFAV1 "
#endif
#ifdef GPAC_DISABLE_RFFLAC
	                       "GPAC_DISABLE_RFFLAC "
#endif
#ifdef GPAC_DISABLE_RFH263
	                       "GPAC_DISABLE_RFH263 "
#endif
#ifdef GPAC_DISABLE_RFIMG
	                       "GPAC_DISABLE_RFIMG "
#endif
#ifdef GPAC_DISABLE_RFLATM
	                       "GPAC_DISABLE_RFLATM "
#endif
#ifdef GPAC_DISABLE_RFMHAS
	                       "GPAC_DISABLE_RFMHAS "
#endif
#ifdef GPAC_DISABLE_RFMP3
	                       "GPAC_DISABLE_RFMP3 "
#endif
#ifdef GPAC_DISABLE_RFMPGVID
	                       "GPAC_DISABLE_RFMPGVID "
#endif
#ifdef GPAC_DISABLE_RFNALU
	                       "GPAC_DISABLE_RFNALU "
#endif
#ifdef GPAC_DISABLE_RFPRORES
	                       "GPAC_DISABLE_RFPRORES "
#endif
#ifdef GPAC_DISABLE_RFQCP
	                       "GPAC_DISABLE_RFQCP "
#endif
#ifdef GPAC_DISABLE_RFPCM
	                       "GPAC_DISABLE_RFPCM "
#endif
#ifdef GPAC_DISABLE_RFRAWVID
	                       "GPAC_DISABLE_RFRAWVID "
#endif
#ifdef GPAC_DISABLE_RFTRUEHD
	                       "GPAC_DISABLE_RFTRUEHD "
#endif
#ifdef GPAC_DISABLE_REFRAMER
	                       "GPAC_DISABLE_REFRAMER "
#endif
#ifdef GPAC_DISABLE_RESAMPLE
	                       "GPAC_DISABLE_RESAMPLE "
#endif
#ifdef GPAC_DISABLE_RESTAMP
	                       "GPAC_DISABLE_RESTAMP "
#endif
#ifdef GPAC_DISABLE_REWIND
	                       "GPAC_DISABLE_REWIND "
#endif
#ifdef GPAC_DISABLE_UFAAC
	                       "GPAC_DISABLE_UFAAC "
#endif
#ifdef GPAC_DISABLE_UFMHAS
	                       "GPAC_DISABLE_UFMHAS "
#endif
#ifdef GPAC_DISABLE_UFM4V
	                       "GPAC_DISABLE_UFM4V "
#endif
#ifdef GPAC_DISABLE_UFNALU
	                       "GPAC_DISABLE_UFNALU "
#endif
#ifdef GPAC_DISABLE_UFOBU
	                       "GPAC_DISABLE_UFOBU "
#endif
#ifdef GPAC_DISABLE_TILEAGG
	                       "GPAC_DISABLE_TILEAGG "
#endif
#ifdef GPAC_DISABLE_TILESPLIT
	                       "GPAC_DISABLE_TILESPLIT "
#endif
#ifdef GPAC_DISABLE_TTMLCONV
	                       "GPAC_DISABLE_TTMLCONV "
#endif
#ifdef GPAC_DISABLE_UNFRAMER
	                       "GPAC_DISABLE_UNFRAMER "
#endif
#ifdef GPAC_DISABLE_VCROP
	                       "GPAC_DISABLE_VCROP "
#endif
#ifdef GPAC_DISABLE_VFLIP
	                       "GPAC_DISABLE_VFLIP "
#endif
#ifdef GPAC_DISABLE_WRITEGEN
	                       "GPAC_DISABLE_WRITEGEN "
#endif
#ifdef GPAC_DISABLE_NHMLW
	                       "GPAC_DISABLE_NHMLW "
#endif
#ifdef GPAC_DISABLE_NHNTW
	                       "GPAC_DISABLE_NHNTW "
#endif
#ifdef GPAC_DISABLE_WRITEQCP
	                       "GPAC_DISABLE_WRITEQCP "
#endif
#ifdef GPAC_DISABLE_UNITS
	                       "GPAC_DISABLE_UNITS "
#endif
#ifdef GPAC_DISABLE_NVDEC
	                       "GPAC_DISABLE_NVDEC "
#endif
#ifdef GPAC_DISABLE_VTBDEC
	                       "GPAC_DISABLE_VTBDEC "
#endif
	                       ;
	return features;
}

GF_EXPORT
const char *gf_sys_features(Bool disabled)
{
	if (disabled)
		return gf_disabled_features();
	else
		return gf_enabled_features();
}

static const struct lang_def {
	const char *name;
	const char *three_char_code;
	const char *two_char_code;
} defined_languages [] =
{
	{ "Abkhazian","abk","ab"} ,
	{ "Achinese","ace",""} ,
	{ "Acoli","ach",""} ,
	{ "Adangme","ada",""} ,
	{ "Adyghe; Adygei","ady",""} ,
	{ "Afar","aar","aa"} ,
	{ "Afrihili","afh",""} ,
	{ "Afrikaans","afr","af"} ,
	{ "Afro-Asiatic languages","afa",""} ,
	{ "Ainu","ain",""} ,
	{ "Akan","aka","ak"} ,
	{ "Akkadian","akk",""} ,
	{ "Albanian","sqi","sq"} ,
	{ "Aleut","ale",""} ,
	{ "Algonquian languages","alg",""} ,
	{ "Altaic languages","tut",""} ,
	{ "Amharic","amh","am"} ,
	{ "Angika","anp",""} ,
	{ "Apache languages","apa",""} ,
	{ "Arabic","ara","ar"} ,
	{ "Aragonese","arg","an"} ,
	{ "Arapaho","arp",""} ,
	{ "Arawak","arw",""} ,
	{ "Armenian","hye","hy"} ,
	{ "Aromanian; Arumanian; Macedo-Romanian","rup",""} ,
	{ "Artificial languages","art",""} ,
	{ "Assamese","asm","as"} ,
	{ "Asturian; Bable; Leonese; Asturleonese","ast",""} ,
	{ "Athapascan languages","ath",""} ,
	{ "Australian languages","aus",""} ,
	{ "Austronesian languages","map",""} ,
	{ "Avaric","ava","av"} ,
	{ "Avestan","ave","ae"} ,
	{ "Awadhi","awa",""} ,
	{ "Aymara","aym","ay"} ,
	{ "Azerbaijani","aze","az"} ,
	{ "Balinese","ban",""} ,
	{ "Baltic languages","bat",""} ,
	{ "Baluchi","bal",""} ,
	{ "Bambara","bam","bm"} ,
	{ "Bamileke languages","bai",""} ,
	{ "Banda languages","bad",""} ,
	{ "Bantu languages","bnt",""} ,
	{ "Basa","bas",""} ,
	{ "Bashkir","bak","ba"} ,
	{ "Basque","eus","eu"} ,
	{ "Batak languages","btk",""} ,
	{ "Beja; Bedawiyet","bej",""} ,
	{ "Belarusian","bel","be"} ,
	{ "Bemba","bem",""} ,
	{ "Bengali","ben","bn"} ,
	{ "Berber languages","ber",""} ,
	{ "Bhojpuri","bho",""} ,
	{ "Bihari languages","bih","bh"} ,
	{ "Bikol","bik",""} ,
	{ "Bini; Edo","bin",""} ,
	{ "Bislama","bis","bi"} ,
	{ "Blin; Bilin","byn",""} ,
	{ "Blissymbols; Blissymbolics; Bliss","zbl",""} ,
	{ "Bokmal, Norwegian; Norwegian Bokmal","nob","nb"} ,
	{ "Bosnian","bos","bs"} ,
	{ "Braj","bra",""} ,
	{ "Breton","bre","br"} ,
	{ "Buginese","bug",""} ,
	{ "Bulgarian","bul","bg"} ,
	{ "Buriat","bua",""} ,
	{ "Burmese","mya","my"} ,
	{ "Caddo","cad",""} ,
	{ "Catalan; Valencian","cat","ca"} ,
	{ "Caucasian languages","cau",""} ,
	{ "Cebuano","ceb",""} ,
	{ "Celtic languages","cel",""} ,
	{ "Central American Indian languages","cai",""} ,
	{ "Central Khmer","khm","km"} ,
	{ "Chagatai","chg",""} ,
	{ "Chamic languages","cmc",""} ,
	{ "Chamorro","cha","ch"} ,
	{ "Chechen","che","ce"} ,
	{ "Cherokee","chr",""} ,
	{ "Cheyenne","chy",""} ,
	{ "Chibcha","chb",""} ,
	{ "Chichewa; Chewa; Nyanja","nya","ny"} ,
	{ "Chinese","zho","zh"} ,
	{ "Chinook jargon","chn",""} ,
	{ "Chipewyan; Dene Suline","chp",""} ,
	{ "Choctaw","cho",""} ,
	{ "Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church}  S{ lavonic","chu","cu"} ,
	{ "Chuukese","chk",""} ,
	{ "Chuvash","chv","cv"} ,
	{ "Classical Newari; Old Newari; Classical Nepal Bhasa","nwc",""} ,
	{ "Classical Syriac","syc",""} ,
	{ "Coptic","cop",""} ,
	{ "Cornish","cor","kw"} ,
	{ "Corsican","cos","co"} ,
	{ "Cree","cre","cr"} ,
	{ "Creek","mus",""} ,
	{ "Creoles and pidgins","crp",""} ,
	{ "Creoles and pidgins, English based","cpe",""} ,
	{ "Creoles and pidgins, French-based","cpf",""} ,
	{ "Creoles and pidgins, Portuguese-based","cpp",""} ,
	{ "Crimean Tatar; Crimean Turkish","crh",""} ,
	{ "Croatian","hrv","hr"} ,
	{ "Cushitic languages","cus",""} ,
	{ "Czech","ces","cs"} ,
	{ "Dakota","dak",""} ,
	{ "Danish","dan","da"} ,
	{ "Dargwa","dar",""} ,
	{ "Delaware","del",""} ,
	{ "Dinka","din",""} ,
	{ "Divehi; Dhivehi; Maldivian","div","dv"} ,
	{ "Dogri","doi",""} ,
	{ "Dogrib","dgr",""} ,
	{ "Dravidian languages","dra",""} ,
	{ "Duala","dua",""} ,
	{ "Dutch, Middle (ca.1050-1350)","dum",""} ,
	{ "Dutch; Flemish","nld","nl"} ,
	{ "Dyula","dyu",""} ,
	{ "Dzongkha","dzo","dz"} ,
	{ "Eastern Frisian","frs",""} ,
	{ "Efik","efi",""} ,
	{ "Egyptian (Ancient)","egy",""} ,
	{ "Ekajuk","eka",""} ,
	{ "Elamite","elx",""} ,
	{ "English","eng","en"} ,
	{ "English, Middle (1100-1500)","enm",""} ,
	{ "English, Old (ca.450-1100)","ang",""} ,
	{ "Erzya","myv",""} ,
	{ "Esperanto","epo","eo"} ,
	{ "Estonian","est","et"} ,
	{ "Ewe","ewe","ee"} ,
	{ "Ewondo","ewo",""} ,
	{ "Fang","fan",""} ,
	{ "Fanti","fat",""} ,
	{ "Faroese","fao","fo"} ,
	{ "Fijian","fij","fj"} ,
	{ "Filipino; Pilipino","fil",""} ,
	{ "Finnish","fin","fi"} ,
	{ "Finno-Ugrian languages","fiu",""} ,
	{ "Fon","fon",""} ,
	{ "French","fra","fr"} ,
	{ "French, Middle (ca.1400-1600)","frm",""} ,
	{ "French, Old (842-ca.1400)","fro",""} ,
	{ "Friulian","fur",""} ,
	{ "Fulah","ful","ff"} ,
	{ "Ga","gaa",""} ,
	{ "Gaelic; Scottish Gaelic","gla","gd"} ,
	{ "Galibi Carib","car",""} ,
	{ "Galician","glg","gl"} ,
	{ "Ganda","lug","lg"} ,
	{ "Gayo","gay",""} ,
	{ "Gbaya","gba",""} ,
	{ "Geez","gez",""} ,
	{ "Georgian","kat","ka"} ,
	{ "German","deu","de"} ,
	{ "German, Middle High (ca.1050-1500)","gmh",""} ,
	{ "German, Old High (ca.750-1050)","goh",""} ,
	{ "Germanic languages","gem",""} ,
	{ "Gilbertese","gil",""} ,
	{ "Gondi","gon",""} ,
	{ "Gorontalo","gor",""} ,
	{ "Gothic","got",""} ,
	{ "Grebo","grb",""} ,
	{ "Greek, Ancient (to 1453)","grc",""} ,
	{ "Greek, Modern (1453-)","ell","el"} ,
	{ "Guarani","grn","gn"} ,
	{ "Gujarati","guj","gu"} ,
	{ "Gwich'in","gwi",""} ,
	{ "Haida","hai",""} ,
	{ "Haitian; Haitian Creole","hat","ht"} ,
	{ "Hausa","hau","ha"} ,
	{ "Hawaiian","haw",""} ,
	{ "Hebrew","heb","he"} ,
	{ "Herero","her","hz"} ,
	{ "Hiligaynon","hil",""} ,
	{ "Himachali languages; Western Pahari languages","him",""} ,
	{ "Hindi","hin","hi"} ,
	{ "Hiri Motu","hmo","ho"} ,
	{ "Hittite","hit",""} ,
	{ "Hmong; Mong","hmn",""} ,
	{ "Hungarian","hun","hu"} ,
	{ "Hupa","hup",""} ,
	{ "Iban","iba",""} ,
	{ "Icelandic","isl","is"} ,
	{ "Ido","ido","io"} ,
	{ "Igbo","ibo","ig"} ,
	{ "Ijo languages","ijo",""} ,
	{ "Iloko","ilo",""} ,
	{ "Inari Sami","smn",""} ,
	{ "Indic languages","inc",""} ,
	{ "Indo-European languages","ine",""} ,
	{ "Indonesian","ind","id"} ,
	{ "Ingush","inh",""} ,
	{ "Interlingua (International Auxiliary Language Association)","ina","ia"} ,
	{ "Interlingue; Occidental","ile","ie"} ,
	{ "Inuktitut","iku","iu"} ,
	{ "Inupiaq","ipk","ik"} ,
	{ "Iranian languages","ira",""} ,
	{ "Irish","gle","ga"} ,
	{ "Irish, Middle (900-1200)","mga",""} ,
	{ "Irish, Old (to 900)","sga",""} ,
	{ "Iroquoian languages","iro",""} ,
	{ "Italian","ita","it"} ,
	{ "Japanese","jpn","ja"} ,
	{ "Javanese","jav","jv"} ,
	{ "Judeo-Arabic","jrb",""} ,
	{ "Judeo-Persian","jpr",""} ,
	{ "Kabardian","kbd",""} ,
	{ "Kabyle","kab",""} ,
	{ "Kachin; Jingpho","kac",""} ,
	{ "Kalaallisut; Greenlandic","kal","kl"} ,
	{ "Kalmyk; Oirat","xal",""} ,
	{ "Kamba","kam",""} ,
	{ "Kannada","kan","kn"} ,
	{ "Kanuri","kau","kr"} ,
	{ "Kara-Kalpak","kaa",""} ,
	{ "Karachay-Balkar","krc",""} ,
	{ "Karelian","krl",""} ,
	{ "Karen languages","kar",""} ,
	{ "Kashmiri","kas","ks"} ,
	{ "Kashubian","csb",""} ,
	{ "Kawi","kaw",""} ,
	{ "Kazakh","kaz","kk"} ,
	{ "Khasi","kha",""} ,
	{ "Khoisan languages","khi",""} ,
	{ "Khotanese; Sakan","kho",""} ,
	{ "Kikuyu; Gikuyu","kik","ki"} ,
	{ "Kimbundu","kmb",""} ,
	{ "Kinyarwanda","kin","rw"} ,
	{ "Kirghiz; Kyrgyz","kir","ky"} ,
	{ "Klingon; tlhIngan-Hol","tlh",""} ,
	{ "Komi","kom","kv"} ,
	{ "Kongo","kon","kg"} ,
	{ "Konkani","kok",""} ,
	{ "Korean","kor","ko"} ,
	{ "Kosraean","kos",""} ,
	{ "Kpelle","kpe",""} ,
	{ "Kru languages","kro",""} ,
	{ "Kuanyama; Kwanyama","kua","kj"} ,
	{ "Kumyk","kum",""} ,
	{ "Kurdish","kur","ku"} ,
	{ "Kurukh","kru",""} ,
	{ "Kutenai","kut",""} ,
	{ "Ladino","lad",""} ,
	{ "Lahnda","lah",""} ,
	{ "Lamba","lam",""} ,
	{ "Land Dayak languages","day",""} ,
	{ "Lao","lao","lo"} ,
	{ "Latin","lat","la"} ,
	{ "Latvian","lav","lv"} ,
	{ "Lezghian","lez",""} ,
	{ "Limburgan; Limburger; Limburgish","lim","li"} ,
	{ "Lingala","lin","ln"} ,
	{ "Lithuanian","lit","lt"} ,
	{ "Lojban","jbo",""} ,
	{ "Low German; Low Saxon; German, Low; Saxon, Low","nds",""} ,
	{ "Lower Sorbian","dsb",""} ,
	{ "Lozi","loz",""} ,
	{ "Luba-Katanga","lub","lu"} ,
	{ "Luba-Lulua","lua",""} ,
	{ "Luiseno","lui",""} ,
	{ "Lule Sami","smj",""} ,
	{ "Lunda","lun",""} ,
	{ "Luo (Kenya and Tanzania)","luo",""} ,
	{ "Lushai","lus",""} ,
	{ "Luxembourgish; Letzeburgesch","ltz","lb"} ,
	{ "Macedonian","mkd","mk"} ,
	{ "Madurese","mad",""} ,
	{ "Magahi","mag",""} ,
	{ "Maithili","mai",""} ,
	{ "Makasar","mak",""} ,
	{ "Malagasy","mlg","mg"} ,
	{ "Malay","msa","ms"} ,
	{ "Malayalam","mal","ml"} ,
	{ "Maltese","mlt","mt"} ,
	{ "Manchu","mnc",""} ,
	{ "Mandar","mdr",""} ,
	{ "Mandingo","man",""} ,
	{ "Manipuri","mni",""} ,
	{ "Manobo languages","mno",""} ,
	{ "Manx","glv","gv"} ,
	{ "Maori","mri","mi"} ,
	{ "Mapudungun; Mapuche","arn",""} ,
	{ "Marathi","mar","mr"} ,
	{ "Mari","chm",""} ,
	{ "Marshallese","mah","mh"} ,
	{ "Marwari","mwr",""} ,
	{ "Masai","mas",""} ,
	{ "Mayan languages","myn",""} ,
	{ "Mende","men",""} ,
	{ "Mi'kmaq; Micmac","mic",""} ,
	{ "Minangkabau","min",""} ,
	{ "Mirandese","mwl",""} ,
	{ "Mohawk","moh",""} ,
	{ "Moksha","mdf",""} ,
	{ "Mon-Khmer languages","mkh",""} ,
	{ "Mongo","lol",""} ,
	{ "Mongolian","mon","mn"} ,
	{ "Mossi","mos",""} ,
	{ "Multiple languages","mul",""} ,
	{ "Munda languages","mun",""} ,
	{ "N'Ko","nqo",""} ,
	{ "Nahuatl languages","nah",""} ,
	{ "Nauru","nau","na"} ,
	{ "Navajo; Navaho","nav","nv"} ,
	{ "Ndebele, North; North Ndebele","nde","nd"} ,
	{ "Ndebele, South; South Ndebele","nbl","nr"} ,
	{ "Ndonga","ndo","ng"} ,
	{ "Neapolitan","nap",""} ,
	{ "Nepal Bhasa; Newari","new",""} ,
	{ "Nepali","nep","ne"} ,
	{ "Nias","nia",""} ,
	{ "Niger-Kordofanian languages","nic",""} ,
	{ "Nilo-Saharan languages","ssa",""} ,
	{ "Niuean","niu",""} ,
	{ "No linguistic content; Not applicable","zxx",""} ,
	{ "Nogai","nog",""} ,
	{ "Norse, Old","non",""} ,
	{ "North American Indian languages","nai",""} ,
	{ "Northern Frisian","frr",""} ,
	{ "Northern Sami","sme","se"} ,
	{ "Norwegian","nor","no"} ,
	{ "Norwegian Nynorsk; Nynorsk, Norwegian","nno","nn"} ,
	{ "Nubian languages","nub",""} ,
	{ "Nyamwezi","nym",""} ,
	{ "Nyankole","nyn",""} ,
	{ "Nyoro","nyo",""} ,
	{ "Nzima","nzi",""} ,
	{ "Occitan (post 1500)","oci","oc"} ,
	{ "Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE)","arc",""} ,
	{ "Ojibwa","oji","oj"} ,
	{ "Oriya","ori","or"} ,
	{ "Oromo","orm","om"} ,
	{ "Osage","osa",""} ,
	{ "Ossetian; Ossetic","oss","os"} ,
	{ "Otomian languages","oto",""} ,
	{ "Pahlavi","pal",""} ,
	{ "Palauan","pau",""} ,
	{ "Pali","pli","pi"} ,
	{ "Pampanga; Kapampangan","pam",""} ,
	{ "Pangasinan","pag",""} ,
	{ "Panjabi; Punjabi","pan","pa"} ,
	{ "Papiamento","pap",""} ,
	{ "Papuan languages","paa",""} ,
	{ "Pedi; Sepedi; Northern Sotho","nso",""} ,
	{ "Persian","fas","fa"} ,
	{ "Persian, Old (ca.600-400 B.C.)","peo",""} ,
	{ "Philippine languages","phi",""} ,
	{ "Phoenician","phn",""} ,
	{ "Pohnpeian","pon",""} ,
	{ "Polish","pol","pl"} ,
	{ "Portuguese","por","pt"} ,
	{ "Prakrit languages","pra",""} ,
	{ "Provencal, Old (to 1500);Occitan, Old (to 1500)","pro",""} ,
	{ "Pushto; Pashto","pus","ps"} ,
	{ "Quechua","que","qu"} ,
	{ "Rajasthani","raj",""} ,
	{ "Rapanui","rap",""} ,
	{ "Rarotongan; Cook Islands Maori","rar",""} ,
	{ "Reserved for local use","qaa-qtz",""} ,
	{ "Romance languages","roa",""} ,
	{ "Romanian; Moldavian; Moldovan","ron","ro"} ,
	{ "Romansh","roh","rm"} ,
	{ "Romany","rom",""} ,
	{ "Rundi","run","rn"} ,
	{ "Russian","rus","ru"} ,
	{ "Salishan languages","sal",""} ,
	{ "Samaritan Aramaic","sam",""} ,
	{ "Sami languages","smi",""} ,
	{ "Samoan","smo","sm"} ,
	{ "Sandawe","sad",""} ,
	{ "Sango","sag","sg"} ,
	{ "Sanskrit","san","sa"} ,
	{ "Santali","sat",""} ,
	{ "Sardinian","srd","sc"} ,
	{ "Sasak","sas",""} ,
	{ "Scots","sco",""} ,
	{ "Selkup","sel",""} ,
	{ "Semitic languages","sem",""} ,
	{ "Serbian","srp","sr"} ,
	{ "Serer","srr",""} ,
	{ "Shan","shn",""} ,
	{ "Shona","sna","sn"} ,
	{ "Sichuan Yi; Nuosu","iii","ii"} ,
	{ "Sicilian","scn",""} ,
	{ "Sidamo","sid",""} ,
	{ "Sign Languages","sgn",""} ,
	{ "Siksika","bla",""} ,
	{ "Sindhi","snd","sd"} ,
	{ "Sinhala; Sinhalese","sin","si"} ,
	{ "Sino-Tibetan languages","sit",""} ,
	{ "Siouan languages","sio",""} ,
	{ "Skolt Sami","sms",""} ,
	{ "Slave (Athapascan)","den",""} ,
	{ "Slavic languages","sla",""} ,
	{ "Slovak","slk","sk"} ,
	{ "Slovenian","slv","sl"} ,
	{ "Sogdian","sog",""} ,
	{ "Somali","som","so"} ,
	{ "Songhai languages","son",""} ,
	{ "Soninke","snk",""} ,
	{ "Sorbian languages","wen",""} ,
	{ "Sotho, Southern","sot","st"} ,
	{ "South American Indian languages","sai",""} ,
	{ "Southern Altai","alt",""} ,
	{ "Southern Sami","sma",""} ,
	{ "Spanish; Castilian","spa","es"} ,
	{ "Sranan Tongo","srn",""} ,
	{ "Sukuma","suk",""} ,
	{ "Sumerian","sux",""} ,
	{ "Sundanese","sun","su"} ,
	{ "Susu","sus",""} ,
	{ "Swahili","swa","sw"} ,
	{ "Swati","ssw","ss"} ,
	{ "Swedish","swe","sv"} ,
	{ "Swiss German; Alemannic; Alsatian","gsw",""} ,
	{ "Syriac","syr",""} ,
	{ "Tagalog","tgl","tl"} ,
	{ "Tahitian","tah","ty"} ,
	{ "Tai languages","tai",""} ,
	{ "Tajik","tgk","tg"} ,
	{ "Tamashek","tmh",""} ,
	{ "Tamil","tam","ta"} ,
	{ "Tatar","tat","tt"} ,
	{ "Telugu","tel","te"} ,
	{ "Tereno","ter",""} ,
	{ "Tetum","tet",""} ,
	{ "Thai","tha","th"} ,
	{ "Tibetan","bod","bo"} ,
	{ "Tigre","tig",""} ,
	{ "Tigrinya","tir","ti"} ,
	{ "Timne","tem",""} ,
	{ "Tiv","tiv",""} ,
	{ "Tlingit","tli",""} ,
	{ "Tok Pisin","tpi",""} ,
	{ "Tokelau","tkl",""} ,
	{ "Tonga (Nyasa)","tog",""} ,
	{ "Tonga (Tonga Islands)","ton","to"} ,
	{ "Tsimshian","tsi",""} ,
	{ "Tsonga","tso","ts"} ,
	{ "Tswana","tsn","tn"} ,
	{ "Tumbuka","tum",""} ,
	{ "Tupi languages","tup",""} ,
	{ "Turkish","tur","tr"} ,
	{ "Turkish, Ottoman (1500-1928)","ota",""} ,
	{ "Turkmen","tuk","tk"} ,
	{ "Tuvalu","tvl",""} ,
	{ "Tuvinian","tyv",""} ,
	{ "Twi","twi","tw"} ,
	{ "Udmurt","udm",""} ,
	{ "Ugaritic","uga",""} ,
	{ "Uighur; Uyghur","uig","ug"} ,
	{ "Ukrainian","ukr","uk"} ,
	{ "Umbundu","umb",""} ,
	{ "Uncoded languages","mis",""} ,
	{ "Undetermined","und",""} ,
	{ "Upper Sorbian","hsb",""} ,
	{ "Urdu","urd","ur"} ,
	{ "Uzbek","uzb","uz"} ,
	{ "Vai","vai",""} ,
	{ "Venda","ven","ve"} ,
	{ "Vietnamese","vie","vi"} ,
	{ "Volapuk","vol","vo"} ,
	{ "Votic","vot",""} ,
	{ "Wakashan languages","wak",""} ,
	{ "Walloon","wln","wa"} ,
	{ "Waray","war",""} ,
	{ "Washo","was",""} ,
	{ "Welsh","cym","cy"} ,
	{ "Western Frisian","fry","fy"} ,
	{ "Wolaitta; Wolaytta","wal",""} ,
	{ "Wolof","wol","wo"} ,
	{ "Xhosa","xho","xh"} ,
	{ "Yakut","sah",""} ,
	{ "Yao","yao",""} ,
	{ "Yapese","yap",""} ,
	{ "Yiddish","yid","yi"} ,
	{ "Yoruba","yor","yo"} ,
	{ "Yupik languages","ypk",""} ,
	{ "Zande languages","znd",""} ,
	{ "Zapotec","zap",""} ,
	{ "Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki","zza",""} ,
	{ "Zenaga","zen",""} ,
	{ "Zhuang; Chuang","zha","za"} ,
	{ "Zulu","zul","zu"} ,
	{ "Zuni","zun",""} ,
};


GF_EXPORT
u32 gf_lang_get_count()
{
	return sizeof(defined_languages) / sizeof(struct lang_def);
}

GF_EXPORT
s32 gf_lang_find(const char *lang_or_rfc_5646_code)
{
	//find the language ISO639 entry for the given code
	u32 i=0;
	u32 len=0;
	char *sep;
	u32 count = sizeof(defined_languages) / sizeof(struct lang_def);

	if (!lang_or_rfc_5646_code) return -1;

	len = (u32)strlen(lang_or_rfc_5646_code);
	sep = (char *) strchr(lang_or_rfc_5646_code, '-');
	if (sep) {
		sep[0] = 0;
		len = (u32) strlen(lang_or_rfc_5646_code);
		sep[0] = '-';
	}

	for (i=0; i<count; i++) {
		if (!strcmp(defined_languages[i].name, lang_or_rfc_5646_code)) {
			return i;
		}
		if ((len==3) && !strnicmp(defined_languages[i].three_char_code, lang_or_rfc_5646_code, 3)) {
			return i;
		}
		if ((len==2) && !strnicmp(defined_languages[i].two_char_code, lang_or_rfc_5646_code, 2)) {
			return i;
		}
	}
	return -1;
}

GF_EXPORT
const char *gf_lang_get_name(u32 idx)
{
	if (idx>=sizeof(defined_languages) / sizeof(struct lang_def)) return NULL;
	return defined_languages[idx].name;
}

GF_EXPORT
const char *gf_lang_get_2cc(u32 idx)
{
	if (idx>=sizeof(defined_languages) / sizeof(struct lang_def)) return NULL;
	return defined_languages[idx].two_char_code;
}

GF_EXPORT
const char *gf_lang_get_3cc(u32 idx)
{
	if (idx>=sizeof(defined_languages) / sizeof(struct lang_def)) return NULL;
	return defined_languages[idx].three_char_code;
}


GF_EXPORT
GF_Err gf_dynstrcat(char **str, const char *to_append, const char *sep)
{
	u32	l1, l2, lsep;
	if (!to_append) return GF_OK;

	lsep = sep ? (u32) strlen(sep) : 0;
	l1 = *str ? (u32) strlen(*str) : 0;
	l2 = (u32) strlen(to_append);
	if (l1) (*str) = gf_realloc((*str), sizeof(char)*(l1+l2+lsep+1));
	else (*str) = gf_realloc((*str), sizeof(char)*(l2+lsep+1));

	if (! (*str) )
		return GF_OUT_OF_MEM;

	(*str)[l1]=0;
	if (l1 && sep) strcat((*str), sep);
	strcat((*str), to_append);
	return GF_OK;
}

GF_EXPORT
Bool gf_parse_lfrac(const char *value, GF_Fraction64 *frac)
{
	Float v;
	u32 len, i;
	Bool all_num=GF_TRUE;
	char *sep;
	if (!frac) return GF_FALSE;
	frac->num = 0;
	frac->den = 0;
	if (!value) return GF_FALSE;

	if (sscanf(value, LLD"/"LLU, &frac->num, &frac->den) == 2) {
		return GF_TRUE;
	}
	if (sscanf(value, LLD"-"LLU, &frac->num, &frac->den) == 2) {
		return GF_TRUE;
	}
	if (sscanf(value, "%g", &v) != 1) {
		frac->num = 0;
		frac->den = 0;
		return GF_FALSE;
	}
	sep = strchr(value, '.');
	if (!sep) sep = strchr(value, ',');
	if (!sep) {
		len = (u32) strlen(value);
		for (i=0; i<len; i++) {
			if (((value[i]<'0') || (value[i]>'9')) && (value[i]!='-') && (value[i]!='+'))
				return GF_FALSE;
		}
		frac->num = atol(value);
		frac->den = 1;
		return GF_TRUE;
	}

	len = (u32) strlen(sep+1);
	for (i=1; i<=len; i++) {
		if ((sep[i]<'0') || (sep[i]>'9')) {
			all_num = GF_FALSE;
			break;
		}
	}
	if (all_num) {
		u32 div_trail_zero = 1;
		sscanf(value, LLD"."LLU, &frac->num, &frac->den);

		i=0;
		frac->den = 1;
		while (i<len) {
			i++;
			frac->den *= 10;
		}
		//trash trailing zero
		i=len;
		while (i>0) {
			if (sep[i] != '0') {
				break;
			}
			div_trail_zero *= 10;
			i--;
		}


		frac->num *= frac->den / div_trail_zero;
		frac->num += atoi(sep+1) / div_trail_zero;
		frac->den /= div_trail_zero;

		return GF_TRUE;
	}

	if (len <= 3) frac->den = 1000;
	else if (len <= 6) frac->den = 1000000;
	else frac->den = 1000000000;

	frac->num = (u64)( (atof(value) * frac->den) + 0.5 );
	return GF_TRUE;
}

GF_EXPORT
Bool gf_parse_frac(const char *value, GF_Fraction *frac)
{
	GF_Fraction64 r;
	Bool res;
	if (!frac) return GF_FALSE;
	res = gf_parse_lfrac(value, &r);
	while ((r.num >= 0x80000000) && (r.den > 1000)) {
		r.num /= 1000;
		r.den /= 1000;
	}
	frac->num = (s32) r.num;
	frac->den = (u32) r.den;
	return res;
}

Bool gf_strict_atoi(const char* str, int* ans) {
    char * end_ptr;
    *ans = strtol(str, &end_ptr, 10); 
    return !isspace(*str) && end_ptr != str && *end_ptr == '\0';
}