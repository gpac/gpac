/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - 8bit<->16bit transfer  -
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
 * $Id: mem_transfer.cpp,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#include "global.h"
#include "mem_transfer.h"

/*****************************************************************************
 *
 * All these functions are used to transfer data from a 8 bit data array
 * to a 16 bit data array.
 *
 * This is typically used during motion compensation, that's why some
 * functions also do the addition/substraction of another buffer during the
 * so called  transfer.
 *
 ****************************************************************************/

//----------------------------
/*
 * SRC - the source buffer
 * DST - the destination buffer
 *
 * Then the function does the 8->16 bit transfer and this serie of operations :
 *
 *    SRC (16bit) = SRC
 *    DST (8bit)  = max(min(SRC, 255), 0)
 */
void transfer_16to8copy(byte *dst, const int *src, dword stride) {

	for(int j = 0; j < 8; j++) {
		for(int i = 0; i < 8; i++) {
			int pixel = *src++;

			if(pixel < 0)
				pixel = 0;
			//else
			if(pixel > 255)
				pixel = 255;
			dst[i] = byte(pixel);
		}
		dst += stride;
	}
}

//----------------------------
/*
 * SRC - the source buffer
 * DST - the destination buffer
 *
 * Then the function does the 16->8 bit transfer and this serie of operations :
 *
 *    SRC (16bit) = SRC
 *    DST (8bit)  = max(min(DST+SRC, 255), 0)
 */
void transfer_16to8add(byte *dst, const int *src, dword stride) {

	for(int j = 0; j < 8; j++) {
		for(int i = 0; i < 8; i++) {
			int pixel = dst[i] + *src++;

			if(pixel < 0)
				pixel = 0;
			//else
			if(pixel > 255)
				pixel = 255;
			dst[i] = byte(pixel);
		}
		//src += 8;
		dst += stride;
	}
}

#ifndef _ARM_
//----------------------------
/*
 * SRC - the source buffer
 * DST - the destination buffer
 *
 * Then the function does the 8->8 bit transfer and this serie of operations :
 *
 *    SRC (8bit) = SRC
 *    DST (8bit) = SRC
 */
void transfer8x8_copy(byte *dst, const byte *src, dword stride) {

	assert(!(stride&3));
	assert(!(dword(dst)&3));
#if defined USE_ARM_ASM && 1
	int y, tmp, tmp1;

#define NL "add %0, %0, %4\n   add %1, %1, %4\n\t"
	asm volatile(
	    "orr %2, %1, %4\n\t"
	    "tst %2, #3\n   bne .tc_no_dw\n\t"
	    //dword version
	    "\n.tc_dw_loop:\n\t"
#define COPY_QW "ldmia %1, { %2, %3 }\n   stmia %0, { %2, %3 }\n\t"
	    COPY_QW NL COPY_QW NL COPY_QW NL COPY_QW NL COPY_QW NL COPY_QW NL COPY_QW NL COPY_QW
	    "b .tc_end\n\t"

	    "\n.tc_no_dw:\n\t"
	    "tst %2, #1\n   bne .tc_no_w\n\t"
	    //word version
	    "\n.tc_w_loop:\n\t"
#define COPY_W \
      "ldrh %2, [%1, #0]\n   strh %2, [%0, #0]\n\t" \
      "ldrh %2, [%1, #2]\n   strh %2, [%0, #2]\n\t" \
      "ldrh %2, [%1, #4]\n   strh %2, [%0, #4]\n\t" \
      "ldrh %2, [%1, #6]\n   strh %2, [%0, #6]\n\t"
	    COPY_W NL COPY_W NL COPY_W NL COPY_W NL COPY_W NL COPY_W NL COPY_W NL COPY_W
	    "b .tc_end\n\t"

	    "\n.tc_no_w:\n\t"
	    "mov %5, #8\n\t"
	    "\n.tc_b_loop:\n\t"
	    "ldrb %2, [%1, #0]\n   strb %2, [%0, #0]\n\t"
	    "ldrb %2, [%1, #1]\n   strb %2, [%0, #1]\n\t"
	    "ldrb %2, [%1, #2]\n   strb %2, [%0, #2]\n\t"
	    "ldrb %2, [%1, #3]\n   strb %2, [%0, #3]\n\t"
	    "ldrb %2, [%1, #4]\n   strb %2, [%0, #4]\n\t"
	    "ldrb %2, [%1, #5]\n   strb %2, [%0, #5]\n\t"
	    "ldrb %2, [%1, #6]\n   strb %2, [%0, #6]\n\t"
	    "ldrb %2, [%1, #7]\n   strb %2, [%0, #7]\n\t"
	    NL
	    "subs %5, %5, #1\n   bne .tc_b_loop\n\t"
	    "\n.tc_end:\n\t"
	    : "+r"(dst), "+r"(src), "&=r"(tmp), "&=r"(tmp1)
	    : "r"(stride), "r"(y)
	);
#else
	if(!(dword(src)&3) && !(dword(dst)&3)) {
		for(dword y = 8; y--; ) {
			((dword*)dst)[0] = ((dword*)src)[0];
			((dword*)dst)[1] = ((dword*)src)[1];

			src += stride;
			dst += stride;
		}
	} else if(!(dword(src)&1) && !(dword(dst)&1)) {
		for(dword y = 8; y--; ) {
			((word*)dst)[0] = ((word*)src)[0];
			((word*)dst)[1] = ((word*)src)[1];
			((word*)dst)[2] = ((word*)src)[2];
			((word*)dst)[3] = ((word*)src)[3];

			src += stride;
			dst += stride;
		}
	} else {
		for(dword y = 8; y--; ) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			dst[4] = src[4];
			dst[5] = src[5];
			dst[6] = src[6];
			dst[7] = src[7];

			src += stride;
			dst += stride;
		}
	}
#endif
}
#endif

//----------------------------
#ifndef PROFILE

#ifdef __SYMBIAN32__

#include <e32base.h>

void MemSet(void *dst, byte c, dword len) {
	Mem::Fill(dst, len, c);
}

//----------------------------

void MemCpy(void *dst, const void *src, dword len) {
	Mem::Copy(dst, src, len);
}

//----------------------------

int MemCmp(const void *mem1, const void *mem2, dword len) {
	return Mem::Compare((byte*)mem1, len, (byte*)mem2, len);
}

//----------------------------

dword StrLen(const char *cp) {
	return User::StringLength((const byte*)cp);
}

//----------------------------

void Fatal(const char *msg, dword code) {

	int len = StrLen(msg);
	TBuf16<20> desc;
	desc.Copy(TPtr8((byte*)msg, Min(len, 20), len));
	User::Panic(desc, code);
}


#else

//----------------------------
#include <memory.h>
#include <stdio.h>

void MemSet(void *dst, byte c, dword len) {
	memset(dst, c, len);
}

//----------------------------

void MemCpy(void *dst, const void *src, dword len) {
	memcpy(dst, src, len);
}

//----------------------------

int MemCmp(const void *mem1, const void *mem2, dword len) {
	return memcmp(mem1, mem2, len);
}

//----------------------------

void *operator new(size_t sz, TLeave) {
	void *vp = new byte[sz];
	if(!vp) {
		//todo: fatal error
		Fatal("Not enough memory", sz);
	}
	return vp;
}

//----------------------------
#include <windows.h>

void Fatal(const char *msg, dword code) {

#ifdef _WIN32_WCE
	wchar_t buf[256];
	swprintf(buf, L"%i (%i)", msg, code);
	MessageBox(NULL, buf, L"Fatal error", MB_OK | MB_ICONERROR);
#else
	char buf[256];
	sprintf(buf, "%i (%i)", msg, code);
	MessageBox(NULL, buf, "Fatal error", MB_OK | MB_ICONERROR);
#endif
	exit(1);
}

//----------------------------
#endif
#endif
//----------------------------
