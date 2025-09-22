/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

static GFINLINE s32 gf_tok_is_char_in_set(const char TestChar, const char *TestSet)
{
	u32 i, Len;
	Len = (u32) strlen(TestSet);
	for (i=0; i<Len; i++) {
		if (TestChar == TestSet[i]) return 1;
	}
	return 0;
}

GF_EXPORT
s32 gf_token_get(const char *Buffer, s32 Start,  const char *Separator,  char *Container, s32 ContainerSize)
{
	s32 i, start, end, Len;
	Container[0]=0;
	if (Start<0) return -1;

	Len = (s32) strlen( Buffer );
	for (i=Start; i<Len; i++ ) {
		if (!gf_tok_is_char_in_set(Buffer[i], Separator)) break;
	}
	start = i;
	if (i == Len) return( -1 );

	for (i=start; i<Len; i++) {
		if (gf_tok_is_char_in_set(Buffer[i], Separator)) break;
	}
	end = i-1;

	for (i=start; ((i<=end) && (i< start+(ContainerSize-1))); i++) {
		Container[i-start] = Buffer[i];
	}
	Container[i-start] = 0;

	return (end+1);
}

GF_EXPORT
s32 gf_token_get_strip(const char *Buffer, s32 Start, const char *Separator, const char *strip_set, char *Container, s32 ContainerSize)
{
	u32 i, k, len;
	s32 res = gf_token_get(Buffer, Start, Separator, Container, ContainerSize);
	if (!strip_set || (res<0)) return res;
	i=k=0;
	len = (u32) strlen(Container);
	while (strchr(strip_set, Container[i]) ) i++;
	while (len && strchr(strip_set, Container[len]) ) {
		Container[len]=0;
		len--;
	}
	while (k+i<=len) {
		Container[k] = Container[k+i];
		k++;
	}
	Container[k] = 0;
	return res;
}


GF_EXPORT
s32 gf_token_get_line(const char *Buffer, u32 Start, u32 Size, char *LineBuffer, u32 LineBufferSize)
{
	u32 offset;
	s32 End, Total;
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
	if ((u32) Total >= LineBufferSize) Total = LineBufferSize-1;
	memcpy(LineBuffer, Buffer + Start, Total);
	LineBuffer[Total] = 0;
	return (End + offset);
}

GF_EXPORT
s32 gf_token_find(const char *Buffer, u32 Start, u32 Size, const char *Pattern)
{
	u32 i, j, flag;
	s32 Len;

	if (Start >= Size) return -1;

	Len = (u32) strlen(Pattern);
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

GF_EXPORT
const char *gf_token_find_word(const char *in_str, const char *word, char *charsep)
{
	u32 len;
	if (!in_str || !word) return NULL;
	len = (u32) strlen(word);
	while (in_str) {
		char *sep = strstr(in_str, word);
		if (!sep) return NULL;
		if (!charsep) return sep;

		if ((sep>in_str) && strchr(charsep, sep[-1]))
			return sep;

		if (strchr(charsep, sep[len]))
			return sep;
		in_str = sep+len;
	}
	return NULL;
}
