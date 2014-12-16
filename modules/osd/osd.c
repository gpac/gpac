/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2011-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Sampe On-Scvreen Display sub-project
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

#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/nodes_mpeg4.h>


typedef struct
{
	GF_ObjectManager *odm;
	GF_Terminal *term;
	GF_TermEventFilter evt_filter;

	/*some of our nodes*/
	M_Switch *visible;
	M_Transform2D *transform;
	M_CompositeTexture2D *ct2d;
	M_Text *text;

	char statBuffer[100];
	u32 refresh_time_ms;
	GF_SystemRTInfo rti;
} GF_OSD;

#if 0
static GFINLINE GF_Node *create_node(GF_OSD *osd, u32 tag, GF_Node *par)
{
	GF_Node *n = gf_node_new(osd->odm->subscene->graph, tag);
	if (n) {
		gf_node_init(n);
		if (par) {
			gf_node_list_add_child( & ((GF_ParentNode *)par)->children, n);
			gf_node_register(n, par);
		}
	}
	return n;
}
#endif

const char *osd_scene_graph = "\
EXTERNPROTO Untransform [\
    exposedField MFNode children []\
]\
[ \"urn:inet:gpac:builtin:Untransform\"]\
OrderedGroup {\
 children [\
  Untransform {\
   children [\
  DEF N1 Switch {\
   whichChoice 0\
   choice [\
    DEF N2 Transform2D {\
     children [\
      Shape {\
       appearance Appearance {\
        material Material2D {\
         transparency 0\
         filled TRUE\
        }\
        texture DEF N3 CompositeTexture2D {\
         pixelWidth 256\
         pixelHeight 16\
         children [\
          Background2D {backColor 0.6 0.6 0.6}\
          Shape {\
           appearance Appearance {\
            material Material2D {\
             emissiveColor 0 0 0\
             filled TRUE\
            }\
           }\
           geometry DEF N4 Text {\
            string [\"My Sample Text !\"]\
            fontStyle FontStyle {\
             size 12\
             justify [\"MIDDLE\", \"MIDDLE\"]\
             family [\"SANS\"]\
            }\
           }\
          }\
         ]\
        }\
       }\
       geometry Bitmap {}\
      }\
     ]\
    }\
   ]\
  }\
   ]\
  }\
 ]\
}\
";

void osd_on_resize(GF_Node *hdl, GF_DOM_Event *event, GF_Node *observer)
{
	GF_DOMHandler *the_hdl = (GF_DOMHandler *) hdl;
	GF_OSD *osd = the_hdl->evt_listen_obj;

	if (osd->ct2d) {
		//osd->ct2d->pixelWidth = FIX2INT(event->screen_rect.width);
		gf_node_dirty_set((GF_Node *) osd->ct2d, GF_SG_NODE_DIRTY, 1);

		if (osd->transform) {
			osd->transform->translation.y = INT2FIX( (FIX2INT(event->screen_rect.height) - osd->ct2d->pixelHeight) / 2 ) ;
			gf_node_dirty_set((GF_Node *) osd->transform, GF_SG_NODE_DIRTY, 1);
		}
	}
}

Bool osd_load_scene(GF_OSD *osd)
{
	GF_Node *n;
	GF_List *nodes;
	const char *opt;
	GF_DOMHandler *hdl;
	/*BT/VRML from string*/
	GF_List *gf_sm_load_bt_from_string(GF_SceneGraph *in_scene, const char *node_str, Bool force_wrl);

	/*create a new scene*/
	osd->odm = gf_odm_new();
	osd->odm->term = osd->term;
	osd->odm->subscene = gf_scene_new(NULL);
	osd->odm->subscene->root_od = osd->odm;
	gf_sg_set_scene_size_info(osd->odm->subscene->graph, 0, 0, 1);

	/*create a scene graph*/
	nodes = gf_sm_load_bt_from_string(osd->odm->subscene->graph, osd_scene_graph, 0);
	n = gf_list_get(nodes, 0);
	gf_list_del(nodes);

	if (!n) return 0;

	gf_sg_set_root_node(osd->odm->subscene->graph, n);
	gf_sg_set_scene_size_info(osd->odm->subscene->graph, 0, 0, 1);

	hdl = gf_dom_listener_build(n, GF_EVENT_RESIZE, 0);
	hdl->handle_event = osd_on_resize;
	hdl->evt_listen_obj = osd;

	osd->visible = (M_Switch *)gf_sg_find_node_by_name(osd->odm->subscene->graph, "N1");
	osd->transform = (M_Transform2D *)gf_sg_find_node_by_name(osd->odm->subscene->graph, "N2");
	osd->ct2d = (M_CompositeTexture2D *)gf_sg_find_node_by_name(osd->odm->subscene->graph, "N3");
	osd->text = (M_Text *)gf_sg_find_node_by_name(osd->odm->subscene->graph, "N4");
	if (osd->text->string.vals[0]) {
		gf_free(osd->text->string.vals[0]);
		osd->text->string.vals[0] = NULL;
	}
	strcpy(osd->statBuffer, "Hello World !");
	osd->text->string.vals[0] = osd->statBuffer;

	opt = gf_cfg_get_key(osd->term->user->config, "OSD", "Visible");
	if (!opt || strcmp(opt, "yes")) osd->visible->whichChoice = -1;


	return 1;
}


Bool osd_on_event_play(void *udta, GF_Event *event, Bool consumed_by_compositor)
{
	GF_OSD* osd = (GF_OSD*)udta;
	switch (event->type) {
	case GF_EVENT_SCENE_SIZE:
		gf_sg_set_scene_size_info(osd->odm->subscene->graph, event->size.width, event->size.height, 1);
		break;
	case GF_EVENT_KEYUP:
		if ( (event->key.key_code == GF_KEY_I) && (event->key.flags & GF_KEY_MOD_CTRL)) {
			if (osd->visible->whichChoice==0) {
				osd->visible->whichChoice = -1;
				gf_cfg_set_key(osd->term->user->config, "OSD", "Visible", "no");
			} else {
				osd->visible->whichChoice = 0;
				gf_cfg_set_key(osd->term->user->config, "OSD", "Visible", "yes");
			}
			gf_node_dirty_set((GF_Node *) osd->visible, GF_SG_NODE_DIRTY, 1);
		}
		break;
	}
	return 0;
}
static Bool osd_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_OSD *osd = termext->udta;

	switch (action) {
	case GF_TERM_EXT_START:
		osd->term = (GF_Terminal *) param;
		opt = gf_modules_get_option((GF_BaseInterface*)termext, "OSD", "Enabled");
		if (opt && strcmp(opt, "yes")) return 0;

		/*load scene*/
		if (! osd_load_scene(osd)) return 0;
		/*attach scene to compositor*/
		gf_sc_register_extra_graph(osd->term->compositor, osd->odm->subscene->graph, 0);


		/*we are not threaded*/
		termext->caps |= GF_TERM_EXTENSION_NOT_THREADED;

		osd->refresh_time_ms = 500;
		osd->evt_filter.on_event = osd_on_event_play;
		osd->evt_filter.udta = osd;
		gf_term_add_event_filter(osd->term, &osd->evt_filter);
		return 1;

	case GF_TERM_EXT_STOP:
		osd->text->string.vals[0] = NULL;
		/*remove scene to compositor*/
		gf_sc_register_extra_graph(osd->term->compositor, osd->odm->subscene->graph, 1);
		gf_odm_disconnect(osd->odm, 1);
		osd->odm = NULL;

		gf_term_remove_event_filter(osd->term, &osd->evt_filter);
		osd->term = NULL;
		break;

	case GF_TERM_EXT_PROCESS:
		/*flush all events until current time if reached*/
		if ((osd->visible->whichChoice==0) && gf_sys_get_rti(osd->refresh_time_ms, &osd->rti, 0)) {
			sprintf(osd->statBuffer, "CPU %02d - FPS %02.2f - MEM "LLU" KB", osd->rti.process_cpu_usage, gf_sc_get_fps(osd->term->compositor, 0), osd->rti.process_memory/1000);
			gf_node_dirty_set((GF_Node *) osd->text, GF_SG_NODE_DIRTY, 1);
		}
		break;
	}
	return 0;
}


GF_TermExt *osd_new()
{
	GF_TermExt *dr;
	GF_OSD *osd;
	dr = (GF_TermExt*)gf_malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "GPAC OnScreen Display", "gpac distribution");

	GF_SAFEALLOC(osd, GF_OSD);
	dr->process = osd_process;
	dr->udta = osd;
	return dr;
}


void osd_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_OSD *osd = dr->udta;
	gf_free(osd);
	gf_free(dr);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_TERM_EXT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)osd_new();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		osd_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( osd )
