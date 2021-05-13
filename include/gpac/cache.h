/*
 *                      GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Pierre Souchay
 *			Copyright (c) Telecom ParisTech 2000-2019
 *                                      All rights reserved
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

#ifndef _GF_CACHE_H_
#define _GF_CACHE_H_

/*!
\file <gpac/cache.h>
\brief HTTP Cache management.
 */

/*!
\addtogroup cache_grp
\brief HTTP Downloader Cache

This section documents the file HTTP caching tools the GPAC framework.

@{
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

/**
 * Handle for Cache Entries.
 * You can use the gf_cache_get_* functions to get the cache properties
 */
typedef struct __DownloadedCacheEntryStruct * DownloadedCacheEntry;

/*! cache object*/
typedef struct __CacheReaderStruct * GF_CacheReader;

/**

Free The DownloadedCacheEntry handle
\param entry The entry to delete
\return GF_OK
 */
GF_Err gf_cache_delete_entry( const DownloadedCacheEntry entry );

/**
 * Get the ETag associated with this cache entry if any
\param entry The entry
\return The ETag if any was defined, NULL otherwise
 */
const char * gf_cache_get_etag_on_server( const DownloadedCacheEntry entry );

/**

Set the eTag in the cache. Data is duplicated, so original string can be freed by caller.
\param entry The entry
\param eTag The eTag to set
\return GF_OK if entry and eTag are valid, GF_BAD_PARAM otherwise
 */
GF_Err gf_cache_set_etag_on_disk(const DownloadedCacheEntry entry, const char * eTag );


/**

Set the eTag in the cache. Data is duplicated, so original string can be freed by caller.
\param entry The entry
\param eTag The eTag to set
\return GF_OK if entry and eTag are valid, GF_BAD_PARAM otherwise
 */
GF_Err gf_cache_set_etag_on_server(const DownloadedCacheEntry entry, const char * eTag );


/**

Get the Mime-Type associated with this cache entry.
\param entry The entry
\return The Mime-Type (never NULL if entry is valid)
 */
const char * gf_cache_get_mime_type( const DownloadedCacheEntry entry );

/**

Set the Mime-Type in the cache. Data is duplicated, so original string can be freed by caller.
\param entry The entry
\param mime_type The mime-type to set
\return GF_OK if entry and mime-type are valid, GF_BAD_PARAM otherwise
 */
GF_Err gf_cache_set_mime_type(const DownloadedCacheEntry entry, const char * mime_type );

/**

Get the URL associated with this cache entry.
\param entry The entry
\return The Hash key (never NULL if entry is valid)
 */
const char * gf_cache_get_url( const DownloadedCacheEntry entry );

/**

Tells whether a cache entry should be cached safely (no
\param entry The entry
\return 1 if entry should be cached
 */
Bool gf_cache_can_be_cached( const DownloadedCacheEntry entry );

/**

Get the Last-Modified information associated with this cache entry.
\param entry The entry
\return The Last-Modified header (can be NULL)
 */
const char * gf_cache_get_last_modified_on_server ( const DownloadedCacheEntry entry );

/**

Set the Last-Modified header for this cache entry
\param entry The entry
\param newLastModified The new value to set, will be duplicated
\return GF_OK if everything went alright, GF_BAD_PARAM if entry is NULL
 */
GF_Err gf_cache_set_last_modified_on_disk ( const DownloadedCacheEntry entry, const char * newLastModified );

/**

Set the Last-Modified header for this cache entry
\param entry The entry
\param newLastModified The new value to set, will be duplicated
\return GF_OK if everything went alright, GF_BAD_PARAM if entry is NULL
 */
GF_Err gf_cache_set_last_modified_on_server ( const DownloadedCacheEntry entry, const char * newLastModified );

/**

Get the file name of cache associated with this cache entry.
\param entry The entry
\return The Cache file (never NULL if entry is valid)
 */
const char * gf_cache_get_cache_filename( const DownloadedCacheEntry entry );

/**

Get the real file size of the cache entry
\param entry The entry
\return the file size
 */
u32 gf_cache_get_cache_filesize( const DownloadedCacheEntry entry );

/*!

Flushes The disk cache for this entry (by persisting the property file
\param entry The entry
\return error if any
*/
GF_Err gf_cache_flush_disk_cache( const DownloadedCacheEntry entry );

/*!

Set content length of resource
\param entry The entry
\param length size of the content in bytes
\return error if any
 */
GF_Err gf_cache_set_content_length( const DownloadedCacheEntry entry, u32 length );

/**

Get content length of resource
\param entry The entry
\return size of the content in bytes
 */
u32 gf_cache_get_content_length( const DownloadedCacheEntry entry);

/**
Get  directives headers associated with the cache
\param entry The entry of cache to use
\param etag set to etag value or NULL if no cache
\param last_modif set to last modif value or NULL if no cache
\return GF_OK if everything went fine, GF_BAD_PARAM if parameters are wrong
 */
GF_Err gf_cache_get_http_headers(const DownloadedCacheEntry entry, const char **etag, const char **last_modif);

/*
 * Cache Management functions
 */

 /*!

Computes the size of the cache elements in directory
\param directory containing cache files
\return size in bytes
 */
u64 gf_cache_get_size(const char * directory);

/*!
Delete all cached files in given directory starting with startpattern
\param directory to clean up
\return GF_OK if everything went fine
 */
GF_Err gf_cache_delete_all_cached_files(const char * directory);


/*!

Check if a given cache entry is corrupted (incomplete)
\param entry The entry
\return GF_TRUE if resource is corrupted
 */
Bool gf_cache_check_if_cache_file_is_corrupted(const DownloadedCacheEntry entry);

/*!
Mark associated files as "to be deleted" when the cache entry is removed
\param entry The entry
 */
void gf_cache_entry_set_delete_files_when_deleted(const DownloadedCacheEntry entry);

/*!
Check if associated files is marked as "to be deleted" when the cache entry is removed
\param entry The entry
\return GF_TRUE if cache entry is flaged as "to be deleted"
 */
Bool gf_cache_entry_is_delete_files_when_deleted(const DownloadedCacheEntry entry);

/*!
Get the number of sessions for a cache entry
\param entry The entry
\return the number of sessions using this cache entry
 */
u32 gf_cache_get_sessions_count_for_cache_entry(const DownloadedCacheEntry entry);

/*!
Get the start range of a cache entry
\param entry The entry
\return the start range in bytes
 */
u64 gf_cache_get_start_range( const DownloadedCacheEntry entry );
/*!
Get the end range of a cache entry
\param entry The entry
\return the end range in bytes
 */
u64 gf_cache_get_end_range( const DownloadedCacheEntry entry );

/*!
Check if the entry is marked as "headers processed" (reply headers have been parsed)
\param entry The entry
\return GF_TRUE if the entry is marked
 */
Bool gf_cache_are_headers_processed(const DownloadedCacheEntry entry);
/*!
Mark the entry as "headers processed"
\param entry The entry
\return error if any
 */
GF_Err gf_cache_set_headers_processed(const DownloadedCacheEntry entry);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* _GF_CACHE_H_ */
