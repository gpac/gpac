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


#include <windows.h>
#include "gdip_priv.h"

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

struct _graphics
{
	GpGraphics *graph;
	GpMatrix *mat;
	GpPath *clip;
	GpPath *current;
	u32 w, h;
	Bool center_coords;

	/*offscreen buffer handling*/
	GpBitmap *pBitmap;
};

GpPath *gdip_create_path(GF_Path *_this)
{
	GpPath *p;
	u32 j, i, nb_pts, cur;
	if (!_this || !_this->n_points) return NULL;
	GdipCreatePath(FillModeAlternate, &p);

	GdipSetPathFillMode(p, (_this->flags & GF_PATH_FILL_ZERO_NONZERO) ? FillModeWinding : FillModeAlternate);

	cur = 0;
	for (i=0; i<_this->n_contours; i++) {
		nb_pts = 1+_this->contours[i] - cur;
		GdipStartPathFigure(p);
		for (j=cur+1; j<cur+nb_pts; ) {
			switch (_this->tags[j]) {
			case GF_PATH_CURVE_ON:
				GdipAddPathLine(p, FIX2FLT(_this->points[j-1].x), FIX2FLT(_this->points[j-1].y), FIX2FLT(_this->points[j].x), FIX2FLT(_this->points[j].y));
				j++;
				break;
			case GF_PATH_CLOSE:
				GdipAddPathLine(p, FIX2FLT(_this->points[j].x), FIX2FLT(_this->points[j].y), FIX2FLT(_this->points[cur].x), FIX2FLT(_this->points[cur].y));
				j++;
				break;
			case GF_PATH_CURVE_CUBIC:
				GdipAddPathBezier(p,
				                  FIX2FLT(_this->points[j-1].x), FIX2FLT(_this->points[j-1].y),
				                  FIX2FLT(_this->points[j].x), FIX2FLT(_this->points[j].y),
				                  FIX2FLT(_this->points[j+1].x), FIX2FLT(_this->points[j+1].y),
				                  FIX2FLT(_this->points[j+2].x), FIX2FLT(_this->points[j+2].y)
				                 );
				j+=3;
				break;
			case GF_PATH_CURVE_CONIC:
			{
				GF_Point2D ctl, end, c1, c2, start;
				start = _this->points[j-1];
				ctl = _this->points[j];
				end = _this->points[j+1];
				c1.x = start.x + 2*(ctl.x - start.x) / 3;
				c1.y = start.y + 2*(ctl.y - start.y) / 3;
				c2.x = c1.x + (end.x - start.x) / 3;
				c2.y = c1.y + (end.y - start.y) / 3;
				GdipAddPathBezier(p,
				                  FIX2FLT(start.x), FIX2FLT(start.y),
				                  FIX2FLT(c1.x), FIX2FLT(c1.y),
				                  FIX2FLT(c2.x), FIX2FLT(c2.y),
				                  FIX2FLT(end.x), FIX2FLT(end.y)
				                 );
				j+=2;
			}
			break;
			}
		}
		GdipClosePathFigure(p);
		cur += nb_pts;
	}
	return p;
}

#define GPGRAPH() struct _graphics *_graph = (struct _graphics *)_this;

static
GF_SURFACE gdip_new_surface(GF_Raster2D *, Bool center_coords)
{
	struct _graphics *graph;
	SAFEALLOC(graph, struct _graphics);
	graph->center_coords = center_coords;
	return graph;
}

static
void gdip_delete_surface(GF_SURFACE _this)
{
	GPGRAPH();
	gf_free(_graph);
}

/*should give the best results with the clippers*/
#define GDIP_PIXEL_MODE PixelOffsetModeHighQuality

static
GF_Err gdip_attach_surface_to_device(GF_SURFACE _this, void *os_handle, u32 width, u32 height)
{
	GpMatrix *mat;
	HDC handle = (HDC) os_handle;
	GPGRAPH();
	if (!_graph || !handle) return GF_BAD_PARAM;
	if (_graph->graph) return GF_BAD_PARAM;
	GdipCreateFromHDC(handle, &_graph->graph);

	GdipCreateMatrix(&mat);
	if (	_graph->center_coords) {
		GdipScaleMatrix(mat, 1.0, -1.0, MatrixOrderAppend);
		GdipTranslateMatrix(mat, (Float) width/2, (Float) height/2, MatrixOrderAppend);
	}
	GdipSetWorldTransform(_graph->graph, mat);
	GdipDeleteMatrix(mat);
	_graph->w = width;
	_graph->h = height;
	GdipSetPixelOffsetMode(_graph->graph, GDIP_PIXEL_MODE);
	return GF_OK;
}
static
GF_Err gdip_attach_surface_to_texture(GF_SURFACE _this, GF_STENCIL sten)
{
	GpMatrix *mat;
	struct _stencil *_sten = (struct _stencil *)sten;
	GPGRAPH();
	if (!_graph || !_sten || !_sten->pBitmap) return GF_BAD_PARAM;

	GdipGetImageGraphicsContext(_sten->pBitmap, &_graph->graph);

	if (_graph->center_coords) {
		GdipCreateMatrix(&mat);
		GdipScaleMatrix(mat, 1.0, -1.0, MatrixOrderAppend);
		GdipTranslateMatrix(mat, (Float) _sten->width/2, (Float) _sten->height/2, MatrixOrderAppend);
		GdipSetWorldTransform(_graph->graph, mat);
		GdipDeleteMatrix(mat);
	}
	_graph->w = _sten->width;
	_graph->h = _sten->height;
	GdipSetPixelOffsetMode(_graph->graph, GDIP_PIXEL_MODE);
	return GF_OK;
}
static
GF_Err gdip_attach_surface_to_buffer(GF_SURFACE _this, char *pixels, u32 width, u32 height, s32 pitch_x, s32 pitch_y, GF_PixelFormat pixelFormat)
{
	GpMatrix *mat;
	u32 pFormat;
	GPGRAPH();

	if (pitch_y%4) return GF_NOT_SUPPORTED;

	switch (pixelFormat) {
	case GF_PIXEL_ALPHAGREY:
		pFormat = PixelFormat16bppGrayScale;
		if (pitch_x != 2) return GF_NOT_SUPPORTED;
		break;
	case GF_PIXEL_RGB_555:
		pFormat = PixelFormat16bppRGB555;
		if (pitch_x != 2) return GF_NOT_SUPPORTED;
		break;
	case GF_PIXEL_RGB_565:
		pFormat = PixelFormat16bppRGB565;
		if (pitch_x != 2) return GF_NOT_SUPPORTED;
		break;
	case GF_PIXEL_RGB_24:
		pFormat = PixelFormat24bppRGB;
		if (pitch_x != 3) return GF_NOT_SUPPORTED;
		break;
	case GF_PIXEL_RGB_32:
		pFormat = PixelFormat32bppRGB;
		if (pitch_x != 4) return GF_NOT_SUPPORTED;
		break;
	case GF_PIXEL_ARGB:
		pFormat = PixelFormat32bppARGB;
		if (pitch_x != 4) return GF_NOT_SUPPORTED;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	GdipCreateBitmapFromScan0(width, height, pitch_y, pFormat, (unsigned char*)pixels, &_graph->pBitmap);
	GdipGetImageGraphicsContext(_graph->pBitmap, &_graph->graph);

	_graph->w = width;
	_graph->h = height;
	if (_graph->center_coords) {
		GdipCreateMatrix(&mat);
		GdipScaleMatrix(mat, 1.0, -1.0, MatrixOrderAppend);
		GdipTranslateMatrix(mat, (Float) width/2, (Float) height/2, MatrixOrderAppend);
		GdipSetWorldTransform(_graph->graph, mat);
		GdipDeleteMatrix(mat);
	}
	GdipSetPixelOffsetMode(_graph->graph, GDIP_PIXEL_MODE);
	return GF_OK;
}

static
void gdip_detach_surface(GF_SURFACE _this)
{
	GPGRAPH();
	if (_graph->graph) GdipDeleteGraphics(_graph->graph);
	_graph->graph = NULL;
	if (_graph->clip) GdipDeletePath(_graph->clip);
	_graph->clip = NULL;
	if (_graph->pBitmap) GdipDisposeImage(_graph->pBitmap);
	_graph->pBitmap = NULL;
	if (_graph->current) GdipDeletePath(_graph->current);
	_graph->current = NULL;
}


static
GF_Err gdip_surface_set_raster_level(GF_SURFACE _this, GF_RasterLevel RasterSetting)
{
	GPGRAPH();
	switch (RasterSetting) {
	case GF_RASTER_HIGH_SPEED:
		GdipSetSmoothingMode(_graph->graph, SmoothingModeHighSpeed);
		GdipSetCompositingQuality(_graph->graph, CompositingQualityHighSpeed);
		break;
	case GF_RASTER_MID:
		GdipSetSmoothingMode(_graph->graph, SmoothingModeDefault);
		GdipSetCompositingQuality(_graph->graph, CompositingQualityDefault);
		break;
	case GF_RASTER_HIGH_QUALITY:
		GdipSetSmoothingMode(_graph->graph, SmoothingModeHighQuality);
		GdipSetCompositingQuality(_graph->graph, CompositingQualityDefault);
		/*THIS IS HORRIBLY SLOW DON'T EVEN THINK ABOUT IT*/
		/*GdipSetCompositingQuality(_graph->graph, CompositingQualityHighQuality);*/
		break;
	}
	return GF_OK;
}
static
GF_Err gdip_surface_set_matrix(GF_SURFACE _this, GF_Matrix2D * mat)
{
	GPGRAPH();
	if (_graph->mat) GdipDeleteMatrix(_graph->mat);

	_graph->mat = mat_gpac_to_gdip(mat);
	return GF_OK;
}
static
GF_Err gdip_surface_set_clipper(GF_SURFACE _this, GF_IRect *rc)
{
	GPGRAPH();
	if (_graph->clip) GdipDeletePath(_graph->clip);
	_graph->clip = 0L;
	if (!rc) return GF_OK;

	GdipCreatePath(FillModeAlternate, &_graph->clip);
	GdipAddPathRectangleI(_graph->clip, rc->x, rc->y - rc->height, rc->width, rc->height);
	return GF_OK;
}

static
GpBrush *gdip_get_brush(struct _stencil *_sten)
{
	if (_sten->pSolid) return _sten->pSolid;
	if (_sten->pLinear) return _sten->pLinear;
	if (_sten->pRadial) return _sten->pRadial;
	if (_sten->pTexture) return _sten->pTexture;
	return NULL;
}

static GpPath *gdip_setup_path(struct _graphics *_this, GF_Path *path)
{
	GpPath *tr = gdip_create_path(path);
	/*append current matrix*/
	if (_this->mat) GdipTransformPath(tr, _this->mat);
	return tr;
}

static
GF_Err gdip_surface_set_path(GF_SURFACE _this, GF_Path *path)
{
	struct _storepath *_path;
	GPGRAPH();
	if (!_graph) return GF_BAD_PARAM;
	if (_graph->current) GdipDeletePath(_graph->current);
	_graph->current = NULL;
	if (!path) return GF_OK;

	_path = (struct _storepath *)path;
	_graph->current = gdip_setup_path(_graph, path);
	return GF_OK;
}

//#define NODRAW

static
GF_Err gdip_surface_fill(GF_SURFACE _this, GF_STENCIL stencil)
{
	GpStatus ret;
	GpMatrix *newmat;
	struct _stencil *_sten;
	GPGRAPH();
	if (!_this) return GF_BAD_PARAM;
	if (!_graph->current) return GF_OK;
	_sten = (struct _stencil *)stencil;
	assert(_sten);

#ifdef NODRAW
	return GF_OK;
#endif


	if (_graph->clip) GdipSetClipPath(_graph->graph, _graph->clip, CombineModeReplace);

	switch (_sten->type) {
	case GF_STENCIL_SOLID:
		assert(_sten->pSolid);
		GdipFillPath(_graph->graph, _sten->pSolid, _graph->current);
		break;
	case GF_STENCIL_LINEAR_GRADIENT:
		if (_sten->pMat) {
			/*rebuild gradient*/
			gdip_recompute_line_gradient(_sten);

			GdipResetTextureTransform((GpTexture*)_sten->pLinear);
			if (_sten->pMat) {
				GdipCloneMatrix(_sten->pMat, &newmat);
			} else {
				GdipCreateMatrix(&newmat);
			}
			GdipMultiplyMatrix(newmat, _sten->pLinearMat, MatrixOrderPrepend);
			GdipSetTextureTransform((GpTexture*)_sten->pLinear, newmat);
			GdipDeleteMatrix(newmat);
		}
		GdipFillPath(_graph->graph, _sten->pLinear, _graph->current);
		break;
	case GF_STENCIL_RADIAL_GRADIENT:
		/*build gradient*/
		gdip_recompute_radial_gradient(_sten);

		GdipSetCompositingQuality(_graph->graph, CompositingQualityHighSpeed);
		GdipSetInterpolationMode(_graph->graph, InterpolationModeLowQuality);
		GdipSetSmoothingMode(_graph->graph, SmoothingModeHighSpeed);

		/*check if we need to draw solid background (GDIplus doesn't implement padded mode on path gradients)*/
		if (_sten->pSolid) {
			GpPath *tr;
			GdipClonePath(_sten->circle, &tr);
			GdipTransformPath(tr, _sten->pMat);
			GdipSetClipPath(_graph->graph, tr, CombineModeExclude);
			GdipFillPath(_graph->graph, _sten->pSolid, _graph->current);
			GdipDeletePath(tr);
			GdipResetClip(_graph->graph);
			if (_graph->clip) GdipSetClipPath(_graph->graph, _graph->clip, CombineModeReplace);
		}
		GdipFillPath(_graph->graph, _sten->pRadial, _graph->current);
		break;
	case GF_STENCIL_VERTEX_GRADIENT:
		assert(_sten->pRadial);
		if (_sten->pMat) GdipSetTextureTransform((GpTexture*)_sten->pRadial, _sten->pMat);
		ret = GdipFillPath(_graph->graph, _sten->pRadial, _graph->current);
		break;
	case GF_STENCIL_TEXTURE:
		gdip_load_texture(_sten);
		if (_sten->pTexture) {
			GpMatrix *newmat;
			GdipResetTextureTransform((GpTexture*)_sten->pTexture);
			if (_sten->pMat) {
				GdipCloneMatrix(_sten->pMat, &newmat);
			} else {
				GdipCreateMatrix(&newmat);
			}
			/*gdip flip*/
			if (_graph->center_coords && !(_sten->tiling&GF_TEXTURE_FLIP) )
				GdipScaleMatrix(newmat, 1, -1, MatrixOrderPrepend);
			else if (!_graph->center_coords && (_sten->tiling&GF_TEXTURE_FLIP) )
				GdipScaleMatrix(newmat, 1, -1, MatrixOrderPrepend);

			GdipSetTextureTransform((GpTexture*)_sten->pTexture, newmat);
			GdipDeleteMatrix(newmat);

			GdipSetInterpolationMode(_graph->graph, (_sten->tFilter==GF_TEXTURE_FILTER_HIGH_QUALITY) ? InterpolationModeHighQuality : InterpolationModeLowQuality);
			GdipFillPath(_graph->graph, _sten->pTexture, _graph->current);
		}
		break;
	}
	return GF_OK;
}


static
GF_Err gdip_surface_flush(GF_SURFACE _this)
{
	GPGRAPH();
	GdipFlush(_graph->graph, FlushIntentionSync);
	return GF_OK;
}

static
GF_Err gdip_surface_clear(GF_SURFACE _this, GF_IRect *rc, u32 color)
{
	GpPath *path;
	GPGRAPH();

	GdipCreatePath(FillModeAlternate, &path);
	if (rc) {
		/*luckily enough this maps well for both flipped and unflipped coords*/
		GdipAddPathRectangleI(path, rc->x, rc->y - rc->height, rc->width, rc->height);
	} else {
		/*		if (_graph->center_coords) {
					GdipAddPathRectangleI(path, -1 * (s32)_graph->w / 2, -1 * (s32)_graph->h / 2, _graph->w, _graph->h);
				} else {
		*/			GdipAddPathRectangleI(path, 0, 0, _graph->w, _graph->h);
//		}
	}
	/*we MUST use clear otherwise ARGB surfaces are not cleared correctly*/
	GdipSetClipPath(_graph->graph, path, CombineModeReplace);
	GdipGraphicsClear(_graph->graph, color);
	GdipDeletePath(path);
	return GF_OK;
}

void gdip_init_driver_surface(GF_Raster2D *driver)
{
	driver->surface_new = gdip_new_surface;
	driver->surface_delete = gdip_delete_surface;
	driver->surface_attach_to_device = gdip_attach_surface_to_device;
	driver->surface_attach_to_texture = gdip_attach_surface_to_texture;
	driver->surface_attach_to_buffer = gdip_attach_surface_to_buffer;
	driver->surface_detach = gdip_detach_surface;
	driver->surface_set_raster_level = gdip_surface_set_raster_level;
	driver->surface_set_matrix = gdip_surface_set_matrix;
	driver->surface_set_clipper = gdip_surface_set_clipper;
	driver->surface_set_path = gdip_surface_set_path;
	driver->surface_fill = gdip_surface_fill;
	driver->surface_flush = gdip_surface_flush;
	driver->surface_clear = gdip_surface_clear;
}


GF_Raster2D *gdip_LoadRenderer()
{
	GdiplusStartupInput startupInput;
	GF_Raster2D *driver;
	struct _gdip_context *ctx;
	SAFEALLOC(ctx, struct _gdip_context);
	SAFEALLOC(driver, GF_Raster2D);
	GdiplusStartup(&ctx->gdiToken, &startupInput, NULL);
	driver->internal = ctx;
	GF_REGISTER_MODULE_INTERFACE(driver, GF_RASTER_2D_INTERFACE, "GDIplus 2D Raster", "gpac distribution")
	gdip_init_driver_texture(driver);
	gdip_init_driver_surface(driver);
	gdip_init_driver_grad(driver);
	return driver;
}

void gdip_ShutdownRenderer(GF_Raster2D *driver)
{
	struct _gdip_context *ctx = (struct _gdip_context *)driver->internal;

	GdiplusShutdown(ctx->gdiToken);
	gf_free(ctx);
	gf_free(driver);
}
