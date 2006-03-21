/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *			Copyright (c) 2005-200X ENST
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

typedef struct tagBITMAPINFOHEADER{
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
#include <gpac/internal/renderer_dev.h>

extern Bool is_connected;
extern GF_Terminal *term;
extern u32 Duration;
extern GF_Err last_error;


void write_bmp(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	char str[GF_MAX_PATH];
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;
	FILE *fout;
	u32 j, i;
	u16 src_16;
	char *ptr;

	sprintf(str, "%s_%d.bmp", rad_name, img_num);

	fout = fopen(str, "wb");
	if (!fout) return;

	memset(&fh, 0, sizeof(fh));
	fh.bfType = 19778;
	fh.bfOffBits = 14 + 40;

	memset(&fi, 0, sizeof(char)*40);
	fi.biSize = sizeof(char)*40;
	fi.biWidth = fb->width;
	fi.biHeight = fb->height;
	fi.biPlanes = 1;
	fi.biBitCount = 24;
	fi.biCompression = BI_RGB;
	fi.biSizeImage = fb->pitch * fb->height;

	/*NOT ALIGNED!!*/
    fwrite(&fh.bfType, 2, 1, fout);
    fwrite(&fh.bfSize, 4, 1, fout);
    fwrite(&fh.bfReserved1, 2, 1, fout);
    fwrite(&fh.bfReserved2, 2, 1, fout);
    fwrite(&fh.bfOffBits, 4, 1, fout);

	fwrite(&fi, 1, 40, fout);

	for (j=fb->height; j>0; j--) {
		ptr = fb->video_buffer + (j-1)*fb->pitch;
		for (i=0;i<fb->width; i++) {
			switch (fb->pixel_format) {
			case GF_PIXEL_RGB_32:
			case GF_PIXEL_ARGB:
				fputc(ptr[0], fout);
				fputc(ptr[1], fout);
				fputc(ptr[2], fout);
				ptr+=4;
				break;
			case GF_PIXEL_BGR_32:
			case GF_PIXEL_RGBA:
				fputc(ptr[3], fout);
				fputc(ptr[2], fout);
				fputc(ptr[1], fout);
				ptr+=4;
				break;
			case GF_PIXEL_RGB_24:
				fputc(ptr[2], fout);
				fputc(ptr[1], fout);
				fputc(ptr[0], fout);
				ptr+=3;
				break;
			case GF_PIXEL_BGR_24:
				fputc(ptr[2], fout);
				fputc(ptr[1], fout);
				fputc(ptr[0], fout);
				ptr+=3;
				break;
			case GF_PIXEL_RGB_565:
				src_16 = * (u16 *)ptr;
				fputc(src_16 & 0x1F, fout);
				fputc((src_16>>5) & 0x3F, fout);
				fputc((src_16>>11) & 0x1F, fout);
				ptr+=2;
				break;
			case GF_PIXEL_RGB_555:
				src_16 = * (u16 *)ptr;
				fputc(src_16 & 0x1F, fout);
				fputc((src_16>>5) & 0x1F, fout);
				fputc((src_16>>10) & 0x1F, fout);
				ptr+=2;
				break;
			}
		}
	}

	fclose(fout);
}


void write_raw(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	char str[GF_MAX_PATH];
	FILE *fout;
	if (img_num<10) {
		sprintf(str, "%s_00%d.raw", rad_name, img_num);
	} else if (img_num<100) {
		sprintf(str, "%s_0%d.raw", rad_name, img_num);
	} else {
		sprintf(str, "%s_%d.raw", rad_name, img_num);
	}

	fout = fopen(str, "wb");
	if (!fout) return;
	fwrite(fb->video_buffer , fb->height*fb->pitch, 1, fout);
	fclose(fout);
}

void dump_frame(GF_Terminal *term, char *rad_name, u32 dump_type, u32 frameNum, char *conv_buf, avi_t *avi_out)
{
	u32 i, k;
	GF_VideoSurface fb;

	/*lock it*/
	gf_sr_get_screen_buffer(term->renderer, &fb);
	/*export frame*/
	switch (dump_type) {
	case 1:
		/*reverse frame*/
		for (k=0; k<fb.height; k++) {
			char *dst, *src;
			u16 src_16;
			dst = conv_buf + k*fb.width*3;
			src = fb.video_buffer + (fb.height-k-1) * fb.pitch;

			for (i=0;i<fb.width; i++) {
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
					src_16 = * (u16 *)src;
					dst[0] = src_16 & 0x1F;
					dst[1] = (src_16>>5) & 0x3F;
					dst[2] = (src_16>>11) & 0x1F;
					src+=2;
					break;
				case GF_PIXEL_RGB_555:
					src_16 = * (u16 *)src;
					dst[0] = src_16 & 0x1F;
					dst[1] = (src_16>>5) & 0x1F;
					dst[2] = (src_16>>10) & 0x1F;
					src+=2;
					break;
				}
				dst += 3;
			}
		}
		if (AVI_write_frame(avi_out, conv_buf, fb.height*fb.width*3, 1) <0)
			printf("Error writing frame\n");
		break;
	case 2:
		write_bmp(&fb, rad_name, frameNum);
		break;
	case 3:
		write_raw(&fb, rad_name, frameNum);
		break;
	}
	/*unlock it*/
	gf_sr_release_screen_buffer(term->renderer, &fb);
}

Bool dump_file(char *url, u32 dump_mode, Double fps, u32 width, u32 height, u32 *times, u32 nb_times)
{
	u32 i = 0;
	GF_VideoSurface fb;
	char szPath[GF_MAX_PATH];
	char *prev = strrchr(url, '/');
	if (!prev) prev = strrchr(url, '\\');
	if (!prev) prev = url;
	strcpy(szPath, prev);
	prev = strrchr(szPath, '.');
	if (prev) prev[0] = 0;

	fprintf(stdout, "Opening URL %s\n", url);
	/*connect in pause mode*/
	gf_term_connect_from_time(term, url, 0, 1);

	while (!term->renderer->scene) {
		if (last_error) return 1;
		gf_term_process(term);
		gf_sleep(10);
	}
	
	if (width && height) {
		gf_term_set_size(term, width, height);
		gf_term_process(term);
	}

	gf_sr_get_screen_buffer(term->renderer, &fb);
	width = fb.width;
	height = fb.height;
	gf_sr_release_screen_buffer(term->renderer, &fb);

	/*we work in RGB24, and we must make sure the pitch is %4*/
	if ((width*3)%4) {
		fprintf(stdout, "Adjusting width (%d) to have a stride multiple of 4\n", width);
		while ((width*3)%4) width--;

		gf_term_set_size(term, width, height);
		gf_term_process(term);

		gf_sr_get_screen_buffer(term->renderer, &fb);
		width = fb.width;
		height = fb.height;
		gf_sr_release_screen_buffer(term->renderer, &fb);
	}

	if (dump_mode==1) {
		u32 time, prev_time, nb_frames, dump_dur;
		char *conv_buf;
		avi_t *avi_out = NULL; 
		char comp[5];
		strcat(szPath, ".avi");
		avi_out = AVI_open_output_file(szPath);
		if (!avi_out) {
			fprintf(stdout, "Error creating AVI file %s\n", szPath);
			return 1;
		}
		if (!fps) fps = 25.0;
		time = prev_time = 0;
		nb_frames = 0;

		if (nb_times==2) {
			prev_time = times[0];
			dump_dur = times[1] - times[0];
		} else {
			dump_dur = times[0] ? times[0] : Duration;
		}
		if (!dump_dur) {
			fprintf(stdout, "Warning: file has no duration, defaulting to 1 sec\n");
			dump_dur = 1000;
		}

		comp[0] = comp[1] = comp[2] = comp[3] = comp[4] = 0;
		AVI_set_video(avi_out, width, height, fps, comp);
		conv_buf = malloc(sizeof(char) * width * height * 3);

		/*step to first frame*/
		if (prev_time) gf_term_step_clocks(term, prev_time);

		while (time < dump_dur) {
			while ((gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)) {
				gf_term_process(term);
			}
			fprintf(stdout, "Dumping %02d/100\r", (u32) ((100.0*prev_time)/dump_dur) );

			dump_frame(term, szPath, dump_mode, i+1, conv_buf, avi_out);

			nb_frames++;
			time = (u32) (nb_frames*1000/fps);
			gf_term_step_clocks(term, time - prev_time);
			prev_time = time;
		}
		AVI_close(avi_out);
		free(conv_buf);
		fprintf(stdout, "AVI Extraction 100/100\n");
	} else {
		if (times[0]) gf_term_step_clocks(term, times[0]);

		for (i=0; i<nb_times; i++) {
			while ((gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)) {
				gf_term_process(term);
			}
			dump_frame(term, szPath, dump_mode, i+1, NULL, NULL);
			
			if (i+1<nb_times) gf_term_step_clocks(term, times[i+1] - times[i]);
		}
	}
	return 0;
}
