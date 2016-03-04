/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / Wrapper
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

/*driver interfaces*/
#include <gpac/modules/video_out.h>
#include <gpac/list.h>
#include <gpac/constants.h>

#include <gpac/setup.h>

#ifdef GPAC_USE_GLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif

#ifdef PI
#undef PI
#endif

#define PI 3.1415926535897932f

/* Uncomment the next line if you want to debug */
/* #define DROID_EXTREME_LOGS */

typedef struct
{
	u32 width, height;
	void * locked_data;
	u8 out_3d_type;

	u32 tex_width, tex_height;

	GLuint texID;
	GLuint framebuff;
	GLuint depthbuff;

	GLubyte* texData;

	u8 draw_texture;
	u8 non_power_two;

	Bool fullscreen;

	//Functions specific to OpenGL ES2
#ifdef GPAC_USE_GLES2
	GLuint base_vertex, base_fragment, base_program;
	GF_Matrix identity, ortho;
#endif
} AndroidContext;


#define RAW_OUT_PIXEL_FORMAT		GF_PIXEL_RGBA
#define NBPP						4

#define RAWCTX	AndroidContext *rc = (AndroidContext *)dr->opaque



//Functions specific to OpenGL ES2
#ifdef GPAC_USE_GLES2

#define GF_TRUE 1
#define GF_FALSE 0


//we custom-define these instead of importing gl_inc.h
#define GL_COMPILE_STATUS 0x8B81
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_LINK_STATUS 0x8B82
#define GL_VERTEX_SHADER 0x8B31



static char *glsl_vertex = "precision mediump float;\
	attribute vec4 gfVertex;\
	attribute vec4 gfTexCoord;\
	varying vec2 TexCoord;\
	uniform mat4 gfModelViewMatrix;\
	uniform mat4 gfProjectionMatrix;\
	void main(void){\
		vec4 gfEye;\
		gfEye = gfModelViewMatrix * gfVertex;\
		TexCoord = vec2(gfTexCoord);\
		gl_Position = gfProjectionMatrix * gfEye;\
	}";

static char *glsl_fragment = "precision mediump float;\
	varying vec2 TexCoord;\
	uniform sampler2D img;\
	void main(void){\
		gl_FragColor = texture2D(img, TexCoord);\
	}";

static inline void gl_check_error()
{
	s32 res = glGetError();
	if (res) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE,
		       ("GL Error %d file %s line %d\n", res,
				__FILE__, __LINE__));
	}
}


static GLint gf_glGetUniformLocation(u32 glsl_program, const char *uniform_name)
{
	GLint loc = glGetUniformLocation(glsl_program, uniform_name);
	if (loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Cannot find uniform \"%s\" in GLSL program\n", uniform_name));
	}
	return loc;
}

static GLint gf_glGetAttribLocation(u32 glsl_program, const char *attrib_name)
{
	GLint loc = glGetAttribLocation(glsl_program, attrib_name);
	if (loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[V3D:GLSL] Cannot find attrib \"%s\" in GLSL program\n", attrib_name));
	}
	return loc;
}

//modified version of visual_3d_compile_shader function
Bool compile_shader(u32 shader_id, const char *name, const char *source) {
	GLint blen = 0;
	GLsizei slen = 0;
	u32 len;
	GLint is_compiled = 0;


	if(!source || !shader_id) return 0;
	len = (u32) strlen(source);
	glShaderSource(shader_id, 1, &source, &len);
	glCompileShader(shader_id);

	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &is_compiled);
	if (is_compiled == 1) return GF_TRUE;

	glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH , &blen);
	if (blen > 1) {
		char* compiler_log = (char*) gf_malloc(blen);
		glGetShaderInfoLog(shader_id, blen, &slen, compiler_log);
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[GLSL] Failed to compile %s shader: %s\n", name, compiler_log));
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[GLSL] ***** faulty shader code ****\n%s\n**********************\n", source));
		gf_free (compiler_log);
		return GF_FALSE;
	}

	return GF_TRUE;
}


static Bool initGLES2(AndroidContext *rc) {


//PRINT OpengGL INFO
	char* ext;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android InitGLES2"));

	ext = (char*)glGetString(GL_VENDOR);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Vendor: %s", ext));

	ext = (char*)glGetString(GL_RENDERER);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Renderer: %s", ext));

	ext = (char*)glGetString(GL_VERSION);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Version: %s", ext));

	ext = (char*)glGetString(GL_EXTENSIONS);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Extensions: %s", ext));



//Generic GL setup
	/* Set the background black */
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	/* Depth buffer setup */
	glClearDepthf(1.0f);

	/* Enables Depth Testing */
	glEnable(GL_DEPTH_TEST);

	/* The Type Of Depth Test To Do */
	glDepthFunc(GL_LEQUAL);


//Shaders setup
	Bool res = GF_FALSE;
	GLint linked;

	gl_check_error();
	gf_mx_init(rc->identity);
	rc->base_program = glCreateProgram();
	rc->base_vertex = glCreateShader(GL_VERTEX_SHADER);
	rc->base_fragment = glCreateShader(GL_FRAGMENT_SHADER);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Compiling shaders"));
	res = compile_shader(rc->base_vertex, "vertex", glsl_vertex);
	if(!res) return GF_FALSE;
	res = compile_shader(rc->base_fragment, "fragment", glsl_fragment);
	if(!res) return GF_FALSE;

	glAttachShader(rc->base_program, rc->base_vertex);
	glAttachShader(rc->base_program, rc->base_fragment);
	glLinkProgram(rc->base_program);

	glGetProgramiv(rc->base_program, GL_LINK_STATUS, &linked);
	if (!linked) {
		int i32CharsWritten, i32InfoLogLength;
		char pszInfoLog[2048];
		glGetProgramiv(rc->base_program, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		glGetProgramInfoLog(rc->base_program, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, (pszInfoLog));
		return GF_FALSE;
	}
	glUseProgram(rc->base_program);
	gl_check_error();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Shaders compiled"))

	return GF_TRUE;
}

static void load_matrix_shaders(GLuint program, Fixed *mat, const char *name)
{
	GLint loc=-1;
#ifdef GPAC_FIXED_POINT
	Float _mat[16];
	u32 i;
#endif
	gl_check_error();
	loc = glGetUniformLocation(program, name);
	if(loc<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("GL Error (file %s line %d): Invalid matrix name", __FILE__, __LINE__));
		return;
	}
	gl_check_error();

#ifdef GPAC_FIXED_POINT
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glUniformMatrix4fv(loc, 1, GL_FALSE, (GLfloat *) _mat);
#else
	glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
#endif
	gl_check_error();
}


//ES2 version of glOrthox() - resulting matrix is stored in rc->ortho
//more info on Orthographic projection matrix at http://www.songho.ca/opengl/gl_projectionmatrix.html#ortho
static void calculate_ortho(Fixed left, Fixed right, Fixed bottom, Fixed top, Fixed near, Fixed far,  AndroidContext *rc) {


	if((left==right)|(bottom==top)|(near==far)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("GL Error (file %s line %d): Invalid Orthogonal projection values", __FILE__, __LINE__));
		return;
	}

	gf_mx_init(rc->ortho);

//For Orthographic Projection
	rc->ortho.m[0] = gf_divfix(2, (right-left));
	rc->ortho.m[5] = gf_divfix(2, (top-bottom));
	rc->ortho.m[10] = gf_divfix(-2, far-near);
	rc->ortho.m[12] = -gf_divfix(right+left, right-left);
	rc->ortho.m[13] = -gf_divfix(top+bottom, top-bottom);
	rc->ortho.m[14] = -gf_divfix(far+near, far-near);
	rc->ortho.m[15] = FIX_ONE;
}



#endif	//Endof specifix for GLES2 (ifdef GPAC_USE_GLES2)


#ifndef GPAC_USE_GLES2
void initGL(AndroidContext *rc)
{
	char* ext;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android InitGL"));

	ext = (char*)glGetString(GL_VENDOR);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Vendor: %s", ext));

	ext = (char*)glGetString(GL_RENDERER);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Renderer: %s", ext));

	ext = (char*)glGetString(GL_VERSION);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Version: %s", ext));

	ext = (char*)glGetString(GL_EXTENSIONS);
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("OpenGL ES Extensions: %s", ext));

	if ( strstr(ext, "GL_OES_draw_texture") )
	{
		rc->draw_texture = 1;
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("Using GL_OES_draw_texture"));
	}
	if ( strstr(ext, "GL_ARB_texture_non_power_of_two") )
	{
		rc->non_power_two = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("Using GL_ARB_texture_non_power_of_two"));
	}

	/* Enable smooth shading */
	glShadeModel(GL_SMOOTH);

	/* Set the background black */
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	/* Depth buffer setup */
	glClearDepthf(1.0f);

	/* Enables Depth Testing */
	glEnable(GL_DEPTH_TEST);

	/* The Type Of Depth Test To Do */
	glDepthFunc(GL_LEQUAL);

	/* Really Nice Perspective Calculations */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}
#endif

void gluPerspective(GLfloat fovy, GLfloat aspect,
                    GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * (GLfloat)tan(fovy * PI / 360);
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;
#ifndef GPAC_USE_GLES2
	glFrustumx((GLfixed)(xmin * 65536), (GLfixed)(xmax * 65536),
	           (GLfixed)(ymin * 65536), (GLfixed)(ymax * 65536),
	           (GLfixed)(zNear * 65536), (GLfixed)(zFar * 65536));
#endif
}

void resizeWindow(AndroidContext *rc)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("resizeWindow : start"));
	/* Height / width ration */
	GLfloat ratio;

	/* Protect against a divide by zero */
	if (rc->height==0)
	{
		rc->height=1;
	}

	ratio=(GLfloat)rc->width/(GLfloat)rc->height;

	/* Setup our viewport. */
	glViewport(0, 0, (GLsizei)rc->width, (GLsizei)rc->height);
#ifndef GPAC_USE_GLES2
	/* change to the projection matrix and set our viewing volume. */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/* Set our perspective */
	glOrthox(0, INT2FIX(rc->width), 0, INT2FIX(rc->height), INT2FIX(-1), INT2FIX(1));

	/* Make sure we're chaning the model view and not the projection */
	glMatrixMode(GL_MODELVIEW);

	/* Reset The View */
	glLoadIdentity();
#else
	gl_check_error();
	glUseProgram(rc->base_program);
	calculate_ortho(0, INT2FIX(rc->width), 0, INT2FIX(rc->height), INT2FIX(-1), INT2FIX(1), rc);
	load_matrix_shaders(rc->base_program, (Fixed *) rc->ortho.m, "gfProjectionMatrix");
	load_matrix_shaders(rc->base_program, (Fixed *) rc->identity.m, "gfModelViewMatrix");
	gl_check_error();
#endif
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("resizeWindow : end"));
}

void drawGLScene(AndroidContext *rc)
{
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("drawGLScene : start"));
#endif /* DROID_EXTREME_LOGS */
#ifdef GPAC_USE_GLES2
	GLuint loc_vertex_array, loc_texcoord_array;
	loc_vertex_array = loc_texcoord_array = -1;
#endif

	GLfloat vertices[4][3];
	GLfloat texcoord[4][2];
//	int i, j;

	float rgba[4];
	gl_check_error();

	// Reset states
	rgba[0] = rgba[1] = rgba[2] = 0.f;
	rgba[0] = 1.f;
#ifndef GPAC_USE_GLES2
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, rgba);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, rgba);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, rgba);
#endif
	/* Clear The Screen And The Depth Buffer */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl_check_error();
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef GPAC_USE_GLES2
	glEnable(GL_TEXTURE_2D);
#endif
	glUseProgram(rc->base_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_2D, rc->texID);
	gl_check_error();
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
#ifndef GPAC_USE_GLES2
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//    for ( i = 0; i < rc->height/2; i++ )
//    	for ( j = 0; j < rc->width; j++ )
//    		rc->texData[ i*rc->width*NBPP + j*NBPP + 3] = 200;

//    memset(rc->texData, 255, 4 * rc->width * rc->height );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, rc->tex_width, rc->tex_height, 0,
	              GL_RGBA, GL_UNSIGNED_BYTE, rc->texData );

	gl_check_error();
	if ( rc->draw_texture )
	{
		gl_check_error();
		int cropRect[4] = {0,rc->height,rc->width,-rc->height};
#ifndef GPAC_USE_GLES2
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cropRect);
		glDrawTexsOES(0, 0, 0, rc->width, rc->height);
#endif
	}
	else
	{
		gl_check_error();

#ifndef GPAC_USE_GLES2
		/* Enable VERTEX array */
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		/* Setup pointer to  VERTEX array */
		glVertexPointer(3, GL_FLOAT, 0, vertices);
		glTexCoordPointer(2, GL_FLOAT, 0, texcoord);

		/* Move Left 1.5 Units And Into The Screen 6.0 */
		glLoadIdentity();
#else
		loc_vertex_array = glGetAttribLocation(rc->base_program, "gfVertex");
		if(loc_vertex_array<0)
			return;
		glEnableVertexAttribArray(loc_vertex_array);
		glVertexAttribPointer(loc_vertex_array, 3, GL_FLOAT, GL_FALSE, 0, vertices);

		loc_texcoord_array = glGetAttribLocation(rc->base_program, "gfTexCoord");
		if (loc_texcoord_array>=0) {
			glVertexAttribPointer(loc_texcoord_array, 2, GL_FLOAT, GL_FALSE, 0, texcoord);
			glEnableVertexAttribArray(loc_texcoord_array);
		}


#endif
		//glTranslatef(0.0f, 0.0f, -3.3f);
		//glTranslatef(0.0f, 0.0f, -2.3f);

		/* Top Right Of The Quad    */
		vertices[0][0]=rc->tex_width;
		vertices[0][1]=rc->tex_height;
		vertices[0][2]=0.0f;
		texcoord[0][0]=1.f;
		texcoord[0][1]=0.f;
		/* Top Left Of The Quad     */
		vertices[1][0]=0.f;
		vertices[1][1]=rc->tex_height;
		vertices[1][2]=0.0f;
		texcoord[1][0]=0.f;
		texcoord[1][1]=0.f;
		/* Bottom Left Of The Quad  */
		vertices[2][0]=rc->tex_width;
		vertices[2][1]=0.f;
		vertices[2][2]=0.0f;
		texcoord[2][0]=1.f;
		texcoord[2][1]=1.f;
		/* Bottom Right Of The Quad */
		vertices[3][0]=0.f;
		vertices[3][1]=0.f;
		vertices[3][2]=0.0f;
		texcoord[3][0]=0.f;
		texcoord[3][1]=1.f;

		/* Drawing using triangle strips, draw triangles using 4 vertices */
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#ifndef GPAC_USE_GLES2
		/* Disable vertex array */
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
	}
#ifndef GPAC_USE_GLES2
	glDisable(GL_TEXTURE_2D);
#endif
	gl_check_error();

	/* Flush all drawings */
	glFinish();
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("drawGLScene : end"));
#endif /* DROID_EXTREME_LOGS */
}

int releaseTexture(AndroidContext *rc)
{
	gl_check_error();
	if (!rc)
		return 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Delete Texture"));

	if ( rc->texID >= 0)
	{
		glDeleteTextures(1, &(rc->texID));
		rc->texID = -1;
	}
	if (rc->texData)
	{
		gf_free(rc->texData);
		rc->texData = NULL;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Delete Texture DONE"))
	gl_check_error();
	return 0;
}

int createTexture(AndroidContext *rc)
{
	if (!rc)
		return 0;
	if ( rc->texID >= 0 )
		releaseTexture(rc);

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("Android Create Texture Size: WxH: %dx%d", rc->tex_width, rc->tex_height));

	glGenTextures( 1, &(rc->texID) );

	rc->texData = (GLubyte*)gf_malloc( 4 * rc->tex_width * rc->tex_height );
	memset(rc->texData, 255, 4 * rc->tex_width * rc->tex_height );
	//memset(data, 0, 4 * width * height/2 );

	glBindTexture( GL_TEXTURE_2D, rc->texID);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
#ifndef GPAC_USE_GLES2
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, rc->tex_width, rc->tex_height, 0,
	              GL_RGBA, GL_UNSIGNED_BYTE, NULL/*rc->texData*/ );

	glBindTexture( GL_TEXTURE_2D, 0);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Create Texture DONE"));
	return 0;
}


u32 find_pow_2(u32 num)
{
	u32 res = 1;
	while (res < num)
		res *= 2;
	return res;
}

static GF_Err droid_Resize(GF_VideoOutput *dr, u32 w, u32 h)
{
	RAWCTX;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Resize: %dx%d", w, h));

	rc->width = w;
	rc->height = h;

	if ((dr->max_screen_width < w) || (dr->max_screen_height < h)) {
		dr->max_screen_width = w;
		dr->max_screen_height = h;
	}
	//npot textures are supported in ES2
#ifdef GPAC_USE_GLES2
	rc->tex_width = rc->width;
	rc->tex_height = rc->height;
#else
	if ( rc->non_power_two )
	{
		rc->tex_width = rc->width;
		rc->tex_height = rc->height;
	}
	else
	{
		rc->tex_width = find_pow_2(rc->width);
		rc->tex_height = find_pow_2(rc->height);
	}
#endif
	gl_check_error();
	resizeWindow(rc);

	if ( rc->out_3d_type == 0 )
	{
		createTexture(rc);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Resize DONE", w, h));
	gl_check_error();
	return GF_OK;
}

GF_Err droid_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	RAWCTX;
	void * pixels;
	Bool res = GF_FALSE;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Setup: %d", init_flags));


#ifdef GPAC_USE_GLES2

	if ( rc->out_3d_type == 0 ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("We are in OpenGL: disable mode"));
		res = initGLES2(rc);
		if(res==GF_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("ERROR Compiling ES2 Shaders"));
		} else {	//set texture
			glUseProgram(rc->base_program);
			GLint loc = gf_glGetUniformLocation(rc->base_program, "img");
			glUniform1i(loc,0);
		}
	}

#else

	if ( rc->out_3d_type == 0 )

		initGL(rc);
#endif //GPAC_USE_GLES2

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Setup DONE"));
	return GF_OK;
}


static void droid_Shutdown(GF_VideoOutput *dr)
{
	RAWCTX;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Shutdown\n"));

	releaseTexture(rc);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Shutdown DONE"));
}


static GF_Err droid_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	RAWCTX;
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Flush\n"));
#endif /* DROID_EXTREME_LOGS */

	if ( rc->out_3d_type == 0 )
		drawGLScene(rc);
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android Flush DONE"));
#endif /* DROID_EXTREME_LOGS */
	return GF_OK;
}

static GF_Err droid_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	RAWCTX;
	int ret;
	void * pixels;
	int i,j,t;
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android LockBackBuffer: %d", do_lock));
#endif /* DROID_EXTREME_LOGS */
	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;

		if ( rc->out_3d_type != 0 )
			return GF_NOT_SUPPORTED;

		memset(vi, 0, sizeof(GF_VideoSurface));
		vi->height = rc->height;
		vi->width = rc->width;
		vi->video_buffer = rc->texData;
		vi->is_hardware_memory = 0;
		vi->pitch_x = NBPP;
		vi->pitch_y = NBPP * rc->tex_width;
		vi->pixel_format = RAW_OUT_PIXEL_FORMAT;
	}
	else
	{
		if (rc->locked_data)
		{
			//glBindTexture( GL_TEXTURE_2D, rc->texID);
			//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

			//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, rc->width, rc->height, 0,
			//			GL_RGBA, GL_UNSIGNED_BYTE, rc->locked_data );
		}
	}
#ifdef DROID_EXTREME_LOGS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android LockBackBuffer DONE"));
#endif /* DROID_EXTREME_LOGS */
	return GF_OK;
}

static GF_Err droid_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	RAWCTX;

	if (evt) {
		switch (evt->type) {
		case GF_EVENT_SIZE:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("GF_EVENT_SIZE( %d x %d)", evt->setup.width, evt->setup.height));
			//if (evt->setup.opengl_mode) return GF_OK;
			//in fullscreen mode: do not change viewport; just update perspective
			if (rc->fullscreen) {
#ifdef GPAC_USE_GLES2
				gl_check_error();
				glUseProgram(rc->base_program);
				calculate_ortho(0, INT2FIX(rc->width), 0, INT2FIX(rc->height), INT2FIX(-1), INT2FIX(1), rc);
				load_matrix_shaders(rc->base_program, (Fixed *) rc->ortho.m, "gfProjectionMatrix");
				load_matrix_shaders(rc->base_program, (Fixed *) rc->identity.m, "gfModelViewMatrix");
				gl_check_error();
#else
				/* change to the projection matrix and set our viewing volume. */
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();

				/* Set our perspective */
				glOrthox(0, INT2FIX(rc->width), 0, INT2FIX(rc->height), INT2FIX(-1), INT2FIX(1));

				/* Make sure we're chaning the model view and not the projection */
				glMatrixMode(GL_MODELVIEW);

				/* Reset The View */
				glLoadIdentity();
#endif
				return GF_OK;
			} else
				return droid_Resize(dr, evt->setup.width, evt->setup.height);

		case GF_EVENT_VIDEO_SETUP:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android OpenGL mode: %d", evt->setup.opengl_mode));
			switch (evt->setup.opengl_mode)
			{
			case 0:
				rc->out_3d_type = 0;
//					initGL(rc);
				droid_Resize(dr, evt->setup.width, evt->setup.height);
				return GF_OK;
			case 1:
				rc->out_3d_type = 1;
				droid_Resize(dr, evt->setup.width, evt->setup.height);
				return GF_OK;
			case 2:
				rc->out_3d_type = 2;
				droid_Resize(dr, evt->setup.width, evt->setup.height);
				return GF_OK;
			case GF_EVENT_SET_CURSOR:
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("GF_EVENT_SET_CURSOR"));
				return GF_OK;
			case GF_EVENT_SET_CAPTION:
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("GF_EVENT_SET_CAPTION"));
				return GF_OK;
			case GF_EVENT_SHOWHIDE:
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("GF_EVENT_SHOWHIDE"));
				return GF_OK;
			default:
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Process Unknown Event: %d", evt->type));
				return GF_OK;
			}
			break;
		case GF_EVENT_TEXT_EDITING_START:
		case GF_EVENT_TEXT_EDITING_END:
			return GF_NOT_SUPPORTED;
		}
	}
	return GF_OK;
}

static GF_Err droid_SetFullScreen(GF_VideoOutput *dr, Bool bOn, u32 *outWidth, u32 *outHeight)
{
	RAWCTX;;

	*outWidth = dr->max_screen_width;
	*outHeight = dr->max_screen_height;
	rc->fullscreen = bOn;
	return droid_Resize(dr, dr->max_screen_width, dr->max_screen_height);
}

GF_VideoOutput *NewAndroidVideoOutput()
{
	AndroidContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("Android Video Initialization in progress..."));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "Android Video Output", "gpac distribution")

	pCtx = gf_malloc(sizeof(AndroidContext));
	memset(pCtx, 0, sizeof(AndroidContext));

	pCtx->texID = -1;
	pCtx->framebuff = -1;
	pCtx->depthbuff = -1;
	driv->opaque = pCtx;

	driv->Flush = droid_Flush;
	driv->LockBackBuffer = droid_LockBackBuffer;
	driv->Setup = droid_Setup;
	driv->Shutdown = droid_Shutdown;
	driv->ProcessEvent = droid_ProcessEvent;
	driv->SetFullScreen = droid_SetFullScreen;

	driv->max_screen_width = 1024;
	driv->max_screen_height = 1024;

	driv->hw_caps = GF_VIDEO_HW_OPENGL;// | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;//GF_VIDEO_HW_DIRECT_ONLY;//

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("Android Video Init Done.\n"));
	return (void *)driv;
}

void DeleteAndroidVideoOutput(void *ifce)
{
	AndroidContext *rc;
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	if (!ifce)
		return;
	droid_Shutdown(driv);
	rc = (AndroidContext *)driv->opaque;
	if (rc)
		gf_free(rc);
	driv->opaque = NULL;
	gf_free(driv);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("Android vout deinit\n"));
}

/*interface query*/
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		0
	};
	return si;
}
/*interface create*/
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewAndroidVideoOutput();
	return NULL;
}
/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteAndroidVideoOutput((GF_VideoOutput *)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( droid_vidgl )
