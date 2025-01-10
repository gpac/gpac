/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre, Pierre Souchay
 *			Copyright (c) Telecom ParisTech 2010-2025
 *					All rights reserved
 *
 *   This file is part of GPAC / downloader sub-project
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

#include "downloader.h"

#ifndef GPAC_DISABLE_NETWORK

#include <gpac/network.h>
#include <gpac/download.h>
#include <gpac/token.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/base_coding.h>
#include <gpac/tools.h>
#include <gpac/config_file.h>
#include <stdio.h>
#include <string.h>

#if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500
#include <unistd.h>
#endif

static const char * CACHE_SECTION_NAME = "cache";
static const char * CACHE_SECTION_KEY_URL = "url";
static const char * CACHE_SECTION_KEY_RANGE = "range";
static const char * CACHE_SECTION_KEY_ETAG = "ETag";
static const char * CACHE_SECTION_KEY_MIME_TYPE = "Content-Type";
static const char * CACHE_SECTION_KEY_CONTENT_SIZE = "Content-Length";
static const char * CACHE_SECTION_KEY_LAST_MODIFIED = "Last-Modified";
static const char * CACHE_SECTION_KEY_MAXAGE = "MaxAge";
static const char * CACHE_SECTION_KEY_MUST_REVALIDATE = "MustRevalidate";
static const char * CACHE_SECTION_KEY_NOCACHE = "NoCache";
static const char * CACHE_SECTION_KEY_CREATED = "Created";
static const char * CACHE_SECTION_KEY_LAST_HIT = "LastHit";
static const char * CACHE_SECTION_KEY_NUM_HIT = "NumHit";
static const char * CACHE_SECTION_KEY_INWRITE = "InWrite";
static const char * CACHE_SECTION_KEY_TODELETE = "ToDelete";
static const char * CACHE_SECTION_USERS = "users";

enum CacheValid
{
	CORRUPTED = 1,
    DELETED = 1<<1,
    IN_PROGRESS = 1<<2,
};

/**
* This opaque structure handles the data from the cache
*/
struct __DownloadedCacheEntryStruct
{
	// URL of the cache (never NULL)
	char *url;
	// Hash of the cache (never NULL)
	char *hash;
	// Name of the cache filename, (NULL for mem)
	char *cache_filename;
	// Name of the config filename, (NULL for mem)
	char *cfg_filename;
	// Theorical size of cache if any
	u32 contentLength;
	// Real size of cache
	u32 cacheSize;
	// The last modification time on the server
	char *serverLastModified;
	//The last modification time of the cache if any
	char *diskLastModified;
	// server ETag if any
	char *serverETag;
	//disk ETag if any
	char * diskETag;
	// Mime-type
	char * mimeType;
	// Write pointer for the cache
	FILE * writeFilePtr;
	// Bytes written during this cache session
	u32 written_in_cache;
	// Flag indicating whether we have to revalidate
	enum CacheValid   flags;
	//pointer to write session
	const GF_DownloadSession * write_session;
	//list of read sessions
	GF_List * sessions;
	//set to true if disk files for this entry shall be removed upon destroy (due to error)
	Bool discard_on_delete;

	u64 max_age;
	Bool must_revalidate;
	Bool other_in_use;

	//start and end range of the cache
	u64 range_start, range_end;

	//append to cache
	Bool continue_file;
	//contentLength before append
	u32 previousRangeContentLength;
	//set once headers have been processed
	Bool headers_done;
	// Set to true if file is not stored on disk
	Bool memory_stored;
	//allocation
	u32 mem_allocated;
	u8 *mem_storage;
    GF_Blob cache_blob;

	//for external blob only
    GF_Blob *external_blob;
	char *forced_headers;
	u32 downtime;

	//prevent deleting files when deleting entries, typically for init segments
    Bool persistent;
};

Bool gf_sys_check_process_id(u32 pid);

Bool gf_cache_entry_persistent(const DownloadedCacheEntry entry)
{
	return entry ? entry->persistent : GF_FALSE;
}
void gf_cache_entry_set_persistent(const DownloadedCacheEntry entry)
{
	if (entry) entry->persistent = GF_TRUE;
}

static const char * cache_file_prefix = "gpac_cache_";

typedef struct
{
	char *name;
	u64 created;
	u64 last_hit;
	u32 nb_hit;
	u32 size;
} CacheInfo;

typedef struct
{
	GF_List *files;
	u64 tot_size;
} CacheGather;

static Bool cache_cleanup_processes(GF_Config *cfg)
{
	u32 i, count=gf_cfg_get_key_count(cfg, CACHE_SECTION_USERS);
	for (i=0;i<count;i++) {
		const char *opt = gf_cfg_get_key_name(cfg, CACHE_SECTION_USERS, i);
		if (!opt) break;
		u32 procid;
		sscanf(opt, "%u", &procid);
		if (gf_sys_check_process_id(procid)) continue;
		gf_cfg_set_key(cfg, CACHE_SECTION_USERS, opt, NULL);
		count--;
		i--;
	}
	return count ? GF_TRUE : GF_FALSE;
}

static Bool gather_cache_files(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	CacheGather *gci = (CacheGather *)cbck;
	GF_Config *file = gf_cfg_new(NULL, item_path);
	u32 i, count, size=0;
	const char *opt = gf_cfg_get_key(file, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CONTENT_SIZE);
	if (opt) sscanf(opt, "%u", &size);
	gci->tot_size += size;

	if (cache_cleanup_processes(file)) {
		gf_cfg_del(file);
		return GF_FALSE;
	}

	CacheInfo *ci;
	GF_SAFEALLOC(ci, CacheInfo);
	if (ci) ci->name = gf_strdup(item_path);
	if (!ci || !ci->name) {
		if (ci) gf_free(ci);
		gf_cfg_del(file);
		return GF_FALSE;
	}
	ci->size = size;
	opt = gf_cfg_get_key(file, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CREATED);
	if (opt) sscanf(opt, LLU, &ci->created);
	opt = gf_cfg_get_key(file, CACHE_SECTION_NAME, CACHE_SECTION_KEY_LAST_HIT);
	if (opt) sscanf(opt, LLU, &ci->last_hit);
	opt = gf_cfg_get_key(file, CACHE_SECTION_NAME, CACHE_SECTION_KEY_NUM_HIT);
	if (opt) { sscanf(opt, "%u", &ci->nb_hit); if (ci->nb_hit) ci->nb_hit--; }
	gf_cfg_del(file);

	count = gf_list_count(gci->files);
	for (i=0; i<count; i++) {
		CacheInfo *a_ci = gf_list_get(gci->files, i);
		//sort by nb hits first
		if (ci->nb_hit<a_ci->nb_hit) {
			gf_list_insert(gci->files, ci, i);
			return GF_FALSE;
		} else if (ci->nb_hit>a_ci->nb_hit) continue;

		//then by last hit
		if (ci->last_hit<a_ci->last_hit) {
			gf_list_insert(gci->files, ci, i);
			return GF_FALSE;
		}
		else if (ci->last_hit>a_ci->last_hit) continue;

		//then by created
		if (ci->created<a_ci->created) {
			gf_list_insert(gci->files, ci, i);
			return GF_FALSE;
		}
		else if (ci->created>a_ci->created) continue;

		//then by size
		if (ci->size<a_ci->size) {
			gf_list_insert(gci->files, ci, i);
			return GF_FALSE;
		}
	}
	gf_list_add(gci->files, ci);
	return 0;
}

u64 gf_cache_cleanup(const char * directory, u32 max_size)
{
	char szLOCK[GF_MAX_PATH];
	sprintf(szLOCK, "%s/.lock", directory);
	if (!gs_sys_create_lockfile(szLOCK))
		return max_size;

	CacheGather gci = {0};
	gci.files = gf_list_new();
	gf_enum_directory(directory, GF_FALSE, gather_cache_files, &gci, "*.txt");
	u32 cache_size = gci.tot_size;

	while (gf_list_count(gci.files)) {
		CacheInfo *ci = gf_list_pop_back(gci.files);
		if (cache_size>max_size) {
			if (cache_size>ci->size) cache_size -= ci->size;
			else cache_size = 0;
			gf_file_delete(ci->name);
			char *sep = strstr(ci->name, ".txt");
			sep[0] = 0;
			gf_file_delete(ci->name);
			sep[0] = '.';
		}
		gf_free(ci->name);
		gf_free(ci);
	}
	gf_list_del(gci.files);
	gf_file_delete(szLOCK);
	return cache_size;
}

void gf_cache_entry_set_delete_files_when_deleted(const DownloadedCacheEntry entry)
{
	if (entry && !entry->persistent) {
		entry->discard_on_delete = GF_TRUE;
	}
}

#define CHECK_ENTRY if (!entry) { GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE, ("[CACHE] entry is null at " __FILE__ ":%d\n", __LINE__)); return GF_BAD_PARAM; }

/*
* Getters functions
*/

const char * gf_cache_get_etag_on_server ( const DownloadedCacheEntry entry )
{
	return entry ? entry->serverETag : NULL;
}

const char * gf_cache_get_mime_type ( const DownloadedCacheEntry entry )
{
	return entry ? entry->mimeType : NULL;
}


GF_Err gf_cache_set_headers_processed(const DownloadedCacheEntry entry)
{
	if (!entry) return GF_BAD_PARAM;
	entry->headers_done = GF_TRUE;
	return GF_OK;
}

Bool gf_cache_are_headers_processed(const DownloadedCacheEntry entry)
{
	if (!entry) return GF_FALSE;
	return entry->headers_done;
}

GF_Err gf_cache_set_etag_on_server(const DownloadedCacheEntry entry, const char * eTag ) {
	if (!entry)
		return GF_BAD_PARAM;
	if (entry->serverETag)
		gf_free(entry->serverETag);
	entry->serverETag = eTag ? gf_strdup(eTag) : NULL;
	return GF_OK;
}

GF_Err gf_cache_set_etag_on_disk(const DownloadedCacheEntry entry, const char * eTag ) {
	if (!entry)
		return GF_BAD_PARAM;
	if (entry->diskETag)
		gf_free(entry->diskETag);
	entry->diskETag = eTag ? gf_strdup(eTag) : NULL;
	return GF_OK;
}

GF_Err gf_cache_set_mime_type(const DownloadedCacheEntry entry, const char * mime_type ) {
	if (!entry)
		return GF_BAD_PARAM;
	if (entry->mimeType)
		gf_free(entry->mimeType);
	entry->mimeType = mime_type? gf_strdup( mime_type) : NULL;
	return GF_OK;
}

u64 gf_cache_get_start_range( const DownloadedCacheEntry entry )
{
	return entry ? entry->range_start : 0;
}

u64 gf_cache_get_end_range( const DownloadedCacheEntry entry )
{
	return entry ? entry->range_end : 0;
}

const char * gf_cache_get_url ( const DownloadedCacheEntry entry )
{
	return entry ? entry->url : NULL;
}

const char * gf_cache_get_last_modified_on_server ( const DownloadedCacheEntry entry )
{
	return entry ? entry->serverLastModified : NULL;
}

GF_Err gf_cache_set_last_modified_on_server ( const DownloadedCacheEntry entry, const char * newLastModified )
{
	if (!entry)
		return GF_BAD_PARAM;
	if (entry->serverLastModified)
		gf_free(entry->serverLastModified);
	entry->serverLastModified = newLastModified ? gf_strdup(newLastModified) : NULL;
	return GF_OK;
}

GF_Err gf_cache_set_last_modified_on_disk ( const DownloadedCacheEntry entry, const char * newLastModified )
{
	if (!entry)
		return GF_BAD_PARAM;
	if (entry->diskLastModified)
		gf_free(entry->diskLastModified);
	entry->diskLastModified = newLastModified ? gf_strdup(newLastModified) : NULL;
	return GF_OK;
}

static GF_LockStatus cache_entry_lock(const char *lockfile)
{
	GF_LockStatus lock_type;
	u32 start=gf_sys_clock();
	while (1) {
		lock_type = gs_sys_create_lockfile(lockfile);
		if (lock_type) break;
		if (gf_sys_clock()-start>50) break;
	}
	return lock_type;
}


GF_Err gf_cache_flush_disk_cache ( const DownloadedCacheEntry entry, Bool success )
{
	char buff[100];
	CHECK_ENTRY;
	if (entry->mem_storage)
		return GF_OK;

	char szLOCK[GF_MAX_PATH];
	sprintf(szLOCK, "%s.lock", entry->cfg_filename);
	GF_LockStatus lock_type = cache_entry_lock(szLOCK);
	GF_Config *cfg = gf_cfg_new(NULL, entry->cfg_filename);

	if (success) {
		char szSize[50];
		sprintf(szSize, "%u", entry->written_in_cache);
		gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CONTENT_SIZE, szSize);
	}
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_INWRITE, NULL);

	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_URL, entry->url);

	if (entry->range_start || entry->range_end) {
		sprintf(buff, LLD"-"LLD, entry->range_start, entry->range_end);
		gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_RANGE, buff);
	}

	if (entry->mimeType)
		gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_MIME_TYPE, entry->mimeType);
	if (entry->diskETag)
		gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_ETAG, entry->diskETag);
	if (entry->diskLastModified)
		gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_LAST_MODIFIED, entry->diskLastModified);

	snprintf(buff, 16, "%d", entry->contentLength);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CONTENT_SIZE, buff);
	//save and destroy
	gf_cfg_del(cfg);
	if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);
	return GF_OK;
}

u32 gf_cache_get_cache_filesize(const DownloadedCacheEntry entry )
{
	return entry ? entry->cacheSize : -1;
}

const char * gf_cache_get_cache_filename( const DownloadedCacheEntry entry )
{
	return entry ? entry->cache_filename : NULL;
}

GF_Err gf_cache_get_http_headers(const DownloadedCacheEntry entry, const char **etag, const char **last_modif)
{
	if (!entry || !etag || !last_modif)
		return GF_BAD_PARAM;

	*etag = *last_modif = NULL;
	if (entry->flags & (CORRUPTED|DELETED))
		return GF_OK;

	*etag = entry->diskETag;
	*last_modif = entry->diskLastModified;
	if (entry->persistent && entry->memory_stored) {
		*etag = entry->serverETag;
		*last_modif = entry->serverLastModified;
	}
	return GF_OK;
}

static void gf_cache_check_if_cache_file_is_corrupted(const DownloadedCacheEntry entry, const char *url, GF_Config *cfg)
{
	const char * keyValue = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_URL );
	if ( keyValue == NULL || stricmp ( url, keyValue ) ) {
		entry->flags |= CORRUPTED;
		return;
	}

	keyValue = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_RANGE);
	if (keyValue) {
		u64 s, e;
		sscanf(keyValue, LLD"-"LLD, &s, &e);
		/*mark as corrupted if not same range (we don't support this for the time being ...*/
		if ((s!=entry->range_start) || (e!=entry->range_end)) {
			entry->flags |= CORRUPTED;
			return;
		}
	}

	if (gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_NOCACHE) ) {
		entry->flags |= CORRUPTED;
		return;
	}

	FILE *the_cache = NULL;
	if (entry->cache_filename && strncmp(entry->cache_filename, "gmem://", 7))
		the_cache = gf_fopen ( entry->cache_filename, "rb" );

	if ( the_cache ) {
		char * endPtr;
		const char * keyValue = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_INWRITE);
		if (keyValue) {
			entry->cacheSize = 0;
			entry->flags |= IN_PROGRESS;
			gf_fclose ( the_cache );
			return;
		}
		keyValue = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CONTENT_SIZE );

		entry->cacheSize = ( u32 ) gf_fsize(the_cache);
		gf_fclose ( the_cache );
		if (keyValue) {
			entry->contentLength = (u32) strtoul( keyValue, &endPtr, 10);
			if (*endPtr!='\0' || entry->contentLength != entry->cacheSize) {
				entry->flags |= CORRUPTED;
				GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Corrupted: file and cache info size mismatch\n"));
			}
		} else {
			entry->flags |= CORRUPTED;
			GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Corrupted: missing cache content size\n"));
		}
	} else if (!entry->mem_storage) {
		entry->flags |= CORRUPTED;
	}
}

#define _CACHE_HASH_SIZE 20
#define _CACHE_MAX_EXTENSION_SIZE 6
static const char * default_cache_file_suffix = ".dat";
static const char * cache_file_info_suffix = ".txt";

DownloadedCacheEntry gf_cache_create_entry(const char * cache_directory, const char * url , u64 start_range, u64 end_range, Bool mem_storage, GF_Mutex *mx)
{
	char tmp[GF_MAX_PATH];
	u8 hash[_CACHE_HASH_SIZE];
	int sz;
	char ext[_CACHE_MAX_EXTENSION_SIZE];
	DownloadedCacheEntry entry = NULL;
	if (!url || !cache_directory) return NULL;

	sz = (u32) strlen ( url );
	if ( sz > GF_MAX_PATH ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE,
		       ("[CACHE] ERROR, URL is too long (%d chars), more than %d chars.\n", sz, GF_MAX_PATH ));
		return NULL;
	}
	tmp[0] = '\0';
	/*generate hash of the full url*/
	if (start_range && end_range) {
		sprintf(tmp, "%s_"LLD"-"LLD, url, start_range, end_range );
	} else {
		strcpy ( tmp, url );
	}
	gf_sha1_csum ((u8*) tmp, (u32) strlen ( tmp ), hash );
	tmp[0] = 0;
	{
		int i;
		for ( i=0; i<20; i++ )
		{
			char t[3];
			t[2] = 0;
			sprintf ( t, "%02X", hash[i] );
			strcat ( tmp, t );
		}
	}
	assert ( strlen ( tmp ) == (_CACHE_HASH_SIZE * 2) );

	GF_SAFEALLOC(entry, struct __DownloadedCacheEntryStruct);
	if ( !entry ) return NULL;

	entry->url = gf_strdup ( url );
	entry->hash = gf_strdup ( tmp );
	entry->memory_stored = mem_storage;
	entry->cacheSize = 0;
	entry->contentLength = 0;
	entry->serverETag = NULL;
	entry->diskETag = NULL;
	entry->flags = 0;
	entry->diskLastModified = NULL;
	entry->serverLastModified = NULL;
	entry->range_start = start_range;
	entry->range_end = end_range;

	entry->discard_on_delete = GF_FALSE;
	entry->write_session = NULL;
	entry->sessions = gf_list_new();

	if (entry->memory_stored) {
		entry->cache_filename = (char*)gf_malloc ( strlen ("gmem://") + 8 + strlen("@") + 16 + 1);
	} else {
		/* Sizeof cache directory + hash + possible extension */
		entry->cache_filename = (char*)gf_malloc ( strlen ( cache_directory ) + strlen(cache_file_prefix) + strlen(tmp) + _CACHE_MAX_EXTENSION_SIZE + 1);
	}

	if ( !entry->hash || !entry->url || !entry->cache_filename || !entry->sessions) {
		/* Probably out of memory */
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE, ("[CACHE] Failed to create cache entry, out of memory\n"));
		gf_cache_delete_entry ( entry );
		return NULL;
	}

	if (entry->memory_stored) {
		entry->cache_blob.mx = mx;
		entry->cache_blob.data = entry->mem_storage;
		entry->cache_blob.size = entry->contentLength;
		char *burl = gf_blob_register(&entry->cache_blob);
		if (burl) {
			strcpy(entry->cache_filename, burl);
			gf_free(burl);
		}
		return entry;
	}


	tmp[0] = '\0';
	strcpy ( entry->cache_filename, cache_directory );
	strcat( entry->cache_filename, cache_file_prefix );
	strcat ( entry->cache_filename, entry->hash );
	strcpy ( tmp, url );

	{
		char * parser;
		parser = strrchr ( tmp, '?' );
		if ( parser )
			parser[0] = '\0';
		parser = strrchr ( tmp, '#' );
		if ( parser )
			parser[0] = '\0';
		parser = strrchr ( tmp, '.' );
		if ( parser && ( strlen ( parser ) < _CACHE_MAX_EXTENSION_SIZE ) )
			strncpy(ext, parser, _CACHE_MAX_EXTENSION_SIZE);
		else
			strncpy(ext, default_cache_file_suffix, _CACHE_MAX_EXTENSION_SIZE);
		assert (strlen(ext));
		strcat( entry->cache_filename, ext);
	}

	gf_dynstrcat(&entry->cfg_filename, entry->cache_filename, NULL);
	gf_dynstrcat(&entry->cfg_filename, cache_file_info_suffix, NULL);

	char szLOCK[GF_MAX_PATH];
	sprintf(szLOCK, "%s.lock", entry->cfg_filename);
	GF_LockStatus lock_type = cache_entry_lock(szLOCK);
	if (!lock_type) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to grab cache lock for entry %s, request will not be cached\n", url));
		gf_cache_delete_entry ( entry );
		return NULL;
	}

	Bool in_cache = gf_file_exists(entry->cfg_filename);

	GF_Config *cfg = gf_cfg_force_new ( NULL, entry->cfg_filename);
	if ( !cfg ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE, ("[CACHE] Failed to create cache entry for %s, request will not be cached\n", url));
		gf_cache_delete_entry ( entry );
		if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);
		return NULL;
	}

	if (in_cache) {
		gf_cache_check_if_cache_file_is_corrupted(entry, url, cfg);
		if ((entry->flags & CORRUPTED) && (gf_cfg_get_key_count(cfg, CACHE_SECTION_USERS)==0)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Cache entry for %s corrupted but no other users, recreating\n", url));
			entry->flags &= ~CORRUPTED;
			gf_file_delete(entry->cache_filename);
			gf_file_delete(entry->cfg_filename);
			gf_cfg_del_section(cfg, CACHE_SECTION_NAME);
			gf_cfg_del_section(cfg, CACHE_SECTION_USERS);
		}
	}

	if (entry->flags & CORRUPTED) {
		gf_cfg_del(cfg);
		GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Incompatible cache entry for %s, request will not be cached\n", url));
		gf_cache_delete_entry ( entry );
		if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);
		return NULL;
	}

	gf_cache_set_etag_on_disk(entry, gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_ETAG));
	gf_cache_set_etag_on_server(entry, gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_ETAG));
	gf_cache_set_mime_type(entry, gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_MIME_TYPE));
	gf_cache_set_last_modified_on_disk(entry, gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_LAST_MODIFIED));
	gf_cache_set_last_modified_on_server(entry, gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_LAST_MODIFIED));

	const char *opt = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_MAXAGE);
	if (opt) sscanf(opt, LLU, &entry->max_age);
	opt = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_MUST_REVALIDATE);
	if (opt && !strcmp(opt, "true")) entry->must_revalidate = GF_TRUE;

	char szBUF[20];
	sprintf(szBUF, LLU, gf_net_get_utc()/1000);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_LAST_HIT, szBUF);

	u32 nb_hits=0;
	opt = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_NUM_HIT );
	if (opt) nb_hits = atoi(opt);
	nb_hits++;
	sprintf(szBUF, "%u", nb_hits);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_NUM_HIT, szBUF);

	if (!mem_storage) {
		char szID[20];
		if (cache_cleanup_processes(cfg))
			entry->other_in_use = GF_TRUE;
		sprintf(szID, "%u", gf_sys_get_process_id());
		gf_cfg_set_key(cfg, CACHE_SECTION_USERS, szID, "true");
	}

	//rewrite to disk
	gf_cfg_del(cfg);
	if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);

	return entry;
}

GF_Err gf_cache_set_content_length( const DownloadedCacheEntry entry, u32 length )
{
	CHECK_ENTRY;
	if (entry->continue_file) {
		entry->contentLength = entry->previousRangeContentLength + length;
	} else {
		entry->contentLength = length;
	}
	return GF_OK;
}

u32 gf_cache_get_content_length( const DownloadedCacheEntry entry)
{
	if (!entry) return 0;
	if (entry->external_blob) {
		return entry->external_blob->size;
	}
	return entry->contentLength;
}

GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success )
{
	GF_Err e = GF_OK;
	CHECK_ENTRY;
	if (!sess || !entry->write_session || entry->write_session != sess)
		return GF_OK;
	gf_assert( sess == entry->write_session );
	if (entry->writeFilePtr) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CACHE,
		       ("[CACHE] Closing file %s, %d bytes written.\n", entry->cache_filename, entry->written_in_cache));

		if (gf_fflush( entry->writeFilePtr ) || gf_fclose( entry->writeFilePtr )) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to flush/close file on disk\n"));
			e = GF_IO_ERR;
		}
		if (!e && success) {
			e = gf_cache_set_last_modified_on_disk( entry, gf_cache_get_last_modified_on_server(entry));
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to set last-modified\n"));
			}
			if (!e) {
				e = gf_cache_set_etag_on_disk( entry, gf_cache_get_etag_on_server(entry));
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to set etag\n"));
				}
			}
		}

		e = gf_cache_flush_disk_cache(entry, success);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to flush cache entry on disk after etag/last-modified\n"));
		}

#if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500
		/* On  UNIX, be sure to flush all the data */
		sync();
#endif
		entry->writeFilePtr = NULL;
		if (GF_OK != e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to fully write file on cache, e=%d\n", e));
		}

		if (!success) {
			entry->discard_on_delete = GF_TRUE;
		}
	}
	entry->write_session = NULL;
	return e;
}

GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess)
{
	CHECK_ENTRY;
	if (!sess)
		return GF_BAD_PARAM;
	entry->write_session = sess;
	if (!entry->continue_file) {
		gf_assert( ! entry->writeFilePtr);

		entry->written_in_cache = 0;
	}
	entry->flags &= ~CORRUPTED;

	if (entry->memory_stored) {
		gf_mx_p(entry->cache_blob.mx);

		GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Opening cache file %s for write (%s)...\n", entry->cache_filename, entry->url));
		if (!entry->mem_allocated || (entry->mem_allocated < entry->contentLength)) {
			if (entry->contentLength) entry->mem_allocated = entry->contentLength;
			else if (!entry->mem_allocated) entry->mem_allocated = 81920;
			entry->mem_storage = (u8*)gf_realloc(entry->mem_storage, sizeof(char)* (entry->mem_allocated + 2) );
		}
		entry->cache_blob.data = entry->mem_storage;
		entry->cache_blob.size = entry->contentLength;
		char *burl = gf_blob_register(&entry->cache_blob);
		if (burl) {
			strcpy(entry->cache_filename, burl);
			gf_free(burl);
		}
		gf_mx_v(entry->cache_blob.mx);

		if (!entry->mem_allocated) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] Failed to create memory storage for file %s\n", entry->url));
			return GF_OUT_OF_MEM;
		}
		return GF_OK;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Opening cache file %s for write (%s)...\n", entry->cache_filename, entry->url));
	entry->writeFilePtr = gf_fopen(entry->cache_filename, entry->continue_file ? "a+b" : "wb");
	if (!entry->writeFilePtr) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE,
		       ("[CACHE] Error while opening cache file %s for writting.\n", entry->cache_filename));
		entry->write_session = NULL;
		return GF_IO_ERR;
	}

	if (entry->continue_file )
		gf_fseek(entry->writeFilePtr, 0, SEEK_END);

	char szLOCK[GF_MAX_PATH];
	sprintf(szLOCK, "%s.lock", entry->cfg_filename);
	GF_LockStatus lock_type = cache_entry_lock(szLOCK);
	GF_Config *cfg = gf_cfg_new(NULL, entry->cfg_filename);
	char szUTC[20];
	sprintf(szUTC, LLU, gf_net_get_utc()/1000);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CREATED, szUTC);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_INWRITE, "true");
	gf_cfg_del(cfg);
	if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);

	return GF_OK;
}

GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size, GF_Mutex *mx) {
	u32 read;
	CHECK_ENTRY;

	if (!data || (!entry->writeFilePtr && !entry->mem_storage) || sess != entry->write_session) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE, ("Incorrect parameter : data=%p, writeFilePtr=%p mem_storage=%p at "__FILE__"\n", data, entry->writeFilePtr, entry->mem_storage));
		return GF_BAD_PARAM;
	}

	if (entry->memory_stored) {
		if (!entry->cache_blob.mx)
			entry->cache_blob.mx = mx;
		gf_assert(mx);
		gf_mx_p(entry->cache_blob.mx);
		if (entry->written_in_cache + size > entry->mem_allocated) {
			u32 new_size = MAX(entry->mem_allocated*2, entry->written_in_cache + size);
			entry->mem_storage = (u8*)gf_realloc(entry->mem_storage, (new_size+2));
			entry->mem_allocated = new_size;
			entry->cache_blob.data = entry->mem_storage;
			entry->cache_blob.size = entry->contentLength;
			char *burl = gf_blob_register(&entry->cache_blob);
			if (burl) {
				strcpy(entry->cache_filename, burl);
				gf_free(burl);
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Reallocating memory cache to %d bytes\n", new_size));
		}
		memcpy(entry->mem_storage + entry->written_in_cache, data, size);
		entry->written_in_cache += size;
		memset(entry->mem_storage + entry->written_in_cache, 0, 2);
		entry->cache_blob.size = entry->written_in_cache;
		gf_mx_v(entry->cache_blob.mx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Storing %d bytes to memory\n", size));
		return GF_OK;
	}

	read = (u32) gf_fwrite(data, size, entry->writeFilePtr);
	if (read > 0)
		entry->written_in_cache+= read;
	if (read != size) {
		/* Something bad happened */
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE,
		       ("[CACHE] Error while writting %d bytes of data to cache : has written only %d bytes.", size, read));
		gf_cache_close_write_cache(entry, sess, GF_FALSE);
		return GF_IO_ERR;
	}
	if (gf_fflush(entry->writeFilePtr)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE,
		       ("[CACHE] Error while flushing data bytes to cache file : %s.", entry->cache_filename));
		gf_cache_close_write_cache(entry, sess, GF_FALSE);
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Writing %d bytes to cache\n", size));
	return GF_OK;
}

void gf_cache_delete_entry( const DownloadedCacheEntry entry )
{
	if ( !entry ) return;

	if (entry->writeFilePtr) {
		/** Cache should have been close before, abornormal situation */
		GF_LOG(GF_LOG_WARNING, GF_LOG_CACHE, ("[CACHE] Cache for %s has not been closed properly\n", entry->url));
		gf_fclose(entry->writeFilePtr);
	}
	gf_blob_unregister(&entry->cache_blob);
	if (entry->external_blob) {
		gf_blob_unregister(entry->external_blob);
	}

	if (!entry->mem_storage ) {
		char szLOCK[GF_MAX_PATH];
		sprintf(szLOCK, "%s.lock", entry->cfg_filename);
		GF_LockStatus lock_type = cache_entry_lock(szLOCK);

		GF_Config *cfg = gf_cfg_new(NULL, entry->cfg_filename);
		if (cfg) {
			char szID[20];
			sprintf(szID, "%u", gf_sys_get_process_id());
			gf_cfg_set_key(cfg, CACHE_SECTION_USERS, szID, NULL);

			Bool still_in_use = cache_cleanup_processes(cfg);
			if (still_in_use && entry->discard_on_delete) {
				gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_TODELETE, "true");
				entry->discard_on_delete = GF_FALSE;
			} else if (!still_in_use) {
				if (gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_TODELETE)) {
					gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_TODELETE, NULL);
					entry->discard_on_delete = GF_TRUE;
				}
			}
			gf_cfg_del(cfg);
		}
		if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);

		if (entry->discard_on_delete) {
			gf_file_delete(entry->cache_filename);
			gf_file_delete(entry->cfg_filename);
		}
    }
	if (entry->cfg_filename) gf_free(entry->cfg_filename);
	if (entry->cache_filename) gf_free(entry->cache_filename);
	if (entry->serverETag) gf_free(entry->serverETag);
	if (entry->diskETag) gf_free(entry->diskETag);
	if (entry->serverLastModified) gf_free(entry->serverLastModified);
	if (entry->diskLastModified) gf_free(entry->diskLastModified);
	if (entry->hash) gf_free(entry->hash);
	if (entry->url) gf_free(entry->url);
	if (entry->mimeType) gf_free(entry->mimeType);
	if (entry->mem_storage && entry->mem_allocated) gf_free(entry->mem_storage);
	if (entry->forced_headers) gf_free(entry->forced_headers);


	if (entry->sessions) {
		gf_assert( gf_list_count(entry->sessions) == 0);
		gf_list_del(entry->sessions);
	}
	gf_free (entry);
}


s32 gf_cache_remove_session_from_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess) {
	u32 i;
	s32 count;
	if (!entry || !sess || !entry->sessions)
		return -1;
	count = gf_list_count(entry->sessions);
	for (i = 0 ; i < (u32)count; i++) {
		GF_DownloadSession *s = (GF_DownloadSession*)gf_list_get(entry->sessions, i);
		if (s == sess) {
			gf_list_rem(entry->sessions, i);
			count --;
			break;
		}
	}
	if (entry->write_session == sess) {
		/* OK, this is not optimal to close it since we are in a mutex,
		* but we don't want to risk to have another session opening
		* a not fully closed cache entry */
		if (entry->writeFilePtr) {
			if (gf_fclose(entry->writeFilePtr)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CACHE] gf_cache_remove_session_from_cache_entry:%d, Failed to properly close cache file '%s' of url '%s', cache may be corrupted !\n", __LINE__, entry->cache_filename, entry->url));
			}
		}
		entry->writeFilePtr = NULL;
		entry->write_session = NULL;
	}
	return count;
}

void gf_cache_set_max_age(const DownloadedCacheEntry entry, u32 max_age, Bool must_revalidate)
{
	char szVal[100];
	if (!entry || entry->mem_storage) return;

	char szLOCK[GF_MAX_PATH];
	sprintf(szLOCK, "%s.lock", entry->cfg_filename);
	GF_LockStatus lock_type = cache_entry_lock(szLOCK);
	GF_Config *cfg = gf_cfg_new(NULL, entry->cfg_filename);

	entry->must_revalidate = must_revalidate;
	if (max_age)
		entry->max_age = gf_net_get_utc()/1000 + max_age;
	sprintf(szVal, LLU, entry->max_age);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_MAXAGE, szVal);
	gf_cfg_set_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_MUST_REVALIDATE, must_revalidate ? "true" : "false");
	gf_cfg_del(cfg);

	if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);
}

static u32 gf_cache_get_sessions_count_for_cache_entry(const DownloadedCacheEntry entry)
{
	if (!entry)
		return 0;
	return gf_list_count(entry->sessions);
}


s32 gf_cache_add_session_to_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess) {
	u32 i;
	s32 count;
	if (!entry || !sess || !entry->sessions)
		return -1;
	count = gf_list_count(entry->sessions);
	for (i = 0 ; i < (u32)count; i++) {
		GF_DownloadSession *s = (GF_DownloadSession*)gf_list_get(entry->sessions, i);
		if (s == sess) {
			return count;
		}
	}
	gf_list_add(entry->sessions, sess);
	return count + 1;
}

void gf_cache_set_end_range(DownloadedCacheEntry entry, u64 range_end)
{
	if (entry) {
		entry->previousRangeContentLength = entry->contentLength;
		entry->range_end = range_end;
		entry->continue_file = GF_TRUE;
	}
}

Bool gf_cache_is_in_progress(const DownloadedCacheEntry entry)
{
	if (!entry) return GF_FALSE;
	if (entry->writeFilePtr) return GF_TRUE;
	if (entry->mem_storage && entry->written_in_cache && entry->contentLength && (entry->written_in_cache<entry->contentLength))
		return GF_TRUE;
	return GF_FALSE;
}

Bool gf_cache_set_mime(const DownloadedCacheEntry entry, const char *mime)
{
	if (!entry || !entry->memory_stored) return GF_FALSE;
	if (entry->mimeType) gf_free(entry->mimeType);
	entry->mimeType = gf_strdup(mime);
	return GF_TRUE;
}

Bool gf_cache_set_range(const DownloadedCacheEntry entry, u64 size, u64 start_range, u64 end_range)
{
	if (!entry || !entry->memory_stored) return GF_FALSE;
	entry->range_start = start_range;
	entry->range_end = end_range;
	entry->contentLength = (u32) size;
	entry->continue_file = GF_FALSE;
	return GF_TRUE;
}

Bool gf_cache_set_headers(const DownloadedCacheEntry entry, const char *headers)
{
	if (!entry || !entry->memory_stored) return GF_FALSE;
	if (entry->forced_headers) gf_free(entry->forced_headers);
	entry->forced_headers = headers ? gf_strdup(headers) : NULL;
	return GF_TRUE;
}

char *gf_cache_get_forced_headers(const DownloadedCacheEntry entry)
{
	if (!entry) return NULL;
	return entry->forced_headers;
}
void gf_cache_set_downtime(const DownloadedCacheEntry entry, u32 download_time_ms)
{
	if (entry) entry->downtime = download_time_ms;
}
u32 gf_cache_get_downtime(const DownloadedCacheEntry entry)
{
	if (!entry) return 0;
	return entry->downtime;
}
u32 gf_cache_is_done(const DownloadedCacheEntry entry)
{
    if (!entry) return 1;
    u32 res = 1;
    if (entry->external_blob) {
        gf_mx_p(entry->external_blob->mx);
        res = (entry->external_blob->flags & GF_BLOB_IN_TRANSFER) ? 0 : 1;
        if (res && (entry->external_blob->flags & GF_BLOB_CORRUPTED))
			res = 2;
        gf_mx_v(entry->external_blob->mx);
    } else if (entry->mem_storage) {
        res = (entry->cache_blob.flags & GF_BLOB_IN_TRANSFER) ? 0 : 1;
    } else {
		if (!(entry->flags & IN_PROGRESS)) return 1;
		char szLOCK[GF_MAX_PATH];
		sprintf(szLOCK, "%s.lock", entry->cfg_filename);
		GF_LockStatus lock_type = cache_entry_lock(szLOCK);
		GF_Config *cfg = gf_cfg_new(NULL, entry->cfg_filename);
		const char *opt = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_INWRITE);
		res = opt ? 0 : 1;
		if (res) {
			entry->flags &= ~IN_PROGRESS;
			opt = gf_cfg_get_key(cfg, CACHE_SECTION_NAME, CACHE_SECTION_KEY_CONTENT_SIZE);
			entry->contentLength = atoi(opt);
		}
		gf_cfg_del(cfg);
		if (lock_type==GF_LOCKFILE_NEW) gf_file_delete(szLOCK);
	}
    return res;
}
const u8 *gf_cache_get_content(const DownloadedCacheEntry entry, u32 *size, u32 *max_valid_size, Bool *was_modified)
{
    if (!entry) return NULL;
	*was_modified = GF_FALSE;
    if (entry->external_blob) {
        u8 *data;
		GF_Err e = gf_blob_get_ex(entry->external_blob, &data, size, NULL);
        if (e) return NULL;
        *max_valid_size = *size;
		if (entry->external_blob->range_valid) {
			gf_mx_p(entry->external_blob->mx);
			entry->external_blob->range_valid(entry->external_blob, 0, max_valid_size);
			gf_mx_v(entry->external_blob->mx);
		}
		if (entry->external_blob->last_modification_time != entry->cache_blob.last_modification_time) {
			*was_modified = GF_TRUE;
			entry->cache_blob.last_modification_time = entry->external_blob->last_modification_time;
		}
        return data;
    }
    *max_valid_size = *size = entry->cache_blob.size;
    return entry->cache_blob.data;
}
void gf_cache_release_content(const DownloadedCacheEntry entry)
{
    if (!entry) return;
    if (!entry->external_blob) return;
    gf_blob_release_ex(entry->external_blob);
}
Bool gf_cache_is_deleted(const DownloadedCacheEntry entry)
{
    if (!entry) return GF_TRUE;
    if (entry->flags & DELETED) return GF_TRUE;
    return GF_FALSE;
}

Bool gf_cache_set_content(const DownloadedCacheEntry entry, GF_Blob *blob, Bool copy, GF_Mutex *mx)
{
	if (!entry || !entry->memory_stored) return GF_FALSE;

    if (!blob) {
        entry->flags |= DELETED;
        if (entry->external_blob) {
			gf_blob_unregister(entry->external_blob);
			entry->external_blob = NULL;
		}
        return GF_TRUE;
    }
    if (blob->mx)
        gf_mx_p(blob->mx);
	
    if (!copy) {
        if (entry->mem_allocated) gf_free(entry->mem_storage);
		entry->mem_storage = (u8 *) blob->data;
        if (!entry->written_in_cache) {
			char *burl = gf_blob_register(blob);
			if (burl) {
				strcpy(entry->cache_filename, burl);
				gf_free(burl);
			}
		}

		entry->written_in_cache = blob->size;
		entry->mem_allocated = 0;
		entry->cache_blob.data = NULL;
		entry->cache_blob.size = 0;
        entry->external_blob = blob;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Storing %d bytes to memory from external module\n", blob->size));
    } else {
		if (!entry->cache_blob.mx)
			entry->cache_blob.mx = mx;
		gf_mx_p(entry->cache_blob.mx);

        if (blob->size >= entry->mem_allocated) {
            u32 new_size;
            new_size = MAX(entry->mem_allocated*2, blob->size+1);
            entry->mem_storage = (u8*)gf_realloc(entry->mem_allocated ? entry->mem_storage : NULL, (new_size+2));
            entry->mem_allocated = new_size;
            entry->cache_blob.data = entry->mem_storage;
            entry->cache_blob.size = entry->contentLength;
            if (!entry->written_in_cache) {
				char *burl = gf_blob_register(&entry->cache_blob);
				if (burl) {
					strcpy(entry->cache_filename, burl);
					gf_free(burl);
				}
            }
            GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Reallocating memory cache to %d bytes\n", new_size));
        }
        memcpy(entry->mem_storage, blob->data, blob->size);
        entry->mem_storage[blob->size] = 0;
        entry->cache_blob.size = entry->written_in_cache = blob->size;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Storing %d bytes to cache memory\n", blob->size));

		gf_mx_v(entry->cache_blob.mx);

		entry->cache_blob.flags = blob->flags;
    }
	if (blob->flags & GF_BLOB_IN_TRANSFER)
		entry->contentLength = 0;
	else
		entry->contentLength = blob->size;

    if (blob->mx)
        gf_mx_v(blob->mx);

    return GF_TRUE;
}

static Bool gf_cache_entry_can_reuse(const DownloadedCacheEntry entry, Bool skip_revalidate)
{
	if (!entry) return GF_FALSE;
	//if corrupted or deleted, we cannot reuse
	if (entry->flags & (CORRUPTED|DELETED) ) return GF_FALSE;
	if (!entry->max_age) return GF_FALSE;
	s64 expires = entry->max_age;
	expires -= gf_net_get_utc()/1000;
	//max age not reached, allow reuse
	if (expires>0)
		return GF_TRUE;
	if (entry->must_revalidate)
		return GF_FALSE;
	//we consider that if not expired and no must-revalidate, we can go on
	return GF_TRUE;
}

Bool gf_cache_entry_is_shared(const DownloadedCacheEntry entry)
{
	return entry ? entry->other_in_use : GF_FALSE;
}

Bool gf_cache_is_mem(const DownloadedCacheEntry entry)
{
	return entry->mem_storage ? GF_TRUE : GF_FALSE;
}
FILE *gf_cache_open_read(const DownloadedCacheEntry entry)
{
	if (!entry) return NULL;
	if (entry->mem_storage) return NULL;
	return gf_fopen(entry->cache_filename, "r");
}


/*!
 * Finds an existing entry in the cache for a given URL
\param sess The session configured with the URL
\return NULL if none found, the DownloadedCacheEntry otherwise
 */
static DownloadedCacheEntry gf_cache_find_entry_by_url(GF_DownloadSession * sess)
{
	u32 i, count;
	gf_assert( sess && sess->dm && sess->dm->cache_entries );
	gf_mx_p( sess->dm->cache_mx );
	count = gf_list_count(sess->dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * url;
		DownloadedCacheEntry e = (DownloadedCacheEntry)gf_list_get(sess->dm->cache_entries, i);
		gf_assert(e);
		url = gf_cache_get_url(e);
		gf_assert( url );

		if (!strncmp(url, "http://gmcast/", 14)) {
			char *sep_1 = strchr(url+14, '/');
			char *sep_2 = strchr(sess->orig_url+14, '/');
			if (!sep_1 || !sep_2 || strcmp(sep_1, sep_2))
				continue;
		} else if (strcmp(url, sess->orig_url)) continue;

		if (! sess->is_range_continuation) {
			if (sess->range_start != gf_cache_get_start_range(e)) continue;
			if (sess->range_end != gf_cache_get_end_range(e)) continue;
		}
		/*OK that's ours*/
		gf_mx_v( sess->dm->cache_mx );
		return e;
	}
	gf_mx_v( sess->dm->cache_mx );
	return NULL;
}


/**
 * Removes a cache entry from cache and performs a cleanup if possible.
 * If the cache entry is marked for deletion and has no sessions associated with it, it will be
 * removed (so some modules using a streaming like cache will still work).
 */
void gf_cache_remove_entry_from_session(GF_DownloadSession * sess)
{
	if (!sess || !sess->cache_entry) return;

	gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
	if (sess->dm
			/*JLF - not sure what the rationale of this test is, and it prevents cleanup of cache entry
			which then results to crash when restarting the session (entry->writeFilePtr is not set back to NULL)*/
			&& sess->cache_entry->discard_on_delete

			&& (0 == gf_cache_get_sessions_count_for_cache_entry(sess->cache_entry)))
	{
		u32 i, count;
		gf_mx_p( sess->dm->cache_mx );
		count = gf_list_count( sess->dm->cache_entries );
		for (i = 0; i < count; i++) {
			DownloadedCacheEntry ex = (DownloadedCacheEntry)gf_list_get(sess->dm->cache_entries, i);
			if (ex == sess->cache_entry) {
				gf_list_rem(sess->dm->cache_entries, i);
				gf_cache_delete_entry( sess->cache_entry );
				sess->cache_entry = NULL;
				break;
			}
		}
		gf_mx_v( sess->dm->cache_mx );
	}

}


void gf_dm_configure_cache(GF_DownloadSession *sess)
{
	DownloadedCacheEntry entry;
	gf_cache_remove_entry_from_session(sess);

	//session is not cached and we don't cache the first URL
	if ((sess->flags & GF_NETIO_SESSION_NOT_CACHED) && !(sess->flags & GF_NETIO_SESSION_KEEP_FIRST_CACHE))  {
		sess->reused_cache_entry = GF_FALSE;
		if (sess->cache_entry)
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);

		sess->cache_entry = NULL;
		return;
	}

	Bool found = GF_FALSE;
	u32 i, count;
	entry = gf_cache_find_entry_by_url(sess);
	if (!entry) {
		if (sess->local_cache_only) {
			sess->cache_entry = NULL;
			SET_LAST_ERR(GF_URL_ERROR)
			return;
		}
		/* We found the existing session */
		if (sess->cache_entry) {
			Bool delete_cache = GF_TRUE;

			if (sess->flags & GF_NETIO_SESSION_KEEP_CACHE) {
				delete_cache = GF_FALSE;
			}
			if (gf_cache_entry_persistent(sess->cache_entry))
				delete_cache = GF_FALSE;

			/*! indicate we can destroy file upon destruction, except if disabled at session level*/
			if (delete_cache)
				gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);

			if (!gf_cache_entry_persistent(sess->cache_entry)
				&& !gf_cache_get_sessions_count_for_cache_entry(sess->cache_entry)
			) {
				gf_mx_p( sess->dm->cache_mx );
				/* No session attached anymore... we can delete it */
				gf_list_del_item(sess->dm->cache_entries, sess->cache_entry);
				gf_mx_v( sess->dm->cache_mx );
				gf_cache_delete_entry(sess->cache_entry);
			}
			sess->cache_entry = NULL;
		}
		Bool use_mem = (sess->flags & (GF_NETIO_SESSION_MEMORY_CACHE | GF_NETIO_SESSION_NO_STORE)) ? GF_TRUE : GF_FALSE;
		entry = gf_cache_create_entry(sess->dm->cache_directory, sess->orig_url, sess->range_start, sess->range_end, use_mem, sess->dm->cache_mx);
		if (!entry) {
			SET_LAST_ERR(GF_OUT_OF_MEM)
			return;
		}
		gf_mx_p( sess->dm->cache_mx );
		gf_list_add(sess->dm->cache_entries, entry);
		gf_mx_v( sess->dm->cache_mx );
		sess->is_range_continuation = GF_FALSE;
	}
	sess->cache_entry = entry;
	sess->reused_cache_entry = 	gf_cache_is_in_progress(entry);
	gf_mx_p(sess->dm->cache_mx);
	count = gf_list_count(sess->dm->all_sessions);
	for (i=0; i<count; i++) {
		GF_DownloadSession *a_sess = (GF_DownloadSession*)gf_list_get(sess->dm->all_sessions, i);
		gf_assert(a_sess);
		if (a_sess==sess) continue;
		if (a_sess->cache_entry==entry) {
			found = GF_TRUE;
			break;
		}
	}
	gf_mx_v(sess->dm->cache_mx);
	if (!found) {
		sess->reused_cache_entry = GF_FALSE;
		if (sess->cache_entry)
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
	}
	gf_cache_add_session_to_cache_entry(sess->cache_entry, sess);
	if (sess->needs_range)
		gf_cache_set_range(sess->cache_entry, 0, sess->range_start, sess->range_end);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CACHE] Cache for %s setup %s\n", sess->orig_url, gf_cache_get_cache_filename(sess->cache_entry)));

	Bool no_revalidate = GF_FALSE;
	if (sess->allow_direct_reuse)
		no_revalidate = GF_TRUE;

	if (sess->cache_entry) {
		if (sess->flags & GF_NETIO_SESSION_KEEP_FIRST_CACHE) {
			sess->flags &= ~GF_NETIO_SESSION_KEEP_FIRST_CACHE;
			gf_cache_entry_set_persistent(sess->cache_entry);
		}
		//this one is only used for init segments in dash/hls
		if ((sess->flags & GF_NETIO_SESSION_MEMORY_CACHE) && (sess->flags & GF_NETIO_SESSION_KEEP_CACHE) ) {
			gf_cache_entry_set_persistent(sess->cache_entry);
		}

		if (gf_cache_entry_can_reuse(sess->cache_entry, sess->dm->allow_offline_cache)) {
			no_revalidate = GF_TRUE;
		}
		//we cannot reuse and others are using the cache file - for now we don't have any other possibility
		//than disabling the cache to avoid overwriting the resource
		else if (gf_cache_entry_is_shared(sess->cache_entry)) {
			gf_cache_remove_entry_from_session(sess);
			sess->cache_entry = NULL;
			sess->reused_cache_entry = GF_FALSE;
			return;
		}
	}

	if (no_revalidate)
		sess->cached_file = gf_cache_open_read(sess->cache_entry);

	if (sess->cached_file) {
		sess->connect_time = 0;
		sess->status = GF_NETIO_CONNECTED;
		const char *mime = gf_cache_get_mime_type(sess->cache_entry);
		if (mime)
			gf_dm_sess_set_header_ex(sess, "Content-Type", mime, GF_TRUE);
		u32 size = gf_cache_get_content_length(sess->cache_entry);
		if (size) {
			char szHdr[20];
			sprintf(szHdr, "%u", size);
			gf_dm_sess_set_header_ex(sess, "Content-Length", szHdr, GF_TRUE);
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] using existing cache entry\n", sess->log_name));
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
	}
}

void gf_dm_delete_cached_file_entry(const GF_DownloadManager * dm,  const char * url)
{
	GF_Err e;
	u32 count, i;
	char * realURL;
	GF_URL_Info info;
	if (!url || !dm)
		return;
	gf_mx_p( dm->cache_mx );
	gf_dm_url_info_init(&info);
	e = gf_dm_get_url_info(url, &info, NULL);
	if (e != GF_OK) {
		gf_mx_v( dm->cache_mx );
		gf_dm_url_info_del(&info);
		return;
	}
	realURL = gf_strdup(info.canonicalRepresentation);
	gf_dm_url_info_del(&info);
	gf_assert( realURL );
	count = gf_list_count(dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * e_url;
		DownloadedCacheEntry cache_ent = (DownloadedCacheEntry)gf_list_get(dm->cache_entries, i);
		gf_assert(cache_ent);
		e_url = gf_cache_get_url(cache_ent);
		gf_assert( e_url );
		if (!strcmp(e_url, realURL)) {
			/* We found the existing session */
			gf_cache_entry_set_delete_files_when_deleted(cache_ent);
			if (0 == gf_cache_get_sessions_count_for_cache_entry( cache_ent )) {
				/* No session attached anymore... we can delete it */
				gf_list_rem(dm->cache_entries, i);
				gf_cache_delete_entry(cache_ent);
			}
			/* If deleted or not, we don't search further */
			gf_mx_v( dm->cache_mx );
			gf_free(realURL);
			return;
		}
	}
	/* If we are here it means we did not found this URL in cache */
	gf_mx_v( dm->cache_mx );
	gf_free(realURL);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CACHE] Cannot find URL %s, cache file won't be deleted.\n", url));
}

GF_EXPORT
void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess,  const char * url, Bool force)
{
	if (sess && sess->dm && url) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[CACHE] Removing cache entry for %s\n", url));
		gf_dm_delete_cached_file_entry(sess->dm, url);
		if (sess->local_cache_only && sess->dm->local_cache_url_provider_cbk)
			sess->dm->local_cache_url_provider_cbk(sess->dm->lc_udta, (char *) url, GF_TRUE);
	}
}

GF_EXPORT
GF_Err gf_dm_set_localcache_provider(GF_DownloadManager *dm, Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool is_cache_destroy), void *lc_udta)
{
	if (!dm)
		return GF_BAD_PARAM;
	dm->local_cache_url_provider_cbk = local_cache_url_provider_cbk;
	dm->lc_udta = lc_udta;
	return GF_OK;

}

GF_EXPORT
DownloadedCacheEntry gf_dm_add_cache_entry(GF_DownloadManager *dm, const char *szURL, GF_Blob *blob, u64 start_range, u64 end_range, const char *mime, Bool clone_memory, u32 download_time_ms)
{
	u32 i, count;
	DownloadedCacheEntry the_entry = NULL;

	gf_mx_p(dm->cache_mx );
	if (blob)
		GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[CACHE] Pushing %s to cache "LLU" bytes (done %s)\n", szURL, blob->size, (blob->flags & GF_BLOB_IN_TRANSFER) ? "no" : "yes"));
	count = gf_list_count(dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * url;
		DownloadedCacheEntry e = (DownloadedCacheEntry)gf_list_get(dm->cache_entries, i);
		gf_assert(e);
		url = gf_cache_get_url(e);
		gf_assert( url );
		if (strcmp(url, szURL)) continue;

		if (end_range) {
			if (start_range != gf_cache_get_start_range(e)) continue;
			if (end_range != gf_cache_get_end_range(e)) continue;
		}
		/*OK that's ours*/
		the_entry = e;
		break;
	}
	if (!the_entry) {
		the_entry = gf_cache_create_entry("", szURL, 0, 0, GF_TRUE, dm->cache_mx);
		if (!the_entry) {
			gf_mx_v(dm->cache_mx );
			return NULL;
		}
		gf_list_add(dm->cache_entries, the_entry);
	}

	gf_cache_set_mime(the_entry, mime);
	if (blob && ! (blob->flags & GF_BLOB_IN_TRANSFER))
		gf_cache_set_range(the_entry, blob->size, start_range, end_range);

	gf_cache_set_content(the_entry, blob, clone_memory ? GF_TRUE : GF_FALSE, dm->cache_mx);
	gf_cache_set_downtime(the_entry, download_time_ms);
	gf_mx_v(dm->cache_mx );
	return the_entry;
}

void gf_dm_sess_reload_cached_headers(GF_DownloadSession *sess)
{
	char *hdrs;

	if (!sess || !sess->local_cache_only) return;

	hdrs = gf_cache_get_forced_headers(sess->cache_entry);

	gf_dm_sess_clear_headers(sess);
	while (hdrs) {
		char *sep2, *sepL = strstr(hdrs, "\r\n");
		if (sepL) sepL[0] = 0;
		sep2 = strchr(hdrs, ':');
		if (sep2) {
			GF_HTTPHeader *hdr;
			GF_SAFEALLOC(hdr, GF_HTTPHeader);
			if (!hdr) break;
			sep2[0]=0;
			hdr->name = gf_strdup(hdrs);
			sep2[0]=':';
			sep2++;
			while (sep2[0]==' ') sep2++;
			hdr->value = gf_strdup(sep2);
			gf_list_add(sess->headers, hdr);
		}
		if (!sepL) break;
		sepL[0] = '\r';
		hdrs = sepL + 2;
	}
}
GF_EXPORT
GF_Err gf_dm_force_headers(GF_DownloadManager *dm, const DownloadedCacheEntry entry, const char *headers)
{
	u32 i, count;
	Bool res;
	if (!entry)
		return GF_BAD_PARAM;
	gf_mx_p(dm->cache_mx);
	res = gf_cache_set_headers(entry, headers);
	count = gf_list_count(dm->all_sessions);
	for (i=0; i<count; i++) {
		GF_DownloadSession *sess = gf_list_get(dm->all_sessions, i);
		if (sess->cache_entry != entry) continue;
		gf_dm_sess_reload_cached_headers(sess);
	}

	gf_mx_v(dm->cache_mx);
	if (res) return GF_OK;
	return GF_BAD_PARAM;
}

#endif //GPAC_DISABLE_NETWORK
