/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

#include <gpac/scenegraph_vrml.h>


/*
		Media Object

  opaque handler for all natural media objects (audio, video, image) so that scene renderer and systems engine
are not too tied up. 
	NOTE: the media object location relies on the node parent graph (this is to deal with namespaces in OD framework)
therefore it is the task of the media management app to setup clear links between the scene graph and its ressources
(but this is not mandatory, cf URLs in VRML )

	TODO - add interface for shape coding positioning in mediaObject and in the decoder API
*/

enum
{
	/*this is set to 0 by the OD manager whenever a change occur in the media (w/h change, SR change, etc) 
	as a hint for the renderer*/
	GF_MO_IS_INIT = (1<<1),
	/*this is used for 3D/GL rendering to indicate an image has been vertically flipped*/
	GF_MO_IS_FLIP = (1<<2),
	/*used by animation stream to remove TEXT from display upon delete and URL change*/
	GF_MO_DISPLAY_REMOVE = (1<<3),
};

struct _mediaobj
{
	/*type is as defined in constants.h # GF_MEDIA_OBJECT_* */
	u32 type;
	/*one of the above flags*/
	u32 mo_flags;
	/*audio props*/
	u32 sample_rate;
	u32 bits_per_sample;
	u32 num_channels;
	/*cf constants.h for audio channel cfg*/
	u32 channel_config;

	/*video props*/
	Fixed FPS;	/*this is not known by default, but computed on the fly*/
	u32 width, height, stride, pixel_ar;
	u32 pixelFormat;
	/*changing at each frame for shape coding, or always 0 for frame coding, indicates 
	X and Y coordinate of object in frame(width, height)*/
	u32 offset_x, offset_y;
	/*shared object info: if 0 a new frame will be checked, otherwise current is returned*/
	u32 num_fetched;
	/*frame presentation time*/
	u32 current_ts;
	/*data frame size*/
	u32 current_size;
	/*pointer to data frame */
	unsigned char *current_frame;

	/* private to ESM*/
	struct _tag_terminal *term;
	/*media object manager - private to the sync engine*/
	struct _od_manager *odm;
	/*OD ID of the object*/
	u32 OD_ID;
	/*OD URL for object not using MPEG4 OD urls*/
	MFURL URLs;
	/*session join*/
	u32 num_open;
	/*shared object restart handling*/
	u32 num_to_restart, num_restart;
	Fixed speed;
};

typedef struct _mediaobj GF_MediaObject;

GF_MediaObject *gf_mo_new(GF_Terminal *term);

/*locate media object related to the given node - url designes the object to find - returns NULL if
URL cannot be handled - note that until the mediaObject.isInit member is true, the media object is not valid
(and could actually never be) */
GF_MediaObject *gf_mo_find(GF_Node *node, MFURL *url);
/*opens media object*/
void gf_mo_play(GF_MediaObject *mo);
/*stops media object - video memory is not reset, last frame is kept*/
void gf_mo_stop(GF_MediaObject *mo);
/*restarts media object - shall be used for all looping media instead of stop/play for mediaControl
to restart appropriated objects*/
void gf_mo_restart(GF_MediaObject *mo);

/*
	Note on mediaControl: mediaControl is the media management app responsability, therefore
is hidden from the rendering app. Since MediaControl overrides default settings of the node (speed and loop)
you must use the gf_mo_get_speed and gf_mo_get_loop in order to know whether the related field applies or not
*/

/*set speed of media - speed is not always applied, depending on media control settings.
NOTE: audio pitching is the responsability of the rendering app*/
void gf_mo_set_speed(GF_MediaObject *mo, Fixed speed);
/*returns current speed of media - in_speed is the speed of the media as set in the node (MovieTexture, 
AudioClip and AudioSource) - the return value is the real speed of the media as overloaded by mediaControl if any*/
Fixed gf_mo_get_speed(GF_MediaObject *mo, Fixed in_speed);
/*returns looping flag of media - in_loop is the looping flag of the media as set in the node (MovieTexture, 
AudioClip) - the return value is the real loop flag of the media as overloaded by mediaControl if any*/
Bool gf_mo_get_loop(GF_MediaObject *mo, Bool in_loop);
/*returns media object duration*/
Double gf_mo_get_duration(GF_MediaObject *mo);
/*returns whether the object should be deactivated (stop) or not - this checks object status as well as 
mediaControl status */
Bool gf_mo_should_deactivate(GF_MediaObject *mo);
/*checks whether the target object is changed - you MUST use this in order to detect url changes*/
Bool gf_mo_url_changed(GF_MediaObject *mo, MFURL *url);
/*fetch media data - returns 1 if new frame (in which case media info of the GF_MediaObject structure are updated)
0 otherwise - eos is set if stream is finished. if resync is specified and no frame is locked the video memory is
resync'ed to its object clock (eg frame droping) 
*/
Bool gf_mo_fetch_data(GF_MediaObject *mo, Bool resync, Bool *eos);
/*release given amount of media data - nb_bytes is used for audio - if forceDrop is set, the unlocked frame will be 
droped if all bytes are consumed, otherwise it will be droped based on object time - typically, video fetches with the resync
flag set and release without forceDrop, while audio fetches without resync but forces buffer drop*/
void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, Bool forceDrop);
/*get media time*/
void gf_mo_get_media_time(GF_MediaObject *mo, u32 *media_time, u32 *media_dur);
/*get object clock*/
void gf_mo_get_object_time(GF_MediaObject *mo, u32 *obj_time);
/*returns mute flag of media - if muted the media shouldn't be displayed*/
Bool gf_mo_is_muted(GF_MediaObject *mo);
/*returns end of stream state*/
Bool gf_mo_is_done(GF_MediaObject *mo);
/*resyncs clock - only audio objects are allowed to use this*/
void gf_mo_adjust_clock(GF_MediaObject *mo, s32 ms_drift);


#ifdef __cplusplus
}
#endif


#endif	/*_GF_MEDIA_OBJECT_H_*/


