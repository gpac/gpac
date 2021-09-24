/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / Stream Management sub-project
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



#ifndef _GF_TERM_INFO_H_
#define _GF_TERM_INFO_H_


#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/term_info.h>
\brief GPAC media object browsing of the player.
*/
	
/*!
\addtogroup terminfo_grp Terminal Info
\ingroup playback_grp
\brief GPAC media object browsing of the player.

This section documents how a user can get information on running media object in the player.

@{
 */
	

/*
	OD Browsing API - YOU MUST INCLUDE <gpac/terminal.h> before
	(this has been separated from terminal.h to limit dependency of core to mpeg4_odf.h header)
	ALL ITEMS ARE READ-ONLY AND SHALL NOT BE MODIFIED
*/
#include <gpac/mpeg4_odf.h>
#include <gpac/terminal.h>

/*! Media object manager*/
typedef struct _od_manager GF_ObjectManager;

/*! gets top-level OD of the presentation
\param term the target terminal
\return the root object descriptor or NULL if not connected*/
GF_ObjectManager *gf_term_get_root_object(GF_Terminal *term);

/*! gets number of objects in sub scene of object
\param term the target terminal
\param scene_od the OD to browse for resources. This must be an inline OD
\return the number of objects in the scene described by the OD*/
u32 gf_term_get_object_count(GF_Terminal *term, GF_ObjectManager *scene_od);

/*! gets an OD manager in the scene
\param term the target terminal
\param scene_od the OD to browse for resources. This must be an inline OD
\param index 0-based index of the obhect to query
\return the object manager or NULL if not found
*/
GF_ObjectManager *gf_term_get_object(GF_Terminal *term, GF_ObjectManager *scene_od, u32 index);

/*! gets the type of the associated subscene
\param term the target terminal
\param odm the object to query
\return	0: regular media object, not inline, 1: root scene, 2: inline scene, 3: externProto library
*/
u32 gf_term_object_subscene_type(GF_Terminal *term, GF_ObjectManager *odm);

/*! selects a given object when stream selection is available
\param term the target terminal
\param odm the object to select
*/
void gf_term_select_object(GF_Terminal *term, GF_ObjectManager *odm);

/*! selects service by given ID for multiplexed services (MPEG-2 TS)
\param term the target terminal
\param odm an object in the scene attached to the same multiplex
\param service_id the service ID to select
*/
void gf_term_select_service(GF_Terminal *term, GF_ObjectManager *odm, u32 service_id);

/*! sets addon on or off (only one addon possible for now). When OFF, the associated service is shut down
\param term the target terminal
\param show_addons if GF_TRUE, displays addons otheriwse hides the addons
*/
void gf_term_toggle_addons(GF_Terminal *term, Bool show_addons);

/*! checks for an existing service ID in a session
\param term the target terminal
\param odm an object in the scene attached to the same multiplex
\param service_id the ID of the service to find
\return GF_TRUE if given service ID is declared*/
Bool gf_term_find_service(GF_Terminal *term, GF_ObjectManager *odm, u32 service_id);

/*! Media Object information*/
typedef struct
{
	u32 ODID;
	u32 ServiceID;
	u32 pid_id, ocr_id;
	Double duration;
	Double current_time;
	/*0: stopped, 1: playing, 2: paused, 3: not setup, 4; setup failed.*/
	u32 status;

	Bool raw_media;
	Bool generated_scene;

	/*name of module handling the service service */
	const char *service_handler;
	/*name of service*/
	const char *service_url;
	/*service redirect object*/
	const char *remote_url;
	/*set if the service is owned by this object*/
	Bool owns_service;

	/*stream buffer:
		-2: stream is not playing
		-1: stream has no buffering
		>=0: amount of media data present in buffer, in ms
	*/
	s32 buffer;
	u32 min_buffer, max_buffer;
	/*number of AUs in DB (cumulated on all input channels)*/
	u32 db_unit_count;
	/*number of CUs in composition memory (if any) and CM capacity*/
	u32 cb_unit_count, cb_max_count;
	/*inidciate that thye composition memory is bypassed for this decoder (video only) */
	Bool direct_video_memory;
	/*clock delay in ms of object clock: this is the delay set by the audio renderer to keep AV in sync
	and corresponds to the amount of ms to delay non-audio streams to keep sync*/
	s32 clock_drift;
	/*codec name*/
	const char *codec_name;
	/*object type - match streamType (cf constants.h)*/
	u32 od_type;
	/*audio properties*/
	u32 sample_rate, afmt, num_channels;
	/*video properties (w & h also used for scene codecs)*/
	u32 width, height, pixelFormat, par;

	/*average birate over last second and max bitrate over one second at decoder input according to media timeline - expressed in bits per sec*/
	u32 avg_bitrate, max_bitrate;
	/*average birate over last second and max bitrate over one second at decoder input according to processing time - expressed in bits per sec*/
	u32 avg_process_bitrate, max_process_bitrate;
	u32 nb_dec_frames, nb_dropped;
	u32 first_frame_time, last_frame_time;
	u64 total_dec_time, irap_total_dec_time;
	u32 max_dec_time, irap_max_dec_time;
	u32 au_duration;
	u32 nb_iraps;
	s32 ntp_diff;
	//0 if mono, 2 or more otherwise
	u32 nb_views;

	/*4CC of original protection scheme used*/
	u32 protection;

	u32 lang;
	const char *lang_code;

	/*name of media if not defined in OD framework*/
	const char *media_url;
} GF_MediaInfo;

/*! gets information on an object
\param term the target terminal
\param odm the object manager to query
\param info filled with object manager properties
\return error if any
*/
GF_Err gf_term_get_object_info(GF_Terminal *term, GF_ObjectManager *odm, GF_MediaInfo *info);

/*! gets current downloads info for the service - only use if ODM owns the service, returns 0 otherwise.
\param term the target terminal
\param odm the object manager to query
\param d_enum in/out current enum - shall start to 0, incremented at each call. fct returns 0 if no more downloads
\param url service URL
\param bytes_done bytes downloaded for this service
\param total_bytes total bytes for this service, may be 0 (eg http streaming)
\param bytes_per_sec download speed
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_term_get_download_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, const char **url, u32 *bytes_done, u32 *total_bytes, u32 *bytes_per_sec);

/*! Net statistics gathering object*/
typedef struct
{
	/*percentage of packet loss from network. This cannot be figured out by the app since there is no
	one-to-one mapping between the protocol packets and the final SL packet (cf RTP payloads)*/
	Float pck_loss_percentage;
	/*channel port, control channel port if any (eg RTCP)*/
	u16 port, ctrl_port;
	/*bandwidth used by channel & its control channel if any (both up and down) - expressed in bits per second
	for HTTP connections, typically only bw_down is used*/
	u32 bw_up, bw_down, ctrl_bw_down, ctrl_bw_up;
	/*set to 0 if channel is not part of a multiplex. Otherwise set to the multiplex port, and
	above port info shall be identifiers in the multiplex - note that multiplexing overhead is ignored
	in GPAC for the current time*/
	u16 multiplex_port;
} GF_TermNetStats;

/*! gets network statistics for the given channel in the given object
\param term the target terminal
\param odm the object manager to query
\param d_enum in/out current enum - shall start to 0, incremented at each call. fct returns 0 if no more downloads
\param chid set to the channel identifier
\param net_stats filled with the channel network statistics
\param ret_code set to the error code if error
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_term_get_channel_net_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, u32 *chid, GF_TermNetStats *net_stats, GF_Err *ret_code);

/*! URL information for current session*/
typedef struct
{
	u32 service_id;
	u16 track_num;
	u16 track_total;
	u32 genre;
	const char *album;
	const char *artist;
	const char *comment;
	const char *composer;
	const char *name;
	const char *writer;
	const char *provider;
	//as in MPEG_DASH role
	const char *role;
	const char *accessibility;
	const char *rating;
} GF_TermURLInfo;
/*! gets meta information about the service
\param term the target terminal
\param odm the object manager to query
\param info filled with service information
\return error if any
*/
GF_Err gf_term_get_service_info(GF_Terminal *term, GF_ObjectManager *odm, GF_TermURLInfo *info);

/*! retrieves world info of the scene of an object.
\param term the target terminal
\param scene_od the object manager to query. If this is an inline OD, the world info of the inline content is retrieved. If NULL, the world info of the main scene is retrieved
\param descriptions any textual descriptions is stored here (const strings not allocated, do NOT modify)
\return NULL if no WorldInfo available or world title if available
*/
const char *gf_term_get_world_info(GF_Terminal *term, GF_ObjectManager *scene_od, GF_List *descriptions);

/*! dumps scene graph in specified file, in BT or XMT format
\param term the target terminal
\param rad_name file radical (NULL for stdout) - if not NULL MUST BE GF_MAX_PATH length
\param filename sets to the complete filename (rad + ext) and shall be destroyed by caller (optional can be NULL)
\param xml_dump if GF_TRUE, duimps using XML format (XMT-A, X3D) for scene graphs having both XML and simple text representations
\param skip_proto is GF_TRUE, proto declarations are not dumped
\param odm if this is an inline OD, the inline scene is dumped; if this is NULL, the main scene is dumped; otherwise the parent scene is dumped
\return error if any
*/
GF_Err gf_term_dump_scene(GF_Terminal *term, char *rad_name, char **filename, Bool xml_dump, Bool skip_proto, GF_ObjectManager *odm);

/*! prints filter session statistics as log tool app log level debuf
\param term the target terminal
*/
void gf_term_print_stats(GF_Terminal *term);

/*! prints filter session graph as log tool app log level debuf
\param term the target terminal
*/
void gf_term_print_graph(GF_Terminal *term);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERM_INFO_H_*/
