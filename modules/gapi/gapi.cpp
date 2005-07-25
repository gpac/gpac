/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / GAPI WinCE-iPaq video render module
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


#include <windows.h>
#include <aygshell.h>
#include <gx.h>

#include "gapi.h"
#include "ColorTools.h"

#define GAPICTX(dr)	GAPIPriv *gctx = (GAPIPriv *) dr->opaque;


static GF_VideoOutput *the_video_driver = NULL;

static void GAPI_GetCoordinates(DWORD lParam, GF_Event *evt)
{
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
	evt->mouse.x = LOWORD(lParam);
	evt->mouse.y = HIWORD(lParam);

	if (ctx->fullscreen) {
		POINT pt;
		pt.x = evt->mouse.x;
		pt.y = evt->mouse.y;
		ClientToScreen(ctx->hWnd, &pt);
		if (ctx->gx.ffFormat & kfLandscape) {
			evt->mouse.x = ctx->fs_w - pt.y;
			evt->mouse.y = pt.x;
		} else {
			evt->mouse.x = pt.x;
			evt->mouse.y = pt.y;
		}
	}
}

static u32 GAPI_TranslateActionKey(u32 VirtKey) 
{
	switch (VirtKey) {
	case VK_HOME: return GF_VK_HOME;
	case VK_END: return GF_VK_END;
	case VK_NEXT: return GF_VK_PRIOR;
	case VK_PRIOR: return GF_VK_NEXT;
	case VK_UP: return GF_VK_UP;
	case VK_DOWN: return GF_VK_DOWN;
	case VK_LEFT: return GF_VK_LEFT;
	case VK_RIGHT: return GF_VK_RIGHT;
	case VK_F1: return GF_VK_F1;
	case VK_F2: return GF_VK_F2;
	case VK_F3: return GF_VK_F3;
	case VK_F4: return GF_VK_F4;
	case VK_F5: return GF_VK_F5;
	case VK_F6: return GF_VK_F6;
	case VK_F7: return GF_VK_F7;
	case VK_F8: return GF_VK_F8;
	case VK_F9: return GF_VK_F9;
	case VK_F10: return GF_VK_F10;
	case VK_F11: return GF_VK_F11;
	case VK_F12: return GF_VK_F12;
	case VK_RETURN: return GF_VK_RETURN;
	case VK_ESCAPE: return GF_VK_ESCAPE;
	case VK_SHIFT: return GF_VK_SHIFT;
	case VK_CONTROL: return GF_VK_CONTROL;
	case VK_MENU: 
		return GF_VK_MENU;
	default: return 0;
	}
}

u32 GX_TRANSLATE_KEY(u32 vk)
{
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
	short res = (LOWORD(vk) != 0x5b) ? LOWORD(vk) : vk;
	if (res==ctx->keys.vkLeft) 
		return GF_VK_LEFT;
	else if (res==ctx->keys.vkRight)
		return GF_VK_RIGHT;
	else if (res==ctx->keys.vkDown) 
		return GF_VK_DOWN;
	else if (res==ctx->keys.vkUp) 
		return GF_VK_UP;
	else if (res==ctx->keys.vkStart) 
		return GF_VK_RETURN;
	else if (res==ctx->keys.vkA) 
		return GF_VK_CONTROL;
	else if (res==ctx->keys.vkB) 
		return GF_VK_SHIFT;
	else if (res==ctx->keys.vkC)
		return GF_VK_MENU;
	else
		return 0;
}

LRESULT APIENTRY GAPI_WindowProc(HWND hWnd, UINT msg, UINT wParam, LONG lParam)
{
	GF_Event evt;
	switch (msg) {
	case WM_SIZE:
	{
		GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
		if (!ctx->is_resizing) {
			ctx->is_resizing = 1;
			evt.type = GF_EVT_SIZE;
			evt.size.width = LOWORD(lParam);
			evt.size.height = HIWORD(lParam);
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
			ctx->is_resizing = 0;
		}
	}
		break;
	case WM_CLOSE:
	{
		evt.type = GF_EVT_QUIT;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
	}
		return 1;
	case WM_DESTROY:
		PostQuitMessage (0);
		break;

	case WM_ERASEBKGND:
	case WM_PAINT:
	{
		GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
		evt.type = GF_EVT_REFRESH;
		if (ctx->backbuffer) the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
	}
		break;

	case WM_MOUSEMOVE:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVT_MOUSEMOVE;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVT_LEFTDOWN;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONUP:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVT_LEFTUP;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;

	/*FIXME - there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		evt.key.vk_code = GAPI_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code) {
			evt.type = (msg==WM_SYSKEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code<=GF_VK_RIGHT) evt.key.virtual_code = 0;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		/*emulate button actions as vk codes*/
		if ((evt.key.vk_code = GX_TRANSLATE_KEY(wParam)) != 0) {
			evt.key.virtual_code = wParam;
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
			break;
		}

		evt.key.vk_code = GAPI_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code) {
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code<=GF_VK_RIGHT) evt.key.virtual_code = 0;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
			/*also send a normal key for non-key-sensors*/
			if (evt.key.vk_code>GF_VK_RIGHT) goto send_key;
		} else {
send_key:
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_KEYDOWN : GF_EVT_KEYUP;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		}
	}
		break;
	case WM_CHAR:
		evt.type = GF_EVT_CHAR;
		evt.character.unicode_char = wParam;
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

void GAPI_WindowThread(void *par)
{
	MSG msg;
	WNDCLASS wc;
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;

	memset(&wc, 0, sizeof(WNDCLASS));
	wc.hInstance = GetModuleHandle(_T("gapi.dll"));
	wc.lpfnWndProc = GAPI_WindowProc;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
	wc.lpszClassName = _T("GPAC DirectDraw Output");
	RegisterClass (&wc);
	
	ctx->hWnd = CreateWindow(_T("GPAC DirectDraw Output"), NULL, WS_POPUP, 0, 0, 120, 100, NULL, NULL, wc.hInstance, NULL);
	if (ctx->hWnd == NULL) {
		ctx->ThreadID = 0;
		ExitThread(1);
	}
	ShowWindow(ctx->hWnd, SW_SHOWNORMAL);

	while (GetMessage (&(msg), NULL, 0, 0)) {
		TranslateMessage (&(msg));
		DispatchMessage (&(msg));
	}
	ctx->ThreadID = 0;
	ExitThread (0);
}


void GAPI_SetupWindow(GF_VideoOutput *dr)
{
	GAPIPriv *ctx = (GAPIPriv *)dr->opaque;
	if (the_video_driver) return;
	the_video_driver = dr;

	if (!ctx->hWnd) {
		ctx->hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) GAPI_WindowThread, (LPVOID) dr, 0, &(ctx->ThreadID) );
		while (!ctx->hWnd && ctx->hThread) gf_sleep(10);
		if (!ctx->hThread) return;
		ctx->owns_hwnd = 1;
	} else {
		/*override window proc*/
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, (DWORD) GAPI_WindowProc);
	}
}

void GAPI_ShutdownWindow(GF_VideoOutput *dr)
{
	GAPIPriv *ctx = (GAPIPriv *)dr->opaque;

	if (ctx->owns_hwnd) {
		PostMessage(ctx->hWnd, WM_DESTROY, 0, 0);
		while (ctx->ThreadID) gf_sleep(10);
		UnregisterClass(_T("GPAC DirectDraw Output"), GetModuleHandle(_T("dx_hw.dll")));
		CloseHandle(ctx->hThread);
		ctx->hThread = NULL;
	}
	ctx->hWnd = NULL;
	the_video_driver = NULL;
}


GF_Err GAPI_Clear(GF_VideoOutput *dr, u32 color)
{
	s32 i, j;
	u32 r, g, b;
	RECT rc;
	char *ptr;
	u16 col;
	GAPICTX(dr);

	if (!gctx || !gctx->is_init) return GF_IO_ERR;
	if (!gctx->fullscreen) return GF_OK;

	gf_mx_p(gctx->mx);
	col = 0;
	switch (gctx->pixel_format) {
	case GF_PIXEL_RGB_555:
		col = GF_COL_TO_555(color);
		break;
	case GF_PIXEL_RGB_565:
		col = GF_COL_TO_565(color);
		break;
	}

	rc.top = rc.left = 0;
	rc.right = gctx->gx.cxWidth;
	rc.bottom = gctx->gx.cyHeight;

	ptr = (char *) GXBeginDraw();
	if (!ptr) {
		gf_mx_v(gctx->mx);
		return GF_IO_ERR;
	}
	r = (col>>16);
	g = (col>>8);
	b = (col)&0xFF;

	if (gctx->BPP==3) {
		for (i=rc.top; i<rc.bottom; i++) {
			char *_ptr =  ptr + rc.left * gctx->gx.cbxPitch + i*gctx->gx.cbyPitch;
			for (j=rc.left; j<rc.right; j++) {
				_ptr[0] = r;
				_ptr[1] = g;
				_ptr[2] = b;
				_ptr += gctx->gx.cbxPitch;
			}
		}
	} else {
		for (i=rc.top; i<rc.bottom; i++) {
			char *_ptr =  ptr + rc.left * gctx->gx.cbxPitch + i*gctx->gx.cbyPitch;
			for (j=rc.left; j<rc.right; j++) {
				* ((unsigned short *)_ptr) = col;
				_ptr += gctx->gx.cbxPitch;
			}
		}
	}
	GXEndDraw();

	gf_mx_v(gctx->mx);
	return GF_OK;
}


static GF_Err GAPI_InitSurface(GF_VideoOutput *dr, u32 VideoWidth, u32 VideoHeight)
{
	GAPICTX(dr);

	if (!gctx || !VideoWidth || !VideoHeight) return GF_BAD_PARAM;

	gf_mx_p(gctx->mx);

	gctx->gx = GXGetDisplayProperties();

	if (gctx->gx.ffFormat & kfDirect555) {
		gctx->pixel_format = GF_PIXEL_RGB_555;
		gctx->BPP = 2;
		gctx->bitsPP = 15;
	} 
	else if (gctx->gx.ffFormat & kfDirect565) {
		gctx->pixel_format = GF_PIXEL_RGB_565;
		gctx->BPP = 2;
		gctx->bitsPP = 16;
	}
	else if (gctx->gx.ffFormat & kfDirect888) {
		gctx->pixel_format = GF_PIXEL_RGB_24;
		gctx->BPP = 3;
		gctx->bitsPP = 24;
	} else {
		gf_mx_v(gctx->mx);
		return GF_NOT_SUPPORTED;
	}
	gctx->screen_w = gctx->gx.cxWidth;
	gctx->screen_h = gctx->gx.cyHeight;

	if (gctx->backbuffer) free(gctx->backbuffer);
	gctx->bb_size = VideoWidth * VideoHeight * gctx->BPP;
	gctx->backbuffer = (unsigned char *) malloc(sizeof(unsigned char) * gctx->bb_size);
	gctx->bb_width = VideoWidth;
	gctx->bb_height = VideoHeight;
	gctx->bb_pitch = VideoWidth * gctx->BPP;
	gctx->is_init = 1;

	GAPI_Clear(dr, 0);
	gf_mx_v(gctx->mx);
	return GF_OK;
}

GF_Err GAPI_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, Bool noover, GF_GLConfig *cfg)
{
	RECT rc;
	GAPICTX(dr);
	gctx->hWnd = (HWND) os_handle;
	
	if (cfg) return GF_NOT_SUPPORTED;
	GAPI_SetupWindow(dr);
	if (!gctx->hWnd || !GXOpenDisplay(gctx->hWnd, 0L)) {
		MessageBox(NULL, _T("Cannot open display"), _T("GAPI Error"), MB_OK);
		return GF_IO_ERR;
	}

	gctx->gapi_open = 1;
    GetClientRect(gctx->hWnd, &rc);
	gctx->disp_w = rc.right - rc.left;
	gctx->disp_h = rc.bottom - rc.top;
	gctx->keys = GXGetDefaultKeys(GX_NORMALKEYS);
	return GAPI_InitSurface(dr, gctx->disp_w, gctx->disp_h);
}

static void GAPI_Shutdown(GF_VideoOutput *dr)
{
	GAPICTX(dr);

	gf_mx_p(gctx->mx);

	if (gctx->backbuffer) free(gctx->backbuffer);
	gctx->backbuffer = NULL;

	while (gf_list_count(gctx->surfaces)) {
		GAPISurface *ptr = (GAPISurface *) gf_list_get(gctx->surfaces, 0);
		gf_list_rem(gctx->surfaces, 0);
		free(ptr->buffer);
		free(ptr);
	}

	if (gctx->gapi_open) {
		GXCloseDisplay();
		gctx->gapi_open = 0;
	}

	GAPI_ShutdownWindow(dr);

	gf_mx_v(gctx->mx);
}

static GF_Err GAPI_SetFullScreen(GF_VideoOutput *dr, Bool bOn, u32 *outWidth, u32 *outHeight)
{
	GF_Err e;
	GAPICTX(dr);

	if (!gctx || !gctx->is_init) return GF_BAD_PARAM;
	if (bOn == gctx->fullscreen) return GF_OK;

	GAPI_Clear(dr, 0);

	gf_mx_p(gctx->mx);
	GXCloseDisplay();
	e = GF_OK;
	if (bOn) {
		if (!GXOpenDisplay(GetParent(gctx->hWnd), GX_FULLSCREEN)) {
			GXOpenDisplay(gctx->hWnd, 0L);
			gctx->fullscreen = 0;
			e = GF_IO_ERR;
		} else {
			gctx->fullscreen = 1;
		}
	} else {
		GXOpenDisplay(gctx->hWnd, 0L);
		gctx->fullscreen = 0;
	}

	if (!e) {
		if (gctx->fullscreen) {
			gctx->disp_w = gctx->bb_width;
			gctx->disp_h = gctx->bb_height;

			if (gctx->gx.ffFormat & kfLandscape) {
				gctx->fs_w = gctx->screen_h;
				gctx->fs_h = gctx->screen_w;
			} else {
				gctx->fs_w = gctx->screen_w;
				gctx->fs_h = gctx->screen_h;
			}
			*outWidth = gctx->fs_w;
			*outHeight = gctx->fs_h;
		} else {
			*outWidth = gctx->disp_w;
			*outHeight = gctx->disp_h;
		}
		e = GAPI_InitSurface(dr, *outWidth, *outHeight); 
	}
	GAPI_Clear(dr, 0);
	gf_mx_v(gctx->mx);

	return e;
}



static GF_Err GAPI_FlushVideo(GF_VideoOutput *dr, GF_Window *dest)
{
	unsigned char *ptr;
	GAPICTX(dr);

	if (!gctx || !gctx->is_init) return GF_BAD_PARAM;
	if (dest && (!dest->w || !dest->h)) return GF_OK;
	if (gctx->is_resizing) return GF_OK;

	gf_mx_p(gctx->mx);

	/*get a pointer to video memory*/
	ptr = (unsigned char *) GXBeginDraw();
	if (!ptr) {
		gf_mx_v(gctx->mx);
		return GF_IO_ERR;
	}
	
	if (gctx->fullscreen) {
		gctx->dst_blt.x = dest->x;
		gctx->dst_blt.y = dest->y;
		gctx->dst_blt.w = dest->w;
		gctx->dst_blt.h = dest->h;

		if (gctx->gx.ffFormat & kfLandscape) {
			ptr += gctx->gx.cbxPitch * dest->y - dest->x * gctx->gx.cbyPitch;

			StretchBits(ptr, gctx->bitsPP, gctx->dst_blt.w, gctx->dst_blt.h, - gctx->gx.cbyPitch, gctx->gx.cbxPitch,
					/*src*/
					gctx->backbuffer, gctx->bitsPP, gctx->bb_width, gctx->bb_height, gctx->bb_pitch);
		} else {
			ptr += gctx->gx.cbxPitch * dest->x + dest->y * gctx->gx.cbyPitch;

			StretchBits(ptr, gctx->bitsPP, gctx->dst_blt.w, gctx->dst_blt.h, gctx->gx.cbxPitch, gctx->gx.cbyPitch,
					/*src*/
					gctx->backbuffer, gctx->bitsPP, gctx->bb_width, gctx->bb_height, gctx->bb_pitch);
		}
	} else {
		RECT rc;
		GetWindowRect(gctx->hWnd, &rc);
		gctx->dst_blt.x = rc.left;
		gctx->dst_blt.y = rc.top;
		gctx->dst_blt.w = rc.right - rc.left;
		gctx->dst_blt.h = rc.bottom - rc.top;

		gctx->dst_blt.x += dest->x;
		gctx->dst_blt.y += dest->y;
		gctx->dst_blt.w = dest->w;
		gctx->dst_blt.h = dest->h;

		ptr += gctx->gx.cbxPitch * gctx->dst_blt.x + gctx->dst_blt.y * gctx->gx.cbyPitch;

		StretchBits(ptr, 
			gctx->bitsPP, gctx->dst_blt.w, gctx->dst_blt.h, gctx->gx.cbxPitch, gctx->gx.cbyPitch, 
			/*src & src bpp*/
			gctx->backbuffer, gctx->bitsPP, gctx->bb_width, gctx->bb_height, gctx->bb_pitch);
	}

	GXEndDraw();
	gf_mx_v(gctx->mx);
	return GF_OK;
}

static GF_Err GAPI_LockSurface(GF_VideoOutput *dr, u32 surface_id, GF_VideoSurface *vi)
{
	u32 i;
	GAPICTX(dr);

	if (!surface_id) {
		vi->width = gctx->bb_width;
		vi->height = gctx->bb_height;
		vi->pitch = gctx->bb_pitch;
		vi->pixel_format = gctx->pixel_format;
		vi->os_handle = NULL;
		vi->video_buffer = gctx->backbuffer;
		return GF_OK;
	}
	/*check surfaces*/
	for (i=0; i<gf_list_count(gctx->surfaces); i++) {
		GAPISurface *ptr = (GAPISurface *) gf_list_get(gctx->surfaces, i);
		if (ptr->ID == surface_id) {
			vi->width = ptr->width;
			vi->height = ptr->height;
			vi->pitch = ptr->pitch;
			vi->pixel_format = ptr->pixel_format;
			vi->os_handle = NULL;
			vi->video_buffer = ptr->buffer;
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

static GF_Err GAPI_UnlockSurface(GF_VideoOutput *dr, u32 surface_id)
{
	/*no special things to do on unlock*/
	return GF_OK;
}

static GF_Err GAPI_GetPixelFormat(GF_VideoOutput *dr, u32 surfaceID, u32 *pixel_format)
{
	u32 i;
	GAPICTX(dr);

	if (!surfaceID) {
		*pixel_format = gctx->pixel_format;
		return GF_OK;
	}
	/*check surfaces*/
	for (i=0; i<gf_list_count(gctx->surfaces); i++) {
		GAPISurface *ptr = (GAPISurface *) gf_list_get(gctx->surfaces, i);
		if (ptr->ID == surfaceID) {
			*pixel_format = ptr->pixel_format;
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

static GF_Err GAPI_Blit(GF_VideoOutput *dr, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst)
{
	GF_VideoSurface vs;
	GF_Err e;
	void *pdst, *psrc;
	
	GAPICTX(dr);

	/*NOT SUPPORTED*/
	if (dst_id) return GF_BAD_PARAM;

	e = GAPI_LockSurface(dr, src_id, &vs);
	if (e) return e;

	switch (vs.pixel_format) {
	case GF_PIXEL_RGB_555:
		pdst = gctx->backbuffer + dst->y * gctx->bb_pitch + dst->x * gctx->BPP;
		psrc = vs.video_buffer + src->y * vs.pitch + src->x * 2;
		StretchBits(pdst, gctx->bitsPP, dst->w, dst->h, gctx->BPP, gctx->bb_pitch, 
					psrc, 15, src->w, src->h, vs.pitch);
		break;
	case GF_PIXEL_RGB_565:
		pdst = gctx->backbuffer + dst->y * gctx->bb_pitch + dst->x * gctx->BPP;
		psrc = vs.video_buffer + src->y * vs.pitch + src->x * 2;
		StretchBits(pdst, gctx->bitsPP, dst->w, dst->h, gctx->BPP, gctx->bb_pitch, 
					psrc, 16, src->w, src->h, vs.pitch);
		break;
	case GF_PIXEL_RGB_24:
		pdst = gctx->backbuffer + dst->y * gctx->bb_pitch + dst->x * gctx->BPP;
		psrc = vs.video_buffer + src->y * vs.pitch + src->x * 3;
		StretchBits(pdst, gctx->bitsPP, dst->w, dst->h, gctx->BPP, gctx->bb_pitch, 
					psrc, 24, src->w, src->h, vs.pitch);
		break;
	case GF_PIXEL_RGB_32:
		pdst = gctx->backbuffer + dst->y * gctx->bb_pitch + dst->x * gctx->BPP;
		psrc = vs.video_buffer + src->y * vs.pitch + src->x * 4;
		StretchBits(pdst, gctx->bitsPP, dst->w, dst->h, gctx->BPP, gctx->bb_pitch, 
					psrc, 32, src->w, src->h, vs.pitch);
		break;
	}
	return GF_OK;
}

static GF_Err GAPI_CreateSurface(GF_VideoOutput *dr, u32 width, u32 height, u32 pixel_format, u32 *surfaceID)
{
	u32 size;
	GAPISurface *surf;
	GAPICTX(dr);

	switch (pixel_format) {
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGB_32:
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	
	surf = (GAPISurface *) malloc(sizeof(GAPISurface));
	surf->width = width;
	surf->height = height;
	surf->ID = (u32) surf;
	surf->pixel_format = pixel_format;

	size = width * height;
	switch (pixel_format) {
	case GF_PIXEL_RGB_555:
		size *= 2;
		surf->pitch = width * 2;
		break;
	case GF_PIXEL_RGB_565:
		size *= 2;
		surf->pitch = width * 2;
		break;
	case GF_PIXEL_RGB_24:
		size *= 3;
		surf->pitch = width * 3;
		break;
	case GF_PIXEL_RGB_32:
		size *= 4;
		surf->pitch = width * 4;
		break;
	}
	surf->buffer = (unsigned char *)malloc(sizeof(unsigned char) * size);
	gf_list_add(gctx->surfaces, surf);

	*surfaceID = surf->ID;

	return GF_OK;
}


/*deletes video surface by id*/
static GF_Err GAPI_DeleteSurface(GF_VideoOutput *dr, u32 surfaceID)
{
	u32 i;
	GAPICTX(dr);
	if (!surfaceID) return GF_BAD_PARAM;

	/*check surfaces*/
	for (i=0; i<gf_list_count(gctx->surfaces); i++) {
		GAPISurface *ptr = (GAPISurface *) gf_list_get(gctx->surfaces, i);
		if (ptr->ID == surfaceID) {
			if (ptr->buffer) free(ptr->buffer);
			free(ptr);
			gf_list_rem(gctx->surfaces, i);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}


Bool GAPI_IsSurfaceValid(GF_VideoOutput *dr, u32 surface_id)
{
	u32 i;
	GAPICTX(dr);
	if (!surface_id) {
		if (!gctx->is_init) return 0;
		return 1;
	}
	/*check surfaces*/
	for (i=0; i<gf_list_count(gctx->surfaces); i++) {
		GAPISurface *ptr = (GAPISurface *) gf_list_get(gctx->surfaces, i);
		if (ptr->ID == surface_id) return 1;
	}
	return 0;
}

static GF_Err GAPI_ResizeSurface(GF_VideoOutput *dr, u32 surface_id, u32 width, u32 height)
{
	u32 i;
	GAPICTX(dr);
	if (!surface_id) return GAPI_InitSurface(dr, width, height); 

	/*check surfaces*/
	for (i=0; i<gf_list_count(gctx->surfaces); i++) {
		GAPISurface *ptr = (GAPISurface *) gf_list_get(gctx->surfaces, i);
		if (ptr->ID == surface_id) {
			if ( (ptr->height>=height) && (ptr->width>=width)) return GF_OK;
			if (ptr->buffer) free(ptr->buffer);
			ptr->pitch = ptr->pitch * width / ptr->width;
			ptr->width = width;
			ptr->height = height;
			ptr->buffer = (unsigned char *) malloc(sizeof(unsigned char) * ptr->height * ptr->pitch);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

static GF_Err GAPI_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	GAPICTX(dr);
	switch (evt->type) {
	case GF_EVT_SHOWHIDE:
		if (gctx->hWnd) ShowWindow(gctx->hWnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
		break;
	case GF_EVT_SIZE:
		gctx->is_resizing = 1;
		//SetWindowPos(gctx->hWnd, NULL, 0, 0, evt->size.width, evt->size.height, SWP_NOZORDER | SWP_NOMOVE);
		gctx->is_resizing = 0;
	/*we should actually never see a windowsize event*/
	case GF_EVT_VIDEO_SETUP:
		gctx->disp_w = evt->size.width;
		gctx->disp_h = evt->size.height;
		break;
	}
	return GF_OK;
}


static void *NewGAPIVideoOutput()
{
	GAPIPriv *priv;
	GF_VideoOutput *driv = (GF_VideoOutput *) malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "GAPI Video Output", "gpac distribution")

	priv = (GAPIPriv *) malloc(sizeof(GAPIPriv));
	memset(priv, 0, sizeof(GAPIPriv));
	priv->mx = gf_mx_new();
	priv->surfaces = gf_list_new();
	driv->opaque = priv;

	/*alpha and keying to do*/
	driv->bHasAlpha = 0;
	driv->bHasKeying = 0;
	driv->bHasYUV = 0;

	driv->Blit = GAPI_Blit;
	driv->Clear = GAPI_Clear;
	driv->CreateSurface = GAPI_CreateSurface;
	driv->DeleteSurface = GAPI_DeleteSurface;
	driv->FlushVideo = GAPI_FlushVideo;
	driv->GetPixelFormat = GAPI_GetPixelFormat;
	driv->LockSurface = GAPI_LockSurface;
	driv->IsSurfaceValid = GAPI_IsSurfaceValid;
	driv->SetFullScreen = GAPI_SetFullScreen;
	driv->Setup = GAPI_Setup;
	driv->Shutdown = GAPI_Shutdown;
	driv->UnlockSurface = GAPI_UnlockSurface;
	driv->ResizeSurface	= GAPI_ResizeSurface;
	driv->ProcessEvent = GAPI_ProcessEvent;
	return (void *)driv;
}

static void DeleteVideoOutput(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	GAPICTX(driv);
	GAPI_Shutdown(driv);
	gf_mx_del(gctx->mx);
	gf_list_del(gctx->surfaces);
	free(gctx);
	free(driv);
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 1;
	return 0;
}
/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewGAPIVideoOutput();
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VideoOutput *dd = (GF_VideoOutput *)ifce;
	switch (dd->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput(dd);
		break;
	}
}

