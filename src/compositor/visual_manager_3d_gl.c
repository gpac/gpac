/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#include "visual_manager.h"
#include "texturing.h"

#ifndef GPAC_DISABLE_3D

#include <gpac/nodes_mpeg4.h>
#include "gl_inc.h"

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
# if defined(GPAC_USE_TINYGL)
#  pragma comment(lib, "TinyGL")

# elif defined(GPAC_USE_GLES1X)

#  if 0
#   pragma message("Using OpenGL-ES Common Lite Profile")
#   pragma comment(lib, "libGLES_CL")
#	define GL_ES_CL_PROFILE
#  else
#   pragma message("Using OpenGL-ES Common Profile")
#   pragma comment(lib, "libGLES_CM")
#  endif

# elif defined(GPAC_USE_GLES2)
#  pragma comment(lib, "libGLESv2")

# else
#  pragma comment(lib, "opengl32")
# endif

#ifdef GPAC_HAS_GLU
#  pragma comment(lib, "glu32")
#endif

#endif

/*!! HORRIBLE HACK, but on my test devices, it seems that glClipPlanex is missing on the device but not in the SDK lib !!*/
#if defined(GL_MAX_CLIP_PLANES) && defined(__SYMBIAN32__)
#undef GL_MAX_CLIP_PLANES
#endif

#define CHECK_GL_EXT(name) ((strstr(ext, name) != NULL) ? 1 : 0)


void gf_sc_load_opengl_extensions(GF_Compositor *compositor, Bool has_gl_context)
{
#ifdef GPAC_USE_TINYGL
	/*let TGL handle texturing*/
	compositor->gl_caps.rect_texture = 1;
	compositor->gl_caps.npot_texture = 1;
#else
	const char *ext = NULL;

	if (has_gl_context)
		ext = (const char *) glGetString(GL_EXTENSIONS);

	if (!ext) ext = gf_opts_get_key("core", "glext");
	/*store OGL extension to config for app usage*/
	else if (gf_opts_get_key("core", "glext")==NULL)
		gf_opts_set_key("core", "glext", ext ? ext : "None");

	if (!ext) return;

	memset(&compositor->gl_caps, 0, sizeof(GLCaps));

	if (CHECK_GL_EXT("GL_ARB_multisample") || CHECK_GL_EXT("GLX_ARB_multisample") || CHECK_GL_EXT("WGL_ARB_multisample"))
		compositor->gl_caps.multisample = 1;
	if (CHECK_GL_EXT("GL_ARB_texture_non_power_of_two"))
		compositor->gl_caps.npot_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_abgr"))
		compositor->gl_caps.abgr_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_bgra"))
		compositor->gl_caps.bgra_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_framebuffer_object") || CHECK_GL_EXT("GL_ARB_framebuffer_object"))
		compositor->gl_caps.fbo = 1;
	if (CHECK_GL_EXT("GL_ARB_texture_non_power_of_two"))
		compositor->gl_caps.npot = 1;


	if (CHECK_GL_EXT("GL_ARB_point_parameters")) {
		compositor->gl_caps.point_sprite = 1;
		if (CHECK_GL_EXT("GL_ARB_point_sprite") || CHECK_GL_EXT("GL_NV_point_sprite")) {
			compositor->gl_caps.point_sprite = 2;
		}
	}

#ifdef GPAC_USE_GLES2
	compositor->gl_caps.vbo = 1;
#else
	if (CHECK_GL_EXT("GL_ARB_vertex_buffer_object")) {
		compositor->gl_caps.vbo = 1;
	}
#endif


#ifndef GPAC_USE_GLES1X
	if (CHECK_GL_EXT("GL_EXT_texture_rectangle") || CHECK_GL_EXT("GL_NV_texture_rectangle")) {
		compositor->gl_caps.rect_texture = 1;
	}
#endif

	if (CHECK_GL_EXT("EXT_unpack_subimage") ) {
		compositor->gl_caps.gles2_unpack = 1;
	}
	
	if (!has_gl_context) return;


	/*we have a GL context, init the rest (proc addresses & co)*/
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &compositor->gl_caps.max_texture_size);

	if (CHECK_GL_EXT("GL_ARB_pixel_buffer_object")) {
		compositor->gl_caps.pbo=1;
	}

#ifdef LOAD_GL_2_0
	if (glCreateProgram != NULL) {
		compositor->gl_caps.has_shaders = 1;

#ifndef GPAC_CONFIG_ANDROID
		if (glGetAttribLocation == NULL) {
			compositor->shader_mode_disabled = GF_TRUE;
		}
#endif

	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] OpenGL shaders not supported\n"));
	}
#endif //LOAD_GL_2_0

#endif //GPAC_USE_TINYGL

#ifdef GPAC_USE_GLES2
	compositor->gl_caps.has_shaders = GF_TRUE;
#elif defined (GL_VERSION_2_0)
	compositor->gl_caps.has_shaders = GF_TRUE;
#endif
	if (!compositor->gl_caps.has_shaders) {
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
		compositor->shader_mode_disabled = GF_TRUE;
#endif
		if (compositor->visual->autostereo_type > GF_3D_STEREO_LAST_SINGLE_BUFFER) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] OpenGL shaders not supported - disabling auto-stereo output\n"));
			compositor->visual->nb_views=1;
			compositor->visual->autostereo_type = GF_3D_STEREO_NONE;
			compositor->visual->camlay = GF_3D_CAMERA_STRAIGHT;
		}
	}

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	if (!compositor->shader_mode_disabled && compositor->vertshader && compositor->fragshader) {
		if (!gf_file_exists(compositor->vertshader)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] GLES Vertex shader %s not found, disabling shaders\n", compositor->vertshader));
			compositor->shader_mode_disabled = GF_TRUE;
		}

		if (!gf_file_exists(compositor->fragshader)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] GLES Fragment shader %s not found, disabling shaders\n", compositor->fragshader));
			compositor->shader_mode_disabled = GF_TRUE;
		}
	}
#endif
}


#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

#ifdef GPAC_USE_GLES2
#define GLES_VERSION_STRING "#version 100 \n"
#else
#define GLES_VERSION_STRING "#version 120 \n"
#endif

#define GLSL_PREFIX GLES_VERSION_STRING \
	"#ifdef GL_ES\n"\
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"\
	"precision highp float;\n"\
	"#else\n"\
	"precision mediump float;\n"\
	"#endif\n" \
	"#endif\n"

static char *glsl_autostereo_vertex = GLSL_PREFIX "\
	attribute vec4 gfVertex;\
	attribute vec2 gfTextureCoordinates;\
	uniform mat4 gfProjectionMatrix;\
	varying vec2 TexCoord;\
	void main(void)\
	{\
		TexCoord = gfTextureCoordinates;\
		gl_Position = gfProjectionMatrix * gfVertex;\
	}";

static char *glsl_view_anaglyph = GLSL_PREFIX "\
	uniform sampler2D gfView1;\
	uniform sampler2D gfView2;\
	varying vec2 TexCoord;\
	void main(void)  \
	{\
		vec4 col1 = texture2D(gfView1, TexCoord.st); \
		vec4 col2 = texture2D(gfView2, TexCoord.st); \
		gl_FragColor.r = col1.r;\
		gl_FragColor.g = col2.g;\
		gl_FragColor.b = col2.b;\
	}";

#ifdef GPAC_UNUSED_FUNC
static char *glsl_view_anaglyph_optimize = GLSL_PREFIX "\
	uniform sampler2D gfView1;\
	uniform sampler2D gfView2;\
	varying vec2 TexCoord;\
	void main(void)  \
	{\
		vec4 col1 = texture2D(gfView1, TexCoord.st); \
		vec4 col2 = texture2D(gfView2, TexCoord.st); \
		gl_FragColor.r = 0.7*col1.g + 0.3*col1.b;\
		gl_FragColor.r = pow(gl_FragColor.r, 1.5);\
		gl_FragColor.g = col2.g;\
		gl_FragColor.b = col2.b;\
	}";
#endif /*GPAC_UNUSED_FUNC*/

static char *glsl_view_columns = GLSL_PREFIX "\
	uniform sampler2D gfView1;\
	uniform sampler2D gfView2;\
	varying vec2 TexCoord;\
	void main(void)  \
	{\
		if ( int( mod(gl_FragCoord.x, 2.0) ) == 0) \
			gl_FragColor = texture2D(gfView1, TexCoord.st); \
		else \
			gl_FragColor = texture2D(gfView2, TexCoord.st); \
	}";

static char *glsl_view_rows = GLSL_PREFIX "\
	uniform sampler2D gfView1;\
	uniform sampler2D gfView2;\
	varying vec2 TexCoord;\
	void main(void)  \
	{\
		if ( int( mod(gl_FragCoord.y, 2.0) ) == 0) \
			gl_FragColor = texture2D(gfView1, TexCoord.st); \
		else \
			gl_FragColor = texture2D(gfView2, TexCoord.st); \
	}";

static char *glsl_view_5VSP19 = GLSL_PREFIX "\
	uniform sampler2D gfView1;\
	uniform sampler2D gfView2;\
	uniform sampler2D gfView3;\
	uniform sampler2D gfView4;\
	uniform sampler2D gfView5;\
	varying vec2 TexCoord;\
	\
	void getTextureSample(in int texID, out vec4 color) { \
	  if (texID == 0 ) color = texture2D(gfView1, TexCoord.st); \
	  else if (texID == 1 ) color = texture2D(gfView2, TexCoord.st); \
	  else if (texID == 2 ) color = texture2D(gfView3, TexCoord.st); \
	  else if (texID == 3 ) color = texture2D(gfView4, TexCoord.st); \
	  else if (texID == 4 ) color = texture2D(gfView5, TexCoord.st); \
	} \
	\
	void main(void) {\
		vec4 color;\
		float pitch = 5.0 + 1.0  - mod(gl_FragCoord.y , 5.0);\
		int col = int( mod(pitch + 3.0 * (gl_FragCoord.x), 5.0 ) );\
		int Vr = int(col);\
		int Vg = int(col) + 1;\
		int Vb = int(col) + 2;\
		if (Vg >= 5) Vg -= 5;\
		if (Vb >= 5) Vb -= 5;\
		getTextureSample(Vr, color); \
		gl_FragColor.r = color.r;\
		getTextureSample(Vg, color); \
		gl_FragColor.g = color.g;\
		getTextureSample(Vb, color); \
		gl_FragColor.b = color.b;\
	}";

static char *glsl_view_8VAlio = GLSL_PREFIX "\
	uniform sampler2D gfView1; \
	uniform sampler2D gfView2; \
	uniform sampler2D gfView3; \
	uniform sampler2D gfView4; \
	uniform sampler2D gfView5; \
	uniform sampler2D gfView6; \
	uniform sampler2D gfView7; \
	uniform sampler2D gfView8; \
	varying vec2 TexCoord;\
	 \
	void getTextureSample(in int texID, out vec4 color) { \
	  if (texID == 0 ) color = texture2D(gfView1, TexCoord.st); \
	  else if (texID == 1 ) color = texture2D(gfView2, TexCoord.st); \
	  else if (texID == 2 ) color = texture2D(gfView3, TexCoord.st); \
	  else if (texID == 3 ) color = texture2D(gfView4, TexCoord.st); \
	  else if (texID == 4 ) color = texture2D(gfView5, TexCoord.st); \
	  else if (texID == 5 ) color = texture2D(gfView6, TexCoord.st); \
	  else if (texID == 6 ) color = texture2D(gfView7, TexCoord.st); \
	  else if (texID == 7 ) color = texture2D(gfView8, TexCoord.st); \
	} \
	 \
	void main() \
	{ \
	  int x = int(gl_FragCoord.x + 0.5); \
	  int y = int(gl_FragCoord.y + 0.5); \
	  int modulox = x/8; \
	  int moduloy = y/8; \
	  modulox = x - 8 * modulox; \
	  moduloy = y - 8 * moduloy; \
	   \
	  int viewLine = 7 - moduloy; \
	  int viewPix = viewLine + 3 * modulox; \
	  int viewR = viewPix - 8*(viewPix/8); \
	  int viewG = viewPix + 1 - 8*((viewPix +1)/8); \
	  int viewB = viewPix + 2 - 8*((viewPix +2)/8); \
	   \
	  vec4 color; \
	  getTextureSample(viewR, color); \
	  gl_FragColor.r = color.r; \
	  getTextureSample(viewG, color);  \
	  gl_FragColor.g = color.g; \
	  getTextureSample(viewB, color); \
	  gl_FragColor.b = color.b; \
	}";


/**
 parses (glShaderSource) and compiles (glCompileShader) shader source
\return GF_TRUE if successful
 */
Bool visual_3d_compile_shader(GF_SHADERID shader_id, const char *name, const char *source)
{
	GLint blen = 0;
	GLsizei slen = 0;
	s32 len;
	GLint is_compiled=0;
	if (!source || !shader_id) return 0;
	len = (u32) strlen(source);
	glShaderSource(shader_id, 1, &source, &len);
	glCompileShader(shader_id);

	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &is_compiled);
	if (is_compiled == GL_TRUE) return GF_TRUE;
 	glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH , &blen);
	if (blen > 1) {
		char* compiler_log = (char*) gf_malloc(blen);
#ifdef CONFIG_DARWIN_GL
		glGetInfoLogARB((GLhandleARB) shader_id, blen, &slen, compiler_log);
#elif defined(GPAC_USE_GLES2)
		glGetShaderInfoLog(shader_id, blen, &slen, compiler_log);
#else
		glGetInfoLogARB(shader_id, blen, &slen, compiler_log);
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[GLSL] Failed to compile %s shader: %s\n", name, compiler_log));
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[GLSL] ***** faulty shader code ****\n%s\n**********************\n", source));
		gf_free (compiler_log);
		return 0;
	}
	return 1;
}
static GF_SHADERID visual_3d_shader_from_source_file(const char *src_path, u32 shader_type)
{
	GF_SHADERID shader = 0;
	u32 size;
	char *shader_src;

	GF_Err e = gf_file_load_data(src_path, (u8 **) &shader_src, &size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to open shader file %s: %s\n", src_path, gf_error_to_string(e) ));
	} else {
		shader = glCreateShader(shader_type);
		if (visual_3d_compile_shader(shader, (shader_type == GL_FRAGMENT_SHADER) ? "fragment" : "vertex", shader_src)==GF_FALSE) {
			glDeleteShader(shader);
			shader = 0;
		}
		gf_free(shader_src);
	}
	return shader;
}

#ifdef GPAC_CONFIG_IOS
#include <errno.h>
#include <sys/sysctl.h>
#endif


static GF_SHADERID visual_3d_shader_with_flags(const char *src_path, u32 shader_type, u32 flags, u32 pixfmt) {

	GF_SHADERID shader = 0;
	char *defs, szKey[100];
	size_t str_size;

	defs = (char *) gf_strdup(GLES_VERSION_STRING);
	str_size = strlen(defs) + 1; //+1 for trailing \0

	if (flags & GF_GL_HAS_LIGHT) {
		sprintf(szKey, "#define GF_GL_HAS_LIGHT\n#define LIGHTS_MAX %d\n", GF_MAX_GL_LIGHTS);
		str_size += strlen(szKey);
		defs = (char *) gf_realloc(defs, sizeof(char)*str_size);
		strcat(defs, szKey);
	}

	if(flags & GF_GL_HAS_COLOR) {
		str_size += strlen("#define GF_GL_HAS_COLOR \n");
		defs = (char *) gf_realloc(defs, sizeof(char)*str_size);
		strcat(defs,"#define GF_GL_HAS_COLOR \n");
	}

	if(flags & GF_GL_HAS_TEXTURE) {
		str_size += strlen("#define GF_GL_HAS_TEXTURE \n");
		defs = (char *) gf_realloc(defs, sizeof(char)*str_size);
		strcat(defs,"#define GF_GL_HAS_TEXTURE \n");
	}

	if(flags & GF_GL_HAS_CLIP) {
		/*clipping is always enabled*/
		sprintf(szKey, "#define CLIPS_MAX %d\n#define GF_GL_HAS_CLIP\n", GF_MAX_GL_CLIPS);
		str_size += strlen(szKey);
		defs = (char *) gf_realloc(defs, sizeof(char)*str_size);
		strcat(defs, szKey);
	}

	if (shader_type==GL_FRAGMENT_SHADER) {
		if(flags & GF_GL_IS_YUV) {
			str_size += strlen("#define GF_GL_IS_YUV \n");
			defs = (char *) gf_realloc(defs, sizeof(char)*str_size);
			strcat(defs,"#define GF_GL_IS_YUV \n");
		}
		if(flags & GF_GL_IS_ExternalOES) {
			str_size += strlen("#define GF_GL_IS_ExternalOES \n");
			defs = (char *) gf_realloc(defs, sizeof(char)*str_size);
			strcat(defs,"#define GF_GL_IS_ExternalOES \n");
		}
	}

	char *shader_src;
	u32 size;
	GF_Err e = gf_file_load_data(src_path ,(u8 **) &shader_src, &size);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to open shader file %s: %s\n", src_path, gf_error_to_string(e)));
	} else {
		char *final_src = NULL;

		gf_dynstrcat(&final_src, defs, NULL);

		if ((shader_type==GL_FRAGMENT_SHADER) && (flags & GF_GL_HAS_TEXTURE)) {
			char *sep = strstr(shader_src, "void main(");
			if (sep) sep[0] = 0;
			gf_dynstrcat(&final_src, shader_src, NULL);

			//add texture code
			gf_gl_txw_insert_fragment_shader(pixfmt, "maintx", &final_src, GF_FALSE);

			//append the rest
			if (sep) {
				sep[0] = 'v';
				gf_dynstrcat(&final_src, sep, "\n");
			}
		} else {
			gf_dynstrcat(&final_src, shader_src, "\n");
		}
		shader = glCreateShader(shader_type);

		if (visual_3d_compile_shader(shader, (shader_type == GL_FRAGMENT_SHADER) ? "fragment" : "vertex", final_src)==GF_FALSE) {
			glDeleteShader(shader);
			shader = 0;
		}

		gf_free(shader_src);
		gf_free(final_src);
		gf_free(defs);
	}
	return shader;
}

void visual_3d_init_stereo_shaders(GF_VisualManager *visual)
{
	GLint linked;
	Bool res;
	if (!visual->compositor->gl_caps.has_shaders) return;

	if (visual->autostereo_glsl_program) return;

	visual->autostereo_glsl_program = glCreateProgram();

	res = GF_TRUE;
	if (!visual->base_glsl_vertex) {
		visual->base_glsl_vertex = glCreateShader(GL_VERTEX_SHADER);
		res = visual_3d_compile_shader(visual->base_glsl_vertex, "vertex", glsl_autostereo_vertex);
	}
	if (res) {
		switch (visual->autostereo_type) {
		case GF_3D_STEREO_COLUMNS:
			visual->autostereo_glsl_fragment = glCreateShader(GL_FRAGMENT_SHADER);
			res = visual_3d_compile_shader(visual->autostereo_glsl_fragment, "fragment", glsl_view_columns);
			break;
		case GF_3D_STEREO_ROWS:
			visual->autostereo_glsl_fragment = glCreateShader(GL_FRAGMENT_SHADER);
			res = visual_3d_compile_shader(visual->autostereo_glsl_fragment, "fragment", glsl_view_rows);
			break;
		case GF_3D_STEREO_ANAGLYPH:
			visual->autostereo_glsl_fragment = glCreateShader(GL_FRAGMENT_SHADER);
			res = visual_3d_compile_shader(visual->autostereo_glsl_fragment, "fragment", glsl_view_anaglyph);
			break;
		case GF_3D_STEREO_5VSP19:
			visual->autostereo_glsl_fragment = glCreateShader(GL_FRAGMENT_SHADER);
			res = visual_3d_compile_shader(visual->autostereo_glsl_fragment, "fragment", glsl_view_5VSP19);
			break;
		case GF_3D_STEREO_8VALIO:
			visual->autostereo_glsl_fragment = glCreateShader(GL_FRAGMENT_SHADER);
			res = visual_3d_compile_shader(visual->autostereo_glsl_fragment, "fragment", glsl_view_8VAlio);
			break;

		case GF_3D_STEREO_CUSTOM:
			visual->autostereo_glsl_fragment = visual_3d_shader_from_source_file(visual->compositor->mvshader, GL_FRAGMENT_SHADER);
			if (visual->autostereo_glsl_fragment) res = GF_TRUE;
			break;
		}
	}

	if (res) {
		glAttachShader(visual->autostereo_glsl_program, visual->base_glsl_vertex);
		glAttachShader(visual->autostereo_glsl_program, visual->autostereo_glsl_fragment);
		glLinkProgram(visual->autostereo_glsl_program);

		glGetProgramiv(visual->autostereo_glsl_program, GL_LINK_STATUS, &linked);
		if (!linked) {
			int i32CharsWritten, i32InfoLogLength;
			char pszInfoLog[2048];
			glGetProgramiv(visual->autostereo_glsl_program, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
			glGetProgramInfoLog(visual->autostereo_glsl_program, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, (pszInfoLog));
			res = GF_FALSE;
		}
		GL_CHECK_ERR()
	}

	if (!res) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Autostereo vertex shader failed - disabling stereo support\n"));
		visual->autostereo_type = 0;
		visual->nb_views = 1;
	}
}

#define DEL_SHADER(_a) if (_a) { glDeleteShader(_a); _a = 0; }
#define DEL_PROGRAM(_a) if (_a) { glDeleteProgram(_a); _a = 0; }



#if 0
static GLint gf_glGetUniformLocation(GF_SHADERID glsl_program, const char *uniform_name)
{
	GLint loc = glGetUniformLocation(glsl_program, uniform_name);
	if (loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Cannot find uniform \"%s\" in GLSL program\n", uniform_name));
	}
	return loc;
}
#endif


#if 0
static GLint gf_glGetAttribLocation(GF_SHADERID glsl_program, const char *attrib_name)
{
	GLint loc = glGetAttribLocation(glsl_program, attrib_name);
	if (loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Cannot find attrib \"%s\" in GLSL program\n", attrib_name));
	}
	return loc;
}
#endif

//The following functions were used for shader testing [in ES2.0]
#if 0
static void gf_glQueryProgram(GF_SHADERID progObj)
{
	GLint err_log = -10;
	GL_CHECK_ERR()
	glValidateProgram(progObj);
	GL_CHECK_ERR()
	glGetProgramiv(progObj, GL_VALIDATE_STATUS, &err_log);
	printf("GL_VALIDATE_STATUS: %d \n ",err_log);
	glGetProgramiv(progObj, GL_LINK_STATUS, &err_log);
	printf("GL_LINK_STATUS: %d \n ",err_log);
	glGetProgramiv(progObj, GL_ATTACHED_SHADERS, &err_log);
	printf("GL_ATTACHED_SHADERS: %d \n ",err_log);
	glGetProgramiv(progObj, GL_ACTIVE_UNIFORMS, &err_log);
	printf("GL_ACTIVE_UNIFORMS: %d \n ",err_log);
}

static void gf_glQueryUniform(GF_SHADERID progObj, const char *name, int index)
{
	GLint loc, i;
	GLfloat res[16];

	loc = glGetUniformLocation(progObj, name);
	if(loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("failed to locate uniform. exiting\n"));
		return;
	}
	glGetUniformfv(progObj, loc, (GLfloat *) res);
	GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("uniform %s has value of: ", name));
	for (i =0; i<index; i++)
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("%f ", res[i]));
}

//same here
static void gf_glQueryAttrib(GF_SHADERID progObj, const char *name, int index, GLenum param)
{
	GLint loc, i;
	GLfloat res[16];

	loc = glGetAttribLocation(progObj, name);
	if (loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("failed to locate attribute. exiting\n"));
		return;
	}
	glGetVertexAttribfv(loc, param, (GLfloat *) res);
	GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("attribute %s has value of: ", name));
	for( i =0; i<index; i++)
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("%f ", res[i]));
}

static void gf_glQueryUniforms(GF_SHADERID progObj)
{
	GLint maxUniformLen;
	GLint numUniforms;
	char *uniformName;
	GLint index;

	glGetProgramiv(progObj, GL_ACTIVE_UNIFORMS, &numUniforms);
	glGetProgramiv(progObj, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformLen);
	uniformName = gf_malloc(sizeof(char) * maxUniformLen);
	for(index = 0; index < numUniforms; index++) {
		GLint size;
		GLenum type;
		GLint location;
		// Get the Uniform Info
		glGetActiveUniform(progObj, index, maxUniformLen, NULL, &size, &type, uniformName);
		// Get the uniform location
		location = glGetUniformLocation(progObj, uniformName);
		if(location) printf("uniform %s is: ",uniformName);
		switch(type) {
		case GL_FLOAT:
			printf("float \n");
			break;
		case GL_FLOAT_VEC2:
			printf("floatvec2 \n");
			break;
		case GL_FLOAT_VEC3:
			printf("floatvec3 \n");
			break;
		case GL_FLOAT_VEC4:
			printf("floatvec4 \n");
			break;
		case GL_INT:
			printf("int \n");
			break;
		case GL_INT_VEC2:
		case GL_INT_VEC3:
		case GL_INT_VEC4:
			printf("intVec \n");
			break;
		case GL_FLOAT_MAT2:
		case GL_FLOAT_MAT3:
		case GL_FLOAT_MAT4:
			printf("fmat \n");
			break;
		case GL_SAMPLER_2D:
			printf("samp2D \n");
			break;
		case GL_SAMPLER_CUBE:
			printf("sampCube \n");
			break;
		default:
			printf("other \n");
			break;
		}
	}
	gf_free(uniformName);
}

static void gf_glQueryAttributes(GF_SHADERID progObj)
{
	GLint maxAttributeLen;
	GLint numAttributes;
	char *attributeName;
	GLint index;

	printf("Listing Attribs... \n");
	glGetProgramiv(progObj, GL_ACTIVE_ATTRIBUTES, &numAttributes);
	glGetProgramiv(progObj, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeLen);
	attributeName = gf_malloc(sizeof(char) * maxAttributeLen);
	for(index = 0; index < numAttributes; index++) {
		GLint size;
		GLenum type;
		GLint location;
		// Get the Attribute Info
		glGetActiveAttrib(progObj, index, maxAttributeLen, NULL, &size, &type, attributeName);
		// Get the attribute location
		location = glGetAttribLocation(progObj, attributeName);
		if(location) printf("attrib %s is: ",attributeName);
		switch(type) {
		case GL_FLOAT:
			printf("float \n");
			break;
		case GL_FLOAT_VEC2:
			printf("floatvec2 \n");
			break;
		case GL_FLOAT_VEC3:
			printf("floatvec3 \n");
			break;
		case GL_FLOAT_VEC4:
			printf("floatvec4 \n");
			break;
		case GL_INT:
			printf("int \n");
			break;
		case GL_INT_VEC2:
		case GL_INT_VEC3:
		case GL_INT_VEC4:
			printf("intVec \n");
			break;
		case GL_FLOAT_MAT2:
		case GL_FLOAT_MAT3:
		case GL_FLOAT_MAT4:
			printf("fmat \n");
			break;
		case GL_SAMPLER_2D:
			printf("samp2D \n");
			break;
		case GL_SAMPLER_CUBE:
			printf("sampCube \n");
			break;
		default:
			printf("other \n");
			break;
		}
	}
	gf_free(attributeName);
}
#endif

static GF_GLProgInstance *visual_3d_build_program(GF_VisualManager *visual, u32 flags, u32 pix_fmt)
{
	s32 linked;
	GF_GLProgInstance *pi;

	GF_SAFEALLOC(pi, GF_GLProgInstance);
	if (!pi) return NULL;
	pi->flags = flags;
	pi->pix_fmt = pix_fmt;

	glGetError();
	pi->prog = glCreateProgram();
	if (!pi->prog) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to create program\n"));
		gf_free(pi);
		return NULL;
	}
	pi->vertex = visual_3d_shader_with_flags(visual->compositor->vertshader , GL_VERTEX_SHADER, flags, 0);
	if (!pi->vertex) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to compile vertex shader\n"));
		DEL_PROGRAM(pi->prog)
		gf_free(pi);
		return NULL;
	}
	if (pix_fmt == GF_PIXEL_GL_EXTERNAL) {
		flags |= GF_GL_IS_ExternalOES;
	}
	pi->fragment = visual_3d_shader_with_flags(visual->compositor->fragshader , GL_FRAGMENT_SHADER, flags, pix_fmt);
	if (!pi->fragment) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to compile fragment shader\n"));
		DEL_PROGRAM(pi->prog)
		DEL_SHADER(pi->vertex);
		gf_free(pi);
		return NULL;
	}
	glAttachShader(pi->prog, pi->vertex);
	GL_CHECK_ERR();
	glAttachShader(pi->prog, pi->fragment);
	GL_CHECK_ERR();

	glLinkProgram(pi->prog);
	GL_CHECK_ERR();

	glGetProgramiv(pi->prog, GL_LINK_STATUS, &linked);
	if (!linked) {
		int i32CharsWritten, i32InfoLogLength;
		char pszInfoLog[2048];
		glGetProgramiv(pi->prog, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		glGetProgramInfoLog(pi->prog, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, (pszInfoLog));
		DEL_PROGRAM(pi->prog)
		DEL_SHADER(pi->vertex);
		DEL_SHADER(pi->fragment);
		gf_free(pi);
		return NULL;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] fragment shader compiled fine\n"));
	return pi;
}

static Bool visual_3d_init_generic_shaders(GF_VisualManager *visual)
{
	GF_GLProgInstance *pi;
	if (gf_list_count(visual->compiled_programs))
		return GF_TRUE;
	if (!gf_file_exists(visual->compositor->vertshader))
		return GF_FALSE;
	if (!gf_file_exists(visual->compositor->fragshader))
		return GF_FALSE;


	pi = visual_3d_build_program(visual, 0, 0);
	if (!pi) return GF_FALSE;

	glDeleteShader(pi->vertex);
	glDeleteShader(pi->fragment);
	glDeleteProgram(pi->prog);
	gf_free(pi);
	return GF_TRUE;
}

void visual_3d_init_shaders(GF_VisualManager *visual)
{
	if (visual->compositor->visual != visual)
		return;

	if (!visual->compositor->gl_caps.has_shaders)
		return;

	if (!visual->compositor->shader_mode_disabled) {
		//If we fail to configure shaders, force 2D mode
		if (! visual_3d_init_generic_shaders(visual)) {
			visual->compositor->hybrid_opengl = GF_FALSE;
			visual->compositor->force_opengl_2d = GF_FALSE;
			/*force resetup*/
			visual->compositor->root_visual_setup = 0;
			/*force texture setup when switching to OpenGL*/
			gf_sc_reset_graphics(visual->compositor);
			/*force redraw*/
			gf_sc_next_frame_state(visual->compositor, GF_SC_DRAW_FRAME);
		}
	}
}

#endif // !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)


void visual_3d_reset_graphics(GF_VisualManager *visual)
{
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	if (visual->compositor->visual != visual)
		return;

	DEL_SHADER(visual->base_glsl_vertex);
	DEL_SHADER(visual->autostereo_glsl_fragment);
	DEL_PROGRAM(visual->autostereo_glsl_program );

	if (visual->gl_textures) {
		glDeleteTextures(visual->nb_views, visual->gl_textures);
		gf_free(visual->gl_textures);
		visual->gl_textures = NULL;
	}
	if (visual->autostereo_mesh) {
		mesh_free(visual->autostereo_mesh);
		visual->autostereo_mesh = NULL;
	}

	while (gf_list_count(visual->compiled_programs)) {
		GF_GLProgInstance *gi = gf_list_pop_back(visual->compiled_programs);
		DEL_SHADER(gi->vertex);
		DEL_SHADER(gi->fragment);
		DEL_PROGRAM(gi->prog);
		gf_free(gi);
	}
#endif
}

void visual_3d_set_clipper_scissor(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
#ifndef GPAC_USE_TINYGL
	if (visual->has_clipper_2d) {
		u32 x, y;
		u32 dw, dh;
		glEnable(GL_SCISSOR_TEST);

		if (visual->offscreen) {
			dw = visual->width;
			dh = visual->height;
		} else {
			dw = visual->compositor->display_width;
			dh = visual->compositor->display_height;
		}

		if (visual->center_coords) {
			x = visual->clipper_2d.x + dw / 2;
			y = dh / 2 + visual->clipper_2d.y - visual->clipper_2d.height;
		} else {
			x = visual->clipper_2d.x;
			y = dh - visual->clipper_2d.y;
		}
		glScissor(x, y, visual->clipper_2d.width, visual->clipper_2d.height);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
#endif
}


#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

static void visual_3d_load_matrix_shaders(GF_SHADERID program, Fixed *mat, const char *name)
{
	GLint loc;
#ifdef GPAC_FIXED_POINT
	Float _mat[16];
	u32 i;
#endif

	loc = glGetUniformLocation(program, name);
	if(loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("GL Error (file %s line %d): Invalid matrix name", __FILE__, __LINE__));
		return;
	}
	GL_CHECK_ERR()

#ifdef GPAC_FIXED_POINT
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glUniformMatrix4fv(loc, 1, GL_FALSE, (GLfloat *) _mat);
#else
	glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
#endif
	GL_CHECK_ERR()
}

#endif

GF_Err visual_3d_init_autostereo(GF_VisualManager *visual)
{
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	u32 bw, bh;
	SFVec2f s;
	Bool use_npot = visual->compositor->gl_caps.npot_texture;
	if (visual->gl_textures) return GF_OK;

	visual->gl_textures = gf_malloc(sizeof(GLuint) * visual->nb_views);
	glGenTextures(visual->nb_views, visual->gl_textures);

	bw = visual->width;
	bh = visual->height;
	/*main (not offscreen) visual*/
	if (visual->compositor->visual==visual) {
		bw = visual->compositor->output_width;
		bh = visual->compositor->output_height;
	}

#ifdef GPAC_USE_GLES2
	use_npot = GF_TRUE;
#endif

	if (use_npot) {
		visual->auto_stereo_width = bw;
		visual->auto_stereo_height = bh;
	} else {
		visual->auto_stereo_width = 2;
		while (visual->auto_stereo_width < bw) visual->auto_stereo_width *= 2;
		visual->auto_stereo_height = 2;
		while (visual->auto_stereo_height < bh) visual->auto_stereo_height *= 2;
	}

	visual->autostereo_mesh = new_mesh();
	s.x = INT2FIX(bw);
	s.y = INT2FIX(bh);
	mesh_new_rectangle(visual->autostereo_mesh, s, NULL, 0);

	if (! use_npot) {
		u32 i;
		Fixed max_u = INT2FIX(bw) / visual->auto_stereo_width;
		Fixed max_v = INT2FIX(bh) / visual->auto_stereo_height;
		for (i=0; i<visual->autostereo_mesh->v_count; i++) {
			if (visual->autostereo_mesh->vertices[i].texcoords.x == FIX_ONE) {
				visual->autostereo_mesh->vertices[i].texcoords.x = max_u;
			}
			if (visual->autostereo_mesh->vertices[i].texcoords.y == FIX_ONE) {
				visual->autostereo_mesh->vertices[i].texcoords.y = max_v;
			}
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual3D] AutoStereo initialized - width %d height %d\n", visual->auto_stereo_width, visual->auto_stereo_height) );

	visual_3d_init_stereo_shaders(visual);
#endif // !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

	return GF_OK;
}

void visual_3d_end_auto_stereo_pass(GF_VisualManager *visual)
{
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	u32 i;
	GLint loc, loc_vertex_attrib, loc_texcoord_attrib;
	Fixed hw, hh;
	GF_Matrix mx;


	glFlush();

	GL_CHECK_ERR()

#ifndef GPAC_USE_GLES2
	glEnable(GL_TEXTURE_2D);
#endif

	glBindTexture(GL_TEXTURE_2D, visual->gl_textures[visual->current_view]);

#ifndef GPAC_USE_GLES2
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif

	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, visual->auto_stereo_width, visual->auto_stereo_height, 0);
	GL_CHECK_ERR()

#ifndef GPAC_USE_GLES2
	glDisable(GL_TEXTURE_2D);
#endif

	glClear(GL_DEPTH_BUFFER_BIT);
	GL_CHECK_ERR()

	if (visual->current_view+1<visual->nb_views) return;
	hw = INT2FIX(visual->width);
	hh = INT2FIX(visual->height);
	/*main (not offscreen) visual*/
	if (visual->compositor->visual==visual) {
		hw = INT2FIX(visual->compositor->output_width);
		hh = INT2FIX(visual->compositor->output_height);
	}

	glViewport(0, 0, (GLsizei) hw, (GLsizei) hh );

	hw /= 2;
	hh /= 2;

	/*use our program*/
	glUseProgram(visual->autostereo_glsl_program);

	//load projection
	gf_mx_ortho(&mx, -hw, hw, -hh, hh, -10, 100);
	visual_3d_load_matrix_shaders(visual->autostereo_glsl_program, mx.m, "gfProjectionMatrix");

	//no need for modelview (identifty)

	/*push number of views if shader uses it*/
	loc = glGetUniformLocation(visual->autostereo_glsl_program, "gfViewCount");
	if (loc != -1) glUniform1i(loc, visual->nb_views);

	loc_texcoord_attrib = -1;
	//setup vertex attrib
	loc_vertex_attrib = glGetAttribLocation(visual->autostereo_glsl_program, "gfVertex");
	if (loc_vertex_attrib>=0) {
		glVertexAttribPointer(loc_vertex_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(GF_Vertex), &visual->autostereo_mesh->vertices[0].pos);
		glEnableVertexAttribArray(loc_vertex_attrib);

		GL_CHECK_ERR()

		//setup texcoord location
		loc_texcoord_attrib = glGetAttribLocation(visual->autostereo_glsl_program, "gfTextureCoordinates");
		if (loc_texcoord_attrib>=0) {
			char szTex[100];
			glVertexAttribPointer(loc_texcoord_attrib, 2, GL_FLOAT, GL_FALSE, sizeof(GF_Vertex), &visual->autostereo_mesh->vertices[0].texcoords);
			glEnableVertexAttribArray(loc_texcoord_attrib);

			GL_CHECK_ERR()

			/*bind all our textures*/
			for (i=0; i<visual->nb_views; i++) {
				sprintf(szTex, "gfView%d", i+1);
				loc = glGetUniformLocation(visual->autostereo_glsl_program, szTex);
				if (loc == -1) continue;

				glActiveTexture(GL_TEXTURE0 + i);

#ifndef GPAC_USE_GLES2
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
#endif

				GL_CHECK_ERR()

				glBindTexture(GL_TEXTURE_2D, visual->gl_textures[i]);

				GL_CHECK_ERR()

				glUniform1i(loc, i);

				GL_CHECK_ERR()
			}

			//draw
#if defined(GPAC_USE_GLES2)
			glDrawElements(GL_TRIANGLES, visual->autostereo_mesh->i_count, GL_UNSIGNED_SHORT, visual->autostereo_mesh->indices);
#else
			glDrawElements(GL_TRIANGLES, visual->autostereo_mesh->i_count, GL_UNSIGNED_INT, visual->autostereo_mesh->indices);
#endif

			GL_CHECK_ERR()
		}
	}


	if (loc_vertex_attrib>=0) glDisableVertexAttribArray(loc_vertex_attrib);
	if (loc_texcoord_attrib>=0) glDisableVertexAttribArray(loc_texcoord_attrib);
	GL_CHECK_ERR()

	glUseProgram(0);

#ifndef GPAC_USE_GLES2
	/*not sure why this is needed but it prevents a texturing bug on XP on parallels*/
	glActiveTexture(GL_TEXTURE0);
	GL_CHECK_ERR()

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
#endif

	GL_CHECK_ERR()
#endif // !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

}


static void visual_3d_setup_quality(GF_VisualManager *visual)
{
#ifndef GPAC_USE_GLES2

	if (visual->compositor->fast) {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
#ifdef GL_POLYGON_SMOOTH_HINT
		glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
#endif
	} else {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
#ifdef GL_POLYGON_SMOOTH_HINT
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
#endif
	}

	if (visual->compositor->aa == GF_ANTIALIAS_FULL) {
		glEnable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_GLES1X
		if (visual->compositor->paa)
			glEnable(GL_POLYGON_SMOOTH);
		else
			glDisable(GL_POLYGON_SMOOTH);
#endif
	} else {
		glDisable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_GLES1X
		glDisable(GL_POLYGON_SMOOTH);
#endif
	}

#endif

}

void visual_3d_setup(GF_VisualManager *visual)
{
	//in non-player mode, we might not be the only ones using the gl context !!
	if (visual->compositor->player && visual->gl_setup) {
		visual->has_fog = GF_FALSE;
		glClear(GL_DEPTH_BUFFER_BIT);
		return;
	}

#ifndef GPAC_USE_TINYGL
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LEQUAL);
#endif
	glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

#ifdef GPAC_USE_GLES1X
	glClearDepthx(FIX_ONE);
	glLightModelx(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, FLT2FIX(0.2f * 128) );
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#elif defined(GPAC_USE_GLES2)
	glClearDepthf(1.0f);
#else
	glClearDepth(1.0f);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, (float) (0.2 * 128));
#endif


#ifndef GPAC_USE_GLES2
	glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glGetIntegerv(GL_MAX_LIGHTS, (GLint*)&visual->max_lights);
	if (visual->max_lights>GF_MAX_GL_LIGHTS)
		visual->max_lights=GF_MAX_GL_LIGHTS;

#ifdef GL_MAX_CLIP_PLANES
	glGetIntegerv(GL_MAX_CLIP_PLANES, (GLint*)&visual->max_clips);
	if (visual->max_clips>GF_MAX_GL_CLIPS)
		visual->max_clips=GF_MAX_GL_CLIPS;
#endif

	glDisable(GL_POINT_SMOOTH);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDisable(GL_FOG);
	/*Note: we cannot enable/disable normalization on the fly, because we have no clue when the GL implementation
	will actually compute the related fragments. Since a typical world always use scaling, we always turn normalization on
	to avoid tracking scale*/
	glEnable(GL_NORMALIZE);
#endif //GLES2

	visual_3d_setup_quality(visual);

	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	visual->has_fog = GF_FALSE;
	visual->max_lights=GF_MAX_GL_LIGHTS;
	visual->max_clips=GF_MAX_GL_CLIPS;

	visual->gl_setup = GF_TRUE;

	glClear(GL_DEPTH_BUFFER_BIT);
}

void visual_3d_set_background_state(GF_VisualManager *visual, Bool on)
{
#ifndef GPAC_USE_GLES2
	if (on) {
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glDisable(GL_LINE_SMOOTH);

		glDisable(GL_BLEND);
#ifndef GPAC_USE_GLES1X
		glDisable(GL_POLYGON_SMOOTH);
#endif
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	} else {
		visual_3d_setup_quality(visual);
	}
#endif

	visual_3d_enable_depth_buffer(visual, ! on);
}



void visual_3d_enable_antialias(GF_VisualManager *visual, Bool bOn)
{
#ifndef GPAC_USE_GLES2

	if (bOn) {
		glEnable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_GLES1X
		if (visual->compositor->paa)
			glEnable(GL_POLYGON_SMOOTH);
		else
			glDisable(GL_POLYGON_SMOOTH);
#endif
	} else {
		glDisable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_GLES1X
		glDisable(GL_POLYGON_SMOOTH);
#endif

/*		glDisable(GL_DITHER);
		glDisable(GL_POINT_SMOOTH);
		glHint(GL_POINT_SMOOTH, GL_DONT_CARE);
		glHint(GL_LINE_SMOOTH, GL_DONT_CARE);
		glHint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);
	
		glDisable( GL_MULTISAMPLE_ARB);
*/
	}

#endif
}

void visual_3d_enable_depth_buffer(GF_VisualManager *visual, Bool on)
{
	if (on) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}
}

void visual_3d_set_viewport(GF_VisualManager *visual, GF_Rect vp)
{
	glViewport(FIX2INT(vp.x), FIX2INT(vp.y), FIX2INT(vp.width), FIX2INT(vp.height));
}

void visual_3d_set_scissor(GF_VisualManager *visual, GF_Rect *vp)
{
#ifndef GPAC_USE_TINYGL
	if (vp) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(FIX2INT(vp->x), FIX2INT(vp->y), FIX2INT(vp->width), FIX2INT(vp->height));
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
#endif
}

void visual_3d_clear_depth(GF_VisualManager *visual)
{
	glClear(GL_DEPTH_BUFFER_BIT);
}

static void visual_3d_draw_aabb_node(GF_TraverseState *tr_state, GF_Mesh *mesh, u32 prim_type, GF_Plane *fplanes, u32 *p_indices, AABBNode *n, void *idx_addr)
{
	u32 i;

	/*if not leaf do cull*/
	if (n->pos) {
		u32 p_idx, cull;
		SFVec3f vertices[8];
		/*get box vertices*/
		gf_bbox_get_vertices(n->min, n->max, vertices);
		cull = CULL_INSIDE;
		for (i=0; i<6; i++) {
			p_idx = p_indices[i];
			/*check p-vertex: if not in plane, we're out (since p-vertex is the closest point to the plane)*/
			if (gf_plane_get_distance(&fplanes[i], &vertices[p_idx])<0) {
				cull = CULL_OUTSIDE;
				break;
			}
			/*check n-vertex: if not in plane, we're intersecting*/
			if (gf_plane_get_distance(&fplanes[i], &vertices[7-p_idx])<0) {
				cull = CULL_INTERSECTS;
				break;
			}
		}

		if (cull==CULL_OUTSIDE) return;

		if (cull==CULL_INTERSECTS) {
			visual_3d_draw_aabb_node(tr_state, mesh, prim_type, fplanes, p_indices, n->pos, idx_addr);
			visual_3d_draw_aabb_node(tr_state, mesh, prim_type, fplanes, p_indices, n->neg, idx_addr);
			return;
		}
	}

	/*the good thing about the structure of the aabb tree is that the primitive idx is valid for both
	leaf and non-leaf nodes, so we can use it as soon as we have a CULL_INSIDE.
	However we must push triangles one by one since primitive order may have been swapped when
	building the AABB tree*/
	for (i=0; i<n->nb_idx; i++) {
		u32 idx = 3*n->indices[i];
		void *vbi_addr;
		if (!idx_addr) vbi_addr = (void *) PTR_TO_U_CAST ( sizeof(IDX_TYPE) * idx );
		else vbi_addr = &mesh->indices[idx];

#if defined(GPAC_USE_GLES1X) || defined(GPAC_USE_GLES2)
		glDrawElements(prim_type, 3, GL_UNSIGNED_SHORT, vbi_addr);
#else
		glDrawElements(prim_type, 3, GL_UNSIGNED_INT, vbi_addr);
#endif
	}
}

#ifndef GPAC_USE_GLES2

static void visual_3d_matrix_load(GF_VisualManager *visual, Fixed *mat)
{
#if defined(GPAC_FIXED_POINT)
	Float _mat[16];
	u32 i;
#endif

	if (!mat) {
		glLoadIdentity();
		return;
	}
#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
	glLoadMatrixx(mat);
#elif defined(GPAC_FIXED_POINT)
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glLoadMatrixf(_mat);
#else
	glLoadMatrixf(mat);
#endif
}

static void visual_3d_update_matrices(GF_TraverseState *tr_state)
{
	GF_Matrix mx;
	if (!tr_state || !tr_state->camera) return;
	if (tr_state->visual->needs_projection_matrix_reload) {
		tr_state->visual->needs_projection_matrix_reload = 0;
		glMatrixMode(GL_PROJECTION);
		visual_3d_matrix_load(tr_state->visual, tr_state->camera->projection.m);
		glMatrixMode(GL_MODELVIEW);
	}

	gf_mx_copy(mx, tr_state->camera->modelview.m);
	gf_mx_add_matrix(&mx, &tr_state->model_matrix);
	visual_3d_matrix_load(tr_state->visual, (Fixed *) &mx);
}


static void visual_3d_set_clippers(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
#ifdef GL_MAX_CLIP_PLANES
	u32 i;
	GF_Matrix inv_mx;

	if (!visual->num_clips)
		return;

	gf_mx_copy(inv_mx, tr_state->model_matrix);
	gf_mx_inverse(&inv_mx);

	for (i=0; i<visual->num_clips; i++) {
		u32 idx = GL_CLIP_PLANE0 + i;
#ifdef GPAC_USE_GLES1X
		Fixed g[4];
#else
		Double g[4];
#endif
		GF_Matrix mx;
		GF_Plane p = visual->clippers[i].p;

		if (visual->clippers[i].is_2d_clip) {
			visual_3d_matrix_load(tr_state->visual, tr_state->camera->modelview.m);
		} else {
			gf_mx_copy(mx, inv_mx);
			if (visual->clippers[i].mx_clipper != NULL) {
				gf_mx_add_matrix(&mx, visual->clippers[i].mx_clipper);
			}
			gf_mx_apply_plane(&mx, &p);
		}

#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
		g[0] = p.normal.x;
		g[1] = p.normal.y;
		g[2] = p.normal.z;
		g[3] = p.d;
		glClipPlanex(idx, g);
#else
		g[0] = FIX2FLT(p.normal.x);
		g[1] = FIX2FLT(p.normal.y);
		g[2] = FIX2FLT(p.normal.z);
		g[3] = FIX2FLT(p.d);

#if defined(GPAC_USE_GLES1X)
		glClipPlanef(idx, g);
#else
		glClipPlane(idx, g);
#endif

#endif
		glEnable(idx);

		if (visual->clippers[i].is_2d_clip) {
			visual_3d_update_matrices(tr_state);
		}

	}
#endif

}

static void visual_3d_reset_clippers(GF_VisualManager *visual)
{
#ifdef GL_MAX_CLIP_PLANES
	u32 i;
	for (i=0; i<visual->num_clips; i++) {
		glDisable(GL_CLIP_PLANE0 + i);
	}
#endif
}


void visual_3d_reset_lights(GF_VisualManager *visual)
{
	u32 i;
	if (!visual->num_lights) return;
	for (i=0; i<visual->num_lights; i++) {
		glDisable(GL_LIGHT0 + i);
	}
	glDisable(GL_LIGHTING);
}


static void visual_3d_set_lights(GF_VisualManager *visual)
{
	u32 i;
#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
	Fixed vals[4], exp;
#else
	Float vals[4], intensity, cutOffAngle, beamWidth, ambientIntensity, exp;
#endif

	if (!visual->num_lights) return;

	for (i=0; i<visual->num_lights; i++) {
		GF_Matrix mx;
		GF_LightInfo *li = &visual->lights[i];
		GLint iLight = GL_LIGHT0 + i;
		if(li->type==3) {
			gf_mx_init(mx);
		} else {
			gf_mx_copy(mx, visual->camera.modelview.m);
			gf_mx_add_matrix(&mx, &li->light_mx);
		}
		visual_3d_matrix_load(visual, mx.m);

		glEnable(iLight);

		switch (li->type) {
		//directional light
		case 0:
		case 3:
#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
			vals[0] = -li->direction.x;
			vals[1] = -li->direction.y;
			vals[2] = -li->direction.z;
			vals[3] = 0;
			glLightxv(iLight, GL_POSITION, vals);
			vals[0] = gf_mulfix(li->color.red, li->intensity);
			vals[1] = gf_mulfix(li->color.green, li->intensity);
			vals[2] = gf_mulfix(li->color.blue, li->intensity);
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_DIFFUSE, vals);
			glLightxv(iLight, GL_SPECULAR, vals);
			vals[0] = gf_mulfix(li->color.red, li->ambientIntensity);
			vals[1] = gf_mulfix(li->color.green, li->ambientIntensity);
			vals[2] = gf_mulfix(li->color.blue, li->ambientIntensity);
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_AMBIENT, vals);

			glLightx(iLight, GL_CONSTANT_ATTENUATION, FIX_ONE);
			glLightx(iLight, GL_LINEAR_ATTENUATION, 0);
			glLightx(iLight, GL_QUADRATIC_ATTENUATION, 0);
			glLightx(iLight, GL_SPOT_CUTOFF, INT2FIX(180) );
#else
			ambientIntensity = FIX2FLT(li->ambientIntensity);
			intensity = FIX2FLT(li->intensity);

			vals[0] = -FIX2FLT(li->direction.x);
			vals[1] = -FIX2FLT(li->direction.y);
			vals[2] = -FIX2FLT(li->direction.z);
			vals[3] = 0;
			glLightfv(iLight, GL_POSITION, vals);
			vals[0] = FIX2FLT(li->color.red)*intensity;
			vals[1] = FIX2FLT(li->color.green)*intensity;
			vals[2] = FIX2FLT(li->color.blue)*intensity;
			vals[3] = 1;
			glLightfv(iLight, GL_DIFFUSE, vals);
			glLightfv(iLight, GL_SPECULAR, vals);
			vals[0] = FIX2FLT(li->color.red)*ambientIntensity;
			vals[1] = FIX2FLT(li->color.green)*ambientIntensity;
			vals[2] = FIX2FLT(li->color.blue)*ambientIntensity;
			vals[3] = 1;
			glLightfv(iLight, GL_AMBIENT, vals);

			glLightf(iLight, GL_CONSTANT_ATTENUATION, 1.0f);
			glLightf(iLight, GL_LINEAR_ATTENUATION, 0);
			glLightf(iLight, GL_QUADRATIC_ATTENUATION, 0);
			glLightf(iLight, GL_SPOT_CUTOFF, 180);
#endif
			break;


		//spot light
		case 1:
#ifndef GPAC_USE_GLES1X
			ambientIntensity = FIX2FLT(li->ambientIntensity);
			intensity = FIX2FLT(li->intensity);
			cutOffAngle = FIX2FLT(li->cutOffAngle);
			beamWidth = FIX2FLT(li->beamWidth);
#endif

#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
			vals[0] = li->direction.x;
			vals[1] = li->direction.y;
			vals[2] = li->direction.z;
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_SPOT_DIRECTION, vals);
			vals[0] = li->position.x;
			vals[1] = li->position.y;
			vals[2] = li->position.z;
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_POSITION, vals);
			glLightx(iLight, GL_CONSTANT_ATTENUATION, li->attenuation.x ? li->attenuation.x : FIX_ONE);
			glLightx(iLight, GL_LINEAR_ATTENUATION, li->attenuation.y);
			glLightx(iLight, GL_QUADRATIC_ATTENUATION, li->attenuation.z);
			vals[0] = gf_mulfix(li->color.red, li->intensity);
			vals[1] = gf_mulfix(li->color.green, li->intensity);
			vals[2] = gf_mulfix(li->color.blue, li->intensity);
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_DIFFUSE, vals);
			glLightxv(iLight, GL_SPECULAR, vals);
			vals[0] = gf_mulfix(li->color.red, li->ambientIntensity);
			vals[1] = gf_mulfix(li->color.green, li->ambientIntensity);
			vals[2] = gf_mulfix(li->color.blue, li->ambientIntensity);
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_AMBIENT, vals);

			if (!li->beamWidth) exp = FIX_ONE;
			else if (li->beamWidth > li->cutOffAngle) exp = 0;
			else {
				exp = FIX_ONE - gf_cos(li->beamWidth);
				if (exp>FIX_ONE) exp = FIX_ONE;
			}
			glLightx(iLight, GL_SPOT_EXPONENT,  exp*128);
			glLightx(iLight, GL_SPOT_CUTOFF, gf_divfix(180*li->cutOffAngle, GF_PI) );
#else
			vals[0] = FIX2FLT(li->direction.x);
			vals[1] = FIX2FLT(li->direction.y);
			vals[2] = FIX2FLT(li->direction.z);
			vals[3] = 1;
			glLightfv(iLight, GL_SPOT_DIRECTION, vals);
			vals[0] = FIX2FLT(li->position.x);
			vals[1] = FIX2FLT(li->position.y);
			vals[2] = FIX2FLT(li->position.z);
			vals[3] = 1;
			glLightfv(iLight, GL_POSITION, vals);
			glLightf(iLight, GL_CONSTANT_ATTENUATION, li->attenuation.x ? FIX2FLT(li->attenuation.x) : 1.0f);
			glLightf(iLight, GL_LINEAR_ATTENUATION, FIX2FLT(li->attenuation.y));
			glLightf(iLight, GL_QUADRATIC_ATTENUATION, FIX2FLT(li->attenuation.z));
			vals[0] = FIX2FLT(li->color.red)*intensity;
			vals[1] = FIX2FLT(li->color.green)*intensity;
			vals[2] = FIX2FLT(li->color.blue)*intensity;
			vals[3] = 1;
			glLightfv(iLight, GL_DIFFUSE, vals);
			glLightfv(iLight, GL_SPECULAR, vals);
			vals[0] = FIX2FLT(li->color.red)*ambientIntensity;
			vals[1] = FIX2FLT(li->color.green)*ambientIntensity;
			vals[2] = FIX2FLT(li->color.blue)*ambientIntensity;
			vals[3] = 1;
			glLightfv(iLight, GL_AMBIENT, vals);

			//glLightf(iLight, GL_SPOT_EXPONENT, 0.5f * (beamWidth+0.001f) /*(Float) (0.5 * log(0.5) / log(cos(beamWidth)) ) */);
			if (!beamWidth) exp = 1;
			else if (beamWidth>cutOffAngle) exp = 0;
			else {
				exp = 1.0f - (Float) cos(beamWidth);
				if (exp>1) exp = 1;
			}
			glLightf(iLight, GL_SPOT_EXPONENT,  exp*128);
			glLightf(iLight, GL_SPOT_CUTOFF, 180*cutOffAngle/FIX2FLT(GF_PI));
#endif
			break;


		//point light
		case 2:
#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
			vals[0] = li->position.x;
			vals[1] = li->position.y;
			vals[2] = li->position.z;
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_POSITION, vals);
			glLightx(iLight, GL_CONSTANT_ATTENUATION, li->attenuation.x ? li->attenuation.x : FIX_ONE);
			glLightx(iLight, GL_LINEAR_ATTENUATION, li->attenuation.y);
			glLightx(iLight, GL_QUADRATIC_ATTENUATION, li->attenuation.z);
			vals[0] = gf_mulfix(li->color.red, li->intensity);
			vals[1] = gf_mulfix(li->color.green, li->intensity);
			vals[2] = gf_mulfix(li->color.blue, li->intensity);
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_DIFFUSE, vals);
			glLightxv(iLight, GL_SPECULAR, vals);
			vals[0] = gf_mulfix(li->color.red, li->ambientIntensity);
			vals[1] = gf_mulfix(li->color.green, li->ambientIntensity);
			vals[2] = gf_mulfix(li->color.blue, li->ambientIntensity);
			vals[3] = FIX_ONE;
			glLightxv(iLight, GL_AMBIENT, vals);

			glLightx(iLight, GL_SPOT_EXPONENT, 0);
			glLightx(iLight, GL_SPOT_CUTOFF, INT2FIX(180) );
#else
			ambientIntensity = FIX2FLT(li->ambientIntensity);
			intensity = FIX2FLT(li->intensity);

			vals[0] = FIX2FLT(li->position.x);
			vals[1] = FIX2FLT(li->position.y);
			vals[2] = FIX2FLT(li->position.z);
			vals[3] = 1;
			glLightfv(iLight, GL_POSITION, vals);
			glLightf(iLight, GL_CONSTANT_ATTENUATION, li->attenuation.x ? FIX2FLT(li->attenuation.x) : 1.0f);
			glLightf(iLight, GL_LINEAR_ATTENUATION, FIX2FLT(li->attenuation.y));
			glLightf(iLight, GL_QUADRATIC_ATTENUATION, FIX2FLT(li->attenuation.z));
			vals[0] = FIX2FLT(li->color.red)*intensity;
			vals[1] = FIX2FLT(li->color.green)*intensity;
			vals[2] = FIX2FLT(li->color.blue)*intensity;
			vals[3] = 1;
			glLightfv(iLight, GL_DIFFUSE, vals);
			glLightfv(iLight, GL_SPECULAR, vals);
			vals[0] = FIX2FLT(li->color.red)*ambientIntensity;
			vals[1] = FIX2FLT(li->color.green)*ambientIntensity;
			vals[2] = FIX2FLT(li->color.blue)*ambientIntensity;
			vals[3] = 1;
			glLightfv(iLight, GL_AMBIENT, vals);

			glLightf(iLight, GL_SPOT_EXPONENT, 0);
			glLightf(iLight, GL_SPOT_CUTOFF, 180);
#endif
			break;
		}
	}

	glEnable(GL_LIGHTING);

}

void visual_3d_enable_fog(GF_VisualManager *visual)
{

#ifndef GPAC_USE_TINYGL

#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
	Fixed vals[4];
	glEnable(GL_FOG);
	if (!visual->fog_type) glFogx(GL_FOG_MODE, GL_LINEAR);
	else if (visual->fog_type==1) glFogx(GL_FOG_MODE, GL_EXP);
	else if (visual->fog_type==2) glFogx(GL_FOG_MODE, GL_EXP2);
	glFogx(GL_FOG_DENSITY, visual->fog_density);
	glFogx(GL_FOG_START, 0);
	glFogx(GL_FOG_END, visual->fog_visibility);
	vals[0] = visual->fog_color.red;
	vals[1] = visual->fog_color.green;
	vals[2] = visual->fog_color.blue;
	vals[3] = FIX_ONE;
	glFogxv(GL_FOG_COLOR, vals);
	glHint(GL_FOG_HINT, visual->compositor->fast ? GL_FASTEST : GL_NICEST);
#else
	Float vals[4];
	glEnable(GL_FOG);

#if defined(GPAC_USE_GLES1X)
	if (!visual->fog_type) glFogf(GL_FOG_MODE, GL_LINEAR);
	else if (visual->fog_type==1) glFogf(GL_FOG_MODE, GL_EXP);
	else if (visual->fog_type==2) glFogf(GL_FOG_MODE, GL_EXP2);
#else
	if (!visual->fog_type) glFogi(GL_FOG_MODE, GL_LINEAR);
	else if (visual->fog_type==1) glFogi(GL_FOG_MODE, GL_EXP);
	else if (visual->fog_type==2) glFogi(GL_FOG_MODE, GL_EXP2);
#endif

	glFogf(GL_FOG_DENSITY, FIX2FLT(visual->fog_density));
	glFogf(GL_FOG_START, 0);
	glFogf(GL_FOG_END, FIX2FLT(visual->fog_visibility));
	vals[0] = FIX2FLT(visual->fog_color.red);
	vals[1] = FIX2FLT(visual->fog_color.green);
	vals[2] = FIX2FLT(visual->fog_color.blue);
	vals[3] = 1;
	glFogfv(GL_FOG_COLOR, vals);
	glHint(GL_FOG_HINT, visual->compositor->fast ? GL_FASTEST : GL_NICEST);
#endif

#endif

}

#endif // ! GPAC_USE_GLES2


static void visual_3d_do_draw_mesh(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	u32 prim_type;
	GF_Matrix mx;
	u32 i, p_idx[6];
	void *idx_addr = NULL;
	GF_Plane fplanes[6];


	switch (mesh->mesh_type) {
	case MESH_LINESET:
		prim_type = GL_LINES;
		break;
	case MESH_POINTSET:
		prim_type = GL_POINTS;
		break;
	default:
		prim_type = GL_TRIANGLES;
		break;
	}

	if (mesh->vbo_idx) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->vbo_idx);
	} else {
		idx_addr = mesh->indices;
	}


	/*if inside or no aabb for the mesh draw vertex array*/
	if (!tr_state->visual->compositor->cull || (tr_state->cull_flag==CULL_INSIDE) || !mesh->aabb_root || !mesh->aabb_root->pos)	{
#if defined(GPAC_USE_GLES1X) || defined(GPAC_USE_GLES2)
		glDrawElements(prim_type, mesh->i_count, GL_UNSIGNED_SHORT, idx_addr);
#else
		glDrawElements(prim_type, mesh->i_count, GL_UNSIGNED_INT, idx_addr);
#endif

	} else {

		/*otherwise cull aabb against frustum - after some testing it appears (as usual) that there must
		 be a compromise: we're slowing down the compositor here, however the gain is really appreciable for
		 large meshes, especially terrains/elevation grids*/

		/*first get transformed frustum in local space*/
		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_inverse(&mx);
		for (i=0; i<6; i++) {
			fplanes[i] = tr_state->camera->planes[i];
			gf_mx_apply_plane(&mx, &fplanes[i]);
			p_idx[i] = gf_plane_get_p_vertex_idx(&fplanes[i]);
		}
		/*then recursively cull & draw AABB tree*/
		visual_3d_draw_aabb_node(tr_state, mesh, prim_type, fplanes, p_idx, mesh->aabb_root->pos, idx_addr);
		visual_3d_draw_aabb_node(tr_state, mesh, prim_type, fplanes, p_idx, mesh->aabb_root->neg, idx_addr);
	}

	if (mesh->vbo_idx) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

static Bool visual_3d_bind_buffer(GF_Compositor *compositor, GF_Mesh *mesh, void **base_address)
{
	*base_address = NULL;
	if ((compositor->reset_graphics==2) && mesh->vbo) {
		/*we lost OpenGL context at previous frame, recreate VBO*/
		mesh->vbo = 0;
		mesh->vbo_idx = 0;
	}
	/*rebuild VBO for large ojects only (we basically filter quads out)*/
	if (!mesh->vbo && compositor->gl_caps.vbo
#ifndef GPAC_USE_GLES2
	        && (mesh->v_count>4)
#endif
	   ) {
		glGenBuffers(1, &mesh->vbo);
		if (mesh->vbo) {
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
			glBufferData(GL_ARRAY_BUFFER, mesh->v_count * sizeof(GF_Vertex) , mesh->vertices, (mesh->vbo_dynamic) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
			mesh->vbo_dirty = 0;
		} else {
			return GF_FALSE;
		}

		glGenBuffers(1, &mesh->vbo_idx);
		if (mesh->vbo_idx) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->vbo_idx);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->i_count*sizeof(IDX_TYPE), mesh->indices, (mesh->vbo_dynamic) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		} else {
			return GF_FALSE;
		}
	}

	if (mesh->vbo) {
		*base_address = NULL;
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	} else {
		*base_address = &mesh->vertices[0].pos;
	}

	if (mesh->vbo_dirty) {
		glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->v_count * sizeof(GF_Vertex) , mesh->vertices);

		if (mesh->vbo_idx) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->vbo_idx);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->i_count*sizeof(IDX_TYPE), mesh->indices, (mesh->vbo_dynamic) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}

		mesh->vbo_dirty = 0;
	}
	return GF_TRUE;
}


#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

static void visual_3d_update_matrices_shaders(GF_TraverseState *tr_state)
{
	GF_Matrix mx;

	if (tr_state->visual->needs_projection_matrix_reload) {
		tr_state->visual->needs_projection_matrix_reload = 0;
		visual_3d_load_matrix_shaders(tr_state->visual->glsl_program, (Fixed *) tr_state->camera->projection.m, "gfProjectionMatrix");
	}

	//calculate ModelView matix (camera.view * model)
	gf_mx_copy(mx, tr_state->camera->modelview);
	gf_mx_add_matrix(&mx, &tr_state->model_matrix);
	visual_3d_load_matrix_shaders(tr_state->visual->glsl_program, (Fixed *) &mx.m, "gfModelViewMatrix");
}

static void visual_3d_set_lights_shaders(GF_TraverseState *tr_state)
{
	u32 i;
	GF_LightInfo *li;
	GF_Vec pt;
	Float ambientIntensity, intensity, vals[4];
	GLint loc;
	GF_Matrix mx;
	GF_VisualManager *visual = tr_state->visual;
	char szName[100];

	loc = glGetUniformLocation(visual->glsl_program, "gfNumLights");
	if (loc>=0)
		glUniform1i(loc, visual->num_lights);
	GL_CHECK_ERR()

	/*
	 * Equivalent to glLightModel(GL_LIGHTMODEL_TWO_SIDE, GL_TRUE);
	 */
	loc = glGetUniformLocation(visual->glsl_program, "gfLightTwoSide");
	if (loc>=0)
		glUniform1i(loc, 1);
	GL_CHECK_ERR()

	li = &visual->lights[0];

	pt = li->direction;
	gf_vec_norm(&pt);
	vals[0] = -FIX2FLT(pt.x);
	vals[1] = -FIX2FLT(pt.y);
	vals[2] = -FIX2FLT(pt.z);
	vals[3] = 0;

	ambientIntensity = FIX2FLT(li->ambientIntensity);
	intensity = FIX2FLT(li->intensity);

	for (i = 0; i < (int) visual->num_lights; i++) {
		GF_Vec orig;
		li = &visual->lights[i];

		if (li->type==3) {	//we have a headlight
			gf_mx_init(mx);
		} else {
			//update mx according to the light mx
			gf_mx_copy(mx, visual->camera.modelview);
			gf_mx_add_matrix(&mx, &li->light_mx);
		}

		sprintf(szName, "%s%d%s", "lights[", i, "].type");
		loc = glGetUniformLocation(visual->glsl_program, szName);		//Uniform name lights[i].type
		if (loc>=0) {
			if (li->type==3) {
				glUniform1i(loc, 0); //headlight; Set type 0-directional
			} else {
				glUniform1i(loc, (GLint) li->type); //Set type 0-directional 1-spot 2-point
			}
		}

		//set for light direction (assuming origin = 0,0,0)
		pt = li->direction;
		orig.x = orig.y = orig.z = 0;
		gf_mx_apply_vec(&mx, &pt);
		gf_mx_apply_vec(&mx, &orig);
		gf_vec_diff(pt, pt, orig);
		gf_vec_norm(&pt);

		vals[0] = -FIX2FLT(pt.x);
		vals[1] = -FIX2FLT(pt.y);
		vals[2] = -FIX2FLT(pt.z);
		vals[3] = 0;
		sprintf(szName, "%s%d%s", "lights[", i, "].direction");
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0)
			glUniform4fv(loc, 1, vals); //Set direction

		if ((li->type==0) || (li->type==3) ) {
			pt = li->direction;
			vals[0] = -FIX2FLT(pt.x);
			vals[1] = -FIX2FLT(pt.y);
			vals[2] = -FIX2FLT(pt.z);
			vals[3] = 0;
		} else {	//we have a spot or point light
			pt = li->position;
			gf_mx_apply_vec(&mx, &pt);
			vals[0] = FIX2FLT(pt.x);
			vals[1] = FIX2FLT(pt.y);
			vals[2] = FIX2FLT(pt.z);
			vals[3] = 1.0;
		}
		sprintf(szName, "%s%d%s", "lights[", i, "].position");
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0)
			glUniform4fv(loc, 1, vals);

		pt = li->attenuation;
		if (li->type && !li->attenuation.x) {
			vals[0]=1.0;
		} else {
			vals[0] = FIX2FLT(pt.x);
		}
		vals[1] = FIX2FLT(pt.y);
		vals[2] = FIX2FLT(pt.z);
		sprintf(szName, "%s%d%s", "lights[", i, "].attenuation");
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0)
			glUniform3fv(loc, 1, vals);

		vals[0] = FIX2FLT(li->color.red);
		vals[1] = FIX2FLT(li->color.green);
		vals[2] = FIX2FLT(li->color.blue);
		vals[3] = 0;
		sprintf(szName, "%s%d%s", "lights[", i, "].color");
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0)
			glUniform4fv(loc, 1, vals);

		sprintf(szName, "%s%d%s", "lights[", i, "].intensity");
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0)
			glUniform1f(loc, li->intensity);

		sprintf(szName, "%s%d%s", "lights[", i, "].cutOffAngle");
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0)
			glUniform1f(loc, li->cutOffAngle);

	}


	vals[0] = FIX2FLT(li->color.red)*intensity;
	vals[1] = FIX2FLT(li->color.green)*intensity;
	vals[2] = FIX2FLT(li->color.blue)*intensity;
	vals[3] = 1;
	loc = glGetUniformLocation(visual->glsl_program, "gfLightDiffuse");
	if (loc>=0) glUniform4fv(loc, 1, vals);

	loc = glGetUniformLocation(visual->glsl_program, "gfLightSpecular");
	if (loc>=0) glUniform4fv(loc, 1, vals);

	vals[0] = FIX2FLT(li->color.red)*ambientIntensity;
	vals[1] = FIX2FLT(li->color.green)*ambientIntensity;
	vals[2] = FIX2FLT(li->color.blue)*ambientIntensity;
	vals[3] = 1;
	loc = glGetUniformLocation(visual->glsl_program, "gfLightAmbient");
	if (loc>=0) glUniform4fv(loc, 1, vals);

	GL_CHECK_ERR()
}

static void visual_3d_set_fog_shaders(GF_VisualManager *visual)
{
	GLint loc;
	if (visual->has_fog) {
		loc = glGetUniformLocation(visual->glsl_program, "gfFogEnabled");
		if(loc>=0)
			glUniform1i(loc, GL_TRUE);

		loc = glGetUniformLocation(visual->glsl_program, "gfFogColor");
		if(loc>=0)
			glUniform3fv(loc, 1, (GLfloat *) &visual->fog_color);

		loc = glGetUniformLocation(visual->glsl_program, "gfFogDensity");
		if(loc>=0)
			glUniform1f(loc, visual->fog_density );

		loc = glGetUniformLocation(visual->glsl_program, "gfFogType");
		if(loc>=0)
			glUniform1i(loc, (GLuint) visual->fog_type );

		loc = glGetUniformLocation(visual->glsl_program, "gfFogVisibility");
		if(loc>=0)
			glUniform1f(loc, visual->fog_visibility);
	} else {
		loc = glGetUniformLocation(visual->glsl_program, "gfFogEnabled");
		if(loc>=0)
			glUniform1i(loc, GL_FALSE);

	}
	GL_CHECK_ERR()
}

static void visual_3d_set_clippers_shaders(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	Fixed vals[4];
	char szName[100];
	GLint loc;
	u32 i;
	GF_Matrix inv_mx, eye_mx;

	if (!visual->num_clips) return;

	gf_mx_copy(inv_mx, tr_state->model_matrix);
	gf_mx_inverse(&inv_mx);

	gf_mx_copy(eye_mx, tr_state->camera->modelview);
	gf_mx_add_matrix(&eye_mx, &tr_state->model_matrix);

	loc = glGetUniformLocation(visual->glsl_program, "gfNumClippers");
	if (loc>=0)
		glUniform1i(loc, visual->num_clips);

	for (i = 0; i < visual->num_clips; i++) {
		GF_Matrix mx;
		GF_Plane p;

		p = visual->clippers[i].p;
		//compute clip plane in eye coordinates
		if (! visual->clippers[i].is_2d_clip) {
			gf_mx_copy(mx, inv_mx);
			if (visual->clippers[i].mx_clipper != NULL) {
				gf_mx_add_matrix(&mx, visual->clippers[i].mx_clipper);
			}
			//plane in local coordinates
			gf_mx_apply_plane(&mx, &p);
			//plane in eye coordinates
			gf_mx_apply_plane(&eye_mx, &p);
		}

		sprintf(szName, "%s%d%s", "clipPlane[", i, "]");	//parse plane values
		loc = glGetUniformLocation(visual->glsl_program, szName);
		if (loc>=0) {
			vals[0] = p.normal.x;
			vals[1] = p.normal.y;
			vals[2] = p.normal.z;
			vals[3] = p.d;
			glUniform4fv(loc, 1, vals); //Set Plane (w = distance)
		}
	}
}

GF_GLProgInstance *visual_3d_check_program_exists(GF_VisualManager *root_visual, u32 flags, u32 pix_fmt)
{
	u32 i, count;
	GF_GLProgInstance *prog = NULL;
	if (root_visual->compositor->shader_mode_disabled)
		return NULL;

	count = gf_list_count(root_visual->compiled_programs);
	for (i=0; i<count; i++) {
		prog = gf_list_get(root_visual->compiled_programs, i);
		if ((prog->flags==flags) && (prog->pix_fmt==pix_fmt)) break;
		prog = NULL;
	}

	if (!prog) {
		prog = visual_3d_build_program(root_visual, flags, pix_fmt);
		if (!prog) return NULL;
		gf_list_add(root_visual->compiled_programs, prog);
	}
	return prog;
}

static void visual_3d_draw_mesh_shader_only(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	void *vertex_buffer_address;
	GF_VisualManager *visual = tr_state->visual;
	GF_VisualManager *root_visual = visual->compositor->visual;
	GLint loc, loc_vertex_array, loc_color_array, loc_normal_array, loc_textcoord_array;
	u32 flags;
	u32 num_lights = visual->num_lights;
	Bool is_debug_bounds = GF_FALSE;
	GF_GLProgInstance *prog = NULL;

	flags = root_visual->active_glsl_flags;

	if (!mesh) {
		is_debug_bounds = GF_TRUE;
		mesh = tr_state->visual->compositor->unit_bbox;
		flags = 0;
	}

	if (visual->has_material_2d) {
		num_lights = 0;
	}

	if (!tr_state->mesh_num_textures && (mesh->flags & MESH_HAS_COLOR)) {
		flags |= GF_GL_HAS_COLOR;
		visual->has_material_2d = GF_FALSE;
	}
	else if (tr_state->mesh_num_textures && (mesh->mesh_type==MESH_TRIANGLES) && !(mesh->flags & MESH_NO_TEXTURE)) {
		flags |= GF_GL_HAS_TEXTURE;
	} else {
		flags &= ~GF_GL_HAS_TEXTURE;
		flags &= ~GF_GL_IS_YUV;
		flags &= ~GF_GL_IS_ExternalOES;

	}

	if (num_lights && !is_debug_bounds) {
		flags |= GF_GL_HAS_LIGHT;
	} else {
		flags &= ~GF_GL_HAS_LIGHT;
	}

	if (visual->num_clips) {
		flags |= GF_GL_HAS_CLIP;
	} else {
		flags &= ~GF_GL_HAS_CLIP;
	}

	root_visual->active_glsl_flags = visual->active_glsl_flags = flags;

	prog = visual_3d_check_program_exists(root_visual, flags, root_visual->bound_tx_pix_fmt);
	if (!prog) return;

	//check if we are using a different program than last time, if so force matrices updates
	if (visual->glsl_program != prog->prog) {
		tr_state->visual->needs_projection_matrix_reload = GF_TRUE;
	}

	GL_CHECK_ERR()
	visual->glsl_program = prog->prog;

	glUseProgram(visual->glsl_program);
	GL_CHECK_ERR()

	if (! visual_3d_bind_buffer(visual->compositor, mesh, &vertex_buffer_address)) {
		glUseProgram(0);
		return;
	}

	if (visual->state_blend_on)
		glEnable(GL_BLEND);

	visual_3d_update_matrices_shaders(tr_state);

	loc_color_array = loc_normal_array = loc_textcoord_array = -1;

	//setup vertext array location (always true)
	loc_vertex_array = glGetAttribLocation(visual->glsl_program, "gfVertex");
	if (loc_vertex_array<0)
		return;

	glEnableVertexAttribArray(loc_vertex_array);
	GL_CHECK_ERR()
#if defined(GPAC_FIXED_POINT)
	glVertexAttribPointer(loc_vertex_array, 3, GL_FIXED, GL_TRUE, sizeof(GF_Vertex), vertex_buffer_address);
#else
	glVertexAttribPointer(loc_vertex_array, 3, GL_FLOAT, GL_FALSE, sizeof(GF_Vertex), vertex_buffer_address);
#endif

	//setup scissor
	visual_3d_set_clipper_scissor(visual, tr_state);

	//setup clippers
	visual_3d_set_clippers_shaders(visual, tr_state);

	if (is_debug_bounds) {
		Float cols[4];
		loc = glGetUniformLocation(visual->glsl_program, "gfEmissionColor");
		cols[0] = 1.0f - FIX2FLT(visual->mat_2d.red);
		cols[1] = 1.0f - FIX2FLT(visual->mat_2d.green);
		cols[2] = 1.0f - FIX2FLT(visual->mat_2d.blue);
		cols[3] = 1.0f;
		if (loc>=0)
			glUniform4fv(loc, 1, (GLfloat *) cols);
		GL_CHECK_ERR()

		loc = glGetUniformLocation(visual->glsl_program, "hasMaterial2D");
		if (loc>=0)
			glUniform1i(loc, 1);
		GL_CHECK_ERR()
	}
	/* Material2D does not have any lights, color used is "gfEmissionColor" uniform */
	else if (visual->has_material_2d) {
		//for YUV manually set alpha
		if (flags & GF_GL_IS_YUV) {
			loc = glGetUniformLocation(visual->glsl_program, "alpha");
			if(loc>=0)
				glUniform1f(loc, FIX2FLT(visual->mat_2d.alpha));
		}
		//otherwise handle alpha with blend if no texture
		else if (!tr_state->mesh_num_textures) {
			if (visual->mat_2d.alpha < FIX_ONE) {
				glEnable(GL_BLEND);
			} else {
				glDisable(GL_BLEND);
			}
		}

		loc = glGetUniformLocation(visual->glsl_program, "gfEmissionColor");
		if (loc>=0)
			glUniform4fv(loc, 1, (GLfloat *) & visual->mat_2d);
		GL_CHECK_ERR()

		loc = glGetUniformLocation(visual->glsl_program, "hasMaterial2D");
		if (loc>=0)
			glUniform1i(loc, 1);
		GL_CHECK_ERR()

	} else {

		loc = glGetUniformLocation(visual->glsl_program, "hasMaterial2D");
		if (loc>=0)
			glUniform1i(loc, 0);
		GL_CHECK_ERR()

		//for YUV manually set alpha
		if (flags & GF_GL_IS_YUV) {
			loc = glGetUniformLocation(visual->glsl_program, "alpha");
			if(loc>=0)
				glUniform1f(loc, 1.0);
			GL_CHECK_ERR()
		}
	}

	/* if lighting is on, setup material */
	if ((flags & GF_GL_HAS_LIGHT) && visual->has_material && !visual->has_material_2d) {
		u32 i;
		for(i =0; i<4; i++) {
			Fixed *rgba = (Fixed *) & visual->materials[i];
#if defined(GPAC_FIXED_POINT)
			Float _rgba[4];
			_rgba[0] = FIX2FLT(rgba[0]);
			_rgba[1] = FIX2FLT(rgba[1]);
			_rgba[2] = FIX2FLT(rgba[2]);
			_rgba[3] = FIX2FLT(rgba[3]);
#elif defined(GPAC_USE_GLES1X)
			Fixed *_rgba = (Fixed *) rgba;
#else
			Float *_rgba = (Float *) rgba;
#endif
			switch (i) {
			case 0:
				loc = glGetUniformLocation(visual->glsl_program, "gfAmbientColor");
				break;
			case 1:
				loc = glGetUniformLocation(visual->glsl_program, "gfDiffuseColor");
				break;
			case 2:
				loc = glGetUniformLocation(visual->glsl_program, "gfSpecularColor");
				break;
			case 3:
				loc = glGetUniformLocation(visual->glsl_program, "gfEmissionColor");
				break;
			}

			if (loc>=0)
				glUniform4fv(loc, 1, _rgba);
		}
		//TO CHECK:  if this does not work as it is supposed to, try: visual->shininess * 128
		loc = glGetUniformLocation(visual->glsl_program, "gfShininess");
		if (loc>=0)
			glUniform1f(loc, FIX2FLT(visual->shininess));

		glDisable(GL_CULL_FACE);	//Enable for performance; if so, check glFrontFace()
	}

	//setup mesh color vertex attribute - only available for some shaders
	if (!tr_state->mesh_num_textures && (mesh->flags & MESH_HAS_COLOR) && !is_debug_bounds) {
		loc_color_array = glGetAttribLocation(visual->glsl_program, "gfMeshColor");
		if (loc_color_array >= 0) {

			//for now colors are 8bit/comp RGB(A), so used GL_UNSIGNED_BYTE and GL_TRUE for normalizing values
			if (mesh->flags & MESH_HAS_ALPHA) {
				glEnable(GL_BLEND);
				tr_state->mesh_is_transparent = 1;
				glVertexAttribPointer(loc_color_array, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GF_Vertex), ((char *)vertex_buffer_address + MESH_COLOR_OFFSET));
			} else {
				glVertexAttribPointer(loc_color_array, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GF_Vertex), ((char *)vertex_buffer_address + MESH_COLOR_OFFSET));
			}
			glEnableVertexAttribArray(loc_color_array);
		}
		GL_CHECK_ERR()
	}

	if (flags & GF_GL_HAS_LIGHT) {
		visual_3d_set_fog_shaders(visual);
	}

	//setup mesh normal vertex attribute - only available for some shaders
	if (!visual->has_material_2d && num_lights && (mesh->mesh_type==MESH_TRIANGLES) ) {
		GF_Matrix normal_mx;
		assert(flags & GF_GL_HAS_LIGHT);

		gf_mx_copy(normal_mx, tr_state->camera->modelview);
		gf_mx_add_matrix(&normal_mx, &tr_state->model_matrix);
		normal_mx.m[12] = normal_mx.m[13] = normal_mx.m[14] = 0;
		gf_mx_inverse(&normal_mx);


		gf_mx_transpose(&normal_mx);

		visual_3d_load_matrix_shaders(tr_state->visual->glsl_program, (Fixed *) &normal_mx.m, "gfNormalMatrix");

		loc_normal_array = glGetAttribLocation(visual->glsl_program, "gfNormal");
		if (loc_normal_array>=0) {
#ifdef MESH_USE_FIXED_NORMAL
			glVertexAttribPointer(loc_normal_array, 3, GL_FLOAT, GL_FALSE, sizeof(GF_Vertex),  ((char *)vertex_buffer_address + MESH_NORMAL_OFFSET) );
#else
			glVertexAttribPointer(loc_normal_array, 3, GL_BYTE, GL_FALSE, sizeof(GF_Vertex),  ((char *)vertex_buffer_address + MESH_NORMAL_OFFSET) );
#endif
			glEnableVertexAttribArray(loc_normal_array);
		}
		GL_CHECK_ERR()

		loc = glGetUniformLocation(visual->glsl_program, "gfNumLights");
		if (loc>=0)
			glUniform1i(loc, num_lights);
		visual_3d_set_lights_shaders(tr_state);
		GL_CHECK_ERR()
	}

	//setup mesh normal vertex attribute - only available for some shaders
	if (tr_state->mesh_num_textures && (mesh->mesh_type==MESH_TRIANGLES) && !(mesh->flags & MESH_NO_TEXTURE)) {
		if (visual->has_tx_matrix) {
			//parsing texture matrix
			loc = glGetUniformLocation(visual->glsl_program, "gfTextureMatrix");
			if (loc>=0)
				glUniformMatrix4fv(loc, 1, GL_FALSE, visual->tx_matrix.m);
			GL_CHECK_ERR()

			loc = glGetUniformLocation(visual->glsl_program, "hasTextureMatrix");
			if (loc>=0) glUniform1i(loc, 1);
		} else {
			loc = glGetUniformLocation(visual->glsl_program, "hasTextureMatrix");
			if (loc>=0) glUniform1i(loc, 0);
		}

		//parsing texture coordinates
		loc_textcoord_array = glGetAttribLocation(visual->glsl_program, "gfMultiTexCoord");
		if (loc_textcoord_array>=0) {
			glVertexAttribPointer(loc_textcoord_array, 2, GL_FLOAT, GL_FALSE, sizeof(GF_Vertex), ((char *)vertex_buffer_address + MESH_TEX_OFFSET));
			glEnableVertexAttribArray(loc_textcoord_array);
			GL_CHECK_ERR()
		}

		if (flags & GF_GL_IS_YUV) {
			loc = glGetUniformLocation(visual->glsl_program, "yuvPixelFormat");
			if (loc>=0) {
				int yuv_mode = 0;
				switch (visual->yuv_pixelformat_type) {
				case GF_PIXEL_NV21:
					yuv_mode = 1;
					break;
				case GF_PIXEL_NV12:
					yuv_mode = 2;
					break;
				}
			
				glUniform1i(loc, yuv_mode);
			}
			GL_CHECK_ERR()
		}
	}

	//
	if (mesh->mesh_type != MESH_TRIANGLES) {
		//According to the spec we should pass a 0,0,1 Normal and disable lights. we just disable lights
		if(flags & GF_GL_HAS_LIGHT) {
			loc = glGetUniformLocation(visual->glsl_program, "gfNumLights");
			if (loc>=0)	glUniform1i(loc, 0);

		}
		glDisable(GL_CULL_FACE);

#if !defined(GPAC_USE_TINYGL) && !defined(GL_ES_CL_PROFILE)
		glLineWidth(1.0f);
#endif

	} else {
		if (visual->compositor->bcull
		        && (!tr_state->mesh_is_transparent || (visual->compositor->bcull ==GF_BACK_CULL_ALPHA) )
		        && (mesh->flags & MESH_IS_SOLID)) {
			glEnable(GL_CULL_FACE);
			if (tr_state->reverse_backface) {
				glFrontFace((mesh->flags & MESH_IS_CW) ? GL_CCW : GL_CW);
			} else {
				glFrontFace((mesh->flags & MESH_IS_CW) ? GL_CW : GL_CCW);
			}
		} else {
			glDisable(GL_CULL_FACE);
		}
	}

	GL_CHECK_ERR()

	//We have a Colour Matrix to be applied
	if(!tr_state->color_mat.identity) {
		GF_Matrix toBeParsed;	//4x4 RGBA Color Matrix
		Fixed translateV[4];	//Vec4 holding translation property of color_mat
		int row,col;

		gf_mx_init(toBeParsed);

		//Copy values from Color Matrix
		for(row=0; row<4; row++) {
			for(col=0; col<4; col++) {
				toBeParsed.m[col+(row*4)]=tr_state->color_mat.m[col+(row*5)];
			}
			translateV[row] = tr_state->color_mat.m[4+(row*5)];
		}

		//the rest of the values form the translation vector
		loc = glGetUniformLocation(visual->glsl_program, "gfTranslationVector");
		if (loc>=0)
			glUniform4fv(loc, 1, (GLfloat *) &translateV);
		GL_CHECK_ERR()

		loc = glGetUniformLocation(visual->glsl_program, "hasColorMatrix");
		if(loc>=0) glUniform1i(loc, 1);

		loc = glGetUniformLocation(visual->glsl_program, "gfColorMatrix");
		if (loc>=0)
			glUniformMatrix4fv(loc, 1, GL_FALSE, toBeParsed.m);
		GL_CHECK_ERR()
	} else {
		loc = glGetUniformLocation(visual->glsl_program, "hasColorMatrix");
		if(loc>=0) glUniform1i(loc, 0);
	}


	//We have a Colour Key to be applied
	if(tr_state->col_key) {

		Float vals[3];
		Float eightbit = 255;	//used for mapping values between 0.0 and 1.0

		glEnable(GL_BLEND);		//normally we shouldn have to need this, but we do

		loc = glGetUniformLocation(visual->glsl_program, "hasColorKey");
		if(loc>=0) glUniform1i(loc, 1);

		vals[0] = tr_state->col_key->r/eightbit;
		vals[1] = tr_state->col_key->g/eightbit;
		vals[2] = tr_state->col_key->b/eightbit;

		loc = glGetUniformLocation(visual->glsl_program, "gfKeyColor");
		if(loc>=0)glUniform3fv(loc, 1, vals);

		loc = glGetUniformLocation(visual->glsl_program, "gfKeyLow");
		if(loc>=0) glUniform1f(loc, tr_state->col_key->low/eightbit);

		loc = glGetUniformLocation(visual->glsl_program, "gfKeyHigh");
		if(loc>=0) glUniform1f(loc, tr_state->col_key->high/eightbit);

		loc = glGetUniformLocation(visual->glsl_program, "gfKeyAlpha");
		if(loc>=0) glUniform1f(loc, tr_state->col_key->alpha/eightbit);

	} else {
		loc = glGetUniformLocation(visual->glsl_program, "hasColorKey");
		if(loc>=0) glUniform1i(loc, 0);
	}




	visual_3d_do_draw_mesh(tr_state, mesh);

	GL_CHECK_ERR()
	//We drawn, now we Reset

	if (mesh->vbo)
		glBindBuffer(GL_ARRAY_BUFFER, 0);

	if (loc_vertex_array>=0) glDisableVertexAttribArray(loc_vertex_array);
	if (loc_color_array>=0) glDisableVertexAttribArray(loc_color_array);
	if (loc_normal_array>=0) glDisableVertexAttribArray(loc_normal_array);
	if (loc_textcoord_array>=0) glDisableVertexAttribArray(loc_textcoord_array);

	//instead of visual_3d_reset_lights(visual);
	if(root_visual->active_glsl_flags & GF_GL_HAS_LIGHT) {
		loc = glGetUniformLocation(visual->glsl_program, "gfNumLights");
		if (loc>=0)	glUniform1i(loc, 0);
		GL_CHECK_ERR()
	}

	if (visual->has_clipper_2d) {
		glDisable(GL_SCISSOR_TEST);
	}

	visual->has_material_2d = GF_FALSE;
	visual->active_glsl_flags = root_visual->active_glsl_flags;
	root_visual->active_glsl_flags &= ~ GF_GL_HAS_COLOR;
	visual->has_material = 0;
	visual->state_color_on = 0;
	if (tr_state->mesh_is_transparent) glDisable(GL_BLEND);
	tr_state->mesh_is_transparent = 0;
	GL_CHECK_ERR()
	glUseProgram(0);
	GL_CHECK_ERR()
}

#endif // !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

//#endif //GPAC_USE_GLES2


static void visual_3d_draw_mesh(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
#ifndef GPAC_USE_GLES2
	GF_Compositor *compositor = tr_state->visual->compositor;
	GF_VisualManager *visual = tr_state->visual;
	Bool has_col, has_tx, has_norm;
	void *base_address = NULL;
	Bool is_debug_bounds = GF_FALSE;

#if defined(GPAC_FIXED_POINT) && !defined(GPAC_USE_GLES1X)
	Float *color_array = NULL;
	Float fix_scale = 1.0f;
	fix_scale /= FIX_ONE;
#endif

#endif

	if (mesh) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[V3D] Drawing mesh %p\n", mesh));
	}

	//clear error
	glGetError();
	GL_CHECK_ERR()

#ifdef GPAC_USE_GLES2
	visual_3d_draw_mesh_shader_only(tr_state, mesh);
	return;
#else

#if !defined(GPAC_CONFIG_ANDROID) && !defined(GPAC_CONFIG_IOS) && !defined(GPAC_FIXED_POINT)
	if (!visual->compositor->shader_mode_disabled) {
		visual_3d_draw_mesh_shader_only(tr_state, mesh);
		return;
	}
#endif

	if (!mesh) {
		mesh = tr_state->visual->compositor->unit_bbox;
		is_debug_bounds = GF_TRUE;
	}

	if (! visual_3d_bind_buffer(compositor, mesh, &base_address)) {
#if! defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL)
		glUseProgram(0);
#endif
		return;
	}
	has_col = has_tx = has_norm = 0;

	if (!is_debug_bounds) {
		//set lights before pushing modelview matrix
		visual_3d_set_lights(visual);
	}

	visual_3d_update_matrices(tr_state);

	/*enable states*/
	if (!is_debug_bounds) {
		if (visual->has_fog) visual_3d_enable_fog(visual);

		if (visual->state_color_on) glEnable(GL_COLOR_MATERIAL);
		else glDisable(GL_COLOR_MATERIAL);

		if (visual->state_blend_on) glEnable(GL_BLEND);
	}


	//setup scissor
	visual_3d_set_clipper_scissor(visual, tr_state);

	visual_3d_set_clippers(visual, tr_state);

	glEnableClientState(GL_VERTEX_ARRAY);
#if defined(GPAC_USE_GLES1X)
	glVertexPointer(3, GL_FIXED, sizeof(GF_Vertex),  base_address);
#elif defined(GPAC_FIXED_POINT)
	/*scale modelview matrix*/
	glPushMatrix();
	glScalef(fix_scale, fix_scale, fix_scale);
	glVertexPointer(3, GL_INT, sizeof(GF_Vertex), base_address);
#else
	glVertexPointer(3, GL_FLOAT, sizeof(GF_Vertex), base_address);
#endif


	/*
	*	Enable colors:
	if mat2d is set, use mat2d and no lighting
	*/
	if (visual->has_material_2d) {
		glDisable(GL_LIGHTING);
		if (visual->mat_2d.alpha != FIX_ONE) {
			glEnable(GL_BLEND);
			visual_3d_enable_antialias(visual, 0);
		} else {
			//disable blending only if no texture !
			if (!tr_state->mesh_num_textures)
				glDisable(GL_BLEND);
			visual_3d_enable_antialias(visual, visual->compositor->aa ? 1 : 0);
		}
#ifdef GPAC_USE_GLES1X
		glColor4x( FIX2INT(visual->mat_2d.red * 255), FIX2INT(visual->mat_2d.green * 255), FIX2INT(visual->mat_2d.blue * 255), FIX2INT(visual->mat_2d.alpha * 255));
#elif defined(GPAC_FIXED_POINT)
		glColor4f(FIX2FLT(visual->mat_2d.red), FIX2FLT(visual->mat_2d.green), FIX2FLT(visual->mat_2d.blue), FIX2FLT(visual->mat_2d.alpha));
#else
		glColor4f(visual->mat_2d.red, visual->mat_2d.green, visual->mat_2d.blue, visual->mat_2d.alpha);
#endif
	}

	//setup material color
	if (visual->has_material) {
		u32 i;
		GL_CHECK_ERR()
		for (i=0; i<4; i++) {
			GLenum mode;
			Fixed *rgba = (Fixed *) & visual->materials[i];
#if defined(GPAC_USE_GLES1X)
			Fixed *_rgba = (Fixed *) rgba;
#elif defined(GPAC_FIXED_POINT)
			Float _rgba[4];
			_rgba[0] = FIX2FLT(rgba[0]);
			_rgba[1] = FIX2FLT(rgba[1]);
			_rgba[2] = FIX2FLT(rgba[2]);
			_rgba[3] = FIX2FLT(rgba[3]);
#else
			Float *_rgba = (Float *) rgba;
#endif

			switch (i) {
			case 0:
				mode = GL_AMBIENT;
				break;
			case 1:
				mode = GL_DIFFUSE;
				break;
			case 2:
				mode = GL_SPECULAR;
				break;
			default:
				mode = GL_EMISSION;
				break;
			}

#if defined(GPAC_USE_GLES1X) && defined(GPAC_FIXED_POINT)
			glMaterialxv(GL_FRONT_AND_BACK, mode, _rgba);
#else
			glMaterialfv(GL_FRONT_AND_BACK, mode, _rgba);
#endif
			GL_CHECK_ERR()
		}
#ifdef GPAC_USE_GLES1X
		glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, visual->shininess * 128);
#else
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, FIX2FLT(visual->shininess) * 128);
#endif
		GL_CHECK_ERR()
	}

	//otherwise setup mesh color
	if (!tr_state->mesh_num_textures && (mesh->flags & MESH_HAS_COLOR)) {
		glEnable(GL_COLOR_MATERIAL);
#if !defined (GPAC_USE_GLES1X)
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
#endif
		glEnableClientState(GL_COLOR_ARRAY);
		has_col = 1;

#if defined (GPAC_USE_GLES1X)

		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			tr_state->mesh_is_transparent = 1;
		}
#ifdef MESH_USE_SFCOLOR
		/*glES only accepts full RGBA colors*/
		glColorPointer(4, GL_FIXED, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
#else
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
#endif	/*MESH_USE_SFCOLOR*/

#elif defined (GPAC_FIXED_POINT)


#ifdef MESH_USE_SFCOLOR
		/*this is a real pain: we cannot "scale" colors through OpenGL, and our components are 16.16 (32 bytes) ranging
		from [0 to 65536] mapping to [0, 1.0], but OpenGL assumes for s32 a range from [-2^31 2^31] mapping to [0, 1.0]
		we must thus rebuild a dedicated array...*/
		if (mesh->flags & MESH_HAS_ALPHA) {
			u32 i;
			color_array = gf_malloc(sizeof(Float)*4*mesh->v_count);
			for (i=0; i<mesh->v_count; i++) {
				color_array[4*i] = FIX2FLT(mesh->vertices[i].color.red);
				color_array[4*i+1] = FIX2FLT(mesh->vertices[i].color.green);
				color_array[4*i+2] = FIX2FLT(mesh->vertices[i].color.blue);
				color_array[4*i+3] = FIX2FLT(mesh->vertices[i].color.alpha);
			}
			glEnable(GL_BLEND);
			glColorPointer(4, GL_FLOAT, 4*sizeof(Float), color_array);
			tr_state->mesh_is_transparent = 1;
		} else {
			color_array = gf_malloc(sizeof(Float)*3*mesh->v_count);
			for (i=0; i<mesh->v_count; i++) {
				color_array[3*i] = FIX2FLT(mesh->vertices[i].color.red);
				color_array[3*i+1] = FIX2FLT(mesh->vertices[i].color.green);
				color_array[3*i+2] = FIX2FLT(mesh->vertices[i].color.blue);
			}
			glColorPointer(3, GL_FLOAT, 3*sizeof(Float), color_array);
		}
#else
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
#endif /*MESH_USE_SFCOLOR*/

#else

#ifdef MESH_USE_SFCOLOR
		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			glColorPointer(4, GL_FLOAT, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
			tr_state->mesh_is_transparent = 1;
		} else {
			glColorPointer(3, GL_FLOAT, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
		}
#else
		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			tr_state->mesh_is_transparent = 1;
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
		} else {
			glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), ((char *)base_address + MESH_COLOR_OFFSET));
		}
#endif /*MESH_USE_SFCOLOR*/

#endif
	}

	if (tr_state->mesh_num_textures && (mesh->mesh_type==MESH_TRIANGLES) && !(mesh->flags & MESH_NO_TEXTURE)) {
		has_tx = 1;

		glMatrixMode(GL_TEXTURE);
		if (visual->has_tx_matrix) {
			visual_3d_matrix_load(visual, visual->tx_matrix.m);
		} else {
			glLoadIdentity();
		}
		glMatrixMode(GL_MODELVIEW);


#if defined(GPAC_USE_GLES1X)
		glTexCoordPointer(2, GL_FIXED, sizeof(GF_Vertex), ((char *)base_address + MESH_TEX_OFFSET));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY );
#elif defined(GPAC_FIXED_POINT)
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glScalef(fix_scale, fix_scale, fix_scale);
		glMatrixMode(GL_MODELVIEW);
		glTexCoordPointer(2, GL_INT, sizeof(GF_Vertex), ((char *)base_address + MESH_TEX_OFFSET));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY );
#else

#ifndef GPAC_USE_TINYGL
		if (tr_state->mesh_num_textures>1) {
			u32 i;
			for (i=0; i<tr_state->mesh_num_textures; i++) {
				glClientActiveTexture(GL_TEXTURE0 + i);
				glTexCoordPointer(2, GL_FLOAT, sizeof(GF_Vertex), ((char *)base_address + MESH_TEX_OFFSET));
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		} else
#endif //GPAC_USE_TINYGL
		{
			glTexCoordPointer(2, GL_FLOAT, sizeof(GF_Vertex), ((char *)base_address + MESH_TEX_OFFSET));
			glEnableClientState(GL_TEXTURE_COORD_ARRAY );
		}
#endif
	}

	if (mesh->mesh_type != MESH_TRIANGLES) {
#ifdef GPAC_USE_GLES1X
		glNormal3x(0, 0, FIX_ONE);
#else
		glNormal3f(0, 0, 1.0f);
#endif
		glDisable(GL_CULL_FACE);
		glDisable(GL_LIGHTING);
		if (mesh->mesh_type==MESH_LINESET) glDisable(GL_LINE_SMOOTH);
		else glDisable(GL_POINT_SMOOTH);

#if !defined(GPAC_USE_TINYGL) && !defined(GL_ES_CL_PROFILE)
		glLineWidth(1.0f);
#endif

	} else {
		u32 normal_type = GL_FLOAT;
		has_norm = 1;
		glEnableClientState(GL_NORMAL_ARRAY );
#ifdef MESH_USE_FIXED_NORMAL

#if defined(GPAC_USE_GLES1X)
		normal_type = GL_FIXED;
#elif defined(GPAC_FIXED_POINT)
		normal_type = GL_INT;
#else
		normal_type = GL_FLOAT;
#endif

#else /*MESH_USE_FIXED_NORMAL*/
		/*normals are stored on signed bytes*/
		normal_type = GL_BYTE;
#endif
		glNormalPointer(normal_type, sizeof(GF_Vertex), ((char *)base_address + MESH_NORMAL_OFFSET));

		if (mesh->mesh_type==MESH_TRIANGLES) {
			if (compositor->bcull
			        && (!tr_state->mesh_is_transparent || (compositor->bcull ==GF_BACK_CULL_ALPHA) )
			        && (mesh->flags & MESH_IS_SOLID)) {
				glEnable(GL_CULL_FACE);
				glFrontFace((mesh->flags & MESH_IS_CW) ? GL_CW : GL_CCW);
			} else {
				glDisable(GL_CULL_FACE);
			}
		}
	}

	GL_CHECK_ERR()
	visual_3d_do_draw_mesh(tr_state, mesh);
	GL_CHECK_ERR()

	glDisableClientState(GL_VERTEX_ARRAY);
	if (has_col) glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_COLOR_MATERIAL);

	if (has_tx) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (has_norm) glDisableClientState(GL_NORMAL_ARRAY);

	if (mesh->vbo)
		glBindBuffer(GL_ARRAY_BUFFER, 0);

#if defined(GPAC_FIXED_POINT) && !defined(GPAC_USE_GLES1X)
	if (color_array) gf_free(color_array);
	if (tr_state->mesh_num_textures && (mesh->mesh_type==MESH_TRIANGLES) && !(mesh->flags & MESH_NO_TEXTURE)) {
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}
	glPopMatrix();
#endif


	if (visual->has_clipper_2d) {
		glDisable(GL_SCISSOR_TEST);
	}
	visual_3d_reset_lights(visual);

	glDisable(GL_COLOR_MATERIAL);

	//reset all our states
	visual_3d_reset_clippers(visual);
	visual->has_material_2d = GF_FALSE;
	visual->has_material = 0;
	visual->state_color_on = 0;
	if (tr_state->mesh_is_transparent) glDisable(GL_BLEND);
	tr_state->mesh_is_transparent = 0;

	GL_CHECK_ERR()
#endif
}

static void visual_3d_set_debug_color(u32 col)
{
#ifndef GPAC_USE_GLES2

#ifdef GPAC_USE_GLES1X
	glColor4x( (col ? GF_COL_R(col) : 255) , (col ? GF_COL_G(col) : 0) , (col ? GF_COL_B(col) : 255), 255);
#else
	glColor4f(col ? GF_COL_R(col)/255.0f : 1, col ? GF_COL_G(col)/255.0f : 0, col ? GF_COL_B(col)/255.0f : 1, 1);
#endif


#endif //GPAC_USE_GLES2
}


/*note we don't perform any culling for normal drawing...*/
static void visual_3d_draw_normals(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
#if !defined( GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES2)

	GF_Vec pt, end;
	u32 i, j;
	Fixed scale = mesh->bounds.radius / 4;

#ifdef GPAC_USE_GLES1X
	GF_Vec va[2];
	u16 indices[2];
	glEnableClientState(GL_VERTEX_ARRAY);
#endif

	visual_3d_set_debug_color(0);

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	//in shader mode force pushing projection and modelview using fixed pipeline API
	if (!tr_state->visual->compositor->shader_mode_disabled) {
		tr_state->visual->needs_projection_matrix_reload=GF_TRUE;
		visual_3d_update_matrices(tr_state);
	}
#endif

	if (tr_state->visual->compositor->norms==GF_NORMALS_VERTEX) {
		IDX_TYPE *idx = mesh->indices;
		for (i=0; i<mesh->i_count; i+=3) {
			for (j=0; j<3; j++) {
				pt = mesh->vertices[idx[j]].pos;
				MESH_GET_NORMAL(end, mesh->vertices[idx[j]]);
				end = gf_vec_scale(end, scale);
				gf_vec_add(end, pt, end);
#ifdef GPAC_USE_GLES1X
				va[0] = pt;
				va[1] = end;
				indices[0] = 0;
				indices[1] = 1;
				glVertexPointer(3, GL_FIXED, 0, va);
				glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, indices);
#else
				glBegin(GL_LINES);
				glVertex3f(FIX2FLT(pt.x), FIX2FLT(pt.y), FIX2FLT(pt.z));
				glVertex3f(FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z));
				glEnd();
#endif
			}
			idx+=3;
		}
	} else {
		IDX_TYPE *idx = mesh->indices;
		for (i=0; i<mesh->i_count; i+=3) {
			gf_vec_add(pt, mesh->vertices[idx[0]].pos, mesh->vertices[idx[1]].pos);
			gf_vec_add(pt, pt, mesh->vertices[idx[2]].pos);
			pt = gf_vec_scale(pt, FIX_ONE/3);
			MESH_GET_NORMAL(end, mesh->vertices[idx[0]]);
			end = gf_vec_scale(end, scale);
			gf_vec_add(end, pt, end);

#ifdef GPAC_USE_GLES1X
			va[0] = pt;
			va[1] = end;
			indices[0] = 0;
			indices[1] = 1;
			glVertexPointer(3, GL_FIXED, 0, va);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, indices);
#else
			glBegin(GL_LINES);
			glVertex3f(FIX2FLT(pt.x), FIX2FLT(pt.y), FIX2FLT(pt.z));
			glVertex3f(FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z));
			glEnd();
#endif
			idx += 3;
		}
	}
#ifdef GPAC_USE_GLES1X
	glDisableClientState(GL_VERTEX_ARRAY);
#endif

#endif	/*GPAC_USE_TINYGL*/
}


void visual_3d_draw_aabb_nodeBounds(GF_TraverseState *tr_state, AABBNode *node)
{
	if (node->pos) {
		visual_3d_draw_aabb_nodeBounds(tr_state, node->pos);
		visual_3d_draw_aabb_nodeBounds(tr_state, node->neg);
	} else {
		GF_Matrix mx;
		SFVec3f c, s;
		gf_vec_diff(s, node->max, node->min);
		c = gf_vec_scale(s, FIX_ONE/2);
		gf_vec_add(c, node->min, c);

		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_add_translation(&tr_state->model_matrix, c.x, c.y, c.z);
		gf_mx_add_scale(&tr_state->model_matrix, s.x, s.y, s.z);

		visual_3d_draw_mesh(tr_state, NULL);

		gf_mx_copy(tr_state->model_matrix, mx);
	}
}

void visual_3d_draw_bbox(GF_TraverseState *tr_state, GF_BBox *box, Bool is_debug)
{
	GF_Matrix mx;
	SFVec3f c, s;

	if (! is_debug) {
		visual_3d_set_debug_color(tr_state->visual->compositor->hlline);
	}

	gf_vec_diff(s, box->max_edge, box->min_edge);
	c.x = box->min_edge.x + s.x/2;
	c.y = box->min_edge.y + s.y/2;
	c.z = box->min_edge.z + s.z/2;

	gf_mx_copy(mx, tr_state->model_matrix);
	gf_mx_add_translation(&tr_state->model_matrix, c.x, c.y, c.z);
	gf_mx_add_scale(&tr_state->model_matrix, s.x, s.y, s.z);

	visual_3d_draw_mesh(tr_state, NULL);
	gf_mx_copy(tr_state->model_matrix, mx);
}

static void visual_3d_draw_bounds(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	visual_3d_set_debug_color(0);

	if (mesh->aabb_root && (tr_state->visual->compositor->bvol==GF_BOUNDS_AABB)) {
		visual_3d_draw_aabb_nodeBounds(tr_state, mesh->aabb_root);
	} else {
		visual_3d_draw_bbox(tr_state, &mesh->bounds, GF_TRUE);
	}
}

void visual_3d_mesh_paint(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
#if !defined(GPAC_USE_GLES2)
	Bool mesh_drawn = 0;
#endif

	GL_CHECK_ERR()

	gf_rmt_begin_gl(visual_3d_mesh_paint);
	glGetError();

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[V3D] Drawing mesh %p\n", mesh));
	if (tr_state->visual->compositor->wire != GF_WIREFRAME_ONLY) {
		visual_3d_draw_mesh(tr_state, mesh);
#if !defined(GPAC_USE_GLES2)
		mesh_drawn = 1;
#endif
	}

#if !defined(GPAC_USE_GLES2)
	if (tr_state->visual->compositor->norms) {
		if (!mesh_drawn) {
			visual_3d_update_matrices(tr_state);
			mesh_drawn=1;
		}
		visual_3d_draw_normals(tr_state, mesh);
	}

	if ((mesh->mesh_type==MESH_TRIANGLES) && (tr_state->visual->compositor->wire != GF_WIREFRAME_NONE)) {
		glDisable(GL_LIGHTING);
		visual_3d_set_debug_color(0xFFFFFFFF);

		if (!mesh_drawn)
			visual_3d_update_matrices(tr_state);


		glEnableClientState(GL_VERTEX_ARRAY);
#ifdef GPAC_USE_GLES1X
		glVertexPointer(3, GL_FIXED, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
		glDrawElements(GL_LINES, mesh->i_count, GL_UNSIGNED_SHORT, mesh->indices);
#else
		glVertexPointer(3, GL_FLOAT, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
		glDrawElements(GL_LINES, mesh->i_count, GL_UNSIGNED_INT, mesh->indices);
#endif
		glDisableClientState(GL_VERTEX_ARRAY);
	}

#endif

	if (tr_state->visual->compositor->bvol)
		visual_3d_draw_bounds(tr_state, mesh);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[V3D] Done drawing mesh %p\n", mesh));

	gf_rmt_end_gl();
	glGetError();
}

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES2)


static GLubyte hatch_horiz[] = {
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};

static GLubyte hatch_vert[] = {
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
};

static GLubyte hatch_up[] = {
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03
};

static GLubyte hatch_down[] = {
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0
};

static GLubyte hatch_cross[] = {
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c
};

void visual_3d_mesh_hatch(GF_TraverseState *tr_state, GF_Mesh *mesh, u32 hatchStyle, SFColor hatchColor)
{
	if (mesh->mesh_type != MESH_TRIANGLES) return;

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
	if (mesh->flags & MESH_IS_2D) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glNormal3f(0, 0, 1.0f);
		glDisable(GL_CULL_FACE);
	} else {
		glEnableClientState(GL_NORMAL_ARRAY );
		glNormalPointer(GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].normal);

		/*if mesh is transparent DON'T CULL*/
		if (!tr_state->mesh_is_transparent && (mesh->flags & MESH_IS_SOLID)) {
			glEnable(GL_CULL_FACE);
			glFrontFace((mesh->flags & MESH_IS_CW) ? GL_CW : GL_CCW);
		} else {
			glDisable(GL_CULL_FACE);
		}
	}

	glEnable(GL_POLYGON_STIPPLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	/*can't access ISO International Register of Graphical Items www site :)*/
	switch (hatchStyle) {
	case 5:
		glPolygonStipple(hatch_cross);
		break;
	case 4:
		glPolygonStipple(hatch_up);
		break;
	case 3:
		glPolygonStipple(hatch_down);
		break;
	case 2:
		glPolygonStipple(hatch_vert);
		break;
	case 1:
		glPolygonStipple(hatch_horiz);
		break;
	default:
		glDisable(GL_POLYGON_STIPPLE);
		break;
	}
	glColor3f(FIX2FLT(hatchColor.red), FIX2FLT(hatchColor.green), FIX2FLT(hatchColor.blue));
	glDrawElements(GL_TRIANGLES, mesh->i_count, GL_UNSIGNED_INT, mesh->indices);

	glDisable(GL_POLYGON_STIPPLE);
}
#endif

/*only used for ILS/ILS2D or IFS2D outline*/
void visual_3d_mesh_strike(GF_TraverseState *tr_state, GF_Mesh *mesh, Fixed width, Fixed line_scale, u32 dash_style)
{
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES2)
	u16 style;
#endif

	if (mesh->mesh_type != MESH_LINESET) return;
	if (line_scale) width = gf_mulfix(width, line_scale);
	width/=2;
#if !defined(GPAC_USE_TINYGL) && !defined(GL_ES_CL_PROFILE)
	glLineWidth( FIX2FLT(width));
#endif

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES2)

	switch (dash_style) {
	case GF_DASH_STYLE_DASH:
		style = 0x1F1F;
		break;
	case GF_DASH_STYLE_DOT:
		style = 0x3333;
		break;
	case GF_DASH_STYLE_DASH_DOT:
		style = 0x6767;
		break;
	case GF_DASH_STYLE_DASH_DASH_DOT:
		style = 0x33CF;
		break;
	case GF_DASH_STYLE_DASH_DOT_DOT:
		style = 0x330F;
		break;
	default:
		style = 0;
		break;
	}
	if (style) {
		u32 factor = FIX2INT(width);
		if (!factor) factor = 1;
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(factor, style);
		visual_3d_mesh_paint(tr_state, mesh);
		glDisable (GL_LINE_STIPPLE);
	} else
#endif
		visual_3d_mesh_paint(tr_state, mesh);
}


void visual_3d_clear(GF_VisualManager *visual, SFColor color, Fixed alpha)
{
#ifdef GPAC_USE_GLES1X
	glClearColorx(color.red, color.green, color.blue, alpha);
#else
	glClearColor(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue), FIX2FLT(alpha));
#endif
	glClear(GL_COLOR_BUFFER_BIT);
}


void visual_3d_fill_rect(GF_VisualManager *visual, GF_Rect rc, SFColorRGBA color)
{
	//TODOk - code this for GLES2 ?
#ifdef GPAC_USE_GLES2
#else

	glDisable(GL_BLEND | GL_LIGHTING | GL_TEXTURE_2D);

#if defined(GPAC_USE_GLES1X)
	glNormal3x(0, 0, FIX_ONE);
	if (color.alpha!=FIX_ONE) glEnable(GL_BLEND);
	glColor4x(color.red, color.green, color.blue, color.alpha);
	{
		Fixed v[8];
		u16 indices[3];
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;

		v[0] = rc.x;
		v[1] = rc.y;
		v[2] = rc.x+rc.width;
		v[3] = rc.y-rc.height;
		v[4] = rc.x+rc.width;
		v[5] = rc.y;
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FIXED, 0, v);
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

		v[4] = rc.x;
		v[5] = rc.y-rc.height;
		glVertexPointer(2, GL_FIXED, 0, v);
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

		glDisableClientState(GL_VERTEX_ARRAY);
	}
#else
	glNormal3f(0, 0, 1);
	if (color.alpha!=FIX_ONE) {
		glEnable(GL_BLEND);
		glColor4f(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue), FIX2FLT(color.alpha));
	} else {
		glColor3f(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue));
	}
	glBegin(GL_QUADS);
	glVertex3f(FIX2FLT(rc.x), FIX2FLT(rc.y), 0);
	glVertex3f(FIX2FLT(rc.x), FIX2FLT(rc.y-rc.height), 0);
	glVertex3f(FIX2FLT(rc.x+rc.width), FIX2FLT(rc.y-rc.height), 0);
	glVertex3f(FIX2FLT(rc.x+rc.width), FIX2FLT(rc.y), 0);
	glEnd();

	glDisable(GL_COLOR_MATERIAL | GL_COLOR_MATERIAL_FACE);
#endif

	glDisable(GL_BLEND);
#endif
}

//note that we always use glReadPixel even if the actual rendering target is a texture
//this is the recommended way by khronos any way
GF_Err compositor_3d_get_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *fb, u32 depth_dump_mode)
{
	u32 i;

	fb->width = compositor->display_width;
	fb->height = compositor->display_height;

	/*depthmap-only dump*/
	if (depth_dump_mode==1) {
		//depth reading not supported on gles <= 1.1
#ifdef GPAC_USE_GLES1X
		return GF_NOT_SUPPORTED;
#else
		Float *depthp;
		Float zFar, zNear;

		fb->pitch_x = 0;
		fb->pitch_y = compositor->vp_width;

		if (compositor->screen_buffer_alloc_size < fb->pitch_y * fb->height) {
			compositor->screen_buffer_alloc_size = fb->pitch_y * fb->height;
			compositor->screen_buffer = gf_realloc(compositor->screen_buffer, compositor->screen_buffer_alloc_size);
		}

		fb->video_buffer = compositor->screen_buffer;

		//read as float
		depthp = (Float*)gf_malloc(sizeof(Float)* fb->pitch_y * fb->height);
		fb->pixel_format = GF_PIXEL_GREYSCALE;

#ifndef GPAC_USE_TINYGL
		//glPixelTransferf(GL_DEPTH_SCALE, FIX2FLT(compositor->OGLDepthGain) );
		//glPixelTransferf(GL_DEPTH_BIAS, FIX2FLT(compositor->OGLDepthOffset) );
#endif

		glReadPixels(compositor->vp_x, compositor->vp_y, fb->width, fb->height, GL_DEPTH_COMPONENT, GL_FLOAT, depthp);

		//linearize z buffer, from 0 (zfar) to 1 (znear)
		zFar = FIX2FLT(compositor->visual->camera.z_far);
		zNear = FIX2FLT(compositor->visual->camera.z_near);
		for (i=0; i<fb->height*fb->width; i++) {
			Float res = ( (2.0f * zNear) / (zFar + zNear - depthp[i] * (zFar - zNear)) ) ;
			fb->video_buffer[i] = (u8) ( 255.0 * (1.0 - res));
		}

		gf_free(depthp);

#endif	/*GPAC_USE_GLES1X*/
	}

	/* RGBDS or RGBD dump*/
	else if (depth_dump_mode==2 || depth_dump_mode==3) {
#ifdef GPAC_USE_GLES1X
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor]: RGB+Depth format not implemented in OpenGL ES\n"));
		return GF_NOT_SUPPORTED;
#else
		char *depth_data=NULL;
		u32 size;
		fb->pitch_x = 4;
		fb->pitch_y = compositor->vp_width*4; /* 4 bytes for each rgbds pixel */

#ifndef GPAC_USE_TINYGL
		size = fb->pitch_y * fb->height;
#else
		size = 2 * fb->pitch_y * fb->height;
#endif
		if (compositor->screen_buffer_alloc_size < size) {
			compositor->screen_buffer_alloc_size = size;
			compositor->screen_buffer = gf_realloc(compositor->screen_buffer, compositor->screen_buffer_alloc_size);
		}
		fb->video_buffer = compositor->screen_buffer;

#ifndef GPAC_USE_TINYGL

		glReadPixels(0, 0, fb->width, fb->height, GL_RGBA, GL_UNSIGNED_BYTE, fb->video_buffer);

		/*
		glPixelTransferf(GL_DEPTH_SCALE, FIX2FLT(compositor->OGLDepthGain));
		glPixelTransferf(GL_DEPTH_BIAS, FIX2FLT(compositor->OGLDepthOffset));
		*/

		depth_data = (char*) gf_malloc(sizeof(char)*fb->width*fb->height);
		glReadPixels(0, 0, fb->width, fb->height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, depth_data);

		if (depth_dump_mode==2) {
			fb->pixel_format = GF_PIXEL_RGBDS;

			/*this corresponds to the RGBDS ordering*/
			for (i=0; i<fb->height*fb->width; i++) {
				u8 ds;
				/* erase lowest-weighted depth bit */
				u8 depth = depth_data[i] & 0xfe;
				/*get alpha*/
				ds = (fb->video_buffer[i*4 + 3]);
				/* if heaviest-weighted alpha bit is set (>128) , turn on shape bit*/
				if (ds & 0x80) depth |= 0x01;
				fb->video_buffer[i*4+3] = depth; /*insert depth onto alpha*/
			}
			/*this corresponds to RGBD ordering*/
		} else if (depth_dump_mode==3) {
			fb->pixel_format = GF_PIXEL_RGBD;
			for (i=0; i<fb->height*fb->width; i++)
				fb->video_buffer[i*4+3] = depth_data[i];
		}
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor]: RGB+Depth format not implemented in TinyGL\n"));
		return GF_NOT_SUPPORTED;
#endif

#endif /*GPAC_USE_GLES1X*/
	} else { /*if (compositor->user && (compositor->user->init_flags & GF_VOUT_WINDOW_TRANSPARENT))*/
		u32 size;
		fb->pitch_x = 3;
		if (!compositor->opfmt && compositor->forced_alpha) {
			fb->pitch_x = 4;
		} else if (compositor->opfmt==GF_PIXEL_RGBA) {
			fb->pitch_x = 4;
		}

		fb->pitch_y = fb->pitch_x * fb->width;
		size = fb->pitch_y * fb->height;
		if (compositor->screen_buffer_alloc_size < size) {
			compositor->screen_buffer_alloc_size = size;
			compositor->screen_buffer = gf_realloc(compositor->screen_buffer, compositor->screen_buffer_alloc_size);
			compositor->line_buffer = gf_realloc(compositor->line_buffer, fb->pitch_y);

		}

		fb->video_buffer = compositor->screen_buffer;

		fb->pixel_format = (fb->pitch_x == 4) ? GF_PIXEL_RGBA : GF_PIXEL_RGB;

		if (compositor->fbo_id) compositor_3d_enable_fbo(compositor, GF_TRUE);

		glReadPixels(0, 0, fb->width, fb->height, (fb->pitch_x == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, fb->video_buffer);

		if (compositor->fbo_id) compositor_3d_enable_fbo(compositor, GF_FALSE);

		/*	} else {
				fb->pitch_x = 3;
				fb->pitch_y = 3*compositor->vp_width;
				fb->video_buffer = (char*)gf_malloc(sizeof(char) * fb->pitch_y * fb->height);
				fb->pixel_format = GF_PIXEL_RGB;

				glReadPixels(compositor->vp_x, compositor->vp_y, fb->width, fb->height, GL_RGB, GL_UNSIGNED_BYTE, fb->video_buffer);
		*/
	}

#ifndef GPAC_USE_TINYGL
	/*flip image (OpenGL always handle image data bottom to top) */
	u32 hy = fb->height/2;
	for (i=0; i<hy; i++) {
		memcpy(compositor->line_buffer, fb->video_buffer+ i*fb->pitch_y, fb->pitch_y);
		memcpy(fb->video_buffer + i*fb->pitch_y, fb->video_buffer + (fb->height - 1 - i) * fb->pitch_y, fb->pitch_y);
		memcpy(fb->video_buffer + (fb->height - 1 - i) * fb->pitch_y, compositor->line_buffer, fb->pitch_y);
	}
#endif
	return GF_OK;
}

GF_Err compositor_3d_release_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer)
{
	framebuffer->video_buffer = 0;
	return GF_OK;
}

GF_Err compositor_3d_get_offscreen_buffer(GF_Compositor *compositor, GF_VideoSurface *fb, u32 view_idx, u32 depth_dump_mode)
{
	//TODOk - habdle offscreen buffers through frameBuffer objects, no read back j
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES2)
	char *tmp;
	u32 hy, i;
	/*not implemented yet*/
	if (depth_dump_mode) return GF_NOT_SUPPORTED;

	if (view_idx>=compositor->visual->nb_views) return GF_BAD_PARAM;
	fb->width = compositor->visual->auto_stereo_width;
	fb->height = compositor->visual->auto_stereo_height;
	fb->pixel_format = GF_PIXEL_RGB;
	fb->pitch_y = 3*fb->width;
	fb->video_buffer = gf_malloc(sizeof(char)*3*fb->width*fb->height);
	if (!fb->video_buffer) return GF_OUT_OF_MEM;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, compositor->visual->gl_textures[view_idx]);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, fb->video_buffer);
	glDisable(GL_TEXTURE_2D);

	/*flip image (OpenGL always handle image data bottom to top) */
	tmp = (char*)gf_malloc(sizeof(char)*fb->pitch_y);
	hy = fb->height/2;
	for (i=0; i<hy; i++) {
		memcpy(tmp, fb->video_buffer+ i*fb->pitch_y, fb->pitch_y);
		memcpy(fb->video_buffer + i*fb->pitch_y, fb->video_buffer + (fb->height - 1 - i) * fb->pitch_y, fb->pitch_y);
		memcpy(fb->video_buffer + (fb->height - 1 - i) * fb->pitch_y, tmp, fb->pitch_y);
	}
	gf_free(tmp);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

void visual_3d_point_sprite(GF_VisualManager *visual, Drawable *stack, GF_TextureHandler *txh, GF_TraverseState *tr_state)
{
	//todo - allow point sprites for GLES2 ?
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES2)
	u32 w, h;
	u32 pixel_format, stride;
	u8 *data;
	Float r, g, b;
	Fixed x, y;
	Float inc, scale;
	Bool in_strip;
	GF_Node *txtrans = NULL;

	if ((visual->compositor->depth_gl_type==GF_SC_DEPTH_GL_POINTS) && visual->compositor->gl_caps.point_sprite) {
		Float z;
		static GLfloat none[3] = { 1.0f, 0, 0 };

		data = (u8 *) gf_sc_texture_get_data(txh, &pixel_format);
		if (!data) return;
		if (pixel_format!=GF_PIXEL_RGBD) return;
		stride = txh->stride;
		if (txh->pixelformat==GF_PIXEL_YUVD) stride *= 4;

		glPointSize(1.0f * visual->compositor->zoom);
		glDepthMask(GL_FALSE);

		glPointParameterfv(GL_DISTANCE_ATTENUATION_EXT, none);
		glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE_EXT, 0.0);
		glEnable(GL_POINT_SMOOTH);
		glDisable(GL_LIGHTING);

//		scale = FIX2FLT(visual->compositor->depth_gl_scale);
		inc = 1;
		if (!tr_state->pixel_metrics) inc /= FIX2FLT(tr_state->min_hsize);
//		x = 0;
		y = 1;
		y = gf_mulfix(y, INT2FIX(txh->height/2));
		if (!tr_state->pixel_metrics) y = gf_divfix(y, tr_state->min_hsize);

		glBegin(GL_POINTS);
		for (h=0; h<txh->height; h++) {
			x = -1;
			x = gf_mulfix(x, INT2FIX(txh->width/2));
			if (!tr_state->pixel_metrics) x = gf_divfix(x, tr_state->min_hsize);
			for (w=0; w<txh->width; w++) {
				u8 *p = data + h*stride + w*4;
				r = p[0];
				r /= 255;
				g = p[1];
				g /= 255;
				b = p[2];
				b /= 255;
				z = p[3];
				z = z / 255;

				glColor4f(r, g, b, 1.0);
				glVertex3f(FIX2FLT(x), FIX2FLT(y), FIX2FLT(-z)*60);
				x += FLT2FIX(inc);
			}
			y -= FLT2FIX(inc);
		}
		glEnd();

		glDepthMask(GL_TRUE);
		return;
	}

	if (visual->compositor->depth_gl_type==GF_SC_DEPTH_GL_STRIPS) {
		u32 first_pass;
		Float delta = FIX2FLT(visual->compositor->depth_gl_strips_filter);
		if (!delta) first_pass = 2;
		else first_pass = 1;

		data = (u8 *) gf_sc_texture_get_data(txh, &pixel_format);
		if (!data) return;
		if (pixel_format!=GF_PIXEL_RGBD) return;
		stride = txh->stride;
		if (txh->pixelformat==GF_PIXEL_YUVD) stride *= 4;

		glDepthMask(GL_FALSE);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_POINT_SMOOTH);
		glDisable(GL_FOG);

restart:
		scale = FIX2FLT(visual->compositor->depth_gl_scale);
		inc = 1;
		if (!tr_state->pixel_metrics) inc /= FIX2FLT(tr_state->min_hsize);
//		x = 0;
		y = 1;
		y = gf_mulfix(y, INT2FIX(txh->height/2));
		if (!tr_state->pixel_metrics) y = gf_divfix(y, tr_state->min_hsize);

		in_strip = 0;
		for (h=0; h<txh->height - 1; h++) {
			u8 *src = data + h*stride;
			x = -1;
			x = gf_mulfix(x, INT2FIX(txh->width/2));
			if (!tr_state->pixel_metrics) x  = gf_divfix(x, tr_state->min_hsize);

			for (w=0; w<txh->width; w++) {
				u8 *p1 = src + w*4;
				u8 *p2 = src + w*4 + stride;
				Float z1 = p1[3];
				Float z2 = p2[3];
				if (first_pass==1) {
					if ((z1>delta) || (z2>delta))
					{
#if 0
						if (in_strip) {
							glEnd();
							in_strip = 0;
						}
#endif
						x += FLT2FIX(inc);
						continue;
					}
				} else if (first_pass==0) {
					if ((z1<=delta) || (z2<=delta))
					{
						if (in_strip) {
							glEnd();
							in_strip = 0;
						}
						x += FLT2FIX(inc);
						continue;
					}
				}
				z1 = z1 / 255;
				z2 = z2 / 255;

				r = p1[0];
				r /= 255;
				g = p1[1];
				g /= 255;
				b = p1[2];
				b /= 255;

				if (!in_strip) {
					glBegin(GL_TRIANGLE_STRIP);
					in_strip = 1;
				}

				glColor3f(r, g, b);
				glVertex3f(FIX2FLT(x), FIX2FLT(y), FIX2FLT(z1)*scale);

				r = p2[0];
				r /= 255;
				g = p2[1];
				g /= 255;
				b = p2[2];
				b /= 255;

				glColor3f(r, g, b);
				glVertex3f(FIX2FLT(x), FIX2FLT(y)-inc, FIX2FLT(z2)*scale);

				x += FLT2FIX(inc);
			}
			if (in_strip) {
				glEnd();
				in_strip = 0;
			}
			y -= FLT2FIX(inc);
		}

		if (first_pass==1) {
			first_pass = 0;
			goto restart;
		}
		return;
	}

	glColor3f(1.0, 0.0, 0.0);
	/*render using vertex array*/
	if (!stack->mesh) {
		stack->mesh = new_mesh();
		stack->mesh->vbo_dynamic = 1;
		inc = 1;
		if (!tr_state->pixel_metrics) inc /= FIX2FLT(tr_state->min_hsize);
//		x = 0;
		y = 1;
		y = gf_mulfix(y, FLT2FIX(txh->height/2));
		if (!tr_state->pixel_metrics) y = gf_divfix(y, tr_state->min_hsize);

		if (txh->width>1 && txh->height>1) {
			for (h=0; h<txh->height; h++) {
				u32 idx_offset = h ? ((h-1)*txh->width) : 0;
				x = -1;
				x = gf_mulfix(x, FLT2FIX(txh->width/2));
				if (tr_state->min_hsize && !tr_state->pixel_metrics) x = gf_divfix(x, tr_state->min_hsize);

				for (w=0; w<txh->width; w++) {
					mesh_set_vertex(stack->mesh, x, y, 0, 0, 0, -FIX_ONE, INT2FIX(w / (txh->width-1)), INT2FIX((txh->height - h  -1) / (txh->height-1)) );
					x += FLT2FIX(inc);

					/*set triangle*/
					if (h && w) {
						u32 first_idx = idx_offset + w - 1;
						mesh_set_triangle(stack->mesh, first_idx, first_idx+1, txh->width + first_idx +1);
						mesh_set_triangle(stack->mesh, first_idx, txh->width + first_idx, txh->width + first_idx +1);
					}
				}
				y -= FLT2FIX(inc);
			}
			/*force recompute of Z*/
			txh->needs_refresh = 1;
		}
	}

	/*texture has been updated, recompute Z*/
	if (txh->needs_refresh) {
		Fixed f_scale = FLT2FIX(visual->compositor->depth_gl_scale);
		txh->needs_refresh = 0;

		data = (u8 *) gf_sc_texture_get_data(txh, &pixel_format);
		if (!data) return;
		if (pixel_format!=GF_PIXEL_RGB_DEPTH) return;
		data += txh->height*txh->width*3;

		for (h=0; h<txh->height; h++) {
			u8 *src = data + h * txh->width;
			for (w=0; w<txh->width; w++) {
				u8 d = src[w];
				Fixed z = INT2FIX(d);
				z = gf_mulfix(z / 255, f_scale);
				stack->mesh->vertices[w + h*txh->width].pos.z = z;
			}
		}
		stack->mesh->vbo_dirty = 1;
	}
#ifndef GPAC_DISABLE_VRML
	if (tr_state->appear) txtrans = ((M_Appearance *)tr_state->appear)->textureTransform;
#endif
	tr_state->mesh_num_textures = gf_sc_texture_enable(txh, txtrans);
	visual_3d_draw_mesh(tr_state, stack->mesh);
	visual_3d_disable_texture(tr_state);

#endif //GPAC_USE_GLES1X

}



GF_Err compositor_3d_setup_fbo(u32 width, u32 height, u32 *fbo_id, u32 *tx_id, u32 *depth_id)
{
#if defined(GPAC_USE_GLES1X)
	GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to setup FBO object: not supported on OpenGL ES 1.x\n"));
	return GF_NOT_SUPPORTED;
#else
	if (! *tx_id) {
		glGenTextures(1, tx_id);
	}
	glBindTexture(GL_TEXTURE_2D, *tx_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	if (! *fbo_id)
		 glGenFramebuffers(1, fbo_id);

	glBindFramebuffer(GL_FRAMEBUFFER, *fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tx_id, 0);

	if (! *depth_id)
		glGenRenderbuffers(1, depth_id);

	glBindRenderbuffer(GL_RENDERBUFFER, *depth_id);
#if defined(GPAC_CONFIG_IOS) ||  defined(GPAC_CONFIG_ANDROID)
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
#else
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
#endif
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depth_id);

   	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   	switch (status) {
   	case GL_FRAMEBUFFER_COMPLETE:
   		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to setup FBO object: FBO status %08x\n", status));
		return GF_NOT_SUPPORTED;
   }

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return GF_OK;
#endif

}

void compositor_3d_delete_fbo(u32 *fbo_id, u32 *fbo_tx_id, u32 *fbo_depth_id, Bool keep_tx_id)
{
#ifndef GPAC_USE_GLES1X
	if (*fbo_id) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, fbo_id);
		*fbo_id = 0;
	}
	if (*fbo_depth_id) {
		glDeleteRenderbuffers(1, fbo_depth_id);
		*fbo_depth_id = 0;
	}
	if (*fbo_tx_id && !keep_tx_id) {
		glDeleteTextures(1, fbo_tx_id);
		*fbo_tx_id = 0;
	}
#endif
}

u32 compositor_3d_get_fbo_pixfmt()
{
	return GL_TEXTURE_2D;
}

void compositor_3d_enable_fbo(GF_Compositor *compositor, Bool enable)
{
#ifndef GPAC_USE_GLES1X
	glBindFramebuffer(GL_FRAMEBUFFER, enable ? compositor->fbo_id : 0);
	if (!enable)
		glBindTexture(GL_TEXTURE_2D, 0);
#endif

}

void visual_3d_clean_state(GF_VisualManager *visual)
{
	glGetError();
}

#endif // GPAC_DISABLE_3D
