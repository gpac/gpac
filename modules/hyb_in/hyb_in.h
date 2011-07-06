/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2010-20xx
 *					All rights reserved
 *
 *  This file is part of GPAC / Hybrid Media input module
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

#ifndef _HYB_IN_H
#define _HYB_IN_H

#include <gpac/modules/codec.h>
#include <gpac/modules/service.h>
#include <gpac/constants.h>


enum {
	HYB_STATE_RUNNING = 0,	/*default state*/
	HYB_STATE_STOPPING,		/*user asked to stop*/
	HYB_STATE_STOPPED,		/*thread received HYB_STATE_STOPPING and stopped*/
};

enum {
	HYB_PUSH = 0,	/*threaded*/
	HYB_PULL,		/*not threaded*/
};

/*base structure for media hybridation*/
typedef struct s_GF_HYBMEDIA {

	/* *** static methods *** */
	
	/*is url handled by this service?*/
	Bool (*CanHandleURL)(const char *url);
	
	/*retrieve object descriptor*/
	GF_ObjectDescriptor* (*GetOD)(struct s_GF_HYBMEDIA *self);


	/* *** other methods *** */
	
	/*create the object and instantiate all the data*/
	GF_Err (*Connect)    (struct s_GF_HYBMEDIA *self, GF_ClientService *service, const char *url);
	
	/*destroy the object*/
	GF_Err (*Disconnect) (struct s_GF_HYBMEDIA *self, GF_ClientService *service);
	
	/*add an external media*/
	GF_Err (*AddMedia)   (struct s_GF_HYBMEDIA *self);
	
	/*in case data retrieval paradigm is pull these two functions shall not be NULL*/
	GF_Err (*GetData)    (struct s_GF_HYBMEDIA *self, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr); /*only available when data_mode is pull*/
	GF_Err (*ReleaseData)(struct s_GF_HYBMEDIA *self); /*only available when data_mode is pull*/


	/* *** data *** */

	/*data retrieval mode: HYB_PUSH or HYB_PULL*/
	u32 data_mode;

	/*object carrying us (needed to communicate with the player)*/
	GF_ClientService *owner;

	/*pivate data which type depends on dynamic considerations*/
	void *private_data;

} GF_HYBMEDIA;

#endif
