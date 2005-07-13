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

#ifndef _GF_BITSTREAM_H_
#define _GF_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>


/**********************************************************************
					GF_BitStream: a bit reader/writer in memory or file
**********************************************************************/
enum
{
	GF_BITSTREAM_READ = 0,
	GF_BITSTREAM_WRITE,
};

typedef struct __tag_bitstream GF_BitStream;

/* Constructs a bitstream from a buffer (READ or WRITE)
in WRITE mode, you can specify a NULL buffer as an input and any size (this
is usefull when you know the max size of an incoming buffer).*/
GF_BitStream *gf_bs_new(unsigned char *buffer, u64 BufferSize, u32 mode);

/* creates a bitstream from a file handle. You have to open your file in the appropriated mode:
GF_BITSTREAM_READ: bitstream is constructed for reading
GF_BITSTREAM_WRITE: bitstream is constructed for writing
you may use any of these modes for a file with read/write access

RESULTS ARE UNEXPECTED IF YOU TOUCH THE FILE WHILE USING THE BITSTREAM 
*/
GF_BitStream *gf_bs_from_file(FILE *f, u32 mode);
/* delete the bitstream. If the buffer was created by the bitstream, it is deleted if still present
*/
void gf_bs_del(GF_BitStream *bs);

u8 gf_bs_read_bit(GF_BitStream *bs);
u32 gf_bs_read_int(GF_BitStream *bs, u32 nBits);
u64 gf_bs_read_long_int(GF_BitStream *bs, u32 nBits);
Float gf_bs_read_float(GF_BitStream *bs);
Double gf_bs_read_double(GF_BitStream *bs);
u32 gf_bs_read_data(GF_BitStream *bs, unsigned char *data, u32 nbBytes);
/*align int reading - you must not use these if not sure bitstream is not aligned*/
u32 gf_bs_read_u8(GF_BitStream *bs);
u32 gf_bs_read_u16(GF_BitStream *bs);
u32 gf_bs_read_u24(GF_BitStream *bs);
u32 gf_bs_read_u32(GF_BitStream *bs);
u64 gf_bs_read_u64(GF_BitStream *bs);

/*little endian handling*/
u32 gf_bs_read_u32_le(GF_BitStream *bs);
u16 gf_bs_read_u16_le(GF_BitStream *bs);

void gf_bs_write_int(GF_BitStream *bs, s32 value, s32 nBits);
void gf_bs_write_long_int(GF_BitStream *bs, s64 value, s32 nBits);
void gf_bs_write_float(GF_BitStream *bs, Float value);
void gf_bs_write_double(GF_BitStream *bs, Double value);
u32 gf_bs_write_data(GF_BitStream *bs, unsigned char *data, u32 nbBytes);
/*align int writing - you must not use these if not sure bitstream is not aligned*/
void gf_bs_write_u8(GF_BitStream *bs, u32 value);
void gf_bs_write_u16(GF_BitStream *bs, u32 value);
void gf_bs_write_u24(GF_BitStream *bs, u32 value);
void gf_bs_write_u32(GF_BitStream *bs, u32 value);
void gf_bs_write_u64(GF_BitStream *bs, u64 value);

/*little endian handling*/
void gf_bs_write_u32_le(GF_BitStream *bs, u32 val);
void gf_bs_write_u16_le(GF_BitStream *bs, u32 val);

void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);

/* Align the buffer to the next byte boundary - returns NbBits stuffed */
u8 gf_bs_align(GF_BitStream *bs);
/* get the number of bytes available till end of buffer (READ modes) or -1 (WRITE modes) */
u64 gf_bs_available(GF_BitStream *bs);
/* used in dynamic WRITE mode to retrieve the content of the buffer */
void gf_bs_get_content(GF_BitStream *bs, unsigned char **output, u32 *outSize);
/* skips nb bytes */
void gf_bs_skip_bytes(GF_BitStream *bs, u64 nbBytes);
/* rewind nb bytes (files only) */
void gf_bs_rewind(GF_BitStream *bs, u64 nbBytes);
/*seek to offset after the begining of the stream */
GF_Err gf_bs_seek(GF_BitStream *bs, u64 offset);

/* READ modes only: peek numBits as a UINT 
 @byte_offset: if set, bitstream is aligned and moved from byte_offset before peeking (byte-aligned picking)
	otherwise, bitstream is NOT aligned and bits are peeked from current state
*/
u32 gf_bs_peek_bits(GF_BitStream *bs, u32 numBits, u32 byte_offset);

/*for READ mode: returns number of available bits if last byte in stream, or 8 otherwise*/
u8 gf_bs_bits_available(GF_BitStream *bs);

/* Gets the size of a bitstream */
u64 gf_bs_get_position(GF_BitStream *bs);
/* Gets the size of a bitstream */
u64 gf_bs_get_size(GF_BitStream *bs);
/* for FILE mode, performs a seek till end of file before returning the size */
u64 gf_bs_get_refreshed_size(GF_BitStream *bs);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_BITSTREAM_H_*/

