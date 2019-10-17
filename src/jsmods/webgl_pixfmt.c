/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *			All rights reserved
 *
 *  This file is part of GPAC / WebGL JavaScript extension
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

#include <gpac/setup.h>
#ifndef GPAC_DISABLE_3D

#include "webgl.h"


static char *wgl_yuv_shader_vars = \
"uniform sampler2D _gf_%s_1;\n\
uniform sampler2D _gf_%s_2;\n\
uniform sampler2D _gf_%s_3;\n\
const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n\
const vec3 R_mul = vec3(1.164,  0.000,  1.596);\n\
const vec3 G_mul = vec3(1.164, -0.391, -0.813);\n\
const vec3 B_mul = vec3(1.164,  2.018,  0.000);\n\
";

static char *wgl_yuv_shader_fun = \
"vec2 texc;\n\
vec3 yuv, rgb;\n\
texc = _gpacTexCoord.st;\n\
yuv.x = texture2D(_gf_%s_1, texc).r;\n\
yuv.y = texture2D(_gf_%s_2, texc).r;\n\
yuv.z = texture2D(_gf_%s_3, texc).r;\n\
yuv += offset;\n\
rgb.r = dot(yuv, R_mul);\n\
rgb.g = dot(yuv, G_mul);\n\
rgb.b = dot(yuv, B_mul);\n\
return vec4(rgb, 1.0);\n\
";

static char *wgl_nv12_shader_vars = \
"uniform sampler2D _gf_%s_1;\n\
uniform sampler2D _gf_%s_2;\n\
const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n\
const vec3 R_mul = vec3(1.164,  0.000,  1.596);\n\
const vec3 G_mul = vec3(1.164, -0.391, -0.813);\n\
const vec3 B_mul = vec3(1.164,  2.018,  0.000);\n\
";

static char *wgl_nv12_shader_fun = \
"vec2 texc;\n\
vec3 yuv, rgb;\n\
texc = _gpacTexCoord.st;\n\
yuv.x = texture2D(_gf_%s_1, texc).r;\n\
yuv.yz = texture2D(_gf_%s_2, texc).ra;\n\
yuv += offset;\n\
rgb.r = dot(yuv, R_mul);\n\
rgb.g = dot(yuv, G_mul);\n\
rgb.b = dot(yuv, B_mul);\n\
return vec4(rgb, 1.0);\n\
";


static char *wgl_nv21_shader_vars = \
"uniform sampler2D _gf_%s_1;\n\
uniform sampler2D _gf_%s_2;\n\
const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n\
const vec3 R_mul = vec3(1.164,  0.000,  1.596);\n\
const vec3 G_mul = vec3(1.164, -0.391, -0.813);\n\
const vec3 B_mul = vec3(1.164,  2.018,  0.000);\n\
";

static char *wgl_nv21_shader_fun = \
"vec2 texc;\n\
vec3 yuv, rgb;\n\
texc = _gpacTexCoord.st;\n\
yuv.x = texture2D(_gf_%s_1, texc).r;\n\
yuv.yz = texture2D(_gf_%s_2, texc).ar;\n\
yuv += offset;\n\
rgb.r = dot(yuv, R_mul);\n\
rgb.g = dot(yuv, G_mul);\n\
rgb.b = dot(yuv, B_mul);\n\
return vec4(rgb, 1.0);\n\
}";


static char *wgl_uyvy_shader_vars = \
	"uniform sampler2D _gf_%s_1;\
	uniform float _gf_%s_width;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	";

static char *wgl_uyvy_shader_fun = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 uyvy;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
uyvy = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	uyvy.g = uyvy.a; \
}\
yuv.r = uyvy.g;\
yuv.g = uyvy.r;\
yuv.b = uyvy.b;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
}";


static char *wgl_vyuy_shader_fun = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 vyuy;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
vyuy = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	vyuy.g = vyuy.a; \
}\
yuv.r = vyuy.g;\
yuv.g = vyuy.b;\
yuv.b = vyuy.r;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
}";


static char *wgl_yuyv_shader_fun = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 yvyu;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
yvyu = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	yvyu.r = yvyu.b; \
}\
yuv.r = yvyu.r;\
yuv.g = yvyu.g;\
yuv.b = yvyu.a;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
}";

static char *wgl_yvyu_shader_fun = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 yuyv;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
yuyv = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	yuyv.r = yuyv.b; \
}\
yuv.r = yuyv.r;\
yuv.g = yuyv.a;\
yuv.b = yuyv.g;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
}";

static char *wgl_rgb_shader_vars = \
"uniform sampler2D _gf_%s_1;\n\
uniform int _gf_%s_rgbmode;\n\
";

static char *wgl_rgb_shader_alhpagrey_fun = \
"vec4 col, out;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
out.r = out.g = out.b = col.a;\n\
out.a = col.r;\n\
return out;\n\
";


static char *wgl_rgb_shader_444_fun = \
"vec4 col, out;\n\
float res, col_g, col_b;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
res = floor( 255.0 * col.a );\n\
col_g = floor(res / 16.0);\n\
col_b = floor(res - col_g*16.0);\n\
out.r = col.r * 17.0;\n\
out.g = col_g / 15.0;\n\
out.b = col_b / 15.0;\n\
out.a = 1.0;\n\
return out;";


static char *wgl_rgb_shader_555_fun = \
"vec4 col, out;\n\
float res, col_r, col_g1, col_g, col_b;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
res = floor(255.0 * col.r);\n\
col_r = floor(res / 4.0);\n\
col_g1 = floor(res - col_r*4.0);\n\
res = floor(255.0 * col.a);\n\
col_g = floor(res / 32);\n\
col_b = floor(res - col_g*32.0);\n\
col_g += col_g1 * 8;\n\
out.r = col_r / 31.0;\n\
out.g = col_g / 31.0;\n\
out.b = col_b / 31.0;\n\
out.a = 1.0:\n\
return out;";

static char *wgl_rgb_shader_565_fun = \
"vec4 col, out;\n\
float res, col_r, col_g1, col_g, col_b;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
res = floor(255.0 * col.r);\n\
col_r = floor(res / 8.0);\n\
col_g1 = floor(res - col_r*8.0);\n\
res = floor(255.0 * col.a);\n\
col_g = floor(res / 32);\n\
col_b = floor(res - col_g*32.0);\n\
col_g += col_g1 * 8;\n\
out.r = col_r / 31.0;\n\
out.g = col_g / 63.0;\n\
out.b = col_b / 31.0;\n\
out.a = 1.0;\n\
return out;\n";


static char *wgl_rgb_shader_argb_fun = \
"vec4 col, out;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
out.r = col.g;\n\
out.g = col.b;\n\
out.b = col.a;\n\
out.a = col.r;\n\
return out;\n\
";

static char *wgl_rgb_shader_abgr_fun = \
"vec4 col, out;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
out.r = col.a;\n\
out.g = col.b;\n\
out.b = col.g;\n\
out.a = col.r;\n\
return out;\n\
";


static char *wgl_rgb_shader_bgra_fun = \
"vec4 col, out;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
out.r = col.b;\n\
out.g = col.g;\n\
out.b = col.r;\n\
out.a = col.a;\n\
return out;\n";


static char *wgl_rgb_shader_fun = \
"return texture2D(_gf_%s_1, _gpacTexCoord.st);\n;";



void wgl_insert_fragment_shader(GF_WebGLNamedTexture *tx, char **f_source)
{
	char szCode[4000];
	const char *shader_vars = NULL;
	const char *shader_fun = NULL;

	switch (tx->pix_fmt) {
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV21_10:
		shader_vars = wgl_nv21_shader_vars;
		shader_fun = wgl_nv21_shader_fun;
		break;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
		shader_vars = wgl_nv12_shader_vars;
		shader_fun = wgl_nv12_shader_fun;
		break;
	case GF_PIXEL_UYVY:
		shader_vars = wgl_uyvy_shader_vars;
		shader_fun = wgl_uyvy_shader_fun;
		break;
	case GF_PIXEL_YUYV:
		shader_vars = wgl_uyvy_shader_vars; //same as uyvy
		shader_fun = wgl_yuyv_shader_fun;
		break;
	case GF_PIXEL_VYUY:
		shader_vars = wgl_uyvy_shader_vars; //same as uyvy
		shader_fun = wgl_vyuy_shader_fun;
		break;
	case GF_PIXEL_YVYU:
		shader_vars = wgl_uyvy_shader_vars; //same as uyvy
		shader_fun = wgl_yvyu_shader_fun;
		break;
	case GF_PIXEL_ALPHAGREY:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_alhpagrey_fun;
		break;
	case GF_PIXEL_RGB_444:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_444_fun;
		break;
	case GF_PIXEL_RGB_555:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_555_fun;
		break;
	case GF_PIXEL_RGB_565:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_565_fun;
		break;
	case GF_PIXEL_ARGB:
	case GF_PIXEL_XRGB:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_argb_fun;
		break;
	case GF_PIXEL_ABGR:
	case GF_PIXEL_XBGR:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_abgr_fun;
		break;
	case GF_PIXEL_BGR:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_BGRX:
		shader_vars = wgl_rgb_shader_vars;
		shader_fun = wgl_rgb_shader_bgra_fun;
		break;

	default:
		if (tx->is_yuv) {
			shader_vars = wgl_yuv_shader_vars;
			shader_fun = wgl_yuv_shader_fun;
		} else {
			shader_vars = wgl_rgb_shader_vars;
			shader_fun = wgl_rgb_shader_fun;
		}
		break;
	}
	if (!shader_vars || !shader_fun) return;
	/*format with max 3 tx_name (for yuv)*/
	sprintf(szCode, shader_vars, tx->tx_name, tx->tx_name, tx->tx_name);
	gf_dynstrcat(f_source, szCode, NULL);

	gf_dynstrcat(f_source, "\nvec4 ", NULL);
	gf_dynstrcat(f_source, tx->tx_name, NULL);
	gf_dynstrcat(f_source, "_sample(vec2 _gpacTexCoord) {", NULL);
	/*format with max 3 tx_name (for yuv)*/
	sprintf(szCode, shader_fun, tx->tx_name, tx->tx_name, tx->tx_name);

	gf_dynstrcat(f_source, szCode, NULL);
	gf_dynstrcat(f_source, "\n}\n", NULL);

	tx->shader_attached = GF_TRUE;
}



const GF_FilterPacket *jsf_get_packet(JSContext *c, JSValue obj);
JSValue wgl_named_texture_upload(JSContext *c, JSValueConst pck_obj, GF_WebGLNamedTexture *named_tx)
{
	GF_FilterPacket *pck = NULL;
	GF_FilterFrameInterface *frame_ifce = NULL;
	u32 i, stride_luma, stride_chroma;
	Bool use_stride = GF_FALSE;
	const u8 *data=NULL, *pY=NULL, *pU=NULL, *pV=NULL, *pA=NULL;

	/*try GF_FilterPacket*/
	if (jsf_is_packet(c, pck_obj)) {
		pck = (GF_FilterPacket *) jsf_get_packet(c, pck_obj);
		if (!pck) return js_throw_err(c, WGL_INVALID_VALUE);

		frame_ifce = gf_filter_pck_get_frame_interface(pck);
	}
	/*try evg Texture*/
	else if (js_evg_is_texture(c, pck_obj)) {
	} else {
		return js_throw_err(c, WGL_INVALID_VALUE);
	}


	if (!named_tx->pix_fmt) {
		if (pck) {
			jsf_get_filter_packet_planes(c, pck_obj, &named_tx->width, &named_tx->height, &named_tx->pix_fmt, &named_tx->stride, &named_tx->uv_stride, NULL, NULL, NULL, NULL);
		} else {
			js_evg_get_texture_info(c, pck_obj, &named_tx->width, &named_tx->height, &named_tx->pix_fmt, NULL, &named_tx->stride, NULL, NULL, &named_tx->uv_stride, NULL);
		}

		
		named_tx->internal_textures = GF_FALSE;
		named_tx->nb_textures = 1;
		named_tx->is_yuv = GF_FALSE;
		named_tx->bit_depth = 8;
		named_tx->gl_format = GL_LUMINANCE;
		switch (named_tx->pix_fmt) {
		case GF_PIXEL_YUV444_10:
			named_tx->bit_depth = 10;
		case GF_PIXEL_YUV444:
			named_tx->uv_w = named_tx->width;
			named_tx->uv_h = named_tx->height;
			named_tx->uv_stride = named_tx->stride;
			named_tx->is_yuv = GF_TRUE;
			named_tx->nb_textures = 3;
			break;
		case GF_PIXEL_YUV422_10:
			named_tx->bit_depth = 10;
		case GF_PIXEL_YUV422:
			named_tx->uv_w = named_tx->width/2;
			named_tx->uv_h = named_tx->height;
			named_tx->uv_stride = named_tx->stride/2;
			named_tx->is_yuv = GF_TRUE;
			named_tx->nb_textures = 3;
			break;
		case GF_PIXEL_YUV_10:
			named_tx->bit_depth = 10;
		case GF_PIXEL_YUV:
			named_tx->uv_w = named_tx->width/2;
			if (named_tx->width % 2) named_tx->uv_w++;
			named_tx->uv_h = named_tx->height/2;
			if (named_tx->height % 2) named_tx->uv_h++;
			if (!named_tx->uv_stride) {
				named_tx->uv_stride = named_tx->stride/2;
				if (named_tx->stride%2) named_tx->uv_stride ++;
			}
			named_tx->is_yuv = GF_TRUE;
			named_tx->nb_textures = 3;
			break;
		case GF_PIXEL_NV12_10:
		case GF_PIXEL_NV21_10:
			named_tx->bit_depth = 10;
		case GF_PIXEL_NV12:
		case GF_PIXEL_NV21:
			named_tx->uv_w = named_tx->width/2;
			named_tx->uv_h = named_tx->height/2;
			named_tx->uv_stride = named_tx->stride;
			named_tx->is_yuv = GF_TRUE;
			named_tx->nb_textures = 2;
			break;
		case GF_PIXEL_UYVY:
		case GF_PIXEL_YUYV:
		case GF_PIXEL_YVYU:
		case GF_PIXEL_VYUY:
			named_tx->uv_w = named_tx->width/2;
			named_tx->uv_h = named_tx->height;
			if (!named_tx->uv_stride) {
				named_tx->uv_stride = named_tx->stride/2;
				if (named_tx->stride%2) named_tx->uv_stride ++;
			}
			named_tx->is_yuv = GF_TRUE;
			named_tx->nb_textures = 1;
			break;
		case GF_PIXEL_GREYSCALE:
			named_tx->gl_format = GL_LUMINANCE;
			named_tx->bytes_per_pix = 1;
			break;
		case GF_PIXEL_ALPHAGREY:
		case GF_PIXEL_GREYALPHA:
			named_tx->gl_format = GL_LUMINANCE_ALPHA;
			named_tx->bytes_per_pix = 2;
			break;
		case GF_PIXEL_RGB:
			named_tx->gl_format = GL_RGB;
			named_tx->bytes_per_pix = 3;
			break;
		case GF_PIXEL_BGR:
			named_tx->gl_format = GL_RGB;
			named_tx->bytes_per_pix = 3;
			break;
		case GF_PIXEL_BGRA:
			named_tx->has_alpha = GF_TRUE;
		case GF_PIXEL_BGRX:
			named_tx->gl_format = GL_RGBA;
			named_tx->bytes_per_pix = 4;
			break;
		case GF_PIXEL_ARGB:
			named_tx->has_alpha = GF_TRUE;
		case GF_PIXEL_XRGB:
			named_tx->gl_format = GL_RGBA;
			named_tx->bytes_per_pix = 4;
			break;
		case GF_PIXEL_ABGR:
			named_tx->has_alpha = GF_TRUE;
		case GF_PIXEL_XBGR:
			named_tx->gl_format = GL_RGBA;
			named_tx->bytes_per_pix = 4;
			break;
		case GF_PIXEL_RGBA:
			named_tx->has_alpha = GF_TRUE;
		case GF_PIXEL_RGBX:
			named_tx->gl_format = GL_RGBA;
			named_tx->bytes_per_pix = 4;
			break;
		case GF_PIXEL_RGB_444:
			named_tx->gl_format = GL_LUMINANCE_ALPHA;
			named_tx->bytes_per_pix = 2;
			break;
		case GF_PIXEL_RGB_555:
			named_tx->gl_format = GL_LUMINANCE_ALPHA;
			named_tx->bytes_per_pix = 2;
			break;
		case GF_PIXEL_RGB_565:
			named_tx->gl_format = GL_LUMINANCE_ALPHA;
			named_tx->bytes_per_pix = 2;
			break;
		default:
			return js_throw_err_msg(c, WGL_INVALID_VALUE, "[WebGL] Pixel format %s unknown, cannot setup NamedTexture\n", gf_4cc_to_str(named_tx->pix_fmt));
		}

		/*create textures*/
		if (!frame_ifce || !frame_ifce->get_gl_texture) {
			named_tx->internal_textures = GF_TRUE;
			named_tx->first_tx_load = GF_TRUE;
			glGenTextures(named_tx->nb_textures, named_tx->textures);
			for (i=0; i<named_tx->nb_textures; i++) {
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, named_tx->textures[i] );

				if (named_tx->is_yuv && named_tx->bit_depth>8) {
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
					//we use x bits but GL will normalise using 16 bits, so we need to multiply the normalized result by 2^(16-x)
					u32 nb_bits = (16 - named_tx->bit_depth);
					u32 scale = 1;
					while (nb_bits) {
						scale*=2;
						nb_bits--;
					}
					glPixelTransferi(GL_RED_SCALE, scale);
					if ((named_tx->nb_textures==2) && (i==1))
						glPixelTransferi(GL_ALPHA_SCALE, scale);

					glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
#endif
					named_tx->memory_format = GL_UNSIGNED_SHORT;

				} else {
					named_tx->memory_format = GL_UNSIGNED_BYTE;
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
					glPixelTransferi(GL_RED_SCALE, 1);
					glPixelTransferi(GL_ALPHA_SCALE, 1);
					glPixelStorei(GL_UNPACK_LSB_FIRST, 0);
#endif
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				}

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

				glDisable(GL_TEXTURE_2D);
			}

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
			if (named_tx->is_yuv && (named_tx->use_pbo)) {
				named_tx->first_tx_load = GF_FALSE;
				glGenBuffers(1, &named_tx->PBOs[0]);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[0]);

				if (named_tx->nb_textures>1) {
					glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->bytes_per_pix * named_tx->width*named_tx->height, NULL, GL_DYNAMIC_DRAW_ARB);

					glGenBuffers(1, &named_tx->PBOs[1]);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[1]);
				} else {
					//packed YUV
					glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->bytes_per_pix * 4*named_tx->width/2*named_tx->height, NULL, GL_DYNAMIC_DRAW_ARB);
				}

				if (named_tx->nb_textures==3) {
					glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->bytes_per_pix * named_tx->uv_w*named_tx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);

					glGenBuffers(1, &named_tx->PBOs[2]);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[2]);
					glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->bytes_per_pix * named_tx->uv_w*named_tx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);
				}
				//nv12/21
				else {
					glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->bytes_per_pix * 2*named_tx->uv_w*named_tx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);
				}
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			}
#endif
		}
		if (named_tx->bit_depth>8)
			named_tx->bytes_per_pix = 2;
	}
	named_tx->frame_ifce = NULL;

	if (frame_ifce && frame_ifce->get_gl_texture) {
		named_tx->frame_ifce = frame_ifce;
		return JS_UNDEFINED;
	}

	stride_luma = named_tx->stride;
	stride_chroma = named_tx->uv_stride;
	if (!frame_ifce) {
		u32 size=0;
		if (pck) {
			data = gf_filter_pck_get_data(pck, &size);
			if (!data)
				return js_throw_err_msg(c, WGL_INVALID_VALUE, "[WebGL] Unable to fetch packet data, cannot setup NamedTexture\n");
		} else {
			js_evg_get_texture_info(c, pck_obj, NULL, NULL, NULL, (u8 **) &data, NULL, NULL, NULL, NULL, NULL);
			if (!data)
				return js_throw_err_msg(c, WGL_INVALID_VALUE, "[WebGL] Unable to fetch EVG texture data, cannot setup NamedTexture\n");
		}

		if (named_tx->is_yuv) {
			if (named_tx->nb_textures==2) {
				pU = data + named_tx->stride * named_tx->height * named_tx->bytes_per_pix;
			} else if (named_tx->nb_textures==3) {
				pU = data + named_tx->stride * named_tx->height * named_tx->bytes_per_pix;
				pV = pU + named_tx->uv_stride * named_tx->uv_h * named_tx->bytes_per_pix;
			}
		}
	} else {
		u32 st_o;
		if (named_tx->nb_textures)
			frame_ifce->get_plane(frame_ifce, 0, &data, &stride_luma);
		if (named_tx->nb_textures>1)
			frame_ifce->get_plane(frame_ifce, 1, &pU, &stride_chroma);
		//todo we need to cleanup alpha frame fetch, how do we differentiate between NV12+alpha (3 planes) and YUV420 ?
		if (named_tx->nb_textures>2)
			frame_ifce->get_plane(frame_ifce, 2, &pV, &st_o);
		if (named_tx->nb_textures>3)
			frame_ifce->get_plane(frame_ifce, 3, &pA, &st_o);
	}
	pY = data;

	if (named_tx->is_yuv && (stride_luma != named_tx->width)) {
		use_stride = GF_TRUE; //whether >8bits or real stride
	}
	GL_CHECK_ERR()

	//push data
	glEnable(GL_TEXTURE_2D);

	if (!named_tx->is_yuv) {
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[0] );
		if (use_stride) {
#if !defined(GPAC_GL_NO_STRIDE)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, named_tx->stride / named_tx->bytes_per_pix);
#endif

		}
		if (named_tx->first_tx_load) {
			glTexImage2D(GL_TEXTURE_2D, 0, named_tx->gl_format, named_tx->width, named_tx->height, 0, named_tx->gl_format, GL_UNSIGNED_BYTE, data);
			named_tx->first_tx_load = GF_FALSE;
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, named_tx->width, named_tx->height, named_tx->gl_format, named_tx->memory_format, data);
		}
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	}
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	else if (named_tx->use_pbo) {
		u32 i, linesize, count, p_stride;
		u8 *ptr;

		//packed YUV
		if (named_tx->nb_textures==1) {

			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[0]);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			linesize = named_tx->width/2 * named_tx->bytes_per_pix * 4;
			p_stride = stride_luma;
			count = named_tx->height;

			for (i=0; i<count; i++) {
				memcpy(ptr, pY, linesize);
				pY += p_stride;
				ptr += linesize;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			glBindTexture(GL_TEXTURE_2D, named_tx->textures[0] );
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, named_tx->width/2, named_tx->height, 0, GL_RGBA, named_tx->memory_format, NULL);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		} else {
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[0]);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			linesize = named_tx->width*named_tx->bytes_per_pix;
			p_stride = stride_luma;
			count = named_tx->height;

			for (i=0; i<count; i++) {
				memcpy(ptr, pY, linesize);
				pY += p_stride;
				ptr += linesize;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[1]);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			linesize = named_tx->uv_w * named_tx->bytes_per_pix;
			p_stride = stride_chroma;
			count = named_tx->uv_h;
			//NV12 and  NV21
			if (!pV) {
				linesize *= 2;
			}

			for (i=0; i<count; i++) {
				memcpy(ptr, pU, linesize);
				pU += p_stride;
				ptr += linesize;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			if (pV) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[2]);
				ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

				for (i=0; i<count; i++) {
					memcpy(ptr, pV, linesize);
					pV += p_stride;
					ptr += linesize;
				}

				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
			}


			glBindTexture(GL_TEXTURE_2D, named_tx->textures[0] );
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, named_tx->width, named_tx->height, 0, named_tx->gl_format, named_tx->memory_format, NULL);

			glBindTexture(GL_TEXTURE_2D, named_tx->textures[1] );
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[1]);
			glTexImage2D(GL_TEXTURE_2D, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, named_tx->uv_w, named_tx->uv_h, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, named_tx->memory_format, NULL);

			if (pV) {
				glBindTexture(GL_TEXTURE_2D, named_tx->textures[2] );
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, named_tx->PBOs[2]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, named_tx->uv_w, named_tx->uv_h, 0, named_tx->gl_format, named_tx->memory_format, NULL);
			}

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		}
	}
#endif
	else if ((named_tx->pix_fmt==GF_PIXEL_UYVY) || (named_tx->pix_fmt==GF_PIXEL_YUYV) || (named_tx->pix_fmt==GF_PIXEL_VYUY) || (named_tx->pix_fmt==GF_PIXEL_YVYU)) {
		u32 uv_stride = 0;
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[0] );

		use_stride = GF_FALSE;
		if (stride_luma > 2*named_tx->width) {
			//stride is given in bytes for packed formats, so divide by 2 to get the number of pixels
			//for YUYV, and we upload as a texture with half the wsize so rdevide again by two
			//since GL_UNPACK_ROW_LENGTH counts in component and we moved the set 2 bytes per comp on 10 bits
			//no need to further divide
			uv_stride = stride_luma/4;
			use_stride = GF_TRUE;
		}
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, uv_stride);
#endif
		if (named_tx->first_tx_load) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, named_tx->width/2, named_tx->height, 0, GL_RGBA, named_tx->memory_format, pY);
			named_tx->first_tx_load = GF_FALSE;
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, named_tx->width/2, named_tx->height, GL_RGBA, named_tx->memory_format, pY);

		}

#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

	}
	else if (named_tx->first_tx_load) {
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[0] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_luma/named_tx->bytes_per_pix);
#endif
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, named_tx->width, named_tx->height, 0, named_tx->gl_format, named_tx->memory_format, pY);

		glBindTexture(GL_TEXTURE_2D, named_tx->textures[1] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, pV ? stride_chroma/named_tx->bytes_per_pix : stride_chroma/named_tx->bytes_per_pix/2);
#endif
		glTexImage2D(GL_TEXTURE_2D, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, named_tx->uv_w, named_tx->uv_h, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, named_tx->memory_format, pU);

		if (pV) {
			glBindTexture(GL_TEXTURE_2D, named_tx->textures[2] );
#if !defined(GPAC_GL_NO_STRIDE)
			if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_chroma/named_tx->bytes_per_pix);
#endif
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, named_tx->uv_w, named_tx->uv_h, 0, named_tx->gl_format, named_tx->memory_format, pV);
		}

#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
		named_tx->first_tx_load = GF_FALSE;
	}
	else {
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[0] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_luma/named_tx->bytes_per_pix);
#endif
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, named_tx->width, named_tx->height, named_tx->gl_format, named_tx->memory_format, pY);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindTexture(GL_TEXTURE_2D, named_tx->textures[1] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, pV ? stride_chroma/named_tx->bytes_per_pix : stride_chroma/named_tx->bytes_per_pix/2);
#endif
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, named_tx->uv_w, named_tx->uv_h, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, named_tx->memory_format, pU);
		glBindTexture(GL_TEXTURE_2D, 0);

		if (pV) {
			glBindTexture(GL_TEXTURE_2D, named_tx->textures[2] );
#if !defined(GPAC_GL_NO_STRIDE)
			if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_chroma/named_tx->bytes_per_pix);
#endif
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, named_tx->uv_w, named_tx->uv_h, named_tx->gl_format, named_tx->memory_format, pV);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	}

	return JS_UNDEFINED;
}

JSValue wgl_bind_named_texture(JSContext *c, GF_WebGLContext *glc, GF_WebGLNamedTexture *named_tx)
{
	u32 texture_unit = glc->active_texture;
	if (!texture_unit)
		texture_unit = GL_TEXTURE0;
	GL_CHECK_ERR()

	/*this happens when using regular calls instead of pck_tx.upload(ipck):
		  gl.bindTexture(gl.TEXTURE_2D, pck_tx);
		  gl.texImage2D(gl.TEXTURE_2D, 0, 0, 0, 0, ipck);

	in this case, the shader is not yet created but we do not throw an error for the sake of compatibility with usual GL programming
	*/
	if (!named_tx->shader_attached || !glc->active_program)
		return JS_UNDEFINED;

	if (!named_tx->uniform_setup) {
		char szName[100];
		u32 i;
		u32 start_idx = texture_unit - GL_TEXTURE0;
		s32 loc;
		for (i=0; i<named_tx->nb_textures; i++) {
			sprintf(szName, "_gf_%s_%d", named_tx->tx_name, i+1);
			loc = glGetUniformLocation(glc->active_program, szName);
	GL_CHECK_ERR()
			if (loc == -1) {
				return js_throw_err_msg(c, WGL_INVALID_OPERATION, "Failed to locate texture %s in shader\n", szName);
			}
			glUniform1i(loc, start_idx + i);
	GL_CHECK_ERR()
		}
	GL_CHECK_ERR()
		switch (named_tx->pix_fmt) {
		case GF_PIXEL_UYVY:
		case GF_PIXEL_YUYV:
		case GF_PIXEL_VYUY:
		case GF_PIXEL_YVYU:
			sprintf(szName, "_gf_%s_width", named_tx->tx_name);
			loc = glGetUniformLocation(glc->active_program, szName);
			if (loc == -1) {
				return js_throw_err_msg(c, WGL_INVALID_OPERATION, "Failed to locate uniform %s in shader\n", szName);
			}
			glUniform1f(loc, (GLfloat) named_tx->width);
			break;
		default:
			break;
		}
		named_tx->uniform_setup = GF_FALSE;
	}
	GL_CHECK_ERR()

	if (named_tx->frame_ifce) {
		u32 i;
		GF_Matrix txmx;
		gf_mx_init(txmx);
		for (i=0; i<named_tx->nb_textures; i++) {
			u32 gl_format;
			if (named_tx->frame_ifce->get_gl_texture(named_tx->frame_ifce, 0, &gl_format, &named_tx->textures[i], &txmx) != GF_OK) {
				if (!i)
					return js_throw_err_msg(c, WGL_INVALID_OPERATION, "Failed to get frame interface OpenGL texture ID for plane %d\n", i);
				break;
			}
			glEnable(gl_format);
			glActiveTexture(texture_unit + i);
			glBindTexture(gl_format, named_tx->textures[i]);
			glTexParameteri(gl_format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(gl_format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(gl_format, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			/*todo pass matrix to shader !!*/
		}
		if (named_tx->nb_textures) {
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glClientActiveTexture(texture_unit);
		}
		return JS_UNDEFINED;
	}
	if (named_tx->nb_textures>2) {
		glActiveTexture(texture_unit + 2);
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[2]);
	}
	if (named_tx->nb_textures>1) {
		glActiveTexture(texture_unit + 1);
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[1]);
	}
	if (named_tx->nb_textures) {
		glActiveTexture(texture_unit);
		glBindTexture(GL_TEXTURE_2D, named_tx->textures[0]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glClientActiveTexture(texture_unit);
	}
	GL_CHECK_ERR()
	return JS_UNDEFINED;
}


#endif

