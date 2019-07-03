/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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

#include <gpac/internal/media_dev.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/constants.h>

#ifdef GPAC_HAS_PNG

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#pragma comment(lib, "libpng")
#endif

/*include png.h before setjmp.h, otherwise we get compilation errors*/
#include <png.h>

#endif /*GPAC_HAS_PNG*/


#ifdef GPAC_HAS_JPEG

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#pragma comment(lib, "libjpeg")
#endif

#include <jpeglib.h>
#include <setjmp.h>

#endif	/*GPAC_HAS_JPEG*/

GF_EXPORT
void gf_img_parse(GF_BitStream *bs, u32 *codecid, u32 *width, u32 *height, u8 **dsi, u32 *dsi_len)
{
	u8 b1, b2, b3;
	u32 size, type;
	u64 pos;
	pos = gf_bs_get_position(bs);
	gf_bs_seek(bs, 0);

	*width = *height = 0;
	*codecid = 0;
	if (dsi) {
		*dsi = NULL;
		*dsi_len = 0;
	}

	b1 = gf_bs_read_u8(bs);
	b2 = gf_bs_read_u8(bs);
	b3 = gf_bs_read_u8(bs);
	/*JPEG*/
	if ((b1==0xFF) && (b2==0xD8) && (b3==0xFF)) {
		u32 offset = 0;
		u32 Xdens, Ydens, nb_comp;
		gf_bs_read_u8(bs);
		gf_bs_skip_bytes(bs, 10);	/*2 size, 5 JFIF\0, 2 version, 1 units*/
		Xdens = gf_bs_read_int(bs, 16);
		Ydens = gf_bs_read_int(bs, 16);
		nb_comp = 0;

		/*get frame header FFC0*/
		while (gf_bs_available(bs)) {
			u32 type, w, h;
			if (gf_bs_read_u8(bs) != 0xFF) continue;
			if (!offset) offset = (u32)gf_bs_get_position(bs) - 1;

			type = gf_bs_read_u8(bs);
			/*process all Start of Image markers*/
			switch (type) {
			case 0xC0:
			case 0xC1:
			case 0xC2:
			case 0xC3:
			case 0xC5:
			case 0xC6:
			case 0xC7:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCD:
			case 0xCE:
			case 0xCF:
				gf_bs_skip_bytes(bs, 3);
				h = gf_bs_read_int(bs, 16);
				w = gf_bs_read_int(bs, 16);
				if ((w > *width) || (h > *height)) {
					*width = w;
					*height = h;
				}
				nb_comp = gf_bs_read_int(bs, 8);
				break;
			}
		}
		*codecid = GF_CODECID_JPEG;
		if (dsi) {
			GF_BitStream *bs_dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u16(bs_dsi, offset);
			gf_bs_write_u16(bs_dsi, Xdens);
			gf_bs_write_u16(bs_dsi, Ydens);
			gf_bs_write_u8(bs_dsi, nb_comp);
			gf_bs_get_content(bs_dsi, dsi, dsi_len);
			gf_bs_del(bs_dsi);
		}
	}
	/*PNG*/
	else if ((b1==0x89) && (b2==0x50) && (b3==0x4E)) {
		/*check for PNG sig*/
		if ( (gf_bs_read_u8(bs) != 0x47) || (gf_bs_read_u8(bs) != 0x0D) || (gf_bs_read_u8(bs) != 0x0A)
		        || (gf_bs_read_u8(bs) != 0x1A) || (gf_bs_read_u8(bs) != 0x0A) ) goto exit;
		gf_bs_read_u32(bs);
		/*check for PNG IHDR*/
		if ( (gf_bs_read_u8(bs) != 'I') || (gf_bs_read_u8(bs) != 'H')
		        || (gf_bs_read_u8(bs) != 'D') || (gf_bs_read_u8(bs) != 'R')) goto exit;

		*width = gf_bs_read_u32(bs);
		*height = gf_bs_read_u32(bs);
		*codecid = GF_CODECID_PNG;
	}
	/*try j2k*/
	else {
		u32 jp2h_size=0, jp2h_start=0;
		size = gf_bs_read_u8(bs);
		type = gf_bs_read_u32(bs);
		if ( ((size==12) && (type==GF_ISOM_BOX_TYPE_JP ))
	        || (type==GF_ISOM_BOX_TYPE_JP2H ) ) {

			if (type==GF_ISOM_BOX_TYPE_JP2H) {
				*codecid = GF_CODECID_J2K;
				goto j2k_restart;
			}

			type = gf_bs_read_u32(bs);
			if (type!=0x0D0A870A) goto exit;

			*codecid = GF_CODECID_J2K;

			while (gf_bs_available(bs)) {
j2k_restart:
				size = gf_bs_read_u32(bs);
				type = gf_bs_read_u32(bs);
				switch (type) {
				case GF_ISOM_BOX_TYPE_JP2H:
					jp2h_size=size-8;
					jp2h_start = (u32) gf_bs_get_position(bs);
					goto j2k_restart;
				case GF_ISOM_BOX_TYPE_IHDR:
				{
					*height = gf_bs_read_u32(bs);
					*width = gf_bs_read_u32(bs);
					/*nb_comp = gf_bs_read_u16(bs);
					BPC = gf_bs_read_u8(bs);
					C = gf_bs_read_u8(bs);
					UnkC = gf_bs_read_u8(bs);
					IPR = gf_bs_read_u8(bs);
					*/
					if (dsi && jp2h_size) {
						*dsi = gf_malloc(sizeof(char)*jp2h_size);
						gf_bs_seek(bs, jp2h_start);
						gf_bs_read_data(bs, *dsi, jp2h_size);
						*dsi_len = jp2h_size;
					}
					goto exit;
				}
				break;
				default:
					gf_bs_skip_bytes(bs, size-8);
					break;
				}
			}
		}
	}

exit:
	gf_bs_seek(bs, pos);
}

#ifdef GPAC_HAS_JPEG

void gf_jpeg_nonfatal_error2(j_common_ptr cinfo, int lev)
{

}

/*JPG context while decoding*/
typedef struct
{
	struct jpeg_error_mgr pub;
	jmp_buf jmpbuf;
} JPGErr;

typedef struct
{
	/*io manager*/
	struct jpeg_source_mgr src;

	s32 skip;
	struct jpeg_decompress_struct cinfo;
} JPGCtx;

static void gf_jpeg_output_message (j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX];
	if (cinfo) {
		/* Create the message */
		(*cinfo->err->format_message) (cinfo, buffer);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPEG OUTPUT MESSAGE]: %s\n", buffer));
	}
}

static void gf_jpeg_fatal_error(j_common_ptr cinfo)
{
	if (cinfo) {
		JPGErr *err = (JPGErr *) cinfo->err;
		gf_jpeg_output_message(cinfo);
		longjmp(err->jmpbuf, 1);
	}
}

void gf_jpeg_stub(j_decompress_ptr cinfo) {}

/*a JPEG is always carried in a complete, single MPEG4 AU so no refill*/
static boolean gf_jpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
	return 0;
}

static void gf_jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	if (cinfo) {
		JPGCtx *jpx = (JPGCtx *) cinfo->src;
		if (num_bytes > (long) jpx->src.bytes_in_buffer) {
			jpx->skip = (s32) (num_bytes - jpx->src.bytes_in_buffer);
			jpx->src.next_input_byte += jpx->src.bytes_in_buffer;
			jpx->src.bytes_in_buffer = 0;
		} else {
			jpx->src.bytes_in_buffer -= num_bytes;
			jpx->src.next_input_byte += num_bytes;
			jpx->skip = 0;
		}
	}
}

#define JPEG_MAX_SCAN_BLOCK_HEIGHT		16

GF_EXPORT
GF_Err gf_img_jpeg_dec(u8 *jpg, u32 jpg_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size, u32 dst_nb_comp)
{
	s32 i, j, scans, k;
	u32 stride;
	char *scan_line, *ptr, *tmp;
	char *lines[JPEG_MAX_SCAN_BLOCK_HEIGHT];
	JPGErr jper;
	JPGCtx jpx;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		gf_jpeg_fatal_error(NULL);
		gf_jpeg_output_message(NULL);
		gf_jpeg_nonfatal_error2(NULL, 0);
		gf_jpeg_fill_input_buffer(NULL);
		gf_jpeg_skip_input_data(NULL, 0);
	}
#endif

	jpx.cinfo.err = jpeg_std_error(&(jper.pub));
	jper.pub.error_exit = gf_jpeg_fatal_error;
	jper.pub.output_message = gf_jpeg_output_message;
	jper.pub.emit_message = gf_jpeg_nonfatal_error2;
	if (setjmp(jper.jmpbuf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[JPEGDecode] : Failed to decode\n"));
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_IO_ERR;
	}

	/*create decompress struct*/
	jpeg_create_decompress(&jpx.cinfo);

	/*prepare IO*/
	jpx.src.init_source = gf_jpeg_stub;
	jpx.src.fill_input_buffer = gf_jpeg_fill_input_buffer;
	jpx.src.skip_input_data = gf_jpeg_skip_input_data;
	jpx.src.resync_to_restart = jpeg_resync_to_restart;
	jpx.src.term_source = gf_jpeg_stub;
	jpx.skip = 0;
	jpx.src.next_input_byte = (JOCTET *) jpg;
	jpx.src.bytes_in_buffer = jpg_size;
	jpx.cinfo.src = (void *) &jpx.src;

	/*read header*/
	do {
		i = jpeg_read_header(&jpx.cinfo, TRUE);
	} while (i == JPEG_HEADER_TABLES_ONLY);
	/*we're supposed to have the full image in the buffer, wrong stream*/
	if (i == JPEG_SUSPENDED) {
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	*width = jpx.cinfo.image_width;
	*height = jpx.cinfo.image_height;
	stride = *width * jpx.cinfo.num_components;

	switch (jpx.cinfo.num_components) {
	case 1:
		*pixel_format = GF_PIXEL_GREYSCALE;
		break;
	case 3:
		*pixel_format = GF_PIXEL_RGB;
		break;
	default:
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (*dst_size < *height **width * jpx.cinfo.num_components) {
		*dst_size = *height **width * jpx.cinfo.num_components;
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_BUFFER_TOO_SMALL;
	}
	if (!dst_nb_comp) dst_nb_comp = jpx.cinfo.num_components;

	scan_line = NULL;
	/*decode*/
	jpx.cinfo.do_fancy_upsampling = FALSE;
	jpx.cinfo.do_block_smoothing = FALSE;
	if (!jpeg_start_decompress(&jpx.cinfo)) {
		if (scan_line) gf_free(scan_line);
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (jpx.cinfo.rec_outbuf_height>JPEG_MAX_SCAN_BLOCK_HEIGHT) {
		if (scan_line) gf_free(scan_line);
		jpeg_destroy_decompress(&jpx.cinfo);
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[gf_img_jpeg_dec] : jpx.cinfo.rec_outbuf_height>JPEG_MAX_SCAN_BLOCK_HEIGHT\n"));
		return GF_IO_ERR;
	}

	/*read scanlines (the scan is not one line by one line so alloc a placeholder for block scaning) */
	scan_line = gf_malloc(sizeof(char) * stride * jpx.cinfo.rec_outbuf_height);
	for (i = 0; i<jpx.cinfo.rec_outbuf_height; i++) {
		lines[i] = scan_line + i * stride;
	}
	tmp = dst;
	for (j=0; j< (s32) *height; j += jpx.cinfo.rec_outbuf_height) {
		jpeg_read_scanlines(&jpx.cinfo, (unsigned char **) lines, jpx.cinfo.rec_outbuf_height);
		scans = jpx.cinfo.rec_outbuf_height;
		if (( (s32) *height - j) < scans) scans = *height - j;
		ptr = scan_line;
		/*for each line in the scan*/
		for (k = 0; k < scans; k++) {
			if (dst_nb_comp==(u32)jpx.cinfo.num_components) {
				memcpy(tmp, ptr, sizeof(char) * stride);
				ptr += stride;
				tmp += stride;
			} else {
				u32 z, c;
				for (z=0; z<*width; z++) {
					for (c=0; c<(u32)jpx.cinfo.num_components; c++) {
						if (c >= dst_nb_comp) break;
						tmp[c] = ptr[c];
					}
					ptr += jpx.cinfo.num_components;
					tmp += dst_nb_comp;
				}
			}
		}
	}

	/*done*/
	jpeg_finish_decompress(&jpx.cinfo);
	jpeg_destroy_decompress(&jpx.cinfo);

	gf_free(scan_line);

	return GF_OK;
}
#else

GF_EXPORT
GF_Err gf_img_jpeg_dec(u8 *jpg, u32 jpg_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size, u32 dst_nb_comp)
{
	return GF_NOT_SUPPORTED;
}

#endif	/*GPAC_HAS_JPEG*/


#ifdef GPAC_HAS_PNG

typedef struct
{
	char *buffer;
	u32 pos;
	u32 size;
	png_byte **rows;
} GFpng;

static void gf_png_user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	GFpng *ctx = (GFpng*)png_get_io_ptr(png_ptr);

	if (ctx->pos + length > ctx->size) {
		png_error(png_ptr, "Read Error");
	} else {
		memcpy(data, (char*) ctx->buffer + ctx->pos, length);
		ctx->pos += (u32) length;
	}
}
static void gf_png_user_error_fn(png_structp png_ptr,png_const_charp error_msg)
{
	if (png_ptr) {
		longjmp(png_jmpbuf(png_ptr), 1);
	}
}


GF_EXPORT
GF_Err gf_img_png_dec(u8 *png, u32 png_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size)
{
	GFpng udta;
	png_struct *png_ptr;
	png_info *info_ptr;
	u32 i, stride, out_size;
	png_bytep trans_alpha;
	int num_trans;
	png_color_16p trans_color;

	if ((png_size<8) || png_sig_cmp((png_bytep)png, 0, 8) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[PNG]: Wrong signature\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	udta.buffer = png;
	udta.size = png_size;
	udta.pos = 0;
	udta.rows=NULL;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		gf_png_user_error_fn(NULL, NULL);
	}
#endif

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) &udta, NULL, NULL);
	if (!png_ptr) return GF_IO_ERR;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return GF_IO_ERR;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		if (udta.rows) gf_free(udta.rows);
		return GF_IO_ERR;
	}
	png_set_read_fn(png_ptr, &udta, (png_rw_ptr) gf_png_user_read_data);
	png_set_error_fn(png_ptr, &udta, (png_error_ptr) gf_png_user_error_fn, NULL);

	png_read_info(png_ptr, info_ptr);

	/*unpaletize*/
	if (png_get_color_type(png_ptr, info_ptr)==PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_ptr);
		png_read_update_info(png_ptr, info_ptr);
	}
	num_trans = 0;
	png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color);
	if (num_trans) {
		png_set_tRNS_to_alpha(png_ptr);
		png_read_update_info(png_ptr, info_ptr);
	}
	*width = (u32) png_get_image_width(png_ptr, info_ptr);
	*height = (u32) png_get_image_height(png_ptr, info_ptr);

	switch (png_get_color_type(png_ptr, info_ptr)) {
	case PNG_COLOR_TYPE_GRAY:
		*pixel_format = GF_PIXEL_GREYSCALE;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		*pixel_format = GF_PIXEL_GREYALPHA;
		break;
	case PNG_COLOR_TYPE_RGB:
		*pixel_format = GF_PIXEL_RGB;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		*pixel_format = GF_PIXEL_RGBA;
		break;
	default:
		png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return GF_NOT_SUPPORTED;

	}

	out_size = (u32) (png_get_rowbytes(png_ptr, info_ptr) * png_get_image_height(png_ptr, info_ptr));
	/*new cfg, reset*/
	if (*dst_size != out_size) {
		*dst_size  = out_size;
		png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return GF_BUFFER_TOO_SMALL;
	}
	*dst_size  = out_size;
	if (!dst) return GF_BAD_PARAM;

	/*read*/
	stride = (u32) png_get_rowbytes(png_ptr, info_ptr);
	udta.rows = (png_bytepp) gf_malloc(sizeof(png_bytep) * png_get_image_height(png_ptr, info_ptr));
	for (i=0; i<png_get_image_height(png_ptr, info_ptr); i++) {
		udta.rows[i] = (png_bytep)dst + i*stride;
	}
	png_read_image(png_ptr, udta.rows);
	png_read_end(png_ptr, NULL);
	gf_free(udta.rows);

	png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
	png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
	return GF_OK;
}


void gf_png_write(png_structp png, png_bytep data, png_size_t size)
{
	GFpng *p = (GFpng *)png_get_io_ptr(png);
	memcpy(p->buffer+p->pos, data, sizeof(char)*size);
	p->pos += (u32) size;
}
void gf_png_flush(png_structp png)
{
}

/* write a png file */
GF_EXPORT
GF_Err gf_img_png_enc(u8 *data, u32 width, u32 height, s32 stride, u32 pixel_format, u8 *dst, u32 *dst_size)
{
	GFpng udta;
	png_color_8 sig_bit;
	png_int_32 k;
	png_bytep *row_pointers;
	png_structp png_ptr;
	png_infop info_ptr;
	u32 type, nb_comp;

	switch (pixel_format) {
	case GF_PIXEL_GREYSCALE:
		nb_comp = 1;
		type = PNG_COLOR_TYPE_GRAY;
		break;
	case GF_PIXEL_GREYALPHA:
		nb_comp = 1;
		type = PNG_COLOR_TYPE_GRAY_ALPHA;
		break;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
		nb_comp = 3;
		type = PNG_COLOR_TYPE_RGB;
		break;
	case GF_PIXEL_RGBA:
	case GF_PIXEL_ARGB:
		nb_comp = 4;
		type = PNG_COLOR_TYPE_RGB_ALPHA;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	if (*dst_size < width*height*nb_comp) return GF_BUFFER_TOO_SMALL;


	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	//      png_voidp user_error_ptr, user_error_fn, user_warning_fn);

	if (png_ptr == NULL) return GF_IO_ERR;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		gf_png_flush(NULL);
	}
#endif

	/* Allocate/initialize the image information data.  REQUIRED */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		return GF_IO_ERR;
	}

	/* Set error handling.  REQUIRED if you aren't supplying your own
	* error handling functions in the png_create_write_struct() call.
	*/
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	udta.buffer = dst;
	udta.pos = 0;
	png_set_write_fn(png_ptr, &udta, gf_png_write, gf_png_flush);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	/* otherwise, if we are dealing with a color image then */
	sig_bit.red = 8;
	sig_bit.green = 8;
	sig_bit.blue = 8;
	sig_bit.gray = 8;
	sig_bit.alpha = 8;
	png_set_sBIT(png_ptr, info_ptr, &sig_bit);

#if 0
	/* Optionally write comments into the image */
	text_ptr[0].key = "Title";
	text_ptr[0].text = "Mona Lisa";
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[1].key = "Author";
	text_ptr[1].text = "Leonardo DaVinci";
	text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[2].key = "Description";
	text_ptr[2].text = "<long text>";
	text_ptr[2].compression = PNG_TEXT_COMPRESSION_zTXt;
#ifdef PNG_iTXt_SUPPORTED
	text_ptr[0].lang = NULL;
	text_ptr[1].lang = NULL;
	text_ptr[2].lang = NULL;
#endif
	png_set_text(png_ptr, info_ptr, text_ptr, 3);
#endif

	png_write_info(png_ptr, info_ptr);

	/* Shift the pixels up to a legal bit depth and fill in
	* as appropriate to correctly scale the image.
	*/
	png_set_shift(png_ptr, &sig_bit);

	/* pack pixels into bytes */
	png_set_packing(png_ptr);

	if (pixel_format==GF_PIXEL_ARGB) {
//		png_set_swap_alpha(png_ptr);
		png_set_bgr(png_ptr);
	}
	switch (pixel_format) {
	case GF_PIXEL_RGBX:
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
		png_set_bgr(png_ptr);
		break;
	case GF_PIXEL_BGRX:
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
		break;
	case GF_PIXEL_BGR:
		png_set_bgr(png_ptr);
		break;
	}
	row_pointers = gf_malloc(sizeof(png_bytep)*height);
	for (k = 0; k < (s32)height; k++)
		row_pointers[k] = (png_bytep) data + k*stride;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	gf_free(row_pointers);
	/* clean up after the write, and free any memory allocated */
	png_destroy_write_struct(&png_ptr, &info_ptr);

	*dst_size = udta.pos;
	return GF_OK;
}

#else
GF_EXPORT
GF_Err gf_img_png_dec(u8 *png, u32 png_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size)
{
	return GF_NOT_SUPPORTED;
}
GF_EXPORT
GF_Err gf_img_png_enc(u8 *data, u32 width, u32 height, s32 stride, u32 pixel_format, u8 *dst, u32 *dst_size)
{
	return GF_NOT_SUPPORTED;
}

#endif	/*GPAC_HAS_PNG*/


