/*
 *					GPAC Multimedia Framework
 *
 *				Copyright (c) 2005-20XX Telecom-Paristech
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

    /* screen width, height */
    u32 width, height, pixel_format;
    Bool use_systems_memory, disable_acceleration, disable_aa, is_init, disable_display;

     /* acceleration */
    int accel_drawline, accel_fillrect;
    DFBSurfaceFlipFlags flip_mode;

    /*===== for key events =====*/

    /* Input interfaces: devices and its buffer */
    IDirectFBEventBuffer *events;

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

void DirectFBVid_DrawHLineWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 length, u8 r, u8 g, u8 b)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawHLine(). Drawing line x %d y %d length %d color %08X\n", x, y, length, color));
	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	ctx->primary->SetColor(ctx->primary, r, g, b, 0xFF); // no alpha
	//ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);	// no acceleration
	ctx->primary->FillRectangle(ctx->primary, x, y,length, 1);
}


void DirectFBVid_DrawHLineAlphaWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 length, u8 r, u8 g, u8 b, u8 alpha)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawHLineAlpha(). Alpha line drawing x %d y %d length %d color %08X alpha %d\n", x, y, length, color, alpha));

	SET_DRAWING_FLAGS( DSDRAW_BLEND ); // use alpha

	ctx->primary->SetColor(ctx->primary, r, g, b, alpha);
	//ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);
	ctx->primary->FillRectangle(ctx->primary, x, y, length, 1);	// with acceleration
}


void DirectFBVid_DrawRectangleWrapper(DirectFBVidCtx *ctx, u32 x, u32 y, u32 width, u32 height, u8 r, u8 g, u8 b, u8 a)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawRectangle(). Drawing rectangle x %d y %d width %d height %d color %08x\n", x, y, width, height, color));

	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	ctx->primary->SetColor(ctx->primary, r, g, b, a);
	ctx->primary->FillRectangle(ctx->primary, x, y, width, height);
	//ctx->primary->Blit( ctx->primary, ctx->primary, NULL, x, y );
}


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


u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf);
void DirectFBVid_InitAndCreateSurface(DirectFBVidCtx *ctx)
{
	DFBResult err;
	DFBSurfaceDescription dsc;
	DFBSurfacePixelFormat dfbpf;
	DeviceInfo *devices = NULL;
	
	//fake arguments and DirectFBInit()
	{
		int i, argc=3, argc_ro=3;
		char **argv = malloc(argc*sizeof(char*));
		char *argv_ro[3];
		//http://directfb.org/wiki/index.php/Configuring_DirectFB
		argv_ro[0]=argv[0]=strdup("gpac");
		argv_ro[1]=argv[1]=strdup("--dfb:system=x11");
		argv_ro[2]=argv[2]=strdup("--dfb:mode=640x480");

		/* create the super interface */
		DFBCHECK(DirectFBInit(&argc, &argv));
	
		for (i=0; i<argc_ro; i++)
			free(argv_ro[i]);
		free(argv);
	}

	DirectFBSetOption ("bg-none", NULL);
	DirectFBSetOption ("no-init-layer", NULL);

	/* create the surface */
	DFBCHECK(DirectFBCreate( &(ctx->dfb) ));

	/* create a list of input devices */
	ctx->dfb->EnumInputDevices(ctx->dfb, enum_input_device, &devices );

	/* create an event buffer for all devices */
	DFBCHECK(ctx->dfb->CreateInputEventBuffer(ctx->dfb, DICAPS_KEYS, DFB_FALSE, &(ctx->events) ));

	/* Set the cooperative level */
	err = ctx->dfb->SetCooperativeLevel( ctx->dfb, DFSCL_FULLSCREEN );
	if (err)
		DirectFBError( "Failed to set cooperative level", err );

	/* Get the primary surface, i.e. the surface of the primary layer. */
	dsc.flags = DSDESC_CAPS;
	dsc.caps = DSCAPS_PRIMARY | DSCAPS_DOUBLE;

	if (ctx->use_systems_memory) dsc.caps |= DSCAPS_SYSTEMONLY;

	DFBCHECK(ctx->dfb->CreateSurface( ctx->dfb, &dsc, &(ctx->primary) ));

	ctx->primary->GetPixelFormat( ctx->primary, &dfbpf );
	ctx->pixel_format = DirectFBVid_TranslatePixelFormatToGPAC(dfbpf);
	ctx->primary->GetSize( ctx->primary, &(ctx->width), &(ctx->height) );
	ctx->primary->Clear( ctx->primary, 0, 0, 0, 0xFF);
}


void DirectFBVid_CtxPrimaryUnlock(DirectFBVidCtx *ctx)
{
	ctx->primary->Unlock(ctx->primary);
}

u32 DirectFBVid_CtxGetWidth(DirectFBVidCtx *ctx)
{
	return ctx->width;
}

u32 DirectFBVid_CtxGetHeight(DirectFBVidCtx *ctx)
{
	return ctx->height;
}

void *DirectFBVid_CtxGetPrimary(DirectFBVidCtx *ctx)
{
	return ctx->primary;
}

u32 DirectFBVid_CtxGetPixelFormat(DirectFBVidCtx *ctx)
{
	return ctx->pixel_format;
}

Bool DirectFBVid_CtxIsHwMemory(DirectFBVidCtx *ctx)
{
	return ctx->use_systems_memory;
}

u32 DirectFBVid_CtxPrimaryFlip(DirectFBVidCtx *ctx)
{
	return ctx->primary->Flip(ctx->primary, NULL, ctx->flip_mode);
}

void DirectFBVid_CtxSetDisableDisplay(DirectFBVidCtx *ctx, Bool val)
{
	ctx->disable_display = val;
}

Bool DirectFBVid_CtxGetDisableDisplay(DirectFBVidCtx *ctx)
{
	return ctx->disable_display;
}

void DirectFBVid_CtxSetDisableAcceleration(DirectFBVidCtx *ctx, Bool val)
{
	ctx->disable_acceleration = val;
}

Bool DirectFBVid_CtxGetDisableAcceleration(DirectFBVidCtx *ctx)
{
	return ctx->disable_acceleration;
}

void DirectFBVid_CtxSetIsInit(DirectFBVidCtx *ctx, Bool val)
{
	ctx->is_init = val;
}


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


void DirectFBVid_CtxPrimaryProcessGetAccelerationMask(DirectFBVidCtx *ctx)
{
	DFBAccelerationMask mask;
	ctx->primary->GetAccelerationMask( ctx->primary, NULL, &mask );

	if (mask & DFXL_DRAWLINE ) // DrawLine() is accelerated. DFXL_DRAWLINE
		ctx->accel_drawline = 1;
	if (mask & DFXL_FILLRECTANGLE) // FillRectangle() is accelerated.
		ctx->accel_fillrect = 1;
}


u32 DirectFBVid_ShutdownWrapper(DirectFBVidCtx *ctx)
{
	if (!ctx->is_init) return 1;
	//ctx->primary->Clear(ctx->primary,0,0,0,0);
	//ctx->primary->Flip( ctx->primary, NULL, DSFLIP_NONE);
	//ctx->primary->Clear(ctx->primary,0,0,0,0);
	ctx->primary->Release( ctx->primary );
	ctx->events->Release( ctx->events );
	ctx->dfb->Release( ctx->dfb );
	ctx->is_init = 0;
	//we use x11 thus it should be useless:
	//system("stgfb_control /dev/fb0 a 0")
	return 0;
}


u32 DirectFBVid_TranslatePixelFormatFromGPAC(u32 gpacpf);
u32 DirectFBVid_BlitWrapper(DirectFBVidCtx *ctx, u32 video_src_width, u32 video_src_height, u32 video_src_pixel_format, char *video_src_buffer, s32 video_src_pitch_y, u32 src_wnd_x, u32 src_wnd_y, u32 src_wnd_w, u32 src_wnd_h, u32 dst_wnd_x, u32 dst_wnd_y, u32 dst_wnd_w, u32 dst_wnd_h, u32 overlay_type)
{
	DFBResult res;
	DFBSurfaceDescription srcdesc;
	IDirectFBSurface *src;
	DFBRectangle dfbsrc, dfbdst;

	if (overlay_type != 0) return 1;
	if (ctx->disable_display) return 0;

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

	res = ctx->dfb->CreateSurface(ctx->dfb, &srcdesc, &src);
	if (res != DFB_OK) {
		//GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] cannot create blit source surface for pixel format %s: %s (%d)\n", gf_4cc_to_str(video_src->pixel_format), DirectFBErrorString(res), res));
		return 1;
	}


	dfbsrc.x = src_wnd_x;
	dfbsrc.y = src_wnd_y;
	dfbsrc.w = src_wnd_w;
	dfbsrc.h = src_wnd_h;

	if (!src_wnd_x && !src_wnd_y && (dst_wnd_w==src_wnd_w) && (dst_wnd_h==src_wnd_h)) {
		 ctx->primary->Blit(ctx->primary, src, &dfbsrc, dst_wnd_x, dst_wnd_y);
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


void directfb_translate_key(DFBInputDeviceKeySymbol DirectFBkey, u32 *flags, u32 *key_code);
u32 DirectFBVid_ProcessMessageQueueWrapper(DirectFBVidCtx *ctx, u8 *type, u32 *flags, u32 *hw_code)
{
	DFBInputEvent directfb_evt;

	if (ctx->events->GetEvent( ctx->events, DFB_EVENT(&directfb_evt) ) == DFB_OK)
	{
		switch (directfb_evt.type){
			case DIET_KEYPRESS:
			case DIET_KEYRELEASE:
				directfb_translate_key(directfb_evt.key_symbol, flags, hw_code);
				*type = (directfb_evt.type == DIET_KEYPRESS) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			default:
				return 0;
		}
	}

	return 1;
}


/* Events translation */

u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf)
{
	switch (dfbpf) {
		case DSPF_RGB16: return GF_PIXEL_RGB_565;
		case DSPF_RGB555: return GF_PIXEL_RGB_555;
		case DSPF_RGB24: return GF_PIXEL_RGB_24;
		case DSPF_RGB32: return GF_PIXEL_RGB_32;
		case DSPF_ARGB: return GF_PIXEL_ARGB;
		default: return 0;
	}
}


u32 DirectFBVid_TranslatePixelFormatFromGPAC(u32 gpacpf)
{
	switch (gpacpf) {
		case GF_PIXEL_RGB_565: return DSPF_RGB16;
		case GF_PIXEL_RGB_555 : return DSPF_RGB555;
		case GF_PIXEL_BGR_24 : return DSPF_RGB24;
		case GF_PIXEL_RGB_24 : return DSPF_RGB24;
		case GF_PIXEL_RGB_32 : return DSPF_RGB32;
		case GF_PIXEL_ARGB: return DSPF_ARGB;
		case GF_PIXEL_RGBA: return DSPF_ARGB;
		case GF_PIXEL_YUY2 : return DSPF_YUY2;
		case GF_PIXEL_YV12 : return DSPF_YV12;
		default:
			;//GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] pixel format %s not supported\n", gf_4cc_to_str(gpacpf)));
	}

	return 0;
}


void directfb_translate_key(DFBInputDeviceKeySymbol DirectFBkey, u32 *flags, u32 *key_code)
{
	switch (DirectFBkey){
	case DIKS_BACKSPACE:
		*key_code = GF_KEY_BACKSPACE; break;
	case DIKS_RETURN:
		*key_code = GF_KEY_ENTER; break;
	case DIKS_CANCEL:
		*key_code = GF_KEY_CANCEL; break;
	case DIKS_ESCAPE:
		*key_code = GF_KEY_ESCAPE; break;
	case DIKS_SPACE:
		*key_code = GF_KEY_SPACE; break;
	case DIKS_EXCLAMATION_MARK:
		*key_code = GF_KEY_EXCLAMATION; break;
	case DIKS_QUOTATION:
		*key_code = GF_KEY_QUOTATION; break;
	case DIKS_NUMBER_SIGN:
		*key_code = GF_KEY_NUMBER; break;
	case DIKS_DOLLAR_SIGN:
		*key_code = GF_KEY_DOLLAR; break;
#if 0
	case DIKS_PERCENT_SIGN:
		*key_code = GF_KEY_PERCENT; break;
#endif
	case DIKS_AMPERSAND:
		*key_code = GF_KEY_AMPERSAND; break;
	case DIKS_APOSTROPHE:
		*key_code = GF_KEY_APOSTROPHE; break;
	case DIKS_PARENTHESIS_LEFT:
		*key_code = GF_KEY_LEFTPARENTHESIS; break;
	case DIKS_PARENTHESIS_RIGHT:
		*key_code = GF_KEY_RIGHTPARENTHESIS; break;
	case DIKS_ASTERISK:
		*key_code = GF_KEY_STAR; break;
	case DIKS_PLUS_SIGN:
		*key_code = GF_KEY_PLUS; break;
	case DIKS_COMMA:
		*key_code = GF_KEY_COMMA; break;
	case DIKS_MINUS_SIGN:
		*key_code = GF_KEY_HYPHEN; break;
	case DIKS_PERIOD:
		*key_code = GF_KEY_FULLSTOP; break;
	case DIKS_SLASH:
		*key_code = GF_KEY_SLASH; break;
	case DIKS_0:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_0; break;
	case DIKS_1:
		//fprintf(stderr, "DIKS_1: %d\n", GF_KEY_1);
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_1; break;
	case DIKS_2:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_2; break;
	case DIKS_3:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_3; break;
	case DIKS_4:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_4; break;
	case DIKS_5:
		// fprintf(stderr, "DIKS_5\n");
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_5; break;
	case DIKS_6:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_6; break;
	case DIKS_7:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_7; break;
	case DIKS_8:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_8; break;
	case DIKS_9:
		*flags = GF_KEY_EXT_NUMPAD; *key_code = GF_KEY_9; break;
	case DIKS_COLON:
		*key_code = GF_KEY_COLON; break;
	case DIKS_SEMICOLON:
		*key_code = GF_KEY_SEMICOLON; break;
	case DIKS_LESS_THAN_SIGN:
		*key_code = GF_KEY_LESSTHAN; break;
	case DIKS_EQUALS_SIGN:
		*key_code = GF_KEY_EQUALS; break;
	case DIKS_GREATER_THAN_SIGN:
		*key_code = GF_KEY_GREATERTHAN; break;
	case DIKS_QUESTION_MARK:
		*key_code = GF_KEY_QUESTION; break;
	case DIKS_AT:
		*key_code = GF_KEY_AT; break;
	case DIKS_CAPITAL_A:
		*key_code = GF_KEY_A; break;
	case DIKS_CAPITAL_B:
		*key_code = GF_KEY_B; break;
	case DIKS_CAPITAL_C:
		*key_code = GF_KEY_C; break;
	case DIKS_CAPITAL_D:
		*key_code = GF_KEY_D; break;
	case DIKS_CAPITAL_E:
		*key_code = GF_KEY_E; break;
	case DIKS_CAPITAL_F:
		*key_code = GF_KEY_F; break;
	case DIKS_CAPITAL_G:
		*key_code = GF_KEY_G; break;
	case DIKS_CAPITAL_H:
		*key_code = GF_KEY_H; break;
	case DIKS_CAPITAL_I:
		*key_code = GF_KEY_I; break;
	case DIKS_CAPITAL_J:
		*key_code = GF_KEY_J; break;
	case DIKS_CAPITAL_K:
		*key_code = GF_KEY_K; break;
	case DIKS_CAPITAL_L:
		*key_code = GF_KEY_L; break;
	case DIKS_CAPITAL_M:
		*key_code = GF_KEY_M; break;
	case DIKS_CAPITAL_N:
		*key_code = GF_KEY_N; break;
	case DIKS_CAPITAL_O:
		*key_code = GF_KEY_O; break;
	case DIKS_CAPITAL_P:
		*key_code = GF_KEY_P; break;
	case DIKS_CAPITAL_Q:
		*key_code = GF_KEY_Q; break;
	case DIKS_CAPITAL_R:
		*key_code = GF_KEY_R; break;
	case DIKS_CAPITAL_S:
		*key_code = GF_KEY_S; break;
	case DIKS_CAPITAL_T:
		*key_code = GF_KEY_T; break;
	case DIKS_CAPITAL_U:
		*key_code = GF_KEY_U; break;
	case DIKS_CAPITAL_V:
		*key_code = GF_KEY_V; break;
	case DIKS_CAPITAL_W:
		*key_code = GF_KEY_W; break;
	case DIKS_CAPITAL_X:
		*key_code = GF_KEY_X; break;
	case DIKS_CAPITAL_Y:
		*key_code = GF_KEY_Y; break;
	case DIKS_CAPITAL_Z:
		*key_code = GF_KEY_Z; break;
	case DIKS_SQUARE_BRACKET_LEFT:
		*key_code = GF_KEY_LEFTSQUAREBRACKET; break;
	case DIKS_BACKSLASH:
		*key_code = GF_KEY_BACKSLASH; break;
	case DIKS_SQUARE_BRACKET_RIGHT:
		*key_code = GF_KEY_RIGHTSQUAREBRACKET; break;
	case DIKS_CIRCUMFLEX_ACCENT:
		*key_code = GF_KEY_CIRCUM; break;
	case DIKS_UNDERSCORE:
		*key_code = GF_KEY_UNDERSCORE; break;
	case DIKS_GRAVE_ACCENT:
		*key_code = GF_KEY_GRAVEACCENT; break;
	case DIKS_CURLY_BRACKET_LEFT:
		*key_code = GF_KEY_LEFTCURLYBRACKET; break;
	case DIKS_VERTICAL_BAR:
		*key_code = GF_KEY_PIPE; break;
	case DIKS_CURLY_BRACKET_RIGHT:
		*key_code = GF_KEY_RIGHTCURLYBRACKET; break;
	case DIKS_TILDE: break;
	case DIKS_DELETE:
		*key_code = GF_KEY_DEL; break;
	case DIKS_CURSOR_LEFT:
		*key_code = GF_KEY_LEFT; break;
	case DIKS_CURSOR_RIGHT:
		*key_code = GF_KEY_RIGHT; break;
	case DIKS_CURSOR_UP:
		*key_code = GF_KEY_UP; break;
	case DIKS_CURSOR_DOWN:
		*key_code = GF_KEY_DOWN; break;
	case DIKS_INSERT:
		*key_code = GF_KEY_INSERT; break;
	case DIKS_HOME:
		*key_code = GF_KEY_HOME; break;
	case DIKS_END:
		*key_code = GF_KEY_END; break;
	case DIKS_PAGE_UP:
		*key_code = GF_KEY_PAGEUP; break;
	case DIKS_PAGE_DOWN:
		*key_code = GF_KEY_PAGEDOWN; break;
	case DIKS_PRINT:
		*key_code = GF_KEY_PRINTSCREEN; break;
	case DIKS_SELECT:
		*key_code = GF_KEY_SELECT; break;
	case DIKS_CLEAR:
		*key_code = GF_KEY_CLEAR; break;
	case DIKS_HELP:
		*key_code = GF_KEY_HELP; break;
	case DIKS_ZOOM:
		*key_code = GF_KEY_ZOOM; break;
	case DIKS_VOLUME_UP:
		*key_code = GF_KEY_VOLUMEUP; break;
	case DIKS_VOLUME_DOWN:
		*key_code = GF_KEY_VOLUMEDOWN; break;
	case DIKS_MUTE:
		*key_code = GF_KEY_VOLUMEMUTE; break;
	case DIKS_PLAYPAUSE:
	case DIKS_PAUSE:
		*key_code = GF_KEY_MEDIAPLAYPAUSE; break;
	case DIKS_PLAY:
		*key_code = GF_KEY_PLAY; break;
	case DIKS_STOP:
		*key_code = GF_KEY_MEDIASTOP; break;
	case DIKS_PREVIOUS:
		*key_code = GF_KEY_MEDIAPREVIOUSTRACK; break;
	case DIKS_F1:
		*key_code = GF_KEY_F1; break;
	case DIKS_F2:
		*key_code = GF_KEY_F2; break;
	case DIKS_F3:
		*key_code = GF_KEY_F3; break;
	case DIKS_F4:
		*key_code = GF_KEY_F4; break;
	case DIKS_F5:
		*key_code = GF_KEY_F5; break;
	case DIKS_F6:
		*key_code = GF_KEY_F6; break;
	case DIKS_F7:
		*key_code = GF_KEY_F7; break;
	case DIKS_F8:
		*key_code = GF_KEY_F8; break;
	case DIKS_F9:
		*key_code = GF_KEY_F9; break;
	case DIKS_F10:
		*key_code = GF_KEY_F10; break;
	case DIKS_F11:
		*key_code = GF_KEY_F11; break;
	case DIKS_F12:
		*key_code = GF_KEY_F12; break;
	case DIKS_SHIFT:
		*key_code = GF_KEY_SHIFT; break;
	case DIKS_CONTROL:
		*key_code = GF_KEY_CONTROL; break;
	case DIKS_ALT:
		*key_code = GF_KEY_ALT; break;
	case DIKS_ALTGR:
		*key_code = GF_KEY_ALTGRAPH; break;
	case DIKS_META:
		*key_code = GF_KEY_META; break;
	case DIKS_CAPS_LOCK:
		*key_code = GF_KEY_CAPSLOCK; break;
	case DIKS_NUM_LOCK:
		*key_code = GF_KEY_NUMLOCK; break;
	case DIKS_SCROLL_LOCK:
		*key_code = GF_KEY_SCROLL; break;
	case DIKS_FAVORITES:
		*key_code = GF_KEY_BROWSERFAVORITES; break;
	case DIKS_CUSTOM0:
		*key_code = GF_KEY_BROWSERREFRESH; break;
	case DIKS_MENU:
		*key_code = GF_KEY_BROWSERHOME; break;
	case DIKS_POWER:
		*key_code = GF_KEY_ENTER; break;
	case DIKS_RED:
		*key_code = GF_KEY_TAB; break;
	case DIKS_GREEN:
		*key_code = GF_KEY_CANCEL; break;
	case DIKS_YELLOW:
		*key_code = GF_KEY_COPY; break;
	case DIKS_BLUE:
		*key_code = GF_KEY_CUT; break;
	case DIKS_MODE:
		*key_code = GF_KEY_MODECHANGE; break;
	case DIKS_BACK:
		*key_code = GF_KEY_BROWSERBACK; break;
	case DIKS_TV:
		*key_code = GF_KEY_CLEAR; break;
	case DIKS_OK:
		*key_code = GF_KEY_SELECT; break;
	case DIKS_REWIND:
		*key_code = GF_KEY_BROWSERBACK; break;
	case DIKS_FASTFORWARD:
		*key_code = GF_KEY_BROWSERFORWARD; break;
	case DIKS_SUBTITLE:
		*key_code = GF_KEY_DEL; break;
	case DIKS_CHANNEL_UP:
		*key_code = GF_KEY_CHANNELUP; break;
	case DIKS_CHANNEL_DOWN:
		*key_code = GF_KEY_CHANNELDOWN; break;
	case DIKS_TEXT:
		*key_code = GF_KEY_TEXT; break;
	case DIKS_INFO:
		*key_code = GF_KEY_INFO; break;
	case DIKS_EPG:
		*key_code = GF_KEY_EPG; break;
	case DIKS_RECORD:
		*key_code = GF_KEY_RECORD; break;
	case DIKS_AUDIO:
		*key_code = GF_KEY_BEGINPAGE; break;
	default:
		*key_code = GF_KEY_UNIDENTIFIED; break;
	}
}

