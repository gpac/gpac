#ifndef _DIRECTFB_OUT_H_
#define _DIRECTFB_OUT_H_


#include <gpac/modules/video_out.h>

/* DirectFB */
#define __DIRECT__STDTYPES__	//prevent u8, s8, ... definitions by directFB as we have them in GPAC
#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>
#include <direct/util.h>

static int do_xor       = 0;
static int accel_drawline   = 0;
static int accel_fillrect = 0;

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
          

typedef struct
{
    /* the super interface */
    IDirectFB *dfb;
    /* the primary surface */
    IDirectFBSurface *primary;

    /* screen width, height */
    u32 width, height, pixel_format;
    Bool use_systems_memory, disable_acceleration, disable_aa, is_init;

    /* Input interfaces: event buffer */
//  IDirectFBEventBuffer *key_events;

} DirectFBVidCtx;

void *DirectFBNewVideo();
void DirectFBDeleteVideo(void *ifce);

#endif
