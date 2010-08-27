#include "directfb_out.h"

#define DirectFBVID() DirectFBVidCtx *ctx = (DirectFBVidCtx *)driv->opaque
// this was supposed to contain argc and argv from main !!!!!
int argc;
char **argv = {"toto"};

u32 DirectFBVid_TranslatePixelFormatToGPAC(u32 dfbpf)
{
  switch (dfbpf) {
   case DSPF_RGB16: return  GF_PIXEL_RGB_565;
   case DSPF_RGB555: return  GF_PIXEL_RGB_555;
   case DSPF_RGB24: return  GF_PIXEL_RGB_24;
   case DSPF_RGB32: return  GF_PIXEL_RGB_32;
   case DSPF_ARGB: return  GF_PIXEL_ARGB;
//   case DSPF_YUY2: return  GF_PIXEL_YUY2;
//   case DSPF_YV12: return  GF_PIXEL_YV12;
//   case DSPF_I420: return  GF_PIXEL_YV12;
   default: return 0;
  }
}

u32 DirectFBVid_TranslatePixelFormatFromGPAC(u32 dfbpf)
{
  switch (dfbpf) {
   case GF_PIXEL_RGB_565: return DSPF_RGB16;
   case GF_PIXEL_RGB_555 : return DSPF_RGB555;
   case GF_PIXEL_RGB_24 : return DSPF_RGB24;
   case GF_PIXEL_RGB_32 : return DSPF_RGB32;
   case GF_PIXEL_ARGB: return DSPF_ARGB;
   case GF_PIXEL_YUY2 : return DSPF_YUY2;
   case GF_PIXEL_YV12 : return DSPF_YV12;
   default: return 0;
  }
}

GF_Err DirectFBVid_Setup(GF_VideoOutput *driv, void *os_handle, void *os_display, u32 init_flags)
{
  DFBResult err;
  DFBSurfaceDescription dsc;
  DFBSurfacePixelFormat dfbpf;
  DFBAccelerationMask mask;
  
  DirectFBVID();
  ctx->is_init = 0;
  argc=0;
  DFBCHECK(DirectFBInit(&argc, & (argv) )); 
  
  DirectFBSetOption ("bg-none", NULL);
  DirectFBSetOption ("no-init-layer", NULL);
 
  /* create the super interface */
  DFBCHECK(DirectFBCreate( &(ctx->dfb) ));
  
  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Initialization\n"));

  /* create an input buffer for key events */
//  DFBCHECK(ctx->dfb->CreateInputEventBuffer( ctx->dfb, DICAPS_KEYS, DFB_FALSE, &key_events ));
  
  /* Set the cooperative level */
  err = ctx->dfb->SetCooperativeLevel( ctx->dfb, DFSCL_FULLSCREEN );
  if (err)
    DirectFBError( "Failed to set cooperative level", err );
  
  /* Get the primary surface, i.e. the surface of the primary layer. */
  dsc.flags = DSDESC_CAPS;
  dsc.caps = DSCAPS_PRIMARY | DSCAPS_DOUBLE;

  if (ctx->use_systems_memory) 
   dsc.caps |= DSCAPS_SYSTEMONLY;
  
  DFBCHECK(ctx->dfb->CreateSurface( ctx->dfb, &dsc, &(ctx->primary) ));

  ctx->primary->GetPixelFormat( ctx->primary, &dfbpf );
  ctx->pixel_format = DirectFBVid_TranslatePixelFormatToGPAC(dfbpf);
  ctx->primary->GetSize( ctx->primary, &(ctx->width), &(ctx->height) );
  ctx->primary->Clear( ctx->primary, 0, 0, 0, 0xFF);
  
  ctx->primary->GetAccelerationMask( ctx->primary, NULL, &mask );
  if (mask & DFXL_DRAWLINE ) // DrawLine() is accelerated.
	accel_drawline = 1;
  if (mask & DFXL_FILLRECTANGLE) // FillRectangle() is accelerated.
	accel_fillrect = 1;

  printf("accel_drawline=%d\n",accel_drawline);
  printf("accel_fillrect=%d\n",accel_fillrect);

  ctx->is_init = 1;
  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Initialization success\n"));
  return GF_OK;  
}

static void DirectFBVid_Shutdown(GF_VideoOutput *driv)
{
  DirectFBVID();
  if (!ctx->is_init) return;
  ctx->primary->Release( ctx->primary );
//  ctx->key_events->Release( ctx->key_events );
  ctx->dfb->Release( ctx->dfb );
  ctx->is_init = 0;
}

static GF_Err DirectFBVid_Flush(GF_VideoOutput *driv, GF_Window *dest)
{
  
  DirectFBVID();
  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Flipping backbuffer\n"));
  ctx->primary->Flip( ctx->primary, NULL, DSFLIP_ONSYNC );
}


GF_Err DirectFBVid_SetFullScreen(GF_VideoOutput *driv, u32 bFullScreenOn, u32 *screen_width, u32 *screen_height)
{
  DFBResult err;
  DirectFBVID();  
  
  *screen_width = ctx->width;
  *screen_height = ctx->height;

  return GF_OK;
}

#if 0
Bool DirectFBVid_ProcessMessageQueue(DirectFBVidCtx *ctx, GF_VideoOutput *driv)
{
  DFBInputEvent ev;
  u32 err;
  
  while (ctx->key_events->GetEvent( ctx->key_events, DFB_EVENT(&ev) ) == DFB_OK) 
  {
          if (ev.type == DIET_KEYPRESS) 
	  {
               switch (ev.key_symbol) 
	       {
                    case DIKS_ESCAPE:
                    case DIKS_SMALL_Q:
                    case DIKS_CAPITAL_Q:
                    case DIKS_BACK:
                    case DIKS_STOP:
                         DirectFBVid_Shutdown(driv);
                         exit( 42 );
                         break;
                    default:
                         break;
               }
          }
  }
  return 1;
}
#endif

static GF_Err DirectFBVid_ProcessEvent(GF_VideoOutput *driv, GF_Event *evt)
{
  DirectFBVID();
  if (!evt) {
    //DirectFBVid_ProcessMessageQueue(ctx, driv);
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
  }
  return GF_OK;
  
}

static GF_Err DirectFBVid_Blit(GF_VideoOutput *driv, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
  DirectFBVID();
  if (overlay_type == 0) return GF_OK;
  else return GF_NOT_SUPPORTED;  
  //return GF_OK;
}

static void DirectFBVid_DrawHLine(GF_VideoOutput *driv, u32 x, u32 y, u32 length, GF_Color color)
{
	DirectFBVID();
	u8 r, g, b;
			
	SET_DRAWING_FLAGS( DSDRAW_NOFX );

	r = GF_COL_R(color);
	g = GF_COL_G(color);
	b = GF_COL_B(color);
			
	ctx->primary->SetColor(ctx->primary, r, g, b, 0xFF); // no alpha
	ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);	

}

static void DirectFBVid_DrawHLineAlpha(GF_VideoOutput *driv, u32 x, u32 y, u32 length, GF_Color color, u8 alpha)
{
	DirectFBVID();
	u8 r, g, b;


	SET_DRAWING_FLAGS( DSDRAW_BLEND ); // use alpha 

	r = GF_COL_R(color);
	g = GF_COL_G(color);
	b = GF_COL_B(color);	
		
	ctx->primary->SetColor(ctx->primary, r, g, b, alpha);
	ctx->primary->DrawLine(ctx->primary, x, y, x+length, y);

}

static void DirectFBVid_DrawRectangle(GF_VideoOutput *driv, u32 x, u32 y, u32 width, u32 height, GF_Color color)
{
	DirectFBVID();

	u8 r, g, b, a;

	r = GF_COL_R(color);
	g = GF_COL_G(color);
	b = GF_COL_B(color);
	a = GF_COL_A(color);
	
	SET_DRAWING_FLAGS( DSDRAW_NOFX );	

	ctx->primary->SetColor(ctx->primary, r, g, b, a);
	ctx->primary->FillRectangle(ctx->primary, x, y, width, height);
	//ctx->primary->Blit( ctx->primary, ctx->primary, NULL, x, y );
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
  driv->hw_caps |= GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_LINE_BLIT;
  
  if (driv->hw_caps & GF_VIDEO_HW_HAS_LINE_BLIT)
	{
		driv->DrawHLine = DirectFBVid_DrawHLine;
		driv->DrawHLineAlpha = DirectFBVid_DrawHLineAlpha;
		driv->DrawRectangle = DirectFBVid_DrawRectangle;	
	}
	
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
