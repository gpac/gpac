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

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>
#include <gpac/module.h>


/*download manager*/
typedef struct __gf_download_manager GF_DownloadManager;
typedef struct __gf_download_session GF_DownloadSession;

/*creates a new download manager
@cfg: configuration file (needed for cache and other options)
*/
GF_DownloadManager *gf_dm_new(GF_Config *cfg);
/*deletes download manager - all current sessions are aborted*/
void gf_dm_delete(GF_DownloadManager *dm);

/*sets callback for password retrieval
@GetUserPassword: function used to retrieve user&pass for a given site. the user and pass buffer passsed to 
this function are 50 bytes long.
@usr_cbk: user data passed back to GetUserPassword
*/
void gf_dm_set_auth_callback(GF_DownloadManager *dm,
							  Bool (*GetUserPassword)(void *usr_cbk, const char *site_url, char *usr_name, char *password), 
							  void *usr_cbk);

/*downloader session status*/
enum
{
	/*setup and waits for connection request*/
	GF_DOWNLOAD_STATE_SETUP = 0,
	/*connection OK*/
	GF_DOWNLOAD_STATE_CONNECTED,
	/*waiting for server reply*/
	GF_DOWNLOAD_STATE_WAIT_FOR_REPLY,
	/*data exchange on this downloader*/
	GF_DOWNLOAD_STATE_RUNNING,
	/*deconnection OK */
	GF_DOWNLOAD_STATE_DISCONNECTED,
	/*downloader session failed or destroyed*/
	GF_DOWNLOAD_STATE_UNAVAILABLE
};

/*session download flags*/
enum
{
	/*session is not threaded, the user must retrieve the data by hand*/
	GF_DOWNLOAD_SESSION_NOT_THREADED	=	1,
	/*session has no cache: data will be sent to the user if threaded mode (live streams like radios & co)*/
	GF_DOWNLOAD_SESSION_NOT_CACHED	=	1<<1,
};

/*creates a new download session
@dm: download manager
@url: file to retrieve (no PUT/POST yet, only download supported)
@dl_flags: one of the above download flags
@OnDataRcv, @usr_cbk: callback function and user data for communication & co:
	void (*OnDataRcv)(void *usr_cbk, char *data, u32 data_size, u32 dnload_status, GF_Err dl_error)
		@data, @data_size: buffer just received if no cache is used
		@dnload_status: session state (one of the above)
		@dl_error: session error if any. It may alse be an GF_SCRIPT_INFO error, in which case the data send is
	some meta info (only used with SHOUTcast and ICEcast live mp3 servers)

@private_data: usually NULL, unless you need to store a pointer which is not the main callback (mainly 
used for bandwidth management)
@error: error for failure cases - if no error and NULL session is returned, this means the file is
local
*/
GF_DownloadSession * gf_dm_new_session(GF_DownloadManager *dm, char *url, u32 dl_flags,
									  void (*OnDataRcv)(void *usr_cbk, char *data, u32 data_size, u32 dnload_status, GF_Err dl_error),
									  void *usr_cbk,
									  void *private_data,
									  GF_Err *error);

/*deletes downloader session*/
void gf_dm_free_session(GF_DownloadSession * sess);
/*aborts all operations*/
void gf_dm_abort(GF_DownloadSession * sess);
/*gets private data*/
void *gf_dm_get_private_data(GF_DownloadSession * sess);

GF_Err gf_dm_get_last_error(GF_DownloadSession *sess);

/*fetches data - this will also performs connections and all needed exchange with server.
can only be used when not threaded*/
GF_Err gf_dm_fetch_data(GF_DownloadSession * sess, char *buffer, u32 buffer_size, u32 *read_size);

/*get (fetches it if needed) mime type for given session*/
const char *gf_dm_get_mime_type(GF_DownloadSession * sess);
/*get cache file name, NULL if no cache*/
const char *gf_dm_get_cache_name(GF_DownloadSession * sess);
/*gets stats - all paramls are optional*/
GF_Err gf_dm_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u32 *total_size, u32 *bytes_done, u32 *bytes_per_sec, u32 *net_status);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_DOWNLOAD_H_*/

