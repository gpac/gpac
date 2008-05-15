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

GF_Err CreateBackBuffer(GF_VideoOutput *dr, u32 Width, u32 Height, Bool use_system_memory)
{
	Bool force_reinit;
	HRESULT hr;
	const char *opt; 
#ifdef USE_DX_3
    DDSURFACEDESC ddsd;
#else
    DDSURFACEDESC2 ddsd;
#endif

	DDCONTEXT;


	force_reinit = 0;
	if (use_system_memory && !dd->systems_memory) force_reinit = 1;
	else if (!use_system_memory && dd->systems_memory) force_reinit = 1;
	if (dd->pBack && !force_reinit&& !dd->fullscreen && (dd->width == Width) && (dd->height == Height) ) {
		return GF_OK;
	}

	if (dd->pBack) 	SAFE_DD_RELEASE(dd->pBack);

	/*create backbuffer*/
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;    
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	if (dd->systems_memory==2) {
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	} else {
		opt = NULL;
		if (use_system_memory) {
			opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "UseHardwareMemory");
			if (opt && !strcmp(opt, "yes")) use_system_memory = 0;
		}
		if (!use_system_memory) {
			dd->systems_memory = 0;
			ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		} else {
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
			dd->systems_memory = 1;
		}
	}

	ddsd.dwWidth = Width;
	ddsd.dwHeight = Height;

#ifdef USE_DX_3
    hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL );
#else
    hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL );
#endif
	if (FAILED(hr)) {
		if (!dd->systems_memory) {
			ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
			dd->systems_memory = 1;
			if (opt && !strcmp(opt, "yes")) {
				gf_modules_set_option((GF_BaseInterface *)dr, "Video", "UseHardwareMemory", "no");

				GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] Hardware Video not available for backbuffer)\n"));
			}
#ifdef USE_DX_3
		    hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL );
#else
		    hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL );
#endif
		}
		if (FAILED(hr)) return GF_IO_ERR;
	}

	/*store size*/
	if (!dd->fullscreen) {
		dd->width = Width;
		dd->height = Height;
	} else {
		dd->fs_store_width = Width;
		dd->fs_store_height = Height;
	}
	DD_ClearBackBuffer(dr, 0xFF000000);

	if (!dd->yuv_init) DD_InitYUV(dr);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] BackBuffer %d x %d created on %s memory\n", Width, Height, dd->systems_memory ? "System" : "Video"));
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
	/*if YUV not initialize, init using HW video memory to setup HW caps*/
	return CreateBackBuffer(dr, Width, Height, dd->yuv_init);
}

static GF_Err DD_LockSurface(DDContext *dd, GF_VideoSurface *vi, void *surface)
{
    HRESULT hr;
#ifdef USE_DX_3
	DDSURFACEDESC desc;
#else
	DDSURFACEDESC2 desc;
#endif
	
	if (!dd || !vi || !surface) return GF_BAD_PARAM;

#ifdef USE_DX_3
	ZeroMemory(&desc, sizeof(DDSURFACEDESC));
	desc.dwSize = sizeof(DDSURFACEDESC);
	if (FAILED(hr = IDirectDrawSurface_Lock( (LPDIRECTDRAWSURFACE)surface, NULL, &desc, DDLOCK_SURFACEMEMORYPTR | /*DDLOCK_WRITEONLY | */ DDLOCK_WAIT, NULL))) {
		return GF_IO_ERR;
	}	
#else
	ZeroMemory(&desc, sizeof(DDSURFACEDESC2));
	desc.dwSize = sizeof(DDSURFACEDESC2);
	if (FAILED(hr = IDirectDrawSurface7_Lock( (LPDIRECTDRAWSURFACE7) surface, NULL, &desc, DDLOCK_SURFACEMEMORYPTR | /*DDLOCK_WRITEONLY | */ DDLOCK_WAIT, NULL))) {
		return GF_IO_ERR;
	}	
#endif
	vi->video_buffer = desc.lpSurface;
	vi->width = desc.dwWidth;
	vi->height = desc.dwHeight;
	vi->pitch = desc.lPitch;
	vi->is_hardware_memory = dd->systems_memory ? 0 : 1;
	return GF_OK;
}

static GF_Err DD_UnlockSurface(DDContext *dd, void *surface)
{
    HRESULT hr;
	if (!dd || !dd->ddraw_init) return GF_IO_ERR;
#ifdef USE_DX_3
	hr = IDirectDrawSurface_Unlock( (LPDIRECTDRAWSURFACE)surface, NULL);
#else
	hr = IDirectDrawSurface7_Unlock((LPDIRECTDRAWSURFACE7)surface, NULL);
#endif
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}


static GF_Err DD_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	DDCONTEXT;

	if (do_lock) {
		vi->pixel_format = dd->pixelFormat;
		return DD_LockSurface(dd, vi, dd->pBack);
	}
	else return DD_UnlockSurface(dd, dd->pBack);
}

static void *LockOSContext(GF_VideoOutput *dr, Bool do_lock)
{
	DDCONTEXT;

	if (!dd->pBack) return NULL;

	if (do_lock) {
		if (!dd->lock_hdc && ! IDirectDrawSurface_IsLost(dd->pBack)) {
			if (FAILED(IDirectDrawSurface_GetDC(dd->pBack, &dd->lock_hdc)) ) 
				dd->lock_hdc = NULL;
		} 
	} else if (dd->lock_hdc) {
		IDirectDrawSurface_ReleaseDC(dd->pBack, dd->lock_hdc);
		dd->lock_hdc = NULL;
	}
	return (void *)dd->lock_hdc ;
}


static GF_Err DD_BlitSurface(DDContext *dd, DDSurface *src, GF_Window *src_wnd, GF_Window *dst_wnd, GF_ColorKey *key)
{
	HRESULT hr;
	u32 dst_w, dst_h, src_w, src_h, flags;
	RECT r_dst, r_src;
	u32 left, top;
	src_w = src_wnd ? src_wnd->w : src->width;
	src_h = src_wnd ? src_wnd->h : src->height;
	dst_w = dst_wnd ? dst_wnd->w : dd->width;
	dst_h = dst_wnd ? dst_wnd->h : dd->height;
	
	if (src_wnd != NULL) MAKERECT(r_src, src_wnd);
	if (dst_wnd != NULL) MAKERECT(r_dst, dst_wnd);

	if (key) {
		u32 col;
		DDCOLORKEY ck;
		col = GF_COL_ARGB(0xFF, key->r, key->g, key->b);
		ck.dwColorSpaceHighValue = ck.dwColorSpaceLowValue = col;
		hr = IDirectDrawSurface_SetColorKey(src->pSurface, DDCKEY_SRCBLT, &ck);
		if (FAILED(hr)) return GF_IO_ERR;
	}

	if ((dst_w==src_w) && (dst_h==src_h)) {
		flags = DDBLTFAST_WAIT;
		if (key) flags |= DDBLTFAST_SRCCOLORKEY;
		if (dst_wnd) { left = r_dst.left; top = r_dst.top; } else left = top = 0; 
		hr = IDirectDrawSurface_BltFast(dd->pBack, left, top, src->pSurface, src_wnd ? &r_src : NULL, flags);
	} else {
		flags = DDBLT_WAIT;
		if (key) flags |= DDBLT_KEYSRC;
		hr = IDirectDrawSurface_Blt(dd->pBack, dst_wnd ? &r_dst : NULL, src->pSurface, src_wnd ? &r_src : NULL, flags, NULL);
	}
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}

static DDSurface *DD_GetSurface(GF_VideoOutput *dr, u32 width, u32 height, u32 pixel_format)
{
#ifdef USE_DX_3
	DDSURFACEDESC ddsd;
#else
	DDSURFACEDESC2 ddsd;
#endif
	HRESULT hr;
	DDCONTEXT;

	/*yuv format*/
	if (pixelformat_yuv(pixel_format)) {
		if (dr->yuv_pixel_format) {
			DDSurface *yuvp = &dd->yuv_pool;		

			/*don't recreate a surface if not needed*/
			if (yuvp->pSurface && (yuvp->width >= width) && (yuvp->height >= height) ) return yuvp;

			width = MAX(yuvp->width, width);
			height = MAX(yuvp->height, height);

			SAFE_DD_RELEASE(yuvp->pSurface);
			memset(yuvp, 0, sizeof(DDSurface));

			memset (&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(ddsd);
			ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
			ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
			ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
			ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
			ddsd.dwWidth = width;
			ddsd.dwHeight = height;
			ddsd.ddpfPixelFormat.dwFourCC = get_win_4CC(dr->yuv_pixel_format);

#ifdef USE_DX_3
			if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &yuvp->pSurface, NULL ) ) )
				return NULL;
#else
			if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &yuvp->pSurface, NULL ) ) )
				return NULL;
#endif
			yuvp->format = dr->yuv_pixel_format;
			yuvp->width = width;
			yuvp->height = height;


			return yuvp;
		}
	}

	/*don't recreate a surface if not needed*/
	if ((dd->rgb_pool.width >= width) && (dd->rgb_pool.height >= height) ) return &dd->rgb_pool;
	width = MAX(dd->rgb_pool.width, width);
	height = MAX(dd->rgb_pool.height, height);
	SAFE_DD_RELEASE(dd->rgb_pool.pSurface);
	memset(&dd->rgb_pool, 0, sizeof(DDSurface));

	memset (&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = width;
	ddsd.dwHeight = height;

#ifdef USE_DX_3
	if( FAILED( hr = IDirectDraw_CreateSurface(dd->pDD, &ddsd, &dd->rgb_pool.pSurface, NULL ) ) )
		return NULL;
#else
	if( FAILED( hr = IDirectDraw7_CreateSurface(dd->pDD, &ddsd, &dd->rgb_pool.pSurface, NULL ) ) )
		return NULL;
#endif

	dd->rgb_pool.width = width;
	dd->rgb_pool.height = height;
	dd->rgb_pool.format = dd->pixelFormat;
	return &dd->rgb_pool;
}

static GF_Err DD_Blit(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	GF_VideoSurface temp_surf;
	GF_Err e;
	GF_Window src_wnd_2;
	u32 w, h;
	DDSurface *pool;
	DDCONTEXT;

	if (src_wnd) {
		w = src_wnd->w;
		h = src_wnd->h;
	} else {
		w = video_src->width;
		h = video_src->height;
	}
	/*get RGB or YUV pool surface*/
	pool = DD_GetSurface(dr, w, h, video_src->pixel_format);
	if (!pool) return GF_IO_ERR;

	temp_surf.pixel_format = pool->format;
	e = DD_LockSurface(dd, &temp_surf, pool->pSurface);
	if (e) return e;

	/*copy pixels to pool*/
	dx_copy_pixels(&temp_surf, video_src, src_wnd);

	e = DD_UnlockSurface(dd, pool->pSurface);
	if (e) return e;

	if (overlay_type) {
		HRESULT hr;
		RECT dst_rc, src_rc;
		POINT pt;
		GF_Window dst = *dst_wnd;
		GF_Window src = *src_wnd;

		src.x = src.y = 0;
		MAKERECT(src_rc, (&src));
		pt.x = dst.x;
		pt.y = dst.y;
		ClientToScreen(dd->cur_hwnd, &pt);
		dst.x = pt.x;
		dst.y = pt.y;
		MAKERECT(dst_rc, (&dst));

#if 1
		if (overlay_type==1) {
			hr = IDirectDrawSurface2_UpdateOverlay(pool->pSurface, &src_rc, dd->pPrimary, &dst_rc, DDOVER_SHOW, NULL);
		} else {
			DDOVERLAYFX ddofx;
			memset(&ddofx, 0, sizeof(DDOVERLAYFX));
			ddofx.dwSize = sizeof(DDOVERLAYFX);
			ddofx.dckDestColorkey.dwColorSpaceLowValue = dr->overlay_color_key;
			ddofx.dckDestColorkey.dwColorSpaceHighValue = dr->overlay_color_key;
			hr = IDirectDrawSurface2_UpdateOverlay(pool->pSurface, &src_rc, dd->pPrimary, &dst_rc, DDOVER_SHOW | DDOVER_KEYDESTOVERRIDE, &ddofx);
		}
		if (FAILED(hr)) {
			IDirectDrawSurface2_UpdateOverlay(pool->pSurface, NULL, dd->pPrimary, NULL, DDOVER_HIDE, NULL);
		}
#else
		if (overlay_type==1) {
			hr = IDirectDrawSurface_Blt(dd->pPrimary, &dst_rc, pool->pSurface, &src_rc, 0, NULL);
		} else {
			DDBLTFX ddfx;
			memset(&ddfx, 0, sizeof(DDBLTFX));
			ddfx.dwSize = sizeof(DDBLTFX);
			ddfx.ddckDestColorkey.dwColorSpaceLowValue = dr->overlay_color_key;
			ddfx.ddckDestColorkey.dwColorSpaceHighValue = dr->overlay_color_key;

			hr = IDirectDrawSurface_Blt(dd->pPrimary, &dst_rc, pool->pSurface, &src_rc, DDBLT_WAIT | DDBLT_KEYDESTOVERRIDE, &ddfx);
		}
#endif
		return FAILED(hr) ? GF_IO_ERR : GF_OK;
	}

	src_wnd_2.x = src_wnd_2.y = 0;
	if (src_wnd) {
		src_wnd_2.w = src_wnd->w;
		src_wnd_2.h = src_wnd->h;
	} else {
		src_wnd_2.w = video_src->width;
		src_wnd_2.h = video_src->height;
	}
	/*and blit surface*/
	return DD_BlitSurface(dd, pool, &src_wnd_2, dst_wnd, NULL);
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
void DD_InitYUV(GF_VideoOutput *dr)
{
	u32 w, h, j, i, num_yuv;
	DWORD numCodes;
	DWORD formats[30];
	DWORD *codes;
	u32 now, min_packed = 0xFFFFFFFF, min_planar = 0xFFFFFFFF;
	u32 best_packed = 0, best_planar = 0;
	Bool checkPacked = TRUE;
	const char *opt;

	DDCONTEXT;

	w = 720;
	h = 576;

	dd->yuv_init = 1;

#ifdef USE_DX_3
	IDirectDraw_GetFourCCCodes(dd->pDD, &numCodes, NULL);
	if (!numCodes) return;
	codes = (DWORD *)malloc(numCodes*sizeof(DWORD));
	IDirectDraw_GetFourCCCodes(dd->pDD, &numCodes, codes);
#else
	IDirectDraw7_GetFourCCCodes(dd->pDD, &numCodes, NULL);
	if (!numCodes) return;
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
	if (!num_yuv) {
		dr->hw_caps &= ~(GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_YUV_OVERLAY);
		dr->yuv_pixel_format = 0;
		return;
	}

	for (i=0; i<num_yuv; i++) {
		/*check planar first*/
		if (!checkPacked && !is_yuv_planar(formats[i])) goto go_on;
		/*then check packed */
		if (checkPacked && is_yuv_planar(formats[i])) goto go_on;

		if (dd->yuv_pool.pSurface) {
			SAFE_DD_RELEASE(dd->yuv_pool.pSurface);
			memset(&dd->yuv_pool, 0, sizeof(DDSurface));
		}

		dr->yuv_pixel_format = formats[i];
		if (DD_GetSurface(dr, w, h, dr->yuv_pixel_format) == NULL)
			goto rem_fmt;

		now = gf_sys_clock();
		/*perform blank blit*/
		for (j=0; j<YUV_NUM_TEST; j++) {
			if (DD_BlitSurface(dd, &dd->yuv_pool, NULL, NULL, NULL) != GF_OK)
				goto rem_fmt;
		}
		now = gf_sys_clock() - now;

		if (!checkPacked) {
			if (now<min_planar) {
				min_planar = now;
				best_planar = dr->yuv_pixel_format;
			}
		} else {
			if (now<min_packed) {
				min_packed = now;
				best_packed = dr->yuv_pixel_format;
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

	if (dd->yuv_pool.pSurface) {
		SAFE_DD_RELEASE(dd->yuv_pool.pSurface);
		memset(&dd->yuv_pool, 0, sizeof(DDSurface));
	}
	/*too bad*/
	if (!num_yuv) {
		dr->hw_caps &= ~(GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_YUV_OVERLAY);
		dr->yuv_pixel_format = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] YUV hardware not available\n"));
		return;
	}

	if (best_planar && (min_planar < min_packed )) {
		dr->yuv_pixel_format = best_planar;
	} else {
		min_planar = min_packed;
		dr->yuv_pixel_format = best_packed;
	} 
	dr->yuv_pixel_format = GF_PIXEL_YV12;

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] Picked YUV format %s - drawn in %d ms\n", gf_4cc_to_str(dr->yuv_pixel_format), min_planar));
	dr->hw_caps |= GF_VIDEO_HW_HAS_YUV_OVERLAY;

	/*enable YUV->RGB on the backbuffer ?*/
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "EnableOffscreenYUV");
	/*no set is the default*/
	if (!opt) {
		opt = "yes";
		gf_modules_set_option((GF_BaseInterface *)dr, "Video", "EnableOffscreenYUV", "yes");
	}
	if (!strcmp(opt, "yes")) dr->hw_caps |= GF_VIDEO_HW_HAS_YUV;
	
	/*get YUV overlay key*/
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "OverlayColorKey");
	/*no set is the default*/
	if (!opt) {
		opt = "0101FE";
		gf_modules_set_option((GF_BaseInterface *)dr, "Video", "OverlayColorKey", "0101FE");
	}
	sscanf(opt, "%06x", &dr->overlay_color_key);
	if (dr->overlay_color_key) dr->overlay_color_key |= 0xFF000000;
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] YUV->RGB enabled: %s - ColorKey enabled: %s (key %x)\n", 
									(dr->hw_caps & GF_VIDEO_HW_HAS_YUV) ? "Yes" : "No",
									dr->overlay_color_key ? "Yes" : "No", dr->overlay_color_key
							));
}

GF_Err DD_SetBackBufferSize(GF_VideoOutput *dr, u32 width, u32 height, Bool use_system_memory)
{
	DDCONTEXT;
	if (dd->output_3d_type) return GF_BAD_PARAM;
	if (!dd->ddraw_init) return InitDirectDraw(dr, width, height);
	return CreateBackBuffer(dr, width, height, use_system_memory);
}


void DD_SetupDDraw(GF_VideoOutput *driv)
{
	driv->hw_caps |= GF_VIDEO_HW_HAS_COLOR_KEY;
	driv->Blit = DD_Blit;
	driv->LockBackBuffer = DD_LockBackBuffer;
	driv->LockOSContext = LockOSContext;
}

