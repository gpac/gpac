/*****************************************************************************
 *
 * XVID MPEG-4 VIDEO CODEC
 * - XviD Main header file -
 *
 *  Copyright(C) 2001-2003 Peter Ross <pross@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: xvid.h,v 1.2 2006-12-13 15:12:27 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _XVID_H_
#define _XVID_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "Rules.h"


#ifdef PROFILE
#include <Profiler.h>
#define PROF_S(n) prof.MarkS(n)
#define PROF_E(n) prof.MarkE(n)

enum {
	PROF_TICK,
	PROF_CONV,
	PROF_UPD,
	PROF_DECODE,
	PROF_DRAWTXT,
	PROF_FRM_I,
	PROF_FRM_P,
	PROF_BINT_MBI,
	PROF_FRM_B,
	PROF_0,
	PROF_1,
	PROF_2,
	PROF_3,
	PROF_4,
	PROF_5,
	PROF_6,
	PROF_7,
};

#else
#define PROF_S(n)
#define PROF_E(n)
#endif

#ifdef __MARM__
//#define USE_ARM_ASM
#endif

/*****************************************************************************
 * versioning
 ****************************************************************************/

/* versioning
	version takes the form "$major.$minor.$patch"
	$patch is incremented when there is no api change
	$minor is incremented when the api is changed, but remains backwards compatible
	$major is incremented when the api is changed significantly

	when initialising an xvid structure, you must always zero it, and set the version field.
		memset(&struct,0,sizeof(struct));
		struct.version = XVID_VERSION;

	XVID_UNSTABLE is defined only during development.
	*/

#define XVID_MAKE_VERSION(a,b,c) ((((a)&0xff)<<16) | (((b)&0xff)<<8) | ((c)&0xff))
#define XVID_VERSION_MAJOR(a)    ((char)(((a)>>16) & 0xff))
#define XVID_VERSION_MINOR(a)    ((char)(((a)>> 8) & 0xff))
#define XVID_VERSION_PATCH(a)    ((char)(((a)>> 0) & 0xff))

#define XVID_MAKE_API(a,b)       ((((a)&0xff)<<16) | (((b)&0xff)<<0))
#define XVID_API_MAJOR(a)        (((a)>>16) & 0xff)
#define XVID_API_MINOR(a)        (((a)>> 0) & 0xff)

#define XVID_VERSION             XVID_MAKE_VERSION(1,0,-127)
#define XVID_API                 XVID_MAKE_API(4, 0)

#define EDGE_SIZE 16

#define XVID_UNSTABLE

/* Bitstream Version
 * this will be writen into the bitstream to allow easy detection of xvid
 * encoder bugs in the decoder, without this it might not possible to
 * automatically distinquish between a file which has been encoded with an
 * old & buggy XVID from a file which has been encoded with a bugfree version
 * see the infamous interlacing bug ...
 *
 * this MUST be increased if an encoder bug is fixed, increasing it too often
 * doesnt hurt but not increasing it could cause difficulty for decoders in the
 * future
 */
#define XVID_BS_VERSION "0023"


/*****************************************************************************
 * error codes
 ****************************************************************************/

/*	all functions return values <0 indicate error */

#define XVID_ERR_FAIL		-1		/* general fault */
#define XVID_ERR_MEMORY		-2		/* memory allocation error */
#define XVID_ERR_FORMAT		-3		/* file format error */
#define XVID_ERR_VERSION	-4		/* structure version not supported */
#define XVID_ERR_END		-5		/* encoder only; end of stream reached */



/*****************************************************************************
 * xvid_image_t
 ****************************************************************************/

/* colorspace values */

//#define XVID_CSP_USER     (1<< 0) /* 4:2:0 planar */
//#define XVID_CSP_I420     (1<< 1) /* 4:2:0 packed(planar win32) */
//#define XVID_CSP_YV12     (1<< 2) /* 4:2:0 packed(planar win32) */
//#define XVID_CSP_YUY2     (1<< 3) /* 4:2:2 packed */
//#define XVID_CSP_UYVY     (1<< 4) /* 4:2:2 packed */
//#define XVID_CSP_YVYU     (1<< 5) /* 4:2:2 packed */
//#define XVID_CSP_BGRA     (1<< 6) /* 32-bit bgra packed */
//#define XVID_CSP_ABGR     (1<< 7) /* 32-bit abgr packed */
//#define XVID_CSP_RGBA     (1<< 8) /* 32-bit rgba packed */
//#define XVID_CSP_BGR      (1<< 9) /* 24-bit bgr packed */
//#define XVID_CSP_RGB555   (1<<10) /* 16-bit rgb555 packed */
//#define XVID_CSP_RGB565   (1<<11) /* 16-bit rgb565 packed */
//#define XVID_CSP_SLICE    (1<<12) /* decoder only: 4:2:0 planar, per slice rendering */
//#define XVID_CSP_INTERNAL (1<<13) /* decoder only: 4:2:0 planar, returns ptrs to internal buffers */
//#define XVID_CSP_NULL     (1<<14) /* decoder only: dont output anything */
//#define XVID_CSP_VFLIP    (1<<31) /* vertical flip mask */

/* xvid_image_t
	for non-planar colorspaces use only plane[0] and stride[0]
	four plane reserved for alpha*/
/*
typedef struct {
	int csp;				// [in] colorspace; or with XVID_CSP_VFLIP to perform vertical flip
	void *plane;		// [in] image plane ptrs
	int stride;			// [in] image stride; "bytes per row"
} xvid_image_t;
*/

/* video-object-sequence profiles */
#define XVID_PROFILE_S_L0    0x08 /* simple */
#define XVID_PROFILE_S_L1    0x01
#define XVID_PROFILE_S_L2    0x02
#define XVID_PROFILE_S_L3    0x03
#define XVID_PROFILE_ARTS_L1 0x91 /* advanced realtime simple */
#define XVID_PROFILE_ARTS_L2 0x92
#define XVID_PROFILE_ARTS_L3 0x93
#define XVID_PROFILE_ARTS_L4 0x94
#define XVID_PROFILE_AS_L0   0xf0 /* advanced simple */
#define XVID_PROFILE_AS_L1   0xf1
#define XVID_PROFILE_AS_L2   0xf2
#define XVID_PROFILE_AS_L3   0xf3
#define XVID_PROFILE_AS_L4   0xf4

/* aspect ratios */
#define XVID_PAR_11_VGA    1 /* 1:1 vga (square), default if AR is not precised (ie: ==0) */
#define XVID_PAR_43_PAL    2 /* 4:3 pal (12:11 625-line) */
#define XVID_PAR_43_NTSC   3 /* 4:3 ntsc (10:11 525-line) */
#define XVID_PAR_169_PAL   4 /* 16:9 pal (16:11 625-line) */
#define XVID_PAR_169_NTSC  5 /* 16:9 ntsc (40:33 525-line) */
#define XVID_PAR_EXT      15 /* extended par; use par_width, par_height */

/* frame type flags */
#define XVID_TYPE_VOL     -1 /* decoder only: vol was decoded */
#define XVID_TYPE_NOTHING  0 /* decoder only (encoder stats): nothing was decoded/encoded */
#define XVID_TYPE_AUTO     0 /* encoder: automatically determine coding type */
#define XVID_TYPE_IVOP     1 /* intra frame */
#define XVID_TYPE_PVOP     2 /* predicted frame */
#define XVID_TYPE_BVOP     3 /* bidirectionally encoded */
#define XVID_TYPE_SVOP     4 /* predicted+sprite frame */


/*****************************************************************************
 * xvid_global()
 ****************************************************************************/

/* cpu_flags definitions (make sure to sync this with cpuid.asm for ia32) */

//#define XVID_CPU_FORCE    (1<<31) /* force passed cpu flags */
//#define XVID_CPU_ASM      (1<< 7) /* native assembly */
/* ARCH_IS_IA32 */
//#define XVID_CPU_MMX      (1<< 0) /*       mmx : pentiumMMX,k6 */
//#define XVID_CPU_MMXEXT   (1<< 1) /*   mmx-ext : pentium2, athlon */
//#define XVID_CPU_SSE      (1<< 2) /*       sse : pentium3, athlonXP */
//#define XVID_CPU_SSE2     (1<< 3) /*      sse2 : pentium4, athlon64 */
//#define XVID_CPU_3DNOW    (1<< 4) /*     3dnow : k6-2 */
//#define XVID_CPU_3DNOWEXT (1<< 5) /* 3dnow-ext : athlon */
//#define XVID_CPU_TSC      (1<< 6) /*       tsc : Pentium */
/* ARCH_IS_PPC */
//#define XVID_CPU_ALTIVEC  (1<< 0) /* altivec */


#define XVID_DEBUG_ERROR     (1<< 0)
#define XVID_DEBUG_STARTCODE (1<< 1)
#define XVID_DEBUG_HEADER    (1<< 2)
#define XVID_DEBUG_TIMECODE  (1<< 3)
#define XVID_DEBUG_MB        (1<< 4)
#define XVID_DEBUG_COEFF     (1<< 5)
#define XVID_DEBUG_MV        (1<< 6)
#define XVID_DEBUG_RC        (1<< 7)
#define XVID_DEBUG_DEBUG     (1<<31)

/* XVID_GBL_INIT param1 */
typedef struct {
	int version;
	unsigned int cpu_flags; /* [in:opt] zero = autodetect cpu; XVID_CPU_FORCE|{cpu features} = force cpu features */
	int debug;     /* [in:opt] debug level */
} xvid_gbl_init_t;


/* XVID_GBL_INFO param1 */
#if 0
typedef struct {
	int version;
	int actual_version; // [out] returns the actual xvidcore version
	const char * build; // [out] if !null, points to description of this xvid core build
	unsigned int cpu_flags;      // [out] detected cpu features
	int num_threads;    // [out] detected number of cpus/threads
} xvid_gbl_info_t;
#endif


#define XVID_GBL_INIT    0 /* initialize xvidcore; must be called before using xvid_decore, or xvid_encore) */
//#define XVID_GBL_INFO    1 /* return some info about xvidcore, and the host computer */
//#define XVID_GBL_CONVERT 2 /* colorspace conversion utility */

int xvid_global(void *handle, int opt, void *param1, void *param2);


class C_xvid_image {
public:
	byte *y;
	byte *u;
	byte *v;

	C_xvid_image():
		y(NULL),
		u(NULL),
		v(NULL)
	{}
};

void * InitCodec(dword sx, dword sy, dword fcc);
void CloseCodec(void *handle);
//int DecodeFrame(void *handle, const void *buf, dword sz_in, byte **y, byte **u, byte **v, dword *pitch);
int DecodeFrame(void *handle, const void *buf, dword sz_in, byte *&y, byte *&u, byte *&v, dword &pitch);

/*****************************************************************************
 * xvid_decore()
 ****************************************************************************/

#define XVID_DEC_CREATE  0 /* create decore instance; return 0 on success */
#define XVID_DEC_DESTROY 1 /* destroy decore instance: return 0 on success */
#define XVID_DEC_DECODE  2 /* decode a frame: returns number of bytes consumed >= 0 */

int xvid_decore(void *handle, int opt, void *param1, void *param2);

/* XVID_DEC_CREATE param 1
	image width & height may be specified here when the dimensions are
	known in advance. */
typedef struct {
	int version;
	int width;     /* [in:opt] image width */
	int height;    /* [in:opt] image width */
	void *handle; /* [out]	   decore context handle */
#ifdef PROFILE
	C_profiler *prof;
#endif
} xvid_dec_create_t;


/* XVID_DEC_DECODE param1 */
/* general flags */
#define XVID_LOWDELAY      (1<<0) /* lowdelay mode  */
#define XVID_DISCONTINUITY (1<<1) /* indicates break in stream */

typedef struct {
	int version;
	int general;         /* [in:opt] general flags */
	const void *bitstream;     /* [in]     bitstream (read from)*/
	int length;          /* [in]     bitstream length */
	//xvid_image_t output; /* [in]     output image (written to) */
	const C_xvid_image *img_out;
} xvid_dec_frame_t;


/* XVID_DEC_DECODE param2 :: optional */
typedef struct
{
	int version;

	int type;                   /* [out] output data type */
	union {
		struct { /* type>0 {XVID_TYPE_IVOP,XVID_TYPE_PVOP,XVID_TYPE_BVOP,XVID_TYPE_SVOP} */
			int general;        /* [out] flags */
			int time_base;      /* [out] time base */
			int time_increment; /* [out] time increment */

			/* XXX: external deblocking stuff */
			int * qscale;	    /* [out] pointer to quantizer table */
			int qscale_stride;  /* [out] quantizer scale stride */

		} vop;
		struct {	/* XVID_TYPE_VOL */
			int general;        /* [out] flags */
			int width;          /* [out] width */
			int height;         /* [out] height */
			int par;            /* [out] picture aspect ratio (refer to XVID_PAR_xxx above) */
			int par_width;      /* [out] aspect ratio width */
			int par_height;     /* [out] aspect ratio height */
		} vol;
	} data;
} xvid_dec_stats_t;



#define XVID_ZONE_QUANT  (1<<0)
#define XVID_ZONE_WEIGHT (1<<1)

typedef struct
{
	int frame;
	int mode;
	int increment;
	int base;
} xvid_enc_zone_t;



/*****************************************************************************
  xvid plugin system -- internals

  xvidcore will call XVID_PLG_INFO and XVID_PLG_CREATE during XVID_ENC_CREATE
  before encoding each frame xvidcore will call XVID_PLG_BEFORE
  after encoding each frame xvidcore will call XVID_PLG_AFTER
  xvidcore will call XVID_PLG_DESTROY during XVID_ENC_DESTROY
 ****************************************************************************/


#define XVID_PLG_CREATE  (1<<0)
#define XVID_PLG_DESTROY (1<<1)
#define XVID_PLG_INFO    (1<<2)
#define XVID_PLG_BEFORE  (1<<3)
#define XVID_PLG_FRAME   (1<<4)
#define XVID_PLG_AFTER   (1<<5)

/* xvid_plg_info_t.flags */
#define XVID_REQORIGINAL (1<<0) /* plugin requires a copy of the original (uncompressed) image */
#define XVID_REQPSNR     (1<<1) /* plugin requires psnr between the uncompressed and compressed image*/
#define XVID_REQDQUANTS  (1<<2) /* plugin requires access to the dquant table */


typedef struct
{
	int version;
	int flags;   /* [in:opt] plugin flags */
} xvid_plg_info_t;


typedef struct
{
	int version;

	int num_zones;           /* [out] */
	xvid_enc_zone_t * zones; /* [out] */

	int width;               /* [out] */
	int height;              /* [out] */
	int mb_width;            /* [out] */
	int mb_height;           /* [out] */
	int fincr;               /* [out] */
	int fbase;               /* [out] */

	void * param;            /* [out] */
} xvid_plg_create_t;


typedef struct
{
	int version;

	int num_frames; /* [out] total frame encoded */
} xvid_plg_destroy_t;



/*****************************************************************************
  xvid plugin system -- external

  the application passes xvid an array of "xvid_plugin_t" at XVID_ENC_CREATE. the array
  indicates the plugin function pointer and plugin-specific data.
  xvidcore handles the rest. example:

  xvid_enc_create_t create;
  xvid_enc_plugin_t plugins[2];

  plugins[0].func = xvid_psnr_func;
  plugins[0].param = NULL;
  plugins[1].func = xvid_cbr_func;
  plugins[1].param = &cbr_data;

  create.num_plugins = 2;
  create.plugins = plugins;

 ****************************************************************************/

typedef int (xvid_plugin_func)(void * handle, int opt, void * param1, void * param2);

typedef struct
{
	xvid_plugin_func * func;
	void * param;
} xvid_enc_plugin_t;


xvid_plugin_func xvid_plugin_single;   /* single-pass rate control */
xvid_plugin_func xvid_plugin_2pass1;   /* two-pass rate control: first pass */
xvid_plugin_func xvid_plugin_2pass2;   /* two-pass rate control: second pass */

xvid_plugin_func xvid_plugin_lumimasking;  /* lumimasking */

xvid_plugin_func xvid_plugin_psnr;	/* write psnr values to stdout */
xvid_plugin_func xvid_plugin_dump;	/* dump before and after yuvpgms */


/* single pass rate control
 * CBR and Constant quantizer modes */
typedef struct
{
	int version;

	int bitrate;               /* [in] bits per second */
	int reaction_delay_factor; /* [in] */
	int averaging_period;      /* [in] */
	int buffer;                /* [in] */
} xvid_plugin_single_t;


typedef struct {
	int version;

	char * filename;
} xvid_plugin_2pass1_t;


#define XVID_PAYBACK_BIAS 0 /* payback with bias */
#define XVID_PAYBACK_PROP 1 /* payback proportionally */

typedef struct {
	int version;

	int bitrate;                  /* [in] bits per second */
	char * filename;              /* [in] first pass stats filename */

	int keyframe_boost;           /* [in] keyframe boost percentage: [0..100] */
	int curve_compression_high;   /* [in] percentage of compression performed on the high part of the curve (above average) */
	int curve_compression_low;    /* [in] percentage of compression performed on the low  part of the curve (below average) */
	int overflow_control_strength;/* [in] Payback delay expressed in number of frames */
	int max_overflow_improvement; /* [in] percentage of allowed range for a frame that gets bigger because of overflow bonus */
	int max_overflow_degradation; /* [in] percentage of allowed range for a frame that gets smaller because of overflow penalty */

	int kfreduction;              /* [in] maximum bitrate reduction applied to an iframe under the kfthreshold distance limit */
	int kfthreshold;              /* [in] if an iframe is closer to the next iframe than this distance, a quantity of bits
								   *      is substracted from its bit allocation. The reduction is computed as multiples of
								   *      kfreduction/kthreshold. It reaches kfreduction when the distance == kfthreshold,
								   *      0 for 1<distance<kfthreshold */

	int container_frame_overhead; /* [in] How many bytes the controller has to compensate per frame due to container format overhead */
} xvid_plugin_2pass2_t;


#ifdef __cplusplus
}
#endif

#endif
