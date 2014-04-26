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


#include "gdip_priv.h"

#define COL_565(c) GF_COL_ARGB(0xFF, (u8) ( (val >> 7) & 0xf8), (u8) ( (val >> 2) & 0xf8), (u8) ( (val << 3) & 0xf8) )
#define COL_555(c) GF_COL_ARGB(0xFF,  (u8) ( (val >> 8) & 0xf8), (u8) ( (val >> 3) & 0xfc),  (u8) ( (val << 3) & 0xf8)	)

static
GF_Err gdip_set_texture(GF_STENCIL _this, char *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat, GF_PixelFormat destination_format_hint, Bool no_copy)
{
	char *ptr;
	Bool is_yuv;
	u32 pFormat, isBGR, BPP, i, j, col;
	unsigned char a, r, g, b;
	unsigned short val;
	Bool copy;
	GPSTEN();
	CHECK_RET(GF_STENCIL_TEXTURE);

	gdip_cmat_reset(&_sten->cmat);
	isBGR = 0;
	BPP = 4;
	copy = GF_FALSE;
	is_yuv = GF_FALSE;
	/*is pixel format supported ?*/
	switch (pixelFormat) {
	case GF_PIXEL_GREYSCALE:
		pFormat = PixelFormat24bppRGB;
		BPP = 1;
		/*no support for 8bit greyscale not indexed in GDIPlus ...*/
		copy = GF_TRUE;
		break;
	case GF_PIXEL_ALPHAGREY:
		pFormat = PixelFormat32bppARGB;
		BPP = 2;
		/*cannot get it to work without using 32bpp argb*/
		copy = GF_TRUE;
		break;
	case GF_PIXEL_RGB_555:
		pFormat = PixelFormat16bppRGB555;
		BPP = 2;
		break;
	case GF_PIXEL_RGB_565:
		pFormat = PixelFormat16bppRGB565;
		BPP = 2;
		break;
	case GF_PIXEL_RGB_24:
		pFormat = PixelFormat24bppRGB;
		BPP = 3;
		/*one day I'll hope to understand how color management works with GDIplus bitmaps...*/
		isBGR = 1;
//		copy = 1;
		break;
	case GF_PIXEL_BGR_24:
		pFormat = PixelFormat24bppRGB;
		BPP = 3;
		break;
	case GF_PIXEL_RGB_32:
		pFormat = PixelFormat32bppRGB;
		BPP = 4;
		break;
	case GF_PIXEL_ARGB:
		pFormat = PixelFormat32bppARGB;
		BPP = 4;
		break;
	case GF_PIXEL_RGBA:
		pFormat = PixelFormat32bppARGB;
		BPP = 4;
		copy = GF_TRUE;
		_sten->orig_buf = (unsigned char *) pixels;
		break;
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		if ( (width*3)%4) return GF_NOT_SUPPORTED;
		_sten->orig_format = GF_PIXEL_YV12;
		is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_YUVA:
		_sten->orig_format = GF_PIXEL_YUVA;
		is_yuv = GF_TRUE;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	if (_sten->pBitmap) GdipDisposeImage(_sten->pBitmap);
	_sten->pBitmap = NULL;
	_sten->width = width;
	_sten->height = height;
	_sten->destination_format = destination_format_hint;
	if (is_yuv) {
		_sten->orig_buf = (unsigned char*)pixels;
		_sten->orig_stride = stride;
		_sten->is_converted = GF_FALSE;
		return GF_OK;
	}

	_sten->is_converted = GF_TRUE;
	_sten->format = pFormat;

	/*GDIplus limitation : horiz_stride shall be multiple of 4 and no support for pure grayscale without palette*/
	if (!copy && pixels && !(stride%4)) {
		if (no_copy && isBGR) return GF_NOT_SUPPORTED;
		GdipCreateBitmapFromScan0(_sten->width, _sten->height, stride, pFormat, (unsigned char*)pixels, &_sten->pBitmap);
		_sten->invert_br = isBGR ? GF_TRUE : GF_FALSE;
	}
	/*all other cases: create a local bitmap in desired format*/
	else {
		if (no_copy) return GF_NOT_SUPPORTED;
		GdipCreateBitmapFromScan0(_sten->width, _sten->height, 0, pFormat, NULL, &_sten->pBitmap);
		ptr = pixels;
		for (j=0; j<_sten->height; j++) {
			for (i=0; i<_sten->width; i++) {
				switch (pixelFormat) {
				case GF_PIXEL_GREYSCALE:
					r = *ptr++;
					col = GF_COL_ARGB(255, r, r, r);
					break;
				case GF_PIXEL_ALPHAGREY:
					r = *ptr++;
					a = *ptr++;
					col = GF_COL_ARGB(a, r, r, r);
					break;
				case GF_PIXEL_RGB_555:
					val = * (unsigned short *) (ptr);
					ptr+= 2;
					col = COL_555(val);
					break;
				case GF_PIXEL_RGB_565:
					val = * (unsigned short *) (ptr);
					ptr+= 2;
					col = COL_565(val);
					break;
				/*scan0 uses bgr...*/
				case GF_PIXEL_BGR_24:
				case GF_PIXEL_RGB_24:
					r = *ptr++;
					g = *ptr++;
					b = *ptr++;
					if (!isBGR) {
						col = GF_COL_ARGB(255, b, g, r);
					} else {
						col = GF_COL_ARGB(255, r, g, b);
					}
					break;
				/*NOTE: we assume little-endian only for GDIplus platforms, so BGRA/BGRX*/
				case GF_PIXEL_RGB_32:
				case GF_PIXEL_ARGB:
					b = *ptr++;
					g = *ptr++;
					r = *ptr++;
					a = *ptr++;
					if (pixelFormat==GF_PIXEL_RGB_32) a = 0xFF;
					col = GF_COL_ARGB(a, r, g, b);
					break;
				case GF_PIXEL_RGBA:
					r = *ptr++;
					g = *ptr++;
					b = *ptr++;
					a = *ptr++;
					col = GF_COL_ARGB(a, r, g, b);
					break;
				default:
					col = GF_COL_ARGB(255, 255, 255, 255);
					break;
				}
				GdipBitmapSetPixel(_sten->pBitmap, i, j, col);
			}
		}
	}

	return GF_OK;
}


static
GF_Err gdip_create_texture(GF_STENCIL _this, u32 width, u32 height, GF_PixelFormat pixelFormat)
{
	u32 pFormat;
	GPSTEN();
	CHECK_RET(GF_STENCIL_TEXTURE);

	gdip_cmat_reset(&_sten->cmat);
	/*is pixel format supported ?*/
	switch (pixelFormat) {
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_RGB_24:
		pFormat = PixelFormat24bppRGB;
		break;
	case GF_PIXEL_ALPHAGREY:
		pFormat = PixelFormat32bppARGB;
		break;
	case GF_PIXEL_RGB_555:
		pFormat = PixelFormat16bppRGB555;
		break;
	case GF_PIXEL_RGB_565:
		pFormat = PixelFormat16bppRGB565;
		break;
	case GF_PIXEL_RGB_32:
		pFormat = PixelFormat32bppRGB;
		break;
	case GF_PIXEL_ARGB:
		pFormat = PixelFormat32bppARGB;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	if (_sten->pBitmap) GdipDisposeImage(_sten->pBitmap);
	_sten->pBitmap = NULL;
	_sten->width = width;
	_sten->height = height;
	_sten->is_converted = GF_TRUE;
	_sten->format = pFormat;
	GdipCreateBitmapFromScan0(_sten->width, _sten->height, 0, pFormat, NULL, &_sten->pBitmap);
	return GF_OK;
}


static
GF_Err gdip_set_texture_repeat_mode(GF_STENCIL _this, GF_TextureTiling mode)
{
	GPSTEN();
	_sten->tiling = mode;
	return GF_OK;
}

static
GF_Err gdip_set_sr_texture_filter(GF_STENCIL _this, GF_TextureFilter filter_mode)
{
	GPSTEN();
	CHECK_RET(GF_STENCIL_TEXTURE);
	_sten->tFilter = filter_mode;
	return GF_OK;
}


static
GF_Err gdip_set_color_matrix(GF_STENCIL _this, GF_ColorMatrix *cmat)
{
	GPSTEN();
	if (!cmat || cmat->identity) {
		_sten->texture_invalid = _sten->has_cmat;
		_sten->has_cmat = GF_FALSE;
	} else {
		if (_sten->invert_br) {
			GF_ColorMatrix fin, rev;
			memcpy(&fin, cmat, sizeof(GF_ColorMatrix));
			memset(&rev, 0, sizeof(GF_ColorMatrix));
			rev.m[0] = 0;
			rev.m[2] = 1;
			rev.m[10] = 1;
			rev.m[12] = 0;
			rev.m[6] = rev.m[18] = 1;
			gf_cmx_multiply(&fin, &rev);
			cmat_gpac_to_gdip(&fin, &_sten->cmat);
		} else {
			cmat_gpac_to_gdip(cmat, &_sten->cmat);
		}
		_sten->has_cmat = GF_TRUE;
	}
	_sten->texture_invalid = GF_TRUE;
	return GF_OK;
}

static
GF_Err gdip_set_alpha(GF_STENCIL _this, u8 alpha)
{
	GPSTEN();
	if (_sten->alpha != alpha) {
		_sten->alpha = alpha;
		_sten->texture_invalid = GF_TRUE;
	}
	return GF_OK;
}

void gdip_convert_texture(struct _stencil *sten);

static
GF_Err gdip_get_pixel(GF_STENCIL _this, u32 x, u32 y, u32 *col)
{
	ARGB v;
	GpStatus st;

	GPSTEN();
	if (!_sten->is_converted) gdip_convert_texture(_sten);
	if (!_sten->pBitmap) return GF_BAD_PARAM;

	st = GdipBitmapGetPixel(_sten->pBitmap, x, y, &v);

	if (_sten->invert_br) {
		*col = GF_COL_ARGB( ((v>>24)&0xFF), ((v)&0xFF), ((v>>8)&0xFF), ((v>>16)&0xFF) );
	} else {
		*col = v;
	}
	return GF_OK;
}


static
GF_Err gdip_set_pixel(GF_STENCIL _this, u32 x, u32 y, u32 col)
{
	GpStatus st;
	ARGB v;
	GPSTEN();
	if (!_sten->pBitmap) return GF_BAD_PARAM;
	if (!_sten->is_converted) gdip_convert_texture(_sten);

	if (_sten->invert_br) {
		v = GF_COL_ARGB( ((col>>24)&0xFF), ((col)&0xFF), ((col>>8)&0xFF), ((col>>16)&0xFF) );
	} else {
		v = col;
	}
	st = GdipBitmapSetPixel(_sten->pBitmap, x, y, v);
	return GF_OK;
}

#if 0
static
GF_Err gdip_get_texture(GF_STENCIL _this, unsigned char **pixels, u32 *width, u32 *height, u32 *stride, GF_PixelFormat *pixelFormat)
{
	GpRect rc;
	BitmapData data;
	GPSTEN();
	if (!_sten->pBitmap) return GF_BAD_PARAM;

	rc.X = rc.Y = 0;
	rc.Width = _sten->width;
	rc.Height = _sten->height;

	GdipBitmapLockBits(_sten->pBitmap, &rc, ImageLockModeRead, _sten->format, &data);
	*pixels = (unsigned char *) data.Scan0;
	*width = data.Width;
	*height = data.Height;
	*stride = data.Stride;
	switch (data.PixelFormat) {
	case PixelFormat16bppRGB555:
		*pixelFormat = GF_PIXEL_RGB_555;
		break;
	case PixelFormat16bppRGB565:
		*pixelFormat = GF_PIXEL_RGB_565;
		break;
	case PixelFormat32bppRGB:
		*pixelFormat = GF_PIXEL_RGB_32;
		break;
	case PixelFormat32bppARGB:
		*pixelFormat = GF_PIXEL_ARGB;
		break;
	case PixelFormat24bppRGB:
	default:
		*pixelFormat = GF_PIXEL_RGB_24;
		break;
	}
	return GF_OK;
}
#endif


void gdip_texture_modified(GF_STENCIL _this)
{
	GPSTEN();
	if (_sten->orig_buf && (_sten->format == PixelFormat32bppARGB)) {
		gdip_set_texture(_this, (char *) _sten->orig_buf, _sten->width, _sten->height, _sten->width * 4, GF_PIXEL_RGBA, GF_PIXEL_RGBA, GF_FALSE);
	}
	_sten->texture_invalid = GF_TRUE;
}

void gdip_init_driver_texture(GF_Raster2D *driver)
{
	driver->stencil_set_texture = gdip_set_texture;
	driver->stencil_set_tiling = gdip_set_texture_repeat_mode;
	driver->stencil_set_filter = gdip_set_sr_texture_filter;
	driver->stencil_set_color_matrix = gdip_set_color_matrix;
	driver->stencil_set_alpha = gdip_set_alpha;
	driver->stencil_create_texture = gdip_create_texture;
	driver->stencil_texture_modified = gdip_texture_modified;
}


void gdip_convert_texture(struct _stencil *sten)
{
	u32 BPP, format;
	GF_VideoSurface src, dst;

	if (sten->orig_format == GF_PIXEL_YV12) {
		BPP = 3;
		dst.pixel_format = GF_PIXEL_BGR_24;
		format = PixelFormat24bppRGB;
	} else {
		BPP = 4;
		dst.pixel_format = GF_PIXEL_ARGB;
		format = PixelFormat32bppARGB;
	}
	if (BPP*sten->width*sten->height > sten->conv_size) {
		if (sten->conv_buf) gf_free(sten->conv_buf);
		sten->conv_size = BPP*sten->width*sten->height;
		sten->conv_buf = (unsigned char *) gf_malloc(sizeof(unsigned char)*sten->conv_size);
	}

	memset(&src, 0, sizeof(GF_VideoSurface));
	src.height = sten->height;
	src.width = sten->width;
	src.pitch_x  =0;
	src.pitch_y = sten->orig_stride;
	src.pixel_format = sten->orig_format;
	src.video_buffer = (char*)sten->orig_buf;

	memset(&dst, 0, sizeof(GF_VideoSurface));
	dst.width = sten->width;
	dst.height = sten->height;
	dst.pitch_x = 0;
	dst.pitch_y = BPP*sten->width;
	dst.video_buffer = (char*)sten->conv_buf;

	gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, GF_FALSE, NULL, NULL);

	if (sten->pBitmap) GdipDisposeImage(sten->pBitmap);
	GdipCreateBitmapFromScan0(sten->width, sten->height, BPP*sten->width, format, sten->conv_buf, &sten->pBitmap);
	sten->is_converted = GF_TRUE;
}

void gdip_load_texture(struct _stencil *sten)
{
	GpImageAttributes *attr;
	ColorMatrix _cmat;

	if (sten->texture_invalid && sten->pTexture) {
		GdipDeleteBrush(sten->pTexture);
		sten->pTexture = NULL;
	}
	/*nothing to do*/
	if (sten->is_converted && sten->pTexture) return;
	sten->texture_invalid = GF_FALSE;

	/*convert*/
	if (!sten->is_converted) gdip_convert_texture(sten);

	GdipCreateImageAttributes(&attr);
	if (sten->has_cmat) {
		memcpy(_cmat.m, sten->cmat.m, sizeof(REAL)*5*5);
	} else {
		memset(_cmat.m, 0, sizeof(REAL)*5*5);
		_cmat.m[0][0] = _cmat.m[1][1] = _cmat.m[2][2] = _cmat.m[3][3] = _cmat.m[4][4] = 1.0;
		if (sten->invert_br) {
			_cmat.m[0][0] = 0;
			_cmat.m[0][2] = 1;
			_cmat.m[2][2] = 0;
			_cmat.m[2][0] = 1;
		}
	}

	_cmat.m[3][3] *= ((REAL) sten->alpha) /255.0f;
	GdipSetImageAttributesColorMatrix(attr, ColorAdjustTypeDefault, TRUE, &_cmat, NULL, ColorMatrixFlagsDefault);
	if (sten->pTexture) GdipDeleteBrush(sten->pTexture);
	GdipCreateTextureIAI(sten->pBitmap, attr, 0, 0, sten->width, sten->height, &sten->pTexture);

	/*1- wrap mode is actually ignored in constructor*/
	/*2- GDIPlus does not support S / T clamping */
	GdipSetTextureWrapMode(sten->pTexture, WrapModeTile);

	GdipDisposeImageAttributes(attr);
}

