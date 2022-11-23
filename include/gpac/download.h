/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

#ifndef _GF_DOWNLOAD_H_
#define _GF_DOWNLOAD_H_

/*!
\file <gpac/download.h>
\brief HTTP(S) Downloader.
 */

/*!

\addtogroup download_grp
\brief HTTP Downloader

This section documents the file downloading tools the GPAC framework. Currently HTTP and HTTPS are supported. HTTPS may not be supported depending on GPAC compilation options (HTTPS in GPAC needs OpenSSL installed on the system).

@{

 */


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>
#include <gpac/config_file.h>
#include <gpac/cache.h>

/*!the download manager object. This is usually not used by GPAC modules*/
typedef struct __gf_download_manager GF_DownloadManager;
/*!the download manager session.*/
typedef struct __gf_download_session GF_DownloadSession;
/*!the optional filter session.*/
typedef struct __gf_filter_session GF_DownloadFilterSession;

/*! URL information object*/
typedef struct GF_URL_Info_Struct {
	const char * protocol;
	char * server_name;
	char * remotePath;
	char * canonicalRepresentation;
	char * userName;
	char * password;
	u16 port;
} GF_URL_Info;

/*!
Extracts the information from an URL. A call to gf_dm_url_info_init() must have been issue before calling this method.
\param url The URL to fill
\param info This structure will be initialized properly and filled with the data
\param baseURL The baseURL to use if any (can be null)
\return GF_OK if URL is well formed and supported by GPAC
 */
GF_Err gf_dm_get_url_info(const char * url, GF_URL_Info * info, const char * baseURL);

/**
Inits the GF_URL_Info structure before it can be used
\param info The structure to initialize
 */
void gf_dm_url_info_init(GF_URL_Info * info);

/*!
Frees the inner structures of a GF_URL_Info_Struct
\param info The info to free
 */
void gf_dm_url_info_del(GF_URL_Info * info);

/*!
\brief download manager constructor

Creates a new download manager object.
\param fsess optional filter session. If not NULL, the filter session will be used for async downloads. Otherwise, threads will be created
\return the download manager object
*/
GF_DownloadManager *gf_dm_new(GF_DownloadFilterSession *fsess);
/*!
\brief download manager destructor

Deletes the download manager. All running sessions are aborted
\param dm the download manager object
 */
void gf_dm_del(GF_DownloadManager *dm);

/*!
\brief callback function for authentication

This function is called back after user and password has been entered
\param usr_cbk opaque user data passed by download manager
\param usr_name the user name for the desired site, or NULL if the authentication was canceled
\param password the password for the desired site and user, or NULL if the authentication was canceled
\param store_info if TRUE, credentials will be stored in gpac config
*/
typedef void (*gf_dm_on_usr_pass)(void *usr_cbk, const char *usr_name, const char *password, Bool store_info);
/*!
\brief function for authentication

The gf_dm_get_usr_pass type is the type for the callback of the \ref gf_dm_set_auth_callback function used for password retrieval

\param usr_cbk opaque user data
\param secure indicates if TLS is used
\param site_url url of the site the user and password are requested for
\param usr_name the user name for this site. The allocated space for this buffer is 50 bytes. \note this varaibale may already be formatted.
\param password the password for this site and user. The allocated space for this buffer is 50 bytes.
\param async_pass async function to call back when user and pass have been entered. If NULL, sync call will be performed
\param async_udta async user data to pass back to the async function. If NULL, sync call will be performed
\return GF_FALSE if user didn't fill in the information which will result in an authentication failure, GF_TRUE otherwise (info was filled if not async, or request was posted).
*/
typedef Bool (*gf_dm_get_usr_pass)(void *usr_cbk, Bool secure, const char *site_url, char *usr_name, char *password, gf_dm_on_usr_pass async_pass, void *async_udta);

/*!
\brief password retrieval assignment

Assigns the callback function used for user password retrieval. If no such function is assigned to the download manager, all downloads requiring authentication will fail.
\param dm the download manager object
\param get_pass specifies \ref gf_dm_get_usr_pass callback function for user and password retrieval.
\param usr_cbk opaque user data passed to callback function
 */
void gf_dm_set_auth_callback(GF_DownloadManager *dm, gf_dm_get_usr_pass get_pass, void *usr_cbk);

/*!downloader session message types*/
typedef enum
{
	/*!signal that session is setup and waiting for connection request*/
	GF_NETIO_SETUP = 0,
	/*!signal that session connection is done*/
	GF_NETIO_CONNECTED,
	/*!request a protocol method from the user. Default value is "GET" for HTTP*/
	GF_NETIO_GET_METHOD,
	/*!request a header from the user. */
	GF_NETIO_GET_HEADER,
	/*!requesting content from the user, if any. Content is appended to the request*/
	GF_NETIO_GET_CONTENT,
	/*!signal that request is sent and waiting for server reply*/
	GF_NETIO_WAIT_FOR_REPLY,
	/*!signal a header to user. */
	GF_NETIO_PARSE_HEADER,
	/*!signal request reply to user. The reply is always sent after the headers*/
	GF_NETIO_PARSE_REPLY,
	/*!send data to the user*/
	GF_NETIO_DATA_EXCHANGE,
	/*!all data has been transfered*/
	GF_NETIO_DATA_TRANSFERED,
	/*!signal that the session has been deconnected*/
	GF_NETIO_DISCONNECTED,
	/*!downloader session failed (error code set) or done/destroyed (no error code)*/
	GF_NETIO_STATE_ERROR,
	/*!signal that a new session is being requested on that same connection (h2, h3)
	This is only used for server sessions*/
	GF_NETIO_REQUEST_SESSION,
	/*! stream has been canceled by remote peer*/
	GF_NETIO_CANCEL_STREAM,
} GF_NetIOStatus;

/*!session download flags*/
typedef enum
{
	/*!session is not threaded, the user must explicitly fetch the data , either with the function gf_dm_sess_fetch_data
	or the function gf_dm_sess_process- if the session is threaded, the user must call gf_dm_sess_process to start the session*/
	GF_NETIO_SESSION_NOT_THREADED = 1,
	/*! session data is cached or not */
	GF_NETIO_SESSION_NOT_CACHED = 1<<1,
	/*! forces data notification even when session is threaded*/
	GF_NETIO_SESSION_NOTIFY_DATA = 1<<2,
	/*indicates that the connection to the server should be kept once the download is successfully completed*/
	GF_NETIO_SESSION_PERSISTENT = 1<<3,
	/*file is stored in memory, and the cache name is set to gpac://%u@%p, where %d is the size in bytes and %d is the the pointer to the memory.
	Memory cached files are destroyed upon downloader destruction*/
	GF_NETIO_SESSION_MEMORY_CACHE = 1<<4,
	/*! do not delete files after download*/
	GF_NETIO_SESSION_KEEP_CACHE = 1<<5,
	/*! do not delete files after download of first resource (used for init segments)*/
	GF_NETIO_SESSION_KEEP_FIRST_CACHE = 1<<6,
	/*! session data is cached if content length is known */
	GF_NETIO_SESSION_AUTO_CACHE = 1<<7,
	/*! use non-blocking IOs*/
	GF_NETIO_SESSION_NO_BLOCK = 1<<8,
} GF_NetIOFlags;


/*!protocol I/O parameter*/
typedef struct
{
	/*!parameter message type*/
	GF_NetIOStatus msg_type;
	/*error code if any. Valid for all message types.*/
	GF_Err error;
	/*!data received or data to send. Only valid for GF_NETIO_GET_CONTENT and GF_NETIO_DATA_EXCHANGE (when no cache is setup) messages*/
	const u8 *data;
	/*!size of associated data. Only valid for GF_NETIO_GET_CONTENT and GF_NETIO_DATA_EXCHANGE messages*/
	u32 size;
	/*protocol header. Only valid for GF_NETIO_GET_HEADER, GF_NETIO_PARSE_HEADER and GF_NETIO_GET_METHOD*/
	const char *name;
	/*protocol header value or server response. Only alid for GF_NETIO_GET_HEADER, GF_NETIO_PARSE_HEADER and GF_NETIO_PARSE_REPLY*/
	char *value;
	/*message-dependend
		for GF_NETIO_PARSE_REPLY, response code
		for GF_NETIO_DATA_EXCHANGE
			Set to 1 in to indicate end of chunk transfer
			Set to 2 in GF_NETIO_DATA_EXCHANGE to indicate complete file is already received (replay of events from cache)
		for all other, usage is reserved
	*/
	u32 reply;
	/*download session for which the message is being sent*/
	GF_DownloadSession *sess;
} GF_NETIO_Parameter;

/*!
\brief callback function for data reception and state signaling

The gf_dm_user_io type is the type for the data callback function of a download session
\param usr_cbk opaque user data
\param parameter the input/output parameter structure
*/
typedef void (*gf_dm_user_io)(void *usr_cbk, GF_NETIO_Parameter *parameter);



/*!
\brief download session constructor

Creates a new download session
\param dm the download manager object
\param url file to retrieve (no PUT/POST yet, only downloading is supported)
\param dl_flags combination of session download flags
\param user_io specifies \ref gf_dm_user_io callback function for data reception and service messages
\param usr_cbk opaque user data passed to callback function
\param error error for failure cases
\return the session object or NULL if error. If no error is indicated and a NULL session is returned, this means the file is local
 */
GF_DownloadSession * gf_dm_sess_new(GF_DownloadManager *dm, const char *url, u32 dl_flags,
                                    gf_dm_user_io user_io,
                                    void *usr_cbk,
                                    GF_Err *error);

/*!
\brief download session simple constructor

Creates a new download session
\param dm The download manager used to create the download session
\param url file to retrieve (no PUT/POST yet, only downloading is supported)
\param dl_flags combination of session download flags
\param user_io specifies \ref gf_dm_user_io callback function for data reception and service messages
\param usr_cbk opaque user data passed to callback function
\param e error for failure cases
\return the session object or NULL if error. If no error is indicated and a NULL session is returned, this means the file is local
 */
GF_DownloadSession *gf_dm_sess_new_simple(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io,
        void *usr_cbk,
        GF_Err *e);

/*!
 \brief downloader session destructor

Deletes the download session, cleaning the cache if indicated in the configuration file of the download manager (section "Downloader", key "CleanCache")
\param sess the download session
*/
void gf_dm_sess_del(GF_DownloadSession * sess);

/*!
\brief aborts downloading

Aborts all operations in the session, regardless of its state. The session cannot be reused once this is called.
\param sess the download session
 */
void gf_dm_sess_abort(GF_DownloadSession * sess);

/*!
\brief gets last session error

Gets the last error that occured in the session
\param sess the download session
\return the last error
 */
GF_Err gf_dm_sess_last_error(GF_DownloadSession *sess);


/*!
\brief fetches data on session

Fetches data from the server. This will also performs connections and all needed exchange with server.
\param sess the download session
\param buffer destination buffer
\param buffer_size destination buffer allocated size
\param read_size amount of data actually fetched
\note this can only be used when the session is not threaded
\return error if any
 */
GF_Err gf_dm_sess_fetch_data(GF_DownloadSession * sess, char *buffer, u32 buffer_size, u32 *read_size);

/*!
\brief get mime type as lower case

Fetches the mime type of the URL this session is fetching, value will be returned lower case, so application/x-mpegURL will be returned as application/x-mpegurl
\param sess the download session
\return the mime type of the URL, or NULL if error. You should get the error with \ref gf_dm_sess_last_error
 */
const char *gf_dm_sess_mime_type(GF_DownloadSession * sess);

/*!
\brief sets session range

Sets the session byte range. This shll be called before processing the session.
\param sess the download session
\param start_range HTTP download start range in byte
\param end_range HTTP download end range in byte
\param discontinue_cache If set, forces a new cache entry if byte range are not continuous. Otherwise a single cache entry is used to reconstruct the file
\note this can only be used when the session is not threaded
\return error if any
 */
GF_Err gf_dm_sess_set_range(GF_DownloadSession *sess, u64 start_range, u64 end_range, Bool discontinue_cache);
/*!
\brief get cache file name

Gets the cache file name for the session.
\param sess the download session
\return the absolute path of the cache file, or NULL if the session is not cached*/
const char *gf_dm_sess_get_cache_name(GF_DownloadSession * sess);

/*!
\brief Marks the cache file to be deleted once the file is not used anymore by any session
\param dm the download manager
\param url The URL associate to the cache entry to be deleted
 */
void gf_dm_delete_cached_file_entry(const GF_DownloadManager * dm, const char * url);

/*!
Marks the cache file for this session to be deleted once the file is not used anymore by any session
\param sess the download session
\param url The URL associate to the cache entry to be deleted
 */
void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess, const char * url);

/*!
\brief get statistics

Gets download statistics for the session. All output parameters are optional and may be set to NULL.
\param sess the download session
\param server the remote server address
\param path the path on the remote server
\param total_size the total size in bytes the file fetched, 0 if unknown.
\param bytes_done the amount of bytes received from the server
\param bytes_per_sec the average data rate in bytes per seconds
\param net_status the session status
\return error if any
 */
GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u64 *total_size, u64 *bytes_done, u32 *bytes_per_sec, GF_NetIOStatus *net_status);

/*!
\brief get start time

Gets session start time in UTC. If chunk-transfer is used, the start time is reset at each chunk start
\param sess the download session
\return UTC start time
 */
u64 gf_dm_sess_get_utc_start(GF_DownloadSession *sess);


/*!
\brief fetch session object

Fetches the session object (process all headers and data transfer). This is only usable if the session is not threaded
\param sess the download session
\return the last error in the session or 0 if none*/
GF_Err gf_dm_sess_process(GF_DownloadSession *sess);

/*!
\brief fetch session object headers

Fetch the session object headers and stops after that. This is only usable if the session is not threaded
\param sess the download session
\return the last error in the session or 0 if none*/
GF_Err gf_dm_sess_process_headers(GF_DownloadSession * sess);

/*!
\brief Get session resource url

Returns the original resource URL associated with the session
\param sess the download session
\return the associated URL
 */
const char *gf_dm_sess_get_resource_name(GF_DownloadSession *sess);
/*!
Downloads a file over the network using a download manager
\param dm The download manager to use, function will use all associated cache resources
\param url The url to download
\param filename The filename to download
\param start_range start position of a byte range
\param end_range end position of a byte range
\param redirected_url If not NULL, \p redirected_url will be allocated and filled with the URL after redirection. Caller takes ownership
\return GF_OK if everything went fine, an error otherwise
 */
GF_Err gf_dm_wget_with_cache(GF_DownloadManager * dm, const char *url, const char *filename, u64 start_range, u64 end_range, char **redirected_url);

/*!
\brief Same as gf_dm_wget_with_cache, but initializes the GF_DownloadManager by itself.

This function is deprecated, please use gf_dm_wget_with_cache instead
\param url The url to download
\param filename The filename to download
\param start_range start position of a byte range
\param end_range end position of a byte range
\param redirected_url If not NULL, \p redirected_url will be allocated and filled with the URL after redirection. Caller takes ownership
\return GF_OK if everything went fine, an error otherwise
 */
GF_Err gf_dm_wget(const char *url, const char *filename, u64 start_range, u64 end_range, char **redirected_url);

/*!
Re-setup an existing, completed session to download a new URL. If same server/port/protocol is used, the same socket will be reused if the session has the GF_NETIO_SESSION_PERSISTENT flag set. This is only possible if the session is not threaded.
\param sess The session
\param url The new url for the session
\param allow_direct_reuse Allow reuse of cache entry without checking cache directives
\return GF_OK or error
 */
GF_Err gf_dm_sess_setup_from_url(GF_DownloadSession *sess, const char *url, Bool allow_direct_reuse);

/*!
\brief retrieves the HTTP header value for the given name

Retrieves the HTTP header value for the given header name.
\param sess the current session
\param name the target header name
\return header value or NULL if not found
 */
const char *gf_dm_sess_get_header(GF_DownloadSession *sess, const char *name);

/*!
\brief enumerates the  HTTP headers for a session

Retrieves the HTTP header name and value for the given header index.
\param sess the current session
\param idx index for the enumeration, must be initialized to 0 for the first call
\param hdr_name where name of header is stored - do not free
\param hdr_val where value of header is stored - do not free
\return error code, GF_OK or GF_EOS if no more headers
 */
GF_Err gf_dm_sess_enum_headers(GF_DownloadSession *sess, u32 *idx, const char **hdr_name, const char **hdr_val);

/*!
\brief sets download manager max rate per session

Sets the maximum rate (per session only at the current time).
\param dm the download manager object
\param rate_in_bits_per_sec the new rate in bits per sec. If 0, HTTP rate will not be limited
 */
void gf_dm_set_data_rate(GF_DownloadManager *dm, u32 rate_in_bits_per_sec);

/*!
\brief gets download manager max rate per session

Sets the maximum rate (per session only at the current time).
\param dm the download manager object
\return the rate in bits per sec. If 0, HTTP rate is not limited
 */
u32 gf_dm_get_data_rate(GF_DownloadManager *dm);


/*!
\brief gets cumultaed download rate for all sessions

Gets the cumultated bitrate in of all active sessions.
\param dm the download manager object
\return the cumulated rate in bits per sec.
 */
u32 gf_dm_get_global_rate(GF_DownloadManager *dm);


/*!
\brief Get header sizes and times stats for the session

Get header sizes and times stats for the session
\param sess the current session
\param req_hdr_size request header size in bytes. May be NULL.
\param rsp_hdr_size response header size in bytes. May be NULL.
\param connect_time connection time in micro seconds. May be NULL.
\param reply_time elapsed time between request sent and response header received, in micro seconds. May be NULL.
\param download_time download time since request sent, in micro seconds. May be NULL.
\return error code if any
 */
GF_Err gf_dm_sess_get_header_sizes_and_times(GF_DownloadSession *sess, u32 *req_hdr_size, u32 *rsp_hdr_size, u32 *connect_time, u32 *reply_time, u32 *download_time);

/*!
\brief Forces session to use memory storage

Forces session to use memory storage for future downloads
\param sess the current session
\param force_cache_type if 1, cache will be kept even if session is reassigned. If 2, cache will ne kept for next resource downloaded, then no caching for subsequent resources (used for init segments)
 */
void gf_dm_sess_force_memory_mode(GF_DownloadSession *sess, u32 force_cache_type);

/*!
Registers a local cache provider (bypassing the http session), used when populating cache from input data (ROUTE for example)

\param dm the download manager
\param local_cache_url_provider_cbk callback function to the cache provider. The callback function shall return GF_TRUE if the requested URL is provided by this local cache
\param lc_udta opaque pointer passed to the callback function
\return error code if any
 */
GF_Err gf_dm_set_localcache_provider(GF_DownloadManager *dm, Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool is_cache_destroy), void *lc_udta);

/*!
Adds a local entry in the cache

\param dm the download manager
\param szURL the URL this resource is caching
\param blob blob object holding the data of the resource
\param start_range start range of the data in the resource
\param end_range start range of the data in the resource. If both start_range and end_range are 0, the data is the complete resource
\param mime associated MIME type if any
\param clone_memory indicates that the data shall be cloned in the cache because the caller will discard it
\param download_time_ms indicates the download time of the associated resource, if known, 0 otherwise.
\return a cache entry structure
 */
DownloadedCacheEntry gf_dm_add_cache_entry(GF_DownloadManager *dm, const char *szURL, GF_Blob *blob, u64 start_range, u64 end_range,  const char *mime, Bool clone_memory, u32 download_time_ms);

/*!
Forces HTTP headers for a given cache entry

\param dm the download manager
\param entry the cache entry to update
\param headers the header string, including CRLF delimiters, to force
\return error code if any
*/
GF_Err gf_dm_force_headers(GF_DownloadManager *dm, const DownloadedCacheEntry entry, const char *headers);

/*! HTTP methods*/
enum
{
	/*! unsupported*/
	GF_HTTP_UNKNOWN = 0,
	/*! GET*/
	GF_HTTP_GET,
	/*! HEAD*/
	GF_HTTP_HEAD,
	/*! OPTIONS*/
	GF_HTTP_OPTIONS,
	/*! CONNECT*/
	GF_HTTP_CONNECT,
	/*! TRACE*/
	GF_HTTP_TRACE,
	/*! PUT*/
	GF_HTTP_PUT,
	/*! POST*/
	GF_HTTP_POST,
	/*! DELETE*/
	GF_HTTP_DELETE
};

/*! User credential request state*/
typedef enum
{
	//! no async request for this credential object
	GF_CREDS_STATE_NONE=0,
	//!  async request pending for this credential object (waiting for user input)
	GF_CREDS_STATE_PENDING,
	//! async request done for this credential object
	GF_CREDS_STATE_DONE,
} GF_CredentialRequestState;

/*! user credential structure - all fields are setup by the credentials functions

Credentials are currently handled indepentently from protocol scheme
*/
typedef struct
{
	/*! parent download manager where credentials are stored*/
	const GF_DownloadManager *dm;
	/*! site name*/
	char site[1024];
	/*! user name*/
	char username[50];
	/*! digest (only Basic for now, "Basic " not included in digest)*/
	char digest[1024];
	/*! indicate if the credentials are valid or not*/
	Bool valid;
	/*! indicate that the async state of the credential request*/
	GF_CredentialRequestState req_state;
} GF_UserCredentials;

/*! Find credentials for given site and user
 \param dm parent download manager
 \param server_name sever name without protocol scheme - must not be NULL
 \param user_name user name, can be NULL (will pick first credential for the site)
 \return credential object or NULL if error
*/
GF_UserCredentials *gf_user_credentials_find_for_site(GF_DownloadManager *dm, const char *server_name, const char *user_name);

/*! Register credentials for given site and user/pass
 \param dm parent download manager
 \param secure indicate if connection is over TLS
 \param server_name sever name without protocol scheme - must not be NULL
 \param username user name, must not be NULL
 \param password user password, must not be NULL
 \param valid indicates if credentials are valid (successfull authentication)
 \return credential object or NULL if error
*/
GF_UserCredentials * gf_user_credentials_register(GF_DownloadManager * dm, Bool secure, const char * server_name, const char * username, const char * password, Bool valid);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_DOWNLOAD_H_*/

