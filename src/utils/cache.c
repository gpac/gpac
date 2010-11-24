/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *				Copyright (c) 2005-2005 ENST
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

#include <gpac/cache.h>
#include <gpac/network.h>
#include <gpac/download.h>
#include <gpac/token.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/base_coding.h>
#include <gpac/crypt.h>
#include <gpac/tools.h>
#include <gpac/config_file.h>
#include <stdio.h>
#include <string.h>

#if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500
#include <unistd.h>
#endif

static const char * CACHE_SECTION_NAME = "cache";

static const char * CACHE_SECTION_NAME_URL = "url";

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

struct __CacheReaderStruct{
    FILE * readPtr;
    s64 readPosition;
};

typedef struct __DownloadedRangeStruc{
    u32 start;
    u32 end;
    const char * filename;
} * DownloadedRange;


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
    
    const GF_DownloadSession * write_lock;
    
    GF_List * sessions;
    
    GF_DownloadManager * dm;
};




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

const char * gf_cache_get_mime_type ( const DownloadedCacheEntry entry )
{
    return entry ? entry->mimeType : NULL;
}

GF_Err gf_cache_set_etag_on_server(const DownloadedCacheEntry entry, const char * eTag ) {
    if (!entry)
        return GF_BAD_PARAM;
    if (entry->serverETag)
        gf_free(entry->serverETag);
    entry->serverETag = eTag ? strdup(eTag) : NULL;
    return GF_OK;
}

GF_Err gf_cache_set_etag_on_disk(const DownloadedCacheEntry entry, const char * eTag ) {
    if (!entry)
        return GF_BAD_PARAM;
    if (entry->diskETag)
        gf_free(entry->diskETag);
    entry->diskETag = eTag ? strdup(eTag) : NULL;
    return GF_OK;
}

GF_Err gf_cache_set_mime_type(const DownloadedCacheEntry entry, const char * mime_type ) {
    if (!entry)
        return GF_BAD_PARAM;
    if (entry->mimeType)
        gf_free(entry->mimeType);
    entry->mimeType = mime_type? strdup( mime_type) : NULL;
    return GF_OK;
}

Bool gf_cache_is_cached_on_disk(const DownloadedCacheEntry entry ){
  if (entry == NULL)
    return 0;
  return entry->flags & NO_CACHE;
}

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

GF_Err gf_cache_set_last_modified_on_server ( const DownloadedCacheEntry entry, const char * newLastModified )
{
    if (!entry)
        return GF_BAD_PARAM;
    if (entry->serverLastModified)
        gf_free(entry->serverLastModified);
    entry->serverLastModified = newLastModified ? strdup(newLastModified) : NULL;
    return GF_OK;
}

GF_Err gf_cache_set_last_modified_on_disk ( const DownloadedCacheEntry entry, const char * newLastModified )
{
    if (!entry)
        return GF_BAD_PARAM;
    if (entry->diskLastModified)
        gf_free(entry->diskLastModified);
    entry->diskLastModified = newLastModified ? strdup(newLastModified) : NULL;
    return GF_OK;
}

#define _CACHE_TMP_SIZE 4096

GF_Err gf_cache_flush_disk_cache ( const DownloadedCacheEntry entry )
{
    CHECK_ENTRY;
    if ( !entry->properties)
        return GF_OK;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] gf_cache_flush_disk_cache:%d for entry=%p\n", __LINE__, entry));
    gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_URL, entry->url);
    if (entry->mimeType)
        gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_MIME_TYPE, entry->mimeType);
    if (entry->diskETag)
        gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_ETAG, entry->diskETag);
    if (entry->diskLastModified)
        gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_LAST_MODIFIED, entry->diskLastModified);
    {
        char buff[16];
        snprintf(buff, 16, "%d", entry->contentLength);
        gf_cfg_set_key(entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_CONTENT_SIZE, buff);
    }
    return gf_cfg_save ( entry->properties );
}

u32 gf_cache_get_cache_filesize ( const DownloadedCacheEntry entry )
{
    return entry ? entry->cacheSize : -1;
}

const char * gf_cache_get_cache_filename( const DownloadedCacheEntry entry )
{
    return entry ? entry->cache_filename : NULL;
}

GF_Err appendHttpCacheHeaders(const DownloadedCacheEntry entry, char * httpRequest) {
    if (!entry || !httpRequest)
        return GF_BAD_PARAM;
    if (entry->flags)
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

static const char * cache_file_prefix = "gpac_cache_";

static const char * default_cache_file_suffix = ".dat";

static const char * cache_file_info_suffix = ".txt";

DownloadedCacheEntry gf_cache_create_entry ( GF_DownloadManager * dm, const char * cache_directory, const char * url )
{
    char tmp[_CACHE_TMP_SIZE];
    u8 hash[_CACHE_HASH_SIZE];
    int sz;
    const char * ext;
    DownloadedCacheEntry entry = NULL;
    if ( !dm || !url || !cache_directory) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
               ("[CACHE] gf_cache_create_entry :%d, dm=%p, url=%s cache_directory=%s, aborting.\n", __LINE__, dm, url, cache_directory));
        return entry;
    }
    sz = strlen ( url );
    if ( sz > _CACHE_TMP_SIZE )
    {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
               ("[CACHE] gf_cache_create_entry:%d : ERROR, URL is too long (%d chars), more than %d chars.\n", __LINE__, sz, _CACHE_TMP_SIZE ));
        return entry;
    }
    tmp[0] = '\0';
    /*generate hash of the full url*/
    strcpy ( tmp, url );
    gf_sha1_csum ( tmp, strlen ( tmp ), hash );
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

    entry = gf_malloc ( sizeof ( struct __DownloadedCacheEntryStruct ) );
    if ( !entry ) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("gf_cache_create_entry:%d : OUT of memory !\n", __LINE__));
        return NULL;
    }
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK,
           ("[CACHE] gf_cache_create_entry:%d, entry=%p\n", __LINE__, entry));
    entry->properties = NULL;
    entry->writeFilePtr = NULL;
    entry->url = strdup ( url );
    entry->hash = strdup ( tmp );
    entry->mimeType = NULL;
    /* Sizeof cache directory + hash + possible extension */
    entry->cache_filename = gf_malloc ( strlen ( cache_directory ) + strlen(cache_file_prefix) + strlen(tmp) + _CACHE_MAX_EXTENSION_SIZE + 1);

    entry->cacheSize = 0;
    entry->contentLength = 0;
    entry->serverETag = NULL;
    entry->diskETag = NULL;
    entry->flags = NO_VALIDATION;
    entry->validity = 0;
    entry->diskLastModified = NULL;
    entry->serverLastModified = NULL;
    entry->dm = dm;
    entry->write_lock = NULL;
    entry->sessions = gf_list_new();
    if ( !entry->hash || !entry->url || !entry->cache_filename || !entry->sessions)
    {
        GF_Err err;
        /* Probably out of memory */
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, aborting due to OUT of MEMORY !\n", __LINE__));
        err = gf_cache_delete_entry ( entry );
        assert ( err == GF_OK );
        return NULL;
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
            parser = NULL;
        parser = strrchr ( tmp, '.' );
        if ( parser && ( strlen ( parser ) < _CACHE_MAX_EXTENSION_SIZE ) )
	  ext = parser;
	else
	  ext = default_cache_file_suffix;
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
        /* OUT of memory ? */
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, aborting due to OUT of MEMORY !\n", __LINE__));
        err = gf_cache_delete_entry ( entry );
        assert ( err == GF_OK );
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
    }
    {
        FILE *the_cache = gf_f64_open ( entry->cache_filename, "rb" );
        if ( the_cache )
        {
            char * endPtr;
            const char * keyValue = gf_cfg_get_key ( entry->properties, CACHE_SECTION_NAME, CACHE_SECTION_NAME_CONTENT_SIZE );

            gf_f64_seek ( the_cache, 0, SEEK_END );
            entry->cacheSize = ( u32 ) gf_f64_tell ( the_cache );
            fclose ( the_cache );
            if (keyValue) {
                entry->contentLength = strtoul( keyValue, &endPtr, 10);
                if (*endPtr!='\0' || entry->contentLength != entry->cacheSize) {
                    entry->flags |= CORRUPTED;
                    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, cached file and cache info size mismatch.\n", __LINE__));
                }
            } else
                entry->flags |= CORRUPTED;

        } else {
            entry->flags |= CORRUPTED;
        }
        if (entry->flags & CORRUPTED)
            GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_create_entry:%d, CACHE is corrupted !\n", __LINE__));


    }

    return entry;
}

GF_Err gf_cache_set_content_length( const DownloadedCacheEntry entry, u32 length ){
    CHECK_ENTRY;
    entry->contentLength = length;
    return GF_OK;
}

u32 gf_cache_get_content_length( const DownloadedCacheEntry entry){
  return entry ? entry->contentLength : 0;
}

GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success ) {
    GF_Err e = GF_OK;
    CHECK_ENTRY;
    if (!sess || !entry->write_lock || entry->write_lock != sess)
      return GF_OK;
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_close_write_cache:%d for entry=%p\n", __LINE__, entry));
    assert( sess == entry->write_lock );
    if (entry->writeFilePtr) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
               ("Closing cache file %s for write, size is %d.\n", entry->cache_filename, entry->written_in_cache));
        if (fflush( entry->writeFilePtr ) || fclose( entry->writeFilePtr ))
            e = GF_IO_ERR;
        e|= gf_cache_flush_disk_cache(entry);
	if (success){
	  gf_cache_set_last_modified_on_disk( entry, gf_cache_get_last_modified_on_server(entry));
	  gf_cache_set_etag_on_disk( entry, gf_cache_get_etag_on_server(entry));
	}
	gf_cache_flush_disk_cache(entry);
#if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500
        /* On  UNIX, be sure to flush all the data */
        sync();
#endif
        entry->writeFilePtr = NULL;
    }
    return e;
}


GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess ) {
    CHECK_ENTRY;
    if (!sess)
      return GF_BAD_PARAM;
    if (entry->write_lock){
      if (entry->write_lock == sess){
	GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] : double open with same entry = %p\n", sess));
	return GF_OK;
      } else {
	GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] : Trying to open cache for write with %p, but cache is already locked by entry = %p\n", sess, entry->write_lock));
      }
    }
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] gf_cache_open_write_cache:%d, entry=%p\n", __LINE__, entry));
    if (!entry->writeFilePtr) {
	entry->written_in_cache = 0;
	entry->write_lock = sess;
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
               ("Opening cache file %s for write...", entry->cache_filename));
        entry->writeFilePtr = fopen(entry->cache_filename, "wb");
        if (!entry->writeFilePtr) {
            GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
                   ("Error while opening cache file %s for writting.", entry->cache_filename));
            return GF_IO_ERR;
        }
    }
    
    return GF_OK;
}

GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size) {
    u32 readen;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[CACHE] gf_cache_write_to_cache:%d\n", __LINE__));
    CHECK_ENTRY;
    if (!data || !entry->writeFilePtr || sess != entry->write_lock) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("Incorrect parameter : data=%p, entry->writeFilePtr=%p at "__FILE__, data, entry->writeFilePtr));
        return GF_BAD_PARAM;
    }
    readen = fwrite(data, sizeof(char), size, entry->writeFilePtr);
    if (readen > 0)
      entry->written_in_cache+= readen;
    if (readen != size) {
        /* Something bad happened */
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
               ("[CACHE] Error while writting %d bytes of data to cache : has written only %d bytes.", size, readen));
        gf_cache_close_write_cache(entry, sess, 0);
        gf_delete_file(entry->cache_filename);
        return GF_IO_ERR;
    }
    if (fflush(entry->writeFilePtr)) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
               ("[CACHE] Error while flushing data bytes to cache file : %s.", entry->cache_filename));
        gf_cache_close_write_cache(entry, sess, 0);
        gf_delete_file(entry->cache_filename);
        return GF_IO_ERR;
    }
    return GF_OK;
}

DownloadedCacheEntry gf_cache_entry_dup_readonly( const DownloadedCacheEntry entry){
    DownloadedCacheEntry ret;
    if (!entry)
      return NULL;
    ret = gf_malloc ( sizeof ( struct __DownloadedCacheEntryStruct ) );
    if (!ret)
      return NULL;
    ret->cache_filename = entry->cache_filename ? gf_strdup( entry->cache_filename ) : NULL;
    ret->cacheSize = entry->cacheSize;
    ret->contentLength = entry->contentLength;
    ret->diskETag = entry->diskETag ? gf_strdup( entry->diskETag) : NULL;
    ret->serverETag = entry->serverETag ? gf_strdup( entry->serverETag) : NULL;
    ret->flags = entry->flags;
    ret->hash = entry->hash ? gf_strdup(entry->hash) : NULL;
    ret->diskLastModified = entry->diskLastModified ? gf_strdup(entry->diskLastModified) : NULL;
    ret->serverLastModified = entry->serverLastModified ? gf_strdup(entry->serverLastModified) : NULL;
    ret->mimeType = entry->mimeType ? gf_strdup(entry->mimeType) : NULL;
    ret->properties = NULL;
    ret->url = entry->url ? strdup(entry->url): NULL;
    ret->validity = entry->validity;
    ret->writeFilePtr = NULL;
    ret->written_in_cache = 0;
    ret->dm = NULL;
    ret->sessions = gf_list_new();
    ret->write_lock = NULL;
    return ret;
}

GF_CacheReader gf_cache_reader_new(const DownloadedCacheEntry entry){
  GF_CacheReader reader;
  if (entry == NULL)
    return NULL;
  reader = gf_malloc(sizeof(struct __CacheReaderStruct));
  if (reader == NULL)
    return NULL;
  reader->readPtr = gf_f64_open( entry->cache_filename, "rb" );
  reader->readPosition = 0;
  if (!reader->readPtr){
    gf_cache_reader_del(reader);
    return NULL;
  }
  return reader;
}

GF_Err gf_cache_reader_del( GF_CacheReader handle ){
  if (!handle)
    return GF_BAD_PARAM;
  if (handle->readPtr)
    fclose(handle->readPtr);
  handle->readPtr = NULL;
  handle->readPosition = -1;
  return GF_OK;
}

s64 gf_cache_reader_seek_at( GF_CacheReader reader, u64 seekPosition){
    if (!reader)
      return -1;
    reader->readPosition = gf_f64_seek(reader->readPtr, seekPosition, SEEK_SET);
    return reader->readPosition;
}

s64 gf_cache_reader_get_position( const GF_CacheReader reader){
  if (!reader)
    return -1;
  return reader->readPosition;
}

s64 gf_cache_reader_get_currentSize( GF_CacheReader reader );

s64 gf_cache_reader_full_size( GF_CacheReader reader );

s32 gf_cache_reader_read( GF_CacheReader reader, char * buff, s32 length){
    s32 readen;
    if (!reader || !buff || length < 0 || !reader->readPtr)
      return -1;
    readen = fread(buff, sizeof(char), length, reader->readPtr);
    if (readen > 0)
      reader->readPosition+= readen;
    return readen;
}

GF_Err gf_cache_delete_entry ( const DownloadedCacheEntry entry )
{
    if ( !entry )
        return GF_OK;
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_delete_entry:%d, entry=%p\n", __LINE__, entry));
    if (entry->writeFilePtr) {
	/** Cache should have been close before, abornormal situation */
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] gf_cache_delete_entry:%d, entry=%p, cache has not been closed properly\n", __LINE__, entry));
        fclose(entry->writeFilePtr);
	entry->write_lock = NULL;
        entry->writeFilePtr = NULL;
    }
    if ( entry->hash )
    {
        gf_free ( entry->hash );
        entry->hash = NULL;
    }
    if ( entry->url )
    {
        gf_free ( entry->url );
        entry->url = NULL;
    }
    if ( entry->mimeType )
    {
        gf_free ( entry->mimeType );
        entry->mimeType = NULL;
    }
    if ( entry->cache_filename )
    {
        gf_free ( entry->cache_filename );
        entry->cache_filename = NULL;
    }
    if ( entry->properties )
    {
        gf_cfg_del ( entry->properties );
        entry->properties = NULL;
    }
    assert( ! entry->write_lock );
    entry->dm = NULL;
    if (entry->sessions){
      while (gf_list_count(entry->sessions))
        gf_list_rem(entry->sessions, 0);
      gf_list_del(entry->sessions);
      entry->sessions = NULL;
    }
    gf_free (entry);
    return GF_OK;
}

