/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / libcaca output filter
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


#include <gpac/constants.h>
#include <gpac/color.h>
#include <gpac/list.h>
#include <gpac/modules/video_out.h>

#ifdef GPAC_HAS_LIBCACA

#include <caca.h>

typedef struct
{
	u32 w, h, pfmt;
	caca_dither_t *dither;
} CacaDither;

typedef struct
{
	const char *drv;

	u32 wnd_w, wnd_h;
	u32 mouse_x, mouse_y;
	caca_canvas_t *canvas;
	caca_display_t *display;
	GF_List *dithers;

	u8 *backbuffer;
	caca_dither_t *bb_dither;
} CacaOutCtx;

static void cacao_shutdown(GF_VideoOutput *dr)
{
	CacaOutCtx *ctx = (CacaOutCtx *)dr->opaque;
	if (ctx->display) caca_free_display(ctx->display);
	ctx->display=NULL;

	while (gf_list_count(ctx->dithers)) {
		CacaDither *d = gf_list_pop_back(ctx->dithers);
		if (d->dither) caca_free_dither(d->dither);
		gf_free(d);
	}

	if (ctx->canvas) caca_free_canvas(ctx->canvas);
	ctx->canvas=NULL;
}


static GF_Err cacao_setup(struct _video_out *dr, void *os_handle, void *os_display, u32 init_flags)
{
	CacaOutCtx *ctx = (CacaOutCtx *)dr->opaque;
	cacao_shutdown(dr);
	ctx->canvas = caca_create_canvas(0, 0);
    if (!ctx->canvas) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[Caca] Failed to create canvas\n"));
        return GF_IO_ERR;
    }

	ctx->display = NULL;
	const char *opt = gf_module_get_key((GF_BaseInterface *)dr, "driver");

#ifdef GPAC_CONFIG_DARWIN
	//if running in xcode disable color logs (not supported by output console)
	if (!opt && getenv("__XCODE_BUILT_PRODUCTS_DIR_PATHS") != NULL) {
		opt = "raw";
	}
#endif

	if (opt)
		ctx->display = caca_create_display_with_driver(ctx->canvas, opt);
	else
		ctx->display = caca_create_display(ctx->canvas);

	if (!ctx->display) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[Caca] Failed to create display using driver %s\n", opt ? opt : "defaul"));
		return GF_IO_ERR;
	}

//    caca_set_mouse(ctx->display, 0);
	caca_set_display_time(ctx->display, 0);
    GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[Caca] Canvas setup size %d %d driver %s\n", caca_get_canvas_width(ctx->canvas), caca_get_canvas_height(ctx->canvas), caca_get_display_driver(ctx->display) ));
	return GF_OK;
}

static GF_Err cacao_fullscreen(GF_VideoOutput *dr, Bool bFullScreenOn, u32 *screen_width, u32 *screen_height)
{
	return GF_OK;
}

static GF_Err cacao_flush(GF_VideoOutput *dr, GF_Window *dest)
{
	CacaOutCtx *ctx = (CacaOutCtx *)dr->opaque;
	if (ctx->backbuffer) {
		caca_dither_bitmap(ctx->canvas, 0, 0, caca_get_canvas_width(ctx->canvas), caca_get_canvas_height(ctx->canvas), ctx->bb_dither, ctx->backbuffer);
	}
    caca_refresh_display(ctx->display);
    return GF_OK;
}

static void cacao_check_event(GF_VideoOutput *dr)
{
	caca_event_t ev;
	CacaOutCtx *ctx = (CacaOutCtx *)dr->opaque;

	while (caca_get_event(ctx->display, CACA_EVENT_ANY, &ev, 0)) {
		GF_Event evt;
		u32 etype=0;
		Bool is_key=GF_FALSE, is_mouse=GF_FALSE;
		if (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS) {
			etype = GF_EVENT_KEYDOWN;
			is_key = GF_TRUE;
		}
		if (caca_get_event_type(&ev) & CACA_EVENT_KEY_RELEASE) {
			etype = GF_EVENT_KEYUP;
			is_key = GF_TRUE;
		}
		if (caca_get_event_type(&ev) & CACA_EVENT_MOUSE_MOTION) {
			etype = GF_EVENT_MOUSEMOVE;
			is_mouse = GF_TRUE;
			ctx->mouse_x = caca_get_event_mouse_x(&ev);
			ctx->mouse_x *= ctx->wnd_w;
			ctx->mouse_x /= caca_get_canvas_width(ctx->canvas);

			ctx->mouse_y = caca_get_event_mouse_y(&ev);
			ctx->mouse_y *= ctx->wnd_h;
			ctx->mouse_y /= caca_get_canvas_height(ctx->canvas);
		}
		if (caca_get_event_type(&ev) & CACA_EVENT_MOUSE_PRESS) {
			etype = GF_EVENT_MOUSEDOWN;
			is_mouse = GF_TRUE;
		}
		if (caca_get_event_type(&ev) & CACA_EVENT_MOUSE_RELEASE) {
			etype = GF_EVENT_MOUSEUP;
			is_mouse = GF_TRUE;
		}
		if (caca_get_event_type(&ev) & CACA_EVENT_QUIT) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			dr->on_event(dr->evt_cbk_hdl, &evt);
		}
		if (caca_get_event_type(&ev) & CACA_EVENT_RESIZE) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_SIZE;
//			evt.size.width = caca_get_event_resize_width(&ev);
//			evt.size.height = caca_get_event_resize_height(&ev);
//			dr->on_event(dr->evt_cbk_hdl, &evt);
		}
		if (is_key) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = etype;
			evt.key.key_code = caca_get_event_key_ch(&ev);
			switch (evt.key.key_code) {
			case CACA_KEY_ESCAPE: evt.type = GF_EVENT_QUIT; break;
			case CACA_KEY_CTRL_C: evt.type = GF_EVENT_QUIT; break;
			case CACA_KEY_LEFT: evt.key.key_code = GF_KEY_LEFT; break;
			case CACA_KEY_RIGHT: evt.key.key_code = GF_KEY_RIGHT; break;
			case CACA_KEY_UP: evt.key.key_code = GF_KEY_UP; break;
			case CACA_KEY_DOWN: evt.key.key_code = GF_KEY_DOWN; break;
			case CACA_KEY_RETURN: evt.key.key_code = GF_KEY_ENTER; break;
			case CACA_KEY_BACKSPACE: evt.key.key_code = GF_KEY_BACKSPACE; break;
			case CACA_KEY_TAB: evt.key.key_code = GF_KEY_TAB; break;
			case CACA_KEY_DELETE: evt.key.key_code = GF_KEY_DEL; break;
			case CACA_KEY_PAUSE: evt.key.key_code = GF_KEY_PAUSE; break;
			case CACA_KEY_INSERT: evt.key.key_code = GF_KEY_INSERT; break;
			case CACA_KEY_HOME: evt.key.key_code = GF_KEY_HOME; break;
			case CACA_KEY_END: evt.key.key_code = GF_KEY_END; break;
			case CACA_KEY_PAGEUP: evt.key.key_code = GF_KEY_PAGEUP; break;
			case CACA_KEY_PAGEDOWN: evt.key.key_code = GF_KEY_PAGEDOWN; break;
			case CACA_KEY_F1: evt.key.key_code = GF_KEY_F1; break;
			case CACA_KEY_F2: evt.key.key_code = GF_KEY_F2; break;
			case CACA_KEY_F3: evt.key.key_code = GF_KEY_F3; break;
			case CACA_KEY_F4: evt.key.key_code = GF_KEY_F4; break;
			case CACA_KEY_F5: evt.key.key_code = GF_KEY_F5; break;
			case CACA_KEY_F6: evt.key.key_code = GF_KEY_F6; break;
			case CACA_KEY_F7: evt.key.key_code = GF_KEY_F7; break;
			case CACA_KEY_F8: evt.key.key_code = GF_KEY_F8; break;
			case CACA_KEY_F9: evt.key.key_code = GF_KEY_F9; break;
			case CACA_KEY_F10: evt.key.key_code = GF_KEY_F10; break;
			case CACA_KEY_F11: evt.key.key_code = GF_KEY_F11; break;
			case CACA_KEY_F12: evt.key.key_code = GF_KEY_F12; break;
			case CACA_KEY_F13: evt.key.key_code = GF_KEY_F13; break;
			case CACA_KEY_F14: evt.key.key_code = GF_KEY_F14; break;
			case CACA_KEY_F15: evt.key.key_code = GF_KEY_F15; break;
			default:
				if ((evt.key.key_code>=0x30) && (evt.key.key_code<=0x39))
					evt.key.key_code = GF_KEY_0 + evt.key.key_code-0x30;
				else if ((evt.key.key_code>=0x41) && (evt.key.key_code<=0x5A))
					evt.key.key_code = GF_KEY_A + evt.key.key_code-0x41;
				else if ((evt.key.key_code>=0x61) && (evt.key.key_code<=0x7A))
					evt.key.key_code = GF_KEY_A + evt.key.key_code-0x61;
				break;
			}
			dr->on_event(dr->evt_cbk_hdl, &evt);
		}
		if (is_mouse) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = etype;
			evt.mouse.x = ctx->mouse_x;
			evt.mouse.y = ctx->mouse_y;
			dr->on_event(dr->evt_cbk_hdl, &evt);
		}

	}
}

static void cacao_print_message(CacaOutCtx *ctx, char *msg)
{
	u32 y = 2;
	char *src = msg;
	while (1) {
		char *sep = strchr(src, '\n');
		if (sep) sep[0] = 0;
		if (src[0])
			caca_put_str(ctx->canvas, 10, y, src);
		y++;
		if (!sep) break;
		sep[0] = '\n';
		src = sep+1;
	}
}


static GF_Err cacao_process_event(GF_VideoOutput *dr, GF_Event *evt)
{
	if (!evt) {
		cacao_check_event(dr);
		return GF_OK;
	}
	CacaOutCtx *ctx = (CacaOutCtx *)dr->opaque;
	switch (evt->type) {
	case GF_EVENT_SET_CURSOR:
		caca_set_mouse(ctx->display, evt->cursor.cursor_type ? 1 : 0);
		caca_set_cursor(ctx->display, evt->cursor.cursor_type ? 1 : 0);
		break;
	case GF_EVENT_SET_CAPTION:
	    caca_set_display_title(ctx->display, evt->caption.caption);
		break;
	case GF_EVENT_SIZE:
	    ctx->wnd_w = evt->size.width;
	    ctx->wnd_h = evt->size.height;
	    if (ctx->backbuffer) gf_free(ctx->backbuffer);
	    ctx->backbuffer = NULL;
	    if (ctx->bb_dither) caca_free_dither(ctx->bb_dither);
	    ctx->bb_dither = NULL;
		break;
	case GF_EVENT_VIDEO_SETUP:
	    ctx->wnd_w = evt->setup.width;
	    ctx->wnd_h = evt->setup.height;
	    if (ctx->backbuffer) gf_free(ctx->backbuffer);
	    ctx->backbuffer = NULL;
	    if (ctx->bb_dither) caca_free_dither(ctx->bb_dither);
	    ctx->bb_dither = NULL;
		break;
	case GF_EVENT_MESSAGE:
		cacao_print_message(ctx, (char *) evt->message.message);
		break;
	}
	return GF_OK;
}

static void set_dither_options(GF_VideoOutput *dr, caca_dither_t *dither)
{
	const char *opt	= gf_module_get_key((GF_BaseInterface *)dr, "antialias");
	if (opt) caca_set_dither_antialias(dither, opt);
	opt	= gf_module_get_key((GF_BaseInterface *)dr, "colors");
	if (opt) caca_set_dither_color(dither, opt);
	opt	= gf_module_get_key((GF_BaseInterface *)dr, "charset");
	if (opt) caca_set_dither_charset(dither, opt);
	opt	= gf_module_get_key((GF_BaseInterface *)dr, "algorithm");
	if (opt) caca_set_dither_algorithm(dither, opt);
	opt	= gf_module_get_key((GF_BaseInterface *)dr, "brightness");
	if (opt) caca_set_dither_brightness(dither, atof(opt));
	opt	= gf_module_get_key((GF_BaseInterface *)dr, "gamma");
	if (opt) caca_set_dither_gamma(dither, atof(opt));
	opt	= gf_module_get_key((GF_BaseInterface *)dr, "contrast");
	if (opt) caca_set_dither_contrast(dither, atof(opt));
}

static GF_Err cacao_blit(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	CacaOutCtx *ctx = (CacaOutCtx *)dr->opaque;

	if (ctx->backbuffer) return GF_NOT_SUPPORTED;

	CacaDither *d = NULL;
	u32 i, count = gf_list_count(ctx->dithers);
	for (i=0; i<count; i++) {
		d = gf_list_get(ctx->dithers, i);
		if (d->pfmt == video_src->pixel_format) break;
		d = NULL;
	}


	if (!d
		|| (d->w != video_src->width)
		|| (d->h != video_src->height)
	) {
		u32 rmask=0, bmask=0, gmask=0, amask=0;
		switch (video_src->pixel_format) {
		case GF_PIXEL_RGBA:
			amask = 0xff000000;
		case GF_PIXEL_RGB:
		case GF_PIXEL_RGBX:
			rmask = 0x0000ff;
			gmask = 0x00ff00;
			bmask = 0xff0000;
			break;
		case GF_PIXEL_BGRA:
			amask = 0xff000000;
		case GF_PIXEL_BGR:
		case GF_PIXEL_BGRX:
			bmask = 0x0000ff;
			gmask = 0x00ff00;
			rmask = 0xff0000;
			break;
		case GF_PIXEL_ARGB:
			amask = 0x000000ff;
			rmask = 0x0000ff00;
			gmask = 0x00ff0000;
			bmask = 0xff000000;
			break;
		case GF_PIXEL_ABGR:
			amask = 0x000000ff;
			bmask = 0x0000ff00;
			gmask = 0x00ff0000;
			rmask = 0xff000000;
			break;
		default:
			return GF_NOT_SUPPORTED;
		}
		if (!d) {
			GF_SAFEALLOC(d, CacaDither);
			d->pfmt = video_src->pixel_format;
			gf_list_add(ctx->dithers, d);
		}
		d->w = video_src->width;
		d->h = video_src->height;

		if (d->dither) caca_free_dither(d->dither);
		u32 bpp = gf_pixel_get_bytes_per_pixel(video_src->pixel_format);
		d->dither = caca_create_dither(bpp*8, d->w, d->h, (video_src->pitch_y>0) ? video_src->pitch_y : (bpp * d->w),
			rmask, gmask, bmask, amask);

		if (!d->dither) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[Caca] Failed to create dither\n"));
			return GF_IO_ERR;
		}

		//set options
		set_dither_options(dr, d->dither);
	}
	int x=0;
	int y=0;
	int w=caca_get_canvas_width(ctx->canvas);
	int h=caca_get_canvas_height(ctx->canvas);
	if (dst_wnd) {
		x = dst_wnd->x * w / ctx->wnd_w;
		y = dst_wnd->y * h / ctx->wnd_h;
		w = dst_wnd->w * w / ctx->wnd_w;
		h = dst_wnd->h * h / ctx->wnd_h;
	}

    caca_dither_bitmap(ctx->canvas, x, y, w, h, d->dither, video_src->video_buffer);
	return GF_OK;
}

static GF_Err cacao_lock_backbuffer(GF_VideoOutput *driv, GF_VideoSurface *video_info, Bool do_lock)
{
	CacaOutCtx *ctx = (CacaOutCtx *)driv->opaque;
	if (!do_lock) return GF_OK;

	if (!ctx->backbuffer) {
		ctx->backbuffer = gf_malloc(3*ctx->wnd_w*ctx->wnd_h);
		if (!ctx->backbuffer) return GF_OUT_OF_MEM;

		ctx->bb_dither = caca_create_dither(24, ctx->wnd_w, ctx->wnd_h, ctx->wnd_w*3, 0x0000ff, 0x00ff00, 0xff0000, 0);
		if (!ctx->bb_dither) return GF_OUT_OF_MEM;
		set_dither_options(driv, ctx->bb_dither);
	}
	memset(video_info, 0, sizeof(GF_VideoSurface));
	video_info->width = ctx->wnd_w;
	video_info->height = ctx->wnd_h;
	video_info->pitch_x = 3;
	video_info->pitch_y = ctx->wnd_w*3;
	video_info->pixel_format = GF_PIXEL_RGB;
	video_info->video_buffer = ctx->backbuffer;
	return GF_OK;
}


//do not change order
static GF_GPACArg CacaArgs[] = {
	GF_DEF_ARG("driver", NULL, NULL, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("antialias", NULL, NULL, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("colors", NULL, NULL, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("charset", NULL, NULL, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("algorithm", NULL, NULL, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("brightness", NULL, "image brightness", "1", NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("gamma", NULL, "image gamma", "1", NULL, GF_ARG_DOUBLE, 0),
	GF_DEF_ARG("contrast", NULL, "image contrast", "1", NULL, GF_ARG_DOUBLE, 0),
	{0},
};

static GF_GPACArg *cacao_push_arg(GF_VideoOutput *driv, u32 *nb_args, const char *opt_desc)
{
	GF_GPACArg *arg = &driv->args[ *nb_args] ;
	(*nb_args) ++;
	if (opt_desc) arg->description = gf_strdup(opt_desc);
	return arg;
}

static void print_opts(GF_VideoOutput *driv, u32 *nb_args, const char *opt_name, const char *opt_desc, char const * const * values, const char *def)
{
	GF_GPACArg *arg = cacao_push_arg(driv, nb_args, opt_desc);
	arg->val = def ? gf_strdup(def) : NULL;

    u32 i;
    for (i=0; values[i]; i+=2) {
		gf_dynstrcat((char **)&arg->description, values[i], "- ");
		gf_dynstrcat((char **)&arg->description, values[i+1], ": ");
		if (values[i+2])
			gf_dynstrcat((char **)&arg->description, "\n", NULL);
	}
}

static void cacao_load_options(GF_VideoOutput *drv)
{
	u32 nb_args=0;

	caca_canvas_t *cv = caca_create_canvas(0, 0);
	caca_display_t *dp = NULL;
#ifdef GPAC_CONFIG_DARWIN
		//if running in xcode disable color logs (not supported by output console)
		if (getenv("__XCODE_BUILT_PRODUCTS_DIR_PATHS") != NULL) {
			dp = caca_create_display_with_driver(cv, "raw");
		}
#endif
	if (!dp) dp = caca_create_display(cv);

	print_opts(drv, &nb_args, "driver", "rendering backend\n", caca_get_display_driver_list(), NULL);
	caca_free_display(dp);
	caca_free_canvas(cv);


	caca_dither_t *d = caca_create_dither(24, 2, 2, 6, 0x0000FF, 0x00FF00, 0xFF0000, 0xFF000000);
	print_opts(drv, &nb_args, "antialias", "image dither mode\n", caca_get_dither_antialias_list(d), caca_get_dither_antialias(d));
	print_opts(drv, &nb_args, "colors", "image color mode\n", caca_get_dither_color_list(d), caca_get_dither_color(d));
	print_opts(drv, &nb_args, "charset", "image charset\n", caca_get_dither_charset_list(d), caca_get_dither_charset(d));
	print_opts(drv, &nb_args, "algorithm", "image dithering algo\n", caca_get_dither_algorithm_list(d), caca_get_dither_algorithm(d));

	caca_free_dither(d);
}

static void *cacao_new()
{
	CacaOutCtx *ctx;
	GF_VideoOutput *driv;

	GF_SAFEALLOC(driv, GF_VideoOutput);
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "caca", "gpac distribution");

	GF_SAFEALLOC(ctx, CacaOutCtx);

	driv->opaque = ctx;
	driv->Setup = cacao_setup;
	driv->Shutdown = cacao_shutdown;
	driv->SetFullScreen = cacao_fullscreen;
	driv->Flush = cacao_flush;
	driv->ProcessEvent = cacao_process_event;
	driv->hw_caps = GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_STRETCH;
	driv->Blit = cacao_blit;
	driv->LockBackBuffer = cacao_lock_backbuffer;
	driv->LockOSContext = NULL;
	ctx->dithers = gf_list_new();
	driv->args = CacaArgs;
	driv->description = "Video output in terminal using libcaca";
	if (gf_opts_get_bool("temp", "gendoc"))
		cacao_load_options(driv);

	gf_opts_set_key("temp", "use_libcaca", "true");

	return driv;
}

static void cacao_del(void *ifce)
{
	GF_VideoOutput *dr = (GF_VideoOutput *)ifce;
	CacaOutCtx *ctx = dr->opaque;
	if (ctx->display) caca_free_display(ctx->display);
	if (ctx->canvas) caca_free_canvas(ctx->canvas);

	while (gf_list_count(ctx->dithers)) {
		CacaDither *d = gf_list_pop_back(ctx->dithers);
		if (d->dither) caca_free_dither(d->dither);
		gf_free(d);
	}
	if (dr->args) {
		for (u32 i=0; i<5; i++) {
			if (dr->args[i].description) gf_free((char *) dr->args[i].description);
			if (dr->args[i].val) gf_free((char *) dr->args[i].val);
		}
	}
	gf_list_del(ctx->dithers);

	if (ctx->bb_dither) caca_free_dither(ctx->bb_dither);
	if (ctx->backbuffer) gf_free(ctx->backbuffer);

	gf_opts_set_key("temp", "use_libcaca", NULL);
	gf_free(ctx);
	gf_free(dr);
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
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface*)cacao_new();
	return NULL;
}

/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		cacao_del(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( caca_out )

#endif // GPAC_HAS_LIBCACA
