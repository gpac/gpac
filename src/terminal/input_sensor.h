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

#ifndef _INPUT_SENSOR_H_
#define _INPUT_SENSOR_H_

#include <gpac/internal/terminal_dev.h>
/*input sensor defs*/
#include <gpac/nodes_mpeg4.h>

#ifndef GPAC_DISABLE_VRML

enum
{
	IS_KeySensor = 1,
	IS_StringSensor,
	IS_Mouse,
	IS_HTKSensor,
};

#define GPAC_HTK_DEMO  0


typedef struct
{
	/*parent scene*/
	GF_Scene *scene;
	/*list of attached nodes*/
	GF_List *is_nodes;
	/*stream ID*/
	u16 ES_ID;
	/*uncompressed data frame*/
	GF_List *ddf;

	GF_InputSensorDevice *io_dev;

	u32 type;

	/*string sensor sep char */
	s16 termChar, delChar;
	/*current typed text in UTF-8*/
	unsigned short enteredText[5000];
	u32 text_len;
	Bool is_local;

#if GPAC_HTK_DEMO
	GF_Thread *th;
	Bool htk_running;
	char szHTKPath[GF_MAX_PATH];
#endif

} ISPriv;


GF_BaseDecoder *gf_isdec_new(GF_ESD *esd, u32 PL);
void gf_isdec_del(GF_BaseDecoder *plug);
GF_Err gf_isdec_configure(GF_BaseDecoder *plug, GF_Scene *scene, Bool is_remote);


typedef struct
{
	/*stream context*/
	u16 ES_ID;
	Bool registered;
	GF_MediaObject *mo;
	M_InputSensor *is;
} ISStack;


void InitInputSensor(GF_Scene *scene, GF_Node *node);
void InputSensorModified(GF_Node *n);

void InitKeySensor(GF_Scene *scene, GF_Node *node);


typedef struct
{
	u16 enteredText[5000];
	u32 text_len;
	GF_Terminal *term;
} StringSensorStack;

void InitStringSensor(GF_Scene *scene, GF_Node *node);

#endif /*GPAC_DISABLE_VRML*/


#endif	/*_INPUT_SENSOR_H_*/

