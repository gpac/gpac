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

#include <gpac/token.h>

static GFINLINE s32 gf_tok_is_char_in_set(unsigned char TestChar, unsigned char *TestSet)
{
	u32 i, Len;
	Len = strlen(TestSet);
	for (i=0; i<Len; i++) {
		if (TestChar == TestSet[i]) return 1;
	}
	return 0;
}

s32 gf_token_get(unsigned char *Buffer, s32 Start,  unsigned char *Separator,  unsigned char *Container, s32 ContainerSize)
{
	s32 i, start, end, Len;

	Len = strlen( Buffer );
	for (i=Start; i<Len; i++ ) {
		if (!gf_tok_is_char_in_set(Buffer[i], Separator)) break;
	}
	start = i;
	if (i == Len) return( -1 );
	
	for (i=start; i<Len; i++) {
		if (gf_tok_is_char_in_set(Buffer[i], Separator)) break;
	}
	end = i-1;

	for (i=start; ((i<=end) && (i< (ContainerSize-1))); i++) {
		Container[i-start] = Buffer[i];
	}
	Container[i-start] = 0;

	return (end+1);
}



s32 gf_token_get_line(unsigned char	*Buffer, 
				  u32 Start,
				  u32 Size,
				  unsigned char *LineBuffer,
				  u32 LineBufferSize)
{
	u32 offset;
	s32 i, End, Total;
	LineBuffer[0] = 0;
	if (Start >= Size) return -1;

	offset = 2;
	End = gf_token_find(Buffer, Start, Size, "\r\n");
	if (End<0) {
		End = gf_token_find(Buffer, Start, Size, "\r");
		if (End<0) End = gf_token_find(Buffer, Start, Size, "\n");
		if (End < 0) return -1;
		offset = 1;
	}

	Total = End - Start + offset;
	if ((u32) Total >= LineBufferSize) Total = LineBufferSize;
	for (i=0; i<Total; i++) LineBuffer[i] = Buffer[Start+i];
	LineBuffer[i] = 0;
	return (End + offset);
}

s32 gf_token_find(unsigned char *Buffer,
				   u32 Start,
				   u32 Size,
				   unsigned char *Pattern)
{
	u32 i, j, flag;
	s32 Len;

	if (Start >= Size) return -1;
	
	Len = strlen(Pattern);
	if ( Len <= 0 ) return -1;
	if (Size - Start < (u32) Len) return -1;

	for (i=Start; i<= Size-Len; i++) {
		flag = 0;
		for (j=0; j< (u32) Len; j++) {
			if (Buffer[i+j] != Pattern[j]) {
				flag = 1;
				break;
			}
		}
		//found
		if (!flag) return i;
	}
	return -1;
}

