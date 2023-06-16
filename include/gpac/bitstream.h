/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

/*!
\file <gpac/bitstream.h>
 \brief bitstream reading and writing.
*/

/*!
\addtogroup bs_grp
\brief Bitstream Reading and Writing

This section documents the bitstream object of the GPAC framework.
\note Unless specified, all functions assume Big-Endian ordering of data in the bitstream.

@{
 */

#include <gpac/tools.h>


/*! bitstream creation modes*/
enum
{
	/*! read-only mode*/
	GF_BITSTREAM_READ = 0,
	/*! read-write mode*/
	GF_BITSTREAM_WRITE,
	/*! allows reallocating the buffer passed in WRITE mode*/
	GF_BITSTREAM_WRITE_DYN
};

/*! bitstream object*/
typedef struct __tag_bitstream GF_BitStream;

/*!
\brief bitstream constructor

Constructs a bitstream from a buffer (read or write mode)
\param buffer buffer to read or write. In WRITE mode, this can be NULL to let the bitstream object dynamically allocate memory, in which case the size param is ignored.
\param size size of the buffer given.
\param mode operation mode for this bitstream: GF_BITSTREAM_READ for read, GF_BITSTREAM_WRITE for write.
\return new bitstream object
\note In write mode on an existing data buffer, data overflow is never signaled but simply ignored, it is the caller responsibility to ensure it
 *	does not write more than possible.
 */
GF_BitStream *gf_bs_new(const u8 *buffer, u64 size, u32 mode);

/*!
\brief bitstream reassignment

Reassigns a bitstream in GF_BITSTREAM_READ mode to a new buffer
\param bs the bitstream to reassign
\param buffer buffer to read
\param size size of the buffer given.
\return error code if any
 */
GF_Err gf_bs_reassign_buffer(GF_BitStream *bs, const u8 *buffer, u64 size);

/*!
\brief bitstream constructor from file handle

Creates a bitstream from a file handle.
\param f handle of the file to use. This handle must be created with binary mode.
\param mode operation mode for this bitstream: GF_BITSTREAM_READ for read, GF_BITSTREAM_WRITE for write.
\return new bitstream object
\note - You have to open your file in the appropriated mode:\n
	- GF_BITSTREAM_READ: bitstream is constructed for reading\n
	- GF_BITSTREAM_WRITE: bitstream is constructed for writing\n
\note - you may use any of these modes for a file with read/write access.
\warning RESULTS ARE UNEXPECTED IF YOU TOUCH THE FILE WHILE USING THE BITSTREAM.
 */
GF_BitStream *gf_bs_from_file(FILE *f, u32 mode);
/*!
\brief bitstream destructor from file handle

Deletes the bitstream object. If the buffer was created by the bitstream, it is deleted if still present.
\warning If the bitstream was constructed from a FILE object in write mode, the FILE object MUST be closed after destructing the bitstream
\param bs the target bitstream
 */
void gf_bs_del(GF_BitStream *bs);

/*!
\brief bitstream constructor from callback output

Creates a bitstream from in write mode suing output callback.
\param on_block_out callback function used to write blocks.
\param usr_data user data for callback.
\param block_size internal buffer size used while dispatching bytes. If 0, defaults to 40k.
\return new bitstream object
 */
GF_BitStream *gf_bs_new_cbk(GF_Err (*on_block_out)(void *cbk, u8 *data, u32 block_size), void *usr_data, u32 block_size);

/*!
\brief bitstream constructor from callback output and preallocated buffer

Creates a bitstream from in write mode suing output callback.
\param on_block_out callback function used to write blocks.
\param usr_data user data for callback.
\param buffer internal buffer to use while dispatching bytes. If NULL, buffer is allocated internally using buffer_size.
\param buffer_size internal buffer size used while dispatching bytes. If 0, defaults to 40k.
\return new bitstream object
 */
GF_BitStream *gf_bs_new_cbk_buffer(GF_Err (*on_block_out)(void *cbk, u8 *data, u32 block_size), void *usr_data, u8 *buffer, u32 buffer_size);

/*!
\brief prevents block dispatch
Prevents byte dispatching in callback mode. This is used when seek operations are used.
\warning Block dispatch prevention acts in a counter mode: you must call as many time the function with prevent_dispatch = GF_FALSE as you called the function with prevent_dispatch = GF_TRUE
\param bs the target bitstream
\param prevent_dispatch activates temporary internal storage if set
 */
void gf_bs_prevent_dispatch(GF_BitStream *bs, Bool prevent_dispatch);


/*!
\brief integer reading

Reads an integer coded on a number of bit.
\param bs the target bitstream
\param nBits the number of bits to read
\return the integer value read.
 */
u32 gf_bs_read_int(GF_BitStream *bs, u32 nBits);
/*!
\brief large integer reading

Reads a large integer coded on a number of bit bigger than 32.
\param bs the target bitstream
\param nBits the number of bits to read
\return the large integer value read.
 */
u64 gf_bs_read_long_int(GF_BitStream *bs, u32 nBits);
/*!
\brief float reading

Reads a float coded as IEEE 32 bit format.
\param bs the target bitstream
\return the float value read.
 */
Float gf_bs_read_float(GF_BitStream *bs);
/*!
\brief double reading

Reads a double coded as IEEE 64 bit format.
\param bs the target bitstream
\return the double value read.
 */
Double gf_bs_read_double(GF_BitStream *bs);
/*!
\brief data reading

Reads a data buffer. Emultation prevention byte removal is NOT applied here
\param bs the target bitstream
\param data the data buffer to be filled
\param nbBytes the amount of bytes to read
\return the number of bytes actually read.
\warning the data buffer passed must be large enough to hold the desired amount of bytes.
 */
u32 gf_bs_read_data(GF_BitStream *bs, u8 *data, u32 nbBytes);

/*!
\brief align char reading

Reads an integer coded on 8 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\return the char value read.
 */
u32 gf_bs_read_u8(GF_BitStream *bs);

/*!
\brief align short reading

Reads an integer coded on 16 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\return the short value read.
 */
u32 gf_bs_read_u16(GF_BitStream *bs);
/*!
\brief align 24-bit integer reading

Reads an integer coded on 24 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\return the integer value read.
 */
u32 gf_bs_read_u24(GF_BitStream *bs);
/*!
\brief align integer reading

Reads an integer coded on 32 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\return the integer value read.
 */
u32 gf_bs_read_u32(GF_BitStream *bs);
/*!
\brief align large integer reading

Reads an integer coded on 64 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\return the large integer value read.
 */
u64 gf_bs_read_u64(GF_BitStream *bs);

/*!
\brief little endian integer reading

Reads an integer coded on 64 bits in little-endian order.
\param bs the target bitstream
\return the large integer value read.
 */
u64 gf_bs_read_u64_le(GF_BitStream *bs);
/*!
\brief little endian integer reading

Reads an integer coded on 32 bits in little-endian order.
\param bs the target bitstream
\return the integer value read.
 */
u32 gf_bs_read_u32_le(GF_BitStream *bs);
/*!
\brief little endian integer reading

Reads an integer coded on 16 bits in little-endian order.
\param bs the target bitstream
\return the integer value read.
 */
u16 gf_bs_read_u16_le(GF_BitStream *bs);


/*!
\brief variable length integer reading

Reads an integer coded on a variable number of 4-bits chunks. The number of chunks is given by the number of non-0 bits at the beginning.
\param bs the target bitstream
\return the integer value read.
 */
u32 gf_bs_read_vluimsbf5(GF_BitStream *bs);

/*!
\brief bit position

Returns current bit position in the bitstream - only works in memory mode.
\param bs the target bitstream
\return the integer value read.
 */
u32 gf_bs_get_bit_offset(GF_BitStream *bs);

/*!
\brief current bit position

Returns bit position in the current byte of the bitstream - only works in memory mode.
\param bs the target bitstream
\return the integer value read.
 */
u32 gf_bs_get_bit_position(GF_BitStream *bs);


/*!
\brief integer writing

Writes an integer on a given number of bits.
\param bs the target bitstream
\param value the integer to write
\param nBits number of bits used to code the integer
 */
void gf_bs_write_int(GF_BitStream *bs, s32 value, s32 nBits);
/*!
\brief large integer writing

Writes an integer on a given number of bits greater than 32.
\param bs the target bitstream
\param value the large integer to write
\param nBits number of bits used to code the integer
 */
void gf_bs_write_long_int(GF_BitStream *bs, s64 value, s32 nBits);
/*!
\brief float writing

Writes a float in IEEE 32 bits format.
\param bs the target bitstream
\param value the float to write
 */
void gf_bs_write_float(GF_BitStream *bs, Float value);
/*!
\brief double writing

Writes a double in IEEE 64 bits format.
\param bs the target bitstream
\param value the double to write
 */
void gf_bs_write_double(GF_BitStream *bs, Double value);
/*!
\brief data writing

Writes a data buffer.
\param bs the target bitstream
\param data the data to write
\param nbBytes number of data bytes to write
\return the number of written bytes
 */
u32 gf_bs_write_data(GF_BitStream *bs, const u8 *data, u32 nbBytes);

/*!
\brief align char writing

Writes an integer on 8 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\param value the char value to write
 */
void gf_bs_write_u8(GF_BitStream *bs, u32 value);
/*!
\brief align short writing

Writes an integer on 16 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\param value the short value to write
 */
void gf_bs_write_u16(GF_BitStream *bs, u32 value);
/*!
\brief align 24-bits integer writing

Writes an integer on 24 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\param value the integer value to write
 */
void gf_bs_write_u24(GF_BitStream *bs, u32 value);
/*!
\brief align integer writing

Writes an integer on 32 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\param value the integer value to write
 */
void gf_bs_write_u32(GF_BitStream *bs, u32 value);

/*!
\brief align large integer writing

Writes an integer on 64 bits starting at a byte boundary in the bitstream.
\warning you must not use this function if the bitstream is not aligned
\param bs the target bitstream
\param value the large integer value to write
 */
void gf_bs_write_u64(GF_BitStream *bs, u64 value);



/*!
\brief little endian integer writing

Writes an integer on 32 bits in little-endian order.
\param bs the target bitstream
\param value the integer value to write
 */
void gf_bs_write_u32_le(GF_BitStream *bs, u32 value);

/*!
\brief little endian large integer writing

Writes an integer on 64 bits in little-endian order.
\param bs the target bitstream
\param value the integer value to write
 */
void gf_bs_write_u64_le(GF_BitStream *bs, u64 value);

/*!
\brief little endian short writing

Writes an integer on 16 bits in little-endian order.
\param bs the target bitstream
\param value the short value to write
 */
void gf_bs_write_u16_le(GF_BitStream *bs, u32 value);

/*!
\brief write byte multiple times

Writes a give byte multiple times.
\param bs the target bitstream
\param byte the byte value to write
\param count the number of times the byte should be written
\return the number of bytes written
 */
u32 gf_bs_write_byte(GF_BitStream *bs, u8 byte, u32 count);

/*!
\brief end of bitstream management

Assigns a notification callback function for end of stream signaling in read mode
\param bs the target bitstream
\param EndOfStream the notification function to use
\param par opaque user data passed to the bitstream
 */
void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);

/*!
\brief bitstream alignment checking

Checks if bitstream position is aligned to a byte boundary.
\param bs the target bitstream
\return GF_TRUE if aligned with regard to the read/write mode, GF_FALSE otherwise
 */
Bool gf_bs_is_align(GF_BitStream *bs);

/*!
\brief bitstream alignment

Aligns bitstream to next byte boundary. In write mode, this will write 0 bit values until alignment.
\param bs the target bitstream
\return the number of bits read/written until alignment
 */
u8 gf_bs_align(GF_BitStream *bs);
/*!
\brief capacity query

Returns the number of bytes still available in the bitstream in read mode.
\param bs the target bitstream
\return the number of bytes still available in read mode, -1 in write modes.
 */
u64 gf_bs_available(GF_BitStream *bs);
/*!
\brief buffer fetching

Fetches the internal bitstream buffer in write mode. If a buffer was given at the bitstream construction, or if the bitstream is in read mode, this does nothing.
\param bs the target bitstream
\param output address of the memory block allocated by the bitstream.
\param outSize size of the allocated memory block.
\note
	- It is the user responsibility to destroy the allocated buffer
	- Once this function has been called, the internal bitstream buffer is reseted.
 */
void gf_bs_get_content(GF_BitStream *bs, u8 **output, u32 *outSize);

/*!
\brief buffer fetching

Fetches the internal bitstream buffer in write mode. If a buffer was given at the bitstream construction, or if the bitstream is in read mode, this does nothing. Retrieves both the allocated buffer size and the written size
\param bs the target bitstream
\param output address of the memory block allocated by the bitstream.
\param outSize  number of bytes written in the allocated memory block.
\param allocSize  size of the allocated memory block.
\note
	- It is the user responsibility to destroy the allocated buffer
	- Once this function has been called, the internal bitstream buffer is reseted.
 */
void gf_bs_get_content_no_truncate(GF_BitStream *bs, u8 **output, u32 *outSize, u32 *allocSize);

/*!
\brief byte skipping

Skips bytes in the bitstream. In Write mode, this will write the 0 integer value for memory-based bitstreams or seek the stream
 for file-based bitstream. In read mode, emultation prevention byte  is applied if enabled
\param bs the target bitstream
\param nbBytes the number of bytes to skip
 */
void gf_bs_skip_bytes(GF_BitStream *bs, u64 nbBytes);

/*!
\brief bitstream seeking

Seeks the bitstream to a given offset after the beginning of the stream. This will perform alignment of the bitstream in all modes.
\warning Results are unpredictable if seeking beyond the bitstream end is performed.
\param bs the target bitstream
\param offset buffer/file offset to seek to
\return error if any
 */
GF_Err gf_bs_seek(GF_BitStream *bs, u64 offset);

/*!
\brief bitstream truncation

Truncates the bitstream at the current position
\param bs the target bitstream
 */
void gf_bs_truncate(GF_BitStream *bs);

/*!
\brief bit peeking

Peeks a given number of bits (read without moving the position indicator) for read modes only.
\param bs the target bitstream
\param numBits the number of bits to peek
\param byte_offset
	* if set, bitstream is aligned and moved from byte_offset before peeking (byte-aligned picking)
	* otherwise, bitstream is not aligned and bits are peeked from current state
\return the integer value read
*/
u32 gf_bs_peek_bits(GF_BitStream *bs, u32 numBits, u64 byte_offset);

/*!
\brief bit reservoir query

Queries the number of bits available in read mode.
\param bs the target bitstream
\return number of available bits if position is in the last byte of the buffer/stream, 8 otherwise
 */
u8 gf_bs_bits_available(GF_BitStream *bs);

/*!
\brief position query

Returns the reading/writting position in the buffer/file.
\param bs the target bitstream
\return the read/write position of the bitstream
 */
u64 gf_bs_get_position(GF_BitStream *bs);

/*!
\brief size query

Returns the size of the associated buffer/file.
\param bs the target bitstream
\return the size of the bitstream
 */
u64 gf_bs_get_size(GF_BitStream *bs);
/*!
\brief file-based size query

Returns the size of a file-based bitstream and force a seek to end of file. This is used in case the file handle describes a file being constructed on disk while being read?
\param bs the target bitstream
\return the disk size of the associated file
 */
u64 gf_bs_get_refreshed_size(GF_BitStream *bs);


/*!
\brief transfer content from source bitstream to destination bitstream

\param dst the target bitstream
\param src the source bitstream.
\param keep_src If not set, the source bitstream is empty after calling the function
\return error if any
 */
GF_Err gf_bs_transfer(GF_BitStream *dst, GF_BitStream *src, Bool keep_src);


/*!
\brief Flushes bitstream content to disk

Flushes bitstream contet to disk
\param bs the target bitstream
 */
void gf_bs_flush(GF_BitStream *bs);

/*!
\brief NALU-based Annex B mode, only used for read mode

Enables or disable emulation byte prevention for NALU-based  annex B formats in read mode. This does NOT apply to \ref gf_bs_read_data nor  \ref gf_bs_skip_bytes
\param bs the target bitstream
\param do_remove if true, emulation prevention bytes will be removed
 */
void gf_bs_enable_emulation_byte_removal(GF_BitStream *bs, Bool do_remove);

/*!
\brief NALU-based Annex B mode, only used for read mode

Enables or disable emulation byte prevention for NALU-based  annex B formats in read mode.
\param bs the target bitstream
\return number of bytes currently removed
 */
u32 gf_bs_get_emulation_byte_removed(GF_BitStream *bs);

/*!
\brief Inserts a data block, moving bytes to the end

Inserts a data block at a given position, pushing all bytes after the insertion point to the end of the stream.
 This does NOT work if \ref gf_bs_enable_emulation_byte_removal or  \ref gf_bs_new_cbk where used.
 The position after the call will be the same as before the call. If the position is not the end of the bitstream
 all bytes after the position will be lost.
\param bs the target bitstream
\param data block to insert
\param size size of the block to insert
\param offset insertion offset from bitstream start
\return error code if any
 */
GF_Err gf_bs_insert_data(GF_BitStream *bs, u8 *data, u32 size, u64 offset);

/*!
\brief Sets cookie

Sets a 64 bit cookie (integer, pointer) on the bitstream, returning the current cookie value
\param bs the target bitstream
\param cookie the new cookie to assign
\return the cookie value before re-assign
 */
u64 gf_bs_set_cookie(GF_BitStream *bs, u64 cookie);

/*!
\brief Gets cookie

Gets the current cookie on the bitstream
\param bs the target bitstream
\return the current cookie value
 */
u64 gf_bs_get_cookie(GF_BitStream *bs);

/*!
\brief Reads string

reads utf-8 NULL-terminated string - bitstream must be aligned
\param bs the target bitstream
\return the string read or NULL if error - MUST be freed by user
 */
char *gf_bs_read_utf8(GF_BitStream *bs);

/*!
\brief Writes string

Writes utf-8 NULL-terminated string - bitstream must be aligned
\param bs the target bitstream
\param str UTF-8 string to write
\return error if any
 */
GF_Err gf_bs_write_utf8(GF_BitStream *bs, const char *str);


/*!
\brief Marks overflow access

Marks the bitstream as overflown (reading outside of buffer range). Marking is done automatically when reading but can be forced using this function.

\param bs the target bitstream
\param reset if GF_TRUE, reset overflown state, otherwise mark as overflown
 */
void gf_bs_mark_overflow(GF_BitStream *bs, Bool reset);

/*!
\brief Gets overflow state

Gets overflow state of the bitstream
\param bs the target bitstream
\return 2 if an overflow was marked by user using \ref gf_bs_mark_overflow, 1 if an overflow occured, 0 otherwise
 */
u32 gf_bs_is_overflow(GF_BitStream *bs);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_BITSTREAM_H_*/

