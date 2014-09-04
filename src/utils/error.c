/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


static char szTYPE[5];


GF_EXPORT
const char *gf_4cc_to_str(u32 type)
{
	u32 ch, i;
	char *ptr, *name = (char *)szTYPE;
	ptr = name;
	for (i = 0; i < 4; i++, name++) {
		ch = type >> (8 * (3-i) ) & 0xff;
		if ( ch >= 0x20 && ch <= 0x7E ) {
			*name = ch;
		} else {
			*name = '.';
		}
	}
	*name = 0;
	return (const char *) ptr;
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

static u64 prev_pos = 0;
static u64 prev_pc = 0;
static void gf_on_progress_std(const char *_title, u64 done, u64 total)
{
	Double prog;
	u32 pos;
	const char *szT = _title ? (char *)_title : (char *) "";
	prog = (double) done;
	prog /= total;
	pos = MIN((u32) (20 * prog), 20);

	if (pos>prev_pos) {
		prev_pos = 0;
		prev_pc = 0;
	}
	if (done==total) {
		u32 len = (u32) strlen(szT) + 40;
		while (len) {
			fprintf(stderr, " ");
			len--;
		};
		fprintf(stderr, "\r");
	}
	else {
		u32 pc = (u32) ( 100 * prog);
		if ((pos!=prev_pos) || (pc!=prev_pc)) {
			prev_pos = pos;
			prev_pc = pc;
			fprintf(stderr, "%s: |%s| (%02d/100)\r", szT, szProg[pos], pc);
			fflush(stderr);
		}
	}
}

static gf_on_progress_cbk prog_cbk = NULL;
static void *user_cbk;

GF_EXPORT
void gf_set_progress(const char *title, u64 done, u64 total)
{
	if (prog_cbk) {
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

/*ENTRIES MUST BE IN THE SAME ORDER AS LOG_TOOL DECLARATION IN <gpac/tools.h>*/
static struct log_tool_info {
	u32 type;
	const char *name;
	u32 level;
} global_log_tools [] =
{
	{ GF_LOG_CORE, "core", GF_LOG_WARNING },
	{ GF_LOG_CODING, "coding", GF_LOG_WARNING },
	{ GF_LOG_CONTAINER, "container", GF_LOG_WARNING },
	{ GF_LOG_NETWORK, "network", GF_LOG_WARNING },
	{ GF_LOG_RTP, "rtp", GF_LOG_WARNING },
	{ GF_LOG_AUTHOR, "author", GF_LOG_WARNING },
	{ GF_LOG_SYNC, "sync", GF_LOG_WARNING },
	{ GF_LOG_CODEC, "codec", GF_LOG_WARNING },
	{ GF_LOG_PARSER, "parser", GF_LOG_WARNING },
	{ GF_LOG_MEDIA, "media", GF_LOG_WARNING },
	{ GF_LOG_SCENE, "scene", GF_LOG_WARNING },
	{ GF_LOG_SCRIPT, "script", GF_LOG_WARNING },
	{ GF_LOG_INTERACT, "interact", GF_LOG_WARNING },
	{ GF_LOG_COMPOSE, "compose", GF_LOG_WARNING },
	{ GF_LOG_CACHE, "cache", GF_LOG_WARNING },
	{ GF_LOG_MMIO, "mmio", GF_LOG_WARNING },
	{ GF_LOG_RTI, "rti", GF_LOG_WARNING },
	{ GF_LOG_SMIL, "smil", GF_LOG_WARNING },
	{ GF_LOG_MEMORY, "mem", GF_LOG_WARNING },
	{ GF_LOG_AUDIO, "audio", GF_LOG_WARNING },
	{ GF_LOG_MODULE, "module", GF_LOG_WARNING },
	{ GF_LOG_MUTEX, "mutex", GF_LOG_WARNING },
	{ GF_LOG_CONDITION, "condition", GF_LOG_WARNING },
	{ GF_LOG_DASH, "dash", GF_LOG_WARNING },
	{ GF_LOG_CONSOLE, "console", GF_LOG_INFO },
	{ GF_LOG_APP, "app", GF_LOG_INFO },
	{ GF_LOG_SCHEDULER, "scheduler", GF_LOG_INFO },
};

GF_EXPORT
GF_Err gf_log_modify_tools_levels(const char *val)
{
#ifndef GPAC_DISABLE_LOG
	u32 level;
	char *sep, *sep_level;
	while (val && strlen(val)) {
		const char *next_val = NULL;
		const char *tools = NULL;
		/*look for log level*/
		sep_level = strchr(val, '@');
		if (!sep_level) {
			fprintf(stderr, "Unrecognized log format %s - expecting logTool@logLevel\n", val);
			return GF_BAD_PARAM;
		}

		level = 0;
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
			fprintf(stderr, "Unknown log level specified: %s\n", sep_level+1);
			return GF_BAD_PARAM;
		}

		sep_level[0] = 0;
		tools = val;
		while (tools) {
			u32 i;

			sep = strchr(tools, ':');
			if (sep) sep[0] = 0;

			if (!stricmp(tools, "all")) {
				for (i=0; i<GF_LOG_TOOL_MAX; i++)
					global_log_tools[i].level = level;
			}
			else {
				Bool found = 0;
				for (i=0; i<GF_LOG_TOOL_MAX; i++) {
					if (!strcmp(global_log_tools[i].name, tools)) {
						global_log_tools[i].level = level;
						found = 1;
					}
				}
				if (!found) {
					sep_level[0] = '@';
					if (sep) sep[0] = ':';
					fprintf(stderr, "Unknown log tool specified: %s\n", val);
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
GF_Err gf_log_set_tools_levels(const char *val)
{
#ifndef GPAC_DISABLE_LOG
	u32 i;
	for (i=0; i<GF_LOG_TOOL_MAX; i++)
		global_log_tools[i].level = GF_LOG_WARNING;

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

#ifndef GPAC_DISABLE_LOG
u32 call_lev = 0;
u32 call_tool = 0;

GF_EXPORT
Bool gf_log_tool_level_on(u32 log_tool, u32 log_level)
{
	assert(log_tool<GF_LOG_TOOL_MAX);
	if (global_log_tools[log_tool].level >= log_level) return GF_TRUE;
	return GF_FALSE;
}

void default_log_callback(void *cbck, u32 level, u32 tool, const char* fmt, va_list vlist)
{
#ifndef _WIN32_WCE
	vfprintf(stderr, fmt, vlist);
#endif
}

static void *user_log_cbk = NULL;
static gf_log_cbk log_cbk = default_log_callback;
static Bool log_exit_on_error = GF_FALSE;

GF_EXPORT
void gf_log(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	log_cbk(user_log_cbk, call_lev, call_tool, fmt, vl);
	va_end(vl);
	if (log_exit_on_error && call_lev==GF_LOG_ERROR)
		exit(1);
}

GF_EXPORT
void gf_log_set_strict_error(Bool strict)
{
	log_exit_on_error = strict;
}

GF_EXPORT
void gf_log_set_tool_level(u32 tool, u32 level)
{
	assert(tool<=GF_LOG_TOOL_MAX);
	if (tool==GF_LOG_ALL) {
		u32 i;
		for (i=0; i<GF_LOG_TOOL_MAX; i++)
			global_log_tools[i].level = level;
	} else {
		global_log_tools[tool].level = level;
	}
}

GF_EXPORT
void gf_log_lt(u32 ll, u32 lt)
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
	user_log_cbk = usr_cbk;
	return prev_cbk;
}

#else
GF_EXPORT
void gf_log(const char *fmt, ...)
{
}
GF_EXPORT
void gf_log_lt(u32 ll, u32 lt)
{
}
GF_EXPORT
void gf_log_set_strict_error(Bool strict)
{
}

GF_EXPORT
gf_log_cbk gf_log_set_callback(void *usr_cbk, gf_log_cbk cbk)
{
	return NULL;
}
#endif

static char szErrMsg[20];

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

	/*MPEG-4 Errors */
	case GF_BUFFER_TOO_SMALL:
		return "Bad Buffer size (too small)";
	case GF_NON_COMPLIANT_BITSTREAM:
		return "BitStream Not Compliant";
	case GF_CODEC_NOT_FOUND:
		return "Media Codec not found";

	/*DMIF errors - local and control plane */
	case GF_URL_ERROR:
		return "Requested URL is not valid or cannot be found";

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
		return "Network Timeout";
	case GF_IP_SOCK_WOULD_BLOCK:
		return "Socket Would Block";
	case GF_IP_CONNECTION_CLOSED:
		return "Connection to server closed";
	case GF_IP_UDP_TIMEOUT:
		return "UDP traffic timeout";
	case GF_AUTHENTICATION_FAILURE:
		return "Authentication failure";
	case GF_SCRIPT_NOT_READY:
		return "Script not ready for playback";
	case GF_INVALID_CONFIGURATION:
		return "Bad configuration for the current context";
	case GF_NOT_FOUND:
		return "At least one required element has not been found";
	case GF_MISSING_REQUIREMENTS:
		return "The filter is missing at least one requirement";
	case GF_WRONG_DATAFORMAT:
		return "Unexpected data format";
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
u32 gf_crc_32(const char *data, u32 len)
{
	register u32 i;
	u32 crc = 0xffffffff;
	if (!data) return 0;
	for (i=0; i<len; i++)
		crc = (crc << 8) ^ gf_crc_table[((crc >> 24) ^ *data++) & 0xff];

	return crc;
}


#define CHECK_MAC(_a) "#_a :" ? (_a) ? "yes":"no"

GF_EXPORT
const char *gpac_features()
{
	const char *features = ""
#ifdef GPAC_64_BITS
	                       "GPAC_64_BITS"
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
#ifdef GPAC_HAS_SSL
	                       "GPAC_HAS_SSL "
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
	                       "GPAC_HAS_SPIDERMONKEY "
#endif
#ifdef GPAC_HAS_JPEG
	                       "GPAC_HAS_JPEG "
#endif
#ifdef GPAC_HAS_PNG
	                       "GPAC_HAS_PNG "
#endif
#ifdef GPAC_DISABLE_3D
	                       "GPAC_DISABLE_3D "
#endif
#ifdef GPAC_USE_TINYGL
	                       "GPAC_USE_TINYGL "
#endif
#ifdef GPAC_USE_OGL_ES
	                       "GPAC_USE_OGL_ES "
#endif
#if defined(_WIN32_WCE)
#ifdef GPAC_USE_IGPP
	                       "GPAC_USE_IGPP "
#endif
#ifdef GPAC_USE_IGPP_HP
	                       "GPAC_USE_IGPP_HP "
#endif
#endif
#ifdef GPAC_DISABLE_SVG
	                       "GPAC_DISABLE_SVG "
#endif
#ifdef GPAC_DISABLE_VRML
	                       "GPAC_DISABLE_VRML "
#endif
#ifdef GPAC_MINIMAL_ODF
	                       "GPAC_MINIMAL_ODF "
#endif
#ifdef GPAC_DISABLE_BIFS
	                       "GPAC_DISABLE_BIFS "
#endif
#ifdef GPAC_DISABLE_QTVR
	                       "GPAC_DISABLE_QTVR "
#endif
#ifdef GPAC_DISABLE_AVILIB
	                       "GPAC_DISABLE_AVILIB "
#endif
#ifdef GPAC_DISABLE_OGG
	                       "GPAC_DISABLE_OGG "
#endif
#ifdef GPAC_DISABLE_MPEG2PS
	                       "GPAC_DISABLE_MPEG2PS "
#endif
#ifdef GPAC_DISABLE_MPEG2PS
	                       "GPAC_DISABLE_MPEG2TS "
#endif
#ifdef GPAC_DISABLE_SENG
	                       "GPAC_DISABLE_SENG "
#endif
#ifdef GPAC_DISABLE_MEDIA_IMPORT
	                       "GPAC_DISABLE_MEDIA_IMPORT "
#endif
#ifdef GPAC_DISABLE_AV_PARSERS
	                       "GPAC_DISABLE_AV_PARSERS "
#endif
#ifdef GPAC_DISABLE_MEDIA_EXPORT
	                       "GPAC_DISABLE_MEDIA_EXPORT "
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
#ifdef GPAC_DISABLE_SCENE_ENCODER
	                       "GPAC_DISABLE_SCENE_ENCODER "
#endif
#ifdef GPAC_DISABLE_LOADER_ISOM
	                       "GPAC_DISABLE_LOADER_ISOM "
#endif
#ifdef GPAC_DISABLE_OD_DUMP
	                       "GPAC_DISABLE_OD_DUMP "
#endif
#ifdef GPAC_DISABLE_MCRYPT
	                       "GPAC_DISABLE_MCRYPT "
#endif
#ifdef GPAC_DISABLE_ISOM
	                       "GPAC_DISABLE_MCRYPT "
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
#ifdef GPAC_DISABLE_LASER
	                       "GPAC_DISABLE_LASER "
#endif
#ifdef GPAC_DISABLE_STREAMING
	                       "GPAC_DISABLE_STREAMING "
#endif

	                       ;
	return features;
}
