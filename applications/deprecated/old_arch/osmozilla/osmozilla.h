/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
*					All rights reserved
*
*  This file is part of GPAC / Osmozilla NPAPI plugin
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

#ifndef __OSMOZILLA_H__
#define __OSMOZILLA_H__


/*DO NOT INCLUDE ANY GPAC FILE IN THIS HEADER, IT CAUSES TYPE REDEFINITION CONFLICT ON OSX*/
typedef struct _tag_terminal GF_Terminal;
typedef struct _tag_user GF_User;

#include <stdio.h>


typedef struct _NPP *NPP;

typedef struct __tag_osmozilla
{
	/*plugiun & window info*/
	NPP np_instance;
	int window_set;
	unsigned int height, width;

	int supports_xembed;
	/*GPAC term*/
	GF_User *user;
	GF_Terminal *term;

	/*general options*/
	unsigned int loop, auto_start, is_connected, use_3d, disable_mime;
	unsigned int aspect_ratio;

	/*the URL we are connected to*/
	char *url;
	/*timing info of current url*/
	double duration;
	char can_seek;
	int use_gui;
	int download_progress;

	/*log file if any*/
	FILE *logs;

#ifdef GECKO_XPCOM
	void *scriptable_peer;
#else
	struct NPObject *script_obj;
#endif

} Osmozilla;


#endif // __OSMOZILLA_H__
