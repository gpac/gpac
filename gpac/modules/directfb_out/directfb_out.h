/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Romain Bouqueau - Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / DirectFB video output module
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

#ifndef _DIRECTFB_OUT_H_
#define _DIRECTFB_OUT_H_

typedef struct __DirectFBVidCtx DirectFBVidCtx;

typedef enum {
	WINDOW_X11 = 0,
	WINDOW_SDL,
	WINDOW_VNC,
	WINDOW_OSX,
	WINDOW_FBDEV,
	WINDOW_DEVMEM,
} WINDOW_MODE;

typedef enum {
	FLIP_SWAP	 = 1,
	FLIP_WAITFORSYNC = 1 << 1,
	FLIP_WAIT	 = 1 << 2,
	FLIP_ONSYNC	 = 1 << 3,
} FLIP_MODE;

#ifndef Bool
#define Bool	u32
#endif

u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf);
u32 DirectFBVid_TranslatePixelFormatFromGPAC(u32 gpacpf);
size_t DirectFBVid_GetCtxSizeOf(void);
void DirectFBVid_InitAndCreateSurface(DirectFBVidCtx *ctx, char *dfb_system);
void DirectFBVid_CtxSetFlipMode(DirectFBVidCtx *ctx, FLIP_MODE flip_mode);
void DirectFBVid_CtxPrimaryProcessGetAccelerationMask(DirectFBVidCtx *ctx);
u32 DirectFBVid_ProcessMessageQueueWrapper(DirectFBVidCtx *ctx, u8 *type, u32 *flags, u32 *key_code, s32 *x, s32 *y, u32 *button);
void DirectFBVid_DrawHLineWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 length, u8 r, u8 g, u8 b);
void DirectFBVid_DrawHLineAlphaWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 length, u8 r, u8 g, u8 b, u8 alpha);
void DirectFBVid_DrawRectangleWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 width, u32 height, u8 r, u8 g, u8 b, u8 a);
u32 DirectFBVid_CtxPrimaryLock(DirectFBVidCtx *ctx, void **buf, u32 *pitch);
void DirectFBVid_CtxPrimaryUnlock(DirectFBVidCtx *ctx);
u32 DirectFBVid_CtxGetWidth(DirectFBVidCtx *ctx);
u32 DirectFBVid_CtxGetHeight(DirectFBVidCtx *ctx);
void *DirectFBVid_CtxGetPrimary(DirectFBVidCtx *ctx);
u32 DirectFBVid_CtxGetPixelFormat(DirectFBVidCtx *ctx);
Bool DirectFBVid_CtxIsHwMemory(DirectFBVidCtx *ctx);
u32 DirectFBVid_CtxPrimaryFlip(DirectFBVidCtx *ctx);
void DirectFBVid_CtxSetIsInit(DirectFBVidCtx *ctx, Bool val);
u32 DirectFBVid_ShutdownWrapper(DirectFBVidCtx *ctx);
u32 DirectFBVid_BlitWrapper(DirectFBVidCtx *ctx, u32 video_src_width, u32 video_src_height, u32 video_src_pixel_format, char *video_src_buffer, s32 video_src_pitch_y, u32 src_wnd_x, u32 src_wnd_y, u32 src_wnd_w, u32 src_wnd_h, u32 dst_wnd_x, u32 dst_wnd_y, u32 dst_wnd_w, u32 dst_wnd_h, u32 overlay_type);


#endif
