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

#include <string.h>

#define STD_MALLOC	0
#define GOOGLE_MALLOC	1
#define INTEL_MALLOC	2
#define DL_MALLOC		3

#ifdef WIN32
#define USE_MALLOC	DL_MALLOC
#else
#define USE_MALLOC	STD_MALLOC
#endif


#if defined(_WIN32_WCE) && !defined(strdup)
#define strdup	_strdup
#endif

/*
	WARNING - you must enable C++ style compilation of this file (error.c) to be able to compile
	with google malloc. This is not set by default in the project settings.
*/
#if (USE_MALLOC==GOOGLE_MALLOC)
#include <config.h>
#include <base/commandlineflags.h>
#include <google/malloc_extension.h>

#ifdef WIN32
#pragma comment(lib, "libtcmalloc_minimal")
#endif

#define MALLOC	malloc
#define CALLOC	calloc
#define REALLOC	realloc
#define FREE	free
#define STRDUP(a) return strdup(a);

/*we must use c++ compiler for google malloc :( */
#define CDECL	extern "C"
#endif

#if (USE_MALLOC==INTEL_MALLOC)
#define CDECL	
CDECL void * scalable_malloc(size_t size);
CDECL void * scalable_realloc(void* ptr, size_t size);
CDECL void * scalable_calloc(size_t num, size_t size);
CDECL void   scalable_free(void* ptr);

#ifdef WIN32
#pragma comment(lib, "tbbmalloc.lib")
#endif

#define MALLOC	scalable_malloc
#define CALLOC	scalable_calloc
#define REALLOC	scalable_realloc
#define FREE	scalable_free
#define STRDUP(_a) if (_a) { unsigned int len = strlen(_a)+1; char *ptr = (char *) scalable_malloc(len); strcpy(ptr, _a); return ptr; } else { return NULL; }

#endif

#ifndef CDECL	
#define CDECL	
#endif

#if (USE_MALLOC==DL_MALLOC)

CDECL void * dlmalloc(size_t size);
CDECL void * dlrealloc(void* ptr, size_t size);
CDECL void * dlcalloc(size_t num, size_t size);
CDECL void   dlfree(void* ptr);

#define MALLOC	dlmalloc
#define CALLOC	dlcalloc
#define REALLOC	dlrealloc
#define FREE	dlfree
#define STRDUP(_a) if (_a) { unsigned int len = strlen(_a)+1; char *ptr = (char *) dlmalloc(len); strcpy(ptr, _a); return ptr; } else { return NULL; }

#endif

#if (USE_MALLOC==STD_MALLOC)

#include <stdlib.h>

#define MALLOC	malloc
#define CALLOC	calloc
#define REALLOC	realloc
#define FREE	free
#define STRDUP(a) return strdup(a);

#endif



#ifndef _WIN32_WCE
#include <assert.h>
#endif

/*This is to handle cases where config.h is generated at the root of the gpac build tree (./configure)
This is only needed when building libgpac and modules when libgpac is not installed*/
#ifdef GPAC_HAVE_CONFIG_H
# include "config.h"
#else
# include <gpac/configuration.h>
#endif

/*GPAC memory tracking*/
#ifndef GPAC_MEMORY_TRACKING

CDECL
void *gf_malloc(size_t size)
{
	return MALLOC(size);
}
CDECL
void *gf_calloc(size_t num, size_t size_of)
{
	return CALLOC(num, size_of);
}
CDECL
void *gf_realloc(void *ptr, size_t size)
{
	return REALLOC(ptr, size);
}
CDECL
void gf_free(void *ptr)
{
	FREE(ptr);
}
CDECL
char *gf_strdup(const char *str)
{
	STRDUP(str);
}

#else


CDECL
size_t gpac_allocated_memory = 0;
size_t gpac_nb_alloc_blocs = 0;

#ifdef _WIN32_WCE
#define assert(p)
#endif

static void register_address(void *ptr, size_t size, char *filename, int line);
static int unregister_address(void *ptr, char *filename, int line);

static void gf_memory_log(unsigned int level, const char *fmt, ...);
enum
{
	/*! Log message describes an error*/
	GF_MEMORY_ERROR = 1,
	/*! Log message describes a warning*/
	GF_MEMORY_WARNING,
	/*! Log message is informational (state, etc..)*/
	GF_MEMORY_INFO,
	/*! Log message is a debug info*/
	GF_MEMORY_DEBUG,
};

static void *gf_mem_malloc_basic(size_t size, char *filename, int line)
{
	return MALLOC(size);
}
static void *gf_mem_calloc_basic(size_t num, size_t size_of, char *filename, int line)
{
	return CALLOC(num, size_of);
}
static void *gf_mem_realloc_basic(void *ptr, size_t size, char *filename, int line)
{
	return REALLOC(ptr, size);
}
static void gf_mem_free_basic(void *ptr, char *filename, int line)
{
	FREE(ptr);
}
static char *gf_mem_strdup_basic(const char *str, char *filename, int line)
{
	STRDUP(str);
}

void *gf_mem_malloc_tracker(size_t size, char *filename, int line)
{
	void *ptr = MALLOC(size);
	if (!ptr) {
		gf_memory_log(GF_MEMORY_ERROR, "malloc() has returned a NULL pointer\n");
		assert(0);
	} else {
		register_address(ptr, size, filename, line);
	}
	//gf_memory_log(GF_MEMORY_DEBUG, "malloc   0x%08X %8d\n", ptr, size, gpac_nb_alloc_blocs, gpac_allocated_memory);
	return ptr;
}

void *gf_mem_calloc_tracker(size_t num, size_t size_of, char *filename, int line)
{
	size_t size = num*size_of;
	void *ptr = CALLOC(num, size_of);
	if (!ptr) {
		gf_memory_log(GF_MEMORY_ERROR, "calloc() has returned a NULL pointer\n");
		assert(0);
	} else {
		register_address(ptr, size, filename, line);
	}
	//gf_memory_log(GF_MEMORY_DEBUG, "calloc   0x%08X %8d\n", ptr, size, gpac_nb_alloc_blocs, gpac_allocated_memory);
	return ptr;
}

void *gf_mem_realloc_tracker(void *ptr, size_t size, char *filename, int line)
{
	void *ptr_g;
	int size_prev;
	if (!ptr) {
		//gf_memory_log(GF_MEMORY_DEBUG, "realloc() from a null pointer: calling malloc() instead\n");
		return gf_mem_malloc_tracker(size, filename, line);
	}
	size_prev = unregister_address(ptr, filename, line);
	//gf_memory_log(GF_MEMORY_DEBUG, "realloc- 0x%08X %8d %8d %8d\n", ptr, size_prev, gpac_nb_alloc_blocs, gpac_allocated_memory);
	ptr_g = REALLOC(ptr, size);
	register_address(ptr_g, size, filename, line);
	//gf_memory_log(GF_MEMORY_DEBUG, "realloc+ 0x%08X %8d %8d %8d\n", ptr_g, size, gpac_nb_alloc_blocs, gpac_allocated_memory);
	return ptr_g;
}

void gf_mem_free_tracker(void *ptr, char *filename, int line)
{
	int size_prev;
	if (ptr && (size_prev=unregister_address(ptr, filename, line))) {
		//gf_memory_log(GF_MEMORY_DEBUG, "free     0x%08X %8d %8d %8d\n", ptr, size_prev, gpac_nb_alloc_blocs, gpac_allocated_memory);
		FREE(ptr);
	}
}

char *gf_mem_strdup_tracker(const char *str, char *filename, int line)
{
	char *ptr = (char*)gf_mem_malloc_tracker(strlen(str)+1, filename, line);
	strcpy(ptr, str);
	return ptr;
}

static void *(*gf_mem_malloc_proto)(size_t size, char *filename, int line) = gf_mem_malloc_basic;
static void *(*gf_mem_calloc_proto)(size_t num, size_t size_of, char *filename, int line) = gf_mem_calloc_basic;
static void *(*gf_mem_realloc_proto)(void *ptr, size_t size, char *filename, int line) = gf_mem_realloc_basic;
static void (*gf_mem_free_proto)(void *ptr, char *filename, int line) = gf_mem_free_basic;
static char *(*gf_mem_strdup_proto)(const char *str, char *filename, int line) = gf_mem_strdup_basic;

CDECL
void *gf_mem_malloc(size_t size, char *filename, int line)
{
	return gf_mem_malloc_proto(size, filename, line);
}
CDECL
void *gf_mem_calloc(size_t num, size_t size_of, char *filename, int line)
{
	return gf_mem_calloc_proto(num, size_of, filename, line);
}
CDECL
void *gf_mem_realloc(void *ptr, size_t size, char *filename, int line)
{
	return gf_mem_realloc_proto(ptr, size, filename, line);
}
CDECL
void gf_mem_free(void *ptr, char *filename, int line)
{
	gf_mem_free_proto(ptr, filename, line);
}
CDECL
char *gf_mem_strdup(const char *str, char *filename, int line)
{
	return gf_mem_strdup_proto(str, filename, line);
}

CDECL
void gf_mem_enable_tracker()
{
	gf_mem_malloc_proto = gf_mem_malloc_tracker;
	gf_mem_calloc_proto = gf_mem_calloc_tracker;
	gf_mem_realloc_proto = gf_mem_realloc_tracker;
	gf_mem_free_proto = gf_mem_free_tracker;
	gf_mem_strdup_proto = gf_mem_strdup_tracker;
}


#define GPAC_MEMORY_TRACKING_HASH_TABLE 1
#if GPAC_MEMORY_TRACKING_HASH_TABLE

#define HASH_ENTRIES 4096

typedef struct s_memory_element
{
	void *ptr;
	int size;
	char *filename;
	int line;
	struct s_memory_element *next;
} memory_element;

/*pointer to the first element of the list*/
typedef memory_element** memory_list;

static unsigned int gf_memory_hash(void *ptr)
{
	return (((unsigned int)ptr>>4)+(unsigned int)ptr)%HASH_ENTRIES;
}

#else

typedef struct s_memory_element
{
	void *ptr;
	int size;
	char *filename;
	int line;
	struct s_memory_element *next;
} memory_element;

/*pointer to the first element of the list*/
typedef memory_element* memory_list;

#endif


/*base functions (add, find, del_item, del) are implemented upon a stack model*/
static void gf_memory_add_stack(memory_element **p, void *ptr, int size, char *filename, int line)
{
	memory_element *element = (memory_element*)MALLOC(sizeof(memory_element));
	element->ptr = ptr;
	element->size = size;
	element->filename = filename;
	element->line = line;
	element->next = *p;
	*p = element;
}
static int gf_memory_find_stack(memory_element *p, void *ptr)
{
	memory_element *element = p;
	while (element) {
		if (element->ptr == ptr) {
			return 1;
		}
		element = element->next;
	}
	return 0;
}

/*returns the size of the deleted item*/
static int gf_memory_del_item_stack(memory_element **p, void *ptr)
{
	int size;
	memory_element *curr_element=*p, *prev_element=NULL;
	while (curr_element) {
		if (curr_element->ptr == ptr) {
			if (prev_element) prev_element->next = curr_element->next;
			else *p = curr_element->next;
			size = curr_element->size;
			FREE(curr_element);
			return size;
		}
		prev_element = curr_element;
		curr_element = curr_element->next;
	}
	return 0;
}

static void gf_memory_del_stack(memory_element **p)
{
	memory_element *curr_element=*p, *next_element;
	while (curr_element) {
		next_element = curr_element->next;
		FREE(curr_element);
		curr_element = next_element;
	}
	*p = NULL;
}


#if GPAC_MEMORY_TRACKING_HASH_TABLE

/*this list is implemented as a stack to minimise the cost of freeing recent allocations*/
static void gf_memory_add(memory_list *p, void *ptr, int size, char *filename, int line)
{
	unsigned int pos;
	if (!*p) *p = (memory_list) CALLOC(HASH_ENTRIES, sizeof(memory_element*));
	assert(*p);

	pos = gf_memory_hash(ptr);
	gf_memory_add_stack(&((*p)[pos]), ptr, size, filename, line);
}


static int gf_memory_find(memory_list p, void *ptr)
{
	unsigned int pos;
	assert(p);
	if (!p) return 0;
	pos = gf_memory_hash(ptr);
	return gf_memory_find_stack(p[pos], ptr);
}

static int gf_memory_del_item(memory_list *p, void *ptr)
{
	unsigned int pos;
	int ret;
	memory_element **sub_list;
	if (!*p) *p = (memory_list) CALLOC(HASH_ENTRIES, sizeof(memory_element*));
	assert(*p);
	pos = gf_memory_hash(ptr);
	sub_list = &((*p)[pos]);
	if (!sub_list) return 0;
	ret = gf_memory_del_item_stack(sub_list, ptr);
	if (ret && !((*p)[pos])) {
		/*check for deletion*/
		int i;
		for (i=0; i<HASH_ENTRIES; i++)
			if (&((*p)[i])) break;
		if (i==HASH_ENTRIES) {
			FREE(*p);
			p = NULL;
		}
	}
	return ret;
}

static void gf_memory_del(memory_list *p)
{
	int i;
	for (i=0; i<HASH_ENTRIES; i++)
		gf_memory_del_stack(&((*p)[i]));
	FREE(*p);
	p = NULL;
}

#else

#define gf_memory_add		gf_memory_add_stack
#define gf_memory_del		gf_memory_del_stack
#define gf_memory_del_item	gf_memory_del_item_stack
#define gf_memory_find		gf_memory_find_stack

#endif


#endif /*GPAC_MEMORY_TRACKING*/


#include <gpac/tools.h>


/*GPAC memory tracking*/
#ifdef GPAC_MEMORY_TRACKING

#include <gpac/thread.h>

/*global lists of allocations and deallocations*/
memory_list memory_add = NULL, memory_rem = NULL;
GF_Mutex *gpac_allocations_lock = NULL;

static void register_address(void *ptr, size_t size, char *filename, int line)
{
	/*mutex initialization*/
	if (gpac_allocations_lock == 0) {
		assert(!memory_add);
		assert(!memory_rem);
		gpac_allocations_lock = (GF_Mutex*)1; /*must be non-null to avoid a recursive infinite call*/
		gpac_allocations_lock = gf_mx_new("gpac_allocations_lock");
	}
	else if (gpac_allocations_lock == (void*)1) {
		/*we're initializing the mutex (ie called by the gf_mx_new() above)*/
		return;
	}

	/*lock*/
	gf_mx_p(gpac_allocations_lock);

	gf_memory_add(&memory_add, ptr, size, filename, line);
	gf_memory_del_item(&memory_rem, ptr); /*the same block can be reallocated, so remove it from the deallocation list*/

	/*update stats*/
	gpac_allocated_memory += size;
	gpac_nb_alloc_blocs++;
	
	gf_memory_log(GF_MEMORY_DEBUG, "register   %6d bytes at 0x%08X (%8d Bytes in %4d Blocks allocated)\n", size, ptr, gpac_allocated_memory, gpac_nb_alloc_blocs);

	/*unlock*/
	gf_mx_v(gpac_allocations_lock);
}

/*returns the size of the unregistered block*/
static int unregister_address(void *ptr, char *filename, int line)
{
	int size = 0; /*default: failure*/

	/*lock*/
	gf_mx_p(gpac_allocations_lock);

	if (!memory_add) {
		if (!memory_rem) {
			/*assume we're rather destroying the mutex (ie calling the gf_mx_del() below)
			  than being called by free() before the first allocation occured*/
			return 1;
			//gf_memory_log(GF_MEMORY_ERROR, "calling free() before the first allocation occured\n");
			//assert(0);
		}
	} else {
		if (!gf_memory_find(memory_add, ptr)) {
			if (!gf_memory_find(memory_rem, ptr)) {
				gf_memory_log(GF_MEMORY_ERROR, "trying to free a never allocated block (0x%08X)\n", ptr);
				//assert(0); /*don't assert since this is often due to allocations that occured out of gpac (fonts, etc.)*/
			} else {
				gf_memory_log(GF_MEMORY_ERROR, "the block 0x%08X has already been freed line%5d from %s\n", line, filename);
				assert(0);
			}
		} else {
			size = gf_memory_del_item(&memory_add, ptr);
			assert(size>=0);

			/*update stats*/
			gpac_allocated_memory -= size;
			gpac_nb_alloc_blocs--;

			gf_memory_log(GF_MEMORY_DEBUG, "unregister %6d bytes at 0x%08X (%8d bytes in %4d blocks remaining)\n", size, ptr, gpac_allocated_memory, gpac_nb_alloc_blocs);

			/*the allocation list is empty: free the lists to avoid a leak (we should be exiting)*/
			if (!memory_add) {
				assert(!gpac_allocated_memory);
				assert(!gpac_nb_alloc_blocs);

				/*we destroy the mutex we own, then we return*/
				gf_mx_del(gpac_allocations_lock);
				gpac_allocations_lock = NULL;

				gf_memory_log(GF_MEMORY_DEBUG, "the allocated-blocks-list is empty: the freed-blocks-list will be emptied too.\n");
				gf_memory_del(&memory_rem);

				return size;
			} else {
				gf_memory_add(&memory_rem, ptr, size, filename, line);
			}
		}
	}

	/*unlock*/
	gf_mx_v(gpac_allocations_lock);

	return size;
}

static void gf_memory_log(unsigned int level, const char *fmt, ...)
{
	va_list vl;
	char msg[255]; /*since we print*/
	assert(strlen(fmt) < 80);
	va_start(vl, fmt);
	vsprintf(msg, fmt, vl);
	GF_LOG(level, GF_LOG_MEMORY, (msg));
	va_end(vl);
}

/*prints the state of current allocations*/
void gf_memory_print()
{
	/*if lists are empty, the mutex is also NULL*/
	if (!memory_add) {
		assert(!gpac_allocations_lock);
		gf_memory_log(GF_MEMORY_INFO, "gf_memory_print(): the memory tracker is not initialized.\n");
	} else {
		int i=0;
		assert(gpac_allocations_lock);

		/*lock*/
		gf_mx_p(gpac_allocations_lock);
#if GPAC_MEMORY_TRACKING_HASH_TABLE
		for (i=0; i<HASH_ENTRIES; i++) {
			memory_element *curr_element = memory_add[i], *next_element;
#else
		{
			memory_element *curr_element = memory_add, *next_element;
#endif
			while (curr_element) {
				next_element = curr_element->next;
				gf_memory_log(GF_MEMORY_INFO, "Memory Block 0x%08X allocated line%5d from %s\n", curr_element->ptr, curr_element->line, curr_element->filename);
				printf("Memory Block 0x%08X allocated line%5d from %s\n", curr_element->ptr, curr_element->line, curr_element->filename);
				curr_element = next_element;
			}
		}
		gf_memory_log(GF_MEMORY_INFO, "Total: %d bytes allocated on %d blocks\n", gpac_allocated_memory, gpac_nb_alloc_blocs);
		printf("Total: %d bytes allocated on %d blocks\n", gpac_allocated_memory, gpac_nb_alloc_blocs);

		/*unlock*/
		gf_mx_v(gpac_allocations_lock);
	}
}

#endif


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


/* crc32 from ffmpeg*/
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
u32 gf_crc_32(char *data, u32 len)
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
