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

#include <gpac/base_coding.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_CORE_TOOLS

static const char base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

GF_EXPORT
u32 gf_base64_encode(const u8 *_in, u32 inSize, u8 *_out, u32 outSize)
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
		} else {
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
	while (len<4) {
		out[len] = (char) 0xFF;
		len++;
	}
	return pos+i;
}

GF_EXPORT
u32 gf_base64_decode(u8 *in_buf, u32 inSize, u8 *out, u32 outSize)
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

static const char base_16[] = "0123456789abcdef";

GF_EXPORT
u32 gf_base16_encode(u8 *_in, u32 inSize, u8 *_out, u32 outSize)
{
	u32 i = 0;
	unsigned char *in = (unsigned char *)_in;
	unsigned char *out = (unsigned char *)_out;

	if (outSize < (inSize * 2)+1) return 0;

	for (i=0; i<inSize; i++) {
		out[2*i] = base_16[((in[i] & 0xf0) >> 4)];
		out[2*i+1] = base_16[(in[i] & 0x0f)];
	}
	out[(inSize * 2)] = 0;

	return inSize * 2;
}

#define char16(nb) (((nb) < 97) ? ((nb)-48) : ((nb)-87))

GF_EXPORT
u32 gf_base16_decode(u8 *in, u32 inSize, u8 *out, u32 outSize)
{
	u32 j=0;
	u32 c[2] = {0,0};

	if (outSize < (inSize / 2)) return 0;
	if ((inSize % 2) != 0) return 0;

	for (j=0; j<inSize/2; j++) {
		c[0] = char16(in[2*j]);
		c[1] = char16(in[2*j+1]);
		out[j] = ((c[0] << 4)&0xf0) | (c[1]&0x0f);
	}
	out[inSize/2] = 0;

	return j;
}


#ifndef GPAC_DISABLE_ZLIB

#include <zlib.h>

#define ZLIB_COMPRESS_SAFE	4

GF_EXPORT
GF_Err gf_gz_compress_payload(u8 **data, u32 data_len, u32 *max_size)
{
	z_stream stream;
	int err;
	char *dest = (char *)gf_malloc(sizeof(char)*data_len*ZLIB_COMPRESS_SAFE);
	stream.next_in = (Bytef*)(*data) ;
	stream.avail_in = (uInt)data_len ;
	stream.next_out = ( Bytef*)dest;
	stream.avail_out = (uInt)data_len*ZLIB_COMPRESS_SAFE;
	stream.zalloc = (alloc_func)NULL;
	stream.zfree = (free_func)NULL;
	stream.opaque = (voidpf)NULL;

	err = deflateInit2(&stream, 9, Z_DEFLATED, 16+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);

	if (err != Z_OK) {
		gf_free(dest);
		return GF_IO_ERR;
	}

	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		gf_free(dest);
		return GF_IO_ERR;
	}
	if (data_len <stream.total_out) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[GZ] compressed data (%d) larger than input (%d)\n", (u32) stream.total_out, (u32) data_len ));
	}

	if (*max_size < stream.total_out) {
		*max_size = data_len*ZLIB_COMPRESS_SAFE;
		*data = (char*)gf_realloc(*data, *max_size * sizeof(char));
	}

	memcpy((*data) , dest, sizeof(char)*stream.total_out);
	*max_size = (u32) stream.total_out;
	gf_free(dest);

	deflateEnd(&stream);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_gz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size)
{
	z_stream d_stream;
	GF_Err e = GF_OK;
	int err;
	Bool owns_buffer=GF_TRUE;
	u32 size = 4096;

	if (! *uncompressed_data) {
		*uncompressed_data = (char*)gf_malloc(sizeof(char)*4096);
		if (!*uncompressed_data) return GF_OUT_OF_MEM;
	} else {
		owns_buffer = GF_FALSE;
		size = *out_size;
	}

	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;
	d_stream.next_in  = (Bytef*)data;
	d_stream.avail_in = data_len;
	d_stream.next_out = (Bytef*) *uncompressed_data;
	d_stream.avail_out = size;

	err = inflateInit2(&d_stream, 16+MAX_WBITS);

	if (err == Z_OK) {
		while (d_stream.total_in < data_len) {
			err = inflate(&d_stream, Z_NO_FLUSH);
			if (err < Z_OK) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
			if (err==Z_STREAM_END) break;

			size *= 2;
			*uncompressed_data = (char*)gf_realloc(*uncompressed_data, sizeof(char)*size);
			if (!*uncompressed_data) return GF_OUT_OF_MEM;
			d_stream.avail_out = (u32) (size - d_stream.total_out);
			d_stream.next_out = (Bytef*) ( *uncompressed_data + d_stream.total_out);
		}
		*out_size = (u32) d_stream.total_out;
		inflateEnd(&d_stream);
		return e;
	}
	if (e!=GF_OK) {
		if (owns_buffer) {
			gf_free(*uncompressed_data);
			*uncompressed_data = NULL;
		}
	}
	return e;
}
#else
GF_EXPORT
GF_Err gf_gz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size)
{
	*out_size = 0;
	return GF_NOT_SUPPORTED;
}
GF_EXPORT
GF_Err gf_gz_compress_payload(u8 **data, u32 data_len, u32 *max_size)
{
	*max_size = 0;
	return GF_NOT_SUPPORTED;
}
#endif /*GPAC_DISABLE_ZLIB*/


#ifdef GPAC_HAS_LZMA

#include <lzma.h>

#define LZMA_COMPRESS_SAFE	2

GF_EXPORT
GF_Err gf_lz_compress_payload(u8 **data, u32 data_len, u32 *max_size)
{
	u32 block_size, comp_size;
	lzma_options_lzma opt_lzma2;
	lzma_stream strm = LZMA_STREAM_INIT;

	lzma_lzma_preset(&opt_lzma2, LZMA_PRESET_DEFAULT);
	lzma_filter filters[] = {
			{ .id = LZMA_FILTER_X86, .options = NULL },
			{ .id = LZMA_FILTER_LZMA2, .options = &opt_lzma2 },
			{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};
	lzma_ret ret = lzma_stream_encoder(&strm, filters, LZMA_CHECK_NONE);
	if (ret != LZMA_OK) return GF_IO_ERR;

	block_size = data_len*LZMA_COMPRESS_SAFE;
	if (block_size < 64) block_size = 64;
	char *dest = (char *)gf_malloc(sizeof(char)*block_size);


	strm.next_in = (const uint8_t *) (*data);
	strm.avail_in = data_len;
	strm.next_out = (uint8_t *) dest;
	strm.avail_out = block_size;

	ret = lzma_code(&strm, LZMA_FINISH);
	if ((ret != LZMA_OK) && (ret != LZMA_STREAM_END)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[LZMA] compressed data failure, code %d\n", ret ));
		return GF_IO_ERR;
	}
	comp_size = block_size - strm.avail_out;

	if (data_len < comp_size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[LZMA] compressed data (%d) larger than input (%d)\n", (u32) comp_size, (u32) data_len ));
	}

	if (*max_size < comp_size) {
		*max_size = block_size;
		*data = (char*)gf_realloc(*data, block_size * sizeof(char));
	}

	memcpy((*data) , dest, sizeof(char) * comp_size);
	*max_size = (u32) comp_size;
	gf_free(dest);

	lzma_end(&strm);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_lz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
	if (ret != LZMA_OK) return GF_IO_ERR;

	Bool owns_buffer=GF_TRUE;
	u32 block_size = 4096;
	u32 done = 0;
	u32 alloc_size = 0;
	u8 block[4096];
	u8 *dst_buffer = NULL;

	if (*uncompressed_data) {
		owns_buffer = GF_FALSE;
		dst_buffer = (u8 *) *uncompressed_data;
		alloc_size = *out_size;
	} else {
		*out_size = 0;
	}

	strm.next_in = (const uint8_t *) data;
	strm.avail_in = data_len;
	strm.next_out = (uint8_t *) block;
	strm.avail_out = block_size;

	while (1) {
		lzma_ret ret = lzma_code(&strm, LZMA_FINISH);

		if ((strm.avail_out == 0) || (ret == LZMA_STREAM_END)) {
			u32 uncomp_size = block_size - strm.avail_out;

			if (done + uncomp_size > alloc_size) {
				alloc_size = done + uncomp_size;
				dst_buffer = gf_realloc(dst_buffer, alloc_size);
				*out_size = alloc_size;
			}
			memcpy(dst_buffer + done, block, uncomp_size);
			done += uncomp_size;

			strm.next_out = (uint8_t *) block;
			strm.avail_out = block_size;
		}
		if (ret == LZMA_STREAM_END) break;
		if (ret != LZMA_OK) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[LZMA] error decompressing data: %d\n", ret ));
			if (owns_buffer) gf_free(dst_buffer);
			return GF_IO_ERR;
		}
	}

	*uncompressed_data = dst_buffer;
	*out_size = done;
	return GF_OK;
}
#else
GF_EXPORT
GF_Err gf_lz_decompress_payload(u8 *data, u32 data_len, u8 **uncompressed_data, u32 *out_size)
{
	*out_size = 0;
	return GF_NOT_SUPPORTED;
}
GF_EXPORT
GF_Err gf_lz_compress_payload(u8 **data, u32 data_len, u32 *max_size)
{
	*max_size = 0;
	return GF_NOT_SUPPORTED;
}

#endif /*GPAC_HAS_LZMA*/

#endif /* GPAC_DISABLE_CORE_TOOLS*/
