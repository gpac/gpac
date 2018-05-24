/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2018
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
#include <gpac/internal/compositor_dev.h>
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
extern Bool no_prog;


static GFINLINE u8 colmask(s32 a, s32 n)
{
	s32 mask = (1 << n) - 1;
	return (u8) (a & (0xff & ~mask)) | ((-((a >> n) & 1)) & mask);
}

static u32 put_pixel(FILE *fout, u32 type, u32 pf, char *ptr)
{
	u16 col;
	switch (pf) {
	case GF_PIXEL_RGBX:
	case GF_PIXEL_ARGB:
		fputc(ptr[0], fout);
		fputc(ptr[1], fout);
		fputc(ptr[2], fout);
		return 4;

	case GF_PIXEL_BGRX:
	case GF_PIXEL_RGBA:
		fputc(ptr[2], fout);
		fputc(ptr[1], fout);
		fputc(ptr[0], fout);
		return 4;

	case GF_PIXEL_RGB:
		fputc(ptr[2], fout);
		fputc(ptr[1], fout);
		fputc(ptr[0], fout);
		return 3;

	case GF_PIXEL_BGR:
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

	fout = gf_fopen(str, "wb");
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
	for (j=fb->height; j>0; j--) {
		ptr = fb->video_buffer + (j-1)*fb->pitch_y;
		for (i=0; i<fb->width; i++) {
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
	gf_fclose(fout);
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


	fout = gf_fopen(str, "wb");
	if (fout) {
		GF_Err e = gf_img_png_enc(fb->video_buffer, fb->width, fb->height, fb->pitch_y, fb->pixel_format, dst, &dst_size);
		if (!e) {
			gf_fwrite(dst, dst_size, 1, fout);
			gf_fclose(fout);
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

	fout = gf_fopen("dump_depth", "wb");
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
	gf_fclose(fout);
}

void write_texture_file(GF_VideoSurface *fb, char *rad_name, u32 img_num, u32 dump_mode_flags)
{

	FILE *fout;
	u32 i, j;
	unsigned char *buf;

	buf = (unsigned char *) fb->video_buffer;

	if (dump_mode_flags & DUMP_RGB_DEPTH_SHAPE) fout = gf_fopen("dump_rgbds", "wb");
	else if (dump_mode_flags & DUMP_RGB_DEPTH) fout = gf_fopen("dump_rgbd", "wb");
	else return;

	if (!fout) return;
	for (j=0; j<fb->height;  j++) {
		for (i=0; i<fb->width*4; i++) {
			fputc(buf[i+j*fb->pitch_y], fout);
		}
	}
	gf_fclose(fout);
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

	fout = gf_fopen(str, "wb");
	if (!fout) return;


	for (j=0; j<fb->height; j++) {
		ptr = fb->video_buffer + j*fb->pitch_y;
		for (i=0; i<fb->width; i++) {
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
	gf_fclose(fout);
}

void write_hash(FILE *sha_out, char *buf, u32 size)
{
	u8 hash[20];
	gf_sha1_csum((u8 *)buf, size, hash);
	fwrite(hash, 1, 20, sha_out);
}


/* creates a .bmp format greyscale image of the byte depthbuffer and a binary with only the content of the depthbuffer */
void dump_depth (GF_Terminal *term, char *rad_name, u32 dump_mode_flags, u32 frameNum, char *conv_buf, FILE *sha_out)
{
	GF_Err e;
	GF_Compositor *compositor = gf_term_get_compositor(term);

	GF_VideoSurface fb;
	u32 dump_mode = dump_mode_flags & 0x0000FFFF;

	/*lock it*/
	e = gf_sc_get_screen_buffer(compositor, &fb, 1);
	if (e) fprintf(stderr, "Error grabbing depth buffer: %s\n", gf_error_to_string(e));
	else  fprintf(stderr, "OK\n");
	/*export frame*/
	switch (dump_mode) {
	case DUMP_SHA1:
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
	gf_sc_release_screen_buffer(compositor, &fb);
}

void dump_frame(GF_Terminal *term, char *rad_name, u32 dump_mode_flags, u32 frameNum, char *conv_buf, FILE *sha_out)
{
	GF_Err e = GF_OK;
	u32 out_size;
	GF_VideoSurface fb;
	GF_Compositor *compositor = gf_term_get_compositor(term);

	u32 dump_mode = dump_mode_flags & 0x0000FFFF;
	u32 depth_dump_mode = 0;

	if (dump_mode_flags & DUMP_RGB_DEPTH_SHAPE) depth_dump_mode = 3;
	else if (dump_mode_flags & DUMP_RGB_DEPTH) depth_dump_mode = 2;
	else if (dump_mode_flags & DUMP_DEPTH_ONLY) depth_dump_mode = 1;

	/*lock it*/
	e = gf_sc_get_screen_buffer(compositor, &fb, depth_dump_mode);
	if (e) fprintf(stderr, "Error grabbing frame buffer: %s\n", gf_error_to_string(e));

	/*export frame*/
	switch (dump_mode) {
	case DUMP_SHA1:
		if (dump_mode_flags & (DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE))  {
			out_size = fb.height*fb.width*4;
		} else {
			out_size = fb.height*fb.width*3;
		}
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
	gf_sc_release_screen_buffer(compositor, &fb);
}

Bool dump_file(char *url, char *out_url, u32 dump_mode_flags, Double fps, u32 width, u32 height, Float scale, u32 *times, u32 nb_times)
{
	GF_Err e;
	Bool ret = 0;
	u32 i = 0;
	char szPath[GF_MAX_PATH];
	char szOutPath[GF_MAX_PATH];
	char *prev=NULL;
	u32 time, prev_time, nb_frames, init_time;
	u64 dump_dur;
	char *conv_buf = NULL;
	GF_Compositor *compositor = gf_term_get_compositor(term);

	FILE *sha_out = NULL;
	FILE *sha_depth_out = NULL;
	char szPath_depth[GF_MAX_PATH];
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
	prev = gf_file_ext_start(szPath);
	if (prev) *prev = 0;

	if (out_url) {
		strcpy(szOutPath, out_url);
	} else {
		strcpy(szOutPath, szPath);
	}
	prev = gf_file_ext_start(szOutPath);
	if (prev) *prev = 0;

	gf_term_set_simulation_frame_rate(term, (Double) fps);
	
	if (mode==DUMP_AVI) {
		sprintf(szPath, "avimx:dst=%s.avi", szOutPath);
		if (dump_mode_flags & DUMP_DEPTH_ONLY) {
			strcat(szPath, ":depth");
		}
		e = gf_term_connect_output_filter(term, szPath);
		if (e) return 1;
	}

	fprintf(stderr, "Opening URL %s\n", url);
	/*connect and pause */
	gf_term_connect_from_time(term, url, 0, 2);
	compositor->use_step_mode = GF_TRUE;

	gf_term_get_visual_output_size(term, &width, &height);

	strcpy(szPath_depth, szOutPath);

	if (mode==DUMP_SHA1) {
		strcat(szOutPath, ".sha1");
		sha_out = gf_fopen(szOutPath, "wb");
		if (!sha_out) {
			fprintf(stderr, "Error creating SHA file %s\n", szOutPath);
			return 1;
		}
	}

	if (dump_mode_flags & DUMP_DEPTH_ONLY) {
		if (mode==DUMP_SHA1) {
			strcat(szPath_depth, "_depth.sha1");
			sha_depth_out = gf_fopen(szPath_depth, "wb");
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
		fprintf(stderr, "Warning: no dump duration indicated, defaulting to 1 sec\n");
		dump_dur = 1000;
	}

	if ((mode==DUMP_AVI) || (mode==DUMP_SHA1)) {

		if (dump_mode_flags & (DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE) )
			conv_buf = gf_malloc(sizeof(char) * width * height * 4);
		else
			conv_buf = gf_malloc(sizeof(char) * width * height * 3);
	}

	cur_time_idx = 0;
	init_time = 0;
	/*step to first frame*/
	if (prev_time) {
		gf_term_step_clocks(term, prev_time);
		init_time = prev_time;
		prev_time=0;
	}

	ret = 0;
	while (time < dump_dur) {
		u32 frame_start_time = gf_sys_clock();
		while ((gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)) {
			e = gf_term_process_flush(term);
			if (e) {
				ret = 1;
				break;
			}
			//if we can't flush a frame in 30 seconds consider this is an error
			if (0 && gf_sys_clock() - frame_start_time > 30000) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("[MP4Client] Could not flush frame in 30 seconds for AVI dump, aborting dump\n"));
				ret = 1;
				break;
			}
		}
		if (ret)
			break;

		if (mode==DUMP_AVI) {

			if (!no_prog)
				fprintf(stderr, "Dumping %02d/100 %% - time %.02f sec\r", (u32) ((100.0*prev_time)/dump_dur), prev_time/1000.0 );

		} else if (mode==DUMP_SHA1) {
			if (!no_prog)
				fprintf(stderr, "Dumping %02d/100 %% - time %.02f sec\r", (u32) ((100.0*prev_time)/dump_dur), prev_time/1000.0 );

			if (dump_mode_flags & DUMP_DEPTH_ONLY) {

				/*we'll dump both buffers at once*/
				gf_mx_p(compositor->mx);
				dump_depth(term, szPath_depth, dump_mode_flags, i+1, conv_buf, sha_depth_out);
				dump_frame(term, szOutPath, mode, i+1, conv_buf, sha_out);
				gf_mx_v(compositor->mx);
			} else {
				dump_frame(term, szOutPath, dump_mode_flags, i+1, conv_buf, sha_out);
			}

		} else {
			if ( times[cur_time_idx] <= time) {
				if (dump_mode_flags & (DUMP_DEPTH_ONLY | DUMP_RGB_DEPTH | DUMP_RGB_DEPTH_SHAPE) ) {
					dump_depth(term, szOutPath, dump_mode_flags, cur_time_idx+1, NULL, NULL);
				} else {
					dump_frame(term, out_url, dump_mode_flags, cur_time_idx+1, NULL, NULL);
				}

				cur_time_idx++;
				if (cur_time_idx>=nb_times)
					break;
			}
		}

		nb_frames++;
		time = (u32) (nb_frames*1000/fps);
		gf_term_step_clocks(term, time - prev_time);
		prev_time = time;

		if (0 && gf_prompt_has_input() && (gf_prompt_get_char()=='q')) {
			fprintf(stderr, "Aborting dump\n");
			break;
		}
	}

	if (sha_out) gf_fclose(sha_out);
	if (sha_depth_out) gf_fclose(sha_depth_out);

	if (conv_buf) {
		gf_free(conv_buf);
		fprintf(stderr, "Dumping done: %d frames at %g FPS\n", nb_frames, fps);
	}

	return ret;
}

