/*
 *			GPAC - Multimedia Framework C SDK
 *
 *          Authors: Romain Bouqueau - Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-20XX
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

#if !defined(WIN32) && !defined(_WIN32_WCE)
/* This is needed for asprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#endif

#include <string.h>


#define STD_MALLOC	0
#define GOOGLE_MALLOC	1
#define INTEL_MALLOC	2
#define DL_MALLOC		3

#ifdef WIN32
#define USE_MALLOC	STD_MALLOC
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

#ifndef SYMBOL_EXPORT
#if defined(__GNUC__) && __GNUC__ >= 4
#define SYMBOL_EXPORT __attribute__((visibility("default")))
#endif
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


size_t gpac_allocated_memory = 0;
size_t gpac_nb_alloc_blocs = 0;

#ifdef _WIN32_WCE
#define assert(p)
#endif

static void register_address(void *ptr, size_t size, const char *filename, int line);
static int unregister_address(void *ptr, const char *filename, int line);

static void gf_memory_log(unsigned int level, const char *fmt, ...);
enum
{
	/*! Disable all Log message*/
	GF_MEMORY_QUIET = 0,
	/*! Log message describes an error*/
	GF_MEMORY_ERROR = 1,
	/*! Log message describes a warning*/
	GF_MEMORY_WARNING,
	/*! Log message is informational (state, etc..)*/
	GF_MEMORY_INFO,
	/*! Log message is a debug info*/
	GF_MEMORY_DEBUG,
};

static void *gf_mem_malloc_basic(size_t size, const char *filename, int line)
{
	return MALLOC(size);
}
static void *gf_mem_calloc_basic(size_t num, size_t size_of, const char *filename, int line)
{
	return CALLOC(num, size_of);
}
static void *gf_mem_realloc_basic(void *ptr, size_t size, const char *filename, int line)
{
	return REALLOC(ptr, size);
}
static void gf_mem_free_basic(void *ptr, const char *filename, int line)
{
	FREE(ptr);
}
static char *gf_mem_strdup_basic(const char *str, const char *filename, int line)
{
	STRDUP(str);
}

void *gf_mem_malloc_tracker(size_t size, const char *filename, int line)
{
	void *ptr = MALLOC(size);
	if (!ptr) {
		gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] malloc() has returned a NULL pointer\n");
		assert(0);
	} else {
		register_address(ptr, size, filename, line);
	}
	gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] malloc %3d bytes at %p\n             in file %s at line %d\n", size, ptr, filename, line);
	return ptr;
}

void *gf_mem_calloc_tracker(size_t num, size_t size_of, const char *filename, int line)
{
	size_t size = num*size_of;
	void *ptr = CALLOC(num, size_of);
	if (!ptr) {
		gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] calloc() has returned a NULL pointer\n");
		assert(0);
	} else {
		register_address(ptr, size, filename, line);
	}
	gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] calloc %3d bytes at %p\n             in file %s at line %d\n", size, ptr, filename, line);
	return ptr;
}

void gf_mem_free_tracker(void *ptr, const char *filename, int line)
{
	int size_prev;
	if (ptr && (size_prev=unregister_address(ptr, filename, line))) {
		gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] free   %3d bytes at %p\n             in file %s at line %d\n", size_prev, ptr, filename, line);
		FREE(ptr);
	}
}

void *gf_mem_realloc_tracker(void *ptr, size_t size, const char *filename, int line)
{
	void *ptr_g;
	int size_prev;
	if (!ptr) {
		gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] realloc() from a null pointer: calling malloc() instead\n");
		return gf_mem_malloc_tracker(size, filename, line);
	}
	/*a) The return value is NULL if the size is zero and the buffer argument is not NULL. In this case, the original block is freed.*/
	if (!size) {
		gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] realloc() with a null size: calling free() instead\n");
		gf_mem_free_tracker(ptr, filename, line);
		return NULL;
	}
	ptr_g = REALLOC(ptr, size);
	if (!ptr_g) {
		/*b) The return value is NULL if there is not enough available memory to expand the block to the given size. In this case, the original block is unchanged.*/
		gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] realloc() has returned a NULL pointer\n");
		assert(0);
	} else {
		size_prev = unregister_address(ptr, filename, line);
		register_address(ptr_g, size, filename, line);
		gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] realloc %3d (instead of %3d) bytes at %p (instead of %p)\n             in file %s at line %d\n", size, size_prev, ptr_g, ptr, filename, line);
	}
	return ptr_g;
}

char *gf_mem_strdup_tracker(const char *str, const char *filename, int line)
{
	char *ptr;
	if (!str) return NULL;
	ptr = (char*)gf_mem_malloc_tracker(strlen(str)+1, filename, line);
	strcpy(ptr, str);
	return ptr;
}

static void *(*gf_mem_malloc_proto)(size_t size, const char *filename, int line) = gf_mem_malloc_basic;
static void *(*gf_mem_calloc_proto)(size_t num, size_t size_of, const char *filename, int line) = gf_mem_calloc_basic;
static void *(*gf_mem_realloc_proto)(void *ptr, size_t size, const char *filename, int line) = gf_mem_realloc_basic;
static void (*gf_mem_free_proto)(void *ptr, const char *filename, int line) = gf_mem_free_basic;
static char *(*gf_mem_strdup_proto)(const char *str, const char *filename, int line) = gf_mem_strdup_basic;

#ifndef MY_GF_EXPORT
#if defined(__GNUC__) && __GNUC__ >= 4
#define MY_GF_EXPORT __attribute__((visibility("default")))
#else
/*use def files for windows or let compiler decide*/
#define MY_GF_EXPORT
#endif
#endif

MY_GF_EXPORT void *gf_mem_malloc(size_t size, const char *filename, int line)
{
	return gf_mem_malloc_proto(size, filename, line);
}

MY_GF_EXPORT void *gf_mem_calloc(size_t num, size_t size_of, const char *filename, int line)
{
	return gf_mem_calloc_proto(num, size_of, filename, line);
}

MY_GF_EXPORT
void *gf_mem_realloc(void *ptr, size_t size, const char *filename, int line)
{
	return gf_mem_realloc_proto(ptr, size, filename, line);
}

MY_GF_EXPORT
void gf_mem_free(void *ptr, const char *filename, int line)
{
	gf_mem_free_proto(ptr, filename, line);
}

MY_GF_EXPORT
char *gf_mem_strdup(const char *str, const char *filename, int line)
{
	return gf_mem_strdup_proto(str, filename, line);
}

MY_GF_EXPORT
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
	struct s_memory_element *next;
	int line;
	char *filename;
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
	struct s_memory_element *next;
	int line;
	char *filename;
} memory_element;

/*pointer to the first element of the list*/
typedef memory_element* memory_list;

#endif


/*base functions (add, find, del_item, del) are implemented upon a stack model*/
static void gf_memory_add_stack(memory_element **p, void *ptr, int size, const char *filename, int line)
{
	memory_element *element = (memory_element*)MALLOC(sizeof(memory_element)+strlen(filename)+1);
	element->ptr = ptr;
	element->size = size;
	element->line = line;
	element->next = *p;
	element->filename = (char*)&(element->filename)+sizeof(element->filename);
	strcpy(element->filename, filename);
	*p = element;
}

/*returns the position of a ptr from a memory_element, 0 if not found*/
static int gf_memory_find_stack(memory_element *p, void *ptr)
{
	int i = 1;
	memory_element *element = p;
	while (element) {
		if (element->ptr == ptr) {
			return i;
		}
		element = element->next;
		i++;
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
static void gf_memory_add(memory_list *p, void *ptr, int size, const char *filename, int line)
{
	unsigned int hash;
	if (!*p) *p = (memory_list) CALLOC(HASH_ENTRIES, sizeof(memory_element*));
	assert(*p);

	hash = gf_memory_hash(ptr);
	gf_memory_add_stack(&((*p)[hash]), ptr, size, filename, line);
}


static int gf_memory_find(memory_list p, void *ptr)
{
	unsigned int hash;
	assert(p);
	if (!p) return 0;
	hash = gf_memory_hash(ptr);
	return gf_memory_find_stack(p[hash], ptr);
}

static int gf_memory_del_item(memory_list *p, void *ptr)
{
	unsigned int hash;
	int ret;
	memory_element **sub_list;
	if (!*p) *p = (memory_list) CALLOC(HASH_ENTRIES, sizeof(memory_element*));
	assert(*p);
	hash = gf_memory_hash(ptr);
	sub_list = &((*p)[hash]);
	if (!sub_list) return 0;
	ret = gf_memory_del_item_stack(sub_list, ptr);
	if (ret && !((*p)[hash])) {
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

static void register_address(void *ptr, size_t size, const char *filename, int line)
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

	gf_memory_add(&memory_add, ptr, (int) size, filename, line);
	gf_memory_del_item(&memory_rem, ptr); /*the same block can be reallocated, so remove it from the deallocation list*/

	/*update stats*/
	gpac_allocated_memory += size;
	gpac_nb_alloc_blocs++;

	/*gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] register   %6d bytes at %p (%8d Bytes in %4d Blocks allocated)\n", size, ptr, gpac_allocated_memory, gpac_nb_alloc_blocs);*/

	/*unlock*/
	gf_mx_v(gpac_allocations_lock);
}

#if 0
void gf_check_address(void *ptr)
{
	int pos;

	/*lock*/
	gf_mx_p(gpac_allocations_lock);

	if ( (pos=gf_memory_find(memory_rem, ptr)) ) {
		int i;
		unsigned int hash = gf_memory_hash(ptr);
		memory_element *element = memory_rem[hash];
		assert(element);
		for (i=1; i<pos; i++)
			element = element->next;
		assert(element);
		gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] the block %p has already been freed\n             in file %s at line %d\n", ptr, element->filename, element->line);
		assert(0);
	}
	/*unlock*/
	gf_mx_v(gpac_allocations_lock);
}
#endif

/*returns the size of the unregistered block*/
static int unregister_address(void *ptr, const char *filename, int line)
{
	int size = 0; /*default: failure*/

	/*lock*/
	gf_mx_p(gpac_allocations_lock);

	if (!memory_add) {
		if (!memory_rem) {
			/*assume we're rather destroying the mutex (ie calling the gf_mx_del() below)
			  than being called by free() before the first allocation occured*/
			return 1;
			/*gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] calling free() before the first allocation occured\n");
			   assert(0); */
		}
	} else {
		if (!gf_memory_find(memory_add, ptr)) {
			int pos;
			if (!(pos=gf_memory_find(memory_rem, ptr))) {
				gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] trying to free a never allocated block (%p)\n", ptr);
				/* assert(0); */ /*don't assert since this is often due to allocations that occured out of gpac (fonts, etc.)*/
			} else {
				int i;
#if GPAC_MEMORY_TRACKING_HASH_TABLE
				unsigned int hash = gf_memory_hash(ptr);
				memory_element *element = memory_rem[hash];
#else
				memory_element *element = memory_rem;
#endif
				assert(element);
				for (i=1; i<pos; i++)
					element = element->next;
				assert(element);
				gf_memory_log(GF_MEMORY_ERROR, "[MemTracker] the block %p trying to be deleted\n             in file %s at line %d\n             has already been freed\n             in file %s at line %d\n", ptr, filename, line, element->filename, element->line);
				assert(0);
			}
		} else {
			size = gf_memory_del_item(&memory_add, ptr);
			assert(size>=0);

			/*update stats*/
			gpac_allocated_memory -= size;
			gpac_nb_alloc_blocs--;

			/*gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] unregister %6d bytes at %p (%8d bytes in %4d blocks remaining)\n", size, ptr, gpac_allocated_memory, gpac_nb_alloc_blocs); */

			/*the allocation list is empty: free the lists to avoid a leak (we should be exiting)*/
			if (!memory_add) {
				assert(!gpac_allocated_memory);
				assert(!gpac_nb_alloc_blocs);

				/*we destroy the mutex we own, then we return*/
				gf_mx_del(gpac_allocations_lock);
				gpac_allocations_lock = NULL;

				gf_memory_log(GF_MEMORY_DEBUG, "[MemTracker] the allocated-blocks-list is empty: the freed-blocks-list will be emptied too.\n");
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
	char msg[1024];
	assert(strlen(fmt) < 200);
	va_start(vl, fmt);
	vsprintf(msg, fmt, vl);
	GF_LOG(level, GF_LOG_MEMORY, (msg));
	va_end(vl);
}

/*prints allocations sum-up*/
static void print_memory_size()
{
	unsigned int level = gpac_nb_alloc_blocs ? GF_MEMORY_ERROR : GF_MEMORY_INFO;
	gf_memory_log(level, "[MemTracker] Total: %d bytes allocated in %d blocks\n", gpac_allocated_memory, gpac_nb_alloc_blocs);
}

GF_EXPORT
u64 gf_memory_size()
{
	return (u64) gpac_allocated_memory;
}

/*prints the state of current allocations*/
void gf_memory_print()
{
	/*if lists are empty, the mutex is also NULL*/
	if (!memory_add) {
		assert(!gpac_allocations_lock);
		gf_memory_log(GF_MEMORY_INFO, "[MemTracker] gf_memory_print(): the memory tracker is not initialized.\n");
	} else {
		int i=0;
		assert(gpac_allocations_lock);

		gf_memory_log(GF_MEMORY_INFO, "\n[MemTracker] Printing the current state of allocations:\n");

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
				char szVal[51];
				u32 size;
				next_element = curr_element->next;
				size = curr_element->size>=50 ? 49 : curr_element->size;
				memcpy(szVal, curr_element->ptr, sizeof(char)*size);
				szVal[size+1] = 0;
				gf_memory_log(GF_MEMORY_INFO, "[MemTracker] Memory Block %p (size %d) allocated\n\tin file %s at line %d\n\tstring dump: %s\n", curr_element->ptr, curr_element->size, curr_element->filename, curr_element->line, szVal);
				curr_element = next_element;
			}
		}
		print_memory_size();

		/*unlock*/
		gf_mx_v(gpac_allocations_lock);
	}
}

#endif /*GPAC_MEMORY_TRACKING*/


/*gf_asprintf(): as_printf portable implementation*/
#if defined(WIN32) || defined(_WIN32_WCE) || (defined (__SVR4) && defined (__sun))
static GFINLINE int gf_vasprintf (char **strp, const char *fmt, va_list ap)
{
	int vsn_ret, size;
	char *buffer, *realloc_buffer;

	size = 2 * (u32) strlen(fmt); /*first guess for the size*/
	buffer = (char*)gf_malloc(size);
	if (buffer == NULL)
		return -1;

	while (1) {
#if !defined(WIN32) && !defined(_WIN32_WCE)
#define _vsnprintf vsnprintf
#endif
		vsn_ret = _vsnprintf(buffer, size, fmt, ap);

		/* If that worked, return the string. */
		if (vsn_ret>-1 && vsn_ret<size)	{
			*strp = buffer;
			return vsn_ret;
		}

		/*else double the allocated size*/
		size *= 2;
		realloc_buffer = (char*)gf_realloc(buffer, size);
		if (!realloc_buffer)	{
			gf_free(buffer);
			return -1;
		} else {
			buffer = realloc_buffer;
		}

	}
}
#endif

int gf_asprintf(char **strp, const char *fmt, ...)
{
	s32 size;
	va_list args;
	va_start(args, fmt);
#if defined(WIN32) || defined(_WIN32_WCE) || (defined (__SVR4) && defined (__sun))
	size = gf_vasprintf(strp, fmt, args);
#else
	size = asprintf(strp, fmt, args);
#endif
	va_end(args);
	return size;
}
