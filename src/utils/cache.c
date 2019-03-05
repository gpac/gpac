/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Pierre Souchay, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2012
 *					All rights reserved
 *
 *   This file is part of GPAC / common tools sub-project
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

#ifndef GPAC_DISABLE_CORE_TOOLS

#include <gpac/cache.h>
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
static const char * CACHE_SECTION_NAME_URL = "url";
static const char * CACHE_SECTION_NAME_RANGE = "range";
static const char * CACHE_SECTION_NAME_ETAG = "ETag";
static const char * CACHE_SECTION_NAME_MIME_TYPE = "Content-Type";
static const char * CACHE_SECTION_NAME_CONTENT_SIZE = "Content-Length";
static const char * CACHE_SECTION_NAME_LAST_MODIFIED = "Last-Modified";

enum CacheValid
{
	NO_VALIDATION = 0,
	MUST_REVALIDATE = 1,
	IS_HTTPS = 2,
	CORRUPTED = 4,
	NO_CACHE = 8
};

struct __CacheReaderStruct {
	FILE * readPtr;
	s64 readPosition;
};

typedef struct __DownloadedRangeStruc {
	u32 start;
	u32 end;
	const char * filename;
} * DownloadedRange;

//#define ENABLE_WRITE_MX

/**
* This opaque structure handles the data from the cache
*/
struct __DownloadedCacheEntryStruct
{
	/**
	* URL of the cache (never NULL)
	*/
	char * url;
	/**
	* Hash of the cache (never NULL)
	*/
	char * hash;
	/**
	* Name of the cache filename, (can be NULL)
	*/
	char * cache_filename;
	/**
	* Name of the cached properties filename , (can be NULL)
	*/
	GF_Config * properties;
	/**
	* Theorical size of cache if any
	*/
	u32          contentLength;
	/**
	* Real size of cache
	*/
	u32          cacheSize;
	/**
	* GMT timestamp for revalidation
	*/
	u32          validity;
	/**
	* The last modification time on the server
	*/
	char *       serverLastModified;
	/**
	* The last modification time of the cache if any
	*/
	char *       diskLastModified;
	/**
	* ETag if any
	*/
	char * serverETag;
	/**
	* ETag if any
	*/
	char * diskETag;
	/**
	* Mime-type (never NULL)
	*/
	char * mimeType;
	/**
	* Write pointer for the cache
	*/
	FILE * writeFilePtr;
	/**
	* Bytes written during this cache session
	*/
	u32 written_in_cache;
	/**
	* Flag indicating whether we have to revalidate
	*/
	enum CacheValid   flags;

	const GF_DownloadSession * write_session;

#ifdef ENABLE_WRITE_MX
	GF_Mutex * write_mutex;
#endif

	GF_List * sessions;

	Bool deletableFilesOnDelete;

	GF_DownloadManager * dm;

	/*start and end range of the cache*/
	u64 range_start, range_end;

	Bool continue_file;
	Bool file_exists;

	u32 previousRangeContentLength;
	/*set once headers have been processed*/
	Bool headers_done;
	/**
	* Set to 1 if file is not stored on disk
	*/
	Bool memory_stored;
	u32 mem_allocated;
	u8 *mem_storage;
	char *forced_headers;
	u32 downtime;
};

Bool delete_cache_files(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info) {
	const char * startPattern;
	int sz;
	assert( cbck );
	assert( item_name );
	assert( item_path);
	startPattern = (const char *) cbck;
	sz = (u32) strlen( startPattern );
	if (!strncmp(startPattern, item_name, sz)) {
		if (GF_OK != gf_delete_file(item_path))
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] : failed to cleanup file %s\n", item_path));
	}
	return GF_FALSE;
}

static const char * cache_file_prefix = "gpac_cache_";

Bool gather_cache_size(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	u64 *out_size = (u64 *)cbck;
	if (!strncmp(cache_file_prefix, item_name, strlen(cache_file_prefix))) {
		*out_size += file_info->size;
	}
	return 0;
}

u64 gf_cache_get_size(const char * directory) {
	u64 size = 0;
	gf_enum_directory(directory, GF_FALSE, gather_cache_size, (void*)&size, NULL);
	return size;
}

GF_Err gf_cache_delete_all_cached_files(const char * directory) {
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("Deleting cached files in %s...\n", directory));
	return gf_enum_directory( directory, GF_FALSE, delete_cache_files, (void*)cache_file_prefix, NULL);
}

void gf_cache_entry_set_delete_files_when_deleted(const DownloadedCacheEntry entry) {
	if (entry)
		entry->deletableFilesOnDelete = GF_TRUE;
}

Bool gf_cache_entry_is_delete_files_when_deleted(const DownloadedCacheEntry entry)
{
	if (!entry)
		return GF_FALSE;
	return entry->deletableFilesOnDelete;
}

#define CHECK_ENTRY if (!entry) { GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] entry is null at " __FILE__ ":%d\n", __LINE__)); return GF_BAD_PARAM; }

/*
* Getters functions
*/

const char * gf_cache_get_etag_on_server ( const DownloadedCacheEntry entry )
{
	return entry ? entry->serverETag : NULL;
}

const char * gf_cache_get_etag_on_disk ( const DownloadedCacheEntry entry )
{
	return entry ? entry->serverETag : NULL;
}

GF_EXPORT
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


GF_EXPORT
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

GF_EXPORT
GF_Err gf_cache_set_mime_type(const DownloadedCacheEntry entry, const char * mime_type ) {
	if (!entry)
		return GF_BAD_PARAM;
	if (entry->mimeType)
		gf_free(entry->mimeType);
	entry->mimeType = mime_type? gf_strdup( mime_type) : NULL;
	return GF_OK;
}

Bool gf_cache_is_cached_on_disk(const DownloadedCacheEntry entry ) {
	if (entry == NULL)
		return GF_FALSE;
	return entry->flags & NO_CACHE;
}

u64 gf_cache_get_start_range( const DownloadedCacheEntry entry )
{
	return entry ? entry->range_start : 0;
}

u64 gf_cache_get_end_range( const DownloadedCacheEntry entry )
{
	return entry ? entry->range_end : 0;
}

GF_EXPORT
const char * gf_cache_get_url ( const DownloadedCacheEntry entry )
{
	return entry ? entry->url : NULL;
}

const char * gf_cache_get_hash ( const DownloadedCacheEntry entry )
{
	return entry ? entry->hash : NULL;
}

const char * gf_cache_get_last_modified_on_server ( const DownloadedCacheEntry entry )
{
	return entry ? entry->serverLastModified : NULL;
}

const char * gf_cache_get_last_modified_on_disk ( const DownloadedCacheEntry entry )
{
	return entry ? entry->diskLastModified : NULL;
}

GF_EXPORT
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

#define _CACHE_TMP_SIZE 4096

GF_Err gf_cache_flush_disk_cache ( const DownloadedCacheEntry entry )
{
	char buff[100];
	CHECK_ENTRY;
	if ( !entry->properties)
		return GF_OK;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] gf_cache_flush_disk_cache:%d for entry=%p\n", __LINE__, entry));
	gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_URL, entry->url);

	sprintf(buff, LLD"-"LLD, entry->range_start, entry->range_end);
	gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_RANGE, buff);

	if (entry->mimeType)
		gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_MIME_TYPE, entry->mimeType);
	if (entry->diskETag)
		gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_ETAG, entry->diskETag);
	if (entry->diskLastModified)
		gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_LAST_MODIFIED, entry->diskLastModified);
	{
		snprintf(buff, 16, "%d", entry->contentLength);
		gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_CONTENT_SIZE, buff);
	}
	return gf_cfg_save ( entry->properties );
}

u32 gf_cache_get_cache_filesize ( const DownloadedCacheEntry entry )
{
	return entry ? entry->cacheSize : -1;
}

GF_EXPORT
const char * gf_cache_get_cache_filename( const DownloadedCacheEntry entry )
{
	return entry ? entry->cache_filename : NULL;
}

GF_EXPORT
GF_Err gf_cache_append_http_headers(const DownloadedCacheEntry entry, char * httpRequest) {
	if (!entry || !httpRequest)
		return GF_BAD_PARAM;
	if (entry->flags)
		return GF_OK;
	if (gf_cache_check_if_cache_file_is_corrupted(entry))
		return GF_OK;
	/* OK, this is potentially bad if httpRequest is not big enough */
	if (entry->diskETag) {
		strcat(httpRequest, "If-None-Match: ");
		strcat(httpRequest, entry->diskETag);
		strcat(httpRequest, "\r\n");
	}
	if (entry->diskLastModified) {
		strcat(httpRequest, "If-Modified-Since: ");
		strcat(httpRequest, entry->diskLastModified);
		strcat(httpRequest, "\r\n");
	}
	return GF_OK;
}

#define _CACHE_HASH_SIZE 20
#define _CACHE_MAX_EXTENSION_SIZE 6
static const char * default_cache_file_suffix = ".dat";
static const char * cache_file_info_suffix = ".txt";

GF_EXPORT
DownloadedCacheEntry gf_cache_create_entry ( GF_DownloadManager * dm, const char * cache_directory, const char * url , u64 start_range, u64 end_range, Bool mem_storage)
{
	char tmp[_CACHE_TMP_SIZE];
	u8 hash[_CACHE_HASH_SIZE];
	int sz;
	char ext[_CACHE_MAX_EXTENSION_SIZE];
	DownloadedCacheEntry entry = NULL;
	if ( !dm || !url || !cache_directory) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
		       ("[CACHE] gf_cache_create_entry :%d, dm=%p, url=%s cache_directory=%s, aborting.\n", __LINE__, dm, url, cache_directory));
		return entry;
	}
	sz = (u32) strlen ( url );
	if ( sz > _CACHE_TMP_SIZE )
	{
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
		       ("[CACHE] gf_cache_create_entry:%d : ERROR, URL is too long (%d chars), more than %d chars.\n", __LINE__, sz, _CACHE_TMP_SIZE ));
		return entry;
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

	if ( !entry ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("gf_cache_create_entry:%d : out of memory !\n", __LINE__));
		return NULL;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, entry=%p\n", __LINE__, entry));

	entry->url = gf_strdup ( url );
	entry->hash = gf_strdup ( tmp );

	entry->memory_stored = mem_storage;

	entry->cacheSize = 0;
	entry->contentLength = 0;
	entry->serverETag = NULL;
	entry->diskETag = NULL;
	entry->flags = NO_VALIDATION;
	entry->validity = 0;
	entry->diskLastModified = NULL;
	entry->serverLastModified = NULL;
	entry->dm = dm;
	entry->range_start = start_range;
	entry->range_end = end_range;

#ifdef ENABLE_WRITE_MX
	{
		char name[1024];
		snprintf(name, sizeof(name), "CachedEntryWriteMx=%p, url=%s", (void*) entry, url);
		entry->write_mutex = gf_mx_new(name);
		assert(entry->write_mutex);
	}
#endif

	entry->deletableFilesOnDelete = GF_FALSE;
	entry->write_session = NULL;
	entry->sessions = gf_list_new();

	if (entry->memory_stored) {
		entry->cache_filename = (char*)gf_malloc ( strlen ("gmem://") + 8 + strlen("@") + 16 + 1);
	} else {
		/* Sizeof cache directory + hash + possible extension */
		entry->cache_filename = (char*)gf_malloc ( strlen ( cache_directory ) + strlen(cache_file_prefix) + strlen(tmp) + _CACHE_MAX_EXTENSION_SIZE + 1);
	}

	if ( !entry->hash || !entry->url || !entry->cache_filename || !entry->sessions)
	{
		GF_Err err;
		/* Probably out of memory */
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, aborting due to OUT of MEMORY !\n", __LINE__));
		err = gf_cache_delete_entry ( entry );
		if ( err != GF_OK ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, failed to delete cache entry!\n", __LINE__));
		}
		return NULL;
	}

	if (entry->memory_stored) {
		sprintf(entry->cache_filename, "gmem://%d@%p", entry->contentLength, entry->mem_storage);
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
	tmp[0] = '\0';
	strcpy( tmp, cache_file_prefix);
	strcat( tmp, entry->hash );
	strcat( tmp , ext);
	strcat ( tmp, cache_file_info_suffix );
	entry->properties = gf_cfg_force_new ( cache_directory, tmp );
	if ( !entry->properties )
	{
		GF_Err err;
		/* out of memory ? */
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, aborting due to OUT of MEMORY !\n", __LINE__));
		err = gf_cache_delete_entry ( entry );
		if ( err != GF_OK ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, failed to delete cache entry!\n", __LINE__));
		}
		return NULL;
	}
	gf_cache_set_etag_on_disk(entry, gf_cfg_get_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_ETAG));
	gf_cache_set_etag_on_server(entry, gf_cfg_get_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_ETAG));
	gf_cache_set_mime_type(entry, gf_cfg_get_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_MIME_TYPE));
	gf_cache_set_last_modified_on_disk(entry, gf_cfg_get_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_LAST_MODIFIED));
	gf_cache_set_last_modified_on_server(entry, gf_cfg_get_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_LAST_MODIFIED));
	{
		const char * keyValue = gf_cfg_get_key ( entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_URL );
		if ( keyValue == NULL || stricmp ( url, keyValue ) )
			entry->flags |= CORRUPTED;

		keyValue = gf_cfg_get_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_RANGE);
		if (keyValue) {
			u64 s, e;
			sscanf(keyValue, LLD"-"LLD, &s, &e);
			/*mark as corrupted if not same range (we don't support this for the time being ...*/
			if ((s!=entry->range_start) || (e!=entry->range_end))
				entry->flags |= CORRUPTED;
		}
	}
	gf_cache_check_if_cache_file_is_corrupted(entry);

	return entry;
}

GF_EXPORT
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
	return entry ? entry->contentLength : 0;
}

GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success )
{
	GF_Err e = GF_OK;
	CHECK_ENTRY;
	if (!sess || !entry->write_session || entry->write_session != sess)
		return GF_OK;
	assert( sess == entry->write_session );
	if (entry->writeFilePtr) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK,
		       ("[CACHE] Closing file %s, %d bytes written.\n", entry->cache_filename, entry->written_in_cache));

		if (fflush( entry->writeFilePtr ) || gf_fclose( entry->writeFilePtr )) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to flush/close file on disk\n"));
			e = GF_IO_ERR;
		}
		if (!e) {
			e = gf_cache_flush_disk_cache(entry);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to flush cache entry on disk\n"));
			}
		}
		if (!e && success) {
			e = gf_cache_set_last_modified_on_disk( entry, gf_cache_get_last_modified_on_server(entry));
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to set last-modified\n"));
			} else {
				e = gf_cache_set_etag_on_disk( entry, gf_cache_get_etag_on_server(entry));
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to set etag\n"));
				}
			}
		}
		if (!e) {
			e = gf_cache_flush_disk_cache(entry);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to flush cache entry on disk after etag/last-modified\n"));
			}
		}

#if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500
		/* On  UNIX, be sure to flush all the data */
		sync();
#endif
		entry->writeFilePtr = NULL;
		if (GF_OK != e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to fully write file on cache, e=%d\n", e));
		}
	}
	entry->write_session = NULL;
#ifdef ENABLE_WRITE_MX
	gf_mx_v(entry->write_mutex);
#endif
	return e;
}

GF_EXPORT
GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess )
{
	CHECK_ENTRY;
	if (!sess)
		return GF_BAD_PARAM;
#ifdef ENABLE_WRITE_MX
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK,("[CACHE] Locking write mutex %p for entry=%s\n", (void*) (entry->write_mutex), entry->url) );
	gf_mx_p(entry->write_mutex);
#endif
	entry->write_session = sess;
	if (!entry->continue_file) {
		assert( ! entry->writeFilePtr);

		entry->written_in_cache = 0;
	}
	entry->flags &= ~CORRUPTED;

	if (entry->memory_stored) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] Opening cache file %s for write (%s)...\n", entry->cache_filename, entry->url));
		if (!entry->mem_allocated || (entry->mem_allocated < entry->contentLength)) {
			if (entry->contentLength) entry->mem_allocated = entry->contentLength;
			else if (!entry->mem_allocated) entry->mem_allocated = 81920;
			entry->mem_storage = (u8*)gf_realloc(entry->mem_storage, sizeof(char)* (entry->mem_allocated + 2) );
		}
		if (!entry->mem_allocated) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Failed to create memory storage for file %s\n", entry->url));
			return GF_OUT_OF_MEM;
		}
		sprintf(entry->cache_filename, "gmem://%d@%p", entry->contentLength, entry->mem_storage);
		return GF_OK;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] Opening cache file %s for write (%s)...\n", entry->cache_filename, entry->url));
	entry->writeFilePtr = gf_fopen(entry->cache_filename, entry->continue_file ? "a+b" : "wb");
	if (!entry->writeFilePtr) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK,
		       ("[CACHE] Error while opening cache file %s for writting.\n", entry->cache_filename));
		entry->write_session = NULL;
#ifdef ENABLE_WRITE_MX
		gf_mx_v(entry->write_mutex);
#endif
		return GF_IO_ERR;
	}
	entry->file_exists = GF_TRUE;
	if (entry->continue_file )
		gf_fseek(entry->writeFilePtr, 0, SEEK_END);
	return GF_OK;
}

GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size) {
	u32 read;
	CHECK_ENTRY;

	if (!data || (!entry->writeFilePtr && !entry->mem_storage) || sess != entry->write_session) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("Incorrect parameter : data=%p, writeFilePtr=%p mem_storage=%p at "__FILE__"\n", data, entry->writeFilePtr, entry->mem_storage));
		return GF_BAD_PARAM;
	}

	if (entry->memory_stored) {
		if (entry->written_in_cache + size > entry->mem_allocated) {
			u32 new_size = MAX(entry->mem_allocated*2, entry->written_in_cache + size);
			entry->mem_storage = (u8*)gf_realloc(entry->mem_storage, (new_size+2));
			entry->mem_allocated = new_size;
			sprintf(entry->cache_filename, "gmem://%d@%p", entry->contentLength, entry->mem_storage);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] Reallocating memory cache to %d bytes\n", new_size));
		}
		memcpy(entry->mem_storage + entry->written_in_cache, data, size);
		entry->written_in_cache += size;
		memset(entry->mem_storage + entry->written_in_cache, 0, 2);
		sprintf(entry->cache_filename, "gmem://%d@%p", entry->written_in_cache, entry->mem_storage);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] Storing %d bytes to memory\n", size));
		return GF_OK;
	}

	read = (u32) gf_fwrite(data, sizeof(char), size, entry->writeFilePtr);
	if (read > 0)
		entry->written_in_cache+= read;
	if (read != size) {
		/* Something bad happened */
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
		       ("[CACHE] Error while writting %d bytes of data to cache : has written only %d bytes.", size, read));
		gf_cache_close_write_cache(entry, sess, GF_FALSE);
		gf_delete_file(entry->cache_filename);
		return GF_IO_ERR;
	}
	if (fflush(entry->writeFilePtr)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
		       ("[CACHE] Error while flushing data bytes to cache file : %s.", entry->cache_filename));
		gf_cache_close_write_cache(entry, sess, GF_FALSE);
		gf_delete_file(entry->cache_filename);
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] Writing %d bytes to cache\n", size));
	return GF_OK;
}

GF_CacheReader gf_cache_reader_new(const DownloadedCacheEntry entry) {
	GF_CacheReader reader;
	if (entry == NULL)
		return NULL;
	reader = (GF_CacheReader)gf_malloc(sizeof(struct __CacheReaderStruct));
	if (reader == NULL)
		return NULL;
	reader->readPtr = gf_fopen( entry->cache_filename, "rb" );
	reader->readPosition = 0;
	if (!reader->readPtr) {
		gf_cache_reader_del(reader);
		return NULL;
	}
	return reader;
}

GF_Err gf_cache_reader_del( GF_CacheReader handle ) {
	if (!handle)
		return GF_BAD_PARAM;
	if (handle->readPtr)
		gf_fclose(handle->readPtr);
	handle->readPtr = NULL;
	handle->readPosition = -1;
	return GF_OK;
}

s64 gf_cache_reader_seek_at( GF_CacheReader reader, u64 seekPosition) {
	if (!reader)
		return -1;
	reader->readPosition = gf_fseek(reader->readPtr, seekPosition, SEEK_SET);
	return reader->readPosition;
}

s64 gf_cache_reader_get_position( const GF_CacheReader reader) {
	if (!reader)
		return -1;
	return reader->readPosition;
}

s64 gf_cache_reader_get_currentSize( GF_CacheReader reader );

s64 gf_cache_reader_full_size( GF_CacheReader reader );

s32 gf_cache_reader_read( GF_CacheReader reader, char * buff, s32 length) {
	s32 read;
	if (!reader || !buff || length < 0 || !reader->readPtr)
		return -1;
	read = (s32) fread(buff, sizeof(char), length, reader->readPtr);
	if (read > 0)
		reader->readPosition+= read;
	return read;
}

GF_Err gf_cache_delete_entry ( const DownloadedCacheEntry entry )
{
	if ( !entry )
		return GF_OK;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] gf_cache_delete_entry:%d, entry=%p\n", __LINE__, entry));
	if (entry->writeFilePtr) {
		/** Cache should have been close before, abornormal situation */
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_delete_entry:%d, entry=%p, cache has not been closed properly\n", __LINE__, entry));
		gf_fclose(entry->writeFilePtr);
	}
#ifdef ENABLE_WRITE_MX
	if (entry->write_mutex) {
		gf_mx_del(entry->write_mutex);
	}
#endif
	if (entry->file_exists && entry->deletableFilesOnDelete) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] url %s cleanup, deleting %s...\n", entry->url, entry->cache_filename));
		if (GF_OK != gf_delete_file(entry->cache_filename))
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_delete_entry:%d, failed to delete file %s\n", __LINE__, entry->cache_filename));
	}
#ifdef ENABLE_WRITE_MX
	entry->write_mutex = NULL;
#endif
	entry->write_session = NULL;
	entry->writeFilePtr = NULL;
	if (entry->serverETag)
		gf_free(entry->serverETag);
	entry->serverETag = NULL;

	if (entry->diskETag)
		gf_free(entry->diskETag);
	entry->diskETag = NULL;

	if (entry->serverLastModified)
		gf_free(entry->serverLastModified);
	entry->serverLastModified = NULL;

	if (entry->diskLastModified)
		gf_free(entry->diskLastModified);
	entry->diskLastModified = NULL;

	if ( entry->hash ) {
		gf_free ( entry->hash );
		entry->hash = NULL;
	}
	if ( entry->url ) {
		gf_free ( entry->url );
		entry->url = NULL;
	}
	if ( entry->mimeType ) {
		gf_free ( entry->mimeType );
		entry->mimeType = NULL;
	}
	if (entry->mem_storage && entry->mem_allocated) {
		gf_free(entry->mem_storage);
	}
	if ( entry->forced_headers ) {
		gf_free ( entry->forced_headers );
	}

	if ( entry->cache_filename ) {
		gf_free ( entry->cache_filename );
		entry->cache_filename = NULL;
	}
	if ( entry->properties ) {
		char * propfile;
		if (entry->deletableFilesOnDelete)
			propfile = gf_cfg_get_filename(entry->properties);
		else
			propfile = NULL;
		gf_cfg_del ( entry->properties );
		entry->properties = NULL;
		if (propfile) {
			//this may fail because the prop file is not yet flushed to disk
			gf_delete_file( propfile );
			gf_free ( propfile );
		}
	}
	entry->dm = NULL;
	if (entry->sessions) {
		assert( gf_list_count(entry->sessions) == 0);
		gf_list_del(entry->sessions);
		entry->sessions = NULL;
	}

	gf_free (entry);
	return GF_OK;
}

Bool gf_cache_check_if_cache_file_is_corrupted(const DownloadedCacheEntry entry)
{
	FILE *the_cache = gf_fopen ( entry->cache_filename, "rb" );
	if ( the_cache ) {
		char * endPtr;
		const char * keyValue = gf_cfg_get_key ( entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_CONTENT_SIZE );

		gf_fseek ( the_cache, 0, SEEK_END );
		entry->cacheSize = ( u32 ) gf_ftell ( the_cache );
		gf_fclose ( the_cache );
		if (keyValue) {
			entry->contentLength = (u32) strtoul( keyValue, &endPtr, 10);
			if (*endPtr!='\0' || entry->contentLength != entry->cacheSize) {
				entry->flags |= CORRUPTED;
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, Cache corrupted: file and cache info size mismatch.\n", __LINE__));
			}
		} else {
			entry->flags |= CORRUPTED;
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, CACHE is corrupted !\n", __LINE__));
		}
	} else {
		entry->flags |= CORRUPTED;
	}
	return entry->flags & CORRUPTED;
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] gf_cache_remove_session_from_cache_entry:%d, Failed to properly close cache file '%s' of url '%s', cache may be corrupted !\n", __LINE__, entry->cache_filename, entry->url));
			}
		}
		entry->writeFilePtr = NULL;
		entry->write_session = NULL;
#ifdef ENABLE_WRITE_MX
		gf_mx_v(entry->write_mutex);
#endif
	}
	return count;
}

u32 gf_cache_get_sessions_count_for_cache_entry(const DownloadedCacheEntry entry)
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

FILE *gf_cache_get_file_pointer(const DownloadedCacheEntry entry)
{
	if (entry) return entry->writeFilePtr;
	return NULL;
}

void gf_cache_set_end_range(DownloadedCacheEntry entry, u64 range_end)
{
	entry->previousRangeContentLength = entry->contentLength;
	entry->range_end = range_end;
	entry->continue_file = GF_TRUE;
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

Bool gf_cache_set_content(const DownloadedCacheEntry entry, char *data, u32 size, Bool copy)
{
	if (!entry || !entry->memory_stored) return GF_FALSE;

	if (!copy) {
		if (entry->mem_allocated) gf_free(entry->mem_storage);
		entry->mem_storage = (u8 *) data;
		entry->written_in_cache = size;
		entry->mem_allocated = 0;
		sprintf(entry->cache_filename, "gmem://%d@%p", entry->written_in_cache, entry->mem_storage);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] Storing %d bytes to memory from external module\n", size));
		return GF_TRUE;
	}
	if ( size > entry->mem_allocated) {
		u32 new_size;
		new_size = MAX(entry->mem_allocated*2, size);
		entry->mem_storage = (u8*)gf_realloc(entry->mem_allocated ? entry->mem_storage : NULL, (new_size+2));
		entry->mem_allocated = new_size;
		sprintf(entry->cache_filename, "gmem://%d@%p", entry->contentLength, entry->mem_storage);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] Reallocating memory cache to %d bytes\n", new_size));
	}
	memcpy(entry->mem_storage, data, size);
	entry->written_in_cache = size;
	sprintf(entry->cache_filename, "gmem://%d@%p", entry->written_in_cache, entry->mem_storage);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] Storing %d bytes to cache memory\n", size));
	return GF_FALSE;
}

#endif
