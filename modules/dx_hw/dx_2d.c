/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / DirectX audio and video render module
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


#include "dx_hw.h"

#define DDCONTEXT	DDContext *dd = (DDContext *)dr->opaque;
#define DDBACK		DDSurface *pBack = (DDSurface *) gf_list_get(dd->surfaces, 0);



static GF_Err DD_Clear(GF_VideoOutput *dr, u32 color);

static Bool surface_valid(DDContext *dd, DDSurface *ds, Bool remove)
{
	s32 i = gf_list_find(dd->surfaces, ds);
	if (i<0) return 0;
	if (remove) gf_list_rem(dd->surfaces, (u32) i);
	return 1;
}


static GF_Err DD_SetBackBufferSize(GF_VideoOutput *dr, u32 width, u32 height)
{
	DDCONTEXT;

	if (dd->is_3D_out) return GF_BAD_PARAM;

	if (!dd->ddraw_init) 
		return InitDirectDraw(dr, width, height);
	else
		return CreateBackBuffer(dr, width, height);
}


static GF_Err DD_ClearBackBuffer(GF_VideoOutput *dr, u32 color)
{
	HRESULT hr;
	RECT rc;
    DDBLTFX ddbltfx;
	DDCONTEXT;

	// Erase the background
    ZeroMemory( &ddbltfx, sizeof(ddbltfx) );
    ddbltfx.dwSize = sizeof(ddbltfx);
	switch (dd->pixelFormat) {
	case GF_PIXEL_RGB_565:
	    ddbltfx.dwFillColor = GF_COL_TO_565(color);
		break;
	case GF_PIXEL_RGB_555:
	    ddbltfx.dwFillColor = GF_COL_TO_555(color);
		break;
	default:
	    ddbltfx.dwFillColor = color;
		break;
	}
	rc.left = rc.top = 0;
	rc.right = dd->width;
	rc.bottom = dd->height;
#ifdef USE_DX_3
	hr = IDirectDrawSurface_Blt(dd->pBack, &rc, NULL, NULL, DDBLT_COLORFILL, &ddbltfx );
#else
	hr = IDirectDrawSurface7_Blt(dd->pBack, &rc, NULL, NULL, DDBLT_COLORFILL, &ddbltfx );
#endif
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}

GF_Err CreateBackBuffer(GF_VideoOutput *dr, u32 Width, u32 Height)
{
	HRESULT hr;
#ifdef USE_DX_3
    DDSURFACEDESC ddsd;
#else
    DDSURFACEDESC2 ddsd;
#endif

	DDCONTEXT;

	if (dd->pBack) 	SAFE_DD_RELEASE(dd->pBack);

	/*create backbuffer*/
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;    
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = Width;
	ddsd.dwHeight = Height;

#ifdef USE_DX_3
    if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL ) ) )
        return GF_IO_ERR;
#else
    if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL ) ) )
        return GF_IO_ERR;
#endif

	/*store size*/
	if (!dd->fullscreen) {
		dd->width = Width;
		dd->height = Height;
	} else {
		dd->fs_store_width = Width;
		dd->fs_store_height = Height;
	}
	DD_Clear(dr, 0xFF000000);
	DD_ClearBackBuffer(dr, 0xFF000000);

	if (!dd->yuv_init) DD_InitYUV(dr);
	dr->bHasYUV = 1;
	return GF_OK;
}

GF_Err InitDirectDraw(GF_VideoOutput *dr, u32 Width, u32 Height)
{
	HRESULT hr;
	DWORD cooplev;
	LPDIRECTDRAW ddraw;
#ifdef USE_DX_3
    DDSURFACEDESC ddsd;
#else
    DDSURFACEDESC2 ddsd;
#endif
	DDPIXELFORMAT pixelFmt;
    LPDIRECTDRAWCLIPPER pcClipper;
	DDCONTEXT;
	
	if (!dd->cur_hwnd || !Width || !Height) return GF_BAD_PARAM;
	DestroyObjects(dd);

	if( FAILED( hr = DirectDrawCreate(NULL, &ddraw, NULL ) ) )
		return GF_IO_ERR;

#ifdef USE_DX_3
	hr = IDirectDraw_QueryInterface(ddraw, &IID_IDirectDraw, (LPVOID *)&dd->pDD);
#else
	hr = IDirectDraw_QueryInterface(ddraw, &IID_IDirectDraw7, (LPVOID *)&dd->pDD);
#endif		
	IDirectDraw_Release(ddraw);
	if (FAILED(hr)) return GF_IO_ERR;

	dd->switch_res = 0;
	cooplev = DDSCL_NORMAL;
	/*Setup FS*/
	if (dd->fullscreen) {

		/*change display mode*/
		if (dd->switch_res) {
#ifdef USE_DX_3
			hr = IDirectDraw_SetDisplayMode(dd->pDD, dd->fs_width, dd->fs_height, dd->video_bpp);
#else
			hr = IDirectDraw7_SetDisplayMode(dd->pDD, dd->fs_width, dd->fs_height, dd->video_bpp, 0, 0 );
#endif
			if( FAILED(hr)) return GF_IO_ERR;
		}
		dd->NeedRestore = 1;
		cooplev = DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN;
	}

	
#ifdef USE_DX_3
	hr = IDirectDraw_SetCooperativeLevel(dd->pDD, dd->cur_hwnd, cooplev);
#else
	hr = IDirectDraw7_SetCooperativeLevel(dd->pDD, dd->cur_hwnd, cooplev);
#endif
	if( FAILED(hr) ) return GF_IO_ERR;

	/*create primary*/
    ZeroMemory( &ddsd, sizeof( ddsd ) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

#ifdef USE_DX_3
    if( FAILED(IDirectDraw_CreateSurface(dd->pDD, &ddsd, &dd->pPrimary, NULL ) ) )
        return GF_IO_ERR;
#else
    if( FAILED(hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &dd->pPrimary, NULL ) ) ) 
		return GF_IO_ERR;
#endif

	/*get pixel format of video board*/
	memset (&pixelFmt, 0, sizeof(pixelFmt));
	pixelFmt.dwSize = sizeof(pixelFmt);
	hr = IDirectDrawSurface_GetPixelFormat(dd->pPrimary, &pixelFmt);
	if( FAILED(hr) ) return GF_IO_ERR;

	switch(pixelFmt.dwRGBBitCount) {
	case 16:
		if ((pixelFmt.dwRBitMask == 0xf800) && (pixelFmt.dwGBitMask == 0x07e0) && (pixelFmt.dwBBitMask == 0x001f))
			dd->pixelFormat = GF_PIXEL_RGB_565;
		else if ((pixelFmt.dwRBitMask == 0x7c00) && (pixelFmt.dwGBitMask == 0x03e0) && (pixelFmt.dwBBitMask == 0x001f))
			dd->pixelFormat = GF_PIXEL_RGB_555;
		else 
			return GF_NOT_SUPPORTED;
		dd->video_bpp = 16;
		break;
	case 24:
		if ((pixelFmt.dwRBitMask == 0x0000FF) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0xFF0000))
			dd->pixelFormat = GF_PIXEL_BGR_24;
		else if ((pixelFmt.dwRBitMask == 0xFF0000) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0x0000FF))
			dd->pixelFormat = GF_PIXEL_RGB_24;
		dd->video_bpp = 24;
		break;
	case 32:
		/*i always have color pbs in 32 bpp !!!*/
		if ((pixelFmt.dwRBitMask == 0x0000FF) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0xFF0000))
			dd->pixelFormat = GF_PIXEL_BGR_32;
		else if ((pixelFmt.dwRBitMask == 0xFF0000) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0x0000FF))
			dd->pixelFormat = GF_PIXEL_RGB_32;
		dd->video_bpp = 32;
		break;
	default:
		return GF_IO_ERR;
	}


#ifdef USE_DX_3
	if( FAILED( hr = IDirectDraw_CreateClipper(dd->pDD, 0, &pcClipper, NULL ) ) )
        return GF_IO_ERR;
#else
	if( FAILED( hr = IDirectDraw7_CreateClipper(dd->pDD, 0, &pcClipper, NULL ) ) )
        return GF_IO_ERR;
#endif
	
	if( FAILED( hr = IDirectDrawClipper_SetHWnd(pcClipper, 0, dd->cur_hwnd) ) ) {
        IDirectDrawClipper_Release(pcClipper);
        return GF_IO_ERR;
    }
    if( FAILED( hr = IDirectDrawSurface_SetClipper(dd->pPrimary, pcClipper ) ) ) {
        IDirectDrawClipper_Release(pcClipper);
        return GF_IO_ERR;
    }
	IDirectDrawClipper_Release(pcClipper);
	dd->ddraw_init = 1;
	return CreateBackBuffer(dr, Width, Height);
}

static GF_Err DD_LockSurface(GF_VideoOutput *dr, u32 surface_id, GF_VideoSurface *vi)
{
    HRESULT hr;
	DDSurface *ds;
#ifdef USE_DX_3
	LPDIRECTDRAWSURFACE surf;
	DDSURFACEDESC desc;
#else
	LPDIRECTDRAWSURFACE7 surf;
	DDSURFACEDESC2 desc;
#endif
	DDCONTEXT;
	
	if (!dd || !vi) return GF_BAD_PARAM;

	if (surface_id) {
		ds = (DDSurface *) surface_id;
		if (!surface_valid(dd, ds, 0)) return GF_BAD_PARAM;
		surf = ds->pSurface;
		vi->pixel_format = ds->format;
		vi->os_handle = NULL;
	} else {
		surf = dd->pBack;
		vi->pixel_format = dd->pixelFormat;
		vi->os_handle = dd->cur_hwnd;
	}

	if (!surf) return GF_BAD_PARAM;

#ifdef USE_DX_3
	ZeroMemory(&desc, sizeof(DDSURFACEDESC));
	desc.dwSize = sizeof(DDSURFACEDESC);
	if (FAILED(hr = IDirectDrawSurface_Lock(surf, NULL, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WRITEONLY | DDLOCK_WAIT, NULL))) {
		return GF_IO_ERR;
	}	
#else
	ZeroMemory(&desc, sizeof(DDSURFACEDESC2));
	desc.dwSize = sizeof(DDSURFACEDESC2);
	if (FAILED(hr = IDirectDrawSurface7_Lock(surf, NULL, &desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WRITEONLY | DDLOCK_WAIT, NULL))) {
		return GF_IO_ERR;
	}	
#endif

	vi->video_buffer = desc.lpSurface;
	vi->width = desc.dwWidth;
	vi->height = desc.dwHeight;
	vi->pitch = desc.lPitch;
	return GF_OK;
}

static GF_Err DD_UnlockSurface(GF_VideoOutput *dr, u32 surface_id)
{
    HRESULT hr;
	DDSurface *ds;
#ifdef USE_DX_3
	LPDIRECTDRAWSURFACE surf;
#else
	LPDIRECTDRAWSURFACE7 surf;
#endif
	DDCONTEXT;

	if (!dd || !dd->ddraw_init) return GF_IO_ERR;

	if (surface_id) {
		ds = (DDSurface *) surface_id;
		if (!surface_valid(dd, ds, 0)) return GF_BAD_PARAM;
		surf = ds->pSurface;
	} else {
		surf = dd->pBack;
	}
#ifdef USE_DX_3
	hr = IDirectDrawSurface_Unlock(surf, NULL);
#else
	hr = IDirectDrawSurface7_Unlock(surf, NULL);
#endif
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}

static GF_Err DD_Clear(GF_VideoOutput *dr, u32 color)
{
	HRESULT hr = S_OK;
	DDBLTFX ddbltfx;
	DDCONTEXT;

	if (!dd->pPrimary) return GF_OK;

	ZeroMemory( &ddbltfx, sizeof(ddbltfx) );
	ddbltfx.dwSize = sizeof(ddbltfx);
	switch (dd->pixelFormat) {
	case GF_PIXEL_RGB_565:
		ddbltfx.dwFillColor = GF_COL_TO_565(color);
		break;
	case GF_PIXEL_RGB_555:
		ddbltfx.dwFillColor = GF_COL_TO_555(color);
		break;
	default:
		ddbltfx.dwFillColor = color;
		break;
	}

#ifdef USE_DX_3
	hr = IDirectDrawSurface_Blt(dd->pPrimary, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx );
#else
	hr = IDirectDrawSurface7_Blt(dd->pPrimary, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx );
#endif
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}

static void *DD_GetContext(GF_VideoOutput *dr, u32 surface_id)
{
	DDSurface *ds;
#ifdef USE_DX_3
	LPDIRECTDRAWSURFACE surf;
#else
	LPDIRECTDRAWSURFACE7 surf;
#endif
	HDC hDC;
	DDCONTEXT;

	if (surface_id) {
		ds = (DDSurface *) surface_id;
		if (!surface_valid(dd, ds, 0)) return NULL;
		surf = ds->pSurface;
	} else {
		surf = dd->pBack;
	}
	if (! IDirectDrawSurface_IsLost(surf)) {
		if (FAILED(IDirectDrawSurface_GetDC(surf, &hDC)) ) return NULL;
		return hDC;
	} 
	return NULL;
}

static void DD_ReleaseContext(GF_VideoOutput *dr, u32 surface_id, void *context)
{
	DDSurface *ds;
#ifdef USE_DX_3
	LPDIRECTDRAWSURFACE surf;
#else
	LPDIRECTDRAWSURFACE7 surf;
#endif
	HDC hDC;
	DDCONTEXT;

	if (surface_id) {
		ds = (DDSurface *) surface_id;
		if (!surface_valid(dd, ds, 0)) return;
		surf = ds->pSurface;
	} else {
		surf = dd->pBack;
	}
	hDC = (HDC) context;
	IDirectDrawSurface_ReleaseDC(surf, hDC);
}


static GF_Err DD_GetPixelFormat(GF_VideoOutput *dr, u32 surfaceID, u32 *pixel_format)
{
	DDSurface *ds;
	DDCONTEXT;
	if (!surfaceID) {
		*pixel_format = dd->pixelFormat;
		return GF_OK;
	}
	ds = (DDSurface *) surfaceID;
	if (!surface_valid(dd, ds, 0)) return GF_BAD_PARAM;
	*pixel_format = ds->format;
	return GF_OK;

}

static GF_Err DD_Blit(GF_VideoOutput *dr, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst)
{
	HRESULT hr;
	u32 dst_w, dst_h, src_w, src_h;
	DDSurface *ds;
#ifdef USE_DX_3
	LPDIRECTDRAWSURFACE s_src, s_dst;
#else
	LPDIRECTDRAWSURFACE7 s_src, s_dst;
#endif
	RECT r_dst, r_src;
	DDCONTEXT;

	if (src_id==dst_id) return GF_BAD_PARAM;
	if (src_id) {
		ds = (DDSurface *) src_id;
		if (!surface_valid(dd, ds, 0)) return GF_BAD_PARAM;
		s_src = ds->pSurface;
		src_w = src ? src->w : ds->width;
		src_h = src ? src->h : ds->height;
	} else {
		src_w = src ? src->w : dd->width;
		src_h = src ? src->h : dd->height;
		s_src = dd->pBack;
	}
	if (dst_id) {
		ds = (DDSurface *) dst_id;
		if (!surface_valid(dd, ds, 0)) return GF_BAD_PARAM;
		s_dst = ds->pSurface;
		dst_w = dst ? dst->w : ds->width;
		dst_h = dst ? dst->h : ds->height;
	} else {
		dst_w = dst ? dst->w : dd->width;
		dst_h = dst ? dst->h : dd->height;
		s_dst = dd->pBack;
	}
	
	if (src != NULL) MAKERECT(r_src, src);
	if (dst != NULL) MAKERECT(r_dst, dst);


	if ((dst_w==src_w) && (dst_h==src_h)) {
		hr = IDirectDrawSurface_BltFast(s_dst, dst ? r_dst.left : 0, dst ? r_dst.top : 0, s_src, src ? &r_src : NULL, DDBLTFAST_WAIT);
	} else {
		hr = IDirectDrawSurface_Blt(s_dst, dst ? &r_dst : NULL, s_src, src ? &r_src : NULL, DDBLT_WAIT, NULL);
	}
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}


static Bool pixelformat_yuv(u32 pixel_format)
{
	switch (pixel_format) {
	case GF_PIXEL_YUY2:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_Y422:
	case GF_PIXEL_UYNV:
	case GF_PIXEL_YUNV:
	case GF_PIXEL_V422:
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		return 1;
	default:
		return 0;
	}
}

static u32 get_win_4CC(u32 pixel_format)
{
	switch (pixel_format) {
	case GF_PIXEL_YUY2:
		return mmioFOURCC('Y', 'U', 'Y', '2');
	case GF_PIXEL_YVYU:
		return mmioFOURCC('Y', 'V', 'Y', 'U');
	case GF_PIXEL_UYVY:
		return mmioFOURCC('U', 'Y', 'V', 'Y');
	case GF_PIXEL_VYUY:
		return mmioFOURCC('V', 'Y', 'U', 'Y');
	case GF_PIXEL_Y422:
		return mmioFOURCC('Y', '4', '2', '2');
	case GF_PIXEL_UYNV:
		return mmioFOURCC('U', 'Y', 'N', 'V');
	case GF_PIXEL_YUNV:
		return mmioFOURCC('Y', 'U', 'N', 'V');
	case GF_PIXEL_V422:
		return mmioFOURCC('V', '4', '2', '2');
	case GF_PIXEL_YV12:
		return mmioFOURCC('Y', 'V', '1', '2');
	case GF_PIXEL_IYUV:
		return mmioFOURCC('I', 'Y', 'U', 'V');
	case GF_PIXEL_I420:
		return mmioFOURCC('I', '4', '2', '0');
	default:
		return 0;
	}
}

static void DD_InitYUV(GF_VideoOutput *dr);

static GF_Err DD_CreateSurface(GF_VideoOutput *dr, u32 width, u32 height, u32 pixel_format, u32 *surfaceID)
{
	DDSurface *ds;
#ifdef USE_DX_3
	LPDIRECTDRAWSURFACE pSurf;
	DDSURFACEDESC ddsd;
#else
	LPDIRECTDRAWSURFACE7 pSurf;
	DDSURFACEDESC2 ddsd;
#endif
	HRESULT hr;
	DDCONTEXT;

	/*yuv format*/
	*surfaceID = 0xFFFFFFFF;
	if (pixelformat_yuv(pixel_format)) {
		if (dd->yuv_format) {
			memset (&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(ddsd);
			ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
			ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
			ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
			ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
			ddsd.dwWidth = width;
			ddsd.dwHeight = height;
			ddsd.ddpfPixelFormat.dwFourCC = get_win_4CC(dd->yuv_format);

			/*if we fail to create the YUV assume the driver cannot work in YUV*/
			dr->bHasYUV = 0;
#ifdef USE_DX_3
			if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &pSurf, NULL ) ) )
				return GF_IO_ERR;
#else
			if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &pSurf, NULL ) ) )
				return GF_IO_ERR;
#endif
			dr->bHasYUV = 1;
			ds = malloc(sizeof(DDSurface));
			ds->format = dd->yuv_format;
			ds->width = width;
			ds->height = height;
			ds->pitch = 0;
			ds->pSurface = pSurf;
			ds->id = (u32) ds;
			*surfaceID = ds->id;
			gf_list_add(dd->surfaces, ds);
			return GF_OK;
		}
	}

	/*rgb format - we have to use the main card format otherwise bliting to the main surface will fail...*/
	memset (&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = width;
	ddsd.dwHeight = height;

#ifdef USE_DX_3
	if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &pSurf, NULL ) ) )
		return GF_IO_ERR;
#else
	if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &pSurf, NULL ) ) )
		return GF_IO_ERR;
#endif

	ds = malloc(sizeof(DDSurface));
	ds->width = width;
	ds->height = height;
	ds->pitch = 0;
	ds->pSurface = pSurf;
	ds->id = (u32) ds;
	*surfaceID = ds->id;
	gf_list_add(dd->surfaces, ds);

	ds->format = dd->pixelFormat;
	
	return GF_OK;
}


/*deletes video surface by id*/
static GF_Err DD_DeleteSurface(GF_VideoOutput *dr, u32 surface_id)
{
	DDSurface *ds;
	DDCONTEXT;
	if (!surface_id) return GF_BAD_PARAM;
	ds = (DDSurface *) surface_id;
	if (!surface_valid(dd, ds, 1)) return GF_BAD_PARAM;
	SAFE_DD_RELEASE(ds->pSurface);
	free(ds);
	return GF_OK;
}

Bool DD_IsSurfaceValid(GF_VideoOutput *dr, u32 surface_id)
{
	DDSurface *ds;
	DDCONTEXT;
	/*main is always valid*/
	if (!surface_id) return 1;
	ds = (DDSurface *) surface_id;
	return surface_valid(dd, ds, 0);
}


static GF_Err DD_ResizeSurface(GF_VideoOutput *dr, u32 surface_id, u32 width, u32 height)
{
	DDSurface *ds;
#ifdef USE_DX_3
	DDSURFACEDESC ddsd;
#else
	DDSURFACEDESC2 ddsd;
#endif
	HRESULT hr;
	DDCONTEXT;

	if (!surface_id) DD_SetBackBufferSize(dr, width, height);

	ds = (DDSurface *) surface_id;
	if (!surface_valid(dd, ds, 0)) return GF_BAD_PARAM;
	if ( (ds->height>=height) && (ds->width>=width)) return GF_OK;
	width = MAX(ds->width, width);
	height = MAX(ds->height, height);

	SAFE_DD_RELEASE(ds->pSurface);

	/*yuv format*/
	if (pixelformat_yuv(ds->format)) {
		memset (&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
		ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
		ddsd.dwWidth = width;
		ddsd.dwHeight = height;
		ddsd.ddpfPixelFormat.dwFourCC = get_win_4CC(dd->yuv_format);

#ifdef USE_DX_3
		if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &ds->pSurface, NULL ) ) )
			return GF_IO_ERR;
#else
		if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &ds->pSurface, NULL ) ) )
			return GF_IO_ERR;
#endif

		ds->width = width;
		ds->height = height;
		return GF_OK;
	}


	/*rgb format*/
	memset (&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = width;
	ddsd.dwHeight = height;

#ifdef USE_DX_3
	if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &ds->pSurface , NULL ) ) )
		return GF_IO_ERR;
#else
	if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &ds->pSurface , NULL ) ) )
		return GF_IO_ERR;
#endif

	ds->width = width;
	ds->height = height;

	return GF_OK;
}


static GFINLINE u32 is_yuv_supported(u32 win_4cc)
{
	if (win_4cc==get_win_4CC(GF_PIXEL_YV12)) return GF_PIXEL_YV12;
	else if (win_4cc==get_win_4CC(GF_PIXEL_I420)) return GF_PIXEL_I420;
	else if (win_4cc==get_win_4CC(GF_PIXEL_IYUV)) return GF_PIXEL_IYUV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_UYVY)) return GF_PIXEL_UYVY;
	else if (win_4cc==get_win_4CC(GF_PIXEL_Y422)) return GF_PIXEL_Y422;
	else if (win_4cc==get_win_4CC(GF_PIXEL_UYNV)) return GF_PIXEL_UYNV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_YUY2)) return GF_PIXEL_YUY2;
	else if (win_4cc==get_win_4CC(GF_PIXEL_YUNV)) return GF_PIXEL_YUNV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_V422)) return GF_PIXEL_V422;
	else if (win_4cc==get_win_4CC(GF_PIXEL_YVYU)) return GF_PIXEL_YVYU;
	else return 0;
}

static GFINLINE Bool is_yuv_planar(u32 format)
{
	switch  (format) {
	case GF_PIXEL_YV12:
	case GF_PIXEL_I420:
	case GF_PIXEL_IYUV:
		return 1;
	default:
		return 0;
	}
}

#define YUV_NUM_TEST	20
/*gets fastest YUV format for YUV to RGB blit from YUV overlay (support is quite random on most cards)*/
static void DD_InitYUV(GF_VideoOutput *dr)
{
	u32 w, h, j, i, num_yuv, surfaceID;
	DWORD numCodes;
	DWORD formats[30];
	DWORD *codes;
	u32 now, min_packed = 0xFFFFFFFF, min_planar = 0xFFFFFFFF;
	u32 best_packed = 0, best_planar = 0;
	Bool checkPacked = TRUE;
	
	DDCONTEXT;

	w = 320;
	h = 240;

	dd->yuv_init = 1;

#ifdef USE_DX_3
	IDirectDraw_GetFourCCCodes(dd->pDD, &numCodes, NULL);
	codes = (DWORD *)malloc(numCodes*sizeof(DWORD));
	IDirectDraw_GetFourCCCodes(dd->pDD, &numCodes, codes);
#else
	IDirectDraw7_GetFourCCCodes(dd->pDD, &numCodes, NULL);
	codes = (DWORD *)malloc(numCodes*sizeof(DWORD));
	IDirectDraw7_GetFourCCCodes(dd->pDD, &numCodes, codes);
#endif
	
	num_yuv = 0;
	for (i=0; i<numCodes; i++) {
		formats[num_yuv] = is_yuv_supported(codes[i]);
		if (formats[num_yuv]) num_yuv++;
	}
	free(codes);
	/*too bad*/
	if (!num_yuv) return;

	gf_sys_clock_start();

	surfaceID = 0;
	for (i=0; i<num_yuv; i++) {
		/*check planar first*/
		if (!checkPacked && !is_yuv_planar(formats[i])) goto go_on;
		/*then check packed */
		if (checkPacked && is_yuv_planar(formats[i])) goto go_on;

		if (surfaceID) DD_DeleteSurface(dr, surfaceID);
		surfaceID = 0;

		dd->yuv_format = formats[i];
		if (DD_CreateSurface(dr, w, h, dd->yuv_format, &surfaceID) != GF_OK)
			goto rem_fmt;

		now = gf_sys_clock();
		/*perform blank blit*/
		for (j=0; j<YUV_NUM_TEST; j++) {
			if (DD_Blit(dr, surfaceID, 0, NULL, NULL) != GF_OK)
				goto rem_fmt;
		}
		now = gf_sys_clock() - now;

		if (!checkPacked) {
			if (now<min_planar) {
				min_planar = now;
				best_planar = dd->yuv_format;
			}
		} else {
			if (now<min_packed) {
				min_packed = now;
				best_packed = dd->yuv_format;
			}
		}

go_on:
		if (checkPacked && (i+1==num_yuv)) {
			i = -1;
			checkPacked = FALSE;
		}
		continue;

rem_fmt:
		for (j=i; j<num_yuv-1; j++) {
			formats[j] = formats[j+1];
		}
		i--;
		num_yuv--;
	}
	gf_sys_clock_stop();

	if (surfaceID) DD_DeleteSurface(dr, surfaceID);


	if (best_planar && (min_planar < min_packed )) {
		dd->yuv_format = best_planar;
	} else {
		dd->yuv_format = best_packed;
	}
}


void DD_SetupDDraw(GF_VideoOutput *driv)
{
	/*alpha and keying to do*/
	driv->bHasAlpha = 0;
	driv->bHasKeying = 0;
	driv->bHasYUV = 1;

	driv->Blit = DD_Blit;
	driv->Clear = DD_Clear;
	driv->CreateSurface = DD_CreateSurface;
	driv->DeleteSurface = DD_DeleteSurface;
	driv->GetContext = DD_GetContext;
	driv->GetPixelFormat = DD_GetPixelFormat;
	driv->LockSurface = DD_LockSurface;
	driv->ReleaseContext = DD_ReleaseContext;
	driv->IsSurfaceValid = DD_IsSurfaceValid;
	driv->UnlockSurface = DD_UnlockSurface;
	driv->ResizeSurface	= DD_ResizeSurface;

	/*to do*/
	/*
	driv->BlitKey = DD_BltKey;
	driv->BlitAlpha = DD_BlitAlpha;
	*/
}

