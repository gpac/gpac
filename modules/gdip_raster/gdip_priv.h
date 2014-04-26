/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / GDIplus rasterizer module
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


#ifndef __GDIP_PRIV_H
#define __GDIP_PRIV_H

#include <math.h>
#include <gpac/modules/raster2d.h>
#include <gpac/modules/font.h>
#include <windows.h>


#define SAFEALLOC(__ptr, __struc) __ptr = (__struc*)gf_malloc(sizeof(__struc)); if (__ptr) memset(__ptr, 0, sizeof(__struc));

/*all GDIPLUS includes for C api*/

struct IDirectDrawSurface7;

#include "GdiplusMem.h"
#include "GdiplusEnums.h"
#include "GdiplusTypes.h"
#include "GdiplusInit.h"
#include "GdiplusPixelFormats.h"
#include "GdiplusColor.h"
#include "GdiplusMetaHeader.h"
#include "GdiplusImaging.h"
#include "GdiplusColorMatrix.h"
#include "GdiplusGpStubs.h"
#include "GdiplusColor.h"
#include "GdiplusFlat.h"

#include <math.h>

#define GD_PI		3.1415926536f

/* default resolution for N-bezier curves*/
#define GDIP_DEFAULT_RESOLUTION		64

struct _gdip_context
{
	ULONG_PTR gdiToken;
};


/*struct translators*/

GFINLINE GpMatrix *mat_gpac_to_gdip(GF_Matrix2D *mat)
{
	GpMatrix *ret;
	if (!mat) return NULL;
	GdipCreateMatrix(&ret);
	GdipSetMatrixElements(ret, FIX2FLT(mat->m[0]), FIX2FLT(mat->m[3]), FIX2FLT(mat->m[1]), FIX2FLT(mat->m[4]), FIX2FLT(mat->m[2]), FIX2FLT(mat->m[5]));
	return ret;
}


GFINLINE void cmat_gpac_to_gdip(GF_ColorMatrix *mat, ColorMatrix *matrix)
{
	memset(matrix->m, 0, sizeof(Float)*5*5);
	matrix->m[0][0] = FIX2FLT(mat->m[0]);
	matrix->m[1][0] = FIX2FLT(mat->m[1]);
	matrix->m[2][0] = FIX2FLT(mat->m[2]);
	matrix->m[3][0] = FIX2FLT(mat->m[3]);
	matrix->m[4][0] = FIX2FLT(mat->m[4]);
	matrix->m[0][1] = FIX2FLT(mat->m[5]);
	matrix->m[1][1] = FIX2FLT(mat->m[6]);
	matrix->m[2][1] = FIX2FLT(mat->m[7]);
	matrix->m[3][1] = FIX2FLT(mat->m[8]);
	matrix->m[4][1] = FIX2FLT(mat->m[9]);
	matrix->m[0][2] = FIX2FLT(mat->m[10]);
	matrix->m[1][2] = FIX2FLT(mat->m[11]);
	matrix->m[2][2] = FIX2FLT(mat->m[12]);
	matrix->m[3][2] = FIX2FLT(mat->m[13]);
	matrix->m[4][2] = FIX2FLT(mat->m[14]);
	matrix->m[0][3] = FIX2FLT(mat->m[15]);
	matrix->m[1][3] = FIX2FLT(mat->m[16]);
	matrix->m[2][3] = FIX2FLT(mat->m[17]);
	matrix->m[3][3] = FIX2FLT(mat->m[18]);
	matrix->m[4][3] = FIX2FLT(mat->m[19]);
}


GFINLINE void gdip_cmat_reset(ColorMatrix *matrix)
{
	memset(matrix->m, 0, sizeof(Float)*5*5);
	matrix->m[0][0] = matrix->m[1][1] = matrix->m[2][2] = matrix->m[3][3] = matrix->m[4][4] = 1.0;
}

#define GPMATRIX() GpMatrix * _mat = mat_gpac_to_gdip(mat);

GpPath *gdip_create_path(GF_Path * _this);

struct _stencil
{
	GF_StencilType type;
	GF_GradientMode spread;
	GF_TextureTiling tiling;

	GpSolidFill *pSolid;

	GpMatrix *pMat;

	/*Linear gradient vars*/
	GpLineGradient *pLinear;
	GpMatrix *pLinearMat;
	GpPointF start, end;

	/*Radial gradient vars*/
	GpPathGradient *pRadial;
	GpPointF center, radius, focal;
	GpPath *circle;

	/*interpolation colors storage*/
	REAL *pos;
	ARGB *cols;
	u32 num_pos;
	Bool needs_rebuild;

	/*texture specific*/
	GpTexture *pTexture;
	GpBitmap *pBitmap;
	u32 width, height;
	ColorMatrix cmat;
	Bool has_cmat;
	PixelFormat format;
	/*GDIplus is expecting ABGR when creating a bitmap with GdipCreateBitmapFromScan0.
	Since we don't want to rewrite by hand the full image when loading textures, we
	force R->B switching */
	Bool invert_br;
	GF_TextureFilter tFilter;

	Bool texture_invalid;
	GF_Rect wnd;
	u8 alpha;

	unsigned char *conv_buf;
	u32 conv_size;
	unsigned char *orig_buf;
	u32 orig_stride, orig_format;
	Bool is_converted;
	/*not used yet, we only convert to RGB or ARGB*/
	u32 destination_format;
};
#define GPSTEN() struct _stencil *_sten = (struct _stencil *) _this; assert(_this);
#define CHECK(_type) if (_sten->type!=_type) return;
#define CHECK_RET(_type) if (_sten->type!=_type) return GF_BAD_PARAM;
#define CHECK2(_t1, _t2) if ((_sten->type!=_t1) && (_sten->type!=_t2)) return;
#define CHECK2_RET(_t1, _t2) if ((_sten->type!=_t1) && (_sten->type!=_t2)) return GF_BAD_PARAM;

void gdip_recompute_line_gradient(GF_STENCIL _this);
void gdip_recompute_radial_gradient(GF_STENCIL _this);

void gdip_load_texture(struct _stencil *sten);

void gdip_init_driver_texture(GF_Raster2D *driver);
void gdip_init_driver_common(GF_Raster2D *driver);
void gdip_init_driver_grad(GF_Raster2D *driver);

typedef struct
{
	ULONG_PTR gdiToken;

	/*text stuff*/
	Float em_size, descent, ascent;
	s32 font_style;
	Float whitespace_width;
	Float underscore_width;
	GpFontFamily *font;

	char font_serif[1024];
	char font_sans[1024];
	char font_fixed[1024];
} FontPriv;

GF_FontReader *gdip_new_font_driver();
void gdip_delete_font_driver(GF_FontReader *dr);


GF_Raster2D *gdip_LoadRenderer();
void gdip_ShutdownRenderer(GF_Raster2D *driver);

#endif	//__GDIP_PRIV_H