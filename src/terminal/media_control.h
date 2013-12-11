/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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



#ifndef _MEDIA_CONTROL_H_
#define _MEDIA_CONTROL_H_

#include <gpac/internal/terminal_dev.h>
/*MediaControl definition*/
#include <gpac/nodes_mpeg4.h>


/*restart object and takes care of media control/clock dependencies*/
void mediacontrol_restart(GF_ObjectManager *odm);
void mediacontrol_pause(GF_ObjectManager *odm);
void mediacontrol_resume(GF_ObjectManager *odm);

Bool MC_URLChanged(MFURL *old_url, MFURL *new_url);

void mediasensor_update_timing(GF_ObjectManager *odm, Bool is_eos);

#ifndef GPAC_DISABLE_VRML

/*to do: add preroll support*/
typedef struct _media_control
{
	M_MediaControl *control;

	/*store current values to detect changes*/
	Double media_start, media_stop;
	Fixed media_speed;
	Bool enabled;
	MFURL url;
	
	GF_Scene *parent;
	/*stream owner*/
	GF_MediaObject *stream;
	/*stream owner's clock*/
	GF_Clock *ck;

	u32 changed;
	Bool is_init;
	Bool paused;
	u32 prev_active;

	/*stream object list (segments)*/
	GF_List *seg;
	/*current active segment index (ie, controling the PLAY range of the media)*/
	u32 current_seg;
} MediaControlStack;
void InitMediaControl(GF_Scene *scene, GF_Node *node);
void MC_Modified(GF_Node *node);

void MC_GetRange(MediaControlStack *ctrl, Double *start_range, Double *end_range);

/*assign mediaControl for this object*/
void gf_odm_set_mediacontrol(GF_ObjectManager *odm, struct _media_control *ctrl);
/*get media control ruling the clock the media is running on*/
struct _media_control *gf_odm_get_mediacontrol(GF_ObjectManager *odm);
/*removes control from OD context*/
void gf_odm_remove_mediacontrol(GF_ObjectManager *odm, struct _media_control *ctrl);
/*switches control (propagates enable=FALSE), returns 1 if control associated with OD has changed to new one*/
Bool gf_odm_switch_mediacontrol(GF_ObjectManager *odm, struct _media_control *ctrl);

/*returns 1 if this is a segment switch, 0 otherwise - takes care of object restart if segment switch*/
Bool gf_odm_check_segment_switch(GF_ObjectManager *odm);

typedef struct _media_sensor
{
	M_MediaSensor *sensor;

	GF_Scene *parent;

	GF_List *seg;
	Bool is_init;
	/*stream owner*/
	GF_MediaObject *stream;

	/*private cache (avoids browsing all sensor*/
	u32 active_seg;
} MediaSensorStack;

void InitMediaSensor(GF_Scene *scene, GF_Node *node);
void MS_Modified(GF_Node *node);

void MS_Stop(MediaSensorStack *st);

#endif	/*GPAC_DISABLE_VRML*/

#endif	/*_MEDIA_CONTROL_H_*/
