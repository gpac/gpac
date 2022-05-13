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


/* DirectFB */
#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>
#include <direct/util.h>

#include <gpac/constants.h>
#include <gpac/events_constants.h>
#include <gpac/events.h>

#include "directfb_out.h"

static int do_xor = 0;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                     \
          do {                                                             \
               err = x;                                                    \
               if (err != DFB_OK) {                                        \
                    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
                    DirectFBErrorFatal( #x, err );                         \
               }                                                           \
          } while (0)

#define SET_DRAWING_FLAGS( flags ) \
          ctx->primary->SetDrawingFlags( ctx->primary, (flags) | (do_xor ? DSDRAW_XOR : 0) )


/* Structs */

struct __DirectFBVidCtx
{
	/* the super interface */
	IDirectFB *dfb;
	/* the primary surface */
	IDirectFBSurface *primary;

	/* for keyboard input */
	//~ IDirectFBInputDevice *keyboard;

	/* screen width, height */
	u32 width, height, pixel_format;
	Bool use_systems_memory, disable_aa, is_init;

	/* acceleration */
	int accel_drawline, accel_fillrect;
	DFBSurfaceFlipFlags flip_mode;

	/*===== for key events =====*/

	/* Input interfaces: devices and its buffer */
	IDirectFBEventBuffer *events;

	/* mouse events */
	IDirectFBInputDevice *mouse;

	/*=============================
	if using window
	   ============================= */

	/* DirectFB window */
	IDirectFBWindow *window;

	/* display layer */
	IDirectFBDisplayLayer *layer;

};


size_t DirectFBVid_GetCtxSizeOf(void)
{
	return sizeof(DirectFBVidCtx);
}


typedef struct _DeviceInfo DeviceInfo;
struct _DeviceInfo {
	DFBInputDeviceID           device_id;
	DFBInputDeviceDescription  desc;
	DeviceInfo                *next;
};



/* Wrapper to DirectFB members */

/**
 *	function DirectFBVid_DrawHLineWrapper
 * 	- using hardware accelerator to draw horizontal line
 **/
void DirectFBVid_DrawHLineWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 length, u8 r, u8 g, u8 b)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawHLine(). Drawing line x %d y %d length %d color %08X\n", x, y, length, color));

	// DSDRAW_NOFX: flag controlling drawing command, drawing without using any effects
	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	// set the color used without alpha
	ctx->primary->SetColor(ctx->primary, r, g, b, 0xFF);

	// to draw a line using hardware accelerators, we can use either DrawLine (in our STB, DrawLine() function is not accelerated) or FillRectangle function with height equals to 1
	//ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);	// no acceleration
	ctx->primary->FillRectangle(ctx->primary, x, y,length, 1);
}


/**
 *	function DirectFBVid_DrawHLineWrapper
 * 	- using hardware accelerator to draw horizontal line with alpha
 **/
void DirectFBVid_DrawHLineAlphaWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 length, u8 r, u8 g, u8 b, u8 alpha)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawHLineAlpha(). Alpha line drawing x %d y %d length %d color %08X alpha %d\n", x, y, length, color, alpha));

	// DSDRAW_BLEND: using alpha from color
	SET_DRAWING_FLAGS( DSDRAW_BLEND );

	ctx->primary->SetColor(ctx->primary, r, g, b, alpha);
	//ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);
	ctx->primary->FillRectangle(ctx->primary, x, y, length, 1);	// with acceleration
}


/**
 *	function DirectFBVid_DrawRectangleWrapper
 *	- using hardware accelerator to fill a rectangle with the given color
 **/
void DirectFBVid_DrawRectangleWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 width, u32 height, u8 r, u8 g, u8 b, u8 a)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawRectangle(). Drawing rectangle x %d y %d width %d height %d color %08x\n", x, y, width, height, color));

	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	ctx->primary->SetColor(ctx->primary, r, g, b, a);
	ctx->primary->FillRectangle(ctx->primary, x, y, width, height);
	//ctx->primary->Blit( ctx->primary, ctx->primary, NULL, x, y );
}


/**
 *	function DirectFBVid_CtxPrimaryLock
 * 	- lock the surface (in order to access certain data)
 **/
u32 DirectFBVid_CtxPrimaryLock(DirectFBVidCtx *ctx, void **buf, u32 *pitch)
{
	DFBResult ret = ctx->primary->Lock(ctx->primary, DSLF_READ | DSLF_WRITE, buf, pitch);
	if (ret != DFB_OK) return 1;
	return 0;
}


static DFBEnumerationResult enum_input_device(DFBInputDeviceID device_id, DFBInputDeviceDescription desc, void *data )
{
	DeviceInfo **devices = data;

	DeviceInfo *device = malloc(sizeof(DeviceInfo) );

	device->device_id = device_id;
	device->desc = desc;
	device->next = *devices;

	*devices = device;

	return 0;
}


/**
 *	function DirectFBVid_InitAndCreateSurface
 * 	- initialize and create DirectFB surface
 **/
u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf);
void DirectFBVid_InitAndCreateSurface(DirectFBVidCtx *ctx, char *dfb_system)
{
	DFBResult err;
	DFBSurfaceDescription dsc;
	DFBSurfacePixelFormat dfbpf;
	DeviceInfo *devices = NULL;

	//fake arguments and DirectFBInit()
	{
		int i, argc=2, argc_ro=2;
		char **argv = malloc(argc*sizeof(char*));
		char *argv_ro[2];
		//http://directfb.org/wiki/index.php/Configuring_DirectFB
		argv_ro[0]=argv[0]=strdup("gpac");
		char dev_name[100];
		snprintf(dev_name, "--dfb:system=%s", dfb_system ? dfb_system : "x11");
		dev_name[99]=0;
		strlwr(dev_name);
		argv_ro[1] = argv[1] = strdup(dev_name);

		// screen resolution 640x480
		//~ argv_ro[2]=argv[2]=strdup("--dfb:mode=640x480");
		//~ argv_ro[2]=argv[2]=strdup("");

		/* create the super interface */
		DFBCHECK(DirectFBInit(&argc, &argv));

		for (i=0; i<argc_ro; i++)
			free(argv_ro[i]);
		free(argv);
	}

	// disable background handling
	DFBCHECK(DirectFBSetOption ("bg-none", NULL));
	// disable layers
	DFBCHECK(DirectFBSetOption ("no-init-layer", NULL));

	/* create the surface */
	DFBCHECK(DirectFBCreate( &(ctx->dfb) ));

	/* create a list of input devices */
	ctx->dfb->EnumInputDevices(ctx->dfb, enum_input_device, &devices );
	if (devices->desc.type & DIDTF_JOYSTICK) {
		// for mouse
		DFBCHECK(ctx->dfb->GetInputDevice(ctx->dfb, devices->device_id, &(ctx->mouse)));
	}

	/* create an event buffer for all devices */
	DFBCHECK(ctx->dfb->CreateInputEventBuffer(ctx->dfb, DICAPS_KEYS, DFB_FALSE, &(ctx->events) ));

	/* Set the cooperative level */
	DFBCHECK(ctx->dfb->SetCooperativeLevel( ctx->dfb, DFSCL_FULLSCREEN ));

	/* Get the primary surface, i.e. the surface of the primary layer. */
	// capabilities field is valid
	dsc.flags = DSDESC_CAPS;
	// primary, double-buffered surface
	dsc.caps = DSCAPS_PRIMARY | DSCAPS_DOUBLE;

	// if using system memory, data is permanently stored in this memory (no video memory allocation)
	if (ctx->use_systems_memory) dsc.caps |= DSCAPS_SYSTEMONLY;

	DFBCHECK(ctx->dfb->CreateSurface( ctx->dfb, &dsc, &(ctx->primary) ));

	// fetch pixel format
	ctx->primary->GetPixelFormat( ctx->primary, &dfbpf );
	// translate DirectFB pixel format to GPAC
	ctx->pixel_format = DirectFBVid_TranslatePixelFormatToGPAC(dfbpf);
	// surface width and height in pixel
	ctx->primary->GetSize( ctx->primary, &(ctx->width), &(ctx->height) );
	ctx->primary->Clear( ctx->primary, 0, 0, 0, 0xFF);
}


/**
 *	function DirectFBVid_CtxPrimaryUnlock
 * 	- unlock a surface after direct access (in order to fetch data)
 **/
void DirectFBVid_CtxPrimaryUnlock(DirectFBVidCtx *ctx)
{
	ctx->primary->Unlock(ctx->primary);
}


/**
 *	function DirectFBVid_CtxGetWidth
 * 	- returns screen width
 **/
u32 DirectFBVid_CtxGetWidth(DirectFBVidCtx *ctx)
{
	return ctx->width;
}


/**
 *	function DirectFBVid_CtxGetHeight
 * 	- returns screen height
 **/
u32 DirectFBVid_CtxGetHeight(DirectFBVidCtx *ctx)
{
	return ctx->height;
}


/**
 *	function DirectFBVid_CtxGetPrimary
 * 	- return the primary surface
 **/
void *DirectFBVid_CtxGetPrimary(DirectFBVidCtx *ctx)
{
	return ctx->primary;
}


/**
 *	function DirectFBVid_CtxGetPixelFormat
 * 	- get pixel format
 **/
u32 DirectFBVid_CtxGetPixelFormat(DirectFBVidCtx *ctx)
{
	return ctx->pixel_format;
}


/**
 *	function DirectFBVid_CtxIsHwMemory
 * 	- return value whether system memory is used or not
 **/
Bool DirectFBVid_CtxIsHwMemory(DirectFBVidCtx *ctx)
{
	return ctx->use_systems_memory;
}


/**
 *	function DirectFBVid_CtxPrimaryFlip
 * 	- flipping buffers
 **/
u32 DirectFBVid_CtxPrimaryFlip(DirectFBVidCtx *ctx)
{
	return ctx->primary->Flip(ctx->primary, NULL, ctx->flip_mode);
}


/**
 *	function DirectFBVid_CtxSetIsInit
 * 	- boolean showing whether DirectFB is initialized
 **/
void DirectFBVid_CtxSetIsInit(DirectFBVidCtx *ctx, Bool val)
{
	ctx->is_init = val;
}


/**
 *	function DirectFBVid_CtxSetFlipMode
 * 	- set flip mode
 **/
void DirectFBVid_CtxSetFlipMode(DirectFBVidCtx *ctx, u32 flip_mode)
{
	ctx->flip_mode = DSFLIP_BLIT;
	switch(flip_mode) {
	case FLIP_WAITFORSYNC:
		ctx->flip_mode |= DSFLIP_WAITFORSYNC;
		break;
	case FLIP_WAIT:
		ctx->flip_mode |= DSFLIP_WAIT;
		break;
	case FLIP_ONSYNC:
		ctx->flip_mode |= DSFLIP_ONSYNC;
		break;
	case FLIP_SWAP:
		ctx->flip_mode &= ~DSFLIP_BLIT;
		break;
	}
}


/**
 * 	function DirectFBVid_CtxPrimaryProcessGetAccelerationMask
 *	- Get a mask of drawing functions that are hardware accelerated with the current settings.
 **/
void DirectFBVid_CtxPrimaryProcessGetAccelerationMask(DirectFBVidCtx *ctx)
{
	DFBAccelerationMask mask;
	ctx->primary->GetAccelerationMask( ctx->primary, NULL, &mask );

	if (mask & DFXL_DRAWLINE ) // DrawLine() is accelerated. DFXL_DRAWLINE
		ctx->accel_drawline = 1;
	if (mask & DFXL_FILLRECTANGLE) // FillRectangle() is accelerated.
		ctx->accel_fillrect = 1;
}


/**
 *	function DirectFBVid_ShutdownWrapper
 * 	- shutdown DirectFB module
 **/
u32 DirectFBVid_ShutdownWrapper(DirectFBVidCtx *ctx)
{
	// case where initialization is not done
	if (!ctx->is_init) return 1;

	//~ ctx->keyboard->Release( ctx->keyboard );
	ctx->primary->Release( ctx->primary );
	ctx->events->Release( ctx->events );
	ctx->dfb->Release( ctx->dfb );
	ctx->is_init = 0;
	//we use x11 thus it should be useless:
	//system("stgfb_control /dev/fb0 a 0")
	return 0;
}


/**
 *	Blit a surface
 **/
u32 DirectFBVid_TranslatePixelFormatFromGPAC(u32 gpacpf);
u32 DirectFBVid_BlitWrapper(DirectFBVidCtx *ctx, u32 video_src_width, u32 video_src_height, u32 video_src_pixel_format, char *video_src_buffer, s32 video_src_pitch_y, u32 src_wnd_x, u32 src_wnd_y, u32 src_wnd_w, u32 src_wnd_h, u32 dst_wnd_x, u32 dst_wnd_y, u32 dst_wnd_w, u32 dst_wnd_h, u32 overlay_type)
{
	DFBResult res;
	DFBSurfaceDescription srcdesc;
	IDirectFBSurface *src;
	DFBRectangle dfbsrc, dfbdst;

	if (overlay_type != 0) return 1;

	memset(&srcdesc, 0, sizeof(srcdesc));

	srcdesc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_PREALLOCATED;
	srcdesc.width = video_src_width;
	srcdesc.height = video_src_height;
	srcdesc.pixelformat = DirectFBVid_TranslatePixelFormatFromGPAC(video_src_pixel_format);
	srcdesc.preallocated[0].data = video_src_buffer;
	srcdesc.preallocated[0].pitch = video_src_pitch_y;


	switch (video_src_pixel_format) {
	case GF_PIXEL_ARGB: //return DSPF_ARGB;
	case GF_PIXEL_RGBA: //return DSPF_ARGB;
		ctx->primary->SetBlittingFlags(ctx->primary, DSBLIT_BLEND_ALPHACHANNEL);
		break;
	default:
		ctx->primary->SetBlittingFlags(ctx->primary, DSBLIT_NOFX);
	}

	// create a surface with the new surface description
	res = ctx->dfb->CreateSurface(ctx->dfb, &srcdesc, &src);
	if (res != DFB_OK) {
		//GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] cannot create blit source surface for pixel format %s: %s (%d)\n", gf_4cc_to_str(video_src->pixel_format), DirectFBErrorString(res), res));
		return 1;
	}


	dfbsrc.x = src_wnd_x;
	dfbsrc.y = src_wnd_y;
	dfbsrc.w = src_wnd_w;
	dfbsrc.h = src_wnd_h;

	// blit on the surface
	if (!src_wnd_x && !src_wnd_y && (dst_wnd_w==src_wnd_w) && (dst_wnd_h==src_wnd_h)) {
		ctx->primary->Blit(ctx->primary, src, &dfbsrc, dst_wnd_x, dst_wnd_y);
		// blit an area scaled from the source to the destination rectangle
	} else {
		dfbdst.x = dst_wnd_x;
		dfbdst.y = dst_wnd_y;
		dfbdst.w = dst_wnd_w;
		dfbdst.h = dst_wnd_h;
		ctx->primary->StretchBlit(ctx->primary, src, &dfbsrc, &dfbdst);
	}

	src->Release(src);

	return 0;
}


/**
 *	function DirectFBVid_ProcessMessageQueueWrapper
 * 	- handle DirectFB events
 * 	- key events
 **/
void directfb_translate_key(DFBInputDeviceKeyIdentifier DirectFBkey, u32 *flags, u32 *key_code);
u32 DirectFBVid_ProcessMessageQueueWrapper(DirectFBVidCtx *ctx, u8 *type, u32 *flags, u32 *key_code, s32 *x, s32 *y, u32 *button)
{
	DFBInputEvent directfb_evt;

	if (ctx->events->GetEvent( ctx->events, DFB_EVENT(&directfb_evt) ) == DFB_OK)
	{
		switch (directfb_evt.type) {
		case DIET_KEYPRESS:
		case DIET_KEYRELEASE:
			directfb_translate_key(directfb_evt.key_id, flags, key_code);
			*type = (directfb_evt.type == DIET_KEYPRESS) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			break;
		case DIET_BUTTONPRESS:
		case DIET_BUTTONRELEASE:
			*type = (directfb_evt.type == DIET_BUTTONPRESS) ? GF_EVENT_MOUSEUP : GF_EVENT_MOUSEDOWN;
			switch(directfb_evt.button) {
			case DIBI_LEFT:
				*button = GF_MOUSE_LEFT;
				break;
			case DIBI_RIGHT:
				*button = GF_MOUSE_RIGHT;
				break;
			case DIBI_MIDDLE:
				*button = GF_MOUSE_MIDDLE;
				break;
			default:
				//fprintf(stderr, "in here for others\n");
				break;
			}
			break;
		case DIET_AXISMOTION:
			*type = GF_EVENT_MOUSEMOVE;
			ctx->mouse->GetXY(ctx->mouse, x, y);
		default:
			break;
		}

		return 0;
	}

	return 1;
}


/* Events translation */
/**
 *	function DirectFBVid_TranslatePixelFormatToGPAC
 * 	- translate pixel from DirectFb to GPAC
 **/
u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf)
{
	switch (dfbpf) {
	case DSPF_RGB16:
		return GF_PIXEL_RGB_565;
	case DSPF_RGB555:
		return GF_PIXEL_RGB_555;
	case DSPF_RGB24:
		return GF_PIXEL_RGB;
	case DSPF_RGB32:
		return GF_PIXEL_RGBX;
	case DSPF_ARGB:
		return GF_PIXEL_ARGB;
	default:
		return 0;
	}
}

/**
 *	function DirectFBVid_TranslatePixelFormatToGPAC
 * 	- translate pixel from GPAC to DirectFB
 **/
u32 DirectFBVid_TranslatePixelFormatFromGPAC(u32 gpacpf)
{
	switch (gpacpf) {
	case GF_PIXEL_RGB_565:
		return DSPF_RGB16;
	case GF_PIXEL_RGB_555 :
		return DSPF_RGB555;
	case GF_PIXEL_BGR:
		return DSPF_RGB24;
	case GF_PIXEL_RGB:
		return DSPF_RGB24;
	case GF_PIXEL_RGBX:
		return DSPF_RGB32;
	case GF_PIXEL_ARGB:
		return DSPF_ARGB;
	case GF_PIXEL_RGBA:
		return DSPF_ARGB;
	case GF_PIXEL_UYVY :
		return DSPF_YUY2;
	case GF_PIXEL_YUV:
		return DSPF_YV12;
	default:
		;//GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] pixel format %s not supported\n", gf_4cc_to_str(gpacpf)));
	}

	return 0;
}


/**
 *	function directfb_translate_key
 * 	- translate key from DirectFB to GPAC
 **/
void directfb_translate_key(DFBInputDeviceKeyIdentifier DirectFBkey, u32 *flags, u32 *key_code)
{
	//~ fprintf(stderr, "DirectFBkey=%d\n", DirectFBkey);

	switch (DirectFBkey) {
	case DIKI_BACKSPACE:
		*key_code = GF_KEY_BACKSPACE;
		//~ fprintf(stderr, "DIKI_BACKSPACE\n");
		break;
	case DIKI_TAB:
		*key_code = GF_KEY_TAB;
		//~ fprintf(stderr, "DIKI_TAB\n");
		break;
	case DIKI_ENTER:
		*key_code = GF_KEY_ENTER;
		//~ fprintf(stderr, "DIKI_ENTER\n");
		break;
	case DIKI_ESCAPE:
		*key_code = GF_KEY_ESCAPE;
		//~ fprintf(stderr, "DIKI_ESCAPE\n");
		break;
	case DIKI_SPACE:
		*key_code = GF_KEY_SPACE;
		//~ fprintf(stderr, "DIKI_SPACE\n");
		break;
	case DIKI_SHIFT_L:
	case DIKI_SHIFT_R:
		*key_code = GF_KEY_SHIFT;
		//~ fprintf(stderr, "DIKI_SHIFT_R/DIKI_SHIFT_L\n");
		break;
	case DIKI_CONTROL_L:
	case DIKI_CONTROL_R:
		*key_code = GF_KEY_CONTROL;
		//~ fprintf(stderr, "DIKI_CONTROL_L/DIKI_CONTROL_R\n");
		break;
	case DIKI_ALT_L:
	case DIKI_ALT_R:
		*key_code = GF_KEY_ALT;
		//~ fprintf(stderr, "DIKI_ALT_L/DIKI_ALT_R\n");
		break;
	case DIKI_CAPS_LOCK:
		*key_code = GF_KEY_CAPSLOCK;
		//~ fprintf(stderr, "DIKI_CAPS_LOCK\n");
		break;
	case DIKI_META_L:
	case DIKI_META_R:
		*key_code = GF_KEY_META;
		break;
	case DIKI_KP_EQUAL:
		*key_code = GF_KEY_EQUALS;
		break;
	case DIKI_SUPER_L:
	case DIKI_SUPER_R:
		*key_code = GF_KEY_WIN;
		break;

	/* alphabets */
	case DIKI_A:
		*key_code = GF_KEY_A;
		//~ fprintf(stderr, "DIKI_A\n");
		break;
	case DIKI_B:
		*key_code = GF_KEY_B;
		//~ fprintf(stderr, "DIKI_B\n");
		break;
	case DIKI_C:
		*key_code = GF_KEY_C;
		//~ fprintf(stderr, "DIKI_C\n");
		break;
	case DIKI_D:
		*key_code = GF_KEY_D;
		//~ fprintf(stderr, "DIKI_D\n");
		break;
	case DIKI_E:
		*key_code = GF_KEY_E;
		//~ fprintf(stderr, "DIKI_E\n");
		break;
	case DIKI_F:
		*key_code = GF_KEY_F;
		break;
	case DIKI_G:
		*key_code = GF_KEY_G;
		break;
	case DIKI_H:
		*key_code = GF_KEY_H;
		break;
	case DIKI_I:
		*key_code = GF_KEY_I;
		break;
	case DIKI_J:
		*key_code = GF_KEY_J;
		break;
	case DIKI_K:
		*key_code = GF_KEY_K;
		break;
	case DIKI_L:
		*key_code = GF_KEY_L;
		break;
	case DIKI_M:
		*key_code = GF_KEY_M;
		break;
	case DIKI_N:
		*key_code = GF_KEY_N;
		break;
	case DIKI_O:
		*key_code = GF_KEY_O;
		break;
	case DIKI_P:
		*key_code = GF_KEY_P;
		break;
	case DIKI_Q:
		*key_code = GF_KEY_Q;
		break;
	case DIKI_R:
		*key_code = GF_KEY_R;
		break;
	case DIKI_S:
		*key_code = GF_KEY_S;
		break;
	case DIKI_T:
		*key_code = GF_KEY_T;
		break;
	case DIKI_U:
		*key_code = GF_KEY_U;
		break;
	case DIKI_V:
		*key_code = GF_KEY_V;
		break;
	case DIKI_W:
		*key_code = GF_KEY_W;
		break;
	case DIKI_X:
		*key_code = GF_KEY_X;
		break;
	case DIKI_Y:
		*key_code = GF_KEY_Y;
		break;
	case DIKI_Z:
		*key_code = GF_KEY_Z;
		break;

	case DIKI_PRINT:
		*key_code = GF_KEY_PRINTSCREEN;
		break;
	case DIKI_SCROLL_LOCK:
		*key_code = GF_KEY_SCROLL;
		break;
	case DIKI_PAUSE:
		*key_code = GF_KEY_PAUSE;
		break;
	case DIKI_INSERT:
		*key_code = GF_KEY_INSERT;
		break;
	case DIKI_DELETE:
		*key_code = GF_KEY_DEL;
		break;
	case DIKI_HOME:
		*key_code = GF_KEY_HOME;
		break;
	case DIKI_END:
		*key_code = GF_KEY_END;
		break;
	case DIKI_PAGE_UP:
		*key_code = GF_KEY_PAGEUP;
		break;
	case DIKI_PAGE_DOWN:
		*key_code = GF_KEY_PAGEDOWN;
		break;

	/* arrows */
	case DIKI_UP:
		*key_code = GF_KEY_UP;
		break;
	case DIKI_DOWN:
		*key_code = GF_KEY_DOWN;
		break;
	case DIKI_RIGHT:
		*key_code = GF_KEY_RIGHT;
		break;
	case DIKI_LEFT:
		*key_code = GF_KEY_LEFT;
		break;

	/* extended numerical pad */
	case DIKI_NUM_LOCK:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_NUMLOCK;
		break;
	case DIKI_KP_DIV:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_SLASH;
		break;
	case DIKI_KP_MULT:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_STAR;
		break;
	case DIKI_KP_MINUS:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_HYPHEN;
		break;
	case DIKI_KP_PLUS:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_PLUS;
		break;
	case DIKI_KP_ENTER:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_ENTER;
		break;
	case DIKI_KP_DECIMAL:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_FULLSTOP;
		break;
	case DIKI_KP_0:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_0;
		break;
	case DIKI_KP_1:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_1;
		break;
	case DIKI_KP_2:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_2;
		break;
	case DIKI_KP_3:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_3;
		break;
	case DIKI_KP_4:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_4;
		break;
	case DIKI_KP_5:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_5;
		break;
	case DIKI_KP_6:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_6;
		break;
	case DIKI_KP_7:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_7;
		break;
	case DIKI_KP_8:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_8;
		break;
	case DIKI_KP_9:
		*flags = GF_KEY_EXT_NUMPAD;
		*key_code = GF_KEY_9;
		break;

	/* Fn functions */
	case DIKI_F1:
		*key_code = GF_KEY_F1;
		break;
	case DIKI_F2:
		*key_code = GF_KEY_F2;
		break;
	case DIKI_F3:
		*key_code = GF_KEY_F3;
		break;
	case DIKI_F4:
		*key_code = GF_KEY_F4;
		break;
	case DIKI_F5:
		*key_code = GF_KEY_F5;
		break;
	case DIKI_F6:
		*key_code = GF_KEY_F6;
		break;
	case DIKI_F7:
		*key_code = GF_KEY_F7;
		break;
	case DIKI_F8:
		*key_code = GF_KEY_F8;
		break;
	case DIKI_F9:
		*key_code = GF_KEY_F9;
		break;
	case DIKI_F10:
		*key_code = GF_KEY_F10;
		break;
	case DIKI_F11:
		*key_code = GF_KEY_F11;
		break;
	case DIKI_F12:
		*key_code = GF_KEY_F12;
		break;

	default:
		*key_code = GF_KEY_UNIDENTIFIED;
		break;
	}
}

