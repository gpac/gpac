/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
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

enum
{
	GF_PIXEL_IYUV = GF_4CC('I', 'Y', 'U', 'V'),
	GF_PIXEL_I420 = GF_4CC('I', '4', '2', '0'),
	/*!YUV packed format*/
	GF_PIXEL_UYNV = GF_4CC('U', 'Y', 'N', 'V'),
	/*!YUV packed format*/
	GF_PIXEL_YUNV = GF_4CC('Y', 'U', 'N', 'V'),
	/*!YUV packed format*/
	GF_PIXEL_V422 = GF_4CC('V', '4', '2', '2'),

	GF_PIXEL_YV12 = GF_4CC('Y', 'V', '1', '2'),
	GF_PIXEL_Y422 = GF_4CC('Y', '4', '2', '2'),
	GF_PIXEL_YUY2 = GF_4CC('Y', 'U', 'Y', '2'),
};



static Bool pixelformat_yuv(u32 pixel_format)
{
	switch (pixel_format) {
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_UYNV:
	case GF_PIXEL_YUNV:
	case GF_PIXEL_V422:
	case GF_PIXEL_YUV:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}


static u32 get_win_4CC(u32 pixel_format)
{
	switch (pixel_format) {
	case GF_PIXEL_YUYV:
		return mmioFOURCC('Y', 'U', 'Y', '2');
	case GF_PIXEL_YVYU:
		return mmioFOURCC('Y', 'V', 'Y', 'U');
	case GF_PIXEL_UYVY:
		return mmioFOURCC('U', 'Y', 'V', 'Y');
	case GF_PIXEL_VYUY:
		return mmioFOURCC('V', 'Y', 'U', 'Y');
	case GF_PIXEL_YUV422:
		return mmioFOURCC('Y', '4', '2', '2');
	case GF_PIXEL_UYNV:
		return mmioFOURCC('U', 'Y', 'N', 'V');
	case GF_PIXEL_YUNV:
		return mmioFOURCC('Y', 'U', 'N', 'V');
	case GF_PIXEL_V422:
		return mmioFOURCC('V', '4', '2', '2');
	case GF_PIXEL_YUV:
		return mmioFOURCC('Y', 'V', '1', '2');
	case GF_PIXEL_IYUV:
		return mmioFOURCC('I', 'Y', 'U', 'V');
	case GF_PIXEL_I420:
		return mmioFOURCC('I', '4', '2', '0');
	case GF_PIXEL_YUV_10:
		return mmioFOURCC('P', '0', '1', '0');
	case GF_PIXEL_YUV444:
		return mmioFOURCC('Y', '4', '4', '4');
	case GF_PIXEL_YUV444_10:
		return mmioFOURCC('Y', '4', '1', '0');
	case GF_PIXEL_YUV422_10:
		return mmioFOURCC('Y', '2', '1', '0');
	default:
		return 0;
	}
}

/*!\hideinitializer transfoms a 32-bits color into a 16-bits one.\note alpha component is lost*/
#define GF_COL_TO_565(c) (((GF_COL_R(c) & 248)<<8) + ((GF_COL_G(c) & 252)<<3)  + (GF_COL_B(c)>>3))
/*!\hideinitializer transfoms a 32-bits color into a 15-bits one.\note alpha component is lost*/
#define GF_COL_TO_555(c) (((GF_COL_R(c) & 248)<<7) + ((GF_COL_G(c) & 248)<<2)  + (GF_COL_B(c)>>3))

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

	hr = dd->pBack->lpVtbl->Blt(dd->pBack, &rc, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);

	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}

GF_Err CreateBackBuffer(GF_VideoOutput *dr, u32 Width, u32 Height, Bool use_system_memory)
{
	Bool force_reinit;
	HRESULT hr;
	const char *opt;
	DDSURFDESC ddsd;

	DDCONTEXT;

	opt = gf_module_get_key((GF_BaseInterface*)dr, "hwvmem");
	if (opt) {
		if (use_system_memory) {
			if (!strcmp(opt, "always")) use_system_memory = GF_FALSE;
		} else {
			if (!strcmp(opt, "never")) use_system_memory = GF_TRUE;
			else if (!strcmp(opt, "auto")) dd->force_video_mem_for_yuv = GF_TRUE;
		}
	}

	force_reinit = GF_FALSE;
	if (use_system_memory && !dd->systems_memory) force_reinit = GF_TRUE;
	else if (!use_system_memory && dd->systems_memory) force_reinit = GF_TRUE;
	if (dd->pBack && !force_reinit&& !dd->fullscreen && (dd->width == Width) && (dd->height == Height) ) {
		return GF_OK;
	}

	SAFE_DD_RELEASE(dd->pBack);

	/*create backbuffer*/
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	if (dd->systems_memory==2) {
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	} else {
		if (!use_system_memory) {
			dd->systems_memory = GF_FALSE;
			ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		} else {
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
			dd->systems_memory = GF_TRUE;
		}
	}
	if (dd->systems_memory) dr->hw_caps &= ~GF_VIDEO_HW_HAS_RGB;
	else  dr->hw_caps |= GF_VIDEO_HW_HAS_RGB;


	ddsd.dwWidth = Width;
	ddsd.dwHeight = Height;

	hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL );

	if (FAILED(hr)) {
		if (!dd->systems_memory) {
			ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
			dd->systems_memory = 2;
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] Hardware Video not available for backbuffer)\n"));

			hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &dd->pBack, NULL );
		}
		if (FAILED(hr)) return GF_IO_ERR;
	}

	/*store size*/
	if (!dd->fullscreen) {
		dd->width = Width;
		dd->height = Height;
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
	DDSURFDESC ddsd;
	DDPIXELFORMAT pixelFmt;
	LPDIRECTDRAWCLIPPER pcClipper;
	DDCONTEXT;
	if (!dd->cur_hwnd || !Width || !Height || !dd->DirectDrawCreate) return GF_BAD_PARAM;
	DestroyObjects(dd);

	if( FAILED( hr = dd->DirectDrawCreate(NULL, &ddraw, NULL ) ) )
		return GF_IO_ERR;

	hr = ddraw->lpVtbl->QueryInterface(ddraw, &IID_IDirectDraw7, (LPVOID *)&dd->pDD);
	ddraw->lpVtbl->Release(ddraw);
	if (FAILED(hr)) return GF_IO_ERR;


	dd->switch_res = GF_FALSE;
	cooplev = DDSCL_NORMAL;
	/*Setup FS*/
	if (dd->fullscreen) {

		/*change display mode*/
		if (dd->switch_res) {
#ifdef USE_DX_3
			hr = dd->pDD->lpVtbl->SetDisplayMode(dd->pDD, dd->fs_width, dd->fs_height, dd->video_bpp);
#else
			hr = dd->pDD->lpVtbl->SetDisplayMode(dd->pDD, dd->fs_width, dd->fs_height, dd->video_bpp, 0, 0 );
#endif
			if( FAILED(hr)) return GF_IO_ERR;
		}
		dd->NeedRestore = 1;
//		cooplev = DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN;
	}

	hr = dd->pDD->lpVtbl->SetCooperativeLevel(dd->pDD, dd->cur_hwnd, cooplev);
	if( FAILED(hr) ) return GF_IO_ERR;

	/*create primary*/
	ZeroMemory( &ddsd, sizeof( ddsd ) );
	ddsd.dwSize = sizeof( ddsd );
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &dd->pPrimary, NULL);
	if( FAILED(hr) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DX Out] Failed creating primary surface: error %08x\n", hr));
		return GF_IO_ERR;
	}

	/*get pixel format of video board*/
	memset (&pixelFmt, 0, sizeof(pixelFmt));
	pixelFmt.dwSize = sizeof(pixelFmt);

	hr = dd->pPrimary->lpVtbl->GetPixelFormat(dd->pPrimary, &pixelFmt);
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
			dd->pixelFormat = GF_PIXEL_BGR;
		else if ((pixelFmt.dwRBitMask == 0xFF0000) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0x0000FF))
			dd->pixelFormat = GF_PIXEL_RGB;
		dd->video_bpp = 24;
		break;
	case 32:
		if ((pixelFmt.dwRBitMask == 0x0000FF) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0xFF0000)) {
			dd->pixelFormat = dd->force_alpha ? GF_PIXEL_RGBA : GF_PIXEL_BGRX;
		} else if ((pixelFmt.dwRBitMask == 0xFF0000) && (pixelFmt.dwGBitMask == 0x00FF00) && (pixelFmt.dwBBitMask == 0x0000FF)) {
			dd->pixelFormat = dd->force_alpha ? GF_PIXEL_ARGB : GF_PIXEL_RGBX;
		}
		dd->video_bpp = 32;
		break;
	default:
		return GF_IO_ERR;
	}

	hr = dd->pDD->lpVtbl->CreateClipper(dd->pDD, 0, &pcClipper, NULL);
	if( FAILED(hr))
		return GF_IO_ERR;

	hr = pcClipper->lpVtbl->SetHWnd(pcClipper, 0, dd->cur_hwnd);
	if( FAILED(hr) ) {
		pcClipper->lpVtbl->Release(pcClipper);
		return GF_IO_ERR;
	}
	hr = dd->pPrimary->lpVtbl->SetClipper(dd->pPrimary, pcClipper);
	if( FAILED(hr)) {
		pcClipper->lpVtbl->Release(pcClipper);
		return GF_IO_ERR;
	}
	pcClipper->lpVtbl->Release(pcClipper);


	dd->ddraw_init = GF_TRUE;
	/*if YUV not initialize, init using HW video memory to setup HW caps*/
	return GF_OK;
}

static GF_Err DD_LockSurface(DDContext *dd, GF_VideoSurface *vi, LPDDRAWSURFACE surface)
{
	HRESULT hr;
	DDSURFDESC desc;

	if (!dd || !vi || !surface) return GF_BAD_PARAM;

	ZeroMemory(&desc, sizeof(desc));
	desc.dwSize = sizeof(DDSURFACEDESC);

	hr = surface->lpVtbl->Lock(surface, NULL, &desc, DDLOCK_SURFACEMEMORYPTR | /*DDLOCK_WRITEONLY | */ DDLOCK_WAIT, NULL);
	if (FAILED(hr))
		return GF_IO_ERR;

	vi->video_buffer = (char*)desc.lpSurface;
	vi->width = desc.dwWidth;
	vi->height = desc.dwHeight;
	vi->pitch_x = 0;
	vi->pitch_y = desc.lPitch;
	vi->is_hardware_memory = dd->systems_memory ? GF_FALSE : GF_TRUE;
	return GF_OK;
}

static GF_Err DD_UnlockSurface(DDContext *dd, LPDDRAWSURFACE surface)
{
	HRESULT hr;
	if (!dd || !dd->ddraw_init) return GF_IO_ERR;
	hr = surface->lpVtbl->Unlock(surface, NULL);
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}


static GF_Err DD_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	DDCONTEXT;

	if (do_lock) {
		memset(vi, 0, sizeof(GF_VideoSurface));
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
		if (!dd->lock_hdc && ! dd->pBack->lpVtbl->IsLost(dd->pBack)) {
			if (FAILED(dd->pBack->lpVtbl->GetDC(dd->pBack, &dd->lock_hdc)) )
				dd->lock_hdc = NULL;
		}
	} else if (dd->lock_hdc) {
		dd->pBack->lpVtbl->ReleaseDC(dd->pBack, dd->lock_hdc);
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
		hr = src->pSurface->lpVtbl->SetColorKey(src->pSurface, DDCKEY_SRCBLT, &ck);
		if (FAILED(hr))
			return GF_IO_ERR;
	}

	if ((dst_w==src_w) && (dst_h==src_h)) {
		flags = DDBLTFAST_WAIT;
		if (key) flags |= DDBLTFAST_SRCCOLORKEY;
		if (dst_wnd) {
			left = r_dst.left;
			top = r_dst.top;
		}
		else left = top = 0;
		hr = dd->pBack->lpVtbl->BltFast(dd->pBack, left, top, src->pSurface, src_wnd ? &r_src : NULL, flags);
	} else {
		flags = DDBLT_WAIT;
		if (key) flags |= DDBLT_KEYSRC;
		hr = dd->pBack->lpVtbl->Blt(dd->pBack, dst_wnd ? &r_dst : NULL, src->pSurface, src_wnd ? &r_src : NULL, flags, NULL);
	}

	if (src->is_yuv && dd->force_video_mem_for_yuv && dd->systems_memory) return GF_IO_ERR;

#ifndef GPAC_DISABLE_LOG
	if (hr) {
		GF_LOG(dd->systems_memory ? GF_LOG_INFO : GF_LOG_ERROR, GF_LOG_MMIO, ("[DX Out] Failed blitting %s %s memory: error %08x\n", src->is_yuv ? "YUV" : "RGB", dd->systems_memory ? "systems" : "hardware", hr));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DX Out] %s blit %s memory %dx%d -> %dx%d\n", src->is_yuv ? "YUV" : "RGB", dd->systems_memory ? "systems" : "hardware", src_w, src_h, dst_w, dst_h));
	}
#endif
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}

static DDSurface *DD_GetSurface(GF_VideoOutput *dr, u32 width, u32 height, u32 pixel_format, Bool check_caps)
{
	DDSURFDESC ddsd;
	HRESULT hr;
	DDCONTEXT;

	if (!dd->pDD) return NULL;

	/*yuv format*/
	if (pixelformat_yuv(pixel_format)) {
		/*some drivers give broken result if YUV surface dimensions are not even*/
		while (width%2) width++;
		while (height%2) height++;

		if (dr->yuv_pixel_format) {
			DDSurface *yuvp = &dd->yuv_pool;

			/*don't recreate a surface if not needed*/
			if (yuvp->pSurface && (yuvp->width >= width) && (yuvp->height >= height) ) return yuvp;

			width = MAX(yuvp->width, width);
			height = MAX(yuvp->height, height);

			SAFE_DD_RELEASE(yuvp->pSurface);
			memset(yuvp, 0, sizeof(DDSurface));
			yuvp->is_yuv = GF_TRUE;

			memset (&ddsd, 0, sizeof(ddsd));
			ddsd.dwSize = sizeof(ddsd);
			ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
			ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;

			ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;

			if (!dd->offscreen_yuv_to_rgb && (dr->hw_caps & GF_VIDEO_HW_HAS_YUV_OVERLAY))
				ddsd.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;

			ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
			ddsd.dwWidth = width;
			ddsd.dwHeight = height;
			ddsd.ddpfPixelFormat.dwFourCC = get_win_4CC(dr->yuv_pixel_format);

			hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &yuvp->pSurface, NULL);
			if (FAILED(hr) ) {
				if (!check_caps) return NULL;

				/*try without offscreen yuv->rgbcap*/
				if (dd->offscreen_yuv_to_rgb) {
					dd->offscreen_yuv_to_rgb = GF_FALSE;
					if (dr->hw_caps & GF_VIDEO_HW_HAS_YUV_OVERLAY) {
						ddsd.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;
						hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &yuvp->pSurface, NULL);
						if( FAILED(hr) ) {
							return NULL;
						}
					} else {
						return NULL;
					}
				}

				/*try without overlay cap*/
				if (dr->hw_caps & GF_VIDEO_HW_HAS_YUV_OVERLAY) {
					dr->hw_caps &= ~GF_VIDEO_HW_HAS_YUV_OVERLAY;
					ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;

					hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &yuvp->pSurface, NULL);
					if( FAILED(hr) ) {
						return NULL;
					}
				}
				/*YUV not supported*/
				else {
					return NULL;
				}

			}
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

	hr = dd->pDD->lpVtbl->CreateSurface(dd->pDD, &ddsd, &dd->rgb_pool.pSurface, NULL);
	if (FAILED(hr))
		return NULL;
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

	if (!video_src) {
		if (overlay_type && dd->yuv_pool.pSurface)
			dd->yuv_pool.pSurface->lpVtbl->UpdateOverlay(dd->yuv_pool.pSurface, NULL, dd->pPrimary, NULL, DDOVER_HIDE, NULL);
		return GF_OK;
	}
	if (src_wnd) {
		w = src_wnd->w;
		h = src_wnd->h;
	} else {
		w = video_src->width;
		h = video_src->height;
	}
	/*get RGB or YUV pool surface*/
	//if (video_src->pixel_format==GF_PIXEL_YUVD) return GF_NOT_SUPPORTED;
	if (video_src->pixel_format==GF_PIXEL_YUVD) video_src->pixel_format = GF_PIXEL_YUV;
	pool = DD_GetSurface(dr, w, h, video_src->pixel_format, GF_FALSE);
	if (!pool)
		return GF_IO_ERR;

	memset(&temp_surf, 0, sizeof(GF_VideoSurface));
	temp_surf.pixel_format = pool->format;
	e = DD_LockSurface(dd, &temp_surf, pool->pSurface);
	if (e) return e;

	/*copy pixels to pool*/
	e = gf_stretch_bits(&temp_surf, video_src, NULL, src_wnd, 0xFF, GF_FALSE, NULL, NULL);

	DD_UnlockSurface(dd, pool->pSurface);
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

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DX] Blit surface to dest %d x %d - overlay type %s\n", dst.w, dst.h, (overlay_type==1) ? "Top-Level" : "ColorKey" ));

#if 1
		if (overlay_type==1) {
			hr = pool->pSurface->lpVtbl->UpdateOverlay(pool->pSurface, &src_rc, dd->pPrimary, &dst_rc, DDOVER_SHOW | DDOVER_AUTOFLIP, NULL);
		} else {
			DDOVERLAYFX ddofx;
			memset(&ddofx, 0, sizeof(DDOVERLAYFX));
			ddofx.dwSize = sizeof(DDOVERLAYFX);
			ddofx.dckDestColorkey.dwColorSpaceLowValue = dr->overlay_color_key;
			ddofx.dckDestColorkey.dwColorSpaceHighValue = dr->overlay_color_key;
			hr = pool->pSurface->lpVtbl->UpdateOverlay(pool->pSurface, &src_rc, dd->pPrimary, &dst_rc, DDOVER_SHOW | DDOVER_KEYDESTOVERRIDE | DDOVER_AUTOFLIP, &ddofx);
		}
		if (FAILED(hr)) {
			pool->pSurface->lpVtbl->UpdateOverlay(pool->pSurface, NULL, dd->pPrimary, NULL, DDOVER_HIDE, NULL);
		}
#else
		if (overlay_type==1) {
			hr = dd->pPrimary->lpVtbl->Blt(dd->pPrimary, &dst_rc, pool->pSurface, &src_rc, 0, NULL);
		} else {
			DDBLTFX ddfx;
			memset(&ddfx, 0, sizeof(DDBLTFX));
			ddfx.dwSize = sizeof(DDBLTFX);
			ddfx.ddckDestColorkey.dwColorSpaceLowValue = dr->overlay_color_key;
			ddfx.ddckDestColorkey.dwColorSpaceHighValue = dr->overlay_color_key;

			hr = dd->pPrimary->lpVtbl->Blt(dd->pPrimary, &dst_rc, pool->pSurface, &src_rc, DDBLT_WAIT | DDBLT_KEYDESTOVERRIDE, &ddfx);
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
	if (win_4cc==get_win_4CC(GF_PIXEL_YV12)) return GF_PIXEL_YUV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_I420)) return GF_PIXEL_I420;
	else if (win_4cc==get_win_4CC(GF_PIXEL_IYUV)) return GF_PIXEL_IYUV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_UYVY)) return GF_PIXEL_UYVY;
	else if (win_4cc==get_win_4CC(GF_PIXEL_Y422)) return GF_PIXEL_YUV422;
	else if (win_4cc==get_win_4CC(GF_PIXEL_UYNV)) return GF_PIXEL_UYNV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_YUY2)) return GF_PIXEL_YUYV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_YUNV)) return GF_PIXEL_YUNV;
	else if (win_4cc==get_win_4CC(GF_PIXEL_V422)) return GF_PIXEL_V422;
	else if (win_4cc==get_win_4CC(GF_PIXEL_YVYU)) return GF_PIXEL_YVYU;
	else return 0;
}

static GFINLINE Bool is_yuv_planar(u32 format)
{
	switch  (format) {
	case GF_PIXEL_YUV:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_I420:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

#define YUV_NUM_TEST	20

/*gets fastest YUV format for YUV to RGB blit from YUV overlay (support is quite random on most cards)*/
void DD_InitYUV(GF_VideoOutput *dr)
{
	u32 w, h, j, i, num_yuv;
	Bool force_yv12 = GF_FALSE;
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


	opt = gf_module_get_key((GF_BaseInterface *)dr, "yuv-4cc");
	if (opt) {
		char a, b, c, d;
		if (sscanf(opt, "%c%c%c%c", &a, &b, &c, &d)==4) {
			dr->yuv_pixel_format = GF_4CC(a, b, c, d);
			dr->hw_caps |= GF_VIDEO_HW_HAS_YUV;
			if (gf_module_get_bool((GF_BaseInterface *)dr, "yuv-overlay"))
				dr->hw_caps |= GF_VIDEO_HW_HAS_YUV_OVERLAY;
			dd->yuv_init = GF_TRUE;
			num_yuv = 1;
		}
	}

	/*first run on this machine, to some benchmark*/
	if (! dd->yuv_init) {
		char szOpt[100];
		dd->yuv_init = GF_TRUE;

		dr->hw_caps |= GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_YUV_OVERLAY;

		dd->pDD->lpVtbl->GetFourCCCodes(dd->pDD, &numCodes, NULL);
		if (!numCodes) return;
		codes = (DWORD *)gf_malloc(numCodes*sizeof(DWORD));
		dd->pDD->lpVtbl->GetFourCCCodes(dd->pDD, &numCodes, codes);

		num_yuv = 0;
		for (i=0; i<numCodes; i++) {
			formats[num_yuv] = is_yuv_supported(codes[i]);
			if (formats[num_yuv]) num_yuv++;
		}
		gf_free(codes);

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

			SAFE_DD_RELEASE(dd->yuv_pool.pSurface);
			memset(&dd->yuv_pool, 0, sizeof(DDSurface));
			dd->yuv_pool.is_yuv = GF_TRUE;

			dr->yuv_pixel_format = formats[i];
			if (DD_GetSurface(dr, w, h, dr->yuv_pixel_format, GF_TRUE) == NULL)
				goto rem_fmt;

			now = gf_sys_clock();
			/*perform blank blit*/
			for (j=0; j<YUV_NUM_TEST; j++) {
				if (DD_BlitSurface(dd, &dd->yuv_pool, NULL, NULL, NULL) != GF_OK)
					goto rem_fmt;
			}
			now = gf_sys_clock() - now;
			if (formats[i]== GF_PIXEL_YUV)
				force_yv12 = GF_TRUE;

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

		SAFE_DD_RELEASE(dd->yuv_pool.pSurface);
		memset(&dd->yuv_pool, 0, sizeof(DDSurface));
		dd->yuv_pool.is_yuv = GF_TRUE;

		if (best_planar && (min_planar <= min_packed )) {
			dr->yuv_pixel_format = best_planar;
		} else {
			min_planar = min_packed;
			dr->yuv_pixel_format = best_packed;
		}
		if (force_yv12)
			dr->yuv_pixel_format = GF_PIXEL_YUV;

		/*store our options*/
		sprintf(szOpt, "%c%c%c%c", (dr->yuv_pixel_format>>24) & 0xFF, (dr->yuv_pixel_format>>16) & 0xFF, (dr->yuv_pixel_format>>8) & 0xFF, (dr->yuv_pixel_format) & 0xFF);
		gf_opts_set_key("directx", "yuv-4cc", szOpt);
		gf_opts_set_key("directx", "yuv-overlay", (dr->hw_caps & GF_VIDEO_HW_HAS_YUV_OVERLAY) ? "yes" : "no");
	}

	opt = gf_module_get_key((GF_BaseInterface*)dr, "hwvmem");
	if (opt && !strcmp(opt, "never")) num_yuv = 0;

	/*too bad*/
	if (!num_yuv) {
		dr->hw_caps &= ~(GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_YUV_OVERLAY);
		dr->yuv_pixel_format = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] YUV hardware not available\n"));
		return;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] Picked YUV format %s - drawn in %d ms\n", gf_4cc_to_str(dr->yuv_pixel_format), min_planar));

	/*enable YUV->RGB on the backbuffer ?*/
	if (gf_module_get_bool((GF_BaseInterface *)dr, "offscreen-yuv")) {
		dr->hw_caps &= ~GF_VIDEO_HW_HAS_YUV;
	} else {
		dd->offscreen_yuv_to_rgb = GF_TRUE;
	}

	/*get YUV overlay key*/
	opt = gf_module_get_bool((GF_BaseInterface *)dr, "overlay-color-key");
	/*no set is the default*/
	if (!opt) {
		opt = "0101FE";
		gf_opts_set_key("directx", "overlay-color-key", "0101FE");
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
	GF_Err e;
	DDCONTEXT;
#ifndef GPAC_DISABLE_3D
	if (dd->output_3d) return GF_BAD_PARAM;
#endif
	if (!dd->ddraw_init) {
		e = InitDirectDraw(dr, width, height);
		if (e) return e;
	}
	return CreateBackBuffer(dr, width, height, use_system_memory);
}

static GF_GPACArg DirectDrawArgs[] = {
	GF_DEF_ARG("yuv-overlay", NULL, "enable yuv overlays", "false", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("yuv-4cc", NULL, "prefered YUV 4CC for overlays", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("offscreen-yuv", NULL, "enable offscreen yuv", "true", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("overlay-color-key", NULL, "color key value for overlay", "0101FE", NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("no-vsync", NULL, "disable vertical synchro", "false", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("switch-vres", NULL, "enable resolution switching of display", "false", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("hwvmem", NULL, "specify (2D rendering only) memory type of main video backbuffer. Depending on the scene type, this may drastically change the playback speed\n"
 "- always: always on hardware\n"
 "- never: always on system memory\n"
 "- auto: selected by GPAC based on content type (graphics or video)", "auto", "auto|always|never", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
	{0},
};

void DD_SetupDDraw(GF_VideoOutput *driv)
{
	driv->hw_caps |= GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_STRETCH;
	driv->Blit = DD_Blit;
	driv->LockBackBuffer = DD_LockBackBuffer;
	driv->LockOSContext = LockOSContext;
	driv->args = DirectDrawArgs;
	driv->description = "Video output using DirectDraw";
}

