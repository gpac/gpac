/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2012
 *					All rights reserved
 *
 *  This file is part of GPAC - sample DASH library usage
 *
 */

#include "defbench.h"



#if defined(WIN32) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#  pragma comment(lib, "libLibOpenHevcWrapper")
#pragma comment(lib, "SDL2")
//#pragma comment(lib, "SDL2main")
#pragma comment(lib, "opengl32")
#endif

//0: memcpy - 1: memmove - 2: u32 * cast and for loop copy of u32* - 3: memset 0 - 4: not touching the mapped buffer: 5: full memcpy, rely on stride in pixelstorei
#define COPY_TYPE 0
//set to 1 to disable final gltexImage in PBO mode
#define NO_TEX	0


SDL_Window *window = NULL;
SDL_GLContext *glctx= NULL;
SDL_Renderer *render= NULL;
GLint txid[3];
u8 *pY = NULL;
u8 *pU = NULL;
u8 *pV = NULL;
u32 width = 0;
u32 height = 0;
u32 size=0;
u32 bpp=8;
u32 Bpp=1;
GLint memory_format=GL_UNSIGNED_BYTE;
GLint pixel_format=GL_LUMINANCE;
GLint texture_type=GL_TEXTURE_RECTANGLE_EXT;
u32 gl_nb_frames = 1;
u64 gl_upload_time = 0;
u64 gl_draw_time = 0;
Bool pbo_mode = GF_TRUE;
Bool first_tx_load = GF_FALSE;
Bool use_vsync=0;

GLint glsl_program;
GLint vertex_shader;
GLint fragment_shader;

GLint pbo_Y=0;
GLint pbo_U=0;
GLint pbo_V=0;

GLDECL_STATIC(glActiveTexture);
GLDECL_STATIC(glClientActiveTexture);
GLDECL_STATIC(glCreateProgram);
GLDECL_STATIC(glDeleteProgram);
GLDECL_STATIC(glLinkProgram);
GLDECL_STATIC(glUseProgram);
GLDECL_STATIC(glCreateShader);
GLDECL_STATIC(glDeleteShader);
GLDECL_STATIC(glShaderSource);
GLDECL_STATIC(glCompileShader);
GLDECL_STATIC(glAttachShader);
GLDECL_STATIC(glDetachShader);
GLDECL_STATIC(glGetShaderiv);
GLDECL_STATIC(glGetInfoLogARB);
GLDECL_STATIC(glGetUniformLocation);
GLDECL_STATIC(glUniform1f);
GLDECL_STATIC(glUniform1i);
GLDECL_STATIC(glGenBuffers);
GLDECL_STATIC(glDeleteBuffers);
GLDECL_STATIC(glBindBuffer);
GLDECL_STATIC(glBufferData);
GLDECL_STATIC(glBufferSubData);
GLDECL_STATIC(glMapBuffer);
GLDECL_STATIC(glUnmapBuffer);


static char *glsl_yuv_shader = "\
	#version 140\n\
	#extension GL_ARB_texture_rectangle : enable\n\
	uniform sampler2DRect y_plane;\
	uniform sampler2DRect u_plane;\
	uniform sampler2DRect v_plane;\
	uniform float width;\
	uniform float height;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	out vec4 FragColor;\
	void main(void)  \
	{\
		vec2 texc;\
		vec3 yuv, rgb;\
		texc = gl_TexCoord[0].st;\
		texc.y = 1.0 - texc.y;\
		texc.x *= width;\
		texc.y *= height;\
		yuv.x = texture2DRect(y_plane, texc).r; \
		texc.x /= 2.0;\
		texc.y /= 2.0;\
		yuv.y = texture2DRect(u_plane, texc).r; \
		yuv.z = texture2DRect(v_plane, texc).r; \
		yuv += offset; \
	    rgb.r = dot(yuv, R_mul); \
	    rgb.g = dot(yuv, G_mul); \
	    rgb.b = dot(yuv, B_mul); \
		FragColor = vec4(rgb, 1.0);\
	}";

static char *default_glsl_vertex = "\
	varying vec3 gfNormal;\
	varying vec3 gfView;\
	void main(void)\
	{\
		gfView = vec3(gl_ModelViewMatrix * gl_Vertex);\
		gfNormal = normalize(gl_NormalMatrix * gl_Normal);\
		gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\
		gl_TexCoord[0] = gl_MultiTexCoord0;\
	}";



Bool sdl_compile_shader(u32 shader_id, const char *name, const char *source)
{
	GLint blen = 0;
	GLsizei slen = 0;
	u32 len;
	if (!source || !shader_id) return 0;
	len = (u32) strlen(source);
	glShaderSource(shader_id, 1, &source, &len);
	glCompileShader(shader_id);

	glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH , &blen);
	if (blen > 1) {
		char* compiler_log = (char*) gf_malloc(blen);
#ifdef CONFIG_DARWIN_GL
		glGetInfoLogARB((GLhandleARB) shader_id, blen, &slen, compiler_log);
#else
		glGetInfoLogARB(shader_id, blen, &slen, compiler_log);
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[GLSL] Failed to compile shader %s: %s\n", name, compiler_log));
		gf_free (compiler_log);
		return 0;
	}
	return 1;
}

void sdl_init(u32 _width, u32 _height, u32 _bpp, u32 stride, Bool use_pbo)
{
	u32 i, flags;
	Float hw, hh;
	GLint loc;
	GF_Matrix mx;
	width = _width;
	height = _height;
	bpp = _bpp;


	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

	flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
	if (use_vsync) flags |= SDL_RENDERER_PRESENTVSYNC;
	window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
	glctx = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, glctx);

	render = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);


#if (COPY_TYPE==5)
	size = stride*height;
#else
	size = width*height;
#endif
	if (bpp>8) {
		size *= 2;
		Bpp = 2;
	}
	pY = gf_malloc(size*sizeof(u8));
	memset(pY, 0x80, size*sizeof(u8));
	pU = gf_malloc(size/4*sizeof(u8));
	memset(pU, 0, size/4*sizeof(u8));
	pV = gf_malloc(size/4*sizeof(u8));
	memset(pV, 0, size/4*sizeof(u8));

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, width, height);

	gf_mx_init(mx);
	hw = ((Float)width)/2;
	hh = ((Float)height)/2;
	gf_mx_ortho(&mx, -hw, hw, -hh, hh, 50, -50);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(mx.m);


	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClear(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_NORMALIZE);
	glDisable(GL_DEPTH_TEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);


	GET_GLFUN(glActiveTexture);
	GET_GLFUN(glClientActiveTexture);
	GET_GLFUN(glCreateProgram);
	GET_GLFUN(glDeleteProgram);
	GET_GLFUN(glLinkProgram);
	GET_GLFUN(glUseProgram);
	GET_GLFUN(glCreateShader);
	GET_GLFUN(glDeleteShader);
	GET_GLFUN(glShaderSource);
	GET_GLFUN(glCompileShader);
	GET_GLFUN(glAttachShader);
	GET_GLFUN(glDetachShader);
	GET_GLFUN(glGetShaderiv);
	GET_GLFUN(glGetInfoLogARB);
	GET_GLFUN(glGetUniformLocation);
	GET_GLFUN(glUniform1f);
	GET_GLFUN(glUniform1i);
	GET_GLFUN(glGenBuffers);
	GET_GLFUN(glDeleteBuffers);
	GET_GLFUN(glBindBuffer);
	GET_GLFUN(glBufferData);
	GET_GLFUN(glBufferSubData);
	GET_GLFUN(glMapBuffer);
	GET_GLFUN(glUnmapBuffer);

	glsl_program = glCreateProgram();
	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	sdl_compile_shader(vertex_shader, "vertex", default_glsl_vertex);

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	sdl_compile_shader(fragment_shader, "fragment", glsl_yuv_shader);

	glAttachShader(glsl_program, vertex_shader);
	glAttachShader(glsl_program, fragment_shader);
	glLinkProgram(glsl_program);

	glGenTextures(3, txid);
	for (i=0; i<3; i++) {

		glEnable(texture_type);
		glBindTexture(texture_type, txid[i] );
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		if (bpp>8) {
			glPixelTransferi(GL_RED_SCALE, 64);
			memory_format=GL_UNSIGNED_SHORT;
		}
		glTexParameteri(texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(texture_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(texture_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		if (bpp>8) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
		} else {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		}
		glDisable(texture_type);
	}

	//sets uniforms: y, u, v textures point to texture slots 0, 1 and 2
	glUseProgram(glsl_program);
	for (i=0; i<3; i++) {
		const char *txname = (i==0) ? "y_plane" : (i==1) ? "u_plane" : "v_plane";
		loc = glGetUniformLocation(glsl_program, txname);
		if (loc == -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to locate texture %s in YUV shader\n", txname));
			continue;
		}
		glUniform1i(loc, i);
	}
	loc = glGetUniformLocation(glsl_program, "width");
	if (loc>= 0) {
		Float w = (Float) width;
		glUniform1f(loc, w);
	}
	loc = glGetUniformLocation(glsl_program, "height");
	if (loc>= 0) {
		Float h = (Float) height;
		glUniform1f(loc, h);
	}

	glUseProgram(0);


	if (glMapBuffer==NULL) use_pbo = GF_FALSE;


	pbo_mode = use_pbo;
	first_tx_load = use_pbo ? GF_FALSE : GF_TRUE;
	if (use_pbo) {
		glGenBuffers(1, &pbo_Y);
		glGenBuffers(1, &pbo_U);
		glGenBuffers(1, &pbo_V);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_Y);
		glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size, NULL, GL_DYNAMIC_DRAW_ARB);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_U);
		glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size/4, NULL, GL_DYNAMIC_DRAW_ARB);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_V);
		glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size/4, NULL, GL_DYNAMIC_DRAW_ARB);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	}
}

void sdl_close()
{
	DEL_SHADER(vertex_shader);
	DEL_SHADER(fragment_shader);
	DEL_PROGRAM(glsl_program );

	if (pbo_mode && pbo_Y) {
		glDeleteBuffers(1, &pbo_Y);
		glDeleteBuffers(1, &pbo_U);
		glDeleteBuffers(1, &pbo_V);
	}

	if (pY) gf_free(pY);
	if (pU) gf_free(pU);
	if (pV) gf_free(pV);

	if (glctx) SDL_GL_DeleteContext(glctx);
	if (render) SDL_DestroyRenderer(render);
	if (window) SDL_DestroyWindow(window);
}

void sdl_draw_quad()
{
	Float w = ((Float)width)/2;
	Float h = ((Float)height)/2;

	glBegin(GL_QUADS);

	glVertex3f(w, h, 0);
	glTexCoord2f(1, 0);

	glVertex3f(w, -h, 0);
	glTexCoord2f(0, 0);

	glVertex3f(-w, -h, 0);
	glTexCoord2f(0, 1);

	glVertex3f(-w, h, 0);
	glTexCoord2f(1, 1);

	glEnd();
}


void sdl_draw_frame(u8 *pY, u8 *pU, u8 *pV, u32 w, u32 h, u32 bit_depth, u32 stride)
{
	u32 needs_stride = 0;
	u64 now, end;

	if (stride != w) {
		if (bit_depth==10) {
			if (stride != 2*w) {
				needs_stride = stride / 2;
			}
		} else {
			needs_stride = stride;
		}
	}

	glEnable(texture_type);

	now = gf_sys_clock_high_res();


	if (first_tx_load) {
		glBindTexture(texture_type, txid[0] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
		glTexImage2D(texture_type, 0, 1, w, h, 0, pixel_format, memory_format, pY);

		glBindTexture(texture_type, txid[1] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexImage2D(texture_type, 0, 1, w/2, h/2, 0, pixel_format, memory_format, pU);

		glBindTexture(texture_type, txid[2] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexImage2D(texture_type, 0, 1, w/2, h/2, 0, pixel_format, memory_format, pV);

		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		first_tx_load = GF_FALSE;
	} else if (pbo_mode) {
		u32 i, linesize, count, p_stride;
		u8 *ptr;
#if (COPY_TYPE==2)
		u32 *s, *d;
		u32 j, c2;
#endif

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_Y);
		ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
#if (COPY_TYPE==5)
		memcpy(ptr, pY, size);
#elif (COPY_TYPE==3)
		memset(ptr, 0x80, size);
#elif (COPY_TYPE==4)
#else
		linesize = width*Bpp;
		p_stride = stride;
		count = h;
#if (COPY_TYPE==2)
		c2 = linesize/4;
		s = (u32 *)pY;
		d = (u32 *)ptr;
#endif
		for (i=0; i<count; i++) {
#if (COPY_TYPE==0) || (COPY_TYPE==1)
#if (COPY_TYPE==0)
			memcpy(ptr, pY, linesize);
#else
			memmove(ptr, pY, linesize);
#endif
			pY+= p_stride;
			ptr += linesize;
#else
			for (j=0; j<linesize/4; j++) {
				*d++ = *s++;;
			}
			s+= (p_stride-linesize)/4;
#endif
		}
#endif
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_U);
		ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
#if (COPY_TYPE==5)
		memcpy(ptr, pU, size/4);
#elif (COPY_TYPE==3)
		memset(ptr, 0x80, size/4);
#elif (COPY_TYPE==4)
#else
		linesize = width*Bpp/2;
		p_stride = stride/2;
		count/=2;
#if (COPY_TYPE==2)
		c2 /= 2;
		s = (u32 *)pU;
		d = (u32 *)ptr;
#endif
		for (i=0; i<count; i++) {
#if (COPY_TYPE==0) || (COPY_TYPE==1)
#if (COPY_TYPE==0)
			memcpy(ptr, pU, linesize);
#else
			memmove(ptr, pU, linesize);
#endif
			pU+= p_stride;
			ptr += linesize;
#else
			for (j=0; j<linesize/4; j++) {
				*d++ = *s++;;
			}
			s+= (p_stride-linesize)/4;
#endif
		}
#endif
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_V);
		ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
#if (COPY_TYPE==5)
		memcpy(ptr, pV, size/4);
#elif (COPY_TYPE==3)
		memset(ptr, 0x80, size/4);
#elif (COPY_TYPE==4)
#else
#if (COPY_TYPE==2)
		s = (u32 *)pV;
		d = (u32 *)ptr;
#endif
		for (i=0; i<count; i++) {
#if (COPY_TYPE==0) || (COPY_TYPE==1)
#if (COPY_TYPE==0)
			memcpy(ptr, pV, linesize);
#else
			memmove(ptr, pV, linesize);
#endif
			pV+= p_stride;
			ptr += linesize;
#else
			for (j=0; j<linesize/4; j++) {
				*d++ = *s++;;
			}
			s+= (p_stride-linesize)/4;
#endif
		}
#endif
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

#if (COPY_TYPE!=5)
		needs_stride=0;
#endif


#if (NO_TEX==0)
		glBindTexture(texture_type, txid[0] );
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_Y);
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
		glTexImage2D(texture_type, 0, 1, w, h, 0, pixel_format, memory_format, NULL);
		//glTexSubImage2D crashes with PBO and 2-bytes luminance on my FirePro W5000 ...
//		glTexSubImage2D(texture_type, 0, 0, 0, w, h, pixel_format, memory_format, pY);

		glBindTexture(texture_type, txid[1] );
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_U);
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexImage2D(texture_type, 0, 1, w/2, h/2, 0, pixel_format, memory_format, NULL);
//		glTexSubImage2D(texture_type, 0, 0, 0, w/2, h/2, pixel_format, memory_format, pU);

		glBindTexture(texture_type, txid[2] );
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo_V);
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexImage2D(texture_type, 0, 1, w/2, h/2, 0, pixel_format, memory_format, NULL);
//		glTexSubImage2D(texture_type, 0, 0, 0, w/2, h/2, pixel_format, memory_format, pV);
#endif

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	} else {
		glBindTexture(texture_type, txid[0] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
		glTexSubImage2D(texture_type, 0, 0, 0, w, h, pixel_format, memory_format, pY);
		glBindTexture(texture_type, 0);

		glBindTexture(texture_type, txid[1] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexSubImage2D(texture_type, 0, 0, 0, w/2, h/2, pixel_format, memory_format, pU);
		glBindTexture(texture_type, 0);

		glBindTexture(texture_type, txid[2] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexSubImage2D(texture_type, 0, 0, 0, w/2, h/2, pixel_format, memory_format, pV);
		glBindTexture(texture_type, 0);

		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	end = gf_sys_clock_high_res() - now;

	if (!first_tx_load) {
		gl_nb_frames ++;
		gl_upload_time += end;
	}

	glUseProgram(glsl_program);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(texture_type, txid[2]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(texture_type, txid[1]);

	glActiveTexture(GL_TEXTURE0 );
	glBindTexture(texture_type, txid[0]);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glClientActiveTexture(GL_TEXTURE0);

	sdl_draw_quad();

	glDisable(texture_type);
	glUseProgram(0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture_type, 0);

	SDL_GL_SwapWindow(window);

	gl_draw_time += gf_sys_clock_high_res() - now;
	return;
}


void sdl_bench()
{
	Double rate;
	u32 i, count;
	u64 start = gf_sys_clock_high_res();

	count = 600;
	for (i=0; i<count; i++) {
		sdl_draw_frame(pY, pU, pV, width, height, bpp, width);
	}

	start = gf_sys_clock_high_res() - start;
	rate = 3*size/2;
	rate *= count*1000;
	rate /= start; //in ms
	rate /= 1000; //==*1000 (in s) / 1000 * 1000 in MB /s
	fprintf(stdout, "gltext pushed %d frames in %d ms - FPS %g - data rate %g MB/s\n", count, start/1000, 1000000.0*count/start, rate);
}

void PrintUsage()
{
	fprintf(stderr, "USAGE: [OPTS] file.mp4\n"
	        "\n"
	        "Options:\n"
	        "-bench-yuv: only bench YUV upload rate\n"
	        "-sys-mem: uses  copy from decoder mem to system mem before upload (removes stride)\n"
	        "-use-pbo: uses PixelBufferObject for texture transfer\n"
	        "-no-display: disables video output\n"
	        "-nb-threads=N: sets number of frame to N (default N=6)\n"
	        "-mode=[frame|wpp|frame+wpp] : sets threading type (default is frame)\n"
	       );
}



int main(int argc, char **argv)
{
	Bool sdl_bench_yuv = GF_FALSE;
	Bool no_display = GF_FALSE;
	u64 start, now;
	u32 check_prompt;
	Bool sdl_is_init=GF_FALSE, run;
	Bool paused = GF_FALSE;
	u64 pause_time = 0;
	GF_ISOFile *isom;
	u32 i, count, track = 0;
	u32 nb_frames_at_start = 0;
	GF_ESD *esd;
	u32 nb_threads = 6;
	u32 mode = 1;
	Bool use_raw_memory = GF_TRUE;
	OpenHevc_Handle ohevc;
	Bool use_pbo = GF_FALSE;
	Bool enable_mem_tracker = GF_FALSE;
	const char *src = NULL;

	if (argc<2) {
		PrintUsage();
		return 0;
	}

	for (i=0; i<(u32)argc; i++) {
		char *arg = argv[i];
		if (arg[0]!='-') {
			src = arg;
			continue;
		}
		if (!strcmp(arg, "-bench-yuv")) sdl_bench_yuv=1;
		else if (!strcmp(arg, "-bench-yuv10")) sdl_bench_yuv=2;
		else if (!strcmp(arg, "-sys-mem")) use_raw_memory = 0;
		else if (!strcmp(arg, "-vsync")) use_vsync = 1;
		else if (!strcmp(arg, "-use-pbo")) use_pbo = 1;
		else if (!strcmp(arg, "-no-display")) no_display = 1;
		else if (!strcmp(arg, "-mem-track")) enable_mem_tracker = GF_TRUE;
		else if (!strncmp(arg, "-nb-threads=", 12)) nb_threads = atoi(arg+12);
		else if (!strncmp(arg, "-mode=", 6)) {
			if (!strcmp(arg+6, "wpp")) mode = 2;
			else if (!strcmp(arg+6, "frame+wpp")) mode = 3;
			else mode = 1;
		}

		else if (!strcmp(arg, "-h")) {
			PrintUsage();
			return 0;
		}
	}



	/*****************/
	/*   gpac init   */
	/*****************/
#ifdef GPAC_MEMORY_TRACKING
	gf_sys_init(enable_mem_tracker);
#else
	gf_sys_init(GF_FALSE);
#endif
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);

	if (sdl_bench_yuv) {
		sdl_init(3840, 2160, (sdl_bench_yuv==2) ? 10 : 8, 3840, use_pbo);
		sdl_bench();
		sdl_close();
		gf_sys_close();
		return 0;
	}
	if (!src) {
		PrintUsage();
		gf_sys_close();
		return 0;
	}


	isom = gf_isom_open(src, GF_ISOM_OPEN_READ, NULL);
	if (!isom) {
		sdl_close();
		gf_sys_close();
		return 0;
	}

	for (i=0; i<gf_isom_get_track_count(isom); i++) {
		if (gf_isom_get_hevc_shvc_type(isom, i+1, 1)>=GF_ISOM_HEVCTYPE_HEVC_ONLY) {
			track = i+1;
			break;
		}
	}

	if (!track) {
		gf_isom_close(isom);
		sdl_close();
		gf_sys_close();
		return 0;
	}

	count = gf_isom_get_sample_count(isom, track);
	start = gf_sys_clock_high_res();

	esd = gf_isom_get_esd(isom, track, 1);
	ohevc = libOpenHevcInit(nb_threads, mode);
	if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		libOpenHevcCopyExtraData(ohevc, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength+8);
	}

	libOpenHevcSetActiveDecoders(ohevc, 1);
	libOpenHevcSetViewLayers(ohevc, 1);

	libOpenHevcStartDecoder(ohevc);
	gf_odf_desc_del((GF_Descriptor *)esd);
	gf_isom_set_sample_padding(isom, track, 8);

	run=1;
	check_prompt=0;
	for (i=0; i<count && run; i++) {
		u32 di;
		if (!paused) {
			GF_ISOSample *sample = gf_isom_get_sample(isom, track, i+1, &di);

			if ( libOpenHevcDecode(ohevc, sample->data, sample->dataLength, sample->DTS+sample->CTS_Offset) ) {
				if (no_display) {
					OpenHevc_Frame HVCFrame_ptr;
					libOpenHevcGetOutput(ohevc, 1, &HVCFrame_ptr);
				} else if (use_raw_memory) {
					OpenHevc_Frame HVCFrame_ptr;
					libOpenHevcGetOutput(ohevc, 1, &HVCFrame_ptr);


					if (!sdl_is_init && !no_display) {
						sdl_init(HVCFrame_ptr.frameInfo.nWidth, HVCFrame_ptr.frameInfo.nHeight, HVCFrame_ptr.frameInfo.nBitDepth, HVCFrame_ptr.frameInfo.nYPitch, use_pbo);
						sdl_is_init=1;
						start = gf_sys_clock_high_res();
						nb_frames_at_start = i+1;
					}

					sdl_draw_frame((u8 *) HVCFrame_ptr.pvY, (u8 *) HVCFrame_ptr.pvU, (u8 *) HVCFrame_ptr.pvV, HVCFrame_ptr.frameInfo.nWidth, HVCFrame_ptr.frameInfo.nHeight, HVCFrame_ptr.frameInfo.nBitDepth, HVCFrame_ptr.frameInfo.nYPitch);
				} else {
					OpenHevc_Frame_cpy HVCFrame;
					memset(&HVCFrame, 0, sizeof(OpenHevc_Frame_cpy) );

					libOpenHevcGetPictureInfoCpy(ohevc, &HVCFrame.frameInfo);

					if (!sdl_is_init && !no_display) {
						sdl_init(HVCFrame.frameInfo.nWidth, HVCFrame.frameInfo.nHeight, HVCFrame.frameInfo.nBitDepth, HVCFrame.frameInfo.nYPitch, use_pbo);
						sdl_is_init=1;
						start = gf_sys_clock_high_res();
						nb_frames_at_start = i+1;
					}

					HVCFrame.pvY = (void*) pY;
					HVCFrame.pvU = (void*) pU;
					HVCFrame.pvV = (void*) pV;

					libOpenHevcGetOutputCpy(ohevc, 1, &HVCFrame);

					sdl_draw_frame(pY, pU, pV, HVCFrame.frameInfo.nWidth, HVCFrame.frameInfo.nHeight, HVCFrame.frameInfo.nBitDepth, HVCFrame.frameInfo.nYPitch);
				}
			}

			gf_isom_sample_del(&sample);

			now = gf_sys_clock_high_res();
			fprintf(stderr, "%d %% %d frames in %d ms - FPS %03.2f - push time "LLD" ms - draw "LLD" ms\r", 100*(i+1-nb_frames_at_start)/count, i+1-nb_frames_at_start, (now-start)/1000, 1000000.0 * (i+1-nb_frames_at_start) / (now-start), gl_upload_time / gl_nb_frames/1000 , (gl_draw_time - gl_upload_time) / gl_nb_frames/1000 );
		} else {
			gf_sleep(10);
			i--;
		}
		check_prompt++;
		if (check_prompt==50) {
			if (gf_prompt_has_input()) {
				switch (gf_prompt_get_char()) {
				case 'q':
					run = 0;
					break;
				case 'm':
					use_raw_memory = !use_raw_memory;
					break;
				case 'p':
					if (paused) {
						paused=0;
						start += gf_sys_clock_high_res()-pause_time;
					} else {
						paused = 1;
						pause_time=gf_sys_clock_high_res();
					}
					break;
				case 'r':
					start = gf_sys_clock_high_res();
					nb_frames_at_start = i+1;
					gl_upload_time = gl_draw_time = 0;
					gl_nb_frames=1;
					break;

				}
			}
			check_prompt=0;
		}
	}
	now = gf_sys_clock_high_res();
	fprintf(stderr, "\nDecoded %d frames in %d ms - FPS %g\n", i+1, (now-start)/1000, 1000000.0 * (i+1) / (now-start) );

	libOpenHevcClose(ohevc);
	gf_isom_close(isom);

	if (!no_display)
		sdl_close();

	gf_sys_close();
	return 1;
}

