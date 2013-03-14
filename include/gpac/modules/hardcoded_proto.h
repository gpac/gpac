/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2012
 *					All rights reserved
 *
 *  This file is part of GPAC / modules interfaces
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


#ifndef _GF_MODULE_PROTO_MOD_H_
#define _GF_MODULE_PROTO_MOD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/module.h>
#include <gpac/scenegraph.h>
#include <gpac/compositor.h>

/*interface name and version for Built-in proto User Extensions*/
#define GF_HARDCODED_PROTO_INTERFACE		GF_4CC('G','H','P', '2')

typedef struct _hc_proto_mod
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	/*Initialize hardcoded proto node.
	 itfs: ProtoModuleInterface
	 compositor: GPAC compositor
	 node: node to be loaded - this node is always a PROTO instance
	 proto_uri: the proto URI
	*/
	Bool (*init)(struct _hc_proto_mod* itfs, GF_Compositor* compositor, GF_Node* node, const char *proto_uri);

	/*check if the module can load a proto
	 uri: proto uri to check for support
	*/
	Bool (*can_load_proto)(const char* uri);

	/*module private*/
	void *udta;
} GF_HardcodedProto;


#ifdef __cplusplus
}
#endif


#endif	/*#define _GF_MODULE_PROTO_MOD_H_*/

