/**************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Image management functions -
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
 * $Id: image.cpp,v 1.1.1.1 2005-07-13 14:36:14 jeanlf Exp $
 *
 ****************************************************************************/
//#include <math.h>

#include "portab.h"
#include "global.h"        /* XVID_CSP_XXX's */
#include "xvid.h"       /* XVID_CSP_XXX's */
#include "image.h"
#include "interpolate8x8.h"
#include "reduced.h"
#include "decoder.h"
#include "mem_align.h"

#define SAFETY 64
#define EDGE_SIZE2  (EDGE_SIZE/2)

//----------------------------

int image_create(IMAGE * image, dword edged_width, dword edged_height) {

	const dword edged_width2 = edged_width / 2;
	const dword edged_height2 = edged_height / 2;

	image->y = (unsigned char*)xvid_malloc(edged_width * (edged_height + 1) + SAFETY, CACHE_LINE);
	if (image->y == NULL) {
		return -1;
	}
	MemSet(image->y, 0, edged_width * (edged_height + 1) + SAFETY);

	image->u = (unsigned char*)xvid_malloc(edged_width2 * edged_height2 + SAFETY, CACHE_LINE);
	if (image->u == NULL) {
		xvid_free(image->y);
		image->y = NULL;
		return -1;
	}
	MemSet(image->u, 0, edged_width2 * edged_height2 + SAFETY);

	image->v = (unsigned char*)xvid_malloc(edged_width2 * edged_height2 + SAFETY, CACHE_LINE);
	if (image->v == NULL) {
		xvid_free(image->u);
		image->u = NULL;
		xvid_free(image->y);
		image->y = NULL;
		return -1;
	}
	MemSet(image->v, 0, edged_width2 * edged_height2 + SAFETY);

	image->y += EDGE_SIZE * edged_width + EDGE_SIZE;
	image->u += EDGE_SIZE2 * edged_width2 + EDGE_SIZE2;
	image->v += EDGE_SIZE2 * edged_width2 + EDGE_SIZE2;

	return 0;
}

//----------------------------

void image_destroy(IMAGE * image, dword edged_width, dword edged_height) {

	const dword edged_width2 = edged_width / 2;

	if(image->y) {
		xvid_free(image->y - (EDGE_SIZE * edged_width + EDGE_SIZE));
		image->y = NULL;
	}
	if(image->u) {
		xvid_free(image->u - (EDGE_SIZE2 * edged_width2 + EDGE_SIZE2));
		image->u = NULL;
	}
	if(image->v) {
		xvid_free(image->v - (EDGE_SIZE2 * edged_width2 + EDGE_SIZE2));
		image->v = NULL;
	}
}

//----------------------------

void IMAGE::Swap(IMAGE *image2) {
	::Swap(y, image2->y);
	::Swap(u, image2->u);
	::Swap(v, image2->v);
}

//----------------------------

#ifdef _ARM_

extern"C"
void XVID_MemCpy(void *dst, const void *src, dword count);

#else

void XVID_MemCpy(void *dst, const void *src, dword count) {
	assert(!(dword(dst)&3));
	assert(!(dword(count)&3));
#ifdef USE_ARM_ASM
	int t0, t1, t2, t3;
	asm volatile(
	    "subs %2, %2, #16\n   blt .mc_no16\n\t"
	    "\n.mc_loop16:\n\t"
	    //the order of regs in {} is not important, now it's ordered as to avoid compiler warnings
	    "ldmia %1!, {%6, %3, %5, %4}\n\t"
	    "stmia %0!, {%6, %3, %5, %4}\n\t"
	    "subs %2, %2, #16\n   bge .mc_loop16\n\t"
	    "\n.mc_no16:\n\t"
	    "adds %2, %2, #16\n\t"
	    "beq .mc_end\n\t"
	    "\n.mc_loop4:\n\t"
	    "ldr %3, [%1], #4\n\t"
	    "str %3, [%0], #4\n\t"
	    "subs %2, %2, #4\n   bne .mc_loop4\n\t"
	    "\n.mc_end:\n\t"
	    : "+r"(dst), "+r"(src), "+r"(count), "&=r"(t0), "&=r"(t1), "&=r"(t2), "&=r"(t3)
	    :
	);
#else
	MemCpy(dst, src, count);
#endif
}

#endif

//---------------------------

void IMAGE::Copy(const IMAGE * image2, dword edged_width, dword height) {
	XVID_MemCpy(y, image2->y, edged_width * height);
	XVID_MemCpy(u, image2->u, edged_width * height / 4);
	XVID_MemCpy(v, image2->v, edged_width * height / 4);
}

//----------------------------

inline void XVID_Set16bytes(void *dst, dword val) {
	assert(!(dword(dst)&3));
	val |= val<<8;
	val |= val<<16;
	((dword*)dst)[0] = val;
	((dword*)dst)[1] = val;
	((dword*)dst)[2] = val;
	((dword*)dst)[3] = val;
}

//---------------------------

inline void XVID_Set8bytes(void *dst, dword val) {
	assert(!(dword(dst)&3));
	val |= val<<8;
	val |= val<<16;
	((dword*)dst)[0] = val;
	((dword*)dst)[1] = val;
}

//--------------------------

void S_decoder::SetEdges(IMAGE &img) const {

	const dword edged_width2 = edged_width / 2;

	assert(EDGE_SIZE==16);

	byte *dst = img.y - (EDGE_SIZE + EDGE_SIZE * edged_width);
	const byte *src = img.y;

	//according to the Standard Clause 7.6.4, padding is done starting at 16
	// * pixel width and height multiples
	dword width  = (S_decoder::width+15)&~15;
	dword height = (S_decoder::height+15)&~15;
	dword width2 = width/2;

	dword i;
	for(i = 0; i < EDGE_SIZE; i++) {
		XVID_Set16bytes(dst, *src);
		XVID_MemCpy(dst + EDGE_SIZE, src, width);
		XVID_Set16bytes(dst + edged_width - EDGE_SIZE, *(src + width - 1));
		dst += edged_width;
	}
	for(i = 0; i < height; i++) {
		XVID_Set16bytes(dst, *src);
		XVID_Set16bytes(dst + edged_width - EDGE_SIZE, src[width - 1]);
		dst += edged_width;
		src += edged_width;
	}

	src -= edged_width;
	for(i = 0; i < EDGE_SIZE; i++) {
		XVID_Set16bytes(dst, *src);
		XVID_MemCpy(dst + EDGE_SIZE, src, width);
		XVID_Set16bytes(dst + edged_width - EDGE_SIZE, *(src + width - 1));
		dst += edged_width;
	}

	//U
	dst = img.u - (EDGE_SIZE2 + EDGE_SIZE2 * edged_width2);
	src = img.u;

	for (i = 0; i < EDGE_SIZE2; i++) {
		XVID_Set8bytes(dst, *src);
		XVID_MemCpy(dst + EDGE_SIZE2, src, width2);
		XVID_Set8bytes(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1));
		dst += edged_width2;
	}

	for (i = 0; i < height / 2; i++) {
		XVID_Set8bytes(dst, *src);
		XVID_Set8bytes(dst + edged_width2 - EDGE_SIZE2, src[width2 - 1]);
		dst += edged_width2;
		src += edged_width2;
	}
	src -= edged_width2;
	for (i = 0; i < EDGE_SIZE2; i++) {
		XVID_Set8bytes(dst, *src);
		XVID_MemCpy(dst + EDGE_SIZE2, src, width2);
		XVID_Set8bytes(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1));
		dst += edged_width2;
	}

	//V
	dst = img.v - (EDGE_SIZE2 + EDGE_SIZE2 * edged_width2);
	src = img.v;

	for(i = 0; i < EDGE_SIZE2; i++) {
		XVID_Set8bytes(dst, *src);
		XVID_MemCpy(dst + EDGE_SIZE2, src, width2);
		XVID_Set8bytes(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1));
		dst += edged_width2;
	}

	for(i = 0; i < height / 2; i++) {
		XVID_Set8bytes(dst, *src);
		XVID_Set8bytes(dst + edged_width2 - EDGE_SIZE2, src[width2 - 1]);
		dst += edged_width2;
		src += edged_width2;
	}
	src -= edged_width2;
	for(i = 0; i < EDGE_SIZE2; i++) {
		XVID_Set8bytes(dst, *src);
		XVID_MemCpy(dst + EDGE_SIZE2, src, width2);
		XVID_Set8bytes(dst + edged_width2 - EDGE_SIZE2, *(src + width2 - 1));
		dst += edged_width2;
	}
}

//----------------------------

void IMAGE::Clear(int width, int height, int edged_width, int y, int u, int v) {

	byte * p;
	int i;

	p = IMAGE::y;
	for(i = 0; i < height; i++) {
		MemSet(p, y, width);
		p += edged_width;
	}

	p = IMAGE::u;
	for(i = 0; i < height/2; i++) {
		MemSet(p, u, width/2);
		p += edged_width/2;
	}

	p = IMAGE::v;
	for(i = 0; i < height/2; i++) {
		MemSet(p, v, width/2);
		p += edged_width/2;
	}
}

//----------------------------
/* reduced resolution deblocking filter
   block = block size (16=rrv, 8=full resolution)
   flags = XVID_DEC_YDEBLOCK|XVID_DEC_UVDEBLOCK
*/
void IMAGE::deblock_rrv(int edged_width, const MACROBLOCK * mbs, int mb_width, int mb_height, int mb_stride, int block, int flags) {

	const int edged_width2 = edged_width /2;
	const int nblocks = block / 8;   //skals code uses 8pixel block uints
	int i,j;

	for (j = 1; j < mb_height*2; j++)      //horizontal deblocking
		for (i = 0; i < mb_width*2; i++)
		{
			if(mbs[(j-1)/2*mb_stride + (i/2)].mode != MODE_NOT_CODED ||
			        mbs[(j+0)/2*mb_stride + (i/2)].mode != MODE_NOT_CODED)
			{
				hfilter_31(IMAGE::y + (j*block - 1)*edged_width + i*block,
				           IMAGE::y + (j*block + 0)*edged_width + i*block, nblocks);
			}
		}

	for (j = 0; j < mb_height*2; j++)      // vertical deblocking
		for (i = 1; i < mb_width*2; i++)
		{
			if (mbs[(j/2)*mb_stride + (i-1)/2].mode != MODE_NOT_CODED ||
			        mbs[(j/2)*mb_stride + (i+0)/2].mode != MODE_NOT_CODED)
			{
				vfilter_31(IMAGE::y + (j*block)*edged_width + i*block - 1,
				           IMAGE::y + (j*block)*edged_width + i*block + 0,
				           edged_width, nblocks);
			}
		}


	//chroma

	for (j = 1; j < mb_height; j++)     //orizontal deblocking
		for (i = 0; i < mb_width; i++)
		{
			if (mbs[(j-1)*mb_stride + i].mode != MODE_NOT_CODED ||
			        mbs[(j+0)*mb_stride + i].mode != MODE_NOT_CODED)
			{
				hfilter_31(IMAGE::u + (j*block - 1)*edged_width2 + i*block,
				           IMAGE::u + (j*block + 0)*edged_width2 + i*block, nblocks);
				hfilter_31(IMAGE::v + (j*block - 1)*edged_width2 + i*block,
				           IMAGE::v + (j*block + 0)*edged_width2 + i*block, nblocks);
			}
		}

	for (j = 0; j < mb_height; j++)     //ertical deblocking
		for (i = 1; i < mb_width; i++)
		{
			if (mbs[j*mb_stride + i - 1].mode != MODE_NOT_CODED ||
			        mbs[j*mb_stride + i + 0].mode != MODE_NOT_CODED)
			{
				vfilter_31(IMAGE::u + (j*block)*edged_width2 + i*block - 1,
				           IMAGE::u + (j*block)*edged_width2 + i*block + 0,
				           edged_width2, nblocks);
				vfilter_31(IMAGE::v + (j*block)*edged_width2 + i*block - 1,
				           IMAGE::v + (j*block)*edged_width2 + i*block + 0,
				           edged_width2, nblocks);
			}
		}
}

//----------------------------
