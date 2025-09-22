/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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



#ifndef _GF_MEDIA_OBJECT_H_
#define _GF_MEDIA_OBJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/mediaobject.h>
\brief Interface between compositor and decoding engine for media data access.
 */
	
/*!
\addtogroup mobj_grp MediaObject
\ingroup playback_grp
\brief Interface between compositor and decoding engine for media data access.

This section documents the API between the compositor of GPAC and the decoding engine (filter pids)

@{
 */
	
#include <gpac/filters.h>
#include <gpac/scenegraph_vrml.h>


/*! Media Object

  opaque handler for all natural media objects (audio, video, image) so that compositor and systems engine
are not too tied up.
	\note The media object location relies on the node parent graph (this is to deal with namespaces in OD framework)
therefore it is the task of the media management app to setup clear links between the scene graph and its resources
(but this is not mandatory, cf URLs in VRML )
*/
typedef struct _mediaobj GF_MediaObject;

/*! locates media object related to the given node - url designes the object to find - returns NULL if
URL cannot be handled - note that until the mediaObject.isInit member is true, the media object is not valid
(and could actually never be)
\param node node querying the URL
\param url the URL to open
\param lock_timelines if GF_TRUE, forces media timelines in remote URL to be locked to the node timeline
\param force_new_res if GF_TRUE, forces loading of a new resource
\return new media object or NULL of URL not valid
 */
GF_MediaObject *gf_mo_register(GF_Node *node, MFURL *url, Bool lock_timelines, Bool force_new_res);
/*! unregisters the node from the media object
\param node node querying the URL
\param mo the media object to unregister - can be destroyed during this function call
*/
void gf_mo_unregister(GF_Node *node, GF_MediaObject *mo);

/*! opens media object
\param mo the target media object
\param clipBegin the playback start time in seconds
\param clipEnd the playback end time in seconds
\param can_loop if GF_TRUE, indicates the playback can be looped
*/
void gf_mo_play(GF_MediaObject *mo, Double clipBegin, Double clipEnd, Bool can_loop);

/*! stops media object. Object is set to null if stop triggers a removal.
If not removed, video memory is not reset, last frame is kept
\param mo the target media object
*/
void gf_mo_stop(GF_MediaObject **mo);

/*! restarts media object - shall be used for all looping media instead of stop/play for mediaControl
to restart appropriated objects
\param mo the target media object
*/
void gf_mo_restart(GF_MediaObject *mo);
/*! pauses a media object
\param mo the target media object
*/
void gf_mo_pause(GF_MediaObject *mo);
/*! resumes a media object
\param mo the target media object
*/
void gf_mo_resume(GF_MediaObject *mo);

/*!
	Note on mediaControl: mediaControl is the media management app responsibility, therefore
is hidden from the rendering app. Since MediaControl overrides default settings of the node (speed and loop)
you must use the gf_mo_get_speed and gf_mo_get_loop in order to know whether the related field applies or not
*/

/*! sets speed of media - speed is not always applied, depending on media control settings.
\note audio pitching is the responsibility of the rendering app
\param mo the target media object
\param speed the playback speed to set
*/
void gf_mo_set_speed(GF_MediaObject *mo, Fixed speed);
/*! gets current speed of media
\param mo the target media object
\param in_speed is the speed of the media as set in the node (MovieTexture, AudioClip and AudioSource)
\return the real speed of the media as overloaded by mediaControl if any*/
Fixed gf_mo_get_speed(GF_MediaObject *mo, Fixed in_speed);
/*! gets current looping flag of media
\param mo the target media object
\param in_loop is the looping flag of the media as set in the node (MovieTexture, AudioClip)
\return the real loop flag of the media as overloaded by mediaControl if any*/
Bool gf_mo_get_loop(GF_MediaObject *mo, Bool in_loop);
/*! get media object duration
\param mo the target media object
\return media object duration*/
Double gf_mo_get_duration(GF_MediaObject *mo);
/*! checks if object should be deactivated (stop) or not - this checks object status as well as
mediaControl status
\param mo the target media object
\return GF_TRUE if should be deactivated*/
Bool gf_mo_should_deactivate(GF_MediaObject *mo);
/*! checks whether the target object is changed - you MUST use this in order to detect url changes
\param mo the target media object
\param url URL to compare to object current URL
\return GF_TRUE if URL changed
*/
Bool gf_mo_url_changed(GF_MediaObject *mo, MFURL *url);


/*! gets minimum frame duration for an object
\param mo the target media object
\return min frame duration or 0 if unknown*/
u32 gf_mo_get_min_frame_dur(GF_MediaObject *mo);
/*! map a timestamp to the object clock
\param mo the target media object
\param ts a timestamp in media object clock base
\return the timestamp in system time base
*/
u32 gf_mo_map_timestamp_to_sys_clock(GF_MediaObject *mo, u32 ts);
/*! checks if object is buffering
\param mo the target media object
\return GF_TRUE if object is buffering*/
Bool gf_mo_is_buffering(GF_MediaObject *mo);

/*! frame fetch mode for media object*/
typedef enum
{
	//never resync the content of the decoded media buffer (used fo audio)
	//if clock is paused do not fetch
	GF_MO_FETCH = 0,
	//always resync the content of the decoded media buffer to the current time (used for video)
	GF_MO_FETCH_RESYNC,
	//never resync the content of the decoded media buffer (used fo audio)
	//if clock is paused,  do fetch (used for audio extraction)
	GF_MO_FETCH_PAUSED
} GF_MOFetchMode;

/*! fetches media data
\param mo the target media object
\param resync resync mode for fetch operation
\param upload_time_ms average time needed to push frame on GPU
\param eos set to end of stream status of the media
\param timestamp set to the frmae timestamp in object time
\param size set to the frame data size
\param ms_until_pres set to the number of milliseconds until presentation is due
\param ms_until_next set to the number of milliseconds until presentation of next frame is due
\param outFrame set to the associated frame interface object if any
\param planar_size set to the planar size of audio data, or 0
\return frame data
*/
u8 *gf_mo_fetch_data(GF_MediaObject *mo, GF_MOFetchMode resync, u32 upload_time_ms, Bool *eos, u32 *timestamp, u32 *size, s32 *ms_until_pres, s32 *ms_until_next, GF_FilterFrameInterface **outFrame, u32 *planar_size);

/*! releases given amount of media data - nb_bytes is used for audio
\param mo the target media object
\param nb_bytes number of audio bytes to remove (ignored for other media types)
\param drop_mode can take the following values:
-1: do not drop
0: do not force drop: the unlocked frame it will be dropped based on object time (typically video)
1: force drop : the unlocked frame will be dropped if all bytes are consumed (typically audio)
2: the frame will be stated as a discraded frame
*/
void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, s32 drop_mode);
/*! gets object clock
\param mo the target media object
\param obj_time set to the object current time in milliseconds in object time base
*/
void gf_mo_get_object_time(GF_MediaObject *mo, u32 *obj_time);
/*! checks if object is muted. Muted media shouldn't be displayed
\param mo the target media object
\return GF_TRUE if muted
*/
Bool gf_mo_is_muted(GF_MediaObject *mo);
/*! checks if a media object is done
\param mo the target media object
\return GF_TRUE if end of stream*/
Bool gf_mo_is_done(GF_MediaObject *mo);
/*! adjusts clock sync (only audio objects are allowed to use this)
\param mo the target media object
\param ms_drift drift in milliseconds between object clock and actual rendering time
*/
void gf_mo_adjust_clock(GF_MediaObject *mo, s32 ms_drift);

/*! checks if a media object is started
\param mo the target media object
\return GF_TRUE if started*/
Bool gf_mo_is_started(GF_MediaObject *mo);

/*! gets visual information of a media object
\param mo the target media object
\param width set to width in pixels
\param height set to height in pixels
\param stride set to stride in bytes for visual objects with data frame, 0 if unknown
\param pixel_ar set to the pixel aspect ratio as \code (PAR_NUM<<16)|PAR_DEN \endcode
\param pixelFormat set to the pixel format of the video
\param is_flipped set to GF_TRUE if the pixels are vertically flipped (happens when reading back OpenGL textures)
\return GF_TRUE if success*/
Bool gf_mo_get_visual_info(GF_MediaObject *mo, u32 *width, u32 *height, u32 *stride, u32 *pixel_ar, u32 *pixelFormat, Bool *is_flipped);

/*! gets visual information of a media object
\param mo the target media object
\param width set to width in pixels
\param height set to height in pixels
\param stride set to stride in bytes for visual objects with data frame, 0 if unknown
\param pixel_ar set to the pixel aspect ratio as \code (PAR_NUM<<16)|PAR_DEN \endcode
\param pixelFormat set to the pixel format of the video
\param is_flipped set to GF_TRUE if the pixels are vertically flipped (happens when reading back OpenGL textures)
\param for_texture if true check for texture dimensions otherwise for SRD dimensions
\return GF_TRUE if success*/
Bool gf_mo_get_visual_info_ex(GF_MediaObject *mo, u32 *width, u32 *height, u32 *stride, u32 *pixel_ar, u32 *pixelFormat, Bool *is_flipped, Bool for_texture);

/*! gets number of views for 3D video object
\param mo the target media object
\param nb_views set to the number of views in the object, vertically packed
*/
void gf_mo_get_nb_views(GF_MediaObject *mo, u32 *nb_views);

/*! gets visual information of a media object
\param mo the target media object
\param sample_rate set to the sampling frequency of the object
\param afmt set to the decoded PCM audio format
\param num_channels set to the number of channels
\param channel_config set to the channel configuration
\param forced_layout set to GF_TRUE if the channel layout is forced (prevents recomputing the layout for mono/stereo setups)
\return GF_TRUE if success*/
Bool gf_mo_get_audio_info(GF_MediaObject *mo, u32 *sample_rate, u32 *afmt, u32 *num_channels, u64 *channel_config, Bool *forced_layout);

/*! gets current playback speed of a media object
\param mo the target media object
\return the playback speed*/
Fixed gf_mo_get_current_speed(GF_MediaObject *mo);

/*! checks if the service associated withthis object has an audio stream
\param mo the target media object
\return 0 if no audio is associated, 1 if there is an audio object associated, 2 if the service is not yet ready (not connected)*/
u32 gf_mo_has_audio(GF_MediaObject *mo);

/*! media object user flags*/
typedef enum
{
	/*used by animation stream to remove TEXT from display upon delete and URL change*/
	GF_MO_DISPLAY_REMOVE = (1<<1),
	/*used when resyncing a stream (dropping late frames)*/
	GF_MO_IN_RESYNC = (1<<2),
} GF_MOUserFlags;
/*! sets flags on a media object
\param mo the target media object
\param flag the flag(s) to adjust
\param set_on if GF_TRUE, set the flag(s), otherwise removes the flag(s)
*/
void gf_mo_set_flag(GF_MediaObject *mo, GF_MOUserFlags flag, Bool set_on);

/*! loads a new resource as indicated in the xlink:href attribute of the node. If this points to a fragment
of the current document, returns NULL. This will automatically trigger a play request on the resource
\param node node with xlink:href attribute to a media resource
\param primary_resource indicates if this is a primary resource (foreign object in SVG)
\param clipBegin the playback start time in seconds
\param clipEnd the playback end time in seconds
\return the media object for this resource
*/
GF_MediaObject *gf_mo_load_xlink_resource(GF_Node *node, Bool primary_resource, Double clipBegin, Double clipEnd);
/*! unloads a media object associated with a given node through xlink:href
\param node node with xlink:href attribute to the media resource
\param mo the target media object
*/
void gf_mo_unload_xlink_resource(GF_Node *node, GF_MediaObject *mo);
/*! gets the scene graph associated with a scene/document object
\param mo the target media object
\return the scene graph, or NULL if wrong type or not loaded
*/
GF_SceneGraph *gf_mo_get_scenegraph(GF_MediaObject *mo);

/*! VR and SRD (Spatial Relationship Description) information of media object*/
typedef struct
{
	u32 vr_type;
	s32 srd_x;
	s32 srd_y;
	s32 srd_w;
	s32 srd_h;
	
	s32 srd_min_x;
	s32 srd_min_y;
	s32 srd_max_x;
	s32 srd_max_y;
	
	u32 scene_width;
	u32 scene_height;

	Bool has_full_coverage;
	Bool is_tiled_srd;
} GF_MediaObjectVRInfo;

/*! gets SRD and VR info for a media object. Returns FALSE if no VR and no SRD info
\param mo the target media object
\param vr_info set to the VR and SRD info of the media object
\return GF_TRUE if SRD info present
*/
Bool gf_mo_get_srd_info(GF_MediaObject *mo, GF_MediaObjectVRInfo *vr_info);

/*! sets quality degradation hint for a media object
\param mo the target media object
\param quality_degradation quality hint value between 0 (max quality) and 100 (worst quality)
*/
void gf_mo_hint_quality_degradation(GF_MediaObject *mo, u32 quality_degradation);

/*! sets visible rectangle for a media object, only used in 360 videos for now
\param mo the target media object
\param min_x minimum horizontal coordinate of visible region, in pixel in the media frame (0 being left column)
\param max_x maximum horizontal coordinate of visible region, in pixel in the media frame (width being right column)
\param min_y minimum vertical coordinate of visible region, in pixel in the media frame (0 being top row)
\param max_y maximum vertical coordinate of visible region, in pixel in the media frame (height being bottom row)
*/
void gf_mo_hint_visible_rect(GF_MediaObject *mo, u32 min_x, u32 max_x, u32 min_y, u32 max_y);

/*! sets gaze position in a media object, only used in 360 videos for now
\param mo the target media object
\param gaze_x minimum horizontal coordinate of visible region, in pixel in the media frame (0 being left column, width being right column)
\param gaze_y maximum horizontal coordinate of visible region, in pixel in the media frame (0 being top row, bottom row)
*/
void gf_mo_hint_gaze(GF_MediaObject *mo, u32 gaze_x, u32 gaze_y);

#include <gpac/scenegraph_svg.h>
/*! destroys a media object
\param mo the target media object
*/
void gf_mo_del(GF_MediaObject *mo);

/*! adds event target node to a media object
\param mo the target media object
\param node the event target node
\return a DOM event target interface object
*/
GF_DOMEventTarget *gf_mo_event_target_add_node(GF_MediaObject *mo, GF_Node *node);
/*! removes event target node from a media object
\param mo the target media object
\param node the event target node
\return error if any
*/
GF_Err gf_mo_event_target_remove_by_node(GF_MediaObject *mo, GF_Node *node);
/*! counts number of event targets associated with a media object
\param mo the target media object
\return number of event targets
*/
u32 gf_mo_event_target_count(GF_MediaObject *mo);

/*! removes event target node from a media object by index
\param mo the target media object
\param index 0-based index of the event target to remove
\return error if any
*/
GF_Err gf_mo_event_target_remove_by_index(GF_MediaObject *mo, u32 index);
/*! gets event target node of a media object by index
\param mo the target media object
\param index 0-based index of the event target to get
\return a DOM event target interface object
*/
GF_DOMEventTarget *gf_mo_event_target_get(GF_MediaObject *mo, u32 index);

/*! resets all event targets of a media object
\param mo the target media object
*/
void gf_mo_event_target_reset(GF_MediaObject *mo);

/*! finds an even target interface of a media object associated to a given node
\param mo the target media object
\param node the node associated with the event target
\return the index of the event target interface object
*/
s32 gf_mo_event_target_find_by_node(GF_MediaObject *mo, GF_Node *node);
/*! enumerates event target nodes associated with a media object
\param mo the target media object
\param i current index to query, incremented upon function return
\return an event target node
*/
GF_Node *gf_mo_event_target_enum_node(GF_MediaObject *mo, u32 *i);
/*! gets the event target node of an event target interface
\param target the target event target interface
\return the associated node, NULL if error
*/
GF_Node *gf_event_target_get_node(GF_DOMEventTarget *target);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_MEDIA_OBJECT_H_*/


