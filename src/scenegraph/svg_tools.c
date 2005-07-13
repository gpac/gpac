/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
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
#include <gpac/internal/scenegraph_dev.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/scenegraph_svg.h>

SVGElement *SVG_NewNode(GF_SceneGraph *inScene, u32 tag)
{
	SVGElement *node;
	if (!inScene) return NULL;
	node = SVG_CreateNode(tag);
	if (node) {
		node->sgprivate->scenegraph = inScene;
	}
	return (SVGElement *)node;
}

#else
/*these ones are only needed for W32 libgpac_dll build in order not to modify export def file*/
u32 SVG_GetTagByName(const char *element_name)
{
	return 0;
}
u32 SVG_GetAttributeCount(GF_Node *n)
{
	return 0;
}
GF_Err SVG_GetAttributeInfo(GF_Node *node, GF_FieldInfo *info)
{
	return GF_NOT_SUPPORTED;
}

GF_Node *SVG_NewNode(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
GF_Node *SVG_CreateNode(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
const char *SVG_GetElementName(u32 tag)
{
	return "Unsupported";
}

#endif	//GPAC_DISABLE_SVG
