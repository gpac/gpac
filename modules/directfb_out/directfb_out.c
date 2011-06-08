#include "directfb_out.h"

#define DirectFBVID() DirectFBVidCtx *ctx = (DirectFBVidCtx *)driv->opaque
// this was supposed to contain argc and argv from main !!!!!
int argc;
char **argv = {"toto"};

//static const PredefineKeyID(keyIDs);

u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf)
{
  switch (dfbpf) {
   case DSPF_RGB16: return  GF_PIXEL_RGB_565;
   case DSPF_RGB555: return  GF_PIXEL_RGB_555;
   case DSPF_RGB24: return  GF_PIXEL_RGB_24;
   case DSPF_RGB32: return  GF_PIXEL_RGB_32;
   case DSPF_ARGB: return  GF_PIXEL_ARGB;
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
    GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] pixel format %s not supported\n", gf_4cc_to_str(gpacpf)));
    return 0;
  }
}

static DFBEnumerationResult enum_input_device(DFBInputDeviceID device_id, DFBInputDeviceDescription desc, void *data )
{
     DeviceInfo **devices = data;
     DeviceInfo  *device;

     device = malloc( sizeof(DeviceInfo) );

     device->device_id = device_id;
     device->desc      = desc;
     device->next      = *devices;

     *devices = device;

     return GF_OK;
}

static void directfb_translate_key(DFBInputDeviceKeySymbol DirectFBkey, GF_EventKey *evt)
{
	evt->flags = 0;
	evt->hw_code = DirectFBkey;
	switch (DirectFBkey){
	case DIKS_BACKSPACE:
		evt->key_code = GF_KEY_BACKSPACE; break;
	case DIKS_RETURN:
		evt->key_code = GF_KEY_ENTER; break;
	case DIKS_CANCEL:
		evt->key_code = GF_KEY_CANCEL; break;
	case DIKS_ESCAPE:
		evt->key_code = GF_KEY_ESCAPE; break;
	case DIKS_SPACE:
		evt->key_code = GF_KEY_SPACE; break;
	case DIKS_EXCLAMATION_MARK:
		evt->key_code = GF_KEY_EXCLAMATION; break;
	case DIKS_QUOTATION:
		evt->key_code = GF_KEY_QUOTATION; break;
	case DIKS_NUMBER_SIGN:
		evt->key_code = GF_KEY_NUMBER; break;
	case DIKS_DOLLAR_SIGN:
		evt->key_code = GF_KEY_DOLLAR; break;
#if 0
	case DIKS_PERCENT_SIGN:
		evt->key_code = GF_KEY_PERCENT; break;
#endif
	case DIKS_AMPERSAND:
		evt->key_code = GF_KEY_AMPERSAND; break;
	case DIKS_APOSTROPHE:
		evt->key_code = GF_KEY_APOSTROPHE; break;
	case DIKS_PARENTHESIS_LEFT:
		evt->key_code = GF_KEY_LEFTPARENTHESIS; break;
	case DIKS_PARENTHESIS_RIGHT:
		evt->key_code = GF_KEY_RIGHTPARENTHESIS; break;
	case DIKS_ASTERISK:
		evt->key_code = GF_KEY_STAR; break;
	case DIKS_PLUS_SIGN:
		evt->key_code = GF_KEY_PLUS; break;
	case DIKS_COMMA:
		evt->key_code = GF_KEY_COMMA; break;
	case DIKS_MINUS_SIGN:
		evt->key_code = GF_KEY_HYPHEN; break;
	case DIKS_PERIOD:
		evt->key_code = GF_KEY_FULLSTOP; break;
	case DIKS_SLASH:
		evt->key_code = GF_KEY_SLASH; break;
	case DIKS_0:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_0; break;
	case DIKS_1:
                //fprintf(stderr, "DIKS_1: %d\n", GF_KEY_1);
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_1; break;
	case DIKS_2:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_2; break;
	case DIKS_3:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_3; break;
	case DIKS_4:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_4; break;
	case DIKS_5:
               // fprintf(stderr, "DIKS_5\n");
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_5; break;
	case DIKS_6:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_6; break;
	case DIKS_7:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_7; break;
	case DIKS_8:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_8; break;
	case DIKS_9:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_9; break;
	case DIKS_COLON:
		evt->key_code = GF_KEY_COLON; break;
	case DIKS_SEMICOLON:
		evt->key_code = GF_KEY_SEMICOLON; break;
	case DIKS_LESS_THAN_SIGN:
		evt->key_code = GF_KEY_LESSTHAN; break;
	case DIKS_EQUALS_SIGN:
		evt->key_code = GF_KEY_EQUALS; break;
	case DIKS_GREATER_THAN_SIGN:
		evt->key_code = GF_KEY_GREATERTHAN; break;
	case DIKS_QUESTION_MARK:
		evt->key_code = GF_KEY_QUESTION; break;
	case DIKS_AT:
		evt->key_code = GF_KEY_AT; break;
	case DIKS_CAPITAL_A:
		evt->key_code = GF_KEY_A; break;
	case DIKS_CAPITAL_B:
		evt->key_code = GF_KEY_B; break;
	case DIKS_CAPITAL_C:
		evt->key_code = GF_KEY_C; break;
	case DIKS_CAPITAL_D:
		evt->key_code = GF_KEY_D; break;
	case DIKS_CAPITAL_E:
		evt->key_code = GF_KEY_E; break;
	case DIKS_CAPITAL_F:
		evt->key_code = GF_KEY_F; break;
	case DIKS_CAPITAL_G:
		evt->key_code = GF_KEY_G; break;
	case DIKS_CAPITAL_H:
		evt->key_code = GF_KEY_H; break;
	case DIKS_CAPITAL_I:
		evt->key_code = GF_KEY_I; break;
	case DIKS_CAPITAL_J:
		evt->key_code = GF_KEY_J; break;
	case DIKS_CAPITAL_K:
		evt->key_code = GF_KEY_K; break;
	case DIKS_CAPITAL_L:
		evt->key_code = GF_KEY_L; break;
	case DIKS_CAPITAL_M:
		evt->key_code = GF_KEY_M; break;
	case DIKS_CAPITAL_N:
		evt->key_code = GF_KEY_N; break;
	case DIKS_CAPITAL_O:
		evt->key_code = GF_KEY_O; break;
	case DIKS_CAPITAL_P:
		evt->key_code = GF_KEY_P; break;
	case DIKS_CAPITAL_Q:
		evt->key_code = GF_KEY_Q; break;
	case DIKS_CAPITAL_R:
		evt->key_code = GF_KEY_R; break;
	case DIKS_CAPITAL_S:
		evt->key_code = GF_KEY_S; break;
	case DIKS_CAPITAL_T:
		evt->key_code = GF_KEY_T; break;
	case DIKS_CAPITAL_U:
		evt->key_code = GF_KEY_U; break;
	case DIKS_CAPITAL_V:
		evt->key_code = GF_KEY_V; break;
	case DIKS_CAPITAL_W:
		evt->key_code = GF_KEY_W; break;
	case DIKS_CAPITAL_X:
		evt->key_code = GF_KEY_X; break;
	case DIKS_CAPITAL_Y:
		evt->key_code = GF_KEY_Y; break;
	case DIKS_CAPITAL_Z:
		evt->key_code = GF_KEY_Z; break;
	case DIKS_SQUARE_BRACKET_LEFT:
		evt->key_code = GF_KEY_LEFTSQUAREBRACKET; break;
	case DIKS_BACKSLASH:
		evt->key_code = GF_KEY_BACKSLASH; break;
	case DIKS_SQUARE_BRACKET_RIGHT:
		evt->key_code = GF_KEY_RIGHTSQUAREBRACKET; break;
	case DIKS_CIRCUMFLEX_ACCENT:
		evt->key_code = GF_KEY_CIRCUM; break;
	case DIKS_UNDERSCORE:
		evt->key_code = GF_KEY_UNDERSCORE; break;
	case DIKS_GRAVE_ACCENT:
		evt->key_code = GF_KEY_GRAVEACCENT; break;
	case DIKS_CURLY_BRACKET_LEFT:
		evt->key_code = GF_KEY_LEFTCURLYBRACKET; break;
	case DIKS_VERTICAL_BAR:
		evt->key_code = GF_KEY_PIPE; break;
	case DIKS_CURLY_BRACKET_RIGHT:
		evt->key_code = GF_KEY_RIGHTCURLYBRACKET; break;
	case DIKS_TILDE: break;
	case DIKS_DELETE:
		evt->key_code = GF_KEY_DEL; break;
	case DIKS_CURSOR_LEFT:
		evt->key_code = GF_KEY_LEFT; break;
	case DIKS_CURSOR_RIGHT:
		evt->key_code = GF_KEY_RIGHT; break;
	case DIKS_CURSOR_UP:
		evt->key_code = GF_KEY_UP; break;
	case DIKS_CURSOR_DOWN:
		evt->key_code = GF_KEY_DOWN; break;
	case DIKS_INSERT:
		evt->key_code = GF_KEY_INSERT; break;
	case DIKS_HOME:
		evt->key_code = GF_KEY_HOME; break;
	case DIKS_END:
		evt->key_code = GF_KEY_END; break;
	case DIKS_PAGE_UP:
		evt->key_code = GF_KEY_PAGEUP; break;
	case DIKS_PAGE_DOWN:
		evt->key_code = GF_KEY_PAGEDOWN; break;
	case DIKS_PRINT:
		evt->key_code = GF_KEY_PRINTSCREEN; break;
	case DIKS_SELECT:
		evt->key_code = GF_KEY_SELECT; break;
	case DIKS_CLEAR:
		evt->key_code = GF_KEY_CLEAR; break;
	case DIKS_HELP:
		evt->key_code = GF_KEY_HELP; break;
	case DIKS_ZOOM:
		evt->key_code = GF_KEY_ZOOM; break;
	case DIKS_VOLUME_UP:
		evt->key_code = GF_KEY_VOLUMEUP; break;
	case DIKS_VOLUME_DOWN:
		evt->key_code = GF_KEY_VOLUMEDOWN; break;
	case DIKS_MUTE:
		evt->key_code = GF_KEY_VOLUMEMUTE; break;
	case DIKS_PLAYPAUSE:
	case DIKS_PAUSE:
		evt->key_code = GF_KEY_MEDIAPLAYPAUSE; break;
	case DIKS_PLAY:
		evt->key_code = GF_KEY_PLAY; break;
	case DIKS_STOP:
		evt->key_code = GF_KEY_MEDIASTOP; break;
	case DIKS_PREVIOUS:
		evt->key_code = GF_KEY_MEDIAPREVIOUSTRACK; break;
	case DIKS_F1:
		evt->key_code = GF_KEY_F1; break;
	case DIKS_F2:
		evt->key_code = GF_KEY_F2; break;
	case DIKS_F3:
		evt->key_code = GF_KEY_F3; break;
	case DIKS_F4:
		evt->key_code = GF_KEY_F4; break;
	case DIKS_F5:
		evt->key_code = GF_KEY_F5; break;
	case DIKS_F6:
		evt->key_code = GF_KEY_F6; break;
	case DIKS_F7:
		evt->key_code = GF_KEY_F7; break;
	case DIKS_F8:
		evt->key_code = GF_KEY_F8; break;
	case DIKS_F9:
		evt->key_code = GF_KEY_F9; break;
	case DIKS_F10:
		evt->key_code = GF_KEY_F10; break;
	case DIKS_F11:
		evt->key_code = GF_KEY_F11; break;
	case DIKS_F12:
		evt->key_code = GF_KEY_F12; break;
	case DIKS_SHIFT:
		evt->key_code = GF_KEY_SHIFT; break;
	case DIKS_CONTROL:
		evt->key_code = GF_KEY_CONTROL; break;
	case DIKS_ALT:
		evt->key_code = GF_KEY_ALT; break;
	case DIKS_ALTGR:
		evt->key_code = GF_KEY_ALTGRAPH; break;
	case DIKS_META:
		evt->key_code = GF_KEY_META; break;
	case DIKS_CAPS_LOCK:
		evt->key_code = GF_KEY_CAPSLOCK; break;
	case DIKS_NUM_LOCK:
		evt->key_code = GF_KEY_NUMLOCK; break;
	case DIKS_SCROLL_LOCK:
		evt->key_code = GF_KEY_SCROLL; break;
	case DIKS_FAVORITES:
		evt->key_code = GF_KEY_BROWSERFAVORITES; break;
	case DIKS_CUSTOM0:
		evt->key_code = GF_KEY_BROWSERREFRESH; break;
	case DIKS_MENU:
		evt->key_code = GF_KEY_BROWSERHOME; break;
	case DIKS_POWER:
		evt->key_code = GF_KEY_ENTER; break;
	case DIKS_RED:
		evt->key_code = GF_KEY_TAB; break;
	case DIKS_GREEN:
		evt->key_code = GF_KEY_CANCEL; break;
	case DIKS_YELLOW:
		evt->key_code = GF_KEY_COPY; break;
	case DIKS_BLUE:
		evt->key_code = GF_KEY_CUT; break;
	case DIKS_MODE:
		evt->key_code = GF_KEY_MODECHANGE; break;
	case DIKS_BACK:
		evt->key_code = GF_KEY_BROWSERBACK; break;
	case DIKS_TV:
		evt->key_code = GF_KEY_CLEAR; break;
	case DIKS_OK:
		evt->key_code = GF_KEY_SELECT; break;
	case DIKS_REWIND:
		evt->key_code = GF_KEY_BROWSERBACK; break;
	case DIKS_FASTFORWARD:
		evt->key_code = GF_KEY_BROWSERFORWARD; break;
	case DIKS_SUBTITLE:
		evt->key_code = GF_KEY_DEL; break;
	case DIKS_CHANNEL_UP:
		evt->key_code = GF_KEY_CHANNELUP; break;
	case DIKS_CHANNEL_DOWN:
		evt->key_code = GF_KEY_CHANNELDOWN; break;
	case DIKS_TEXT:
		evt->key_code = GF_KEY_TEXT; break;
	case DIKS_INFO:
		evt->key_code = GF_KEY_INFO; break;
	case DIKS_EPG:
		evt->key_code = GF_KEY_EPG; break;
	case DIKS_RECORD:
		evt->key_code = GF_KEY_RECORD; break;
	case DIKS_AUDIO:
		evt->key_code = GF_KEY_BEGINPAGE; break;
	default:
		evt->key_code = GF_KEY_UNIDENTIFIED; break;
	}
}
#if 0

static int compare_symbol( const void *a, const void *b )
{
     u32      *keycode  = (u32 *) a;
     struct predef_keyid *idname = (struct predef_keyid *) b;

     return *keycode - idname->key_code;
}

static const char *get_local_key_name(u32 key_code) {
     struct predef_keyid      *predefine_keyid;

     predefine_keyid = bsearch(&key_code, keyIDs, sizeof(keyIDs) / sizeof(keyIDs[0]) - 1, sizeof(keyIDs[0]), compare_symbol );
     if (predefine_keyid) return predefine_keyid->name;
     else return NULL;
}

static void show_key_event(DirectFBVidCtx *ctx, GF_EventKey *evt )
{
     //DirectFBVID();
     char                               buf[16];
     struct predef_keyid      *predefine_keyid;

     predefine_keyid = bsearch(&evt->key_code, keyIDs, sizeof(keyIDs) / sizeof(keyIDs[0]) - 1, sizeof(keyIDs[0]), compare_symbol );


     if (predefine_keyid) {
	  GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DirectFB] Predefined key ID from show_key_event():%s\n",predefine_keyid->name));
     }
     ctx->primary->SetColor( ctx->primary, 0x60, 0x60, 0x60, 0xFF );
     snprintf (buf, sizeof(buf), "0x%X", evt->key_code);
     ctx->primary->DrawString( ctx->primary, buf, -1,
                          ctx->width - 40, ctx->height/3,
                          DSTF_RIGHT );


}

#endif

static void DirectFBVid_DrawHLine(GF_VideoOutput *driv, u32 x, u32 y, u32 length, GF_Color color)
{
	DirectFBVID();
	u8 r, g, b;

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawHLine(). Drawing line x %d y %d length %d color %08X\n", x, y, length, color));
	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	r = GF_COL_R(color);
	g = GF_COL_G(color);
	b = GF_COL_B(color);

	ctx->primary->SetColor(ctx->primary, r, g, b, 0xFF); // no alpha
	//ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);	// no acceleration
	ctx->primary->FillRectangle(ctx->primary, x, y,length, 1);

}

static void DirectFBVid_DrawHLineAlpha(GF_VideoOutput *driv, u32 x, u32 y, u32 length, GF_Color color, u8 alpha)
{
	DirectFBVID();
	u8 r, g, b;

        //GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawHLineAlpha(). Alpha line drawing x %d y %d length %d color %08X alpha %d\n", x, y, length, color, alpha));

	SET_DRAWING_FLAGS( DSDRAW_BLEND ); // use alpha

	r = GF_COL_R(color);
	g = GF_COL_G(color);
	b = GF_COL_B(color);

	ctx->primary->SetColor(ctx->primary, r, g, b, alpha);
	//ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);
	ctx->primary->FillRectangle(ctx->primary, x, y, length, 1);	// with acceleration
}

static void DirectFBVid_DrawRectangle(GF_VideoOutput *driv, u32 x, u32 y, u32 width, u32 height, GF_Color color)
{
	DirectFBVID();
	u8 r, g, b, a;

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] in DirectFBVid_DrawRectangle(). Drawing rectangle x %d y %d width %d height %d color %08x\n", x, y, width, height, color));


	r = GF_COL_R(color);
	g = GF_COL_G(color);
	b = GF_COL_B(color);
	a = GF_COL_A(color);

	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	ctx->primary->SetColor(ctx->primary, r, g, b, a);
	ctx->primary->FillRectangle(ctx->primary, x, y, width, height);
	//ctx->primary->Blit( ctx->primary, ctx->primary, NULL, x, y );
}


GF_Err DirectFBVid_Setup(GF_VideoOutput *driv, void *os_handle, void *os_display, u32 init_flags)
{
  const char* opt;
  DFBResult err;
  DFBSurfaceDescription dsc;
  DFBSurfacePixelFormat dfbpf;
  DFBAccelerationMask mask;
  DeviceInfo *devices = NULL;

  DirectFBVID();
  ctx->is_init = 0;
  argc=0;
  DFBCHECK(DirectFBInit(&argc, & (argv) ));

  DirectFBSetOption ("bg-none", NULL);
  DirectFBSetOption ("no-init-layer", NULL);

  /* create the super interface */
  DFBCHECK(DirectFBCreate( &(ctx->dfb) ));

  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Initialization\n"));

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

  ctx->disable_acceleration = 0;
  opt = gf_modules_get_option((GF_BaseInterface *)driv, "DirectFB", "DisableAcceleration");
  if (!opt) gf_modules_set_option((GF_BaseInterface *)driv, "DirectFB", "DisableAcceleration", "no");
  if (opt && !strcmp(opt, "yes")) ctx->disable_acceleration = 1;

  ctx->disable_display = 0;
  opt = gf_modules_get_option((GF_BaseInterface *)driv, "DirectFB", "DisableDisplay");
  if (!opt) gf_modules_set_option((GF_BaseInterface *)driv, "DirectFB", "DisableDisplay", "no");
  if (opt && !strcmp(opt, "yes")) ctx->disable_display = 1;


  ctx->flip_mode = DSFLIP_BLIT;
  opt = gf_modules_get_option((GF_BaseInterface *)driv, "DirectFB", "FlipSyncMode");
  if (!opt) gf_modules_set_option((GF_BaseInterface *)driv, "DirectFB", "FlipSyncMode", "waitsync");
  if (!opt || !strcmp(opt, "waitsync")) ctx->flip_mode |= DSFLIP_WAITFORSYNC;
  else if (opt && !strcmp(opt, "wait")) ctx->flip_mode |= DSFLIP_WAIT;
  else if (opt && !strcmp(opt, "sync")) ctx->flip_mode |= DSFLIP_ONSYNC;
  else if (opt && !strcmp(opt, "swap")) ctx->flip_mode &= ~DSFLIP_BLIT;

  opt = gf_modules_get_option((GF_BaseInterface *)driv, "DirectFB", "DisableBlit");
  if (!opt) gf_modules_set_option((GF_BaseInterface *)driv, "DirectFB", "DisableBlit", "no");
  if (opt && !strcmp(opt, "all")) {
    driv->hw_caps &= ~(GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_YUV);
  }
  else if (opt && !strcmp(opt, "yuv")) driv->hw_caps &= ~GF_VIDEO_HW_HAS_YUV;
  else if (opt && !strcmp(opt, "rgb")) driv->hw_caps &= ~GF_VIDEO_HW_HAS_RGB;
  else if (opt && !strcmp(opt, "rgba")) driv->hw_caps &= ~GF_VIDEO_HW_HAS_RGBA;


  if (!ctx->disable_acceleration){
  	ctx->primary->GetAccelerationMask( ctx->primary, NULL, &mask );
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DirectFB] hardware acceleration mask %08x \n", mask));

  	if (mask & DFXL_DRAWLINE ) // DrawLine() is accelerated. DFXL_DRAWLINE
		ctx->accel_drawline = 1;
  	if (mask & DFXL_FILLRECTANGLE) // FillRectangle() is accelerated.
		ctx->accel_fillrect = 1;

  GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DirectFB] hardware acceleration mask %08x - Line: %d Rectangle: %d\n", mask, ctx->accel_drawline, ctx->accel_fillrect));

       	driv->hw_caps |= GF_VIDEO_HW_HAS_LINE_BLIT;
	driv->DrawHLine = DirectFBVid_DrawHLine;
	driv->DrawHLineAlpha = DirectFBVid_DrawHLineAlpha;
	driv->DrawRectangle = DirectFBVid_DrawRectangle;
  }

  ctx->is_init = 1;
  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Initialization success - HW caps %08x\n", driv->hw_caps));

//  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Pixel format %s\n", gf_4cc_to_str(ctx->pixel_format)));
  return GF_OK;
}

static void DirectFBVid_Shutdown(GF_VideoOutput *driv)
{
  DirectFBVID();
  if (!ctx->is_init) return;
  //ctx->primary->Clear(ctx->primary,0,0,0,0);
  //ctx->primary->Flip( ctx->primary, NULL, DSFLIP_NONE);
  //ctx->primary->Clear(ctx->primary,0,0,0,0);
  ctx->primary->Release( ctx->primary );
  ctx->events->Release( ctx->events );
  ctx->dfb->Release( ctx->dfb );
  ctx->is_init = 0;
  system("stgfb_control /dev/fb0 a 0");
}

static GF_Err DirectFBVid_Flush(GF_VideoOutput *driv, GF_Window *dest)
{
  const char* opt;

  DirectFBVID();
  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Flipping backbuffer\n"));
  if (ctx->disable_display) return GF_OK;

  ctx->primary->Flip( ctx->primary, NULL, ctx->flip_mode);
}


GF_Err DirectFBVid_SetFullScreen(GF_VideoOutput *driv, u32 bFullScreenOn, u32 *screen_width, u32 *screen_height)
{
  DFBResult err;
  DirectFBVID();

  *screen_width = ctx->width;
  *screen_height = ctx->height;

  return GF_OK;
}


Bool DirectFBVid_ProcessMessageQueue(DirectFBVidCtx *ctx, GF_VideoOutput *driv)
{
  DFBInputEvent directfb_evt;
  GF_Event gpac_evt;

  while (ctx->events->GetEvent( ctx->events, DFB_EVENT(&directfb_evt) ) == DFB_OK)
  {
	u32 i;
	switch (directfb_evt.type){
		case DIET_KEYPRESS:
		case DIET_KEYRELEASE:
			//GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DirectFB] in ProcessMessageQueue\n"));
			directfb_translate_key(directfb_evt.key_symbol, &gpac_evt.key);
			gpac_evt.type = (directfb_evt.type == DIET_KEYPRESS) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
#if 0
			fprintf(stderr, "before %d %s\n", gpac_evt.key.key_code,
							     gf_dom_get_key_name(gpac_evt.key.key_code));

			for (i=0;i<200; i++) {
			  fprintf(stderr, "%03d %s\n", i, gf_dom_get_key_name(i));
			}
#endif
			driv->on_event(driv->evt_cbk_hdl, &gpac_evt);
			//fprintf(stderr, "after %d %s\n", gpac_evt.key.key_code, gf_dom_get_key_name(gpac_evt.key.key_code));
		default:
			break;
	}

  }

  return GF_OK;
}


static GF_Err DirectFBVid_ProcessEvent(GF_VideoOutput *driv, GF_Event *evt)
{
  DirectFBVID();

  if (!evt) {
    DirectFBVid_ProcessMessageQueue(ctx, driv);
    return GF_OK;
  }

  switch (evt->type) {
  case GF_EVENT_SIZE:
   if ((ctx->width !=evt->size.width) || (ctx->height != evt->size.height)) {
	GF_Event gpac_evt;
  	gpac_evt.type = GF_EVENT_SIZE;
	gpac_evt.size.width = ctx->width;
	gpac_evt.size.height = ctx->height;
	driv->on_event(driv->evt_cbk_hdl, &gpac_evt);
   }
    return GF_OK;

  case GF_EVENT_VIDEO_SETUP:
   if (evt->setup.opengl_mode) return GF_NOT_SUPPORTED;

   if ((ctx->width !=evt->setup.width) || (ctx->height != evt->setup.height)) {
	GF_Event gpac_evt;
  	gpac_evt.type = GF_EVENT_SIZE;
	gpac_evt.size.width = ctx->width;
	gpac_evt.size.height = ctx->height;
	driv->on_event(driv->evt_cbk_hdl, &gpac_evt);
   }
   return GF_OK;
  default:
    return GF_OK;
  }
}

static GF_Err DirectFBVid_LockBackBuffer(GF_VideoOutput *driv, GF_VideoSurface *video_info, u32 do_lock)
{
  DFBResult ret;
  u32 pitch;
  void *buf;
  u32 width, height;
  DFBSurfacePixelFormat format;

  DirectFBVID();
  if (!ctx->primary) return GF_BAD_PARAM;
  if (do_lock)
  {
    if (!video_info) return GF_BAD_PARAM;
    ret = ctx->primary->Lock(ctx->primary, DSLF_READ | DSLF_WRITE, &buf, &pitch);
    if (ret != DFB_OK) return GF_IO_ERR;

    video_info->width = ctx->width;
    video_info->height = ctx->height;
    video_info->pitch_x = 0;
    video_info->pitch_y = pitch;
    video_info->video_buffer = buf;
    video_info->pixel_format = ctx->pixel_format;
    video_info->is_hardware_memory = !ctx->use_systems_memory;

   GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] backbuffer locked\n"));
  } else {
    ctx->primary->Unlock(ctx->primary);
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] backbuffer unlocked\n"));
  }
  return GF_OK;

}

static GF_Err DirectFBVid_Blit(GF_VideoOutput *driv, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
  DirectFBVID();

  DFBResult res;
  DFBSurfaceDescription srcdesc;
  IDirectFBSurface *src;
  DFBRectangle dfbsrc, dfbdst;
  DFBAccelerationMask mask;

  if (overlay_type != 0) return GF_NOT_SUPPORTED;
  if (ctx->disable_display) return GF_OK;

  memset(&srcdesc, 0, sizeof(srcdesc));

  srcdesc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_PREALLOCATED;
  srcdesc.width = video_src->width;
  srcdesc.height = video_src->height;
  srcdesc.pixelformat = DirectFBVid_TranslatePixelFormatFromGPAC(video_src->pixel_format);
  srcdesc.preallocated[0].data = video_src->video_buffer;
  srcdesc.preallocated[0].pitch = video_src->pitch_y;


  switch (video_src->pixel_format){
  case GF_PIXEL_ARGB: //return DSPF_ARGB;
  case GF_PIXEL_RGBA: //return DSPF_ARGB;
	ctx->primary->SetBlittingFlags(ctx->primary, DSBLIT_BLEND_ALPHACHANNEL);
	break;
  default:
	ctx->primary->SetBlittingFlags(ctx->primary, DSBLIT_NOFX);
  }


  res = ctx->dfb->CreateSurface(ctx->dfb, &srcdesc, &src);
  if (res != DFB_OK) {
   GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] cannot create blit source surface for pixel format %s: %s (%d)\n", gf_4cc_to_str(video_src->pixel_format), DirectFBErrorString(res), res));
   return GF_IO_ERR;
  }


   dfbsrc.x = src_wnd->x;
   dfbsrc.y = src_wnd->y;
   dfbsrc.w = src_wnd->w;
   dfbsrc.h = src_wnd->h;

   if (!src_wnd->x && !src_wnd->y && (dst_wnd->w==src_wnd->w) && (dst_wnd->h==src_wnd->h)) {
	   ctx->primary->Blit(ctx->primary, src, &dfbsrc, dst_wnd->x, dst_wnd->y);
   } else {
	   dfbdst.x = dst_wnd->x;
	   dfbdst.y = dst_wnd->y;
	   dfbdst.w = dst_wnd->w;
	   dfbdst.h = dst_wnd->h;
	   ctx->primary->StretchBlit(ctx->primary, src, &dfbsrc, &dfbdst);
  }

  src->Release(src);
  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] blit successful\n"));

  return GF_OK;
}

void *DirectFBNewVideo()
{
  DirectFBVidCtx *ctx;
  GF_VideoOutput *driv;

  driv = gf_malloc(sizeof(GF_VideoOutput));
  memset(driv, 0, sizeof(GF_VideoOutput));
  GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "DirectFB Video Output", "gpac distribution");

  ctx = gf_malloc(sizeof(DirectFBVidCtx));
  memset(ctx, 0, sizeof(DirectFBVidCtx));

  /* GF_VideoOutput */
  driv->opaque = ctx;
  driv->Setup = DirectFBVid_Setup;
  driv->Shutdown = DirectFBVid_Shutdown;
  driv->Flush = DirectFBVid_Flush;
  driv->SetFullScreen = DirectFBVid_SetFullScreen;
  driv->ProcessEvent = DirectFBVid_ProcessEvent;
  driv->LockBackBuffer = DirectFBVid_LockBackBuffer;
  driv->LockOSContext = NULL;
  driv->Blit = DirectFBVid_Blit;
  driv->hw_caps |= GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_STRETCH;

  return driv;
}

void DirectFBDeleteVideo(void *ifce)
{
  GF_VideoOutput *driv = (GF_VideoOutput *)ifce;
  DirectFBVID();
  gf_free(ctx);
  gf_free(driv);
}


/*interface query*/
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		0
	};
	return si;
}

/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return DirectFBNewVideo();
	return NULL;
}

/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DirectFBDeleteVideo(ifce);
		break;
	}
}
