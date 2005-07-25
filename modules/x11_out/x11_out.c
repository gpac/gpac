#include "x11_out.h"
#include <gpac/constants.h>

int XShmMajor, XShmMinor;

void ff(char* debug_string){
#ifdef GF_DEBUG
	fprintf(stdout, debug_string);
	fprintf(stdout, "\n");
#endif
}

//=====================================
/*
 * Flush video: draw main image(surface/pixmap) -> os_handle
 */
//=====================================
GF_Err
X11_FlushVideo (struct _video_out *vout, GF_Window * dest)
{
	/*
	 * write backbuffer to screen
	 */
	X11VID ();
	switch (xWindow->videoaccesstype)
	{
#ifdef __USE_X_SHAREDMEMORY__
	case VIDEO_XI_SHMSTD:
		XShmPutImage (xWindow->display, xWindow->os_handle,
			      xWindow->gc, xWindow->surface, 0, 0, dest->x,
			      dest->y, dest->w, dest->h, True);
		ff ("VIDEO_XI_SHMSTD put\n");
		break;

	case VIDEO_XI_SHMPIXMAP:
		XClearWindow (xWindow->display, xWindow->os_handle);
		// we don't need to do any things else, when the backbuffer filled, it's drawn automatically
		ff ("VIDEO_XI_SHMPIXMAP put\n");
		break;
#endif
	case VIDEO_XI_STANDARD:
		XPutImage (xWindow->display, xWindow->os_handle, xWindow->gc,
			   xWindow->surface, 0, 0, dest->x, dest->y, dest->w,
			   dest->h);
		break;
	}
	return GF_OK;
}

//=====================================
/*
 * Translate X_Key to GF_Key
 */
//=====================================
static u32
X11_TranslateActionKey (u32 VirtKey)
{
	switch (VirtKey)
	{
	case XK_Home:
		return GF_VK_HOME;
	case XK_End:
		return GF_VK_END;
	case XK_Page_Up:
		return GF_VK_PRIOR;
	case XK_Page_Down:
		return GF_VK_NEXT;
	case XK_Up:
		return GF_VK_UP;
	case XK_Down:
		return GF_VK_DOWN;
	case XK_Left:
		return GF_VK_LEFT;
	case XK_Right:
		return GF_VK_RIGHT;
	case XK_F1:
		return GF_VK_F1;
	case XK_F2:
		return GF_VK_F2;
	case XK_F3:
		return GF_VK_F3;
	case XK_F4:
		return GF_VK_F4;
	case XK_F5:
		return GF_VK_F5;
	case XK_F6:
		return GF_VK_F6;
	case XK_F7:
		return GF_VK_F7;
	case XK_F8:
		return GF_VK_F8;
	case XK_F9:
		return GF_VK_F9;
	case XK_F10:
		return GF_VK_F10;
	case XK_F11:
		return GF_VK_F11;
	case XK_F12:
		return GF_VK_F12;
	case XK_KP_Enter:
		return GF_VK_RETURN;
	case XK_Return:
		return GF_VK_RETURN;
	case XK_Escape:
		return GF_VK_ESCAPE;
	case XK_Shift_L:
	case XK_Shift_R:
		return GF_VK_SHIFT;
	case XK_Control_L:
	case XK_Control_R:
		return GF_VK_CONTROL;
	case XK_Alt_L:
	case XK_Alt_R:
		return GF_VK_MENU;
	default:
		return 0;
	}
}

//=====================================
/*
 * detect mouse pos
 */
//=====================================
static void
X11_MapBIFSCoordinate (XWindow * xWindow, XEvent * xevent, GF_Event * evt)
{
	if (xWindow->fullscreen && xWindow->is_3D_out)
	{
		evt->mouse.x =
			xevent->xmotion.x - xWindow->display_width / 2;
		evt->mouse.y =
			xWindow->display_height / 2 - xevent->xmotion.y;
	}
	else
	{
		evt->mouse.x = xevent->xmotion.x - xWindow->width / 2;
		evt->mouse.y = xWindow->height / 2 - xevent->xmotion.y;
	}
}

//=====================================
/*
 * handle X11 events
 * here we handle key, mouse, repaint and window sizing events
 */
//=====================================
u32 X11_EventProc (void *par)
{
	GF_VideoOutput *vout = (GF_VideoOutput *) par;
	GF_Event evt;

	X11VID ();

	u32 last_mouse_move;

	xWindow->x11_th_state = 1;
	XEvent xevent;

	char ukey;
	while (xWindow->x11_th_state)
	{
		while (XCheckWindowEvent
		       (xWindow->display, xWindow->os_handle,
			StructureNotifyMask | ExposureMask | KeyPressMask |
			KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			PointerMotionMask, &xevent))
		{
			switch (xevent.type)
			{
//======================================
				/*
				 * X11 window resized event
				 * must inform GPAC to resize os_handle wnd
				 */
			case ConfigureNotify:
				if ((unsigned int) xevent.xconfigure.width !=
				    xWindow->width
				    || (unsigned int) xevent.xconfigure.
				    height != xWindow->height)
				{
					evt.type = GF_EVT_WINDOWSIZE;
					evt.size.width =
						xevent.xconfigure.width;
					evt.size.height =
						xevent.xconfigure.height;
					vout->on_event (vout->evt_cbk_hdl,
							&evt);
				}
				break;
//======================================
				/*
				 * Windows need repaint
				 */
			case Expose:
				if (xevent.xexpose.count > 0)
					break;
				evt.type = GF_EVT_REFRESH;
				evt.size.width = xevent.xexpose.width;
				evt.size.height = xevent.xexpose.height;
				vout->on_event (vout->evt_cbk_hdl, &evt);
				break;
//======================================
			case KeyPress:
			case KeyRelease:
				evt.key.vk_code =
					X11_TranslateActionKey (xevent.xkey.
								keycode);
				evt.key.virtual_code =
					XKeycodeToKeysym (xWindow->display,
							  xevent.xkey.keycode,
							  0);
				if (evt.key.vk_code)
				{
					#ifdef GF_DEBUG
					fprintf(stdout, "%d\n",evt.key.vk_code);
					#endif
					evt.type = (xevent.type ==KeyPress) ? GF_EVT_VKEYDOWN :GF_EVT_VKEYUP;
					if (evt.key.vk_code <=GF_VK_RIGHT)
						evt.key.virtual_code = 0;
					vout->on_event (vout->evt_cbk_hdl, &evt);
					/*also send a normal key for non-key-sensors */
					if (evt.key.vk_code >
					    GF_VK_RIGHT)
						goto send_key;
				}
				else
				{
				    send_key:
					XLookupString (&xevent.xkey, &ukey, 1,
						       NULL, NULL);
					evt.type =
						(xevent.type ==
						 KeyPress) ? GF_EVT_VKEYDOWN :
						GF_EVT_VKEYUP;
					vout->on_event (vout->evt_cbk_hdl,
							&evt);

					if ((xevent.type == KeyPress) && ukey)
					{
						evt.character.
							unicode_char = ukey;
						evt.type = GF_EVT_CHAR;
						vout->on_event (vout->
								evt_cbk_hdl,
								&evt);
					}
				}
				break;
//======================================
			case ButtonPress:
			case ButtonRelease:
				last_mouse_move =
					((XButtonEvent *) & xevent)->time;
				X11_MapBIFSCoordinate (xWindow, &xevent,
						       &evt);
				switch (((XButtonEvent *) & xevent)->button)
				{
				case Button1:
					evt.type =
						(xevent.type ==
						 ButtonRelease) ? GF_EVT_LEFTUP :
						GF_EVT_LEFTDOWN;
					vout->on_event (vout->evt_cbk_hdl,
							&evt);
					break;
				case Button3:
					evt.type =
						(xevent.type ==
						 ButtonRelease) ? GF_EVT_MIDDLEUP
						: GF_EVT_MIDDLEDOWN;
					vout->on_event (vout->evt_cbk_hdl,
							&evt);
					break;
				case Button2:
					evt.type =
						(xevent.type ==
						 ButtonRelease) ? GF_EVT_RIGHTUP
						: GF_EVT_RIGHTDOWN;
					vout->on_event (vout->evt_cbk_hdl,
							&evt);
					break;
				}
				break;
//======================================
			case MotionNotify:
				last_mouse_move =
					((XButtonEvent *) & xevent)->time;
				evt.type = GF_EVT_MOUSEMOVE;
				X11_MapBIFSCoordinate (xWindow, &xevent,
						       &evt);
				vout->on_event (vout->evt_cbk_hdl, &evt);
				break;
//======================================
			case PropertyNotify:
				break;
			case MapNotify:
				break;
			case CirculateNotify:
				break;
			case UnmapNotify:
				break;
			case ReparentNotify:
				break;
//======================================
			case DestroyNotify:
				xWindow->x11_th_state = 0;
				evt.type = GF_EVT_QUIT;
				vout->on_event (vout->evt_cbk_hdl, &evt);
				break;
//======================================
			default:
				break;
			}
		}
		gf_sleep (5);
	}			/* End while */
	return 0;
}

//=====================================
/*
 * 
 */
//=====================================

void
X11_DeleteBackBuffer (GF_VideoOutput * vout)
{
	X11VID ();
	if (xWindow->is_init == 1)
	{
		switch (xWindow->videoaccesstype)
		{
#ifdef __USE_X_SHAREDMEMORY__
		case VIDEO_XI_SHMSTD:
			ff ("VIDEO_XI_SHMSTD delete buff\n");
			XShmDetach (xWindow->display, xWindow->shmseginfo);
			if (xWindow->surface)
				XDestroyImage (xWindow->surface);
			if (xWindow->shmseginfo->shmaddr)
				shmdt (xWindow->shmseginfo->shmaddr);
			if (xWindow->shmseginfo->shmid >= 0)
				shmctl (xWindow->shmseginfo->shmid, IPC_RMID,
					NULL);
			free (xWindow->shmseginfo);
			break;
		case VIDEO_XI_SHMPIXMAP:
			ff ("VIDEO_XI_SHMPIXMAP delete buff\n");
			XShmDetach (xWindow->display, xWindow->shmseginfo);
			XFreePixmap (xWindow->display, xWindow->pixmap);
			if (xWindow->shmseginfo->shmaddr)
				shmdt (xWindow->shmseginfo->shmaddr);
			if (xWindow->shmseginfo->shmid >= 0)
				shmctl (xWindow->shmseginfo->shmid, IPC_RMID,
					NULL);
			free (xWindow->shmseginfo);
			break;
#endif
		case VIDEO_XI_STANDARD:
			if (xWindow->back_buffer)
			{
				if (xWindow->back_buffer->buffer)
				{
					free (xWindow->back_buffer->buffer);
				}
			}
			if (xWindow->surface)
				XFree (xWindow->surface);
			break;
		}
		xWindow->is_init = 0;
	}
}

//=====================================
/*
 * init backbuffer
 */
//=====================================
GF_Err
X11_InitBackBuffer (GF_VideoOutput * vout, u32 VideoWidth, u32 VideoHeight)
{
	ff("Init back buffer");
	#ifdef GF_DEBUG
	fprintf(stdout,"%dx%d\n",VideoWidth, VideoHeight);
	#endif
	u32 size;
	VideoWidth = VideoWidth > 32 ? VideoWidth : 32;
	VideoWidth = VideoWidth < 4096 ? VideoWidth : 4096;
	VideoHeight = VideoHeight > 32 ? VideoHeight : 32;
	VideoHeight = VideoHeight > 4096 ? 4096 : VideoHeight;

	X11VID ();
	if (!xWindow || !VideoWidth || !VideoHeight)
		return GF_BAD_PARAM;

	gf_mx_p(xWindow->mx);

	X11_DeleteBackBuffer (vout);

	xWindow->back_buffer->BPP = xWindow->bpp;
	xWindow->back_buffer->width = VideoWidth;
	xWindow->back_buffer->height = VideoHeight;
	xWindow->back_buffer->pitch = VideoWidth * xWindow->bpp;
	xWindow->is_init = 1;
	size = VideoWidth * VideoHeight * xWindow->bpp;

	switch (xWindow->videoaccesstype)
	{
#ifdef __USE_X_SHAREDMEMORY__
	case VIDEO_XI_SHMPIXMAP:
		xWindow->shmseginfo =
			(XShmSegmentInfo *) malloc (sizeof (XShmSegmentInfo));
		ff ("SMHPIXMAP ... ");
		memset (xWindow->shmseginfo, 0, sizeof (XShmSegmentInfo));
		xWindow->shmseginfo->shmid = shmget (IPC_PRIVATE,
						     size, IPC_CREAT | 0777);
		ff ("ID ... ");
		xWindow->back_buffer->buffer =
			(unsigned char *) (xWindow->shmseginfo->shmaddr =
					   shmat (xWindow->shmseginfo->shmid,
						  0, 0));
		xWindow->shmseginfo->readOnly = False;
		XShmAttach (xWindow->display, xWindow->shmseginfo);
		ff ("Attach ... ");
		xWindow->pixmap =
			XShmCreatePixmap (xWindow->display,
					  xWindow->os_handle,
					  xWindow->back_buffer->buffer,
					  xWindow->shmseginfo, VideoWidth,
					  VideoHeight, xWindow->depth);
		XSetWindowBackgroundPixmap (xWindow->display,
					    xWindow->os_handle,
					    xWindow->pixmap);
		ff ("OK\n");
		break;

	case VIDEO_XI_SHMSTD:
		xWindow->shmseginfo =
			(XShmSegmentInfo *) malloc (sizeof (XShmSegmentInfo));
		ff ("SHMSTD ... ");

		memset (xWindow->shmseginfo, 0, sizeof (XShmSegmentInfo));

		xWindow->surface =
			XShmCreateImage (xWindow->display, xWindow->visual,
					 xWindow->depth, ZPixmap, NULL,
					 xWindow->shmseginfo, VideoWidth,
					 VideoHeight);
		ff ("IMG ... ");

		xWindow->shmseginfo->shmid = shmget (IPC_PRIVATE,
						     xWindow->surface->
						     bytes_per_line *
						     xWindow->surface->height,
						     IPC_CREAT | 0777);
		ff ("ID ... ");

		xWindow->back_buffer->buffer =
			(unsigned char *) (xWindow->surface->data =
					   xWindow->shmseginfo->shmaddr =
					   shmat (xWindow->shmseginfo->shmid,
						  NULL, 0));
		ff ("BUFFER ... ");
		xWindow->shmseginfo->readOnly = False;
		XShmAttach (xWindow->display, xWindow->shmseginfo);
		ff ("Attach ... OK\n");
		break;
#endif

	case VIDEO_XI_STANDARD:
		xWindow->back_buffer->buffer =
			(unsigned char *) malloc (size * sizeof (unsigned char));

		xWindow->surface =
			XCreateImage (xWindow->display, xWindow->visual,
				      xWindow->depth, ZPixmap,
				      0,
				      xWindow->back_buffer->buffer,
				      VideoWidth, VideoHeight,
				      xWindow->bpp*8, xWindow->back_buffer->pitch);
	}
	gf_mx_v(xWindow->mx);
	return GF_OK;
}

//=====================================
/*
 * Shutdown X11
 */
//=====================================
void
X11_Shutdown (struct _video_out *vout)
{
	/*
	 * free all assoiated resources 
	 */
	// /*delete window if created by plugin*/
	X11VID ();
	gf_mx_p (xWindow->mx);


	while (gf_list_count (xWindow->surfaces))
	{
		X11WrapSurface *ptr =
			(X11WrapSurface *) gf_list_get(xWindow->surfaces,
							  0);
		gf_list_rem(xWindow->surfaces, 0);
		if (ptr)
		{
			if (ptr->buffer)
				free (ptr->buffer);
			free (ptr);
		}
	}
	gf_list_del(xWindow->surfaces);

	X11_DeleteBackBuffer (vout);


	XFreeGC (xWindow->display, xWindow->gc);

	if (xWindow->ext_wnd)
	{
		XUnmapWindow (xWindow->display, (Window) xWindow->normal_wnd);
	}

	XDestroyWindow (xWindow->display, (Window) xWindow->normal_wnd);
	XDestroyWindow (xWindow->display, (Window) xWindow->full_wnd);

	XCloseDisplay (xWindow->display);

	xWindow->x11_th_state = 0;

	gf_sleep (5);

	gf_th_del(xWindow->th);	//sometimes get hung here

	gf_mx_v (xWindow->mx);

	gf_mx_del(xWindow->mx);

	free (xWindow);
}

//=====================================
/*
 * resize buffer: note we don't resize window here
 */
//=====================================
GF_Err
X11_ResizeBackBuffer (struct _video_out *vout, u32 newWidth, u32 newHeight)
{

	/*
	 * resize BACKBUFFER & associated resource || recreate openGL context
	 */
	X11VID ();
	#ifdef GF_DEBUG
	fprintf(stdout, "requesting resize back buffer from %dx%d to %dx%d\n",
		xWindow->back_buffer->width, xWindow->back_buffer->height,
		newWidth,newHeight);
	#endif
	if ((newWidth != xWindow->back_buffer->width)
	    || (newHeight != xWindow->back_buffer->height))
		if ((newWidth >= 32) && (newHeight >= 32))
		{
			ff("resizing backbuffer");
			return X11_InitBackBuffer (vout, newWidth, newHeight);
		}
	return GF_OK;
}



/*
 * window events: only set cursor, set title, set style and set visible
 * used
 */
GF_Err
X11_ProcessEvent (struct _video_out * vout, GF_Event * evt)
{
	X11VID ();
	GF_Window a_wnd;
	switch (evt->type)
	{
	case GF_EVT_REFRESH:
		gf_mx_p (xWindow->mx);
		a_wnd.w = evt->size.width;
		a_wnd.h = evt->size.height;
		a_wnd.x = a_wnd.y = 0;
		X11_FlushVideo (vout, &a_wnd);
		gf_mx_v (xWindow->mx);
		break;
	case GF_EVT_SET_CURSOR:

		break;
	case GF_EVT_SET_CAPTION:

		break;
	case GF_EVT_SHOWHIDE:

		break;
	case GF_EVT_SCENESIZE:
		gf_mx_p (xWindow->mx);
		#ifdef GF_DEBUG
		fprintf(stdout, "resizing window to %d %d\n", evt->size.width, evt->size.height);
		#endif
		xWindow->width = evt->size.width, xWindow->height =
			evt->size.height;
		XResizeWindow (xWindow->display, xWindow->os_handle,
			       evt->size.width, evt->size.height);
		gf_mx_v(xWindow->mx);
		break;
	}
	return GF_OK;

}

//=====================================
/*
 * switch from/to full screen mode
 */
//=====================================
GF_Err
X11_SetFullScreen (struct _video_out * vout, u32 bFullScreenOn,
		   u32 * screen_width, u32 * screen_height)
{
	X11VID ();
	#ifdef GF_DEBUG
	fprintf(stdout, "Set Full Screen: %dx%d\n", *screen_width, *screen_height);
	#endif
	gf_mx_p (xWindow->mx);
	xWindow->fullscreen = bFullScreenOn;
	if (bFullScreenOn)
	{
		xWindow->store_width = xWindow->width;
		xWindow->store_height = xWindow->height;
		
		xWindow->width = xWindow->display_width;
		xWindow->height = xWindow->display_height;

		XMoveResizeWindow (xWindow->display,
				   (Window) xWindow->full_wnd, 0, 0,
				   xWindow->display_width,
				   xWindow->display_height);
		if (!xWindow->ext_wnd)
			XUnmapWindow (xWindow->display,
				      (Window) xWindow->normal_wnd);
		xWindow->os_handle = (Window) xWindow->full_wnd;
		*screen_width = xWindow->width;
		*screen_height = xWindow->height;
	}
	else
	{
		XUnmapWindow (xWindow->display, (Window) xWindow->full_wnd);
		xWindow->os_handle = (Window) xWindow->normal_wnd;
		xWindow->width = xWindow->store_width;
		xWindow->height = xWindow->store_height;
		X11_ResizeBackBuffer(vout, xWindow->width, xWindow->height);
		*screen_width = xWindow->width;
		*screen_height = xWindow->height;
	}

	XMapRaised (xWindow->display, xWindow->os_handle);
	gf_mx_v (xWindow->mx);
	return GF_OK;
}

/*
 * clears screen with specified color
 */
GF_Err
X11_Clear (struct _video_out * vout, u32 color)
{
	X11VID ();
	return GF_OK;

	gf_mx_p (xWindow->mx);
	switch (xWindow->videoaccesstype)
	{
	case VIDEO_XI_STANDARD:
		// TODO convert color to PIXEL
		XSetWindowBackground (xWindow->display, xWindow->os_handle,
				      color);
		XClearWindow (xWindow->display, xWindow->os_handle);
		break;
	}
	gf_mx_v (xWindow->mx);
	return GF_OK;
}

/*
 * lock video mem
 */
GF_Err
X11_LockSurface (struct _video_out * vout, u32 surface_id,
		 GF_VideoSurface * vi)
{
	u32 i;
	X11VID ();

	gf_mx_p (xWindow->mx);
	if (!surface_id)
	{
		vi->width = xWindow->back_buffer->width;
		vi->height = xWindow->back_buffer->height;
		vi->pitch = xWindow->back_buffer->pitch;
		vi->pixel_format = xWindow->pixel_format;
		vi->os_handle = NULL;
		vi->video_buffer = xWindow->back_buffer->buffer;
		gf_mx_v (xWindow->mx);
		return GF_OK;
	}
	/*
	 * check surfaces 		gf_mx_p (xWindow->mx);

	 */
	for (i = 0; i < gf_list_count(xWindow->surfaces); i++)
	{
		X11WrapSurface *ptr =
			(X11WrapSurface *) gf_list_get(xWindow->surfaces,
							  i);
		if (ptr->id == surface_id)
		{
			vi->width = ptr->width;
			vi->height = ptr->height;
			vi->pitch = ptr->pitch;
			vi->pixel_format = ptr->pixel_format;
			vi->os_handle = NULL;
			vi->video_buffer = ptr->buffer;
			gf_mx_v (xWindow->mx);
			return GF_OK;
		}
	}
	gf_mx_v (xWindow->mx);
	return GF_BAD_PARAM;
}

/*
 * unlock video mem actually do nothing ???
 */
GF_Err
X11_UnlockSurface (struct _video_out * vout, u32 surface_id)
{
	return GF_OK;
}

/*
 * creates a offscreen video surface and setup surface id - pixel format
 * MUST be respected except for YUV formats, where the hardware is free to 
 * choose the fastest format for blit
 */
static GF_Err
X11_CreateSurface (struct _video_out *vout, u32 width, u32 height,
		   u32 pixel_format, u32 * surfaceID)
{
	u32 size;
	X11VID ();
	if (!surfaceID) return X11_InitBackBuffer(vout, width, height);
	gf_mx_p (xWindow->mx);
	X11WrapSurface *surf;
	surf = (X11WrapSurface *) malloc (sizeof (X11WrapSurface));

	switch (pixel_format)
	{
	case GF_PIXEL_RGB_555:		
		surf->BPP = 2;
		break;
	case GF_PIXEL_RGB_565:
		surf->BPP = 2;
		break;
	case GF_PIXEL_RGB_24:		
		surf->BPP = 3;
		break;
	case GF_PIXEL_RGB_32:		
		surf->BPP = 4;
		break;
	}
	
	surf->pitch = width * surf->BPP;
	size = height * surf->pitch;
	surf->buffer = (unsigned char *) malloc (sizeof (unsigned char) * size);
	surf->pixel_format = pixel_format;
	surf->id = (u32) surf;
	gf_list_add(xWindow->surfaces, surf);
	*surfaceID = surf->id;
	gf_mx_v (xWindow->mx);
	return GF_OK;
}

// /*deletes video surface by id*/
GF_Err
X11_DeleteSurface (struct _video_out * vout, u32 surface_id)
{
	u32 i;
	X11VID ();
    if (!surface_id)
		return GF_BAD_PARAM;

	gf_mx_p (xWindow->mx);

	/*
	 * check surfaces
	 */
	for (i = 0; i < gf_list_count (xWindow->surfaces); i++)
	{
		X11WrapSurface *ptr =
			(X11WrapSurface *) gf_list_get (xWindow->surfaces,
							  i);
		if (ptr->id == surface_id)
		{
			if (ptr->buffer)
				free (ptr->buffer);
			free (ptr);
			gf_list_rem (xWindow->surfaces, i);
			gf_mx_v (xWindow->mx);
			return GF_OK;
		}
	}
	gf_mx_v (xWindow->mx);
	return GF_BAD_PARAM;
}

//=====================================
/*
 *
 */
//=====================================
X11WrapSurface *
X11_GetSurface (struct _video_out * vout, u32 surface_id)
{
	/*
	 * not important for now (only needed for image & video display) cd
	 * SDL_video2d.c
	 */
	u32 i;

	X11VID ();
	if (!surface_id)
		return NULL;

	/*
	 * check surfaces
	 */
	for (i = 0; i < gf_list_count (xWindow->surfaces); i++)
	{
		X11WrapSurface *ptr =
			(X11WrapSurface *) gf_list_get (xWindow->surfaces,
							  i);
		if (ptr->id == surface_id)
		{
			return ptr;
		}
	}
	return NULL;
}


/*
 * checks if the surface is valid - this is used to discard surfaces when
 * changing video mode (fullscreen)
 */
u32
X11_IsSurfaceValid (struct _video_out * vout, u32 surface_id)
{
	return (X11_GetSurface(vout, surface_id) != NULL) ? 1 : 0;
}

/*
 * resize surface - the resulting surface can still be larger than what
 * requested
 */
GF_Err
X11_ResizeSurface (struct _video_out * vout, u32 surface_id,
		   u32 width, u32 height)
{
	X11WrapSurface *wrap;
	X11VID ();
	//do nothing ????
	#ifdef GF_DEBUG
	fprintf(stdout, "requesting resize surface %d to %dx%d \n",
		surface_id, width, height);
	#endif
	
	if (!surface_id) return X11_ResizeBackBuffer(vout, width, height);
	
	gf_mx_p(xWindow->mx);
	wrap = X11_GetSurface(vout, surface_id);
	if (!wrap || !wrap->BPP) {
		gf_mx_v(xWindow->mx);
		return GF_BAD_PARAM;
	}
	if ((wrap->width>= width) && (wrap->height>=height)) {
		#ifdef GF_DEBUG
		f(" do nothing");
		#endif
		gf_mx_v(xWindow->mx);
		return GF_OK;
	}
	
	ff(" resizing...");
	free(wrap->buffer);
	wrap->pitch = wrap->BPP * width;
	wrap->width = width;
	wrap->height = height;
	wrap->buffer = malloc(sizeof(char) * wrap->pitch * wrap->height);
	ff(" OK");
	return GF_OK;
}


/*
 * blit surface src to surface dest - if a window is not specified, the
 * full surface is used _ can be NULL
 */
GF_Err
X11_Blit (struct _video_out * vout, u32 src_id, u32 dst_id,
	  GF_Window * src, GF_Window * dst)
{
	void *pdst, *psrc;
	X11VID ();
	if (dst_id) return GF_NOT_SUPPORTED;

	gf_mx_p (xWindow->mx);
	#ifdef GF_DEBUG
	fprintf(stdout,"blit %d,%d %dx%d->%d,%d %dx%d\n",
		src->x,src->y,src->w,src->h,dst->x,dst->y,dst->w,dst->h);
	#endif
	X11WrapSurface *dest_surf = xWindow->back_buffer;
	X11WrapSurface *src_surf = X11_GetSurface (vout, src_id);

	if (0 && src_surf && src_surf->buffer && (src->x == dst->x)
	    && (src->y == dst->y) && (src->w == dst->w) && (src->h == dst->h))
	{
		memcpy(dest_surf->buffer, src_surf->buffer, sizeof(char)*src_surf->pitch*src_surf->height);
	}
	else
	{
		pdst = dest_surf->buffer +
			dst->y * xWindow->back_buffer->pitch +
			dst->x * xWindow->bpp;
		int dst_depth = xWindow->bpp*8;
//		int src_depth =
//			src_surf->pixel_format == GF_PIXEL_RGB_565 ? 16 : 32;
		int src_depth = src_surf->BPP*8;
//		int src_bpp = src_surf->pixel_format == GF_PIXEL_RGB_565 ? 2 : 4;
		int src_bpp = src_surf->BPP;
		
		psrc = src_surf->buffer + src->y * src_surf->pitch +
			src->x * src_bpp;
		StretchBits (pdst, dst_depth, dst->w, dst->h,
			     dest_surf->pitch, psrc, src_depth, src->w,
			     src->h, src_surf->pitch, 0);
	}
	gf_mx_v (xWindow->mx);
	return GF_OK;
}

/*
 * returns pixel format of the surface - if surfaceID is 0, the main video
 * memory format is requested
 */
GF_Err
X11_GetPixelFormat (struct _video_out * vout, u32 surfaceID,
		    u32 * pixel_format)
{
	u32 i;
	X11VID ();
	gf_mx_p (xWindow->mx);
	if (!surfaceID)
	{
		*pixel_format = xWindow->pixel_format;
		gf_mx_v (xWindow->mx);
		return GF_OK;
	}

	/*
	 * check surfaces
	 */
	for (i = 0; i < gf_list_count (xWindow->surfaces); i++)
	{
		X11WrapSurface *ptr =
			(X11WrapSurface *) gf_list_get (xWindow->surfaces,
							  i);

		if (ptr->id == surfaceID)
		{
			*pixel_format = ptr->pixel_format;
			gf_mx_v (xWindow->mx);
			return GF_OK;
		}
	}
	gf_mx_v (xWindow->mx);
	return GF_OK;
}

//=====================================
/*
 * Setup X11 wnd System
 */
//=====================================
void
X11_SetupWindow (GF_VideoOutput * vout)
{
	X11VID ();

	/*
	 * normal X11 routine
	 */
	//==============================================
	if (!xWindow->display)
		xWindow->display = XOpenDisplay (NULL);

	xWindow->screennum = DefaultScreen (xWindow->display);

	xWindow->screenptr = DefaultScreenOfDisplay (xWindow->display);
	xWindow->visual = DefaultVisualOfScreen (xWindow->screenptr);
	xWindow->depth = DefaultDepth (xWindow->display, xWindow->screennum);

	switch (xWindow->depth)
	{
	case 8:
		xWindow->pixel_format = GF_PIXEL_GREYSCALE;
		break;
	case 16:
		xWindow->pixel_format = GF_PIXEL_RGB_565;
		break;
	case 24:
		xWindow->pixel_format = GF_PIXEL_RGB_32;
		break;
	default:
		xWindow->pixel_format = GF_PIXEL_GREYSCALE;
		break;
	}
	xWindow->bpp = xWindow->depth / 8;
	xWindow->bpp = xWindow->bpp == 3 ? 4 : xWindow->bpp;

	char *title = "Test de X11_out";
	if (title)
	{
		xWindow->title = (char *) malloc (strlen (title));
		strcpy (xWindow->title, title);
	}

	xWindow->display_width =
		DisplayWidth (xWindow->display, xWindow->screennum);
	xWindow->display_height =
		DisplayHeight (xWindow->display, xWindow->screennum);
	//==============================================

	/*
	 * Full screen wnd
	 */
	Window fullscreen = XCreateWindow (xWindow->display,
					   RootWindowOfScreen (xWindow->
							       screenptr),
					   0, 0,
					   xWindow->display_width,
					   xWindow->display_height, 0,
					   xWindow->depth, InputOutput,
					   xWindow->visual, 0, NULL);

	xWindow->full_wnd = (Window *) fullscreen;

	XSelectInput (xWindow->display, (Window) xWindow->full_wnd,
		      ExposureMask |
		      PointerMotionMask | ButtonReleaseMask | ButtonPressMask
		      | KeyPressMask | KeyReleaseMask);
	//==============================================	

	/*
	 * normal wnd
	 */
	//==============================================
	Window normal_wnd;
	if (!xWindow->ext_wnd)
	{
		normal_wnd =
			XCreateWindow (xWindow->display,
				       RootWindowOfScreen (xWindow->
							   screenptr), 0, 0,
				       xWindow->width, xWindow->height, 0,
				       xWindow->depth, InputOutput,
				       xWindow->visual, 0, NULL);
	}
	else
	{
		Window win_tmp;
		unsigned int tmp1, tmp2;	// don't know :) ask vlc guys
		XGetGeometry (xWindow->display, (Window) xWindow->ext_wnd,
			      &win_tmp, &tmp1, &tmp2, &xWindow->width,
			      &xWindow->height, &tmp1, &tmp2);

		normal_wnd =
			XCreateWindow (xWindow->display,
				       (Window) xWindow->ext_wnd,
				       0, 0,
				       xWindow->width, xWindow->height, 0,
				       xWindow->depth, 0,
				       xWindow->visual, 0, NULL);

	}

	xWindow->normal_wnd = (Window *) normal_wnd;
	/*
	 * base os_handle
	 */
	//==============================================
	xWindow->os_handle = (Window) xWindow->normal_wnd;

	XSelectInput (xWindow->display, xWindow->os_handle,
		      StructureNotifyMask | PropertyChangeMask | ExposureMask
		      | PointerMotionMask | ButtonReleaseMask |
		      ButtonPressMask | KeyPressMask | KeyReleaseMask);
	//==============================================

	/*
	 * Size Hints for nds
	 */
	//==============================================
	XSizeHints *Hints = XAllocSizeHints ();
	Hints->flags = PSize | PMinSize;
	Hints->min_width = 32;
	Hints->min_height = 32;
	Hints->max_height = 4096;
	Hints->max_width = 4096;
	XSetWMNormalHints (xWindow->display, xWindow->os_handle, Hints);
	Hints->x = 0;
	Hints->y = 0;
	Hints->flags |= USPosition;
	XSetWMNormalHints (xWindow->display, (Window) xWindow->full_wnd,
			   Hints);
	XFree (Hints);
	//==============================================

	/*
	 * Wnds attributes
	 */
	//==============================================	 
	XStoreName (xWindow->display, xWindow->os_handle, xWindow->title);

	xWindow->gc =
		XCreateGC (xWindow->display, xWindow->os_handle, 0, NULL);

	xWindow->videoaccesstype = VIDEO_XI_STANDARD;
#ifdef __USE_X_SHAREDMEMORY__

	Bool XShmPixmaps;
	if (XShmQueryVersion
	    (xWindow->display, &XShmMajor, &XShmMinor, &XShmPixmaps))
	{
		if (XShmPixmaps
		    && XShmPixmapFormat (xWindow->display) == ZPixmap)
		{
			xWindow->videoaccesstype = VIDEO_XI_SHMPIXMAP;
			ff ("VIDEO_XI_SHMPIXMAP\n");
		}
		else
		{
			xWindow->videoaccesstype = VIDEO_XI_SHMSTD;
			ff ("VIDEO_XI_SHMSTD\n");
		}
	}
#endif
	xWindow->screensize = xWindow->height * xWindow->width * xWindow->bpp;

	xWindow->back_buffer =
		(X11WrapSurface *) malloc (sizeof (X11WrapSurface));

	xWindow->back_buffer->id = -1;

	XSetWindowAttributes xsw;
	xsw.border_pixel = WhitePixel (xWindow->display, xWindow->screennum);
	xsw.background_pixel =
		BlackPixel (xWindow->display, xWindow->screennum);
	xsw.win_gravity = NorthWestGravity;
	XChangeWindowAttributes (xWindow->display, (Window) xWindow->normal_wnd,
				 CWBackPixel | CWBorderPixel | CWWinGravity,
				 &xsw);

	/*
	 * make full_wnd full screen
	 */
	//==============================================
	xsw.override_redirect = True;
	XChangeWindowAttributes (xWindow->display, (Window) xWindow->full_wnd,
				 CWOverrideRedirect | CWBackPixel |
				 CWBorderPixel | CWWinGravity, &xsw);
	//==============================================

	/*
	 * Wnds manager
	 */
	//==============================================
	xWindow->WM_DELETE_WINDOW =
		XInternAtom (xWindow->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols (xWindow->display, xWindow->os_handle,
			 &xWindow->WM_DELETE_WINDOW, 1);

	{
		XEvent ev;
		long mask;

		memset (&ev, 0, sizeof (ev));
		ev.xclient.type = ClientMessage;
		ev.xclient.window = RootWindowOfScreen (xWindow->screenptr);
		ev.xclient.message_type = XInternAtom (xWindow->display,
						       "KWM_KEEP_ON_TOP",
						       False);
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = (Window) xWindow->full_wnd;
		ev.xclient.data.l[1] = CurrentTime;
		mask = SubstructureRedirectMask;
		XSendEvent (xWindow->display,
			    RootWindowOfScreen (xWindow->screenptr), False,
			    mask, &ev);
	}
}

/*
 * setup system - if os_handle is NULL the driver shall create the output
 * display (common case) the other case is currently only used by child
 * windows on win32 and winCE @no_proc_override: when set and a os_handle
 * is passed, the plugin shall not try to override the window proc if cfg
 * is specified, the output is 3D, otherwise 2D
 */
GF_Err
X11_Setup(struct _video_out *vout, void *os_handle,
		   void *os_display, u32 no_proc_override, GF_GLConfig * cfg)
{
	GF_Err e;
	X11VID ();
	if (os_display)
	{
		xWindow->display = (Display *) os_display;
	}
	if (os_handle) xWindow->ext_wnd = os_handle;
	xWindow->is_3D_out = cfg ? 1 : 0;
	X11_SetupWindow (vout);

	e = X11_InitBackBuffer (vout, xWindow->width, xWindow->height);
	if (e) return e;
	XMapWindow (xWindow->display, (Window) xWindow->normal_wnd);
	gf_th_run(xWindow->th, X11_EventProc, vout);
	return GF_OK;
}


void *
X11_NewVideo ()
{
	GF_VideoOutput *driv = (GF_VideoOutput *) malloc (sizeof (GF_VideoOutput));

	if (!driv)
		return NULL;

	memset (driv, 0, sizeof (GF_VideoOutput));

	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "X11 Video Output",
		     "gpac distribution")
		XWindow *xWindow = (XWindow *) malloc (sizeof (XWindow));

	memset (xWindow, 0, sizeof (XWindow));

	xWindow->th = gf_th_new();
	xWindow->mx = gf_mx_new();
	xWindow->surfaces = gf_list_new();
	xWindow->width = 320;
	xWindow->height = 240;

	driv->opaque = xWindow;

	driv->Blit = X11_Blit;
	driv->Clear = X11_Clear;
	driv->CreateSurface = X11_CreateSurface;
	driv->DeleteSurface = X11_DeleteSurface;
	driv->FlushVideo = X11_FlushVideo;
	driv->GetPixelFormat = X11_GetPixelFormat;
	driv->LockSurface = X11_LockSurface;
	driv->IsSurfaceValid = X11_IsSurfaceValid;
	driv->SetFullScreen = X11_SetFullScreen;
	driv->Setup = X11_Setup;
	driv->Shutdown = X11_Shutdown;
	driv->UnlockSurface = X11_UnlockSurface;
	driv->ResizeSurface = X11_ResizeSurface;
	driv->ProcessEvent = X11_ProcessEvent;
	driv->bHas3DSupport = 1;
	
	return (void *) driv;

}

void
X11_DeleteVideo (GF_VideoOutput * vout)
{
	free (vout);
}

/*
 * interface query
 */
Bool
QueryInterface (u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE)
		return 1;
	return 0;
}


/*
 * interface create
 */
GF_BaseInterface *
LoadInterface (u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE)
		return (GF_BaseInterface *) X11_NewVideo ();
	return NULL;
}


/*
 * interface destroy
 */
void
ShutdownInterface (GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType)
	{
	case GF_VIDEO_OUTPUT_INTERFACE:
		X11_DeleteVideo ((GF_VideoOutput *)ifce);
		break;
	}
}
