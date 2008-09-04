/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


#include <stdlib.h>
#include <string.h>
#ifndef _WIN32_WCE
#include <assert.h>
#endif

/*GPAC memory tracking*/
size_t gpac_allocated_memory = 0;
size_t gpac_nb_alloc_blocs = 0;
void *gf_malloc(size_t size)
{
	void *ptr;
	size_t size_g = size + sizeof(size_t);
	ptr = malloc(size_g);
	*(size_t *)ptr = size;
	gpac_allocated_memory += size;
	gpac_nb_alloc_blocs++;
	return (void *) ( (char *)ptr + sizeof(size_t) );
}
void *gf_realloc(void *ptr, size_t size)
{
	size_t prev_size;
	char *ptr_g = (char *)ptr;
	if (!ptr) return gf_malloc(size);
	ptr_g -= sizeof(size_t);
	prev_size = *(size_t *)ptr_g;
#ifndef _WIN32_WCE
	assert(gpac_allocated_memory >= prev_size);
#endif
	gpac_allocated_memory -= prev_size;
	ptr_g = (char *) realloc(ptr_g, size+sizeof(size_t));
	*(size_t *)ptr_g = size;
	gpac_allocated_memory += size;
	return ptr_g + sizeof(size_t);
}
void gf_free(void *ptr)
{
	if (ptr) {
		char *ptr_g = (char *)ptr - sizeof(size_t);
		size_t size_g = *(size_t *)ptr_g;
#ifndef _WIN32_WCE
		assert(gpac_allocated_memory >= size_g);
#endif
		gpac_allocated_memory -= size_g;
		gpac_nb_alloc_blocs--;
		free(ptr_g);
	}
}
char *gf_strdup(const char *str)
{
	size_t len = strlen(str) + 1;
	char *ptr = (char *) gf_malloc(len);
	memcpy(ptr, str, len);
	return ptr;
}


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

static u32 prev_pos = 0;
static u32 prev_pc = 0;
static void gf_on_progress_stdout(char *_title, u32 done, u32 total)
{
	Double prog;
	u32 pos;
	char *szT = _title ? (char *)_title : (char *) "";
	prog = done;
	prog /= total;
	pos = MIN((u32) (20 * prog), 20);

	if (pos>prev_pos) {
		prev_pos = 0;
		prev_pc = 0;
	}
	if (done==total) { 
		u32 len = strlen(szT) + 40;
		while (len) { fprintf(stdout, " "); len--; };
		fprintf(stdout, "\r");
	}
	else {
		u32 pc = (u32) ( 100 * prog);
		if ((pos!=prev_pos) || (pc!=prev_pc)) {
			prev_pos = pos;
			prev_pc = pc;
			fprintf(stdout, "%s: |%s| (%02d/100)\r", szT, szProg[pos], pc);
			fflush(stdout);
		}
	}
}

static gf_on_progress_cbk prog_cbk = NULL;
static void *user_cbk;

GF_EXPORT
void gf_set_progress(char *title, u32 done, u32 total)
{
	if (prog_cbk) {
		prog_cbk(user_cbk, title, done, total);
	}
#ifndef _WIN32_WCE
	else {
		gf_on_progress_stdout(title, done, total);
	}
#endif
}

GF_EXPORT
void gf_set_progress_callback(void *_user_cbk, gf_on_progress_cbk _prog_cbk)
{
	prog_cbk = _prog_cbk;
	user_cbk = _user_cbk;
}


u32 global_log_level = 0;
u32 global_log_tools = 0;


#ifndef GPAC_DISABLE_LOG
u32 call_lev = 0;
u32 call_tool = 0;

GF_EXPORT
u32 gf_log_get_level() { return global_log_level; }

GF_EXPORT
u32 gf_log_get_tools() { return global_log_tools; }

void default_log_callback(void *cbck, u32 level, u32 tool, const char* fmt, va_list vlist)
{
#ifndef _WIN32_WCE
    vfprintf(stdout, fmt, vlist);
#endif
}


static void *user_log_cbk = NULL;
static gf_log_cbk log_cbk = default_log_callback;

GF_EXPORT
void gf_log(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	log_cbk(user_log_cbk, call_lev, call_tool, fmt, vl);
	va_end(vl);
}

GF_EXPORT
void gf_log_set_level(u32 level)
{
	global_log_level = level;
}
GF_EXPORT
void gf_log_set_tools(u32 modules)
{
	global_log_tools = modules;
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
void gf_log_set_level(u32 level)
{
}
GF_EXPORT
void gf_log_set_tools(u32 modules)
{
}
GF_EXPORT
gf_log_cbk gf_log_set_callback(void *usr_cbk, gf_log_cbk cbk)
{
	return NULL;
}
GF_EXPORT
u32 gf_log_get_level() { return 0; }

GF_EXPORT
u32 gf_log_get_tools() { return 0; }

#endif


GF_EXPORT
const char *gf_error_to_string(GF_Err e)
{
	switch (e) {
	case GF_SCRIPT_INFO:
		return "Script message";
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

	default:
		return "Unknown Error";
	}
}

