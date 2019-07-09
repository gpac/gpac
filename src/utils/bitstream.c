/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/bitstream.h>

/*the default size for new streams allocation...*/
#define BS_MEM_BLOCK_ALLOC_SIZE		512

/*private types*/
enum
{
	GF_BITSTREAM_FILE_READ = GF_BITSTREAM_WRITE_DYN + 1,
	GF_BITSTREAM_FILE_WRITE,
};

struct __tag_bitstream
{
	/*original stream data*/
	FILE *stream;

	/*original data*/
	char *original;
	/*the size of our buffer in bytes*/
	u64 size;
	/*current position in BYTES*/
	u64 position;
	/*the byte readen/written*/
	u32 current;
	/*the number of bits in the current byte*/
	u32 nbBits;
	/*the bitstream mode*/
	u32 bsmode;

	void (*EndOfStream)(void *par);
	void *par;


	char *buffer_io;
	u32 buffer_io_size, buffer_written;

	Bool remove_emul_prevention_byte;
	u32 nb_zeros;

	GF_Err (*on_block_out)(void *cbk, u8 *data, u32 block_size);
	void *usr_data;
	u64 bytes_out;
	Bool prevent_dispatch;

	u64 cookie;
};

GF_Err gf_bs_reassign_buffer(GF_BitStream *bs, const u8 *buffer, u64 BufferSize)
{
	if (!bs) return GF_BAD_PARAM;
	if (bs->bsmode == GF_BITSTREAM_READ) {
		bs->original = (char*)buffer;
		bs->size = BufferSize;
		bs->position = 0;
		bs->current = 0;
		bs->nbBits = 8;
		bs->current = 0;
		bs->nb_zeros = 0;
		return GF_OK;
	}
	if (bs->bsmode==GF_BITSTREAM_WRITE) {
		if (!buffer || !BufferSize) return GF_BAD_PARAM;
		bs->original = (char*)buffer;
		bs->size = BufferSize;
		bs->position = 0;
		bs->current = 0;
		bs->nbBits = 0;
		bs->current = 0;
		return GF_OK;
	}
	if (bs->bsmode!=GF_BITSTREAM_WRITE_DYN) return GF_BAD_PARAM;
	if (bs->original) return GF_BAD_PARAM;

	bs->position = 0;
	bs->current = 0;
	bs->nbBits = 0;
	bs->current = 0;
	bs->size = BufferSize ? BufferSize : BS_MEM_BLOCK_ALLOC_SIZE;
	if (buffer) {
		bs->original = (char *) buffer;
	} else {
		bs->original = (char *) gf_malloc(sizeof(char) * ((u32) bs->size));
		if (! bs->original) {
			return GF_OUT_OF_MEM;
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_BitStream *gf_bs_new(const u8 *buffer, u64 BufferSize, u32 mode)
{
	GF_BitStream *tmp;
	if ( (buffer && ! BufferSize)) return NULL;

	tmp = (GF_BitStream *)gf_malloc(sizeof(GF_BitStream));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_BitStream));

	tmp->original = (char*)buffer;
	tmp->size = BufferSize;

	tmp->position = 0;
	tmp->current = 0;
	tmp->bsmode = mode;
	tmp->stream = NULL;

	switch (tmp->bsmode) {
	case GF_BITSTREAM_READ:
		tmp->nbBits = 8;
		tmp->current = 0;
		break;
	case GF_BITSTREAM_WRITE:
	case GF_BITSTREAM_WRITE_DYN:
		tmp->nbBits = 0;
		if (! buffer) {
			/*if BufferSize is specified, use it. This is typically used when AvgSize of
			some buffers is known, but some exceed it.*/
			if (BufferSize) {
				tmp->size = BufferSize;
			} else {
				tmp->size = BS_MEM_BLOCK_ALLOC_SIZE;
			}
			tmp->original = (char *) gf_malloc(sizeof(char) * ((u32) tmp->size));
			if (! tmp->original) {
				gf_free(tmp);
				return NULL;
			}
			tmp->bsmode = GF_BITSTREAM_WRITE_DYN;
		} else {
			tmp->original = (char*)buffer;
			tmp->size = BufferSize;
		}
		break;
	default:
		/*the stream constructor is not the same...*/
		gf_free(tmp);
		return NULL;
	}
	return tmp;
}

GF_EXPORT
GF_BitStream *gf_bs_from_file(FILE *f, u32 mode)
{
	GF_BitStream *tmp;
	if (!f) return NULL;

	tmp = (GF_BitStream *)gf_malloc(sizeof(GF_BitStream));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_BitStream));
	/*switch to internal mode*/
	mode = (mode==GF_BITSTREAM_READ) ? GF_BITSTREAM_FILE_READ : GF_BITSTREAM_FILE_WRITE;
	tmp->bsmode = mode;
	tmp->current = 0;
	tmp->nbBits = (mode == GF_BITSTREAM_FILE_READ) ? 8 : 0;
	tmp->original = NULL;
	tmp->position = 0;
	tmp->stream = f;

	/*get the size of this file (for read streams)*/
	tmp->position = gf_ftell(f);
	gf_fseek(f, 0, SEEK_END);
	tmp->size = gf_ftell(f);
	gf_fseek(f, tmp->position, SEEK_SET);
	return tmp;
}

GF_BitStream *gf_bs_new_cbk(GF_Err (*on_block_out)(void *cbk, u8 *data, u32 block_size), void *usr_data, u32 block_size)
{
	GF_BitStream *tmp;

	if (!on_block_out) return NULL;

	tmp = (GF_BitStream *)gf_malloc(sizeof(GF_BitStream));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_BitStream));

	tmp->position = 0;
	tmp->current = 0;
	tmp->stream = NULL;
	tmp->nbBits = 0;
	tmp->size = block_size ? block_size : 10*BS_MEM_BLOCK_ALLOC_SIZE;
	tmp->original = (char *) gf_malloc(sizeof(char) * ((u32) tmp->size));
	if (! tmp->original) {
		gf_free(tmp);
		return NULL;
	}
	tmp->bsmode = GF_BITSTREAM_WRITE_DYN;
	tmp->on_block_out = on_block_out;
	tmp->usr_data = usr_data;

	return tmp;
}

void gf_bs_prevent_dispatch(GF_BitStream *bs, Bool prevent_dispatch)
{
	if (bs) bs->prevent_dispatch = prevent_dispatch;
	if (bs->on_block_out && !bs->prevent_dispatch) {
		assert(bs->position >= bs->bytes_out);
		if (bs->position > bs->bytes_out) {
			bs->on_block_out(bs->usr_data, bs->original, (u32) (bs->position - bs->bytes_out));
			bs->bytes_out = bs->position;
		}
	}
}

static void bs_flush_cache(GF_BitStream *bs)
{
	if (bs->buffer_written) {
		u32 nb_write = (u32) fwrite(bs->buffer_io, 1, bs->buffer_written, bs->stream);
		bs->size += nb_write;
		bs->position += nb_write;
		bs->buffer_written = 0;
	}
}


GF_EXPORT
GF_Err gf_bs_set_output_buffering(GF_BitStream *bs, u32 size)
{
	if (!bs->stream) return GF_OK;
	if (bs->bsmode != GF_BITSTREAM_FILE_WRITE) {
		return GF_OK;
	}
	bs_flush_cache(bs);
	bs->buffer_io = (char*)gf_realloc(bs->buffer_io, size);
	if (!bs->buffer_io) return GF_IO_ERR;
	bs->buffer_io_size = size;
	bs->buffer_written = 0;
	return GF_OK;
}


GF_EXPORT
u32 gf_bs_get_output_buffering(GF_BitStream *bs)
{
	return bs ? bs->buffer_io_size : 0;
}

GF_EXPORT
void gf_bs_del(GF_BitStream *bs)
{
	if (!bs) return;
	if (bs->on_block_out && bs->position>bs->bytes_out) {
		bs->on_block_out(bs->usr_data, bs->original, (u32) (bs->position - bs->bytes_out) );
	}
	/*if we are in dynamic mode (alloc done by the bitstream), free the buffer if still present*/
	if ((bs->bsmode == GF_BITSTREAM_WRITE_DYN) && bs->original) gf_free(bs->original);
	if (bs->buffer_io) {
		bs_flush_cache(bs);
		gf_free(bs->buffer_io);
	}
	gf_free(bs);
}

GF_EXPORT
void gf_bs_enable_emulation_byte_removal(GF_BitStream *bs, Bool do_remove)
{
	if (bs) {
		bs->remove_emul_prevention_byte = do_remove;
		bs->nb_zeros = 0;
	}
}

/*returns 1 if aligned wrt current mode, 0 otherwise*/
Bool gf_bs_is_align(GF_BitStream *bs)
{
	switch (bs->bsmode) {
	case GF_BITSTREAM_READ:
	case GF_BITSTREAM_FILE_READ:
		return ( (8 == bs->nbBits) ? GF_TRUE : GF_FALSE);
	default:
		return !bs->nbBits;
	}
}


/*fetch a new byte in the bitstream switch between packets*/
static u8 BS_ReadByte(GF_BitStream *bs)
{
	if (bs->bsmode == GF_BITSTREAM_READ) {
		u8 res;
		if (bs->position >= bs->size) {
			if (bs->EndOfStream) bs->EndOfStream(bs->par);
			return 0;
		}
		res = bs->original[bs->position++];

		if (bs->remove_emul_prevention_byte) {
			if ((bs->nb_zeros==2) && (res==0x03) && (bs->position<bs->size) && (bs->original[bs->position]<0x04)) {
				bs->nb_zeros = 0;
				res = bs->original[bs->position++];
			}
			if (!res) bs->nb_zeros++;
			else bs->nb_zeros = 0;
		}
		return res;
	}
	if (bs->buffer_io)
		bs_flush_cache(bs);

	/*we are in FILE mode, test for end of file*/
	if (!feof(bs->stream)) {
		u8 res;
		assert(bs->position<=bs->size);
		bs->position++;
		res = fgetc(bs->stream);

		if (bs->remove_emul_prevention_byte) {
			if ((bs->nb_zeros==2) && (res==0x03) && (bs->position<bs->size)) {
				u8 next = fgetc(bs->stream);
				if (next < 0x04) {
					bs->nb_zeros = 0;
					res = next;
					bs->position++;
				} else {
					gf_bs_seek(bs, bs->position);
				}
			}
			if (!res) bs->nb_zeros++;
			else bs->nb_zeros = 0;
		}
		return res;
	}
	if (bs->EndOfStream) {
		bs->EndOfStream(bs->par);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[BS] Attempt to overread bitstream\n"));
	}
	assert(bs->position <= 1+bs->size);
	return 0;
}

#define NO_OPTS

#ifndef NO_OPTS
static u32 bit_mask[] = {0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1};
static u32 bits_mask[] = {0x0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F};
#endif

GF_EXPORT
u8 gf_bs_read_bit(GF_BitStream *bs)
{
	if (bs->nbBits == 8) {
		bs->current = BS_ReadByte(bs);
		bs->nbBits = 0;
	}
#ifdef NO_OPTS
	{
		s32 ret;
		bs->current <<= 1;
		bs->nbBits++;
		ret = (bs->current & 0x100) >> 8;
		return (u8) ret;
	}
#else
	return (u8) (bs->current & bit_mask[bs->nbBits++]) ? 1 : 0;
#endif

}

GF_EXPORT
u32 gf_bs_read_int(GF_BitStream *bs, u32 nBits)
{
	u32 ret;

#ifndef NO_OPTS
	if (nBits + bs->nbBits <= 8) {
		bs->nbBits += nBits;
		ret = (bs->current >> (8 - bs->nbBits) ) & bits_mask[nBits];
		return ret;
	}
#endif
	ret = 0;
	while (nBits-- > 0) {
		ret <<= 1;
		ret |= gf_bs_read_bit(bs);
	}
	return ret;
}

GF_EXPORT
u32 gf_bs_read_u8(GF_BitStream *bs)
{
	assert(bs->nbBits==8);
	return (u32) BS_ReadByte(bs);
}

#if 0
GF_EXPORT
u32 gf_bs_read_u8_until_delimiter(GF_BitStream *bs, u8 delimiter, u8* out, u32 max_length) {
	u32 i = 0;
	char token=0;
	u64 cur_pos = gf_bs_get_position(bs);

	if (!max_length) out = NULL;

	while(gf_bs_available(bs) && (!max_length || i < max_length)) {
		gf_bs_read_data(bs, &token, 1);
		if (token == delimiter) goto found;
		if (out) out[i] = token;
		i++;
	}

	/* Delimiter not found */
	gf_bs_seek(bs, cur_pos);
	return 0;

found:
	return i;
}
#endif

GF_EXPORT
u32 gf_bs_read_u16(GF_BitStream *bs)
{
	u32 ret;
	assert(bs->nbBits==8);
	ret = BS_ReadByte(bs);
	ret<<=8;
	ret |= BS_ReadByte(bs);
	return ret;
}


GF_EXPORT
u32 gf_bs_read_u24(GF_BitStream *bs)
{
	u32 ret;
	assert(bs->nbBits==8);
	ret = BS_ReadByte(bs);
	ret<<=8;
	ret |= BS_ReadByte(bs);
	ret<<=8;
	ret |= BS_ReadByte(bs);
	return ret;
}

GF_EXPORT
u32 gf_bs_read_u32(GF_BitStream *bs)
{
	u32 ret;
	assert(bs->nbBits==8);
	ret = BS_ReadByte(bs);
	ret<<=8;
	ret |= BS_ReadByte(bs);
	ret<<=8;
	ret |= BS_ReadByte(bs);
	ret<<=8;
	ret |= BS_ReadByte(bs);
	return ret;
}

GF_EXPORT
u64 gf_bs_read_u64(GF_BitStream *bs)
{
	u64 ret;
	ret = gf_bs_read_u32(bs);
	ret<<=32;
	ret |= gf_bs_read_u32(bs);
	return ret;
}

GF_EXPORT
u64 gf_bs_read_long_int(GF_BitStream *bs, u32 nBits)
{
	u64 ret = 0;
	if (nBits>64) {
		gf_bs_read_long_int(bs, nBits-64);
		ret = gf_bs_read_long_int(bs, 64);
	} else {
		while (nBits-- > 0) {
			ret <<= 1;
			ret |= gf_bs_read_bit(bs);
		}
	}
	return ret;
}


GF_EXPORT
Float gf_bs_read_float(GF_BitStream *bs)
{
	char buf [4] = "\0\0\0";
#ifdef NO_OPTS
	s32 i;
	for (i = 0; i < 32; i++)
		buf[3-i/8] |= gf_bs_read_bit(bs) << (7 - i%8);
#else
	buf[3] = gf_bs_read_int(bs, 8);
	buf[2] = gf_bs_read_int(bs, 8);
	buf[1] = gf_bs_read_int(bs, 8);
	buf[0] = gf_bs_read_int(bs, 8);
#endif
	return (* (Float *) buf);
}

GF_EXPORT
Double gf_bs_read_double(GF_BitStream *bs)
{
	char buf [8] = "\0\0\0\0\0\0\0";
	s32 i;
	for (i = 0; i < 64; i++)
		buf[7-i/8] |= gf_bs_read_bit(bs) << (7 - i%8);
	return (* (Double *) buf);
}

GF_EXPORT
u32 gf_bs_read_data(GF_BitStream *bs, u8 *data, u32 nbBytes)
{
	u64 orig = bs->position;

	if (bs->position+nbBytes > bs->size) return 0;

	if (gf_bs_is_align(bs) ) {
		s32 bytes_read;
		switch (bs->bsmode) {
		case GF_BITSTREAM_READ:
		case GF_BITSTREAM_WRITE:
		case GF_BITSTREAM_WRITE_DYN:
			memcpy(data, bs->original + bs->position, nbBytes);
			bs->position += nbBytes;
			return nbBytes;
		case GF_BITSTREAM_FILE_READ:
		case GF_BITSTREAM_FILE_WRITE:
			if (bs->buffer_io)
				bs_flush_cache(bs);
			bytes_read = (s32) fread(data, 1, nbBytes, bs->stream);
			if (bytes_read<0) return 0;
			bs->position += bytes_read;
			return bytes_read;
		default:
			return 0;
		}
	}

	while (nbBytes-- > 0) {
		*data++ = gf_bs_read_int(bs, 8);
	}
	return (u32) (bs->position - orig);

}



static void BS_WriteByte(GF_BitStream *bs, u8 val)
{
	/*we don't allow write on READ buffers*/
	if ( (bs->bsmode == GF_BITSTREAM_READ) || (bs->bsmode == GF_BITSTREAM_FILE_READ) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[BS] Attempt to write on read bitstream\n"));
		return;
	}
	if (!bs->original && !bs->stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[BS] Attempt to write on unassigned bitstream\n"));
		return;
	}
	/*we are in MEM mode*/
	if ( (bs->bsmode == GF_BITSTREAM_WRITE) || (bs->bsmode == GF_BITSTREAM_WRITE_DYN) ) {
		//if callback mode and dispatch is not blocked, dispatch
		if (bs->on_block_out && !bs->prevent_dispatch) {
			assert(bs->position >= bs->bytes_out);
			if (bs->position - bs->bytes_out == bs->size) {
				bs->on_block_out(bs->usr_data, bs->original, (u32) (bs->position - bs->bytes_out));
				bs->bytes_out = bs->position;
			}
			if (bs->original)
				bs->original[bs->position - bs->bytes_out] = val;
			bs->position++;
			assert(bs->position >= bs->bytes_out);
			return;
		}
		//otherwise store
		if (bs->position - bs->bytes_out == bs->size) {
			/*no more space...*/
			if (bs->bsmode != GF_BITSTREAM_WRITE_DYN) return;
			/*gf_realloc if enough space...*/
			if (bs->size > 0xFFFFFFFF) return;
			bs->size = bs->size ? (bs->size * 2) : BS_MEM_BLOCK_ALLOC_SIZE;
			bs->original = (char*)gf_realloc(bs->original, (u32)bs->size);
			if (!bs->original) return;	
		}
		if (bs->original)
			bs->original[bs->position - bs->bytes_out] = val;
		bs->position++;
		return;
	}
	if (bs->buffer_io) {
		if (bs->buffer_written == bs->buffer_io_size) {
			bs_flush_cache(bs);
		}
		bs->buffer_io[bs->buffer_written] = val;
		bs->buffer_written++;
		if (bs->buffer_written == bs->buffer_io_size) {
			bs_flush_cache(bs);
		}
		return;
	}
	/*we are in FILE mode, no pb for any gf_realloc...*/
	fputc(val, bs->stream);
	/*check we didn't rewind the stream*/
	if (bs->size == bs->position) bs->size++;
	bs->position += 1;
}

static void BS_WriteBit(GF_BitStream *bs, u32 bit)
{
	bs->current <<= 1;
	bs->current |= bit;
	if (++ bs->nbBits == 8) {
		bs->nbBits = 0;
		BS_WriteByte(bs, (u8) bs->current);
		bs->current = 0;
	}
}

GF_EXPORT
void gf_bs_write_int(GF_BitStream *bs, s32 _value, s32 nBits)
{
	u32 value, nb_shift;
	if (!nBits) return;
	//move to unsigned to avoid sanitizer warnings when we pass a value not codable on the given number of bits
	//we do this when setting bit fields to all 1's
	value = (u32) _value;
	nb_shift = sizeof (s32) * 8 - nBits;
	if (nb_shift)
		value <<= nb_shift;

	while (--nBits >= 0) {
		//but check value as signed
		BS_WriteBit (bs, ((s32)value) < 0);
		value <<= 1;
	}
}

GF_EXPORT
void gf_bs_write_long_int(GF_BitStream *bs, s64 _value, s32 nBits)
{
	if (!nBits) return;
	if (nBits>64) {
		gf_bs_write_int(bs, 0, nBits-64);
		gf_bs_write_long_int(bs, _value, 64);
	} else if (nBits) {
		//cf note in gf_bs_write_int
		u64 value = (u64) _value;
		value <<= sizeof (s64) * 8 - nBits;
		while (--nBits >= 0) {
			BS_WriteBit (bs, ((s64)value) < 0);
			value <<= 1;
		}
	}
}

GF_EXPORT
void gf_bs_write_u8(GF_BitStream *bs, u32 value)
{
	assert(!bs->nbBits);
	BS_WriteByte(bs, (u8) value);
}

GF_EXPORT
void gf_bs_write_u16(GF_BitStream *bs, u32 value)
{
	assert(!bs->nbBits);
	BS_WriteByte(bs, (u8) ((value>>8)&0xff));
	BS_WriteByte(bs, (u8) ((value)&0xff));
}

GF_EXPORT
void gf_bs_write_u24(GF_BitStream *bs, u32 value)
{
	assert(!bs->nbBits);
	BS_WriteByte(bs, (u8) ((value>>16)&0xff));
	BS_WriteByte(bs, (u8) ((value>>8)&0xff));
	BS_WriteByte(bs, (u8) ((value)&0xff));
}

GF_EXPORT
void gf_bs_write_u32(GF_BitStream *bs, u32 value)
{
	assert(!bs->nbBits);
	BS_WriteByte(bs, (u8) ((value>>24)&0xff));
	BS_WriteByte(bs, (u8) ((value>>16)&0xff));
	BS_WriteByte(bs, (u8) ((value>>8)&0xff));
	BS_WriteByte(bs, (u8) ((value)&0xff));
}

GF_EXPORT
void gf_bs_write_u64(GF_BitStream *bs, u64 value)
{
	assert(!bs->nbBits);
	gf_bs_write_u32(bs, (u32) ((value>>32)&0xffffffff));
	gf_bs_write_u32(bs, (u32) (value&0xffffffff));
}

GF_EXPORT
u32 gf_bs_write_byte(GF_BitStream *bs, u8 byte, u32 repeat_count)
{
	if (!gf_bs_is_align(bs) || bs->buffer_io) {
		u32 count = 0;
		while (count<repeat_count) {
			gf_bs_write_int(bs, byte, 8);
			count++;
		}
		return count;
	}

	switch (bs->bsmode) {
	case GF_BITSTREAM_WRITE:
		if (bs->position + repeat_count > bs->size)
			return 0;
		memset(bs->original + bs->position, byte, repeat_count);
		bs->position += repeat_count;
		return repeat_count;
	case GF_BITSTREAM_WRITE_DYN:
		/*need to gf_realloc ...*/
		if (bs->position+repeat_count> bs->size) {
			u32 new_size = (u32) (bs->size*2);
			if (!new_size) new_size = BS_MEM_BLOCK_ALLOC_SIZE;

			if (bs->size + repeat_count > 0xFFFFFFFF)
				return 0;
			while (new_size < (u32) ( bs->size + repeat_count))
				new_size *= 2;
			bs->original = (char*)gf_realloc(bs->original, sizeof(u32)*new_size);
			if (!bs->original)
				return 0;
			bs->size = new_size;
		}
		memset(bs->original + bs->position, byte, repeat_count);
		bs->position += repeat_count;
		return repeat_count;
	case GF_BITSTREAM_FILE_READ:
	case GF_BITSTREAM_FILE_WRITE:

		if (gf_fwrite(&byte, 1, repeat_count, bs->stream) != repeat_count) return 0;
		if (bs->size == bs->position) bs->size += repeat_count;
		bs->position += repeat_count;
		return repeat_count;
	default:
		return 0;
	}
}



GF_EXPORT
void gf_bs_write_float(GF_BitStream *bs, Float value)
{
	u32 i;
	union
	{	float f;
		char sz [4];
	} float_value;
	float_value.f = value;

	for (i = 0; i < 32; i++)
		BS_WriteBit(bs, (float_value.sz [3 - i / 8] & 1 << (7 - i % 8)) != 0);

}

GF_EXPORT
void gf_bs_write_double (GF_BitStream *bs, Double value)
{
	u32 i;
	union
	{	Double d;
		char sz [8];
	} double_value;
	double_value.d = value;
	for (i = 0; i < 64; i++) {
		BS_WriteBit(bs, (double_value.sz [7 - i / 8] & 1 << (7 - i % 8)) != 0);
	}
}


GF_EXPORT
u32 gf_bs_write_data(GF_BitStream *bs, const u8 *data, u32 nbBytes)
{
	/*we need some feedback for this guy...*/
	u64 begin = bs->position;
	if (!nbBytes) return 0;

	if (gf_bs_is_align(bs)) {
		switch (bs->bsmode) {
		case GF_BITSTREAM_WRITE:
			if (bs->position+nbBytes > bs->size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[BS] Attempt to overwrite bitstream by %d bytes\n", bs->position + nbBytes - bs->size));
				return 0;
			}
			memcpy(bs->original + bs->position, data, nbBytes);
			bs->position += nbBytes;
			return nbBytes;
		case GF_BITSTREAM_WRITE_DYN:
			//if callback mode and dispatch not disabled, dispatch bytes
			if (bs->on_block_out && !bs->prevent_dispatch) {
				assert(bs->position >= bs->bytes_out);

				if (bs->position - bs->bytes_out + nbBytes <= bs->size) {
					memcpy(bs->original + bs->position - bs->bytes_out, data, nbBytes);
					bs->position += nbBytes;
				} else {
					bs->on_block_out(bs->usr_data, bs->original, (u32) (bs->position - bs->bytes_out) );
					bs->on_block_out(bs->usr_data, (char *) data, nbBytes);
					bs->position += nbBytes;
					bs->bytes_out = bs->position;
				}
				assert(bs->position >= bs->bytes_out);
				return nbBytes;
			}
			//otherwise store
			/*need to gf_realloc ...*/
			if (bs->position + nbBytes - bs->bytes_out > bs->size) {
				u32 new_size = (u32) (bs->size*2);
				if (!new_size) new_size = BS_MEM_BLOCK_ALLOC_SIZE;

				if (bs->size + nbBytes > 0xFFFFFFFF)
					return 0;

				while (new_size < (u32) ( bs->size + nbBytes))
					new_size *= 2;
				bs->original = (char*)gf_realloc(bs->original, sizeof(u32)*new_size);
				if (!bs->original)
					return 0;
				bs->size = new_size;
			}
			memcpy(bs->original + bs->position - bs->bytes_out, data, nbBytes);
			bs->position += nbBytes;
			return nbBytes;
		case GF_BITSTREAM_FILE_READ:
		case GF_BITSTREAM_FILE_WRITE:
			if (bs->buffer_io) {
				if (bs->buffer_written + nbBytes > bs->buffer_io_size) {
					bs_flush_cache(bs);
					if (nbBytes>bs->buffer_io_size) {
						bs->buffer_io = (char*)gf_realloc(bs->buffer_io, 2*nbBytes);
						bs->buffer_io_size = 2*nbBytes;
					}
				}
				memcpy(bs->buffer_io+bs->buffer_written, data, nbBytes);
				bs->buffer_written+=nbBytes;
				return nbBytes;
			}
			if (gf_fwrite(data, nbBytes, 1, bs->stream) != 1) return 0;
			if (bs->size == bs->position) bs->size += nbBytes;
			bs->position += nbBytes;
			return nbBytes;
		default:
			return 0;
		}
	}

	while (nbBytes) {
		gf_bs_write_int(bs, (s32) *data, 8);
		data++;
		nbBytes--;
	}
	return (u32) (bs->position - begin);
}

/*align return the num of bits read in READ mode, 0 in WRITE*/
GF_EXPORT
u8 gf_bs_align(GF_BitStream *bs)
{
	u8 res = 8 - bs->nbBits;
	if ( (bs->bsmode == GF_BITSTREAM_READ) || (bs->bsmode == GF_BITSTREAM_FILE_READ)) {
		if (res > 0) {
			gf_bs_read_int(bs, res);
		}
		return res;
	}
	if (bs->nbBits > 0) {
		gf_bs_write_int (bs, 0, res);
		return res;
	}
	return 0;
}


/*size available in the bitstream*/
GF_EXPORT
u64 gf_bs_available(GF_BitStream *bs)
{
	s64 cur, end;

	/*in WRITE mode only, this should not be called, but return something big in case ...*/
	if ( (bs->bsmode == GF_BITSTREAM_WRITE)
	        || (bs->bsmode == GF_BITSTREAM_WRITE_DYN)
	   )
		return (u64) -1;

	/*we are in MEM mode*/
	if (bs->bsmode == GF_BITSTREAM_READ) {
		if (bs->size < bs->position)
			return 0;
		else
			return (bs->size - bs->position);
	}
	/*FILE READ: assume size hasn't changed, otherwise the user shall call gf_bs_get_refreshed_size*/
	if (bs->bsmode==GF_BITSTREAM_FILE_READ) {
		if (bs->position>bs->size) return 0;
		return (bs->size - bs->position);
	}
	if (bs->buffer_io)
		bs_flush_cache(bs);

	cur = gf_ftell(bs->stream);
	gf_fseek(bs->stream, 0, SEEK_END);
	end = gf_ftell(bs->stream);
	gf_fseek(bs->stream, cur, SEEK_SET);
	return (u64) (end - cur);
}

/*call this funct to set the buffer size to the nb of bytes written
Used only in WRITE mode, as we don't know the real size during allocation...
return -1 for bad param or gf_malloc failed
return nbBytes cut*/
static s32 BS_CutBuffer(GF_BitStream *bs)
{
	s32 nbBytes;
	if ( (bs->bsmode != GF_BITSTREAM_WRITE_DYN) && (bs->bsmode != GF_BITSTREAM_WRITE)) return (u32) -1;
	/*Align our buffer or we're dead!*/
	gf_bs_align(bs);

	nbBytes = (u32) (bs->size - bs->position);
	if (!nbBytes || (nbBytes == 0xFFFFFFFF) || (bs->position >= 0xFFFFFFFF)) return 0;
	/*
		bs->original = (char*)gf_realloc(bs->original, (u32) bs->position);
		if (! bs->original) return (u32) -1;
	*/
	/*just in case, re-adjust..*/
	bs->size = bs->position;
	return nbBytes;
}

/*For DYN mode, this gets the content out without cutting the buffer to the number of written bytes*/
GF_EXPORT
void gf_bs_get_content_no_truncate(GF_BitStream *bs, u8 **output, u32 *outSize, u32 *alloc_size)
{
	/*only in WRITE MEM mode*/
	if (bs->bsmode != GF_BITSTREAM_WRITE_DYN) return;
	if (!bs->position && !bs->nbBits) {
		if (!alloc_size) {
			*output = NULL;
			gf_free(bs->original);
		} else {
			*alloc_size = (u32) bs->size;
			*output = bs->original;
		}
		*outSize = 0;
	} else {
		if (alloc_size) {
			/*Align our buffer or we're dead!*/
			gf_bs_align(bs);
			*alloc_size = (u32) bs->size;
			*outSize = (u32) bs->position;
			*output = bs->original;
		} else {
			s32 copy = BS_CutBuffer(bs);
			if (copy < 0) {
				*output = NULL;
			} else
				*output = bs->original;
			*outSize = (u32) bs->size;
		}
	}
	bs->original = NULL;
	bs->size = 0;
	bs->position = 0;
}

/*For DYN mode, this gets the content out*/
GF_EXPORT
void gf_bs_get_content(GF_BitStream *bs, u8 **output, u32 *outSize)
{
	gf_bs_get_content_no_truncate(bs, output, outSize, NULL);
}

/*	Skip nbytes.
	Align
	If READ (MEM or FILE) mode, just read n times 8 bit
	If WRITE (MEM or FILE) mode, write n times 0 on 8 bit
*/
GF_EXPORT
void gf_bs_skip_bytes(GF_BitStream *bs, u64 nbBytes)
{
	if (!bs || !nbBytes) return;

	gf_bs_align(bs);

	/*special case for file skipping...*/
	if ((bs->bsmode == GF_BITSTREAM_FILE_WRITE) || (bs->bsmode == GF_BITSTREAM_FILE_READ)) {
		if (bs->buffer_io)
			bs_flush_cache(bs);
		gf_fseek(bs->stream, nbBytes, SEEK_CUR);
		bs->position += nbBytes;
		return;
	}

	/*special case for reading*/
	if (bs->bsmode == GF_BITSTREAM_READ) {
		bs->position += nbBytes;
		return;
	}
	/*for writing we must do it this way, otherwise pb in dynamic buffers*/
	while (nbBytes) {
		gf_bs_write_int(bs, 0, 8);
		nbBytes--;
	}
}

#ifdef GPAC_ENABLE_BIFS_PMF

void gf_bs_rewind_bits(GF_BitStream *bs, u64 nbBits)
{
	u64 nbBytes;
	if (bs->bsmode != GF_BITSTREAM_READ) return;

	nbBits -= (bs->nbBits);
	nbBytes = (nbBits+8)>>3;
	nbBits = nbBytes*8 - nbBits;
	gf_bs_align(bs);
	assert(bs->position >= nbBytes);
	bs->position -= nbBytes + 1;
	gf_bs_read_int(bs, (u32)nbBits);
	return;
}

#endif

/*seek from beginning of stream: use internally even when non aligned!*/
static GF_Err BS_SeekIntern(GF_BitStream *bs, u64 offset)
{
	u32 i;
	/*if mem, do it */
	if ((bs->bsmode == GF_BITSTREAM_READ) || (bs->bsmode == GF_BITSTREAM_WRITE) || (bs->bsmode == GF_BITSTREAM_WRITE_DYN)) {
		if (offset > 0xFFFFFFFF) return GF_IO_ERR;
		if (!bs->original) return GF_BAD_PARAM;
		/*0 for write, read will be done automatically*/
		if (offset >= bs->size) {
			if ( (bs->bsmode == GF_BITSTREAM_READ) || (bs->bsmode == GF_BITSTREAM_WRITE) ) {
				if (offset > bs->size) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[BS] Attempt to seek to %d after end of bitstream %d, assuming seek to end\n", offset, bs->size));
				}
				bs->position = bs->size;
				bs->nbBits = (bs->bsmode == GF_BITSTREAM_READ) ? 8 : 0;
				return GF_OK;
			}
			/*in DYN, gf_realloc ...*/
			bs->original = (char*)gf_realloc(bs->original, (u32) (offset + 1));
			if (!bs->original)
				return GF_OUT_OF_MEM;
			for (i = 0; i < (u32) (offset + 1 - bs->size); i++) {
				bs->original[bs->size + i] = 0;
			}
			bs->size = offset + 1;
		}
		bs->current = bs->original[offset];
		bs->position = offset;
		bs->nbBits = (bs->bsmode == GF_BITSTREAM_READ) ? 8 : 0;
		return GF_OK;
	}

	if (bs->buffer_io)
		bs_flush_cache(bs);

	gf_fseek(bs->stream, offset, SEEK_SET);

	bs->position = offset;
	bs->current = 0;
	/*setup NbBits so that next acccess to the buffer will trigger read/write*/
	bs->nbBits = (bs->bsmode == GF_BITSTREAM_FILE_READ) ? 8 : 0;
	return GF_OK;
}

/*seek from beginning of stream: align before anything else*/
GF_EXPORT
GF_Err gf_bs_seek(GF_BitStream *bs, u64 offset)
{
	if (bs->on_block_out) {
		GF_Err e;
		if (offset < bs->bytes_out) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[BS] Attempt to seek on byte range already forwarded\n"));
			return GF_BAD_PARAM;
		}
		/*warning: we allow offset = bs->size for WRITE buffers*/
		if (offset > bs->size) return GF_BAD_PARAM;
		gf_bs_align(bs);
		e = BS_SeekIntern(bs, offset - bs->bytes_out);
		bs->position += bs->bytes_out;
		return e;
	}
	/*warning: we allow offset = bs->size for WRITE buffers*/
	if (offset > bs->size) return GF_BAD_PARAM;

	gf_bs_align(bs);
	return BS_SeekIntern(bs, offset);
}

/*peek bits (as int!!) from orig position (ON BYTE BOUNDARIES, from 0) - only for read ...*/
GF_EXPORT
u32 gf_bs_peek_bits(GF_BitStream *bs, u32 numBits, u64 byte_offset)
{
	u64 curPos;
	u32 curBits, ret, current, nb_zeros;

	if ( (bs->bsmode != GF_BITSTREAM_READ) && (bs->bsmode != GF_BITSTREAM_FILE_READ)) return 0;
	if (!numBits || (bs->size < bs->position + byte_offset)) return 0;

	/*store our state*/
	curPos = bs->position;
	curBits = bs->nbBits;
	current = bs->current;
	nb_zeros = bs->nb_zeros;

	if (byte_offset) {
		if (bs->remove_emul_prevention_byte) {
			while (byte_offset) {
				gf_bs_read_int(bs, 8);
				byte_offset--;
			}
		} else {
			gf_bs_seek(bs, bs->position + byte_offset);
		}
	}
	ret = gf_bs_read_int(bs, numBits);

	/*restore our cache - position*/
	gf_bs_seek(bs, curPos);
	/*to avoid re-reading our bits ...*/
	bs->nbBits = curBits;
	bs->current = current;
	bs->nb_zeros = nb_zeros;
	return ret;
}

GF_EXPORT
u64 gf_bs_get_refreshed_size(GF_BitStream *bs)
{
	s64 offset;

	switch (bs->bsmode) {
	case GF_BITSTREAM_READ:
	case GF_BITSTREAM_WRITE:
		return bs->size;

	default:
		if (bs->buffer_io)
			bs_flush_cache(bs);
		offset = gf_ftell(bs->stream);
		gf_fseek(bs->stream, 0, SEEK_END);
		bs->size = gf_ftell(bs->stream);
		gf_fseek(bs->stream, offset, SEEK_SET);
		return bs->size;
	}
}

GF_EXPORT
u64 gf_bs_get_size(GF_BitStream *bs)
{
	if (bs->buffer_io)
		return bs->size + bs->buffer_written;
	if (bs->on_block_out)
		return bs->position;
	return bs->size;
}

GF_EXPORT
u64 gf_bs_get_position(GF_BitStream *bs)
{
	if (bs->buffer_io)
		return bs->position + bs->buffer_written;
	return bs->position;
}

GF_EXPORT
u8 gf_bs_bits_available(GF_BitStream *bs)
{
	if (bs->size > bs->position) return 8;
	if (bs->nbBits < 8) return (8-bs->nbBits);
	return 0;
}

GF_EXPORT
void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par)
{
	bs->EndOfStream = EndOfStream;
	bs->par = par;
}


GF_EXPORT
u64 gf_bs_read_u64_le(GF_BitStream *bs)
{
	u64 ret, v;
	ret = gf_bs_read_int(bs, 8);
	v = gf_bs_read_int(bs, 8);
	v<<=8;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=16;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=24;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=32;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=40;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=48;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=56;
	ret |= v;
	return ret;
}

GF_EXPORT
u32 gf_bs_read_u32_le(GF_BitStream *bs)
{
	u32 ret, v;
	ret = gf_bs_read_int(bs, 8);
	v = gf_bs_read_int(bs, 8);
	v<<=8;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=16;
	ret |= v;
	v = gf_bs_read_int(bs, 8);
	v<<=24;
	ret |= v;
	return ret;
}

GF_EXPORT
u16 gf_bs_read_u16_le(GF_BitStream *bs)
{
	u32 ret, v;
	ret = gf_bs_read_int(bs, 8);
	v = gf_bs_read_int(bs, 8);
	v<<=8;
	ret |= v;
	return ret;
}

GF_EXPORT
void gf_bs_write_u32_le(GF_BitStream *bs, u32 val)
{
	gf_bs_write_int(bs, val & 0xFF, 8);
	gf_bs_write_int(bs, val>>8, 8);
	gf_bs_write_int(bs, val>>16, 8);
	gf_bs_write_int(bs, val>>24, 8);
}

GF_EXPORT
void gf_bs_write_u16_le(GF_BitStream *bs, u32 val)
{
	gf_bs_write_int(bs, val & 0xFF, 8);
	gf_bs_write_int(bs, val>>8, 8);
}

GF_EXPORT
u32 gf_bs_get_bit_offset(GF_BitStream *bs)
{
	if (bs->stream) return 0;
	if (bs->bsmode==GF_BITSTREAM_READ) return (u32) ( (bs->position - 1) * 8 + bs->nbBits);
	return (u32) ( (bs->position ) * 8 + bs->nbBits);
}

GF_EXPORT
u32 gf_bs_get_bit_position(GF_BitStream *bs)
{
	if (bs->stream) return 0;
	return bs->nbBits;
}

GF_EXPORT
u32 gf_bs_read_vluimsbf5(GF_BitStream *bs)
{
	u32 nb_words = 0;
	while (gf_bs_read_int(bs, 1)) nb_words++;
	nb_words++;
	return gf_bs_read_int(bs, 4*nb_words);
}

GF_EXPORT
void gf_bs_truncate(GF_BitStream *bs)
{
	bs->size = bs->position;
}


GF_EXPORT
GF_Err gf_bs_transfer(GF_BitStream *dst, GF_BitStream *src, Bool keep_src)
{
	u8 *data;
	u32 data_len, written;

	data = NULL;
	data_len = 0;
	gf_bs_get_content(src, &data, &data_len);
	if (!data || !data_len)
	{
		if (data) {
			if (keep_src) {
				src->original = data;
				src->size = data_len;
			} else {
				gf_free(data);
			}
			return GF_IO_ERR;
		}
		return GF_OK;
	}
	written = gf_bs_write_data(dst, data, data_len);
	if (keep_src) {
		src->original = data;
		src->size = data_len;
	} else {
		gf_free(data);
	}
	if (written<data_len) return GF_IO_ERR;
	return GF_OK;
}

GF_EXPORT
void gf_bs_flush(GF_BitStream *bs)
{
	if (bs->buffer_io)
		bs_flush_cache(bs);

	if (!bs->stream) return;

	if (bs->bsmode != GF_BITSTREAM_FILE_WRITE) return;
	fflush(bs->stream);
}

#if 0 //unused
/*!
 *\brief Reassigns FILE object for stream-based bitstreams
 *
 *Reassigns FILE object for stream-based bitstreams. Automatically sets the stream position to the bitstream position
 *\param bs the target bitstream
 *\param stream the new stream to assign
 */
void gf_bs_reassign(GF_BitStream *bs, FILE *stream)
{
	if (!bs) return;
	switch (bs->bsmode) {
	case GF_BITSTREAM_FILE_WRITE:
	case GF_BITSTREAM_FILE_READ:
		bs->stream = stream;
		if (gf_ftell(stream) != bs->position)
			gf_bs_seek(bs, bs->position);
		break;
	}
}
#endif

u64 gf_bs_set_cookie(GF_BitStream *bs, u64 cookie)
{
	u64 res = 0;
	if (bs) {
		res = bs->cookie;
		bs->cookie = cookie;
	}
	return res;
}

u64 gf_bs_get_cookie(GF_BitStream *bs)
{
	if (!bs) return 0;
	return bs->cookie;
}
