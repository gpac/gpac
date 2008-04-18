/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / image format module
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


#ifndef _IMG_IN_H
#define _IMG_IN_H

/*all codecs are regular */
#include <gpac/modules/codec.h>
#include <gpac/modules/service.h>
#include <gpac/constants.h>

enum
{
	DEC_RESERVED = 0,
	DEC_PNG,
	DEC_JPEG,
	DEC_JP2,
	DEC_BMP,
};

typedef struct
{
	u32 type;
	void *opaque;
} IMGDec;

/*all constructors shall setup the wraper type and handle
	return 1 for success, 0 otherwise
all destructors only destroy their private stacks (eg not the interface nor the wraper)
*/
Bool NewPNGDec(GF_BaseDecoder *dec);
void DeletePNGDec(GF_BaseDecoder *dec);

Bool NewJPEGDec(GF_BaseDecoder *dec);
void DeleteJPEGDec(GF_BaseDecoder *dec);

#ifdef GPAC_HAS_JP2
Bool NewJP2Dec(GF_BaseDecoder *dec);
void DeleteJP2Dec(GF_BaseDecoder *dec);
#endif


#if defined(WIN32) || defined(_WIN32_WCE)
#include <windows.h>
#else
typedef struct tagBITMAPFILEHEADER 
{
    u16	bfType;
    u32	bfSize;
    u16	bfReserved1;
    u16	bfReserved2;
    u32 bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{
	u32	biSize;
	s32	biWidth;
	s32	biHeight;
	u16	biPlanes;
	u16	biBitCount;
	u32	biCompression;
	u32	biSizeImage;
	s32	biXPelsPerMeter;
	s32	biYPelsPerMeter;
	u32	biClrUsed;
	u32	biClrImportant;
} BITMAPINFOHEADER;

#define BI_RGB        0L

#endif

#define GPAC_BMP_OTI	0x82

Bool NewBMPDec(GF_BaseDecoder *dec);
void DeleteBMPDec(GF_BaseDecoder *dec);

#define IMG_CM_SIZE		1


void *NewLoaderInterface();
void DeleteLoaderInterface(void *ifce);

#endif
