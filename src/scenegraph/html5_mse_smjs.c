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
    HTML_MEDIASOURCE_PROP_SOURCEBUFFERS         = -1,
    HTML_MEDIASOURCE_PROP_ACTIVESOURCEBUFFERS   = -2,
    HTML_MEDIASOURCE_PROP_DURATION              = -3,
    HTML_MEDIASOURCE_PROP_READYSTATE            = -4,
    /* SourceBuffer properties */
    HTML_SOURCEBUFFER_PROP_MODE					= -5,
    HTML_SOURCEBUFFER_PROP_UPDATING             = -6,
    HTML_SOURCEBUFFER_PROP_BUFFERED             = -7,
    HTML_SOURCEBUFFER_PROP_TIMESTAMPOFFSET      = -8,
    HTML_SOURCEBUFFER_PROP_TIMESCALE            = -9,
    HTML_SOURCEBUFFER_PROP_AUDIOTRACKS          = -10,
    HTML_SOURCEBUFFER_PROP_VIDEOTRACKS          = -11,
    HTML_SOURCEBUFFER_PROP_TEXTTRACKS           = -12,
    HTML_SOURCEBUFFER_PROP_APPENDWINDOWSTART    = -13,
    HTML_SOURCEBUFFER_PROP_APPENDWINDOWEND      = -14,
    /* SourceBufferList properties */
    HTML_SOURCEBUFFERLIST_PROP_LENGTH           = -15,
} GF_HTML_MediaSourcePropEnum;

static GF_HTML_MediaRuntime *html_media_rt = NULL;

Bool gf_mse_is_mse_object(JSContext *c, JSObject *obj) {
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) return GF_TRUE;
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferClass, NULL) ) return GF_TRUE;
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferListClass, NULL) ) return GF_TRUE;
	return GF_FALSE;
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

void gf_mse_get_event_target(JSContext *c, JSObject *obj, GF_DOMEventTarget **target, GF_SceneGraph **sg) {
	if (!sg || !target) return;
	*sg = mediasource_get_scenegraph(c);
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
		GF_HTML_MediaSource *ms = NULL;
		ms = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
		*target = ms->evt_target;
	} else if (GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferClass, NULL) ) {
		GF_HTML_SourceBuffer *sb = NULL;
		sb = (GF_HTML_SourceBuffer *)SMJS_GET_PRIVATE(c, obj);
		*target = sb->evt_target;
	} else if (GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferListClass, NULL) ) {
		GF_HTML_SourceBufferList *sbl = NULL;
		sbl = (GF_HTML_SourceBufferList *)SMJS_GET_PRIVATE(c, obj);
		*target = sbl->evt_target;
	} else {
		*target = NULL;
		*sg = NULL;
	}
}

static void mediasource_sourceBuffer_initjs(JSContext *c, JSObject *ms_obj, GF_HTML_SourceBuffer *sb)
{
    sb->_this = JS_NewObject(c, &html_media_rt->sourceBufferClass._class, 0, 0);
    //gf_js_add_root(c, &sb->_this, GF_JSGC_OBJECT);
    SMJS_SET_PRIVATE(c, sb->_this, sb);
    sb->buffered->_this = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, sb->_this);
    SMJS_SET_PRIVATE(c, sb->buffered->_this, sb->buffered);
}

#include <gpac/internal/terminal_dev.h>
Bool gf_term_is_type_supported(GF_Terminal *term, const char* mime);

static JSBool SMJS_FUNCTION(mediasource_is_type_supported)
{
    SMJS_ARGS
    GF_SceneGraph *sg;
    GF_JSAPIParam par;
    Bool isSupported = GF_TRUE;
    char *mime;
    if (!argc || !JSVAL_CHECK_STRING(argv[0])) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    mime = SMJS_CHARS(c, argv[0]);
    sg = mediasource_get_scenegraph(c);
	assert(sg);
	if (!strlen(mime)) { 
		isSupported = GF_FALSE;
	} else {
		sg->script_action(sg->script_action_cbck, GF_JSAPI_OP_GET_TERM, NULL, &par);
		isSupported = gf_term_is_type_supported((GF_Terminal *)par.term, mime);
	}
    SMJS_SET_RVAL(BOOLEAN_TO_JSVAL(isSupported ? JS_TRUE : JS_FALSE));
	SMJS_FREE(c, mime);
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(mediasource_addSourceBuffer)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_SourceBuffer    *sb;
    GF_HTML_MediaSource     *ms;
    const char              *mime;
	GF_Err					e;
	u32						exception = 0;

	e = GF_OK;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    if (!argc || !JSVAL_CHECK_STRING(argv[0])) 
    {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    mime = SMJS_CHARS(c, argv[0]);
    if (!strlen(mime))     {
		exception = GF_DOM_EXC_INVALID_ACCESS_ERR;
		goto exit;
    }
    ms = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
	if (!ms) {
		exception = GF_DOM_EXC_INVALID_ACCESS_ERR;
		goto exit;
	} else if (ms->readyState != MEDIA_SOURCE_READYSTATE_OPEN) {
		exception = GF_DOM_EXC_INVALID_STATE_ERR;
		goto exit;
    }
    assert(ms->service);
    /* 
	if (gf_list_count(ms->sourceBuffers.list) > 0) {
		dom_throw_exception(c, GF_DOM_EXC_QUOTA_EXCEEDED_ERR);
		e = GF_BAD_PARAM;
		goto exit;
	}
	*/
    sb = gf_mse_source_buffer_new(ms);
	assert(sb);
    e = gf_mse_source_buffer_load_parser(sb, mime);
	if (e == GF_OK) {
	    gf_mse_mediasource_add_source_buffer(ms, sb);
		mediasource_sourceBuffer_initjs(c, obj, sb);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(sb->_this) );
	} else {
		gf_mse_source_buffer_del(sb);
		exception = GF_DOM_EXC_NOT_SUPPORTED_ERR;
	}
exit:
	if (mime) { 
		SMJS_FREE(c, (void *)mime);
	}
	if (exception) {
		return dom_throw_exception(c, exception);
	} else {
		return JS_TRUE;
	}
}

static JSBool SMJS_FUNCTION(mediasource_removeSourceBuffer)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_MediaSource *ms;
	GF_HTML_SourceBuffer *sb;
	JSObject *sb_obj;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    ms = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
	if (!ms) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
    if (!argc || JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0])) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    sb_obj = JSVAL_TO_OBJECT(argv[0]);
    if (!GF_JS_InstanceOf(c, sb_obj, &html_media_rt->sourceBufferClass, NULL) ) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    sb = (GF_HTML_SourceBuffer *)SMJS_GET_PRIVATE(c, sb_obj);
	if (!sb) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	} else {
		GF_Err e = gf_mse_remove_source_buffer(ms, sb);
		if (e == GF_NOT_FOUND) {
			return dom_throw_exception(c, GF_DOM_EXC_NOT_FOUND_ERR);
		}
	}
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(mediasource_endOfStream)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_MediaSource *ms;
	u32 i;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL) ) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    ms = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
	if (!ms) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	if (ms->readyState != MEDIA_SOURCE_READYSTATE_OPEN) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
	}
	for (i = 0; i < gf_list_count(ms->sourceBuffers.list); i++) {
		GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)gf_list_get(ms->sourceBuffers.list, i);
		if (sb->updating) {
			return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
		}
	}
    if (argc > 0)  {
		char *error = NULL;
		if (!JSVAL_CHECK_STRING(argv[0])) {
			return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
		}
	    error = SMJS_CHARS(c, argv[0]);
		if (strcmp(error, "decode") && strcmp(error, "network")) {
			SMJS_FREE(c, error);
			return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
		}
		SMJS_FREE(c, error);
    }
	gf_mse_mediasource_end(ms);
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
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (!p) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	} else {
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
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    p = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (!p) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    } else {
        switch (p->readyState)
        {
        case MEDIA_SOURCE_READYSTATE_CLOSED:
            *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "closed"));
            break;
        case MEDIA_SOURCE_READYSTATE_OPEN:
            *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "open"));
            break;
        case MEDIA_SOURCE_READYSTATE_ENDED:
            *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "ended"));
            break;
        }
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
        if (p->readyState == MEDIA_SOURCE_READYSTATE_CLOSED || p->durationType == DURATION_NAN) {
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
    GF_HTML_MediaSource *ms;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->mediaSourceClass, NULL)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    ms = (GF_HTML_MediaSource *)SMJS_GET_PRIVATE(c, obj);
    if (!ms) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    } else {
        if (ms->readyState != MEDIA_SOURCE_READYSTATE_OPEN) {
			return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
		} else if (!JSVAL_IS_NUMBER(*vp)) {
			return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
		} else {
			u32 i, count;
			count = gf_list_count(ms->sourceBuffers.list);
			for (i = 0; i < count; i++) {
				GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)gf_list_get(ms->sourceBuffers.list, i);
				if (sb->updating) { 
					return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
				}
			}
			{
				jsdouble durationValue;
				JS_ValueToNumber(c, *vp, &durationValue);
				if (durationValue < 0) {
					return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
				} else {
					ms->duration = durationValue;
					ms->durationType = DURATION_VALUE;
					/* TODO: call the run duration algorithm */
				}
			}
		}
	}
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( sourcebufferlist_getProperty)
    GF_HTML_SourceBufferList *p;
	u32 count;
	u32 idx;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferListClass, NULL) ) 
    {
        return JS_TRUE;
    }
    p = (GF_HTML_SourceBufferList *)SMJS_GET_PRIVATE(c, obj);
    if (p) {
		count = gf_list_count(p->list);
		if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

		switch (SMJS_ID_TO_INT(id)) {
		case HTML_SOURCEBUFFERLIST_PROP_LENGTH:
			*vp = INT_TO_JSVAL(count);
			break;
		default:
			idx = SMJS_ID_TO_INT(id);
			if ((idx<0) || ((u32) idx>=count)) {
				*vp = JSVAL_VOID;
				return JS_TRUE;
			} else {
				GF_HTML_SourceBuffer *sb;
				sb = (GF_HTML_SourceBuffer *)gf_list_get(p->list, idx);
				*vp = OBJECT_TO_JSVAL(sb->_this);
				return JS_TRUE;
			}

		}
    }
    return JS_TRUE;

}


#define SB_BASIC_CHECK \
    GF_HTML_SourceBuffer *sb; \
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->sourceBufferClass, NULL) )  \
    { \
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR); \
    }\
    sb = (GF_HTML_SourceBuffer *)SMJS_GET_PRIVATE(c, obj);\
    if (!sb)\
    {\
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR); \
    }

#define SB_UPDATING_CHECK \
    SB_BASIC_CHECK \
    /* check if this source buffer is still in the list of source buffers */\
    if (gf_list_find(sb->mediasource->sourceBuffers.list, sb) < 0)\
    {\
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR); \
    } \
    if (sb->updating)\
    {\
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR); \
    }

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

static JSBool SMJS_FUNCTION(sourcebuffer_appendBuffer)
{
    SMJS_OBJ
    SMJS_ARGS
    GF_HTML_ArrayBuffer  *ab;
    JSObject             *js_ab;

    SB_UPDATING_CHECK
    if (sb->mediasource->readyState == MEDIA_SOURCE_READYSTATE_CLOSED) {
        return JS_TRUE;
    } else if (sb->mediasource->readyState == MEDIA_SOURCE_READYSTATE_ENDED) {
		gf_mse_mediasource_open(sb->mediasource, NULL);
    }

    if (!argc || JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0])) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    js_ab = JSVAL_TO_OBJECT(argv[0]);
    if (!GF_JS_InstanceOf(c, js_ab, &html_media_rt->arrayBufferClass, NULL) ) 
    {
		return dom_throw_exception(c, GF_DOM_EXC_TYPE_MISMATCH_ERR);
    }
    ab = (GF_HTML_ArrayBuffer *)SMJS_GET_PRIVATE(c, js_ab);
    if (!ab->length)
    {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    /* TODO: handle buffer full flag case */
	/* TODO: run the coded frame eviction algo */
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
    SB_BASIC_CHECK
    if (gf_list_find(sb->mediasource->sourceBuffers.list, sb) < 0) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
    }
    if (sb->mediasource->readyState != MEDIA_SOURCE_READYSTATE_OPEN) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
    }
    if (gf_mse_source_buffer_abort(sb) != GF_OK) {
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
    if (argc < 2 || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    JS_ValueToNumber(c, argv[0], &start);
    JS_ValueToNumber(c, argv[1], &end);
    if (start < 0 /* || start > sb->duration */ || start >= end) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    if (sb->mediasource->readyState != MEDIA_SOURCE_READYSTATE_OPEN) {
		gf_mse_mediasource_open(sb->mediasource, NULL);
    }
	gf_mse_remove(sb, start, end);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_mode)
    SB_BASIC_CHECK
	if (sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEGMENTS) {
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "segments"));
	} else if (sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEQUENCE) {
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "sequence"));
	}
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_mode)
	char *smode = NULL;
	GF_HTML_MediaSource_AppendMode mode;
    SB_BASIC_CHECK
    if (!JSVAL_CHECK_STRING(*vp)) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    smode = SMJS_CHARS(c, *vp);
	if (stricmp(smode, "segments") && stricmp(smode, "sequence")) {
		SMJS_FREE(c, smode);
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	if (!stricmp(smode, "segments")) {
        mode = MEDIA_SOURCE_APPEND_MODE_SEGMENTS;
    } else if (!stricmp(smode, "sequence")) {
        mode = MEDIA_SOURCE_APPEND_MODE_SEQUENCE;
    }
	SMJS_FREE(c, smode);
    if (gf_list_find(sb->mediasource->sourceBuffers.list, sb) < 0) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
    }
    if (sb->updating) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
    }
	if (sb->mediasource->readyState == MEDIA_SOURCE_READYSTATE_ENDED) {
		gf_mse_mediasource_open(sb->mediasource, NULL);
	}
	if (sb->append_state == MEDIA_SOURCE_APPEND_STATE_PARSING_MEDIA_SEGMENT) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
	}
    sb->append_mode = mode;
	if (sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEQUENCE) {
		sb->group_start_timestamp_flag = GF_TRUE;
		sb->group_start_timestamp = sb->group_end_timestamp;
	}
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_updating)
    SB_BASIC_CHECK
    *vp = BOOLEAN_TO_JSVAL(sb->updating ? JS_TRUE : JS_FALSE);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_timestampOffset)
    SB_BASIC_CHECK
    *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, sb->timestampOffset*1.0/sb->timescale));
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_timestampOffset)
    jsdouble d;
    SB_UPDATING_CHECK
	if (sb->mediasource->readyState == MEDIA_SOURCE_READYSTATE_ENDED) {
		gf_mse_mediasource_open(sb->mediasource, NULL);
	}
	if (sb->append_state == MEDIA_SOURCE_APPEND_STATE_PARSING_MEDIA_SEGMENT) {
        return dom_throw_exception(c, GF_DOM_EXC_INVALID_STATE_ERR);
	}
    JS_ValueToNumber(c, *vp, &d);
	gf_mse_source_buffer_set_timestampOffset(sb, d);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_timescale)
    SB_BASIC_CHECK
    *vp = INT_TO_JSVAL(sb->timescale);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_timescale)
    SB_BASIC_CHECK
    gf_mse_source_buffer_set_timescale(sb, JSVAL_TO_INT(*vp));
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_appendWindowStart)
    SB_BASIC_CHECK
    *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, sb->appendWindowStart));
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_appendWindowStart)
    jsdouble d;
    SB_UPDATING_CHECK
	if (!JSVAL_IS_NUMBER(*vp)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
    JS_ValueToNumber(c, *vp, &d);
    if (d < 0 || d >= sb->appendWindowEnd) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    sb->appendWindowStart = d;
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_appendWindowEnd)
    SB_BASIC_CHECK
	if (sb->appendWindowEnd == GF_MAX_DOUBLE) {
		*vp = JS_GetPositiveInfinityValue(c);
	} else {
		*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, sb->appendWindowEnd));
	}
    return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(sourceBuffer_set_appendWindowEnd)
    jsdouble d;
    SB_UPDATING_CHECK
	if (!JSVAL_IS_NUMBER(*vp)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
    JS_ValueToNumber(c, *vp, &d);
    if (d <= sb->appendWindowStart) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
    }
    sb->appendWindowEnd = d;
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_buffered)
    SB_BASIC_CHECK
    gf_mse_source_buffer_update_buffered(sb);
    *vp = OBJECT_TO_JSVAL(sb->buffered->_this);
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(sourceBuffer_get_tracks)
    SB_BASIC_CHECK
	/* TODO */
    return JS_TRUE;
}

static JSBool SMJS_FUNCTION(html_url_createObjectURL)
{
    SMJS_ARGS
    JSObject    *js_ms;
    GF_HTML_MediaSource *ms;
    char        blobURI[256];

    SMJS_SET_RVAL(JSVAL_NULL);
    if (!argc || JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0])) {
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
    if (argc && !JSVAL_IS_NULL(argv[0]) && JSVAL_IS_INT(argv[0])) {
        length = JSVAL_TO_INT(argv[0]);
    }
    if (length > 0) {
        gf_arraybuffer_js_new(c, (char *)gf_malloc(length), length, NULL);
    }
    return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(arraybuffer_get_byteLength)
    GF_HTML_ArrayBuffer *p;
    if (!GF_JS_InstanceOf(c, obj, &html_media_rt->arrayBufferClass, NULL) ) {
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

static JSBool SMJS_FUNCTION(mse_event_add_listener)
{
	JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_add_listener, GF_Node *vrml_node);
	return gf_sg_js_event_add_listener(SMJS_CALL_ARGS, NULL);
}

static JSBool SMJS_FUNCTION(mse_event_remove_listener)
{
	JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_remove_listener, GF_Node *vrml_node);
	return gf_sg_js_event_remove_listener(SMJS_CALL_ARGS, NULL);
}

static JSBool SMJS_FUNCTION(mse_event_dispatch)
{
	/* TODO */
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
                {"byteLength",       -1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, arraybuffer_get_byteLength, 0},
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
                SMJS_FUNCTION_SPEC("addEventListener", mse_event_add_listener, 3),
                SMJS_FUNCTION_SPEC("removeEventListener", mse_event_remove_listener, 3),
                SMJS_FUNCTION_SPEC("dispatchEvent", mse_event_dispatch, 1),
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
                {"mode",			  HTML_SOURCEBUFFER_PROP_MODE,             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, sourceBuffer_get_mode, sourceBuffer_set_mode},
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
                SMJS_FUNCTION_SPEC("addEventListener", mse_event_add_listener, 3),
                SMJS_FUNCTION_SPEC("removeEventListener", mse_event_add_listener, 3),
                SMJS_FUNCTION_SPEC("dispatchEvent", mse_event_add_listener, 1),
                SMJS_FUNCTION_SPEC(0, 0, 0)
            };
            GF_JS_InitClass(js_ctx, global, 0, &html_media_rt->sourceBufferClass, 0, 0, SourceBufferClassProps, SourceBufferClassFuncs, 0, 0);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTML Media Source API] SourceBuffer class initialized\n"));
        }

        JS_SETUP_CLASS(html_media_rt->sourceBufferListClass, "SourceBufferList", JSCLASS_HAS_PRIVATE, sourcebufferlist_getProperty, JS_PropertyStub_forSetter, JS_PropertyStub);
        {
            JSPropertySpec SourceBufferListClassProps[] = {
                {"length",        HTML_SOURCEBUFFERLIST_PROP_LENGTH,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
                {0, 0, 0, 0, 0}
            };
            JSFunctionSpec SourceBufferListClassFuncs[] = {
                SMJS_FUNCTION_SPEC("addEventListener", mse_event_add_listener, 3),
                SMJS_FUNCTION_SPEC("removeEventListener", mse_event_add_listener, 3),
                SMJS_FUNCTION_SPEC("dispatchEvent", mse_event_add_listener, 1),
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
