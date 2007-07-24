/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/base_coding.h>
#include <gpac/constants.h>

static const char base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

GF_EXPORT
u32 gf_base64_encode(char *_in, u32 inSize, char *_out, u32 outSize)
{
	s32 padding;
	u32 i = 0, j = 0;
	unsigned char *in = (unsigned char *)_in;
	unsigned char *out = (unsigned char *)_out;
	
	if (outSize < (inSize * 4 / 3)) return 0;

	while (i < inSize) {
		padding = 3 - (inSize - i);
		if (padding == 2) {
			out[j] = base_64[in[i]>>2];
			out[j+1] = base_64[(in[i] & 0x03) << 4];
			out[j+2] = '=';
			out[j+3] = '=';
		} else if (padding == 1) {
			out[j] = base_64[in[i]>>2];
			out[j+1] = base_64[((in[i] & 0x03) << 4) | ((in[i+1] & 0xf0) >> 4)];
			out[j+2] = base_64[(in[i+1] & 0x0f) << 2];
			out[j+3] = '=';
		} else{
			out[j] = base_64[in[i]>>2];
			out[j+1] = base_64[((in[i] & 0x03) << 4) | ((in[i+1] & 0xf0) >> 4)];
			out[j+2] = base_64[((in[i+1] & 0x0f) << 2) | ((in[i+2] & 0xc0) >> 6)];
			out[j+3] = base_64[in[i+2] & 0x3f];
		}
		i += 3;
		j += 4;
	}
	return j;
}

static const unsigned char index_64[128] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   62, 0xff, 0xff, 0xff,   63,
      52,   53,   54,   55,   56,   57,   58,   59,   60,   61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
      15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
      41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, 0xff, 0xff, 0xff, 0xff, 0xff
};

#define char64(c)  ((c > 127) ? (char) 0xff : index_64[(c)])

/*denoise input*/
u32 load_block(char *in, u32 size, u32 pos, char *out) 
{ 
	u32 i, len; 
	u8 c;
	len = i = 0;
	while ((len<4) && ((pos+i)<size)) {
		c = in[pos+i];
		if ( ((c>='A') && (c<='Z'))
		|| ((c>='a') && (c<='z'))
		|| ((c>='0') && (c<='9'))
		|| (c=='=') || (c=='+') || (c=='/')
		) {
			out[len] = c;
			len++;
		}
		i++;
	}
	while (len<4) { out[len] = (char) 0xFF; len++; }
	return pos+i;
}

GF_EXPORT
u32 gf_base64_decode(char *in_buf, u32 inSize, char *out, u32 outSize)
{
	u32 i = 0, j = 0, padding;
	unsigned char c[4], in[4];

	if (outSize < (inSize * 3 / 4)) return 0;

	while ((i + 3) < inSize) {
		padding = 0;
		i = load_block(in_buf, inSize, i, (char*)in);
		c[0] = char64(in[0]); 
		padding += (c[0] == 0xff);
		c[1] = char64(in[1]);
		padding += (c[1] == 0xff);
		c[2] = char64(in[2]);
		padding += (c[2] == 0xff);
		c[3] = char64(in[3]); 
		padding += (c[3] == 0xff);
		if (padding == 2) {
			out[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
			out[j]   = (c[1] & 0x0f) << 4;
		} else if (padding == 1) {
			out[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
			out[j++] = ((c[1] & 0x0f) << 4) | ((c[2] & 0x3c) >> 2);
			out[j]   = (c[2] & 0x03) << 6;
		} else {
			out[j++] = (c[0] << 2) | ((c[1] & 0x30) >> 4);
			out[j++] = ((c[1] & 0x0f) << 4) | ((c[2] & 0x3c) >> 2);
			out[j++] = ((c[2] & 0x03) << 6) | (c[3] & 0x3f);
		}
		//i += 4;
	}
	return j;
}


/*
 *			Copyright (c) ENST 2004  - Philippe de Cuetos 
 */

static const char base_16[] = "0123456789abcdef";

GF_EXPORT
u32 gf_base16_encode(char *_in, u32 inSize, char *_out, u32 outSize)
{
	u32 i = 0;
	unsigned char *in = (unsigned char *)_in;
	unsigned char *out = (unsigned char *)_out;

	if (outSize < (inSize * 2)+1) return 0;

	for (i=0;i<inSize;i++) {
		out[2*i] = base_16[((in[i] & 0xf0) >> 4)];
		out[2*i+1] = base_16[(in[i] & 0x0f)];	
	}
	out[(inSize * 2)] = 0;
	
	return i;
}

#define char16(nb) (((nb) < 97) ? ((nb)-48) : ((nb)-87))

GF_EXPORT
u32 gf_base16_decode(char *in, u32 inSize, char *out, u32 outSize)
{
	u32 j=0;
	u32 c[2] = {0,0};

	if (outSize < (inSize / 2)) return 0;
	if ((inSize % 2) != 0) return 0;
	
	for (j=0;j<inSize/2;j++) {
		c[0] = char16(in[2*j]);
		c[1] = char16(in[2*j+1]);
		out[j] = ((c[0] << 4)&0xf0) | (c[1]&0x0f);
	}
	out[inSize/2] = 0;
	
	return j;
}
