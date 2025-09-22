//This software module was originally developed by TelecomParisTech in the
//course of the development of MPEG-U Widgets (ISO/IEC 23007-1) standard.
//
//This software module is an implementation of a part of one or
//more MPEG-U Widgets (ISO/IEC 23007-1) tools as specified by the MPEG-U Widgets
//(ISO/IEC 23007-1) standard. ISO/IEC gives users of the MPEG-U Widgets
//(ISO/IEC 23007-1) free license to this software module or modifications
//thereof for use in hardware or software products claiming conformance to
//the MPEG-U Widgets (ISO/IEC 23007-1). Those intending to use this software
//module in hardware or software products are advised that its use may
//infringe existing patents.
//The original developer of this software module and his/her company, the
//subsequent editors and their companies, and ISO/IEC have no liability
//for use of this software module or modifications thereof in an implementation.
//Copyright is not released for non MPEG-U Widgets (ISO/IEC 23007-1) conforming
//products.
//Telecom ParisTech retains full right to use the code for his/her own purpose,
//assign or donate the code to a third party and to inhibit third parties from
//using the code for non MPEG-U Widgets (ISO/IEC 23007-1) conforming products.
//
//This copyright notice must be included in all copies or derivative works.
//
//Copyright (c) 2009 Telecom ParisTech.
//
// Alternatively, this software module may be redistributed and/or modified
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2, or (at your option)
//  any later version.
//
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:
//					Jean Le Feuvre, Telecom ParisTech
//					Cyril Concolato, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _WIDGETMAN_H_
#define _WIDGETMAN_H_

#include "unzip.h"

/*base SVG type*/
#include <gpac/modules/js_usr.h>

#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>
#include <gpac/nodes_svg.h>
/*dom events*/
#include <gpac/events.h>

#include <gpac/download.h>
#include <gpac/crypt.h>
#include <gpac/network.h>
#include <gpac/xml.h>
#include <gpac/internal/scenegraph_dev.h>


#include <gpac/isomedia.h>

#include <gpac/internal/smjs_api.h>

#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>


JSBool gf_sg_js_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node);
JSBool gf_sg_js_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node);

typedef struct _widget_manager
{
	JSContext *ctx;
	/*widget manager class*/
	GF_JSClass widmanClass;
	/*widget class used by the widget manager*/
	GF_JSClass wmWidgetClass;

	/*widget class used by the widget scripts*/
	GF_JSClass widgetClass;

	GF_JSClass widgetAnyClass;

	JSObject *obj;
	GF_Terminal *term;
	GF_List *widget_instances;
	/*list of loaded prototypes (eg 1 per all instances of the same widget*/
	GF_List *widgets;
} GF_WidgetManager;



enum
{
	GF_WPIN_STRING,
	GF_WPIN_INTEGER,
	GF_WPIN_NUMBER,
};

typedef struct
{
	char *inner_path;
	char *extracted_path;
	Bool extracted;
} GF_WidgetPackageResource;

typedef struct _widget_package_relocator
{
	GF_TERM_URI_RELOCATOR
	GF_List *resources;
	struct _widget *widget;
	GF_WidgetManager *wm;
	Bool is_zip;
	char root_extracted_path[GF_MAX_PATH];
	char archive_id[14];
	char *package_path;
	GF_DownloadSession *sess;
} GF_WidgetPackage;


enum
{
	GF_WM_PARAM_OUTPUT,
	GF_WM_PARAM_INPUT,
	GF_WM_INPUT_ACTION,
	GF_WM_OUTPUT_TRIGGER,
	GF_WM_BIND_ACTION,
	GF_WM_UNBIND_ACTION,
	GF_WM_PREF_CONNECT,
	GF_WM_ACTIVATE_TRIGGER,
	GF_WM_DEACTIVATE_TRIGGER,
	GF_WM_PREF_SAVEDACTION,
	GF_WM_PREF_RESTOREDACTION,
	GF_WM_PREF_SAVETRIGGER,
	GF_WM_PREF_RESTORETRIGGER,
};

#define GF_WM_PARAM_SCRIPT_STRING	(u16) 0
#define GF_WM_PARAM_SCRIPT_BOOL		(u16) 1
#define GF_WM_PARAM_SCRIPT_NUMBER	(u16) 2

typedef struct
{
	struct __widget_message *msg;

	u16 type;
	u16 script_type;
	Bool in_action;

	char *name;
	char *node;
	char *attribute;
	char *default_value;
} GF_WidgetPin;

typedef struct __widget_message
{
	struct _widget_interface *ifce;

	char *name;
	Bool is_output;
	GF_List *params;

	GF_WidgetPin *input_action;
	GF_WidgetPin *output_trigger;

} GF_WidgetMessage;

typedef struct _widget_interface
{
	struct __widget_content *content;
	char *type;
	GF_List *messages;

	GF_WidgetPin *bind_action;
	GF_WidgetPin *unbind_action;
	Bool provider, multiple_binding, required;
	char *connectTo;

	JSObject *obj;
} GF_WidgetInterface;

typedef struct _widget_component
{
	struct __widget_content *content;
	char *id;	/*may be NULL*/
	char *src;	/*may be NULL*/
	GF_List *required_interfaces;	/*may be empty*/
	GF_WidgetPin *activateTrigger;
	GF_WidgetPin *deactivateTrigger;
	GF_WidgetPin *activatedAction;
	GF_WidgetPin *deactivatedAction;
} GF_WidgetComponent;

enum
{
	GF_WM_PREF_READONLY = 1,
	GF_WM_PREF_SAVE = 1<<1,
	GF_WM_PREF_MIGRATE = 1<<2,
};

typedef struct
{
	char *name, *value;
	u32 flags;
	GF_WidgetPin *connectTo;
} GF_WidgetPreference;

typedef struct
{
	char *name, *value;
} GF_WidgetFeatureParam;

typedef struct
{
	char *name;
	Bool required;
	GF_List *params;
} GF_WidgetFeature;

typedef struct __widget_content
{
	struct _widget *widget;

	char *src;
	char *relocated_src;
	u32 width, height;

	char *encoding, *mimetype;

	GF_List *interfaces;
	GF_List *components;
	/*list of preferences for the widget content*/
	GF_List *preferences;

	GF_WidgetPin *savedAction;
	GF_WidgetPin *restoredAction;
	GF_WidgetPin *saveTrigger;
	GF_WidgetPin *restoreTrigger;
} GF_WidgetContent;

typedef struct _widget
{
	GF_WidgetManager *wm;

	u32 nb_instances;

	/* url to the file.wgt file when zip packaged or to the config.xml file when unpackaged */
	char *url;
	/* path to the manifest/config document */
	char *manifest_path;

	GF_List *icons;
	/*
		GF_WidgetContent *simple;
		char *icon_url;
	*/
	GF_WidgetContent *main;

	GF_WidgetPackage *wpack;

	/*misc metadata for W3C Widgets API*/
	char *name, *shortname, *identifier,
	     *authorName, *authorEmail, *authorHref,
	     *description, *version,
	     *uuid, *license, *licenseHref, *viewmodes;

	u32 width, height;

	GF_List *features;

	Bool discardable, multipleInstance;

	/*when a widget is being received from a remote peer,
	we remember where we locally store it to be able to further remote it
	This is only supported for packaged widgets*/
	char *local_path;
} GF_Widget;

typedef struct _widget_instance
{
	GF_Widget *widget;

	u32 instance_id;
	JSObject *obj;
	u8 secname[18];
	GF_SceneGraph *scene;
	/*node in the widget manager which holds the widget: Inline {} , <animation>, ...*/
	GF_Node *anchor;

	Bool activated, permanent;

	GF_List *output_triggers;
	GF_List *bound_ifces;
	/*list of components for a parent widget*/
	GF_List *components;
	/*parent of the widget for a component widget*/
	struct _widget_instance *parent;

	GF_DOMParser *mpegu_context;


	/*scripting context of the widget scene*/
	JSContext *scene_context;
	JSObject *scene_global;
	/*"Widget" object in the scene*/
	JSObject *scene_obj;
} GF_WidgetInstance;


typedef struct
{
	GF_WidgetInterface *ifce;
	GF_WidgetInstance *wid;
	char *hostname;
	JSObject *obj;
	JSObject *cookie;
} GF_WidgetInterfaceInstance;

typedef struct
{
	GF_WidgetComponent *comp;
	GF_WidgetInstance *wid;
} GF_WidgetComponentInstance;


GF_WidgetInstance *wm_load_widget(GF_WidgetManager *wm, const char *path, u32 InstanceID, Bool skip_context);


JSBool SMJS_FUNCTION(widget_has_feature);
JSBool SMJS_FUNCTION(widget_open_url);
JSBool SMJS_FUNCTION(widget_get_attention);
JSBool SMJS_FUNCTION(widget_show_notification);
JSBool SMJS_FUNCTION(widget_get_interface);
SMJS_DECL_FUNC_PROP_GET(widget_getProperty);
SMJS_DECL_FUNC_PROP_SET(widget_setProperty);

void widget_on_interface_bind(GF_WidgetInterfaceInstance *ifce, Bool unbind);

void widget_load(GF_WidgetManager *wm, GF_SceneGraph *scene, JSContext *c, JSObject *global, Bool unload);

GF_WidgetComponentInstance *wm_activate_component(JSContext *c, GF_WidgetInstance *wid, GF_WidgetComponent *comp, Bool skip_wm_notification);

void wm_deactivate_component(JSContext *c, GF_WidgetInstance *wid, GF_WidgetComponent *comp, GF_WidgetComponentInstance *comp_inst);



const char *wm_xml_get_attr(GF_XMLNode *root, const char *name);

GF_BaseInterface *LoadWidgetReader();
void ShutdownWidgetReader(GF_BaseInterface *ifce);


#endif /*GPAC_HAS_SPIDERMONKEY*/

#endif
