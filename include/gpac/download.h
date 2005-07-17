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

/*!downloader session status*/
enum
{
	/*!setup and waiting for connection request*/
	GF_DOWNLOAD_STATE_SETUP = 0,
	/*!connection is done*/
	GF_DOWNLOAD_STATE_CONNECTED,
	/*!waiting for server reply*/
	GF_DOWNLOAD_STATE_WAIT_FOR_REPLY,
	/*!data exchange on this downloader*/
	GF_DOWNLOAD_STATE_RUNNING,
	/*!deconnection OK */
	GF_DOWNLOAD_STATE_DISCONNECTED,
	/*!downloader session failed or destroyed*/
	GF_DOWNLOAD_STATE_UNAVAILABLE
};

/*!session download flags*/
enum
{
	/*!session is not threaded, the user must retrieve the data by hand*/
	GF_DOWNLOAD_SESSION_NOT_THREADED	=	1,
	/*!session has no cache: data will be sent to the user if threaded mode (live streams like radios & co)*/
	GF_DOWNLOAD_SESSION_NOT_CACHED	=	1<<1,
};


/*!
 *\brief callback function for data reception and state signaling
 *
 * The gf_dm_on_data_rcv type is the type for the data callback function of a download session
 *\param usr_cbk opaque user data
 *\param data 
	- buffer just received if no cache is used
	- NULL if cache is used
	- NULL if session is not threaded
	- NULL if message callback
 *\param data_size
	- size of buffer just received
	- 0 if message callback or if session is not threaded
 *\param dnload_status session state for message callback
 *\param dl_error session error if any
*/
typedef void (*gf_dm_on_data_rcv)(void *usr_cbk, char *data, u32 data_size, u32 dnload_status, GF_Err dl_error);

/*!
 *\brief download session constructor
 *
 *Creates a new download session
 *\param dm the download manager object
 *\param url file to retrieve (no PUT/POST yet, only downloading is supported)
 *\param dl_flags combination of session download flags
 *\param OnDataRcv \ref gf_dm_on_data_rcv callback function for data reception and service messages
 *\param usr_cbk opaque user data passed to callback function
 *\param private_data private data associated with session.
 *\param error error for failure cases 
 *\return the session object or NULL if error. If no error is indicated and a NULL session is returned, this means the file is local
 *\warning the private_data parameter is reserved for bandwidth statistics per service when used in the GPAC terminal.
 */
GF_DownloadSession * gf_dm_sess_new(GF_DownloadManager *dm, char *url, u32 dl_flags,
									  gf_dm_on_data_rcv OnDataRcv,
									  void *usr_cbk,
									  void *private_data,
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

