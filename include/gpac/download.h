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

#ifndef _GF_DOWNLOAD_H_
#define _GF_DOWNLOAD_H_

/*!
 *	\file <gpac/download.h>
 *	\brief Downloader functions.
 */

/*!
 *	\addtogroup dld_grp downloader
 *	\ingroup utils_grp
 *	\brief File Downloader objects
 *
 *	This section documents the file downloading tools the GPAC framework. Currently HTTP is supported, HTTPS is under testing but may not be supported
 *depending on GPAC compilation options (HTTPS in GPAC needs OpenSSL installed on the system).
 *
 *	@{
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>
#include <gpac/module.h>


/*!the download manager object. This is usually not used by GPAC modules*/
typedef struct __gf_download_manager GF_DownloadManager;
/*!the download manager session.*/
typedef struct __gf_download_session GF_DownloadSession;

/*!
 *\brief download manager constructor
 *
 *Creates a new download manager object.
 *\param cfg optional configuration file. Currently the download manager needs a configuration file for cache location and 
 *other options. The cache directory must be indicated in the section "General", key "CacheDirectory" of the configuration
 *file. If the cache directory is not found, the cache will be disabled but the downloader will still work.
 *\return the download manager object
*/
GF_DownloadManager *gf_dm_new(GF_Config *cfg);
/*
 *\brief download manager destructor
 *
 *Deletes the download manager. All running sessions are aborted
 *\param dm the download manager object
 */
void gf_dm_del(GF_DownloadManager *dm);

/*!
 *\brief callback function for authentication
 *
 * The gf_dm_get_usr_pass type is the type for the callback of the \ref gf_dm_set_auth_callback function used for password retrieval
 *\param usr_cbk opaque user data
 *\param site_url url of the site the user and password are requested for
 *\param usr_name the user name for this site. The allocated space for this buffer is 50 bytes. \note this varaibale may already be formatted.
 *\param password the password for this site and user. The allocated space for this buffer is 50 bytes.
 *\return 0 if user didn't fill in the information which will result in an authentication failure, 1 otherwise.
*/
typedef Bool (*gf_dm_get_usr_pass)(void *usr_cbk, const char *site_url, char *usr_name, char *password);

/*!
 *\brief password retrieval assignment
 *
 *Assigns the callback function used for user password retrieval. If no such function is assigned to the download manager, 
 *all downloads requiring authentication will fail.
 *\param dm the download manager object
 *\param get_pass \ref gf_dm_get_usr_pass callback function for user and password retrieval. 
 *\param usr_cbk opaque user data passed to callback function
 */
void gf_dm_set_auth_callback(GF_DownloadManager *dm, gf_dm_get_usr_pass get_pass, void *usr_cbk);

/*!downloader session message types*/
enum
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
	GF_NETIO_STATE_ERROR
};

/*!session download flags*/
enum
{
	/*!session is not threaded, the user must explicitely fetch the data */
	GF_NETIO_SESSION_NOT_THREADED	=	1,
	/*!session has no cache: data will be sent to the user if threaded mode (live streams like radios & co)*/
	GF_NETIO_SESSION_NOT_CACHED	=	1<<1,
};


/*!protocol I/O parameter*/
typedef struct
{
	/*!parameter message type*/
	u32 msg_type;
	/*error code if any. Valid for all message types.*/
	GF_Err error;
	/*!data received or data to send. Only valid for GF_NETIO_GET_CONTENT and GF_NETIO_DATA_EXCHANGE (when no cache is setup) messages*/
	char *data;
	/*!size of associated data. Only valid for GF_NETIO_GET_CONTENT and GF_NETIO_DATA_EXCHANGE messages*/
	u32 size;
	/*protocol header. Only valid for GF_NETIO_GET_HEADER, GF_NETIO_PARSE_HEADER and GF_NETIO_GET_METHOD*/
	char *name;
	/*protocol header value or server response. Only alid for GF_NETIO_GET_HEADER, GF_NETIO_PARSE_HEADER and GF_NETIO_PARSE_REPLY*/
	char *value;
	/*response code - only valid for GF_NETIO_PARSE_REPLY*/
	u32 reply;
} GF_NETIO_Parameter;

/*!
 *\brief callback function for data reception and state signaling
 *
 * The gf_dm_user_io type is the type for the data callback function of a download session
 *\param usr_cbk opaque user data
 *\param parameter the input/output parameter structure 
*/
typedef void (*gf_dm_user_io)(void *usr_cbk, GF_NETIO_Parameter *parameter);



/*!
 *\brief download session constructor
 *
 *Creates a new download session
 *\param dm the download manager object
 *\param url file to retrieve (no PUT/POST yet, only downloading is supported)
 *\param dl_flags combination of session download flags
 *\param user_io \ref gf_dm_user_io callback function for data reception and service messages
 *\param usr_cbk opaque user data passed to callback function
 *\param error error for failure cases 
 *\return the session object or NULL if error. If no error is indicated and a NULL session is returned, this means the file is local
 */
GF_DownloadSession * gf_dm_sess_new(GF_DownloadManager *dm, char *url, u32 dl_flags,
									  gf_dm_user_io user_io,
									  void *usr_cbk,
									  GF_Err *error);

/*!
 *brief downloader session destructor
 *
 *Deletes the download session, cleaning the cache if indicated in the configuration file of the download manager (section "Downloader", key "CleanCache")
 *\param sess the download session
*/
void gf_dm_sess_del(GF_DownloadSession * sess);
/*!
 *\brief aborts downloading
 *
 *Aborts all operations in the session, regardless of its state. The session cannot be reused once this is called.
 *\param sess the download session
 */
void gf_dm_sess_abort(GF_DownloadSession * sess);
/*!
 *\brief sets private data
 *
 *associate private data with the session.
 *\param sess the download session
 *\param private_data the private data
 *\warning the private_data parameter is reserved for bandwidth statistics per service when used in the GPAC terminal.
 */
void gf_dm_sess_set_private(GF_DownloadSession * sess, void *private_data);

/*!
 *\brief gets private data
 *
 *Gets private data associated with the session.
 *\param sess the download session
 *\return the private data
 *\warning the private_data parameter is reserved for bandwidth statistics per service when used in the GPAC terminal.
 */
void *gf_dm_sess_get_private(GF_DownloadSession * sess);
/*!
 *\brief gets last session error 
 *
 *Gets the last error that occured in the session
 *\param sess the download session
 *\return the last error
 */
GF_Err gf_dm_sess_last_error(GF_DownloadSession *sess);

/*!
 *\brief fetches data on session
 *
 *Fetches data from the server. This will also performs connections and all needed exchange with server.
 *\param sess the download session
 *\param buffer destination buffer
 *\param buffer_size destination buffer allocated size
 *\param read_size amount of data actually fetched
 *\note this can only be used when the session is not threaded
 */
GF_Err gf_dm_sess_fetch_data(GF_DownloadSession * sess, char *buffer, u32 buffer_size, u32 *read_size);

/*!
 *\brief get mime type 
 *
 *Fetches the mime type of the URL this session is fetching
 *\param sess the download session
 *\return the mime type of the URL, or NULL if error. You should get the error with \ref gf_dm_sess_last_error
 */
const char *gf_dm_sess_mime_type(GF_DownloadSession * sess);
/*!
 *\brief get cache file name
 *
 *Gets the cache file name for the session.
 *\param sess the download session
 *\return the absolute path of the cache file, or NULL if the session is not cached*/
const char *gf_dm_sess_get_cache_name(GF_DownloadSession * sess);
/*!
 *\brief get statistics
 *
 *Gets download statistics for the session. All output parameters are optional and may be set to NULL.
 *\param sess the download session
 *\param server the remote server address
 *\param path the path on the remote server
 *\param total_size the total size in bytes the file fetched, 0 if unknown.
 *\param bytes_done the amount of bytes received from the server
 *\param bytes_per_sec the average data rate in bytes per seconds
 *\param net_status the session status
 */
GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u32 *total_size, u32 *bytes_done, u32 *bytes_per_sec, u32 *net_status);


/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_DOWNLOAD_H_*/

