/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line client
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


#include <gpac/terminal.h>
#include <gpac/options.h>
#include <gpac/media_tools.h>

#ifdef WIN32
#include <windows.h>
#else
typedef struct tagBITMAPFILEHEADER
{
	u16	bfType;
	u32	bfSize;
	u16	bfReserved1;
	u16	bfReserved2;
	u32 bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
	u32	biSize;
	s32	biWidth;
	s32	biHeight;
	u16	biPlanes;
	u16	biBitCount;
	u32	biCompression;
	u32	biSizeImage;
	s32	biXPelsPerMeter;
	s32	biYPelsPerMeter;
	u32	biClrUsed;
	u32	biClrImportant;
} BITMAPINFOHEADER;

#define BI_RGB        0L

#endif


#include <gpac/internal/avilib.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>


enum
{
	DUMP_NONE = 0,
	DUMP_AVI = 1,
	DUMP_BMP = 2,
	DUMP_PNG = 3,
	DUMP_RAW = 4,
	DUMP_SHA1 = 5,

	//DuMP flags
	DUMP_DEPTH_ONLY = 1<<16,
	DUMP_RGB_DEPTH = 1<<17,
	DUMP_RGB_DEPTH_SHAPE = 1<<18
};


extern Bool is_connected;
extern GF_Terminal *term;
extern u64 Duration;
extern GF_Err last_error;

static GFINLINE u8 colmask(s32 a, s32 n)
{
	s32 mask = (1 << n) - 1;
	return (u8) (a & (0xff & ~mask)) | ((-((a >> n) & 1)) & mask);
}

static u32 put_pixel(FILE *fout, u32 type, u32 pf, char *ptr)
{
	u16 col;
	switch (pf) {
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_ARGB:
		fputc(ptr[0], fout);
		fputc(ptr[1], fout);
		fputc(ptr[2], fout);
		return 4;

	case GF_PIXEL_BGR_32:
	case GF_PIXEL_RGBA:
		fputc(ptr[2], fout);
		fputc(ptr[1], fout);
		fputc(ptr[0], fout);
		return 4;

	case GF_PIXEL_RGB_24:
		fputc(ptr[2], fout);
		fputc(ptr[1], fout);
		fputc(ptr[0], fout);
		return 3;

	case GF_PIXEL_BGR_24:
		fputc(ptr[2], fout);
		fputc(ptr[1], fout);
		fputc(ptr[0], fout);
		return 3;
	case GF_PIXEL_RGB_565:
		col = * (u16 *)ptr;
		fputc(colmask(col << 3, 3), fout);
		fputc(colmask(col >> (5 - 2), 2), fout);
		fputc(colmask(col >> (11 - 3), 3), fout);
		return 2;

	case GF_PIXEL_RGB_555:
		col = * (u16 *)ptr;
		fputc(colmask(col << 3, 3), fout);
		fputc(colmask(col >> (5 - 3), 3), fout);
		fputc(colmask(col >> (10 - 3), 3), fout);
		return 2;
	/* this is used to write the byte depthbuffer in greyscale when dumping depth*/
	case GF_PIXEL_GREYSCALE:
		/* bmp always needs 3 pixels */
		fputc(ptr[0], fout);
		fputc(ptr[0], fout);
		fputc(ptr[0], fout);
		/* if printing the characters corresponding to the float depth buffer: */
		/*
		{
		u32 i=0;
		while (ptr[i]!='\0') {
			fputc(ptr[i], fout);
			i++;
		}
		fputc('\b', fout);
		}
		*/
		return 1;


	case 0:
		fputc(ptr[0], fout);
		return 1;
	}
	return 0;
}

void write_bmp(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	char str[GF_MAX_PATH];
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;
	FILE *fout;
	u32 j, i;
	char *ptr;

	if (fb->pixel_format==GF_PIXEL_GREYSCALE) sprintf(str, "%s_%d_depth.bmp", rad_name, img_num);
	else sprintf(str, "%s_%d.bmp", rad_name, img_num);

	fout = gf_f64_open(str, "wb");
	if (!fout) return;

	memset(&fh, 0, sizeof(fh));
	fh.bfType = 19778;
	fh.bfOffBits = 14 + 40;

	memset(&fi, 0, sizeof(char)*40);
	fi.biSize = sizeof(char)*40;
	fi.biWidth = fb->width;
	fi.biHeight = fb->height;
	fi.biPlanes = 1;
	if (fb->pixel_format==GF_PIXEL_GREYSCALE) fi.biBitCount = 24;
	else fi.biBitCount = 24;
	fi.biCompression = BI_RGB;
	fi.biSizeImage = fb->width * fb->height * 3;

	/*NOT ALIGNED!!*/
	gf_fwrite(&fh.bfType, 2, 1, fout);
	gf_fwrite(&fh.bfSize, 4, 1, fout);
	gf_fwrite(&fh.bfReserved1, 2, 1, fout);
	gf_fwrite(&fh.bfReserved2, 2, 1, fout);
	gf_fwrite(&fh.bfOffBits, 4, 1, fout);

	gf_fwrite(&fi, 1, 40, fout);
//#ifndef GPAC_USE_TINYGL
	for (j=fb->height; j>0; j--) {
		ptr = fb->video_buffer + (j-1)*fb->pitch_y;
		for (i=0; i<fb->width; i++) {
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
//#else
#if 0
	for (j=0; j<fb->height; j++) {
		ptr = fb->video_buffer + j*fb->pitch;
		for (i=0; i<fb->width; i++) {
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
#endif

	fclose(fout);
}

#include <gpac/avparse.h>

void write_png(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	char str[GF_MAX_PATH];
	FILE *fout;
	u32 dst_size;
	char *dst;
	char *prev = strrchr(rad_name, '.');
	if (prev) prev[0] = '\0';
	sprintf(str, "%s_%d.png", rad_name, img_num);
	if (prev) prev[0] = '.';

	switch (fb->pixel_format) {
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
		dst_size = fb->width*fb->height*4;
		break;
	default:
		dst_size = fb->width*fb->height*3;
		break;
	}
	dst = (char*)gf_malloc(sizeof(char)*dst_size);


	fout = gf_f64_open(str, "wb");
	if (fout) {
		GF_Err e = gf_img_png_enc(fb->video_buffer, fb->width, fb->height, fb->pitch_y, fb->pixel_format, dst, &dst_size);
		if (!e) {
			gf_fwrite(dst, dst_size, 1, fout);
			fclose(fout);
		}
	}

	gf_free(dst);
#endif //GPAC_DISABLE_AV_PARSERS
}



/*writes onto a file the content of the framebuffer in *fb interpreted as the byte depthbuffer */
/*it's also possible to write a float depthbuffer by passing the floats to strings and writing chars in putpixel - see comments*/
void write_depthfile(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	FILE *fout;
	u32 i, j;
	unsigned char *depth;

	depth = (unsigned char *) fb->video_buffer;

	fout = gf_f64_open("dump_depth", "wb");
	if (!fout) return;
	for (j=0; j<fb->height;  j++) {
		for (i=0; i<fb->width; i++) {

#ifdef GPAC_USE_TINYGL
			fputc(depth[2*i+j*fb->width*sizeof(unsigned short)], fout);
			fputc(depth[2*i+j*fb->width*sizeof(unsigned short) + 1], fout);
#else
			fputc(depth[i+j*fb->width], fout);
#endif
		}
	}
	fclose(fout);
}

void write_texture_file(GF_VideoSurface *fb, char *rad_name, u32 img_num, u32 dump_mode_flags)
{

	FILE *fout;
	u32 i, j;
	unsigned char *buf;

	buf = (unsigned char *) fb->video_buffer;

	if (dump_mode_flags & DUMP_RGB_DEPTH_SHAPE) fout = gf_f64_open("dump_rgbds", "wb");
	else if (dump_mode_flags & DUMP_RGB_DEPTH) fout = gf_f64_open("dump_rgbd", "wb");
	else return;

	if (!fout) return;
	for (j=0; j<fb->height;  j++) {
		for (i=0; i<fb->width*4; i++) {
			fputc(buf[i+j*fb->pitch_y], fout);
		}
	}
	fclose(fout);
}


void write_raw(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	u32 j, i;
	char *ptr, *prev;
	char str[GF_MAX_PATH];
	FILE *fout;
	prev = strrchr(rad_name, '.');
	if (prev) prev[0] = '\0';
	if (img_num<10) {
		sprintf(str, "%s_00%d.raw", rad_name, img_num);
	} else if (img_num<100) {
		sprintf(str, "%s_0%d.raw", rad_name, img_num);
	} else {
		sprintf(str, "%s_%d.raw", rad_name, img_num);
	}

	fout = gf_f64_open(str, "wb");
	if (!fout) return;


	for (j=0; j<fb->height; j++) {
		ptr = fb->video_buffer + j*fb->pitch_y;
		for (i=0; i<fb->width; i++) {
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
	fclose(fout);
}

void write_hash(FILE *sha_out, char *buf, u32 size)
{
	u8 hash[20];
	gf_sha1_csum((u8 *)buf, size, hash);
	fwrite(hash, 1, 20, sha_out);
}


/* creates a .bmp format greyscale image of the byte depthbuffer and a binary with only the content of the depthbuffer */
void dump_depth (GF_Terminal *term, char *rad_name, u32 dump_mode_flags, u32 frameNum, char *conv_buf, void *avi_out, FILE *sha_out)
{
	GF_Err e;
	u32 i, k;
	GF_VideoSurface fb;
	u32 dump_mode = dump_mode_flags & 0x0000FFFF;

	/*lock it*/
	e = gf_sc_get_screen_buffer(term->compositor, &fb, 1);
	if (e) fprintf(stderr, "Error grabbing depth buffer: %s\n", gf_error_to_string(e));
	else  fprintf(stderr, "OK\n");
	/*export frame*/
	switch (dump_mode) {
	case DUMP_AVI:
		/*reverse frame*/
		for (k=0; k<fb.height; k++) {
			char *dst, *src;
			u16 src_16;
			dst = conv_buf + k*fb.width*3;
			src = fb.video_buffer + (fb.height-k-1) * fb.pitch_y;

			for (i=0; i<fb.width; i++) {
				switch (fb.pixel_format) {
				case GF_PIXEL_RGB_32:
				case GF_PIXEL_ARGB:
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					src+=4;
					break;

				case GF_PIXEL_BGR_32:
				case GF_PIXEL_RGBA:
					dst[0] = src[3];
					dst[1] = src[2];
					dst[2] = src[1];
					src+=4;
					break;
				case GF_PIXEL_RGB_24:
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					break;
				case GF_PIXEL_BGR_24:
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					break;
				case GF_PIXEL_RGB_565:
					src_16 = * ( (u16 *)src );
					dst[2] = colmask(src_16 >> 8/*(11 - 3)*/, 3);
					dst[1] = colmask(src_16 >> 3/*(5 - 2)*/, 2);
					dst[0] = colmask(src_16 << 3, 3);
					src+=2;
					break;
				case GF_PIXEL_RGB_555:
					src_16 = * (u16 *)src;
					dst[2] = colmask(src_16 >> 7/*(10 - 3)*/, 3);
					dst[1] = colmask(src_16 >> 2/*(5 - 3)*/, 3);
					dst[0] = colmask(src_16 << 3, 3);
					src+=2;
					break;
				/*for depth .avi*/
				case GF_PIXEL_GREYSCALE:
					dst[0] = src[0];
					dst[1] = src[0];
					dst[2] = src[0];
					src+=1;
					break;
				}
				dst += 3;
			}
		}
		if (avi_out) {
#ifndef GPAC_DISABLE_AVILIB
			if (AVI_write_frame(avi_out, conv_buf, fb.height*fb.width*3, 1) <0)
				fprintf(stderr, "Error writing frame\n");
		} else
#endif
		if (sha_out) {
			write_hash(sha_out, conv_buf, fb.height*fb.width*3);
		}

		/*in -depth -avi mode, do not release it yet*/
		if (dump_mode_flags & DUMP_DEPTH_ONLY) return;
		break;
	case DUMP_BMP:
		write_bmp(&fb, rad_name, frameNum);
		break;
	case DUMP_PNG:
		write_png(&fb, rad_name, frameNum);
		break;
	case DUMP_RAW:
		write_raw(&fb, rad_name, frameNum);
		break;
	default:
		write_depthfile(&fb, rad_name, frameNum);
		break;
	}
	/*unlock it*/
	gf_sc_release_screen_buffer(term->compositor, &fb);
}

void dump_frame(GF_Terminal *term, char *rad_name, u32 dump_mode_flags, u32 frameNum, char *conv_buf, void *avi_out, FILE *sha_out)
{
	GF_Err e = GF_OK;
	u32 i, k, out_size;
	GF_VideoSurface fb;

	u32 dump_mode = dump_mode_flags & 0x0000FFFF;
	u32 depth_dump_mode = 0;

	if (dump_mode_flags & DUMP_RGB_DEPTH_SHAPE) depth_dump_mode = 3;
	else if (dump_mode_flags & DUMP_RGB_DEPTH) depth_dump_mode = 2;
	else if (dump_mode_flags & DUMP_DEPTH_ONLY) depth_dump_mode = 1;

	/*lock it*/
	e = gf_sc_get_screen_buffer(term->compositor, &fb, depth_dump_mode);
	if (e) fprintf(stderr, "Error grabbing frame buffer: %s\n", gf_error_to_string(e));

	/*export frame*/
	switch (dump_mode) {
	case DUMP_AVI:
	case DUMP_SHA1:
		/*reverse frame*/
		for (k=0; k<fb.height; k++) {
			char *dst, *src;
			u16 src_16;
			if (dump_mode_flags & (DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE)) dst = conv_buf + k*fb.width*4;
			else dst = conv_buf + k*fb.width*3;

			src = fb.video_buffer + (fb.height-k-1) * fb.pitch_y;

			switch (fb.pixel_format) {
			case GF_PIXEL_RGB_32:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					src+=4;
					dst += 3;
				}
				break;
			case GF_PIXEL_RGBA:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=4;
					dst += 3;
				}
				break;
			case GF_PIXEL_RGBDS:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					dst[3] = src[3];
					dst +=4;
					src+=4;
				}
				break;
			case GF_PIXEL_RGBD:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					dst[3] = src[3];
					dst += 4;
					src+=4;
				}
				break;
			case GF_PIXEL_BGR_32:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[3];
					dst[1] = src[2];
					dst[2] = src[1];
					src+=4;
					dst+=3;
				}
				break;
			case GF_PIXEL_ARGB:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[1];
					dst[1] = src[2];
					dst[2] = src[3];
					src+=4;
					dst+=3;
				}
				break;
			case GF_PIXEL_RGB_24:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					dst+=3;
				}
				break;
			case GF_PIXEL_BGR_24:
				for (i=0; i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					dst+=3;
				}
				break;
			case GF_PIXEL_RGB_565:
				for (i=0; i<fb.width; i++) {
					src_16 = * ( (u16 *)src );
					dst[2] = colmask(src_16 >> 8/*(11 - 3)*/, 3);
					dst[1] = colmask(src_16 >> 3/*(5 - 2)*/, 2);
					dst[0] = colmask(src_16 << 3, 3);
					src+=2;
					dst+=3;
				}
				break;
			case GF_PIXEL_RGB_555:
				for (i=0; i<fb.width; i++) {
					src_16 = * (u16 *)src;
					dst[2] = colmask(src_16 >> 7/*(10 - 3)*/, 3);
					dst[1] = colmask(src_16 >> 2/*(5 - 3)*/, 3);
					dst[0] = colmask(src_16 << 3, 3);
					src+=2;
					dst +=3;
				}
				break;
			}
		}
		if (dump_mode_flags & (DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE))  {
			out_size = fb.height*fb.width*4;
		} else {
			out_size = fb.height*fb.width*3;
		}
		if (avi_out) {
#ifndef GPAC_DISABLE_AVILIB
			if (AVI_write_frame(avi_out, conv_buf, out_size, 1) <0)
				fprintf(stderr, "Error writing frame\n");
		} else 
#endif
		if (sha_out) {
			write_hash(sha_out, conv_buf, out_size);
		}
		break;
	case DUMP_BMP:
		write_bmp(&fb, rad_name, frameNum);
		break;
	case DUMP_PNG:
		write_png(&fb, rad_name, frameNum);
		break;
	case DUMP_RAW:
		write_raw(&fb, rad_name, frameNum);
		break;

	default:
		write_texture_file(&fb, rad_name, frameNum, dump_mode_flags);
		break;
	}
	/*unlock it*/
	gf_sc_release_screen_buffer(term->compositor, &fb);
}

#ifndef GPAC_DISABLE_AVILIB

typedef struct 
{
	GF_AudioListener al;
	GF_Mutex *mx;
	avi_t *avi;
	u32 time_scale;
	u64 max_dur, nb_bytes, audio_time;
	u32 video_time, video_next_time, video_time_init, audio_time_init, flush_retry, nb_write;
} AVI_AudioListener;

void avi_audio_frame(void *udta, char *buffer, u32 buffer_size, u32 time, u32 delay)
{
	AVI_AudioListener *avil = (AVI_AudioListener *)udta;

	if (avil->audio_time >= avil->audio_time_init + avil->max_dur)
		return;

	gf_mx_p(avil->mx);
	
	avil->nb_bytes+=buffer_size;
	avil->flush_retry=0;

	if (avil->audio_time >= avil->audio_time_init) {
		avil->nb_write++;
		AVI_write_audio(avil->avi, buffer, buffer_size);
	}


	avil->audio_time = 1000*avil->nb_bytes/avil->time_scale;

	//we are behind video dump, force audio flush 
	if (avil->audio_time < avil->video_next_time)  {
		gf_sc_flush_next_audio(term->compositor);
	}
	gf_mx_v(avil->mx);
}

void avi_audio_reconfig(void *udta, u32 samplerate, u32 bits_per_sample, u32 nb_channel, u32 channel_cfg)
{
	AVI_AudioListener *avil = (AVI_AudioListener *)udta;

	gf_mx_p(avil->mx);
	AVI_set_audio(avil->avi, nb_channel, samplerate, bits_per_sample, WAVE_FORMAT_PCM, 0);
	avil->time_scale = nb_channel*bits_per_sample*samplerate/8;
	gf_mx_v(avil->mx);
}
#endif

Bool dump_file(char *url, char *out_url, u32 dump_mode_flags, Double fps, u32 width, u32 height, Float scale, u32 *times, u32 nb_times)
{
	GF_Err e;
	u32 i = 0;
	GF_VideoSurface fb;
	char szPath[GF_MAX_PATH];
	char szOutPath[GF_MAX_PATH];
	char *prev=NULL;
	u32 time, prev_time, nb_frames, init_time;
	u64 dump_dur;
	char *conv_buf = NULL;
#ifndef GPAC_DISABLE_AVILIB
	avi_t *avi_out = NULL;
	avi_t *depth_avi_out = NULL;
	GF_Mutex *avi_mx = NULL;
	AVI_AudioListener avi_al;
#endif
	FILE *sha_out = NULL;
	FILE *sha_depth_out = NULL;
	char szPath_depth[GF_MAX_PATH];
	char comp[5];
	u32 cur_time_idx;
	u32 mode = dump_mode_flags & 0x0000FFFF;

    if (!out_url) out_url = url;
	prev = strstr(url, "://");
	if (prev) {
		prev = strrchr(url, '/');
		if (prev) prev++;
	}

	if (!prev) prev = url;
	strcpy(szPath, prev);
	prev = strrchr(szPath, '.');
	if (prev) prev[0] = 0;

	if (out_url) {
		strcpy(szOutPath, out_url);
	} else {
		strcpy(szOutPath, szPath);
	}
	prev = strrchr(szOutPath, '.');
	if (prev) prev[0] = 0;


	fprintf(stderr, "Opening URL %s\n", url);
	/*connect in pause mode*/
	gf_term_connect_from_time(term, url, 0, 1);

	while (!term->compositor->scene
	        || term->compositor->msg_type
	        || (gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)
	      ) {
		if (last_error) return 1;
		gf_term_process_flush(term);
		gf_sleep(10);
	}

	if (width && height) {
		gf_term_set_size(term, width, height);
		gf_term_process_flush(term);
	}
#ifndef GPAC_USE_TINYGL
	e = gf_sc_get_screen_buffer(term->compositor, &fb, 0);
#else
	e = gf_sc_get_screen_buffer(term->compositor, &fb, 1);
#endif
	if (e != GF_OK) {
		fprintf(stderr, "Error grabbing screen buffer: %s\n", gf_error_to_string(e));
		return 0;
	}
	width = fb.width;
	height = fb.height;
	gf_sc_release_screen_buffer(term->compositor, &fb);

	if (scale != 1) {
		width = (u32)(width * scale);
		height = (u32)(height * scale);
		gf_term_set_size(term, width, height);
		gf_term_process_flush(term);
	}

	/*we work in RGB24, and we must make sure the pitch is %4*/
	if ((width*3)%4) {
		fprintf(stderr, "Adjusting width (%d) to have a stride multiple of 4\n", width);
		while ((width*3)%4) width--;

		gf_term_set_size(term, width, height);
		gf_term_process_flush(term);

		gf_sc_get_screen_buffer(term->compositor, &fb, 0);
		width = fb.width;
		height = fb.height;
		gf_sc_release_screen_buffer(term->compositor, &fb);
	}


	strcpy(szPath_depth, szOutPath);

	if (mode==DUMP_AVI) {
#ifdef GPAC_DISABLE_AVILIB
		fprintf(stderr, "AVILib is disabled in this build of GPAC\n");
		return 0;
#else
		strcat(szOutPath, ".avi");
		avi_out = AVI_open_output_file(szOutPath);
		if (!avi_out) {
			fprintf(stderr, "Error creating AVI file %s\n", szOutPath);
			return 1;
		}
#endif
	}
	
	if (mode==DUMP_SHA1) {
		strcat(szOutPath, ".sha1");
		sha_out = fopen(szOutPath, "wb");
		if (!sha_out) {
			fprintf(stderr, "Error creating SHA file %s\n", szOutPath);
			return 1;
		}
	}

	if (dump_mode_flags & DUMP_DEPTH_ONLY) {
		if (mode==DUMP_AVI) {
#ifndef GPAC_DISABLE_AVILIB
			strcat(szPath_depth, "_depth.avi");
			depth_avi_out = AVI_open_output_file(szPath_depth);
			if (!depth_avi_out) {
				fprintf(stderr, "Error creating depth AVI file %s\n", szPath_depth);
				return 1;
			}
#endif
		}
		if (mode==DUMP_SHA1) {
			strcat(szPath_depth, "_depth.sha1");
			sha_depth_out = fopen(szPath_depth, "wb");
			if (!sha_depth_out) {
				fprintf(stderr, "Error creating depgth SHA file %s\n", szPath_depth);
				return 1;
			}
		}
	}


	if (!fps) fps = GF_IMPORT_DEFAULT_FPS;
	time = prev_time = 0;
	nb_frames = 0;

	if (nb_times==2) {
		prev_time = times[0];
		dump_dur = times[1] - times[0];
	} else if ((mode==DUMP_AVI) || (mode==DUMP_SHA1)) {
		dump_dur = times[0] ? times[0] : Duration;
	} else {
		dump_dur = times[nb_times-1];
		dump_dur ++;
	}
	if (!dump_dur) {
		fprintf(stderr, "Warning: file has no duration, defaulting to 1 sec\n");
		dump_dur = 1000;
	}

	if (mode==DUMP_AVI) {
#ifndef GPAC_DISABLE_AVILIB
		comp[0] = comp[1] = comp[2] = comp[3] = comp[4] = 0;
		AVI_set_video(avi_out, width, height, fps, comp);

		avi_mx = gf_mx_new("AVIMutex");
		
		if (! (term->user->init_flags & GF_TERM_NO_AUDIO)) {
			memset(&avi_al, 0, sizeof(avi_al));
			avi_al.al.udta = &avi_al;
			avi_al.al.on_audio_frame = avi_audio_frame;
			avi_al.al.on_audio_reconfig = avi_audio_reconfig;
			avi_al.mx = avi_mx;
			avi_al.avi = avi_out;
			avi_al.max_dur=dump_dur;

			gf_sc_add_audio_listener(term->compositor, &avi_al.al);
		}

		if (dump_mode_flags & DUMP_DEPTH_ONLY) 
			AVI_set_video(depth_avi_out, width, height, fps, comp);
#endif
	}

	if ((mode==DUMP_AVI) || (mode==DUMP_SHA1)) {
		
		if (dump_mode_flags & (DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE) ) 
			conv_buf = gf_malloc(sizeof(char) * width * height * 4);
		else
			conv_buf = gf_malloc(sizeof(char) * width * height * 3);
	}

	cur_time_idx = 0;
	/*step to first frame*/
	if (prev_time) {
		gf_term_step_clocks(term, prev_time);
		init_time = avi_al.audio_time_init = prev_time;
		avi_al.video_next_time = avi_al.video_time = init_time;
		prev_time=0;
	}

	while (time < dump_dur) {
		while ((gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)) {
			gf_term_process_flush(term);
		}

		if ((mode==DUMP_AVI) || (mode==DUMP_SHA1)) {

			fprintf(stderr, "Dumping %02d/100 %% - time %.02f sec\r", (u32) ((100.0*prev_time)/dump_dur), prev_time/1000.0 );

			if (avi_mx) gf_mx_p(avi_mx);

			if (dump_mode_flags & DUMP_DEPTH_ONLY) {

				/*we'll dump both buffers at once*/
				gf_mx_p(term->compositor->mx);
				dump_depth(term, szPath_depth, dump_mode_flags, i+1, conv_buf, depth_avi_out, sha_depth_out);
				dump_frame(term, szOutPath, mode, i+1, conv_buf, avi_out, sha_out);
				gf_mx_v(term->compositor->mx);
			} else {
				dump_frame(term, szOutPath, dump_mode_flags, i+1, conv_buf, avi_out, sha_out);
			}

			if (avi_mx) gf_mx_v(avi_mx);

		} else {
			if ( times[cur_time_idx] <= time) {
				if (dump_mode_flags & (DUMP_DEPTH_ONLY | DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE) ) {
					dump_depth(term, szOutPath, dump_mode_flags, cur_time_idx+1, NULL, NULL, NULL);
				} else {
					dump_frame(term, out_url, dump_mode_flags, cur_time_idx+1, NULL, NULL, NULL);
				}

				cur_time_idx++;
				if (cur_time_idx>=nb_times)
					break;
			}
		}

		avi_al.video_time = init_time + (u32) (nb_frames*1000/fps);
		nb_frames++;
		time = (u32) (nb_frames*1000/fps);
		avi_al.video_next_time = init_time + time;
		gf_term_step_clocks(term, time - prev_time);
		prev_time = time;

		if (gf_prompt_has_input() && (gf_prompt_get_char()=='q')) {
			fprintf(stderr, "Aborting dump\n");
			dump_dur=0;
			break;
		}
	}

	//flush audio dump
	if (! (term->user->init_flags & GF_TERM_NO_AUDIO)) {
		avi_al.flush_retry=0;
		while ((avi_al.flush_retry <100) && (avi_al.audio_time < dump_dur)) {
			gf_sc_flush_next_audio(term->compositor);
			gf_sleep(1);
			avi_al.flush_retry++;
		}
	}

#ifndef GPAC_DISABLE_AVILIB
	if (! (term->user->init_flags & GF_TERM_NO_AUDIO)) {
		gf_sc_remove_audio_listener(term->compositor, &avi_al.al);
	}
	if (avi_out) AVI_close(avi_out);
	if (depth_avi_out) AVI_close(depth_avi_out);
	if (avi_mx) gf_mx_del(avi_mx);
#endif

	if (sha_out) fclose(sha_out);
	if (sha_depth_out) fclose(sha_depth_out);

	if (conv_buf) {
		gf_free(conv_buf);
		fprintf(stderr, "Dumping done: %d frames at %g FPS\n", nb_frames, fps);
	}

	return 0;
}

