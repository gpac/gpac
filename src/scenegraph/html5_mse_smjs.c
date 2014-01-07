/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2012-
 *          All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
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

#ifndef GPAC_DISABLE_SVG

#ifdef GPAC_HAS_SPIDERMONKEY

#ifdef GPAC_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif /* XP_UNIX */
#endif

#include <gpac/html5_mse.h>

#define JSVAL_CHECK_STRING(_v) (JSVAL_IS_STRING(_v) || JSVAL_IS_NULL(_v))

typedef enum 
{
    /* MediaSource properties */
    HTML_MEDIASOURCE_PROP_SOURCEBUFFERS         = 34,
    HTML_MEDIASOURCE_PROP_ACTIVESOURCEBUFFERS   = 35,
    HTML_MEDIASOURCE_PROP_DURATION              = 36,
    HTML_MEDIASOURCE_PROP_READYSTATE            = 37,
    /* SourceBuffer properties */
    HTML_SOURCEBUFFER_PROP_UPDATING             = 38,
    HTML_SOURCEBUFFER_PROP_BUFFERED             = 39,
    HTML_SOURCEBUFFER_PROP_TIMESTAMPOFFSET      = 40,
    HTML_SOURCEBUFFER_PROP_TIMESCALE            = 41,
    HTML_SOURCEBUFFER_PROP_AUDIOTRACKS          = 42,
    HTML_SOURCEBUFFER_PROP_VIDEOTRACKS          = 43,
    HTML_SOURCEBUFFER_PROP_TEXTTRACKS           = 44,
    HTML_SOURCEBUFFER_PROP_APPENDWINDOWSTART    = 45,
    HTML_SOURCEBUFFER_PROP_APPENDWINDOWEND      = 46,
    /* SourceBufferList properties */
    HTML_SOURCEBUFFERLIST_PROP_LENGTH           = 47,
} GF_HTML_MediaSourcePropEnum;

static GF_HTML_MediaRuntime *html_media_rt = NULL;

static void mediasource_sourceBuffer_initjs(JSContext *c, JSObject *ms_obj, GF_HTML_SourceBuffer *sb)
{
    sb->_this = JS_NewObject(c, &html_media_rt->sourceBufferClass._class, 0, 0);
    //gf_js_add_root(c, &sb->_this, GF_JSGC_OBJECT);
    SMJS_SET_PRIVATE(c, sb->_this, sb);
    sb->buffered._this = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, sb->_this);
    SMJS_SET_PRIVATE(c, sb->buffered._this, &sb->buffered);
}

static GFINLINE GF_SceneGraph *mediasource_get_scenegraph(JSContext *c)
{
    GF_SceneGraph *scene;
    JSObject *global = JS_GetGlobalObject(c);
    assert(global);
    scene = (GF_SceneGraph *)SMJS_GET_PRIVATE(c, global);
    assert(scene);
    return scene;
}

#include <gpac/internal/terminal_dev.h>
Bool gf_term_is_type_supported(GF_Terminal *term, const char* mime);

static JSBool SMJS_FUNCTION(mediasource_is_type_supported)
{
    SMJS_ARGS
    GF_SceneGraph *sg;
    GF_JSAPIParam par;
    Bool isSupported;
    char *mime;
    if (!argc || !JSVAL_CHECK_STRING(argv[0]) ) 
    {
        return JS_TRUE;
    }
    mime = SMJS_CHARS(c, argv[0]);
    sg = mediasource_get_scenegraph(c);
    sg->script_action(sg->script_action_cbck, GF_JSAPI_OP_GET_TERM, NULL, &par);
    isSupported = gf_term_is_type_supported((GF_Terminal *)par.term, mime);
    SMJS_SET_RVAL(BOOLEAN_TO_JSVAL(isSupported ? JS_TRUE : JS_FALSE));
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(mediasource_addSourceBuffer)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_SourceBuffer    *sb;
    GF_HTML_MediaSource     *p;
    const char              *mime;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return JS_TRUE;
    }
    if (!argc || !JSVAL_CHECK_STRING(argv[0])) 
    {
        return JS_TRUE;
    }
    mime = SMJS_CHARS(c, argv[0]);
    if (!strlen(mime))
    {
        return JS_TRUE;
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (p->readyState != MEDIA_SOURCE_OPEN) 
    {
        return JS_TRUE;
    }
    assert(p->service);
    sb = gf_mse_source_buffer_new(p);
    gf_mse_source_buffer_load_parser(sb, mime);
    mediasource_sourceBuffer_initjs(c, obj, sb);
    SMJS_SET_RVAL( OBJECT_TO_JSVAL(sb->_this) );
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(mediasource_removeSourceBuffer)
{
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(mediasource_endOfStream)
{
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(media_source_constructor)
{
    GF_HTML_MediaSource *p;
    SMJS_OBJ_CONSTRUCTOR(&html_media_rt->mediaSourceClass)

    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) return JS_TRUE;

    p = gf_mse_media_source_new();
    p->c = c;
    p->_this = obj;
    SMJS_SET_PRIVATE(c, obj, p);

    p->sourceBuffers._this = JS_NewObject(c, &html_media_rt->sourceBufferListClass._class, 0, 0);
    SMJS_SET_PRIVATE(c, p->sourceBuffers._this, &p->sourceBuffers);

    p->activeSourceBuffers._this = JS_NewObject(c, &html_media_rt->sourceBufferListClass._class, 0, 0);
    SMJS_SET_PRIVATE(c, p->activeSourceBuffers._this, &p->activeSourceBuffers);

    SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
    return JS_TRUE;
}

static DECL_FINALIZE(media_source_finalize)

    GF_HTML_MediaSource *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) return;
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
	gf_mse_mediasource_del(p, GF_TRUE);
}

static SMJS_FUNC_PROP_GET(media_source_get_source_buffers)
    GF_HTML_MediaSource *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return JS_TRUE;
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
        *vp = OBJECT_TO_JSVAL(p->sourceBuffers._this);
        return JS_TRUE;
    }
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(media_source_get_active_source_buffers)
    GF_HTML_MediaSource *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return JS_TRUE;
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
        *vp = OBJECT_TO_JSVAL(p->activeSourceBuffers._this);
        return JS_TRUE;
    }
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(media_source_get_ready_state)
    GF_HTML_MediaSource *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return JS_TRUE;
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
        switch (p->readyState)
        {
        case MEDIA_SOURCE_CLOSED:
            *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "closed"));
            break;
        case MEDIA_SOURCE_OPEN:
            *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "open"));
            break;
        case MEDIA_SOURCE_ENDED:
            *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "ended"));
            break;
        }
        return JS_TRUE;
    }
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(media_source_get_duration)
    GF_HTML_MediaSource *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return JS_TRUE;
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
        if (p->durationType == DURATION_NAN) {
            *vp = JS_GetNaNValue(c);
        } else if (p->durationType == DURATION_INFINITY) {
            *vp = JS_GetPositiveInfinityValue(c);
        } else {
            *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, p->duration));
        }
        return JS_TRUE;
    }
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(media_source_set_duration)
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourcebufferlist_get_length)
    GF_HTML_SourceBufferList *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferListClass, NULL) ) 
    {
        return JS_TRUE;
    }
    p = (GF_HTML_SourceBufferList *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
        u32 length = gf_list_count(p->list);
        *vp = INT_TO_JSVAL(length);
    }
    return JS_TRUE;
}

#define SB_BASIC_CHECK \
    GF_HTML_SourceBuffer *sb; \
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferClass, NULL) )  \
    { \
        return JS_TRUE;\
    }\
    sb = (GF_HTML_SourceBuffer *)SMJS_GET_PRIVATE(c, obj);\
    /* check if this source buffer is still in the list of source buffers */\
    if (!sb || gf_list_find(sb->mediasource->sourceBuffers.list, sb) < 0)\
    {\
        return JS_TRUE;\
    }

#define SB_UPDATING_CHECK \
    SB_BASIC_CHECK \
    if (sb->updating)\
    {\
        return JS_TRUE;\
    }\

/* FIXME : Function not used, generates warning on debian
static DECL_FINALIZE(sourcebuffer_finalize)

    GF_HTML_SourceBuffer *sb;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferClass, NULL) ) return;
    sb = (GF_HTML_SourceBuffer *)SMJS_GET_PRIVATE(c, obj);
    if (sb) {
         TODO delete lists, remove functions
        gf_free(sb);
    }
}
*/

static void gf_mse_sourcebuffer_reopen(GF_HTML_SourceBuffer *sb)
{
    if (sb->mediasource->readyState == MEDIA_SOURCE_ENDED) {
        sb->mediasource->readyState = MEDIA_SOURCE_OPEN;
    }
}

static JSBool SMJS_FUNCTION(sourcebuffer_appendBuffer)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_ArrayBuffer  *ab;
    JSObject             *js_ab;

    SB_UPDATING_CHECK
    if (sb->mediasource->readyState == MEDIA_SOURCE_CLOSED) {
        return JS_TRUE;
    } 
    gf_mse_sourcebuffer_reopen(sb);

    if (!argc || JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0])) 
    {
        return JS_TRUE;
    }
    js_ab = JSVAL_TO_OBJECT(argv[0]);
    if (!GF_JS_InstanceOf(c, js_ab, &html_media_rt->arrayBufferClass, NULL) ) 
    {
        return JS_TRUE;
    }
    ab = (GF_HTML_ArrayBuffer *)SMJS_GET_PRIVATE(c, js_ab);
    if (!ab->length)
    {
        return JS_TRUE;
    }
    gf_mse_source_buffer_append_arraybuffer(sb, ab);
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(sourcebuffer_appendStream)
{
    SMJS_OBJ
    SB_UPDATING_CHECK
    /* TODO */
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(sourcebuffer_abort)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_MediaSource_AbortMode mode;
    char *smode;
    SB_BASIC_CHECK
    if (sb->mediasource->readyState != MEDIA_SOURCE_OPEN) {
        return JS_TRUE;
    } 

    if (!argc || !JSVAL_CHECK_STRING(argv[0])) 
    {
        mode = MEDIA_SOURCE_ABORT_MODE_NONE;
    }
    smode = SMJS_CHARS(c, argv[0]);
    if (!strlen(smode))
    {
        mode = MEDIA_SOURCE_ABORT_MODE_NONE;
    } else if (!stricmp(smode, "continuation")) {
        mode = MEDIA_SOURCE_ABORT_MODE_CONTINUATION;
    } else if (!stricmp(smode, "timestampOffset")) {
        mode = MEDIA_SOURCE_ABORT_MODE_OFFSET;
    } else {
        return JS_TRUE;
    }

    if (gf_mse_source_buffer_abort(sb, mode) != GF_OK) 
    {
        return JS_TRUE;
    }

    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(sourcebuffer_remove)
{
    SMJS_OBJ
    SMJS_ARGS
    jsdouble start, end;
    SB_UPDATING_CHECK
    if (argc < 2 || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) 
    {
        return JS_TRUE;
    }
    JS_ValueToNumber(c, argv[0], &start);
    JS_ValueToNumber(c, argv[1], &end);
    if (start < 0 /* || start > sb->duration */ || start >= end) 
    {
        return JS_TRUE;
    }
    if (sb->mediasource->readyState != MEDIA_SOURCE_OPEN) 
    {
        return JS_TRUE;
    }
    sb->updating = GF_TRUE;
    if (!sb->remove_thread) {
        sb->remove_thread = gf_th_new(NULL);
    }
    gf_th_run(sb->remove_thread, gf_mse_source_buffer_remove, sb);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_updating)
    SB_BASIC_CHECK
    *vp = BOOLEAN_TO_JSVAL(sb->updating ? JS_TRUE : JS_FALSE);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_timestampOffset)
    SB_BASIC_CHECK
    *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, sb->timestampOffset));
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_timestampOffset)
    jsdouble d;
    SB_BASIC_CHECK
    JS_ValueToNumber(c, *vp, &d);
    sb->timestampOffset = d;
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_timescale)
    SB_BASIC_CHECK
    *vp = INT_TO_JSVAL(sb->timescale);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_timescale)
    SB_BASIC_CHECK
    sb->timescale = JSVAL_TO_INT(*vp);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_appendWindowStart)
    SB_UPDATING_CHECK
    *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, sb->appendWindowStart));
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_appendWindowStart)
    jsdouble d;
    SB_UPDATING_CHECK
    JS_ValueToNumber(c, *vp, &d);
    if (d < 0 || d >= sb->appendWindowEnd) {
        return JS_TRUE;
    }
    sb->appendWindowStart = d;
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_appendWindowEnd)
    SB_UPDATING_CHECK
    *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, sb->appendWindowEnd));
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_appendWindowEnd)
    jsdouble d;
    SB_UPDATING_CHECK
    JS_ValueToNumber(c, *vp, &d);
    if (d <= sb->appendWindowStart) {
        return JS_TRUE;
    }
    sb->appendWindowEnd = d;
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_buffered)
    SB_BASIC_CHECK
    gf_mse_source_buffer_update_buffered(sb);
    *vp = OBJECT_TO_JSVAL(sb->buffered._this);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_tracks)
    SB_BASIC_CHECK
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(html_url_createObjectURL)
{
    SMJS_ARGS
    JSObject    *js_ms;
    GF_HTML_MediaSource *ms;
    char        blobURI[256];

    SMJS_SET_RVAL(JSVAL_NULL);
    if (!argc || JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0])) 
    {
        return JS_TRUE;
    }
    js_ms = JSVAL_TO_OBJECT(argv[0]);
    if (!GF_JS_InstanceOf(c, js_ms, &html_media_rt->mediaSourceClass, NULL) ) 
    {
        return JS_TRUE;
    }
    ms = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, js_ms);
    sprintf(blobURI, "blob:%p", ms);
    ms->blobURI = gf_strdup(blobURI);
    SMJS_SET_RVAL(STRING_TO_JSVAL( JS_NewStringCopyZ(c, blobURI)));
    return JS_TRUE;
}

GF_HTML_ArrayBuffer *gf_arraybuffer_new(char *data, u32 length)
{
    GF_HTML_ArrayBuffer *ab = NULL;
    GF_SAFEALLOC(ab, GF_HTML_ArrayBuffer);
    if (length > 0) {
        ab->data = data;
        ab->length = length;
    }
    ab->reference_count = 0;
    return ab;
}

JSObject *gf_arraybuffer_js_new(JSContext *c, char *data, u32 length, JSObject *parent)
{
    JSObject *obj;
    GF_HTML_ArrayBuffer *p;
    if (!data) return NULL;
    p = gf_arraybuffer_new(data, length);
    obj = JS_NewObject(c, &html_media_rt->arrayBufferClass._class, NULL, parent);
    if (!parent) {
        p->reference_count = 0;
    }
    SMJS_SET_PRIVATE(c, obj, p);
    p->c = c;
    p->_this = obj;
    return obj;
}

void xhr_del_array_buffer(void *udta);

void gf_arraybuffer_del(GF_HTML_ArrayBuffer *buffer, Bool del_js) 
{
    if (buffer) {
        if (del_js && buffer->_this) {
			/*if we have a JS object, make sure the parent XHR doesn't point to us ...*/
			JSObject *xhro = JS_GetParent(buffer->c, buffer->_this); 
			if (xhro) {
				void *xhr_ctx = SMJS_GET_PRIVATE(buffer->c, xhro);
				xhr_del_array_buffer(xhr_ctx);
			}

            /* finalize the object from the JS perspective */
            buffer->c = NULL;
            buffer->_this = NULL;
        }
        /* but only delete if the data is not used elsewhere */
        if(!del_js && buffer->reference_count) {
            buffer->reference_count--;
        }
        if (!buffer->reference_count && !buffer->_this) {
            gf_free(buffer->data);
			if (buffer->url) gf_free(buffer->url);
            gf_free(buffer);
        }
    }
}

static JSBool SMJS_FUNCTION(arraybuffer_constructor)
{
    SMJS_ARGS
    u32 length = 0;
    SMJS_OBJ_CONSTRUCTOR(&html_media_rt->arrayBufferClass)

    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->arrayBufferClass, NULL)) 
    {
        return JS_TRUE;
    }
    if (argc && !JSVAL_IS_NULL(argv[0]) && JSVAL_IS_INT(argv[0]))
    {
        length = JSVAL_TO_INT(argv[0]);
    }
    if (length > 0)
    {
        gf_arraybuffer_js_new(c, (char *)gf_malloc(length), length, NULL);
    }
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(arraybuffer_get_byteLength)
    GF_HTML_ArrayBuffer *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->arrayBufferClass, NULL) ) 
    {
        return JS_TRUE;
    }
    p = (GF_HTML_ArrayBuffer *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
        *vp = INT_TO_JSVAL(p->length);
    }
    return JS_TRUE;
}

static DECL_FINALIZE(arraybuffer_finalize)

    GF_HTML_ArrayBuffer *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->arrayBufferClass, NULL) ) return;
    p = (GF_HTML_ArrayBuffer *)SMJS_GET_PRIVATE(c, obj);
    if (!p) return;
	gf_arraybuffer_del(p, GF_TRUE);
}

static JSBool SMJS_FUNCTION(js_event_add_listener)
{
    SMJS_OBJ
    SMJS_ARGS
    char                *type = NULL;
    char                *callback = NULL;
    jsval               funval = JSVAL_NULL;
    JSObject            *evt_handler = NULL;
    u32                 evtType;
    GF_DOMEventTarget   *target = NULL;
    GF_DOMListener      *listener = NULL;

    target = (GF_DOMEventTarget *)SMJS_GET_PRIVATE(c, obj);

    if (!JSVAL_CHECK_STRING(argv[0])) {
        goto err_exit;
    }
    type = SMJS_CHARS(c, argv[0]);

    if (JSVAL_CHECK_STRING(argv[1])) {
        callback = SMJS_CHARS(c, argv[1]);
        if (!callback) {
            goto err_exit;
        }
    } else if (JSVAL_IS_OBJECT(argv[1])) {
        if (JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(argv[1]))) {
            funval = argv[1];
        } else {
            JSBool found;
            jsval evt_fun;
            evt_handler = JSVAL_TO_OBJECT(argv[1]);
            found = JS_GetProperty(c, evt_handler, "handleEvent", &evt_fun);
            if (!found || !JSVAL_IS_OBJECT(evt_fun) || !JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(evt_fun))) {
                goto err_exit;
            }
            funval = evt_fun;
        }
    }

    evtType = gf_dom_event_type_by_name(type);
    if (evtType==GF_EVENT_UNKNOWN) {
        goto err_exit;
    }

    GF_SAFEALLOC(listener, GF_DOMListener);
    if (!callback) {
        listener->js_fun_val = *(u64 *) &funval;
        if (listener->js_fun_val) {
            listener->js_context = c;
            /*protect the function - we don't know how it was passed to us, so prevent it from being GCed*/
            gf_js_add_root((JSContext *)listener->js_context, &listener->js_fun_val, GF_JSGC_VAL);
        }
        listener->evt_listen_obj = evt_handler;
    } else {
        listener->callback = gf_strdup(callback);
    }
    gf_list_add(target->listeners, listener);

err_exit:
    SMJS_FREE(c, type);
    SMJS_FREE(c, callback);
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(js_event_remove_listener)
{
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(js_event_dispatch)
{
    return JS_TRUE;
}


void html_media_source_init_js_api(JSContext *js_ctx, JSObject *global, GF_HTML_MediaRuntime *_html_media_rt)
{
    /* HTML Media Element */
    if (_html_media_rt) {
        html_media_rt = _html_media_rt;

        JS_SETUP_CLASS(html_media_rt->arrayBufferClass, "ArrayBuffer", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, arraybuffer_finalize);
        {
            JSPropertySpec arrayBufferClassProps[] = {
                {"byteLength",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, arraybuffer_get_byteLength, 0},
                {0, 0, 0, 0, 0}
            };
            GF_JS_InitClass(js_ctx, global, 0, &html_media_rt->arrayBufferClass, arraybuffer_constructor, 0, arrayBufferClassProps, 0, 0, 0);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTML Media Source API] ArrayBuffer class initialized\n"));
        }

        JS_SETUP_CLASS(html_media_rt->mediaSourceClass, "MediaSource", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, media_source_finalize);
        {
            JSPropertySpec htmlMediaSourceClassProps[] = {
                {"sourceBuffers",       HTML_MEDIASOURCE_PROP_SOURCEBUFFERS,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, media_source_get_source_buffers, 0},
                {"activeSourceBuffers", HTML_MEDIASOURCE_PROP_ACTIVESOURCEBUFFERS, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, media_source_get_active_source_buffers, 0},
                {"readyState",          HTML_MEDIASOURCE_PROP_READYSTATE,          JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, media_source_get_ready_state, 0},
                {"duration",            HTML_MEDIASOURCE_PROP_DURATION,            JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, media_source_get_duration, media_source_set_duration},
                {0, 0, 0, 0, 0}
            };
            JSFunctionSpec htmlMediaSourceClassFuncs[] = {
                SMJS_FUNCTION_SPEC("addSourceBuffer", mediasource_addSourceBuffer, 1),
                SMJS_FUNCTION_SPEC("removeSourceBuffer", mediasource_removeSourceBuffer, 1),
                SMJS_FUNCTION_SPEC("endOfStream", mediasource_endOfStream, 1),
                SMJS_FUNCTION_SPEC("addEventListener", js_event_add_listener, 3),
                SMJS_FUNCTION_SPEC("removeEventListener", js_event_remove_listener, 3),
                SMJS_FUNCTION_SPEC("dispatchEvent", js_event_dispatch, 1),
                SMJS_FUNCTION_SPEC(0, 0, 0)
            };
            JSFunctionSpec htmlMediaSourceClassStaticFuncs[] = {
                SMJS_FUNCTION_SPEC("isTypeSupported", mediasource_is_type_supported, 1),
                SMJS_FUNCTION_SPEC(0, 0, 0)
            };
            GF_JS_InitClass(js_ctx, global, 0, &html_media_rt->mediaSourceClass, media_source_constructor, 0, htmlMediaSourceClassProps, htmlMediaSourceClassFuncs, 0, htmlMediaSourceClassStaticFuncs);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTML Media Source API] MediaSource class initialized\n"));
        }

        JS_SETUP_CLASS(html_media_rt->sourceBufferClass, "SourceBuffer", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_PropertyStub);
        {
            JSPropertySpec SourceBufferClassProps[] = {
                {"updating",          HTML_SOURCEBUFFER_PROP_UPDATING,         JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, sourceBuffer_get_updating, 0},
                {"buffered",          HTML_SOURCEBUFFER_PROP_BUFFERED,         JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, sourceBuffer_get_buffered, 0},
                {"timestampOffset",   HTML_SOURCEBUFFER_PROP_TIMESTAMPOFFSET,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, sourceBuffer_get_timestampOffset, sourceBuffer_set_timestampOffset},
                {"timescale",         HTML_SOURCEBUFFER_PROP_TIMESCALE,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, sourceBuffer_get_timescale, sourceBuffer_set_timescale},
                {"appendWindowStart", HTML_SOURCEBUFFER_PROP_APPENDWINDOWSTART,JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, sourceBuffer_get_appendWindowStart, sourceBuffer_set_appendWindowStart},
                {"appendWindowEnd",   HTML_SOURCEBUFFER_PROP_APPENDWINDOWEND,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, sourceBuffer_get_appendWindowEnd, sourceBuffer_set_appendWindowEnd},
                {"audioTracks",       HTML_SOURCEBUFFER_PROP_AUDIOTRACKS,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, sourceBuffer_get_tracks, 0},
                {"videoTracks",       HTML_SOURCEBUFFER_PROP_VIDEOTRACKS,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, sourceBuffer_get_tracks, 0},
                {"textTracks",        HTML_SOURCEBUFFER_PROP_TEXTTRACKS,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, sourceBuffer_get_tracks, 0},
                {0, 0, 0, 0, 0}
            };
            JSFunctionSpec SourceBufferClassFuncs[] = {
                SMJS_FUNCTION_SPEC("appendBuffer", sourcebuffer_appendBuffer, 1),
                SMJS_FUNCTION_SPEC("appendStream", sourcebuffer_appendStream, 1),
                SMJS_FUNCTION_SPEC("abort",  sourcebuffer_abort, 0),
                SMJS_FUNCTION_SPEC("remove", sourcebuffer_remove, 2),
                SMJS_FUNCTION_SPEC(0, 0, 0)
            };
            GF_JS_InitClass(js_ctx, global, 0, &html_media_rt->sourceBufferClass, 0, 0, SourceBufferClassProps, SourceBufferClassFuncs, 0, 0);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTML Media Source API] SourceBuffer class initialized\n"));
        }

        JS_SETUP_CLASS(html_media_rt->sourceBufferListClass, "SourceBufferList", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_PropertyStub);
        {
            JSPropertySpec SourceBufferListClassProps[] = {
                {"length",        HTML_SOURCEBUFFERLIST_PROP_LENGTH,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, sourcebufferlist_get_length, 0},
                {0, 0, 0, 0, 0}
            };
            JSFunctionSpec SourceBufferListClassFuncs[] = {
                SMJS_FUNCTION_SPEC(0, 0, 0)
            };
            GF_JS_InitClass(js_ctx, global, 0, &html_media_rt->sourceBufferListClass, 0, 0, SourceBufferListClassProps, SourceBufferListClassFuncs, 0, 0);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTML Media Source API] SourceBufferList class initialized\n"));
        }

        JS_SETUP_CLASS(html_media_rt->URLClass, "URL", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_PropertyStub);
        {
            JSPropertySpec URLClassProps[] = {
                {0, 0, 0, 0, 0}
            };
            JSFunctionSpec URLClassFuncs[] = {
                SMJS_FUNCTION_SPEC("createObjectURL", html_url_createObjectURL, 1),
                SMJS_FUNCTION_SPEC(0, 0, 0)
            };
            GF_JS_InitClass(js_ctx, global, 0, &html_media_rt->URLClass, 0, 0, URLClassProps, 0, 0, URLClassFuncs);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTML Media Source API] URL class initialized\n"));
        }
    }
}


#endif  /*GPAC_HAS_SPIDERMONKEY*/

#endif  /*GPAC_DISABLE_SVG*/
