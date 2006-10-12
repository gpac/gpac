/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Rendering sub-project
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

#include <gpac/constants.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/mediaobject.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/renderer.h>

/* Creates a subscene from the xlink:href */
GF_InlineScene *gf_svg_subscene_get(SVGElement *elt)
{
	MFURL url;
	GF_MediaObject *mo;

	GF_SceneGraph *graph = gf_node_get_graph((GF_Node *) elt);
	GF_InlineScene *is = gf_sg_get_private(graph);
	if (!is) return 0;

	memset(&url, 0, sizeof(MFURL));
	gf_svg_set_mfurl_from_uri(NULL, &url, &elt->xlink->href);

	/*
		creates the media object if not already created at the InlineScene level
		TODO FIX ME: do it at the terminal level
	*/
	mo = gf_is_get_media_object(is, &url, GF_MEDIA_OBJECT_SCENE);
	if (!mo || !mo->odm) return NULL;

	gf_sg_vrml_mf_reset(&url, GF_SG_VRML_MFURL);
	return mo->odm->subscene;
}

void gf_svg_subscene_start(GF_InlineScene *is)
{
	GF_MediaObject *mo = is->root_od->mo;
	if (!mo) return;
	gf_mo_play(mo, 0, 0);
}

void gf_svg_subscene_stop(GF_InlineScene *is, Bool reset_ck)
{
	u32 i;
	GF_ObjectManager *ctrl_od;
	GF_Clock *ck;

	if (!is->root_od->mo->num_open) return;
	
	/* Can we control the timeline of the subscene (e.g. non broadcast ...) */
	if (is->root_od->no_time_ctrl) return;

	assert(is->root_od->parentscene);
	
	/* Is it the same timeline as the parent scene ? */
	ck = gf_odm_get_media_clock(is->root_od);
	if (!ck) return;
    /* Achtung: unspecified ! do we have the right to stop the parent timeline as well ??? */
	if (gf_odm_shares_clock(is->root_od->parentscene->root_od, ck)) return;
	
	/* stop main subod and all other sub od */
	gf_mo_stop(is->root_od->mo);
	i=0;
	while ((ctrl_od = gf_list_enum(is->ODlist, &i))) {
		if (ctrl_od->mo->num_open) gf_mo_stop(ctrl_od->mo);
	}
	gf_mo_stop(is->root_od->mo);
	if (reset_ck) 
		gf_clock_reset(ck);
	else
		ck->clock_init = 0;
}



#endif //GPAC_DISABLE_SVG

