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

#ifndef GPAC_READ_ONLY


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
#include <gpac/internal/compositor_dev.h>

extern Bool is_connected;
extern GF_Terminal *term;
extern u32 Duration;
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
		fputc(ptr[3], fout);
		fputc(ptr[2], fout);
		fputc(ptr[1], fout);
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
	char *ptr, *prev;

	prev = strrchr(rad_name, '.');
	if (prev) prev[0] = '\0'; 

	if (fb->pixel_format==GF_PIXEL_GREYSCALE) sprintf(str, "%s_%d_depth.bmp", rad_name, img_num);
	else sprintf(str, "%s_%d.bmp", rad_name, img_num);

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
	if (fb->pixel_format==GF_PIXEL_GREYSCALE) fi.biBitCount = 24;
	else fi.biBitCount = 24;
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
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
	fclose(fout);
}

/*writes onto a file the content of the framebuffer in *fb interpreted as the byte depthbuffer */
/*it's also possible to write a float depthbuffer by passing the floats to strings and writing chars in putpixel - see comments*/
void write_depthfile(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	FILE *fout;
 	u32 i, j;
 	char val;
	unsigned char *depth;

	depth = (unsigned char *) fb->video_buffer;
	
	fout = fopen("dump_depth", "wb");
	if (!fout) return;
	for (j=0; j<fb->height;  j++) {
		for (i=0;i<fb->width; i++) {

#ifdef GPAC_USE_TINYGL
			val = fputc(depth[2*i+j*fb->width*sizeof(unsigned short)], fout);
			val = fputc(depth[2*i+j*fb->width*sizeof(unsigned short) + 1], fout);
#else
			val = fputc(depth[i+j*fb->width], fout);
#endif
		}
	}
	fclose(fout);
}

void write_texture_file(GF_VideoSurface *fb, char *rad_name, u32 img_num, u32 dump_mode)
{

	FILE *fout;
 	u32 i, j;
 	char val;
	unsigned char *buf;

	buf = (unsigned char *) fb->video_buffer;
	
	if (dump_mode==6) fout = fopen("dump_rgbds", "wb");
	else if (dump_mode==9) fout = fopen("dump_rgbd", "wb");
	else return;
	
	if (!fout) return;
	for (j=0; j<fb->height;  j++) {
		for (i=0;i<fb->width*4; i++) {
			val = fputc(buf[i+j*fb->pitch], fout);
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

	fout = fopen(str, "wb");
	if (!fout) return;

	
	for (j=0;j<fb->height; j++) {
		ptr = fb->video_buffer + j*fb->pitch;
		for (i=0;i<fb->width; i++) {
			u32 res = put_pixel(fout, 0, fb->pixel_format, ptr);
			assert(res);
			ptr += res;
		}
	}
	fclose(fout);
}


/* creates a .bmp format greyscale image of the byte depthbuffer and a binary with only the content of the depthbuffer */
void dump_depth (GF_Terminal *term, char *rad_name, u32 dump_type, u32 frameNum, char *conv_buf, avi_t *avi_out)
{
	GF_Err e;
	u32 i, k;
	GF_VideoSurface fb;

	/*lock it*/
	e = gf_sc_get_screen_buffer(term->compositor, &fb, 1);
	if (e) fprintf(stdout, "Error grabbing depth buffer: %s\n", gf_error_to_string(e));
	else  fprintf(stdout, "OK\n");
	/*export frame*/
	switch (dump_type) {
	case 1:
	case 8:
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
		if (AVI_write_frame(avi_out, conv_buf, fb.height*fb.width*3, 1) <0)
			printf("Error writing frame\n");
		break;
	case 2:
		write_bmp(&fb, rad_name, frameNum);
		break;
	case 3:
		write_raw(&fb, rad_name, frameNum);
		break;
	case 4:
		write_depthfile(&fb, rad_name, frameNum);
		break;
	case 7:
		write_bmp(&fb, rad_name, frameNum);
		break;	
		
	}
	/*unlock it*/
	/*in -depth -avi mode, do not release it yet*/
	if (dump_type!=8) gf_sc_release_screen_buffer(term->compositor, &fb);
}

void dump_frame(GF_Terminal *term, char *rad_name, u32 dump_type, u32 frameNum, char *conv_buf, avi_t *avi_out)
{
	GF_Err e = GF_OK;
	u32 i, k, out_size;
	GF_VideoSurface fb;

	/*lock it*/
	if (dump_type==5 || dump_type==6) e = gf_sc_get_screen_buffer(term->compositor, &fb, 2);
	else if (dump_type== 9 || dump_type==10) e = gf_sc_get_screen_buffer(term->compositor, &fb, 3);
	else e = gf_sc_get_screen_buffer(term->compositor, &fb, 0);
	if (e) fprintf(stdout, "Error grabbing frame buffer: %s\n", gf_error_to_string(e));

	if (dump_type!=5 && dump_type!= 10) { 
		out_size = fb.height*fb.width*3;
	} else {
		out_size = fb.height*fb.width*4;
	}
	/*export frame*/
	switch (dump_type) {
	case 1:
	case 5:
	case 10:
	case 8:
		/*reverse frame*/
		for (k=0; k<fb.height; k++) {
			char *dst, *src;
			u16 src_16;
			if (dump_type==5 || dump_type==10) dst = conv_buf + k*fb.width*4;
			else dst = conv_buf + k*fb.width*3;
			src = fb.video_buffer + (fb.height-k-1) * fb.pitch;

			switch (fb.pixel_format) {
			case GF_PIXEL_RGB_32:
			case GF_PIXEL_ARGB:
				for (i=0;i<fb.width; i++) {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					src+=4;
					dst += 3;
				}
				break;
			case GF_PIXEL_RGBDS:
				for (i=0;i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					dst[3] = src[3];
					dst +=4;
					src+=4;
				}
				break;		
			case GF_PIXEL_RGBD:
				for (i=0;i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					dst[3] = src[3];
					dst += 4;
					src+=4;
				}
				break;				
			case GF_PIXEL_BGR_32:
			case GF_PIXEL_RGBA:
				for (i=0;i<fb.width; i++) {
					dst[0] = src[3];
					dst[1] = src[2];
					dst[2] = src[1];
					src+=4;
					dst+=3;
				}
				break;
			case GF_PIXEL_RGB_24:
				for (i=0;i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					dst+=3;
				}
				break;
			case GF_PIXEL_BGR_24:
				for (i=0;i<fb.width; i++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					dst+=3;
				}
				break;
			case GF_PIXEL_RGB_565:
				for (i=0;i<fb.width; i++) {
					src_16 = * ( (u16 *)src );
					dst[2] = colmask(src_16 >> 8/*(11 - 3)*/, 3);
					dst[1] = colmask(src_16 >> 3/*(5 - 2)*/, 2);
					dst[0] = colmask(src_16 << 3, 3);
					src+=2;
					dst+=3;
				}
				break;
			case GF_PIXEL_RGB_555:
				for (i=0;i<fb.width; i++) {
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
		if (dump_type!=5 && dump_type!= 10) { 
			if (AVI_write_frame(avi_out, conv_buf, out_size, 1) <0)
			printf("Error writing frame\n");
		} else {
			if (AVI_write_frame(avi_out, conv_buf, out_size, 1) <0)
			printf("Error writing frame\n");				
		}
		break;
	case 2:
		write_bmp(&fb, rad_name, frameNum);
		break;
	case 6:
	case 9:
		write_texture_file(&fb, rad_name, frameNum, dump_type);
		break;
	
	case 3:
		write_raw(&fb, rad_name, frameNum);
		break;
	}
	/*unlock it*/
	gf_sc_release_screen_buffer(term->compositor, &fb);
}

Bool dump_file(char *url, u32 dump_mode, Double fps, u32 width, u32 height, Float scale, u32 *times, u32 nb_times)
{
	GF_Err e;
	u32 i = 0;
	GF_VideoSurface fb;
	char szPath[GF_MAX_PATH];
	char *prev=NULL;  

	prev = strstr(url, "://");
	if (prev) {
		prev = strrchr(url, '/');
		if (prev) prev++;
	}

	if (!prev) prev = url; 
	strcpy(szPath, prev);
	prev = strrchr(szPath, '.');
	if (prev) prev[0] = 0;

	fprintf(stdout, "Opening URL %s\n", url);
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

	e = gf_sc_get_screen_buffer(term->compositor, &fb, 0);
	if (e != GF_OK) {
		fprintf(stdout, "Error grabbing screen buffer: %s\n", gf_error_to_string(e));
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
		fprintf(stdout, "Adjusting width (%d) to have a stride multiple of 4\n", width);
		while ((width*3)%4) width--;

		gf_term_set_size(term, width, height);
		gf_term_process_flush(term);

		gf_sc_get_screen_buffer(term->compositor, &fb, 0);
		width = fb.width;
		height = fb.height;
		gf_sc_release_screen_buffer(term->compositor, &fb);
	}

	if (dump_mode==1 || dump_mode==5 || dump_mode==8 || dump_mode==10) {
		u32 time, prev_time, nb_frames, dump_dur;
		char *conv_buf;
		avi_t *avi_out = NULL; 
		avi_t *depth_avi_out = NULL; 
		char szPath_depth[GF_MAX_PATH];
		char comp[5];
		strcpy(szPath_depth, szPath);
		strcat(szPath, ".avi");
		avi_out = AVI_open_output_file(szPath);
		if (!avi_out) {
			fprintf(stdout, "Error creating AVI file %s\n", szPath);
			return 1;
		}
		if (dump_mode==8) {
			strcat(szPath_depth, "_depth.avi");
			depth_avi_out = AVI_open_output_file(szPath_depth);
			if (!depth_avi_out) {
				fprintf(stdout, "Error creating AVI file %s\n", szPath);
				return 1;
			}	
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
		if (dump_mode==8) AVI_set_video(depth_avi_out, width, height, fps, comp);
		if (dump_mode != 5 && dump_mode!=10) conv_buf = malloc(sizeof(char) * width * height * 3);
		else conv_buf = malloc(sizeof(char) * width * height * 4);
		/*step to first frame*/
		if (prev_time) gf_term_step_clocks(term, prev_time);

		while (time < dump_dur) {
			while ((gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)) {
				gf_term_process_flush(term);
			}
			fprintf(stdout, "Dumping %02d/100\r", (u32) ((100.0*prev_time)/dump_dur) );

			if (dump_mode==8) {
				/*we'll dump both buffers at once*/
				gf_mx_p(term->compositor->mx);
				dump_depth(term, szPath_depth, dump_mode, i+1, conv_buf, depth_avi_out);
				dump_frame(term, szPath, dump_mode, i+1, conv_buf, avi_out);
				gf_mx_v(term->compositor->mx);

			}
			else dump_frame(term, szPath, dump_mode, i+1, conv_buf, avi_out);
			
			nb_frames++;
			time = (u32) (nb_frames*1000/fps);
			gf_term_step_clocks(term, time - prev_time);
			prev_time = time;
		}
		AVI_close(avi_out);
		if (dump_mode==8) AVI_close(depth_avi_out);
		free(conv_buf);
		fprintf(stdout, "AVI Extraction 100/100\n");
	} else {
		if (times[0]) gf_term_step_clocks(term, times[0]);

		for (i=0; i<nb_times; i++) {
			while ((gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_STEP_PAUSE)) {
				gf_term_process_flush(term);
			}

			if (dump_mode==4 || dump_mode==7) {
				dump_depth(term, szPath, dump_mode, i+1, NULL, NULL);
			} else {
				dump_frame(term, url, dump_mode, i+1, NULL, NULL);
			}
			
			if (i+1<nb_times) gf_term_step_clocks(term, times[i+1] - times[i]);
		}
	}
	return 0;
}

#endif

