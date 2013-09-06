/*
 *		Copyright (c) 2010-2011 Telecom-Paristech
 *                 All Rights Reserved
 *	GPAC is free software; you can redistribute it and/or modify
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
 *		Authors:    Stanislas Selle		
 *				
 */
/*
** HbbTV Plugin PC
*/


#include <gdk/gdk.h>
#ifdef XP_UNIX
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif
#include <gtk/gtk.h>

#include "hbbtvbrowserplugin.h"
#include "oipfapplicationmanager.h"
#include "oipfconfiguration.h"
#include "videobroadcast.h"

#define MIMETYPEDESCRIPTION "application/hbbtvbrowserplugin::Hbbtv Browser Plugin;" \
							"video/broadcast::Video Broadcast;"\
                            "application/oipfapplicationmanager::Oipf Application Manager;"\
                            "application/oipfconfiguration::Oipf Configuration;"


#define PLUGIN_NAME			"hbbtvbrowserplugin"
#define PLUGIN_DESCRIPTION	"HbbTV browser plugin "
#define PLUGIN_VERSION		"0.20110704"



static void
fillPluginFunctionTable(NPPluginFuncs* pFuncs)
{
  TRACEINFO;
  pFuncs->version = 11;
  pFuncs->size = sizeof(*pFuncs);
  pFuncs->newp = OIPF_NPP_New;
  pFuncs->destroy = OIPF_NPP_Destroy;
  pFuncs->setwindow = OIPF_NPP_SetWindow;
  pFuncs->newstream = OIPF_NPP_NewStream;
//  pFuncs->destroystream = NPP_DestroyStream;
//  pFuncs->asfile = NPP_StreamAsFile;
//  pFuncs->writeready = NPP_WriteReady;
//  pFuncs->write = NPP_Write;
//  pFuncs->print = NPP_Print;
  pFuncs->event = OIPF_NPP_HandleEvent;
//  pFuncs->urlnotify = NPP_URLNotify;
  pFuncs->getvalue = OIPF_NPP_GetValue;
  pFuncs->setvalue = OIPF_NPP_SetValue;
}

char* booltostr(bool data)
{
    char *result;
    if (data)
        result = "true";
    else
        result = "false";
    return result;
}

/*
 void drawWindow (HBBTVPluginData* instanceData, GdkDrawable* gdkWindow)
{
  TRACEINFO;
  NPWindow window = *(instanceData->window);
  int x = window.x;
  int y = window.y;
  int width = window.width;
  int height = window.height;
  fprintf(stderr,"%p, x = %i  y=%i, width = %i height = %i\n",gdkWindow, x ,y,width,height);

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  const char* uaString = "DemoPlug"; // sBrowserFuncs->uagent(npp);
  if (!uaString)
    return;

  GdkGC* gdkContext = gdk_gc_new(gdkWindow);

  // draw a grey background for the plugin frame
  GdkColor grey;
  grey.red = 24000; grey.blue = 24000; grey.green = 24000;
  gdk_gc_set_rgb_fg_color(gdkContext, &grey);
  gdk_draw_rectangle(gdkWindow, gdkContext, TRUE, x, y, width, height);

  // draw a 3-pixel-thick black frame around the plugin
  GdkColor black;
  black.red = black.green = black.blue = 0;
  gdk_gc_set_rgb_fg_color(gdkContext, &black);
  gdk_gc_set_line_attributes(gdkContext, 3, GDK_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);
  gdk_draw_rectangle(gdkWindow, gdkContext, FALSE, x + 1, y + 1,
                     width - 3, height - 3);

  // paint the UA string
  PangoContext* pangoContext = gdk_pango_context_get();
  PangoLayout* pangoTextLayout = pango_layout_new(pangoContext);
  pango_layout_set_width(pangoTextLayout, (width - 10) * PANGO_SCALE);
  pango_layout_set_text(pangoTextLayout, uaString, -1);
  gdk_draw_layout(gdkWindow, gdkContext, x + 5, y + 5, pangoTextLayout);
  g_object_unref(pangoTextLayout);

  g_object_unref(gdkContext);
}
*/


NPError NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{
	TRACEINFO;

    sBrowserFuncs = bFuncs;
    
	fillPluginFunctionTable(pFuncs);

	return NPERR_NO_ERROR;
}

char* NP_GetPluginVersion()
{
    TRACEINFO;
	return PLUGIN_VERSION;
}

NPError NP_Shutdown()
{
    TRACEINFO;
	return NPERR_NO_ERROR;
}

char* NP_GetMIMEDescription()
{
    TRACEINFO;
	return MIMETYPEDESCRIPTION;
}


NPError NP_GetValue(void *instance, NPPVariable variable, void *value)
{
    TRACEINFO;
	switch (variable)
	{
		case NPPVpluginNameString :
			*(char**)value = PLUGIN_NAME;
			break;
		case NPPVpluginDescriptionString :
			*(char**)value = PLUGIN_DESCRIPTION;
			break;
		default :
			break;
	}
	return NPERR_NO_ERROR;
}

NPError OIPF_NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *saved)
{
	TRACEINFO;
	int i = 0;
	
	NPBool browserSupportsWindowless = false;
	sBrowserFuncs->getvalue(instance, NPNVSupportsWindowless, &browserSupportsWindowless);
	if (!browserSupportsWindowless)
	{		
		return NPERR_GENERIC_ERROR;
	}
	sBrowserFuncs->setvalue(instance, NPPVpluginWindowBool, (void*)false);
	sBrowserFuncs->setvalue(instance, NPPVpluginTransparentBool, (void*)false);
	

    NPObject* obj;
    for(i = 0 ; i<argc ; i++ ){
		if((strcmp(argn[i],"type")) == 0){
			instance->pdata = (HBBTVPluginData*)MEMALLOC(sizeof(HBBTVPluginData));
			HBBTVPluginData* pdata = ((HBBTVPluginData*)instance->pdata);
			
			if (strcmp(argv[i], "application/oipfApplicationManager") == 0)
			{
				obj = sBrowserFuncs->createobject(instance, fillOAMpclass());				
			}
			else if (strcmp(argv[i], "application/oipfConfiguration") == 0)
			{
				obj = sBrowserFuncs->createobject(instance, fillOCFGpclass());				
			}
			else if (strcmp(argv[i], "video/broadcast") == 0)
			{
				obj = sBrowserFuncs->createobject(instance, fillVIDBRCpclass());								
			}else{
				
				obj = NULL;
			}
			pdata->type = (char*)MEMALLOC(strlen(argv[i]));
			strcpy(pdata->type, argv[i] );
			pdata->obj = obj;
			pdata->npp = instance;
			
		}else{
			
			obj = NULL;
		}
	}

	return NPERR_NO_ERROR;
}

NPError OIPF_NPP_Destroy(NPP instance, NPSavedData **save)
{
    TRACEINFO;
    HBBTVPluginData* pdata = (HBBTVPluginData*)instance->pdata;
    if (pdata)
    {		
        sBrowserFuncs->releaseobject(pdata->obj);
        MEMFREE(pdata->type);
        MEMFREE(pdata);
    }
    return NPERR_NO_ERROR;
}


NPError OIPF_NPP_SetWindow(NPP instance, NPWindow *window)
{
	TRACEINFO;
	
	
	
	HBBTVPluginData* pdata = (HBBTVPluginData*)instance->pdata;
	if(pdata)
	{
		pdata->window = window;
		if (pdata->type)
		{
			if (strcmp(pdata->type, "video/broadcast") == 0)
			{
				fprintf(stderr, "%s found \n", pdata->type);
				if (pdata->obj)
				{
					NPObj_VidBrc* vidbrcobj  = (NPObj_VidBrc*)pdata->obj;
					if (!(vidbrcobj->fullscreen))
					{
						OnNoFullscreenSetWindow(pdata->window->x,
											pdata->window->y,
											pdata->window->width,
											pdata->window->height);
					}
				}
			}
		}
		/*fprintf(stderr, "\tWindow found : %p\n", window);
		fprintf(stderr, "\twindow = %p ws_info %p \n\tx = %d, y= %d\n\tclipRect.left = %d clipRect.top= %d\n\twidth=%d,  height= %d\n",
			pdata->window->window,
			pdata->window->ws_info,
			pdata->window->x,
			pdata->window->y,
			pdata->window->clipRect.left,
			pdata->window->clipRect.top,
			pdata->window->width,
			pdata->window->height);*/
	}
	
	return NPERR_NO_ERROR;
}

NPError OIPF_NPP_HandleEvent(NPP instance, void* event)
{
	 //TRACEINFO; // frequent event, avoid over log...

	XEvent *nativeEvent = (XEvent*)event;
	if (nativeEvent->type == GraphicsExpose)
	{
		XGraphicsExposeEvent *expose = &nativeEvent->xgraphicsexpose;
		GC gc = XCreateGC(expose->display, expose->drawable, 0, 0);	
		XFillRectangle(expose->display, expose->drawable, gc, expose->x, expose->y,expose->width, expose->height);
		XFreeGC(expose->display, gc);
		return true;
	}

	
	return false;
}



NPError OIPF_NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    TRACEINFO;

    if ( variable == NPPVpluginScriptableNPObject )
    {			
        void ** v = (void **)value;        
        if(instance->pdata)
        {
        NPObject* obj = ((HBBTVPluginData*)instance->pdata)->obj;
		if(obj == NULL){			
		}else{			
			sBrowserFuncs->retainobject(obj);
		}	
		*v = obj;
		}
    }
	return NPERR_NO_ERROR;
}


NPError OIPF_NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    TRACEINFO;
	return NPERR_GENERIC_ERROR;
}


NPError OIPF_NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
    TRACEINFO;
	return NPERR_GENERIC_ERROR;
}

