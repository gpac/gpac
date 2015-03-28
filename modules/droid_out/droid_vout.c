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

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <android/log.h>

#define TAG "DROID_VIDEO"

#define LOG   __android_log_print

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
} AndroidContext;


#define RAW_OUT_PIXEL_FORMAT		GF_PIXEL_RGBA
#define NBPP						4

#define RAWCTX	AndroidContext *rc = (AndroidContext *)dr->opaque

//#define GLES_FRAMEBUFFER_TEST

#ifdef GLES_FRAMEBUFFER_TEST
#warning "Using FrameBuffer"
#endif

void initGL(AndroidContext *rc)
{
	char* ext;

	LOG( ANDROID_LOG_DEBUG, TAG, "Android InitGL");

	ext = (char*)glGetString(GL_VENDOR);
	LOG( ANDROID_LOG_INFO, TAG, "OpenGL ES Vendor: %s", ext);

	ext = (char*)glGetString(GL_RENDERER);
	LOG( ANDROID_LOG_INFO, TAG, "OpenGL ES Renderer: %s", ext);

	ext = (char*)glGetString(GL_VERSION);
	LOG( ANDROID_LOG_INFO, TAG, "OpenGL ES Version: %s", ext);

	ext = (char*)glGetString(GL_EXTENSIONS);
	LOG( ANDROID_LOG_INFO, TAG, "OpenGL ES Extensions: %s", ext);

	if ( strstr(ext, "GL_OES_draw_texture") )
	{
		rc->draw_texture = 1;
		LOG( ANDROID_LOG_INFO, TAG, "Using GL_OES_draw_texture");
	}
	if ( strstr(ext, "GL_ARB_texture_non_power_of_two") )
	{
		rc->non_power_two = 0;
		LOG( ANDROID_LOG_INFO, TAG, "Using GL_ARB_texture_non_power_of_two");
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

	glDisable(GL_CULL_FACE | GL_NORMALIZE | GL_LIGHTING | GL_BLEND | GL_FOG | GL_COLOR_MATERIAL | GL_TEXTURE_2D);
}

void gluPerspective(GLfloat fovy, GLfloat aspect,
                    GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * (GLfloat)tan(fovy * PI / 360);
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustumx((GLfixed)(xmin * 65536), (GLfixed)(xmax * 65536),
	           (GLfixed)(ymin * 65536), (GLfixed)(ymax * 65536),
	           (GLfixed)(zNear * 65536), (GLfixed)(zFar * 65536));
}

void resizeWindow(AndroidContext *rc)
{
	LOG( ANDROID_LOG_VERBOSE, TAG, "resizeWindow : start");
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

	/* change to the projection matrix and set our viewing volume. */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/* Set our perspective */
	glOrthox(0, INT2FIX(rc->width), 0, INT2FIX(rc->height), INT2FIX(-1), INT2FIX(1));

	/* Make sure we're chaning the model view and not the projection */
	glMatrixMode(GL_MODELVIEW);

	/* Reset The View */
	glLoadIdentity();
	LOG( ANDROID_LOG_VERBOSE, TAG, "resizeWindow : end");
}

void drawGLScene(AndroidContext *rc)
{
#ifdef DROID_EXTREME_LOGS
	LOG( ANDROID_LOG_VERBOSE, TAG, "drawGLScene : start");
#endif /* DROID_EXTREME_LOGS */
	GLfloat vertices[4][3];
	GLfloat texcoord[4][2];
//	int i, j;

	float rgba[4];

#ifdef GLES_FRAMEBUFFER_TEST
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
#endif

	// Reset states
	rgba[0] = rgba[1] = rgba[2] = 0.f;
	rgba[0] = 1.f;
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, rgba);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, rgba);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, rgba);
	glDisable(GL_CULL_FACE | GL_NORMALIZE | GL_LIGHTING | GL_BLEND | GL_FOG | GL_COLOR_MATERIAL | GL_TEXTURE_2D);

	/* Clear The Screen And The Depth Buffer */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
	glBindTexture( GL_TEXTURE_2D, rc->texID);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//    for ( i = 0; i < rc->height/2; i++ )
//    	for ( j = 0; j < rc->width; j++ )
//    		rc->texData[ i*rc->width*NBPP + j*NBPP + 3] = 200;

//    memset(rc->texData, 255, 4 * rc->width * rc->height );
#ifndef GLES_FRAMEBUFFER_TEST
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, rc->tex_width, rc->tex_height, 0,
	              GL_RGBA, GL_UNSIGNED_BYTE, rc->texData );
#endif

	if ( rc->draw_texture )
	{
		int cropRect[4] = {0,rc->height,rc->width,-rc->height};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cropRect);

		glDrawTexsOES(0, 0, 0, rc->width, rc->height);
	}
	else
	{
		/* Enable VERTEX array */
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		/* Setup pointer to  VERTEX array */
		glVertexPointer(3, GL_FLOAT, 0, vertices);
		glTexCoordPointer(2, GL_FLOAT, 0, texcoord);

		/* Move Left 1.5 Units And Into The Screen 6.0 */
		glLoadIdentity();
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

		/* Disable vertex array */
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glDisable(GL_TEXTURE_2D);

	/* Flush all drawings */
	glFinish();
#ifdef GLES_FRAMEBUFFER_TEST
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, rc->framebuff);
#endif
#ifdef DROID_EXTREME_LOGS
	LOG( ANDROID_LOG_VERBOSE, TAG, "drawGLScene : end");
#endif /* DROID_EXTREME_LOGS */
}

int releaseTexture(AndroidContext *rc)
{
	if (!rc)
		return 0;
	LOG( ANDROID_LOG_DEBUG, TAG, "Android Delete Texture");

	if ( rc->texID >= 0 )
	{
		glDeleteTextures(1, &(rc->texID));
		rc->texID = -1;
	}
	if (rc->texData)
	{
		gf_free(rc->texData);
		rc->texData = NULL;
	}
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Delete Texture DONE");
	return 0;
}

int createTexture(AndroidContext *rc)
{
	if (!rc)
		return 0;
	if ( rc->texID >= 0 )
		releaseTexture(rc);

	LOG( ANDROID_LOG_INFO, TAG, "Android Create Texture Size: WxH: %dx%d",
	     rc->tex_width, rc->tex_height);

	glGenTextures( 1, &(rc->texID) );

	rc->texData = (GLubyte*)gf_malloc( 4 * rc->tex_width * rc->tex_height );
	memset(rc->texData, 255, 4 * rc->tex_width * rc->tex_height );
	//memset(data, 0, 4 * width * height/2 );

	glBindTexture( GL_TEXTURE_2D, rc->texID);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, rc->tex_width, rc->tex_height, 0,
	              GL_RGBA, GL_UNSIGNED_BYTE, NULL/*rc->texData*/ );

	glBindTexture( GL_TEXTURE_2D, 0);
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Create Texture DONE");
	return 0;
}

#ifdef GLES_FRAMEBUFFER_TEST

int releaseFrameBuffer(AndroidContext *rc)
{
	LOG( ANDROID_LOG_DEBUG, TAG, "Android Delete FrameBuffer");

	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);

	if ( rc->framebuff >= 0 )
	{
		glDeleteFramebuffersOES(1, &(rc->framebuff));
		rc->framebuff = -1;
	}
	if ( rc->depthbuff >= 0 )
	{
		glDeleteRenderbuffersOES(1, &(rc->depthbuff));
		rc->depthbuff = -1;
	}
}

int createFrameBuffer(AndroidContext *rc)
{
	int backingWidth;
	int backingHeight;
	int res;

	if ( rc->framebuff >= 0 )
		releaseFrameBuffer(rc);

	LOG( ANDROID_LOG_DEBUG, TAG, "Android Create FrameBuffer"));

	glGenFramebuffersOES(1, &(rc->framebuff));
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, rc->framebuff);

//	glGenRenderbuffersOES(1, &(rc->depthbuff));
//	glBindRenderbufferOES(GL_RENDERBUFFER_OES, rc->depthbuff);

//	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
//	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

//	LOG( ANDROID_LOG_ERROR, TAG, "Android Depth Buffer Size: %dx%d\n", backingWidth, backingHeight));

//    glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, rc->width, rc->height);

//    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES,
//            GL_RENDERBUFFER_OES, rc->depthbuff);

	glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
	                          GL_TEXTURE_2D, rc->texID, 0);

	if ( (res=(int)glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES)) != GL_FRAMEBUFFER_COMPLETE_OES )
	{
		LOG( ANDROID_LOG_ERROR, TAG, "Android failed to make complete framebuffer object:");
		switch (res)
		{
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES:
			LOG( ANDROID_LOG_ERROR, TAG, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES:
			LOG( ANDROID_LOG_ERROR, TAG, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES:
			LOG( ANDROID_LOG_ERROR, TAG, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES:
			LOG( ANDROID_LOG_ERROR, TAG, "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_OES:
			LOG( ANDROID_LOG_ERROR, TAG, "GL_FRAMEBUFFER_UNSUPPORTED_OES");
			break;
		default :
			LOG( ANDROID_LOG_ERROR, TAG, "Unknown error: %d", res);
			break;
		}

		return 1;
	}

	//glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);

	return 0;
}

#endif

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
	LOG( ANDROID_LOG_DEBUG, TAG, "Android Resize: %dx%d", w, h);

	rc->width = w;
	rc->height = h;

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

	resizeWindow(rc);

	if ( rc->out_3d_type == 0 )
	{
		createTexture(rc);
#ifdef GLES_FRAMEBUFFER_TEST
		createFrameBuffer(rc);
#endif
	}
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Resize DONE", w, h);
	return GF_OK;
}

GF_Err droid_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	RAWCTX;
	void * pixels;
	int ret;

	LOG( ANDROID_LOG_DEBUG, TAG, "Android Setup: %d", init_flags);

#ifndef GLES_FRAMEBUFFER_TEST
	if ( rc->out_3d_type == 0 )
#endif
		initGL(rc);
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Setup DONE");
	return GF_OK;
}


static void droid_Shutdown(GF_VideoOutput *dr)
{
	RAWCTX;
	LOG( ANDROID_LOG_DEBUG, TAG, "Android Shutdown\n");

	releaseTexture(rc);
#ifdef GLES_FRAMEBUFFER_TEST
	releaseFrameBuffer(rc);
#endif
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Shutdown DONE");
}


static GF_Err droid_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	RAWCTX;
#ifdef DROID_EXTREME_LOGS
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Flush\n");
#endif /* DROID_EXTREME_LOGS */

#ifndef GLES_FRAMEBUFFER_TEST
	if ( rc->out_3d_type == 0 )
#endif
		drawGLScene(rc);
#ifdef DROID_EXTREME_LOGS
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android Flush DONE");
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
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android LockBackBuffer: %d", do_lock);
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
	LOG( ANDROID_LOG_VERBOSE, TAG, "Android LockBackBuffer DONE");
#endif /* DROID_EXTREME_LOGS */
	return GF_OK;
}

static GF_Err droid_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	RAWCTX;

	if (evt) {
		switch (evt->type) {
		case GF_EVENT_SIZE:
			LOG( ANDROID_LOG_VERBOSE, TAG, "GF_EVENT_SIZE( %d x %d)",
			     evt->setup.width, evt->setup.height);
			//if (evt->setup.opengl_mode) return GF_OK;
			return droid_Resize(dr, evt->setup.width, evt->setup.height);
		case GF_EVENT_VIDEO_SETUP:
			LOG( ANDROID_LOG_DEBUG, TAG, "Android OpenGL mode: %d", evt->setup.opengl_mode);
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
				LOG( ANDROID_LOG_VERBOSE, TAG, "GF_EVENT_SET_CURSOR");
				return GF_OK;
			case GF_EVENT_SET_CAPTION:
				LOG( ANDROID_LOG_VERBOSE, TAG, "GF_EVENT_SET_CAPTION");
				return GF_OK;
			case GF_EVENT_SHOWHIDE:
				LOG( ANDROID_LOG_VERBOSE, TAG, "GF_EVENT_SHOWHIDE");
				return GF_OK;
			default:
				LOG( ANDROID_LOG_DEBUG, TAG, "Process Unknown Event: %d", evt->type);
				return GF_OK;
			}
			break;
		}
	}
	return GF_OK;
}

GF_VideoOutput *NewAndroidVideoOutput()
{
	AndroidContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	LOG( ANDROID_LOG_INFO, TAG, "Android Video Initialization in progress...");
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

	driv->max_screen_width = 1024;
	driv->max_screen_height = 1024;

	driv->hw_caps = GF_VIDEO_HW_OPENGL;// | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;//GF_VIDEO_HW_DIRECT_ONLY;//

	LOG( ANDROID_LOG_INFO, TAG, "Android Video Init Done.\n");
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
	LOG( ANDROID_LOG_DEBUG, TAG, "Android vout deinit\n");
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
