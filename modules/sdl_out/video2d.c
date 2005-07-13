/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / SDL audio and video module
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

#include "sdl_out.h"
#include <gpac/constants.h>


#define SDLVID()	SDLVidCtx *ctx = (SDLVidCtx *)dr->opaque

static Bool SDLVid_SurfaceOK(SDLVidCtx *ctx, SDLWrapSurface *surf, Bool remove)
{
	s32 i = gf_list_find(ctx->surfaces, surf);
	if (i<0) return 0;
	if (remove) gf_list_rem(ctx->surfaces, (u32) i);
	return 1;
}

static u32 SDLVid_MapPixelFormat(SDL_PixelFormat *format)
{
	if (format->palette) return 0;
	switch (format->BitsPerPixel) {
	case 16:
		if ((format->Rmask==0x7c00) && (format->Gmask==0x03e0) && (format->Bmask==0x001f) ) return GF_PIXEL_RGB_555;
		if ((format->Rmask==0xf800) && (format->Gmask==0x07e0) && (format->Bmask==0x001f) ) return GF_PIXEL_RGB_565;
		return 0;
	case 24:
		if (format->Rmask==0x00FF0000) return GF_PIXEL_RGB_24;
		if (format->Rmask==0x000000FF) return GF_PIXEL_BGR_24;
		return 0;
	case 32:
		if (format->Amask==0xFF000000) return GF_PIXEL_ARGB;
		if (format->Rmask==0x00FF0000) return GF_PIXEL_RGB_32;
		if (format->Rmask==0x000000FF) return GF_PIXEL_BGR_32;
		return 0;
	default:
		return 0;
	}
}


GF_Err SDLVid_Clear(GF_VideoOutput *dr, u32 color)
{
	u32 col;
	SDLVID();
	if (!ctx->screen) return GF_OK;
	col = SDL_MapRGB(ctx->screen->format, (u8) ((color>>16)&0xFF), (u8) ((color>>8)&0xFF), (u8) ((color)&0xFF));
	SDL_FillRect(ctx->screen, NULL, col);
	SDL_Flip(ctx->screen);
	return GF_OK;
}

SDL_Surface *SDLVid_CreateSDLSurface(SDLVidCtx *ctx, u32 width, u32 height, u32 pixel_format)
{
	u32 Amask, Rmask, Gmask, Bmask;
	SDL_Surface *surf;
	u32 bpp;
	Amask = 0;
	switch (pixel_format) {
	case GF_PIXEL_RGB_555:
		bpp = 16;
		Rmask=0x7c00; Gmask=0x03e0; Bmask=0x001f;
		break;
	case GF_PIXEL_RGB_565:
		bpp = 16;
		Rmask=0xf800; Gmask=0x07e0; Bmask=0x001f;
		break;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGB_32:
		bpp = (pixel_format==GF_PIXEL_RGB_24) ? 24 : 32;
		Rmask=0x00ff0000; Gmask=0x0000ff00; Bmask=0x000000ff;
		break;
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_BGR_32:
		bpp = (pixel_format==GF_PIXEL_BGR_24) ? 24 : 32;
		Rmask=0x000000ff; Gmask=0x0000ff00; Bmask=0x00ff0000;
		break;
	case GF_PIXEL_ARGB:
		bpp = 32;
		Amask = 0xFF000000; Rmask=0x00ff0000; Gmask=0x0000ff00; Bmask=0x000000ff;
		break;
	default:
		return NULL;
	}
	surf = SDL_CreateRGBSurface(0L, width, height, bpp, Rmask, Gmask, Bmask, Amask);

	/*try with surface settings*/
	if (!surf) surf = SDL_CreateRGBSurface(0L, width, height, ctx->screen->format->BitsPerPixel, 
							ctx->screen->format->Rmask, ctx->screen->format->Gmask, 
							ctx->screen->format->Bmask, ctx->screen->format->Amask);

	return surf;
}

static GF_Err SDLVid_CreateSurface(GF_VideoOutput *dr, u32 width, u32 height, u32 pixel_format, u32 *surface_id)
{
	SDL_Surface *surf;
	SDLWrapSurface *wrap;
	SDLVID();

	surf = SDLVid_CreateSDLSurface(ctx, width, height, pixel_format);
	if (!surf) return GF_NOT_SUPPORTED;

	wrap = malloc(sizeof(SDLWrapSurface));
	wrap->pixel_format = SDLVid_MapPixelFormat(surf->format);
	wrap->surface = surf;
	wrap->id = (u32) wrap;
	*surface_id = wrap->id;
	return gf_list_add(ctx->surfaces, wrap);
}

static GF_Err SDLVid_DeleteSurface(GF_VideoOutput *dr, u32 surface_id)
{
	SDLWrapSurface *wrap;
	SDLVID();
	if (!surface_id) return GF_BAD_PARAM;
	wrap = (SDLWrapSurface *) surface_id;
	if (!SDLVid_SurfaceOK(ctx, wrap, 1)) return GF_BAD_PARAM;
	if (wrap->surface) SDL_FreeSurface(wrap->surface);
	free(wrap);
	return GF_OK;
}

static GF_Err SDLVid_LockSurface(GF_VideoOutput *dr, u32 surface_id, GF_VideoSurface *video_info)
{
	SDL_Surface *surf;
	SDLVID();
	if (!surface_id) {
		surf = ctx->back_buffer;
	} else {
		SDLWrapSurface *wrap = (SDLWrapSurface *) surface_id;
		if (!SDLVid_SurfaceOK(ctx, wrap, 0)) return GF_BAD_PARAM;
		surf = wrap->surface;
	}
	if (!surf) return GF_BAD_PARAM;
	if (SDL_LockSurface(surf)<0) return GF_IO_ERR;
	video_info->width = surf->w;
	video_info->height = surf->h;
	video_info->pitch = surf->pitch;
	video_info->os_handle = NULL;
	video_info->video_buffer = surf->pixels;
	video_info->pixel_format = SDLVid_MapPixelFormat(surf->format);
	return GF_OK;
}

static GF_Err SDLVid_UnlockSurface(GF_VideoOutput *dr, u32 surface_id)
{
	SDL_Surface *surf;
	SDLVID();
	if (!surface_id) {
		surf = ctx->back_buffer;
	} else {
		SDLWrapSurface *wrap = (SDLWrapSurface *) surface_id;
		if (!SDLVid_SurfaceOK(ctx, wrap, 0)) return GF_BAD_PARAM;
		surf = wrap->surface;
	}
	if (!surf) return GF_BAD_PARAM;
	SDL_UnlockSurface(surf);
	return GF_OK;
}

static Bool SDLVid_IsSurfaceValid(GF_VideoOutput *dr, u32 surface_id)
{
	SDLVID();
	SDLWrapSurface *wrap = (SDLWrapSurface *) surface_id;
	return SDLVid_SurfaceOK(ctx, wrap, 0);
}

static GF_Err SDLVid_ResizeSurface(GF_VideoOutput *dr, u32 surface_id, u32 width, u32 height)
{
	SDLVID();
	SDLWrapSurface *wrap = (SDLWrapSurface *) surface_id;
	if (!SDLVid_SurfaceOK(ctx, wrap, 0)) return GF_BAD_PARAM;
	if (wrap->surface && ((u32) wrap->surface->w >= width) && ((u32) wrap->surface->h >= height)) return GF_OK;
	width = MAX((u32) wrap->surface->w, width);
	height = MAX((u32) wrap->surface->h, height);
	if (wrap->surface) SDL_FreeSurface(wrap->surface);
	wrap->surface = SDLVid_CreateSDLSurface(ctx, width, height, wrap->pixel_format);
	return wrap->surface ? GF_OK : GF_IO_ERR;
}

GF_Err SDLVid_Blit(GF_VideoOutput *dr, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst)
{
	SDLWrapSurface *wrap;
	SDL_Rect src_rc, dst_rc;
	u32 dst_bpp, src_bpp, dst_w, dst_h, src_w, src_h;
	SDL_Surface *src_s, *dst_s;
	SDLVID();

	if (src_id==0) {
		src_s = ctx->back_buffer;
	} else {
		wrap = (SDLWrapSurface *) src_id;
		if (!SDLVid_SurfaceOK(ctx, wrap, 0)) return GF_BAD_PARAM;
		src_s = wrap->surface;
	}
	if (dst_id==(u32)-1) {
		dst_s = ctx->screen;
	} else if (dst_id==0) {
		dst_s = ctx->back_buffer;
	} else {
		wrap = (SDLWrapSurface *) dst_id;
		if (!SDLVid_SurfaceOK(ctx, wrap, 0)) return GF_BAD_PARAM;
		dst_s = wrap->surface;
	}

	dst_w = dst ? dst->w : dst_s->w;
	dst_h = dst ? dst->h : dst_s->h;
	src_w = src ? src->w : src_s->w;
	src_h = src ? src->h : src_s->h;


	/*fast blit*/
	if ((dst_w==src_w) && (dst_h==src_h)) {
		if (src) { src_rc.x = src->x; src_rc.y = src->y; src_rc.w = src->w; src_rc.h = src->h; }
		if (dst) {
			dst_rc.x = dst->x; dst_rc.y = dst->y; dst_rc.w = dst->w; dst_rc.h = dst->h; 
			SDL_SetClipRect(dst_s, &dst_rc);
		}
		SDL_BlitSurface(src_s, src ? &src_rc : NULL, dst_s, dst ? &dst_rc : NULL);
	}
	/*soft stretch*/
	else {
		char *src_pix, *dst_pix;
		src_w = src ? src->w : src_s->w;
		src_h = src ? src->h : src_s->h;
		dst_w = dst ? dst->w : dst_s->w;
		dst_h = dst ? dst->h : dst_s->h;
		src_bpp = src_s->format->BitsPerPixel;
		dst_bpp = dst_s->format->BitsPerPixel;

		/*safety check, something went wrong during resize*/
		if (dst) {
		  if (dst->x + dst->w > dst_s->w) return GF_OK;
		  if (dst->y + dst_h > dst_s->h) return GF_OK; 
		}
		
		SDL_LockSurface(dst_s);
		SDL_LockSurface(src_s);

		src_pix = src_s->pixels;
		if (src) src_pix += src->y*src_s->pitch + src->x * (src_bpp/8);
		dst_pix = dst_s->pixels;
		if (dst) dst_pix += dst->y*dst_s->pitch + dst->x * (dst_bpp/8);
		
		if ((src_bpp==16) && (SDLVid_MapPixelFormat(src_s->format)==GF_PIXEL_RGB_555)) src_bpp = 15;
		if ((dst_bpp==16) && (SDLVid_MapPixelFormat(dst_s->format)==GF_PIXEL_RGB_555)) dst_bpp = 15;

		StretchBits(dst_pix, dst_bpp, dst_w, dst_h, dst_s->pitch, 
				src_pix, src_bpp, src_w, src_h, src_s->pitch, 
				0);

		SDL_UnlockSurface(dst_s);
		SDL_UnlockSurface(src_s);
	}

	return GF_OK;
}

static GF_Err SDLVid_GetPixelFormat(GF_VideoOutput *dr, u32 surface_id, u32 *pixel_format)
{
	SDL_Surface *surf;
	SDLVID();

	if (!surface_id) {
		surf = ctx->screen;
	} else {
		SDLWrapSurface *wrap = (SDLWrapSurface *) surface_id;
		if (!SDLVid_SurfaceOK(ctx, wrap, 0)) return GF_BAD_PARAM;
		surf = wrap->surface;
	}
	*pixel_format = SDLVid_MapPixelFormat(surf->format);
	return GF_OK;
}

void SDL_SetupVideo2D(GF_VideoOutput *driv)
{
	/*alpha and keying to do*/
	driv->bHasAlpha = 0;
	driv->bHasKeying = 0;
	/*no YUV hardware blitting in SDL (only overlays)*/
	driv->bHasYUV = 0;

	driv->Blit = SDLVid_Blit;
	driv->Clear = SDLVid_Clear;
	driv->GetPixelFormat = SDLVid_GetPixelFormat;
	driv->CreateSurface = SDLVid_CreateSurface;
	driv->DeleteSurface = SDLVid_DeleteSurface;
	driv->LockSurface = SDLVid_LockSurface;
	driv->UnlockSurface = SDLVid_UnlockSurface;
	driv->ResizeSurface	= SDLVid_ResizeSurface;
	driv->IsSurfaceValid = SDLVid_IsSurfaceValid;

	/*
	driv->BlitKey = SDLVid_BltKey;
	driv->BlitAlpha = SDLVid_BlitAlpha;
	driv->YV12_to_RGB = SDLVid_YV12_to_RGB;
	*/

}
