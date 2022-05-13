/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line mp4 toolbox
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

#include <gpac/isomedia.h>
#include <gpac/compositor.h>
#include <gpac/scenegraph.h>
#include <gpac/bifs.h>
#include <gpac/options.h>

#ifdef WIN32
#include <windows.h>
#define GPAC_CFG_FILE "GPAC.cfg"
#else
#include <pwd.h>
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

#define GPAC_CFG_FILE ".gpacrc"
#endif

#include <gpac/internal/avilib.h>
#include <gpac/internal/compositor_dev.h>

void PrintVersion()
{
	printf ("MP42AVI - GPAC version %s\n", GPAC_FULL_VERSION);
}

void PrintUsage()
{
	printf ("MP42AVI [option] input\n"
	        "Dumps BIFS media frames as AVI, BMP or raw\n\n"
	        "Options\n"
	        "-fps Framerate: specifies extraction framerate - if not set computed from track length\n"
	        "-size WxH: forces output BIFS to the given resolution\n"
	        "-raw [frame]: uses raw format for output - only dumps one frame if specified\n"
	        "-bmp [frame]: uses BMP format for output - only dumps one frame if specified\n"
	        "-outpath path: specifies where to dump frames/movie\n"
	        "\n"
	        "Note: when dumping a frame, either the frame number can be specified or the frame time\n"
	        "in the format hh:mm:ss:xFz where hh, mm, ss are hours, minutes, seconds, x the number\n"
	        "of the frame in the seconds and z the frame rate used to express the time\n"
	        "\n"
	        "-cfg: specifies path to GPAC config file (GPAC.cfg)\n"
	        "-v: prints version\n"
	        "-h: prints this message\n"
	        "\nWritten by Jean Le Feuvre - (c) 2000-2005\n");
}


typedef struct
{
	GF_Compositor *sr;
	GF_SceneGraph *sg;
	GF_BifsDecoder *bifs;
	GF_ISOFile *file;

	u32 track;
	u64 duration, cts;
} BIFSVID;

void node_init(void *cbk, GF_Node *node)
{
	BIFSVID *b2v = cbk;
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Conditional:
	case TAG_MPEG4_QuantizationParameter:
		break;
	default:
		if (b2v->sr) gf_sc_on_node_init(b2v->sr, node);
		break;
	}
}

void node_modif(void *cbk, GF_Node *node)
{
	BIFSVID *b2v = cbk;
	if (b2v->sr) gf_sc_invalidate(b2v->sr, node);
}

Double get_scene_time(void *cbk)
{
	Double res;
	BIFSVID *b2v = cbk;
	res = (Double) (s64) b2v->cts;
	res /= (Double) (s64) b2v->duration;
	return res;
}

void write_bmp(GF_VideoSurface *fb, char *rad_name, u32 img_num)
{
	char str[GF_MAX_PATH];
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;
	FILE *fout;
	u32 j, i;
	char *ptr;

	if (img_num<10) {
		sprintf(str, "%s_00%d.bmp", rad_name, img_num);
	} else if (img_num<100) {
		sprintf(str, "%s_0%d.bmp", rad_name, img_num);
	} else {
		sprintf(str, "%s_%d.bmp", rad_name, img_num);
	}

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
	fi.biBitCount = 24;
	fi.biCompression = BI_RGB;
	fi.biSizeImage = fb->pitch * fb->height;

	/*NOT ALIGNED!!*/
	gf_fwrite(&fh.bfType, 2, 1, fout);
	gf_fwrite(&fh.bfSize, 4, 1, fout);
	gf_fwrite(&fh.bfReserved1, 2, 1, fout);
	gf_fwrite(&fh.bfReserved2, 2, 1, fout);
	gf_fwrite(&fh.bfOffBits, 4, 1, fout);

	gf_fwrite(&fi, 1, 40, fout);

	for (j=fb->height; j>0; j--) {
		ptr = fb->video_buffer + (j-1)*fb->pitch;
		//gf_fwrite(ptr, 1, fb->width  * 3, fout);
		for (i=0; i<fb->width; i++) {
			fputc(ptr[2], fout);
			fputc(ptr[1], fout);
			fputc(ptr[0], fout);
			ptr+=3;
		}
	}

	gf_fclose(fout);
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

	fout = gf_fopen(str, "wb");
	if (!fout) return;
	gf_fwrite(fb->video_buffer , fb->height*fb->pitch, 1, fout);
	gf_fclose(fout);
}

void dump_frame(BIFSVID b2v, char *conv_buf, char *out_path, u32 dump_type, avi_t *avi_out, u32 frameNum)
{
	u32 k;
	GF_VideoSurface fb;

	/*lock it*/
	gf_sc_get_screen_buffer(b2v.sr, &fb);
	/*export frame*/
	switch (dump_type) {
	case 0:
		/*reverse frame*/
		for (k=0; k<fb.height; k++) {
			memcpy(conv_buf + k*fb.width*3, fb.video_buffer + (fb.height-k-1) * fb.pitch, sizeof(char) * fb.width  * 3);
		}
		if (AVI_write_frame(avi_out, conv_buf, fb.height*fb.width*3, 1) <0)
			printf("Error writing frame\n");
		break;
	case 2:
		write_raw(&fb, out_path, frameNum);
		break;
	case 1:
		write_bmp(&fb, out_path, frameNum);
		break;
	}
	/*unlock it*/
	gf_sc_release_screen_buffer(b2v.sr, &fb);
}

/*generates an intertwined bmp from a scene file with 5 different viewpoints*/
void bifs3d_viewpoints_merger(GF_ISOFile *file, char *szConfigFile, u32 width, u32 height, char *rad_name, u32 dump_type, char *out_dir, Double fps, s32 frameID, s32 dump_time)
{
	GF_User user;
	char out_path[GF_MAX_PATH];
	char old_driv[1024];
	BIFSVID b2v;
	Bool needs_raw;
	GF_Err e;
	GF_VideoSurface fb;
	unsigned char **rendered_frames;
	u32 nb_viewpoints = 5;
	u32 viewpoint_index;


	/* Configuration of the Rendering Capabilities */
	{
		const char *test;
		char config_path[GF_MAX_PATH];
		memset(&user, 0, sizeof(GF_User));
		user.config = gf_cfg_init(szConfigFile, NULL);

		if (!user.config) {
			fprintf(stdout, "Error: Configuration File \"%s\" not found in %s\n", GPAC_CFG_FILE, config_path);
			return;
		}

		test = gf_cfg_get_key(user.config, "Core", "ModulesDirectory");
		user.modules = gf_modules_new((const unsigned char *) test, user.config);
		strcpy(old_driv, "raw_out");
		if (!gf_modules_get_count(user.modules)) {
			printf("Error: no modules found\n");
			goto err_exit;
		}

		/*switch driver to raw_driver*/
		test = gf_cfg_get_key(user.config, "core", "video-output");
		if (test) strcpy(old_driv, test);

		needs_raw = 0;
		test = gf_cfg_get_key(user.config, "Compositor", "RendererName");
		/*since we only support RGB24 for MP42AVI force using RAW out with 2D driver*/
		if (test && strstr(test, "2D")) {
			needs_raw = 1;
		}
	}

	memset(&b2v, 0, sizeof(BIFSVID));
	user.init_flags = 0;
	/* Initialization of the compositor */
	b2v.sr = gf_sc_new(&user, 0, NULL);
	gf_sc_set_option(b2v.sr, GF_OPT_VISIBLE, 0);

	/* Initialization of the scene graph */
	b2v.sg = gf_sg_new();
	gf_sg_set_scene_time_callback(b2v.sg, get_scene_time, &b2v);
	gf_sg_set_init_callback(b2v.sg, node_init, &b2v);
	gf_sg_set_modified_callback(b2v.sg, node_modif, &b2v);

	/*load config*/
	gf_sc_set_option(b2v.sr, GF_OPT_RELOAD_CONFIG, 1);

	{
		u32 di;
		u32 track_number;
		GF_ESD *esd;
		u16 es_id;
		b2v.bifs = gf_bifs_decoder_new(b2v.sg, 0);

		for (track_number=0; track_number<gf_isom_get_track_count(file); track_number++) {
			esd = gf_isom_get_esd(file, track_number+1, 1);
			if (!esd) continue;
			if (!esd->dependsOnESID && (esd->decoderConfig->streamType == GF_STREAM_SCENE)) break;
			gf_odf_desc_del((GF_Descriptor *) esd);
			esd = NULL;
		}
		if (!esd) {
			printf("no bifs track found\n");
			goto err_exit;
		}

		es_id = (u16) gf_isom_get_track_id(file, track_number+1);
		e = gf_bifs_decoder_configure_stream(b2v.bifs, es_id, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->decoderConfig->objectTypeIndication);
		if (e) {
			printf("BIFS init error %s\n", gf_error_to_string(e));
			gf_odf_desc_del((GF_Descriptor *) esd);
			esd = NULL;
			goto err_exit;
		}

		{
			GF_ISOSample *samp = gf_isom_get_sample(file, track_number+1, 1, &di);
			b2v.cts = samp->DTS + samp->CTS_Offset;
			/*apply command*/
			gf_bifs_decode_au(b2v.bifs, es_id, samp->data, samp->dataLength, ((Double)(s64)b2v.cts)/1000.0);
			gf_isom_sample_del(&samp);
		}

		b2v.duration = gf_isom_get_media_duration(file, track_number+1);

		gf_odf_desc_del((GF_Descriptor *) esd);

	}
	gf_sc_set_scene(b2v.sr, b2v.sg);

	if (!width || !height) {
		gf_sg_get_scene_size_info(b2v.sg, &width, &height);
	}
	/*we work in RGB24, and we must make sure the pitch is %4*/
	if ((width*3)%4) {
		printf("Adjusting width (%d) to have a stride multiple of 4\n", width);
		while ((width*3)%4) width--;
	}
	gf_sc_set_size(b2v.sr, width, height);
	gf_sc_get_screen_buffer(b2v.sr, &fb);
	width = fb.width;
	height = fb.height;
	gf_sc_release_screen_buffer(b2v.sr, &fb);

	GF_SAFEALLOC(rendered_frames, nb_viewpoints*sizeof(char *));
	for (viewpoint_index = 1; viewpoint_index <= nb_viewpoints; viewpoint_index++) {
		GF_SAFEALLOC(rendered_frames[viewpoint_index-1], fb.width*fb.height*3);
		gf_sc_set_viewpoint(b2v.sr, viewpoint_index, NULL);
		gf_sc_draw_frame(b2v.sr, 0, NULL);
		/*needed for background2D !!*/
		gf_sc_draw_frame(b2v.sr, 0, NULL);
		strcpy(out_path, "");
		if (out_dir) {
			strcat(out_path, out_dir);
			if (out_path[strlen(out_path)-1] != '\\') strcat(out_path, "\\");
		}
		strcat(out_path, rad_name);
		strcat(out_path, "_view");
		gf_sc_get_screen_buffer(b2v.sr, &fb);
		write_bmp(&fb, out_path, viewpoint_index);
		memcpy(rendered_frames[viewpoint_index-1], fb.video_buffer, fb.width*fb.height*3);
		gf_sc_release_screen_buffer(b2v.sr, &fb);
	}

	if (width != 800 || height != 480) {
		printf("Wrong scene dimension, cannot produce output\n");
		goto err_exit;
	} else {
		u32 x, y;
		GF_VideoSurface out_fb;
		u32 bpp = 3;
		out_fb.width = 800;
		out_fb.height = 480;
		out_fb.pitch = 800*bpp;
		out_fb.pixel_format = GF_PIXEL_RGB_24;
		out_fb.is_hardware_memory = 0;
		GF_SAFEALLOC(out_fb.video_buffer, out_fb.pitch*out_fb.height)
#if 1
		for (y=0; y<out_fb.height; y++) {
			/*starting red pixel is R1, R5, R4, R3, R2, R1, R5, ... when increasing line num*/
			u32 line_shift = (5-y) % 5;
			for (x=0; x<out_fb.width; x++) {
				u32 view_shift = (line_shift+bpp*x)%5;
				u32 offset = out_fb.pitch*y + x*bpp;
				/* red */
				out_fb.video_buffer[offset] = rendered_frames[view_shift][offset];
				/* green */
				out_fb.video_buffer[offset+1] = rendered_frames[(view_shift+1)%5][offset+1];
				/* blue */
				out_fb.video_buffer[offset+2] = rendered_frames[(view_shift+2)%5][offset+2];
			}
		}
#else
		/*calibration*/
		for (y=0; y<out_fb.height; y++) {
			u32 line_shift = (5- y%5) % 5;
			for (x=0; x<out_fb.width; x++) {
				u32 view_shift = (line_shift+bpp*x)%5;
				u32 offset = out_fb.pitch*y + x*bpp;
				out_fb.video_buffer[offset] = ((view_shift)%5 == 2) ? 0xFF : 0;
				out_fb.video_buffer[offset+1] = ((view_shift+1)%5 == 2) ? 0xFF : 0;
				out_fb.video_buffer[offset+2] = ((view_shift+2)%5 == 2) ? 0xFF : 0;
			}
		}
#endif
		write_bmp(&out_fb, "output", 0);
	}

	/*destroy everything*/
	gf_bifs_decoder_del(b2v.bifs);
	gf_sg_del(b2v.sg);
	gf_sc_set_scene(b2v.sr, NULL);
	gf_sc_del(b2v.sr);



err_exit:
	/*	if (rendered_frames) {
			for (viewpoint_index = 1; viewpoint_index <= nb_viewpoints; viewpoint_index++) {
				if (rendered_frames[viewpoint_index-1]) gf_free(rendered_frames[viewpoint_index-1]);
			}
			gf_free(rendered_frames);
		}
		if (output_merged_frame) gf_free(output_merged_frame);
	*/
	if (user.modules) gf_modules_del(user.modules);
	if (needs_raw) gf_cfg_set_key(user.config, "core", "video-output", old_driv);
	gf_cfg_del(user.config);
}

void bifs_to_vid(GF_ISOFile *file, char *szConfigFile, u32 width, u32 height, char *rad_name, u32 dump_type, char *out_dir, Double fps, s32 frameID, s32 dump_time)
{
	GF_User user;
	BIFSVID b2v;
	u16 es_id;
	Bool first_dump, needs_raw;
	u32 i, j, di, count, timescale, frameNum;
	u32 duration, cur_time;
	GF_VideoSurface fb;
	GF_Err e;
	char old_driv[1024];
	const char *test;
	char config_path[GF_MAX_PATH];
	avi_t *avi_out;
	Bool reset_fps;
	GF_ESD *esd;
	char comp[5];
	char *conv_buf;

	memset(&user, 0, sizeof(GF_User));
	if (szConfigFile && strlen(szConfigFile)) {
		user.config = gf_cfg_init(config_path, NULL);
	} else {
		user.config = gf_cfg_init(NULL, NULL);
	}

	if (!user.config) {
		fprintf(stdout, "Error: Configuration File \"%s\" not found in %s\n", GPAC_CFG_FILE, config_path);
		return;
	}
	avi_out = NULL;
	conv_buf = NULL;
	esd = NULL;
	needs_raw = 0;
	test = gf_cfg_get_key(user.config, "Core", "ModulesDirectory");
	user.modules = gf_modules_new((const unsigned char *) test, user.config);
	strcpy(old_driv, "raw_out");
	if (!gf_modules_get_count(user.modules)) {
		printf("Error: no modules found\n");
		goto err_exit;
	}

	/*switch driver to raw_driver*/
	test = gf_cfg_get_key(user.config, "core", "video-output");
	if (test) strcpy(old_driv, test);

	test = gf_cfg_get_key(user.config, "Compositor", "RendererName");
	/*since we only support RGB24 for MP42AVI force using RAW out with 2D driver*/
	if (test && strstr(test, "2D")) {
		needs_raw = 1;
	}

	needs_raw = 0;
	user.init_flags = 0;
	b2v.sr = gf_sc_new(&user, 0, NULL);
	gf_sc_set_option(b2v.sr, GF_OPT_VISIBLE, 0);

	b2v.sg = gf_sg_new();
	gf_sg_set_scene_time_callback(b2v.sg, get_scene_time, &b2v);
	gf_sg_set_init_callback(b2v.sg, node_init, &b2v);
	gf_sg_set_modified_callback(b2v.sg, node_modif, &b2v);

	/*load config*/
	gf_sc_set_option(b2v.sr, GF_OPT_RELOAD_CONFIG, 1);

	b2v.bifs = gf_bifs_decoder_new(b2v.sg, 0);

	strcpy(config_path, "");
	if (out_dir) {
		strcat(config_path, out_dir);
		if (config_path[strlen(config_path)-1] != '\\') strcat(config_path, "\\");
	}
	strcat(config_path, rad_name);
	strcat(config_path, "_bifs");
	if (!dump_type) {
		strcat(config_path, ".avi");
		avi_out = AVI_open_output_file(config_path);
		comp[0] = comp[1] = comp[2] = comp[3] = comp[4] = 0;
		if (!avi_out) goto err_exit;
	}


	for (i=0; i<gf_isom_get_track_count(file); i++) {
		esd = gf_isom_get_esd(file, i+1, 1);
		if (!esd) continue;
		if (!esd->dependsOnESID && (esd->decoderConfig->streamType == GF_STREAM_SCENE)) break;
		gf_odf_desc_del((GF_Descriptor *) esd);
		esd = NULL;
	}
	if (!esd) {
		printf("no bifs track found\n");
		goto err_exit;
	}

	b2v.duration = gf_isom_get_media_duration(file, i+1);
	timescale = gf_isom_get_media_timescale(file, i+1);
	es_id = (u16) gf_isom_get_track_id(file, i+1);
	e = gf_bifs_decoder_configure_stream(b2v.bifs, es_id, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->decoderConfig->objectTypeIndication);
	if (e) {
		printf("BIFS init error %s\n", gf_error_to_string(e));
		gf_odf_desc_del((GF_Descriptor *) esd);
		esd = NULL;
		goto err_exit;
	}
	if (dump_time>=0) dump_time = dump_time *1000 / timescale;

	gf_sc_set_scene(b2v.sr, b2v.sg);
	count = gf_isom_get_sample_count(file, i+1);

	reset_fps = 0;
	if (!fps) {
		fps = (Float) (count * timescale);
		fps /= (Double) (s64) b2v.duration;
		printf("Estimated BIFS FrameRate %g\n", fps);
		reset_fps = 1;
	}

	if (!width || !height) {
		gf_sg_get_scene_size_info(b2v.sg, &width, &height);
	}
	/*we work in RGB24, and we must make sure the pitch is %4*/
	if ((width*3)%4) {
		printf("Adjusting width (%d) to have a stride multiple of 4\n", width);
		while ((width*3)%4) width--;
	}

	gf_sc_set_size(b2v.sr, width, height);
	gf_sc_draw_frame(b2v.sr, 0, NULL);

	gf_sc_get_screen_buffer(b2v.sr, &fb);
	width = fb.width;
	height = fb.height;
	if (avi_out) {
		AVI_set_video(avi_out, width, height, fps, comp);
		conv_buf = gf_malloc(sizeof(char) * width * height * 3);
	}
	printf("Dumping at BIFS resolution %d x %d\n\n", width, height);
	gf_sc_release_screen_buffer(b2v.sr, &fb);

	cur_time = 0;

	duration = (u32)(timescale / fps);
	if (reset_fps) fps = 0;

	frameNum = 1;
	first_dump = 1;
	for (j=0; j<count; j++) {
		GF_ISOSample *samp = gf_isom_get_sample(file, i+1, j+1, &di);

		b2v.cts = samp->DTS + samp->CTS_Offset;
		/*apply command*/
		gf_bifs_decode_au(b2v.bifs, es_id, samp->data, samp->dataLength, ((Double)(s64)b2v.cts)/1000.0);
		gf_isom_sample_del(&samp);

		if ((frameID>=0) && (j<(u32)frameID)) continue;
		if ((dump_time>=0) && ((u32) dump_time>b2v.cts)) continue;
		/*render frame*/
		gf_sc_draw_frame(b2v.sr, 0, NULL);
		/*needed for background2D !!*/
		if (first_dump) {
			gf_sc_draw_frame(b2v.sr, 0, NULL);
			first_dump = 0;
		}

		if (fps) {
			if (cur_time > b2v.cts) continue;

			while (1) {
				printf("dumped frame time %f (frame %d - sample %d)\r", ((Float)cur_time)/timescale, frameNum, j+1);
				dump_frame(b2v, conv_buf, config_path, dump_type, avi_out, frameNum);
				frameNum++;
				cur_time += duration;
				if (cur_time > b2v.cts) break;
			}
		} else {
			dump_frame(b2v, conv_buf, config_path, dump_type, avi_out, (frameID>=0) ? frameID : frameNum);
			if (frameID>=0 || dump_time>=0) break;
			frameNum++;
			printf("dumped frame %d / %d\r", j+1, count);
		}

	}
	gf_odf_desc_del((GF_Descriptor *) esd);

	/*destroy everything*/
	gf_bifs_decoder_del(b2v.bifs);
	gf_sg_del(b2v.sg);
	gf_sc_set_scene(b2v.sr, NULL);
	gf_sc_del(b2v.sr);

err_exit:
	if (avi_out) AVI_close(avi_out);
	if (conv_buf) gf_free(conv_buf);
	if (user.modules) gf_modules_del(user.modules);
	if (needs_raw) gf_cfg_set_key(user.config, "core", "video-output", old_driv);
	gf_cfg_del(user.config);
}

int main (int argc, char **argv)
{
	Double fps_dump;
	u32 i;
	char rad[500];
	s32 frameID, h, m, s, f;
	Float fps;
	u32 dump_type;
	s32 dump_time;
	u32 dump_w, dump_h;
	Bool copy;
	char szConfigFile[4096];
	char *dump_out;
	char *inName, *arg;
	GF_ISOFile *file;

	if (argc < 2) {
		PrintUsage();
		return 0;
	}

	dump_type = 0;
	fps_dump = 0.0f;
	dump_w = dump_h = 0;
	dump_out = NULL;
	inName = NULL;
	frameID = -1;
	dump_time = -1;
	szConfigFile[0] = 0;

	for (i = 1; i < (u32) argc ; i++) {
		arg = argv[i];
		if (arg[0] != '-') {
			inName = arg;
			break;
		}
		if (!stricmp(arg, "-h")) {
			PrintUsage();
			return 0;
		} else if (!stricmp(arg, "-version")) {
			PrintVersion();
			return 0;
		} else if (!stricmp(arg, "-size")) {
			sscanf(argv[i+1], "%dx%d", &dump_w, &dump_h);
			i++;
		} else if (!stricmp(arg, "-raw")) {
			dump_type = 2;
			if ((i+1<(u32)argc) && (argv[i+1][0]!='-')) {
				if (strstr(argv[i+1], "T")) {
					if (strstr(argv[i+1], "F")) {
						sscanf(argv[i+1], "T%d:%d:%d:%dF%f", &h, &m, &s, &f, &fps);
						dump_time = (s32) ((3600*h + 60*m + s)*1000 + 1000*f/fps);
					} else {
						sscanf(argv[i+1], "T%d:%d:%d", &h, &m, &s);
						dump_time = (s32) ((3600*h + 60*m + s)*1000);
					}
				} else {
					frameID = atoi(argv[i+1]);
				}
				i++;
			}
		} else if (!stricmp(arg, "-bmp")) {
			dump_type = 1;
			if ((i+1<(u32)argc) && (argv[i+1][0]!='-')) {
				if (strstr(argv[i+1], "T")) {
					if (strstr(argv[i+1], "F")) {
						sscanf(argv[i+1], "T%d:%d:%d:%dF%f", &h, &m, &s, &f, &fps);
						dump_time = (s32) ((3600*h + 60*m + s)*1000 + 1000*f/fps);
					} else {
						sscanf(argv[i+1], "T%d:%d:%d", &h, &m, &s);
						dump_time = (s32) ((3600*h + 60*m + s)*1000);
					}
				} else {
					frameID = atoi(argv[i+1]);
				}
				i++;
			}
		} else if (!stricmp(arg, "-3d")) {
			dump_type = 3;
		} else if (!stricmp(arg, "-outpath")) {
			dump_out = argv[i+1];
			i++;
		} else if (!stricmp(arg, "-fps")) {
			fps_dump = atof(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-copy")) {
			copy = 1;
		} else if (!stricmp(arg, "-cfg")) {
			strcpy(szConfigFile, argv[i+1]);
			i += 1;
		} else {
			PrintUsage();
			return (0);
		}
	}
	if (!inName) {
		PrintUsage();
		return 0;
	}
	gf_sys_init(GF_MemTrackerNone);

	file = gf_isom_open(inName, GF_ISOM_OPEN_READ, NULL);
	if (!file) {
		printf("Error opening file: %s\n", gf_error_to_string(gf_isom_last_error(NULL)));
		return 0;
	}

	if (dump_out) {
		arg = strrchr(inName, GF_PATH_SEPARATOR);
		if (arg) {
			strcpy(rad, arg + 1);
		} else {
			strcpy(rad, inName);
		}
	} else {
		strcpy(rad, inName);
	}
	while (rad[strlen(rad)-1] != '.') rad[strlen(rad)-1] = 0;
	rad[strlen(rad)-1] = 0;
	if (dump_type == 3) {
		bifs3d_viewpoints_merger(file, szConfigFile, dump_w, dump_h, rad, dump_type, dump_out, fps_dump, frameID, dump_time);
	}
	else bifs_to_vid(file, szConfigFile, dump_w, dump_h, rad, dump_type, dump_out, fps_dump, frameID, dump_time);
	printf("\ndone\n");
	gf_isom_delete(file);
	return 0;

}

