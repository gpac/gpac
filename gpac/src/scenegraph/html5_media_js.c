/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2007-2012
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


#include <gpac/internal/scenegraph_dev.h>

/*base SVG type*/
#include <gpac/nodes_svg.h>
/*dom events*/
#include <gpac/events.h>
/*dom text event*/
#include <gpac/utf.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/xml.h>

#ifndef GPAC_DISABLE_SVG


#ifdef GPAC_CONFIG_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif /* XP_UNIX */
#endif

#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/internal/smjs_api.h>
#include <gpac/html5_media.h>

/***********************************************************
 *
 * DOM generic functions being used
 *
 ***********************************************************/
#ifdef USE_FFDEV_15
void dom_element_finalize(JSFreeOp *fop, JSObject *obj);
void dom_document_finalize(JSFreeOp *fop, JSObject *obj);
#else
void dom_element_finalize(JSContext *c, JSObject *obj);
void dom_document_finalize(JSContext *c, JSObject *obj);
#endif

void gf_svg_set_attributeNS(GF_Node *n, u32 ns_code, char *name, char *val);

#define JSVAL_CHECK_STRING(_v) (JSVAL_IS_STRING(_v) || JSVAL_IS_NULL(_v))

/* including terminal dev for MediaObject */

/* including Compositor dev for TextureHandler and AudioInput */
#include <gpac/internal/compositor_dev.h>

static GF_HTML_MediaRuntime *html_media_rt = NULL;

/* Enumeration of the property codes for use in JS Bindings for the the HTML5 MediaError interface */
typedef enum
{
	MEDIA_ERROR_PROP_ABORTED            = -1,
	MEDIA_ERROR_PROP_NETWORK            = -2,
	MEDIA_ERROR_PROP_DECODE             = -3,
	MEDIA_ERROR_PROP_SRC_NOT_SUPPORTED  = -4,
	MEDIA_ERROR_PROP_CODE               = -5
} GF_HTML_MediaErrorPropEnum;

/* Enumeration of the property codes for use in JS Bindings for the the HTML5 HTMLMediaElement interface */
typedef enum
{
	HTML_MEDIA_PROP_SRC                         = -1,
	HTML_MEDIA_PROP_CURRENTSRC                  = -2,
	HTML_MEDIA_PROP_CROSSORIGIN                 = -3,
	HTML_MEDIA_PROP_NETWORKSTATE                = -4,
	HTML_MEDIA_PROP_PRELOAD                     = -5,
	HTML_MEDIA_PROP_BUFFERED                    = -6,
	HTML_MEDIA_PROP_READYSTATE                  = -7,
	HTML_MEDIA_PROP_SEEKING                     = -8,
	HTML_MEDIA_PROP_CURRENTTIME                 = -9,
	HTML_MEDIA_PROP_DURATION                    = -10,
	HTML_MEDIA_PROP_STARTDATE                   = -11,
	HTML_MEDIA_PROP_PAUSED                      = -12,
	HTML_MEDIA_PROP_DEFAULTPLAYBACKRATE         = -13,
	HTML_MEDIA_PROP_PLAYBACKRATE                = -14,
	HTML_MEDIA_PROP_PLAYED                      = -15,
	HTML_MEDIA_PROP_SEEKABLE                    = -16,
	HTML_MEDIA_PROP_ENDED                       = -17,
	HTML_MEDIA_PROP_AUTOPLAY                    = -18,
	HTML_MEDIA_PROP_LOOP                        = -19,
	HTML_MEDIA_PROP_MEDIAGROUP                  = -20,
	HTML_MEDIA_PROP_CONTROLLER                  = -21,
	HTML_MEDIA_PROP_CONTROLS                    = -22,
	HTML_MEDIA_PROP_VOLUME                      = -23,
	HTML_MEDIA_PROP_MUTED                       = -24,
	HTML_MEDIA_PROP_DEFAULTMUTED                = -25,
	HTML_MEDIA_PROP_AUDIOTRACKS                 = -26,
	HTML_MEDIA_PROP_VIDEOTRACKS                 = -27,
	HTML_MEDIA_PROP_TEXTTRACKS                  = -28,
	HTML_MEDIA_PROP_NETWORK_EMPTY               = -29,
	HTML_MEDIA_PROP_NETWORK_IDLE                = -30,
	HTML_MEDIA_PROP_NETWORK_LOADING             = -31,
	HTML_MEDIA_PROP_NETWORK_NO_SOURCE           = -32,
	HTML_MEDIA_PROP_HAVE_NOTHING                = -33,
	HTML_MEDIA_PROP_HAVE_METADATA               = -34,
	HTML_MEDIA_PROP_HAVE_CURRENT_DATA           = -35,
	HTML_MEDIA_PROP_HAVE_FUTURE_DATA            = -36,
	HTML_MEDIA_PROP_HAVE_ENOUGH_DATA            = -37,
	HTML_MEDIA_PROP_ERROR                       = -38
} GF_HTML_MediaEltPropEnum;

/* Enumeration of the property codes for use in JS Bindings for the the HTML5 HTMLVideoElement interface */
typedef enum {
	HTML_VIDEO_PROP_WIDTH                       = -1,
	HTML_VIDEO_PROP_HEIGHT                      = -2,
	HTML_VIDEO_PROP_VIDEOWIDTH                  = -3,
	HTML_VIDEO_PROP_VIDEOHEIGHT                 = -4,
	HTML_VIDEO_PROP_POSTER                      = -5
} GF_HTML_VideoEltPropEnum;

/* Enumeration of the property codes for use in JS Bindings for the the HTML5 HTMLTrackElement interface */
typedef enum {
	HTML_TRACK_PROP_ID                          = -1,
	HTML_TRACK_PROP_KIND                        = -2,
	HTML_TRACK_PROP_LABEL                       = -3,
	HTML_TRACK_PROP_LANGUAGE                    = -4,
	HTML_TRACK_PROP_SELECTED                    = -5,
	HTML_TRACK_PROP_ENABLED                     = -6,
	HTML_TRACK_PROP_INBANDTYPE                  = -7
} GF_HTML_TrackPropEnum;

#define HTML_MEDIA_JS_CHECK     GF_HTML_MediaElement *me; \
                                GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj); \
                                if (!n || (n->sgprivate->tag != TAG_SVG_video && n->sgprivate->tag != TAG_SVG_audio)) \
                                { \
									return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR); \
                                } \
                                me = (GF_HTML_MediaElement *)html_media_element_get_from_node(c, n); \
                                if (!me) { \
									return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR); \
                                }

#define HTML_MEDIA_JS_START     HTML_MEDIA_JS_CHECK

/* Structure copied over from svg_media.c
   TODO : fix this - remove the copy */
typedef struct
{
	GF_TextureHandler txh;
	struct _drawable *graph;
	MFURL txurl;
	Bool first_frame_fetched;
	GF_Node *audio;
	Bool audio_dirty;
	Bool stop_requested;
} SVG_video_stack;

/* Structure copied over from svg_media.c
   TODO : fix this - remove the copy */
typedef struct
{
	GF_AudioInput input;
	Bool is_active, is_error;
	MFURL aurl;
} SVG_audio_stack;

/* Gets the GF_MediaObject associated with this node
 *
 *  \param n the node either an SVG/HTML5 video or audio element
 *  \return the GF_MediaObject attached to this node or null if none
 */
static GF_MediaObject *gf_html_media_object(GF_Node *n)
{
	GF_MediaObject *mo = NULL;
	if (n->sgprivate->tag == TAG_SVG_video)
	{
		SVG_video_stack *video_stack;
		video_stack = (SVG_video_stack *)n->sgprivate->UserPrivate;
		mo = video_stack->txh.stream;
	}
	else if (n->sgprivate->tag == TAG_SVG_audio)
	{
		SVG_audio_stack *audio_stack;
		audio_stack = (SVG_audio_stack *)n->sgprivate->UserPrivate;
		mo = audio_stack->input.stream;
	}
	return mo;
}

static void gf_html_track_init_js(GF_HTML_Track *track, JSContext *c, GF_HTML_TrackList *tracklist)
{
	JSObject *track_obj;
	track->c     = c;
	if (track->type == HTML_MEDIA_TRACK_TYPE_AUDIO)
	{
		track_obj = JS_NewObject(c, &html_media_rt->audioTrackClass._class, NULL, tracklist->_this);
	}
	else if (track->type == HTML_MEDIA_TRACK_TYPE_VIDEO)
	{
		track_obj = JS_NewObject(c, &html_media_rt->videoTrackClass._class, NULL, tracklist->_this);
	}
	else if (track->type == HTML_MEDIA_TRACK_TYPE_TEXT)
	{
		track_obj = JS_NewObject(c, &html_media_rt->textTrackClass._class, NULL, tracklist->_this);
	}
	else
	{
		return;
	}
	tracklist->onchange = JSVAL_NULL;
	tracklist->onaddtrack = JSVAL_NULL;
	tracklist->onremovetrack = JSVAL_NULL;
	track->_this = track_obj;
	SMJS_SET_PRIVATE(c, track->_this, track);
}

/* Not yet used, removed for GCC warning
static void gf_html_media_controller_init_js(GF_HTML_MediaController *mc, JSContext *c)
{
    mc->c            = c;
    mc->_this        = JS_NewObject(c, &html_media_rt->mediaControllerClass._class, NULL, NULL);
    SMJS_SET_PRIVATE(c, mc->_this, mc);
    mc->buffered->c              = c;
    mc->buffered->_this          = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, mc->_this);
    SMJS_SET_PRIVATE(c, mc->buffered->_this, mc->buffered);
    mc->played->c                = c;
    mc->played->_this            = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, mc->_this);
    SMJS_SET_PRIVATE(c, mc->played->_this, mc->played);
    mc->seekable->c              = c;
    mc->seekable->_this          = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, mc->_this);
    SMJS_SET_PRIVATE(c, mc->seekable->_this, mc->seekable);
}
*/

static void gf_html_media_element_init_js(GF_HTML_MediaElement *me, JSContext *c, JSObject *node_obj)
{
	me->c                       = c;
	me->_this                   = JS_NewObject(c, &html_media_rt->htmlMediaElementClass._class, NULL, node_obj);
	SMJS_SET_PRIVATE(c, me->_this, me);

	me->error.c                 = c;
	me->error._this             = JS_NewObject(c, &html_media_rt->mediaErrorClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->error._this, &me->error);

	me->audioTracks.c           = c;
	me->audioTracks._this       = JS_NewObject(c, &html_media_rt->audioTrackListClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->audioTracks._this, &me->audioTracks);

	me->videoTracks.c           = c;
	me->videoTracks._this       = JS_NewObject(c, &html_media_rt->videoTrackListClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->videoTracks._this, &me->videoTracks);

	me->textTracks.c            = c;
	me->textTracks._this        = JS_NewObject(c, &html_media_rt->textTrackListClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->textTracks._this, &me->textTracks);

	me->buffered->c              = c;
	me->buffered->_this          = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->buffered->_this, me->buffered);

	me->played->c                = c;
	me->played->_this            = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->played->_this, me->played);

	me->seekable->c              = c;
	me->seekable->_this          = JS_NewObject(c, &html_media_rt->timeRangesClass._class, NULL, me->_this);
	SMJS_SET_PRIVATE(c, me->seekable->_this, me->seekable);
}

/* Function to browse the tracks in the MediaObject associated with the Media Element and to create appropriate HTML Track objects
 *
 * \param c The JavaScript Context to create the new JS object
 * \param me the HTMLMediaElement to update
 */
static void html_media_element_populate_tracks(JSContext *c, GF_HTML_MediaElement *me)
{
	GF_HTML_Track *track;
	GF_MediaObject *mo;
	u32 source_id=0;
	GF_Scene *scene;
	u32 i, count;

	if (!me || !me->node) {
		return;
	}
	mo = gf_html_media_object(me->node);
	if (!mo || !mo->odm || !mo->odm->parentscene) {
		return;
	}
	source_id = mo->odm->source_id;
	scene = mo->odm->parentscene;

	count = gf_list_count(scene->resources);
	for (i=0; i<count; i++) {
		char id[256];
		char *lang = "";
		GF_ObjectManager *odm = (GF_ObjectManager *)gf_list_get(scene->resources, i);
		if (odm->source_id != source_id) continue;
		if (!odm->mo) continue;

		sprintf(id, "track%d", odm->ID);
#ifdef FILTER_FIXME
#error "set track ID and lang"
#endif
		switch(odm->mo->type) {
		case GF_MEDIA_OBJECT_VIDEO:
			if (!html_media_tracklist_has_track(&me->videoTracks, id)) {
				track = html_media_add_new_track_to_list(&me->videoTracks, HTML_MEDIA_TRACK_TYPE_VIDEO,
					        "video/mp4", GF_TRUE, id, "myKind", "myLabel", lang);
				gf_html_track_init_js(track, c, &me->videoTracks);
			}
			break;
		case GF_MEDIA_OBJECT_AUDIO:
			if (!html_media_tracklist_has_track(&me->audioTracks, id)) {
				track = html_media_add_new_track_to_list(&me->audioTracks, HTML_MEDIA_TRACK_TYPE_AUDIO,
					        "audio/mp4", GF_TRUE, id, "myKind", "myLabel", lang);
				gf_html_track_init_js(track, c, &me->audioTracks);
			}
			break;
		case GF_MEDIA_OBJECT_TEXT:
			if (!html_media_tracklist_has_track(&me->textTracks, id)) {
				track = html_media_add_new_track_to_list(&me->textTracks, HTML_MEDIA_TRACK_TYPE_TEXT,
					        "text/plain", GF_TRUE, id, "myKind", "myLabel", lang);
				gf_html_track_init_js(track, c, &me->textTracks);
			}
			break;
		}
	}
}

/* Used to retrieve the structure implementing the GF_HTML_MediaElement interface associated with this node
 * Usually this is done with the private stack of the node (see gf_node_get_private), but in this case,
 * the stack already contains the rendering stack SVG_video_stack.
 * So, we store the structure implementing the GF_HTML_MediaElement interface in the JavaScript context of this node,
 * as a non enumeratable property named 'gpac_me_impl'
 *
 *  \param c the global JavaScript context
 *  \param n the audio or video node
 *  \return the GF_HTML_MediaElement associated with this node in the given context
 */
static GF_HTML_MediaElement *html_media_element_get_from_node(JSContext *c, GF_Node *n)
{
	jsval vp;
	JSObject *me_obj;
	JSObject *node_obj;
	GF_HTML_MediaElement *me = NULL;

	if ((n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio) && n->sgprivate->interact && n->sgprivate->interact->js_binding) {
		node_obj = (JSObject *)n->sgprivate->interact->js_binding->node;
		if (node_obj) {
			JS_GetProperty(c, node_obj, "gpac_me_impl", &vp);
			me_obj = JSVAL_TO_OBJECT(vp);
			me = (GF_HTML_MediaElement *)SMJS_GET_PRIVATE(c, me_obj);
		}
	}
	return me;
}

static JSBool SMJS_FUNCTION(html_media_load)
{
	SMJS_OBJ
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlAudioElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlMediaElementClass, NULL))
	{
		MFURL mfurl;
		GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj);
		GF_HTML_MediaElement *me = html_media_element_get_from_node(c, n);
		mfurl.count = 1;
		mfurl.vals = (SFURL *)gf_malloc(sizeof(SFURL));
		mfurl.vals[0].url = me->currentSrc;
		mfurl.vals[0].OD_ID = GF_MEDIA_EXTERNAL_ID;
		gf_mo_register(n, &mfurl, GF_FALSE, GF_FALSE);
		gf_free(mfurl.vals);
		return JS_TRUE;
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
}

static JSBool SMJS_FUNCTION(html_media_canPlayType)
{
	SMJS_OBJ
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlAudioElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlMediaElementClass, NULL))
	{
		return JS_TRUE;
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
}

static JSBool SMJS_FUNCTION(html_media_fastSeek)
{
	SMJS_OBJ
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlAudioElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlMediaElementClass, NULL))
	{
		return JS_TRUE;
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
}

static JSBool SMJS_FUNCTION(html_media_addTextTrack)
{
	SMJS_OBJ
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlAudioElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlMediaElementClass, NULL))
	{
		return JS_TRUE;
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
}

void *html_get_element_class(GF_Node *n)
{
	if (!n || !html_media_rt)
	{
		return NULL;
	} else if (n->sgprivate->tag == TAG_SVG_video) {
		return &html_media_rt->htmlVideoElementClass;
	} else if (n->sgprivate->tag == TAG_SVG_audio) {
		return &html_media_rt->htmlAudioElementClass;
	} else {
		return NULL;
	}
}

/* Creates the GF_HTML_MediaElement structure for this node
 * Store it in the JavaScript context of this node, as a non enumeratable property named 'gpac_me_impl'
 * see \ref html_media_element_get_from_node for retrieving it
 *
 *  \param c the global JavaScript context
 *  \param n the audio or video node
 *  \param node_obj the JavaScript object representing the audio or video node
 */
void html_media_element_js_init(JSContext *c, JSObject *node_obj, GF_Node *n)
{
	if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio) {
		GF_HTML_MediaElement *me;
		me = gf_html_media_element_new(n, NULL);
		gf_html_media_element_init_js(me, c, node_obj);
		/* To be able to retrieve the Media Element interface from the Node element */
		JS_DefineProperty(c, node_obj, "gpac_me_impl", OBJECT_TO_JSVAL(me->_this), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	}
}

/* Deletes the GF_HTML_MediaElement structure for this node
 *
 *  \param c the global JavaScript context
 *  \param n the audio or video node
 */
void html_media_element_js_finalize(JSContext *c, GF_Node *n)
{
	GF_HTML_MediaElement *me = html_media_element_get_from_node(c, n);
	/* TODO: finalize child objects */
	gf_html_media_element_del(me);
}

/* Gets The EventTarget and SceneGraph from the GF_HTML_MediaElement structure for a audio/video JS node
 *
 *  \param c the global JavaScript context
 *  \param obj the JS object representing the HTMLAudioElement or HTMLVideoElement
 *  \param target the GF_DOMEventTarget object
 *  \param sg the scene graph
 */
void gf_html_media_get_event_target(JSContext *c, JSObject *obj, GF_DOMEventTarget **target, GF_SceneGraph **sg) {
	if (!sg || !target || !html_media_rt) return;
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL) ||
	        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlAudioElementClass, NULL) ) {
		GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj);
		*target = gf_dom_event_get_target_from_node(n);
		*sg = n->sgprivate->scenegraph;
	} else {
		*target = NULL;
		*sg = NULL;
	}
}

static SMJS_FUNC_PROP_GET(html_media_error_get_code)
if (GF_JS_InstanceOf(c, obj, &html_media_rt->mediaErrorClass, NULL))
{
	GF_HTML_MediaError *error = (GF_HTML_MediaError*)SMJS_GET_PRIVATE(c, obj);
	if (!SMJS_ID_IS_INT(id)) {
		return JS_TRUE;
	}
	switch (SMJS_ID_TO_INT(id))
	{
	case MEDIA_ERROR_PROP_ABORTED:
		*vp = INT_TO_JSVAL(MEDIA_ERR_ABORTED);
		return JS_TRUE;
	case MEDIA_ERROR_PROP_NETWORK:
		*vp = INT_TO_JSVAL(MEDIA_ERR_NETWORK);
		return JS_TRUE;
	case MEDIA_ERROR_PROP_DECODE:
		*vp = INT_TO_JSVAL(MEDIA_ERR_DECODE);
		return JS_TRUE;
	case MEDIA_ERROR_PROP_SRC_NOT_SUPPORTED:
		*vp = INT_TO_JSVAL(MEDIA_ERR_SRC_NOT_SUPPORTED);
		return JS_TRUE;
	case MEDIA_ERROR_PROP_CODE:
		*vp = INT_TO_JSVAL(error->code);
		return JS_TRUE;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_error)
HTML_MEDIA_JS_START
*vp = OBJECT_TO_JSVAL(me->error._this);
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_src)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "href", GF_XMLNS_XLINK, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) );
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_src)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_XLINK, "href", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_cors)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "crossorigin", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) );
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_cors)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "crossorigin", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

void gf_odm_collect_buffer_info(void *net, GF_ObjectManager *odm, GF_DOMMediaEvent *media_event, u32 *min_time, u32 *min_buffer);

static void html_media_get_states(GF_MediaObject *mo, GF_HTML_MediaReadyState *readyState, GF_HTML_NetworkState *networkState)
{
	GF_DOMMediaEvent media_event;
#ifdef FILTER_FIXME
	u32 min_time=0;
#endif
	u32 min_buffer=0;
	if (!mo) return;

	if (!mo->odm) {
		*readyState = HAVE_NOTHING;
		*networkState = NETWORK_EMPTY;
		return;
	}

	memset(&media_event, 0, sizeof(GF_DOMMediaEvent));
#ifdef FILTER_FIXME
	gf_odm_collect_buffer_info(mo->odm->net_service, mo->odm, &media_event, &min_time, &min_buffer);
#endif
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] min_time: %d, min_buffer: %d loaded: "LLD"/"LLD"\n",
	//                                                    min_time, min_buffer, media_event.loaded_size, media_event.total_size));
	if (min_buffer == 0)
	{
		*readyState = HAVE_NOTHING;
		*networkState = NETWORK_EMPTY;
	}
	else
	{
		if (media_event.loaded_size != 0)
		{
			*readyState = HAVE_METADATA;
		}
		else if (media_event.loaded_size == media_event.total_size)
		{
			*readyState = HAVE_ENOUGH_DATA;
		}
	}
}

static SMJS_FUNC_PROP_GET(html_media_get_ready_state)
GF_HTML_MediaReadyState readyState = HAVE_NOTHING;
GF_HTML_NetworkState networkState = NETWORK_EMPTY;
GF_MediaObject *mo = NULL;
HTML_MEDIA_JS_START
html_media_element_populate_tracks(c, me);
mo = gf_html_media_object(n);
html_media_get_states(mo, &readyState, &networkState);
*vp = INT_TO_JSVAL( readyState );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_network_state)
GF_HTML_MediaReadyState readyState = HAVE_NOTHING;
GF_HTML_NetworkState networkState = NETWORK_EMPTY;
GF_MediaObject *mo = NULL;
HTML_MEDIA_JS_START
mo = gf_html_media_object(n);
html_media_get_states(mo, &readyState, &networkState);
*vp = INT_TO_JSVAL( networkState );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_const)
u32 v = 0;
if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlMediaElementClass, NULL) ||
        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL) ||
        GF_JS_InstanceOf(c, obj, &html_media_rt->htmlAudioElementClass, NULL))
{
	if (!SMJS_ID_IS_INT(id)) {
		return JS_TRUE;
	}
	switch (SMJS_ID_TO_INT(id)) {
	case HTML_MEDIA_PROP_NETWORK_EMPTY:
		v = NETWORK_EMPTY;
		break;
	case HTML_MEDIA_PROP_NETWORK_IDLE:
		v = NETWORK_IDLE;
		break;
	case HTML_MEDIA_PROP_NETWORK_LOADING:
		v = NETWORK_LOADING;
		break;
	case HTML_MEDIA_PROP_NETWORK_NO_SOURCE:
		v = NETWORK_NO_SOURCE;
		break;
	case HTML_MEDIA_PROP_HAVE_NOTHING:
		v = HAVE_NOTHING;
		break;
	case HTML_MEDIA_PROP_HAVE_METADATA:
		v = HAVE_METADATA;
		break;
	case HTML_MEDIA_PROP_HAVE_CURRENT_DATA:
		v = HAVE_CURRENT_DATA;
		break;
	case HTML_MEDIA_PROP_HAVE_FUTURE_DATA:
		v = HAVE_FUTURE_DATA;
		break;
	case HTML_MEDIA_PROP_HAVE_ENOUGH_DATA:
		v = HAVE_ENOUGH_DATA;
		break;
	default:
		return JS_TRUE;
	}
	*vp = INT_TO_JSVAL( v );
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_preload)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "preload", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) );
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_preload)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "preload", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_buffered)
HTML_MEDIA_JS_START
*vp = OBJECT_TO_JSVAL( me->buffered->_this );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_seeking)
HTML_MEDIA_JS_START
/* TODO: implement it for real */
*vp = BOOLEAN_TO_JSVAL( (me->seeking ? JS_TRUE : JS_FALSE) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_current_time)
u32 time_ms = 0;
Double time_s;
GF_MediaObject *mo = NULL;
HTML_MEDIA_JS_START
mo = gf_html_media_object(n);
gf_mo_get_object_time(mo, &time_ms);
time_s = time_ms / 1000.0;
*vp = JS_MAKE_DOUBLE(c, time_s );
return JS_TRUE;
}

static GFINLINE Bool ScriptAction(GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (scene->script_action)
	{
		return scene->script_action(scene->script_action_cbck, type, node, param);
	}
	return GF_FALSE;
}

static SMJS_FUNC_PROP_SET(html_media_set_current_time)
double d;
GF_JSAPIParam par;
HTML_MEDIA_JS_START
if (!JSVAL_IS_NUMBER(*vp)) {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
JS_ValueToNumber(c, *vp, &d);
par.time = d;
ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_TIME, (GF_Node *)n, &par);
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_duration)
Double dur;
GF_MediaObject *mo = NULL;
HTML_MEDIA_JS_START
mo = gf_html_media_object(n);
dur = gf_mo_get_duration(mo);
if (dur <= 0) {
	*vp = JS_GetNaNValue(c);
} else {
	*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, dur));
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_start_date)
HTML_MEDIA_JS_START
// SpiderMonkey 1.8.5 JSObject * JS_NewDateObject(JSContext *cx, int year, int mon, int mday, int hour, int min, int sec);
*vp = JSVAL_NULL;
/* TODO */
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_paused)
HTML_MEDIA_JS_START
*vp =( BOOLEAN_TO_JSVAL( (me->paused ? JS_TRUE : JS_FALSE) ) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_default_playback_rate)
HTML_MEDIA_JS_START
*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, me->defaultPlaybackRate));
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_default_playback_rate)
jsdouble d;
HTML_MEDIA_JS_START
if (!JSVAL_IS_NUMBER(*vp)) {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
JS_ValueToNumber(c, *vp, &d);
me->defaultPlaybackRate = d;
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_playback_rate)
Double speed;
GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj);
GF_MediaObject *mo = gf_html_media_object(n);
speed = gf_mo_get_current_speed(mo);
*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, speed));
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_playback_rate)
jsdouble d;
Fixed speed;
GF_MediaObject *mo;
HTML_MEDIA_JS_START
mo = gf_html_media_object(n);
if (!JSVAL_IS_NUMBER(*vp)) {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
JS_ValueToNumber(c, *vp, &d);
speed = FLT2FIX(d);
gf_mo_set_speed(mo, speed);
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_played)
HTML_MEDIA_JS_START
*vp =( OBJECT_TO_JSVAL( me->played->_this ) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_seekable)
HTML_MEDIA_JS_START
*vp =( OBJECT_TO_JSVAL( me->seekable->_this ) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_ended)
GF_MediaObject *mo;
HTML_MEDIA_JS_START
mo = gf_html_media_object(n);
*vp = BOOLEAN_TO_JSVAL( gf_mo_is_done(mo) ? JS_TRUE : JS_FALSE);
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_autoplay)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "autoplay", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	if (!strcmp(szAtt, "true") || !strcmp(szAtt, "TRUE")) {
		*vp =( BOOLEAN_TO_JSVAL( JS_TRUE ) );
	} else {
		*vp =( BOOLEAN_TO_JSVAL( JS_FALSE ) );
	}
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_autoplay)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "autoplay", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_loop)
GF_FieldInfo info;
HTML_MEDIA_JS_START
/* TODO: gf_mo_get_loop */
if (gf_node_get_attribute_by_name(n, "loop", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	if (!strcmp(szAtt, "true") || !strcmp(szAtt, "TRUE")) {
		*vp =( BOOLEAN_TO_JSVAL( JS_TRUE ) );
	} else {
		*vp =( BOOLEAN_TO_JSVAL( JS_FALSE ) );
	}
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_loop)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "loop", str);
	//TODO: use gf_mo_get_loop
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_mediagroup)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "mediagroup", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	*vp =( STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) ) );
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_mediagroup)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "mediagroup", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_controller)
HTML_MEDIA_JS_START
if (me->controller) {
	*vp = OBJECT_TO_JSVAL( me->controller->_this );
} else {
	*vp = JSVAL_NULL;
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_controller)
HTML_MEDIA_JS_START
if (JSVAL_IS_OBJECT(*vp)) {
	me->controller = (GF_HTML_MediaController *)SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp));
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_controls)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "controls", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	if (!strcmp(szAtt, "true") || !strcmp(szAtt, "TRUE")) {
		*vp =( BOOLEAN_TO_JSVAL( JS_TRUE ) );
	} else {
		*vp =( BOOLEAN_TO_JSVAL( JS_FALSE ) );
	}
	if (szAtt) gf_free(szAtt);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_controls)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "controls", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SVG_audio_stack *html_media_get_audio_stack(GF_Node *n) {
	SVG_video_stack *video_stack = NULL;
	SVG_audio_stack *audio_stack = NULL;
	if (n->sgprivate->tag == TAG_SVG_video) {
		video_stack = (SVG_video_stack *)n->sgprivate->UserPrivate;
		if (video_stack->audio) {
			audio_stack = (SVG_audio_stack *)video_stack->audio->sgprivate->UserPrivate;
		}
	} else if (n->sgprivate->tag == TAG_SVG_audio) {
		audio_stack = (SVG_audio_stack *)n->sgprivate->UserPrivate;
	}
	return audio_stack;
}

static SMJS_FUNC_PROP_GET(html_media_get_volume)
Fixed volume = FIX_ONE;
SVG_audio_stack *audio_stack = NULL;
HTML_MEDIA_JS_CHECK
audio_stack = html_media_get_audio_stack(n);
if (audio_stack) {
	audio_stack->input.input_ifce.GetChannelVolume(&audio_stack->input.input_ifce.callback, &volume);
}
*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT(volume)));
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_volume)
jsdouble volume;
SVG_audio_stack *audio_stack = NULL;
HTML_MEDIA_JS_CHECK
audio_stack = html_media_get_audio_stack(n);
if (audio_stack) {
	JS_ValueToNumber(c, *vp, &volume);
	audio_stack->input.intensity = FLT2FIX(volume);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_muted)
SVG_audio_stack *audio_stack = NULL;
HTML_MEDIA_JS_CHECK
audio_stack = html_media_get_audio_stack(n);
if (audio_stack) {
	*vp = BOOLEAN_TO_JSVAL(audio_stack->input.is_muted ? JS_TRUE : JS_FALSE);
} else {
	*vp =( BOOLEAN_TO_JSVAL( JS_FALSE ) );
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_muted)
SVG_audio_stack *audio_stack = NULL;
HTML_MEDIA_JS_CHECK
audio_stack = html_media_get_audio_stack(n);
if (audio_stack) {
	audio_stack->input.is_muted = (JSVAL_TO_BOOLEAN(*vp) == JS_TRUE ? GF_TRUE : GF_FALSE);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_default_muted)
GF_FieldInfo info;
HTML_MEDIA_JS_START
if (gf_node_get_attribute_by_name(n, "muted", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
	char *szAtt = gf_svg_dump_attribute(n, &info);
	if (!strcmp(szAtt, "true") || !strcmp(szAtt, "TRUE")) {
		*vp =( BOOLEAN_TO_JSVAL( JS_TRUE ) );
	} else {
		*vp =( BOOLEAN_TO_JSVAL( JS_FALSE ) );
	}
	if (szAtt) gf_free(szAtt);
} else {
	*vp =( BOOLEAN_TO_JSVAL( JS_FALSE ) );
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(html_media_set_default_muted)
HTML_MEDIA_JS_START
if (JSVAL_CHECK_STRING(*vp)) {
	char *str = SMJS_CHARS(c, *vp);
	gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "muted", str);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_media_get_audio_tracks)
HTML_MEDIA_JS_START
*vp =( OBJECT_TO_JSVAL( me->audioTracks._this ) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_video_tracks)
HTML_MEDIA_JS_START
*vp =( OBJECT_TO_JSVAL( me->videoTracks._this ) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_media_get_text_tracks)
HTML_MEDIA_JS_START
*vp =( OBJECT_TO_JSVAL( me->textTracks._this ) );
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(html_time_ranges_get_length)
GF_HTML_MediaTimeRanges *timeranges;
if (!GF_JS_InstanceOf(c, obj, &html_media_rt->timeRangesClass, NULL)) {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
timeranges = (GF_HTML_MediaTimeRanges *)SMJS_GET_PRIVATE(c, obj);
*vp = INT_TO_JSVAL( gf_list_count(timeranges->times)/2);
return JS_TRUE;
}

static JSBool SMJS_FUNCTION(html_time_ranges_start)
{
	GF_HTML_MediaTimeRanges *timeranges;
	SMJS_OBJ
	SMJS_ARGS
	if ((argc!=1) || !GF_JS_InstanceOf(c, obj, &html_media_rt->timeRangesClass, NULL)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	timeranges = (GF_HTML_MediaTimeRanges *)SMJS_GET_PRIVATE(c, obj);
	if (!timeranges) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	if (JSVAL_IS_INT(argv[0])) {
		u32 i = JSVAL_TO_INT(argv[0]);
		u64 *start_value = (u64 *)gf_list_get(timeranges->times, 2*i);
		if (!start_value) {
			return dom_throw_exception(c, GF_DOM_EXC_INDEX_SIZE_ERR);
		} else {
			SMJS_SET_RVAL(DOUBLE_TO_JSVAL(JS_NewDouble(c, (*start_value)*1.0/timeranges->timescale)));
		}
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(html_time_ranges_end)
{
	GF_HTML_MediaTimeRanges *timeranges;
	SMJS_OBJ
	SMJS_ARGS
	if ((argc!=1) || !GF_JS_InstanceOf(c, obj, &html_media_rt->timeRangesClass, NULL)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	timeranges = (GF_HTML_MediaTimeRanges *)SMJS_GET_PRIVATE(c, obj);
	if (!timeranges) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	if (JSVAL_IS_INT(argv[0])) {
		u32 i = JSVAL_TO_INT(argv[0]);
		u64 *end_value = (u64 *)gf_list_get(timeranges->times, 2*i+1);
		if (!end_value) {
			return dom_throw_exception(c, GF_DOM_EXC_INDEX_SIZE_ERR);
		} else {
			SMJS_SET_RVAL(DOUBLE_TO_JSVAL(JS_NewDouble(c, (*end_value)*1.0/timeranges->timescale)));
		}
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	return JS_TRUE;
}

static Bool html_is_track_list(JSContext *c, JSObject *obj) {
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->videoTrackListClass, NULL)) return GF_TRUE;
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->audioTrackListClass, NULL)) return GF_TRUE;
	if (GF_JS_InstanceOf(c, obj, &html_media_rt->textTrackListClass, NULL)) return GF_TRUE;
	return GF_FALSE;
}

static SMJS_FUNC_PROP_GET(html_track_list_get_length)
GF_HTML_TrackList *tracklist;
if (html_is_track_list(c, obj)) {
	tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	if (tracklist) {
		*vp = INT_TO_JSVAL( gf_list_count(tracklist->tracks) );
		return JS_TRUE;
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static JSBool SMJS_FUNCTION(html_track_list_get_track_by_id)
{
	SMJS_OBJ
	SMJS_ARGS
	if (html_is_track_list(c, obj)) {
		GF_HTML_TrackList *tracklist;
		GF_HTML_Track *track;
		char *str;
		tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
		str = SMJS_CHARS_FROM_STRING(c, JS_ValueToString(c, argv[0]) );
		track = html_media_tracklist_get_track(tracklist, str);
		SMJS_FREE(c, str);
		if (track) {
			SMJS_SET_RVAL(OBJECT_TO_JSVAL(track->_this));
			return JS_TRUE;
		} else {
			return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
		}
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
}

static SMJS_FUNC_PROP_GET(html_track_list_get_property)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist;
	s32 index;

	tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	index = SMJS_ID_TO_INT(id);
	if (index >= 0 && (u32)index < gf_list_count(tracklist->tracks)) {
		GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(tracklist->tracks, (u32)index);
		*vp = OBJECT_TO_JSVAL(track->_this);
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_track_list_get_selected_index)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	*vp = INT_TO_JSVAL(tracklist->selected_index);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_track_list_get_onchange)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	if (! JSVAL_IS_NULL(tracklist->onchange)) {
		*vp = tracklist->onchange;
	} else {
		*vp = JSVAL_NULL;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

JSBool gf_set_js_eventhandler(JSContext *c, jsval vp, jsval *callbackfuncval);

static SMJS_FUNC_PROP_SET(html_track_list_set_onchange)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	gf_set_js_eventhandler(c, *vp, &tracklist->onchange);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_track_list_get_onaddtrack)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	if (! JSVAL_IS_NULL(tracklist->onaddtrack)) {
		*vp = tracklist->onaddtrack;
	} else {
		*vp = JSVAL_NULL;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_SET(html_track_list_set_onaddtrack)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	gf_set_js_eventhandler(c, *vp, &tracklist->onaddtrack);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_track_list_get_onremovetrack)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	if (! JSVAL_IS_NULL(tracklist->onremovetrack) ) {
		*vp = tracklist->onremovetrack;
	} else {
		*vp = JSVAL_NULL;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_SET(html_track_list_set_onremovetrack)
if (html_is_track_list(c, obj)) {
	GF_HTML_TrackList *tracklist = (GF_HTML_TrackList *)SMJS_GET_PRIVATE(c, obj);
	gf_set_js_eventhandler(c, *vp, &tracklist->onremovetrack);
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_track_get_property)
if (html_is_track_list(c, obj)) {
	GF_HTML_Track *track = (GF_HTML_Track *)SMJS_GET_PRIVATE(c, obj);
	if (!SMJS_ID_IS_INT(id)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	switch (SMJS_ID_TO_INT(id)) {
	case HTML_TRACK_PROP_ID:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, track->id ? track->id : "") );
		return JS_TRUE;
	case HTML_TRACK_PROP_KIND:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, track->kind ? track->kind : "") );
		return JS_TRUE;
	case HTML_TRACK_PROP_LABEL:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, track->label ? track->label : "") );
		return JS_TRUE;
	case HTML_TRACK_PROP_LANGUAGE:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, track->language ? track->language  : "") );
		return JS_TRUE;
	case HTML_TRACK_PROP_INBANDTYPE:
		if (GF_JS_InstanceOf(c, obj, &html_media_rt->textTrackClass, NULL)) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, track->mime ? track->mime  : "") );
			return JS_TRUE;
		}
	case HTML_TRACK_PROP_SELECTED:
		if (GF_JS_InstanceOf(c, obj, &html_media_rt->videoTrackClass, NULL)) {
			*vp = BOOLEAN_TO_JSVAL(track->enabled_or_selected ? JS_TRUE : JS_FALSE);
		}
		return JS_TRUE;
	case HTML_TRACK_PROP_ENABLED:
		if (GF_JS_InstanceOf(c, obj, &html_media_rt->audioTrackClass, NULL)) {
			*vp = BOOLEAN_TO_JSVAL(track->enabled_or_selected ? JS_TRUE : JS_FALSE);
		}
		return JS_TRUE;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_SET(html_track_set_property)
if (html_is_track_list(c, obj)) {
	GF_HTML_Track *track = (GF_HTML_Track *)SMJS_GET_PRIVATE(c, obj);
	if (!SMJS_ID_IS_INT(id)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	switch (SMJS_ID_TO_INT(id)) {
	case HTML_TRACK_PROP_SELECTED:
		if (GF_JS_InstanceOf(c, obj, &html_media_rt->videoTrackClass, NULL))
		{
			track->enabled_or_selected = (JSVAL_TO_BOOLEAN(*vp) == JS_TRUE ? GF_TRUE : GF_FALSE);
		}
		return JS_TRUE;
	case HTML_TRACK_PROP_ENABLED:
		if (GF_JS_InstanceOf(c, obj, &html_media_rt->audioTrackClass, NULL))
		{
			track->enabled_or_selected = (JSVAL_TO_BOOLEAN(*vp) == JS_TRUE ? GF_TRUE : GF_FALSE);
		}
		return JS_TRUE;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_GET(html_video_get_property)
if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL))
{
	GF_FieldInfo    info;
	SVG_video_stack *video;
	GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj);
	if (!n) return JS_TRUE;

	video = (SVG_video_stack *)n->sgprivate->UserPrivate;
	if (!SMJS_ID_IS_INT(id)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	switch (SMJS_ID_TO_INT(id)) {
	case HTML_VIDEO_PROP_WIDTH:
		if (gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_width, GF_TRUE, GF_TRUE, &info)==GF_OK) {
			SVG_Length *length;
			length = (SVG_Length *)info.far_ptr;
			*vp = INT_TO_JSVAL( FIX2FLT(length->value) );
		}
		return JS_TRUE;
	case HTML_VIDEO_PROP_HEIGHT:
		if (gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_height, GF_TRUE, GF_TRUE, &info)==GF_OK) {
			SVG_Length *length;
			length = (SVG_Length *)info.far_ptr;
			*vp = INT_TO_JSVAL( FIX2FLT(length->value) );
		}
		return JS_TRUE;
	case HTML_VIDEO_PROP_VIDEOWIDTH:
		*vp = INT_TO_JSVAL( video->txh.width );
		return JS_TRUE;
	case HTML_VIDEO_PROP_VIDEOHEIGHT:
		*vp = INT_TO_JSVAL( video->txh.height );
		return JS_TRUE;
	case HTML_VIDEO_PROP_POSTER:
		if (gf_node_get_attribute_by_name(n, "poster", GF_XMLNS_SVG, GF_TRUE, GF_TRUE, &info)==GF_OK) {
			char *szAtt = gf_svg_dump_attribute(n, &info);
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) );
			if (szAtt) gf_free(szAtt);
		}
		return JS_TRUE;
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static SMJS_FUNC_PROP_SET(html_video_set_property)
if (GF_JS_InstanceOf(c, obj, &html_media_rt->htmlVideoElementClass, NULL))
{
	GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj);
	if (!SMJS_ID_IS_INT(id)) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	switch (SMJS_ID_TO_INT(id)) {
	case HTML_VIDEO_PROP_WIDTH:
		/* TODO: change value and dirty node */
		return JS_TRUE;
	case HTML_VIDEO_PROP_HEIGHT:
		/* TODO: change value and dirty node */
		return JS_TRUE;
	case HTML_VIDEO_PROP_POSTER:
	{
		char *str = SMJS_CHARS_FROM_STRING(c, JS_ValueToString(c, *vp) );
		gf_svg_set_attributeNS(n, GF_XMLNS_SVG, "poster", str);
		SMJS_FREE(c, str);
		return JS_TRUE;
	}
	}
	return JS_TRUE;
} else {
	return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
}
}

static JSBool SMJS_FUNCTION(html_media_event_add_listener)
{
	JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_add_listener, GF_Node *vrml_node);
	return gf_sg_js_event_add_listener(SMJS_CALL_ARGS, NULL);
}

static JSBool SMJS_FUNCTION(html_media_event_remove_listener)
{
	JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_remove_listener, GF_Node *vrml_node);
	return gf_sg_js_event_remove_listener(SMJS_CALL_ARGS, NULL);
}

static JSBool SMJS_FUNCTION(html_media_event_dispatch)
{
	return JS_TRUE;
}

void html_media_source_init_js_api(JSContext *js_ctx, JSObject *global, GF_HTML_MediaRuntime *html_media_rt);

void html_media_js_api_del() {
	if (html_media_rt) {
		gf_free(html_media_rt);
		html_media_rt = NULL;
	}
}

void html_media_init_js_api(GF_SceneGraph *scene) {
	/* HTML Media Element */
	if (!html_media_rt) {
		GF_SAFEALLOC(html_media_rt, GF_HTML_MediaRuntime);

		/* Setting up MediaError class (no constructor, no finalizer) */
		JS_SETUP_CLASS(html_media_rt->mediaErrorClass, "MediaError", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec mediaErrorClassProps[] = {
				{"MEDIA_ERR_ABORTED",           MEDIA_ERROR_PROP_ABORTED,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_error_get_code, 0},
				{"MEDIA_ERR_NETWORK",           MEDIA_ERROR_PROP_NETWORK,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_error_get_code, 0},
				{"MEDIA_ERR_DECODE",            MEDIA_ERROR_PROP_DECODE,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_error_get_code, 0},
				{"MEDIA_ERR_SRC_NOT_SUPPORTED", MEDIA_ERROR_PROP_SRC_NOT_SUPPORTED,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_error_get_code, 0},
				{"code",                        MEDIA_ERROR_PROP_CODE,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_error_get_code, 0},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec mediaErrorClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->mediaErrorClass, 0, 0, mediaErrorClassProps, mediaErrorClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] MediaError class initialized\n"));
		}

		/* Setting up TimeRanges class (no constructor, no finalizer) */
		JS_SETUP_CLASS(html_media_rt->timeRangesClass, "TimeRanges", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec timeRangesClassProps[] = {
				{"length",               0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_time_ranges_get_length, 0},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec timeRangesClassFuncs[] = {
				SMJS_FUNCTION_SPEC("start",                 html_time_ranges_start, 1),
				SMJS_FUNCTION_SPEC("end",                   html_time_ranges_end, 1),
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->timeRangesClass, 0, 0, timeRangesClassProps, timeRangesClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] TimeRanges class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->audioTrackClass, "AudioTrack", JSCLASS_HAS_PRIVATE, html_track_get_property, html_track_set_property, JS_FinalizeStub);
		{
			JSPropertySpec audioTrackClassProps[] = {
				{"id",               HTML_TRACK_PROP_ID,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"kind",             HTML_TRACK_PROP_KIND,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"label",            HTML_TRACK_PROP_LABEL,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"language",         HTML_TRACK_PROP_LANGUAGE,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"enabled",          HTML_TRACK_PROP_SELECTED,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec audioTrackClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->audioTrackClass, 0, 0, audioTrackClassProps, audioTrackClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] AudioTrack class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->videoTrackClass, "VideoTrack", JSCLASS_HAS_PRIVATE, html_track_get_property, html_track_set_property, JS_FinalizeStub);
		{
			JSPropertySpec videoTrackClassProps[] = {
				{"id",               HTML_TRACK_PROP_ID,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"kind",             HTML_TRACK_PROP_KIND,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"label",            HTML_TRACK_PROP_LABEL,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"language",         HTML_TRACK_PROP_LANGUAGE,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"selected",         HTML_TRACK_PROP_SELECTED,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec videoTrackClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->videoTrackClass, 0, 0, videoTrackClassProps, videoTrackClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] VideoTrack class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->textTrackClass, "TextTrack", JSCLASS_HAS_PRIVATE, html_track_get_property, html_track_set_property, JS_FinalizeStub);
		{
			JSPropertySpec textTrackClassProps[] = {
				{"id",               HTML_TRACK_PROP_ID,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"kind",             HTML_TRACK_PROP_KIND,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"label",            HTML_TRACK_PROP_LABEL,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"language",         HTML_TRACK_PROP_LANGUAGE,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"inBandMetadataTrackDispatchType",         HTML_TRACK_PROP_INBANDTYPE,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec textTrackClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->textTrackClass, 0, 0, textTrackClassProps, textTrackClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] TextTrack class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->audioTrackListClass, "AudioTrackList", JSCLASS_HAS_PRIVATE, html_track_list_get_property, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec audioTrackListClassProps[] = {
				{"length",               0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_track_list_get_length, 0},
				{"onchange",             0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onchange, html_track_list_set_onchange},
				{"onaddtrack",           0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onaddtrack, html_track_list_set_onaddtrack},
				{"onremovetrack",        0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onremovetrack, html_track_list_set_onremovetrack},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec audioTrackListClassFuncs[] = {
				SMJS_FUNCTION_SPEC("getTrackById",          html_track_list_get_track_by_id, 1),
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->audioTrackListClass, 0, 0, audioTrackListClassProps, audioTrackListClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] AudioTrackList class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->videoTrackListClass, "VideoTrackList", JSCLASS_HAS_PRIVATE, html_track_list_get_property, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec videoTrackListClassProps[] = {
				{"selectedIndex",        0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_track_list_get_selected_index, 0},
				{"length",               0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_track_list_get_length, 0},
				{"onchange",             0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onchange, html_track_list_set_onchange},
				{"onaddtrack",           0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onaddtrack, html_track_list_set_onaddtrack},
				{"onremovetrack",        0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onremovetrack, html_track_list_set_onremovetrack},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec videoTrackListClassFuncs[] = {
				SMJS_FUNCTION_SPEC("getTrackById",          html_track_list_get_track_by_id, 1),
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->videoTrackListClass, 0, 0, videoTrackListClassProps, videoTrackListClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] VideoTrackList class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->textTrackListClass, "TextTrackList", JSCLASS_HAS_PRIVATE, html_track_list_get_property, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec textTrackListClassProps[] = {
				{"length",               0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_track_list_get_length, 0},
				{"onaddtrack",           0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onaddtrack, html_track_list_set_onaddtrack},
				{"onremovetrack",        0,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_track_list_get_onremovetrack, html_track_list_set_onremovetrack},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec textTrackListClassFuncs[] = {
				SMJS_FUNCTION_SPEC("getTrackById",          html_track_list_get_track_by_id, 1),
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->textTrackListClass, 0, 0, textTrackListClassProps, textTrackListClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] TextTrackList class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->mediaControllerClass, "MediaController", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec mediaControllerClassProps[] = {
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec mediaControllerClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &html_media_rt->mediaControllerClass, 0, 0, mediaControllerClassProps, mediaControllerClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] MediaController class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->htmlMediaElementClass, "HTMLMediaElement", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);
		{
			JSPropertySpec htmlMediaElementClassProps[] = {
				{"error",               HTML_MEDIA_PROP_ERROR,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_error, 0},
				{"src",                 HTML_MEDIA_PROP_SRC,          JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , html_media_get_src, html_media_set_src},
				{"currentSrc",          HTML_MEDIA_PROP_CURRENTSRC,   JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_src, 0},
				{"crossOrigin",         HTML_MEDIA_PROP_CROSSORIGIN,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_cors, html_media_set_cors},
				{"NETWORK_EMPTY",       HTML_MEDIA_PROP_NETWORK_EMPTY,    JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"NETWORK_IDLE",        HTML_MEDIA_PROP_NETWORK_IDLE,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"NETWORK_LOADING",     HTML_MEDIA_PROP_NETWORK_LOADING,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"NETWORK_NO_SOURCE",   HTML_MEDIA_PROP_NETWORK_NO_SOURCE,JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"networkState",        HTML_MEDIA_PROP_NETWORKSTATE, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_network_state, 0},
				{"preload",             HTML_MEDIA_PROP_PRELOAD,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_preload, html_media_set_preload},
				{"buffered",            HTML_MEDIA_PROP_BUFFERED,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_buffered, 0},
				{"HAVE_NOTHING",        HTML_MEDIA_PROP_HAVE_NOTHING,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"HAVE_METADATA",       HTML_MEDIA_PROP_HAVE_METADATA,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"HAVE_CURRENT_DATA",   HTML_MEDIA_PROP_HAVE_CURRENT_DATA, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"HAVE_FUTURE_DATA",    HTML_MEDIA_PROP_HAVE_FUTURE_DATA,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"HAVE_ENOUGH_DATA",    HTML_MEDIA_PROP_HAVE_ENOUGH_DATA,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_const, 0},
				{"readyState",          HTML_MEDIA_PROP_READYSTATE,   JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_ready_state, 0},
				{"seeking",             HTML_MEDIA_PROP_SEEKING,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_seeking, 0},
				{"currentTime",         HTML_MEDIA_PROP_CURRENTTIME,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_current_time, html_media_set_current_time},
				{"duration",            HTML_MEDIA_PROP_DURATION,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_duration, 0},
				{"startDate",           HTML_MEDIA_PROP_STARTDATE,    JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_start_date, 0},
				{"paused",              HTML_MEDIA_PROP_PAUSED,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_paused, 0},
				{"defaultPlaybackRate", HTML_MEDIA_PROP_DEFAULTPLAYBACKRATE, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_default_playback_rate, html_media_set_default_playback_rate},
				{"playbackRate",        HTML_MEDIA_PROP_PLAYBACKRATE, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_playback_rate, html_media_set_playback_rate},
				{"played",              HTML_MEDIA_PROP_PLAYED,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_played, 0},
				{"seekable",            HTML_MEDIA_PROP_SEEKABLE,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_seekable, 0},
				{"ended",               HTML_MEDIA_PROP_ENDED,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_ended, 0},
				{"autoplay",            HTML_MEDIA_PROP_AUTOPLAY,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_autoplay, html_media_set_autoplay},
				{"loop",                HTML_MEDIA_PROP_LOOP,         JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_loop, html_media_set_loop},
				{"mediaGroup",          HTML_MEDIA_PROP_MEDIAGROUP,   JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_mediagroup, html_media_set_mediagroup},
				{"controller",          HTML_MEDIA_PROP_CONTROLLER,   JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_controller, html_media_set_controller},
				{"controls",            HTML_MEDIA_PROP_CONTROLS,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_controls, html_media_set_controls},
				{"volume",              HTML_MEDIA_PROP_VOLUME,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_volume, html_media_set_volume},
				{"muted",               HTML_MEDIA_PROP_MUTED,        JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_muted, html_media_set_muted},
				{"defaultMuted",        HTML_MEDIA_PROP_DEFAULTMUTED, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, html_media_get_default_muted, html_media_set_default_muted},
				{"audioTracks",         HTML_MEDIA_PROP_AUDIOTRACKS,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_audio_tracks, 0},
				{"videoTracks",         HTML_MEDIA_PROP_VIDEOTRACKS,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_video_tracks, 0},
				{"textTracks",          HTML_MEDIA_PROP_TEXTTRACKS,   JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, html_media_get_text_tracks, 0},
				{0, 0, 0, 0, 0}
			};
			JSBool SMJS_FUNCTION(svg_udom_smil_pause);
			JSBool SMJS_FUNCTION(svg_udom_smil_begin);
			JSBool SMJS_FUNCTION(svg_udom_smil_end);
			JSFunctionSpec htmlMediaElementClassFuncs[] = {
				SMJS_FUNCTION_SPEC("load",                  html_media_load, 0),
				SMJS_FUNCTION_SPEC("canPlayType",           html_media_canPlayType, 1),
				SMJS_FUNCTION_SPEC("fastSeek",              html_media_fastSeek, 1),
				SMJS_FUNCTION_SPEC("play",                  svg_udom_smil_begin, 0),
				SMJS_FUNCTION_SPEC("pause",                 svg_udom_smil_pause, 0),
				SMJS_FUNCTION_SPEC("addTextTrack",          html_media_addTextTrack, 1),
				SMJS_FUNCTION_SPEC("addEventListener",		html_media_event_add_listener, 3),
				SMJS_FUNCTION_SPEC("removeEventListener",	html_media_event_remove_listener, 3),
				SMJS_FUNCTION_SPEC("dispatchEvent",			html_media_event_dispatch, 1),
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			JSObject *elt_proto = dom_js_get_element_proto(scene->svg_js->js_ctx);
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, elt_proto, &html_media_rt->htmlMediaElementClass, 0, 0, htmlMediaElementClassProps, htmlMediaElementClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] HTMLMediaElement class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->htmlVideoElementClass, "HTMLVideoElement", JSCLASS_HAS_PRIVATE, html_video_get_property, html_video_set_property, dom_element_finalize);
		{
			JSPropertySpec htmlVideoElementClassProps[] = {
				{"width",       HTML_VIDEO_PROP_WIDTH,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0},
				{"height",      HTML_VIDEO_PROP_HEIGHT,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0},
				{"videoWidth",  HTML_VIDEO_PROP_VIDEOWIDTH,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"videoHeight", HTML_VIDEO_PROP_VIDEOHEIGHT, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0},
				{"poster",      HTML_VIDEO_PROP_POSTER,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0},
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec htmlVideoElementClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, html_media_rt->htmlMediaElementClass._proto, &html_media_rt->htmlVideoElementClass, 0, 0, htmlVideoElementClassProps, htmlVideoElementClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] HTMLVideoElement class initialized\n"));
		}

		JS_SETUP_CLASS(html_media_rt->htmlAudioElementClass, "HTMLAudioElement", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, dom_element_finalize);
		{
			JSPropertySpec htmlAudioElementClassProps[] = {
				{0, 0, 0, 0, 0}
			};
			JSFunctionSpec htmlAudioElementClassFuncs[] = {
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};
			GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, html_media_rt->htmlMediaElementClass._proto, &html_media_rt->htmlAudioElementClass, 0, 0, htmlAudioElementClassProps, htmlAudioElementClassFuncs, 0, 0);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[HTMLMediaAPI] HTMLAudioElement class initialized\n"));
		}
		html_media_source_init_js_api(scene->svg_js->js_ctx, scene->svg_js->global, html_media_rt);
	}
}
#endif  /*GPAC_HAS_SPIDERMONKEY*/

#endif  /*GPAC_DISABLE_SVG*/
